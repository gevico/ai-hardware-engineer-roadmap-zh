---
title: 第 02 讲 - 数据 II：清洗、变换与特征工程
description: 第 02 讲 - 数据 II：清洗、变换与特征工程
---

# 第 02 讲 - 数据 II：清洗、变换与特征工程

**合集：** [Practical Machine Learning (CS329P)](README.md) | **上一篇：** [← 第 01 讲](Lecture-01.md) | **下一篇：** [第 03 讲](Lecture-03.md)

---

第 01 讲中，你把数据弄进了大楼 —— 抓取而来、贴上标签，无论多不完美。本讲讨论的，是在这些数据被允许靠近模型之前发生的一切。它是整条 pipeline 中最不光鲜的阶段，却最决定模型的上限。*Garbage in, garbage out* 在这里不是口号，而是一个可量化的论断。用脏的、尺度错误的、表征糟糕的数据训练出的模型不会大声报错 —— 它会收敛，画出一条看似合理的 loss 曲线，然后安静地输给拿到干净输入的模型；你可能直到它上线、给出糟糕的推荐、进而污染你下一批采集到的数据时才会察觉。

这项工作分成三个动作，大致按顺序执行：**清洗**（找出并修正错误的值）、**变换**（把正确的值重塑成算法真正想要的定长、良态、分布良好的形式）、**特征工程**（把原始列变成与目标相关的表示）。然后是贯穿这三者的第四项：**数据摘要** —— 足够认真地审视分布，从而判断前三个中你到底需要哪些。深度学习已经侵蚀了非结构化数据上的手工特征工程环节（CNN 自己学习特征），但对表格数据 —— 仍然是工业界 ML 的大头 —— 下面每一步都得你亲手做，做得好坏决定了你是拿到 Kaggle 奖牌还是提交一份无人记得的结果。

CS329P 的思维模型：这是 pipeline 中的一个方框 —— `raw data → labelling & cleaning → data transformation → feature engineering → model training` —— 也是你花费最多 wall-clock 时间、赢得最多 accuracy 的那个方框。

---

## 学习目标

学完本讲后，你应当能够：

1. **对数据错误分类**：异常值、规则违反或模式违反，并为每一类选择正确的检测器。
2. **归一化实值列**，并针对给定分布，在 min-max、z-score、decimal 和 log 缩放之间做出选择。
3. **变换非结构化数据** —— 缩放/裁剪/白化图像、截取与采样视频、对文本做 tokenize —— 同时权衡存储、质量与加载速度。
4. **为以下数据做特征工程**：表格数据（分桶、one-hot、hashing、datetime、crosses）、文本（BoW、TF-IDF、n-grams、embeddings）和图像数据（手工设计 → 学习得到）。
5. **读懂数据集的分布**，足以判断哪些清洗和变换步骤确实有必要。
6. **把预处理开销当作吞吐问题来推理**，而不是一个免费的预备步骤。

---

## 1. 数据清洗 —— 找出哪里错了

数据错误是与 ground truth 不匹配的地方：缺失值、错误值、极端值。好的模型对其中一部分是*鲁棒*的 —— 用 SGD 训练的深度网络对标签噪声的容忍度远好于决策树，后者能围绕单个坏点切出一个叶子 —— 但鲁棒性只是缓冲，不是许可证。跳过清洗的后果之所以阴险，恰恰因为它们悄无声息：训练照样收敛（只是更慢），准确率以一种难以归因的方式下降，而基于脏数据构建的已部署模型会开始塑造下一批采集到的数据。一个糟糕的推荐器会产生那些「正」点击，它们随后成为明天的训练标签，错误就此在这个数据飞轮中不断累积。

CS329P 把错误分成三类，这一划分很重要，因为每一类需要不同的检测器：

| 错误类型 | 定义 | 例子 | 如何捕获 |
|---|---|---|---|
| **异常值** | 与其他观测值显著偏离的值 | 房产数据集中标价 \$5 的房子 | 统计/分布方法（箱线图、IQR） |
| **规则违反** | 破坏完整性约束的值 | `NOT NULL` 字段为 null；年龄为负；主键重复 | 规则/约束检查 |
| **模式违反** | 破坏句法或语义约束的值 | 同一语言的 `"eng"`、`"en"`、`"english"`；`Country` 列中出现 `"Stanford"` | 模式/类型/知识检查 |


<details>
<summary>English original</summary>

**Lecture 02 - Data II: Cleaning, Transformation & Feature Engineering**

**Collection:** [Practical Machine Learning (CS329P)](README.md) | **Previous:** [← Lecture 01](Lecture-01.md) | **Next:** [Lecture 03](Lecture-03.md)

---

In Lecture 01 you got data into the building — scraped, labeled, however imperfectly. This lecture is what happens before any of it is allowed near a model. It is the least glamorous stage in the entire pipeline and the one that most decides your model's ceiling. *Garbage in, garbage out* is not a slogan here; it is a quantitative claim. A model trained on dirty, mis-scaled, badly-represented data does not fail loudly — it converges, posts a plausible loss curve, and quietly underperforms a model that got clean inputs, and you may not notice until it is in production making bad recommendations that poison the very data you collect next.

The work splits into three movements, run roughly in order: **cleaning** (find and fix the values that are wrong), **transformation** (reshape what's correct into the fixed-length, well-conditioned, nicely-distributed form algorithms actually want), and **feature engineering** (turn raw columns into representations relevant to the target). Then a fourth, threaded through all of them: **data summary** — looking at distributions hard enough to know which of the first three you even need. Deep learning has eroded the manual feature-engineering step for unstructured data (a CNN learns its own features), but for tabular data — still most of industry's ML — every step below is yours to do by hand, and doing it well is the difference between a Kaggle medal and a forgettable submission.

The mental model from CS329P: this is one box in a pipeline — `raw data → labelling & cleaning → data transformation → feature engineering → model training` — and it is the box where you spend most of your wall-clock time and earn most of your accuracy.

---

**Learning objectives**

By the end of this lecture you should be able to:

1. **Classify a data error** as an outlier, a rule violation, or a pattern violation, and pick the right detector for each.
2. **Normalize real-valued columns** and choose between min-max, z-score, decimal, and log scaling for a given distribution.
3. **Transform unstructured data** — resize/crop/whiten images, clip and sample video, tokenize text — while trading off storage, quality, and load speed.
4. **Engineer features** for tabular (bucketing, one-hot, hashing, datetime, crosses), text (BoW, TF-IDF, n-grams, embeddings), and image data (hand-crafted → learned).
5. **Read a dataset's distributions** well enough to decide which cleaning and transformation steps are actually warranted.
6. **Reason about preprocessing cost** as a throughput problem, not a free pre-step.

---

**1. Data cleaning — finding what's wrong**

Data errors are mismatches with ground truth: missing values, erroneous values, extreme values. Good models are *robust* to some of this — a deep net trained with SGD shrugs off label noise far better than a decision tree, which can carve a leaf around a single bad point — but robustness is a buffer, not a license. The consequences of skipping cleaning are insidious precisely because they are quiet: training still converges (just slower), accuracy degrades in a way that is hard to attribute, and a deployed model built on dirty data starts shaping the next batch of collected data. A poor recommender generates the "positive" clicks that become tomorrow's training labels, and the error compounds into the data flywheel.

CS329P splits errors into three types, and the split matters because each demands a different detector:

| Error type | Definition | Example | How you catch it |
|---|---|---|---|
| **Outliers** | Values that deviate significantly from other observations | A house priced at \$5 in a real-estate set | Statistical / distributional (boxplot, IQR) |
| **Rule violations** | Values that break integrity constraints | A `NOT NULL` field that is null; a negative age; a duplicate primary key | Rule / constraint checks |
| **Pattern violations** | Values that break syntactic or semantic constraints | `"eng"`, `"en"`, `"english"` for the same language; `"Stanford"` in a `Country` column | Pattern / type / knowledge checks |

</details>

### 离群点 vs. 欠采样的稀有事件

离群点检测的难点在于，仅凭数值本身，离群点与*合法的稀有事件*看起来完全一样。一笔 \$40M 的销售额在住房数据集中是统计意义上的离群点，同时又是一笔真实交易。仅因为它远离中位数就删除它，会丢掉信号。该做何种判断——丢弃、截断（winsorize）还是保留——属于领域推理，而非阈值。标准的*首轮*工具是箱线图 / Tukey fence：超出 `Q3 + 1.5·IQR` 或低于 `Q1 − 1.5·IQR` 的任何值都被标记为候选，而非定论。

```python
import numpy as np

def iqr_outlier_mask(x: np.ndarray, k: float = 1.5) -> np.ndarray:
    q1, q3 = np.percentile(x, [25, 75])
    iqr = q3 - q1
    lo, hi = q1 - k * iqr, q3 + k * iqr
    return (x < lo) | (x > hi)   # True = candidate outlier, then USE JUDGMENT
```

### 基于规则的检测

规则编码了你确知必须成立的完整性约束。讲义给出两类：

- **函数依赖** —— `x → y` 表示 `x` 的一个取值决定唯一的 `y`。Zip code → state。EIN → company name。若在你的数据中一个 zip 映射到两个 state，则有一行是错的。
- **否定约束** —— 更丰富的一阶逻辑条件：*"若供应商有 EIN，则电话号码不得为空"*；*"若两条 capture 共用同一 tag number，则较早的那条必须标记为 original。"* 这些能捕获单列检查永远发现不了的跨列不一致。

```python
# Functional dependency check: does each zip map to exactly one state?
violations = (
    df.groupby("zip")["state"].nunique()
      .loc[lambda s: s > 1]          # zips with >1 distinct state = broken FD
)
```

### 基于模式的检测

- **语法模式** —— 将一列映射到其最显著的数据类型，并标记不符合的值，或对变体做规范化（`eng`、`en`、`english` → `English`）。一个 "date" 列中有 2% 的单元格是自由文本，就是语法违规。
- **语义模式** —— 引入外部知识，例如一个知识图谱指出 `Country` 列中的取值必须是首都/国家名，因此 `"Stanford"` 无效，尽管它本身是完全格式良好的字符串。

**修复**，而不只是发现，是另一半工作。沿手动到自动的谱系存在多种工具：交互式图形化数据整理工具（Trifacta Wrangler、OpenRefine）让人可以看到并修复；自动系统则按上述规则大规模地检测并修复。实践中，你先在样本上交互式操作以*发现*规则，再把它们固化为自动检查，在未来每一批数据上运行——清洗是流水线的一个阶段，而非一次性的擦洗。

---

## 2. 数据变换——重塑正确的数据

ML 算法*偏好定义良好、定长、良态、分布漂亮的输入。* 清洗让数据正确；变换让数据可消化。方法是按模态划分的。

### 2.1 实值列的归一化

归一化让训练更稳定——它改善优化的条件，使各特征上的梯度可比，损失曲面不至于是被拉伸的深谷。四种标准映射：

| 方法 | 公式 | 输出范围 | 适用场景 |
|---|---|---|---|
| **Min-max** | `x' = (x − min)/(max − min)·(b − a) + a` | 恰为 `[a, b]` | 需要有界范围；分布大致均匀；*但*对离群点敏感（一个极端值会压扁其余所有值） |
| **Z-score** | `x' = (x − mean)/std` | 均值 0、标准差 1、无界 | 默认选择。特征大致为高斯分布；比 min-max 更不易受离群点影响 |
| **Decimal scaling** | `x' = x / 10ʲ`，最小的 `j` 使得 `max(|x'|) < 1` | `(−1, 1)` | 快速量级归一化 |
| **Log scaling** | `x' = log(x)` | 压缩尾部 | 重尾 / 乘性数据（价格、计数、人口） |

**Min-max vs. z-score** 是你最常做的选择。Min-max 保证范围，某些算法（以及有界激活函数）需要这一点，但 `max` 处的一个离群点会把每个真实值压向 `a`。Z-score 无界，但对极端值稳健得多，对于假定输入居中的无树模型是安全的默认选择。Log scaling 是正交的——对重尾列（房价跨度 \$10K–\$40M）*先*施加它，再对结果做 z-score。

```python
x_minmax = (x - x.min()) / (x.max() - x.min())          # → [0, 1]
x_zscore = (x - x.mean()) / x.std()                      # → mean 0, std 1
x_log    = np.log1p(x)                                   # log(1+x), safe at 0
```

> **纪律：**只在**训练划分**上拟合归一化统计量（`min`、`max`、`mean`、`std`），然后将其应用于验证集和测试集。在整个数据集上计算它们会把测试信息泄漏进训练，是最常见的隐性评估 bug 之一——这也是 Lecture 04 的预告。


<details>
<summary>English original</summary>

**Outliers vs. under-sampled rare events**

The hard part of outlier detection is that an outlier and a *legitimate rare event* look identical from the value alone. A \$40M sale is a statistical outlier in a housing dataset and also a real transaction. Deleting it because it's far from the median throws away signal. The judgment call — drop, cap (winsorize), or keep — is domain reasoning, not a threshold. The standard *first-pass* tool is the boxplot / Tukey fence: anything beyond `Q3 + 1.5·IQR` or below `Q1 − 1.5·IQR` is flagged as a candidate, not a verdict.

```python
import numpy as np

def iqr_outlier_mask(x: np.ndarray, k: float = 1.5) -> np.ndarray:
    q1, q3 = np.percentile(x, [25, 75])
    iqr = q3 - q1
    lo, hi = q1 - k * iqr, q3 + k * iqr
    return (x < lo) | (x > hi)   # True = candidate outlier, then USE JUDGMENT
```

**Rule-based detection**

Rules encode integrity constraints you know must hold. Two flavors from the lecture:

- **Functional dependencies** — `x → y` means a value of `x` determines a unique `y`. Zip code → state. EIN → company name. If one zip maps to two states in your data, one row is wrong.
- **Denial constraints** — richer first-order-logic conditions: *"phone number must not be empty if the vendor has an EIN"*; *"if two captures share a tag number, the earlier one must be marked original."* These catch cross-column inconsistencies a single-column check never would.

```python
# Functional dependency check: does each zip map to exactly one state?
violations = (
    df.groupby("zip")["state"].nunique()
      .loc[lambda s: s > 1]          # zips with >1 distinct state = broken FD
)
```

**Pattern-based detection**

- **Syntactic patterns** — map a column to its most prominent data type and flag values that don't fit, or canonicalize variants (`eng`, `en`, `english` → `English`). A "date" column where 2% of cells are free text is a syntactic violation.
- **Semantic patterns** — bring in external knowledge, e.g. a knowledge graph that says values in a `Country` column must be capitals/nations, so `"Stanford"` is invalid even though it's a perfectly well-formed string.

**Fixing**, not just finding, is the other half. Multiple tools exist along a spectrum from manual to automatic: interactive graphical wranglers (Trifacta Wrangler, OpenRefine) let a human see and fix; automatic systems detect-and-repair against the rules above at scale. In practice you start interactive on a sample to *discover* the rules, then codify them into an automatic check that runs on every future batch — cleaning is a pipeline stage, not a one-time scrub.

---

**2. Data transformation — reshaping correct data**

ML algorithms *prefer well-defined, fixed-length, well-conditioned, nicely-distributed input.* Cleaning made the data correct; transformation makes it digestible. The methods are per-modality.

**2.1 Normalization for real-valued columns**

Normalization makes training more stable — it conditions the optimization so gradients across features are comparable and the loss surface isn't a stretched ravine. Four standard maps:

| Method | Formula | Output range | Use when |
|---|---|---|---|
| **Min-max** | `x' = (x − min)/(max − min)·(b − a) + a` | exactly `[a, b]` | You need a bounded range; distribution is roughly uniform; *but* sensitive to outliers (one extreme value squashes everything else) |
| **Z-score** | `x' = (x − mean)/std` | mean 0, std 1, unbounded | The default. Roughly Gaussian features; less outlier-sensitive than min-max |
| **Decimal scaling** | `x' = x / 10ʲ`, smallest `j` s.t. `max(|x'|) < 1` | `(−1, 1)` | Quick magnitude normalization |
| **Log scaling** | `x' = log(x)` | compresses tail | Heavy-tailed / multiplicative data (prices, counts, populations) |

**Min-max vs. z-score** is the choice you make most. Min-max guarantees a range, which some algorithms (and bounded activations) want, but a single outlier at `max` compresses every real value toward `a`. Z-score has no bound but is far more robust to extremes and is the safe default for tree-free models that assume centered inputs. Log scaling is orthogonal — apply it *first* to a heavy-tailed column (house price spans \$10K–\$40M), then z-score the result.

```python
x_minmax = (x - x.min()) / (x.max() - x.min())          # → [0, 1]
x_zscore = (x - x.mean()) / x.std()                      # → mean 0, std 1
x_log    = np.log1p(x)                                   # log(1+x), safe at 0
```

> **Discipline:** fit normalization statistics (`min`, `max`, `mean`, `std`) on the **training split only**, then apply them to validation and test. Computing them over the full dataset leaks test information into training and is one of the most common silent evaluation bugs — a preview of Lecture 04.

</details>

### 2.2 图像变换

存储是驱动因素。CS329P 的贯穿示例：抓取每年约 5M 条美国房屋销售 × 约 20 张图像 × 约 153 KB（约 1041×732）≈ **15 TB/年**。缩放到约 320×224 后就降到约 **1.4 TB**——缩小 10 倍——而 ML 擅长处理低分辨率图像，所以准确率几乎不变。涉及的变换：

- **裁剪、降采样、压缩**——降低分辨率并重新编码，以节省存储，并在训练时加载得更快。
- **有损压缩意识**——JPEG 不是免费的。中等（80–90%）JPEG 压缩在 ImageNet 上可能损失约 1% 准确率。了解你的质量旋钮；不要把评测集压缩成与真实情况不同的分布。
- **图像白化**——针对向量的广义归一化。局部邻域内的像素高度相关；白化通过线性变换去除这种冗余。对于均值为 0、协方差估计为 `Σ` 的数据 `x`，选择 `W` 使得 `WᵀW = Σ⁻¹`，从而使 `y = Wx` 具有单位对角协方差。常见选择：`Σ` 的特征系统（PCA 白化）或 `Σ^(−1/2)`（ZCA 白化）。模型——尤其是 GAN 这类无监督模型——在白化后的输入上收敛更快。

### 2.3 视频变换

视频的问题在于输入的多样性：电影约 2 h，YouTube 约 11 min，TikTok 约 15 s。ML 问题只有在**短片（<10 s）**上才变得可处理，理想情况下每个片段是一个连贯事件（一个人体动作）——把长视频语义分割成这类事件极其困难。常见的流水线在存储与质量、加载速度之间做取舍：**解码出可播放的片段，采样一个帧序列，并为音频计算频谱图。** 帧和频谱图喂给模型很省事，但比源视频占用更多存储；这个取舍就是全部关键。

### 2.4 文本变换

- **词干提取与词形还原**——把单词归并到一个共同的基形式。`am, are, is → be`；`car, cars, car's, cars' → car`。在表层形式属于噪声的场景（如主题建模）有用。（现代子词 tokenizer 往往让这一步变得不必要——见 2026 更新。）
- **Tokenization**——把字符串切分成算法所看到的最小单元：

| 粒度 | 方法 | 取舍 |
|---|---|---|
| **按词** | `text.split(' ')` | 可解释；词表巨大；遇到 OOV / 拼写错误就崩 |
| **按字符** | `list(text)` | 词表极小，无 OOV；序列很长，单元弱 |
| **按子词** | 学习得到的词表（WordPiece、Unigram、BPE） | 两者兼得——词表固定，对罕见词处理得宜 |

子词是现代默认做法：`"a new gpu!"` → `"a", "new", "gp", "##u", "!"`，其中的词表是*从语料中学习*得到的（WordPiece/Unigram），因此高频词保持完整，罕见词分解为已知片段。

---

## 3. 特征工程——原始数据 → 有用的表示

特征是与目标任务相关的原始数据的表示。在深度学习之前，特征工程*就是*工作本身：经典 CV 手工检测角点和兴趣点，再喂给 SVM 或 softmax 回归。深度网络翻转了这一点——CNN 端到端地学习特征提取器，因此特征比任何手工设计都更贴合任务，代价是吃数据、耗算力。分界线在于模态：**对于非结构化数据（图像/视频/音频/文本），学习到的特征更胜一筹，应当直接采用；对于表格数据，准确率仍然来自手工工程的特征。**

### 3.1 表格特征

- **Int / float**——直接使用，或**分箱**成 `n` 个离散桶。分箱让线性模型能表达非线性（0–18、18–35、35–65、65+ 岁的行为不同），并通过把离群值截断到边缘桶来抑制其影响。
- **类别型 → one-hot**——`cat → [0,1,0,0,0]`，`dog → [0,0,0,1,0]`。把罕见类别映射到单个 `"Unknown"` 桶，以免因只出现两次的取值把维度撑爆。当基数极大（数百万个用户 ID）时，one-hot 不可行——使用**哈希技巧**：把类别哈希到固定数量的桶中，以少量碰撞为代价换取有界、定宽的向量。
- **日期时间**——单个时间戳会展开成一个特征列表：`[year, month, day, day_of_year, week_of_year, day_of_week]`，再加上 is-weekend / is-holiday 标志。大部分时间信号存在于这些周期性部分中，而不是原始 epoch。
- **特征交叉（组合）**——两组特征的笛卡尔积：`[cat, dog] × [male, female] → [(cat,male), (cat,female), (dog,male), (dog,female)]`。交叉让线性模型捕捉它本来无法捕捉的交互（“雨天”的效果取决于“周末”）。它们会迅速撑爆维度，因此要有意识地交叉，并依靠哈希来限制结果规模。

```python
# Datetime explosion
ts = df["event_time"]
df["year"]        = ts.dt.year
df["month"]       = ts.dt.month
df["day_of_week"] = ts.dt.dayofweek
df["is_weekend"]  = (ts.dt.dayofweek >= 5).astype(int)
```


<details>
<summary>English original</summary>

**2.2 Image transformations**

Storage is the forcing function. CS329P's running example: scraping ~5M US home sales/year × ~20 images × ~153 KB at ~1041×732 is ~**15 TB/year**. Resize to ~320×224 and it drops to ~**1.4 TB** — a 10× cut — and ML is good at low-resolution images, so accuracy barely moves. The transforms:

- **Cropping, downsampling, compression** — shrink resolution and re-encode to save storage and load faster at training time.
- **Lossy compression awareness** — JPEG is not free. Medium (80–90%) JPEG compression can cost ~1% accuracy on ImageNet. Know your quality knob; don't compress your eval set into a different distribution than reality.
- **Image whitening** — a generalized normalization for vectors. Pixels in a local neighborhood are highly correlated; whitening removes that redundancy via a linear transform. With data `x` of mean 0 and covariance estimate `Σ`, choose `W` such that `WᵀW = Σ⁻¹`, so `y = Wx` has unit-diagonal covariance. Common choices: the eigensystem of `Σ` (PCA whitening) or `Σ^(−1/2)` (ZCA whitening). Models — especially unsupervised ones like GANs — converge faster on whitened input.

**2.3 Video transformations**

Video's problem is input variability: movies run ~2 h, YouTube ~11 min, TikTok ~15 s. ML problems get tractable on **short clips (<10 s)**, ideally each a single coherent event (one human action) — semantic segmentation of long video into such events is extremely hard. The common pipeline trades storage against quality and load speed: **decode a playable clip, sample a sequence of frames, and compute spectrograms for the audio.** Frames-and-spectrograms are trivial to feed a model but cost more storage than the source video; that tradeoff is the whole game.

**2.4 Text transformations**

- **Stemming and lemmatization** — collapse a word to a common base form. `am, are, is → be`; `car, cars, car's, cars' → car`. Useful where surface form is noise, e.g. topic modeling. (Modern subword tokenizers often make this unnecessary — see the 2026 update.)
- **Tokenization** — split a string into the smallest unit the algorithm sees:

| Granularity | Method | Tradeoff |
|---|---|---|
| **By word** | `text.split(' ')` | Interpretable; huge vocab; chokes on OOV / typos |
| **By char** | `list(text)` | Tiny vocab, no OOV; very long sequences, weak units |
| **By subword** | learned vocab (WordPiece, Unigram, BPE) | Best of both — fixed vocab, graceful on rare words |

Subword is the modern default: `"a new gpu!"` → `"a", "new", "gp", "##u", "!"`, where the vocabulary is *learned from the corpus* (WordPiece/Unigram) so frequent words stay whole and rare ones decompose into known pieces.

---

**3. Feature engineering — raw data → useful representation**

A feature is a representation of raw data relevant to the target task. Before deep learning, feature engineering *was* the job: classical CV detected corners and interest points by hand and fed them to an SVM or softmax regression. Deep nets flipped this — a CNN learns the feature extractor end-to-end, so features become more relevant to the task than anything hand-designed, at the cost of being data-hungry and compute-heavy. The dividing line is modality: **for unstructured data (image/video/audio/text), learned features win and you should reach for them; for tabular data, hand-engineered features are still where accuracy comes from.**

**3.1 Tabular features**

- **Int / float** — use directly, or **bin** into `n` discrete buckets. Bucketing lets a linear model express nonlinearity (age 0–18, 18–35, 35–65, 65+ behave differently) and tames outliers by capping them into an edge bin.
- **Categorical → one-hot** — `cat → [0,1,0,0,0]`, `dog → [0,0,0,1,0]`. Map rare categories to a single `"Unknown"` bucket so you don't explode the dimension on values seen twice. When cardinality is huge (millions of user IDs), one-hot is infeasible — use the **hashing trick**: hash the category into a fixed number of buckets, accepting rare collisions for a bounded, fixed-width vector.
- **Datetime** — a single timestamp explodes into a feature list: `[year, month, day, day_of_year, week_of_year, day_of_week]`, plus is-weekend / is-holiday flags. Most temporal signal lives in these cyclic parts, not the raw epoch.
- **Feature crosses (combinations)** — the Cartesian product of two feature groups: `[cat, dog] × [male, female] → [(cat,male), (cat,female), (dog,male), (dog,female)]`. Crosses let a linear model capture interactions it otherwise can't (the effect of "rainy" depends on "weekend"). They blow up dimensionality fast, so cross deliberately and lean on hashing to bound the result.

```python
# Datetime explosion
ts = df["event_time"]
df["year"]        = ts.dt.year
df["month"]       = ts.dt.month
df["day_of_week"] = ts.dt.dayofweek
df["is_weekend"]  = (ts.dt.dayofweek >= 5).astype(int)
```

</details>

### 3.2 文本特征

从词袋计数一路做到学习到的向量：

- **词袋（BoW）** —— 把文本表示为词表上的 token 计数。`"dog and cat and dinosaur"` → 相对词表 `[fish, cat, and, dog, dinosaur]` 得到形如 `[0, 1, 2, 1, 1]` 的计数向量。简单，且是强基线，但需要精心设计词表，并且*丢失词上下文*——词序信息没了。
- **n-gram** —— 统计连续序列（bigram、trigram）而非单个 token，恢复少量局部词序（`"not good"` 成为独立特征），代价是词表大得多、也稀疏得多。
- **TF-IDF** —— 用 **词频 × 逆文档频率** 对 BoW 计数重新加权，使得在*本文档*中常见、在整个语料中罕见的词得分高，而无处不在的词（"the"）被抑制。对经典文本模型而言，这是优于原始计数的标准升级。
- **词嵌入（如 Word2vec）** —— 把每个词映射为稠密向量，使相近的词彼此靠近；训练方式是依据上下文预测目标词。稠密、低维，编码的是语义而非词的身份。
- **预训练语言模型（BERT、GPT、universal sentence encoder）** —— 在海量无标注文本上训练的超大 Transformer。有两种用法：抽取文本 **embedding** 作为特征，或在下游任务上**微调**整个模型。2026 年，凡正经的文本问题都默认这么做。

| 文本表示 | 是否捕捉上下文？ | 维度 | 适用场景 |
|---|---|---|---|
| **BoW** | 否 | 高、稀疏 | 快速基线、可解释 |
| **n-gram** | 仅局部 | 更高、更稀疏 | 短语信号（情感） |
| **TF-IDF** | 否 | 高、稀疏 | 经典文本分类 / 检索 |
| **Word2vec** | 词级 | 低、稠密 | 相似度、深度学习之前的流水线 |
| **预训练 LM** | 完整 | 稠密、带上下文 | 如今凡是在意准确率的场景 |

### 3.3 图像 / 视频特征

传统做法是用 **SIFT** 之类手工设计的描述子提取图像特征，再送入分类器。如今的默认做法是把**预训练深度网络当作冻结的特征提取器**：

- **ResNet** —— 在 ImageNet（图像分类）上训练——图像特征提取的主力骨干网络。
- **I3D** —— 在 Kinetics（动作分类）上训练——用于视频。

现成的骨干网络有很多，很少需要从零训练特征提取器。这是通往**迁移学习**（第 09 讲）的入口，在那里复用预训练特征*就是*工作流本身。

**数据增强** —— 用保持标签不变的变换合成地扩充训练数据：图像用随机裁剪、水平翻转、颜色抖动、旋转、cutout/mixup；音频用时间平移和加噪。数据增强一半是数据准备，一半是正则化——它教给模型不变性（翻转的猫还是猫），是在有限数据上换取泛化能力最便宜的手段之一。

> **要点：** 特征*工程* vs. 特征*学习*。能用学习就用学习——图像、视频、音频、文本。表格数据则手工设计特征。这一条规则贯穿了本节的大部分内容。

---


<details>
<summary>English original</summary>

**3.2 Text features**

Working up from bag-of-counts to learned vectors:

- **Bag of words (BoW)** — represent text as token counts over a vocabulary. `"dog and cat and dinosaur"` → a count vector like `[0, 1, 2, 1, 1]` against vocab `[fish, cat, and, dog, dinosaur]`. Simple and strong baselines, but needs careful vocabulary design and *loses word context* — order is gone.
- **n-grams** — count contiguous sequences (bigrams, trigrams) instead of single tokens, recovering a little local order (`"not good"` becomes its own feature) at the cost of a much larger, sparser vocabulary.
- **TF-IDF** — reweight BoW counts by **term frequency × inverse document frequency** so that words common in *this* document but rare across the corpus score high, and ubiquitous words ("the") are damped. This is the standard upgrade over raw counts for classical text models.
- **Word embeddings (e.g. Word2vec)** — map each word to a dense vector so similar words sit close together; trained by predicting a target word from its context. Dense, low-dimensional, and they encode meaning rather than identity.
- **Pre-trained language models (BERT, GPT, universal sentence encoders)** — giant transformers trained on vast unannotated text. Use them two ways: pull out a text **embedding** as features, or **fine-tune** the whole model on your downstream task. In 2026 this is the default for any serious text problem.

| Text representation | Captures context? | Dimensionality | Where it shines |
|---|---|---|---|
| **BoW** | No | High, sparse | Fast baselines, interpretable |
| **n-grams** | Local only | Higher, sparser | Phrase signals (sentiment) |
| **TF-IDF** | No | High, sparse | Classical text classification / retrieval |
| **Word2vec** | Word-level | Low, dense | Similarity, pre-DL pipelines |
| **Pretrained LM** | Full | Dense, contextual | Anything where accuracy matters today |

**3.3 Image / video features**

Traditionally you extracted images by hand-crafted descriptors like **SIFT** and fed them to a classifier. Now the default is a **pre-trained deep net as a frozen feature extractor**:

- **ResNet** — trained on ImageNet (image classification) — the workhorse image-feature backbone.
- **I3D** — trained on Kinetics (action classification) — for video.

Many off-the-shelf backbones exist; you rarely train a feature extractor from scratch. This is the on-ramp to **transfer learning** (Lecture 09), where reusing pretrained features *is* the workflow.

**Augmentation** — synthetically expand training data with label-preserving transforms: random crop, horizontal flip, color jitter, rotation, cutout/mixup for images; time-shift and noise for audio. Augmentation is half data-prep, half regularization — it teaches invariances (a flipped cat is a cat) and is one of the cheapest ways to buy generalization on limited data.

> **The headline:** feature *engineering* vs. feature *learning*. Prefer learning when it's available — images, video, audio, text. Hand-engineer when it's tabular. That single rule organizes most of this section.

---

</details>

## 4. 数据概览 — 理解分布

贯穿上述一切的是本讲开篇即定下的准则：**探索性数据分析。** 在选择归一化器、分桶方案，或判定某个值是否离群之前，你首先要*看*——看每列分布、缺失值比例、基数、相关性、类别平衡。这份概览会告诉你，前三节中哪些是你真正需要的：

- 右尾厚重，说明*先取对数再算 z-score*。
- 某列 60% 为空，说明*插补或丢弃，不要把空值 one-hot*。
- 某类别特征有 10⁶ 个唯一值，说明*做哈希，不要 one-hot*。
- 95/5 的类别不平衡会彻底改变你的指标与验证方案（第 04 讲）。

```python
df.describe(include="all")          # ranges, mean/std, top categories, counts
df.isna().mean().sort_values()      # per-column missing-value rate
df.nunique()                        # cardinality → one-hot vs. hashing decision
df["label"].value_counts(normalize=True)   # class balance
```

CS329P 把整个数据部分画成一张项目启动流程图：*数据够吗？* → 不够，就去发现 / 增强 / 生成（第 01 讲）；够，则**预处理**——EDA → 清洗 → 变换 → 特征工程——然后训练、评测、迭代。这个迭代循环（“改进标签、数据，还是模型？”）通常把你*送回本讲*，而不是推向更花哨的模型。数据才是杠杆。

---

> **2026 更新：** 自 2021 年讲义以来有三个转变。**(1) 深度网络终结了非结构化数据上的手工特征工程**——生产级 NLP 中已无人手写 SIFT 或设计 BoW 词表；你改为微调预训练 Transformer 或取 embedding。但对于**表格数据——仍占工业 ML 的大多数——特征工程依然具有决定性**，在精心构造的特征上跑梯度提升树（XGBoost/LightGBM）往往胜过深度网络。**(2) 特征存储**——Feast（开源）与 Tecton（托管）如今接管了“特征只算一次，并一致地供给训练与推理”这一问题，消灭了过去会悄无声息毁掉部署的 train/serve skew（你在 notebook 里用一种方式算特征，在推理服务路径里又用另一种方式算）。**(3) 以数据为中心的 AI**——Andrew Ng 的重新定位，即*系统性地改进数据胜过反复调模型*，把本讲中这些“不体面”的清洗 / 标注 / 增强工作变成了一门带工具链的一等方法论（用于标签错误的 cleanlab、弱监督框架）。本讲的论点经受住了时间考验；整个领域追上了它。在分词方面，subword（BPE/WordPiece/Unigram）如今已是通用做法，而词干化 / 词形还原在神经流水线中已基本退场。

> **硬件视角：** 预处理不是免费的准备工作——它是一个与训练争抢算力的吞吐阶段，在快速加速器上它通常就是瓶颈。典型故障：多 GPU 训练只跑到 40% 利用率，因为几个 CPU 核无法足够快地 JPEG 解码、缩放和增强图像来喂饱它——GPU 只能空等输入流水线。修复手段本身就是一门工程学科：预取与重叠（GPU 计算当前批时，双缓冲下一批），缓存解码 / 缩放后的张量，让这份开销只付一次而不是每个 epoch 都付，用顺序格式存储（TFRecord / WebDataset / Parquet）以便读取的是流而不是几百万个小文件，以及——最关键的一条——用 NVIDIA **DALI** 把 **decode/resize/augment 下推到 GPU 本身**，这能把 CPU 受限的流水线变成 GPU 受限的流水线，把那部分闲置利用率找回来。§2.2 中那次 15 TB → 1.4 TB 的缩放讲的是同一个道理，只是处于静止状态：更小的输入意味着更少的 I/O、更少的解码、更快的 epoch。衡量输入流水线吞吐要用 samples/s，并与模型 FLOPs 并列对比；一个*本可以*以 5,000 img/s 训练、却只被喂以 1,200 img/s 的模型，是个披着模型外衣的数据加载问题。

---

## 内容时效

撰写于 2026 年 6 月。**CS329P 原始内容**——三类错误分类法（离群值 / 规则违反 / 模式违反）、四种归一化器、图像 / 视频 / 文本变换方法，以及表格 / 文本 / 图像特征工程目录——先讲，是因为它仍是正确的工作心智模型，并与 2021 年讲义一一对应。**刷新层**标出了发生变动之处：非结构化数据上学习型特征对手工特征的压倒性优势（同时确认特征工程对表格数据仍居首位）、**特征存储**（Feast/Tecton）成为标准基础设施、**以数据为中心的 AI** 的重新定位、subword 分词的通用化，以及**DALI / GPU 侧预处理**的吞吐故事——原讲义聚焦存储成本，对此只是点到为止。凡 2021 年的表述已过时之处，都先呈现原文再给更新；没有任何内容被悄悄改写。

*改编自 [Stanford CS329P](https://c.d2l.ai/stanford-cs329p) — Huang, Li & Smola，CC-BY-SA-4.0。*


<details>
<summary>English original</summary>

**4. Data summary — understanding distributions**

Threaded through everything above is the discipline the lecture opens with: **exploratory data analysis.** Before you choose a normalizer or a bucketing scheme or decide a value is an outlier, you *look* — at per-column distributions, missing-value rates, cardinalities, correlations, class balance. The summary tells you which of the prior three sections you actually need:

- A heavy right tail says *log-scale before z-score*.
- A column that's 60% null says *impute or drop, don't one-hot the nulls*.
- A categorical with 10⁶ unique values says *hash, don't one-hot*.
- A 95/5 class imbalance changes your metric and validation scheme entirely (Lecture 04).

```python
df.describe(include="all")          # ranges, mean/std, top categories, counts
df.isna().mean().sort_values()      # per-column missing-value rate
df.nunique()                        # cardinality → one-hot vs. hashing decision
df["label"].value_counts(normalize=True)   # class balance
```

CS329P frames the whole data part as a flow chart that starts a project: *have enough data?* → if no, discover / augment / generate (Lecture 01); if yes, **preprocess** — EDA → cleaning → transformation → feature engineering — then train, evaluate, and iterate. The iteration loop ("improve label, data, or model?") usually sends you *back into this lecture*, not forward to a fancier model. The data is the lever.

---

> **2026 update:** Three shifts since the 2021 slides. **(1) Deep nets killed hand feature-engineering for unstructured data** — nobody hand-codes SIFT or designs BoW vocabularies for production NLP anymore; you fine-tune a pretrained transformer or pull embeddings. But for **tabular data — still the majority of industry ML — feature engineering remains decisive**, and gradient-boosted trees (XGBoost/LightGBM) on well-crafted features routinely beat deep nets. **(2) Feature stores** — Feast (open-source) and Tecton (managed) now own the "compute a feature once, serve it consistently to training and inference" problem, killing the train/serve skew that used to silently wreck deployments (you computed a feature one way in your notebook and another way in the serving path). **(3) Data-centric AI** — Andrew Ng's reframing that *systematically improving the data beats tweaking the model* turned the "unglamorous" cleaning/labeling/augmentation work of this lecture into a first-class methodology with tooling (cleanlab for label errors, weak-supervision frameworks). The lecture's thesis aged extremely well; the field caught up to it. On tokenization, subword (BPE/WordPiece/Unigram) is now universal, and stemming/lemmatization have largely faded for neural pipelines.

> **Hardware lens:** Preprocessing is not free setup — it is a throughput stage that competes with training for compute, and on a fast accelerator it is usually the bottleneck. The classic failure: a multi-GPU training run sits at 40% utilization because a handful of CPU cores can't JPEG-decode, resize, and augment images fast enough to feed it — the GPUs starve waiting on the input pipeline. Fixes are an engineering discipline of their own: prefetch and overlap (double-buffer the next batch while the GPU computes the current one), cache the decoded/resized tensors so you pay the cost once not every epoch, store in a sequential format (TFRecord / WebDataset / Parquet) so you read streams not millions of tiny files, and — the big one — **push decode/resize/augment onto the GPU itself** with NVIDIA **DALI**, which can turn a CPU-bound pipeline into a GPU-bound one and recover that idle utilization. The 15 TB → 1.4 TB resize from §2.2 is the same lesson at rest: smaller inputs mean less I/O, less decode, faster epochs. Measure input-pipeline throughput in samples/s alongside model FLOPs; a model that *could* train at 5,000 img/s but is fed at 1,200 img/s is a data-loading problem wearing a model costume.

---

**Current as of**

Written June 2026. The **original CS329P content** — the three-way error taxonomy (outliers / rule / pattern violations), the four normalizers, the image/video/text transformation methods, and the tabular/text/image feature-engineering catalog — is taught first because it is still the correct working mental model and maps one-to-one onto the 2021 slides. The **refresh layer** flags what moved: the dominance of learned over hand-crafted features for unstructured data (while affirming feature engineering's continued primacy for tabular), the arrival of **feature stores** (Feast/Tecton) as standard infrastructure, the **data-centric AI** reframing, subword tokenization as universal, and the **DALI / GPU-side preprocessing** throughput story that the original slides — focused on storage cost — only gestured at. Where 2021 framing is dated, the original is presented before the update; nothing is silently rewritten.

*Adapted from [Stanford CS329P](https://c.d2l.ai/stanford-cs329p) — Huang, Li & Smola, CC-BY-SA-4.0.*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/4. ML Engineering and MLOps/Practical Machine Learning (CS329P)/Lecture-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/4.%20ML%20Engineering%20and%20MLOps/Practical%20Machine%20Learning%20%28CS329P%29/Lecture-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
