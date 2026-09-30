---
title: 01 — NCCL 基础
description: 01 — NCCL 基础
---

# 01 — NCCL 基础

## 1. NCCL 是什么

NCCL 是一个**集合通信原语库**，专门针对 NVIDIA GPU 拓扑优化。它位于 ML 框架与硬件之间：

```
┌────────────────────────────────────┐
│    PyTorch / TensorFlow / JAX      │  ← You write code here
├────────────────────────────────────┤
│         NCCL 2.x Library           │  ← Collective operations
├───────────────────┬────────────────┤
│   CUDA / cuDNN    │  UCX / libfabric│  ← Compute & network primitives
├───────────────────┼────────────────┤
│  NVLink / NVSwitch │  IB / Ethernet │  ← Physical interconnect
└───────────────────┴────────────────┘
```

NCCL 自动检测 GPU 拓扑，并挑出**最优通信路径**（NVLink、PCIe 还是 InfiniBand），无需手动指定。

## 2. 五个核心集合通信操作

### AllReduce —— 最重要的一个

AllReduce 在所有 GPU 上对张量做规约，并把**结果分发回所有 GPU**。

```
Before AllReduce:
  GPU0: [1, 2, 3, 4]
  GPU1: [5, 6, 7, 8]
  GPU2: [9, 0, 1, 2]
  GPU3: [3, 4, 5, 6]

After AllReduce (sum):
  GPU0: [18, 12, 16, 20]
  GPU1: [18, 12, 16, 20]
  GPU2: [18, 12, 16, 20]
  GPU3: [18, 12, 16, 20]
```

在分布式训练中：每个 GPU 计算本地梯度 → AllReduce 对其求平均 → 所有 GPU 执行相同的优化器更新 → 模型保持同步。

```python
import torch.distributed as dist
import torch

# Each GPU has computed its local gradient
local_grad = compute_gradient(...)   # shape [hidden, hidden]

# NCCL AllReduce: average across all GPUs in-place
dist.all_reduce(local_grad, op=dist.ReduceOp.SUM)
local_grad /= dist.get_world_size()

# Now all GPUs have the same averaged gradient
optimizer.step()
```

**调用时机：** DDP 训练中每次反向传播之后。
**传输数据：** 完整梯度张量，两次（先规约再广播）。

---

### Broadcast —— 把一个张量发给所有 GPU

```
Before:
  GPU0 (src): [1.0, 2.0, 3.0]
  GPU1:        [?,   ?,   ?  ]
  GPU2:        [?,   ?,   ?  ]

After Broadcast from GPU0:
  GPU0: [1.0, 2.0, 3.0]
  GPU1: [1.0, 2.0, 3.0]
  GPU2: [1.0, 2.0, 3.0]
```

```python
# Broadcast initial model weights from rank 0 to all GPUs
# (ensures all GPUs start with identical weights)
tensor = model_weights if rank == 0 else torch.empty_like(model_weights)
dist.broadcast(tensor, src=0)
```

**调用时机：** DDP 中的模型初始化；广播批索引。

---

### Reduce —— 把张量汇总到一个 GPU

```
Before:
  GPU0: [1, 2]
  GPU1: [3, 4]
  GPU2: [5, 6]

After Reduce to GPU0 (sum):
  GPU0: [9, 12]   ← result
  GPU1: [3,  4]   ← unchanged
  GPU2: [5,  6]   ← unchanged
```

```python
dist.reduce(tensor, dst=0, op=dist.ReduceOp.SUM)
```

**调用时机：** 将指标（损失、准确率）汇总到单个 rank 以便记录日志。

---

### AllGather —— 在所有 GPU 上收集全部张量

每个 GPU 贡献一个**分片**；所有 GPU 收到**拼接后的完整张量**。

```
Before (each GPU holds 1 shard of a 4-shard tensor):
  GPU0: [A]   → shard 0
  GPU1: [B]   → shard 1
  GPU2: [C]   → shard 2
  GPU3: [D]   → shard 3

After AllGather:
  GPU0: [A, B, C, D]
  GPU1: [A, B, C, D]
  GPU2: [A, B, C, D]
  GPU3: [A, B, C, D]
```

```python
# Collect sharded embeddings (e.g., tensor parallel)
local_shard = model.embedding_shard   # each GPU has 1/N of the embedding table
full_embedding = [torch.zeros_like(local_shard) for _ in range(world_size)]
dist.all_gather(full_embedding, local_shard)
```

**调用时机：** FSDP 在前向传播前收集参数；Megatron 中的张量并行。

---

### ReduceScatter —— 先规约再分发（ZeRO 的核心操作）

AllGather 的逆操作。每个 GPU 贡献一个完整张量；各自收到一个**规约后的分片**。

```
Before (each GPU has the full gradient):
  GPU0: [A0, A1, A2, A3]
  GPU1: [B0, B1, B2, B3]
  GPU2: [C0, C1, C2, C3]
  GPU3: [D0, D1, D2, D3]

After ReduceScatter (sum, 4 GPUs → 4 shards):
  GPU0: [A0+B0+C0+D0]   ← sum of column 0
  GPU1: [A1+B1+C1+D1]   ← sum of column 1
  GPU2: [A2+B2+C2+D2]   ← sum of column 2
  GPU3: [A3+B3+C3+D3]   ← sum of column 3
```

```python
# ZeRO-3: each GPU only stores its shard of the gradient
output_shard = torch.zeros(grad_size // world_size, device="cuda")
dist.reduce_scatter(output_shard, full_grad_list, op=dist.ReduceOp.SUM)
# Now GPU i holds only the i-th shard of the summed gradient
```

**调用时机：** ZeRO-1/2/3 的梯度分片；FSDP 的反向传播。

---


<details>
<summary>English original</summary>

**01 — NCCL Fundamentals**

**1. What NCCL Is**

NCCL is a **library of collective communication primitives** optimized specifically for NVIDIA GPU topologies. It sits between your ML framework and the hardware:

```
┌────────────────────────────────────┐
│    PyTorch / TensorFlow / JAX      │  ← You write code here
├────────────────────────────────────┤
│         NCCL 2.x Library           │  ← Collective operations
├───────────────────┬────────────────┤
│   CUDA / cuDNN    │  UCX / libfabric│  ← Compute & network primitives
├───────────────────┼────────────────┤
│  NVLink / NVSwitch │  IB / Ethernet │  ← Physical interconnect
└───────────────────┴────────────────┘
```

NCCL automatically detects the GPU topology and picks the **optimal communication path** (NVLink vs PCIe vs InfiniBand) without you having to specify it.

**2. The Five Core Collective Operations**

**AllReduce — The Most Important One**

AllReduce reduces a tensor across all GPUs and **distributes the result back to all**.

```
Before AllReduce:
  GPU0: [1, 2, 3, 4]
  GPU1: [5, 6, 7, 8]
  GPU2: [9, 0, 1, 2]
  GPU3: [3, 4, 5, 6]

After AllReduce (sum):
  GPU0: [18, 12, 16, 20]
  GPU1: [18, 12, 16, 20]
  GPU2: [18, 12, 16, 20]
  GPU3: [18, 12, 16, 20]
```

In distributed training: each GPU computes local gradients → AllReduce averages them → all GPUs take the same optimizer step → model stays synchronized.

```python
import torch.distributed as dist
import torch

# Each GPU has computed its local gradient
local_grad = compute_gradient(...)   # shape [hidden, hidden]

# NCCL AllReduce: average across all GPUs in-place
dist.all_reduce(local_grad, op=dist.ReduceOp.SUM)
local_grad /= dist.get_world_size()

# Now all GPUs have the same averaged gradient
optimizer.step()
```

**When it's called:** After every backward pass in DDP training.
**Data moved:** Full gradient tensor, twice (reduce then broadcast).

---

**Broadcast — Send One Tensor to All GPUs**

```
Before:
  GPU0 (src): [1.0, 2.0, 3.0]
  GPU1:        [?,   ?,   ?  ]
  GPU2:        [?,   ?,   ?  ]

After Broadcast from GPU0:
  GPU0: [1.0, 2.0, 3.0]
  GPU1: [1.0, 2.0, 3.0]
  GPU2: [1.0, 2.0, 3.0]
```

```python
# Broadcast initial model weights from rank 0 to all GPUs
# (ensures all GPUs start with identical weights)
tensor = model_weights if rank == 0 else torch.empty_like(model_weights)
dist.broadcast(tensor, src=0)
```

**When it's called:** Model initialization in DDP; broadcasting batch indices.

---

**Reduce — Combine Tensors to One GPU**

```
Before:
  GPU0: [1, 2]
  GPU1: [3, 4]
  GPU2: [5, 6]

After Reduce to GPU0 (sum):
  GPU0: [9, 12]   ← result
  GPU1: [3,  4]   ← unchanged
  GPU2: [5,  6]   ← unchanged
```

```python
dist.reduce(tensor, dst=0, op=dist.ReduceOp.SUM)
```

**When it's called:** Aggregating metrics (loss, accuracy) to a single rank for logging.

---

**AllGather — Collect All Tensors on All GPUs**

Each GPU contributes a **shard**; all GPUs receive the **full concatenated tensor**.

```
Before (each GPU holds 1 shard of a 4-shard tensor):
  GPU0: [A]   → shard 0
  GPU1: [B]   → shard 1
  GPU2: [C]   → shard 2
  GPU3: [D]   → shard 3

After AllGather:
  GPU0: [A, B, C, D]
  GPU1: [A, B, C, D]
  GPU2: [A, B, C, D]
  GPU3: [A, B, C, D]
```

```python
# Collect sharded embeddings (e.g., tensor parallel)
local_shard = model.embedding_shard   # each GPU has 1/N of the embedding table
full_embedding = [torch.zeros_like(local_shard) for _ in range(world_size)]
dist.all_gather(full_embedding, local_shard)
```

**When it's called:** FSDP parameter gathering before forward pass; tensor parallelism in Megatron.

---

**ReduceScatter — Reduce then Distribute (ZeRO's Core Op)**

The inverse of AllGather. Each GPU contributes a full tensor; each receives a **reduced shard**.

```
Before (each GPU has the full gradient):
  GPU0: [A0, A1, A2, A3]
  GPU1: [B0, B1, B2, B3]
  GPU2: [C0, C1, C2, C3]
  GPU3: [D0, D1, D2, D3]

After ReduceScatter (sum, 4 GPUs → 4 shards):
  GPU0: [A0+B0+C0+D0]   ← sum of column 0
  GPU1: [A1+B1+C1+D1]   ← sum of column 1
  GPU2: [A2+B2+C2+D2]   ← sum of column 2
  GPU3: [A3+B3+C3+D3]   ← sum of column 3
```

```python
# ZeRO-3: each GPU only stores its shard of the gradient
output_shard = torch.zeros(grad_size // world_size, device="cuda")
dist.reduce_scatter(output_shard, full_grad_list, op=dist.ReduceOp.SUM)
# Now GPU i holds only the i-th shard of the summed gradient
```

**When it's called:** ZeRO-1/2/3 gradient sharding; FSDP backward pass.

---

</details>

### AllToAll — 跨 GPU 转置

每个 GPU 向其他每个 GPU 发送**不同的数据**。用于混合专家模型（MoE）布线。

```
Before:
  GPU0 sends: [to_GPU0, to_GPU1, to_GPU2, to_GPU3]
  GPU1 sends: [to_GPU0, to_GPU1, to_GPU2, to_GPU3]
  ...

After AllToAll:
  GPU0 receives: [GPU0→GPU0, GPU1→GPU0, GPU2→GPU0, GPU3→GPU0]
  GPU1 receives: [GPU0→GPU1, GPU1→GPU1, GPU2→GPU1, GPU3→GPU1]
  ...
```

**调用时机：**MoE 模型（Mixtral、GPT-MoE）中的专家布线 — token 被发送到分配给它的 expert GPU。

---

## 3. NCCL 数据类型

NCCL 支持所有标准 ML 数据类型：

| NCCL 类型 | PyTorch 对应类型 | 大小 |
|---|---|---|
| `ncclFloat32` | `torch.float32` | 4 bytes |
| `ncclFloat16` | `torch.float16` | 2 bytes |
| `ncclBfloat16` | `torch.bfloat16` | 2 bytes |
| `ncclInt32` | `torch.int32` | 4 bytes |
| `ncclInt64` | `torch.int64` | 8 bytes |
| `ncclFloat64` | `torch.float64` | 8 bytes |

BF16 与 FP16 在 AI 训练中最常用 — 相比 FP32，它们把带宽需求减半。

## 4. NCCL 通信域

**通信域**是一组共同参与一次集合通信操作的 GPU。

```python
import torch.distributed as dist

# Default communicator: all GPUs in the job
dist.init_process_group(backend="nccl", world_size=8, rank=rank)

# Custom communicator: subset of GPUs (e.g., for tensor parallelism)
tp_ranks = [0, 1, 2, 3]   # first 4 GPUs form one tensor-parallel group
dp_ranks = [0, 4]          # GPUs 0 and 4 form a data-parallel pair

tp_group = dist.new_group(ranks=tp_ranks)
dp_group = dist.new_group(ranks=dp_ranks)

# Now you can communicate within each group separately
dist.all_reduce(tensor, group=tp_group)   # only among GPU 0,1,2,3
dist.all_reduce(tensor, group=dp_group)   # only between GPU 0 and 4
```

Megatron-LM 正是以此**并行运行 TP、PP 与 DP** — 每个都使用不同的 NCCL 通信域。

## 5. 同步与异步操作

```python
# Synchronous (blocking): waits for operation to complete
dist.all_reduce(tensor)  # returns only when all GPUs have the result

# Asynchronous (non-blocking): returns immediately, operation runs in background
handle = dist.all_reduce(tensor, async_op=True)

# Do other work while AllReduce runs:
other_computation()

# Wait for AllReduce to complete before using the result
handle.wait()
```

异步操作对**计算与通信重叠**至关重要 — 在调优良好的系统中可带来 30–60% 的训练加速。

## 参考文献

- [NCCL Developer Guide](https://docs.nvidia.com/deeplearning/nccl/user-guide/docs/)
- [PyTorch Distributed Overview](https://pytorch.org/docs/stable/distributed.html)
- [NCCL API Reference](https://docs.nvidia.com/deeplearning/nccl/api-guide/index.html)


<details>
<summary>English original</summary>

**AllToAll — Transpose Across GPUs**

Each GPU sends **different data** to each other GPU. Used in Mixture of Experts (MoE) routing.

```
Before:
  GPU0 sends: [to_GPU0, to_GPU1, to_GPU2, to_GPU3]
  GPU1 sends: [to_GPU0, to_GPU1, to_GPU2, to_GPU3]
  ...

After AllToAll:
  GPU0 receives: [GPU0→GPU0, GPU1→GPU0, GPU2→GPU0, GPU3→GPU0]
  GPU1 receives: [GPU0→GPU1, GPU1→GPU1, GPU2→GPU1, GPU3→GPU1]
  ...
```

**When it's called:** Expert routing in MoE models (Mixtral, GPT-MoE) — tokens are sent to their assigned expert GPU.

---

**3. NCCL Data Types**

NCCL supports all standard ML data types:

| NCCL Type | PyTorch Equivalent | Size |
|---|---|---|
| `ncclFloat32` | `torch.float32` | 4 bytes |
| `ncclFloat16` | `torch.float16` | 2 bytes |
| `ncclBfloat16` | `torch.bfloat16` | 2 bytes |
| `ncclInt32` | `torch.int32` | 4 bytes |
| `ncclInt64` | `torch.int64` | 8 bytes |
| `ncclFloat64` | `torch.float64` | 8 bytes |

BF16 and FP16 are the most common in AI training — they halve bandwidth requirements compared to FP32.

**4. NCCL Communicators**

A **communicator** is a group of GPUs that participate in a collective operation together.

```python
import torch.distributed as dist

# Default communicator: all GPUs in the job
dist.init_process_group(backend="nccl", world_size=8, rank=rank)

# Custom communicator: subset of GPUs (e.g., for tensor parallelism)
tp_ranks = [0, 1, 2, 3]   # first 4 GPUs form one tensor-parallel group
dp_ranks = [0, 4]          # GPUs 0 and 4 form a data-parallel pair

tp_group = dist.new_group(ranks=tp_ranks)
dp_group = dist.new_group(ranks=dp_ranks)

# Now you can communicate within each group separately
dist.all_reduce(tensor, group=tp_group)   # only among GPU 0,1,2,3
dist.all_reduce(tensor, group=dp_group)   # only between GPU 0 and 4
```

This is how Megatron-LM runs **TP, PP, and DP in parallel** — each uses a different NCCL communicator.

**5. Synchronous vs Asynchronous Operations**

```python
# Synchronous (blocking): waits for operation to complete
dist.all_reduce(tensor)  # returns only when all GPUs have the result

# Asynchronous (non-blocking): returns immediately, operation runs in background
handle = dist.all_reduce(tensor, async_op=True)

# Do other work while AllReduce runs:
other_computation()

# Wait for AllReduce to complete before using the result
handle.wait()
```

Async operations are critical for **overlapping compute and communication** — a 30–60% training speedup in well-tuned systems.

**References**

- [NCCL Developer Guide](https://docs.nvidia.com/deeplearning/nccl/user-guide/docs/)
- [PyTorch Distributed Overview](https://pytorch.org/docs/stable/distributed.html)
- [NCCL API Reference](https://docs.nvidia.com/deeplearning/nccl/api-guide/index.html)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/NCCL-Deep-Dive/01-Fundamentals.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/NCCL-Deep-Dive/01-Fundamentals.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
