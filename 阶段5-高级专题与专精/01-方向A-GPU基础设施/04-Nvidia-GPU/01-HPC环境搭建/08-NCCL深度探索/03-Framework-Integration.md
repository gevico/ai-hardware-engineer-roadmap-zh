---
title: 03 — PyTorch、DeepSpeed 与 Megatron 中的 NCCL
description: 03 — PyTorch、DeepSpeed 与 Megatron 中的 NCCL
---

# 03 — PyTorch、DeepSpeed 与 Megatron 中的 NCCL

## 1. PyTorch 如何使用 NCCL

PyTorch 从不在用户代码中直接调用 NCCL。取而代之的是，`torch.distributed` 模块封装了 NCCL。

### 完整调用栈

```
User code: loss.backward()
    ↓
DDP hook: _allreduce_fut = all_reduce_coalesced(grads)
    ↓
torch.distributed.all_reduce()
    ↓
torch._C._distributed_c10d.ProcessGroupNCCL.allreduce()  ← C++ layer
    ↓
ncclAllReduce(sendbuff, recvbuff, count, datatype, op, comm, stream)  ← NCCL C API
    ↓
NVLink DMA engine  ← hardware
```

### ProcessGroupNCCL 内部机制

PyTorch 的 `ProcessGroupNCCL` 负责管理：
- **NCCL communicator 生命周期**（创建、缓存、销毁）
- **CUDA 流管理**（每个 NCCL 操作用独立流以实现异步）
- **错误检测**（watchdog 线程监控挂起）
- **工作队列**（缓冲 NCCL 调用以实现流水线）

```python
# What happens when you call dist.init_process_group("nccl", ...)
# 1. Each rank creates an ncclUniqueId (like a session token)
# 2. Rank 0 broadcasts the unique ID to all ranks
# 3. All ranks call ncclCommInitRank() → creates NCCL communicator
# 4. NCCL runs topology detection (which GPUs share NVLink, PCIe, etc.)
# 5. NCCL selects optimal ring/tree topology for each communicator

# Internally, ncclCommInitRank does:
# - Probes NVLink topology via nvmlDeviceGetNvLinkCapability()
# - Tests inter-GPU bandwidth to rank GPUs
# - Builds ring and tree graphs for AllReduce/AllGather/ReduceScatter
```

### DDP 通信 hook

DDP 通过 hook 让 AllReduce 与反向计算重叠：

```python
# Simplified DDP gradient hook (PyTorch source)
def _make_param_hook(self, param, bucket_view):
    def hook(*args):
        if param.requires_grad and param.grad is not None:
            # Mark this param as ready for AllReduce
            self.reducer.autograd_hook(bucket_view)
            # When all params in a bucket are ready:
            #   NCCL AllReduce fires asynchronously
            #   Continues backward pass without waiting
    return hook

# Bucket structure: parameters are grouped into ~25 MB buckets
# Each bucket is AllReduced independently as it becomes ready
# This is what enables compute-communication overlap

# Tune bucket size:
model = DDP(model, bucket_cap_mb=50)  # increase for fewer, larger AllReduces
```

---

## 2. FSDP（FullyShardedDataParallel）—— 每一步都要用 NCCL

FSDP 把模型参数分片到多块 GPU 上，并在**每一次前向与反向传播**中使用 NCCL：

```
Forward pass of layer i:
  1. NCCL AllGather: reconstruct full parameter from shards
     [GPU0 shard] [GPU1 shard] [GPU2 shard] ... → [Full param on all GPUs]
  2. Compute forward pass with full parameter
  3. Free gathered full parameter from memory (only keep shard)

Backward pass of layer i:
  1. NCCL AllGather: reconstruct full parameter (needed for gradient computation)
  2. Compute local gradient
  3. NCCL ReduceScatter: reduce gradients, each GPU keeps only its shard
     [Full grad on all GPUs] → [GPU0 shard] [GPU1 shard] ...
  4. Free full gradient, update shard with optimizer
```

**每层每步的 NCCL 操作数 = 2 次 AllGather + 1 次 ReduceScatter = 3 次集合通信**

对于 80 层的 Llama-3 70B：每个训练步 80 × 3 = 240 次 NCCL 操作。

```python
# FSDP uses two NCCL groups:
# 1. process_group: for parameter AllGather and gradient ReduceScatter
# 2. reduce_scatter_process_group (optional): separate group for gradient reduction

from torch.distributed.fsdp import FullyShardedDataParallel as FSDP
from torch.distributed.fsdp import ShardingStrategy

model = FSDP(
    model,
    sharding_strategy=ShardingStrategy.FULL_SHARD,    # ZeRO-3: AllGather + ReduceScatter
    # ShardingStrategy.SHARD_GRAD_OP,                  # ZeRO-2: ReduceScatter only
    # ShardingStrategy.NO_SHARD,                       # ZeRO-0: no sharding (DDP)
    process_group=dist.new_group(ranks=list(range(world_size))),
)
```

---

## 3. DeepSpeed ZeRO 与 NCCL

DeepSpeed 的 ZeRO 优化器各阶段使用的 NCCL 操作逐级增多：

### ZeRO Stage 1 —— 优化器状态分片

```
Forward: standard (no extra NCCL)
Backward: standard gradient AllReduce (like DDP)
Optimizer step: each GPU updates only its shard of optimizer states
After optimizer: NCCL AllGather to reconstruct full parameters for next forward
```

### ZeRO Stage 2 —— 加上梯度分片

```
Backward:
  After each bucket completes: NCCL ReduceScatter (not AllReduce)
  Each GPU accumulates only its gradient shard
  Less memory: no need to store full gradient tensor

After all layers: NCCL AllGather for parameters (same as ZeRO-1)
```


<details>
<summary>English original</summary>

**03 — NCCL Inside PyTorch, DeepSpeed, and Megatron**

**1. How PyTorch Uses NCCL**

PyTorch never calls NCCL directly in user code. Instead, the `torch.distributed` module wraps NCCL.

**The Full Call Stack**

```
User code: loss.backward()
    ↓
DDP hook: _allreduce_fut = all_reduce_coalesced(grads)
    ↓
torch.distributed.all_reduce()
    ↓
torch._C._distributed_c10d.ProcessGroupNCCL.allreduce()  ← C++ layer
    ↓
ncclAllReduce(sendbuff, recvbuff, count, datatype, op, comm, stream)  ← NCCL C API
    ↓
NVLink DMA engine  ← hardware
```

**ProcessGroupNCCL Internals**

PyTorch's `ProcessGroupNCCL` manages:
- **NCCL communicator lifecycle** (creation, caching, destruction)
- **CUDA stream management** (a separate stream per NCCL op for async)
- **Error detection** (watchdog thread monitors for hangs)
- **Work queue** (buffers NCCL calls for pipelining)

```python
# What happens when you call dist.init_process_group("nccl", ...)
# 1. Each rank creates an ncclUniqueId (like a session token)
# 2. Rank 0 broadcasts the unique ID to all ranks
# 3. All ranks call ncclCommInitRank() → creates NCCL communicator
# 4. NCCL runs topology detection (which GPUs share NVLink, PCIe, etc.)
# 5. NCCL selects optimal ring/tree topology for each communicator

# Internally, ncclCommInitRank does:
# - Probes NVLink topology via nvmlDeviceGetNvLinkCapability()
# - Tests inter-GPU bandwidth to rank GPUs
# - Builds ring and tree graphs for AllReduce/AllGather/ReduceScatter
```

**DDP Communication Hook**

DDP overlaps AllReduce with backward computation using hooks:

```python
# Simplified DDP gradient hook (PyTorch source)
def _make_param_hook(self, param, bucket_view):
    def hook(*args):
        if param.requires_grad and param.grad is not None:
            # Mark this param as ready for AllReduce
            self.reducer.autograd_hook(bucket_view)
            # When all params in a bucket are ready:
            #   NCCL AllReduce fires asynchronously
            #   Continues backward pass without waiting
    return hook

# Bucket structure: parameters are grouped into ~25 MB buckets
# Each bucket is AllReduced independently as it becomes ready
# This is what enables compute-communication overlap

# Tune bucket size:
model = DDP(model, bucket_cap_mb=50)  # increase for fewer, larger AllReduces
```

---

**2. FSDP (FullyShardedDataParallel) — NCCL at Every Step**

FSDP shards model parameters across GPUs and uses NCCL for **every forward and backward pass**:

```
Forward pass of layer i:
  1. NCCL AllGather: reconstruct full parameter from shards
     [GPU0 shard] [GPU1 shard] [GPU2 shard] ... → [Full param on all GPUs]
  2. Compute forward pass with full parameter
  3. Free gathered full parameter from memory (only keep shard)

Backward pass of layer i:
  1. NCCL AllGather: reconstruct full parameter (needed for gradient computation)
  2. Compute local gradient
  3. NCCL ReduceScatter: reduce gradients, each GPU keeps only its shard
     [Full grad on all GPUs] → [GPU0 shard] [GPU1 shard] ...
  4. Free full gradient, update shard with optimizer
```

**NCCL operations per layer per step = 2 AllGathers + 1 ReduceScatter = 3 collectives**

For 80-layer Llama-3 70B: 80 × 3 = 240 NCCL operations per training step.

```python
# FSDP uses two NCCL groups:
# 1. process_group: for parameter AllGather and gradient ReduceScatter
# 2. reduce_scatter_process_group (optional): separate group for gradient reduction

from torch.distributed.fsdp import FullyShardedDataParallel as FSDP
from torch.distributed.fsdp import ShardingStrategy

model = FSDP(
    model,
    sharding_strategy=ShardingStrategy.FULL_SHARD,    # ZeRO-3: AllGather + ReduceScatter
    # ShardingStrategy.SHARD_GRAD_OP,                  # ZeRO-2: ReduceScatter only
    # ShardingStrategy.NO_SHARD,                       # ZeRO-0: no sharding (DDP)
    process_group=dist.new_group(ranks=list(range(world_size))),
)
```

---

**3. DeepSpeed ZeRO and NCCL**

DeepSpeed's ZeRO optimizer stages use progressively more NCCL operations:

**ZeRO Stage 1 — Optimizer State Sharding**

```
Forward: standard (no extra NCCL)
Backward: standard gradient AllReduce (like DDP)
Optimizer step: each GPU updates only its shard of optimizer states
After optimizer: NCCL AllGather to reconstruct full parameters for next forward
```

**ZeRO Stage 2 — + Gradient Sharding**

```
Backward:
  After each bucket completes: NCCL ReduceScatter (not AllReduce)
  Each GPU accumulates only its gradient shard
  Less memory: no need to store full gradient tensor

After all layers: NCCL AllGather for parameters (same as ZeRO-1)
```

</details>

### ZeRO Stage 3 — + 参数分片

```
Forward layer i:
  NCCL AllGather parameters of layer i
  Compute forward
  Free gathered parameters

Backward layer i:
  NCCL AllGather parameters of layer i (for gradient computation)
  NCCL ReduceScatter gradients
  Free gathered parameters and full gradients

Optimizer:
  Each GPU updates its parameter shards locally (no communication)
```

```python
# DeepSpeed ZeRO-3 config
{
  "zero_optimization": {
    "stage": 3,
    "overlap_comm": true,            # overlap AllGather with backward compute
    "contiguous_gradients": true,    # pack grads into contiguous buffer for AllReduce
    "reduce_bucket_size": 5e8,       # 500 MB bucket → fewer, larger NCCL calls
    "stage3_prefetch_bucket_size": 5e8,  # prefetch next layer's params in advance
    "stage3_param_persistence_threshold": 1e6  # keep small params ungathered (cached)
  }
}
```

---

## 4. Megatron-LM 张量并行 — layer 内部的 NCCL

Megatron-LM 把 NCCL 用在 Transformer layer **内部**（而不只是在 layer 之间）。

### 列并行线性层（QKV 投影）

```
Weight W [H, 3H] split column-wise across N GPUs:
  GPU0: W_col[:, 0:3H/N]
  GPU1: W_col[:, 3H/N:6H/N]
  ...
  GPU7: W_col[:, 21H/N:3H]

Input X [B, S, H] replicated on all GPUs

Forward:
  Each GPU: Y_i = X @ W_col_i      → [B, S, H/N]   (local, no comm)

Output Y_i is consumed by per-head attention (also sharded) — no AllReduce needed here!
```

### 行并行线性层（输出投影）

```
Weight W [H, H] split row-wise across N GPUs:
  GPU0: W_row[0:H/N, :]
  GPU1: W_row[H/N:2H/N, :]
  ...

Input Y [B, S, H/N] already sharded from previous column-parallel layer

Forward:
  Each GPU: Z_i = Y_i @ W_row_i    → [B, S, H]  (each partial sum)

  NCCL AllReduce: Z = Z_0 + Z_1 + ... + Z_7   → [B, S, H] on all GPUs
                                       ↑
                               This is the only NCCL call per MLP/attention block
```

**Megatron-LM 中的 NCCL 调用频率（TP=8，80 个 layer）：**
- Attention：每 layer 1 次 AllReduce（输出投影之后）
- 多层感知机：每 layer 1 次 AllReduce（下投影之后）
- 合计：每 layer 2 次 AllReduce × 80 个 layer = **每次前向传播 160 次 AllReduce**
- 每次 AllReduce 的张量大小：B × S × H × 2 bytes（BF16）
  - 当 B=4，S=2048，H=8192 时：4 × 2048 × 8192 × 2 = 每次 AllReduce 134 MB

```python
# Megatron's actual AllReduce call (simplified)
from megatron.core.parallel_state import get_tensor_model_parallel_group

def linear_with_grad_accumulation_and_async_allreduce(
    input, weight, bias, sequence_parallel
):
    output = torch.mm(input, weight.t())

    if not sequence_parallel:
        # Standard: AllReduce after row-parallel linear
        handle = torch.distributed.all_reduce(
            output,
            group=get_tensor_model_parallel_group(),
            async_op=True,   # overlap with next operation
        )
    return output, handle
```

---

## 5. 通信-计算重叠模式

所有框架中最重要的一项 NCCL 优化，就是把通信与计算**重叠**起来。

### DDP Bucket 重叠

```
Without overlap:
  [forward]────────[backward layer 80→1]────────[AllReduce all grads]─[optimizer]
                                                  ← waits for all grads ↗

With DDP bucket overlap:
  [forward]──[backward: layer 80-70]──[AR bucket 1]
                                ↘──[backward: layer 69-60]──[AR bucket 2]
                                                       ↘──[backward: layer 59-50]──[AR bucket 3]
                                                                              ↘──...
  AllReduce of early layers overlaps with backward of later layers → ~30% speedup
```

### FSDP Prefetch 重叠

```
[AllGather layer i params] [Forward layer i] [AllGather layer i+1 params]
                          ↘                ↗               ↘
                           [overlap both AllGathers with forward]

FSDP prefetches next layer's parameters during current layer's forward/backward.
```

```python
# Enable prefetching in FSDP
from torch.distributed.fsdp import FullyShardedDataParallel as FSDP

model = FSDP(
    model,
    forward_prefetch=True,       # prefetch next layer params during forward
    backward_prefetch=BackwardPrefetch.BACKWARD_PRE,  # prefetch during backward
)
```

---

## 6. 框架实操配置


<details>
<summary>English original</summary>

**ZeRO Stage 3 — + Parameter Sharding**

```
Forward layer i:
  NCCL AllGather parameters of layer i
  Compute forward
  Free gathered parameters

Backward layer i:
  NCCL AllGather parameters of layer i (for gradient computation)
  NCCL ReduceScatter gradients
  Free gathered parameters and full gradients

Optimizer:
  Each GPU updates its parameter shards locally (no communication)
```

```python
# DeepSpeed ZeRO-3 config
{
  "zero_optimization": {
    "stage": 3,
    "overlap_comm": true,            # overlap AllGather with backward compute
    "contiguous_gradients": true,    # pack grads into contiguous buffer for AllReduce
    "reduce_bucket_size": 5e8,       # 500 MB bucket → fewer, larger NCCL calls
    "stage3_prefetch_bucket_size": 5e8,  # prefetch next layer's params in advance
    "stage3_param_persistence_threshold": 1e6  # keep small params ungathered (cached)
  }
}
```

---

**4. Megatron-LM Tensor Parallelism — NCCL Inside Layers**

Megatron-LM uses NCCL **inside** the transformer layer (not just between layers).

**Column Parallel Linear (QKV projection)**

```
Weight W [H, 3H] split column-wise across N GPUs:
  GPU0: W_col[:, 0:3H/N]
  GPU1: W_col[:, 3H/N:6H/N]
  ...
  GPU7: W_col[:, 21H/N:3H]

Input X [B, S, H] replicated on all GPUs

Forward:
  Each GPU: Y_i = X @ W_col_i      → [B, S, H/N]   (local, no comm)

Output Y_i is consumed by per-head attention (also sharded) — no AllReduce needed here!
```

**Row Parallel Linear (output projection)**

```
Weight W [H, H] split row-wise across N GPUs:
  GPU0: W_row[0:H/N, :]
  GPU1: W_row[H/N:2H/N, :]
  ...

Input Y [B, S, H/N] already sharded from previous column-parallel layer

Forward:
  Each GPU: Z_i = Y_i @ W_row_i    → [B, S, H]  (each partial sum)

  NCCL AllReduce: Z = Z_0 + Z_1 + ... + Z_7   → [B, S, H] on all GPUs
                                       ↑
                               This is the only NCCL call per MLP/attention block
```

**NCCL call frequency in Megatron-LM (TP=8, 80 layers):**
- Attention: 1 AllReduce per layer (after output projection)
- MLP: 1 AllReduce per layer (after down projection)
- Total: 2 AllReduces per layer × 80 layers = **160 AllReduces per forward pass**
- Each AllReduce tensor size: B × S × H × 2 bytes (BF16)
  - For B=4, S=2048, H=8192: 4 × 2048 × 8192 × 2 = 134 MB per AllReduce

```python
# Megatron's actual AllReduce call (simplified)
from megatron.core.parallel_state import get_tensor_model_parallel_group

def linear_with_grad_accumulation_and_async_allreduce(
    input, weight, bias, sequence_parallel
):
    output = torch.mm(input, weight.t())

    if not sequence_parallel:
        # Standard: AllReduce after row-parallel linear
        handle = torch.distributed.all_reduce(
            output,
            group=get_tensor_model_parallel_group(),
            async_op=True,   # overlap with next operation
        )
    return output, handle
```

---

**5. Communication-Computation Overlap Patterns**

The most important NCCL optimization across all frameworks is **overlapping** communication with computation.

**DDP Bucket Overlap**

```
Without overlap:
  [forward]────────[backward layer 80→1]────────[AllReduce all grads]─[optimizer]
                                                  ← waits for all grads ↗

With DDP bucket overlap:
  [forward]──[backward: layer 80-70]──[AR bucket 1]
                                ↘──[backward: layer 69-60]──[AR bucket 2]
                                                       ↘──[backward: layer 59-50]──[AR bucket 3]
                                                                              ↘──...
  AllReduce of early layers overlaps with backward of later layers → ~30% speedup
```

**FSDP Prefetch Overlap**

```
[AllGather layer i params] [Forward layer i] [AllGather layer i+1 params]
                          ↘                ↗               ↘
                           [overlap both AllGathers with forward]

FSDP prefetches next layer's parameters during current layer's forward/backward.
```

```python
# Enable prefetching in FSDP
from torch.distributed.fsdp import FullyShardedDataParallel as FSDP

model = FSDP(
    model,
    forward_prefetch=True,       # prefetch next layer params during forward
    backward_prefetch=BackwardPrefetch.BACKWARD_PRE,  # prefetch during backward
)
```

---

**6. Practical Framework Setup**

</details>

### PyTorch DDP — 最小可运行示例

```python
# train_ddp.py
import torch
import torch.distributed as dist
from torch.nn.parallel import DistributedDataParallel as DDP
import os

def main():
    # NCCL backend init (reads RANK, WORLD_SIZE, MASTER_ADDR, MASTER_PORT from env)
    dist.init_process_group(backend="nccl")
    rank = dist.get_rank()
    torch.cuda.set_device(rank)

    model = MyModel().cuda()
    model = DDP(model, device_ids=[rank])

    optimizer = torch.optim.AdamW(model.parameters(), lr=1e-4, fused=True)

    for batch in get_dataloader(rank):
        with torch.autocast("cuda", dtype=torch.bfloat16):
            loss = model(batch).loss
        loss.backward()
        # NCCL AllReduce fires automatically here via DDP gradient hook
        optimizer.step()
        optimizer.zero_grad()

    dist.destroy_process_group()

if __name__ == "__main__":
    main()
```

```bash
# torchrun sets RANK, WORLD_SIZE, MASTER_ADDR, MASTER_PORT automatically
torchrun --nproc_per_node=8 train_ddp.py
```

### DeepSpeed — 最小可运行示例

```python
# train_ds.py
import deepspeed
import torch

def main():
    model = MyModel()
    optimizer = torch.optim.AdamW(model.parameters())

    # DeepSpeed handles NCCL init, gradient sync, ZeRO sharding
    model_engine, optimizer, _, _ = deepspeed.initialize(
        model=model,
        optimizer=optimizer,
        config="ds_config.json",  # ZeRO stage, BF16, etc.
    )

    for batch in dataloader:
        loss = model_engine(batch).loss
        model_engine.backward(loss)
        # NCCL ReduceScatter (ZeRO-3) fires here
        model_engine.step()
        # NCCL AllGather fires here to restore parameters
```

```bash
deepspeed --num_gpus=8 train_ds.py
```

---

## 参考资料

- [PyTorch DDP Internal Design](https://pytorch.org/docs/stable/notes/ddp.html)
- [FSDP Source Code](https://github.com/pytorch/pytorch/tree/main/torch/distributed/fsdp)
- [DeepSpeed ZeRO Paper](https://arxiv.org/abs/1910.02054)
- [Megatron-LM Source](https://github.com/NVIDIA/Megatron-LM/blob/main/megatron/core/tensor_parallel/layers.py)

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/NCCL-Deep-Dive/03-Framework-Integration.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/NCCL-Deep-Dive/03-Framework-Integration.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
