---
title: Lecture 05 - 模型组合：Bagging、Boosting、Stacking
description: Lecture 05 - 模型组合：Bagging、Boosting、Stacking
---

# Lecture 05 - 模型组合：Bagging、Boosting、Stacking

**合集：** [Practical Machine Learning (CS329P)](README.md) | **上一篇：** [← Lecture 04](Lecture-04.md) | **下一篇：** [Lecture 06](Lecture-06.md)

---

Lecture 04 教你 *信任分数* — 诚实地验证单个模型。本讲讨论的是：当你已经拥有若干个还不错但不完美的模型，想要一个比它们都更好的模型时，该怎么做。单个模型很少是排行榜上最强的参赛者，也很少是生产环境中最可靠的那个；标准做法是**组合**模型。集成归根结底是一种**用算力换准确率**的方式 — 你训练并提供推理服务的模型数量超过实际所需，而组合结果胜过最好的单个学习器。

集成之所以有效，以及之所以有 *三种不同* 的做法，原因直接来自**偏差–方差分解**。泛化误差拆分为三个可加的分量 — 偏差²、方差与不可约噪声 — 而每种组合器攻击的是不同的分量。**Bagging** 通过对独立模型取平均来压低*方差*。**Boosting** 通过依次堆叠弱模型来压低*偏差*，每个弱模型都修正上一个的错误。**Stacking** 在多样的基学习器之上学习一个元模型，可以同时削减两者。所以这个分解不是抽象理论；它就是决策流程。诊断你的模型是受偏差限制（欠拟合）还是受方差限制（过拟合），数学就会告诉你该用哪种组合器。

代价是真实存在的，值得一开始就说明：由 $N$ 个模型组成的集成，推理延迟约为单个模型的 $N\times$，内存约为 $N\times$。这种张力 — 组合带来的准确率与推理服务多个模型的成本之间的对立 — 正是使 Lecture 10 的压缩与蒸馏成为必要的压力。组合以赢得离线指标；压缩以负担得起在线指标。

---

## 学习目标

学完本讲，你应当能够：

1. **推导偏差–方差分解** $\mathbb{E}[(y - \hat f)^2] = \text{bias}^2 + \text{variance} + \text{noise}$，并说明是什么抬高了每一项，以及模型复杂度如何在它们之间做权衡。
2. **把每种集成方法对应到它所削减的项** — bagging→方差，boosting→偏差，stacking→两者 — 并依据偏差/方差诊断选出正确的那一种。
3. **实现 bagging**：bootstrap 采样、独立并行训练、平均/投票、out-of-bag 估计，以及作为典范案例的 Random Forest。
4. **实现 boosting**：顺序修正残差的循环、AdaBoost 重加权与 gradient boosting 残差拟合的对比，以及为什么没有正则化时它会过拟合。
5. **构建 stacking 集成**：使用多样的基学习器，以及在 *out-of-fold* 预测上训练的元学习器 — 并解释会击垮朴素 stacking 的泄漏陷阱。
6. **分析集成的推理成本**，并将其与模型压缩关联起来。

---

## 1. 偏差–方差分解 — 决策流程

本讲的一切都系于一个等式，所以先推导它。假设数据按 $y = f(x) + \varepsilon$ 生成，其中 $f$ 是真实的（未知）函数，$\varepsilon$ 是不可约噪声，满足 $\mathbb{E}[\varepsilon] = 0$、方差为 $\sigma^2$。采样一个数据集 $D = \{(x_1, y_1), \dots, (x_n, y_n)\}$，通过在 $D$ 上最小化 MSE 学得估计器 $\hat f_D$。希望 $\hat f_D$ 能够**泛化** — 在它从未训练过的新点 $(x, y)$ 上表现良好。

关心的量是在该新点上的期望误差，*且对所有可能抽到的训练集 $D$ 取平均*。在平方内加上并减去平均预测 $\mathbb{E}_D[\hat f_D]$：

```text
E_D[(y - f̂_D(x))²]
  = E_D[ ( (f - E_D[f̂_D]) - (f̂_D - E_D[f̂_D]) + ε )² ]
  = (f - E_D[f̂_D])²            ← Bias²    : how far the average model is from truth
  + E_D[(f̂_D - E_D[f̂_D])²]      ← Variance : how much the model wobbles across datasets
  + σ²                          ← Noise    : irreducible, no model can beat it

  = Bias[f̂_D]² + Var[f̂_D] + σ²
```


交叉项在期望下消失，因为 $\varepsilon$ 均值为零且与数据独立，也因为按定义有 $\mathbb{E}_D[\hat f_D - \mathbb{E}_D[\hat f_D]] = 0$。剩下的是三个**可加、非负**的项：

| 项 | 它衡量什么 | 什么会抬高它 | 如何降低它 |
|---|---|---|---|
| **偏差²** | *平均*模型的误差与真值之差 | 模型太简单，无法表达 $f$（欠拟合） | 使用更复杂的模型 — 更多层 / 隐藏单元；**boosting**；stacking |
| **方差** | 数据集变化时模型变化的程度 | 模型太灵活，记住了样本（过拟合） | 使用更简单的模型；正则化；**bagging**；stacking |
| **噪声 $\sigma^2$** | 给定 $x$ 时 $y$ 中的不可约随机性 | 特征差/不足；测量误差 | 无法降低 — 只有更好的 *数据* 才能缩小它 |


<details>
<summary>English original</summary>

**Lecture 05 - Model Combination: Bagging, Boosting, Stacking**

**Collection:** [Practical Machine Learning (CS329P)](README.md) | **Previous:** [← Lecture 04](Lecture-04.md) | **Next:** [Lecture 06](Lecture-06.md)

---

Lecture 04 taught you to *trust the score* — to validate a single model honestly. This lecture is about what you do once you have several decent-but-imperfect models and want one that is better than any of them. A single model is rarely the strongest entry on a leaderboard or the most reliable thing in production; the standard move is to **combine** models. Ensembling is, at bottom, a way to **spend compute to buy accuracy** — you train and serve more models than you strictly need, and the combination beats the best individual learner.

The reason ensembling works, and the reason there are *three different* ways to do it, comes straight from the **bias–variance decomposition**. Generalization error splits into three additive pieces — bias², variance, and irreducible noise — and each combiner attacks a different piece. **Bagging** drives down *variance* by averaging independent models. **Boosting** drives down *bias* by stacking weak models that each fix the last one's mistakes. **Stacking** learns a meta-model over diverse base learners and can chip at both. So the decomposition is not abstract theory; it is the decision procedure. Diagnose whether your model is bias-limited (underfitting) or variance-limited (overfitting), and the math tells you which combiner to reach for.

The cost is real and worth stating up front: an ensemble of $N$ models costs roughly $N\times$ the inference latency and $N\times$ the memory of one model. That tension — accuracy from combination versus the cost of serving many models — is exactly the pressure that makes the compression and distillation of Lecture 10 necessary. Combine to win the offline metric; compress to afford the online one.

---

**Learning objectives**

By the end of this lecture you should be able to:

1. **Derive the bias–variance decomposition** $\mathbb{E}[(y - \hat f)^2] = \text{bias}^2 + \text{variance} + \text{noise}$, and say what raises each term and how model complexity trades them off.
2. **Map each ensemble method to the term it reduces** — bagging→variance, boosting→bias, stacking→both — and pick the right one from a bias/variance diagnosis.
3. **Implement bagging**: bootstrap sampling, independent parallel training, averaging/voting, the out-of-bag estimate, and Random Forest as the canonical case.
4. **Implement boosting**: the sequential residual-fixing loop, AdaBoost reweighting vs. gradient boosting's residual fitting, and why it overfits without regularization.
5. **Build a stacking ensemble** with diverse base learners and a meta-learner trained on *out-of-fold* predictions — and explain the leakage trap that breaks naive stacking.
6. **Reason about the inference cost** of an ensemble and connect it forward to model compression.

---

**1. Bias–variance decomposition — the decision procedure**

Everything in this lecture hangs off one equation, so we derive it. Assume the data is generated as $y = f(x) + \varepsilon$, where $f$ is the true (unknown) function and $\varepsilon$ is irreducible noise with $\mathbb{E}[\varepsilon] = 0$ and variance $\sigma^2$. We sample a dataset $D = \{(x_1, y_1), \dots, (x_n, y_n)\}$ and learn an estimator $\hat f_D$ by minimizing MSE on $D$. We want $\hat f_D$ to **generalize** — to do well on a fresh point $(x, y)$ it never trained on.

The quantity we care about is the expected error on that new point, *averaged over all the training sets $D$ we could have drawn*. Add and subtract the average prediction $\mathbb{E}_D[\hat f_D]$ inside the square:

```text
E_D[(y - f̂_D(x))²]
  = E_D[ ( (f - E_D[f̂_D]) - (f̂_D - E_D[f̂_D]) + ε )² ]
  = (f - E_D[f̂_D])²            ← Bias²    : how far the average model is from truth
  + E_D[(f̂_D - E_D[f̂_D])²]      ← Variance : how much the model wobbles across datasets
  + σ²                          ← Noise    : irreducible, no model can beat it

  = Bias[f̂_D]² + Var[f̂_D] + σ²
```

The cross-terms vanish in expectation because $\varepsilon$ has zero mean and is independent of the data, and because $\mathbb{E}_D[\hat f_D - \mathbb{E}_D[\hat f_D]] = 0$ by definition. What remains are three **additive, non-negative** terms:

| Term | What it measures | What raises it | How you lower it |
|---|---|---|---|
| **Bias²** | Error of the *average* model vs. the truth | Model too simple to express $f$ (underfitting) | Use a more complex model — more layers / hidden units; **boosting**; stacking |
| **Variance** | How much the model changes when the dataset changes | Model too flexible, memorizes the sample (overfitting) | Use a simpler model; regularization; **bagging**; stacking |
| **Noise $\sigma^2$** | Irreducible randomness in $y$ given $x$ | Bad/insufficient features; measurement error | You can't — only better *data* shrinks it |

</details>

### 取舍

随着模型复杂度提高，**偏差下降**（更丰富的模型能更贴近地拟合 $f$），但**方差上升**（更丰富的模型会抓住这个特定 $D$ 中的噪声）。总误差呈 U 形：太简单是**欠拟合**（高偏差、低方差），太复杂是**过拟合**（低偏差、高方差），而最佳点——最好的泛化——位于两者之间。

```text
 Error
   │ \                                   /   ← total = bias² + var + σ²
   │  \                                 /
   │   \        best generalization    /
   │    \              ↓              /
   │     \____      ___________     /        ← Variance (rises with complexity)
   │          \____/           \___/
   │   Bias²  ___                              ← Bias² (falls with complexity)
   │      \__/   \________________________
   └───────────────────────────────────────►  Model complexity
     underfitting          overfitting
```

### 为什么这决定了你的组合器

这是统领本讲其余内容的关键结论。**集成学习**指训练并组合多个模型以提升预测性能——而这个分解告诉了你*如何*组合：

- 如果模型**欠拟合**（高偏差，例如浅层树）→ 需要**降低偏差** → **boosting**（或 stacking）。
- 如果模型**过拟合**（高方差，例如未剪枝的深层树）→ 需要**降低方差** → **bagging**（或 stacking）。
- **Stacking** 两者都能应对，办法是学习如何对多样化的模型加权。

降低噪声只能靠**改进数据**——更多样本、更好的特征、更干净的标签。没有任何组合器能触及 $\sigma^2$。

---

## 2. Bagging —— Bootstrap AGGregatING

**Bagging 通过对许多独立模型取平均来降低方差。** 你*并行*训练 $n$ 个基学习器，每个都在数据的略微不同版本上训练，然后组合它们的输出。由于这些模型是独立训练的，它就是消除方差的组合器。

所谓“数据的另一个版本”就是一个 **bootstrap 样本**：给定含 $m$ 个样本的数据集，**有放回地**抽取 $m$ 个样本。有放回抽样意味着有些样本出现多次，另一些则完全不出现。一个经典结果：每个 bootstrap 样本大约包含 $1 - 1/e \approx 63\%$ 的不重复样本，剩下约 37% 未被触及——即 **out-of-bag (OOB)** 样本，你可以把它们当作免费的验证数据使用。

对回归任务，用**取平均**来组合学习器的输出；对分类任务，则用**多数投票**。

```python
from sklearn.base import clone
import numpy as np

class Bagging:
    def __init__(self, base_learner, n_learners):
        self.learners = [clone(base_learner) for _ in range(n_learners)]

    def fit(self, X, y):
        for learner in self.learners:
            # bootstrap: sample len(X) indices WITH replacement
            idx = np.random.choice(len(X), len(X), replace=True)
            learner.fit(X.iloc[idx], y.iloc[idx])

    def predict(self, X):
        preds = [learner.predict(X) for learner in self.learners]
        return np.array(preds).mean(axis=0)   # vote for classification
```

### 为什么取平均能缩小方差

对回归任务，bagging 后的预测是各基学习器上的平均，$\hat f(x) = \mathbb{E}_D[\hat f_D(x)]$。由 Jensen 不等式，$\mathbb{E}[X]^2 \le \mathbb{E}[X^2]$，因此：

```text
(f(x) - f̂(x))²  ≤  E_D[(f(x) - f̂_D(x))²]
└── error of the bagged model ──┘   └── average error of a single learner ──┘
```

集成模型的误差**不差于**、实践中还好于单个学习器的平均误差——收益来自抵消独立的波动（方差），同时不触及共享的偏差。因此 Bagging **不会降低偏差**：如果每个基学习器都以相同方式系统性地欠拟合，对它们取平均仍会保留该偏差。

### 不稳定学习器才是 Bagging 见效之处

Bagging 对**不稳定**（高方差）学习器帮助最大——这类模型的预测会在训练集稍有变化时大幅摆动。

- **决策树是不稳定的**——重新采样数据后，一棵深层树可能基于完全不同的特征进行划分，产生截然不同的函数。是 Bagging 的完美候选。
- **线性回归是稳定的**——其拟合在重采样下几乎不变，因此几乎没有方差可供平均消除，Bagging 几乎带不来任何收益。


<details>
<summary>English original</summary>

**The tradeoff**

As you increase model complexity, **bias falls** (a richer model can fit $f$ more closely) but **variance rises** (a richer model latches onto the noise in this particular $D$). Total error is U-shaped: too simple is **underfitting** (high bias, low variance), too complex is **overfitting** (low bias, high variance), and the sweet spot — best generalization — sits in between.

```text
 Error
   │ \                                   /   ← total = bias² + var + σ²
   │  \                                 /
   │   \        best generalization    /
   │    \              ↓              /
   │     \____      ___________     /        ← Variance (rises with complexity)
   │          \____/           \___/
   │   Bias²  ___                              ← Bias² (falls with complexity)
   │      \__/   \________________________
   └───────────────────────────────────────►  Model complexity
     underfitting          overfitting
```

**Why this picks your combiner**

This is the punchline that organizes the rest of the lecture. **Ensemble learning** means training and combining multiple models to improve predictive performance — and the decomposition tells you *how* to combine:

- If your model **underfits** (high bias, e.g. shallow trees) → you need to **reduce bias** → **boosting** (or stacking).
- If your model **overfits** (high variance, e.g. deep unpruned trees) → you need to **reduce variance** → **bagging** (or stacking).
- **Stacking** can attack either, by learning how to weight diverse models.

Reduce noise only by **improving the data** — more samples, better features, cleaner labels. No combiner touches $\sigma^2$.

---

**2. Bagging — Bootstrap AGGregatING**

**Bagging reduces variance by averaging many independent models.** You train $n$ base learners *in parallel*, each on a slightly different version of the data, then combine their outputs. Because the models are trained independently, this is the variance-killing combiner.

The "different version of the data" is a **bootstrap sample**: given a dataset of $m$ examples, draw $m$ examples **with replacement**. Sampling with replacement means some examples appear several times and others not at all. A classic result: each bootstrap sample contains about $1 - 1/e \approx 63\%$ of the unique examples, leaving ~37% untouched — the **out-of-bag (OOB)** examples, which you get to use as free validation data.

Combine the learners by **averaging** their outputs for regression, or **majority voting** for classification.

```python
from sklearn.base import clone
import numpy as np

class Bagging:
    def __init__(self, base_learner, n_learners):
        self.learners = [clone(base_learner) for _ in range(n_learners)]

    def fit(self, X, y):
        for learner in self.learners:
            # bootstrap: sample len(X) indices WITH replacement
            idx = np.random.choice(len(X), len(X), replace=True)
            learner.fit(X.iloc[idx], y.iloc[idx])

    def predict(self, X):
        preds = [learner.predict(X) for learner in self.learners]
        return np.array(preds).mean(axis=0)   # vote for classification
```

**Why averaging shrinks variance**

For regression, the bagged prediction is the average over base learners, $\hat f(x) = \mathbb{E}_D[\hat f_D(x)]$. By Jensen's inequality, $\mathbb{E}[X]^2 \le \mathbb{E}[X^2]$, so:

```text
(f(x) - f̂(x))²  ≤  E_D[(f(x) - f̂_D(x))²]
└── error of the bagged model ──┘   └── average error of a single learner ──┘
```

The ensemble's error is **no worse than**, and in practice better than, the average individual learner's error — the gain comes from cancelling the independent fluctuations (variance) while leaving the shared bias untouched. Bagging therefore **does not reduce bias**: if every base learner systematically underfits the same way, averaging them keeps that bias.

**Unstable learners are where bagging pays off**

Bagging helps most for **unstable** (high-variance) learners — models whose predictions swing a lot when the training set changes slightly.

- **Decision trees are unstable** — re-sample the data and a deep tree can split on entirely different features, producing a very different function. Perfect bagging candidate.
- **Linear regression is stable** — its fit barely moves under resampling, so there is little variance to average away, and bagging buys you almost nothing.

</details>

### 随机森林 —— 经典案例

**随机森林 = 用决策树做 bagging，再加一个额外技巧。** 除了对行做 bootstrap 重采样，随机森林在每个分裂点只考虑**一个随机的特征子集**。这让树之间去相关：没有这一步，少数几个主导特征会让每棵树都长得一样，而对近乎相同的模型取平均几乎降不了方差。通过迫使树使用不同的特征，你得到真正多样的学习器，平均的收益也更实在。其结果是现存最稳健的现成模型之一 —— 几乎不用调参、默认设置就很强，而且 OOB 样本免费给你一个验证估计，无需单独划分。

> Bagging 是**尴尬并行**的：$n$ 个学习器在训练期间互不共享任何东西，因此可以同时在 $n$ 台机器/核心上训练它们。这是它的工程标志，也是与 boosting 最鲜明的对比。

---

## 3. Boosting —— 顺序降低偏差

**Boosting 通过顺序训练弱学习器来降低偏差，每个弱学习器都在修补上一轮集成的错误。** bagging 并行训练独立模型再取平均，而 boosting *一个接一个*地训练模型，每个新学习器都聚焦于当前集成模型答错的样本。所谓「弱学习器」就是仅比随机猜测略好的模型 —— 一棵浅树（*stump* 是深度为 1 的树）。以正确的方式叠加，弱学习器就能复合成一个强学习器。

通用循环，在第 $t$ 步：

1. 在训练数据上评估当前集成模型的误差 $\varepsilon_t$。
2. 训练一个新的弱学习器 $\hat f_t$，让它**聚焦于预测错误的样本**。
3. 把 $\hat f_t$ **加性组合**进集成模型。

两个著名变体的差别只在于*如何*让学习器聚焦于误差：

- **AdaBoost** —— *按误差重新加权/重采样。* 被错分的样本获得更高权重（或被更频繁地重采样），使下一个学习器更关注它们；每个学习器在最终投票中还会获得一个置信度权重。
- **Gradient Boosting** —— *拟合残差。* 训练下一个学习器直接预测当前的误差，然后把它加上去。

### 精确地看梯度提升

梯度提升支持任意**可微损失**。设 $H_t(x)$ 为第 $t$ 步时组合模型的输出，从 $H_1(x) = 0$ 出发。每一步：

- 在**残差** $\{(x_i,\; y_i - H_t(x_i))\}_{i=1}^{m}$ 上训练一个新的学习器 $\hat f_t$。
- 用 **shrinkage**（学习率）参数 $\eta$ 做正则化地组合：$H_{t+1}(x) = H_t(x) + \eta\, \hat f_t(x)$。

这个名字来自梯度视角。对平方误差损失 $L = \tfrac12 (H(x) - y)^2$，残差*就是*损失对模型输出的负梯度：

```text
L = ½ (H(x) − y)²        ⇒        y − H(x) = − ∂L/∂H
```

所以拟合残差 = **在函数空间**中做一步梯度下降。对一般损失 $L$，新学习器逼近负梯度 $-\partial L / \partial H_t$ —— 这就是*梯度*提升得名的由来。$\eta$ 是步长：小的 $\eta$ 意味着每棵树只贡献一点点，需要更多树，而泛化更好。

```python
from sklearn.base import clone
import numpy as np

class GradientBoosting:
    def __init__(self, base_learner, n_learners, learning_rate):
        self.learners = [clone(base_learner) for _ in range(n_learners)]
        self.lr = learning_rate

    def fit(self, X, y):
        residual = y.copy()
        for learner in self.learners:
            learner.fit(X, residual)
            residual = residual - self.lr * learner.predict(X)   # chase what's left

    def predict(self, X):
        preds = [learner.predict(X) for learner in self.learners]
        return np.array(preds).sum(axis=0) * self.lr
```

### 为什么 boosting 不做正则化就会过拟合

Boosting 把**偏差**压下去 —— 从构造上它不断拟合残留的任何误差，所以只要轮数够多，它能把训练集拟合到任意好，*包括其中的噪声*。这正是过拟合的失效模式：一直 boosting，训练误差趋近于零，而测试误差掉头回升。与 bagging 不同（在 bagging 里增加树基本是免费的，且单调安全），**增加 boosting 轮数是一个可能有害的旋钮。** 标准的正则化手段：

- **Shrinkage** —— 小的学习率 $\eta$（例如 0.01–0.1）；每个学习器只把集成模型挪动一点点。
- **子采样** —— 在随机的行子集（stochastic gradient boosting）和/或列子集上训练每个学习器。
- **早停** —— 当验证误差不再改善时停止添加树。
- **弱基学习器** —— 浅树（小的 `max_depth`），这样任何单个学习器都不会过拟合。


<details>
<summary>English original</summary>

**Random Forest — the canonical case**

**Random Forest = bagging with decision trees, plus one extra trick.** Beyond bootstrapping the rows, at each split a random forest considers only a **random subset of features**. This decorrelates the trees: without it, a few dominant features would make every tree look alike, and averaging near-identical models barely reduces variance. By forcing trees to use different features, you get genuinely diverse learners and the averaging bites harder. The result is one of the most robust off-the-shelf models in existence — little tuning, strong defaults, and the OOB samples give you a validation estimate for free, no separate split required.

> Bagging is **embarrassingly parallel**: the $n$ learners share nothing during training, so you can train them on $n$ machines/cores at once. This is its operational signature and the cleanest contrast with boosting.

---

**3. Boosting — sequential bias reduction**

**Boosting reduces bias by training weak learners sequentially, each fixing the previous ensemble's mistakes.** Where bagging trains independent models in parallel and averages, boosting trains models *one after another*, and each new learner concentrates on the examples the current ensemble gets wrong. A "weak learner" is a model only slightly better than chance — a shallow tree (a *stump* is depth-1). Stacked the right way, weak learners compound into a strong one.

The general loop, at step $t$:

1. Evaluate the current ensemble's errors $\varepsilon_t$ on the training data.
2. Train a new weak learner $\hat f_t$ that **focuses on the wrongly-predicted examples**.
3. **Additively combine** $\hat f_t$ into the ensemble.

The two famous variants differ only in *how* a learner is made to focus on the errors:

- **AdaBoost** — *reweight/resample by error.* Misclassified examples get higher weight (or are resampled more often) so the next learner pays them more attention; each learner also gets a confidence weight in the final vote.
- **Gradient Boosting** — *fit the residual.* Train the next learner to predict the current errors directly, then add it on.

**Gradient boosting, precisely**

Gradient boosting supports any **differentiable loss**. Let $H_t(x)$ be the combined model's output at step $t$, starting from $H_1(x) = 0$. At each step:

- Train a new learner $\hat f_t$ on the **residuals** $\{(x_i,\; y_i - H_t(x_i))\}_{i=1}^{m}$.
- Combine with a **shrinkage** (learning-rate) parameter $\eta$ for regularization: $H_{t+1}(x) = H_t(x) + \eta\, \hat f_t(x)$.

The name comes from the gradient view. For squared-error loss $L = \tfrac12 (H(x) - y)^2$, the residual *is* the negative gradient of the loss w.r.t. the model's output:

```text
L = ½ (H(x) − y)²        ⇒        y − H(x) = − ∂L/∂H
```

So fitting the residual = taking a gradient-descent step **in function space**. For a general loss $L$, the new learner approximates the negative gradient $-\partial L / \partial H_t$ — hence *gradient* boosting. $\eta$ is the step size: small $\eta$ means each tree contributes a little, you need more trees, and you generalize better.

```python
from sklearn.base import clone
import numpy as np

class GradientBoosting:
    def __init__(self, base_learner, n_learners, learning_rate):
        self.learners = [clone(base_learner) for _ in range(n_learners)]
        self.lr = learning_rate

    def fit(self, X, y):
        residual = y.copy()
        for learner in self.learners:
            learner.fit(X, residual)
            residual = residual - self.lr * learner.predict(X)   # chase what's left

    def predict(self, X):
        preds = [learner.predict(X) for learner in self.learners]
        return np.array(preds).sum(axis=0) * self.lr
```

**Why boosting overfits without regularization**

Boosting drives **bias** down — by construction it keeps fitting whatever error remains, so given enough rounds it can fit the training set arbitrarily well, *including its noise*. That is exactly the overfitting failure mode: keep boosting and training error goes to zero while test error turns back up. Unlike bagging (where adding more trees is essentially free and monotonically safe), **more boosting rounds is a knob that can hurt.** The standard regularizers:

- **Shrinkage** — small learning rate $\eta$ (e.g. 0.01–0.1); each learner moves the ensemble only a little.
- **Subsampling** — train each learner on a random subset of rows (stochastic gradient boosting) and/or columns.
- **Early stopping** — stop adding trees when validation error stops improving.
- **Weak base learners** — shallow trees (small `max_depth`) so no single learner overfits.

</details>

### GBDT 实践 —— XGBoost 与 LightGBM

主流形式是 **梯度提升决策树（GBDT）**：弱学习器是决策树，由较小的 `max_depth` 和随机特征采样做正则化。问题在于，boosting **本质上是串行的** —— 第 $t$ 棵树需要第 $t-1$ 棵树的残差，因此无法像 bagging 那样并行训练各棵树。朴素的 GBDT 很慢。

工业级库用加速算法解决了这一点，它们在每棵树的构建过程*内部*实现并行：

- **XGBoost** —— 基于直方图的分裂查找、正则化目标、稀疏感知分裂，默认的主力。
- **LightGBM** —— leaf-wise 树生长与直方图分桶，面向大数据集；通常最快，内存效率很高。
- **CatBoost** —— 有序 boosting 与原生存量类别特征处理，类别特征多时表现强。

正因如此，**表格化**数据的默认选择仍是 GBDT，而不是深度网络。

---

## 4. Stacking —— 在多样模型之上的元学习器

**Stacking 先训练多样的基学习器，再在其预测结果之上训练一个元学习器。** bagging 的多样性来自*同一模型类型的 bootstrap 采样*，而 stacking 的多样性来自**完全不同的模型类型** —— Random Forest、GBDT 和 MLP 看的是同样的输入，却提取出不同种类的结构。元学习器（通常是一个简单线性模型）学习如何**加权并组合**它们的输出。

```text
                 ┌─────────────┐
                 │   Dense     │   ← meta-learner: learns the combination weights
                 └──────┬──────┘
                 ┌──────┴──────┐
                 │   Concat    │   ← stack base-learner predictions
                 └──────┬──────┘
        ┌───────────────┼───────────────┐
   ┌────┴────┐     ┌────┴────┐      ┌────┴────┐
   │ Random  │     │  GBDT   │  …   │   MLP   │   ← diverse base learners
   │ Forest  │     │         │      │         │
   └────┬────┘     └────┬────┘      └────┬────┘
        └───────────────┼───────────────┘
                 ┌──────┴──────┐
                 │   Inputs    │
                 └─────────────┘
```

这是**在竞赛中取胜**的方法。在 CS329P 的 house-sales benchmark 上，stacking 多个多样的学习器胜过任何单个学习器：

| Model | Test error |
|---|---|
| GBDT | 0.259 |
| RandomForest | 0.243 |
| **Stacking (AutoGluon)** | **0.229** |

```python
from autogluon.tabular import TabularPredictor

predictor = TabularPredictor(label=label).fit(train)   # stacks a zoo of models for you
```

### 泄漏陷阱 —— 必须使用留出预测

下面这个错误会悄无声息地毁掉朴素 stacking。如果在完整训练集上训练基学习器，然后把它们在*同一个训练集上*的预测喂给元学习器，那么基模型已经**见过**这些标签 —— 它们的训练预测好得不真实，于是元学习器学会过度信任它们。训练时看起来惊艳，一到新数据上就崩掉。这是通过 meta-feature 造成的**标签泄漏**。

解决办法是给元学习器**out-of-fold（OOF）预测** —— 即基学习器*未*在其上训练过的数据上的预测：

- **Blending** —— 切出一小块 holdout：在其余数据上训练基学习器，在 holdout 上预测，用这些 holdout 预测训练元学习器。简单，但元学习器只能见到一小片数据。
- **Stacking（正规做法）** —— 使用 **k-fold** 交叉验证。在 $k-1$ 个 fold 上训练基学习器，在被留出的那个 fold 上预测；轮换，使每行训练数据都能得到一个 out-of-fold 预测。元学习器在全部 OOF 预测上训练 —— 用到了全部数据，且无泄漏。

### 多层 stacking

可以做**多层** stacking，同时降低**偏差**：第 2 层学习器在第 1 层学习器的*输出*上训练（通常还会**拼接原始输入**，这有帮助）。为免过拟合，每一层用不同的数据训练 —— 划分为 A 和 B，在 A 上训练 L1，在 B 上做推理以生成 L2 的训练数据。**AutoGluon** 用**重复 k-fold bagging** 把这一点推广开：像 k-fold CV 那样训练 $k$ 个模型，合并每个模型的 out-of-fold 预测，然后重复 $n$ 次并取平均 —— 从而给上层提供干净、低方差的训练数据。

代价极其高昂，且值得用数字一看。在同一个 benchmark 上，增加**一层**带 5-fold 重复 bagging 的 stacking，误差只变化了 `0.229 → 0.227`，而训练时间变成 `39 s → 207 s`（≈5×）：

```python
from autogluon.tabular import TabularPredictor

predictor = TabularPredictor(label=label).fit(
    train, num_stack_levels=1, num_bag_folds=5)
```

这就是一页幻灯片上的收益递减定律：准确率提升微不足道，代价却是成倍增长 —— 与本讲贯穿始终的准确率对算力的权衡，是同一笔交易。

---


<details>
<summary>English original</summary>

**GBDT in practice — XGBoost & LightGBM**

The dominant form is **Gradient Boosting Decision Trees (GBDT)**: the weak learner is a decision tree, regularized by a small `max_depth` and random feature sampling. The catch is that boosting is **inherently sequential** — tree $t$ needs the residuals from tree $t-1$, so you cannot train the trees in parallel the way bagging does. Naive GBDT is slow.

The industry libraries solve this with accelerated algorithms that parallelize *within* each tree's construction:

- **XGBoost** — histogram-based split finding, regularized objective, sparsity-aware splits, the workhorse default.
- **LightGBM** — leaf-wise tree growth and histogram bucketing for large datasets; typically the fastest, very memory-efficient.
- **CatBoost** — ordered boosting and native categorical handling, strong when you have many categorical features.

These are the reason GBDTs, not deep nets, remain the default for **tabular** data.

---

**4. Stacking — a meta-learner over diverse models**

**Stacking trains diverse base learners and then a meta-learner on top of their predictions.** Where bagging gets diversity from *bootstrap samples of the same model type*, stacking gets it from **different model types entirely** — a Random Forest, a GBDT, and an MLP all look at the same inputs but extract different kinds of structure. The meta-learner (often a simple linear model) learns how to **weight and combine** their outputs.

```text
                 ┌─────────────┐
                 │   Dense     │   ← meta-learner: learns the combination weights
                 └──────┬──────┘
                 ┌──────┴──────┐
                 │   Concat    │   ← stack base-learner predictions
                 └──────┬──────┘
        ┌───────────────┼───────────────┐
   ┌────┴────┐     ┌────┴────┐      ┌────┴────┐
   │ Random  │     │  GBDT   │  …   │   MLP   │   ← diverse base learners
   │ Forest  │     │         │      │         │
   └────┬────┘     └────┬────┘      └────┬────┘
        └───────────────┼───────────────┘
                 ┌──────┴──────┐
                 │   Inputs    │
                 └─────────────┘
```

This is the method that **wins competitions**. On the CS329P house-sales benchmark, stacking diverse learners beat each one alone:

| Model | Test error |
|---|---|
| GBDT | 0.259 |
| RandomForest | 0.243 |
| **Stacking (AutoGluon)** | **0.229** |

```python
from autogluon.tabular import TabularPredictor

predictor = TabularPredictor(label=label).fit(train)   # stacks a zoo of models for you
```

**The leakage trap — you MUST use held-out predictions**

Here is the mistake that quietly ruins naive stacking. If you train the base learners on the full training set and then feed their predictions *on that same training set* to the meta-learner, the base models have already **seen** those labels — their training predictions are unrealistically good, so the meta-learner learns to trust them far more than it should. It looks brilliant in training and falls apart on new data. This is **label leakage** through the meta-features.

The fix is to give the meta-learner **out-of-fold (OOF) predictions** — predictions on data the base learner did *not* train on:

- **Blending** — split off a small holdout: train base learners on the rest, predict on the holdout, train the meta-learner on those holdout predictions. Simple, but the meta-learner only sees a small slice.
- **Stacking (proper)** — use **k-fold** cross-validation. Train base learners on $k-1$ folds, predict the held-out fold; rotate so every training row gets an out-of-fold prediction. The meta-learner trains on the full set of OOF predictions — uses all the data, no leakage.

**Multi-layer stacking**

You can stack in **multiple levels** to also reduce **bias**: the level-2 learners train on the *outputs* of the level-1 learners (often **concatenating the original inputs**, which helps). To keep this from overfitting, train each level on different data — split into A and B, train L1 on A, run inference on B to generate L2's training data. **AutoGluon** generalizes this with **repeated k-fold bagging**: train $k$ models as in k-fold CV, combine each model's out-of-fold predictions, then repeat $n$ times and average — giving the upper level clean, low-variance training data.

The cost is brutal and worth seeing in numbers. On the same benchmark, adding **one** extra stacked level with 5-fold repeated bagging moved error only `0.229 → 0.227` while training time went `39 s → 207 s` (≈5×):

```python
from autogluon.tabular import TabularPredictor

predictor = TabularPredictor(label=label).fit(
    train, num_stack_levels=1, num_bag_folds=5)
```

That is the law of diminishing returns on a slide: a tiny accuracy gain for a multiplicative cost — the same accuracy-vs-compute bargain that runs through this whole lecture.

---

</details>

## 5. 综合起来看——何时用哪种组合方法

| | **Bagging** | **Boosting** | **Stacking** |
|---|---|---|---|
| **降低的是** | 方差 | 偏差 | 方差（多层时两者皆降） |
| **训练顺序** | 并行（相互独立） | 串行（每个修正上一个） | 基学习器并行，meta 在顶层 |
| **可并行？** | 是——embarrassingly 并行 | 否——存在串行依赖 | 基学习器可以；各层串行 |
| **基学习器** | *一个*不稳定模型的众多副本（如树） | 众多 *弱* 学习器（浅层树） | *多样* 的模型类型（RF + GBDT + 多层感知机） |
| **多样性来源** | Bootstrap 采样（+ 特征子集） | 重加权 / 聚焦残差 | 不同的模型架构 |
| **过拟合风险** | 低——学习器越多越安全 | **无正则化时高**——轮数越多可能越差 | 中等——跳过 out-of-fold 就会泄漏 |
| **典型例子** | 随机森林 | XGBoost / LightGBM（GBDT） | AutoGluon / Kaggle 竞赛优胜方案 |
| **免费验证** | Out-of-bag（OOB）估计 | 在留出集上 early stopping | Out-of-fold 预测 |

CS329P 的汇总表，其中 $n$ = 学习器数量，$l$ = 层数，$k$ = 折数：

```text
                       Reduce Bias   Reduce Var   Compute cost   Parallelization
   Bagging                  -            Y             n               n
   Boosting                 Y            -             n               1
   Stacking                 -            Y             n               n
   K-fold multi-level       Y            Y           n×l×k            n×k
```

把它当作 recipe 来读：**欠拟合 → boost；过拟合 → bag；想拿最后 1% 且有算力 → stack。**

> **硬件视角：** 本讲里的每一分准确率提升，都是用 **推理成本** 换来的。由 $N$ 个模型组成的集成，在推理服务时就是 $N$ 次前向传播——大约是单个模型的 **$N\times$ 倍延迟、$N\times$ 倍内存**（1000 棵树的 GBDT 每次预测要评估 1000 棵树；5 个模型的 stack 要跑 5 个完整模型再加 meta-learner；多层 stacking 还要再乘上层数与折数）。离线场景下，在一台 Kaggle 机器上，这是免费的——你有整晚的时间。在线场景下，在固定的加速器预算和延迟 SLA 约束下，它往往 *负担不起*：在必须 20 ms 内应答的请求路径里，你塞不进 5× 的延迟开销，也未必有 5× 的 GPU 内存让整个集成常驻。这正是 **Lecture 10（模型压缩——剪枝、量化、蒸馏）** 要填补的缺口。**蒸馏** 是最直接的答案：先训练大集成去赢下指标，再训练 *一个* 小的学生模型去模仿集成的输出——以 $1\times$ 的推理成本保住大部分准确率。真正交付的工作流是 *用集成探到上限，用蒸馏负担得起。* 所以要同时测两个数字：榜单分数 **以及** 单次预测成本，因为在生产环境里，后者是硬约束，不是脚注。[→ Lecture 10](Lecture-10.md)

> **2026 更新：** 五年过去，本讲的论点依然成立，而且更锋利。**（1）GBDT 集成依然统治表格数据。** XGBoost、LightGBM 和 CatBoost 仍是表格类 Kaggle 竞赛和大多数结构化工业数据上的默认赢家——尽管屡有尝试（TabNet、FT-Transformer、SAINT），深度网络在那里 *并未* 取代它们；近期像 **TabPFN v2** 这类 in-context 表格模型在小数据上有前景，但在大规模上还没有把 boosted trees 拉下马。**（2）用深度集成处理不确定性。** 用不同随机种子训练几个神经网络再做平均，如今已是 **预测不确定性与校准** 的标准强基线——它稳定胜过更花哨的贝叶斯近似，被用在需要知道 *模型何时不确定* 的场合（安全、主动学习、OOD 检测）。**（3）用权重平均替代输出平均——"model soups"。** 一个 2022 年之后提出并留存下来的想法：与其在推理服务时部署 $N$ 个模型，不如把 **它们的权重平均** 成一组。**Model soups**（把同一基座模型的多个微调结果的权重做平均）和 **SWA / WiSE-FT** 在提升准确率与鲁棒性的同时只付出 **$1\times$ 推理成本**——这正是硬件视角所求的圣杯，前提是这些模型必须处在权重空间中相互兼容的区域（通常是共享初始化的多个微调，而非任意架构）。对 LLM 而言，权重空间的 *合并*（例如 linear/SLERP 合并、TIES、DARE）是同一思路，用来把多个专用微调合并成一个被部署的模型。这十年的模式是：**拿到集成的准确率，却不付集成的推理服务成本**——靠蒸馏它（Lecture 10），或靠平均权重而不是平均输出。

---


<details>
<summary>English original</summary>

**5. Putting it together — which combiner, when**

| | **Bagging** | **Boosting** | **Stacking** |
|---|---|---|---|
| **Reduces** | Variance | Bias | Variance (both, if multi-layer) |
| **Training order** | Parallel (independent) | Sequential (each fixes the last) | Base parallel, meta on top |
| **Parallelizable?** | Yes — embarrassingly | No — sequential dependency | Base learners yes; levels sequential |
| **Base learners** | Many copies of *one* unstable model (e.g. trees) | Many *weak* learners (shallow trees) | *Diverse* model types (RF + GBDT + MLP) |
| **Diversity from** | Bootstrap samples (+ feature subsets) | Reweighting / residual focus | Different model architectures |
| **Overfit risk** | Low — more learners is safe | **High if unregularized** — more rounds can hurt | Medium — leaks if you skip out-of-fold |
| **Canonical example** | Random Forest | XGBoost / LightGBM (GBDT) | AutoGluon / Kaggle winners |
| **Free validation** | Out-of-bag (OOB) estimate | Early-stopping on a holdout | Out-of-fold predictions |

The CS329P summary table, with $n$ = number of learners, $l$ = levels, $k$ = folds:

```text
                       Reduce Bias   Reduce Var   Compute cost   Parallelization
   Bagging                  -            Y             n               n
   Boosting                 Y            -             n               1
   Stacking                 -            Y             n               n
   K-fold multi-level       Y            Y           n×l×k            n×k
```

Read it as a recipe: **underfitting → boost; overfitting → bag; want the last 1% and have the compute → stack.**

> **Hardware lens:** Every accuracy gain in this lecture is bought with **inference cost**. An ensemble of $N$ models is, at serve time, $N$ forward passes — roughly **$N\times$ the latency and $N\times$ the memory** of a single model (a 1000-tree GBDT evaluates 1000 trees per prediction; a 5-model stack runs 5 full models plus the meta-learner; multi-layer stacking multiplies again by levels and folds). Offline, on a Kaggle box, that is free — you have all night. Online, behind a latency SLA on a fixed accelerator budget, it is often *unaffordable*: you cannot put a 5× latency hit in a request path that must answer in 20 ms, and you may not have 5× the GPU memory to hold the ensemble resident. This is precisely the gap **Lecture 10 (Model Compression — Pruning, Quantization, Distillation)** exists to close. **Distillation** is the most direct answer: train the big ensemble to win the metric, then train *one* small student to mimic the ensemble's outputs — keeping much of the accuracy at $1\times$ inference cost. The workflow that ships is *ensemble to find the ceiling, distill to afford it.* So measure both numbers: the leaderboard score **and** the per-prediction cost, because in production the second one is a hard constraint, not a footnote. [→ Lecture 10](Lecture-10.md)

> **2026 update:** Five years on, the lecture's thesis holds, sharpened. **(1) GBDT ensembles still own tabular.** XGBoost, LightGBM, and CatBoost remain the default winners on tabular Kaggle competitions and most structured industry data — deep nets have *not* displaced them there, despite repeated attempts (TabNet, FT-Transformer, SAINT), and recent in-context tabular models like **TabPFN v2** are promising on small data but have not dethroned boosted trees at scale. **(2) Deep ensembles for uncertainty.** Training a handful of neural nets with different seeds and averaging is now a standard, strong baseline for **predictive uncertainty and calibration** — it reliably beats fancier Bayesian approximations, and is used where knowing *when the model is unsure* matters (safety, active learning, OOD detection). **(3) Weight-averaging instead of output-averaging — "model soups".** A 2022-onward idea that stuck: rather than serve $N$ models, **average their weights** into a single set. **Model soups** (averaging the weights of multiple fine-tunes of the same base model) and **SWA / WiSE-FT** improve accuracy and robustness while paying only **$1\times$ inference cost** — the holy grail the hardware lens asks for, with the catch that it requires the models to live in a compatible region of weight space (typically fine-tunes of a shared initialization, not arbitrary architectures). For LLMs, weight-space *merging* (e.g. linear/SLERP merges, TIES, DARE) is the same instinct applied to combining specialized fine-tunes into one served model. The pattern of the decade: **get the ensemble's accuracy without paying the ensemble's serving cost** — by distilling it (Lecture 10) or by averaging weights instead of outputs.

---

</details>

## 截至当前

撰写于 2026 年 6 月。**最初的 CS329P 内容** —— 偏差–方差分解与取舍、bagging（bootstrap、OOB、Random Forest、方差归约论证）、boosting（AdaBoost 与梯度提升、残差 = 负梯度、收缩/子采样/早停、用 XGBoost/LightGBM 实现的 GBDT），以及 stacking（多样化基学习器、折外元特征、配合重复 k 折 bagging 的多层 stacking、AutoGluon，以及汇总表）—— 被排在最前面讲授，因为它仍是正确的工作心智模型，并与 2021 年的幻灯片一一对应。**2026 刷新层**标出哪些内容发生了变化：GBDT 在表格数据上对深度网络的持续优势（以及作为值得持续关注的例外的 TabPFN）、**深度集成**作为标准的不确定性/校准基线，以及**权重平均 —— model soups、SWA、WiSE-FT 与 LLM 权重合并** —— 作为以单模型推理成本获得集成准确率的方式，它与**蒸馏（第 10 讲）**一起，回应了硬件视角的核心诟病。凡 2021 年表述已显陈旧之处，先呈现原文，再给出更新；没有任何内容被悄悄改写。

*改编自 [Stanford CS329P](https://c.d2l.ai/stanford-cs329p) — Huang, Li & Smola, CC-BY-SA-4.0。*


<details>
<summary>English original</summary>

**Current as of**

Written June 2026. The **original CS329P content** — the bias–variance decomposition and tradeoff, bagging (bootstrap, OOB, Random Forest, the variance-reduction argument), boosting (AdaBoost vs. gradient boosting, residual = negative gradient, shrinkage/subsampling/early-stopping, GBDT with XGBoost/LightGBM), and stacking (diverse base learners, out-of-fold meta-features, multi-layer stacking with repeated k-fold bagging, AutoGluon, and the summary table) — is taught first because it remains the correct working mental model and maps one-to-one onto the 2021 slides. The **2026 refresh layer** flags what moved: GBDTs' continued dominance over deep nets on tabular data (and TabPFN as the watch-this-space exception), **deep ensembles** as the standard uncertainty/calibration baseline, and **weight-averaging — model soups, SWA, WiSE-FT, and LLM weight-merging** — as the way to capture an ensemble's accuracy at single-model inference cost, which together with **distillation (Lecture 10)** answers the hardware lens's core complaint. Where 2021 framing is dated, the original is presented before the update; nothing is silently rewritten.

*Adapted from [Stanford CS329P](https://c.d2l.ai/stanford-cs329p) — Huang, Li & Smola, CC-BY-SA-4.0.*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/4. ML Engineering and MLOps/Practical Machine Learning (CS329P)/Lecture-05.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/4.%20ML%20Engineering%20and%20MLOps/Practical%20Machine%20Learning%20%28CS329P%29/Lecture-05.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
