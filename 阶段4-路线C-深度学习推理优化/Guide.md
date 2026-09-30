---
title: 阶段 4 — 方向 C: DL 推理优化 (6–12 个月)
description: 阶段 4 — 方向 C: DL 推理优化 (6–12 个月)
---

# 阶段 4 — 方向 C: DL 推理优化 (6–12 个月)

<div class="course-identity dl-inference" markdown="1">
<div class="course-identity__icon">INF</div>
<div markdown="1">
<p class="course-identity__eyebrow">方向 C · DL 推理优化</p>
<p class="course-identity__title">将模型图 lower 为优化后的 kernel，然后针对真实推理目标进行性能剖析、融合、量化和部署神经网络。</p>
<p class="course-identity__meta">产物：优化后的推理流水线 · 衡量指标：延迟、吞吐、内存、准确率</p>
</div>
</div>

> *AI 模型与硬件之间的桥梁——了解编译器如何将神经网络图 lower 为高效、硬件特定的代码，然后应用这些知识来构建和优化真实的推理流水线。*

**前置要求：** 阶段 1 §4 (C++ 与并行计算)，阶段 3 (神经网络)。推荐：阶段 1 §3 (操作系统——内存、进程)。

**Layer 映射：** 主要是 AI 芯片栈的 **Layer 2** (编译器与图优化)，并与 Layer 1 (框架图) 和 Layer 3 (执行编译后产物的 runtime) 相连。

**目标职位：** DL 推理优化工程师 · **MTS Kernels** (Member of Technical Staff, Kernels) · AI 编译器工程师 · DL 图优化工程师 · ML 编译器后端工程师

**也符合：** AGI/大语言模型公司中专注于 kernel 的职位——设计和实现用于训练和推理的高性能 kernel、长上下文优化，以及在 NVIDIA GPU 和替代加速器 (TPU 等) 上的生产部署。

---

## 为什么选择这个方向

方向 A (FPGA) 和 B (Jetson) 教你针对特定硬件进行部署。本方向教你 *模型如何变成硬件指令* ——横亘于 PyTorch/ONNX 图和任何加速器上运行的 kernel 代码之间的编译器栈——然后如何让这些代码足够快以交付。无论你的目标是 GPU、FPGA 还是自定义 NPU，你都需要 IR 设计、图优化、调度和代码生成，接下来是 kernel 编写、量化和 runtime 工作，将编译后的图变成可服务的模型。这也是 AI 基础设施中增长最快的招聘领域。

---

## 方向结构

本方向有两个部分。**第 1 部分**涵盖编译器基础 (IR、图优化、LLVM、MLIR、编译流水线、融合、自定义后端)。**第 2 部分**将这些概念应用于真实的 DL 推理工作负载 (性能剖析、kernel 工程、量化、runtime、tinygrad 深入探索)。两者结合带你从理论走向生产。

| 部分 | 重点 | 章节 |
|------|-------|----------|
| **第 1 部分 — 编译器基础** | 编译器如何工作，从 IR 到硬件代码 | 下面的 §1–§7 |
| **第 2 部分 — DL 推理优化** | 将编译器 + kernel 技能应用于真实推理 | [下面的第 2 部分](#part-2--dl-inference-optimization) (6 个单元) |

**推荐顺序：** 首先学习第 1 部分 (或至少 §1–§2 和 §5)，然后按顺序学习第 2 部分。第 2 部分的单元 01–03 通过 GPU 特定的实践强化和深化第 1 部分的概念；单元 04–06 增加量化、部署和动手实践 tinygrad。

---

## 第 1 部分 — 编译器基础

### 1. 图表示与中间表示 (IR)

* **计算图：**
    * PyTorch、TensorFlow 和 ONNX 如何将模型表示为有向无环图 (DAG)。
    * 追踪 vs 脚本 vs 导出：`torch.export`、`torch.compile`、ONNX 导出。
    * 图级元数据：形状、dtype、内存布局 (NCHW vs NHWC)。

* **中间表示：**
    * **图 IR vs 线性化 IR** — 图：节点 = 算子，边 = 张量。线性化：按执行顺序排列的算子列表 (tinygrad 的方法)。
    * **静态单赋值 (SSA) 形式** — 每个值只定义一次；可实现清晰的别名和内存分析。
    * **ONNX 作为交换 IR** — 算子集、形状推断、版本转换器。

* **tinygrad IR 研究：**
    * 追踪一个模型从 `Tensor` 算子到 `LazyBuffer` 到线性化算子再到生成的代码。
    * 理解 tinygrad 的 IR 与基于图的 IR (TorchFX、ONNX) 有何不同。

**项目：**
* 将卷积神经网络 (ResNet-18) 导出为 ONNX。用 Netron 可视化图。识别冗余算子。
* 在 tinygrad 中追踪一个矩阵乘：`Tensor` → `LazyBuffer` → 调度后的算子 → 生成的 CUDA kernel。记录每个 IR 边界。


<details>
<summary>English original</summary>

**Phase 4 — Track C: DL Inference Optimization (6–12 months)**

<div class="course-identity dl-inference" markdown="1">
<div class="course-identity__icon">INF</div>
<div markdown="1">
<p class="course-identity__eyebrow">Track C · DL Inference Optimization</p>
<p class="course-identity__title">Lower model graphs into optimized kernels, then profile, fuse, quantize, and deploy neural networks for real inference targets.</p>
<p class="course-identity__meta">Artifact: optimized inference pipeline · Measure: latency, throughput, memory, accuracy</p>
</div>
</div>


> *The bridge between AI models and hardware — learn how compilers lower neural-network graphs to efficient, hardware-specific code, then apply that knowledge to build and optimize real inference pipelines.*

**Prerequisites:** Phase 1 §4 (C++ and Parallel Computing), Phase 3 (Neural Networks). Recommended: Phase 1 §3 (Operating Systems — memory, processes).

**Layer mapping:** Primarily **Layer 2** (Compiler & Graph Optimization) of the AI chip stack, with connections into Layer 1 (framework graphs) and Layer 3 (runtime that executes compiled artifacts).

**Role targets:** DL Inference Optimization Engineer · **MTS Kernels** (Member of Technical Staff, Kernels) · AI Compiler Engineer · DL Graph Optimization Engineer · ML Compiler Backend Engineer

**Also aligns with:** kernel-focused roles at AGI/LLM companies — designing and implementing high-performance kernels for training and inference, long-context optimization, and production deployment on NVIDIA GPUs and alternative accelerators (TPU, etc.).

---

**Why this track**

Tracks A (FPGA) and B (Jetson) teach you to deploy on specific hardware. This track teaches you *how models become hardware instructions* — the compiler stack that sits between a PyTorch/ONNX graph and the kernel code running on any accelerator — and then how to make that code fast enough to ship. Whether you target GPU, FPGA, or a custom NPU, you need IR design, graph optimization, scheduling, and code generation, followed by the kernel authoring, quantization, and runtime work that turns a compiled graph into a served model. This is also the fastest-growing hiring area in AI infrastructure.

---

**Track structure**

This track has two parts. **Part 1** covers compiler fundamentals (IR, graph optimization, LLVM, MLIR, compilation pipelines, fusion, custom backends). **Part 2** applies these concepts to real DL inference workloads (profiling, kernel engineering, quantization, runtimes, tinygrad deep dive). Together they take you from theory to production.

| Part | Focus | Sections |
|------|-------|----------|
| **Part 1 — Compiler Fundamentals** | How compilers work, from IR to hardware code | §1–§7 below |
| **Part 2 — DL Inference Optimization** | Applying compiler + kernel skills to real inference | [Part 2 below](#part-2--dl-inference-optimization) (6 units) |

**Recommended order:** Work through Part 1 first (or at least §1–§2 and §5), then Part 2 in order. Part 2 units 01–03 reinforce and deepen Part 1 concepts with GPU-specific practice; units 04–06 add quantization, deployment, and hands-on tinygrad.

---

**Part 1 — Compiler Fundamentals**

**1. Graph Representation & Intermediate Representation (IR)**

* **Computational graphs:**
    * How PyTorch, TensorFlow, and ONNX represent models as directed acyclic graphs (DAGs).
    * Tracing vs scripting vs export: `torch.export`, `torch.compile`, ONNX export.
    * Graph-level metadata: shapes, dtypes, memory layout (NCHW vs NHWC).

* **Intermediate representations:**
    * **Graph IR vs linearized IR** — Graph: nodes = ops, edges = tensors. Linearized: list of ops in execution order (tinygrad's approach).
    * **SSA (Single Static Assignment) form** — Each value defined once; enables clean alias and memory analysis.
    * **ONNX as interchange IR** — Opsets, shape inference, version converters.

* **tinygrad IR study:**
    * Trace a model from `Tensor` ops to `LazyBuffer` to linearized ops to generated code.
    * Understand how tinygrad's IR differs from graph-based IRs (TorchFX, ONNX).

**Projects:**
* Export a CNN (ResNet-18) to ONNX. Visualize the graph with Netron. Identify redundant ops.
* Trace a matmul through tinygrad: `Tensor` → `LazyBuffer` → scheduled ops → generated CUDA kernel. Document every IR boundary.

---

</details>

### 2. 图优化 pass

* **代数简化：**
    * 常量折叠、死代码消除、公共子表达式消除（CSE）。
    * 强度削减（例如，将 `x / 2` 替换为 `x * 0.5`，将 `pow(x, 2)` 替换为 `x * x`）。

* **算子融合：**
    * 融合为何重要：减少内存流量和 kernel 启动开销。
    * **垂直融合：** Conv → BatchNorm → ReLU 合并为单个 kernel。
    * **水平融合：** 独立算子在一个 kernel 中运行以打满计算。
    * 实践中的融合：TensorRT 的 layer 融合、tinygrad 基于 BEAM 的融合、XLA 的融合启发式。

* **布局与内存优化：**
    * 数据布局变换（NCHW ↔ NHWC），以匹配硬件偏好。
    * 通过别名分析检测原地操作。
    * 内存规划：算子级活跃性分析 → buffer 复用 → 降低峰值内存。

* **量化作为图 pass：**
    * 插入量化/反量化节点（PTQ）。
    * 在量化前将 BN 折叠进 Conv 权重。
    * 校准：收集激活值范围以设置 scale/zero-point。

**项目：**
* 使用 `onnx` + `onnxruntime` Python API 在 ONNX 图上实现 Conv+BN+ReLU 融合 pass。测量 kernel 数量减少。
* 编写内存规划 pass：给定算子调度，使用活跃区间计算最小 buffer 分配。

---

### 3. AI 编译器的 LLVM 基础

* **三阶段编译器设计：**
    * 前端 → 优化器 → 后端。这种模块化对 AI 目标为何重要。
    * LLVM IR：类型、指令、静态单赋值、基本块、函数。

* **深入 LLVM IR：**
    * 地址空间（对 GPU/加速器内存模型很重要）。
    * 内建函数：硬件相关操作如何暴露在 IR 中。
    * 元数据与调试信息。

* **优化 pass：**
    * 规范化、循环展开、向量化、死存储消除。
    * 编写自定义 LLVM pass（C++）：注册 pass、遍历 IR、变换。

* **后端与代码生成：**
    * 通过 SelectionDAG / GlobalISel 进行指令选择。
    * TableGen：以声明方式描述目标指令。
    * 寄存器分配、指令调度。
    * GPU 目标（NVPTX、AMDGPU）如何使用 LLVM。

**资源：**
* [LLVM Language Reference](https://llvm.org/docs/LangRef.html)
* [LLVM Programmer's Manual](https://llvm.org/docs/ProgrammersManual.html)
* [Writing an LLVM Pass](https://llvm.org/docs/WritingAnLLVMPass.html)

**项目：**
* 编写一个小函数并将其编译为 LLVM IR（`clang -emit-llvm`）。阅读该 IR，识别静态单赋值形式的值，追踪经过 `opt` 个 pass。
* 编写一个自定义 LLVM pass，统计函数中的浮点乘累加（FMA）操作数——作为 FLOPS 估算的代理。

---

### 4. MLIR：多级中间表示

* **为什么需要 MLIR：**
    * LLVM 只有一个 IR 层级；AI 编译器需要许多层级。MLIR 提供了在每个抽象层级构建 *方言* 的框架。
    * 渐进式 lowering：高层张量算子 → 循环嵌套 → 向量指令 → 硬件代码。

* **MLIR 核心概念：**
    * **方言：** `tensor`、`linalg`、`memref`、`affine`、`vector`、`scf`、`gpu`、`llvm`。
    * **操作、区域、块** — MLIR 数据模型。
    * **类型与属性** — 可扩展的类型系统。

* **AI 的关键方言：**
    * **`linalg`** — 张量上的命名算子（conv、矩阵乘、pooling）和通用算子。分块、融合、提升。
    * **`affine`** — 多面体循环分析。依赖分析、循环交换、分块。
    * **`tensor` / `memref`** — Bufferization：将值语义张量转换为内存中的 buffer。
    * **`vector`** — 目标无关的 SIMD 表示。
    * **`gpu`** — GPU kernel 启动、线程/block 映射。

* **渐进式 lowering 走查：**
    * `tosa`（来自 ONNX/TF）→ `linalg` → `affine`/`scf` → `vector` → `gpu` 或 `llvm`。
    * Bufferization pass：张量何时以及如何变成 memref。

* **自定义方言开发：**
    * 使用 ODS（Operation Definition Specification）定义自定义 NPU 方言。
    * 编写从 `linalg` 到自定义方言的 lowering pass。

**资源：**
* [MLIR Documentation](https://mlir.llvm.org/)
* [MLIR Tutorial: Creating a Dialect](https://mlir.llvm.org/docs/Tutorials/CreatingADialect/)
* [Toy Tutorial](https://mlir.llvm.org/docs/Tutorials/Toy/) — 在 MLIR 中端到端实现自定义语言。

**项目：**
* 完成 MLIR Toy tutorial（第 1–7 章）。理解如何定义算子、编写 lowering pass，以及生成 LLVM IR。
* 编写一个最小 NPU 方言，其中只有一个 `npu.matmul` op。使用分块策略将 `linalg.matmul` lower 到 `npu.matmul`。

---


<details>
<summary>English original</summary>

**2. Graph Optimization Passes**

* **Algebraic simplifications:**
    * Constant folding, dead code elimination, common sub-expression elimination (CSE).
    * Strength reduction (e.g., replace `x / 2` with `x * 0.5`, replace `pow(x, 2)` with `x * x`).

* **Operator fusion:**
    * Why fusion matters: reduces memory traffic and kernel launch overhead.
    * **Vertical fusion:** Conv → BatchNorm → ReLU merged into a single kernel.
    * **Horizontal fusion:** Independent ops run in one kernel to saturate compute.
    * Fusion in practice: TensorRT's layer fusion, tinygrad's BEAM-based fusion, XLA's fusion heuristics.

* **Layout and memory optimizations:**
    * Data layout transformations (NCHW ↔ NHWC) to match hardware preference.
    * In-place operation detection via alias analysis.
    * Memory planning: operator-level liveness analysis → buffer reuse → reduced peak memory.

* **Quantization as a graph pass:**
    * Inserting quantize/dequantize nodes (PTQ).
    * Folding BN into Conv weights before quantization.
    * Calibration: collecting activation ranges to set scale/zero-point.

**Projects:**
* Implement a Conv+BN+ReLU fusion pass on an ONNX graph using `onnx` + `onnxruntime` Python APIs. Measure kernel count reduction.
* Write a memory planning pass: given an op schedule, compute minimum buffer allocation using liveness intervals.

---

**3. LLVM Fundamentals for AI Compilers**

* **Three-phase compiler design:**
    * Frontend → Optimizer → Backend. Why this modularity matters for AI targets.
    * LLVM IR: types, instructions, SSA, basic blocks, functions.

* **LLVM IR in depth:**
    * Address spaces (important for GPU/accelerator memory models).
    * Intrinsics: how hardware-specific operations are exposed in IR.
    * Metadata and debug info.

* **Optimization passes:**
    * Canonicalization, loop unrolling, vectorization, dead store elimination.
    * Writing a custom LLVM pass (C++): register a pass, walk the IR, transform.

* **Backend and code generation:**
    * Instruction selection via SelectionDAG / GlobalISel.
    * TableGen: declaratively describing target instructions.
    * Register allocation, instruction scheduling.
    * How GPU targets (NVPTX, AMDGPU) use LLVM.

**Resources:**
* [LLVM Language Reference](https://llvm.org/docs/LangRef.html)
* [LLVM Programmer's Manual](https://llvm.org/docs/ProgrammersManual.html)
* [Writing an LLVM Pass](https://llvm.org/docs/WritingAnLLVMPass.html)

**Projects:**
* Write and compile a small function to LLVM IR (`clang -emit-llvm`). Read the IR, identify SSA values, trace through `opt` passes.
* Write a custom LLVM pass that counts floating-point multiply-accumulate (FMA) operations in a function — a proxy for FLOPS estimation.

---

**4. MLIR: Multi-Level Intermediate Representation**

* **Why MLIR:**
    * LLVM has one IR level; AI compilers need many. MLIR provides a framework for building *dialects* at every abstraction level.
    * Progressive lowering: high-level tensor ops → loop nests → vector instructions → hardware code.

* **Core MLIR concepts:**
    * **Dialects:** `tensor`, `linalg`, `memref`, `affine`, `vector`, `scf`, `gpu`, `llvm`.
    * **Operations, regions, blocks** — the MLIR data model.
    * **Types and attributes** — extensible type system.

* **Key dialects for AI:**
    * **`linalg`** — Named ops (conv, matmul, pooling) and generic ops on tensors. Tiling, fusion, promotion.
    * **`affine`** — Polyhedral loop analysis. Dependence analysis, loop interchange, tiling.
    * **`tensor` / `memref`** — Bufferization: converting value-semantics tensors to in-memory buffers.
    * **`vector`** — Target-independent SIMD representation.
    * **`gpu`** — GPU kernel launch, thread/block mapping.

* **Progressive lowering walkthrough:**
    * `tosa` (from ONNX/TF) → `linalg` → `affine`/`scf` → `vector` → `gpu` or `llvm`.
    * Bufferization pass: when and how tensors become memrefs.

* **Custom dialect development:**
    * Defining a custom NPU dialect with ODS (Operation Definition Specification).
    * Writing lowering passes from `linalg` to your custom dialect.

**Resources:**
* [MLIR Documentation](https://mlir.llvm.org/)
* [MLIR Tutorial: Creating a Dialect](https://mlir.llvm.org/docs/Tutorials/CreatingADialect/)
* [Toy Tutorial](https://mlir.llvm.org/docs/Tutorials/Toy/) — end-to-end custom language in MLIR.

**Projects:**
* Complete the MLIR Toy tutorial (chapters 1–7). Understand how to define ops, write lowering passes, and generate LLVM IR.
* Write a minimal NPU dialect with a single `npu.matmul` op. Lower `linalg.matmul` to `npu.matmul` with a tiling strategy.

---

</details>

### 5. ML 到硬件的编译流水线

* **TVM：**
    * **Relay**（图级 IR）→ **TIR**（Tensor IR，循环级）→ 目标代码。
    * 调度原语：`tile`、`vectorize`、`parallel`、`unroll`、`reorder`。
    * **AutoTVM / AutoScheduler (Ansor)：** 基于搜索的算子调度调优。
    * **BYOC（Bring Your Own Codegen）：** 将子图卸载到自定义加速器。

* **tinygrad 编译器：**
    * 调度器：算子如何分组为 kernel。
    * **BEAM search：** 探索融合选择以最小化 runtime。
    * 后端：CUDA、OpenCL、Metal、LLVM、自定义。
    * 向 tinygrad 添加新后端。

* **生产级编译器：**
    * **torch.compile + Inductor：** TorchFX → Triton kernel。`torch._inductor` 如何调度并生成代码。
    * **XLA（Accelerated Linear Algebra）：** 由 JAX 和 TF 使用。HLO IR → 目标代码（TPU、GPU、CPU）。
    * **IREE：** 基于 MLIR 的编译器，用于异构部署（CPU、GPU、DSP）。
    * **Triton-MLIR：** Python 级 kernel 编写，经 MLIR 流水线编译。

* **端到端流程对比：**

| 流水线 | 输入 | IR | 调度 | 目标 |
|----------|-------|-------|------------|--------|
| TVM | ONNX/Relay | Relay → TIR | AutoTVM / Ansor | CUDA、LLVM、自定义 |
| tinygrad | tinygrad 图 | 线性化算子 | BEAM search | CUDA、OpenCL、Metal、LLVM |
| torch.compile | TorchFX | FX → Inductor IR | Triton 代码生成 | CUDA (Triton)、CPU |
| XLA | HLO | HLO → LLO | XLA 调度器 | TPU、GPU、CPU |
| IREE | MLIR (tosa/linalg) | linalg → vector → spirv/llvm | 分块 + 分发 | CPU、GPU、DSP |

**项目：**
* 用 TVM 为你的 GPU 编译 ResNet-18。使用 AutoTVM 调优 3 个关键算子（conv2d、dense、batch_matmul）。对比调优前后的延迟。
* 研究 tinygrad BEAM：在小模型上使用 `BEAM=3` 运行。对比 kernel 数量与延迟，并与 `BEAM=0` 比较。
* 在 Transformer block 上使用 `torch.compile`。阅读为 attention 算子生成的 Triton kernel。标注分块策略。

---

### 6. kernel 融合与分块策略

* **面向存储层次的分块：**
    * 为何分块：使工作集适合 L1/L2/共享内存/SRAM scratchpad。
    * 分块大小选择：自动调优 vs 分析模型（roofline 引导，即性能上界模型）。
    * 多级分块：thread-block 分块 → warp 分块 → register 分块（GPU）；array 分块 → PE 分块（加速器）。

* **融合策略：**
    * **生产者-消费者融合：** 融合一个算子的输出是另一个算子的输入的情况（Conv → ReLU）。
    * **并行融合：** 融合共享输入的独立算子以减少读取。
    * **归约融合：** 融合逐元素算子与归约（softmax 组件）。
    * 融合合法性：当别名或数据依赖阻止融合时。

* **Dataflow 调度：**
    * 权重固定、输出固定、行固定、无局部复用。
    * 编译器如何将这些策略映射到硬件分块参数。
    * 与 Layer 5（硬件架构）的联系：编译器必须了解硬件的 dataflow。

**项目：**
* 用 C++ 实现 2 级分块矩阵乘（外层分块用于 L2，内层分块用于 L1）。与朴素实现做 benchmark，并与厂商 BLAS 比较。
* 在 tinygrad 或 TVM 中，修改一个融合启发式。度量对真实模型的影响（kernel 数量、总延迟、内存流量）。

---

### 7. 自定义后端开发

* **什么是自定义后端：**
    * 接收编译器 IR 并为你特定硬件（FPGA、NPU、自定义 ASIC）生成指令的代码生成器。

* **TVM BYOC 路径：**
    * 标注子图 → 划分 → 自定义代码生成函数 → 生成代码或 runtime 调用。
    * 为模拟加速器构建最小 BYOC 后端。

* **tinygrad 自定义后端：**
    * 为新目标实现 `Runtime` 和 `Compiler` 类。
    * 将线性化算子映射到硬件指令。

* **MLIR 自定义 lowering：**
    * 定义目标 dialect → 编写从 `linalg` 的 lowering pass → 生成目标特定代码。
    * 与 LLVM 后端或独立代码发射器集成。

* **与其他方向的联系：**
    * 方向 A（FPGA）：你的编译器后端生成 HLS 指令或 RTL 控制序列。
    * 方向 B（Jetson）：你的编译器后端面向 CUDA、DLA（深度学习加速器）或 TensorRT。

**项目：**
* 构建一个最小 TVM BYOC 后端，将 `nn.dense` 卸载到 Python 模拟的加速器。验证正确性。
* 为模拟的 4×4 脉动阵列添加一个简单 tinygrad 后端（计算矩阵乘分块，验证输出）。
* （高级）编写一个 MLIR lowering pass，从 `linalg.matmul` 到自定义 dialect，发射分块 DMA + 计算命令。

---


<details>
<summary>English original</summary>

**5. ML-to-Hardware Compilation Pipelines**

* **TVM:**
    * **Relay** (graph-level IR) → **TIR** (Tensor IR, loop-level) → target code.
    * Schedule primitives: `tile`, `vectorize`, `parallel`, `unroll`, `reorder`.
    * **AutoTVM / AutoScheduler (Ansor):** Search-based tuning for operator schedules.
    * **BYOC (Bring Your Own Codegen):** Offloading subgraphs to custom accelerators.

* **tinygrad compiler:**
    * Scheduler: how ops are grouped into kernels.
    * **BEAM search:** exploring fusion choices to minimize runtime.
    * Backends: CUDA, OpenCL, Metal, LLVM, custom.
    * Adding a new backend to tinygrad.

* **Production compilers:**
    * **torch.compile + Inductor:** TorchFX → Triton kernels. How `torch._inductor` schedules and generates code.
    * **XLA (Accelerated Linear Algebra):** Used by JAX and TF. HLO IR → target code (TPU, GPU, CPU).
    * **IREE:** MLIR-based compiler for heterogeneous deployment (CPU, GPU, DSP).
    * **Triton-MLIR:** Python-level kernel writing compiled via MLIR pipeline.

* **End-to-end flow comparison:**

| Pipeline | Input | IR(s) | Scheduling | Target |
|----------|-------|-------|------------|--------|
| TVM | ONNX/Relay | Relay → TIR | AutoTVM / Ansor | CUDA, LLVM, custom |
| tinygrad | tinygrad graph | Linearized ops | BEAM search | CUDA, OpenCL, Metal, LLVM |
| torch.compile | TorchFX | FX → Inductor IR | Triton codegen | CUDA (Triton), CPU |
| XLA | HLO | HLO → LLO | XLA scheduler | TPU, GPU, CPU |
| IREE | MLIR (tosa/linalg) | linalg → vector → spirv/llvm | Tile + distribute | CPU, GPU, DSP |

**Projects:**
* Compile a ResNet-18 with TVM for your GPU. Use AutoTVM to tune 3 key ops (conv2d, dense, batch_matmul). Compare latency before/after tuning.
* Study tinygrad BEAM: run with `BEAM=3` on a small model. Compare kernel count and latency vs `BEAM=0`.
* Use `torch.compile` on a transformer block. Read the generated Triton kernel for the attention op. Annotate the tiling strategy.

---

**6. Kernel Fusion & Tiling Strategies**

* **Tiling for memory hierarchy:**
    * Why tile: make working sets fit in L1/L2/shared memory/SRAM scratchpad.
    * Tile size selection: auto-tuning vs analytical models (roofline-guided).
    * Multi-level tiling: thread-block tiles → warp tiles → register tiles (GPU); array tiles → PE tiles (accelerator).

* **Fusion strategies:**
    * **Producer-consumer fusion:** Fuse ops where one's output is the other's input (Conv → ReLU).
    * **Parallel fusion:** Fuse independent ops sharing inputs to reduce reads.
    * **Reduction fusion:** Fuse element-wise ops with reductions (softmax components).
    * Fusion legality: when aliasing or data dependencies prevent fusion.

* **Dataflow scheduling:**
    * Weight-stationary, output-stationary, row-stationary, no-local-reuse.
    * How the compiler maps these strategies to hardware tiling parameters.
    * Connection to Layer 5 (hardware architecture): the compiler must know the hardware's dataflow.

**Projects:**
* Implement a 2-level tiled matmul in C++ (outer tiles for L2, inner tiles for L1). Benchmark vs naive and compare with vendor BLAS.
* In tinygrad or TVM, modify a fusion heuristic. Measure the impact on a real model (kernel count, total latency, memory traffic).

---

**7. Custom Backend Development**

* **What is a custom backend:**
    * The code generator that takes compiler IR and produces instructions for your specific hardware (FPGA, NPU, custom ASIC).

* **TVM BYOC path:**
    * Annotate subgraph → partition → custom codegen function → emit code or runtime calls.
    * Build a minimal BYOC backend for a simulated accelerator.

* **tinygrad custom backend:**
    * Implement `Runtime` and `Compiler` classes for a new target.
    * Map linearized ops to hardware instructions.

* **MLIR custom lowering:**
    * Define target dialect → write lowering pass from `linalg` → emit target-specific code.
    * Integration with LLVM backend or standalone code emitter.

* **Connection to other tracks:**
    * Track A (FPGA): your compiler backend generates HLS directives or RTL control sequences.
    * Track B (Jetson): your compiler backend targets CUDA, DLA, or TensorRT.

**Projects:**
* Build a minimal TVM BYOC backend that offloads `nn.dense` to a Python-simulated accelerator. Verify correctness.
* Add a simple tinygrad backend for a simulated 4×4 systolic array (compute matmul tiles, verify output).
* (Advanced) Write an MLIR lowering pass from `linalg.matmul` to a custom dialect that emits tiled DMA + compute commands.

---

</details>

## 第 2 部分 — DL 推理优化

> *把编译器和 kernel 技能应用到真实推理工作负载：性能剖析、编写 kernel、量化、部署。*

本部分此前是阶段 5 的一个专精方向。现在放在这里，是因为这些技能（性能剖析、kernel 编写、量化、runtime 集成）正是第 1 部分编译器基础的直接应用——而且在进入阶段 5 的高级专精方向（HPC 基础设施、AI 芯片设计）之前必须先掌握。

**目标岗位：** DL Inference Optimization Engineer · **MTS Kernels**（Member of Technical Staff, Kernels）

**第 2 部分的前置要求：** 第 1 部分（至少 §1–§2 和 §5）、阶段 4 方向 B（Jetson、TensorRT、CUDA）。

**按顺序**学习各单元。每个文件夹是一个单元；逐个完成。

| 顺序 | 单元 | 学习内容 | 指南 |
|:-----:|------|----------------|-------|
| **1** | 图与算子优化 | 要优化的对象：图、算子、融合、性能剖析、瓶颈。用 GPU 专属的性能剖析强化第 1 部分 §2。 | [01 →](01-图与算子优化/Guide.md) |
| **2** | Kernel 工程 | 如何编写并负责 kernel：Triton、CUTLASS/CuTe、Flash-Attention、长上下文、NCCL。MTS Kernels 的核心。 | [02 →](02-内核工程/Guide.md) |
| **3** | 编译器栈 | 编译器如何产出 kernel：IR、调度（BEAM）、codegen、TVM、MLIR。通过动手项目强化第 1 部分 §3–§5。 | [03 →](03-编译器栈/Guide.md) |
| **4** | 量化 | 低精度推理：PTQ、QAT、INT8/INT4、kernel 与 runtime 集成。 | [04 →](04-量化/Guide.md) |
| **5** | 推理 runtime 与部署 | 生产环境：TensorRT、ONNX Runtime、Triton server；延迟、吞吐、方法论。 | [05 →](05-推理运行时与部署/Guide.md) |
| **6** | tinygrad 深入剖析 | 可选：动手实践 IR、调度器、后端；编译器-kernel 接口。 | [06 →](06-tinygrad深入解析/Guide.md) |

---

## 基础概念（阅读第 2 部分之前先读）

在深入图优化、kernel 和编译器之前，需要先建立现代大语言模型推理的**词汇与心智模型**，并理解 kernel 工程师为何至关重要。本节为第 2 部分的其余内容做铺垫。

### 大语言模型推理：TensorRT-LLM、vLLM 与核心优化

生产级大语言模型推理服务依赖：

* **在途批处理（动态请求批处理）** — 请求到达即组批；不要等到凑满一个批。在提升吞吐的同时不牺牲延迟。
* **分页 KV-cache** — attention 需要每个 token 的 key/value 缓存；长上下文 = 巨大内存。分页与复用使其内存高效。
* **投机解码** — 用小模型草拟多个 token，用大模型验证；相同输出下前向次数更少。
* **EAGLE 解码与多 token 预测** — 每步预测多个 token 以降低延迟。
* **吞吐与延迟** — 批越大 → 吞吐越高，延迟越差。需要根据 SLA 调整批处理与调度。

会用到的框架：**TensorRT-LLM**、**vLLM**、Hugging Face 集成、NVIDIA NGC 容器。模型：Llama 3/4、DeepSeek R1、Qwen 3、Gemma 3、Phi 4、T5/BART。优化技术：量化（INT8、FP8、FP4）、LoRA 集成、kernel 融合。

### 高级 attention 与内存

* **KV-cache 分片、分页、复用** — 将缓存分散或分页到多个设备，并在请求之间复用内存。
* **长上下文优化** — 100K–1M+ token；内存带宽与 layout 起主导作用。高效的 attention kernel 设计是关键抓手。
* **内存带宽与计算** — 许多推理工作负载是带宽受限的。要优化数据搬运与复用。

### 分布式推理与训练

当模型或批无法装入单块 GPU 时：

* **数据并行** — 相同模型、不同数据；同步梯度（如 AllReduce）。
* **模型 / 张量并行** — 将 layer 或张量切分到多块 GPU。
* **流水线并行** — 不同 layer 放在不同 GPU 上；保持流水线满载。
* **专家并行（MoE）** — 通过对专家做分片来扩展混合专家模型。

在大规模场景下，**通信与同步**占主导。**NCCL**（及其替代方案）成为瓶颈。Kernel 工程师让计算与通信重叠，并减少内存搬运；仅此一项就能带来 20–40% 的加速（例如用计算 + 异步同步，而不是计算 → 同步 → 计算 → 同步）。

### 生产级推理系统

* **分离式推理服务** — 将上下文编码与 token 生成拆分到不同 GPU 或节点。
* **连续批处理** — 在批中增删请求，无需整体清空。
* **高吞吐推理服务** — 面向数百万请求和 100B+ 参数模型的架构与调度。
* **GPU 资源调度** — 利用率、公平性、多租户。


<details>
<summary>English original</summary>

**Part 2 — DL Inference Optimization**

> *Apply compiler and kernel skills to real inference workloads: profile, write kernels, quantize, and deploy.*

This part was previously a Phase 5 specialization track. It now lives here because the skills (profiling, kernel authoring, quantization, runtime integration) are the direct application of Part 1's compiler fundamentals — and they're needed before the advanced Phase 5 specializations (HPC infrastructure, AI chip design).

**Role target:** DL Inference Optimization Engineer · **MTS Kernels** (Member of Technical Staff, Kernels)

**Prerequisites for Part 2:** Part 1 (at least §1–§2 and §5), Phase 4 Track B (Jetson, TensorRT, CUDA).

Study the units **in order**. Each folder is one unit; do them one by one.

| Order | Unit | What you learn | Guide |
|:-----:|------|----------------|-------|
| **1** | Graph & Operator Optimization | What we're optimizing: graph, ops, fusion, profiling, bottlenecks. Reinforces Part 1 §2 with GPU-specific profiling. | [01 →](01-图与算子优化/Guide.md) |
| **2** | Kernel Engineering | How to write and own kernels: Triton, CUTLASS/CuTe, Flash-Attention, long-context, NCCL. Core of MTS Kernels. | [02 →](02-内核工程/Guide.md) |
| **3** | Compiler Stack | How compilers produce kernels: IR, scheduling (BEAM), codegen, TVM, MLIR. Reinforces Part 1 §3–§5 with hands-on projects. | [03 →](03-编译器栈/Guide.md) |
| **4** | Quantization | Low-precision inference: PTQ, QAT, INT8/INT4, kernel and runtime integration. | [04 →](04-量化/Guide.md) |
| **5** | Inference Runtimes & Deployment | Production: TensorRT, ONNX Runtime, Triton server; latency, throughput, methodology. | [05 →](05-推理运行时与部署/Guide.md) |
| **6** | tinygrad Deep Dive | Optional: hands-on IR, scheduler, backends; compiler-kernel interface. | [06 →](06-tinygrad深入解析/Guide.md) |

---

**Basic concepts (read before Part 2)**

Before diving into graph optimization, kernels, and compilers, you need the **vocabulary and mental model** of modern LLM inference and why kernel engineers are critical. This section sets the stage for the rest of Part 2.

**LLM inference: TensorRT-LLM, vLLM, and core optimizations**

Production LLM serving relies on:

* **In-flight batching (dynamic request batching)** — Batch requests as they arrive; don't wait for a full batch. Improves throughput without killing latency.
* **Paged KV-cache** — Attention needs key/value cache per token; long context = huge memory. Paging and reuse make it memory-efficient.
* **Speculative decoding** — Draft multiple tokens with a small model, verify with the big model; fewer forward passes for the same output.
* **EAGLE decoding & multi-token prediction** — Predict several tokens per step to cut latency.
* **Throughput vs latency** — Batch more → higher throughput, worse latency. You tune batching and scheduling to the SLA.

Frameworks you'll work with: **TensorRT-LLM**, **vLLM**, Hugging Face integration, NVIDIA NGC containers. Models: Llama 3/4, DeepSeek R1, Qwen 3, Gemma 3, Phi 4, T5/BART. Optimization techniques: quantization (INT8, FP8, FP4), LoRA integration, kernel fusion.

**Advanced attention & memory**

* **KV-cache sharding, paging, reuse** — Spread or page the cache across devices and reuse memory across requests.
* **Long-context optimization** — 100K–1M+ tokens; memory bandwidth and layout dominate. Efficient attention kernel design is the lever.
* **Memory bandwidth vs compute** — Many inference workloads are memory-bound. You optimize data movement and reuse.

**Distributed inference & training**

When the model or batch doesn't fit on one GPU:

* **Data parallelism** — Same model, different data; sync gradients (e.g. AllReduce).
* **Model / tensor parallelism** — Split layers or tensors across GPUs.
* **Pipeline parallelism** — Different layers on different GPUs; keep the pipeline full.
* **Expert parallelism (MoE)** — Scale mixture-of-experts by sharding experts.

At scale, **communication and synchronization** dominate. **NCCL** (and alternatives) become the bottleneck. Kernel engineers overlap compute with communication and reduce memory movement; that alone can yield 20–40% speedup (e.g. compute + async sync instead of compute → sync → compute → sync).

**Production inference systems**

* **Disaggregated serving** — Split context encoding vs token generation across GPUs or nodes.
* **Continuous batching** — Add and remove requests from the batch without full flush.
* **High-throughput serving** — Architecture and scheduling for millions of requests and 100B+ parameter models.
* **GPU resource scheduling** — Utilization, fairness, multi-tenant.

</details>

### CuTe DSL（CUDA Template Engine）——为何它会出现在 kernel 工作中

读 CUTLASS、cuBLASLt 或 kernel 相关的分享时，你会看到 **CuTe**（CUDA Template Engine）。它是一个 C++ 头文件库兼 DSL，用于定义 **layout**（张量维度如何映射到内存：shape + stride，可能分块）与 **copy** 操作（向量化、异步、可组合的 load/store）。Kernel 作者用 CuTe 描述分块（例如 block 分块、warp 分块、线程分块）与全局内存、共享内存和寄存器之间的数据移动，而无需手写索引。这让你更容易达到峰值性能，并在硬件变化时（例如 Blackwell 上新的分块尺寸）重新适配。在本方向中，你会在 **02 — Kernel Engineering**（CUTLASS/CuTe）以及学习生产级 GEMM/attention kernel 时遇到它。

### 新架构 = 新的 kernel 挑战

每一代新 GPU（例如 **NVIDIA Blackwell**、Hopper、Ada Lovelace）都会改变：

* **执行模型** — warp 调度、occupancy、需要多少 warp 来隐藏延迟。
* **存储层次** — 寄存器、共享内存、L2、HBM 的容量与带宽。
* **指令吞吐** — 新的算子（例如 Transformer Engine、FP4），以及不同的最优分块尺寸。

旧的 kernel 与分块尺寸可能变得次优甚至错误。**分块**（能放进共享内存/寄存器中的块）必须重新调优：较老的 GPU → 更小的分块；较新的 GPU → 更大的共享内存 → 更大的分块。分块尺寸错误 → 低 occupancy、内存停顿。**warp 调度**与**内存延迟模式**也会变化；你要去测量并适配，而不是沿用旧技巧。

### 为什么针对硬件的优化很重要（例如 Blackwell）

* **为下一代大语言模型而构建** — 巨型 Transformer、长上下文、高吞吐推理。
* **内存是瓶颈** — 大语言模型往往是带宽受限的。Blackwell 改进了 HBM 与数据移动；你的任务是在 attention 与 layout 中利用它。
* **Transformer Engine / 低精度** — FP4 与混合精度流水线；你要编写使用它们且保持数值稳定的 kernel。
* **多 GPU 扩展** — 更好的 NVLink/互连；对分布式训练和大规模推理集群至关重要。

那些要求"Blackwell 经验"的公司，意思是：*你能比别人更早榨干最新硬件的性能吗？* 这意味着性能剖析（Nsight Compute、Nsight Systems）、第一性原理推理（带宽 vs 算力、延迟 vs occupancy），以及在旧假设不成立时果断抛弃它们。

### 工程关注点

* **端到端流水线优化** — 从计算图到已部署的 kernel。
* **性能剖析与瓶颈分析** — 时间花在哪里？SM 为什么空闲？停顿在哪里？
* **可扩展部署** — 单 GPU → 多 GPU → 集群。
* **生产级可靠性与性能调优** — 可度量的延迟/吞吐，而不只是 benchmark。

**一句话：** 这个角色要做的是，在别人还没搞懂之前，把新的 GPU 硬件转化为真实世界的 AI 性能收益。

### Part 2 技能小结

| 领域 | 关键技能 |
|------|------------|
| 计算图与算子 | 融合、常量折叠、性能剖析、瓶颈分析 |
| **Kernel 工程** | Triton、CUTLASS、CuTe；Flash-Attention、长上下文；NCCL/MSCCLPP；TPU/Pallas/Mojo；测试、正确性、移植 |
| 编译器 | IR、调度（例如 BEAM）、codegen、TVM/MLIR 概念 |
| 量化 | PTQ、QAT、INT8/INT4、工具链（TensorRT、ONNX Runtime） |
| Runtime | TensorRT、ONNX Runtime、Triton；延迟/吞吐方法论 |

---

## 与其他方向的关系

| 本方向（C）提供 | 方向 A（FPGA）用它来 | 方向 B（Jetson）用它来 |
|--------------------------|---------------------------|------------------------------|
| 计算图优化 pass | 把 ONNX 映射为 HLS 友好的子图 | 理解 TensorRT 计算图优化 |
| MLIR / TVM 编译 | Vitis AI / FINN 编译流程 | GPU 上的 torch.compile + Inductor |
| 自定义后端开发 | TVM 或 tinygrad 中的 FPGA 后端 | DLA/TensorRT 后端集成 |
| 分块与 dataflow 调度 | HLS pragma 驱动的分块 | CUDA kernel 分块策略 |
| IR 与 SSA 基础 | 理解 Vivado 综合 IR | 理解 NVPTX 代码生成 |
| Kernel 工程（Part 2） | — | 用于 GPU 推理的 Triton/CUTLASS kernel |
| 量化（Part 2） | 理解 Vitis AI 量化器 | TensorRT INT8/INT4 部署 |
| 推理 runtime（Part 2） | Vitis AI / FINN runtime | TensorRT、Triton server、DeepStream |

---

## Build 小结


<details>
<summary>English original</summary>

**CuTe DSL (CUDA Template Engine) — why it shows up in kernel work**

When you read CUTLASS, cuBLASLt, or kernel talks, you'll see **CuTe** (CUDA Template Engine). It's a C++ header library and DSL that defines **layouts** (how tensor dimensions map to memory: shape + stride, possibly tiled) and **copy** operations (vectorized, async, composable loads/stores). Kernel authors use CuTe to describe tiling (e.g. block tile, warp tile, thread tile) and data movement between global memory, shared memory, and registers without hand-written indexing. That makes it easier to get peak performance and to retarget when hardware changes (e.g. new tile sizes on Blackwell). In this track you'll meet it in **02 — Kernel Engineering** (CUTLASS/CuTe) and when studying production GEMM/attention kernels.

**New architecture = new kernel challenges**

Every new GPU generation (e.g. **NVIDIA Blackwell**, Hopper, Ada Lovelace) changes:

* **Execution model** — Warp scheduling, occupancy, how many warps hide latency.
* **Memory hierarchy** — Registers, shared memory, L2, HBM sizes and bandwidth.
* **Instruction throughput** — New ops (e.g. Transformer Engine, FP4), different optimal tile sizes.

Old kernels and tile sizes can become suboptimal or wrong. **Tiling** (blocks that fit in shared memory/registers) must be retuned: older GPUs → smaller tiles; newer GPUs → larger shared memory → bigger tiles. Wrong tile size → low occupancy, memory stalls. **Warp scheduling** and **memory latency patterns** also change; you measure and adapt instead of reusing old tricks.

**Why hardware-specific optimization matters (e.g. Blackwell)**

* **Built for next-gen LLMs** — Huge transformers, long context, high-throughput inference.
* **Memory is the bottleneck** — LLMs are often memory-bound. Blackwell improves HBM and data movement; your job is to exploit it in attention and layout.
* **Transformer Engine / low precision** — FP4 and mixed-precision pipelines; you write kernels that use them and stay numerically stable.
* **Multi-GPU scaling** — Better NVLink/interconnects; critical for distributed training and large inference clusters.

Companies that ask for "Blackwell experience" mean: *can you get the most out of the latest hardware before everyone else?* That implies profiling (Nsight Compute, Nsight Systems), first-principles reasoning (bandwidth vs compute, latency vs occupancy), and throwing away old assumptions when they don't hold.

**Engineering focus**

* **End-to-end pipeline optimization** — From graph to deployed kernel.
* **Profiling and bottleneck analysis** — Where is time spent? Why is the SM idle? Where are the stalls?
* **Scalable deployment** — Single GPU → multi-GPU → clusters.
* **Production-grade reliability and performance tuning** — Measurable latency/throughput, not just benchmarks.

**In one sentence:** this role is about turning new GPU hardware into real-world AI performance gains before anyone else knows how.

**Part 2 skills summary**

| Area | Key skills |
|------|------------|
| Graph & operators | Fusion, constant folding, profiling, bottleneck analysis |
| **Kernel engineering** | Triton, CUTLASS, CuTe; Flash-Attention, long-context; NCCL/MSCCLPP; TPU/Pallas/Mojo; testing, correctness, porting |
| Compiler | IR, scheduling (e.g. BEAM), codegen, TVM/MLIR concepts |
| Quantization | PTQ, QAT, INT8/INT4, tooling (TensorRT, ONNX Runtime) |
| Runtimes | TensorRT, ONNX Runtime, Triton; latency/throughput methodology |

---

**Relationship to Other Tracks**

| This track (C) provides | Track A (FPGA) uses it for | Track B (Jetson) uses it for |
|--------------------------|---------------------------|------------------------------|
| Graph optimization passes | Mapping ONNX → HLS-friendly subgraphs | TensorRT graph optimization understanding |
| MLIR / TVM compilation | Vitis AI / FINN compilation flow | torch.compile + Inductor on GPU |
| Custom backend development | FPGA backend in TVM or tinygrad | DLA/TensorRT backend integration |
| Tiling & dataflow scheduling | HLS pragma-driven tiling | CUDA kernel tiling strategies |
| IR & SSA fundamentals | Understanding Vivado synthesis IR | Understanding NVPTX code generation |
| Kernel engineering (Part 2) | — | Triton/CUTLASS kernels for GPU inference |
| Quantization (Part 2) | Vitis AI quantizer understanding | TensorRT INT8/INT4 deployment |
| Inference runtimes (Part 2) | Vitis AI / FINN runtime | TensorRT, Triton server, DeepStream |

---

**Build Summary**

</details>

### 第 1 部分 — 编译器基础

| 模块 | 动手实践交付物 |
|--------|---------------------|
| §1 IR | ONNX 图分析 + tinygrad IR trace |
| §2 图优化 | Conv+BN+ReLU 融合 pass、内存规划器 |
| §3 LLVM | 自定义 LLVM pass（FMA 计数器） |
| §4 MLIR | Toy tutorial + 最小 NPU dialect |
| §5 流水线 | TVM AutoTVM 调优、BEAM 对比、torch.compile 分析 |
| §6 融合/分块 | 分块矩阵乘、融合启发式修改 |
| §7 自定义后端 | 用于模拟加速器的 TVM BYOC 或 tinygrad 后端 |

### 第 2 部分 — 深度学习推理优化

| 单元 | 动手实践交付物 |
|------|---------------------|
| 01 图与算子 | 融合 + 测量、性能剖析报告、端到端拆解 |
| 02 kernel 工程 | Triton 融合 kernel、长上下文 attention、大规模 NCCL |
| 03 编译器栈 | tinygrad 中的 BEAM、融合 pass、lowering trace |
| 04 量化 | 使用 TensorRT 的 INT8、PTQ 与 QAT 对比、kernel 路径 trace |
| 05 runtime | runtime 对比、Triton server 搭建、benchmark 报告 |
| 06 tinygrad（可选） | 流水线 trace、添加优化、后端 hook |


<details>
<summary>English original</summary>

**Part 1 — Compiler Fundamentals**

| Module | Hands-on deliverable |
|--------|---------------------|
| §1 IR | ONNX graph analysis + tinygrad IR trace |
| §2 Graph opts | Conv+BN+ReLU fusion pass, memory planner |
| §3 LLVM | Custom LLVM pass (FMA counter) |
| §4 MLIR | Toy tutorial + minimal NPU dialect |
| §5 Pipelines | TVM AutoTVM tuning, BEAM comparison, torch.compile analysis |
| §6 Fusion/tiling | Tiled matmul, fusion heuristic modification |
| §7 Custom backend | TVM BYOC or tinygrad backend for simulated accelerator |

**Part 2 — DL Inference Optimization**

| Unit | Hands-on deliverable |
|------|---------------------|
| 01 Graph & ops | Fusion + measure, profiling report, end-to-end breakdown |
| 02 Kernel engineering | Triton fused kernel, long-context attention, NCCL at scale |
| 03 Compiler stack | BEAM in tinygrad, fusion pass, lowering trace |
| 04 Quantization | INT8 with TensorRT, PTQ vs QAT comparison, kernel path trace |
| 05 Runtimes | Runtime comparison, Triton server setup, benchmark report |
| 06 tinygrad (optional) | Pipeline trace, add optimization, backend hook |

</details>

---

> 原文：[`Phase 4 - Track C - DL Inference Optimization/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20C%20-%20DL%20Inference%20Optimization/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
