---
title: Transformer 基础 — attention、self-attention 与完整 block
description: Transformer 基础 — attention、self-attention 与完整 block
---

# Transformer 基础 — attention、self-attention 与完整 block

## 概述

这是关于 **Transformer** 架构的基础课程。它从 attention 所解决的问题出发，逐步构建缩放点积 attention 运算，推广到 self-attention 与 multi-head attention，再组装出完整的 Transformer block（attention + FFN + residual + norm）以及三种经典配置（encoder-only、decoder-only、encoder–decoder）。

请在**阶段 5** 推理课程**之前**阅读本文。那些课程假定你能读懂张量形状，并知道 `Q, K, V` 中哪个字母对应哪个投影。本文正好教你这个，只讲你真正需要的数学，并明确指出常见的误解。

**配套可视化：** 阅读时，在一个标签页中打开 [LLM Visualization (bbycroft.net/llm)](https://bbycroft.net/llm)。它把一个小 GPT 的每个张量都在 3D 中走一遍前向传播 —— embedding、Q/K/V 投影、attention 分数网格、softmax、按 value 加权的求和、残差相加、layer norm、多层感知机，以及输出投影。当本文说"Q 是一个形状为 (T, d_k) 的 2D 张量"时，那个页面能让你真切看到各个单元格在移动。每当某个形状不再有直观意义时，就去用它。

读完后，你应该能够：

* 解释 attention 为何存在 —— 它从更早的序列模型中去掉了哪个瓶颈。
* 从"把一个 query 与许多 key 比较"推导出缩放点积 attention 公式。
* 用 30 行 PyTorch 实现 self-attention。
* 勾画出 multi-head attention，并解释每个 head 可以专门负责什么。
* 在流水线中正确放置 causal mask 与 padding mask。
* 从 block 图中辨认 self-attention 与 cross-attention。
* 读懂 Transformer block 的完整前向传播，并将其对应到实际的生产模型（BERT、GPT、T5、Llama、Qwen）。

---

## 1. 为什么 attention 存在

### 1.1 attention 去除的瓶颈

在 Transformer 之前，主流的序列模型是**循环网络** —— RNN、LSTM、GRU。它们逐 token 处理序列，将单个**隐藏状态**向量向前串联传递：

```
hidden_0 ─► [RNN cell] ─► hidden_1 ─► [RNN cell] ─► hidden_2 ─► ... ─► hidden_T
              │                          │                          │
            x_0                         x_1                         x_T
```

每个 token 的信息都必须打包进同一个**固定大小的隐藏状态**，然后传递到下一步。当模型走到 `x_T` 时，关于 `x_0` 的信息必须挺过 `T` 轮覆写。

这就是**固定上下文瓶颈**。对短序列而言它表现良好。对长序列（段落翻译、摘要、代码）则彻底失效 —— 早期 token 相对于晚期预测的梯度消失，模型真的会忘掉序列的开头。

LSTM 和 GRU 加入了**门控**，有所缓解，但并未消除这个问题。这些架构仍然把所有信息都汇入单个隐藏向量。

### 1.2 attention 的洞见

!!! abstract "核心思想"
    **并非每个 token 对每个决策都同等重要。**

对于"She sat by the river bank"中的单词 **bank**，单词 **river** 是整句话里信息量最大的 token。对于"I deposited money at the bank"中同一个 **bank**，则是 **money** 和 **deposited**。所谓"正确"的上下文是**依赖输入的**。

attention 把这一点具象化：在每个输出位置，模型产生**对每个输入位置表示的加权组合**，其中权重由数据动态计算得出。

```
output_i  =  Σ_j  weight(i, j) · value(j)

where  weight(i, j)  depends on  (query at position i,  key at position j)
```

没有隐藏状态瓶颈。没有信息必须挺过一连串覆写。每个位置都能直接从其他任意位置取用信息。

### 1.3 attention *不是*什么

!!! warning "attention 不是的三件事"
    * **不是对模型决策的完整解释。** attention 权重是有用的线索 ——"模型把更多权重放在了 token X 上"—— 但决策还要经过下游的线性投影、FFN 层、残差相加，以及并行的许多其他 head。把 attention 图当作证据，而不是证明。
    * **不是检索。** 检索系统会挑出最相关的那一个 key 并返回它的 value。attention 返回的是**软性的加权平均**。即便得分最低的 token 也贡献了一丝。
    * **不是免费的。** 对序列长度 `N` 计算 `Q @ K^T`，在时间和内存上都是 `O(N²)`。这就是长上下文推理困难的原因 —— 也是滑动窗口、稀疏 attention、FlashAttention 和 paged attention 全都存在的原因。

---

## 2. Query、Key 与 Value

这是 Transformer 中最值得放慢节奏细读的部分。一旦 **Q/K/V** 想通了，架构的其余部分就都是机械的了。在它想通之前，每张图看起来都像一锅字母汤。


<details>
<summary>English original</summary>

**Transformer Fundamentals — Attention, Self-Attention, and the Full Block**

**Overview**

This is the foundational lecture on the **transformer** architecture. It starts from the problem that attention solves, builds up the scaled dot-product attention operation, generalizes to self-attention and multi-head attention, then assembles the full transformer block (attention + FFN + residual + norm) and the three canonical configurations (encoder-only, decoder-only, encoder–decoder).

Read this **before** the Phase 5 inference lectures. Those lectures assume you can read a tensor shape and know which projection corresponds to which letter in `Q, K, V`. This lecture teaches you that, with the math you actually need and with the common misconceptions called out explicitly.

**Companion visualization:** as you read, keep [LLM Visualization (bbycroft.net/llm)](https://bbycroft.net/llm) open in a tab. It walks every tensor of a small GPT through the forward pass in 3D — embeddings, Q/K/V projections, the attention scores grid, softmax, the value-weighted sum, the residual add, layer norm, MLP, and the output projection. When this lecture says "Q is a 2D tensor of shape (T, d_k)," that page lets you literally see the cells move. Use it whenever a shape stops making intuitive sense.

By the end you should be able to:

* Explain why attention exists at all — what bottleneck it removes from older sequence models.
* Derive the scaled dot-product attention formula from "compare a query to many keys."
* Implement self-attention in 30 lines of PyTorch.
* Sketch multi-head attention and explain what each head can specialize in.
* Place a causal mask and a padding mask correctly in the pipeline.
* Identify self-attention vs cross-attention from a block diagram.
* Read the full forward pass of a transformer block and match it to actual production models (BERT, GPT, T5, Llama, Qwen).

---

**1. Why Attention Exists**

**1.1 The bottleneck attention removes**

Before transformers, the dominant sequence models were **recurrent networks** — RNNs, LSTMs, GRUs. They processed sequences token by token, threading a single **hidden state** vector forward:

```
hidden_0 ─► [RNN cell] ─► hidden_1 ─► [RNN cell] ─► hidden_2 ─► ... ─► hidden_T
              │                          │                          │
            x_0                         x_1                         x_T
```

Every token's information had to be packed into the same **fixed-size hidden state**, then passed to the next step. By the time the model reached `x_T`, information about `x_0` had to survive `T` rounds of overwriting.

This is the **fixed-context bottleneck**. For short sequences it works fine. For long sequences (translation of a paragraph, summarization, code) it falls apart — the gradient of the early tokens with respect to a late prediction vanishes, and the model literally forgets the start of the sequence.

LSTMs and GRUs added **gates** that helped but didn't eliminate the problem. The architectures still funneled everything through a single hidden vector.

**1.2 The attention insight**

!!! abstract "The core idea"
    **Not every token matters equally for every decision.**

For the word **bank** in "She sat by the river bank," the word **river** is the single most informative token in the sentence. For the same **bank** in "I deposited money at the bank," it's **money** and **deposited**. The "right" context is **input-dependent**.

Attention makes this concrete: at each output position, the model produces a **weighted combination of every input position's representation**, where the weights are computed dynamically from the data.

```
output_i  =  Σ_j  weight(i, j) · value(j)

where  weight(i, j)  depends on  (query at position i,  key at position j)
```

No hidden-state bottleneck. No information has to survive a chain of overwrites. Every position can pull from every other position directly.

**1.3 What attention is *not***

!!! warning "Three things attention is not"
    * **Not a full explanation of the model's decision.** Attention weights are useful clues — "the model put more weight on token X" — but the decision also goes through downstream linear projections, FFN layers, residual additions, and many other heads in parallel. Treat attention maps as evidence, not proof.
    * **Not retrieval.** A retrieval system would pick the single most relevant key and return its value. Attention returns a **soft, weighted average**. Even the lowest-scoring token contributes a sliver.
    * **Not free.** Computing `Q @ K^T` for sequence length `N` is `O(N²)` in both time and memory. This is why long-context inference is hard — and why sliding window, sparse attention, FlashAttention, and paged attention all exist.

---

**2. Queries, Keys, and Values**

This is the part of the transformer most worth slowing down for. Once **Q/K/V** clicks, the rest of the architecture is mechanical. Until it clicks, every diagram looks like alphabet soup.

</details>

### 2.1 三种角色

| 组件 | 角色 | 一句话直觉 |
|---|---|---|
| **Query (Q)** | 当前位置*想找什么* | “我需要什么信息？” |
| **Key (K)** | 每个位置*能提供什么用于匹配* | “我包含哪一类信息？” |
| **Value (V)** | 匹配发生时所传递的实际*内容* | “这是我的信息。” |

该机制分三步：

1. 把 query `q` 与每个 key `k_j` 比较。比较产生一个 **score**。
2. 用 softmax 把分数转换成概率分布（权重非负且总和为 1）。
3. 用这些权重对 values `v_j` 做加权求和。这个加权求和**就是** attention 输出。

### 2.2 图书馆类比

想象你带着一个问题走进图书馆。这个问题就是你的 **query**。图书馆的书架上有成千上万本书。每本书的书脊上印着**标题**，封皮里面有**内容**。

* 你扫视书脊（即 **keys**），判断哪些标题与你的 query 最相关。
* 你还没有读内容（即 **values**）——只是拿你的问题与书脊作比较。
* 根据每个标题与你问题的匹配程度，决定对每本书投入多少注意力。
* 然后你取下相关的书，按感兴趣的程度翻阅其内容。标题完全匹配的书会被完整读完；标题勉强相关的只会被扫一眼；不相关的则被忽略。
* 你带走的“答案”，是所有读过的书按相关性加权后的**混合摘要**。

有三个关键性质值得注意：

1. **标题和内容是两回事。** 一本讲 *river ecology* 的书，标题可能是 "Riparian Systems"——标题匹配（"river bank?" → "riparian!"）与内容阅读是两种不同的操作。因此：key ≠ value，即便它们描述的是同一事物。
2. **你的 query 只属于你。** 两个问题不同的人站在同一座图书馆里，会关注不同的书。query 是*位置相关*的——每个 token 都有自己的 query。
3. **你关注每一本书，而不只是最好的那本。** 你不会只挑出最佳匹配来读，而是按相关性对所有书都读一些。哪怕匹配很差的卖也贡献一点。（这就是 softmax：权重小，但不是零。）

### 2.3 为什么是三个向量而不是一个

这个问题问得合理：模型为什么需要每个 token 的三种不同向量表示？为什么不能只用同一个 embedding 来扮演这三个角色？

因为这三组关系是**非对称**的：

* 一个 token 在别的 token 里*寻找什么*（Q），与它*向别的 token 提供什么*（K）并不相同。
* 它*提供出来供匹配*的东西（K），与它*被匹配时贡献*的东西（V）并不相同。

具体例子："river bank" 中的 **"bank"**：

* 作为 **query**，"bank" 在寻找能消歧自己义项的上下文——它想要金融类词还是地理类词？
* 作为 **key**，"bank" 把自己作为一个可能的匹配目标提供出去——"如果你的 query 与钱或地理有关，我就是你可能要关注的词。"
* 作为 **value**，"bank" 携带另一个 token 在关注它时会取走的信息——词义、词性、语义内容。

这三个角色需要三个不同的**向量子空间**。单一的共享 embedding 会把它们混在一起，迫使模型折中。给模型同一输入的**三个独立可学习投影**，就让它能灵活地用一个子空间做匹配、用另一个子空间传递内容。


<details>
<summary>English original</summary>

**2.1 The three roles**

| Component | Role | One-line intuition |
|---|---|---|
| **Query (Q)** | What the current position is *looking for* | "What information do I need?" |
| **Key (K)** | What each position *offers for matching* | "What kind of information do I contain?" |
| **Value (V)** | The actual *content* passed along when a match happens | "Here is my information." |

The mechanism, in three steps:

1. Compare a query `q` to every key `k_j`. The comparison produces a **score**.
2. Convert scores to a probability distribution with softmax (weights that are non-negative and sum to 1).
3. Take a weighted sum of values `v_j` using those weights. That weighted sum **is** the attention output.

**2.2 The library analogy**

Imagine you walk into a library with a question in your head. The question is your **query**. The library has thousands of books on shelves. Each book has a **title** printed on its spine and **contents** inside its covers.

* You scan the spines (the **keys**) and judge which titles look most relevant to your query.
* You don't read the contents (the **values**) yet — you just compare your question to the spines.
* Based on how well each title matches your question, you decide how much attention to pay to each book.
* You then pull down the relevant books and skim their contents in proportion to your interest. A book with a perfect-match title gets fully read; one with a barely-related title gets a glance; an irrelevant one is ignored.
* The "answer" you leave with is a **blended summary** of all the books you read, weighted by relevance.

Three crucial properties to notice:

1. **The title and the contents are separate things.** A book about *river ecology* might have the title "Riparian Systems" — title-matching ("river bank?" → "riparian!") and content-reading are different operations. Hence: key ≠ value, even when they describe the same item.
2. **Your query is yours alone.** Two people with different questions standing in the same library would attend to different books. The query is *position-dependent* — each token has its own.
3. **You attend to every book, not just the best one.** You don't pick the single best match and read it; you read all of them in proportion to relevance. Even a poorly-matching book contributes a little. (This is softmax: small weight, not zero.)

**2.3 Why three vectors and not one**

It's a fair question: why does the model need three different vector representations of each token? Why can't it just use the same embedding to play all three roles?

Because the three relationships are **asymmetric**:

* What a token is *looking for* in others (Q) is not the same as what it *offers to others* (K).
* What it *offers to be matched* (K) is not the same as what it *contributes when matched* (V).

Concrete example: the word **"bank"** in "river bank":

* As a **query**, "bank" is looking for context that disambiguates its sense — does it want financial words or geographical words?
* As a **key**, "bank" is offering itself as a possible match target — "I'm the word you might attend to if your query is about money or geography."
* As a **value**, "bank" carries the information another token will pull when it attends to it — the word-sense, the part-of-speech, the semantic content.

These three roles need three different **vector subspaces**. A single shared embedding would conflate them and force the model to compromise. By giving the model **three independent learned projections** of the same input, you give it the flexibility to use one subspace for matching and a different subspace for content delivery.

</details>

### 2.4 Q、K、V 实际如何产生

对于单个 token 的 embedding `x`（维度为 `d_model` 的向量），三个向量来自三个不同的可学习权重矩阵：

```
q = W_Q · x        W_Q is [d_k × d_model]    →  q is [d_k]
k = W_K · x        W_K is [d_k × d_model]    →  k is [d_k]
v = W_V · x        W_V is [d_v × d_model]    →  v is [d_v]
```

写成矩阵形式，对整个序列 `X`，其形状为 `[N, d_model]`（`N` 个 token）：

```
Q = X · W_Q^T       →  Q is [N, d_k]
K = X · W_K^T       →  K is [N, d_k]
V = X · W_V^T       →  V is [N, d_v]
```

（写成 `W_Q · x` 还是 `x · W_Q^T`，取决于框架把输入当作行向量还是列向量。PyTorch 使用行向量，所以是 `X @ W_Q^T`。数学上是一样的。）

权重矩阵 `W_Q`、`W_K`、`W_V` 像其他任何神经网络权重一样被**训练**——通过对 Transformer 所优化的任意损失做梯度下降。训练信号推动：

* `W_Q` 把 token 投影到一个空间中，使得“寻找”的 query 落在匹配的 key 附近。
* `W_K` 把 token 投影到一个空间中，使得“我提供的东西”落在正确的 query 能找到的地方。
* `W_V` 把 token 投影到一个空间中，其**加权和**编码有用的上下文信息。

关键在于，这些矩阵在初始化时**没有内置语义**——它们一开始是随机数。角色从训练中涌现。模型会自己根据数据弄清楚“query”和“key”应该长什么样。

### 2.5 维度——哪些自由、哪些锁定

公式中出现三个维度：`d_model`、`d_k`、`d_v`。两条约束：

* Q 的 `d_k` 和 K 的 `d_k` **必须匹配**——因为 attention 分数是内积 `q · k`，只有当两个向量处于同一空间时它才存在。
* `d_v` 是**自由的**。V 可以是任意宽度。attention 的输出形状为 `[N, d_v]`——它继承 V 的维度。

实践中，**所有生产级 Transformer 都设定 `d_v = d_k`。**为什么？因为 attention 之后的下一个操作通常是一个**线性投影**，投影回 `d_model`（残差流维度），而保持 `d_v = d_k` 能让记账清晰、参数量可预测。

对于有 `H` 个头的多头 attention，每头维度是 `d_k = d_v = d_model / H`。对于 Qwen3-4B：`d_model = 2560, H = 32`，所以 `d_k = d_v = 80`。等等——实际上 Qwen 特别使用 `head_dim = 128`（它不遵循严格的 `d_model / H` 规则），使 Q 投影的输出为 `n_heads · head_dim = 32 · 128 = 4096`，随后再向下投影回去。要点：这些维度是设计选择，现代模型会独立地调整它们。


<details>
<summary>English original</summary>

**2.4 How Q, K, V are actually produced**

For a single token's embedding `x` (a vector of dimension `d_model`), the three vectors come from three different learned weight matrices:

```
q = W_Q · x        W_Q is [d_k × d_model]    →  q is [d_k]
k = W_K · x        W_K is [d_k × d_model]    →  k is [d_k]
v = W_V · x        W_V is [d_v × d_model]    →  v is [d_v]
```

In matrix form, for a whole sequence `X` of shape `[N, d_model]` (`N` tokens):

```
Q = X · W_Q^T       →  Q is [N, d_k]
K = X · W_K^T       →  K is [N, d_k]
V = X · W_V^T       →  V is [N, d_v]
```

(Whether you write `W_Q · x` or `x · W_Q^T` depends on whether the framework treats inputs as row or column vectors. PyTorch uses row vectors, so it's `X @ W_Q^T`. The math is the same.)

The weight matrices `W_Q`, `W_K`, `W_V` are **trained** like any other neural-network weights — via gradient descent on whatever loss the transformer is optimized for. The training signal pushes:

* `W_Q` to project tokens into a space where "looking for" queries land near matching keys.
* `W_K` to project tokens into a space where "what I offer" lands where the right queries can find it.
* `W_V` to project tokens into a space whose **weighted sums** encode useful contextual information.

Crucially, these matrices have **no built-in semantics** at initialization — they start as random numbers. The roles emerge from training. The model figures out on its own what "queries" and "keys" should look like, given the data.

**2.5 Dimensions — what's free and what's locked**

Three dimensions appear in the formulas: `d_model`, `d_k`, `d_v`. Two constraints:

* `d_k` for Q and `d_k` for K **must match** — because the attention score is the inner product `q · k`, which only exists when the two vectors live in the same space.
* `d_v` is **free**. V can be any width. The output of attention has shape `[N, d_v]` — it inherits V's dimension.

In practice, **all production transformers set `d_v = d_k`.** Why? Because the next operation after attention is usually a **linear projection** back to `d_model` (the residual stream dimension), and keeping `d_v = d_k` makes the bookkeeping clean and the parameter count predictable.

For multi-head attention with `H` heads, the per-head dimension is `d_k = d_v = d_model / H`. For Qwen3-4B: `d_model = 2560, H = 32`, so `d_k = d_v = 80`. Wait — actually Qwen specifically uses `head_dim = 128` (it doesn't follow the strict `d_model / H` rule), giving the Q projection an output of `n_heads · head_dim = 32 · 128 = 4096`, which is then projected back down. The point: these dimensions are design choices, and modern models tune them independently.

</details>

### 2.6 一个具体数值演练

手工在一个玩具示例上运行 attention。4 个 token，`d_model = 4`，`d_k = d_v = 2`（数值小以便阅读）。

输入矩阵 `X`（4 个 token，每个是 4 维 embedding）：

```
X = [[1, 0, 1, 0],     # token 0
     [0, 1, 0, 1],     # token 1
     [1, 1, 0, 0],     # token 2
     [0, 0, 1, 1]]     # token 3
```

三个（数值小）学习到的投影矩阵（`W` 形状：`[d_k, d_model]` = `[2, 4]`）：

```
W_Q = [[1, 0, 1, 0],
       [0, 1, 0, 1]]

W_K = [[0, 1, 1, 0],
       [1, 0, 0, 1]]

W_V = [[1, 1, 0, 0],
       [0, 0, 1, 1]]
```

计算 Q、K、V（每个 `[4, 2]`）：

```
Q = X · W_Q^T = [[2, 0],      # token 0's query
                 [0, 2],      # token 1's query
                 [1, 1],      # token 2's query
                 [1, 1]]      # token 3's query

K = X · W_K^T = [[1, 1],      # token 0's key
                 [1, 1],      # token 1's key
                 [1, 1],      # token 2's key
                 [1, 1]]      # token 3's key

V = X · W_V^T = [[1, 1],      # token 0's value
                 [1, 1],      # token 1's value
                 [2, 0],      # token 2's value
                 [0, 2]]      # token 3's value
```

**token 0** 的 attention（其 query 为 `[2, 0]`）：

```
scores  = Q[0] · K^T = [2, 0]·[[1,1,1,1],[1,1,1,1]] = [2, 2, 2, 2]
                       (each key is identical here, so all scores match)
weights = softmax([2,2,2,2]) = [0.25, 0.25, 0.25, 0.25]
output  = 0.25·V[0] + 0.25·V[1] + 0.25·V[2] + 0.25·V[3]
        = 0.25·[1,1] + 0.25·[1,1] + 0.25·[2,0] + 0.25·[0,2]
        = [1, 1]
```

token 0 的输出为 `[1, 1]`——所有 value 的平均值。因为在这个玩具示例中每个 key 看起来都与其 query 相同，attention 均匀分布。

现在 token 1（query 为 `[0, 2]`）：

```
scores  = [0,2]·[[1,1,1,1],[1,1,1,1]] = [2, 2, 2, 2]
weights = [0.25, 0.25, 0.25, 0.25]
output  = [1, 1]
```

结果相同，因为 key 是退化的。**这就是模型需要*训练过的*投影的原因**——随机或平凡的 key 会产生均匀的 attention，这并不比取平均更好。训练之后，`W_K` 塑造 key，使不同 token 从不同角度看都不同，而 softmax 产生有意义的权重。

训练一个小模型后重做 §6.2 的多头练习，你会看到类似 `[8.3, -1.2, 12.7, 0.4]` 而非 `[2, 2, 2, 2]` 的分数——softmax 随后将权重集中在相关的 token 上。

### 2.7 attention 的非对称性

`Q · K^T` **不是对称的**。token A 对 token B 的 attention 可以与 B 对 A 的 attention 非常不同。具体来说：

```
score(i, j)  =  Q[i] · K[j]      ≠ in general     Q[j] · K[i]  =  score(j, i)
```

因为 Q 和 K 由应用于同一输入的*不同*学习到的矩阵产生，`Q[i]` 和 `K[i]` 是不同的向量。“非对称关系”直接内建于数学中。

为什么这很重要：在 “She picked up the book that her teacher had recommended” 这样的句子中，单词 **book** 可能强烈地反向关注 **teacher**（以确定*谁的*书），而 **teacher** 可能完全不需要关注 **book**（它已经知道自己是什么）。模型学习这些单向依赖，因为架构中没有任何东西强制对称。

### 2.8 K 和 V 总是按位置配对

一条微妙但关键的结构规则：`K[j]` 和 `V[j]` 由同一个 token `j` 产生。它们不是独立的槽位。当位置 `j` 上的 attention 权重高时，你拉取 `V[j]`——绝不会为某个其他 `k` 拉取 `V[k]`。

为什么这对 **cross-attention** 重要：在 encoder–decoder 模型中，decoder 产生 query，encoder 同时产生 key 和 value。encoder 从相同的源句 token 生成 K 和 V，并按位置配对。decoder 随后“跨越”图示，询问“这里哪个源 token 重要？”（通过 K），并拉取该 token 的内容（V）。

在现代 decoder-only 大语言模型的 **self-attention** 中，三者都来自同一序列——但按位置配对的规则仍然成立。token `j` 的 key 和 value 总是配对出现。

这就是生产 runtime 将 K 和 V 缓存为按位置索引的单个 **KV cache** 的原因。每个过去的 token 对应一个逻辑条目，包含其 key 向量和 value 向量。


<details>
<summary>English original</summary>

**2.6 A concrete numerical walkthrough**

Let's run attention on a toy example by hand. 4 tokens, `d_model = 4`, `d_k = d_v = 2` (small for legibility).

The input matrix `X` (4 tokens, each a 4-dim embedding):

```
X = [[1, 0, 1, 0],     # token 0
     [0, 1, 0, 1],     # token 1
     [1, 1, 0, 0],     # token 2
     [0, 0, 1, 1]]     # token 3
```

Three (small) learned projection matrices (`W` shapes: `[d_k, d_model]` = `[2, 4]`):

```
W_Q = [[1, 0, 1, 0],
       [0, 1, 0, 1]]

W_K = [[0, 1, 1, 0],
       [1, 0, 0, 1]]

W_V = [[1, 1, 0, 0],
       [0, 0, 1, 1]]
```

Compute Q, K, V (each `[4, 2]`):

```
Q = X · W_Q^T = [[2, 0],      # token 0's query
                 [0, 2],      # token 1's query
                 [1, 1],      # token 2's query
                 [1, 1]]      # token 3's query

K = X · W_K^T = [[1, 1],      # token 0's key
                 [1, 1],      # token 1's key
                 [1, 1],      # token 2's key
                 [1, 1]]      # token 3's key

V = X · W_V^T = [[1, 1],      # token 0's value
                 [1, 1],      # token 1's value
                 [2, 0],      # token 2's value
                 [0, 2]]      # token 3's value
```

Attention for **token 0** (its query is `[2, 0]`):

```
scores  = Q[0] · K^T = [2, 0]·[[1,1,1,1],[1,1,1,1]] = [2, 2, 2, 2]
                       (each key is identical here, so all scores match)
weights = softmax([2,2,2,2]) = [0.25, 0.25, 0.25, 0.25]
output  = 0.25·V[0] + 0.25·V[1] + 0.25·V[2] + 0.25·V[3]
        = 0.25·[1,1] + 0.25·[1,1] + 0.25·[2,0] + 0.25·[0,2]
        = [1, 1]
```

Token 0's output is `[1, 1]` — the average of all values. Because every key looked identical to its query in this toy, attention spread evenly.

Now token 1 (query `[0, 2]`):

```
scores  = [0,2]·[[1,1,1,1],[1,1,1,1]] = [2, 2, 2, 2]
weights = [0.25, 0.25, 0.25, 0.25]
output  = [1, 1]
```

Same result, because the keys were degenerate. **This is why the model needs *trained* projections** — random or trivial keys produce uniform attention, which is no better than averaging. After training, `W_K` shapes the keys so that different tokens look different from different angles, and softmax produces meaningful weights.

Repeat the §6.2 multi-head exercise after training a small model and you'll see scores like `[8.3, -1.2, 12.7, 0.4]` instead of `[2, 2, 2, 2]` — softmax then concentrates weight on the relevant tokens.

**2.7 The asymmetry of attention**

`Q · K^T` is **not symmetric**. Token A's attention to token B can be very different from B's attention to A. Concretely:

```
score(i, j)  =  Q[i] · K[j]      ≠ in general     Q[j] · K[i]  =  score(j, i)
```

Because Q and K are produced by *different* learned matrices applied to the same input, `Q[i]` and `K[i]` are different vectors. The "asymmetric relationship" is built directly into the math.

Why this matters: in a sentence like "She picked up the book that her teacher had recommended," the word **book** may strongly attend back to **teacher** (to figure out *whose* book), while **teacher** may not need to attend to **book** at all (it already knows what it is). The model learns these one-way dependencies because nothing in the architecture forces symmetry.

**2.8 K and V are always paired by position**

A subtle but critical structural rule: `K[j]` and `V[j]` are produced from the same token `j`. They are not independent slots. When attention weight on position `j` is high, you pull `V[j]` — never `V[k]` for some other `k`.

Why this matters for **cross-attention**: in an encoder–decoder model, the decoder produces queries, and the encoder produces both keys and values. The encoder generates K and V from the same source-sentence tokens, paired by position. The decoder then "reaches across" the diagram, asking "which source token matters here?" (via K) and pulling that token's content (V).

In **self-attention** in modern decoder-only LLMs, all three come from the same sequence — but the position-pairing rule still holds. Token `j`'s key and value always go together.

This is why production runtimes cache K and V as a single **KV cache** indexed by position. It's one logical entry per past token, containing both its key vector and its value vector.

</details>

### 2.9 KV cache —— 通往推理工程的桥梁

一个在训练期间完全无关紧要、但在推理期间极其重要的特性：

* 一旦一个序列被处理完毕，每个 token 的 **K 和 V 向量就固定了**。当你 decode（逐 token 生成阶段）下一个 token 时它们不会改变。
* 新 token 的 **Q 每一步都会变化**（每次都是一个新查询）。
* 因此，在自回归 decode 过程中，每一步都重新计算 Q，但你会**从缓存中重用 K 和 V**，而不是为每个过去的 token 重新运行 `W_K · x` 和 `W_V · x`。

这就是 **KV cache**。它是大语言模型推理中最重要的数据结构，也是 Q、K、V 被设计为独立投影而非融合成单一表示的原因。如果键和值不能从过去的 token 预先计算，那么每个 decode 的 token 都会花费 O(N²) 的工作量——并且你无法以可接受的延迟运行 4 k 上下文的聊天。

具体来说，对于 4 k 上下文的 Qwen3-4B，一个序列的 KV cache 包含：

```
2 (K and V) × 36 layers × 8 KV heads × 128 head_dim × 4096 positions × 2 bytes (FP16)
  ≈ 576 MB
```

该缓存在每个 decode 的 token 上都会被读取。它是整个 decode 路径上第二大的带宽负载（仅次于权重本身）。阶段 5 的推理讲座花了大量时间讨论它——参见 [Edge LLM Inference Internals](../../../阶段5-高级专题与专精/03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md) §5。

!!! tip "为什么存在三向量设计"
    Q/K/V 不仅仅是一种架构上的怪癖——它使得长上下文大语言模型推理在经济上可行。**“Q 每步重新计算”** 与 **“K、V 可跨步缓存”** 之间的不对称性，正是 GPT 级模型能在不从头重新计算过去的情况下进行对话的全部原因。

### 2.10 常见思维陷阱

!!! danger "六个需要摒弃的误解"
    * **“Q、K、V 是不同类型的信息。”** 不——它们是同一个输入 embedding 的三个 *投影*。token 的身份相同，只是角色不同。
    * **“在自注意力中，K 和 V 是同一个东西。”** 不——它们由不同的矩阵 `W_K` 和 `W_V` 生成，进入不同的子空间。它们按位置配对，但内容不相等。
    * **“Attention 仅仅是检索——找到最佳匹配并使用它。”** 不——它是对所有位置的 *软* 加权混合。即使最差的匹配也会贡献一小部分加权切片。
    * **“Attention 输出是单个 token 的值。”** 不——它是**每个 token 的值向量的加权组合**，权重之和为 1。无论序列长度如何，输出的形状都是 `[d_v]`。
    * **“每个头都从头计算自己的 Q、K、V。”** 是的，没错——这就是关键。每个头都有自己的 `W_Q^h`、`W_K^h`、`W_V^h`，因此每个头可以专注于不同的关系类型。
    * **“为什么 Q 来自当前 token，而 K 和 V 来自过去的 token？这很奇怪。”** 只有当你把 attention 看作“当前 token 向过去的 token 求助”时，它才奇怪。如果你把它看作图书馆的类比，就会更清晰：你（Q）带着问题走进去；书（K 和 V）无论谁走进来都放在书架上。

### 2.11 最小代码，再讲一次

现在所有结构都清楚了，每个查询的 attention 操作简化为四行：

```python
def attention_one_query(q, K, V):
    """
    q: [d_k]              one query vector  (e.g., from the current token)
    K: [n, d_k]           n key vectors     (one per past token)
    V: [n, d_v]           n value vectors   (one per past token, paired with K)
    returns:               [d_v]             a weighted blend of values
    """
    scores  = K @ q                  # [n]  — one score per (q, k_j) pair
    weights = softmax(scores)        # [n]  — non-negative, sum to 1
    return weights @ V               # [d_v] — weighted sum of values
```

这就是针对一个查询的整个 attention 操作。§3–§9 中的所有内容（缩放、掩码、批处理、多头、完整块、编码器与解码器）都是对这四行核心的泛化或包装。

---


<details>
<summary>English original</summary>

**2.9 The KV cache — a bridge to inference engineering**

A property that doesn't matter at all during training, but matters enormously during inference:

* Once a sequence has been processed, every token's **K and V vectors are fixed**. They don't change when you decode the next token.
* The new token's **Q changes** every step (it's a new query each time).
* So during autoregressive decoding, you compute Q fresh each step, but you **reuse K and V from a cache** rather than re-running `W_K · x` and `W_V · x` for every past token.

This is the **KV cache**. It is the single most important data structure in LLM inference, and it's the reason Q, K, V are designed as separate projections rather than fused into a single representation. If keys and values weren't pre-computable from past tokens, every decoded token would cost O(N²) work — and you couldn't run a 4 k-context chat at acceptable latency.

Concretely for Qwen3-4B at 4 k context, the KV cache for one sequence holds:

```
2 (K and V) × 36 layers × 8 KV heads × 128 head_dim × 4096 positions × 2 bytes (FP16)
  ≈ 576 MB
```

That cache is read on every decoded token. It's the second-biggest bandwidth load (after the weights themselves) on the entire decode path. The Phase 5 inference lectures spend a lot of time on it — see [Edge LLM Inference Internals](../../../阶段5-高级专题与专精/03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md) §5.

!!! tip "Why the three-vector design exists at all"
    Q/K/V is not just an architectural quirk — it is what makes long-context LLM inference economically viable. The asymmetry between **"Q recomputed per step"** and **"K, V cacheable across steps"** is the entire reason GPT-class models can hold a conversation without recomputing the past from scratch on every token.

**2.10 Common mental traps**

!!! danger "Six misconceptions to unlearn"
    * **"Q, K, V are different types of information."** No — they are three *projections* of the same input embedding. The token's identity is the same; only the role differs.
    * **"K and V are the same thing in self-attention."** No — they're produced by different matrices `W_K` and `W_V`, into different subspaces. They're paired by position, but not equal in content.
    * **"Attention is just retrieval — find the best match and use it."** No — it's a *soft* weighted blend over all positions. Even the worst match contributes a small weighted slice.
    * **"The attention output is a single token's value."** No — it's a **weighted combination of every token's value vector**, with weights summing to 1. The output has shape `[d_v]` regardless of sequence length.
    * **"Each head computes its own Q, K, V from scratch."** Yes, exactly — and that's the point. Each head has its own `W_Q^h`, `W_K^h`, `W_V^h`, so each head can specialize in a different relationship type.
    * **"Why does Q come from the current token but K and V from past tokens? That's weird."** It's weird only if you're thinking of attention as "the current token asking past tokens for help." It's cleaner if you think of it as the library analogy: you (Q) walk in with a question; the books (K and V) are sitting on the shelves regardless of who walks in.

**2.11 The minimal code, one more time**

With all the structure now clear, the per-query attention operation reduces to four lines:

```python
def attention_one_query(q, K, V):
    """
    q: [d_k]              one query vector  (e.g., from the current token)
    K: [n, d_k]           n key vectors     (one per past token)
    V: [n, d_v]           n value vectors   (one per past token, paired with K)
    returns:               [d_v]             a weighted blend of values
    """
    scores  = K @ q                  # [n]  — one score per (q, k_j) pair
    weights = softmax(scores)        # [n]  — non-negative, sum to 1
    return weights @ V               # [d_v] — weighted sum of values
```

That's the entire attention operation for one query. Everything in §3–§9 (scaling, masking, batching, multi-head, the full block, encoder vs decoder) is generalization or wrapping around this four-line core.

---

</details>

## 3. 打分函数 —— 从几何角度看点积

打分最常见的选择是**点积**：

```
score(q, k)  =  q · k  =  Σ_i q_i · k_i
```

为什么这在几何上成立：

```
q · k  =  ‖q‖ · ‖k‖ · cos(θ)
```

点积抓住了两点：

1. **方向一致性** —— `cos(θ)` 的取值范围是 −1 到 1。
2. **模长** —— 向量越长，得分越大。

当模型训练充分时，query 和 key 会落在同一个学到的 embedding 空间中，其中**指向相近方向的向量对应语义相关的 token**。点积自然会奖励这种对齐。

如果把 `q` 和 `k` 归一化到单位长度，点积就退化为**余弦相似度**：

```
cos(q, k)  =  (q · k) / (‖q‖ · ‖k‖)
```

余弦相似度有时会被使用（例如某些检索 embedding），但标准的 Transformer attention 并**不**做归一化 —— 让模型自己学会把模长当作信号的一部分。

---

## 4. 缩放点积 attention

写成矩阵形式，一次处理多个 query：

```
                          Q · Kᵀ
Attention(Q, K, V)  =  softmax( ─────── ) · V
                           √d_k
```

其中：

* `Q ∈ ℝ^(N × d_k)` —— `N` 个 query 向量，每个维度为 `d_k`。
* `K ∈ ℝ^(N × d_k)` —— `N` 个 key 向量，维度相同。
* `V ∈ ℝ^(N × d_v)` —— `N` 个 value 向量，维度为 `d_v`（通常为 `d_v = d_k`）。
* `d_k` 是 key 每个 head 的维度。

输出的形状为 `[N, d_v]` —— 每个 query 位置对应一个上下文感知的向量。

### 4.1 为什么要除以 sqrt(d_k)

随着 `d_k` 增大，点积 `q · k` 的**方差**大致随 `d_k` 线性增长（即两个随机向量的乘积之和）。不做缩放时，原始得分的量级会变大，把 softmax 推入**高度尖锐**的状态 —— 一两个元素的权重几乎占满，其余的梯度变得极小。

!!! warning "常见误解，恰好说反了"
    许多入门讲解会说“缩放是为了防止 softmax 过于平坦”。这恰好说反了。未缩放的点积产生的是**过尖**的 softmax（某个元素占主导），而不是平坦的。`1/√d_k` 这个因子才是修正手段，它把分布**展宽**回有用的范围。

修正的做法是除以点积分布的标准差来补偿，该标准差为 `√d_k`：

```
                          q · k         (q · k) / √d_k
Without scaling:  e^(q·k) ─────►        After scaling: e^(scaled) ─►
                          dominant                       balanced
                          single entry                   distribution
```

对于 `d_k = 64`（典型的每个 head 维度），`√d_k = 8`。对于 `d_k = 128`（Qwen），`√d_k ≈ 11.3`。缩放因子内建在 attention 公式里，在 softmax 之前应用。

### 4.2 Softmax —— 把得分变成权重

```
              e^(z_i)
softmax(z)_i = ─────────────
              Σ_j e^(z_j)
```

softmax 保证：

* 每个输出都非负。
* 所有输出之和恰好为 1。
* 输入越大输出越大（单调）。

因此输出是 `N` 个 key 位置上的合法概率分布。这个分布就是该 query 的 **attention pattern** —— 有时画成热力图，用来解读模型在“看”什么。

### 4.3 PyTorch 参考实现，约 10 行

```python
import torch
import torch.nn.functional as F
import math

def scaled_dot_product_attention(Q, K, V, mask=None):
    """
    Q, K: [..., N, d_k]
    V:    [..., N, d_v]
    mask: [..., N, N]  (optional, additive — use 0 to allow, -inf to block)
    """
    d_k = Q.size(-1)
    scores = (Q @ K.transpose(-2, -1)) / math.sqrt(d_k)   # [..., N, N]
    if mask is not None:
        scores = scores + mask
    weights = F.softmax(scores, dim=-1)                    # [..., N, N]
    return weights @ V                                     # [..., N, d_v]
```

跑一遍这段代码。整个 attention 机制就在这里。世上每一个 Transformer —— BERT、GPT、T5、Llama、Qwen —— 其核心不过是反复调用这个 10 行函数。

---

## 5. 自注意力

在**自注意力**中，Q、K、V 全部来自**同一个**输入序列。每个 token 的表示会被投影到三个不同的空间（用三个不同的学习到的权重矩阵），分别用作 query、key 和 value。

```
input X ∈ ℝ^(N × d_model)
   │
   ├─► [W_Q] ──► Q = X · W_Q      ∈ ℝ^(N × d_k)
   ├─► [W_K] ──► K = X · W_K      ∈ ℝ^(N × d_k)
   └─► [W_V] ──► V = X · W_V      ∈ ℝ^(N × d_v)

Attention(Q, K, V) → output ∈ ℝ^(N × d_v)
```

现在每个 token 都能关注到其他所有 token（包括它自己），且全部并行完成。位置 `i` 的输出是一个上下文感知的表示，融合了序列中每个相关位置的信息。


<details>
<summary>English original</summary>

**3. The Score Function — Dot Product, Geometrically**

The dominant choice for scoring is the **dot product**:

```
score(q, k)  =  q · k  =  Σ_i q_i · k_i
```

Why this works, geometrically:

```
q · k  =  ‖q‖ · ‖k‖ · cos(θ)
```

The dot product captures:

1. **Direction agreement** — `cos(θ)` ranges from −1 to 1.
2. **Magnitude** — longer vectors get bigger scores.

When the model is well-trained, queries and keys end up in a learned embedding space where **vectors pointing in similar directions correspond to semantically related tokens**. The dot product naturally rewards those alignments.

If you normalize `q` and `k` to unit length, the dot product collapses to **cosine similarity**:

```
cos(q, k)  =  (q · k) / (‖q‖ · ‖k‖)
```

Cosine similarity is sometimes used (e.g., in some retrieval embeddings), but standard transformer attention does **not** normalize — letting the model learn to use magnitude as part of the signal.

---

**4. Scaled Dot-Product Attention**

In matrix form, for many queries at once:

```
                          Q · Kᵀ
Attention(Q, K, V)  =  softmax( ─────── ) · V
                           √d_k
```

Where:

* `Q ∈ ℝ^(N × d_k)` — `N` query vectors, each of dimension `d_k`.
* `K ∈ ℝ^(N × d_k)` — `N` key vectors, same dimension.
* `V ∈ ℝ^(N × d_v)` — `N` value vectors, dimension `d_v` (usually `d_v = d_k`).
* `d_k` is the per-head dimension of the keys.

The output has shape `[N, d_v]` — one context-aware vector per query position.

**4.1 Why divide by sqrt(d_k)**

As `d_k` grows, the **variance** of the dot product `q · k` grows roughly linearly with `d_k` (the sum-of-products of two random vectors). Without scaling, the raw scores become large in magnitude, which pushes the softmax into a **highly peaked** regime — one or two entries get nearly all the weight, and gradients with respect to the rest become tiny.

!!! warning "Common misconception, reversed"
    Many beginner explanations say "scaling prevents the softmax from being too flat." This is backwards. Unscaled dot products produce a **too-sharp** softmax (one entry dominates), not a flat one. The `1/√d_k` factor is the fix that **broadens** the distribution back into a useful range.

The fix is to compensate by dividing by the standard deviation of the dot-product distribution, which is `√d_k`:

```
                          q · k         (q · k) / √d_k
Without scaling:  e^(q·k) ─────►        After scaling: e^(scaled) ─►
                          dominant                       balanced
                          single entry                   distribution
```

For `d_k = 64` (a typical per-head dim), `√d_k = 8`. For `d_k = 128` (Qwen), `√d_k ≈ 11.3`. The scaling factor is built into the attention formula and applied before the softmax.

**4.2 Softmax — turning scores into weights**

```
              e^(z_i)
softmax(z)_i = ─────────────
              Σ_j e^(z_j)
```

The softmax guarantees:

* Every output is non-negative.
* All outputs sum to exactly 1.
* Larger inputs become larger outputs (monotonic).

So the output is a valid probability distribution over the `N` key positions. This distribution is the **attention pattern** for this query — sometimes visualized as a heatmap to interpret what the model is "looking at."

**4.3 PyTorch reference, ~10 lines**

```python
import torch
import torch.nn.functional as F
import math

def scaled_dot_product_attention(Q, K, V, mask=None):
    """
    Q, K: [..., N, d_k]
    V:    [..., N, d_v]
    mask: [..., N, N]  (optional, additive — use 0 to allow, -inf to block)
    """
    d_k = Q.size(-1)
    scores = (Q @ K.transpose(-2, -1)) / math.sqrt(d_k)   # [..., N, N]
    if mask is not None:
        scores = scores + mask
    weights = F.softmax(scores, dim=-1)                    # [..., N, N]
    return weights @ V                                     # [..., N, d_v]
```

Run this once. It's the entire attention mechanism. Every transformer in the world — BERT, GPT, T5, Llama, Qwen — is, at its core, repeated applications of this 10-line function.

---

**5. Self-Attention**

In **self-attention**, Q, K, and V all come from the **same** input sequence. Each token's representation is projected three different ways (with three different learned weight matrices) and used as both a query and a key and a value.

```
input X ∈ ℝ^(N × d_model)
   │
   ├─► [W_Q] ──► Q = X · W_Q      ∈ ℝ^(N × d_k)
   ├─► [W_K] ──► K = X · W_K      ∈ ℝ^(N × d_k)
   └─► [W_V] ──► V = X · W_V      ∈ ℝ^(N × d_v)

Attention(Q, K, V) → output ∈ ℝ^(N × d_v)
```

Now every token can attend to every other token, including itself, all in parallel. The output for position `i` is a context-aware representation that incorporates information from every relevant position in the sequence.

</details>

### 5.1 "river bank" 示例的机械化

给定句子「She sat by the river bank」：

1. 分词：`[She, sat, by, the, river, bank]` → 6 个 token embedding。
2. 将每个 token 通过 `W_Q`、`W_K`、`W_V` 投影。
3. 对于 `bank` 的 query 位置：
    * 计算与 6 个 key 各自的分数（包括它自己）。
    * 与 `river` 的分数很高（学到的：类似 river 的 query 匹配类似 river 的 key）。
    * 与 `the` 的分数很低。
4. Softmax → 权重 → value 的加权和。
5. `bank` 的输出向量现在包含来自 `river` 的信息——消歧已经发生。

对每个位置重复。每个 token 的输出都是其自身的上下文感知版本。

### 5.2 自注意力没有顺序概念——位置编码

一个微妙的问题：自注意力是**置换等变**的。如果打乱输入 token，输出也以相同方式打乱，但每个输出的**内容**不变。模型完全无法区分「She sat by the river bank」与「bank river the by sat She」。

解决办法：在输入 embedding 进入 attention 之前，将位置信息注入其中。三种常见方案：

* **正弦位置编码（原始 Transformer）**——将不同频率的固定 `sin/cos` 波加到每个 token embedding 上。
* **可学习位置 embedding（BERT、GPT-2）**——每个位置一个可学习向量，加到 token embedding 上。
* **旋转位置编码（RoPE）（Llama、Qwen、Mistral）**——在每个 attention block 内部旋转 Q 和 K（不加到残差流上）。这是现代大语言模型使用的方案；完整论述见阶段 5 / Qwen Inference Optimization / Lecture 01 §4。

没有**某种**位置信号，Transformer 本质上就是一个忽略顺序的词袋模型。这些方案中总有一种在使用。

---

## 6. 多头注意力

单次 attention 操作只有固定的视角——一组 `(W_Q, W_K, W_V)` 矩阵、一种「看待」token 之间关系的「方式」。**多头注意力**并行运行多个 attention 操作，每个操作都有自己的投影矩阵：

```
input X
   │
   ├──► head 0: own W_Q, W_K, W_V ──► attention ──► out_0 ∈ ℝ^(N × d_v)
   ├──► head 1: own W_Q, W_K, W_V ──► attention ──► out_1
   ├──► ...
   └──► head H-1                                    ──► out_{H-1}
                                                          │
                       concatenate along feature dim:    [out_0 | out_1 | ... | out_{H-1}]   ∈ ℝ^(N × H·d_v)
                                                          │
                                                 ┌────────┴────────┐
                                                 │   linear W_O    │
                                                 └────────┬────────┘
                                                          ▼
                                                 output ∈ ℝ^(N × d_model)
```

每个 head 的维度通常是 `d_v = d_k = d_model / H`。所以多头注意力的开销大致等于一个大的单头 attention——但每个 head 可以专门化。

### 6.1 head 最终在做什么（经验层面）

当研究者探查训练好的 Transformer 时，常常会发现具有可解释专门化的 head：

* **位置 head**——关注前一个或后一个 token，与内容无关。
* **句法 head**——关注句法依存关系（主谓、修饰语-名词）。
* **共指 head**——回溯关注同一实体的更早提及。
* **标点 / 边界 head**——关注句子结束 token。
* **长距离 head**——关注较远的内容（尤其是在更深的 layer 中）。

并非每个 head 都能干净地专门化，而且 head 之间存在冗余。但这给出了一个直觉，解释多头为何优于单头：单个 soft 的 attention 模式是同时捕捉多种关系类型的低带宽瓶颈。


<details>
<summary>English original</summary>

**5.1 The "river bank" example, mechanized**

Given the sentence "She sat by the river bank":

1. Tokenize: `[She, sat, by, the, river, bank]` → 6 token embeddings.
2. Project each through `W_Q`, `W_K`, `W_V`.
3. For the query position of `bank`:
    * Compute scores against each of the 6 keys (including its own).
    * The score against `river` is high (learned: river-like queries match river-like keys).
    * The score against `the` is low.
4. Softmax → weights → weighted sum of values.
5. `bank`'s output vector now contains information from `river` — the disambiguation has happened.

Repeat for every position. Every token's output is a context-aware version of itself.

**5.2 Self-attention has no notion of order — positional encoding**

A subtle problem: self-attention is **permutation-equivariant**. If you shuffle the input tokens, the outputs shuffle the same way, but the **content** of each output is the same. The model literally cannot tell "She sat by the river bank" from "bank river the by sat She."

The fix: inject position information into the input embeddings before they hit attention. Three common schemes:

* **Sinusoidal positional encoding (original Transformer)** — add fixed `sin/cos` waves of different frequencies to each token embedding.
* **Learned positional embeddings (BERT, GPT-2)** — a learned vector per position, added to the token embedding.
* **Rotary Position Embedding (RoPE) (Llama, Qwen, Mistral)** — rotate Q and K inside each attention block (not added to the residual stream). This is what modern LLMs use; see Phase 5 / Qwen Inference Optimization / Lecture 01 §4 for the full treatment.

Without **some** positional signal, the transformer is essentially a bag-of-words model that ignores order. Always one of these schemes is in use.

---

**6. Multi-Head Attention**

A single attention operation has a fixed perspective — one set of `(W_Q, W_K, W_V)` matrices, one "way of looking at" relationships between tokens. **Multi-head attention** runs several attention operations in parallel, each with its own projection matrices:

```
input X
   │
   ├──► head 0: own W_Q, W_K, W_V ──► attention ──► out_0 ∈ ℝ^(N × d_v)
   ├──► head 1: own W_Q, W_K, W_V ──► attention ──► out_1
   ├──► ...
   └──► head H-1                                    ──► out_{H-1}
                                                          │
                       concatenate along feature dim:    [out_0 | out_1 | ... | out_{H-1}]   ∈ ℝ^(N × H·d_v)
                                                          │
                                                 ┌────────┴────────┐
                                                 │   linear W_O    │
                                                 └────────┬────────┘
                                                          ▼
                                                 output ∈ ℝ^(N × d_model)
```

The per-head dim is typically `d_v = d_k = d_model / H`. So multi-head attention costs roughly the same as one large single-head attention — but each head can specialize.

**6.1 What heads end up doing (empirically)**

When researchers probe trained transformers, they often find heads with interpretable specializations:

* **Positional heads** — attend to the previous or next token, regardless of content.
* **Syntactic heads** — attend to syntactic dependencies (subject-verb, modifier-noun).
* **Coreference heads** — attend back to earlier mentions of the same entity.
* **Punctuation / boundary heads** — attend to sentence-ending tokens.
* **Long-range heads** — attend to distant content (especially in deeper layers).

Not every head specializes cleanly, and there's redundancy across heads. But this gives intuition for why multi-head outperforms single-head: a single soft attention pattern is a low-bandwidth bottleneck for capturing multiple relationship types simultaneously.

</details>

### 6.2 带批处理的 PyTorch 实现

```python
import torch
import torch.nn as nn
import torch.nn.functional as F
import math

class MultiHeadAttention(nn.Module):
    def __init__(self, d_model: int, n_heads: int):
        super().__init__()
        assert d_model % n_heads == 0
        self.n_heads = n_heads
        self.d_k = d_model // n_heads
        self.W_qkv = nn.Linear(d_model, 3 * d_model, bias=False)   # fused QKV
        self.W_o   = nn.Linear(d_model, d_model, bias=False)

    def forward(self, x, mask=None):
        # x: [B, N, d_model]
        B, N, _ = x.shape
        qkv = self.W_qkv(x)                                         # [B, N, 3·d_model]
        q, k, v = qkv.chunk(3, dim=-1)                              # each [B, N, d_model]
        # Split into heads: [B, N, n_heads, d_k] → [B, n_heads, N, d_k]
        q = q.view(B, N, self.n_heads, self.d_k).transpose(1, 2)
        k = k.view(B, N, self.n_heads, self.d_k).transpose(1, 2)
        v = v.view(B, N, self.n_heads, self.d_k).transpose(1, 2)

        scores = (q @ k.transpose(-2, -1)) / math.sqrt(self.d_k)    # [B, n_heads, N, N]
        if mask is not None:
            scores = scores + mask
        weights = F.softmax(scores, dim=-1)
        out = weights @ v                                           # [B, n_heads, N, d_k]

        # Recombine heads
        out = out.transpose(1, 2).contiguous().view(B, N, -1)       # [B, N, d_model]
        return self.W_o(out)
```

这本质上就是 `torch.nn.MultiheadAttention` 做的事。真实的生产代码（`scaled_dot_product_attention`、FlashAttention）要快得多，但数学上等价。

---

## 7. 掩码 —— Padding 与 Causal

attention 默认是宽松的 —— 每个位置都可以 attend 到其他任意位置。有两种情况需要**屏蔽**部分 attention：

### 7.1 Padding 掩码

在批处理训练和推理中，同一批内的序列会被填充到统一长度。padding token 是占位符，绝不应参与 attention 输出。解决办法：在 softmax 之前把 padding 位置的分数设为 `-∞`：

```
mask[i, j] = -inf  if token j is padding
mask[i, j] =    0  otherwise
```

经过 softmax 后，`e^(-∞) = 0`，因此 padding 位置的权重为 0。它们不影响输出。

### 7.2 Causal（自回归）掩码

对于从左到右生成 token 的语言模型，每个输出位置只能依赖 ≤ 自身的位置。否则模型在训练时会通过看未来而“作弊”，而在推理时，每追加一个新 token，模型的预测都会不同。

因果掩码是上三角的 `-∞`：

```
       j=0   j=1   j=2   j=3
i=0    0    -inf  -inf  -inf
i=1    0     0    -inf  -inf
i=2    0     0     0    -inf
i=3    0     0     0     0
```

结合 softmax 使 `-∞` 变为 0 这一技巧，就强制了“位置 i 只能看到 j ≤ i”。

**所有 decoder-only 大语言模型**都用这个掩码：GPT-2/3/4、Llama、Qwen、Mistral、Phi、Gemma。位置 `i` 的输出只由位置 `0..i` 计算得到。

### 7.3 组合掩码

实践中把两个掩码相加：

```python
combined_mask = padding_mask + causal_mask
# Both are -inf at "block" positions, 0 at "allow" positions; sum is also -inf or 0.
```

---

## 8. Self-Attention 与 Cross-Attention

到目前为止 Q、K、V 都来自同一个序列。**Cross-attention** 打破了这种对称性：Q 来自一个序列，K 和 V 来自另一个序列。

| 类型 | Q 来源 | K、V 来源 | 常见用途 |
|---|---|---|---|
| Self-attention | 序列 A | 序列 A | Encoder 块、decoder-only 大语言模型 |
| Cross-attention | 序列 A | 序列 B | Encoder–decoder 模型（翻译、T5、Whisper、vision-language） |

在 encoder–decoder Transformer 中（例如原始 Transformer、T5、BART、Whisper）：

```
Input sequence (e.g., source language)
   │
   ▼
┌─────────────────────────┐
│  Encoder (N layers)     │
│   each layer:           │
│   - self-attention      │
│   - FFN                 │
└──────────┬──────────────┘
           │  encoder_output ∈ ℝ^(N_enc × d_model)
           │
Output sequence (e.g., target lang., generated so far)
   │       │
   ▼       │
┌──────────────────────────────┐
│  Decoder (N layers)          │
│   each layer:                │
│   - masked self-attention    │  ← Q, K, V from generated-so-far
│   - cross-attention          │  ← Q from decoder, K & V from encoder_output
│   - FFN                      │
└──────────┬───────────────────┘
           ▼
         output logits
```

Cross-attention 是 decoder 从编码后的源端取信息的方式 —— 在每个生成位置，decoder“注视”源端表示，以决定生成什么。

现代 decoder-only 大语言模型（GPT、Llama、Qwen）**不使用 cross-attention** —— 它们把一切折叠为对拼接后 `[prompt | generation]` 序列的一次大规模 self-attention，并配合因果掩码。这更简单，扩展性也更好。

---


<details>
<summary>English original</summary>

**6.2 The PyTorch implementation, with batching**

```python
import torch
import torch.nn as nn
import torch.nn.functional as F
import math

class MultiHeadAttention(nn.Module):
    def __init__(self, d_model: int, n_heads: int):
        super().__init__()
        assert d_model % n_heads == 0
        self.n_heads = n_heads
        self.d_k = d_model // n_heads
        self.W_qkv = nn.Linear(d_model, 3 * d_model, bias=False)   # fused QKV
        self.W_o   = nn.Linear(d_model, d_model, bias=False)

    def forward(self, x, mask=None):
        # x: [B, N, d_model]
        B, N, _ = x.shape
        qkv = self.W_qkv(x)                                         # [B, N, 3·d_model]
        q, k, v = qkv.chunk(3, dim=-1)                              # each [B, N, d_model]
        # Split into heads: [B, N, n_heads, d_k] → [B, n_heads, N, d_k]
        q = q.view(B, N, self.n_heads, self.d_k).transpose(1, 2)
        k = k.view(B, N, self.n_heads, self.d_k).transpose(1, 2)
        v = v.view(B, N, self.n_heads, self.d_k).transpose(1, 2)

        scores = (q @ k.transpose(-2, -1)) / math.sqrt(self.d_k)    # [B, n_heads, N, N]
        if mask is not None:
            scores = scores + mask
        weights = F.softmax(scores, dim=-1)
        out = weights @ v                                           # [B, n_heads, N, d_k]

        # Recombine heads
        out = out.transpose(1, 2).contiguous().view(B, N, -1)       # [B, N, d_model]
        return self.W_o(out)
```

This is essentially what `torch.nn.MultiheadAttention` does. Real production code (`scaled_dot_product_attention`, FlashAttention) is much faster but mathematically equivalent.

---

**7. Masking — Padding and Causal**

Attention is permissive by default — every position can attend to every other position. Two cases where we need to **block** some attention:

**7.1 Padding mask**

In batched training and inference, sequences in the same batch are padded to a common length. The padding tokens are placeholders and should never contribute to the attention output. The fix: set the scores for padding positions to `-∞` before the softmax:

```
mask[i, j] = -inf  if token j is padding
mask[i, j] =    0  otherwise
```

After softmax, `e^(-∞) = 0`, so padding positions get weight 0. They don't affect the output.

**7.2 Causal (autoregressive) mask**

For a language model that generates tokens left-to-right, each output position must only depend on positions ≤ itself. Otherwise the model would "cheat" at training time by looking at the future, and at inference time the model would predict differently each time you appended a new token.

The causal mask is upper-triangular `-∞`:

```
       j=0   j=1   j=2   j=3
i=0    0    -inf  -inf  -inf
i=1    0     0    -inf  -inf
i=2    0     0     0    -inf
i=3    0     0     0     0
```

Combined with the softmax-makes-`-∞`-go-to-0 trick, this enforces "position i can only see j ≤ i."

This is the mask used by **all decoder-only LLMs**: GPT-2/3/4, Llama, Qwen, Mistral, Phi, Gemma. The output at position `i` is computed from positions `0..i` only.

**7.3 Combining masks**

In practice you add the two masks:

```python
combined_mask = padding_mask + causal_mask
# Both are -inf at "block" positions, 0 at "allow" positions; sum is also -inf or 0.
```

---

**8. Self-Attention vs Cross-Attention**

So far Q, K, V have all come from the same sequence. **Cross-attention** breaks that symmetry: Q comes from one sequence, K and V come from another.

| Type | Q source | K, V source | Common use |
|---|---|---|---|
| Self-attention | sequence A | sequence A | Encoder blocks, decoder-only LLMs |
| Cross-attention | sequence A | sequence B | Encoder–decoder models (translation, T5, Whisper, vision-language) |

In an encoder–decoder transformer (e.g., the original Transformer, T5, BART, Whisper):

```
Input sequence (e.g., source language)
   │
   ▼
┌─────────────────────────┐
│  Encoder (N layers)     │
│   each layer:           │
│   - self-attention      │
│   - FFN                 │
└──────────┬──────────────┘
           │  encoder_output ∈ ℝ^(N_enc × d_model)
           │
Output sequence (e.g., target lang., generated so far)
   │       │
   ▼       │
┌──────────────────────────────┐
│  Decoder (N layers)          │
│   each layer:                │
│   - masked self-attention    │  ← Q, K, V from generated-so-far
│   - cross-attention          │  ← Q from decoder, K & V from encoder_output
│   - FFN                      │
└──────────┬───────────────────┘
           ▼
         output logits
```

Cross-attention is how the decoder pulls information from the encoded source — at each generated position, the decoder "looks at" the source representation to decide what to generate.

Modern decoder-only LLMs (GPT, Llama, Qwen) **don't use cross-attention** — they fold everything into one big self-attention over a concatenated `[prompt | generation]` sequence, with a causal mask. This is simpler and scales better.

---

</details>

## 9. 完整的 Transformer Block

Attention 只是其中一块。一个真正的 Transformer block 还包含：

* **前馈网络（FFN）** —— 逐位置独立施加。两个 linear layer，中间夹一个非线性。绝大多数参数都在这里。
* **残差连接**，环绕 attention 和 FFN 这两个 sub-layer。
* **Layer normalization**（现代模型中为 RMSNorm），置于每个 sub-layer 之前。

典型的（现代、pre-norm）block：

```
x ─┬──────────► RMSNorm ──► MultiHeadAttention ──► (residual add) ──┐
   │                                                                 │
   └──────────────────────────────────────────────────────────────┐  │
                                                                  ▼  ▼
                                                                 x + attn_out  =  x'
                                                                       │
                                                                       │
   ┌───────────────────────────────────────────────────────────────────┘
   │
x' ─┬──────────► RMSNorm ──► FFN(x') ──► (residual add) ──► x''
    │
    └─────────────────────────────────────────────►
```

一个 `D` layer 的 Transformer 就是 `D` 个这样的 block 堆叠而成。对 Qwen3-4B，`D = 36`；对 Qwen2.5-72B，`D = 80`；对 BERT-base，`D = 12`；对 GPT-3，`D = 96`。

### 9.1 FFN 详解

经典的 FFN，逐位置：

```
ffn(x) = W_2 · activation(W_1 · x + b_1) + b_2
```

`W_1: [d_model → d_ff]` 加宽；`W_2: [d_ff → d_model]` 投影回原维度。`d_ff` 通常是 `4 · d_model`（BERT、GPT-2）或 `~2.7 · d_model`（Llama、Qwen 用 SwiGLU）。

现代大语言模型使用 **SwiGLU**（带 SiLU 激活函数的门控线性单元）：

```
ffn(x) = W_down · ( silu(W_gate · x) ⊙ (W_up · x) )
```

用三个投影代替两个；gate 和 up 两条路径产生宽度为 `d_ff` 的向量，逐元素相乘，再投影回去。计算量略多，但在相同参数预算下质量始终更好。

### 9.2 Pre-norm 与 post-norm

**原始 Transformer** 把 LayerNorm 放在 sub-layer + 残差**之后**（“post-norm”）。现代模型把它放在**之前**（“pre-norm”）。Pre-norm 在深层时更稳定（无需 learning-rate warmup 即可训练 100+ layer 的模型），现已成为标准。

忘掉原始的顺序。真实模型用的都是 pre-norm。

### 9.3 完整的前向传播

对于一个像 Qwen3 这样的 decoder-only 大语言模型，把它们合起来：

```
tokens ─► token_embedding ─┬─► (positional encoding, if any) ─►
                           │
                           ▼
                     ┌────────────────────────┐
                     │  Transformer block × D │
                     │                        │
                     │  RMSNorm → MHA  ⊕      │
                     │  RMSNorm → FFN  ⊕      │
                     └────────────┬───────────┘
                                  │
                                  ▼
                              RMSNorm
                                  │
                                  ▼
                     LM head (Linear: d_model → vocab)
                                  │
                                  ▼
                              logits
                                  │
                                  ▼
                         softmax → sample → next token
```

把采样得到的 token 追加到输入，重复。这就是自回归生成。

---

## 10. 三种典型配置

| 配置 | 示例 | Mask | 用途 |
|---|---|---|---|
| **Encoder-only** | BERT、RoBERTa、DeBERTa | 仅 padding | 分类、embedding、检索 |
| **Decoder-only** | GPT-2/3/4、Llama、Qwen、Mistral、Phi、Gemma | Causal（+ padding） | 生成、对话、代码补全 |
| **Encoder–decoder** | 原始 Transformer、T5、BART、Whisper、mT5 | Encoder：padding。Decoder：causal + cross-attention | 翻译、摘要、自动语音识别 |

随时间的变化：

* **2017–2019** —— encoder–decoder 被视为标准。
* **2018–2020** —— encoder-only（BERT）主导了 NLP benchmark。
* **2020–至今** —— decoder-only 大语言模型规模扩大，吃下了一切。

decoder-only 大语言模型胜出的原因：一个因果掩码的 self-attention 模型，只要**规模足够**，就能完成 encoder–decoder 模型能完成的每一项任务，外加交互式生成。它统一了训练与推理，统一了预训练与微调，也统一了“理解”任务与“生成”任务。

推理课程中要部署的所有模型都是 decoder-only。

---


<details>
<summary>English original</summary>

**9. The Full Transformer Block**

Attention is one piece. A real transformer block also has:

* A **feed-forward network (FFN)** — applied independently per position. Two linear layers with a nonlinearity in between. This is where most of the parameters live.
* **Residual connections** around both the attention and the FFN sub-layers.
* **Layer normalization** (or RMSNorm in modern models) before each sub-layer.

The canonical (modern, pre-norm) block:

```
x ─┬──────────► RMSNorm ──► MultiHeadAttention ──► (residual add) ──┐
   │                                                                 │
   └──────────────────────────────────────────────────────────────┐  │
                                                                  ▼  ▼
                                                                 x + attn_out  =  x'
                                                                       │
                                                                       │
   ┌───────────────────────────────────────────────────────────────────┘
   │
x' ─┬──────────► RMSNorm ──► FFN(x') ──► (residual add) ──► x''
    │
    └─────────────────────────────────────────────►
```

A `D`-layer transformer is just `D` of these blocks stacked. For Qwen3-4B, `D = 36`; for Qwen2.5-72B, `D = 80`; for BERT-base, `D = 12`; for GPT-3, `D = 96`.

**9.1 The FFN, in detail**

The classic FFN, per position:

```
ffn(x) = W_2 · activation(W_1 · x + b_1) + b_2
```

`W_1: [d_model → d_ff]` widens; `W_2: [d_ff → d_model]` projects back. `d_ff` is typically `4 · d_model` (BERT, GPT-2) or `~2.7 · d_model` (Llama, Qwen with SwiGLU).

Modern LLMs use **SwiGLU** (gated linear unit with SiLU activation):

```
ffn(x) = W_down · ( silu(W_gate · x) ⊙ (W_up · x) )
```

Three projections instead of two; the gate and up paths produce vectors of width `d_ff`, multiply element-wise, then project back. Slightly more compute but consistently better quality at the same parameter budget.

**9.2 Pre-norm vs post-norm**

The **original Transformer** put LayerNorm **after** the sub-layer + residual ("post-norm"). Modern models put it **before** ("pre-norm"). Pre-norm is more stable at depth (you can train 100+ layer models without learning-rate warmup) and is now standard.

Forget the original ordering. Pre-norm is what real models use.

**9.3 The full forward pass**

Putting it together for a decoder-only LLM like Qwen3:

```
tokens ─► token_embedding ─┬─► (positional encoding, if any) ─►
                           │
                           ▼
                     ┌────────────────────────┐
                     │  Transformer block × D │
                     │                        │
                     │  RMSNorm → MHA  ⊕      │
                     │  RMSNorm → FFN  ⊕      │
                     └────────────┬───────────┘
                                  │
                                  ▼
                              RMSNorm
                                  │
                                  ▼
                     LM head (Linear: d_model → vocab)
                                  │
                                  ▼
                              logits
                                  │
                                  ▼
                         softmax → sample → next token
```

Append the sampled token to the input, repeat. That's autoregressive generation.

---

**10. The Three Canonical Configurations**

| Configuration | Examples | Mask | Use |
|---|---|---|---|
| **Encoder-only** | BERT, RoBERTa, DeBERTa | Padding only | Classification, embedding, retrieval |
| **Decoder-only** | GPT-2/3/4, Llama, Qwen, Mistral, Phi, Gemma | Causal (+ padding) | Generation, chat, code completion |
| **Encoder–decoder** | Original Transformer, T5, BART, Whisper, mT5 | Encoder: padding. Decoder: causal + cross-attention | Translation, summarization, ASR |

What changed over time:

* **2017–2019** — encoder–decoder was assumed standard.
* **2018–2020** — encoder-only (BERT) dominated NLP benchmarks.
* **2020–present** — decoder-only LLMs scaled up and ate everything.

The reason decoder-only LLMs won: a single causal-masked self-attention model with **enough scale** can do every task an encoder–decoder model can, plus interactive generation. You unify training and inference, you unify pretraining and fine-tuning, and you unify "understanding" tasks with "generation" tasks.

All the models you'll deploy in the inference lectures are decoder-only.

---

</details>

## 11. 从架构到推理

本讲是阶段 5 推理各讲的前置要求。桥梁：

* **阶段 5 / 边缘 AI / 边缘大语言模型推理内部机制** 把相同的 QKV/FFN 数学视为**带宽问题**。你刚学到的 transformer block 正是 GEMV-decode（GEMV 即矩阵-向量乘，decode 即逐 token 生成阶段）所计算的内容。
* **阶段 5 / Qwen 推理优化（6 讲系列）** 把相同架构专门化到两个具体的 Qwen 模型：用于边缘的 Qwen3-4B-Q4_K_M 和用于数据中心的 Qwen2.5-72B-FP16。相同的框图；不同的硬件工程。
* **阶段 5 / Qwen 推理优化 / 第 01 讲 §4** 是完整的旋转位置编码（RoPE）深入讲解——Qwen、Llama、Mistral 等使用的现代位置编码方案。
* **阶段 5 / Qwen 推理优化 / 第 06 讲** 是 cuBLAS GEMM（矩阵-矩阵乘）/batched-GEMM 讲解——即 attention `Q @ Kᵀ` 在底层实际调用的内容。

!!! tip "为什么本讲能解锁其余所有内容"
    一旦理解这里的材料，推理各讲就不再是一堵缩写墙：

    * **分组查询注意力（GQA）、MQA、MHA** — 关于多头 attention 是否跨 head 共享 K/V 的选择。
    * **AWQ、GPTQ、Q4_K_M** — 关于如何量化 `W_Q, W_K, W_V, W_O, W_gate, W_up, W_down` 矩阵的选择。
    * **FlashAttention** — 你在 §6.2 实现的相同 attention 的更快、分块实现。
    * **投机解码** — 让小自回归模型与大模型并行运行。

    每一个都嵌入你刚学到的架构中。

---

## 12. 动手练习

1. **用 30 行实现 attention。** 敲出 §4.3 的参考实现，并在一个小例子（4 个 token，d_k=8）上验证。打印 attention 权重。确认它们沿最后一轴求和为 1。

2. **构建单个 transformer block。** 把 §6.2 的多头 attention 与 SwiGLU FFN、RMSNorm 及残差连接组合起来。输入一个随机 `[1, 16, 256]` 输入，并确认输出形状为 `[1, 16, 256]`。

3. **与 PyTorch 内置实现对比。** 把你手写的 attention 替换为 `torch.nn.functional.scaled_dot_product_attention`，并验证输出在 1e-5 内匹配。然后启用 Flash 后端（`with torch.nn.attention.sdpa_kernel(SDPBackend.FLASH_ATTENTION):`），确认结果仍在数值上等价。

4. **因果掩码实战。** 在字符级数据集（Shakespeare，约 1 MB）上训练一个很小的 decoder-only transformer（2 层，d_model=64）。经过 1000 步后从中采样。然后故意移除因果掩码，重新训练，并观察损失在训练期间看起来好得多，但模型生成乱码。（因为它学会了通过查看未来来“作弊”。）

5. **可视化 attention。** 在预训练模型上（通过 `transformers` 加载 `Qwen/Qwen3-4B-Instruct`），针对提示 “She sat by the river bank.” 挂接到某一层的 attention 权重。绘制将 “bank” 与 “river” 联系得最强的 head 的 attention 热力图。与另一个模式无法解释的 attention head 对比。

6. **统计参数量。** 对于具有 `d_model = 2560, n_heads = 32, n_layers = 36, d_ff = 6912, vocab = 151936` 的 transformer，从头计算总参数量。与 Qwen3-4B 公布的 4 B 数字对比。（你应该落在接近 3.2 B 的位置——embedding 贡献了剩余的大部分。）

7. **排列等变性演示。** 构建一个 **不使用** 位置编码的单个 self-attention 层。向它输入两个彼此互为排列的输入（相同 token，不同顺序）。确认输出也被排列（相同值，不同顺序）。然后加入正弦位置编码并重新运行；输出现在应在值上不同，而不只是在位置上不同。

8. **阅读真实模型。** 打开 `Qwen/Qwen3-4B-Instruct` 的 `config.json`，按名称找出本讲中的每一个量：`d_model`（`hidden_size`）、`n_heads`（`num_attention_heads`）、`n_kv_heads`（`num_key_value_heads`——注意 GQA，即分组查询注意力）、`n_layers`（`num_hidden_layers`）、`d_ff`（`intermediate_size`）、vocab、RoPE base。根据配置预测 `model.layers[0].self_attn.q_proj.weight` 的形状，并用 `print(model.state_dict()['model.layers.0.self_attn.q_proj.weight'].shape)` 验证。

---


<details>
<summary>English original</summary>

**11. From Architecture to Inference**

This lecture is the prerequisite for the Phase 5 inference lectures. The bridge:

* **Phase 5 / Edge AI / Edge LLM Inference Internals** treats the same QKV/FFN math as a **bandwidth problem**. The transformer block you just learned about is what GEMV-decode is computing.
* **Phase 5 / Qwen Inference Optimization (6-lecture series)** specializes the same architecture to two specific Qwen models: Qwen3-4B-Q4_K_M for edge and Qwen2.5-72B-FP16 for datacenter. Same block diagram; different hardware engineering.
* **Phase 5 / Qwen Inference Optimization / Lecture 01 §4** is the full RoPE deep dive — the modern positional-encoding scheme used by Qwen, Llama, Mistral, etc.
* **Phase 5 / Qwen Inference Optimization / Lecture 06** is the cuBLAS GEMM/batched-GEMM lecture — what the attention `Q @ Kᵀ` actually calls under the hood.

!!! tip "Why this lecture unlocks everything else"
    Once you understand the material here, the inference lectures stop being a wall of acronyms:

    * **GQA, MQA, MHA** — choices about whether multi-head attention shares K/V across heads.
    * **AWQ, GPTQ, Q4_K_M** — choices about how to quantize the `W_Q, W_K, W_V, W_O, W_gate, W_up, W_down` matrices.
    * **FlashAttention** — a faster, tiled implementation of the same attention you implemented in §6.2.
    * **Speculative decoding** — running a small autoregressive model alongside a big one.

    Each one slots into the architecture you just learned.

---

**12. Hands-On Exercises**

1. **Implement attention in 30 lines.** Type out the §4.3 reference and verify on a small example (4 tokens, d_k=8). Print the attention weights. Confirm they sum to 1 along the last axis.

2. **Build a single transformer block.** Combine the multi-head attention from §6.2 with a SwiGLU FFN, RMSNorm, and residual connections. Feed in a random `[1, 16, 256]` input and confirm the output shape is `[1, 16, 256]`.

3. **Compare to PyTorch's built-ins.** Replace your manual attention with `torch.nn.functional.scaled_dot_product_attention` and verify the output matches to within 1e-5. Then enable Flash backend (`with torch.nn.attention.sdpa_kernel(SDPBackend.FLASH_ATTENTION):`) and confirm the result is still numerically equivalent.

4. **Causal mask in action.** Train a tiny decoder-only transformer (2 layers, d_model=64) on a character-level dataset (Shakespeare, ~1 MB). Sample from it after 1000 steps. Then deliberately remove the causal mask, retrain, and observe how the loss looks much better during training but the model generates gibberish. (Because it learned to "cheat" by looking at the future.)

5. **Visualize attention.** On a pretrained model (load `Qwen/Qwen3-4B-Instruct` via `transformers`), hook into one layer's attention weights for the prompt "She sat by the river bank." Plot the attention heatmap for the head that most strongly connects "bank" to "river." Compare to a different attention head where the pattern is uninterpretable.

6. **Count parameters.** For a transformer with `d_model = 2560, n_heads = 32, n_layers = 36, d_ff = 6912, vocab = 151936`, compute total parameters from scratch. Compare to Qwen3-4B's published 4 B figure. (You should land near 3.2 B — the embedding contributes most of the remainder.)

7. **Permutation-equivariance demo.** Build a single self-attention layer **without** positional encoding. Feed it two inputs that are permutations of each other (same tokens in different order). Confirm the outputs are also permuted (same values, different order). Then add sinusoidal positional encoding and re-run; outputs should now differ in value, not just position.

8. **Read a real model.** Open `Qwen/Qwen3-4B-Instruct`'s `config.json` and identify, by name, every quantity from this lecture: `d_model` (`hidden_size`), `n_heads` (`num_attention_heads`), `n_kv_heads` (`num_key_value_heads` — note GQA), `n_layers` (`num_hidden_layers`), `d_ff` (`intermediate_size`), vocab, RoPE base. Predict the shape of `model.layers[0].self_attn.q_proj.weight` from the config and verify with `print(model.state_dict()['model.layers.0.self_attn.q_proj.weight'].shape)`.

---

</details>

## 13. 要点

| 要点 | 为什么重要 |
|---|---|
| Attention 消除了 RNN 的固定上下文瓶颈 | 每个位置都能直接取用其他任意位置的信息 |
| Q/K/V 是「按相似度查找、按 softmax 聚合」的三角色视图 | 所有 attention 数学都归结于此 |
| 缩放点积除以 √d_k 是为了防止 softmax 过于尖锐 | 这是*修正*，不是成因——不缩放会太尖锐，而非太平缓 |
| 自注意力没有顺序概念——需要位置编码 | 没有它，Transformer 就是一个词袋模型 |
| 多头注意力用各自的投影并行运行多组 attention 运算 | 各头可以特化；总参数量与单头相近 |
| padding mask 把对 padding 的注意力置零；causal mask 屏蔽未来位置 | mask 以 `-inf` 加到分数上，剩下交给 softmax |
| 现代 LLM 都是带 causal mask 的 decoder-only 架构 | 理解与生成共用一套统一架构 |
| 完整 block 是 RMSNorm → attention → ⊕ → RMSNorm → FFN → ⊕ | 这个模式在你部署的每个 Transformer 中重复 `D` 次 |
| Attention 权重是线索，不是完整解释 | 对可解释性有用，但绝不能证明「模型决定了什么」 |
| 自注意力在序列长度上是 `O(N²)` | 这就是长上下文推理困难的原因；所有进阶技术都是为了管理它 |

---

## 14. 一句话总结

!!! quote ""
    Attention 让神经网络通过把 query 与 key 相比较，动态聚焦于序列中最相关的部分，并用得到的权重把 value 组合成具备上下文感知的表示——而 Transformer 不过是 `D` 层多头注意力加带残差连接的 FFN，mask 的选择决定它是 encoder、decoder 还是 encoder–decoder 模型。

---

## 资源

* **["Attention Is All You Need" (Vaswani et al., 2017)](https://arxiv.org/abs/1706.03762)：** 原始 Transformer 论文。简洁；读两遍。
* **[The Annotated Transformer (Harvard NLP)](https://nlp.seas.harvard.edu/2018/04/03/attention.html)：** 与论文文本对照的逐行 PyTorch 实现。最好的学习资源。
* **[The Illustrated Transformer (Jay Alammar)](https://jalammar.github.io/illustrated-transformer/)：** 最易上手的可视化讲解；适合建立第一直觉。
* **["Transformers from Scratch" (Brandon Rohrer)](https://e2eml.school/transformers.html)：** 用不同直觉再过一遍同样的内容。
* **[3Blue1Brown — Neural Networks chapter 5+ (Attention)](https://www.youtube.com/watch?v=eMlx5fFNoYc)：** 视频讲解，图很干净。
* **[Hugging Face Transformers — `modeling_qwen2.py`](https://github.com/huggingface/transformers/blob/main/src/transformers/models/qwen2/modeling_qwen2.py)：** 一份可以从头读到尾的生产级 Transformer 实现。
* **[PyTorch — `scaled_dot_product_attention` docs](https://pytorch.org/docs/stable/generated/torch.nn.functional.scaled_dot_product_attention.html)：** 现代融合注意力原语。
* **[FlashAttention paper (Dao et al., 2022)](https://arxiv.org/abs/2205.14135)：** 如何通过分块把同样的 attention 算得快 5×。学基础不需要，但它是通向推理工程的桥梁。
* **[阶段 5 — Edge LLM Inference Internals](../../../阶段5-高级专题与专精/03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md)：** 第一节把这些内容应用到真实硬件的下游课程。
* **[阶段 5 — Qwen Inference Optimization](../../../阶段5-高级专题与专精/03-方向C-边缘AI/05-Qwen推理优化/README.md)：** 把该架构专门化到 Qwen3-4B 与 Qwen2.5-72B 的 6 讲系列。


<details>
<summary>English original</summary>

**13. Key Takeaways**

| Takeaway | Why it matters |
|---|---|
| Attention removes the fixed-context bottleneck of RNNs | Every position can pull from every other position directly |
| Q/K/V is a three-role view of "look up by similarity, aggregate by softmax" | All attention math reduces to this |
| Scaled dot product divides by √d_k to prevent peaked softmax | This is the *fix*, not the cause — unscaled is too peaked, not too flat |
| Self-attention has no order — you need positional encoding | Without it, a transformer is a bag-of-words model |
| Multi-head attention runs many attention ops in parallel with their own projections | Heads can specialize; total parameter cost is similar to single-head |
| Padding mask zeros out attention to padding; causal mask blocks future positions | Mask is added to scores as `-inf`, then softmax does the rest |
| Modern LLMs are decoder-only with causal mask | One unified architecture for understanding and generation |
| The full block is RMSNorm → attention → ⊕ → RMSNorm → FFN → ⊕ | This pattern repeats `D` times in every transformer you'll deploy |
| Attention weights are clues, not full explanations | Useful for interpretability but never proof of "what the model decided" |
| Self-attention is `O(N²)` in sequence length | This is why long-context inference is hard; all advanced techniques exist to manage it |

---

**14. The One-Sentence Takeaway**

!!! quote ""
    Attention lets a neural network dynamically focus on the most relevant parts of a sequence by comparing queries to keys, using the resulting weights to combine values into context-aware representations — and a transformer is just `D` layers of multi-head attention plus FFN with residual connections, with a mask choice that determines whether it's an encoder, decoder, or encoder–decoder model.

---

**Resources**

* **["Attention Is All You Need" (Vaswani et al., 2017)](https://arxiv.org/abs/1706.03762):** The original transformer paper. Concise; read it twice.
* **[The Annotated Transformer (Harvard NLP)](https://nlp.seas.harvard.edu/2018/04/03/attention.html):** A line-by-line PyTorch implementation of the original Transformer paired with the paper text. The single best learning resource.
* **[The Illustrated Transformer (Jay Alammar)](https://jalammar.github.io/illustrated-transformer/):** The most accessible visual explainer; great for building first intuition.
* **["Transformers from Scratch" (Brandon Rohrer)](https://e2eml.school/transformers.html):** A second pass through the same material with different intuition.
* **[3Blue1Brown — Neural Networks chapter 5+ (Attention)](https://www.youtube.com/watch?v=eMlx5fFNoYc):** Video treatment with clean diagrams.
* **[Hugging Face Transformers — `modeling_qwen2.py`](https://github.com/huggingface/transformers/blob/main/src/transformers/models/qwen2/modeling_qwen2.py):** A real production-grade transformer implementation you can read top-to-bottom.
* **[PyTorch — `scaled_dot_product_attention` docs](https://pytorch.org/docs/stable/generated/torch.nn.functional.scaled_dot_product_attention.html):** The modern fused-attention primitive.
* **[FlashAttention paper (Dao et al., 2022)](https://arxiv.org/abs/2205.14135):** How to compute the same attention 5× faster by tiling. Not required for fundamentals, but the bridge to inference engineering.
* **[Phase 5 — Edge LLM Inference Internals](../../../阶段5-高级专题与专精/03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md):** The first downstream lecture that applies this material to real hardware.
* **[Phase 5 — Qwen Inference Optimization](../../../阶段5-高级专题与专精/03-方向C-边缘AI/05-Qwen推理优化/README.md):** The 6-lecture series that specializes this architecture to Qwen3-4B and Qwen2.5-72B.

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/1. Neural Networks/Transformer Fundamentals/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/1.%20Neural%20Networks/Transformer%20Fundamentals/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
