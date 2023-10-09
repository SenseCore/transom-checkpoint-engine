import torch
from deepspeed.runtime.checkpoint_engine.checkpoint_engine import CheckpointEngine
from deepspeed.utils import log_dist, logger

from transomSnapshot.engine import engine


class TransomCheckpointEngine(CheckpointEngine):
    def __init__(self, config_params=None):
        super().__init__(config_params)

    def create(self, tag):
        log_dist(f"[Transom] Checkpoint {tag} is about to be saved!", ranks=[0])

    def save(self, state_dict, path: str):
        logger.info(f"[Transom] Saving {path}...")
        engine.save(state_dict, path)
        logger.info(f"[Transom] Saved {path}.")
        return None

    def load(self, path: str, map_location=None):
        logger.info(f"[Transom] Loading checkpoint from {path}...")
        partition = engine.load(path, map_location=map_location)
        logger.info(f"[Transom] Loaded checkpoint from {path}.")
        return partition

    def commit(self, tag):
        logger.info(f"[Transom] Checkpoint {tag} is ready now!")
        return True
