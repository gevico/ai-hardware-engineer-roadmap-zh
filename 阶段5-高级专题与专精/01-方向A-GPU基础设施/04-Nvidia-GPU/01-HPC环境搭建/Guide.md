---
title: 高性能计算（HPC）搭建
description: 高性能计算（HPC）搭建
---

# 高性能计算（HPC）搭建

<div class="course-identity auto-course" style="--course-accent: #ea580c; --course-accent-rgb: 234, 88, 12;" markdown="1">
<div class="course-identity__icon">HS</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入探索 · 专业化</p>
<p class="course-identity__title">针对高性能计算搭建的专项课程定位。</p>
<p class="course-identity__meta">产物：专项案例研究 · 衡量：性能、可靠性、岗位匹配</p>
</div>
</div>


**隶属于：** [高性能计算 — Nvidia GPU](../Guide.md)（阶段 5）

本指南将**高性能计算基础、虚拟化、互连与进阶主题**同面向真实 GPU 集群搭建与优化的**硬件专属深入探索**结合起来。先学习基础，再针对目标硬件（8x H200、L40S、NCCL、CUDA、GDS）使用深入探索内容。

---

## 硬件与软件栈深入探索

针对特定 GPU 集群配置与子系统的详细指南：

| 配置 | 用例 | 指南 |
|-------|----------|-------|
| **8x H200 SXM5** | 大模型训练与推理（1.1 TB HBM3e、NVLink 4.0） | [8x-H200-Training-Inference/](01-8x-H200训练推理/README.md) |
| **L40S x12** | 高性价比推理部署（576 GB GDDR6、PCIe） | [L40S-x12-Inference/](06-L40S-x12推理/README.md) |
| **NCCL 深入探索** | GPU 间通信：算法、调优、调试、1T 规模 | [NCCL-Deep-Dive/](08-NCCL深度探索/README.md) |
| **CUDA 进阶优化** | CUDA Graphs、Cooperative Groups、Persistent Kernels、融合、warp 特化 | [CUDA-Advanced-Optimization/](03-CUDA高级优化/README.md) |
| **GPUDirect Storage (GDS)** | NVMe→GPU 直接 DMA、NVMe-oF、WD OpenFlex + RapidFlex、libcufile API | [GPUDirect-Storage/](05-GPUDirect存储/README.md) |
| **长上下文 MoE（混合专家模型）基础训练** | 端到端课程：长上下文 attention、MoE 路由、mesh 设计、自适应数据、诚实评测 | [Long-Context-MoE-Foundation-Training/](07-长上下文MoE基础训练/README.md) |
| **机密计算与硬件远程证明** | 面向不可信/租用/去中心化集群节点的 Intel TDX + NVIDIA CC 远程证明：DCAP quote 验证、NRAS/JWKS、双厂商声明绑定 | [Confidential-Computing-Attestation/](04-机密计算认证/README.md) |

### 8x H200 — 主题
- [01 硬件架构](01-8x-H200训练推理/01-Hardware-Architecture.md) — GH100 die、HBM3e、NVLink 4.0、NVSwitch 拓扑
- [02 训练搭建](01-8x-H200训练推理/02-Training-Setup.md) — FSDP、DeepSpeed ZeRO-3、Megatron-LM、3D 并行
- [03 推理搭建](01-8x-H200训练推理/03-Inference-Setup.md) — vLLM、TensorRT-LLM、FP8、投机解码
- [04 内存管理](01-8x-H200训练推理/04-Memory-Management.md) — KV cache、PagedAttention、分组查询注意力、性能剖析
- [05 性能优化](01-8x-H200训练推理/05-Performance-Optimization.md) — roofline（性能上界模型）、CUDA Graphs、kernel 融合、NCCL 调优
- [06 Benchmark 与验证](01-8x-H200训练推理/06-Benchmarks-and-Validation.md) — MFU、MBU、延迟/吞吐目标

### L40S x12 — 主题
- [01 硬件架构](06-L40S-x12推理/01-Hardware-Architecture.md) — AD102 die、GDDR6、PCIe 拓扑、无 NVLink 约束
- [02 推理优化](06-L40S-x12推理/02-Inference-Optimization.md) — GPTQ、AWQ、FP8 量化、vLLM、连续批处理
- [03 多 GPU 策略](06-L40S-x12推理/03-Multi-GPU-Strategy.md) — PCIe 并行、流水线 vs 张量并行、InfiniBand
- [04 部署指南](06-L40S-x12推理/04-Deployment-Guide.md) — systemd、Docker、Kubernetes、NGINX 负载均衡、监控
- [05 Benchmark](06-L40S-x12推理/05-Benchmarks.md) — 吞吐表、L40S 与 H200 对比、负载测试

### NCCL 深入探索 — 主题
- [01 基础](08-NCCL深度探索/01-Fundamentals.md) — 配图讲解 AllReduce、广播、AllGather、ReduceScatter、AllToAll
- [02 算法与带宽](08-NCCL深度探索/02-Algorithms-and-Bandwidth.md) — Ring、Tree、Double Binary Tree 对比，900 GB/s 如何达成，带宽计算
- [03 框架集成](08-NCCL深度探索/03-Framework-Integration.md) — PyTorch DDP、FSDP、DeepSpeed ZeRO、Megatron 如何在内部调用 NCCL
- [04 配置与调优](08-NCCL深度探索/04-Configuration-and-Tuning.md) — 每个重要环境变量、按拓扑的 recipe（H200 / L40S / 多节点）
- [05 多节点集群](08-NCCL深度探索/05-Multi-Node-Clusters.md) — 分层 AllReduce、InfiniBand、GPUDirect RDMA、SHARP 网内计算
- [06 调试](08-NCCL深度探索/06-Debugging.md) — 挂起、报错、XID 码、容错、恢复模式
- [07 万亿参数规模](08-NCCL深度探索/07-Trillion-Parameter-Scale.md) — 3D 并行的 NCCL 模式、MoE AllToAll、大规模下的通信预算


<details>
<summary>English original</summary>

**HPC Setup**

<div class="course-identity auto-course" style="--course-accent: #ea580c; --course-accent-rgb: 234, 88, 12;" markdown="1">
<div class="course-identity__icon">HS</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Specialization</p>
<p class="course-identity__title">Specialized course identity for HPC Setup.</p>
<p class="course-identity__meta">Artifact: specialization case study · Measure: performance, reliability, role fit</p>
</div>
</div>


**Part of:** [High Performance Computing — Nvidia GPU](../Guide.md) (Phase 5)

This guide combines **HPC fundamentals, virtualization, interconnects, and advanced topics** with **hardware-specific deep dives** for real-world GPU cluster setup and optimization. Study the fundamentals first, then use the deep dives for your target hardware (8x H200, L40S, NCCL, CUDA, GDS).

---

**Hardware & Stack Deep Dives**

Detailed guides for specific GPU cluster configurations and subsystems:

| Setup | Use Case | Guide |
|-------|----------|-------|
| **8x H200 SXM5** | Large model training & inference (1.1 TB HBM3e, NVLink 4.0) | [8x-H200-Training-Inference/](01-8x-H200训练推理/README.md) |
| **L40S x12** | Cost-efficient inference deployment (576 GB GDDR6, PCIe) | [L40S-x12-Inference/](06-L40S-x12推理/README.md) |
| **NCCL Deep Dive** | GPU-to-GPU communication: algorithms, tuning, debugging, 1T-scale | [NCCL-Deep-Dive/](08-NCCL深度探索/README.md) |
| **CUDA Advanced Optimization** | CUDA Graphs, Cooperative Groups, Persistent Kernels, Fusion, Warp Specialization | [CUDA-Advanced-Optimization/](03-CUDA高级优化/README.md) |
| **GPUDirect Storage (GDS)** | Direct NVMe→GPU DMA, NVMe-oF, WD OpenFlex + RapidFlex, libcufile API | [GPUDirect-Storage/](05-GPUDirect存储/README.md) |
| **Long-Context MoE Foundation Training** | End-to-end course: long-context attention, MoE routing, mesh design, adaptive data, honest eval | [Long-Context-MoE-Foundation-Training/](07-长上下文MoE基础训练/README.md) |
| **Confidential Computing & Hardware Attestation** | Intel TDX + NVIDIA CC remote attestation for untrusted/rented/decentralized cluster nodes: DCAP quote verification, NRAS/JWKS, dual-vendor claim binding | [Confidential-Computing-Attestation/](04-机密计算认证/README.md) |

**8x H200 — Topics**
- [01 Hardware Architecture](01-8x-H200训练推理/01-Hardware-Architecture.md) — GH100 die, HBM3e, NVLink 4.0, NVSwitch topology
- [02 Training Setup](01-8x-H200训练推理/02-Training-Setup.md) — FSDP, DeepSpeed ZeRO-3, Megatron-LM, 3D parallelism
- [03 Inference Setup](01-8x-H200训练推理/03-Inference-Setup.md) — vLLM, TensorRT-LLM, FP8, speculative decoding
- [04 Memory Management](01-8x-H200训练推理/04-Memory-Management.md) — KV cache, PagedAttention, GQA, profiling
- [05 Performance Optimization](01-8x-H200训练推理/05-Performance-Optimization.md) — Roofline, CUDA Graphs, kernel fusion, NCCL tuning
- [06 Benchmarks & Validation](01-8x-H200训练推理/06-Benchmarks-and-Validation.md) — MFU, MBU, latency/throughput targets

**L40S x12 — Topics**
- [01 Hardware Architecture](06-L40S-x12推理/01-Hardware-Architecture.md) — AD102 die, GDDR6, PCIe topology, no-NVLink constraints
- [02 Inference Optimization](06-L40S-x12推理/02-Inference-Optimization.md) — GPTQ, AWQ, FP8 quantization, vLLM, continuous batching
- [03 Multi-GPU Strategy](06-L40S-x12推理/03-Multi-GPU-Strategy.md) — PCIe parallelism, pipeline vs tensor parallel, InfiniBand
- [04 Deployment Guide](06-L40S-x12推理/04-Deployment-Guide.md) — systemd, Docker, Kubernetes, NGINX load balancing, monitoring
- [05 Benchmarks](06-L40S-x12推理/05-Benchmarks.md) — throughput tables, L40S vs H200 comparison, load testing

**NCCL Deep Dive — Topics**
- [01 Fundamentals](08-NCCL深度探索/01-Fundamentals.md) — AllReduce, Broadcast, AllGather, ReduceScatter, AllToAll explained with diagrams
- [02 Algorithms & Bandwidth](08-NCCL深度探索/02-Algorithms-and-Bandwidth.md) — Ring vs Tree vs Double Binary Tree, how 900 GB/s is achieved, bandwidth math
- [03 Framework Integration](08-NCCL深度探索/03-Framework-Integration.md) — how PyTorch DDP, FSDP, DeepSpeed ZeRO, Megatron call NCCL internally
- [04 Configuration & Tuning](08-NCCL深度探索/04-Configuration-and-Tuning.md) — every important env var, per-topology recipes (H200 / L40S / multi-node)
- [05 Multi-Node Clusters](08-NCCL深度探索/05-Multi-Node-Clusters.md) — hierarchical AllReduce, InfiniBand, GPUDirect RDMA, SHARP in-network compute
- [06 Debugging](08-NCCL深度探索/06-Debugging.md) — hangs, errors, XID codes, fault tolerance, recovery patterns
- [07 Trillion-Parameter Scale](08-NCCL深度探索/07-Trillion-Parameter-Scale.md) — 3D parallelism NCCL patterns, MoE AllToAll, communication budgets at scale

</details>

### CUDA 高级优化 — 主题
- [01 CUDA Graphs](03-CUDA高级优化/01-CUDA-Graphs.md) — capture/replay 流水线，PyTorch 模式，动态形状分桶，性能剖析
- [02 Cooperative Groups](03-CUDA高级优化/02-Cooperative-Groups.md) — 线程块，warp，分块划分，合并访问，网格级同步及示例
- [03 Persistent Kernels](03-CUDA高级优化/03-Persistent-Kernels.md) — 常驻 GPU worker，GPU 侧工作队列，零开销调度
- [04 Kernel 融合](03-CUDA高级优化/04-Kernel-Fusion.md) — 消除 HBM 往返，Triton，torch.compile，FlashAttention 作为融合示例
- [05 warp 特化](03-CUDA高级优化/05-Warp-Specialization.md) — 生产者/消费者 warpgroup，TMA，WGMMA，软件流水线，CUTLASS 3.x

### GPUDirect Storage (GDS) — 主题
- [01 架构与数据路径](05-GPUDirect存储/01-Architecture-and-Data-Path.md) — CPU vs GDS 数据路径，PCIe 拓扑，NUMA 绑定，3 种传输模式
- [02 硬件设置](05-GPUDirect存储/02-Hardware-Setup.md) — WD OpenFlex 参考配置 (A100 + CX-7 + SN3700)，PCIe 布局布线，版本矩阵
- [03 软件栈](05-GPUDirect存储/03-Software-Stack.md) — OFED 5.8，GDS 2.17.3，libcufile 安装，gdscheck 验证，cufile.json 配置
- [04 libcufile API](05-GPUDirect存储/04-libcufile-API.md) — cuFileRead/Write，缓冲区注册，批 I/O，PyTorch DataLoader 集成
- [05 性能调优](05-GPUDirect存储/05-Performance-Tuning.md) — 512 字节对齐，最优传输大小，队列深度，缓冲池，benchmarks
- [06 解耦存储](05-GPUDirect存储/06-Disaggregated-Storage.md) — NVMe-oF over RoCEv2，WD OpenFlex + RapidFlex，75 GB/s 横向扩展，无损配置

### 机密计算与硬件远程证明 — 主题
- [01 Intel TDX 远程证明链](04-机密计算认证/01-Intel-TDX-Attestation-Chain.md) — TDX Module RTM，MRTD/RTMR/REPORTDATA，TD Report → Quoting Enclave → TD Quote，DCAP 验证链
- [02 NVIDIA CC 远程证明链](04-机密计算认证/02-NVIDIA-CC-Attestation-Chain.md) — GPU EAT tokens，eat_nonce 绑定，NRAS/JWKS 签名验证，对称密钥自证明陷阱
- [03 双厂商声明绑定与验证审查](04-机密计算认证/03-Dual-Vendor-Claim-Binding-and-Verification.md) — 组合两条链，绑定与真实性审查准则，威胁模型，验证检查清单

---

## 1. Nvidia GPU 高性能计算（HPC）基础

* **面向 HPC 的 GPU 架构：**
    * **CUDA 和 Tensor Core：** 掌握面向 HPC 工作负载的 CUDA 编程。理解科学和 AI 应用中混合精度计算（FP16、BF16、TF32）的 Tensor Core 利用率。
    * **NVLink 和 NVSwitch：** 学习面向多 GPU 系统的高带宽 GPU 互连技术。理解 NVLink 拓扑、用于可扩展 GPU 集群的 NVSwitch 以及带宽优化。
    * **GPU 存储层次：** 深入探究 GPU 内存——全局内存、共享内存、L1/L2 缓存和统一内存。优化面向 HPC 工作负载的内存访问模式。

* **多 GPU 编程：**
    * **NCCL（Nvidia Collective Communications Library）：** 掌握 NCCL 以实现高效的多 GPU 和多节点集合操作——全规约、广播、全收集。理解 NCCL 拓扑检测和调优以实现最优性能。
    * **CUDA 多进程服务（MPS）：** 学习 MPS 以跨多个进程共享 GPU，提高 HPC 和推理工作负载中的利用率。
    * **MPI + CUDA：** 将用于分布式计算的 MPI 与用于 GPU 加速的 CUDA 结合。为大规模 HPC 集群实现混合 MPI-CUDA 应用。

**资源：** Nvidia NCCL Documentation · Nvidia vGPU Documentation · "Professional CUDA C Programming" by Cheng et al.

**项目：** 实现使用 NCCL 全规约的多 GPU 训练流水线；对 NVLink 与 PCIe 进行 benchmark。

---


<details>
<summary>English original</summary>

**CUDA Advanced Optimization — Topics**
- [01 CUDA Graphs](03-CUDA高级优化/01-CUDA-Graphs.md) — capture/replay pipelines, PyTorch patterns, bucketing for dynamic shapes, profiling
- [02 Cooperative Groups](03-CUDA高级优化/02-Cooperative-Groups.md) — thread block, warp, tiled partition, coalesced, grid-wide sync with examples
- [03 Persistent Kernels](03-CUDA高级优化/03-Persistent-Kernels.md) — always-resident GPU workers, GPU-side work queues, zero-overhead dispatch
- [04 Kernel Fusion](03-CUDA高级优化/04-Kernel-Fusion.md) — HBM round-trip elimination, Triton, torch.compile, FlashAttention as fusion example
- [05 Warp Specialization](03-CUDA高级优化/05-Warp-Specialization.md) — producer/consumer warpgroups, TMA, WGMMA, software pipelining, CUTLASS 3.x

**GPUDirect Storage (GDS) — Topics**
- [01 Architecture & Data Path](05-GPUDirect存储/01-Architecture-and-Data-Path.md) — CPU vs GDS data paths, PCIe topology, NUMA pinning, 3 transport modes
- [02 Hardware Setup](05-GPUDirect存储/02-Hardware-Setup.md) — WD OpenFlex reference config (A100 + CX-7 + SN3700), PCIe layout, version matrix
- [03 Software Stack](05-GPUDirect存储/03-Software-Stack.md) — OFED 5.8, GDS 2.17.3, libcufile install, gdscheck verification, cufile.json config
- [04 libcufile API](05-GPUDirect存储/04-libcufile-API.md) — cuFileRead/Write, buffer registration, batch I/O, PyTorch DataLoader integration
- [05 Performance Tuning](05-GPUDirect存储/05-Performance-Tuning.md) — 512-byte alignment, optimal transfer size, queue depth, buffer pool, benchmarks
- [06 Disaggregated Storage](05-GPUDirect存储/06-Disaggregated-Storage.md) — NVMe-oF over RoCEv2, WD OpenFlex + RapidFlex, 75 GB/s scale-out, lossless config

**Confidential Computing & Hardware Attestation — Topics**
- [01 Intel TDX Attestation Chain](04-机密计算认证/01-Intel-TDX-Attestation-Chain.md) — TDX Module RTM, MRTD/RTMR/REPORTDATA, TD Report → Quoting Enclave → TD Quote, DCAP verification chain
- [02 NVIDIA CC Attestation Chain](04-机密计算认证/02-NVIDIA-CC-Attestation-Chain.md) — GPU EAT tokens, eat_nonce binding, NRAS/JWKS signature verification, the symmetric-key self-attestation trap
- [03 Dual-Vendor Claim Binding & Verification Review](04-机密计算认证/03-Dual-Vendor-Claim-Binding-and-Verification.md) — composing both chains, binding-vs-authenticity review discipline, threat model, verification checklist

---

**1. Nvidia GPU HPC Fundamentals**

* **GPU Architecture for HPC:**
    * **CUDA and Tensor Cores:** Master CUDA programming for HPC workloads. Understand Tensor Core utilization for mixed-precision compute (FP16, BF16, TF32) in scientific and AI applications.
    * **NVLink and NVSwitch:** Learn about high-bandwidth GPU interconnect technologies for multi-GPU systems. Understand NVLink topology, NVSwitch for scalable GPU clusters, and bandwidth optimization.
    * **GPU Memory Hierarchy:** Deep dive into GPU memory—global memory, shared memory, L1/L2 cache, and unified memory. Optimize memory access patterns for HPC workloads.

* **Multi-GPU Programming:**
    * **NCCL (Nvidia Collective Communications Library):** Master NCCL for efficient multi-GPU and multi-node collective operations—all-reduce, broadcast, all-gather. Understand NCCL topology detection and tuning for optimal performance.
    * **CUDA Multi-Process Service (MPS):** Learn MPS for sharing GPUs across multiple processes, improving utilization in HPC and inference workloads.
    * **MPI + CUDA:** Combine MPI for distributed computing with CUDA for GPU acceleration. Implement hybrid MPI-CUDA applications for large-scale HPC clusters.

**Resources:** Nvidia NCCL Documentation · Nvidia vGPU Documentation · "Professional CUDA C Programming" by Cheng et al.

**Projects:** Implement a Multi-GPU Training Pipeline with NCCL all-reduce; benchmark NVLink vs. PCIe.

---

</details>

## 2. 虚拟化与云端高性能计算（HPC）（vGPU、KVM）

* **Nvidia vGPU（虚拟 GPU）：**
    * **vGPU 架构：** 理解用于在多个虚拟机之间共享物理 GPU 的 vGPU 技术。了解 vGPU 类型（如 vComputeServer、vPC、vApp）与许可模式。
    * **vGPU 部署：** 在 hypervisor 上部署并配置 vGPU。理解 GPU 分区、时间分片，以及用于细粒度共享的 MIG（Multi-Instance GPU）。
    * **面向高性能计算与 AI 的 vGPU：** 为虚拟化数据中心中的高性能计算工作负载、ML 训练与推理配置 vGPU 环境。

* **KVM 与 GPU 直通：**
    * **GPU 直通（VFIO）：** 学习通过 PCIe 直通把物理 GPU 独占分配给 VM。理解 IOMMU 组、VFIO 驱动，以及用于 GPU 虚拟化的 SR-IOV。
    * **搭配 Nvidia GPU 的 KVM：** 配置基于 KVM 并使用 Nvidia GPU 的虚拟化。探索嵌套虚拟化与 GPU 资源管理。
    * **编排：** 把 GPU VM 与 Kubernetes、Slurm 或其他高性能计算作业调度器集成，以实现资源分配。

* **面向高性能计算的容器化：**
    * **Nvidia Container Toolkit：** 使用 Nvidia Container Toolkit 在 Docker 与 Podman 容器中运行 GPU 工作负载。
    * **Singularity/Apptainer：** 用 Singularity/Apptainer 在共享集群中部署 GPU 加速的容器化高性能计算应用。

**资源：** Nvidia vGPU Software Documentation · Linux VFIO and IOMMU Documentation · Nvidia Container Toolkit。

**项目：** 部署一个 vGPU 环境；用 KVM（VFIO）配置 GPU 直通。

---

## 3. 高性能计算互连与存储

* **高速互连：**
    * **InfiniBand：** 掌握 InfiniBand，以构建低延迟、高带宽的高性能计算网络。理解 RDMA（Remote Direct Memory Access）、GPUDirect RDMA 与拓扑设计。
    * **RoCE（RDMA over Converged Ethernet）：** 探索 RoCE，在高性能计算与云环境中实现基于以太网的 RDMA。
    * **GPUDirect Storage：** 学习 GPUDirect Storage（GDS），实现 GPU 到 NVMe 的直接数据访问，为 I/O 密集型工作负载绕过 CPU。*参见上文 [GPUDirect-Storage](05-GPUDirect存储/README.md) 深入剖析。*

* **并行文件系统与 I/O：**
    * **Lustre 与 GPFS：** 理解用于高性能计算存储的并行文件系统。为大规模科学应用优化 I/O 模式。
    * **DAOS（Distributed Asynchronous Object Storage）：** 探索用于下一代高性能计算存储、原生支持 GPU 的 DAOS。

* **作业调度与编排：**
    * **支持 GPU 的 Slurm：** 为 GPU 资源管理、GRES（Generic Resources）与多节点 GPU 作业配置 Slurm。
    * **面向高性能计算/AI 的 Kubernetes：** 使用 Kubernetes 配合 Nvidia GPU operator，在混合高性能计算/云环境中编排 GPU 工作负载。

**资源：** Nvidia GPUDirect Documentation · Slurm GPU Configuration · TOP500 and Green500。

**项目：** 用 InfiniBand 或高速以太网、NCCL 与 Slurm 搭建多节点 GPU 集群；用 GPUDirect Storage 优化 I/O。

---

## 阶段 2：高级高性能计算（24–48 个月）

### 1. 面向高性能计算的高级 CUDA 编程

* **CUDA 内存优化：** 内存访问合并、共享内存与 L1 缓存、锁页内存与统一内存。用 Nsight Compute 分析；重构布局（AoS → SoA）。
* **warp 级与线程级优化：** warp 发散、warp shuffle 内建函数（`__shfl_sync`、`__ballot_sync`、`__reduce_sync`）、张量核心（WMMA/CUTLASS）。
* **CUDA Graphs 与流：** 用流重叠计算与传输；用 CUDA Graphs 捕获/回放；cooperative groups。*参见 [CUDA-Advanced-Optimization](03-CUDA高级优化/README.md) 深入剖析。*

**资源：** Nsight Compute 与 Nsight Systems · CUTLASS · Shane Cook 著 "CUDA Programming"。

**项目：** 优化 GEMM（矩阵-矩阵乘）kernel（分块、共享内存、张量核心）并与 cuBLAS 对比；用流实现重叠流水线；用 CUDA Graph 做推理。

---

### 2. 分布式训练与大规模 AI

* **并行训练策略：** 数据并行（NCCL 全规约）、模型并行（张量 + 流水线）、3D 并行（DeepSpeed + Megatron）。
* **框架与基础设施：** PyTorch DDP 与 FSDP、DeepSpeed ZeRO（1/2/3）、Megatron-LM。*参见 [8x-H200 Training Setup](01-8x-H200训练推理/02-Training-Setup.md) 与 [NCCL](08-NCCL深度探索/03-Framework-Integration.md)。*
* **监控与容错：** Weights & Biases / TensorBoard；分布式检查点；用 PyTorch Elastic 处理节点故障与规模调整。

**资源：** DeepSpeed Documentation · Megatron-LM GitHub · "Efficient Large-Scale Language Model Training on GPU Clusters Using Megatron-LM"（论文）。

**项目：** 3D 并行训练运行（>1B 参数）；ZeRO-3 内存分析；用 Elastic 做容错训练。

---


<details>
<summary>English original</summary>

**2. Virtualization and Cloud HPC (vGPU, KVM)**

* **Nvidia vGPU (Virtual GPU):**
    * **vGPU Architecture:** Understand vGPU technology for sharing physical GPUs across multiple virtual machines. Learn vGPU types (e.g., vComputeServer, vPC, vApp) and licensing.
    * **vGPU Deployment:** Deploy and configure vGPU on hypervisors. Understand GPU partitioning, time-slicing, and MIG (Multi-Instance GPU) for fine-grained sharing.
    * **vGPU for HPC and AI:** Configure vGPU environments for HPC workloads, ML training, and inference in virtualized data centers.

* **KVM and GPU Passthrough:**
    * **GPU Passthrough (VFIO):** Learn PCIe passthrough for dedicating physical GPUs to VMs. Understand IOMMU groups, VFIO drivers, and SR-IOV for GPU virtualization.
    * **KVM with Nvidia GPUs:** Configure KVM-based virtualization with Nvidia GPUs. Explore nested virtualization and GPU resource management.
    * **Orchestration:** Integrate GPU VMs with Kubernetes, Slurm, or other HPC job schedulers for resource allocation.

* **Containerization for HPC:**
    * **Nvidia Container Toolkit:** Use the Nvidia Container Toolkit to run GPU workloads in Docker and Podman containers.
    * **Singularity/Apptainer:** Deploy HPC applications with Singularity/Apptainer for GPU-accelerated containerized workloads in shared clusters.

**Resources:** Nvidia vGPU Software Documentation · Linux VFIO and IOMMU Documentation · Nvidia Container Toolkit.

**Projects:** Deploy a vGPU environment; configure GPU passthrough with KVM (VFIO).

---

**3. HPC Interconnects and Storage**

* **High-Speed Interconnects:**
    * **InfiniBand:** Master InfiniBand for low-latency, high-bandwidth HPC networking. Understand RDMA (Remote Direct Memory Access), GPUDirect RDMA, and topology design.
    * **RoCE (RDMA over Converged Ethernet):** Explore RoCE for Ethernet-based RDMA in HPC and cloud environments.
    * **GPUDirect Storage:** Learn GPUDirect Storage (GDS) for direct GPU-to-NVMe data access, bypassing CPU for I/O-bound workloads. *See [GPUDirect-Storage](05-GPUDirect存储/README.md) deep dive above.*

* **Parallel File Systems and I/O:**
    * **Lustre and GPFS:** Understand parallel file systems for HPC storage. Optimize I/O patterns for large-scale scientific applications.
    * **DAOS (Distributed Asynchronous Object Storage):** Explore DAOS for next-generation HPC storage with native GPU support.

* **Job Scheduling and Orchestration:**
    * **Slurm with GPU Support:** Configure Slurm for GPU resource management, GRES (Generic Resources), and multi-node GPU jobs.
    * **Kubernetes for HPC/AI:** Use Kubernetes with Nvidia GPU operator for orchestrating GPU workloads in hybrid HPC/cloud environments.

**Resources:** Nvidia GPUDirect Documentation · Slurm GPU Configuration · TOP500 and Green500.

**Projects:** Build a multi-node GPU cluster with InfiniBand or high-speed Ethernet, NCCL, and Slurm; optimize I/O with GPUDirect Storage.

---

**Phase 2: Advanced HPC (24–48 months)**

**1. Advanced CUDA Programming for HPC**

* **CUDA Memory Optimization:** Memory access coalescing, shared memory and L1 cache, pinned and unified memory. Analyze with Nsight Compute; restructure layouts (AoS → SoA).
* **Warp-Level and Thread-Level Optimization:** Warp divergence, warp shuffle intrinsics (`__shfl_sync`, `__ballot_sync`, `__reduce_sync`), Tensor Cores (WMMA/CUTLASS).
* **CUDA Graphs and Streams:** Overlap compute and transfer with streams; capture/replay with CUDA Graphs; cooperative groups. *See [CUDA-Advanced-Optimization](03-CUDA高级优化/README.md) deep dive.*

**Resources:** Nsight Compute and Nsight Systems · CUTLASS · "CUDA Programming" by Shane Cook.

**Projects:** Optimize a GEMM kernel (tiling, shared memory, Tensor Cores) vs cuBLAS; overlapped pipeline with streams; CUDA Graph for inference.

---

**2. Distributed Training and Large-Scale AI**

* **Parallel Training Strategies:** Data parallelism (NCCL all-reduce), model parallelism (tensor + pipeline), 3D parallelism (DeepSpeed + Megatron).
* **Frameworks and Infrastructure:** PyTorch DDP and FSDP, DeepSpeed ZeRO (1/2/3), Megatron-LM. *See [8x-H200 Training Setup](01-8x-H200训练推理/02-Training-Setup.md) and [NCCL](08-NCCL深度探索/03-Framework-Integration.md).*
* **Monitoring and Fault Tolerance:** Weights & Biases / TensorBoard; distributed checkpointing; PyTorch Elastic for node failure and resizing.

**Resources:** DeepSpeed Documentation · Megatron-LM GitHub · "Efficient Large-Scale Language Model Training on GPU Clusters Using Megatron-LM" (paper).

**Projects:** 3D-parallel training run (>1B params); ZeRO-3 memory analysis; fault-tolerant training with Elastic.

---

</details>

### 3. 高性能计算性能建模与应用优化

* **Roofline 模型与性能分析：** 算力受限 vs 带宽受限；算术强度（FLOPs/Byte）；分层存储 roofline（性能上界模型）（L1、L2、HBM）。*参见 [8x-H200 Performance Optimization](01-8x-H200训练推理/05-Performance-Optimization.md)。*
* **科学计算应用：** 分子动力学（GROMACS、AMBER）、CFD（稀疏求解器、CG、GMRES）、用于基于 FFT 应用的 cuFFT。
* **编译器与自动调优：** 面向 GPU 的 TVM、用于自定义 kernel 的 Triton、cuBLAS/cuDNN 调优（workspace、算法选择、math modes）。

**资源：** Nsight Compute Roofline Analysis ·《Programming Massively Parallel Processors》（Kirk & Hwu）· OpenAI Triton Documentation。

**项目：** 对某个科学 kernel 做 roofline 分析；自定义 Triton kernel（例如 layer norm + dropout）；自动调优的 cuFFT 流水线。


<details>
<summary>English original</summary>

**3. HPC Performance Modeling and Application Optimization**

* **Roofline Model and Performance Analysis:** Compute-bound vs memory-bound; arithmetic intensity (FLOPs/Byte); hierarchical memory rooflines (L1, L2, HBM). *See [8x-H200 Performance Optimization](01-8x-H200训练推理/05-Performance-Optimization.md).*
* **Scientific Computing Applications:** Molecular dynamics (GROMACS, AMBER), CFD (sparse solvers, CG, GMRES), cuFFT for FFT-based applications.
* **Compiler and Auto-Tuning:** TVM for GPU, Triton for custom kernels, cuBLAS/cuDNN tuning (workspace, algorithm selection, math modes).

**Resources:** Nsight Compute Roofline Analysis · "Programming Massively Parallel Processors" (Kirk & Hwu) · OpenAI Triton Documentation.

**Projects:** Roofline analysis of a scientific kernel; custom Triton kernel (e.g. layer norm + dropout); auto-tuned cuFFT pipeline.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
