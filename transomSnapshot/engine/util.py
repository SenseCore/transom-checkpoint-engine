import base64
import json
import os
from enum import IntEnum

import requests
from loguru import logger


class CheckpointState(IntEnum):
    PENDING = 0
    CACHED = 1
    BACKED_UP = 2
    PERSISTENT = 3
    BORKEN = 4
    OBSOLESCENT = 5
    STATE_NUM = 6
    STATE_ANY = 99


ENGINE_SERVER_URL = (
    "http://localhost:20002/"
    if os.getenv("CKPT_ENGINE_HTTP_PORT") is None
    else "http://localhost:" + str(os.getenv("CKPT_ENGINE_HTTP_PORT"))
)


def SaveMetaRequest(
    filename: str, iteration: str, checkpointstate: CheckpointState, size: int
):
    """send SaveMetaRequest to server"""
    if filename is None:
        raise RuntimeError("filename is None")
    metadata = {
        "filename": filename,
        "iteration": iteration,
        "checkpointstate": checkpointstate.value,
        "size": size,
    }
    # logger.debug("SaveMetaRequest params: {}", metadata)
    response = requests.get(
        ENGINE_SERVER_URL + "/createMetadata", data=json.dumps(metadata)
    )
    if not response.ok:
        raise RuntimeError("send SaveMetaRequest failed")
    resp = response.json()
    logger.debug(resp["message"])
    if resp["status"] == "ERROR":
        raise RuntimeError(resp["message"])
    return resp["pid"], resp["memfd"]


def updateMetadataRequest(filename: str, checkpointstate: CheckpointState):
    """send SaveMetaRequest to server"""
    if filename is None:
        raise RuntimeError("filename is None")
    metadata = {
        "filename": filename,
        "checkpointstate": checkpointstate.value,
    }
    # logger.debug("updateMetadataRequest params: {}", metadata)
    response = requests.get(
        ENGINE_SERVER_URL + "/updateMetadata", data=json.dumps(metadata)
    )
    if not response.ok:
        raise RuntimeError("send updateMetadataRequest failed")
    resp = response.json()
    logger.debug(resp["message"])
    if resp["status"] == "ERROR":
        raise RuntimeError(resp["message"])


def LoadMetaRequest(filename: str):
    """send LoadMetaRequest to server"""
    if filename is None:
        raise RuntimeError("filename is None")
    metadata = {
        "filename": filename,
    }
    logger.debug("LoadMetaRequest params: {}", metadata)
    response = requests.get(
        ENGINE_SERVER_URL + "/getMetadata", data=json.dumps(metadata)
    )
    if not response.ok:
        RuntimeError("send LoadMetaRequest failed")
    resp = response.json()
    logger.debug(resp["message"])
    if resp["status"] == "ERROR":
        raise RuntimeError(resp["message"])
    return resp["checkpointstate"], resp["pid"], resp["memfd"]
