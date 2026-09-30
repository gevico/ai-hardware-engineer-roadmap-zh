---
title: 模块 01 — 推理物理
description: 模块 01 — 推理物理
---

# 模块 01 — 推理物理

**合集：** [硬件感知的大语言模型量化](README.md) | **上一节：** [← 课程索引](README.md) | **下一节：** [模块 02 →](Lecture-02.md)

---

这是本课程最重要的模块，其中完全没有量化算法。

在问 *“该用哪种量化方法？”* 之前，必须先回答 *“是什么资源在限制这个工作负载？”* 量化一个算力受限的工作负载与量化一个带宽受限的工作负载，是**两个不同的优化问题，正确答案也不同**。搞反这一点，后续每个决策都会继承这个错误。

---

## 学习目标

学完本模块后，你应能：

1. 计算一个 decode（逐 token 生成阶段）步骤的**算术强度**，并将其放到 roofline（性能上界模型）上。
2. 推导批大小为 1 的 decode 上限 `tok/s ≈ BW_eff / B_token`，并用它将吞吐预测到约 10 % 以内。
3. 区分**常驻字节**与**每 token 读取字节**，并解释为什么将二者混为一谈会高估带宽利用率。
4. 应用 `Opportunity = Traffic × Compressibility × HardwareSpeedup × BehaviorTolerance` 框架对量化目标排序。
5. 解释为什么“最小化位数”和“最大化 tok/s”都是错误的目标。

---

## 1. 一个 token 穿过大语言模型

自回归生成是一个循环，循环体就是整个模型：

```text
   token N ─▶ embedding ─▶ layer 0 ─▶ layer 1 ─▶ ... ─▶ layer L-1 ─▶ final norm
                                                                        │
                              token N+1 ◀── sample ◀── logits ◀── lm_head
                                    │
                                    └────────── repeat ──────────┐
                                                                  ▼
```

对于**每一个生成的 token**，GPU 都必须应用数十亿个参数。在批大小为 1 且并发为 1 时，这些权重不会在同时处理的 token 之间复用——加载一个权重，用一次，就丢弃，下一个 token 再重新加载。

这造就了本地大语言模型 decode 的决定性条件：

> **GPU 在搬运权重上花的时间比在权重上做算术的时间更多。**

Prefill（首字前的整段计算）正相反。Prefill 通过同一次权重加载处理数百或数千个 token，因此权重得到复用，工作负载变为算力受限。**Prefill 和 decode 位于 roofline 的两侧，却在同一个模型、同一个请求中。** 这个领域几乎每一个错误都来自把 decode 的直觉套用到 prefill 上，或反之。

---

## 2. 算术强度

将其形式化的工具是**算术强度**——每移动一字节所执行的操作数：

```text
              FLOPs performed
   AI  =  ───────────────────────        [FLOP / byte]
              bytes transferred
```

由此得到两种区间，由机器的**脊点**（`peak FLOP/s ÷ peak bytes/s`）分隔：

```text
   AI < ridge  →  MEMORY-BOUND        T ≈ bytes_moved / bandwidth
                  more tensor-core throughput does ~nothing
                  the fix is: MOVE FEWER BYTES

   AI > ridge  →  COMPUTE-BOUND       T ≈ FLOPs / (FLOP/s)
                  more bandwidth does ~nothing
                  the fix is: DO LESS MATH, or use faster math units
```

现在计算批大小为 1 的 decode GEMV（矩阵-向量乘）的 AI。一个包含 `N×K` 个元素的权重矩阵执行 `2·N·K` FLOPs（每个元素一次乘法、一次加法），并移动 `N·K·bytes_per_weight` 字节：

```text
                2 · N · K                 2
   AI_decode = ─────────────────  =  ────────────
               N · K · bytes_w        bytes_w
```

矩阵维度相互抵消。**批大小为 1 时的算术强度只取决于权重格式：**

| 权重格式 | 字节/权重 | AI (FLOP/byte) |
|---|---|---|
| BF16 | 2.0 | 1.0 |
| FP8 | 1.0 | 2.0 |
| NVFP4 | 0.5625 | 3.56 |

RTX 5090 针对 BF16 张量运算的脊点约为 `419 TFLOP/s ÷ 1792 GB/s ≈ 234 FLOP/byte`。批大小为 1 的 decode 位于 **1.0**。

```text
   FLOP/s
     ▲
     │                        ┌──────────────── compute-bound roof
     │                       ╱
     │                      ╱
     │                     ╱  ridge ≈ 234 FLOP/byte (BF16)
     │                    ╱
     │  ┌────────────────╱
     │  │  memory-bound slope = 1792 GB/s
     │  │
     └──┴──▲─────────────────────────────────────▶ AI (FLOP/byte)
           │
        AI = 1.0
     batch-1 decode is ~234× BELOW the ridge
```

更一般地，在批大小为 `B` 时，强度为 `2B / bytes_w`，因此达到脊点所需的批大小为：

| 格式 | 批大小为 B 时的 AI | 达到脊点所需的 B |
|---|---|---|
| BF16 | `B` | ~234 |
| NVFP4 | `3.56 · B` | ~263 |

**在 decode 不再是带宽受限之前，你需要大约 250 个并发序列的批。** 单用户本地推理远未接近，也不会接近，每个优化决策都应当由此推出。

---


<details>
<summary>English original</summary>

**Module 01 — Inference Physics**

**Collection:** [Hardware-Aware LLM Quantization](README.md) | **Previous:** [← Course index](README.md) | **Next:** [Module 02 →](Lecture-02.md)

---

This is the most important module in the course, and it contains no quantization algorithms at all.

Before asking *"what quantization method should I use?"* you must answer *"what resource is limiting this workload?"* Quantizing a compute-bound workload and quantizing a bandwidth-bound workload are **different optimization problems with different correct answers**. Get this backwards and every downstream decision inherits the error.

---

**Learning objectives**

By the end of this module you should be able to:

1. Compute **arithmetic intensity** for a decode step and place it on a roofline.
2. Derive the batch-1 decode ceiling `tok/s ≈ BW_eff / B_token` and use it to predict throughput within ~10 %.
3. Distinguish **resident bytes** from **per-token-read bytes**, and explain why conflating them overstates bandwidth utilization.
4. Apply the `Opportunity = Traffic × Compressibility × HardwareSpeedup × BehaviorTolerance` framework to rank quantization targets.
5. Explain why "minimize bits" and "maximize tok/s" are both wrong objectives.

---

**1. One token through an LLM**

Autoregressive generation is a loop, and the loop body is the entire model:

```text
   token N ─▶ embedding ─▶ layer 0 ─▶ layer 1 ─▶ ... ─▶ layer L-1 ─▶ final norm
                                                                        │
                              token N+1 ◀── sample ◀── logits ◀── lm_head
                                    │
                                    └────────── repeat ──────────┐
                                                                  ▼
```

For **every single generated token**, the GPU must apply billions of parameters. At batch size 1 and concurrency 1, those weights are not reused across simultaneous tokens — you load a weight, you use it once, you throw it away, and next token you load it again.

That produces the defining condition of local LLM decode:

> **The GPU spends more time transporting weights than doing arithmetic on them.**

Prefill is the opposite. Prefill processes hundreds or thousands of tokens through the same weight-load, so weights get reused and the workload becomes compute-bound. **Prefill and decode live on opposite sides of the roofline, in the same model, in the same request.** Almost every mistake in this field comes from applying a decode intuition to prefill or vice versa.

---

**2. Arithmetic intensity**

The tool that formalizes this is **arithmetic intensity** — operations performed per byte moved:

```text
              FLOPs performed
   AI  =  ───────────────────────        [FLOP / byte]
              bytes transferred
```

Two regimes follow, separated by the machine's **ridge point** (`peak FLOP/s ÷ peak bytes/s`):

```text
   AI < ridge  →  MEMORY-BOUND        T ≈ bytes_moved / bandwidth
                  more tensor-core throughput does ~nothing
                  the fix is: MOVE FEWER BYTES

   AI > ridge  →  COMPUTE-BOUND       T ≈ FLOPs / (FLOP/s)
                  more bandwidth does ~nothing
                  the fix is: DO LESS MATH, or use faster math units
```

Now compute AI for a batch-1 decode GEMV. A weight matrix of `N×K` elements does `2·N·K` FLOPs (one multiply, one add per element) and moves `N·K·bytes_per_weight` bytes:

```text
                2 · N · K                 2
   AI_decode = ─────────────────  =  ────────────
               N · K · bytes_w        bytes_w
```

The matrix dimensions cancel. **Arithmetic intensity at batch 1 depends only on the weight format:**

| Weight format | bytes/weight | AI (FLOP/byte) |
|---|---|---|
| BF16 | 2.0 | 1.0 |
| FP8 | 1.0 | 2.0 |
| NVFP4 | 0.5625 | 3.56 |

The RTX 5090's ridge point for BF16 tensor math is roughly `419 TFLOP/s ÷ 1792 GB/s ≈ 234 FLOP/byte`. Batch-1 decode sits at **1.0**.

```text
   FLOP/s
     ▲
     │                        ┌──────────────── compute-bound roof
     │                       ╱
     │                      ╱
     │                     ╱  ridge ≈ 234 FLOP/byte (BF16)
     │                    ╱
     │  ┌────────────────╱
     │  │  memory-bound slope = 1792 GB/s
     │  │
     └──┴──▲─────────────────────────────────────▶ AI (FLOP/byte)
           │
        AI = 1.0
     batch-1 decode is ~234× BELOW the ridge
```

More generally, at batch size `B` the intensity is `2B / bytes_w`, so the batch needed to reach the ridge is:

| Format | AI at batch B | B needed to reach ridge |
|---|---|---|
| BF16 | `B` | ~234 |
| NVFP4 | `3.56 · B` | ~263 |

**You would need a batch of roughly 250 concurrent sequences before decode stops being memory-bound.** Single-user local inference is not close, is not going to get close, and every optimization decision should follow from that.

---

</details>

## 3. 测量——以及一个单位陷阱

以下是 RTX 5090 上一个约 27 B 的 NVFP4 模型的真实测量结果：

```text
   Resident weights   :   18.80 GiB
   Decode throughput  :   81.6  tok/s
   RTX 5090 peak BW   :   1792  GB/s   (32 GB GDDR7, 512-bit @ 28 Gbps)
```

最自然的做法是把常驻字节数乘以每秒 token 数。请务必小心，因为 **GiB 与 GB 相差 7.4 %，而大多数公开发表的带宽数字正是错在这里**：

```text
   18.80 GiB  ×  1024³ / 10⁹   =   20.19 GB          ← convert FIRST
   20.19 GB   ×  81.6 tok/s     =   1647 GB/s
   1647 / 1792                  =   91.9 %  of peak
```

跳过换算，你会得到 `18.80 × 81.6 = 1534`，报出 “1.53 TB/s / 86 %”，把机器低估 7.4 %。带宽以 **GB/s（十进制）** 引用；`nvidia-smi` 和大多数分配器以 **GiB（二进制）** 报告内存。只在边界处换算一次，并给每个数字标注单位。

所以核心结果是：decode（逐 token 生成阶段）路径似乎跑在 **理论峰值内存带宽的 ~92 %**。这是一个极强的信号：

> *“我主要需要的不是更多的算术吞吐。我需要的是每个生成 token 更少的字节。”*

记住这个数字。第 5 节会修正它，而这个修正正是本课程的核心。

---

## 4. 基本方程

对于强带宽受限的 decode 工作负载：

```text
              BW_effective
   tok/s  ≈  ──────────────
                B_token
```

其中 `BW_effective` 是可达带宽（绝不是 datasheet 上的数字——写得好的 kernel 可以期望达到峰值的 85–93 %），而 `B_token` 是生成一个 token **必须** 取回的字节数。

举个例子。假设 `BW_eff = 1650 GB/s` 且 `B_token = 20 GB`：

```text
   tok/s ≈ 1650 / 20   =  82.5
```

现在去掉 2 GB 的每 token 流量：

```text
   tok/s ≈ 1650 / 18   =  91.7      →  +11 % throughput
```

你没有增加一个 CUDA 核心。你没有写更快的 GEMM。你只是不再每秒 60 次搬运那多余的 2 GB。**这就是硬件感知量化。**

把方程重新整理，它同时也是一个 *设计工具*。如果你有吞吐目标，它会告诉你你的字节预算：

```text
   B_token_required  =  BW_effective / tok/s_target

   want 120 tok/s at 1650 GB/s   →   B_token must be ≤ 13.75 GB
```

这是一条硬性、可检验的工程约束，你可以在 **写任何代码之前** 就对照检查点评估它。

---

## 5. 检查点大小 ≠ 每 token 字节数

下面是同一个模型的检查点清单。约 **6.91 GB（33.6 %）仍是 BF16**：

```text
   Embeddings      2.54 GB  BF16
   lm_head         2.54 GB  BF16
   Vision tower    0.92 GB  BF16
   MTP head        0.85 GB  BF16
   norms / misc    0.06 GB
   ───────────────────────
                   6.91 GB  BF16   (remaining 13.28 GB is NVFP4 transformer body)
```

初学者的结论是“太好了——6.91 GB 的低垂果实，把它们全部量化掉”。这个结论对四个张量中的三个都是错的，原因在于 **这四个张量的 runtime 行为完全不同。**

### Embedding —— 2.54 GB，每 token 读取 ~16 KB

embedding 查找是一次 **gather**，不是矩阵乘：

```cpp
// you touch ONE row of a [vocab × d_model] table
const __nv_bfloat16* row = embedding + (size_t)token_id * d_model;
```

在 BF16 下，`d_model = 8192` 得到 `8192 × 2 = 16 KB`——约占 2.54 GB 表的 **0.0008 %**。量化 embedding 是一次 *容量* 优化，值 1.3 GB 的 VRAM。它对 decode 吞吐的影响与零无法区分。

### 视觉塔 —— 0.92 GB，每 token 读 0 字节

在纯文本 decode 期间，视觉编码器从不运行。量化它、驱逐它、删掉它——VRAM 会变，`tok/s` 不会变。

### MTP 头 —— 0.85 GB，条件性

如果投机解码关闭，它根本不在 decode 路径上。如果开启，它每个 draft 步运行一次，成为系统中访问最热的内存之一。**它的流量是你推理服务配置的函数，不是检查点的函数。** 模块 10 会妥善处理这一点。

### lm_head —— 2.54 GB，每个 token 全量读取

完全不同。输出投影在整个词表上计算 `logits = W_vocab · h`。这里没有 gather；**矩阵的每个元素都参与每个 token**。这 2.54 GB 正落在每 token 关键路径上——而且它仍是 BF16，而 Transformer 主体已经是 NVFP4。

这才是藏在清单里的真正目标，而且它 *不是* 最大的张量组。


<details>
<summary>English original</summary>

**3. The measurement — and a unit trap**

Here is a real measurement from a ~27 B NVFP4 model on an RTX 5090:

```text
   Resident weights   :   18.80 GiB
   Decode throughput  :   81.6  tok/s
   RTX 5090 peak BW   :   1792  GB/s   (32 GB GDDR7, 512-bit @ 28 Gbps)
```

The obvious move is to multiply resident bytes by tokens per second. Do it carefully, because **GiB and GB differ by 7.4 % and this is where most published bandwidth numbers go wrong**:

```text
   18.80 GiB  ×  1024³ / 10⁹   =   20.19 GB          ← convert FIRST
   20.19 GB   ×  81.6 tok/s     =   1647 GB/s
   1647 / 1792                  =   91.9 %  of peak
```

Skip the conversion and you get `18.80 × 81.6 = 1534`, report "1.53 TB/s / 86 %", and understate the machine by 7.4 %. Bandwidth is quoted in **GB/s (decimal)**; `nvidia-smi` and most allocators report memory in **GiB (binary)**. Convert once, at the boundary, and label every number.

So the headline result: the decode path appears to run at **~92 % of theoretical peak memory bandwidth**. That is an extraordinarily strong signal:

> *"I do not primarily need more arithmetic throughput. I need fewer bytes per generated token."*

Hold that number. Section 5 is going to correct it, and the correction is the whole point of this course.

---

**4. The fundamental equation**

For a strongly bandwidth-bound decode workload:

```text
              BW_effective
   tok/s  ≈  ──────────────
                B_token
```

where `BW_effective` is achievable bandwidth (never the datasheet number — expect 85–93 % of peak on a well-written kernel) and `B_token` is bytes that **must** be fetched to produce one token.

Work an example. Suppose `BW_eff = 1650 GB/s` and `B_token = 20 GB`:

```text
   tok/s ≈ 1650 / 20   =  82.5
```

Now remove two gigabytes of per-token traffic:

```text
   tok/s ≈ 1650 / 18   =  91.7      →  +11 % throughput
```

You did not add a CUDA core. You did not write a faster GEMM. You stopped moving two unnecessary gigabytes, sixty times a second. **That is what hardware-aware quantization is.**

Rearranged, the equation is also a *design tool*. If you have a throughput target, it tells you your byte budget:

```text
   B_token_required  =  BW_effective / tok/s_target

   want 120 tok/s at 1650 GB/s   →   B_token must be ≤ 13.75 GB
```

That is a hard, checkable engineering constraint, and you can evaluate it against a checkpoint **before writing any code**.

---

**5. Checkpoint size ≠ bytes per token**

Here is the checkpoint inventory for the same model. About **6.91 GB (33.6 %) is still BF16**:

```text
   Embeddings      2.54 GB  BF16
   lm_head         2.54 GB  BF16
   Vision tower    0.92 GB  BF16
   MTP head        0.85 GB  BF16
   norms / misc    0.06 GB
   ───────────────────────
                   6.91 GB  BF16   (remaining 13.28 GB is NVFP4 transformer body)
```

The beginner's conclusion is "great — 6.91 GB of low-hanging fruit, quantize all of it." That conclusion is wrong for three of the four tensors, and the reason is that **these four have completely different runtime behavior.**

**Embeddings — 2.54 GB, read ~16 KB per token**

An embedding lookup is a **gather**, not a matrix multiply:

```cpp
// you touch ONE row of a [vocab × d_model] table
const __nv_bfloat16* row = embedding + (size_t)token_id * d_model;
```

With `d_model = 8192` at BF16 that is `8192 × 2 = 16 KB` — about **0.0008 %** of the 2.54 GB table. Quantizing embeddings is a *capacity* optimization worth 1.3 GB of VRAM. Its effect on decode throughput is indistinguishable from zero.

**Vision tower — 0.92 GB, read 0 bytes per token**

During text-only decode the vision encoder never runs. Quantize it, evict it, delete it — VRAM changes, `tok/s` does not.

**MTP head — 0.85 GB, conditional**

If speculative decoding is off, it is not on the decode path at all. If it is on, it runs once per draft step and becomes some of the hottest memory in the system. **Its traffic is a function of your serving configuration, not of the checkpoint.** Module 10 handles this properly.

**lm_head — 2.54 GB, read in full, every token**

Completely different. The output projection computes `logits = W_vocab · h` over the whole vocabulary. There is no gather; **every element of the matrix participates in every token**. It is 2.54 GB squarely on the per-token critical path — and it is still BF16 while the transformer body is already NVFP4.

That is the real target hiding in the inventory, and it is *not* the biggest tensor group.

</details>

### 修正利用率数字

现在只用 decoder 实际读取的字节数，重做第 3 节的算术：

```text
   resident                        20.19 GB
   − embeddings (gathered)         − 2.54
   − vision tower (inactive)       − 0.92
   − MTP head (spec. disabled)     − 0.85
   ─────────────────────────────────────────
   B_token                        ≈ 15.88 GB
```

```text
   real traffic  =  15.88 GB × 81.6 tok/s  =  1296 GB/s   =  72.3 % of peak
```

**92 % 这个数字是高估了。** 它让内存系统为一笔 4.3 GB 的字节买单，而这些字节在文本 decode 期间从未被取用。实际带宽利用率更接近 **72 %**，这彻底改变了工程结论：

```text
   at 92 % utilization  →  the byte count is the only lever; you are at the wall
   at 72 % utilization  →  ~20 points of headroom exist that are NOT a bytes problem
                            (kernel efficiency, launch gaps, tail effects, scheduling)

   throughput if the SAME 15.88 GB were moved at 92 % of peak (1650 GB/s):
        1650 / 15.88  =  103.9 tok/s     ← vs. 81.6 measured
```

在你再砍掉任何一个额外的 bit 之前，大约就有 **22 tok/s 可用。** 如果团队信了 92 % 这个数字，就会把那个季度花在量化上却一无所获，因为在该工作点上，这些字节从来就不是约束瓶颈。

> **这是本课程中最有价值的一个习惯：** 绝不要用检查点大小来算带宽利用率。要用每 token 字节账本来算。[Module 04](Lecture-04.md) 会把这个账本正确地建立起来。

---

## 6. 硬件感知量化 vs. 普通量化

朴素的压缩策略问的是 **“哪个张量大？”** 更好的策略问的是 **“哪个张量产生的 runtime 内存流量最多？”** 最好的策略则同时给四个因素打分：

```text
   Opportunity_i  =  Traffic_i  ×  Compressibility_i  ×  HardwareSpeedup_i  ×  BehaviorTolerance_i
```

| 因素 | 问题 | 忽略的后果 |
|---|---|---|
| **流量** | 每生成一个 token 移动的字节数 | 你去量化一个 vision tower，换来 0 tok/s |
| **可压缩性** | 这个张量实际能舍弃多少 bit？ | 你对一个本需要 FP8 的张量强上 FP4 |
| **硬件加速比** | 这块硅能原生*执行*压缩后的形式吗？ | 你上 3-bit，结果 dequant kernel 比 4-bit 还慢 |
| **行为容忍度** | 这个张量能吸收多少误差？ | 你量化 Q/K，损失四分之一的接受率 |

第三个因素正是人们会跳过的那个，而在 Blackwell 上它是决定性的：

```text
   4 bits (NVFP4)  →  fewer bytes  +  NATIVE block-scaled tensor-core path
   3 bits          →  fewer bytes  +  no native path → unpack to a wider type first
   2 bits          →  fewest bytes +  unpack overhead can exceed the bandwidth saved
```

> **最低的 bit 宽度并不意味着最快的推理。** 只有当张量核心能不经解包绕路地直接消费某种格式时，它才算快。[Module 03](Lecture-03.md) 针对 `sm_120` 把这一点讲精确了。

---

## 7. 准确率也不是单一变量

考虑两个候选方案：

```text
   Profile A :  148 tok/s,  acceptance 2.79
   Profile B :  151 tok/s,  acceptance 2.55
```

B 在头号指标上更快。你会发布它吗？几乎肯定不会 —— B 让模型与自己 drafter 的一致性下降了 9 %，这意味着目标分布已经移动。它用一次行为退化换来了 2 % 的吞吐。

这不是假设。在案例模型上实测：

```text
   MLP + O quantized          147.87 tok/s     acceptance 2.792
   MLP + O + QKV quantized    150.73 tok/s     acceptance 2.546
                              ──────────       ─────────────────
                              +1.9 % speed     −8.8 % acceptance
```

把 QKV 加入量化集合，换来 **1.9 % 的吞吐**，代价是 **8.8 % 的接受率**。这是一笔糟糕的交易，而如果你唯一的指标是 tok/s，这一点就完全看不见。

所以目标从来不是 `min(bits)`，也从来不是 `max(tok/s)`。它是一个**带约束的优化**：

```text
   maximize    tok/s

   subject to  BehaviorLoss  <  ε           (KL vs. reference, acceptance length)
               VRAM          <  32 GB
               context       =  262 144 tokens
               format ∈ {hardware-native formats on sm_120}
```

[Module 11](Lecture-11.md) 明确地求解这个问题。从这里到那里的所有内容，都是在度量它的各项。

---


<details>
<summary>English original</summary>

**Correcting the utilization number**

Now redo Section 3's arithmetic with only the bytes the decoder actually reads:

```text
   resident                        20.19 GB
   − embeddings (gathered)         − 2.54
   − vision tower (inactive)       − 0.92
   − MTP head (spec. disabled)     − 0.85
   ─────────────────────────────────────────
   B_token                        ≈ 15.88 GB
```

```text
   real traffic  =  15.88 GB × 81.6 tok/s  =  1296 GB/s   =  72.3 % of peak
```

**The 92 % figure was an overestimate.** It charged the memory system for 4.3 GB of bytes that are never fetched during text decode. Actual bandwidth utilization is closer to **72 %**, and that changes the engineering conclusion completely:

```text
   at 92 % utilization  →  the byte count is the only lever; you are at the wall
   at 72 % utilization  →  ~20 points of headroom exist that are NOT a bytes problem
                            (kernel efficiency, launch gaps, tail effects, scheduling)

   throughput if the SAME 15.88 GB were moved at 92 % of peak (1650 GB/s):
        1650 / 15.88  =  103.9 tok/s     ← vs. 81.6 measured
```

There are roughly **22 tok/s available before you remove a single additional bit.** A team that believed the 92 % number would have spent that quarter on quantization and found nothing, because the bytes were never the binding constraint at that operating point.

> **This is the single most valuable habit in the course:** never compute bandwidth utilization from checkpoint size. Compute it from a per-token byte ledger. [Module 04](Lecture-04.md) builds that ledger properly.

---

**6. Hardware-aware quantization vs. ordinary quantization**

A naive compression strategy asks **"which tensor is large?"** A better one asks **"which tensor produces the most runtime memory traffic?"** The best one scores four factors at once:

```text
   Opportunity_i  =  Traffic_i  ×  Compressibility_i  ×  HardwareSpeedup_i  ×  BehaviorTolerance_i
```

| Factor | Question | Failure if ignored |
|---|---|---|
| **Traffic** | bytes moved per generated token | you quantize a vision tower for 0 tok/s |
| **Compressibility** | how many bits can this tensor actually give up? | you force FP4 on a tensor that needed FP8 |
| **HardwareSpeedup** | does this silicon *execute* the compressed form natively? | you ship 3-bit and get a dequant kernel that is slower than 4-bit |
| **BehaviorTolerance** | how much error can this tensor absorb? | you quantize Q/K and lose a quarter of your acceptance rate |

The third factor is the one people skip, and on Blackwell it is decisive:

```text
   4 bits (NVFP4)  →  fewer bytes  +  NATIVE block-scaled tensor-core path
   3 bits          →  fewer bytes  +  no native path → unpack to a wider type first
   2 bits          →  fewest bytes +  unpack overhead can exceed the bandwidth saved
```

> **Lowest bit-width does not mean fastest inference.** A format is only fast if the tensor cores can consume it without an unpacking detour. [Module 03](Lecture-03.md) makes this precise for `sm_120`.

---

**7. Accuracy is not one variable either**

Consider two candidate profiles:

```text
   Profile A :  148 tok/s,  acceptance 2.79
   Profile B :  151 tok/s,  acceptance 2.55
```

B is faster on the headline metric. Would you ship it? Almost certainly not — B degraded the model's agreement with its own drafter by 9 %, which means the target distribution moved. It bought 2 % throughput with a behavioral regression.

This is not hypothetical. Measured on the case-study model:

```text
   MLP + O quantized          147.87 tok/s     acceptance 2.792
   MLP + O + QKV quantized    150.73 tok/s     acceptance 2.546
                              ──────────       ─────────────────
                              +1.9 % speed     −8.8 % acceptance
```

Adding QKV to the quantization set bought **1.9 % throughput** and cost **8.8 % acceptance**. That is a bad trade, and it is invisible if your only metric is tok/s.

So the objective is never `min(bits)` and never `max(tok/s)`. It is a **constrained optimization**:

```text
   maximize    tok/s

   subject to  BehaviorLoss  <  ε           (KL vs. reference, acceptance length)
               VRAM          <  32 GB
               context       =  262 144 tokens
               format ∈ {hardware-native formats on sm_120}
```

[Module 11](Lecture-11.md) solves this problem explicitly. Everything between here and there is about measuring its terms.

---

</details>

## 8. 优化层级

为每个张量组附加一份行为画像，而不是附加一个大小：

```text
                         Large?   Read every token?   Speed target?
   ────────────────────────────────────────────────────────────────
   Embeddings             YES           NO                 NO
   Vision tower           YES           NO                 NO
   MTP head               YES        conditional        conditional
   lm_head                YES           YES                YES
   MLP weights            HUGE          YES                YES
   Attention weights      YES           YES           YES (but see Mod. 07)
   KV cache             grows           YES          context-dependent
```

由此得到一个**随上下文长度变化**的优先级顺序：

```text
   short context                      very long context (262 K)
   ─────────────                      ─────────────────────────
   1. MLP weights                     1. KV cache traffic
   2. lm_head                         2. MLP weights
   3. attention weights               3. lm_head
   4. (KV cache — small)              4. attention weights
```

在 262 K token 时，KV cache 不再是脚注，而成为共同主导的流量来源。这就是为什么长上下文目标会引入一个在短上下文 benchmark 中根本不会出现的优化问题 —— [Module 09](Lecture-09.md)。

---

## 9. 练习 —— 在写任何代码之前先给它们排序

| 张量 | 大小 | 每个 token 都读？ | 当前精度 |
|---|---:|---|---|
| Embeddings | 3 GB | 否（gather） | BF16 |
| MLP | 10 GB | 是 | FP4 |
| lm_head | 2.5 GB | 是 | BF16 |
| Vision | 1 GB | 否 | BF16 |
| QKV | 1 GB | 是 | FP4 |

**差的目标 —— `Vision → FP4`。** 存储收益大，文本 decode 收益为零。`Traffic = 0` 让整个机会乘积归零。

**最佳目标 —— `lm_head BF16 → FP8`。** checkpoint 缩减幅度不大（2.5 → 1.25 GB），但这是 2.5 GB 的*每 token* 流量，而 FP8 有原生路径。用 §4 的公式估算预期效果，其中 `B_token = 13.5 GB`：

```text
   before:  1650 / 13.5   =  122.2 tok/s
   after :  1650 / 12.25  =  134.7 tok/s      →  +10.2 %
```

**危险的目标 —— `QKV → lower precision`。** 流量只有 1 GB（占账本的 7 %），所以收益上限约 7 %，而 [Module 07](Lecture-07.md) 表明其行为代价却不成比例。低回报、高风险 —— 最差的象限。

注意，这个排序几乎完全由 **Traffic** 和 **BehaviorTolerance** 两项驱动，与张量大小毫无关系。Embeddings 是案例研究中最大的 BF16 张量，却排在最后。

---

## 10. 心智模型

不要再这样看：

```text
   Qwen3.8-27B  →  27 B parameters
```

而要这样看：

```text
   Qwen3.8-27B
   │
   ├── read EVERY token  ────────────────  the decode critical path
   │   ├── MLP weights
   │   ├── attention weights
   │   └── lm_head
   │
   ├── read SPARSELY  ───────────────────  capacity cost only
   │   └── embeddings (one row per token)
   │
   ├── CONDITIONAL  ─────────────────────  traffic depends on serving config
   │   ├── vision tower (multimodal requests only)
   │   └── MTP head (speculation enabled only)
   │
   └── GROWS WITH CONTEXT  ──────────────  the long-context term
       └── KV cache
```

并给每个节点附加：

```text
   bytes  ·  precision  ·  bytes/token  ·  kernel path  ·  quant error  ·  behavior sensitivity
```

那棵带标注的树 —— 而不是参数量 —— 才是你优化的对象。

---

## 检查点

现在你应该能解释为什么下面这四点同时成立：

1. 一个 **3-bit 模型可能比 NVFP4 模型更小，却更慢**。*（没有原生 tensor-core 路径 → 解包开销超过节省下来的带宽。）*
2. 移除一个 **1 GB 的视觉塔可省下 1 GB 显存，却只换来 0 tok/s**。*（文本 decode 期间流量 = 0。）*
3. 量化一个 **2.5 GB 的 `lm_head` 对 decode 的意义，比量化一张更大的 embedding 表更大**。*（全量参与 vs. 单行 gather —— 每 token 字节数相差约 150,000×。）*
4. 量化 **Q/K 带来的速度收益极小，行为损伤却不成比例**。*（流量占比小；softmax 会放大 logit 误差 —— Module 07。）*

还有一条，这是本模块真正的教训：

5. **由 checkpoint 大小算出的「峰值的 92 %」这个带宽数字，可以掩盖 20 个百分点的非带宽余量。** 先把字节记进账本，再作决定。

---

## 交付

在继续之前，为你实际在跑的一个模型做一页**字节账本**：

- 常驻字节（换算为 GB，并标注）
- 每 token 读取的字节，按张量组分项列出
- 实测 tok/s
- 反推的 `BW_effective` 及其占 datasheet 峰值的百分比
- 若同样这些字节以峰值的 90 % 搬运，预测的 tok/s

如果实测值与预测值之间的差距很大，**你接下来的任务就不是量化。** 那正说明本模块在起作用。

---


<details>
<summary>English original</summary>

**8. The optimization hierarchy**

Attach a behavior profile to every tensor group, not a size:

```text
                         Large?   Read every token?   Speed target?
   ────────────────────────────────────────────────────────────────
   Embeddings             YES           NO                 NO
   Vision tower           YES           NO                 NO
   MTP head               YES        conditional        conditional
   lm_head                YES           YES                YES
   MLP weights            HUGE          YES                YES
   Attention weights      YES           YES           YES (but see Mod. 07)
   KV cache             grows           YES          context-dependent
```

Which gives a priority order that **changes with context length**:

```text
   short context                      very long context (262 K)
   ─────────────                      ─────────────────────────
   1. MLP weights                     1. KV cache traffic
   2. lm_head                         2. MLP weights
   3. attention weights               3. lm_head
   4. (KV cache — small)              4. attention weights
```

At 262 K tokens the KV cache stops being a footnote and becomes a co-dominant traffic source. That is why long-context targets introduce an optimization problem that simply does not appear in short-context benchmarks — [Module 09](Lecture-09.md).

---

**9. Exercise — rank these before writing any code**

| Tensor | Size | Read every token? | Current precision |
|---|---:|---|---|
| Embeddings | 3 GB | No (gather) | BF16 |
| MLP | 10 GB | Yes | FP4 |
| lm_head | 2.5 GB | Yes | BF16 |
| Vision | 1 GB | No | BF16 |
| QKV | 1 GB | Yes | FP4 |

**Bad target — `Vision → FP4`.** Large storage win, zero text-decode win. `Traffic = 0` zeroes the whole opportunity product.

**Best target — `lm_head BF16 → FP8`.** Modest checkpoint reduction (2.5 → 1.25 GB) but it is 2.5 GB of *per-token* traffic, and FP8 has a native path. Expected effect using the equation from §4, with `B_token = 13.5 GB`:

```text
   before:  1650 / 13.5   =  122.2 tok/s
   after :  1650 / 12.25  =  134.7 tok/s      →  +10.2 %
```

**Dangerous target — `QKV → lower precision`.** Only 1 GB of traffic (7 % of the ledger), so the ceiling on the win is ~7 %, while [Module 07](Lecture-07.md) shows the behavioral cost is disproportionate. Low reward, high risk — the worst quadrant.

Notice the ranking is driven almost entirely by the **Traffic** and **BehaviorTolerance** terms, and not at all by tensor size. Embeddings are the largest BF16 tensor in the case study and rank last.

---

**10. The mental model**

Stop seeing this:

```text
   Qwen3.8-27B  →  27 B parameters
```

Start seeing this:

```text
   Qwen3.8-27B
   │
   ├── read EVERY token  ────────────────  the decode critical path
   │   ├── MLP weights
   │   ├── attention weights
   │   └── lm_head
   │
   ├── read SPARSELY  ───────────────────  capacity cost only
   │   └── embeddings (one row per token)
   │
   ├── CONDITIONAL  ─────────────────────  traffic depends on serving config
   │   ├── vision tower (multimodal requests only)
   │   └── MTP head (speculation enabled only)
   │
   └── GROWS WITH CONTEXT  ──────────────  the long-context term
       └── KV cache
```

and attach to every node:

```text
   bytes  ·  precision  ·  bytes/token  ·  kernel path  ·  quant error  ·  behavior sensitivity
```

That annotated tree — not the parameter count — is the object you optimize.

---

**Checkpoint**

You should now be able to explain why all four of these are simultaneously true:

1. A **3-bit model can be smaller but slower** than an NVFP4 model. *(No native tensor-core path → unpack overhead exceeds the bandwidth saved.)*
2. Removing a **1 GB vision tower saves 1 GB of VRAM and 0 tok/s**. *(Traffic = 0 during text decode.)*
3. Quantizing a **2.5 GB `lm_head` matters more for decode than quantizing a larger embedding table**. *(Full participation vs. single-row gather — a ~150,000× difference in per-token bytes.)*
4. Quantizing **Q/K yields a tiny speed win but disproportionate behavioral damage**. *(Small traffic share; softmax amplifies logit error — Module 07.)*

And one more, which is the module's real lesson:

5. **A 92 %-of-peak bandwidth number computed from checkpoint size can hide 20 points of non-bandwidth headroom.** Ledger the bytes, then decide.

---

**Ship it**

Before continuing, produce a one-page **byte ledger** for a model you actually run:

- resident bytes (converted to GB, labelled)
- per-token-read bytes, itemized by tensor group
- measured tok/s
- implied `BW_effective` and its percentage of datasheet peak
- predicted tok/s if the same bytes moved at 90 % of peak

If the gap between measured and predicted is large, **your next task is not quantization.** That is the module working.

---

</details>

## 截止当前

* **长期有效：** 算术强度、roofline 推理（性能上界模型）、`tok/s ≈ BW_eff / B_token`、常驻与每 token 的区分、机会框架。
* **2026 硬件锚点：** RTX 5090（GB202，`sm_120`）为 1792 GB/s / 32 GB GDDR7；BF16 拐点 ≈ 234 FLOP/byte。其他型号请自行重新推导两者 —— [Module 03](Lecture-03.md)。
* **案例研究测量值**（18.80 GiB 常驻、81.6 tok/s、155.75 tok/s @ 2.886 接受率）来自 [课程索引](README.md) 中链接的已发布 NVFP4 与 DSpark 构建。

---

**Next:** [Module 02 — Quantization Mathematics →](Lecture-02.md)


<details>
<summary>English original</summary>

**Current as of**

* **Timeless:** arithmetic intensity, roofline reasoning, `tok/s ≈ BW_eff / B_token`, the resident-vs-per-token distinction, the opportunity framework.
* **2026 hardware pin:** RTX 5090 (GB202, `sm_120`) at 1792 GB/s / 32 GB GDDR7; BF16 ridge ≈ 234 FLOP/byte. Re-derive both for any other part — [Module 03](Lecture-03.md).
* **Case-study measurements** (18.80 GiB resident, 81.6 tok/s, 155.75 tok/s @ 2.886 acceptance) are from the published NVFP4 and DSpark builds linked in the [course index](README.md).

---

**Next:** [Module 02 — Quantization Mathematics →](Lecture-02.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/Hardware-Aware LLM Quantization/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/Hardware-Aware%20LLM%20Quantization/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
