# background & motivation

## what is general checkpoint and why it's necessary

A general checkpoint consists of model states, gradients, optimizer states, maybe loss and epch info. It helps **snapshot** training job, thus it can be resumed from the snapshot on software/hardware problems.

In google, there're hardware failure everyday. Though the probability for each server is relatively small, when it scales to hundreds, even thousands of servers, the probility of hardware failure for the whole datacenter is surprisingly high. As a result, in large-scale LLM training, general checkpoints are necessary.

## checkpoint interval: a dilemma

Assuming hardware scale and hardware failure rate is static. Assuming time cost for each iteration is static. The final end-to-end training time is affected additionally by total checkpoint time.

Choose checkpoint interval is non-trivial and in dilemma. If you checkpoint too often, the overhead is relatively high, while you lose less progress on training failure. If you checkpoint too infrequent, the overhead is trivial, but you may suffer more progress lost on training failure.

The proper solution is not to find the proper interval, but to improve checkpoint efficiency. If overhead is trivial, users can checkpoint very often, without hurting training efficiency.

## what's the drawback of saving/loading checkpoints in pytorch/deepspeed

Currently, pytorch saves checkpoint to file system(also loads from). Due to training is distributed, a shared file storage is necessary. Of course, users can hack pytorch to use object storage, it doesn't matter.

The procedure of saving checkpoint is as follows:

![pytorch-save-checkpoint-procedure](pytorch-save-checkpoint-procedure.png)

User data structure is firstly serialized, then they're analysised to figure out all tensors. Each tensor is then copied on sequence from GPU memory to host memory, then being compressed and written to file. 

Network attached storage has two side-effects.

1. the bandwidth is limited, ranks race for bandwidth at server side
2. there're usually **slow** ranks that are still transmitting data after others finish their job

As a result, users may suffer from slow IO operations, even torch barrier timeout.

The problem gets worse when loading. For example, in deepspeed, every `DP` ranks share the same model state. One rank saves, all ranks read. The total size of loading is even larger.

## how to improve checkpoint efficiency

This is what we want to address. By caching checkpoint in local memory and async persistent to storage, the checkpoint efficiency is amazingly improved.

## related work

- [CPR: UNDERSTANDING AND IMPROVING FAILURE TOLERANT TRAINING FOR DEEP LEARNING RECOMMENDATION WITH PARTIAL RECOVERY](https://arxiv.org/abs/2011.02999): this paper proposes a method for partial recovery, but it's less practical in real world
- [Optimize Checkpoint Performance for Large Models](https://learn.microsoft.com/en-us/azure/machine-learning/reference-checkpoint-performance-for-large-models?view=azureml-api-2&tabs=PYTORCH): Azure Machine Learning proposes the idea of cache & async persistent. Our work is similar to theirs, with better performance and are open-source
