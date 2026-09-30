---
title: Module 09 — KV cache 与长上下文
description: Module 09 — KV cache 与长上下文
---

# Module 09 — KV cache 与长上下文

**合集：** [Hardware-Aware LLM Quantization](README.md) | **上一节：** [← Module 08](Lecture-08.md) | **下一节：** [Module 10 →](Lecture-10.md)

---

到目前为止，每一份账本都把 KV cache 当成脚注。在 4 K 上下文下它确实是。在 262 K 下，它**与整个模型的权重共同主导**，正确的优化策略随之反转。

本模块精确计算这个反转点，并说明在 32 GB 卡上做 262 K 上下文目标，不是靠更狠地压缩权重就能解决的量化问题——它是架构与 KV 格式的问题，存在硬性的可行性边界。

---

## 学习目标

学完本模块，你应当能够：

1. 根据模型的 GQA 几何结构，计算每 token 的 KV cache 字节数。
2. 计算 KV 流量等于权重流量处的**交叉点上下文长度**。
3. 预测 tok/s 随上下文长度的变化。
4. 在运行任何东西之前，判定给定的（模型、上下文、VRAM）三元组是否**根本可行**。
5. 解释为什么 KV 量化与权重量化是不同的问由——它是在线的、逐 head 的、且对延迟敏感。

---

## 1. KV cache 的计算

缓存为每个 layer、每个 token 存储一份 K 向量和一份 V 向量，按 **KV head** 计（而不是按 query head——这正是 GQA 换来的东西）：

```text
   KV_bytes_per_token  =  2  ×  n_layers  ×  n_kv_heads  ×  head_dim  ×  bytes_per_element
                          │                  │
                     K and V          GQA groups, NOT query heads
```

对于案例研究的几何结构（48 个 layer，`head_dim` 128——由于公开清单未确定 `n_kv_heads`，两种取值都列出）：

| `n_kv` | 每 token 元素数 | BF16 | FP8 | NVFP4 |
|---:|---:|---:|---:|---:|
| 8 | 98,304 | **192 KiB** | 96 KiB | 54 KiB |
| 4 | 49,152 | 96 KiB | **48 KiB** | 27 KiB |

由此立刻得到两点，且都是结构性的：

* **KV 开销与 `n_kv_heads` 成线性。** KV head 减半，容量和流量都减半——这比任何 KV 量化格式都更有力。这正是 GQA（以及 MLA）存在的原因，也是为什么在这里架构选择压过格式选择。
* **KV 开销与 `d_model` 和 `d_ff` 无关。** 更宽的 MLP 增加的是权重开销，而不是缓存。权重优化与 KV 优化是**两个解耦的问题**。

---

## 2. 交叉点：KV 流量何时超过权重

在每个 decode 步骤（逐 token 生成阶段），attention 都要读取所有先前 token 的**全部**缓存。因此 KV 流量随上下文线性增长，而权重流量保持不变：

```text
   B_token(L)  =  W  +  L × KV_bytes_per_token
                  │      └──────────────────┘
             15.88 GB      grows with context
```

令两项相等，即得交叉点：

```text
   L_crossover  =  W / KV_bytes_per_token
```

| `n_kv` | KV 格式 | 每 token KV | **交叉点** |
|---:|---|---:|---:|
| 8 | BF16 | 192 KiB | **80,770 tokens** |
| 8 | FP8 | 96 KiB | 161,540 |
| 8 | NVFP4 | 54 KiB | 287,182 |
| 4 | BF16 | 96 KiB | 161,540 |
| 4 | FP8 | 48 KiB | 323,079 |
| 4 | NVFP4 | 27 KiB | 574,363 |

```text
   traffic
     ▲
     │                                          ╱ KV (grows with L)
     │                                       ╱
     │                                    ╱
  W ─┼─────────────────────────────────╳──────────────  weights (flat)
     │                              ╱   │
     │                           ╱      └── crossover
     │                        ╱
     └──────────────────────────────────────────────▶  context length L

   L < crossover  :  a WEIGHT-dominated problem  →  Modules 04–07 apply
   L > crossover  :  a KV-dominated problem      →  weight quantization stops helping
```

> **在 BF16 KV 和 8 个 KV head 下，交叉点约为 81 K token。** 超过这一点，继续做权重量化只是在攻击两项中较小的那一项。如果你以 262 K 服务，那么在大部分上下文窗口里，你都站在这条线的错误一侧。

---

## 3. 吞吐与上下文的关系

在实测 `BW_eff = 1296 GB/s`、4 个 KV head 和 FP8 KV 下的预测 decode 吞吐：

| 上下文 | KV 流量 | `B_token` | **tok/s** | 对比短上下文 |
|---:|---:|---:|---:|---:|
| 0 | 0.00 GB | 15.88 GB | **81.6** | — |
| 4 K | 0.20 GB | 16.08 GB | 80.6 | −1 % |
| 32 K | 1.61 GB | 17.49 GB | 74.1 | −9 % |
| 128 K | 6.44 GB | 22.32 GB | 58.1 | −29 % |
| **262 K** | **12.88 GB** | **28.76 GB** | **45.1** | **−45 %** |

**跨越整个上下文窗口，decode 吞吐几乎减半**——即使已用 FP8 KV 且只有 4 个 KV head，而这已经是相当激进的配置。

这对 benchmark 的诚实性有直接影响：**不给出上下文长度的 tok/s 数字毫无意义。** 在 4 K 下跑 benchmark 却在 262 K 下部署，会把真实吞吐高估约 80 %。正是出于这个原因，[Module 12](Lecture-12.md) 把上下文当作必须报告的变量。

---


<details>
<summary>English original</summary>

**Module 09 — KV Cache & Long Context**

**Collection:** [Hardware-Aware LLM Quantization](README.md) | **Previous:** [← Module 08](Lecture-08.md) | **Next:** [Module 10 →](Lecture-10.md)

---

Every ledger so far has treated the KV cache as a footnote. At 4 K context it is one. At 262 K it is **co-dominant with the entire model's weights**, and the correct optimization strategy inverts.

This module computes the inversion point exactly, and shows that a 262 K context target on a 32 GB card is not a quantization problem you can solve by compressing weights harder — it is an architecture-and-KV-format problem with a hard feasibility boundary.

---

**Learning objectives**

By the end of this module you should be able to:

1. Compute KV cache bytes per token from a model's GQA geometry.
2. Compute the **crossover context length** where KV traffic equals weight traffic.
3. Predict tok/s as a function of context length.
4. Determine whether a given (model, context, VRAM) triple is **feasible at all**, before running anything.
5. Explain why KV quantization is a different problem from weight quantization — online, per-head, and latency-critical.

---

**1. KV cache arithmetic**

The cache stores one K and one V vector per **KV head** (not per query head — that is what GQA buys), per layer, per token:

```text
   KV_bytes_per_token  =  2  ×  n_layers  ×  n_kv_heads  ×  head_dim  ×  bytes_per_element
                          │                  │
                     K and V          GQA groups, NOT query heads
```

For the case-study geometry (48 layers, `head_dim` 128 — with `n_kv_heads` shown both ways since the published inventory does not pin it):

| `n_kv` | Elements/token | BF16 | FP8 | NVFP4 |
|---:|---:|---:|---:|---:|
| 8 | 98,304 | **192 KiB** | 96 KiB | 54 KiB |
| 4 | 49,152 | 96 KiB | **48 KiB** | 27 KiB |

Two things follow immediately, and both are structural:

* **KV cost is linear in `n_kv_heads`.** Halving KV heads halves both capacity and traffic — a bigger lever than any KV quantization format. This is why GQA (and MLA) exist, and why architecture choices dominate format choices here.
* **KV cost is independent of `d_model` and `d_ff`.** A wider MLP costs weights, not cache. Weight optimization and KV optimization are **decoupled problems**.

---

**2. The crossover: when KV traffic overtakes the weights**

At every decode step, attention reads the **entire** cache for all previous tokens. So KV traffic grows linearly with context while weight traffic stays flat:

```text
   B_token(L)  =  W  +  L × KV_bytes_per_token
                  │      └──────────────────┘
             15.88 GB      grows with context
```

Setting the two terms equal gives the crossover:

```text
   L_crossover  =  W / KV_bytes_per_token
```

| `n_kv` | KV format | KV/token | **Crossover** |
|---:|---|---:|---:|
| 8 | BF16 | 192 KiB | **80,770 tokens** |
| 8 | FP8 | 96 KiB | 161,540 |
| 8 | NVFP4 | 54 KiB | 287,182 |
| 4 | BF16 | 96 KiB | 161,540 |
| 4 | FP8 | 48 KiB | 323,079 |
| 4 | NVFP4 | 27 KiB | 574,363 |

```text
   traffic
     ▲
     │                                          ╱ KV (grows with L)
     │                                       ╱
     │                                    ╱
  W ─┼─────────────────────────────────╳──────────────  weights (flat)
     │                              ╱   │
     │                           ╱      └── crossover
     │                        ╱
     └──────────────────────────────────────────────▶  context length L

   L < crossover  :  a WEIGHT-dominated problem  →  Modules 04–07 apply
   L > crossover  :  a KV-dominated problem      →  weight quantization stops helping
```

> **With BF16 KV and 8 KV heads, the crossover is ~81 K tokens.** Past that point, further weight quantization is attacking the smaller of the two terms. If you serve at 262 K, you have been on the wrong side of this line for most of your context window.

---

**3. Throughput versus context**

Predicted decode throughput at the measured `BW_eff = 1296 GB/s`, with 4 KV heads and FP8 KV:

| Context | KV traffic | `B_token` | **tok/s** | vs short context |
|---:|---:|---:|---:|---:|
| 0 | 0.00 GB | 15.88 GB | **81.6** | — |
| 4 K | 0.20 GB | 16.08 GB | 80.6 | −1 % |
| 32 K | 1.61 GB | 17.49 GB | 74.1 | −9 % |
| 128 K | 6.44 GB | 22.32 GB | 58.1 | −29 % |
| **262 K** | **12.88 GB** | **28.76 GB** | **45.1** | **−45 %** |

**Decode throughput nearly halves across the context window** — even with FP8 KV and only 4 KV heads, which is already an aggressive configuration.

This has a direct consequence for benchmarking honesty: **a tok/s number without a context length is meaningless.** A benchmark run at 4 K and deployed at 262 K overstates real throughput by ~80 %. [Module 12](Lecture-12.md) treats context as a mandatory reported variable for exactly this reason.

---

</details>

## 4. 可行性：262 K 到底放得下吗？

容量，而非流量，才是更难满足的约束。预算如下：

```text
   32 GB card                    =  29.80 GiB
   − weights                      −  18.80 GiB
   − workspace / activations      −   1.00 GiB  (approx.)
   ────────────────────────────────────────────
   available for KV               ≈  10.00 GiB
```

再看每种配置下 262 144 个 token 的缓存：

| `n_kv` | 格式 | 262 K 缓存 | 能放进 10 GiB 吗？ |
|---:|---|---:|:---:|
| 8 | BF16 | 48.00 GiB | ✗（超出 4.8×） |
| 8 | FP8 | 24.00 GiB | ✗（超出 2.4×） |
| 8 | NVFP4 | 13.50 GiB | ✗（超出 1.35×） |
| 4 | BF16 | 24.00 GiB | ✗ |
| 4 | FP8 | 12.00 GiB | ✗（临界） |
| **4** | **NVFP4** | **6.75 GiB** | **✓** |

```text
   262 K context on a 32 GB card with 18.8 GiB of weights requires
   BOTH  4 KV heads  AND  4-bit KV.  Every other combination overflows.
```

还要注意哪些做法**不**起作用：量化 vision tower（−0.86 GiB）或 embedding（−1.18 GiB）能换来 VRAM，但远不足以救回 8 个 KV head 的 BF16 配置，它超预算 38 GiB。

**正是在这里，[Module 01](Lecture-01.md) 中「容量优化 ≠ 带宽优化」的区分终于在相反方向上有了回报。** 换出 vision tower、量化 embedding，给你带来的 tok/s 增量是 *零* —— 但在 262 K 下，它们就是 2.04 GiB 的余量，而余量恰恰是最稀缺的东西。**同一个改动，对一个目标毫无价值，对另一个目标却价值巨大。** 这正是该框架对四个因素而不是一个因素打分的原因。

长上下文下的实际执行顺序：

```text
   1. reduce n_kv_heads          ← architecture; biggest lever, but requires the model to have it
   2. quantize KV to FP8/FP4     ← format; 2–3.6× capacity AND traffic
   3. evict Class B/C tensors    ← vision tower, unused adapters: pure capacity
   4. quantize embeddings        ← pure capacity
   5. quantize weights harder    ← LAST; you are attacking the smaller term
```

注意这个列表与 [Module 01 §8](Lecture-01.md) 中的短上下文优先级顺序**恰好相反**。

---

## 5. KV 量化是另一类工程问题

权重量化是离线的、一次性的，你可以在每个张量上花一个小时。KV 量化这三条一条都不占：

```text
   WEIGHTS                          KV CACHE
   ──────────────────────────       ───────────────────────────────────
   quantized once, offline          quantized ONLINE, every token
   full calibration available       no future data — must be causal
   error is static                  error accumulates over the context
   cost amortized to zero           cost is IN the decode critical path
```

有四个后果决定了任何可行设计的形态：

**1. 缩放必须逐 head、逐 token。** 这些是 [Module 06 §4](Lecture-06.md) 中给出的合法轴 —— 对 K 向量做逐 token 的 scale，会从 attention logit 中约掉，因此是免费的。而跨 `head_dim` 做逐通道 scale 则不行。

**2. 量化器必须廉价。** 它跑在 decode（逐 token 生成阶段）的关键路径上。MSE 网格搜索（[Module 05](Lecture-05.md)）根本不用考虑；你负担得起的只有 absmax 或滑动百分位数。

**3. 误差会持续存在。** 权重误差在每个 token 上都一样。KV 误差写一次，然后**在序列中后续每一个 token 上被反复读取** —— 一个早期 token 的量化误差会影响下游全部 262 K 步。这就支持把前几个 token 的 KV 保持更高精度。

**4. 这一点直接关联到 attention sink。** [Module 06 §3](Lecture-06.md) 表明，第一个 token 携带 massive 激活值，并吸收多余的 attention 质量。它也是被重读次数最多的条目。两个论据指向同一方向：

```text
   keep the first N tokens' KV in BF16/FP8  (N ≈ 4–128, cheap: 128 tokens × 48 KiB = 6 MB)
   quantize the rest to FP4

   → protects the sink mechanism
   → protects the most-re-read entries
   → costs ~0.06 % of the cache
```

这种「保 sink 的 KV 量化」模式在当下的长上下文系统中很常见，而它两半论证依据都是你在前面模块里推导过的东西。

---


<details>
<summary>English original</summary>

**4. Feasibility: does 262 K fit at all?**

Capacity, not traffic, is the harder constraint. The budget:

```text
   32 GB card                    =  29.80 GiB
   − weights                      −  18.80 GiB
   − workspace / activations      −   1.00 GiB  (approx.)
   ────────────────────────────────────────────
   available for KV               ≈  10.00 GiB
```

Now the 262 144-token cache under each configuration:

| `n_kv` | Format | 262 K cache | Fits in 10 GiB? |
|---:|---|---:|:---:|
| 8 | BF16 | 48.00 GiB | ✗ (4.8× over) |
| 8 | FP8 | 24.00 GiB | ✗ (2.4× over) |
| 8 | NVFP4 | 13.50 GiB | ✗ (1.35× over) |
| 4 | BF16 | 24.00 GiB | ✗ |
| 4 | FP8 | 12.00 GiB | ✗ (marginal) |
| **4** | **NVFP4** | **6.75 GiB** | **✓** |

```text
   262 K context on a 32 GB card with 18.8 GiB of weights requires
   BOTH  4 KV heads  AND  4-bit KV.  Every other combination overflows.
```

And notice what does **not** help: quantizing the vision tower (−0.86 GiB) or the embeddings (−1.18 GiB) buys VRAM but not nearly enough to rescue an 8-KV-head BF16 configuration, which is 38 GiB over budget.

**This is where the "capacity optimization ≠ bandwidth optimization" distinction from [Module 01](Lecture-01.md) finally pays off in the other direction.** Evicting the vision tower and quantizing embeddings gained you *zero* tok/s — but at 262 K they are 2.04 GiB of headroom, and headroom is exactly what is scarce. **The same change is worthless for one objective and valuable for the other.** That is the whole reason the framework scores four factors instead of one.

Practical order of operations at long context:

```text
   1. reduce n_kv_heads          ← architecture; biggest lever, but requires the model to have it
   2. quantize KV to FP8/FP4     ← format; 2–3.6× capacity AND traffic
   3. evict Class B/C tensors    ← vision tower, unused adapters: pure capacity
   4. quantize embeddings        ← pure capacity
   5. quantize weights harder    ← LAST; you are attacking the smaller term
```

Note that the list is **exactly inverted** from the short-context priority order in [Module 01 §8](Lecture-01.md).

---

**5. KV quantization is a different engineering problem**

Weight quantization is offline, one-shot, and you can spend an hour per tensor. KV quantization is none of those things:

```text
   WEIGHTS                          KV CACHE
   ──────────────────────────       ───────────────────────────────────
   quantized once, offline          quantized ONLINE, every token
   full calibration available       no future data — must be causal
   error is static                  error accumulates over the context
   cost amortized to zero           cost is IN the decode critical path
```

Four consequences that shape any workable design:

**1. Scaling must be per-head and per-token.** These are the legal axes from [Module 06 §4](Lecture-06.md) — a per-token scale on the K vector factors out of the attention logit, so it is free. A per-channel scale across `head_dim` does not.

**2. The quantizer must be cheap.** It runs on the decode critical path. An MSE grid search ([Module 05](Lecture-05.md)) is out of the question; absmax or a running percentile is what you can afford.

**3. Errors persist.** A weight error is the same on every token. A KV error is written once and then **re-read for every subsequent token in the sequence** — an early-token quantization error influences all 262 K downstream steps. This argues for keeping the first few tokens' KV in higher precision.

**4. Which connects directly to attention sinks.** [Module 06 §3](Lecture-06.md) showed the first token carries massive activations and absorbs surplus attention mass. It is also the entry that gets re-read most often. Both arguments point the same way:

```text
   keep the first N tokens' KV in BF16/FP8  (N ≈ 4–128, cheap: 128 tokens × 48 KiB = 6 MB)
   quantize the rest to FP4

   → protects the sink mechanism
   → protects the most-re-read entries
   → costs ~0.06 % of the cache
```

This "sink-preserving KV quantization" pattern is common in current long-context systems, and both halves of its justification are things you derived in earlier modules.

---

</details>

## 6. 长上下文台账

扩展 [Module 04 的](Lecture-04.md) 台账，加入上下文项：

```python
def ledger_at_context(B_token_weights_GB, n_layers, n_kv, head_dim,
                      kv_bytes_per_elem, context_len,
                      vram_GB=32.0, weights_GiB=18.80, workspace_GiB=1.0,
                      bw_eff_GBs=1296):
    GiB = 1024**3
    kv_per_token = 2 * n_layers * n_kv * head_dim * kv_bytes_per_elem   # bytes
    kv_traffic   = context_len * kv_per_token / 1e9                     # GB per decode step
    kv_capacity  = context_len * kv_per_token / GiB                     # GiB resident

    b_token   = B_token_weights_GB + kv_traffic
    available = vram_GB * 1e9 / GiB - weights_GiB - workspace_GiB

    return {
        "kv_per_token_KiB": kv_per_token / 1024,
        "crossover_tokens": B_token_weights_GB * 1e9 / kv_per_token,
        "B_token_GB":       b_token,
        "predicted_tps":    bw_eff_GBs / b_token,
        "kv_capacity_GiB":  kv_capacity,
        "fits":             kv_capacity <= available,
        "headroom_GiB":     available - kv_capacity,
    }
```

在选择 KV 格式**之前**，先在完整上下文范围内跑一遍。它能在微秒级回答可行性问题，而在消费级显卡上，100 K+ 上下文下不可行的配置极其常见。

---

## 检查点

现在你应当能够：

1. 根据分组查询注意力几何计算 KV 字节/token，并解释为何 `d_model` 不出现。
2. 计算交叉点的上下文长度，并判断自己处在它的哪一侧。
3. 预测一段上下文范围内的 tok/s 衰减曲线。
4. 在跑任何东西之前，判定 (model, context, VRAM) 三元组的可行性。
5. 给出把 sink token 的 KV 保留在更高精度的两条理由。
6. 解释为何长上下文的优化顺序与短上下文相反。

---

## 交付

为你的部署产出一次**上下文扫描**：每种格式下你的几何结构对应的 KV 字节/token；每种格式的交叉点长度；一张从 0 到你最大值的 tok/s vs 上下文表格；一张带余量的可行性表；以及一份书面说明，指出你的 **p50 与 p99 推理服务上下文长度**落在交叉点的哪一侧。

如果 p99 越过了交叉点，你的下一个优化目标是 KV，而不是权重——无论这门课其余部分让你多想动权重。

---

## 截至当前

* **恒久有效：** KV 算术、交叉点推导、容量/流量之别、按 token 缩放的合法性、sink 保留论证。
* **案例锚定值：** `W = 15.88 GB`、`BW_eff = 1296 GB/s`、48 layers / `head_dim` 128、32 GB VRAM。`n_kv_heads` **没有**被已公开的清单锚定——表中同时列出 8 和 4；请替换为你模型的实际取值。
* **需刷新的部分：** FP4 KV cache 的 runtime 支持不如 FP8 成熟。在围绕 4-bit KV 做规划之前，先核实你的推理服务栈实际实现了什么；不受支持的格式是不可行的方案，而不是激进的方案。

---

**下一章：** [Module 10 — 投机解码 →](Lecture-10.md)


<details>
<summary>English original</summary>

**6. The long-context ledger**

Extend [Module 04's](Lecture-04.md) ledger with the context term:

```python
def ledger_at_context(B_token_weights_GB, n_layers, n_kv, head_dim,
                      kv_bytes_per_elem, context_len,
                      vram_GB=32.0, weights_GiB=18.80, workspace_GiB=1.0,
                      bw_eff_GBs=1296):
    GiB = 1024**3
    kv_per_token = 2 * n_layers * n_kv * head_dim * kv_bytes_per_elem   # bytes
    kv_traffic   = context_len * kv_per_token / 1e9                     # GB per decode step
    kv_capacity  = context_len * kv_per_token / GiB                     # GiB resident

    b_token   = B_token_weights_GB + kv_traffic
    available = vram_GB * 1e9 / GiB - weights_GiB - workspace_GiB

    return {
        "kv_per_token_KiB": kv_per_token / 1024,
        "crossover_tokens": B_token_weights_GB * 1e9 / kv_per_token,
        "B_token_GB":       b_token,
        "predicted_tps":    bw_eff_GBs / b_token,
        "kv_capacity_GiB":  kv_capacity,
        "fits":             kv_capacity <= available,
        "headroom_GiB":     available - kv_capacity,
    }
```

Run it across your full context range **before** choosing a KV format. It answers the feasibility question in microseconds, and infeasible configurations are extremely common at 100 K+ on consumer cards.

---

**Checkpoint**

You should now be able to:

1. Compute KV bytes/token from GQA geometry and explain why `d_model` does not appear.
2. Compute the crossover context length and interpret which side of it you are on.
3. Predict the tok/s decay curve across a context range.
4. Determine feasibility of a (model, context, VRAM) triple before running anything.
5. Give both reasons for keeping sink-token KV in higher precision.
6. Explain why the long-context optimization order is the inverse of the short-context one.

---

**Ship it**

Produce a **context sweep** for your deployment: KV bytes/token for your geometry in each format; the crossover length for each; a tok/s-vs-context table from 0 to your maximum; a feasibility table with headroom; and a written statement of which side of the crossover your **p50 and p99 serving context lengths** fall on.

If p99 is past the crossover, your next optimization is KV, not weights — regardless of what the rest of this course made you want to do.

---

**Current as of**

* **Timeless:** KV arithmetic, the crossover derivation, the capacity/traffic distinction, per-token scaling legality, the sink-preservation argument.
* **Case-study pins:** `W = 15.88 GB`, `BW_eff = 1296 GB/s`, 48 layers / `head_dim` 128, 32 GB VRAM. `n_kv_heads` is **not** pinned by the published inventory — both 8 and 4 are tabulated; substitute your model's actual value.
* **Refresh surface:** runtime support for FP4 KV cache is less mature than for FP8. Verify what your serving stack actually implements before planning around 4-bit KV; an unsupported format is an infeasible plan, not an aggressive one.

---

**Next:** [Module 10 — Speculative Decoding →](Lecture-10.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/Hardware-Aware LLM Quantization/Lecture-09.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/Hardware-Aware%20LLM%20Quantization/Lecture-09.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
