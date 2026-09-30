---
title: PyTorch 与 micrograd —— 掌握 tinygrad 的洞见
description: PyTorch 与 micrograd —— 掌握 tinygrad 的洞见
---

# PyTorch 与 micrograd —— 掌握 tinygrad 的洞见

<div class="course-identity auto-course" style="--course-accent: #dc2626; --course-accent-rgb: 220, 38, 38;" markdown="1">
<div class="course-identity__icon">PAMI</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入解析 · AI 工作负载</p>
<p class="course-identity__title">PyTorch 与 micrograd 的专属课程标识 —— 掌握 tinygrad 的洞见。</p>
<p class="course-identity__meta">产物：模型或工作负载研究 · 度量：准确率、延迟、内存、吞吐</p>
</div>
</div>


> **课程参考：** [OpenCV PyTorch Bootcamp & Deep Learning](https://courses.opencv.org/courses/course-v1:PyTorch+Bootcamp+Deep-Learning/course/)
> **micrograd 参考：** [karpathy/micrograd](https://github.com/karpathy/micrograd)
>
> **本指南为何存在：** 只有理解了 *它在解决什么问题*，tinygrad 的设计才讲得通。这意味着要理解 PyTorch 的 API（tinygrad 模仿的对象）和 micrograd 的内部实现（tinygrad 扩展的对象）。请在 tinygrad 深入解析之前阅读本文。

---

## 三框架心智模型

```
micrograd                  PyTorch                   tinygrad
──────────────────         ───────────────────        ────────────────────
Scalar values only         Full tensor engine         Full tensor engine
~150 lines Python          ~3M lines C++/Python       ~5000 lines Python
No GPU                     GPU (opaque CUDA)          GPU (readable Python)
Pure education             Production                 Hackable production

Teaches:                   Teaches:                   Teaches:
  ∙ What autograd IS         ∙ The API you'll use       ∙ What the API DOES
  ∙ Chain rule concretely    ∙ Real training loops      ∙ Lazy eval, IR, kernels
  ∙ Computational graph      ∙ CNNs, transfer learning  ∙ Custom backends
  ∙ How backward() works     ∙ Industry patterns        ∙ Compiler internals
```

**学习顺序：** micrograd → PyTorch → tinygrad

---


## 1. micrograd —— 150 行实现自动微分

### micrograd 是什么

Karpathy 的 micrograd 在 **标量层面** 实现反向模式自动微分（autograd）。每个数字都是一个 `Value` 对象，它跟踪：
- 它的数值数据
- 它的梯度（`grad`）
- 创建它的运算（`_op`）
- 创建它的输入（`_prev`）

这正是 PyTorch 的 `Tensor` 所做的事 —— 但 PyTorch 是在多维数组上用 C++ kernel 做，micrograd 则是在 Python float 上做，所以你能读懂每一行。


<details>
<summary>English original</summary>

**PyTorch and micrograd — Insight for Mastering tinygrad**

<div class="course-identity auto-course" style="--course-accent: #dc2626; --course-accent-rgb: 220, 38, 38;" markdown="1">
<div class="course-identity__icon">PAMI</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · AI Workloads</p>
<p class="course-identity__title">Specialized course identity for PyTorch and micrograd — Insight for Mastering tinygrad.</p>
<p class="course-identity__meta">Artifact: model or workload study · Measure: accuracy, latency, memory, throughput</p>
</div>
</div>


> **Course reference:** [OpenCV PyTorch Bootcamp & Deep Learning](https://courses.opencv.org/courses/course-v1:PyTorch+Bootcamp+Deep-Learning/course/)
> **micrograd reference:** [karpathy/micrograd](https://github.com/karpathy/micrograd)
>
> **Why this guide exists:** tinygrad's design makes sense only when you understand *what problem it is solving*. That means understanding PyTorch's API (what tinygrad mimics) and micrograd's internals (what tinygrad extends). Read this before the tinygrad deep-dive.

---

**The Three-Framework Mental Model**

```
micrograd                  PyTorch                   tinygrad
──────────────────         ───────────────────        ────────────────────
Scalar values only         Full tensor engine         Full tensor engine
~150 lines Python          ~3M lines C++/Python       ~5000 lines Python
No GPU                     GPU (opaque CUDA)          GPU (readable Python)
Pure education             Production                 Hackable production

Teaches:                   Teaches:                   Teaches:
  ∙ What autograd IS         ∙ The API you'll use       ∙ What the API DOES
  ∙ Chain rule concretely    ∙ Real training loops      ∙ Lazy eval, IR, kernels
  ∙ Computational graph      ∙ CNNs, transfer learning  ∙ Custom backends
  ∙ How backward() works     ∙ Industry patterns        ∙ Compiler internals
```

**Learning order:** micrograd → PyTorch → tinygrad

---


**1. micrograd — Autograd from 150 Lines**

**What micrograd is**

Karpathy's micrograd implements reverse-mode automatic differentiation (autograd) **at the scalar level**. Every number is a `Value` object that tracks:
- Its numeric data
- Its gradient (`grad`)
- What operation created it (`_op`)
- What inputs created it (`_prev`)

This is exactly what PyTorch's `Tensor` does — but PyTorch does it over multi-dimensional arrays with C++ kernels. micrograd does it over Python floats so you can read every line.

</details>

### Implement the `Value` class

```python
# micrograd.py  — the entire autograd engine in one class
import math

class Value:
    """
    A scalar value with autograd support.
    Wraps a Python float and tracks how it was computed.
    """

    def __init__(self, data, _children=(), _op='', label=''):
        self.data  = float(data)
        self.grad  = 0.0          # dL/d(self) — starts at zero
        self._op   = _op          # what operation produced this node
        self._prev = set(_children)
        self._label = label

        # _backward: function that computes gradients for this node's inputs
        # Default: leaf node, nothing to propagate back through
        self._backward = lambda: None

    # ── Forward operations ──────────────────────────────────────────────

    def __add__(self, other):
        other = other if isinstance(other, Value) else Value(other)
        out = Value(self.data + other.data, (self, other), '+')

        def _backward():
            # d(out)/d(self)  = 1   →  self.grad += 1 * out.grad
            # d(out)/d(other) = 1   →  other.grad += 1 * out.grad
            self.grad  += out.grad
            other.grad += out.grad

        out._backward = _backward
        return out

    def __mul__(self, other):
        other = other if isinstance(other, Value) else Value(other)
        out = Value(self.data * other.data, (self, other), '*')

        def _backward():
            # d(a*b)/da = b  →  self.grad  += other.data * out.grad
            # d(a*b)/db = a  →  other.grad += self.data  * out.grad
            self.grad  += other.data * out.grad
            other.grad += self.data  * out.grad

        out._backward = _backward
        return out

    def __pow__(self, exponent):
        assert isinstance(exponent, (int, float))
        out = Value(self.data ** exponent, (self,), f'**{exponent}')

        def _backward():
            # d(x^n)/dx = n * x^(n-1)
            self.grad += (exponent * self.data ** (exponent - 1)) * out.grad

        out._backward = _backward
        return out

    def relu(self):
        out = Value(max(0, self.data), (self,), 'ReLU')

        def _backward():
            # d(relu(x))/dx = 1 if x > 0 else 0
            self.grad += (1.0 if self.data > 0 else 0.0) * out.grad

        out._backward = _backward
        return out

    def tanh(self):
        t = math.tanh(self.data)
        out = Value(t, (self,), 'tanh')

        def _backward():
            # d(tanh(x))/dx = 1 - tanh(x)^2
            self.grad += (1.0 - t ** 2) * out.grad

        out._backward = _backward
        return out

    def exp(self):
        e = math.exp(self.data)
        out = Value(e, (self,), 'exp')

        def _backward():
            # d(e^x)/dx = e^x
            self.grad += e * out.grad

        out._backward = _backward
        return out

    def log(self):
        out = Value(math.log(self.data + 1e-10), (self,), 'log')

        def _backward():
            # d(ln(x))/dx = 1/x
            self.grad += (1.0 / (self.data + 1e-10)) * out.grad

        out._backward = _backward
        return out

    # ── Reverse operations (make Python operators work both ways) ───────
    def __radd__(self, other): return self + other
    def __rmul__(self, other): return self * other
    def __neg__(self):         return self * -1
    def __sub__(self, other):  return self + (-other)
    def __truediv__(self, other): return self * other**-1
    def __rtruediv__(self, other): return Value(other) * self**-1

    # ── Backward pass ────────────────────────────────────────────────────

    def backward(self):
        """
        Compute gradients for all nodes in the computational graph.

        Algorithm:
          1. Build topological order of all nodes (leaf → root)
          2. Set self.grad = 1.0  (dL/dL = 1)
          3. Walk in REVERSE topological order
          4. Call each node's _backward() to propagate gradient to its inputs
        """
        topo = []
        visited = set()

        def build_topo(node):
            if id(node) not in visited:
                visited.add(id(node))
                for child in node._prev:
                    build_topo(child)
                topo.append(node)

        build_topo(self)

        self.grad = 1.0           # seed: dL/dL = 1

        for node in reversed(topo):
            node._backward()      # propagate gradient through this op

    def __repr__(self):
        return f"Value(data={self.data:.4f}, grad={self.grad:.4f})"
```

### 手动 trace 一个完整的 backward pass

```python
# Verify against finite differences (numerical gradient check)

def numerical_gradient(f, x, h=1e-5):
    """Estimate df/dx numerically."""
    x_plus  = Value(x.data + h)
    x_minus = Value(x.data - h)
    return (f(x_plus).data - f(x_minus).data) / (2 * h)

# Test: f(x) = (x^2 + 3x + 2) * tanh(x)
x = Value(2.0)
y = (x**2 + 3*x + 2) * x.tanh()
y.backward()

print(f"Autograd gradient: {x.grad:.6f}")
print(f"Numerical gradient: {numerical_gradient(lambda v: (v**2 + 3*v + 2) * v.tanh(), x):.6f}")
# Both should match to 4+ decimal places

# Step-by-step trace — add this debugging helper
def trace_graph(root):
    """Print the computational graph."""
    nodes, edges = set(), set()
    def build(v):
        if v not in nodes:
            nodes.add(v)
            for child in v._prev:
                edges.add((child, v))
                build(child)
    build(root)
    for n in nodes:
        print(f"  {n._label or id(n)}: data={n.data:.4f} grad={n.grad:.4f} op={n._op}")

x = Value(2.0, label='x')
w = Value(-3.0, label='w')
b = Value(1.0, label='b')
z = x * w + b;  z._label = 'z'
y = z.tanh();   y._label = 'y'
y.backward()
trace_graph(y)
```

### 为什么梯度要用 `+=`（而不是 `=`）

```python
# A critical subtlety: nodes can be reused in the graph
a = Value(2.0)
b = a + a          # a appears TWICE as input to b
c = b * b
c.backward()

# When computing dc/da:
#   c = b^2         → dc/db = 2b
#   b = a + a       → db/da = 1 + 1 = 2
#   dc/da = dc/db * db/da = 2b * 2 = 4b = 4*(2+2) = 16

print(f"a.grad = {a.grad}")   # Should be 16.0
# If we used = instead of +=, second path would overwrite first → wrong answer

# This is why every _backward uses:  self.grad += ...  (not =)
```

---

## 2. 在 micrograd 上构建神经网络

### 神经元、Layer、多层感知机

```python
import random

class Neuron:
    def __init__(self, n_in, activation='tanh'):
        self.w   = [Value(random.uniform(-1, 1)) for _ in range(n_in)]
        self.b   = Value(0.0)
        self.act = activation

    def __call__(self, x):
        # z = w·x + b
        z = sum(wi * xi for wi, xi in zip(self.w, x)) + self.b
        return z.tanh() if self.act == 'tanh' else z.relu() if self.act == 'relu' else z

    def parameters(self):
        return self.w + [self.b]

class Layer:
    def __init__(self, n_in, n_out, **kwargs):
        self.neurons = [Neuron(n_in, **kwargs) for _ in range(n_out)]

    def __call__(self, x):
        return [n(x) for n in self.neurons]

    def parameters(self):
        return [p for n in self.neurons for p in n.parameters()]

class MLP:
    def __init__(self, n_in, layer_sizes):
        sizes = [n_in] + layer_sizes
        self.layers = [
            Layer(sizes[i], sizes[i+1],
                  activation='tanh' if i < len(layer_sizes)-1 else 'linear')
            for i in range(len(layer_sizes))
        ]

    def __call__(self, x):
        for layer in self.layers:
            x = layer(x)
        return x[0] if len(x) == 1 else x

    def parameters(self):
        return [p for layer in self.layers for p in layer.parameters()]

    def zero_grad(self):
        for p in self.parameters():
            p.grad = 0.0
```

### 在 XOR 上的训练循环

```python
# XOR dataset — linearly inseparable, needs hidden layer
X = [[0.0, 0.0], [0.0, 1.0], [1.0, 0.0], [1.0, 1.0]]
Y = [0.0, 1.0, 1.0, 0.0]

model = MLP(2, [4, 1])     # 2 → 4 → 1

for epoch in range(200):
    # Forward pass
    preds = [model(x) for x in X]

    # MSE loss
    loss = sum((pred - y)**2 for pred, y in zip(preds, Y)) * (1/len(Y))

    # Backward
    model.zero_grad()
    loss.backward()

    # SGD update
    lr = 0.1
    for p in model.parameters():
        p.data -= lr * p.grad

    if epoch % 20 == 0:
        print(f"Epoch {epoch:3d}: loss={loss.data:.4f}")

# Test
for x, y in zip(X, Y):
    pred = model(x)
    print(f"Input {x} → pred={pred.data:.3f}  truth={y}")
```

### micrograd 教会了我们关于 tinygrad 的什么

```
micrograd concept                tinygrad equivalent
──────────────────────           ─────────────────────────────────
Value._prev (set of inputs)      LazyBuffer.srcs (source buffers)
Value._backward (grad fn)        Tensor.grad_fn
build_topo() + reversed()        topological sort in realize()
self.grad += ...                  gradient accumulation in backward
Value.data = float               LazyBuffer → realized numpy/cuda array
```

---

## 3. PyTorch 模块 1 — 张量

### 张量创建

```python
import torch
import numpy as np

# From data
a = torch.tensor([1.0, 2.0, 3.0])          # 1D, float32
b = torch.tensor([[1, 2], [3, 4]])           # 2D, int64

# Factory functions
zeros = torch.zeros(3, 4)                   # shape [3, 4], all zeros
ones  = torch.ones(2, 3)
eye   = torch.eye(4)                         # identity matrix
rand  = torch.rand(3, 3)                     # uniform [0, 1)
randn = torch.randn(3, 3)                    # standard normal

# From numpy (shares memory — zero copy)
arr = np.array([1.0, 2.0, 3.0])
t   = torch.from_numpy(arr)                 # no copy
t[0] = 99.0
print(arr[0])   # 99.0 — same memory!

# To numpy
np_arr = t.numpy()                          # CPU only

# Device
t_gpu = t.cuda()
t_cpu = t_gpu.cpu()
device = 'cuda' if torch.cuda.is_available() else 'cpu'
t = torch.randn(3, 3, device=device)
```

### 张量运算

```python
a = torch.tensor([[1.0, 2.0], [3.0, 4.0]])
b = torch.tensor([[5.0, 6.0], [7.0, 8.0]])

# Elementwise
print(a + b)
print(a * b)        # elementwise multiply (NOT matmul)

# Matrix multiply
print(a @ b)        # matmul — preferred syntax
print(torch.mm(a, b))
print(torch.matmul(a, b))

# Reduction
print(a.sum())                  # scalar sum
print(a.sum(dim=0))             # sum over rows → shape [2]
print(a.sum(dim=1))             # sum over cols → shape [2]
print(a.mean(), a.max(), a.min())

# Shape manipulation
t = torch.randn(2, 3, 4)
print(t.shape)                  # torch.Size([2, 3, 4])
print(t.reshape(6, 4).shape)    # [6, 4]
print(t.view(2, -1).shape)      # [2, 12]  (must be contiguous)
print(t.permute(1, 0, 2).shape) # [3, 2, 4]
print(t.transpose(0, 1).shape)  # [3, 2, 4]

# Adding dimensions
x = torch.randn(3)
print(x.unsqueeze(0).shape)     # [1, 3]
print(x.unsqueeze(1).shape)     # [3, 1]
print(x[None, :].shape)         # [1, 3]  — same as unsqueeze(0)

# Broadcasting
a = torch.randn(3, 1)
b = torch.randn(1, 4)
print((a + b).shape)            # [3, 4] — broadcasts
```

### dtype 与 device 最佳实践

```python
# Always be explicit about dtype for edge/inference work
x = torch.tensor([1.0], dtype=torch.float32)   # not float64 (doubles GPU memory)
x = torch.tensor([1.0], dtype=torch.float16)   # FP16 for inference
x = torch.tensor([1],   dtype=torch.int8)      # INT8 for quantized inference

# Move to device
x = x.to(device)
x = x.to('cuda:0')             # specific GPU

# In-place ops (use with caution — breaks autograd graph)
x.add_(1.0)                    # in-place add
x.mul_(2.0)                    # in-place mul
```

---

## 4. PyTorch 模块 2 — Autograd

### PyTorch 中 autograd 的工作原理

PyTorch autograd 是 micrograd 的 `Value` 类在张量层面的等价物：

```
micrograd:               PyTorch:
Value.data       →       Tensor.data
Value.grad       →       Tensor.grad
Value._backward  →       Tensor.grad_fn (C++ function object)
Value._prev      →       grad_fn.next_functions
build_topo()     →       Engine.execute_graph()
value.backward() →       tensor.backward()
```

### requires_grad — 显式启用求导

```python
x = torch.tensor([2.0], requires_grad=True)   # track this
w = torch.tensor([3.0], requires_grad=True)
b = torch.tensor([1.0], requires_grad=False)  # don't track bias here

# Forward pass — builds computational graph
z = x * w + b
y = z ** 2
loss = y.sum()

print(loss.grad_fn)                 # MulBackward0 (or similar)
print(loss.grad_fn.next_functions)  # shows the graph structure

# Backward pass
loss.backward()

print(f"x.grad = {x.grad}")    # dL/dx
print(f"w.grad = {w.grad}")    # dL/dw

# Verify manually:
# L = (x*w + b)^2 = (2*3+1)^2 = 49
# dL/dx = 2*(x*w+b)*w = 2*7*3 = 42
# dL/dw = 2*(x*w+b)*x = 2*7*2 = 28
```

### 梯度累积 — 理解 += 模式

```python
x = torch.tensor([2.0], requires_grad=True)

# First backward
loss1 = x ** 2
loss1.backward()
print(f"After first backward:  x.grad = {x.grad}")   # 4.0

# Second backward WITHOUT zero_grad
loss2 = x ** 3
loss2.backward()
print(f"After second backward: x.grad = {x.grad}")   # 4.0 + 12.0 = 16.0 ← ACCUMULATES

# Why? In real training you batch gradients before updating weights
# Always zero gradients before each new backward:
x.grad.zero_()    # in-place zero
```

### No-grad 上下文 — 推理与验证

```python
# During inference: don't build graph (saves memory, faster)
with torch.no_grad():
    output = model(input_data)    # no grad_fn attached

# Alternative decorator
@torch.no_grad()
def predict(model, x):
    return model(x)

# Detach a tensor from graph (useful for target values in RL)
target = output.detach()    # same data, no grad tracking
```

### 自定义 autograd 函数

理解如何编写自定义 `Function`，就能确切揭示 autograd 的工作机制：

```python
class MyReLU(torch.autograd.Function):
    @staticmethod
    def forward(ctx, x):
        # ctx.save_for_backward saves tensors needed in backward
        ctx.save_for_backward(x)
        return x.clamp(min=0)   # ReLU

    @staticmethod
    def backward(ctx, grad_output):
        # grad_output: gradient flowing IN from the next layer
        x, = ctx.saved_tensors
        # Gradient of ReLU: 1 where x > 0, else 0
        grad_input = grad_output.clone()
        grad_input[x < 0] = 0
        return grad_input    # gradient flowing OUT to previous layer

# Use it exactly like a built-in operation
x = torch.tensor([-1.0, 0.5, 2.0], requires_grad=True)
y = MyReLU.apply(x)
y.sum().backward()
print(x.grad)   # tensor([0., 1., 1.])
```

---

## 5. PyTorch 模块 4 —— 深度学习基础

### nn.Module —— 构建块

每个 PyTorch 模型都是 `nn.Module` 的子类。理解它的内部机制，对理解 tinygrad 中的对应实现至关重要。

```python
import torch.nn as nn

class Linear(nn.Module):
    """Re-implement nn.Linear from scratch to understand Module."""

    def __init__(self, in_features, out_features, bias=True):
        super().__init__()
        # nn.Parameter: a tensor that is registered as a parameter
        # (included in model.parameters(), moved with .to(), saved with state_dict())
        self.weight = nn.Parameter(torch.empty(out_features, in_features))
        self.bias   = nn.Parameter(torch.empty(out_features)) if bias else None

        # Initialize (Kaiming uniform, same as PyTorch default)
        nn.init.kaiming_uniform_(self.weight, a=math.sqrt(5))
        if self.bias is not None:
            fan_in, _ = nn.init._calculate_fan_in_and_fan_out(self.weight)
            bound = 1 / math.sqrt(fan_in)
            nn.init.uniform_(self.bias, -bound, bound)

    def forward(self, x):
        # F.linear computes: x @ weight.T + bias
        return x @ self.weight.T + (self.bias if self.bias is not None else 0)

# nn.Module automatically handles:
#   .parameters()  — iterate all registered Parameter tensors
#   .to(device)    — move all parameters to device
#   .train()/.eval() — set training/eval mode (affects BatchNorm, Dropout)
#   .state_dict()  — serialize all parameters
#   .load_state_dict() — restore from checkpoint
```

### 构建一个完整的多层感知机

```python
import torch
import torch.nn as nn
import torch.nn.functional as F

class MLP(nn.Module):
    def __init__(self, in_features, hidden_sizes, out_features, dropout=0.2):
        super().__init__()
        sizes = [in_features] + hidden_sizes + [out_features]
        layers = []
        for i in range(len(sizes) - 1):
            layers.append(nn.Linear(sizes[i], sizes[i+1]))
            if i < len(sizes) - 2:     # no activation/dropout after last layer
                layers.append(nn.BatchNorm1d(sizes[i+1]))
                layers.append(nn.ReLU())
                layers.append(nn.Dropout(dropout))
        self.net = nn.Sequential(*layers)

    def forward(self, x):
        return self.net(x)

model = MLP(784, [256, 128], 10)
print(f"Parameters: {sum(p.numel() for p in model.parameters()):,}")
print(model)   # shows layer structure
```

### 损失函数

```python
# Classification
loss_fn = nn.CrossEntropyLoss()
# Input: logits [B, C] (raw scores, no softmax needed)
# Target: class indices [B] (long tensor)
logits = torch.randn(32, 10)   # batch=32, classes=10
labels = torch.randint(0, 10, (32,))
loss = loss_fn(logits, labels)

# CrossEntropyLoss internally does:
#   softmax(logits, dim=1) → log → NLLLoss
# Equivalent to:
#   F.cross_entropy(logits, labels)
#   -log_softmax(logits, dim=1).gather(1, labels.unsqueeze(1)).mean()

# Regression
loss_fn = nn.MSELoss()
preds   = torch.randn(32, 1)
targets = torch.randn(32, 1)
loss    = loss_fn(preds, targets)

# Binary classification
loss_fn = nn.BCEWithLogitsLoss()   # sigmoid inside (numerically stable)
logits  = torch.randn(32, 1)
targets = torch.randint(0, 2, (32, 1)).float()
loss    = loss_fn(logits, targets)
```

### 优化器

```python
# SGD
opt = torch.optim.SGD(model.parameters(), lr=0.01, momentum=0.9, weight_decay=1e-4)

# Adam (most common for deep learning)
opt = torch.optim.Adam(model.parameters(), lr=1e-3, betas=(0.9, 0.999), eps=1e-8)

# AdamW (Adam + decoupled weight decay — preferred for Transformers)
opt = torch.optim.AdamW(model.parameters(), lr=1e-3, weight_decay=0.01)

# Learning rate scheduler
scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(opt, T_max=100)
# or
scheduler = torch.optim.lr_scheduler.OneCycleLR(
    opt, max_lr=1e-2, steps_per_epoch=len(train_loader), epochs=30
)
```

### DataLoader

```python
from torch.utils.data import Dataset, DataLoader

class MNISTDataset(Dataset):
    def __init__(self, X, Y):
        self.X = torch.tensor(X, dtype=torch.float32)
        self.Y = torch.tensor(Y, dtype=torch.long)

    def __len__(self):
        return len(self.Y)

    def __getitem__(self, idx):
        return self.X[idx], self.Y[idx]

train_ds = MNISTDataset(X_train, Y_train)
train_dl = DataLoader(train_ds, batch_size=64, shuffle=True, num_workers=2, pin_memory=True)
# pin_memory=True: allocates batch in pinned (page-locked) CPU memory → faster GPU transfer
# num_workers=2: 2 background processes prefetch data while GPU trains
```

### 完整的训练循环

```python
def train_epoch(model, loader, loss_fn, optimizer, device):
    model.train()   # enables BatchNorm running stats update + Dropout
    total_loss, correct, total = 0, 0, 0

    for X, y in loader:
        X, y = X.to(device), y.to(device)

        # 1. Forward
        logits = model(X)
        loss   = loss_fn(logits, y)

        # 2. Backward
        optimizer.zero_grad()   # CRITICAL: clear previous gradients
        loss.backward()

        # 3. Gradient clipping (optional but important for deep nets)
        torch.nn.utils.clip_grad_norm_(model.parameters(), max_norm=1.0)

        # 4. Update
        optimizer.step()

        total_loss += loss.item()
        correct    += (logits.argmax(1) == y).sum().item()
        total      += len(y)

    return total_loss / len(loader), correct / total

@torch.no_grad()
def evaluate(model, loader, loss_fn, device):
    model.eval()   # disables Dropout, uses running stats in BatchNorm
    total_loss, correct, total = 0, 0, 0
    for X, y in loader:
        X, y = X.to(device), y.to(device)
        logits = model(X)
        total_loss += loss_fn(logits, y).item()
        correct    += (logits.argmax(1) == y).sum().item()
        total      += len(y)
    return total_loss / len(loader), correct / total

# Run training
device = 'cuda' if torch.cuda.is_available() else 'cpu'
model  = MLP(784, [256, 128], 10).to(device)
opt    = torch.optim.Adam(model.parameters(), lr=1e-3)
loss_fn = nn.CrossEntropyLoss()
scheduler = torch.optim.lr_scheduler.CosineAnnealingLR(opt, T_max=20)

for epoch in range(20):
    train_loss, train_acc = train_epoch(model, train_dl, loss_fn, opt, device)
    val_loss, val_acc     = evaluate(model, val_dl, loss_fn, device)
    scheduler.step()
    print(f"Epoch {epoch+1:2d}: train={train_acc:.3%}  val={val_acc:.3%}  lr={opt.param_groups[0]['lr']:.2e}")
```

---

## 6. PyTorch 模块 5 — 卷积神经网络

### PyTorch 中的卷积

```python
import torch.nn as nn

# nn.Conv2d(in_channels, out_channels, kernel_size, stride, padding)
conv = nn.Conv2d(3, 64, kernel_size=3, stride=1, padding=1)

# Parameter count: out_ch × in_ch × kH × kW + out_ch (bias)
params = 64 * 3 * 3 * 3 + 64    # = 1,792

# Output size formula: floor((in + 2p - k) / s) + 1
# With padding=1, kernel=3, stride=1: output = input (same padding)

# Forward through a batch
x = torch.randn(8, 3, 224, 224)   # [B, C, H, W]
y = conv(x)
print(y.shape)   # [8, 64, 224, 224]
```

### 深度可分离卷积（MobileNet 构建块）

```python
class DepthwiseSeparableConv(nn.Module):
    """
    Depthwise: one filter per channel (groups=in_channels)
    Pointwise: 1×1 conv to mix channels

    Standard conv: in_ch × out_ch × k × k MACs per position
    DSConv:        in_ch × k × k + in_ch × out_ch MACs
    Speedup:       ~8× for k=3 vs standard conv
    """
    def __init__(self, in_ch, out_ch, stride=1):
        super().__init__()
        self.depthwise  = nn.Conv2d(in_ch, in_ch, 3, stride=stride,
                                     padding=1, groups=in_ch, bias=False)
        self.pointwise  = nn.Conv2d(in_ch, out_ch, 1, bias=False)
        self.bn1 = nn.BatchNorm2d(in_ch)
        self.bn2 = nn.BatchNorm2d(out_ch)

    def forward(self, x):
        x = self.bn1(self.depthwise(x)).relu()
        x = self.bn2(self.pointwise(x)).relu()
        return x
```

### ResNet 残差块

```python
class ResBlock(nn.Module):
    """
    Skip connection: output = F(x) + x
    Lets gradient flow directly through the + operation
    → solves vanishing gradient in deep networks
    """
    def __init__(self, channels):
        super().__init__()
        self.conv1 = nn.Conv2d(channels, channels, 3, padding=1, bias=False)
        self.bn1   = nn.BatchNorm2d(channels)
        self.conv2 = nn.Conv2d(channels, channels, 3, padding=1, bias=False)
        self.bn2   = nn.BatchNorm2d(channels)

    def forward(self, x):
        residual = x
        x = self.bn1(self.conv1(x)).relu()
        x = self.bn2(self.conv2(x))
        x = x + residual     # skip connection — gradient flows directly here
        return x.relu()
```

### 为 MNIST 构建一个小型卷积神经网络

```python
class SmallCNN(nn.Module):
    def __init__(self, num_classes=10):
        super().__init__()
        self.features = nn.Sequential(
            nn.Conv2d(1, 32, 3, padding=1), nn.BatchNorm2d(32), nn.ReLU(),
            nn.MaxPool2d(2),                                    # 28→14
            nn.Conv2d(32, 64, 3, padding=1), nn.BatchNorm2d(64), nn.ReLU(),
            nn.MaxPool2d(2),                                    # 14→7
        )
        self.classifier = nn.Sequential(
            nn.Flatten(),
            nn.Linear(64 * 7 * 7, 128), nn.ReLU(), nn.Dropout(0.3),
            nn.Linear(128, num_classes)
        )

    def forward(self, x):
        return self.classifier(self.features(x))

model = SmallCNN()
x = torch.randn(32, 1, 28, 28)   # [B, C, H, W]
print(model(x).shape)             # [32, 10]

# Count parameters
total = sum(p.numel() for p in model.parameters())
print(f"Parameters: {total:,}")   # ~421K
```

---

## 7. PyTorch 模块 7 — 迁移学习

迁移学习是边缘 AI 中最重要的实用技术：取一个在数百万张图像上预训练好的模型，用一个小型数据集把它适配到你的特定任务上。

```python
import torchvision.models as models
import torch.nn as nn

# ── Strategy 1: Feature Extractor (freeze backbone) ──────────────────
# Use when: very small dataset (<1000 images)
model = models.resnet18(weights=models.ResNet18_Weights.IMAGENET1K_V1)

# Freeze all layers
for param in model.parameters():
    param.requires_grad = False

# Replace the classification head (only this will train)
in_features = model.fc.in_features   # 512 for ResNet-18
model.fc = nn.Linear(in_features, num_classes)  # new head, requires_grad=True by default

# Only head parameters will update
optimizer = torch.optim.Adam(model.fc.parameters(), lr=1e-3)

# ── Strategy 2: Fine-tuning (unfreeze backbone) ──────────────────────
# Use when: medium dataset (>5000 images), or domain shift from ImageNet
model = models.resnet18(weights=models.ResNet18_Weights.IMAGENET1K_V1)
model.fc = nn.Linear(512, num_classes)

# Lower LR for backbone (don't destroy pretrained features)
# Higher LR for head (train from scratch)
optimizer = torch.optim.Adam([
    {'params': model.layer4.parameters(),   'lr': 1e-4},
    {'params': model.layer3.parameters(),   'lr': 1e-5},
    {'params': model.fc.parameters(),       'lr': 1e-3},
], lr=1e-4)

# ── Strategy 3: Progressive unfreezing ────────────────────────────────
# Epoch 1-3:  train head only
# Epoch 4-6:  unfreeze layer4 + train
# Epoch 7-10: unfreeze layer3 + train
# This is the ULMFiT approach — prevents catastrophic forgetting

def unfreeze_layer(model, layer_name):
    for name, param in model.named_parameters():
        if layer_name in name:
            param.requires_grad = True
            print(f"Unfrozen: {name}")
```

### 面向边缘部署的模型对比

```python
# Compare accuracy vs speed vs size for transfer learning targets
import torchvision.models as models
import time

models_to_compare = {
    'ResNet-18':       (models.resnet18,       models.ResNet18_Weights.IMAGENET1K_V1),
    'MobileNetV3-S':   (models.mobilenet_v3_small,  models.MobileNet_V3_Small_Weights.IMAGENET1K_V1),
    'EfficientNet-B0': (models.efficientnet_b0, models.EfficientNet_B0_Weights.IMAGENET1K_V1),
}

x = torch.randn(1, 3, 224, 224)
for name, (fn, weights) in models_to_compare.items():
    m = fn(weights=weights).eval()
    params = sum(p.numel() for p in m.parameters())

    # Latency
    with torch.no_grad():
        for _ in range(10): m(x)   # warmup
        t0 = time.perf_counter()
        for _ in range(100): m(x)
        lat = (time.perf_counter() - t0) * 10   # ms

    print(f"{name:20s}: {params/1e6:.1f}M params, {lat:.1f}ms/frame")

# Expected (CPU):
# ResNet-18:           11.7M params, 45ms/frame
# MobileNetV3-S:        2.5M params, 12ms/frame  ← best for edge
# EfficientNet-B0:      5.3M params, 30ms/frame
```

---

## 8. PyTorch 模块 9 — 目标检测

### 从 PyTorch 内部机制理解 YOLO

```python
# The YOLO head: for each cell in the grid, predict B boxes
# Each box: [tx, ty, tw, th, objectness, class1...classN]

class YOLOHead(nn.Module):
    def __init__(self, in_channels, num_anchors, num_classes):
        super().__init__()
        self.num_anchors = num_anchors
        self.num_classes = num_classes
        # Output per cell: anchors × (5 + num_classes)
        out_ch = num_anchors * (5 + num_classes)
        self.conv = nn.Conv2d(in_channels, out_ch, 1)

    def forward(self, x):
        B, C, H, W = x.shape
        out = self.conv(x)   # [B, A*(5+C), H, W]
        # Reshape to [B, A, H, W, 5+C]
        out = out.view(B, self.num_anchors, 5 + self.num_classes, H, W)
        out = out.permute(0, 1, 3, 4, 2)   # [B, A, H, W, 5+C]

        # Apply sigmoid to tx, ty (center offset) and objectness
        out[..., :2]  = out[..., :2].sigmoid()    # tx, ty ∈ [0,1]
        out[..., 4:5] = out[..., 4:5].sigmoid()   # objectness
        out[..., 5:]  = out[..., 5:].sigmoid()    # class probs

        return out

# Load pretrained YOLOv8 with ultralytics
from ultralytics import YOLO
model = YOLO('yolov8n.pt')

# Access the underlying PyTorch model
pt_model = model.model
print(type(pt_model))   # ultralytics.nn.tasks.DetectionModel

# Export to ONNX (standard transfer for TensorRT)
model.export(format='onnx', opset=17, simplify=True)
```

### 用于微调 YOLO 的自定义数据集

```yaml
# dataset.yaml — YOLO training config
path: /data/my_dataset
train: images/train
val:   images/val

nc: 3   # number of classes
names: ['car', 'pedestrian', 'cyclist']
```

```python
from ultralytics import YOLO

model = YOLO('yolov8n.pt')   # start from pretrained nano model
results = model.train(
    data='dataset.yaml',
    epochs=50,
    imgsz=640,
    batch=16,
    device='cuda',
    lr0=0.01,
    lrf=0.01,      # final LR = lr0 * lrf
    warmup_epochs=3,
    augment=True,  # built-in mosaic, copy-paste, mixup augmentations
    save=True,
    project='runs/detect',
    name='custom_yolo',
)
```

---

## 9. 横向对比：micrograd vs PyTorch vs tinygrad

### 三个框架中的同一个 MLP

```python
# ── micrograd ────────────────────────────────────────────────────────
from micrograd import Value, MLP as MgMLP

mg_model = MgMLP(2, [4, 1])
mg_x = [Value(0.5), Value(1.0)]
mg_out = mg_model(mg_x)
mg_loss = (mg_out - Value(1.0)) ** 2
mg_loss.backward()

# ── PyTorch ──────────────────────────────────────────────────────────
import torch, torch.nn as nn

pt_model = nn.Sequential(
    nn.Linear(2, 4), nn.Tanh(),
    nn.Linear(4, 1)
)
pt_x    = torch.tensor([[0.5, 1.0]])
pt_out  = pt_model(pt_x)
pt_loss = (pt_out - 1.0) ** 2
pt_loss.backward()

# ── tinygrad ─────────────────────────────────────────────────────────
from tinygrad.tensor import Tensor
from tinygrad.nn import Linear as TgLinear

class TgMLP:
    def __init__(self):
        self.l1 = TgLinear(2, 4)
        self.l2 = TgLinear(4, 1)
    def __call__(self, x):
        return self.l2(self.l1(x).tanh())
    def parameters(self):
        return [*self.l1.weight, self.l1.bias, *self.l2.weight, self.l2.bias]

tg_model = TgMLP()
tg_x     = Tensor([[0.5, 1.0]])
tg_out   = tg_model(tg_x)
tg_loss  = (tg_out - 1.0).pow(2).mean()
tg_loss.backward()
```

### Autograd 内部机制对比

```python
# Inspect what's actually in the graph

# PyTorch: follow grad_fn chain
x = torch.tensor([2.0], requires_grad=True)
y = x ** 2 + 3 * x
print(y.grad_fn)                    # AddBackward0
print(y.grad_fn.next_functions)     # (PowBackward0, MulBackward0)
print(y.grad_fn.next_functions[0][0].next_functions)  # (AccumulateGrad,)

# tinygrad: inspect LazyBuffer
import os; os.environ['DEBUG'] = '1'   # show schedule
from tinygrad.tensor import Tensor

x = Tensor([2.0])
y = x ** 2 + x * 3
y.numpy()   # trigger computation — DEBUG=1 shows the ops

# With DEBUG=4: shows the generated GPU/CPU kernel code
os.environ['DEBUG'] = '4'
z = (x * x).sum().numpy()
```

### 梯度流可视化

```python
import torch
from torchviz import make_dot   # pip install torchviz

model = nn.Sequential(nn.Linear(4, 8), nn.ReLU(), nn.Linear(8, 2))
x = torch.randn(1, 4)
y = model(x)
loss = y.sum()

# Render the computational graph
dot = make_dot(loss, params=dict(model.named_parameters()))
dot.render('graph', format='png')
# Open graph.png to see the full backward graph with all grad_fns
```

---

## 10. PyTorch 隐藏了什么 —— tinygrad 又暴露了什么

这是精通 tinygrad 的关键一节。对每一个 PyTorch 操作，都要问：tinygrad 相应地做了什么？

### 惰性求值

```python
# PyTorch: eager execution — operations run immediately
x = torch.randn(1000, 1000)
y = x @ x                     # matmul runs NOW, result in y immediately

# tinygrad: lazy execution — operations are scheduled, not run
from tinygrad.tensor import Tensor
x = Tensor.randn(1000, 1000)
y = x.matmul(x)               # NO computation yet — y is a LazyBuffer
                               # just a description of "matmul of x and x"
result = y.numpy()             # NOW the computation runs (realize())

# Why lazy?
#   → Operations can be fused before execution
#   → Redundant operations can be eliminated
#   → The scheduler can optimize the compute graph
#   → This is how XLA (TensorFlow/JAX), tinygrad, and torch.compile all work
```

### Kernel 融合

```python
# PyTorch (without torch.compile): 3 separate GPU kernel launches
x = torch.randn(1024, 1024, device='cuda')
y = x.relu()        # launch kernel 1: relu
z = y * 2           # launch kernel 2: mul
w = z + 1           # launch kernel 3: add
# Each launch has overhead: CPU→GPU sync, memory roundtrip

# tinygrad: these FUSE into a single kernel
import os; os.environ['CUDA'] = '1'; os.environ['DEBUG'] = '4'
from tinygrad.tensor import Tensor
x = Tensor.randn(1024, 1024)
w = (x.relu() * 2 + 1).numpy()   # ONE kernel: relu+mul+add fused
# DEBUG=4 shows the generated kernel source — single loop doing all three ops

# torch.compile() (PyTorch 2.0+) also does this, but via C++ Triton
# tinygrad does it in pure Python — readable
```

### 你能读懂的 kernel 代码

```python
# tinygrad lets you inspect the generated kernel
import os
os.environ['DEBUG'] = '4'
os.environ['CPU'] = '1'

from tinygrad.tensor import Tensor

a = Tensor.randn(4, 4)
b = Tensor.randn(4, 4)
c = (a @ b).relu()
c.numpy()

# DEBUG=4 output shows something like:
# void E_4_4(float* data0, const float* data1, const float* data2) {
#   for (int i = 0; i < 4; i++) {
#     for (int j = 0; j < 4; j++) {
#       float acc = 0.0f;
#       for (int k = 0; k < 4; k++) acc += data1[i*4+k] * data2[k*4+j];
#       data0[i*4+j] = fmax(0.0f, acc);  // relu fused!
#     }
#   }
# }
# You can READ this. Try doing that with PyTorch.
```

### 内存管理：统一 vs 分离

```python
# PyTorch on GPU: CPU memory and GPU memory are separate
cpu_tensor = torch.randn(1000)
gpu_tensor = cpu_tensor.cuda()   # copy: CPU → GPU (PCIe transfer)
back       = gpu_tensor.cpu()    # copy: GPU → CPU

# tinygrad on Jetson: unified memory — no copy needed!
# (see Edge AI Optimization guide for full discussion)
# Jetson's LPDDR5 is shared between CPU and GPU
# tinygrad's cuda backend uses cuMemAllocManaged() for unified alloc

# PyTorch on Jetson: still works but you must use .cuda()/.cpu() explicitly
# tinygrad: if running on Jetson with CUDA backend, tensors are already accessible to both
```

---

## 11. 练习

### micrograd 练习

1. **验证链式法则：** 在 `Value` 类中实现 `sin(x)`。用数值方法检查梯度是否与 `cos(x)` 吻合。

2. **计算图可视化：** 用 `graphviz` 画出 `f(x, y) = (x^2 + y) * tanh(x - y)` 的计算图。调用 `f.backward()` 之后，在每个节点上标注其 `data` 和 `grad` 值。

3. **手动批处理：** 扩展 micrograd，使其支持 `Value` 的向量（一个 `Tensor1D` 类）。实现一个批前向传播，并验证梯度正确。

4. **从零推导一个 layer：** 把 `BatchNorm1d` 重新实现为一个 `Value` 级操作。证明在推理（eval 模式）期间它正确使用 running mean/variance，而非批统计量。

### PyTorch 练习

5. **复现 micrograd：** 用 PyTorch（`nn.Linear`、`torch.tanh`、Adam）实现第 2 节的 XOR 训练。验证在相同超参数下，它能在 <200 个 epoch 内收敛。

6. **自定义损失函数：** 把 `FocalLoss`（用于 RetinaNet 的目标检测）实现为一个自定义 `nn.Module`。验证它对简单样本给出更低的损失权重。

7. **梯度磁带：** 仅使用 `requires_grad`、`backward()` 和手动参数更新（`param.data -= lr * param.grad`），在 MNIST 上训练一个 2 层 MLP，不使用 `nn.Module` 或任何优化器。验证准确率 >95%。

8. **分析内存：** 用 `torch.cuda.memory_summary()` 测量训练 ResNet-18 时批大小为 16、32、64、128 的 GPU 峰值内存。把结果画出来。

### 通向 tinygrad 的桥梁

9. **把 micrograd 移植到 tinygrad 张量：** 用 tinygrad 的 `Tensor` 算子重写 `Value` 类，替代 Python 浮点数。前向传播将使用 tinygrad；反向传播则使用 tinygrad 的自动微分。

10. **阅读 tinygrad 的 Linear：** 打开 `tinygrad/nn/__init__.py`。找到 `Linear` 类。把每一行映射到其在 micrograd/PyTorch 中的对应实现。写下注释。

11. **比较 kernel：** 在 PyTorch 中（用 `torch.profiler` 分析）和在 tinygrad 中（用 `DEBUG=4`）运行同一个矩阵乘。比较生成/调用的 kernel 名称。

---

## 12. 资源


<details>
<summary>English original</summary>

**Lazy evaluation**

```python
# PyTorch: eager execution — operations run immediately
x = torch.randn(1000, 1000)
y = x @ x                     # matmul runs NOW, result in y immediately

# tinygrad: lazy execution — operations are scheduled, not run
from tinygrad.tensor import Tensor
x = Tensor.randn(1000, 1000)
y = x.matmul(x)               # NO computation yet — y is a LazyBuffer
                               # just a description of "matmul of x and x"
result = y.numpy()             # NOW the computation runs (realize())

# Why lazy?
#   → Operations can be fused before execution
#   → Redundant operations can be eliminated
#   → The scheduler can optimize the compute graph
#   → This is how XLA (TensorFlow/JAX), tinygrad, and torch.compile all work
```

**Kernel fusion**

```python
# PyTorch (without torch.compile): 3 separate GPU kernel launches
x = torch.randn(1024, 1024, device='cuda')
y = x.relu()        # launch kernel 1: relu
z = y * 2           # launch kernel 2: mul
w = z + 1           # launch kernel 3: add
# Each launch has overhead: CPU→GPU sync, memory roundtrip

# tinygrad: these FUSE into a single kernel
import os; os.environ['CUDA'] = '1'; os.environ['DEBUG'] = '4'
from tinygrad.tensor import Tensor
x = Tensor.randn(1024, 1024)
w = (x.relu() * 2 + 1).numpy()   # ONE kernel: relu+mul+add fused
# DEBUG=4 shows the generated kernel source — single loop doing all three ops

# torch.compile() (PyTorch 2.0+) also does this, but via C++ Triton
# tinygrad does it in pure Python — readable
```

**The kernel code you can read**

```python
# tinygrad lets you inspect the generated kernel
import os
os.environ['DEBUG'] = '4'
os.environ['CPU'] = '1'

from tinygrad.tensor import Tensor

a = Tensor.randn(4, 4)
b = Tensor.randn(4, 4)
c = (a @ b).relu()
c.numpy()

# DEBUG=4 output shows something like:
# void E_4_4(float* data0, const float* data1, const float* data2) {
#   for (int i = 0; i < 4; i++) {
#     for (int j = 0; j < 4; j++) {
#       float acc = 0.0f;
#       for (int k = 0; k < 4; k++) acc += data1[i*4+k] * data2[k*4+j];
#       data0[i*4+j] = fmax(0.0f, acc);  // relu fused!
#     }
#   }
# }
# You can READ this. Try doing that with PyTorch.
```

**Memory management: unified vs separate**

```python
# PyTorch on GPU: CPU memory and GPU memory are separate
cpu_tensor = torch.randn(1000)
gpu_tensor = cpu_tensor.cuda()   # copy: CPU → GPU (PCIe transfer)
back       = gpu_tensor.cpu()    # copy: GPU → CPU

# tinygrad on Jetson: unified memory — no copy needed!
# (see Edge AI Optimization guide for full discussion)
# Jetson's LPDDR5 is shared between CPU and GPU
# tinygrad's cuda backend uses cuMemAllocManaged() for unified alloc

# PyTorch on Jetson: still works but you must use .cuda()/.cpu() explicitly
# tinygrad: if running on Jetson with CUDA backend, tensors are already accessible to both
```

---

**11. Exercises**

**Micrograd exercises**

1. **Verify chain rule:** Implement `sin(x)` in the `Value` class. Check that the gradient matches `cos(x)` numerically.

2. **Computational graph visualization:** Use `graphviz` to draw the computational graph for `f(x, y) = (x^2 + y) * tanh(x - y)`. Annotate each node with its `data` and `grad` values after calling `f.backward()`.

3. **Manual batching:** Extend micrograd to support vectors of `Value`s (a `Tensor1D` class). Implement a batch forward pass and verify gradients are correct.

4. **Derive a layer from scratch:** Re-implement `BatchNorm1d` as a `Value`-level operation. Show that during inference (eval mode) it correctly uses running mean/variance instead of batch statistics.

**PyTorch exercises**

5. **Reproduce micrograd:** Implement the XOR training from Section 2 using PyTorch (`nn.Linear`, `torch.tanh`, Adam). Verify it converges in <200 epochs with identical hyperparameters.

6. **Custom loss function:** Implement `FocalLoss` (used in RetinaNet for object detection) as a custom `nn.Module`. Verify it gives lower loss weight to easy examples.

7. **Gradient tape:** Using only `requires_grad`, `backward()`, and manual parameter updates (`param.data -= lr * param.grad`), train a 2-layer MLP on MNIST without using `nn.Module` or any optimizer. Verify >95% accuracy.

8. **Profile memory:** Use `torch.cuda.memory_summary()` to measure peak GPU memory for batch sizes 16, 32, 64, 128 during training a ResNet-18. Plot the result.

**Bridge to tinygrad**

9. **Port micrograd to tinygrad tensors:** Rewrite the `Value` class using tinygrad `Tensor` ops instead of Python floats. The forward pass will use tinygrad; the backward pass is tinygrad's autograd.

10. **Read tinygrad's Linear:** Open `tinygrad/nn/__init__.py`. Find the `Linear` class. Map each line to its micrograd/PyTorch equivalent. Write annotations.

11. **Compare kernels:** Run the same matmul in PyTorch (profile with `torch.profiler`) and in tinygrad with `DEBUG=4`. Compare the generated/called kernel names.

---

**12. Resources**

</details>

### micrograd
- **karpathy/micrograd** — github.com/karpathy/micrograd：150 行 — 逐行读完
- **"The spelled-out intro to neural networks and backpropagation"**（Karpathy，YouTube，2h25m）：现场逐步构建 micrograd。读源码之前先看。
- **"Neural Networks: Zero to Hero"**（Karpathy 系列）：micrograd → makemore → GPT。现有最好的 ML 教育系列。

### PyTorch（与 OpenCV 课程对齐）
- **OpenCV PyTorch Bootcamp** — courses.opencv.org：本指南的参考课程。覆盖模块 1–10，配有 notebook 和作业。
- **Official PyTorch Tutorials** — pytorch.org/tutorials：尤其是 "Learning PyTorch with Examples" 和 "Writing Custom Datasets, DataLoaders and Transforms"
- **PyTorch Documentation** — pytorch.org/docs/stable：`torch.autograd`、`torch.nn`、`torch.optim`
- **"Deep Learning with PyTorch"**（Eli Stevens 等，Manning）：免费 PDF 见 pytorch.org/deep-learning-with-pytorch

### 与 tinygrad 的关联
- **tinygrad/tensor.py** — 这个 1000 行的文件替代了 micrograd 的 `Value` + PyTorch 的 `Tensor`
- **"tinygrad: a simple and powerful nn/ml framework"** — geohot 的原始博客文章
- **DEBUG=1,2,3,4** 环境变量：最好的 tinygrad 教程就是不断提高 debug 级别运行它并阅读输出

### 补充数学
- **"Matrix Calculus for Deep Learning"**（Parr & Howard）：在链式法则的标量（micrograd）与矩阵/张量微积分（PyTorch/tinygrad）之间架起数学桥梁
- **3Blue1Brown "Essence of Calculus"**：15 分钟直观讲清链式法则

---

*父级：[神经网络](../Guide.md)*
*下一节：[tinygrad 深入剖析](../../../Phase 5 - Advanced Topics and Specialization/Track E - Autonomous Vehicles/3. tinygrad for Inference/Guide.md)*


<details>
<summary>English original</summary>

**micrograd**
- **karpathy/micrograd** — github.com/karpathy/micrograd: 150 lines — read every line
- **"The spelled-out intro to neural networks and backpropagation"** (Karpathy, YouTube, 2h25m): walks through building micrograd live. Watch before reading the source.
- **"Neural Networks: Zero to Hero"** (Karpathy series): micrograd → makemore → GPT. Best ML education series available.

**PyTorch (OpenCV Course aligned)**
- **OpenCV PyTorch Bootcamp** — courses.opencv.org: the reference course for this guide. Covers Modules 1–10 with notebooks and assignments.
- **Official PyTorch Tutorials** — pytorch.org/tutorials: especially "Learning PyTorch with Examples" and "Writing Custom Datasets, DataLoaders and Transforms"
- **PyTorch Documentation** — pytorch.org/docs/stable: `torch.autograd`, `torch.nn`, `torch.optim`
- **"Deep Learning with PyTorch"** (Eli Stevens et al., Manning): free PDF at pytorch.org/deep-learning-with-pytorch

**tinygrad connection**
- **tinygrad/tensor.py** — the 1000-line file that replaces micrograd's `Value` + PyTorch's `Tensor`
- **"tinygrad: a simple and powerful nn/ml framework"** — geohot's original blog post
- **DEBUG=1,2,3,4** environment variable: the best tinygrad tutorial is running it with increasing debug levels and reading the output

**Supplementary math**
- **"Matrix Calculus for Deep Learning"** (Parr & Howard): the mathematical bridge between chain rule scalars (micrograd) and matrix/tensor calculus (PyTorch/tinygrad)
- **3Blue1Brown "Essence of Calculus"**: chain rule in 15 minutes, visually

---

*Parent: [Neural Networks](../Guide.md)*
*Next: [tinygrad deep dive](../../../Phase 5 - Advanced Topics and Specialization/Track E - Autonomous Vehicles/3. tinygrad for Inference/Guide.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/2. Deep Learning Frameworks/micrograd/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/2.%20Deep%20Learning%20Frameworks/micrograd/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
