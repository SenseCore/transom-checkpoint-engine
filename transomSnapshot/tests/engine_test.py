import os
import time
import unittest
from json import load

import torch
from engine import engine
from loguru import logger

path = "/data/global_step1234/layer_17-model_01-model_states.pt"
if os.getenv("CKPT_PATH") is not None:
    path = os.getenv("CKPT_PATH")


class TestCache(unittest.TestCase):
    # def test_string(self):
    #     logger.info("----save------")
    #     save_data = {"iteration": 4, "data": "abcdefghijklmn"}
    #     engine.save(
    #         save_data,
    #         "/data/test_string.pt",
    #     )
    #     logger.info("----load------")
    #     load_data = engine.load("/data/test_string.pt")
    #     logger.info(f"load_data: {load_data}")
    #     assert save_data == load_data

    def test_cpu_tensor(self):
        logger.info("----save------")
        torch.manual_seed(7)
        raw_data = torch.randn(3355443, dtype=torch.float32)  # 125MB
        data = raw_data.repeat(20)
        data1 = raw_data.repeat(20)
        data2 = raw_data.repeat(20)
        data3 = raw_data.repeat(20)
        logger.info(f"save data: {data[:5]}")
        save_data = {
            "111111": "cccccc",
            "data": data,
            "data1": data1,
            "data2": data2,
            "data3": data3,
        }
        start = time.perf_counter()
        engine.save(
            save_data,
            path,
        )
        logger.info(f"engine save in {time.perf_counter() - start:0.4f} seconds.")

        logger.info("----load------")
        start = time.perf_counter()
        load_data = engine.load(
            path,
        )
        logger.info(f"engine load in {time.perf_counter() - start:0.4f} seconds.")
        load_tensor = load_data["data"]
        logger.info(f"load data: {load_tensor[:5]}")
        logger.info(
            "save_data == load_data? {} {} {} {}".format(
                torch.equal(save_data["data"], load_data["data"]),
                torch.equal(save_data["data1"], load_data["data1"]),
                torch.equal(save_data["data2"], load_data["data2"]),
                torch.equal(save_data["data3"], load_data["data3"]),
            )
        )

    # def test_gpu_tensor(self):
    #     torch.manual_seed(7)
    #     raw_data2 = torch.randn(100, dtype=torch.float32).cuda()  # 125MB
    #     data = raw_data2.repeat(20)
    #     data1 = raw_data2.repeat(20)
    #     data2 = raw_data2.repeat(20)
    #     data3 = raw_data2.repeat(20)
    #     # repeat_meta = torch.ones(33554432)  # 125MB
    #     # logger.info(f"load data: {data}")
    #     save_data = {
    #         "111111": "cccccc",
    #         "data": data,
    #         "data1": data1,
    #         "data2": data2,
    #         "data3": data3,
    #     }
    #     engine.save(save_data, "test_tensor.pt")
    #     logger.info("----load------")
    #     start = time.perf_counter()
    #     load_data = engine.load("test_tensor.pt")
    #     logger.info(f"engine save in {time.perf_counter() - start:0.4f} seconds.")
    #     load_tensor = load_data["data"]
    #     logger.info(f"load data: {load_tensor[:5]}")
    #     logger.info(
    #         "save_data == load_data? {} {} {} {}".format(
    #             torch.equal(save_data["data"], load_data["data"]),
    #             torch.equal(save_data["data1"], load_data["data1"]),
    #             torch.equal(save_data["data2"], load_data["data2"]),
    #             torch.equal(save_data["data3"], load_data["data3"]),
    #         )
    #     )
