---
title: Lecture 07 - 超越 IID 的数据：序列与图
description: Lecture 07 - 超越 IID 的数据：序列与图
---

# Lecture 07 - 超越 IID 的数据：序列与图

**合集：** [Practical Machine Learning (CS329P)](README.md) | **上一讲：** [← Lecture 06](Lecture-06.md) | **下一讲：** [Lecture 08](Lecture-08.md)

---

几乎所有入门级的 ML 结论都默默假设你的数据是 **IID**——独立同分布。正是这个假设让你可以打乱行、切出随机的 20% 测试集，并相信由此得到的准确率数字。麻烦在于，大多数值得建模的数据*并不*是 IID。股票价格、传感器流、句子中的词、用户会话中的点击、支付网络上的交易、社交图中的好友关系——在每一种情形里，单个观测的取值都能告诉你关于其邻居的一些信息。样本是*相依*的：`p(x, y) ≠ p(x) · p(y)`。

相依性不是需要剔除的麻烦——它通常就是全部的信号。我们能从今天预测明天、自动补全句子、标记出欺诈团伙，原因恰恰*在于*观测之间彼此相关。危险在于假装相依性不存在。对时间序列做随机 train/test 划分会把未来泄漏到过去，并报告一个你在生产中永远见不到的准确率。对社交图做随机划分会把某个用户的好友同时放到划分的两侧，让模型直接背下答案。模型在离线时表现极好，上线后一败涂地。

本讲覆盖相依型数据的两大族类——**序列**（沿时间或 token 位置展开的一维相依链）与**图**（实体之间任意的关系相依）——以及用来判断你的数据到底是否存在相依的检验方法。贯穿始终的主题：一旦数据不是 IID，你的评估协议*和*模型架构*都*必须改变。划分搞错了，再好的架构也救不了你。

---

## 学习目标

学完本讲，你应当能够：

- 解释 IID 的含义，识别常见的非 IID 数据，并说明为什么相依会使随机 train/test 划分失效。
- 运用实用的独立性检验（置换/分类器检验、MMD/HSIC、互信息），并说明每种检验度量的是什么。
- 把序列问题以自回归方式表述为 `p(x_t | x_<t)`，并在经典模型、RNN/LSTM 与 Transformer 之间做出选择。
- 在不泄漏未来的前提下准备序列数据：滑窗、按时间顺序划分、teacher forcing、平稳性检验。
- 描述图数据（节点、边、特征）、节点/边/图级别的任务，以及 GCN 与 GraphSAGE 背后的消息传递直觉。
- 认识序列与图在真实系统中出现的位置，以及各自推理服务的硬件开销。

---

## 1. 独立性检验与 IID 为何失效

### “独立”能带给你什么

当联合分布可以分解为边缘分布的乘积时，两个随机变量相互独立：

```text
Independent:   p(x, y) = p(x) · p(y)
Dependent:     p(x, y) ≠ p(x) · p(y)
```

我们为什么在意这两者的区别？因为监督学习*正是*建立在相依之上。分类与回归估计的都是 `p(y | x) = p(x, y) / p(x)`。如果 `x` 与 `y` 真的相互独立，整个表达式就会退化为 `p(y | x) = p(y)`——输入什么也告诉不了你，也就没有东西可学。反过来，如果你*假定*独立的观测实际上彼此相依，那么你的采样与划分逻辑就建立在一个错误的前提之上。

### 非 IID 数据的例子

| 数据 | 相依结构 | 正确的划分 |
|------|----------------------|----------------|
| 股票 / 传感器时间序列 | 每个取值与其近期历史相关 | 按时间（用过去训练，用未来测试） |
| 文本 / 语言 | 每个 token 依赖之前的 token | 按文档，然后在文档内从左到右 |
| 用户点击流 / 会话 | 同一会话内的事件彼此相关 | 按用户 / 会话，而不是按事件 |
| 病历 | 每个患者有多行记录 | 按患者（分组划分） |
| 社交网络 | 好友之间标签相同（同质性） | 按社区 / 时间，而不是按节点 |
| 分子 | 原子成键构成结构 | 按骨架 / 分子 |


<details>
<summary>English original</summary>

**Lecture 07 - Data Beyond IID: Sequences & Graphs**

**Collection:** [Practical Machine Learning (CS329P)](README.md) | **Previous:** [← Lecture 06](Lecture-06.md) | **Next:** [Lecture 08](Lecture-08.md)

---

Almost every introductory ML result quietly assumes your data is **IID** — independent and identically distributed. That assumption is what lets you shuffle rows, carve out a random 20% test set, and trust the resulting accuracy number. The trouble is that most data worth modeling is *not* IID. Stock prices, sensor streams, words in a sentence, clicks in a user session, transactions on a payment network, friendships in a social graph — in every one of these, the value of one observation tells you something about its neighbors. The samples are *dependent*: `p(x, y) ≠ p(x) · p(y)`.

Dependence is not a nuisance to be removed — it is usually the entire signal. The reason we can forecast tomorrow from today, autocomplete a sentence, or flag a fraud ring is precisely *because* observations are correlated. The danger is pretending the dependence isn't there. A random train/test split on a time series leaks the future into the past and reports an accuracy you will never see in production. A random split on a social graph puts a user's friends on both sides of the split and lets the model memorize the answer. The model looks great offline and collapses live.

This lecture covers the two big families of dependent data — **sequences** (a 1-D chain of dependence through time or token position) and **graphs** (arbitrary relational dependence between entities) — plus the tests that tell you whether your data is dependent at all. The recurring theme: once data is non-IID, *both* your evaluation protocol *and* your model architecture have to change. Get the split wrong and no architecture saves you.

---

**Learning objectives**

By the end of this lecture you should be able to:

- Explain what IID means, recognize common non-IID data, and state why dependence invalidates a random train/test split.
- Apply practical tests for independence (permutation/classifier test, MMD/HSIC, mutual information) and reason about what each measures.
- Frame a sequence problem autoregressively as `p(x_t | x_<t)` and choose between classical models, RNN/LSTM, and Transformers.
- Prepare sequence data without leaking the future: windowing, time-ordered splits, teacher forcing, and stationarity checks.
- Describe graph data (nodes, edges, features), node/edge/graph-level tasks, and the message-passing intuition behind GCN and GraphSAGE.
- Recognize where sequences and graphs show up in real systems and the hardware cost of serving each.

---

**1. Independence tests & why IID breaks**

**What "independent" buys you**

Two random variables are independent when the joint factorizes into the product of marginals:

```text
Independent:   p(x, y) = p(x) · p(y)
Dependent:     p(x, y) ≠ p(x) · p(y)
```

Why do we care about the difference? Because supervised learning *lives* on dependence. Classification and regression both estimate `p(y | x) = p(x, y) / p(x)`. If `x` and `y` were truly independent, that whole expression would collapse to `p(y | x) = p(y)` — the input would tell you nothing, and there would be nothing to learn. Conversely, if observations you *assumed* were independent are actually dependent, your sampling and splitting logic is built on a false premise.

**Examples of non-IID data**

| Data | Dependence structure | Correct split |
|------|----------------------|----------------|
| Stock / sensor time series | Each value correlated with its recent past | By time (train on past, test on future) |
| Text / language | Each token depends on prior tokens | By document, then within-doc left-to-right |
| User clickstream / sessions | Events within a session are correlated | By user / session, not by event |
| Medical records | Multiple rows per patient | By patient (group split) |
| Social network | Friends share labels (homophily) | By community / time, not by node |
| Molecules | Atoms bonded into structure | By scaffold / molecule |

</details>

### 为什么随机划分会说谎

随机划分假设每一行都是独立抽取的，因此任何划分都给出同样可靠的估计。一旦打破这个假设，就会冒出两种失效模式：

- **时间泄漏。** 打乱一条时间序列，来自测试点*之后*的行就会落进训练集。模型实际上偷看了未来。离线准确率飙升；线上准确率却不会，因为在推理时未来根本还不存在。
- **分组/实体泄漏。** 把同一个患者、用户或社交邻域放在划分的两侧，模型就会记住实体特有的怪癖，而不是学到可泛化的规则。它是在自己实际上已经见过的数据上被打分。

经验法则：**沿着依赖的轴进行划分。** 按时间排序的数据按时间划分；分组数据按组划分；图数据按社区或按时间切分。绝不要让预测时无法获得的信息跨过训练/测试的边界。

### 检验依赖性

你常常*怀疑*存在依赖，却想确认它。有若干种检验方法，大致按复杂程度排列：

- **置换 / 分类器检验。** 取你真实的成对数据 `Z = {(x_i, y_i)}`。构造一个打乱后的副本 `Z' = {(x_i, y_π(i))}`，其中 `π` 随机置换标签——这会破坏任何真实的 `x`–`y` 关系，同时保持各自的边缘分布不变。训练一个分类器来区分 `Z` 和 `Z'`。如果它能做到，就说明这种配对携带信息，且 `x`、`y` 是相关的。（额外好处：分类器*最*有把握的那些配对，正是关联最强的那些。）同一个用来给“这是不是一个真实配对”打分的 `f(x, y)`，甚至可以通过 `ŷ = argmax_y f(x, y)` 变成预测器。
- **MMD（最大均值差异，Maximum Mean Discrepancy）。** 在 kernel 特征空间中，将联合期望 `E_{(x,y)}[φ(x)·φ(y)]` 与独立期望 `E_x E_y[φ(x)·φ(y)]` 作比较。差距大就意味着存在依赖。当特征映射无法因式分解时很有用。
- **HSIC（希尔伯特-施密特独立性准则，Hilbert-Schmidt Independence Criterion）。** 一种 kernel 协方差算子，当 `x ⊥ y` 时恰好为零。它化简为简洁的迹形式 `tr(H K H L)`，其中中心化矩阵为 `H_ij = δ_ij − 1/m`，kernel 矩阵 `K`、`L` 定义在 `x` 和 `y` 上。事实上现代非参数独立性检验的标准做法。
- **互信息。** 从信息论看，`I(x, y) = H[x] + H[y] − H[(x, y)]` 等于联合分布与边缘分布之积之间的 KL 散度——它计量的是分别编码 `x` 和 `y` 而非联合编码所需的*额外比特数*。如果数据独立，则 `I = 0`，无法省下任何比特。

```python
# Permutation (classifier) independence test — the practical workhorse.
import numpy as np
from sklearn.ensemble import GradientBoostingClassifier
from sklearn.model_selection import cross_val_score

def independence_pvalue(X, y, n_perms=200):
    real = np.column_stack([X, y])              # genuine (x, y) pairs
    def auc(Z):                                 # can a classifier spot real vs shuffled?
        Zp = np.column_stack([X, np.random.permutation(y)])
        data = np.vstack([Z, Zp])
        lab  = np.r_[np.ones(len(Z)), np.zeros(len(Zp))]
        return cross_val_score(GradientBoostingClassifier(), data, lab,
                               cv=3, scoring="roc_auc").mean()
    observed = auc(real)
    null = [auc(np.column_stack([X, np.random.permutation(y)]))   # AUC under H0
            for _ in range(n_perms)]
    # p = fraction of null AUCs at least as separable as observed
    return float((np.sum(np.asarray(null) >= observed) + 1) / (n_perms + 1))
# p small  -> reject independence -> data is dependent -> do NOT random-split.
```

---

## 2. 序列模型


<details>
<summary>English original</summary>

**Why a random split lies**

A random split assumes every row is an independent draw, so any partition gives an equally reliable estimate. Break that assumption and two failure modes appear:

- **Temporal leakage.** Shuffle a time series and rows from *after* the test point land in the training set. The model effectively peeks at the future. Offline accuracy soars; live accuracy does not, because at inference time the future genuinely does not exist yet.
- **Group/entity leakage.** Put the same patient, user, or social neighborhood on both sides of the split and the model memorizes entity-specific quirks instead of learning a generalizable rule. It is graded on data it has effectively already seen.

The rule of thumb: **split along the axis of dependence.** Time-ordered data splits by time; grouped data splits by group; graph data splits by community or by a time cut. Never let information cross the train/test boundary that wouldn't be available at prediction time.

**Testing for dependence**

You often *suspect* dependence but want to confirm it. Several tests, in rough order of sophistication:

- **Permutation / classifier test.** Take your real paired data `Z = {(x_i, y_i)}`. Build a shuffled copy `Z' = {(x_i, y_π(i))}` where `π` randomly permutes the labels — this destroys any real `x`–`y` relationship while preserving each marginal. Train a classifier to tell `Z` from `Z'`. If it can, the pairing carries information and `x`, `y` are dependent. (Bonus: the pairs the classifier is *most* confident about are your most strongly related ones.) The same `f(x, y)` that scores "is this a real pair" can even be turned into a predictor via `ŷ = argmax_y f(x, y)`.
- **MMD (Maximum Mean Discrepancy).** Compare the joint expectation `E_{(x,y)}[φ(x)·φ(y)]` against the independent expectation `E_x E_y[φ(x)·φ(y)]` in a kernel feature space. A large gap means dependence. Useful when feature maps don't factorize.
- **HSIC (Hilbert-Schmidt Independence Criterion).** A kernel covariance operator that vanishes exactly when `x ⊥ y`. It reduces to the clean trace form `tr(H K H L)` with centering matrix `H_ij = δ_ij − 1/m` and kernel matrices `K`, `L` over `x` and `y`. The de-facto modern nonparametric independence test.
- **Mutual information.** From information theory, `I(x, y) = H[x] + H[y] − H[(x, y)]` equals the KL divergence between the joint and the product of marginals — it counts the *extra bits* needed to encode `x` and `y` separately rather than jointly. If the data is independent, `I = 0` and no bits can be saved.

```python
# Permutation (classifier) independence test — the practical workhorse.
import numpy as np
from sklearn.ensemble import GradientBoostingClassifier
from sklearn.model_selection import cross_val_score

def independence_pvalue(X, y, n_perms=200):
    real = np.column_stack([X, y])              # genuine (x, y) pairs
    def auc(Z):                                 # can a classifier spot real vs shuffled?
        Zp = np.column_stack([X, np.random.permutation(y)])
        data = np.vstack([Z, Zp])
        lab  = np.r_[np.ones(len(Z)), np.zeros(len(Zp))]
        return cross_val_score(GradientBoostingClassifier(), data, lab,
                               cv=3, scoring="roc_auc").mean()
    observed = auc(real)
    null = [auc(np.column_stack([X, np.random.permutation(y)]))   # AUC under H0
            for _ in range(n_perms)]
    # p = fraction of null AUCs at least as separable as observed
    return float((np.sum(np.asarray(null) >= observed) + 1) / (n_perms + 1))
# p small  -> reject independence -> data is dependent -> do NOT random-split.
```

---

**2. Sequence models**

</details>

### 自回归框架

序列即观测 `x_1, x_2, …, x_T`，其中顺序很重要。根据概率的链式法则，联合分布*总是*可以分解：

```text
p(x_1, …, x_T) = p(x_1) · p(x_2 | x_1) · p(x_3 | x_1, x_2) · … · p(x_T | x_1, …, x_{T-1})
```

这就是**自回归**视角：每一步都根据它之前的全部内容来预测，`x_t ∼ p(x_t | x_<t)`。由于因果性，沿时间*向前*分解比（数学上同样成立的）向后分解更准确——是过去真正生成了未来，而不是相反。

以*全部*过去作为条件代价高昂，且往往并不必要。两种标准简化：

- **加窗（马尔可夫假设）。** 只以最近 `τ` 步为条件：`p(x_t | x_{t-τ}, …, x_{t-1})`。Takens 定理指出，在温和正则性下，近期过去的有限窗口就已携带足够的信息。随后训练一个普通回归器，目标为 `x_t`，特征为 `(x_{t-τ}, …, x_{t-1})`。这依赖**平稳性**——即假设条件分布取决于窗口中的*取值*，而不取决于绝对时间 `t`。
- **隐状态。** 当长历史重要时，将其概括为一个隐状态，而非携带原始过去：`h_t = g(x_{t-1}, h_{t-1})`，`x_t = f(x_{t-1}, h_t)`。状态 `h_t` 是对此前全部内容的一种学习得到的、固定大小的记忆。

### 模型阶梯

| 模型 | 历史机制 | 优点 | 缺点 |
|-------|-------------------|-----------|------------|
| AR / ARIMA | 固定线性窗口 | 简单、可解释、强基线 | 线性；假设平稳性 |
| 马尔可夫链 | 最近 `k` 个离散状态 | 廉价、透明 | 状态空间随 `k` 爆炸 |
| RNN | 循环隐状态 | 原则上上下文无界 | 梯度消失；慢（串行） |
| LSTM / GRU | 门控记忆单元 | 保留长程信号 | 仍为串行；截断 BPTT |
| Transformer | 所有位置上的自注意力 | 并行训练、长程、SOTA | O(T²) attention；内存大 |

经典这一端（ARIMA、马尔可夫）是*起点*——它给出一个你必须超越的基线。**RNN** 增加了学习得到的状态，但训练缓慢，因为每一步都依赖上一步。**LSTM**（Hochreiter & Schmidhuber，'97）模拟一个带输入/遗忘/输出门的电路记忆单元，使梯度能在长链中存活；**GRU**（Cho et al.，'14）是更廉价、稍弱的变体。实践中，深而简单胜过浅而复杂，训练开销大（沿长链反向传播），且框架默认截断梯度。

```text
LSTM cell (the gated memory device):
  i_t = σ(W_i · [x_t, h_{t-1}] + b_i)              # input gate
  f_t = σ(W_f · [x_t, h_{t-1}] + b_f)              # forget gate
  o_t = σ(W_o · [x_t, h_{t-1}] + b_o)              # output gate
  c_t = f_t ⊙ c_{t-1} + i_t ⊙ tanh(W_c·[x_t,h_{t-1}] + b_c)   # cell state
  h_t = o_t ⊙ tanh(c_t)                            # hidden state
```

**Transformer** 用 attention 取代了循环。Seq2seq（Sutskever et al.，'14）先用 LSTM 把源句子编码为一个固定向量 `φ(s)`，再逐 token 地 decode（逐 token 生成阶段）——这在长输入上会失效，因为单个向量不够丰富（“The table is round” 有效；一段长文本则产生 “Error…”）。加入 **attention**（Bahdanau et al.，'14）让解码器可以通过 query/key/value 权重 `α_ij ∝ exp(a(h̃_{i-1}, h_j))` 回看任意源位置，从而消除了瓶颈。完全去掉循环便得到了 Transformer：并行训练、embedding 天然的双向上下文，以及所有现代序列模型的基础（Lecture 10++ 中有深入讲解）。

### 序列任务

- **预测** —— 根据过去预测未来取值 `x_{t+1}, …, x_T`（需求、负载、价格）。
- **标注 / 打标签** —— 每个输入位置一个输出（词性、命名实体识别、异常标记）。
- **Seq2seq** —— 将输入序列映射为长度不同的输出序列（翻译、摘要、语音转文本）。


<details>
<summary>English original</summary>

**The autoregressive framing**

A sequence is observations `x_1, x_2, …, x_T` where order matters. By the chain rule of probability, the joint *always* factorizes:

```text
p(x_1, …, x_T) = p(x_1) · p(x_2 | x_1) · p(x_3 | x_1, x_2) · … · p(x_T | x_1, …, x_{T-1})
```

This is the **autoregressive** view: predict each step from everything before it, `x_t ∼ p(x_t | x_<t)`. Because of causality, decomposing *forward* in time is more accurate than the (mathematically valid) backward decomposition — the past genuinely generates the future, not the reverse.

Conditioning on the *entire* past is expensive and often unnecessary. Two standard simplifications:

- **Windowing (Markov assumption).** Condition only on the last `τ` steps: `p(x_t | x_{t-τ}, …, x_{t-1})`. Takens' theorem says that, under mild regularity, a finite window of the recent past carries enough information. You then train an ordinary regressor with target `x_t` and features `(x_{t-τ}, …, x_{t-1})`. This relies on **stationarity** — the assumption that the conditional distribution depends on the *values* in the window, not on the absolute time `t`.
- **Latent state.** When a long history matters, summarize it into a hidden state instead of carrying the raw past: `h_t = g(x_{t-1}, h_{t-1})`, `x_t = f(x_{t-1}, h_t)`. The state `h_t` is a learned, fixed-size memory of everything that came before.

**The model ladder**

| Model | History mechanism | Strengths | Weaknesses |
|-------|-------------------|-----------|------------|
| AR / ARIMA | Fixed linear window | Simple, interpretable, strong baseline | Linear; assumes stationarity |
| Markov chain | Last `k` discrete states | Cheap, transparent | State space explodes with `k` |
| RNN | Recurrent hidden state | Unbounded context in principle | Vanishing gradients; slow (sequential) |
| LSTM / GRU | Gated memory cell | Keeps long-range signal | Still sequential; truncated BPTT |
| Transformer | Self-attention over all positions | Parallel training, long range, SOTA | O(T²) attention; large memory |

The classical end (ARIMA, Markov) is the place to *start* — it gives a baseline you must beat. **RNNs** add a learned state but train slowly because each step depends on the previous one. **LSTM** (Hochreiter & Schmidhuber, '97) mimics a circuit memory cell with input/forget/output gates so gradients survive long chains; **GRU** (Cho et al., '14) is a cheaper, slightly weaker variant. In practice, deep-and-simple beats shallow-and-complex, training is expensive (backprop through a long chain), and frameworks truncate the gradient by default.

```text
LSTM cell (the gated memory device):
  i_t = σ(W_i · [x_t, h_{t-1}] + b_i)              # input gate
  f_t = σ(W_f · [x_t, h_{t-1}] + b_f)              # forget gate
  o_t = σ(W_o · [x_t, h_{t-1}] + b_o)              # output gate
  c_t = f_t ⊙ c_{t-1} + i_t ⊙ tanh(W_c·[x_t,h_{t-1}] + b_c)   # cell state
  h_t = o_t ⊙ tanh(c_t)                            # hidden state
```

**Transformers** replaced the recurrence with attention. Seq2seq (Sutskever et al., '14) first encoded a source sentence into one fixed vector `φ(s)` with an LSTM and decoded it token by token — which broke on long inputs because a single vector isn't rich enough ("The table is round" worked; a long paragraph produced "Error…"). Adding **attention** (Bahdanau et al., '14) let the decoder look back at any source position via query/key/value weights `α_ij ∝ exp(a(h̃_{i-1}, h_j))`, removing the bottleneck. Dropping the recurrence entirely gave the Transformer: parallel training, natural bidirectional context for embeddings, and the foundation of every modern sequence model (covered in depth in Lecture 10++).

**Sequence tasks**

- **Forecasting** — predict future values `x_{t+1}, …, x_T` from the past (demand, load, prices).
- **Tagging / labeling** — one output per input position (part-of-speech, named-entity recognition, anomaly flags).
- **Seq2seq** — map an input sequence to an output sequence of different length (translation, summarization, speech-to-text).

</details>

### 数据准备的坑：不要用未来预测过去

这正是多数序列项目在还没选模型之前就失败的地方。

- **不要跨时间打乱。** IID 数据可以随机划分，每一折都同样可靠。依赖数据则必须尊重顺序：**在 `x_1, …, x_t` 上训练，在 `x_{t+1}, …, x_T` 上评测。** 打乱会泄漏未来。
- **切窗口而不泄漏。** 用滑动窗口构造 `(features, target)` 对，并确保每个特征时间戳都严格*早于*目标。scaler、encoder、imputer 必须只在训练窗口上 fit——在完整序列上 fit 会泄漏未来的统计量。
- **留意平稳性与漂移。** 使用全部历史数据的前提是平稳性。真实数据会漂移：**概念漂移**（COVID 把支出从外出就餐转移到耐用商品）、**季节性**（每年圣诞节），以及一次性的**非平稳**（新产品发布）。有时漂移有外部原因——把它作为条件（给定天气的雨伞销量），残差就会更接近独立。
- **Teacher forcing 与 student forcing。** 训练时喂入*真实*的前一个 token（teacher forcing），使模型始终距离 ground truth 只有一步。推理时它必须消费*自己*的预测（student forcing），小误差会累积——自回归 rollout 可能迅速发散。scheduled sampling，以及生成式训练（GAN/VAE 风格的目标函数），有助于弥合这一 train/serve 差距。

```python
# Time-ordered split + windowing — never shuffle a time series.
import numpy as np

def make_windows(series, tau, horizon=1):
    X, y = [], []
    for t in range(tau, len(series) - horizon + 1):
        X.append(series[t - tau:t])          # features: strictly the past
        y.append(series[t + horizon - 1])    # target:   a future step
    return np.array(X), np.array(y)

cut = int(len(series) * 0.8)                 # chronological cut, NOT random
train, test = series[:cut], series[cut:]     # past trains, future tests
mu, sd = train.mean(), train.std()           # fit scaler on TRAIN ONLY
train, test = (train - mu) / sd, (test - mu) / sd   # no future leakage
Xtr, ytr = make_windows(train, tau=24)
Xte, yte = make_windows(test,  tau=24)
```

---

## 3. 图

有些依赖关系根本无法压平成序列。路网、支付图、引用网络、“谁关注谁”的社交网络——它们具有任意的关系结构。最干净的分解方式是图模型——团（clique）上的有向 `p(x) = ∏_i p(x_i | x_{π(i)})` 或无向 `p(x) = ∏_C ψ_C(x_C)`——但在其上做精确推理往往不可解，因此现代做法直接学习顶点表示。

### 图数据

图是 `G(V, E)`：

- **顶点（节点）** `i ∈ V`，每个可选地带一个特征向量 `x_i`。
- **边** `(i, j) ∈ E`，每条可选地带一个特征向量 `x_ij`。

### 任务层级

| 层级 | 问题 | 示例 |
|-------|----------|----------|
| 节点 | 给未知顶点打标签 | 欺诈检测、用户分类 |
| 边 | 预测缺失 / 未来的边 | 链接预测、推荐 |
| 图 | 整个图一个标签 | 分子属性、毒性 |

经典的节点任务：给定*部分*顶点的标签，推断其余顶点的标签（欺诈）。经典的边任务：给定部分边属性，预测缺失的那些边（链接推荐）。

### 消息传递——GNN 的直觉

核心思想早于深度学习出现。**Weisfeiler-Lehman** 算法（1976）通过*反复把每个顶点与其邻居一起做哈希*直到标签稳定，从而使顶点唯一化——而这些哈希结果恰恰是极好的结构特征。**PageRank**（Page & Brin，90 年代）做的是同一形态的计算：每个页面的分数是其邻居分数的函数，迭代到不动点。两者都是**局部更新**规则：节点的新值是其邻域的函数。

**图神经网络**把这一更新变得*可学习*。每一 layer，每个节点：

1. 从其邻居**收集**特征向量（*message*），
2. 用一个置换不变函数**聚合**它们——sum、mean 或 max（一个“定义在集合上的函数”），
3. 把聚合结果与自身当前状态结合来**更新**自身表示，然后施加一个学到的变换。

```text
Message passing, one GNN layer:
  m_i      = AGGREGATE_{j ∈ N(i)}  message(h_j, x_ij)     # gather from neighbors
  h_i'     = UPDATE(h_i, m_i)                              # combine + learned transform

After k layers, each node "sees" its k-hop neighborhood.
```

堆叠 `k` 个 layer，信息流动 `k` 跳：一个节点的表示吸收其整个 `k` 跳邻域。这正是 GNN 把原始节点特征加上图结构变成 embedding 的方式，这些 embedding 可用于分类或链接预测。


<details>
<summary>English original</summary>

**The data-prep gotcha: don't train on the future to predict the past**

This is where most sequence projects fail before the model is even chosen.

- **No shuffling across time.** With IID data you random-partition and every fold is equally reliable. With dependent data you must respect order: **train on `x_1, …, x_t`, evaluate on `x_{t+1}, …, x_T`.** Shuffling leaks the future.
- **Windowing without leakage.** Build `(features, target)` pairs from a sliding window, and make sure every feature timestamp is strictly *before* the target. Scalers, encoders, and imputers must be fit on the training window only — fitting on the full series leaks future statistics.
- **Watch stationarity and drift.** Using all past history assumes stationarity. Real data drifts: **concept shift** (COVID shifted spending from dining out to durable goods), **seasonality** (Christmas every year), and one-off **nonstationarity** (a new product launch). Sometimes the drift has an external cause — condition on it (umbrella sales given weather) and the residual becomes closer to independent.
- **Teacher vs. student forcing.** During training, feed the *true* previous token (teacher forcing) so the model is always one step from ground truth. At inference it must consume its *own* predictions (student forcing), and small errors compound — autoregressive rollouts can diverge rapidly. Scheduled sampling, and generative training (GAN/VAE-style objectives), help close this train/serve gap.

```python
# Time-ordered split + windowing — never shuffle a time series.
import numpy as np

def make_windows(series, tau, horizon=1):
    X, y = [], []
    for t in range(tau, len(series) - horizon + 1):
        X.append(series[t - tau:t])          # features: strictly the past
        y.append(series[t + horizon - 1])    # target:   a future step
    return np.array(X), np.array(y)

cut = int(len(series) * 0.8)                 # chronological cut, NOT random
train, test = series[:cut], series[cut:]     # past trains, future tests
mu, sd = train.mean(), train.std()           # fit scaler on TRAIN ONLY
train, test = (train - mu) / sd, (test - mu) / sd   # no future leakage
Xtr, ytr = make_windows(train, tau=24)
Xte, yte = make_windows(test,  tau=24)
```

---

**3. Graphs**

Some dependence simply cannot be flattened into a sequence. A road network, a payment graph, a citation web, a "who-follows-whom" social network — these have arbitrary relational structure. The clean factorizations are graphical models — directed `p(x) = ∏_i p(x_i | x_{π(i)})` or undirected `p(x) = ∏_C ψ_C(x_C)` over cliques — but exact inference on them is often intractable, so modern practice learns vertex representations directly.

**Graph data**

A graph is `G(V, E)`:

- **Vertices (nodes)** `i ∈ V`, each optionally with a feature vector `x_i`.
- **Edges** `(i, j) ∈ E`, each optionally with a feature vector `x_ij`.

**Task levels**

| Level | Question | Examples |
|-------|----------|----------|
| Node | Label the unknown vertices | Fraud detection, user classification |
| Edge | Predict missing / future edges | Link prediction, recommendation |
| Graph | One label for the whole graph | Molecular property, toxicity |

A classic node task: given labels on *some* vertices, infer the rest (fraud). A classic edge task: given some edge attributes, predict the missing ones (link recommendation).

**Message passing — the GNN intuition**

The core idea predates deep learning. The **Weisfeiler-Lehman** algorithm (1976) makes vertices unique by *repeatedly hashing each vertex together with its neighbors* until the labels stabilize — and those hashes turn out to be excellent structural features. **PageRank** (Page & Brin, '90s) does the same shape of computation: each page's score is a function of its neighbors' scores, iterated to a fixed point. Both are **local update** rules: a node's new value is a function of its neighborhood.

**Graph Neural Networks** make that update *learnable*. Each layer, every node:

1. **collects** feature vectors from its neighbors (the *message*),
2. **aggregates** them with a permutation-invariant function — sum, mean, or max (a "function on a set"),
3. **updates** its own representation by combining the aggregate with its current state, then applies a learned transform.

```text
Message passing, one GNN layer:
  m_i      = AGGREGATE_{j ∈ N(i)}  message(h_j, x_ij)     # gather from neighbors
  h_i'     = UPDATE(h_i, m_i)                              # combine + learned transform

After k layers, each node "sees" its k-hop neighborhood.
```

Stack `k` layers and information flows `k` hops: a node's representation absorbs its entire `k`-hop neighborhood. That is exactly how a GNN turns raw node features plus graph structure into embeddings you can classify or use for link prediction.

</details>

### GCN 与 GraphSAGE

- **GCN**（Kipf & Welling，2016）——Graph *Convolutional* Network。每一层用**度归一化求和**聚合邻居特征，再施加线性映射与非线性。它是 message-passing GNN 的经典形式：简单、强大，且是标准基线。
- **GraphSAGE**（Hamilton et al.，2017）——**SA**mple-and-aggrer**G**at**E**。它不使用*整个*邻域（在十亿条边的图上不可能做到），而是为每个节点**采样固定数量的邻居**，并学习一个聚合器（mean / pooling / LSTM）。采样使其可扩展，并支持对训练时从未见过的节点做**归纳式**预测——在生产环境中图持续增长，这一点至关重要。
- **GAT**（Veličković et al.，2017）——**G**raph **AT**tention。它用学到的 attention `α_ij` 为每个邻居加权，而非固定归一化，因此最相关的邻居占主导。

```python
# A GCN layer in PyTorch Geometric — message passing in a few lines.
import torch
import torch.nn.functional as F
from torch_geometric.nn import GCNConv

class GCN(torch.nn.Module):
    def __init__(self, in_feats, hidden, num_classes):
        super().__init__()
        self.conv1 = GCNConv(in_feats, hidden)        # aggregate 1-hop neighborhood
        self.conv2 = GCNConv(hidden, num_classes)     # aggregate 2-hop neighborhood

    def forward(self, x, edge_index):
        x = F.relu(self.conv1(x, edge_index))         # gather + transform + nonlinearity
        return self.conv2(x, edge_index)              # logits per node

# Semi-supervised: loss is computed on labeled nodes only; the graph
# propagates that signal to the unlabeled ones via message passing.
```

实践中的一个注意事项（Dai et al.，2018）：对整个图做反向传播开销很大——"六度分隔"意味着几跳就会触及整张图，而图上 BPTT 会爆炸。解决办法包括：用不动点迭代替代深度展开，以及每步**只采样一小部分顶点更新**（GraphSAGE 的技巧）。

### 图出现在哪里

- **推荐系统**——用户、物品与商家构成二分图/异构图；推荐即边预测。
- **欺诈 / 反滥用**——欺诈团伙表现为可疑子图；带有强关系信号的节点分类。
- **分子与材料**——原子为节点，化学键为边；图级性质预测。
- **社交与信息网络**——meme/假新闻的传播、影响力与社区发现。

> **硬件视角：** 这两类数据以相反的方式给硬件施压。**序列推理受内存带宽约束。** 自回归 decoder 一次生成一个 token（decode，逐 token 生成阶段），并在每一步重新读取 **KV cache**——即此前每个 token 对应的已存 key/value——因此吞吐取决于把该 cache 从 HBM 流出的速度，而非原始 FLOPs。cache 随上下文长度线性增长，并主导内存占用。**GNN 是稀疏、不规则内存的工作负载。** 聚合邻居特征，就是对大型顶点表做分散的、数据相关的索引——与 GPU 钟爱的稠密、规则 matmul 截然相反。性能受限于随机访问内存带宽和糟糕的缓存局部性；mini-batch 邻居采样（GraphSAGE）部分就是为了驯服这一点。二者都推动软硬件协同设计——KV cache 分页、量化与稀疏 gather kernel——这些内容在 [阶段 5 — MLSys Deep Dives](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/README.md) 中讨论。

> **2026 更新：** Transformer 已决定性地赢得通用序列建模——RNN 和 LSTM 如今大多只是遗留方案，或边缘/流式场景中的细分选择。对于*极长*上下文，**状态空间模型（Mamba / S4 及其后继）**提供了线性时间、常数内存的方案，替代二次复杂度的 attention，并以 attention-SSM 混合栈的形式落地。对于小而干净的低频预测任务，经典 **ARIMA/Prophet 仍然胜出**——那种场景下 Transformer 会过拟合。**GNN 仍是专门工具**——但很强——主导分子/材料 ML、药物发现、推荐检索与欺诈/反滥用，其中**时序图网络（TGN 式）**处理随时间演化的图（本讲两半的自然融合）。打破 IID 的教训——按时间/实体划分，绝不泄漏未来——没有改变，并且仍是"离线表现极好、上线就崩"最常见的单一原因。

---

## 内容截至

2026 年 6 月。模型推荐（Transformer、用于长上下文的 SSM/Mamba、GCN/GraphSAGE/GAT、时序图网络）反映的是截至该日期的格局；独立性与泄漏原则则不受时间影响。

*改编自 [Stanford CS329P](https://c.d2l.ai/stanford-cs329p) — Huang, Li & Smola, CC-BY-SA-4.0。*


<details>
<summary>English original</summary>

**GCN and GraphSAGE**

- **GCN** (Kipf & Welling, 2016) — Graph *Convolutional* Network. Each layer aggregates neighbor features with a **degree-normalized sum**, applies a linear map and a nonlinearity. It is the canonical message-passing GNN: simple, strong, and the standard baseline.
- **GraphSAGE** (Hamilton et al., 2017) — **SA**mple-and-aggrer**G**at**E**. Instead of using the *whole* neighborhood (impossible on a billion-edge graph), it **samples a fixed number of neighbors** per node and learns an aggregator (mean / pooling / LSTM). Sampling makes it scale and enables **inductive** prediction on nodes never seen at training time — essential in production where the graph keeps growing.
- **GAT** (Veličković et al., 2017) — **G**raph **AT**tention. Weights each neighbor with learned attention `α_ij` instead of a fixed normalization, so the most relevant neighbors dominate.

```python
# A GCN layer in PyTorch Geometric — message passing in a few lines.
import torch
import torch.nn.functional as F
from torch_geometric.nn import GCNConv

class GCN(torch.nn.Module):
    def __init__(self, in_feats, hidden, num_classes):
        super().__init__()
        self.conv1 = GCNConv(in_feats, hidden)        # aggregate 1-hop neighborhood
        self.conv2 = GCNConv(hidden, num_classes)     # aggregate 2-hop neighborhood

    def forward(self, x, edge_index):
        x = F.relu(self.conv1(x, edge_index))         # gather + transform + nonlinearity
        return self.conv2(x, edge_index)              # logits per node

# Semi-supervised: loss is computed on labeled nodes only; the graph
# propagates that signal to the unlabeled ones via message passing.
```

A practical caveat (Dai et al., 2018): backprop through a whole graph is expensive — "six degrees of separation" means a few hops touch the entire graph, and BPTT-over-graph blows up. Fixes include fixed-point iteration in place of deep unrolling and **sampling a small subset of vertex updates** per step (the GraphSAGE trick).

**Where graphs appear**

- **Recommenders** — users, items, and vendors as a bipartite/heterogeneous graph; recommendation is edge prediction.
- **Fraud / anti-abuse** — fraud rings show up as suspicious subgraphs; node classification with strong relational signal.
- **Molecules & materials** — atoms as nodes, bonds as edges; graph-level property prediction.
- **Social & information networks** — meme/fake-news spread, influence, and community detection.

> **Hardware lens:** the two data families stress hardware in opposite ways. **Sequence inference is memory-bandwidth-bound.** An autoregressive decoder generates one token at a time and re-reads the **KV cache** — the stored keys/values for every prior token — at every step, so throughput is gated by how fast you can stream that cache out of HBM, not by raw FLOPs. The cache grows linearly with context length and dominates memory. **GNNs are sparse, irregular-memory workloads.** Gathering neighbor features is scattered, data-dependent indexing into large vertex tables — the antithesis of the dense, regular matmuls GPUs love. Performance is bound by random-access memory bandwidth and poor cache locality; mini-batch neighbor sampling (GraphSAGE) exists partly to tame this. Both motivate the hardware-software co-design — KV-cache paging, quantization, and sparse-gather kernels — covered in [Phase 5 — MLSys Deep Dives](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/README.md).

> **2026 update:** Transformers have decisively won general sequence modeling — RNNs and LSTMs are now mostly legacy or edge/streaming niches. For *very long* context, **state-space models (Mamba / S4 and successors)** offer linear-time, constant-memory alternatives to quadratic attention and ship in hybrid attention-SSM stacks. Classical **ARIMA/Prophet still win on small, clean, low-frequency forecasts** where a Transformer would overfit. **GNNs remain a specialist tool** — but a strong one — dominating molecular/materials ML, drug discovery, recommender retrieval, and fraud/anti-abuse, with **temporal graph networks (TGN-style)** handling graphs that evolve over time (the natural fusion of this lecture's two halves). The IID-breaking lessons — split by time/entity, never leak the future — are unchanged and remain the single most common cause of "great offline, broken in production."

---

**Current as of**

June 2026. Model recommendations (Transformers, SSMs/Mamba for long context, GCN/GraphSAGE/GAT, temporal graph nets) reflect the landscape as of this date; the independence and leakage principles are timeless.

*Adapted from [Stanford CS329P](https://c.d2l.ai/stanford-cs329p) — Huang, Li & Smola, CC-BY-SA-4.0.*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/4. ML Engineering and MLOps/Practical Machine Learning (CS329P)/Lecture-07.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/4.%20ML%20Engineering%20and%20MLOps/Practical%20Machine%20Learning%20%28CS329P%29/Lecture-07.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
