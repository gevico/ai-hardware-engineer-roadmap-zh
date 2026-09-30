---
title: PyTorch — 行业标准深度学习框架
description: PyTorch — 行业标准深度学习框架
---

# PyTorch — 行业标准深度学习框架

<div class="course-identity auto-course" style="--course-accent: #0891b2; --course-accent-rgb: 8, 145, 178;" markdown="1">
<div class="course-identity__icon">PISD</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入解析 · AI 工作负载</p>
<p class="course-identity__title">PyTorch — 行业标准深度学习框架 的专属课程标识。</p>
<p class="course-identity__meta">产物：模型或工作负载研究 · 度量：准确率、延迟、内存、吞吐</p>
</div>
</div>


**父级：** [模块 2 — 深度学习框架](../Guide.md)

> *大多数模型所用的框架。你必须在此达到熟练，因为你在硬件上部署的每个模型都始于 PyTorch 代码。*

---

## 需要掌握的内容

| 概念 | 对硬件的意义 |
|---------|---------------------------|
| `torch.Tensor` | 加速器所处理的数据结构。shape、dtype、layout（contiguous、channels-last）。 |
| `nn.Module` | 模型如何组织。layer → 前向传播 → 计算图。 |
| Autograd（`loss.backward()`） | 生成训练硬件所执行的反向图。 |
| 数据加载（`DataLoader`） | CPU-GPU 流水线。若未与计算重叠即为瓶颈。 |
| `torch.onnx.export()` | 模型如何离开 PyTorch 并进入编译器/runtime 栈（阶段 4C）。 |
| `torch.compile()` | PyTorch 内置编译器（Inductor）。生成 Triton kernel。 |
| `torch.profiler` | 时间花在哪里？kernel 启动、内存拷贝、CPU 开销。 |
| 混合精度（`torch.cuda.amp`） | FP16/BF16 训练 —— 张量核心所加速的部分。 |
| 量化（`torch.ao.quantization`） | INT8 推理 —— L6 PE 阵列必须支持的部分。 |

## 项目

1. **训练 ResNet-18**：在 CIFAR-10 上从零开始。用 `torch.profiler` 做性能分析。找出最耗时的 3 个 op。
2. **导出为 ONNX。** 用 Netron 可视化计算图。统计 op 总数与参数总数。
3. **训练后量化**到 INT8。测量准确率下降与推理加速比。
4. 在 transformer block 上做 **`torch.compile()`**。对比 eager 与编译后的执行时间。

## 资源

- [PyTorch Tutorials](https://pytorch.org/tutorials/)
- *Deep Learning with PyTorch*（Stevens、Antiga、Viehmann）
- [PyTorch Profiler](https://pytorch.org/tutorials/recipes/recipes/profiler_recipe.html)


<details>
<summary>English original</summary>

**PyTorch — Industry-Standard Deep Learning Framework**

<div class="course-identity auto-course" style="--course-accent: #0891b2; --course-accent-rgb: 8, 145, 178;" markdown="1">
<div class="course-identity__icon">PISD</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · AI Workloads</p>
<p class="course-identity__title">Specialized course identity for PyTorch — Industry-Standard Deep Learning Framework.</p>
<p class="course-identity__meta">Artifact: model or workload study · Measure: accuracy, latency, memory, throughput</p>
</div>
</div>


**Parent:** [Module 2 — Deep Learning Frameworks](../Guide.md)

> *The framework most models are written in. You need fluency here because every model you deploy on hardware starts as PyTorch code.*

---

**What to Master**

| Concept | Why it matters for hardware |
|---------|---------------------------|
| `torch.Tensor` | The data structure accelerators process. Shape, dtype, layout (contiguous, channels-last). |
| `nn.Module` | How models are structured. Layers → forward pass → computational graph. |
| Autograd (`loss.backward()`) | Generates the backward graph that training hardware executes. |
| Data loading (`DataLoader`) | CPU-GPU pipeline. Bottleneck if not overlapped with compute. |
| `torch.onnx.export()` | How models leave PyTorch and enter the compiler/runtime stack (Phase 4C). |
| `torch.compile()` | PyTorch's built-in compiler (Inductor). Generates Triton kernels. |
| `torch.profiler` | Where is time spent? Kernel launches, memory copies, CPU overhead. |
| Mixed precision (`torch.cuda.amp`) | FP16/BF16 training — what tensor cores accelerate. |
| Quantization (`torch.ao.quantization`) | INT8 inference — what L6 PE arrays must support. |

**Projects**

1. **Train ResNet-18** on CIFAR-10 from scratch. Profile with `torch.profiler`. Identify top-3 time-consuming ops.
2. **Export to ONNX.** Visualize graph with Netron. Count total ops and parameters.
3. **Post-training quantization** to INT8. Measure accuracy drop and inference speedup.
4. **`torch.compile()`** on a transformer block. Compare eager vs compiled execution time.

**Resources**

- [PyTorch Tutorials](https://pytorch.org/tutorials/)
- *Deep Learning with PyTorch* (Stevens, Antiga, Viehmann)
- [PyTorch Profiler](https://pytorch.org/tutorials/recipes/recipes/profiler_recipe.html)

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/2. Deep Learning Frameworks/PyTorch/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/2.%20Deep%20Learning%20Frameworks/PyTorch/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
