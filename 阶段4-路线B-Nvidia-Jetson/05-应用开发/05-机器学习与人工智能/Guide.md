---
title: ML and AI
description: ML and AI
---

# ML and AI

<div class="course-identity auto-course" style="--course-accent: #db2777; --course-accent-rgb: 219, 39, 119;" markdown="1">
<div class="course-identity__icon">MAA</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入探索 · Jetson 方向</p>
<p class="course-identity__title">ML 与 AI 的专属课程标识。</p>
<p class="course-identity__meta">产物：Jetson 集成 demo · 测量：延迟、内存、功耗、日志</p>
</div>
</div>


**阶段 4 — 方向 B — 模块 5.5** · 应用开发

> **重点：** 用 **tinygrad** 从第一性原理理解优化，再把完整的移植流水线配合 TensorRT 应用到 **Jetson Orin Nano 8GB** 上。每个概念都有可运行的代码。

**Hub:** [5. 应用开发](../Guide.md)

---

## 深入探索子文件夹

| 子文件夹 | 说明 |
|-----------|-------------|
| [tao-toolkit/](05-TAO工具包/Guide.md) | NVIDIA TAO Toolkit——微调 NGC 预训练模型、剪枝、QAT、导出为 TensorRT，并部署到 Jetson，全程无需编写训练循环 |
| [small-object-detection-jetson/](04-Jetson小目标检测/Guide.md) | **项目：** Jetson 上的小目标检测——使用 VisDrone2019-DET、YOLOv8、最佳实践与 DeepStream 部署的完整演练 |
| [non-contact-monitoring-edge/](03-非接触监测边缘/Guide.md) | **项目：** 非接触式监测——RGB/Depth + 热成像融合，0.8–3 Hz 微波动提取（EVM、带通），边缘部署，BLE/MQTT IoT |
| [llm-optimization-jetson/](02-Jetson-LLM优化/Guide.md) | **指南：** Jetson 上的大语言模型优化——量化（AWQ/GGUF）、模型选型、KV cache 管理、FlashAttention、TensorRT-LLM、投机解码、内存预算 |
| [jetson-llm-runtime/](01-Jetson-LLM运行时/Guide.md) | **项目：** 内存优先的大语言模型 runtime——fork llama.cpp，加入 Jetson 内存管理器，为 Orin 调优的 CUDA kernel，功耗感知推理，OpenAI 兼容 API |

---

## 1. 为什么要优化？——边缘约束

### 问题

一个在云端 A100 GPU 上跑得很好的模型，不加优化就无法在 Jetson Orin Nano 上达到可接受的运行效果。

```
A100 GPU (cloud):
  Memory:  80 GB HBM2e
  BW:      2 TB/s
  Power:   400W
  Cost:    ~$2/hour cloud

Orin Nano 8GB (edge):
  Memory:  8 GB LPDDR5 (shared CPU+GPU)
  BW:      68 GB/s
  Power:   5–15W
  Cost:    one-time hardware

YOLOv8x (unoptimized FP32):
  Model size: 136 MB
  GPU memory: ~1.2 GB
  FPS on A100: 500+
  FPS on Orin Nano FP32: ~4   ← unusable
  FPS on Orin Nano FP16: ~18
  FPS on Orin Nano INT8: ~32  ← acceptable
```

### 优化目标

| 目标       | 技术                         | 典型收益      |
|--------------|-----------------------------------|-------------------|
| 延迟      | TensorRT + FP16/INT8              | 3–8×              |
| 内存       | 量化、剪枝             | 缩小 2–4×      |
| 功耗        | 更低精度、DLA 卸载      | 瓦数降低 2–3×   |
| 吞吐   | 批处理、CUDA graphs             | 2–5× FPS          |

### 准确率与效率的取舍

```
Accuracy
 99% |  FP32   ──────────────────────
 98% |         FP16 ─────────────────
 97% |              INT8 ────────────
 95% |                   INT4 ───────
 90% |                        PRUNE ─
      ──────────────────────────────→ Speed / Efficiency
```

目标是在这条曲线上向右移动，同时保持在最低准确率要求之上。

---

## 2. 量化——从第一性原理出发

### 量化做了什么

量化把浮点值映射为整数：

```
FP32:   1.2847  stored as 4 bytes (32 bits)
INT8:   127     stored as 1 byte  (8 bits)

Compression: 4× smaller
Speed:       4–8× faster (integer ops + Tensor Core)
```

### 数学原理：仿射量化

每个张量都用两个参数量化：**scale (s)** 和 **zero-point (z)**：

```
Quantize:   q = round(x / s + z)    clip to [q_min, q_max]
Dequantize: x̂ = s × (q - z)

For INT8 (symmetric, zero-point = 0):
  s = max(|x|) / 127
  q = round(x / s)

For UINT8 (asymmetric):
  s = (x_max - x_min) / 255
  z = round(-x_min / s)
  q = round(x / s + z)
```

### 量化误差

```python
import numpy as np

# Simulate quantizing a weight tensor
weights = np.random.randn(256, 256).astype(np.float32)

# Symmetric INT8 quantization
scale = np.max(np.abs(weights)) / 127.0
quantized = np.round(weights / scale).astype(np.int8)
dequantized = quantized.astype(np.float32) * scale

# Measure error
error = np.abs(weights - dequantized)
print(f"Max quantization error: {error.max():.6f}")
print(f"Mean quantization error: {error.mean():.6f}")
print(f"SNR: {20 * np.log10(np.std(weights) / np.std(error)):.1f} dB")

# Typical results:
# Max quantization error: 0.012
# Mean quantization error: 0.003
# SNR: ~40 dB  (good — well above audible noise floor analogy)
```


<details>
<summary>English original</summary>

**ML and AI**

<div class="course-identity auto-course" style="--course-accent: #db2777; --course-accent-rgb: 219, 39, 119;" markdown="1">
<div class="course-identity__icon">MAA</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Jetson Track</p>
<p class="course-identity__title">Specialized course identity for ML and AI.</p>
<p class="course-identity__meta">Artifact: Jetson integration demo · Measure: latency, memory, power, logs</p>
</div>
</div>


**Phase 4 — Track B — Module 5.5** · Application Development

> **Focus:** Understand optimization from first principles using **tinygrad**, then apply the full porting pipeline to **Jetson Orin Nano 8GB** with TensorRT. Every concept has working code.

**Hub:** [5. Application Development](../Guide.md)

---

**Deep Dive Subfolders**

| Subfolder | Description |
|-----------|-------------|
| [tao-toolkit/](05-TAO工具包/Guide.md) | NVIDIA TAO Toolkit — fine-tune NGC pre-trained models, prune, QAT, export to TensorRT, and deploy on Jetson without writing a training loop |
| [small-object-detection-jetson/](04-Jetson小目标检测/Guide.md) | **Project:** Small object detection on Jetson — full walkthrough using VisDrone2019-DET, YOLOv8, best practices, and DeepStream deployment |
| [non-contact-monitoring-edge/](03-非接触监测边缘/Guide.md) | **Project:** Non-contact monitoring — RGB/Depth + thermal fusion, 0.8–3 Hz micro-fluctuation extraction (EVM, bandpass), edge deployment, BLE/MQTT IoT |
| [llm-optimization-jetson/](02-Jetson-LLM优化/Guide.md) | **Guide:** LLM optimization on Jetson — quantization (AWQ/GGUF), model selection, KV cache management, FlashAttention, TensorRT-LLM, speculative decoding, memory budgeting |
| [jetson-llm-runtime/](01-Jetson-LLM运行时/Guide.md) | **Project:** Memory-first LLM runtime — fork llama.cpp, add Jetson memory manager, Orin-tuned CUDA kernels, power-aware inference, OpenAI-compatible API |

---

**1. Why Optimize? — The Edge Constraints**

**The Problem**

A model that runs fine on a cloud A100 GPU will not run acceptably on Jetson Orin Nano without optimization.

```
A100 GPU (cloud):
  Memory:  80 GB HBM2e
  BW:      2 TB/s
  Power:   400W
  Cost:    ~$2/hour cloud

Orin Nano 8GB (edge):
  Memory:  8 GB LPDDR5 (shared CPU+GPU)
  BW:      68 GB/s
  Power:   5–15W
  Cost:    one-time hardware

YOLOv8x (unoptimized FP32):
  Model size: 136 MB
  GPU memory: ~1.2 GB
  FPS on A100: 500+
  FPS on Orin Nano FP32: ~4   ← unusable
  FPS on Orin Nano FP16: ~18
  FPS on Orin Nano INT8: ~32  ← acceptable
```

**Optimization Targets**

| Target       | Technique                         | Typical Gain      |
|--------------|-----------------------------------|-------------------|
| Latency      | TensorRT + FP16/INT8              | 3–8×              |
| Memory       | Quantization, pruning             | 2–4× smaller      |
| Power        | Lower precision, DLA offload      | 2–3× less watts   |
| Throughput   | Batching, CUDA graphs             | 2–5× FPS          |

**The Accuracy-Efficiency Tradeoff**

```
Accuracy
 99% |  FP32   ──────────────────────
 98% |         FP16 ─────────────────
 97% |              INT8 ────────────
 95% |                   INT4 ───────
 90% |                        PRUNE ─
      ──────────────────────────────→ Speed / Efficiency
```

The goal is to move right on this curve while staying above your minimum accuracy requirement.

---

**2. Quantization — From First Principles**

**What Quantization Does**

Quantization maps floating-point values to integers:

```
FP32:   1.2847  stored as 4 bytes (32 bits)
INT8:   127     stored as 1 byte  (8 bits)

Compression: 4× smaller
Speed:       4–8× faster (integer ops + Tensor Core)
```

**The Math: Affine Quantization**

Every tensor is quantized with two parameters: **scale (s)** and **zero-point (z)**:

```
Quantize:   q = round(x / s + z)    clip to [q_min, q_max]
Dequantize: x̂ = s × (q - z)

For INT8 (symmetric, zero-point = 0):
  s = max(|x|) / 127
  q = round(x / s)

For UINT8 (asymmetric):
  s = (x_max - x_min) / 255
  z = round(-x_min / s)
  q = round(x / s + z)
```

**Quantization Error**

```python
import numpy as np

# Simulate quantizing a weight tensor
weights = np.random.randn(256, 256).astype(np.float32)

# Symmetric INT8 quantization
scale = np.max(np.abs(weights)) / 127.0
quantized = np.round(weights / scale).astype(np.int8)
dequantized = quantized.astype(np.float32) * scale

# Measure error
error = np.abs(weights - dequantized)
print(f"Max quantization error: {error.max():.6f}")
print(f"Mean quantization error: {error.mean():.6f}")
print(f"SNR: {20 * np.log10(np.std(weights) / np.std(error)):.1f} dB")

# Typical results:
# Max quantization error: 0.012
# Mean quantization error: 0.003
# SNR: ~40 dB  (good — well above audible noise floor analogy)
```

</details>

### 量化的失效之处

有些 layer 对量化敏感：
- 首尾 layer（直接处理原始输入/输出）
- attention layer（动态范围大）
- 批归一化参数

混合精度量化把敏感 layer 保留为 FP16，其余量化为 INT8。

---

## 3. tinygrad 中的量化

在通过 TensorRT 应用量化之前，用 tinygrad 理解量化是把它内化的最佳方式。

### 在 tinygrad 中从零实现 INT8 量化

```python
# quantization.py
from tinygrad.tensor import Tensor
import numpy as np

def quantize_tensor_int8(x: Tensor):
    """Symmetric per-tensor INT8 quantization"""
    x_np = x.numpy()
    scale = float(np.max(np.abs(x_np))) / 127.0
    scale = max(scale, 1e-8)    # avoid division by zero

    q = np.round(x_np / scale).astype(np.int8)
    q = np.clip(q, -128, 127)
    return q, scale

def dequantize_tensor(q: np.ndarray, scale: float) -> Tensor:
    return Tensor(q.astype(np.float32) * scale)

def quantized_linear(x: Tensor, W_q: np.ndarray, W_scale: float,
                     b: Tensor = None) -> Tensor:
    """
    Simulated quantized linear layer:
    1. Quantize input
    2. Integer matmul (simulated in float for this demo)
    3. Dequantize output
    """
    # Quantize input
    x_q, x_scale = quantize_tensor_int8(x)

    # Integer matmul (in practice, hardware does this natively)
    # output_q = x_q @ W_q  (INT8 @ INT8 = INT32 accumulation)
    out_q = x_q.astype(np.int32) @ W_q.astype(np.int32)

    # Dequantize: multiply by combined scale
    combined_scale = x_scale * W_scale
    out = Tensor(out_q.astype(np.float32) * combined_scale)

    if b is not None:
        out = out + b
    return out

# ── Demo ──────────────────────────────────────────────────────
# Original layer
W = Tensor.randn(128, 64)
x = Tensor.randn(32, 128)  # batch=32, input=128

# FP32 reference
out_fp32 = x.matmul(W)

# Quantized version
W_q, W_scale = quantize_tensor_int8(W)
out_int8 = quantized_linear(x, W_q, W_scale)

# Compare
diff = np.abs(out_fp32.numpy() - out_int8.numpy())
print(f"Mean error vs FP32: {diff.mean():.6f}")
print(f"Max error  vs FP32: {diff.max():.6f}")
```

### 逐通道量化（更高的准确率）

```python
def quantize_weight_per_channel(W: Tensor):
    """
    Per-output-channel quantization.
    Each output neuron gets its own scale → much better accuracy than per-tensor.
    """
    W_np = W.numpy()                         # shape [out, in]
    # Compute scale per output channel (row)
    scales = np.max(np.abs(W_np), axis=1, keepdims=True) / 127.0
    scales = np.maximum(scales, 1e-8)

    W_q = np.round(W_np / scales).astype(np.int8)
    W_q = np.clip(W_q, -128, 127)
    return W_q, scales.squeeze()

# Compare per-tensor vs per-channel accuracy
W = Tensor.randn(256, 256)
x = Tensor.randn(16, 256)
ref = x.matmul(W).numpy()

# Per-tensor
W_q_pt, W_s_pt = quantize_tensor_int8(W)
out_pt = x.numpy().astype(np.float32) @ W_q_pt.astype(np.float32) * W_s_pt

# Per-channel
W_q_pc, W_s_pc = quantize_weight_per_channel(W)
out_pc = (x.numpy().astype(np.float32) @ W_q_pc.astype(np.float32)) * W_s_pc

print(f"Per-tensor  mean error: {np.abs(ref - out_pt).mean():.6f}")
print(f"Per-channel mean error: {np.abs(ref - out_pc).mean():.6f}")
# Per-channel is typically 2-3× more accurate
```


<details>
<summary>English original</summary>

**Where Quantization Breaks**

Some layers are sensitive to quantization:
- First and last layers (directly process raw inputs/outputs)
- Attention layers (large dynamic range)
- Batch normalization parameters

Mixed-precision quantization keeps sensitive layers in FP16 and quantizes the rest to INT8.

---

**3. Quantization in tinygrad**

Understanding quantization through tinygrad is the best way to internalize it before applying it via TensorRT.

**Implement INT8 Quantization from Scratch in tinygrad**

```python
# quantization.py
from tinygrad.tensor import Tensor
import numpy as np

def quantize_tensor_int8(x: Tensor):
    """Symmetric per-tensor INT8 quantization"""
    x_np = x.numpy()
    scale = float(np.max(np.abs(x_np))) / 127.0
    scale = max(scale, 1e-8)    # avoid division by zero

    q = np.round(x_np / scale).astype(np.int8)
    q = np.clip(q, -128, 127)
    return q, scale

def dequantize_tensor(q: np.ndarray, scale: float) -> Tensor:
    return Tensor(q.astype(np.float32) * scale)

def quantized_linear(x: Tensor, W_q: np.ndarray, W_scale: float,
                     b: Tensor = None) -> Tensor:
    """
    Simulated quantized linear layer:
    1. Quantize input
    2. Integer matmul (simulated in float for this demo)
    3. Dequantize output
    """
    # Quantize input
    x_q, x_scale = quantize_tensor_int8(x)

    # Integer matmul (in practice, hardware does this natively)
    # output_q = x_q @ W_q  (INT8 @ INT8 = INT32 accumulation)
    out_q = x_q.astype(np.int32) @ W_q.astype(np.int32)

    # Dequantize: multiply by combined scale
    combined_scale = x_scale * W_scale
    out = Tensor(out_q.astype(np.float32) * combined_scale)

    if b is not None:
        out = out + b
    return out

# ── Demo ──────────────────────────────────────────────────────
# Original layer
W = Tensor.randn(128, 64)
x = Tensor.randn(32, 128)  # batch=32, input=128

# FP32 reference
out_fp32 = x.matmul(W)

# Quantized version
W_q, W_scale = quantize_tensor_int8(W)
out_int8 = quantized_linear(x, W_q, W_scale)

# Compare
diff = np.abs(out_fp32.numpy() - out_int8.numpy())
print(f"Mean error vs FP32: {diff.mean():.6f}")
print(f"Max error  vs FP32: {diff.max():.6f}")
```

**Per-Channel Quantization (Better Accuracy)**

```python
def quantize_weight_per_channel(W: Tensor):
    """
    Per-output-channel quantization.
    Each output neuron gets its own scale → much better accuracy than per-tensor.
    """
    W_np = W.numpy()                         # shape [out, in]
    # Compute scale per output channel (row)
    scales = np.max(np.abs(W_np), axis=1, keepdims=True) / 127.0
    scales = np.maximum(scales, 1e-8)

    W_q = np.round(W_np / scales).astype(np.int8)
    W_q = np.clip(W_q, -128, 127)
    return W_q, scales.squeeze()

# Compare per-tensor vs per-channel accuracy
W = Tensor.randn(256, 256)
x = Tensor.randn(16, 256)
ref = x.matmul(W).numpy()

# Per-tensor
W_q_pt, W_s_pt = quantize_tensor_int8(W)
out_pt = x.numpy().astype(np.float32) @ W_q_pt.astype(np.float32) * W_s_pt

# Per-channel
W_q_pc, W_s_pc = quantize_weight_per_channel(W)
out_pc = (x.numpy().astype(np.float32) @ W_q_pc.astype(np.float32)) * W_s_pc

print(f"Per-tensor  mean error: {np.abs(ref - out_pt).mean():.6f}")
print(f"Per-channel mean error: {np.abs(ref - out_pc).mean():.6f}")
# Per-channel is typically 2-3× more accurate
```

</details>

### 在 tinygrad 中量化多层感知机

```python
from tinygrad.tensor import Tensor
from tinygrad.nn.optim import Adam
import numpy as np

class QuantizedMLP:
    """MLP where weights are stored as INT8, but inference simulated in FP32"""

    def __init__(self, layers):
        # Train in FP32
        self.fp32_weights = [Tensor.kaiming_uniform(layers[i], layers[i+1])
                             for i in range(len(layers)-1)]
        self.biases = [Tensor.zeros(layers[i+1])
                       for i in range(len(layers)-1)]
        self.quantized = False

    def quantize(self):
        """Quantize all weights to INT8 after training"""
        self.int8_weights = []
        self.scales = []
        for W in self.fp32_weights:
            W_q, scale = quantize_weight_per_channel(W)
            self.int8_weights.append(W_q)
            self.scales.append(scale)
        self.quantized = True
        print("Model quantized to INT8")

    def __call__(self, x):
        if not self.quantized:
            # FP32 training forward pass
            for i, (W, b) in enumerate(zip(self.fp32_weights, self.biases)):
                x = x.matmul(W) + b
                if i < len(self.fp32_weights) - 1:
                    x = x.relu()
        else:
            # Simulated INT8 inference forward pass
            for i, (W_q, scale, b) in enumerate(zip(self.int8_weights, self.scales, self.biases)):
                x = quantized_linear(x, W_q, scale, b)
                if i < len(self.int8_weights) - 1:
                    x = x.relu()
        return x.softmax()

    def parameters(self):
        return self.fp32_weights + self.biases

# Train in FP32
model = QuantizedMLP([784, 256, 128, 10])
optimizer = Adam(model.parameters(), lr=1e-3)

# ... (training loop here) ...

# After training: quantize and compare accuracy
model_q = QuantizedMLP([784, 256, 128, 10])
model_q.fp32_weights = model.fp32_weights
model_q.biases = model.biases
model_q.quantize()

# Memory comparison
fp32_bytes = sum(W.numpy().nbytes for W in model.fp32_weights)
int8_bytes  = sum(W.nbytes for W in model_q.int8_weights)
print(f"FP32 weights: {fp32_bytes/1024:.1f} KB")
print(f"INT8 weights: {int8_bytes/1024:.1f} KB")
print(f"Compression:  {fp32_bytes/int8_bytes:.1f}×")
```

---

## 4. 使用 TensorRT 进行训练后量化（PTQ）

### INT8 校准 —— 为何重要

INT8 量化需要知道推理过程中流经每个激活值的数值**范围**。这要通过在**校准数据集**（100–1000 个有代表性的样本）上运行模型来确定。

```
Without calibration:
  Scale = max possible value (very conservative)
  Most values underutilize the INT8 range → poor accuracy

With calibration:
  Scale = percentile of actual activation range
  INT8 range fully utilized → much better accuracy
```


<details>
<summary>English original</summary>

**Quantizing an MLP in tinygrad**

```python
from tinygrad.tensor import Tensor
from tinygrad.nn.optim import Adam
import numpy as np

class QuantizedMLP:
    """MLP where weights are stored as INT8, but inference simulated in FP32"""

    def __init__(self, layers):
        # Train in FP32
        self.fp32_weights = [Tensor.kaiming_uniform(layers[i], layers[i+1])
                             for i in range(len(layers)-1)]
        self.biases = [Tensor.zeros(layers[i+1])
                       for i in range(len(layers)-1)]
        self.quantized = False

    def quantize(self):
        """Quantize all weights to INT8 after training"""
        self.int8_weights = []
        self.scales = []
        for W in self.fp32_weights:
            W_q, scale = quantize_weight_per_channel(W)
            self.int8_weights.append(W_q)
            self.scales.append(scale)
        self.quantized = True
        print("Model quantized to INT8")

    def __call__(self, x):
        if not self.quantized:
            # FP32 training forward pass
            for i, (W, b) in enumerate(zip(self.fp32_weights, self.biases)):
                x = x.matmul(W) + b
                if i < len(self.fp32_weights) - 1:
                    x = x.relu()
        else:
            # Simulated INT8 inference forward pass
            for i, (W_q, scale, b) in enumerate(zip(self.int8_weights, self.scales, self.biases)):
                x = quantized_linear(x, W_q, scale, b)
                if i < len(self.int8_weights) - 1:
                    x = x.relu()
        return x.softmax()

    def parameters(self):
        return self.fp32_weights + self.biases

# Train in FP32
model = QuantizedMLP([784, 256, 128, 10])
optimizer = Adam(model.parameters(), lr=1e-3)

# ... (training loop here) ...

# After training: quantize and compare accuracy
model_q = QuantizedMLP([784, 256, 128, 10])
model_q.fp32_weights = model.fp32_weights
model_q.biases = model.biases
model_q.quantize()

# Memory comparison
fp32_bytes = sum(W.numpy().nbytes for W in model.fp32_weights)
int8_bytes  = sum(W.nbytes for W in model_q.int8_weights)
print(f"FP32 weights: {fp32_bytes/1024:.1f} KB")
print(f"INT8 weights: {int8_bytes/1024:.1f} KB")
print(f"Compression:  {fp32_bytes/int8_bytes:.1f}×")
```

---

**4. Post-Training Quantization (PTQ) with TensorRT**

**INT8 Calibration — Why It Matters**

INT8 quantization needs to know the **range** of values that flow through each activation during inference. This is determined by running the model on a **calibration dataset** (100–1000 representative samples).

```
Without calibration:
  Scale = max possible value (very conservative)
  Most values underutilize the INT8 range → poor accuracy

With calibration:
  Scale = percentile of actual activation range
  INT8 range fully utilized → much better accuracy
```

</details>

### 用 TensorRT 做 INT8 校准

```python
# int8_calibrator.py
import tensorrt as trt
import pycuda.driver as cuda
import numpy as np

class EntropyCalibrator(trt.IInt8EntropyCalibrator2):
    """
    Entropy calibration: minimizes KL divergence between FP32 and INT8 distributions.
    Usually best for CNNs.
    """
    def __init__(self, calibration_data, cache_file='calib.cache'):
        super().__init__()
        self.data = calibration_data        # list of numpy arrays, shape [1, C, H, W]
        self.idx = 0
        self.cache_file = cache_file

        # Allocate device buffer for one batch
        self.device_input = cuda.mem_alloc(self.data[0].nbytes)

    def get_batch_size(self):
        return 1

    def get_batch(self, names):
        if self.idx >= len(self.data):
            return None                     # signal end of calibration data

        batch = self.data[self.idx]
        cuda.memcpy_htod(self.device_input, batch)
        self.idx += 1
        return [int(self.device_input)]

    def read_calibration_cache(self):
        if os.path.exists(self.cache_file):
            with open(self.cache_file, 'rb') as f:
                return f.read()
        return None

    def write_calibration_cache(self, cache):
        with open(self.cache_file, 'wb') as f:
            f.write(cache)

def build_int8_engine(onnx_path, engine_path, calibration_data):
    TRT_LOGGER = trt.Logger(trt.Logger.WARNING)

    with trt.Builder(TRT_LOGGER) as builder, \
         builder.create_network(1 << int(trt.NetworkDefinitionCreationFlag.EXPLICIT_BATCH)) as net, \
         trt.OnnxParser(net, TRT_LOGGER) as parser, \
         builder.create_builder_config() as config:

        # 2 GB workspace
        config.set_memory_pool_limit(trt.MemoryPoolType.WORKSPACE, 2 << 30)

        # Enable INT8
        config.set_flag(trt.BuilderFlag.INT8)
        config.set_flag(trt.BuilderFlag.FP16)          # FP16 fallback for INT8 unsupported layers

        # Attach calibrator
        calibrator = EntropyCalibrator(calibration_data)
        config.int8_calibrator = calibrator

        with open(onnx_path, 'rb') as f:
            parser.parse(f.read())

        engine_bytes = builder.build_serialized_network(net, config)
        with open(engine_path, 'wb') as f:
            f.write(engine_bytes)
        print(f"INT8 engine saved: {engine_path}")

# Prepare calibration data from your dataset
import cv2, os

def prepare_calibration_data(image_dir, n=500, size=(640, 640)):
    images = []
    for fname in os.listdir(image_dir)[:n]:
        img = cv2.imread(os.path.join(image_dir, fname))
        img = cv2.resize(img, size)
        img = img[:,:,::-1].transpose(2, 0, 1)          # BGR→RGB, HWC→CHW
        img = img.astype(np.float32) / 255.0
        img = np.expand_dims(img, 0)                     # [1, 3, H, W]
        images.append(np.ascontiguousarray(img))
    return images

calib_data = prepare_calibration_data('/path/to/calib/images')
build_int8_engine('model.onnx', 'model_int8.engine', calib_data)
```

### 检查 TensorRT 量化了什么

```bash
# See which layers are INT8 vs FP16 vs FP32
trtexec --loadEngine=model_int8.engine \
        --verbose 2>&1 | grep -E "(INT8|FP16|FP32)" | head -40

# Build with layer timing info
trtexec --onnx=model.onnx \
        --int8 \
        --calib=calib.cache \
        --saveEngine=model_int8.engine \
        --verbose \
        --separateProfileRun \
        --avgRuns=100 2>&1 | grep "Timing"
```

### 准确率与速度对比脚本

```python
import time, numpy as np

def benchmark(engine_path, test_data, labels, n_runs=200):
    inferencer = TRTInferencer(engine_path)    # from Jetson Platform guide

    # Accuracy
    correct = 0
    for x, y in zip(test_data[:1000], labels[:1000]):
        pred = inferencer.infer(x).argmax()
        correct += (pred == y)
    accuracy = correct / 1000

    # Latency
    dummy = test_data[0]
    for _ in range(50):  # warmup
        inferencer.infer(dummy)

    times = []
    for _ in range(n_runs):
        t0 = time.perf_counter()
        inferencer.infer(dummy)
        times.append((time.perf_counter() - t0) * 1000)

    return accuracy, np.mean(times), 1000/np.mean(times)

for engine in ['model_fp32.engine', 'model_fp16.engine', 'model_int8.engine']:
    acc, lat, fps = benchmark(engine, X_test, Y_test)
    print(f"{engine:25s}  acc={acc:.3f}  lat={lat:.1f}ms  fps={fps:.1f}")

# Expected on Orin Nano 8GB (YOLOv8n example):
# model_fp32.engine     acc=0.921  lat=28.3ms  fps=35.3
# model_fp16.engine     acc=0.920  lat=12.1ms  fps=82.6
# model_int8.engine     acc=0.918  lat=7.4ms   fps=135.1
```

---

## 5. 用 tinygrad 做量化感知训练（QAT）

QAT 在**训练期间**模拟量化噪声，使模型学会对其鲁棒。结果是 INT8 准确率几乎与 FP32 持平。

### 直通估计器（STE）

问题：`round()` 在所有位置的梯度都为零 —— 反向传播会失败。

解决方案：**直通估计器** —— 让梯度穿过取整操作，视其为恒等映射：

```
Forward:  q = round(x / s) * s   (quantize + dequantize)
Backward: dL/dx = dL/dq          (ignore the rounding, pass gradient through)
```

### tinygrad 中的 QAT 实现

```python
# qat.py
from tinygrad.tensor import Tensor
import numpy as np

class FakeQuantize:
    """
    Fake-quantize: simulates INT8 quantization in the forward pass,
    uses straight-through estimator in backward pass.
    """
    def __init__(self, num_bits=8, symmetric=True):
        self.num_bits = num_bits
        self.q_min = -(2 ** (num_bits - 1))     # -128 for INT8
        self.q_max =  (2 ** (num_bits - 1)) - 1  # 127 for INT8

        # Learnable scale (initialized to 1.0)
        self.scale = Tensor([1.0])

    def __call__(self, x: Tensor) -> Tensor:
        # Compute scale from current batch statistics
        x_max = float(x.abs().max().numpy())
        scale = max(x_max, 1e-8) / self.q_max
        self.scale = Tensor([scale])

        # Quantize: q = clip(round(x/s), q_min, q_max) * s
        x_scaled = x / scale
        # tinygrad doesn't have round() that supports backprop STE natively,
        # so we approximate: clamp then use the value
        x_clamped = x_scaled.clip(self.q_min, self.q_max)

        # Simulate quantization noise: add uniform noise ± 0.5 * scale
        # (approximates the effect of rounding for gradient purposes)
        noise = Tensor(np.random.uniform(-0.5, 0.5, x.shape).astype(np.float32))
        x_quant_sim = (x_clamped + noise) * scale

        return x_quant_sim

class QATLinear:
    """Linear layer with fake-quantized weights and activations"""
    def __init__(self, n_in, n_out):
        self.weight = Tensor.kaiming_uniform(n_in, n_out)
        self.bias   = Tensor.zeros(n_out)
        self.w_fq   = FakeQuantize(num_bits=8)
        self.a_fq   = FakeQuantize(num_bits=8)

    def __call__(self, x):
        w_q = self.w_fq(self.weight)    # fake-quantize weights
        out = x.matmul(w_q) + self.bias
        return self.a_fq(out)            # fake-quantize activations

    def parameters(self):
        return [self.weight, self.bias]

class QATMLP:
    def __init__(self, layers):
        self.layers = [QATLinear(layers[i], layers[i+1])
                       for i in range(len(layers)-1)]

    def __call__(self, x):
        for i, layer in enumerate(self.layers[:-1]):
            x = layer(x).relu()
        return self.layers[-1](x).softmax()

    def parameters(self):
        params = []
        for layer in self.layers:
            params.extend(layer.parameters())
        return params

# QAT Training Loop
from tinygrad.nn.optim import Adam

model = QATMLP([784, 256, 128, 10])
optimizer = Adam(model.parameters(), lr=5e-4)   # Lower LR for QAT

# Phase 1: Pretrain in FP32 (skip fake-quant initially)
# Phase 2: Enable QAT (fake-quant active) and fine-tune 3-5 epochs
# This two-phase approach gives best results

BATCH = 64
for epoch in range(5):    # QAT fine-tuning
    for i in range(0, len(X_train), BATCH):
        xb = Tensor(X_train[i:i+BATCH].astype(np.float32))
        yb_oh = np.zeros((BATCH, 10), dtype=np.float32)
        yb_oh[np.arange(BATCH), Y_train[i:i+BATCH]] = 1.0

        out = model(xb)
        loss = -(Tensor(yb_oh) * out.log()).sum(axis=1).mean()

        optimizer.zero_grad()
        loss.backward()
        optimizer.step()

    print(f"QAT Epoch {epoch+1}: loss={loss.numpy():.4f}")
```

---

## 6. 剪枝 —— 结构化与非结构化

### 非结构化剪枝（权重幅值）

移除低于阈值的单个权重。实现简单，但没有稀疏硬件时难以在 GPU 上加速。

```python
# magnitude_pruning.py
from tinygrad.tensor import Tensor
import numpy as np

def magnitude_prune(model, sparsity=0.5):
    """
    Prune the bottom `sparsity` fraction of weights by magnitude.
    Returns masks (1=keep, 0=prune).
    """
    masks = {}
    all_weights = []

    for name, W in model.named_weights():
        all_weights.append(np.abs(W.numpy()).flatten())

    # Find global threshold
    all_weights_flat = np.concatenate(all_weights)
    threshold = np.percentile(all_weights_flat, sparsity * 100)

    for name, W in model.named_weights():
        mask = (np.abs(W.numpy()) > threshold).astype(np.float32)
        masks[name] = mask
        actual_sparsity = 1.0 - mask.mean()
        print(f"  {name}: {actual_sparsity:.1%} sparse")

    return masks

def apply_masks(model, masks):
    """Zero out pruned weights"""
    for name, W in model.named_weights():
        if name in masks:
            W_pruned = W.numpy() * masks[name]
            # In-place update
            W.assign(Tensor(W_pruned))

# Usage with our MLP
masks = magnitude_prune(model, sparsity=0.7)   # prune 70% of weights
apply_masks(model, masks)

# After pruning, fine-tune for 1-2 epochs to recover accuracy
# During fine-tuning, re-apply masks after each update to keep weights zeroed
```

### 结构化剪枝（通道剪枝）

移除整个神经元/通道。对硬件友好——剪枝后的模型确实更小、更快。

```python
def structured_prune_layer(W: Tensor, keep_fraction=0.5):
    """
    Prune output neurons with smallest L1 norm of their weight vector.
    Returns pruned weight matrix.

    W shape: [n_in, n_out]
    Prunes output neurons (columns).
    """
    W_np = W.numpy()
    n_out = W_np.shape[1]
    n_keep = max(1, int(n_out * keep_fraction))

    # L1 norm of each output neuron's weights
    norms = np.sum(np.abs(W_np), axis=0)      # shape [n_out]

    # Keep top n_keep neurons
    keep_idx = np.argsort(norms)[-n_keep:]
    keep_idx = np.sort(keep_idx)

    return Tensor(W_np[:, keep_idx]), keep_idx

# Example: prune hidden layer
W1 = Tensor.randn(784, 256)   # first layer
W1_pruned, keep_idx = structured_prune_layer(W1, keep_fraction=0.5)
print(f"Layer 1: {W1.shape} → {W1_pruned.shape}")
# Layer 1: (784, 256) → (784, 128)

# Next layer input must match pruned output
W2 = Tensor.randn(256, 128)
W2_pruned = Tensor(W2.numpy()[keep_idx, :])   # keep matching rows
print(f"Layer 2: {W2.shape} → {W2_pruned.shape}")
# Layer 2: (256, 128) → (128, 128)

# Model is now physically smaller — real speedup
```

### 用 L1 正则化促进稀疏

在训练期间加入 L1 惩罚项，把权重推向零，使其后续更易被剪枝：

```python
# During training with L1 regularization
l1_lambda = 1e-4

for i in range(0, len(X_train), BATCH):
    xb = Tensor(X_train[i:i+BATCH].astype(np.float32))
    out = model(xb)
    task_loss = ...  # cross-entropy

    # L1 penalty: sum of absolute values of all weights
    l1_loss = sum(W.abs().sum() for W in model.parameters())
    total_loss = task_loss + l1_lambda * l1_loss

    optimizer.zero_grad()
    total_loss.backward()
    optimizer.step()
```

---

## 7. 知识蒸馏

训练一个小 **学生模型** 去模仿大的 **教师模型**。学生模型从软概率输出中学习（其携带的信息比硬标签更多）。

```
Teacher (large, slow):      ResNet-50, 25M params, 80ms on Orin Nano
                 ↓ soft predictions (probabilities)
Student (small, fast):      MobileNet, 3M params, 8ms on Orin Nano
                 ↓ learns teacher's "knowledge"
Student accuracy ≈ Teacher accuracy   (within 1-2%)
Student speed    = 10×  faster
```

### 蒸馏损失

```
L_distill = α × L_CE(student_pred, hard_labels)
           + (1-α) × L_KD(student_soft, teacher_soft, temperature T)

L_KD = KL divergence between teacher and student softmax outputs at temperature T

Temperature T:
  T=1: normal softmax (sharp)
  T>1: softer distribution (reveals more inter-class relationships)
  Typical: T=3 or T=4
```


<details>
<summary>English original</summary>

**Unstructured Pruning (Weight Magnitude)**

Remove individual weights below a threshold. Easy to implement, hard to accelerate on GPU without sparse hardware.

```python
# magnitude_pruning.py
from tinygrad.tensor import Tensor
import numpy as np

def magnitude_prune(model, sparsity=0.5):
    """
    Prune the bottom `sparsity` fraction of weights by magnitude.
    Returns masks (1=keep, 0=prune).
    """
    masks = {}
    all_weights = []

    for name, W in model.named_weights():
        all_weights.append(np.abs(W.numpy()).flatten())

    # Find global threshold
    all_weights_flat = np.concatenate(all_weights)
    threshold = np.percentile(all_weights_flat, sparsity * 100)

    for name, W in model.named_weights():
        mask = (np.abs(W.numpy()) > threshold).astype(np.float32)
        masks[name] = mask
        actual_sparsity = 1.0 - mask.mean()
        print(f"  {name}: {actual_sparsity:.1%} sparse")

    return masks

def apply_masks(model, masks):
    """Zero out pruned weights"""
    for name, W in model.named_weights():
        if name in masks:
            W_pruned = W.numpy() * masks[name]
            # In-place update
            W.assign(Tensor(W_pruned))

# Usage with our MLP
masks = magnitude_prune(model, sparsity=0.7)   # prune 70% of weights
apply_masks(model, masks)

# After pruning, fine-tune for 1-2 epochs to recover accuracy
# During fine-tuning, re-apply masks after each update to keep weights zeroed
```

**Structured Pruning (Channel Pruning)**

Remove entire neurons/channels. Hardware-friendly — the pruned model is actually smaller and faster.

```python
def structured_prune_layer(W: Tensor, keep_fraction=0.5):
    """
    Prune output neurons with smallest L1 norm of their weight vector.
    Returns pruned weight matrix.

    W shape: [n_in, n_out]
    Prunes output neurons (columns).
    """
    W_np = W.numpy()
    n_out = W_np.shape[1]
    n_keep = max(1, int(n_out * keep_fraction))

    # L1 norm of each output neuron's weights
    norms = np.sum(np.abs(W_np), axis=0)      # shape [n_out]

    # Keep top n_keep neurons
    keep_idx = np.argsort(norms)[-n_keep:]
    keep_idx = np.sort(keep_idx)

    return Tensor(W_np[:, keep_idx]), keep_idx

# Example: prune hidden layer
W1 = Tensor.randn(784, 256)   # first layer
W1_pruned, keep_idx = structured_prune_layer(W1, keep_fraction=0.5)
print(f"Layer 1: {W1.shape} → {W1_pruned.shape}")
# Layer 1: (784, 256) → (784, 128)

# Next layer input must match pruned output
W2 = Tensor.randn(256, 128)
W2_pruned = Tensor(W2.numpy()[keep_idx, :])   # keep matching rows
print(f"Layer 2: {W2.shape} → {W2_pruned.shape}")
# Layer 2: (256, 128) → (128, 128)

# Model is now physically smaller — real speedup
```

**L1 Regularization to Encourage Sparsity**

Add L1 penalty during training to push weights toward zero, making them easier to prune later:

```python
# During training with L1 regularization
l1_lambda = 1e-4

for i in range(0, len(X_train), BATCH):
    xb = Tensor(X_train[i:i+BATCH].astype(np.float32))
    out = model(xb)
    task_loss = ...  # cross-entropy

    # L1 penalty: sum of absolute values of all weights
    l1_loss = sum(W.abs().sum() for W in model.parameters())
    total_loss = task_loss + l1_lambda * l1_loss

    optimizer.zero_grad()
    total_loss.backward()
    optimizer.step()
```

---

**7. Knowledge Distillation**

Train a small **student** model to mimic a large **teacher** model. The student learns from soft probability outputs (which carry more information than hard labels).

```
Teacher (large, slow):      ResNet-50, 25M params, 80ms on Orin Nano
                 ↓ soft predictions (probabilities)
Student (small, fast):      MobileNet, 3M params, 8ms on Orin Nano
                 ↓ learns teacher's "knowledge"
Student accuracy ≈ Teacher accuracy   (within 1-2%)
Student speed    = 10×  faster
```

**Distillation Loss**

```
L_distill = α × L_CE(student_pred, hard_labels)
           + (1-α) × L_KD(student_soft, teacher_soft, temperature T)

L_KD = KL divergence between teacher and student softmax outputs at temperature T

Temperature T:
  T=1: normal softmax (sharp)
  T>1: softer distribution (reveals more inter-class relationships)
  Typical: T=3 or T=4
```

</details>

### tinygrad 中的知识蒸馏

```python
# distillation.py
from tinygrad.tensor import Tensor
from tinygrad.nn.optim import Adam
import numpy as np

def softmax_with_temperature(logits: Tensor, T: float) -> Tensor:
    return (logits / T).softmax()

def kl_divergence(p: Tensor, q: Tensor, eps=1e-8) -> Tensor:
    """KL(p || q) = sum(p * log(p/q))"""
    return (p * (p + eps).log() - p * (q + eps).log()).sum(axis=1).mean()

def distillation_loss(
    student_logits: Tensor,
    teacher_logits: Tensor,
    labels: Tensor,
    T: float = 4.0,
    alpha: float = 0.7
):
    """
    Combined distillation loss.
    alpha: weight of distillation loss (1-alpha = weight of cross-entropy)
    T: temperature for soft targets
    """
    # Soft targets (teacher knowledge)
    teacher_soft = softmax_with_temperature(teacher_logits, T)
    student_soft = softmax_with_temperature(student_logits, T)
    loss_kd = kl_divergence(teacher_soft, student_soft) * (T ** 2)

    # Hard targets (ground truth)
    student_prob = student_logits.softmax()
    loss_ce = -(labels * student_prob.log()).sum(axis=1).mean()

    return alpha * loss_kd + (1 - alpha) * loss_ce

# Teacher: large pretrained model (fixed, no gradient)
teacher = LargeMLP([784, 1024, 1024, 512, 10])
# ... load pretrained weights ...

# Student: small model (trained from scratch with distillation)
student = SmallMLP([784, 128, 64, 10])
optimizer = Adam(student.parameters(), lr=1e-3)

for epoch in range(20):
    for i in range(0, len(X_train), BATCH):
        xb = Tensor(X_train[i:i+BATCH].astype(np.float32))
        yb_oh = Tensor(one_hot(Y_train[i:i+BATCH], 10))

        # Teacher forward (no gradients needed)
        with Tensor.no_grad():
            teacher_logits = teacher(xb)

        # Student forward
        student_logits = student(xb)

        # Distillation loss
        loss = distillation_loss(
            student_logits, teacher_logits, yb_oh, T=4.0, alpha=0.7
        )

        optimizer.zero_grad()
        loss.backward()
        optimizer.step()

    print(f"Epoch {epoch+1}: loss={loss.numpy():.4f}")
```

---

## 8. 完整模型移植流水线

### 完整流程

```
tinygrad model  ──→  PyTorch weights  ──→  ONNX  ──→  TensorRT  ──→  Jetson
   (training)         (export)         (universal)   (optimized)   (inference)
```

### 步骤 1：在 tinygrad 中训练、导出权重

```python
# export_weights.py
import numpy as np

def export_model_weights(model, path='model_weights.npz'):
    """Export all model weights as numpy arrays"""
    weights = {}
    for i, layer in enumerate(model.linears):
        weights[f'layer_{i}_W'] = layer.w.numpy()
        weights[f'layer_{i}_b'] = layer.b.numpy()

    np.savez(path, **weights)
    print(f"Saved {len(weights)} weight tensors to {path}")
    for k, v in weights.items():
        print(f"  {k}: {v.shape} {v.dtype}")
```

### 步骤 2：将 tinygrad 权重加载进 PyTorch 以导出 ONNX

```python
# tinygrad_to_onnx.py
import torch
import torch.nn as nn
import numpy as np

class TorchMLP(nn.Module):
    """Identical architecture to tinygrad MLP"""
    def __init__(self, layers):
        super().__init__()
        self.layers = nn.ModuleList([
            nn.Linear(layers[i], layers[i+1])
            for i in range(len(layers)-1)
        ])

    def forward(self, x):
        for i, layer in enumerate(self.layers[:-1]):
            x = torch.relu(layer(x))
        return self.layers[-1](x)

def load_tinygrad_weights(torch_model, npz_path):
    """Load tinygrad weights into PyTorch model"""
    weights = np.load(npz_path)

    for i, layer in enumerate(torch_model.layers):
        W = weights[f'layer_{i}_W']
        b = weights[f'layer_{i}_b']

        # tinygrad: weight shape [n_in, n_out]
        # PyTorch:  weight shape [n_out, n_in] (transposed!)
        layer.weight.data = torch.from_numpy(W.T)
        layer.bias.data   = torch.from_numpy(b)

    print("Weights loaded from tinygrad export")

# Export to ONNX
model_pt = TorchMLP([784, 256, 128, 10])
load_tinygrad_weights(model_pt, 'model_weights.npz')
model_pt.eval()

dummy_input = torch.randn(1, 784)
torch.onnx.export(
    model_pt,
    dummy_input,
    'model.onnx',
    opset_version=17,
    input_names=['input'],
    output_names=['output'],
    dynamic_axes={'input': {0: 'batch'}, 'output': {0: 'batch'}}
)

# Verify
import onnx, onnxruntime as ort
onnx.checker.check_model(onnx.load('model.onnx'))
sess = ort.InferenceSession('model.onnx')
out_onnx = sess.run(None, {'input': dummy_input.numpy()})[0]
out_torch = model_pt(dummy_input).detach().numpy()
print(f"ONNX vs PyTorch max diff: {np.abs(out_onnx - out_torch).max():.8f}")
```

### 步骤 3：在 TensorRT 之前于 Jetson 上验证 ONNX

```bash
# Install onnxruntime for Jetson (GPU version)
pip3 install onnxruntime-gpu    # NVIDIA provides builds for Jetson
# Or install from NVIDIA wheel:
# pip3 install onnxruntime_gpu-*.whl

# Quick validation
python3 -c "
import onnxruntime as ort
import numpy as np

sess = ort.InferenceSession('model.onnx', providers=['CUDAExecutionProvider'])
x = np.random.randn(1, 784).astype(np.float32)
out = sess.run(None, {'input': x})
print('ONNX Runtime OK, output shape:', out[0].shape)
"
```

### 步骤 4：在 Jetson 上完成 ONNX → TensorRT 转换

```bash
# Always build TensorRT engines on the target Jetson (hardware-specific)

# FP16 (best balance for most models)
trtexec --onnx=model.onnx \
        --saveEngine=model_fp16.engine \
        --fp16 \
        --minShapes=input:1x784 \
        --optShapes=input:64x784 \
        --maxShapes=input:256x784 \
        --verbose 2>&1 | tee build_fp16.log

# INT8 with calibration cache
trtexec --onnx=model.onnx \
        --saveEngine=model_int8.engine \
        --int8 \
        --calib=calib.cache \
        --fp16 \
        --verbose 2>&1 | tee build_int8.log

# Check engine info after build
trtexec --loadEngine=model_fp16.engine \
        --dumpLayerInfo \
        --dumpProfile 2>&1 | head -80
```

### 步骤 5：在每个阶段交叉核对准确率

```python
# validate_pipeline.py
import numpy as np

# Load test data
X_test = np.load('x_test.npy').astype(np.float32)
Y_test = np.load('y_test.npy')

def accuracy(preds, labels):
    return (preds.argmax(axis=1) == labels).mean()

# Stage 1: tinygrad
tg_preds = model(Tensor(X_test)).numpy()
print(f"tinygrad FP32: {accuracy(tg_preds, Y_test):.3%}")

# Stage 2: ONNX Runtime
ort_preds = np.vstack([
    sess.run(None, {'input': X_test[i:i+64]})[0]
    for i in range(0, len(X_test), 64)
])
print(f"ONNX Runtime:  {accuracy(ort_preds, Y_test):.3%}")

# Stage 3: TensorRT FP16
fp16_preds = run_trt_batch('model_fp16.engine', X_test)
print(f"TRT FP16:      {accuracy(fp16_preds, Y_test):.3%}")

# Stage 4: TensorRT INT8
int8_preds = run_trt_batch('model_int8.engine', X_test)
print(f"TRT INT8:      {accuracy(int8_preds, Y_test):.3%}")

# Acceptable accuracy drop:
# ONNX vs tinygrad: < 0.01% (should be near-zero)
# FP16 vs FP32:     < 0.1%
# INT8 vs FP32:     < 1.0%
```

---

## 9. TensorRT Engine 优化 — Jetson 深入剖析

### Layer 融合

TensorRT 会自动融合相邻 layer，以降低内存带宽与 kernel 启动开销。

```
Before fusion (3 kernel launches):
  CONV → BN → ReLU

After fusion (1 kernel launch):
  CBR (Conv-BN-ReLU fused)

Result: removes 2 round-trips to GPU memory per fused block
On Orin Nano: ~15-25% latency reduction for CNN workloads
```

```bash
# See what TensorRT fused:
trtexec --onnx=model.onnx --fp16 \
        --dumpLayerInfo 2>&1 | grep -A1 "Fused"
```

### 用时序缓存加速 Engine 构建

TensorRT 在 engine 构建期间会对大量 kernel 变体进行 profile。时序缓存会保存这些结果，因此后续构建（例如同一模型、不同批大小）要快得多：

```python
config.set_flag(trt.BuilderFlag.ENABLE_TACTIC_SOURCES)

# Save timing cache
timing_cache_file = 'timing.cache'
if os.path.exists(timing_cache_file):
    with open(timing_cache_file, 'rb') as f:
        timing_cache = config.create_timing_cache(f.read())
else:
    timing_cache = config.create_timing_cache(b'')

config.set_timing_cache(timing_cache, ignore_mismatch=False)

# Build engine...
engine_bytes = builder.build_serialized_network(net, config)

# Save updated timing cache
with open(timing_cache_file, 'wb') as f:
    f.write(config.get_timing_cache().serialize())
```

### 强类型模式（TensorRT 10+）

让你显式控制张量类型，而非交由 TensorRT 自行选择：

```python
# Enable strongly typed mode
config.set_flag(trt.BuilderFlag.STRONGLY_TYPED)

# Now you must specify types for inputs and outputs explicitly
# Prevents unexpected precision downgrades in sensitive layers
```

### 用于可变批大小推理的动态形状

```python
# Build with dynamic shapes for production flexibility
profile = builder.create_optimization_profile()

profile.set_shape(
    'images',
    min=(1, 3, 640, 640),     # minimum batch size
    opt=(4, 3, 640, 640),     # most common (profile optimized for this)
    max=(16, 3, 640, 640)     # maximum batch size
)
config.add_optimization_profile(profile)

# At inference, set input shape dynamically
context.set_input_shape('images', (batch_size, 3, 640, 640))
```


<details>
<summary>English original</summary>

**Step 3: Validate ONNX on Jetson Before TensorRT**

```bash
# Install onnxruntime for Jetson (GPU version)
pip3 install onnxruntime-gpu    # NVIDIA provides builds for Jetson
# Or install from NVIDIA wheel:
# pip3 install onnxruntime_gpu-*.whl

# Quick validation
python3 -c "
import onnxruntime as ort
import numpy as np

sess = ort.InferenceSession('model.onnx', providers=['CUDAExecutionProvider'])
x = np.random.randn(1, 784).astype(np.float32)
out = sess.run(None, {'input': x})
print('ONNX Runtime OK, output shape:', out[0].shape)
"
```

**Step 4: Convert ONNX → TensorRT on Jetson**

```bash
# Always build TensorRT engines on the target Jetson (hardware-specific)

# FP16 (best balance for most models)
trtexec --onnx=model.onnx \
        --saveEngine=model_fp16.engine \
        --fp16 \
        --minShapes=input:1x784 \
        --optShapes=input:64x784 \
        --maxShapes=input:256x784 \
        --verbose 2>&1 | tee build_fp16.log

# INT8 with calibration cache
trtexec --onnx=model.onnx \
        --saveEngine=model_int8.engine \
        --int8 \
        --calib=calib.cache \
        --fp16 \
        --verbose 2>&1 | tee build_int8.log

# Check engine info after build
trtexec --loadEngine=model_fp16.engine \
        --dumpLayerInfo \
        --dumpProfile 2>&1 | head -80
```

**Step 5: Cross-Check Accuracy at Each Stage**

```python
# validate_pipeline.py
import numpy as np

# Load test data
X_test = np.load('x_test.npy').astype(np.float32)
Y_test = np.load('y_test.npy')

def accuracy(preds, labels):
    return (preds.argmax(axis=1) == labels).mean()

# Stage 1: tinygrad
tg_preds = model(Tensor(X_test)).numpy()
print(f"tinygrad FP32: {accuracy(tg_preds, Y_test):.3%}")

# Stage 2: ONNX Runtime
ort_preds = np.vstack([
    sess.run(None, {'input': X_test[i:i+64]})[0]
    for i in range(0, len(X_test), 64)
])
print(f"ONNX Runtime:  {accuracy(ort_preds, Y_test):.3%}")

# Stage 3: TensorRT FP16
fp16_preds = run_trt_batch('model_fp16.engine', X_test)
print(f"TRT FP16:      {accuracy(fp16_preds, Y_test):.3%}")

# Stage 4: TensorRT INT8
int8_preds = run_trt_batch('model_int8.engine', X_test)
print(f"TRT INT8:      {accuracy(int8_preds, Y_test):.3%}")

# Acceptable accuracy drop:
# ONNX vs tinygrad: < 0.01% (should be near-zero)
# FP16 vs FP32:     < 0.1%
# INT8 vs FP32:     < 1.0%
```

---

**9. TensorRT Engine Optimization — Jetson Deep Dive**

**Layer Fusion**

TensorRT automatically fuses adjacent layers to reduce memory bandwidth and kernel launch overhead.

```
Before fusion (3 kernel launches):
  CONV → BN → ReLU

After fusion (1 kernel launch):
  CBR (Conv-BN-ReLU fused)

Result: removes 2 round-trips to GPU memory per fused block
On Orin Nano: ~15-25% latency reduction for CNN workloads
```

```bash
# See what TensorRT fused:
trtexec --onnx=model.onnx --fp16 \
        --dumpLayerInfo 2>&1 | grep -A1 "Fused"
```

**Timing Cache for Faster Engine Builds**

TensorRT profiles many kernel variants during engine build. The timing cache saves these results so subsequent builds (e.g., same model, different batch size) are much faster:

```python
config.set_flag(trt.BuilderFlag.ENABLE_TACTIC_SOURCES)

# Save timing cache
timing_cache_file = 'timing.cache'
if os.path.exists(timing_cache_file):
    with open(timing_cache_file, 'rb') as f:
        timing_cache = config.create_timing_cache(f.read())
else:
    timing_cache = config.create_timing_cache(b'')

config.set_timing_cache(timing_cache, ignore_mismatch=False)

# Build engine...
engine_bytes = builder.build_serialized_network(net, config)

# Save updated timing cache
with open(timing_cache_file, 'wb') as f:
    f.write(config.get_timing_cache().serialize())
```

**Strongly Typed Mode (TensorRT 10+)**

Gives you explicit control over tensor types instead of letting TensorRT choose:

```python
# Enable strongly typed mode
config.set_flag(trt.BuilderFlag.STRONGLY_TYPED)

# Now you must specify types for inputs and outputs explicitly
# Prevents unexpected precision downgrades in sensitive layers
```

**Dynamic Shapes for Variable Batch Inference**

```python
# Build with dynamic shapes for production flexibility
profile = builder.create_optimization_profile()

profile.set_shape(
    'images',
    min=(1, 3, 640, 640),     # minimum batch size
    opt=(4, 3, 640, 640),     # most common (profile optimized for this)
    max=(16, 3, 640, 640)     # maximum batch size
)
config.add_optimization_profile(profile)

# At inference, set input shape dynamically
context.set_input_shape('images', (batch_size, 3, 640, 640))
```

</details>

### 搭配 TensorRT 使用 CUDA Graphs（低延迟的关键）

没有 CUDA Graphs 时，每次 `execute_async_v2()` 调用因 kernel 启动会产生 ~20–50µs 的 CPU 开销。使用 CUDA Graphs 后，仅约 2µs：

```python
import tensorrt as trt
import pycuda.driver as cuda
import numpy as np

class TRTWithCUDAGraph:
    def __init__(self, engine_path):
        with open(engine_path, 'rb') as f:
            runtime = trt.Runtime(trt.Logger(trt.Logger.WARNING))
            self.engine = runtime.deserialize_cuda_engine(f.read())
        self.context = self.engine.create_execution_context()

        # Allocate buffers
        self._allocate()

        # Capture CUDA Graph
        self._capture_graph()

    def _allocate(self):
        self.stream = cuda.Stream()
        self.inputs, self.outputs, self.bindings = [], [], []
        for i in range(self.engine.num_io_tensors):
            name = self.engine.get_tensor_name(i)
            shape = self.context.get_tensor_shape(name)
            dtype = trt.nptype(self.engine.get_tensor_dtype(name))
            host = cuda.pagelocked_empty(trt.volume(shape), dtype)
            device = cuda.mem_alloc(host.nbytes)
            self.bindings.append(int(device))
            if self.engine.get_tensor_mode(name) == trt.TensorIOMode.INPUT:
                self.inputs.append({'host': host, 'device': device, 'name': name})
            else:
                self.outputs.append({'host': host, 'device': device, 'name': name})

    def _capture_graph(self):
        # Warmup (required before graph capture)
        for _ in range(3):
            self._execute()

        # Capture
        self.graph = cuda.Graph()
        stream_for_capture = cuda.Stream()
        cuda.start_graph_capture(stream_for_capture)
        self._execute(stream=stream_for_capture)
        self.graph_exec = cuda.end_graph_capture_and_instantiate(stream_for_capture)
        print("CUDA Graph captured")

    def _execute(self, stream=None):
        s = stream or self.stream
        for inp in self.inputs:
            cuda.memcpy_htod_async(inp['device'], inp['host'], s)
        self.context.execute_async_v2(self.bindings, s.handle)
        for out in self.outputs:
            cuda.memcpy_dtoh_async(out['host'], out['device'], s)
        s.synchronize()

    def infer(self, data):
        np.copyto(self.inputs[0]['host'], data.ravel())
        # Replay graph — very low overhead
        self.graph_exec.launch(self.stream)
        self.stream.synchronize()
        return self.outputs[0]['host'].copy()
```

---

## 10. Orin Nano 上的 DLA（深度学习加速器）

### 什么是 DLA？

DLA 是集成在 Orin SoC 中的固定功能神经网络加速器。它独立于 GPU 运行。

```
GPU:           1024 CUDA Ampere cores, 40 TOPS (shared with all workloads)
DLA:           Fixed-function INT8/FP16 engine, ~10 TOPS dedicated

Running on DLA:
  - Frees GPU for other tasks (sensor processing, computer vision)
  - Lower power than GPU for supported ops
  - Can run DLA + GPU simultaneously
```

### 哪些 layer 可在 DLA 上运行

```
Supported:      Conv2d, Pooling, BatchNorm, ReLU, Sigmoid
                DepthwiseConv, FullyConnected (limited), Softmax

NOT Supported:  Custom plugins, dynamic shapes, many attention ops
                Operations with large memory footprint

Reality check: Most CNNs (ResNet, MobileNet, YOLO backbone) run well on DLA.
               Transformers do NOT run on DLA (attention is not supported).
```

### 为 DLA 构建 TensorRT Engine

```python
# build_dla_engine.py
import tensorrt as trt

def build_dla_engine(onnx_path, engine_path):
    TRT_LOGGER = trt.Logger(trt.Logger.WARNING)

    with trt.Builder(TRT_LOGGER) as builder, \
         builder.create_network(1 << int(trt.NetworkDefinitionCreationFlag.EXPLICIT_BATCH)) as net, \
         trt.OnnxParser(net, TRT_LOGGER) as parser, \
         builder.create_builder_config() as config:

        config.set_memory_pool_limit(trt.MemoryPoolType.WORKSPACE, 1 << 30)

        # DLA configuration
        config.default_device_type = trt.DeviceType.DLA
        config.DLA_core = 0                          # Orin Nano has 1 DLA (core 0)
        config.set_flag(trt.BuilderFlag.GPU_FALLBACK) # GPU handles unsupported layers
        config.set_flag(trt.BuilderFlag.FP16)         # DLA requires FP16 or INT8

        with open(onnx_path, 'rb') as f:
            parser.parse(f.read())

        engine_bytes = builder.build_serialized_network(net, config)
        with open(engine_path, 'wb') as f:
            f.write(engine_bytes)

# Alternatively with trtexec:
# trtexec --onnx=model.onnx \
#         --saveEngine=model_dla.engine \
#         --useDLACore=0 \
#         --fp16 \
#         --allowGPUFallback \
#         --verbose 2>&1 | grep "DLA"
```

### DLA + GPU 并发执行

```python
import threading

# Run DLA inference and GPU inference on different models simultaneously
# DLA handles the backbone, GPU handles the detection head

dla_engine   = load_engine('backbone_dla.engine')
gpu_engine   = load_engine('detection_head_gpu.engine')

dla_context  = dla_engine.create_execution_context()
gpu_context  = gpu_engine.create_execution_context()

dla_stream   = cuda.Stream()
gpu_stream   = cuda.Stream()

def run_dla(input_data):
    # Backbone on DLA
    cuda.memcpy_htod_async(dla_input_buf, input_data, dla_stream)
    dla_context.execute_async_v2(dla_bindings, dla_stream.handle)
    cuda.memcpy_dtoh_async(features_cpu, dla_output_buf, dla_stream)
    dla_stream.synchronize()
    return features_cpu

def run_gpu(features):
    # Detection head on GPU
    cuda.memcpy_htod_async(gpu_input_buf, features, gpu_stream)
    gpu_context.execute_async_v2(gpu_bindings, gpu_stream.handle)
    cuda.memcpy_dtoh_async(detections_cpu, gpu_output_buf, gpu_stream)
    gpu_stream.synchronize()
    return detections_cpu

# In production: overlap DLA and GPU work
# While DLA processes frame N, GPU processes frame N-1's features
```

### DLA benchmark 对比

```bash
# Compare GPU vs DLA on Orin Nano
# (run while monitoring tegrastats for power)

# GPU only
trtexec --loadEngine=model_fp16_gpu.engine \
        --iterations=100 --avgRuns=100

# DLA only
trtexec --loadEngine=model_dla.engine \
        --iterations=100 --avgRuns=100

# Typical results for MobileNetV2 on Orin Nano:
#   GPU FP16:  8ms, ~300mW GPU power
#   DLA FP16: 12ms, ~100mW DLA power (saves GPU for other tasks)
#   Use DLA when: power matters, GPU needed elsewhere, supported ops only
#   Use GPU when: lowest latency needed, unsupported ops exist
```

---

## 11. Jetson 上的 tinygrad — CUDA 后端

### 在 Jetson 上用 CUDA 运行 tinygrad

tinygrad 原生支持 CUDA。在 Jetson 上，它使用 Ampere 架构 GPU。

```bash
# Verify CUDA is available
python3 -c "from tinygrad.runtime.ops_cuda import CUDADevice; print('CUDA OK')"

# Set tinygrad to use CUDA backend
export CUDA=1    # or GPU=1
```

```python
# tinygrad_cuda_jetson.py
import os
os.environ['CUDA'] = '1'    # use CUDA backend

from tinygrad.tensor import Tensor
import numpy as np
import time

# All tensors default to CUDA device
x = Tensor.randn(64, 784)     # lives on Jetson GPU
W = Tensor.randn(784, 256)

# Operations execute on Ampere GPU
out = x.matmul(W).relu()

# Force computation and bring back to CPU
result = out.numpy()
print(f"Output shape: {result.shape}")

# Benchmark tinygrad CUDA vs CPU on Jetson
def benchmark_backend(backend_env, shape=(64, 784)):
    os.environ.clear()
    if backend_env:
        os.environ['CUDA'] = '1'

    from tinygrad.tensor import Tensor
    import importlib
    import tinygrad.tensor
    importlib.reload(tinygrad.tensor)

    x = Tensor.randn(*shape)
    W = Tensor.randn(shape[1], 256)

    # Warmup
    for _ in range(10):
        (x.matmul(W).relu()).numpy()

    t0 = time.perf_counter()
    for _ in range(100):
        (x.matmul(W).relu()).numpy()
    elapsed = (time.perf_counter() - t0) * 1000 / 100

    return elapsed

# Note: set backend before importing tinygrad, not mid-session
```

### 在 Jetson GPU 上用 tinygrad 训练卷积神经网络

```python
# cnn_jetson.py
import os
os.environ['CUDA'] = '1'

from tinygrad.tensor import Tensor
from tinygrad.nn import Conv2d, BatchNorm2d
from tinygrad.nn.optim import Adam
import numpy as np

class ConvBlock:
    def __init__(self, in_ch, out_ch, stride=1):
        self.conv = Conv2d(in_ch, out_ch, 3, padding=1, stride=stride, bias=False)
        self.bn   = BatchNorm2d(out_ch)

    def __call__(self, x):
        return self.bn(self.conv(x)).relu()

    def parameters(self):
        return self.conv.weight, *self.bn.weight, *self.bn.bias

class TinyResNet:
    """Lightweight ResNet-style CNN for CIFAR-10"""
    def __init__(self, num_classes=10):
        self.c1 = ConvBlock(3, 32)
        self.c2 = ConvBlock(32, 64, stride=2)
        self.c3 = ConvBlock(64, 128, stride=2)
        self.c4 = ConvBlock(128, 256, stride=2)
        # After 3 stride-2 convolutions: 32×32 → 4×4
        self.fc = lambda x: x.reshape(x.shape[0], -1).linear(
            Tensor.kaiming_uniform(256 * 4 * 4, num_classes)
        )

    def __call__(self, x):
        x = self.c1(x)
        x = self.c2(x)
        x = self.c3(x)
        x = self.c4(x)
        x = x.mean(axis=(2, 3))    # global average pooling
        return x.softmax()

model = TinyResNet()
optimizer = Adam([p for block in [model.c1, model.c2, model.c3, model.c4]
                  for p in block.parameters()], lr=1e-3)

# Monitor Jetson GPU during training
# In another terminal: sudo tegrastats --interval 100
# You should see GR3D_FREQ increase to 60-100% during training
```

### 将 tinygrad 模型导出为 ONNX 以供 TensorRT 使用

生产推理场景下，在 tinygrad 中训练，导出为 ONNX，再转换为 TensorRT：

```python
# The general approach: save weights, load into PyTorch, export ONNX
# (same as Section 8 above)

# Alternatively: use tinygrad's built-in ONNX support
# tinygrad can run ONNX models natively:

from tinygrad.runtime.ops_cuda import CUDADevice
import tinygrad.frontend.onnx as onnx_runner
import onnx

model_onnx = onnx.load('model.onnx')
run_onnx = onnx_runner.get_run_onnx(model_onnx)

# Run inference with tinygrad CUDA backend
x = Tensor(np.random.randn(1, 3, 224, 224).astype(np.float32))
output = run_onnx({'input': x})
print(output['output'].numpy())
```

---

## 12. Jetson 上的性能剖析与 benchmark

### tegrastats：必备工具

```bash
# Always run tegrastats while developing — know your baseline
sudo tegrastats --interval 100

# Output fields:
# RAM 3045/7772MB          : used / total unified memory
# CPU [35%@1510, ...]      : % utilization @ MHz per core
# EMC_FREQ 38%             : External Memory Controller bandwidth utilization
# GR3D_FREQ 89%            : GPU utilization %
# CPU@42C GPU@44C tj@44C   : temperatures
# VDD_IN 6234mW            : total system power draw
# VDD_CPU_GPU_CV 2901mW    : CPU+GPU+CV power
# VDD_SOC 1158mW           : SoC power

# Log to file for analysis
sudo tegrastats --interval 100 | tee run_$(date +%s).log &
TEGRA_PID=$!

# Run your workload
python3 my_inference.py

# Stop logging
kill $TEGRA_PID

# Analyze: plot GPU%, temperature, power over time
python3 analyze_tegrastats.py run_*.log
```

```python
# analyze_tegrastats.py
import re
import matplotlib.pyplot as plt

def parse_tegrastats(log_file):
    gpu_util, gpu_temp, power = [], [], []

    with open(log_file) as f:
        for line in f:
            m_gpu  = re.search(r'GR3D_FREQ (\d+)%', line)
            m_temp = re.search(r'GPU@(\d+\.?\d*)C', line)
            m_pwr  = re.search(r'VDD_IN (\d+)mW', line)

            if m_gpu:  gpu_util.append(int(m_gpu.group(1)))
            if m_temp: gpu_temp.append(float(m_temp.group(1)))
            if m_pwr:  power.append(int(m_pwr.group(1)) / 1000)  # W

    return gpu_util, gpu_temp, power

gpu_util, gpu_temp, power = parse_tegrastats('run_log.log')

fig, axes = plt.subplots(3, 1, figsize=(12, 8), sharex=True)
axes[0].plot(gpu_util);  axes[0].set_ylabel('GPU Util %')
axes[1].plot(gpu_temp);  axes[1].set_ylabel('GPU Temp °C')
axes[2].plot(power);     axes[2].set_ylabel('Total Power W')
plt.savefig('profile.png')
```

### trtexec：TensorRT benchmark

```bash
# Full benchmark report
trtexec --loadEngine=model_fp16.engine \
        --warmUp=500 \
        --iterations=1000 \
        --avgRuns=100 \
        --percentile=99 \
        --separateProfileRun 2>&1 | tail -30

# Output:
# [I] Latency: min = 11.2ms, max = 13.1ms, mean = 11.8ms
# [I] GPU Compute Time: min = 10.8ms, max = 12.5ms, mean = 11.3ms
# [I] H2D Latency: min = 0.15ms, max = 0.22ms
# [I] D2H Latency: min = 0.08ms, max = 0.12ms
# [I] Throughput: 84.7 qps

# Profile per-layer timing
trtexec --loadEngine=model_fp16.engine \
        --dumpProfile \
        --iterations=100 2>&1 | grep "Layer Time" | sort -t= -k2 -rn | head -20
```

### Nsight Systems：系统级性能剖析

```bash
# Install on Jetson
sudo apt-get install nsight-systems

# Profile your inference script
nsys profile \
    --trace=cuda,cudnn,tensorrt \
    --output=inference_profile \
    python3 inference.py

# View on Jetson display or copy to desktop
nsys-ui inference_profile.qdrep

# CLI report (no GUI needed)
nsys stats inference_profile.qdrep
```

### 内存性能剖析（8GB 统一内存上的关键环节）

```bash
# Check GPU memory usage during inference
nvidia-smi -l 1    # if available on Orin
# or:
cat /sys/kernel/debug/nvmap/clients     # Jetson-specific

# In Python: track allocation
from tinygrad.tensor import Tensor

def get_gpu_mem_mb():
    """Read current GPU memory from Jetson sysfs"""
    try:
        with open('/sys/devices/gpu.0/mem_info_vram_used') as f:
            return int(f.read()) / (1024 * 1024)
    except:
        return None

before = get_gpu_mem_mb()
engine = load_trt_engine('model.engine')
after  = get_gpu_mem_mb()
print(f"Engine memory: {after - before:.1f} MB")
```

### 能效指标：TOPS/W

```python
# Compute TOPS/W for your model on Jetson

# Step 1: Count operations (MACs) in your model
# For a linear layer [n_in, n_out]: n_in * n_out MACs
# For a conv layer: out_h * out_w * k_h * k_w * in_ch * out_ch MACs

def count_mlp_macs(layers):
    total = 0
    for i in range(len(layers)-1):
        total += layers[i] * layers[i+1]
    return total

# MLP [784→256→128→10]
macs = count_mlp_macs([784, 256, 128, 10])
tops_per_infer = macs * 2 / 1e12    # × 2: multiply + add

# Step 2: Measure FPS and power from tegrastats
fps = 135                  # from benchmark
power_w = 6.2              # from tegrastats VDD_IN

# TOPS/W
tops_total = tops_per_infer * fps
efficiency = tops_total / power_w
print(f"Efficiency: {efficiency:.4f} TOPS/W")
```

---

## 13. 用于视频流水线的 DeepStream

DeepStream 是 NVIDIA 基于 GStreamer 的框架，用于构建经过优化的多流视频 AI 流水线。它专为 Jetson 设计。

### 何时该用 DeepStream 而非裸 TensorRT

```
Use DeepStream when:
  ✓ Processing live video streams (RTSP, CSI camera, USB camera)
  ✓ Multiple concurrent streams (4× cameras)
  ✓ Need tracking, re-identification, analytics
  ✓ Building production video AI systems

Use raw TensorRT when:
  ✓ Single-frame inference (non-video)
  ✓ Custom pipeline logic
  ✓ Robotic sensor fusion (LiDAR + camera)
  ✓ Need maximum control
```

### 用于目标检测的 DeepStream 流水线

```python
# deepstream_detect.py
import gi
gi.require_version('Gst', '1.0')
from gi.repository import Gst, GLib
import pyds   # DeepStream Python bindings

Gst.init(None)

pipeline = Gst.parse_launch("""
    nvarguscamerasrc sensor-id=0 !
    video/x-raw(memory:NVMM),width=1280,height=720,framerate=30/1 !
    nvvideoconvert !
    video/x-raw(memory:NVMM),format=NV12 !
    m.sink_0 nvstreammux name=m batch-size=1 width=1280 height=720 !
    nvinfer config-file-path=yolo_config.txt !
    nvtracker tracker-width=640 tracker-height=360
              ll-lib-file=/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so !
    nvdsosd !
    nvvideoconvert !
    video/x-raw,format=RGBA !
    nveglglessink
""")

def on_detection(pad, info):
    """Process detections from nvinfer"""
    gst_buffer = info.get_buffer()
    batch_meta = pyds.gst_buffer_get_nvds_batch_meta(hash(gst_buffer))

    for frame_meta in pyds.NvDsFrameMetaList(batch_meta.frame_meta_list):
        for obj_meta in pyds.NvDsObjectMetaList(frame_meta.obj_meta_list):
            box = obj_meta.rect_params
            label = obj_meta.obj_label
            conf = obj_meta.confidence
            print(f"  {label}: conf={conf:.2f} box=[{box.left:.0f},{box.top:.0f},{box.width:.0f},{box.height:.0f}]")

pipeline.set_state(Gst.State.PLAYING)
GLib.MainLoop().run()
```

### DeepStream nvinfer 配置（yolo_config.txt）

```ini
[property]
gpu-id=0
net-scale-factor=0.00392156     ; 1/255.0
model-engine-file=yolo_fp16.engine
batch-size=1
network-mode=2                  ; 0=FP32, 1=INT8, 2=FP16
num-detected-classes=80
gie-unique-id=1
output-blob-names=output

[class-attrs-all]
threshold=0.4
```

---

## 14. 项目

### 项目 1：在 tinygrad 中量化 MNIST MLP
用纯 tinygrad 从零实现对称 INT8 量化。比较 per-tensor 与 per-channel 准确率。绘制 4-bit 到 16-bit 的准确率-压缩率曲线。

**目标：** 确切理解传入 `--int8` 时 TensorRT 内部到底做了什么。

### 项目 2：完整移植流水线
在 Jetson GPU 上用 tinygrad 训练 CIFAR-10 上的 CNN。导出 → ONNX → TensorRT INT8。验证每个阶段的准确率。从 tinygrad FP32 到 TensorRT INT8 的准确率下降必须小于 1%。

### 项目 3：结构化剪枝 + 蒸馏
- 从 ResNet-18 开始（11M 参数，CIFAR-10 准确率 82%）
- 剪掉 50% 的通道 → 3M 参数
- 从剪枝前的 teacher 蒸馏 → 恢复到 80%+ 准确率
- 测量 Orin Nano 上剪枝前后的 FPS 提升

### 项目 4：DLA 与 GPU benchmark 对比
取一个 MobileNetV2 模型。构建三个 engine：GPU FP16、GPU INT8、DLA FP16。对每一种测量：
- 推理延迟（trtexec）
- 功耗（tegrastats VDD_IN）
- 计算 TOPS/W 效率
确定最佳工作点。

### 项目 5：CUDA Graph 推理节点
把 TensorRT 推理封装进 CUDA Graph。用 Nsight Systems 测量有图和没图时的启动开销。集成进 ROS2 节点并展示延迟改善。

### 项目 6：混合精度搜索
有些 layer 对 INT8 敏感。编写一个脚本：
1. 从全 INT8 开始
2. 逐步把 layer 提升为 FP16（一次一个）
3. 每次提升后重新测量准确率
4. 达到准确率目标时停止

这是 NVIDIA 的 AMO（Automatic Mixed Precision Optimizer）这类工具所做事情的简化版本。


<details>
<summary>English original</summary>

**Power Efficiency Metric: TOPS/W**

```python
# Compute TOPS/W for your model on Jetson

# Step 1: Count operations (MACs) in your model
# For a linear layer [n_in, n_out]: n_in * n_out MACs
# For a conv layer: out_h * out_w * k_h * k_w * in_ch * out_ch MACs

def count_mlp_macs(layers):
    total = 0
    for i in range(len(layers)-1):
        total += layers[i] * layers[i+1]
    return total

# MLP [784→256→128→10]
macs = count_mlp_macs([784, 256, 128, 10])
tops_per_infer = macs * 2 / 1e12    # × 2: multiply + add

# Step 2: Measure FPS and power from tegrastats
fps = 135                  # from benchmark
power_w = 6.2              # from tegrastats VDD_IN

# TOPS/W
tops_total = tops_per_infer * fps
efficiency = tops_total / power_w
print(f"Efficiency: {efficiency:.4f} TOPS/W")
```

---

**13. DeepStream for Video Pipelines**

DeepStream is NVIDIA's GStreamer-based framework for building optimized multi-stream video AI pipelines. It is specifically designed for Jetson.

**When to Use DeepStream vs Raw TensorRT**

```
Use DeepStream when:
  ✓ Processing live video streams (RTSP, CSI camera, USB camera)
  ✓ Multiple concurrent streams (4× cameras)
  ✓ Need tracking, re-identification, analytics
  ✓ Building production video AI systems

Use raw TensorRT when:
  ✓ Single-frame inference (non-video)
  ✓ Custom pipeline logic
  ✓ Robotic sensor fusion (LiDAR + camera)
  ✓ Need maximum control
```

**DeepStream Pipeline for Object Detection**

```python
# deepstream_detect.py
import gi
gi.require_version('Gst', '1.0')
from gi.repository import Gst, GLib
import pyds   # DeepStream Python bindings

Gst.init(None)

pipeline = Gst.parse_launch("""
    nvarguscamerasrc sensor-id=0 !
    video/x-raw(memory:NVMM),width=1280,height=720,framerate=30/1 !
    nvvideoconvert !
    video/x-raw(memory:NVMM),format=NV12 !
    m.sink_0 nvstreammux name=m batch-size=1 width=1280 height=720 !
    nvinfer config-file-path=yolo_config.txt !
    nvtracker tracker-width=640 tracker-height=360
              ll-lib-file=/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so !
    nvdsosd !
    nvvideoconvert !
    video/x-raw,format=RGBA !
    nveglglessink
""")

def on_detection(pad, info):
    """Process detections from nvinfer"""
    gst_buffer = info.get_buffer()
    batch_meta = pyds.gst_buffer_get_nvds_batch_meta(hash(gst_buffer))

    for frame_meta in pyds.NvDsFrameMetaList(batch_meta.frame_meta_list):
        for obj_meta in pyds.NvDsObjectMetaList(frame_meta.obj_meta_list):
            box = obj_meta.rect_params
            label = obj_meta.obj_label
            conf = obj_meta.confidence
            print(f"  {label}: conf={conf:.2f} box=[{box.left:.0f},{box.top:.0f},{box.width:.0f},{box.height:.0f}]")

pipeline.set_state(Gst.State.PLAYING)
GLib.MainLoop().run()
```

**DeepStream nvinfer Config (yolo_config.txt)**

```ini
[property]
gpu-id=0
net-scale-factor=0.00392156     ; 1/255.0
model-engine-file=yolo_fp16.engine
batch-size=1
network-mode=2                  ; 0=FP32, 1=INT8, 2=FP16
num-detected-classes=80
gie-unique-id=1
output-blob-names=output

[class-attrs-all]
threshold=0.4
```

---

**14. Projects**

**Project 1: Quantize MNIST MLP in tinygrad**
Implement symmetric INT8 quantization from scratch in pure tinygrad. Compare per-tensor vs per-channel accuracy. Plot the accuracy-compression curve for 4-bit through 16-bit.

**Goal:** understand exactly what TensorRT does internally when you pass `--int8`.

**Project 2: Full Porting Pipeline**
Train a CNN on CIFAR-10 in tinygrad on Jetson GPU. Export → ONNX → TensorRT INT8. Verify accuracy at each stage. Must achieve <1% accuracy drop from tinygrad FP32 to TensorRT INT8.

**Project 3: Structured Pruning + Distillation**
- Start with ResNet-18 (11M params, 82% CIFAR-10 accuracy)
- Prune 50% of channels → 3M params
- Distill from unpruned teacher → recover to 80%+ accuracy
- Measure FPS improvement on Orin Nano before/after

**Project 4: DLA vs GPU Benchmark**
Take a MobileNetV2 model. Build three engines: GPU FP16, GPU INT8, DLA FP16. For each, measure:
- Inference latency (trtexec)
- Power draw (tegrastats VDD_IN)
- Compute TOPS/W efficiency
Determine the best operating point.

**Project 5: CUDA Graph Inference Node**
Wrap TensorRT inference in a CUDA Graph. Measure launch overhead with and without graphs using Nsight Systems. Integrate into a ROS2 node and show latency improvement.

**Project 6: Mixed Precision Search**
Some layers are sensitive to INT8. Write a script that:
1. Starts with full INT8
2. Progressively promotes layers to FP16 (one at a time)
3. Re-measures accuracy after each promotion
4. Stops when accuracy target is met

This is a simplified version of what tools like NVIDIA's AMO (Automatic Mixed Precision Optimizer) do.

</details>

### 项目 7：Jetson 上的小目标检测（实时 CV 后端）

**背景：** 改进 Jetson Orin Nano 上的现有 MVP：一个实时视觉后端（DeepStream + GStreamer + FastAPI），执行视频接入、目标检测、可选的二级分类、跟踪、流传输和录制。目标在帧内非常小（仅几个像素）；系统需要在资源受限的嵌入式环境中提高检测可靠性并降低误报。

**完整项目指南：** [small-object-detection-jetson/](04-Jetson小目标检测/Guide.md) — 数据集（来自 GitHub 的 VisDrone2019-DET）、标注格式和 YOLO 转换、最佳方法（YOLOv8 + 多尺度、SPD 等）、训练流水线、TensorRT 导出和 DeepStream 集成。

**要求（技术）：**
- **平台：** NVIDIA Jetson Orin Nano；技术栈：DeepStream、GStreamer、FastAPI。
- **检测：** 改进主目标检测和可选的二级分类；减少误报；处理非常小的目标（低像素数）。
- **约束：** 实时流水线、有限的内存和算力；代码质量、优化和功能完整性。

**解决方案（技术）：**
- **小目标处理：** 多尺度检测（例如 FPN 或多尺度输入）、特征金字塔网络、针对小框的 anchor 调优，以及在延迟预算允许时使用更高分辨率推理。调整置信度和 NMS 阈值，以在控制误报的同时偏向小目标的召回率。
- **推理：** 优化 TensorRT 引擎（FP16/INT8、校准、layer 布局）；如果吞吐允许，为主检测器考虑 DLA（深度学习加速器）。使用 `tegrastats` 和 Nsight Systems 进行性能分析，以保持在实时预算内。
- **数据有限或稀缺：** 从预训练检测器进行迁移学习；在领域数据上微调。使用数据增强（尺度变化、运动模糊、光照/对比度、背景多样性），并在需要时使用合成或采集的数据来提高鲁棒性。
- **稳定性：** 对于小、快速移动或模糊的目标，跨帧稳定检测（跟踪关联、时间平滑或轻量级 re-ID），使后端向 API 交付一致的结果。
- **交付物：** 清理/重构后的流水线、改进的检测和分类指标、减少的误报，以及模型选择、分辨率与速度取舍和 TensorRT 设置的文档。

**目标：** 在 Jetson 上交付一个生产就绪的实时检测/分类后端，能够可靠地找到小目标，并以可接受的延迟和准确率支持二级分类。

### 项目 8：边缘端非接触式多传感器监测（RGB/Depth + 热成像 + 微信号）

**背景：** 一种非接触式监测设备，使用**双摄像头设置**（RGB/Depth + 热成像）和可选音频远程观察对象。目标是以亚像素精度**在摄像头之间映射 ROI**，并从热数据中提取**微波动信号**（0.8–3 Hz）——例如生理线索——同时在**边缘端**（例如 Raspberry Pi）运行完整的流水线，并可选支持 **IoT**（低功耗蓝牙（BLE）、MQTT）。

**完整项目指南：** [non-contact-monitoring-edge/](03-非接触监测边缘/Guide.md) — 设备/传感器、多摄像头融合、热微波动（EVM、带通、SNR）、边缘约束、阶段 1（离线）和阶段 2（实时 + IoT）工作流。

**要求（技术）：**
- **传感器：** RGB/Depth 摄像头（彩色 + 深度）、热成像摄像头；不同的 FOV、分辨率、对齐。可选音频用于同步。
- **融合：** 外参和内参校准；以亚像素精度从 RGB/Depth 到热成像的 ROI 映射。
- **DSP：** 提取 0.8–3 Hz 热波动（EVM 或类似方法、带通滤波器）；验证 SNR。
- **边缘：** 所有处理均在本地；实时；高效的 NumPy/SciPy（或同等库）；无云卸载。
- **阶段 2：** 实时硬件、同步音频、BLE 配网、MQTT 流传输。

**解决方案（技术）：**
- **校准：** 计算两个摄像头的内参和外参；实现从 RGB/Depth 到热成像的 ROI 投影/变换。
- **微信号：** EVM（或类似的时间放大）、0.8–3 Hz 带通、FFT/PSD 用于验证；针对 SNR 和边缘 runtime 优化。
- **部署：** 为目标设备优化流水线；添加 BLE 和 MQTT 用于配网和实时数据流传输。

**目标：** 演示从对齐的 ROI 中可靠提取 0.8–3 Hz 频段的热微波动，首先在预录制数据上（阶段 1），然后在带有可选 IoT 的实时边缘设备上（阶段 2）。


<details>
<summary>English original</summary>

**Project 7: Small Object Detection on Jetson (Real-Time CV Backend)**

**Context:** Improve an existing MVP on Jetson Orin Nano: a real-time vision backend (DeepStream + GStreamer + FastAPI) that does video ingest, object detection, optional secondary classification, tracking, streaming, and recording. Targets are very small in-frame (few pixels); the system needs better detection reliability and lower false positives in a constrained embedded environment.

**Full project guide:** [small-object-detection-jetson/](04-Jetson小目标检测/Guide.md) — dataset (VisDrone2019-DET from GitHub), annotation format and YOLO conversion, best methods (YOLOv8 + multi-scale, SPD, etc.), training pipeline, TensorRT export, and DeepStream integration.

**Requirements (technical):**
- **Platform:** NVIDIA Jetson Orin Nano; stack: DeepStream, GStreamer, FastAPI.
- **Detection:** Improve primary object detection and optional secondary classification; reduce false positives; handle very small targets (low pixel count).
- **Constraints:** Real-time pipeline, limited memory and compute; code quality, optimization, and feature completion.

**Solution (technical):**
- **Small-object handling:** Multi-scale detection (e.g. FPN or multi-scale inputs), feature pyramid networks, anchor tuning for small boxes, and higher-resolution inference where latency budget allows. Tune confidence and NMS thresholds to favor recall on small targets while controlling false positives.
- **Inference:** Optimize TensorRT engines (FP16/INT8, calibration, layer placement); consider DLA for primary detector if throughput allows. Profile with `tegrastats` and Nsight Systems to stay within real-time budget.
- **Limited or scarce data:** Transfer learning from pretrained detectors; fine-tune on domain data. Use augmentation (scale variation, motion blur, lighting/contrast, background diversity) and, if needed, synthetic or collected data to improve robustness.
- **Stability:** For small, fast-moving or ambiguous targets, stabilize detections across frames (tracking association, temporal smoothing, or lightweight re-ID) so the backend delivers consistent results to the API.
- **Deliverables:** Cleaned/refactored pipeline, improved detection and classification metrics, reduced false positives, and documentation of model choices, resolution vs. speed tradeoffs, and TensorRT settings.

**Goal:** Ship a production-ready real-time detection/classification backend on Jetson that reliably finds small objects and supports secondary classification with acceptable latency and accuracy.

**Project 8: Non-Contact Multi-Sensor Monitoring on Edge (RGB/Depth + Thermal + Micro-Signals)**

**Context:** A non-contact monitoring device that observes a subject remotely using a **dual-camera setup** (RGB/Depth + thermal) and optional audio. The goal is to **map ROIs between cameras** with sub-pixel accuracy and extract **micro-fluctuation signals** (0.8–3 Hz) from thermal data—e.g. physiological cues—while running the full pipeline **on the edge** (e.g. Raspberry Pi) with optional **IoT** (BLE, MQTT).

**Full project guide:** [non-contact-monitoring-edge/](03-非接触监测边缘/Guide.md) — device/sensors, multi-camera fusion, thermal micro-fluctuation (EVM, bandpass, SNR), edge constraints, Phase 1 (offline) and Phase 2 (live + IoT) workflow.

**Requirements (technical):**
- **Sensors:** RGB/Depth camera (color + depth), thermal camera; different FOVs, resolutions, alignments. Optional audio for sync.
- **Fusion:** Extrinsic and intrinsic calibration; ROI mapping from RGB/Depth to thermal with sub-pixel accuracy.
- **DSP:** Extract 0.8–3 Hz thermal fluctuations (EVM or similar, bandpass filter); validate SNR.
- **Edge:** All processing local; real-time; efficient NumPy/SciPy (or equivalent); no cloud offload.
- **Phase 2:** Live hardware, synchronized audio, BLE provisioning, MQTT streaming.

**Solution (technical):**
- **Calibration:** Compute intrinsics and extrinsics for both cameras; implement ROI projection/warping from RGB/Depth to thermal.
- **Micro-signals:** EVM (or analogous temporal magnification), 0.8–3 Hz bandpass, FFT/PSD for validation; optimize for SNR and edge runtime.
- **Deployment:** Optimize pipelines for target device; add BLE and MQTT for provisioning and real-time data streaming.

**Goal:** Demonstrate reliable extraction of thermal micro-fluctuations in the 0.8–3 Hz band from aligned ROIs, first on pre-recorded data (Phase 1), then on a live edge device with optional IoT (Phase 2).

</details>

### Project 9：从 NVIDIA CUDA 到 AMD GPU 生态系统的 HPC（高性能计算）移植

**上下文：** 已经有基于 CUDA 的计算与 AI 工作负载（训练或推理）跑在 NVIDIA GPU 上，另外还有假定单一厂商 GPU 栈的桌面可视化/UX 代码。目标是把这些工作负载**移植到 AMD GPU**（RDNA 或 CDNA）上且**性能损失最小**，同时把代码现代化，以支持**多后端（NVIDIA + AMD）桌面应用**，其中包含**光线追踪**和 AI kernel（如 tinygrad）。

**需求（技术）：**
- **计算移植（HPC）：** 至少取一个真实的 CUDA HPC 工作负载（如 stencil/CFD、Monte Carlo、线性代数或图分析），并：
  - 把它移植到 **HIP/ROCm**（如需要，可用 SYCL/oneAPI 作为第二个变体）。
  - 在 **NVIDIA + AMD** GPU 上验证数值等价性与性能。
- **桌面多后端 + 光线追踪：**
  - 实现一个小型**桌面应用**（Windows/Linux），能渲染简单 3D 场景并运行一个 compute kernel，带**两个渲染后端**：
    - NVIDIA 路径：如 DirectX 12 / Vulkan + RTX（DXR/VK_KHR_ray_tracing_pipeline）。
    - AMD 路径：RDNA GPU 上的 Vulkan 光线追踪或 DXR。
  - 把 GPU 资源（缓冲区、描述符、加速结构）抽象到内部 API 之后，使**同一个应用二进制**能面向两家厂商。
- **AI 移植（CUDA → AMD，tinygrad）：**
  - 至少把一个基于 CUDA 的 AI kernel / 模型流水线移植到 AMD 上运行：
    - 方案 A：在 AMD 上用带 ROCm/OpenCL 后端的 **tinygrad**，在 NVIDIA 上用 CUDA 后端。
    - 方案 B：把自定义 CUDA kernel 移植到 **HIP**，并与 ROCm 集成。
  - 确保跨 GPU 厂商的输出在数值容差内一致，并比较吞吐/延迟。

**解决方案（技术）：**
- **1. 把 CUDA HPC kernel 移植到 AMD：**
  - 从现有的 CUDA kernel（矩阵乘、Poisson 求解器、N-body 等）出发。
  - 用 `hipify-clang` 或手工移植把 CUDA 转成 **HIP**，再用 ROCm 为 AMD GPU 构建。
  - 加一个**统一的 benchmark harness**（agent 运行时框架），它：
    - 检测 GPU 厂商。
    - 构建并启动对应的二进制（CUDA vs HIP）。
    - 测量各平台上的 runtime、内存带宽和 FLOPs。
- **2. 带光线追踪的多后端桌面应用：**
  - 尽可能选择 **Vulkan** 作为公共 API：
    - 实现一条基本 PBR 路径，用简单路径追踪器或混合式光栅 + 光线追踪阴影/反射。
    - 使用 `VK_KHR_ray_tracing_pipeline` 和 `VK_KHR_acceleration_structure`，使同一套代码路径在 NVIDIA 和 AMD 上都能工作，并配厂商无关的 SPIR-V shader。
  - 另一种做法是在 Windows 上实现 **DX12 + DXR** 作为第二个后端，把同一套引擎抽象（场景图、材质、BLAS/TLAS、ray-gen/miss/hit shader）映射到 DXR。
  - 构建一个**后端接口**：
    - `IGPUDevice`、`IGPUBuffer`、`IAccelerationStructure`、`IRayTracingPipeline` 接口。
    - 实现 `VulkanDevice` 和 `DX12Device` 两个变体；在 runtime 按 OS / GPU 选择。
- **3. 用 tinygrad 把 AI 移植到 AMD：**
  - 用同一个模型，在 **CUDA**（NVIDIA）和 **ROCm/OpenCL**（AMD）上运行 tinygrad：
    - 通过环境变量确认后端切换（`CUDA=1`、`ROCM=1` 或 `OPENCL=1`，视 tinygrad 版本而定）。
    - 在小 CNN/MLP 上训练或跑推理，并比较性能与数值输出。
  - 对于自定义 CUDA kernel：
    - 用 **HIP**（或通用 OpenCL）重写 kernel，并提供一层薄封装，让 tinygrad（或你的应用）能调用它们。
    - 用单元测试验证正确性，让同一输入分别跑在 CPU 参考实现、NVIDIA CUDA 和 AMD ROCm 上。

**交付物：**
- 至少一个**非平凡 HPC kernel** 的 CUDA + HIP 版本，附一个共享 benchmark 脚本，比较 NVIDIA vs AMD GPU（runtime、带宽和 FLOPs）。
- 一个**桌面 demo 应用**，它：
  - 用**两个 GPU 后端**（如 NVIDIA/AMD 上的纯 Vulkan，或 Vulkan + DX12）渲染一个最小的光线追踪场景。
  - 运行一个小型 compute shader 或 AI kernel，并在 UI 中显示时序 / 性能。
- 一条**基于 tinygrad 的 AI 流水线**（或 HIP 移植的 AI kernel），能在 NVIDIA 和 AMD 上运行，并包含：
  - 跨后端比较数值准确率的脚本。
  - 在代表性模型（如小 CNN、Transformer block 或 MLP）上的吞吐/延迟 benchmark。

**目标：** 建立**厂商可移植的 GPU 技能**：能够拿到一个以 CUDA 为中心的现有代码库（HPC + AI + 可视化），用 ROCm/hip/Vulkan 系统性地把它移植到 AMD GPU，同时保持性能、正确性和统一的应用 UX。

---

## 15. 资源

### tinygrad
- **tinygrad 源码** —— `tinygrad/tensor.py`：算子如何变成 CUDA kernel
- **tinygrad 示例**：`examples/mnist.py`、`examples/efficientnet.py`
- **tinygrad ONNX 前端**：`tinygrad/frontend/onnx.py`


<details>
<summary>English original</summary>

**Project 9: HPC Porting from NVIDIA CUDA to AMD GPU Ecosystem**

**Context:** You already have CUDA-based compute and AI workloads (training or inference) running on NVIDIA GPUs, plus desktop visualization/UX code that assumes a single-vendor GPU stack. The goal is to **port those workloads to AMD GPUs** (RDNA or CDNA) with **minimal performance loss**, while modernizing the code to support **multi-backend (NVIDIA + AMD) desktop apps** including **ray tracing** and AI kernels (e.g., tinygrad).

**Requirements (technical):**
- **Compute port (HPC):** Take at least one real CUDA HPC workload (e.g., stencil/CFD, Monte Carlo, linear algebra, or graph analytics) and:
  - Port it to **HIP/ROCm** (or SYCL/oneAPI as a second variant if desired).
  - Verify numerical equivalence and performance on both **NVIDIA + AMD** GPUs.
- **Desktop multi-backend + ray tracing:**
  - Implement a small **desktop app** (Windows/Linux) that can render a simple 3D scene and run a compute kernel, with **two rendering backends**:
    - NVIDIA path: e.g., DirectX 12 / Vulkan + RTX (DXR/VK_KHR_ray_tracing_pipeline).
    - AMD path: Vulkan ray tracing or DXR on RDNA GPUs.
  - Abstract GPU resources (buffers, descriptors, acceleration structures) behind an internal API so the **same app binary** can target both vendors.
- **AI port (CUDA → AMD, tinygrad):**
  - Port at least one CUDA-based AI kernel / model pipeline to run on AMD:
    - Option A: Use **tinygrad** with ROCm/OpenCL backend on AMD and CUDA backend on NVIDIA.
    - Option B: Port custom CUDA kernels to **HIP** and integrate with ROCm.
  - Ensure identical outputs within numerical tolerance across GPU vendors, and compare throughput/latency.

**Solution (technical):**
- **1. Port CUDA HPC kernel to AMD:**
  - Start from an existing CUDA kernel (matrix multiply, Poisson solver, N-body, etc.).
  - Use `hipify-clang` or manual porting to convert CUDA to **HIP**, then build with ROCm for AMD GPUs.
  - Add a **unified benchmarking harness** that:
    - Detects GPU vendor.
    - Builds and launches the right binary (CUDA vs HIP).
    - Measures runtime, memory bandwidth, and FLOPs on each platform.
- **2. Multi-backend desktop app with ray tracing:**
  - Choose **Vulkan** as the common API where possible:
    - Implement a basic PBR path with a simple path tracer or hybrid raster + ray traced shadows/reflections.
    - Use `VK_KHR_ray_tracing_pipeline` and `VK_KHR_acceleration_structure` so the same code path works on NVIDIA and AMD, with vendor-agnostic SPIR-V shaders.
  - Alternatively, implement **DX12 + DXR** on Windows as a second backend, mapping the same engine abstractions (scene graph, materials, BLAS/TLAS, ray-gen/miss/hit shaders) to DXR.
  - Build a **backend interface**:
    - `IGPUDevice`, `IGPUBuffer`, `IAccelerationStructure`, `IRayTracingPipeline` interfaces.
    - Implement `VulkanDevice` and `DX12Device` variants; select at runtime based on OS / GPU.
- **3. AI port to AMD with tinygrad:**
  - Run tinygrad on **CUDA** (NVIDIA) and **ROCm/OpenCL** (AMD) with the same model:
    - Confirm backend switch via environment variables (`CUDA=1`, `ROCM=1` or `OPENCL=1`, depending on tinygrad version).
    - Train or run inference on a small CNN/MLP and compare performance and numerical outputs.
  - For custom CUDA kernels:
    - Rewrite kernels using **HIP** (or generic OpenCL) and provide a thin wrapper so tinygrad (or your app) can call into them.
    - Validate correctness with unit tests that run the same input on CPU reference, NVIDIA CUDA, and AMD ROCm.

**Deliverables:**
- CUDA + HIP versions of at least one **non-trivial HPC kernel** with a shared benchmark script comparing NVIDIA vs AMD GPUs (runtime, bandwidth, and FLOPs).
- A **desktop demo app** that:
  - Renders a minimal ray-traced scene with **two GPU backends** (e.g., Vulkan-only on NVIDIA/AMD, or Vulkan + DX12).
  - Runs a small compute shader or AI kernel and displays timing / performance in the UI.
- A **tinygrad-based AI pipeline** (or HIP-ported AI kernels) that runs on both NVIDIA and AMD, with:
  - Scripts to compare numerical accuracy across backends.
  - Benchmarks of throughput/latency on representative models (e.g., small CNN, transformer block, or MLP).

**Goal:** Build **vendor-portable GPU skills**: be able to take an existing CUDA-centric codebase (HPC + AI + visualization) and systematically port it to AMD GPUs with ROCm/hip/Vulkan while preserving performance, correctness, and a unified application UX.

---

**15. Resources**

**tinygrad**
- **tinygrad source** — `tinygrad/tensor.py`: how ops become CUDA kernels
- **tinygrad examples**: `examples/mnist.py`, `examples/efficientnet.py`
- **tinygrad ONNX frontend**: `tinygrad/frontend/onnx.py`

</details>

### 量化理论
- **Nagel et al. "A White Paper on Neural Network Quantization"**（2021，Qualcomm）：训练后量化方法的权威参考
- **Krishnamoorthi "Quantizing Deep Convolutional Networks for Efficient Inference"**（Google）：解释对称与非对称、逐层与逐通道

### TensorRT
- **TensorRT Developer Guide** — docs.nvidia.com/deeplearning/tensorrt/developer-guide/
- **TensorRT OSS** — github.com/NVIDIA/TensorRT：插件示例与 INT8 校准样例
- **TensorRT OSS 中的 trtexec 源码**：精确展示 benchmark 工具的工作方式

### Jetson 专用
- **Jetson Benchmarks** — developer.nvidia.com/embedded/jetson-benchmarks：各模式的官方 TOPS 数据
- **Deep Learning Inference Benchmarking with TensorRT on Jetson** — NVIDIA Jetson AI Lab
- **NVIDIA NGC Containers** — ngc.nvidia.com：为 Jetson 预优化的容器（感知 JetPack、预构建的 TRT engine）

### 论文
- **"EfficientNet: Rethinking Model Scaling"** — 面向高效卷积神经网络的 NAS + 复合缩放
- **"MobileNetV2: Inverted Residuals and Linear Bottlenecks"** — 面向边缘的深度可分离卷积
- **"Distilling the Knowledge in a Neural Network"**（Hinton et al.）— 蒸馏的原始论文
- **"The Lottery Ticket Hypothesis"** — 从训练视角理解剪枝

---

## 深入探讨：子文件夹

### [NVIDIA TAO Toolkit](05-TAO工具包/Guide.md)

手动流水线（tinygrad → ONNX → TensorRT）让你完全掌控。TAO Toolkit 走另一条路：**从 NGC 的预训练模型出发，通过 YAML 配置，无需编写训练代码即可得到生产级 INT8 TensorRT engine**。

- **迁移学习**：用 NGC 预训练模型（ResNet、EfficientNet、YOLO、SSD），只需 500–5,000 张图像而非数百万张
- **一条命令剪枝**：`tao model yolo_v4 prune -prune_ratio 0.6` 移除 60% 的通道
- **内置 QAT**：spec 文件中的 `enable_qat: True` — 自动插入伪量化
- **直接导出 TensorRT**：`tao model yolo_v4 export` → 校准后的 `.engine` 文件
- **DeepStream 即插即用**：生成的模型配合配置文件即可作为 `nvinfer` primary GIE 工作

```
TAO vs Manual tradeoff:
  TAO:    hours to working INT8 engine, limited to NGC architectures
  Manual: days to implement, unlimited architectural freedom
```

在 Jetson 上做标准检测/分类用 TAO。自定义架构（BEVFusion、PointPillars、自定义 Transformer）用手动流水线。

---

*前置要求图：[阶段 3 — 神经网络](../../../阶段3-人工智能/01-神经网络/Guide.md)*
*下一步（子模块）：[5.6 ROS2](../06-ROS-2/Guide.md) · **传感器融合**（阶段 3）：[传感器融合](../../../阶段3-人工智能/03-硬件与边缘AI/02-传感器融合/Guide.md)*


<details>
<summary>English original</summary>

**Quantization Theory**
- **Nagel et al. "A White Paper on Neural Network Quantization"** (2021, Qualcomm): the definitive reference on post-training quantization methods
- **Krishnamoorthi "Quantizing Deep Convolutional Networks for Efficient Inference"** (Google): explains symmetric vs asymmetric, per-layer vs per-channel

**TensorRT**
- **TensorRT Developer Guide** — docs.nvidia.com/deeplearning/tensorrt/developer-guide/
- **TensorRT OSS** — github.com/NVIDIA/TensorRT: plugin examples and INT8 calibration samples
- **trtexec** source in TensorRT OSS: shows exactly how benchmark tool works

**Jetson-Specific**
- **Jetson Benchmarks** — developer.nvidia.com/embedded/jetson-benchmarks: official TOPS numbers per mode
- **Deep Learning Inference Benchmarking with TensorRT on Jetson** — NVIDIA Jetson AI Lab
- **NVIDIA NGC Containers** — ngc.nvidia.com: pre-optimized containers for Jetson (JetPack-aware, pre-built TRT engines)

**Papers**
- **"EfficientNet: Rethinking Model Scaling"** — NAS + compound scaling for efficient CNNs
- **"MobileNetV2: Inverted Residuals and Linear Bottlenecks"** — depthwise separable convolution for edge
- **"Distilling the Knowledge in a Neural Network"** (Hinton et al.) — original distillation paper
- **"The Lottery Ticket Hypothesis"** — understanding pruning from a training perspective

---

**Deep Dive: Subfolders**

**[NVIDIA TAO Toolkit](05-TAO工具包/Guide.md)**

The manual pipeline (tinygrad → ONNX → TensorRT) gives you full control. TAO Toolkit takes the other approach: **start from a pre-trained model from NGC, configure via YAML, and get a production INT8 TensorRT engine without writing training code**.

- **Transfer learning** from NGC pretrained models (ResNet, EfficientNet, YOLO, SSD) with 500–5,000 images instead of millions
- **One-command pruning**: `tao model yolo_v4 prune -prune_ratio 0.6` removes 60% of channels
- **QAT built-in**: `enable_qat: True` in the spec file — fake quantization inserted automatically
- **Direct TensorRT export**: `tao model yolo_v4 export` → calibrated `.engine` file
- **DeepStream drop-in**: generated model works as `nvinfer` primary GIE with a config file

```
TAO vs Manual tradeoff:
  TAO:    hours to working INT8 engine, limited to NGC architectures
  Manual: days to implement, unlimited architectural freedom
```

Use TAO for standard detection/classification on Jetson. Use the manual pipeline for custom architectures (BEVFusion, PointPillars, custom transformers).

---

*Prerequisite map: [Phase 3 — Neural Networks](../../../阶段3-人工智能/01-神经网络/Guide.md)*
*Next (sub-module): [5.6 ROS2](../06-ROS-2/Guide.md) · **Sensor fusion** (Phase 3): [Sensor Fusion](../../../阶段3-人工智能/03-硬件与边缘AI/02-传感器融合/Guide.md)*

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/5. Application Development/5. ML and AI/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/5.%20Application%20Development/5.%20ML%20and%20AI/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
