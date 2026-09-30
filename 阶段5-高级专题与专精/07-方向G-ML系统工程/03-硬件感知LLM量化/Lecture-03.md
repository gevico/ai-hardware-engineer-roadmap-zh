---
title: 模块 03 — Blackwell 硬件：sm120 究竟加速了什么
description: 模块 03 — Blackwell 硬件：sm120 究竟加速了什么
---

# 模块 03 — Blackwell 硬件：`sm_120` 究竟加速了什么

**合集：** [Hardware-Aware LLM Quantization](README.md) | **上一模块：** [← 模块 02](Lecture-02.md) | **下一模块：** [模块 04 →](Lecture-04.md)

---

[模块 02](Lecture-02.md) 把格式当作数学来对待。本模块把它们当作 **指令**。只有当硅片能不绕路地消费某种格式时，它才算快，而“更少的 bit”与“更快”之间的落差，正是大多数量化项目悄悄翻车的地方。

本模块的总纲：

> **无法喂给 Tensor Core 的位宽，就是你在模拟的位宽。** 模拟要付出对齐、issue slot 和原生 MMA 路径的代价 —— 而它通常比省下的字节代价更高。

---

## 学习目标

学完本模块后，你应该能够：

1. 描述 RTX 5090 的存储系统与计算 roofline（性能上界模型），并推导各精度的脊点。
2. 区分 **消费级 Blackwell（`sm_120`）**与 **数据中心 Blackwell（`sm_100`）**的 Tensor Core 路径，并说出各自使用的指令族。
3. 解释为什么奇数位宽（3-bit、5-bit）即便减少了标称字节数，仍会损失*有效带宽*。
4. 计算非原生格式的**盈亏平衡实测带宽阈值**。
5. 读懂 Nsight Compute 报告，确认自己处在快路径上，而不是 dequant 路径上。

---

## 1. 机器

```text
   NVIDIA GeForce RTX 5090  —  GB202, compute capability 12.0 (sm_120)

   ┌──────────────────────────────────────────────────────────────┐
   │  170 SMs  ×  128 FP32 lanes  =  21,760 CUDA cores            │
   │  5th-generation Tensor Cores (FP4 / FP6 / FP8 / BF16 / FP16) │
   │  boost ~2.41 GHz                                             │
   ├──────────────────────────────────────────────────────────────┤
   │  L2 cache: tens of MB  (irrelevant here — see §2)             │
   ├──────────────────────────────────────────────────────────────┤
   │  32 GB GDDR7, 512-bit bus @ 28 Gbps  →  1792 GB/s            │
   └──────────────────────────────────────────────────────────────┘
```

稠密 Tensor Core 吞吐大致随精度每降一档翻倍：

| 精度 | 稠密 TFLOP/s（约） | 脊点（FLOP/byte） |
|---|---:|---:|
| FP32（shader） | 105 | 59 |
| BF16 / FP16 | 419 | 234 |
| FP8（E4M3/E5M2） | 838 | 468 |
| **NVFP4** | **1676** | **935** |

NVIDIA 为这颗芯片打出的头条数字「**3352 AI TOPS**」，是**带 2:4 结构化稀疏的 FP4**。稠密 FP4 只有它的一半。稀疏要求模型经过剪枝、强制 2:4 模式，还需要稀疏感知的 kernel；如果你没有刻意做过这项工作，适用于你的数字就是 1676，而在 roofline 里引用 3352，会让你以为自己拥有的算力余量是实际的两倍。

---

## 2. 为什么 L2 缓存救不了你

按 GPU 的标准，GB202 的 L2 很大 —— 有几十 MB。你的 decode（逐 token 生成阶段）工作集是 **~16 GB 的权重，每个 token 恰好流过一次。**

```text
   working set per token   ≈  15.9 GB
   L2 capacity              ≈  0.1 GB
   reuse within one token   =  ~1×      (each weight read once, used once)
   reuse across tokens      =  0×       (evicted long before the next token needs it)
```

**权重流上的缓存命中率基本为零，再多的 L2 也修不好这一点。** 这就是 [模块 01](Lecture-01.md) 中的 roofline 直接使用 HBM/GDDR 带宽、不做任何缓存修正的原因 —— 对 batch-1 的 LLM decode 而言，缓存层级只是个旁观者。

两个实际后果：

* 减少权重流量的唯一办法就是把权重做得*更小*。没有任何局部性技巧可用。
* KV cache **确实**小到能在短上下文下受益于 L2，这既是短上下文 decode 略优于朴素 roofline 的原因之一，也是该优势随上下文增长而消失的原因（[模块 09](Lecture-09.md)）。

---


<details>
<summary>English original</summary>

**Module 03 — Blackwell Hardware: What `sm_120` Actually Accelerates**

**Collection:** [Hardware-Aware LLM Quantization](README.md) | **Previous:** [← Module 02](Lecture-02.md) | **Next:** [Module 04 →](Lecture-04.md)

---

[Module 02](Lecture-02.md) treated formats as mathematics. This module treats them as **instructions**. A format is only fast if the silicon can consume it without a detour, and the gap between "fewer bits" and "faster" is where most quantization projects quietly fail.

The governing rule of this module:

> **A bit-width you cannot feed to a tensor core is a bit-width you are emulating.** Emulation costs alignment, issue slots, and the native MMA path — and it usually costs more than the bytes it saved.

---

**Learning objectives**

By the end of this module you should be able to:

1. Describe the RTX 5090's memory system and compute roofline, and derive ridge points per precision.
2. Distinguish the **consumer Blackwell (`sm_120`)** tensor-core path from the **datacenter Blackwell (`sm_100`)** one, and name the instruction family each uses.
3. Explain why odd bit-widths (3-bit, 5-bit) lose *effective bandwidth* even when they reduce nominal bytes.
4. Compute the **break-even achieved-bandwidth threshold** for a non-native format.
5. Read a Nsight Compute report and confirm you are on the fast path rather than a dequant path.

---

**1. The machine**

```text
   NVIDIA GeForce RTX 5090  —  GB202, compute capability 12.0 (sm_120)

   ┌──────────────────────────────────────────────────────────────┐
   │  170 SMs  ×  128 FP32 lanes  =  21,760 CUDA cores            │
   │  5th-generation Tensor Cores (FP4 / FP6 / FP8 / BF16 / FP16) │
   │  boost ~2.41 GHz                                             │
   ├──────────────────────────────────────────────────────────────┤
   │  L2 cache: tens of MB  (irrelevant here — see §2)             │
   ├──────────────────────────────────────────────────────────────┤
   │  32 GB GDDR7, 512-bit bus @ 28 Gbps  →  1792 GB/s            │
   └──────────────────────────────────────────────────────────────┘
```

Dense tensor-core throughput roughly doubles per precision step:

| Precision | Dense TFLOP/s (approx.) | Ridge point (FLOP/byte) |
|---|---:|---:|
| FP32 (shader) | 105 | 59 |
| BF16 / FP16 | 419 | 234 |
| FP8 (E4M3/E5M2) | 838 | 468 |
| **NVFP4** | **1676** | **935** |

NVIDIA's headline "**3352 AI TOPS**" for this part is **FP4 with 2:4 structured sparsity**. Dense FP4 is half that. Sparsity requires a pruned model with the 2:4 pattern enforced and a sparsity-aware kernel; if you have not deliberately done that work, the number that applies to you is 1676, and quoting 3352 in a roofline will make you think you have 2× more compute headroom than you do.

---

**2. Why the L2 cache does not save you**

GB202's L2 is large by GPU standards — tens of megabytes. Your decode working set is **~16 GB of weights, streamed exactly once per token.**

```text
   working set per token   ≈  15.9 GB
   L2 capacity              ≈  0.1 GB
   reuse within one token   =  ~1×      (each weight read once, used once)
   reuse across tokens      =  0×       (evicted long before the next token needs it)
```

**Cache hit rate on the weight stream is essentially zero, and no amount of L2 fixes that.** This is why the roofline in [Module 01](Lecture-01.md) uses HBM/GDDR bandwidth directly with no cache correction — for batch-1 LLM decode, the cache hierarchy is a bystander.

Two practical consequences:

* The only way to reduce weight traffic is to make the weights *smaller*. There is no locality trick available.
* The KV cache **is** small enough to benefit from L2 at short context, which is one reason short-context decode outperforms the naive roofline slightly, and why that advantage evaporates as context grows ([Module 09](Lecture-09.md)).

---

</details>

## 3. 两个 Blackwell，两套张量核心编程模型

这是当前 Blackwell 材料中被错误归因最多的一条事实，因此要精确陈述：

```text
   HOPPER  (sm_90)     :  wgmma      — warpgroup MMA, async, operands in shared memory
   BLACKWELL datacenter
           (sm_100)    :  tcgen05    — 5th-gen MMA + Tensor Memory (TMEM), B200 / GB200
   BLACKWELL consumer
           (sm_120)    :  mma.sync   — warp-level MMA with BLOCK-SCALED FP4/FP8 operands
                                        RTX 50-series / GB202. No TMEM, no tcgen05.
```

两个 Blackwell 变体都能原生执行 NVFP4，但**通过不同的指令、有不同的分块要求**，这意味着：

* 为 B200（`sm_100a`）调优的 kernel **并不能**简单地重新编译到 RTX 5090。
* CUTLASS 为这两个目标分别维护独立的 collective/kernel 调度；必须用正确的 arch（`sm_120a`）构建，才可能拿到块缩放路径。
* 库的覆盖范围不同。在 B200 上有快速 NVFP4 kernel 的东西，在 `sm_120` 上可能会回退到通用路径。

> **要验证，不要假设。**"Blackwell 支持 NVFP4"是正确的，但不充分。问题始终是：*这个 runtime，在这个版本下，有没有为 `sm_120a` 编译的、适用于该算子 shape 的块缩放 kernel？* 用性能分析器回答，而不是用数据手册。

---

## 4. 原生/模拟边界

```text
   NATIVE  (tensor core consumes the format directly, block scales in hardware)
   ├── NVFP4   (E2M1 + E4M3 block scale)     ← the target
   ├── MXFP4   (E2M1 + E8M0 block scale)
   ├── FP6     (E3M2 / E2M3)
   ├── FP8     (E4M3 / E5M2)
   └── BF16 / FP16

   EMULATED  (must be unpacked to a wider type before any MMA)
   ├── INT4 with arbitrary group sizes      (fast kernels exist — Marlin-class — but hand-written)
   ├── 3-bit, 5-bit, 6-bit integer          ← no native path, no tuned kernels
   └── 2-bit                                 ← worst alignment behaviour
```

### 为什么奇数位宽会损失*有效*带宽

为 3-bit 辩护的朴素论证很有说服力。比较每权重的存储量：

```text
   NVFP4  :  4 + 8/16   = 4.50 bits  =  0.5625 bytes/weight
   INT3   :  3 + 16/128 = 3.125 bits =  0.3906 bytes/weight      →  1.44× fewer bytes
```

如果 decode（逐 token 生成阶段）是带宽受限的，字节数减少 1.44× 就应当意味着 tok/s 增加 1.44×。但通常并非如此，而且原因**并不是**主要在解包的算术开销：

```text
   4-bit:  8 weights pack into one 32-bit word, exactly.
           ┌────┬────┬────┬────┬────┬────┬────┬────┐
           │ w0 │ w1 │ w2 │ w3 │ w4 │ w5 │ w6 │ w7 │   aligned, coalesced,
           └────┴────┴────┴────┴────┴────┴────┴────┘   one shift+mask per weight

   3-bit:  10.67 weights per 32-bit word. Values STRADDLE word boundaries.
           ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬─┐┌─┬ ...
           │w0 │w1 │w2 │w3 │w4 │w5 │w6 │w7 │w8 │w9 │w│││10 ...
           └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴─┘└─┴ ...
                                                      ▲
                                        split across two words
```

跨边界存放迫使存储布局要么采用 bit 重排（这会破坏 GEMV 访问模式天然的合并），要么采用配合跨 lane shuffle 的多字读取。无论哪种方式，**实际达到的带宽都会下降**，而实际达到的带宽正是吞吐公式的分子。

### 盈亏平衡规则

把它量化。一种格式只有整体上每秒交付更多 token 才算胜出：

```text
                 BW_achieved
   tok/s   ∝   ────────────────
               bytes_per_weight
```

令 NVFP4（达到峰值的 92 %）等于 INT3（达到峰值的 `x`）：

```text
       0.92                 x
   ───────────   =   ───────────
     0.5625             0.3906

              0.3906
   x  =  0.92 × ──────── =  0.639
              0.5625
```

> **一个 INT3 kernel 必须维持 ≥ 64 % 的峰值内存带宽，才能追平 NVFP4。** 如果你手写的 3-bit GEMV 只达到 55 %——对未对齐布局来说这是完全典型的结果——那你就交付了一个 *小* 14 % 却 *慢* 14 % 的模型。

把它推广开。对任何候选格式：

```text
                                  bytes_candidate
   BW_achieved_required  =  0.92 × ────────────────
                                    bytes_baseline
```

在写 kernel **之前**就把这个算出来。它会告诉你门槛在哪，而且常常告诉你根本不值得去做。

### 第二项代价：离开原生路径

在 batch 1 时，以上所述就是全部。一旦你开始批处理——投机解码每个 pass 验证 K+1 个 token，这*就是*一个小 batch——模拟格式还会一并丧失 FP4 矩阵乘累加吞吐：

```text
   NVFP4 native   :  block-scaled mma.sync  →  up to 1676 TFLOP/s
   INT3 emulated  :  unpack → BF16 mma.sync →  up to  419 TFLOP/s     (4× less)
```

这一点尤为关键，因为 [模块 10](Lecture-10.md) 的投机会提高你的有效 batch。在 batch 1 时看似零代价的格式选择，可能会给你的投机收益封顶。


<details>
<summary>English original</summary>

**3. Two Blackwells, two tensor-core programming models**

This is the single most misattributed fact in current Blackwell material, so state it precisely:

```text
   HOPPER  (sm_90)     :  wgmma      — warpgroup MMA, async, operands in shared memory
   BLACKWELL datacenter
           (sm_100)    :  tcgen05    — 5th-gen MMA + Tensor Memory (TMEM), B200 / GB200
   BLACKWELL consumer
           (sm_120)    :  mma.sync   — warp-level MMA with BLOCK-SCALED FP4/FP8 operands
                                        RTX 50-series / GB202. No TMEM, no tcgen05.
```

Both Blackwell variants execute NVFP4 natively, but **through different instructions with different tiling requirements**, which means:

* A kernel tuned for B200 (`sm_100a`) does **not** simply recompile for RTX 5090.
* CUTLASS carries separate collective/kernel schedules for the two targets; you must build with the right arch (`sm_120a`) to get the block-scaled path at all.
* Library coverage differs. Something that has a fast NVFP4 kernel on B200 may fall back to a generic path on `sm_120`.

> **Verify, do not assume.** "Blackwell supports NVFP4" is true and insufficient. The question is always: *does this runtime, at this version, have a block-scaled kernel compiled for `sm_120a` for this operator shape?* Answer it with a profiler, not a datasheet.

---

**4. The native/emulated boundary**

```text
   NATIVE  (tensor core consumes the format directly, block scales in hardware)
   ├── NVFP4   (E2M1 + E4M3 block scale)     ← the target
   ├── MXFP4   (E2M1 + E8M0 block scale)
   ├── FP6     (E3M2 / E2M3)
   ├── FP8     (E4M3 / E5M2)
   └── BF16 / FP16

   EMULATED  (must be unpacked to a wider type before any MMA)
   ├── INT4 with arbitrary group sizes      (fast kernels exist — Marlin-class — but hand-written)
   ├── 3-bit, 5-bit, 6-bit integer          ← no native path, no tuned kernels
   └── 2-bit                                 ← worst alignment behaviour
```

**Why odd bit-widths lose *effective* bandwidth**

The naive argument for 3-bit is compelling. Compare per-weight storage:

```text
   NVFP4  :  4 + 8/16   = 4.50 bits  =  0.5625 bytes/weight
   INT3   :  3 + 16/128 = 3.125 bits =  0.3906 bytes/weight      →  1.44× fewer bytes
```

If decode is bandwidth-bound, 1.44× fewer bytes should mean 1.44× more tok/s. It usually does not, and the reason is **not** primarily the arithmetic cost of unpacking:

```text
   4-bit:  8 weights pack into one 32-bit word, exactly.
           ┌────┬────┬────┬────┬────┬────┬────┬────┐
           │ w0 │ w1 │ w2 │ w3 │ w4 │ w5 │ w6 │ w7 │   aligned, coalesced,
           └────┴────┴────┴────┴────┴────┴────┴────┘   one shift+mask per weight

   3-bit:  10.67 weights per 32-bit word. Values STRADDLE word boundaries.
           ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬─┐┌─┬ ...
           │w0 │w1 │w2 │w3 │w4 │w5 │w6 │w7 │w8 │w9 │w│││10 ...
           └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴─┘└─┴ ...
                                                      ▲
                                        split across two words
```

Straddling forces either a bit-shuffled storage layout (which breaks the natural coalescing of a GEMV's access pattern) or multi-word reads with cross-lane shuffles. Either way **achieved bandwidth drops**, and achieved bandwidth is the numerator of your throughput equation.

**The break-even rule**

Make it quantitative. A format wins only if it delivers more tokens per second overall:

```text
                 BW_achieved
   tok/s   ∝   ────────────────
               bytes_per_weight
```

Setting NVFP4 (achieving 92 % of peak) equal to INT3 (achieving `x` of peak):

```text
       0.92                 x
   ───────────   =   ───────────
     0.5625             0.3906

              0.3906
   x  =  0.92 × ──────── =  0.639
              0.5625
```

> **An INT3 kernel must sustain ≥ 64 % of peak memory bandwidth just to tie NVFP4.** If your hand-written 3-bit GEMV achieves 55 % — an entirely typical result for an unaligned layout — you have shipped a model that is 14 % *smaller* and 14 % *slower*.

Generalize it. For any candidate format:

```text
                                  bytes_candidate
   BW_achieved_required  =  0.92 × ────────────────
                                    bytes_baseline
```

Compute this **before** writing the kernel. It tells you the bar, and it frequently tells you not to bother.

**The second penalty: leaving the native path**

At batch 1 the above is the whole story. The moment you batch — speculative decoding verifies K+1 tokens per pass, which *is* a small batch — an emulated format also forfeits the FP4 MMA throughput:

```text
   NVFP4 native   :  block-scaled mma.sync  →  up to 1676 TFLOP/s
   INT3 emulated  :  unpack → BF16 mma.sync →  up to  419 TFLOP/s     (4× less)
```

This matters specifically because [Module 10](Lecture-10.md)'s speculation raises your effective batch. A format choice that looks free at batch 1 can cap your speculative gains.

---

</details>

## 5. 快路径在性能分析器中是什么样

不要相信配置里的格式名。要确认 kernel。在 **Nsight Compute** 中：

| 信号 | 快路径（原生 NVFP4） | 慢路径（反量化模拟） |
|---|---|---|
| Kernel 名称 | 包含 `nvfp4` / `mxf4` / `blockscaled` / CUTLASS `sm120` 调度 | 包含 `dequant`、`unpack`，或通用的 `gemv` |
| `sm__inst_executed_pipe_tensor` | 高 | 低或为零 |
| 整数/ALU 指令占比 | 低 | **高** —— 解包 |
| `dram__throughput.avg.pct_of_peak_sustained_elapsed` | 85–93 % | 通常 50–70 % |
| 寄存器压力 / occupancy | 中等 | 压力升高，occupancy 下降 |

最快的单项检查，就是那个零成本的检查：

```bash
# Are we even launching a block-scaled kernel?
nsys profile -o trace ./run_decode.sh
nsys stats --report cuda_gpu_kern_sum trace.nsys-rep | head -20

# Then the decisive counter:
ncu --metrics dram__throughput.avg.pct_of_peak_sustained_elapsed,\
sm__inst_executed_pipe_tensor.avg.pct_of_peak_sustained_active \
    --kernel-name-base demangled ./run_decode.sh
```


如果 `dram__throughput` 是 72 %，而张量流水线处于空闲，那么限制你的就不是物理带宽 —— 而是你的 kernel。**这正是 [模块 01 §5](Lecture-01.md) 为案例模型预测的情形**，这是 kernel 问题，不是量化问题。

---

## 6. 把硬件加速比这一列填上

回到 [模块 01](Lecture-01.md) 的机会框架，下面是 `sm_120` 的 `HardwareSpeedup` 因子：

| 目标格式 | 字节/权重 | 在 `sm_120` 上原生支持？ | 加速比因子 | 结论 |
|---|---:|---|---:|---|
| BF16 → FP8 | 2.0 → 1.0 | 是 | 2.0× | **安全，热点张量上始终值得** |
| BF16 → NVFP4 | 2.0 → 0.5625 | 是 | 3.56× | **主要抓手** |
| NVFP4 → MXFP4 | 0.5625 → 0.5312 | 是 | 1.06× | 不值得为此增加误差 |
| NVFP4 → INT3 | 0.5625 → 0.3906 | **否** | 标称 ≤1.44×，**实际 <1.0×** | **不要** |
| NVFP4 → INT2 | 0.5625 → 0.2656 | **否** | 标称 2.1×，实际 ≪1 | **不要** |

由此得到这套芯片的实用规则：

```text
   Every hot tensor should be NVFP4 or FP8.
   Nothing should be below 4 bits.
   The remaining wins are in WHICH tensors and in kernel quality — not in lower bit-widths.
```


这是一个很窄的设计空间，而收窄它正是目的。这意味着 [模块 04–11](Lecture-04.md) 可以完全聚焦于**分配** —— 哪个张量用 FP8、哪个用 NVFP4 —— 而不是在冷门格式上做无边界的搜索。

---

## 检查点

现在你应当能够：

1. 说出 RTX 5090 的带宽（1792 GB/s）、稠密 FP4 吞吐（~1676 TFLOP/s）以及 FP4 ridge point（~935 FLOP/byte）。
2. 解释为什么 “3352 AI TOPS” 不应出现在你的 roofline（性能上界模型）里。
3. 说出 `sm_90`、`sm_100`、`sm_120` 对应的张量核心指令族，且不把它们混淆。
4. 从对齐和实际达成带宽的角度，解释为什么小 44 % 的 3-bit 格式反而可能更慢。
5. 计算任一候选格式的盈亏平衡达成带宽阈值。
6. 说出区分原生 kernel 与反量化 kernel 的两个 Nsight 计数器。

---

## 交付

为你自己的目标 GPU 产出一张**格式可行性表**：

- 实测峰值带宽（跑一个 STREAM 风格或 `bandwidthTest` 的 copy benchmark；不要用数据手册）
- 各精度的稠密 TFLOP/s，以及每个精度对应的 ridge point
- 对你正在考虑的每种格式：字节/权重、原生还是模拟、盈亏平衡达成带宽阈值
- 一个 decode（逐 token 生成阶段）步骤的性能分析器 trace，含前三个 kernel 的 kernel 名称与 `dram__throughput`

任何人读了这张表，都应能在**不跑一次量化实验的情况下**说出哪些格式在你的机器上可行。

---

## 时效说明

* **长期有效：** roofline、原生与模拟之分、对齐论证、盈亏平衡推导。
* **2026 年硬件固定参数：** RTX 5090 / GB202 / `sm_120` / CC 12.0、1792 GB/s、~1676 稠密 FP4 TFLOP/s。数据中心 Blackwell = `sm_100`，配 `tcgen05` + TMEM；消费级 Blackwell = `sm_120`，配 block-scaled `mma.sync`。Hopper = `sm_90`，配 `wgmma`。
* **需要刷新的部分 —— 这是全课程最容易过期的模块。** CUTLASS、TensorRT-LLM 和 vLLM 对 `sm_120a` block-scaled GEMM（矩阵-矩阵乘）的 kernel 覆盖情况每个 release 都会变。每次升级 runtime 后都要重跑 §5 的性能分析器检查；版本号一变，就可能悄无声息地把你挪到快路径和慢路径之间。

---

**下一节：** [模块 04 —— 模型解剖 →](Lecture-04.md)


<details>
<summary>English original</summary>

**5. What the fast path looks like in a profiler**

Do not trust the format name in your config. Confirm the kernel. In **Nsight Compute**:

| Signal | Fast path (native NVFP4) | Slow path (dequant emulation) |
|---|---|---|
| Kernel name | contains `nvfp4` / `mxf4` / `blockscaled` / CUTLASS `sm120` schedule | contains `dequant`, `unpack`, or a generic `gemv` |
| `sm__inst_executed_pipe_tensor` | high | low or zero |
| Integer/ALU instruction share | low | **high** — the unpack |
| `dram__throughput.avg.pct_of_peak_sustained_elapsed` | 85–93 % | often 50–70 % |
| Register pressure / occupancy | moderate | elevated pressure, reduced occupancy |

The quickest single check is the one that costs nothing:

```bash
# Are we even launching a block-scaled kernel?
nsys profile -o trace ./run_decode.sh
nsys stats --report cuda_gpu_kern_sum trace.nsys-rep | head -20

# Then the decisive counter:
ncu --metrics dram__throughput.avg.pct_of_peak_sustained_elapsed,\
sm__inst_executed_pipe_tensor.avg.pct_of_peak_sustained_active \
    --kernel-name-base demangled ./run_decode.sh
```

If `dram__throughput` is 72 % and the tensor pipe is idle, you are not bandwidth-limited by physics — you are limited by your kernel. **That is exactly the situation [Module 01 §5](Lecture-01.md) predicted for the case-study model**, and it is a kernel problem, not a quantization problem.

---

**6. The hardware-speedup column, filled in**

Returning to the opportunity framework from [Module 01](Lecture-01.md), here is the `HardwareSpeedup` factor for `sm_120`:

| Target format | Bytes/weight | Native on `sm_120`? | Speedup factor | Verdict |
|---|---:|---|---:|---|
| BF16 → FP8 | 2.0 → 1.0 | yes | 2.0× | **safe, always worth it on hot tensors** |
| BF16 → NVFP4 | 2.0 → 0.5625 | yes | 3.56× | **the main lever** |
| NVFP4 → MXFP4 | 0.5625 → 0.5312 | yes | 1.06× | not worth the error increase |
| NVFP4 → INT3 | 0.5625 → 0.3906 | **no** | ≤1.44× nominal, **<1.0× realistic** | **do not** |
| NVFP4 → INT2 | 0.5625 → 0.2656 | **no** | nominal 2.1×, realistic ≪1 | **do not** |

Which gives the practical rule for this silicon:

```text
   Every hot tensor should be NVFP4 or FP8.
   Nothing should be below 4 bits.
   The remaining wins are in WHICH tensors and in kernel quality — not in lower bit-widths.
```

That is a narrow design space, and narrowing it is the point. It means [Modules 04–11](Lecture-04.md) can focus entirely on **allocation** — which tensor gets FP8 and which gets NVFP4 — rather than on an unbounded search over exotic formats.

---

**Checkpoint**

You should now be able to:

1. State the RTX 5090's bandwidth (1792 GB/s), dense FP4 throughput (~1676 TFLOP/s), and FP4 ridge point (~935 FLOP/byte).
2. Explain why "3352 AI TOPS" should not appear in your roofline.
3. Name the tensor-core instruction family for `sm_90`, `sm_100`, and `sm_120` without confusing them.
4. Explain why a 44 %-smaller 3-bit format can be slower, in terms of alignment and achieved bandwidth.
5. Compute the break-even achieved-bandwidth threshold for any candidate format.
6. Name the two Nsight counters that distinguish a native kernel from a dequant kernel.

---

**Ship it**

Produce a **format feasibility table** for your own target GPU:

- measured peak bandwidth (run a STREAM-style or `bandwidthTest` copy benchmark; do not use the datasheet)
- dense TFLOP/s per precision, and the ridge point for each
- for each format you are considering: bytes/weight, native or emulated, break-even achieved-bandwidth threshold
- a profiler trace of one decode step, with the kernel names and `dram__throughput` for the top three kernels

Anyone reading that table should be able to say which formats are viable on your machine **without running a single quantization experiment.**

---

**Current as of**

* **Timeless:** the roofline, the native-vs-emulated distinction, the alignment argument, the break-even derivation.
* **2026 hardware pins:** RTX 5090 / GB202 / `sm_120` / CC 12.0, 1792 GB/s, ~1676 dense FP4 TFLOP/s. Datacenter Blackwell = `sm_100` with `tcgen05` + TMEM; consumer Blackwell = `sm_120` with block-scaled `mma.sync`. Hopper = `sm_90` with `wgmma`.
* **Refresh surface — this is the most perishable module in the course.** Kernel coverage in CUTLASS, TensorRT-LLM, and vLLM for `sm_120a` block-scaled GEMMs changes release to release. Re-run the profiler checks in §5 after every runtime upgrade; a version bump can silently move you between the fast and slow paths.

---

**Next:** [Module 04 — Model Anatomy →](Lecture-04.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/Hardware-Aware LLM Quantization/Lecture-03.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/Hardware-Aware%20LLM%20Quantization/Lecture-03.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
