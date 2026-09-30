---
title: 使用 Nvidia GPU 的高性能计算（HPC）
description: 使用 Nvidia GPU 的高性能计算（HPC）
---

# 使用 Nvidia GPU 的高性能计算（HPC）

<div class="course-identity auto-course" style="--course-accent: #db2777; --course-accent-rgb: 219, 39, 119;" markdown="1">
<div class="course-identity__icon">HWNG</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度剖析 · 专项课程</p>
<p class="course-identity__title">使用 Nvidia GPU 的高性能计算的专项课程标识。</p>
<p class="course-identity__meta">产物：专项课程案例研究 · 度量：性能、可靠性、岗位匹配度</p>
</div>
</div>


**父级：** [高性能计算](../Guide.md)

**时间线：** 12–24 个月（基础与深入专题）；高级阶段为 24–48 个月。

---

## 基本概念：“使用 Nvidia GPU 的高性能计算”指什么


**HPC** = 高性能计算：通过让多台机器（或多块 GPU）协同工作，解决需要海量计算和内存的问题。在 AI 中，“HPC”通常指 GPU 簇上的**大规模训练**和**高吞吐推理**，而非单台工作站。

**为什么用 Nvidia GPU？** Nvidia GPU 是训练和部署大模型的主导硬件。它们提供支持最好的软件栈：**CUDA**（编程模型和 runtime）、**cuDNN** 和 **CUTLASS**（高性能 kernel —— cuDNN 用于 conv/RNN/attention；CUTLASS 用于可定制的 GEMM（矩阵-矩阵乘）、矩阵乘以及框架和自定义 kernel 使用的 epilogue 融合）、**TensorRT**（推理优化与部署）和 **NCCL**（多 GPU 集合通信）。再加上最快的 GPU 间互连（NVLink、NVSwitch）以及 ML 框架优先适配的架构（Hopper、Blackwell），就会明白为什么 AI 基础设施和 kernel 级优化工作几乎总会在数据中心里涉及 Nvidia GPU。

**本方向涵盖：**

* **单 GPU → 多 GPU** —— 从一块 GPU（例如 Jetson，你在阶段 4 方向 B 见过）到 **多 GPU 节点** 和 **多节点簇**。需要理解作业如何放置、数据和梯度如何移动，以及如何避免通信成为瓶颈。
* **两类主要工作负载：**
    * **训练** —— 一个大模型、一个大型数据集；把工作拆分到多块 GPU 上（数据并行、模型并行、流水线并行）。性能关乎吞吐（样本/秒）以及扩展到数百或数千块 GPU。
    * **推理** —— 多个请求、一个（或多个）已部署模型；关注负载下的延迟和吞吐。规模化时意味着批处理、KV-cache，以及往往是多 GPU 或多节点推理服务（例如 TensorRT-LLM、vLLM）。
* **必须理解的栈：**
    * **硬件：** GPU（A100、H100/H200、L40S 等）、节点内的 NVLink/NVSwitch、跨节点的 InfiniBand 或以太网。
    * **软件：** CUDA、驱动、容器（NGC）、编排器（Slurm、Kubernetes）以及用于多 GPU 通信的集合通信库（NCCL）。
    * **存储与 I/O：** 快速把数据送到 GPU（数据加载器、GPUDirect Storage、高吞吐磁盘），使 GPU 不会空等。


<details>
<summary>English original</summary>

**HPC with Nvidia GPU**

<div class="course-identity auto-course" style="--course-accent: #db2777; --course-accent-rgb: 219, 39, 119;" markdown="1">
<div class="course-identity__icon">HWNG</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Specialization</p>
<p class="course-identity__title">Specialized course identity for HPC with Nvidia GPU.</p>
<p class="course-identity__meta">Artifact: specialization case study · Measure: performance, reliability, role fit</p>
</div>
</div>


**Parent:** [High Performance Computing](../Guide.md)

**Timeline:** 12–24 months (fundamentals and deep dives); 24–48 months for advanced phase.

---

**Basic concepts: what "HPC with Nvidia GPU" means**


**HPC** = high-performance computing: solving problems that need massive compute and memory by using many machines (or many GPUs) working together. In AI, "HPC" usually means **large-scale training** and **high-throughput inference** on GPU clusters, not single workstations.

**Why Nvidia GPUs?** Nvidia GPUs are the dominant hardware for training and deploying large models. They offer the best-supported software stack: **CUDA** (programming model and runtime), **cuDNN** and **CUTLASS** (high-performance kernels — cuDNN for conv/RNN/attention; CUTLASS for customizable GEMM, matrix multiply, and epilogue fusion used by frameworks and custom kernels), **TensorRT** (inference optimization and deployment), and **NCCL** (multi-GPU collectives). Add the fastest inter-GPU links (NVLink, NVSwitch) and the architectures (Hopper, Blackwell) that ML frameworks target first, and you see why AI infrastructure and kernel-level optimization work almost always involves Nvidia GPUs in data centers.

**What this track covers:**

* **Single GPU → many GPUs** — From one GPU (e.g. Jetson, which you saw in Phase 4 Track B) to **multi-GPU nodes** and **multi-node clusters**. You need to understand how jobs are placed, how data and gradients move, and how to avoid communication becoming the bottleneck.
* **Two main workloads:**
    * **Training** — One big model, one big dataset; you split work across GPUs (data parallelism, model parallelism, pipeline parallelism). Performance is about throughput (samples/sec) and scaling to hundreds or thousands of GPUs.
    * **Inference** — Many requests, one (or many) deployed model; you care about latency and throughput under load. At scale this means batching, KV-cache, and often multi-GPU or multi-node serving (e.g. TensorRT-LLM, vLLM).
* **The stack you must understand:**
    * **Hardware:** GPUs (A100, H100/H200, L40S, etc.), NVLink/NVSwitch inside a node, InfiniBand or Ethernet across nodes.
    * **Software:** CUDA, drivers, containers (NGC), orchestrators (Slurm, Kubernetes), and collective libraries (NCCL) for multi-GPU communication.
    * **Storage and I/O:** Getting data to GPUs fast (dataloaders, GPUDirect Storage, high-throughput disks) so the GPU is not waiting.

</details>

### 关键术语（本方向使用）

*顺序：基础算子 → attention → 分布式。*

| 术语 | 含义 |
|------|--------|
| **矩阵乘** | 对矩阵 A、B 计算 A·B（通常还要加上 bias 或激活值）。线性层以及大多数重计算中的核心操作。在库里，标准叫法是 "GEMM"（矩阵-矩阵乘）。 |
| **GEMM** | **G**eneral **E**lement-wise **M**atrix **M**ultiply：C = α(A·B) + βC。矩阵乘的 BLAS/cuBLAS/CUTLASS 接口。GPU GEMM kernel（分块、张量核心）主导了训练和推理的耗时。 |
| **Epilogue 融合** | 在 GEMM kernel 中，**epilogue** 指对结果所做的处理（加 bias、ReLU/GELU、写入内存）。**融合** = 在乘法所在的*同一个* kernel 里完成这些处理。可省下内存带宽和启动开销；CUTLASS 及类似库支持该做法。 |
| **Attention** | 在 Transformer 中：每个 token 都有 **Q、K、V**（来自线性层）；*attention* = softmax(Q·K^T/√d)·V。它让模型聚焦到相关的 token 上。对长序列而言这些矩阵乘开销很大；优化过的 attention kernel 与 KV-cache 至关重要。 |
| **FlashAttention** | 一类 **attention kernel**，通过分块并把 Q、K、V 留在 SRAM 中来减少内存流量，并避免把完整的 Q·K^T 矩阵实际写出来。比朴素 attention 更快、更省内存；已成为 LLM 训练和推理的标准做法（如 FlashAttention-2、-3）。 |
| **KV-cache** | 在 Transformer 的 attention 中：把之前 token 的 key 和 value 缓存起来，以免重新计算。**KV-cache** = 这份缓存。长上下文 → 缓存巨大 → 内存和带宽成为瓶颈；分页/分片以及高效 kernel 很关键。 |
| **数据 / 模型 / 流水线并行** | **数据：**每个 GPU 上是同一个模型、不同的数据；同步梯度。**模型：**把模型切分到多个 GPU 上。**流水线：**不同的 layer 放在不同 GPU 上，以流水线方式传递激活值。 |
| **集合通信** | 多 GPU 操作：**AllReduce**（每个 GPU 都拿到相同的和）、**AllGather**（每个 GPU 都拿到所有分片）、**ReduceScatter**。用于同步梯度（数据并行）或交换激活值（模型/流水线并行）。 |
| **NCCL** | **N**vidia **C**ollective **C**ommunications **L**ibrary。为多 GPU 训练和推理实现集合通信操作（AllReduce、AllGather 等）。在规模扩大时常常是瓶颈。 |
| **NVLink / NVSwitch** | **NVLink：**节点内的高带宽 GPU↔GPU（以及 GPU↔CPU）互连。**NVSwitch：**连接单节点内众多 GPU 的交换芯片。在 GPU↔GPU 流量上比 PCIe 快得多。 |

---

## 本方向涵盖的内容（重新组织后）

本方向现在聚焦于 **HPC（高性能计算）基础设施与多 GPU 操作**。**DL 推理优化** 的内容（图优化、kernel 工程、编译器栈、量化、推理 runtime、tinygrad 深入剖析）已作为第 2 部分移至 **[阶段 4 方向 C — ML 编译器与图优化](../../../阶段4-路线C-深度学习推理优化/Guide.md)**，因为这些技能是所有硬件方向的共同基础，而不只是 HPC。

| 部分 | 说明 | 指南 |
|------|--------------|-------|
| **HPC Setup** | 基础原理、虚拟化、互连、进阶 CUDA/分布式训练/性能 —— 外加针对具体硬件的深入剖析（8x H200、L40S、NCCL、CUDA Advanced、GPUDirect Storage） | [HPC Setup →](01-HPC环境搭建/Guide.md) |
| ~~DL 推理优化~~ | **已移至 [阶段 4 方向 C 第 2 部分](../../../阶段4-路线C-深度学习推理优化/Guide.md)** | [方向 C →](../../../阶段4-路线C-深度学习推理优化/Guide.md) |

---

## 如何使用本方向

1. **先完成 [阶段 4 方向 C](../../../阶段4-路线C-深度学习推理优化/Guide.md)** —— 涵盖编译器基础与 DL 推理优化（图算子、kernel、编译器栈、量化、runtime）。
2. **然后 [HPC Setup](01-HPC环境搭建/Guide.md)** —— 涵盖 NVIDIA GPU HPC 基础、虚拟化（vGPU、KVM）、互连与存储（InfiniBand、GDS、Slurm、Kubernetes），以及阶段 2 的进阶主题（进阶 CUDA、分布式训练、性能建模）。针对你的目标硬件和软件栈，使用相应的深入剖析（8x H200、L40S、NCCL、CUDA Advanced、GDS）。

**前置要求：** 阶段 4 方向 B（Jetson、TensorRT、CUDA）与阶段 4 方向 C（编译器 + 推理优化）。


<details>
<summary>English original</summary>

**Key terms (used in this track)**

*Order: basic ops → attention → distributed.*

| Term | Meaning |
|------|--------|
| **Matrix multiply** | Compute A·B for matrices A, B (often plus bias or activation). The core operation in linear layers and most heavy compute. "GEMM" is the standard name in libraries. |
| **GEMM** | **G**eneral **E**lement-wise **M**atrix **M**ultiply: C = α(A·B) + βC. The BLAS/cuBLAS/CUTLASS interface for matrix multiply. GPU GEMM kernels (tiling, tensor cores) dominate training and inference time. |
| **Epilogue fusion** | In a GEMM kernel, the **epilogue** is what you do with the result (add bias, ReLU/GELU, write to memory). **Fusion** = doing that in the *same* kernel as the multiply. Saves memory bandwidth and launch overhead; CUTLASS and similar libraries support it. |
| **Attention** | In transformers: each token has **Q, K, V** (from linear layers); *attention* = softmax(Q·K^T/√d)·V. Lets the model focus on relevant tokens. The matmuls are heavy for long sequences; optimized attention kernels and KV-cache are critical. |
| **FlashAttention** | A family of **attention kernels** that reduce memory traffic by tiling and keeping Q,K,V in SRAM, and avoid materializing the full Q·K^T matrix. Faster and more memory-efficient than naive attention; standard in LLM training and inference (e.g. FlashAttention-2, -3). |
| **KV-cache** | In transformer attention: keys and values for previous tokens are cached so you don't recompute them. **KV-cache** = that cache. Long context → huge cache → memory and bandwidth become the bottleneck; paging/sharding and efficient kernels matter. |
| **Data / model / pipeline parallelism** | **Data:** same model on every GPU, different data; sync gradients. **Model:** split the model across GPUs. **Pipeline:** different layers on different GPUs, pass activations in a pipeline. |
| **Collectives** | Multi-GPU operations: **AllReduce** (everyone gets the same sum), **AllGather** (everyone gets all pieces), **ReduceScatter**. Used to sync gradients (data parallel) or exchange activations (model/pipeline parallel). |
| **NCCL** | **N**vidia **C**ollective **C**ommunications **L**ibrary. Implements collectives (AllReduce, AllGather, etc.) for multi-GPU training and inference. Often the bottleneck at scale. |
| **NVLink / NVSwitch** | **NVLink:** high-bandwidth GPU↔GPU (and GPU↔CPU) link inside a node. **NVSwitch:** switch connecting many GPUs in one node. Much faster than PCIe for GPU↔GPU traffic. |

---

**What this track covers (after reorganization)**

This track now focuses on **HPC infrastructure and multi-GPU operations**. The **DL Inference Optimization** content (graph optimization, kernel engineering, compiler stack, quantization, inference runtimes, tinygrad deep dive) has moved to **[Phase 4 Track C — ML Compiler & Graph Optimization](../../../阶段4-路线C-深度学习推理优化/Guide.md)** as Part 2, since those skills are foundational for all hardware tracks, not just HPC.

| Part | Description | Guide |
|------|--------------|-------|
| **HPC Setup** | Fundamentals, virtualization, interconnects, advanced CUDA/distributed training/performance — plus hardware-specific deep dives (8x H200, L40S, NCCL, CUDA Advanced, GPUDirect Storage) | [HPC Setup →](01-HPC环境搭建/Guide.md) |
| ~~DL Inference Optimization~~ | **Moved to [Phase 4 Track C Part 2](../../../阶段4-路线C-深度学习推理优化/Guide.md)** | [Track C →](../../../阶段4-路线C-深度学习推理优化/Guide.md) |

---

**How to use this track**

1. **Complete [Phase 4 Track C](../../../阶段4-路线C-深度学习推理优化/Guide.md)** first — covers compiler fundamentals and DL inference optimization (graph ops, kernels, compiler stack, quantization, runtimes).
2. **Then [HPC Setup](01-HPC环境搭建/Guide.md)** — Covers Nvidia GPU HPC fundamentals, virtualization (vGPU, KVM), interconnects and storage (InfiniBand, GDS, Slurm, Kubernetes), and Phase 2 advanced topics (advanced CUDA, distributed training, performance modeling). Use the deep dives (8x H200, L40S, NCCL, CUDA Advanced, GDS) for your target hardware and stack.

**Prerequisite:** Phase 4 Track B (Jetson, TensorRT, CUDA) and Phase 4 Track C (compiler + inference optimization).

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
