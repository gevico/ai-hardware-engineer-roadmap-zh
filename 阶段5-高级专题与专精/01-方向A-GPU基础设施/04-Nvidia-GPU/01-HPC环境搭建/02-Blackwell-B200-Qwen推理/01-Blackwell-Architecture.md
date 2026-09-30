---
title: 第 1 章：面向 Transformer 推理的 Blackwell B200 架构
description: 第 1 章：面向 Transformer 推理的 Blackwell B200 架构
---

# 第 1 章：面向 Transformer 推理的 Blackwell B200 架构

## 概述

Blackwell B200 是 NVIDIA 首款在单一封装内由**两颗光罩尺寸 die** 构建的数据中心 GPU，二者通过 10 TB/s 的封装内互连相连。它以 8 TB/s 承载 192 GB HBM3e，新增**原生支持 FP4/FP6 的第 5 代张量核心**，并可接入机架级 NVL72 互连架构——该架构把 72 颗 B200 GPU 和 36 颗 Grace CPU 置于同一个一致内存镜像之后。

对 Qwen 级 Transformer 推理而言，最抢眼的数字是带宽：**每 GPU 8 TB/s，是 H100 SXM5 的 2.4×、H200 的约 1.9×**。decode（逐 token 生成阶段）受带宽约束；带宽翻倍，tok/s 大致也翻倍。让 8 TB/s 真正可用的那些架构决策——双 die 扩展、die 之间的 NVLink-C2C、与新带宽相匹配的张量核心——正是本章要讲的内容。

读完本章后，你应当能够：

* 画出 B200 封装示意图，并解释双 die 如何影响软件（单卡内 TP=2）。
* 给出 Qwen 级工作负载下每 GPU 与聚合的带宽、容量和算力数字。
* 在相关推理指标上对比 B200 与 H100、H200 以及仍在出货的 L40S。
* 把 GB200 超级芯片和 NVL72 机架纳入同一个心智模型。

---

## 1. 封装：两颗 die，一颗 GPU

```
       ┌────────────────────────────────────────┐
       │                  B200                  │
       │   ┌────────────┐ NVLink-C2C ┌────────────┐│
       │   │   Die 0    │◄══════════►│   Die 1    ││
       │   │ (104 B txn)│  10 TB/s   │ (104 B txn)││
       │   │  4×HBM3e   │            │  4×HBM3e   ││
       │   │  96 GB     │            │  96 GB     ││
       │   │  4 TB/s    │            │  4 TB/s    ││
       │   └────────────┘            └────────────┘│
       │                                          │
       │   208 B transistors total                │
       │   192 GB HBM3e total                     │
       │   ~8 TB/s aggregate HBM bandwidth        │
       │   NVLink 5 external: 1.8 TB/s            │
       └────────────────────────────────────────┘
```

两颗光罩极限尺寸的 die（各约 800 mm²）共同封装在 CoWoS-L 基板上。每颗 die 都有自己的 SM 阵列、自己的 L2 缓存，以及四个 HBM3e 堆栈（各 24 GB、各 1 TB/s → 每颗 die 4 TB/s）。

两颗 die 通过 **NVLink-C2C** 相连——这是一条宽位、低延迟的封装内链路，可提供 **10 TB/s 双向带宽**。作为对比：H100 SXM 两张卡之间的 NVLink 4 为 900 GB/s。因此在一颗 B200 内部，die 间带宽是单条 H100 到 H100 NVLink 的 10 倍以上。两颗 die 对 CUDA 呈现为**一颗 GPU**，但感知到这种分区方式的软件可以做到 H100 那一代做不到的事——在**单颗 GPU 内部跨两颗 die 运行张量并行推理**。

### 1.1 为什么这对 Qwen 推理重要

对于 FP4 的 Qwen2.5-72B-Instruct（权重约 36 GB），模型可以轻松放进**一颗** die。KV cache、激活值和 CUDA context 还要再占 10–20 GB。你可以把整个模型跑在一颗 die 上，让第二颗 die 空出来用于以下任一用途：

* 第二个并发模型实例（推理服务吞吐翻倍，代价是高负载下每请求延迟上升）。
* 跨两颗 die 对同一模型做 TP=2，decode 带宽翻倍、单流延迟缩短。
* 供投机解码使用的草稿模型（Qwen3-8B 为 Qwen2.5-72B 目标模型做草稿）。

这是一个在 H100/H200 上根本不存在的区间——在那些卡上，FP4/FP8 的 70B 级模型能放进一张卡，但你无法再把这张卡细分。在 B200 上，双 die 架构是一个软件可见的自由度。

---

## 2. HBM3e：带宽的故事

| 规格 | B200 | H200 | H100 SXM |
|---|---|---|---|
| HBM 容量 | 192 GB | 141 GB | 80 GB |
| HBM 带宽 | ~8 TB/s | ~4.8 TB/s | ~3.35 TB/s |
| L2 缓存总量 | ~120 MB | ~50 MB | ~50 MB |
| SM 数量 | ~160（两颗 die 合计） | ~132 | 132 |

对于受 decode 约束的 LLM 推理，相关的比较是**带宽 × 数据类型效率**：

| 配置 | 有效字节/token（Qwen2.5-72B） | 理论 tok/s（单 GPU） |
|---|---|---|
| H100 SXM，FP8 权重，FP16 KV | ~75 GB | ~45 |
| H200，FP8 权重，FP16 KV | ~75 GB | ~64 |
| H100 SXM，FP16 权重 | ~145 GB | ~23 |
| H200，FP16 权重 | ~145 GB | ~33 |
| **B200，FP4 权重，FP8 KV** | **~36 GB** | **~220** |
| B200，FP8 权重，FP16 KV | ~75 GB | ~106 |
| B200，FP16 权重 | ~145 GB | ~55 |

带宽的跃升与 FP4 能力合在一起，产出的单流 decode 速率约为 H100 的 **5–6×**，而**单 GPU 占用保持不变**。正是这个部署经济性的拐点，让 Blackwell 在大规模服务 70B 级对话模型时值得关注。

---


<details>
<summary>English original</summary>

**Chapter 1: Blackwell B200 Architecture for Transformer Inference**

**Overview**

Blackwell B200 is the first NVIDIA datacenter GPU built from **two reticle-sized dies** on a single package, linked by a 10 TB/s on-package interconnect. It carries 192 GB of HBM3e at 8 TB/s, adds **5th-generation tensor cores with native FP4/FP6** support, and slots into a rack-scale NVL72 fabric that puts 72 B200 GPUs and 36 Grace CPUs behind one coherent memory image.

For Qwen-class transformer inference the headline number is bandwidth: **8 TB/s per GPU is 2.4× H100 SXM5 and ~1.9× H200**. Decode is bandwidth-bound; the bandwidth doubles; tok/s roughly doubles. The architectural decisions that make 8 TB/s usable — dual-die scaling, NVLink-C2C between dies, tensor cores that match the new bandwidth — are what this chapter is about.

By the end you should be able to:

* Sketch the B200 package and explain how dual-die affects software (TP=2 inside one card).
* Quote per-GPU and aggregate bandwidth, capacity, and compute numbers for Qwen-class workloads.
* Compare B200 against H100, H200, and the still-shipping L40S on relevant inference metrics.
* Place GB200 superchip and NVL72 rack in the same mental model.

---

**1. The Package: Two Dies, One GPU**

```
       ┌────────────────────────────────────────┐
       │                  B200                  │
       │   ┌────────────┐ NVLink-C2C ┌────────────┐│
       │   │   Die 0    │◄══════════►│   Die 1    ││
       │   │ (104 B txn)│  10 TB/s   │ (104 B txn)││
       │   │  4×HBM3e   │            │  4×HBM3e   ││
       │   │  96 GB     │            │  96 GB     ││
       │   │  4 TB/s    │            │  4 TB/s    ││
       │   └────────────┘            └────────────┘│
       │                                          │
       │   208 B transistors total                │
       │   192 GB HBM3e total                     │
       │   ~8 TB/s aggregate HBM bandwidth        │
       │   NVLink 5 external: 1.8 TB/s            │
       └────────────────────────────────────────┘
```

Two reticle-limit dies (~800 mm² each) are co-packaged on a CoWoS-L substrate. Each die has its own SM array, its own L2 cache, and four HBM3e stacks (24 GB each, 1 TB/s each → 4 TB/s per die).

The two dies are connected by **NVLink-C2C** — a wide, low-latency, on-package link that delivers **10 TB/s bidirectional**. For comparison: H100 SXM's NVLink 4 between two cards is 900 GB/s. So inside one B200, the die-to-die bandwidth is more than 10× a single H100-to-H100 NVLink. The two dies present themselves to CUDA as **one GPU**, but software that's aware of the partitioning can do something the H100 generation could not — run **tensor-parallel inference across two dies inside a single GPU**.

**1.1 Why this matters for Qwen inference**

For Qwen2.5-72B-Instruct at FP4 (~36 GB weights), the model fits comfortably on **one** die. The KV cache, activations, and CUDA context take another 10–20 GB. You can run the whole model on one die and keep the second die free for either:

* A second concurrent model instance (doubles serving throughput at the cost of higher per-request latency under load).
* TP=2 of the same model across the two dies, doubling decode bandwidth and shrinking per-stream latency.
* A draft model for speculative decoding (Qwen3-8B drafting Qwen2.5-72B target).

This is a regime that simply doesn't exist on H100/H200 — there, a 70B-class model in FP4/FP8 fits on one card, but you can't subdivide the card further. On B200 the dual-die architecture is a software-visible degree of freedom.

---

**2. HBM3e: The Bandwidth Story**

| Spec | B200 | H200 | H100 SXM |
|---|---|---|---|
| HBM capacity | 192 GB | 141 GB | 80 GB |
| HBM bandwidth | ~8 TB/s | ~4.8 TB/s | ~3.35 TB/s |
| L2 cache total | ~120 MB | ~50 MB | ~50 MB |
| SM count | ~160 (across both dies) | ~132 | 132 |

For decode-bound LLM inference, the relevant comparison is **bandwidth × dtype efficiency**:

| Configuration | Effective bytes/token (Qwen2.5-72B) | Theoretical tok/s (one GPU) |
|---|---|---|
| H100 SXM, FP8 weights, FP16 KV | ~75 GB | ~45 |
| H200, FP8 weights, FP16 KV | ~75 GB | ~64 |
| H100 SXM, FP16 weights | ~145 GB | ~23 |
| H200, FP16 weights | ~145 GB | ~33 |
| **B200, FP4 weights, FP8 KV** | **~36 GB** | **~220** |
| B200, FP8 weights, FP16 KV | ~75 GB | ~106 |
| B200, FP16 weights | ~145 GB | ~55 |

The bandwidth jump and the FP4 capability together produce roughly **5–6×** the single-stream decode rate of an H100, with **the same single-GPU footprint**. This is the deployment-economics inflection point that makes Blackwell interesting for serving 70B-class chat at scale.

---

</details>

## 3. 第五代 Tensor Core

自 Volta 以来，Tensor Core 一直是 NVIDIA GPU 上占主导地位的矩阵乘加速器。Blackwell 的第五代迭代新增了：

* **原生 FP4**（E2M1，带共享 block scale）— 每元素 4 bits。
* **原生 FP6**（E2M3 / E3M2）— 6 bits。
* **MX（microscaling）格式** — 子块 scale（每 32 个元素为一个块），共享指数 — 即 OCP-MX 标准。
* **改进的异步 warp-group MMA（WGMMA）** — 对 Hopper 时代的 WGMMA 做了扩展：tile 尺寸更好，FP4/FP6 路径上的寄存器压力更低。

对 Qwen 推理而言，最重要的数字是 **FP4 下的吞吐**：单个 B200 die 可提供约 2.25 PFLOPS 稠密 FP4，约 4.5 PFLOPS（结构化稀疏）。在相同的物理面积预算下，这大约是 H100 FP16 稠密吞吐的 8 倍。

对 decode（逐 token 生成阶段）来说，算力很少是瓶颈（仍然受带宽限制），但它主导了 **prefill（首字前的整段计算）** 与**高并发下的批处理 decode**。Qwen2.5-72B 上一次 4 k token 的 prefill，在 H100 上约需 250 ms，而在使用 FP4 权重与 FP8 激活值的 B200 上约需 70–90 ms。

### 3.1 在 kernel 层面，「第五代」究竟意味着什么

* WGMMA 指令仍然接收由 4 个 warp（128 个线程）组成的 warp group，但采用了更契合 FP4 的新 tile 形状：FP4 为 `64×16×64`，FP16 为 `64×16×16`。
* Transformer Engine 2 按块自动管理 scale factor，对 CUDA kernel 透明。
* `tcgen05.mma` 指令（Blackwell 专属）为 FP4/FP6 带来寄存器压力更低的更快路径 — 但需要 CUTLASS 4 / cuBLAS 13 才能暴露出来。
* 稀疏支持现在有 2:4 *和* 4:8 结构化稀疏。

作为 LLM 推理工程师，你很少手写 WGMMA。生产 runtime（TRT-LLM、vLLM、SGLang）通过 Triton-MLIR、CUTLASS 模板或手工调优的 kernel 编译到这些指令 — 但在阅读 kernel trace 时你应当知道它们的存在。

---

## 4. NVLink-C2C 与「GPU 即两个 GPU」模式

在 B200 内部，两个 die 通过 NVLink-C2C 相连。为保持兼容，CUDA 把它们暴露为一个设备，但感知 Blackwell 的代码路径可以：

* 通过 NUMA 风格的布局把张量固定到指定 die（Die-0 HBM 或 Die-1 HBM）。
* 以 NVLink-C2C 作为互联结构，**跨两个 die** 做张量并行集合通信 — 10 TB/s 的「免费」封装内带宽，不触及外部 NVLink。

对于单个 B200 内的 Qwen2.5-72B TP=2：

```
Per layer collectives:
  AllReduce post-attention:  d_model floats × 2 bytes
  AllReduce post-FFN:        d_model floats × 2 bytes
  = ~32 KB per layer × 80 layers = 2.56 MB / token

NVLink-C2C bandwidth: 10 TB/s
Roundtrip latency: ~150 ns
Per-token NVLink time: ~0.0003 ms (negligible)
```

换句话说：**封装内 TP 基本上是免费的**。集合通信时间小到足以让你获得 TP=2 的带宽翻倍效果，而没有通常的 NCCL/NVLink 惩罚。这是 Blackwell 独有的。

---

## 5. NVLink 5 与封装外互联

对于多 GPU 部署，B200 支持 **NVLink 5**，每 GPU 双向 **1.8 TB/s**（跨全部 18 个 NVLink-5 端口）。这是 H100 SXM 每 GPU NVLink 带宽的 2 倍。

在 HGX B200 8-GPU 板卡（H100 SXM 的直接继任者）中：

```
8 × B200 each with NVLink-5 → NVSwitch
NVSwitch fabric: 130 TB/s aggregate (full bisection)
Total HBM: 8 × 192 GB = 1.54 TB
Total HBM bandwidth: 8 × 8 TB/s = 64 TB/s
```

当同时需要**高批吞吐**与**高单流延迟**时，这是 Qwen2.5-72B 级别推理服务的自然部署方式。采用 TP=8 时，把模型切分到 8 个 B200 上；每个持有约 1/8 的权重与 KV。聚合后的 decode 带宽可在长上下文下支撑数万条并发流。

---


<details>
<summary>English original</summary>

**3. 5th-Generation Tensor Cores**

Tensor cores have been the dominant matmul accelerator on NVIDIA GPUs since Volta. Blackwell's 5th-gen iteration adds:

* **Native FP4** (E2M1 with shared block scale) — 4 bits per element.
* **Native FP6** (E2M3 / E3M2) — 6 bits.
* **MX (microscaling) formats** — sub-block scales (per-32-element block), shared exponent — the OCP-MX standard.
* **Refined async warp-group MMA (WGMMA)** — Hopper-era WGMMA is extended with better tile sizes, lower register pressure on the FP4/FP6 paths.

For Qwen inference, the most important number is **throughput at FP4**: a B200 die delivers ~2.25 PFLOPS dense FP4, ~4.5 PFLOPS with structured sparsity. That's roughly 8× H100 FP16 dense throughput, on the same physical area budget.

The compute is rarely the bottleneck for decode (still bandwidth-bound), but it dominates **prefill** and **batched-decode at high concurrency**. A prefill of 4 k tokens on Qwen2.5-72B that takes ~250 ms on H100 takes ~70–90 ms on B200 with FP4 weights and FP8 activations.

**3.1 What "5th-gen" actually means at the kernel level**

* WGMMA instructions still take warp groups of 4 warps (128 threads) but with new tile shapes that match FP4 better: `64×16×64` for FP4 vs `64×16×16` for FP16.
* The Transformer Engine 2 manages scale factors per block automatically, transparently to the CUDA kernel.
* The `tcgen05.mma` instruction (Blackwell-only) brings a faster path with reduced register pressure for FP4/FP6 — but requires CUTLASS 4 / cuBLAS 13 to be exposed.
* Sparsity support is now 2:4 *and* 4:8 structured.

You as an LLM inference engineer rarely write WGMMA by hand. Production runtimes (TRT-LLM, vLLM, SGLang) compile down to these instructions via Triton-MLIR, CUTLASS templates, or hand-tuned kernels — but you should know they exist when reading kernel traces.

---

**4. NVLink-C2C and the "GPU as Two GPUs" Pattern**

Inside a B200, the two dies are NVLink-C2C-connected. CUDA exposes them as one device for compatibility, but Blackwell-aware code paths can:

* Pin tensors to a specific die (Die-0 HBM or Die-1 HBM) via NUMA-style placement.
* Do tensor-parallel collectives **across the two dies** using NVLink-C2C as the fabric — 10 TB/s of "free" intra-package bandwidth that doesn't touch the external NVLink.

For Qwen2.5-72B TP=2 inside one B200:

```
Per layer collectives:
  AllReduce post-attention:  d_model floats × 2 bytes
  AllReduce post-FFN:        d_model floats × 2 bytes
  = ~32 KB per layer × 80 layers = 2.56 MB / token

NVLink-C2C bandwidth: 10 TB/s
Roundtrip latency: ~150 ns
Per-token NVLink time: ~0.0003 ms (negligible)
```

In other words: **intra-package TP is essentially free**. The collective time is so small that you get the bandwidth doubling effect of TP=2 without the usual NCCL/NVLink penalty. This is unique to Blackwell.

---

**5. NVLink 5 and the Off-Package Fabric**

For multi-GPU deployments, B200 supports **NVLink 5** at **1.8 TB/s per GPU bidirectional** (across all 18 NVLink-5 ports). That's 2× the per-GPU NVLink bandwidth of H100 SXM.

In an HGX B200 8-GPU board (the direct H100 SXM successor):

```
8 × B200 each with NVLink-5 → NVSwitch
NVSwitch fabric: 130 TB/s aggregate (full bisection)
Total HBM: 8 × 192 GB = 1.54 TB
Total HBM bandwidth: 8 × 8 TB/s = 64 TB/s
```

This is the natural deployment for Qwen2.5-72B-class serving when you want both **high batch throughput** and **high single-stream latency**. With TP=8 you partition the model across 8 B200s; each holds ~1/8th of weights and KV. Aggregate decode bandwidth supports tens of thousands of concurrent streams at long context.

---

</details>

## 6. GB200 Superchip — 一致性 Grace 内存

在单颗 B200 之上是 **GB200 superchip**：两颗 B200 GPU 加一颗 **Grace** ARM CPU，位于同一 NVLink-C2C fabric 上，二者之间具备**一致性内存**。

```
┌──────────────────────────────────────────────────────────┐
│                       GB200 Superchip                    │
│  ┌─────────────┐   NVLink-C2C   ┌────────────────────┐   │
│  │   Grace     │◄══════════════►│   B200 (Die 0+1)   │   │
│  │ 72-core ARM │    900 GB/s    │  192 GB HBM3e      │   │
│  │ 480 GB LPDDR│    coherent    │                    │   │
│  └─────────────┘                └────────────────────┘   │
│  ┌─────────────┐                ┌────────────────────┐   │
│  │   Grace     │◄══════════════►│   B200 (Die 0+1)   │   │
│  │ 72-core ARM │                │  192 GB HBM3e      │   │
│  │ 480 GB LPDDR│                │                    │   │
│  └─────────────┘                └────────────────────┘   │
│                                                          │
│  Aggregate: 384 GB HBM3e + 960 GB LPDDR5X = 1.34 TB     │
│             coherent across GPUs and CPUs                │
└──────────────────────────────────────────────────────────┘
```

对于大语言模型推理服务，关键特性是 **Grace LPDDR 内存能以内存一致的速度被 GPU 寻址**（每颗 Grace 约 900 GB/s）。这实际提供了一个分级内存系统：

* 热层：HBM3e（8 TB/s，每颗 GPU 192 GB）
* 温层：Grace LPDDR（900 GB/s，每颗 Grace 480 GB）

温层在推理中的用例：

* 为超出 HBM 预算的长上下文序列溢出 KV cache。
* 容纳大型 MoE（混合专家模型）专家池，仅将激活的专家放在 HBM。
* 跨大量请求缓存 prefill（首字前的整段计算）前缀 / 系统提示词。

---

## 7. NVL72 — 机架即计算机

Blackwell 最大的单元是 **NVL72** 机架：**72 颗 B200 GPU + 36 颗 Grace CPU** 位于单个液冷机箱，完全通过 NVSwitch fabric 连接。

```
NVL72 Rack:
  - 36 GB200 superchips
    = 72 × B200 + 36 × Grace
  - 18 compute trays × 2 GB200 per tray
  - 9 NVSwitch trays
  - Liquid cooling, ~120 kW per rack
  - Total HBM: 72 × 192 GB = 13.8 TB HBM3e
  - Total Grace LPDDR: 36 × 480 GB = 17.3 TB
  - Total coherent memory: ~30 TB
  - Aggregate HBM bandwidth: 72 × 8 TB/s = 576 TB/s
  - NVLink-5 fabric: 130 TB/s bisection
```

该平台面向两种模式：

1. **大规模推理服务** — Qwen2.5-72B 以 TP=8 在整机架复制 9 份，批大小 >> 1000，针对 tokens/sec/$ 和 tokens/sec/W 优化。
2. **单模型巨兽** — 前沿 500B+/1T 模型以 TP=72 运行，权重根本无法放入更少的 GPU。

具体到 Qwen 产品线，NVL72 对 Qwen2.5-72B（可装入单颗 B200）而言是过度配置，但对假想中的 Qwen3-300B+ 级模型、Qwen-Max 级稠密模型或大型 Qwen-MoE 部署**恰到好处**。

---

## 8. 对比表 — B200 何时胜出？

| 工作负载 | H100 SXM | H200 | B200 | 胜出者 |
|---|---|---|---|---|
| 单流 Qwen2.5-72B FP16 decode（逐 token 生成阶段） | ~22 tok/s（TP=4） | ~33 tok/s（TP=4） | ~55 tok/s（TP=2） | B200 |
| 单流 Qwen2.5-72B FP4 decode | n/a（无 FP4） | n/a | ~220 tok/s（单颗 GPU） | **B200** |
| 批处理推理服务，B=64，Qwen2.5-72B | ~3,500 tok/s 合计（8×H100） | ~4,800 tok/s（8×H200） | ~10,000 tok/s（8×B200） | B200 |
| Prefill 32k Qwen2.5-72B | ~2.5 s（TP=4） | ~1.8 s | ~0.6 s（单颗 GPU FP4） | B200 |
| Qwen2.5-72B 单 GPU 装入 | 否 | 勉强（FP4 模拟） | 是（FP4 原生） | B200 |
| Qwen3-4B 边缘 | n/a（过度） | n/a | n/a | 用 Jetson |
| 长上下文 131k decode | KV 提前溢出 | KV 可容纳且有余量 | KV 轻松容纳 | B200 |
| 高负载下每 token 成本 | 基线 | ~0.75× | ~0.35× | B200 |

诚实的总结：**对每一个能在生产中利用 FP4 的 Qwen2.5-72B 级工作负载，B200 都胜出。** 唯一不占优的情况是仅做微调或处于 FP4 验证之前的流水线——此时仍在做 FP16——在那里，带宽跃升仍有帮助，但倍数更小（2× 而非 5×）。

---

## 9. 在 `nvidia-smi` 中解读 B200

实操指引：

```bash
$ nvidia-smi --query-gpu=name,memory.total,memory.used,utilization.gpu,power.draw \
             --format=csv,noheader

NVIDIA B200, 196608 MiB, 38912 MiB, 87 %, 925.43 W
```

* **196 608 MiB ≈ 192 GB** — 确认是一颗 B200。
* 拓扑输出（`nvidia-smi topo -m`）显示卡间 NVLink-5 连接；在 HGX B200 上，每对 GPU 之间应看到 `NV18`（18 链路 NVLink 5）。
* 一颗 B200 内部的两个 die 默认在 `nvidia-smi` 中无法分别寻址；`nvbandwidth` 和 CUDA Multi-Instance GPU API 等工具可直接探测 C2C 链路。

---


<details>
<summary>English original</summary>

**6. GB200 Superchip — Coherent Grace Memory**

Above the single B200 sits the **GB200 superchip**: two B200 GPUs plus one **Grace** ARM CPU on the same NVLink-C2C fabric, with **coherent memory** between them.

```
┌──────────────────────────────────────────────────────────┐
│                       GB200 Superchip                    │
│  ┌─────────────┐   NVLink-C2C   ┌────────────────────┐   │
│  │   Grace     │◄══════════════►│   B200 (Die 0+1)   │   │
│  │ 72-core ARM │    900 GB/s    │  192 GB HBM3e      │   │
│  │ 480 GB LPDDR│    coherent    │                    │   │
│  └─────────────┘                └────────────────────┘   │
│  ┌─────────────┐                ┌────────────────────┐   │
│  │   Grace     │◄══════════════►│   B200 (Die 0+1)   │   │
│  │ 72-core ARM │                │  192 GB HBM3e      │   │
│  │ 480 GB LPDDR│                │                    │   │
│  └─────────────┘                └────────────────────┘   │
│                                                          │
│  Aggregate: 384 GB HBM3e + 960 GB LPDDR5X = 1.34 TB     │
│             coherent across GPUs and CPUs                │
└──────────────────────────────────────────────────────────┘
```

For LLM serving, the relevant property is that **Grace LPDDR memory is GPU-addressable at memory-coherent speeds** (~900 GB/s per Grace). This effectively gives you a tiered memory system:

* Hot tier: HBM3e (8 TB/s, 192 GB per GPU)
* Warm tier: Grace LPDDR (900 GB/s, 480 GB per Grace)

Use cases for the warm tier in inference:

* Spill KV cache for long-context sequences that exceed HBM budget.
* Hold a large MoE expert pool with only the active experts in HBM.
* Cache prefill prefixes / system prompts across many requests.

---

**7. NVL72 — The Rack as a Computer**

The largest Blackwell unit is the **NVL72** rack: **72 B200 GPUs + 36 Grace CPUs** in a single liquid-cooled chassis, fully NVSwitch-fabric-connected.

```
NVL72 Rack:
  - 36 GB200 superchips
    = 72 × B200 + 36 × Grace
  - 18 compute trays × 2 GB200 per tray
  - 9 NVSwitch trays
  - Liquid cooling, ~120 kW per rack
  - Total HBM: 72 × 192 GB = 13.8 TB HBM3e
  - Total Grace LPDDR: 36 × 480 GB = 17.3 TB
  - Total coherent memory: ~30 TB
  - Aggregate HBM bandwidth: 72 × 8 TB/s = 576 TB/s
  - NVLink-5 fabric: 130 TB/s bisection
```

This is the platform for two regimes:

1. **Massive serving** — Qwen2.5-72B at TP=8 replicated 9× across the rack, batch >> 1000, optimized for tokens/sec/$ and tokens/sec/W.
2. **Single-model giants** — frontier 500B+/1T models at TP=72, where you simply can't fit the weights on fewer GPUs.

For the Qwen lineup specifically, NVL72 is overkill for Qwen2.5-72B (which fits on a single B200) but **exactly right** for hypothetical Qwen3-300B+ class models, Qwen-Max-class dense models, or large Qwen-MoE deployments.

---

**8. Comparison Table — When Does B200 Win?**

| Workload | H100 SXM | H200 | B200 | Winner |
|---|---|---|---|---|
| Single-stream Qwen2.5-72B FP16 decode | ~22 tok/s (TP=4) | ~33 tok/s (TP=4) | ~55 tok/s (TP=2) | B200 |
| Single-stream Qwen2.5-72B FP4 decode | n/a (no FP4) | n/a | ~220 tok/s (one GPU) | **B200** |
| Batched serving, B=64, Qwen2.5-72B | ~3,500 tok/s aggregate (8×H100) | ~4,800 tok/s (8×H200) | ~10,000 tok/s (8×B200) | B200 |
| Prefill 32k Qwen2.5-72B | ~2.5 s (TP=4) | ~1.8 s | ~0.6 s (one GPU FP4) | B200 |
| Qwen2.5-72B single-GPU fit | no | barely (FP4 emulated) | yes (FP4 native) | B200 |
| Qwen3-4B edge | n/a (overkill) | n/a | n/a | use Jetson |
| Long-context 131k decode | KV spills early | KV fits with room | KV fits easily | B200 |
| Per-token cost at high load | baseline | ~0.75× | ~0.35× | B200 |

The honest summary: **B200 wins on every Qwen2.5-72B-class workload that can use FP4 in production.** The one case where it doesn't dominate is fine-tuning-only or pre-FP4-validation pipelines where you're still doing FP16 — there, the bandwidth jump still helps but the multiple is smaller (2× not 5×).

---

**9. Reading a B200 in `nvidia-smi`**

Practical orientation:

```bash
$ nvidia-smi --query-gpu=name,memory.total,memory.used,utilization.gpu,power.draw \
             --format=csv,noheader

NVIDIA B200, 196608 MiB, 38912 MiB, 87 %, 925.43 W
```

* **196 608 MiB ≈ 192 GB** — confirms one B200.
* Topology output (`nvidia-smi topo -m`) shows NVLink-5 connectivity between cards; on an HGX B200 you should see `NV18` (18-link NVLink 5) between every GPU pair.
* The two dies inside one B200 are not separately addressable by default in `nvidia-smi`; tools like `nvbandwidth` and CUDA Multi-Instance GPU APIs can probe the C2C link directly.

---

</details>

## 核心要点

| 要点 | 为何重要 |
|---|---|
| B200 是单个封装上的两颗 die，由 10 TB/s 的 NVLink-C2C 互连 | 使封装内 TP=2 成为可能——这一模式在 H100/H200 上并不存在 |
| 192 GB HBM3e，带宽 8 TB/s | 是 H100 带宽的 2.4 倍；decode 的 tok/s 大致与之成正比 |
| 原生 FP4 Tensor Core | 70B 级别的 Qwen 可在单 GPU 内放下，权重约 36 GB |
| NVLink-C2C 集合通信时间可忽略（约 0.0003 ms/token） | 封装内 TP 实际上免费 |
| NVL72 机架 = 13.8 TB HBM + 30 TB 一致内存 | 使 Qwen-300B+ 级别的 TP=72 部署成为可能 |
| GB200 超级芯片将 Grace LPDDR 暴露为一致的热层 | 长上下文 KV、MoE 专家池、前缀缓存都可干净地溢出到此处 |
| 生产 runtime 需要 Blackwell 支持（CUDA 13、cuBLAS 13、TRT-LLM 0.20+） | 旧软件栈要么模拟 FP4，要么回退到 FP8 路径 |

---

## 资源

* **[NVIDIA Blackwell Architecture Whitepaper](https://resources.nvidia.com/en-us-blackwell-architecture)：** 官方架构深度解读。
* **[GB200 NVL72 Product Page](https://www.nvidia.com/en-us/data-center/gb200-nvl72/)：** 机架级平台规格。
* **[HGX B200 Platform Brief](https://www.nvidia.com/en-us/data-center/hgx/)：** 面向非机架部署的 8-GPU 板卡规格。
* **[OCP Microscaling Formats Specification (MX-FP4/FP6/FP8)](https://www.opencompute.org/documents/ocp-microscaling-formats-mx-v1-0-spec-final-pdf)：** 新数值格式背后的标准。
* **[NVLink-C2C Programming Guide](https://docs.nvidia.com/cuda/cuda-c-programming-guide/)：** 针对 Blackwell die-aware 编程做了更新。
* **[第 2 章 — FP4 数值与 Transformer Engine 2](02-FP4-Numerics-Transformer-Engine.md)：** 本系列的下一章。


<details>
<summary>English original</summary>

**Key Takeaways**

| Takeaway | Why it matters |
|---|---|
| B200 is two dies on one package, linked by 10 TB/s NVLink-C2C | Enables intra-package TP=2 — a regime that doesn't exist on H100/H200 |
| 192 GB HBM3e at 8 TB/s | 2.4× H100 bandwidth; decode tok/s roughly tracks this |
| Native FP4 tensor cores | A 70B-class Qwen fits in one GPU at ~36 GB weights |
| NVLink-C2C collective time is negligible (~0.0003 ms/token) | Intra-package TP is effectively free |
| NVL72 rack = 13.8 TB HBM + 30 TB coherent memory | Enables Qwen-300B+ class TP=72 deployments |
| GB200 superchip exposes Grace LPDDR as coherent warm tier | Long-context KV, MoE expert pools, prefix caches can spill cleanly |
| Production runtimes need Blackwell support (CUDA 13, cuBLAS 13, TRT-LLM 0.20+) | Older stacks emulate FP4 or fall back to FP8 paths |

---

**Resources**

* **[NVIDIA Blackwell Architecture Whitepaper](https://resources.nvidia.com/en-us-blackwell-architecture):** Official architecture deep dive.
* **[GB200 NVL72 Product Page](https://www.nvidia.com/en-us/data-center/gb200-nvl72/):** Rack-scale platform specs.
* **[HGX B200 Platform Brief](https://www.nvidia.com/en-us/data-center/hgx/):** 8-GPU board spec for non-rack deployments.
* **[OCP Microscaling Formats Specification (MX-FP4/FP6/FP8)](https://www.opencompute.org/documents/ocp-microscaling-formats-mx-v1-0-spec-final-pdf):** The standard behind the new numeric formats.
* **[NVLink-C2C Programming Guide](https://docs.nvidia.com/cuda/cuda-c-programming-guide/):** Updated for Blackwell die-aware programming.
* **[Chapter 2 — FP4 Numerics & Transformer Engine 2](02-FP4-Numerics-Transformer-Engine.md):** The next chapter in this series.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Blackwell-B200-Qwen-Inference/01-Blackwell-Architecture.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Blackwell-B200-Qwen-Inference/01-Blackwell-Architecture.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
