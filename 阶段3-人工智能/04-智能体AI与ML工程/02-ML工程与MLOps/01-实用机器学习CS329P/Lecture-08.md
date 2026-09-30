---
title: Lecture 08 - 模型与超参数调优：HPO、NAS、深度网络调优
description: Lecture 08 - 模型与超参数调优：HPO、NAS、深度网络调优
---

# Lecture 08 - 模型与超参数调优：HPO、NAS、深度网络调优

**Collection:** [Practical Machine Learning (CS329P)](README.md) | **Previous:** [← Lecture 07](Lecture-07.md) | **Next:** [Lecture 09](Lecture-09.md)

---

调优是算力撞上收益递减的地方。你有一个能拟合的模型，一套你信得过的验证方案（Lecture 04–05），如今面前是一面布满旋钮的控制台：学习率、批大小、权重衰减、层数、通道宽度、优化器选择、dropout、warmup 调度。每个旋钮都有一个合理取值区间，这些区间相乘构成一个随旋钮数量指数增长的搜索空间，而每评测一个点都要付出一次完整训练。本讲要练的纪律 *不是*「找出最佳超参数」——那是目标，不是技能。技能在于**明智地花掉固定的搜索预算**，并提前知道哪一类改动——架构、超参数还是训练技巧——真正能推动你的问题向前，从而不必烧掉一千 GPU-hours 才发现你的学习率本来就没问题。

CS329P 用一条自 2021 年以来只会愈发尖锐的成本曲线来框定这件事：**每次试验的算力成本指数下降，人力成本上升。** 一名数据科学家的成本 >\$500/天；典型任务上的一次试验是几分钟到一小时的 CPU/GPU 时间，成本从几美分到几美元。当自动调优器在约 1000 次试验后超过人类——一个像样的调优器能胜过大约 90% 的数据科学家——经济账就说明：把搜索自动化，把人力花在搜索做不到的部分上：界定目标、设计搜索空间、解读结果。这正是 **AutoML** 的全部前提：把应用 ML 的每一步自动化——数据清洗、特征提取、模型选择——其中**超参数优化（HPO）**与**神经架构搜索（NAS）**是发展最成熟的两根支柱。

本讲分三个部分展开，然后是第四个部分。HPO：搜索算法，从简单得令人尴尬到真正聪明。NAS：搜索*架构本身*，它短暂的高光时刻与诚实的衰落。深度网络调优工具箱：三项结构性发明——normalization、residual、attention——它们对深度网络的贡献超过任何调优器所能做到的，因为它们从一开始就改变了什么*可训练*。而贯穿始终的是 2026 年的现实检验：这个领域重新洗了牌，今天的正确默认做法已不是这些幻灯片写下时的样子。

---

## 学习目标

学完本讲后，你应当能够：

1. **对超参数分类**，并决定哪些手工调、哪些自动化、哪些留在精心挑选的基线值上。
2. **选择 HPO 算法**——grid、random、Bayesian optimization、Hyperband/ASHA、BOHB——并根据预算与并行度而非习惯来论证这一选择。
3. **解释为什么 random search 胜过 grid search**，以及为什么在大多数配置都很差时 multi-fidelity 方法胜过两者。
4. **描述 NAS 流水线**——搜索空间、搜索策略、性能估计——并解释那个让朴素 NAS（约 2000 GPU-days）不切实际、从而必须采用 one-shot 方法的成本问题。
5. **对比 batch norm 与 layer norm**，解释为什么 Transformer 上 LN 胜出，并解释残差连接如何让极深的网络仍然可训练。
6. **对硬件感知调优进行推理**——以延迟为目标的 NAS、norm/残差对 kernel 融合的影响——并诚实地说明 NAS 在 2026 年的位置。

---

## 1. 模型调优概览


<details>
<summary>English original</summary>

**Lecture 08 - Model & Hyperparameter Tuning: HPO, NAS, Deep-Net Tuning**

**Collection:** [Practical Machine Learning (CS329P)](README.md) | **Previous:** [← Lecture 07](Lecture-07.md) | **Next:** [Lecture 09](Lecture-09.md)

---

Tuning is where compute meets diminishing returns. You have a model that fits, a validation scheme that you trust (Lectures 04–05), and now a knob-filled console: learning rate, batch size, weight decay, layer counts, channel widths, optimizer choice, dropout, warmup schedule. Each knob has a plausible range, the ranges multiply into a search space that is exponential in the number of knobs, and every point you evaluate costs a full training run. The discipline of this lecture is *not* "find the best hyperparameters" — that's the goal, not the skill. The skill is **spending a fixed search budget wisely**, and knowing in advance which family of changes — architecture, hyperparameters, or training tricks — is actually going to move the needle for your problem, so you don't burn a thousand GPU-hours discovering that your learning rate was already fine.

CS329P frames this through a cost curve that has only sharpened since 2021: **compute cost per trial falls exponentially, human cost rises.** A data scientist costs >\$500/day; a trial on a typical task is minutes-to-an-hour of CPU/GPU time costing cents to a few dollars. The moment an automated tuner beats a human after ~1000 trials — and a decent one beats roughly 90% of data scientists — the economics say automate the search and spend the human on the parts a search can't do: framing the objective, designing the search space, and reading the results. This is the entire premise of **AutoML**: automate every step of applying ML — data cleaning, feature extraction, model selection — with **Hyperparameter Optimization (HPO)** and **Neural Architecture Search (NAS)** as its two best-developed pillars.

We teach this in three movements and then a fourth. HPO: the search algorithms, from the embarrassingly simple to the genuinely clever. NAS: searching the *architecture itself*, its brief moment of glory and its honest decline. The deep-network tuning toolkit: the three structural inventions — normalization, residuals, attention — that did more for deep nets than any tuner ever could, because they changed what was *trainable* in the first place. And throughout, the 2026 reality check: the field re-sorted itself, and the right default today is not what it was when these slides were written.

---

**Learning objectives**

By the end of this lecture you should be able to:

1. **Classify hyperparameters** and decide what to tune manually, what to automate, and what to leave at a well-chosen baseline.
2. **Choose an HPO algorithm** — grid, random, Bayesian optimization, Hyperband/ASHA, BOHB — and justify it from your budget and parallelism, not from habit.
3. **Explain why random search beats grid search**, and why multi-fidelity methods beat both when most configurations are bad.
4. **Describe the NAS pipeline** — search space, search strategy, performance estimation — and explain the cost problem that made naive NAS (~2000 GPU-days) impractical and one-shot methods necessary.
5. **Contrast batch norm and layer norm**, explain why LN won for transformers, and explain how residual connections keep very deep nets trainable.
6. **Reason about hardware-aware tuning** — latency-targeted NAS, the effect of norms/residuals on kernel fusion — and state honestly where NAS sits in 2026.

---

**1. Model tuning overview**

</details>

### 1.1 你实际在调的是什么

超参数是训练之前*你*设定的值，与之相对的是由数据通过梯度下降填充的参数。它们粗略分为几个族，而族别决定了该如何对待其中的每一个：

| 族 | 示例 | 性质 |
|---|---|---|
| **优化** | 学习率、批大小、动量、权重衰减、warmup/调度 | 连续或对数连续；通常是杠杆最高的旋钮 |
| **架构** | #layers、#channels/width、kernel 大小、#hidden units、#attention heads | 离散/类别型；改动代价高（需完整重训） |
| **正则化** | dropout 率、label smoothing、增强强度 | 连续；与数据集大小强交互 |
| **容量 vs 成本** | 模型大小、序列长度、分辨率 | 决定*其他每一个*试验的成本 |

CS329P 给出的务实第一步是**从一个好的基线出发** —— 采用高质量工具包的默认设置，或类似任务论文中报告的值 —— 然后*相对它*调参。默认设置之所以存在，是因为已经有人花掉了这笔预算；你免费继承它。接着调一个值、重新训练、观察变化，重复。重复的意义不只是找到一个更好的值，而是建立**洞见**：哪些超参数真正重要、模型对每一个有多敏感、好的取值范围是什么。这种洞见才是你带到下一个项目里的东西，也是调参工具给不了你的。

### 1.2 手动 vs 自动

手动调参就是人手工跑那个循环。它有效，能建立直觉，对 2–3 个旋钮的问题往往最快。它的失效模式是**实验管理**：跑完五十次之后，你记不清哪个学习率产生了哪条曲线。CS329P 对解决办法说得很直白 —— *保存训练日志和超参数，以便比较、分享和复现。* 最简单的做法是日志存文本、关键指标放电子表格；更好的选择是 TensorBoard 和 Weights & Biases（以及 2026 年的 MLflow）。纪律比工具更重要。

自动调参把循环交给算法。你为此付出算力代价，以及前期指定搜索空间的工作；换回的是覆盖度、一致性，以及无需人值守就能在一夜之间跑完上百次试验的能力。二者的取舍就是引言里的成本曲线：当算力比搜索原本要消耗的数据科学家工时更便宜时，就自动化。

### 1.3 可复现性确实很难

复现一个结果比听起来更难，因为它依赖三样都会漂移的东西：

- **环境** —— 硬件与库的版本。不同的 GPU、不同的 cuDNN、不同的 PyTorch 会改变数值结果，足以让准确率的最后一位发生变化。
- **代码** —— 确切的代码路径，包括你忘了是非确定性的预处理。
- **随机性** —— 随机种子。权重初始化、打乱、dropout 和增强都是随机的；不固定种子，就谈不上「同一次运行」。

对调参而言，这有一个尖锐的后果：**公平比较要求你把干扰变量固定住。** 如果 config A 用的种子、机器与 config B 不同，或者训练计划更长，二者之间的差异就被污染了，你可能一路「调」出一个更差的模型。固定种子、固定环境、给每个 config 相同的预算，并在同一个留出集上比较 —— 否则搜索只是在测量噪声。

### 1.4 调参的成本

每一次试验都是一次完整（或部分）的训练运行，而搜索空间随超参数数量呈指数增长。这就是把**维度灾难**表述成一个预算问题：你负担不起评估整个网格，所以整场游戏就是*让每个试验成本换到更多信号。* 第 2 节的全部内容都是对这一个压力的回答 —— 要么评估更少的点（更聪明的采样），要么以更低的成本评估每个点（更低的保真度）。

---

## 2. HPO 算法

### 2.1 定义搜索空间

在任何算法运行之前，你必须**为每个超参数指定一个范围** —— 对数尺度上的 `lr ∈ [1e-5, 1e-1]`、`batch_size ∈ {32, 64, 128, 256}`、`optimizer ∈ {sgd, adam, adamw}`。空间可能是指数级的大，所以**把它设计好本身就是一项调参决策**：学习率用对数尺度、边界设定合理以排除已知会发散的区间、丢掉你已经知道不重要的旋钮。紧凑而形状良好的空间能让平庸的算法显得不错；草率的空间则会打败优秀的算法。


<details>
<summary>English original</summary>

**1.1 What you are actually tuning**

Hyperparameters are the values *you* set before training, as opposed to parameters the data fills in by gradient descent. They split into a few rough families, and the family tells you how to treat each one:

| Family | Examples | Character |
|---|---|---|
| **Optimization** | learning rate, batch size, momentum, weight decay, warmup/schedule | Continuous or log-continuous; usually the highest-leverage knobs |
| **Architecture** | #layers, #channels/width, kernel size, #hidden units, #attention heads | Discrete/categorical; expensive to change (full retrain) |
| **Regularization** | dropout rate, label smoothing, augmentation strength | Continuous; interacts strongly with dataset size |
| **Capacity-vs-cost** | model size, sequence length, resolution | Sets the cost of *every other* trial |

The practical first move from CS329P is **start from a good baseline** — default settings from a high-quality toolkit, or values reported in a paper on a similar task — and tune *relative to it*. Defaults exist because someone already spent the budget; you inherit it for free. Then tune one value, retrain, observe the change, and repeat. The point of the repetition is not just to find a better value; it's to build **insight**: which hyperparameters actually matter, how sensitive the model is to each, and what the good ranges are. That insight is what you carry to the next project, and it's the thing a tuner can't hand you.

**1.2 Manual vs automated**

Manual tuning is a human running that loop by hand. It works, it builds intuition, and for a 2–3 knob problem it's often fastest. Its failure mode is **experiment management**: after fifty runs you cannot remember which learning rate produced which curve. CS329P is blunt about the fix — *save your training logs and hyperparameters so you can compare, share, and reproduce.* The simplest version is logs in text and key metrics in a spreadsheet; better options are TensorBoard and Weights & Biases (and in 2026, MLflow). The discipline matters more than the tool.

Automated tuning hands the loop to an algorithm. You pay for it in compute and in the upfront work of specifying a search space, and you get back coverage, consistency, and the ability to run a hundred trials overnight without a human in the seat. The decision between them is the cost curve from the intro: automate when compute is cheaper than the data-scientist-hours the search would otherwise consume.

**1.3 Reproducibility is genuinely hard**

Reproducing a result is harder than it sounds, because it depends on three things that all drift:

- **Environment** — hardware and library versions. A different GPU, a different cuDNN, a different PyTorch can change numerics enough to move the last point of accuracy.
- **Code** — the exact code path, including the preprocessing you forgot was non-deterministic.
- **Randomness** — the seed. Weight init, shuffling, dropout, and augmentation are all stochastic; without a pinned seed, "the same run" isn't.

For tuning specifically this has a sharp consequence: **fair comparison demands you hold the nuisance variables fixed.** If config A ran on a different seed, a different machine, or a longer schedule than config B, the difference between them is contaminated and you may "tune" your way to a worse model. Pin the seed, pin the environment, give every config the same budget, and compare on the same held-out split — otherwise the search is measuring noise.

**1.4 The cost of tuning**

Every trial is a full (or partial) training run, and the search space is exponential in the number of hyperparameters. This is the **curse of dimensionality** stated as a budget problem: you cannot afford to evaluate the grid, so the whole game is *getting more signal per trial-dollar.* Everything in Section 2 is an answer to that single pressure — either evaluate fewer points (smarter sampling) or evaluate each point more cheaply (lower fidelity).

---

**2. HPO algorithms**

**2.1 Defining the search space**

Before any algorithm runs you must specify a **range for each hyperparameter** — `lr ∈ [1e-5, 1e-1]` on a log scale, `batch_size ∈ {32, 64, 128, 256}`, `optimizer ∈ {sgd, adam, adamw}`. The space can be exponentially large, so **designing it well is itself a tuning decision**: a log scale for learning rate, sane bounds that exclude regimes you know diverge, and dropping knobs you've already learned don't matter. A tight, well-shaped space makes a mediocre algorithm look good; a sloppy one defeats a great algorithm.

</details>

### 2.2 黑盒 vs 多保真度

两种哲学将所有 HPO 方法一分为二：

- **黑盒** 把每个训练任务当作一个不透明的函数：交给它一个配置，它运行到结束，返回一个分数。网格搜索、随机搜索和贝叶斯优化都是黑盒。
- **多保真度** *修改* 训练任务，以获得某个配置有多好的廉价、带噪估计，从而可以尽早终止差的配置。标准的廉价化手段：在子采样数据集上训练、缩小模型（更少的 layer/channel），或者——最重要的——**提前停止一个差的配置**，而不是让它跑到收敛。Successive Halving 和 Hyperband 都是多保真度。

多保真度的赌注是：**大多数配置都很差，并且会很快暴露这一点**，所以把全部预算花在它们身上就是浪费。

### 2.3 网格搜索

评估空间中的每一种组合。

```python
def grid_search(search_space):
    best = None
    for config in search_space:          # every point on the grid
        score = train_and_eval(config)
        best = better_of(best, score, config)
    return best
```

它保证能找到 *网格内* 的最佳点，而且极易并行。它也栽在维度灾难上：6 个超参数上各取 5 个值的网格就是 15,625 次试验，其中大多数只是在调整模型并不关心的旋钮，却锁死了它真正关心的那一个。

### 2.4 随机搜索——以及它为什么胜过网格搜索

从空间中均匀随机地采样 `n` 个配置。

```python
def random_search(search_space, n):
    best = None
    for _ in range(n):
        config = random_select(search_space)   # sample, don't enumerate
        score = train_and_eval(config)
        best = better_of(best, score, config)
    return best
```

这个反直觉的结果——由 Bergstra & Bengio 在 *Random Search for Hyper-Parameter Optimization* 中确立——是：随机搜索**在经验上和理论上都比网格搜索更高效。** 原因是大多数 HPO 问题的 *低有效维度*：只有少数几个超参数真正重要，但你不知道是哪几个。网格通过尝试许多 *不重要* 旋钮的不同值，同时对重要的那个只测试 `grid_size` 个不同的值，浪费了自己的分辨率。随机搜索则在每次试验中独立采样每个旋钮，在相同的 `n` 次试验中尝试了重要旋钮的 `n` 个不同的值。CS329P 的实际结论毫不含糊：**实践中，从随机搜索开始。** 它是正确的默认选择，也是每个更花哨的方法都必须击败的基线。

### 2.5 贝叶斯优化（BO）

BO 是"聪明"的黑盒方法：它会在进行中 *学习* 目标函数的形状，并据此挑选下一步该看哪里，而不是盲目采样。

它由两部分组成。一个 **代理模型** 是一个概率回归模型——高斯过程或随机森林——它估计目标（验证分数）如何依赖于超参数，*并且* 报告自身的不确定性。一个 **采集函数** 将代理模型转化为一个决策：它 *同时* 根据预测的目标值和代理模型在该处的不确定性来为每个候选配置打分，下一个试验就采样在分数最高的位置。最大化采集函数明确地 **权衡探索**（去代理模型不确定的地方——你可能会学到一些东西）**与利用**（去它预测高分的地方——你现在就可能获胜）。

```text
loop:
  fit surrogate(observed trials)          # GP / random forest over HP -> score
  next_config = argmax acquisition(surrogate)   # high predicted score OR high uncertainty
  score = train_and_eval(next_config)
  observe (next_config, score)
```

BO 有两个真正的局限，二者都来自幻灯片。**在初始阶段，它的行为就像随机搜索**——没有任何观测时，代理模型是平的，所以最初几次试验没有任何优势。而且**优化本质上是串行的**：每一次试验的选择都依赖于之前的所有结果，这使得 BO 相比随机搜索那种令人尴尬的并行性难以并行。你可以用批量 BO 绕过这一点，但核心循环本身要求串行。


<details>
<summary>English original</summary>

**2.2 Black-box vs multi-fidelity**

Two philosophies divide every HPO method:

- **Black-box** treats each training job as an opaque function: you hand it a config, it runs to completion, it returns a score. Grid search, random search, and Bayesian optimization are black-box.
- **Multi-fidelity** *modifies* the training job to get a cheap, noisy estimate of how good a config will be, so you can kill bad ones early. The standard cheapening moves: train on a subsampled dataset, shrink the model (fewer layers/channels), or — most importantly — **stop a bad configuration early** instead of running it to convergence. Successive Halving and Hyperband are multi-fidelity.

The multi-fidelity bet is that **most configurations are bad and reveal it quickly**, so spending full budget on them is waste.

**2.3 Grid search**

Evaluate every combination in the space.

```python
def grid_search(search_space):
    best = None
    for config in search_space:          # every point on the grid
        score = train_and_eval(config)
        best = better_of(best, score, config)
    return best
```

It guarantees you find the best point *in the grid* and it's trivially parallel. It also dies to the curse of dimensionality: a 5-value grid over 6 hyperparameters is 15,625 trials, and most of them vary a knob the model doesn't care about while pinning the one it does.

**2.4 Random search — and why it beats grid**

Sample `n` configurations uniformly at random from the space.

```python
def random_search(search_space, n):
    best = None
    for _ in range(n):
        config = random_select(search_space)   # sample, don't enumerate
        score = train_and_eval(config)
        best = better_of(best, score, config)
    return best
```

The counterintuitive result — established by Bergstra & Bengio, *Random Search for Hyper-Parameter Optimization* — is that random search is **more efficient than grid search, both empirically and in theory.** The reason is the *low effective dimensionality* of most HPO problems: only a couple of hyperparameters actually matter, but you don't know which. A grid wastes its resolution by trying many distinct values of the *unimportant* knobs while testing only `grid_size` distinct values of the important one. Random search, by sampling every knob independently each trial, tries `n` distinct values of the important knob for the same `n` trials. CS329P's practical verdict is unambiguous: **in practice, start with random search.** It's the right default and the baseline every fancier method must beat.

**2.5 Bayesian optimization (BO)**

BO is the "smart" black-box method: it *learns* the shape of the objective as it goes and uses that to pick where to look next, rather than sampling blindly.

It has two parts. A **surrogate model** is a probabilistic regression model — a Gaussian process or a random forest — that estimates how the objective (validation score) depends on the hyperparameters, *and* reports its own uncertainty. An **acquisition function** turns the surrogate into a decision: it scores each candidate config by *both* its predicted objective and the surrogate's uncertainty there, and the next trial is sampled where that score is highest. Maximizing the acquisition function explicitly **trades off exploration** (go where the surrogate is uncertain — you might learn something) **against exploitation** (go where it predicts a high score — you might win now).

```text
loop:
  fit surrogate(observed trials)          # GP / random forest over HP -> score
  next_config = argmax acquisition(surrogate)   # high predicted score OR high uncertainty
  score = train_and_eval(next_config)
  observe (next_config, score)
```

BO has two real limitations, both from the slides. **In its initial stages it behaves like random search** — with no observations the surrogate is flat, so the first handful of trials carry no advantage. And the **optimization is inherently sequential**: each trial's choice depends on all previous results, which makes BO awkward to parallelize compared to random search's embarrassing parallelism. You can batch-BO around this, but the core loop wants to be serial.

</details>

### 2.6 Successive Halving 与 Hyperband（多保真度）

Successive Halving 从另一侧攻击预算问题：**不去更聪明地采样，而是更廉价地评估，并把预算重新分配给存活者。**

```text
Successive Halving:
  randomly pick n configs, train each for m epochs
  repeat until one config remains:
    keep the best n/2, train them another m epochs
    keep the best n/4, train them another 2m epochs
    ...
```

机制是：给大量 config 极小的预算，丢掉最差的一半，把省下的预算灌给存活者。有希望的 config 逐步挣得更多 epoch；没希望的跑几个 epoch 就被杀掉。你根据总预算和一次完整训练需要多少 epoch 来选定 `n` 与 `m`。

矛盾在于 `n` 与 `m` 相互拉扯：**`n` 控制探索**（尝试多少个 config），**`m` 控制利用**（在做出判断前给它多久）。`m` 取得太小，会杀掉一个起步慢但本可胜出的 config；`n` 取得太小，则根本采不到那个好 config。**Hyperband** 通过*不做选择*来化解这一矛盾——它运行**多个 Successive Halving bracket**，每个 bracket 采用不同的 `(n, m)` 权衡，从「config 多、预算短」一路扫到「config 少、预算长」。靠前的 bracket 大范围探索，靠后的 bracket 转向利用；跨 bracket 的最佳结果胜出。**ASHA**（Asynchronous Successive Halving）是生产级变体：它去掉同步屏障，使 worker 不会空等某个 rung 填满，这正是它能扩展到数百个并行 worker 的原因——也是你在 Ray Tune 里实际会遇到的版本。

### 2.7 BOHB——把两种思路结合起来

BO 与 Hyperband 修补的是不同弱点。Hyperband 的预算分配很出色，但**以随机方式采样 config**，因此它对*该往哪儿看*永远不会变得更聪明。BO 采样 config 很聪明，但**对每个 config 都花满预算**，而且冷启动。**BOHB**（Bayesian Optimization + Hyperband）把两者结合：用 Hyperband 的 bracket 结构决定每个 config *拿到多少预算*，用 BO 代理模型决定*采样哪些 config*，而不是均匀抽取。一种方法同时具备 Hyperband 的早停效率与 BO 的样本效率。

### 2.8 对比

| 算法 | 类型 | 样本效率 | 并行性 | 早停 | 何时选用 |
|---|---|---|---|---|---|
| **网格搜索** | 黑盒 | 最差 | 天然并行 | 否 | 空间极小（1–2 个旋钮）、要求可复现/覆盖 |
| **随机搜索** | 黑盒 | 良好基线 | 天然并行 | 否 | **默认的第一步**；强、简单、天然并行 |
| **贝叶斯优化（BO）** | 黑盒 | 高 | 差（串行） | 否 | 单次试验昂贵、预算有限、worker 少 |
| **Successive Halving** | 多保真度 | 高 | 好 | 是 | 大量可廉价评判的 config；多数一上来就很差 |
| **Hyperband / ASHA** | 多保真度 | 高 | 极好（ASHA 异步） | 是 | 并行算力充足、未知 `(n,m)` 权衡 |
| **BOHB** | 两者兼有 | 最高 | 好 | 是 | 你既要智能采样*又*要早停 |

**实践要点（CS329P）：**先用随机搜索建立基线并感受空间；有并行算力时升级到 Hyperband/ASHA，单次试验各自昂贵时用 BO/BOHB。而且无论如何，**都要挖掘自己的日志**——表现最好的配置会聚成簇，查阅这里过去什么管用、或相关论文与代码库用过什么 config，往往比任何盲目搜索更快找到它们。

---

## 3. 神经架构搜索（NAS）

NAS 提出下一个问题：与其调优一个固定网络的超参数，能否*搜索网络本身*？神经网络有架构级超参数——**拓扑结构**（类 ResNet 还是类 MobileNet、多少层）与**逐层选择**（kernel 大小、卷积层的通道数、稠密/循环层的隐层宽度）。NAS 把选择这些参数自动化。按 Elsken et al. 2019 的说法，每个 NAS 方法都分解为三个组件：**搜索空间**（哪些架构可达）、**搜索策略**（如何探索它们）、**性能估计**（如何在不把自己搞破产的前提下给候选打分）。

### 3.1 搜索空间——cell 与 macro

搜索空间定义了 NAS 能构建什么，其设计主导最终结果。

- **Macro 搜索**直接选择整个网络的连线——每一层、每一条连接。表达能力最强，规模也大得离谱。
- **Cell（micro）搜索**搜索一个小的可重复积木块——一个「cell」——再把它堆叠成带固定 macro 骨架的网络（NASNet 的技巧）。搜索空间急剧缩小，找到的 cell 可跨深度与数据集迁移，这也成了标准做法，因为它是唯一让搜索变得可行的办法。


<details>
<summary>English original</summary>

**2.6 Successive Halving and Hyperband (multi-fidelity)**

Successive Halving attacks the budget from the other side: **don't sample smarter, evaluate cheaper, and reallocate budget toward survivors.**

```text
Successive Halving:
  randomly pick n configs, train each for m epochs
  repeat until one config remains:
    keep the best n/2, train them another m epochs
    keep the best n/4, train them another 2m epochs
    ...
```

The mechanism: give many configs a tiny budget, throw away the worst half, and pour the saved budget into the survivors. Promising configs earn progressively more epochs; hopeless ones are killed after a few. You pick `n` and `m` from your total budget and how many epochs a full training needs.

The tension is that `n` and `m` pull against each other: **`n` controls exploration** (how many configs you try) and **`m` controls exploitation** (how long before you judge them). Pick `m` too small and you kill a slow-starting config that would have won; pick `n` too small and you never sample the good config at all. **Hyperband** resolves this by *not picking* — it runs **multiple Successive Halving brackets**, each with a different `(n, m)` trade-off, sweeping from "many configs, short budget" to "few configs, long budget." Early brackets explore widely; later brackets exploit; the best result across brackets wins. **ASHA** (Asynchronous Successive Halving) is the production-grade variant: it removes the synchronization barrier so workers don't idle waiting for a rung to fill, which is what makes it scale to hundreds of parallel workers — the version you'll actually find inside Ray Tune.

**2.7 BOHB — combining the two ideas**

BO and Hyperband fix different weaknesses. Hyperband allocates budget brilliantly but **samples configs at random**, so it never gets smarter about *where* to look. BO samples configs intelligently but **spends full budget on each** and starts cold. **BOHB** (Bayesian Optimization + Hyperband) marries them: use Hyperband's bracket structure to decide *how much budget* each config gets, and a BO surrogate to decide *which configs to sample* instead of drawing them uniformly. You get Hyperband's early-stopping efficiency and BO's sample-efficiency in one method.

**2.8 Comparison**

| Algorithm | Type | Sample efficiency | Parallelism | Early stop | When to reach for it |
|---|---|---|---|---|---|
| **Grid search** | Black-box | Worst | Embarrassing | No | Tiny space (1–2 knobs), reproducibility/coverage demands |
| **Random search** | Black-box | Good baseline | Embarrassing | No | **Default first move**; strong, simple, trivially parallel |
| **Bayesian opt (BO)** | Black-box | High | Poor (sequential) | No | Expensive trials, modest budget, few workers |
| **Successive Halving** | Multi-fidelity | High | Good | Yes | Many cheap-to-judge configs; most are bad early |
| **Hyperband / ASHA** | Multi-fidelity | High | Excellent (ASHA async) | Yes | Lots of parallel compute, unknown `(n,m)` trade-off |
| **BOHB** | Both | Highest | Good | Yes | You want smart sampling *and* early stopping |

**Practical takeaway (CS329P):** start with random search to get a baseline and a feel for the space; graduate to Hyperband/ASHA when you have parallel compute, or BO/BOHB when trials are individually expensive. And whatever you do, **mine your own logs** — the top-performing configurations cluster, and you can often find them faster by looking at what worked here before, or what configs the relevant papers and codebases used, than by any blind search.

---

**3. Neural Architecture Search (NAS)**

NAS asks the next question: instead of tuning a fixed network's hyperparameters, can we *search the network itself*? A neural net has architecture-level hyperparameters — the **topological structure** (ResNet-ish vs MobileNet-ish, how many layers) and the **per-layer choices** (kernel size, channel count in a conv layer, hidden width in a dense/recurrent layer). NAS automates choosing them. Every NAS method, following Elsken et al. 2019, decomposes into three components: **search space** (what architectures are reachable), **search strategy** (how you explore them), and **performance estimation** (how you score a candidate without bankrupting yourself).

**3.1 Search space — cell vs macro**

The search space defines what NAS can build, and its design dominates the result.

- **Macro search** chooses the whole network's wiring directly — every layer, every connection. Maximally expressive, brutally large.
- **Cell (micro) search** searches for a small repeatable building block — a "cell" — then stacks copies of it into a network with a fixed macro skeleton (the trick from NASNet). The search space shrinks enormously, the found cell transfers across depths and datasets, and this became the standard because it's the only thing that made the search tractable.

</details>

### 3.2 搜索策略

**强化学习（Zoph & Le, 2017）。** 开创性的方法：一个 **RNN 控制器**发出描述架构的*token 序列*（这一层是 3×3 卷积、这么多滤波器、在这里连接……）。提出的架构被训练至收敛，其**验证准确率即为奖励**。控制器用 **REINFORCE** 策略梯度规则更新，使高奖励的架构更可能出现。它奏效了——而且*代价高得惊人*：朴素方法样本效率低，单次搜索要烧掉大约 **2000 GPU-days**，因为每个奖励都需要从头训练一个网络。随即出现了两条出路：更廉价地**估计性能**，以及在候选架构之间**共享参数**（EAS、ENAS），这样就不必每次都从零重训。

**进化搜索。** 维护一个架构种群，对好的个体做变异（替换一个算子、增加一条连接），保留适应度高的，丢弃适应度低的。概念简单，天然可并行，且与 RL 有竞争力——AmoebaNet 追平甚至击败了 RL 找到的网络——但如果每个候选都从头训练，它背负着同样根本性的成本。

**One-shot / 权重共享。** 成本上的突破。不再训练成千上万个独立网络，而是**把架构的学习与权重的学习合并进单个过参数化的 “supernet”**，它把每个候选架构都作为共享同一组权重的子路径包含在内。训练一次 supernet；然后继承这些共享权重来评测候选子架构，仅在几个 epoch 后测量准确率——你只需要候选的*排序*，而非绝对准确率，因此一个廉价的代理指标就足够了。最后，**从头重训最有希望的候选**以得到真实结果。**ENAS** 把权重共享用于 RL 控制器，将搜索从约 2000 GPU-days 削减到大约*半个 GPU-day*。

**DARTS——可微架构搜索。** 最优雅的 one-shot 方法让离散搜索变得*连续且可梯度训练*。不再为每层硬性选择单个算子，而是**把类别选择松弛为对所有候选算子的 softmax**。在第 `l` 层有候选算子 `oᵢˡ` 时，传给下一层的输出是*加权混合*：

```text
output(l) = Σ_i  αᵢˡ · oᵢˡ(input)        with   αˡ = softmax(aˡ)
```

混合权重 `aˡ` 现在就是普通的连续参数。你用梯度下降**联合学习 `aˡ` 和网络权重**，最后在每层挑选 `α` 最大的单个算子（`argmaxᵢ αᵢ`）。因为整个搜索就是一次可微优化，**DARTS 达到了 SOTA，并把搜索时间削减到约 3 GPU-days**——比最初的 RL 低了三个数量级。

### 3.3 性能估计

NAS 的成本*就是*给候选打分的成本，因此估计才是省下预算的地方。工具箱包括：**代理指标**（几个 epoch 后的准确率而非训练至收敛的准确率，赌的是早期排序能预测最终排序）；**权重继承 / 共享**（无需从头训练即可评测，如上面的 one-shot）；在**子采样数据集或更小模型**上训练；以及普遍意义上的**低保真度**代理——与 Hyperband 相同的多保真度逻辑，应用于架构上。

### 3.4 另一种杠杆：复合缩放（EfficientNet）

值得与搜索本身分开讲，因为对 CNN 而言它基本*取代*了搜索。CNN 可以从三个方向缩放——**更深**（更多层）、**更宽**（更多通道）、**更大的输入**（更高分辨率）。EfficientNet 的洞见是，按*固定比例一起*缩放它们，胜过单独缩放其中任意一个。**复合缩放**设定 depth `∝ αᵠ`、width `∝ βᵠ`、resolution `∝ γᵠ`，约束为 `α·β²·γ² ≈ 2`，从而单个系数 `ϕ` 就能干净地调节一个旋钮——每增加一个单位的 `ϕ`，总 FLOPs 大约翻倍。你只需做一次小规模搜索来找到好的 `α, β, γ`，然后调节 `ϕ` 就能得到整个家族（EfficientNet-B0…B7）。这比针对每个算力预算搜索一个新架构便宜得多，也正是为什么在实践中“缩放一个已知良好的架构”胜过了“搜索一个新架构”。


<details>
<summary>English original</summary>

**3.2 Search strategy**

**Reinforcement learning (Zoph & Le, 2017).** The seminal approach: an **RNN controller** emits a *sequence of tokens* describing an architecture (this layer is a 3×3 conv, that many filters, connect here…). The proposed architecture is trained to convergence, and its **validation accuracy is the reward**. The controller is updated with the **REINFORCE** policy-gradient rule to make high-reward architectures more likely. It worked — and it was *staggeringly expensive*: the naive approach is sample-inefficient and burned on the order of **~2000 GPU-days** for a single search, because every reward required training a network from scratch. Two escape routes were proposed immediately: **estimate performance** more cheaply, and **share parameters** across candidate architectures (EAS, ENAS) so you don't retrain from zero every time.

**Evolutionary search.** Maintain a population of architectures, mutate the good ones (swap an operation, add a connection), keep the fit, discard the unfit. Conceptually simple, parallelizes naturally, and competitive with RL — AmoebaNet matched or beat RL-found nets — but it carries the same fundamental cost if each candidate is trained from scratch.

**One-shot / weight-sharing.** The cost breakthrough. Instead of training thousands of separate networks, **combine the learning of architecture and weights into a single over-parameterized "supernet"** that contains every candidate architecture as a sub-path sharing one set of weights. Train the supernet once; then evaluate candidate sub-architectures by inheriting those shared weights and measuring accuracy after only a few epochs — you only need the candidate *ranking*, not absolute accuracy, so a cheap proxy metric suffices. Finally, **re-train the most promising candidate from scratch** for the real result. **ENAS** applied weight-sharing to the RL controller and cut the search from ~2000 GPU-days to roughly *half a GPU-day*.

**DARTS — Differentiable Architecture Search.** The most elegant one-shot method makes the discrete search *continuous and gradient-trainable*. Instead of hard-choosing one operation per layer, **relax the categorical choice into a softmax over all candidate operations.** With candidate operations `oᵢˡ` at layer `l`, the output passed to the next layer is the *weighted mixture*:

```text
output(l) = Σ_i  αᵢˡ · oᵢˡ(input)        with   αˡ = softmax(aˡ)
```

The mixing weights `aˡ` are now ordinary continuous parameters. You **jointly learn `aˡ` and the network weights by gradient descent**, then at the end pick the single operation with the largest `α` at each layer (`argmaxᵢ αᵢ`). Because the whole search is one differentiable optimization, **DARTS reached SOTA and cut search time to ~3 GPU-days** — three orders of magnitude below the RL original.

**3.3 Performance estimation**

The cost of NAS *is* the cost of scoring candidates, so estimation is where budget is won. The toolkit: a **proxy metric** (accuracy after a few epochs instead of to convergence, on the bet that early ranking predicts final ranking); **weight inheritance / sharing** (evaluate without training from scratch, per one-shot above); training on a **subsampled dataset or smaller model**; and **lower-fidelity** proxies generally — the same multi-fidelity logic as Hyperband, applied to architectures.

**3.4 A different lever: compound scaling (EfficientNet)**

Worth separating from search proper, because it largely *replaced* it for CNNs. A CNN can be scaled three ways — **deeper** (more layers), **wider** (more channels), **larger inputs** (higher resolution). EfficientNet's insight is that scaling them *together in a fixed ratio* beats scaling any one alone. **Compound scaling** sets depth `∝ αᵠ`, width `∝ βᵠ`, resolution `∝ γᵠ`, under the constraint `α·β²·γ² ≈ 2`, so a single coefficient `ϕ` cleanly trades one knob — total FLOPs roughly double per unit of `ϕ`. You do a small search once to find good `α, β, γ`, then just dial `ϕ` to get an entire family (EfficientNet-B0…B7). This is dramatically cheaper than searching a new architecture per compute budget, and it's why "scale a known-good architecture" beat "search a new one" in practice.

</details>

### 3.5 NAS 的真实状态

CS329P（2021）将 NAS 描述为“现在就能实际使用”——在当时，凭借复合缩放和可微分 one-shot 搜索，确实如此。但要诚实看待这段历程。即便在当时，NAS 也有真实的研究问题：**可解释性**（你得到一个架构，却讲不出它*为什么*好），以及干净的搜索 benchmark 与杂乱的真实任务之间的差距。而且它有一个结构性弱点——它*从头搜索小型网络*，而这恰恰是该领域抛弃的范式。下面的 2026 更新在这里不是脚注；它就是头条。

> **2026 更新：** 经典 NAS 已基本从主流实践中淡出。支撑它的前提——*为你的任务从头搜索一个小型网络*——已被**迁移学习、scaling law 和基础模型**取代：你不再设计定制网络，而是**取一个大型预训练模型并适配它**（Lecture 09），架构由什么能可预测地扩展来决定，而不是由控制器发现什么来决定。那些著名成果（NASNet、AmoebaNet、DARTS、EfficientNet）作为思想仍值得了解，而**硬件感知 NAS 在边缘延续下来**（MnasNet、FBNet、Once-for-All——见 Hardware lens）。AutoML 整体上在**表格数据**上活得最好，在那里 `AutoGluon`/`auto-sklearn` 真正胜出，且没有可迁移的基础模型。对于日常 HPO，没人手搓调参器：工具是 **Optuna**（define-by-run、TPE/CMA-ES 采样器、内置剪枝器）和 **Ray Tune**（分布式、ASHA/PBT/BOHB）。先学原始算法——它们是心智模型——然后再去用这些。

---

## 4. 深度网络调优工具箱

深度学习最大的收益并非来自调优，而是来自**改变了什么可训练的结构性发明。** CS329P 的框架是：深度学习是一门*可微分编程语言*，用于从数据中提取信息，其设计模式从 layer 级直到架构级。有三种模式最为重要。


<details>
<summary>English original</summary>

**3.5 The honest state of NAS**

CS329P (2021) presents NAS as "practical to use now" — and at the time, with compound scaling and differentiable one-shot search, it was. But be honest about the arc. NAS had real research problems even then: **explainability** (you get an architecture with no story for *why* it's good), and the gap between a clean search benchmark and a messy real task. And it had a structural vulnerability — it searches *small networks from scratch*, which is exactly the regime that the field walked away from. The 2026 update below is not a footnote here; it's the headline.

> **2026 update:** Classic NAS has largely faded from mainstream practice. The premise that justified it — *search a small network from scratch for your task* — was overtaken by **transfer learning, scaling laws, and foundation models**: you no longer design a bespoke net, you **take a large pretrained model and adapt it** (Lecture 09), and architecture is set by what scales predictably, not by what a controller discovers. The famous results (NASNet, AmoebaNet, DARTS, EfficientNet) are still worth knowing as ideas, and **hardware-aware NAS lives on at the edge** (MnasNet, FBNet, Once-for-All — see the Hardware lens). AutoML as a whole survives best for **tabular data**, where `AutoGluon`/`auto-sklearn` genuinely win and there's no foundation model to transfer from. For everyday HPO, nobody hand-rolls a tuner: the tools are **Optuna** (define-by-run, TPE/CMA-ES samplers, built-in pruners) and **Ray Tune** (distributed, ASHA/PBT/BOHB). Learn the original algorithms first — they're the mental model — then reach for these.

---

**4. The deep-network tuning toolkit**

The biggest gains in deep learning came not from tuning but from **structural inventions that changed what was trainable.** CS329P's framing: deep learning is a *differentiable programming language* for extracting information from data, with design patterns from the layer level up to the architecture level. Three patterns matter most.

</details>

### 4.1 归一化：batch norm vs layer norm

**为什么要归一化。** 标准化输入会让损失曲面*更平滑* —— 形式上，在 `‖∇f(x) − ∇f(y)‖ ≤ β‖x − y‖` 中更小的 Lipschitz 常数 `β`，更小的 `β` 则允许更大的学习率。这个技巧对输入上的线性模型有效，但**对深度网络没有帮助**，因为*内部*层的输入在训练过程中会漂移。**批归一化（Batch Normalization，BN）**会标准化内部层的输入，改善平滑性，使训练更容易。（BN 为什么有效*仍然*存在一些争议——最初“内部协变量偏移”的说法受到质疑——但它确实有效这一点没有争议。）

CS329P 把*每一个*归一化层都分解为相同的三个步骤，这正是把它们记在脑子里的正确方式：

1. 把输入 **Reshape** 成 2D 矩阵。
2. 沿选定的轴进行 **Normalize**（标准化）：`x̂ ← (x − mean) / std`。
3. 用可学习的 scale 和 shift 进行 **Recover**：`y = γ·x̂ + β`，这样当撤销归一化才是最佳选择时，该层就能撤销它。

区分这些变体的*唯一*东西是 **reshape** ——你沿哪个轴归一化。

- **Batch norm** 把 `X ∈ ℝ^{n×c×w×h} → ℝ^{nwh×c}` reshape 后，**按通道、跨批**归一化。它的统计量在批上计算，因此需要在训练期间维护移动 mean/var，供推理时使用。
- **Layer norm** 把 `X ∈ ℝ^{n×c×w×h} → ℝ^{cwh×n}` reshape 后，**按样本、跨该样本的特征**归一化——其余一切都与 BN 相同。

```python
def batch_norm(X, gamma, beta, moving_mean, moving_var, eps, momentum):
    if not torch.is_grad_enabled():                 # inference: use running stats
        X_hat = (X - moving_mean) / torch.sqrt(moving_var + eps)
    else:
        if len(X.shape) == 2:                        # (batch, features)
            mean = X.mean(dim=0)
            var = ((X - mean) ** 2).mean(dim=0)
        else:                                        # (batch, channel, H, W)
            mean = X.mean(dim=(0, 2, 3), keepdim=True)
            var = ((X - mean) ** 2).mean(dim=(0, 2, 3), keepdim=True)
        X_hat = (X - mean) / torch.sqrt(var + eps)
        moving_mean = momentum * moving_mean + (1.0 - momentum) * mean
        moving_var = momentum * moving_var + (1.0 - momentum) * var
    return gamma * X_hat + beta, moving_mean, moving_var
```

**为什么 LN 在 Transformer 上胜出。** BN 对批的依赖对序列模型是致命的。用在 RNN 上时，BN 必须为**每个时间步维护单独的移动统计量**——而在推理时面对很长的序列，训练中从未见过的时间步根本没有统计量。LN 完全避开了这一点：它在**每个样本内部、截至当前步**做归一化，因此不需要批统计量，而且无论序列长度或批大小如何，**训练与推理之间都保持一致**。这一性质——样本之间没有耦合、训练与测试行为一致——正是 Transformer 所需要的，也正是**LN 是 Transformer block 的归一化方式**、而 BN 仍是 CNN 默认选择的原因。（其他变体只是选了别的 reshape：InstanceNorm 按每通道每样本归一化，GroupNorm 把通道分成组，你也可以对权重或梯度而不是激活值做归一化。）

| | Batch Norm | Layer Norm |
|---|---|---|
| 归一化维度 | 批维度（按通道） | 特征维度（按样本） |
| 需要批统计量 | 是（移动 mean/var） | 否 |
| 训练 ≠ 推理行为 | 是（测试时使用运行统计量） | 否（完全一致） |
| 对批大小敏感 | 是（批大小为 1 时失效） | 否 |
| 长序列 | 有问题（逐步统计量） | 自然 |
| 事实上的归属 | CNN | Transformer / RNN |


<details>
<summary>English original</summary>

**4.1 Normalization: batch norm vs layer norm**

**Why normalize at all.** Standardizing inputs makes the loss surface *smoother* — formally, a smaller Lipschitz constant `β` in `‖∇f(x) − ∇f(y)‖ ≤ β‖x − y‖`, and a smaller `β` permits a larger learning rate. That trick works for linear models on the input but **doesn't help deep nets**, because the inputs to *internal* layers drift during training. **Batch Normalization (BN)** standardizes the inputs to internal layers, improving smoothness and making training easier. (Why BN works is *still* somewhat controversial — the original "internal covariate shift" story is contested — but that it helps is not.)

CS329P factors *every* normalization layer into the same three steps, which is the right way to hold them in your head:

1. **Reshape** the input into a 2D matrix.
2. **Normalize** (standardize) along the chosen axis: `x̂ ← (x − mean) / std`.
3. **Recover** with learnable scale and shift: `y = γ·x̂ + β`, so the layer can undo the normalization if that's what's best.

The *only* thing that distinguishes the variants is the **reshape** — which axis you normalize over.

- **Batch norm** reshapes `X ∈ ℝ^{n×c×w×h} → ℝ^{nwh×c}` and normalizes **per channel, across the batch**. Its statistics are computed over the batch, so it needs a moving mean/var maintained during training for use at inference.
- **Layer norm** reshapes `X ∈ ℝ^{n×c×w×h} → ℝ^{cwh×n}` and normalizes **per example, across that example's features** — everything else is identical to BN.

```python
def batch_norm(X, gamma, beta, moving_mean, moving_var, eps, momentum):
    if not torch.is_grad_enabled():                 # inference: use running stats
        X_hat = (X - moving_mean) / torch.sqrt(moving_var + eps)
    else:
        if len(X.shape) == 2:                        # (batch, features)
            mean = X.mean(dim=0)
            var = ((X - mean) ** 2).mean(dim=0)
        else:                                        # (batch, channel, H, W)
            mean = X.mean(dim=(0, 2, 3), keepdim=True)
            var = ((X - mean) ** 2).mean(dim=(0, 2, 3), keepdim=True)
        X_hat = (X - mean) / torch.sqrt(var + eps)
        moving_mean = momentum * moving_mean + (1.0 - momentum) * mean
        moving_var = momentum * moving_var + (1.0 - momentum) * var
    return gamma * X_hat + beta, moving_mean, moving_var
```

**Why LN won for transformers.** BN's dependence on the batch is fatal for sequence models. Applied to an RNN, BN must keep **separate moving statistics for every time step** — and for very long sequences at inference, time steps you never saw during training have no statistics at all. LN sidesteps this entirely: it normalizes **within each example, up to the current step**, so it needs no batch statistics and is **consistent between training and inference** regardless of sequence length or batch size. That property — no cross-example coupling, identical behavior train and test — is exactly what a transformer needs, and it's why **LN is the normalization of the transformer block** while BN remains the default for CNNs. (Other variants just pick other reshapes: InstanceNorm normalizes per-channel-per-example, GroupNorm splits channels into groups, and you can also normalize weights or gradients instead of activations.)

| | Batch Norm | Layer Norm |
|---|---|---|
| Normalizes over | Batch dimension (per channel) | Feature dimension (per example) |
| Needs batch statistics | Yes (moving mean/var) | No |
| Train ≠ inference behavior | Yes (uses running stats at test) | No (identical) |
| Batch-size sensitive | Yes (fails at batch size 1) | No |
| Long sequences | Problematic (per-step stats) | Natural |
| De-facto home | CNNs | Transformers / RNNs |

</details>

### 4.2 残差连接

**它们所解决的问题。** 朴素地看，*增加层会让准确率变差* —— 不只是过拟合，在训练数据上同样会退化，因为优化器很难把多出来的层推向哪怕只是一个恒等映射。增加一层 `f` 会把函数类从 `g(x)` 变为 `f(g(x))`；如果新函数类并不*包含*旧的函数类，更深的网络可能严格更差。

**修复办法。** 残差连接让该块计算 `f(g(x)) + g(x)` —— 通过恒等捷径把输入加回来。由此可得两点。第一，函数类现在是**嵌套**的：通过驱动 `f → 0`，该块就能精确恢复 `g(x)`，所以增加层绝不会损害可达的函数类。第二点，也是操作上起决定作用的一点，**梯度获得了一条恒等路径**：

```text
without residual:   ∂/∂x [ f(g(x)) ]        = f'(g(x)) · g'(x)        # product of Jacobians, can vanish
with residual:      ∂/∂x [ f(g(x)) + g(x) ] = f'(g(x)) · g'(x) + g'(x)  # extra +g'(x) survives
```


多出来的 `+g'(x)` 项正是*较浅*子网络的梯度；即便矩阵乘的 Jacobian 相乘后趋近于零，它仍能不受衰减地反向流动。正是这一点让真正深的网络变得可训练 —— 人们凭它把 CNN 训练到了 **1000 层**以上。**ResNet** 不过是堆叠起来的残差块，却成了事实上的 CNN 架构：

```python
class ResidualBlock(nn.Module):
    def __init__(self, in_ch, num_ch):
        super().__init__()
        self.conv1 = nn.Conv2d(in_ch, num_ch, kernel_size=3, padding=1)
        self.conv2 = nn.Conv2d(num_ch, num_ch, kernel_size=3, padding=1)
        self.bn1, self.bn2 = nn.BatchNorm2d(num_ch), nn.BatchNorm2d(num_ch)

    def forward(self, X):
        Y = F.relu(self.bn1(self.conv1(X)))
        Y = self.bn2(self.conv2(Y))
        return F.relu(Y + X)            # the identity shortcut: + X
```


从概念上讲，这是**在特征上做 boosting**（每个块都为持续演进的表示添加一次修正），它还有若干变体 —— 在不同位置添加捷径、改变输出通道数，或者用拼接代替相加（DenseNet）。恒等路径是承载全局的核心思想，它在每个 Transformer 里都以残差流的形式重新出现。


<details>
<summary>English original</summary>

**4.2 Residual connections**

**The problem they solve.** Naively, *adding layers can make accuracy worse* — not just overfit, but degrade on training data too, because the optimizer struggles to drive the extra layers toward even an identity mapping. Adding a layer `f` changes the function class from `g(x)` to `f(g(x))`; if the new class doesn't *contain* the old one, the deeper net can be strictly worse.

**The fix.** A residual connection makes the block compute `f(g(x)) + g(x)` — adding the input back via an identity shortcut. Two things follow. First, the function class is now **nested**: the block can recover `g(x)` exactly by driving `f → 0`, so adding the layer can never hurt the achievable class. Second, and operationally decisive, the **gradient gets an identity path**:

```text
without residual:   ∂/∂x [ f(g(x)) ]        = f'(g(x)) · g'(x)        # product of Jacobians, can vanish
with residual:      ∂/∂x [ f(g(x)) + g(x) ] = f'(g(x)) · g'(x) + g'(x)  # extra +g'(x) survives
```

That extra `+g'(x)` term is the gradient of the *shallower* sub-network; it flows backward undiminished even when the matmul Jacobians multiply down toward zero. This is what makes genuinely deep nets trainable — people have trained CNNs past **1000 layers** on the strength of it. **ResNet** is just stacked residual blocks and became the de-facto CNN architecture:

```python
class ResidualBlock(nn.Module):
    def __init__(self, in_ch, num_ch):
        super().__init__()
        self.conv1 = nn.Conv2d(in_ch, num_ch, kernel_size=3, padding=1)
        self.conv2 = nn.Conv2d(num_ch, num_ch, kernel_size=3, padding=1)
        self.bn1, self.bn2 = nn.BatchNorm2d(num_ch), nn.BatchNorm2d(num_ch)

    def forward(self, X):
        Y = F.relu(self.bn1(self.conv1(X)))
        Y = self.bn2(self.conv2(Y))
        return F.relu(Y + X)            # the identity shortcut: + X
```

Conceptually it's **boosting over features** (each block adds a correction to a running representation), and it has variants — add the shortcut at different points, change the output channel count, or concatenate instead of add (DenseNet). The identity path is the load-bearing idea, and it reappears as the residual stream of every transformer.

</details>

### 4.3 attention（简要 —— 详见阶段 5）

attention 解决了 RNN 的瓶颈：在时刻 `t`，RNN 访问过去的唯一途径是单个隐状态 `h_{t−1}`；它无法直接触及 `h_{t−2}, …, h_1`。**attention 让输出成为对*所有*过去状态的加权和**，`Σ αᵢ hᵢ`，并有 `α = softmax(a)`，其中每个权重 `aᵢ` 衡量状态 `i` 与当前 query 的相关程度。

主流的打分方式是 **scaled dot-product attention**：`aᵢ = ⟨hᵢ, x_t⟩ / √d`，其中 `d` 是向量长度，`√d` 这个除数防止点积增长到足以让 softmax 饱和。推广到 **queries、keys、values（QKV）**——query `q`，key–value 对 `(kᵢ, vᵢ)`——输出为 `Σ αᵢ vᵢ`，并有 `αᵢ = softmax(score(q, kᵢ))`：

```python
def dot_product_attention(queries, keys, values):
    d = queries.shape[-1]
    scores = torch.bmm(queries, keys.transpose(1, 2)) / math.sqrt(d)
    return torch.bmm(F.softmax(scores, dim=-1), values)
```

**Self-attention** 对 Q、K、V 使用*同一*序列，因此每个位置都关注其他所有位置。其开销特性正是与 CNN/RNN 权衡的关键：

| | CNN | RNN | Self-attention |
|---|---|---|---|
| 计算 | `O(k·n·d²)` | `O(n·d²)` | `O(n²·d)` |
| 并行化 | `O(n)` | `O(1)` | `O(n)` |
| 最大路径长度 | `O(n/k)` | `O(n)` | `O(1)` |

起决定作用的是最后两行：self-attention **完全并行**（不同于严格串行的 RNN），且**最大路径长度为 `O(1)`**——任意两个位置直接交互，所以无论距离多远，梯度与信息都只走一跳。代价是 `O(n²·d)` 的计算开销，而驯服它则是阶段 5 的议题。

一个 **transformer block** 恰好堆叠了本节的三样工具：**multi-head self-attention** 按元素间关系聚合输入，**point-wise FFN**（逐位置、权重共享的 MLP）变换每个输出，以及包裹在两者外部的 **LayerNorm + 残差连接**，让训练变得容易。

```python
def transformer_block(X):                 # X: (batch, seq_len, d)
    Y = nn.LayerNorm(...)(multi_head_attention(X, X, X) + X)   # attention + residual + LN
    return nn.LayerNorm(...)(ffn(Y) + Y)                       # FFN + residual + LN
```

把这些堆叠起来，就得到了 **BERT**（仅 encoder，擅长*编码*文本）、**GPT**（仅 decoder，擅长*生成*文本）和 **ViT**（输入图像 patch 的 transformer）背后的架构。attention 已成为与 MLP/CNN/RNN 并列的第四种基础架构——完整机制（multi-head、masking、KV-cache、位置编码、FlashAttention）在 **[阶段 5 — ML Systems Engineering](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/README.md)** 中展开。

> **硬件视角：** 调优在硬件侧有一个孪生问题，也正是 NAS 中存活下来的那部分。**Hardware-aware NAS** 不再把准确率当作唯一目标，而是在*设备延迟约束*下搜索——该约束在*实际*目标器件上测量（或建模），因为边缘设备横跨 CPU/GPU/DSP/NPU，性能相差约 100×，且功耗预算紧张，所以 FLOPs 无法很好地代表 wall-clock 延迟。讲义中的表述是 **最小化 `loss × log(latency)^β`**，让搜索在准确率与实测速度之间权衡；**MnasNet** 直接搜索手机上的延迟，**Once-for-All（OFA）** 训练一个 supernet，再*为每类设备提取专用子网络*，无需重训练——正是「设计一次，部署到多种约束」的目标。这些结构性工具还有直接的 kernel 级后果：**残差相加**与**归一化层**是 **kernel 融合**的首要目标（把 conv→BN→ReLU 融成一个 kernel；推理时把 BN 折进前一层 conv 的权重，使其*零*开销；把残差相加融进 epilogue），而 BN 与 LN 之争会改变融合 kernel 必须支持的内存访问模式。这些直接衔接 **[阶段 5 — Edge AI / Model Compression](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/Guide.md)** 以及 MLSys 深度专题中的推理栈工作——在那里，你在此调优的模型会变成你在预算下*服务*的模型（第 10 讲）。

---


<details>
<summary>English original</summary>

**4.3 Attention (brief — detail in Phase 5)**

Attention solves an RNN bottleneck: at time `t`, an RNN's only access to the past is the single hidden state `h_{t−1}`; it can't directly reach `h_{t−2}, …, h_1`. **Attention lets the output be a weighted sum over *all* past states**, `Σ αᵢ hᵢ` with `α = softmax(a)`, where each weight `aᵢ` scores how relevant state `i` is to the current query.

The dominant scorer is **scaled dot-product attention**: `aᵢ = ⟨hᵢ, x_t⟩ / √d`, where `d` is the vector length and the `√d` divisor keeps the dot products from growing large enough to saturate the softmax. Generalized to **queries, keys, and values (QKV)** — query `q`, key–value pairs `(kᵢ, vᵢ)` — the output is `Σ αᵢ vᵢ` with `αᵢ = softmax(score(q, kᵢ))`:

```python
def dot_product_attention(queries, keys, values):
    d = queries.shape[-1]
    scores = torch.bmm(queries, keys.transpose(1, 2)) / math.sqrt(d)
    return torch.bmm(F.softmax(scores, dim=-1), values)
```

**Self-attention** uses the *same* sequence for Q, K, and V, so every position attends to every other. Its cost profile is the key trade against CNN/RNN:

| | CNN | RNN | Self-attention |
|---|---|---|---|
| Computation | `O(k·n·d²)` | `O(n·d²)` | `O(n²·d)` |
| Parallelization | `O(n)` | `O(1)` | `O(n)` |
| Max path length | `O(n/k)` | `O(n)` | `O(1)` |

The decisive rows are the last two: self-attention is **fully parallel** (unlike the strictly sequential RNN) and has **`O(1)` maximum path length** — any two positions interact directly, so gradients and information travel in one hop regardless of distance. That `O(n²·d)` compute cost is the price, and taming it is a Phase 5 topic.

A **transformer block** stacks exactly the three tools in this section: **multi-head self-attention** to aggregate inputs by element-relations, a **point-wise FFN** (a per-position MLP with shared weights) to transform each output, and **LayerNorm + residual connections** wrapped around both to make training easy.

```python
def transformer_block(X):                 # X: (batch, seq_len, d)
    Y = nn.LayerNorm(...)(multi_head_attention(X, X, X) + X)   # attention + residual + LN
    return nn.LayerNorm(...)(ffn(Y) + Y)                       # FFN + residual + LN
```

Stack these and you get the architecture behind **BERT** (encoder-only, good at *encoding* text), **GPT** (decoder-only, good at *generating* text), and **ViT** (a transformer fed image patches). Attention has become a fourth fundamental architecture alongside MLP/CNN/RNN — the full mechanics (multi-head, masking, KV-cache, positional encodings, FlashAttention) are developed in **[Phase 5 — ML Systems Engineering](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/README.md)**.

> **Hardware lens:** Tuning has a hardware twin, and it's the part of NAS that survived. **Hardware-aware NAS** drops accuracy as the sole objective and searches under a *device latency constraint* — measured (or modeled) on the *actual* target, because edge devices span CPU/GPU/DSP/NPU with ~100× performance spread and hard power budgets, so FLOPs are a poor proxy for wall-clock latency. The slide's formulation is to **minimize `loss × log(latency)^β`** so the search trades accuracy against measured speed; **MnasNet** searches on-phone latency directly, and **Once-for-All (OFA)** trains one supernet and extracts a *specialized sub-network per device* with no retraining — exactly the "design once, deploy to many constraints" goal. The structural tools also have direct kernel-level consequences: **residual adds** and **norm layers** are prime **kernel-fusion** targets (fuse conv→BN→ReLU into one kernel; fold BN into the preceding conv's weights at inference so it costs *nothing*; fuse the residual add into the epilogue), and BN-vs-LN changes the memory-access pattern a fused kernel must support. These connect straight to **[Phase 5 — Edge AI / Model Compression](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/Guide.md)** and the inference-stack work in the MLSys deep dives — where the model you tuned here becomes a model you *serve* under a budget (Lecture 10).

---

</details>

## 更新至

**2026 年 6 月。** HPO 算法（random > grid、贝叶斯优化、Successive Halving/Hyperband/ASHA、BOHB）与深度网络结构工具箱（BN/LN、残差、attention）仍作为 CS329P 中经久不衰的原始内容讲授——它们并未过时，依然是完全正确的心智模型。需要重新框定的是 **NAS**：CS329P（2021）将其呈现为一门刚具备实用性的技术，而 2026 年的头号修正在于**经典的从零开始 NAS 已大幅退潮**——迁移学习、scaling laws 与基础模型取代了「从零搜索一个小网络」，AutoML 在表格数据上存续得最好（AutoGluon），硬件感知 NAS 在边缘侧延续（MnasNet、Once-for-All），而日常的 HPO 跑在 **Optuna 与 Ray Tune** 上，而非手写的搜索。先讲原始内容，因为它仍是*思考*该问题的正确方式；更新之处加以标注，而非悄悄替换。

*改编自 [Stanford CS329P](https://c.d2l.ai/stanford-cs329p) — Huang, Li & Smola, CC-BY-SA-4.0。*


<details>
<summary>English original</summary>

**Current as of**

**June 2026.** The HPO algorithms (random > grid, Bayesian optimization, Successive Halving/Hyperband/ASHA, BOHB) and the deep-net structural toolkit (BN/LN, residuals, attention) are taught as the durable, original CS329P material — they have not dated and remain exactly the right mental model. The reframing is **NAS**: CS329P (2021) presents it as freshly practical, and the headline 2026 correction is that **classic from-scratch NAS has largely faded** — transfer learning, scaling laws, and foundation models replaced "search a small net from scratch," AutoML survives best for tabular data (AutoGluon), hardware-aware NAS persists at the edge (MnasNet, Once-for-All), and day-to-day HPO runs on **Optuna and Ray Tune**, not hand-rolled search. The original is taught first because it is still how to *think* about the problem; the update is flagged, not silently substituted.

*Adapted from [Stanford CS329P](https://c.d2l.ai/stanford-cs329p) — Huang, Li & Smola, CC-BY-SA-4.0.*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/4. ML Engineering and MLOps/Practical Machine Learning (CS329P)/Lecture-08.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/4.%20ML%20Engineering%20and%20MLOps/Practical%20Machine%20Learning%20%28CS329P%29/Lecture-08.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
