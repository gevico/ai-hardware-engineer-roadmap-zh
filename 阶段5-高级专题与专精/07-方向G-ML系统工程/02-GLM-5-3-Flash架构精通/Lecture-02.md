---
title: Module 02 — MoE：容量、计算量与访存流量
description: Module 02 — MoE：容量、计算量与访存流量
---

# Module 02 — MoE：容量、计算量与访存流量

**Collection:** [GLM-5.3-Flash Architecture Mastery](README.md) | **Previous:** [← Module 01](Lecture-01.md) | **Next:** [Module 03 →](Lecture-03.md)

---

"18B active parameters" 是每个人都在复述的数字，也是让最多工程师低估这个模型推理服务成本的那个数字。本模块把 MoE 压缩进一句流行话术里的三个量拆开，推导 router 的实际算术（它并不是“softmax, then top-8”），并精确计算检查点的大小如何在稀疏激活下保持不变。

---

## 学习目标

读完本模块后，你应当能够：

1. 区分 MoE 的**容量**、**计算量**与**访存流量**，并举出一个三者中有两者独立变化的例子。
2. 写出 router 的实际计算 —— sigmoid 打分、用于*选择*的加性修正偏置、用于*加权*的重新归一化后的原始分数 —— 并解释为什么这是两个不同量的两种不同用途。
3. 写出 clamped-SwiGLU 专家函数的表达式，并指出它的非对称性。
4. 从检查点的维度推导单专家与全部路由专家的参数量。
5. 解释为什么一个选择正确、但加权错误的 kernel 会产生看似合理却错误的输出。

---

## 1. MoE 拆开来的三个量

对于前馈子层的输入 `x`，输出为：

```text
   y  =  f_shared(x)  +  Σ_{e in E(x)}  w_e(x) · f_e(x)
```

其中 `E(x)` 是为该 token 选出的 8 个路由专家的集合，而共享专家 `f_shared` 对每个 token 都无条件参与计算。这单个等式掩盖了三个行为截然不同的量：

```text
   CAPACITY    :  all 288 routed + 1 shared expert weights that EXIST in the checkpoint
   ARITHMETIC  :  the 8 routed + 1 shared experts SELECTED for this one token
   TRAFFIC     :  the weights actually FETCHED, for the whole batch, given the serving strategy
```

```text
   two tokens select the SAME expert  ──▶  ONE weight-fetch can serve both   (reuse)
   two tokens select DIFFERENT experts ──▶  TWO weight-fetches, no reuse available

   Same arithmetic (8 experts each). Different traffic (1× vs 2× the fetch).
```

这就是为什么 "18B active" 回答的是一个算术问题（每 token 的 FLOPs），而对访存流量几乎什么都没说，而在带宽受限的 GPU 上，恰恰是访存流量决定 decode（逐 token 生成阶段）的吞吐（通用论证见 [Hardware-Aware LLM Quantization — Module 01](../03-硬件感知LLM量化/Lecture-01.md) for the general argument）。在批大小为 1 时，如果每个在途 token 都选出不同的 8 个专家，流量就随**计算量**伸缩，而不只随 18B active 这个数字伸缩 —— 而在更大的批大小下，路由冲突率会成为一阶的推理服务变量，而 "18B active" 对此只字未提。

---

## 2. router 不是“softmax, then top-8”

参考计算，精确地写：

```text
   r    =  W_r · x                            router logits, computed in FP32
   s_e  =  sigmoid(r_e)                       per-expert score

   E(x) =  TopK_8( s_e + b_e )                SELECTION uses score + correction bias

   w_e  =  2.5 · s_e / Σ_{j in E(x)} s_j       WEIGHTING uses the raw sigmoid score,
                                                renormalized over the selected set only
                                                (2.5 is the configured routing scale)
```

仔细读，因为其中包含一个很容易实现错的区别：

```text
   b_e (correction bias)  ──▶  used ONLY to decide WHICH experts get selected
                                NEVER appears in the final mixture weight

   s_e (sigmoid score)    ──▶  used to decide selection (added to b_e)
                                AND reused, on its own, to decide the WEIGHT
```

```text
   ┌──────────────────────────────────────────────────────────────────┐
   │  A kernel that selects the correct 8 experts but then reuses      │
   │  (s_e + b_e) instead of s_e alone when computing w_e will select   │
   │  the RIGHT experts and MIX them with the WRONG weights.            │
   │                                                                    │
   │  The output will look fluent. It will not be this model.          │
   └──────────────────────────────────────────────────────────────────┘
```

这正是 [Module 11](Lecture-11.md) 的正确性矩阵要捕捉的那类 bug，现在就该把它内化：**文本看起来合理，并不能证明 kernel 正确。** router 实现的测试方式，应当是分别把选中的专家 ID *和*混合权重与参考实现对比 —— 绝不能靠读生成的文本、觉得它“看起来对”来判断。

---


<details>
<summary>English original</summary>

**Module 02 — MoE: Capacity, Work, and Traffic**

**Collection:** [GLM-5.3-Flash Architecture Mastery](README.md) | **Previous:** [← Module 01](Lecture-01.md) | **Next:** [Module 03 →](Lecture-03.md)

---

"18B active parameters" is the number everyone repeats and the number that misleads the most engineers into underestimating what this model costs to serve. This module separates three quantities that MoE collapses into one popular sentence, derives the router's actual arithmetic (which is not "softmax, then top-8"), and computes exactly how the checkpoint's size survives sparse activation.

---

**Learning objectives**

By the end of this module you should be able to:

1. Distinguish MoE **capacity**, **arithmetic**, and **memory traffic**, and give an example where two of the three move independently.
2. Write the router's actual computation — sigmoid scoring, additive correction bias for *selection*, renormalized raw scores for *weighting* — and explain why those are two different uses of two different quantities.
3. State the clamped-SwiGLU expert function and identify its asymmetry.
4. Derive the per-expert and total routed-expert parameter counts from the checkpoint's dimensions.
5. Explain why a correctly-selecting, incorrectly-weighting kernel produces plausible but wrong output.

---

**1. Three quantities MoE separates**

For an input `x` to a feed-forward sublayer, the output is:

```text
   y  =  f_shared(x)  +  Σ_{e in E(x)}  w_e(x) · f_e(x)
```

where `E(x)` is the set of 8 routed experts selected for this token, and the shared expert `f_shared` runs unconditionally, every token. This single equation hides three quantities that behave completely differently:

```text
   CAPACITY    :  all 288 routed + 1 shared expert weights that EXIST in the checkpoint
   ARITHMETIC  :  the 8 routed + 1 shared experts SELECTED for this one token
   TRAFFIC     :  the weights actually FETCHED, for the whole batch, given the serving strategy
```

```text
   two tokens select the SAME expert  ──▶  ONE weight-fetch can serve both   (reuse)
   two tokens select DIFFERENT experts ──▶  TWO weight-fetches, no reuse available

   Same arithmetic (8 experts each). Different traffic (1× vs 2× the fetch).
```

This is why "18B active" answers an arithmetic question (FLOPs per token) and answers almost nothing about memory traffic, which is what governs decode throughput on a bandwidth-bound GPU (see [Hardware-Aware LLM Quantization — Module 01](../03-硬件感知LLM量化/Lecture-01.md) for the general argument). At batch size 1, if every token in flight selects a different set of 8 experts, traffic scales with **arithmetic**, not with the 18B active figure alone — and at larger batch sizes, routing collision rate becomes a first-order serving variable that "18B active" says nothing about.

---

**2. The router is not "softmax, then top-8"**

The reference computation, precisely:

```text
   r    =  W_r · x                            router logits, computed in FP32
   s_e  =  sigmoid(r_e)                       per-expert score

   E(x) =  TopK_8( s_e + b_e )                SELECTION uses score + correction bias

   w_e  =  2.5 · s_e / Σ_{j in E(x)} s_j       WEIGHTING uses the raw sigmoid score,
                                                renormalized over the selected set only
                                                (2.5 is the configured routing scale)
```

Read that carefully, because it contains a distinction that is easy to implement wrong:

```text
   b_e (correction bias)  ──▶  used ONLY to decide WHICH experts get selected
                                NEVER appears in the final mixture weight

   s_e (sigmoid score)    ──▶  used to decide selection (added to b_e)
                                AND reused, on its own, to decide the WEIGHT
```

```text
   ┌──────────────────────────────────────────────────────────────────┐
   │  A kernel that selects the correct 8 experts but then reuses      │
   │  (s_e + b_e) instead of s_e alone when computing w_e will select   │
   │  the RIGHT experts and MIX them with the WRONG weights.            │
   │                                                                    │
   │  The output will look fluent. It will not be this model.          │
   └──────────────────────────────────────────────────────────────────┘
```

This is exactly the kind of bug [Module 11](Lecture-11.md)'s correctness matrix exists to catch, and it is worth internalizing now: **plausible text is not evidence of a correct kernel.** A router implementation should be tested by comparing selected expert IDs *and* mixture weights independently against a reference — never by reading the generated text and deciding it "looks right."

---

</details>

## 3. 专家函数：带 clamp 的 SwiGLU

每个专家——无论是路由专家还是共享专家——都计算：

```text
   a  =  min( W_g · x , 10 )                  gate projection, clamped ABOVE only
   u  =  clip( W_u · x , −10, 10 )             up projection, clamped BOTH sides

   f_e(x)  =  W_d · [ SiLU(a) ⊙ u ]
```

这种不对称，正是通用 SwiGLU 实现会漏掉的细节：

```text
   gate (a)  :  upper-clamped only     min(a, 10)
   up   (u)  :  clamped both sides     clip(u, −10, 10)
```

一个即插即用的「标准 SwiGLU」替换实现——通常两个投影都不做 clamp，或对称地对两个投影都做 clamp——会在激活值较小的几乎所有位置与该专家的行为一致，却恰好在大激活值尾部悄然偏离：这类输入被 [Hardware-Aware LLM Quantization — Module 06](../03-硬件感知LLM量化/Lecture-06.md) 称为 **massive activations**。它们罕见、承重，而且正是 clamp 不一致会最先暴露出来的那类 token——这意味着短时间的评估运行很可能完全漏掉这个 bug，而长时间的评估运行则不会。

---

## 4. 推导检查点的实际大小

router 为每个 token 从 288 个专家中挑 8 个，但模型要服务任意 token，**全部 288 个都必须常驻**。直接从检查点维度算出这份容量所需的开销。

**单个专家的参数量。** 专家的中间宽度为 2,048，hidden 宽度为 4,096。三个矩阵——gate、up、down——每个均为 `4096 × 2048`：

```text
   P_expert  =  3 × 4096 × 2048  =  25,165,824  parameters
```

**路由专家总计。** 42 个 MoE layer × 每 layer 288 个路由专家：

```text
   P_routed  =  42 × 288 × 25,165,824
             =  12,096 experts  ×  25,165,824
             ≈  304.4 × 10⁹  parameters
```

```text
   ┌────────────────────────────────────────────────────────────────────┐
   │  The routed-expert matrices ALONE account for ~304.4B of the       │
   │  ~320B total. The "18B active" figure describes roughly 9 of        │
   │  those 12,096 experts' worth of arithmetic per token (8 routed      │
   │  + 1 shared, per MoE layer) — not a reduction in what must be       │
   │  stored, dispatched, or communicated to serve the model at all.    │
   └────────────────────────────────────────────────────────────────────┘
```

再加上共享专家（每个 MoE layer 1 个，始终激活，形状与路由专家相同）：

```text
   P_shared  =  42 × 25,165,824  ≈  1.06 × 10⁹  parameters

   P_routed + P_shared  ≈  305.5 × 10⁹  parameters
```

这样大约剩下 `320B − 305.5B ≈ 14.5B` 留给模型中其余部分：全部 45 个 layer 的 attention 机制（KDA 与 MLA/DSA）、3 个 dense FFN layer、embedding 以及 LM head。**请自行对照实际检查点核验这一划分**，而不要把上面的算术当作穷尽——它是由两个最大、规格最清晰的组件构造出的下界，并非完整的参数审计。

### 具体来说，这对推理服务意味着什么

```text
   VRAM required to serve ANY request   ≈  ALL 288 experts/layer resident
                                             (roughly the full 320B-parameter footprint)

   VRAM required to serve ONE token's compute  ≈  ~18B-parameters' worth of arithmetic

   These are different budgets. Only the FIRST one determines whether the
   model fits on your hardware at all. [Module 09](Lecture-09.md) builds the
   full per-GPU budget from this starting point.
```

---

## 5. 容量、算术量与访存流量——一组具体对照

把 §1 中的三个量并排放在同一个具体场景里：一个含 32 个 decode（逐 token 生成阶段）请求的批，单个 MoE layer，假设没有布线冲突。

| 量 | 值 | 决定什么 |
|---|---:|---|
| **容量** | 288 × 25.17M ≈ 7.25 B 参数常驻 | layer 到底装不装得进 VRAM |
| **算术量** | 32 个请求 × 9 个专家 × 25.17M ≈ 相当于 7.25 B 参数的 FLOPs | 若算力受限，则决定计算时间 |
| **访存流量（最坏情况，无复用）** | 最多 32 × 9 = 288 次不同的专家抓取 → 整个 layer 的容量被取一遍 | 若带宽受限，则决定带宽（低 batch 下的常见情形——见 [Quantization Module 01](../03-硬件感知LLM量化/Lecture-01.md)） |
| **访存流量（最好情况，完全复用）** | 9 个不同专家各取一次，在全部 32 个请求间复用 | 带宽比最坏情况少 32×，算术量相同 |

**最后两行的算术量相同。访存流量相差 32×。** 这正是 MoE 推理服务系统极度看重布线局部性、专家并行布局和批组成的原因——而这些都不是「18B active」或朴素的 FLOP 计数能体现出来的。这也是专家并行推理服务系统把 *expert popularity skew* 作为一等指标跟踪的原因：恰好集中在少数热门专家上的批，表现如同最好情况那一行；而布线均匀分散的批，则表现如同最坏情况——在完全相同的硬件、相同的模型、相同的 token 数下。

---


<details>
<summary>English original</summary>

**3. The expert function: clamped SwiGLU**

Each expert — routed or shared — computes:

```text
   a  =  min( W_g · x , 10 )                  gate projection, clamped ABOVE only
   u  =  clip( W_u · x , −10, 10 )             up projection, clamped BOTH sides

   f_e(x)  =  W_d · [ SiLU(a) ⊙ u ]
```

The asymmetry is the detail a generic SwiGLU implementation misses:

```text
   gate (a)  :  upper-clamped only     min(a, 10)
   up   (u)  :  clamped both sides     clip(u, −10, 10)
```

A drop-in "standard SwiGLU" replacement — which typically clamps neither, or clamps both projections symmetrically — will match this expert's behavior almost everywhere activations are small, and silently diverge exactly on the large-activation tail: the inputs [Hardware-Aware LLM Quantization — Module 06](../03-硬件感知LLM量化/Lecture-06.md) calls **massive activations**. Those are rare, load-bearing, and precisely the tokens where a clamping mismatch would first show up — which means a short evaluation run can easily miss this bug entirely and a long one will not.

---

**4. Deriving the checkpoint's actual size**

The router picks 8 of 288 experts per token, but **all 288 must be resident** for the model to serve arbitrary tokens. Compute the cost of that capacity directly from the checkpoint dimensions.

**Per-expert parameter count.** Expert intermediate width is 2,048; hidden width is 4,096. Three matrices — gate, up, down — each `4096 × 2048`:

```text
   P_expert  =  3 × 4096 × 2048  =  25,165,824  parameters
```

**Routed-expert total.** Across 42 MoE layers × 288 routed experts per layer:

```text
   P_routed  =  42 × 288 × 25,165,824
             =  12,096 experts  ×  25,165,824
             ≈  304.4 × 10⁹  parameters
```

```text
   ┌────────────────────────────────────────────────────────────────────┐
   │  The routed-expert matrices ALONE account for ~304.4B of the       │
   │  ~320B total. The "18B active" figure describes roughly 9 of        │
   │  those 12,096 experts' worth of arithmetic per token (8 routed      │
   │  + 1 shared, per MoE layer) — not a reduction in what must be       │
   │  stored, dispatched, or communicated to serve the model at all.    │
   └────────────────────────────────────────────────────────────────────┘
```

Add the shared experts (1 per MoE layer, always active, same shape as a routed expert):

```text
   P_shared  =  42 × 25,165,824  ≈  1.06 × 10⁹  parameters

   P_routed + P_shared  ≈  305.5 × 10⁹  parameters
```

That leaves roughly `320B − 305.5B ≈ 14.5B` for everything else in the model: all 45 layers' attention mechanisms (KDA and MLA/DSA), the 3 dense FFN layers, embeddings, and the LM head. **Sanity-check this split for yourself against the actual checkpoint** rather than trusting the arithmetic above as exhaustive — it is a lower bound built from the two largest, most cleanly-specified components, not a full parameter audit.

**Why this matters for serving, concretely**

```text
   VRAM required to serve ANY request   ≈  ALL 288 experts/layer resident
                                             (roughly the full 320B-parameter footprint)

   VRAM required to serve ONE token's compute  ≈  ~18B-parameters' worth of arithmetic

   These are different budgets. Only the FIRST one determines whether the
   model fits on your hardware at all. [Module 09](Lecture-09.md) builds the
   full per-GPU budget from this starting point.
```

---

**5. Capacity, arithmetic, and traffic — worked contrast**

Put the three quantities from §1 next to each other for one concrete scenario: a batch of 32 decode requests, one MoE layer, no routing collisions assumed.

| Quantity | Value | What it governs |
|---|---:|---|
| **Capacity** | 288 × 25.17M ≈ 7.25 B params resident | whether the layer fits in VRAM at all |
| **Arithmetic** | 32 requests × 9 experts × 25.17M ≈ 7.25 B params' worth of FLOPs | compute time, if compute-bound |
| **Traffic (worst case, no reuse)** | up to 32 × 9 = 288 distinct expert fetches → the entire layer's capacity, fetched once | bandwidth, if bandwidth-bound (the common case at low batch — see [Quantization Module 01](../03-硬件感知LLM量化/Lecture-01.md)) |
| **Traffic (best case, full reuse)** | 9 distinct experts fetched once, reused across all 32 requests | 32× less bandwidth than the worst case, same arithmetic |

**Arithmetic is identical in the last two rows. Traffic differs by 32×.** This is why MoE serving systems care intensely about routing locality, expert-parallel placement, and batch composition — none of which "18B active" or a naive FLOP count will surface. It is also the reason expert-parallel serving systems track *expert popularity skew* as a first-class metric: a batch that happens to concentrate on a few popular experts behaves like the best-case row; a batch with uniformly scattered routing behaves like the worst case, on the exact same hardware, same model, same token count.

---

</details>

## 检查点

现在应该能够：

1. 举一个例子，说明同一个请求下 capacity、arithmetic 与 traffic 三者取值各不相同。
2. 把 router 的选择规则与加权规则写成两个独立的表达式，并指出哪一个用到了 correction bias。
3. 陈述 clamped-SwiGLU 的非对称性，并预测哪些激活值会暴露实现不匹配。
4. 凭记忆从检查点所声明的维度推导出 `P_expert = 25,165,824` 与 `P_routed ≈ 304.4B`。
5. 解释为什么路由碰撞率是一个与推理服务相关的指标，仅靠参数数量无法预测。

---

## 交付

这是 **[capstone 阶梯](Lecture-12.md) 的第 6 阶段**的一半（与 [模块 07](Lecture-07.md) 的残差流测试配对）。针对参考实现产出一份 **router/专家审计**：（1）一个分别比较所选专家 ID 与混合权重的测试，其输入经过构造，使得一旦 `s_e + b_e` 与 `s_e` 被混淆，仅凭它们就会做出不同的选择或加权；（2）一个覆盖 gate 与 up 两个投影上 clamp 边界的测试；（3）§4 中的参数推导，依据实际检查点的张量形状复现，而不是仅依赖配置所声明的维度。

---

## 截至当前版本

* **不随时间变化：** capacity/arithmetic/traffic 的区分，以及此 router 所遵循的 DeepSeek 风格稀疏 MoE（混合专家模型）选择/加权通用模式。
* **特定于检查点：** 路由 scale（2.5）、clamp 边界（±10，以及 gate 仅上界的非对称 clamp）、专家中间宽度（2,048）、专家数量（288 个路由 + 1 个共享）以及 top-k（8）都是此检查点配置的属性 —— 在把这些常量复用到其他修订版之前，先对照实际 config 核实。

---

**下一节：** [模块 03 — KDA I：Delta 规则递推 →](Lecture-03.md)


<details>
<summary>English original</summary>

**Checkpoint**

You should now be able to:

1. Give an example where capacity, arithmetic, and traffic each take a different value for the same request.
2. Write the router's selection rule and weighting rule as two separate expressions, and say which uses the correction bias.
3. State the clamped-SwiGLU asymmetry and predict which activations expose a mismatched implementation.
4. Derive `P_expert = 25,165,824` and `P_routed ≈ 304.4B` from the checkpoint's stated dimensions, from memory.
5. Explain why routing collision rate is a serving-relevant metric that parameter count alone cannot predict.

---

**Ship it**

This is half of **Stage 6 of the [capstone ladder](Lecture-12.md)** (paired with [Module 07](Lecture-07.md)'s residual-flow tests). Produce a **router/expert audit** against the reference implementation: (1) a test that compares selected expert IDs and mixture weights independently, using inputs constructed so that `s_e + b_e` and `s_e` alone would select or weight differently if confused; (2) a test that exercises the clamp boundaries on both the gate and up projections; (3) the parameter derivation from §4, reproduced against the actual checkpoint's tensor shapes rather than the config's stated dimensions alone.

---

**Current as of**

* **Timeless:** the capacity/arithmetic/traffic distinction, the general DeepSeek-style sparse-MoE selection/weighting pattern this router follows.
* **Checkpoint-specific:** the routing scale (2.5), clamp bounds (±10, and the gate's asymmetric upper-only clamp), expert intermediate width (2,048), expert count (288 routed + 1 shared), and top-k (8) are properties of this checkpoint's configuration — verify against the actual config before reusing these constants for a different revision.

---

**Next:** [Module 03 — KDA I: The Delta-Rule Recurrence →](Lecture-03.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/GLM-5.3-Flash Architecture Mastery/Lecture-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/GLM-5.3-Flash%20Architecture%20Mastery/Lecture-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
