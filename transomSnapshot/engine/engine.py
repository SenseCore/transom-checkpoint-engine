import ctypes
import io
import math
import os
import pickle
import re
import sys
import time
import warnings
from pathlib import Path
from typing import IO, Any, BinaryIO, Dict, Optional, Tuple, Type, Union, cast

import torch
import torch.distributed as dist
import torch.serialization as helper
from loguru import logger
from torch._sources import get_source_lines_and_file
from torch.types import Storage

from .transom_memcpy import _save_to_memory
from .util import *


def check_torchversion() -> bool:
    """Check whether the torch version is greater than 1.10"""
    if (
        hasattr(torch.storage, "_TypedStorage")
        or hasattr(torch.storage, "TypedStorage")
        or hasattr(torch.storage, "_UntypedStorage")
        or hasattr(torch.storage, "UntypedStorage")
    ):
        return True
    else:
        return False


threads = []


def wait():
    logger.debug("exec engine.wait")
    for t in threads:
        t.join()
    # func = shm._get_wait_cfunc()
    # func()


def save(
    obj,
    f: Union[str, os.PathLike, BinaryIO, IO[bytes]],
    pickle_module=pickle,
    pickle_protocol=helper.DEFAULT_PROTOCOL,
    _use_new_zipfile_serialization=True,
) -> None:
    """start a thread to asynchronously execute torch.save"""
    # logger.debug("exec engine.save: {}".format(f))
    start = time.perf_counter()
    helper._check_dill_version(pickle_module)

    filename = str(f)
    # try to extract iteration from filename
    iteration = None
    if isinstance(obj, dict):
        iteration = obj.get("iter") or obj.get("iteration")
        # logger.debug("iteration:{} {}", iteration, obj.get("iteration"))
        if iteration is None:
            iteration = "unknown"
    else:
        iteration = "unknown"
    match = re.search(r"global_step(\d+)", filename)
    if match:
        iteration = match.group(1)
    data_buf = io.BytesIO()
    data_references = _legacy_save_v2(obj, data_buf, pickle_module, pickle_protocol)

    # calculate checkpoint size
    ckpt_size = 0
    # tensor size
    for _, size, _ in data_references:
        ckpt_size += size
    # size of the len of tensors
    ckpt_size += len(data_references) * ctypes.sizeof(ctypes.c_size_t)
    # data_buf size
    ckpt_size += len(data_buf.getvalue())
    if ckpt_size == 0:
        raise Exception("checkpoint size is 0")

    if torch.distributed.is_initialized():
        logger.debug(
            "rank-{} save {} size: {:.2f}MB".format(
                dist.get_rank(), str(f), ckpt_size / 1024 / 1024
            )
        )
    pid, memfd = SaveMetaRequest(
        filename, str(iteration), CheckpointState.PENDING, ckpt_size
    )

    save_success = _save_to_memory(
        filename, data_references, data_buf.getvalue(), ckpt_size, pid, memfd
    )
    if not save_success:
        raise Exception("write shared memory failed")
    logger.info(f"engine save in {time.perf_counter() - start:0.4f} seconds.")
    updateMetadataRequest(filename, CheckpointState.CACHED)


def load(f, map_location=None, pickle_module=pickle, **pickle_load_args) -> Any:
    """load checkpoint"""
    # logger.debug("engine.load: {}".format(f))
    helper._check_dill_version(pickle_module)

    filename = str(f)
    checkpointstate, pid, memfd = LoadMetaRequest(filename)
    logger.debug("checkpointstate: {}".format(CheckpointState(checkpointstate).name))
    mem_checkpoint = f"/proc/{pid}/fd/{memfd}"
    if Path(mem_checkpoint).exists():
        logger.debug(
            "load from in-memory: {} pid:{} memfd:{}".format(filename, pid, memfd)
        )
        return torch.load(mem_checkpoint, map_location)
    elif Path(f).exists():
        logger.debug("load from persistent checkpoint: {}".format(f))
        return torch.load(f, map_location)
    else:
        raise RuntimeError("in-memory and persistent checkpoint both do not exist")
    # if checkpointstate < CheckpointState.STATE_NUM:
    #     logger.debug(
    #         "metadata checkpointstate: {} is incorrect, delete it".format(
    #             CheckpointState(checkpointstate).name
    #         )
    #     )
    #     DeleteMetaRequest(base64_filename)
    # raise RuntimeError("in-memory and persistent checkpoint both do not exist")
    # if checkpointstate == CheckpointState.CACHED or checkpointstate == CheckpointState.BACKED_UP:
    # 本地没有，但是有备份


def _legacy_save_v2(obj, f, pickle_module, pickle_protocol) -> Any:
    """This is raw code copied from pytorch 1.10

    To ease serialization of object, we reuse _legacy_save and _legacy_load
    """

    import torch.nn as nn

    serialized_container_types = {}
    serialized_storages = {}
    storage_dtypes: Dict[int, torch.dtype] = {}

    def persistent_id_v2(obj: Any) -> Optional[Tuple]:
        # FIXME: the docs say that persistent_id should only return a string
        # but torch store returns tuples. This works only in the binary protocol
        # see
        # https://docs.python.org/2/library/pickle.html#pickling-and-unpickling-external-objects
        # https://github.com/python/cpython/blob/master/Lib/pickle.py#L527-L537
        if isinstance(obj, type) and issubclass(obj, nn.Module):
            if obj in serialized_container_types:
                return None
            serialized_container_types[obj] = True
            source_file = source = None
            try:
                source_lines, _, source_file = get_source_lines_and_file(obj)
                source = "".join(source_lines)
            except (
                Exception
            ):  # saving the source is optional, so we can ignore any errors
                warnings.warn(
                    "Couldn't retrieve source code for container of "
                    "type " + obj.__name__ + ". It won't be checked "
                    "for correctness upon loading."
                )
            return ("module", obj, source_file, source)

        if torch.is_storage(obj):
            # storage: torch.UntypedStorage
            # if isinstance(obj, torch.storage.TypedStorage):
            if hasattr(torch.storage, "TypedStorage") or hasattr(
                torch.storage, "_TypedStorage"
            ):
                # TODO: Once we decide to break serialization FC, this case
                # can be deleted
                if hasattr(obj, "_untyped_storage"):
                    storage = obj._untyped_storage
                else:
                    storage = obj._storage
                storage_dtype = obj.dtype
                storage_type_str = obj.pickle_storage_type()
                storage_type = getattr(torch, storage_type_str)
                dtype = obj.dtype
                storage_numel = obj.size()
            elif hasattr(torch.storage, "UntypedStorage") or hasattr(
                torch.storage, "_UntypedStorage"
            ):
                storage = obj
                storage_dtype = torch.uint8
                storage_type = helper.normalize_storage_type(type(obj))
                dtype = torch.uint8
                storage_numel = storage.nbytes()
            else:
                raise TypeError(f"type not recognized: {type(obj)}")

            # If storage is allocated, ensure that any other saved storages
            # pointing to the same data all have the same dtype. If storage is
            # not allocated, don't perform this check
            if storage.data_ptr() != 0:
                if storage.data_ptr() in storage_dtypes:
                    if storage_dtype != storage_dtypes[storage.data_ptr()]:
                        raise RuntimeError(
                            "Cannot save multiple tensors or storages that "
                            "view the same data as different types"
                        )
                else:
                    storage_dtypes[storage.data_ptr()] = storage_dtype

            view_metadata: Optional[Tuple[str, int, int]]
            storage = cast(Storage, storage)
            # Offset is always 0, but we keep it for backwards compatibility
            # with the old serialization format (which supported storage views)
            offset = 0
            storage_key = str(storage._cdata)
            location = helper.location_tag(storage)

            if storage_key not in serialized_storages:
                serialized_storages[storage_key] = (storage, dtype)
            is_view = storage._cdata != storage._cdata
            if is_view:
                view_metadata = (str(storage._cdata), offset, storage.nbytes())
            else:
                view_metadata = None

            res = (
                "storage",
                storage_type,
                storage_key,
                location,
                storage_numel,
                view_metadata,
            )
            return res
        return None

    def persistent_id(obj: Any) -> Optional[Tuple]:
        # FIXME: the docs say that persistent_id should only return a string
        # but torch store returns tuples. This works only in the binary protocol
        # see
        # https://docs.python.org/2/library/pickle.html#pickling-and-unpickling-external-objects
        # https://github.com/python/cpython/blob/master/Lib/pickle.py#L527-L537
        if isinstance(obj, type) and issubclass(obj, nn.Module):
            if obj in serialized_container_types:
                return None
            serialized_container_types[obj] = True
            source_file = source = None
            try:
                source_lines, _, source_file = get_source_lines_and_file(obj)
                source = "".join(source_lines)
            except (
                Exception
            ):  # saving the source is optional, so we can ignore any errors
                warnings.warn(
                    "Couldn't retrieve source code for container of "
                    "type " + obj.__name__ + ". It won't be checked "
                    "for correctness upon loading."
                )
            return ("module", obj, source_file, source)

        elif torch.is_storage(obj):
            view_metadata: Optional[Tuple[str, int, int]]
            obj = cast(Storage, obj)
            storage_type = helper.normalize_storage_type(type(obj))
            # Offset is always 0, but we keep it for backwards compatibility
            # with the old serialization format (which supported storage views)
            offset = 0
            obj_key = str(obj._cdata)
            location = helper.location_tag(obj)
            serialized_storages[obj_key] = obj
            is_view = obj._cdata != obj._cdata
            if is_view:
                view_metadata = (str(obj._cdata), offset, obj.size())
            else:
                view_metadata = None

            return (
                "storage",
                storage_type,
                obj_key,
                location,
                obj.size(),
                view_metadata,
            )
        return None

    sys_info = dict(
        protocol_version=helper.PROTOCOL_VERSION,
        little_endian=sys.byteorder == "little",
        type_sizes=dict(
            short=helper.SHORT_SIZE,
            int=helper.INT_SIZE,
            long=helper.LONG_SIZE,
        ),
    )
    # start = time.perf_counter()
    pickle_module.dump(helper.MAGIC_NUMBER, f, protocol=pickle_protocol)
    pickle_module.dump(helper.PROTOCOL_VERSION, f, protocol=pickle_protocol)
    pickle_module.dump(sys_info, f, protocol=pickle_protocol)
    pickler = pickle_module.Pickler(f, protocol=pickle_protocol)
    if check_torchversion():
        pickler.persistent_id = persistent_id_v2
    else:
        pickler.persistent_id = persistent_id
    pickler.dump(obj)
    serialized_storage_keys = sorted(serialized_storages.keys())
    pickle_module.dump(serialized_storage_keys, f, protocol=pickle_protocol)
    # logger.info(f"pickle dump in {time.perf_counter() - start:0.4f} seconds.")

    data_references = []

    if check_torchversion():
        for key in serialized_storage_keys:
            storage, _dtype = serialized_storages[key]
            num_bytes = storage.nbytes()
            storage_size = math.floor(
                storage.nbytes() / torch._utils._element_size(_dtype)
            )
            data_references.append((storage.data_ptr(), num_bytes, storage_size))
    else:
        for key in serialized_storage_keys:
            storage = serialized_storages[key]
            num_bytes = storage.size() * storage.element_size()
            data_references.append((storage.data_ptr(), num_bytes, storage.size()))

    return data_references
