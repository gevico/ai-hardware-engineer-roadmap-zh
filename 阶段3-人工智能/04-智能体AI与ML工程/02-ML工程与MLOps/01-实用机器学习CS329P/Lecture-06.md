---
title: 第 06 讲 - 分布偏移：协变量偏移与标签偏移
description: 第 06 讲 - 分布偏移：协变量偏移与标签偏移
---

# 第 06 讲 - 分布偏移：协变量偏移与标签偏移

**合集：** [Practical Machine Learning (CS329P)](README.md) | **上一篇：** [← 第 05 讲](Lecture-05.md) | **下一篇：** [第 07 讲](Lecture-07.md)

---

你训练过的每一个监督模型，都建立在一个如此悄无声息、以至于你很可能从未把它写下来的假设之上：你据以训练的数据与你据以服务的数据，都抽自*同一个*分布 `p(x, y)`。这就是 IID 假设——独立同分布——它是机器学习中沉默的承重墙。泛化理论的整座大厦，以及一个好的验证分数之所以本应具有任何意义的原因，都建立在它之上。而生产环境会悄然打破它。不是大张旗鼓地，也不是靠日志里的一条异常——而是悄然地。在你的 notebook 里拿到 94% 的模型上线了，平稳运行了一个季度，随后准确率漂移到 88%，期间没有任何代码改动、没有 bug、没有糟糕的部署。模型并没有腐坏。是世界变了。

这就是**分布偏移**，它是已部署模型衰减最常见的单一原因。经验分布会说谎——你的训练集始终只是一个不断在其脚下变化的世界的有限样本。一个在 CES 上演示的自动售货机视觉模型在展会现场失效，因为光照与实验室不同（这是 CES 2019 的一个真实故事：解决办法是新数据、一块用来消除桌面反光的桌布，以及一整夜的重新训练）。一个用西海岸口音训练的语音模型，遇到得克萨斯的拖腔便磕磕绊绊。一个在 COVID 尚属罕见时训练的医学分类器，在超级传播事件之后被部署到一个 COVID 突然变得常见的城镇。这些都不是通常意义上的建模失败。函数 `f` 本身没问题；它如今被问及的输入来自一个它从未见过的分布。

本讲构建了应对它的分类体系和数学：如何**命名**你所面临的偏移类型（协变量、标签或概念），如何通过对训练数据重新加权来**纠正**两种可处理的类型，如何使用双样本检验来**检测**偏移究竟是否发生，以及偏移的最坏情况——对抗数据——如何与鲁棒性相联系。贯穿始终的主线，按 CS329P 的表述：*训练 ≠ 测试*，而 ML 工程师的纪律，就在于精确地知道它们*如何*不同，以及你被允许对此做什么。

---

## 学习目标

在本讲结束时，你应该能够：

1. **陈述 IID 假设**，并解释为什么一个强的验证分数对于良好的测试时性能是*必要但不充分*的。
2. 从联合 `p(x, y)` 如何分解出发，**将偏移分类**为协变量偏移、标签偏移或概念漂移，并各给出一个具体的生产示例。
3. 通过重要性加权**纠正协变量偏移**，用域分类器估计密度比，并通过裁剪权重 / 监控有效样本量来稳定它。
4. **将对抗样本与分布偏移联系起来**，视其为最坏情况扰动，并解释作为防御手段的不变性与对抗鲁棒损失。
5. 用双样本检验**检测偏移**——分类器检验、最大均值差异（MMD）和 Kolmogorov–Smirnov 统计量——并知道该选用哪一个。
6. 用黑盒偏移估计（BBSE）**纠正标签偏移**：从混淆矩阵和预测标签分布中恢复 `q(y)`，然后重新加权。

---


<details>
<summary>English original</summary>

**Lecture 06 - Distribution Shift: Covariate & Label Shift**

**Collection:** [Practical Machine Learning (CS329P)](README.md) | **Previous:** [← Lecture 05](Lecture-05.md) | **Next:** [Lecture 07](Lecture-07.md)

---

Every supervised model you have ever trained rests on one assumption so quiet you probably never wrote it down: that the data you train on and the data you serve on are drawn from the *same* distribution `p(x, y)`. This is the IID assumption — independent and identically distributed — and it is the silent load-bearing wall of machine learning. The whole edifice of generalization theory, the reason a good validation score is supposed to mean anything at all, is built on it. And production quietly breaks it. Not loudly, not with an exception in the logs — quietly. The model that scored 94% in your notebook ships, runs fine for a quarter, and then accuracy drifts down to 88% with no code change, no bug, no bad deploy. The model did not rot. The world moved.

This is **distribution shift**, and it is the single most common reason deployed models decay. The empirical distribution lies — your training set was only ever a finite sample of a world that keeps changing underneath it. A vending-machine vision model demoed at CES fails on the show floor because the lighting is different from the lab (a true story from CES 2019: the fix was new data, a tablecloth to kill the table reflection, and an all-night retrain). A speech model trained on West-Coast accents stumbles on a Texan drawl. A medical classifier trained when COVID was rare gets deployed into a town right after a super-spreader event where it is suddenly common. None of these is a modeling failure in the usual sense. The function `f` is fine; the input it is now being asked about comes from a distribution it never saw.

This lecture builds the taxonomy and the math to handle it: how to **name** the kind of shift you have (covariate, label, or concept), how to **correct** for the two tractable kinds by reweighting your training data, how to **detect** that a shift happened at all using two-sample tests, and how the worst case of shift — adversarial data — connects to robustness. The throughline, in CS329P's framing: *training ≠ testing*, and the discipline of an ML engineer is knowing precisely *how* they differ and what you are allowed to do about it.

---

**Learning objectives**

By the end of this lecture you should be able to:

1. **State the IID assumption** and explain why a strong validation score is *necessary but not sufficient* for good test-time performance.
2. **Classify a shift** as covariate shift, label shift, or concept drift from how the joint `p(x, y)` factorizes, and give a concrete production example of each.
3. **Correct covariate shift** by importance weighting, estimating the density ratio with a domain classifier, and stabilizing it by clipping weights / monitoring effective sample size.
4. **Connect adversarial examples to distribution shift** as the worst-case perturbation, and explain invariances and adversarially-robust loss as defenses.
5. **Detect a shift** with a two-sample test — classifier test, Maximum Mean Discrepancy (MMD), and the Kolmogorov–Smirnov statistic — and know which to reach for.
6. **Correct label shift** with Black-Box Shift Estimation (BBSE): recover `q(y)` from a confusion matrix and the predicted-label distribution, then reweight.

---

</details>

## 1. 泛化回顾 —— 为什么好的验证分数还不够

从训练实际做的事出发。你有一个数据分布 `p(x, y)`，从中抽取有限的数据集，训练则最小化**经验风险**加正则化项：

```text
Empirical (training) risk:
  R_emp[f, X, Y] = (1/m) Σ_{i=1..m} l( f(x_i, w), y_i )

What you actually care about — expected risk over the true distribution:
  R[f, p] = E_{(x,y)~p} [ l( f(x, w), y ) ]
```

训练压低的是前者。部署则由后者评判 —— 即*你本可能见到的所有其他数据*上的期望损失，而不是你实际见到的那一小撮。二者之间的差距正是泛化的全部主题，而这一差距之所以能被限定，靠的是 IID 假设：训练与测试是同一个 `p`。

如何获得「低经验风险蕴含低期望风险」的保证？留出一个从未用于训练的**验证集**，并应用集中不等式。对于界在 `[0, 1]` 的损失，Chernoff/Hoeffding 界表明：`m` 个独立点上的经验均值以高概率接近真实均值：

```text
Pr[ | (1/m) Σ l(f(x_i), y_i) − E[l(f(x), y)] | > ε ]  ≤  exp(−2 m ε²)

Solving for a confidence 1 − δ gives a sample-size rule of thumb:
  for δ = 0.05 and ε = 0.01  →  m ≈ 15,000 independent validation points.
```

两个注意点恰恰是生产环境出问题的地方。**第一**，该界假设验证集*从未*用于训练 —— 一旦你针对它调超参数、反复偷看，或者让预处理统计量跨切分泄漏，这一假设就"常被违反"（Lecture 02 的纪律：normalizer 只在 train 上拟合）。**第二，也是本讲的重点：该界假设验证点与测试点抽自同一分布。** 一个干净、未被触碰、与训练完全 IID 的验证集，对于分布已经漂移的部署，仍然*什么都*说明不了。

所以结论很鲜明。训练集上的好表现并不保证好的测试表现，除非你对容量做正则化（输入噪声、dropout、权重衰减 —— 都是某种形式的平滑 `f`）*或*留出一个独立验证集做诚实的校准。而即便两者齐备，也只能证明**在验证集所来自的那个分布上**的表现。好的验证分数是必要条件 —— 连验证都做不好的模型毫无希望 —— 但并不充分。本讲余下部分讨论的就是「不充分」这部分。

---


<details>
<summary>English original</summary>

**1. Generalization recap — why a good val score is not enough**

Start from what training actually does. You have a data distribution `p(x, y)`, you draw a finite dataset from it, and training minimizes the **empirical risk** plus regularization:

```text
Empirical (training) risk:
  R_emp[f, X, Y] = (1/m) Σ_{i=1..m} l( f(x_i, w), y_i )

What you actually care about — expected risk over the true distribution:
  R[f, p] = E_{(x,y)~p} [ l( f(x, w), y ) ]
```

Training drives down the first. Deployment is judged by the second — the expected loss over *all the other data you could have seen*, not the handful you did. The gap between them is the entire subject of generalization, and the reason the gap is ever bounded is the IID assumption: train and test are the same `p`.

How do we get any guarantee that low empirical risk implies low expected risk? Hold out a **validation set** that was never used for training, and apply a concentration bound. For a loss bounded in `[0, 1]`, the Chernoff/Hoeffding bound says the empirical mean over `m` independent points is close to the true mean with high probability:

```text
Pr[ | (1/m) Σ l(f(x_i), y_i) − E[l(f(x), y)] | > ε ]  ≤  exp(−2 m ε²)

Solving for a confidence 1 − δ gives a sample-size rule of thumb:
  for δ = 0.05 and ε = 0.01  →  m ≈ 15,000 independent validation points.
```

Two caveats are exactly where production breaks. **First**, the bound assumes the validation set was *never* used for training — an assumption "often violated" the moment you tune hyperparameters against it, peek repeatedly, or leak preprocessing statistics across the split (the discipline from Lecture 02: fit normalizers on train only). **Second, and the whole point of this lecture: the bound assumes the validation points are drawn from the same distribution as test.** A pristine, untouched, perfectly-IID-with-training validation set still tells you *nothing* about a deployment whose distribution has moved.

So the takeaway is sharp. Good performance on the training set does not guarantee good test performance unless you regularize capacity (input noise, dropout, weight decay — all forms of smoothing `f`) *or* hold out an independent validation set for honest calibration. And even both together only certify performance **on the distribution the validation set came from.** A good val score is necessary — a model that can't even validate well is hopeless — but it is not sufficient. The rest of this lecture is about the "not sufficient" part.

---

</details>

## 2. 偏移的分类体系

组织分布偏移的清晰做法，是按 *联合分布的哪个因子发生了变化* 来划分。把训练分布记作 `p(x, y)`，测试分布记作 `q(x, y)`。联合分布总有两种分解方式 —— `p(x)·p(y|x)` 或 `p(y)·p(x|y)` —— 而每一类偏移都是固定其中一个因子、让另一个因子变动。

| 偏移类型 | 变化的是什么 | 不变的是什么 | 分解形式 | 具体例子 |
|---|---|---|---|---|
| **协变量偏移** | `p(x) → q(x)`（输入） | `p(y\|x)`（给定输入下的标签） | `q(x, y) = q(x)·p(y\|x)` | 在影棚灯光下训练人脸/物体识别器；部署到荧光灯照明的商店里。像素偏移了；*给定像素后猫长什么样* 没有偏移。（CES 自动售货机。） |
| **标签偏移** | `p(y) → q(y)`（类别先验） | `p(x\|y)`（给定标签下的输入） | `q(x, y) = q(y)·p(x\|y)` | 一个 COVID 分类器：在疾病罕见的地区训练，部署到超级传播事件之后的小镇，那里 `q(C19) ≫ p(C19)`。*流行率* 偏移了；而病人的症状，`p(symptoms\|C19)`，没有偏移。 |
| **概念漂移** | `p(y\|x) → q(y\|x)`（标注规则） | `p(x)`（通常是） | 条件分布本身发生移动 | 什么算「时尚」、「垃圾邮件」或「欺诈」交易，会随时间变化。同样的输入现在映射到不同的标签。 |

这一区分并非学术讨论 —— **它决定了哪种修复才是可能的。**

- **协变量偏移**（`p(x) ≠ q(x)`，标注规则固定）可以通过对训练样本重新加权来纠正，让训练输入分布看起来像测试分布。你学到的关系 `p(y|x)` 仍然有效；你只是从错误的密度中采样了输入。见 §3。
- **标签偏移**（`p(y) ≠ q(y)`，`p(x|y)` 固定）可以通过*按类别*重新加权来纠正。类条件外观仍然有效；改变的只是类别的混合比例。见 §6，而且它在某些方面是更容易的修复 —— *「当你有 q(y) 时，修模型很容易。」*
- **概念漂移**才是真正困难的一类。依赖关系 `p(y|x)` 本身已经改变，所以你学到的关于旧映射的任何东西都无法干净地迁移。CS329P 说得很直白：*「如果概念在训练集与测试集之间发生偏移，问题要大得多 —— 不可能给出真正的保证。」* 如果它随时间缓慢漂移，有时可以通过训练一个带时间索引的模型 `p(y|x, t)` 来跟踪它；如果它突然偏移，你就需要新的标签。

讲课时点出了两个相关情形，标注为「我们没有覆盖的内容」，但值得命名。**协变量漂移**是随时间缓慢发生的协变量偏移（语言用法、人口结构、地理偏好 —— 加拿大与美国的搜索行为）；策略是把协变量密度建模为随时间变化的函数。而 **对抗数据** 是它自己的病态情形 —— `supp(p) ≠ supp(q)`，其中测试数据完全落在训练数据的*支撑集之外* —— §4 将其视为最坏情况。

一条有用的自检规则：协变量偏移 = 「协变量分布撒谎」，标签偏移 = 「标签分布撒谎」，概念漂移 = 「关系撒谎」。判断自己面对哪一种时，问一句 *数据是由哪个因果方向生成的*。如果 `x` 导致 `y`（图像 → 标签），那么 `p(x)` 的偏移就是协变量偏移。如果 `y` 导致 `x`（疾病 → 症状），那么 `p(y)` 的偏移就是标签偏移。因果箭头告诉你哪个因子是稳定的，因此也告诉你哪种修复适用。

---


<details>
<summary>English original</summary>

**2. A taxonomy of shift**

The clean way to organize distribution shift is by *which factor of the joint distribution changed.* Write the training distribution as `p(x, y)` and the test distribution as `q(x, y)`. The joint always factors two ways — `p(x)·p(y|x)` or `p(y)·p(x|y)` — and each kind of shift holds one factor fixed while the other moves.

| Shift type | What changes | What stays fixed | Factorization | Concrete example |
|---|---|---|---|---|
| **Covariate shift** | `p(x) → q(x)` (the inputs) | `p(y\|x)` (label given input) | `q(x, y) = q(x)·p(y\|x)` | Train a face/object recognizer on studio lighting; deploy under fluorescent store lighting. The pixels shift; *what a cat looks like given its pixels* does not. (The CES vending machine.) |
| **Label shift** | `p(y) → q(y)` (class priors) | `p(x\|y)` (input given label) | `q(x, y) = q(y)·p(x\|y)` | A COVID classifier: train where the disease is rare, deploy in a town after a super-spreader event where `q(C19) ≫ p(C19)`. The *prevalence* shifts; the symptoms of a sick patient, `p(symptoms\|C19)`, do not. |
| **Concept drift** | `p(y\|x) → q(y\|x)` (the labeling rule) | `p(x)` (often) | the conditional itself moves | What counts as "fashionable", "spam", or a "fraudulent" transaction changes over time. The same input now maps to a different label. |

The distinction is not academic — **it dictates what fix is even possible.**

- **Covariate shift** (`p(x) ≠ q(x)`, label rule fixed) is correctable by reweighting training examples so the training input distribution looks like the test one. The relationship you learned, `p(y|x)`, is still valid; you just sampled the inputs from the wrong density. This is §3.
- **Label shift** (`p(y) ≠ q(y)`, `p(x|y)` fixed) is correctable by reweighting *by class*. The class-conditional appearance is still valid; only the mix of classes changed. This is §6, and it is in some ways the easier fix — *"when you have q(y), fixing models is easy."*
- **Concept drift** is the genuinely hard one. The dependency `p(y|x)` itself has changed, so nothing you learned about the old mapping transfers cleanly. CS329P is blunt: *"much bigger problem if concept shifts between training and test set — no real guarantees possible."* If it drifts slowly over time, you can sometimes track it by training a time-indexed model `p(y|x, t)`; if it shifts abruptly, you need fresh labels.

Two related cases the lecture flags as "things we didn't cover" but worth naming. **Covariate drift** is slow covariate shift over time (language usage, demographics, geographic preferences — Canada vs. USA search behavior); the strategy is to model the covariate density as a time-varying function. And **adversarial data** is its own pathological case — `supp(p) ≠ supp(q)`, where test data lands *off the support* of training data entirely — which §4 treats as the worst case.

A useful sanity rule: covariate shift = "the covariate distribution lies," label shift = "the label distribution lies," concept drift = "the relationship lies." When deciding which you face, ask *which causal direction generated the data.* If `x` causes `y` (image → label), shift in `p(x)` is covariate shift. If `y` causes `x` (disease → symptoms), shift in `p(y)` is label shift. The causal arrow tells you which factor is stable and therefore which fix applies.

---

</details>

## 3. 协变量偏移校正 —— 数学

假设存在协变量偏移：`p(y|x)` 不变，但 `p(x) ≠ q(x)`。你的训练风险是把损失对 *训练* 输入密度 `p(x)` 做积分；而你真正想要的是对 *测试* 密度 `q(x)` 的风险：

```text
What you minimized (training):   ∫ p(x) ∫ p(y|x) l(f(x,w), y) dy dx
What you want (test):            ∫ q(x) ∫ p(y|x) l(f(x,w), y) dy dx
```

两者只在最前面的输入密度上不同。解决办法是测度变换 —— 乘上并除以 `p(x)`：

```text
∫ q(x) f(x) dx  =  ∫ p(x) · [ q(x)/p(x) ] · f(x) dx  =  ∫ p(x) · β(x) · f(x) dx
                                  └───────┘
                            importance weight β(x)
```

所以，如果按 **密度比** `β(x) = q(x)/p(x)` 对每个训练样本重新加权，*重新加权后*的训练风险就是测试风险的无偏估计。在训练中相对测试被过度代表的样本会被降权；看起来像测试的样本会被升权。具体来说，校正后的目标函数变为：

```text
Original:    minimize_w  (1/m) Σ_i           l( y_i, f(x_i, w) )
Reweighted:  minimize_w  (1/m) Σ_i  β(x_i) · l( y_i, f(x_i, w) )
```

**问题在于：** 你并没有 `p` 或 `q` —— 你只有各自的 *样本*（你的训练集和一批测试/生产输入），而为了做除法去直接估计两个高维密度既困难又不稳定（密度接近零的地方你怎么办？）。优雅的做法是根本不去估计密度。**直接训练一个分类器来区分训练点和测试点，从而估计这个比值。**

给每个训练点打标签 `+1`、每个测试点打标签 `−1`，把它们混在一起，拟合一个概率二分类器 `r(y | x)`（逻辑回归是标准选择）。在最优处，它的条件类别概率为：

```text
r(y = +1 | x) =  p(x) / ( p(x) + q(x) )

⇒  β(x) = q(x)/p(x) = r(y = −1 | x) / r(y = +1 | x)
```

分类器两个输出概率之比 *就是* 重要性权重 —— 无需密度估计。完整流程：

```text
COVARIATE SHIFT CORRECTION
  1. Build a pooled dataset: training points labeled +1, test/prod points labeled −1.
  2. Train a binary classifier (e.g. logistic regression) to separate the two.
  3. CHECK FIRST: if it can't beat chance, there is no covariate shift — stop, do nothing.
     (Reuse the generalization-performance estimate from §1 to decide.)
  4. For each training point x_i, set weight  β(x_i) = r(−1 | x_i) / r(+1 | x_i).
  5. Retrain your real model on the training data, reweighting example i by β(x_i).
```

第 3 步一举两得：产生权重的 *同一个* 分类器本身就是一个双样本检验（§5）。如果训练与测试无法区分，权重全都 ≈ 1，跳过校正也没有任何坏处。

这里有一个值得一看的更深层统一。这种「重新加权到无法区分」**正是 GAN 的目标**：生成器对训练数据 `{(x_i, y_i)} → {β_i(x_i, y_i)}` 重新加权，使判别器再也无法把它与测试数据区分开，而极小极大最优恰好是在 `β(x) = q(x)/p(x)` 且两个分布不再可区分时达到（判别器被迫进入熵模式，处处 `r = 0.5`）。协变量偏移校正、GAN，以及最大熵 / 矩匹配视角（通过 MMD 匹配特征均值）是同一思想的三副面孔。

**实际中的不稳定性 —— 真正会咬人的部分。** 权重 `β` 是 *估计值*，因此会带来偏差和方差，真正的危险在于 `p` 与 `q` 差异很大时：少数几个巨大的权重占据主导，有效样本量随之崩塌。把它量化。对于加权样本均值 `x̄ = Σ β_i x_i`，方差按 `‖β‖₂² σ²` 缩放，这就引出了 **有效样本量**：

```text
m* = ‖β‖₁² / ‖β‖₂²        (equals m when all weights are equal; collapses when a few dominate)
```

如果三个点承载了 90% 的权重质量，你那「10,000 样本」的重新加权训练集表现起来就像几个点 —— 高方差，垃圾。标准补救办法是 **裁剪权重**，`β̄_i = min(β_i, C)`：

```text
Unclipped weights:   less bias, but HIGH variance when m* is small.
Clipped weights:     a little bias, but smaller variance (larger effective sample size).
```

裁剪用一个可控的偏差换取大幅的方差降低 —— 在实践中几乎总是值得。纪律是：在信任一次重新加权之前，先计算 `m*`；如果它相对于 `m` 已经大幅塌陷，那你的协变量偏移就严重到无法用重要性加权掩盖过去，你需要的是新数据，而不是新权重。

---


<details>
<summary>English original</summary>

**3. Covariate shift correction — the math**

Assume covariate shift: `p(y|x)` is unchanged, but `p(x) ≠ q(x)`. Your training risk integrates the loss against the *training* input density `p(x)`; what you actually want is the risk against the *test* density `q(x)`:

```text
What you minimized (training):   ∫ p(x) ∫ p(y|x) l(f(x,w), y) dy dx
What you want (test):            ∫ q(x) ∫ p(y|x) l(f(x,w), y) dy dx
```

The two differ only in the input density out front. The fix is a change of measure — multiply and divide by `p(x)`:

```text
∫ q(x) f(x) dx  =  ∫ p(x) · [ q(x)/p(x) ] · f(x) dx  =  ∫ p(x) · β(x) · f(x) dx
                                  └───────┘
                            importance weight β(x)
```

So if you reweight each training example by the **density ratio** `β(x) = q(x)/p(x)`, the *reweighted* training risk is an unbiased estimate of the test risk. Examples that are over-represented in training relative to test get down-weighted; examples that look like test get up-weighted. Concretely, the corrected objective becomes:

```text
Original:    minimize_w  (1/m) Σ_i           l( y_i, f(x_i, w) )
Reweighted:  minimize_w  (1/m) Σ_i  β(x_i) · l( y_i, f(x_i, w) )
```

**The catch:** you don't have `p` or `q` — you have *samples* from each (your training set and a batch of test/production inputs), and directly estimating two high-dimensional densities just to divide them is hard and unstable (what do you do where a density is near zero?). The elegant move is to never estimate the densities at all. **Estimate the ratio directly by training a classifier to tell training points from test points.**

Label every training point `+1` and every test point `−1`, pool them, and fit a probabilistic binary classifier `r(y | x)` (logistic regression is the canonical choice). At optimum its conditional class probability is:

```text
r(y = +1 | x) =  p(x) / ( p(x) + q(x) )

⇒  β(x) = q(x)/p(x) = r(y = −1 | x) / r(y = +1 | x)
```

That ratio of the classifier's two output probabilities *is* the importance weight — no density estimation required. The full procedure:

```text
COVARIATE SHIFT CORRECTION
  1. Build a pooled dataset: training points labeled +1, test/prod points labeled −1.
  2. Train a binary classifier (e.g. logistic regression) to separate the two.
  3. CHECK FIRST: if it can't beat chance, there is no covariate shift — stop, do nothing.
     (Reuse the generalization-performance estimate from §1 to decide.)
  4. For each training point x_i, set weight  β(x_i) = r(−1 | x_i) / r(+1 | x_i).
  5. Retrain your real model on the training data, reweighting example i by β(x_i).
```

Step 3 is doing double duty: the *same* classifier that produces the weights is itself a two-sample test (§5). If train and test are indistinguishable, the weights are all ≈ 1 and you have done no harm by skipping the correction.

There is a deeper unity here worth seeing. This reweighting-to-be-indistinguishable is **exactly the GAN objective**: a generator reweights training data `{(x_i, y_i)} → {β_i(x_i, y_i)}` so a discriminator can no longer tell it from test data, and the minimax optimum is reached precisely when `β(x) = q(x)/p(x)` and the distributions are no longer distinguishable (the discriminator is forced to entropy mode, `r = 0.5` everywhere). Covariate-shift correction, GANs, and a maximum-entropy / moment-matching view (matching feature means via MMD) are three faces of the same idea.

**Practical instability — the part that bites you.** The weights `β` are *estimates*, so they add bias and variance, and the real danger is when `p` and `q` are very different: a few enormous weights dominate and your effective sample size collapses. Make it quantitative. For a weighted sample mean `x̄ = Σ β_i x_i`, the variance scales as `‖β‖₂² σ²`, which motivates the **effective sample size**:

```text
m* = ‖β‖₁² / ‖β‖₂²        (equals m when all weights are equal; collapses when a few dominate)
```

If three points carry 90% of the weight mass, your "10,000-example" reweighted training set behaves like a handful of points — high variance, garbage. The standard remedy is to **clip the weights**, `β̄_i = min(β_i, C)`:

```text
Unclipped weights:   less bias, but HIGH variance when m* is small.
Clipped weights:     a little bias, but smaller variance (larger effective sample size).
```

Clipping trades a controlled bias for a large variance reduction — almost always worth it in practice. The discipline: compute `m*` before trusting a reweighting; if it has cratered relative to `m`, your covariate shift is too severe to paper over with importance weighting, and you need new data, not new weights.

---

</details>

## 4. 对抗样本与不变性

分布偏移有一个最坏情况，而且它很有启发性。与其让世界偶然漂移，不如设想有一个对手 *选择* 偏移来最大化你的损失。这就是**对抗样本**：取一个被正确分类的输入 `x`，找到能翻转预测的最小扰动 `δ`。

```text
maximize_δ   l( f(x + δ), y )
subject to   ‖δ‖ ≤ ε        (perturbation imperceptibly small)
```

这类样本确实存在，而且很容易构造——能躲过人脸识别的对抗图像（甚至以 3D 打印眼镜框的形式物理实现）、被以听不见的方式扰动从而转写成另一句话的音频。**为什么它有效？** 因为真实的训练数据和“自然”测试数据只落在输入空间中一个又小又薄的子集里，而函数的行为在 *数据未曾出现的区域本质上是未定义的*。对抗点就落在该支撑集稍外侧——`supp(p) ≠ supp(q)`——位于模型从未受约束的区域，因此那里的损失曲面可以被推向任何地方。这也正是你已经熟悉的一种军备竞赛的抽象结构：**垃圾邮件过滤。** 防御方重新训练，垃圾邮件发送者找到一个能规避的改动，防御方扩充数据集并重新训练——一个由对手驱动的分布，永远在移动。

与本讲其余部分的联系：有一个定理指出，*你总能找到一个让情况变得更糟的分布。* 给定均值为 `R[p, f]`、方差为 `σ²[p, f]` 的损失，存在一个 `q(x)` 满足 `R[q, f] ≥ R[p, f] + σ`——只需对模型本就出错的输入加权（一个中值定理式的论证：某个区域的条件损失高于平均值；把 `q` 的质量放在那里）。教训是防御性的：**在得出结论之前，永远要确认你的训练/测试分布确实匹配**——否则一个看似失败的结果可能只是一次不走运的（或对抗性的）重加权。

这两种防御互为对偶，区别在于 *你知道什么*：

- **不变性** —— 你 *知道* 会保持标签不变的变换。把猫左右翻转仍是猫；裁剪、改色、轻微畸变的图像仍是同一类；加入背景噪声的语音仍是同样的话。于是你**用这些变换增强**训练集（这就是 Lecture 02 的数据增强，现在透过偏移的视角来看）：你在有意地 *拓宽* `p` 的支撑集，以覆盖测试时预期出现的各种变化。经典源头：切线距离（Simard 1995）与虚拟支持向量（Schölkopf 1997）探索了一个点的邻域；如今则是标准的 ImageNet 增强组合（随机裁剪/缩放/翻转，以及通过 imgaug / Albumentations 之类的库做色相–饱和度–亮度抖动）。
- **对抗鲁棒性** —— 你并 *不* 知道应当保持标签不变的变换，而它们事实上会改变模型的输出。防御办法是把它们 *当作* 不变性来处理：把最坏情况烘进损失里。**对抗鲁棒的损失**在一族变换 `Δ` 上取上确界（另加一个惩罚项 `η(δ)` 来抑制极端畸变）：

```text
L(x, y, f) = sup_{δ ∈ Δ}  η(δ) · l( f(x + δ), y )
```

在每一步训练中，你找出 *最坏* 的扰动并针对它训练，于是模型被迫不仅在数据点上正确，还要在其周围一个鲁棒邻域上正确。简洁的总结：面对**不变性**，你 *知道* 该变换不改变结果，于是把它加进去以获得更强的鲁棒性；面对**对抗数据**，你 *不* 知道它应当不改变结果，观察到它确实改变了结果，于是仍然把它当作不变性来防御。

---


<details>
<summary>English original</summary>

**4. Adversarial examples & invariants**

Distribution shift has a worst case, and it is instructive. Instead of the world drifting by accident, imagine an adversary *choosing* the shift to maximize your loss. This is an **adversarial example**: take a correctly-classified input `x`, and find the smallest perturbation `δ` that flips the prediction.

```text
maximize_δ   l( f(x + δ), y )
subject to   ‖δ‖ ≤ ε        (perturbation imperceptibly small)
```

These exist and are easy to construct — adversarial images that dodge face recognition (even realized physically as 3D-printed glasses frames), audio perturbed inaudibly to transcribe as a different phrase. **Why does it work?** Because real training and "natural" test data live in a small, thin subset of input space, and the function's behavior is essentially *undefined away from where data occurred.* An adversarial point sits slightly off that support — `supp(p) ≠ supp(q)` — in a region the model was never constrained on, so the loss surface there can be pushed anywhere. This is also the abstract structure of an arms race you already know: **spam filtering.** The host retrains, the spammer finds a modification that evades, the host extends the dataset and retrains — a moving distribution driven by an adversary, forever.

The connection to the rest of the lecture: there is a theorem that *you can always find a distribution that makes things worse.* Given a loss with mean `R[p, f]` and variance `σ²[p, f]`, there exists a `q(x)` with `R[q, f] ≥ R[p, f] + σ` — just overweight the inputs where the model already errs (a mean-value-theorem argument: some region has above-average conditional loss; put `q`'s mass there). The lesson is defensive: **always confirm your train/test distributions actually match before drawing conclusions** — otherwise an apparent failure may just be an unlucky (or adversarial) reweighting.

The two defenses are duals of each other, and the difference is *what you know*:

- **Invariances** — transformations you *know* leave the label unchanged. A left-right flip of a cat is still a cat; a cropped, recolored, slightly-distorted image is the same class; speech with background noise added is the same words. So you **augment** the training set with these transforms (this is the data augmentation of Lecture 02, seen now through the shift lens): you are deliberately *widening the support* of `p` to cover variations you expect at test time. Classic roots: tangent distance (Simard 1995) and virtual support vectors (Schölkopf 1997) explored a point's neighborhood; today it's the standard ImageNet augmentation stack (random crop/scale/flip, hue–saturation–brightness jitter via libraries like imgaug / Albumentations).
- **Adversarial robustness** — transformations you do *not* know should preserve the label, and which in fact change the model's output. The defense is to treat them *as if* they were invariances: bake the worst case into the loss. An **adversarially-robust loss** takes the supremum over a family of transformations `Δ` (with a penalty `η(δ)` discouraging extreme distortions):

```text
L(x, y, f) = sup_{δ ∈ Δ}  η(δ) · l( f(x + δ), y )
```

At each training step you find the *worst* perturbation and train against it, so the model is forced to be correct not just on the data point but on a robust neighborhood around it. The clean summary: with **invariances** you *know* the transformation keeps the outcome unchanged and add it to be more robust; with **adversarial data** you *don't* know it should, observe that it changes the outcome, and defend by treating it as an invariance anyway.

---

</details>

## 5. 双样本检验 —— 如何确知发生了漂移

如果不知道漂移已经发生，纠偏就无从谈起。检测问题是一个经典的统计学问题：给定从 `p` 中抽取的 `X = {x₁, …, x_m}` 与从 `q` 中抽取的 `X' = {x'₁, …, x'_{m'}}`，**检验是否 `p = q`。** 三种工具，大致按“实际该用哪个”的顺序排列。

**（1）分类器双样本检验 —— 应当选用的那一个。** 这与 §3 是同一个技巧，只是被改用作假设检验：*如果你能训练出一个分类器，在留出数据上以高于随机的准确率把两个样本区分开，那么 `p ≠ q`。* 背后的数学：分类器目标 `E_p[log π(+1|x)] + E_q[log π(−1|x)]` 在 `π(+1|x) = p(x)/(p(x)+q(x))` 处取最小，把它代回得到 `2·H[(p+q)/2] − H[p] − H[q] + 2log 2`，由熵的凸性可知，该式*恰在 `p = q` 时*取最小。因此不可分性等价于分布相等。之所以推荐这个检验，是因为它在高维下可用、复用你已有的工具链，而且如果检验结果为阳性，*训练好的分类器还可兼作重要性权重估计器*。

**（2）最大均值差异（MMD）。** 找出在两个分布之间期望差距最大的那个函数：

```text
MMD(p, q) = sup_{f ∈ F}  ( E_p[f(x)] − E_q[f(x)] )      — if large, p ≠ q
```

对于核特征空间 `φ`（再生核希尔伯特空间，`k(x, x') = ⟨φ(x), φ(x')⟩`）中的线性函数，该上确界具有*闭式解*：即均值 embedding 之间的距离 `‖E_p[φ(x)] − E_q[φ(x)]‖`。实践上的最大好处是**不需要训练任何东西** —— 判别函数就是 `f(x') = E_p[k(x, x')] − E_q[k(x, x')]`，而在有限样本上，MMD² 只是各点对上 kernel 取值的一个简单求和：

```text
MMD² ∝ (1/m(m−1)) Σ_{i≠j} [ k(x_i, x_j) + k(x'_i, x'_j) − k(x_i, x'_j) − k(x'_i, x_j) ]
```

选一个 RBF kernel，算一下，就完成了 —— 一个易于生成、无需训练循环的判别函数。

**（3）Kolmogorov–Smirnov（KS）检验 —— 适合一维。** 把见证函数限制在有界全变差 `TV[f] ≤ 1` 内，MMD 就退化为两个**累积分布函数**之间的最大间距：

```text
sup_z | F_p(z) − F_q(z) |  =  ‖F_p − F_q‖_∞ ,    where  F_p(z) = ∫_{−∞}^{z} p(x) dx
```

CDF 之间的这个上确界距离正是 KS 统计量。对于**逐特征监控**，它是自然之选 —— 对线上流的每个标量特征与训练参考各跑一次 KS 检验，就能得到一个廉价、可解释的逐特征漂移告警。（对于高维联合漂移，回到分类器检验。）

三者回答的是同一个问题 —— *`X` 与 `X'` 是否来自同一分布？* —— 而 CS329P 的定位是：在你相信任何结论（或触发任何纠偏）之前，用它作为确认分布是否一致的**合理性检查**。

---


<details>
<summary>English original</summary>

**5. Two-sample tests — how to KNOW a shift happened**

Correction is moot if you don't know a shift occurred. The detection question is a classical statistics problem: given `X = {x₁, …, x_m}` drawn from `p` and `X' = {x'₁, …, x'_{m'}}` drawn from `q`, **test whether `p = q`.** Three tools, in rough order of "what to actually use."

**(1) Classifier two-sample test — the one to choose.** This is the same trick as §3, repurposed as a hypothesis test: *if you can train a classifier that tells the two samples apart with above-chance accuracy on held-out data, then `p ≠ q`.* The math underneath: the classifier objective `E_p[log π(+1|x)] + E_q[log π(−1|x)]` is minimized at `π(+1|x) = p(x)/(p(x)+q(x))`, and plugging that back in yields `2·H[(p+q)/2] − H[p] − H[q] + 2log 2`, which by convexity of entropy is minimized *exactly when `p = q`*. So inseparability is equivalent to equality of distributions. It's the recommended test because it works in high dimensions, uses tooling you already have, and the *trained classifier doubles as the importance-weight estimator* if the test comes back positive.

**(2) Maximum Mean Discrepancy (MMD).** Find the function with the largest gap in expectation between the two distributions:

```text
MMD(p, q) = sup_{f ∈ F}  ( E_p[f(x)] − E_q[f(x)] )      — if large, p ≠ q
```

For linear functions in a kernel feature space `φ` (a Reproducing Kernel Hilbert Space, `k(x, x') = ⟨φ(x), φ(x')⟩`), the supremum has a *closed form*: it's the distance between the mean embeddings, `‖E_p[φ(x)] − E_q[φ(x)]‖`. The big practical win is that **you don't have to train anything** — the discriminant is `f(x') = E_p[k(x, x')] − E_q[k(x, x')]`, and on finite samples MMD² is a simple sum of kernel evaluations over pairs of points:

```text
MMD² ∝ (1/m(m−1)) Σ_{i≠j} [ k(x_i, x_j) + k(x'_i, x'_j) − k(x_i, x'_j) − k(x'_i, x_j) ]
```

Pick an RBF kernel, evaluate, done — an easy-to-generate discriminator with no training loop.

**(3) Kolmogorov–Smirnov (KS) test — great for 1-D.** Restrict the witness function to bounded total variation `TV[f] ≤ 1`, and the MMD reduces to the largest gap between the two **cumulative distribution functions**:

```text
sup_z | F_p(z) − F_q(z) |  =  ‖F_p − F_q‖_∞ ,    where  F_p(z) = ∫_{−∞}^{z} p(x) dx
```

That sup-distance between CDFs is exactly the KS statistic. It's the natural choice for **monitoring one feature at a time** — run a KS test per scalar feature of your production stream against a training reference, and you get a cheap, interpretable per-feature drift alarm. (For high-dimensional joint shift, go back to the classifier test.)

All three answer the same question — *are `X` and `X'` from the same distribution?* — and CS329P's framing is that this is the **sanity check** you run to confirm distributions match before you trust any conclusion (or trigger any correction).

---

</details>

## 6. 标签偏移校正 — BBSE

现在看另一个可处理的情形：标签偏移，`q(x, y) = q(y)·p(x|y)`，其中类条件外观 `p(x|y)` 固定不变，只有类先验 `p(y) → q(y)` 发生了移动（疾病流行率上升；患病病人的症状不会变）。有两种场景。

**简单的情形 — 你已经知道 `q(y)`。** 那么校正一个已训练模型几乎不值一提。由贝叶斯法则，测试后验等于训练后验乘以先验比：

```text
q(y|x) ∝ p(y|x) · [ q(y) / p(y) ]      then renormalize over y
```

你在原始数据上训练了 `p(y|x)`；把每个类概率乘以 `β(y) = q(y)/p(y)`，重新归一化，就得到了校正后的预测。等价地，若要重新训练，用 `β(y_i)` 对每个样本重新加权。*"当你拥有 q(y) 时，修模型很容易。"*

**真实的情形 — 你没有来自测试分布的标签**，所以你不能直接统计出 `q(y)`。你手上有未标注的生产输入和一个已训练模型。驱动 **Black-Box Shift Estimation (BBSE)** 的关键洞见（Lipton et al., 2018）：因为 `p(x|y)` 不变，所以*模型在每个真实类上的预测分布，在 train 与 test 之间也不变*。所以要测量模型的行为，而不是那些不可观测的标签。定义混淆结构和测试预测分布：

```text
Confusion (per-class prediction dist. on train):  p(ŷ, y) = ∫ p(ŷ | x) p(x|y) p(y) dx
Predicted-label distribution observed on test:     q(ŷ) = ∫ p(ŷ|x) q(x) dx = Σ_y p(ŷ, y) · β_y
```

最后一个方程就是引擎：一个**线性系统** `q(ŷ) = C · β`，其中 `C` 是（可估计的）混淆矩阵，`q(ŷ)` 是（可观测的）向量，记录每个类在未标注测试集上被*预测*的频率。对它求解得到各类权重 `β`，进而得到 `q(y) = β · p(y)`。算法很短：

```text
BBSE — BLACK-BOX SHIFT ESTIMATION
  C = 0 ;  q = 0
  for each training point i:           # build confusion matrix
      C[:, y[i]]  +=  p(· | x[i])      # soft predictions, column = true class
  for each test point i (unlabeled):   # build predicted-label distribution
      q           +=  p(· | x'[i])
  β = C⁻¹ q                            # naive solve
  # Better: constrained least squares —
  minimize_β  ‖ q − C β ‖²   s.t.   β[y] ≥ 0  and  Σ_y β[y] p[y] = 1
  # Then deploy: q(y|x) ∝ p(y|x) · β[y], renormalized.
```


<details>
<summary>English original</summary>

**6. Label shift correction — BBSE**

Now the other tractable case: label shift, `q(x, y) = q(y)·p(x|y)`, where the class-conditional appearance `p(x|y)` is fixed and only the class prior `p(y) → q(y)` moved (disease prevalence jumps; the symptoms of a sick patient don't). Two scenarios.

**The easy case — you already know `q(y)`.** Then correcting a trained model is almost trivial. By Bayes' rule, the test posterior is the training posterior times the prior ratio:

```text
q(y|x) ∝ p(y|x) · [ q(y) / p(y) ]      then renormalize over y
```

You trained `p(y|x)` on the original data; multiply each class probability by `β(y) = q(y)/p(y)`, renormalize, and you have the corrected predictions. Equivalently, for retraining, reweight each example by `β(y_i)`. *"When you have q(y), fixing models is easy."*

**The real case — you do NOT have labels from the test distribution**, so you can't just count up `q(y)`. You have unlabeled production inputs and a trained model. The key insight powering **Black-Box Shift Estimation (BBSE)** (Lipton et al., 2018): because `p(x|y)` is unchanged, the *distribution of your model's predictions per true class is also unchanged* between train and test. So measure the model's behavior, not the unobservable labels. Define the confusion structure and the test prediction distribution:

```text
Confusion (per-class prediction dist. on train):  p(ŷ, y) = ∫ p(ŷ | x) p(x|y) p(y) dx
Predicted-label distribution observed on test:     q(ŷ) = ∫ p(ŷ|x) q(x) dx = Σ_y p(ŷ, y) · β_y
```

That last equation is the engine: a **linear system** `q(ŷ) = C · β` where `C` is the (estimable) confusion matrix and `q(ŷ)` is the (observable) vector of how often each class is *predicted* on the unlabeled test set. Solve it for the per-class weights `β`, which give you `q(y) = β · p(y)`. The algorithm is short:

```text
BBSE — BLACK-BOX SHIFT ESTIMATION
  C = 0 ;  q = 0
  for each training point i:           # build confusion matrix
      C[:, y[i]]  +=  p(· | x[i])      # soft predictions, column = true class
  for each test point i (unlabeled):   # build predicted-label distribution
      q           +=  p(· | x'[i])
  β = C⁻¹ q                            # naive solve
  # Better: constrained least squares —
  minimize_β  ‖ q − C β ‖²   s.t.   β[y] ≥ 0  and  Σ_y β[y] p[y] = 1
  # Then deploy: q(y|x) ∝ p(y|x) · β[y], renormalized.
```

</details>

应使用约束版本（非负权重、合法的归一化先验）——它不会返回无意义的负先验或未归一化先验。**为什么它可信：** BBSE 在模型设定错误下依然 *稳健* —— 即使模型的预测 `ŷ(x)` 本身是 *错的*，只要模型 **校准一致**，即它在 hold-out 集与测试集上犯 *同样* 的错误（其混淆结构稳定），该方法仍能恢复出正确的权重。不需要一个准确的模型；需要一个 *错误方式一致* 的模型。混淆矩阵与标签向量会集中（可用 matrix Bernstein 证明），且算法开销很低：类别数的三次方、样本量的线性 —— 因此可以轻松扩展到数千个样本和中等类别数。扩展方法可处理更难的场景：通过对矩匹配做 SGD 流式估计权重，以及对超大规模标签集使用特征/GAN 矩匹配（MMD）或训练集-vs-测试集得分分类器。

与 §3 的对比正是这套分类法价值的体现：**covariate shift 按 `β(x) = q(x)/p(x)` 重加权（逐 *输入*）；label shift 按 `β(y) = q(y)/p(y)` 重加权（逐 *类别*）。** 同一套重要性加权机制，作用于真正发生偏移的那个因子。

---


<details>
<summary>English original</summary>

The constrained version (non-negative weights, valid normalized prior) is the one to use — it can't return a nonsensical negative or unnormalized prior. **Why it's trustworthy:** BBSE is *robust under misspecification* — even if the model's predictions `ŷ(x)` are themselves *wrong*, the method still recovers the right weights as long as the model is **calibrated consistently**, i.e. it makes the *same* errors on the hold-out and test sets (its confusion structure is stable). You don't need an accurate model; you need a *consistently-erring* one. The confusion matrix and label vector concentrate (provable via matrix Bernstein), and the algorithm is cheap: cubic in the number of classes, linear in sample size — so it scales fine to thousands of examples and modest class counts. Extensions handle the harder regimes: streaming estimation of the weights via SGD on moment-matching, and feature/GAN moment-matching (MMD) or a train-vs-test score classifier for very large label sets.

The contrast with §3 is the whole point of the taxonomy paying off: **covariate shift reweights by `β(x) = q(x)/p(x)` (per *input*); label shift reweights by `β(y) = q(y)/p(y)` (per *class*).** Same importance-weighting machinery, applied to the factor that actually moved.

---

</details>

> **Hardware lens / production:** 分布偏移正是 MLOps 体现价值之处，其机制很具体：**对已部署模型做漂移监控，无非就是持续对其输入和输出跑双样本检验。** 训练时把特征分布快照下来作为参考；然后在实时推理服务流上，对每个标量特征跑逐特征 **KS test**（§5），对联合分布跑 **分类器双样本检验**，以捕获任何单个特征都揭示不了的相关性偏移 —— 两者任一越过阈值即告警。也要盯住 *预测* 分布`q(ŷ)`：那是 **BBSE**（§6）廉价的前半部分，也是标签偏移的第一个信号，因为你可以从无标注的生产流量中算出它，**零新标签**（真值标签通常来得很晚，或永远不来）。这直接对应 **MLOps Module 4B** —— 漂移检测是一条监控流水线，而不是一次性的审计：它有计算成本（那些 kernel 求和与逐特征检验在每个批上都要跑），它需要把参考统计量与模型一起做版本管理，并且它应当门控一个自动化响应 —— 呼叫人工、触发重要性重加权或新数据重训练，或者在严重情况下（有效样本量`m*`崩塌，§3）拒绝自动纠正，并要求新的有标注数据。与 Lecture 02 输入流水线的工程类比：正如你测量 samples/s 来发现饥饿的 GPU，你测量 *随时间的分布距离* 来发现饥饿的模型 —— 那个模型的训练分布已悄悄偏离了它如今所服务的世界。


<details>
<summary>English original</summary>

> **Hardware lens / production:** Distribution shift is where MLOps earns its keep, and the mechanism is concrete: **monitoring a deployed model for drift is just running two-sample tests on its inputs and outputs, continuously.** Snapshot the feature distribution at training time as a reference; then on the live serving stream run a per-feature **KS test** (§5) on each scalar feature and a **classifier two-sample test** on the joint to catch correlated shift no single feature reveals — alarm when either crosses threshold. Watch the *prediction* distribution `q(ŷ)` too: that's the cheap front-half of **BBSE** (§6) and the first sign of label shift, because you can compute it from unlabeled production traffic with **zero new labels** (ground-truth labels usually arrive late or never). This ties directly to **MLOps Module 4B** — drift detection is a monitoring pipeline, not a one-off audit: it has compute cost (those kernel sums and per-feature tests run on every batch), it needs the reference statistics versioned alongside the model, and it should gate an automated response — page a human, trigger importance-reweighted or fresh-data retraining, or in the severe case (effective sample size `m*` collapsed, §3) refuse to auto-correct and demand new labeled data. The engineering parallel to Lecture 02's input pipeline: just as you measure samples/s to catch a starved GPU, you measure *distributional distance over time* to catch a starved model — one whose training distribution has quietly diverged from the world it now serves.

</details>

> **2026 更新：** 漂移监控如今已是产品化基础设施，而不是你自己手工打造的东西。**Evidently**（开源）生成漂移仪表盘与测试套件 —— 逐特征统计检验（KS、PSI、Wasserstein、卡方）外加预测漂移与数据质量报告 —— 并且是表格流水线的常见默认选项。**NannyML** 专攻 §6 中那个艰难而有价值的场景：*在无标注生产数据上估计模型性能*，通过基于置信度与 BBSE 风格的方法，从而在真值标签到达之前就能得到一个准确率估计。**WhyLabs**（构建于开源 `whylogs` 性能剖析格式之上）为流式/生产遥测进行大规模的轻量级统计性能剖析，而无需将原始数据传出本机。云平台如今原生提供该能力 —— SageMaker Model Monitor、Vertex AI Model Monitoring、Azure ML 数据漂移监控器都会针对训练基线运行定时的双样本检验并发出告警。最新的前沿是 **大语言模型与 embedding 漂移**：输入是非结构化文本，因此监控转向了 **embedding 空间漂移**（对 embedding 分布做 MMD / 分类器检验 —— 正是学得特征空间中的 §5），外加 **评测漂移** —— 随着 prompt、用户行为以及上游模型自身的更新悄悄改变你脚下的分布，持续跟踪大语言模型评判器或任务准确率指标（§4 的垃圾信息军备竞赛以 prompt 注入与使用方式变化的形式重生）。CS329P 的心智模型 —— *命名漂移、对它做检验、重加权或重训练* —— 清晰地映射到上述每一个工具；它们正是这堂课的产品化版本。

---


<details>
<summary>English original</summary>

> **2026 update:** Drift monitoring is now productized infrastructure rather than something you hand-roll. **Evidently** (open-source) generates drift dashboards and test suites — per-feature statistical tests (KS, PSI, Wasserstein, chi-square) plus prediction-drift and data-quality reports — and is the common default for tabular pipelines. **NannyML** specializes in the hard, valuable case from §6: *estimating model performance on unlabeled production data* via confidence-based and BBSE-style methods, so you get an accuracy estimate before ground-truth labels land. **WhyLabs** (built on the open-source `whylogs` profiling format) does lightweight statistical profiling at scale for streaming/production telemetry without shipping raw data off-box. Cloud platforms ship it natively now — SageMaker Model Monitor, Vertex AI Model Monitoring, Azure ML data-drift monitors all run scheduled two-sample tests against a training baseline and emit alerts. The newest frontier is **LLM and embedding drift**: the inputs are unstructured text, so monitoring moved to **embedding-space drift** (MMD / classifier tests on embedding distributions — exactly §5 in a learned feature space), plus **eval drift** — tracking an LLM-judge or task-accuracy metric over time as prompts, user behavior, and an upstream model's own updates silently change the distribution under you (the spam-arms-race of §4 reborn as prompt-injection and changing usage). The CS329P mental model — *name the shift, test for it, reweight or retrain* — maps cleanly onto every one of these tools; they are productized versions of this exact lecture.

---

</details>

## 时效

写于 2026 年 6 月。**原始 CS329P 内容** —— IID 假设与泛化回顾、依据 `p(x, y)` 分解方式划分的 covariate/label/concept 分类体系、通过带权重裁剪与有效样本量的域分类器实现的重要性加权、对抗样本与不变性/鲁棒损失、三种双样本检验（classifier / MMD / KS），以及用于 label shift 的 BBSE —— 会最先讲授，因为它仍是正确的工作框架，并与 2021 年的讲义一一对应（原课程的 Lectures 6–7）。

**更新层**仅标出工具层面新增的内容：漂移监控的产品化（Evidently、WhyLabs、NannyML 以及云原生模型监控器）、作为标准能力的无标注数据性能估计，以及将这些同样的检验扩展到 embedding 空间与 LLM 评测漂移。

数学没有改变；它所监控的世界变大了。

*改编自 [Stanford CS329P](https://c.d2l.ai/stanford-cs329p) — Huang, Li & Smola, CC-BY-SA-4.0.*


<details>
<summary>English original</summary>

**Current as of**

Written June 2026. The **original CS329P content** — the IID assumption and generalization recap, the covariate/label/concept taxonomy keyed to how `p(x, y)` factors, importance weighting via a domain classifier with weight clipping and effective sample size, adversarial examples and invariances/robust loss, the three two-sample tests (classifier / MMD / KS), and BBSE for label shift — is taught first because it is still the correct working framework and maps one-to-one onto the 2021 slides (Lectures 6–7 of the original course). The **refresh layer** flags only what's new in tooling: the productization of drift monitoring (Evidently, WhyLabs, NannyML, and native cloud model monitors), unlabeled-data performance estimation as a standard capability, and the extension of these exact tests into embedding space and LLM-eval drift. The math is unchanged; the world it monitors got bigger.

*Adapted from [Stanford CS329P](https://c.d2l.ai/stanford-cs329p) — Huang, Li & Smola, CC-BY-SA-4.0.*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/4. ML Engineering and MLOps/Practical Machine Learning (CS329P)/Lecture-06.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/4.%20ML%20Engineering%20and%20MLOps/Practical%20Machine%20Learning%20%28CS329P%29/Lecture-06.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
