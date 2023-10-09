# how to build from scratch

The compilation is tested on ubuntu 22.04 and centos 7. If you're using other distributions, the success of build cannot be guaranteed. You may need to search google for hints.

## pre-requisition

Python dependencies can be installed by `pip install -r requirements.txt`.

Cpp server mainly relies on mysql, spdlog, protobuf, brpc. The compilation and installation can be found at our [build dockerfile](../Dockerfile.build).

## build cpp server

just run `./hack/build.sh` as long as all requirements are met.