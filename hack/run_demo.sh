#!/bin/bash

set -e

echo -e "please make sure you've properly installed python dependencies"

ROOT_DIR=$(cd "$(dirname $(dirname ${BASH_SOURCE[0]}))" && pwd)

CKPT_ENGINE_PATH="${ROOT_DIR}/transomSnapshot"

PYTHONPATH=$PYTHONPATH:${TORCH_VISION_PATH}:${CKPT_ENGINE_PATH} \
    CKPT_ENGINE_HTTP_PORT="15345" \
    CKPT_PATH="/tmp/ckpt-1" \
    LD_LIBRARY_PATH=/usr/local/cuda/lib64:/usr/local/cudnn/lib \
    python -m unittest \
    tests.engine_test