---
title: 阶段 5 — 方向 A：GPU 基础设施
description: 阶段 5 — 方向 A：GPU 基础设施
---

# 阶段 5 — 方向 A：GPU 基础设施

<div class="course-identity gpu-infra" markdown="1">
<div class="course-identity__icon">GPU</div>
<div markdown="1">
<p class="course-identity__eyebrow">方向 5A · GPU 基础设施</p>
<p class="course-identity__title">运维多 GPU 系统、互连、分布式 runtime 与加速器平台。</p>
<p class="course-identity__meta">产物：GPU 集群/runbook · 度量：扩展性、带宽、利用率、成本</p>
</div>
</div>


**时间线：** 12–24 个月（基础部分）；进阶阶段 24–48 个月。

**前置要求：** 阶段 4 方向 B（Jetson、CUDA 软件栈），阶段 4 方向 C（ML 编译器 + DL 推理优化）。

---

## 本方向涵盖的内容

**HPC** = 高性能计算（high-performance computing）：借助多台机器（或多个 GPU）协同工作，解决需要海量算力与内存的问题。在 AI 领域，“HPC”通常指 GPU 集群上的**大规模训练**与**高吞吐推理**，而非单台工作站。

本方向围绕你在真实 HPC 与大规模 AI 工作中要评估的主要加速器平台来组织：

| 子方向 | 重点 | 指南 |
|-----------|-------|-------|
| **Nvidia GPU** | CUDA、NCCL、NVLink/NVSwitch、InfiniBand、GPUDirect、Slurm/K8s、多 GPU/多节点集群 | [Nvidia GPU →](04-Nvidia-GPU/Guide.md) |
| **AMD GPU** | ROCm、HIP、RCCL、AMD Instinct (MI300X)、RDNA/CDNA 架构、移植 CUDA 工作负载 | [AMD GPU →](01-AMD-GPU/Guide.md) |
| **分布式 AI 互连** | vLLM、PyTorch distributed、UCX、UCC、NCCL/RCCL、拓扑感知的多节点推理，以及 broken-fabric 调试 | [分布式 AI 互连 →](03-分布式人工智能互连/Guide.md) |
| **加速器平台评估** | 面向训练、推理、扩展、工具链与性价比取舍，实操对比 NVIDIA GPU、AMD GPU 与 Google 加速器 | [加速器平台评估 →](02-加速器平台评估/Guide.md) |

关于完整的 CUDA-X 库生态（cuBLAS、cuDNN、CUTLASS、TensorRT、NCCL、RAPIDS 等 40 多个库），见 **[阶段 5B — 高性能计算](../02-方向B-高性能计算/Guide.md)**。

---

## 如何使用本方向

1. **从 [Nvidia GPU](04-Nvidia-GPU/Guide.md)** 开始** — AI HPC 的主导生态。涵盖基础、虚拟化、互连、存储、分布式训练与性能建模。
2. **再补上 [AMD GPU](01-AMD-GPU/Guide.md)** — 用于可移植性、替代硬件，以及理解不断壮大的 AMD AI 生态（MI300X、ROCm 6+）。
3. 当你的项目跨越 vLLM、PyTorch distributed、UCX/UCC、NCCL/RCCL 以及非平凡的物理拓扑时，**学习 [分布式 AI 互连](03-分布式人工智能互连/Guide.md)**。
4. 当你需要在真实工作负载与真实集群运维中，于 NVIDIA、AMD 和 Google 加速器路线之间做选择时，**使用 [加速器平台评估](02-加速器平台评估/Guide.md)**。

这些方向假定你已完成阶段 4 方向 C（编译器 + 推理优化）。这里的 HPC 内容聚焦于**基础设施、多 GPU 扩展与分布式系统**——编译器与 kernel 优化技能来自方向 C。


<details>
<summary>English original</summary>

**Phase 5 — Track A: GPU Infrastructure**

<div class="course-identity gpu-infra" markdown="1">
<div class="course-identity__icon">GPU</div>
<div markdown="1">
<p class="course-identity__eyebrow">Track 5A · GPU Infrastructure</p>
<p class="course-identity__title">Operate multi-GPU systems, interconnects, distributed runtimes, and accelerator platforms.</p>
<p class="course-identity__meta">Artifact: GPU cluster/runbook · Measure: scaling, bandwidth, utilization, cost</p>
</div>
</div>


**Timeline:** 12–24 months (fundamentals); 24–48 months for advanced phase.

**Prerequisites:** Phase 4 Track B (Jetson, CUDA stack), Phase 4 Track C (ML compiler + DL inference optimization).

---

**What this track covers**

**HPC** = high-performance computing: solving problems that need massive compute and memory by using many machines (or many GPUs) working together. In AI, "HPC" usually means **large-scale training** and **high-throughput inference** on GPU clusters, not single workstations.

This track is organized around the main accelerator platforms you will evaluate in real HPC and large-scale AI work:

| Sub-track | Focus | Guide |
|-----------|-------|-------|
| **Nvidia GPU** | CUDA, NCCL, NVLink/NVSwitch, InfiniBand, GPUDirect, Slurm/K8s, multi-GPU/multi-node clusters | [Nvidia GPU →](04-Nvidia-GPU/Guide.md) |
| **AMD GPU** | ROCm, HIP, RCCL, AMD Instinct (MI300X), RDNA/CDNA architecture, porting CUDA workloads | [AMD GPU →](01-AMD-GPU/Guide.md) |
| **Distributed AI Interconnects** | vLLM, PyTorch distributed, UCX, UCC, NCCL/RCCL, topology-aware multi-node inference, and broken-fabric debugging | [Distributed AI Interconnects →](03-分布式人工智能互连/Guide.md) |
| **Accelerator Platform Evaluation** | Practical comparison of NVIDIA GPUs, AMD GPUs, and Google accelerators for training, inference, scaling, tooling, and cost/performance tradeoffs | [Accelerator Platform Evaluation →](02-加速器平台评估/Guide.md) |

For the full CUDA-X library ecosystem (cuBLAS, cuDNN, CUTLASS, TensorRT, NCCL, RAPIDS, and 40+ more), see **[Phase 5B — High Performance Computing](../02-方向B-高性能计算/Guide.md)**.

---

**How to use this track**

1. **Start with [Nvidia GPU](04-Nvidia-GPU/Guide.md)** — the dominant ecosystem for AI HPC. Covers fundamentals, virtualization, interconnects, storage, distributed training, and performance modeling.
2. **Add [AMD GPU](01-AMD-GPU/Guide.md)** — for portability, alternative hardware, and understanding the growing AMD AI ecosystem (MI300X, ROCm 6+).
3. **Study [Distributed AI Interconnects](03-分布式人工智能互连/Guide.md)** when your project crosses vLLM, PyTorch distributed, UCX/UCC, NCCL/RCCL, and non-trivial physical topologies.
4. **Use [Accelerator Platform Evaluation](02-加速器平台评估/Guide.md)** when you need to choose between NVIDIA, AMD, and Google accelerator paths for real workloads and real cluster operations.

These tracks assume you've completed Phase 4 Track C (compiler + inference optimization). The HPC content here focuses on **infrastructure, multi-GPU scaling, and distributed systems** — the compiler and kernel optimization skills come from Track C.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
