---
title: 神经网络
description: 神经网络
---

# 神经网络

<div class="course-identity neural-networks" markdown="1">
<div class="course-identity__icon">NN</div>
<div markdown="1">
<p class="course-identity__eyebrow">模块 1 · 神经网络</p>
<p class="course-identity__title">从张量、反向传播、卷积神经网络、Transformer 和训练流程建立工作负载直觉。</p>
<p class="course-identity__meta">产物：模型实现笔记 · 度量：损失、内存、FLOPs、吞吐</p>
</div>
</div>


**阶段 3 — 人工智能**（在 **[阶段 1 §4 — C++ 与并行计算](../../Phase 1 - Foundational Knowledge/4. C++ and Parallel Computing/Guide.md)** 之后）。*如果更倾向先学嵌入式，可在阶段 2 之后选读。*

> **目标：** 构建对 AI 是什么、人工神经网络是什么、它们如何学习，以及如何使用 **tinygrad** 动手实现所有内容的具体、从零开始的理解 — 这个极简 ML 框架暴露了每一个基础操作。

**相关（单独主题）：** **[边缘 AI](../03-硬件与边缘AI/04-边缘AI与模型优化/Guide.md)** — 模型在端侧运行之处，延迟/隐私层级，训练 → 优化 → 部署。

---

## 深入子文件夹

| 子文件夹 | 描述 |
|-----------|-------------|
| [pytorch-and-micrograd/](../2. Deep Learning Frameworks/micrograd/Guide.md) | 用 micrograd 从零构建 autograd，掌握 PyTorch，然后衔接到 tinygrad — 深入掌握 tinygrad 的必要前置要求 |

---

## 1. 训练 vs 边缘

本指南聚焦 **训练和推理数学**（通常在具备足够 RAM 和 FLOPs 的工作站或云上）。**端侧约束**—MCU vs SBC vs Jetson、延迟、隐私，以及完整的 训练 → 量化 → 部署 循环—位于 **[边缘 AI](../03-硬件与边缘AI/04-边缘AI与模型优化/Guide.md)**。你可以在下文各节之后阅读该方向，或者先略读它以获取动机。

---

## 2. 什么是人工智能？

### 核心思想

经典编程：
```
Rules + Data → Program → Output
```

机器学习（当今占主导的 AI 方法）：
```
Data + Output (labels) → Learning Algorithm → Rules (model weights)
```

你不编写规则。算法从示例中**学习**规则。

### 机器学习的类型

```
Supervised Learning:
  Input: labeled pairs (image, "cat") (image, "dog")
  Learns: mapping from inputs to labels
  Examples: image classification, speech recognition, fraud detection

Unsupervised Learning:
  Input: unlabeled data
  Learns: hidden structure (clusters, patterns)
  Examples: customer segmentation, anomaly detection

Reinforcement Learning:
  Agent learns by trial and error in an environment
  Reward signal guides behavior
  Examples: game playing (AlphaGo), robot locomotion, autonomous driving
```

### 神经网络适合何处

神经网络是一**族监督（和自监督）学习算法**，对以下任务尤其强大：
- 图像和视频（卷积神经网络）
- 文本和序列（Transformer、RNN）
- 音频（卷积神经网络 + Transformer）
- 表格数据（多层感知机）
- 图（GNN）

它们主导现代 AI，因为只要有足够的数据和算力，它们就能学习**任意复杂的映射**。

---

## 3. 什么是神经网络？

### 生物学启发

大脑包含约 860 亿个神经元。每个神经元：
- 通过**树突**接收来自其他神经元的电信号
- 对信号求和
- 如果和超过**阈值**，它就**发放**（向其他神经元发送信号），经由**轴突**
- 神经元之间连接的强度称为**突触权重**

学习 = 改变突触权重。

### 人工类比

```
Biological Neuron          Artificial Neuron
─────────────────          ─────────────────
Dendrites                  Inputs  x₁, x₂, ..., xₙ
Synaptic weights           Weights w₁, w₂, ..., wₙ
Cell body summation        z = Σ(wᵢ · xᵢ) + b
Firing threshold           Activation function: a = f(z)
Axon output                Output a
```

### 神经元的网络

神经元被组织成**层**：

```
Input Layer     Hidden Layers      Output Layer
    x₁  ─────→  [neuron]  ─────→
    x₂  ─────→  [neuron]  ─────→  [neuron] → ŷ
    x₃  ─────→  [neuron]  ─────→
               [neuron]
```

- **输入层**：原始特征（像素值、传感器读数等）
- **隐藏层**：学习中间表示
- **输出层**：最终预测（类别概率、回归值）

**深度学习**中的“深度” = 许多隐藏层。

---

## 4. 神经元：构建模块

### 数学定义

给定输入 **x** = [x₁, x₂, ..., xₙ]：

```
Step 1 — Linear combination (weighted sum):
  z = w₁x₁ + w₂x₂ + ... + wₙxₙ + b
    = w·x + b          (dot product notation)

Step 2 — Activation:
  a = f(z)             (apply nonlinearity)
```

其中：
- **w** = 权重向量（学习到的参数）
- **b** = 偏置（学习到的标量偏移）
- **f** = 激活函数


<details>
<summary>English original</summary>

**Neural Networks**

<div class="course-identity neural-networks" markdown="1">
<div class="course-identity__icon">NN</div>
<div markdown="1">
<p class="course-identity__eyebrow">Module 1 · Neural Networks</p>
<p class="course-identity__title">Build workload intuition from tensors, backprop, CNNs, transformers, and training flow.</p>
<p class="course-identity__meta">Artifact: model implementation note · Measure: loss, memory, FLOPs, throughput</p>
</div>
</div>


**Phase 3 — Artificial Intelligence** (after **[Phase 1 §4 — C++ and Parallel Computing](../../Phase 1 - Foundational Knowledge/4. C++ and Parallel Computing/Guide.md)**). *Optional after Phase 2 if you prefer embedded first.*

> **Goal:** Build a concrete, ground-up understanding of what AI is, what artificial neural networks are, how they learn, and how to implement everything hands-on using **tinygrad** — the minimal ML framework that exposes every fundamental operation.

**Related (separate topic):** **[Edge AI](../03-硬件与边缘AI/04-边缘AI与模型优化/Guide.md)** — where models run on-device, latency/privacy tiers, train → optimize → deploy.

---

**Deep Dive Subfolders**

| Subfolder | Description |
|-----------|-------------|
| [pytorch-and-micrograd/](../2. Deep Learning Frameworks/micrograd/Guide.md) | Build autograd from scratch with micrograd, master PyTorch, then bridge to tinygrad — the essential prerequisite for deep tinygrad mastery |

---

**1. Training vs the edge**

This guide focuses on **training and inference math** (usually on a workstation or cloud with enough RAM and FLOPs). **On-device constraints**—MCU vs SBC vs Jetson, latency, privacy, and the full train → quantize → deploy loop—live in **[Edge AI](../03-硬件与边缘AI/04-边缘AI与模型优化/Guide.md)**. You can read that track after the sections below, or skim it first for motivation.

---

**2. What is Artificial Intelligence?**

**The Core Idea**

Classical programming:
```
Rules + Data → Program → Output
```

Machine learning (the dominant AI approach today):
```
Data + Output (labels) → Learning Algorithm → Rules (model weights)
```

You don't write the rules. The algorithm **learns** the rules from examples.

**Types of Machine Learning**

```
Supervised Learning:
  Input: labeled pairs (image, "cat") (image, "dog")
  Learns: mapping from inputs to labels
  Examples: image classification, speech recognition, fraud detection

Unsupervised Learning:
  Input: unlabeled data
  Learns: hidden structure (clusters, patterns)
  Examples: customer segmentation, anomaly detection

Reinforcement Learning:
  Agent learns by trial and error in an environment
  Reward signal guides behavior
  Examples: game playing (AlphaGo), robot locomotion, autonomous driving
```

**Where Neural Networks Fit**

Neural networks are a **family of supervised (and self-supervised) learning algorithms** that are especially powerful for:
- Images and video (CNNs)
- Text and sequences (Transformers, RNNs)
- Audio (CNNs + Transformers)
- Tabular data (MLP)
- Graphs (GNNs)

They dominate modern AI because they can learn **arbitrary complex mappings** given enough data and compute.

---

**3. What is a Neural Network?**

**Biological Inspiration**

The brain contains ~86 billion neurons. Each neuron:
- Receives electrical signals from other neurons via **dendrites**
- Sums up the signals
- If the sum exceeds a **threshold**, it **fires** (sends a signal to others) via the **axon**
- The strength of connections between neurons is called **synaptic weight**

Learning = changing synaptic weights.

**The Artificial Analogy**

```
Biological Neuron          Artificial Neuron
─────────────────          ─────────────────
Dendrites                  Inputs  x₁, x₂, ..., xₙ
Synaptic weights           Weights w₁, w₂, ..., wₙ
Cell body summation        z = Σ(wᵢ · xᵢ) + b
Firing threshold           Activation function: a = f(z)
Axon output                Output a
```

**A Network of Neurons**

Neurons are organized into **layers**:

```
Input Layer     Hidden Layers      Output Layer
    x₁  ─────→  [neuron]  ─────→
    x₂  ─────→  [neuron]  ─────→  [neuron] → ŷ
    x₃  ─────→  [neuron]  ─────→
               [neuron]
```

- **Input layer**: raw features (pixel values, sensor readings, etc.)
- **Hidden layers**: learn intermediate representations
- **Output layer**: final prediction (class probabilities, regression value)

The "deep" in **deep learning** = many hidden layers.

---

**4. The Neuron: Building Block**

**Mathematical Definition**

Given inputs **x** = [x₁, x₂, ..., xₙ]:

```
Step 1 — Linear combination (weighted sum):
  z = w₁x₁ + w₂x₂ + ... + wₙxₙ + b
    = w·x + b          (dot product notation)

Step 2 — Activation:
  a = f(z)             (apply nonlinearity)
```

Where:
- **w** = weight vector (learned parameters)
- **b** = bias (learned scalar offset)
- **f** = activation function

</details>

### 为什么需要偏置？

没有偏置时，决策边界必须过原点。有了偏置：
```
z = wx + b
```
偏置会平移激活值，使网络能够学习在何种情况下激活，而与输入幅度无关。可以把它看作神经元的「默认激活水平」。

### 具体示例

单个神经元判断肿瘤是否为恶性：
```
Inputs: x₁ = tumor size (cm), x₂ = patient age (years)
Weights: w₁ = 0.8, w₂ = 0.3
Bias: b = -2.0

z = 0.8 × 3.5 + 0.3 × 45 + (-2.0)
  = 2.8 + 13.5 - 2.0
  = 14.3

a = sigmoid(14.3) ≈ 0.9999  → malignant (high probability)
```

---

## 5. 激活函数

没有激活函数时，一堆线性层会塌缩成单个线性函数。**非线性**才赋予神经网络表达能力。

### Sigmoid

```
σ(z) = 1 / (1 + e^(-z))

Range: (0, 1)
Use: binary classification output
Problem: vanishing gradients for large |z|
```

```
σ(z)
 1.0 |          ___________
 0.5 |        /
 0.0 |_______/
      -5   0   5     z
```

### Tanh

```
tanh(z) = (e^z - e^(-z)) / (e^z + e^(-z))

Range: (-1, 1)
Use: hidden layers (historically), RNNs
Better than sigmoid: zero-centered
Problem: still has vanishing gradient
```

### ReLU（Rectified Linear Unit）

```
ReLU(z) = max(0, z)

Range: [0, ∞)
Use: default for hidden layers in modern networks
Advantage: no vanishing gradient for z > 0, computationally fast
Problem: "dying ReLU" — neurons stuck at 0 if z always negative
```

```
ReLU(z)
  |          /
  |         /
  |        /
  |_______/
  -2  -1  0  1  2   z
```

### Leaky ReLU

```
LeakyReLU(z) = z        if z > 0
              = 0.01·z   if z ≤ 0

Fixes dying ReLU by allowing small negative gradient
```

### GELU（Gaussian Error Linear Unit）

```
GELU(z) ≈ z · σ(1.702·z)

Used in: Transformers (BERT, GPT)
Smooth approximation of ReLU
```

### Softmax（多分类的输出层）

```
softmax(z)ᵢ = e^(zᵢ) / Σⱼ e^(zⱼ)

Converts raw scores to probabilities that sum to 1
Use: multi-class classification output
```

### 该用哪个？

| 层类型       | 推荐激活函数 |
|------------------|------------------------|
| 隐藏层（MLP/CNN） | ReLU 或 GELU           |
| 输出层（二分类）  | Sigmoid                |
| 输出层（多分类）   | Softmax                |
| 输出层（回归） | 无（线性）       |
| RNN 门控        | Tanh + Sigmoid         |

---

## 6. 多层感知机（MLP）

### 架构

含 2 个隐藏层的 MLP：

```
Input        Hidden 1      Hidden 2      Output
(3 neurons)  (4 neurons)   (4 neurons)   (2 neurons)

  x₁ ─┐
  x₂ ─┼──→ [h₁¹]          [h₁²]
  x₃ ─┘    [h₂¹] ──────→  [h₂²] ──────→ [o₁]
            [h₃¹]          [h₃²]          [o₂]
            [h₄¹]          [h₄²]
```

### 矩阵形式

对于有 n 个输入、m 个神经元的层：

```
Z = X @ W + b

X: input matrix    shape [batch_size, n_inputs]
W: weight matrix   shape [n_inputs, n_neurons]
b: bias vector     shape [n_neurons]
Z: output          shape [batch_size, n_neurons]

Then: A = f(Z)   (apply activation element-wise)
```

这一次矩阵乘法就计算了**一层中的全部神经元**——在 GPU 上高度可并行。

### 参数量

```
Layer (n_in → n_out):
  Weights: n_in × n_out
  Biases:  n_out
  Total:   n_in × n_out + n_out

Example MLP: 784 → 256 → 128 → 10
  Layer 1: 784×256 + 256 = 201,216
  Layer 2: 256×128 + 128 = 32,896
  Layer 3: 128×10  + 10  = 1,290
  Total:   235,402 parameters
```

---

## 7. 前向传播：做出预测

前向传播逐层计算给定输入的输出。

### 分步过程（2 层 MLP）

```python
# Pseudocode
def forward(x):
    # Layer 1
    z1 = x @ W1 + b1       # linear
    a1 = relu(z1)           # activation

    # Layer 2
    z2 = a1 @ W2 + b2      # linear
    a2 = relu(z2)           # activation

    # Output layer
    z3 = a2 @ W3 + b3      # linear
    output = softmax(z3)    # probabilities

    return output
```

### 网络逐层学到了什么

以图像分类（MNIST 数字）为例：
```
Layer 1 (edges):    detects horizontal, vertical, diagonal edges
Layer 2 (shapes):   combines edges into curves, corners
Layer 3 (parts):    detects parts of digits (loops, lines)
Output (class):     combines parts into digit predictions
```

每一层都学到输入的越来越**抽象的表征**。

---

## 8. 损失函数：度量误差

损失（或代价）函数度量**模型的预测错得有多厉害**。训练 = 最小化损失。

### 均方误差（MSE）——回归

```
MSE = (1/N) Σᵢ (yᵢ - ŷᵢ)²

yᵢ  = true value
ŷᵢ  = predicted value
N   = number of samples

Good for: predicting continuous values (house price, temperature)
```


<details>
<summary>English original</summary>

**Why the Bias?**

Without bias, the decision boundary must pass through the origin. With bias:
```
z = wx + b
```
The bias shifts the activation, allowing the network to learn when to activate regardless of input magnitude. Think of it as the neuron's "default activation level."

**Concrete Example**

A single neuron classifying whether a tumor is malignant:
```
Inputs: x₁ = tumor size (cm), x₂ = patient age (years)
Weights: w₁ = 0.8, w₂ = 0.3
Bias: b = -2.0

z = 0.8 × 3.5 + 0.3 × 45 + (-2.0)
  = 2.8 + 13.5 - 2.0
  = 14.3

a = sigmoid(14.3) ≈ 0.9999  → malignant (high probability)
```

---

**5. Activation Functions**

Without activation functions, a stack of linear layers collapses to a single linear function. **Nonlinearity** is what gives neural networks their expressive power.

**Sigmoid**

```
σ(z) = 1 / (1 + e^(-z))

Range: (0, 1)
Use: binary classification output
Problem: vanishing gradients for large |z|
```

```
σ(z)
 1.0 |          ___________
 0.5 |        /
 0.0 |_______/
      -5   0   5     z
```

**Tanh**

```
tanh(z) = (e^z - e^(-z)) / (e^z + e^(-z))

Range: (-1, 1)
Use: hidden layers (historically), RNNs
Better than sigmoid: zero-centered
Problem: still has vanishing gradient
```

**ReLU (Rectified Linear Unit)**

```
ReLU(z) = max(0, z)

Range: [0, ∞)
Use: default for hidden layers in modern networks
Advantage: no vanishing gradient for z > 0, computationally fast
Problem: "dying ReLU" — neurons stuck at 0 if z always negative
```

```
ReLU(z)
  |          /
  |         /
  |        /
  |_______/
  -2  -1  0  1  2   z
```

**Leaky ReLU**

```
LeakyReLU(z) = z        if z > 0
              = 0.01·z   if z ≤ 0

Fixes dying ReLU by allowing small negative gradient
```

**GELU (Gaussian Error Linear Unit)**

```
GELU(z) ≈ z · σ(1.702·z)

Used in: Transformers (BERT, GPT)
Smooth approximation of ReLU
```

**Softmax (Output Layer for Multi-class)**

```
softmax(z)ᵢ = e^(zᵢ) / Σⱼ e^(zⱼ)

Converts raw scores to probabilities that sum to 1
Use: multi-class classification output
```

**Which to Use?**

| Layer Type       | Recommended Activation |
|------------------|------------------------|
| Hidden (MLP/CNN) | ReLU or GELU           |
| Output (binary)  | Sigmoid                |
| Output (multi)   | Softmax                |
| Output (regression) | None (linear)       |
| RNN gates        | Tanh + Sigmoid         |

---

**6. The Multi-Layer Perceptron (MLP)**

**Architecture**

An MLP with 2 hidden layers:

```
Input        Hidden 1      Hidden 2      Output
(3 neurons)  (4 neurons)   (4 neurons)   (2 neurons)

  x₁ ─┐
  x₂ ─┼──→ [h₁¹]          [h₁²]
  x₃ ─┘    [h₂¹] ──────→  [h₂²] ──────→ [o₁]
            [h₃¹]          [h₃²]          [o₂]
            [h₄¹]          [h₄²]
```

**Matrix Form**

For a layer with n inputs and m neurons:

```
Z = X @ W + b

X: input matrix    shape [batch_size, n_inputs]
W: weight matrix   shape [n_inputs, n_neurons]
b: bias vector     shape [n_neurons]
Z: output          shape [batch_size, n_neurons]

Then: A = f(Z)   (apply activation element-wise)
```

This single matrix multiplication computes **all neurons in a layer at once** — highly parallelizable on GPU.

**Parameter Count**

```
Layer (n_in → n_out):
  Weights: n_in × n_out
  Biases:  n_out
  Total:   n_in × n_out + n_out

Example MLP: 784 → 256 → 128 → 10
  Layer 1: 784×256 + 256 = 201,216
  Layer 2: 256×128 + 128 = 32,896
  Layer 3: 128×10  + 10  = 1,290
  Total:   235,402 parameters
```

---

**7. Forward Pass: Making a Prediction**

The forward pass computes the output for a given input, layer by layer.

**Step-by-Step (2-layer MLP)**

```python
# Pseudocode
def forward(x):
    # Layer 1
    z1 = x @ W1 + b1       # linear
    a1 = relu(z1)           # activation

    # Layer 2
    z2 = a1 @ W2 + b2      # linear
    a2 = relu(z2)           # activation

    # Output layer
    z3 = a2 @ W3 + b3      # linear
    output = softmax(z3)    # probabilities

    return output
```

**What the Network Learns Layer by Layer**

For image classification (MNIST digits):
```
Layer 1 (edges):    detects horizontal, vertical, diagonal edges
Layer 2 (shapes):   combines edges into curves, corners
Layer 3 (parts):    detects parts of digits (loops, lines)
Output (class):     combines parts into digit predictions
```

Each layer learns increasingly **abstract representations** of the input.

---

**8. Loss Functions: Measuring Error**

The loss (or cost) function measures **how wrong the model's predictions are**. Training = minimizing loss.

**Mean Squared Error (MSE) — Regression**

```
MSE = (1/N) Σᵢ (yᵢ - ŷᵢ)²

yᵢ  = true value
ŷᵢ  = predicted value
N   = number of samples

Good for: predicting continuous values (house price, temperature)
```

</details>

### Binary Cross-Entropy — 二分类

```
BCE = -(1/N) Σᵢ [yᵢ log(ŷᵢ) + (1-yᵢ) log(1-ŷᵢ)]

yᵢ ∈ {0, 1}       true label
ŷᵢ ∈ (0, 1)       predicted probability (after sigmoid)

Intuition: penalizes confident wrong predictions heavily
  Predicted 0.99 when truth is 0 → very high loss
  Predicted 0.5  when truth is 0 → moderate loss
```

### Categorical Cross-Entropy — 多分类

```
CCE = -(1/N) Σᵢ Σₖ yᵢₖ log(ŷᵢₖ)

yᵢₖ = 1 if sample i belongs to class k, else 0  (one-hot)
ŷᵢₖ = predicted probability for class k (after softmax)

Most common loss for image classification
```

### 为什么用交叉熵？

交叉熵来自信息论。最小化它等价于**最大似然估计** — 即找到让观测数据最可能出现的参数。它按模型对正确答案的意外程度成比例地惩罚模型。

---

## 9. 反向传播：网络如何学习

### 核心思想

Backpropagation = 把**微积分链式法则**用于计算损失对网络中每个权重的梯度。

```
∂Loss/∂W = how much does the loss change when W changes slightly?

We want: decrease the loss
Strategy: move W in the direction that decreases Loss
Update:   W ← W - α · (∂Loss/∂W)
```

### 链式法则基础

对于复合函数 f(g(x))：
```
df/dx = (df/dg) · (dg/dx)
```

在神经网络中，损失流经许多复合函数：
```
Loss = L(softmax(W₃ · relu(W₂ · relu(W₁ · x + b₁) + b₂) + b₃))
```

Backprop 从输出到输入**反向**应用链式法则。

### 计算图

把网络看作一张运算图：

```
x → [W₁ matmul] → z₁ → [relu] → a₁ → [W₂ matmul] → z₂ → [loss] → L
```

前向传播：计算并**缓存**每一个中间值。
反向传播：利用缓存值 + 链式法则计算每个节点处的梯度。

### 逐步反向传播（单层示例）

```
Forward:
  z = x·w + b
  a = relu(z)
  L = MSE(a, y)

Backward (chain rule):
  dL/da = 2(a - y)/N                    (MSE gradient)
  dL/dz = dL/da · d(relu)/dz           (relu gradient: 1 if z>0 else 0)
  dL/dw = dL/dz · x                    (matmul gradient)
  dL/db = dL/dz · 1                    (bias gradient)
  dL/dx = dL/dz · w                    (input gradient, for prev layer)
```

### 自动微分（Autograd）

现代框架（包括 tinygrad）实现了 **autograd**：在前向传播期间构建计算图，然后在 `.backward()` 期间自动计算所有梯度。

你从不需要手工实现 backprop — 只需定义前向传播。

```python
# tinygrad autograd example
x = Tensor([2.0], requires_grad=True)
w = Tensor([3.0], requires_grad=True)
b = Tensor([1.0], requires_grad=True)

z = x * w + b          # forward: builds graph
loss = z.pow(2).mean() # loss computation

loss.backward()         # backward: compute all gradients

print(w.grad)  # dL/dw computed automatically
```

### 梯度消失与梯度爆炸

**梯度消失**：在深层网络中，梯度反向传播穿过许多层后会变得极小。
- 原因：sigmoid/tanh 饱和（导数 ≈ 0）
- 解决：ReLU、残差连接（ResNet）、批归一化

**梯度爆炸**：梯度呈指数增长。
- 原因：大权重 × 多层
- 解决：梯度裁剪、权重初始化方案

---

## 10. 梯度下降与优化器

### 梯度下降（基础算法）

```
For each training step:
  1. Compute loss on a batch of data
  2. Compute gradients via backprop
  3. Update every parameter:
     W ← W - α · ∂Loss/∂W

α = learning rate (hyperparameter, typically 1e-3 to 1e-4)
```

### 批处理变体

| 变体               | 批大小        | 优点                      | 缺点                        |
|-------------------|---------------|---------------------------|-----------------------------|
| 批 GD             | 全数据集      | 梯度稳定                  | 慢、内存密集                |
| 随机 GD（SGD）    | 1 个样本      | 更新快                    | 噪声很大                    |
| 小批量 GD         | 32–256        | 两者兼顾                  | 业界标准                    |

### 带动量的 SGD

```
vₜ = β·vₜ₋₁ + (1-β)·∇L     (momentum term, β ≈ 0.9)
W ← W - α·vₜ

Intuition: gradient as a ball rolling downhill, accumulates velocity
Benefit: faster convergence, escapes shallow local minima
```

### Adam（自适应矩估计）

最广泛使用的优化器：

```
mₜ = β₁·mₜ₋₁ + (1-β₁)·∇L          (1st moment: mean of gradients)
vₜ = β₂·vₜ₋₁ + (1-β₂)·(∇L)²       (2nd moment: variance of gradients)

m̂ₜ = mₜ/(1-β₁ᵗ)                   (bias correction)
v̂ₜ = vₜ/(1-β₂ᵗ)

W ← W - α · m̂ₜ / (√v̂ₜ + ε)

Defaults: β₁=0.9, β₂=0.999, ε=1e-8, α=1e-3
```

Adam 基于历史梯度**逐参数**自适应调整学习率。对大多数任务开箱即用。


<details>
<summary>English original</summary>

**Binary Cross-Entropy — Binary Classification**

```
BCE = -(1/N) Σᵢ [yᵢ log(ŷᵢ) + (1-yᵢ) log(1-ŷᵢ)]

yᵢ ∈ {0, 1}       true label
ŷᵢ ∈ (0, 1)       predicted probability (after sigmoid)

Intuition: penalizes confident wrong predictions heavily
  Predicted 0.99 when truth is 0 → very high loss
  Predicted 0.5  when truth is 0 → moderate loss
```

**Categorical Cross-Entropy — Multi-class Classification**

```
CCE = -(1/N) Σᵢ Σₖ yᵢₖ log(ŷᵢₖ)

yᵢₖ = 1 if sample i belongs to class k, else 0  (one-hot)
ŷᵢₖ = predicted probability for class k (after softmax)

Most common loss for image classification
```

**Why Cross-Entropy?**

Cross-entropy comes from information theory. Minimizing it is equivalent to **maximum likelihood estimation** — finding parameters that make the observed data most probable. It penalizes the model proportionally to how surprised it was by the correct answer.

---

**9. Backpropagation: How Networks Learn**

**The Core Idea**

Backpropagation = **chain rule of calculus** applied to compute gradients of the loss with respect to every weight in the network.

```
∂Loss/∂W = how much does the loss change when W changes slightly?

We want: decrease the loss
Strategy: move W in the direction that decreases Loss
Update:   W ← W - α · (∂Loss/∂W)
```

**Chain Rule Foundation**

For a composed function f(g(x)):
```
df/dx = (df/dg) · (dg/dx)
```

In a neural network, the loss flows through many composed functions:
```
Loss = L(softmax(W₃ · relu(W₂ · relu(W₁ · x + b₁) + b₂) + b₃))
```

Backprop applies chain rule **backwards** from output to input.

**Computational Graph**

Think of the network as a graph of operations:

```
x → [W₁ matmul] → z₁ → [relu] → a₁ → [W₂ matmul] → z₂ → [loss] → L
```

Forward pass: compute and **cache** every intermediate value.
Backward pass: compute gradient at each node using cached values + chain rule.

**Step-by-Step Backward Pass (1 layer example)**

```
Forward:
  z = x·w + b
  a = relu(z)
  L = MSE(a, y)

Backward (chain rule):
  dL/da = 2(a - y)/N                    (MSE gradient)
  dL/dz = dL/da · d(relu)/dz           (relu gradient: 1 if z>0 else 0)
  dL/dw = dL/dz · x                    (matmul gradient)
  dL/db = dL/dz · 1                    (bias gradient)
  dL/dx = dL/dz · w                    (input gradient, for prev layer)
```

**Automatic Differentiation (Autograd)**

Modern frameworks (including tinygrad) implement **autograd**: they build the computational graph during the forward pass, then automatically compute all gradients during `.backward()`.

You never implement backprop by hand — you just define the forward pass.

```python
# tinygrad autograd example
x = Tensor([2.0], requires_grad=True)
w = Tensor([3.0], requires_grad=True)
b = Tensor([1.0], requires_grad=True)

z = x * w + b          # forward: builds graph
loss = z.pow(2).mean() # loss computation

loss.backward()         # backward: compute all gradients

print(w.grad)  # dL/dw computed automatically
```

**Vanishing and Exploding Gradients**

**Vanishing gradients**: in deep networks, gradients become extremely small as they propagate back through many layers.
- Cause: sigmoid/tanh saturate (derivative ≈ 0)
- Fix: ReLU, residual connections (ResNets), batch normalization

**Exploding gradients**: gradients grow exponentially.
- Cause: large weights × many layers
- Fix: gradient clipping, weight initialization schemes

---

**10. Gradient Descent and Optimizers**

**Gradient Descent (Base Algorithm)**

```
For each training step:
  1. Compute loss on a batch of data
  2. Compute gradients via backprop
  3. Update every parameter:
     W ← W - α · ∂Loss/∂W

α = learning rate (hyperparameter, typically 1e-3 to 1e-4)
```

**Batch Variants**

| Variant           | Batch Size    | Pros                      | Cons                        |
|-------------------|---------------|---------------------------|-----------------------------|
| Batch GD          | Full dataset  | Stable gradients          | Slow, memory-intensive      |
| Stochastic GD (SGD)| 1 sample    | Fast updates              | Very noisy                  |
| Mini-batch GD     | 32–256        | Balance of both           | Industry standard           |

**SGD with Momentum**

```
vₜ = β·vₜ₋₁ + (1-β)·∇L     (momentum term, β ≈ 0.9)
W ← W - α·vₜ

Intuition: gradient as a ball rolling downhill, accumulates velocity
Benefit: faster convergence, escapes shallow local minima
```

**Adam (Adaptive Moment Estimation)**

The most widely used optimizer:

```
mₜ = β₁·mₜ₋₁ + (1-β₁)·∇L          (1st moment: mean of gradients)
vₜ = β₂·vₜ₋₁ + (1-β₂)·(∇L)²       (2nd moment: variance of gradients)

m̂ₜ = mₜ/(1-β₁ᵗ)                   (bias correction)
v̂ₜ = vₜ/(1-β₂ᵗ)

W ← W - α · m̂ₜ / (√v̂ₜ + ε)

Defaults: β₁=0.9, β₂=0.999, ε=1e-8, α=1e-3
```

Adam adapts the learning rate **per parameter** based on historical gradients. Works well out of the box for most tasks.

</details>

### 学习率调度

```
Constant LR:       α = 0.001 throughout
Step decay:        α = α₀ × 0.1 every 10 epochs
Cosine annealing:  α follows a cosine curve
Warmup + decay:    Start low, increase, then decrease (Transformers)
```

---

## 11. 训练循环

### 完整的训练循环

```python
# Pseudocode (very close to tinygrad code)

model = MyNetwork()
optimizer = Adam(model.parameters(), lr=1e-3)
loss_fn = CrossEntropyLoss()

for epoch in range(num_epochs):
    # ── Training phase ──────────────────────────────
    model.train()
    for batch_x, batch_y in train_loader:

        # 1. Forward pass
        predictions = model(batch_x)

        # 2. Compute loss
        loss = loss_fn(predictions, batch_y)

        # 3. Zero gradients (clear previous step's gradients)
        optimizer.zero_grad()

        # 4. Backward pass (compute gradients)
        loss.backward()

        # 5. Update weights
        optimizer.step()

    # ── Validation phase ─────────────────────────────
    model.eval()
    val_loss, val_acc = evaluate(model, val_loader)
    print(f"Epoch {epoch}: loss={val_loss:.4f}, acc={val_acc:.2%}")
```

### 循环中的关键概念

**Epoch**：对整个训练数据集的一次完整 pass。

**Batch**：一起处理的数据子集（例如 32 张图像）。它带来：
- GPU 并行
- 梯度平均（比单样本更稳定）
- 让大数据集装进内存

**过拟合**：模型记住了训练数据，在新数据上失效。
```
Training loss:   ↓ ↓ ↓ ↓ ↓ 0.01
Validation loss: ↓ ↓ ↑ ↑ ↑ 0.45   ← overfitting after this point
```

**欠拟合**：模型过于简单，在训练和验证上都失效。
```
Training loss:   0.40 (stays high)
Validation loss: 0.42 (also high)
```

### 超参数与参数

```
Parameters (learned by training):
  - Weights W
  - Biases b

Hyperparameters (set by you before training):
  - Learning rate α
  - Batch size
  - Number of layers
  - Number of neurons per layer
  - Number of epochs
  - Dropout rate
  - Weight decay
```

---

## 12. 卷积神经网络（CNN）

### 为什么图像不直接用 MLP？

一张 224×224 的 RGB 图像 = 224×224×3 = 150,528 个输入。
MLP 第一隐藏层 1024 个神经元 = 150,528 × 1024 = **1.54 亿参数**，仅一层。

问题：
- 无法捕捉空间结构（相邻像素共同起作用）
- 没有权重共享（同一边缘检测器要用于每个位置）
- 参数太多 → 过拟合

### 卷积运算

一个 **filter（kernel）** 在输入上滑动，在每个位置计算加权和：

```
Input (5×5):          Filter (3×3):      Output (3×3):
 1  2  3  4  5         1  0  1            ?  ?  ?
 5  6  7  8  9         0  1  0            ?  ?  ?
 1  2  3  4  5         1  0  1            ?  ?  ?
 5  6  7  8  9
 1  2  3  4  5

At top-left position:
  (1×1)+(2×0)+(3×1) + (5×0)+(6×1)+(7×0) + (1×1)+(2×0)+(3×1)
  = 1+3+6+1+3 = 14
```

该 filter 是一个**可学习的模式检测器**。网络自动学习出能检测边缘、纹理、形状的 filter。

### CNN 架构组件

```
Input Image
    ↓
Conv Layer (learns feature maps)
    ↓
Activation (ReLU)
    ↓
Pooling (downsample, reduce size)
    ↓
... (repeat)
    ↓
Flatten
    ↓
FC (Fully Connected) Layers
    ↓
Output (Softmax)
```

### 池化

最大池化缩小空间维度，保留最显著的特征：

```
Input (4×4):       Max Pool 2×2:
 1  3  2  4          3  4
 5  6  7  8    →     6  8
 3  1  4  2          3  4
 7  8  5  6          8  6
```

### 图像任务上 CNN 与 MLP 的对比

```
MLP:
  - Every pixel connects to every neuron
  - No spatial structure preserved
  - Millions of redundant parameters

CNN:
  - Local connectivity (filter slides over image)
  - Weight sharing (same filter at all positions)
  - Translation invariant (cat in corner = cat in center)
  - Far fewer parameters
```

### 经典 CNN 架构

| Model      | Year | Layers | Params  | Top-5 Acc |
|------------|------|--------|---------|-----------|
| LeNet-5    | 1998 | 7      | 60K     | —         |
| AlexNet    | 2012 | 8      | 61M     | 84.6%     |
| VGG-16     | 2014 | 16     | 138M    | 92.7%     |
| ResNet-50  | 2015 | 50     | 25M     | 95.3%     |
| MobileNetV2| 2018 | 53     | 3.4M    | 93.4%     |
| EfficientNet| 2019| varies | varies  | 97.1%     |

MobileNet 和 EfficientNet 面向**边缘部署**设计——准确率与算力的取舍。

---

## 13. 正则化与泛化

### 偏差-方差取舍

```
Total Error = Bias² + Variance + Irreducible Noise

High Bias (underfitting):   model too simple, misses patterns
High Variance (overfitting): model too complex, memorizes noise
```


<details>
<summary>English original</summary>

**Learning Rate Scheduling**

```
Constant LR:       α = 0.001 throughout
Step decay:        α = α₀ × 0.1 every 10 epochs
Cosine annealing:  α follows a cosine curve
Warmup + decay:    Start low, increase, then decrease (Transformers)
```

---

**11. The Training Loop**

**Complete Training Loop**

```python
# Pseudocode (very close to tinygrad code)

model = MyNetwork()
optimizer = Adam(model.parameters(), lr=1e-3)
loss_fn = CrossEntropyLoss()

for epoch in range(num_epochs):
    # ── Training phase ──────────────────────────────
    model.train()
    for batch_x, batch_y in train_loader:

        # 1. Forward pass
        predictions = model(batch_x)

        # 2. Compute loss
        loss = loss_fn(predictions, batch_y)

        # 3. Zero gradients (clear previous step's gradients)
        optimizer.zero_grad()

        # 4. Backward pass (compute gradients)
        loss.backward()

        # 5. Update weights
        optimizer.step()

    # ── Validation phase ─────────────────────────────
    model.eval()
    val_loss, val_acc = evaluate(model, val_loader)
    print(f"Epoch {epoch}: loss={val_loss:.4f}, acc={val_acc:.2%}")
```

**Key Concepts in the Loop**

**Epoch**: one complete pass through the entire training dataset.

**Batch**: a subset of the dataset processed together (e.g., 32 images). Enables:
- GPU parallelism
- Gradient averaging (more stable than single samples)
- Fitting large datasets in memory

**Overfitting**: model memorizes training data, fails on new data.
```
Training loss:   ↓ ↓ ↓ ↓ ↓ 0.01
Validation loss: ↓ ↓ ↑ ↑ ↑ 0.45   ← overfitting after this point
```

**Underfitting**: model too simple, fails on both training and validation.
```
Training loss:   0.40 (stays high)
Validation loss: 0.42 (also high)
```

**Hyperparameters vs Parameters**

```
Parameters (learned by training):
  - Weights W
  - Biases b

Hyperparameters (set by you before training):
  - Learning rate α
  - Batch size
  - Number of layers
  - Number of neurons per layer
  - Number of epochs
  - Dropout rate
  - Weight decay
```

---

**12. Convolutional Neural Networks (CNNs)**

**Why Not Just Use MLP for Images?**

A 224×224 RGB image = 224×224×3 = 150,528 inputs.
MLP first hidden layer with 1024 neurons = 150,528 × 1024 = **154 million parameters** for one layer.

Problems:
- Doesn't capture spatial structure (neighboring pixels matter together)
- No weight sharing (same edge detector at every location)
- Too many parameters → overfitting

**The Convolution Operation**

A **filter (kernel)** slides over the input, computing a weighted sum at each position:

```
Input (5×5):          Filter (3×3):      Output (3×3):
 1  2  3  4  5         1  0  1            ?  ?  ?
 5  6  7  8  9         0  1  0            ?  ?  ?
 1  2  3  4  5         1  0  1            ?  ?  ?
 5  6  7  8  9
 1  2  3  4  5

At top-left position:
  (1×1)+(2×0)+(3×1) + (5×0)+(6×1)+(7×0) + (1×1)+(2×0)+(3×1)
  = 1+3+6+1+3 = 14
```

The filter is a **learned pattern detector**. The network learns filters that detect edges, textures, shapes automatically.

**CNN Architecture Components**

```
Input Image
    ↓
Conv Layer (learns feature maps)
    ↓
Activation (ReLU)
    ↓
Pooling (downsample, reduce size)
    ↓
... (repeat)
    ↓
Flatten
    ↓
FC (Fully Connected) Layers
    ↓
Output (Softmax)
```

**Pooling**

Max pooling reduces spatial dimensions, retaining the most prominent features:

```
Input (4×4):       Max Pool 2×2:
 1  3  2  4          3  4
 5  6  7  8    →     6  8
 3  1  4  2          3  4
 7  8  5  6          8  6
```

**CNN vs MLP for Images**

```
MLP:
  - Every pixel connects to every neuron
  - No spatial structure preserved
  - Millions of redundant parameters

CNN:
  - Local connectivity (filter slides over image)
  - Weight sharing (same filter at all positions)
  - Translation invariant (cat in corner = cat in center)
  - Far fewer parameters
```

**Classic CNN Architectures**

| Model      | Year | Layers | Params  | Top-5 Acc |
|------------|------|--------|---------|-----------|
| LeNet-5    | 1998 | 7      | 60K     | —         |
| AlexNet    | 2012 | 8      | 61M     | 84.6%     |
| VGG-16     | 2014 | 16     | 138M    | 92.7%     |
| ResNet-50  | 2015 | 50     | 25M     | 95.3%     |
| MobileNetV2| 2018 | 53     | 3.4M    | 93.4%     |
| EfficientNet| 2019| varies | varies  | 97.1%     |

MobileNet and EfficientNet are designed for **edge deployment** — accuracy vs. compute tradeoff.

---

**13. Regularization and Generalization**

**The Bias-Variance Tradeoff**

```
Total Error = Bias² + Variance + Irreducible Noise

High Bias (underfitting):   model too simple, misses patterns
High Variance (overfitting): model too complex, memorizes noise
```

</details>

### Dropout

训练时随机将神经元置零：

```python
# During training:
  For each neuron, with probability p, set output to 0
  Scale remaining neurons by 1/(1-p)

# During inference:
  No dropout (all neurons active)

Effect: Forces network to learn redundant representations
        Acts as ensemble of many sub-networks
Typical p: 0.2–0.5
```

### 权重衰减（L2 正则化）

对较大的权重在损失中加入惩罚项：

```
L_total = L_task + λ · Σ w²

λ = regularization strength (e.g., 1e-4)

Effect: Keeps weights small, reduces overfitting
Equivalent to Gaussian prior on weights (Bayesian view)
```

### 批归一化

对每一层的输入做归一化：

```
For a mini-batch of activations z:
  μ = mean(z)
  σ² = var(z)
  z_norm = (z - μ) / √(σ² + ε)
  output = γ · z_norm + β        (γ, β are learned)

Benefits:
  - Faster training (higher learning rates)
  - Less sensitive to weight initialization
  - Mild regularization effect
  - Reduces internal covariate shift
```

### 早停

```
Monitor validation loss during training
Stop training when validation loss stops improving

Epoch: 1  train_loss=2.3  val_loss=2.3
Epoch: 5  train_loss=1.2  val_loss=1.3
Epoch:10  train_loss=0.5  val_loss=0.8   ← save checkpoint here
Epoch:15  train_loss=0.2  val_loss=1.1   ← overfitting, stop
Epoch:20  train_loss=0.1  val_loss=1.4
```

### 数据增强

通过变换已有样本人为扩充数据集：

```
Image augmentations:
  - Horizontal/vertical flip
  - Random crop and resize
  - Color jitter (brightness, contrast, saturation)
  - Random rotation
  - Gaussian noise

Effect: Model sees more variety → better generalization
Edge AI benefit: reduces need for large datasets
```

---

## 14. tinygrad 实战

### 为什么用 tinygrad？

tinygrad 是一个极简 ML 框架（核心代码约 1000 行），把每一个基础操作都清晰地暴露出来。与 PyTorch 或 TensorFlow 不同：

- 没有魔法——整个源码你都能读懂
- 让你**确切**理解神经网络内部发生了什么
- 可编译到 CPU、GPU、CUDA、Metal、WebGPU
- 已在 comma.ai（自动驾驶）的生产环境中使用

### tinygrad 核心概念

```python
from tinygrad.tensor import Tensor
import numpy as np

# Tensor creation
x = Tensor([[1.0, 2.0, 3.0]])           # shape [1, 3]
w = Tensor.kaiming_uniform(3, 4)        # shape [3, 4]

# Operations (lazy by default)
z = x.matmul(w)                         # shape [1, 4]
a = z.relu()

# Realize (execute computation)
result = a.numpy()                       # triggers actual computation

# Gradient computation
loss = a.sum()
loss.backward()
print(w.grad.numpy())                   # ∂loss/∂w
```

### 从零实现一个神经元

```python
from tinygrad.tensor import Tensor

class Neuron:
    def __init__(self, n_inputs):
        # Initialize weights with small random values
        self.w = Tensor.randn(n_inputs, 1) * 0.01
        self.b = Tensor.zeros(1)

    def __call__(self, x):
        z = x.matmul(self.w) + self.b
        return z.relu()

    def parameters(self):
        return [self.w, self.b]

# Test
neuron = Neuron(3)
x = Tensor([[0.5, 1.2, -0.3]])
output = neuron(x)
print(output.numpy())  # shape [1, 1]
```

### 实现一个 layer

```python
class Linear:
    def __init__(self, n_in, n_out):
        # Kaiming initialization for ReLU
        self.w = Tensor.kaiming_uniform(n_in, n_out)
        self.b = Tensor.zeros(n_out)

    def __call__(self, x):
        return x.matmul(self.w) + self.b

    def parameters(self):
        return [self.w, self.b]
```

### 实现一个 MLP

```python
class MLP:
    def __init__(self, layers):
        # layers = [784, 256, 128, 10]
        self.linears = [
            Linear(layers[i], layers[i+1])
            for i in range(len(layers)-1)
        ]

    def __call__(self, x):
        for i, layer in enumerate(self.linears[:-1]):
            x = layer(x).relu()          # hidden layers: ReLU
        x = self.linears[-1](x)          # output layer: no activation
        return x.softmax()               # softmax for probabilities

    def parameters(self):
        params = []
        for layer in self.linears:
            params.extend(layer.parameters())
        return params
```


<details>
<summary>English original</summary>

**Dropout**

Randomly zero out neurons during training:

```python
# During training:
  For each neuron, with probability p, set output to 0
  Scale remaining neurons by 1/(1-p)

# During inference:
  No dropout (all neurons active)

Effect: Forces network to learn redundant representations
        Acts as ensemble of many sub-networks
Typical p: 0.2–0.5
```

**Weight Decay (L2 Regularization)**

Add a penalty to the loss for large weights:

```
L_total = L_task + λ · Σ w²

λ = regularization strength (e.g., 1e-4)

Effect: Keeps weights small, reduces overfitting
Equivalent to Gaussian prior on weights (Bayesian view)
```

**Batch Normalization**

Normalize the inputs to each layer:

```
For a mini-batch of activations z:
  μ = mean(z)
  σ² = var(z)
  z_norm = (z - μ) / √(σ² + ε)
  output = γ · z_norm + β        (γ, β are learned)

Benefits:
  - Faster training (higher learning rates)
  - Less sensitive to weight initialization
  - Mild regularization effect
  - Reduces internal covariate shift
```

**Early Stopping**

```
Monitor validation loss during training
Stop training when validation loss stops improving

Epoch: 1  train_loss=2.3  val_loss=2.3
Epoch: 5  train_loss=1.2  val_loss=1.3
Epoch:10  train_loss=0.5  val_loss=0.8   ← save checkpoint here
Epoch:15  train_loss=0.2  val_loss=1.1   ← overfitting, stop
Epoch:20  train_loss=0.1  val_loss=1.4
```

**Data Augmentation**

Artificially expand dataset by transforming existing samples:

```
Image augmentations:
  - Horizontal/vertical flip
  - Random crop and resize
  - Color jitter (brightness, contrast, saturation)
  - Random rotation
  - Gaussian noise

Effect: Model sees more variety → better generalization
Edge AI benefit: reduces need for large datasets
```

---

**14. Hands-On with tinygrad**

**Why tinygrad?**

tinygrad is a minimal ML framework (~1000 lines of core code) that exposes every fundamental operation clearly. Unlike PyTorch or TensorFlow:

- No magic — you can read and understand the entire source
- Teaches you **exactly** what happens inside a neural network
- Compiles to CPU, GPU, CUDA, Metal, WebGPU
- Used in production at comma.ai (autonomous driving)

**tinygrad Core Concepts**

```python
from tinygrad.tensor import Tensor
import numpy as np

# Tensor creation
x = Tensor([[1.0, 2.0, 3.0]])           # shape [1, 3]
w = Tensor.kaiming_uniform(3, 4)        # shape [3, 4]

# Operations (lazy by default)
z = x.matmul(w)                         # shape [1, 4]
a = z.relu()

# Realize (execute computation)
result = a.numpy()                       # triggers actual computation

# Gradient computation
loss = a.sum()
loss.backward()
print(w.grad.numpy())                   # ∂loss/∂w
```

**Implementing a Neuron from Scratch**

```python
from tinygrad.tensor import Tensor

class Neuron:
    def __init__(self, n_inputs):
        # Initialize weights with small random values
        self.w = Tensor.randn(n_inputs, 1) * 0.01
        self.b = Tensor.zeros(1)

    def __call__(self, x):
        z = x.matmul(self.w) + self.b
        return z.relu()

    def parameters(self):
        return [self.w, self.b]

# Test
neuron = Neuron(3)
x = Tensor([[0.5, 1.2, -0.3]])
output = neuron(x)
print(output.numpy())  # shape [1, 1]
```

**Implementing a Layer**

```python
class Linear:
    def __init__(self, n_in, n_out):
        # Kaiming initialization for ReLU
        self.w = Tensor.kaiming_uniform(n_in, n_out)
        self.b = Tensor.zeros(n_out)

    def __call__(self, x):
        return x.matmul(self.w) + self.b

    def parameters(self):
        return [self.w, self.b]
```

**Implementing an MLP**

```python
class MLP:
    def __init__(self, layers):
        # layers = [784, 256, 128, 10]
        self.linears = [
            Linear(layers[i], layers[i+1])
            for i in range(len(layers)-1)
        ]

    def __call__(self, x):
        for i, layer in enumerate(self.linears[:-1]):
            x = layer(x).relu()          # hidden layers: ReLU
        x = self.linears[-1](x)          # output layer: no activation
        return x.softmax()               # softmax for probabilities

    def parameters(self):
        params = []
        for layer in self.linears:
            params.extend(layer.parameters())
        return params
```

</details>

### 在 MNIST 上训练

```python
from tinygrad.tensor import Tensor
from tinygrad.nn.optim import Adam
import numpy as np

# Load MNIST (use fetch from tinygrad)
from tinygrad.helpers import fetch
import gzip

def load_mnist():
    base = "https://storage.googleapis.com/cvdf-datasets/mnist/"
    X_train = np.frombuffer(gzip.open(fetch(base+"train-images-idx3-ubyte.gz")).read(), np.uint8, offset=16).reshape(-1, 784)
    Y_train = np.frombuffer(gzip.open(fetch(base+"train-labels-idx1-ubyte.gz")).read(), np.uint8, offset=8)
    X_test  = np.frombuffer(gzip.open(fetch(base+"t10k-images-idx3-ubyte.gz")).read(), np.uint8, offset=16).reshape(-1, 784)
    Y_test  = np.frombuffer(gzip.open(fetch(base+"t10k-labels-idx1-ubyte.gz")).read(), np.uint8, offset=8)
    return X_train/255.0, Y_train, X_test/255.0, Y_test

X_train, Y_train, X_test, Y_test = load_mnist()

# Define model
model = MLP([784, 256, 128, 10])
optimizer = Adam(model.parameters(), lr=1e-3)

# Training loop
BATCH = 64
EPOCHS = 10

for epoch in range(EPOCHS):
    # Shuffle
    idx = np.random.permutation(len(X_train))
    X_train, Y_train = X_train[idx], Y_train[idx]

    total_loss = 0
    for i in range(0, len(X_train), BATCH):
        xb = Tensor(X_train[i:i+BATCH].astype(np.float32))
        yb = Y_train[i:i+BATCH]

        # Forward pass
        out = model(xb)

        # Cross-entropy loss
        # One-hot encode labels
        yb_onehot = np.zeros((len(yb), 10), dtype=np.float32)
        yb_onehot[np.arange(len(yb)), yb] = 1.0
        yb_t = Tensor(yb_onehot)

        loss = -(yb_t * out.log()).sum(axis=1).mean()

        # Backward
        optimizer.zero_grad()
        loss.backward()
        optimizer.step()

        total_loss += loss.numpy()

    # Validation
    test_out = model(Tensor(X_test.astype(np.float32)))
    preds = test_out.numpy().argmax(axis=1)
    acc = (preds == Y_test).mean()
    print(f"Epoch {epoch+1}: loss={total_loss/(len(X_train)//BATCH):.4f}, test_acc={acc:.2%}")
```

### 理解 tinygrad 的自动微分

```python
# tinygrad builds a computation graph lazily
# Every operation on a Tensor records itself

x = Tensor([2.0])
y = x * x       # records: mul(x, x)
z = y + x       # records: add(y, x)
loss = z.sum()

# .backward() traverses graph in reverse (topological sort)
# applying chain rule at each node
loss.backward()

print(x.grad)   # d(loss)/d(x) = d(x²+x)/dx = 2x+1 = 5.0
```

### 在 tinygrad 中实现 CNN

```python
from tinygrad.tensor import Tensor
from tinygrad.nn import Conv2d, BatchNorm2d
from tinygrad.nn.optim import Adam

class SimpleCNN:
    def __init__(self):
        # Conv layers
        self.c1 = Conv2d(1, 32, 3, padding=1)   # 1 channel in, 32 out, 3×3 kernel
        self.c2 = Conv2d(32, 64, 3, padding=1)

        # Fully connected
        self.fc1 = Linear(64 * 7 * 7, 128)
        self.fc2 = Linear(128, 10)

    def __call__(self, x):
        # x shape: [batch, 1, 28, 28]
        x = self.c1(x).relu().max_pool2d()       # → [batch, 32, 14, 14]
        x = self.c2(x).relu().max_pool2d()       # → [batch, 64, 7, 7]
        x = x.reshape(x.shape[0], -1)            # flatten → [batch, 3136]
        x = self.fc1(x).relu()
        x = self.fc2(x)
        return x.softmax()

    def parameters(self):
        params = []
        for layer in [self.c1, self.c2, self.fc1, self.fc2]:
            params.extend(layer.parameters() if hasattr(layer, 'parameters') else [layer.weight, layer.bias])
        return params
```

### 探索 tinygrad 内部实现

研读 tinygrad 源码中的这些文件，以深入理解 ML 的工作方式：

```
tinygrad/
  tensor.py          ← Tensor class, all ops, autograd engine
  lazybuffer.py      ← Lazy evaluation / computation graph
  ops.py             ← All primitive operations
  nn/
    optim.py         ← SGD, Adam, AdaGrad implementations
  runtime/
    ops_cpu.py       ← How ops execute on CPU
    ops_gpu.py       ← How ops execute on GPU
```

阅读 `tensor.py` 是理解自动微分与深度学习框架内部工作原理的最佳途径之一。

---

## 15. 项目

### 项目 1：从零实现神经元（不使用框架）
用纯 Python/NumPy 实现单个神经元。手动计算梯度，并用有限差分验证。

```
Goal: understand forward pass, loss, gradient by hand
Dataset: XOR problem (4 samples)
Deliverable: working neuron with manual backprop
```

### 项目 2：用 tinygrad 实现 MNIST 的 MLP
在 MNIST 手写数字分类上训练全连接网络。

```
Goal: achieve >97% test accuracy
Architecture: 784 → 256 → 128 → 10
Deliverable: training script + loss/accuracy curves
```

### 项目 3：用 tinygrad 实现 MNIST/CIFAR-10 的 CNN
用卷积网络替换 MLP。

```
Goal: understand convolution, pooling, feature maps
Dataset: MNIST (>99%) or CIFAR-10 (>85%)
Deliverable: CNN training script, visualize learned filters
```


<details>
<summary>English original</summary>

**Training on MNIST**

```python
from tinygrad.tensor import Tensor
from tinygrad.nn.optim import Adam
import numpy as np

# Load MNIST (use fetch from tinygrad)
from tinygrad.helpers import fetch
import gzip

def load_mnist():
    base = "https://storage.googleapis.com/cvdf-datasets/mnist/"
    X_train = np.frombuffer(gzip.open(fetch(base+"train-images-idx3-ubyte.gz")).read(), np.uint8, offset=16).reshape(-1, 784)
    Y_train = np.frombuffer(gzip.open(fetch(base+"train-labels-idx1-ubyte.gz")).read(), np.uint8, offset=8)
    X_test  = np.frombuffer(gzip.open(fetch(base+"t10k-images-idx3-ubyte.gz")).read(), np.uint8, offset=16).reshape(-1, 784)
    Y_test  = np.frombuffer(gzip.open(fetch(base+"t10k-labels-idx1-ubyte.gz")).read(), np.uint8, offset=8)
    return X_train/255.0, Y_train, X_test/255.0, Y_test

X_train, Y_train, X_test, Y_test = load_mnist()

# Define model
model = MLP([784, 256, 128, 10])
optimizer = Adam(model.parameters(), lr=1e-3)

# Training loop
BATCH = 64
EPOCHS = 10

for epoch in range(EPOCHS):
    # Shuffle
    idx = np.random.permutation(len(X_train))
    X_train, Y_train = X_train[idx], Y_train[idx]

    total_loss = 0
    for i in range(0, len(X_train), BATCH):
        xb = Tensor(X_train[i:i+BATCH].astype(np.float32))
        yb = Y_train[i:i+BATCH]

        # Forward pass
        out = model(xb)

        # Cross-entropy loss
        # One-hot encode labels
        yb_onehot = np.zeros((len(yb), 10), dtype=np.float32)
        yb_onehot[np.arange(len(yb)), yb] = 1.0
        yb_t = Tensor(yb_onehot)

        loss = -(yb_t * out.log()).sum(axis=1).mean()

        # Backward
        optimizer.zero_grad()
        loss.backward()
        optimizer.step()

        total_loss += loss.numpy()

    # Validation
    test_out = model(Tensor(X_test.astype(np.float32)))
    preds = test_out.numpy().argmax(axis=1)
    acc = (preds == Y_test).mean()
    print(f"Epoch {epoch+1}: loss={total_loss/(len(X_train)//BATCH):.4f}, test_acc={acc:.2%}")
```

**Understanding tinygrad's Autograd**

```python
# tinygrad builds a computation graph lazily
# Every operation on a Tensor records itself

x = Tensor([2.0])
y = x * x       # records: mul(x, x)
z = y + x       # records: add(y, x)
loss = z.sum()

# .backward() traverses graph in reverse (topological sort)
# applying chain rule at each node
loss.backward()

print(x.grad)   # d(loss)/d(x) = d(x²+x)/dx = 2x+1 = 5.0
```

**Implementing a CNN in tinygrad**

```python
from tinygrad.tensor import Tensor
from tinygrad.nn import Conv2d, BatchNorm2d
from tinygrad.nn.optim import Adam

class SimpleCNN:
    def __init__(self):
        # Conv layers
        self.c1 = Conv2d(1, 32, 3, padding=1)   # 1 channel in, 32 out, 3×3 kernel
        self.c2 = Conv2d(32, 64, 3, padding=1)

        # Fully connected
        self.fc1 = Linear(64 * 7 * 7, 128)
        self.fc2 = Linear(128, 10)

    def __call__(self, x):
        # x shape: [batch, 1, 28, 28]
        x = self.c1(x).relu().max_pool2d()       # → [batch, 32, 14, 14]
        x = self.c2(x).relu().max_pool2d()       # → [batch, 64, 7, 7]
        x = x.reshape(x.shape[0], -1)            # flatten → [batch, 3136]
        x = self.fc1(x).relu()
        x = self.fc2(x)
        return x.softmax()

    def parameters(self):
        params = []
        for layer in [self.c1, self.c2, self.fc1, self.fc2]:
            params.extend(layer.parameters() if hasattr(layer, 'parameters') else [layer.weight, layer.bias])
        return params
```

**Exploring tinygrad Internals**

Study these files in the tinygrad source to deeply understand how ML works:

```
tinygrad/
  tensor.py          ← Tensor class, all ops, autograd engine
  lazybuffer.py      ← Lazy evaluation / computation graph
  ops.py             ← All primitive operations
  nn/
    optim.py         ← SGD, Adam, AdaGrad implementations
  runtime/
    ops_cpu.py       ← How ops execute on CPU
    ops_gpu.py       ← How ops execute on GPU
```

Reading `tensor.py` is one of the best ways to understand how autograd and deep learning frameworks work internally.

---

**15. Projects**

**Project 1: Neuron from Scratch (No Framework)**
Implement a single neuron in pure Python/NumPy. Manually compute gradients and verify with finite differences.

```
Goal: understand forward pass, loss, gradient by hand
Dataset: XOR problem (4 samples)
Deliverable: working neuron with manual backprop
```

**Project 2: MLP for MNIST with tinygrad**
Train a fully connected network on MNIST digit classification.

```
Goal: achieve >97% test accuracy
Architecture: 784 → 256 → 128 → 10
Deliverable: training script + loss/accuracy curves
```

**Project 3: CNN for MNIST/CIFAR-10 with tinygrad**
Replace MLP with a convolutional network.

```
Goal: understand convolution, pooling, feature maps
Dataset: MNIST (>99%) or CIFAR-10 (>85%)
Deliverable: CNN training script, visualize learned filters
```

</details>

### 项目 4：从零实现 Adam
在 tinygrad 中手工实现 Adam 优化器（不使用 tinygrad 内置的 Adam）。

```
Goal: understand optimizer internals
Deliverable: custom Adam class matching tinygrad's results
```

### 项目 5：阅读 tinygrad 的反向传播
在 tinygrad 源码中跟踪一个简单运算（例如 `x * x`），并完整记录 `backward()` 过程中究竟发生了什么。

```
Goal: understand autograd mechanics
Deliverable: annotated source code walkthrough
Files: tensor.py, ops.py
```

### 项目 6：Jetson 上的 MNIST（边缘部署）
在桌面端训练，导出模型权重，在 Jetson Nano 上跑推理。

```
Goal: complete edge AI pipeline
Steps:
  1. Train CNN on desktop
  2. Export weights as numpy arrays
  3. Load weights in tinygrad on Jetson
  4. Measure inference latency: CPU vs GPU
  5. Quantize to INT8, compare accuracy/speed
```

---

## 16. 资源

### 基础理论

- **3Blue1Brown — Neural Networks**（YouTube 系列）：介绍神经网络工作原理的最佳可视化入门。写代码之前先看完这 4 集。
  - 「但什么是神经网络？」
  - 「梯度下降，神经网络如何学习」
  - 「反向传播到底在做什么？」
  - 「反向传播的微积分」

- **Andrej Karpathy — micrograd**（GitHub + YouTube）：用约 150 行从零构建 autograd。深入理解反向传播的必读材料。之后与 tinygrad 的实现做对比。
  - https://github.com/karpathy/micrograd

- **CS231n: Convolutional Neural Networks for Visual Recognition**（Stanford）：CNN 的权威课程。即使不看视频，讲义本身也极为出色。
  - https://cs231n.github.io/

- **The Deep Learning Book**（Goodfellow、Bengio、Courville）：在线免费。第 6-9 章严谨地覆盖 MLP、反向传播、正则化、CNN。
  - https://www.deeplearningbook.org/

### tinygrad 专属

- **tinygrad 源码**：最好的 tinygrad 文档就是源码本身。
  - `tensor.py` 用于 ops 与 autograd
  - `examples/` 用于训练脚本
  - `test/` 用于理解预期行为

- **tinygrad MNIST 示例**：`tinygrad/examples/mnist.py` —— 标准起点
- **tinygrad CNN 示例**：`tinygrad/examples/efficientnet.py`

### 边缘 AI 背景

- **TinyML book**（Pete Warden、Daniel Situnayake）：在微控制器上跑 ML。第 1-3 章充分说明了边缘 AI 的设计取舍为何重要。

- **AI at the Edge**（Daniel Situnayake、Jenny Plunkett）：端到端边缘 AI 系统设计。

### 数学前置要求

- **线性代数**：3Blue1Brown「Essence of Linear Algebra」—— 尤其是把矩阵看作变换
- **微积分**：链式法则、偏导数 —— Khan Academy 微积分课程或 3Blue1Brown「Essence of Calculus」
- **统计学**：概率、分布 —— 损失函数与正则化需要用到

### 练习数据集

| 数据集   | 任务                | 样本数 | 输入尺寸 | 基线 |
|-----------|---------------------|---------|------------|----------|
| MNIST     | 手写数字分类 | 70,000 | 28×28×1    | 99.7%    |
| Fashion-MNIST | 服装分类 | 70,000 | 28×28×1 | 94%   |
| CIFAR-10  | 物体分类 | 60,000 | 32×32×3   | 95%+     |
| Iris      | 花卉分类   | 150   | 4 个特征 | 97%     |

---

## 速查：关键公式

```
Neuron forward:
  z = Wx + b
  a = f(z)

MSE Loss:
  L = (1/N) Σ(y - ŷ)²

Cross-entropy Loss:
  L = -(1/N) Σ y·log(ŷ)

Gradient descent:
  W ← W - α·∂L/∂W

Adam update:
  m = β₁m + (1-β₁)g
  v = β₂v + (1-β₂)g²
  W ← W - α·m̂/√(v̂+ε)

Convolution output size:
  out = floor((in + 2p - k) / s) + 1
  in=input, p=padding, k=kernel, s=stride
```

---

## 深入：子文件夹

### [PyTorch + micrograd → tinygrad](../2. Deep Learning Frameworks/micrograd/Guide.md)

在深入 tinygrad 内部实现或移植模型之前，需要对 autograd 的真实工作原理有牢固的基础。本子文件夹正是为此准备：

- **第 1 部分 — micrograd**：从零重新实现 Karpathy 的标量 autograd 引擎。每个 `+`、`*`、`tanh`、`exp` 都带有一个 `_backward` 闭包。你会看到链式法则以运行中的代码出现，而不是数学符号。
- **第 2 部分 — PyTorch**：与 OpenCV PyTorch Bootcamp 课程对齐。张量、autograd、`nn.Module`、CNN、迁移学习、目标检测头 —— 与 micrograd 保持同一套心智模型。
- **第 3 部分 — 通向 tinygrad 的桥梁**：同一个 MLP 在三个框架中的并排对比。PyTorch 隐藏了什么（惰性求值、kernel 融合），而 tinygrad 通过 `DEBUG=4` 把它暴露出来。

**为什么这对精通 tinygrad 很重要：**
```
micrograd  →  you understand every gradient operation
PyTorch    →  you understand the production API and patterns
tinygrad   →  you see what frameworks hide, control the scheduler
```

---

*下一篇（阶段 4 方向 B）：[ML and AI](../../阶段4-路线B-Nvidia-Jetson/05-应用开发/05-机器学习与人工智能/Guide.md) — 量化、剪枝、TensorRT、TFLite*


<details>
<summary>English original</summary>

**Project 4: Implement Adam from Scratch**
Implement the Adam optimizer manually in tinygrad (without using tinygrad's built-in Adam).

```
Goal: understand optimizer internals
Deliverable: custom Adam class matching tinygrad's results
```

**Project 5: Read tinygrad's Backprop**
Trace through tinygrad source code for a simple operation (e.g., `x * x`) and document exactly what happens during `backward()`.

```
Goal: understand autograd mechanics
Deliverable: annotated source code walkthrough
Files: tensor.py, ops.py
```

**Project 6: MNIST on Jetson (Edge Deployment)**
Train on desktop, export model weights, run inference on Jetson Nano.

```
Goal: complete edge AI pipeline
Steps:
  1. Train CNN on desktop
  2. Export weights as numpy arrays
  3. Load weights in tinygrad on Jetson
  4. Measure inference latency: CPU vs GPU
  5. Quantize to INT8, compare accuracy/speed
```

---

**16. Resources**

**Foundational Theory**

- **3Blue1Brown — Neural Networks** (YouTube series): Best visual introduction to how neural networks work. Watch all 4 episodes before writing code.
  - "But what is a neural network?"
  - "Gradient descent, how neural networks learn"
  - "What is backpropagation really doing?"
  - "Backpropagation calculus"

- **Andrej Karpathy — micrograd** (GitHub + YouTube): Build autograd from scratch in ~150 lines. Essential for understanding backprop deeply. Then compare with tinygrad's implementation.
  - https://github.com/karpathy/micrograd

- **CS231n: Convolutional Neural Networks for Visual Recognition** (Stanford): The definitive CNN course. Lecture notes are excellent even without watching videos.
  - https://cs231n.github.io/

- **The Deep Learning Book** (Goodfellow, Bengio, Courville): Free online. Chapters 6-9 cover MLP, backprop, regularization, CNNs rigorously.
  - https://www.deeplearningbook.org/

**tinygrad-Specific**

- **tinygrad source code**: The best tinygrad documentation is the source itself.
  - `tensor.py` for ops and autograd
  - `examples/` for training scripts
  - `test/` for understanding expected behavior

- **tinygrad MNIST example**: `tinygrad/examples/mnist.py` — canonical starting point
- **tinygrad CNN example**: `tinygrad/examples/efficientnet.py`

**Edge AI Context**

- **TinyML book** (Pete Warden, Daniel Situnayake): Running ML on microcontrollers. Chapters 1-3 give strong context for why edge AI design choices matter.

- **AI at the Edge** (Daniel Situnayake, Jenny Plunkett): End-to-end edge AI system design.

**Math Prerequisites**

- **Linear Algebra**: 3Blue1Brown "Essence of Linear Algebra" — especially matrices as transformations
- **Calculus**: Chain rule, partial derivatives — Khan Academy calculus or 3Blue1Brown "Essence of Calculus"
- **Statistics**: Probability, distributions — needed for loss functions and regularization

**Practice Datasets**

| Dataset   | Task                | Samples | Input Size | Baseline |
|-----------|---------------------|---------|------------|----------|
| MNIST     | Digit classification | 70,000 | 28×28×1    | 99.7%    |
| Fashion-MNIST | Clothing classification | 70,000 | 28×28×1 | 94%   |
| CIFAR-10  | Object classification | 60,000 | 32×32×3   | 95%+     |
| Iris      | Flower classification | 150   | 4 features | 97%     |

---

**Quick Reference: Key Equations**

```
Neuron forward:
  z = Wx + b
  a = f(z)

MSE Loss:
  L = (1/N) Σ(y - ŷ)²

Cross-entropy Loss:
  L = -(1/N) Σ y·log(ŷ)

Gradient descent:
  W ← W - α·∂L/∂W

Adam update:
  m = β₁m + (1-β₁)g
  v = β₂v + (1-β₂)g²
  W ← W - α·m̂/√(v̂+ε)

Convolution output size:
  out = floor((in + 2p - k) / s) + 1
  in=input, p=padding, k=kernel, s=stride
```

---

**Deep Dive: Subfolders**

**[PyTorch + micrograd → tinygrad](../2. Deep Learning Frameworks/micrograd/Guide.md)**

Before diving deep into tinygrad internals or porting models, you need a rock-solid foundation in how autograd actually works. This subfolder provides exactly that:

- **Part 1 — micrograd**: Reimplement Karpathy's scalar autograd engine from scratch. Every `+`, `*`, `tanh`, `exp` carries a `_backward` closure. You'll see chain rule as running code, not math notation.
- **Part 2 — PyTorch**: Aligned with the OpenCV PyTorch Bootcamp curriculum. Tensors, autograd, `nn.Module`, CNNs, transfer learning, object detection heads — with the same mental model as micrograd.
- **Part 3 — Bridge to tinygrad**: Side-by-side comparison of the same MLP in all three frameworks. What PyTorch hides (lazy evaluation, kernel fusion) that tinygrad exposes via `DEBUG=4`.

**Why this matters for tinygrad mastery:**
```
micrograd  →  you understand every gradient operation
PyTorch    →  you understand the production API and patterns
tinygrad   →  you see what frameworks hide, control the scheduler
```

---

*Next (Phase 4 Track B): [ML and AI](../../阶段4-路线B-Nvidia-Jetson/05-应用开发/05-机器学习与人工智能/Guide.md) — quantization, pruning, TensorRT, TFLite*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/1. Neural Networks/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/1.%20Neural%20Networks/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
