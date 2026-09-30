---
title: 模块 08 — 行为保持
description: 模块 08 — 行为保持
---

# 模块 08 — 行为保持

**Collection:** [硬件感知 LLM 量化](README.md) | **Previous:** [← 模块 07](Lecture-07.md) | **Next:** [模块 09 →](Lecture-09.md)

---

到目前为止，每个模块讲的都是如何把模型做得更快。这一个讲的是如何证明你没有把它弄坏 —— 这是更难的一半，因为这种失败是静默的。损害了模型的量化不会崩溃，不会告警，而且常常不会让困惑度发生变化。

本模块的核心结果是一个精确恒等式，它把投机解码的接受率变成**一种免费、在线、有统计依据的度量，用来衡量量化模型漂移了多远。** 这是现有最便宜的锐利工具，而大多数团队已经在运行它，却没有在读它。

---

## 学习目标

读完本模块后，你应该能够：

1. 按灵敏度和成本对行为指标排序，并说明何时该跑哪一个。
2. 解释为什么困惑度只是一个*粗略*的筛选，以及它漏掉了什么。
3. 把相对 FP16 参考模型的平均 KL 与 top-1 一致率作为实际使用时的门禁。
4. **证明**投机接受率等于 `1 − TV(p, q)`，并用它来度量分布漂移。
5. 计算检测给定的接受率变化所需的 token 数，并避免把噪声当成结果报告。

---

## 1. 指标阶梯

```text
   CHEAP, INSENSITIVE                                    EXPENSIVE, DECISIVE
   ─────────────────────────────────────────────────────────────────────────▶

   weight MSE ──▶ layer-output MSE ──▶ perplexity ──▶ mean KL ──▶ top-1 ──▶ acceptance ──▶ benchmarks
        │              │                    │            │          │           │             │
     seconds        minutes             ~10 min      ~10 min    ~10 min     ~minutes       hours
     tells you      tells you           weak         THE        intuitive   sharp,         ground
     nothing        which layer         signal       GATE       and cheap   free,          truth
     about behavior  moved                                                   online
```

有两条规则支配这个阶梯：

* **绝不能只凭较低层级就通过。** 权重 MSE 是调试辅助手段，不是证据。
* **绝不跳级直接看 benchmark。** 它们是 ground truth，而且慢得无法用来指导在几十种量化配置上的搜索。用它们来*确认*决定，而不是用来做决定。

---

## 2. 为什么困惑度是件弱工具

困惑度是语料上的 `exp(mean NLL)`（[Logprobs, Perplexity & KL Divergence — Lecture 03](../04-Logprobs-困惑度与KL散度/Lecture-03.md)）。它用于量化评分的弱点是结构性的：

```text
   PPL is a MEAN over tens of thousands of tokens.

   Most tokens are easy: "of the", "  return", "). "  →  the model is confident
   and stays confident after quantization. These dominate the mean and DILUTE
   the signal from the tokens where the damage actually lands.
```

```text
   PPL  16.21  →  16.28     ( +0.4 % )    "looks fine, ship it"
   but underneath:
      98 % of tokens        : unchanged
       2 % of tokens        : top-1 prediction FLIPPED
      long-context behavior : degraded (Module 07 §2)
      tool-call formatting  : intermittently broken
```

三个具体的盲点：

| 盲点 | PPL 为什么漏掉它 |
|---|---|
| **尾部行为** | 稀有但关键的 token 被平均掉了 |
| **长上下文** | PPL 通常用很短的滑动窗口测量 |
| **分布形状** | PPL 只读取*观测到*的那个 token 的概率；分布的其他部分可以自由变形 |

最后一条最致命，而这正是 KL 所修正的。

> 把 PPL 当作**冒烟测试**：大幅跳变意味着某处严重出错。小幅跳变几乎说明不了什么。绝不只凭 PPL 就上线。

---


<details>
<summary>English original</summary>

**Module 08 — Behavior Preservation**

**Collection:** [Hardware-Aware LLM Quantization](README.md) | **Previous:** [← Module 07](Lecture-07.md) | **Next:** [Module 09 →](Lecture-09.md)

---

Every module so far has been about making the model faster. This one is about proving you did not break it — which is the harder half, because the failure is silent. A quantization that damages the model does not crash, does not warn, and frequently does not move perplexity.

The headline result of this module is an exact identity that turns speculative decoding's acceptance rate into a **free, online, statistically-grounded measurement of how far your quantized model drifted.** It is the cheapest sharp instrument available, and most teams already have it running and are not reading it.

---

**Learning objectives**

By the end of this module you should be able to:

1. Order the behavioral metrics by sensitivity and cost, and say which to run when.
2. Explain why perplexity is a *rough* screen and what it misses.
3. Use mean KL vs. the FP16 reference and top-1 agreement as the working gate.
4. **Prove** that speculative acceptance rate equals `1 − TV(p, q)`, and use it to measure distribution drift.
5. Compute the number of tokens needed to detect a given acceptance change, and avoid reporting noise.

---

**1. The metric ladder**

```text
   CHEAP, INSENSITIVE                                    EXPENSIVE, DECISIVE
   ─────────────────────────────────────────────────────────────────────────▶

   weight MSE ──▶ layer-output MSE ──▶ perplexity ──▶ mean KL ──▶ top-1 ──▶ acceptance ──▶ benchmarks
        │              │                    │            │          │           │             │
     seconds        minutes             ~10 min      ~10 min    ~10 min     ~minutes       hours
     tells you      tells you           weak         THE        intuitive   sharp,         ground
     nothing        which layer         signal       GATE       and cheap   free,          truth
     about behavior  moved                                                   online
```

Two rules govern the ladder:

* **Never promote on a lower rung alone.** Weight MSE is a debugging aid, not evidence.
* **Never skip to benchmarks.** They are the ground truth and far too slow to steer a search over dozens of quantization configurations. Use them to *confirm* a decision, not to make it.

---

**2. Why perplexity is a weak instrument**

Perplexity is `exp(mean NLL)` over a corpus ([Logprobs, Perplexity & KL Divergence — Lecture 03](../04-Logprobs-困惑度与KL散度/Lecture-03.md)). Its weakness for quantization grading is structural:

```text
   PPL is a MEAN over tens of thousands of tokens.

   Most tokens are easy: "of the", "  return", "). "  →  the model is confident
   and stays confident after quantization. These dominate the mean and DILUTE
   the signal from the tokens where the damage actually lands.
```

```text
   PPL  16.21  →  16.28     ( +0.4 % )    "looks fine, ship it"
   but underneath:
      98 % of tokens        : unchanged
       2 % of tokens        : top-1 prediction FLIPPED
      long-context behavior : degraded (Module 07 §2)
      tool-call formatting  : intermittently broken
```

Three specific blind spots:

| Blind spot | Why PPL misses it |
|---|---|
| **Tail behavior** | rare-but-critical tokens are averaged away |
| **Long context** | PPL is usually measured with a short sliding window |
| **Distribution shape** | PPL only reads the probability of the *observed* token; the rest of the distribution can deform freely |

That last one is the killer, and it is exactly what KL fixes.

> Use PPL as a **smoke test**: a large jump means something is badly wrong. A small jump means almost nothing. Never ship on PPL alone.

---

</details>

## 3. 可用的门禁：KL 与 top-1 一致率

将量化模型 `q` 与未量化参考 `p` **在同一批输入上** 对比，比较完整的 next-token 分布，而不是只看实际观测到的 token：

```text
   D_KL(p ‖ q)  =  Σ_x  p(x) · log( p(x) / q(x) )
```

它读取的是 *整个* 分布，这正是它比困惑度信息量严格更大的原因。三个数字要一起报告：

```python
def grade_quantization(ref_model, quant_model, dataset, top_k=1):
    kls, agree, probs_ref, probs_q = [], [], [], []
    for batch in dataset:
        with torch.no_grad():
            lp = torch.log_softmax(ref_model(batch).logits.float(),   dim=-1)
            lq = torch.log_softmax(quant_model(batch).logits.float(), dim=-1)
        p = lp.exp()
        kls.append((p * (lp - lq)).sum(-1).flatten())          # per-token KL, nats
        agree.append((lp.argmax(-1) == lq.argmax(-1)).flatten())
        probs_ref.append(p.max(-1).values.flatten())
        probs_q.append(lq.exp().gather(-1, lp.argmax(-1, keepdim=True)).squeeze(-1).flatten())

    kl = torch.cat(kls)
    return {
        "mean_kl":      kl.mean().item(),
        "p99_kl":       kl.quantile(0.99).item(),      # ← the tail PPL hides
        "top1_agree":   torch.cat(agree).float().mean().item(),
        "prob_rms":     (torch.cat(probs_ref) - torch.cat(probs_q)).pow(2).mean().sqrt().item(),
    }
```

可用阈值（按自己的容忍度校准，但下面是站得住脚的起点）：

| 平均 KL（nats） | Top-1 一致率 | 结论 |
|---:|---:|---|
| < 0.01 | > 99 % | 无法区分 — 可以发布 |
| 0.01 – 0.05 | 97–99 % | 多数用途可接受；需在 benchmark 上确认 |
| 0.05 – 0.15 | 93–97 % | 可感知；仅适用于激进的吞吐目标 |
| > 0.15 | < 93 % | 已损坏 — 用 [Module 07 的](Lecture-07.md) sweep 定位是哪一层 |

**始终把 `p99_kl` 与 `mean_kl` 一起报告。** 平均 KL 0.02、p99 KL 3.0 的配置正在灾难性地损伤一小组 token —— 这正是困惑度所掩盖的失效模式。

---

## 4. 接受长度 —— 让它变得严格的那个恒等式

现在是你手里最锐利的廉价仪器。

在投机解码中，draft 模型 `q` 提议一个 token，target `p` 以概率 `min(1, p(x)/q(x))` 接受它（[Module 10](Lecture-10.md)）。**整体** 接受概率为：

```text
   α  =  Σ_x  q(x) · min( 1,  p(x)/q(x) )
      =  Σ_x  min( q(x),  p(x) )
      =  1  −  TV(p, q)
```

其中使用 `Σ min(p,q) = 1 − ½Σ|p−q| = 1 − TV(p,q)`。

```text
   ┌──────────────────────────────────────────────────────────────┐
   │   acceptance rate  =  1  −  total variation distance          │
   │                                between target and draft       │
   └──────────────────────────────────────────────────────────────┘
```

这是精确的，不是近似。而且它有一个容易被忽略的推论：

> 如果保持 **drafter 固定**，只量化 **target**，那么接受率的任何变化都是 **对 target 分布偏移距离的直接测量** —— 以 total variation 度量，在真实推理服务分布上，且在推理服务过程中免费算得。

你其实已经在做这个测量了。只是你可能不知道它是一台分布漂移仪。

### 从接受长度反推 α

runtime 通常报告的是 **接受长度** `τ`（每次 target 前向传播平均输出的 token 数），而不是逐 token 的接受率。每轮有 `K` 个 draft token 时：

```text
   τ  =  1 + α + α² + ... + α^K  =  (1 − α^{K+1}) / (1 − α)
```

数值反解得到 `α`，进而得到 `TV = 1 − α`。将其应用到案例研究的测量结果：

| 配置 | τ | α (K=3) | **TV(p, q)** | 相对基线的 Δ TV |
|---|---:|---:|---:|---:|
| 基线（target 未量化） | 2.886 | 0.7852 | 0.2148 | — |
| MLP + O 量化 | 2.792 | 0.7636 | 0.2364 | **+0.0216** |
| MLP + O + QKV 量化 | 2.546 | 0.7034 | 0.2966 | **+0.0818** |

**加入 QKV 对 target 分布的推动幅度，几乎是量化 MLP + O 的四倍**（TV 上 `+0.082` vs `+0.022`），而 [Module 07](Lecture-07.md) 显示它只换来 1.9 % 的吞吐。接受率这个数字一直都在告诉你这件事。

> `K` 会影响 α 的绝对值（在 `K=2` 下，同样的 τ 意味着 α ≈ 0.96；在 `K=4` 下，α ≈ 0.72），所以 **报告 τ 时务必同时给出 draft 深度。** 各配置的 *排序* 不受 `K` 影响，这就是为什么即使你不确定 drafter 的确切配置，接受率仍能作为比较器使用。


<details>
<summary>English original</summary>

**3. The working gate: KL and top-1 agreement**

Compare your quantized model `q` against the unquantized reference `p` **on the same inputs**, comparing full next-token distributions rather than just the observed token:

```text
   D_KL(p ‖ q)  =  Σ_x  p(x) · log( p(x) / q(x) )
```

This reads the *entire* distribution, which is what makes it strictly more informative than perplexity. Report three numbers together:

```python
def grade_quantization(ref_model, quant_model, dataset, top_k=1):
    kls, agree, probs_ref, probs_q = [], [], [], []
    for batch in dataset:
        with torch.no_grad():
            lp = torch.log_softmax(ref_model(batch).logits.float(),   dim=-1)
            lq = torch.log_softmax(quant_model(batch).logits.float(), dim=-1)
        p = lp.exp()
        kls.append((p * (lp - lq)).sum(-1).flatten())          # per-token KL, nats
        agree.append((lp.argmax(-1) == lq.argmax(-1)).flatten())
        probs_ref.append(p.max(-1).values.flatten())
        probs_q.append(lq.exp().gather(-1, lp.argmax(-1, keepdim=True)).squeeze(-1).flatten())

    kl = torch.cat(kls)
    return {
        "mean_kl":      kl.mean().item(),
        "p99_kl":       kl.quantile(0.99).item(),      # ← the tail PPL hides
        "top1_agree":   torch.cat(agree).float().mean().item(),
        "prob_rms":     (torch.cat(probs_ref) - torch.cat(probs_q)).pow(2).mean().sqrt().item(),
    }
```

Working thresholds (calibrate to your own tolerance, but these are defensible starting points):

| Mean KL (nats) | Top-1 agreement | Verdict |
|---:|---:|---|
| < 0.01 | > 99 % | indistinguishable — ship |
| 0.01 – 0.05 | 97–99 % | acceptable for most uses; confirm on benchmarks |
| 0.05 – 0.15 | 93–97 % | noticeable; only for aggressive throughput targets |
| > 0.15 | < 93 % | broken — find which layer, using [Module 07's](Lecture-07.md) sweep |

**Always report `p99_kl` alongside `mean_kl`.** A configuration with mean KL 0.02 and p99 KL 3.0 is damaging a small set of tokens catastrophically — the exact failure perplexity conceals.

---

**4. Acceptance length — the identity that makes it rigorous**

Now the sharpest cheap instrument you have.

In speculative decoding, a draft model `q` proposes a token, and the target `p` accepts it with probability `min(1, p(x)/q(x))` ([Module 10](Lecture-10.md)). The **overall** acceptance probability is:

```text
   α  =  Σ_x  q(x) · min( 1,  p(x)/q(x) )
      =  Σ_x  min( q(x),  p(x) )
      =  1  −  TV(p, q)
```

using `Σ min(p,q) = 1 − ½Σ|p−q| = 1 − TV(p,q)`.

```text
   ┌──────────────────────────────────────────────────────────────┐
   │   acceptance rate  =  1  −  total variation distance          │
   │                                between target and draft       │
   └──────────────────────────────────────────────────────────────┘
```

This is exact, not an approximation. And it has a consequence that is easy to miss:

> If you hold the **drafter fixed** and quantize only the **target**, then any change in acceptance rate is a **direct measurement of how far the target's distribution moved** — in total variation, over the real serving distribution, computed for free while serving.

You are already running this measurement. You may just not have known it was a distribution-drift meter.

**Recovering α from acceptance length**

Runtimes usually report **acceptance length** `τ` (mean tokens emitted per target forward pass) rather than the per-token rate. With `K` draft tokens per cycle:

```text
   τ  =  1 + α + α² + ... + α^K  =  (1 − α^{K+1}) / (1 − α)
```

Invert numerically to recover `α`, then `TV = 1 − α`. Applying this to the case-study measurements:

| Config | τ | α (K=3) | **TV(p, q)** | Δ TV vs baseline |
|---|---:|---:|---:|---:|
| Baseline (target unquantized) | 2.886 | 0.7852 | 0.2148 | — |
| MLP + O quantized | 2.792 | 0.7636 | 0.2364 | **+0.0216** |
| MLP + O + QKV quantized | 2.546 | 0.7034 | 0.2966 | **+0.0818** |

**Adding QKV moved the target distribution nearly four times as far as quantizing MLP + O did** (`+0.082` vs `+0.022` in TV), while [Module 07](Lecture-07.md) showed it bought only 1.9 % throughput. The acceptance number was telling you that the whole time.

> `K` matters for the absolute α values (at `K=2` the same τ implies α ≈ 0.96; at `K=4`, α ≈ 0.72), so **always report your draft depth alongside τ.** The *ranking* of configurations is unaffected by `K`, which is why acceptance works as a comparator even when you are unsure of the drafter's exact configuration.

</details>

### 为什么这优于困惑度

| | 困惑度 | 接受长度 |
|---|---|---|
| 读取内容 | 观测到的 token 的概率 | 完整分布，通过 TV |
| 分布 | 静态语料库 | **你实际的推理服务流量** |
| 成本 | 一次单独的评估运行 | **免费——已经在跑** |
| 信号延迟 | ~10 分钟 | **连续、在线** |
| 对尾部损伤的敏感度 | 差（被平均掉） | 好（拒绝集中在那里） |

唯一的注意点：**接受率是与你的 drafter 作比较，而不是与真值作比较。** 如果你把 drafter 也量化了，两个分布都会移动，测量就被混淆了。在一次量化扫描中要保持 drafter 固定——这是一个受控实验要求，[Module 12](Lecture-12.md) 正是这样对待它的。

---

## 5. 要看多少个 token 才能相信这个数字？

接受率是一个伯努利过程，因此由 `N` 个 draft token 得到的 `α` 的标准误是 `√(α(1−α)/N)`：

| α | 目标 SE | 需要的 draft token |
|---:|---:|---:|
| 0.75 | 0.010 | ~1,900 |
| 0.75 | 0.005 | ~7,500 |
| 0.75 | 0.002 | ~46,900 |
| 0.80 | 0.005 | ~6,400 |

要在两个配置之间声称 **1 个点**的接受率差异，每个配置大约需要 **8,000 个 draft token**——几分钟的生成量。要声称 0.4 个点的差异，则需要 ~47,000 个。

```text
   Reported: "acceptance dropped from 2.886 to 2.871"
   Measured over: 500 tokens
   ⟹  SE ≈ 0.019 on α;  the difference is INSIDE the noise.
       This is not a result. It is a coin flip with a decimal point.
```

大多数已发表的接受率对比都没有说明样本量。你的应该说明——如果差值落在两个标准误之内，就把它报告为「无可检测差异」，而不是小幅改进。

---

## 6. 组装好的关卡

按顺序执行，遇到第一个失败就停下：

```text
   1.  layer-output MSE          seconds   →  did the quantization run correctly at all?
   2.  perplexity                ~10 min   →  smoke test; large jump = something is broken
   3.  mean KL + p99 KL          ~10 min   →  THE GATE (thresholds in §3)
   4.  top-1 agreement           free      →  intuitive cross-check on the same run
   5.  acceptance length         minutes   →  drift measured on real traffic; ≥8k draft tokens
   6.  task benchmarks           hours     →  confirm the decision; never steer with it
   7.  long-context probe        hours     →  Modules 07 §2, 09 — DO NOT SKIP if you serve long context
```

第 7 步是各团队最容易省略、也最容易后悔的一步。上面除最后一项外的每个指标通常都在短上下文下测量，而 [Module 07 的](Lecture-07.md) RoPE 论证预测 Q/K 损伤会随上下文长度*增长*。在 4 K 下通过的配置可能在 262 K 下失败，而第 1–6 步中没有任何东西会提醒你。

---

## 检查点

现在你应该能够：

1. 排列指标阶梯，并说出每一级的成本与敏感度。
2. 解释困惑度在结构上无法看到的三个东西。
3. 设定 KL 与 top-1 阈值，并解释为什么 `p99_kl` 必须伴随 `mean_kl`。
4. 从拒绝采样规则推导出 `α = 1 − TV(p, q)`。
5. 把接受长度与 draft 深度换算成 TV 距离。
6. 计算支撑一个接受率结论所需的样本量。

---

## 交付

构建一个 **quant grader**，每个配置输出一行：

```text
   config | B_token | tok/s | PPL | mean_KL | p99_KL | top1% | τ | α | TV | n_draft_tokens | verdict
```

把它在 [Module 07](Lecture-07.md) 的 leave-one-out 配置上跑一遍。交付物是这张表**外加一份事先写定的晋升规则**——你将要上线的 KL 与接受率阈值，在*看到结果之前*就已确定。正是这种先后顺序，才让它成为一次实验而不是一次合理化解释。

---

## 时效性

* **不随时间变化：** 指标阶梯、KL 关卡、`α = 1 − TV(p,q)`、`τ = (1−α^{K+1})/(1−α)` 关系、伯努利样本量计算。
* **前置深度：** [Logprobs, Perplexity & KL Divergence](../04-Logprobs-困惑度与KL散度/README.md) 推导了这里用到的信息论恒等式；[Lecture 05](../04-Logprobs-困惑度与KL散度/Lecture-05.md) 覆盖了 llama.cpp 的 `--kl-divergence` 评分模式。
* **案例锚定：** τ 值 2.886 / 2.792 / 2.546。α 与 TV 两列假设 draft 深度为 `K = 3`；请针对你自己的 `K` 重新计算。

---

**下一篇：** [Module 09 — KV Cache & Long Context →](Lecture-09.md)


<details>
<summary>English original</summary>

**Why this beats perplexity**

| | Perplexity | Acceptance length |
|---|---|---|
| Reads | probability of the observed token | full distribution, via TV |
| Distribution | a static corpus | **your actual serving traffic** |
| Cost | a separate evaluation run | **free — already running** |
| Latency to signal | ~10 minutes | **continuous, online** |
| Sensitivity to tail damage | poor (averaged away) | good (rejections concentrate there) |

The one caveat: **acceptance is a comparison against your drafter, not against truth.** If you quantize the drafter too, both distributions move and the measurement is confounded. Keep the drafter fixed across a quantization sweep — this is a controlled-experiment requirement, and [Module 12](Lecture-12.md) treats it as such.

---

**5. How many tokens before you believe the number?**

Acceptance is a Bernoulli process, so the standard error on `α` from `N` draft tokens is `√(α(1−α)/N)`:

| α | Target SE | Draft tokens needed |
|---:|---:|---:|
| 0.75 | 0.010 | ~1,900 |
| 0.75 | 0.005 | ~7,500 |
| 0.75 | 0.002 | ~46,900 |
| 0.80 | 0.005 | ~6,400 |

To claim a **1-point** acceptance difference between two configurations you need roughly **8,000 draft tokens per configuration** — a couple of minutes of generation. To claim a 0.4-point difference you need ~47,000.

```text
   Reported: "acceptance dropped from 2.886 to 2.871"
   Measured over: 500 tokens
   ⟹  SE ≈ 0.019 on α;  the difference is INSIDE the noise.
       This is not a result. It is a coin flip with a decimal point.
```

Most published acceptance comparisons do not state their sample size. Yours should — and if the delta is inside two standard errors, report it as "no detectable difference", not as a small improvement.

---

**6. The gate, assembled**

Run this in order and stop at the first failure:

```text
   1.  layer-output MSE          seconds   →  did the quantization run correctly at all?
   2.  perplexity                ~10 min   →  smoke test; large jump = something is broken
   3.  mean KL + p99 KL          ~10 min   →  THE GATE (thresholds in §3)
   4.  top-1 agreement           free      →  intuitive cross-check on the same run
   5.  acceptance length         minutes   →  drift measured on real traffic; ≥8k draft tokens
   6.  task benchmarks           hours     →  confirm the decision; never steer with it
   7.  long-context probe        hours     →  Modules 07 §2, 09 — DO NOT SKIP if you serve long context
```

Step 7 is the one teams omit and regret. Every metric above except the last is typically measured at short context, and [Module 07's](Lecture-07.md) RoPE argument predicts that Q/K damage *grows* with context length. A configuration that passes at 4 K can fail at 262 K, and nothing in steps 1–6 will warn you.

---

**Checkpoint**

You should now be able to:

1. Order the metric ladder and state each rung's cost and sensitivity.
2. Explain the three things perplexity structurally cannot see.
3. Set KL and top-1 thresholds, and explain why `p99_kl` must accompany `mean_kl`.
4. Derive `α = 1 − TV(p, q)` from the rejection-sampling rule.
5. Convert an acceptance length and draft depth into a TV distance.
6. Compute the sample size needed to support an acceptance claim.

---

**Ship it**

Build a **quant grader** that emits one row per configuration:

```text
   config | B_token | tok/s | PPL | mean_KL | p99_KL | top1% | τ | α | TV | n_draft_tokens | verdict
```

Run it across the [Module 07](Lecture-07.md) leave-one-out configurations. The deliverable is the table **plus a written promotion rule** fixed in advance — the KL and acceptance thresholds you will ship at, committed *before* you see the results. That ordering is what makes it an experiment rather than a rationalization.

---

**Current as of**

* **Timeless:** the metric ladder, the KL gate, `α = 1 − TV(p,q)`, the `τ = (1−α^{K+1})/(1−α)` relation, the Bernoulli sample-size arithmetic.
* **Prerequisite depth:** [Logprobs, Perplexity & KL Divergence](../04-Logprobs-困惑度与KL散度/README.md) derives the information-theoretic identities used here; [Lecture 05](../04-Logprobs-困惑度与KL散度/Lecture-05.md) covers llama.cpp's `--kl-divergence` grading mode.
* **Case-study pins:** τ values 2.886 / 2.792 / 2.546. The α and TV columns assume draft depth `K = 3`; recompute for your own `K`.

---

**Next:** [Module 09 — KV Cache & Long Context →](Lecture-09.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/Hardware-Aware LLM Quantization/Lecture-08.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/Hardware-Aware%20LLM%20Quantization/Lecture-08.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
