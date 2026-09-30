---
title: Tinygrad 项目
description: Tinygrad 项目
---

# Tinygrad 项目

为 [tinygrad 学习指南](../Guide.md) 准备的动手项目。按顺序完成它们 — 每个都基于前一个。

## 设置

```bash
# Install tinygrad (editable from source — recommended)
git clone https://github.com/tinygrad/tinygrad
cd tinygrad
pip install -e ".[dev]"
cd -

# Or from pip (stable)
pip install tinygrad

# Verify
python3 -c "from tinygrad import Tensor; print(Tensor([1,2,3]).numpy())"
```

## 调试环境变量

这些变量能解锁 tinygrad 的内部机制。在完成这些项目的过程中要持续使用它们。

| 命令 | 用途 |
|---------|---------|
| `DEBUG=1 python3 script.py` | 显示 kernel 数量以及每个 `.realize()` 的耗时 |
| `DEBUG=2 python3 script.py` | 显示 kernel 名称和输出形状 |
| `DEBUG=3 python3 script.py` | 打印生成的 kernel 源代码（C/CUDA/MSL） |
| `DEBUG=4 python3 script.py` | 在每个优化阶段转储完整 UOp IR |
| `VIZ=1 python3 script.py` | 打开基于浏览器的计算图可视化器 |
| `BEAM=2 python3 script.py` | 启用 BEAM search kernel 自动调优 |
| `NOOPT=1 python3 script.py` | 禁用所有代数重写（基线对比） |
| `CLANG=1 python3 script.py` | 强制 CPU/Clang 后端（可读的 C 输出） |

---

## 项目 1：张量基础与惰性求值

**文件：** `01_tensor_basics.py`

学习内容：
- 所有张量工厂方法及其形状/dtype
- 为什么 tinygrad 直到 `.realize()` 或 `.numpy()` 才计算
- Op 融合 — 多个 Python op 编译为单个 GPU kernel
- 如何在运行前检查执行调度

```bash
python3 01_tensor_basics.py
DEBUG=1 python3 01_tensor_basics.py     # see that the fused path uses 1 kernel
DEBUG=3 python3 01_tensor_basics.py     # read the generated C for the fused kernel
```

**需要回答的关键问题：** `(a + b).relu().sum()` 会生成多少个 kernel？为什么？

---

## 项目 2：三种 Op 类型

**文件：** `02_op_types.py`

学习内容：
- ElementwiseOps：逐元素独立 — 可轻松融合
- ReduceOps：压缩维度 — 打破融合边界
- MovementOps：通过 ShapeTracker 实现零拷贝
- 矩阵乘如何分解为 expand + multiply + sum

```bash
python3 02_op_types.py
DEBUG=1 python3 02_op_types.py
```

**关键洞察：** 仅 Movement ops 就产生 **零个 kernel**。用 `DEBUG=1` 验证这一点。

---

## 项目 3：从零实现 Autograd

**文件：** `03_autograd.py`

学习内容：
- `.backward()` 如何通过反向模式 autodiff 计算梯度
- 用解析方法验证梯度（平方和、sigmoid 等）
- 用有限差分进行数值梯度检查
- 训练一个小多层感知机来过拟合 XOR

```bash
python3 03_autograd.py
```

**关键练习：** 自己实现数值梯度检查器。把它应用到你在项目 6 中编写的任何自定义 op。

---

## 项目 4：训练 MNIST

**文件：** `04_mnist.py`

学习内容：
- 完整训练循环：数据 → forward → 损失 → backward → 优化器 step
- tinygrad 的 `nn` 模块：`Conv2d`、`BatchNorm`、`Linear`
- 用于模型持久化的 `safe_save` / `safe_load`
- 用 `DEBUG=1` 和 BEAM search 进行性能剖析

```bash
python3 04_mnist.py
BEAM=2 python3 04_mnist.py              # auto-tune kernels
DEBUG=1 python3 04_mnist.py             # profile kernel counts per step
```

**目标：** 在 5 个 epoch 内测试准确率 >98%。

---

## 项目 5：检查编译器

**文件：** `05_compiler_pipeline.py`

学习内容：
- 调度生成：哪些 op 被融合进哪些 kernel
- UOp 树结构：表示计算的 IR 节点
- 代数重写：`x*1→x`、`log(exp(x))→x`、常量折叠
- 常见 ML op 的 kernel 数量模式（attention、layernorm、conv）
- BEAM search：tinygrad 如何自动调优 kernel 分块

```bash
python3 05_compiler_pipeline.py
VIZ=1   python3 05_compiler_pipeline.py     # graph browser
DEBUG=4 python3 05_compiler_pipeline.py     # full IR dump
NOOPT=1 python3 05_compiler_pipeline.py     # see unoptimized kernels
BEAM=4  python3 05_compiler_pipeline.py     # enable kernel tuning
```

**深入探究：** 运行 `DEBUG=4 CLANG=1` 并阅读 softmax 的 IR 阶段。找出归约和归一化除法在何处被调度。

---

## 项目 6：自定义操作

**文件：** `06_custom_ops.py`

学习内容：
- 用 tinygrad 原语构建 GELU、Swish、Mish、RMSNorm、LayerNorm
- 用原语实现缩放点积 attention（带因果掩码）
- 自定义 op 的梯度验证
- 融合影响：同一 op 的融合与未融合实现

```bash
python3 06_custom_ops.py
DEBUG=1 python3 06_custom_ops.py    # count kernels for each op
DEBUG=3 python3 06_custom_ops.py    # see the generated kernels
```

**挑战：** 用 tinygrad 原语实现 Flash Attention 的分块 softmax。验证它在数值上与朴素实现一致。

---


<details>
<summary>English original</summary>

**Tinygrad Projects**

Hands-on projects for the [tinygrad learning guide](../Guide.md). Work through them in order — each builds on the previous.

**Setup**

```bash
# Install tinygrad (editable from source — recommended)
git clone https://github.com/tinygrad/tinygrad
cd tinygrad
pip install -e ".[dev]"
cd -

# Or from pip (stable)
pip install tinygrad

# Verify
python3 -c "from tinygrad import Tensor; print(Tensor([1,2,3]).numpy())"
```

**Debug Environment Variables**

These unlock tinygrad's internals. Use them constantly while working through the projects.

| Command | Purpose |
|---------|---------|
| `DEBUG=1 python3 script.py` | Show kernel count and timing per `.realize()` |
| `DEBUG=2 python3 script.py` | Show kernel names and output shapes |
| `DEBUG=3 python3 script.py` | Print generated kernel source code (C/CUDA/MSL) |
| `DEBUG=4 python3 script.py` | Dump full UOp IR at every optimization stage |
| `VIZ=1 python3 script.py` | Open browser-based computation graph visualizer |
| `BEAM=2 python3 script.py` | Enable BEAM search kernel auto-tuning |
| `NOOPT=1 python3 script.py` | Disable all algebraic rewrites (baseline comparison) |
| `CLANG=1 python3 script.py` | Force CPU/Clang backend (readable C output) |

---

**Project 1: Tensor Basics and Lazy Evaluation**

**File:** `01_tensor_basics.py`

What you learn:
- All tensor factory methods and their shapes/dtypes
- Why tinygrad doesn't compute until `.realize()` or `.numpy()`
- Op fusion — multiple Python ops compiled into a single GPU kernel
- How to inspect the execution schedule before running

```bash
python3 01_tensor_basics.py
DEBUG=1 python3 01_tensor_basics.py     # see that the fused path uses 1 kernel
DEBUG=3 python3 01_tensor_basics.py     # read the generated C for the fused kernel
```

**Key question to answer:** How many kernels does `(a + b).relu().sum()` produce? Why?

---

**Project 2: The Three Op Types**

**File:** `02_op_types.py`

What you learn:
- ElementwiseOps: independent per-element — trivially fuse
- ReduceOps: dimension-collapsing — break fusion boundaries
- MovementOps: zero-copy via ShapeTracker
- How matmul decomposes to expand + multiply + sum

```bash
python3 02_op_types.py
DEBUG=1 python3 02_op_types.py
```

**Key insight:** Movement ops alone produce **zero kernels**. Verify this with `DEBUG=1`.

---

**Project 3: Autograd from Scratch**

**File:** `03_autograd.py`

What you learn:
- How `.backward()` computes gradients via reverse-mode autodiff
- Verifying gradients analytically (sum of squares, sigmoid, etc.)
- Numerical gradient checking with finite differences
- Training a small MLP to overfit XOR

```bash
python3 03_autograd.py
```

**Key exercise:** Implement the numerical gradient checker yourself. Apply it to any custom op you write in Project 6.

---

**Project 4: Training MNIST**

**File:** `04_mnist.py`

What you learn:
- Full training loop: data → forward → loss → backward → optimizer step
- tinygrad's `nn` module: `Conv2d`, `BatchNorm`, `Linear`
- `safe_save` / `safe_load` for model persistence
- Profiling with `DEBUG=1` and BEAM search

```bash
python3 04_mnist.py
BEAM=2 python3 04_mnist.py              # auto-tune kernels
DEBUG=1 python3 04_mnist.py             # profile kernel counts per step
```

**Target:** >98% test accuracy in 5 epochs.

---

**Project 5: Inspecting the Compiler**

**File:** `05_compiler_pipeline.py`

What you learn:
- Schedule generation: which ops get fused into which kernels
- UOp tree structure: the IR nodes that represent computation
- Algebraic rewrites: `x*1→x`, `log(exp(x))→x`, constant folding
- Kernel count patterns for common ML ops (attention, layernorm, conv)
- BEAM search: how tinygrad auto-tunes kernel tiling

```bash
python3 05_compiler_pipeline.py
VIZ=1   python3 05_compiler_pipeline.py     # graph browser
DEBUG=4 python3 05_compiler_pipeline.py     # full IR dump
NOOPT=1 python3 05_compiler_pipeline.py     # see unoptimized kernels
BEAM=4  python3 05_compiler_pipeline.py     # enable kernel tuning
```

**Deep dive:** Run `DEBUG=4 CLANG=1` and read the IR stages for a softmax. Identify where the reduction and the normalization division are scheduled.

---

**Project 6: Custom Operations**

**File:** `06_custom_ops.py`

What you learn:
- Building GELU, Swish, Mish, RMSNorm, LayerNorm from tinygrad primitives
- Scaled dot-product attention (with causal masking) from primitives
- Gradient verification for custom ops
- Fusion impact: fused vs unfused implementation of the same op

```bash
python3 06_custom_ops.py
DEBUG=1 python3 06_custom_ops.py    # count kernels for each op
DEBUG=3 python3 06_custom_ops.py    # see the generated kernels
```

**Challenge:** Implement Flash Attention's tiled softmax using tinygrad primitives. Verify it matches the naive implementation numerically.

---

</details>

## 项目 7：自定义后端

**File:** `07_custom_backend/backend.py`

你将学到：
- 每个 tinygrad 后端都要实现的三个组件：Allocator、Compiler、Runner
- 设备层面的内存如何管理
- 生成的 C 源码如何编译为 .so 并执行
- 如何将你的后端与内置 CLANG 后端做 benchmark 对比

```bash
python3 07_custom_backend/backend.py              # run tests
python3 07_custom_backend/backend.py benchmark    # benchmark vs CLANG
```

**扩展：** 为生成的 C 中的逐元素循环加入 OpenMP 并行。按你 CPU 的核数测量加速比。

---

## 学习路径

```
01 Basics → 02 Op Types → 03 Autograd → 04 MNIST → 05 Compiler → 06 Custom Ops → 07 Backend
  (3h)         (3h)           (4h)         (4h)        (6h)           (6h)            (12h)
```

总计：约 38 小时的专注工作。做完后面的项目后，再用 `DEBUG=4` 和 `VIZ=1` 回到更早的项目——你会看到多得多的内容。

---

## 延伸阅读

- [Guide.md](../Guide.md) — 完整学习指南，含每个主题的理论
- tinygrad-notes.md — 概览与核心哲学（见 tinygrad 源码仓库）
- hacking-tinygrad.md — 内部实现的代码片段（见 tinygrad 源码仓库）
- [tinygrad source](https://github.com/tinygrad/tinygrad) — 阅读 `tinygrad/tensor.py` 和 `tinygrad/runtime/ops_clang.py`
- [abstractions.py](https://github.com/tinygrad/tinygrad/blob/master/docs/abstractions.py) — 带注释的架构走读


<details>
<summary>English original</summary>

**Project 7: Custom Backend**

**File:** `07_custom_backend/backend.py`

What you learn:
- The three components every tinygrad backend implements: Allocator, Compiler, Runner
- How memory is managed at the device level
- How generated C source is compiled to a .so and executed
- How to benchmark your backend vs the built-in CLANG backend

```bash
python3 07_custom_backend/backend.py              # run tests
python3 07_custom_backend/backend.py benchmark    # benchmark vs CLANG
```

**Extension:** Add OpenMP parallelism to the element-wise loop in the generated C. Measure speedup on your CPU's core count.

---

**Learning Path**

```
01 Basics → 02 Op Types → 03 Autograd → 04 MNIST → 05 Compiler → 06 Custom Ops → 07 Backend
  (3h)         (3h)           (4h)         (4h)        (6h)           (6h)            (12h)
```

Total: ~38 hours of focused work. Return to earlier projects with `DEBUG=4` and `VIZ=1` after completing later ones — you'll see much more.

---

**Further Reading**

- [Guide.md](../Guide.md) — full learning guide with theory for each topic
- tinygrad-notes.md — overview and core philosophy (see tinygrad source repo)
- hacking-tinygrad.md — code snippets for internals (see tinygrad source repo)
- [tinygrad source](https://github.com/tinygrad/tinygrad) — read `tinygrad/tensor.py` and `tinygrad/runtime/ops_clang.py`
- [abstractions.py](https://github.com/tinygrad/tinygrad/blob/master/docs/abstractions.py) — annotated architecture walkthrough

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track E - Autonomous Vehicles/3. tinygrad for Inference/projects/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20E%20-%20Autonomous%20Vehicles/3.%20tinygrad%20for%20Inference/projects/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
