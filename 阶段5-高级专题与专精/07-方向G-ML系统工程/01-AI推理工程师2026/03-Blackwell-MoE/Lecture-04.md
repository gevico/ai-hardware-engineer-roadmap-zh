---
title: Part 3 · Lecture 04 — 分离式 Prefill / Decode：Mooncake、Splitwise、DistServe
description: Part 3 · Lecture 04 — 分离式 Prefill / Decode：Mooncake、Splitwise、DistServe
---

# Part 3 · Lecture 04 — 分离式 Prefill / Decode：Mooncake、Splitwise、DistServe

## 概述

Part 2 Lecture 05 中的推理服务栈（连续批处理、paged KV、prefix cache、推测）把每块 GPU 都视为同构——每块 GPU 既做 prefill（首字前的整段计算）又做 decode（逐 token 生成阶段）。2024–2025 年这一代推理研究显示这种做法并非最优：**prefill 是算力受限，decode 是带宽受限，二者理想的 GPU 不同、理想的精度也不同。**

**分离式 prefill / decode（P/D 分离）** 将一组 GPU 专门用于 prefill，另一组专门用于 decode，并在二者之间传输已填充的 KV cache。每个池针对其阶段进行优化。结果：**$/MTok 改善 2–3×**，代价是集群复杂度。

本节涵盖：

1. 分离的**成本经济学论证**。
2. **Mooncake**——Moonshot 公开的 P/D 架构。
3. **Splitwise**——微软研究院更早提出的方案。
4. **DistServe**——学术基线 + 开源实现。
5. **KV 传输机制**——带宽、延迟、调度。
6. **分离何时占优**——工作负载形态、模型架构、集群规模。
7. **NVL72 分离**——Blackwell 规模的实用部署。

读完本节，你应当能够针对给定的工作负载 + 模型 + 集群，预判分离是否划算，以及大致划算多少。

---

## 1. 成本经济学论证

对于 4× H200 上的 Llama 3.3 70B FP8 聊天工作负载，采用同址连续批处理：

* 每块 GPU 既做 prefill *又*做 decode。
* 在长 prefill 期间（4K token 的 prompt，TP=4 下约 250 ms），GPU 的 tensor core 利用率很高（~70%），但 HBM 读取利用率很低（~30%）。
* 在随后的 decode 步中（逐 token、带宽受限），GPU 的 HBM 利用率很高（~80%），但 tensor core 利用率很低（~5%）。

在每个阶段，**约 70% 的*另一个*维度被浪费**。GPU 为算力和带宽都付出了成本，但**一次只能充分利用其一**。

### 1.1 分离的核心洞察

运行两个集群：

* **Prefill 池**——针对算力优化的 GPU。HBM 带宽较低的更便宜的 GPU（例如用 H100 代替 H200）表现良好，因为 prefill 是 FLOP 受限。
* **Decode 池**——针对带宽优化的 GPU（H200、B200）。算力不是瓶颈。

用户请求流程：

```text
1. Request arrives → prefill pool
2. Prefill GPU runs the full prefill → produces full KV cache
3. KV cache transferred to decode pool over NVLink (or RDMA)
4. Decode GPU emits tokens
5. New request can join decode batch immediately (separate from any prefill)
```

### 1.2 经济账

如果 H100 的成本是 H200 的 2/3，且 prefill 是算力受限：

* **同址：** 所有 GPU 都是 H200（~$3/hour). 70% efficiency = effective $4.3/小时。
* **分离：** prefill 池是 H100（$2/hour, near 95% utilization); decode pool is H200 (95% utilization). Average $2.5/小时有效成本。

大致 **$/MTok 改善 30-40%**。来自 **Mooncake** 的实测数据与这相近。

对于 Blackwell 上的 MoE（混合专家模型），收益更大，因为：

* 激活参数（decode）与总参数不同（prefill 需要加载所有专家）。
* all-to-all 通信在批形状恒定的专用 decode 集群上更高效。

---

## 2. Mooncake——Moonshot AI 公开的架构

[Mooncake](https://arxiv.org/abs/2407.00079) 是 Moonshot 用于服务 Kimi（其旗舰 LLM）的生产级 P/D 系统。2024 年的论文揭示了若干工程洞察：

### 2.1 架构

```text
                ┌─── Prefill pool ────┐
   user req ───►│  H100 × 8 GPUs      │
                │  TP=8               │
                │  FP8 weights        │
                └─────────┬───────────┘
                          │ KV transfer
                          ▼
                ┌─── Decode pool ─────┐
                │  H200 × 8 GPUs      │
                │  TP=8               │
                │  FP8 weights        │
                │  FP8 KV cache       │
                └─────────────────────┘
```

* **Prefill 池**：成本更低的 GPU（用 H100 代替 H200）。算力比带宽更重要。
* **Decode 池**：带宽更高的 GPU。带宽比算力更重要。
* **KV 传输**：经高速网络或 NVLink 互联。

### 2.2 关键优化

1. **KV cache 布局**——为跨池传输效率而设计。按块对齐，逐 layer 连续。
2. **Prefix cache**——实现在 decode 池中（此处可服务大量用户）。跨请求的 prefix 匹配使用全局索引。
3. **延迟感知调度**——当 decode 池达到容量时，放缓 prefill 的接纳而不是堆积队列。
4. **KV 复用**——prefill 池计算完整的 KV；若之后到达相似请求，decode 池的 prefix cache 可短路这次传输。


<details>
<summary>English original</summary>

**Part 3 · Lecture 04 — Disaggregated Prefill / Decode: Mooncake, Splitwise, DistServe**

**Overview**

The serving stack from Part 2 Lecture 05 (continuous batching, paged KV, prefix cache, speculation) treats every GPU as homogeneous — every GPU does prefill *and* decode. The 2024–2025 generation of inference research showed this is suboptimal: **prefill is compute-bound, decode is bandwidth-bound, and they have different ideal GPUs and different ideal precisions.**

**Disaggregated prefill / decode (P/D disaggregation)** dedicates one pool of GPUs to prefill and another to decode, transferring the populated KV cache between them. Each pool is optimized for its phase. The result: **2–3× $/MTok improvement** at the cost of cluster complexity.

This lecture covers:

1. **The cost-economics argument** for disaggregation.
2. **Mooncake** — Moonshot's published P/D architecture.
3. **Splitwise** — Microsoft Research's earlier proposal.
4. **DistServe** — academic baseline + open-source implementation.
5. **KV transfer mechanics** — bandwidth, latency, scheduling.
6. **When disaggregation wins** — workload shape, model architecture, cluster size.
7. **NVL72 disaggregation** — practical Blackwell-scale deployment.

By the end you should be able to predict, for a given workload + model + cluster, whether disaggregation pays off, and roughly by how much.

---

**1. The cost-economics argument**

For Llama 3.3 70B FP8 chat workload on 4× H200, colocated continuous batching:

* Each GPU does prefill *and* decode.
* During a long prefill (4K-token prompt, ~250 ms on TP=4), the GPU is at high tensor-core utilization (~70%) but low HBM read utilization (~30%).
* During subsequent decode steps (per-token bandwidth-bound), the GPU is at high HBM utilization (~80%) but low tensor-core utilization (~5%).

In each phase, **~70% of the *other* dimension is wasted**. The GPU is paying for both compute and bandwidth but **only fully using one at a time**.

**1.1 The disaggregation insight**

Run two clusters:

* **Prefill pool** — GPUs optimized for compute. Cheaper GPUs with less HBM bandwidth (e.g., H100 instead of H200) work well because the prefill is FLOP-bound.
* **Decode pool** — GPUs optimized for bandwidth (H200, B200). Compute is not the bottleneck.

The user's request flow:

```text
1. Request arrives → prefill pool
2. Prefill GPU runs the full prefill → produces full KV cache
3. KV cache transferred to decode pool over NVLink (or RDMA)
4. Decode GPU emits tokens
5. New request can join decode batch immediately (separate from any prefill)
```

**1.2 The economics**

If H100s are 2/3 the cost of H200s and prefill is compute-bound:

* **Colocated:** all GPUs are H200 (~$3/hour). 70% efficiency = effective $4.3/hour.
* **Disaggregated:** prefill pool is H100 ($2/hour, near 95% utilization); decode pool is H200 (95% utilization). Average $2.5/hour effective.

Roughly **30-40% $/MTok improvement**. Real measurements from **Mooncake** show similar numbers.

For MoE on Blackwell, the gain is larger because:

* Active params (decode) are different from total params (prefill needs all experts loaded).
* All-to-all communication is more efficient on dedicated decode clusters with constant batch shape.

---

**2. Mooncake — Moonshot AI's published architecture**

[Mooncake](https://arxiv.org/abs/2407.00079) is the production P/D system Moonshot uses to serve Kimi (their flagship LLM). The 2024 paper revealed several engineering insights:

**2.1 Architecture**

```text
                ┌─── Prefill pool ────┐
   user req ───►│  H100 × 8 GPUs      │
                │  TP=8               │
                │  FP8 weights        │
                └─────────┬───────────┘
                          │ KV transfer
                          ▼
                ┌─── Decode pool ─────┐
                │  H200 × 8 GPUs      │
                │  TP=8               │
                │  FP8 weights        │
                │  FP8 KV cache       │
                └─────────────────────┘
```

* **Prefill pool**: lower-cost GPUs (H100 instead of H200). Compute matters more than bandwidth.
* **Decode pool**: higher-bandwidth GPUs. Bandwidth matters more than compute.
* **KV transfer**: via fast network or NVLink fabric.

**2.2 Key optimizations**

1. **KV cache layout** — laid out for cross-pool transfer efficiency. Block-aligned, contiguous per layer.
2. **Prefix cache** — implemented in the decode pool (where it can serve many users). Cross-request prefix matching uses a global index.
3. **Latency-aware scheduling** — when decode pool is at capacity, slow down prefill admission rather than build queues.
4. **KV reuse** — prefill pool computes the full KV; if a similar request arrives later, decode-pool prefix cache can short-circuit the transfer.

</details>

### 2.3 已报告的收益

论文中的数据：

* 同等硬件成本下，相比同置部署吞吐提升 75%。
* p99 首 token 时延改善约 40%。
* 长上下文（128K）工作负载获益最大。

这些都是**特定于工作负载**的。Mooncake 的收益假设是**具备大量前缀共享的对话型流量**。

---

## 3. Splitwise —— Microsoft Research

[Splitwise](https://arxiv.org/abs/2311.18677)（2024）是 Mooncake 的学术前身。核心洞见相同（把 prefill（首字前的整段计算）与 decode（逐 token 生成阶段）拆开），但工程选择不同：

* **硬件**：A100 / H100 混布，重点是异构硬件间的成本优化。
* **KV 传输**：基于 InfiniBand 的 RDMA，带宽低于 NVLink。
* **调度**：采用先 prefill 后 decode 的锁步方式，而非异步。

Splitwise 报告的收益与之相近（吞吐提升约 30-50%），但工程实现更早，且 Mooncake 采用的 NVLink 组网方式带宽效率更高。

---

## 4. DistServe —— 学术与开源

[DistServe](https://arxiv.org/abs/2401.09670)（2024）是把其中许多思路形式化的学术基线。它带有开源参考实现，已集成进 vLLM（实验性）和 SGLang（生产可用）。

关键思路：

* **Goodput 指标** —— 把“有用”吞吐定义为每秒满足 SLO 的请求数，而不是原始 tokens/s。
* **只有工作负载的 SLO 组合能获益时，分离部署才有用。** 纯吞吐型工作负载没有收益。
* **异构资源池调度** —— 显式建模哪个 GPU 类别属于哪个池。

DistServe 是当前落地到 vLLM V1 和 SGLang 中的分离部署特性的概念基础。

---

## 5. KV 传输机制

分离部署中最难的工程问题。

### 5.1 数据量

8K 上下文下 DeepSeek V3.1（MLA）：

```text
KV per request = 8192 tokens × 69 KB / token ≈ 560 MB
```

8K 上下文下 Llama 3.3 70B（GQA）：

```text
KV per request = 8192 tokens × 320 KB / token ≈ 2.6 GB
```

Qwen3-MoE 235B-A22B（GQA，94 层，4 个 KV head）：

```text
KV per request = 8192 × 192 KB ≈ 1.6 GB
```

**采用 MLA 的 MoE（混合专家模型，DeepSeek）KV 传输量远小于 GQA 模型。** 这是 MLA 压缩 KV 带来收益的另一个方面。

### 5.2 带宽

同一 NVLink 域内的两块 GPU 之间（NVL72）：

* NVLink 5：1.8 TB/s
* 传输 500 MB：约 0.3 ms
* 传输 2.6 GB：约 1.5 ms

两者**相对于 prefill 时间都很小**（长 prompt 下 prefill 需要数百毫秒）。在单个 NVLink 域内，KV 传输开销**不是瓶颈**。

跨 NVLink 域的两块 GPU 之间（不同的 NVL72 机架）：

* InfiniBand NDR（400 Gb/s）或 XDR（800 Gb/s）：有效 50-100 GB/s。
* 500 MB：约 10 ms
* 2.6 GB：约 50 ms

跨机架时，对于短 prompt，**KV 传输相对于 prefill 时间已相当可观**。跨机架的分离部署只对长 prompt（>4K token）才划算。

### 5.3 调度

设计良好的分离式系统会做流水线化：

1. Prefill GPU 计算第 1 层 → 在计算第 2 层的同时开始传输第 1 层的 KV。
2. Decode GPU 接收第 1 层 → 完整 prefill 一到即可使用。
3. 等 prefill 算完第 61 层时，第 1–60 层已经在 decode GPU 上。

有效的 KV 传输时间 ≈ **最后一层传输的时间，而非所有层** —— 比朴素做法小一个数量级。

这是 SGLang / Mooncake 的做法。各实现有所不同。

---

## 6. 分离部署何时胜出

决策表：

| 工作负载特征 | 分离部署收益 | 说明 |
|-------------------|----------------------|-------|
| 长 prompt（>2K） | 高 | prefill 开销重要；算力受限场景胜出 |
| 短 prompt（<512） | 极小 | prefill 很小；延迟下限主导 |
| 混合流量（对话） | 中等 | 取决于混合流量中 prefill/decode 的比例 |
| 大量前缀共享 | 若 decode 池有缓存则高 | 缓存命中可降低 prefill 数据量 |
| 纯 batch / 离线 | 低 | 无 SLO，同置部署的连续批处理更优 |
| MoE | 高于稠密模型 | prefill/decode 的激活参数量不同 |
| 纯 Hopper 部署 | 低 | 没有 FP4 来区分 decode GPU |
| NVL72 | 高 | 域内 KV 传输几乎无开销 |
| 跨机架 | 低到中等 | KV 传输占首 token 时延的很大比例 |

### 6.1 简单判据

如果工作负载满足：

* 平均 prompt 长度 > 2K token
* 平均输出长度 50-500 token
* 16 块以上 GPU 的多机架集群
* 前缀共享足够多

→ 分离部署在 $/MTok 上很可能带来 20-50% 的收益。

如果工作负载是**短 prompt 的 agent 循环或纯 batch**，**就保持同置部署** —— 工程复杂度不值得。

---


<details>
<summary>English original</summary>

**2.3 Reported gains**

From the paper:

* 75% throughput improvement vs colocated at the same hardware cost.
* p99 TTFT improved ~40%.
* Long-context (128K) workloads benefit most.

These are **workload-specific**. Mooncake's gains assume **chat-shape traffic with substantial prefix sharing**.

---

**3. Splitwise — Microsoft Research**

[Splitwise](https://arxiv.org/abs/2311.18677) (2024) is the academic precursor to Mooncake. Same basic insight (split prefill from decode), with different engineering choices:

* **Hardware**: A100 / H100 mix, focused on cost optimization across heterogeneous hardware.
* **KV transfer**: RDMA over InfiniBand, lower bandwidth than NVLink.
* **Scheduling**: lock-step prefill-then-decode rather than asynchronous.

Splitwise's reported gains were similar (~30-50% throughput improvement), but the engineering is older and the NVLink-fabric approach Mooncake uses is more bandwidth-efficient.

---

**4. DistServe — academic + open-source**

[DistServe](https://arxiv.org/abs/2401.09670) (2024) is the academic baseline that formalized many of these ideas. It comes with an open-source reference implementation, integrated into vLLM (experimental) and SGLang (production).

Key ideas:

* **Goodput metric** — defines "useful" throughput as requests-meeting-SLO per second, not raw tokens/sec.
* **Disaggregation only helps if the workload SLO-mix benefits.** Pure throughput workloads don't gain.
* **Heterogeneous-pool scheduling** — explicit modeling of which GPU class belongs to which pool.

DistServe is the conceptual foundation for the disaggregation features now landing in vLLM V1 and SGLang.

---

**5. KV transfer mechanics**

The hardest engineering problem in disaggregation.

**5.1 The volume**

For DeepSeek V3.1 (MLA) at 8K context:

```text
KV per request = 8192 tokens × 69 KB / token ≈ 560 MB
```

For Llama 3.3 70B (GQA) at 8K context:

```text
KV per request = 8192 tokens × 320 KB / token ≈ 2.6 GB
```

For Qwen3-MoE 235B-A22B (GQA, 94 layers, 4 KV heads):

```text
KV per request = 8192 × 192 KB ≈ 1.6 GB
```

**MoE with MLA (DeepSeek) has much smaller KV transfer than GQA models.** This is another way MLA's compressed KV pays off.

**5.2 The bandwidth**

Between two GPUs in the same NVLink domain (NVL72):

* NVLink 5: 1.8 TB/s
* 500 MB transfer: ~0.3 ms
* 2.6 GB transfer: ~1.5 ms

Both are **small relative to the prefill time** (which is hundreds of milliseconds for long prompts). KV transfer cost is **not the bottleneck** within a single NVLink domain.

Between two GPUs across NVLink domains (different NVL72 racks):

* InfiniBand NDR (400 Gb/s) or XDR (800 Gb/s): 50-100 GB/s effective.
* 500 MB: ~10 ms
* 2.6 GB: ~50 ms

Across racks, **KV transfer becomes substantial relative to prefill time** for short prompts. Cross-rack disaggregation is only worth it for long prompts (>4K tokens).

**5.3 Scheduling**

A well-designed disaggregated system pipelines:

1. Prefill GPU does layer 1 → starts transferring layer 1 KV while computing layer 2.
2. Decode GPU receives layer 1 → ready to use as soon as full prefill arrives.
3. By the time prefill finishes layer 61, layers 1–60 are already on the decode GPU.

Effective KV transfer time ≈ **time of last layer's transfer, not all layers** — order-of-magnitude smaller than naive.

This is the SGLang / Mooncake approach. Implementations differ.

---

**6. When disaggregation wins**

A decision table:

| Workload property | Disaggregation gain | Notes |
|-------------------|----------------------|-------|
| Long prompts (>2K) | high | prefill cost matters; compute-bound win |
| Short prompts (<512) | minimal | prefill is small; latency floor dominates |
| Mixed traffic (chat) | moderate | depends on prefill/decode ratio in the mix |
| Heavy prefix sharing | high if decode-pool cache | cache hit reduces prefill volume |
| Pure batch / offline | low | no SLO, colocated continuous-batching wins |
| MoE | higher than dense | active params differ between prefill/decode |
| Hopper-only deployment | low | no FP4 to differentiate decode GPU |
| NVL72 | high | KV transfer near-free within domain |
| Cross-rack | low to moderate | KV transfer is large fraction of TTFT |

**6.1 The simple test**

If your workload has:

* Average prompt length > 2K tokens
* Average output length 50-500 tokens
* Multiple-rack cluster of 16+ GPUs
* Sufficient prefix sharing

→ disaggregation likely wins 20-50% on $/MTok.

If your workload is **short-prompt agent loops or pure batch**, **stay colocated** — the engineering complexity isn't worth it.

---

</details>

## 7. NVL72 分离式部署实践

对于 NVL72 上的 DeepSeek V3.1：

```text
72 GPUs total
- Prefill pool: 16 GPUs (TP=2 × EP=8), holding full 671B at FP4
- Decode pool: 48 GPUs (multiple replicas: TP=2 × EP=8, run 3 replicas)
- 8 GPUs: shared infrastructure (scheduler, prefix cache, monitoring)
```


prefill 池与 decode 池之间的 KV 传输发生在 NVLink fabric 内部——即使是 4K 上下文的 KV，也只有约 1-3 ms。

相比共置部署报告的吞吐提升（来自类似配置下的 SGLang benchmark）：

* 相同硬件成本下 tokens/sec 提升 35-50%。
* p99 TTFT 改善：30-40%。

### 7.1 SGLang 中的配置

```bash
# Prefill server
python -m sglang.launch_server \
    --model-path deepseek-ai/DeepSeek-V3.1 \
    --tp-size 2 --ep-size 8 \
    --disaggregation-mode prefill \
    --port 30001

# Decode server (run multiple)
python -m sglang.launch_server \
    --model-path deepseek-ai/DeepSeek-V3.1 \
    --tp-size 2 --ep-size 8 \
    --disaggregation-mode decode \
    --port 30002 \
    --connect-to-prefill <prefill-host>:30001
```


vLLM V1 的分离式支持仍处于实验阶段；截至 2026 年年中，可生产使用的选项是 SGLang。

### 7.2 NVL72 何时不划算

对于低并发工作负载或短 prompt 的 agent 循环，NVL72 分离式部署的开销超过收益。在 8× B200 或更小的部署上**保持共置**。

---

## 实验——在 Qwen3-MoE 上测量分离式部署

目标：产出一份分离式部署的成本经济性报告。

1. **硬件**——NVL72 分区，最好 16 张 GPU（或 8× B200 簇）。
2. **模型**——Qwen3-MoE 235B-A22B FP4（若具备该簇，可用 DeepSeek）。
3. **Runtime**——带分离式支持的 SGLang 0.5+。
4. **基线**——8× B200 上的共置连续批处理，concurrency=64。
5. **候选方案**——4× B200 prefill + 4× B200 decode，并发相同。
6. **测量**——吞吐、p99 TTFT、p99 TPOT，覆盖 chat 形态与长上下文形态的工作负载。
7. **计算**——各自的 $/MTok。每个 replica 使用相同的硬件成本。

通过标准：你能用实测数据论证某个具体产品工作负载是否应上线分离式部署。

---

## 自检

1. 对 8K 上下文的 Llama 3.3 70B GQA，KV 传输量约 2.6 GB。对 8K 的 DeepSeek V3.1 MLA，约 560 MB。MLA 为什么对分离式部署特别有帮助？
2. 同事提议对纯批处理工作负载（无 SLO）做分离式部署。用两句话支持或否决。
3. NVLink 5 让 NVL72 内部的 KV 传输变得轻而易举。为什么跨机架的分离式部署仍然吃力？
4. Mooncake 报告了 75% 的吞吐提升。哪些工作负载假设让这一结果可信，又可能在哪里失效？
5. 在 8× B200 上用连续批处理时，p99 TTFT 为 800 ms（目标 500 ms）。你考虑做分离式部署。决定是否投入的第一个测量是什么？

---

## 参考资料

* Mooncake — [arXiv:2407.00079](https://arxiv.org/abs/2407.00079)
* Splitwise — [arXiv:2311.18677](https://arxiv.org/abs/2311.18677)
* DistServe — [arXiv:2401.09670](https://arxiv.org/abs/2401.09670)
* SGLang 分离式部署指南 — [sgl-project.github.io](https://sgl-project.github.io/)
* "TetriInfer"（近期分离式部署工作）— [arXiv:2401.11181](https://arxiv.org/abs/2401.11181)

交叉引用：

* [第 1 部分 → 第 05 讲——Runtime 全景（分离式部署注记）](../01-基础/Lecture-05.md)
* [第 2 部分 → 第 05 讲——现代推理服务栈](../02-Hopper稠密计算/Lecture-05.md)——用于共置基线

---

## 截至 2026-06 的现状

Mooncake / DistServe / Splitwise 是 2024–2025 年的权威论文。SGLang 0.5+ 具备生产级分离式部署能力；vLLM V1 仍为实验性。当出现重要的新 P/D 论文，或 vLLM 稳定了分离式 API 时再更新。

---

## 接下来

* 下一讲：[第 05 讲——生产级 MoE 推理服务：MTP 推测、受限 decode、成本模型](Lecture-05.md)
* 上一讲：[第 03 讲——专家并行](Lecture-03.md)
* 上级：[第 3 部分——Blackwell 上的 MoE](README.md)


<details>
<summary>English original</summary>

**7. NVL72 disaggregation in practice**

For DeepSeek V3.1 on NVL72:

```text
72 GPUs total
- Prefill pool: 16 GPUs (TP=2 × EP=8), holding full 671B at FP4
- Decode pool: 48 GPUs (multiple replicas: TP=2 × EP=8, run 3 replicas)
- 8 GPUs: shared infrastructure (scheduler, prefix cache, monitoring)
```

KV transfer between prefill and decode pools is within the NVLink fabric — ~1-3 ms even for 4K-context KVs.

Reported throughput improvement vs colocated (from SGLang benchmarks on similar setups):

* +35-50% tokens/sec at the same hardware cost.
* p99 TTFT improvement: 30-40%.

**7.1 Configuration in SGLang**

```bash
# Prefill server
python -m sglang.launch_server \
    --model-path deepseek-ai/DeepSeek-V3.1 \
    --tp-size 2 --ep-size 8 \
    --disaggregation-mode prefill \
    --port 30001

# Decode server (run multiple)
python -m sglang.launch_server \
    --model-path deepseek-ai/DeepSeek-V3.1 \
    --tp-size 2 --ep-size 8 \
    --disaggregation-mode decode \
    --port 30002 \
    --connect-to-prefill <prefill-host>:30001
```

vLLM V1 has experimental disaggregation support; the production-ready option as of mid-2026 is SGLang.

**7.2 When NVL72 doesn't pay**

For low-concurrency workloads or short-prompt agent loops, NVL72 disaggregation overhead exceeds the gain. **Stay colocated** on 8× B200 or smaller deployments.

---

**Lab — measure disaggregation on Qwen3-MoE**

Goal: produce the disaggregation cost-economics report.

1. **Hardware** — NVL72 partition, ideally 16 GPUs (or 8× B200 cluster).
2. **Model** — Qwen3-MoE 235B-A22B FP4 (DeepSeek if you have the cluster).
3. **Runtime** — SGLang 0.5+ with disaggregation.
4. **Baseline** — colocated continuous batching on 8× B200 with concurrency=64.
5. **Candidate** — 4× B200 prefill + 4× B200 decode, same concurrency.
6. **Measure** — throughput, p99 TTFT, p99 TPOT for chat-shape and long-context-shape workloads.
7. **Compute** — $/MTok for each. Use the same hardware cost per replica.

Pass criterion: you can defend whether to ship disaggregation for a specific product workload with measured numbers.

---

**Self-check**

1. For Llama 3.3 70B GQA at 8K context, KV transfer is ~2.6 GB. For DeepSeek V3.1 MLA at 8K, it's ~560 MB. Why does MLA help disaggregation specifically?
2. A teammate proposes disaggregating a pure batch workload (no SLO). Defend or reject in two sentences.
3. NVLink 5 makes KV transfer within NVL72 trivial. Why does cross-rack disaggregation still struggle?
4. Mooncake reports 75% throughput improvement. What workload assumptions make this plausible, and where might it fail?
5. At 8× B200 with continuous batching, your p99 TTFT is 800 ms (target 500 ms). You consider disaggregation. What's the first measurement that decides whether to commit?

---

**References**

* Mooncake — [arXiv:2407.00079](https://arxiv.org/abs/2407.00079)
* Splitwise — [arXiv:2311.18677](https://arxiv.org/abs/2311.18677)
* DistServe — [arXiv:2401.09670](https://arxiv.org/abs/2401.09670)
* SGLang disaggregation guide — [sgl-project.github.io](https://sgl-project.github.io/)
* "TetriInfer" (recent disaggregation work) — [arXiv:2401.11181](https://arxiv.org/abs/2401.11181)

Cross-references:

* [Part 1 → Lecture 05 — Runtime landscape (disaggregation note)](../01-基础/Lecture-05.md)
* [Part 2 → Lecture 05 — Modern serving stack](../02-Hopper稠密计算/Lecture-05.md) — for the colocated baseline

---

**Current as of 2026-06**

Mooncake / DistServe / Splitwise as the canonical 2024–2025 papers. SGLang 0.5+ has production-quality disaggregation; vLLM V1 experimental. Refresh when major new P/D papers land or when vLLM stabilizes the disaggregation API.

---

**Next**

* Next: [Lecture 05 — Production MoE serving: MTP speculation, constrained decode, cost model](Lecture-05.md)
* Previous: [Lecture 03 — Expert parallelism](Lecture-03.md)
* Up: [Part 3 — MoE at Blackwell](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 3 - MoE at Blackwell/Lecture-04.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%203%20-%20MoE%20at%20Blackwell/Lecture-04.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
