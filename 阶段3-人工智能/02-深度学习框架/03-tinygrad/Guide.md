---
title: tinygrad — 可改造的编译器框架
description: tinygrad — 可改造的编译器框架
---

# tinygrad — 可改造的编译器框架

<div class="course-identity auto-course" style="--course-accent: #0891b2; --course-accent-rgb: 8, 145, 178;" markdown="1">
<div class="course-identity__icon">TTHC</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入探索 · AI 工作负载</p>
<p class="course-identity__title">tinygrad — 可改造的编译器框架的专属课程标识。</p>
<p class="course-identity__meta">产物：模型或工作负载研究 · 度量：准确率、延迟、内存、吞吐</p>
</div>

</div>


**父模块：** [模块 2 — 深度学习框架](../Guide.md)

> *一个极简的深度学习框架（约 10K 行），用可读的 Python 暴露整条编译器流水线。要理解 `loss.backward()` 与真正执行的 GPU kernel 之间发生了什么，它是理想的代码库。*

---

## 为什么 tinygrad 具有独特价值

- 它是 **openpilot**（阶段 5E）内部的推理引擎
- 它暴露了 **阶段 4C** 教你构建的 IR、调度器和代码生成
- 你可以添加一个**自定义后端**（阶段 4C §7），面向你自己的加速器
- 它可运行在 CUDA、OpenCL、Metal、LLVM 以及自定义目标上

## 关键概念

| 概念 | 它教什么 | 技术栈关联 |
|---------|----------------|-----------------|
| 惰性求值 | 在 `.realize()` 之前什么都不执行 | L2：由编译器决定何时执行 |
| 3 种操作类型 | Elementwise、Reduce、Movement（25 个原语） | L5：PE 阵列必须支持什么 |
| ShapeTracker | 零拷贝 reshape 与 transpose | L2：内存布局优化 |
| UOp IR | 代码生成前的中间表示 | L2：与 MLIR/TVM IR 概念相同 |
| BEAM search | 探索各种融合方案以最小化 runtime | L2：自动调优 |
| 后端 | 同一份 IR 生成 CUDA、OpenCL、LLVM 代码 | L2：多目标编译 |

## 项目

1. **trace 一个矩阵乘：** `Tensor` → lazy buffer → scheduled ops → 生成的 CUDA kernel。记录每一步。
2. **DEBUG=4：** 运行一个小模型，阅读生成的 kernel。统计 kernel 启动次数。
3. **BEAM=3 vs BEAM=0：** 在同一模型上比较 kernel 数量和延迟。
4. **日志后端：** 添加一个最小化的钩子，打印每次 kernel 启动。

## 深入探索

完整的 tinygrad 学习路径（11 个部分、7 个项目）见 [阶段 5E — 自动驾驶汽车 / tinygrad](../../../阶段5-高级专题与专精/05-方向E-自动驾驶/03-tinygrad推理/Guide.md)。

## 资源

- [tinygrad GitHub](https://github.com/tinygrad/tinygrad)
- [tinygrad Discord](https://discord.gg/tinygrad)


<details>
<summary>English original</summary>

**tinygrad — The Hackable Compiler Framework**

<div class="course-identity auto-course" style="--course-accent: #0891b2; --course-accent-rgb: 8, 145, 178;" markdown="1">
<div class="course-identity__icon">TTHC</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · AI Workloads</p>
<p class="course-identity__title">Specialized course identity for tinygrad — The Hackable Compiler Framework.</p>
<p class="course-identity__meta">Artifact: model or workload study · Measure: accuracy, latency, memory, throughput</p>
</div>
</div>


**Parent:** [Module 2 — Deep Learning Frameworks](../Guide.md)

> *A minimal DL framework (~10K lines) that exposes the entire compiler pipeline in readable Python. The ideal codebase for understanding what happens between `loss.backward()` and the GPU kernel that actually runs.*

---

**Why tinygrad Is Uniquely Valuable**

- It's the inference engine inside **openpilot** (Phase 5E)
- It exposes the IR, scheduler, and code generation that **Phase 4C** teaches you to build
- You can add a **custom backend** (Phase 4C §7) targeting your own accelerator
- It runs on CUDA, OpenCL, Metal, LLVM, and custom targets

**Key Concepts**

| Concept | What it teaches | Stack connection |
|---------|----------------|-----------------|
| Lazy evaluation | Nothing runs until `.realize()` | L2: compiler decides when to execute |
| 3 operation types | Elementwise, Reduce, Movement (25 primitives) | L5: what the PE array must support |
| ShapeTracker | Zero-copy reshapes and transposes | L2: memory layout optimization |
| UOp IR | Intermediate representation before codegen | L2: same concept as MLIR/TVM IR |
| BEAM search | Explores fusion choices to minimize runtime | L2: auto-tuning |
| Backends | Same IR generates CUDA, OpenCL, LLVM code | L2: multi-target compilation |

**Projects**

1. **Trace a matmul:** `Tensor` → lazy buffer → scheduled ops → generated CUDA kernel. Document every step.
2. **DEBUG=4:** Run a small model, read the generated kernels. Count kernel launches.
3. **BEAM=3 vs BEAM=0:** Compare kernel count and latency on the same model.
4. **Logging backend:** Add a minimal hook that prints each kernel launch.

**Deep Dive**

The full tinygrad learning path (11 parts, 7 projects) is in [Phase 5E — Autonomous Vehicles / tinygrad](../../../阶段5-高级专题与专精/05-方向E-自动驾驶/03-tinygrad推理/Guide.md).

**Resources**

- [tinygrad GitHub](https://github.com/tinygrad/tinygrad)
- [tinygrad Discord](https://discord.gg/tinygrad)

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/2. Deep Learning Frameworks/tinygrad/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/2.%20Deep%20Learning%20Frameworks/tinygrad/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
