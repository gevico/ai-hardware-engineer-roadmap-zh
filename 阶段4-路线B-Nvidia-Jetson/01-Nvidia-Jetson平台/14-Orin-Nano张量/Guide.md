---
title: Orin Nano —— Tensor Core 架构及其工作原理
description: Orin Nano —— Tensor Core 架构及其工作原理
---

# Orin Nano —— Tensor Core 架构及其工作原理

<div class="course-identity auto-course" style="--course-accent: #4f46e5; --course-accent-rgb: 79, 70, 229;" markdown="1">
<div class="course-identity__icon">ONTC</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度解析 · Jetson 方向</p>
<p class="course-identity__title">Orin Nano 的专属课程标识 —— Tensor Core 架构及其工作原理。</p>
<p class="course-identity__meta">产物：Jetson 集成演示 · 测量：延迟、内存、功耗、日志</p>
</div>
</div>


> **背景：** 本深度解析讲解 Jetson Orin Nano（Ampere 架构 GPU）上的 **Tensor Core** 架构，以及它如何实现快速、高能效的 AI 推理。理解这些内容，有助于选择精度（FP16/INT8）、解读 benchmark，并理解 TensorRT 与 cuDNN 为何能在 Orin 上达到高 TOPS。

---


## 1. 什么是 Tensor Core？

**Tensor Core** 是 NVIDIA GPU 内部的专用硬件单元，能在单条指令中完成 **矩阵乘累加（MMA）** 运算。它们针对深度学习中最主要的稠密矩阵运算做了优化：线性层、卷积和 attention 都由矩阵乘构成。

* **引入版本：** Volta（2017）；在 Turing、Ampere、Ada、Hopper、Blackwell 中持续演进。
* **Orin Nano GPU：** 基于 **Ampere 架构** —— 与数据中心 A100 同属一个系列，但为边缘场景做了缩减（更少的 SM、更低的功耗）。

在一个周期内，Tensor Core 在同一矩阵运算上能完成的乘加次数远超 CUDA 核心。这正是 Orin Nano 上的 **FP16** 和 **INT8** 推理能在几瓦功耗内达到 **40 AI TOPS**（每秒万亿次运算）的原因：绝大部分工作由 Tensor Core 完成，而非通用 CUDA 核心。

---

## 2. Tensor Core 与 CUDA 核心

| 方面 | CUDA 核心 | Tensor Core |
|--------|------------|--------------|
| **功能** | 通用：标量/向量运算、逻辑、访存操作 | 专用：矩阵乘累加（D = A×B + C） |
| **粒度** | 按 thread：每条指令一个或几个操作 | 按 warp：一条指令完成一个完整的小矩阵（如 16×16×16） |
| **精度** | FP32、INT32 等 | FP16、BF16、TF32、INT8、INT4（取决于架构） |
| **用途** | 非矩阵乘工作：激活值、归约、逐元素、控制流 | 矩阵乘：线性层、卷积、attention（当表示为矩阵乘时） |
| **吞吐** | 矩阵运算的 ops/cycle 较低 | 矩阵运算的 ops/cycle 高得多 |

在 Orin Nano（Ampere）上：

* **1024 个 CUDA 核心** —— 处理所有非稠密矩阵乘的工作：激活函数（ReLU、GELU）、softmax、归一化、数据搬运、自定义 kernel。
* **32 个 Tensor Core** —— 承担繁重的矩阵乘工作。当 TensorRT 或 cuDNN 执行某个 layer 时，它们会调度 **WMMA**（Warp Matrix Multiply-Accumulate）或库内构建的 kernel，使其针对这 32 个 Tensor Core。

因此：**CUDA 核心** = 通用计算；**Tensor Core** = 矩阵引擎。当大部分时间花在 Tensor Core 的矩阵乘上、其余部分尽量少（融合算子、良好的内存访问）时，推理就很快。

---

## 3. Ampere Tensor Core 架构（Orin Nano）

Orin Nano 的 GPU 是一颗 **Tegra234**（T234）SoC，内含 **Ampere** 级 GPU。确切的布局属于专有信息，但公开的模型如下：

* **流式多处理器（SM）：** GPU 被划分为多个 SM。每个 SM 包含：
  * **CUDA 核心**（整数与浮点）
  * **Tensor Core**（每个 SM 一个或多个）
  * **共享内存**、**L1 缓存**、**warp 调度器**

* **Orin Nano 8GB** 共有 **32 个 Tensor Core**（关键规格中通常写作 “32 个 Tensor Core”）。它们由所有 SM 共享；每个 SM 都能从自己的 warp 发出 Tensor Core 指令。

* **内存路径：** Tensor Core 从**寄存器**和/或**共享内存**读取 **A**、**B** 矩阵（以及可选的累加项 **C**）。数据由 CUDA 核心或 load 指令从**全局内存**（LPDDR5）取入，随后暂存到共享内存和寄存器中，供 Tensor Core 对分块进行运算。因此，**内存带宽**（LPDDR5 速率）和**分块**（数据在共享内存中的复用程度）仍会限制 Tensor Core 的峰值利用率。

概念上：

```
                    Orin Nano GPU (Ampere)
┌─────────────────────────────────────────────────────────────┐
│  SMs (Streaming Multiprocessors)                             │
│  ┌─────────────┐  ┌─────────────┐       ┌─────────────┐     │
│  │ CUDA cores  │  │ Tensor cores│  ...  │ Tensor cores│     │
│  │ Shared mem  │  │ (MMA units) │       │ (MMA units) │     │
│  └─────────────┘  └─────────────┘       └─────────────┘     │
│         ↕                  ↕                      ↕            │
│  L1 / Shared memory ←→ Register file ←→ Tensor Core arrays    │
└─────────────────────────────────────────────────────────────┘
                              ↕
                    L2 cache / Unified memory (LPDDR5)
```

---

## 4. Tensor Core 如何工作：矩阵乘累加


<details>
<summary>English original</summary>

**Orin Nano — Tensor Core Architecture and How It Works**

<div class="course-identity auto-course" style="--course-accent: #4f46e5; --course-accent-rgb: 79, 70, 229;" markdown="1">
<div class="course-identity__icon">ONTC</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Jetson Track</p>
<p class="course-identity__title">Specialized course identity for Orin Nano — Tensor Core Architecture and How It Works.</p>
<p class="course-identity__meta">Artifact: Jetson integration demo · Measure: latency, memory, power, logs</p>
</div>
</div>


> **Context:** This deep dive explains **tensor core** architecture on the Jetson Orin Nano (Ampere GPU) and how it enables fast, power-efficient AI inference. Understanding this helps you choose precisions (FP16/INT8), interpret benchmarks, and reason about why TensorRT and cuDNN achieve high TOPS on Orin.

---


**1. What Are Tensor Cores?**

**Tensor Cores** are dedicated hardware units inside NVIDIA GPUs that perform **matrix multiply-accumulate (MMA)** operations in a single instruction. They are optimized for the dense matrix math that dominates deep learning: linear layers, convolutions, and attention are all built from matrix multiplies.

* **Introduced:** Volta (2017); evolved in Turing, Ampere, Ada, Hopper, Blackwell.
* **Orin Nano GPU:** Based on **Ampere** architecture — the same family as datacenter A100, but scaled down for edge (fewer SMs, lower power).

In one cycle, a tensor core can do many more multiply-adds than a CUDA core on the same matrix operation. That is why **FP16** and **INT8** inference on Orin Nano can reach **40 AI TOPS** (trillion operations per second) while staying within a few watts: most of the work is done by tensor cores, not by the general-purpose CUDA cores.

---

**2. Tensor Cores vs CUDA Cores**

| Aspect | CUDA Cores | Tensor Cores |
|--------|------------|--------------|
| **Function** | General-purpose: scalar/vector math, logic, memory ops | Specialized: matrix multiply-accumulate (D = A×B + C) |
| **Granularity** | Per-thread: one or a few ops per instruction | Per-warp: one instruction does a full small matrix (e.g. 16×16×16) |
| **Precision** | FP32, INT32, etc. | FP16, BF16, TF32, INT8, INT4 (architecture-dependent) |
| **Used for** | Non-matmul work: activations, reductions, element-wise, control flow | Matmul: linear layers, conv, attention (when expressed as matmul) |
| **Throughput** | Lower ops/cycle for matrix math | Much higher ops/cycle for matrix math |

On Orin Nano (Ampere):

* **1024 CUDA cores** — handle everything that is not a dense matmul: activations (ReLU, GELU), softmax, normalization, data movement, custom kernels.
* **32 tensor cores** — handle the heavy matmul work. When TensorRT or cuDNN runs a layer, they schedule **WMMA** (Warp Matrix Multiply-Accumulate) or library-built kernels that target these 32 tensor cores.

So: **CUDA cores** = general compute; **tensor cores** = matrix engines. Inference is fast when most time is spent in tensor-core matmul and the rest is minimal (fused ops, good memory access).

---

**3. Ampere Tensor Core Architecture (Orin Nano)**

Orin Nano’s GPU is a **Tegra234** (T234) SoC with an **Ampere**-class GPU. The exact layout is proprietary, but the public model is:

* **Streaming Multiprocessors (SMs):** The GPU is divided into SMs. Each SM has:
  * **CUDA cores** (integer and floating-point)
  * **Tensor cores** (one or more per SM)
  * **Shared memory**, **L1 cache**, **warp schedulers**

* **Orin Nano 8GB** has **32 tensor cores** total (often quoted as “32 tensor cores” in the key specs). These are shared across all SMs; each SM can issue tensor-core instructions from its warps.

* **Memory path:** Tensor cores read **A** and **B** matrices (and optionally **C** for accumulate) from **registers** and/or **shared memory**. Data is brought from **global memory** (LPDDR5) by CUDA cores or load instructions, then staged in shared memory and registers so that tensor cores operate on tiles. So **memory bandwidth** (LPDDR5 speed) and **tiling** (how well you reuse data in shared memory) still limit peak tensor-core utilization.

Conceptually:

```
                    Orin Nano GPU (Ampere)
┌─────────────────────────────────────────────────────────────┐
│  SMs (Streaming Multiprocessors)                             │
│  ┌─────────────┐  ┌─────────────┐       ┌─────────────┐     │
│  │ CUDA cores  │  │ Tensor cores│  ...  │ Tensor cores│     │
│  │ Shared mem  │  │ (MMA units) │       │ (MMA units) │     │
│  └─────────────┘  └─────────────┘       └─────────────┘     │
│         ↕                  ↕                      ↕            │
│  L1 / Shared memory ←→ Register file ←→ Tensor Core arrays    │
└─────────────────────────────────────────────────────────────┘
                              ↕
                    L2 cache / Unified memory (LPDDR5)
```

---

**4. How Tensor Cores Work: Matrix Multiply-Accumulate**

</details>

### 4.1 运算

张量核心计算：

**D = A × B + C**

* **A**、**B**：输入矩阵（例如激活值和权重）。
* **C**：累加器（通常是之前的中间结果）。
* **D**：输出（累加结果）。

这正是线性层或卷积（展平为矩阵乘时）的模式。一条 **Tensor Core 指令**完成其中一个小块（例如 FP16 下的 16×16×16 MMA）。编译器/runtime 会调度许多这样的小块来覆盖整个矩阵。

### 4.2 warp 级操作

张量核心是 **warp 级**的：一个 **warp**（32 个线程）协同为一次 MMA 提供数据。warp 中的线程持有矩阵的不同部分（例如 16×16 分块的不同行/列）。硬件一次性执行完整的小矩阵乘。因此：

* 无需编写标量乘法循环；而是**加载分块**到寄存器（以及共享内存），然后每个分块**发射一条 WMMA 指令**。
* **分块**（如何把大矩阵拆成 16×16 或 8×8 的块）的选择，要使得分块能放进寄存器和共享内存，并让张量核心保持忙碌。

### 4.3 数据流（简化）

1. **加载：** CUDA 把 **A** 和 **B** 的分块从全局/共享内存加载到寄存器（或共享内存，供 Tensor Core 读取）。
2. **计算：** Tensor Core 指令：对该分块执行 **D = A×B + C**。
3. **存储：** 结果 **D** 写回共享内存或寄存器；随后写入全局内存，或供下一层复用。

高效的 kernel 会在同一个 kernel 内**融合**多个步骤（例如加 bias、ReLU），使结果 **D** 不必写入全局内存再读回——这能节省带宽并提升性能。

---

## 5. 精度支持：FP16、BF16、INT8、TF32

张量核心支持不同的**数值格式**；每种格式都在精度与吞吐、功耗之间做权衡。

| 格式 | 位宽 | 典型用途 | 吞吐（相对 FP32） | Orin Nano / Ampere 架构 |
|--------|-----------|-------------|------------------------|---------------------|
| **FP32** | 32 位浮点 | 训练、参照 | 1×（无 Tensor Core） | 仅 CUDA 核心 |
| **TF32** | 19 位（尾数截断） | 在 Ampere 架构及更高架构上训练 | 高 | Ampere 架构支持 |
| **FP16** | 16 位浮点 | 推理、混合精度 | ~2×（Tensor Core） | ✅ 原生 |
| **BF16** | 16 位（指数范围与 FP32 相同） | 训练、部分推理 | ~2×（Tensor Core） | ✅ 原生 |
| **INT8** | 8 位整数 | 量化推理 | ~4×（Tensor Core） | ✅ 原生 |
| **INT4** | 4 位 | 极低位宽推理 | 更高 | 取决于架构 |

在 **Orin Nano（Ampere 架构）** 上做推理时，最需要关注的是：

* **FP16** — TensorRT 和许多模型的默认选项；会使用张量核心；准确率良好。
* **INT8** — 量化模型；正确校准后相比 FP16 提速 2× 或更多；准确率略有损失。
* **FP32** — 不使用张量核心；运行在 CUDA 核心上；较慢，用于调试或需要高精度时。

在 Jetson 上，用 `--fp16` 或 `--int8` 构建 engine 时，TensorRT 会选择使用张量核心的 kernel。因此，“张量核心如何工作”直接解释了为什么同样的硬件上 FP16/INT8 engine 比 FP32 快得多。

---

## 6. 软件如何使用张量核心（TensorRT、cuDNN）

### 6.1 TensorRT

构建 TensorRT engine 时（例如 `trtexec --onnx=model.onnx --fp16`）：

1. 解析并优化 **ONNX**（或其他）图。
2. 各层（例如 `Gemm`、`Conv`）被**映射到 GPU kernel**。对于线性层/卷积层，TensorRT 在可用时会选择**张量核心 kernel**（FP16 或 INT8）。
3. engine 是一条**由 kernel 启动组成的序列**。其中许多 kernel 是 **基于 WMMA 的**，或使用 NVIDIA 内部的张量核心 API，从而让 Orin Nano 上的 32 个张量核心承担大部分计算。
4. **融合**（例如在一个 kernel 中完成 conv + bias + ReLU）把数据保留在寄存器/共享内存中，减少往返全局内存的次数。

做推理时不需要手写张量核心代码；TensorRT（以及底层的 cuDNN）会完成这件事。理解它们使用了张量核心，就能解释 **40 TOPS** 以及从 `--fp16` / `--int8` 获得的巨大提升。

### 6.2 cuDNN

cuDNN 为卷积、矩阵乘和其他算子提供**例程**。在 Ampere 架构上，当问题规模和精度匹配时，这些例程会使用**张量核心实现**。PyTorch 和其他框架会调用 cuDNN；TensorRT 也使用 cuDNN 或自己的张量核心 kernel。因此，“软件使用张量核心”意味着：**当你使用 FP16/INT8 时，TensorRT 和 cuDNN（进而大多数框架）会自动调度张量核心 MMA 指令**。


<details>
<summary>English original</summary>

**4.1 The Operation**

Tensor cores compute:

**D = A × B + C**

* **A**, **B**: input matrices (e.g. activations and weights).
* **C**: accumulator (often the previous partial result).
* **D**: output (accumulated result).

This is exactly the pattern of a linear layer or a convolution (when flattened to matmul). One **tensor-core instruction** completes a small block of this (e.g. a 16×16×16 MMA in FP16). Many such blocks are scheduled by the compiler/runtime to cover the full matrix.

**4.2 Warp-Level Operation**

Tensor cores are **warp-level**: one **warp** (32 threads) cooperates to feed one MMA. The threads in the warp hold different parts of the matrices (e.g. different rows/columns of the 16×16 tile). The hardware executes the full small matmul in one go. So:

* You don’t write a loop of scalar multiplies; you **load tiles** into registers (and shared memory), then **issue one WMMA instruction** per tile.
* **Tiling** (how you break the big matrix into 16×16 or 8×8 blocks) is chosen so that tiles fit in registers and shared memory and so that tensor cores stay busy.

**4.3 Data Flow (Simplified)**

1. **Load:** CUDA loads tiles of **A** and **B** from global/shared memory into registers (or shared memory for the tensor core to read).
2. **Compute:** Tensor core instruction: **D = A×B + C** on that tile.
3. **Store:** Result **D** is written back to shared memory or registers; then written to global memory or reused for the next layer.

Efficient kernels **fuse** steps (e.g. add bias, ReLU) in the same kernel so that result **D** is not written to global memory and read back — that saves bandwidth and improves performance.

---

**5. Precision Support: FP16, BF16, INT8, TF32**

Tensor cores support different **numeric formats**; each trades precision for throughput and power.

| Format | Bit width | Typical use | Throughput (vs FP32) | Orin Nano / Ampere |
|--------|-----------|-------------|------------------------|---------------------|
| **FP32** | 32-bit float | Training, reference | 1× (no tensor core) | CUDA cores only |
| **TF32** | 19-bit (mantissa truncated) | Training on Ampere+ | High | Ampere supports |
| **FP16** | 16-bit float | Inference, mixed precision | ~2× (tensor core) | ✅ Native |
| **BF16** | 16-bit (same exponent range as FP32) | Training, some inference | ~2× (tensor core) | ✅ Native |
| **INT8** | 8-bit integer | Quantized inference | ~4× (tensor core) | ✅ Native |
| **INT4** | 4-bit | Very low bit inference | Higher still | Architecture-dependent |

On **Orin Nano (Ampere)** for inference you care most about:

* **FP16** — Default for TensorRT and many models; tensor cores are used; good accuracy.
* **INT8** — Quantized models; 2× or more speedup over FP16 when calibrated correctly; slight accuracy loss.
* **FP32** — No tensor cores; runs on CUDA cores; slower, used for debugging or when precision is required.

TensorRT on Jetson will choose kernels that use tensor cores when you build an engine with `--fp16` or `--int8`. So “how tensor cores work” directly explains why FP16/INT8 engines are so much faster than FP32 on the same hardware.

---

**6. How Software Uses Tensor Cores (TensorRT, cuDNN)**

**6.1 TensorRT**

When you build a TensorRT engine (e.g. `trtexec --onnx=model.onnx --fp16`):

1. The **ONNX** (or other) graph is parsed and optimized.
2. Layers (e.g. `Gemm`, `Conv`) are **mapped to GPU kernels**. For linear/conv layers, TensorRT selects **tensor-core kernels** (FP16 or INT8) when available.
3. The engine is a **sequence of kernel launches**. Many of those kernels are **WMMA-based** or use NVIDIA’s internal tensor-core APIs so that the 32 tensor cores on Orin Nano are used for the bulk of the math.
4. **Fusion** (e.g. conv + bias + ReLU in one kernel) keeps data in registers/shared memory and reduces round-trips to global memory.

You don’t write tensor-core code by hand for inference; TensorRT (and cuDNN under the hood) do it. Understanding that they are using tensor cores explains the **40 TOPS** and the big gain from `--fp16` / `--int8`.

**6.2 cuDNN**

cuDNN provides **routines** for convolutions, matmuls, and other ops. On Ampere, these routines use **tensor-core implementations** when the problem size and precision match. PyTorch and other frameworks call cuDNN; TensorRT also uses cuDNN or its own tensor-core kernels. So “software uses tensor cores” means: **TensorRT and cuDNN (and thus most frameworks) automatically schedule tensor-core MMA instructions** when you use FP16/INT8.

</details>

### 6.3 编写自己的 kernel（WMMA、CUTLASS）

如果要编写自定义 CUDA：

* **WMMA（warp 矩阵乘累加）** — PTX/CUDA API 让一个 warp 在一条指令中完成一次小矩阵乘（如 16×16×16）。编译器会把它下沉为张量核心指令。
* **CUTLASS / CuTe** — NVIDIA 用于 GEMM 及相关运算的模板库；它显式地对张量核心 MMA 做分块与调度。在需要最大控制力时使用（如自定义形状、融合）。

在 Orin Nano 上，大多数用户依赖 TensorRT/cuDNN；这里深入展开是为了让你知道，启用 FP16/INT8 时 32 个张量核心上运行的**究竟是什么**。

---

## 7. 为什么这对边缘推理很重要

* **吞吐：** Orin Nano 8GB 上 **40 AI TOPS** 的大部分由张量核心提供。没有它们，同一颗芯片跑神经网络会慢得多。
* **功耗：** 每周期完成更多运算意味着同一工作负载更早结束，GPU 也能更早休眠 — 这对电池与热管理限制很重要。
* **精度选择：** FP16 和 INT8 是“张量核心路径”；FP32 是慢路径。选择 FP16（或经过校准的 INT8）才能同时获得速度与可接受的准确率。
* **调试：** 如果模型很慢，检查 engine 是否以 FP16/INT8 构建，以及各 layer 是否没有回退到 FP32 或非张量核心 kernel（如尺寸很小或形状怪异）。
* **DLA 与 GPU：** Orin Nano 还有 **DLA**（深度学习加速器）。DLA 是独立的固定功能模块；**张量核心**位于 **GPU** 内部。TensorRT 可以把某些 layer 跑在 DLA 上，另一些跑在 GPU 上（张量核心 + CUDA 核心）。因此“张量核心”= GPU 矩阵乘加速；“DLA”= 针对受支持运算的独立加速器。

---

## 8. 资源

* **NVIDIA Ampere 架构（白皮书）** — 张量核心的描述与框图。
* **NVIDIA CUDA Programming Guide** — WMMA API 与 warp 级矩阵运算。
* **TensorRT Developer Guide** — TensorRT 如何选择 kernel 以及如何使用 FP16/INT8。
* **Jetson Orin Nano datasheet / technical brief** — 官方的 SM 与张量核心数量以及 TOPS。
* **cuDNN Developer Guide** — 卷积与矩阵乘算法以及张量核心的使用。

---

*返回 [Nvidia Jetson Platform — Practical Complete Guide](../Guide.md)。*


<details>
<summary>English original</summary>

**6.3 Writing Your Own Kernels (WMMA, CUTLASS)**

If you write custom CUDA:

* **WMMA (Warp Matrix Multiply-Accumulate)** — PTX/CUDA APIs let a warp perform a small matrix multiply (e.g. 16×16×16) in one instruction. The compiler lowers this to tensor-core instructions.
* **CUTLASS / CuTe** — NVIDIA’s template library for GEMM and related ops; it explicitly tiles and schedules tensor-core MMAs. Used when you need maximum control (e.g. custom shapes, fusions).

On Orin Nano, most users rely on TensorRT/cuDNN; the deep dive here is so you know **what** is running on the 32 tensor cores when you enable FP16/INT8.

---

**7. Why This Matters for Edge Inference**

* **Throughput:** Tensor cores deliver most of the **40 AI TOPS** on Orin Nano 8GB. Without them, the same chip would be much slower on neural networks.
* **Power:** Doing more ops per cycle means the same workload finishes sooner and the GPU can sleep sooner — important for battery and thermal limits.
* **Precision choice:** FP16 and INT8 are the “tensor-core paths”; FP32 is the slow path. Picking FP16 (or INT8 with calibration) is how you get both speed and acceptable accuracy.
* **Debugging:** If a model is slow, check that the engine is built with FP16/INT8 and that layers are not falling back to FP32 or to non–tensor-core kernels (e.g. small or odd shapes).
* **DLA vs GPU:** Orin Nano also has a **DLA** (Deep Learning Accelerator). The DLA is a separate fixed-function block; the **tensor cores** are inside the **GPU**. TensorRT can run some layers on DLA and others on GPU (tensor cores + CUDA cores). So “tensor cores” = GPU matmul acceleration; “DLA” = separate accelerator for supported ops.

---

**8. Resources**

* **NVIDIA Ampere Architecture (white paper)** — Tensor core description and block diagrams.
* **NVIDIA CUDA Programming Guide** — WMMA API and warp-level matrix ops.
* **TensorRT Developer Guide** — How TensorRT selects kernels and uses FP16/INT8.
* **Jetson Orin Nano datasheet / technical brief** — Official SM and tensor core counts and TOPS.
* **cuDNN Developer Guide** — Convolution and matmul algorithms and tensor-core usage.

---

*Back to [Nvidia Jetson Platform — Practical Complete Guide](../Guide.md).*

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/1. Nvidia Jetson Platform/Orin-Nano-Tensor/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/1.%20Nvidia%20Jetson%20Platform/Orin-Nano-Tensor/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
