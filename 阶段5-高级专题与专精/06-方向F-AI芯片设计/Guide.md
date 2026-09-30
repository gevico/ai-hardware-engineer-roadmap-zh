---
title: 阶段 5 — 路线 F：AI 芯片设计（18-36 个月）
description: 阶段 5 — 路线 F：AI 芯片设计（18-36 个月）
---

# 阶段 5 — 路线 F：AI 芯片设计（18-36 个月）

<div class="course-identity ai-chip-design" markdown="1">
<div class="course-identity__icon">ASIC</div>
<div markdown="1">
<p class="course-identity__eyebrow">路线 5F · AI 芯片设计</p>
<p class="course-identity__title">设计加速器 dataflow、存储层次、接口，以及面向硅片的架构规格。</p>
<p class="course-identity__meta">产物：加速器架构规格 · 度量：面积、SRAM、带宽、能耗、吞吐</p>
</div>
</div>


**前置要求：先掌握 AI/软件栈**

* **为何软件优先：**
    * **软硬件协同设计：** AI 芯片设计需要深入理解软件栈——训练框架、推理 runtime 和算子语义。设计软件真正需要的硬件。
    * **工作负载刻画：** 对真实 AI 工作负载（训练、推理）做性能剖析，定位瓶颈。在设计硬件之前，先理解内存带宽、计算强度和数据复用模式。
    * **参考：tinygrad：** 研究 tinygrad——一个极简、可读的深度学习框架。它揭示了张量运算、autograd 和代码生成的本质。理解 tinygrad 有助于看清硬件必须加速什么。

* **tinygrad 与极简 ML 框架：**
    * **tinygrad 架构：** 研究 tinygrad 的惰性求值、线性化 IR（中间表示），以及面向不同后端（CPU、GPU、自定义）的代码生成。
    * **算子语义：** 理解 tinygrad 如何实现 conv2d、matmul、attention 及其他算子。追踪计算图与内存访问模式。
    * **扩展 tinygrad：** 添加自定义后端或新算子。这会让你理解软件与硬件之间的接口。
    * **其他极简框架：** 探索 mlx、JAX（用于理解追踪与编译），或 Triton（用于 GPU kernel 设计）。

**资源：**

* **tinygrad GitHub：** https://github.com/tinygrad/tinygrad — 极简且可读。研究其代码库。
* **tinygrad 学习资料：** 参见 [5. Autonomous Vehicles/tinygrad](../05-方向E-自动驾驶/03-tinygrad推理/Guide.md)，其中有实操指南、算子参考和 Jetson 支持。
* **"Tinygrad: A Simple Autograd Engine"（博客/视频）：** George Hotz 对 tinygrad 设计的讲解。
* **"Computer Architecture: A Quantitative Approach"（Hennessy & Patterson）：** 理解加速器设计的基础。

**项目：**


<details>
<summary>English original</summary>

**Phase 5 — Track F: AI Chip Design (18-36 months)**

<div class="course-identity ai-chip-design" markdown="1">
<div class="course-identity__icon">ASIC</div>
<div markdown="1">
<p class="course-identity__eyebrow">Track 5F · AI Chip Design</p>
<p class="course-identity__title">Design accelerator dataflows, memory hierarchies, interfaces, and silicon-facing architecture specs.</p>
<p class="course-identity__meta">Artifact: accelerator architecture spec · Measure: area, SRAM, bandwidth, energy, throughput</p>
</div>
</div>


**Prerequisite: Master AI/Software Stack First**

* **Why Software First:**
    * **Hardware-Software Co-Design:** AI chip design requires deep understanding of the software stack—training frameworks, inference runtimes, and operator semantics. Design hardware that software actually needs.
    * **Workload Characterization:** Profile real AI workloads (training, inference) to identify bottlenecks. Understand memory bandwidth, compute intensity, and data reuse patterns before designing hardware.
    * **Reference: tinygrad:** Study tinygrad—a minimal, readable deep learning framework. It exposes the essence of tensor operations, autograd, and code generation. Understanding tinygrad helps you see what hardware must accelerate.

* **tinygrad and Minimal ML Frameworks:**
    * **tinygrad Architecture:** Study tinygrad's lazy evaluation, linearized IR (intermediate representation), and code generation for different backends (CPU, GPU, custom).
    * **Operator Semantics:** Understand how tinygrad implements conv2d, matmul, attention, and other ops. Trace the graph and memory access patterns.
    * **Extending tinygrad:** Add a custom backend or new op. This teaches you the interface between software and hardware.
    * **Other Minimal Frameworks:** Explore mlx, JAX (for understanding tracing and compilation), or Triton (for GPU kernel design).

**Resources:**

* **tinygrad GitHub:** https://github.com/tinygrad/tinygrad — Minimal and readable. Study the codebase.
* **tinygrad Learning Materials:** See [5. Autonomous Vehicles/tinygrad](../05-方向E-自动驾驶/03-tinygrad推理/Guide.md) for hands-on guides, ops reference, and Jetson support.
* **"Tinygrad: A Simple Autograd Engine" (blog/videos):** George Hotz's explanations of tinygrad design.
* **"Computer Architecture: A Quantitative Approach" (Hennessy & Patterson):** Foundation for understanding accelerator design.

**Projects:**

</details>

* **实现自定义 tinygrad 后端：** 让 tinygrad 面向一个简单加速器（如 FPGA、自定义模拟器）。
* **在 tinygrad 中剖析并优化模型：** 区分算力受限与带宽受限的 layer。提出硬件优化方案。


**2. AI 加速器架构**

* **计算与存储：**
    * **脉动阵列与 TPU：** 理解用于矩阵乘法的脉动阵列架构。研究 Google TPU 及类似设计。
    * **Dataflow 架构：** 了解 dataflow（如 Eyeriss、NVDLA）与 control-flow 架构的差异。理解空间计算与时间计算。
    * **存储层次：** 围绕内存带宽进行设计——片上 SRAM、HBM 与数据复用。面向加速器的 roofline 模型（性能上界模型）。

* **量化与精度：**
    * **INT8/INT4 推理：** 面向低精度计算的量化感知设计。理解不同精度之间的取舍。
    * **混合精度训练：** 用 FP16/BF16 做训练。研究梯度缩放与 loss 缩放。


<details>
<summary>English original</summary>

* **Implement a Custom tinygrad Backend:** Target a simple accelerator (e.g., FPGA, custom simulator) from tinygrad.
* **Profile and Optimize a Model in tinygrad:** Identify compute-bound vs. memory-bound layers. Propose hardware optimizations.


**2. AI Accelerator Architecture**

* **Compute and Memory:**
    * **Systolic Arrays and TPUs:** Understand systolic array architecture for matrix multiplication. Study Google TPU and similar designs.
    * **Dataflow Architectures:** Learn dataflow (e.g., Eyeriss, NVDLA) vs. control-flow architectures. Understand spatial vs. temporal compute.
    * **Memory Hierarchy:** Design for memory bandwidth—on-chip SRAM, HBM, and data reuse. Roofline model for accelerators.

* **Quantization and Precision:**
    * **INT8/INT4 Inference:** Quantization-aware design for low-precision compute. Understand the trade-offs for different precisions.
    * **Mixed-Precision Training:** FP16/BF16 for training. Study gradient scaling and loss scaling.

</details>

* **编译器与 runtime：**
    * **LLVM：** 理解 LLVM IR、三阶段编译器设计（前端 → 优化器 → 后端），以及如何为自定义加速器编写后端。研究与 AI 相关的关键 pass：循环向量化、别名分析、通过 TableGen 进行指令选择。
    * **MLIR：** 研究多级中间表示 —— 方言（linalg、tensor、memref、affine、vector、gpu）、渐进 lowering，以及如何为你的加速器定义自定义方言。
    * **TVM 与基于 MLIR 的编译器：** 研究 TVM 基于 schedule 的方法（TIR + 自动调优）、基于 MLIR 的编译器（IREE、Triton-MLIR、torch-mlir），以及 tinygrad 的极简编译器设计。理解 ML 模型如何被编译成硬件特定的代码。
    * **kernel 融合与调度：** 了解编译器如何融合算子（linalg 融合、tinygrad 调度器），以及如何调度 kernel 以获得最优性能。
    * **详细讲座：** 参见 [LLVM & MLIR Lecture Series](02-讲义/Lecture-01.md) 获取深入讲解：
        * [Lecture 1: LLVM IR & Architecture](02-讲义/Lecture-01.md) —— IR 类型、静态单赋值、地址空间、intrinsics
        * [Lecture 2: LLVM Passes & Code Generation](02-讲义/Lecture-02.md) —— 优化 pass、TableGen、后端流水线
        * [Lecture 3: MLIR Fundamentals](02-讲义/Lecture-03.md) —— 方言、操作、渐进 lowering
        * [Lecture 4: MLIR for ML Compilers](02-讲义/Lecture-04.md) —— linalg、tensor、affine、vector 方言
        * [Lecture 5: ML-to-Hardware Compilation Pipelines](02-讲义/Lecture-05.md) —— TVM、IREE、Triton-MLIR、tinygrad

**资源：**

* **"Eyeriss: A Spatial Architecture for Energy-Efficient Dataflow for CNN"（论文）：** 经典加速器架构。
* **"A Full-Stack Accelerator for Deep Learning"（NVDLA）：** 开源的 Nvidia 设计。
* **TVM 与 Apache TVM：** 面向深度学习的编译器栈。
* **[LLVM Language Reference](https://llvm.org/docs/LangRef.html)：** 权威的 LLVM IR 规范。
* **[MLIR Documentation](https://mlir.llvm.org/)：** MLIR 官方文档、方言参考与教程。

**项目：**

* **设计一个简单的矩阵乘法加速器：** 用 RTL 或高层次综合为一个小型加速器指定架构（例如脉动阵列）。
* **将 tinygrad 模型映射到你的加速器：** 定义从 tinygrad 算子到你的硬件的映射。


**3. 从软件到硅**


<details>
<summary>English original</summary>

* **Compiler and Runtime:**
    * **LLVM:** Understand LLVM IR, the three-phase compiler design (frontend → optimizer → backend), and how to write a backend for a custom accelerator. Study passes critical for AI: loop vectorization, alias analysis, instruction selection via TableGen.
    * **MLIR:** Study multi-level intermediate representation — dialects (linalg, tensor, memref, affine, vector, gpu), progressive lowering, and how to define a custom dialect for your accelerator.
    * **TVM and MLIR-Based Compilers:** Study TVM's schedule-based approach (TIR + auto-tuning), MLIR-based compilers (IREE, Triton-MLIR, torch-mlir), and tinygrad's minimal compiler design. Understand how ML models are compiled to hardware-specific code.
    * **Kernel Fusion and Scheduling:** Learn how compilers fuse ops (linalg fusion, tinygrad scheduler) and schedule kernels for optimal performance.
    * **Detailed Lectures:** See the [LLVM & MLIR Lecture Series](02-讲义/Lecture-01.md) for in-depth coverage:
        * [Lecture 1: LLVM IR & Architecture](02-讲义/Lecture-01.md) — IR types, SSA, address spaces, intrinsics
        * [Lecture 2: LLVM Passes & Code Generation](02-讲义/Lecture-02.md) — optimization passes, TableGen, backend pipeline
        * [Lecture 3: MLIR Fundamentals](02-讲义/Lecture-03.md) — dialects, operations, progressive lowering
        * [Lecture 4: MLIR for ML Compilers](02-讲义/Lecture-04.md) — linalg, tensor, affine, vector dialects
        * [Lecture 5: ML-to-Hardware Compilation Pipelines](02-讲义/Lecture-05.md) — TVM, IREE, Triton-MLIR, tinygrad

**Resources:**

* **"Eyeriss: A Spatial Architecture for Energy-Efficient Dataflow for CNN" (paper):** Classic accelerator architecture.
* **"A Full-Stack Accelerator for Deep Learning" (NVDLA):** Open-source Nvidia design.
* **TVM and Apache TVM:** Compiler stack for deep learning.
* **[LLVM Language Reference](https://llvm.org/docs/LangRef.html):** Authoritative LLVM IR specification.
* **[MLIR Documentation](https://mlir.llvm.org/):** Official MLIR docs, dialect references, and tutorials.

**Projects:**

* **Design a Simple Matrix Multiply Accelerator:** Specify the architecture (e.g., systolic array) for a small accelerator in RTL or high-level synthesis.
* **Map a tinygrad Model to Your Accelerator:** Define the mapping from tinygrad ops to your hardware.


**3. From Software to Silicon**

</details>

* **RTL 与验证：**
    * **用于加速器的 Verilog/SystemVerilog：** 实现 AI 加速器的关键数据通路与控制模块。
    * **高层次综合（HLS）：** 使用 HLS（Vitis HLS 等）为矩阵运算和自定义 kernel 加速设计。
    * **形式验证：** 对关键数据通路的正确性应用形式化方法。

* **FPGA 原型验证：**
    * **FPGA 作为加速器：** 在 FPGA 上部署小型 AI 加速器。使用 FINN（Xilinx）等框架实现量化神经网络。
    * **与 CPU/GPU 协同设计：** 将 FPGA 加速器与主机系统集成。理解 PCIe、DMA 和驱动接口。

* **ASIC 与流片（进阶）：**
    * **物理设计：** 概述 AI 芯片的布局布线、时序收敛和功耗分析。
    * **产业与初创公司：** 研究商用 AI 芯片（Nvidia、AMD、Cerebras、Groq 等）与初创公司的技术路线。
    * **RISC-V AI 平台：** 研究 RISC-V 作为 AI 芯片设计切入点，包括向量 AI CPU、CPU+NPU SoC、加速器内部的 RISC-V 控制核，以及市场定位。参见专题课程：[RISC-V Based AI Chip Design and Market Analysis](03-RISC-V-AI芯片设计与市场/Guide.md)。

**资源：**

* **FINN（Xilinx）：** 用于为神经网络构建快速、灵活的 FPGA 加速器的框架。
* **NVDLA（Nvidia）：** 开源深度学习加速器。
* **芯片设计课程（如 Berkeley、Stanford）：** 数字设计与计算机体系结构在线课程。

**项目：**

* **在 FPGA 上实现一个小型加速器：** 使用 HLS 或 RTL 构建矩阵乘法或 conv2d 加速器。
* **对比 tinygrad 在 CPU 与你的 FPGA 加速器上的表现：** benchmark 并分析加速比与效率。


**阶段 2（大幅扩展）：AI 芯片设计（36-60 个月）**

**1. 进阶加速器微架构**


<details>
<summary>English original</summary>

* **RTL and Verification:**
    * **Verilog/SystemVerilog for Accelerators:** Implement key datapath and control blocks for an AI accelerator.
    * **High-Level Synthesis (HLS):** Use HLS (Vitis HLS, etc.) to accelerate design for matrix ops and custom kernels.
    * **Formal Verification:** Apply formal methods for critical datapath correctness.

* **FPGA Prototyping:**
    * **FPGA as Accelerator:** Deploy a small AI accelerator on FPGA. Use frameworks like FINN (Xilinx) for quantized neural networks.
    * **Co-Design with CPUs/GPUs:** Integrate FPGA accelerators with host systems. Understand PCIe, DMA, and driver interfaces.

* **ASIC and Tape-Out (Advanced):**
    * **Physical Design:** Overview of place-and-route, timing closure, and power analysis for AI chips.
    * **Industry and Startups:** Study commercial AI chips (Nvidia, AMD, Cerebras, Groq, etc.) and startup approaches.
    * **RISC-V AI Platforms:** Study RISC-V as an AI-chip design point, including vector AI CPUs, CPU+NPU SoCs, RISC-V control cores inside accelerators, and market positioning. See the focused course: [RISC-V Based AI Chip Design and Market Analysis](03-RISC-V-AI芯片设计与市场/Guide.md).

**Resources:**

* **FINN (Xilinx):** Framework for building fast, flexible FPGA accelerators for neural networks.
* **NVDLA (Nvidia):** Open-source deep learning accelerator.
* **Chip design courses (e.g., Berkeley, Stanford):** Online courses on digital design and computer architecture.

**Projects:**

* **Implement a Small Accelerator on FPGA:** Build a matrix multiply or conv2d accelerator using HLS or RTL.
* **Compare tinygrad on CPU vs. Your FPGA Accelerator:** Benchmark and analyze the speedup and efficiency.


**Phase 2 (Significantly Expanded): AI Chip Design (36-60 months)**

**1. Advanced Accelerator Microarchitecture**

</details>

* **Dataflow 优化：**
    * **Dataflow 架构（深入）：**  研究 Eyeriss 框架中“面向卷积神经网络的高能效 dataflow”设计空间。理解七类 dataflow（Weight Stationary、Output Stationary、Input Stationary、No Local Reuse、Row Stationary）及其内存带宽与能耗的权衡。
    * **空间计算 vs. 时间计算：**  设计将计算静态映射到 PE 的空间架构（如 TPU、Groq TSP）。与在时间上共享资源的时间架构（如 GPU）作对比。
    * **Dataflow 编译器：**  研究将张量运算映射到空间架构的 dataflow 编译器（Halide、TVM 调度、MLIR Linalg 方言）。理解支撑它们的仿射循环分析与多面体优化。

* **PE 设计：**
    * **脉动阵列设计：**  在 RTL 中实现参数化的矩阵乘脉动阵列——配置 PE 数量、数据精度与累加深度。通过 FPGA 或综合报告分析吞吐、延迟与面积。
    * **混合精度与近似计算：**  设计支持多种精度（FP32、FP16、BF16、INT8、INT4）且可在 runtime 配置精度的 PE。探索面向能效的近似计算（随机舍入、低精度累加）。
    * **稀疏加速器：**  研究利用权重与激活值稀疏性的架构——Nvidia 的 A100 结构化稀疏（2:4 稀疏）、SCNN（稀疏卷积神经网络加速器）与 Cambricon-X。在 FPGA 上实现稀疏 GEMM（矩阵-矩阵乘）。

* **片上网络（NoC）与存储系统：**
    * **NoC 设计：**  为多 PE 加速器设计片上互连——mesh、ring 与分层 NoC 拓扑。针对不同工作负载的通信模式分析带宽、延迟与功耗。
    * **Scratchpad vs. 缓存：**  理解 scratchpad（程序员管理的 SRAM）与基于缓存的片上存储之间的权衡。设计带 DMA 引擎的 scratchpad 控制器以完成批量数据搬运。
    * **HBM 接口设计：**  研究 HBM2/HBM3 接口要求——PHY、内存控制器设计与请求调度。理解片上带宽如何限制加速器性能，并据此设计存储子系统。

**资源：**


<details>
<summary>English original</summary>

* **Dataflow Optimization:**
    * **Dataflow Architectures (In-Depth):**  Study the "energy-efficient dataflow for CNN" design space from the Eyeriss framework. Understand the seven categories of dataflow (Weight Stationary, Output Stationary, Input Stationary, No Local Reuse, Row Stationary) and their memory bandwidth and energy trade-offs.
    * **Spatial vs. Temporal Compute:**  Design spatial architectures (like TPUs, Groq TSP) where computation is statically mapped to processing elements. Compare with temporal architectures (like GPUs) that share resources across time.
    * **Dataflow Compilers:**  Study dataflow compilers (Halide, TVM schedules, MLIR Linalg dialect) that map tensor operations onto spatial architectures. Understand the affine loop analysis and polyhedral optimization that underpins them.

* **Processing Element (PE) Design:**
    * **Systolic Array Design:**  Implement a parameterized systolic array for matrix multiply in RTL—configure PE count, data precision, and accumulation depth. Analyze throughput, latency, and area via FPGA or synthesis reports.
    * **Mixed-Precision and Approximate Computing:**  Design PEs that support multiple precisions (FP32, FP16, BF16, INT8, INT4) with configurable precision at runtime. Explore approximate computing (stochastic rounding, reduced-precision accumulation) for energy efficiency.
    * **Sparse Accelerators:**  Study architectures that exploit sparsity in weights and activations—Nvidia's A100 structured sparsity (2:4 sparsity), SCNN (sparse CNN accelerator), and Cambricon-X. Implement sparse GEMM on FPGA.

* **On-Chip Network (NoC) and Memory System:**
    * **NoC Design:**  Design on-chip interconnect for multi-PE accelerators—mesh, ring, and hierarchical NoC topologies. Analyze bandwidth, latency, and power for different workload communication patterns.
    * **Scratchpad vs. Cache:**  Understand the trade-offs between scratchpad (programmer-managed SRAM) and cache-based on-chip memory. Design scratchpad controllers with DMA engines for bulk data movement.
    * **HBM Interface Design:**  Study HBM2/HBM3 interface requirements—PHY, memory controller design, and request scheduling. Understand how on-chip bandwidth limits accelerator performance and design memory subsystems accordingly.

**Resources:**

</details>

* **"Eyeriss: A Spatial Architecture for Energy-Efficient Dataflow for CNN"（Chen et al., ISCA 2016）：**  CNN 加速器 dataflow 与能耗分析的奠基性论文。
* **"Efficient Processing of Deep Neural Networks"，作者 Sze、Chen、Yang、Emer：**  DNN 硬件加速领域的综合性著作与 MIT OCW 课程。
* **Timeloop 与 Accelergy：**  DNN 加速器的架构级建模与评估框架——将工作负载映射到架构，并估算性能、能耗与面积。
* **Halide** — [github.com/halide/Halide](https://github.com/halide/Halide)：面向快速、可移植的数据并行计算的语言（图像处理、GPU）。研究 schedule/algorithm 的分离，以及它如何映射到空间架构。

**项目：**

* **参数化脉动阵列：**  用 SystemVerilog 实现一个可扩展的脉动阵列（例如 16×16 PE），数据位宽可配置（INT8/FP16）。在 FPGA 上综合并测量 TOPS/W。
* **用 Timeloop 做 dataflow 分析：**  用 Timeloop 在假想加速器上评估 ResNet-50 的三种不同 dataflow（Weight Stationary、Output Stationary、Row Stationary）。比较能效，找出最优 dataflow。
* **FPGA 上的稀疏 GEMM：**  用 HLS 在 FPGA 上实现 2:4 结构化稀疏矩阵乘。与稠密 GEMM 比较性能和资源利用率。


**2. 芯片架构与物理设计**

* **RTL 到 GDS 流程：**
    * **综合与时序：**  用商业工具（Synopsys Design Compiler、Cadence Genus）或开源工具（Yosys + ABC）跑逻辑综合。理解工艺映射、多工艺角多模式（MCMM）时序，以及面积/功耗/时序优化。
    * **Floorplan 与布局：**  规划芯片 floorplan——为计算阵列、SRAM、I/O 和控制逻辑分配面积。理解布局密度、布线拥塞，以及电源/地分配网络规划。
    * **布线与签核：**  完成物理布线（全局 + 详细），执行签核检查——DRC（设计规则检查）、LVS（版图与原理图一致性检查），以及带寄生参数提取的时序签核（RC 提取、带 SPEF 的 STA）。


<details>
<summary>English original</summary>

* **"Eyeriss: A Spatial Architecture for Energy-Efficient Dataflow for CNN" (Chen et al., ISCA 2016):**  Foundational paper on CNN accelerator dataflow and energy analysis.
* **"Efficient Processing of Deep Neural Networks" by Sze, Chen, Yang, and Emer:**  Comprehensive book and MIT OCW course on DNN hardware acceleration.
* **Timeloop and Accelergy:**  Architecture-level modeling and evaluation framework for DNN accelerators—maps workloads to architectures and estimates performance, energy, and area.
* **Halide** — [github.com/halide/Halide](https://github.com/halide/Halide): Language for fast, portable data-parallel computation (image processing, GPU). Study the schedule/algorithm separation and how it maps to spatial architectures.

**Projects:**

* **Parameterized Systolic Array:**  Implement a scalable systolic array (e.g., 16×16 PEs) in SystemVerilog with configurable data width (INT8/FP16). Synthesize on FPGA and measure TOPS/W.
* **Dataflow Analysis with Timeloop:**  Use Timeloop to evaluate three different dataflows (Weight Stationary, Output Stationary, Row Stationary) for ResNet-50 on a hypothetical accelerator. Compare energy efficiency and identify optimal dataflow.
* **Sparse GEMM on FPGA:**  Implement a 2:4 structured sparse matrix multiply on FPGA using HLS. Compare performance and resource utilization against dense GEMM.


**2. Chip Architecture and Physical Design**

* **RTL-to-GDS Flow:**
    * **Synthesis and Timing:**  Run logic synthesis with commercial tools (Synopsys Design Compiler, Cadence Genus) or open-source (Yosys + ABC). Understand technology mapping, multi-corner multi-mode (MCMM) timing, and area/power/timing optimization.
    * **Floorplanning and Placement:**  Plan chip floorplan—allocate areas for compute arrays, SRAM, I/O, and control logic. Understand placement density, routing congestion, and power/ground distribution network planning.
    * **Routing and Signoff:**  Complete physical routing (global + detailed), perform signoff checks—DRC (Design Rule Check), LVS (Layout vs. Schematic), and timing signoff with parasitic extraction (RC extraction, STA with SPEF).

</details>

* **开源 EDA 与 OpenROAD：**
    * **OpenROAD Flow：**  针对一个小型 AI 加速器设计，运行 OpenROAD 开源的 RTL-to-GDS 流程。选用 SkyWater 130nm 或 GlobalFoundries 180nm PDK，以获得可制造的设计。
    * **Magic 与 KLayout：**  用 Magic 和 KLayout 查看并编辑 VLSI 版图。理解 GDSII 格式、层映射与设计规则的强制执行。
    * **Efabless Chipignite：**  了解多项目晶圆（MPW）shuttle 计划（Efabless chipIgnite、TinyTapeout），为学生设计提供低成本的实际芯片制造。

* **功耗分析与优化：**
    * **动态功耗与静态功耗：**  用基于仿真（VCD/SAIF）或统计估算的工具，分析动态功耗（开关活动 × 电容 × 电压²）与静态（漏电）功耗。
    * **时钟门控与电源域：**  实现细粒度时钟门控以关断空闲逻辑，并实现多电压电源域（UPF/CPF），以降低的电压为不活跃模块供电。
    * **IR Drop 与电迁移：**  分析电源分配网络（PDN）的 IR drop 与电迁移限制。优化金属宽度与 via 阵列，以在峰值电流负载下实现可靠供电。

**资源：**

* **OpenROAD Project：**  开源 RTL-to-GDS 流程，为学术界与研究提供商业级质量的工具。
* **TinyTapeout：**  教育性芯片制造 shuttle——设计一个小芯片，作为多项目晶圆的一部分完成流片。
* **"VLSI Physical Design: From Graph Partitioning to Timing Closure" by Kahng, Lienig, Markov, and Hu：**  全面的物理设计教材。

**项目：**

* **OpenROAD 加速器流片：**  把一个小型 RTL 设计（如 8×8 INT8 脉动阵列）跑通 SkyWater 130nm 上的 OpenROAD 流程。生成 GDSII，并分析时序、面积与功耗报告。
* **TinyTapeout 提交：**  设计一个最小的 AI 推理模块（如 4 元素点积单元），并提交到 TinyTapeout MPW shuttle 进行实际制造。
* **电源域分析：**  在仿真环境中实现一个多电压设计（计算核心 0.8V，I/O 1.2V）。分析漏电功耗的节省，并验证 UPF 的正确性。


**3. AI 芯片行业、商业与研究前沿**


<details>
<summary>English original</summary>

* **Open-Source EDA and OpenROAD:**
    * **OpenROAD Flow:**  Run the OpenROAD open-source RTL-to-GDS flow for a small AI accelerator design. Use the SkyWater 130nm or GlobalFoundries 180nm PDK for a manufacturable design.
    * **Magic and KLayout:**  View and edit VLSI layouts using Magic and KLayout. Understand GDSII format, layer mappings, and design rule enforcement.
    * **Efabless Chipignite:**  Explore multi-project wafer (MPW) shuttle programs (Efabless chipIgnite, TinyTapeout) for low-cost physical chip fabrication of student designs.

* **Power Analysis and Optimization:**
    * **Dynamic and Static Power:**  Analyze dynamic power (switching activity × capacitance × voltage²) and static (leakage) power using simulation-based (VCD/SAIF) or statistical estimation tools.
    * **Clock Gating and Power Domains:**  Implement fine-grained clock gating to disable idle logic and multi-voltage power domains (UPF/CPF) to supply inactive blocks at reduced voltage.
    * **IR Drop and Electromigration:**  Analyze power distribution network (PDN) IR drop and electromigration limits. Optimize metal width and via arrays for reliable power delivery under peak current loads.

**Resources:**

* **OpenROAD Project:**  Open-source RTL-to-GDS flow with commercial-quality tools for academia and research.
* **TinyTapeout:**  Educational chip fabrication shuttle—design a small chip and get it fabricated as part of a multi-project wafer.
* **"VLSI Physical Design: From Graph Partitioning to Timing Closure" by Kahng, Lienig, Markov, and Hu:**  Comprehensive physical design textbook.

**Projects:**

* **OpenROAD Accelerator Tape-Out:**  Take a small RTL design (e.g., an 8×8 INT8 systolic array) through the OpenROAD flow on SkyWater 130nm. Generate GDSII and analyze timing, area, and power reports.
* **TinyTapeout Submission:**  Design a minimal AI inference block (e.g., a 4-element dot product unit) and submit it to a TinyTapeout MPW shuttle for physical fabrication.
* **Power Domain Analysis:**  Implement a multi-voltage design (compute core at 0.8V, I/O at 1.2V) in a simulation environment. Analyze leakage power savings and verify UPF correctness.


**3. AI Chip Industry, Business, and Research Frontier**

</details>

* **AI 芯片版图：**
    * **商用 AI 芯片分析：**  研究商用 AI 芯片的架构——Nvidia H100（SXM5）、AMD MI300X（CDNA3）、Google TPUv4、Cerebras WSE-3、Groq TSP、Graphcore IPU 与 SambaNova RDU。比较性能、内存带宽、精度支持与总拥有成本。
    * **推理芯片与训练芯片：**  理解训练加速器（高精度、大内存、灵活）与推理加速器（低延迟、低功耗、量化、固定功能）不同的设计取向。
    * **边缘 AI 芯片：**  研究边缘推理芯片——Apple Neural Engine、Qualcomm Hexagon DSP、Hailo-8、Kendryte K210——及其面向功耗受限嵌入式部署的设计权衡。
    * **RISC-V AI 市场：**  分析 RISC-V 最先能在哪些领域胜出——工业边缘、机器人、AI 传感器、汽车控制器与开发者平台——以及 CUDA、Arm 和 x86 生态在哪些领域仍然更强。

* **Chiplet 与先进封装：**
    * **Chiplet 架构：**  理解基于 chiplet 的设计，即把一颗大芯片拆分为多个更小的 die，通过高带宽 die-to-die 互连（UCIe、BoW、AIB、HBI）相连。
    * **2.5D 与 3D 集成：**  研究用于集成计算 die 与内存 die 的 2.5D 封装（HBM 置于 interposer 上，如 AMD Instinct MI 系列）与 3D 堆叠（TSMC SoIC、Intel Foveros）。
    * **面向 Chiplet 的设计：**  理解 die-to-die 接口的 PHY 设计、堆叠 die 中的供电挑战，以及 3D 集成 AI 芯片的热管理。

* **研究方向：**
    * **神经形态计算：**  研究基于脉冲的神经形态架构（Intel Loihi、IBM TrueNorth）及其在事件驱动、稀疏、超低功耗 AI 处理上的潜在优势。
    * **存内计算与近存计算：**  探索在 SRAM 或 DRAM 阵列内部完成计算的存内计算（CIM）架构，大幅降低数据搬运能耗——这是 AI 推理中的主要成本。
    * **光子与模拟 AI：**  综述新兴技术路线——光学神经网络（Lightelligence、Lightmatter）、模拟存内计算（IBM PCM、Mythic）——及其可能的商业化路径。

**资源：**


<details>
<summary>English original</summary>

* **AI Chip Landscape:**
    * **Commercial AI Chip Analysis:**  Study the architectures of commercial AI chips—Nvidia H100 (SXM5), AMD MI300X (CDNA3), Google TPUv4, Cerebras WSE-3, Groq TSP, Graphcore IPU, and SambaNova RDU. Compare performance, memory bandwidth, precision support, and total cost of ownership.
    * **Inference vs. Training Chips:**  Understand the different design points for training (high-precision, large memory, flexible) vs. inference (low-latency, low-power, quantized, fixed function) accelerators.
    * **Edge AI Chips:**  Study edge inference chips—Apple Neural Engine, Qualcomm Hexagon DSP, Hailo-8, Kendryte K210—and their design trade-offs for power-constrained embedded deployment.
    * **RISC-V AI Market:**  Analyze where RISC-V can win first—industrial edge, robotics, AI sensors, automotive controllers, and developer platforms—and where CUDA, Arm, and x86 ecosystems remain stronger.

* **Chiplets and Advanced Packaging:**
    * **Chiplet Architecture:**  Understand chiplet-based design where a large chip is disaggregated into smaller dies connected via high-bandwidth die-to-die interconnects (UCIe, BoW, AIB, HBI).
    * **2.5D and 3D Integration:**  Study 2.5D packaging (HBM on an interposer, as in AMD Instinct MI series) and 3D stacking (TSMC SoIC, Intel Foveros) for integrating compute and memory dies.
    * **Design for Chiplets:**  Understand PHY design for die-to-die interfaces, power delivery challenges in stacked dies, and thermal management for 3D-integrated AI chips.

* **Research Directions:**
    * **Neuromorphic Computing:**  Study spike-based neuromorphic architectures (Intel Loihi, IBM TrueNorth) and their potential advantages for event-driven, sparse, ultra-low-power AI processing.
    * **In-Memory and Near-Memory Computing:**  Explore compute-in-memory (CIM) architectures that perform computations within SRAM or DRAM arrays, dramatically reducing data movement energy—the dominant cost in AI inference.
    * **Photonic and Analog AI:**  Survey emerging modalities—optical neural networks (Lightelligence, Lightmatter), analog compute-in-memory (IBM PCM, Mythic), and their potential paths to commercialization.

**Resources:**

</details>

* **Chip Architects Podcast 与 SemiAnalysis：**  AI 芯片架构、竞争格局与技术趋势的行业分析。
* **Hot Chips 与 ISSCC 会议论文集：**  领先公司展示 AI 芯片架构的年度会议——理解最先进设计的主要来源。
* **“Demystifying AI Chipmakers”（各类分析师报告）：**  AI 芯片初创公司和现有厂商的业务与技术格局。
* **RISC-V AI 芯片设计与市场课程：**  [聚焦课程](03-RISC-V-AI芯片设计与市场/Guide.md) 涉及 SpacemiT、RISC-V 向量 AI、软件使能与边缘 AI 市场契合度。
* **智能体化芯片设计 2026：**  [专题课程](01-智能体芯片设计2026/Guide.md) 讲解如何使用大语言模型和 agent 贯穿 RTL 到硅的流程 —— RTL 生成、验证与智能体化 EDA —— 衔接 [AI 智能体开发 2026](../../阶段3-人工智能/04-智能体AI与ML工程/01-智能体AI与生成式AI/Guide.md) 课程。基于 [Chip-Design-LLM-Zoo](https://iprc-dip.github.io/Chip-Design-LLM-Zoo/) 排行榜。

**项目：**

* **AI 芯片架构比较：**  对三款 AI 芯片（例如 H100、MI300X、TPUv4）针对大语言模型训练，在性能、内存带宽、精度和 TCO 方面进行系统比较。以技术报告形式呈现结果。
* **RISC-V AI 平台评测：**  评测一款 RISC-V AI 芯片，例如 SpacemiT K3、Tenstorrent、SiFive Intelligence、MIPS S8200、Semidynamics Cervell 或 Andes vector AI cores。分析架构、软件栈、工作负载契合度、市场地位和采用风险。
* **Chiplet 设计研究：**  设计一个简单的 chiplet 系统，包含一个计算 die 和一个等效 HBM 的存储 die。使用 UCIe 规范对 die 间带宽、延迟和功耗建模。
* **研究提案：**  撰写一份 2 页的研究提案，针对特定空白（例如长上下文 Transformer 推理、稀疏 GNN 加速）提出新颖的 AI 加速器架构。包括工作负载分析、提出的架构和预期收益。

---


<details>
<summary>English original</summary>

* **Chip Architects Podcast and SemiAnalysis:**  Industry analysis of AI chip architectures, competitive landscape, and technology trends.
* **Hot Chips and ISSCC Conference Proceedings:**  Annual conferences where leading companies present AI chip architectures—primary source for understanding state-of-the-art designs.
* **"Demystifying AI Chipmakers" (Various analyst reports):**  Business and technology landscape of AI chip startups and incumbents.
* **RISC-V AI Chip Design and Market Course:**  [Focused course](03-RISC-V-AI芯片设计与市场/Guide.md) on SpacemiT, RISC-V vector AI, software enablement, and edge AI market fit.
* **Agentic Chip Design 2026:**  [Special course](01-智能体芯片设计2026/Guide.md) on using LLMs and agents across the RTL-to-silicon flow — RTL generation, verification, and agentic EDA — bridging the [AI Agent Development 2026](../../阶段3-人工智能/04-智能体AI与ML工程/01-智能体AI与生成式AI/Guide.md) course. Grounded in the [Chip-Design-LLM-Zoo](https://iprc-dip.github.io/Chip-Design-LLM-Zoo/) leaderboard.

**Projects:**

* **AI Chip Architecture Comparison:**  Perform a systematic comparison of three AI chips (e.g., H100, MI300X, TPUv4) across performance, memory bandwidth, precision, and TCO for LLM training. Present findings as a technical report.
* **RISC-V AI Platform Review:**  Review a RISC-V AI chip such as SpacemiT K3, Tenstorrent, SiFive Intelligence, MIPS S8200, Semidynamics Cervell, or Andes vector AI cores. Analyze architecture, software stack, workload fit, market position, and adoption risk.
* **Chiplet Design Study:**  Design a simple chiplet system with a compute die and an HBM-equivalent memory die. Model die-to-die bandwidth, latency, and power using UCIe specifications.
* **Research Proposal:**  Write a 2-page research proposal for a novel AI accelerator architecture targeting a specific gap (e.g., long-context transformer inference, sparse GNN acceleration). Include workload analysis, proposed architecture, and expected benefits.


---

</details>

## 4. 自定义 ML 框架工程：NVIDIA 原生设计

> 本节直接源自一个关键的工程问题：tinygrad 声称在单块 NVIDIA GPU 上会比 PyTorch 快 2 倍——但这是真的吗？如果不是，要真正造出一个在 NVIDIA 硬件上胜出的框架，究竟需要什么？

---

### 4.1 tinygrad 的 2 倍声明：一次诚实的评估

tinygrad 的 README 写道：*“当它能在 1 块 NVIDIA GPU 上以 2 倍于 PyTorch 的速度复现常见论文时，就会退出 alpha。”* 这是 alpha 里程碑的工程退出条件，而非当前的性能现实。

**benchmark 实际显示的内容（截至 2026 年初）：**

| 工作负载 | tinygrad 对比 PyTorch | 备注 |
|---|---|---|
| **训练（NVIDIA）** | 约 **慢** 2 倍 | tinygrad 正积极试图弥合的差距——PyTorch+cuDNN+torch.compile 与 NVIDIA 深度协同设计 |
| **推理（NVIDIA，简单算子）** | 在峰值带宽的 ~10% 以内 | 在 cuDNN 优势最小的带宽受限推理中具备竞争力 |
| **非 NVIDIA 硬件** | 常常是现有**最快**的 | 在 Qualcomm、AMD、Apple Metal 上真正胜出，那里不存在 cuDNN 的对等物 |
| **openpilot（Snapdragon 845）** | 比 SNPE 快 2 倍 | 最接近真正 2 倍胜出的例子——但发生在 Qualcomm，而非 NVIDIA |

**为什么 NVIDIA 尤其困难：**

1. **cuDNN 的护城河**：PyTorch 调用 `cudnnConvolutionForward()`，它从数百个手工调优、针对特定架构的 kernel（Volta、Ampere、Hopper、Blackwell）中挑选。这些代表了 NVIDIA 十多年的工程师工时，且是闭源的。代码生成系统无法从通用循环嵌套中复现它们。

2. **FlashAttention-3 集成**：PyTorch 的 `scaled_dot_product_attention()`（SDPA）在 Hopper 上分派到 FlashAttention-3——借助 Hopper 特有的硬件特性（TMA、WGMMA、warp specialization）达到 **740 TFLOP/s（H100 峰值吞吐的 75%）**。tinygrad 无法从其 12 个原语 UOp 中推导出这个 kernel。

3. **torch.compile + CUDA Graphs**：PyTorch 2.x 对模型做 trace，为 elementwise/归约算子生成 Triton kernel，对重算子调用 cuDNN/cuBLAS，并通过 `cudaGraphLaunch()` 重放整个序列——消除全部 CPU 开销，每个推理步骤的 Python 分派成本为零。

4. **多 GPU 的 NCCL**：使用 NVLink 拓扑感知算法的 all-reduce、all-gather 和 reduce-scatter。在 NVSwitch 系统上，多播寻址把每一步的数据量降为一条消息，与 GPU 数量无关。

5. **NVIDIA 协同设计**：NVIDIA 向 PyTorch 提供早期硬件访问，并在每代 GPU 发布前验证 cuDNN/cuBLAS 的兼容性。tinygrad 只能在事后逆向工程或重新实现这些能力。

**tinygrad 通往 2 倍里程碑的现实路径**：最有可能出现在 **Transformer 推理工作负载**（LLaMA decode，即逐 token 生成阶段）上——其中 CUDA Graphs 消除 CPU 开销，FP8 使 GEMM（矩阵-矩阵乘）吞吐翻倍，惰性调度器把反量化融合进相邻的 elementwise 算子——而这些 PyTorch eager 默认都做不到。这是一个狭窄但真实的胜出条件，也正是这个 NVIDIA 原生框架应当竞争的地方。

---


<details>
<summary>English original</summary>

**4. Custom ML Framework Engineering: NVIDIA-Native Design**

> This section grows directly from a critical engineering question: tinygrad claims it will be 2x faster than PyTorch on a single NVIDIA GPU — but is that true, and if not, what would it actually take to build a framework that genuinely wins on NVIDIA hardware?

---

**4.1 The Tinygrad 2x Claim: An Honest Assessment**

Tinygrad's README states: *"Will leave alpha when it can reproduce common papers 2x faster than PyTorch on 1 NVIDIA GPU."* This is an engineering exit condition for the alpha milestone, not a current performance reality.

**What the benchmarks actually show (as of early 2026):**

| Workload | Tinygrad vs PyTorch | Notes |
|---|---|---|
| **Training (NVIDIA)** | Roughly 2x **slower** | The gap tinygrad is actively trying to close — PyTorch+cuDNN+torch.compile is deeply co-engineered with NVIDIA |
| **Inference (NVIDIA, simple ops)** | Within ~10% of peak bandwidth | Competitive for memory-bound inference where cuDNN advantage is smallest |
| **Non-NVIDIA hardware** | Often **fastest** available | Genuine win on Qualcomm, AMD, Apple Metal where no cuDNN equivalent exists |
| **Openpilot (Snapdragon 845)** | 2x faster than SNPE | The closest thing to a real 2x win — but on Qualcomm, not NVIDIA |

**Why NVIDIA is specifically hard:**

1. **The cuDNN moat**: PyTorch calls `cudnnConvolutionForward()` which selects from hundreds of hand-tuned, architecture-specific kernels (Volta, Ampere, Hopper, Blackwell). These represent 10+ years of NVIDIA engineer-hours and are closed-source. A codegen system cannot reproduce them from generic loop nests.

2. **FlashAttention-3 integration**: PyTorch's `scaled_dot_product_attention()` (SDPA) dispatches to FlashAttention-3 on Hopper — achieving **740 TFLOP/s (75% of peak H100 throughput)** via Hopper-specific hardware features (TMA, WGMMA, warp specialization). Tinygrad cannot derive this kernel from its 12 primitive UOps.

3. **torch.compile + CUDA Graphs**: PyTorch 2.x traces the model, generates Triton kernels for elementwise/reduction ops, calls cuDNN/cuBLAS for heavy ops, and replays the full sequence via `cudaGraphLaunch()` — eliminating all CPU overhead with zero Python dispatch cost per inference step.

4. **NCCL for multi-GPU**: All-reduce, all-gather, and reduce-scatter using NVLink topology-aware algorithms. On NVSwitch systems, multicast addressing reduces the data volume to one message per step regardless of GPU count.

5. **NVIDIA co-engineering**: NVIDIA provides PyTorch with early hardware access and validates cuDNN/cuBLAS compatibility before each GPU generation launches. Tinygrad must reverse-engineer or re-implement those capabilities afterward.

**Tinygrad's realistic path to the 2x milestone**: Most likely on a **transformer inference workload** (LLaMA decode) where CUDA Graphs eliminate CPU overhead, FP8 doubles GEMM throughput, and the lazy scheduler fuses dequantization into adjacent elementwise ops — none of which PyTorch eager does by default. This is a narrow but real win condition, and it is exactly where this NVIDIA-native framework should compete.

---

</details>

### 4.2 NVIDIA 硬件提供而通用后端缺失的能力

理解这些特性，对框架设计与 AI 芯片设计都是必需的——这是软硬件协同设计的软件侧。

**Tensor Core：三代演进**

| API 层级 | 架构 | PTX 指令 | 粒度 |
|---|---|---|---|
| WMMA (C++) | Volta (sm_70+) | `mma.sync.aligned.*` | warp（32 线程）16×16 fragment |
| MMA (PTX) | Ampere 架构（sm_80+） | `mma.sync.aligned.m16n8k16.*` | warp 级，精度控制更细（TF32、BF16、INT8） |
| WGMMA (PTX) | Hopper（sm_90） | `wgmma.mma_async.sync.aligned.*` | warpgroup（128 线程），异步——与 TMA 数据搬运重叠 |

通用的循环 codegen 无法自动发现乘-归约模式对应 `wgmma.mma_async`。框架必须显式识别这一图模式，并生成正确的 PTX。

**存储系统：从 cp.async 到 TMA**

* **寄存器 → 共享内存 → 全局内存**是经典的 GPU 存储层次。每一跳都受延迟与带宽约束。
* **`cp.async`（Ampere，sm_80+）：** 从全局内存直接拷贝到共享内存，*不经过寄存器*——为计算腾出每 SM 32K 个寄存器。可实现**软件流水线双缓冲**：在计算分块 N 的同时，把分块 N+1 载入共享内存。
* **TMA —— Tensor Memory Accelerator（Hopper，sm_90）：** 专用硬件单元，接受张量描述符（基址指针、shape、步长、swizzle pattern），在全局内存与共享内存之间自主搬运完整的张量分块。由单个线程发出 TMA 指令，其余线程继续计算。这消除了线程代码中的所有地址运算。

**warp 级原语**

* **`__shfl_xor_sync()`**：一个线程直接读取另一个线程的寄存器——无需共享内存。使 warp 级归约只需 `log₂(32) = 5` 条指令，而不是 32 次 `atomicAdd` 调用。任何对 warp 规模问题生成共享内存归约的框架，都白白丢掉了 5× 的延迟收益。
* **Warp vote 函数**（`__ballot_sync`、`__any_sync`、`__all_sync`）：在一条指令中对全部 32 个线程做集合谓词判断——用于条件执行与稀疏模式检测。

**常驻 kernel**

在常规 GPU 执行中，每个 Cooperative Thread Array（CTA）处理一个分块后退出。随后调度器启动下一个 CTA。对于大量小分块，这种重新调度开销相当可观。

常驻 kernel 让 CTA 跨多个分块存活：CTA 完成分块 N 后，原子地取回分块 N+1 的索引，然后继续。这是 Hopper 上 warp specialization 所需的执行模型——生产者 warpgroup 发出 TMA 请求，消费者 warpgroup 持续运行 WGMMA。CUTLASS 3.x 对所有 Hopper GEMM 默认采用常驻 kernel。

**warp specialization（Hopper）**

CTA 内的不同 warpgroup 走不同的代码路径：
* **生产者 warpgroup**：为 Q、K、V 分块发出 TMA load。通过 `mbarrier` 发出完成信号。
* **消费者 warpgroup**：等待 `mbarrier`，对已载入的分块运行 `wgmma.mma_async`，累加到输出。

这种生产者-消费者重叠，是 FlashAttention-3 达到 H100 理论吞吐 75% 的主要原因。它需要结构化控制流，而这是单程序多线程（SPMT）模型无法表达的。

**其他 NVIDIA 专有能力**

* **CUDA Graphs**：把 kernel 启动记录成 DAG（`cudaGraph_t`），用 `cudaGraphLaunch()` 重放——零 CPU 开销、零 Python 派发、每步零 CUDA 驱动调用。对于静态 shape 的 LLM 推理：在一串本已很快的 kernel 之上，相比 eager 执行仍有 **1.2–3× 加速**。
* **2:4 结构化稀疏（Ampere+）**：每 4 个值一组，恰好 2 个必须为零。硬件存储压缩后的值 + 2-bit 索引，并在搬运过程中解压，以 **2× Tensor Core 吞吐**执行。需要以 2:4 剪枝进行训练；压缩后的权重送入 `cuSPARSELt`。
* **L2 缓存持久化（Ampere+）**：用 `cudaAccessPropertyPersisting` 标记一块缓冲区，使其跨 kernel 启动驻留在 40 MB L2 中。对于每步都复用 KV-cache 的 Transformer 推理，这消除了重复的全局内存读取。

---


<details>
<summary>English original</summary>

**4.2 What NVIDIA Hardware Provides That Generic Backends Miss**

Understanding these features is mandatory for both framework design and AI chip design — this is the software side of hardware-software co-design.

**Tensor Cores: Three Generations**

| API Level | Architecture | PTX Instruction | Granularity |
|---|---|---|---|
| WMMA (C++) | Volta (sm_70+) | `mma.sync.aligned.*` | Warp (32 threads) 16×16 fragments |
| MMA (PTX) | Ampere (sm_80+) | `mma.sync.aligned.m16n8k16.*` | Warp-level, finer precision control (TF32, BF16, INT8) |
| WGMMA (PTX) | Hopper (sm_90) | `wgmma.mma_async.sync.aligned.*` | Warpgroup (128 threads), asynchronous — overlaps with TMA data movement |

A generic loop codegen cannot automatically discover that a reduce-of-multiply pattern maps to `wgmma.mma_async`. The framework must explicitly detect this graph pattern and emit the correct PTX.

**Memory System: From cp.async to TMA**

* **Registers → Shared Memory → Global Memory** is the classic GPU memory hierarchy. Each hop has latency and bandwidth constraints.
* **`cp.async` (Ampere, sm_80+):** Copies from global memory directly to shared memory *without routing through registers* — freeing 32K registers per SM for compute. Enables **software-pipelined double buffering**: load tile N+1 into shared memory simultaneously with computing on tile N.
* **TMA — Tensor Memory Accelerator (Hopper, sm_90):** A dedicated hardware unit that accepts a tensor descriptor (base pointer, shape, stride, swizzle pattern) and autonomously moves full tensor tiles between global and shared memory. A single thread issues the TMA instruction while all other threads continue computing. This eliminates all address arithmetic from thread code.

**Warp-Level Primitives**

* **`__shfl_xor_sync()`**: A thread reads another thread's register directly — no shared memory needed. Enables warp-level reductions in `log₂(32) = 5` instructions instead of 32 `atomicAdd` calls. Any framework emitting shared-memory reductions for warp-sized problems is leaving 5× latency on the table.
* **Warp vote functions** (`__ballot_sync`, `__any_sync`, `__all_sync`): Collective predicates across all 32 threads in one instruction — used for conditional execution and sparse pattern detection.

**Persistent Kernels**

In conventional GPU execution, each Cooperative Thread Array (CTA) processes one tile and exits. The scheduler then launches the next CTA. For many small tiles, this re-scheduling overhead is significant.

Persistent kernels keep CTAs alive across multiple tiles: a CTA finishes tile N, atomically fetches tile N+1's index, and continues. This is the execution model required for warp specialization on Hopper — producer warpgroups issue TMA requests while consumer warpgroups run WGMMA continuously. CUTLASS 3.x uses persistent kernels as the default for all Hopper GEMMs.

**Warp Specialization (Hopper)**

Different warpgroups within a CTA take different code paths:
* **Producer warpgroup**: Issues TMA loads for Q, K, V tiles. Signals completion via `mbarrier`.
* **Consumer warpgroup**: Waits on `mbarrier`, runs `wgmma.mma_async` on loaded tiles, accumulates to output.

This producer-consumer overlap is the primary reason FlashAttention-3 reaches 75% of H100 theoretical throughput. It requires structured control flow that is impossible to express in a single-program-multiple-thread (SPMT) model.

**Additional NVIDIA-Specific Capabilities**

* **CUDA Graphs**: Record kernel launches into a DAG (`cudaGraph_t`), replay with `cudaGraphLaunch()` — zero CPU overhead, zero Python dispatch, zero CUDA driver call per step. For LLM inference with static shapes: **1.2–3× speedup** over eager execution on top of an already-fast kernel sequence.
* **2:4 Structured Sparsity (Ampere+)**: In every group of 4 values, exactly 2 must be zero. The hardware stores compressed values + 2-bit indices and executes at **2× Tensor Core throughput** with decompression in-flight. Requires training with 2:4 pruning; compressed weights fed to `cuSPARSELt`.
* **L2 Cache Persistence (Ampere+)**: Mark a buffer with `cudaAccessPropertyPersisting` so it stays in the 40 MB L2 across kernel launches. For transformer inference where the KV-cache is reused every step, this eliminates repeated global memory fetches.

---

</details>

### 4.3 设计 NVIDIA 原生框架：架构

**主要目标：推理。次要目标：NVIDIA 上的 Transformer 训练。** 这一聚焦收窄了竞争格局，也让胜出条件变得具体。

---

**竞争格局：你真正在对抗的是谁**

| 竞品 | 优势 | 可利用的弱点 |
|---|---|---|
| **TensorRT** | 卷积推理最快；逐层插件融合 | 插件系统无法*跨*插件边界融合；自定义算子需手工改图；对非标准模型的开发者体验极差 |
| **TRT-LLM** | NVIDIA 官方的大语言模型推理栈；H100 上支持 FP8 | C++ 内部实现不透明；迭代周期慢；不可魔改；与 TensorRT 的图划分强耦合 |
| **vLLM** | PagedAttention 实现动态 KV-cache；连续批处理 | PyTorch eager 后端——无 CUDA Graphs，每步的 CPU 派发开销高；算子融合极少 |
| **PyTorch eager + torch.compile** | 迭代快；适合训练 | 编译开销；逐 kernel 的 Triton 路线在推理阶段错过跨层融合机会 |
| **ONNX Runtime + TensorRT EP** | 适合固定图的 ONNX 模型 | 无法表达动态控制流；在 ONNX 导入边界处丢失融合机会 |

**结构性突破口**：一个惰性求值的全图调度器，把整个 attention + FFN 块视为单一可融合单元，能消除 TensorRT 的插件边界模型无法消除的中间缓冲区分配。再叠加 CUDA Graph 重放与面向推理的内存管理（PagedAttention），这就是实打实的架构优势——不是营销话术。

---

**从 Tinygrad 保留什么**

| 组件 | 保留理由 |
|---|---|
| 惰性求值 + UOp DAG | 能看到完整模型图——跨越 attention + FFN + 归一化边界做融合，这是逐 kernel 编译器做不到的 |
| `ShapeTracker` (movement ops) | 零拷贝 reshape/permute/expand——对无缓冲区拷贝的 KV-cache 管理至关重要 |
| 调度器（kernel 边界检测） | 最有价值的部分：决定哪些算子合并为一次 kernel 启动 |
| BEAM 搜索自动调优器 | 针对每代 GPU 找出最优分块尺寸、展开因子、shared memory 布局 |
| `Tensor` API + `nn` 模块 | 兼容 PyTorch——无需转换即可加载任意 PyTorch 模型权重 |
| Python 优先、可魔改的代码库 | 整个编译器可见；新的量化方案与 kernel 模板可快速迭代 |

---


<details>
<summary>English original</summary>

**4.3 Designing an NVIDIA-Native Framework: Architecture**

**Primary target: inference. Secondary target: transformer training on NVIDIA.** This focus narrows the competitor landscape and makes the win conditions concrete.

---

**Competitor Landscape: Who You Are Actually Fighting**

| Competitor | Strength | Exploitable Weakness |
|---|---|---|
| **TensorRT** | Fastest conv inference; per-layer plugin fusion | Plugin system cannot fuse *across* plugin boundaries; requires manual graph surgery for custom ops; terrible developer experience for non-standard models |
| **TRT-LLM** | NVIDIA's official LLM inference stack; FP8 on H100 | Opaque C++ internals; slow iteration cycle; not hackable; tightly coupled to TensorRT's graph partitioning |
| **vLLM** | PagedAttention for dynamic KV-cache; continuous batching | PyTorch eager backend — no CUDA Graphs, high CPU dispatch overhead per step; operator fusion is minimal |
| **PyTorch eager + torch.compile** | Fast iteration; good for training | Compile overhead; per-kernel Triton approach misses cross-layer fusion opportunities at inference |
| **ONNX Runtime + TensorRT EP** | Good for fixed-graph ONNX models | Cannot express dynamic control flow; loses fusion opportunities at ONNX import boundaries |

**The structural opening**: A lazy-evaluation whole-graph scheduler that sees the full attention + FFN block as a single fusible unit can eliminate intermediate buffer allocations that TensorRT's plugin-boundary model cannot. Combined with CUDA Graph replay and inference-specific memory management (PagedAttention), this is a real architectural advantage — not a marketing claim.

---

**What to Keep from Tinygrad**

| Component | Why Keep It |
|---|---|
| Lazy evaluation + UOp DAG | Sees the full model graph — fuses across attention + FFN + normalization boundaries that per-kernel compilers cannot |
| `ShapeTracker` (movement ops) | Zero-copy reshape/permute/expand — critical for KV-cache management without buffer copies |
| Scheduler (kernel boundary detection) | The most valuable piece: decides which ops collapse into one kernel launch |
| BEAM search auto-tuner | Finds optimal tile sizes, unroll factors, shared memory layouts per GPU generation |
| `Tensor` API + `nn` module | PyTorch-compatible — load any PyTorch model weight without conversion |
| Python-first, hackable codebase | Entire compiler visible; rapid iteration on new quantization schemes and kernel templates |

---

</details>

**The Two-Mode Compiler Pipeline**

```
                    ┌─────────────────────────────────┐
                    │     UOp DAG (tinygrad base)      │
                    └────────────┬────────────────────┘
                                 │
                    ┌────────────▼────────────────────┐
                    │   Pattern Matcher (NVIDIA Ext.)  │
                    │  matmul-reduce → WGMMA/MMA       │
                    │  softmax(Q@K.T)@V → ATTN_KERNEL  │
                    │  elementwise chain → FUSED_EW    │
                    │  fixed-shape exec → GRAPH_REGION │
                    └────────────┬────────────────────┘
                                 │
              ┌──────────────────┼──────────────────────┐
              │ INFERENCE PATH   │                       │ TRAINING PATH
              ▼                  │                       ▼
  ┌───────────────────────┐      │         ┌────────────────────────────┐
  │ Inference Scheduler   │      │         │ Training Scheduler         │
  │ - Static shape spec.  │      │         │ - Gradient graph extension │
  │ - KV-cache allocation │      │         │ - Gradient checkpointing   │
  │ - Batch slot mgmt     │      │         │ - BF16 loss scaling        │
  │ - FP8 quant pipeline  │      │         │ - NCCL all-reduce hooks    │
  └───────────┬───────────┘      │         └──────────────┬─────────────┘
              │                  │                        │
              ▼                  │                        ▼
  ┌───────────────────────┐      │         ┌────────────────────────────┐
  │ Kernel Template Lib   │      │         │ Kernel Template Lib        │
  │ [Inference]           │      │         │ [Training]                 │
  │ FlashDecoding (FA-3)  │      │         │ FlashAttention-3 (forward) │
  │ PagedAttention KV     │      │         │ FlashAttention-3 (backward)│
  │ FP8 E4M3 GEMM         │      │         │ BF16/TF32 GEMM (CUTLASS)  │
  │ INT8/INT4 GEMM        │      │         │ Fused AdamW (FP8 master)  │
  │ 2:4 sparse weights    │      │         │ Gradient all-reduce        │
  └───────────┬───────────┘      │         └──────────────┬─────────────┘
              │                  │                        │
              └──────────────────┼──────────────────────┘
                                 │
                    ┌────────────▼────────────────────┐
                    │      PTX Emitter (NVIDIA)        │
                    │  wgmma.mma_async (Hopper)        │
                    │  mma.sync.aligned (Ampere)       │
                    │  cp.async.bulk.tensor (TMA)      │
                    │  __shfl_xor_sync (warp reduce)   │
                    │  mbarrier.* (Hopper sync)        │
                    └────────────┬────────────────────┘
                                 │
                    ┌────────────▼────────────────────┐
                    │     CUDA Graph Runtime           │
                    │  cudaGraphInstantiate() — once   │
                    │  cudaGraphLaunch() — every step  │
                    │  Shape-keyed graph cache         │
                    │  Dynamic batch: graph pool       │
                    └─────────────────────────────────┘
```

**推理优先：关键组件**

**FP8 E4M3 推理流水线（仅 H100）**

H100 Tensor Core 支持 FP8（E4M3 与 E5M2 格式），**吞吐是 FP16 的 2×** —— 单块 H100 SXM5 上 FP8 为 3.9 PFLOP/s，FP16 为 1.9 PFLOP/s。要利用这一点：
- 权重离线量化为 FP8 E4M3，per-tensor 或 per-channel 缩放因子与权重一同存储
- 激活值在线量化，通过融合进前一个逐元素算子的快速 FP8 转换 kernel 完成
- GEMM（矩阵-矩阵乘）kernel 使用 `wgmma.mma_async`，累加使用 `.f8f8f32`
- 输出反量化融合进 GEMM 之后的激活 kernel（无额外内存往返）

整图调度器在这里至关重要：它可以把反量化 → 激活 → 重量化这条链融合成两个 GEMM 之间的单个逐元素 kernel。逐 kernel 编译器（Triton、TensorRT 插件）通常会把中间的 FP32 结果物化，从而失去融合机会。

**PagedAttention KV-Cache**

在生产环境的 LLM 推理服务中，同一批里的不同请求 KV-cache 长度各不相同（有的在第 10 个 token，有的在第 8000 个 token）。PagedAttention（vLLM 的核心创新）的解法是：用固定大小的**页**（例如 16 个 token 一块）管理 KV-cache，按需分配页，而不是为每个请求预分配一个最大长度的缓冲区。

集成到框架中：
- KV-cache 分配器在 VRAM 中维护一个固定大小页缓冲区的池
- 调度器知道哪些页属于哪个请求（每个请求一张 block table）
- attention kernel 模板把 block table 作为输入，在内部处理非连续 KV-cache 读取 —— 这是带分页访存的 FlashDecoding 变体

**长上下文 decode 的 FlashDecoding**

标准 FlashAttention 针对 **prefill**（首字前的整段计算）阶段优化（处理完整 prompt，算力受限）。在 **decode**（逐 token 生成阶段，一次生成一个 token 且 KV-cache 很长）期间，该 kernel 是内存带宽受限的，需要不同的分块调度策略：
- FlashDecoding 把 KV 序列切分到各线程块，每个块负责 KV 维度的一段子区间
- 部分 softmax 结果并行算出，然后跨块规约
- 在配 32K token KV-cache 的 H100 上：decode 时 FlashDecoding 比朴素 FlashAttention 快约 5×

**带以 shape 为键的池的 CUDA Graphs**


<details>
<summary>English original</summary>

---

**Inference-First: The Critical Components**

**FP8 E4M3 Inference Pipeline (H100 only)**

H100 Tensor Cores support FP8 (E4M3 and E5M2 formats) at **2× the FP16 throughput** — 3.9 PFLOP/s for FP8 vs 1.9 PFLOP/s for FP16 on one H100 SXM5. To exploit this:
- Weights quantized offline to FP8 E4M3 with per-tensor or per-channel scaling factors stored alongside
- Activations quantized online via a fast FP8-cast kernel fused into the preceding elementwise op
- The GEMM kernel uses `wgmma.mma_async` with `.f8f8f32` accumulation
- Output dequantization fused into the post-GEMM activation kernel (no extra memory round-trip)

The whole-graph scheduler is essential here: it can fuse the dequantization → activation → requantization chain as a single elementwise kernel between two GEMMs. Per-kernel compilers (Triton, TensorRT plugins) typically materialize intermediate FP32 results, losing the fusion opportunity.

**PagedAttention KV-Cache**

In production LLM serving, different requests in a batch have different KV-cache lengths (some are on token 10, others on token 8000). PagedAttention (the key innovation in vLLM) solves this by managing KV-cache in fixed-size **pages** (blocks of, e.g., 16 tokens), allocating pages on demand rather than pre-allocating a maximum-length buffer per request.

Integration into the framework:
- A KV-cache allocator maintains a pool of fixed-size page buffers in VRAM
- The scheduler knows which pages belong to which request (a block table per request)
- The attention kernel template takes the block table as an input and handles non-contiguous KV-cache reads internally — this is the FlashDecoding variant with paged memory access

**FlashDecoding for Long-Context Decode**

Standard FlashAttention is optimized for the **prefill** phase (processing the full prompt, compute-bound). During **decode** (generating one token at a time with a long KV-cache), the kernel is memory-bandwidth-bound and needs a different tile scheduling strategy:
- FlashDecoding splits the KV sequence across thread blocks, with each block handling a sub-range of the KV dimension
- Partial softmax results are computed in parallel and then reduced across blocks
- On H100 with a 32K-token KV-cache: FlashDecoding is ~5× faster than naive FlashAttention for decode

**CUDA Graphs with a Shape-Keyed Pool**

</details>

推理服务的批大小是可变的（1 到 max_batch）。与其只用一个 graph，不如维护一个以批大小为键的预编译 graph 池：
```
graph_pool = {1: cuda_graph_1, 4: cuda_graph_4, 8: cuda_graph_8, ...}
```
对于动态批大小，向上取整到最近的已编译尺寸，并用 dummy token 填充（对 padding 走一次带 mask 的 attention pass）。vLLM 的 CUDA Graph 模式和 TRT-LLM 采用的就是这一策略。框架应自动管理这个池。

**decode（逐 token 生成阶段）的 L2 KV-Cache 持久化**

在 decode 中，KV-cache 每一步都会被读取，但极少写入（只有新 token 追加）。用 `cudaAccessPropertyPersisting` 标记 KV-cache 缓冲区，让最近访问过的 KV 页在多次 kernel 启动之间留在 H100 的 50 MB L2 缓存中。对于短到中等长度的上下文（最多约 2048 个 token），这可以完全消除重复的全局内存取数。

**面向权重受限层的 2:4 结构化稀疏**

在小批大小 decode 中，GEMM 受权重带宽限制（而非算力受限），2:4 稀疏可将权重传输量减半、有效带宽翻倍，带来实打实的吞吐收益。框架把离线剪枝 + cuSPARSELt 压缩作为模型导出步骤来处理。

---

**在 NVIDIA 上做训练：聚焦的制胜条件**

训练是次要的，但必须认真对待。目标是 **Transformer 训练**（大语言模型、视觉 Transformer）——不是卷积神经网络，也不是任意模型。结构性优势正是在这里成立：


<details>
<summary>English original</summary>

Inference serving has variable batch sizes (1 to max_batch). Rather than one graph, maintain a pool of pre-compiled graphs keyed by batch size:
```
graph_pool = {1: cuda_graph_1, 4: cuda_graph_4, 8: cuda_graph_8, ...}
```
For dynamic batch sizes, round up to the nearest compiled size and pad with dummy tokens (a masked attention pass on padding). This is the strategy used in vLLM's CUDA Graph mode and TRT-LLM. The framework should manage this pool automatically.

**L2 KV-Cache Persistence for Decode**

In decode, the KV-cache is read every step and rarely written (only the new token appends). Mark KV-cache buffers with `cudaAccessPropertyPersisting` so the most recently accessed KV pages stay in the H100's 50 MB L2 cache across kernel launches. For short-to-medium contexts (up to ~2048 tokens), this can eliminate repeated global memory fetches entirely.

**2:4 Structured Sparsity for Weight-Bound Layers**

For small batch decode where the GEMM is weight-bandwidth-bound (not compute-bound), 2:4 sparsity halves the weight transfer volume and doubles effective bandwidth, providing a real throughput gain. The framework handles the offline pruning + cuSPARSELt compression as a model export step.

---

**Training on NVIDIA: The Focused Win Condition**

Training is secondary but serious. The target is **transformer training** (LLMs, vision transformers) — not CNNs, not arbitrary models. This is where the structural advantage holds:

</details>

* **FlashAttention-3 前向 + 反向**：attention layer 是长序列训练的吞吐瓶颈。FA-3 在 H100 上可达到理论 FLOP/s 的约 75%。当框架在 UOp graph 中检测到 attention pattern 时，便替换为 FA-3。
* **BF16 混合精度**：前向/反向 pass 使用 BF16 Tensor Core GEMM，优化器使用 FP32 master weights。调度器管理 cast op，并确保它们与周围的 elementwise op 融合。
* **FP8 训练（实验性，B100/B200 上的 Blackwell 原生支持）**：FP8 前向 + FP8 反向，配合 BF16 master weights 与梯度缩放。NVIDIA Transformer Engine 目前已能做到；本框架应提供同等能力。
* **梯度检查点集成**：对于极长序列，在反向时重新计算激活值而非存储它们。调度器标记被检查点的区域，并在反向时重新调度前向子图。
* **NCCL 全规约 hook**：每次反向 pass 之后、优化器 step 之前，跨 GPU 全规约梯度。它作为 UOp graph 中被调度的 barrier 集成，而非临时的同步点。

**训练明确不做的事**：ResNet 风格的 CNN 训练、任意的 op graph、含复杂动态控制流的模型。cuDNN conv 的护城河是真实存在的，不值得去硬拼。Transformer 技术栈才是市场。

---

**框架 vs. 生态：如何定位**

| 层级 | 工具 | 本框架的角色 |
|---|---|---|
| Kernel 语言 | Triton, CUDA C++ | 消费 kernel 模板；不做替代 |
| 推理引擎 | TensorRT, TRT-LLM | 针对 Transformer 工作负载做替代 —— 保留可 hack 性 |
| 大语言模型推理服务 | vLLM, SGLang | 提供执行后端（替代 vLLM 中的 PyTorch eager） |
| 训练框架 | PyTorch + torch.compile | 在 Transformer 训练上竞争；在 CNN 训练上让步 |
| 量化 | NVIDIA Modelopt, AutoGPTQ | 提供 FP8/INT8/2:4 量化导出流水线 |

**诚实的性能上限（推理优先）**


<details>
<summary>English original</summary>

* **FlashAttention-3 forward + backward**: The attention layer is the throughput bottleneck for long-sequence training. FA-3 on H100 achieves ~75% of theoretical FLOP/s. The framework substitutes FA-3 when it detects the attention pattern in the UOp graph.
* **BF16 mixed precision**: BF16 Tensor Core GEMMs for forward/backward passes, FP32 master weights for the optimizer. The scheduler manages the cast ops and ensures they fuse with surrounding elementwise ops.
* **FP8 training (experimental, Blackwell-native on B100/B200)**: FP8 forward + FP8 backward with BF16 master weights and gradient scaling. NVIDIA Transformer Engine does this today; the framework should provide the same capability.
* **Gradient checkpointing integration**: For very long sequences, re-compute activations during backward instead of storing them. The scheduler marks checkpointed regions and re-schedules the forward subgraph during backward.
* **NCCL all-reduce hooks**: After each backward pass, all-reduce gradients across GPUs before the optimizer step. Integrated as a scheduled barrier in the UOp graph rather than an ad-hoc synchronization point.

**What training explicitly does NOT target**: ResNet-style CNN training, arbitrary op graphs, models with complex dynamic control flow. The cuDNN conv moat is real and not worth fighting. The transformer stack is the market.

---

**Framework vs. Ecosystem: Where to Position**

| Layer | Tool | This Framework's Role |
|---|---|---|
| Kernel language | Triton, CUDA C++ | Consumes kernel templates; does not replace |
| Inference engine | TensorRT, TRT-LLM | Replaces for transformer workloads — keeps hackability |
| LLM serving | vLLM, SGLang | Provides the execution backend (replaces PyTorch eager in vLLM) |
| Training framework | PyTorch + torch.compile | Competes on transformer training; yields on CNN training |
| Quantization | NVIDIA Modelopt, AutoGPTQ | Provides the FP8/INT8/2:4 quantization export pipeline |

**Honest Performance Ceiling (Inference-First)**

</details>

| 工作负载 | 可达性能 vs. 同类最佳 |
|---|---|
| LLM prefill（首字前的整段计算）（H100、BF16、长 prompt） | 与 FA-3 吞吐差距在 5% 以内 —— FA-3 模板定下上限 |
| LLM decode（逐 token 生成阶段）（H100、FP8、大批） | **可超越 vLLM** —— CUDA Graph + L2 persistence + paged FA-3 decode，配合 FP8 权重 |
| LLM decode（H100、FP8、小批、长 KV） | **可超越 TRT-LLM** —— 更好的跨层融合、FP8 配合 dequant 融合 |
| 卷积神经网络推理（任意模型） | 无法超越 TensorRT —— 不要在此竞争 |
| Transformer 训练（H100、BF16、长 seq） | 与优化后的 PyTorch+FA-3 差距在 10% 以内 —— 有竞争力 |
| 卷积神经网络训练 | 不要进入该市场 |

---


<details>
<summary>English original</summary>

| Workload | Achievable vs. Best-in-Class |
|---|---|
| LLM prefill (H100, BF16, long prompt) | Within 5% of FA-3 throughput — FA-3 template sets the ceiling |
| LLM decode (H100, FP8, large batch) | **Can beat vLLM** — CUDA Graph + L2 persistence + paged FA-3 decode with FP8 weights |
| LLM decode (H100, FP8, small batch, long KV) | **Can beat TRT-LLM** — better cross-layer fusion, FP8 with dequant fusion |
| CNN inference (any model) | Cannot beat TensorRT — do not compete here |
| Transformer training (H100, BF16, long seq) | Within 10% of optimized PyTorch+FA-3 — competitive |
| CNN training | Do not enter this market |

---

</details>

### 4.4 资源

* **CUTLASS 3.x（NVIDIA GitHub）：** 面向 Hopper 上 WGMMA、TMA、warp specialization 与 persistent kernel 的生产级 C++ 模板。是 CUDA 层面 NVIDIA 最优 GEMM 长什么样的权威参考。
* **FlashAttention-3 论文与博客（Tri Dao，2024）：** 详细拆解 TMA + WGMMA + warp specialization 如何使 attention 达到 H100 理论吞吐的 75%。
* **Tawa: Automatic Warp Specialization（arXiv 2510.14719）：** 在 Triton IR 之上加入自动 warp specialization，最高达到 FlashAttention-3 吞吐的 96%，相较未使用 warp specialization 的 Triton 提升 1.21×。清楚表明了 Triton 的 SPMT 模型的短板所在。
* **NVIDIA Hopper 架构深入解析（developer.nvidia.com）：** 关于 TMA、WGMMA、warp specialization 与 Hopper 编程模型的官方深度解析。
* **CUDA 编程指南：L2 缓存控制：** 如何使用 `cudaAccessPropertyPersisting` 进行 KV-cache 优化。
* **tinygrad UOp IR 与调度器：** `tinygrad/codegen/`、`tinygrad/engine/schedule.py`——可供 fork 并扩展的组件。
* **NCCL 源代码：** 生产级多 GPU 集合通信如何处理 NVLink 拓扑检测与算法选择。
* **"Can tinygrad win?" — geohot 的博客（2025 年 7 月）：** 对 tinygrad 所处位置以及竞争路径长什么样的坦诚内部评估。

---


<details>
<summary>English original</summary>

**4.4 Resources**

* **CUTLASS 3.x (NVIDIA GitHub):** Production C++ templates for WGMMA, TMA, warp specialization, and persistent kernels on Hopper. The authoritative reference for what NVIDIA-optimal GEMM looks like at the CUDA level.
* **FlashAttention-3 paper and blog (Tri Dao, 2024):** Detailed walkthrough of how TMA + WGMMA + warp specialization achieves 75% of H100 theoretical throughput for attention.
* **Tawa: Automatic Warp Specialization (arXiv 2510.14719):** Adds automatic warp specialization on top of Triton IR, achieving up to 96% of FlashAttention-3 throughput and 1.21× over unspecialized Triton. Shows exactly where Triton's SPMT model falls short.
* **NVIDIA Hopper Architecture In-Depth (developer.nvidia.com):** Official deep-dive on TMA, WGMMA, warp specialization, and the Hopper programming model.
* **CUDA Programming Guide: L2 Cache Control:** How to use `cudaAccessPropertyPersisting` for KV-cache optimization.
* **tinygrad UOp IR and Scheduler:** `tinygrad/codegen/`, `tinygrad/engine/schedule.py` — the components to fork and extend.
* **NCCL Source Code:** How production multi-GPU collective communication handles NVLink topology detection and algorithm selection.
* **"Can tinygrad win?" — geohot's blog (July 2025):** Honest internal assessment of where tinygrad is and what the competitive path looks like.

---

</details>

### 4.5 项目

项目按优先级排序：推理优先，训练其次。

**推理项目**

* **Decode 吞吐 Benchmark —— 建立基线：** 在单张 H100 上，用 PyTorch eager、vLLM 和 TensorRT-LLM 运行 LLaMA-2-7B 或类似模型。用 Nsight Systems 做 profile：测量 time-per-token、GPU 利用率、内存带宽利用率，以及 kernel 启动之间的 CPU 调度开销。这精确确立了需要击败的目标，以及差距在哪里。为每个系统写一份带 roofline 分析的报告。

* **面向可变批推理的 CUDA Graph 池：** 用 Python 实现一个按 shape 索引的 CUDA Graph 池（或 fork tinygrad 的 JIT）。为批大小 [1, 2, 4, 8, 16, 32] 预编译 graph。每次推理调用时，选择最接近的已编译批大小并做 padding。在同一个 Transformer decode 步骤上，测量相对 eager tinygrad 和相对 eager PyTorch 的 CPU 开销降低量。

* **FlashAttention 模式分发：** fork tinygrad。在调度器中加入一个模式识别器，检测 UOp graph 中的 `softmax(Q @ K.T / sqrt(d)) @ V`。替换为预写好的 FlashAttention-2 CUDA kernel（使用 Tri Dao 仓库中的参考实现）。对照朴素的 tinygrad 结果验证数值正确性。对序列长度 512、2048、8192 测量 prefill 吞吐（tokens/sec）。

* **FP8 量化 GEMM —— 反量化融合：** 为 FP8 E4M3 矩阵乘编写一个 CUDA kernel，把输出反量化和 ReLU/SiLU 激活融合进同一个 kernel（无中间 FP32 buffer）。用 Nsight Compute 的内存吞吐计数器，对比其与未融合版本的内存流量。在真实的 FFN 层尺寸上测量（例如 4096×4096 的权重矩阵）。

* **L2 KV-Cache 驻留实验：** 用 CUDA 实现自回归 Transformer decode。变体 A：默认内存策略。变体 B：KV-cache buffer 标记为 `cudaAccessPropertyPersisting`。对上下文长度 512、2048、4096、8192 测量 tokens/sec 和 L2 命中率（Nsight Compute）。确定 KV-cache 超过 L2 容量、该优化不再起作用的交叉点。以图表形式报告。

* **PagedAttention 内存管理器：** 用 C++/CUDA 实现一个 KV-cache 页分配器：固定大小的页（例如 16 tokens × head_dim × num_heads）、一个空闲链表分配器，以及每个请求一个 block table。编写一个接受 block table、能处理非连续 KV 读取的 attention kernel。用一个包含 8 个请求、KV-cache 长度随机变化的批进行测试。

**训练项目**

* **Transformer 训练 Benchmark —— tinygrad 输在哪里：** 用 tinygrad 和 PyTorch 各跑 100 步 GPT-2 small 训练，PyTorch 侧用 `torch.compile`。用 Nsight Systems 做 profile。具体找出哪些 op 造成了性能差距：是 attention kernel？GEMM？elementwise 链？还是优化器步骤？这项诊断工作是前置要求，据此才能知道先构建哪些 kernel 模板。

* **Tensor Core Lowering Pass：** fork tinygrad。在调度器中加入一个模式匹配器，检测 UOp graph 中矩阵乘形状的 reduce-of-multiply，并为其标注 `TENSOR_CORE`。编写一个 PTX emitter，对该模式生成 `mma.sync.aligned.m16n8k16.row.col.bf16.bf16.f32.f32` 而非通用循环。在 4096×4096 BF16 GEMM 上对生成的 kernel 与 tinygrad 通用输出做 benchmark。用 Nsight Compute 验证 Tensor Core 利用率（应读到约 90%+，而不是 0%）。

* **FlashAttention-3 训练集成（进阶）：** 把 FlashAttention-3 的前向与反向 CUDA kernel 作为模式分发模板集成进框架。反向 kernel 是更难的部分——它需要前向传播中保存的 log-sum-exp。用有限差分检查验证梯度正确性。在 4096 序列长度的 LLaMA attention 层上，对比 PyTorch SDPA 测量端到端训练吞吐。

* **框架架构设计文档：** 写一份 6 页的技术设计文档。第 1 节：推理服务架构（graph 池、分页 KV-cache、FP8 pipeline）。第 2 节：训练架构（梯度图扩展、BF16 混合精度、NCCL hooks）。第 3 节：UOp 扩展分类法（新增哪些 UOp 类型和标注，及其 lowering 规则）。第 4 节：构建优先级——根据上述各项目的 benchmark 发现，决定先构建哪些组件。这份文档是真正构建该框架的路线图。


<details>
<summary>English original</summary>

**4.5 Projects**

Projects are ordered by priority: inference first, training second.

**Inference Projects**

* **Decode Throughput Benchmark — Establish the Baseline:** Run LLaMA-2-7B or similar on PyTorch eager, vLLM, and TensorRT-LLM on a single H100. Profile with Nsight Systems: measure time-per-token, GPU utilization, memory bandwidth utilization, and CPU dispatch overhead between kernel launches. This establishes exactly what you need to beat and where the gaps are. Write a report with roofline analysis for each system.

* **CUDA Graph Pool for Variable-Batch Inference:** Implement a shape-keyed CUDA Graph pool in Python (or fork tinygrad's JIT). Pre-compile graphs for batch sizes [1, 2, 4, 8, 16, 32]. For each inference call, select the nearest compiled batch size and pad. Measure the CPU overhead reduction vs. eager tinygrad and vs. eager PyTorch on the same transformer decode step.

* **FlashAttention Pattern Dispatch:** Fork tinygrad. Add a pattern recognizer to the scheduler that detects `softmax(Q @ K.T / sqrt(d)) @ V` in the UOp graph. Substitute a pre-written FlashAttention-2 CUDA kernel (use the reference implementation from Tri Dao's repo). Verify numerical correctness against the naive tinygrad result. Benchmark prefill throughput (tokens/sec) for sequence lengths 512, 2048, 8192.

* **FP8 Quantized GEMM — Dequant Fusion:** Write a CUDA kernel for FP8 E4M3 matrix multiply that fuses output dequantization and a ReLU/SiLU activation into the same kernel (no intermediate FP32 buffer). Compare memory traffic vs. an unfused version using Nsight Compute's memory throughput counters. Measure on a realistic FFN layer size (e.g., 4096×4096 weight matrix).

* **L2 KV-Cache Persistence Experiment:** Implement autoregressive transformer decode in CUDA. Variant A: default memory policy. Variant B: KV-cache buffers marked with `cudaAccessPropertyPersisting`. Measure tokens/sec and L2 hit rate (Nsight Compute) for context lengths 512, 2048, 4096, 8192. Determine the crossover point where the KV-cache exceeds L2 capacity and the optimization stops helping. Report as a graph.

* **PagedAttention Memory Manager:** Implement a KV-cache page allocator in C++/CUDA: fixed-size pages (e.g., 16 tokens × head_dim × num_heads), a free-list allocator, and a block table per request. Write an attention kernel that accepts a block table and handles non-contiguous KV reads. Test with a batch of 8 requests with randomly varying KV-cache lengths.

**Training Projects**

* **Transformer Training Benchmark — Where tinygrad Loses:** Run GPT-2 small training for 100 steps using both tinygrad and PyTorch with `torch.compile`. Profile with Nsight Systems. Identify specifically which ops account for the performance gap: is it the attention kernel? The GEMM? The elementwise chain? The optimizer step? This diagnostic work is the prerequisite for knowing what kernel templates to build first.

* **Tensor Core Lowering Pass:** Fork tinygrad. Add a pattern matcher in the scheduler that detects a matmul-shaped reduce-of-multiply in the UOp graph and annotates it with `TENSOR_CORE`. Write a PTX emitter that for this pattern emits `mma.sync.aligned.m16n8k16.row.col.bf16.bf16.f32.f32` instead of the generic loop. Benchmark the generated kernel vs. the generic tinygrad output on a 4096×4096 BF16 GEMM. Use Nsight Compute to verify tensor core utilization (should read ~90%+, not 0%).

* **FlashAttention-3 Training Integration (Advanced):** Integrate FlashAttention-3's forward and backward CUDA kernels into the framework as a pattern-dispatched template. The backward kernel is the harder part — it requires the log-sum-exp saved from the forward pass. Verify gradient correctness with a finite-difference check. Measure end-to-end training throughput on a 4096-sequence LLaMA attention layer vs. PyTorch SDPA.

* **Framework Architecture Design Document:** Write a 6-page technical design document. Section 1: the inference serving architecture (graph pool, paged KV-cache, FP8 pipeline). Section 2: the training architecture (gradient graph extension, BF16 mixed precision, NCCL hooks). Section 3: the UOp extension taxonomy (which new UOp types and annotations, and their lowering rules). Section 4: build prioritization — which components to build first based on the benchmark findings from the projects above. This document is the roadmap for actually building the framework.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track F - AI Chip Design/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20F%20-%20AI%20Chip%20Design/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
