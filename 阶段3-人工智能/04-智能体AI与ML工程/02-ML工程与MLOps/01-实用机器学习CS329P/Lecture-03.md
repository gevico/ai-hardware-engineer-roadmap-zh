---
title: Lecture 03 - ML 模型回顾：树、线性与神经网络
description: Lecture 03 - ML 模型回顾：树、线性与神经网络
---

# Lecture 03 - ML 模型回顾：树、线性与神经网络

**Collection:** [Practical Machine Learning (CS329P)](README.md) | **Previous:** [← Lecture 02](Lecture-02.md) | **Next:** [Lecture 04](Lecture-04.md)

---

前两讲把数据搬进了大楼，并把它变成了干净、条件良好的特征。本讲盘点的是这些工作完成之后你会去取用的模型。它刻意*不是*一门从零开始推导的课程——没有最小二乘的闭式证明，没有反向传播的链式法则，也没有收敛性分析。那些内容属于理论课。这是实践者的工作集：你在真实的 ML 岗位上真正会去拟合的四类模型，以及——更重要的是——*何时该取用哪一类*的决策。在实践中，知道 softmax 怎么推导，远不如知道梯度提升树很可能在你面前这份表格数据上打败你的神经网络、以及为什么，来得重要。

CS329P 把每个监督模型都拆成三个可互换的部分：**模型**（从输入到预测的参数化函数）、**损失**（预测错到什么程度）和 **优化** 过程（如何移动参数以降低损失）。下面几乎所有内容，都是在这同一副骨架上对这三个部分做出不同的选择。树把优化器换成了贪心的递归划分；线性方法与神经网络共享 mini-batch SGD，差别只在于模型函数的表达能力有多强。以这种方式看待这些模型族——把它们看作 `model + loss + optimization` 的不同变体，而不是互不相关的算法——正是它让你只问这三个问题，就能对一个从未见过的新方法做出推理。

回报是一张你能记在脑子里的图：**表格数据 → 梯度提升树；图像 → CNN；序列与文本 → Transformer；一切小规模或对可解释性至关重要的场景 → 线性。** 本讲余下部分将挣得那张图，结尾的决策表会让它落到实处。把它当作地形图，随后 Lecture 04 会教你*信任*模型给出的分数，Lecture 05 会教你*组合*其中若干个。

---

## 学习目标

本讲结束时，你应能够：

1. **把任意监督模型拆解**成三个部分——模型函数、损失与优化——并把一个问题归类为监督、半监督/自监督、无监督或强化学习。
2. **解释 iid 假设**，以及泛化（测试集表现）才是真正的目标，而非训练集拟合。
3. **逐步搭建树家族**：从单棵决策树到 Random Forest（bagging）再到梯度提升树，并说出其优点（可解释、原生支持混合/表格数据、几乎无需预处理）与缺点（边界不平滑、不稳定）。
4. **把线性回归、softmax/logistic 回归与 MLP 串成**一条演进线——先是线性层，然后是分类头，再是堆叠的线性加非线性层——并说明正则化和线性决策边界能给你带来什么。
5. **把神经网络架构与数据结构相匹配**——向量用 MLP，图像用 CNN（局部性 + 参数共享），序列用 RNN，长程依赖用 Transformer。
6. **为给定数据集挑出正确的模型族**，依据一条站得住脚的决策规则，并推理每一族在推理时对硬件的截然不同要求。

---

## 1. ML 模型全景


<details>
<summary>English original</summary>

**Lecture 03 - ML Models Recap: Trees, Linear, and Neural Nets**

**Collection:** [Practical Machine Learning (CS329P)](README.md) | **Previous:** [← Lecture 02](Lecture-02.md) | **Next:** [Lecture 04](Lecture-04.md)

---

The last two lectures got data into the building and turned it into clean, well-conditioned features. This lecture is the inventory of models you reach for once that work is done. It is deliberately *not* a from-scratch derivation course — there is no closed-form least-squares proof, no backpropagation chain rule, no convergence analysis. Those live in a theory class. This is the practitioner's working set: the four model families you will actually fit in a real ML role, and — more importantly — the decision of *when to reach for each one*. Knowing how to derive softmax matters far less in practice than knowing that a gradient-boosted tree will probably beat your neural net on the tabular dataset in front of you, and why.

CS329P frames every supervised model as three interchangeable parts: a **model** (a parameterized function from inputs to a prediction), a **loss** (how wrong a prediction is), and an **optimization** procedure (how you move the parameters to reduce the loss). Almost everything below is a different choice of those three parts bolted to the same scaffold. Trees swap out the optimizer for a greedy recursive split; linear methods and neural networks share mini-batch SGD and differ only in how expressive the model function is. Seeing the families this way — as variations on `model + loss + optimization` rather than as unrelated algorithms — is what lets you reason about a new method you have never seen by asking only those three questions.

The payoff is a single chart you can hold in your head: **tabular data → gradient-boosted trees; images → CNN; sequences and text → Transformer; everything small or interpretability-critical → linear.** The rest of this lecture earns that chart, and the closing decision table makes it concrete. Treat this as the map of the territory before Lecture 04 teaches you to *trust the score* a model gives you and Lecture 05 teaches you to *combine* several of them.

---

**Learning objectives**

By the end of this lecture you should be able to:

1. **Decompose any supervised model** into its three parts — model function, loss, and optimization — and classify a problem as supervised, semi-/self-supervised, unsupervised, or reinforcement learning.
2. **Explain the iid assumption** and how generalization (test-set performance) is the actual goal, not training-set fit.
3. **Build up the tree family** from a single decision tree to Random Forest (bagging) to gradient-boosted trees, and state the pros (interpretable, native mixed/tabular data, little preprocessing) and cons (no smooth boundaries, unstable).
4. **Connect linear regression, softmax/logistic regression, and the MLP** as a single progression — a linear layer, then a classification head, then stacked linear-plus-nonlinear layers — and say what regularization and the linear decision boundary buy you.
5. **Match a neural architecture to a data structure** — MLP for vectors, CNN for images (locality + parameter sharing), RNN for sequences, Transformer for long-range dependencies.
6. **Pick the right model family for a given dataset** using a defensible decision rule, and reason about the very different hardware each family demands at inference.

---

**1. The ML model landscape**

</details>

### 1.1 学习的类型

在谈模型之前，先看学习的 *设定* —— 你拥有哪一类监督信号，决定了哪些算法还有资格上场。

| 类型 | 训练数据 | 思路 | 示例 |
|---|---|---|---|
| **监督学习** | 有标注数据 | 学习从输入到已知标签的映射 | 房源信息 → 成交价 |
| **半监督学习** | 有标注 + 无标注 | 用模型为无标注部分推断标签，再从两者一起学习 | 自训练 |
| **无监督学习** | 无标注数据 | 在没有目标的情况下发现结构 | 聚类、密度估计 |
| **自监督学习** | 无标注数据 | 从数据本身*生成*标签，使无监督问题变成有监督问题 | word2vec、BERT（遮住一个词，预测它） |
| **强化学习** | 交互 | 在环境中采取动作以最大化奖励信号 | 玩游戏、机器人 |

其中两类是刻意模糊的。**自监督学习**是打开现代 NLP 与大部分视觉领域的钥匙：没有人标注的标签，但你可以*在原始数据上设计一个有监督任务* —— 遮住一个词并预测它（BERT）、预测下一个 token（GPT），或者用一个人为已知的「假」标签来生成假样本（GANs）。CS329P 强调的关键微妙之处在于：*训练任务可以与模型最终的评测方式或使用方式不同*：你在「填空」这个没人在意的任务上预训练 BERT，正是为了让学到的表示迁移到你真正在意的任务上。训练目标与最终用途的这种解耦，是基础模型时代的引擎（第 09 讲）。

### 1.2 有监督训练的三大组成部分

本讲中每一个有监督模型都是同一副骨架，只是换上了不同的部件：

```text
   Model         a parameterized function  f(x; θ)  mapping input x to a prediction
                   (parameters θ are learned; hyperparameters are set by you)
   Loss          a number measuring how bad one prediction is
                   (squared error, cross-entropy, contrastive, triplet, ranking, ...)
   Objective     the thing to minimize — usually the mean loss over all examples
   Optimization  the algorithm that adjusts θ to minimize the objective (SGD, greedy splits, ...)
```

**模型参数**（权重 `w`、偏置 `b` —— 从数据中学习）与**超参数**（学习率、树深度、层数 —— 由你设定，在第 08 讲中调优）之间的区分贯穿整个课程。当你「训练一个模型」时，你是在跑优化以找到好的参数；当你「调一个模型」时，你是在超参数上做搜索。在脑子里把这两者分开，机器学习的大部分内容就不再令人困惑。

CS329P 点出的四个有监督家族，按模型函数如何构建来划分：

- **决策树** —— 通过走一棵由是/否问题构成的树来做预测。
- **线性方法** —— 由输入特征的线性组合来预测。
- **kernel 机器** —— 通过 kernel 函数计算特征*相似度*（SVM 一脉；这里只顺带一提）。
- **神经网络** —— *学习*特征表示，而非手工设计。

### 1.3 iid 假设与泛化

几乎所有有监督学习都建立在一个不言明的前提上：训练数据与未来的数据是从同一个底层分布中**独立同分布（iid）**地抽取的。*同分布*意味着测试世界看起来像训练世界；*独立*意味着一个样本不会告诉你关于下一个样本的任何信息。正是这个假设，让训练集上的平均值成为未来性能的合理代理 —— 而第 06–07 讲整篇都在讨论它被打破时该怎么办（分布偏移；样本*不*独立的序列与图）。

目标从来不是拟合训练集 —— 那太简单了；背下来就行。目标是**泛化**：在同一分布中*未见过的*数据上保持低误差。在训练上表现完美却在测试上失败，是**过拟合**；两者都失败，是**欠拟合**。训练拟合程度与真实性能之间的这个差距，是实用机器学习中最重要的单一概念，第 04 讲专门讨论如何诚实地度量它。看下面每一个模型时都记住这一点：表达能力是一把双刃剑，因为一个强大到能拟合任何模式的模型，也强大到能拟合噪声。

---

## 2. 树方法

树是表格型机器学习的干将，当你的数据存放在电子表格里时，它是第一个该抓来的家族。


<details>
<summary>English original</summary>

**1.1 Types of learning**

Before models, the learning *setting* — what kind of supervision you have decides which algorithms are even on the table.

| Type | Trains on | Idea | Example |
|---|---|---|---|
| **Supervised** | Labeled data | Learn a map from inputs to known labels | Listing → sale price |
| **Semi-supervised** | Labeled + unlabeled | Use a model to infer labels for the unlabeled part, then learn from both | Self-training |
| **Unsupervised** | Unlabeled data | Find structure with no targets | Clustering, density estimation |
| **Self-supervised** | Unlabeled data | *Generate* labels from the data itself, so an unsupervised problem becomes a supervised one | word2vec, BERT (mask a word, predict it) |
| **Reinforcement learning** | Interaction | Take actions in an environment to maximize a reward signal | Game-playing, robotics |

Two of these blur on purpose. **Self-supervised learning** is the trick that unlocked modern NLP and much of vision: there are no human labels, but you can *design a supervised task* over raw data — hide a word and predict it (BERT), predict the next token (GPT), or generate fake samples with a trivially-known "fake" label (GANs). The crucial subtlety CS329P stresses is that *the training task can differ from how the model is ultimately evaluated or used*: you pre-train BERT on mask-filling, a task nobody cares about, precisely so the learned representations transfer to the task you do care about. That decoupling of training objective from end use is the engine of the foundation-model era (Lecture 09).

**1.2 The three components of supervised training**

Every supervised model in this lecture is the same scaffold with different parts swapped in:

```text
   Model         a parameterized function  f(x; θ)  mapping input x to a prediction
                   (parameters θ are learned; hyperparameters are set by you)
   Loss          a number measuring how bad one prediction is
                   (squared error, cross-entropy, contrastive, triplet, ranking, ...)
   Objective     the thing to minimize — usually the mean loss over all examples
   Optimization  the algorithm that adjusts θ to minimize the objective (SGD, greedy splits, ...)
```

The distinction between **model parameters** (weights `w`, bias `b` — learned from data) and **hyperparameters** (learning rate, tree depth, number of layers — set by you, tuned in Lecture 08) runs through the whole course. When you "train a model" you are running the optimization to find good parameters; when you "tune a model" you are searching over hyperparameters. Keep them separate in your head and most of ML stops being confusing.

The four supervised families CS329P names, by how the model function is built:

- **Decision trees** — make a prediction by walking a tree of yes/no questions.
- **Linear methods** — predict from a linear combination of input features.
- **Kernel machines** — compute feature *similarities* via a kernel function (the SVM lineage; we touch them only in passing).
- **Neural networks** — *learn* the feature representation rather than hand-craft it.

**1.3 The iid assumption and generalization**

Almost all of supervised learning rests on one quiet premise: training and future data are drawn **independently and identically distributed (iid)** from the same underlying distribution. *Identically* means the test world looks like the training world; *independently* means one example tells you nothing about the next. This is the assumption that makes a training-set average a sensible proxy for future performance — and Lectures 06–07 are entirely about what to do when it breaks (distribution shift; sequences and graphs where samples are *not* independent).

The goal is never to fit the training set — that is trivial; memorize it. The goal is **generalization**: low error on *unseen* data from the same distribution. A model that nails training and fails test has **overfit**; one that fails both has **underfit**. This gap between training fit and true performance is the single most important idea in practical ML, and Lecture 04 is devoted to measuring it honestly. Hold it in mind for every model below: expressiveness is a double-edged sword, because a model powerful enough to fit any pattern is also powerful enough to fit the noise.

---

**2. Tree methods**

Trees are the workhorse of tabular ML and the first family to reach for when your data lives in a spreadsheet.

</details>

### 2.1 决策树

决策树通过从根节点向下走过一系列是/否问题来预测，直到抵达存放答案的叶子节点。它用同一套结构处理 **classification**（“has enough data?” → branch → “improve label?” → leaf）与 **regression**（“is in Palo Alto?” → “living sqft > 2k?” → `price = $2.8M`）——叶子在分类时存放类别，在回归时存放数值。

它突出的优点，直接来自讲义：

- **可解释。** 决策路径*就是*解释——你可以直接读出预测为何如此，这在受监管场景（信贷、医疗）中极其重要。
- **同时处理数值特征与类别特征，且无需预处理。** 无需归一化，无需 one-hot 编码，无需缩放。树可以原生地切分 `sqft > 2000` 和 `city == "Palo Alto"`。经历了 Lecture 02 的预处理折磨之后，这确实让人松一口气——树几乎不受特征尺度和单调变换的影响。

树的构建是**自顶向下且贪心**的。从根节点开始，持有全部样本与全部特征。在每个节点上，选出最能区分样本的那一个 特征-阈值 切分，然后对每个子节点递归。“最佳”由切分准则衡量：

| 准则 | 目标类型 | 最大化 |
|---|---|---|
| **方差下降** | 连续 | 切分后目标方差的下降 |
| **信息增益**（`1 − entropy`） | 类别 | 熵（无序度）的下降 |
| **Gini 不纯度**（`1 − Σ pᵢ²`） | 类别 | 不纯度的下降 |

节点上的所有样本都参与选择该节点的切分。当某个节点纯净、过小或触达深度上限时，树停止生长。

### 2.2 为什么单棵树不够

两个局限促使你走出单棵树，它们也是后续一切内容的动因：

- **树会过拟合。** 不加约束的树会一直生长到每个叶子只含一个训练样本——训练准确率完美，泛化能力极差。对抗手段是*限制深度*（更少的切分层级），以及对在留出数据上不划算的分支进行*剪枝*。
- **树不稳定。** 改动少量训练样本就可能翻转早期的一个切分，并在下游产生完全不同的树。这种高*方差*（Lecture 05 的术语）是单棵树的深层缺陷——也是集成方法占据主导的原因。

还有一个实际麻烦：树的*构建* **难以并行化**，因为每个切分都依赖于它上面的切分。这种顺序依赖正是树的硬件图景与神经网络如此不同的原因（见 Hardware lens）。

### 2.3 Random Forest——用 bagging 消除不稳定性

解决不稳定性的办法是训练**许多棵树并取平均**。Random Forest 训练一片决策树森林并将它们组合——分类用**多数投票**，回归用**平均**。其鲁棒性完全来自*注入的随机性*，来源有两个：

- **Bagging（bootstrap aggregating）。** 每棵树都在一个 bootstrap 样本上训练——*有放回地*抽取 `n` 个样本，因此 `[1,2,3,4,5]` 可能变成 `[1,2,2,3,4]`。每棵树看到的数据集都略有不同，所以它们的误差部分独立，会被平均掉。
- **随机特征子集。** 每次切分只考虑随机的一部分特征，这进一步降低树之间的相关性，使任何单一主导特征都无法让所有树变得一样。

因为树之间相互独立，它们可以**并行训练**——找回了单棵树所缺乏的并行性。Random Forest 是简单、鲁棒的默认选择，把不稳定的单棵树变成可信的东西，而且几乎不需要调参。


<details>
<summary>English original</summary>

**2.1 The decision tree**

A decision tree predicts by walking from the root down a series of yes/no questions until it reaches a leaf, which holds the answer. It handles **classification** ("has enough data?" → branch → "improve label?" → leaf) and **regression** ("is in Palo Alto?" → "living sqft > 2k?" → `price = $2.8M`) with the same structure — a leaf simply holds a class for classification or a number for regression.

Its standout virtues, straight from the slides:

- **Explainable.** The decision path *is* the explanation — you can read off exactly why a prediction was made, which matters enormously in regulated settings (credit, healthcare).
- **Handles numerical and categorical features together, with no preprocessing.** No normalization, no one-hot encoding, no scaling. A tree splits `sqft > 2000` and `city == "Palo Alto"` natively. After Lecture 02's preprocessing gauntlet, this is a genuine relief — trees are nearly immune to feature scale and monotone transforms.

Trees are built **top-down and greedily**. Start at the root with all examples and all features. At each node, pick the one feature-and-threshold split that best separates the examples, then recurse on each child. "Best" is measured by a split criterion:

| Criterion | Target type | Maximize |
|---|---|---|
| **Variance reduction** | Continuous | Drop in target variance after the split |
| **Information gain** (`1 − entropy`) | Categorical | Reduction in entropy (disorder) |
| **Gini impurity** (`1 − Σ pᵢ²`) | Categorical | Reduction in impurity |

All examples at a node participate in choosing its split. The tree stops when a node is pure, too small, or hits a depth limit.

**2.2 Why a single tree is not enough**

Two limitations push you past one tree, and they motivate everything that follows:

- **Trees overfit.** An unconstrained tree grows until each leaf is a single training example — perfect training accuracy, terrible generalization. You fight this by *limiting depth* (fewer levels of splitting) and *pruning* branches that don't pay their way on held-out data.
- **Trees are unstable.** Changing a handful of training examples can flip an early split and produce a completely different tree downstream. This high *variance* (Lecture 05's term) is the deep flaw of single trees — and the reason ensembles dominate.

One more practical wrinkle: tree *building* is **hard to parallelize** because each split depends on the one above it. This sequential dependence is exactly why the hardware story for trees looks so different from neural nets (see the Hardware lens).

**2.3 Random Forest — bagging away the instability**

The fix for instability is to train **many trees and average them**. A Random Forest trains a forest of decision trees and combines them — **majority vote** for classification, **average** for regression. The robustness comes entirely from *injected randomness*, from two sources:

- **Bagging (bootstrap aggregating).** Each tree trains on a bootstrap sample — draw `n` examples *with replacement*, so `[1,2,3,4,5]` might become `[1,2,2,3,4]`. Every tree sees a slightly different dataset, so their errors are partly independent and average out.
- **Random feature subsets.** At each split, consider only a random subset of features, which decorrelates the trees further so no single dominant feature makes them all the same.

Because the trees are independent, they **train in parallel** — recovering the parallelism a single tree lacked. Random Forest is the easy, robust default that turns the unstable single tree into something you can trust, and it needs very little tuning.

</details>

### 2.4 梯度提升 —— 表格数据之王

Bagging *并行*构建树以降低方差。**Boosting** 则*串行*构建树，每棵新树都在纠正当前集成的错误 —— 拟合当前集成仍然预测错的残差，再以较小的学习率把它加进去。（bagging 与 boosting 在偏差–方差上的对比属于 Lecture 05 的范围；这里只需记住，boosting 是组合树的另一种方式。）

实践中几乎不会自己手写它 —— 你会直接取用两个库之一，而它们是应用机器学习中最重要的工具之一：

| 库 | 优势 | 适用场景 |
|---|---|---|
| **XGBoost** | 久经考验、带正则化、生态庞大 | 表格类竞赛与生产环境的安全默认选择 |
| **LightGBM** | 基于直方图的分裂、按叶生长、大数据上更快 | 训练速度重要的大规模表格数据集 |

**截至 2026 年，梯度提升树（GBT）仍然是表格数据上最好的通用模型** —— 在结构化/表格类问题上，它们常常胜过深度网络，这是本次课最重要的一个实践事实。当数据是行与列时，你的第一选择是 XGBoost 或 LightGBM，而不是神经网络。

```python
# Gradient-boosted trees: the tabular default
from xgboost import XGBClassifier
clf = XGBClassifier(
    n_estimators=300,      # number of sequential trees
    max_depth=6,           # shallow trees, boosted — controls overfitting
    learning_rate=0.05,    # shrinkage: each tree contributes a little
)
clf.fit(X_train, y_train)  # native mixed types, no scaling needed
```

### 2.5 树模型家族 —— 优点与缺点

| 优点 | 缺点 |
|---|---|
| 可解释（单棵树）；集成可给出特征重要性 | **没有平滑的决策边界** —— 预测是轴对齐的阶梯，对本质平滑/连续的关系表现差 |
| 原生处理数值 + 类别混合特征 | 单棵树**不稳定**（高方差）—— 只能靠集成解决 |
| 几乎无需预处理 —— 尺度不变，不需要归一化 | 提升树若长得过大可能过拟合；需要自己的调参 |
| 在表格数据上表现出色；训练和调参都快 | 不原生处理非结构化数据（像素、原始文本、音频） |

---

## 3. 线性方法

线性方法是最简单的模型家族，也是神经网络生长的概念种子。掌握本节之后，下一节基本只是记账。

### 3.1 线性回归

把实数预测为特征的加权和加上偏置。对于一套有 `x₁` 间卧室、`x₂` 间浴室、`x₃` 平方英尺居住面积的房子：

```text
   ŷ = w₁·x₁ + w₂·x₂ + w₃·x₃ + b
```

一般地，对于输入 `x = [x₁, …, xₚ]`：

```text
   ŷ = ⟨w, x⟩ + b           (inner product of weights and features, plus bias)
```

权重 `w = [w₁, …, wₚ]` 和偏置 `b` 是学习到的参数。损失是**均方误差（MSE）** —— 在 `n` 个训练样本上对预测值与真实值之差的平方取平均：

```text
   w*, b*  =  argmin  (1/n) · Σᵢ (yᵢ − ⟨xᵢ, w⟩ − b)²
              w, b
```

MSE 有闭式解（值得一做练习），但你很少用它 —— 对于任何规模较大的问题，以及本节之后的每一个模型，你都改用迭代方式优化。

### 3.2 小批量随机梯度下降

除了树之外驱动一切模型的优化器。步骤：

- 随机初始化权重。
- 重复直到收敛：随机采样一个由 `b` 个样本组成的**小批量**，只在该批上计算损失的梯度，并让参数沿下坡方向走一步：`w ← w − η · ∇w ℓ`。

这里 `b` 是批大小，`η` 是学习率。**优点：** 它能求解本课程中除树以外的*每一个*目标函数 —— 同一个算法可以训练线性回归、softmax 回归和深度神经网络。**缺点：** 它对超参数敏感；`b` 或 `η` 选得不好，训练就会发散或慢如爬行（Lecture 08 讲如何调）。

```python
# Mini-batch SGD for linear regression (the pattern behind all NN training)
w = torch.normal(0, 0.01, size=(p, 1), requires_grad=True)
b = torch.zeros(1, requires_grad=True)

for epoch in range(num_epochs):
    for X, y in data_iter(batch_size, features, labels):   # random mini-batches
        y_hat = X @ w + b
        loss = ((y_hat - y) ** 2 / 2).mean()               # MSE
        loss.backward()                                    # gradients
        for param in (w, b):
            param -= learning_rate * param.grad            # SGD step
            param.grad.zero_()
```


<details>
<summary>English original</summary>

**2.4 Gradient Boosting — the tabular champion**

Bagging builds trees *in parallel* to reduce variance. **Boosting** builds them *sequentially*, each new tree correcting the errors of the ensemble so far — fitting the residual the current ensemble still gets wrong, then adding it in with a small learning rate. (The bias–variance contrast between bagging and boosting is Lecture 05's territory; here, just register that boosting is the other way to combine trees.)

In practice you almost never hand-roll this — you reach for one of two libraries, and they are among the most important tools in applied ML:

| Library | Edge | Use when |
|---|---|---|
| **XGBoost** | Battle-tested, regularized, huge ecosystem | The safe default for tabular competitions and production |
| **LightGBM** | Histogram-based splits, leaf-wise growth, faster on large data | Big tabular datasets where training speed matters |

**Gradient-boosted trees (GBT) are, as of 2026, still the best general-purpose model for tabular data** — they routinely beat deep nets on structured/spreadsheet problems, which is the single most important practical fact in this lecture. When the data is rows and columns, your first move is XGBoost or LightGBM, not a neural network.

```python
# Gradient-boosted trees: the tabular default
from xgboost import XGBClassifier
clf = XGBClassifier(
    n_estimators=300,      # number of sequential trees
    max_depth=6,           # shallow trees, boosted — controls overfitting
    learning_rate=0.05,    # shrinkage: each tree contributes a little
)
clf.fit(X_train, y_train)  # native mixed types, no scaling needed
```

**2.5 Tree family — pros and cons**

| Pros | Cons |
|---|---|
| Interpretable (single tree); feature-importance for ensembles | **No smooth decision boundaries** — predictions are axis-aligned staircases, bad for inherently smooth/continuous relationships |
| Native handling of mixed numerical + categorical features | A single tree is **unstable** (high variance) — fixed only by ensembling |
| Little to no preprocessing — scale-invariant, no normalization | Boosted trees can overfit if over-grown; need their own tuning |
| Excellent on tabular data; fast to train and tune | Don't natively handle unstructured data (pixels, raw text, audio) |

---

**3. Linear methods**

Linear methods are the simplest model family and the conceptual seed from which neural networks grow. Master this section and the next one is mostly bookkeeping.

**3.1 Linear regression**

Predict a real number as a weighted sum of features plus a bias. For a house with `x₁` beds, `x₂` baths, `x₃` living-sqft:

```text
   ŷ = w₁·x₁ + w₂·x₂ + w₃·x₃ + b
```

and in general, for input `x = [x₁, …, xₚ]`:

```text
   ŷ = ⟨w, x⟩ + b           (inner product of weights and features, plus bias)
```

The weights `w = [w₁, …, wₚ]` and bias `b` are the learned parameters. The loss is **mean squared error (MSE)** — average the squared gap between prediction and truth over `n` training examples:

```text
   w*, b*  =  argmin  (1/n) · Σᵢ (yᵢ − ⟨xᵢ, w⟩ − b)²
              w, b
```

MSE has a closed-form solution (a worthwhile exercise), but you rarely use it — for anything large, and for every model after this one, you optimize iteratively instead.

**3.2 Mini-batch stochastic gradient descent**

The optimizer that powers everything except trees. The recipe:

- Randomly initialize the weights.
- Repeat until convergence: sample a small random **mini-batch** of `b` examples, compute the gradient of the loss on just that batch, and step the parameters downhill: `w ← w − η · ∇w ℓ`.

Here `b` is the batch size and `η` the learning rate. **Pros:** it solves *every* objective in this course except trees — the same algorithm trains linear regression, softmax regression, and deep neural networks. **Cons:** it is sensitive to its hyperparameters; pick `b` or `η` badly and training diverges or crawls (Lecture 08 tunes them).

```python
# Mini-batch SGD for linear regression (the pattern behind all NN training)
w = torch.normal(0, 0.01, size=(p, 1), requires_grad=True)
b = torch.zeros(1, requires_grad=True)

for epoch in range(num_epochs):
    for X, y in data_iter(batch_size, features, labels):   # random mini-batches
        y_hat = X @ w + b
        loss = ((y_hat - y) ** 2 / 2).mean()               # MSE
        loss.backward()                                    # gradients
        for param in (w, b):
            param -= learning_rate * param.grad            # SGD step
            param.grad.zero_()
```

</details>

### 3.3 从回归到分类：softmax / 逻辑回归

要对 `m` 个类别做分类，先试朴素路线：把标签编码成 **one-hot** 向量（`y = [0,…,1,…,0]`，真实类别所在的位置为 1），对每个类别拟合一个线性模型 `oᵢ = ⟨x, wᵢ⟩ + bᵢ`，用 MSE 训练并预测 `argmaxᵢ oᵢ`。它可行，但会**浪费模型容量**，迫使非目标类别的输出精确趋向 0，而我们真正在意的只是它们的*相对顺序*。

**Softmax 回归**解决了这个问题。把原始分数 `o`（即 *logits*）过一遍 softmax，转成概率分布——非负、求和为 1：

```text
   ŷᵢ = softmax(o)ᵢ = exp(oᵢ) / Σₖ exp(oₖ)
```


然后用**交叉熵损失**训练，它把预测分布 `ŷ` 与真实分布 `y` 做比较，对于 one-hot 标签可简化为 `−log ŷ_(true class)`：

```text
   H(y, ŷ) = − Σᵢ yᵢ · log ŷᵢ = − log ŷ_y
```


妙处在于：交叉熵只惩罚*真实*类别的预测概率，因此只要错误类别的 logits 保持在正确类别之下，模型就不再费力把它们推向任何特定值。关键在于，**softmax 回归仍然是线性模型**——决策是在输入的线性变换上作出的，因为 `argmaxᵢ ŷᵢ = argmaxᵢ oᵢ`。softmax 只是对不可微的 argmax 的一个平滑、可微的替代品。它的两类特例就是**逻辑回归**，业界部署最广的分类器。

```python
# Softmax classification head: a dense layer + cross-entropy
logits = X @ W + b                    # raw scores, shape (batch, num_classes)
loss = F.cross_entropy(logits, y)     # softmax + cross-entropy in one stable op
```


### 3.4 线性决策边界与正则化

线性分类器用一张单一的**平直超平面**切割输入空间——一侧预测类别 A，另一侧预测类别 B。这既是它的能力（简单、快速、可解释——每个权重就是某个特征带符号的重要性），也是它的天花板：线性不可分的数据（经典的 XOR 模式）无论权重如何，都无法被任何一条直线完美分开。突破这一天花板正是 §4 的主题。

当特征数相对于样本数很多时，无约束的线性模型会过拟合——它拟合的是噪声。**正则化**通过惩罚大权重把它拉回来，在目标函数中加一项：

- **L2 (ridge)：** 加 `λ‖w‖²`——把权重平滑地压向零，是标准的稳定化手段。
- **L1 (lasso)：** 加 `λ‖w‖₁`——把部分权重压到*恰好*为零，顺带完成特征选择。

旋钮 `λ` 在训练拟合度与权重规模之间做权衡；它是需要调的超参数（Lecture 08），也是你抵御过拟合的第一道、最廉价的防线——Lecture 04 教你怎么发现过拟合。

### 3.5 线性方法 → MLP

这里是通往神经网络的桥梁，而且它比看上去要小。一个**稠密（全连接、线性）层**，权重矩阵为 `W ∈ ℝ^{m×n}`、偏置为 `b ∈ ℝ^m`，计算 `y = Wx + b`——即输入的 `m` 个线性组合构成的向量。用这套语言来说：

- **线性回归** = 输出为 **1** 的稠密层。
- **Softmax 回归** = 输出为 **m** 的稠密层，后接 softmax。

把两个稠密层叠起来，你得到的……仍然是线性模型——两个线性映射的复合还是一个线性映射，所以什么也没多出来。缺的那味料是**非线性**，而正是这一项补充把线性方法变成了神经网络。

---

## 4. 神经网络

神经网络的定义性动作：不再把*手工设计*的特征喂给线性 / softmax 模型，而是让网络端到端地**学习特征**。代价是更多数据和更多计算；回报是模型能自建表示，并在所有非结构化模态上占据主导。


<details>
<summary>English original</summary>

**3.3 From regression to classification: softmax / logistic regression**

To classify into `m` classes, first try the naive route: encode the label as a **one-hot** vector (`y = [0,…,1,…,0]`, a 1 in the true class's slot) and fit a linear model per class, `oᵢ = ⟨x, wᵢ⟩ + bᵢ`, training with MSE and predicting `argmaxᵢ oᵢ`. It works but **wastes model capacity** forcing the off-class outputs toward exactly 0 when all we care about is their *order*.

**Softmax regression** fixes this. Pass the raw scores `o` (the *logits*) through softmax to turn them into a probability distribution — non-negative, summing to 1:

```text
   ŷᵢ = softmax(o)ᵢ = exp(oᵢ) / Σₖ exp(oₖ)
```

Then train with **cross-entropy loss**, which compares the predicted distribution `ŷ` to the true distribution `y` and, for a one-hot label, simplifies to `−log ŷ_(true class)`:

```text
   H(y, ŷ) = − Σᵢ yᵢ · log ŷᵢ = − log ŷ_y
```

The elegance: cross-entropy only penalizes the *true* class's predicted probability, so the model stops wasting effort pushing wrong-class logits to any particular value as long as they stay below the right one. Crucially, **softmax regression is still a linear model** — the decision is made on a linear transformation of the input, since `argmaxᵢ ŷᵢ = argmaxᵢ oᵢ`. The softmax is just a smooth, differentiable stand-in for the non-differentiable argmax. The two-class special case of this is **logistic regression**, the most widely deployed classifier in industry.

```python
# Softmax classification head: a dense layer + cross-entropy
logits = X @ W + b                    # raw scores, shape (batch, num_classes)
loss = F.cross_entropy(logits, y)     # softmax + cross-entropy in one stable op
```

**3.4 The linear decision boundary, and regularization**

A linear classifier carves the input space with a single **straight hyperplane** — on one side it predicts class A, on the other class B. That is its power (simple, fast, interpretable — each weight is the signed importance of a feature) and its ceiling: data that isn't linearly separable (the classic XOR pattern) cannot be perfectly split by any line, no matter the weights. Breaking past that ceiling is exactly what §4 is about.

When features are many relative to examples, an unconstrained linear model overfits — it fits noise. **Regularization** reins it in by penalizing large weights, adding a term to the objective:

- **L2 (ridge):** add `λ‖w‖²` — shrinks weights smoothly toward zero, the standard stabilizer.
- **L1 (lasso):** add `λ‖w‖₁` — drives some weights to *exactly* zero, doing feature selection for free.

The knob `λ` trades training fit against weight size; it is a hyperparameter you tune (Lecture 08), and it is your first and cheapest defense against the overfitting that Lecture 04 teaches you to detect.

**3.5 Linear methods → MLP**

Here is the bridge to neural networks, and it is smaller than it looks. A **dense (fully-connected, linear) layer** with weight matrix `W ∈ ℝ^{m×n}` and bias `b ∈ ℝ^m` computes `y = Wx + b` — a vector of `m` linear combinations of the inputs. In this language:

- **Linear regression** = a dense layer with **1** output.
- **Softmax regression** = a dense layer with **m** outputs, followed by softmax.

Stack two dense layers and you get… still a linear model — the composition of two linear maps is one linear map, so nothing is gained. The missing ingredient is **nonlinearity**, and that single addition is what turns linear methods into neural networks.

---

**4. Neural networks**

The defining move of neural networks: instead of feeding *hand-crafted* features to a linear/softmax model, you let the network **learn the features** end-to-end. The price is more data and more computation; the prize is models that build their own representations and dominate every unstructured modality.

</details>

### 4.1 多层感知机（MLP）

在全连接层之间插入逐元素的**非线性激活函数**，复合就不再坍缩。标准激活函数如下：

```text
   sigmoid(x) = 1 / (1 + exp(−x))        # squashes to (0, 1)
   ReLU(x)    = max(x, 0)                 # the modern default — cheap, no saturation
```

**多层感知机**堆叠若干*隐藏层*，每个隐藏层是一个全连接层后接一个激活函数，最后接一个输出层。即便只有一个隐藏层，**通用近似定理**也表明：给定足够多的隐藏单元，多层感知机可以以任意精度逼近任意连续函数 —— 这就是“只需加一个隐藏层”为何成立的正式表述。它的超参数是隐藏层的数量以及每层的宽度（输出个数）—— 这是对架构设计的初次接触。

```python
# MLP with one hidden layer — linear, nonlinear, linear
W1 = nn.Parameter(torch.randn(num_inputs, num_hiddens) * 0.01)
b1 = nn.Parameter(torch.zeros(num_hiddens))
W2 = nn.Parameter(torch.randn(num_hiddens, num_outputs) * 0.01)
b2 = nn.Parameter(torch.zeros(num_outputs))

H = relu(X @ W1 + b1)       # hidden layer: dense + nonlinearity
Y = H @ W2 + b2             # output layer
```

### 4.2 卷积神经网络（CNN）—— 用于图像

在图像上使用多层感知机毫无希望。对 300×300 的 ImageNet 图像，一个含 10K 个单元的隐藏层需要 **约 10 亿个参数**，因为“全连接”意味着每个输出都是对*每一个*输入像素的加权求和。大到无法训练，而且它无视了所有关于图像的先验知识。卷积神经网络则把两条先验知识直接固化进架构：

- **平移不变性。** 一个物体无论位于画面何处都还是同一个物体，因此同一个检测器应当适用于所有位置。
- **局部性。** 一个像素与它邻近的像素关系最密切，而不是与图像另一端的像素。

**卷积层**把两者都编码进去。每个输出由输入的一个小 `k × k` 窗口计算得到（**局部性**），而*同一个* `k × k` 权重矩阵 —— 即 **kernel** —— 在整个图像上滑动（**平移不变性**，通过**参数共享**实现）。最关键的特性是：卷积层的参数量只取决于 kernel 大小，**与输入或输出的分辨率无关** —— 同一个 `3×3` kernel 可用于任意图像尺寸。卷积神经网络正是以极小一部分参数，获得了与那个 10 亿参数多层感知机相同的建模能力。学到的 kernel 会成为一个模式检测器（边缘、纹理、曲线）。

```python
# Single-channel 2D convolution: slide kernel K over input X
h, w = K.shape
Y = torch.zeros((X.shape[0] - h + 1, X.shape[1] - w + 1))
for i in range(Y.shape[0]):
    for j in range(Y.shape[1]):
        Y[i, j] = (X[i:i+h, j:j+w] * K).sum()    # same K reused everywhere
```

**池化层**随后在小窗口上取最大值或均值，缩小特征图并增加对小位移的容忍度。真正的**卷积神经网络**反复堆叠 卷积 → 激活 → 池化，以提取层次化的特征（边缘 → 纹理 → 部件 → 物体），最后接全连接层完成预测。现代卷积神经网络 —— **AlexNet、VGG、Inception、ResNet、MobileNet** —— 就是这种模式的深层堆叠，并带有各种连接上的技巧（ResNet 的跳跃连接会在第 08 讲重新讨论）。

### 4.3 循环神经网络（RNN）—— 用于序列

语言是序列：根据前面的词预测下一个词（`hello` → `world`；`hello world` → `!`）。普通的多层感知机处理序列效果很差，因为它对先前出现的内容没有记忆。**循环神经网络**引入了一个**隐状态**，它在每个时间步被向前传递并更新，把信息贯穿整个序列：

```text
   hₜ = ϕ(W_hh · hₜ₋₁ + W_hx · xₜ + b_h)
```

与多层感知机相比，*唯一*的结构差异就是 `W_hh · hₜ₋₁` 这一项 —— 上一步的隐状态会送入当前步，从而赋予网络记忆。**门控循环神经网络（LSTM、GRU）**增加了可学习的门，以更精细地控制信息流动 —— 选择性地*遗忘输入*或*遗忘过去* —— 这正是它们能在长序列中携带信号而不致消失的原因。循环神经网络还有**双向**版本（对序列双向读取，用于未来上下文可获得的场景）和**深层**版本（堆叠循环层）。

```python
# Simple RNN: carry hidden state H across time steps
H = torch.zeros(num_hiddens)
for X in inputs:                                  # inputs: (num_steps, batch, num_inputs)
    H = torch.tanh(X @ W_xh + H @ W_hh + b_h)     # update memory each step
    outputs.append(H)
```


<details>
<summary>English original</summary>

**4.1 The multilayer perceptron (MLP)**

Insert an elementwise **nonlinear activation** between dense layers and the composition stops collapsing. The standard activations:

```text
   sigmoid(x) = 1 / (1 + exp(−x))        # squashes to (0, 1)
   ReLU(x)    = max(x, 0)                 # the modern default — cheap, no saturation
```

An **MLP** stacks *hidden layers*, each a dense layer followed by an activation, then a final output layer. With even one hidden layer, the **universal approximation theorem** says an MLP can approximate any continuous function to arbitrary accuracy given enough hidden units — the formal statement of why "just add a hidden layer" works. Its hyperparameters are the number of hidden layers and the width (number of outputs) of each — your first taste of architecture design.

```python
# MLP with one hidden layer — linear, nonlinear, linear
W1 = nn.Parameter(torch.randn(num_inputs, num_hiddens) * 0.01)
b1 = nn.Parameter(torch.zeros(num_hiddens))
W2 = nn.Parameter(torch.randn(num_hiddens, num_outputs) * 0.01)
b2 = nn.Parameter(torch.zeros(num_outputs))

H = relu(X @ W1 + b1)       # hidden layer: dense + nonlinearity
Y = H @ W2 + b2             # output layer
```

**4.2 Convolutional neural networks (CNN) — for images**

An MLP on images is hopeless. A single hidden layer of 10K units on 300×300 ImageNet images needs **~1 billion parameters**, because "fully connected" means every output is a weighted sum over *every* input pixel. Too big to train, and it ignores everything we know about images. CNNs bake two pieces of prior knowledge into the architecture instead:

- **Translation invariance.** An object is the same object wherever it sits in the frame, so the same detector should apply everywhere.
- **Locality.** A pixel relates most to its near neighbors, not to one across the image.

A **convolution layer** encodes both. Each output is computed from a small `k × k` window of the input (**locality**), and *the same* `k × k` weight matrix — the **kernel** — slides across the whole image (**translation invariance**, via **parameter sharing**). The killer property: a conv layer's parameter count is just the kernel size and **does not depend on the input or output resolution** — the same `3×3` kernel works on any image size. That is how CNNs get the same modeling power as that 1-billion-parameter MLP with a tiny fraction of the parameters. A learned kernel becomes a pattern detector (an edge, a texture, a curve).

```python
# Single-channel 2D convolution: slide kernel K over input X
h, w = K.shape
Y = torch.zeros((X.shape[0] - h + 1, X.shape[1] - w + 1))
for i in range(Y.shape[0]):
    for j in range(Y.shape[1]):
        Y[i, j] = (X[i:i+h, j:j+w] * K).sum()    # same K reused everywhere
```

A **pooling layer** then takes the max or mean over small windows, shrinking the feature map and adding tolerance to small shifts. A real **CNN** stacks convolution → activation → pooling repeatedly to extract a hierarchy of features (edges → textures → parts → objects), ending in dense layers for the prediction. Modern CNNs — **AlexNet, VGG, Inception, ResNet, MobileNet** — are deep stacks of this pattern with various connectivity tricks (ResNet's skip connections are revisited in Lecture 08).

**4.3 Recurrent neural networks (RNN) — for sequences**

Language is a sequence: predict the next word from those before it (`hello` → `world`; `hello world` → `!`). A plain MLP handles sequences badly because it has no memory of what came earlier. An **RNN** adds a **hidden state** that is carried forward and updated at every time step, threading information through the sequence:

```text
   hₜ = ϕ(W_hh · hₜ₋₁ + W_hx · xₜ + b_h)
```

The *only* structural difference from an MLP is that term `W_hh · hₜ₋₁` — the hidden state from the previous step feeds into the current one, giving the network memory. **Gated RNNs (LSTM, GRU)** add learned gates for finer control of information flow — selectively *forgetting the input* or *forgetting the past* — which is what lets them carry signal across long sequences without it vanishing. RNNs also come **bidirectional** (read the sequence both ways, for tasks where future context is available) and **deep** (stack recurrent layers).

```python
# Simple RNN: carry hidden state H across time steps
H = torch.zeros(num_hiddens)
for X in inputs:                                  # inputs: (num_steps, batch, num_inputs)
    H = torch.tanh(X @ W_xh + H @ W_hh + b_h)     # update memory each step
    outputs.append(H)
```

</details>

### 4.4 Attention 与 Transformer —— 前瞻指引

RNN 的序列隐藏状态也是它的弱点：必须逐步计算（与决策树一样，无法跨时间并行），而且长序列中来自很远位置的信息会因为被反复覆盖而退化。**attention 机制**同时解决了这两个问题：它让每个位置*直接*看到其他所有位置——不通过隐藏状态中继——并且并行完成。完全基于 attention 构建的 **Transformer** 架构，如今本质上是所有 NLP、大部分视觉、音频以及整个路线图所围绕的大语言模型的骨干。CS329P（以及本课程）在 **[Lecture 08](Lecture-08.md)** 中系统展开 attention/Transformer 工具集；目前，只需将其记作第四种架构，也是取代 RNN 处理序列的架构。

### 4.5 哪种神经网络用于哪种数据

| 架构 | 编码内容 | 适用于 |
|---|---|---|
| **多层感知机** | 对定长向量的通用函数逼近 | 表格向量、更大网络的输出头 |
| **卷积神经网络** | 通过参数共享实现局部性 + 平移不变性 | 图像、音频频谱图、视频 |
| **RNN / LSTM / GRU** | 通过隐藏状态实现序列记忆 | 模型需小型/流式时的序列 |
| **Transformer** | 全对 attention，完全并行 | 文本、长序列、多模态——现代默认 ([L08](Lecture-08.md)) |

---


<details>
<summary>English original</summary>

**4.4 Attention and the Transformer — a pointer forward**

The RNN's sequential hidden state is also its weakness: it must be computed step by step (no parallelism across time, like the decision tree), and information from far back in a long sequence degrades as it is repeatedly overwritten. The **attention mechanism** solves both by letting every position look *directly* at every other position — no relaying through a hidden state — and doing it in parallel. The **Transformer** architecture, built entirely on attention, is now the backbone of essentially all of NLP, much of vision, audio, and the large language models this whole roadmap orbits. CS329P (and this course) develops the attention/Transformer toolkit properly in **[Lecture 08](Lecture-08.md)**; for now, register it as the fourth architecture and the one that displaced the RNN for sequences.

**4.5 Which neural net for which data**

| Architecture | Encodes | Use for |
|---|---|---|
| **MLP** | Universal function approximation over a fixed-length vector | Tabular vectors, the output head of bigger nets |
| **CNN** | Locality + translation invariance via parameter sharing | Images, audio spectrograms, video |
| **RNN / LSTM / GRU** | Sequential memory via a hidden state | Sequences when models must be small/streaming |
| **Transformer** | All-pairs attention, fully parallel | Text, long sequences, multimodal — the modern default ([L08](Lecture-08.md)) |

---

</details>

## 5. 何时用哪种模型

整讲压缩成一个决策：让模型族匹配你数据的*结构*。

| 数据类型 | 首选 | 原因 | 备选 |
|---|---|---|---|
| **表格数据**（行 × 列，混合类型） | **梯度提升树**（XGBoost / LightGBM） | 原生支持混合类型，无需预处理，在结构化数据上胜过深度网络 | 随机森林；线性/logistic 作为快速可解释基线 |
| **图像 / 视频** | **CNN**（ResNet、MobileNet）或视觉 Transformer | 局部性 + 参数共享契合图像结构 | 预训练 backbone 作为特征提取器（[L09](Lecture-09.md)） |
| **文本 / 语言** | **Transformer**（预训练 LLM） | 全对 attention 捕捉长程依赖 | 在 TF-IDF 上做线性/logistic，作为廉价而强力的基线 |
| **时间序列 / 序列** | **Transformer** 或 **RNN/LSTM** | 需要序列或长程结构 | 在滞后/加窗特征上用 GBT 出奇地强 |
| **音频** | 频谱图上的 **CNN** 或音频 **Transformer** | 频谱图就是图像；长上下文用 attention | — |
| **小数据 / 需要可解释性** | **线性 / logistic 回归**或单棵树 | 参数少，不易过拟合；模型*本身*就是解释 | 正则化线性；浅树 |

表格之上还有两条经验法则。第一，**在复杂模型之前先拟合一个简单基线**——正则化线性模型或随机森林速度快、不易出错，并能告诉你问题是否可解、花哨的模型是否配得上其复杂度（Lecture 04 让这一点可度量）。第二，调参之前**先让数据结构挑选模型族**：结构 → 模型族 → 模型 → 超参数，顺序如此。

> **2026 更新：** 幻灯片时代的模型选择图经受住了时间考验，只有两处大变化。**（1）Transformer 吞掉了 CV、NLP 和音频。** 对大多数新工作而言，RNN 和 LSTM 已成遗留——视觉 Transformer 可与 CNN 匹敌，attention 是任何序列或文本任务的默认选择。图中 “文本/语音用 RNN” 那一格今天应写作 “Transformer”。**（2）基础模型工作流颠覆了 “挑一个模型再训练”。** 对于非结构化数据，已很少从零训练——取一个预训练基础模型（视觉 backbone、LLM），再微调或提示它（Lecture 09），把模型选择变成*模型选择与复用*。唯一**没有**变的那格：**梯度提升树仍是表格数据的王者。** 尽管多年来 “面向表格的深度学习” 论文层出不穷（TabNet、FT-Transformer 等），XGBoost 和 LightGBM 仍是结构化数据上的首选、通常也是最佳选择——这是整讲中最经久耐用的实用结论。

> **硬件视角：** 这两类模型族对硬件的需求截然相反，这一分野会贯穿路线图其余部分。**树的推理分支多、控制流重**——评估一棵树意味着一连串数据相关的 `if` 比较，沿树走出不规则路径，内存局部性差，也没有大矩阵可乘。这是 **CPU** 工作负载：分支预测器和大容量缓存正是它需要的，而 GPU 的数千条 lane 在其上大部分时间闲置。**神经网络推理则相反——稠密、规则的 GEMM**（矩阵-矩阵乘）：每一层都是无分支的大矩阵乘法，正是 **GPU、TPU 或 NPU** 大规模并行 ALU 的经典任务，也是这些加速器存在的原因。所以同样的准确率可以落在完全不同的硅片上：做欺诈判定推理服务的梯度提升树在 CPU 上以微秒级延迟轻松运行，而 “体型” 相近的 Transformer 则需要 GPU 和批处理系统才经济。这种分支 vs GEMM 的分野，是整个推理服务与压缩栈的基础（见 **[阶段 5 — ML Systems Engineering](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/Guide.md)**），也是 Lecture 10 的压缩技术（量化、剪枝、蒸馏）专门针对神经网络的原因——因为有稠密 GEMM 可以缩小。当你选定一个模型族时，也就选定了一个硬件目标。

---


<details>
<summary>English original</summary>

**5. Which model when**

The whole lecture compresses to one decision: match the model family to the *structure* of your data.

| Data type | First choice | Why | Fallback |
|---|---|---|---|
| **Tabular** (rows × columns, mixed types) | **Gradient-boosted trees** (XGBoost / LightGBM) | Native mixed types, no preprocessing, beats deep nets on structured data | Random Forest; linear/logistic for a fast interpretable baseline |
| **Images / video** | **CNN** (ResNet, MobileNet) or a vision Transformer | Locality + parameter sharing match image structure | Pretrained backbone as a feature extractor ([L09](Lecture-09.md)) |
| **Text / language** | **Transformer** (pretrained LLM) | All-pairs attention captures long-range dependence | Linear/logistic on TF-IDF for a cheap, strong baseline |
| **Time series / sequences** | **Transformer** or **RNN/LSTM** | Need sequential or long-range structure | GBT on lagged/windowed features is shockingly strong |
| **Audio** | **CNN** on spectrograms or audio **Transformer** | Spectrograms are images; attention for long context | — |
| **Small data / need interpretability** | **Linear / logistic regression** or a single tree | Few parameters resist overfitting; the model *is* its own explanation | Regularized linear; shallow tree |

Two rules of thumb sit on top of the table. First, **always fit a simple baseline before a complex model** — a regularized linear model or a Random Forest is fast, hard to get wrong, and tells you whether your problem is even tractable and whether the fancy model is earning its complexity (Lecture 04 makes this measurable). Second, **let the data's structure pick the family** before you tune anything: structure → family → model → hyperparameters, in that order.

> **2026 update:** The slide-era model selection chart aged remarkably well, with two big shifts. **(1) Transformers ate CV, NLP, and audio.** RNNs and LSTMs are now legacy for most new work — vision Transformers rival CNNs, and attention is the default for any sequence or text task. The chart's "RNNs for text/speech" box should read "Transformers" today. **(2) The foundation-model workflow inverted "pick and train a model."** For unstructured data you rarely train from scratch anymore — you take a pretrained foundation model (a vision backbone, an LLM) and fine-tune or prompt it (Lecture 09), turning model selection into *model selection-and-reuse*. The one box that did **not** change: **gradient-boosted trees are still the champion for tabular data.** Despite years of "deep learning for tabular" papers (TabNet, FT-Transformer, and others), XGBoost and LightGBM remain the first and usually best choice on structured data — the most durable practical result in this entire lecture.

> **Hardware lens:** the two model families demand opposite hardware, and this split echoes through the rest of the roadmap. **Tree inference is branchy and control-flow heavy** — evaluating a tree means a chain of data-dependent `if` comparisons that walk an irregular path down the tree, with poor memory locality and no big matrix to multiply. That is a **CPU** workload: branch predictors and large caches are exactly what it needs, and a GPU's thousands of lanes sit mostly idle on it. **Neural-net inference is the opposite — dense, regular GEMM** (general matrix multiply): every layer is a big matrix multiplication with no branching, the canonical job for the massively parallel ALUs of a **GPU, TPU, or NPU**, and the reason those accelerators exist. So the same accuracy can land on wildly different silicon: a gradient-boosted tree serving fraud decisions runs happily on a CPU at microsecond latency, while a Transformer of similar "size" wants a GPU and a batching system to be economical. This branchy-vs-GEMM divide is the foundation of the entire serving and compression stack in **[Phase 5 — ML Systems Engineering](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/Guide.md)**, and the reason Lecture 10's compression techniques (quantization, pruning, distillation) target neural nets specifically — there is a dense GEMM to shrink. When you pick a model family, you are also picking a hardware target.

---

</details>

## 截至

撰写于 2026 年 6 月。先讲 **CS329P 原有内容**——学习类型分类法、`model + loss + optimization` 分解、树模型演进（决策树 → Random Forest → boosting）、线性到 softmax 再到 MLP 的层层搭建，以及 MLP/CNN/RNN 架构及其数据结构层面的动机——因为它仍是正确的工作心智模型，且与 2021 年幻灯片一一对应。**更新层**标出此后发生的位移：梯度提升树在表格数据上持续占据统治地位，RNN 在 CV/NLP/音频领域被 **Transformer** 取代（正式讲解推迟到第 08 讲），**基础模型**工作流把“训练一个模型”变为“微调或提示一个预训练模型”（第 09 讲），以及 **branchy-CPU vs. dense-GEMM（矩阵-矩阵乘）加速器**的硬件分野——原幻灯片未涉及，却正是阶段 5 的驱动力。凡 2021 年的表述已过时之处，均先呈现原文再给出更新；不做任何悄无声息的改写。

*改编自 [Stanford CS329P](https://c.d2l.ai/stanford-cs329p) — Huang, Li & Smola, CC-BY-SA-4.0.*


<details>
<summary>English original</summary>

**Current as of**

Written June 2026. The **original CS329P content** — the learning-type taxonomy, the `model + loss + optimization` decomposition, the tree progression (decision tree → Random Forest → boosting), the linear-to-softmax-to-MLP build-up, and the MLP/CNN/RNN architectures with their data-structure motivations — is taught first because it remains the correct working mental model and maps one-to-one onto the 2021 slides. The **refresh layer** flags what moved since: gradient-boosted trees' continued reign over tabular data, the displacement of RNNs by **Transformers** across CV/NLP/audio (with the proper treatment deferred to Lecture 08), the **foundation-model** workflow that turned "train a model" into "fine-tune or prompt a pretrained one" (Lecture 09), and the **branchy-CPU vs. dense-GEMM-accelerator** hardware split that the original slides did not address but that drives Phase 5. Where 2021 framing is dated, the original is presented before the update; nothing is silently rewritten.

*Adapted from [Stanford CS329P](https://c.d2l.ai/stanford-cs329p) — Huang, Li & Smola, CC-BY-SA-4.0.*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/4. ML Engineering and MLOps/Practical Machine Learning (CS329P)/Lecture-03.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/4.%20ML%20Engineering%20and%20MLOps/Practical%20Machine%20Learning%20%28CS329P%29/Lecture-03.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
