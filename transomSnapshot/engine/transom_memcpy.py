import ctypes
import os

from loguru import logger


# correspond Tensor class in C++
class Tensor(ctypes.Structure):
    _fields_ = [
        ("data_ptr", ctypes.POINTER(ctypes.c_char)),
        ("nBytes", ctypes.c_size_t),
        ("size", ctypes.c_size_t),
    ]


def _save_to_memory(
    filename: str,
    obj_reference: list,
    obj_data: bytes,
    total_size: int,
    pid: int,
    memfd: int,
) -> bool:
    if len(obj_data) < 1 or filename is None:
        return False
    tensor_numbers = len(obj_reference)
    # logger.debug(f"tensor numbers: {tensor_numbers}")
    tensors_def = Tensor * tensor_numbers
    tensors = tensors_def()
    index = 0
    for ptr, num_bytes, size in obj_reference:
        tensors[index] = Tensor(
            ctypes.cast(
                ptr, ctypes.POINTER(ctypes.c_char)
            ),  # storage.data_ptr() -> ctypes.c_char
            ctypes.c_size_t(num_bytes),
            ctypes.c_size_t(size),
        )
        index += 1
    func = _get_cfunc()
    return func(
        filename.encode("utf-8"),
        obj_data,
        ctypes.c_size_t(len(obj_data)),
        tensors,
        ctypes.c_size_t(tensor_numbers),
        ctypes.c_size_t(total_size),
        ctypes.c_int(pid),
        ctypes.c_int(memfd),
    )


def _get_cfunc():
    cwd = os.path.dirname(os.path.abspath(__file__))
    memcpylib_path = os.path.join(cwd, "libtransom_memcpy.so")
    if not os.path.exists(memcpylib_path):
        logger.debug("libtransom_memcpy.so path: {}".format(memcpylib_path))
        raise Exception("libtransom_memcpy.so is not exists")
    try:
        dll = ctypes.CDLL(memcpylib_path, mode=ctypes.RTLD_GLOBAL)
    except OSError as e:
        raise Exception("%s failed to load: %s" % (memcpylib_path, e))
    func = dll.transom_memcpy
    func.argtypes = [
        ctypes.c_char_p,
        ctypes.c_char_p,
        ctypes.c_size_t,
        ctypes.POINTER(Tensor),
        ctypes.c_size_t,
        ctypes.c_size_t,
        ctypes.c_int,
        ctypes.c_int,
    ]
    func.restype = ctypes.c_bool
    return func
