#!/bin/bash

# This scripts show how to setup checkpoint server for a two-node distributed training job

ROOT_DIR=$(cd "$(dirname $(dirname ${BASH_SOURCE[0]}))" && pwd)

CKPT_ENGINE_MYSQL_PASSWORD="12345678"

export TRANSOM_RANK=1
export TRANSOM_HOSTS="10.198.32.49,10.198.32.48"
export TRANSOM_WORLD_SIZE=2
export CKPT_ENGINE_TCP_PORT=20000
export CKPT_ENGINE_HTTP_PORT=20002
export CKPT_ENGINE_MYSQL_ADDR="10.198.32.49"
export CKPT_ENGINE_MYSQL_PORT="3306"
export CKPT_ENGINE_MYSQL_USER="root"
export CKPT_ENGINE_MYSQL_PASSWORD=${CKPT_ENGINE_MYSQL_PASSWORD}
export IS_PERSISTENT=off
export CKPT_ENGINE_MAX_ITERATION_IN_CACHE=2
export CKPT_ENGINE_MEM_LIMIT_GB=6
./build/transom_snapshot_server