---
title: 模块 02 — 量化数学
description: 模块 02 — 量化数学
---

# 模块 02 — 量化数学

**合集：** [硬件感知的 LLM 量化](README.md) | **上一模块：** [← 模块 01](Lecture-01.md) | **下一模块：** [模块 03 →](Lecture-03.md)

---

[模块 01](Lecture-01.md) 确立了*为什么*更少的字节意味着每秒更多的 token。本模块讲的是一个数在被拿走若干比特之后究竟会发生什么——各种格式、让 4 bit 真正可用的缩放机制，以及一个激活值落到 NVFP4 网格上的精确算术。

低精度推理的核心事实：**一个 4-bit 浮点数的动态范围是 12×。** 不是 12 个数量级——而是十二倍。神经网络里没有任何东西能装进这个范围。让 FP4 能用的全部，就是套在它外面的缩放机制。

---

## 学习目标

读完本模块后，你应该能够：

1. 仅凭 `ExMy` 名称读懂任意浮点格式，并枚举其可表示的值。
2. 解释指数/尾数划分所编码的范围与精度权衡，以及为什么 BF16 在训练上胜过 FP16。
3. 说出 NVFP4 的完整编码——E2M1 + 16 元素块 + E4M3 块缩放 + FP32 全局缩放——并计算其**每权重有效 4.5 bit**。
4. **手工**量化与反量化一个块，并预测其中哪些值会被抹掉。
5. 解释为什么 NVFP4 的 E4M3 缩放优于 MXFP4 的 E8M0 缩放，以及块大小能换来什么。
6. 区分 W4A16 / W8A8 / W4A4，并指出硬件真正奖励的是哪一种。

---

## 1. 浮点格式是什么

这里每种格式都把自己的比特分成三份：

```text
   ┌───┬─────────────┬──────────────┐
   │ S │  exponent   │   mantissa   │
   └───┴─────────────┴──────────────┘
     1       E bits       M bits

   value = (−1)^S × (1 + mantissa/2^M) × 2^(exp − bias)        [normal]
   value = (−1)^S × (mantissa/2^M)     × 2^(1 − bias)          [subnormal, exp = 0]

   bias = 2^(E−1) − 1
```

这个划分就是全部的设计决策：

* **指数位 → 动态范围**（最大与最小可表示幅值之间相差多远）
* **尾数位 → 相对精度**（在一个 binade 内值的间隔有多细）

| 格式 | 位数 | E | M | 最大有限值 | 最小规格化值 | 动态范围 | 相对步长 |
|---|---:|---:|---:|---:|---:|---:|---:|
| FP32 | 32 | 8 | 23 | 3.4e38 | 1.2e−38 | ~2e76 | 1.2e−7 |
| **BF16** | 16 | 8 | 7 | 3.4e38 | 1.2e−38 | ~2e76 | 7.8e−3 |
| FP16 | 16 | 5 | 10 | 65504 | 6.1e−5 | ~1e9 | 9.8e−4 |
| **FP8 E4M3** | 8 | 4 | 3 | 448 | 1.6e−2 | ~2.9e4 | 6.3e−2 |
| FP8 E5M2 | 8 | 5 | 2 | 57344 | 6.1e−5 | ~9.4e8 | 1.3e−1 |
| **FP4 E2M1** | 4 | 2 | 1 | **6** | **1.0** | **12** | **2.5e−1** |

有两行解释了过去十年的实践：

**BF16 vs FP16。** 同样 16 个比特，选择却相反。BF16 保留 FP32 的 8 个指数位，只把 7 位花在尾数上——范围与 FP32 相同，精度更差。FP16 的相对精度好 3.5×，但在 65504 处溢出。训练梯度跨越极大的动态范围，且几乎不在意第 4 位有效数字，所以 BF16 胜出，loss scaling 基本变得不必要。**范围战胜了精度。**

**E2M1。** 再看最后一行。最大有限值 6，最小规格化值 1.0。这就是一个 4-bit 浮点数可表示的全部世界。

---

## 2. E2M1 全貌

四个比特，十六个编码，八个不同的幅值。没有无穷大也没有 NaN——每个编码都是一个有限数：

```text
   exp=00 (subnormal, ×2⁰)  :  0.0,  0.5
   exp=01 (×2⁰)             :  1.0,  1.5
   exp=10 (×2¹)             :  2.0,  3.0
   exp=11 (×2²)             :  4.0,  6.0

   full grid:  ±{ 0, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 6.0 }
```

画出来看，非均匀性正是关键所在：

```text
   0    0.5   1.0  1.5   2.0        3.0        4.0             6.0
   ├─────┼─────┼────┼─────┼──────────┼──────────┼───────────────┤
     0.5   0.5  0.5   0.5     1.0        1.0          2.0
                        ← gap size grows with magnitude →
```

间隔随幅值成比例增大——这正是浮点*存在的意义*。相对步长在整个范围内保持在 25 % 附近，而 INT4 网格（等间距）会在最大值附近给出很细的绝对分辨率，却在零附近造成灾难性的相对误差。**权重与激活值分布大致呈钟形、以零为中心，浮点网格恒定的相对误差与之更匹配。** 这是同等位宽下 FP4 优于 INT4 的根本原因，也是业界转向微缩放浮点格式、而不是把整数量化继续往下压的原因。

---


<details>
<summary>English original</summary>

**Module 02 — Quantization Mathematics**

**Collection:** [Hardware-Aware LLM Quantization](README.md) | **Previous:** [← Module 01](Lecture-01.md) | **Next:** [Module 03 →](Lecture-03.md)

---

[Module 01](Lecture-01.md) established *why* fewer bytes means more tokens per second. This module is about what actually happens to a number when you take its bits away — the formats, the scaling machinery that makes 4 bits usable at all, and the exact arithmetic of one activation landing on the NVFP4 grid.

The central fact of low-precision inference: **a 4-bit float has a dynamic range of 12×.** Not 12 orders of magnitude — a factor of twelve. Nothing in a neural network fits in that range. Everything that makes FP4 work is the scaling machinery wrapped around it.

---

**Learning objectives**

By the end of this module you should be able to:

1. Read any floating-point format from its `ExMy` name and enumerate its representable values.
2. Explain the range-vs-precision trade encoded in the exponent/mantissa split, and why BF16 beat FP16 for training.
3. State NVFP4's full encoding — E2M1 + 16-element blocks + E4M3 block scale + FP32 global scale — and compute its **effective 4.5 bits per weight**.
4. Quantize and dequantize a block **by hand**, and predict which values in it get annihilated.
5. Explain why NVFP4's E4M3 scale beats MXFP4's E8M0 scale, and what block size buys.
6. Distinguish W4A16 / W8A8 / W4A4 and say which one the hardware actually rewards.

---

**1. What a floating-point format is**

Every format here splits its bits three ways:

```text
   ┌───┬─────────────┬──────────────┐
   │ S │  exponent   │   mantissa   │
   └───┴─────────────┴──────────────┘
     1       E bits       M bits

   value = (−1)^S × (1 + mantissa/2^M) × 2^(exp − bias)        [normal]
   value = (−1)^S × (mantissa/2^M)     × 2^(1 − bias)          [subnormal, exp = 0]

   bias = 2^(E−1) − 1
```

The split is the entire design decision:

* **exponent bits → dynamic range** (how far apart the largest and smallest representable magnitudes are)
* **mantissa bits → relative precision** (how finely spaced values are within one binade)

| Format | Bits | E | M | Max finite | Min normal | Dynamic range | Relative step |
|---|---:|---:|---:|---:|---:|---:|---:|
| FP32 | 32 | 8 | 23 | 3.4e38 | 1.2e−38 | ~2e76 | 1.2e−7 |
| **BF16** | 16 | 8 | 7 | 3.4e38 | 1.2e−38 | ~2e76 | 7.8e−3 |
| FP16 | 16 | 5 | 10 | 65504 | 6.1e−5 | ~1e9 | 9.8e−4 |
| **FP8 E4M3** | 8 | 4 | 3 | 448 | 1.6e−2 | ~2.9e4 | 6.3e−2 |
| FP8 E5M2 | 8 | 5 | 2 | 57344 | 6.1e−5 | ~9.4e8 | 1.3e−1 |
| **FP4 E2M1** | 4 | 2 | 1 | **6** | **1.0** | **12** | **2.5e−1** |

Two rows explain a decade of practice:

**BF16 vs FP16.** Same 16 bits, opposite choices. BF16 keeps FP32's 8 exponent bits and spends only 7 on mantissa — same range as FP32, worse precision. FP16 has 3.5× better relative precision but overflows at 65504. Training gradients span enormous dynamic range and care little about the 4th significant digit, so BF16 won and loss scaling largely became unnecessary. **Range beat precision.**

**E2M1.** Look at the last row again. Max finite value 6, min normal 1.0. That is the entire representable universe of a 4-bit float.

---

**2. E2M1 in full**

Four bits, sixteen codes, eight distinct magnitudes. There are no infinities and no NaN — every code is a finite number:

```text
   exp=00 (subnormal, ×2⁰)  :  0.0,  0.5
   exp=01 (×2⁰)             :  1.0,  1.5
   exp=10 (×2¹)             :  2.0,  3.0
   exp=11 (×2²)             :  4.0,  6.0

   full grid:  ±{ 0, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 6.0 }
```

Plotted, the non-uniformity is the point:

```text
   0    0.5   1.0  1.5   2.0        3.0        4.0             6.0
   ├─────┼─────┼────┼─────┼──────────┼──────────┼───────────────┤
     0.5   0.5  0.5   0.5     1.0        1.0          2.0
                        ← gap size grows with magnitude →
```

Gaps grow proportionally to magnitude — that is exactly what a float is *for*. The relative step stays near 25 % across the range, whereas an INT4 grid (uniform spacing) would give fine absolute resolution near the maximum and catastrophic relative error near zero. **For weight and activation distributions, which are roughly bell-shaped and centered on zero, the float grid's constant relative error is the better match.** This is the core reason FP4 outperforms INT4 at equal bit-width, and it is why the industry moved to microscaling float formats rather than pushing integer quantization lower.

---

</details>

## 3. 块缩放——让 4 bit 可行的机制

12 的动态范围本身毫无用处。解法是在一小组数值旁边存一个**共享缩放因子**，让每一组都有自己在数轴上的窗口：

```text
   real value  ≈  scale_block  ×  code_E2M1        (code ∈ ±{0,0.5,1,1.5,2,3,4,6})
```

设计空间只有一个参数——**块大小**——它是一笔纯粹的取舍：

```text
   small blocks  →  each scale fits a tighter local range  →  less error
                 →  more scales stored                     →  more bits/element

   large blocks  →  fewer scales                           →  fewer bits/element
                 →  one outlier ruins a wider group        →  more error
```

每元素有效位数是精确的算术：

```text
   bits/elem  =  4  +  (bits_per_scale / block_size)
```

| 格式 | 块 | 缩放类型 | 缩放位数 | 有效位数/元素 |
|---|---:|---|---:|---:|
| **NVFP4** | 16 | FP8 **E4M3** | 8 | **4 + 8/16 = 4.50** |
| **MXFP4** (OCP) | 32 | **E8M0**（2 的幂） | 8 | 4 + 8/32 = 4.25 |

> **NVFP4 是每个权重 4.5 bit，不是 4。** 本课程中每一次容量与带宽计算都用 **0.5625 bytes/element**。用 0.5 会把你的检查点低估 12.5 %——而这个误差会直接传导到 [Module 01](Lecture-01.md) 的字节账本里。

### 为什么 E4M3 缩放因子优于 E8M0 缩放因子

这是两种格式之间最尖锐的区别，而它与块大小无关。

**E8M0** 是 8 个指数位、**零个尾数位**——它只能表示 2 的幂。你的块缩放因子必须取整到 `2^k`。假设某块的理想缩放因子是 3.95：E8M0 只能在 2.0 和 4.0 之间二选一。

**E4M3** 有 3 个尾数位，因此能表示 3.75、4.0、4.25……——它能以约 3 % 的误差落在 3.95 附近，而不是高达 41 %。

```text
   ideal block scale = 3.95

   E8M0 (MXFP4)  →  must pick 4.0 (or 2.0)  →  quantization grid is coarse-stepped
   E4M3 (NVFP4)  →  picks 4.0 exactly here, and 3.75 / 4.25 elsewhere
```

取整很差的缩放因子会浪费可表示的码字：缩放因子太大，块内数值会挤在低码字上；太小则会被截断。NVFP4 为每个元素多付 0.25 bit，换来一个能准确落点的缩放因子，外加一半大小的块。**它用少量容量换取显著更低的误差，在 4 bit 下这通常是取舍的正确一侧。**

### 两级缩放因子

实际中的 NVFP4 用的是*两*级缩放因子：

```text
   real ≈  global_scale_FP32  ×  block_scale_E4M3  ×  code_E2M1
           └── per tensor ──┘   └── per 16 elems ─┘  └─ per elem ─┘
```

FP32 全局缩放因子之所以存在，是因为 E4M3 本身最大只到 448。如果一个张量的块缩放因子会超过这个上限，全局缩放因子会把整个张量预归一化到范围内。它每个张量只花 4 bytes——在数值上等于免费——也正是它让 NVFP4 能处理块量级差异极大的张量。

---

## 4. 手工算一个块

这个练习能让格式变得具体。取一个 16 元素的激活值块，其绝对值最大值为 **23.7**，其中包含 `0.9`、`4.1` 和 `−11.2` 等值。

**第 1 步——选择块缩放因子。** 把 absmax 映射到最大的 E2M1 码字（6.0）：

```text
   s_raw = 23.7 / 6 = 3.95
```

**第 2 步——把缩放因子取整到 E4M3。** 在 3.95 附近，E4M3 可表示的值步长为 0.25（`2.0, 2.25, … 3.75, 4.0`）：

```text
   s = 4.0
```

**第 3 步——量化每个元素**（`x/s` → 最近的 E2M1 码字 → `× s`）：

| x | x / s | 最近码字 | 重建值 | 绝对误差 | 相对误差 |
|---:|---:|---:|---:|---:|---:|
| 23.70 | 5.9250 | 6.0 | 24.000 | +0.300 | 1.27 % |
| 4.10 | 1.0250 | 1.0 | 4.000 | −0.100 | 2.44 % |
| −11.20 | −2.8000 | −3.0 | −12.000 | −0.800 | 7.14 % |
| **0.90** | **0.2250** | **0.0** | **0.000** | **−0.900** | **100 %** |

看最后一行。`0.225` 距离 `0` 比距离 `0.5` 更近，所以**该值被抹掉——它精确地变成了零。**

```text
   block scale set by ONE large value (23.7)
                    │
                    ▼
   ├──────────────────────────────────────────────┤   representable window
   0                                            24.0
   ▲        ▲
   │        └─ smallest nonzero code = 0.5 × 4.0 = 2.0
   │
   └─ everything below 1.0 in real units rounds to ZERO

   0.9 is not "slightly wrong". It is GONE.
```

**仅这一个事实就驱动了 Module 05、06 和 07。** 一个块的缩放因子受制于它最大的那个元素。16 个元素中出现一个离群值，就会毁掉其余 15 个的分辨率。你之后会遇到的每一项技术——clipping、AWQ 的缩放因子迁移、SmoothQuant 的离群值转移、把敏感的 layer 保持为宽位宽——都是对这一个行为的回应。


<details>
<summary>English original</summary>

**3. Block scaling — the machinery that makes 4 bits work**

A dynamic range of 12 is useless on its own. The fix is to store a **shared scale** alongside a small group of values, so each group gets its own window into the number line:

```text
   real value  ≈  scale_block  ×  code_E2M1        (code ∈ ±{0,0.5,1,1.5,2,3,4,6})
```

The design space is one parameter — **block size** — and it is a pure trade:

```text
   small blocks  →  each scale fits a tighter local range  →  less error
                 →  more scales stored                     →  more bits/element

   large blocks  →  fewer scales                           →  fewer bits/element
                 →  one outlier ruins a wider group        →  more error
```

Effective bits per element is exact arithmetic:

```text
   bits/elem  =  4  +  (bits_per_scale / block_size)
```

| Format | Block | Scale type | Scale bits | Effective bits/elem |
|---|---:|---|---:|---:|
| **NVFP4** | 16 | FP8 **E4M3** | 8 | **4 + 8/16 = 4.50** |
| **MXFP4** (OCP) | 32 | **E8M0** (power-of-two) | 8 | 4 + 8/32 = 4.25 |

> **NVFP4 is 4.5 bits per weight, not 4.** Every capacity and bandwidth calculation in this course uses **0.5625 bytes/element**. Using 0.5 understates your checkpoint by 12.5 % — and that error propagates straight into the byte ledger from [Module 01](Lecture-01.md).

**Why E4M3 scales beat E8M0 scales**

This is the sharpest distinction between the two formats, and it is not about block size.

**E8M0** is 8 exponent bits and **zero mantissa bits** — it can only represent powers of two. Your block scale must be rounded to `2^k`. Suppose a block's ideal scale is 3.95: E8M0 must choose between 2.0 and 4.0.

**E4M3** has 3 mantissa bits, so it represents 3.75, 4.0, 4.25… — it can land near 3.95 with ~3 % error instead of up to 41 %.

```text
   ideal block scale = 3.95

   E8M0 (MXFP4)  →  must pick 4.0 (or 2.0)  →  quantization grid is coarse-stepped
   E4M3 (NVFP4)  →  picks 4.0 exactly here, and 3.75 / 4.25 elsewhere
```

A badly-rounded scale wastes representable codes: if the scale is too large the block's values crowd into the low codes; too small and they clip. NVFP4 pays 0.25 extra bits per element for a scale that lands accurately, plus a 2× smaller block. **It trades a little capacity for materially lower error, which is usually the right side of the trade at 4 bits.**

**The two-level scale**

NVFP4 in practice is *two* scales:

```text
   real ≈  global_scale_FP32  ×  block_scale_E4M3  ×  code_E2M1
           └── per tensor ──┘   └── per 16 elems ─┘  └─ per elem ─┘
```

The FP32 global scale exists because E4M3 itself tops out at 448. If a tensor's block scales would exceed that, the global scale pre-normalizes the whole tensor into range. It costs 4 bytes per tensor — numerically free — and it is why NVFP4 handles tensors with wildly varying block magnitudes.

---

**4. One block, by hand**

This is the exercise that makes the format concrete. Take a 16-element activation block whose absolute maximum is **23.7**, containing among others the values `0.9`, `4.1`, and `−11.2`.

**Step 1 — choose the block scale.** Map absmax onto the largest E2M1 code (6.0):

```text
   s_raw = 23.7 / 6 = 3.95
```

**Step 2 — round the scale to E4M3.** Near 3.95 the representable E4M3 values step by 0.25 (`2.0, 2.25, … 3.75, 4.0`):

```text
   s = 4.0
```

**Step 3 — quantize each element** (`x/s` → nearest E2M1 code → `× s`):

| x | x / s | nearest code | reconstructed | abs error | rel error |
|---:|---:|---:|---:|---:|---:|
| 23.70 | 5.9250 | 6.0 | 24.000 | +0.300 | 1.27 % |
| 4.10 | 1.0250 | 1.0 | 4.000 | −0.100 | 2.44 % |
| −11.20 | −2.8000 | −3.0 | −12.000 | −0.800 | 7.14 % |
| **0.90** | **0.2250** | **0.0** | **0.000** | **−0.900** | **100 %** |

Look at the last row. `0.225` is nearer to `0` than to `0.5`, so **the value is annihilated — it becomes exactly zero.**

```text
   block scale set by ONE large value (23.7)
                    │
                    ▼
   ├──────────────────────────────────────────────┤   representable window
   0                                            24.0
   ▲        ▲
   │        └─ smallest nonzero code = 0.5 × 4.0 = 2.0
   │
   └─ everything below 1.0 in real units rounds to ZERO

   0.9 is not "slightly wrong". It is GONE.
```

**This single fact drives Modules 05, 06, and 07.** A block's scale is hostage to its largest element. One outlier in a group of 16 destroys the resolution of the other 15. Every technique you will meet — clipping, AWQ's scale migration, SmoothQuant's outlier transfer, keeping sensitive layers wide — is an answer to this one behaviour.

</details>

### 裁剪的取舍，定量分析

最直接的修法是别再让离群值决定缩放因子。把 absmax 裁剪到 12.0 而不是 23.7（于是 `s = 2.0`），再对同一个 block 重跑一遍：

| x | 未裁剪 (s = 4.0) | **裁剪后 (s = 2.0)** |
|---:|---:|---:|
| 23.70 | 24.000 (1.27 %) | **12.000 (49.37 %)** ← 被裁得很狠 |
| 4.10 | 4.000 (2.44 %) | 4.000 (2.44 %) |
| −11.20 | −12.000 (7.14 %) | −12.000 (7.14 %) |
| **0.90** | **0.000 (100 %)** | **1.000 (11.11 %)** ← 被救回来 |

```text
   NO CLIPPING          :  outliers exact,  bulk annihilated
   AGGRESSIVE CLIPPING  :  outliers mangled, bulk preserved

   the optimum is in between, and it depends on which error the
   NEXT operator amplifies  ──────────▶  Modules 05 and 07
```

这里不存在放之四海皆准的答案——只有一个误差度量和一个搜索过程。那个搜索*就是*校准（[Module 05](Lecture-05.md)）。

---

## 5. 量化误差的正式表述

对一个标量 `x` 量化到 `Q(x)`，其误差为 `e = Q(x) − x`，可分解为两种行为截然不同的机制：

```text
   e_total  =  e_rounding   +   e_clipping
               │                 │
               │                 └── x fell outside [−s·6, +s·6]; grows without bound
               └── x landed between two codes; bounded by half the local gap
```

对步长为 `Δ` 的均匀量化器，舍入误差在 `[−Δ/2, +Δ/2]` 上近似均匀分布，由此得到经典结果：

```text
   σ²_round  =  Δ² / 12
```

即熟悉的 **每 bit 约 6.02 dB 的 SNR**。E2M1 不是均匀的，因此有用的表述要用相对量给出：在 1 个尾数位下，一个 binade 内最坏情况的相对舍入误差为 25 %，而一个缩放良好的 block 上的 RMS 相对误差落在 8–12 % 附近——*这是逐元素*的。

听起来很致命。其实不是，原因见下一节。

### 为什么 10 % 的逐元素误差不意味着模型错了 10 %

长度为 `K` 的点积累加 `K` 个近乎独立的误差。若逐元素误差零均值、标准差为 `σ`，则**和**的误差按 `√K` 增长，而**信号**按 `K` 增长：

```text
   relative error of the dot product  ≈  σ / √K
```

当 `K = 8192` 且 `σ = 0.10` 时：

```text
   0.10 / √8192  ≈  0.0011      →  ~0.11 % error on the output activation
```

**误差平均正是低比特推理能够成立的全部原因。** 有两条推论你会不断用到：

1. **零均值比误差小更重要。** 带有系统性偏置的量化器不会被平均掉——偏置随 `K` 线性累积，而不是按 `√K` 增长。这就是好的量化流水线中会出现随机舍入和偏置校正的原因。
2. **破坏平均假设的算子很危险。** softmax 不会把误差平均掉；它把误差指数放大。这正是 [Module 07](Lecture-07.md) 的主题，也是 Q/K 的行为与其他所有张量都不同的原因。

---

## 6. 比特去了哪里：W、A 和 KV

「4-bit 模型」这个说法有歧义。三个张量可以各自独立量化：

```text
                weights (W)          activations (A)         KV cache
   W4A16        4-bit  ◀── dequant to BF16 ──▶  16-bit       16-bit
   W8A8         8-bit                            8-bit        8-bit
   W4A4         4-bit                            4-bit       4/8-bit
```

| 方案 | 权重流量 | Tensor core 输入 | decode 收益 | 难度 |
|---|---|---|---|---|
| **W4A16** | 4× 更低 | BF16（权重上转换） | **收益的大部分** —— decode 受权重限制 | 容易；默认选择 |
| **W8A8** | 2× 更低 | FP8 原生 | 中等 | 中等 |
| **W4A4** | 4× 更低 | **FP4 原生** | 大部分 + 计算 | **难** —— 激活值有离群值 |

关于 batch-1 decode（逐 token 生成阶段）的关键洞见，直接来自 [Module 01](Lecture-01.md)：**权重占据绝对主导 `B_token`，激活值只是一个舍入误差。** 单个 token 的激活值只有几百 KB；权重约 16 GB。所以：

> 在 batch 1 下，**W4A16 拿下了几乎全部可得的吞吐收益。** W4A4 的额外收益在计算侧，而距离你需要它还有 250× 之遥。

当你 batch 做得足够大、接近 ridge point 时，或者当融合 kernel 里激活值一侧的访存流量开始变得重要时，W4A4 才值得付出那份难度。在单用户的 RTX 5090 上，**把激活值量化到 FP4 收益甚微、风险很大**——这个结论直接出自 roofline（性能上界模型），而不是出自任何实验。

这也解释了为什么激活值量化会单独占一个模块（[06](Lecture-06.md)）：在 batch 1 下它正是收益/风险比很差的那部分，因此你需要准确知道它何时才划算。

---


<details>
<summary>English original</summary>

**The clipping trade-off, quantified**

The obvious fix is to stop letting the outlier set the scale. Clip absmax to 12.0 instead of 23.7 (so `s = 2.0`) and re-run the same block:

| x | unclipped (s = 4.0) | **clipped (s = 2.0)** |
|---:|---:|---:|
| 23.70 | 24.000 (1.27 %) | **12.000 (49.37 %)** ← clipped hard |
| 4.10 | 4.000 (2.44 %) | 4.000 (2.44 %) |
| −11.20 | −12.000 (7.14 %) | −12.000 (7.14 %) |
| **0.90** | **0.000 (100 %)** | **1.000 (11.11 %)** ← rescued |

```text
   NO CLIPPING          :  outliers exact,  bulk annihilated
   AGGRESSIVE CLIPPING  :  outliers mangled, bulk preserved

   the optimum is in between, and it depends on which error the
   NEXT operator amplifies  ──────────▶  Modules 05 and 07
```

There is no universally right answer here — only an error metric and a search. That search *is* calibration ([Module 05](Lecture-05.md)).

---

**5. Quantization error, formally**

For a scalar `x` quantized to `Q(x)`, the error is `e = Q(x) − x`, and it decomposes into two mechanisms with completely different behavior:

```text
   e_total  =  e_rounding   +   e_clipping
               │                 │
               │                 └── x fell outside [−s·6, +s·6]; grows without bound
               └── x landed between two codes; bounded by half the local gap
```

For a uniform quantizer with step `Δ`, rounding error is approximately uniform on `[−Δ/2, +Δ/2]`, giving the classic result:

```text
   σ²_round  =  Δ² / 12
```

which yields the familiar **~6.02 dB of SNR per bit**. E2M1 is not uniform, so the useful statement is in relative terms: with 1 mantissa bit, the worst-case relative rounding error inside a binade is 25 %, and the RMS relative error across a well-scaled block lands around 8–12 % — *per element*.

That sounds fatal. It is not, and the reason is the next section.

**Why 10 % element-wise error does not mean a 10 % wrong model**

A dot product of length `K` sums `K` independent-ish errors. If per-element errors are zero-mean with standard deviation `σ`, the **sum's** error grows as `√K` while the **signal** grows as `K`:

```text
   relative error of the dot product  ≈  σ / √K
```

With `K = 8192` and `σ = 0.10`:

```text
   0.10 / √8192  ≈  0.0011      →  ~0.11 % error on the output activation
```

**Error averaging is the entire reason low-bit inference works.** Two corollaries you will use constantly:

1. **Zero-mean matters more than small.** A quantizer with a systematic bias does not average out — the bias accumulates linearly with `K`, not as `√K`. This is why stochastic rounding and bias correction appear in good quantization pipelines.
2. **Operators that break the averaging assumption are dangerous.** Softmax does not average errors; it exponentiates them. That is [Module 07](Lecture-07.md)'s subject and the reason Q/K behave unlike every other tensor.

---

**6. Where the bits go: W, A, and KV**

"4-bit model" is ambiguous. Three tensors can each be quantized independently:

```text
                weights (W)          activations (A)         KV cache
   W4A16        4-bit  ◀── dequant to BF16 ──▶  16-bit       16-bit
   W8A8         8-bit                            8-bit        8-bit
   W4A4         4-bit                            4-bit       4/8-bit
```

| Scheme | Weight traffic | Tensor-core input | Decode benefit | Difficulty |
|---|---|---|---|---|
| **W4A16** | 4× less | BF16 (weights upconverted) | **most of it** — decode is weight-bound | easy; the default |
| **W8A8** | 2× less | FP8 native | moderate | moderate |
| **W4A4** | 4× less | **FP4 native** | most + compute | **hard** — activations have outliers |

The key insight for batch-1 decode, straight from [Module 01](Lecture-01.md): **weights dominate `B_token`, activations are a rounding error.** A single token's activations are a few hundred KB; the weights are ~16 GB. So:

> At batch 1, **W4A16 captures nearly all of the available throughput win.** W4A4's extra benefit is compute-side, which you are 250× away from needing.

W4A4 becomes worth its difficulty when you are batching hard enough to approach the ridge point, or when the activation-side memory traffic in a fused kernel starts to matter. On a single-user RTX 5090, **quantizing activations to FP4 buys little and risks a lot** — a conclusion that falls directly out of the roofline, not out of any experiment.

This also explains why activation quantization gets its own module ([06](Lecture-06.md)): it is the part with a bad reward/risk ratio at batch 1, so you need to know precisely when it pays.

---

</details>

## 7. 格式速查表

一个 `8192 × 8192` 权重矩阵的存储占用（67.1 M 个元素）：

| 格式 | 字节/元素 | 矩阵大小 | 相对 BF16 |
|---|---:|---:|---:|
| BF16 | 2.0 | 134.2 MB | 1.00× |
| FP8 E4M3 | 1.0 | 67.1 MB | 2.00× |
| **NVFP4** | **0.5625** | **37.7 MB** | **3.56×** |
| MXFP4 | 0.5312 | 35.7 MB | 3.76× |
| INT4（group 128，FP16 scale） | 0.5156 | 34.6 MB | 3.88× |

注意 NVFP4 **并非**最小的——它是在 Blackwell 上每字节误差最优、*并且*具备原生执行路径的那一个。后半句正是 [Module 03](Lecture-03.md) 要讲的内容，也正是它让那些略小一点的替代方案落败。

---

## 检查点

现在你应当能够：

1. 凭记忆列举 E2M1 的全部 8 个幅值，并说出该格式的动态范围（12×）。
2. 计算 NVFP4 的有效 bits/element（4.5），并解释这两个词的含义。
3. 给定一个 block 的 absmax，预测其中哪些元素会被量化为零。
4. 解释为什么 MXFP4 的 E8M0 scale 在数值上比 NVFP4 的 E4M3 更差，尽管它占用的比特更少。
5. 解释为什么约 10 % 的逐元素误差只带来约 0.1 % 的输出误差，并指出会打破这一论证的算子。
6. 仅用 roofline（性能上界模型）论证，说明单用户 decode（逐 token 生成阶段）为何选择 W4A16 而非 W4A4。

---

## 交付

用 NumPy 或 PyTorch 实现 `quantize_nvfp4(tensor)` 和 `dequantize_nvfp4(...)`——参考语义，不用 CUDA：

```text
   1.  reshape to [..., n_blocks, 16]
   2.  global_scale = amax(tensor) / (6 × 448)          # keep block scales inside E4M3
   3.  block_scale  = amax(block) / 6 / global_scale  →  round to E4M3
   4.  codes        = round_to_nearest_E2M1(block / (block_scale × global_scale))
   5.  dequant      = codes × block_scale × global_scale
```

然后针对一个真实的权重张量报告：RMS 相对误差、**被清零元素的比例**，以及误差直方图。再用 block size 32 和 E8M0 scale 各重做一遍，亲手复现 NVFP4 与 MXFP4 之间的差距。那张表就是你的产物。

---

## 内容时效

* **长期有效：** IEEE 风格的浮点分解、block scaling 的算术、`Δ²/12`、`σ/√K` 平均论证、裁剪与舍入之间的权衡。
* **2026 年格式定版：** NVFP4 = E2M1 + block 16 + E4M3 block scale + FP32 per-tensor scale。MXFP4（OCP Microscaling）= E2M1 + block 32 + E8M0 scale。两者都是当前在用的；后者的参考规范是 OCP MX specification。
* **关注：** microscaling 的变体还在不断出现（6-bit MXFP6、混合 block size）。`4 + scale_bits/block_size` 算术可推广到所有变体。

---

**下一章：** [Module 03 — Blackwell Hardware →](Lecture-03.md)


<details>
<summary>English original</summary>

**7. Format cheat sheet**

Storage for one `8192 × 8192` weight matrix (67.1 M elements):

| Format | bytes/elem | Matrix size | vs BF16 |
|---|---:|---:|---:|
| BF16 | 2.0 | 134.2 MB | 1.00× |
| FP8 E4M3 | 1.0 | 67.1 MB | 2.00× |
| **NVFP4** | **0.5625** | **37.7 MB** | **3.56×** |
| MXFP4 | 0.5312 | 35.7 MB | 3.76× |
| INT4 (group 128, FP16 scale) | 0.5156 | 34.6 MB | 3.88× |

Note that NVFP4 is **not** the smallest — it is the one with the best error-per-byte on Blackwell *and* a native execution path. That second clause is what [Module 03](Lecture-03.md) is about, and it is what makes the marginally-smaller alternatives lose.

---

**Checkpoint**

You should now be able to:

1. Enumerate all 8 E2M1 magnitudes from memory and state the format's dynamic range (12×).
2. Compute NVFP4's effective bits/element (4.5) and explain both terms.
3. Predict, given a block's absmax, which of its elements will quantize to zero.
4. Explain why MXFP4's E8M0 scale is numerically worse than NVFP4's E4M3 despite costing fewer bits.
5. Explain why ~10 % per-element error yields ~0.1 % output error, and name the operator that breaks the argument.
6. Justify choosing W4A16 over W4A4 for single-user decode using a roofline argument alone.

---

**Ship it**

Implement `quantize_nvfp4(tensor)` and `dequantize_nvfp4(...)` in NumPy or PyTorch — reference semantics, no CUDA:

```text
   1.  reshape to [..., n_blocks, 16]
   2.  global_scale = amax(tensor) / (6 × 448)          # keep block scales inside E4M3
   3.  block_scale  = amax(block) / 6 / global_scale  →  round to E4M3
   4.  codes        = round_to_nearest_E2M1(block / (block_scale × global_scale))
   5.  dequant      = codes × block_scale × global_scale
```

Then report, for one real weight tensor: RMS relative error, **fraction of elements annihilated to zero**, and the error histogram. Repeat with block size 32 and with an E8M0 scale to reproduce the NVFP4-vs-MXFP4 gap yourself. That table is your artifact.

---

**Current as of**

* **Timeless:** IEEE-style float decomposition, block scaling arithmetic, `Δ²/12`, the `σ/√K` averaging argument, the clipping/rounding trade.
* **2026 format pins:** NVFP4 = E2M1 + block 16 + E4M3 block scale + FP32 per-tensor scale. MXFP4 (OCP Microscaling) = E2M1 + block 32 + E8M0 scale. Both are current; the OCP MX specification is the reference for the latter.
* **Watch:** microscaling variants continue to appear (6-bit MXFP6, mixed block sizes). The `4 + scale_bits/block_size` arithmetic generalizes to all of them.

---

**Next:** [Module 03 — Blackwell Hardware →](Lecture-03.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/Hardware-Aware LLM Quantization/Lecture-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/Hardware-Aware%20LLM%20Quantization/Lecture-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
