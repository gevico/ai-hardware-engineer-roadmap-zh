---
title: 模块 3 — tinygrad for Inference
description: 模块 3 — tinygrad for Inference
---

# 模块 3 — tinygrad for Inference

<div class="course-identity auto-course" style="--course-accent: #7c3aed; --course-accent-rgb: 124, 58, 237;" markdown="1">
<div class="course-identity__icon">MTFI</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入剖析 · 专项方向</p>
<p class="course-identity__title">模块 3 —— tinygrad for Inference 的专项课程标识。</p>
<p class="course-identity__meta">产物：专项方向案例研究 · 衡量指标：性能、可靠性、岗位匹配度</p>
</div>
</div>


**父级：** [阶段 5 — 自动驾驶](../Guide.md)

**从第一个张量到自定义后端的一条结构化、动手实践路径**

> Tinygrad 是一个极简的深度学习框架，**整个编译器和 IR 都可见、可在 Python 中随意修改**。要理解 `loss.backward()` 与真正运行的 GPU kernel 之间发生了什么，它是最理想的代码库。

**时间预估：** 第 1–6 部分 3–6 个月 · 第 7 部分长期进行

---

## 前置要求

- 熟练使用 Python（函数、类、装饰器、生成器）
- 基础线性代数（矩阵乘、点积、广播）
- 熟悉神经网络训练（前向传播、损失、反向传播、优化器）
- 可选但有用：后端章节所需的 CUDA/OpenCL 基础

---

## 第 1 部分：为什么选 Tinygrad？

### 用 PyTorch 学习的痛点

PyTorch 的 autograd、JIT 和 kernel 调度藏在数千行 C++/CUDA 里。你读不到它们，只能观察它们的效果。Tinygrad 解决了这一点：

- **整个框架约 5,000 行 Python**
- 每一处优化、IR 变换和 kernel 都可阅读
- 可以设置 `DEBUG=4`，观察编译的每一个阶段

### Tinygrad 在生态中的位置

```
micrograd  →  tinygrad  →  PyTorch
(no GPU)      (hackable)   (production)
```

- **micrograd**（Karpathy）：纯 autograd，没有张量、没有 GPU — 适合理解反向传播
- **tinygrad**：惰性张量、真实 GPU 后端、可修改的编译器 — 适合理解框架
- **PyTorch**：完整生产框架，内部不透明 — 适合用来搭东西

### 为什么这对 AI 硬件工程师重要

Tinygrad 是 ML 工作负载与自定义硬件之间的软件接口。理解它，你就能：
- 明确知道你的加速器必须支持哪些算子
- 针对真实的计算模式（矩阵乘、归约、逐元素）设计硬件
- 编写面向自研芯片的编译器后端
- 理解 Openpilot 为何在生产级 ADAS 推理中使用 tinygrad

---

## 第 2 部分：环境搭建与起步

### 安装

```bash
# Option 1: pip (stable)
pip install tinygrad

# Option 2: from source (recommended for learning — you can read/modify it)
git clone https://github.com/tinygrad/tinygrad
cd tinygrad
pip install -e ".[dev]"   # editable install so changes take effect immediately
```

### 验证环境

```python
from tinygrad import Tensor, Device
print(Device.DEFAULT)           # shows your default backend (CUDA, METAL, CLANG, etc.)
print(Tensor([1,2,3]).numpy())  # [1. 2. 3.]
```

### 调试环境变量

这些是理解 tinygrad 行为最重要的工具：

| 变量 | 取值 | 你会看到什么 |
|----------|-------|-------------|
| `DEBUG` | `1` | kernel 数量与耗时 |
| `DEBUG` | `2` | kernel 名称与形状 |
| `DEBUG` | `3` | 生成的 kernel 源码 |
| `DEBUG` | `4` | 每个优化阶段的完整 UOp IR |
| `VIZ` | `1` | 打开浏览器中的 UOp 图可视化器 |
| `BEAM` | `2` | 启用 BEAM search kernel 优化 |
| `NOOPT` | `1` | 关闭优化（查看未优化的 IR） |

---

## 第 3 部分：张量 API

### 创建张量

```python
from tinygrad import Tensor
import numpy as np

# From Python lists
t = Tensor([1, 2, 3, 4])

# From numpy
t = Tensor(np.array([[1.0, 2.0], [3.0, 4.0]]))

# Factory methods
t = Tensor.zeros(3, 4)
t = Tensor.ones(3, 4)
t = Tensor.randn(3, 4)         # normal distribution
t = Tensor.rand(3, 4)          # uniform [0, 1)
t = Tensor.arange(0, 10, 2)    # [0, 2, 4, 6, 8]
t = Tensor.eye(4)              # identity matrix
```

### 算子（与 PyTorch 兼容的 API）

```python
a = Tensor.randn(4, 4)
b = Tensor.randn(4, 4)

# Elementwise
c = a + b
c = a * b
c = a.relu()
c = a.exp()
c = a.log()
c = a.sqrt()

# Reduction
s = a.sum()
s = a.sum(axis=0)              # sum along rows → shape (4,)
m = a.max(axis=1, keepdim=True)

# Matrix operations
c = a @ b                      # matmul → shape (4,4)
c = a.T                        # transpose

# Shape manipulation
c = a.reshape(2, 8)
c = a.permute(1, 0)            # like numpy.transpose
c = a.expand(2, 4, 4)         # broadcast-expand (zero-copy)
c = a.unsqueeze(0)             # add dim at position 0
c = a.squeeze(0)               # remove dim of size 1
```


<details>
<summary>English original</summary>

**Module 3 — tinygrad for Inference**

<div class="course-identity auto-course" style="--course-accent: #7c3aed; --course-accent-rgb: 124, 58, 237;" markdown="1">
<div class="course-identity__icon">MTFI</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Specialization</p>
<p class="course-identity__title">Specialized course identity for Module 3 — tinygrad for Inference.</p>
<p class="course-identity__meta">Artifact: specialization case study · Measure: performance, reliability, role fit</p>
</div>
</div>


**Parent:** [Phase 5 — Autonomous Driving](../Guide.md)

**A structured, hands-on path from first tensor to custom backend**

> Tinygrad is a minimal deep learning framework where **the entire compiler and IR are visible and hackable in Python**. It is the ideal codebase for understanding what happens between `loss.backward()` and the GPU kernel that actually runs.

**Time estimate:** 3–6 months for Parts 1–6 · Ongoing for Part 7

---

**Prerequisites**

- Python proficiency (functions, classes, decorators, generators)
- Basic linear algebra (matrix multiply, dot product, broadcasting)
- Familiarity with neural network training (forward pass, loss, backprop, optimizer)
- Optional but helpful: CUDA/OpenCL basics for backend sections

---

**Part 1: Why Tinygrad?**

**The Problem with PyTorch for Learning**

PyTorch's autograd, JIT, and kernel dispatch live in thousands of lines of C++/CUDA. You can't read them. You can only observe their effects. Tinygrad solves this:

- **The entire framework is ~5,000 lines of Python**
- Every optimization, IR transformation, and kernel is readable
- You can set `DEBUG=4` and watch every stage of compilation

**Tinygrad's Position in the Ecosystem**

```
micrograd  →  tinygrad  →  PyTorch
(no GPU)      (hackable)   (production)
```

- **micrograd** (Karpathy): pure autograd, no tensors, no GPU — great for understanding backprop
- **tinygrad**: lazy tensors, real GPU backends, hackable compiler — great for understanding frameworks
- **PyTorch**: full production framework, opaque internals — great for building things

**Why It Matters for AI Hardware Engineers**

Tinygrad is the software interface between ML workloads and custom hardware. Understanding it lets you:
- Know exactly what operations your accelerator must support
- Design hardware for the actual compute patterns (matmul, reduction, elementwise)
- Write compiler backends that target your custom chip
- Understand why Openpilot uses tinygrad for production ADAS inference

---

**Part 2: Setup and First Steps**

**Installation**

```bash
# Option 1: pip (stable)
pip install tinygrad

# Option 2: from source (recommended for learning — you can read/modify it)
git clone https://github.com/tinygrad/tinygrad
cd tinygrad
pip install -e ".[dev]"   # editable install so changes take effect immediately
```

**Verifying Your Setup**

```python
from tinygrad import Tensor, Device
print(Device.DEFAULT)           # shows your default backend (CUDA, METAL, CLANG, etc.)
print(Tensor([1,2,3]).numpy())  # [1. 2. 3.]
```

**Debug Environment Variables**

These are your most important tools for understanding what tinygrad does:

| Variable | Value | What You See |
|----------|-------|-------------|
| `DEBUG` | `1` | Kernel count and timing |
| `DEBUG` | `2` | Kernel names and shapes |
| `DEBUG` | `3` | Generated kernel source code |
| `DEBUG` | `4` | Full UOp IR at every optimization stage |
| `VIZ` | `1` | Opens a browser graph visualizer for UOps |
| `BEAM` | `2` | Enable BEAM search kernel optimization |
| `NOOPT` | `1` | Disable optimizations (see unoptimized IR) |

---

**Part 3: Tensor API**

**Creating Tensors**

```python
from tinygrad import Tensor
import numpy as np

# From Python lists
t = Tensor([1, 2, 3, 4])

# From numpy
t = Tensor(np.array([[1.0, 2.0], [3.0, 4.0]]))

# Factory methods
t = Tensor.zeros(3, 4)
t = Tensor.ones(3, 4)
t = Tensor.randn(3, 4)         # normal distribution
t = Tensor.rand(3, 4)          # uniform [0, 1)
t = Tensor.arange(0, 10, 2)    # [0, 2, 4, 6, 8]
t = Tensor.eye(4)              # identity matrix
```

**Operations (PyTorch-Compatible API)**

```python
a = Tensor.randn(4, 4)
b = Tensor.randn(4, 4)

# Elementwise
c = a + b
c = a * b
c = a.relu()
c = a.exp()
c = a.log()
c = a.sqrt()

# Reduction
s = a.sum()
s = a.sum(axis=0)              # sum along rows → shape (4,)
m = a.max(axis=1, keepdim=True)

# Matrix operations
c = a @ b                      # matmul → shape (4,4)
c = a.T                        # transpose

# Shape manipulation
c = a.reshape(2, 8)
c = a.permute(1, 0)            # like numpy.transpose
c = a.expand(2, 4, 4)         # broadcast-expand (zero-copy)
c = a.unsqueeze(0)             # add dim at position 0
c = a.squeeze(0)               # remove dim of size 1
```

</details>

### 取回数值（物化）

```python
# .realize() executes the lazy graph and returns the same tensor
t = Tensor.randn(3, 3)
t = t.realize()

# .numpy() realizes AND copies to numpy
arr = t.numpy()    # triggers .realize() if not already done

# .item() for scalar tensors
loss_val = loss.item()
```

---

## 第 4 部分：惰性求值 —— 核心概念

这是最需要内化的概念。**在你调用 `.realize()` 或 `.numpy()` 之前，什么都不会运行。**

### 「惰性」的含义

```python
import os
os.environ['DEBUG'] = '2'

a = Tensor.randn(4, 4)   # no computation
b = a + 1                 # no computation — records ADD in graph
c = b * 2                 # no computation — records MUL in graph
d = c.relu()              # no computation — records RELU in graph

# Only now does tinygrad compile all 3 ops into ONE kernel
d.realize()               # prints: "1 kernels, X.Xms"
```

tinygrad 把 `a+1`、`*2` 和 `relu()` 融合进单个 kernel —— 不需要中间缓冲区。这就是 **op 融合**，一项关键优化。

### UOp：tinygrad 的中间表示节点

每个张量都有一个 `.uop` 属性 —— 计算图中的一个节点：

```python
from tinygrad import Tensor

x = Tensor([1.0, 2.0, 3.0])
y = x + 1

print(type(y.uop))    # <class 'tinygrad.uop.UOp'>
print(y.uop.op)       # the operation type
print(y.uop.src)      # input UOps (the graph edges)
```

UOp 构成 DAG（有向无环图）。编译器遍历该图以：
1. 融合运算
2. 应用代数重写（例如 `x * 1 → x`）
3. 生成 kernel 代码

---

## 第 5 部分：三种运算类型

tinygrad 中的一切都分解为三类原语运算。**没有 conv2d 原语。没有矩阵乘原语。** 它们都由这三类构建而来：

### 1. ElementwiseOps

对每个元素独立运算 —— 极易并行：

| 类别 | 运算 |
|----------|-----------|
| UnaryOps | `SQRT, LOG2, EXP2, SIN, NEG, RECIP, CAST` |
| BinaryOps | `ADD, MUL, SUB, DIV, MAX, MOD, CMPLT` |
| TernaryOps | `WHERE (if/else), MULACC` |

```python
# All of these lower to ElementwiseOps:
x.relu()          # WHERE(x > 0, x, 0)  →  TernaryOp(WHERE)
x.sigmoid()       # 1 / (1 + exp(-x))   →  chain of UnaryOps + BinaryOps
x.exp()           # EXP2(x * log2(e))   →  BinaryOp(MUL) + UnaryOp(EXP2)
```

### 2. ReduceOps

折叠一个维度 —— 需要跨元素通信：

```python
x = Tensor.randn(1024, 1024)

# These are ReduceOps:
x.sum(axis=0)     # SUM reduce along axis 0
x.max(axis=1)     # MAX reduce along axis 1
x.mean()          # SUM / size  →  ReduceOp + ElementwiseOp
```

ReduceOps 是 GPU 编程中最难的部分 —— 它们需要精心设计的并行归约模式。

### 3. MovementOps（经由 ShapeTracker）

Reshape、permute、expand —— **零拷贝**，因为它们只改变索引到内存的映射方式：

```python
x = Tensor.randn(4, 8, 16)

# These don't copy data — they update the ShapeTracker:
y = x.reshape(32, 16)       # just relabels dimensions
y = x.permute(2, 0, 1)      # reorders access pattern
y = x.expand(10, 4, 8, 16)  # broadcasts (adds a new dimension)
y = x[1:3, :, ::2]          # slice + stride

# Data only moves when a materialization (realize/numpy) is needed
```

**ShapeTracker** 保存步长与偏移量，它们定义了逻辑索引如何映射到物理缓冲区索引。零拷贝视图就是这样实现的。

---

## 第 6 部分：Autograd

### tinygrad 如何实现反向传播

tinygrad 采用**反向模式自动微分**（backprop）。每个需要梯度的运算都有对应的 backward 函数。

```python
from tinygrad import Tensor

# Gradient tracking is enabled with requires_grad=True
# (or automatically when a leaf tensor needs grad)
x = Tensor([2.0, 3.0], requires_grad=True)
y = (x * x).sum()          # y = sum(x^2)

y.backward()               # computes dy/dx = 2x

print(x.grad.numpy())      # [4. 6.]  — correct: d(sum(x^2))/dx = 2x
```

### 训练循环模式

```python
from tinygrad import Tensor
from tinygrad.nn.optim import Adam, SGD

# Model (simple 2-layer MLP)
class MLP:
    def __init__(self):
        self.l1 = Tensor.randn(784, 128) * 0.01
        self.l2 = Tensor.randn(128, 10) * 0.01

    def __call__(self, x):
        return x.linear(self.l1).relu().linear(self.l2)

model = MLP()
optim = Adam([model.l1, model.l2], lr=0.001)

for step in range(1000):
    x = Tensor.randn(32, 784)          # fake batch
    y_target = Tensor.zeros(32, 10)    # fake labels

    optim.zero_grad()
    y_pred = model(x)
    loss = (y_pred - y_target).pow(2).mean()   # MSE loss
    loss.backward()
    optim.step()

    if step % 100 == 0:
        print(f"step {step}, loss={loss.item():.4f}")
```


<details>
<summary>English original</summary>

**Getting Values Back (Materializing)**

```python
# .realize() executes the lazy graph and returns the same tensor
t = Tensor.randn(3, 3)
t = t.realize()

# .numpy() realizes AND copies to numpy
arr = t.numpy()    # triggers .realize() if not already done

# .item() for scalar tensors
loss_val = loss.item()
```

---

**Part 4: Lazy Evaluation — The Core Concept**

This is the most important concept to internalize. **Nothing runs until you call `.realize()` or `.numpy()`.**

**What "Lazy" Means**

```python
import os
os.environ['DEBUG'] = '2'

a = Tensor.randn(4, 4)   # no computation
b = a + 1                 # no computation — records ADD in graph
c = b * 2                 # no computation — records MUL in graph
d = c.relu()              # no computation — records RELU in graph

# Only now does tinygrad compile all 3 ops into ONE kernel
d.realize()               # prints: "1 kernels, X.Xms"
```

Tinygrad fused `a+1`, `*2`, and `relu()` into a single kernel — no intermediate buffers needed. This is **op fusion**, a key optimization.

**The UOp: Tinygrad's IR Node**

Every tensor has a `.uop` attribute — a node in the computation graph:

```python
from tinygrad import Tensor

x = Tensor([1.0, 2.0, 3.0])
y = x + 1

print(type(y.uop))    # <class 'tinygrad.uop.UOp'>
print(y.uop.op)       # the operation type
print(y.uop.src)      # input UOps (the graph edges)
```

UOps form a DAG (Directed Acyclic Graph). The compiler walks this graph to:
1. Fuse operations
2. Apply algebraic rewrites (e.g., `x * 1 → x`)
3. Generate kernel code

---

**Part 5: The Three Operation Types**

Everything in tinygrad decomposes to three kinds of primitive operations. **No conv2d primitive. No matmul primitive.** They are built from these three:

**1. ElementwiseOps**

Operate on each element independently — trivially parallelizable:

| Category | Operations |
|----------|-----------|
| UnaryOps | `SQRT, LOG2, EXP2, SIN, NEG, RECIP, CAST` |
| BinaryOps | `ADD, MUL, SUB, DIV, MAX, MOD, CMPLT` |
| TernaryOps | `WHERE (if/else), MULACC` |

```python
# All of these lower to ElementwiseOps:
x.relu()          # WHERE(x > 0, x, 0)  →  TernaryOp(WHERE)
x.sigmoid()       # 1 / (1 + exp(-x))   →  chain of UnaryOps + BinaryOps
x.exp()           # EXP2(x * log2(e))   →  BinaryOp(MUL) + UnaryOp(EXP2)
```

**2. ReduceOps**

Collapse a dimension — require communication across elements:

```python
x = Tensor.randn(1024, 1024)

# These are ReduceOps:
x.sum(axis=0)     # SUM reduce along axis 0
x.max(axis=1)     # MAX reduce along axis 1
x.mean()          # SUM / size  →  ReduceOp + ElementwiseOp
```

ReduceOps are the hard part of GPU programming — they require careful parallel reduction patterns.

**3. MovementOps (via ShapeTracker)**

Reshape, permute, expand — **zero-copy** because they only change how indices map to memory:

```python
x = Tensor.randn(4, 8, 16)

# These don't copy data — they update the ShapeTracker:
y = x.reshape(32, 16)       # just relabels dimensions
y = x.permute(2, 0, 1)      # reorders access pattern
y = x.expand(10, 4, 8, 16)  # broadcasts (adds a new dimension)
y = x[1:3, :, ::2]          # slice + stride

# Data only moves when a materialization (realize/numpy) is needed
```

**ShapeTracker** stores the strides and offsets that define how a logical index maps to a physical buffer index. This is how zero-copy views work.

---

**Part 6: Autograd**

**How tinygrad Implements Backprop**

Tinygrad uses **reverse-mode automatic differentiation** (backprop). Every op that needs a gradient has a corresponding backward function.

```python
from tinygrad import Tensor

# Gradient tracking is enabled with requires_grad=True
# (or automatically when a leaf tensor needs grad)
x = Tensor([2.0, 3.0], requires_grad=True)
y = (x * x).sum()          # y = sum(x^2)

y.backward()               # computes dy/dx = 2x

print(x.grad.numpy())      # [4. 6.]  — correct: d(sum(x^2))/dx = 2x
```

**Training Loop Pattern**

```python
from tinygrad import Tensor
from tinygrad.nn.optim import Adam, SGD

# Model (simple 2-layer MLP)
class MLP:
    def __init__(self):
        self.l1 = Tensor.randn(784, 128) * 0.01
        self.l2 = Tensor.randn(128, 10) * 0.01

    def __call__(self, x):
        return x.linear(self.l1).relu().linear(self.l2)

model = MLP()
optim = Adam([model.l1, model.l2], lr=0.001)

for step in range(1000):
    x = Tensor.randn(32, 784)          # fake batch
    y_target = Tensor.zeros(32, 10)    # fake labels

    optim.zero_grad()
    y_pred = model(x)
    loss = (y_pred - y_target).pow(2).mean()   # MSE loss
    loss.backward()
    optim.step()

    if step % 100 == 0:
        print(f"step {step}, loss={loss.item():.4f}")
```

</details>

### tinygrad 的 nn 模块

```python
from tinygrad.nn import Linear, BatchNorm, Conv2d
from tinygrad.nn.optim import Adam, SGD, AdamW
from tinygrad.nn.state import get_parameters, get_state_dict, load_state_dict, safe_save, safe_load

# Linear layer
layer = Linear(128, 64, bias=True)

# Conv2d
conv = Conv2d(in_channels=3, out_channels=64, kernel_size=3, stride=1, padding=1)

# Get all parameters of a model
params = get_parameters(model)

# Save/load weights
state = get_state_dict(model)
safe_save(state, "model.safetensors")
state = safe_load("model.safetensors")
load_state_dict(model, state)
```

---

## 第 7 部分：编译器流水线

这正是 tinygrad 独具教学价值之处。设置 `DEBUG=4`，即可观察每一个阶段。

### 阶段 1：调度生成

调用 `.realize()` 之后，tinygrad 首先创建一个 **schedule**——即需要运行的 kernel 列表：

```python
from tinygrad import Tensor

x = Tensor.randn(4, 4)
y = Tensor.randn(4, 4)
z = (x @ y).relu().sum(axis=0)

# Get the schedule without executing
sched = z.schedule()
print(f"{len(sched)} kernels")
for item in sched:
    print(item.ast)    # the abstract syntax tree for each kernel
```

被融合的算子显示为单个 schedule 条目。跨 buffer 边界的多个归约或算子会拆成独立条目。

### 阶段 2：UOp 降级与优化

每个 schedule 条目的 AST 被降级为一棵 **UOp 树**，随后通过一系列 **模式匹配重写规则** 进行优化：

```
AST  →  UOp tree  →  rewrite rules  →  optimized UOp  →  codegen
```

关键重写规则包括：
- **常量折叠**：`x + 0 → x`、`x * 1 → x`
- **代数恒等式**：`log(exp(x)) → x`
- **循环融合**：合并访问模式兼容的相邻循环
- **线性化**：把树转换为供 codegen 使用的扁平算子列表

### 阶段 3：Kernel 优化（BEAM Search）

设置 `BEAM=2` 即可启用 BEAM search——tinygrad 会尝试多种循环顺序与分块尺寸，对它们做 benchmark，并选出最快的一种：

```bash
BEAM=2 python3 my_script.py    # slower first run, fast after caching
```

BEAM search 会探索：
- **循环顺序**：并行化哪个维度、对哪个维度分块
- **工作组大小**：每个 block 有多少线程
- **展开与向量化**：SIMD 宽度

### 阶段 4：代码生成

优化后的 UOp 树被转换为目标平台专属代码：

```python
import os
os.environ['DEBUG'] = '3'
os.environ['CLANG'] = '1'    # use CPU backend to see readable C

from tinygrad import Tensor
x = Tensor.randn(4, 4)
y = Tensor.randn(4, 4)
(x @ y).realize()
# Prints: actual C function that implements the matmul
```

后端：`CLANG`（C）、`CUDA`（PTX/SASS）、`METAL`（MSL）、`AMD`（HIP）、`OpenCL`

---

## 第 8 部分：后端

### 现有后端结构

在 tinygrad 源码中，后端位于 `tinygrad/runtime/`：

```
tinygrad/runtime/
  ops_gpu.py    # OpenCL backend
  ops_cuda.py   # NVIDIA CUDA backend
  ops_metal.py  # Apple Metal backend
  ops_clang.py  # CPU/Clang backend (simplest — read this first)
  ops_hsa.py    # AMD HIP backend
```

每个后端实现三件事：
1. **分配器**：分配/释放设备内存，在 host 与设备之间拷贝
2. **编译器**：接收生成的源代码，编译为二进制（PTX、SPIR-V 等）
3. **Runtime**：加载已编译的程序，并用给定 buffer 执行

### 阅读 Clang 后端

`ops_clang.py` 是最简单的——它生成 C 代码，用 clang 编译，并以共享库形式运行。从这里开始：

```python
# Simplified structure of ops_clang.py:
class ClangAllocator(Allocator):
    def _alloc(self, size): return (ctypes.c_float * size)()
    def _free(self, buf): del buf
    def copyin(self, dst, src): ctypes.memmove(dst, src, src.nbytes)
    def copyout(self, dst, src): ctypes.memmove(dst, src, ctypes.sizeof(src))

class ClangCompiler(Compiler):
    def compile(self, src: str) -> bytes:
        # Write C to temp file, compile with clang, return .so bytes
        ...

class ClangRuntime(Runner):
    def __call__(self, *bufs, global_size, local_size):
        # Call the compiled C function with the given buffers
        ...
```


<details>
<summary>English original</summary>

**tinygrad's nn Module**

```python
from tinygrad.nn import Linear, BatchNorm, Conv2d
from tinygrad.nn.optim import Adam, SGD, AdamW
from tinygrad.nn.state import get_parameters, get_state_dict, load_state_dict, safe_save, safe_load

# Linear layer
layer = Linear(128, 64, bias=True)

# Conv2d
conv = Conv2d(in_channels=3, out_channels=64, kernel_size=3, stride=1, padding=1)

# Get all parameters of a model
params = get_parameters(model)

# Save/load weights
state = get_state_dict(model)
safe_save(state, "model.safetensors")
state = safe_load("model.safetensors")
load_state_dict(model, state)
```

---

**Part 7: The Compiler Pipeline**

This is where tinygrad becomes uniquely educational. Set `DEBUG=4` and watch every stage.

**Stage 1: Schedule Generation**

After you call `.realize()`, tinygrad first creates a **schedule** — a list of kernels that need to run:

```python
from tinygrad import Tensor

x = Tensor.randn(4, 4)
y = Tensor.randn(4, 4)
z = (x @ y).relu().sum(axis=0)

# Get the schedule without executing
sched = z.schedule()
print(f"{len(sched)} kernels")
for item in sched:
    print(item.ast)    # the abstract syntax tree for each kernel
```

Fused ops appear as a single schedule item. Multiple reductions or ops across buffer boundaries become separate items.

**Stage 2: UOp Lowering and Optimization**

Each schedule item's AST is lowered to a **UOp tree** and then optimized through a series of **pattern-matching rewrite rules**:

```
AST  →  UOp tree  →  rewrite rules  →  optimized UOp  →  codegen
```

Key rewrite rules include:
- **Constant folding**: `x + 0 → x`, `x * 1 → x`
- **Algebraic identities**: `log(exp(x)) → x`
- **Loop fusion**: merge adjacent loops with compatible access patterns
- **Linearization**: convert the tree into a flat list of operations for codegen

**Stage 3: Kernel Optimization (BEAM Search)**

Set `BEAM=2` to enable BEAM search — tinygrad tries multiple loop orderings and tile sizes, benchmarks them, and picks the fastest:

```bash
BEAM=2 python3 my_script.py    # slower first run, fast after caching
```

BEAM search explores:
- **Loop ordering**: which dimension to parallelize, which to tile
- **Work-group sizes**: how many threads per block
- **Unrolling and vectorization**: SIMD width

**Stage 4: Code Generation**

The optimized UOp tree is converted to target-specific code:

```python
import os
os.environ['DEBUG'] = '3'
os.environ['CLANG'] = '1'    # use CPU backend to see readable C

from tinygrad import Tensor
x = Tensor.randn(4, 4)
y = Tensor.randn(4, 4)
(x @ y).realize()
# Prints: actual C function that implements the matmul
```

Backends: `CLANG` (C), `CUDA` (PTX/SASS), `METAL` (MSL), `AMD` (HIP), `OpenCL`

---

**Part 8: Backends**

**Existing Backend Structure**

In the tinygrad source, backends live in `tinygrad/runtime/`:

```
tinygrad/runtime/
  ops_gpu.py    # OpenCL backend
  ops_cuda.py   # NVIDIA CUDA backend
  ops_metal.py  # Apple Metal backend
  ops_clang.py  # CPU/Clang backend (simplest — read this first)
  ops_hsa.py    # AMD HIP backend
```

Each backend implements three things:
1. **Allocator**: allocate/free device memory, copy to/from host
2. **Compiler**: take generated source code, compile to binary (PTX, SPIR-V, etc.)
3. **Runtime**: load a compiled program and execute it with given buffers

**Reading the Clang Backend**

`ops_clang.py` is the simplest — it generates C, compiles with clang, and runs as a shared library. Start here:

```python
# Simplified structure of ops_clang.py:
class ClangAllocator(Allocator):
    def _alloc(self, size): return (ctypes.c_float * size)()
    def _free(self, buf): del buf
    def copyin(self, dst, src): ctypes.memmove(dst, src, src.nbytes)
    def copyout(self, dst, src): ctypes.memmove(dst, src, ctypes.sizeof(src))

class ClangCompiler(Compiler):
    def compile(self, src: str) -> bytes:
        # Write C to temp file, compile with clang, return .so bytes
        ...

class ClangRuntime(Runner):
    def __call__(self, *bufs, global_size, local_size):
        # Call the compiled C function with the given buffers
        ...
```

</details>

### 添加自定义后端（骨架）

```python
# my_backend.py — minimal custom backend skeleton

from tinygrad.device import Compiled, Allocator, Compiler, Runner
from tinygrad.renderer.cstyle import CStyleLanguage

class MyAllocator(Allocator):
    def _alloc(self, size: int, options):
        # Allocate `size` bytes on your device
        # Return a handle (pointer, buffer object, etc.)
        ...

    def _free(self, buf, options):
        # Free the buffer
        ...

    def copyin(self, dst, src: memoryview):
        # Copy from host (numpy array) to device buffer
        ...

    def copyout(self, dst: memoryview, src):
        # Copy from device buffer to host (numpy array)
        ...

class MyCompiler(Compiler):
    def compile(self, src: str) -> bytes:
        # src is the generated C/kernel source as a string
        # Compile it to binary and return bytes
        ...

class MyRunner(Runner):
    def __init__(self, name: str, lib: bytes):
        # Load the compiled binary
        ...

    def __call__(self, *bufs, global_size, local_size, wait=False):
        # Execute the kernel with given buffers and launch dimensions
        ...

class MyDevice(Compiled):
    def __init__(self, device: str):
        super().__init__(
            device,
            MyAllocator(),
            CStyleLanguage(),   # reuse the C code generator
            MyCompiler(),
            MyRunner,
        )

# Register the device:
# from tinygrad.device import Device
# Device._devices["MYDEVICE"] = MyDevice
```

---

## 第 9 部分：项目

按顺序完成这些项目。每个都建立在前一个之上。

### 项目 1：张量基础与惰性求值

**目标：** 理解 tinygrad 在 `.realize()` 之前不做计算，并亲眼看到 op 融合的实际效果。

**文件：** `projects/01_tensor_basics.py`

**任务：**
1. 用所有工厂方法创建张量。检查 shape 与 dtype。
2. 设置 `DEBUG=1`。运行 `(a + b).relu().sum()`，统计跑了多少个 kernel（应为 1 —— 已融合）。
3. 设置 `DEBUG=3`。阅读生成的 kernel。在 C 代码中找出 ADD、RELU 和 SUM 操作。
4. 破坏融合：在每个 op 之后调用 `.realize()`。再统计 kernel 数量（应为 3）。比较速度。
5. 用 `.schedule()` 打印 `.realize()` 调用前后的 schedule。

**关键洞见：** op 融合是白来的性能 —— 只要在 realize 之前让计算图自由生长，tinygrad 就会自动完成融合。

---

### 项目 2：三类 op

**目标：** 通过检视 tinygrad 为每类 op 生成的内容，理解 ElementwiseOps、ReduceOps 和 MovementOps。

**文件：** `projects/02_op_types.py`

**任务：**
1. ElementwiseOps：用 `DEBUG=3` 运行 `x.relu()`、`x * 2`、`x.exp()`。在 C 输出中找出对应的操作。
2. ReduceOps：用 `DEBUG=3` 运行 `x.sum()`、`x.max(axis=0)`。观察归约 kernel 中的循环结构。
3. MovementOps：运行 `x.reshape(...)`、`x.permute(...)`、`x.expand(...)`。使用 `DEBUG=2` —— 注意单靠 movement op 会产生 **0 个 kernel**。它们是零拷贝的。
4. 混合：构建 `(x.permute(1,0) @ y.reshape(4,4)).sum(axis=1)`。用 `DEBUG=1` 统计 kernel 数量。在运行前先试着预测会是多少。
5. 不用 `@` 手动实现矩阵乘：使用 `expand` + `*` + `sum`。验证结果与 `@` 一致。

**关键洞见：** movement op 之所以免费，是因为 ShapeTracker 只改索引运算。把 movement op 融合进下游的 compute op，正是 tinygrad 避免多余内存拷贝的方式。

---

### 项目 3：从零实现 Autograd

**目标：** 理解 tinygrad 如何实现反向模式自动微分。

**文件：** `projects/03_autograd.py`

**任务：**
1. 手动验证梯度：
   - `y = x.pow(2).sum()` → `dy/dx = 2x`（用 `.grad` 验证）
   - `y = (x * w).sum()` → `dy/dw = x`（验证）
   - `y = x.sigmoid()` → `dy/dx = sigmoid(x) * (1 - sigmoid(x))`（验证）
2. 构建一个 2 层 MLP，训练它过拟合一个 10 样本的 XOR 数据集。验证 loss 降到接近零。
3. 阅读 tinygrad 源码：`tinygrad/tensor.py`，搜索 `def _broadcasted` 和 `class Function`。读 3 个 backward 函数（例如 `Mul`、`Add`、`Sum`）。为每个写一段注释说明。
4. 实现一个自定义 loss 函数：Huber loss。用有限差分 `(f(x+ε) - f(x-ε)) / 2ε` 对其梯度做数值验证。

**关键洞见：** autograd 不过是一串存放在计算图里的 `backward()` 函数。每个 op 都记录了如何把梯度反向传播回去。

---


<details>
<summary>English original</summary>

**Adding a Custom Backend (Skeleton)**

```python
# my_backend.py — minimal custom backend skeleton

from tinygrad.device import Compiled, Allocator, Compiler, Runner
from tinygrad.renderer.cstyle import CStyleLanguage

class MyAllocator(Allocator):
    def _alloc(self, size: int, options):
        # Allocate `size` bytes on your device
        # Return a handle (pointer, buffer object, etc.)
        ...

    def _free(self, buf, options):
        # Free the buffer
        ...

    def copyin(self, dst, src: memoryview):
        # Copy from host (numpy array) to device buffer
        ...

    def copyout(self, dst: memoryview, src):
        # Copy from device buffer to host (numpy array)
        ...

class MyCompiler(Compiler):
    def compile(self, src: str) -> bytes:
        # src is the generated C/kernel source as a string
        # Compile it to binary and return bytes
        ...

class MyRunner(Runner):
    def __init__(self, name: str, lib: bytes):
        # Load the compiled binary
        ...

    def __call__(self, *bufs, global_size, local_size, wait=False):
        # Execute the kernel with given buffers and launch dimensions
        ...

class MyDevice(Compiled):
    def __init__(self, device: str):
        super().__init__(
            device,
            MyAllocator(),
            CStyleLanguage(),   # reuse the C code generator
            MyCompiler(),
            MyRunner,
        )

# Register the device:
# from tinygrad.device import Device
# Device._devices["MYDEVICE"] = MyDevice
```

---

**Part 9: Projects**

Work through these in order. Each builds on the previous.

**Project 1: Tensor Basics and Lazy Evaluation**

**Goal:** Understand that tinygrad doesn't compute until `.realize()`, and see op fusion in action.

**File:** `projects/01_tensor_basics.py`

**Tasks:**
1. Create tensors with all factory methods. Check shapes and dtypes.
2. Set `DEBUG=1`. Run `(a + b).relu().sum()` and count how many kernels run (should be 1 — fused).
3. Set `DEBUG=3`. Read the generated kernel. Find the ADD, RELU, and SUM operations in the C code.
4. Break fusion: call `.realize()` after each op. Count kernels now (should be 3). Compare speed.
5. Use `.schedule()` to print the schedule before and after a `.realize()` call.

**Key insight:** Op fusion is free performance — tinygrad does it automatically when you let the graph grow before realizing.

---

**Project 2: The Three Op Types**

**Goal:** Understand ElementwiseOps, ReduceOps, and MovementOps by inspecting what tinygrad generates for each.

**File:** `projects/02_op_types.py`

**Tasks:**
1. ElementwiseOps: Run `x.relu()`, `x * 2`, `x.exp()` with `DEBUG=3`. Find the corresponding operations in the C output.
2. ReduceOps: Run `x.sum()`, `x.max(axis=0)` with `DEBUG=3`. Observe the loop structure in the reduction kernel.
3. MovementOps: Run `x.reshape(...)`, `x.permute(...)`, `x.expand(...)`. Use `DEBUG=2` — notice that movement ops alone produce **0 kernels**. They are zero-copy.
4. Mixed: Build `(x.permute(1,0) @ y.reshape(4,4)).sum(axis=1)`. Count kernels with `DEBUG=1`. Try to predict how many before running.
5. Implement matmul manually without `@`: use `expand` + `*` + `sum`. Verify it matches `@`.

**Key insight:** Movement ops are free because ShapeTracker only changes index math. Fusing movement ops into downstream compute ops is how tinygrad avoids unnecessary memory copies.

---

**Project 3: Autograd from Scratch**

**Goal:** Understand how tinygrad implements reverse-mode autodiff.

**File:** `projects/03_autograd.py`

**Tasks:**
1. Manually verify gradients:
   - `y = x.pow(2).sum()` → `dy/dx = 2x` (verify with `.grad`)
   - `y = (x * w).sum()` → `dy/dw = x` (verify)
   - `y = x.sigmoid()` → `dy/dx = sigmoid(x) * (1 - sigmoid(x))` (verify)
2. Build a 2-layer MLP and train it to overfit a 10-sample XOR dataset. Verify loss reaches near zero.
3. Read the tinygrad source: `tinygrad/tensor.py`, search for `def _broadcasted` and `class Function`. Read 3 backward functions (e.g., `Mul`, `Add`, `Sum`). Write a comment explaining each.
4. Implement a custom loss function: Huber loss. Verify its gradient numerically using finite differences `(f(x+ε) - f(x-ε)) / 2ε`.

**Key insight:** Autograd is just a chain of `backward()` functions stored in the graph. Each op records how to propagate the gradient back through it.

---

</details>

### Project 4：训练 MNIST

**目标：** 端到端训练一个真实模型。理解完整的训练循环、数据加载与评估。

**文件：** `projects/04_mnist.py`

**任务：**
1. 下载 MNIST：tinygrad 提供 `from tinygrad.nn.datasets import mnist`（或手动下载）。
2. 用 `Conv2d`、`BatchNorm`、`relu` 和 `Linear` 构建 CNN。
3. 用 Adam 训练 5 个 epoch。目标：测试准确率 >98%。
4. 用 `DEBUG=1` 剖析训练循环。每步有多少个 kernel？哪些耗时最多？
5. 启用 `BEAM=2`。它能加速训练吗？加速多少？
6. 用 `safe_save` 保存模型并重新加载。验证重新加载后准确率完全一致。

**参考：** tinygrad 源码中的 `tinygrad/examples/mnist.py` —— 动手写自己的实现之前先读它。

---

### Project 5：剖析编译器

**目标：** 理解从 Python 张量算子到 GPU kernel 的完整流水线。

**文件：** `projects/05_compiler_pipeline.py`

**任务：**
1. **Schedule 检查：** 构建一个含 5 个以上算子的计算图。调用 `.schedule()`。打印每个 kernel item 的 AST。解释为什么有些算子被融合、有些没有。
2. **UOp 树：** 设置 `VIZ=1` 并运行一次矩阵乘。在浏览器图可视化器中穿行。找出 elementwise、reduce 和 loop 节点。
3. **用 DEBUG=4 观察 IR 阶段：** 用 `DEBUG=4` 运行一个小矩阵乘。识别并描述：
   - 初始的 UOp IR（优化前）
   - 常量折叠后的 IR
   - 循环分析后的 IR
   - 最终线性化后的 IR
4. **代数重写：** 设置 `NOOPT=1` 并运行 `x * 1 + 0`。记下 kernel。去掉 `NOOPT=1` 再运行一次。观察优化后的 kernel。
5. **BEAM search：** 用 `BEAM=0` 和 `BEAM=4` 运行 1024×1024 矩阵乘。比较 runtime。BEAM 选中的分块是多大？

---

### Project 6：自定义算子与后端扩展

**目标：** 用自定义算子扩展 tinygrad，并理解后端接口。

**文件：** `projects/06_custom_ops.py`

**任务：**
1. **组合自定义算子：** 仅用原语实现下列算子（不得使用 `torch` 这类内置实现）：
   - `gelu(x)`：`x * 0.5 * (1 + (x / sqrt(2)).erf())`  —— 使用 tinygrad 的 `erf`，或对它做近似
   - `rms_norm(x)`：`x / sqrt((x*x).mean() + 1e-6)`
   - `swiglu(x, gate)`：`x * gate.sigmoid()`
2. 验证每个自定义算子在数值上与参考 PyTorch 实现一致。
3. 为 `rms_norm` 编写梯度测试：用 autograd 计算梯度，再用有限差分验证。
4. **检查它们编译成什么：** 用 `DEBUG=3` 查看每个算子对应的 kernel。统计操作数。
5. **融合与不融合：** 实现 `gelu`，一种是在每步之间加 `.realize()` 逐步执行，另一种是一次性完成。比较 kernel 数量与 runtime。

---

### Project 7：实现自定义后端

**目标：** 构建一个能运行 tinygrad 算子的可用自定义后端。

**文件：** `projects/07_custom_backend/`

这是收官项目。你要实现一个面向 **借助 ctypes 的 CPU** 的后端（比 CUDA 简单，但能讲清完整接口）。

**Part A —— Allocator：**
- 用 `ctypes.create_string_buffer` 实现 `_alloc(size)`
- 用 `ctypes.memmove` 实现 `copyin` 和 `copyout`
- 测试：分配一个 buffer，把一个 numpy 数组拷进去，再拷出来。验证往返一致。

**Part B —— Compiler：**
- 针对生成的 C 源码，以子进程方式调用 `clang` 来实现 `compile(src: str) → bytes`
- 把编译得到的 `.so` 以 bytes 返回
- 测试：编译一个简单的 C kernel `void kernel(float* a) { a[0] = 42.0f; }`，验证能构建成功

**Part C —— Runner：**
- 用 `ctypes.CDLL` 把编译得到的 `.so` bytes 加载进内存
- 实现 `__call__(*bufs, global_size, local_size)` 来调用编译出的函数
- 在 Python 中处理 global_size 循环（单线程 —— 优先保证正确性）

**Part D —— Integration：**
- 把 `MyAllocator`、`MyCompiler` 和 `MyRunner` 组装成一个 `MyDevice(Compiled)` 类
- 注册它：`Device._devices["MY"] = MyDevice`
- 运行 `Tensor([1,2,3], device="MY") + Tensor([4,5,6], device="MY")` 并得到 `[5,7,9]`

**Part E —— Benchmark：**
- 在 `MY`、`CLANG` 以及（如可用）`CUDA` 上运行 256×256 矩阵乘
- 比较 GFLOPS。记录你的 Python runtime 相对 C clang runtime 的开销。


<details>
<summary>English original</summary>

**Project 4: Training MNIST**

**Goal:** Train a real model end-to-end. Understand the full training loop, data loading, and evaluation.

**File:** `projects/04_mnist.py`

**Tasks:**
1. Download MNIST: tinygrad provides `from tinygrad.nn.datasets import mnist` (or download manually).
2. Build a CNN with `Conv2d`, `BatchNorm`, `relu`, and `Linear`.
3. Train for 5 epochs with Adam. Target: >98% test accuracy.
4. Profile the training loop with `DEBUG=1`. How many kernels per step? Which ones take the most time?
5. Enable `BEAM=2`. Does it speed up training? By how much?
6. Save the model with `safe_save` and reload it. Verify accuracy is identical after reload.

**Reference:** `tinygrad/examples/mnist.py` in the tinygrad source — read it before writing your own.

---

**Project 5: Inspecting the Compiler**

**Goal:** Understand the full pipeline from Python tensor ops to GPU kernel.

**File:** `projects/05_compiler_pipeline.py`

**Tasks:**
1. **Schedule inspection:** Build a computation graph with 5+ ops. Call `.schedule()`. Print the AST for each kernel item. Explain why some ops are fused and others aren't.
2. **UOp tree:** Set `VIZ=1` and run a matmul. Navigate the browser graph visualizer. Find the elementwise, reduce, and loop nodes.
3. **IR stages with DEBUG=4:** Run a small matmul with `DEBUG=4`. Identify and describe:
   - The initial UOp IR (before optimization)
   - The IR after constant folding
   - The IR after loop analysis
   - The final linearized IR
4. **Algebraic rewrites:** Set `NOOPT=1` and run `x * 1 + 0`. Note the kernel. Remove `NOOPT=1` and run again. Observe the optimized kernel.
5. **BEAM search:** Run a 1024×1024 matmul with `BEAM=0` and `BEAM=4`. Compare runtime. What tile size did BEAM choose?

---

**Project 6: Custom Operations and Backend Extensions**

**Goal:** Extend tinygrad with a custom op and understand the backend interface.

**File:** `projects/06_custom_ops.py`

**Tasks:**
1. **Compose custom ops:** Implement these from primitives only (no `torch`-like built-ins):
   - `gelu(x)`: `x * 0.5 * (1 + (x / sqrt(2)).erf())`  — use tinygrad's `erf` or approximate it
   - `rms_norm(x)`: `x / sqrt((x*x).mean() + 1e-6)`
   - `swiglu(x, gate)`: `x * gate.sigmoid()`
2. Verify each custom op matches a reference PyTorch implementation numerically.
3. Write gradients test for `rms_norm`: compute gradient via autograd, verify with finite differences.
4. **Inspect what they compile to:** Use `DEBUG=3` to see the kernel for each. Count operations.
5. **Fuse vs. unfused:** Implement `gelu` step-by-step with `.realize()` between each step vs. all at once. Compare kernel count and runtime.

---

**Project 7: Implement a Custom Backend**

**Goal:** Build a functional custom backend that runs tinygrad ops.

**File:** `projects/07_custom_backend/`

This is the capstone project. You'll implement a backend that targets the **CPU via ctypes** (simpler than CUDA, but teaches the full interface).

**Part A — Allocator:**
- Implement `_alloc(size)` using `ctypes.create_string_buffer`
- Implement `copyin` and `copyout` using `ctypes.memmove`
- Test: allocate a buffer, copy a numpy array in, copy it back out. Verify round-trip.

**Part B — Compiler:**
- Implement `compile(src: str) → bytes` by calling `clang` as a subprocess on the generated C source
- Return the compiled `.so` as bytes
- Test: compile a trivial C kernel `void kernel(float* a) { a[0] = 42.0f; }` and verify it builds

**Part C — Runner:**
- Load the compiled `.so` bytes into memory with `ctypes.CDLL`
- Implement `__call__(*bufs, global_size, local_size)` to invoke the compiled function
- Handle the global_size loop in Python (single-threaded — focus on correctness)

**Part D — Integration:**
- Assemble `MyAllocator`, `MyCompiler`, and `MyRunner` into a `MyDevice(Compiled)` class
- Register it: `Device._devices["MY"] = MyDevice`
- Run `Tensor([1,2,3], device="MY") + Tensor([4,5,6], device="MY")` and get `[5,7,9]`

**Part E — Benchmark:**
- Run a 256×256 matmul on `MY`, `CLANG`, and (if available) `CUDA`
- Compare GFLOPS. Document the overhead of your Python runtime vs. the C clang runtime.

---

</details>

## Part 10：阅读源码

完成项目之后，按顺序阅读 tinygrad 源码中的这些文件：

| 文件 | 它教什么 |
|------|----------------|
| `tinygrad/tensor.py` | Tensor API 与 autograd —— 面向用户的 layer |
| `tinygrad/engine/lazy.py` | LazyBuffer —— 惰性求值是如何实现的 |
| `tinygrad/engine/schedule.py` | 执行调度是如何从图构建出来的 |
| `tinygrad/codegen/uops.py` | UOp IR 定义与所有 op 类型 |
| `tinygrad/codegen/lowerer.py` | 把调度 AST 下降为 UOp IR |
| `tinygrad/codegen/linearizer.py` | 线性化 —— 把 UOp 树变成扁平 kernel 指令 |
| `tinygrad/renderer/cstyle.py` | 从线性 UOp 生成 C/CUDA/Metal 代码 |
| `tinygrad/runtime/ops_clang.py` | 最简单的后端 —— Allocator、Compiler、Runner |
| `tinygrad/runtime/ops_cuda.py` | CUDA 后端 —— 与 clang 后端对比 |
| `tinygrad/nn/__init__.py` | 标准 layer（Linear、Conv2d、BatchNorm） |

**学习方法：** 挑一个函数，加上 `print()` 语句，用 `DEBUG=4` 跑一个例子，然后从 `Tensor.add()` 一路 trace 到编译出的 C 字符串。

---

## Part 11：为 Tinygrad 做贡献

### 从哪里开始

1. **阅读 tinygrad 源码中的 `CONTRIBUTING.md`**
2. **运行测试套件：** `python -m pytest test/ -x` —— 了解都测了什么
3. **在 GitHub issue tracker 上找一个"good first issue"**

### 贡献的类型

- **新 op 或 nn layer：** 实现缺失的激活函数、损失或 layer
- **后端改进：** 为特定 GPU 架构优化 kernel 生成
- **新后端：** 添加对新设备的支持（例如 RISC-V 模拟器、自定义 FPGA）
- **Bug 修复：** 修复某个 op/dtype 组合的正确性问题
- **文档：** 编写示例或澄清现有文档

### 测试你的改动

```bash
# Run tests for a specific file
python -m pytest test/test_tensor.py -x -v

# Run tests for a specific test
python -m pytest test/test_ops.py::TestOps::test_relu -v

# Test with a specific backend
DEVICE=CLANG python -m pytest test/ -x

# Run the full CI suite (slow)
python -m pytest test/ -x --timeout=300
```

### tinygrad 的标准

核心团队对代码质量要求严格。贡献时：
- 保持改动最小 —— tinygrad 把简洁看得高于一切
- 不新增依赖
- 所有测试必须通过
- 新功能需要测试
- 代码风格：函数体内不加类型注解，注释尽量少

---

## 资源

| 资源 | 用途 |
|----------|--------------|
| [tinygrad GitHub](https://github.com/tinygrad/tinygrad) | 源码、issue、讨论 |
| [tinygrad 文档](https://tinygrad.github.io/tinygrad/) | API 参考与快速上手 |
| [tinygrad Discord](https://discord.gg/tinygrad) | 社区、答疑、贡献者帮助 |
| [tinygrad-notes](https://mesozoic-egg.github.io/tinygrad-notes/) | 社区对内部实现的深入剖析 |
| [George Hotz 直播（Twitch/YouTube）](https://www.youtube.com/@georgehotzarchive) | 直播写 tinygrad —— 看编译器如何演进 |
| [abstractions.py](https://github.com/tinygrad/tinygrad/blob/master/docs/abstractions.py) | 带注释的 tinygrad 架构走读 |
| `hacking-tinygrad.md`（本文件夹） | 用于考察内部实现的代码片段 |

---

*每个项目的可运行 Python 脚本见 `projects/` 文件夹。*


<details>
<summary>English original</summary>

**Part 10: Reading the Source**

After completing the projects, read these files in the tinygrad source in order:

| File | What It Teaches |
|------|----------------|
| `tinygrad/tensor.py` | Tensor API and autograd — the user-facing layer |
| `tinygrad/engine/lazy.py` | LazyBuffer — how lazy evaluation is implemented |
| `tinygrad/engine/schedule.py` | How the execution schedule is built from the graph |
| `tinygrad/codegen/uops.py` | UOp IR definition and all op types |
| `tinygrad/codegen/lowerer.py` | Lowering schedule AST to UOp IR |
| `tinygrad/codegen/linearizer.py` | Linearization — UOp tree to flat kernel instructions |
| `tinygrad/renderer/cstyle.py` | C/CUDA/Metal code generation from linear UOps |
| `tinygrad/runtime/ops_clang.py` | Simplest backend — Allocator, Compiler, Runner |
| `tinygrad/runtime/ops_cuda.py` | CUDA backend — compare with clang backend |
| `tinygrad/nn/__init__.py` | Standard layers (Linear, Conv2d, BatchNorm) |

**Study method:** Pick a function, add `print()` statements, run an example with `DEBUG=4`, and trace the execution path from `Tensor.add()` all the way to the compiled C string.

---

**Part 11: Contributing to Tinygrad**

**Where to Start**

1. **Read `CONTRIBUTING.md`** in the tinygrad source
2. **Run the test suite:** `python -m pytest test/ -x` — understand what's tested
3. **Find a "good first issue"** on the GitHub issue tracker

**Types of Contributions**

- **New ops or nn layers:** Implement a missing activation, loss, or layer
- **Backend improvements:** Optimize kernel generation for a specific GPU architecture
- **New backend:** Add support for a new device (e.g., RISC-V simulator, custom FPGA)
- **Bug fixes:** Fix a correctness issue with a specific op/dtype combination
- **Documentation:** Write examples or clarify existing docs

**Testing Your Changes**

```bash
# Run tests for a specific file
python -m pytest test/test_tensor.py -x -v

# Run tests for a specific test
python -m pytest test/test_ops.py::TestOps::test_relu -v

# Test with a specific backend
DEVICE=CLANG python -m pytest test/ -x

# Run the full CI suite (slow)
python -m pytest test/ -x --timeout=300
```

**The tinygrad Standard**

The core team is strict about code quality. When contributing:
- Keep changes minimal — tinygrad values simplicity above all
- No new dependencies
- All tests must pass
- New features need tests
- Code style: no type annotations in function bodies, minimal comments

---

**Resources**

| Resource | What It's For |
|----------|--------------|
| [tinygrad GitHub](https://github.com/tinygrad/tinygrad) | Source, issues, discussions |
| [tinygrad Docs](https://tinygrad.github.io/tinygrad/) | API reference and quickstart |
| [tinygrad Discord](https://discord.gg/tinygrad) | Community, Q&A, contributor help |
| [tinygrad-notes](https://mesozoic-egg.github.io/tinygrad-notes/) | Community deep-dives on internals |
| [George Hotz streams (Twitch/YouTube)](https://www.youtube.com/@georgehotzarchive) | Live coding tinygrad — watch the compiler evolve |
| [abstractions.py](https://github.com/tinygrad/tinygrad/blob/master/docs/abstractions.py) | Annotated walkthrough of tinygrad's architecture |
| `hacking-tinygrad.md` (this folder) | Code snippets for inspecting internals |

---

*See the `projects/` folder for runnable Python scripts for each project above.*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track E - Autonomous Vehicles/3. tinygrad for Inference/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20E%20-%20Autonomous%20Vehicles/3.%20tinygrad%20for%20Inference/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
