---
title: 03 — 推理的编译器栈（IR、调度、代码生成）
description: 03 — 推理的编译器栈（IR、调度、代码生成）
---

# 03 — 推理的编译器栈（IR、调度、代码生成）

<div class="course-identity auto-course" style="--course-accent: #0891b2; --course-accent-rgb: 8, 145, 178;" markdown="1">
<div class="course-identity__icon">CSFI</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入专题 · 编译器方向</p>
<p class="course-identity__title">03 — 推理的编译器栈（IR、调度、代码生成）的专属课程标识。</p>
<p class="course-identity__meta">产物：compiler/inference optimization · 度量：op 数量、内存、延迟</p>
</div>
</div>


**顺序：** 第三。在 graph/ops（01）与 kernel 编写（02）之后，看编译器如何生成并调度 kernel。

**岗位目标：** DL Inference Optimization Engineer · **MTS Kernels**（Member of Technical Staff, Kernels —— 聚焦代码生成、编译器与硬件的映射，并负责大规模 kernel/后端实现的岗位）。

---

## 为什么排在第三

kernel（02）要么手写，要么由编译器生成。本单元讲编译器如何表示模型（IR）、如何决定融合与布局（调度），以及如何生成代码（codegen）。要与框架协同设计、要新增或调优后端，就需要这些。

---

## 1. 中间表示（IR）

* **图 IR 与线性化 IR** —— 图：节点 = op，边 = 张量。线性化：按执行顺序排列的 op 列表（例如 tinygrad 的线性化 op）。
* **SSA 形式** —— 静态单赋值；每个值只定义一次。便于清晰的内存与别名分析。
* **内存与别名分析** —— 哪些 buffer 可以重叠；何时融合或原地操作是安全的。

**要点：** IR 是「模型图」与「kernel 后端」之间的契约。你的 kernel 就是该 IR 向下 lowering 的目标。

---

## 2. 调度与 lowering

* **tinygrad** —— 调度器、用于 kernel 融合与布局的 BEAM 搜索。单 op 与融合 op；BEAM 如何探索融合选择。
* **TVM** —— TIR（Tensor IR）、AutoTVM/AutoScheduler 用于映射到硬件。调度原语（分块、向量化、并行）。
* **MLIR** —— linalg/tensor 方言；渐进式 lowering（linalg → 循环 → 向量 → gpu）。高层 op 如何变成循环、再变成 GPU kernel。

**要点：** 调度决定*哪些* kernel 运行（融合与否）以及*如何*分块/并行；随后 codegen 生成 CUDA/LLVM 等。

---

## 3. 代码生成

* **后端 codegen** —— 从 IR/调度到 CUDA、OpenCL、LLVM 或自定义目标。codegen 在 Triton、TVM、tinygrad 中的角色。
* **kernel 融合与分块选择** —— 编译器如何为 GPU 与加速器选择分块大小与融合集合。

---

## 资源

* [tinygrad](https://github.com/tinygrad/tinygrad) —— IR、调度器、BEAM、后端。
* [TVM Documentation](https://tvm.apache.org/docs/) —— TIR、AutoTVM、BYOC。
* [MLIR Tutorial](https://mlir.llvm.org/docs/Tutorials/) —— 方言与 lowering。

---

## 项目

1. **tinygrad 中的 BEAM** —— 在小模型上用 BEAM 运行 tinygrad。对比调度后的 kernel 数量与 runtime 与默认调度器的差异；记录 BEAM 融合了什么。
2. **融合 pass** —— 在你自己可控的图（ONNX 或 tinygrad）中实现一个简单的融合 pass（例如 Conv+ReLU）。度量其对 kernel 数量与延迟的影响。
3. **追踪 lowering** —— 在 TVM 或 tinygrad 中选一个 op（例如矩阵乘），从高层 op 一路 trace 到生成的代码。记录 lowering 的各个步骤。

---

## 下一步

→ **[04 — 量化](../04-量化/Guide.md)** —— 低精度推理（PTQ、QAT）及其对 kernel 与部署的影响。


<details>
<summary>English original</summary>

**03 — Compiler Stack for Inference (IR, Scheduling, Codegen)**

<div class="course-identity auto-course" style="--course-accent: #0891b2; --course-accent-rgb: 8, 145, 178;" markdown="1">
<div class="course-identity__icon">CSFI</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Compiler Track</p>
<p class="course-identity__title">Specialized course identity for 03 — Compiler Stack for Inference (IR, Scheduling, Codegen).</p>
<p class="course-identity__meta">Artifact: compiler/inference optimization · Measure: op count, memory, latency</p>
</div>
</div>


**Order:** Third. After graph/ops (01) and kernel authoring (02), you see how compilers generate and schedule kernels.

**Role target:** DL Inference Optimization Engineer · **MTS Kernels** (Member of Technical Staff, Kernels — roles focused on code generation, compiler–hardware mapping, and owning kernel/backend implementation at scale).

---

**Why this comes third**

Kernels (02) are either hand-written or compiler-generated. This unit covers how compilers represent the model (IR), decide fusion and placement (scheduling), and emit code (codegen). You need this to co-design with frameworks and to add or tune backends.

---

**1. Intermediate representation (IR)**

* **Graph IR vs linearized IR** — Graph: nodes = ops, edges = tensors. Linearized: list of ops in execution order (e.g. tinygrad's linearized ops).
* **SSA form** — Single assignment; each value defined once. Enables clear memory and alias analysis.
* **Memory and alias analysis** — Which buffers can overlap; when fusion or in-place is safe.

**Takeaway:** The IR is the contract between "model graph" and "kernel backend." Your kernels are targets for lowering from this IR.

---

**2. Scheduling and lowering**

* **tinygrad** — Scheduler, BEAM search for kernel fusion and placement. One op vs fused op; how BEAM explores fusion choices.
* **TVM** — TIR (Tensor IR), AutoTVM/AutoScheduler for mapping to hardware. Schedule primitives (tile, vectorize, parallel).
* **MLIR** — linalg/tensor dialects; progressive lowering (linalg → loops → vector → gpu). How high-level ops become loops and then GPU kernels.

**Takeaway:** Scheduling decides *which* kernels run (fused or not) and *how* they're tiled/parallelized; codegen then emits CUDA/LLVM/etc.

---

**3. Code generation**

* **Backend codegen** — From IR/schedule to CUDA, OpenCL, LLVM, or custom target. Role of codegen in Triton, TVM, tinygrad.
* **Kernel fusion and tile selection** — How the compiler chooses tile sizes and fusion sets for GPUs and accelerators.

---

**Resources**

* [tinygrad](https://github.com/tinygrad/tinygrad) — IR, scheduler, BEAM, backends.
* [TVM Documentation](https://tvm.apache.org/docs/) — TIR, AutoTVM, BYOC.
* [MLIR Tutorial](https://mlir.llvm.org/docs/Tutorials/) — Dialects and lowering.

---

**Projects**

1. **BEAM in tinygrad** — Run tinygrad with BEAM on a small model. Compare scheduled kernel count and runtime vs default scheduler; document what BEAM fused.
2. **Fusion pass** — Implement a simple fusion pass (e.g. Conv+ReLU) in a graph you control (ONNX or tinygrad). Measure impact on kernel count and latency.
3. **Trace lowering** — Pick one op (e.g. matmul) in TVM or tinygrad and trace from high-level op to generated code. Document the lowering steps.

---

**Next**

→ **[04 — Quantization](../04-量化/Guide.md)** — Low-precision inference (PTQ, QAT) and how it affects kernels and deployment.

</details>

---

> 原文：[`Phase 4 - Track C - DL Inference Optimization/03 - Compiler Stack/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20C%20-%20DL%20Inference%20Optimization/03%20-%20Compiler%20Stack/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
