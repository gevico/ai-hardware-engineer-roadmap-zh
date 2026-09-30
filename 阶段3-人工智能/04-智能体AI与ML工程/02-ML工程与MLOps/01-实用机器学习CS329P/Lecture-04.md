---
title: 第 04 讲 - 模型验证与评估
description: 第 04 讲 - 模型验证与评估
---

# 第 04 讲 - 模型验证与评估

**合集：** [Practical Machine Learning (CS329P)](README.md) | **上一讲：** [← 第 03 讲](Lecture-03.md) | **下一讲：** [第 05 讲](Lecture-05.md)

---

你报告的关于模型的每一个数字，都是对它从未见过的数据许下的一个承诺。「94% 准确率」不是关于你的测试集的陈述；它是一个赌注，赌的是真实世界里接下来的一千个样本会像你测量过的那些一样表现。评估就是让这个赌注变得诚实的学问。本讲令人不安的真相是，*人们衡量模型质量的大多数方式都在对他们撒谎*——而且它们朝着乐观的方向撒谎，这是最糟糕的方向，因为虚高的分数恰恰是你不会去深究的那一个。你就把它上线了。

谎言有三大类，本讲按顺序逐一讨论。第一类是**错误的指标**：在一个 95% 的样本都是负例的问题上取得 94% 准确率，意味着你的模型还不如一个永远说「否」的常量。第二类是**在复杂度曲线上站错了位置**：一个在训练数据上表现完美、在新数据上却崩溃的模型是记住了，而不是学会了，训练分数什么也没告诉你。第三类，也是最致命的，因为它能逃过代码审查，是**被污染的估计**——泄漏，即来自未来或来自测试集的信息悄悄混进训练，而你的验证分数悄然不再衡量泛化。CS329P 直白的经验法则：*如果你的模型表现好得不像真的，那它几乎肯定就不是真的，而被污染的验证集正是头号原因。*

贯穿全讲的是一条你永远无法直接观测到的量——**泛化误差**，即在真实分布上的误差——而整个由指标、偏差-方差图景和留出集划分构成的体系，存在的意义就是在不欺骗自己的前提下*估计*它。把估计做对了，下游的一切（模型选择、超参数调优、是否上线的决策）都立于坚实之地。做错了，你优化的就是一个虚构。

---

## 学习目标

学完本讲，你应该能够：

1. **为分类、回归或排序问题选择合适的指标**，并准确解释为什么准确率在类别不平衡下会失效。
2. **读懂混淆矩阵**，并从中推导出精度、召回率、F1、ROC-AUC 和 PR-AUC——并刻意选择决策阈值，而不是默认用 0.5。
3. **根据训练误差与泛化误差的差距以及模型复杂度曲线，诊断欠拟合与过拟合**，并为每种情况给出正确的对策。
4. **设计训练/验证/测试划分**，秉持测试集恰好只用一次、超参数只在验证集上调优的纪律。
5. **应用 k 折交叉验证**，并识别出朴素 CV *错误*的情形——时间序列和分组数据。
6. **在经典的泄漏 bug 抬高你的分数之前识别它们**——在完整数据集上拟合 scaler、目标泄漏、以及跨越划分边界的近重复记录。

---

## 1. 评估指标——衡量正确的东西

你训练所用的损失（交叉熵、MSE）衡量的是模型拟合得有多好，但它很少是任何人真正关心的那个数字。我们用**多个指标**来评估模型，CS329P 把它们分成两类：

- **模型指标**衡量在样本上的表现：准确率、精度/召回率、F1、AUC。这些是你在留出集上计算的东西。
- **业务指标**衡量模型对产品的影响：营收、推理延迟、点击率。这些才是公司真正在优化的东西。

选择模型就像选车——你要同时权衡多个指标，验证准确率最高的那个并不自动就是你要上线的模型。记住这个想法；本节末尾会回到它。


<details>
<summary>English original</summary>

**Lecture 04 - Model Validation & Evaluation**

**Collection:** [Practical Machine Learning (CS329P)](README.md) | **Previous:** [← Lecture 03](Lecture-03.md) | **Next:** [Lecture 05](Lecture-05.md)

---

Every number you report about a model is a promise about data it has never seen. "94% accuracy" is not a statement about your test set; it is a bet that the next thousand examples from the real world will behave like the ones you measured on. Evaluation is the discipline of making that bet honest. The uncomfortable truth of this lecture is that *most of the ways people measure model quality lie to them* — and they lie in the optimistic direction, which is the worst direction, because an inflated score is the one you don't investigate. You ship it.

There are three families of lie, and this lecture takes them in order. The first is **the wrong metric**: 94% accuracy on a problem where 95% of examples are negative means your model is worse than a constant that always says "no." The second is **the wrong place on the complexity curve**: a model that nails the training data and falls apart on new data has memorized, not learned, and the training score told you nothing. The third, and the deadliest because it survives code review, is **a contaminated estimate** — leakage, where information from the future or from the test set sneaks into training and your validation score quietly stops measuring generalization at all. CS329P's blunt rule of thumb: *if your model's performance is too good to be true, it almost certainly is, and a contaminated validation set is the number-one reason.*

The throughline is a single quantity you can never directly observe — **generalization error**, the error on the true distribution — and the entire apparatus of metrics, the bias-variance picture, and held-out splits exists to *estimate* it without fooling yourself. Get the estimate right and everything downstream (model selection, hyperparameter tuning, the go/no-go ship decision) rests on solid ground. Get it wrong and you are optimizing a fiction.

---

**Learning objectives**

By the end of this lecture you should be able to:

1. **Choose the right metric** for a classification, regression, or ranking problem, and explain precisely why accuracy fails under class imbalance.
2. **Read a confusion matrix** and derive precision, recall, F1, ROC-AUC, and PR-AUC from it — and pick a decision threshold deliberately rather than defaulting to 0.5.
3. **Diagnose underfitting vs. overfitting** from the training-vs-generalization error gap and the model-complexity curve, and prescribe the right fix for each.
4. **Design a train/validation/test split** with the discipline that the test set is used exactly once and hyperparameters are tuned on validation only.
5. **Apply k-fold cross-validation** and recognize the cases where vanilla CV is *wrong* — time series and grouped data.
6. **Spot the classic leakage bugs** — fitting a scaler on the full dataset, target leakage, and near-duplicate records straddling the split — before they inflate your score.

---

**1. Evaluation metrics — measuring the right thing**

The loss you train on (cross-entropy, MSE) measures how well the model fits, but it is rarely the number anyone cares about. We evaluate models with **multiple metrics**, and CS329P sorts them into two buckets:

- **Model metrics** measure performance on examples: accuracy, precision/recall, F1, AUC. These are what you compute on a held-out set.
- **Business metrics** measure the model's impact on the product: revenue, inference latency, click-through rate. These are what the company actually optimizes.

Selecting a model is like choosing a car — you weigh several metrics at once, and the best validation accuracy is not automatically the model you ship. Hold that idea; we return to it at the end of the section.

</details>

### 1.1 分类指标

**准确率** = (正确预测数) / (总样本数)。它是所有人第一时间会伸手去拿的指标，也是第一个反噬他们的指标。其失效模式是**类别不平衡**。以欺诈检测为例，其中 99.8% 的交易是合法的。一个对*每一笔交易*都预测「非欺诈」的模型准确率达到 99.8%，却毫无价值——它抓到的欺诈为零，而这是系统存在的唯一目的。准确率被多数类主导，因此在任何偏斜问题（欺诈、疾病筛查、广告点击、缺陷检测）上，它都具有主动误导性。解决办法是分别度量各个类别。

**混淆矩阵**是其余一切所建立在其上的基础。对二分类，它把预测与真实值对照制表：

```text
                      Predicted Positive    Predicted Negative
Actual Positive       True Positive  (TP)   False Negative (FN)
Actual Negative       False Positive (FP)   True Negative  (TN)
```

由它的四个单元格，得到那些在类别不平衡下*不会*崩塌的指标：

- **精度** = TP / (TP + FP) —— 在你标记为正的全部样本中，真正为正的比例是多少？精度低 = 狼来了。*（注意模型未预测出任何正例时的除零。）*
- **召回率**（又称灵敏度、TPR）= TP / (TP + FN) = TP /（全部实际正例）—— 在真正为正的全部样本中，你抓住了多大比例？召回率低 = 漏掉真实病例。

精度与召回率相互权衡，而*哪一个重要是领域决策，不是数学决策。*癌症筛查看重召回率——漏掉的肿瘤（假阴性）是灾难性的，误报仅仅意味着再做一次随访扫描。垃圾邮件过滤器看重精度——漏掉的垃圾邮件（假阴性）只是小小的烦扰，但一封真实邮件被丢进垃圾文件夹（假阳性）会让你丢掉一份工作机会。你无法白白同时最大化两者。

**F1** 把两者压缩成一个数字——精度与召回率的**调和平均**：

```text
F1 = 2 · (precision · recall) / (precision + recall)
```

用调和平均（而非算术平均）是刻意的：它惩罚两者之间的不平衡。精度 1.0、召回率 0.0 的模型算术平均为 0.5，但 F1 为 0——F1 拒绝为一个轴出色而另一个轴失败的表现记功。当假阳性与假阴性代价不同时，用更一般的 **Fβ** 对它们加权（β > 1 偏向召回率，β < 1 偏向精度）。

**阈值选择。**分类器输出一个*分数*（一个类似概率的数 `o`），你把它与阈值 θ 比较，从而转化为标签：若 `o ≥ θ` 则预测为正，否则为负。默认的 θ = 0.5 是一种惯例，不是定律。降低 θ 能抓到更多正例（召回率 ↑），代价是更多误报（精度 ↓）；提高 θ 则相反。你上面看到的每一对（精度, 召回率）都是阈值扫出的*曲线上的一点*——而下面两个指标正是对这条曲线的概括。

**ROC-AUC** 衡量模型*在全部阈值上区分两个类别的能力*，与 θ 的任一具体取值无关。把 θ 从高到低扫过，画出 **ROC 曲线**——真阳性率对假阳性率：

```text
TPR = #(true positive predictions)  / #(positive examples)     ← y-axis (= recall)
FPR = #(false positive predictions) / #(negative examples)     ← x-axis
```

**该曲线下的面积（AUC）**落在 `[0.5, 1]`：0.5 是抛硬币（对角线，无区分能力），1.0 是完美区分。直观上，AUC 是模型给随机一个正例的分数高于随机一个负例的概率。它的长处——阈值无关性——也正是它的陷阱：**ROC-AUC 在严重不平衡的数据上偏乐观**，因为 FPR 的分母是巨大的负类规模，所以即便有数千个假阳性，也几乎推不动 x 轴。

**PR-AUC**（**精度-召回率**曲线下的面积）就是解决办法。它绘制跨阈值的精度对召回率，并完全忽略真阴性，因此在负例数量远超正例时仍保持诚实。**经验法则：类别平衡 → ROC-AUC 可以；正例稀少的问题（欺诈、检索、异常检测）→ 信任 PR-AUC。**


<details>
<summary>English original</summary>

**1.1 Classification metrics**

**Accuracy** = (correct predictions) / (total examples). It is the first metric everyone reaches for and the first one that betrays them. The failure mode is **class imbalance**. Consider fraud detection where 99.8% of transactions are legitimate. A model that predicts "not fraud" for *every single transaction* scores 99.8% accuracy and is completely worthless — it catches zero fraud, the only thing the system exists to do. Accuracy is dominated by the majority class, so on any skewed problem (fraud, disease screening, ad clicks, defect detection) it is actively misleading. The fix is to measure the classes separately.

**The confusion matrix** is the foundation everything else is built on. For binary classification it tabulates predictions against truth:

```text
                      Predicted Positive    Predicted Negative
Actual Positive       True Positive  (TP)   False Negative (FN)
Actual Negative       False Positive (FP)   True Negative  (TN)
```

From its four cells come the metrics that *don't* collapse under imbalance:

- **Precision** = TP / (TP + FP) — of everything you flagged positive, what fraction really was? Low precision = crying wolf. *(Watch for division by zero when the model predicts no positives.)*
- **Recall** (a.k.a. sensitivity, TPR) = TP / (TP + FN) = TP / (all actual positives) — of everything that truly was positive, what fraction did you catch? Low recall = missing real cases.

Precision and recall trade off, and *which one matters is a domain decision, not a math one.* Cancer screening prizes recall — a missed tumor (false negative) is catastrophic, a false alarm merely means a follow-up scan. A spam filter prizes precision — a missed spam (false negative) is a minor annoyance, but a real email dumped in the spam folder (false positive) loses you a job offer. You cannot maximize both for free.

**F1** collapses the two into one number — the **harmonic mean** of precision and recall:

```text
F1 = 2 · (precision · recall) / (precision + recall)
```

The harmonic mean (not the arithmetic) is deliberate: it punishes imbalance between the two. A model with precision 1.0 and recall 0.0 has arithmetic mean 0.5 but F1 of 0 — F1 refuses to give credit for being great on one axis while failing the other. When false positives and false negatives carry different costs, weight them with the general **Fβ** (β > 1 favors recall, β < 1 favors precision).

**Threshold choice.** A classifier outputs a *score* (a probability-like number `o`), and you turn it into a label by comparing to a threshold θ: predict positive if `o ≥ θ`, else negative. The default θ = 0.5 is a convention, not a law. Lowering θ catches more positives (recall ↑) at the cost of more false alarms (precision ↓); raising it does the reverse. Every (precision, recall) pair you saw above is *a single point on a curve* that the threshold sweeps out — which is exactly what the next two metrics summarize.

**ROC-AUC** measures how well the model *separates the two classes across all thresholds*, independent of any one choice of θ. Sweep θ from high to low and plot the **ROC curve** — True Positive Rate against False Positive Rate:

```text
TPR = #(true positive predictions)  / #(positive examples)     ← y-axis (= recall)
FPR = #(false positive predictions) / #(negative examples)     ← x-axis
```

The **area under this curve (AUC)** lands in `[0.5, 1]`: 0.5 is a coin flip (the diagonal, no separating power), 1.0 is perfect separation. Intuitively, AUC is the probability that the model scores a random positive higher than a random negative. Its strength — threshold-independence — is also its trap: **ROC-AUC is optimistic on heavily imbalanced data** because FPR has a huge negative-class denominator, so even thousands of false positives barely move the x-axis.

**PR-AUC** (area under the **Precision-Recall** curve) is the fix. It plots precision against recall across thresholds and ignores true negatives entirely, so it stays honest when negatives vastly outnumber positives. **Rule of thumb: balanced classes → ROC-AUC is fine; rare-positive problems (fraud, retrieval, anomaly detection) → trust PR-AUC.**

</details>

### 1.2 回归指标

当目标是连续值时，衡量的是残差的规模 `(yᵢ − ŷᵢ)`：

| 指标 | 公式 | 单位 | 行为 |
|---|---|---|---|
| **MSE**（均方误差） | `(1/n) Σ (yᵢ − ŷᵢ)²` | target² | 误差取平方 → 对大偏差惩罚极重；对离群点敏感 |
| **RMSE**（均方根误差） | `√MSE` | target | MSE 还原回 target 单位——可直接解读；同样对离群点敏感 |
| **MAE**（平均绝对误差） | `(1/n) Σ \|yᵢ − ŷᵢ\|` | target | 与误差呈线性 → 对离群点稳健；把 \$1M miss as 10× a \$100K 的偏差当作 100K，而不是 100 倍 |
| **R²**（决定系数） | `1 − SS_res / SS_tot` | 无量纲 | 被解释的方差占比；1.0 = 完美，0 = 不比预测均值更好，<0 = 比均值还差 |

**MSE 与 MAE** 的取舍对应 precision 与 recall 的取舍：核心在于你有多忌惮大误差。MSE/RMSE 对残差取平方，因此单个严重错误的预测就能主导分数——当大偏差的代价不成比例地高时选它们（并留意离群点别劫持训练）。MAE 对每一块钱的误差同等加权，对离群点无动于衷——当数据带有你不愿去追的重尾时选它。**R²** 是向非专业人士汇报时该用的那个，因为它归一化且无量纲：「模型解释了 85% 的方差」能跨越受众传播，而「RMSE = 41,000」做不到。

### 1.3 排序与业务指标

许多真实系统并非孤立地做分类——它们做**排序**。搜索、推荐、广告系统返回一个有序列表，关键在于好条目是否落在靠前位置。指标也随之改变：**Precision@k** 与 **Recall@k**（前 *k* 项的质量）、**MAP**（平均精度均值）、**NDCG**（归一化折损累计增益，它按排名位置对相关性做折损，因此位置 1 的优质结果胜过位置 10 的同一结果）。对于目标检测，对应的是 **mAP**（跨 IoU 阈值的平均精度均值）。

但最终决定一切的是**业务指标**，而 CS329P 的广告展示案例研究是说明模型指标与业务指标为何分道扬镳的经典一课。展示广告本质上是一个**二分类问题**：为每个候选广告估计点击率（CTR），然后按 `CTR × price` 排序展示靠前的广告。模型的首要指标是 **AUC**。而业务本身则由下式主导：

```text
revenue = #pageviews × ASN × CTR × ACP
          where  ASN = avg #ads shown per page
                 CTR = actual user click-through rate
                 ACP = avg price advertiser pays per click
```

这里有个迟早会绊倒每个 ML 团队的陷阱：**AUC 更高的新模型可能*损害*营收。** 原因可能来自讲义原文——校准更好的模型估计出的 CTR *更低*，于是系统展示更少的广告（ASN ↓）；真实世界的 CTR 低于离线数字，因为你训练和评测所用的*过去*数据已不再反映当前行为；或者排序变化压低了成交价格。离线指标改善了，而你真正在意的东西变差了。**唯一能弄清的办法是在线实验**——把模型部署到真实流量的一小片上（A/B 测试），直接测量业务指标。离线 AUC 提议，在线营收裁决。

### 指标选择速查表

| 问题类型 | 默认指标 | 以下情况改用…… | 避免 |
|---|---|---|---|
| **均衡分类** | Accuracy、ROC-AUC | — | — |
| **不均衡分类** | F1、PR-AUC | recall 不可妥协（筛检）→ recall@fixed-precision | **Accuracy**（多数类陷阱） |
| **概率校准重要** | Log loss、Brier score | 下游要用到概率（CTR × price） | 阈值化后的 accuracy |
| **回归** | RMSE | 重尾目标 / 存在离群点 → **MAE**；跨受众汇报 → **R²** | 单用 MSE（单位不可解读） |
| **排序 / 检索 / 推荐** | NDCG、MAP | 只有最顶端重要 → Precision@k | accuracy |
| **真正养活业务的那个** | **通过在线 A/B 测试得到的业务指标** | 永远，在上线之前 | 只信任离线指标 |

---


<details>
<summary>English original</summary>

**1.2 Regression metrics**

When the target is continuous, you measure the size of the residuals `(yᵢ − ŷᵢ)`:

| Metric | Formula | Units | Behavior |
|---|---|---|---|
| **MSE** (mean squared error) | `(1/n) Σ (yᵢ − ŷᵢ)²` | target² | Squares errors → punishes large misses hard; outlier-sensitive |
| **RMSE** (root MSE) | `√MSE` | target | MSE back in the target's units — directly interpretable; still outlier-sensitive |
| **MAE** (mean absolute error) | `(1/n) Σ \|yᵢ − ŷᵢ\|` | target | Linear in error → robust to outliers; treats a \$1M miss as 10× a \$100K miss, not 100× |
| **R²** (coefficient of determination) | `1 − SS_res / SS_tot` | unitless | Fraction of variance explained; 1.0 = perfect, 0 = no better than predicting the mean, <0 = worse than the mean |

The **MSE-vs-MAE** choice mirrors precision-vs-recall: it is about how much you fear large errors. MSE/RMSE square the residual, so a single wildly-wrong prediction dominates the score — choose them when big misses are disproportionately bad (and watch that outliers don't hijack training). MAE weights every dollar of error equally and shrugs off outliers — choose it when the data has a heavy tail you don't want to chase. **R²** is the one to report to non-specialists because it's normalized and unit-free: "the model explains 85% of the variance" travels across audiences in a way "RMSE = 41,000" does not.

**1.3 Ranking and business metrics**

Many real systems don't classify in isolation — they **rank**. Search, recommendation, and ad systems return an ordered list, and what matters is whether the good items land near the top. The metrics shift accordingly: **Precision@k** and **Recall@k** (quality of the top *k*), **MAP** (mean average precision), **NDCG** (normalized discounted cumulative gain, which discounts relevance by rank position so a great result at position 1 beats the same result at position 10). For object detection the analog is **mAP** (mean average precision across IoU thresholds).

But the metric that ultimately decides everything is the **business metric**, and CS329P's ad-display case study is the canonical lesson in why model metrics and business metrics diverge. Displaying ads is, at its core, a **binary classification problem**: estimate the click-through rate (CTR) for each candidate ad, then show the top ads ranked by `CTR × price`. The headline model metric is **AUC**. The business, meanwhile, is governed by:

```text
revenue = #pageviews × ASN × CTR × ACP
          where  ASN = avg #ads shown per page
                 CTR = actual user click-through rate
                 ACP = avg price advertiser pays per click
```

Here is the trap that catches every ML team eventually: **a new model with higher AUC can *hurt* revenue.** Possible reasons, straight from the slides — the better-calibrated model estimates *lower* CTRs, so the system displays fewer ads (ASN ↓); the real-world CTR comes in below the offline number because you trained and evaluated on *past* data that no longer reflects current behavior; or the ranking shift lowers realized prices. The offline metric improved and the thing you actually care about got worse. **The only way to know is an online experiment** — deploy the model to a slice of real traffic (an A/B test) and measure the business metrics directly. Offline AUC proposes; online revenue disposes.

**Metric-selection cheat sheet**

| Problem type | Default metric | Reach for instead when… | Avoid |
|---|---|---|---|
| **Balanced classification** | Accuracy, ROC-AUC | — | — |
| **Imbalanced classification** | F1, PR-AUC | recall is non-negotiable (screening) → recall@fixed-precision | **Accuracy** (majority-class trap) |
| **Probability calibration matters** | Log loss, Brier score | downstream uses the probability (CTR × price) | thresholded accuracy |
| **Regression** | RMSE | heavy-tailed targets / outliers present → **MAE**; cross-audience reporting → **R²** | MSE alone (uninterpretable units) |
| **Ranking / retrieval / recsys** | NDCG, MAP | only the very top matters → Precision@k | accuracy |
| **The thing that pays the bills** | **Business metric via online A/B test** | always, before shipping | trusting offline metrics alone |

---

</details>

## 2. 欠拟合 vs. 过拟合 —— 落在复杂度曲线上

CS329P 用一个寓言引出这个话题。一位放贷人请你预测谁会还贷。你有 100 位申请人；其中 5 位违约。你建了一个模型，发现一个「令人惊讶」的信号 —— **5 位违约者面试时都穿了蓝色衬衫**，于是你的模型死死倚赖这个信号。它在这 100 个人身上会得分漂亮，到了下一位申请人身上则一文不值，因为衬衫颜色只是噪声，恰巧在一个极小的样本里表现出相关性。这就是一句话概括的**过拟合**：学到的是训练集的怪癖，而非世界的结构。

要把它说精确，先定义两种误差：

- **训练误差** —— 模型在训练所用数据上的误差。
- **泛化误差** —— 模型在*新的、未见过的*数据上的误差。只有它才重要；也只有它你无法直接看到。

两者的关系可用于诊断模型：

| | 训练误差**低** | 训练误差**高** |
|---|---|---|
| **泛化误差低** | **好** —— 目标 | （不可能 —— 有 bug；泛化不可能好过拟合） |
| **泛化误差高** | **过拟合** —— 记住了训练集 | **欠拟合** —— 太弱，连训练集都拟合不了 |

两者之间的差距就是线索。**欠拟合**：两种误差都高且*接近* —— 模型太简单，连它见过的数据里的模式都抓不住（想想用一条直线去穿一条曲线）。**过拟合**：训练误差低，但泛化误差高，且*差距很大* —— 模型把训练数据连同其中的噪声一起拟合了，而这种噪声不会重现。

### 模型复杂度曲线

把误差对**模型复杂度**作图 —— 模型复杂度是某个函数类拟合数据的能力，大致就是可学习参数的数目以及它们能取值的范围（更严格地说，是 **VC 维**：模型能打散的最大点集）。这张图是整个话题的核心图示：

```text
 error
   ^
   |  \                                              /   <- generalization error
   |   \  underfitting                              /        (U-shaped: high when too
   |    \  (both high)                             /          simple AND too complex)
   |     \                                        /
   |      \___                              _____/   <- overfitting
   |          \___                    _____/             (gap opens up)
   |              \___           ____/
   |                  \_________/  <-- training error (falls monotonically:
   |                      ^               more capacity always fits train better)
   |                      |
   |              optimal complexity (minimize GENERALIZATION error, not training)
   +----------------------|------------------------------> model complexity
```

训练误差单调下降 —— 给模型更多容量，它总能更好地拟合训练集，一直好到把它完全记住。泛化误差呈 **U 形**：随着模型获得足够容量去捕捉真实结构，它先下降，在**最优复杂度**处触底，随后*上升*，因为多出来的容量被花在拟合噪声上。你要的是 U 的底部，而找到它的办法是盯住泛化（验证）误差，绝不是训练误差。CS329P 的具体演示：在房屋销售数据上用一个 scikit-learn `DecisionTreeRegressor(max_depth=n)` —— `max_depth=2` 欠拟合（树太浅，表达不了价格曲面），较大的 `max_depth` 过拟合（几乎每套房一个叶子），合适的深度介于两者之间。


<details>
<summary>English original</summary>

**2. Underfitting vs. overfitting — landing on the complexity curve**

CS329P opens this topic with a parable. A lender asks you to predict who will repay their loans. You have 100 applicants; 5 defaulted. You build a model and discover a "surprising" signal — **all 5 who defaulted wore blue shirts to their interviews**, and your model leans hard on it. It will score beautifully on these 100 people and be worthless on the next applicant, because shirt color is noise that happened to correlate in a tiny sample. That is **overfitting** in one sentence: learning the quirks of the training set instead of the structure of the world.

To make this precise, define two errors:

- **Training error** — the model's error on the data it was trained on.
- **Generalization error** — the model's error on *new, unseen* data. This is the only one that matters; the only one you can't directly see.

The relationship between them diagnoses the model:

| | Training error **low** | Training error **high** |
|---|---|---|
| **Generalization error low** | **Good** — the goal | (impossible — a bug; you can't generalize better than you fit) |
| **Generalization error high** | **Overfitting** — memorized the training set | **Underfitting** — too weak to fit even the training set |

The gap between the two is the tell. **Underfitting**: both errors are high and *close* — the model is too simple to capture the pattern even in data it has seen (think a straight line through a curve). **Overfitting**: training error is low but generalization error is high and the *gap is wide* — the model fit the training data including its noise, and that noise doesn't recur.

**The model-complexity curve**

Plot error against **model complexity** — the capacity of a function class to fit data, roughly the number of learnable parameters and the range of values they can take (more rigorously, **VC dimension**: the largest set of points the model can shatter). The picture is the central diagram of the whole topic:

```text
 error
   ^
   |  \                                              /   <- generalization error
   |   \  underfitting                              /        (U-shaped: high when too
   |    \  (both high)                             /          simple AND too complex)
   |     \                                        /
   |      \___                              _____/   <- overfitting
   |          \___                    _____/             (gap opens up)
   |              \___           ____/
   |                  \_________/  <-- training error (falls monotonically:
   |                      ^               more capacity always fits train better)
   |                      |
   |              optimal complexity (minimize GENERALIZATION error, not training)
   +----------------------|------------------------------> model complexity
```

Training error falls monotonically — give a model more capacity and it will always fit the training set better, all the way to memorizing it. Generalization error is **U-shaped**: it falls as the model gains enough capacity to capture real structure, bottoms out at the **optimal complexity**, then *rises* as extra capacity is spent fitting noise. You want the bottom of the U, and you find it by watching generalization (validation) error, never training error. CS329P's concrete demo: a scikit-learn `DecisionTreeRegressor(max_depth=n)` on house-sales data — `max_depth=2` underfits (the tree is too shallow to express the price surface), a large `max_depth` overfits (a leaf for nearly every house), and the right depth sits in between.

</details>

### 不只是模型——数据复杂度同样重要

“最优复杂度”并非模型自身的属性；它还取决于**数据**。数据复杂度随样本数量、每个样本的特征数量以及类别可分性的增加而上升（严格版本是 **Kolmogorov complexity**——若一个短程序能生成它，数据就是简单的）。二者相互作用：

| | 数据复杂度 **低** | 数据复杂度 **高** |
|---|---|---|
| **模型复杂度低** | 正常（匹配） | **欠拟合**——模型对丰富数据太弱 |
| **模型复杂度高** | **过拟合**——模型对单薄数据过于复杂 | 正常（匹配） |

对角线才是理想位置：**让模型复杂度与数据复杂度匹配。** 在 200 行数据上跑深度神经网络会过拟合；在 2 亿个丰富样本上跑线性模型则会欠拟合，并白白损失准确率。这也是“复杂模型需要更多数据”的原因——只有当*数据量足够*、能约束所有这些参数时，它们的泛化误差才会低于简单模型；低于该交叉点，简单模型胜出。

这张图背后的理论是**泛化误差界**（非正式地说）：未见数据误差与训练误差之间的差距随 VC-dimension `D` 增大而扩大，并随训练样本 `N` 增多而缩小——容量越大，潜在差距越大；数据越多，差距越小。关键在于，泛化还取决于**训练算法**，而不仅仅是模型：加入**正则化**会惩罚复杂模型并将其拉回最优处，而用**随机梯度方法**训练的模型，其泛化表现往往比仅依据该界所能预期的更好。

### 诊断与修复

| 症状 | 诊断 | 对策 |
|---|---|---|
| 训练误差**高**，验证误差**高**（小差距） | **欠拟合**——模型太简单 / 数据太丰富 | **增加容量**：更大的模型、更多/更好的特征、更多特征交叉、训练更久、*减少*正则化 |
| 训练误差**低**，验证误差**高**（大差距） | **过拟合**——模型在记忆噪声 | **约束它**：更多训练数据、正则化（L2、dropout）、早停、数据增强、*减少*容量 / 特征数量 |
| 两种误差都低且接近 | **拟合良好**——你正处于 U 形曲线底部 | 发布它（在 §3 的验证纪律之后） |

这两种处方几乎互为镜像，这就是为什么诊断你*究竟*遇到哪个问题——通过读训练-验证差距——才是关键所在。新手最常见的错误是：对过拟合问题扔一个更大的模型（让它更糟），或在模型欠拟合时堆正则化（让*那个*问题更糟）。先读差距。

---

## 3. 模型验证——在不自欺的情况下估计泛化

你无法观测泛化误差，所以**用留出测试集来近似它**——模型从未见过、且*只能被恰好使用一次*的数据。CS329P 的类比很犀利：它是你的期中考试成绩（看过题目后就不能重考），是一笔待售房屋的最终成交价，是 Kaggle 竞赛中的私有排行榜数据。一旦你根据测试集做出决策——调整超参数、选择模型——它就不再是测试集，而成为训练的一部分。它的一次性本质正是全部意义所在。

那么，如何在不消耗测试集的情况下在开发过程中做决策？从训练数据中留出一个**验证集**：

- 数据的一个子集，**不用于训练**，你可以*多次*使用——用于模型选择和超参数调优。
- 它的抽取应使其**接近测试分布**（以及真实世界），这样在它上面表现好，才能预示在测试集上表现好。
- 一个值得记住的术语雷区：在日常 ML 用法中，“**测试误差**”几乎总是指*验证*集上的误差。真正的测试集是不可触碰的期末考试。

### 三分纪律

```text
┌─────────────────────────┬──────────────┬──────────────┐
│         TRAIN           │  VALIDATION  │     TEST      │
│  fit model parameters   │  tune hyper- │  touch ONCE,  │
│  (weights, splits)      │  params,     │  final number │
│                         │  pick model  │  you report   │
└─────────────────────────┴──────────────┴──────────────┘
   used many times          used many       used exactly
                            times            once
```


保护这一估计的规则是：**在训练集上拟合参数，在验证集上选择超参数，在测试集上报告一次。** 划分通常是用于验证的随机 `n%`——典型的 `n` 是 10–50——但*如何*划分才是容易出错的地方，我们将会看到。


<details>
<summary>English original</summary>

**It's not just the model — data complexity matters too**

"Optimal complexity" is not a property of the model alone; it depends on the **data**. Data complexity rises with the number of examples, the number of features per example, and the separability of the classes (the rigorous version is **Kolmogorov complexity** — data is simple if a short program can generate it). The two interact:

| | Data complexity **low** | Data complexity **high** |
|---|---|---|
| **Model complexity low** | Normal (matched) | **Underfitting** — model too weak for rich data |
| **Model complexity high** | **Overfitting** — model too rich for thin data | Normal (matched) |

The diagonal is where you want to be: **match model complexity to data complexity.** A deep neural network on 200 rows overfits; a linear model on 200 million rich examples underfits and leaves accuracy on the table. This is also why "complex models need more data" — their generalization error only beats a simple model's *once enough data is available* to constrain all those parameters; below that crossover point the simple model wins.

The theory behind the picture is the **generalization-error bound** (informally): the gap between unseen-data error and training error grows with VC-dimension `D` and shrinks as training examples `N` grow — more capacity widens the potential gap, more data narrows it. Crucially, generalization also depends on the **training algorithm**, not just the model: adding **regularization** penalizes complex models and pulls them back toward the optimum, and models trained with **stochastic gradient methods** tend to generalize better than the bound alone would suggest.

**Diagnosing and fixing**

| Symptom | Diagnosis | What to do |
|---|---|---|
| Train error **high**, val error **high** (small gap) | **Underfitting** — model too simple / data too rich | **Increase capacity**: bigger model, more/better features, more feature crosses, train longer, *reduce* regularization |
| Train error **low**, val error **high** (large gap) | **Overfitting** — model memorizing noise | **Constrain it**: more training data, regularization (L2, dropout), early stopping, data augmentation, *reduce* capacity / feature count |
| Both errors low and close | **Good fit** — you're at the bottom of the U | Ship it (after §3's validation discipline) |

The two prescriptions are near-mirror images, which is why diagnosing *which* problem you have — by reading the train-vs-val gap — is the whole game. The most common rookie error is throwing a bigger model at an overfitting problem (making it worse) or piling on regularization when the model is underfitting (making *that* worse). Read the gap first.

---

**3. Model validation — estimating generalization without fooling yourself**

You can't observe generalization error, so you **approximate it with a holdout test set** — data the model has never seen and that *can be used exactly once*. CS329P's analogies are sharp: it's your midterm exam score (you don't get to retake it after seeing the questions), the final price of a pending house sale, the private-leaderboard data in a Kaggle competition. The instant you make a decision based on the test set — tweak a hyperparameter, pick a model — it stops being a test set and becomes part of training. Its one-shot nature is the entire point.

So how do you make decisions during development without burning your test set? You hold out a **validation set** from the training data:

- A subset of the data, **not used for training**, that you *can* use many times — for model selection and hyperparameter tuning.
- It should be drawn to be **close to the test distribution** (and the real world), so that doing well on it predicts doing well on test.
- A terminology landmine worth memorizing: in casual ML usage "**test error**" almost always means error on the *validation* set. The true test set is the untouchable final exam.

**The three-way discipline**

```text
┌─────────────────────────┬──────────────┬──────────────┐
│         TRAIN           │  VALIDATION  │     TEST      │
│  fit model parameters   │  tune hyper- │  touch ONCE,  │
│  (weights, splits)      │  params,     │  final number │
│                         │  pick model  │  you report   │
└─────────────────────────┴──────────────┴──────────────┘
   used many times          used many       used exactly
                            times            once
```

The rule that protects the estimate: **fit parameters on train, choose hyperparameters on validation, report on test once.** Splits are often a random `n%` for validation — typical `n` is 10–50 — but *how* you split is where it goes wrong, as we'll see.

</details>

### k 折交叉验证

当没有足够数据留出一个较大的验证集时，**k 折交叉验证** 会复用它。算法：

```text
Partition the training data into K equal folds.
For i = 1 … K:
    train on the K−1 folds that aren't fold i
    validate on fold i  →  record validation error_i
Report the average validation error over all K rounds.
```

```text
            ┌──────┬──────┬──────┬──────┬──────┐
   Fold 1:  │ VALID│ train│ train│ train│ train│
   Fold 2:  │ train│ VALID│ train│ train│ train│
   Fold 3:  │ train│ train│ VALID│ train│ train│   →  error = mean(error_1..error_5)
   Fold 4:  │ train│ train│ train│ VALID│ train│
   Fold 5:  │ train│ train│ train│ train│ VALID│
            └──────┴──────┴──────┴──────┴──────┘
```

每个样本都恰好充当一次验证，因此你能以训练 K 次为代价，免费得到方差更低的泛化误差估计，以及一个误差条（各折之间的波动）。**常用选择是 K = 5 或 10。**

### 交叉验证在何时是 *错误* 的

普通随机划分（以及普通 k 折）假设样本是 **i.i.d.**——独立同分布。当该假设不成立时，随机划分会 **低估** 泛化误差，因为信息会在划分之间泄漏。两种情形占主导，外加一个采样修正：

- **序列 / 时间序列数据**（房屋销售、股票价格）。验证集必须 **在时间上与训练集不重叠**。如果 3 月的销售在训练集中，而同一街区 2 月的销售在验证集中，那就是用未来预测过去——在生产环境中不会有这种奢侈。修正方法是 **前向链式**：始终用过去训练、用未来验证，并向前扩展窗口。CS329P 的房屋销售案例研究把这一点讲得很具体——对同一数据做 **随机 vs. 顺序** 划分会改变结论：随机划分会使更深的树显得更好（最佳 `max_depth ≈ 13`），而诚实的顺序划分更偏好更浅的树（最佳 `max_depth ≈ 6`）。随机划分让模型利用了时间泄漏，并且 *看起来比实际更好。*

```text
 forward chaining (time series): never let train see anything after valid
   fold 1:  [== train ==][valid]
   fold 2:  [===== train =====][valid]
   fold 3:  [======== train ========][valid]
            └──────────── time ────────────►
```

- **簇状 / 分组数据** — 同一个人的照片、同一个视频的片段、同一个患者的多项实验室结果。簇内样本彼此相关，因此如果某个视频的一些片段落在训练集中，另一些落在验证集中，模型识别的是 *视频*，而不是动作。**划分整个簇，而不是单个样本** — 这就是 **group k-fold**，其中共享同一 group ID 的每条记录都留在划分的同一侧。
- **高度不平衡的类别** — 在形成划分时 **从少数类中多采样**（分层），以免某个稀有类别碰巧完全不出现在验证集中。


<details>
<summary>English original</summary>

**k-fold cross-validation**

When you don't have enough data to spare a fat validation set, **k-fold cross-validation** recycles it. The algorithm:

```text
Partition the training data into K equal folds.
For i = 1 … K:
    train on the K−1 folds that aren't fold i
    validate on fold i  →  record validation error_i
Report the average validation error over all K rounds.
```

```text
            ┌──────┬──────┬──────┬──────┬──────┐
   Fold 1:  │ VALID│ train│ train│ train│ train│
   Fold 2:  │ train│ VALID│ train│ train│ train│
   Fold 3:  │ train│ train│ VALID│ train│ train│   →  error = mean(error_1..error_5)
   Fold 4:  │ train│ train│ train│ VALID│ train│
   Fold 5:  │ train│ train│ train│ train│ VALID│
            └──────┴──────┴──────┴──────┴──────┘
```

Every example serves as validation exactly once, so you get a lower-variance estimate of generalization error and an error bar (the spread across folds) for free, at the cost of training K times. **Popular choices are K = 5 or 10.**

**When cross-validation is *wrong***

Vanilla random splitting (and vanilla k-fold) assumes examples are **i.i.d.** — independent and identically distributed. When that assumption breaks, random splitting **underestimates** generalization error, because information leaks across the split. Two cases dominate, plus a sampling fix:

- **Sequential / time-series data** (house sales, stock prices). The validation set must **not overlap with training in time**. If a March sale is in train and a February sale from the same neighborhood is in validation, you're using the future to predict the past — a luxury you won't have in production. The fix is **forward chaining**: always train on the past and validate on the future, growing the window forward. CS329P's house-sales case study makes this concrete — splitting the same data **randomly vs. sequentially** changes the picture: the random split flatters a deeper tree (best `max_depth ≈ 13`) while the honest sequential split prefers a shallower one (best `max_depth ≈ 6`). Random splitting let the model exploit temporal leakage and *looked better than it was.*

```text
 forward chaining (time series): never let train see anything after valid
   fold 1:  [== train ==][valid]
   fold 2:  [===== train =====][valid]
   fold 3:  [======== train ========][valid]
            └──────────── time ────────────►
```

- **Clustered / grouped data** — photos of the same person, clips from the same video, multiple lab results from the same patient. Examples within a cluster are correlated, so if some clips of a video land in train and others in validation, the model recognizes the *video*, not the action. **Split whole clusters, not individual examples** — this is **group k-fold**, where every record sharing a group ID stays on the same side of the split.
- **Highly imbalanced classes** — **sample more from the minority class** when forming splits (stratify) so a rare class isn't entirely absent from validation by chance.

</details>

### 数据泄漏名人堂

CS329P 最直白的一张幻灯片：**如果模型的表现好得不像真的，那很可能存在 bug，而验证集被污染是头号原因。** 数据泄漏之所以能躲过代码评审，是因为代码本身是*正确*的——错的是数据流。几个经典的坑：

- **在全量数据集上拟合 scaler（或任何预处理）。** 这是最隐蔽、也最常见的一种。你在全部数据上调用 `StandardScaler().fit(X)`，*然后*才做划分——此时训练数据的归一化统计量是用验证/测试数据的均值和方差算出来的。测试信息已经泄漏进训练。修法是：每个变换只在**训练折上**拟合，再 `transform` 验证集和测试集。（这就是第 02 讲的归一化警告，如今已是一条硬性规则。）

```python
from sklearn.preprocessing import StandardScaler
from sklearn.model_selection import train_test_split

X_tr, X_val, y_tr, y_val = train_test_split(X, y, test_size=0.2, random_state=0)

scaler = StandardScaler().fit(X_tr)    # fit on TRAIN ONLY
X_tr_s  = scaler.transform(X_tr)
X_val_s = scaler.transform(X_val)      # apply the same stats to val — no leakage

# WRONG, and silently inflates your score:
#   X_all_s = StandardScaler().fit_transform(X)   # learns stats from val+test too
#   then split  →  val statistics have bled into the scaler
```

而 `Pipeline` 能让这件事自动完成——它会在每个 CV 折内部重新拟合每一步，这正是为什么应当把预处理包进其中，而不是提前做变换。


<details>
<summary>English original</summary>

**The leakage hall of fame**

CS329P's bluntest slide: **if your model's performance is too good to be true, there is very likely a bug, and a contaminated validation set is the #1 reason.** Leakage is the failure that survives code review because the code is *correct* — it's the data flow that's wrong. The classic bugs:

- **Fitting the scaler (or any preprocessing) on the full dataset.** This is the subtlest and most common. You call `StandardScaler().fit(X)` on all the data, *then* split — and now the training data's normalization statistics were computed using the validation/test data's mean and variance. Test information has leaked into training. The fix: fit every transform on the **training fold only**, then `transform` validation and test. (This is the Lecture-02 normalization warning, now a load-bearing rule.)

```python
from sklearn.preprocessing import StandardScaler
from sklearn.model_selection import train_test_split

X_tr, X_val, y_tr, y_val = train_test_split(X, y, test_size=0.2, random_state=0)

scaler = StandardScaler().fit(X_tr)    # fit on TRAIN ONLY
X_tr_s  = scaler.transform(X_tr)
X_val_s = scaler.transform(X_val)      # apply the same stats to val — no leakage

# WRONG, and silently inflates your score:
#   X_all_s = StandardScaler().fit_transform(X)   # learns stats from val+test too
#   then split  →  val statistics have bled into the scaler
```

A `Pipeline` makes this automatic — it re-fits every step inside each CV fold, which is exactly why you should wrap preprocessing in one rather than transforming up front.

</details>

- **目标泄漏** — 编码了答案的特征。若手续费只在违约*之后*才收取，那么一个预测“贷款违约”的 `was_charged_late_fee` 列就是泄漏；一个预测用户流失的 `account_closed_date` 也是；用结果发生后信息算出来的特征同样是。它会给出惊人的验证分数，却在上线后彻底失败，因为在预测时该泄漏特征尚不存在。审查方法：对每个特征都问一句——*“在需要预测的那一刻，我手上真的会有这个值吗？”*
- **跨越切分边界的重复／近似重复记录。** 在合并数据集时很常见——例如，你从搜索引擎抓取图像来评测一个在 ImageNet 上训练过的模型，其中一些正是完全相同的图像。那条“测试”样本其实是模型已经记住的样本，分数纯属虚构。在切分**之前**就去重（包括*近似*重复——缩放、重压缩、轻微裁剪过的副本）。
- **过度复用验证集就是作弊。** 对着同一个验证集调几百次超参数，你就会慢慢*对验证集*过拟合——你的“验证误差”不再跟得住测试集。嵌套 CV，或一个最终从未触碰的测试集，就是防线。

总结：测试集只能花一次；从训练中留出的验证集用来估计它，并可用于选择——但前提是它的抽取方式要与测试分布相似，且保持干净。不当的验证集是**高估**模型性能最常见的原因，而会被上线的恰恰是那些高估的结果。

---


<details>
<summary>English original</summary>

- **Target leakage** — a feature that encodes the answer. A `was_charged_late_fee` column predicting "loan defaulted" is leakage if the fee is only assessed *after* default; an `account_closed_date` predicting churn; a feature computed using post-outcome information. It produces a spectacular validation score and total failure in production, because at prediction time the leaking feature doesn't exist yet. Audit: for every feature, ask *"would I actually have this value at the moment I need to predict?"*
- **Duplicate / near-duplicate records straddling the split.** Common when you merge datasets — e.g. you scrape images from a search engine to evaluate a model trained on ImageNet, and some are the very same images. The "test" example is one the model already memorized, so the score is fiction. De-duplicate (including *near*-duplicates — resized, recompressed, lightly cropped copies) **before** splitting.
- **Excessive validation reuse is cheating.** Tune hyperparameters against the same validation set hundreds of times and you slowly overfit *to the validation set* — your "validation error" stops tracking the test set. Nested CV, or a final untouched test set, is the guard.

The summary: the test set is spent once; a validation set held out from training estimates it and may be reused for selection — but only if it's drawn to resemble the test distribution and kept clean. An improper validation set is the most common path to **over-estimating** model performance, and over-estimates are the ones that ship.

---

</details>

> **2026 更新：**评估是 2021 年讲座中承受生成式 AI 时代压力测试最严峻的部分，因为那个根本假设——*存在一个单一的 ground-truth 标签可用于计算指标*——往往不再成立。当输出是一段文字、一篇文章或生成的代码时，没有唯一正确的字符串可供匹配：一个翻译可以有十几种优秀的表层形式，因此 exact-match 乃至 BLEU/ROUGE 与质量的相关性都很弱。该领域的务实答案是 **大语言模型作为评委**——提示一个强模型对输出打分或排序（成对比较比绝对打分更可靠），并用人类偏好进行验证。它可以扩展，但会引入自身的偏差（对第一个选项的位置偏差、对更长答案的冗长偏差、对自身家族风格的自我偏好），因此它是一个有噪声的代理，而非 ground truth，并且应对照人类评分进行校准。（关于*如何*接入大语言模型评委以及该调用哪个模型，请以提供方自己当前的文档为准，而不是凭记忆——见 Lecture-05 工具说明。）与此同时，§3 的**泄漏**主题再次出现，但扩展到了整个互联网，成为 **benchmark 污染**：如果测试题（MMLU、GSM8K、HumanEval）泄漏进了预训练语料，高分衡量的是记忆，而不是能力——正是“跨划分的重复记录”这个缺陷，只不过这里的划分是*互联网 vs. benchmark*，而且通常也无法检查训练集。防御手段在精神上相同（留出的、新收集的、时间门控的评估集，在模型训练截止时间*之后*发布；canary 字符串；私有排行榜），寓意也是如此：**分数是对未见数据的承诺，而污染就是那个让它好得令人难以置信的谎言。**


<details>
<summary>English original</summary>

> **2026 update:** Evaluation is the part of the 2021 lecture that the generative-AI era stress-tested hardest, because the bedrock assumption — *there exists a single ground-truth label to compute a metric against* — often no longer holds. When the output is a paragraph, an essay, or generated code, there's no one right string to match: a translation can be excellent in a dozen surface forms, so exact-match and even BLEU/ROUGE correlate weakly with quality. The field's pragmatic answer is **LLM-as-judge** — prompt a strong model to score or rank outputs (pairwise comparison is more reliable than absolute scoring), validated against human preference. It scales, but it imports its own biases (position bias toward the first option, verbosity bias toward longer answers, self-preference toward its own family's style), so it is a noisy proxy, not ground truth, and should be calibrated against human ratings. (For *how* to wire up an LLM judge and which model to call, defer to the provider's own current docs rather than memory — see the Lecture-05 tooling notes.) Meanwhile the **leakage** theme of §3 reappears, scaled to the whole internet, as **benchmark contamination**: if the test questions (MMLU, GSM8K, HumanEval) leaked into the pretraining corpus, a high score measures memorization, not capability — the exact "duplicate records across the split" bug, except the split is *the internet vs. the benchmark* and you usually can't inspect the training set. The defenses are the same in spirit (held-out, freshly-collected, time-gated evaluation sets released *after* a model's training cutoff; canary strings; private leaderboards), and so is the moral: **the score is a promise about unseen data, and contamination is the lie that makes it too good to be true.**

</details>

> **硬件视角：** 指标不只是统计数字，它们是*部署约束*，而在系统层面，最关键的是本讲几乎一带而过的那些运行指标——**推理延迟**与吞吐。在 F1 上胜出但未满足 p99 延迟预算的模型，无法在广告投放或交互式路径中交付；“最佳模型”是*质量 × 延迟 × 成本*的帕累托前沿，而不是准确率列的最高值。这将评估重新定义为硬件问题：你要在*实际*目标加速器上、在真实的批大小下测量尾部延迟（p50/p95/p99），因为一个足够准确但慢 3 倍的模型会被量化（INT8/FP8）、蒸馏或剪枝——每一种都会用 §1 质量指标的一小部分来换取 §1.3 业务指标所要求的延迟。而评估本身现在就是一项计算预算：运行一个 50 任务大语言模型 benchmark 套件，或对数千次生成运行一遍大语言模型作为评判者的 pass，是一笔不小的 GPU 开销，这就是为什么离线评估越来越像训练一样，需要同样的吞吐工程——批处理、缓存，以及 Lecture-02 的输入流水线规范。泛化，最终是在你实际用于推理服务的硬件上、在业务实际要求的延迟下来衡量的。

---


<details>
<summary>English original</summary>

> **Hardware lens:** Metrics are not just statistics, they are *deployment constraints*, and on the systems side the most consequential ones are the operational metrics this lecture lists almost in passing — **inference latency** and throughput. A model that wins on F1 but misses a p99 latency budget cannot ship in an ad-serving or interactive path; the "best model" is the Pareto frontier of *quality × latency × cost*, not the top of the accuracy column. This reframes evaluation as a hardware problem: you measure tail latency (p50/p95/p99) under realistic batch sizes on the *actual* target accelerator, because a model that's accurate enough but 3× too slow gets quantized (INT8/FP8), distilled, or pruned — each of which trades a sliver of the §1 quality metrics for the latency the §1.3 business metrics demand. And evaluation itself is now a compute budget: running a 50-task LLM benchmark suite, or an LLM-as-judge pass over thousands of generations, is a non-trivial GPU bill, which is why offline eval increasingly gets the same throughput engineering — batching, caching, the input-pipeline discipline from Lecture-02 — as training. Generalization, in the end, is measured on the hardware you'll actually serve from, under the latency the business actually requires.

---

</details>

## 内容截至

写于 2026 年 6 月。**原始 CS329P 内容** —— 模型指标与业务指标的划分、二分类指标家族（准确率及其不平衡陷阱、精度/召回/F1、ROC-AUC）、广告展示案例研究、通过训练与泛化差距以及模型/数据复杂度曲线诊断欠拟合/过拟合，以及验证纪律（留出法、k 折、非 i.i.d. 划分和泄漏“常见错误”）—— 首先讲授，因为它仍是正确的工作心智模型，并与 2021 年幻灯片一一对应。**刷新层**只增加幻灯片未及涵盖的内容：**PR-AUC** 作为 ROC-AUC 对不平衡更诚实的补充，显式排序指标（NDCG/MAP）和回归指标表，**forward chaining** 和 **group k-fold** 作为幻灯片中“顺序”和“聚类”情形的命名修复方案，以及 2026 年评估危机 —— **LLM-as-judge** 和 **benchmark 污染**，作为生成式时代对讲座自身泄漏论题的再生。在 2021 年框架已过时之处，先呈现原始内容，再给出更新；没有任何内容被悄然改写。

*改编自 [Stanford CS329P](https://c.d2l.ai/stanford-cs329p) — Huang, Li & Smola, CC-BY-SA-4.0.*


<details>
<summary>English original</summary>

**Current as of**

Written June 2026. The **original CS329P content** — the model-vs-business-metric split, the binary-classification metric family (accuracy and its imbalance trap, precision/recall/F1, ROC-AUC), the ad-display case study, the underfitting/overfitting diagnosis via the train-vs-generalization gap and the model-/data-complexity curves, and the validation discipline (holdout, k-fold, non-i.i.d. splitting, and the leakage "common mistakes") — is taught first because it remains the correct working mental model and maps one-to-one onto the 2021 slides. The **refresh layer** adds only what the slides predate: **PR-AUC** as the imbalance-honest companion to ROC-AUC, the explicit ranking metrics (NDCG/MAP) and regression-metric table, **forward chaining** and **group k-fold** as the named fixes for the slides' "sequential" and "clustered" cases, and the 2026 evaluation crisis — **LLM-as-judge** and **benchmark contamination** as the generative-era reincarnation of the lecture's own leakage thesis. Where the 2021 framing is dated, the original is presented before the update; nothing is silently rewritten.

*Adapted from [Stanford CS329P](https://c.d2l.ai/stanford-cs329p) — Huang, Li & Smola, CC-BY-SA-4.0.*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/4. ML Engineering and MLOps/Practical Machine Learning (CS329P)/Lecture-04.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/4.%20ML%20Engineering%20and%20MLOps/Practical%20Machine%20Learning%20%28CS329P%29/Lecture-04.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
