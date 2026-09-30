---
title: Part 1 · Lecture 03 — Roofline（性能上界模型）、带宽与存储层次
description: Part 1 · Lecture 03 — Roofline（性能上界模型）、带宽与存储层次
---

# Part 1 · Lecture 03 — Roofline（性能上界模型）、带宽与存储层次

## 概述

Lecture 02 已经确立：decode（逐 token 生成阶段）是带宽受限的，而 prefill（首字前的整段计算）是算力受限的。本讲将这些说法转化为一个定量模型——**roofline**——仅凭模型 + GPU 组合，就能预测工作负载中任意 kernel 的可达性能上限。

roofline 图回答一个具体问题：

> *给定一个 kernel 的算术强度（每从 HBM 读取一字节的 FLOPs），它在当前 GPU 上可达吞吐的上界是多少？*

答案是：**(intensity × bandwidth) 与 (peak FLOPs) 中的最小值**。该上限是**物理的**——在该硬件上，没有 kernel 能超过它。如果实测 kernel 远低于该上限，说明还有工程优化空间。如果已经达到上限，唯一的前进方向是**改变工作负载**（降低精度、更大的批、前缀缓存）或更换硬件。

本讲从第一性原理构建 roofline 模型：

1. Hopper 和 Blackwell 上的**存储层次**——每一层的开销是什么，以及工作集由什么承载。
2. **roofline 方程**以及如何计算任意 GPU 的 ridge point。
3. 针对 H100、H200、B200、Jetson Orin Nano Super 8 GB 的**roofline 演算**。
4. 如何**将 kernel 映射到 roofline 上**——测量算术强度，在图上定位，识别上限。
5. roofline 无法告诉你什么——以及其后的两层分析。

到本讲结束时，你应能根据数据手册为任意 GPU 绘制 roofline，将任何矩阵乘形状的 kernel 放到图上，并在运行前预测性能上限。

---

## 1. 存储层次

GPU 是一个**带宽金字塔**。越靠近 SM 意味着越快但越小。现代 Hopper / Blackwell SM 如下所示：

```text
                 ┌─────────────────────────────┐
                 │  Registers (per thread)     │  256 × 32-bit = ~1 KB / thread
                 │   ~10s of TB/s effective    │  total per SM: ~256 KB
                 └─────────────────────────────┘
                                │
                 ┌─────────────────────────────┐
                 │  Shared memory / L1 (per SM)│  H100/B200: ~228 KB usable per SM
                 │  ~25 TB/s aggregate         │
                 └─────────────────────────────┘
                                │
                 ┌─────────────────────────────┐
                 │  L2 cache (per GPU)         │  H100: 50 MB · B200: 60+ MB
                 │  ~5 TB/s effective          │
                 └─────────────────────────────┘
                                │
                 ┌─────────────────────────────┐
                 │  HBM (per GPU)              │  H100 80 GB HBM3 @ 3.35 TB/s
                 │                             │  H200 141 GB HBM3e @ 4.8 TB/s
                 │                             │  B200 192 GB HBM3e @ 8.0 TB/s
                 └─────────────────────────────┘
                                │
                 ┌─────────────────────────────┐
                 │  NVLink (cross-GPU)         │  H100 NVLink 4: 900 GB/s per GPU
                 │                             │  B200 NVLink 5: 1.8 TB/s per GPU
                 └─────────────────────────────┘
                                │
                 ┌─────────────────────────────┐
                 │  PCIe / fabric (cross-node) │  PCIe Gen5: 64 GB/s per direction
                 │                             │  IB NDR: 400 Gb/s · XDR: 800 Gb/s
                 └─────────────────────────────┘
```

在大语言模型推理中，每一层承载什么：

* **寄存器 + 共享内存**——矩阵乘期间的活动矩阵分块、正在做 softmax 的 attention 分数。SM 调度器将这些资源划分给各 warp。
* **L2 缓存**——最近访问过的激活值、seq 较短时当前 token 的 KV cache、频繁访问的权重分块。好的 kernel 会复用 L2。
* **HBM**——权重（数 GB）、KV cache（长上下文时达数 GB）、层与层之间的激活值。**这才是 roofline 关心的层级。**
* **NVLink**——张量并行中的跨 GPU 全规约流量，分离式 P/D 中的 KV 传输。
* **PCIe / IB**——跨节点。单副本推理服务不在关键路径上；分布式训练和大规模簇推理则在关键路径上。

roofline 方程假设 **HBM 是带宽瓶颈**。对于 batch=1 时 **>95% 的大语言模型推理 kernel**，该假设成立。

---


<details>
<summary>English original</summary>

**Part 1 · Lecture 03 — Roofline, Bandwidth, and the Memory Hierarchy**

**Overview**

Lecture 02 established that decode is bandwidth-bound and prefill is compute-bound. This lecture turns those statements into a quantitative model — the **roofline** — that lets you predict, from a model + GPU pair alone, what the achievable performance ceiling is for any kernel in your workload.

A roofline plot answers one specific question:

> *Given a kernel's arithmetic intensity (FLOPs per byte read from HBM), what is the upper bound on its achievable throughput on this GPU?*

The answer is: the **minimum of (intensity × bandwidth) and (peak FLOPs)**. That ceiling is **physical** — no kernel can exceed it on that hardware. If your measured kernel is far below the ceiling, you have engineering room. If it is at the ceiling, the only way forward is **changing the workload** (precision drop, larger batches, prefix cache) or the hardware.

This lecture builds the roofline model from first principles:

1. The **memory hierarchy** on Hopper and Blackwell — what each level costs and what holds the working set.
2. The **roofline equation** and how to compute the ridge point for any GPU.
3. **Worked rooflines** for H100, H200, B200, Jetson Orin Nano Super 8 GB.
4. How to **map a kernel onto a roofline** — measure arithmetic intensity, locate it on the chart, identify the ceiling.
5. What roofline cannot tell you — and the next two layers of analysis after it.

By the end you should be able to plot the roofline for any GPU from its datasheet, place any matmul-shaped kernel on it, and predict the performance ceiling before running it.

---

**1. The memory hierarchy**

A GPU is a **bandwidth pyramid**. Closer to the SM means faster but smaller. Modern Hopper / Blackwell SMs look like this:

```text
                 ┌─────────────────────────────┐
                 │  Registers (per thread)     │  256 × 32-bit = ~1 KB / thread
                 │   ~10s of TB/s effective    │  total per SM: ~256 KB
                 └─────────────────────────────┘
                                │
                 ┌─────────────────────────────┐
                 │  Shared memory / L1 (per SM)│  H100/B200: ~228 KB usable per SM
                 │  ~25 TB/s aggregate         │
                 └─────────────────────────────┘
                                │
                 ┌─────────────────────────────┐
                 │  L2 cache (per GPU)         │  H100: 50 MB · B200: 60+ MB
                 │  ~5 TB/s effective          │
                 └─────────────────────────────┘
                                │
                 ┌─────────────────────────────┐
                 │  HBM (per GPU)              │  H100 80 GB HBM3 @ 3.35 TB/s
                 │                             │  H200 141 GB HBM3e @ 4.8 TB/s
                 │                             │  B200 192 GB HBM3e @ 8.0 TB/s
                 └─────────────────────────────┘
                                │
                 ┌─────────────────────────────┐
                 │  NVLink (cross-GPU)         │  H100 NVLink 4: 900 GB/s per GPU
                 │                             │  B200 NVLink 5: 1.8 TB/s per GPU
                 └─────────────────────────────┘
                                │
                 ┌─────────────────────────────┐
                 │  PCIe / fabric (cross-node) │  PCIe Gen5: 64 GB/s per direction
                 │                             │  IB NDR: 400 Gb/s · XDR: 800 Gb/s
                 └─────────────────────────────┘
```

What lives at each level for LLM inference:

* **Registers + shared memory** — the active matrix tile during a matmul, attention scores being softmaxed. The SM scheduler partitions these between warps.
* **L2 cache** — recently-touched activations, KV cache for the current token if seq is short, frequently-accessed weight tiles. Good kernels reuse L2.
* **HBM** — the weights (gigabytes), the KV cache (gigabytes at long context), activations between layers. **This is the level the roofline cares about.**
* **NVLink** — cross-GPU all-reduce traffic in tensor parallelism, KV transfer in disaggregated P/D.
* **PCIe / IB** — cross-node. Off the critical path for single-replica serving; on the critical path for distributed training and large-cluster inference.

The roofline equation assumes **HBM is the bandwidth bottleneck**. That assumption is correct for **>95% of LLM inference kernels** at batch=1.

---

</details>

## 2. roofline 方程

roofline（性能上界模型）图有两个轴：

* **X 轴：算术强度**（每字节 HBM 读取对应的 FLOPs）
* **Y 轴：实测吞吐**（TFLOPs/s）

两条上限：

* **内存上限** — `throughput ≤ bandwidth × arithmetic_intensity`
* **算力上限** — `throughput ≤ peak_FLOPs`

**ridge point** 是两者相交之处：

```text
ridge_point = peak_FLOPs / bandwidth
```

对任何算术强度低于 ridge point 的 kernel：**带宽受限**（内存是上限）。
对任何高于 ridge point 的 kernel：**算力受限**（FLOPs 是上限）。

```text
   throughput
       ▲
peak ──┼───────────────────  ← compute ceiling (peak TFLOPs)
       │              ╱
       │           ╱       ← linear region (bandwidth × intensity)
       │        ╱
       │     ╱
       │  ╱
       │╱
       └────┴──────────────► arithmetic intensity (FLOPs/byte)
            ↑
         ridge point
```

对 LLM 推理，问题*永远*是：**这个 kernel 在这张图的什么位置？** 若远低于上限，就还有优化空间。若已到上限，唯一的进展是**改变 X 坐标**（通过批处理、融合或降精度提高算术强度）或**改变这张图**（换 GPU）。

---

## 3. roofline 演算实例

### 3.1 H100 SXM（80 GB HBM3）

* BF16/FP16 峰值 TFLOPs：989（张量核心）
* FP8 峰值 TFLOPs：1,979（张量核心）
* HBM3 带宽：3.35 TB/s

ridge point：

* FP16：989 × 10¹² / 3.35 × 10¹² = **~295 FLOPs/byte**
* FP8：1,979 × 10¹² / 3.35 × 10¹² = **~591 FLOPs/byte**

batch=1 下的一个 decode（逐 token 生成阶段）步骤，算术强度 ≈ 2/bytes_per_param = FP16 下约 1 FLOP/byte。这比 ridge point 低约 300 倍——**深度带宽受限**。

batch=1 下 4K-token prompt 的 prefill（首字前的整段计算），强度在数百 FLOPs/byte——达到或超过 ridge point，**算力受限**。

### 3.2 H200 SXM（141 GB HBM3e）

* BF16/FP16 峰值 TFLOPs：989（与 H100 相同）
* FP8 峰值 TFLOPs：1,979（相同）
* HBM3e 带宽：4.80 TB/s

ridge point：

* FP16：989 / 4.80 = **~206 FLOPs/byte**（比 H100 低约 30%）
* FP8：1,979 / 4.80 = **~412 FLOPs/byte**

H200 降低了 ridge point，因为带宽增长而 FLOPs 未增。相同精度下，**带宽受限的 kernel 在 H200 上比 H100 快 43%**。算力受限的 kernel 没有变化——H200 的优势在 **decode，而不在 prefill**。这就是 H200 适合 **chat 工作负载**、H100 适合 **embedding / 批处理**的原因。

### 3.3 B200 SXM（192 GB HBM3e）

* BF16 峰值 TFLOPs：2,250（张量核心，约 2.3× H100）
* FP8 峰值 TFLOPs：4,500（约 2.3× H100）
* FP4 峰值 TFLOPs：9,000（Transformer Engine 2，原生 FP4）
* HBM3e 带宽：8.0 TB/s

ridge point：

* FP16：2,250 / 8.0 = **~281 FLOPs/byte**
* FP8：4,500 / 8.0 = **~562 FLOPs/byte**
* FP4：9,000 / 8.0 = **~1,125 FLOPs/byte**

Blackwell 的 FP4 路径把 ridge point 大幅推高——意味着在 FP4 下，算力上限远高于大多数 kernel 所处的位置。**对带宽受限的 decode，FP4 的收益在 X 坐标**：因为每参数字节数减半，算术强度相对 FP8 翻倍。

### 3.4 Jetson Orin Nano Super 8 GB（边缘对照）

* BF16/FP16 峰值 TFLOPs：~17 稠密（Ampere 架构 SM 8.7 张量核心）
* INT8 峰值 TOPs：~33 稠密（NVIDIA 宣传的 "67 TOPS" 是稀疏 INT8）
* LPDDR5 带宽：~102 GB/s（0.1 TB/s）

ridge point：

* FP16：17 × 10¹² / 102 × 10⁹ = **~167 FLOPs/byte**
* INT8：33 × 10¹² / 102 × 10⁹ = **~324 OPs/byte**

注意 Orin Nano 的 FP16 ridge point 实际上*低于* Hopper 的 ~295——但这并不值得宽慰，因为真正的约束是 ~102 GB/s 的 LPDDR5 带宽，约为 H100 的 1/33。4B 级模型在 Orin Nano 上 batch=1 解码时会极其严重地撞上**带宽墙**（这正是 [Qwen Inference Optimization](../../../03-方向C-边缘AI/05-Qwen推理优化/README.md) 专题课程) 的全部出发点）。**批处理在边缘几乎从无帮助**，因为每个用户都是独立的。

### 3.5 跨 GPU 汇总

| GPU | FP16 峰值 | HBM 带宽 | FP16 ridge | FP8 ridge | FP4 ridge |
|-----|-----------|--------|------------|-----------|-----------|
| Jetson Orin Nano Super 8G | ~17 TFLOPs | 102 GB/s | ~167 | (INT8: ~324) | — |
| H100 SXM | 989 TFLOPs | 3.35 TB/s | 295 | 591 | — |
| H200 SXM | 989 TFLOPs | 4.80 TB/s | 206 | 412 | — |
| B200 SXM | 2,250 TFLOPs | 8.0 TB/s | 281 | 562 | 1,125 |

**要点：** ridge point 是（GPU, 精度）组合的属性。选择与 kernel 所处区间相匹配的精度。

---

## 4. 把 kernel 映射到 roofline

对你的工作负载中的每个 kernel：


<details>
<summary>English original</summary>

**2. The roofline equation**

A roofline plot has two axes:

* **X axis: arithmetic intensity** (FLOPs per byte read from HBM)
* **Y axis: achieved throughput** (TFLOPs/s)

Two ceilings:

* **Memory ceiling** — `throughput ≤ bandwidth × arithmetic_intensity`
* **Compute ceiling** — `throughput ≤ peak_FLOPs`

The **ridge point** is where they meet:

```text
ridge_point = peak_FLOPs / bandwidth
```

For any kernel with arithmetic intensity below the ridge point: **bandwidth-bound** (memory is the ceiling).
For any kernel above the ridge point: **compute-bound** (FLOPs are the ceiling).

```text
   throughput
       ▲
peak ──┼───────────────────  ← compute ceiling (peak TFLOPs)
       │              ╱
       │           ╱       ← linear region (bandwidth × intensity)
       │        ╱
       │     ╱
       │  ╱
       │╱
       └────┴──────────────► arithmetic intensity (FLOPs/byte)
            ↑
         ridge point
```

For LLM inference, the question is *always*: **where is this kernel on this chart?** If far below the ceiling, optimization is possible. If at the ceiling, the only progress is to **change the X coordinate** (raise arithmetic intensity by batching, fusion, or precision drop) or **the chart** (different GPU).

---

**3. Worked rooflines**

**3.1 H100 SXM (80 GB HBM3)**

* Peak BF16/FP16 TFLOPs: 989 (tensor cores)
* Peak FP8 TFLOPs: 1,979 (tensor cores)
* HBM3 bandwidth: 3.35 TB/s

Ridge points:

* FP16: 989 × 10¹² / 3.35 × 10¹² = **~295 FLOPs/byte**
* FP8: 1,979 × 10¹² / 3.35 × 10¹² = **~591 FLOPs/byte**

A decode step at batch=1 has arithmetic intensity ≈ 2/bytes_per_param = ~1 FLOP/byte at FP16. That is ~300× below the ridge — **deeply bandwidth-bound**.

A prefill of a 4K-token prompt with batch=1 has intensity in the hundreds of FLOPs/byte — at or above the ridge, **compute-bound**.

**3.2 H200 SXM (141 GB HBM3e)**

* Peak BF16/FP16 TFLOPs: 989 (unchanged from H100)
* Peak FP8 TFLOPs: 1,979 (unchanged)
* HBM3e bandwidth: 4.80 TB/s

Ridge points:

* FP16: 989 / 4.80 = **~206 FLOPs/byte** (~30% lower than H100)
* FP8: 1,979 / 4.80 = **~412 FLOPs/byte**

H200 lowers the ridge point because bandwidth grew without FLOPs growing. **Bandwidth-bound kernels are 43% faster on H200 than H100** at the same precision. Compute-bound kernels are unchanged — H200's win is **decode, not prefill**. This is why H200 is the right pick for **chat workloads** and H100 is the right pick for **embedding / batch**.

**3.3 B200 SXM (192 GB HBM3e)**

* Peak BF16 TFLOPs: 2,250 (tensor cores, ~2.3× H100)
* Peak FP8 TFLOPs: 4,500 (~2.3× H100)
* Peak FP4 TFLOPs: 9,000 (Transformer Engine 2, native FP4)
* HBM3e bandwidth: 8.0 TB/s

Ridge points:

* FP16: 2,250 / 8.0 = **~281 FLOPs/byte**
* FP8: 4,500 / 8.0 = **~562 FLOPs/byte**
* FP4: 9,000 / 8.0 = **~1,125 FLOPs/byte**

Blackwell's FP4 path moves the ridge point dramatically higher — meaning at FP4 the compute ceiling is far above where most kernels sit. **For bandwidth-bound decode, the FP4 win is in the X coordinate**: arithmetic intensity doubles vs FP8 because bytes per param halve.

**3.4 Jetson Orin Nano Super 8 GB (edge cross-reference)**

* Peak BF16/FP16 TFLOPs: ~17 dense (Ampere SM 8.7 tensor cores)
* Peak INT8 TOPs: ~33 dense (NVIDIA's headline "67 TOPS" is sparse INT8)
* LPDDR5 bandwidth: ~102 GB/s (0.1 TB/s)

Ridge points:

* FP16: 17 × 10¹² / 102 × 10⁹ = **~167 FLOPs/byte**
* INT8: 33 × 10¹² / 102 × 10⁹ = **~324 OPs/byte**

Notice the FP16 ridge point on Orin Nano is actually *below* Hopper's ~295 — but that is no comfort, because the binding constraint is the ~102 GB/s LPDDR5 bandwidth, roughly 1/33 of an H100's. A 4B-class model decoding at batch=1 on Orin Nano hits the **bandwidth wall** extremely hard (this is the entire premise of the [Qwen Inference Optimization](../../../03-方向C-边缘AI/05-Qwen推理优化/README.md) special course). **Batching almost never helps on edge** because each user is alone.

**3.5 Cross-GPU summary**

| GPU | Peak FP16 | HBM BW | FP16 Ridge | FP8 Ridge | FP4 Ridge |
|-----|-----------|--------|------------|-----------|-----------|
| Jetson Orin Nano Super 8G | ~17 TFLOPs | 102 GB/s | ~167 | (INT8: ~324) | — |
| H100 SXM | 989 TFLOPs | 3.35 TB/s | 295 | 591 | — |
| H200 SXM | 989 TFLOPs | 4.80 TB/s | 206 | 412 | — |
| B200 SXM | 2,250 TFLOPs | 8.0 TB/s | 281 | 562 | 1,125 |

**The takeaway:** the ridge point is a property of the (GPU, precision) pair. Choose precision to match the regime your kernel is in.

---

**4. Mapping a kernel onto a roofline**

For each kernel in your workload:

</details>

### 4.1 计算其算术强度

```text
intensity = FLOPs_per_call / bytes_read_from_HBM_per_call
```

对于一个在 `M × K + K × N + M × N` 矩阵上进行的 `(M, K) @ (K, N)` 矩阵乘，使用 FP16：

```text
FLOPs       = 2 × M × N × K
bytes       = 2 × (M × K + K × N + M × N)   [if everything streams from HBM]
intensity   = M × N × K / (M × K + K × N + M × N)
```

对于 “fat” 矩阵乘（M、N、K 都很大）：算术强度 ≈ MNK / (MK + KN + MN) → 随矩阵规模增长。
对于 “skinny” 矩阵乘（decode，逐 token 生成阶段；M=1）：算术强度 ≈ K × N / (K + KN + N) ≈ 1（FP16 下）。**带宽受限。**

### 4.2 在图上定位它

在 kernel 的算术强度处画一条垂直线。可达到的上限是这条线与两个上限中较低者的交点：

* 如果算术强度 < ridge：上限 = 带宽 × 算术强度（带宽受限）
* 如果算术强度 > ridge：上限 = 峰值 FLOPs（算力受限）

### 4.3 与实测比较

运行 kernel，测量达到的 TFLOPs/s。比值（**achieved / ceiling**）就是**效率**。

* > 80%：kernel 优化良好；进一步工作必须改变 kernel 的算术强度（降低精度、批处理、融合）。
* 30–80%：kernel 还有改进空间 — 内存访问模式差、未使用 Tensor Memory Accelerator、occupancy 不足。
* < 30%：通常是结构性问题 — kernel 启动之间的 Python 开销、没有 CUDA Graph、kernel 启动延迟、精度错误。

### 4.4 roofline（性能上界模型）*没有*捕捉到的两种失效模式

roofline 必要但不充分：

1. **调度器 / Python 开销** — kernel 启动之间存在 CPU 时间。roofline 假设 100% 为 kernel 时间。对于非常短的 decode（<1 ms），Python 循环可能占据主导。
2. **warp 执行效率** — 即使 occupancy 很高，分支发散或内存访问模式也可能意味着每个 warp 做的实际工作更少。Nsight Compute 用 `sm__sass_thread_inst_executed_per_inst_executed.ratio` 表示这一点。

如果你的 kernel 效率为 50%，而 roofline 说应该达到 80%，在尝试重写 kernel 之前，运行 Nsight Compute 并检查这两项。

---

## 5. roofline 心智检查

熟悉 roofline 的工程师在做出任何改动前会问的五个问题：

1. **我的 kernel 的算术强度是多少？** 先从数学计算，而不是先测量。
2. **在这个 GPU 上、这个精度下，ridge 点在哪里？**
3. **我的 kernel 在 ridge 之上还是之下？** 这告诉我正面对哪个上限。
4. **相对于上限，我测得的效率是多少？**
5. **四个杠杆中哪一个**能移动它：精度（改变算术强度）、批处理（改变算术强度）、融合（改变字节数）、GPU（改变上限）？

问题 5 的答案就是本课程第 2 部分和第 3 部分的全部内容。

---

## 实验 — 为目标 GPU 和三个 kernel 绘制 roofline

目标：生成一张图表，提交到你的 benchmark 仓库，并在上面标出三个点。

1. **选择一个目标 GPU** — H100、H200，或你手头的任何 GPU。
2. 为该 GPU **绘制 FP16、FP8 和 INT4 下的 roofline**。使用数据手册上的数字。
3. **从你的 benchmark 中选三个 kernel**：
   * 一个 4B 级模型上 batch=1 的 decode 步骤。
   * 同一模型上 2K token 的 prefill（首字前的整段计算）步骤。
   * batch=64 时的 FFN gate+up 矩阵乘（decode 场景，更大的批）。
4. **测量**每个 kernel 达到的 FLOPs/sec 和 HBM 读取字节数（Nsight Compute → `gpu__time_duration` + `dram__bytes`）。计算算术强度和达到的 TFLOPs。
5. **标出** roofline 上的三个点。标注每个点受哪个上限约束。

通过标准：图表放入你的 benchmark 报告。任何阅读它的人都能理解，在这个 GPU 上哪些 kernel 是带宽受限的，哪些是算力受限的。

---

## 自检

1. H200 与 H100 具有相同的峰值 FP16 TFLOPs，但 HBM 带宽高 43%。对于一个 80% decode（带宽受限）和 20% prefill（算力受限）的工作负载，H200 相对于 H100 的预期端到端加速比是多少？简要推导数学。
2. 你在 H200 上运行一个 decode kernel，测得 HBM 读取为 480 GB/s（峰值 4.8 TB/s）。你的带宽效率是多少？kernel 处于这个水平而不是 80%+ 的两个可能原因是什么？
3. 一位同事提议将 70B 级 decode 的批大小从 64 加倍到 128。roofline 表明该 kernel 在 batch=64 时处于算力上限。预测吞吐变化。用一句话说明理由。
4. 为什么 B200 上的 FP4 ridge 点（约 1125 FLOPs/byte）感觉“高得永远无法达到”？实际上什么样的工作负载形状能达到它？
5. 在 Jetson Orin Nano Super 8 GB 上，INT8 下的 ridge 点约为 324 OPs/byte。一个在 batch=1 下 decode 的 Qwen3-4B 的算术强度约为 2。仅根据 roofline，tokens/sec 的上限是多少？什么单一改动（不改变硬件）能最接近它？


<details>
<summary>English original</summary>

**4.1 Compute its arithmetic intensity**

```text
intensity = FLOPs_per_call / bytes_read_from_HBM_per_call
```

For a `(M, K) @ (K, N)` matmul on `M × K + K × N + M × N` matrix in FP16:

```text
FLOPs       = 2 × M × N × K
bytes       = 2 × (M × K + K × N + M × N)   [if everything streams from HBM]
intensity   = M × N × K / (M × K + K × N + M × N)
```

For a "fat" matmul (M, N, K all large): intensity ≈ MNK / (MK + KN + MN) → grows with the matrix size.
For a "skinny" matmul (decode, M=1): intensity ≈ K × N / (K + KN + N) ≈ 1 in FP16. **Bandwidth-bound.**

**4.2 Locate it on the chart**

Draw a vertical line at the kernel's intensity. The achievable ceiling is where that line meets the lower of the two ceilings:

* If intensity < ridge: ceiling = bandwidth × intensity (bandwidth-bound)
* If intensity > ridge: ceiling = peak FLOPs (compute-bound)

**4.3 Compare to measured**

Run the kernel, measure achieved TFLOPs/s. The ratio (**achieved / ceiling**) is the **efficiency**.

* > 80%: the kernel is well-optimized; further work must change the kernel's intensity (precision drop, batching, fusion).
* 30–80%: room for kernel improvement — bad memory access pattern, no Tensor Memory Accelerator usage, insufficient occupancy.
* < 30%: usually something structural — Python overhead between launches, no CUDA Graph, kernel launch latency, wrong precision.

**4.4 The two failure modes the roofline does *not* catch**

Roofline is necessary but not sufficient:

1. **Scheduler / Python overhead** — between kernel launches there is CPU time. Roofline assumes 100% kernel time. For very short decodes (<1 ms), the Python loop can dominate.
2. **Warp execution efficiency** — even at high occupancy, divergent branches or memory access patterns can mean each warp does less actual work. Nsight Compute gives this as `sm__sass_thread_inst_executed_per_inst_executed.ratio`.

If your kernel is at 50% efficiency and the roofline says you should be at 80%, run Nsight Compute and check both of these before trying to rewrite the kernel.

---

**5. The roofline mental check**

Five questions a roofline-literate engineer asks before changing anything:

1. **What is my kernel's arithmetic intensity?** Compute it from the math, not measurement first.
2. **Where is the ridge point on this GPU at this precision?**
3. **Is my kernel above or below the ridge?** This tells me which ceiling I am up against.
4. **What is my measured efficiency vs the ceiling?**
5. **Which of the four levers** moves it: precision (changes intensity), batching (changes intensity), fusion (changes bytes), GPU (changes ceiling)?

The answer to question 5 is the entire content of Parts 2 and 3 of this course.

---

**Lab — plot the roofline for your target GPU and three kernels**

Goal: produce a single chart, committed to your benchmark repo, with three points on it.

1. **Pick a target GPU** — H100, H200, or whatever you have.
2. **Draw the roofline at FP16, FP8, and INT4** for that GPU. Use the datasheet numbers.
3. **Pick three kernels** from your benchmark:
   * A decode step at batch=1 on a 4B-class model.
   * A prefill step at 2K tokens on the same model.
   * The FFN gate+up matmul at batch=64 (decode regime, larger batch).
4. **Measure** each kernel's FLOPs/sec achieved and HBM bytes read (Nsight Compute → `gpu__time_duration` + `dram__bytes`). Compute arithmetic intensity and achieved TFLOPs.
5. **Plot** the three points on the roofline. Annotate which ceiling each one is bound by.

Pass criterion: the chart goes in your benchmark report. Anyone reading it understands which kernels are bandwidth-bound and which are compute-bound on this GPU.

---

**Self-check**

1. The H200 has the same peak FP16 TFLOPs as the H100 but 43% more HBM bandwidth. For a workload that is 80% decode (bandwidth-bound) and 20% prefill (compute-bound), what is the expected end-to-end speedup of H200 over H100? Sketch the math.
2. You run a decode kernel and measure 480 GB/s of HBM read on an H200 (4.8 TB/s peak). What is your bandwidth efficiency? What are two likely reasons the kernel is at this level instead of 80%+?
3. A teammate proposes doubling batch size from 64 to 128 for a 70B-class decode. The roofline says the kernel is at the compute ceiling at batch=64. Predict the throughput change. Defend in one sentence.
4. Why does the FP4 ridge point on B200 (~1125 FLOPs/byte) feel "too high to ever hit"? What workload shape would actually reach it?
5. On Jetson Orin Nano Super 8 GB at INT8 the ridge point is ~324 OPs/byte. A Qwen3-4B decoded at batch=1 has intensity ~2. What is the upper bound on tokens/sec from the roofline alone? What single change (without changing hardware) gets closest to it?

---

</details>

## 参考文献

* "Roofline: An Insightful Visual Performance Model for Multicore Architectures" — Williams et al., 2009 — 原始论文，至今仍是权威
* NVIDIA H100 架构白皮书 — [nvidia.com/en-us/data-center/h100/](https://www.nvidia.com/en-us/data-center/h100/)
* NVIDIA H200 产品页 — [nvidia.com/en-us/data-center/h200/](https://www.nvidia.com/en-us/data-center/h200/)
* NVIDIA B200 / Blackwell 白皮书 — [nvidia.com/en-us/data-center/technologies/blackwell-architecture/](https://www.nvidia.com/en-us/data-center/technologies/blackwell-architecture/)
* "Liger Kernel: Efficient Triton Kernels for LLM Training" — 融合 kernel 的实用读物 — [arXiv:2410.10989](https://arxiv.org/abs/2410.10989)
* "FlashAttention: Fast and Memory-Efficient Exact Attention with IO-Awareness" — [arXiv:2205.14135](https://arxiv.org/abs/2205.14135) — IO-aware ≈ roofline-aware

交叉引用：

* [阶段 5 → GPU 基础设施 → Blackwell-B200-Qwen-Inference → 01 Blackwell 架构](../../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/02-Blackwell-B200-Qwen推理/01-Blackwell-Architecture.md)
* [阶段 5 → GPU 基础设施 → CUDA 高级优化 → 04 kernel 融合](../../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/03-CUDA高级优化/04-Kernel-Fusion.md)

---

## 内容截至 2026-06

GPU 峰值数据来自 NVIDIA 公布的 H100、H200、B200 SXM 数据手册。若 NVIDIA 发布修正后的峰值，或有新的精度模式推出（FP6、FP3），需更新。

---

## 接下来

* 下一讲：[第 04 讲 — 精度栈，FP16 → FP8 → FP4 → INT4](Lecture-04.md)
* 上一讲：[第 02 讲 — Transformer 的执行过程，从 token 到比特](Lecture-02.md)
* 上一级：[第 1 部分 — 基础](README.md)


<details>
<summary>English original</summary>

**References**

* "Roofline: An Insightful Visual Performance Model for Multicore Architectures" — Williams et al., 2009 — the original paper, still authoritative
* NVIDIA H100 architecture whitepaper — [nvidia.com/en-us/data-center/h100/](https://www.nvidia.com/en-us/data-center/h100/)
* NVIDIA H200 product page — [nvidia.com/en-us/data-center/h200/](https://www.nvidia.com/en-us/data-center/h200/)
* NVIDIA B200 / Blackwell whitepaper — [nvidia.com/en-us/data-center/technologies/blackwell-architecture/](https://www.nvidia.com/en-us/data-center/technologies/blackwell-architecture/)
* "Liger Kernel: Efficient Triton Kernels for LLM Training" — practical fused-kernel reading — [arXiv:2410.10989](https://arxiv.org/abs/2410.10989)
* "FlashAttention: Fast and Memory-Efficient Exact Attention with IO-Awareness" — [arXiv:2205.14135](https://arxiv.org/abs/2205.14135) — IO-aware ≈ roofline-aware

Cross-references:

* [Phase 5 → GPU Infrastructure → Blackwell-B200-Qwen-Inference → 01 Blackwell Architecture](../../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/02-Blackwell-B200-Qwen推理/01-Blackwell-Architecture.md)
* [Phase 5 → GPU Infrastructure → CUDA Advanced Optimization → 04 Kernel Fusion](../../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/03-CUDA高级优化/04-Kernel-Fusion.md)

---

**Current as of 2026-06**

GPU peak numbers from NVIDIA's published datasheets for H100, H200, B200 SXM. Update if NVIDIA releases corrected peaks or if new precision modes ship (FP6, FP3).

---

**Next**

* Next: [Lecture 04 — The precision stack, FP16 → FP8 → FP4 → INT4](Lecture-04.md)
* Previous: [Lecture 02 — Transformer execution, from tokens to bits](Lecture-02.md)
* Up: [Part 1 — Fundamentals](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 1 - Fundamentals/Lecture-03.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%201%20-%20Fundamentals/Lecture-03.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
