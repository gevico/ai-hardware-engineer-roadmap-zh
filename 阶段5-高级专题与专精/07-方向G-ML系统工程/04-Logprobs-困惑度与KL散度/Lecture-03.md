---
title: 第 03 讲 - 困惑度：困惑的指数
description: 第 03 讲 - 困惑度：困惑的指数
---

# 第 03 讲 - 困惑度：困惑的指数

**合集：** [对数概率、困惑度与 KL 散度](README.md) | **上一讲：** [← 第 02 讲](Lecture-02.md) | **下一讲：** [第 04 讲](Lecture-04.md)

---

第 02 讲留给你的是一笔损失：交叉熵 `H(p, q) = −Σ p log q`，即模型每个 token 付出的平均负对数似然。这个数字的单位是 nats，对像样的现代大语言模型来说落在 1.8 到 2.5 之间，而几乎没有人引用它。人们引用的是**困惑度**——同一个量，只是经过了 `exp`。困惑度是语言建模中被引用最多的单个数字，出现在每一张量化表格里，也出现在每一条“我们的模型打败了他们”的推文里。它也是被滥用最甚的一个，因为有三种不同的失效模式会悄悄让人们试图用它做的比较失效。

困惑度不是什么新想法。它就是 `exp(H(p, q))`，仅此而已——对你已经在训练的损失做一次单调换皮。它之所以能单独占一讲，是因为 `exp` 为你换来了一种*解读*（有效分支因子——模型实际上是在多少个等概率 token 之间做选择），而围绕它的报告*惯例*（tokenizer 归一化、滑动窗口、语料选择）承载着每一个会把这个干净指标变成错误结论的陷阱。

本讲直接从第 02 讲推导困惑度，用一个微例子给出分支因子的读法，然后把大部分篇幅花在三件让从业者把它报错的事情上：tokenizer 依赖（用 bits-per-byte 解决）、长文档打分问题（用滑动窗口解决），以及把困惑度当成输出质量真值的诱惑（它只是一个粗略代理——严格版本在第 05 讲）。到本讲结束，困惑度会成为你对量化模型的第一道验收测试，而你也清楚知道该在什么时候停止相信它。

---

## 学习目标

本讲结束时，你应该能够：

1. 直接从第 02 讲的损失推导出 `PPL = exp(mean NLL) = exp(H(p, q)) = 2^(bits per token)`，并把它读作 `1/q(x_t)` 的几何平均。
2. 把困惑度解释为**有效分支因子**，并在一个小例子上手算它。
3. 解释为什么困惑度**不能跨 tokenizer 比较**，并转换到 **bits-per-byte（BPB）** 使其可比。
4. 实现 **HF 带步长的滑动窗口**困惑度循环，并解释为什么朴素分块会把这个数字抬高。
5. 说出标准语料（WikiText-2/103、C4）以及为使结果可复现你必须报告的上下文/步长。
6. 把困惑度用作**标准的量化门禁**（INT4 相对 FP16 的 PPL 差值），并准确说明为什么它只是一个*粗略*代理——把严格的评分器留到第 05 讲。

---

## 1. 定义与推导

从第 02 讲结束的地方开始。对于一条留出序列 `x_1 … x_N`，在模型 `q` 下以 teacher forcing 计算，损失就是平均负对数似然：

```text
    L  =  mean NLL  =  (1/N) Σ_t [ −log q(x_t | x_<t) ]   (nats)
```

当参考分布 `p` 就是经验数据（在实际出现的那个 token 上取 one-hot）时，这个平均 NLL *就是*交叉熵 `H(p, q)`——这正是第 02 讲建立起来的恒等式。**困惑度就是它的指数：**

```text
    PPL  =  exp(L)  =  exp( H(p, q) )  =  exp( (1/N) Σ_t −log q(x_t | x_<t) )
```

这就是全部定义。其余的一切都只是换种方式读它。

**换个底数来看。** 这里 `exp` 和 `log` 是自然底（nats），但损失在任何底数下内容都一样。如果用**比特**来度量交叉熵（`log2`），困惑度就是 `2` 的 bits-per-token 次幂：

```text
    PPL  =  2^( bits per token )  =  2^( H_bits(p, q) )
    H_bits  =  H_nats / ln 2          (1 nat = 1/ln2 ≈ 1.4427 bits)
```

损失 `2.0 nats` 即 `2.0 / 0.6931 ≈ 2.885 bits`，也即 `PPL = e^2.0 = 2^2.885 ≈ 7.39`。工具报告哪个底数就用哪个；只是绝不要默不作声地把它们混用。

**作为几何平均。** 把 `exp` 推入求和式，它就变成乘积。困惑度就是模型赋予真实 token 的*倒数*概率的几何平均：

```text
    PPL  =  exp( (1/N) Σ_t −log q(x_t) )
         =  exp( (1/N) Σ_t  log (1 / q(x_t)) )
         =  ( Π_t  1 / q(x_t) )^(1/N)
```

这是最具物理意味的读法。`1/q(x_t)` 就是“模型对 token `t` 有多惊讶”——当它给真相分配的概率很小时，这个值就大。困惑度是这类惊讶的典型值，采用几何平均，于是一个灾难性的 token（概率接近零）会以乘法方式把它撑爆，正如它以加法方式撑爆损失一样。是几何平均，不是算术平均，因为求平均是在对数空间里进行的。

---


<details>
<summary>English original</summary>

**Lecture 03 - Perplexity: The Exponential of Confusion**

**Collection:** [Logprobs, Perplexity & KL Divergence](README.md) | **Previous:** [← Lecture 02](Lecture-02.md) | **Next:** [Lecture 04](Lecture-04.md)

---

Lecture 02 left you holding the loss: cross-entropy `H(p, q) = −Σ p log q`, the mean negative log-likelihood your model pays per token. That number is in nats, it lives somewhere between 1.8 and 2.5 for a decent modern LLM, and almost nobody quotes it. They quote **perplexity** instead — the same quantity, run through `exp`. Perplexity is the single most-cited number in language modeling, the one in every quantization table and every "our model beats theirs" tweet. It is also the most misused, because three different failure modes quietly invalidate the comparison people are trying to make with it.

Perplexity is not a new idea. It is `exp(H(p, q))` and nothing more — a monotone re-skinning of the loss you already train on. The reason it earns its own lecture is that the `exp` buys you an *interpretation* (an effective branching factor — how many equally-likely tokens the model is, in effect, choosing between) and the reporting *conventions* around it (tokenizer normalization, sliding windows, corpus choice) carry every trap that turns a clean metric into a wrong conclusion.

This lecture derives perplexity straight from Lecture 02, gives you the branching-factor reading with a worked micro-example, then spends most of its length on the three things that make practitioners report it wrong: tokenizer dependence (fixed by bits-per-byte), the long-document scoring problem (fixed by the sliding window), and the temptation to treat perplexity as ground truth for output quality (it is a rough proxy — Lecture 05 does the rigorous version). By the end, perplexity is your first-line acceptance test for a quantized model, and you know exactly when to stop trusting it.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Derive `PPL = exp(mean NLL) = exp(H(p, q)) = 2^(bits per token)` directly from the Lecture 02 loss, and read it as a geometric mean of `1/q(x_t)`.
2. Interpret perplexity as an **effective branching factor**, and compute it by hand on a tiny example.
3. Explain why perplexity is **not comparable across tokenizers**, and convert to **bits-per-byte (BPB)** so it is.
4. Implement the **HF strided sliding-window** perplexity loop and explain why naive chunking inflates the number.
5. Name the standard corpora (WikiText-2/103, C4) and the context/stride you must report for a result to be reproducible.
6. Use perplexity as the **canonical quantization gate** (PPL delta of INT4 vs FP16), and state precisely why it is a *rough* proxy — deferring the rigorous grader to Lecture 05.

---

**1. Definition and derivation**

Start from where Lecture 02 ended. For a held-out sequence `x_1 … x_N`, teacher-forced under the model `q`, the loss is the mean negative log-likelihood:

```text
    L  =  mean NLL  =  (1/N) Σ_t [ −log q(x_t | x_<t) ]   (nats)
```

When the reference distribution `p` is the empirical data (one-hot on the token that actually occurred), this mean NLL *is* the cross-entropy `H(p, q)` — that is the identity Lecture 02 built. **Perplexity is its exponential:**

```text
    PPL  =  exp(L)  =  exp( H(p, q) )  =  exp( (1/N) Σ_t −log q(x_t | x_<t) )
```

That is the whole definition. Everything else is reading it differently.

**As a base change.** `exp` and `log` are natural (nats) here, but the loss is identical content in any base. If you measure cross-entropy in **bits** (`log2`), perplexity is `2` raised to the bits-per-token:

```text
    PPL  =  2^( bits per token )  =  2^( H_bits(p, q) )
    H_bits  =  H_nats / ln 2          (1 nat = 1/ln2 ≈ 1.4427 bits)
```

A loss of `2.0 nats` is `2.0 / 0.6931 ≈ 2.885 bits`, and `PPL = e^2.0 = 2^2.885 ≈ 7.39`. Use whichever base your tooling reports; just never mix them silently.

**As a geometric mean.** Push the `exp` through the sum and it becomes a product. Perplexity is the geometric mean of the *reciprocal* probabilities the model assigned to the true tokens:

```text
    PPL  =  exp( (1/N) Σ_t −log q(x_t) )
         =  exp( (1/N) Σ_t  log (1 / q(x_t)) )
         =  ( Π_t  1 / q(x_t) )^(1/N)
```

This is the most physical reading. `1/q(x_t)` is "how surprised the model was at token `t`" — large when it assigned the truth a small probability. Perplexity is the typical such surprise, geometric-averaged so one catastrophic token (a probability near zero) blows it up multiplicatively, exactly as it blows up the loss additively. A geometric mean, not arithmetic, because we averaged in log-space.

---

</details>

## 2. 解读：有效分支因子

下面是需要记住的句子：

> 困惑度为 `K` 的模型，**平均而言，其不确定性等同于在每一步从 `K` 个等概率 token 中均匀选择**。

锚点是均匀分布。如果 `q` 将其概率质量均匀分散在大小为 `V` 的词表上，那么对于每个 token，`q(x_t) = 1/V`，每个 `1/q(x_t) = V`，几何平均恰好是 `V`：

```text
    uniform over V symbols   →   PPL = V        (maximal confusion: no information)
    perfect, confident model →   PPL → 1        (q(truth)=1 everywhere → 0 loss)
    real LLM on English text →   PPL ≈ 3–12     (per-token, BPE vocab ~32k–256k)
```

因此，困惑度将一个 `V` 维分布压缩为 `[1, V]` 尺度上的一个数：*有效*选择数。一个具有 128k token 词表、PPL 为 8 的模型意味着，尽管有 128k 个选项，模型的行为就像在 8 个选项中猜测——它已经利用上下文排除了其他 ~128,000 个选项。

**微型示例。** 词表 `V = 4`：`{a, b, c, d}`。一个两 token 的参考序列 `[a, c]`。模型预测：

```text
    step 1, true=a:   q = (a:0.7, b:0.1, c:0.1, d:0.1)   →  q(a) = 0.7
    step 2, true=c:   q = (a:0.2, b:0.2, c:0.5, d:0.1)   →  q(c) = 0.5
```

计算损失和困惑度：

```text
    NLL_1 = −ln 0.7 = 0.3567
    NLL_2 = −ln 0.5 = 0.6931
    L     = (0.3567 + 0.6931) / 2 = 0.5249 nats
    PPL   = exp(0.5249) = 1.690
    check (geometric mean): ( (1/0.7)(1/0.5) )^(1/2) = (2.857)^(1/2) = 1.690  ✓
```

在 4 符号词表上，PPL `1.69`：模型表现良好——其有效分支远低于均匀分布的 `4` 上限。如果它在两个步骤都预测均匀的 `(0.25, 0.25, 0.25, 0.25)`，每个 `1/q = 4` 和 `PPL = 4` 恰好：它将什么也没学到。语言模型的全部价值就在于它在 `PPL` 和 `V` 之间拉开的距离。

---

## 3. tokenizer 依赖陷阱

这是使大多数实际中的跨模型困惑度比较失效的错误，也是资深工程师在评审中能发现的错误。

困惑度是**按 token 计算的**。但“token”不是文本的属性——它是 *tokenizer* 的属性。同一个英文句子在 GPT-4 的 BPE、Llama 的 SentencePiece 和字节级回退下会变成不同数量的 token。损失按 token 求和并除以 token 计数，因此*相同的底层信息*被切成不同数量的片段，而每个 token 的平均值发生偏移——**即使两个模型在预测实际字节方面同样出色。**

```text
    Text: "internationalization"   (20 characters, 1 word)

    Tokenizer A (coarse BPE):   [internation, alization]        → 2 tokens
    Tokenizer B (fine BPE):     [inter, nation, al, ization]    → 4 tokens
    Tokenizer C (byte-level):   [i,n,t,e,r,...,n]               → 20 tokens

    Suppose each model spends the SAME total 12 nats to predict this word.
        A:  PPL = exp(12 / 2)  = exp(6.0) = 403
        B:  PPL = exp(12 / 4)  = exp(3.0) = 20.1
        C:  PPL = exp(12 / 20) = exp(0.6) = 1.82
```

三个截然不同的困惑度——`403` 对 `20` 对 `1.8`——对应**在相同文本上相同的预测能力**。将文本拆分成更多、更小、更易预测的 token 的模型，按每个 token 的 PPL 看会显得“好”得多，纯粹是 tokenization 造成的假象。因此，比较不同 tokenizer 或词表的模型的 PPL 没有意义。它比较的是*它们自己*的拼图块的平均难度，而不是它们的能力。

**修正方法：用 tokenizer 不变的东西归一化——原始字节。** 语料库上的总负对数似然（以 nats 为单位）与 tokenizer 无关（它是模型对*文本*的总惊讶度，无论你如何切分；链式法则保证联合序列概率是每个 token 概率的乘积，无论怎么分割）。将其除以原始 UTF-8 **字节**数而不是 token 数，并转换为比特。这就是**每字节比特数 (BPB)：**

```text
    BPB  =  total_nats / ( ln 2 · n_bytes )      =  total_bits / n_bytes
```

对于上面的例子，三个模型都在 20 字节上花费了 12 nats（`"internationalization"` 是 20 个 ASCII 字节），所以三个模型得分**相同**，为 `BPB = 12 / (0.6931 × 20) = 0.866` bits/byte。Tokenization 被抵消。BPB（及其同类 bits-per-character 和 per-word perplexity，用于字节计数不方便时，例如 CJK 文本）是排名不同 tokenization 模型的唯一公平方法。如果你看到排行榜用原始 PPL 比较两个不同 tokenizer 的模型，不要相信它；如果它报告 BPB，就可以相信。

---


<details>
<summary>English original</summary>

**2. Interpretation: the effective branching factor**

Here is the sentence to memorize:

> A model with perplexity `K` is **as uncertain, on average, as if it were choosing uniformly among `K` equally-likely tokens** at each step.

The anchor is the uniform distribution. If `q` spreads its mass evenly over a vocabulary of size `V`, then `q(x_t) = 1/V` for every token, every `1/q(x_t) = V`, and the geometric mean is exactly `V`:

```text
    uniform over V symbols   →   PPL = V        (maximal confusion: no information)
    perfect, confident model →   PPL → 1        (q(truth)=1 everywhere → 0 loss)
    real LLM on English text →   PPL ≈ 3–12     (per-token, BPE vocab ~32k–256k)
```

So perplexity collapses a `V`-dimensional distribution to a single number on the scale `[1, V]`: the *effective* number of choices. A 128k-token vocabulary with PPL 8 means the model, despite 128k options, behaves as if it were guessing among 8 — it has used context to rule out the other ~128,000.

**Worked micro-example.** Vocabulary `V = 4`: `{a, b, c, d}`. A two-token reference sequence `[a, c]`. The model predicts:

```text
    step 1, true=a:   q = (a:0.7, b:0.1, c:0.1, d:0.1)   →  q(a) = 0.7
    step 2, true=c:   q = (a:0.2, b:0.2, c:0.5, d:0.1)   →  q(c) = 0.5
```

Compute the loss and perplexity:

```text
    NLL_1 = −ln 0.7 = 0.3567
    NLL_2 = −ln 0.5 = 0.6931
    L     = (0.3567 + 0.6931) / 2 = 0.5249 nats
    PPL   = exp(0.5249) = 1.690
    check (geometric mean): ( (1/0.7)(1/0.5) )^(1/2) = (2.857)^(1/2) = 1.690  ✓
```

PPL `1.69` on a 4-symbol vocabulary: the model is performing well — its effective branching is far below the uniform ceiling of `4`. If it had predicted uniform `(0.25, 0.25, 0.25, 0.25)` at both steps, every `1/q = 4` and `PPL = 4` exactly: it would have learned nothing. The whole value of a language model is the distance it opens between `PPL` and `V`.

---

**3. The tokenizer-dependence trap**

This is the error that voids most cross-model perplexity comparisons in the wild, and the one a senior engineer catches in review.

Perplexity is **per token**. But "token" is not a property of the text — it is a property of the *tokenizer*. The same English sentence becomes a different number of tokens under GPT-4's BPE, Llama's SentencePiece, and a byte-level fallback. The loss is summed over tokens and divided by the token count, so the *same underlying information* gets sliced into a different number of pieces, and the per-token average shifts — **even if the two models are equally good at predicting the actual bytes.**

```text
    Text: "internationalization"   (20 characters, 1 word)

    Tokenizer A (coarse BPE):   [internation, alization]        → 2 tokens
    Tokenizer B (fine BPE):     [inter, nation, al, ization]    → 4 tokens
    Tokenizer C (byte-level):   [i,n,t,e,r,...,n]               → 20 tokens

    Suppose each model spends the SAME total 12 nats to predict this word.
        A:  PPL = exp(12 / 2)  = exp(6.0) = 403
        B:  PPL = exp(12 / 4)  = exp(3.0) = 20.1
        C:  PPL = exp(12 / 20) = exp(0.6) = 1.82
```

Three wildly different perplexities — `403` vs `20` vs `1.8` — for **identical predictive skill on identical text**. The model that splits text into more, smaller, easier-to-predict tokens looks dramatically "better" by per-token PPL, purely as a tokenization artifact. Comparing PPL across models with different tokenizers or vocabularies is therefore meaningless. It is comparing the average difficulty of *their own* puzzle pieces, not their skill.

**The fix: normalize by something tokenizer-invariant — raw bytes.** The total negative log-likelihood (in nats) over the corpus is tokenizer-independent (it is the model's total surprise at the *text*, however you slice it; the chain rule guarantees the joint sequence probability is the product of the per-token ones regardless of the split). Divide it by the number of raw UTF-8 **bytes** instead of tokens, and convert to bits. That is **bits-per-byte (BPB):**

```text
    BPB  =  total_nats / ( ln 2 · n_bytes )      =  total_bits / n_bytes
```

For the example above, all three models spent 12 nats on 20 bytes (`"internationalization"` is 20 ASCII bytes), so all three score the **same** `BPB = 12 / (0.6931 × 20) = 0.866` bits/byte. Tokenization cancels. BPB (and its cousins bits-per-character and per-word perplexity, used when byte counts are awkward, e.g. CJK text) is the only fair way to rank models that tokenize differently. If you ever see a leaderboard comparing two different-tokenizer models by raw PPL, distrust it; if it reports BPB, trust it.

---

</details>

## 4. 长文档的滑动窗口困惑度

第二个陷阱与 *上下文* 有关，一旦你的评估文档长于模型的上下文窗口，它立刻就会发作。

上下文长度为 `L`（比如 4096）的模型无法在一次前向传播中吞下一篇 10,000 token 的 WikiText 文档。偷懒的修法——把文档切成 `L` 的 **不重叠** 块，逐块独立打分——是错的，而且错的方向会 *美化* 一个更差的方案，同时 *抬高* 你报告出来的数字。第一块之后的每一块都是冷启动：其开头的 token 是在 **几乎没有或完全没有左侧上下文** 的情况下预测的，因为本应排在它们前面的上下文留在上一块里，已经被丢掉了。上下文更少时预测出的 token 具有更高的 NLL，因此不重叠分块会系统性地 **高估** 困惑度。

```text
    Document (10k tokens), context L = 4096:

    NON-OVERLAPPING (wrong):
      chunk 1: [t0 .................. t4095]   t0 has 0 ctx, t4095 has full ctx
      chunk 2: [t4096 ............... t8191]   t4096 has 0 ctx AGAIN  ← inflates PPL
      chunk 3: [t8192 ............... t9999]   t8192 has 0 ctx AGAIN
                ↑ every chunk boundary re-pays the "cold start" penalty

    STRIDED SLIDING WINDOW (HF method), stride s < L:
      window 1: [t0 ........................ t4095]   score ALL (first window)
      window 2: [t_s ....................... t_{s+4095}]
                 └ first (L − s) tokens: CONTEXT ONLY, masked with -100
                                          last  s tokens: SCORED with full ctx
      window 3: slide by s again ...
                → every scored token (after the first window) sees a FULL L of context
```

HuggingFace 的 strided 方案以步长 `s < L` 滑动宽度为 `L` 的窗口。在每个窗口中，它只预测 **最后 `s` 个 token**（这些 token 如今拥有完整的 `L` 左侧上下文），并 **把其余部分用 `-100` 掩掉**，让它们只提供上下文、不贡献 loss。重叠 `L − s` 就是让每个被打分的 token 都拿到完整上下文所要付出的算力代价。步长越小 → 重叠越多 → 越准确（越接近用 `L−1` 个 token 的上下文给每个 token 打分的理想情形）→ 前向传播次数越多。`s = L` 会退化成错误的非重叠方法；`s = 1` 则是精确但昂贵的极限。通常人们会选 `s = L/2` 或 `s = 512`。

下面是 WikiText-2 上标准的 HF 风格循环：

```python
import torch
from datasets import load_dataset
from transformers import AutoModelForCausalLM, AutoTokenizer

device = "cuda"
model_id = "your/model"
model = AutoModelForCausalLM.from_pretrained(model_id, torch_dtype=torch.float16).to(device)
tok   = AutoTokenizer.from_pretrained(model_id)

# WikiText-2: concatenate the test split into one long token stream.
test = load_dataset("wikitext", "wikitext-2-raw-v1", split="test")
enc  = tok("\n\n".join(test["text"]), return_tensors="pt")

max_len = model.config.max_position_embeddings   # the model's context L
stride  = 512                                    # s < L : the sliding stride
seq_len = enc.input_ids.size(1)

nll_sum, n_tokens, prev_end = 0.0, 0, 0
for begin in range(0, seq_len, stride):
    end      = min(begin + max_len, seq_len)     # window of width <= L
    trg_len  = end - prev_end                    # the NEW tokens to actually score
    ids      = enc.input_ids[:, begin:end].to(device)

    targets  = ids.clone()
    targets[:, :-trg_len] = -100                 # mask context tokens: no loss, just ctx

    with torch.no_grad():
        out = model(ids, labels=targets)
        # HF returns MEAN loss over the (trg_len - 1) scored positions;
        # multiply back up to a SUM so windows of unequal size aggregate correctly.
        num_scored = trg_len - 1
        nll_sum   += out.loss.float() * num_scored
        n_tokens  += num_scored

    prev_end = end
    if end == seq_len:
        break

avg_nll = nll_sum / n_tokens
ppl     = torch.exp(avg_nll)
print(f"WikiText-2  PPL = {ppl.item():.3f}   (L={max_len}, stride={stride})")
```

有两个细节区分「正确」与「几乎正确」：(1) 必须累加按被打分 token 数加权的 NLL **总和**，最后只除一次——当最后一个窗口更短时，对各窗口的平均 loss 再求平均是错的；(2) 差一（`trg_len - 1`）是因为 causal LM 的最后一个位置没有下一个 token 作为目标。两者任一弄错，你的 PPL 都会悄悄偏掉几个百分点——足以让量化的结论翻转。

---


<details>
<summary>English original</summary>

**4. Sliding-window perplexity for long documents**

The second trap is about *context*, and it bites the moment your evaluation document is longer than the model's context window.

A model with context length `L` (say 4096) cannot ingest a 10,000-token WikiText document in one forward pass. The lazy fix — chop the document into **non-overlapping** chunks of `L` and score each independently — is wrong, and wrong in a direction that *flatters* a worse setup while *inflating* your reported number. Every chunk after the first starts cold: its first tokens are predicted with **little or no left context**, because the context that should have preceded them lives in the previous chunk and was thrown away. Tokens predicted with less context have higher NLL, so non-overlapping chunking systematically **over-estimates** perplexity.

```text
    Document (10k tokens), context L = 4096:

    NON-OVERLAPPING (wrong):
      chunk 1: [t0 .................. t4095]   t0 has 0 ctx, t4095 has full ctx
      chunk 2: [t4096 ............... t8191]   t4096 has 0 ctx AGAIN  ← inflates PPL
      chunk 3: [t8192 ............... t9999]   t8192 has 0 ctx AGAIN
                ↑ every chunk boundary re-pays the "cold start" penalty

    STRIDED SLIDING WINDOW (HF method), stride s < L:
      window 1: [t0 ........................ t4095]   score ALL (first window)
      window 2: [t_s ....................... t_{s+4095}]
                 └ first (L − s) tokens: CONTEXT ONLY, masked with -100
                                          last  s tokens: SCORED with full ctx
      window 3: slide by s again ...
                → every scored token (after the first window) sees a FULL L of context
```

The HuggingFace strided approach slides a window of width `L` by a stride `s < L`. In each window it predicts only the **last `s` tokens** (those that now enjoy a full `L` of left context) and **masks the rest with `-100`** so they contribute context but no loss. Overlap `L − s` is the price you pay in compute for giving every scored token full context. Smaller stride → more overlap → more accurate (closer to the ideal of scoring every token with `L−1` tokens of context) → more forward passes. `s = L` degenerates back to the wrong non-overlapping method; `s = 1` is the exact-but-expensive limit. People typically pick `s = L/2` or `s = 512`.

Here is the canonical HF-style loop over WikiText-2:

```python
import torch
from datasets import load_dataset
from transformers import AutoModelForCausalLM, AutoTokenizer

device = "cuda"
model_id = "your/model"
model = AutoModelForCausalLM.from_pretrained(model_id, torch_dtype=torch.float16).to(device)
tok   = AutoTokenizer.from_pretrained(model_id)

# WikiText-2: concatenate the test split into one long token stream.
test = load_dataset("wikitext", "wikitext-2-raw-v1", split="test")
enc  = tok("\n\n".join(test["text"]), return_tensors="pt")

max_len = model.config.max_position_embeddings   # the model's context L
stride  = 512                                    # s < L : the sliding stride
seq_len = enc.input_ids.size(1)

nll_sum, n_tokens, prev_end = 0.0, 0, 0
for begin in range(0, seq_len, stride):
    end      = min(begin + max_len, seq_len)     # window of width <= L
    trg_len  = end - prev_end                    # the NEW tokens to actually score
    ids      = enc.input_ids[:, begin:end].to(device)

    targets  = ids.clone()
    targets[:, :-trg_len] = -100                 # mask context tokens: no loss, just ctx

    with torch.no_grad():
        out = model(ids, labels=targets)
        # HF returns MEAN loss over the (trg_len - 1) scored positions;
        # multiply back up to a SUM so windows of unequal size aggregate correctly.
        num_scored = trg_len - 1
        nll_sum   += out.loss.float() * num_scored
        n_tokens  += num_scored

    prev_end = end
    if end == seq_len:
        break

avg_nll = nll_sum / n_tokens
ppl     = torch.exp(avg_nll)
print(f"WikiText-2  PPL = {ppl.item():.3f}   (L={max_len}, stride={stride})")
```

Two details that separate correct from almost-correct: (1) you must accumulate a **sum** of NLL weighted by the number of scored tokens and divide once at the end — averaging the per-window mean losses is wrong when the final window is shorter; (2) the off-by-one (`trg_len - 1`) is because the last position in a causal LM has no next-token target. Get either wrong and your PPL is quietly off by a few percent — enough to flip a quantization verdict.

---

</details>

## 5. 标准做法与可复现性

困惑度只是一个数字；它只有在与另一个用*相同方式*算出的数字相比时才具可比性。社区已经收敛到一小组基线上，好让 "PPL 5.68" 具备明确含义：

| 语料库 | 说明 | 典型用途 |
|---|---|---|
| **WikiText-2** | 约 2M tokens，经整理的 Wikipedia（raw + tokenized 变体） | 事实上的快速 sanity 基线；`llama-perplexity` 默认接近的取值 |
| **WikiText-103** | 约 103M tokens，同一来源，规模更大 | 更长上下文 / 更低方差的 LM 评估 |
| **C4** | Colossal Clean Crawled Corpus，web 规模 | 预训练分布的 PPL，对 Wikipedia 过拟合更少 |
| **The Pile / 领域数据集** | 代码、书籍、论文 | 领域迁移检查（量化可能在代码上退化，但在散文上不会） |

除非在给出数字的同时一并报告下列内容，否则困惑度结果**不可复现**：

```text
    □ exact corpus + split + version   (wikitext-2-raw-v1 test ≠ wikitext-2-v1)
    □ context length L                 (PPL drops as L grows: more context = less surprise)
    □ stride s                         (smaller stride → slightly lower PPL)
    □ dtype / precision                (FP16 vs BF16 vs the quant under test)
    □ tokenizer + whether raw or pre-tokenized text was used
    □ how documents were joined (newline glue) and whether BOS was prepended
```

最常见的可复现性失误，是拿你的 PPL 与论文中的 PPL 相比时用了不同的 `L` 或 stride：更长的上下文与更小的 stride 都会*降低* PPL，于是设置上的差异就被伪装成了模型差异。把这六个旋钮全部固定，否则这种比较就是传闻。

---


<details>
<summary>English original</summary>

**5. Standard practice and reproducibility**

Perplexity is only a number; it is comparable only against another number computed *the same way*. The community has converged on a small set of baselines so that "PPL 5.68" means something:

| Corpus | What it is | Typical use |
|---|---|---|
| **WikiText-2** | ~2M tokens, curated Wikipedia (raw + tokenized variants) | the fast de-facto sanity baseline; what `llama-perplexity` defaults near |
| **WikiText-103** | ~103M tokens, same source, larger | longer-context / lower-variance LM evaluation |
| **C4** | Colossal Clean Crawled Corpus, web-scale | pretraining-distribution PPL, less Wikipedia-overfit |
| **The Pile / domain sets** | code, books, papers | domain-shift checks (a quant can regress on code but not prose) |

A perplexity result is **not reproducible** unless you report, alongside the number:

```text
    □ exact corpus + split + version   (wikitext-2-raw-v1 test ≠ wikitext-2-v1)
    □ context length L                 (PPL drops as L grows: more context = less surprise)
    □ stride s                         (smaller stride → slightly lower PPL)
    □ dtype / precision                (FP16 vs BF16 vs the quant under test)
    □ tokenizer + whether raw or pre-tokenized text was used
    □ how documents were joined (newline glue) and whether BOS was prepended
```

The single most common reproducibility failure is comparing your PPL to a paper's while using a different `L` or stride: longer context and smaller stride both *lower* PPL, so a setup difference masquerades as a model difference. Pin all six knobs, or the comparison is folklore.

---

</details>

## 6. 困惑度：量化的标准度量

这正是困惑度对系统工程师的价值所在。当你量化一个模型——FP16 → INT4——你需要一个快速、自动的答案来回答「它坏了吗？」。困惑度是你最先拿起的仪器，因为 recipe 简单，信号真实：

```text
    1. Compute PPL_fp16 on a fixed corpus / L / stride  (the reference).
    2. Quantize → compute PPL_int4 on the SAME corpus / L / stride.
    3. Report the DELTA, absolute and relative:
           ΔPPL      = PPL_int4 − PPL_fp16
           PPL ratio = PPL_int4 / PPL_fp16
```

社区沿用的经验法则：**好的 4-bit 量化相对 FP16 增加的困惑度远低于 1–1.5%**。落在 +0.5–1.5% 的 Q4_K_M 或 AWQ INT4 属于可出厂水平；增加 5%、10%，或「爆炸」到几百 PPL 的量化，意味着 scale 损坏、outlier channel 处理错误，或 group size 不当——而你在一次前向传播中就抓到了它，而不是在生产环境里。

GGUF 世界里事实上的工具是 **`llama-perplexity`**（`llama.cpp` 二进制，原为 `perplexity`）：

```text
    # FP16 reference
    ./llama-perplexity -m model-f16.gguf  -f wiki.test.raw -c 4096
        →  PPL = 5.6789  (final), printed with a running estimate + stderr

    # INT4 candidate, IDENTICAL corpus and context
    ./llama-perplexity -m model-q4_k_m.gguf -f wiki.test.raw -c 4096
        →  PPL = 5.7361
        ⇒  ΔPPL = +0.057  ,  ratio = 1.0101   (+1.01%)  →  acceptable, ship-track
```

> **硬件视角：** 困惑度是量化 GGUF 或任何边缘部署的**一线验收测试**，原因在于成本。它是对固定语料库的一次前向传播——几百到几千条序列——相对于跑完整的下游 benchmark 套件（MMLU、GSM8K、HumanEval），这是*廉价*的；后者每一个都是大量生成，还配套采样与评分 harness。在 Jetson 或笔记本上，你可以在几分钟内给 WikiText-2 打分，得到一个明确的 go/no-go：**ΔPPL 决定你的 INT4 构建能否进入 benchmark 队列。** 如果量化增加了 8% 困惑度，你不会浪费 GPU-hours 去 benchmark 它；先把量化修好。PPL 是集成测试之前跑的冒烟测试。

**但困惑度只是输出质量的*粗略*代理指标，资深工程师会明说出来。** 两个*困惑度相同*的量化在生成时可能表现不同：PPL 是对语料库上 teacher-forced 下一 token 惊异度的平均，因此它看不见错误落在*哪里*。一个量化可以保住平均困惑度，却恰恰在决定代码能否跑起来、数学是否正确的高风险低熵 token（右花括号、正确的数字、函数名）上退化——而这些正是用户会注意到的 token。困惑度也没有说明模型的*最高* token 是否仍与 FP16 模型的最高 token 一致，而后者才是贪心解码实际输出的东西。更严谨的仪器——量化与 FP16 下一 token 分布之间的 **KL 散度**，以及 **top-token 一致率**——与感知质量的相关性好得多；用全部四个数字（PPL ratio、mean KLD、top-1 一致率、token 概率 RMS）正确地给量化打分，是 **[Lecture 05](Lecture-05.md)** 的全部主题。把这里的 PPL 视为必要的第一道闸门，而非最终裁决。

关于这套量化评分工作流如何接入更广泛的模型压缩流水线——剪枝、蒸馏与量化作为一次协同行动——参见 [Practical Machine Learning (CS329P) — Lecture 10](../../../阶段3-人工智能/04-智能体AI与ML工程/02-ML工程与MLOps/01-实用机器学习CS329P/Lecture-10.md) 中的压缩部分，那里*使用*了这些指标；而本讲才是它们被推导出来的地方。

> **2026 更新：** 实践中有两个变化。第一，困惑度越来越多地以 **bits-per-byte** 而非原始的 per-token PPL 报告，正是为了让 tokenizer 不同的模型（现在已是常态，词表已分化到 128k–256k）至少能被排序——跨 tokenizer 的原始 PPL 比较如今在评审中被视为危险信号。第二，社区共识已经固化：**仅靠 PPL 不足以做量化选型**：2026 年 1 月关于量化*格式*比标称 bit-width 更重要的发现，进一步印证了内在指标是必要的但不充分；如今，KL 散度加 top-token 一致率（Lecture 05）是区分困惑度相同的候选量化时的首选判别指标。PPL 把关；KLD 决断。

---


<details>
<summary>English original</summary>

**6. Perplexity as the canonical quantization metric**

This is where perplexity earns its keep for a systems engineer. When you quantize a model — FP16 → INT4 — you need a fast, automatic answer to "did that break it?" Perplexity is the first instrument you reach for, because the recipe is trivial and the signal is real:

```text
    1. Compute PPL_fp16 on a fixed corpus / L / stride  (the reference).
    2. Quantize → compute PPL_int4 on the SAME corpus / L / stride.
    3. Report the DELTA, absolute and relative:
           ΔPPL      = PPL_int4 − PPL_fp16
           PPL ratio = PPL_int4 / PPL_fp16
```

The rule of thumb the community uses: a **good 4-bit quant adds well under 1–2% perplexity** over FP16. A Q4_K_M or AWQ INT4 that lands at +0.5–1.5% is shipping-grade; a quant that adds 5%, 10%, or "blows up" to a PPL of hundreds has a broken scale, a mis-handled outlier channel, or a bad group size — and you caught it in one forward pass instead of in production.

The de-facto tool in the GGUF world is **`llama-perplexity`** (the `llama.cpp` binary, formerly `perplexity`):

```text
    # FP16 reference
    ./llama-perplexity -m model-f16.gguf  -f wiki.test.raw -c 4096
        →  PPL = 5.6789  (final), printed with a running estimate + stderr

    # INT4 candidate, IDENTICAL corpus and context
    ./llama-perplexity -m model-q4_k_m.gguf -f wiki.test.raw -c 4096
        →  PPL = 5.7361
        ⇒  ΔPPL = +0.057  ,  ratio = 1.0101   (+1.01%)  →  acceptable, ship-track
```

> **Hardware lens:** perplexity is the **first-line acceptance test** for a quantized GGUF or any edge deployment, and the reason is cost. It is a single forward pass over a fixed corpus — a few hundred to a few thousand sequences — which is *cheap* relative to running a full downstream benchmark suite (MMLU, GSM8K, HumanEval), each of which is many generations with sampling and scoring harnesses. On a Jetson or a laptop you can score WikiText-2 in minutes and get a hard go/no-go: the **ΔPPL gates whether your INT4 build even enters the benchmark queue.** If the quant added 8% perplexity, you do not waste GPU-hours benchmarking it; you fix the quantization first. PPL is the smoke test that runs before the integration tests.

**But perplexity is a *rough* proxy for output quality, and a senior engineer says so out loud.** Two quants with *equal* perplexity can behave differently in generation: PPL is an average over a corpus of teacher-forced next-token surprise, so it is blind to *where* the errors land. A quant can preserve average perplexity while degrading specifically on the high-stakes low-entropy tokens (the closing brace, the correct digit, the function name) that determine whether code runs or math is right — precisely the tokens a user notices. Perplexity also says nothing about whether the model's *top* token still matches the FP16 model's top token, which is what greedy decoding actually emits. The rigorous instruments — **KL divergence** between the quant and FP16 next-token distributions, and **top-token agreement** — correlate far better with perceived quality, and grading a quant properly with all four numbers (PPL ratio, mean KLD, top-1 agreement, token-probability RMS) is the entire subject of **[Lecture 05](Lecture-05.md)**. Treat PPL here as the necessary first gate, not the verdict.

For where this quantization-grading workflow plugs into a broader model-compression pipeline — pruning, distillation, and quantization as a coordinated campaign — see the compression treatment in [Practical Machine Learning (CS329P) — Lecture 10](../../../阶段3-人工智能/04-智能体AI与ML工程/02-ML工程与MLOps/01-实用机器学习CS329P/Lecture-10.md), which *uses* these metrics; this lecture is where they are derived.

> **2026 update:** two shifts in practice. First, perplexity is increasingly reported as **bits-per-byte** rather than raw per-token PPL precisely so that models with different tokenizers (the norm now, as vocabularies have diverged to 128k–256k) can be ranked at all — raw cross-tokenizer PPL comparisons are now treated as a red flag in review. Second, the community consensus has hardened that **PPL alone is insufficient for quantization selection**: the January 2026 finding that quantization *format* matters more than nominal bit-width reinforced that intrinsic metrics are necessary but not sufficient, and KL-divergence plus top-token agreement (Lecture 05) are now the preferred discriminators between candidate quants of equal perplexity. PPL gates; KLD decides.

---

</details>

## 关键要点

- **困惑度是损失的 `exp`** —— `PPL = exp(mean NLL) = exp(H(p, q)) = 2^(bits per token)` —— 同时也是 **`1/q(x_t)` 的几何平均**。除第 02 讲外没有新内容；只是一种新的*读法*。
- 它读作**有效分支因子**：在 `V` 上均匀分布 → `PPL = V`；一个自信且正确的模型 → `PPL → 1`；PPL 与 `V` 之间的差距，正是模型学到的东西。
- **tokenizer 陷阱：** per-token PPL 在不同 tokenizer 之间不可比；token 越多、越小，这个数字越好看。解决办法是报告 **bits-per-byte**，`BPB = total_nats / (ln2 · n_bytes)`，它抵消了 tokenization 的影响。
- **长文档陷阱：** 不重叠分块会在每个边界重复支付冷启动代价，从而抬高 PPL；**HF strided window**（步长 `s < L`，只对最后 `L−s` 个 token 计分，其余用 `-100` 掩掉）让每个被计分的 token 都拥有完整上下文。
- **可复现性：** 没有 corpus/split/version、上下文 `L`、步长 `s`、dtype 和 tokenizer，PPL 数字毫无意义。要把它们全部固定下来。
- **量化门禁：** 报告 INT4 相对 FP16 的 **ΔPPL / PPL 比值**（好的 4-bit quant：<1–2%）；`llama-perplexity` 是 GGUF 工具。但 PPL 只是**粗略**的代理指标 —— PPL 相同的量化版本表现也会不同 —— 因此最终由 KL-divergence 和 top-token agreement（第 05 讲）来定夺。

---

## 截至

2026-06。其数学部分（`PPL = exp(H(p,q))`、BPB 归一化、滑窗修正）已经定型，且不随时间改变。工具锁定：`llama.cpp` 的 perplexity binary 现在叫 `llama-perplexity`；HuggingFace 的 strided-PPL recipe 是长文档方法的参考。实践锁定：随着词表扩大到 128k–256k，bits-per-byte 成为跨 tokenizer 报告的通行做法；社区共识（由 2026 年 1 月的量化格式发现进一步强化）是：PPL 是必要的第一道门禁，但**不足以**用于量化选型 —— KL-divergence 和 top-token agreement（第 05 讲）才是更受青睐的判别指标。WikiText-2/103 和 C4 仍是事实上的基线。


<details>
<summary>English original</summary>

**Key takeaways**

- **Perplexity is `exp` of the loss** — `PPL = exp(mean NLL) = exp(H(p, q)) = 2^(bits per token)` — and also the **geometric mean of `1/q(x_t)`**. No new content beyond Lecture 02; a new *reading*.
- It reads as an **effective branching factor**: uniform over `V` → `PPL = V`; a confident correct model → `PPL → 1`; the gap between PPL and `V` is exactly what the model learned.
- **Tokenizer trap:** per-token PPL is not comparable across tokenizers; more, smaller tokens flatter the number. Fix by reporting **bits-per-byte**, `BPB = total_nats / (ln2 · n_bytes)`, which cancels tokenization.
- **Long-document trap:** non-overlapping chunking inflates PPL by re-paying a cold-start penalty at every boundary; the **HF strided window** (stride `s < L`, score only the last `L−s` tokens, mask the rest with `-100`) gives every scored token full context.
- **Reproducibility:** a PPL number is meaningless without corpus/split/version, context `L`, stride `s`, dtype, and tokenizer. Pin all of them.
- **Quantization gate:** report **ΔPPL / PPL ratio** of INT4 vs FP16 (good 4-bit quant: <1–2%); `llama-perplexity` is the GGUF tool. But PPL is a **rough** proxy — equal-PPL quants differ — so KL-divergence and top-token agreement (Lecture 05) make the final call.

---

**Current as of**

2026-06. The mathematics (`PPL = exp(H(p,q))`, the BPB normalization, the sliding-window correction) is settled and timeless. Tooling pins: `llama.cpp`'s perplexity binary is now `llama-perplexity`; HuggingFace's strided-PPL recipe is the reference long-document method. Practice pins: bits-per-byte is the cross-tokenizer reporting norm as vocabularies diverged to 128k–256k; community consensus (reinforced by the Jan-2026 quantization-format finding) is that PPL is a necessary first gate but **not** sufficient for quant selection — KL-divergence and top-token agreement (Lecture 05) are the preferred discriminators. WikiText-2/103 and C4 remain the de-facto baselines.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/Logprobs, Perplexity and KL Divergence/Lecture-03.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/Logprobs%2C%20Perplexity%20and%20KL%20Divergence/Lecture-03.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
