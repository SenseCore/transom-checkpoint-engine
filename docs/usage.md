# usage

## configure the options

TCE offers various configs through environment variables, which are listed below

| KEY | DEFAULT VALUE | MEANING |
| :-----: | :----: | :---- |
| ENV_KEY_META_CLIENT | mysql | metadata client type, e.g. mysql client stores metadata into a mysql instance |
| ENV_KEY_MYSQL_ADDR | 0.0.0.0 | address of mysql, only IP is supported for now |
| ENV_KEY_MYSQL_PORT | 3306 | port of mysql |
| ENV_KEY_MYSQL_USER | root | user name to login to mysql |
| ENV_KEY_MYSQL_PASSWORD | "" | password for mysql authn |
| ENV_KEY_MYSQL_FLUSH_TABLE | false | flush table after connected to mysql |
| ENV_KEY_TCP_PORT | 18080 | port of inter-node socket server |
| ENV_KEY_HTTP_PORT | 15345 | port of intra-node http server |
| ENV_KEY_SKIP_BOOTSTRAP | false | **only for experiment**, skip bootstrap means backup & inter-node loading is forbidden |
| CKPT_ENGINE_ENABLE_PERSISTENT | on | **only for experiment**, disable it will not persistent cache into storage, you may suffer data loss |
| ENV_KEY_LOG_LEVEL | 0 | trace level, refer to [spdlog](https://github.com/gabime/spdlog) for detail |
| ENV_MAX_ITERATION_IN_CACHE | 99999 | max rounds of cache in memory before evicted, to control memory consumption |
| ENV_KEY_MEMORY_LIMIT_GB | "" | max cache memory amount, to control memory consumption |
| TRANSOM_JOBNAME | test-job | key of job and checkpoint file name is used as primay key in database |
| TRANSOM_RANK | 0 | node rank |
| TRANSOM_WORLD_SIZE | 1 | node size in total |
| TRANSOM_HOSTS | `hostname` | hostname or IP lists of nodes in the tranining job |

## run from scratch

If you're running LLM training, it's strongly recommended that you use transom system to ease the effort of configuring and starting checkpoint engine since you must start server at each node and keep their configuration consistent.

If you insist on managing everything on our own, this sections deep dives into checkpoint engine to show how to run from scratch.

### setup TCE server

after compiling server, you should

1. setup a mysql instance
2. configure environment variables
3. run server, it's strongly recommended to run on bare-metal, or inside your training container. **Never start in a separate PID namespace**

An example of single node training is shown below.

```bash
#!/bin/bash
ROOT_DIR=$(cd "$(dirname $(dirname ${BASH_SOURCE[0]}))" && pwd)

if ! docker ps | grep mysql; then
docker run --name mysql --rm -e MYSQL_ROOT_PASSWORD=12345678 -d -i -p 3306:3306 \
    registry.cn-hangzhou.aliyuncs.com/acs-sample/mysql:5.7
fi

export TRANSOM_RANK=0                           # only one node, world size is 1, rank is 0
export TRANSOM_HOSTS="0.0.0.0"
export TRANSOM_WORLD_SIZE=1
export CKPT_ENGINE_TCP_PORT=20000               # change port if it's in-use
export CKPT_ENGINE_HTTP_PORT=20002
export CKPT_ENGINE_MYSQL_ADDR="0.0.0.0"         # depends on your sql config
export CKPT_ENGINE_MYSQL_PORT="3306"
export CKPT_ENGINE_MYSQL_USER="root"
export CKPT_ENGINE_MYSQL_PASSWORD="12345678"
export CKPT_ENGINE_MYSQL_FLUSH=true
export CKPT_ENGINE_MAX_ITERATION_IN_CACHE=3     # the value does not matter, just an example
export CKPT_ENGINE_MEM_LIMIT_GB=10

./build/transom_snapshot_server
```

### modify python code

All you do is

```python
from transomSnapshot.engine import engine
```

If you utilize Deepspeed, it's enough. But you utilize native pytorch, replace `torch.save()` with `engine.save()`. It's guaranteed that `engine.save()` and `engine.load()` is completely compatible to pytorch.

Now enjoy the extremely fast checkpoint system!