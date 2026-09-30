---
title: 模块 07 — 层敏感度：为什么 Q/K 会崩，而 O 与 MLP 能幸存
description: 模块 07 — 层敏感度：为什么 Q/K 会崩，而 O 与 MLP 能幸存
---

# 模块 07 — 层敏感度：为什么 Q/K 会崩，而 O 与 MLP 能幸存

**合集：** [Hardware-Aware LLM Quantization](README.md) | **上一节：** [← 模块 06](Lecture-06.md) | **下一节：** [模块 08 →](Lecture-08.md)

---

每个从业者最终都会发现同一条经验规律：**MLP 和输出投影可以随便量化；碰 Q 和 K 则后果自负。** 它通常作为口口相传的经验流传，有时还配上一句对激活值离群点的含糊解释。

这种含糊解释是错的，而搞清它错在哪里，能换来实打实的吞吐。机制并不在 Q/K 取值的*分布*中——而在**消费它们的那个算子**里。进入线性算子的量化误差会被平均掉。进入 softmax 的量化误差会被**指数化**。

本模块推导这一点，把它与实测的案例结果联系起来，并给出一套直接测量敏感度的流程，而不是从统计量上去猜。

---

## 学习目标

学完本模块，你应当能够：

1. 推导为什么逐元素量化误差经过线性投影会*被平均掉*，而经过 attention 会*被放大*。
2. 量化给定的 Q/K 相对误差所产生的 attention 权重失真。
3. 解释为什么 GQA 让 K 成为整个模型中流量与损伤之比最差的一项。
4. 解释为什么**敏感度不能由离群点幅度预测**，以及这对方法选择意味着什么。
5. 跑一次留一法敏感度扫描，并把它变成一张精度分配表。

---

## 1. 量化误差的两种命运

两条路径的起点完全相同：一个权重张量被量化，对 NVFP4 而言产生逐元素相对误差 `η ≈ 0.10`（[模块 02 §5](Lecture-02.md)）。

### 路径 A — 经过线性投影（O、MLP、V）

输出是一个长度为 `K` 的点积。误差近似零均值且相互独立，因此它们按平方相加，而信号按线性相加：

```text
   relative output error  ≈  η / √K
```


| 层 | `K` | 由 η = 0.10 引起的输出误差 |
|---|---:|---:|
| O 投影 | 8192 | **0.11 %** |
| MLP 下投影 | 13,870 | **0.09 %** |

随后结果被**加进残差流**，而残差流到网络中段时已经累积出很大的范数。对一个巨大累加和的单项贡献施加 0.1 % 的加性扰动，还会被进一步稀释。

```text
   x  ←  x  +  f(x) + δ            δ/|x| ≪ 0.1 %
         └── large ──┘  └ tiny ┘

   LINEAR · ADDITIVE · DILUTED BY THE RESIDUAL · AVERAGES OVER K TERMS
```


**这就是 MLP 与 O 投影能很好地容忍 4-bit 量化的原因。** 并不是它们的权重有什么特别，而是四种彼此独立的机制都在帮你的忙。

### 路径 B — 经过 attention 分数（Q、K）

现在同样的误差进入一个随后会被**指数化**的点积。

```text
   s_i  =  (q · k_i) / √d_head              attention logit
   p    =  softmax(s)                       attention weights
```


扰动 `q → q + δ` 和 `k_i → k_i + ε_i`：

```text
   Δs_i  =  (δ · k_i  +  q · ε_i) / √d_head
```


关键的一步在这里。对于分量满足 iid 的 `q, k`，`q·k` 与 `δ·k` 都以 `σ²√d` 缩放，因此*相对* logit 误差为 `≈ η√2`——**√K 的平均化收益并不出现**，因为该误差是相对于一个本身就是同长度点积的量。拯救了路径 A 的那种平均化在这里被抵消掉了。

所以**绝对** logit 误差为：

```text
   Δs  ≈  η · √2 · |s|
```


而 softmax 把绝对的 logit 差值变成**乘性**的权重比：

```text
   p_i / p_j  =  exp(s_i − s_j)      ⟹      a shift Δs changes the ratio by  exp(Δs)
```


| η | 典型 \|logit\| | Δs | **attention 权重被扭曲的倍数** |
|---:|---:|---:|---:|
| 0.05 | 5 | 0.35 | **×1.42** |
| 0.05 | 10 | 0.71 | **×2.03** |
| 0.05 | 20 | 1.41 | **×4.11** |
| 0.10 | 10 | 1.41 | **×4.11** |
| 0.10 | 20 | 2.83 | **×16.9** |

```text
   PATH A  (O, MLP)          PATH B  (Q, K)
   ─────────────────         ─────────────────────────
   error / √K                error × exp(·)
   0.10 → 0.001              0.10 → attention mass moves by 2–17×
   AVERAGES DOWN             AMPLIFIES
```


**这就是全部的答案。** 两种命运相差三个数量级，而原因在算子，不在数据。

有两个推论值得牢记：

* **敏感度随 logit 幅度增长**，而 logit 幅度随 `d_head` 增长，也随训练进程增长。更大、训练得更好的模型对 Q/K *更*敏感，而不是更不敏感。
* **V 在路径 A 上，不在路径 B 上。** V 被加权和消费，而不是被 softmax 消费。`V` 量化起来跟 `O` 一样——随便量化。当人们说"attention 敏感"时，他们特指 Q 和 K；把 V 也算进去只会白白付出搬运代价。

---


<details>
<summary>English original</summary>

**Module 07 — Layer Sensitivity: Why Q/K Break While O and MLP Survive**

**Collection:** [Hardware-Aware LLM Quantization](README.md) | **Previous:** [← Module 06](Lecture-06.md) | **Next:** [Module 08 →](Lecture-08.md)

---

Every practitioner eventually discovers the same empirical rule: **quantize the MLP and output projection freely; touch Q and K at your peril.** It is usually passed on as folklore, sometimes with a hand-wave about activation outliers.

The hand-wave is wrong, and knowing why it is wrong is worth real throughput. The mechanism is not in the *distribution* of Q/K values — it is in the **operator that consumes them**. Quantization error into a linear operator averages down. Quantization error into a softmax gets **exponentiated**.

This module derives that, connects it to the measured case-study result, and gives you a protocol for measuring sensitivity directly instead of guessing it from statistics.

---

**Learning objectives**

By the end of this module you should be able to:

1. Derive why per-element quantization error *averages down* through a linear projection but *amplifies* through attention.
2. Quantify the attention-weight distortion produced by a given relative error on Q/K.
3. Explain why GQA makes K the worst traffic-to-damage ratio in the entire model.
4. Explain why **sensitivity is not predicted by outlier magnitude**, and what that implies for method selection.
5. Run a leave-one-out sensitivity sweep and turn it into a precision-allocation table.

---

**1. The two fates of a quantization error**

Both paths start identically: a weight tensor is quantized, producing per-element relative error `η ≈ 0.10` for NVFP4 ([Module 02 §5](Lecture-02.md)).

**Path A — through a linear projection (O, MLP, V)**

The output is a dot product of length `K`. Errors are approximately zero-mean and independent, so they add in quadrature while the signal adds linearly:

```text
   relative output error  ≈  η / √K
```

| Layer | `K` | Output error from η = 0.10 |
|---|---:|---:|
| O projection | 8192 | **0.11 %** |
| MLP down projection | 13,870 | **0.09 %** |

Then the result is **added into the residual stream**, which by mid-network has accumulated a large norm. An additive perturbation of 0.1 % on one contribution to a large running sum is diluted further still.

```text
   x  ←  x  +  f(x) + δ            δ/|x| ≪ 0.1 %
         └── large ──┘  └ tiny ┘

   LINEAR · ADDITIVE · DILUTED BY THE RESIDUAL · AVERAGES OVER K TERMS
```

**This is why MLP and O projections tolerate 4-bit quantization well.** It is not that their weights are special. It is that four separate mechanisms all work in your favour.

**Path B — through attention scores (Q, K)**

Now the same error enters a dot product that is then **exponentiated**.

```text
   s_i  =  (q · k_i) / √d_head              attention logit
   p    =  softmax(s)                       attention weights
```

Perturb `q → q + δ` and `k_i → k_i + ε_i`:

```text
   Δs_i  =  (δ · k_i  +  q · ε_i) / √d_head
```

Here is the critical step. For `q, k` with iid components, both `q·k` and `δ·k` scale as `σ²√d`, so the *relative* logit error is `≈ η√2` — **the √K averaging benefit does not appear**, because the error is relative to a quantity that is itself a dot product of the same length. The averaging that saved Path A cancels out.

So the **absolute** logit error is:

```text
   Δs  ≈  η · √2 · |s|
```

And softmax turns absolute logit differences into **multiplicative** weight ratios:

```text
   p_i / p_j  =  exp(s_i − s_j)      ⟹      a shift Δs changes the ratio by  exp(Δs)
```

| η | typical \|logit\| | Δs | **attention weight distorted by** |
|---:|---:|---:|---:|
| 0.05 | 5 | 0.35 | **×1.42** |
| 0.05 | 10 | 0.71 | **×2.03** |
| 0.05 | 20 | 1.41 | **×4.11** |
| 0.10 | 10 | 1.41 | **×4.11** |
| 0.10 | 20 | 2.83 | **×16.9** |

```text
   PATH A  (O, MLP)          PATH B  (Q, K)
   ─────────────────         ─────────────────────────
   error / √K                error × exp(·)
   0.10 → 0.001              0.10 → attention mass moves by 2–17×
   AVERAGES DOWN             AMPLIFIES
```

**That is the whole answer.** Three orders of magnitude separate the two fates, and the cause is the operator, not the data.

Two corollaries worth internalizing:

* **Sensitivity grows with logit magnitude**, which grows with `d_head` and over the course of training. Bigger, better-trained models are *more* Q/K-sensitive, not less.
* **V is on Path A, not Path B.** V is consumed by a weighted sum, not a softmax. `V` quantizes like `O` — freely. When people say "attention is sensitive" they mean Q and K specifically; lumping V in with them costs you traffic for no reason.

---

</details>

## 2. RoPE 把幅度误差变成相位误差

旋转位置编码在位置 `m` 处以角度 `m·θ_j` 旋转 `(q_{2j}, q_{2j+1})` 对。attention logit 变成各对的求和：

```text
   r_q,j · r_k,j · cos( (m − n)·θ_j  +  φ_j )
```


其中 `r` 是各对的幅度，`φ` 是相对相位。量化扰动笛卡尔分量，这**同时**扰动了每一对的幅度**和角度**：

```text
   (x, y) ──quantize──▶ (x + δ, y)          angular error  Δφ ≈ δ / r
                                             ▲
                    small-magnitude pairs suffer LARGE angular error
```


attention 读取的是相位。所以 NVFP4 的逐块缩放与 RoPE 的相互作用有特定方式：一个 16 元素块跨越 8 个旋转对，块缩放由其中最大的那个决定——因此高幅度块内部的低幅度对同时得到粗糙的幅度分辨率*和*较大的角度误差。

这给出一个**可检验的预测**：

> Q/K 量化损伤应当**随上下文长度增长**，因为 `(m−n)·θ_j` 项使 attention 在大的相对位置上对相位更敏感。

不要凭空相信这一点——§5 的协议包含了检验它的扫描。如果你的模型面向 262 K 上下文（[Module 09](Lecture-09.md)），那么在 4 K 下测得的 Q/K 敏感度并不是决定部署的那个数字。

---

## 3. GQA 让 K 糟糕得独一无二

现在再来看流量这一侧。对于案例研究的几何参数（`d_model = 8192`，48 层，8 个 KV head × 128，`d_ff ≈ 13,870`）：

| 张量 | 每层参数量 | 占主体的比例 | 流量 | **占 `B_token` 的比例** |
|---|---:|---:|---:|---:|
| MLP（3 个矩阵） | 340.9 M | 69.3 % | 9.20 GB | **57.96 %** |
| Q | 67.1 M | 13.6 % | 1.81 GB | **11.41 %** |
| O | 67.1 M | 13.6 % | 1.81 GB | **11.41 %** |
| **K + V** | **16.8 M** | **3.4 %** | **0.45 GB** | **2.85 %** |

在分组查询注意力下，`K` 和 `V` 由 `G = n_q / n_kv = 8` 个 query head 共享。于是：

```text
   K is the SMALLEST tensor group in the model  (2.85 % of B_token, and V is half of that)
   K feeds 8 query heads                        (one error → 8 corrupted attention distributions)
   K enters via Path B                          (exponential amplification)
```

```text
                 traffic saved          behavioral damage
   MLP    ████████████████████████       ▏
   O      ████▌                          ▏
   Q      ████▌                          ████████
   K      ▊                              ████████████████████
   V      ▊                              ▏
```

**在模型的所有张量中，K 的收益/风险比是最差的，而且差距悬殊。** 把 K 量化到 4 bit 只为你换来不到 1.5 % 的字节预算，代价却是被放大的、又被 GQA 成倍放大的 attention 失真。这根本不是相差无几的取舍。

---

## 4. 实测的案例研究

预测与数据相互印证：

```text
   MLP + O quantized          147.87 tok/s     acceptance 2.792
   MLP + O + QKV quantized    150.73 tok/s     acceptance 2.546
                              ──────────       ────────────────
                              +1.9 % speed     −8.8 % acceptance
```

两个数字都与理论相符：

* **速度：+1.9 %。** Q + K + V 合计占 `B_token` 的 14.3 %；把它们从一个已经压缩过的基线转换过来，只能带来个位数的小幅增益。§3 的表格给出了可能收益的上界，而实测落在这个范围内。
* **行为：−8.8 % 接受率。** 路径 B 的放大，再乘以 GQA 共享。接受长度直接反映了目标分布移动了多少（[Module 08](Lecture-08.md)）——而它移动了很多。

**这笔交易是用 8.8 % 的接受率换 1.9 % 的吞吐，而且很不划算。** 实际上比看上去更糟：[Module 10](Lecture-10.md) 表明接受率会反馈到吞吐上，所以那 1.9 % 中有一部分会直接还回去。

正确的配置把 **MLP + O + V** 留在 NVFP4，让 **Q + K** 保持宽位宽。代价：`B_token` 中约 11 % 未压缩。收益：完全避开了接受率的退化。

---


<details>
<summary>English original</summary>

**2. RoPE turns magnitude error into phase error**

Rotary position embedding rotates `(q_{2j}, q_{2j+1})` pairs by angle `m·θ_j` at position `m`. The attention logit becomes a sum over pairs of

```text
   r_q,j · r_k,j · cos( (m − n)·θ_j  +  φ_j )
```

where `r` are pair magnitudes and `φ` the relative phase. Quantization perturbs the Cartesian components, which perturbs **both** the magnitude and the **angle** of each pair:

```text
   (x, y) ──quantize──▶ (x + δ, y)          angular error  Δφ ≈ δ / r
                                             ▲
                    small-magnitude pairs suffer LARGE angular error
```

Attention reads the phase. So NVFP4's per-block scaling interacts with RoPE in a specific way: a 16-element block spans 8 rotary pairs, and the block scale is set by the largest of them — so low-magnitude pairs inside a high-magnitude block get both coarse magnitude resolution *and* large angular error.

This yields a **testable prediction**:

> Q/K quantization damage should **grow with context length**, because the `(m−n)·θ_j` term makes attention more phase-sensitive at large relative positions.

Do not take that on faith — §5's protocol includes the sweep that checks it. If your model is destined for 262 K context ([Module 09](Lecture-09.md)), a Q/K sensitivity measured at 4 K is not the number that governs deployment.

---

**3. GQA makes K uniquely bad**

Now add the traffic side. For the case-study geometry (`d_model = 8192`, 48 layers, 8 KV heads × 128, `d_ff ≈ 13,870`):

| Tensor | Params/layer | Share of body | Traffic | **Share of `B_token`** |
|---|---:|---:|---:|---:|
| MLP (3 mats) | 340.9 M | 69.3 % | 9.20 GB | **57.96 %** |
| Q | 67.1 M | 13.6 % | 1.81 GB | **11.41 %** |
| O | 67.1 M | 13.6 % | 1.81 GB | **11.41 %** |
| **K + V** | **16.8 M** | **3.4 %** | **0.45 GB** | **2.85 %** |

With grouped-query attention, `K` and `V` are shared across `G = n_q / n_kv = 8` query heads. So:

```text
   K is the SMALLEST tensor group in the model  (2.85 % of B_token, and V is half of that)
   K feeds 8 query heads                        (one error → 8 corrupted attention distributions)
   K enters via Path B                          (exponential amplification)
```

```text
                 traffic saved          behavioral damage
   MLP    ████████████████████████       ▏
   O      ████▌                          ▏
   Q      ████▌                          ████████
   K      ▊                              ████████████████████
   V      ▊                              ▏
```

**K has the worst reward/risk ratio of any tensor in the model, by a wide margin.** Quantizing K to 4 bits buys you under 1.5 % of your byte budget and pays for it with amplified, GQA-multiplied attention distortion. This is not a close call.

---

**4. The measured case study**

The prediction meets the data:

```text
   MLP + O quantized          147.87 tok/s     acceptance 2.792
   MLP + O + QKV quantized    150.73 tok/s     acceptance 2.546
                              ──────────       ────────────────
                              +1.9 % speed     −8.8 % acceptance
```

Both numbers match theory:

* **Speed: +1.9 %.** Q + K + V together are 14.3 % of `B_token`; converting them from an already-compressed baseline yields a small single-digit gain. The table in §3 bounds the possible win, and the measurement lands inside it.
* **Behavior: −8.8 % acceptance.** Path B amplification, multiplied by GQA sharing. Acceptance length is a direct read on how much the target distribution moved ([Module 08](Lecture-08.md)) — and it moved a lot.

**The trade is 1.9 % throughput for 8.8 % acceptance, and it is a bad one.** Worse than it looks, in fact: [Module 10](Lecture-10.md) shows that acceptance feeds back into throughput, so part of that 1.9 % is given straight back.

The correct configuration keeps **MLP + O + V** in NVFP4 and leaves **Q + K** wide. Cost: ~11 % of `B_token` left uncompressed. Benefit: the entire acceptance regression avoided.

---

</details>

## 5. 敏感度 ≠ 离群值幅度

下面这个发现解开了上述困惑，也是本模块中最重要的实践要点。**Q/K 的退化无法用激活值离群值统计来解释。** 你可以用 [Module 06 的](Lecture-06.md)工具对 Q/K 做 profile，发现 channel 和 token 比例平平无奇——但它们依然是对量化损害最大的张量。

原因在于，这两者度量的是不同的东西：

```text
   outlier statistics    ──▶  predict QUANTIZATION ERROR      (how wrong the tensor becomes)
   sensitivity           ──▶  error  ×  DOWNSTREAM AMPLIFICATION
                                        └── the operator's doing, invisible to any
                                            statistic computed on the tensor itself
```

```text
   sensitivity_i  =  quantization_error_i   ×   amplification_i
                     └── measurable from ──┘    └── a property of the OPERATOR,
                         the tensor alone            not of the tensor ──┘
```

Q/K 处于误差不大而放大倍数巨大的位置。Embedding 处于误差高而放大倍数近乎零的位置（一次 gather 直接喂入残差流）。**这两者都无法通过观察张量本身预测出来。**

> **实践准则：绝不要从权重或激活值统计量推断敏感度。要端到端地测量。** 统计量告诉你该用哪种*方法*（[Module 05](Lecture-05.md)）；只有消融实验才能告诉你该量化哪些*张量*。

### 测量流程

```python
# Leave-one-out: quantize everything EXCEPT the group under test.
# The DELTA against all-quantized is that group's contribution to the damage.
GROUPS = ["mlp", "o_proj", "q_proj", "k_proj", "v_proj", "lm_head", "embed"]

baseline_all = evaluate(quantize(model, groups=GROUPS))     # everything quantized
results = {}
for g in GROUPS:
    held_out = [x for x in GROUPS if x != g]
    m = quantize(model, groups=held_out)
    results[g] = {
        "kl_vs_fp16":   mean_kl(m, reference),          # Module 08
        "acceptance":   acceptance_length(m, drafter),  # Module 08 / 10
        "traffic_saved": traffic_of(g),                 # Module 04
        "recovery":     baseline_all.kl - mean_kl(m, reference),
    }
```

然后按真正重要的那个比值排序：

```text
                       traffic_saved_i
   value_i  =  ────────────────────────────
                 KL_increase_i  +  ϵ
```

一个有代表性的结果——**你自己模型的数字会不同，这正是需要测量的原因**：

| Group | Traffic saved | ΔKL | Δacceptance | Value | Verdict |
|---|---:|---:|---:|---:|---|
| MLP | 57.96 % | 低 | 小 | **最高** | 量化 |
| O | 11.41 % | 低 | 小 | 高 | 量化 |
| V | ~1.4 % | 低 | 小 | 中 | 量化 |
| lm_head | 16.0 % | 低–中 | 小 | 高 | 量化（先 FP8） |
| Q | 11.41 % | **高** | **大** | 低 | **保持宽位宽** |
| K | ~1.4 % | **最高** | **最大** | **最低** | **保持宽位宽** |
| Embedding | ~0 % | 低 | 无 | **零** | 量化与否都没有意义 |

这张表正是 [Module 11](Lecture-11.md) 分配求解器的直接输入。

---

## 检查点

现在你应该能够：

1. 推导 Path A 的 `η/√K` 和 Path B 的 `exp(η√2·|s|)`，并解释为什么在后者中平均带来的收益消失了。
2. 对给定的 η 和 logit 幅度，计算 attention 权重的失真。
3. 解释为什么 V 的量化表现像 O 而不像 K。
4. 解释为什么 GQA 使 K 成为模型中收益/风险最差的张量。
5. 说明为什么离群值统计量无法预测敏感度，以及它们*真正*擅长什么。
6. 设计 leave-one-out 扫描，并按每单位 KL 的流量对分组排序。

---

## 交付

为你的模型产出一张**敏感度表**：每个张量分组、其来自 [Module 04](Lecture-04.md) 台账的流量占比、其来自 leave-one-out 扫描的 ΔKL 和 Δ接受率，以及由此得到的价值比。再加上 §2 中的上下文长度扫描——在 4 K 和你的最大上下文长度下测量 Q/K 敏感度，并报告预期的增长是否出现。

然后说明你的分配方案，更重要的是，**你决定不量化的那两个分组及原因。**

---

## 时效性说明

* **长期有效：** Path A / Path B 推导、softmax 放大上界、GQA 共享论证、敏感度 ≠ 离群值幅度。
* **案例研究锚点：** 147.87/2.792 与 150.73/2.546 的对比；各张量的流量占比由 [Module 04](Lecture-04.md) 的重构推出，其中假设了 48 层的几何结构——请针对你自己的配置重新计算，而不要复用这些百分比。
* **开放/可验证：** Q/K 敏感度随上下文长度增长的预测源自 RoPE 相位论证，应针对每个模型分别验证，不能想当然。

---

**Next:** [Module 08 — Behavior Preservation →](Lecture-08.md)


<details>
<summary>English original</summary>

**5. Sensitivity ≠ outlier magnitude**

Here is the finding that resolves the confusion, and it is the most important practical point in the module. **The Q/K degradation is not explained by activation-outlier statistics.** You can profile Q/K with [Module 06's](Lecture-06.md) tooling and find unremarkable channel and token ratios — and they will still be the most damaging tensors to quantize.

The reason is that these measure different things:

```text
   outlier statistics    ──▶  predict QUANTIZATION ERROR      (how wrong the tensor becomes)
   sensitivity           ──▶  error  ×  DOWNSTREAM AMPLIFICATION
                                        └── the operator's doing, invisible to any
                                            statistic computed on the tensor itself
```

```text
   sensitivity_i  =  quantization_error_i   ×   amplification_i
                     └── measurable from ──┘    └── a property of the OPERATOR,
                         the tensor alone            not of the tensor ──┘
```

Q/K sit at modest error and enormous amplification. Embeddings sit at high error and near-zero amplification (a gather feeds a residual stream). **Neither is predicted by looking at the tensor.**

> **The practical rule: never infer sensitivity from weight or activation statistics. Measure it end-to-end.** Statistics tell you which *method* to use ([Module 05](Lecture-05.md)); only an ablation tells you which *tensors* to quantize.

**The measurement protocol**

```python
# Leave-one-out: quantize everything EXCEPT the group under test.
# The DELTA against all-quantized is that group's contribution to the damage.
GROUPS = ["mlp", "o_proj", "q_proj", "k_proj", "v_proj", "lm_head", "embed"]

baseline_all = evaluate(quantize(model, groups=GROUPS))     # everything quantized
results = {}
for g in GROUPS:
    held_out = [x for x in GROUPS if x != g]
    m = quantize(model, groups=held_out)
    results[g] = {
        "kl_vs_fp16":   mean_kl(m, reference),          # Module 08
        "acceptance":   acceptance_length(m, drafter),  # Module 08 / 10
        "traffic_saved": traffic_of(g),                 # Module 04
        "recovery":     baseline_all.kl - mean_kl(m, reference),
    }
```

Then rank by the ratio that actually matters:

```text
                       traffic_saved_i
   value_i  =  ────────────────────────────
                 KL_increase_i  +  ϵ
```

A representative outcome — **your model's numbers will differ, which is the point of measuring**:

| Group | Traffic saved | ΔKL | Δacceptance | Value | Verdict |
|---|---:|---:|---:|---:|---|
| MLP | 57.96 % | low | small | **highest** | quantize |
| O | 11.41 % | low | small | high | quantize |
| V | ~1.4 % | low | small | moderate | quantize |
| lm_head | 16.0 % | low–moderate | small | high | quantize (FP8 first) |
| Q | 11.41 % | **high** | **large** | low | **keep wide** |
| K | ~1.4 % | **highest** | **largest** | **lowest** | **keep wide** |
| Embeddings | ~0 % | low | none | **zero** | pointless either way |

That table is the direct input to [Module 11](Lecture-11.md)'s allocation solver.

---

**Checkpoint**

You should now be able to:

1. Derive `η/√K` for Path A and `exp(η√2·|s|)` for Path B, and explain why the averaging benefit vanishes in the second.
2. Compute the attention-weight distortion for a given η and logit magnitude.
3. Explain why V quantizes like O rather than like K.
4. Explain why GQA makes K the worst reward/risk tensor in the model.
5. State why outlier statistics do not predict sensitivity, and what they *are* good for.
6. Design a leave-one-out sweep and rank groups by traffic-per-unit-KL.

---

**Ship it**

Produce a **sensitivity table** for your model: every tensor group, its traffic share from [Module 04's](Lecture-04.md) ledger, its ΔKL and Δacceptance from a leave-one-out sweep, and the resulting value ratio. Add the context-length sweep from §2 — measure Q/K sensitivity at 4 K and at your maximum context and report whether the predicted growth appears.

Then state your allocation and, more importantly, **the two groups you decided not to quantize and why.**

---

**Current as of**

* **Timeless:** the Path A / Path B derivation, the softmax amplification bound, the GQA sharing argument, sensitivity ≠ outlier magnitude.
* **Case-study pins:** the 147.87/2.792 vs 150.73/2.546 comparison; the per-tensor traffic shares are derived from the [Module 04](Lecture-04.md) reconstruction with an assumed 48-layer geometry — recompute for your own config rather than reusing the percentages.
* **Open/testable:** the prediction that Q/K sensitivity grows with context length follows from the RoPE phase argument and should be verified per model, not assumed.

---

**Next:** [Module 08 — Behavior Preservation →](Lecture-08.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/Hardware-Aware LLM Quantization/Lecture-07.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/Hardware-Aware%20LLM%20Quantization/Lecture-07.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
