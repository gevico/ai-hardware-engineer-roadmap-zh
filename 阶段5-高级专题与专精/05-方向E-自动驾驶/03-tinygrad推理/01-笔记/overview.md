---
title: tinygrad: 极简深度学习框架
description: tinygrad: 极简深度学习框架
---

# tinygrad: 极简深度学习框架

## 概述

tinygrad 是一个轻量级神经网络框架，由 George Hotz (geohot) 创建，由 tiny corp 维护。它定位在 PyTorch 和 micrograd 之间，在不牺牲功能的前提下提供简洁性。

**链接：**
- 主页：https://tinygrad.org
- GitHub：https://github.com/tinygrad/tinygrad
- 文档：https://tinygrad.github.io/tinygrad/quickstart/
- Discord：https://discord.gg/tinygrad

## 核心哲学

tinygrad 将复杂的神经网络分解为仅 3 种操作类型：

### 1. ElementwiseOps
UnaryOps、BinaryOps 和 TernaryOps，对 1-3 个张量进行逐元素操作
- **UnaryOps**（1 个输入）：SQRT, LOG2, EXP2, SIN, NEG, RECIP, CAST
- **BinaryOps**（2 个输入）：ADD, MUL, SUB, DIV, MAX, MOD, CMPLT
- **TernaryOps**（3 个输入）：WHERE, MULACC

### 2. ReduceOps
对一个张量进行操作并返回一个更小的张量
- 示例：SUM, MAX

### 3. MovementOps
虚拟操作，用于移动数据，通过 ShapeTracker 实现无复制
- 示例：RESHAPE, PERMUTE, EXPAND 等

**注意：** 没有用于 CONV 或 MATMUL 的原语算子——它们由基本操作构建而成！

## 关键特性

- **极简** — 最容易添加新加速器的框架
- **惰性求值** — 所有张量都是惰性的，从而实现激进的操作融合
- **自定义 kernel 编译** — 为每个操作编译自定义 kernel
- **完整训练支持** — 使用自动微分进行前向和反向传播
- **可修改** — 整个编译器和 IR 可见且可修改
- **多后端** — 支持 NVIDIA、AMD 和其他加速器

## 性能

tinygrad 的目标是在 1 块 NVIDIA GPU 上，对常见 ML 论文比 PyTorch 快 2 倍。

速度优势：
1. 为每个操作自定义 kernel 编译
2. 通过惰性张量进行激进的操作融合
3. 简化 10 倍以上的后端使优化更具影响力

## 安装

```bash
pip install tinygrad
```

## 基本用法

```python
from tinygrad import Tensor

# Create tensors
t1 = Tensor([1, 2, 3, 4, 5])
t2 = Tensor([2, 3, 4, 5, 6])

# Operations (similar to PyTorch)
result = t1 + t2
result = t1 * t2

# Lazy evaluation — computation happens when .realize() is called
result.realize()
```

## 实际应用

tinygrad 被用于 [Openpilot](https://github.com/commaai/openpilot) (comma.ai ADAS) 中，在 Snapdragon 845 GPU 上运行驾驶模型，替代 SNPE，具有以下优势：
- 更好的性能
- 支持加载 ONNX 文件
- 支持训练
- 支持 attention 机制

## tinygrad 与厂商 SDK 对比：SNPE 案例研究

Qualcomm 并非无所作为——他们的动机与 tinygrad 的根本不同。结果就是一个保守、封闭、面向生产的 SDK（SNPE），而不是一个对黑客友好、追求最大性能的开放技术栈。

### Snapdragon 845 上的 2× 性能差距

tinygrad 在 Snapdragon 845 上为 openpilot 的驾驶模型实现了相对于 SNPE（Qualcomm 自己的库）约 **2× 的加速**。如何做到的：

- tinygrad 由那些只关心一件事的人优化：从少数目标模型和 GPU 中榨取最大性能——即使这意味着依赖未文档化的技巧或脆弱的假设（例如，Adreno 如何处理图像纹理、分块、缓存行为）。
- SNPE 必须用一个二进制 SDK 支持众多客户、模型、量化方案和产品周期。更多的抽象、更多的安全检查，对“奇怪但高性能”的形状更少激进的专用 kernel。对 OEM 来说足够好，但没有针对某一个开源 ADAS 项目进行优化。

### 为什么 Qualcomm 不像 tinygrad 那样打造 SNPE

| 约束 | Qualcomm (SNPE) | tinygrad |
|-----------|----------------|----------|
| **风险状况** | 面向手机/汽车销售，带有 SLA——人脸解锁或相机流水线出现回归是业务问题 | 可以破坏 main 之后再修复 |
| **支持矩阵** | 必须在数十种 SoC、操作系统版本、模型类型上运行良好 | “在这块 GPU 和这些模型上快——其他都尽力而为” |
| **硬件文档** | 底层 Hexagon/HTP 和 Adreno 细节受 NDA 保护；甚至内部团队也被 API 稳定性和 OEM 法律约束所限制 | 通过 OpenCL/GL/Vulkan 逆向工程，激进实验 |
| **商业模式** | 优先销售芯片 + 为大客户提供稳定 SDK | 优先性能；没有需要保护的 OEM 合同 |
| **战略利益** | 发布一个绕过 SNPE 的开放“锐边”框架会削弱他们的 SDK 故事，并产生他们不想要的支持期望 | 自由发布一切 |

从 Qualcomm 的角度来看，在某些设置下 SNPE 比 tinygrad 慢是**可接受的**，只要：
- 对 OEM 的用例来说足够快
- 稳定、有支持，并且不会每季度都出问题
- 有助于销售更多基于 Snapdragon 的设备


<details>
<summary>English original</summary>

**Tinygrad: A Minimalist Deep Learning Framework**

**Overview**

Tinygrad is a lightweight neural network framework created by George Hotz (geohot) and maintained by tiny corp. It positions itself between PyTorch and micrograd, offering simplicity without sacrificing functionality.

**Links:**
- Homepage: https://tinygrad.org
- GitHub: https://github.com/tinygrad/tinygrad
- Documentation: https://tinygrad.github.io/tinygrad/quickstart/
- Discord: https://discord.gg/tinygrad

**Core Philosophy**

Tinygrad breaks down complex neural networks into just 3 operation types:

**1. ElementwiseOps**
UnaryOps, BinaryOps, and TernaryOps that operate on 1-3 tensors elementwise
- **UnaryOps** (1 input): SQRT, LOG2, EXP2, SIN, NEG, RECIP, CAST
- **BinaryOps** (2 inputs): ADD, MUL, SUB, DIV, MAX, MOD, CMPLT
- **TernaryOps** (3 inputs): WHERE, MULACC

**2. ReduceOps**
Operate on one tensor and return a smaller tensor
- Examples: SUM, MAX

**3. MovementOps**
Virtual ops that move data around, copy-free with ShapeTracker
- Examples: RESHAPE, PERMUTE, EXPAND, etc.

**Note:** No primitive operators for CONV or MATMUL — these are built from basic operations!

**Key Features**

- **Extreme simplicity** — Easiest framework to add new accelerators to
- **Lazy evaluation** — All tensors are lazy, enabling aggressive operation fusion
- **Custom kernel compilation** — Compiles a custom kernel for every operation
- **Full training support** — Forward and backward passes with autodiff
- **Hackable** — Entire compiler and IR are visible and modifiable
- **Multi-backend** — Supports NVIDIA, AMD, and other accelerators

**Performance**

Tinygrad aims to be 2x faster than PyTorch for common ML papers on 1 NVIDIA GPU.

Speed advantages:
1. Custom kernel compilation for each operation
2. Aggressive operation fusion through lazy tensors
3. 10x+ simpler backend makes optimizations more impactful

**Installation**

```bash
pip install tinygrad
```

**Basic Usage**

```python
from tinygrad import Tensor

# Create tensors
t1 = Tensor([1, 2, 3, 4, 5])
t2 = Tensor([2, 3, 4, 5, 6])

# Operations (similar to PyTorch)
result = t1 + t2
result = t1 * t2

# Lazy evaluation — computation happens when .realize() is called
result.realize()
```

**Real-World Usage**

Tinygrad is used in [Openpilot](https://github.com/commaai/openpilot) (comma.ai ADAS) to run the driving model on Snapdragon 845 GPU, replacing SNPE with:
- Better performance
- ONNX file loading support
- Training support
- Attention mechanism support

**tinygrad vs Vendor SDKs: The SNPE Case Study**

Qualcomm isn't asleep — their incentives are just fundamentally different from tinygrad's. The result is a conservative, closed, production-oriented SDK (SNPE) instead of a hacker-friendly, maximum-performance open stack.

**The 2× Performance Gap on Snapdragon 845**

tinygrad achieved roughly **2× speedup vs SNPE** (Qualcomm's own library) for openpilot's driving model on the Snapdragon 845. How:

- tinygrad is optimized by people who only care about one thing: wringing maximum performance out of a few target models and GPUs — even if that means relying on undocumented tricks or brittle assumptions (e.g., how Adreno handles image textures, tiling, cache behavior).
- SNPE has to support many customers, models, quantization schemes, and product cycles with a single binary SDK. More abstraction, more safety checks, less aggressively specialized kernels for "weird but high-performing" shapes. Good enough for OEMs, not optimized for one open-source ADAS project.

**Why Qualcomm Doesn't Make SNPE Like tinygrad**

| Constraint | Qualcomm (SNPE) | tinygrad |
|-----------|----------------|----------|
| **Risk profile** | Sells into phones/cars with SLAs — a regression in face unlock or camera pipeline is a business problem | Can break main and fix it later |
| **Support matrix** | Must run well on dozens of SoCs, OS versions, model types | "Fast on this GPU and these models — everything else is best-effort" |
| **Hardware docs** | Low-level Hexagon/HTP and Adreno details are under NDA; even internal teams are boxed in by API stability and OEM legal constraints | Reverse-engineer via OpenCL/GL/Vulkan, experiment aggressively |
| **Business model** | Priority is selling silicon + providing a stable SDK for big customers | Priority is performance; no OEM contracts to protect |
| **Strategic interest** | Shipping an open "sharp-edges" framework that bypasses SNPE undercuts their SDK story and creates support expectations they don't want | Freely publish everything |

From Qualcomm's perspective, SNPE being slower than tinygrad in some setups is **acceptable** as long as:
- It's fast enough for OEMs' use cases
- It's stable, supported, and doesn't break every quarter
- It helps sell more Snapdragon-based devices

</details>

### 这对开源 ML 栈意味着什么

- 这一性能差距证明：**开源、硬件感知的栈只要被允许做专门优化并快速迭代，就能在厂商自家的硬件上击败厂商 SDK**。
- Qualcomm 在这一方向上并未积极竞争，这就给独立项目留出了在 Snapdragon 上定义「同类最佳」性能的空间——openpilot + tinygrad 做的正是这件事。
- 长期看，这种压力会推动厂商走向以下两者之一：
  - 暴露更多底层可调项（更好的 Vulkan/CL、性能计数器、调度提示），或
  - 发布自家高性能实验性栈，同时把 SNPE 保留为保守的默认选项。

### 系统架构层面的启示

这是经典的**「面向大众市场的厂商 SDK vs. 面向狭窄领域的手工调优栈」**故事：

```
Vendor SDK (SNPE):
  Optimize for: stability, broad support, OEM contracts
  Accept tradeoff: 2× slower on specific workloads
  Target: millions of devices, dozens of use cases

tinygrad on Adreno:
  Optimize for: one GPU, one model, maximum FLOP/s
  Accept tradeoff: brittle, undocumented, may break
  Target: openpilot's driving model on 845
```

tinygrad 中的 QCOM 后端（`DEVICE=QCOM`）就是直接证据：tinygrad 提供了一个面向 Adreno 的一流 Qualcomm GPU 后端——而 Qualcomm 不会通过 SNPE 为你做这件事，因为不存在既符合 NDA 又安全的方式来暴露同样的底层调优能力。

## 同样的规律在 Jetson Orin Nano 8GB 上

「厂商 SDK vs 黑客栈」这一规律同样适用于 Orin Nano——但 NVIDIA 已比 Qualcomm 更接近 tinygrad 的理念，这显著改变了局势。

### Orin 上的官方路径 vs 开放栈

NVIDIA 的官方路径是 **TensorRT + CUDA/cuDNN**，它和 SNPE 一样，是为了跨模型、跨客户的稳定性而设计，而不是为了某个项目的绝对最高性能。关键区别在于：**NVIDIA 已经暴露了非常底层、文档完善的 CUDA 和 Tensor Core API**。开放栈（tinygrad、PyTorch 自定义 kernel、Triton）可以通过手工调优 kernel、融合与内存布局，在特定工作负载上非常接近甚至超过 TensorRT。

### 为什么 Orin 的体感优于 Snapdragon

| 方面 | Snapdragon 845 (Adreno) | Jetson Orin Nano 8GB (Ampere) |
|--------|------------------------|-------------------------------|
| **工具链** | 不透明的 CL/Vulkan/HTP 栈，文档缺失 | 完整 CUDA 工具链、Nsight 性能分析器、稳定的 ISA 视图 |
| **硬件文档** | 最有用的细节都在 NDA 之下 | Tensor Core 布局、warp 调度公开有文档 |
| **优化路径** | 逆向工程分块/缓存行为 | 直接手工调优矩阵乘、融合卷积、Tensor Core 路径 |
| **性能分析器** | 有限，受厂商限制 | Nsight Systems + Nsight Compute 暴露周期级细节 |
| **上限** | 没有内部文档时很快撞到硬件限制 | 高得多——你不是在跟平台较劲 |
| **tinygrad 后端** | `DEVICE=QCOM`——逆向工程而来 | `DEVICE=NV`——一流 CUDA 路径 |

在 Snapdragon 上你要对抗的是不透明的栈；在 Orin 上你要对抗的是数学本身——这要好得多。

### 对 ADAS 开发的实用结论

```
Qualcomm 845 with tinygrad:
  2× faster than SNPE
  Achieved by: undocumented texture/tiling tricks, reverse-engineered cache behavior
  Cost: brittle, may break on SDK updates

Orin Nano 8GB with tinygrad (DEVICE=NV):
  Can beat generic TensorRT on your exact ADAS models
  Achieved by: custom kernel fusion, tensor-core paths, graph-specific scheduling
  Cost: kernel writing effort — but no reverse-engineering needed
```

如果你愿意编写或调优 kernel，**Orin Nano 8GB 是一个极佳的 tinygrad 目标平台**。同样的原则依然成立——一个小而狠的开放栈能针对你的具体模型击败通用的 TensorRT 路径——但 NVIDIA 给了你利用它的工具和可见性，因此你会把更多时间花在优化上，而不是跟平台较劲。

### Orin 上的性能收益来自哪里

| 技术 | 通用 TensorRT | tinygrad / 自定义 | 收益 |
|-----------|-----------------|-------------------|-----|
| Kernel 融合 | 逐层（保守） | 通过 lazy 调度器做跨算子融合 | 更少内存带宽 |
| Tensor Core 布局 | 自动（可能不匹配你的形状） | 手工挑选 `m×n×k` 分块尺寸 | 更好利用率 |
| 内存布局 | NCHW/NHWC 自动选择 | 按层选择以获得缓存局部性 | 更少停顿 |
| 图调度 | 固定的 TRT 构建期计划 | 动态 lazy 图，runtime 重排序 | 更好批处理 |
| DLA offload | 手动、粗粒度 | 可更细粒度切分算子 | 更好的功耗/性能 |

---

## 支持的设备

Tinygrad 支持多种后端：
- **NV/CUDA**：NVIDIA GPU
- **AMD**：RDNA2+ GPU
- **METAL**：Apple M1+ 设备
- **QCOM**：Qualcomm 6xx 系列 GPU
- **OpenCL**：任何 OpenCL 2.0 设备
- **CPU**：使用 clang/LLVM 的回退路径
- **WEBGPU**：基于浏览器，经由 Dawn

## Tinygrad 与 PyTorch 的对比


<details>
<summary>English original</summary>

**What This Means for Open-Source ML Stacks**

- The performance gap is proof that **open-source, hardware-aware stacks can beat vendor SDKs on the vendor's own hardware** when allowed to specialize and iterate quickly.
- Qualcomm not competing aggressively on this front leaves space for independent projects to define "best-in-class" performance on Snapdragon — exactly what openpilot + tinygrad did.
- Long-term, this pressure pushes vendors toward either:
  - Exposing more low-level knobs (better Vulkan/CL, perf counters, scheduling hints), or
  - Shipping their own high-performance experimental stacks while keeping SNPE as the conservative default.

**The Systems Architecture Lesson**

This is the classic **"vendor SDK for mass market vs. hand-tuned stack for a narrow domain"** story:

```
Vendor SDK (SNPE):
  Optimize for: stability, broad support, OEM contracts
  Accept tradeoff: 2× slower on specific workloads
  Target: millions of devices, dozens of use cases

tinygrad on Adreno:
  Optimize for: one GPU, one model, maximum FLOP/s
  Accept tradeoff: brittle, undocumented, may break
  Target: openpilot's driving model on 845
```

The QCOM backend in tinygrad (`DEVICE=QCOM`) is direct evidence of this: tinygrad ships a first-class Qualcomm GPU backend targeting Adreno — something Qualcomm won't do for you via SNPE because there's no NDA-safe way to expose the same low-level tuning.

**The Same Rule on Jetson Orin Nano 8GB**

The "vendor SDK vs hacker stack" rule applies equally to Orin Nano — but NVIDIA is already much closer to tinygrad's philosophy than Qualcomm is, which changes the dynamics significantly.

**Official Path vs Open Stack on Orin**

NVIDIA's official path is **TensorRT + CUDA/cuDNN**, which — like SNPE — is designed for stability across models and customers, not for one project's absolute maximum performance. The critical difference: **NVIDIA already exposes very low-level, well-documented CUDA and tensor core APIs**. An open stack (tinygrad, PyTorch custom kernels, Triton) can get very close to or even beat TensorRT on specific workloads by hand-tuning kernels, fusion, and memory layout.

**Why Orin Feels Better Than Snapdragon**

| Aspect | Snapdragon 845 (Adreno) | Jetson Orin Nano 8GB (Ampere) |
|--------|------------------------|-------------------------------|
| **Tooling** | Opaque CL/Vulkan/HTP stack, missing docs | Full CUDA toolchain, Nsight profilers, stable ISA view |
| **Hardware docs** | Most useful details under NDA | Tensor core layout, warp scheduling publicly documented |
| **Optimization path** | Reverse-engineer tiling/cache behavior | Hand-tune matmuls, fused convs, tensor-core paths directly |
| **Profiler** | Limited, vendor-gated | Nsight Systems + Nsight Compute expose cycle-level detail |
| **Ceiling** | Hit hardware limits quickly without inside docs | Much higher — you're not fighting the platform |
| **tinygrad backend** | `DEVICE=QCOM` — reverse-engineered | `DEVICE=NV` — first-class CUDA path |

On Snapdragon you fight opaque stacks; on Orin you fight the actual math — which is a much better place to be.

**Practical Takeaway for ADAS Development**

```
Qualcomm 845 with tinygrad:
  2× faster than SNPE
  Achieved by: undocumented texture/tiling tricks, reverse-engineered cache behavior
  Cost: brittle, may break on SDK updates

Orin Nano 8GB with tinygrad (DEVICE=NV):
  Can beat generic TensorRT on your exact ADAS models
  Achieved by: custom kernel fusion, tensor-core paths, graph-specific scheduling
  Cost: kernel writing effort — but no reverse-engineering needed
```

If you're willing to write or tune kernels, **Orin Nano 8GB is an excellent tinygrad target**. The same principle applies — a small, ruthless open stack can beat the generic TensorRT path for your exact models — but NVIDIA gives you the tools and visibility to exploit it, so you spend more time optimizing and less time fighting the platform.

**Where the Performance Wins Come From on Orin**

| Technique | Generic TensorRT | tinygrad / custom | Win |
|-----------|-----------------|-------------------|-----|
| Kernel fusion | Layer-by-layer (conservative) | Cross-op fusion via lazy scheduler | Less memory bandwidth |
| Tensor core layout | Auto (may not match your shape) | Hand-pick `m×n×k` tile sizes | Better utilization |
| Memory layout | NCHW/NHWC auto-selection | Choose per-layer for cache locality | Fewer stalls |
| Graph scheduling | Fixed TRT build-time plan | Dynamic lazy graph, reorder at runtime | Better batching |
| DLA offload | Manual, coarse-grained | Can slice ops more finely | Better power/perf |

---

**Supported Devices**

Tinygrad supports multiple backends:
- **NV/CUDA**: NVIDIA GPUs
- **AMD**: RDNA2+ GPUs
- **METAL**: Apple M1+ devices
- **QCOM**: Qualcomm 6xx series GPUs
- **OpenCL**: Any OpenCL 2.0 device
- **CPU**: Fallback using clang/LLVM
- **WEBGPU**: Browser-based via Dawn

**How Tinygrad Compares to PyTorch**

</details>

### Similar
- Eager Tensor API
- Autograd（自动微分）
- 优化器（SGD、Adam 等）
- 基础数据集和层
- 可以编写熟悉的训练循环

### Unlike PyTorch
- **整个编译器和 IR 可见且可改造**
- 一切都是 Python（没有隐藏的 C++/CUDA）
- 默认惰性求值
- 更简单、更透明的架构
- 更容易添加自定义后端

## 社区教程（tinygrad-notes）

贡献前的前置知识。[GitHub](https://github.com/mesozoic-egg/tinygrad-notes) · [Website](https://mesozoic-egg.github.io/tinygrad-notes/)

| 主题 | 描述 |
|-------|-------------|
| 简介 | 先读这篇 |
| JIT 讲解 | 即时编译 |
| Shapetracker 讲解 | 形状与步长跟踪 |
| 卷积与 arange | conv/arange 中的技巧 |
| BEAM 搜索 | kernel 优化 |
| 矩阵乘法 | matmul 中的技巧 |
| VIZ=1 | 可视化图重写 |
| 模式匹配器 | 重写规则 |
| Memoryview | Buffer 视图 |
| 算子融合 | 融合算子 |
| UOp 是单例 | IR 设计 |
| LOP3（PTX/SASS）| GPU 指令 |

## Tinybox

Tiny corp 销售高性能 AI 工作站：
- **Red v2**：4x AMD 9070XT，$12,000
- **Green v2**：4x RTX PRO 6000，$60,000
- **Pro v2**：8x RTX 5090，$60,000

## 状态

目前处于 alpha 阶段。当它能在 1 块 NVIDIA GPU 上以比 PyTorch 快 2 倍的速度复现常见论文时，就会离开 alpha 阶段。

## 学习资源

- [快速入门指南](https://tinygrad.github.io/tinygrad/quickstart/)
- [MNIST 教程](https://docs.tinygrad.org/mnist/)
- [GitHub 示例](https://github.com/tinygrad/tinygrad/tree/main/examples)
- [Runtime 文档](https://docs.tinygrad.org/runtime/)
- 参见 [internals.md](internals.md) 以改造编译器、IR 和调度器


<details>
<summary>English original</summary>

**Similar**
- Eager Tensor API
- Autograd (automatic differentiation)
- Optimizers (SGD, Adam, etc.)
- Basic datasets and layers
- You can write familiar training loops

**Unlike PyTorch**
- **The entire compiler and IR are visible and hackable**
- Everything is in Python (no hidden C++/CUDA)
- Lazy evaluation by default
- Simpler, more transparent architecture
- Easier to add custom backends

**Community Tutorials (tinygrad-notes)**

Prerequisite knowledge before contributing. [GitHub](https://github.com/mesozoic-egg/tinygrad-notes) · [Website](https://mesozoic-egg.github.io/tinygrad-notes/)

| Topic | Description |
|-------|-------------|
| Introduction | Read first |
| JIT explained | Just-in-time compilation |
| Shapetracker explained | Shape and stride tracking |
| Convolution and arange | The trick in conv/arange |
| BEAM search | Kernel optimization |
| Matrix multiplication | The trick in matmul |
| VIZ=1 | Visualizing graph rewrite |
| Pattern matcher | Rewrite rules |
| Memoryview | Buffer views |
| Operator fusion | Fusing ops |
| UOp is singleton | IR design |
| LOP3 (PTX/SASS) | GPU instruction |

**The Tinybox**

Tiny corp sells high-performance AI workstations:
- **Red v2**: 4x AMD 9070XT, $12,000
- **Green v2**: 4x RTX PRO 6000, $60,000
- **Pro v2**: 8x RTX 5090, $60,000

**Status**

Currently in alpha. Will leave alpha when it can reproduce common papers 2x faster than PyTorch on 1 NVIDIA GPU.

**Learning Resources**

- [Quickstart Guide](https://tinygrad.github.io/tinygrad/quickstart/)
- [MNIST Tutorial](https://docs.tinygrad.org/mnist/)
- [GitHub Examples](https://github.com/tinygrad/tinygrad/tree/main/examples)
- [Runtime Documentation](https://docs.tinygrad.org/runtime/)
- See [internals.md](internals.md) for hacking the compiler, IR, and scheduler

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track E - Autonomous Vehicles/3. tinygrad for Inference/notes/overview.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20E%20-%20Autonomous%20Vehicles/3.%20tinygrad%20for%20Inference/notes/overview.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
