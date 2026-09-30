---
title: 面向 HPC（高性能计算）与 AI 的 AMD GPU
description: 面向 HPC（高性能计算）与 AI 的 AMD GPU
---

# 面向 HPC（高性能计算）与 AI 的 AMD GPU

<div class="course-identity auto-course" style="--course-accent: #ca8a04; --course-accent-rgb: 202, 138, 4;" markdown="1">
<div class="course-identity__icon">AGFH</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度剖析 · 专业方向</p>
<p class="course-identity__title">面向 HPC 和 AI 的 AMD GPU 的专业课程标识。</p>
<p class="course-identity__meta">产物：专业化案例研究 · 度量：性能、可靠性、角色匹配</p>
</div>
</div>


**时间线：** 6–12 个月。

**前置要求：** 阶段 1 §4（C++ 与并行计算 — CUDA/OpenCL）、阶段 4 方向 C（ML 编译器基础），建议有 Nvidia GPU 子方向以建立背景。

---

## 为什么选 AMD GPU

AMD 的 Instinct GPU（MI300X、MI300A、MI350）是规模化 AI 训练与推理中 Nvidia 之外的主要替代选择。同时理解两个生态会让你更全面、更有价值 — 尤其是当云厂商（Azure、Oracle、Meta）在 Nvidia 之外一并部署 AMD 硬件时。

---

## 1. AMD GPU 架构

* **CDNA 与 RDNA：**
    * CDNA（Compute DNA）：面向数据中心优化 — MI300X、MI250X。Matrix Core、大容量 HBM、高带宽。
    * RDNA（Radeon DNA）：消费级/游戏 GPU。有助于理解整个架构家族。
* **MI300X 架构：**
    * Chiplet 设计：8 个 XCD（Accelerator Complex Die）+ 4 个 IOD。
    * 192 GB HBM3，带宽 5.3 TB/s。
    * Matrix Core：FP16、BF16、FP8、INT8。
    * Infinity Fabric 用于 die 间与 GPU 间通信。
* **计算单元：**
    * Wavefront（64 线程）对 CUDA warp（32 线程）。
    * SIMD 单元、LDS（Local Data Share）对 CUDA 共享内存。
    * occupancy 模型与 Nvidia 存在差异。

---

## 2. ROCm 软件栈

* **ROCm（Radeon Open Compute）：**
    * 开源 GPU 计算平台。MI300X 使用 ROCm 6+。
    * kernel 驱动（`amdgpu`）、runtime（`hip-runtime`）、编译器（`amd-clang`）。
* **HIP（Heterogeneous-computing Interface for Portability）：**
    * 面向 AMD GPU 的类 CUDA API。`hipMalloc`、`hipMemcpy`、`hipLaunchKernelGGL`。
    * **HIPIFY：** 将 CUDA 源码转换为 HIP 的工具（`hipify-perl`、`hipify-clang`）。
    * 一次编写的 kernel 可同时面向 AMD 与 Nvidia GPU。
* **库：**
    * **rocBLAS**（GEMM）、**MIOpen**（cuDNN 对应物）、**rocFFT**、**rocSPARSE**。
    * **RCCL**（ROCm Communication Collectives Library）— AMD 面向多 GPU 的 NCCL 对应实现。
    * **Composable Kernel（CK）** — AMD 对应 CUTLASS 的库，用于自定义 GEMM/attention kernel。
* **性能剖析：**
    * **rocProfiler** / **rocTracer** — kernel 级性能剖析。
    * **Omniperf** — roofline（性能上界模型）分析、occupancy、内存吞吐（类似 Nsight Compute）。
    * **Omnitrace** — 时间线性能剖析（类似 Nsight Systems）。

---

## 3. 将 CUDA 移植到 AMD

* **HIPIFY 工作流：**
    * 自动转换：用 `hipify-clang` 做源到源翻译。
    * 人工处理：CUDA 专有 intrinsic、内联 PTX、warp 级原语。
* **关键差异：**
    * warp 大小为 32（Nvidia）而 wavefront 大小为 64（AMD）— 影响归约、ballot、shuffle 操作。
    * 共享内存 bank 冲突：32 个 bank（Nvidia）对 64 个 bank（AMD）。
    * 内存合并访问规则略有不同。
    * 没有与 Nvidia Tensor Core 直接对应的单元 — 通过 `rocWMMA` 或 CK 使用 Matrix Core。
* **框架支持：**
    * PyTorch：原生支持 ROCm（`torch.cuda` 通过 HIP 在 AMD 上运行）。
    * TensorFlow、JAX：提供 ROCm 后端。
    * TVM、ONNX Runtime：ROCm execution provider。
    * AMD 上的 vLLM、TensorRT-LLM 替代方案：vLLM + ROCm、AMD Inference Server。

---

## 4. 多 GPU 与集群运维

* **Infinity Fabric：**
    * 节点内 GPU 间互连。其带宽与拓扑可与 NVLink 对比。
    * MI300X：每 GPU 的 fabric 聚合带宽 896 GB/s。
* **RCCL：**
    * 在 AMD GPU 上执行 all-reduce、all-gather、reduce-scatter。
    * 针对 MI300X 拓扑与 Infinity Fabric 调优。
* **多节点：**
    * 支持 InfiniBand 与 RoCE（RDMA over Converged Ethernet）。
    * AMD 上对应的 GPUDirect RDMA。
    * Slurm 与 Kubernetes 支持 ROCm 容器。

---

## 5. 在 AMD 上开发 kernel

* **编写 HIP kernel：**
    * 线程层次：grid → block → thread（与 CUDA 相同）。
    * 共享内存（`__shared__`）、同步（`__syncthreads()`）。
    * 面向 wavefront 的编程：`__ballot`、`__shfl`，用 wavefront 的对应操作实现 warp intrinsic。
* **Composable Kernel（CK）：**
    * 用于高性能 GEMM、attention 与自定义算子的模板库。
    * 基于分块的编程模型（概念类似 CuTe/CUTLASS）。
    * 用 CK 构建块编写自定义 kernel。
* **AMD 上的 Triton：**
    * Triton 通过 ROCm 后端支持 AMD GPU。
    * 相同的 Python kernel 代码，不同的后端代码生成。
    * 性能调优存在差异（分块大小、occupancy 目标）。

---


<details>
<summary>English original</summary>

**AMD GPU for HPC and AI**

<div class="course-identity auto-course" style="--course-accent: #ca8a04; --course-accent-rgb: 202, 138, 4;" markdown="1">
<div class="course-identity__icon">AGFH</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Specialization</p>
<p class="course-identity__title">Specialized course identity for AMD GPU for HPC and AI.</p>
<p class="course-identity__meta">Artifact: specialization case study · Measure: performance, reliability, role fit</p>
</div>
</div>


**Timeline:** 6–12 months.

**Prerequisites:** Phase 1 §4 (C++ and Parallel Computing — CUDA/OpenCL), Phase 4 Track C (ML compiler fundamentals), Nvidia GPU sub-track recommended for context.

---

**Why AMD GPU**

AMD's Instinct GPUs (MI300X, MI300A, MI350) are the primary alternative to Nvidia for AI training and inference at scale. Understanding both ecosystems makes you more versatile and valuable — especially as cloud providers (Azure, Oracle, Meta) deploy AMD hardware alongside Nvidia.

---

**1. AMD GPU Architecture**

* **CDNA vs RDNA:**
    * CDNA (Compute DNA): data-center optimized — MI300X, MI250X. Matrix cores, large HBM, high bandwidth.
    * RDNA (Radeon DNA): consumer/gaming GPUs. Relevant for understanding the architecture family.
* **MI300X architecture:**
    * Chiplet design: 8 XCDs (Accelerator Complex Dies) + 4 IODs.
    * 192 GB HBM3 with 5.3 TB/s bandwidth.
    * Matrix cores: FP16, BF16, FP8, INT8.
    * Infinity Fabric for inter-chiplet and inter-GPU communication.
* **Compute units:**
    * Wavefront (64 threads) vs CUDA warp (32 threads).
    * SIMD units, LDS (Local Data Share) vs CUDA shared memory.
    * Occupancy model differences from Nvidia.

---

**2. ROCm Software Stack**

* **ROCm (Radeon Open Compute):**
    * Open-source GPU compute platform. ROCm 6+ for MI300X.
    * Kernel driver (`amdgpu`), runtime (`hip-runtime`), compiler (`amd-clang`).
* **HIP (Heterogeneous-computing Interface for Portability):**
    * CUDA-like API for AMD GPUs. `hipMalloc`, `hipMemcpy`, `hipLaunchKernelGGL`.
    * **HIPIFY:** Tool to convert CUDA source to HIP (`hipify-perl`, `hipify-clang`).
    * Write-once kernels that target both AMD and Nvidia GPUs.
* **Libraries:**
    * **rocBLAS** (GEMM), **MIOpen** (cuDNN equivalent), **rocFFT**, **rocSPARSE**.
    * **RCCL** (ROCm Communication Collectives Library) — AMD's NCCL equivalent for multi-GPU.
    * **Composable Kernel (CK)** — AMD's equivalent to CUTLASS for custom GEMM/attention kernels.
* **Profiling:**
    * **rocProfiler** / **rocTracer** — kernel-level profiling.
    * **Omniperf** — roofline analysis, occupancy, memory throughput (like Nsight Compute).
    * **Omnitrace** — timeline profiling (like Nsight Systems).

---

**3. Porting CUDA to AMD**

* **HIPIFY workflow:**
    * Automated conversion: `hipify-clang` for source-to-source translation.
    * Manual work: CUDA-specific intrinsics, inline PTX, warp-level primitives.
* **Key differences:**
    * Warp size 32 (Nvidia) vs wavefront size 64 (AMD) — affects reduction, ballot, shuffle ops.
    * Shared memory bank conflicts: 32 banks (Nvidia) vs 64 banks (AMD).
    * Memory coalescing rules differ slightly.
    * No direct equivalent to Nvidia Tensor Cores — use Matrix Cores via `rocWMMA` or CK.
* **Framework support:**
    * PyTorch: native ROCm support (`torch.cuda` works on AMD via HIP).
    * TensorFlow, JAX: ROCm backends available.
    * TVM, ONNX Runtime: ROCm execution providers.
    * vLLM, TensorRT-LLM alternatives for AMD: vLLM + ROCm, AMD Inference Server.

---

**4. Multi-GPU and Cluster Operations**

* **Infinity Fabric:**
    * Intra-node GPU-to-GPU interconnect. Bandwidth and topology compared to NVLink.
    * MI300X: 896 GB/s aggregate fabric bandwidth per GPU.
* **RCCL:**
    * All-reduce, all-gather, reduce-scatter on AMD GPUs.
    * Tuning for MI300X topology and Infinity Fabric.
* **Multi-node:**
    * InfiniBand and RoCE (RDMA over Converged Ethernet) support.
    * GPUDirect RDMA equivalent on AMD.
    * Slurm and Kubernetes with ROCm container support.

---

**5. Kernel Development on AMD**

* **HIP kernel writing:**
    * Thread hierarchy: grid → block → thread (same as CUDA).
    * Shared memory (`__shared__`), synchronization (`__syncthreads()`).
    * Wavefront-aware programming: `__ballot`, `__shfl`, warp intrinsics via wavefront equivalents.
* **Composable Kernel (CK):**
    * Templated library for high-performance GEMM, attention, and custom ops.
    * Tile-based programming model (similar concept to CuTe/CUTLASS).
    * Writing custom kernels with CK building blocks.
* **Triton on AMD:**
    * Triton supports AMD GPUs via ROCm backend.
    * Same Python kernel code, different backend code generation.
    * Performance tuning differences (tile sizes, occupancy targets).

---

</details>

## 资源

* [ROCm Documentation](https://rocm.docs.amd.com/)
* [HIP Programming Guide](https://rocm.docs.amd.com/projects/HIP/en/latest/)
* [Composable Kernel](https://github.com/ROCm/composable_kernel)
* [HIPIFY](https://rocm.docs.amd.com/projects/HIPIFY/en/latest/)
* [Omniperf](https://rocm.docs.amd.com/projects/omniperf/en/latest/)
* [AMD Instinct MI300X](https://www.amd.com/en/products/accelerators/instinct/mi300/mi300x.html)

---

## 项目

1. **把 CUDA kernel HIPIFY** —— 把阶段 1 §4 的 CUDA 矩阵乘或 vector-add kernel 拿来。用 `hipify-clang` 转成 HIP。在 AMD GPU（或 ROCm Docker）上运行。对比输出与性能。
2. **用 Omniperf 做 profile** —— 在 ROCm 上 profile 一个 PyTorch 模型。生成 roofline 图（性能上界模型）。识别算力受限与带宽受限的 layer。
3. **RCCL benchmark** —— 在多个 AMD GPU 上跑 RCCL all-reduce。与同等 Nvidia 硬件上的 NCCL 对比带宽和延迟。
4. **AMD 上的 Triton** —— 写一个 Triton 融合 kernel（例如 layer norm + residual）。在 Nvidia（CUDA）和 AMD（ROCm）上分别运行。对比生成的代码与性能。
5. **CK 自定义 GEMM** —— 用 Composable Kernel 实现一个带 epilogue 融合（bias + 激活值）的自定义 GEMM。与 rocBLAS 对比 benchmark。


<details>
<summary>English original</summary>

**Resources**

* [ROCm Documentation](https://rocm.docs.amd.com/)
* [HIP Programming Guide](https://rocm.docs.amd.com/projects/HIP/en/latest/)
* [Composable Kernel](https://github.com/ROCm/composable_kernel)
* [HIPIFY](https://rocm.docs.amd.com/projects/HIPIFY/en/latest/)
* [Omniperf](https://rocm.docs.amd.com/projects/omniperf/en/latest/)
* [AMD Instinct MI300X](https://www.amd.com/en/products/accelerators/instinct/mi300/mi300x.html)

---

**Projects**

1. **HIPIFY a CUDA kernel** — Take your CUDA matmul or vector-add kernel from Phase 1 §4. Convert to HIP using `hipify-clang`. Run on AMD GPU (or ROCm Docker). Compare output and performance.
2. **Profile with Omniperf** — Profile a PyTorch model on ROCm. Generate a roofline plot. Identify compute-bound vs memory-bound layers.
3. **RCCL benchmark** — Run RCCL all-reduce across multiple AMD GPUs. Compare bandwidth and latency with NCCL on equivalent Nvidia hardware.
4. **Triton on AMD** — Write a Triton fused kernel (e.g., layer norm + residual). Run on both Nvidia (CUDA) and AMD (ROCm). Compare generated code and performance.
5. **CK custom GEMM** — Use Composable Kernel to implement a custom GEMM with epilogue fusion (bias + activation). Benchmark against rocBLAS.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/AMD GPU/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/AMD%20GPU/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
