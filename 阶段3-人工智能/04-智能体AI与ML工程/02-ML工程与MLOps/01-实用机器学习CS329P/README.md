---
title: Practical Machine Learning — 改编自 Stanford CS329P
description: Practical Machine Learning — 改编自 Stanford CS329P
---

# Practical Machine Learning — 改编自 Stanford CS329P

<div class="course-identity ai-workloads" markdown="1">
<div class="course-identity__icon">PML</div>
<div markdown="1">
<p class="course-identity__eyebrow">阶段 3 · ML Engineering & MLOps · 专题课程</p>
<p class="course-identity__title">完整的机器学习生命周期，按从业者实际遭遇它的样子展开——数据、模型、验证、分布偏移、调优、压缩与负责任的部署——改编自 Stanford CS329P，并为 2026 年做了更新。</p>
<p class="course-identity__meta">产物：在单个数据集 + 单个模型上完成的一项端到端 ML 研究 · 度量：经检验的泛化能力、对偏移的鲁棒性，以及一个命中延迟/内存预算的压缩模型</p>
</div>
</div>

> *“机器学习中重要却常被跳过的那些主题。”* —— CS329P 的开篇目标。那些著名课程教你拟合一个模型。这门课教的是围绕拟合的一切：数据从哪来、它如何骗你、世界如何偏离你的训练集，以及把它交付出去要付出什么代价。

本课程改编自 **Stanford CS329P — Practical Machine Learning**（2021 秋季）——作者为 **Qingqing Huang、Mu Li 与 Alex Smola**，即 [*Dive into Deep Learning*](https://d2l.ai/) 背后的团队。原课程以三条关注点构成的主干来讲授：**数据 → 模型训练 → 部署**。本课程保留这条主干，让每一讲都扎根于原始幻灯片内容，并增加一层 **2026 更新层**——因为数据工具链、调优实践，尤其是压缩技术栈自 2021 年以来已经大幅演进（INT4 大语言模型量化、GPTQ/AWQ、现代蒸馏、手工 NAS 的崩塌）。

这门课会让你在真实的 ML 岗位上变得危险。大多数工程师会调用 `model.fit()`。能告诉你 *为什么他们的验证分数在说谎*、察觉生产流量已经漂移、或是把模型缩小 4× 而不损失当初支撑它的准确率的人，则少得多。这道鸿沟，就是本课程大纲。

**层级映射：** ML 工程层——位于研究模型与已上线服务产品之间的实践。它直接对接 [Module 4B — ML Engineering & MLOps](../Guide.md)，以及 [Phase 5 — ML Systems Engineering](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/Guide.md) 中的推理服务/压缩工作。

**角色定位：** 机器学习工程师 · 应用科学家 · MLOps 工程师 · 数据科学家（建模方向） · ML 平台工程师。

**前置要求：**

* 熟练使用 Python，具备基础统计学与基础 ML 知识（知道损失函数与梯度下降是什么）。
* [Module 2 — Deep Learning Frameworks](../../../02-深度学习框架/Guide.md)，对应模型与压缩相关讲次所用的 PyTorch。
* 无需 MLOps 前置知识——这门课*本身*就是它的入门坡道。

**配套：** [Module 4B — ML Engineering & MLOps](../Guide.md)（交付的基础设施侧），以及，就第 10 讲的硬件收益而言，[Phase 5 — Model Compression and the inference stack](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/README.md)。

---

## 这门课为什么这样组织

研究型课程为模型做优化。*实用*课程为模型撞上现实的那一刻做优化——而现实总是按固定顺序发起攻击：

```text
   1. your data is dirty, unlabeled, and not yet features     → Lectures 01–02
   2. you pick and fit a model                                → Lecture 03
   3. your validation score is optimistic and you don't know  → Lectures 04–05
   4. production data drifts away from training                → Lectures 06–07
   5. the model is too slow / too big / under-tuned            → Lectures 08, 10
   6. you need to reuse pretrained knowledge, not start cold   → Lecture 09
   7. the model must be multimodal, fair, and explainable      → Lecture 11
```

每一讲都是这些碰撞中的一次。贯穿的线索是 **现实条件下的泛化**：不是“它能否拟合训练集”，而是“当数据脏乱、发生偏移、多模态，且要在预算内运行时，它是否仍然管用”。

---


<details>
<summary>English original</summary>

**Practical Machine Learning — adapted from Stanford CS329P**

<div class="course-identity ai-workloads" markdown="1">
<div class="course-identity__icon">PML</div>
<div markdown="1">
<p class="course-identity__eyebrow">Phase 3 · ML Engineering & MLOps · Special Course</p>
<p class="course-identity__title">The full machine-learning lifecycle the way a practitioner actually meets it — data, models, validation, distribution shift, tuning, compression, and responsible deployment — adapted from Stanford CS329P and refreshed for 2026.</p>
<p class="course-identity__meta">Artifact: an end-to-end ML study on one dataset + one model · Measure: validated generalization, robustness to shift, and a compressed model that hits a latency/memory budget</p>
</div>
</div>

> *"Machine learning topics that matter but are often skipped."* — the opening goal of CS329P. The famous courses teach you to fit a model. This one teaches you everything that surrounds the fit: where data comes from, how it lies to you, how the world drifts away from your training set, and what it costs to ship.

This course is an adaptation of **Stanford CS329P — Practical Machine Learning** (2021 Fall) by **Qingqing Huang, Mu Li, and Alex Smola** — the team behind [*Dive into Deep Learning*](https://d2l.ai/). The original is taught as a spine of three concerns: **Data → Model training → Deployment**. We keep that spine, ground every lecture in the original slide content, and add a **2026 refresh layer** — because the data tooling, the tuning practice, and especially the compression stack have moved hard since 2021 (INT4 LLM quantization, GPTQ/AWQ, modern distillation, the collapse of hand-rolled NAS).

This is the course that makes you dangerous in a real ML role. Most engineers can call `model.fit()`. Far fewer can tell you *why their validation score lied*, detect that production traffic has drifted, or shrink a model 4× without losing the accuracy that justified it. That gap is this syllabus.

**Layer mapping:** the ML-engineering layer — the practice that sits between a research model and a served product. It feeds directly into [Module 4B — ML Engineering & MLOps](../Guide.md) and the serving/compression work in [Phase 5 — ML Systems Engineering](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/Guide.md).

**Role targets:** Machine Learning Engineer · Applied Scientist · MLOps Engineer · Data Scientist (modeling track) · ML Platform Engineer.

**Prerequisites:**

* Python fluency, basic statistics, and basic ML (what a loss function and gradient descent are).
* [Module 2 — Deep Learning Frameworks](../../../02-深度学习框架/Guide.md) for the PyTorch used in the model and compression lectures.
* No prior MLOps knowledge needed — this course *is* the on-ramp to it.

**Pairs with:** [Module 4B — ML Engineering & MLOps](../Guide.md) (the infrastructure side of shipping) and, for the hardware payoff of Lecture 10, [Phase 5 — Model Compression and the inference stack](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/README.md).

---

**Why this course is structured the way it is**

A research course optimizes for the model. A *practical* course optimizes for the moment the model meets reality — and reality attacks in a fixed order:

```text
   1. your data is dirty, unlabeled, and not yet features     → Lectures 01–02
   2. you pick and fit a model                                → Lecture 03
   3. your validation score is optimistic and you don't know  → Lectures 04–05
   4. production data drifts away from training                → Lectures 06–07
   5. the model is too slow / too big / under-tuned            → Lectures 08, 10
   6. you need to reuse pretrained knowledge, not start cold   → Lecture 09
   7. the model must be multimodal, fair, and explainable      → Lecture 11
```

Every lecture is one of these collisions. The thread is **generalization under reality**: not "does it fit the training set" but "does it still work when the data is messy, shifted, multimodal, and running on a budget."

---

</details>

## 课程地图（11 讲）

<div class="lecture-map" markdown>

| # | 讲座 | 线索 |
|---|---------|-----------|
| [01](Lecture-01.md) | **数据 I — 采集、爬取与标注** — 数据从哪来、网页爬取，以及标注技术栈（主动学习、弱监督、半监督） | 获取数据 |
| [02](Lecture-02.md) | **数据 II — 清洗、变换与特征工程** — 脏数据分类法、归一化，以及面向表格 / 文本 / 图像的特征 | 数据 → 特征 |
| [03](Lecture-03.md) | **ML 模型回顾 — 树模型、线性模型、神经网络** — 实践者的常用工具集：每种模型何时才是正确选择 | 模型动物园 |
| [04](Lecture-04.md) | **模型验证与评估** — 与问题匹配的指标、欠拟合 / 过拟合，以及不对你说谎的验证 | 信任分数 |
| [05](Lecture-05.md) | **模型组合 — Bagging、Boosting、Stacking** — 偏差–方差，以及在 Kaggle 和生产环境同样奏效的三种模型组合方式 | 集成 |
| [06](Lecture-06.md) | **分布偏移 — 协变量偏移与标签偏移** — 生产环境准确率为何衰减、用双样本检验检测，以及重要性加权修正 | 世界会漂移 |
| [07](Lecture-07.md) | **超越 IID 的数据 — 序列与图** — 独立性检验、序列模型，以及样本不独立时的图 / GNN 结构 | 结构化数据 |
| [08](Lecture-08.md) | **模型与超参数调优 — HPO、NAS、深度网络调优** — 搜索算法、NAS 的兴与衰，以及 norm/residual/attention 工具集 | 调优 |
| [09](Lecture-09.md) | **迁移学习 — CV、NLP、提示** — 把微调作为默认工作流，以及从特征提取到基于提示的学习这条线 | 复用，不要重来 |
| [10](Lecture-10.md) | **模型压缩 — 剪枝、量化、蒸馏** — 面向硬件的一讲：把模型压到延迟 / 内存 / 能耗预算之内 | 小体积交付 |
| [11](Lecture-11.md) | **多模态、公平性与可解释性** — 融合模态、公平性准则及其不可能性，以及让模型解释自己 | 负责任地部署 |

</div>

---

## 课程目标

学完本课程，你应当能够：

* 搭建一条从采集到清洗、变换与特征工程的 **数据流水线**，并说出每个阶段防范的失效模式。
* 挑选与问题（类别不平衡、时序数据、小数据）匹配的 **评估指标与验证方案**，而不是默认用准确率加随机划分。
* **检测并修正分布偏移** — 区分协变量偏移与标签偏移，跑双样本检验判断它正在发生，并用重要性加权修正。
* 有章法地做 **超参数搜索与深度网络调优**，并解释为何 NAS 衰落而迁移学习胜出。
* 用剪枝、量化与蒸馏 **压缩模型**，达到目标延迟 / 内存 / 能耗预算，并用数字报告准确率取舍 — 这项技能把本课程与路线图其余部分连接起来。
* 对 **公平性准则** 做推理（以及为何无法同时满足全部准则），并对模型的预测给出基本的 **解释**。

---

## 来源与时效性

本课程是 **改编，而非复制。** 源材料 — 讲座结构、示例，以及每讲所依托的幻灯片内容 — 来自 Qingqing Huang、Mu Li 与 Alex Smola 的 **Stanford CS329P（2021 秋季）**，以 **CC-BY-SA-4.0**（幻灯片）与 **MIT-0**（notebook）发布。本改编版以相同的 **CC-BY-SA-4.0** 条款共享。

* 原课程：**<https://c.d2l.ai/stanford-cs329p>** · 配套教材：**[D2L](https://d2l.ai/)**。
* 每讲结尾都有一段 **`## Current as of`** 说明，标出哪些内容为 2026 年做了更新、哪些仍按原始 CS329P 材料讲授 — 更新最集中的是 **第 08 讲（NAS）**、**第 09 讲（prompting → instruction tuning）** 和 **第 10 讲（LLM 时代的量化 / 蒸馏）**。
* 当 2021 年的表述如今已过时，先讲授原始内容（它仍是正确的心智模型），再明确标出更新之处。不会悄悄改写历史。

---


<details>
<summary>English original</summary>

**Course Map (11 lectures)**

<div class="lecture-map" markdown>

| # | Lecture | The thread |
|---|---------|-----------|
| [01](Lecture-01.md) | **Data I — Acquisition, Scraping & Labeling** — where data comes from, web scraping, and the labeling stack (active learning, weak supervision, semi-supervised) | getting data |
| [02](Lecture-02.md) | **Data II — Cleaning, Transformation & Feature Engineering** — dirty-data taxonomy, normalization, and features for tabular / text / image | data → features |
| [03](Lecture-03.md) | **ML Models Recap — Trees, Linear, Neural Nets** — the practitioner's working set: when each model is the right call | the model zoo |
| [04](Lecture-04.md) | **Model Validation & Evaluation** — the metrics that match the problem, under/overfitting, and validation that doesn't lie to you | trusting the score |
| [05](Lecture-05.md) | **Model Combination — Bagging, Boosting, Stacking** — bias–variance, and the three ways to combine models that win Kaggle and production alike | ensembling |
| [06](Lecture-06.md) | **Distribution Shift — Covariate & Label Shift** — why production accuracy decays, detection via two-sample tests, and importance-weighting correction | the world drifts |
| [07](Lecture-07.md) | **Data Beyond IID — Sequences & Graphs** — independence tests, sequence models, and graph/GNN structure when samples aren't independent | structured data |
| [08](Lecture-08.md) | **Model & Hyperparameter Tuning — HPO, NAS, Deep-Net Tuning** — search algorithms, the rise and fall of NAS, and the norm/residual/attention toolkit | tuning |
| [09](Lecture-09.md) | **Transfer Learning — CV, NLP, Prompting** — fine-tuning as the default workflow, and the line from feature-extraction to prompt-based learning | reuse, don't restart |
| [10](Lecture-10.md) | **Model Compression — Pruning, Quantization, Distillation** — the hardware-facing lecture: shrink a model to a latency/memory/energy budget | shipping it small |
| [11](Lecture-11.md) | **Multimodal, Fairness & Explainability** — fusing modalities, fairness criteria and their impossibility, and making a model explain itself | responsible deployment |

</div>

---

**Course Outcomes**

By the end you should be able to:

* Stand up a **data pipeline** from acquisition through cleaning, transformation, and feature engineering, and name the failure mode each stage guards against.
* Pick an **evaluation metric and validation scheme** that match the problem (imbalanced classes, time-ordered data, small data) instead of defaulting to accuracy + a random split.
* **Detect and correct distribution shift** — distinguish covariate shift from label shift, run a two-sample test to know it's happening, and apply importance weighting to fix it.
* Run **hyperparameter search and deep-network tuning** deliberately, and explain why NAS faded while transfer learning won.
* **Compress a model** with pruning, quantization, and distillation to hit a target latency/memory/energy budget, and report the accuracy tradeoff with numbers — the skill that connects this course to the rest of the roadmap.
* Reason about **fairness criteria** (and why you can't satisfy all of them at once) and produce a basic **explanation** of a model's prediction.

---

**Attribution & Currency**

This course is an **adaptation, not a copy.** Source material — lecture structure, examples, and the slide content each lecture is grounded in — is **Stanford CS329P (2021 Fall)** by Qingqing Huang, Mu Li, and Alex Smola, released under **CC-BY-SA-4.0** (the slides) and **MIT-0** (the notebooks). This adaptation is shared under the same **CC-BY-SA-4.0** terms.

* Original course: **<https://c.d2l.ai/stanford-cs329p>** · companion textbook: **[D2L](https://d2l.ai/)**.
* Each lecture closes with a **`## Current as of`** note marking what was refreshed for 2026 versus what is taught as the original CS329P material — most heavily in **Lecture 08 (NAS)**, **Lecture 09 (prompting → instruction tuning)**, and **Lecture 10 (LLM-era quantization/distillation)**.
* Where 2021 framing is now dated, the original is taught first (it is still the right mental model), then the update is flagged explicitly. We do not silently rewrite history.

---

</details>

## 达成标准

当你能够把**一个数据集和一个模型端到端**走完，这门课就算学完了：

* 获取它、清洗它、做特征工程，并说明每一次变换的理由。
* 用一套经得起推敲的方案验证它——并且在看到分数*之前*就说出指标是什么。
* 探查它的分布偏移，说明训练与生产是否为同一分布。
* 调优它，然后**把它压缩到部署预算**，并报告每一档的 tokens/s（或延迟）与准确率。
* 就它的公平性说一句诚实的话，就它为何做出某个预测说一句诚实的话。

如果只能拟合一个模型，却做不了围绕这次拟合的那十件事，那你手里只有一个 notebook。这门课的重点是另外那十件事。

---

*Related: [模块 4B — ML Engineering & MLOps](../Guide.md) · [阶段 5 — ML Systems Engineering](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/Guide.md) · original [Stanford CS329P](https://c.d2l.ai/stanford-cs329p)*


<details>
<summary>English original</summary>

**Exit Criteria**

You are done with this course when you can take **one dataset and one model end to end**:

* Acquire it, clean it, engineer features, and justify each transformation.
* Validate it with a scheme that survives scrutiny — and state the metric *before* you look at the score.
* Probe it for distribution shift and show whether train and production are the same distribution.
* Tune it, then **compress it to a deployment budget** and report tokens/s (or latency) and accuracy at each rung.
* Say one honest sentence about its fairness and one about why it made a given prediction.

If you can fit a model but can't do the ten things around the fit, you have a notebook. The point of this course is the other ten things.

---

*Related: [Module 4B — ML Engineering & MLOps](../Guide.md) · [Phase 5 — ML Systems Engineering](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/Guide.md) · original [Stanford CS329P](https://c.d2l.ai/stanford-cs329p)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/4. ML Engineering and MLOps/Practical Machine Learning (CS329P)/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/4.%20ML%20Engineering%20and%20MLOps/Practical%20Machine%20Learning%20%28CS329P%29/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
