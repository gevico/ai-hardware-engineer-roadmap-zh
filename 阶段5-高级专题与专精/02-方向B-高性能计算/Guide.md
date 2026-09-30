---
title: 阶段 5 — 方向 B：高性能计算
description: 阶段 5 — 方向 B：高性能计算
---

# 阶段 5 — 方向 B：高性能计算

<div class="course-identity hpc" markdown="1">
<div class="course-identity__icon">HPC</div>
<div markdown="1">
<p class="course-identity__eyebrow">Track 5B · HPC / CUDA-X</p>
<p class="course-identity__title">使用 GPU 加速库与性能工具构建高吞吐计算系统。</p>
<p class="course-identity__meta">产物：CUDA-X benchmark · 度量：FLOP/s、带宽、加速比、效率</p>
</div>
</div>


**时间线：** 长期持续 — 按项目需要逐个类别学习。

**前置要求：** 阶段 1 §4（C++/CUDA）、阶段 4 方向 B（Jetson、CUDA runtime）、阶段 4 方向 C（ML 编译器 + DL 推理优化）、阶段 5A（GPU 基础设施）。

---

## 本方向涵盖的内容

高性能计算不止于单 GPU 编程与集群搭建（阶段 5A GPU 基础设施已涵盖）。本方向涵盖运行在这套基础设施之上的 **GPU 加速库生态** 与 **领域专用 HPC 应用**。

| 子方向 | 关注点 | 指南 |
|-----------|-------|-------|
| **CUDA-X Libraries** | NVIDIA 完整的 GPU 加速库套件：数学（cuBLAS、cuFFT）、DL（cuDNN、CUTLASS、TensorRT）、数据（RAPIDS）、视觉（DALI、CV-CUDA）、通信（NCCL、NVSHMEM）、并行算法（Thrust、CUB），以及另外 40+ 个 | [CUDA-X Libraries →](01-CUDA-X库/Guide.md) |

---

## 如何使用本方向

1. **先完成 [阶段 5A — GPU 基础设施](../01-方向A-GPU基础设施/Guide.md)** — 涵盖 NVIDIA/AMD GPU 集群、多 GPU 组网、Slurm/K8s、分布式训练。
2. **然后 [CUDA-X Libraries](01-CUDA-X库/Guide.md)** — 每一个 GPU 加速库的完整参考。按类别（数学、DL、数据、视觉、通信）组织，配有基于角色的学习路径与动手项目。

## 与其他方向的关系

| 本方向提供 | 其他方向用于 |
|--------------------|------------------------|
| cuBLAS、cuDNN、CUTLASS | 阶段 4C（编译器目标）、阶段 5F（AI 芯片设计 — 硬件必须加速什么） |
| NCCL、NVSHMEM | 阶段 5A（GPU 基础设施 — 多 GPU 通信） |
| TensorRT、FlashInfer | 阶段 4B §8（Jetson runtime）、阶段 4C 第 2 部分（推理优化） |
| RAPIDS（cuDF、cuML、cuGraph） | 任意 ML 工作流的数据流水线加速 |
| CV-CUDA、DALI、Video Codec SDK | 阶段 5E（自动驾驶 — 感知流水线） |


<details>
<summary>English original</summary>

**Phase 5 — Track B: High Performance Computing**

<div class="course-identity hpc" markdown="1">
<div class="course-identity__icon">HPC</div>
<div markdown="1">
<p class="course-identity__eyebrow">Track 5B · HPC / CUDA-X</p>
<p class="course-identity__title">Use GPU-accelerated libraries and performance tools to build high-throughput compute systems.</p>
<p class="course-identity__meta">Artifact: CUDA-X benchmark · Measure: FLOP/s, bandwidth, speedup, efficiency</p>
</div>
</div>


**Timeline:** Ongoing — study each category as your projects demand it.

**Prerequisites:** Phase 1 §4 (C++/CUDA), Phase 4 Track B (Jetson, CUDA runtime), Phase 4 Track C (ML compiler + DL inference optimization), Phase 5A (GPU Infrastructure).

---

**What this track covers**

High Performance Computing goes beyond single-GPU programming and cluster setup (covered in Phase 5A GPU Infrastructure). This track covers the **GPU-accelerated library ecosystem** and **domain-specific HPC applications** that run on top of that infrastructure.

| Sub-track | Focus | Guide |
|-----------|-------|-------|
| **CUDA-X Libraries** | The full NVIDIA GPU-accelerated library suite: math (cuBLAS, cuFFT), DL (cuDNN, CUTLASS, TensorRT), data (RAPIDS), vision (DALI, CV-CUDA), communication (NCCL, NVSHMEM), parallel algorithms (Thrust, CUB), and 40+ more | [CUDA-X Libraries →](01-CUDA-X库/Guide.md) |

---

**How to use this track**

1. **Complete [Phase 5A — GPU Infrastructure](../01-方向A-GPU基础设施/Guide.md)** first — covers Nvidia/AMD GPU clusters, multi-GPU networking, Slurm/K8s, distributed training.
2. **Then [CUDA-X Libraries](01-CUDA-X库/Guide.md)** — the comprehensive reference for every GPU-accelerated library. Organized by category (math, DL, data, vision, communication) with role-based learning paths and hands-on projects.

**Relationship to other tracks**

| This track provides | Other tracks use it for |
|--------------------|------------------------|
| cuBLAS, cuDNN, CUTLASS | Phase 4C (compiler targets), Phase 5F (AI Chip Design — what hardware must accelerate) |
| NCCL, NVSHMEM | Phase 5A (GPU Infrastructure — multi-GPU communication) |
| TensorRT, FlashInfer | Phase 4B §8 (Jetson runtime), Phase 4C Part 2 (inference optimization) |
| RAPIDS (cuDF, cuML, cuGraph) | Data pipeline acceleration for any ML workflow |
| CV-CUDA, DALI, Video Codec SDK | Phase 5E (Autonomous Vehicles — perception pipelines) |

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track B - High Performance Computing/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20B%20-%20High%20Performance%20Computing/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
