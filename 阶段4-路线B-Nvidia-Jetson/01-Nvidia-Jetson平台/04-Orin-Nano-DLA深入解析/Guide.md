---
title: Orin Nano 8GB — 深度学习加速器（DLA）深入解析
description: Orin Nano 8GB — 深度学习加速器（DLA）深入解析
---

# Orin Nano 8GB — 深度学习加速器（DLA）深入解析

<div class="course-identity auto-course" style="--course-accent: #be123c; --course-accent-rgb: 190, 18, 60;" markdown="1">
<div class="course-identity__icon">ON8D</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入解析 · Jetson 专题</p>
<p class="course-identity__title">Orin Nano 8GB — 深度学习加速器（DLA）深入解析的专属课程标识。</p>
<p class="course-identity__meta">产物：Jetson 集成演示 · 度量：延迟、内存、功耗、日志</p>
</div>
</div>


> **范围：** 对 Jetson Orin Nano 8GB 上 DLA 的生产级理解 — 硬件架构、内存交互、软件栈、TensorRT 集成、layer 支持、多引擎调度、性能剖析，以及生产部署模式。
>
> **前置要求：** 熟悉 [Orin Nano 内存架构](../07-Orin-Nano内存架构/Guide.md)（CMA、SMMU、零拷贝）与 [kernel 内部机制](../06-Orin-Nano内核内部机制/Guide.md)（驱动模型、模块加载）。

---


## 1. DLA 是什么

DLA（深度学习加速器）是 Tegra SoC 内部一个**专用硬件模块**，专为神经网络推理设计。它不是 GPU，也不是 CPU — 它是一个固定功能加速器，针对低功耗、高效率的 AI 工作负载优化。

关键特性：

* **专为推理打造** — 卷积、池化、激活函数、归一化
* **高能效** — 相比 GPU 可实现更高的 TOPS/watt
* **确定性的延迟** — 不与其他 GPU 工作负载争抢
* **可与 CPU 和 GPU 并行运行** — 真正的异构计算

DLA **不**支持训练，只支持推理。它**不**支持所有神经网络算子 — 不支持的 layer 回退到 GPU。

---

## 2. Orin Nano 8GB 上的 DLA — 规格

| Specification            | Value                                |
|--------------------------|--------------------------------------|
| DLA 引擎数量    | 1                                    |
| 峰值性能（INT8）  | 最高 10 TOPS                        |
| 峰值性能（FP16）  | 最高 5 TFLOPS                       |
| 支持的精度     | INT8、FP16                           |
| 片上 SRAM             | 用于权重/激活值的小容量缓冲 |
| 内存访问    | 经 DMA + SMMU 访问系统 DRAM           |
| 功耗        | 显著低于 GPU         |

注：Orin Nano 8GB 有 **1 个 DLA 引擎**。更高端的 Orin 模块（NX、AGX）有 2 个 DLA 引擎，可同时对两个模型做并行推理。

---

## 3. DLA vs GPU vs CPU — 各自的使用场景

| Feature              | CPU                  | GPU                    | DLA                           |
|----------------------|----------------------|------------------------|-------------------------------|
| 架构         | 通用      | 大规模并行     | 固定功能 AI 加速器 |
| 最佳工作负载        | OS、控制逻辑    | 并行 FP/INT 运算    | 卷积神经网络/RNN 推理             |
| 支持的算子 | 全部           | 全部（CUDA）      | 子集（卷积、池化等）     |
| 功耗    | 计算时高     | 中高            | 低                           |
| 延迟              | 较高               | 中                 | 极低（确定性的）      |
| 精度          | FP32/FP64            | FP32/FP16/INT8/TF32    | 仅 INT8/FP16                |
| 可编程性      | 完整（C/C++/Python）  | 完整（CUDA）            | 仅通过 TensorRT             |
| 与其他单元并行 | 是                  | 是                    | 是                           |

### 决策矩阵

| Scenario                                     | Best Engine |
|----------------------------------------------|-------------|
| 单模型，最大吞吐             | GPU         |
| 单模型，最低功耗                  | DLA         |
| 两个模型同时运行                   | DLA + GPU   |
| 含大量不支持 layer 的模型           | GPU         |
| 电池供电设备                       | DLA         |
| 前/后处理 + 推理              | CPU + DLA   |
| 实时视频 + 推理 + 显示        | GPU + DLA   |

### 功耗论据

在 Orin Nano 8GB 上（总 TDP 15W）：

* 在 GPU 上跑推理：GPU 消耗约 5–8W，留给 CPU 和 I/O 的余量更少
* 在 DLA 上跑推理：DLA 消耗约 1–3W，为 GPU（显示、编码）和 CPU 释放出功耗预算

在功耗受限的系统（电池、太阳能、散热受限的机箱）中，DLA 往往决定了功耗预算是达标还是超限。

---

## 4. DLA 硬件架构


<details>
<summary>English original</summary>

**Orin Nano 8GB — Deep Learning Accelerator (DLA) Deep Dive**

<div class="course-identity auto-course" style="--course-accent: #be123c; --course-accent-rgb: 190, 18, 60;" markdown="1">
<div class="course-identity__icon">ON8D</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Jetson Track</p>
<p class="course-identity__title">Specialized course identity for Orin Nano 8GB — Deep Learning Accelerator (DLA) Deep Dive.</p>
<p class="course-identity__meta">Artifact: Jetson integration demo · Measure: latency, memory, power, logs</p>
</div>
</div>


> **Scope:** Production-level understanding of the DLA on Jetson Orin Nano 8GB — hardware architecture, memory interaction, software stack, TensorRT integration, layer support, multi-engine scheduling, performance profiling, and production deployment patterns.
>
> **Prerequisites:** Familiarity with the [Orin Nano memory architecture](../07-Orin-Nano内存架构/Guide.md) (CMA, SMMU, zero-copy) and [kernel internals](../06-Orin-Nano内核内部机制/Guide.md) (driver model, module loading).

---


**1. What Is DLA**

DLA (Deep Learning Accelerator) is a **dedicated hardware block** inside the Tegra SoC designed specifically for neural network inference. It is not a GPU, not a CPU — it is a fixed-function accelerator optimized for low-power, high-efficiency AI workloads.

Key characteristics:

* **Purpose-built for inference** — convolution, pooling, activation, normalization
* **Power-efficient** — achieves high TOPS/watt compared to GPU
* **Deterministic latency** — no contention with other GPU workloads
* **Runs in parallel** with CPU and GPU — true heterogeneous computing

DLA does **not** support training, only inference. It does **not** support all neural network operations — unsupported layers fall back to GPU.

---

**2. DLA on Orin Nano 8GB — Specifications**

| Specification            | Value                                |
|--------------------------|--------------------------------------|
| Number of DLA engines    | 1                                    |
| Peak performance (INT8)  | Up to 10 TOPS                        |
| Peak performance (FP16)  | Up to 5 TFLOPS                       |
| Supported precisions     | INT8, FP16                           |
| On-chip SRAM             | Small buffer for weights/activations |
| Memory access            | System DRAM via DMA + SMMU           |
| Power consumption        | Significantly lower than GPU         |

Note: Orin Nano 8GB has **1 DLA engine**. Higher-end Orin modules (NX, AGX) have 2 DLA engines, enabling parallel inference on two models simultaneously.

---

**3. DLA vs GPU vs CPU — When to Use Each**

| Feature              | CPU                  | GPU                    | DLA                           |
|----------------------|----------------------|------------------------|-------------------------------|
| Architecture         | General-purpose      | Massively parallel     | Fixed-function AI accelerator |
| Best workload        | OS, control logic    | Parallel FP/INT ops    | CNN/RNN inference             |
| Supported operations | Everything           | Everything (CUDA)      | Subset (conv, pool, etc.)     |
| Power consumption    | High for compute     | Medium-high            | Low                           |
| Latency              | Higher               | Medium                 | Very low (deterministic)      |
| Precision            | FP32/FP64            | FP32/FP16/INT8/TF32    | INT8/FP16 only                |
| Programmability      | Full (C/C++/Python)  | Full (CUDA)            | Via TensorRT only             |
| Parallel with others | Yes                  | Yes                    | Yes                           |

**Decision Matrix**

| Scenario                                     | Best Engine |
|----------------------------------------------|-------------|
| Single model, maximum throughput             | GPU         |
| Single model, minimum power                  | DLA         |
| Two models simultaneously                   | DLA + GPU   |
| Model with many unsupported layers           | GPU         |
| Battery-powered device                       | DLA         |
| Pre/post-processing + inference              | CPU + DLA   |
| Real-time video + inference + display        | GPU + DLA   |

**The Power Argument**

On Orin Nano 8GB (15W TDP total):

* Running inference on GPU: GPU consumes ~5–8W, leaving less for CPU and I/O
* Running inference on DLA: DLA consumes ~1–3W, freeing power budget for GPU (display, encode) and CPU

In power-constrained systems (battery, solar, thermal-limited enclosures), DLA can be the difference between meeting and missing the power budget.

---

**4. DLA Hardware Architecture**

</details>

### 框图

```
┌─────────────────────────────────────────────────┐
│                  DLA Engine                      │
│                                                  │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐       │
│  │ Convolution│  │ SDP      │  │ PDP      │       │
│  │ Core      │  │ (Single  │  │ (Planar  │       │
│  │           │  │  Data    │  │  Data    │       │
│  │ MAC array │  │  Proc.)  │  │  Proc.)  │       │
│  └─────┬─────┘  └─────┬────┘  └─────┬────┘       │
│        │              │              │            │
│  ┌─────┴──────────────┴──────────────┴─────┐     │
│  │           Internal Data Bus              │     │
│  └─────────────────┬────────────────────────┘     │
│                    │                              │
│  ┌─────────────────┴────────────────────────┐     │
│  │          SRAM Buffer (on-chip)            │     │
│  └─────────────────┬────────────────────────┘     │
│                    │                              │
│  ┌─────────────────┴────────────────────────┐     │
│  │          DMA Engine                       │     │
│  │  (reads/writes tensors from/to DRAM)      │     │
│  └─────────────────┬────────────────────────┘     │
│                    │                              │
└────────────────────┼──────────────────────────────┘
                     │
                     ↓
              SMMU → System DRAM
```

### 核心组件

#### 卷积核心

* 包含 MAC（Multiply-Accumulate）阵列
* DLA 的心脏 —— 执行卷积、矩阵乘法、反卷积
* 针对 INT8 和 FP16 数据类型优化
* 支持多种 kernel 尺寸（1x1、3x3、5x5、7x7 等）
* 处理带步长和带空洞的卷积

#### SDP（Single Data Processor）

* 在卷积之后执行逐元素运算
* 处理：偏置相加、批归一化、ReLU、PReLU、sigmoid、tanh
* 在写入内存之前对卷积核心的输出进行处理
* 融合多个卷积后操作，避免内存往返

#### PDP（Planar Data Processor）

* 执行池化操作（最大池化、平均池化）
* 作用于 2D 空间数据
* 支持多种池化尺寸和步长

#### CDP（Channel Data Processor）

* 执行逐通道操作
* 局部响应归一化（LRN）
* 逐通道缩放

#### SRAM 缓冲区

* 小型片上内存，用于暂存权重和激活值
* 降低频繁访问数据的 DRAM 带宽消耗
* DLA 编译器决定哪些数据缓存到 SRAM、哪些从 DRAM 流式读取

#### DMA 引擎

* 将输入张量从 DRAM 搬入 DLA 处理核心
* 将输出张量从 DLA 写回 DRAM
* 使用 IOVA 地址（经 SMMU 映射）
* 支持针对非连续缓冲区的 scatter-gather

---

## 5. DLA 内存交互

DLA **没有**大容量专用内存。所有张量存储都使用系统 DRAM。

### 内存流

```
System DRAM (8GB LPDDR5, shared with CPU/GPU)
      ↑ ↓
    SMMU (ARM SMMU v2)
      ↑ ↓
  IOVA address space (DLA's view of memory)
      ↑ ↓
  DMA Engine (inside DLA)
      ↑ ↓
  SRAM Buffer (small, on-chip)
      ↑ ↓
  Processing Cores (Conv, SDP, PDP, CDP)
```

### 缓冲区分配

DLA 缓冲区由 TensorRT runtime / DLA driver 分配：

1. **输入张量** —— 从 CMA（连续内存）或 carve-out 内存中分配
2. **权重张量** —— 从序列化的 TensorRT engine 文件加载
3. **中间张量** —— 为 layer 间数据流分配
4. **输出张量** —— 从 CMA 分配，返回给调用方

所有缓冲区都经 SMMU 映射，因此 DLA 通过 IOVA 访问它们。

### 与 GPU 的零拷贝

当某个 DLA layer 的输出送入 GPU layer（或反之）时：

```
DLA output buffer (in DRAM)
   ↓
Same physical pages
   ↓
GPU SMMU maps same pages at different IOVA
   ↓
GPU reads data — no copy needed
```

在构建 DLA+GPU 混合 engine 时，TensorRT 会自动处理这一点。

### 内存预算影响

DLA 缓冲区与 GPU、CPU 的分配一样消耗系统 DRAM：

| 组件             | 典型内存         |
|--------------------|------------------|
| 模型权重         | 10–100 MB        |
| 输入张量         | 1–12 MB          |
| 中间张量         | 10–50 MB         |
| 输出张量         | < 1 MB           |
| **每个模型总计** | **20–160 MB**   |

在 8GB 系统上，这一开销相当可观。要在 CPU + GPU + DLA 各类工作负载之间规划内存预算。

---


<details>
<summary>English original</summary>

**Block Diagram**

```
┌─────────────────────────────────────────────────┐
│                  DLA Engine                      │
│                                                  │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐       │
│  │ Convolution│  │ SDP      │  │ PDP      │       │
│  │ Core      │  │ (Single  │  │ (Planar  │       │
│  │           │  │  Data    │  │  Data    │       │
│  │ MAC array │  │  Proc.)  │  │  Proc.)  │       │
│  └─────┬─────┘  └─────┬────┘  └─────┬────┘       │
│        │              │              │            │
│  ┌─────┴──────────────┴──────────────┴─────┐     │
│  │           Internal Data Bus              │     │
│  └─────────────────┬────────────────────────┘     │
│                    │                              │
│  ┌─────────────────┴────────────────────────┐     │
│  │          SRAM Buffer (on-chip)            │     │
│  └─────────────────┬────────────────────────┘     │
│                    │                              │
│  ┌─────────────────┴────────────────────────┐     │
│  │          DMA Engine                       │     │
│  │  (reads/writes tensors from/to DRAM)      │     │
│  └─────────────────┬────────────────────────┘     │
│                    │                              │
└────────────────────┼──────────────────────────────┘
                     │
                     ↓
              SMMU → System DRAM
```

**Core Components**

**Convolution Core**

* Contains the MAC (Multiply-Accumulate) array
* Heart of DLA — performs convolution, matrix multiplication, deconvolution
* Optimized for INT8 and FP16 data types
* Supports various kernel sizes (1x1, 3x3, 5x5, 7x7, etc.)
* Handles strided and dilated convolutions

**SDP (Single Data Processor)**

* Performs element-wise operations after convolution
* Handles: bias addition, batch normalization, ReLU, PReLU, sigmoid, tanh
* Operates on the output of the convolution core before writing to memory
* Fuses multiple post-convolution operations to avoid memory round-trips

**PDP (Planar Data Processor)**

* Performs pooling operations (max pool, average pool)
* Operates on 2D spatial data
* Supports various pool sizes and strides

**CDP (Channel Data Processor)**

* Performs channel-wise operations
* Local Response Normalization (LRN)
* Channel-wise scaling

**SRAM Buffer**

* Small on-chip memory for staging weights and activations
* Reduces DRAM bandwidth consumption for frequently accessed data
* DLA compiler decides what to cache in SRAM vs. stream from DRAM

**DMA Engine**

* Moves input tensors from DRAM into DLA processing cores
* Writes output tensors from DLA back to DRAM
* Uses IOVA addresses (mapped through SMMU)
* Supports scatter-gather for non-contiguous buffers

---

**5. DLA Memory Interaction**

DLA does **not** have large dedicated memory. It uses system DRAM for all tensor storage.

**Memory Flow**

```
System DRAM (8GB LPDDR5, shared with CPU/GPU)
      ↑ ↓
    SMMU (ARM SMMU v2)
      ↑ ↓
  IOVA address space (DLA's view of memory)
      ↑ ↓
  DMA Engine (inside DLA)
      ↑ ↓
  SRAM Buffer (small, on-chip)
      ↑ ↓
  Processing Cores (Conv, SDP, PDP, CDP)
```

**Buffer Allocation**

DLA buffers are allocated by the TensorRT runtime / DLA driver:

1. **Input tensors** — allocated from CMA (contiguous) or carved-out memory
2. **Weight tensors** — loaded from the serialized TensorRT engine file
3. **Intermediate tensors** — allocated for layer-to-layer data flow
4. **Output tensors** — allocated from CMA, returned to the caller

All buffers are mapped through SMMU so DLA accesses them via IOVA.

**Zero-Copy With GPU**

When a DLA layer's output feeds into a GPU layer (or vice versa):

```
DLA output buffer (in DRAM)
   ↓
Same physical pages
   ↓
GPU SMMU maps same pages at different IOVA
   ↓
GPU reads data — no copy needed
```

TensorRT handles this automatically when building a hybrid DLA+GPU engine.

**Memory Budget Impact**

DLA buffers consume system DRAM just like GPU and CPU allocations:

| Component          | Typical Memory   |
|--------------------|------------------|
| Model weights      | 10–100 MB        |
| Input tensor       | 1–12 MB          |
| Intermediate       | 10–50 MB         |
| Output tensor      | < 1 MB           |
| **Total per model** | **20–160 MB**   |

On an 8GB system, this is significant. Plan memory budgets across CPU + GPU + DLA workloads.

---

</details>

## 6. 软件栈 — 从模型到 DLA 执行

```
┌──────────────────────────────────────────┐
│  User Application                         │
│  (Python/C++ — inference request)         │
└──────────────────┬───────────────────────┘
                   ↓
┌──────────────────────────────────────────┐
│  TensorRT Runtime                         │
│  (engine deserialization, execution)      │
│  Selects DLA or GPU per layer             │
└──────────────────┬───────────────────────┘
                   ↓
┌──────────────────────────────────────────┐
│  libnvdla (DLA runtime library)           │
│  Programs DLA registers                   │
│  Manages DMA descriptors                  │
│  Handles synchronization                  │
└──────────────────┬───────────────────────┘
                   ↓
┌──────────────────────────────────────────┐
│  Kernel Driver (nvdla.ko / nvhost)        │
│  Allocates CMA buffers                    │
│  Creates SMMU/IOVA mappings               │
│  Submits work to DLA hardware             │
│  Handles completion interrupts            │
└──────────────────┬───────────────────────┘
                   ↓
┌──────────────────────────────────────────┐
│  DLA Hardware                             │
│  Executes neural network layers           │
│  DMA reads/writes tensors from/to DRAM    │
└──────────────────────────────────────────┘
```

---

## 7. TensorRT DLA 集成

### 构建启用 DLA 的 engine

```python
import tensorrt as trt

logger = trt.Logger(trt.Logger.INFO)
builder = trt.Builder(logger)
network = builder.create_network(1 << int(trt.NetworkDefinitionCreationFlag.EXPLICIT_BATCH))
parser = trt.OnnxParser(network, logger)

# Parse ONNX model
with open("model.onnx", "rb") as f:
    parser.parse(f.read())

config = builder.create_builder_config()
config.max_workspace_size = 1 << 30  # 1 GB

# Enable DLA
config.default_device_type = trt.DeviceType.DLA
config.DLA_core = 0  # Use DLA core 0

# Allow GPU fallback for unsupported layers
config.set_flag(trt.BuilderFlag.GPU_FALLBACK)

# Use FP16 or INT8 (DLA does not support FP32)
config.set_flag(trt.BuilderFlag.FP16)
# Or for INT8:
# config.set_flag(trt.BuilderFlag.INT8)
# config.int8_calibrator = MyCalibrator()

engine = builder.build_engine(network, config)

# Serialize engine
with open("model_dla.engine", "wb") as f:
    f.write(engine.serialize())
```

### 关键的 TensorRT DLA 选项

| 选项                   | 用途                                        |
|--------------------------|------------------------------------------------|
| `default_device_type`    | 默认将所有 layer 设为 DLA               |
| `DLA_core`               | 选择使用哪个 DLA engine（0 或 1） |
| `GPU_FALLBACK`           | 允许不支持的 layer 在 GPU 上运行          |
| `FP16` / `INT8`          | DLA 要求使用降低的精度                  |
| `set_device_type(layer)` | 覆盖逐 layer 的设备分配            |

### 逐 layer 设备分配

若需细粒度控制，可将特定 layer 分配到 DLA 或 GPU：

```python
for i in range(network.num_layers):
    layer = network.get_layer(i)
    if can_run_on_dla(layer):
        config.set_device_type(layer, trt.DeviceType.DLA)
    else:
        config.set_device_type(layer, trt.DeviceType.GPU)
```

### 检查 DLA layer 分配

构建 engine 后，检查哪些 layer 运行在 DLA 上：

```bash
# Use trtexec to build and profile
trtexec --onnx=model.onnx --useDLACore=0 --fp16 --allowGPUFallback --verbose

# Output shows per-layer device assignment:
# Layer: conv1 ... Device: DLA
# Layer: unsupported_op ... Device: GPU (fallback)
```

---

## 8. 支持的 layer 与精度


<details>
<summary>English original</summary>

**6. Software Stack — From Model to DLA Execution**

```
┌──────────────────────────────────────────┐
│  User Application                         │
│  (Python/C++ — inference request)         │
└──────────────────┬───────────────────────┘
                   ↓
┌──────────────────────────────────────────┐
│  TensorRT Runtime                         │
│  (engine deserialization, execution)      │
│  Selects DLA or GPU per layer             │
└──────────────────┬───────────────────────┘
                   ↓
┌──────────────────────────────────────────┐
│  libnvdla (DLA runtime library)           │
│  Programs DLA registers                   │
│  Manages DMA descriptors                  │
│  Handles synchronization                  │
└──────────────────┬───────────────────────┘
                   ↓
┌──────────────────────────────────────────┐
│  Kernel Driver (nvdla.ko / nvhost)        │
│  Allocates CMA buffers                    │
│  Creates SMMU/IOVA mappings               │
│  Submits work to DLA hardware             │
│  Handles completion interrupts            │
└──────────────────┬───────────────────────┘
                   ↓
┌──────────────────────────────────────────┐
│  DLA Hardware                             │
│  Executes neural network layers           │
│  DMA reads/writes tensors from/to DRAM    │
└──────────────────────────────────────────┘
```

---

**7. TensorRT DLA Integration**

**Building a DLA-Enabled Engine**

```python
import tensorrt as trt

logger = trt.Logger(trt.Logger.INFO)
builder = trt.Builder(logger)
network = builder.create_network(1 << int(trt.NetworkDefinitionCreationFlag.EXPLICIT_BATCH))
parser = trt.OnnxParser(network, logger)

# Parse ONNX model
with open("model.onnx", "rb") as f:
    parser.parse(f.read())

config = builder.create_builder_config()
config.max_workspace_size = 1 << 30  # 1 GB

# Enable DLA
config.default_device_type = trt.DeviceType.DLA
config.DLA_core = 0  # Use DLA core 0

# Allow GPU fallback for unsupported layers
config.set_flag(trt.BuilderFlag.GPU_FALLBACK)

# Use FP16 or INT8 (DLA does not support FP32)
config.set_flag(trt.BuilderFlag.FP16)
# Or for INT8:
# config.set_flag(trt.BuilderFlag.INT8)
# config.int8_calibrator = MyCalibrator()

engine = builder.build_engine(network, config)

# Serialize engine
with open("model_dla.engine", "wb") as f:
    f.write(engine.serialize())
```

**Key TensorRT DLA Options**

| Option                   | Purpose                                        |
|--------------------------|------------------------------------------------|
| `default_device_type`    | Set all layers to DLA by default               |
| `DLA_core`               | Select which DLA engine (0 or 1)               |
| `GPU_FALLBACK`           | Allow unsupported layers to run on GPU          |
| `FP16` / `INT8`          | DLA requires reduced precision                  |
| `set_device_type(layer)` | Override per-layer device assignment            |

**Per-Layer Device Assignment**

For fine-grained control, assign specific layers to DLA or GPU:

```python
for i in range(network.num_layers):
    layer = network.get_layer(i)
    if can_run_on_dla(layer):
        config.set_device_type(layer, trt.DeviceType.DLA)
    else:
        config.set_device_type(layer, trt.DeviceType.GPU)
```

**Inspecting DLA Layer Assignment**

After building the engine, check which layers run on DLA:

```bash
# Use trtexec to build and profile
trtexec --onnx=model.onnx --useDLACore=0 --fp16 --allowGPUFallback --verbose

# Output shows per-layer device assignment:
# Layer: conv1 ... Device: DLA
# Layer: unsupported_op ... Device: GPU (fallback)
```

---

**8. Supported Layers and Precision**

</details>

### DLA 支持的操作

| Operation              | Supported | Notes                                  |
|------------------------|-----------|----------------------------------------|
| Convolution (2D)       | Yes       | 所有 kernel 尺寸、步长、dilation    |
| Deconvolution          | Yes       | 转置卷积                  |
| Fully connected        | Yes       | 通过 1x1 卷积实现                     |
| Pooling (max, avg)     | Yes       | 各种尺寸和步长               |
| ReLU                   | Yes       | 在 SDP 中与卷积融合           |
| PReLU / Leaky ReLU     | Yes       | 在 SDP 中融合                            |
| Sigmoid                | Yes       | 通过 SDP                                 |
| Tanh                   | Yes       | 通过 SDP                                 |
| Batch Normalization    | Yes       | 在 SDP 中与卷积融合           |
| Element-wise (add/mul) | Yes       | 通过 SDP                                 |
| Concatenation          | Yes       | 通道拼接                   |
| Softmax                | Limited   | 可能回退到 GPU                    |
| Resize / Upsample      | Limited   | 仅最近邻，存在一些约束 |
| Transpose              | No        | 回退到 GPU                       |
| Attention (QKV)        | No        | 回退到 GPU                       |
| Custom plugins         | No        | DLA 只运行编译后的操作       |
| Dynamic shapes         | No        | 要求固定的输入维度          |

### 精度支持

| Precision | DLA Support | Notes                                   |
|-----------|-------------|-----------------------------------------|
| FP32      | No          | 必须转换为 FP16 或 INT8            |
| FP16      | Yes         | DLA 的默认精度                          |
| INT8      | Yes         | 需要校准数据                |
| TF32      | No          | 仅 GPU 支持的特性                         |
| BF16      | No          | Orin DLA 不支持                |

### 遇到不支持的 layer 时会发生什么

如果启用了 `GPU_FALLBACK`：

```
DLA layers → DLA engine
   ↓
Unsupported layer → data transfer → GPU
   ↓
GPU executes layer
   ↓
Next DLA layer → data transfer → back to DLA
```

每次 DLA↔GPU 切换都涉及一次内存同步（如果使用零拷贝则不是拷贝，而是一个同步 fence）。切换次数过多会增加延迟。

### 最大化 DLA 利用率

* 使用包含 DLA 友好操作的架构（以卷积为主的 CNN）
* 避免 attention 机制、动态 shape 和自定义操作
* 在导出前将 batch normalization 融合进卷积
* 使用 INT8 以获得最大 DLA 吞吐
* 通过把不支持的 layer 分组来减少 DLA↔GPU 切换

---

## 9. DLA 执行流程 —— 逐步解析

### 示例：图像分类（在 DLA 上运行 ResNet50）

```
Step 1: Application submits inference request
        Input: 224x224x3 image tensor (FP16)
             ↓
Step 2: TensorRT runtime selects DLA engine
        Deserializes compiled DLA loadable
             ↓
Step 3: libnvdla programs DLA registers
        Sets up DMA descriptors for input/weights/output
        All addresses are IOVA (mapped through SMMU)
             ↓
Step 4: DLA DMA engine reads input tensor from DRAM
        Streams data into on-chip SRAM buffer
             ↓
Step 5: Convolution core processes layer 1
        MAC array performs conv2d on input
        SDP applies batch norm + ReLU (fused)
        PDP applies max pooling
             ↓
Step 6: Intermediate result written to DRAM (or kept in SRAM)
        Next layer reads from previous output
             ↓
Step 7: Repeat for all DLA-compatible layers
        Conv → BN → ReLU → Pool → Conv → ...
             ↓
Step 8: If unsupported layer encountered:
        DLA writes intermediate to DRAM
        GPU reads same physical pages (zero-copy via SMMU)
        GPU executes unsupported layer(s)
        DLA reads GPU output and continues
             ↓
Step 9: Final output tensor written to DRAM via DMA
             ↓
Step 10: DLA raises completion interrupt
         Kernel driver signals userspace
         Application reads output tensor
```

---

## 10. 多引擎调度（DLA + GPU）

### 异构执行

在 Orin Nano 上，可以同时在 DLA 和 GPU 上运行工作负载：

```
┌─────────────────────────────────────────┐
│               Time →                     │
│                                          │
│  DLA:  [Model A inference ][Model A    ] │
│  GPU:  [Model B inference      ][post] │
│  CPU:  [pre-process][      ][result]    │
│                                          │
└─────────────────────────────────────────┘
```


<details>
<summary>English original</summary>

**DLA-Supported Operations**

| Operation              | Supported | Notes                                  |
|------------------------|-----------|----------------------------------------|
| Convolution (2D)       | Yes       | All kernel sizes, strides, dilation    |
| Deconvolution          | Yes       | Transposed convolution                  |
| Fully connected        | Yes       | Via 1x1 convolution                     |
| Pooling (max, avg)     | Yes       | Various sizes and strides               |
| ReLU                   | Yes       | Fused with convolution in SDP           |
| PReLU / Leaky ReLU     | Yes       | Fused in SDP                            |
| Sigmoid                | Yes       | Via SDP                                 |
| Tanh                   | Yes       | Via SDP                                 |
| Batch Normalization    | Yes       | Fused with convolution in SDP           |
| Element-wise (add/mul) | Yes       | Via SDP                                 |
| Concatenation          | Yes       | Channel concatenation                   |
| Softmax                | Limited   | May fall back to GPU                    |
| Resize / Upsample      | Limited   | Nearest-neighbor only, some constraints |
| Transpose              | No        | Falls back to GPU                       |
| Attention (QKV)        | No        | Falls back to GPU                       |
| Custom plugins         | No        | DLA only runs compiled operations       |
| Dynamic shapes         | No        | Fixed input dimensions required          |

**Precision Support**

| Precision | DLA Support | Notes                                   |
|-----------|-------------|-----------------------------------------|
| FP32      | No          | Must convert to FP16 or INT8            |
| FP16      | Yes         | Default for DLA                          |
| INT8      | Yes         | Requires calibration data                |
| TF32      | No          | GPU-only feature                         |
| BF16      | No          | Not supported on Orin DLA                |

**What Happens With Unsupported Layers**

If `GPU_FALLBACK` is enabled:

```
DLA layers → DLA engine
   ↓
Unsupported layer → data transfer → GPU
   ↓
GPU executes layer
   ↓
Next DLA layer → data transfer → back to DLA
```

Each DLA↔GPU transition involves a memory synchronization (not a copy if using zero-copy, but a sync fence). Too many transitions add latency.

**Maximizing DLA Utilization**

* Use architectures with DLA-friendly operations (convolution-heavy CNNs)
* Avoid attention mechanisms, dynamic shapes, and custom operations
* Fuse batch normalization into convolution before export
* Use INT8 for maximum DLA throughput
* Minimize DLA↔GPU transitions by grouping unsupported layers

---

**9. DLA Execution Flow — Step by Step**

**Example: Image Classification (ResNet50 on DLA)**

```
Step 1: Application submits inference request
        Input: 224x224x3 image tensor (FP16)
             ↓
Step 2: TensorRT runtime selects DLA engine
        Deserializes compiled DLA loadable
             ↓
Step 3: libnvdla programs DLA registers
        Sets up DMA descriptors for input/weights/output
        All addresses are IOVA (mapped through SMMU)
             ↓
Step 4: DLA DMA engine reads input tensor from DRAM
        Streams data into on-chip SRAM buffer
             ↓
Step 5: Convolution core processes layer 1
        MAC array performs conv2d on input
        SDP applies batch norm + ReLU (fused)
        PDP applies max pooling
             ↓
Step 6: Intermediate result written to DRAM (or kept in SRAM)
        Next layer reads from previous output
             ↓
Step 7: Repeat for all DLA-compatible layers
        Conv → BN → ReLU → Pool → Conv → ...
             ↓
Step 8: If unsupported layer encountered:
        DLA writes intermediate to DRAM
        GPU reads same physical pages (zero-copy via SMMU)
        GPU executes unsupported layer(s)
        DLA reads GPU output and continues
             ↓
Step 9: Final output tensor written to DRAM via DMA
             ↓
Step 10: DLA raises completion interrupt
         Kernel driver signals userspace
         Application reads output tensor
```

---

**10. Multi-Engine Scheduling (DLA + GPU)**

**Heterogeneous Execution**

On Orin Nano, you can run workloads on DLA and GPU simultaneously:

```
┌─────────────────────────────────────────┐
│               Time →                     │
│                                          │
│  DLA:  [Model A inference ][Model A    ] │
│  GPU:  [Model B inference      ][post] │
│  CPU:  [pre-process][      ][result]    │
│                                          │
└─────────────────────────────────────────┘
```

</details>

### 流水线架构

用于实时视频推理：

```
Frame N:    CPU pre-process → DLA inference → CPU post-process
Frame N+1:  CPU pre-process → GPU inference → CPU post-process
Frame N+2:  CPU pre-process → DLA inference → CPU post-process

DLA and GPU alternate or run different models in parallel.
```

### TensorRT 多流执行

```python
import tensorrt as trt
import pycuda.driver as cuda

# Create two execution contexts
context_dla = engine_dla.create_execution_context()
context_gpu = engine_gpu.create_execution_context()

# Create two CUDA streams
stream_dla = cuda.Stream()
stream_gpu = cuda.Stream()

# Execute in parallel
context_dla.execute_async_v2(bindings_dla, stream_dla.handle)
context_gpu.execute_async_v2(bindings_gpu, stream_gpu.handle)

# Wait for both
stream_dla.synchronize()
stream_gpu.synchronize()
```

### DLA + GPU 并行的收益

| 指标           | 仅 GPU       | 仅 DLA      | DLA + GPU      |
|----------------|-------------|-------------|----------------|
| 吞吐           | 1x           | 0.5–0.8x   | 1.3–1.8x       |
| 功耗           | 高           | 低          | 中              |
| 延迟           | 中           | 低          | 中（流水线）    |
| GPU 可用性     | 0%           | 100%        | 100%（可用于其他任务） |

在 DLA 上运行推理可释放 GPU 用于：

* 显示渲染
* 视频编码/解码（NVENC/NVDEC）
* 额外的 CUDA 工作负载
* GStreamer 处理

---

## 11. DLA Kernel 驱动

### 驱动架构

DLA kernel 驱动是 nvhost 子系统的一部分：

```
/dev/nvhost-nvdla0        ← DLA engine 0 device node
/dev/nvhost-nvdla1        ← DLA engine 1 (if present)
```

### 驱动职责

| 职责                  | 说明                                            |
|-----------------------|-------------------------------------------------|
| 缓冲区分配            | 为 DLA DMA 分配 CMA 缓冲区                      |
| SMMU 映射             | 将物理页映射到 DLA 的 IOVA 空间                  |
| 任务提交              | 配置 DLA 寄存器，启动执行                        |
| 中断处理              | 接收完成中断，通知用户态                         |
| 电源管理              | 空闲时时钟门控、电源门控                         |
| 错误处理              | 检测 DLA 故障，上报用户态                        |

### 模块加载

```bash
# Check if DLA driver is loaded
lsmod | grep nvdla
# nvdla                  12345  0

# Check device nodes
ls /dev/nvhost-nvdla*
# /dev/nvhost-nvdla0

# Check driver messages
dmesg | grep nvdla
# nvdla 15880000.nvdla0: probed
```

### DLA 时钟与电源

DLA 有自己的时钟域，由 BPMP 管理：

```bash
# Check DLA clock rate
cat /sys/kernel/debug/clk/clk_summary | grep dla

# DLA power domain
cat /sys/kernel/debug/bpmp/debug/regulator/*/name | grep dla
```

DLA 空闲时处于电源门控状态 —— 不执行推理时功耗接近零。

---

## 12. DLA 内存路径 —— 完整流水线

### 完整数据流

```
1. TensorRT allocates input buffer
   └→ dma_alloc_coherent() → CMA region → physical pages
   └→ SMMU maps pages → IOVA for DLA

2. Application fills input buffer (e.g., camera frame)
   └→ If from camera: DMA-BUF import (zero-copy from ISP)
   └→ If from CPU: memcpy into mapped buffer

3. TensorRT submits inference to DLA
   └→ libnvdla programs DMA descriptors with IOVAs
   └→ ioctl to /dev/nvhost-nvdla0

4. Kernel driver submits work
   └→ Writes to DLA control registers
   └→ DLA starts execution

5. DLA DMA engine reads input tensor
   └→ IOVA → SMMU translation → physical DRAM
   └→ Streams into on-chip SRAM

6. DLA processes layers
   └→ Conv core → SDP → PDP (all on-chip)
   └→ Intermediate results: SRAM or spill to DRAM

7. DLA DMA engine writes output tensor
   └→ IOVA → SMMU → physical DRAM

8. DLA raises IRQ
   └→ Kernel driver handles interrupt
   └→ Signals completion to userspace

9. Application reads output
   └→ Same mapped buffer (zero-copy to CPU)
   └→ Or GPU reads same pages (zero-copy via GPU SMMU)
```

---


<details>
<summary>English original</summary>

**Pipeline Architecture**

For real-time video inference:

```
Frame N:    CPU pre-process → DLA inference → CPU post-process
Frame N+1:  CPU pre-process → GPU inference → CPU post-process
Frame N+2:  CPU pre-process → DLA inference → CPU post-process

DLA and GPU alternate or run different models in parallel.
```

**TensorRT Multi-Stream Execution**

```python
import tensorrt as trt
import pycuda.driver as cuda

# Create two execution contexts
context_dla = engine_dla.create_execution_context()
context_gpu = engine_gpu.create_execution_context()

# Create two CUDA streams
stream_dla = cuda.Stream()
stream_gpu = cuda.Stream()

# Execute in parallel
context_dla.execute_async_v2(bindings_dla, stream_dla.handle)
context_gpu.execute_async_v2(bindings_gpu, stream_gpu.handle)

# Wait for both
stream_dla.synchronize()
stream_gpu.synchronize()
```

**Benefits of DLA + GPU Parallelism**

| Metric         | GPU Only     | DLA Only    | DLA + GPU      |
|----------------|-------------|-------------|----------------|
| Throughput     | 1x           | 0.5–0.8x   | 1.3–1.8x       |
| Power          | High         | Low         | Medium          |
| Latency        | Medium       | Low         | Medium (pipelined) |
| GPU availability | 0%         | 100%        | 100% (for other tasks) |

Running inference on DLA frees the GPU for:

* Display rendering
* Video encode/decode (NVENC/NVDEC)
* Additional CUDA workloads
* GStreamer processing

---

**11. DLA Kernel Driver**

**Driver Architecture**

The DLA kernel driver is part of the nvhost subsystem:

```
/dev/nvhost-nvdla0        ← DLA engine 0 device node
/dev/nvhost-nvdla1        ← DLA engine 1 (if present)
```

**Driver Responsibilities**

| Responsibility        | Details                                         |
|-----------------------|-------------------------------------------------|
| Buffer allocation     | Allocates CMA buffers for DLA DMA               |
| SMMU mapping          | Maps physical pages to DLA's IOVA space          |
| Work submission       | Programs DLA registers, starts execution          |
| Interrupt handling    | Receives completion interrupt, signals userspace  |
| Power management      | Clock gating, power gating when idle              |
| Error handling        | Detects DLA faults, reports to userspace          |

**Module Loading**

```bash
# Check if DLA driver is loaded
lsmod | grep nvdla
# nvdla                  12345  0

# Check device nodes
ls /dev/nvhost-nvdla*
# /dev/nvhost-nvdla0

# Check driver messages
dmesg | grep nvdla
# nvdla 15880000.nvdla0: probed
```

**DLA Clock and Power**

DLA has its own clock domain managed by BPMP:

```bash
# Check DLA clock rate
cat /sys/kernel/debug/clk/clk_summary | grep dla

# DLA power domain
cat /sys/kernel/debug/bpmp/debug/regulator/*/name | grep dla
```

DLA is power-gated when idle — it consumes near-zero power when not executing inference.

---

**12. DLA Memory Path — Full Pipeline**

**Complete Data Flow**

```
1. TensorRT allocates input buffer
   └→ dma_alloc_coherent() → CMA region → physical pages
   └→ SMMU maps pages → IOVA for DLA

2. Application fills input buffer (e.g., camera frame)
   └→ If from camera: DMA-BUF import (zero-copy from ISP)
   └→ If from CPU: memcpy into mapped buffer

3. TensorRT submits inference to DLA
   └→ libnvdla programs DMA descriptors with IOVAs
   └→ ioctl to /dev/nvhost-nvdla0

4. Kernel driver submits work
   └→ Writes to DLA control registers
   └→ DLA starts execution

5. DLA DMA engine reads input tensor
   └→ IOVA → SMMU translation → physical DRAM
   └→ Streams into on-chip SRAM

6. DLA processes layers
   └→ Conv core → SDP → PDP (all on-chip)
   └→ Intermediate results: SRAM or spill to DRAM

7. DLA DMA engine writes output tensor
   └→ IOVA → SMMU → physical DRAM

8. DLA raises IRQ
   └→ Kernel driver handles interrupt
   └→ Signals completion to userspace

9. Application reads output
   └→ Same mapped buffer (zero-copy to CPU)
   └→ Or GPU reads same pages (zero-copy via GPU SMMU)
```

---

</details>

## 13. CPU/GPU/DLA/SMMU/CMA 交互图

```
┌─────────────────────────────────────────────────────────────┐
│                        8GB LPDDR5                            │
│                                                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐    │
│  │ OS/Kernel │  │ CUDA     │  │ CMA      │  │ Carve-   │    │
│  │ Memory   │  │ Memory   │  │ Region   │  │ outs     │    │
│  │          │  │ (GPU)    │  │          │  │ (BPMP,   │    │
│  │          │  │          │  │          │  │  OP-TEE) │    │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └──────────┘    │
│       │              │              │                        │
└───────┼──────────────┼──────────────┼────────────────────────┘
        │              │              │
   ┌────┴────┐    ┌────┴────┐    ┌────┴────────────────────┐
   │  CPU    │    │  GPU    │    │  SMMU                    │
   │  MMU    │    │  SMMU   │    │  (shared by DLA, ISP,   │
   │         │    │  context│    │   VI, NVENC, NVDEC)      │
   │  VA→PA  │    │  IOVA→PA│    │  IOVA→PA                │
   └────┬────┘    └────┬────┘    └────┬────────┬───────────┘
        │              │              │        │
   ┌────┴────┐    ┌────┴────┐    ┌────┴───┐ ┌──┴──────┐
   │  CPU    │    │  GPU    │    │  DLA   │ │  ISP    │
   │  A78AE  │    │ Ampere  │    │ Engine │ │  Camera │
   │  cores  │    │ 1024    │    │        │ │  pipe   │
   │         │    │ cores   │    │ MAC +  │ │         │
   │         │    │         │    │ SRAM + │ │         │
   │         │    │         │    │ DMA    │ │         │
   └─────────┘    └─────────┘    └────────┘ └─────────┘
```

### 数据流示例

**Camera → DLA 推理（零拷贝）：**

```
Camera sensor
 → NVCSI → VI → ISP
 → CMA buffer (physical pages)
 → SMMU maps to ISP IOVA (write) and DLA IOVA (read)
 → DLA reads same physical pages — zero copy
 → DLA output to CMA
 → CPU reads result — zero copy
```

**Camera → GPU 推理 → DLA 后处理：**

```
Camera → CMA buffer
 → GPU SMMU maps buffer → GPU inference
 → GPU output to CUDA memory
 → DLA SMMU maps same pages → DLA post-processing
 → DLA output to CMA → CPU reads result
```

**DLA + GPU 并行推理（两个模型）：**

```
Input → CMA buffer
 ├→ DLA SMMU → DLA runs Model A
 └→ GPU SMMU → GPU runs Model B
Both access DRAM through SMMU, no copies between them
Results available simultaneously
```

---

## 14. 性能特征

### 典型推理延迟（Orin Nano 8GB）

| 模型         | 精度 | 引擎 | 批 | 延迟    | 功耗   |
|---------------|-----------|--------|-------|------------|---------|
| ResNet-50     | INT8      | DLA    | 1     | ~5–8 ms    | ~1.5W   |
| ResNet-50     | INT8      | GPU    | 1     | ~3–5 ms    | ~5W     |
| ResNet-50     | FP16      | DLA    | 1     | ~8–12 ms   | ~2W     |
| MobileNetV2   | INT8      | DLA    | 1     | ~2–4 ms    | ~1W     |
| MobileNetV2   | INT8      | GPU    | 1     | ~1–3 ms    | ~3W     |
| YOLOv5s       | FP16      | DLA    | 1     | ~15–25 ms  | ~2.5W   |
| YOLOv5s       | FP16      | GPU    | 1     | ~8–12 ms   | ~6W     |

### 吞吐与能效对比

| 指标                  | GPU        | DLA         |
|-------------------------|------------|-------------|
| 原始吞吐（FPS）    | 更高     | 更低       |
| 每瓦 TOPS           | 更低      | **更高**  |
| 每焦耳帧数        | 更低      | **更高**  |

DLA 胜在能效（TOPS/watt），GPU 胜在原始速度。依据你的约束选择——功耗预算还是吞吐目标。

### DLA 上的 INT8 与 FP16

| 精度 | 吞吐 | 准确率 | 是否需要校准 |
|-----------|-----------|----------|----------------------|
| FP16      | 1x        | 更高   | 否                   |
| INT8      | ~2x       | 更低    | 是（校准数据集） |

INT8 大致使 DLA 吞吐翻倍。当准确率损失可接受时使用 INT8（用你的数据集实测）。

---

## 15. DLA 工作负载的性能剖析

### trtexec —— 快速性能剖析

```bash
# Profile DLA inference
trtexec \
    --onnx=model.onnx \
    --useDLACore=0 \
    --fp16 \
    --allowGPUFallback \
    --verbose \
    --iterations=100

# Key output:
# [DLA] Layer conv1: 1.2ms
# [GPU] Layer unsupported_op: 0.5ms (fallback)
# [DLA] Layer conv2: 0.8ms
# Total: 5.3ms
# DLA utilization: 78%
```

### Nsight Systems —— 详细时间线

```bash
nsys profile --trace=cuda,nvtx,osrt \
    trtexec --loadEngine=model_dla.engine --iterations=50

# Open in Nsight Systems GUI
# Shows:
# - DLA execution blocks
# - GPU fallback blocks
# - DMA transfers
# - CPU overhead
# - DLA↔GPU sync points
```

### DLA 专属指标

```bash
# Check DLA utilization via tegrastats
tegrastats --interval 1000
# Output includes DLA% utilization

# DLA clock frequency
cat /sys/kernel/debug/clk/clk_summary | grep dla
```


<details>
<summary>English original</summary>

**13. CPU/GPU/DLA/SMMU/CMA Interaction Diagram**

```
┌─────────────────────────────────────────────────────────────┐
│                        8GB LPDDR5                            │
│                                                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐    │
│  │ OS/Kernel │  │ CUDA     │  │ CMA      │  │ Carve-   │    │
│  │ Memory   │  │ Memory   │  │ Region   │  │ outs     │    │
│  │          │  │ (GPU)    │  │          │  │ (BPMP,   │    │
│  │          │  │          │  │          │  │  OP-TEE) │    │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └──────────┘    │
│       │              │              │                        │
└───────┼──────────────┼──────────────┼────────────────────────┘
        │              │              │
   ┌────┴────┐    ┌────┴────┐    ┌────┴────────────────────┐
   │  CPU    │    │  GPU    │    │  SMMU                    │
   │  MMU    │    │  SMMU   │    │  (shared by DLA, ISP,   │
   │         │    │  context│    │   VI, NVENC, NVDEC)      │
   │  VA→PA  │    │  IOVA→PA│    │  IOVA→PA                │
   └────┬────┘    └────┬────┘    └────┬────────┬───────────┘
        │              │              │        │
   ┌────┴────┐    ┌────┴────┐    ┌────┴───┐ ┌──┴──────┐
   │  CPU    │    │  GPU    │    │  DLA   │ │  ISP    │
   │  A78AE  │    │ Ampere  │    │ Engine │ │  Camera │
   │  cores  │    │ 1024    │    │        │ │  pipe   │
   │         │    │ cores   │    │ MAC +  │ │         │
   │         │    │         │    │ SRAM + │ │         │
   │         │    │         │    │ DMA    │ │         │
   └─────────┘    └─────────┘    └────────┘ └─────────┘
```

**Data Flow Examples**

**Camera → DLA inference (zero-copy):**

```
Camera sensor
 → NVCSI → VI → ISP
 → CMA buffer (physical pages)
 → SMMU maps to ISP IOVA (write) and DLA IOVA (read)
 → DLA reads same physical pages — zero copy
 → DLA output to CMA
 → CPU reads result — zero copy
```

**Camera → GPU inference → DLA post-processing:**

```
Camera → CMA buffer
 → GPU SMMU maps buffer → GPU inference
 → GPU output to CUDA memory
 → DLA SMMU maps same pages → DLA post-processing
 → DLA output to CMA → CPU reads result
```

**DLA + GPU parallel inference (two models):**

```
Input → CMA buffer
 ├→ DLA SMMU → DLA runs Model A
 └→ GPU SMMU → GPU runs Model B
Both access DRAM through SMMU, no copies between them
Results available simultaneously
```

---

**14. Performance Characteristics**

**Typical Inference Latency (Orin Nano 8GB)**

| Model         | Precision | Engine | Batch | Latency    | Power   |
|---------------|-----------|--------|-------|------------|---------|
| ResNet-50     | INT8      | DLA    | 1     | ~5–8 ms    | ~1.5W   |
| ResNet-50     | INT8      | GPU    | 1     | ~3–5 ms    | ~5W     |
| ResNet-50     | FP16      | DLA    | 1     | ~8–12 ms   | ~2W     |
| MobileNetV2   | INT8      | DLA    | 1     | ~2–4 ms    | ~1W     |
| MobileNetV2   | INT8      | GPU    | 1     | ~1–3 ms    | ~3W     |
| YOLOv5s       | FP16      | DLA    | 1     | ~15–25 ms  | ~2.5W   |
| YOLOv5s       | FP16      | GPU    | 1     | ~8–12 ms   | ~6W     |

**Throughput vs Power Efficiency**

| Metric                  | GPU        | DLA         |
|-------------------------|------------|-------------|
| Raw throughput (FPS)    | Higher     | Lower       |
| TOPS per watt           | Lower      | **Higher**  |
| Frames per joule        | Lower      | **Higher**  |

DLA wins on efficiency (TOPS/watt), GPU wins on raw speed. Choose based on your constraint — power budget or throughput target.

**INT8 vs FP16 on DLA**

| Precision | Throughput | Accuracy | Calibration Required |
|-----------|-----------|----------|----------------------|
| FP16      | 1x        | Higher   | No                   |
| INT8      | ~2x       | Lower    | Yes (calibration dataset) |

INT8 roughly doubles DLA throughput. Use INT8 when accuracy loss is acceptable (test with your dataset).

---

**15. Profiling DLA Workloads**

**trtexec — Quick Profiling**

```bash
# Profile DLA inference
trtexec \
    --onnx=model.onnx \
    --useDLACore=0 \
    --fp16 \
    --allowGPUFallback \
    --verbose \
    --iterations=100

# Key output:
# [DLA] Layer conv1: 1.2ms
# [GPU] Layer unsupported_op: 0.5ms (fallback)
# [DLA] Layer conv2: 0.8ms
# Total: 5.3ms
# DLA utilization: 78%
```

**Nsight Systems — Detailed Timeline**

```bash
nsys profile --trace=cuda,nvtx,osrt \
    trtexec --loadEngine=model_dla.engine --iterations=50

# Open in Nsight Systems GUI
# Shows:
# - DLA execution blocks
# - GPU fallback blocks
# - DMA transfers
# - CPU overhead
# - DLA↔GPU sync points
```

**DLA-Specific Metrics**

```bash
# Check DLA utilization via tegrastats
tegrastats --interval 1000
# Output includes DLA% utilization

# DLA clock frequency
cat /sys/kernel/debug/clk/clk_summary | grep dla
```

</details>

### 识别瓶颈

| 症状 | 原因 | 解决方案 |
|--------------------------------|--------------------------------|------------------------------------|
| DLA 利用率低 | 大量回退到 GPU 的 layer | 采用对 DLA 友好的架构 |
| DLA-GPU 切换耗时长 | 引擎频繁切换 | 将 DLA/GPU layer 连续分组 |
| DLA 延迟高于 GPU | 模型对 DLA 而言过小 | 改用 GPU（开销 > 收益）|
| DLA 延迟不稳定 | 内存带宽争用 | 减少并发 DRAM 访问 |

---

## 16. DLA 限制与回退行为

### 硬性限制

* **不支持 FP32** — 必须使用 FP16 或 INT8
* **不支持动态 shape** — 输入维度必须在构建时固定
* **不支持自定义 CUDA plugin** — DLA 只运行已编译的算子
* **layer 支持有限** — 完整列表见第 8 节
* **仅支持单批** — 并非所有 layer 都支持 batch > 1
* **不支持原地操作** — 每个算子都写入新的缓冲区

### 回退行为

当启用 `GPU_FALLBACK` 且某个 layer 在 DLA 上不受支持时：

1. TensorRT 构建包含 DLA 段与 GPU 段的混合引擎
2. 在 runtime 中，DLA 执行自己的 layer，然后同步
3. GPU 接手不受支持的 layer
4. GPU 完成后，DLA 继续执行（如果后面还有 DLA layer）

每次 DLA→GPU→DLA 切换都会增加：

* 内存同步开销（约 0.1–0.5 ms）
* 上下文切换开销
* 无数据拷贝（通过 SMMU 实现零拷贝）

### 何时不应使用 DLA

* 以 Transformer 为主的模型（不支持 attention）
* 包含大量自定义算子的模型
* 需要 FP32 精度的模型
* 模型极小，DLA 的启动开销超过计算本身
* 对延迟敏感的单模型推理，且 GPU 更快

---

## 17. 生产部署模式

### 模式 1：纯 DLA 推理（能效最高）

```
Camera → pre-process (CPU) → DLA inference → post-process (CPU) → output
GPU: idle / display only
```

适用场景：电池供电设备、热管理受限的系统、常开监控。

### 模式 2：DLA + GPU 流水线（吞吐最高）

```
Camera → pre-process (CPU)
   ├→ DLA: detection model (lightweight, e.g., MobileNet-SSD)
   └→ GPU: classification model (heavier, e.g., ResNet-50)
Both run simultaneously on alternating frames or different ROIs.
```

适用场景：多模型系统、检测 + 分类的视频分析。

### 模式 3：DLA 为主，GPU 回退（均衡）

```
Full model compiled for DLA with GPU fallback enabled.
DLA handles convolutions, GPU handles unsupported ops.
TensorRT manages transitions automatically.
```

适用场景：部分 layer 不受支持的单模型部署。

### 模式 4：DLA 负责常开 + GPU 按需启动

```
DLA: continuously running lightweight detection (person, vehicle)
GPU: idle, wakes up for heavy processing when DLA detects event
   → GPU runs detailed classification, tracking, or segmentation
   → GPU returns to idle
```

适用场景：安防监控、智能摄像头、事件驱动系统。可最小化平均功耗。

### 生产环境的引擎序列化

生产部署应始终序列化（保存）TensorRT 引擎：

```python
# Build once (slow — compile time)
engine = builder.build_engine(network, config)

# Serialize
with open("model_dla_int8.engine", "wb") as f:
    f.write(engine.serialize())

# Deploy: deserialize (fast — load time)
runtime = trt.Runtime(logger)
with open("model_dla_int8.engine", "rb") as f:
    engine = runtime.deserialize_cuda_engine(f.read())
```

引擎文件与设备相关——为 Orin Nano 构建的引擎无法在 Orin NX 或 AGX Orin 上运行。每个目标平台都需重新构建。

---

## 18. DLA 常见问题与解决方案

### DLA 引擎构建失败

```
[TensorRT] ERROR: Layer X is not supported on DLA
```

**原因：** 模型包含不受支持的算子。

**解决方案：** 启用 `GPU_FALLBACK`，或修改模型改用对 DLA 友好的算子。

### DLA 推理比 GPU 慢

**原因：** DLA↔GPU 切换过多，或模型过小、DLA 开销占比过大。

**解决方案：** 用 `trtexec --verbose` 做 profile，统计切换次数。若次数很多，考虑只用 GPU。若模型极小，GPU 通常更快。

### DLA 准确率下降（INT8）

**原因：** INT8 校准不佳，或敏感 layer 量化不当。

**解决方案：**
* 使用有代表性的校准数据集（>500 张图像）
* 使用 per-channel 量化而非 per-tensor 量化
* 将敏感 layer（首/末 conv、skip connection）保持为 FP16

### DLA 缓冲区分配失败

```
nvdla: failed to allocate buffer
```

**原因：** CMA 耗尽或碎片化。

**解决方案：** 参见 [Memory Architecture Guide — CMA](../07-Orin-Nano内存架构/Guide.md)，了解 CMA 容量规划与碎片化缓解。


<details>
<summary>English original</summary>

**Identifying Bottlenecks**

| Symptom                        | Cause                          | Solution                           |
|--------------------------------|--------------------------------|------------------------------------|
| Low DLA utilization            | Many GPU fallback layers       | Use DLA-friendly architecture      |
| High DLA-GPU transition time   | Frequent engine switches       | Group DLA/GPU layers contiguously  |
| DLA latency higher than GPU    | Model too small for DLA        | Use GPU instead (overhead > benefit)|
| Inconsistent DLA latency       | Memory bandwidth contention    | Reduce concurrent DRAM access      |

---

**16. DLA Limitations and Fallback Behavior**

**Hard Limitations**

* **No FP32** — must use FP16 or INT8
* **No dynamic shapes** — input dimensions must be fixed at build time
* **No custom CUDA plugins** — DLA only runs compiled operations
* **Limited layer support** — see Section 8 for full list
* **Single batch only** — batch > 1 is not supported on all layers
* **No in-place operations** — every operation writes to a new buffer

**Fallback Behavior**

When `GPU_FALLBACK` is enabled and a layer is not supported on DLA:

1. TensorRT builds a hybrid engine with DLA and GPU sections
2. At runtime, DLA executes its layers, then synchronizes
3. GPU picks up the unsupported layers
4. After GPU finishes, DLA resumes (if more DLA layers follow)

Each DLA→GPU→DLA transition adds:

* Memory synchronization overhead (~0.1–0.5 ms)
* Context switch overhead
* No data copy (zero-copy via SMMU)

**When NOT to Use DLA**

* Transformer-heavy models (attention is not supported)
* Models with many custom operations
* Models requiring FP32 precision
* Very small models where DLA setup overhead exceeds computation
* Latency-critical single-model inference where GPU is faster

---

**17. Production Deployment Patterns**

**Pattern 1: DLA-Only Inference (Maximum Power Efficiency)**

```
Camera → pre-process (CPU) → DLA inference → post-process (CPU) → output
GPU: idle / display only
```

Best for: battery-powered devices, thermal-constrained systems, always-on monitoring.

**Pattern 2: DLA + GPU Pipeline (Maximum Throughput)**

```
Camera → pre-process (CPU)
   ├→ DLA: detection model (lightweight, e.g., MobileNet-SSD)
   └→ GPU: classification model (heavier, e.g., ResNet-50)
Both run simultaneously on alternating frames or different ROIs.
```

Best for: multi-model systems, video analytics with detection + classification.

**Pattern 3: DLA Primary, GPU Fallback (Balanced)**

```
Full model compiled for DLA with GPU fallback enabled.
DLA handles convolutions, GPU handles unsupported ops.
TensorRT manages transitions automatically.
```

Best for: single-model deployment where some layers are unsupported.

**Pattern 4: DLA for Always-On + GPU for On-Demand**

```
DLA: continuously running lightweight detection (person, vehicle)
GPU: idle, wakes up for heavy processing when DLA detects event
   → GPU runs detailed classification, tracking, or segmentation
   → GPU returns to idle
```

Best for: surveillance, smart cameras, event-driven systems. Minimizes average power.

**Engine Serialization for Production**

Always serialize (save) TensorRT engines for production deployment:

```python
# Build once (slow — compile time)
engine = builder.build_engine(network, config)

# Serialize
with open("model_dla_int8.engine", "wb") as f:
    f.write(engine.serialize())

# Deploy: deserialize (fast — load time)
runtime = trt.Runtime(logger)
with open("model_dla_int8.engine", "rb") as f:
    engine = runtime.deserialize_cuda_engine(f.read())
```

Engine files are device-specific — an engine built for Orin Nano will not work on Orin NX or AGX Orin. Rebuild for each target.

---

**18. Common DLA Issues and Solutions**

**DLA Engine Build Fails**

```
[TensorRT] ERROR: Layer X is not supported on DLA
```

**Cause:** Model contains unsupported operations.

**Solution:** Enable `GPU_FALLBACK`, or modify the model to use DLA-friendly operations.

**DLA Inference Slower Than GPU**

**Cause:** Too many DLA↔GPU transitions, or model is too small for DLA overhead.

**Solution:** Profile with `trtexec --verbose` to count transitions. If many, consider GPU-only. If model is tiny, GPU is likely faster.

**DLA Accuracy Degradation (INT8)**

**Cause:** Poor INT8 calibration or sensitive layers quantized incorrectly.

**Solution:**
* Use a representative calibration dataset (>500 images)
* Use per-channel quantization instead of per-tensor
* Keep sensitive layers (first/last conv, skip connections) in FP16

**DLA Buffer Allocation Failure**

```
nvdla: failed to allocate buffer
```

**Cause:** CMA exhaustion or fragmentation.

**Solution:** See [Memory Architecture Guide — CMA](../07-Orin-Nano内存架构/Guide.md) for CMA sizing and fragmentation mitigation.

</details>

### DLA 未检测到

```
ls /dev/nvhost-nvdla*
# No output
```

**原因：** DLA 设备树节点被禁用、驱动未加载，或 JetPack 版本不匹配。

**解决方法：** 检查 `dmesg | grep nvdla`，确认 DTB 中 DLA 节点已启用，确保 `nvdla.ko` 已加载。

---

## 19. 参考资料

* [NVIDIA TensorRT — DLA 文档](https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/index.html#dla_topic) — 官方 DLA 集成指南
* [NVIDIA TensorRT — DLA 支持的 layer](https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/index.html#dla-lay-supp-matrix) — layer 支持矩阵
* [NVIDIA Jetson Linux — DLA](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/) — Jetson DLA 文档
* [trtexec 参考](https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/index.html#trtexec) — TensorRT 命令行性能剖析工具
* 主指南：[Nvidia Jetson 平台指南](../Guide.md)
* 内存深入解析：[Orin Nano 内存架构](../07-Orin-Nano内存架构/Guide.md)
* kernel 内部机制：[Orin Nano kernel 内部机制](../06-Orin-Nano内核内部机制/Guide.md)


<details>
<summary>English original</summary>

**DLA Not Detected**

```
ls /dev/nvhost-nvdla*
# No output
```

**Cause:** DLA device tree node disabled, driver not loaded, or JetPack version mismatch.

**Solution:** Check `dmesg | grep nvdla`, verify DTB has DLA nodes enabled, ensure `nvdla.ko` is loaded.

---

**19. References**

* [NVIDIA TensorRT — DLA Documentation](https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/index.html#dla_topic) — official DLA integration guide
* [NVIDIA TensorRT — DLA Supported Layers](https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/index.html#dla-lay-supp-matrix) — layer support matrix
* [NVIDIA Jetson Linux — DLA](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/) — Jetson DLA documentation
* [trtexec Reference](https://docs.nvidia.com/deeplearning/tensorrt/developer-guide/index.html#trtexec) — TensorRT command-line profiling tool
* Main guide: [Nvidia Jetson Platform Guide](../Guide.md)
* Memory deep dive: [Orin Nano Memory Architecture](../07-Orin-Nano内存架构/Guide.md)
* Kernel internals: [Orin Nano Kernel Internals](../06-Orin-Nano内核内部机制/Guide.md)

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/1. Nvidia Jetson Platform/Orin-Nano-DLA-Deep-Dive/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/1.%20Nvidia%20Jetson%20Platform/Orin-Nano-DLA-Deep-Dive/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
