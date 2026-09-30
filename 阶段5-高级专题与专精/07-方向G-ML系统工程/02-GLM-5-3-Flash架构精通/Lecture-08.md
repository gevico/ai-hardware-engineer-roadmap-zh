---
title: 模块 08 — 视觉、MTP 与混合推理服务状态
description: 模块 08 — 视觉、MTP 与混合推理服务状态
---

# 模块 08 — 视觉、MTP 与混合推理服务状态

**合集：** [GLM-5.3-Flash 架构精讲](README.md) | **上一节：** [← 模块 07](Lecture-07.md) | **下一节：** [模块 09 →](Lecture-09.md)

---

系统还有两个部分容易被低估，恰恰因为它们看起来只是在图上多加了一小块：前端挂一个视觉编码器，后端挂一个辅助预测头。两者描述起来都很简单，但各自引入了一项推理服务正确性上的义务，而仅文本、非推测式的思维模型并不会让你对这项义务有所准备。

---

## 学习目标

学完本模块，你应当能够：

1. 解释为什么视觉与语言处理必须作为两个独立阶段来计时，并预测不这样做会导致的一个具体性能剖析错误。
2. 说明单个 MTP 辅助层能确立什么、不能确立什么关于推理服务加速的结论。
3. 针对这一特定混合架构，枚举推测性回滚必须恢复的每一项每请求状态，并解释为什么“减小 KV 长度”是不够的。

---

## 1. 视觉是一个独立子系统

GLM-5.3-Flash 是多模态的：图像 patch 被编码，经过一个视觉 Transformer 处理，再合并/投影到语言模型的 hidden 宽度，之后汇入 [模块 01 §3](Lecture-01.md) 中描述的四条残差流。关键的运维事实是：它在架构上与语言解码器**相互独立** —— 一套不同的权重，一种不同的计算形态（是 patch，不是 token），在语言模型自身的前向传播基于所得特征开始之前运行。

```text
   image ──▶ patchify ──▶ vision transformer ──▶ project to d_model=4096 ──▶ joins
                                                                              the four
   text  ─────────────────────────────────────────────────────────────────▶ residual
                                                                              streams
```

### 性能剖析上的后果

如果把“这个请求花了多久”压缩成一个单一数字，那么视觉密集的请求与纯文本请求就不可比较了，而且一项真正有效的文本解码器优化可能看起来是失败的：

```text
   request A:  text-only, 500 tokens                    total = T_prefill + T_decode
   request B:  one image + 500 tokens of text            total = T_preprocess + T_vision + T_prefill + T_decode

   You ship a 30% faster KDA kernel. It improves T_decode specifically.

   Measuring only TOTAL time:
     request A: total drops noticeably     (T_decode was a large share of the total)
     request B: total barely moves          (T_preprocess + T_vision dominate; T_decode
                                              was always a small slice of this request's time)

   Conclusion a careless benchmark draws: "the KDA optimization doesn't help
   multimodal requests." WRONG — it helped exactly as much as it should have;
   the request's bottleneck was simply somewhere else.
```

```text
   ┌────────────────────────────────────────────────────────────────────┐
   │   Keep at least THREE timings distinct in any profiling setup      │
   │   that touches multimodal requests:                                 │
   │                                                                      │
   │        image preprocessing   │   vision encoding   │   LM prefill    │
   │                                                                      │
   │   Collapsing these into one number makes it impossible to tell      │
   │   whether an optimization worked or was simply irrelevant to        │
   │   this request's actual bottleneck.                                 │
   └────────────────────────────────────────────────────────────────────┘
```

这与 [模块 10](Lecture-10.md) 应用于模型其他每个区域的纪律相同 —— 绝不要用一个混入了不同瓶颈阶段的聚合数字来优化、或评判一项优化。

---


<details>
<summary>English original</summary>

**Module 08 — Vision, MTP, and Hybrid Serving State**

**Collection:** [GLM-5.3-Flash Architecture Mastery](README.md) | **Previous:** [← Module 07](Lecture-07.md) | **Next:** [Module 09 →](Lecture-09.md)

---

Two remaining pieces of the system are easy to under-scope precisely because they look like small additions to a diagram: a vision encoder bolted onto the front, and one auxiliary prediction head bolted onto the back. Both are simple to describe and each introduces a serving-correctness obligation that a text-only, non-speculative mental model does not prepare you for.

---

**Learning objectives**

By the end of this module you should be able to:

1. Explain why vision and language processing must be timed as separate stages, and predict a specific profiling error that results from not doing so.
2. State what a single MTP auxiliary layer does and does not establish about serving speedup.
3. Enumerate every piece of per-request state a speculative rollback must restore for this specific hybrid architecture, and explain why "decrease the KV length" is insufficient.

---

**1. Vision is a separate subsystem**

GLM-5.3-Flash is multimodal: image patches are encoded, processed through a vision transformer, and merged/projected into the language model's hidden width before joining the four residual streams described in [Module 01 §3](Lecture-01.md). The important operational fact is that this is architecturally **separate** from the language decoder — a different set of weights, a different computational shape (patches, not tokens), running before the language model's own forward pass begins on the resulting features.

```text
   image ──▶ patchify ──▶ vision transformer ──▶ project to d_model=4096 ──▶ joins
                                                                              the four
   text  ─────────────────────────────────────────────────────────────────▶ residual
                                                                              streams
```

**The profiling consequence**

If you collapse "how long did this request take" into a single number, a vision-heavy request and a text-only request become incomparable, and a genuinely effective text-decoder optimization can appear to fail:

```text
   request A:  text-only, 500 tokens                    total = T_prefill + T_decode
   request B:  one image + 500 tokens of text            total = T_preprocess + T_vision + T_prefill + T_decode

   You ship a 30% faster KDA kernel. It improves T_decode specifically.

   Measuring only TOTAL time:
     request A: total drops noticeably     (T_decode was a large share of the total)
     request B: total barely moves          (T_preprocess + T_vision dominate; T_decode
                                              was always a small slice of this request's time)

   Conclusion a careless benchmark draws: "the KDA optimization doesn't help
   multimodal requests." WRONG — it helped exactly as much as it should have;
   the request's bottleneck was simply somewhere else.
```

```text
   ┌────────────────────────────────────────────────────────────────────┐
   │   Keep at least THREE timings distinct in any profiling setup      │
   │   that touches multimodal requests:                                 │
   │                                                                      │
   │        image preprocessing   │   vision encoding   │   LM prefill    │
   │                                                                      │
   │   Collapsing these into one number makes it impossible to tell      │
   │   whether an optimization worked or was simply irrelevant to        │
   │   this request's actual bottleneck.                                 │
   └────────────────────────────────────────────────────────────────────┘
```

This is the same discipline [Module 10](Lecture-10.md) applies to every other region of the model — never optimize, or judge an optimization, against an aggregate number that mixes stages with different bottlenecks.

---

</details>

## 2. 单个 MTP layer 本身并不能带来推理服务加速

该检查点配置了**一个下一 token 预测辅助 layer**——一个与主模型一同训练的多 token 预测（MTP）head。仅凭这一点，只能说明*训练 recipe*包含了这一辅助目标。它本身**并不**能证明推理服务更快，因为投机式推理服务需要若干额外组件协同工作，而这些组件不会仅因为检查点带有一个 draft head 就自动具备：

```text
   a checkpoint HAS an MTP head          ⇏      serving IS faster

   speculative serving additionally needs:

     1.  a PROPOSAL mechanism            (the MTP head generates draft continuations)
     2.  TARGET VERIFICATION              (the full model checks the drafts)
     3.  an ACCEPTANCE RULE                (which drafted tokens get kept)
     4.  CORRECT STATE HANDLING            (§3 — the hard part for THIS architecture)
```

而且即便这四项全部正确实现，收益也不是“提议了多少 token”的函数——它是**相对于 draft 与 verify 成本的已接受进展**的函数，正是 [Hardware-Aware LLM Quantization — Module 10](../03-硬件感知LLM量化/Lecture-10.md) 中完整推导过的 accept rate 与 overhead 经济学。一个提议激进却经常被拒绝的 drafter 会让推理服务*更慢*，而不是更快——检查点里是否存在 MTP head，并不能说明某个具体部署会落在那条分界线的哪一侧。

---


<details>
<summary>English original</summary>

**2. One MTP layer is not a serving speedup by itself**

The checkpoint configures **one next-token-prediction auxiliary layer** — a multi-token-prediction (MTP) head trained alongside the main model. That fact, on its own, establishes only that the *training recipe* included this auxiliary objective. It does **not** by itself establish that serving is faster, because speculative serving needs several additional pieces working together, none of which come for free just because a checkpoint has a draft head:

```text
   a checkpoint HAS an MTP head          ⇏      serving IS faster

   speculative serving additionally needs:

     1.  a PROPOSAL mechanism            (the MTP head generates draft continuations)
     2.  TARGET VERIFICATION              (the full model checks the drafts)
     3.  an ACCEPTANCE RULE                (which drafted tokens get kept)
     4.  CORRECT STATE HANDLING            (§3 — the hard part for THIS architecture)
```

And even with all four correctly implemented, the benefit is not a function of "how many tokens get proposed" — it is a function of **accepted progress relative to the cost of drafting and verifying**, exactly the accept-rate-versus-overhead economics worked out in full in [Hardware-Aware LLM Quantization — Module 10](../03-硬件感知LLM量化/Lecture-10.md). A drafter that proposes aggressively but gets rejected often can make serving *slower*, not faster — the presence of an MTP head in the checkpoint says nothing about which side of that line a given deployment lands on.

---

</details>

## 3. 回滚：该架构实际携带的状态

这是本模块的核心要点，它直接承接本课程中已经完成的工作。常规 Transformer 的推测解码回滚相对简单：在某个 draft token 被拒绝时，把 KV cache 截断回最后一个被接受的位置，然后继续。**该操作对本架构而言并不充分**，因为本模型还携带若干其他种类的 per-request 状态，而 KV 长度截断根本触及不到它们。

回想一下，在混合栈中，request 作用域下实际累积的是什么：

```text
   MECHANISM              STATE THAT MUST BE RECONCILED ON ROLLBACK
   ──────────────────      ────────────────────────────────────────────────────
   KDA (Modules 03–04)    the recurrent state matrix S_t, per head, per KDA layer —
                          NOT indexed by token position at all. You cannot "truncate"
                          it the way you truncate a list; a rejected token means the
                          state update it caused must be UNDONE or RECOMPUTED from a
                          checkpoint taken before that update.

   short causal            convolution state (Module 04 §3) — a small sliding window
   convolution              of recent inputs feeding the convolution. Also not simply
                             "shorter" after rollback; it must reflect exactly the
                             accepted prefix, no more.

   MLA (Module 05)         the token-indexed latent cache c_t — THIS one genuinely is
                          truncatable by position, much like an ordinary KV cache.

   DSA/KPool (Module 06)  index/selection metadata built over the (now-rolled-back)
                          history — must be rebuilt or truncated consistently with
                          the new, shorter accepted prefix, respecting the same
                          pool-completion causality rule from Module 06 §3.
```

```text
   ┌────────────────────────────────────────────────────────────────────┐
   │   "Rollback" for this model means: restore or recompute the KDA     │
   │   recurrent state AND the convolution state to the point matching   │
   │   the last accepted token, AND truncate the MLA latent cache to      │
   │   that same point, AND reconcile the DSA indexer's selection         │
   │   metadata against the new prefix length.                            │
   │                                                                        │
   │   Treating rollback as "decrease the KV length" — correct for a       │
   │   conventional transformer — silently leaves the KDA and             │
   │   convolution state pointing at a history that includes REJECTED     │
   │   tokens. Every subsequent token generated from that corrupted        │
   │   state is wrong, and nothing about the symptom will look like an     │
   │   obvious crash — it will look like a model that's slightly, then     │
   │   increasingly, incoherent.                                           │
   └────────────────────────────────────────────────────────────────────┘
```

有两种实用设计可以弥合这一缺口，而真实系统通常需要两者某种程度的组合：

```text
   CHECKPOINT-AND-RESTORE:  snapshot S_t (and convolution state) before each
                            speculative verification step; on rejection, restore
                            the snapshot exactly. Costs memory proportional to
                            how many speculative steps you checkpoint.

   RECOMPUTE:               on rejection, recompute S_t from the last known-good
                            checkpoint forward through the accepted tokens only,
                            using the ordinary recurrent update (Module 03).
                            Costs compute instead of memory.
```

无论哪种方式，正确性标准都与 [Module 04 §4](Lecture-04.md) 已为分块执行与循环执行确立的标准相同：**回滚后的状态必须与一个从一开始就从未生成过被拒 token 的请求所产生的状态一致** —— 不仅仅是“接近”，也不能仅靠输出合理性来验证。

---

## Checkpoint

现在你应该能够：

1. 解释为什么一个视觉密集型请求和一个纯文本请求无法在一个聚合延迟数字下进行比较，并给出一个忽略这一点会得出错误结论的具体例子。
2. 列出多模态性能剖析设置必须分开保留的三种时序。
3. 说明推测推理服务在“检查点带有 MTP 头”之外所需的四个组成部分。
4. 针对这一特定架构，枚举一次正确回滚必须协调的每一种状态 —— 而不只是 KV/latent cache。
5. 解释为什么“减小 KV 长度”对常规 Transformer 是正确回滚策略，而在这里却不完整。

---


<details>
<summary>English original</summary>

**3. Rollback: the state this architecture actually carries**

This is the module's central point, and it follows directly from work already done in this course. A conventional transformer's speculative-decoding rollback is comparatively simple: on a rejected draft token, truncate the KV cache back to the last accepted position, and continue. **That operation is insufficient for this architecture**, because this model carries several other kinds of per-request state that a KV-length truncation does not touch at all.

Recall what's actually accumulating, request-scoped, across the hybrid stack:

```text
   MECHANISM              STATE THAT MUST BE RECONCILED ON ROLLBACK
   ──────────────────      ────────────────────────────────────────────────────
   KDA (Modules 03–04)    the recurrent state matrix S_t, per head, per KDA layer —
                          NOT indexed by token position at all. You cannot "truncate"
                          it the way you truncate a list; a rejected token means the
                          state update it caused must be UNDONE or RECOMPUTED from a
                          checkpoint taken before that update.

   short causal            convolution state (Module 04 §3) — a small sliding window
   convolution              of recent inputs feeding the convolution. Also not simply
                             "shorter" after rollback; it must reflect exactly the
                             accepted prefix, no more.

   MLA (Module 05)         the token-indexed latent cache c_t — THIS one genuinely is
                          truncatable by position, much like an ordinary KV cache.

   DSA/KPool (Module 06)  index/selection metadata built over the (now-rolled-back)
                          history — must be rebuilt or truncated consistently with
                          the new, shorter accepted prefix, respecting the same
                          pool-completion causality rule from Module 06 §3.
```

```text
   ┌────────────────────────────────────────────────────────────────────┐
   │   "Rollback" for this model means: restore or recompute the KDA     │
   │   recurrent state AND the convolution state to the point matching   │
   │   the last accepted token, AND truncate the MLA latent cache to      │
   │   that same point, AND reconcile the DSA indexer's selection         │
   │   metadata against the new prefix length.                            │
   │                                                                        │
   │   Treating rollback as "decrease the KV length" — correct for a       │
   │   conventional transformer — silently leaves the KDA and             │
   │   convolution state pointing at a history that includes REJECTED     │
   │   tokens. Every subsequent token generated from that corrupted        │
   │   state is wrong, and nothing about the symptom will look like an     │
   │   obvious crash — it will look like a model that's slightly, then     │
   │   increasingly, incoherent.                                           │
   └────────────────────────────────────────────────────────────────────┘
```

Two practical designs close this gap, and a real system typically needs some combination of both:

```text
   CHECKPOINT-AND-RESTORE:  snapshot S_t (and convolution state) before each
                            speculative verification step; on rejection, restore
                            the snapshot exactly. Costs memory proportional to
                            how many speculative steps you checkpoint.

   RECOMPUTE:               on rejection, recompute S_t from the last known-good
                            checkpoint forward through the accepted tokens only,
                            using the ordinary recurrent update (Module 03).
                            Costs compute instead of memory.
```

Either way, the correctness bar is the same one [Module 04 §4](Lecture-04.md) already established for chunked-versus-recurrent execution: **state after rollback must agree with state from a request that had simply never generated the rejected tokens in the first place** — not merely "close," and not verified by output plausibility alone.

---

**Checkpoint**

You should now be able to:

1. Explain why a vision-heavy and a text-only request are not comparable under one aggregate latency number, with a concrete example of the wrong conclusion that follows from ignoring this.
2. List the three timings a multimodal profiling setup must keep separate.
3. State the four components speculative serving needs beyond "the checkpoint has an MTP head."
4. Enumerate, for this specific architecture, every kind of state a correct rollback must reconcile — not just the KV/latent cache.
5. Explain why "decrease the KV length" is a correct rollback strategy for a conventional transformer but an incomplete one here.

---

</details>

## 交付

设计一个**回滚正确性测试**（并在可行时，针对一个小型测试用 harness（agent 运行时框架）实现）：开启投机解码生成一个序列，在选定位置强制触发一次拒绝，并验证回滚后的 KDA 状态、卷积状态与 MLA 缓存，是否与一次独立运行中直接只生成被接受前缀所得的对应状态逐项完全一致——即把 §3 中「从未生成过被拒绝的 token」这一不变量变成可执行的检验。

---

## 截至目前的适用范围

* **不随版本变化：** 多阶段多模态请求的性能剖析规范、一个可用的投机解码系统的一般要求，以及混合循环/token 索引架构在回滚下的状态对账论证。
* **与检查点相关：** 恰好存在一个 MTP 辅助层，以及特定的多模态编码器/merge 设计，都是该检查点的属性——在假定另一个修订版本具备同样的投机解码支持之前，先对照实际的推理服务（serving）实现进行核实。

---

**Next:** [Module 09 — The 8-GPU Memory Model →](Lecture-09.md)


<details>
<summary>English original</summary>

**Ship it**

Design (and, where feasible, implement against a small test harness) a **rollback correctness test**: generate a sequence with speculative decoding enabled, force a rejection at a chosen position, and verify that the post-rollback KDA state, convolution state, and MLA cache each exactly match the corresponding state from an independent run that generated only the accepted prefix directly — the "never generated the rejected tokens" invariant from §3, made executable.

---

**Current as of**

* **Timeless:** the profiling discipline for multi-stage multimodal requests, the general requirements for a working speculative-decoding system, and the state-reconciliation argument for hybrid recurrent/token-indexed architectures under rollback.
* **Checkpoint-specific:** the presence of exactly one MTP auxiliary layer, and the specific multimodal encoder/merge design, are properties of this checkpoint — verify against the actual serving implementation before assuming a different revision carries the same speculative-decoding support.

---

**Next:** [Module 09 — The 8-GPU Memory Model →](Lecture-09.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/GLM-5.3-Flash Architecture Mastery/Lecture-08.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/GLM-5.3-Flash%20Architecture%20Mastery/Lecture-08.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
