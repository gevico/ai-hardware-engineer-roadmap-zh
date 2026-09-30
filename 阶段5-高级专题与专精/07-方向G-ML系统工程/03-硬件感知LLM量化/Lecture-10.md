---
title: 模块 10 — 投机解码与接受税
description: 模块 10 — 投机解码与接受税
---

# 模块 10 — 投机解码与接受税

**合集：** [硬件感知 LLM 量化](README.md) | **上一章：** [← 模块 09](Lecture-09.md) | **下一章：** [模块 11 →](Lecture-11.md)

---

投机解码是本课程中唯一一项改变吞吐方程本身、而非改变其中某一项的技术。其他一切都只是在降低 `B_token`。投机让**每次权重读取产出多个 token**，从而把上限成倍放大。

它还会形成一个反馈回路，悄无声息地毁掉大部分量化收益，而本模块的核心结论给出了它的量化结果：在案例模型上，**量化 QKV 带来了 11.8 % 的字节收益，实际兑现 1.9 % —— 接受税吃掉了其中的 84 %。**

---

## 学习目标

读完本模块，你应当能够：

1. 陈述拒绝采样规则，并解释投机解码为何是**无损**的。
2. 推导加速模型 `S = τ / (1 + K·c)`，并选出最优草稿深度。
3. 解释量化**目标模型**与**草稿模型**各自如何以不同方式影响接受率。
4. 计算一次量化改动带来的**接受税**，并判断它是否值得上线。
5. 解释为什么投机改变了哪一种优化才是正确的。

---

## 1. 机制，以及它为何无损

```text
   1. DRAFT     cheap model q proposes K tokens autoregressively
   2. VERIFY    target model p scores all K+1 positions in ONE forward pass
   3. ACCEPT    walk left to right; accept token x with probability min(1, p(x)/q(x))
   4. ON REJECT sample a correction from the residual  (p(x) − q(x))⁺ / Σ(p − q)⁺
   5. EMIT      the accepted prefix + one correction token
```

第 4 步正是让整个过程不付出质量代价的原因：

```text
   ┌──────────────────────────────────────────────────────────────────┐
   │  The output distribution is EXACTLY p — the target's own          │
   │  distribution — regardless of how bad the drafter q is.           │
   │  A bad drafter costs SPEED (low acceptance), never QUALITY.       │
   └──────────────────────────────────────────────────────────────────┘
```

正是这一保证，使投机可以安全部署，也使它能够被当作*测量仪器*使用（[模块 08](Lecture-08.md)）—— 被测对象不会因为被测量而受到扰动。

这套经济学之所以成立，是因为**验证 K+1 个 token 的代价几乎与验证 1 个相同。** decode（逐 token 生成阶段）是带宽受限的（[模块 01](Lecture-01.md)）；无论有多少个位置搭车，权重都只读取一次。K+1 个位置就是一个大小为 K+1 的批 —— 仍比 ridge point 低约 60×。

### 草稿来源

| 来源 | 工作方式 | 典型 α |
|---|---|---|
| **小型草稿模型** | 独立的 0.5–1 B 模型 | 中等；tokenizer 必须匹配 |
| **MTP head** | 与模型一同训练的额外预测头 | 高 —— 在同一分布上训练 |
| **EAGLE 式** | 在目标模型的*特征*而非 token 上做自回归的头 | 最高 |
| **N-gram / prompt 查找** | 从上下文里检索后续内容 | 对重复性/代码文本很高，且免费 |

案例模型使用一个 **MTP head** —— 0.85 GB，在 [模块 04 的](Lecture-04.md)账本中属于 Class C。它只在投机开启时才位于 decode 路径上，而正是这种条件性，使模块 04 §5 中的账本形态发生了变化。

---

## 2. 加速模型

由 [模块 08 §4](Lecture-08.md)，其中每 token 接受率为 `α`、草稿深度为 `K`：

```text
   τ  =  1 + α + α² + ... + α^K  =  (1 − α^{K+1}) / (1 − α)          expected tokens per cycle
```

一个循环的代价是 1 次目标模型 pass 加上 `K` 次草稿 pass。当 `c = T_draft / T_target` 时：

```text
                 τ
   S  =  ─────────────────
           1  +  K · c
```

```text
   NUMERATOR   τ      grows with α and K, but SATURATES (α^K → 0)
   DENOMINATOR 1+K·c  grows LINEARLY with K, forever
                      ⟹ an interior optimum in K always exists
```

不同区间下的最优 `K`：

| α | c | **K\*** | S |
|---:|---:|---:|---:|
| 0.70 | 0.05 | 6 | 2.35 |
| 0.70 | 0.10 | 4 | 1.98 |
| 0.70 | 0.20 | 3 | 1.58 |
| **0.785** | **0.10** | **5** | **2.38** |
| 0.85 | 0.05 | 10 | 3.70 |
| 0.85 | 0.20 | 5 | 2.08 |

由此可以得出两条操作规则：

* **更便宜的草稿模型能买到深度。** 在 α = 0.785 时，把 `c` 从 0.10 减半到 0.05，可使 `K*` 从 5 升到 7，并把加速提升约 24 %。这是把 MTP head 保持*小巧*的有力论据，而 —— 正如 §4 所示 —— 这也是量化它的一个薄弱论据。
* **任何对 α 的改动之后，都必须重新调 `K`。** 降低接受率的量化，同时也会降低最优草稿深度。那些把 `K` 写死在配置文件里、之后才做量化的团队，跑的其实是一个次优的 `K`，却在责怪量化。


<details>
<summary>English original</summary>

**Module 10 — Speculative Decoding and the Acceptance Tax**

**Collection:** [Hardware-Aware LLM Quantization](README.md) | **Previous:** [← Module 09](Lecture-09.md) | **Next:** [Module 11 →](Lecture-11.md)

---

Speculative decoding is the only technique in this course that changes the throughput equation itself rather than one of its terms. Everything else reduces `B_token`. Speculation emits **multiple tokens per weight-read**, which multiplies the ceiling.

It also creates a feedback loop that quietly destroys most quantization wins, and this module's central result quantifies it: on the case-study model, **quantizing QKV delivered an 11.8 % byte win and realized 1.9 % — the acceptance tax consumed 84 % of it.**

---

**Learning objectives**

By the end of this module you should be able to:

1. State the rejection-sampling rule and explain why speculative decoding is **lossless**.
2. Derive the speedup model `S = τ / (1 + K·c)` and choose an optimal draft depth.
3. Explain how quantizing the **target** and the **drafter** each affect acceptance, differently.
4. Compute the **acceptance tax** on a quantization change and decide whether it is worth shipping.
5. Explain why speculation changes which optimization is correct.

---

**1. The mechanism, and why it is lossless**

```text
   1. DRAFT     cheap model q proposes K tokens autoregressively
   2. VERIFY    target model p scores all K+1 positions in ONE forward pass
   3. ACCEPT    walk left to right; accept token x with probability min(1, p(x)/q(x))
   4. ON REJECT sample a correction from the residual  (p(x) − q(x))⁺ / Σ(p − q)⁺
   5. EMIT      the accepted prefix + one correction token
```

Step 4 is what makes the whole thing free of quality cost:

```text
   ┌──────────────────────────────────────────────────────────────────┐
   │  The output distribution is EXACTLY p — the target's own          │
   │  distribution — regardless of how bad the drafter q is.           │
   │  A bad drafter costs SPEED (low acceptance), never QUALITY.       │
   └──────────────────────────────────────────────────────────────────┘
```

That guarantee is why speculation is safe to deploy and why it can be used as a *measurement instrument* ([Module 08](Lecture-08.md)) — the thing being measured is not perturbed by measuring it.

The economics work because **verifying K+1 tokens costs almost the same as verifying one.** Decode is memory-bound ([Module 01](Lecture-01.md)); the weights are read once regardless of how many positions ride along. K+1 positions is a batch of K+1 — still ~60× below the ridge point.

**Draft sources**

| Source | How it works | Typical α |
|---|---|---|
| **Small draft model** | separate 0.5–1 B model | moderate; tokenizer must match |
| **MTP head** | extra prediction heads trained with the model | high — trained on the same distribution |
| **EAGLE-style** | autoregressive head over the target's *features*, not tokens | highest |
| **N-gram / prompt lookup** | retrieve continuations from context | high for repetitive/code text, free |

The case study uses an **MTP head** — 0.85 GB, Class C in [Module 04's](Lecture-04.md) ledger. It is on the decode path only when speculation is enabled, and that conditionality is exactly what made the ledger change shape in Module 04 §5.

---

**2. The speedup model**

From [Module 08 §4](Lecture-08.md), with per-token acceptance `α` and draft depth `K`:

```text
   τ  =  1 + α + α² + ... + α^K  =  (1 − α^{K+1}) / (1 − α)          expected tokens per cycle
```

One cycle costs one target pass plus `K` draft passes. With `c = T_draft / T_target`:

```text
                 τ
   S  =  ─────────────────
           1  +  K · c
```

```text
   NUMERATOR   τ      grows with α and K, but SATURATES (α^K → 0)
   DENOMINATOR 1+K·c  grows LINEARLY with K, forever
                      ⟹ an interior optimum in K always exists
```

Optimal `K` by regime:

| α | c | **K\*** | S |
|---:|---:|---:|---:|
| 0.70 | 0.05 | 6 | 2.35 |
| 0.70 | 0.10 | 4 | 1.98 |
| 0.70 | 0.20 | 3 | 1.58 |
| **0.785** | **0.10** | **5** | **2.38** |
| 0.85 | 0.05 | 10 | 3.70 |
| 0.85 | 0.20 | 5 | 2.08 |

Two operational rules fall out:

* **A cheaper drafter buys depth.** Halving `c` from 0.10 to 0.05 raises `K*` from 5 to 7 at α = 0.785 and lifts speedup ~24 %. This is a strong argument for keeping the MTP head *small*, and — as §4 shows — a weak argument for quantizing it.
* **`K` must be re-tuned after any change to α.** A quantization that lowers acceptance also lowers the optimal draft depth. Teams that fix `K` in a config file and then quantize are running a suboptimal `K` and blaming the quantization.

---

</details>

## 3. 反馈回路

这就是本课程其余部分一直在铺垫的那种相互作用。

```text
   quantize the target
        │
        ├──▶  B_token ↓   ──▶  each target pass is FASTER      ──▶  tok/s ↑
        │
        └──▶  p moves    ──▶  TV(p,q) ↑  ──▶  α ↓  ──▶  τ ↓   ──▶  tok/s ↓
                                          (Module 08 §4)

   NET = (byte gain)  ×  (acceptance loss)
```

结合吞吐方程：

```text
              BW_eff        τ(α)
   tok/s  =  ────────  ×  ──────────
              B_token      1 + K·c
              └──────┘     └────────┘
              Modules 1–7   this module
```

**量化 target 时这两个因子都会变化，而且方向相反。**

### 量化案例研究中的代价

```text
   MLP + O quantized          147.87 tok/s     τ = 2.792
   MLP + O + QKV quantized    150.73 tok/s     τ = 2.546
```

分解观测到的变化：

```text
   observed tok/s ratio        =  150.73 / 147.87  =  1.0193
   τ ratio                     =  2.546  / 2.792   =  0.9119
   ⟹ implied B_token ratio     =  1.0193 / 0.9119  =  1.1178
                                   (B_token fell 10.5 %)
```

与 [模块 07 的](Lecture-07.md) 流量表交叉核对 —— Q + K + V 占 `B_token` 的 14.26 %，而 BF16 → NVFP4 会去掉它们字节数的 `(1 − 0.5625/2) = 71.9 %`：

```text
   predicted B_token reduction  =  14.26 % × 71.9 %  =  10.25 %
   implied from measurement                          =  10.5 %      ✓
```

模型闭合到 0.25 个百分点以内。所以：

```text
   ┌────────────────────────────────────────────────────────────────┐
   │   potential gain (if acceptance had held)   :   +11.78 %       │
   │   realized gain                             :    +1.93 %       │
   │   ─────────────────────────────────────────────────────────    │
   │   ACCEPTANCE TAX  =  83.6 % of the byte win, consumed          │
   └────────────────────────────────────────────────────────────────┘
```

**吞吐收益中有七分之六以接受率损失的形式被还了回去** —— 而这还*没有*计入 target 分布以 0.082 的总变差移动所带来的行为代价（[模块 08](Lecture-08.md)）。

一个唯一理由就是「+1.9 % tok/s」的配置，不值得为之付出可测量的行为退化。**接受率代价把一个看似边缘的收益变成了明确的损失**，而只有当你把接受率与吞吐一起测量时才能看到它。

> 这是普遍的教训：**在投机系统中，吞吐与行为不是彼此独立的轴。** 损害 target 的分布会通过 α 直接让你付出速度代价。你原以为相互牵制的那些指标其实是部分一致的 —— 这是好消息，因为它意味着诚实的选择通常也是快的那个。

---

## 4. 量化 drafter 是另一项决策

target 与 drafter 位于方程的两侧：

| | 量化 **target** | 量化 **drafter** |
|---|---|---|
| 对 `B_token` 的影响 | 大 —— 它就是模型本身 | 小 —— MTP head 为 0.85 GB |
| 对 `c` 的影响 | 无 | 降低它 → 允许更深的 `K` |
| 对 α 的影响 | 降低（p 移动） | 降低（q 移动） |
| 对 **输出质量** 的影响 | **有 —— p 就是输出分布** | **无 —— 无损性成立** |

两个推论：

**1. drafter 没有需要保护的质量预算。** 由无损性保证，退化的 drafter 无法改变输出分布。所以 drafter 量化是**纯粹的速度/接受率权衡**，没有行为风险 —— 比 target 量化好决策得多。

**2. 但 drafter 很小，所以收益也小。** MTP head 是 0.85 GB，而 target 是 15.88 GB。把它量化为 NVFP4 每个 draft step 省下 0.61 GB，小幅降低 `c`，并付出接受率代价。用 [模块 04 §5](Lecture-04.md) 的数字，在 `K = 3` 时 MTP head 约占流量的 4.6 %。

```text
   Do NOT quantize the drafter aggressively.
   It is 4.6 % of the traffic and 100 % of the acceptance rate.
```

这种不对称很尖锐：drafter 的*唯一*职责就是与 target 一致。损害它就是在攻击那个乘到你整个吞吐上的量 —— α。除非你已经实测证明更低的精度能保住接受率，否则把 drafter 保持在 BF16 或 FP8。

---


<details>
<summary>English original</summary>

**3. The feedback loop**

Here is the interaction the rest of this course has been building toward.

```text
   quantize the target
        │
        ├──▶  B_token ↓   ──▶  each target pass is FASTER      ──▶  tok/s ↑
        │
        └──▶  p moves    ──▶  TV(p,q) ↑  ──▶  α ↓  ──▶  τ ↓   ──▶  tok/s ↓
                                          (Module 08 §4)

   NET = (byte gain)  ×  (acceptance loss)
```

Combining with the throughput equation:

```text
              BW_eff        τ(α)
   tok/s  =  ────────  ×  ──────────
              B_token      1 + K·c
              └──────┘     └────────┘
              Modules 1–7   this module
```

**Both factors move when you quantize the target, and they move in opposite directions.**

**Quantifying the tax on the case study**

```text
   MLP + O quantized          147.87 tok/s     τ = 2.792
   MLP + O + QKV quantized    150.73 tok/s     τ = 2.546
```

Decompose the observed change:

```text
   observed tok/s ratio        =  150.73 / 147.87  =  1.0193
   τ ratio                     =  2.546  / 2.792   =  0.9119
   ⟹ implied B_token ratio     =  1.0193 / 0.9119  =  1.1178
                                   (B_token fell 10.5 %)
```

Cross-check against [Module 07's](Lecture-07.md) traffic table — Q + K + V are 14.26 % of `B_token`, and BF16 → NVFP4 removes `(1 − 0.5625/2) = 71.9 %` of their bytes:

```text
   predicted B_token reduction  =  14.26 % × 71.9 %  =  10.25 %
   implied from measurement                          =  10.5 %      ✓
```

The model closes to within 0.25 points. So:

```text
   ┌────────────────────────────────────────────────────────────────┐
   │   potential gain (if acceptance had held)   :   +11.78 %       │
   │   realized gain                             :    +1.93 %       │
   │   ─────────────────────────────────────────────────────────    │
   │   ACCEPTANCE TAX  =  83.6 % of the byte win, consumed          │
   └────────────────────────────────────────────────────────────────┘
```

**Six sevenths of the throughput win was paid back as lost acceptance** — and that is *before* counting the behavioral cost of a target distribution that moved by 0.082 in total variation ([Module 08](Lecture-08.md)).

A configuration whose sole justification is "+1.9 % tok/s" is not worth a measurable behavior regression. **The acceptance tax converts what looks like a marginal win into a clear loss**, and you can only see it if you measure acceptance alongside throughput.

> This is the general lesson: **in a speculative system, throughput and behavior are not independent axes.** Damaging the target's distribution costs you speed directly, through α. The metrics you thought were in tension are partly aligned — which is good news, because it means the honest choice is usually also the fast one.

---

**4. Quantizing the drafter is a different decision**

Target and drafter sit on opposite sides of the equation:

| | Quantize the **target** | Quantize the **drafter** |
|---|---|---|
| Effect on `B_token` | large — it is the model | small — MTP head is 0.85 GB |
| Effect on `c` | none | reduces it → allows deeper `K` |
| Effect on α | reduces (p moves) | reduces (q moves) |
| Effect on **output quality** | **yes — p is the output distribution** | **none — losslessness holds** |

Two consequences:

**1. The drafter has no quality budget to protect.** By the losslessness guarantee, a degraded drafter cannot change the output distribution. So drafter quantization is a **pure speed/acceptance trade** with no behavioral risk — a much easier decision than target quantization.

**2. But the drafter is small, so the win is small.** The MTP head is 0.85 GB against a 15.88 GB target. Quantizing it to NVFP4 saves 0.61 GB per draft step, reduces `c` modestly, and costs acceptance. Using the numbers from [Module 04 §5](Lecture-04.md), the MTP head is ~4.6 % of traffic at `K = 3`.

```text
   Do NOT quantize the drafter aggressively.
   It is 4.6 % of the traffic and 100 % of the acceptance rate.
```

The asymmetry is sharp: the drafter's *only* job is to agree with the target. Damaging it attacks the one quantity — α — that multiplies your entire throughput. Keep the drafter at BF16 or FP8 unless you have measured that a lower precision holds acceptance.

---

</details>

## 5. 推测解码改变了哪个优化才是正确的

[Module 04 §5](Lecture-04.md) 显示，启用推测解码后带宽利用率从 72 % 降到约 55 %。现在原因清楚了：推测解码把一次权重读取摊到约 2.9 个输出 token 上，因此同一份权重每秒能支撑多得多的 token。

```text
   NO SPECULATION           SPECULATION ENABLED
   ────────────────         ─────────────────────────────
   72 % of peak BW          ~55 % of peak BW
   bandwidth-bound          NOT bandwidth-bound
   → remove bytes           → raise α, and fix the kernels
```

| 手段 | 未启用推测解码时的取值 | 启用推测解码时的取值 |
|---|---|---|
| 进一步量化权重 | 高 | **递减** — 已处于峰值的 55 % |
| 提高接受率 α | n/a | **最高** — 对一切都成倍放大 |
| 调优 draft depth `K` | n/a | 高，而且免费 |
| 提升 kernel 效率 | 高 | **高** — 45 % 的缺口不在字节上 |
| 减少 KV 流量（长 ctx） | 高 | 高 — [Module 09](Lecture-09.md) |

**在 DSpark 配置下，后续动作的排序为：(1) 修 kernel，(2) 调 `K`，(3) 提高 α，(4) 量化 `lm_head`。进一步量化主体排第五** — 而一旦计入接受税，量化 Q/K 就是负收益。

这个排序是整门课的实用收获，注意它不是任何单次测量能告诉你的。

---

## 检查点

你现在应该能够：

1. 陈述拒绝采样规则，并解释无损性保证。
2. 推导 `S = τ/(1+K·c)`，并解释 `K` 中为何存在内部最优。
3. 把观测到的吞吐变化分解为字节因子与接受率因子。
4. 计算接受税，并用它否决一个边缘配置。
5. 解释为什么 drafter 没有质量预算，却仍不应重度量化。
6. 解释为什么启用推测解码会重排优化优先级。

---

## 交付

构建一个**感知接受率的 benchmark harness**，对每个配置报告：

```text
   config | B_token | τ | α | K | c | S | tok/s | predicted tok/s | byte gain % | realized % | tax %
```

以及 `predicted tok/s = BW_eff/B_token × τ/(1+Kc)`。然后在固定量化下跑 `K` 扫描，在固定 `K` 下跑量化扫描，并报告：

* 你实测的 `K*`，以及它在量化后是否变化
* 每个量化步的接受税
* **任何接受税超过字节收益 50 % 的配置** — 那些就是你否决掉的候选，把它们列出来才是重点

---

## 时效性

* **不随时间变化：** 拒绝采样规则与无损性、`τ = (1−α^{K+1})/(1−α)`、`S = τ/(1+K·c)`、接受税分解。
* **案例锚定值：** 147.87 tok/s @ τ 2.792 与 150.73 @ τ 2.546；推导出的 10.5 % `B_token` 降幅与 83.6 % 接受税。α 的取值假设 `K = 3`；接受税的计算与 `K` 无关，因为它直接使用 τ 比值。
* **2026 年的 draft 方法：** MTP heads、EAGLE-3 风格的 feature 级 draft、n-gram/prompt-lookup 是当前的方法族。系统层面的讲解见 [MLSys Deep Dives — Lecture 06](../05-MLSys深度探索/Lecture-06.md)。

---

**下一章：** [Module 11 — Hardware-Aware AutoQuant →](Lecture-11.md)


<details>
<summary>English original</summary>

**5. Speculation changes which optimization is correct**

[Module 04 §5](Lecture-04.md) showed bandwidth utilization dropping from 72 % to ~55 % when speculation is enabled. Now the reason is clear: speculation amortizes one weight-read across ~2.9 emitted tokens, so the same weights support far more tokens per second.

```text
   NO SPECULATION           SPECULATION ENABLED
   ────────────────         ─────────────────────────────
   72 % of peak BW          ~55 % of peak BW
   bandwidth-bound          NOT bandwidth-bound
   → remove bytes           → raise α, and fix the kernels
```

| Lever | Value without speculation | Value with speculation |
|---|---|---|
| Quantize weights further | high | **diminishing** — you are at 55 % of peak |
| Raise acceptance α | n/a | **highest** — multiplies everything |
| Tune draft depth `K` | n/a | high, and free |
| Improve kernel efficiency | high | **high** — the 45 % gap is not bytes |
| Reduce KV traffic (long ctx) | high | high — [Module 09](Lecture-09.md) |

**In the DSpark configuration, the ranked next actions are: (1) fix the kernels, (2) tune `K`, (3) raise α, (4) quantize `lm_head`. Further body quantization is fifth** — and quantizing Q/K is negative-value once the acceptance tax is counted.

That ordering is the practical payoff of the whole course, and note that it is not what any single measurement would have told you.

---

**Checkpoint**

You should now be able to:

1. State the rejection-sampling rule and explain the losslessness guarantee.
2. Derive `S = τ/(1+K·c)` and explain why an interior optimum in `K` exists.
3. Decompose an observed throughput change into byte and acceptance factors.
4. Compute the acceptance tax and use it to reject a marginal configuration.
5. Explain why the drafter has no quality budget but should still not be quantized hard.
6. Explain why enabling speculation reorders the optimization priorities.

---

**Ship it**

Build an **acceptance-aware benchmark harness** that reports, for every configuration:

```text
   config | B_token | τ | α | K | c | S | tok/s | predicted tok/s | byte gain % | realized % | tax %
```

with `predicted tok/s = BW_eff/B_token × τ/(1+Kc)`. Then run the `K` sweep at fixed quantization and the quantization sweep at fixed `K`, and report:

* your measured `K*`, and whether it moved after quantization
* the acceptance tax for each quantization step
* **any configuration where the tax exceeded 50 % of the byte win** — those are your rejected candidates, and listing them is the point

---

**Current as of**

* **Timeless:** the rejection-sampling rule and losslessness, `τ = (1−α^{K+1})/(1−α)`, `S = τ/(1+K·c)`, the acceptance-tax decomposition.
* **Case-study pins:** 147.87 tok/s @ τ 2.792 and 150.73 @ τ 2.546; the derived 10.5 % `B_token` reduction and 83.6 % acceptance tax. The α values assume `K = 3`; the tax computation is independent of `K` since it uses τ ratios directly.
* **2026 drafting methods:** MTP heads, EAGLE-3-style feature-level drafting, and n-gram/prompt-lookup are the current families. See [MLSys Deep Dives — Lecture 06](../05-MLSys深度探索/Lecture-06.md) for the systems treatment.

---

**Next:** [Module 11 — Hardware-Aware AutoQuant →](Lecture-11.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/Hardware-Aware LLM Quantization/Lecture-10.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/Hardware-Aware%20LLM%20Quantization/Lecture-10.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
