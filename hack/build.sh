#!/bin/bash
set -e

ROOT_DIR=$(cd "$(dirname $(dirname ${BASH_SOURCE[0]}))" && pwd)

mkdir -p ${ROOT_DIR}/build \
    && cd ${ROOT_DIR}/build \
    && cmake .. -DBRPC_LIB_DIR=/usr/lib64 \
    && make -j

if which nvcc; then
    cd ${ROOT_DIR}/transomSnapshot/transom_memcpy && make
else
    echo -e "nvcc not found!"
fi

# for packaging python lib
cp ${ROOT_DIR}/build/transom_snapshot_server ${ROOT_DIR}/transomSnapshot/
