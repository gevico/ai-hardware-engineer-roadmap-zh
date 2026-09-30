---
title: 模块 05 — MoE（混合专家模型）系统与基础设施
description: 模块 05 — MoE（混合专家模型）系统与基础设施
---

# 模块 05 — MoE（混合专家模型）系统与基础设施

**父级：** [长上下文 MoE 基础训练](README.md)

**一句话目的：** 通过理解专家并行、all-to-all 分发、真实批大小下的容量因子，以及通信悬崖在哪里，让 MoE 训练在多节点规模下保持快速。

**前置要求：** 模块 04（能写 top-k MoE 前向）。HPC（高性能计算）环境搭建 [NCCL 深入解析](../08-NCCL深度探索/README.md).

**产物：** 一个跨 NVLink 和 InfiniBand 域的 all-to-all 分发 micro-benchmark，外加真实批大小下的容量因子扫描，展示丢 token 率和每迭代时间。

---

## 为何重要

如果 all-to-all 很差，正确实现的 MoE FFN 可能比稠密 FFN 更慢。生产 MoE 训练有 20–60% 的时间花在分发和合并 all-to-all 上；在拓扑较差的多节点环境中，这一数字会升到 80% 以上。本模块就是“MoE 在 notebook 里能跑通”和“MoE 能扩展到 1024 GPU”之间的差别。

---

## 心智模型

### 专家并行（EP）

跨大量 GPU 承载 `E` 个专家的最自然方式：每个 EP rank 放 `E / EP` 个专家。路由到专家 `e` 的 token 必须传到拥有专家 `e` 的那个 rank。专家运行后，结果必须传回。

- **分发**：每个 rank 把 token 发送到其所选专家所在的位置。
- **计算**：每个 rank 对收到的 token 运行自己拥有的专家。
- **合并**：每个 rank 把结果发回原 token 的归属 rank。

分发和合并都是 **all-to-all** 集合通信：每个 rank 向其他每个 rank 发送一块数据。

### 在并行 mesh 中布局 MoE 的两种方式

现代 MoE 训练 mesh 有 5–6 个维度：DP × TP × PP × CP × EP（有时 × ZeRO）。EP 通常是：

- **节点内**：EP ≤ 8，使用 NVLink/NVSwitch。all-to-all 很快（每对有效约 600+ GB/s）。
- **跨节点**：EP > 8。all-to-all 跨 InfiniBand 或 RoCE；每链路有效带宽约 25–50 GB/s，慢一个数量级。

当模型需要的专家数超过单节点容量时，跨节点 EP 不可避免。设计目标是最小化跨节点跳数。

### 大规模下的容量因子

在模块 04 中，你看到了每个专家的 `capacity = ceil(capacity_factor · T · k / E)`。在符合训练实际的批大小下（每个 EP rank 每个 micro-batch 数万 token），容量溢出在 `capacity_factor = 1.25` 时很少见。但如果放任它漂移：

- 太高（例如 `2.0`）：会为永远不来的 token 的填充槽浪费内存和计算。
- 太低（例如 `1.0`）：负载不均时 token 会被丢弃，这会产生噪声梯度，并更用力驱动负载均衡器，进而可能振荡。

正确的取值是“在你的批大小下，配合健康的路由器，给出 <2% 丢 token 率的最小值”。凭经验调优。

### all-to-all 通信成本

对于每个 rank `T` 个 token、top-k 为 `k`、`E` 个专家、EP rank 数 `P`、hidden size 为 `H`：

- 每次 all-to-all 每个 rank 发送的字节数：`T · k · H · 2`（bf16）。
- 对端 rank 数：`P - 1`。
- 每次 all-to-all 的量：每个 rank `T · k · H · 2 · (P - 1) / P`。

对于 `T = 8192, k = 2, H = 4096, P = 8`：

`8192 · 2 · 4096 · 2 · 7/8 ≈ 117 MB per rank per all-to-all`.

每个 MoE layer 有两次 all-to-all（分发 + 合并）。每个 pass 有 32 个 MoE layer：每个 rank 每个 micro-batch 约 7.5 GB all-to-all 流量。在有效约 500 GB/s 的 NVLink 上，约 15 ms。在约 25 GB/s 的 InfiniBand 上，约 300 ms。

这个比例就是 EP 通常留在节点内的全部原因。

### 降低 all-to-all 成本

- **拓扑感知 all-to-all**：NCCL 根据 `NCCL_ALGO` 选择 ring 还是 hierarchical。对于 MoE 分发，hierarchical 通常更优。设置 `NCCL_ALGO=Tree,Ring` 让它自行选择，或用 `NCCL_ALL_TO_ALL_PIPELINE` 固定。
- **打包**：发送前把同一专家的所有 token 收集到连续 buffer 中。Megatron-LM 会这样做。否则，你要为每个目的地发送 `k` 个单独的块。
- **与计算重叠**：从 layer `L` 发起分发，然后在分发还在进行时开始 layer `L+1` 的 attention。Megatron 的 `--moe-token-dispatcher-type alltoall` + 仔细的 CUDA Graph 捕获能实现这种重叠。
- **用共享专家彻底去掉 all-to-all**：一个小的稠密 FFN，无论路由如何都对每个 token 运行，与 all-to-all 并行。DeepSeek-V3 使用。

### MoE + TP

对专家 FFN 做张量并行很直接（TP 像切分任何其他 linear 一样切分专家的 hidden dim）。微妙之处：当 TP 和 EP 共存时，每个 token 选中的专家位于一个 `(EP_rank, TP_group)` 对上。分发变成 2D 集合通信。Megatron 处理这一点；你要同时设置 `--tensor-model-parallel-size` 和 `--expert-model-parallel-size`。

一种常见的生产布局是 `TP = 8 (intra-node), EP = N_nodes`。每个专家本身在其 EP rank 所在节点的 8 个 GPU 上做 TP 并行化。


<details>
<summary>English original</summary>

**Module 05 — MoE Systems and Infrastructure**

**Parent:** [Long-Context MoE Foundation Training](README.md)

**One-line purpose:** Make MoE training fast at multi-node scale by understanding expert parallelism, all-to-all dispatch, capacity factor under real batch sizes, and where the communication cliff is.

**Prerequisites:** Module 04 (you can write a top-k MoE forward). HPC Setup [NCCL Deep Dive](../08-NCCL深度探索/README.md).

**Artifact:** An all-to-all dispatch micro-benchmark across NVLink and InfiniBand domains, plus a capacity-factor sweep at realistic batch size showing dropped-token rate and per-iter time.

---

**Why it matters**

A correctly-implemented MoE FFN can be slower than a dense one if the all-to-all is bad. Production MoE training spends 20–60% of its time in dispatch and combine all-to-alls; on a multi-node setup with poor topology, that figure climbs above 80%. This module is the difference between "MoE worked in a notebook" and "MoE scales to 1024 GPUs."

---

**Mental model**

**Expert parallelism (EP)**

The natural way to host `E` experts across many GPUs: put `E / EP` experts on each EP rank. A token routed to expert `e` must travel to whichever rank owns expert `e`. After the expert runs, the result must travel back.

- **Dispatch**: each rank sends tokens to wherever their chosen expert lives.
- **Compute**: each rank runs the experts it owns on the tokens it received.
- **Combine**: each rank sends results back to the original token's home rank.

Both dispatch and combine are **all-to-all** collectives: every rank sends a chunk to every other rank.

**Two ways to lay out MoE in a parallel mesh**

A modern MoE training mesh has 5–6 dimensions: DP × TP × PP × CP × EP (× ZeRO, sometimes). EP is usually:

- **Inside a node**: EP ≤ 8, using NVLink/NVSwitch. All-to-all is fast (~600+ GB/s effective per pair).
- **Across nodes**: EP > 8. All-to-all crosses InfiniBand or RoCE; effective bandwidth ~25–50 GB/s per link, an order of magnitude slower.

Cross-node EP is unavoidable when the model needs more experts than fit on one node. The design goal is to minimize the cross-node hop count.

**Capacity factor at scale**

In Module 04 you saw `capacity = ceil(capacity_factor · T · k / E)` per expert. At training-realistic batch sizes (tens of thousands of tokens per micro-batch per EP rank), capacity overflow is rare with `capacity_factor = 1.25`. But if you let it drift:

- Too high (e.g. `2.0`): you waste memory and compute on padded slots for tokens that never arrive.
- Too low (e.g. `1.0`): tokens get dropped when load is uneven, which produces noisy gradients and drives the load-balancer harder, which can oscillate.

The right value is "the smallest one that gives <2% dropped tokens at your batch size with a healthy router." Tune empirically.

**All-to-all communication cost**

For `T` tokens per rank, `k` top-k, `E` experts, EP ranks `P`, hidden size `H`:

- Bytes sent per rank per all-to-all: `T · k · H · 2` (bf16).
- Number of partner ranks: `P - 1`.
- Volume per all-to-all: `T · k · H · 2 · (P - 1) / P` per rank.

For `T = 8192, k = 2, H = 4096, P = 8`:

`8192 · 2 · 4096 · 2 · 7/8 ≈ 117 MB per rank per all-to-all`.

Two all-to-alls per MoE layer (dispatch + combine). With 32 MoE layers per pass: ~7.5 GB of all-to-all traffic per micro-batch per rank. On NVLink at ~500 GB/s effective, that's ~15 ms. On InfiniBand at ~25 GB/s, that's ~300 ms.

That ratio is the entire reason EP usually stays within a node.

**Reducing all-to-all cost**

- **Topology-aware all-to-all**: NCCL chooses ring vs hierarchical based on `NCCL_ALGO`. For MoE dispatch, hierarchical usually wins. Set `NCCL_ALGO=Tree,Ring` and let it pick, or pin with `NCCL_ALL_TO_ALL_PIPELINE`.
- **Packing**: gather all tokens for the same expert into a contiguous buffer before sending. Megatron-LM does this. Without it, you send `k` separate chunks per destination.
- **Overlap with compute**: launch dispatch from layer `L`, then begin attention on layer `L+1` while dispatch is in flight. Megatron's `--moe-token-dispatcher-type alltoall` + careful CUDA graph capture gets this overlap.
- **Drop the all-to-all entirely** with shared experts: a small dense FFN that runs on every token regardless of routing, in parallel with the all-to-all. Used by DeepSeek-V3.

**MoE + TP**

Tensor-parallelizing the expert FFNs is straightforward (TP slices the expert's hidden dim like any other linear). The subtlety: when TP and EP coexist, every token's chosen expert lives on a `(EP_rank, TP_group)` pair. Dispatch becomes a 2D collective. Megatron handles this; you set both `--tensor-model-parallel-size` and `--expert-model-parallel-size`.

A common production layout is `TP = 8 (intra-node), EP = N_nodes`. Each expert is itself TP-parallelized across the 8 GPUs of its EP rank's node.

</details>

### runtime 下的专家负载不均衡

即便有 aux loss 和 z-loss，真实工作负载仍会产生逐分钟级的不均衡——当一段长代码块进入批时，某个专家会突然飙高。capacity-factor padding 会吸收这些尖峰；若尖峰超出 capacity，token 就会被丢弃。

要像盯股价一样盯住的两个诊断指标：

- **每专家利用率**（滚动）：应徘徊在 `1/E` ± 30% 附近。
- **token 丢弃率**：应保持在 0% 附近。突然飙升通常是数据流水线的问题（数据集的某个分片内容异常），而非 router 的锅。

---

## 动手构建

### 1. all-to-all microbenchmark

单节点 8 张 GPU（节点内，NVLink）：

```python
# alltoall_microbench.py
import torch, torch.distributed as dist, time
dist.init_process_group("nccl")
rank, world = dist.get_rank(), dist.get_world_size()
torch.cuda.set_device(rank)

for size_mb in [1, 4, 16, 64, 128]:
    n = size_mb * 1024 * 1024 // 2          # bf16 elements per rank-pair
    send = torch.randn(world, n, device="cuda", dtype=torch.bfloat16)
    recv = torch.empty_like(send)

    # warmup
    for _ in range(3):
        dist.all_to_all_single(recv, send)
    torch.cuda.synchronize()

    t0 = time.perf_counter()
    for _ in range(20):
        dist.all_to_all_single(recv, send)
    torch.cuda.synchronize()
    t = (time.perf_counter() - t0) / 20 * 1000

    total_bytes = world * n * 2
    bw = total_bytes * (world - 1) / world / (t / 1000) / 1e9
    if rank == 0:
        print(f"size/rank-pair={size_mb:>4} MB  time={t:7.2f} ms  bus_bw={bw:6.1f} GB/s")
```

在单节点上运行并记录耗时。然后在 2 节点环境（16 张 GPU，跨 InfiniBand）上运行同一个脚本。节点内总线带宽应为跨节点带宽的 4–8×。

### 2. capacity factor 扫描

把 Module 04 里的 `minimal_moe.py` 拿来，用 `torchrun --nproc-per-node 8` 接进多 GPU 训练步。在真实的每步 token 数（例如 16K tokens × 8 ranks = 128K tokens 每步）下改变 `capacity_factor ∈ {1.0, 1.1, 1.25, 1.5, 2.0}`。报告：

- token 丢弃率。
- 每 iter 耗时。
- `N` 个 warmup step 后的最终任务损失。

你应该看到 `cf=1.0` 产生 5–15% 的丢弃且损失抖动；`cf=1.25` 产生 0–2% 的丢弃且损失稳定；`cf=2.0` 产生 0% 的丢弃但浪费内存。

---

## 在真实技术栈中使用

在 Megatron-LM 中：

```
--num-experts 64
--expert-model-parallel-size 8
--moe-router-topk 2
--moe-router-load-balancing-type aux_loss
--moe-aux-loss-coeff 0.01
--moe-z-loss-coeff 1e-3
--moe-expert-capacity-factor 1.25
--moe-token-dispatcher-type alltoall
--moe-pad-expert-input-to-capacity
--use-flash-attn
```

一个 64 专家、top-2 的模型跑在 64 张 GPU（8 节点 8×H200）上时，通常会用 `EP=8`（节点内）和 `DP=8`（跨节点）。此时跨节点通信用于梯度全规约，而非 MoE all-to-all——便宜得多。

DeepSpeed-MoE 有对应的开关（`moe_param_group`、`ep_size`、`min_capacity`、`top_k`）。两个库的 MoE 文档各读一遍；名字不同，但概念一一对应。

---

## 测量

针对你的扫描：

- **all-to-all 带宽**：多种消息大小，节点内与跨节点。画图。
- **每 iter 耗时拆解**：forward、backward、all-to-all、all-reduce。Megatron 的 `--log-throughput --log-communication-volume` 会输出这些。
- **每 iteration 的 token 流量**，单位 GB。与你在心智模型那节做的粗略估算对一下。

一次健康的 MoE 训练步应满足：

- 节点内 EP=8 时，all-to-all 占步时间不到 25%。
- EP=16（一跳跨节点）时不到 40%。
- 超过 50% 意味着你要么降低 EP，要么加 overlap，要么重新审视拓扑。

---

## 交付

放进 `lcm-course/`：

1. `alltoall_microbench.py` 与 `alltoall.csv`，包含节点内和跨节点的结果。
2. `capacity_factor_sweep.csv`，包含 token 丢弃率、每 iter 耗时和 warmup 后的损失。
3. `moe_systems_notes.md`——各写一段：专家并行、all-to-all 开销、生产环境中的 capacity factor，以及至少一个你自己制造出的、能叫上名字的失败（例如 "at cf=1.0 with 16K tokens/rank, dropped rate spiked to 12% during the first 200 steps, then settled to 2%"）。

---

## 相关页面

- [Module 04 — MoE 基础](04-MoE-Fundamentals.md)
- [Module 08 — 长上下文与 MoE 的结合](08-Combining-LongContext-and-MoE.md)
- [Module 09 — 分布式训练基础设施](09-Distributed-Training-Infrastructure.md)
- [NCCL 深入剖析](../08-NCCL深度探索/README.md)
- Megatron-LM MoE: <https://github.com/NVIDIA/Megatron-LM/blob/main/megatron/core/transformer/moe/README.md>
- DeepSpeed-MoE: <https://www.deepspeed.ai/tutorials/mixture-of-experts/>


<details>
<summary>English original</summary>

**Expert load imbalance at runtime**

Even with aux loss and z-loss, real workloads produce minute-to-minute imbalance — one expert spikes when a long code block enters a batch. The capacity-factor padding absorbs spikes; if spikes exceed capacity, tokens drop.

Two diagnostics you watch like a stock ticker:

- **Per-expert utilisation** (rolling): should hover near `1/E` ± 30%.
- **Dropped token rate**: should stay near 0%. Sudden spikes are usually data-pipeline issues (a shard of the dataset has unusual content) rather than the router's fault.

---

**Build it**

**1. All-to-all microbenchmark**

Across 8 GPUs in one node (intra-node, NVLink):

```python
# alltoall_microbench.py
import torch, torch.distributed as dist, time
dist.init_process_group("nccl")
rank, world = dist.get_rank(), dist.get_world_size()
torch.cuda.set_device(rank)

for size_mb in [1, 4, 16, 64, 128]:
    n = size_mb * 1024 * 1024 // 2          # bf16 elements per rank-pair
    send = torch.randn(world, n, device="cuda", dtype=torch.bfloat16)
    recv = torch.empty_like(send)

    # warmup
    for _ in range(3):
        dist.all_to_all_single(recv, send)
    torch.cuda.synchronize()

    t0 = time.perf_counter()
    for _ in range(20):
        dist.all_to_all_single(recv, send)
    torch.cuda.synchronize()
    t = (time.perf_counter() - t0) / 20 * 1000

    total_bytes = world * n * 2
    bw = total_bytes * (world - 1) / world / (t / 1000) / 1e9
    if rank == 0:
        print(f"size/rank-pair={size_mb:>4} MB  time={t:7.2f} ms  bus_bw={bw:6.1f} GB/s")
```

Run on a single node and capture the times. Then run the same script on a 2-node setup (16 GPUs across InfiniBand). The intra-node bus bandwidth should be 4–8× the cross-node bandwidth.

**2. Capacity-factor sweep**

Take the `minimal_moe.py` from Module 04 and wire it into a multi-GPU training step with `torchrun --nproc-per-node 8`. Vary `capacity_factor ∈ {1.0, 1.1, 1.25, 1.5, 2.0}` at a realistic per-step token count (e.g. 16K tokens × 8 ranks = 128K tokens per step). Report:

- Dropped-token rate.
- Per-iter time.
- Final task loss after `N` warmup steps.

You should see `cf=1.0` produce 5–15% drops and noisy loss; `cf=1.25` produce 0–2% drops and stable loss; `cf=2.0` produce 0% drops but waste memory.

---

**Use it in the real stack**

In Megatron-LM:

```
--num-experts 64
--expert-model-parallel-size 8
--moe-router-topk 2
--moe-router-load-balancing-type aux_loss
--moe-aux-loss-coeff 0.01
--moe-z-loss-coeff 1e-3
--moe-expert-capacity-factor 1.25
--moe-token-dispatcher-type alltoall
--moe-pad-expert-input-to-capacity
--use-flash-attn
```

A 64-expert, top-2 model on 64 GPUs (8-node 8×H200) would typically run with `EP=8` (intra-node) and `DP=8` (across nodes). The cross-node communication is then for gradient all-reduce, not for MoE all-to-all — much cheaper.

DeepSpeed-MoE has analogous knobs (`moe_param_group`, `ep_size`, `min_capacity`, `top_k`). Read both libraries' MoE docs once; the names differ but the concepts map 1:1.

---

**Measure it**

For your sweep:

- **All-to-all bandwidth** at multiple message sizes, intra-node and cross-node. Plot.
- **Per-iter time breakdown**: forward, backward, all-to-all, all-reduce. Megatron's `--log-throughput --log-communication-volume` produces these.
- **Token traffic per iteration** in GB. Match against your back-of-envelope from the mental-model section.

A healthy MoE training step has:

- All-to-all under 25% of step time at EP=8 intra-node.
- Under 40% at EP=16 (one cross-node hop).
- Above 50% means you should either reduce EP, add overlap, or revisit topology.

---

**Ship it**

Drop into `lcm-course/`:

1. `alltoall_microbench.py` and `alltoall.csv` with intra-node and cross-node results.
2. `capacity_factor_sweep.csv` with dropped-token rate, per-iter time, and loss after warmup.
3. `moe_systems_notes.md` — one paragraph each on expert parallelism, all-to-all costs, capacity factor in production, and at least one named failure you induced (e.g. "at cf=1.0 with 16K tokens/rank, dropped rate spiked to 12% during the first 200 steps, then settled to 2%").

---

**Related pages**

- [Module 04 — MoE fundamentals](04-MoE-Fundamentals.md)
- [Module 08 — Combining long-context and MoE](08-Combining-LongContext-and-MoE.md)
- [Module 09 — Distributed training infrastructure](09-Distributed-Training-Infrastructure.md)
- [NCCL Deep Dive](../08-NCCL深度探索/README.md)
- Megatron-LM MoE: <https://github.com/NVIDIA/Megatron-LM/blob/main/megatron/core/transformer/moe/README.md>
- DeepSpeed-MoE: <https://www.deepspeed.ai/tutorials/mixture-of-experts/>

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Long-Context-MoE-Foundation-Training/05-MoE-Systems-Infrastructure.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Long-Context-MoE-Foundation-Training/05-MoE-Systems-Infrastructure.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
