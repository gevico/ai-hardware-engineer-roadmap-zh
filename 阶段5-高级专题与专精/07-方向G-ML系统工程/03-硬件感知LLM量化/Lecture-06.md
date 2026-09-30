---
title: 模块 06 — 激活值离群值
description: 模块 06 — 激活值离群值
---

# 模块 06 — 激活值离群值

**合集：** [Hardware-Aware LLM Quantization](README.md) | **上一章：** [← Module 05](Lecture-05.md) | **下一章：** [Module 07 →](Lecture-07.md)

---

权重是一个固定、可检查、行为良好的张量，花一小时就能校准。激活值则是**一个分布，覆盖尚未见过的输入**，而且其中包含的结构对任何固定缩放因子的量化器都是病态的。

本模块解释这些结构、它们为何存在（它们不是缺陷——模型是有意构建它们的），以及什么做法真正有效。它也为 [Module 02](Lecture-02.md) 中的建议提供了依据：在批大小为 1 时，把激活值保持在 BF16，把精力花到别处。

---

## 学习目标

学完本模块后，你应该能够：

1. 对比权重分布与激活值分布，并解释后者为何更难处理。
2. 区分**系统性通道离群值**与**罕见的 token 尖峰**，并指出两者各自需要什么不同的补救手段。
3. 解释**巨量激活值**与 **attention sink**，以及为何移除它们会破坏模型。
4. 说明对 GEMM（矩阵-矩阵乘）而言哪些缩放轴在数学上是合法的，以及为何逐通道的激活值缩放需要做权重校正。
5. 判断在给定工作点上 W4A4 是否值得。

---

## 1. 两种不同的张量

```text
   WEIGHTS                                ACTIVATIONS
   ────────────────────────────           ──────────────────────────────
   fixed after training                   change with every input
   roughly Gaussian, zero-mean            heavy-tailed, structured
   dynamic range ~10²                     dynamic range 10³–10⁵
   calibrate once, offline                must be handled ONLINE
   outliers scattered                     outliers in FIXED CHANNELS
   you can spend an hour per tensor       you have microseconds
```

权重分布接近量化理论所假设的形态。激活值分布则不然，而且差距并不小——这是良态问题与病态问题之间的差别。

---

## 2. 结构一：系统性通道离群值

在训练好的 Transformer 中，**一小组固定不变的隐藏维度所承载的值比其余维度大 10–100×**，且对几乎每个 token 都落在相同的通道上。

```text
   hidden dimension index (d_model = 8192)
   0        1000      2000      3000      4000      5000      6000      7000
   │         │         │         │         │         │         │         │
   ▁▁▁▂▁▁▁▁▁▁█▁▁▁▁▁▂▁▁▁▁▁▁▁▁▁▁▁▁▁▁█▁▁▁▁▁▁▁▁▁▁▂▁▁▁▁▁▁▁▁▁▁▁▁▁█▁▁▁▁▁▁▁▁▁▁▁▁▁
            ▲                     ▲                          ▲
            └── the SAME channels, on nearly EVERY token ────┘
```

对 per-tensor 缩放的后果立竿见影。如果 8192 个通道中有 3 个比其余大 50×，per-tensor 的 absmax 缩放就由这 3 个通道决定，其余 8189 个通道被压缩进可表示网格最底部 2 % 的范围内。在 E2M1 的 8 个量级下，这意味着**几乎一切都量化为零**——即 [Module 02 §4](Lecture-02.md) 的湮灭，作用在张量的 99.96 % 上。

**由于这种结构是固定不变的，它是可修复的。** SmoothQuant（[Module 05](Lecture-05.md)）把逐通道的量级迁移到权重中，在那里将其吸收。逐通道的激活值缩放也能修复它——前提是它合法，这就引出了 §4。

---


<details>
<summary>English original</summary>

**Module 06 — Activation Outliers**

**Collection:** [Hardware-Aware LLM Quantization](README.md) | **Previous:** [← Module 05](Lecture-05.md) | **Next:** [Module 07 →](Lecture-07.md)

---

Weights are a fixed, inspectable, well-behaved tensor you can spend an hour calibrating. Activations are a **distribution over inputs you have not seen yet**, and they contain structures that are pathological for any fixed-scale quantizer.

This module explains those structures, why they exist (they are not defects — the model built them on purpose), and what actually works. It also justifies the recommendation from [Module 02](Lecture-02.md): at batch 1, keep activations in BF16 and spend your effort elsewhere.

---

**Learning objectives**

By the end of this module you should be able to:

1. Contrast weight and activation distributions and explain why the latter are harder.
2. Distinguish **systematic channel outliers** from **rare token spikes**, and name the different remedy each requires.
3. Explain **massive activations** and **attention sinks**, and why removing them breaks the model.
4. State which scaling axes are mathematically legal for a GEMM, and why per-channel activation scaling requires a weight correction.
5. Decide whether W4A4 is worth it for a given operating point.

---

**1. Two different tensors**

```text
   WEIGHTS                                ACTIVATIONS
   ────────────────────────────           ──────────────────────────────
   fixed after training                   change with every input
   roughly Gaussian, zero-mean            heavy-tailed, structured
   dynamic range ~10²                     dynamic range 10³–10⁵
   calibrate once, offline                must be handled ONLINE
   outliers scattered                     outliers in FIXED CHANNELS
   you can spend an hour per tensor       you have microseconds
```

Weight distributions are close to what quantization theory assumes. Activation distributions are not, and the gap is not small — it is the difference between a well-conditioned problem and a badly-conditioned one.

---

**2. Structure one: systematic channel outliers**

In a trained transformer, a **small, consistent set of hidden dimensions carries values 10–100× larger than the rest**, in the same channels, for essentially every token.

```text
   hidden dimension index (d_model = 8192)
   0        1000      2000      3000      4000      5000      6000      7000
   │         │         │         │         │         │         │         │
   ▁▁▁▂▁▁▁▁▁▁█▁▁▁▁▁▂▁▁▁▁▁▁▁▁▁▁▁▁▁▁█▁▁▁▁▁▁▁▁▁▁▂▁▁▁▁▁▁▁▁▁▁▁▁▁█▁▁▁▁▁▁▁▁▁▁▁▁▁
            ▲                     ▲                          ▲
            └── the SAME channels, on nearly EVERY token ────┘
```

The consequence for a per-tensor scale is immediate. If three channels out of 8192 are 50× larger than the rest, a per-tensor absmax scale is set by those three, and the other 8189 channels are compressed into the bottom 2 % of the representable grid. With E2M1's eight magnitudes, that means **almost everything quantizes to zero** — the [Module 02 §4](Lecture-02.md) annihilation, applied to 99.96 % of the tensor.

**Because this structure is consistent, it is fixable.** SmoothQuant ([Module 05](Lecture-05.md)) migrates the per-channel magnitude into the weights, where it can be absorbed. Per-channel activation scaling would also fix it — if it were legal, which brings us to §4.

---

</details>

## 3. 结构二：稀有的 token 尖峰（以及它们为何承重）

第二种结构在性质上不同。**少数 token 位置**——往往就是第一个 token——产生的激活值量级是中位数的数千倍，且集中在少数几个维度上。这就是近期文献所描述的 **massive activations**，它们与 **attention sink** 密切相关。

```text
   activation magnitude by token position
   ▲
   │ █
   │ █
   │ █                                                    ← position 0 (often BOS):
   │ █                                                       magnitude 1000×+ the rest
   │ █
   │ █ ▁ ▂ ▁ ▁ ▂ ▁ ▁ ▁ ▂ ▁ ▁ ▁ ▁ ▂ ▁ ▁ ▁ ▁ ▁ ▂ ▁ ▁ ▁ ▁
   └──┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴──▶ token position
      0 1 2 3 ...
```

**模型为何会形成它们。** softmax attention 必须在每个 head、每个位置上，把总量恰好为 1 的概率质量分配到各个 key 上——即使某个 head 没有任何想要关注的东西。模型需要一个「以上皆非」的选项，于是它指定一个 token（通常是第一个）作为 **attention sink**，把多余的质量倾倒在它上面。massive activation 就是让该 token 稳定具有吸引力的机制。

**两个在此处至关重要的后果：**

1. **不能把它们裁掉。** 它们不是噪声。抑制 sink 后，attention 质量会被重新分配到该 head 刻意忽略的 token 上，从而破坏输出。激进裁剪激活值的量化方案损害的正是这一机制，而且损害表现为长上下文下的不连贯，而非困惑度的变化。

2. **百分位校准抓不住它们。** 它们只出现在百分之零点几的位置上。在校准集上算出的 99.9 百分位阈值会把 sink 裁掉。这正是标准 recipe 明显出错的场合。

```text
   channel outliers   →  SYSTEMATIC   →  migrate them (SmoothQuant)         ✅
   token spikes       →  RARE + LOAD-BEARING  →  must be REPRESENTED, not clipped
                                             →  needs per-token dynamic scaling
                                                or keep-in-higher-precision
```

**这就是为什么这两种结构需要不同的补救办法，也是为什么把「激活值离群点」当作单一问题来处理会失败。**

---

## 4. 哪些缩放轴合法

这是多数论述会跳过的部分，它解释了为什么激活值量化所受的约束与权重量化不同。

对于 `Y = X · W`，其中 `X : [T, K]` 和 `W : [K, N]`：

```text
   Y[t, n]  =  Σ_k  X[t, k] · W[k, n]
```

**对 X 做 per-token 缩放（按行）——合法。**

```text
   X[t, :] = a_t · X̂[t, :]     ⟹     Y[t, n] = a_t · Σ_k X̂[t,k] W[k,n]
```

`a_t` 可以干净地从整个输出行中提取出来。之后再反量化即可。**这是免费的**，也正是 per-token 动态激活值缩放成为标准做法的原因。

**对 W 做 per-output-channel 缩放（按列）——合法。**

```text
   W[:, n] = b_n · Ŵ[:, n]     ⟹     Y[t, n] = b_n · Σ_k X[t,k] Ŵ[k,n]
```

它可以从输出列中提取出来。同样是免费的。

**对 X 做 per-input-channel 缩放（按列，索引 `k`）——单独使用不合法。**

```text
   X[t, k] = c_k · X̂[t, k]     ⟹     Y[t,n] = Σ_k c_k X̂[t,k] W[k,n]
```

`c_k` 位于 **对 `k` 的求和内部**。它无法提取出来。在 GEMM 之后无法撤销。

```text
   The reduction axis is the one you cannot scale freely.
   ──────────────────────────────────────────────────────────
   And it is exactly the axis where the channel outliers live.
```

使用 per-input-channel 因子的唯一办法，是在另一个操作数上把它抵消掉：

```text
   Y = (X · diag(c)⁻¹) · (diag(c) · W)
```

这**恰恰就是 SmoothQuant**（[模块 05 §4.3](Lecture-05.md)）。现在可以看出它并非启发式方法——它是把 per-input-channel 因子纳入计算的唯一合法构造。

所以工具箱是：

| 结构 | 合法补救 |
|---|---|
| 通道离群点（归约轴） | SmoothQuant 式迁移进 W，或细粒度分块 |
| token 尖峰（行轴） | per-token 动态缩放——免费且有效 |
| 两者兼具 | per-token 缩放 **与** 迁移，外加小分块 |

---


<details>
<summary>English original</summary>

**3. Structure two: rare token spikes (and why they are load-bearing)**

The second structure is different in kind. A **small number of token positions** — very often the first token — produce activations with magnitudes thousands of times the median, concentrated in a couple of dimensions. These are the **massive activations** described in the recent literature, and they are closely tied to **attention sinks**.

```text
   activation magnitude by token position
   ▲
   │ █
   │ █
   │ █                                                    ← position 0 (often BOS):
   │ █                                                       magnitude 1000×+ the rest
   │ █
   │ █ ▁ ▂ ▁ ▁ ▂ ▁ ▁ ▁ ▂ ▁ ▁ ▁ ▁ ▂ ▁ ▁ ▁ ▁ ▁ ▂ ▁ ▁ ▁ ▁
   └──┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴─┴──▶ token position
      0 1 2 3 ...
```

**Why the model builds them.** Softmax attention must distribute a total probability mass of exactly 1 across the keys, on every head, at every position — even when a head has nothing it wants to attend to. The model needs a "none of the above" option, so it designates a token (usually the first) as an **attention sink** and dumps surplus mass there. The massive activation is the mechanism that makes that token reliably attractive.

**Two consequences that matter enormously here:**

1. **You cannot clip these away.** They are not noise. Suppress the sink and attention mass gets redistributed onto tokens the head was deliberately ignoring, which corrupts the output. Quantization schemes that aggressively clip activations damage exactly this mechanism, and the damage shows up as incoherence at long context rather than as a perplexity change.

2. **Percentile calibration will not catch them.** They occur on a fraction of a percent of positions. A 99.9th-percentile threshold computed over a calibration set clips the sink. This is a case where the standard recipe is actively wrong.

```text
   channel outliers   →  SYSTEMATIC   →  migrate them (SmoothQuant)         ✅
   token spikes       →  RARE + LOAD-BEARING  →  must be REPRESENTED, not clipped
                                             →  needs per-token dynamic scaling
                                                or keep-in-higher-precision
```

**This is why the two structures need different remedies, and why treating "activation outliers" as one problem fails.**

---

**4. Which scaling axes are legal**

This is the part most treatments skip, and it explains why activation quantization is constrained in a way weight quantization is not.

For `Y = X · W` with `X : [T, K]` and `W : [K, N]`:

```text
   Y[t, n]  =  Σ_k  X[t, k] · W[k, n]
```

**Per-token scaling of X (per row) — LEGAL.**

```text
   X[t, :] = a_t · X̂[t, :]     ⟹     Y[t, n] = a_t · Σ_k X̂[t,k] W[k,n]
```

`a_t` factors cleanly out of the whole output row. You can dequantize afterwards. **This is free**, and it is why per-token dynamic activation scaling is the standard.

**Per-output-channel scaling of W (per column) — LEGAL.**

```text
   W[:, n] = b_n · Ŵ[:, n]     ⟹     Y[t, n] = b_n · Σ_k X[t,k] Ŵ[k,n]
```

Factors out of the output column. Also free.

**Per-input-channel scaling of X (per column, index `k`) — NOT LEGAL alone.**

```text
   X[t, k] = c_k · X̂[t, k]     ⟹     Y[t,n] = Σ_k c_k X̂[t,k] W[k,n]
```

`c_k` sits **inside the summation over `k`**. It does not factor out. You cannot undo it after the GEMM.

```text
   The reduction axis is the one you cannot scale freely.
   ──────────────────────────────────────────────────────────
   And it is exactly the axis where the channel outliers live.
```

The only way to use a per-input-channel factor is to cancel it on the other operand:

```text
   Y = (X · diag(c)⁻¹) · (diag(c) · W)
```

which is **precisely SmoothQuant** ([Module 05 §4.3](Lecture-05.md)). Now you can see it is not a heuristic — it is the unique legal construction for putting a per-input-channel factor into the computation.

So the toolkit is:

| Structure | Legal remedy |
|---|---|
| Channel outliers (reduction axis) | SmoothQuant-style migration into W, or fine-grained blocks |
| Token spikes (row axis) | per-token dynamic scaling — free and effective |
| Both | per-token scaling **and** migration, plus small blocks |

---

</details>

## 5. 块缩放为什么有效，效果有多大

NVFP4 的 16 元素块提供了第三重防线，完全不需要任何代数：**离群值只会污染它自己所在的块。**

```text
   PER-TENSOR scale, one 50× outlier in 8192 values:
   ├──────────────────────── all 8192 values share one window ─────────────────────┤
        → 8191 values crushed into the bottom 2 % of the grid  → mass annihilation

   BLOCK-16 scale, same outlier:
   ├──16──┤├──16──┤├──16──┤ ... ├──16──┤
                    ▲
                    └─ only THESE 16 values are affected;  16/8192 = 0.2 % of the tensor
```

破坏范围由块大小界定，而不是由张量大小界定——对 `d_model = 8192` 而言，**爆炸半径缩小了 512×**。这才是细粒度块格式取代 per-tensor INT8 的更深层原因：不是位宽，而是**爆炸半径**。

这也解释了为什么 NVFP4 与 MXFP4 的块大小差异（16 对 32）对激活值的影响比对权重更大。离群值就出在激活值上。

---

## 6. W4A4 值得吗？——工作点给出的回答

以上全是成本侧。下面是收益侧，来自 [Module 01](Lecture-01.md) 的 roofline（性能上界模型）。

**在 batch 1 时：**

```text
   weight traffic per token     ≈  15.88 GB
   activation traffic per token ≈  a few hundred KB
                                   ────────────────
   activation share of B_token  ≈  0.01 %
```

把激活值量化到 FP4 只会让 `B_token` 下降约 0.005 %。算力收益（相对 FP8 有 2× 的 tensor core 吞吐）作用在一个位于**拐点以下 263×** 的工作负载上。所以：

```text
   W4A4 benefit at batch 1  ≈  0
   W4A4 risk    at batch 1  =  every structure in this module
```

> **RTX 5090 上单用户 decode（逐 token 生成阶段）的结论：不要量化激活值。** 用 W4A16（NVFP4 权重，BF16 激活值）。这不是保守——这就是 roofline。

**W4A4 什么时候才划算：**

| 条件 | 原因 |
|---|---|
| 大批推理服务（B ≳ 100） | 逼近拐点；算力开始成为瓶颈 |
| 长上下文 prefill | prefill（首字前的整段计算）天生就是算力受限 |
| 激活值会被重复读取的融合 kernel | 激活值的访问流量不再可忽略 |
| 激活值 buffer 带来的显存容量压力 | 大批 × 长序列 |

注意，**投机解码会把你的有效批大小抬高**到每次验证 pass 的 `K+1`（[Module 10](Lecture-10.md)）。在 `K = 3` 时那就是 batch 4——仍在拐点以下 60×。投机并不改变这个结论。

---

## 7. 自己动手测

对你的模型，不要凭信任接受以上任何一条。诊断代码只有三十行：

```python
import torch
from collections import defaultdict

stats = defaultdict(list)

def make_hook(name):
    def hook(_mod, inp, _out):
        x = inp[0].detach().float()                  # [batch, seq, hidden]
        stats[name].append({
            "per_channel_absmax": x.abs().amax(dim=(0, 1)).cpu(),   # [hidden]
            "per_token_absmax":   x.abs().amax(dim=-1).flatten().cpu(),
            "median":             x.abs().median().item(),
        })
    return hook

for name, mod in model.named_modules():
    if isinstance(mod, torch.nn.Linear):
        mod.register_forward_hook(make_hook(name))

# ... run 32 calibration sequences ...

for name, records in stats.items():
    ch  = torch.stack([r["per_channel_absmax"] for r in records]).amax(0)
    tok = torch.cat([r["per_token_absmax"] for r in records])
    med = sum(r["median"] for r in records) / len(records)

    print(f"{name:50s}  "
          f"channel_ratio={ch.max()/ch.median():7.1f}  "   # >10 → channel outliers
          f"token_ratio={tok.max()/tok.median():8.1f}  "   # >100 → massive activations
          f"outlier_channels={(ch > 10*ch.median()).sum().item():4d}")
```

读法如下：

| 信号 | 阈值 | 诊断 | 处理 |
|---|---|---|---|
| `channel_ratio` | > 10 | 系统性的通道离群值 | SmoothQuant / 更小的块 |
| `token_ratio` | > 100 | massive activations / sink | per-token 动态缩放；**绝不截断** |
| `outlier_channels` | 小（1–10） | 经典 Transformer 结构 | 符合预期；不是 bug |
| 两者都低 | — | 这一层很简单 | 放心量化 |

在选方法**之前**先跑一遍。它正是 [Module 05 §4](Lecture-05.md) 的选择树所要求的诊断，而且只要十分钟。

---

## 检查点

现在你应该能够：

1. 说出两种激活值离群结构，并给出各自需要的不同处理。
2. 解释 attention sink 为什么存在，以及为什么截断它们会破坏长上下文行为。
3. 证明哪些 GEMM 缩放轴是合法的，并从非法的那种推导出 SmoothQuant 的形式。
4. 量化 block-16 缩放相对 per-tensor 缩放在爆炸半径上的优势。
5. 从 roofline 出发论证在 batch 1 时 W4A16 优于 W4A4，并说出会翻转该结论的条件。
6. 读懂通道/token 比值表，并据此选出方法。

---


<details>
<summary>English original</summary>

**5. Why block scaling helps, and how much**

NVFP4's 16-element blocks provide a third defence that needs no algebra at all: **an outlier only contaminates its own block.**

```text
   PER-TENSOR scale, one 50× outlier in 8192 values:
   ├──────────────────────── all 8192 values share one window ─────────────────────┤
        → 8191 values crushed into the bottom 2 % of the grid  → mass annihilation

   BLOCK-16 scale, same outlier:
   ├──16──┤├──16──┤├──16──┤ ... ├──16──┤
                    ▲
                    └─ only THESE 16 values are affected;  16/8192 = 0.2 % of the tensor
```

The damage is bounded by the block size, not by the tensor size — a **512× reduction in blast radius** for `d_model = 8192`. This is the deeper reason fine-grained block formats displaced per-tensor INT8: not the bit-width, the **blast radius**.

It is also why the NVFP4-vs-MXFP4 block-size difference (16 vs 32) matters more for activations than for weights. Activations are where the outliers are.

---

**6. Is W4A4 worth it? — the operating-point answer**

Everything above is the cost side. Here is the benefit side, from [Module 01](Lecture-01.md)'s roofline.

**At batch 1:**

```text
   weight traffic per token     ≈  15.88 GB
   activation traffic per token ≈  a few hundred KB
                                   ────────────────
   activation share of B_token  ≈  0.01 %
```

Quantizing activations to FP4 reduces `B_token` by ~0.005 %. The compute benefit (2× tensor-core throughput over FP8) applies to a workload sitting **263× below the ridge point**. So:

```text
   W4A4 benefit at batch 1  ≈  0
   W4A4 risk    at batch 1  =  every structure in this module
```

> **Verdict for single-user decode on RTX 5090: do not quantize activations.** Use W4A16 (NVFP4 weights, BF16 activations). This is not caution — it is the roofline.

**When W4A4 does pay:**

| Condition | Why |
|---|---|
| Large-batch serving (B ≳ 100) | approaching the ridge; compute starts to bind |
| Long-context prefill | prefill is compute-bound by construction |
| Fused kernels where activations are re-read | activation traffic stops being negligible |
| Memory-capacity pressure from activation buffers | large batch × long sequence |

Note that **speculative decoding raises your effective batch** to `K+1` per verification pass ([Module 10](Lecture-10.md)). At `K = 3` that is batch 4 — still 60× below the ridge. Speculation does not change this verdict.

---

**7. Measure it yourself**

Do not take any of this on faith for your model. The diagnostic is thirty lines:

```python
import torch
from collections import defaultdict

stats = defaultdict(list)

def make_hook(name):
    def hook(_mod, inp, _out):
        x = inp[0].detach().float()                  # [batch, seq, hidden]
        stats[name].append({
            "per_channel_absmax": x.abs().amax(dim=(0, 1)).cpu(),   # [hidden]
            "per_token_absmax":   x.abs().amax(dim=-1).flatten().cpu(),
            "median":             x.abs().median().item(),
        })
    return hook

for name, mod in model.named_modules():
    if isinstance(mod, torch.nn.Linear):
        mod.register_forward_hook(make_hook(name))

# ... run 32 calibration sequences ...

for name, records in stats.items():
    ch  = torch.stack([r["per_channel_absmax"] for r in records]).amax(0)
    tok = torch.cat([r["per_token_absmax"] for r in records])
    med = sum(r["median"] for r in records) / len(records)

    print(f"{name:50s}  "
          f"channel_ratio={ch.max()/ch.median():7.1f}  "   # >10 → channel outliers
          f"token_ratio={tok.max()/tok.median():8.1f}  "   # >100 → massive activations
          f"outlier_channels={(ch > 10*ch.median()).sum().item():4d}")
```

Read it like this:

| Signal | Threshold | Diagnosis | Remedy |
|---|---|---|---|
| `channel_ratio` | > 10 | systematic channel outliers | SmoothQuant / smaller blocks |
| `token_ratio` | > 100 | massive activations / sinks | per-token dynamic scaling; **never clip** |
| `outlier_channels` | small (1–10) | classic transformer structure | expected; not a bug |
| both low | — | this layer is easy | quantize it freely |

Run this **before** choosing a method. It is the diagnosis that [Module 05 §4](Lecture-05.md)'s selection tree requires, and it takes ten minutes.

---

**Checkpoint**

You should now be able to:

1. Name the two activation-outlier structures and give the different remedy each needs.
2. Explain why attention sinks exist and why clipping them corrupts long-context behavior.
3. Prove which GEMM scaling axes are legal, and derive SmoothQuant's form from the illegal one.
4. Quantify the blast-radius advantage of block-16 scaling over per-tensor scaling.
5. Justify W4A16 over W4A4 at batch 1 from the roofline, and name the conditions that flip the verdict.
6. Read a channel/token ratio table and pick a method from it.

---

</details>

## 交付

为一个模型产出一份 **激活值画像**：§7 中的逐层表格、最差层的 per-channel absmax 直方图、per-token absmax 随位置变化的曲线（显示位置 0 处的 sink），以及一段诊断文字，点明你的模型具备哪些结构、你选择了哪种补救手段。

如果 `token_ratio` 超过 100，而你的校准用的是 99.9 百分位，那么 **你已经发现了一个 bug** —— 那正是产物。

---

## 时效性

* **不随时间改变：** 两种 outlier 结构、合法轴证明、影响半径论证、batch-1 下的 W4A4 结论。
* **2026 年的认识：** massive activation 及其与 attention sink 的关联已是既定结论（StreamingLLM 的 sink 观察；massive activations 一系工作表明，少数几个维度充当了习得的 attention bias）。把“不要裁剪 sink”当作既定实践。
* **待刷新内容：** runtime 是否在 `sm_120` 上暴露 NVFP4 的 per-token 动态激活值缩放。若不暴露，则无论本文的分析如何，W4A4 对你都不在考虑范围内。

---

**下一节：** [Module 07 — Layer Sensitivity →](Lecture-07.md)


<details>
<summary>English original</summary>

**Ship it**

Produce an **activation profile** for one model: the per-layer table from §7, a histogram of per-channel absmax for the worst layer, a plot of per-token absmax versus position (showing the sink at position 0), and a one-paragraph diagnosis naming which structures your model has and which remedy you selected.

If your `token_ratio` exceeds 100 and your calibration used a 99.9th percentile, **you have already found a bug** — and that is the artifact.

---

**Current as of**

* **Timeless:** the two outlier structures, the legal-axis proof, the blast-radius argument, the batch-1 W4A4 verdict.
* **2026 understanding:** massive activations and their link to attention sinks are established results (StreamingLLM's sink observation; the massive-activations line of work showing a handful of dimensions acting as learned attention biases). Treat "do not clip the sink" as settled practice.
* **Refresh surface:** whether runtimes expose per-token dynamic activation scaling for NVFP4 on `sm_120`. If they do not, W4A4 is off the table for you regardless of the analysis here.

---

**Next:** [Module 07 — Layer Sensitivity →](Lecture-07.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/Hardware-Aware LLM Quantization/Lecture-06.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/Hardware-Aware%20LLM%20Quantization/Lecture-06.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
