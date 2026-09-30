---
title: 02 — 面向训练与推理的 kernel 工程
description: 02 — 面向训练与推理的 kernel 工程
---

# 02 — 面向训练与推理的 kernel 工程

<div class="course-identity auto-course" style="--course-accent: #dc2626; --course-accent-rgb: 220, 38, 38;" markdown="1">
<div class="course-identity__icon">KEFT</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度剖析 · 编译器方向</p>
<p class="course-identity__title">为 02 — 面向训练与推理的 kernel 工程 设定的专项课程标识。</p>
<p class="course-identity__meta">产物：编译器/推理优化 · 度量：算子数量、内存、延迟</p>
</div>
</div>


**顺序：** 第二。在了解计算图与瓶颈（01）之后，你实现并长期负责这些 kernel。

**岗位目标：** **MTS Kernels**（Member of Technical Staff, Kernels）与 **DL Inference Optimization Engineer** 的核心 —— 设计、实现、部署并维护高性能 kernel；生产可靠性；与训练、推理及强化学习（RL）团队协同设计。

---

## 为什么排在第二

第 01 单元告诉你*优化什么*（哪些算子、时间花在哪里）。本单元讲的是*怎么优化*：编写并调优实际的 **kernel** —— 运行在 GPU 线程上、执行重负载运算（矩阵乘、attention 等）的小型硬件相关程序。这是 kernel 工程师岗位的核心（例如 NVIDIA、AGI/LLM kernel 团队）。

---


<details>
<summary>English original</summary>

**02 — Kernel Engineering for Training & Inference**

<div class="course-identity auto-course" style="--course-accent: #dc2626; --course-accent-rgb: 220, 38, 38;" markdown="1">
<div class="course-identity__icon">KEFT</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Compiler Track</p>
<p class="course-identity__title">Specialized course identity for 02 — Kernel Engineering for Training & Inference.</p>
<p class="course-identity__meta">Artifact: compiler/inference optimization · Measure: op count, memory, latency</p>
</div>
</div>


**Order:** Second. After you know the graph and bottlenecks (01), you implement and own the kernels.

**Role target:** Core of **MTS Kernels** (Member of Technical Staff, Kernels) and **DL Inference Optimization Engineer** — design, implement, deploy, and maintain high-performance kernels; production reliability; co-design with training, inference, and reinforcement-learning (RL) teams.

---

**Why this is second**

Unit 01 tells you *what* to optimize (which ops and where time is spent). This unit is *how*: writing and tuning the actual **kernels** — the small, hardware-specific programs that run on GPU threads and execute the heavy operations (matrix multiplies, attention, etc.). This is the heart of kernel-engineer roles (e.g. NVIDIA, AGI/LLM kernel teams).

---

</details>

## 1. 自定义 kernel 编写框架

* **Triton**
    * 一种语言与编译器，可用 **Python 编写 GPU kernel**。你以**分块**方式描述计算：不是一个线程对应一个元素，而是在可装入**共享内存**（线程块共享的快速片上内存）的小块（tile）数据上工作，从而减少对**全局内存**（慢速 GPU DRAM）的重复读取。用于矩阵乘、attention 与自定义算子。
    * **自动调优**指编译器在分块大小、block 大小及其他参数上搜索，为你的 GPU 找到快速配置；无需每次都手工调优即可获得良好性能。
    * 通过 `torch.compile` 与自定义算子集成进 PyTorch。研读官方教程与 Flash-Attention 风格的写法，了解生产级 attention kernel 是如何表达的。
* **CUTLASS / CuTe (CuTe DSL)**
    * **CUTLASS** —— NVIDIA 的 **CUDA 模板库**，面向 **GEMM**（General Matrix Multiply：线性层与矩阵乘背后的核心数学）、**conv**（卷积，用于 CNN）以及自定义算子。它是 NVIDIA GPU 上高性能矩阵数学的参考实现；许多框架与库基于其设计构建，或模仿其设计。
    * **CuTe（CUDA Template Engine）** —— 随 CUTLASS 一同发布的 C++ **DSL**（领域特定语言）与头文件库。它为你提供：
        * **布局抽象：**一种描述数据在内存中*如何*排布的方式——**shape**（如 128×64）、**stride**（沿每个维度跳过多少元素）与 **composition**（如把大矩阵视为由小分块构成的网格）。你定义逻辑**分块**（矩形子块），并把它们映射到**全局内存**（GPU DRAM）、**共享内存**（片上，每线程块）与**寄存器**（每线程），而无需手写易错的索引运算。
        * **拷贝原语：**诸如 `copy()` 之类的构建块，用于生成优化后的 load/store 指令——**向量化**（每条指令搬运多个元素）、**异步**（硬件支持时与计算重叠）与**谓词化**（屏蔽越界）。对于不同的线程布局与内存布局，它们**构造即正确**，因此在改变分块大小或硬件时不会引入隐蔽 bug。
        * **分块与划分：**组合布局以描述**线程块分块**（每个线程块的工作量）、**warp 分块**（每个 warp——32 个线程——的工作量）与 **MMA**（Matrix Multiply-Accumulate：Tensor Core 在一条指令中完成大量乘加的操作）。要把这些尺寸定对，就必须匹配 **Tensor Core**（快速完成稠密矩阵数学的硬件单元）、**共享内存大小**与**寄存器压力**（每线程需要多少寄存器；过多会限制 occupancy）。
    * CuTe 是现代 CUTLASS 与 **cuBLASLt**（NVIDIA 面向灵活布局的批处理 GEMM 库）的骨干。学习它有助于理解生产级 GEMM 与 attention kernel 的结构，以及如何为推理编写或定制 kernel（如 Blackwell、**FP8**——8 位浮点，用于更快、更低精度的数学计算）。
* **Flash-Attention (v2/v3), Quack**
    * **融合 attention** kernel：把 attention 的各步骤（由输入计算 **Q/K/V**——query、key、value；计算 attention 分数与 softmax；作用到 value 上）合并为一个或少数几个 kernel，而不是许多独立的 kernel 启动。更少的启动次数与更少的访存往返，意味着更低延迟与更高吞吐。
    * **Online softmax** 指通过一次数据遍历（借助递推）完成 softmax 计算，因此无需把所有分数都存进内存——这对**长上下文**（超长序列）至关重要，因为完整的 attention 矩阵装不下。
    * **内存高效 attention** 避免物化完整的 N×N attention 矩阵；而是按分块流式处理并归约。研究**分块**（工作负载如何切分为块）、**内存利用率**（用掉了多少带宽）与**数据搬运**（哪些数据被读写、发生在何处）。
    * **深入阅读：**[FlashAttention — A Systems / Kernel Course](01-FlashAttention课程/Guide.md) — 10 讲课程，覆盖 roofline（性能上界模型）、online softmax、FA1/FA2/FA3 算法、仓库剖析、反向正确性 harness、推理 / paged-KV 路径、Hopper 细节，以及一个 capstone patch。
* **Mojo, Pallas/Mosaic (JAX)**
    * **Mojo** —— 一种系统级语言，目标是在 CPU、GPU 及其他加速器之间实现性能可移植性；当需要面向非 NVIDIA 硬件时，可用于编写或移植 kernel。
    * **Pallas / Mosaic** —— JAX 编写自定义 **GPU 与 TPU kernel** 的方式（TPU = Google 的 Tensor Processing Unit）。当需要在 TPU 上运行或移植 kernel，或跨后端比较行为时很有用。

---



---


<details>
<summary>English original</summary>

**1. Custom kernel authoring frameworks**

* **Triton**
    * A language and compiler that lets you write **GPU kernels in Python**. You describe **tile-based** computation: instead of one thread touching one element, you work in small blocks (tiles) of data that fit in **shared memory** (fast on-chip memory shared by a thread block), which reduces repeated reads from **global memory** (slow GPU DRAM). Used for matmul, attention, and custom ops.
    * **Automatic tuning** means the compiler searches over tile sizes, block sizes, and other parameters to find a fast configuration for your GPU; you get good performance without hand-tuning every time.
    * Integrates with PyTorch via `torch.compile` and custom ops. Study official tutorials and Flash-Attention–style patterns to see how production attention kernels are expressed.
* **CUTLASS / CuTe (CuTe DSL)**
    * **CUTLASS** — NVIDIA's **CUDA template library** for **GEMM** (General Matrix Multiply: the core math behind linear layers and matmuls), **conv** (convolution, used in CNNs), and custom ops. It is the reference implementation for high-performance matrix math on NVIDIA GPUs; many frameworks and libraries build on or mimic its design.
    * **CuTe (CUDA Template Engine)** — A C++ **DSL** (domain-specific language) and header library shipped inside CUTLASS. It gives you:
        * **Layout abstractions:** A way to describe *how* data is arranged in memory — **shape** (e.g. 128×64), **stride** (how many elements to skip along each dimension), and **composition** (e.g. a big matrix as a grid of smaller tiles). You define logical **tiles** (rectangular sub-blocks) and map them to **global memory** (GPU DRAM), **shared memory** (on-chip, per thread block), and **registers** (per-thread), without writing error-prone index math by hand.
        * **Copy primitives:** Building blocks like `copy()` that generate optimized load/store instructions — **vectorized** (moving multiple elements per instruction), **async** (overlap with compute when the hardware supports it), and **predicated** (mask off out-of-bounds). They are **correct by construction** for different thread and memory layouts, so you avoid subtle bugs when changing tile sizes or hardware.
        * **Tiling and partitioning:** You compose layouts to describe **thread block tile** (work per block of threads), **warp tile** (work per warp — 32 threads), and **MMA** (Matrix Multiply-Accumulate: the tensor-core operation that does many multiply-adds in one instruction). Getting these sizes right is essential to match **tensor cores** (hardware units that do dense matrix math very fast), **shared memory size**, and **register pressure** (how many registers each thread needs; too many can limit occupancy).
    * CuTe is the backbone of modern CUTLASS and **cuBLASLt** (NVIDIA's library for batched GEMM with flexible layouts). Learning it helps you understand how production GEMM and attention kernels are structured and how to write or customize kernels for inference (e.g. Blackwell, **FP8** — 8-bit floating point for faster, lower-precision math).
* **Flash-Attention (v2/v3), Quack**
    * **Fused attention** kernels: they combine the steps of attention (computing **Q/K/V** — query, key, value — from inputs; doing the attention scores and softmax; applying to values) into one or a few kernels instead of many separate kernel launches. Fewer launches and less round-trips to memory mean lower latency and higher throughput.
    * **Online softmax** means computing the softmax in a single pass over the data (with a recurrence), so you don't need to store all scores in memory — critical for **long-context** (very long sequences) where the full attention matrix would not fit.
    * **Memory-efficient attention** avoids materializing the full N×N attention matrix; you stream and reduce in tiles. Study **tiling** (how the workload is split into blocks), **memory utilization** (how much of bandwidth you use), and **data movement** (what gets read/written and where).
    * **Deep dive:** [FlashAttention — A Systems / Kernel Course](01-FlashAttention课程/Guide.md) — 10-lecture course covering roofline, online softmax, FA1/FA2/FA3 algorithms, repo anatomy, backward correctness harness, inference / paged-KV path, Hopper specifics, and a capstone patch.
* **Mojo, Pallas/Mosaic (JAX)**
    * **Mojo** — A systems language aimed at performance portability across CPUs, GPUs, and other accelerators; useful for writing or porting kernels when you need to target non-NVIDIA hardware.
    * **Pallas / Mosaic** — JAX's way to write custom **GPU and TPU kernels** (TPU = Google's Tensor Processing Unit). Useful when you need to run or port kernels on TPU or compare behavior across backends.

---

</details>

## 2. 长上下文与 attention kernel

* **挑战**
    * **内存利用率** — 对长序列，attention 机制朴素实现会需要 O(N²) 内存（N = 序列长度）。必须设计节省内存的 kernel（分块、流式、online softmax），使模型能扩展到 **1M+ 上下文**（数百万 token）。
    * **KV-cache 布局** — 在自回归生成期间，先前 token 的 **key** 和 **value** 张量会被缓存并复用。它们在内存中的布局方式（连续、分页、分片）会影响带宽和 kernel 效率；糟糕的布局可能主导 runtime。
    * **数据移动与带宽** — attention 通常 **带宽受限**：GPU 花在移动数据上的时间比计算更多。能减少冗余读/写并最大化有用 **带宽**（从内存获取的字节/秒）的 kernel 至关重要。
* **模式** — Flash-Attention、**FlashInfer**、**Magic-Attention**（例如 GTC 2026）：具有 **可变长度**（批中每项的序列长度不同）和生产级正确性与测试的 fused attention 实现。
* **分析**
    * **roofline**（性能上界模型） — 一个将性能与 **算术强度**（每字节操作数）关联起来的简单模型。它展示一个“roof”（算力受限上限）和一个“ridge”（带宽受限上限）。对 attention，通常位于带宽受限一侧；目标是减少移动的字节数或增加复用。
    * **Occupancy** — 有多少 **warp**（32 个线程的组）可以在 **SM**（Streaming Multiprocessor——GPU 的计算单元）上并发运行。更高的 occupancy 可以隐藏 **延迟**（例如内存访问延迟），但每个线程寄存器过多会降低 occupancy；需要平衡寄存器使用与并行度。
    * **带宽受限瓶颈** — 当 GPU 在等待内存而不是做计算时。通过减少数据移动、改善局部性，以及使用正确的分块大小，使共享内存/寄存器中的数据被复用，可以避免这些瓶颈。
    * **持续吞吐** — 真实工作负载中实际达到的 GFLOPS 或 tokens/sec，而不只是理论峰值；这是对生产至关重要的指标。

---

## 3. 集合通信

当模型或批跨 **多个 GPU** 或 **多个节点**（机器）分布时，kernel 必须交换数据。**集合通信**是用于此的一组模式：每个 rank（GPU）参与同一操作。

* **NCCL**（NVIDIA Collective Communications Library）
    * **全规约** — 每个 GPU 有一个张量；调用后，每个 GPU 拥有所有张量的 *和*（或其他归约结果）。例如用于数据并行训练中对梯度求和，或同步状态。
    * **All-gather** — 每个 GPU 有一个数据块；调用后，每个 GPU 拥有完整拼接后的张量。当需要每个 rank 上都有整个张量时使用。
    * **Reduce-scatter** — 先在 rank 间归约（例如求和），再分散结果，使每个 rank 得到不同的切片。常与 all-gather 结合使用，以实现高效的梯度归约。
    * **多节点、多 GPU 调优** — 不同拓扑（NVLink、InfiniBand、以太网）和规模需要不同的算法与缓冲区大小；NCCL 针对 NVIDIA 硬件进行调优。
    * **通信与计算重叠** — 当模型的一部分在计算时，可以在后台为下一步发送/接收数据，因此通信不会完全阻塞进展；对扩展至关重要。
* **MSCCLPP** — Microsoft 的集合通信库；NCCL 的替代方案。当需要可移植性（例如 AMD/其他 GPU）或替代后端时，可与 NCCL 对比。

---

## 4. 生产与可移植性

* **鲁棒性与测试**
    * **功能正确性** — kernel 对所有支持的输入和配置产生正确输出（或在可接受的数值容差内）。
    * **数值稳定性** — 尤其对自定义 **attention** 和 **softmax**：运算顺序、缩放和精度会影响溢出/下溢和准确率；必须在边界情况上验证。
    * **可复现 benchmark 与 CI** — 在各处都以相同方式运行的 benchmark，以及 **CI**（Continuous Integration）在每次变更时运行测试、有时运行 benchmark，以便尽早捕获回归。
* **移植到替代硬件** — 将 kernel 评测或移植到 **TPU**（Pallas/Mosaic）、AMD GPU 或其他加速器。这通常涉及 **抽象层**（例如一份 kernel 描述、多个后端）和 **后端特定权衡**（不同的分块大小、指令集、内存模型）。
* **协同设计** — 与训练、推理和 RL 团队有清晰的 **契约**和 **APIs**：谁拥有哪个 kernel、它们暴露什么接口、版本管理和部署如何运作，以便在模型和框架演进时生产保持可靠。

---


<details>
<summary>English original</summary>

**2. Long-context and attention kernels**

* **Challenges**
    * **Memory utilization** — For long sequences, the attention mechanism would naively need O(N²) memory (N = sequence length). You must design kernels that use memory sparingly (tiling, streaming, online softmax) so models can scale to **1M+ context** (millions of tokens).
    * **KV-cache layout** — During autoregressive generation, **key** and **value** tensors from previous tokens are cached and reused. How you lay them out in memory (contiguous, paged, sharded) affects bandwidth and kernel efficiency; poor layout can dominate runtime.
    * **Data movement and bandwidth** — Attention is often **memory-bound**: the GPU spends more time moving data than computing. Kernels that reduce redundant reads/writes and maximize useful **bandwidth** (bytes per second from memory) are critical.
* **Patterns** — Flash-Attention, **FlashInfer**, **Magic-Attention** (e.g. GTC 2026): fused attention implementations with **variable length** (different sequence lengths per batch item) and production-grade correctness and testing.
* **Analysis**
    * **Roofline** — A simple model that relates performance to **arithmetic intensity** (ops per byte). It shows a "roof" (compute-bound limit) and a "ridge" (memory-bound limit). For attention, you often sit on the memory-bound side; the goal is to reduce bytes moved or increase reuse.
    * **Occupancy** — How many **warps** (groups of 32 threads) can run concurrently on an **SM** (Streaming Multiprocessor — the GPU's compute unit). Higher occupancy can hide **latency** (e.g. memory access delay), but too many registers per thread can lower it; you balance register use and parallelism.
    * **Memory-bound bottlenecks** — When the GPU is waiting on memory rather than doing math. You avoid them by reducing data movement, improving locality, and using the right tile sizes so that data in shared memory/registers is reused.
    * **Sustained throughput** — Actual achieved GFLOPS or tokens/sec in real workloads, not just peak theoretical; the metric that matters for production.

---

**3. Collective communication**

When the model or batch is spread across **multiple GPUs** or **multiple nodes** (machines), kernels must exchange data. **Collective communication** is the set of patterns for this: every rank (GPU) participates in the same operation.

* **NCCL** (NVIDIA Collective Communications Library)
    * **All-reduce** — Every GPU has a tensor; after the call, every GPU has the *sum* (or another reduction) of all tensors. Used e.g. to sum gradients in data-parallel training or to synchronize state.
    * **All-gather** — Each GPU has a chunk; after the call, every GPU has the full concatenated tensor. Used when you need the whole tensor on every rank.
    * **Reduce-scatter** — First reduce (e.g. sum) across ranks, then scatter the result so each rank gets a distinct slice. Often used in combination with all-gather for efficient gradient reduction.
    * **Tuning for multi-node, multi-GPU** — Different topologies (NVLink, InfiniBand, Ethernet) and sizes require different algorithms and buffer sizes; NCCL is tuned for NVIDIA hardware.
    * **Overlap of communication with compute** — While one part of the model is computing, you can be sending/receiving data for the next step in the background, so communication does not fully block progress; critical for scaling.
* **MSCCLPP** — Microsoft's collective library; an alternative to NCCL. Compare with NCCL when you need portability (e.g. AMD/other GPUs) or alternative backends.

---

**4. Production and portability**

* **Robustness and testing**
    * **Functional correctness** — Kernels produce the right outputs (or within acceptable numerical tolerance) for all supported inputs and configurations.
    * **Numerical stability** — Especially for custom **attention** and **softmax**: order of operations, scaling, and precision can affect overflow/underflow and accuracy; you must validate on edge cases.
    * **Reproducible benchmarks and CI** — Benchmarks that run the same way everywhere, and **CI** (Continuous Integration) that runs tests and sometimes benchmarks on every change, so regressions are caught early.
* **Porting to alternative hardware** — Evaluate or port kernels to **TPU** (Pallas/Mosaic), AMD GPUs, or other accelerators. This often involves **abstraction layers** (e.g. one kernel description, multiple backends) and **backend-specific trade-offs** (different tile sizes, instruction sets, memory models).
* **Co-design** — Clear **contracts** and **APIs** with training, inference, and RL teams: who owns which kernel, what interfaces they expose, how versioning and deployment work so that production stays reliable as models and frameworks evolve.

---

</details>

## 5. 计算机体系结构与代码生成

* **底层专业知识** — 理解你的 kernel 如何映射到硬件：
    * **存储层次** — 寄存器（最快，每线程）→ **共享内存**（片上，每线程块）→ L1/L2 缓存 → **全局内存**（GPU DRAM）。kernel 调优靠的是把热数据留在更快的层级，并尽量降低对全局内存的访问量。
    * **warp 与 SM 行为** — **warp** 是 32 个锁步执行的线程；**SM**（Streaming Multiprocessor）是运行 warp 的单元。要解释并改进性能，需要分析 **divergence**（一个 warp 内的线程走不同路径，可能使执行串行化）、**occupancy**（每个 SM 上有多少 warp）以及**指令吞吐**（硬件每周期能执行多少 op）。
* **代码生成** — 编译器如何把高层描述变成 GPU/TPU 代码：
    * **IR**（Intermediate Representation，中间表示） — 程序的一种内部形式（例如 **MLIR**、**Triton IR**），编译器先对其优化，再下降为机器码。kernel 工程师常常需要理解或生成 IR，以便选出并生成正确的 kernel。
    * **把算子映射到 kernel** — 单个高层算子（例如 “matmul”）可以由许多种 kernel 实现（不同的分块大小、精度、后端）。编译器根据 shape、设备与启发式规则来选择或生成 kernel；你可以通过编写自定义 kernel 或改进编译器的选择逻辑来施加影响。

---

## 资源

* [Triton Documentation](https://triton-lang.org/) — 语言与 GPU kernel 模式。
* [CUTLASS](https://github.com/NVIDIA/cutlass) — 用于 GEMM（矩阵-矩阵乘）及更多的 CUDA 模板。
* [CuTe](https://github.com/NVIDIA/cutlass/tree/main/cute) — layout 与 copy DSL。
* [Flash-Attention](https://github.com/Dao-AILab/flash-attention) — 内存高效的 attention。
* [NCCL](https://developer.nvidia.com/nccl) — NVIDIA Collective Communications Library。
* [Pallas / Mosaic (JAX)](https://github.com/google/jax/tree/main/jax/experimental/pallas) — GPU/TPU kernel 编写。
* GTC 2026：Magic-Attention 与长上下文 kernel 相关演讲。

---

## 项目

1. **Triton 融合 kernel** — 用 Triton 实现一个**融合算子**（例如 **layer norm** + **残差**：用同一个 kernel 完成激活值的归一化与跳跃连接的相加，而不是拆成两个 kernel），或实现一个自定义的 attention 变体。与 PyTorch 做 benchmark 对比，并用 **Nsight Compute**（NVIDIA 面向 GPU kernel 的性能分析器：指令级时序、内存吞吐、occupancy）做**性能分析**。
2. **长上下文 attention** — 研究 Flash-Attention 的分块与在线 softmax。实现一个简化的长上下文 attention kernel；测量内存在吞吐上的权衡。
3. **大规模 NCCL** — 在规模化场景下运行 NCCL 全规约（多 GPU 或多节点，视条件而定）。调优并记录与计算重叠的机会。
4. **可移植性报告** — 一页的书面报告：把一个 kernel 通过 Pallas 移植到 TPU，或移植到另一个后端（例如 Mojo），需要做哪些工作。

---

## 下一步

→ **[03 — Compiler Stack](../03-编译器栈/Guide.md)** — **IR**（中间表示）、**调度**（算子何时何地运行）与 **codegen**（代码生成）如何产生并选择这些 kernel（例如在 tinygrad、TVM、MLIR 中）。


<details>
<summary>English original</summary>

**5. Computer architecture and code generation**

* **Low-level expertise** — Understanding how your kernels map to hardware:
    * **Memory hierarchy** — Registers (fastest, per thread) → **shared memory** (on-chip, per thread block) → L1/L2 cache → **global memory** (GPU DRAM). Kernels are tuned by keeping hot data in faster levels and minimizing traffic to global memory.
    * **Warp and SM behavior** — A **warp** is 32 threads that execute in lockstep; **SM** (Streaming Multiprocessor) is the unit that runs warps. You reason about **divergence** (threads in a warp taking different paths, which can serialize execution), **occupancy** (how many warps per SM), and **instruction throughput** (how many ops per cycle the hardware can do) to explain and improve performance.
* **Code generation** — How compilers turn high-level descriptions into GPU/TPU code:
    * **IR** (Intermediate Representation) — An internal form of the program (e.g. **MLIR**, **Triton IR**) that the compiler optimizes and then lowers to machine code. Kernel engineers often need to understand or emit IR so that the right kernels are selected and generated.
    * **Mapping ops to kernels** — A single high-level op (e.g. "matmul") can be implemented by many possible kernels (different tile sizes, precisions, backends). Compilers choose or generate kernels based on shape, device, and heuristics; you influence this by writing custom kernels or improving the compiler's selection.

---

**Resources**

* [Triton Documentation](https://triton-lang.org/) — Language and GPU kernel patterns.
* [CUTLASS](https://github.com/NVIDIA/cutlass) — CUDA templates for GEMM and more.
* [CuTe](https://github.com/NVIDIA/cutlass/tree/main/cute) — Layout and copy DSL.
* [Flash-Attention](https://github.com/Dao-AILab/flash-attention) — Memory-efficient attention.
* [NCCL](https://developer.nvidia.com/nccl) — NVIDIA Collective Communications Library.
* [Pallas / Mosaic (JAX)](https://github.com/google/jax/tree/main/jax/experimental/pallas) — GPU/TPU kernel authoring.
* GTC 2026: Magic-Attention and long-context kernel talks.

---

**Projects**

1. **Triton fused kernel** — Implement a **fused operator** (e.g. **layer norm** + **residual**: normalize activations and add the skip connection in one kernel instead of two) or a custom attention variant in Triton. Benchmark vs PyTorch and **profile** with **Nsight Compute** (NVIDIA's profiler for GPU kernels: instruction-level timing, memory throughput, occupancy).
2. **Long-context attention** — Study Flash-Attention's tiling and online softmax. Implement a simplified long-context attention kernel; measure memory vs throughput trade-offs.
3. **NCCL at scale** — Run NCCL all-reduce at scale (multi-GPU or multi-node if available). Tune and document overlap opportunities with compute.
4. **Portability report** — One-page write-up: what it would take to port one of your kernels to TPU via Pallas or to another backend (e.g. Mojo).

---

**Next**

→ **[03 — Compiler Stack](../03-编译器栈/Guide.md)** — How **IR** (intermediate representation), **scheduling** (when and where ops run), and **codegen** (code generation) produce and select these kernels (e.g. in tinygrad, TVM, MLIR).

</details>

---

> 原文：[`Phase 4 - Track C - DL Inference Optimization/02 - Kernel Engineering/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20C%20-%20DL%20Inference%20Optimization/02%20-%20Kernel%20Engineering/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
