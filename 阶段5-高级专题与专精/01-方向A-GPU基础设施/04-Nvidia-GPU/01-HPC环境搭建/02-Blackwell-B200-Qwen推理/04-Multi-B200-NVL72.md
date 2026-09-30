---
title: 第 4 章：Multi-B200、GB200 Superchip 与 NVL72
description: 第 4 章：Multi-B200、GB200 Superchip 与 NVL72
---

# 第 4 章：Multi-B200、GB200 Superchip 与 NVL72

## 概览

单 B200 部署能干净地覆盖 FP4 下的 Qwen2.5-72B。**它覆盖不了：**大于 192 GB 的稠密模型、专家池庞大的 MoE（混合专家模型）、极端批大小的长上下文推理服务，或规模化下 72B 级对话的亚 30 ms p99 延迟。这些场景都要上多 B200 —— HGX B200（8-GPU 板卡）、GB200 superchip（2 B200 + 1 Grace，缓存一致），或 NVL72（单机架内 72 B200 + 36 Grace）。

本章讲的是 Blackwell 上 Qwen 级工作负载的多 GPU 扩展。涵盖 fabric、TP/PP 选择、NCCL 热路径，以及 GB200/NVL72 解锁的新内存层级。

读完后你应能：

* 针对给定的 Qwen 工作负载选对 Blackwell 平台（HGX vs GB200 vs NVL72）。
* 计算 TP=8 / TP=16 / TP=72 在 decode（逐 token 生成阶段）热路径中的 NCCL 带宽。
* 用 Grace LPDDR 缓存一致内存做 KV spill、前缀缓存或 MoE 专家卸载。
* 预测机架规模下 Qwen2.5-72B 与假想 Qwen3-300B+ 的推理服务吞吐。

---

## 1. Blackwell 平台阶梯

```
┌────────────────────────────────────────────────────────────────┐
│  Tier 1: Single B200 (~$30k chip + carrier)                    │
│    192 GB HBM  ·  8 TB/s  ·  1.8 TB/s NVLink-5                 │
│    Fits Qwen2.5-72B-MX-FP4 entirely                            │
└────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌────────────────────────────────────────────────────────────────┐
│  Tier 2: HGX B200 (8 × B200, SXM)                              │
│    1.54 TB HBM total                                           │
│    64 TB/s aggregate HBM bandwidth                             │
│    NVSwitch fabric, 130 TB/s bisection                         │
│    Drop-in HGX H100/H200 replacement                           │
└────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌────────────────────────────────────────────────────────────────┐
│  Tier 3: GB200 Superchip (2 × B200 + 1 Grace, NVLink-C2C)      │
│    384 GB HBM + 480 GB LPDDR5X coherent                        │
│    Grace ↔ B200: 900 GB/s coherent                             │
│    Building block of NVL72 racks                               │
└────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌────────────────────────────────────────────────────────────────┐
│  Tier 4: NVL72 Rack (36 × GB200, liquid-cooled)                │
│    72 × B200 + 36 × Grace                                      │
│    13.8 TB HBM + 17.3 TB LPDDR = ~30 TB coherent               │
│    576 TB/s aggregate HBM bandwidth                            │
│    NVLink-5 fabric: 130 TB/s bisection                         │
│    ~120 kW per rack                                            │
└────────────────────────────────────────────────────────────────┘
```

**Qwen 工作负载的决策规则：**

| 工作负载 | 合适层级 |
|---|---|
| 单用户低延迟 Qwen2.5-72B | Tier 1（单 B200，封装内 TP=2） |
| 多租户对话，数百并发用户 | Tier 2（HGX B200 8-GPU） |
| 长上下文 KV spill / 前缀缓存 / MoE 专家池 | Tier 3（GB200，使用 Grace 缓存一致内存） |
| 前沿模型推理服务（Qwen3-300B+ / Qwen-MoE） | Tier 4（NVL72） |
| 万亿参数稠密模型 | Tier 4（NVL72），TP=72 |

多数生产环境的 Qwen2.5-72B 推理服务落在 **Tier 1 或 Tier 2**。对当前 Qwen 产品线而言 NVL72 是大材小用，但对即将到来的模型是必然选择。

---


<details>
<summary>English original</summary>

**Chapter 4: Multi-B200, GB200 Superchip, and NVL72**

**Overview**

Single-B200 deployment covers Qwen2.5-72B at FP4 cleanly. **It doesn't cover:** dense models larger than 192 GB, MoE models with huge expert pools, extreme-batch long-context serving, or sub-30 ms p99 latency on 72B-class chat at scale. For all of those you go multi-B200 — HGX B200 (8-GPU board), GB200 superchip (2 B200 + 1 Grace, coherent), or NVL72 (72 B200 + 36 Grace in one rack).

This chapter is the multi-GPU scaling story for Qwen-class workloads on Blackwell. It covers the fabric, the TP/PP choices, the NCCL hot path, and the new memory tiers that GB200/NVL72 unlock.

By the end you should be able to:

* Pick the right Blackwell platform (HGX vs GB200 vs NVL72) for a given Qwen workload.
* Compute NCCL bandwidth in the decode hot path for TP=8 / TP=16 / TP=72.
* Use Grace LPDDR coherent memory for KV spill, prefix caching, or MoE expert offload.
* Predict serving throughput for Qwen2.5-72B and hypothetical Qwen3-300B+ at rack scale.

---

**1. The Blackwell Platform Ladder**

```
┌────────────────────────────────────────────────────────────────┐
│  Tier 1: Single B200 (~$30k chip + carrier)                    │
│    192 GB HBM  ·  8 TB/s  ·  1.8 TB/s NVLink-5                 │
│    Fits Qwen2.5-72B-MX-FP4 entirely                            │
└────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌────────────────────────────────────────────────────────────────┐
│  Tier 2: HGX B200 (8 × B200, SXM)                              │
│    1.54 TB HBM total                                           │
│    64 TB/s aggregate HBM bandwidth                             │
│    NVSwitch fabric, 130 TB/s bisection                         │
│    Drop-in HGX H100/H200 replacement                           │
└────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌────────────────────────────────────────────────────────────────┐
│  Tier 3: GB200 Superchip (2 × B200 + 1 Grace, NVLink-C2C)      │
│    384 GB HBM + 480 GB LPDDR5X coherent                        │
│    Grace ↔ B200: 900 GB/s coherent                             │
│    Building block of NVL72 racks                               │
└────────────────────────────────────────────────────────────────┘
                                │
                                ▼
┌────────────────────────────────────────────────────────────────┐
│  Tier 4: NVL72 Rack (36 × GB200, liquid-cooled)                │
│    72 × B200 + 36 × Grace                                      │
│    13.8 TB HBM + 17.3 TB LPDDR = ~30 TB coherent               │
│    576 TB/s aggregate HBM bandwidth                            │
│    NVLink-5 fabric: 130 TB/s bisection                         │
│    ~120 kW per rack                                            │
└────────────────────────────────────────────────────────────────┘
```

**Decision rule for Qwen workloads:**

| Workload | Right tier |
|---|---|
| Single-user low-latency Qwen2.5-72B | Tier 1 (single B200, intra-package TP=2) |
| Multi-tenant chat, 100s of concurrent users | Tier 2 (HGX B200 8-GPU) |
| Long-context KV spill / prefix cache / MoE expert pool | Tier 3 (GB200, uses Grace coherent memory) |
| Frontier model serving (Qwen3-300B+ / Qwen-MoE) | Tier 4 (NVL72) |
| Trillion-parameter dense models | Tier 4 (NVL72), TP=72 |

Most production Qwen2.5-72B serving lives at **Tier 1 or Tier 2**. NVL72 is overkill for current Qwen lineups but inevitable for what's coming.

---

</details>

## 2. HGX B200 —— 8 GPU 标准

HGX B200 板卡是 HGX H100 SXM 和 HGX H200 的直接后继。单板 8 颗 B200 GPU，全部通过 NVSwitch-5 互联。

```
       ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐
       │ B200 #0 │    │ B200 #1 │    │ B200 #2 │    │ B200 #3 │
       └────┬────┘    └────┬────┘    └────┬────┘    └────┬────┘
            │              │              │              │
            └──────────────┴───┬──────────┴──────────────┘
                               │
                       ┌───────┴───────┐
                       │   NVSwitch    │   (130 TB/s bisection)
                       │     fabric    │
                       └───────┬───────┘
                               │
            ┌──────────────────┼──────────────────┐
            │                  │                  │
       ┌────┴────┐    ┌────────┴────┐    ┌────────┴────┐    ┌─────────┐
       │ B200 #4 │    │  B200 #5    │    │  B200 #6    │    │ B200 #7 │
       └─────────┘    └─────────────┘    └─────────────┘    └─────────┘

  Aggregate: 1.54 TB HBM3e, 64 TB/s HBM bandwidth, 130 TB/s NVLink fabric
```

对 **TP=8** 的 Qwen2.5-72B，模型切分到全部 8 张卡上：

* 每张 GPU 持有 1/8 的权重（MX-FP4-mixed 下约 5.6 GB）。
* 每条驻留序列的 KV，每张 GPU 持有 1/8。
* attention 与 FFN 之后的 AllReduce 走 NVLink-5 / NVSwitch。

相比 HGX H100 的关键变化：**集合通信开销大幅下降**。NVLink-5 的单 GPU 带宽是 NVLink-4 的 2 倍，NVSwitch-5 在 130 TB/s 下提供 full bisection。对 Qwen2.5-72B 的单 token：

```
Per layer collectives (TP=8):
  AllReduce post-attention:  d_model=8192 × 2 bytes = 16 KB
  AllReduce post-FFN:        d_model=8192 × 2 bytes = 16 KB
                                                    = 32 KB / layer
× 80 layers                                         = 2.56 MB / token

NVLink-5 bandwidth per GPU: 1.8 TB/s
Time per 16-KB AllReduce on NVLink-5: ~4 µs (vs ~10 µs on NVLink-4)
Per-token NVLink-5 time: 2 × 80 × 4 µs = 0.64 ms
```

这 0.64 ms 是集合通信新的延迟下限。对比 H100 SXM TP=8 约 1.6 ms 的下限。集合通信开销在（更快的）计算步中占比更小，因此摊销后的扩展效率更干净。

### 2.1 HGX B200 在 Qwen2.5-72B 上的预期数据

| 指标 | TP=4 | TP=8 |
|---|---|---|
| 单流 decode | ~450 tok/s | ~620 tok/s |
| Batch=32 聚合 decode | ~9,500 tok/s | ~14,000 tok/s |
| Batch=128 聚合 decode | ~22,000 tok/s | ~32,000 tok/s |
| TTFT @ 2k prompt, B=1 | ~15 ms | ~12 ms |
| TTFT @ 32k prompt, B=8 | ~280 ms | ~180 ms |

单台 HGX B200 8-GPU 整机在 Qwen-72B 级推理服务中扛下的**生产流量相当于 4–6 台 HGX H100 8-GPU 整机**。无论从运维还是经济性看，这都是拐点。

---

## 3. 超出 TP=8 的张量并行节奏

超过 TP=8 后，对 Qwen2.5-72B 而言，就开始失去与 `n_kv_heads = 8` 的自然对齐。可选方案：

* **TP=16** —— 把 KV head 两两配对（每对 GPU 共享一个 KV head 的分片）。每个分片的 KV 显存开销翻倍（因为被复制），只有在需要单流延迟时才值得。
* **TP=72** —— 针对前沿模型，需要类 PP 的切分或专家并行（MoE，混合专家模型）。纯 dense 的 Qwen2.5-72B 在 TP=72 下浪费资源；该配置留给 Qwen3-300B+ 级别。

通用规则：**TP 应整除 `n_kv_heads`。** Qwen2.5-72B 在 `n_kv_heads = 8` 下，TP ∈ {1, 2, 4, 8} 都很合适。假设 Qwen3-MoE 有 16 个 KV head，则可干净地扩展到 TP=16。

---

## 4. GB200 超级芯片与一致性的 Grace 内存

在 HGX 之上是 **GB200 超级芯片**：2 × B200 + 1 × Grace ARM CPU，Grace 与 B200 之间通过 NVLink-C2C 连接。

```
┌──────────────────────────────────────────────────────────┐
│                    GB200 Superchip                       │
│  ┌────────────────────┐   NVLink-C2C   ┌──────────────┐ │
│  │   B200 (2 dies)    │◄══════════════►│   Grace      │ │
│  │   192 GB HBM3e     │   900 GB/s     │   72-core    │ │
│  │   8 TB/s           │   coherent     │   480 GB     │ │
│  │                    │                │   LPDDR5X    │ │
│  └────────────────────┘                └──────────────┘ │
│  ┌────────────────────┐                ┌──────────────┐ │
│  │   B200 (2 dies)    │◄══════════════►│   Grace      │ │
│  │   192 GB HBM3e     │                │   72-core    │ │
│  │   8 TB/s           │                │   480 GB     │ │
│  │                    │                │   LPDDR5X    │ │
│  └────────────────────┘                └──────────────┘ │
└──────────────────────────────────────────────────────────┘
```

关键特性：**Grace LPDDR 能以内存一致性的速度被 GPU 寻址**。这为推理带来两级存储层次：

* **热层：** HBM3e —— 8 TB/s，每颗 B200 192 GB。
* **温层：** Grace LPDDR —— 900 GB/s，每颗 Grace 480 GB。


<details>
<summary>English original</summary>

**2. HGX B200 — The 8-GPU Standard**

The HGX B200 board is the direct successor to HGX H100 SXM and HGX H200. Eight B200 GPUs on a board, all connected through NVSwitch-5.

```
       ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐
       │ B200 #0 │    │ B200 #1 │    │ B200 #2 │    │ B200 #3 │
       └────┬────┘    └────┬────┘    └────┬────┘    └────┬────┘
            │              │              │              │
            └──────────────┴───┬──────────┴──────────────┘
                               │
                       ┌───────┴───────┐
                       │   NVSwitch    │   (130 TB/s bisection)
                       │     fabric    │
                       └───────┬───────┘
                               │
            ┌──────────────────┼──────────────────┐
            │                  │                  │
       ┌────┴────┐    ┌────────┴────┐    ┌────────┴────┐    ┌─────────┐
       │ B200 #4 │    │  B200 #5    │    │  B200 #6    │    │ B200 #7 │
       └─────────┘    └─────────────┘    └─────────────┘    └─────────┘

  Aggregate: 1.54 TB HBM3e, 64 TB/s HBM bandwidth, 130 TB/s NVLink fabric
```

For Qwen2.5-72B at **TP=8** the model partitions across all 8 cards:

* Each GPU holds 1/8 of weights (~5.6 GB at MX-FP4-mixed).
* Each GPU holds 1/8 of KV per stored sequence.
* AllReduce after attention and FFN happens on NVLink-5 / NVSwitch.

The key change vs HGX H100: **collective overhead drops dramatically**. NVLink-5 is 2× the per-GPU bandwidth of NVLink-4, and NVSwitch-5 has full bisection at 130 TB/s. For Qwen2.5-72B per-token:

```
Per layer collectives (TP=8):
  AllReduce post-attention:  d_model=8192 × 2 bytes = 16 KB
  AllReduce post-FFN:        d_model=8192 × 2 bytes = 16 KB
                                                    = 32 KB / layer
× 80 layers                                         = 2.56 MB / token

NVLink-5 bandwidth per GPU: 1.8 TB/s
Time per 16-KB AllReduce on NVLink-5: ~4 µs (vs ~10 µs on NVLink-4)
Per-token NVLink-5 time: 2 × 80 × 4 µs = 0.64 ms
```

That 0.64 ms is the new latency floor on collectives. Compare to H100 SXM TP=8's ~1.6 ms floor. The collective overhead is now a smaller fraction of the (faster) compute step, so amortized scaling is even cleaner.

**2.1 Expected HGX B200 numbers on Qwen2.5-72B**

| Metric | TP=4 | TP=8 |
|---|---|---|
| Single-stream decode | ~450 tok/s | ~620 tok/s |
| Batch=32 aggregate decode | ~9,500 tok/s | ~14,000 tok/s |
| Batch=128 aggregate decode | ~22,000 tok/s | ~32,000 tok/s |
| TTFT @ 2k prompt, B=1 | ~15 ms | ~12 ms |
| TTFT @ 32k prompt, B=8 | ~280 ms | ~180 ms |

A single HGX B200 8-GPU box handles **production traffic equivalent to 4–6 HGX H100 8-GPU boxes** for Qwen-72B-class serving. Operationally and economically, this is the inflection point.

---

**3. The Tensor Parallel Cadence Beyond TP=8**

Above TP=8 you start losing the natural alignment with `n_kv_heads = 8` for Qwen2.5-72B. Options:

* **TP=16** — pair off KV heads (each pair of GPUs shares one KV head's slice). Doubles KV memory cost per slice (it's replicated), worth it only when you need the per-stream latency.
* **TP=72** — for frontier models, requires PP-like fragmentation or expert-parallelism (MoE). Pure dense Qwen2.5-72B at TP=72 wastes resources; reserved for Qwen3-300B+ class.

The general rule: **TP should divide `n_kv_heads` evenly.** Qwen2.5-72B with `n_kv_heads = 8` is happy at TP ∈ {1, 2, 4, 8}. Hypothetical Qwen3-MoE with 16 KV heads would extend to TP=16 cleanly.

---

**4. GB200 Superchip and Coherent Grace Memory**

Above HGX sits the **GB200 superchip**: 2 × B200 + 1 × Grace ARM CPU, with NVLink-C2C between the Grace and the B200s.

```
┌──────────────────────────────────────────────────────────┐
│                    GB200 Superchip                       │
│  ┌────────────────────┐   NVLink-C2C   ┌──────────────┐ │
│  │   B200 (2 dies)    │◄══════════════►│   Grace      │ │
│  │   192 GB HBM3e     │   900 GB/s     │   72-core    │ │
│  │   8 TB/s           │   coherent     │   480 GB     │ │
│  │                    │                │   LPDDR5X    │ │
│  └────────────────────┘                └──────────────┘ │
│  ┌────────────────────┐                ┌──────────────┐ │
│  │   B200 (2 dies)    │◄══════════════►│   Grace      │ │
│  │   192 GB HBM3e     │                │   72-core    │ │
│  │   8 TB/s           │                │   480 GB     │ │
│  │                    │                │   LPDDR5X    │ │
│  └────────────────────┘                └──────────────┘ │
└──────────────────────────────────────────────────────────┘
```

The crucial property: **Grace LPDDR is GPU-addressable at memory-coherent speeds**. This creates a two-tier memory hierarchy for inference:

* **Hot tier:** HBM3e — 8 TB/s, 192 GB per B200.
* **Warm tier:** Grace LPDDR — 900 GB/s, 480 GB per Grace.

</details>

### 4.1 温层里放什么

温层 Grace 内存的三个生产相关用途：

**（a）长上下文 KV 溢出。** 当某个序列的 KV 超出 HBM 预算时，将冷块（最旧的 token、attention 权重最低的块）分页到 Grace LPDDR。热块（最近的 token、高 attention 权重的块）留在 HBM 中。生产框架用 LRU 或基于 attention 权重的驱逐等策略处理这种情况。

```
Single sequence at 256k context:
  KV per layer per token (FP8) = 2 KB
  × 80 layers × 256k = 41 GB
  → All in HBM? Yes, but eats 20% of budget for one sequence.
  → Spill cold blocks to Grace: keep latest 32k in HBM, page older.
```

**（b）用于共享系统提示词的前缀缓存。** 多租户部署常常在大量请求之间共享一个长系统提示词（RAG（检索增强生成）上下文、agent 系统消息、persona）。计算一次 KV，存入 Grace，为每个新请求粘贴到 HBM。将提示词共享流量的首 token 时延降低 10–100×。

**（c）MoE（混合专家模型）专家池卸载。** 对于 Qwen-MoE 变体（或假设的 Qwen3-MoE），每个 token 仅约 10% 的专家处于活跃状态。将所有专家权重保留在 Grace 中；每一步将活跃专家流式传输到 HBM。用一些带宽换取每块 GPU 托管大得多的专家池的能力。

### 4.2 温层 KV 的数值

一个算例：Qwen2.5-72B 以 128k 平均上下文服务 16 个并发用户。

```
KV per user @ MX-FP8: 80 layers × 2 × 8 KV-heads × 128 head-dim × 128k × 1 byte
                    = 21 GB per user
16 users × 21 GB = 336 GB

Single-B200 HBM budget for KV: ~120 GB
GB200 superchip HBM (2 B200): ~240 GB
GB200 Grace LPDDR: 960 GB

Strategy:
  - Hot 16k tokens per user in HBM: 16 × 2.6 GB = 42 GB (fits HBM)
  - Cold 112k tokens per user in Grace LPDDR: 16 × 18.4 GB = 295 GB (fits Grace)
  - Total: 337 GB, comfortable.

Bandwidth cost per decode:
  Hot KV read (HBM): ~2.6 GB × 16 = 42 GB at 8 TB/s = 5 ms aggregate
  Cold KV read (Grace): ~18.4 GB × 16 = 295 GB at 900 GB/s = 330 ms aggregate
  → Cold-tier reads dominate if you re-read every step.
```

这就是为什么生产框架**不会**每一步都重新读取冷 KV。而是稀疏地重新计算冷层的 KV 分数（例如每 16 个 token），或者对冷层使用**滑动窗口 attention**，使其永远不会以全带宽被重新读取。这两种模式正是 256k+ 上下文在 GB200 上变得可负担的方式。

---

## 5. NVL72 — 机架级计算机

```
NVL72 Rack:
  - 36 GB200 superchips (72 B200, 36 Grace)
  - 18 compute trays, each holding 2 GB200
  - 9 NVSwitch trays
  - Liquid cooling loop
  - ~120 kW power, ~3000 lb
  - 13.8 TB HBM3e + 17.3 TB Grace LPDDR = ~30 TB coherent
  - 576 TB/s aggregate HBM bandwidth
  - 130 TB/s NVLink-5 bisection (full crossbar through NVSwitch)
```

决定性特性：**全部 72 块 GPU 都是 NVLink-5 对等节点。** 任意 GPU 都能以全 NVLink-5 速度读取另一块 GPU 的 HBM。任意 GPU 都能用相同的一致性协议读取任意 Grace LPDDR。整个机架呈现为一个逻辑加速器，具有 30 TB 内存和 576 TB/s 带宽。

### 5.1 NVL72 的实际用途

**不**用于以默认配置为 Qwen2.5-72B 提供推理服务。那能装进一块 B200。把它放到 NVL72 上会浪费 71 张卡。

**是**用于：

* **假设的 Qwen3-Max / Qwen-1T** — 比 GB200 的 384 GB 更大的稠密模型。TP=72 将 1T 参数模型放入一个逻辑设备中。
* **大规模 Qwen-MoE 推理服务** — 总专家池达数百 GB，由 router 驱动的激活每个 token 选择约 10%。NVL72 将整个 MoE 保留在 HBM 中以获得全带宽。
* **万亿 token 并发上下文** — 10,000 个并发用户，每个 32k 上下文。聚合 KV = FP16 下 320 TB（FP8 下 210 TB）。大部分常驻 Grace，热层在 HBM 中。
* **极端批量的前沿推理服务** — 在 Qwen 级大模型上使用连续批处理实现 batch=2048+。

### 5.2 NVLink-5 网络上的 NCCL

NVL72 的 NCCL 集合通信使用 NVSwitch 网络。对于 1T 参数模型上的 TP=72（`d_model = 16384`）：

```
Per-layer AllReduce: 16384 floats × 2 bytes = 32 KB
× ~100 layers (1T-class) = 3.2 MB / token

NVLink-5 bandwidth in the rack: 130 TB/s bisection
Time per 32-KB AllReduce: ~10 µs at TP=72 (latency-bound)
Per-token NCCL time: 2 × 100 × 10 µs = 2 ms

Compute time per token estimate: ~25 ms
NCCL fraction: 2/27 ≈ 7%
```

健康。超过约 TP=72 后，你将开始受集合通信延迟限制，但在一个 NVL72 机架内你还有余量。

---


<details>
<summary>English original</summary>

**4.1 What you put in the warm tier**

Three production-relevant uses for warm-tier Grace memory:

**(a) Long-context KV spill.** When a sequence's KV exceeds HBM budget, page the cold blocks (oldest tokens, lowest-attention-weight blocks) to Grace LPDDR. Hot blocks (recent tokens, high-attention ones) stay in HBM. Production frameworks handle this with policies like LRU or attention-weight-based eviction.

```
Single sequence at 256k context:
  KV per layer per token (FP8) = 2 KB
  × 80 layers × 256k = 41 GB
  → All in HBM? Yes, but eats 20% of budget for one sequence.
  → Spill cold blocks to Grace: keep latest 32k in HBM, page older.
```

**(b) Prefix cache for shared system prompts.** Multi-tenant deployments often share a long system prompt across many requests (RAG context, agent system message, persona). Compute KV once, store in Grace, paste into HBM for each new request. Cuts TTFT for prompt-shared traffic by 10–100×.

**(c) MoE expert pool offload.** For Qwen-MoE variants (or hypothetical Qwen3-MoE), only ~10% of experts are active per token. Keep all expert weights in Grace; stream active experts to HBM each step. Trade some bandwidth for the ability to host much larger expert pools per GPU.

**4.2 Numbers for warm-tier KV**

A worked example: Qwen2.5-72B serving 16 concurrent users at 128k average context.

```
KV per user @ MX-FP8: 80 layers × 2 × 8 KV-heads × 128 head-dim × 128k × 1 byte
                    = 21 GB per user
16 users × 21 GB = 336 GB

Single-B200 HBM budget for KV: ~120 GB
GB200 superchip HBM (2 B200): ~240 GB
GB200 Grace LPDDR: 960 GB

Strategy:
  - Hot 16k tokens per user in HBM: 16 × 2.6 GB = 42 GB (fits HBM)
  - Cold 112k tokens per user in Grace LPDDR: 16 × 18.4 GB = 295 GB (fits Grace)
  - Total: 337 GB, comfortable.

Bandwidth cost per decode:
  Hot KV read (HBM): ~2.6 GB × 16 = 42 GB at 8 TB/s = 5 ms aggregate
  Cold KV read (Grace): ~18.4 GB × 16 = 295 GB at 900 GB/s = 330 ms aggregate
  → Cold-tier reads dominate if you re-read every step.
```

This is why production frameworks **don't** re-read cold KV every step. Instead, recompute KV scores for the cold tier sparingly (e.g., every 16 tokens), or use **sliding-window attention** for the cold tier so it never gets re-read at full bandwidth. Both patterns are how 256k+ context becomes affordable on GB200.

---

**5. NVL72 — The Rack-Scale Computer**

```
NVL72 Rack:
  - 36 GB200 superchips (72 B200, 36 Grace)
  - 18 compute trays, each holding 2 GB200
  - 9 NVSwitch trays
  - Liquid cooling loop
  - ~120 kW power, ~3000 lb
  - 13.8 TB HBM3e + 17.3 TB Grace LPDDR = ~30 TB coherent
  - 576 TB/s aggregate HBM bandwidth
  - 130 TB/s NVLink-5 bisection (full crossbar through NVSwitch)
```

The defining property: **all 72 GPUs are NVLink-5 peers.** Any GPU can read another GPU's HBM at full NVLink-5 speed. Any GPU can read any Grace LPDDR with the same coherent protocol. The entire rack presents as one logical accelerator with 30 TB of memory and 576 TB/s of bandwidth.

**5.1 What NVL72 is actually for**

**Not** for serving Qwen2.5-72B in default config. That fits in one B200. Putting it on NVL72 wastes 71 cards.

**Yes** for:

* **Hypothetical Qwen3-Max / Qwen-1T** — dense models bigger than the GB200's 384 GB. TP=72 puts a 1T-parameter model in one logical device.
* **Massive Qwen-MoE serving** — total expert pool in 100s of GB, with router-driven activation choosing ~10% per token. NVL72 holds the entire MoE in HBM for full bandwidth.
* **Trillion-token concurrent context** — 10,000 concurrent users at 32k context each. Aggregate KV = 320 TB at FP16 (210 TB at FP8). Mostly Grace-resident, with hot tiers in HBM.
* **Extreme-batch frontier serving** — batch=2048+ on Qwen-class large models with continuous batching.

**5.2 NCCL on NVLink-5 fabric**

NVL72's NCCL collectives use the NVSwitch fabric. For TP=72 on a 1T-parameter model (`d_model = 16384`):

```
Per-layer AllReduce: 16384 floats × 2 bytes = 32 KB
× ~100 layers (1T-class) = 3.2 MB / token

NVLink-5 bandwidth in the rack: 130 TB/s bisection
Time per 32-KB AllReduce: ~10 µs at TP=72 (latency-bound)
Per-token NCCL time: 2 × 100 × 10 µs = 2 ms

Compute time per token estimate: ~25 ms
NCCL fraction: 2/27 ≈ 7%
```

Healthy. Above ~TP=72 you'd start being collective-latency-bound, but within one NVL72 rack you have headroom.

---

</details>

## 6. 选择正确的平台 —— 决策矩阵

| Qwen 工作负载 | 推荐平台 | 原因 |
|---|---|---|
| Qwen2.5-72B，单用户低延迟对话 | 单块 B200，封装内 TP=2 | 单芯片 280 tok/s |
| Qwen2.5-72B，多租户对话（数百用户） | HGX B200（1 台整机，TP=8） | 聚合 32k tok/s，最简单 |
| Qwen2.5-72B，多租户且 128k+ 上下文 | GB200 superchip | 用 Grace LPDDR 做 KV spill |
| Qwen-MoE 推理服务 | GB200（小规模）或 NVL72（大规模） | Grace 承载专家池 |
| Qwen3-Max 级 dense（假设 300B+） | NVL72 部分节点，TP=16-32 | 较小的平台装不下 |
| 前沿 1T+ dense 或 MoE | NVL72 整机架 | TP=72，借助 NVSwitch 交叉开关 |
| 万亿 token 并发推理服务 | NVL72 多机架 | 需要大规模一致性内存 |
| 研究/训练邻接场景 | NVL72 —— 与 DGX SuperPOD 同款硬件 | 复用训练集群 |

---

## 7. 成本经济学 —— 多 GPU 何时划算

2026 年中的近似价格（芯片 + 集成，非标价，非本地部署 TCO）：

| 平台 | 成本 | 每 GPU 有效成本 |
|---|---|---|
| 单块 B200 载板 | ~$45k | $45k |
| HGX B200 8-GPU 板卡 | ~$280k | $35k |
| GB200 superchip | ~$100k | $50k |
| NVL72 机架 | ~$3.0M | $42k |

对于单块 B200 即可胜任的纯 Qwen2.5-72B 推理服务，**单 GPU 部署的 $/tok/s 最优**。对于需要多 GPU 集合通信的工作负载（前沿模型、极端并发），HGX B200 把多 GPU 的溢价摊薄到吞吐上。只有当工作负载确实要求 NVL72 的规模时，它才经济；规模更小的场景下，8-GPU HGX 的每 token 服务成本更低。

粗略的启发式：**停留在能装下你的工作负载、并达到所需吞吐的最小平台上。** 多 GPU 只用于「模型根本装不下」或「我需要超过 8 TB/s 的聚合带宽」。不要因为平台唾手可得就扩容。

---

## 8. 多 B200 部署的诊断

```bash
# 1. Confirm NVLink topology
nvidia-smi topo -m
# HGX B200 expected: NV18 (18-link NVLink-5) between every GPU pair

# 2. Confirm NCCL using NVLink not PCIe
NCCL_DEBUG=INFO NCCL_DEBUG_SUBSYS=ALL trtllm-bench ...
# Look for: "NCCL INFO Channel 00 : ... [NVLink]"

# 3. Check NCCL bandwidth in isolation
nccl-tests/build/all_reduce_perf -b 16K -e 1M -f 2 -g 8
# Healthy on HGX B200: 32 KB AllReduce ≤ 6 µs

# 4. On GB200, check Grace coherent path
numactl --hardware
# Should show 2 NUMA nodes per superchip with low cross-node distance

# 5. NVL72: check rack-level NVLink reach
nvidia-smi nvlink --status -l 0
# All 18 links should be Active with 100 Gbps each
```

如果其中任何一项暴露出连接性能下降，就可能出现单 GPU 性能正常而多 GPU 崩溃。在对模型做 benchmark 之前，务必先验证 fabric。

---

## 关键要点

| 要点 | 为何重要 |
|---|---|
| HGX B200 8-GPU 是 Qwen2.5-72B 的天然推理服务平台 | 一台整机即可替代 4–6 台 HGX H100 达到同等吞吐 |
| GB200 每 GPU 增加 480 GB 一致性 Grace LPDDR | 解锁长上下文 KV spill、前缀缓存、MoE 专家池 |
| NVL72 是机架级：30 TB 一致性内存，576 TB/s HBM | 面向前沿 dense 与 MoE 模型的平台 |
| TP 应整除 `n_kv_heads` —— Qwen2.5-72B 取 8 | TP=1,2,4,8 是 Qwen-72B 的天然配置 |
| NVLink-5 的集合通信延迟是 NVLink-4 的一半 | 在 HGX B200 上集合通信地板降到约 0.6 ms/token |
| 成本高效的部署 = 能装下的最小平台 | 不要仅仅因为机架可得就扩容 |
| 在对模型做 benchmark 之前务必验证 fabric | PCIe 上的 NCCL 会悄无声息地摧毁多 GPU 性能 |

---

## 资源

* **[NVIDIA GB200 NVL72 Architecture Brief](https://www.nvidia.com/en-us/data-center/gb200-nvl72/)：** 官方机架级平台。
* **[HGX B200 Datasheet](https://www.nvidia.com/en-us/data-center/hgx/)：** 8-GPU 板卡规格。
* **[NVIDIA NCCL Documentation](https://docs.nvidia.com/deeplearning/nccl/)：** 已针对 NVLink-5 / NVSwitch-5 更新。
* **[Chapter 3 — Single-B200 Qwen Inference](03-Single-B200-Qwen-Inference.md)：** 当你不需要多 GPU 时。
* **[Chapter 5 — Blackwell Kernel Engineering](05-Blackwell-Kernel-Engineering.md)：** 支撑这些集合通信的 kernel。
* **[阶段 5 — NCCL Deep Dive](../08-NCCL深度探索/README.md)：** 关于集合通信层的配套深度解析。


<details>
<summary>English original</summary>

**6. Choosing the Right Platform — The Decision Matrix**

| Qwen Workload | Recommended Platform | Why |
|---|---|---|
| Qwen2.5-72B, single-user low-latency chat | Single B200, intra-package TP=2 | 280 tok/s in one chip |
| Qwen2.5-72B, multi-tenant chat (100s users) | HGX B200 (1 box, TP=8) | 32k tok/s aggregate, simplest |
| Qwen2.5-72B, multi-tenant with 128k+ context | GB200 superchip | Grace LPDDR for KV spill |
| Qwen-MoE serving | GB200 (small) or NVL72 (large) | Grace holds expert pool |
| Qwen3-Max-class dense (hypothetical 300B+) | NVL72 partial, TP=16-32 | Doesn't fit smaller platforms |
| Frontier 1T+ dense or MoE | NVL72 full rack | TP=72 with NVSwitch crossbar |
| Trillion-token concurrent serving | NVL72 multi-rack | Need coherent memory at scale |
| Research/training adjacency | NVL72 — same hardware as DGX SuperPOD | Reuses the training fleet |

---

**7. Cost Economics — When Multi-GPU Pays Off**

Approximate mid-2026 pricing (chip + integration, not list, not on-prem TCO):

| Platform | Cost | Per-GPU effective cost |
|---|---|---|
| Single B200 carrier | ~$45k | $45k |
| HGX B200 8-GPU board | ~$280k | $35k |
| GB200 superchip | ~$100k | $50k |
| NVL72 rack | ~$3.0M | $42k |

For pure Qwen2.5-72B serving where one B200 suffices, **single-GPU deployment has the best $/tok/s**. For workloads that need multi-GPU collectives (frontier models, extreme concurrency), HGX B200 amortizes the multi-GPU premium across throughput. NVL72 is only economic when the workload demands its scale; for anything smaller, an 8-GPU HGX is cheaper per token served.

The rough heuristic: **stay on the smallest platform that fits your workload at the throughput you need.** Multi-GPU is for "the model literally doesn't fit" or "I need more aggregate bandwidth than 8 TB/s." Don't scale up because it's available.

---

**8. Diagnostics for Multi-B200 Deployments**

```bash
# 1. Confirm NVLink topology
nvidia-smi topo -m
# HGX B200 expected: NV18 (18-link NVLink-5) between every GPU pair

# 2. Confirm NCCL using NVLink not PCIe
NCCL_DEBUG=INFO NCCL_DEBUG_SUBSYS=ALL trtllm-bench ...
# Look for: "NCCL INFO Channel 00 : ... [NVLink]"

# 3. Check NCCL bandwidth in isolation
nccl-tests/build/all_reduce_perf -b 16K -e 1M -f 2 -g 8
# Healthy on HGX B200: 32 KB AllReduce ≤ 6 µs

# 4. On GB200, check Grace coherent path
numactl --hardware
# Should show 2 NUMA nodes per superchip with low cross-node distance

# 5. NVL72: check rack-level NVLink reach
nvidia-smi nvlink --status -l 0
# All 18 links should be Active with 100 Gbps each
```

If any of these reveal degraded connectivity, single-GPU performance can be fine while multi-GPU collapses. Always validate the fabric before bench-marking the model.

---

**Key Takeaways**

| Takeaway | Why it matters |
|---|---|
| HGX B200 8-GPU is the natural Qwen2.5-72B serving platform | One box replaces 4–6 HGX H100 boxes for the same throughput |
| GB200 adds 480 GB of coherent Grace LPDDR per GPU | Unlocks long-context KV spill, prefix caching, MoE pools |
| NVL72 is rack-scale: 30 TB coherent memory, 576 TB/s HBM | The platform for frontier dense and MoE models |
| TP should divide `n_kv_heads` — 8 for Qwen2.5-72B | TP=1,2,4,8 are the natural Qwen-72B configurations |
| NVLink-5 collective latency is half NVLink-4's | Collective floor drops to ~0.6 ms/token on HGX B200 |
| Cost-efficient deployment = smallest platform that fits | Don't scale up just because the rack is available |
| Always validate the fabric before benchmarking the model | NCCL on PCIe destroys multi-GPU performance silently |

---

**Resources**

* **[NVIDIA GB200 NVL72 Architecture Brief](https://www.nvidia.com/en-us/data-center/gb200-nvl72/):** Official rack-scale platform.
* **[HGX B200 Datasheet](https://www.nvidia.com/en-us/data-center/hgx/):** 8-GPU board spec.
* **[NVIDIA NCCL Documentation](https://docs.nvidia.com/deeplearning/nccl/):** Updated for NVLink-5 / NVSwitch-5.
* **[Chapter 3 — Single-B200 Qwen Inference](03-Single-B200-Qwen-Inference.md):** When you don't need multi-GPU.
* **[Chapter 5 — Blackwell Kernel Engineering](05-Blackwell-Kernel-Engineering.md):** The kernels backing these collectives.
* **[Phase 5 — NCCL Deep Dive](../08-NCCL深度探索/README.md):** Companion deep dive on the collective layer.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Blackwell-B200-Qwen-Inference/04-Multi-B200-NVL72.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Blackwell-B200-Qwen-Inference/04-Multi-B200-NVL72.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
