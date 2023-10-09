# Transom Checkpoint Engine

## background

**Checkpoint** in pytorch has two different meaning, one is general checkpoint, the other is [TORCH.UTILS.CHECKPOINT](https://pytorch.org/docs/stable/checkpoint.html). This project proposes a light-weighted general checkpoint acceleration system that improves checkpointing efficiency.

The background and motivation of this system is depicted in detail at [BACKGROUND](docs/background.md). Just declare the conclusion here: In large-scale LLM training, our system improves training efficiency by around **~X%**.

If you're academically interested in transom or checkpoint subsystem, please refer to our paper: [TBD](TBD).

## architecture & workflow

The modular composition and general workflow is depicted [here](docs/ckpt-arch.drawio.svg). The project consists of a python client and cpp server.

The key points of workflow could be concluded as **using in-memory cache** and **async backup & persistence**.

Checkpoints are first written to the in-memory cache and persisted asynchronously to reliable storage, hiding the latency of slow persistence operations. By optimizing the checkpoint lifecycle in this manner, TCE can significantly reduce checkpoint latency, thus improve training efficiency.

The volatile nature of in-memory caching poses a challenge for fault tolerance in our approach, especially when failed containers are rescheduled and lose their in-memory caches. To address this issue, we leverage RDMA high-speed networking for cache backup. After scheduling all pods, TCE maps the training pod list to node ranks, with each TCE server on a given node rank asynchronously and durably backing up its checkpoint cache to the TCE server on the next node rank in sequence. TCE servers attempt to autonomously restore any lost caches after recovery by fetching the backup data from the previously backing-up node and notifying it to re-backup.

## run demo

The most straightforward to comprehend the system is to run a demo. The only requirement is that you have `docker`(if you use `podman`, remember to `alias docker=podman`) and `python3` installed.

Firstly, compile the project if you're willing to. You can use existing demo image. But if you want to build from scratch, just run `make build`, let docker handles everything!

Secondly, install python dependencies by `pip install -r requirement.txt`. Feel free to use **conda**, or anything else. Make sure you've properly configured before actually running demo script.

Thirdly, start a mysql server by `docker run --name mysql --rm -e MYSQL_ROOT_PASSWORD=sensecore1900 -d -i -p 3306:3306 registry.cn-hangzhou.aliyuncs.com/acs-sample/mysql:5.7`, start the checkpoint server locally by `docker run -ti --rm --name tce --net host --pid host --user $(id -u):$(id -u) -e CKPT_ENGINE_MYSQL_PASSWORD=sensecore1900 --rm registry.cn-hangzhou.aliyuncs.com/sensecore-transom/checkpoint-engine:1.0-ubuntu`

At last, run mnist demo by `./hack/run_demo.sh`.

*NOTE: running server in container is not recommended since `memfd` require strict permission.*

## coding detail

[Doxygen](https://www.doxygen.nl/) is utilized for auto generating document. Just run `make doc`, documents are generated under *docs* directory. All coding details are available, we write a lot of comments and are still improving.

## environment setup

It's highly recommended to use transom toolkit. (TODO: xxx)

However, you can build from sratch following [BUILD](docs/build.md).

## usage

The core of TCE is the substitution of pytorch save & load. We offer seamlessly integration with [Deepspeed](https://github.com/microsoft/DeepSpeed), also it's compatible to native pytorch.

What's more, we offer more options to precisely manage the behavior of our system.

For more detail, please refer to [USAGE](docs/usage.md).

## performance benchmark

The performance of TCE is SOTA, please refer to [BENCHMARK](docs/benchmark.md).

## contributing

If you're interested in this project and want to contribute, please read [CONTRIBUTING](CONTRIBUTING.md) at first, the code of conduct is included in that document.

## limitations

- Tensorflow and other training framworks are not supported

## future work

- optimize RDMA backup code, it's kind of ugly right now
- support other frameworks if necessary
- continuously improving code quality and fulfill tests
- utilize huge page for optimization

etc.
