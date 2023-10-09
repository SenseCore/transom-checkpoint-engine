import os
import time
import unittest

import torch
from engine import engine
from loguru import logger


class TestCache(unittest.TestCase):
    def test_maxIteration(self):
        logger.info("----save------")
        torch.manual_seed(7)
        raw_data = torch.randn(33554432, dtype=torch.float32)  # 125MB
        data = raw_data.repeat(20)
        save_data = {
            "iteration": 0,
            "data": data,
        }
        engine.save(
            save_data,
            "/data/test_string0a.pt",
        )
        save_data = {
            "iteration": 0,
            "data": data,
        }
        engine.save(
            save_data,
            "/data/test_string0b.pt",
        )
        # time.sleep(3)
        save_data = {
            "iteration": 1,
            "data": data,
        }
        engine.save(
            save_data,
            "/data/test_string1.pt",
        )
        # time.sleep(3)
        save_data = {
            "iteration": 1,
            "data": data,
        }
        engine.save(
            save_data,
            "/data/test_string1.pt",
        )
        save_data = {
            "iteration": 2,
            "data": data,
        }
        engine.save(
            save_data,
            "/data/test_string2.pt",
        )
        save_data = {
            "iteration": 0,
            "data": data,
        }
        engine.save(
            save_data,
            "/data/test_string0a.pt",
        )
        save_data = {
            "iteration": 0,
            "data": data,
        }
        engine.save(
            save_data,
            "/data/test_string0b.pt",
        )
        save_data = {
            "iteration": 3,
            "data": data,
        }
        engine.save(
            save_data,
            "/data/test_string3.pt",
        )
