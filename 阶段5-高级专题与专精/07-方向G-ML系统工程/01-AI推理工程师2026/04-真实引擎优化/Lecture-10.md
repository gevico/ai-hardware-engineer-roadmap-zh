---
title: Part 4 · 第 10 讲 — Silently Wrong：推理引擎独有的失效模式
description: Part 4 · 第 10 讲 — Silently Wrong：推理引擎独有的失效模式
---

# Part 4 · 第 10 讲 — Silently Wrong：推理引擎独有的失效模式

## 概览

大多数软件的失效都很吵闹。它崩溃、抛异常、返回错误码，或者产出明显坏到没人会发布的结果。推理引擎没有这张安全网。

语言模型是一个把任意输入变成看似合理文本的函数。损坏一个权重切片、丢掉一个 expert、跳过一次 all-reduce，或者按错误的步长读取张量，模型都不会崩溃 —— **它仍在写流利的英文。** 只是稍微差一点的英文，来自一个与你加载的模型略有不同的模型，而光靠看是发现不了的。

本讲讨论的就是这种失效模式：为什么它是结构性的而非偶发的、它分为哪五个族、捕获每一族的断言，以及那个应当让你恐惧的特殊情形 —— **让引擎变快的 bug。**

读完本讲，你应当能面对一个待定的优化，说出它可能悄悄破坏什么，以及能以低成本捕获它的检查。正是这套纪律，让本部分其余每一讲都能被安全地应用。

---

## 1. 为什么安全网缺失

三个性质叠加在一起，而它们都是该工作负载固有的。

**输出空间里没有非法值。** 损坏的图像有可见的伪影；损坏的 JSON 解析会抛错。而损坏的 logit 向量*仍然是一个 logit 向量*。对它做 softmax、做 argmax，你就得到一个 token。每个 token 都是合法 token。不存在“这个分布是错的”这种表示。

**正确输出是未知的。** 对大多数软件，你可以写出 `assert result == expected`。而对一个 128k 上下文下的 2.8T 参数模型，期望的下一个 token 就是模型所说的那个。唯一的 ground truth 是同一模型的另一份实现 —— 这正是 [Lecture 02](Lecture-02.md) 把正确性定义为*在相同权重与相同 token id 下与参考实现一致*的原因，也是拿不到参考实现的引擎面临真正难题的原因。

**质量是平滑退化的，而人眼是件糟糕的仪器。** 一个 expert 被丢掉 6% 的模型仍然能写出像样的文章。人类读样本，分辨不出“top-1 一致率 1.00”和“top-1 一致率 0.93”。案例研究自己就提醒过：即便是目标量化相对全精度的 top-1 也只有 **90.4%** —— 所以在 bug 出没的那个尺度上，“看着没问题”毫无区分力。

再加上 [Lecture 02](Lecture-02.md) 里的激励 —— 你拿报酬换速度 —— 就凑成了这件事最糟的版本：**一个唯一可见症状是 benchmark 数字变好的 bug。**

---


<details>
<summary>English original</summary>

**Part 4 · Lecture 10 — Silently Wrong: The Failure Mode Unique to Inference Engines**

**Overview**

Most software fails loudly. It crashes, throws, returns an error code, or produces output so obviously broken that nobody ships it. An inference engine does not have that safety net.

A language model is a function that turns any input into plausible-looking text. Corrupt a weight slice, drop an expert, skip an all-reduce, or read a tensor at the wrong stride, and the model does not crash — **it keeps writing fluent English.** Slightly worse English, from a slightly different model than the one you loaded, and you will not notice by looking.

This lecture is about that failure mode: why it is structural rather than incidental, the five families it comes in, the assertions that catch each, and the special case that should terrify you — **bugs that make the engine faster.**

By the end you should be able to look at a proposed optimization and name what it could silently break, plus the cheap check that would catch it. This is the discipline that makes every other lecture in this part safe to apply.

---

**1. Why the safety net is missing**

Three properties combine, and all three are inherent to the workload.

**The output space has no invalid values.** A corrupted image has visible artifacts; a corrupted JSON parse throws. A corrupted logit vector is *a logit vector*. Softmax it, argmax it, and you get a token. Every token is a legal token. There is no representation for "this distribution is wrong."

**The correct output is not known.** For most software you can write `assert result == expected`. For a 2.8T-parameter model at 128k context, the expected next token is whatever the model says it is. The only ground truth is another implementation of the same model — which is why [Lecture 02](Lecture-02.md) defines correctness as *agreement with a reference on identical weights and identical token ids*, and why an engine that cannot get a reference has a genuinely hard problem.

**Quality degrades smoothly and the eye is a terrible instrument.** A model whose experts are 6% dropped still writes competent prose. Humans reading samples cannot distinguish "top-1 agreement 1.00" from "top-1 agreement 0.93." The case study's own reminder: even the target quantization's top-1 against full precision is **90.4%** — so "it looks fine" has no discriminating power at the scale where bugs live.

Add the incentive from [Lecture 02](Lecture-02.md) — you are being paid for speed — and you have the setup for the worst version of this: **a bug whose only visible symptom is a better benchmark number.**

---

</details>

## 2. 一个静默 bug 的解剖

这里完整给出一个，因为读一个真实的例子胜过一份分类法。它来自 [PR #148](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/148)，即 [Lecture 09](Lecture-09.md) 中的 batched-prefill 改动。

**签名与调用。**

```text
   void attn_res_mix_f32(n_rows, act_row_stride, bank_row_stride, ...)

   all three call sites passed:
        attn_res_mix_f32(n_rows, bank_row, H)
                                 ^^^^^^^^  ^
                                 the two strides, transposed
```

于是激活值按 *bank 的* pitch 读取，而 bank 按 *激活值的* pitch 读取。一次双参数转置 —— 系统编程中最普通的 bug。

**为什么整个项目周期内都没人抓到它。** 两层相互独立的遮蔽，叠加在一起：

```text
   MASK 1 — the depth mask
     bank_row = res_bank_row_elems × max_ckpt
     max_ckpt = ceil(n_layers / 12)

     at n_layers ≤ 12:   max_ckpt == 1   →   bank_row == H
                         the two strides are EQUAL, so each wrong
                         argument lands on the right value.
     bit-identical.  every short-model test passes.

   MASK 2 — the row mask
     n_rows == 1 never reads either stride.
     decode and the per-token prefill walk both pass n_rows == 1.
     → for the entire history of the engine, this code was exact.

   the chunk driver is the FIRST caller ever to pass more than one row.
```

所有既有测试都落在两个盲区的交集里。这个 bug 不是因疏忽而漏掉的；在新功能触达它之前，它一直**不可达**。

**它是怎么被找到的。** 按 layer 深度对着 4-token 精度一致性探针做二分：

```text
   layers ≤ 12   →  bit-identical
   layers = 14   →  KLD 1.64
   layers = 93   →  KLD 3.47
```

这是一个漂亮的诊断形态，值得记住：**一个在阈值之前恰好为零、之后却很大的指标，是结构性 bug，而不是数值漂移。** 精度损失是逐步累积的，且大致单调。`n_layers = 13` 处的断崖直指某个内部带 `12` 的东西 —— 而 `max_ckpt = ceil(n_layers / 12)` 只差一条 grep。

> **值得偷走的技术。** 当精度一致性指标不对时，不要死盯着 kernel。**扫描结构性参数** —— layer 数、head 数、chunk 宽度、tp_size、上下文深度 —— 找出指标在何处离开零。那个阈值就点出了 bug 的名字。

**同一个 PR 里的第二个 bug**，因为它们总是成对出现：

```c
/* the comment says DEFAULT OFF; the code says otherwise */
bool k3_kda_qkvg_batch_enabled(void) {
    const char *e = getenv("SPARKINFER_K3_KDA_QKVG_BATCH");
    return !(e && e[0] == '0');       /* unset -> TRUE. opt-OUT. */
}

/* eight lines below, the sibling gets it right */
bool k3_kda_pre_batch_enabled(void) {
    const char *e = getenv("SPARKINFER_K3_KDA_PRE_BATCH");
    return e && e[0] == '1';          /* unset -> FALSE. opt-IN. */
}
```

这个特性在它自己的注释里*以及*消费它的函数里都被记为 `DEFAULT OFF`，而且是开启状态。这一个至少在被触达时会明确失败 —— 每个 ≥2 token 的 chunk 都以 `LAUNCH FAILED at layer 0, phase Attn` 挂掉。

> **偷走这一条。** 环境变量开关在整个代码库里应当只有唯一一个辅助函数，处处复用。相隔八行的两种写法，就是一个已经发生过的 bug；你只是在等结果落在哪一边。

---


<details>
<summary>English original</summary>

**2. Anatomy of a silent bug**

Here is one, in full, because reading a real one is worth more than a taxonomy. It is from [PR #148](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/148), the batched-prefill change from [Lecture 09](Lecture-09.md).

**The signature and the call.**

```text
   void attn_res_mix_f32(n_rows, act_row_stride, bank_row_stride, ...)

   all three call sites passed:
        attn_res_mix_f32(n_rows, bank_row, H)
                                 ^^^^^^^^  ^
                                 the two strides, transposed
```

So activations were read at the *bank's* pitch and banks at the *activation's* pitch. A two-argument transposition — the most ordinary bug in systems programming.

**Why nothing caught it for the life of the project.** Two independent masks, stacked:

```text
   MASK 1 — the depth mask
     bank_row = res_bank_row_elems × max_ckpt
     max_ckpt = ceil(n_layers / 12)

     at n_layers ≤ 12:   max_ckpt == 1   →   bank_row == H
                         the two strides are EQUAL, so each wrong
                         argument lands on the right value.
     bit-identical.  every short-model test passes.

   MASK 2 — the row mask
     n_rows == 1 never reads either stride.
     decode and the per-token prefill walk both pass n_rows == 1.
     → for the entire history of the engine, this code was exact.

   the chunk driver is the FIRST caller ever to pass more than one row.
```

Every existing test was in the intersection of two blind spots. The bug was not missed through carelessness; it was **unreachable** until a new feature reached it.

**How it was found.** By bisecting on layer depth against the 4-token parity probe:

```text
   layers ≤ 12   →  bit-identical
   layers = 14   →  KLD 1.64
   layers = 93   →  KLD 3.47
```

That is a beautiful diagnostic shape, and worth recognizing: **a metric that is exactly zero up to a threshold and then large is a structural bug, not numerical drift.** Precision loss accumulates gradually and roughly monotonically. A cliff at `n_layers = 13` points straight at something with a `12` in it — and `max_ckpt = ceil(n_layers / 12)` was one grep away.

> **The technique to steal.** When a parity metric is bad, do not stare at the kernel. **Sweep the structural parameters** — layers, heads, chunk width, tp_size, context depth — and find where the metric leaves zero. The threshold names the bug.

**The second bug in the same PR**, because they travel in pairs:

```c
/* the comment says DEFAULT OFF; the code says otherwise */
bool k3_kda_qkvg_batch_enabled(void) {
    const char *e = getenv("SPARKINFER_K3_KDA_QKVG_BATCH");
    return !(e && e[0] == '0');       /* unset -> TRUE. opt-OUT. */
}

/* eight lines below, the sibling gets it right */
bool k3_kda_pre_batch_enabled(void) {
    const char *e = getenv("SPARKINFER_K3_KDA_PRE_BATCH");
    return e && e[0] == '1';          /* unset -> FALSE. opt-IN. */
}
```

The feature was documented `DEFAULT OFF` in its own comment *and* in the function that consumed it, and was on. This one at least failed loudly once reached — every chunk of ≥2 tokens died with `LAUNCH FAILED at layer 0, phase Attn`.

> **Steal this.** Environment-variable gates should have exactly one helper in the codebase, used everywhere. Two idioms eight lines apart is a bug that has already happened; you are just waiting to find out which side it landed on.

---

</details>

## 3. 因为坏掉而更快

接下来是让这一讲成为*性能*课、而不只是测试课的部分。

[PR #148](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/148) 最初测得 prefill（首字前的整段计算）**169.72 tok/s**。这本来会是项目历史上最大的单项结果——4.2× 的跃升。它因准确率被否决。同一改动在上面的两个缺陷都修复之后，诚实的数字是 **98.80**。

原因在 changelog 里说得一清二楚，也正是整个这一部分要记住的那句话：

> *“损坏的遍历**更快**，因为它读的是错的行。”*

读错行意味着读的是你刚刚读过的行——是缓存命中而不是缓存未命中。在带宽受限的工作负载上，**错误与速度正相关**，因为大部分开销都花在从正确的位置取回正确的字节上。带宽受限的 kernel 里的损坏不是 runtime 的随机扰动；它在系统性地*有利*。

这颠覆了大多数工程师的直觉：

```text
   intuition:   a bug makes things slower or breaks them.
                a speedup is evidence the change worked.

   reality on a memory-bound engine:
                a bug that skips work, reads the wrong (nearer) bytes,
                drops an expert, or elides a collective  →  FASTER.

                the largest numbers in your sweep are the most
                likely to be wrong.
```

由此直接引出三条实践：

**在结构上，正确性门禁先于速度门禁。** 不是“我们两个都检查”——准确率结果必须能够*压制*速度数字，使它永远不进记录。[Lecture 02 §2](Lecture-02.md)。

**合理性上限。** [PR #130](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/130) 把超过参考值 5× 的声称判为不可信，而不是收入账。这看起来偏执，直到你亲眼见过一个损坏的 kernel 报出惊人的数字。上限并不断言什么是可达到的；它断言的是**好到不真实的结果会被检查，而不是被记录。**

**把意外的胜利当作 bug 报告，直到被证明不是。** 如果某个改动在你预测 1.5× 的地方给出了 4×，那么要么你的预测错了，*要么*你的代码错了。后者的先验概率比工程师们愿意承认的高得多。这就是 [Lecture 03 §7.1](Lecture-03.md) 的“写下你的预测”规则的实际价值：没有预测，你就没有什么可意外的。

---

## 4. 五个家族

案例研究中每一个静默错误 bug 都落在这五类之一。这个模式值得内化，因为*防御手段*是按家族区分的。

### 4.1 配置读错

模型文件说的是一回事；加载器相信的是另一回事。没有任何东西损坏——你跑的是和你以为的**不同的模型**。

| 陷阱 | 机制 | 症状 |
|---|---|---|
| `full_attn_layers` 是 **1-indexed** | converter 用 `(il + 1) in full_attn_layers` 做测试 | 本该是 MLA 的地方成了 KDA。文本流畅，模型错误。 |
| MLA 被**存成 MQA** | `head_count_kv = 1`、`key_length = kv_lora + qk_rope = 576`；每层的 `head_count_kv == 0` 标记一个 KDA 层 | 当成真正的 MQA 来读，你的分片算术就会用 1 除以 8 |
| 专家处于**降投影空间** | `expert_latent_length 3584`，而不是 `hidden_size 7168` | 每个专家 GEMM 都错 2× |
| **被静默取默认值的 KV key** | `expert_latent_length`、`attn_res.block_size`，两个 `situ` beta | “加载干净，输出垃圾” |

最后一类的防御手段是应对整个家族的最佳答案，它就在*参考*引擎里：锁定的 fork 把这四个 key 设为**必填，而非有默认值。** 缺失的 key 会变成加载失败，而不是一个错误的模型。

> **照搬这一点。** 对每一个模型超参数都问：*如果它缺失或错误，我会得到报错，还是一个更差的模型？* 每一个回答“更差的模型”的参数，都应该改成必填、不给默认值。默认值就是一次静默的猜测。

视觉塔还带有另外四个此类陷阱——非方形的 fused QKV（1536 ≠ `n_embd`）、RMSNorm、无 bias、post-norm projector——每一个在仓库里都被记录为*“静默错误输出的陷阱，而不是编译错误。”* 这个说法正是你在自己代码里标注这一类问题的正确方式。


<details>
<summary>English original</summary>

**3. Faster because broken**

Now the part that makes this a *performance* lecture and not just a testing one.

[PR #148](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/148) first measured **169.72 tok/s** prefill. That would have been the largest single result in the project's history — a 4.2× step. It was rejected on accuracy. The honest number for the same change, once both defects above were fixed, was **98.80**.

The reason is stated exactly in the changelog, and it is the sentence to remember from this entire part:

> *"The corrupted walk was **faster** because it was reading the wrong rows."*

Reading the wrong rows means reading rows you had recently read — a cache hit instead of a cache miss. On a memory-bound workload, **incorrectness and speed are positively correlated**, because most of the cost is fetching the right bytes from the right places. Corruption in a memory-bound kernel is not a random perturbation of runtime; it is systematically *favorable*.

This inverts the intuition most engineers carry:

```text
   intuition:   a bug makes things slower or breaks them.
                a speedup is evidence the change worked.

   reality on a memory-bound engine:
                a bug that skips work, reads the wrong (nearer) bytes,
                drops an expert, or elides a collective  →  FASTER.

                the largest numbers in your sweep are the most
                likely to be wrong.
```

Three practices follow directly:

**Correctness gates before speed gates, structurally.** Not "we check both" — the accuracy result must be able to *suppress* the speed number so it never enters the record. [Lecture 02 §2](Lecture-02.md).

**A plausibility ceiling.** [PR #130](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/130) rejects a claim above 5× the reference as implausible rather than banking it. This looks paranoid until you have watched a corrupted kernel post a spectacular number. The ceiling does not assert what is achievable; it asserts that **a result too good to be true gets checked, not recorded.**

**Treat a surprising win as a bug report until proven otherwise.** If a change delivers 4× where you predicted 1.5×, your prediction was wrong *or* your code is. The prior on the second is much higher than engineers like to admit. This is the practical value of [Lecture 03 §7.1](Lecture-03.md)'s write-down-your-prediction rule: without a prediction, you have nothing to be surprised by.

---

**4. The five families**

Every silent-wrongness bug in the case study falls into one of these. The pattern is worth internalizing because the *defenses* are family-specific.

**4.1 Configuration read wrong**

The model file says one thing; the loader believes another. Nothing is corrupt — you are running a **different model** than you think.

| Trap | Mechanism | Symptom |
|---|---|---|
| `full_attn_layers` is **1-indexed** | converter tests `(il + 1) in full_attn_layers` | KDA where MLA belongs. Fluent text, wrong model. |
| MLA is **stored as MQA** | `head_count_kv = 1`, `key_length = kv_lora + qk_rope = 576`; per-layer `head_count_kv == 0` marks a KDA layer | Read as real MQA and your shard math divides 1 by 8 |
| Experts are in a **down-projected space** | `expert_latent_length 3584`, not `hidden_size 7168` | Every expert GEMM wrong by 2× |
| **Silently defaulted KV keys** | `expert_latent_length`, `attn_res.block_size`, both `situ` betas | "Loads cleanly and emits garbage" |

The defense for the last one is the best available answer to this whole family, and it lives in the *reference* engine: the pinned fork makes those four keys **required, not defaulted.** A missing key becomes a load failure instead of a wrong model.

> **Steal this.** For every model hyperparameter, ask: *if this were absent or wrong, would I get an error or a worse model?* Every parameter that answers "a worse model" should be made mandatory, with no default. A default is a silent guess.

The vision tower carries four more of these — non-square fused QKV (1536 ≠ `n_embd`), RMSNorm, bias-free, post-norm projector — each documented in the repo as *"a silent-wrong-output trap rather than a compile error."* That phrase is the correct way to annotate this class in your own code.

</details>

### 4.2 划分算术

分片就是索引算术，而稍有差错的索引算术，产出的却是一个能跑的模型。

**band 必须恰好铺满。** 出现空隙会静默丢掉某个专家的贡献；出现重叠会把它重复计算。两种情况都让全规约合并出错，而输出看上去仍然合理。防御手段既穷尽又廉价：`test_expert_bands_tile_exactly` 遍历每个 rank、每个可行的 `tp_size`，并断言 **每个专家都恰好被拥有一次**——而加载检查用 **16,351 次检查** 证明 band 在真实张量上恰好铺满，分片张量的每个字节都恰好被一个 rank 认领。

**KV head 数常常不能整除。** K3 把 MLA 存为 MQA，KV head 数恰好是 **1**。在 `tp_size 8` 下，无法给每个 rank 分配一个完整的 KV head，而把一个 KV head 拆开是 *错误* 的——一个 KV group 必须对经由它做 attention 的每个 query head 可见。因此当 `n_kv_heads < tp_size` 时，KV 投影被**复制**，只有 query 一侧做分片。朴素的 `n_kv_heads / tp_size` 得到 **0**，用仓库里的话说，就产出 *"一个任何 rank 上都没有 key、却仍能加载、仍能吐字的模型。"*

**每个轴都必须能整除，而不只是那个引人注目的轴。** K3 有 896 个专家和 `896 % 7 == 0`，所以只看专家的检查会欣然接受 `tp_size 7`——但它还有 96 个 query head 和 `96 % 7 != 0`。`shard_dims()` 会拒绝整个 shape，并指出出错的字段。仓库里关于这一点的注释透着难得的人味：*"我自己测试的第一版就犯了这个错，是测试把它抓了出来。"*

这三者背后的战略决策才是值得照搬的那个：**分片数学不依赖 CUDA，且在没有 GPU 的情况下完成单元测试**——`shard.cpp` 上有 4,972 项检查，权重方案上有 230 项，后端选择上有 44 项。理由是：

> *"TP 的 bug 不在集合通信里——NCCL 是对的。它们在 band 算术里，而这恰恰是你在烧掉机时之前、可以在笔记本上验证的那部分。"*

### 4.3 集合通信的位置与次数

这一类只属于分布式推理，也是五类中最优美的一类，因为这里的 bug 是 *代数* 层面的。

**归约为什么放在它所在的位置。** 被路由的专家按专家分片，因此每个 rank 的 dispatch 累加器是 top-16 上的一个**部分和**。下一个算子是 `ffn_routed_norm`，一个 RMS norm——而：

```text
   rms_norm(Σ partial)  ≠  Σ rms_norm(partial)
```

RMS norm **不是线性**的，所以跨 rank 求和必须在它 *之前* 完成。把归约往后挪两个算子、放到 `routed_up` 之后，就会同时出两个问题：你跳过了专家所需要的那个归约，*并且*——因为 `routed_norm` / `routed_up` / `shexp` 都是复制的，所以每个 rank 都已经持有完整的张量——你对一个完整张量做归约，于是**把 FFN 输出乘上了 `tp_size`。**

把 FFN 输出乘以 8 不会崩溃。它产出的是通顺的文本。

**而且次数是被断言的，不是目测的。** `collectives/token 92`——每个 MoE layer 一次全规约，93 个 layer 减去开头那个 dense block。*"少一次归约会留下部分专家和；多一次归约会把一个完整张量乘上 `tp_size`。两者都不崩溃，所以次数要靠断言，而不是靠目测。"*

这条通用规则适用于任何并行前向传播：

> **集合通信的位置由部分和下游的第一个非线性决定。** 从代数把它推出来，断言其次数，绝不要照着一张图去摆它。

这一类里还有两道防御值得点名：

* **归约通过构造来验证。** `tp_allreduce_check` 让 rank *r* 用 `r + 1` 填充自己的缓冲区，因此唯一正确的总和就是 `tp(tp+1)/2`。少一个 rank、某个 rank 被重复计算、某个 rank 归约了错误的缓冲区，各自都会产出 *不同* 的数。而且**每个 rank 都被检查，不只是 rank 0**——在一处正确、在另一处错误的集合通信是真实的 bug，而只检查 rank 0 正是它被发出去的路径。
* **精度是正确性的一部分。** K3 刻意让残差流跑 f32。把它走一遍 bf16 的全规约，就会**在每个 layer 边界**截断到约 8 位尾数，把执行器的数值设计毁掉。所以 `make_collective(..., need_f32=true)` 会在 20 分钟的权重加载 *之前*，把只支持 bf16 的快速后端降级为 NCCL，而不是在第一次集合通信时才失败。早失败，在代价便宜的时刻失败。

正面结果是这一切换来的：TP 与 layer-split 流水线的输出一致到 **1.85e-09**，全规约对峰值的精确度达到 **1.127e-07**——低于 f32 epsilon，一个 ulp。


<details>
<summary>English original</summary>

**4.2 Partition arithmetic**

Sharding is index arithmetic, and index arithmetic that is slightly wrong yields a model that runs.

**Bands must tile exactly.** A gap silently drops an expert's contribution; an overlap double-counts it. Either way the all-reduce combine is wrong and the output is plausible. The defense is exhaustive and cheap: `test_expert_bands_tile_exactly` walks every rank at every viable `tp_size` and asserts **each expert is owned exactly once** — and the load check proves the bands tile over real tensors with **16,351 checks**, every byte of a sharded tensor claimed by exactly one rank.

**KV heads often do not divide.** K3 stores MLA as MQA with exactly **1** KV head. At `tp_size 8` there is no way to give each rank a whole KV head, and splitting one is *incorrect* — a KV group must be visible to every query head attending through it. So when `n_kv_heads < tp_size` the KV projections are **replicated** and only the query side is sharded. The naive `n_kv_heads / tp_size` yields **0**, producing — in the repo's words — *"a model with no keys on any rank that still loads and still emits text."*

**Every axis must divide, not just the interesting one.** K3 has 896 experts and `896 % 7 == 0`, so an expert-only check happily accepts `tp_size 7` — but it also has 96 query heads and `96 % 7 != 0`. `shard_dims()` rejects the whole shape and names the offending field. The repo's note on this is refreshingly human: *"My own first draft of the test made exactly this mistake and the test caught it."*

The strategic decision behind all three is the one to copy: **the shard math is CUDA-free and unit-tested without a GPU** — 4,972 checks on `shard.cpp`, 230 on the weight plan, 44 on backend selection. The reasoning:

> *"TP bugs do not live in the collective — NCCL is correct. They live in the band arithmetic, and that is exactly the part you can verify on a laptop before burning node hours."*

**4.3 Collective placement and count**

This family is unique to distributed inference and is the most elegant of the five, because the bug is *algebraic*.

**Why the reduce sits where it does.** The routed experts are expert-sharded, so each rank's dispatch accumulator is a **partial sum** over the top-16. The next op is `ffn_routed_norm`, an RMS norm — and:

```text
   rms_norm(Σ partial)  ≠  Σ rms_norm(partial)
```

RMS norm is **not linear**, so the cross-rank sum must complete *before* it. Move the reduce two ops later, after `routed_up`, and two things go wrong at once: you skip the reduce the experts needed, *and* — because `routed_norm` / `routed_up` / `shexp` are all replicated, so every rank already holds the complete tensor — you reduce a complete tensor and **multiply the FFN output by `tp_size`.**

Multiplying an FFN output by 8 does not crash. It produces fluent text.

**And the count is asserted, not eyeballed.** `collectives/token 92` — one all-reduce per MoE layer, 93 layers minus the leading dense block. *"A missing reduce leaves a partial expert sum; an extra one multiplies a complete tensor by `tp_size`. Neither crashes, so the count is asserted rather than eyeballed."*

The general rule, which applies to any parallel forward pass:

> **A collective's position is determined by the first non-linearity downstream of the partial sum.** Derive it from the algebra, assert the count, and never place it from a diagram.

Two more defenses in this family worth naming:

* **The reduction is verified by construction.** `tp_allreduce_check` has rank *r* fill its buffer with `r + 1`, so the only correct total is `tp(tp+1)/2`. A missing rank, a double-counted rank, or a rank reducing the wrong buffer each produce a *different* number. And **every rank is checked, not just rank 0** — a collective that is correct on one rank and wrong on another is a real bug, and checking only rank 0 is how it ships.
* **Precision is part of correctness.** K3 runs an f32 residual stream deliberately. Routing it through a bf16 all-reduce would truncate to ~8 mantissa bits **at every layer boundary**, undoing the executor's numerics. So `make_collective(..., need_f32=true)` downgrades a bf16-only fast backend to NCCL *before* the 20-minute weight load, rather than failing at the first collective. Fail early, at the cheap moment.

The positive result is what this buys: TP and layer-split pipeline outputs agree to **1.85e-09**, and the all-reduce is exact to **1.127e-07** of peak — below f32 epsilon, one ulp.

</details>

### 4.4 静默不发生的工作

这是最恶劣的一类，因为代码是正确的，只是根本没有运行。

典型案例是 [PR #33](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/33)：*"MLA decode attention **在上下文超过约 11.7k 后静默停止启动**。"* 一个 launch 配置超出了限制，launch 失败，返回码未被检查，前向传播就带着过期或被清零的 buffer 继续跑下去。在约 11.7k 以内它工作得完美无缺。超过之后，attention 输出全是垃圾——而引擎仍在继续生成。

修复随 [PR #49](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/49) 一起落地：*"**launch 失败时让前向传播失败**。"* 一行错误检查，就让一整类 bug 变得大声可见。

同样的形态遍布整个 harness（agent 运行时框架），而这正是一个项目是否已理解该模式的标志：

| PR | 静默通过 |
|---|---|
| [#88](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/88) | "报告 `llama-tokenize` 失败，而不是静默退出" |
| [#100](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/100) | "基线写了一个 harness 从不读取的 slot" |
| [#112](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/112) | "三个 harness 缺陷，让一次坏掉的运行看起来像是好运行" |
| [#125](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/125) | "harness 在空 `--ids` 上静默通过，以及 logits 写入失败" |

把这份列表读作同一个 bug 的重复：**空结果和成功结果无法区分。** 空输入、未写入的输出、未被读取的 slot、失败的 launch——每一个都会产出一个绿色的运行。

> **规则。** 每个阶段都必须能区分「什么都没做」与「做对了」。如果你的 pipeline 的成功条件是「没有报错」，那你根本就没有成功条件。要对*输出的存在与形状*做断言，而不是对没有失败做断言。

### 4.5 数值正确，但不是你指定的那套数值

症状更温和，但这正是精度一致性 gate 花掉大部分时间的地方。本案例研究的源头例子是 [PR #7](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/7)：*"llama.cpp 精度一致性——beta sigmoid + KDA decay 轴（**KLD 5.34 → 4e-3**）。"*

5.34 nats 的 KL 不是轻微的漂移——那是另一个模型。成因是用错了激活函数变体，以及 **decay 施加在了错误的轴上**。两者都产出流畅的输出；两者在没有参考实现时都不可见；两者都是被 gate 发现的，而不是靠读样本发现的。

还有一个值得记住的对应事实：**并非所有偏离都是 bug。** 本引擎中的平均 KLD 比同一实现跑两次所应期望的 1e-5 高出约 400×，且成因已定位——K3 保留 f32 激活值，而 ggml 在量化 mat-vec 之前会把它们量化掉。给贡献者的指示完全正确：*"不要把它当成 bug 去追；不要让它变得更糟。"*

> 了解你**不可约**的偏离及其成因，才能把偏离的任何*变化*当作信号。没有刻画清楚自身底噪的团队，用不了自己的 gate。

---


<details>
<summary>English original</summary>

**4.4 Work that silently does not happen**

The nastiest family, because the code is correct and simply is not running.

The canonical case is [PR #33](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/33): *"MLA decode attention **silently stops launching** past ~11.7k context."* A launch configuration exceeded a limit, the launch failed, the return code was not checked, and the forward pass continued with a stale or zeroed buffer. Under ~11.7k it worked perfectly. Past it, the attention output was garbage — and the engine kept generating.

The fix shipped alongside [PR #49](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/49): *"**fail the forward on a failed launch**." One line of error checking, and an entire class of bug becomes loud.

The same shape appears throughout the harness, and this is the tell that a project has understood the pattern:

| PR | The silent pass |
|---|---|
| [#88](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/88) | "report a `llama-tokenize` failure instead of exiting silently" |
| [#100](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/100) | "the baseline wrote a slot the harness never reads" |
| [#112](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/112) | "three harness defects that make a broken run look like a good one" |
| [#125](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/125) | "harness silent-pass on empty `--ids` and failed logits write" |

Read that list as one bug repeated: **an empty result and a successful result were indistinguishable.** Empty input, unwritten output, unread slot, failed launch — every one produced a green run.

> **The rule.** Every stage must be able to distinguish "did nothing" from "did the right thing." If your pipeline's success condition is "no error was raised," you do not have a success condition. Assert on the *presence and shape of output*, not the absence of failure.

**4.5 Numerics that are correct but not the numerics you specified**

Milder, but it is what a parity gate spends most of its time on. The case study's founding example is [PR #7](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/7): *"llama.cpp parity — beta sigmoid + KDA decay axis (**KLD 5.34 → 4e-3**)."*

A KL of 5.34 nats is not a subtle drift — it is a different model. The causes were a wrong activation variant and a **decay applied along the wrong axis**. Both produce fluent output; both are invisible without a reference; both were found by the gate rather than by reading samples.

And the counterpart worth holding onto: **not all divergence is a bug.** Mean KLD in this engine sits ~400× above the 1e-5 you would expect from two runs of the same implementation, from an identified cause — K3 keeps f32 activations where ggml quantizes them before a quantized mat-vec. The instruction to contributors is exactly right: *"Do not go hunting it as a bug; do not make it worse."*

> Knowing your **irreducible** divergence, and its cause, is what lets you treat any *change* in divergence as signal. A team that has not characterized its floor cannot use its gate.

---

</details>

## 5. 防御栈

按成本排序。便宜的那些能抓住大部分。

```text
   FREE, no GPU
     · shard / partition math as pure functions, exhaustively tested
       (4972 + 230 + 44 checks here, all on a laptop)
     · every axis validated to divide, with the offending field named
     · required-not-defaulted config keys
     · one env-gate idiom, used everywhere
     · lint EVERY script, not a hand-maintained list

   CHEAP, one run
     · check every launch's return code; fail the forward
     · assert the collective COUNT per token
     · assert bands tile exactly over real tensors
     · verify the reduce by construction (r+1 ⇒ tp(tp+1)/2), on EVERY rank
     · compute-sanitizer clean: 0 errors

   PER-CHANGE
     · parity vs reference at multiple depths, worst-of not average
     · bit-identity claims PROVED bit-identical
     · structural-parameter sweep when parity degrades (§2)
     · plausibility ceiling on the speed number

   ONGOING
     · correctness gate ordered BEFORE the speed gate
     · known irreducible divergence documented, with its cause
     · pin-drift audit on external references
```

注意顶层中有多少是免费的。本案例研究中代价最高的 bug —— expert bands、KV-head 划分、reduce 布局 —— 全都能靠笔记本上的纯函数捕获。**GPU 是用来验证管线的，不是用来验证算术的。**

再补一条，同属一类而且容易被忽视：[PR #97](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/97) 发现 copycat guard *“从未运行过 —— **100 次运行中有 100 次是启动失败**。”* 一百轮全绿，零覆盖。一个失败看起来像通过的检查，比没有检查更糟，因为它让人就此放下了这份担忧。

> **断言你的断言确实运行了。** 一个 guard 需要一个正向信号，证明它执行了并且评估了某些东西。“没有报告失败”与“从未启动”是同一份输出。

CRLF 这个故事是同一教训的微缩版：一个以 CRLF 提交的脚本，其 heredoc 终止符是 `PY\r`，而它永远无法匹配 `PY`，于是 `bash` 报出 “unexpected end of file”，**脚本根本无法执行。** 这是在*“几秒钟内 lint 每个脚本、而不是四个”*时发现的。手工维护的检查清单，是一种不检查东西的方式。

---

## 6. 负面结果，以及那次错误的 revert

一条从未出现 failure 的 ladder，一定是被编辑过的。真实的分布是这样的。

### 6.1 合并、revert、重新应用

仓库中最有价值的三 PR 序列：

```text
   #81   MERGED, eval:s
         "quantise each activation once, not once per projection
          (+4.8%, bit-identical)"

   #84   MERGED
         "Revert #81 — 31.5% regression on current main"

   #94   MERGED
         "Reapply #81 — THE REVERT WAS BASED ON A BAD MEASUREMENT"
```

一次真实的优化因为一个糟糕的测量被 revert，不得不再度合入。三条教训，按不适程度递增排列：

1. **你的回归检测器本身也是一种测量，有着与被它评分的对象相同的失效模式。** 一个 31.5% 的“回归”是个很大的数字，而大数字让人觉得权威。这一次是噪声、机器方差，或者一次坏的 build。
2. **revert 不是一个免费动作。** 它是对 `main` 的一次改动，理应享有与被它撤销的那次改动同等的证据标准。基于一次糟糕读数就 revert，代价是多出两个 PR，并让 ladder 暂时处于错误状态。
3. **只有历史完好，你才可能恢复。** 要正确地重新应用，必须回到封存的凭证，确定两个相互矛盾的测量中哪一个是真实的。这就是 [Lecture 02](Lecture-02.md) 的回报。


<details>
<summary>English original</summary>

**5. The defense stack**

Ordered by cost. The cheap ones catch most of it.

```text
   FREE, no GPU
     · shard / partition math as pure functions, exhaustively tested
       (4972 + 230 + 44 checks here, all on a laptop)
     · every axis validated to divide, with the offending field named
     · required-not-defaulted config keys
     · one env-gate idiom, used everywhere
     · lint EVERY script, not a hand-maintained list

   CHEAP, one run
     · check every launch's return code; fail the forward
     · assert the collective COUNT per token
     · assert bands tile exactly over real tensors
     · verify the reduce by construction (r+1 ⇒ tp(tp+1)/2), on EVERY rank
     · compute-sanitizer clean: 0 errors

   PER-CHANGE
     · parity vs reference at multiple depths, worst-of not average
     · bit-identity claims PROVED bit-identical
     · structural-parameter sweep when parity degrades (§2)
     · plausibility ceiling on the speed number

   ONGOING
     · correctness gate ordered BEFORE the speed gate
     · known irreducible divergence documented, with its cause
     · pin-drift audit on external references
```

Note how much of the top tier is free. The most expensive bugs in this case study — expert bands, KV-head division, reduce placement — are all catchable by pure functions on a laptop. **The GPU is where you validate plumbing, not arithmetic.**

One more, from the same family and easy to overlook: [PR #97](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/97) found that the copycat guard *"has never run — **100 of 100 runs were startup failures**."* A hundred green rounds, zero coverage. A check whose failures look like passes is worse than no check, because it retires the worry.

> **Assert your assertions ran.** A guard needs a positive signal that it executed and evaluated something. "No failure reported" is the same output as "never started."

The CRLF story is the same lesson in miniature: a script committed with CRLF had heredoc terminator `PY\r`, which never matches `PY`, so `bash` reported "unexpected end of file" and **the script could not execute at all.** Found *"within seconds of linting every script instead of four."* Hand-maintained lists of what to check are a way of not checking things.

---

**6. Negative results, and the revert that was wrong**

A ladder with no failures has been edited. Here is what the real distribution looks like.

**6.1 Merged, reverted, reapplied**

The most valuable three-PR sequence in the repository:

```text
   #81   MERGED, eval:s
         "quantise each activation once, not once per projection
          (+4.8%, bit-identical)"

   #84   MERGED
         "Revert #81 — 31.5% regression on current main"

   #94   MERGED
         "Reapply #81 — THE REVERT WAS BASED ON A BAD MEASUREMENT"
```

A real optimization was reverted on a bad measurement and had to be re-landed. Three lessons, in increasing order of discomfort:

1. **Your regression detector is a measurement too, with the same failure modes as the thing it grades.** A 31.5% "regression" is a large number, and large numbers feel authoritative. This one was noise, box variance, or a bad build.
2. **Revert is not a free action.** It is a change to `main`, and it deserves the same standard of evidence as the change it undoes. Reverting on one bad reading cost two extra PRs and left the ladder temporarily wrong.
3. **You can only recover if the history is intact.** Reapplying correctly required going back to the sealed receipts and establishing which of two contradictory measurements was real. This is [Lecture 02](Lecture-02.md)'s payoff.

</details>

### 6.2 `eval:none` 清单

`eval:none` 表示已测量、真实存在，且**小到不足以计分** —— 收益落在 2% 显著性门槛之内。

| PR | 声称 | 结果 |
|---|---|---|
| [#113](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/113) | 五处 decode（逐 token 生成阶段）路径改动，128k 下 +15.4% | `eval:none`，已关闭 |
| [#104](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/104) | MoE act-fuse + packed IQ1_S + MLA pipe，128k 下 +10.4% | `eval:none`，已关闭 |
| [#123](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/123) | 让 RMS norm 自己输出 Q8_0 | `eval:none`，已关闭 |
| [#129](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/129) | 分阶段处理 MLA broadcast 共享读取，+3.4% 且 bit 级一致 | 已关闭 |
| [#71](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/71) | 加宽最后那次单 block launch | `eval:none` —— **两次** |

从这张表能读出两件事。

**两位数的声称可能测得为零。**[#113](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/113) 与 [#104](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/104) 都声称 >10%，实测计分 `none`。这是自测 microbenchmark 撞上交错同机 harness（agent 运行时框架）的常态结果 —— 也正是贡献规则要求 *端到端* 改进、而非孤立 kernel benchmark 的原因。

**同一个想法两次计分 `none`，之后又计分 `s`。**[#71](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/71) 是「加宽最后那次单 block launch」；它两次尝试都计分 `none`。同一个想法后来以 [#115](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/115) 合入，带 `s` 层级和一个让人记得住的标题 —— *「327 norms/token 跑在 128 个线程上」*（[Lecture 04](Lecture-04.md)）。

变的是**引擎的其余部分。** 显著性门槛是*前沿的* 2%，所以它是一个随你改进而升高的绝对 tok/s 门槛。在前沿某一处小到测不出的收益，换到另一处就可能测得出 —— 或者反过来，随着速度变快，一个固定大小的收益按比例变得*更难*计分。repo 自己的注记：*「一个真实但很小的收益现在计分 `none` —— 见 #71，两次。」*

> **一个被拒绝的优化是针对某个状态被拒绝的，而非永远被拒绝。** 保留该分支。等前沿移动后重新测量。

### 6.3 要记录什么

对你自己的产物，摘自 [Part 4 README](README.md)：

* 每一项改动，包括那些什么都没测出来的。
* 声称 *与* 实测结果，让差距可见。
* 回滚，以及该回滚后来是否被发现是错的。
* bit 级一致性的声称，以及是如何证明的。
* 已撤回的数字，以及它们是由什么产生的。

案例研究的 changelog 里有一节标题就叫 **“Note on a retracted number”**，把 169.72 与真实的 98.80 并列记录。这就是可审计记录的样子，而它只花一段话的篇幅。

---

## 7. 检查清单

合并任何优化之前，按顺序：

1. **这可能悄无声息地破坏什么？** 说出 §4 中的那一类。如果说不出，说明你还没理解这项改动。
2. **有什么东西断言它没有破坏吗？** 不是「没报错」—— 有没有对输出的形状和存在性做正向检查？
3. **如果它声称 bit 级一致，它被证明 bit 级一致了吗？** 相同输入，逐字节比对输出。「应该是」不是测量。
4. **哪个结构参数会让它暴露？** 层数、head 数、chunk 宽度、`tp_size`、上下文深度。对改动触及的每一个参数，都在多于一个取值上测试（§2）。
5. **收益与预测相符吗？** 意外在被证伪之前都是一份 bug 报告（§3）。
6. **它好得不像真的吗？** 如果它超过了你认为可信的上限，先核实再收下。
7. **精度一致性关卡在深度上跑过吗？** 对 KV cache、attention、路由或 LM head 的改动，恰恰是浅层探针会漏掉的那一类。
8. **每个 guard 都真的跑了吗？** 找正向信号，而不是没有红色（§5）。

---


<details>
<summary>English original</summary>

**6.2 The `eval:none` catalog**

`eval:none` means measured, real, and **not big enough to score** — a gain inside the 2% significance gate.

| PR | Claimed | Outcome |
|---|---|---|
| [#113](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/113) | five decode-path changes, +15.4% at 128k | `eval:none`, closed |
| [#104](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/104) | MoE act-fuse + packed IQ1_S + MLA pipe, +10.4% at 128k | `eval:none`, closed |
| [#123](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/123) | let the RMS norm emit its own Q8_0 | `eval:none`, closed |
| [#129](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/129) | stage MLA broadcast shared reads, +3.4% bit-identical | closed |
| [#71](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/71) | widen the last single-block launch | `eval:none` — **twice** |

Two things to read off this table.

**A double-digit claim can measure zero.** [#113](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/113) and [#104](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/104) each claimed >10% and scored `none`. This is the normal outcome of a self-measured microbenchmark meeting an interleaved same-box harness — and it is why the contribution rules require an *end-to-end* improvement rather than an isolated kernel benchmark.

**The same idea scored `none` twice and then `s`.** [#71](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/71) was "widen the last single-block launch"; it scored `none` on two attempts. The identical idea later merged as [#115](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/115) with an `s` tier and a memorable title — *"327 norms/token were running on 128 threads"* ([Lecture 04](Lecture-04.md)).

What changed was **the rest of the engine.** The significance gate is 2% *of the frontier*, so it is an absolute bar in tok/s that rises as you improve. A win too small to measure at one frontier can be measurable at another — or, in the other direction, a fixed-size win becomes proportionally *harder* to score as you get faster. The repo's own note: *"A real but small win now scores `none` — see #71, twice."*

> **A rejected optimization is rejected against a state, not for all time.** Keep the branch. Re-measure after the frontier moves.

**6.3 What to record**

For your own artifact, from [the Part 4 README](README.md):

* Every change, including the ones that measured nothing.
* The claim *and* the measured result, so the gap is visible.
* Reverts, and whether the revert was later found to be wrong.
* Bit-identity claims and how they were proved.
* Retracted numbers, with what produced them.

The case study's changelog contains a section literally headed **"Note on a retracted number,"** documenting the 169.72 alongside the real 98.80. That is what an auditable record looks like, and it costs one paragraph.

---

**7. The checklist**

Before merging any optimization, in order:

1. **What could this silently break?** Name the family from §4. If you cannot name one, you do not understand the change yet.
2. **Does anything assert it did not?** Not "did no error occur" — is there a positive check on the shape and presence of output?
3. **If it claims bit-identical, is it proved bit-identical?** Same inputs, byte-compared outputs. "Should be" is not a measurement.
4. **What structural parameter would expose it?** Layers, heads, chunk width, `tp_size`, context depth. Test at more than one value of each the change touches (§2).
5. **Did the win match the prediction?** A surprise is a bug report until proven otherwise (§3).
6. **Is it too good to be true?** If it beats your plausibility ceiling, check before banking.
7. **Was the parity gate exercised at depth?** A change to KV cache, attention, routing, or the LM head is exactly the class the shallow probes miss.
8. **Did every guard actually run?** Look for the positive signal, not the absence of red (§5).

---

</details>

## 实验 — 搭建 corruption 套件

目标：亲手写出这些 bug，证明你的 gate 能抓住静默的错误。

1. **写出五种刻意引入的 corruption**，对应 §4 中每一类各一个。建议：把某个 config 索引偏移一位；在某个 partition 中引入一个元素的空洞；把某个集合通信移到非线性算子之后；让某次 kernel launch 失败却不做检查；更换激活函数的某个变体。
2. **每一项记录三件事**：它会不会崩？输出肉眼看着是否合理？相对你的参考实现，最差深度处的 KL 是多少？
3. **为每个 corrupted build 计时。** 注意哪些比正确版本 *更快*。预计至少有一个。
4. **确认你的 gate 拒绝了全部五个。** 有任何一个通过，就去修 gate —— 那才是这个实验真正的产出。
5. **扫一个结构参数**：在你使其依赖深度或宽度的那项 corruption 上扫描该参数，找出 parity 脱离 0 的阈值。确认该阈值正好指向那个 bug（§2）。
6. **为断言本身加断言。** 挑出你最重要的 guard，让它 *不运行*，看 CI 是否仍然全绿。修到它不可能再绿。
7. **写出 `CORRUPTIONS.md`** —— 那五个 bug、它们的症状、它们的耗时，以及抓住每一个的检查。提交它。这是你写过的最有用的测试文档。

通过标准：一个可测量地比 `main` *更快*、且在 harness 报告其速度之前就被拒绝的 corrupted build。

---

## 自检

1. 为什么语言模型无法察觉自己的权重错了 6%？请从输出空间的角度回答。
2. 某个 parity 指标在 `n_layers` ≤ 12 时读数恰好为 0，在 14 时为 1.64，在 93 时为 3.47。这是哪一类 bug？你会 grep 什么表达式？
3. 你的 all-reduce 放在 FFN up-projection 之后，而不是放在 routed norm 之前。两个版本都能跑，也都输出流畅的文本。用代数方式解释各自算的是什么，以及为什么错的那一个差了一个因子。
4. 你的引擎保留 f32 residual stream，而你的快速集合通信后端只支持 bf16。描述这个 corruption、它的量级、每个 token 发生多少次，以及这项检查该放在哪里。
5. 某个 PR 声称 "bit-identical, +4.8%"。在相信前半句之前，你确切要求什么？
6. 某处改动实测 4.2×，而你预测的是 1.4×。按顺序列出你在庆祝之前要跑的检查。
7. 某个优化两次得分都是 `none`，第三次没有任何代码改动就合并了。什么变了？这对保留被拒分支意味着什么？
8. 你的 CI 已经连续 100 次全绿。给出两个不同的理由，说明这不能证明你的 guard 有效。
9. `n_kv_heads = 1` 与 `tp_size = 8`。给出朴素的 shard 结果、它产生的输出，以及正确的处理方式。

---

## 参考文献

* **SparkInfer-K3 正确性记录** — stride 转置与被撤回的 169.72，见 [`CHANGELOG.md`](https://github.com/gittensor-ai-lab/sparkinfer-k3/blob/main/CHANGELOG.md) ("Note on a retracted number")；配置陷阱见 [`docs/technical.md`](https://github.com/gittensor-ai-lab/sparkinfer-k3/blob/main/docs/technical.md)；partition 与集合通信布局的陷阱见 [`docs/tensor-parallel.md`](https://github.com/gittensor-ai-lab/sparkinfer-k3/blob/main/docs/tensor-parallel.md)。
* **回滚循环** — [#81](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/81) → [#84](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/84) → [#94](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/94)。值得按顺序读。
* **Silent-launch 类** — [#33](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/33)（超过 ~11.7k 后就不再 launch）、[#49](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/49)（launch 失败时让 forward 失败）、[#112](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/112)、[#125](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/125)。
* **compute-sanitizer** — [docs.nvidia.com/compute-sanitizer](https://docs.nvidia.com/compute-sanitizer/) — `memcheck`、`racecheck`、`initcheck`、`synccheck`。本仓库要求其干净通过。
* **KL 散度与 top-1 一致率作为正确性 gate** — [Logprobs、Perplexity 与 KL 散度 — Lecture 05](../../04-Logprobs-困惑度与KL散度/Lecture-05.md)，其中推导了 §4.5 所依赖的指标。
* **"Which Quantization Should I Use?"** — [arXiv:2601.14277](https://arxiv.org/abs/2601.14277) — 内在指标是必要的但不充分；这与 parity gate 看不到的东西相关。

交叉引用：

* [Lecture 02 — 记分板](Lecture-02.md) — gate 顺序、合理性上限，以及让 §6.1 可恢复的封存凭据。
* [Lecture 03 — 诊断](Lecture-03.md) — 那条「先写下预测」的规则，让 §3 变得可操作。
* [Lecture 07 — 分片 896 个 expert](Lecture-07.md) — §4.3 的 reduce 布局代数及其性能语境。
* [Lecture 09 — 批处理 prefill](Lecture-09.md) — §2 剖析其两个缺陷的那次改动。


<details>
<summary>English original</summary>

**Lab — build a corruption suite**

Goal: prove your gate catches silent wrongness, by writing the bugs yourself.

1. **Write five deliberate corruptions**, one per family in §4. Suggested: shift a config index by one; introduce a one-element gap in a partition; move a collective past a non-linearity; make a kernel launch fail without checking; change an activation variant.
2. **For each, record three things**: does it crash? does the output look plausible to you by eye? what is the worst-depth KL against your reference?
3. **Time each corrupted build.** Note which ones are *faster* than correct. Expect at least one.
4. **Confirm your gate rejects all five.** Any that pass, fix the gate — that is the real output of this lab.
5. **Sweep a structural parameter** on the corruption you made depth- or width-dependent, and find the threshold where parity leaves zero. Confirm the threshold points at the bug (§2).
6. **Assert your assertions.** Pick your most important guard, make it *not run*, and check whether your CI still goes green. Fix it so it cannot.
7. **Write `CORRUPTIONS.md`** — the five bugs, their symptoms, their timings, and the check that catches each. Commit it. It is the most useful test documentation you will write.

Pass criterion: a corrupted build that is measurably *faster* than `main` and is rejected by your harness before its speed is ever reported.

---

**Self-check**

1. Why can a language model not detect that its own weights are 6% wrong? Answer in terms of the output space.
2. A parity metric reads exactly 0 for `n_layers` ≤ 12, then 1.64 at 14 and 3.47 at 93. What class of bug is this, and what expression would you grep for?
3. Your all-reduce sits after the FFN up-projection instead of before the routed norm. Both versions run and produce fluent text. Explain algebraically what each computes and why the wrong one is off by a factor.
4. Your engine keeps an f32 residual stream and your fast collective backend is bf16-only. Describe the corruption, its magnitude, how many times per token it occurs, and where the check belongs.
5. A PR claims "bit-identical, +4.8%." What exactly do you require before believing the first half?
6. A change measures 4.2× where you predicted 1.4×. List the checks you run before celebrating, in order.
7. An optimization scored `none` twice and merged the third time with no code change. What moved, and what does that imply about keeping rejected branches?
8. Your CI has been green for 100 runs. Give two distinct reasons that is not evidence your guard works.
9. `n_kv_heads = 1` and `tp_size = 8`. Give the naive shard result, the output it produces, and the correct handling.

---

**References**

* **SparkInfer-K3 correctness record** — the stride transposition and retracted 169.72 in [`CHANGELOG.md`](https://github.com/gittensor-ai-lab/sparkinfer-k3/blob/main/CHANGELOG.md) ("Note on a retracted number"); the config traps in [`docs/technical.md`](https://github.com/gittensor-ai-lab/sparkinfer-k3/blob/main/docs/technical.md); the partition and collective-placement traps in [`docs/tensor-parallel.md`](https://github.com/gittensor-ai-lab/sparkinfer-k3/blob/main/docs/tensor-parallel.md).
* **The revert cycle** — [#81](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/81) → [#84](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/84) → [#94](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/94). Worth reading in order.
* **Silent-launch class** — [#33](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/33) (stops launching past ~11.7k), [#49](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/49) (fail the forward on a failed launch), [#112](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/112), [#125](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/125).
* **compute-sanitizer** — [docs.nvidia.com/compute-sanitizer](https://docs.nvidia.com/compute-sanitizer/) — `memcheck`, `racecheck`, `initcheck`, `synccheck`. Required clean in this repo.
* **KL divergence and top-1 agreement as a correctness gate** — [Logprobs, Perplexity & KL Divergence — Lecture 05](../../04-Logprobs-困惑度与KL散度/Lecture-05.md), which derives the metrics §4.5 relies on.
* **"Which Quantization Should I Use?"** — [arXiv:2601.14277](https://arxiv.org/abs/2601.14277) — intrinsic metrics are necessary but not sufficient; relevant to what a parity gate cannot see.

Cross-references:

* [Lecture 02 — The scoreboard](Lecture-02.md) — gate order, the plausibility ceiling, the sealed receipts that made §6.1 recoverable.
* [Lecture 03 — Diagnosis](Lecture-03.md) — the write-down-your-prediction rule that makes §3 operational.
* [Lecture 07 — Sharding 896 experts](Lecture-07.md) — the reduce-placement algebra of §4.3 in its performance context.
* [Lecture 09 — Batched prefill](Lecture-09.md) — the change whose two defects §2 dissects.

---

</details>

## 截至 2026-08

SparkInfer-K3 位于 `7689cc7`。精度一致性门限：top-1 ≥ 0.95，平均 KL ≤ 0.05，七个深度，取最差；合并运行落在 0.004–0.008。已知不可约发散约为同一实现下限的 400×，原因已定位（f32 激活值 vs 预量化 mat-vec）。TP/pipeline 一致性 1.85e-09；all-reduce 精确到峰值的 1.127e-07。需要 CUDA 12.8+，且要求 `compute-sanitizer` clean。五个家族与防御栈是耐久的内容。

---

## 下一讲

* 这是 Part 4 的最后一讲。上一级：[Part 4 — 优化真实引擎](README.md)
* 上一讲：[Lecture 09 — 你遗忘的阶段：批处理 prefill](Lecture-09.md)
* 课程主页：[AI Inference Engineer 2026](../README.md)


<details>
<summary>English original</summary>

**Current as of 2026-08**

SparkInfer-K3 at `7689cc7`. Parity gate: top-1 ≥ 0.95, mean KL ≤ 0.05, seven depths, worst-of; merged runs sit at 0.004–0.008. Known irreducible divergence ~400× the same-implementation floor, cause identified (f32 activations vs pre-quantized mat-vec). TP/pipeline agreement 1.85e-09; all-reduce exact to 1.127e-07 of peak. CUDA 12.8+, `compute-sanitizer` clean required. The five families and the defense stack are the durable content.

---

**Next**

* This is the last lecture of Part 4. Up: [Part 4 — Optimizing a Real Engine](README.md)
* Previous: [Lecture 09 — The phase you forgot: batched prefill](Lecture-09.md)
* Course home: [AI Inference Engineer 2026](../README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 4 - Optimizing a Real Engine/Lecture-10.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%204%20-%20Optimizing%20a%20Real%20Engine/Lecture-10.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
