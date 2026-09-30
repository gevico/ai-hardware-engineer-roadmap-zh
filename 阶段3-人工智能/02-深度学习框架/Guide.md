---
title: 模块 2 — 深度学习框架
description: 模块 2 — 深度学习框架
---

# 模块 2 — 深度学习框架

<div class="course-identity dl-frameworks" markdown="1">
<div class="course-identity__icon">FWK</div>
<div markdown="1">
<p class="course-identity__eyebrow">模块 2 · 深度学习框架</p>
<p class="course-identity__title">追踪 micrograd、PyTorch 和 tinygrad 如何将张量代码转化为可执行工作负载。</p>
<p class="course-identity__meta">产物：框架 trace · 度量：图结构、算子数量、内存、runtime</p>
</div>
</div>


**父级：** [阶段 3 — 人工智能](../Guide.md)

> *理解软件如何生成你的硬件必须运行的工作负载——从 autograd 到 GPU kernel。*

**前置要求：** 模块 1（神经网络——理解前向/反向 pass 计算的是什么）。

**Layer 映射：** **L1**（应用）——你使用框架构建模型。**L2**（编译器）——tinygrad 暴露了阶段 4C 教你构建的编译器流水线。

---

## 为什么需要一个专门的框架模块

模块 1 教你神经网络计算*什么*。本模块教你*如何*——将 `model(x)` 转化为 GPU kernel 启动的软件机制。理解这种机制至关重要，因为：

- **L2（编译器）：** 如果不理解框架产生什么（计算图、算子、张量），就无法构建 ML 编译器
- **L5（架构）：** 如果不了解哪些算子在真实工作负载中占主导，就无法设计加速器
- **L6（RTL）：** 如果不理解实际训练/推理的精度和数据流，就无法构建 PE 阵列

---

## 三框架心智模型

按顺序学习这三个框架。每个框架教授堆栈的不同层级。

| 框架 | 教授内容 | 规模 | 你的学习目标 |
|-----------|----------------|------|-------------------|
| **[micrograd](02-micrograd/Guide.md)** | autograd 如何工作——从头实现反向模式微分 | ~100 行 | 自己构建。在代码层面理解反向传播。 |
| **[PyTorch](01-PyTorch/Guide.md)** | 行业标准 API——张量、模块、优化器、数据加载 | 数百万行 | 流畅使用。训练模型、导出 ONNX、用 torch.profiler 进行性能分析。 |
| **[tinygrad](03-tinygrad/Guide.md)** | 编译器如何将张量算子转化为 GPU kernel——中间表示、调度器、后端 | ~10,000 行 | 阅读源码。从 `Tensor` 追踪到生成的 CUDA/OpenCL 代码。 |

```
micrograd          PyTorch              tinygrad
(education)        (production)         (hackable production)
    │                  │                     │
    ▼                  ▼                     ▼
 Autograd          Full API             Compiler pipeline
 from scratch      industry standard    IR → scheduler → codegen
    │                  │                     │
    └──────────────────┴─────────────────────┘
              Understanding grows left → right
```

---

## 1. micrograd — 从头实现 Autograd

**[micrograd](https://github.com/karpathy/micrograd)** 由 Andrej Karpathy 编写。约 100 行 Python 代码。实现：
- 一个追踪计算历史的 `Value` 类
- 反向模式自动微分（反向传播）
- 一个小型神经网络 API（`Neuron`、`Layer`、`MLP`）

**你将构建：**
```python
from micrograd.engine import Value

# Forward pass
x = Value(2.0)
y = Value(3.0)
z = x * y + y ** 2  # z = 2*3 + 9 = 15

# Backward pass (autograd)
z.backward()
print(x.grad)  # dz/dx = y = 3.0
print(y.grad)  # dz/dy = x + 2y = 2 + 6 = 8.0
```

**为什么对硬件重要：** 每个训练加速器都必须实现这个反向 pass。理解计算图和梯度流告诉你硬件必须支持哪些内存访问模式和操作。

**项目：** 从头实现 micrograd（不要复制——自己敲代码）。在 2D 分类数据集上训练多层感知机。可视化计算图。

---


<details>
<summary>English original</summary>

**Module 2 — Deep Learning Frameworks**

<div class="course-identity dl-frameworks" markdown="1">
<div class="course-identity__icon">FWK</div>
<div markdown="1">
<p class="course-identity__eyebrow">Module 2 · Deep Learning Frameworks</p>
<p class="course-identity__title">Trace how micrograd, PyTorch, and tinygrad turn tensor code into executable workloads.</p>
<p class="course-identity__meta">Artifact: framework trace · Measure: graph shape, op count, memory, runtime</p>
</div>
</div>


**Parent:** [Phase 3 — Artificial Intelligence](../Guide.md)

> *Understand how software generates the workloads your hardware must run — from autograd to GPU kernels.*

**Prerequisites:** Module 1 (Neural Networks — understand what a forward/backward pass computes).

**Layer mapping:** **L1** (Application) — you use frameworks to build models. **L2** (Compiler) — tinygrad exposes the compiler pipeline that Phase 4C teaches you to build.

---

**Why a Dedicated Frameworks Module**

Module 1 teaches you *what* neural networks compute. This module teaches you *how* — the software machinery that turns `model(x)` into GPU kernel launches. Understanding this machinery is essential because:

- **L2 (Compiler):** You can't build an ML compiler without understanding what frameworks produce (computational graphs, ops, tensors)
- **L5 (Architecture):** You can't design an accelerator without knowing which ops dominate real workloads
- **L6 (RTL):** You can't build a PE array without understanding the precision and data flow of actual training/inference

---

**Three-Framework Mental Model**

Study these three frameworks in order. Each teaches a different level of the stack.

| Framework | What it teaches | Size | Your learning goal |
|-----------|----------------|------|-------------------|
| **[micrograd](02-micrograd/Guide.md)** | How autograd works — reverse-mode differentiation from scratch | ~100 lines | Build it yourself. Understand backprop at the code level. |
| **[PyTorch](01-PyTorch/Guide.md)** | Industry-standard API — tensors, modules, optimizers, data loading | Millions of lines | Use it fluently. Train models, export ONNX, profile with torch.profiler. |
| **[tinygrad](03-tinygrad/Guide.md)** | How a compiler turns tensor ops into GPU kernels — IR, scheduler, backends | ~10,000 lines | Read the source. Trace from `Tensor` to generated CUDA/OpenCL code. |

```
micrograd          PyTorch              tinygrad
(education)        (production)         (hackable production)
    │                  │                     │
    ▼                  ▼                     ▼
 Autograd          Full API             Compiler pipeline
 from scratch      industry standard    IR → scheduler → codegen
    │                  │                     │
    └──────────────────┴─────────────────────┘
              Understanding grows left → right
```

---

**1. micrograd — Autograd from Scratch**

**[micrograd](https://github.com/karpathy/micrograd)** by Andrej Karpathy. ~100 lines of Python. Implements:
- A `Value` class that tracks computation history
- Reverse-mode automatic differentiation (backpropagation)
- A tiny neural network API (`Neuron`, `Layer`, `MLP`)

**What you'll build:**
```python
from micrograd.engine import Value

# Forward pass
x = Value(2.0)
y = Value(3.0)
z = x * y + y ** 2  # z = 2*3 + 9 = 15

# Backward pass (autograd)
z.backward()
print(x.grad)  # dz/dx = y = 3.0
print(y.grad)  # dz/dy = x + 2y = 2 + 6 = 8.0
```

**Why it matters for hardware:** Every training accelerator must implement this backward pass. Understanding the computation graph and gradient flow tells you what memory access patterns and operations the hardware must support.

**Project:** Implement micrograd from scratch (don't copy — type it yourself). Train an MLP on a 2D classification dataset. Visualize the computation graph.

---

</details>

## 2. PyTorch — 行业标准

**[PyTorch](https://pytorch.org/)** 是大多数模型编写所用的框架。你需要熟练掌握它，因为：
- 部署到硬件上的模型都是用 PyTorch 写的
- ONNX 导出源自 PyTorch（`torch.onnx.export`）
- `torch.compile`（Inductor）是一个生产级 ML 编译器
- 性能剖析工具（`torch.profiler`、Nsight）能告诉你时间花在哪里

**需要掌握的关键概念：**

| 概念 | 为何对硬件重要 |
|---------|---------------------------|
| `torch.Tensor` | 加速器所处理的数据结构。shape、dtype、layout（contiguous、channels-last）。 |
| `nn.Module` | 模型如何组织。layer → 前向传播 → 计算图。 |
| Autograd（`loss.backward()`） | 生成训练硬件所执行的反向图。 |
| 数据加载（`DataLoader`） | CPU-GPU 流水线。如果不与计算重叠，就会成为瓶颈。 |
| `torch.onnx.export()` | 模型如何离开 PyTorch 进入编译器/runtime 栈（阶段 4C）。 |
| `torch.compile()` | PyTorch 内置的编译器（Inductor）。生成 Triton kernel。与阶段 4C 相关联。 |
| `torch.profiler` | 时间花在哪里？kernel 启动、内存拷贝、CPU 开销。 |
| 混合精度（`torch.cuda.amp`） | FP16/BF16 训练——即 tensor cores 所加速的内容。 |
| 量化（`torch.ao.quantization`） | INT8 推理——L6 PE 阵列必须支持的内容。 |

**项目：**
1. 在 CIFAR-10 上从零训练一个 CNN（ResNet-18）。用 `torch.profiler` 做性能剖析。找出耗时前三的操作。
2. 将训练好的模型导出为 ONNX。用 Netron 可视化计算图。统计算子总数与参数总数。
3. 应用训练后量化（PTQ）到 INT8。测量准确率下降与 CPU 上的推理加速。
4. 在一个 Transformer block 上使用 `torch.compile()`。比较 eager 与编译执行的耗时。

---

## 3. tinygrad — 可魔改的编译器

**[tinygrad](https://github.com/tinygrad/tinygrad)** 是一个极简 DL 框架（约 10K 行），用可读的 Python 暴露了整条编译器流水线。它是理解 `loss.backward()` 与实际运行的 GPU kernel 之间发生了什么的最佳代码库。

**为什么 tinygrad 对这份路线图有独特价值：**
- 它是 openpilot 内部的推理引擎（阶段 5E）
- 它暴露了 IR、调度器和代码生成，也就是阶段 4C 教你构建的东西
- 你可以添加自定义后端（阶段 4C §7）——面向你自己的加速器
- 它可运行在 CUDA、OpenCL、Metal、LLVM 以及自定义目标上

**关键概念：**

| 概念 | 它教会你什么 | 与栈的关联 |
|---------|----------------|-------------------|
| 惰性求值 | 在 `.realize()` 之前什么都不运行 | L2：编译器决定何时执行 |
| 3 种操作类型 | Elementwise、Reduce、Movement（共 25 个原语） | L5：PE 阵列必须支持的内容 |
| ShapeTracker | 零拷贝 reshape 与 transpose | L2：内存布局优化 |
| UOp IR | 代码生成之前的中间表示 | L2：与 MLIR/TVM IR 相同的概念 |
| BEAM search | 探索融合选择以最小化 runtime | L2：用于 kernel 优化的自动调优 |
| 后端 | 同一份 IR 如何生成 CUDA、OpenCL 或 LLVM 代码 | L2：多目标编译 |

**项目：**
1. 在 tinygrad 中追踪一次矩阵乘：`Tensor` → lazy buffer → scheduled ops → 生成的 CUDA kernel。记录每一步。
2. 用 `DEBUG=4` 运行一个小模型，查看生成的 kernel。统计 kernel 启动次数。
3. 用 `BEAM=3` 运行，并对比 kernel 数量与延迟（相对于 `BEAM=0`）。
4. （进阶）添加一个极简日志后端，打印每次 kernel 启动——验证哪些算子发生了融合。

**深入钻研：** 完整的 tinygrad 学习路径（11 部分，7 个项目）在 [阶段 5E — 自动驾驶 / tinygrad](../../阶段5-高级专题与专精/05-方向E-自动驾驶/03-tinygrad推理/Guide.md)。

---

## 框架如何与路线图的其余部分衔接

| 框架技能 | 通向何处 |
|----------------|---------------|
| micrograd autograd | 阶段 4C：理解编译器必须微分什么 |
| PyTorch 模型导出（ONNX） | 阶段 4C §1：图 IR 作为编译器输入 |
| PyTorch 量化 | 阶段 4C 第 2 部分 §4：量化 pass |
| `torch.compile`（Inductor） | 阶段 4C §5：生产级 ML 编译器流水线 |
| tinygrad IR 与调度器 | 阶段 4C §5：BEAM search、融合策略 |
| tinygrad 后端 | 阶段 4C §7：面向你的加速器的自定义后端 |
| PyTorch 性能剖析 | 阶段 4C 第 2 部分 §1：图/算子优化 |

---


<details>
<summary>English original</summary>

**2. PyTorch — Industry Standard**

**[PyTorch](https://pytorch.org/)** is the framework most models are written in. You need fluency here because:
- Models you deploy on hardware are written in PyTorch
- ONNX export comes from PyTorch (`torch.onnx.export`)
- `torch.compile` (Inductor) is a production ML compiler
- Profiling tools (`torch.profiler`, Nsight) show you where time is spent

**Key concepts to master:**

| Concept | Why it matters for hardware |
|---------|---------------------------|
| `torch.Tensor` | The data structure accelerators process. Shape, dtype, layout (contiguous, channels-last). |
| `nn.Module` | How models are structured. Layers → forward pass → computational graph. |
| Autograd (`loss.backward()`) | Generates the backward graph that training hardware executes. |
| Data loading (`DataLoader`) | CPU-GPU pipeline. Bottleneck if not overlapped with compute. |
| `torch.onnx.export()` | How models leave PyTorch and enter the compiler/runtime stack (Phase 4C). |
| `torch.compile()` | PyTorch's built-in compiler (Inductor). Generates Triton kernels. Connection to Phase 4C. |
| `torch.profiler` | Where is time spent? Kernel launches, memory copies, CPU overhead. |
| Mixed precision (`torch.cuda.amp`) | FP16/BF16 training — what tensor cores accelerate. |
| Quantization (`torch.ao.quantization`) | INT8 inference — what L6 PE arrays must support. |

**Projects:**
1. Train a CNN (ResNet-18) on CIFAR-10 from scratch. Profile with `torch.profiler`. Identify the top-3 time-consuming operations.
2. Export the trained model to ONNX. Visualize the graph with Netron. Count the total number of ops and parameters.
3. Apply post-training quantization (PTQ) to INT8. Measure accuracy drop and inference speedup on CPU.
4. Use `torch.compile()` on a transformer block. Compare eager vs compiled execution time.

---

**3. tinygrad — The Hackable Compiler**

**[tinygrad](https://github.com/tinygrad/tinygrad)** is a minimal DL framework (~10K lines) that exposes the entire compiler pipeline in readable Python. It's the ideal codebase for understanding what happens between `loss.backward()` and the GPU kernel that actually runs.

**Why tinygrad is uniquely valuable for this roadmap:**
- It's the inference engine inside openpilot (Phase 5E)
- It exposes the IR, scheduler, and code generation that Phase 4C teaches you to build
- You can add a custom backend (Phase 4C §7) — targeting your own accelerator
- It runs on CUDA, OpenCL, Metal, LLVM, and custom targets

**Key concepts:**

| Concept | What it teaches | Connection to stack |
|---------|----------------|-------------------|
| Lazy evaluation | Nothing runs until `.realize()` | L2: compiler decides when to execute |
| 3 operation types | Elementwise, Reduce, Movement (25 primitives total) | L5: what the PE array must support |
| ShapeTracker | Zero-copy reshapes and transposes | L2: memory layout optimization |
| UOp IR | The intermediate representation before code generation | L2: same concept as MLIR/TVM IR |
| BEAM search | Explores fusion choices to minimize runtime | L2: auto-tuning for kernel optimization |
| Backends | How the same IR generates CUDA, OpenCL, or LLVM code | L2: multi-target compilation |

**Projects:**
1. Trace a matmul through tinygrad: `Tensor` → lazy buffer → scheduled ops → generated CUDA kernel. Document every step.
2. Run a small model with `DEBUG=4` to see the generated kernels. Count the number of kernel launches.
3. Run with `BEAM=3` and compare kernel count and latency vs `BEAM=0`.
4. (Advanced) Add a minimal logging backend that prints each kernel launch — verify which ops fuse.

**Deep dive:** The full tinygrad learning path (11 parts, 7 projects) is in [Phase 5E — Autonomous Vehicles / tinygrad](../../阶段5-高级专题与专精/05-方向E-自动驾驶/03-tinygrad推理/Guide.md).

---

**How Frameworks Connect to the Rest of the Roadmap**

| Framework skill | Where it leads |
|----------------|---------------|
| micrograd autograd | Phase 4C: understand what compiler must differentiate |
| PyTorch model export (ONNX) | Phase 4C §1: graph IR as compiler input |
| PyTorch quantization | Phase 4C Part 2 §4: quantization passes |
| `torch.compile` (Inductor) | Phase 4C §5: production ML compiler pipeline |
| tinygrad IR and scheduler | Phase 4C §5: BEAM search, fusion strategies |
| tinygrad backends | Phase 4C §7: custom backend for your accelerator |
| PyTorch profiling | Phase 4C Part 2 §1: graph/operator optimization |

---

</details>

## 资源

| 资源 | 涵盖内容 |
|----------|---------------|
| [Andrej Karpathy — micrograd 视频](https://www.youtube.com/watch?v=VMj-3S1tku0) | 从零构建 autograd（2 小时） |
| [PyTorch 教程](https://pytorch.org/tutorials/) | 官方入门 → 进阶教程 |
| [tinygrad GitHub](https://github.com/tinygrad/tinygrad) | 源代码——去读它 |
| [tinygrad Discord](https://discord.gg/tinygrad) | 社区、贡献、求助 |
| *Deep Learning with PyTorch*（Stevens、Antiga、Viehmann） | 全面的 PyTorch 书 |

---

## 下一章

→ [**模块 3 — 计算机视觉**](../03-硬件与边缘AI/01-计算机视觉/Guide.md) — 驱动边缘 AI 与自主系统的感知工作负载。


<details>
<summary>English original</summary>

**Resources**

| Resource | What it covers |
|----------|---------------|
| [Andrej Karpathy — micrograd video](https://www.youtube.com/watch?v=VMj-3S1tku0) | Build autograd from scratch (2 hours) |
| [PyTorch Tutorials](https://pytorch.org/tutorials/) | Official beginner → advanced tutorials |
| [tinygrad GitHub](https://github.com/tinygrad/tinygrad) | Source code — read it |
| [tinygrad Discord](https://discord.gg/tinygrad) | Community, contributions, help |
| *Deep Learning with PyTorch* (Stevens, Antiga, Viehmann) | Comprehensive PyTorch book |

---

**Next**

→ [**Module 3 — Computer Vision**](../03-硬件与边缘AI/01-计算机视觉/Guide.md) — the perception workloads that drive edge AI and autonomous systems.

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/2. Deep Learning Frameworks/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/2.%20Deep%20Learning%20Frameworks/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
