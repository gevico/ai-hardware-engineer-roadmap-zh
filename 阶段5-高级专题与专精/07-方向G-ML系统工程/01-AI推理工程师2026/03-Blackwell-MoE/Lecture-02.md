---
title: 第 3 部分 · 第 02 讲 — Blackwell 硬件故事：B200、B300、GB200 NVL72、Transformer Engine 2、FP4
description: 第 3 部分 · 第 02 讲 — Blackwell 硬件故事：B200、B300、GB200 NVL72、Transformer Engine 2、FP4
---

# 第 3 部分 · 第 02 讲 — Blackwell 硬件故事：B200、B300、GB200 NVL72、Transformer Engine 2、FP4

## 概述

**Blackwell**（Compute Capability 10.0）是面向 **2025–2027 年万亿参数 MoE（混合专家模型）推理服务**的推理芯片。它建立在 Hopper 之上，但改变了**三件事**，这三件事对 MoE 推理至关重要：

1. **FP4 原生算术**，通过 Transformer Engine 2 实现 —— FP8 吞吐翻倍，权重的 HBM 字节数减半。
2. **NVLink 5 + 更大的 NVLink 域（NVL72）** —— 把 72 个 GPU 互连为单一一致性 fabric，使 EP 得以规模化。
3. **8 TB/s 的 HBM3e，每 GPU 192-288 GB** —— 具备把分片的 MoE 专家放在离计算很近处的带宽与容量。

本讲覆盖适用于 MoE 推理的 Blackwell 栈：

1. **Blackwell 芯片** —— 双 die 封装、SM 架构、张量核心。
2. **B200 与 B300** —— 容量与带宽差异。
3. **Grace+Blackwell（GB200）** —— 一致的 CPU-GPU 内存及其带来的能力。
4. **GB200 NVL72** —— 作为生产级 MoE 目标的 72-GPU NVLink 域。
5. **Transformer Engine 2 与 FP4** —— microscaling 格式与 scale 管理。
6. **NVLink 5** —— fabric 带宽、延迟、all-to-all 原语。
7. 相对 Hopper，**runtime 层有什么变化**。

读完之后，你应当能把 DeepSeek V3.1 与 Qwen3-MoE 235B-A22B 映射到 B200 / B300 / GB200 NVL72 部署上，并为精度与互连的选择给出理由。

---

## 1. Blackwell 芯片 —— 盒子里有什么

### 1.1 封装

Blackwell 以**双 die** 封装出货。每颗 die 相当于一整颗 Hopper 芯片；两者通过 **10 TB/s** 的 chip-to-chip 互连（NV-C2C）相连。对软件而言，这个双 die 封装**看起来就是一颗 GPU**，只是 SM 数量翻倍、HBM 堆栈翻倍。

* **总 SM 数（B200）：** 208（每 die 104 × 2）
* **张量核心总数：** 832（每 SM 4 个）
* **HBM 堆栈：** 8（每 die 4 个）
* **L2 缓存：** 片上约 60 MB（每封装）
* **NVLink：** 18 条链路，每条 100 GB/s → 每 GPU 1.8 TB/s（NVLink 5）

### 1.2 张量核心

第 5 代张量核心支持：

* BF16、FP16、FP8（继承自 Hopper）
* **FP6**（E3M2、E2M3 —— 部分支持）
* **FP4**（E2M1、MX-FP4 microscaling —— 新增的原生运算）
* INT8、INT4（仅权重）

各精度的峰值吞吐：

| 精度 | 峰值 TFLOPs（B200） | 相对 FP16 的倍数 |
|-----------|---------------------|----------------|
| BF16/FP16 | 2,250 | 1.0× |
| FP8 | 4,500 | 2.0× |
| FP6 | 4,500（与 FP8 相同） | 2.0× |
| FP4 | 9,000 | 4.0× |

**FP4 是最大亮点：**吞吐达到 FP16 的 4 倍。对于权重主导 HBM 带宽的 MoE 工作负载，FP4 还把每参数读取字节数减半。两种效应叠加。

### 1.3 HBM

* **B200 HBM3e：** 192 GB，8.0 TB/s
* **B300 HBM3e：** 288 GB，约 8.0 TB/s（容量提升，带宽相当）

相对 Hopper 的带宽倍数：

* H100 HBM3：3.35 TB/s → B200：8.0 TB/s —— **带宽提升 2.4 倍**
* H200 HBM3e：4.80 TB/s → B200：8.0 TB/s —— **带宽提升 1.67 倍**

对于 decode（逐 token 生成阶段）这类带宽受限的场景，在尚未考虑 FP4 之前，Blackwell 在相同精度下就比 Hopper **快得多**。

---

## 2. B200 与 B300

这两款 SKU 在 2025–2026 年的不同时间点出货：

| 规格 | B200 SXM | B300 SXM |
|------|----------|----------|
| HBM 容量 | 192 GB | 288 GB |
| HBM 带宽 | 8.0 TB/s | ~8.0 TB/s |
| 峰值 FP16 TFLOPs | 2,250 | ~2,500（略高） |
| 峰值 FP4 TFLOPs | 9,000 | ~15,000（dense） |
| TDP | 1000 W | 1200 W |
| NVLink | NVLink 5 | NVLink 5 |

**B200** 是 2025 年的主力 SKU。**B300** 于 2025 年底 / 2026 年初推出，带来容量提升（对高 batch 下最大的 MoE 而言是必需的）。

DeepSeek V3.1 在 FP4 下（总计 350 GB）：

* B200 192 GB：至少需要 2× B200，权重拆分。
* B300 288 GB：至少需要 2× B300。
* 考虑实际批大小与 KV cache 余量：4× B200 / B300 或更多。

Qwen3-MoE 235B-A22B 在 FP4 下（118 GB）：

* B200 192 GB：**可放在单颗 GPU 上**，KV 余量可支持约 16K 上下文。
* B300 288 GB：在长上下文下也轻松装下。

---

## 3. Grace + Blackwell（GB200）

GB200 超级芯片在单块板卡上把**一颗 Grace CPU** 与**两颗 Blackwell GPU**（每颗 Blackwell 含 2 颗 B200 die，即每块 GB200 共 4 颗 die）配对，并具备**一致性内存**。

### 3.1 一致的 CPU-GPU 内存

* Grace CPU 最多有 480 GB LPDDR5x。
* 两颗 Blackwell GPU 各有 192 GB HBM3e（每块 GB200 共 384 GB）。
* NV-C2C 以总计 900 GB/s 的双向带宽连接 CPU 内存与 GPU 内存（每个方向约 450 GB/s）。
* **GPU 可以直接读取 CPU 内存** —— pinned、大页，无需显式拷贝。

对 MoE 推理而言，这带来了**常驻 CPU 的专家卸载**：很少被激活的专家可以放在 CPU 内存中，按需取用。这是一个**活跃的研究课题**；生产 runtime 已开始使用，但并非默认配置。


<details>
<summary>English original</summary>

**Part 3 · Lecture 02 — Blackwell Hardware Story: B200, B300, GB200 NVL72, Transformer Engine 2, FP4**

**Overview**

**Blackwell** (Compute Capability 10.0) is the inference silicon for **2025–2027 trillion-parameter MoE serving**. It builds on Hopper but changes **three things** that matter for MoE inference:

1. **FP4 native arithmetic** via Transformer Engine 2 — doubles FP8 throughput, halves HBM bytes for weights.
2. **NVLink 5 + larger NVLink domains (NVL72)** — interconnect for 72 GPUs in one coherent fabric, enabling EP at scale.
3. **HBM3e at 8 TB/s, 192-288 GB per GPU** — the bandwidth and capacity to hold sharded MoE experts close to the compute.

This lecture covers the Blackwell stack as it applies to MoE inference:

1. **Blackwell silicon** — dual-die package, SM architecture, tensor cores.
2. **B200 vs B300** — capacity and bandwidth differences.
3. **Grace+Blackwell (GB200)** — coherent CPU-GPU memory and what it enables.
4. **GB200 NVL72** — the 72-GPU NVLink domain that is the production MoE target.
5. **Transformer Engine 2 and FP4** — microscaling format, scale management.
6. **NVLink 5** — fabric bandwidth, latency, all-to-all primitives.
7. **What changes vs Hopper** for the runtime layer.

By the end you should be able to map DeepSeek V3.1 and Qwen3-MoE 235B-A22B onto a B200 / B300 / GB200 NVL72 deployment and justify the precision + interconnect choices.

---

**1. Blackwell silicon — what's in the box**

**1.1 The package**

Blackwell ships as a **dual-die** package. Each die is comparable to a full Hopper chip; they are connected via a chip-to-chip interconnect (NV-C2C) at **10 TB/s**. To software, the dual-die package **looks like one GPU** with twice the SMs and twice the HBM stacks.

* **Total SMs (B200):** 208 (104 per die × 2)
* **Total tensor cores:** 832 (4 per SM)
* **HBM stacks:** 8 (4 per die)
* **L2 cache:** ~60 MB on-chip (per package)
* **NVLink:** 18 links at 100 GB/s each → 1.8 TB/s per GPU (NVLink 5)

**1.2 Tensor cores**

The 5th-generation tensor cores support:

* BF16, FP16, FP8 (inherited from Hopper)
* **FP6** (E3M2, E2M3 — partial support)
* **FP4** (E2M1, MX-FP4 microscaling — new native operation)
* INT8, INT4 (weight-only)

Peak throughput at each precision:

| Precision | Peak TFLOPs (B200) | Ratio vs FP16 |
|-----------|---------------------|----------------|
| BF16/FP16 | 2,250 | 1.0× |
| FP8 | 4,500 | 2.0× |
| FP6 | 4,500 (same as FP8) | 2.0× |
| FP4 | 9,000 | 4.0× |

**FP4 is the headline:** 4× the FP16 throughput. For MoE workloads where weights dominate HBM bandwidth, FP4 also halves the bytes-per-param read. Both effects compound.

**1.3 HBM**

* **B200 HBM3e:** 192 GB at 8.0 TB/s
* **B300 HBM3e:** 288 GB at ~8.0 TB/s (capacity bump, bandwidth comparable)

The bandwidth ratio vs Hopper:

* H100 HBM3: 3.35 TB/s → B200: 8.0 TB/s — **2.4× more bandwidth**
* H200 HBM3e: 4.80 TB/s → B200: 8.0 TB/s — **1.67× more bandwidth**

For decode (bandwidth-bound), Blackwell is **much faster at the same precision** than Hopper, before FP4 even enters the discussion.

---

**2. B200 vs B300**

The two SKUs ship at different points in 2025–2026:

| Spec | B200 SXM | B300 SXM |
|------|----------|----------|
| HBM capacity | 192 GB | 288 GB |
| HBM bandwidth | 8.0 TB/s | ~8.0 TB/s |
| Peak FP16 TFLOPs | 2,250 | ~2,500 (slightly higher) |
| Peak FP4 TFLOPs | 9,000 | ~15,000 (dense) |
| TDP | 1000 W | 1200 W |
| NVLink | NVLink 5 | NVLink 5 |

**B200** is the primary 2025 SKU. **B300** lands in late 2025 / early 2026 with a capacity bump (necessary for the largest MoEs at high batch).

For DeepSeek V3.1 at FP4 (350 GB total):

* B200 192 GB: requires 2× B200 minimum, weights split.
* B300 288 GB: requires 2× B300 minimum.
* For practical batch sizes and KV cache headroom: 4× B200 / B300 or larger.

For Qwen3-MoE 235B-A22B at FP4 (118 GB):

* B200 192 GB: **fits on a single GPU** with KV headroom up to ~16K context.
* B300 288 GB: fits comfortably with long context.

---

**3. Grace + Blackwell (GB200)**

The GB200 superchip pairs **one Grace CPU** with **two Blackwell GPUs** (B200 dies × 2 per Blackwell = 4 dies per GB200) on a single board with **coherent memory**.

**3.1 Coherent CPU-GPU memory**

* The Grace CPU has up to 480 GB of LPDDR5x.
* The two Blackwell GPUs have 192 GB HBM3e each (384 GB total per GB200).
* NV-C2C links the CPU memory to the GPU memory at 900 GB/s total bidirectional (~450 GB/s per direction).
* **The GPU can read CPU memory directly** — pinned, large pages, no explicit copy.

For MoE inference, this enables **CPU-resident expert offload**: experts that are rarely activated can live in CPU memory, fetched on demand. This is an **active research topic**; production runtimes are starting to use it but it's not the default.

</details>

### 3.2 GB200 为何重要

GB200 超级芯片是 **NVL72 的构建单元**。一个 NVL72 机架包含 36 颗 GB200 超级芯片 = 72 颗 Blackwell GPU + 36 颗 Grace CPU，全部处于同一个 NVLink 域内。

对于 671B MoE（混合专家模型）的规模化推理服务，**NVL72 才是实际的目标形态**。单机（8× B200）部署适用于**更小的 MoE 和更低的规模**。

---

## 4. GB200 NVL72 —— 生产环境的 MoE 目标平台

```text
NVL72 Rack:
  - 36× GB200 superchips
  - 72× Blackwell GPUs (B200 × 36 boards × 2 GPUs)
  - 36× Grace CPUs
  - 13.5 TB aggregate HBM3e (72 × 192 GB)
  - 17 TB aggregate Grace LPDDR5x
  - NVLink Switch v4: full all-to-all 1.8 TB/s per GPU
  - Aggregate NVLink bandwidth: ~130 TB/s
  - Liquid cooled
  - ~120 kW power
```

对于 FP4（350 GB）的 DeepSeek V3.1 671B：

* 可分布在 4-8 颗 GPU 上（一个 TP+EP 分区）。
* NVL72 中其余约 64 颗 GPU 承载更多分区（数据并行副本），从而实现大规模并发。

对于一个推理工作负载：

* **单副本：** 8-16 颗 GPU，采用 TP × EP 组合。
* **每个 NVL72 多个副本：** 4-8 个副本，各自服务独立流量，共享 NVLink 互联结构。

NVL72 还通过把 72 颗 GPU 划分为 prefill（首字前的整段计算）池和 decode（逐 token 生成阶段）池、两者由 NVLink 互联结构连接以传输 KV，实现了**分离式 prefill / decode**（Lecture 04）。

### 4.1 NVL72 vs HGX H100 8-GPU box

飞跃是巨大的：

| 属性 | 8× H100 HGX | GB200 NVL72 |
|----------|-------------|-------------|
| 域内 GPU 数 | 8 | 72 |
| 单 GPU HBM | 80 GB HBM3 | 192 GB HBM3e |
| 单 GPU HBM 带宽 | 3.35 TB/s | 8.0 TB/s |
| 单 GPU NVLink | 900 GB/s | 1.8 TB/s |
| HBM 总量 | 640 GB | 13.5 TB |
| NVLink 带宽总量 | 7.2 TB/s | 130 TB/s |

**大 9 倍的 NVLink 域，正是 MoE EP 能在全规模下落到实处的关键。** 在 8× H100 上，EP=8 意味着每颗 GPU 承载 1/8 的专家；在 NVL72 上，EP=64 意味着每颗 GPU 承载 1/64。token 路由更便宜，因为每步落到每个 rank 上的 token 更少——但负载均衡变得**更难**，而不是更容易：每个 rank 上的专家更少意味着单 rank 负载方差更大，一个热点专家就可能主导其所在 rank 的步进时间（跨 rank 的热点专家复制是标准缓解手段——Lecture 03）。

---

## 5. Transformer Engine 2 与 FP4

这是 Blackwell 上对推理而言最重要的软件特性。

### 5.1 FP4 存的是什么

**E2M1 格式：** 1 位符号 + 2 位指数 + 1 位尾数 = 4 bit。可表示的数值：{0, ±0.5, ±1, ±1.5, ±2, ±3, ±4, ±6}。即 8 个数值再加上符号。

这是一种**非常粗糙**的表示。单独使用的话，精度一致性会非常差。诀窍在于 **microscaling**。

### 5.2 Microscaling（MX-FP4）

每 32 个连续元素组成的块共享一个**共享缩放因子**（通常是 FP8 E8M0——只保留指数）。元素 i 的实际值为：

```text
value_i = fp4_value_i × 2^(shared_scale)
```

这赋予了 FP4 **接近 FP8 的有效动态范围**——缩放因子为每个块提供 256 个可能的指数，FP4 尾数则在该范围内提供 8 个不同的取值。

该格式由 Open Compute Project（OCP）以 **MX-FP4** 之名标准化（[opencompute.org](https://www.opencompute.org/)）。

### 5.3 TE2 中的缩放因子管理

Transformer Engine 2 会自动管理缩放因子：

* 权重的**逐张量缩放因子**（对张量取统计最大绝对值）。
* 张量内部的**逐块缩放因子**（即 microscaling 因子）。
* 推理过程中激活值的**在线重新校准**（类似 TE1 中的 FP8）。

这相当于 FP4 版本的 TE1 FP8 amax 历史。部署时通常无需手动管理缩放因子——由 TE2 库完成。

### 5.4 FP4 的代价所在

FP4 权重和激活值已有充分研究。**FP4 KV cache 更激进**，即便采用逐块缩放，在长上下文评测中通常也会损失 1–3 pp 的精度一致性。**FP8 KV 才是生产默认值**；FP4 KV 仅用于显存压力下的恢复。

### 5.5 吞吐影响

对于 Qwen3-MoE 235B-A22B，B200 上的 FP4 对比 H200 上的 FP8：

| 精度 | 硬件 | 每 token 读取的激活参数权重 | Decode TPOT（batch=1） |
|-----------|----------|--------------------------------------|------------------------|
| FP8 | 4× H200 | 22 GB | ~7 ms |
| FP4 | 4× B200 | 11 GB | ~2 ms（带宽上限） |

在该模型上从 FP8→FP4 大约带来 **3-4 倍的 decode 提升**，主要由带宽增益（8 TB/s 对比 4.8 TB/s）叠加精度减半所主导。

实际中，真实 runtime 效率会把这一数字降到约 2.5×。但依然非常可观。

---

## 6. NVLink 5 —— 互联结构

### 6.1 单 GPU 带宽

* **NVLink 5：** 每条链路 100 GB/s，每颗 GPU 18 条链路 → 每颗 GPU 1.8 TB/s。
* **NVLink Switch v4：** 在 NVL72 域内全互联。

**这是 Hopper 单 GPU NVLink 带宽的 2 倍**。对于 **all-to-all 通信**（MoE EP 中的主要开销），这直接转化为**快 2 倍的专家分发**。


<details>
<summary>English original</summary>

**3.2 Why GB200 matters**

The GB200 superchip is the **building block of NVL72**. One NVL72 rack has 36 GB200 superchips = 72 Blackwell GPUs + 36 Grace CPUs, all in one NVLink domain.

For 671B MoE serving at scale, **NVL72 is the practical target**. Single-server (8× B200) deployments work for **smaller MoEs and lower scales**.

---

**4. GB200 NVL72 — the production MoE target**

```text
NVL72 Rack:
  - 36× GB200 superchips
  - 72× Blackwell GPUs (B200 × 36 boards × 2 GPUs)
  - 36× Grace CPUs
  - 13.5 TB aggregate HBM3e (72 × 192 GB)
  - 17 TB aggregate Grace LPDDR5x
  - NVLink Switch v4: full all-to-all 1.8 TB/s per GPU
  - Aggregate NVLink bandwidth: ~130 TB/s
  - Liquid cooled
  - ~120 kW power
```

For DeepSeek V3.1 671B at FP4 (350 GB):

* Fits across 4-8 GPUs (one TP+EP partition).
* The other ~64 GPUs in NVL72 host more partitions (data-parallel replicas), enabling massive concurrency.

For an inference workload:

* **Single replica:** 8-16 GPUs with TP × EP combinations.
* **Multiple replicas per NVL72:** 4-8 replicas, each serving independent traffic, sharing the NVLink fabric.

NVL72 also enables **disaggregated prefill / decode** (Lecture 04) by partitioning the 72 GPUs into a prefill pool and a decode pool, connected by the NVLink fabric for KV transfer.

**4.1 NVL72 vs HGX H100 8-GPU box**

The leap is large:

| Property | 8× H100 HGX | GB200 NVL72 |
|----------|-------------|-------------|
| GPUs in domain | 8 | 72 |
| Per-GPU HBM | 80 GB HBM3 | 192 GB HBM3e |
| Per-GPU HBM BW | 3.35 TB/s | 8.0 TB/s |
| NVLink per GPU | 900 GB/s | 1.8 TB/s |
| Total HBM | 640 GB | 13.5 TB |
| Total NVLink BW | 7.2 TB/s | 130 TB/s |

**The 9× larger NVLink domain is what makes MoE EP practical at full scale.** On 8× H100, EP=8 means each GPU holds 1/8 of experts; on NVL72, EP=64 means each GPU holds 1/64. Token routing is cheaper because fewer tokens land on each rank per step — but load balancing gets **harder**, not easier: fewer experts per rank means higher per-rank load variance, and one hot expert can dominate its rank's step time (hot-expert replication across ranks is the standard mitigation — Lecture 03).

---

**5. Transformer Engine 2 and FP4**

The most important Blackwell software feature for inference.

**5.1 What FP4 stores**

**E2M1 format:** 1 sign + 2 exponent + 1 mantissa = 4 bits. Representable values: {0, ±0.5, ±1, ±1.5, ±2, ±3, ±4, ±6}. That's 8 values plus signs.

This is a **very coarse** representation. Used alone, parity would be terrible. The trick is **microscaling**.

**5.2 Microscaling (MX-FP4)**

Each block of 32 consecutive elements shares a **shared scale factor** (typically an FP8 E8M0 — just the exponent). The actual value of element i is:

```text
value_i = fp4_value_i × 2^(shared_scale)
```

This gives FP4 **effective dynamic range similar to FP8** — the scale provides 256 possible exponents per block, the FP4 mantissa provides 8 distinct values within that range.

The format is standardized by the Open Compute Project (OCP) under the name **MX-FP4** ([opencompute.org](https://www.opencompute.org/)).

**5.3 Scale management in TE2**

Transformer Engine 2 manages scales automatically:

* **Per-tensor scales** for weights (statistical max-abs over the tensor).
* **Per-block scales** within a tensor (the microscaling factor).
* **Online recalibration** for activations during inference (similar to FP8 in TE1).

This is the FP4-equivalent of TE1's FP8 amax history. For deployment you typically don't manage scales by hand — the TE2 library does it.

**5.4 Where FP4 hurts**

FP4 weights and activations are well-studied. **FP4 KV cache is more aggressive** and typically loses 1–3 pp parity on long-context evals even with per-block scaling. **FP8 KV is the production default**; FP4 KV is for memory-pressure recovery only.

**5.5 Throughput impact**

For Qwen3-MoE 235B-A22B at FP4 on B200 vs FP8 on H200:

| Precision | Hardware | Active param weight read per token | Decode TPOT (batch=1) |
|-----------|----------|--------------------------------------|------------------------|
| FP8 | 4× H200 | 22 GB | ~7 ms |
| FP4 | 4× B200 | 11 GB | ~2 ms (bandwidth ceiling) |

Roughly **3-4× decode improvement** going FP8→FP4 on this model, dominated by the bandwidth gain (8 TB/s vs 4.8 TB/s) combined with the halved precision.

In practice, real runtime efficiency reduces this to ~2.5×. Still very large.

---

**6. NVLink 5 — the fabric**

**6.1 Per-GPU bandwidth**

* **NVLink 5:** 100 GB/s per link, 18 links per GPU → 1.8 TB/s per GPU.
* **NVLink Switch v4:** fully-connected within an NVL72 domain.

This is **2× the per-GPU NVLink bandwidth of Hopper**. For **all-to-all communication** (the dominant cost in MoE EP), this directly translates to **2× faster expert dispatching**.

</details>

### 6.2 All-to-all 原语

NCCL **没有专用的 all-to-all 集合通信**。MoE（混合专家模型）的 dispatch/combine 由点对点原语组成：

* `ncclSend` / `ncclRecv` — 点对点传输。
* 在 `ncclGroupStart()` / `ncclGroupEnd()` 内分组 — 每个 rank 向每个对端各发送一次并接收一次，NCCL 将其执行为单个融合的 all-to-all 步骤。典型的 MoE 原语。
* 可变大小的 "alltoallv" 模式 — 相同的分组 send/recv，但每个对端的大小不同，用于每个专家的 token 数不均匀时。

在 NVL72 上，一个具有 64 个 GPU、每对 1 MB 的 `alltoall` 耗时约 1 ms。对于 MoE EP，这是每 layer 的成本；在 61–94 个 layer 下，每个 token 的总 all-to-all 可能相当可观。这将在 Lecture 03 中诊断。

### 6.3 延迟

NVL72 GPU 任意一对之间单次 4 KB 传输的延迟为**亚微秒级**。对于非常小的传输（几百字节——例如单个 token 的隐藏状态发往一个专家），**延迟下限主导，而非带宽**。这就是为什么**批处理对 MoE EP 的帮助**不亚于对 dense decode（逐 token 生成阶段）的帮助。

---

## 7. 与 Hopper 相比有哪些变化

对于从 Hopper 迁移到 Blackwell 的 MoE 推理工程师：

1. **精度下限降至 FP4。** 为 FP8（Hopper 兼容）和 FP4（仅 Blackwell）制定 recipe。TRT-LLM 的 FP4 路径最为成熟。
2. **NVLink 域从 8 个 GPU 扩展到 72 个。** EP 分区可扩展到更高程度；每个 layer 的 token 布线变得更高效。
3. **每 GPU 的 HBM 容量翻倍**（跨代 80→192 → 288 GB）。能放在 8× H100 上的 MoE 专家，现在可以放在 2-4× B200/B300 上。
4. **All-to-all 带宽翻倍**（每 GPU 900 GB/s → 1.8 TB/s）。EP 扩展效率提升。
5. **解耦式 P/D 变得可行**，在单个 NVLink 域内——prefill（首字前的整段计算）和 decode GPU 之间的 KV 传输成本足够小。
6. **Grace 一致性内存** 支持 CPU 侧的专家卸载——新兴模式，尚未成为默认。

第 2 部分中的 Hopper recipe（TP + 连续批处理 + paged KV + 推测 + 前缀缓存）仍然适用。Blackwell 特有的 recipe（FP4、NVL72 规模的 EP、P/D 解耦、面向 DeepSeek 的 MLA 感知 kernel）是附加的*新增项*。

---

## Lab — 验证你的 Blackwell 栈并在 FP4 下测一个矩阵乘

目标：与第 2 部分的 Lecture 02 相同，但针对 Blackwell。

1. **打印版本** — 驱动 R580+、CUDA 13.3+（12.8 为引入版本）、cuDNN 9.x、FA4、TE 2.15+。
2. **选取一个 FFN 矩阵乘形状**，来自 DeepSeek V3.1 或 Qwen3-MoE 235B-A22B 的每专家 FFN——例如，DeepSeek 的 `(64, 7168) × (7168, 2048)`。
3. **测量**，使用 `torch.matmul`：
   * BF16 参考。
   * 通过 TE2 的 FP8（`te.Linear` 搭配 `fp8_recipe="hybrid"`）。
   * 通过 TE2 的 FP4（`te.Linear` 搭配 `fp8_recipe="fp4"`）。
4. **使用 Nsight Compute 进行 profile** — 确认 Blackwell 原生指令（`tcgen05` UMMA + 新的 FP4 操作 — 而非 Hopper 的 WGMMA）。
5. **将测得的 TFLOPs 与峰值对比**（2,250 BF16 / 4,500 FP8 / 9,000 FP4）。计算效率。

通过标准：在 FFN 形状上，观察到 FP4 相比 FP8 有 ~1.8× 的提升，FP4 相比 BF16 有 ~3-4× 的提升。

---

## 自检

1. H200 具有 4.8 TB/s HBM 和 990 BF16 TFLOPs（ridge ~206 FLOPs/byte）。B200 具有 8.0 TB/s HBM 和 2250 BF16 TFLOPs（ridge ~281 FLOPs/byte）。对于算术强度为 4 FLOPs/byte 的 FP8 decode kernel，哪个 GPU 的上限更高，高多少？
2. 对于 FP4 下的 DeepSeek V3.1（350 GB 权重），在并发 32 时，需要多少块 B200 来容纳整个模型，并为 KV + 激活值 + 调度器状态留出 50% 余量？给出计算过程。
3. 为什么 Grace 一致性内存对 MoE 特别重要，而不是对稠密模型？勾画一个使用它的工作负载。
4. Microscaling FP4 使用逐块缩放因子。为什么与逐张量缩放相比，这对 FP4 权重特别有帮助？
5. NVLink 5 相比 NVLink 4 将每 GPU 带宽翻倍。对于在 all-to-all 中花费 25% 步时间的 MoE EP=8 工作负载，在 Blackwell 与 Hopper 上步时间的预期减少量是多少（仅来自 NVLink 改进）？

---


<details>
<summary>English original</summary>

**6.2 All-to-all primitives**

NCCL has **no dedicated all-to-all collective**. The MoE dispatch/combine is composed from point-to-point primitives:

* `ncclSend` / `ncclRecv` — point-to-point transfers.
* Grouped inside `ncclGroupStart()` / `ncclGroupEnd()` — every rank posts one send and one receive per peer, which NCCL executes as a single fused all-to-all step. The canonical MoE primitive.
* The variable-size "alltoallv" pattern — same grouped send/recv with per-peer sizes, used when tokens-per-expert is uneven.

On NVL72, an `alltoall` with 64 GPUs and 1 MB per pair is ~1 ms. For MoE EP, this is the per-layer cost; with 61–94 layers, total all-to-all per token can be substantial. We'll diagnose this in Lecture 03.

**6.3 Latency**

Latency for a single 4 KB transfer between any pair of NVL72 GPUs is **sub-microsecond**. For very small transfers (a few hundred bytes — e.g. a single token's hidden state to one expert), the **latency floor dominates over bandwidth**. This is why **batching helps MoE EP** just as much as it helps dense decode.

---

**7. What changes vs Hopper**

For an MoE inference engineer moving from Hopper to Blackwell:

1. **Precision floor drops to FP4.** Plan recipes for both FP8 (Hopper-compatible) and FP4 (Blackwell-only). TRT-LLM's FP4 path is the most mature.
2. **NVLink domain grows from 8 to 72 GPUs.** EP partitioning scales to higher degrees; token routing becomes more efficient per layer.
3. **HBM capacity doubles per GPU** (80→192 → 288 GB across generations). MoE experts that fit on 8× H100 can fit on 2-4× B200/B300.
4. **All-to-all bandwidth doubles** (900 GB/s → 1.8 TB/s per GPU). EP scaling efficiency improves.
5. **Disaggregated P/D becomes practical** within a single NVLink domain — the KV transfer cost between prefill and decode GPUs is small enough.
6. **Grace coherent memory** enables CPU-side expert offload — emerging pattern, not the default yet.

The Hopper recipes from Part 2 (TP + continuous batching + paged KV + speculation + prefix cache) carry forward. The Blackwell-specific recipes (FP4, EP at NVL72 scale, P/D disaggregation, MLA-aware kernels for DeepSeek) are *additions* on top.

---

**Lab — verify your Blackwell stack and bench one matmul at FP4**

Goal: same as Lecture 02 from Part 2, but for Blackwell.

1. **Print versions** — driver R580+, CUDA 13.3+ (12.8 was the introduction), cuDNN 9.x, FA4, TE 2.15+.
2. **Pick one FFN matmul shape** from DeepSeek V3.1 or Qwen3-MoE 235B-A22B per-expert FFN — e.g., `(64, 7168) × (7168, 2048)` for DeepSeek.
3. **Measure** with `torch.matmul`:
   * BF16 reference.
   * FP8 via TE2 (`te.Linear` with `fp8_recipe="hybrid"`).
   * FP4 via TE2 (`te.Linear` with `fp8_recipe="fp4"`).
4. **Profile with Nsight Compute** — confirm Blackwell-native instructions (`tcgen05` UMMA + new FP4 ops — not Hopper's WGMMA).
5. **Compare measured TFLOPs** to peak (2,250 BF16 / 4,500 FP8 / 9,000 FP4). Compute efficiency.

Pass criterion: you see ~1.8× FP4 over FP8 and ~3-4× FP4 over BF16 on the FFN shape.

---

**Self-check**

1. The H200 has 4.8 TB/s HBM and 990 BF16 TFLOPs (ridge ~206 FLOPs/byte). The B200 has 8.0 TB/s HBM and 2250 BF16 TFLOPs (ridge ~281 FLOPs/byte). For a decode kernel with arithmetic intensity 4 FLOPs/byte at FP8, which GPU has the higher ceiling, and by how much?
2. For DeepSeek V3.1 at FP4 (350 GB weights), how many B200s are needed to hold the full model with 50% headroom for KV + activations + scheduler state at concurrency 32? Show the math.
3. Why does Grace coherent memory matter specifically for MoE rather than for dense? Sketch one workload that uses it.
4. Microscaling FP4 uses per-block scale factors. Why does this help compared to per-tensor scaling for FP4 weights specifically?
5. NVLink 5 doubles per-GPU bandwidth vs NVLink 4. For an MoE EP=8 workload that spends 25% of step time in all-to-all, what is the expected reduction in step time on Blackwell vs Hopper (just from the NVLink improvement)?

---

</details>

## 参考资料

* NVIDIA Blackwell 架构页面 — [nvidia.com/en-us/data-center/technologies/blackwell-architecture/](https://www.nvidia.com/en-us/data-center/technologies/blackwell-architecture/)
* NVIDIA GB200 NVL72 产品页面 — [nvidia.com/en-us/data-center/gb200-nvl72/](https://www.nvidia.com/en-us/data-center/gb200-nvl72/)
* Transformer Engine 文档（含 FP4）— [docs.nvidia.com/deeplearning/transformer-engine/](https://docs.nvidia.com/deeplearning/transformer-engine/)
* MX（microscaling）格式规范 — [opencompute.org](https://www.opencompute.org/)（属于 MX format OCP 标准）
* NVLink Switch 系统白皮书 — [nvidia.com/en-us/data-center/nvlink/](https://www.nvidia.com/en-us/data-center/nvlink/)
* "FP4 Quantization for LLMs" 近期论文 — 在 arXiv 检索 2024–2025 年的 "FP4 quantization"

交叉引用：

* [Phase 5 → GPU Infrastructure → Blackwell-B200-Qwen-Inference](../../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/02-Blackwell-B200-Qwen推理/README.md) — Blackwell + Qwen 课程，讲得更深入
* [Part 1 → Lecture 03 — roofline（性能上界模型）](../01-基础/Lecture-03.md) — 用于 Blackwell 的 ridge point

---

## 截至 2026-06

Blackwell SKU 已锁定：B200 SXM (192 GB)、B300 SXM (288 GB)、GB200 superchip、GB200 NVL72。NVLink 5（每 GPU 1.8 TB/s）、HBM3e（8.0 TB/s）。Vera Rubin（Blackwell 之后）落地或 B400 出货时更新。

---

## 接下来

* 下一讲：[Lecture 03 — Expert parallelism (EP) and the gating hot path](Lecture-03.md)
* 上一讲：[Lecture 01 — 现代 MoE（混合专家模型）剖析](Lecture-01.md)
* 上级：[Part 3 — Blackwell 上的 MoE](README.md)


<details>
<summary>English original</summary>

**References**

* NVIDIA Blackwell architecture page — [nvidia.com/en-us/data-center/technologies/blackwell-architecture/](https://www.nvidia.com/en-us/data-center/technologies/blackwell-architecture/)
* NVIDIA GB200 NVL72 product page — [nvidia.com/en-us/data-center/gb200-nvl72/](https://www.nvidia.com/en-us/data-center/gb200-nvl72/)
* Transformer Engine documentation (including FP4) — [docs.nvidia.com/deeplearning/transformer-engine/](https://docs.nvidia.com/deeplearning/transformer-engine/)
* MX (microscaling) format specification — [opencompute.org](https://www.opencompute.org/) (under MX format OCP standard)
* NVLink Switch system whitepapers — [nvidia.com/en-us/data-center/nvlink/](https://www.nvidia.com/en-us/data-center/nvlink/)
* "FP4 Quantization for LLMs" recent papers — search arXiv for "FP4 quantization" in 2024–2025

Cross-references:

* [Phase 5 → GPU Infrastructure → Blackwell-B200-Qwen-Inference](../../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/02-Blackwell-B200-Qwen推理/README.md) — Blackwell + Qwen courses going deeper
* [Part 1 → Lecture 03 — Roofline](../01-基础/Lecture-03.md) — for Blackwell ridge points

---

**Current as of 2026-06**

Blackwell SKUs pinned: B200 SXM (192 GB), B300 SXM (288 GB), GB200 superchip, GB200 NVL72. NVLink 5 (1.8 TB/s per GPU), HBM3e (8.0 TB/s). Refresh when Vera Rubin (post-Blackwell) lands or B400 ships.

---

**Next**

* Next: [Lecture 03 — Expert parallelism (EP) and the gating hot path](Lecture-03.md)
* Previous: [Lecture 01 — Anatomy of a modern MoE](Lecture-01.md)
* Up: [Part 3 — MoE at Blackwell](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 3 - MoE at Blackwell/Lecture-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%203%20-%20MoE%20at%20Blackwell/Lecture-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
