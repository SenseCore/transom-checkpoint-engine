import sys

from loguru import logger


class Formatter:
    """Alignment name-function-line"""

    def __init__(self):
        self.padding = 0  # {name}:{function}:{line}'s max length
        # self.fmt = "<green>{time:YYYY-MM-DD HH:mm:ss.SSSS}</green> | <level>{level: ^7}</level> | <cyan>{function}</cyan>:<cyan>{line}</cyan>{extra[padding]} - <level>{message}</level>\n{exception}"
        self.fmt = "{time:YYYY-MM-DD HH:mm:ss.SSSS} | {level: ^7} | {function}:{line}{extra[padding]} - {message}\n{exception}"

    def format(self, record):
        length = len("{function}:{line}".format(**record))
        self.padding = max(self.padding, length)
        record["extra"]["padding"] = " " * (self.padding - length)
        return self.fmt


logger.remove()
logger.add(sys.stdout, colorize=True, format=Formatter().format)

# deepspeed monkey patch
if "deepspeed" in sys.modules:
    from deepspeed.runtime.checkpoint_engine.torch_checkpoint_engine import (
        TorchCheckpointEngine,
    )

    from transomSnapshot.engine.transom_checkpoint_engine import TransomCheckpointEngine

    TorchCheckpointEngine.save = TransomCheckpointEngine.save
    TorchCheckpointEngine.load = TransomCheckpointEngine.load
    TorchCheckpointEngine.create = TransomCheckpointEngine.create
    TorchCheckpointEngine.create = TransomCheckpointEngine.commit

    print(
        "\nUse TransomCheckpointEngine instead of TorchCheckpointEngine!\n", flush=True
    )
