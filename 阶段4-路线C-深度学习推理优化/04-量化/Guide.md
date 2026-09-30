---
title: 04 — 量化与低精度推理
description: 04 — 量化与低精度推理
---

# 04 — 量化与低精度推理

<div class="course-identity auto-course" style="--course-accent: #475569; --course-accent-rgb: 71, 85, 105;" markdown="1">
<div class="course-identity__icon">QLPI</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度解析 · 编译器方向</p>
<p class="course-identity__title">04 的专用课程标识 — 量化与低精度推理。</p>
<p class="course-identity__meta">产物：compiler/inference optimization · 度量：op 数量、内存、延迟</p>
</div>
</div>


**顺序：** 第四。在具备 kernel 与编译器上下文（01–03）之后，再加入低精度 kernel 与部署。

**角色目标：** DL Inference Optimization Engineer · **MTS Kernels**（INT8/INT4 kernel、量化感知实现）。

---

## 为什么排在第四

量化（INT8、INT4 等）是降低延迟、提升吞吐的重要手段。它涉及图变换、kernel 实现（整数矩阵乘、量化 attention）以及 runtime 集成。先做 01–03，才能看清量化在哪里接入，以及它如何影响 kernel 选择与融合。

---

## 1. 训练后量化（PTQ）

* **INT8 / INT4** — 校准（代表性数据）、per-tensor 与 per-channel 的 scale、校准数据集。
* **工具链** — TensorRT INT8、ONNX Runtime QDQ、PyTorch `torch.quantization`。各自如何表示 scale 与 zero-point。
* **对 kernel 的影响** — runtime 会分派到量化 kernel（如 INT8 GEMM、INT4 矩阵乘）；理解量化模型的 kernel 执行路径。

---

## 2. 量化感知训练（QAT）

* **伪量化** — 在前向传播中模拟量化；梯度用直通估计器。
* **准确率恢复** — 通过微调在量化后恢复准确率。
* **何时用 QAT、何时用 PTQ** — 准确率与投入的权衡；生产中的取舍。

---

## 3. 进阶格式与方法

* **精度** — GPU 上使用 FP16/BF16；CPU 与加速器上使用 INT4/INT8。
* **面向 LLM** — 用于 LLM 推理的 SmoothQuant、GPTQ、AWQ（概念及其在栈中的接入点）。

---

## 资源

* [TensorRT Quantization](https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/index.html#optimizing_int8_c)
* [PyTorch Quantization](https://pytorch.org/docs/stable/quantization.html)
* [tinygrad](https://github.com/tinygrad/tinygrad) — 搜索其中的量化 pass 与 kernel 支持。

---

## 项目

1. **用 TensorRT 跑 INT8** — 使用 TensorRT 将 CNN 量化为 INT8。报告准确率变化，以及与 FP32 相比的延迟/加速比。
2. **PTQ 与 QAT 对比** — 在同一模型与同一目标上比较 PTQ 和 QAT。分别记录准确率、延迟与工程投入。
3. **Kernel 路径** — 针对一个量化模型（如 TensorRT 或 ONNX Runtime），找出几个关键 layer（如 INT8 conv/GEMM）实际运行的 kernel。记录分派路径。

---

## 下一步

→ **[05 — 推理 runtime 与部署](../05-推理运行时与部署/Guide.md)** — 生产部署（TensorRT、ONNX Runtime、Triton server）与可度量的结果。


<details>
<summary>English original</summary>

**04 — Quantization & Low-Precision Inference**

<div class="course-identity auto-course" style="--course-accent: #475569; --course-accent-rgb: 71, 85, 105;" markdown="1">
<div class="course-identity__icon">QLPI</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Compiler Track</p>
<p class="course-identity__title">Specialized course identity for 04 — Quantization & Low-Precision Inference.</p>
<p class="course-identity__meta">Artifact: compiler/inference optimization · Measure: op count, memory, latency</p>
</div>
</div>


**Order:** Fourth. After you have kernels and compiler context (01–03), you add low-precision kernels and deployment.

**Role target:** DL Inference Optimization Engineer · **MTS Kernels** (INT8/INT4 kernels, quantization-aware implementations).

---

**Why this comes fourth**

Quantization (INT8, INT4, etc.) is a major lever for latency and throughput. It touches graph transforms, kernel implementations (integer matmul, quantized attention), and runtime integration. Doing 01–03 first lets you see where quantization plugs in and how it affects kernel choice and fusion.

---

**1. Post-training quantization (PTQ)**

* **INT8 / INT4** — Calibration (representative data), per-tensor vs per-channel scales, calibration datasets.
* **Tooling** — TensorRT INT8, ONNX Runtime QDQ, PyTorch `torch.quantization`. How each represents scales and zero-points.
* **Kernel impact** — Runtimes dispatch to quantized kernels (e.g. INT8 GEMM, INT4 matmul); understanding the kernel path for quantized models.

---

**2. Quantization-aware training (QAT)**

* **Fake quantization** — Simulate quantization in the forward pass; straight-through estimator for gradients.
* **Accuracy recovery** — Fine-tuning to recover accuracy after quantization.
* **When to use QAT vs PTQ** — Accuracy vs effort; production trade-offs.

---

**3. Advanced formats and methods**

* **Precision** — FP16/BF16 on GPUs; INT4/INT8 on CPUs and accelerators.
* **LLM-focused** — SmoothQuant, GPTQ, AWQ for LLM inference (concepts and where they hook into the stack).

---

**Resources**

* [TensorRT Quantization](https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/index.html#optimizing_int8_c)
* [PyTorch Quantization](https://pytorch.org/docs/stable/quantization.html)
* [tinygrad](https://github.com/tinygrad/tinygrad) — Search for quantization passes and kernel support.

---

**Projects**

1. **INT8 with TensorRT** — Quantize a CNN to INT8 using TensorRT. Report accuracy change and latency/speedup vs FP32.
2. **PTQ vs QAT** — Compare PTQ and QAT on the same model and target. Document accuracy, latency, and engineering effort for each.
3. **Kernel path** — For one quantized model (e.g. TensorRT or ONNX Runtime), identify which kernels run for a few key layers (e.g. INT8 conv/GEMM). Document the dispatch path.

---

**Next**

→ **[05 — Inference Runtimes and Deployment](../05-推理运行时与部署/Guide.md)** — Production deployment (TensorRT, ONNX Runtime, Triton server) and measurable outcomes.

</details>

---

> 原文：[`Phase 4 - Track C - DL Inference Optimization/04 - Quantization/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20C%20-%20DL%20Inference%20Optimization/04%20-%20Quantization/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
