---
title: C++ 与并行计算（阶段 1 §4）
description: C++ 与并行计算（阶段 1 §4）
---

# C++ 与并行计算（阶段 1 §4）

<div class="course-identity parallel-computing" markdown="1">
<div class="course-identity__icon">CUDA</div>
<div markdown="1">
<p class="course-identity__eyebrow">模块 4 · C++ 与并行计算</p>
<p class="course-identity__title">将工作映射到 CPU SIMD、线程池、CUDA、HIP、OpenCL 和 SYCL 执行模型。</p>
<p class="course-identity__meta">产物：并行 benchmark · 测量：加速比、occupancy、带宽、同步</p>
</div>
</div>


> *从一条指令到一千个 GPU 线程——计算如何变得并行，以及为什么每一位 AI 硬件工程师都必须以并行方式思考。*

**layer 映射：** **L1**（应用——你编写在硬件上运行的代码），**L3**（runtime——CUDA runtime、OpenCL runtime 是通往 GPU 驱动的桥梁）。

**前置要求：** 阶段 1 §2（计算机体系结构——存储层次、流水线、缓存），阶段 1 §3（操作系统——线程、进程、调度）。

**后续内容：** 阶段 3（神经网络——张量直接映射到这些执行模型），阶段 4 方向 B（Jetson 上带功耗限制的 CUDA），阶段 4 方向 A（HLS/RTL——你设计这些 kernel 运行的硬件）。

---

## 本模块为何存在

每一款 AI 推理芯片——从 NVIDIA 的 H100 到你未来定制的 NPU——之所以存在，是因为**顺序计算撞了墙**。理解并行*为什么*必要、*如何*演化，以及*如何*编程，是本路线图中一切内容的基础。

如果你不能以并行方式思考，就无法设计运行并行工作负载的硬件。

---

## 学习成果

完成本模块后，你应当能够：

- 解释为什么现代 AI 系统是吞吐导向而非时钟频率导向
- 在 SIMD、CPU 多核和 GPU 执行模型上编写并调试并行代码
- 思考内存局部性、同步、occupancy 和带宽瓶颈
- 衡量正确实现与高性能实现之间的差异
- 产出至少一个展示真实底层性能工作的作品集产物

---

## 如何学习本模块

不要把这当作五个孤立的阅读小节。按顺序学习：

1. 学习执行模型
2. 实现一个小型 kernel 或并行工作负载
3. 测量加速比并找出限制瓶颈
4. 写下改变了什么以及为什么

对于每个子方向，以可见的产物收尾。好的产出包括：

- 向量化 benchmark 表格
- OpenMP 扩展性图
- Nsight 截图
- occupancy 计算
- HIP 移植笔记
- SYCL 可移植性比较笔记

---

## 第 1 部分——计算如何变得并行

在深入代码之前，先理解创造你将设计的硬件的那些历史压力。

### 步骤 1：顺序 → 时钟速度墙

几十年来，性能提升来自时钟速度扩展（1980 年的 4.77 MHz → 2004 年的 3+ GHz）。随后**功耗墙**袭来：功耗与 `P ∝ V² × f` 成正比，芯片无法散发热量。时钟速度停滞在 3–4 GHz。

**这就是 AI 芯片存在的原因。** 如果时钟速度仍然自由扩展，你只需在更快的 CPU 上运行一切即可。功耗墙迫使行业走向并行和专用化。

### 步骤 2：指令级并行（ILP / SIMD）

单个核内每个时钟周期做多件事：

```
Pipelining:   overlap fetch/decode/execute of different instructions
Superscalar:  issue 4–6 independent instructions per cycle
SIMD:         one instruction processes 4/8/16/32 data elements

Without SIMD:       With SIMD (8-wide AVX2):
a[0] = b[0] + c[0]  a[0..7] = b[0..7] + c[0..7]   ← ONE instruction
a[1] = b[1] + c[1]
...                  8× throughput improvement
a[7] = b[7] + c[7]
```

这是对**数据并行**的初次体验——GPU 将同一概念推向极致。

### 步骤 3：多核 / 共享内存

当时钟速度停止扩展时，芯片设计者增加了核（2005 年：2 核 → 2024 年：96 核）。但软件必须显式使用多个核——单线程程序会让 95 个核闲置。

```
Core 0: Process chunk [0..N/4]
Core 1: Process chunk [N/4..N/2]     ← all running simultaneously
Core 2: Process chunk [N/2..3N/4]
Core 3: Process chunk [3N/4..N]
```

挑战：共享内存意味着**竞态条件**、**死锁**和**缓存一致性**。

### 步骤 4：异构计算（CPU + GPU）

**针对特定工作负载类型的专用硬件。**

| | CPU | GPU |
|---|---|---|
| 核 | 4–96（复杂） | 1,000–16,000（简单） |
| 优化目标 | 延迟（单个任务快） | 吞吐（同时多个任务） |
| 控制逻辑 | ~50% 的芯片面积 | ~5% 的芯片面积 |
| 每核缓存 | 大（MB） | 小（KB） |
| 最适合 | 顺序代码、分支 | 数据并行：矩阵运算、卷积 |

神经网络推理几乎完全是矩阵乘法——完美地数据并行。在相同功耗下，GPU 每秒处理的乘累加运算比 CPU 多 1,000 倍。

---


<details>
<summary>English original</summary>

**C++ and Parallel Computing (Phase 1 §4)**

<div class="course-identity parallel-computing" markdown="1">
<div class="course-identity__icon">CUDA</div>
<div markdown="1">
<p class="course-identity__eyebrow">Module 4 · C++ & Parallel Computing</p>
<p class="course-identity__title">Map work onto CPU SIMD, thread pools, CUDA, HIP, OpenCL, and SYCL execution models.</p>
<p class="course-identity__meta">Artifact: parallel benchmark · Measure: speedup, occupancy, bandwidth, synchronization</p>
</div>
</div>


> *From a single instruction to a thousand GPU threads — how computing became parallel, and why every AI hardware engineer must think in parallel.*

**Layer mapping:** **L1** (application — you write the code that runs on hardware), **L3** (runtime — CUDA runtime, OpenCL runtime are the bridge to the GPU driver).

**Prerequisites:** Phase 1 §2 (Computer Architecture — memory hierarchy, pipelining, caches), Phase 1 §3 (Operating Systems — threads, processes, scheduling).

**What comes after:** Phase 3 (Neural Networks — tensors map directly to these execution models), Phase 4 Track B (CUDA on Jetson with power limits), Phase 4 Track A (HLS/RTL — you design the hardware these kernels run on).

---

**Why This Module Exists**

Every AI inference chip — from NVIDIA's H100 to your future custom NPU — exists because **sequential computing hit a wall**. Understanding *why* parallelism is necessary, *how* it evolved, and *how to program it* is the foundation for everything in this roadmap.

If you can't think in parallel, you can't design hardware that runs parallel workloads.

---

**Learning Outcomes**

By the end of this module, you should be able to:

- explain why modern AI systems are throughput-oriented rather than clock-speed-oriented
- write and debug parallel code across SIMD, CPU multicore, and GPU execution models
- reason about memory locality, synchronization, occupancy, and bandwidth bottlenecks
- measure the difference between a correct implementation and a performant one
- produce at least one portfolio artifact that demonstrates real low-level performance work

---

**How To Work This Module**

Do not treat this as five isolated reading sections. Work it as a sequence:

1. Learn the execution model
2. Implement a small kernel or parallel workload
3. Measure speedup and identify the limiting bottleneck
4. Write down what changed and why

For every sub-track, finish with a visible artifact. Good outputs include:

- vectorization benchmark tables
- OpenMP scaling plots
- Nsight screenshots
- occupancy calculations
- HIP porting notes
- SYCL portability comparison notes

---

**Part 1 — How Computing Became Parallel**

Before diving into code, understand the historical pressure that created the hardware you'll design.

**Step 1: Sequential → Clock Speed Wall**

For decades, performance came from clock speed scaling (4.77 MHz in 1980 → 3+ GHz by 2004). Then the **power wall** hit: power scales as `P ∝ V² × f`, and chips couldn't dissipate the heat. Clock speeds plateaued at 3–4 GHz.

**This is why AI chips exist.** If clock speed still scaled freely, you'd just run everything on a faster CPU. The power wall forced the industry into parallelism and specialization.

**Step 2: Instruction-Level Parallelism (ILP / SIMD)**

Multiple things per clock cycle inside a single core:

```
Pipelining:   overlap fetch/decode/execute of different instructions
Superscalar:  issue 4–6 independent instructions per cycle
SIMD:         one instruction processes 4/8/16/32 data elements

Without SIMD:       With SIMD (8-wide AVX2):
a[0] = b[0] + c[0]  a[0..7] = b[0..7] + c[0..7]   ← ONE instruction
a[1] = b[1] + c[1]
...                  8× throughput improvement
a[7] = b[7] + c[7]
```

This is the first taste of **data parallelism** — the same concept that GPUs take to the extreme.

**Step 3: Multi-Core / Shared Memory**

When clock speed stopped scaling, chip designers added cores (2005: 2 cores → 2024: 96 cores). But software must explicitly use multiple cores — a single-threaded program leaves 95 cores idle.

```
Core 0: Process chunk [0..N/4]
Core 1: Process chunk [N/4..N/2]     ← all running simultaneously
Core 2: Process chunk [N/2..3N/4]
Core 3: Process chunk [3N/4..N]
```

The challenge: shared memory means **race conditions**, **deadlocks**, and **cache coherence**.

**Step 4: Heterogeneous Computing (CPU + GPU)**

**Specialized hardware for specific workload types.**

| | CPU | GPU |
|---|---|---|
| Cores | 4–96 (complex) | 1,000–16,000 (simple) |
| Optimized for | Latency (one task fast) | Throughput (many tasks at once) |
| Control logic | ~50% of die area | ~5% of die area |
| Cache per core | Large (MB) | Small (KB) |
| Best for | Sequential code, branching | Data-parallel: matrix math, convolution |

Neural network inference is almost entirely matrix multiplication — perfectly data-parallel. A GPU processes 1,000× more multiply-accumulate operations per second than a CPU at the same power.

---

</details>

## 第 2 部分 — 五个子赛道

按**顺序**学习这些内容。每个都建立在前一个之上。

| # | 子赛道 | 学习内容 | 指南 |
|:-:|-----------|---------------|-------|
| 1 | **C++ 与 SIMD** | 用于并行代码的现代 C++17、SIMD intrinsics（SSE/AVX/NEON）、自动向量化、AoS vs SoA、roofline（性能上界模型） | [指南 →](01-C++与SIMD/Guide.md) |
| 2 | **OpenMP 与 oneTBB** | 多核 CPU 并行、fork-join、归约、任务、工作窃取、流图 | [指南 →](04-OpenMP与OneTBB/Guide.md) |
| 3 | **CUDA 与 SIMT** | NVIDIA GPU 编程——线程层次、内存空间、分块、流、Tensor Core、性能剖析 | [指南 →](02-CUDA与SIMT/Guide.md) |
| 4 | **ROCm 与 HIP** | AMD GPU 编程——CDNA 架构、HIP API、HIPIFY 移植、Matrix Cores、RCCL 多 GPU | [指南 →](05-ROCm与HIP/Guide.md) |
| 5 | **OpenCL 与 SYCL** | 厂商中立计算——OpenCL 平台模型、SYCL 现代 C++、子组、FPGA pipes | [指南 →](03-OpenCL与SYCL/Guide.md) |

---

## 构建、度量、交付

使用此完成标准，而非止步于“我读过指南了”。

| 子赛道 | 构建 | 度量 | 交付 |
|-----------|-------|---------|------|
| C++ 与 SIMD | 向量化数学 kernel | 标量 vs SIMD 加速比、缓存行为 | 简短 benchmark 报告 |
| OpenMP / oneTBB | 多核循环或流水线 | 跨核心数的扩展性、负载不均衡 | 图表 + 调度选择说明 |
| CUDA | 至少一个分块 GPU kernel | occupancy、内存吞吐、kernel 时间线 | Nsight 性能剖析与调优后的 kernel |
| ROCm / HIP | CUDA kernel 的 HIP 移植 | 正确性 + 相对 CUDA 的性能差异 | 可移植性说明 |
| OpenCL / SYCL | 一个可移植的并行 kernel | 后端/设备比较 | 可移植性取舍总结 |

如果整个模块只完成一个产物，就选 CUDA 那个。

---

## 达成标准

当无需猜测即可完成以下全部事项时，就可以继续前进：

- 针对简单工作负载，在 SIMD、CPU 线程与 GPU 执行之间做选择
- 从计算、内存或同步的角度解释 kernel 的瓶颈
- 阅读性能分析器输出，并找出一个可操作的优化
- 将 kernel 级行为与下游 AI 工作负载（如矩阵乘、attention 或卷积）联系起来

这是阶段 3 和阶段 4 发挥作用的最低门槛。

---

### 子赛道 1：C++ 与 SIMD

> *GPU 思维的网关——同一操作，多个数据。*

CPU 向量指令，每条指令处理 4–16+ 个数据元素。GPU 将同一概念扩展到 32 宽 warp。

| ISA | 扩展 | 宽度 | 每指令 FP32 元素数 |
|-----|-----------|-------|---------------------------|
| x86 | SSE4.2 | 128 bits | 4 |
| x86 | AVX2 | 256 bits | 8 |
| x86 | AVX-512 | 512 bits | 16 |
| ARM | NEON | 128 bits | 4 |
| ARM | SVE2 | 128–2048 bits | 4–64（可伸缩） |

**指南涵盖内容：**
- 用于并行计算的现代 C++17（lambda、移动语义、智能指针、并行 STL、模板、constexpr）
- 自动向量化与手动 intrinsics（`_mm256_fmadd_ps`、`_mm256_load_ps` 等）
- 最常用的 10 个 intrinsics 及独立示例
- 用于向量化的 AoS vs SoA 数据布局
- 缓存对齐、跨 lane 限制、调试 SIMD 寄存器
- roofline 模型与 SIMD → GPU 心智模型桥梁
- 9 个动手项目

---

### 子赛道 2：OpenMP 与 oneTBB

> *从一个核心扩展到多个核心——CPU 并行层。*

**OpenMP**——向循环添加 `#pragma omp parallel for`，即可将其分配到所有核心。零样板代码。

**oneTBB**——基于任务的并行，带工作窃取调度器。更适合不规则工作负载（树遍历、图算法、嵌套并行）。

| | OpenMP | oneTBB |
|---|---|---|
| 模型 | Pragma 注解 | C++ 模板库 |
| 易用性 | 一行改动 | 中等（lambda + ranges） |
| 负载均衡 | 静态/动态（循环级） | 工作窃取（任务级） |
| 最适合 | 规则循环 | 不规则并行、流水线、流图 |

**指南涵盖内容：**
- OpenMP fork-join、调度（static/dynamic/guided）、归约、自定义归约、屏障、sections、任务
- Fibonacci benchmark：串行 vs OpenMP vs oneTBB，含真实测量数据
- oneTBB parallel_for、parallel_reduce、parallel_scan（exclusive scan、流压缩、prefix max、CDF）
- parallel_pipeline 与 4 阶段批推理示例
- parallel_invoke、task_group、enumerable_thread_specific（ETS）
- 流图：拓扑图、ML 特征提取、完整视频推理流水线模板
- 工作窃取调度器深入剖析


<details>
<summary>English original</summary>

**Part 2 — The Five Sub-Tracks**

Study these **in order**. Each builds on the previous.

| # | Sub-track | What you learn | Guide |
|:-:|-----------|---------------|-------|
| 1 | **C++ and SIMD** | Modern C++17 for parallel code, SIMD intrinsics (SSE/AVX/NEON), auto-vectorization, AoS vs SoA, roofline | [Guide →](01-C++与SIMD/Guide.md) |
| 2 | **OpenMP and oneTBB** | Multi-core CPU parallelism, fork-join, reductions, tasks, work-stealing, flow graphs | [Guide →](04-OpenMP与OneTBB/Guide.md) |
| 3 | **CUDA and SIMT** | NVIDIA GPU programming — thread hierarchy, memory spaces, tiling, streams, Tensor Cores, profiling | [Guide →](02-CUDA与SIMT/Guide.md) |
| 4 | **ROCm and HIP** | AMD GPU programming — CDNA architecture, HIP API, HIPIFY porting, Matrix Cores, RCCL multi-GPU | [Guide →](05-ROCm与HIP/Guide.md) |
| 5 | **OpenCL and SYCL** | Vendor-neutral compute — OpenCL platform model, SYCL modern C++, sub-groups, FPGA pipes | [Guide →](03-OpenCL与SYCL/Guide.md) |

---

**Build, Measure, Ship**

Use this completion rubric instead of stopping at "I read the guide."

| Sub-track | Build | Measure | Ship |
|-----------|-------|---------|------|
| C++ and SIMD | vectorized math kernel | scalar vs SIMD speedup, cache behavior | short benchmark report |
| OpenMP / oneTBB | multicore loop or pipeline | scaling across core counts, load imbalance | plot + notes on scheduling choice |
| CUDA | at least one tiled GPU kernel | occupancy, memory throughput, kernel timeline | Nsight profile and tuned kernel |
| ROCm / HIP | HIP port of a CUDA kernel | correctness + performance delta vs CUDA | portability notes |
| OpenCL / SYCL | one portable parallel kernel | backend/device comparison | portability tradeoff summary |

If you only complete one artifact in this whole module, make it the CUDA one.

---

**Exit Criteria**

You are ready to move on when you can do all of the following without guessing:

- choose between SIMD, CPU threads, and GPU execution for a simple workload
- explain the bottleneck of a kernel in terms of compute, memory, or synchronization
- read a profiler output and identify one actionable optimization
- connect kernel-level behavior to downstream AI workloads such as matmul, attention, or convolution

That is the minimum bar for Phase 3 and Phase 4 to be useful.

---

**Sub-Track 1: C++ and SIMD**

> *The gateway to GPU thinking — same operation, multiple data.*

CPU vector instructions that process 4–16+ data elements per instruction. The same concept GPUs take to 32-wide warps.

| ISA | Extension | Width | FP32 elements/instruction |
|-----|-----------|-------|---------------------------|
| x86 | SSE4.2 | 128 bits | 4 |
| x86 | AVX2 | 256 bits | 8 |
| x86 | AVX-512 | 512 bits | 16 |
| ARM | NEON | 128 bits | 4 |
| ARM | SVE2 | 128–2048 bits | 4–64 (scalable) |

**What the guide covers:**
- Modern C++17 for parallel computing (lambdas, move semantics, smart pointers, parallel STL, templates, constexpr)
- Auto-vectorization and manual intrinsics (`_mm256_fmadd_ps`, `_mm256_load_ps`, etc.)
- Top 10 most-used intrinsics with isolated examples
- AoS vs SoA data layout for vectorization
- Cache alignment, cross-lane limitations, debugging SIMD registers
- Roofline model and the SIMD → GPU mental model bridge
- 9 hands-on projects

---

**Sub-Track 2: OpenMP and oneTBB**

> *Scale from one core to many — the CPU parallelism layer.*

**OpenMP** — add `#pragma omp parallel for` to a loop and it distributes across all cores. Zero boilerplate.

**oneTBB** — task-based parallelism with work-stealing scheduler. Better for irregular workloads (tree traversals, graph algorithms, nested parallelism).

| | OpenMP | oneTBB |
|---|---|---|
| Model | Pragma annotations | C++ template library |
| Ease | One-line changes | Moderate (lambdas + ranges) |
| Load balancing | Static/dynamic (loop-level) | Work-stealing (task-level) |
| Best for | Regular loops | Irregular parallelism, pipelines, flow graphs |

**What the guide covers:**
- OpenMP fork-join, scheduling (static/dynamic/guided), reductions, custom reductions, barriers, sections, tasks
- Fibonacci benchmark: serial vs OpenMP vs oneTBB with real measurements
- oneTBB parallel_for, parallel_reduce, parallel_scan (exclusive scan, stream compaction, prefix max, CDF)
- parallel_pipeline with 4-stage batch inference example
- parallel_invoke, task_group, enumerable_thread_specific (ETS)
- Flow graphs: topology diagrams, ML feature extraction, complete video inference pipeline template
- Work-stealing scheduler deep dive

---

</details>

### 子专题 3：CUDA 与 SIMT（重点）

> *数千线程，一个程序——支撑所有现代 AI 工作负载的执行模型。*

这是阶段 1 中**最重要的子专题**。CUDA 用于对 NVIDIA GPU 编程，而绝大多数 AI 训练与推理都跑在这些 GPU 上。

```
Grid (the entire job)
├── Block 0
│   ├── Warp 0 (threads 0–31)     ← 32 threads execute same instruction (SIMT)
│   ├── Warp 1 (threads 32–63)
│   └── ...
├── Block 1
└── ...
```

**本指南涵盖内容（21 节）：**
- GPU 晶体管预算（GPU 为何存在）、异构编程模型
- 线程层次（grid → block → warp → thread）、索引、边界检查
- SM 架构（Ampere/Hopper/Vera）：CUDA 核心、Tensor Core、warp 调度器、寄存器堆
- 存储层次：寄存器 → 共享内存 → L1 → L2 → HBM（附延迟/带宽表）
- 合并访存、bank conflict、共享内存 padding
- 同步（`__syncthreads`、原子操作、cooperative groups）
- warp 级原语（`__shfl_sync`、`__ballot_sync`、`__reduce_sync`）
- 基于共享内存的分块矩阵乘
- 流、事件、异步拷贝、多流重叠
- cuBLAS / cuDNN / Tensor Core WMMA
- Nsight Systems 与 Nsight Compute 性能剖析
- occupancy 分析、寄存器压力、启动配置
- 8 个递进式项目（向量加 → 多流流水线）

---

### 子专题 4：ROCm 与 HIP

> *AMD 对 CUDA 的回应——编写能同时跑在 NVIDIA 与 AMD 硬件上的 GPU 代码。*

HIP 与 CUDA 有 95% 相同。关键差异在于：AMD 的 wavefront 宽度为 **64 线程**（而 CUDA 的 warp = 32）。AMD CDNA GPU（MI300X）配备 192 GB HBM3，带宽 5.3 TB/s——可与 H100 抗衡。

| CUDA | HIP | 说明 |
|------|-----|-------|
| `cudaMalloc()` | `hipMalloc()` | 函数签名相同 |
| `__syncthreads()` | `__syncthreads()` | 完全相同 |
| Warp（32 线程） | **Wavefront（64 线程）** | 关键架构差异 |
| Tensor Core | **Matrix Core** | rocWMMA API |
| cuBLAS / cuDNN | rocBLAS / MIOpen | 面向 DNN 的 API 不同 |
| NCCL | **RCCL** | 多 GPU 集合通信 |

**本指南涵盖内容（11 节）：**
- CDNA（数据中心）与 RDNA（游戏）架构
- MI300X chiplet 架构（8 个 XCD、304 个 CU、192 GB HBM3）
- 带代码示例的 HIP API、wavefront 宽度的影响
- HIPIFY 工作流（hipify-clang、哪些能自动转换、哪些需要手工处理）
- AMD CU 深入剖析，附 NVIDIA SM 对比表
- ROCm 软件栈与库的对应关系
- HIP 流、事件、pinned memory、异步执行
- 内存管理（显式、managed/HMM、coherent）
- 用 rocWMMA 进行 Matrix Core 编程（FP16/BF16/FP8/INT8）
- 使用 RCCL 的多 GPU 与 Infinity Fabric 拓扑
- AMD 上的 PyTorch（`torch.cuda` 透明映射到 HIP）

---

### 子专题 5：OpenCL 与 SYCL

> *一次编写，随处运行——跨 GPU、FPGA 与 CPU 的可移植计算。*

CUDA 把你锁死在 NVIDIA。HIP 让你能用 AMD。OpenCL 与 SYCL 让你能用**一切**。

```
OpenCL:  Explicit C API — verbose but runs on any device
SYCL:    Modern C++ lambdas — clean but same portability

OpenCL setup:   ~50 lines of boilerplate (platform, device, context, queue, ...)
SYCL equivalent: ~5 lines (queue + parallel_for lambda)
```

| | CUDA | OpenCL | SYCL |
|---|---|---|---|
| 厂商 | 仅 NVIDIA | 任意（Khronos） | 任意（Khronos） |
| 语言 | CUDA C++ | OpenCL C（独立文件） | 标准 C++ |
| kernel 风格 | `__global__` | `.cl` 源码字符串 | 宿主代码中的 lambda |
| 目标平台 | NVIDIA GPU | CPU、GPU、FPGA | CPU、GPU、FPGA |

**本指南涵盖内容：**
- **第 1 部分——OpenCL：** 平台模型、执行模型（CUDA 对应表）、内存模型、完整宿主代码走查（8 步初始化）、基于 local memory 的分块矩阵乘、事件性能剖析、设备查询
- **第 2 部分——SYCL：** buffer/accessor 模型、USM（类 CUDA 指针）、local_accessor 分块、sub-group 可移植归约（不区分 warp/wavefront）、基于 Intel oneAPI 的 FPGA pipe、各实现对比（DPC++、AdaptiveCpp）
- **第 3 部分：** 选型指南（何时用 OpenCL、SYCL、CUDA、HIP）
- 10 个递进式项目

---

## 第 3 部分——并行度谱系

本模块的一切都落在一条谱系上，从窄（1 个核心、SIMD）到宽（10,000 个 GPU 线程）：

```
Parallelism level    Mechanism           Hardware            Threads    Sub-track
─────────────────────────────────────────────────────────────────────────────────
Instruction-level    SIMD intrinsics     CPU vector unit     4–16       1
Thread-level         OpenMP / oneTBB     CPU cores           4–96       2
Massive (NVIDIA)     CUDA kernels        NVIDIA GPU SMs      10K+       3
Massive (AMD)        HIP kernels         AMD GPU CUs         10K+       4
Portable massive     OpenCL / SYCL       Any GPU/FPGA/CPU    10K+       5
```

每一层级都适用**同一条优化原则**：**最小化访存流量，最大化计算复用**（分块）。对 CPU 上的 AVX2 intrinsics、CUDA kernel 中的共享内存、FPGA 上的 BRAM 分块，以及定制 AI 芯片中的 scratchpad 设计，同样成立。

---


<details>
<summary>English original</summary>

**Sub-Track 3: CUDA and SIMT (Main Focus)**

> *Thousands of threads, one program — the execution model behind every modern AI workload.*

This is the **most important sub-track** in Phase 1. CUDA programs NVIDIA GPUs, which run the vast majority of AI training and inference.

```
Grid (the entire job)
├── Block 0
│   ├── Warp 0 (threads 0–31)     ← 32 threads execute same instruction (SIMT)
│   ├── Warp 1 (threads 32–63)
│   └── ...
├── Block 1
└── ...
```

**What the guide covers (21 sections):**
- GPU transistor budget (why GPU exists), heterogeneous programming model
- Thread hierarchy (grid → block → warp → thread), indexing, bounds checking
- SM architecture (Ampere/Hopper/Vera): CUDA cores, Tensor Cores, warp schedulers, register file
- Memory hierarchy: registers → shared memory → L1 → L2 → HBM (with latency/bandwidth table)
- Coalesced memory access, bank conflicts, shared memory padding
- Synchronization (`__syncthreads`, atomics, cooperative groups)
- Warp-level primitives (`__shfl_sync`, `__ballot_sync`, `__reduce_sync`)
- Tiled matrix multiply with shared memory
- Streams, events, async copies, multi-stream overlap
- cuBLAS / cuDNN / Tensor Core WMMA
- Nsight Systems and Nsight Compute profiling
- Occupancy analysis, register pressure, launch configuration
- 8 progressive projects (vector add → multi-stream pipeline)

---

**Sub-Track 4: ROCm and HIP**

> *AMD's answer to CUDA — write GPU code that runs on both NVIDIA and AMD hardware.*

HIP is 95% identical to CUDA. The critical difference: AMD wavefronts are **64 threads** wide (vs CUDA warps = 32). AMD CDNA GPUs (MI300X) have 192 GB HBM3 at 5.3 TB/s — competitive with H100.

| CUDA | HIP | Notes |
|------|-----|-------|
| `cudaMalloc()` | `hipMalloc()` | Same signature |
| `__syncthreads()` | `__syncthreads()` | Identical |
| Warp (32 threads) | **Wavefront (64 threads)** | Key architectural difference |
| Tensor Cores | **Matrix Cores** | rocWMMA API |
| cuBLAS / cuDNN | rocBLAS / MIOpen | Different API for DNN |
| NCCL | **RCCL** | Multi-GPU collectives |

**What the guide covers (11 sections):**
- CDNA (data center) vs RDNA (gaming) architectures
- MI300X chiplet architecture (8 XCDs, 304 CUs, 192 GB HBM3)
- HIP API with code examples, wavefront width implications
- HIPIFY workflow (hipify-clang, what auto-converts, what needs manual work)
- AMD CU deep dive with NVIDIA SM comparison table
- ROCm software stack and library mapping
- HIP streams, events, pinned memory, async execution
- Memory management (explicit, managed/HMM, coherent)
- Matrix Core programming with rocWMMA (FP16/BF16/FP8/INT8)
- Multi-GPU with RCCL and Infinity Fabric topology
- PyTorch on AMD (`torch.cuda` maps to HIP transparently)

---

**Sub-Track 5: OpenCL and SYCL**

> *Write once, run anywhere — portable compute across GPU, FPGA, and CPU.*

CUDA locks you to NVIDIA. HIP gets you AMD. OpenCL and SYCL get you **everything**.

```
OpenCL:  Explicit C API — verbose but runs on any device
SYCL:    Modern C++ lambdas — clean but same portability

OpenCL setup:   ~50 lines of boilerplate (platform, device, context, queue, ...)
SYCL equivalent: ~5 lines (queue + parallel_for lambda)
```

| | CUDA | OpenCL | SYCL |
|---|---|---|---|
| Vendor | NVIDIA only | Any (Khronos) | Any (Khronos) |
| Language | CUDA C++ | OpenCL C (separate) | Standard C++ |
| Kernel style | `__global__` | `.cl` source string | Lambda in host code |
| Targets | NVIDIA GPUs | CPU, GPU, FPGA | CPU, GPU, FPGA |

**What the guide covers:**
- **Part 1 — OpenCL:** platform model, execution model (CUDA mapping table), memory model, full host code walkthrough (8-step setup), tiled matmul with local memory, event profiling, device query
- **Part 2 — SYCL:** buffer/accessor model, USM (CUDA-like pointers), local_accessor tiling, sub-group portable reductions (warp/wavefront-agnostic), FPGA pipes with Intel oneAPI, implementations comparison (DPC++, AdaptiveCpp)
- **Part 3:** decision guide (when to use OpenCL vs SYCL vs CUDA vs HIP)
- 10 progressive projects

---

**Part 3 — The Parallelism Spectrum**

Everything in this module sits on a spectrum from narrow (1 core, SIMD) to wide (10,000 GPU threads):

```
Parallelism level    Mechanism           Hardware            Threads    Sub-track
─────────────────────────────────────────────────────────────────────────────────
Instruction-level    SIMD intrinsics     CPU vector unit     4–16       1
Thread-level         OpenMP / oneTBB     CPU cores           4–96       2
Massive (NVIDIA)     CUDA kernels        NVIDIA GPU SMs      10K+       3
Massive (AMD)        HIP kernels         AMD GPU CUs         10K+       4
Portable massive     OpenCL / SYCL       Any GPU/FPGA/CPU    10K+       5
```

The **same optimization principle** applies at every level: **minimize memory traffic, maximize compute reuse** (tiling). This is equally true for AVX2 intrinsics on a CPU, shared memory in a CUDA kernel, BRAM tiling on an FPGA, and scratchpad design in a custom AI chip.

---

</details>

## 第 4 部分 — 这与路线图的关联

| 你在这里学到的 | 它通向哪里 |
|--------------------|---------------|
| SIMD / 向量化 | 阶段 4C：MLIR `vector` 方言、编译器自动向量化 |
| OpenMP / 多核 | 阶段 2：Jetson SPE、Zynq PS 上的 FreeRTOS 多核 |
| CUDA kernel | 阶段 4B：Jetson 推理，阶段 4C：Triton/CUTLASS kernel 工程 |
| ROCm / HIP | 阶段 5A：AMD GPU 基础设施（MI300X）、可移植 kernel 工程 |
| OpenCL | 阶段 4A：Xilinx Vitis FPGA 主机 API、嵌入式 GPU 计算 |
| SYCL | 未来的可移植计算 — 从单一源码覆盖 CPU、GPU、FPGA、自定义 NPU |
| 存储层次思维 | 阶段 4A：FPGA BRAM/URAM 分块，阶段 5F：AI 芯片的 scratchpad 设计 |
| 分块矩阵乘 | 阶段 4A：HLS 矩阵乘加速器，阶段 5F：脉动阵列架构 |
| GPU 架构模型 | 阶段 5B：CUDA-X 库，阶段 5F：设计出更好的东西 |

**全局图景：**
- 阶段 1 §4 教你**编写**并行硬件程序
- 阶段 4 教你**在真实并行硬件上优化与部署**
- 阶段 5F 教你**设计**新的并行硬件

你先学工作负载。然后你会造出运行它的机器。

---

## 关键要点

1. **并行存在于多个层级** — 指令（SIMD）、线程（OpenMP）、大规模（CUDA/HIP）
2. **CPU vs GPU 是延迟 vs 吞吐** — 不同的活配不同的工具
3. **内存才是真正的瓶颈** — 不是计算。对 CUDA kernel、FPGA 加速器和自定义 AI 芯片都是如此。
4. **CUDA 是行业标准**，用于 GPU 计算与 AI — 先学它
5. **HIP 让你的 CUDA 技能可移植** — 同一份代码在 AMD 和 NVIDIA 上都能跑
6. **分块是通用优化手段** — 从 CUDA 的共享内存到硅片上的脉动阵列
7. **未来 = 异构 + 可移植** — SYCL 用一份代码库同时瞄准 CPU、GPU、FPGA 和自定义加速器

---

## 下一步

→ [**阶段 3 — 神经网络**](../../阶段3-人工智能/01-神经网络/Guide.md) — 正是这些工作负载让上述全部并行成为必需。


<details>
<summary>English original</summary>

**Part 4 — How This Connects to the Roadmap**

| What you learn here | Where it leads |
|--------------------|---------------|
| SIMD / vectorization | Phase 4C: MLIR `vector` dialect, compiler auto-vectorization |
| OpenMP / multi-core | Phase 2: FreeRTOS multi-core on Jetson SPE, Zynq PS |
| CUDA kernels | Phase 4B: Jetson inference, Phase 4C: Triton/CUTLASS kernel engineering |
| ROCm / HIP | Phase 5A: AMD GPU infrastructure (MI300X), portable kernel engineering |
| OpenCL | Phase 4A: Xilinx Vitis FPGA host API, embedded GPU compute |
| SYCL | Future portable compute — CPU, GPU, FPGA, custom NPU from one source |
| Memory hierarchy thinking | Phase 4A: FPGA BRAM/URAM tiling, Phase 5F: scratchpad design for AI chip |
| Tiled matmul | Phase 4A: HLS matmul accelerator, Phase 5F: systolic array architecture |
| GPU architecture model | Phase 5B: CUDA-X libraries, Phase 5F: design something better |

**The big picture:**
- Phase 1 §4 teaches you to **program** parallel hardware
- Phase 4 teaches you to **optimize and deploy** on real parallel hardware
- Phase 5F teaches you to **design** new parallel hardware

You're learning the workload first. Then you'll build the machine that runs it.

---

**Key Takeaways**

1. **Parallelism exists at multiple levels** — instruction (SIMD), thread (OpenMP), massive (CUDA/HIP)
2. **CPU vs GPU is latency vs throughput** — different tools for different jobs
3. **Memory is the real bottleneck** — not compute. This is true for CUDA kernels, FPGA accelerators, and custom AI chips.
4. **CUDA is the industry standard** for GPU compute and AI — learn it first
5. **HIP makes your CUDA skills portable** — same code runs on AMD and NVIDIA
6. **Tiling is the universal optimization** — from shared memory in CUDA to systolic arrays in silicon
7. **Future = heterogeneous + portable** — SYCL targets CPU, GPU, FPGA, and custom accelerators from one codebase

---

**Next**

→ [**Phase 3 — Neural Networks**](../../阶段3-人工智能/01-神经网络/Guide.md) — the workloads that make all this parallelism necessary.

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/4. C++ and Parallel Computing/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/4.%20C%2B%2B%20and%20Parallel%20Computing/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
