---
title: 面向 Qwen Transformer 推理的 Blackwell B200
description: 面向 Qwen Transformer 推理的 Blackwell B200
---

# 面向 Qwen Transformer 推理的 Blackwell B200

共 6 章的专题课程，讲解如何在 NVIDIA **Blackwell B200** 一代硬件上运行 Qwen 级 Transformer 模型：单张 B200、GB200 超级芯片，以及 NVL72 机架级系统。只涉及推理——不涉及训练，不涉及微调。

本系列是边缘 AI 中 [Qwen 推理优化系列](../../../../03-方向C-边缘AI/05-Qwen推理优化/README.md) 的 Blackwell 对应版本。该系列覆盖了 Jetson 上的 Qwen3-4B 和 4×H100 上的 Qwen2.5-72B。本系列从 H100 止步之处继续：**当你把 Qwen2.5-72B（以及更大模型）迁到 Blackwell 时会发生哪些变化。**

简而言之：单张 B200 以 8 TB/s 提供 192 GB HBM3e，并支持 FP4 张量核心，因此 Qwen2.5-72B 级模型能放进**一张** GPU，其带宽受限的 decode（逐 token 生成阶段）吞吐需要 4–8 张 H100 才能匹敌。NVL72 机架用单一一致内存映像把同一模型扩展到极端批大小 + 长上下文。软件必须跟上——Transformer Engine 2、FP4 微缩放、第 5 代张量核心上的 FlashAttention-3、TMA 增强、persistent kernel。

| 章节 | 标题 | 重点 |
|---|---|---|
| 01 | [Blackwell 架构](01-Blackwell-Architecture.md) | 双 die 封装、NVLink-C2C、HBM3e、第 5 代张量核心、NVL72 |
| 02 | [FP4 数值与 Transformer Engine 2](02-FP4-Numerics-Transformer-Engine.md) | MX-FP4/FP6/FP8 微缩放、动态逐块量化、质量与带宽的权衡 |
| 03 | [单张 B200 上的 Qwen](03-Single-B200-Qwen-Inference.md) | Qwen2.5-72B 放进单个 die、布局、通过 NVLink-C2C 做 per-die TP |
| 04 | [多 B200 与 NVL72](04-Multi-B200-NVL72.md) | GB200 超级芯片、NVL72 架构、TP=16/72、NVLink-5 上的 NCCL |
| 05 | [Blackwell Kernel 工程](05-Blackwell-Kernel-Engineering.md) | 第 5 代 WGMMA、TMA-2、异步 warp 特化、FlashAttention-3、CUTLASS 4 |
| 06 | [Blackwell 上的生产级推理服务](06-Production-Serving-on-Blackwell.md) | TRT-LLM 0.20+、vLLM Blackwell 后端、生产环境中的 FP4、benchmark、成本 |

## 前置要求

* [阶段 5 — 边缘 LLM 推理内部机制](../../../../03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md) — GEMV（矩阵-向量乘）与 GEMM（矩阵-矩阵乘）的 roofline（性能上界模型）计算。
* [阶段 5 — Qwen 推理优化（完整系列）](../../../../03-方向C-边缘AI/05-Qwen推理优化/README.md) — 尤其是 Lecture 04（H100 上的 Qwen2.5-72B 多 GPU FP16）。
* [阶段 5 — NCCL 深入剖析](../08-NCCL深度探索/README.md) — 多 GPU 章节所需的集合通信数学。
* [阶段 5 — CUDA 高级优化](../03-CUDA高级优化/README.md) — TMA、persistent kernel、warp 特化模式。

## 范围

* **涵盖范围：** B200 硅片上的 Qwen2.5-72B-Instruct、Qwen3-32B/A3B（MoE，混合专家模型），以及假想中的 Qwen3-300B+ 级模型。FP4/FP6/FP8/FP16 数据类型。从单 GPU 到 NVL72 机架级。
* **不涵盖：** 训练、微调、LoRA。Hopper（H100/H200）recipe——见 Qwen 系列的 H100 章节。AMD MI300X——不同章节、不同 fabric、不同软件栈。

## 关于日期与软件版本的说明

本系列假定使用 2026 年中的 Blackwell 适配软件栈：CUDA 13、cuBLAS 13、TensorRT-LLM 0.20+、vLLM Blackwell 后端（PR 系列已于 2026 年 Q1 合入）、Transformer Engine 2.x、FlashAttention 3.x。若你读到本文时所用工具链更早，预计会遇到 kernel 缺失，以及比文中数字更慢的性能。


<details>
<summary>English original</summary>

**Blackwell B200 for Qwen Transformer Inference**

A 6-chapter special course on running Qwen-class transformer models on NVIDIA's **Blackwell B200** generation: single B200, GB200 superchip, and the NVL72 rack-scale system. Inference only — no training, no fine-tuning.

This series is a Blackwell counterpart to the [Qwen Inference Optimization series](../../../../03-方向C-边缘AI/05-Qwen推理优化/README.md) in Edge AI. That series covered Qwen3-4B on Jetson and Qwen2.5-72B on 4×H100. This series picks up where the H100 left off: **what changes when you move Qwen2.5-72B (and bigger) to Blackwell.**

The short version: a single B200 holds 192 GB of HBM3e at 8 TB/s and supports FP4 tensor cores, so a Qwen2.5-72B-class model fits in **one** GPU with bandwidth-bound decode throughput that requires 4–8 H100s to match. The NVL72 rack scales the same model to extreme batch + long context with one coherent memory image. Software has to keep up — Transformer Engine 2, FP4 microscaling, FlashAttention-3 on 5th-gen tensor cores, TMA enhancements, persistent kernels.

| Chapter | Title | Focus |
|---|---|---|
| 01 | [Blackwell Architecture](01-Blackwell-Architecture.md) | Dual-die package, NVLink-C2C, HBM3e, 5th-gen tensor cores, NVL72 |
| 02 | [FP4 Numerics & Transformer Engine 2](02-FP4-Numerics-Transformer-Engine.md) | MX-FP4/FP6/FP8 microscaling, dynamic per-block quant, quality vs bandwidth |
| 03 | [Qwen on a Single B200](03-Single-B200-Qwen-Inference.md) | Qwen2.5-72B fitting in one die, layout, per-die TP via NVLink-C2C |
| 04 | [Multi-B200 and NVL72](04-Multi-B200-NVL72.md) | GB200 superchip, NVL72 architecture, TP=16/72, NCCL on NVLink-5 |
| 05 | [Blackwell Kernel Engineering](05-Blackwell-Kernel-Engineering.md) | 5th-gen WGMMA, TMA-2, async warp specialization, FlashAttention-3, CUTLASS 4 |
| 06 | [Production Serving on Blackwell](06-Production-Serving-on-Blackwell.md) | TRT-LLM 0.20+, vLLM Blackwell backend, FP4 in prod, benchmarks, cost |

**Prerequisites**

* [Phase 5 — Edge LLM Inference Internals](../../../../03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md) — GEMV vs GEMM roofline math.
* [Phase 5 — Qwen Inference Optimization (full series)](../../../../03-方向C-边缘AI/05-Qwen推理优化/README.md) — especially Lecture 04 (Qwen2.5-72B Multi-GPU FP16 on H100).
* [Phase 5 — NCCL Deep Dive](../08-NCCL深度探索/README.md) — collective math for the multi-GPU chapters.
* [Phase 5 — CUDA Advanced Optimization](../03-CUDA高级优化/README.md) — TMA, persistent kernels, warp specialization patterns.

**Scope**

* **In scope:** Qwen2.5-72B-Instruct, Qwen3-32B/A3B (MoE), and hypothetical Qwen3-300B+ class models on B200 silicon. FP4/FP6/FP8/FP16 dtypes. Single-GPU through NVL72 rack-scale.
* **Out of scope:** training, fine-tuning, LoRA. Hopper (H100/H200) recipes — see the H100 chapter of the Qwen series. AMD MI300X — different chapter, different fabric, different software stack.

**A note on dates and software versions**

This series assumes Blackwell-aware software stacks from mid-2026: CUDA 13, cuBLAS 13, TensorRT-LLM 0.20+, vLLM Blackwell backend (PR series merged Q1 2026), Transformer Engine 2.x, FlashAttention 3.x. If you're reading on an earlier toolchain, expect missing kernels and slower-than-quoted numbers.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Blackwell-B200-Qwen-Inference/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Blackwell-B200-Qwen-Inference/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
