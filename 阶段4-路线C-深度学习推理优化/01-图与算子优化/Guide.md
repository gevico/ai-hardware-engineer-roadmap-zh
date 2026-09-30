---
title: 01 — 图与算子优化
description: 01 — 图与算子优化
---

# 01 — 图与算子优化

<div class="course-identity auto-course" style="--course-accent: #0284c7; --course-accent-rgb: 2, 132, 199;" markdown="1">
<div class="course-identity__icon">GAOO</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度专题 · 编译器方向</p>
<p class="course-identity__title">01 — 图与算子优化的专属课程标识。</p>
<p class="course-identity__meta">产物：compiler/inference optimization · 度量：op 数、内存、延迟</p>
</div>
</div>


**顺序：** 第一（基础）。写 kernel 之前，必须先知道自己要优化的是*什么*。

**目标岗位：** DL Inference Optimization Engineer · MTS Kernels

**本节前置：** 阅读课程轨道指南中的[基础概念](../Guide.md)（LLM 推理、TensorRT-LLM/vLLM、分布式训练、KV-cache，以及新硬件为何会改变 kernel 设计）。

---

## 为什么先讲这一节

在编写或调优 kernel 之前，必须：

1. **理解计算图** — 哪些算子会运行、以什么顺序运行、彼此如何连接。
2. **找出瓶颈** — 哪些算子或 layer 是算力受限、哪些是带宽受限。
3. **了解融合机会** — 哪些算子链可以变成单个 kernel（例如 Conv–BN–ReLU）。

本节给出计算图/算子视角与性能剖析技能，这是每位 kernel 工程师每天都会用到的。

---

## 1. 图级优化

* **常量折叠** — 在构建期求值常量子图（例如 shape 算子、固定权重）。
* **死代码消除** — 删除输出从未被使用的算子。
* **公共子表达式消除（CSE）** — 复用已算出的值，而不是重新计算。
* **算子融合** — 把多个算子合并成一个 kernel：
    * Conv–BN–ReLU、Linear–Activation、Attention（Q/K/V + softmax + 矩阵乘）。
    * 减少内存流量与 kernel 启动开销。
* **布局与形状变换** — NCHW 与 NHWC、转置折叠、为硬件友好的布局做 reshape/expand。
* **框架计算图格式** — ONNX、TorchScript、TensorFlow SavedModel；各自如何应用优化 pass。

**需要内化的概念：** 模型里的单个「layer」在计算图中常常变成许多算子；融合再把它们变回数量更少、速度更快的 kernel。

---

## 2. 算子级优化

* **kernel 选择与派发** — runtime 如何挑选实现：cuBLAS、cuDNN、oneDNN，或自定义 kernel。算法选择（例如 conv 算法）与启发式策略。
* **内存规划** — buffer 分配、在安全处使用 in-place 算子（输入/输出共用同一 buffer）、降低峰值内存。
* **批处理与动态批处理** — 为推理服务做请求批处理；延迟与吞吐之间的权衡。

---

## 3. 性能剖析与瓶颈定位

* **工具：**
    * **Nsight Systems** — 时间线视图：kernel 启动、内存拷贝、CPU–GPU 重叠。
    * **Nsight Compute** — 逐 kernel：occupancy、内存吞吐、算力利用率。
    * **PyTorch profiler** — Python 中的算子级与 kernel 级计时。
    * **ONNX Runtime** — execution provider 计时、算子开销。
* **roofline 式分析（性能上界模型）** — 对每个主要算子/layer：算力受限还是带宽受限；算术强度与 roofline 上界。
* **端到端延迟拆解** — 数据加载 → 预处理 → 推理（逐 layer）→ 后处理。时间花在哪里？

**目标：** 从一次模型运行中，就能指出前 3–5 个瓶颈，并说明它们属于算力受限还是带宽受限。

---

## 资源

* [TensorRT Developer Guide](https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/) — 计算图优化与 layer 融合。
* [ONNX Runtime Performance Tuning](https://onnxruntime.ai/docs/performance/) — 计算图与 execution provider 调优。
* [PyTorch Profiler](https://pytorch.org/tutorials/recipes/recipes/profiler_recipe.html) — 剖析 PyTorch 模型。
* [NVIDIA Nsight Systems / Compute](https://developer.nvidia.com/nsight-systems) — GPU 性能剖析。

---

## 项目

1. **融合并度量** — 取一个 ResNet 风格的模型（或小型 Transformer）。在 ONNX 或 TorchScript 中融合 Conv–BN–ReLU（或使用自带该功能的框架）。测量融合前后的延迟；记录加速比。
2. **剖析并报告** — 用 Nsight Systems 和 PyTorch profiler 剖析一个 Transformer block（attention + FFN）。找出前 3 个瓶颈；逐个说明它是算力受限还是带宽受限，并给出理由。
3. **端到端拆解** — 对一条推理流水线（例如图像 → 模型 → 结果），把时间拆成：数据加载、预处理、每个主要计算图区域、后处理。画一条简单时间线，并标出占比最大的片段。

---

## 下一节

→ **[02 — Kernel 工程](../02-内核工程/Guide.md)** — 设计并实现支撑这些算子的高性能 kernel（Triton、CUTLASS、Flash-Attention）。


<details>
<summary>English original</summary>

**01 — Graph and Operator Optimization**

<div class="course-identity auto-course" style="--course-accent: #0284c7; --course-accent-rgb: 2, 132, 199;" markdown="1">
<div class="course-identity__icon">GAOO</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Compiler Track</p>
<p class="course-identity__title">Specialized course identity for 01 — Graph and Operator Optimization.</p>
<p class="course-identity__meta">Artifact: compiler/inference optimization · Measure: op count, memory, latency</p>
</div>
</div>


**Order:** First (foundation). You need to know *what* you're optimizing before writing kernels.

**Role target:** DL Inference Optimization Engineer · MTS Kernels

**Before this unit:** Read [Basic concepts](../Guide.md) in the track guide (LLM inference, TensorRT-LLM/vLLM, distributed training, KV-cache, and why new hardware changes kernel design).

---

**Why this comes first**

Before writing or tuning kernels, you must:

1. **Understand the graph** — which ops run, in what order, and how they connect.
2. **Find bottlenecks** — which ops or layers are compute-bound vs memory-bound.
3. **Know fusion opportunities** — which op chains can become a single kernel (e.g. Conv–BN–ReLU).

This unit gives you the graph/operator view and profiling skills that every kernel engineer uses daily.

---

**1. Graph-level optimizations**

* **Constant folding** — Evaluate constant subgraphs at build time (e.g. shape ops, fixed weights).
* **Dead code elimination** — Remove ops whose outputs are never used.
* **Common subexpression elimination (CSE)** — Reuse computed values instead of recomputing.
* **Operator fusion** — Combine multiple ops into one kernel:
    * Conv–BN–ReLU, Linear–Activation, Attention (Q/K/V + softmax + matmul).
    * Reduces memory traffic and kernel launch overhead.
* **Layout and shape transformations** — NCHW vs NHWC, transpose folding, reshape/expand for hardware-friendly layouts.
* **Framework graph formats** — ONNX, TorchScript, TensorFlow SavedModel; how optimization passes are applied in each.

**Concepts to internalize:** A single "layer" in a model often becomes many ops in the graph; fusion turns them back into fewer, faster kernels.

---

**2. Operator-level optimization**

* **Kernel selection and dispatch** — How runtimes choose implementations: cuBLAS, cuDNN, oneDNN, or custom kernels. Algorithm selection (e.g. conv algorithm) and heuristics.
* **Memory planning** — Buffer allocation, in-place ops where safe (same buffer for input/output), reducing peak memory.
* **Batching and dynamic batching** — Batching requests for inference servers; trade-offs between latency and throughput.

---

**3. Profiling and bottleneck identification**

* **Tools:**
    * **Nsight Systems** — Timeline view: kernel launches, memory copies, CPU–GPU overlap.
    * **Nsight Compute** — Per-kernel: occupancy, memory throughput, compute utilization.
    * **PyTorch profiler** — Op-level and kernel-level timing in Python.
    * **ONNX Runtime** — Execution provider timing, operator cost.
* **Roofline-style analysis** — For each major op/layer: compute-bound vs memory-bound; arithmetic intensity and roofline limits.
* **End-to-end latency breakdown** — Data loading → preprocess → inference (per layer) → postprocess. Where does time go?

**Goal:** From a single model run, you should be able to name the top 3–5 bottlenecks and say whether they are compute or memory bound.

---

**Resources**

* [TensorRT Developer Guide](https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/) — Graph optimization and layer fusion.
* [ONNX Runtime Performance Tuning](https://onnxruntime.ai/docs/performance/) — Graph and execution provider tuning.
* [PyTorch Profiler](https://pytorch.org/tutorials/recipes/recipes/profiler_recipe.html) — Profiling PyTorch models.
* [NVIDIA Nsight Systems / Compute](https://developer.nvidia.com/nsight-systems) — GPU profiling.

---

**Projects**

1. **Fusion and measure** — Take a ResNet-style model (or small transformer). Fuse Conv–BN–ReLU in ONNX or TorchScript (or use a framework that does it). Measure latency before and after; document the speedup.
2. **Profile and report** — Profile a transformer block (attention + FFN) with Nsight Systems and PyTorch profiler. Identify the top 3 bottlenecks; for each, state whether it is compute-bound or memory-bound and why.
3. **End-to-end breakdown** — For one inference pipeline (e.g. image → model → result), break down time into: data load, preprocess, each major graph region, postprocess. Draw a simple timeline and note the largest segment.

---

**Next**

→ **[02 — Kernel Engineering](../02-内核工程/Guide.md)** — Design and implement the high-performance kernels that implement these ops (Triton, CUTLASS, Flash-Attention).

</details>

---

> 原文：[`Phase 4 - Track C - DL Inference Optimization/01 - Graph and Operator Optimization/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20C%20-%20DL%20Inference%20Optimization/01%20-%20Graph%20and%20Operator%20Optimization/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
