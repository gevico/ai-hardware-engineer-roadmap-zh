---
title: 第 11 讲 - 多模态、公平性与可解释性
description: 第 11 讲 - 多模态、公平性与可解释性
---

# 第 11 讲 - 多模态、公平性与可解释性

**合集：** [Practical Machine Learning (CS329P)](README.md) | **上一讲：** [← 第 10 讲](Lecture-10.md) | **下一讲：** [课程索引](README.md)

---

之前的每一讲都假设了一个整洁的接口：输入张量，输出一个数字，你优化这个数字。实践机器学习的最后一英里，正是这一假设与现实发生冲突的地方。真实数据不会以单一干净模态的形式出现——房屋列表是表格*和*照片*和*自由文本，自动驾驶汽车融合摄像头、激光雷达和雷达，Amazon 产品页面是图像加评论加类别图。真实部署也不是在真空中行动：其输出决定谁获得贷款、谁获得保释、谁看到招聘广告——而同一个取得出色 AUC 的模型，可能对受保护群体安静地、系统性地不公平。而真实的利益相关者——监管者、被拒绝的申请人、调试工程师——不接受“网络这么说的”；他们要求知道*为什么*。

本讲整合了 CS329P 中支配**负责任部署**的三个主题：多模态数据（消费混乱的真实世界）、公平性（公平对待人）和可解释性（能够为决策提供理由）。它们共享一个论点。每一个都是流水线中如果被跳过，不会表现为糟糕的损失曲线的部分——它会在之后表现为一个漏掉一半信号的模型、让你的产品被禁、或让你的公司被起诉。准确率是必要的；但不充分。下面的工作就是介于一个离线得分良好的模型和一个在与世界及其法律接触后存活下来的模型之间的东西。

第二条线索贯穿所有三者：**常识和对问题的理解**。融合策略、公平性标准和特征归因都是技术机制，但每一个都编码了判断——什么算“相似”，什么算“公平”，什么算“原因”。机制不会替你做出这些判断。它只是让它们足够明确，以便争论。

---

## 学习目标

到本讲结束时，你应该能够：

1. **定义多模态数据**，并针对给定的表格、文本和图像组合，在早期、晚期和中期融合之间选择。
2. **解释对比式图像-文本预训练**（CLIP 风格）以及为什么文本监督产生比 ImageNet 标签迁移更好的特征。
3. **说出公平性背后的法律框架**（受保护属性；差别对待 vs. 差别影响），并将它们与现实世界的危害联系起来。
4. **陈述形式化的公平性标准**——人口统计精度一致性、均衡几率、校准/预测精度一致性——并解释它们不能同时成立的**不可能性结果**。
5. **区分解释策略**沿两个轴（内在 vs. 事后，全局 vs. 局部），并识别虚假相关（“Clever Hans”）失败。
6. **比较归因方法**——公理式（SHAP、Integrated Gradients）vs. 启发式（LIME、saliency、Grad-CAM）——并判断每种方法何时可以信任、何时不能。

---

## 1. 多模态数据

### 1.1 “多模态”的含义

在行业应用中，数据天然是**多模态的**——原始记录包含表格、文本、图像、音频、图，常常同时出现。CS329P 的贯穿示例具体说明了这一点：

- **房屋销售**——表格属性（卧室、浴室、邮编）、自由文本描述和房源照片。
- **Amazon 产品**——图像 + 文本 + 表格，外加一个类别/共同购买**图**。
- **自动驾驶汽车**——摄像头图像、激光雷达点云、雷达，全部带有时间戳并空间对齐。

仅在这些通道之一上训练的单模态模型，会漏掉信号。工程问题是如何组合模态，使模型看到比任何单一通道提供的更多信息。


<details>
<summary>English original</summary>

**Lecture 11 - Multimodal, Fairness & Explainability**

**Collection:** [Practical Machine Learning (CS329P)](README.md) | **Previous:** [← Lecture 10](Lecture-10.md) | **Next:** [Course index](README.md)

---

Every earlier lecture assumed a tidy interface: a tensor goes in, a number comes out, you optimize the number. The last mile of practical ML is where that assumption breaks against reality. Real data does not arrive as one clean modality — a house listing is tables *and* photos *and* free text, a self-driving car fuses camera, lidar, and radar, an Amazon product page is images plus reviews plus a category graph. A real deployment does not act in a vacuum either: its outputs decide who gets a loan, who gets bail, who sees a job ad — and the same model that posts a great AUC can be quietly, systematically unfair to a protected group. And a real stakeholder — a regulator, a denied applicant, a debugging engineer — does not accept "the network said so"; they demand to know *why*.

This lecture consolidates the three CS329P topics that govern **responsible deployment**: multimodal data (consuming the messy real world), fairness (treating people equitably), and explainability (being able to justify a decision). They share a thesis. Each is the part of the pipeline that, if skipped, does not show up as a bad loss curve — it shows up later as a model that misses half its signal, gets your product banned, or gets your company sued. Accuracy is necessary; it is not sufficient. The work below is what stands between a model that scores well offline and a model that survives contact with the world and its laws.

A second thread runs underneath all three: **common sense and understanding the problem**. Fusion strategy, fairness criterion, and feature attribution are all technical machinery, but every one of them encodes a judgment call — what counts as "similar," what counts as "fair," what counts as "the reason." The machinery does not make those calls for you. It only makes them explicit enough to argue about.

---

**Learning objectives**

By the end of this lecture you should be able to:

1. **Define multimodal data** and choose between early, late, and intermediate fusion for a given mix of tables, text, and images.
2. **Explain contrastive image-text pretraining** (CLIP-style) and why text supervision yields features that transfer better than ImageNet labels.
3. **Name the legal frameworks** behind fairness (protected attributes; disparate treatment vs. disparate impact) and connect them to real-world harms.
4. **State the formal fairness criteria** — demographic parity, equalized odds, calibration/predictive parity — and explain the **impossibility result** that they cannot all hold at once.
5. **Distinguish explanation strategies** along two axes (intrinsic vs. post-hoc, global vs. local) and recognize spurious-correlation ("Clever Hans") failures.
6. **Compare attribution methods** — axiomatic (SHAP, Integrated Gradients) vs. heuristic (LIME, saliency, Grad-CAM) — and judge when each can and cannot be trusted.

---

**1. Multimodal data**

**1.1 What "multimodal" means**

Data is naturally **multimodal** in industry applications — the raw record contains tables, text, images, audio, graphs, often all at once. CS329P's running examples make the point concretely:

- **House sales** — tabular attributes (beds, baths, zip), free-text description, and listing photos.
- **Amazon product** — images + text + tabular, plus a category/co-purchase **graph**.
- **Self-driving cars** — camera images, lidar point clouds, radar, all timestamped and spatially aligned.

A unimodal model trained on only one of these channels is leaving signal on the table. The engineering question is how to combine modalities so the model sees more than any single channel offers.

</details>

### 1.2 融合策略

多模态学习的核心问题是**如何把不同模态的数据匹配到同一语义空间**，然后如何在这个组合表示上构造损失。不同策略的差别在于模态在*哪里*汇合。

| Strategy | Where modalities combine | Mechanism | Best when |
|---|---|---|---|
| **早期融合** | 在输入处 | 拼接原始特征 / 浅层 embedding，送入单个模型 | 模态耦合紧密；单个模型就能学到跨模态交互 |
| **中间融合** | 在某个隐藏层 | 每个模态各有自己的编码器；合并中层表示，再联合处理 | 既想要模态专属的特征提取器，*又*想要学到的交互——深度学习常见的默认做法 |
| **晚期融合** | 在输出处 | 为每个模态单独训练一个模型（“塔”）；再组合它们的预测（如平均、stacking） | 模态耦合松散；需要模块化、可独立训练的组件 |

在 CS329P 的记号里，**早期融合**把 `[text, image]` 推过一个深层网络，而**晚期融合**分别跑一个 `Tower A` 和一个 `Tower B`，最后再合并。中间融合介于两者之间：各模态的编码器接同一个共享 head。

哪种更好？课上的实证答案是**两者很接近，在他们的实验中晚期融合略占优势**。在一个表格 + 文本的 benchmark 上（使用 ELECTRA 文本编码器的浅层变体，Shi et al., NeurIPS '21），在 13 个数据集上取平均：

- **早期融合：0.662**
- **晚期融合：0.667**（越大越好）

差距很小。真正好用的抓手往往不是融合点本身，而是**在其上做 ensemble**——把多模态网络与其他基座模型 stacking，把同一个 benchmark 从 0.667 推到 **0.683**，超过 AutoGluon 的 n-gram 文本路径（0.659）或 H2O Word2vec 路径（0.600）这类单一策略基线。

### 1.3 对齐模态并构造损失

在融合后的模态上构造训练目标，有两种方式：

- **联合标签学习**——合并各模态，端到端训练去预测一个共享标签（监督式）。
- **对比学习**——学出一个 embedding 空间，在其中**相似的样本对彼此靠近，不相似的被推远**。这是自监督的：“标签”仅仅是两样东西是否应当归在一起。它就是下文“从文本监督图像”背后的引擎。

### 1.4 图像 + 文本：对比预训练（CLIP 风格）

这个时期最抢眼的多模态成果是**用文本监督来学图像表示**。**CLIP**（Radford et al., ICML '21）把一个图像编码器（ResNet 或 ViT）与一个文本编码器（GPT 风格的 Transformer）配对，在从网上爬取的 **300M 个 (image, text) 对**上用对比目标训练：一张图像的 embedding 应当靠近其 caption 的 embedding，并远离同一批内其他所有 caption。

回报是，得到的特征**与在 ImageNet 上训练的特征相当甚至更好**，尽管从未见过一个手工标注的类别标签。自然语言监督比固定标签集更丰富——“a photo of a corgi on a skateboard”所携带的结构比类别索引 `263` 更多——而且它能扩展到网上已经附在图像上的任何文本。

同一套对比做法可以推广到各种模态对。**VideoBERT**（Sun et al., ICCV '19）对齐**视频 + 音频**：它取音频轨上自动语音识别（ASR）得到的文本，与视频帧配对，在约 23K 小时的烹饪/食谱 YouTube 视频上训练。从语音得到的文本成了视频理解的免费监督。

### 1.5 挑战

- **对齐**——把两个模态弄进*同一个*语义空间才是难的部分。一个像素和一个 token 生活在完全不同的空间里；编码器必须学出一种共享的几何，让“dog”这个词和一张狗的照片落在彼此附近。对不齐会悄无声息地拖累下游的一切。
- **模态缺失**——推理时，一条记录可能缺照片、缺评论或缺传感器读数。假设所有通道都在的模型可能崩得很惨；鲁棒的多模态系统必须在某个模态掉线时优雅降级（晚期融合在这里有帮助——缺的那座塔可以跳过）。
- **规模与噪声**——网上爬来的配对数量多但很脏；对比目标对噪声的容忍度远高于要求干净标签的方案，这正是它能 scale 的原因。

> **CS329P 给出的总结：** 真实数据常常是多模态的；用早期或晚期融合把每个模态投影到共同空间；然后要么联合学习标签，要么用对比学习做自监督训练。

---

## 2. 公平性


<details>
<summary>English original</summary>

**1.2 Fusion strategies**

The core problem of multimodal learning is **how to match different-modal data into the same semantic space**, then how to construct a loss over the combined representation. The strategies differ in *where* the modalities meet.

| Strategy | Where modalities combine | Mechanism | Best when |
|---|---|---|---|
| **Early fusion** | At the input | Concatenate raw features / shallow embeddings, feed one model | Modalities are tightly coupled; a single model can learn cross-modal interactions |
| **Intermediate fusion** | At a hidden layer | Each modality gets its own encoder; merge the mid-level representations, then jointly process | You want modality-specific feature extractors *and* learned interaction — the common deep-learning default |
| **Late fusion** | At the output | Train a separate model ("tower") per modality; combine their predictions (e.g. average, stack) | Modalities are loosely coupled; you want modular, independently-trainable components |

In CS329P's notation, **early fusion** pushes `[text, image]` through one deep network, while **late fusion** runs a `Tower A` and a `Tower B` separately and combines at the end. Intermediate fusion sits between: per-modality encoders feeding a shared head.

Which wins? The empirical answer from the lecture is **it's close, and late fusion has a slight edge** in their experiments. On a tabular + text benchmark (a shallow variant using an ELECTRA text encoder, Shi et al., NeurIPS '21), averaged over 13 datasets:

- **Early fusion: 0.662**
- **Late fusion: 0.667** (larger is better)

The gap is small. The practical lever is often **ensembling on top** rather than the fusion point itself — stacking the multimodal net with other base models pushed the same benchmark from 0.667 to **0.683**, beating single-strategy baselines like AutoGluon's n-gram text path (0.659) or an H2O Word2vec path (0.600).

**1.3 Aligning modalities and constructing the loss**

Two ways to construct the training objective over fused modalities:

- **Joint label learning** — combine the modalities and train to predict a shared label end-to-end (supervised).
- **Contrastive learning** — learn an embedding space in which **similar sample pairs stay close and dissimilar ones are pushed far apart**. This is self-supervised: the "label" is just whether two things belong together. It is the engine behind image-from-text supervision below.

**1.4 Image + text: contrastive pretraining (CLIP-style)**

The headline multimodal result of the era is learning **image representations from text supervision**. **CLIP** (Radford et al., ICML '21) pairs an image encoder (ResNet or ViT) with a text encoder (a GPT-style transformer) and trains on **300M (image, text) pairs** scraped from the web with a contrastive objective: the embedding of an image should be close to the embedding of its caption and far from every other caption in the batch.

The payoff is that the resulting features are **comparable to or better than features trained on ImageNet**, despite never seeing a single hand-drawn class label. Natural-language supervision is richer than a fixed label set — "a photo of a corgi on a skateboard" carries more structure than the class index `263` — and it scales to whatever text the web already attaches to images.

The same contrastive recipe generalizes across modality pairs. **VideoBERT** (Sun et al., ICCV '19) aligns **video + audio**: it takes text from automatic speech recognition (ASR) on the audio track and pairs it with video frames, trained on ~23K hours of cooking/recipe YouTube videos. Text-from-speech becomes free supervision for video understanding.

**1.5 Challenges**

- **Alignment** — getting two modalities into the *same* semantic space is the hard part. A pixel and a token live in totally different spaces; the encoders must learn a shared geometry where "dog" the word and a dog photo land near each other. Misalignment quietly degrades everything downstream.
- **Missing modalities** — at inference a record may lack a photo, a review, or a sensor reading. A model that assumed all channels are present can fail badly; robust multimodal systems must degrade gracefully when a modality drops out (late fusion helps here — a missing tower can be skipped).
- **Scale and noise** — web-scraped pairs are abundant but noisy; the contrastive objective tolerates noise far better than a scheme that demands clean labels, which is precisely why it scaled.

> **The summary CS329P lands on:** real data is often multimodal; project each modality into a common space via early or late fusion; then either jointly learn labels or use contrastive learning for self-supervised training.

---

**2. Fairness**

</details>

### 2.1 真实世界的危害

公平性并非抽象概念——有偏模型已经伤害了人们，有时长达一个世纪。CS329P 以案例开篇：

- **辛普森悖论（UC Berkeley 招生，1973）**——总体录取率看起来对女性有偏，然而*按院系*看，偏差反转或消失。女性不成比例地申请了竞争更激烈的院系。总体数据会骗人；你必须对正确的变量进行条件化。
- **COMPAS（ProPublica，2016）**——一种累犯风险工具为黑人被告与白人被告分配了系统性不同的风险评分分布，导致**黑人被告的假阳性率更高**（在 Broward County，那些*没有*再犯的人中，31% 对 15% 被标记为高风险）。
- **在线广告中的偏见**（Lambrecht & Tucker）——一个表面上中立的广告投放系统向男性展示了比女性更多的 STEM 职位广告，出于与意图无关的经济原因。
- **贷款中的偏见**（Martinez & Kirchner，2021）——现代抵押贷款审批算法显示出按种族的差异。
- **红线制度**——20 世纪 30 年代基于位置的信贷拒绝歧视了少数族裔，其影响**在 90 年后仍然可测量**。系统内嵌的偏见会代际传播。

讲座直白地给出的教训是：**偏见与公平性的缺失可以伤害人们长达一个世纪——我们有责任对每个人保持审慎。**

### 2.2 法律框架

公平性有一个法律基础，它约束已部署模型被允许做什么。**受保护属性**是法律禁止据以歧视的特征。美国联邦法律：

- **《民权法案》第七章（1964）**——禁止基于**种族、肤色、宗教、民族血统或性别**的歧视（并保护提出申诉的人免遭报复）。
- **《怀孕歧视法案》**——将“性别”扩展到怀孕和分娩。
- **《同工同酬法案》（1963）**——禁止同等工作中基于性别的工资歧视。
- **《就业年龄歧视法案》（1967）**——保护**40 岁及以上**的劳动者。
- **《美国残疾人法案》第一章（1990）**——禁止歧视合格的残疾人。

欧洲的 **GDPR** 增加了数据保护以及围绕自动化决策的解释权。监管机构正朝着**要求证明招聘算法无偏的法律要求**迈进——这立即引出了本节其余部分要回答的问题：*“无偏”到底意味着什么？*

每个 ML 工程师都应掌握的两个法律概念：

- **差别性对待**——*故意*使用受保护属性（或因它而区别对待人们）。大致是**反分类**的思想：不要把 `race` 放进模型。
- **差别性影响**——一项表面上中立的政策，却在不同群体间产生*不平等结果*，无论意图如何。这对 ML 来说是危险的那个：你可以用一个从未见过受保护属性的模型触发差别性影响，因为**次级属性**（邮政编码、姓名、脏辫 vs. 牛津衬衫、“小马 vs. 摩托车”）会代理它。

### 2.3 跨群体的风险分布

Corbett-Davies & Goel 的框架（ICML 2019 教程）是最清晰的心智模型。对于像“搜查这辆车是否有违禁品？”这样的决策，每个群体都有一个**风险分布**——`p(contraband | x)`在其成员上的分布，从 0 到 1。关于这些分布的两个事实驱动一切：

- **不同的平均风险** = 实际携带违禁品的不同基础率。
- **不同的方差** = 不同的*辨别谁*在携带的能力——方差越低，在该群体内区分有罪与无辜就越难。

这暴露了朴素**“结果测试”**（用命中率判断公平性）的陷阱。考虑两个群体，都在 30% 的时间里携带违禁品，在表面上中立的政策下：*如果概率 > 50% 就搜查*。**方差更高**的风险群体有更多“明显有罪”的成员高于阈值，因此其**命中率结果更高**——而结果测试会错误地指责对另一群体有偏见。反过来，一个真正歧视的政策（以更低的阈值搜查一个群体，45% vs. 50%）可以被调整得**命中率结果相等**，而结果测试会错误地发现*没有*歧视。相等的命中率既不是公平性的必要条件，也不是充分条件。你必须对分布进行推理，而不是表面统计量。


<details>
<summary>English original</summary>

**2.1 Real-world harm**

Fairness is not abstract — biased models have harmed people, sometimes for a century. CS329P opens with cases:

- **Simpson's paradox (UC Berkeley admissions, 1973)** — aggregate admission rates looked biased against women, yet *per department* the bias reversed or vanished. Women applied disproportionately to more competitive departments. The aggregate lies; you must condition on the right variable.
- **COMPAS (ProPublica, 2016)** — a recidivism risk tool assigned systematically different risk-score distributions to Black vs. White defendants, producing **higher false-positive rates for Black defendants** (31% vs. 15% of those who did *not* reoffend were flagged high-risk in Broward County).
- **Bias in online advertising** (Lambrecht & Tucker) — an ostensibly neutral ad-delivery system showed a STEM job ad to men more than women, for economic reasons unrelated to intent.
- **Bias in lending** (Martinez & Kirchner, 2021) — modern mortgage-approval algorithms showed disparities by race.
- **Redlining** — location-based credit denial in the 1930s discriminated against minorities, and its effects are **still measurable 90 years later**. Bias baked into a system propagates across generations.

The lesson the lecture states bluntly: **bias and lack of fairness can harm people for a century — we owe it to everyone to be mindful.**

**2.2 Legal frameworks**

Fairness has a legal substrate that constrains what a deployed model is allowed to do. **Protected attributes** are the characteristics the law forbids discriminating on. US federal law:

- **Title VII of the Civil Rights Act (1964)** — bars discrimination on **race, color, religion, national origin, or sex** (and protects against retaliation for raising a claim).
- **Pregnancy Discrimination Act** — extends "sex" to pregnancy and childbirth.
- **Equal Pay Act (1963)** — bars sex-based wage discrimination for equal work.
- **Age Discrimination in Employment Act (1967)** — protects workers **40 and older**.
- **Americans with Disabilities Act, Title I (1990)** — bars discrimination against a qualified person with a disability.

Europe's **GDPR** adds data-protection and a right to explanation around automated decisions. And regulators are moving toward a **legal requirement to show that hiring algorithms are unbiased** — which immediately raises the question the rest of this section answers: *what does "unbiased" actually mean?*

Two legal concepts every ML engineer should hold:

- **Disparate treatment** — *intentionally* using a protected attribute (or treating people differently because of it). Roughly the **anti-classification** idea: don't put `race` in the model.
- **Disparate impact** — a facially neutral policy that nonetheless produces *unequal outcomes* across groups, regardless of intent. This is the dangerous one for ML: you can trigger disparate impact with a model that never sees the protected attribute, because **secondary attributes** (zip code, name, dreadlocks vs. an Oxford shirt, "pony vs. motorbike") proxy for it.

**2.3 Risk distributions across groups**

Corbett-Davies & Goel's framing (ICML 2019 tutorial) is the cleanest mental model. For a decision like "search this vehicle for contraband?", each group has a **risk distribution** — the spread of `p(contraband | x)` over its members, from 0 to 1. Two facts about these distributions drive everything:

- **Different average risk** = different base rates of actually carrying contraband.
- **Different variance** = different *ability to tell who* is carrying — lower variance means harder to separate the guilty from the innocent within that group.

This exposes the trap in the naive **"outcome test"** (judge fairness by hit rates). Consider two groups that both carry contraband 30% of the time, under a facially neutral policy: *search if probability > 50%*. The group with **higher-variance** risk has more "obviously guilty" members above the threshold, so its **hit rate comes out higher** — and the outcome test wrongly cries bias against the other group. Conversely, a policy that genuinely discriminates (searching one group at a lower threshold, 45% vs. 50%) can be tuned so the **hit rates come out equal**, and the outcome test wrongly finds *no* discrimination. Equal hit rates are neither necessary nor sufficient for fairness. You have to reason about the distributions, not the surface statistic.

</details>

### 2.4 形式公平性判据

为评测分类器 `f : 𝒳 → ℝ`（其中 `f(x)` 越大意味着 `y = 1` 更可能，不过原始分数无需校准），以标准诊断为基础——混淆矩阵（TP/FP/FN/TN）、**ROC 曲线**（真正例率 vs. 假正例率，随阈值 `z` 变化）以及**精度-召回率曲线**。公平性问题是：**在群体之间匹配这些质量分数中的哪一个**（非裔美国人、亚裔、拉丁裔、白人；男性、女性；……）？候选判据：

| 判据 | 通俗定义 | 形式条件 | 又称 |
|---|---|---|---|
| **人口统计 / 统计均等** | 各群体中 *正预测率* 相同，不论真值如何 | 每组 `TP + FP = const` → 切分点在群体间不同 | 群体公平 |
| **均等化几率 / 分类均等** | 各群体中相同**错误率**（例如相同的假正例率、相同的真正例率） | `FPR_a = FPR_b` 和 `TPR_a = TPR_b` | 分类均等 |
| **预测均等 / 校准** | 给定相同风险分数，**结果与群体无关**——0.7 的分数对每个人都意味着 70% | `p(y = 1 | score, group)` 与群体无关 | 组内校准 |
| **反分类** | 算法**不使用**受保护属性 | `race ∉ features` | 通过不感知实现公平 |
| **条件人口统计差异（CDD）** | 某群体在正 vs. 负结果中获得的份额，*相对于其人口统计特征*，是否更小，并通过划分避免辛普森悖论 | `CDD = Σ_c p(c)·[ p(ŷ=1∣c)/p(ŷ=1) − p(ŷ=−1∣c)/p(ŷ=−1) ]` | — |

讲座强调两点警示。**反分类可能使结果更糟**——去掉受保护属性会让模型通过代理变量进行歧视，*并且* 剥夺你度量或纠正它的能力；还会损失准确率。而且**校准可以被“钻空子”**：通过选择对某个群体根本不具预测性的特征，你可以把该群体的分数移到决策阈值以下，同时这些分数 *在技术上仍保持校准*——例如，拘留风险 > 0.5 的被告，但设计特征使任何 blue-group 被告都不会超过 0.5，从而使平均再犯率保持不变。在纸面上满足一项判据并不等同于公平。

### 2.5 不可能性结果

以下就是使公平性真正困难、而不只是琐碎的结果。**Kleinberg, Mullainathan & Raghavan (2016)** 和 **Chouldechova (2017)** 独立证明：

> **不可能**同时满足 (1) 组内校准、(2) 正类平衡，以及 (3) 负类平衡——**除非**分类器是完美的，*或者*分数分布在群体间完全相同。

通俗地说：通常**无法同时拥有人口统计均等、均等化几率和校准。** 改进一个判据可证明会降低另一个——这一张力早在 Darlington (1971) 就被指出，并在 Hutchinson & Mitchell 的 "50 Years of Test (Un)fairness" (2018) 中得到综述。

CS329P 将深层原因表述为一个 **“Pokémon” 定理**。令 `p` 和 `q` 为两个受保护群体在 `𝒳 × {0,1}` 上的分布。假设你已匹配了一组有限统计量，`sᵢ[p] = sᵢ[q]` 对应 `i = 1…n`。如果 `p ≠ q`，则 *总是* 存在另一个统计量 `s′`，满足 `s′[p] ≠ s′[q]`。证明是最大均值差异：两个分布只有在**所有**期望（在一无限类测试函数上）都匹配时才相同——有限数量的匹配统计量永远不够，因此总有一个公平性判据你会不满足。**无论你满足多少判据，总会有另一个被打破。**（相关：Simoiu, Corbett-Davies & Goel 2017，infra-marginality。）根本原因是几何性的——不同分布有不同的 ROC/PR 曲线，而在阈值处切分不同曲线通常会产生不同结果。

结论不是虚无主义。而是 **“公平”不是单一可优化的目标。** 你必须选择 *哪个* 判据对 *你的* 问题重要，并为这一选择辩护——因为你无法同时获得全部。


<details>
<summary>English original</summary>

**2.4 Formal fairness criteria**

To evaluate a classifier `f : 𝒳 → ℝ` (where larger `f(x)` means `y = 1` is more likely, though the raw scores need not be calibrated), we build on standard diagnostics — the confusion matrix (TP/FP/FN/TN), the **ROC curve** (true-positive rate vs. false-positive rate as the threshold `z` varies), and the **precision-recall curve**. The fairness question is: **match which of these quality scores across groups** (African American, Asian, Latino, White; men, women; …)? The candidates:

| Criterion | Plain-English definition | Formal condition | Also called |
|---|---|---|---|
| **Demographic / statistical parity** | Same *rate of positive predictions* across groups, regardless of truth | `TP + FP = const` per group → cutpoints vary between groups | Group fairness |
| **Equalized odds / classification parity** | Same **error rates** across groups (e.g. equal false-positive rate, equal true-positive rate) | `FPR_a = FPR_b` and `TPR_a = TPR_b` | Classification parity |
| **Predictive parity / calibration** | Given the same risk score, the **outcome is independent of group** — a score of 0.7 means 70% for everyone | `p(y = 1 | score, group)` independent of group | Calibration within groups |
| **Anti-classification** | The protected attribute is **not used** by the algorithm | `race ∉ features` | Fairness through unawareness |
| **Conditional demographic disparity (CDD)** | Whether a group gets a smaller share of positive vs. negative outcomes *relative to its demographics*, partitioned to avoid Simpson's paradox | `CDD = Σ_c p(c)·[ p(ŷ=1∣c)/p(ŷ=1) − p(ŷ=−1∣c)/p(ŷ=−1) ]` | — |

Two cautions the lecture stresses. **Anti-classification can make outcomes worse** — dropping the protected attribute lets the model discriminate via proxies *and* removes your ability to measure or correct for it; it also costs accuracy. And **calibration can be "hacked"**: by choosing features that simply aren't predictive for one group, you can shift that group's scores below the decision threshold while the scores *remain technically calibrated* — e.g. detain defendants with risk > 0.5, but engineer features so no blue-group defendant ever crosses 0.5, leaving the average reoffending rate unchanged. Satisfying a criterion on paper is not the same as being fair.

**2.5 The impossibility result**

Here is the result that makes fairness genuinely hard rather than merely fiddly. **Kleinberg, Mullainathan & Raghavan (2016)** and **Chouldechova (2017)** independently proved:

> It is **impossible** to simultaneously satisfy (1) calibration within groups, (2) balance for the positive class, and (3) balance for the negative class — **unless** the classifier is perfect, *or* the score distributions are identical across groups.

In plain terms: you generally **cannot have demographic parity, equalized odds, and calibration all at once.** Improving one criterion provably degrades another — a tension noted as far back as Darlington (1971) and surveyed in Hutchinson & Mitchell's "50 Years of Test (Un)fairness" (2018).

CS329P frames the deep reason as a **"Pokémon" theorem**. Let `p` and `q` be the distributions on `𝒳 × {0,1}` for two protected groups. Suppose you've matched a finite set of statistics, `sᵢ[p] = sᵢ[q]` for `i = 1…n`. If `p ≠ q`, there *always* exists another statistic `s′` with `s′[p] ≠ s′[q]`. The proof is Maximum Mean Discrepancy: two distributions are identical only if **all** expectations (over an infinite class of test functions) match — a finite number of matched statistics can never be enough, so there is always one fairness criterion you fail. **Regardless of how many criteria you satisfy, another one breaks.** (Related: Simoiu, Corbett-Davies & Goel 2017, infra-marginality.) The root cause is geometric — different distributions have different ROC/PR curves, and cutting different curves at thresholds produces different outcomes in general.

The takeaway is not nihilism. It is that **"fair" is not a single optimizable target.** You must choose *which* criterion matters for *your* problem and defend that choice — because you cannot get them all.

</details>

### 2.6 实践中的公平性

那实际该怎么做？讲座的指导很明确：

- **把（多个）公平性度量当作*指标*，而不是训练目标。** 多算几个；如果发现群体之间存在较大差异，就**调试模型**——它们在*发现*问题上非常出色。但**不要只是把一个公平性准则拧到损失上然后指望它管用**——天真地优化某个准则，可能会在别处*加剧*歧视；丢掉受保护属性会降低准确率；而在目标函数里做粗暴的「平权行动」可能适得其反（例如通过刻板印象降低多样性；Lipton 2019 关于招生的讨论）。

在确有正当理由时，缓解措施落在流水线的三个阶段：

| 阶段 | 作用 | 示例 / 风险 |
|---|---|---|
| **预处理** | 在训练前修正**数据** | 对代表性不足的群体重新加权/重采样；移除有偏特征。检查数据采集偏差：人群偏斜（CelebFace 中白人演员过多）、文本中的文化刻板印象（“女护士 / 男医生”）、时间偏差（社交网络的早期用户群） |
| **处理中** | 约束**训练**目标 | 加入公平性约束/惩罚项。强大但危险——可能降低准确率，或把歧视转移到别处；绝不要盲目部署 |
| **后处理** | 调整**输出 / 阈值** | 用分组特定的切分点来均衡选定的准则。注意：按群体分别设阈值本身可能就构成差别对待——这是法律问题，不只是技术问题 |

还有讲座坚持的非算法保障措施：**多元化团队**能发现更多问题；收集**利益相关方反馈**；追问**数据从哪来**；*主动*找问题（“如果看起来不对劲，那多半就是不对劲”）；以及**部署后持续测试**——外部红队会找到你的弱点（Microsoft 的 *Tay* 在几小时内就变得种族主义；GPT-2 上的 *AI Dungeon* 生成了辱骂性内容；图像分类器曾给人贴错标签）。最后，把**非对称风险**编码进决策，而不是天真地取最可能的标签：一盒 99% 可安全食用的蘑菇并不值得吃，因为 `R[poison | edible]` ≫ `R[edible | poison]`。要用风险矩阵，而不是 MLE。

> **公平性小结：** 示例、法律、算法准则、不可能性、实践——但最重要的是，**用你的常识，努力去理解问题。** 数学给出约束；它不替你做决定。

---

## 3. 可解释性

### 3.1 为什么要解释

引子是这样的：你来到美国，找到一份工程师工作，申请信用卡——却因为你不理解的「不良信用记录」而被**拒绝**。模型用了些奇怪的特征（你的信用额度*年龄*让这张卡保持存活），你必须主动去刷分，而一旦建立起来就很容易维持——但前提是有人告诉你它*看重什么*。这就是面向用户的可解释性理由。完整的理由集合：

- **信任**——用户和利益相关方会接受他们能理解的决定。
- **调试**——解释能揭示模型何时是因*错误的原因*而做对（见下文后门）。
- **合规**——GDPR 的解释权，以及不断涌现的审计要求，都可能让解释成为*法律*义务。

形式上，给定数据 `X` 和已训练好的模型 `f`，有三个不同的解释问题：

1. **全局**——解释 `f` *总体上*在做什么。
2. **特征归因**——解释特征 `xᵢ` *哪些/如何*影响输出 `f(x)`。
3. **局部**——解释 `f(x)` *在某个特定点附近*的行为（为什么*这个*申请人的评分很差？）。


<details>
<summary>English original</summary>

**2.6 Fairness in practice**

So what do you actually do? The lecture's guidance is pointed:

- **Use (multiple) fairness measures as *indicators*, not training targets.** Compute several; if you find large discrepancies between groups, **debug the model** — they are excellent at *spotting* problems. But **do not just bolt a fairness criterion onto the loss and hope** — naively optimizing one criterion can *increase* discrimination elsewhere, dropping the protected attribute reduces accuracy, and crude "affirmative action" in the objective can backfire (e.g. reduce diversity via stereotyping; Lipton 2019 on admissions).

Mitigations, when warranted, attach at three stages of the pipeline:

| Stage | What it does | Examples / risks |
|---|---|---|
| **Pre-processing** | Fix the **data** before training | Reweight/resample under-represented groups; remove biased features. Check for data-collection bias: population skew (too many white actors in CelebFace), cultural stereotypes in text ("female nurse / male doctor"), temporal bias (a social network's early user base) |
| **In-processing** | Constrain the **training** objective | Add a fairness constraint/penalty. Powerful but dangerous — can reduce accuracy or shift discrimination; never deploy blind |
| **Post-processing** | Adjust the **outputs / thresholds** | Use group-specific cutpoints to equalize a chosen criterion. Note: per-group thresholds may itself be disparate treatment — a legal question, not just a technical one |

And the non-algorithmic safeguards the lecture insists on: a **diverse team** catches more issues; gather **stakeholder feedback**; ask **where the data came from**; look for problems *proactively* ("if things look strange, they probably are"); and **keep testing after deployment** — external red-teamers will find your weaknesses (Microsoft's *Tay* turned racist within hours; *AI Dungeon* on GPT-2 generated abusive content; image classifiers have mislabeled humans). Finally, encode **asymmetric risk** into decisions rather than naively taking the most-likely label: a box of mushrooms 99% safe to eat is not worth eating, because `R[poison | edible]` ≫ `R[edible | poison]`. Use the risk matrix, not the MLE.

> **The fairness summary:** examples, law, algorithmic criteria, impossibility, practice — but above all, **use your common sense and try to understand the problem.** The math constrains; it does not decide.

---

**3. Explainability**

**3.1 Why explain**

The motivating story: you arrive in the US, land an engineer job, apply for a credit card — **denied** for "bad credit history" you don't understand. The model used weird features (the *age* of your credit line keeps your card alive), you have to actively game the score, and once set it's easy to maintain — but only if someone tells you *what it keys on*. That is the user-facing case for explainability. The full set of reasons:

- **Trust** — users and stakeholders accept a decision they can understand.
- **Debugging** — explanations reveal when a model is right for the *wrong reason* (see backdoors below).
- **Compliance** — GDPR's right to explanation, and emerging audit requirements, can make explanation a *legal* obligation.

Formally, given data `X` and a trained model `f`, three distinct explanation questions:

1. **Global** — explain what `f` does *in general*.
2. **Feature attribution** — explain *which/how* features `xᵢ` affect the output `f(x)`.
3. **Local** — explain how `f(x)` behaves *near a specific point* (why is *this* applicant's rating poor?).

</details>

### 3.2 策略 —— 两个轴

解释方法沿两个轴组织。**内在 vs. 事后**：模型是 *构造上* 可解释的，还是事后解释一个黑箱？**全局 vs. 局部**：解释整个模型，还是单个预测？

| 族 | 轴 | 思路 | 代价 |
|---|---|---|---|
| **简单性**（内在，全局） | 使用本身可解释的模型 | 线性/logistic（权重 `wᵢ` = 重要性，可选带 `ℓ₁` 稀疏性）、小型决策**树**、决策**列表**（if-then-elsif）。烧伤分诊的“九分法”是理想情形：足够简单，可在压力下使用 | 容量有限；线性在高维图像/文本/时间序列上失效；“过于繁琐，难以落地” |
| **近似简单性**（事后，全局） | 训练一个复杂模型，然后将其 **蒸馏** 成简单模型 | 拟合 `g` 以匹配 `f` 的预测：`minimize_g Σ l(g(xᵢ), f(xᵢ))`。若训练集较小，则生成辅助数据用于蒸馏（Fakoor et al., 2020） | 简单代理的忠实度仅取决于蒸馏 |
| **局部简单性**（事后，局部） | 在 **单个查询附近** 近似黑箱 | LIME —— 一种“用回归做泰勒展开”：在 `x` 周围采样点 `xⱼ`，对 `(xⱼ, f(xⱼ))` 拟合局部线性 `g`（Ribeiro et al., 2016） | 仅在局部忠实；不稳定（见 §3.5） |

局部方法的直觉：黑箱在 *全局* 上可能复杂到无望，却能在 **小邻域内线性化**。你无法近似整个曲面，但可以对你关心的点周围的局部区域拟合一个简单模型。

### 3.3 条件化与后门 —— Clever Hans 问题

最微妙、最重要的部分。要归因影响，需度量当你改变特征 `Δx` 时 `f` 相对 **参考值 `x₀`** 如何变化（对图像、文本、表格、音频而言，均值是合理的默认值）。但改变 `xᵢ` 是混杂的：`xᵢ` 可能与 *其他* 特征 `x₋ᵢ` 相关，因此朴素扰动把 **直接** 影响（你想要的）与经由相关性产生的 **间接** 影响混为一谈。

这就是 **后门问题**，画成因果图。一个潜变量 `z`（比如真实的信用度）生成两个观测特征 `x₁, x₂`。观测到的 `x₁, x₂` 是相依的 —— `p(x₁, x₂) ≠ p(x₁)p(x₂)` —— *经由* `z`。如果你通过扰动观测属性、同时让它们保持相关来解释，度量到的就是后门路径，而非因果效应。**改变一个观测属性并不改变底层的条件。** 修复方法是遵循 Pearl 的 **`do`-算子**，从 **边缘** `p(x₋ᵢ)` 而非条件 `p(x₋ᵢ | xᵢ)` 中抽取被留出的特征 —— 从边缘分布采样打破了虚假的相依关系，且既更容易，通常也 *更正确*：

```text
E[ y | do(X₁ = x₁) ] = ∫ p(x₂, x₃) · E[ y | x₁, x₂, x₃ ] dx₂ dx₃     # average over marginals, not conditionals
```


它的实际表现就是 **Clever Hans** 效应 —— 模型看似解决了任务，实际却依赖于数据中的 **虚假相关**。（Clever Hans 是一匹马，靠解读驯养师的身体语言来“做算术”。）经典的 ML 版本有：一个哈士奇 vs. 狼分类器实际上检测的是 **背景中的雪**；一个肺炎模型依赖于 **医院的扫描仪 ID**；一个坦克检测器学到的是 **一天中的时间**。模型在测试集上正确，部署中却灾难性地错误，因为它所倚重的后门特征无法迁移。**可解释性正是抓住这一点的手段** —— 好的归因方法会指向雪，于是你意识到模型根本没学到动物。


<details>
<summary>English original</summary>

**3.2 Strategies — two axes**

Explanation methods organize along two axes. **Intrinsic vs. post-hoc**: is the model interpretable *by construction*, or do you explain a black box after the fact? **Global vs. local**: explain the whole model, or one prediction?

| Family | Axis | Idea | Cost |
|---|---|---|---|
| **Simplicity** (intrinsic, global) | Use an inherently interpretable model | Linear/logistic (weights `wᵢ` = importance, optionally with `ℓ₁` sparsity), small decision **trees**, decision **lists** (if-then-elsif). The "Rule of Nines" for burn triage is the ideal: simple enough to use under stress | Limited capacity; linearity fails on high-dim image/text/time-series; "too tedious to operationalize" |
| **Approximate simplicity** (post-hoc, global) | Train a complex model, then **distill** it into a simple one | Fit `g` to match `f`'s predictions: `minimize_g Σ l(g(xᵢ), f(xᵢ))`. Generate auxiliary data to distill on if the training set is small (Fakoor et al., 2020) | The simple surrogate is only as faithful as the distillation |
| **Local simplicity** (post-hoc, local) | Approximate the black box **near one query** | LIME — a "Taylor expansion by regression": sample points `xⱼ` around `x`, fit a local linear `g` to `(xⱼ, f(xⱼ))` (Ribeiro et al., 2016) | Faithful only locally; unstable (see §3.5) |

The intuition for local methods: a black box may be hopelessly complex *globally* yet **linearizable in a small neighborhood**. You can't approximate the whole surface, but you can fit a simple model to the patch around the point you care about.

**3.3 Conditioning and backdoors — the Clever Hans problem**

The subtle, important part. To attribute influence, you measure how `f` changes when you change a feature, `Δx`, relative to a **reference value `x₀`** (means are a reasonable default for images, text, tabular, audio). But changing `xᵢ` is confounded: `xᵢ` may be correlated with the *other* features `x₋ᵢ`, so naive perturbation conflates **direct** influence (what you want) with **indirect** influence through the correlations.

This is the **backdoor problem**, drawn as a causal graph. A latent variable `z` (say, true creditworthiness) generates two observed features `x₁, x₂`. The observed `x₁, x₂` are dependent — `p(x₁, x₂) ≠ p(x₁)p(x₂)` — *through* `z`. If you explain by perturbing observed attributes while letting them stay correlated, you measure the backdoor path, not the causal effect. **Changing an observed attribute does not change the underlying condition.** The fix, following Pearl's **`do`-operator**, is to draw the left-out features from their **marginal** `p(x₋ᵢ)` rather than the conditional `p(x₋ᵢ | xᵢ)` — sampling from marginals breaks the spurious dependence and is both easier and usually *more correct*:

```text
E[ y | do(X₁ = x₁) ] = ∫ p(x₂, x₃) · E[ y | x₁, x₂, x₃ ] dx₂ dx₃     # average over marginals, not conditionals
```

The practical face of this is the **Clever Hans** effect — a model that appears to solve the task but actually keys on a **spurious correlation** in the data. (Clever Hans was a horse that "did arithmetic" by reading its trainer's body language.) The canonical ML versions: a husky-vs-wolf classifier that really detects **snow in the background**; a pneumonia model that keys on the **hospital's scanner ID**; a tank detector that learned **time of day**. The model is right on the test set and catastrophically wrong in deployment, because the backdoor feature it leaned on doesn't transfer. **Explainability is how you catch this** — a good attribution method points at the snow, and you realize the model never learned the animal at all.

</details>

### 3.4 公理化方法 —— SHAP 与 Integrated Gradients

有原则的回应是：不要发明一个启发式方法然后指望它合理——而要写下归因*应该*满足的**公理**，并推导出满足这些公理的唯一方法。

**Shapley 值**来自合作博弈论。CS329P 的寓言：密克罗尼西亚议会中，党派 A（45 席）、B、C、D（各 15–25 席）必须凑够 51 票才能通过一项 \$1M 的法案。一个联盟的 “payoff” `v(S)` 是它是否获胜。在参与者之间公平分配功劳的方式满足三条公理——**对称性**（可互换的参与者获得相同功劳）、**哑元参与者**（不带来任何增益的参与者获得其单独价值），以及**可加性**（多个博弈之和上的功劳等于各自功劳之和）——而 **Shapley 值定理**指出，这些公理锁定了一个*唯一*的分配：参与者 `i` 在所有排列上的平均边际贡献，

```text
ϕ(i, N) = Σ_{S ⊆ N∖{i}}  [ |S|! (|N|−|S|−1)! / |N|! ] · [ v(S ∪ {i}) − v(S) ]
```

把**党派换成特征**、**payoff 换成模型输出**，就得到了 **SHAP**（Lundberg & Lee, 2017）。SHAP 定理是博弈论那条定理的镜像：满足**局部准确性**（解释在该点与 `f` 一致）、**缺失性**（缺失的特征得到零归因，`ϕᵢ = 0`）和**一致性**（若某特征在新模型下的边际贡献增大，其归因不缩小）的*唯一*特征归因评分，就是 Shapley 值。它作为特例恢复了线性模型的精确权重以及 LIME 的局部权重——一个统一性的结果。不过**魔鬼藏在细节里**：把某个特征“排除在外”究竟*意味着*什么——置零，还是取其均值？（幻灯片的建议：不要试图对条件分布建模；从边际分布中抽取**无关值**——Janzing et al., 2020——这对表格数据有效，但对上下文携带语义的文本/图像更棘手。）而且这个求和在特征子集上是 `O(2^|N|)`，所以需要近似：**TreeSHAP**、**DeepSHAP**、**KernelSHAP** 以及 Shapley **采样**都在精确性与可处理性之间做取舍。

**Integrated Gradients**（Sundararajan et al.）是基于梯度的公理化方法。它的公理——**完备性**（归因之和为 `f(x) − f(x₀)`）、**敏感性**（`f` 不依赖的某个特征得到零）、**实现不变性**（评分不取决于 `f` 是*如何*编码的）、**线性**和**对称性**——唯一地确定了梯度沿从基线 `x′` 到输入 `x` 的直线路径的积分：

```text
ϕ(i, x) = (xᵢ − x′ᵢ) · ∫₀¹ ∂_{xᵢ} f( x′ + α(x − x′) ) dα
```

*沿路径*积分（而不是在单个点上读取梯度）正是它获得完备性、并治愈困扰普通梯度的饱和问题的原因——而且有一个有用的联系：在 IG 可计算的地方，它是通往 Shapley 式归因的一条更廉价的路径。


<details>
<summary>English original</summary>

**3.4 Axiomatic approaches — SHAP and Integrated Gradients**

The principled response: don't invent a heuristic and hope it's reasonable — write down the **axioms** an attribution *should* satisfy and derive the unique method that satisfies them.

**Shapley values** come from cooperative game theory. CS329P's parable: the Parliament of Micronesia, parties A (45 seats), B, C, D (15–25 each), must reach 51 votes to pass a \$1M bill. A coalition's "payoff" `v(S)` is whether it wins. The fair way to split credit among players satisfies three axioms — **symmetry** (interchangeable players get equal credit), **dummy player** (a player who adds nothing gets their stand-alone value), and **additivity** (credit over a sum of games is the sum of credits) — and the **Shapley value theorem** says these axioms pin down a *unique* allocation: the average marginal contribution of player `i` over all orderings,

```text
ϕ(i, N) = Σ_{S ⊆ N∖{i}}  [ |S|! (|N|−|S|−1)! / |N|! ] · [ v(S ∪ {i}) − v(S) ]
```

Replace **parties with features** and **payoff with model output**, and you get **SHAP** (Lundberg & Lee, 2017). The SHAP theorem mirrors the game-theory one: the *only* feature-attribution score satisfying **local accuracy** (the explanation matches `f` at the point), **missingness** (a missing feature gets zero attribution, `ϕᵢ = 0`), and **consistency** (if a feature's marginal contribution grows under a new model, its attribution doesn't shrink) is the Shapley value. It recovers the exact weights for linear models and the local weights for LIME as special cases — a unifying result. The **devil is in the details**, though: what does "leaving a feature out" *mean* — set it to zero, or to its mean? (The slide's guidance: don't try to model the conditional distribution; draw **unrelated values** from the marginal — Janzing et al., 2020 — which works for tabular but is trickier for text/images where context carries meaning.) And the sum is `O(2^|N|)` over feature subsets, so it needs approximation: **TreeSHAP**, **DeepSHAP**, **KernelSHAP**, and Shapley **sampling** all trade exactness for tractability.

**Integrated Gradients** (Sundararajan et al.) is the gradient-based axiomatic method. Its axioms — **completeness** (attributions sum to `f(x) − f(x₀)`), **sensitivity** (a feature `f` doesn't depend on gets zero), **implementation invariance** (the score doesn't depend on *how* `f` is coded), **linearity**, and **symmetry** — uniquely determine the integral of the gradient along a straight path from a baseline `x′` to the input `x`:

```text
ϕ(i, x) = (xᵢ − x′ᵢ) · ∫₀¹ ∂_{xᵢ} f( x′ + α(x − x′) ) dα
```

Integrating *along the path* (rather than reading the gradient at a single point) is what gives it completeness and cures the saturation problem that plagues plain gradients — and there's a useful connection: where IG is computable, it's a cheaper route to Shapley-style attributions.

</details>

### 3.5 启发式方法——以及为何不应轻信它们

廉价、流行的方法——以及随之而来的警告。**敏感度分析 / 显著性**不过是局部梯度 `s_f(x) = ∂_x f(x)`，用 backprop 就能轻松算出。问题在于：**ReLU 及其他截断操作会把梯度置零**，于是显著性「漏掉相关的变化」，并**导致怪异、充满噪声的结果**。第一个补丁是 **grad × input**（`Δx · ∂_x f(x)`，Bach 等，2015）；更好的一个是 **DeepLIFT**，它用相对参考的**有限差分**替代导数，`[f(x′ + Δxᵢ) − f(x′)] / Δxᵢ · Δxᵢ`，并针对 ReLU、一般激活函数和线性算子配有专门的分解规则，全都能由一个改造过的 backprop 算出。这一家族的其他成员：**guided backprop**、**Grad-CAM**（梯度加权的类别激活图，用于高亮图像区域）、**KernelSHAP**，以及 §3.2 中的 LIME。

CS329P 给出的诚实评价是：这些启发式方法**不可靠**。原始梯度噪声大，且会被截断破坏。把它们用于**文本和图像**则更难——必须识别出*更大的组成单元*（不能随便丢掉像素或字符还指望有意义），而且没有明显的**参考 `x₀`**（什么才算「中性」文本？）。而最深的鸿沟在于：它们全都只告诉你模型关注了*什么*，但**我们真正想要的是因果性——为什么。** 一幅覆盖在雪上的显著性图告诉你这些像素起了作用；它本身并不会告诉你模型没能学到狼。把启发式方法当作快速初筛；当归因结论必须站得住脚时，就该改用公理化方法（以及 §3.3 的边缘采样准则）。

| 方法 | 家族 | 局部/全局 | 可靠性 |
|---|---|---|---|
| **线性权重 / 树 / 列表** | 内在简洁 | 全局 | 高——但仅当模型确实简单时 |
| **蒸馏** | 近似简洁 | 全局 | 与代理模型同样忠实 |
| **LIME** | 局部代理 | 局部 | 有用，但重复运行会**不稳定** |
| **SHAP** | 公理化（Shapley） | 局部（+ 全局聚合） | 公理强；代价/参考的选择很关键 |
| **Integrated Gradients** | 公理化（梯度） | 局部 | 公理强；需要基线 `x₀` |
| **显著性 / grad×input** | 启发式梯度 | 局部 | **噪声大**，会被 ReLU 截断破坏 |
| **Grad-CAM / guided backprop** | 启发式梯度 | 局部 | 在图像上很流行；但可能产生误导 |


<details>
<summary>English original</summary>

**3.5 Heuristic approaches — and why to distrust them**

The cheap, popular methods — and the warning that comes with them. **Sensitivity analysis / saliency** is just the local gradient `s_f(x) = ∂_x f(x)`, trivially computed by backprop. The problem: **ReLU and other clipping operations zero out gradients**, so saliency "misses out on relevant changes" and **leads to weird, noisy results**. The first patch is **grad × input** (`Δx · ∂_x f(x)`, Bach et al., 2015); a better one is **DeepLIFT**, which replaces derivatives with **finite differences** against a reference, `[f(x′ + Δxᵢ) − f(x′)] / Δxᵢ · Δxᵢ`, with special decomposition rules for ReLU, general activations, and linear ops, all computable by a modified backprop. Other members of the family: **guided backprop**, **Grad-CAM** (gradient-weighted class activation maps that highlight image regions), **KernelSHAP**, and LIME from §3.2.

The honest assessment CS329P delivers: these heuristics are **unreliable**. Raw gradients are noisy and broken by clipping. Applying them to **text and images** is harder still — you must identify *larger components* (you can't drop random pixels or characters and expect meaning), and there's no obvious **reference `x₀`** (what is the "neutral" text?). And the deepest gap: all of them tell you *what* the model attended to, but **what we actually want is causality — why.** A saliency map over the snow tells you the pixels mattered; it does not, on its own, tell you the model failed to learn the wolf. Use heuristics as fast first looks; reach for axiomatic methods (and the marginal-sampling discipline of §3.3) when an attribution has to hold up.

| Method | Family | Local/Global | Reliability |
|---|---|---|---|
| **Linear weights / trees / lists** | Intrinsic simplicity | Global | High — but only if the model is genuinely simple |
| **Distillation** | Approximate simplicity | Global | As faithful as the surrogate |
| **LIME** | Local surrogate | Local | Useful but **unstable** across reruns |
| **SHAP** | Axiomatic (Shapley) | Local (+ global agg.) | Strong axioms; cost/reference choices matter |
| **Integrated Gradients** | Axiomatic (gradient) | Local | Strong axioms; needs a baseline `x₀` |
| **Saliency / grad×input** | Heuristic gradient | Local | **Noisy**, broken by ReLU clipping |
| **Grad-CAM / guided backprop** | Heuristic gradient | Local | Popular for images; can be misleading |

</details>

> **可解释性总结：** 简单性、近似简单性、局部简单性；条件化与后门；公理化方法（SHAP、Integrated Gradients）；启发式方法。优先用公理而非启发式，从边际分布采样以规避后门，并且永远不要忘记目标是 *为什么*，而不只是 *什么*。

---

> **2026 更新：** 自 2021 年讲义以来的五个转变。**（1）多模态成为主流并走向生成式。** CLIP 是先行者；此后的时代由**多模态大语言模型**主导 —— **GPT-4V/4o**、**Gemini**、**带视觉的 Claude**，以及开源的 **LLaVA** 系列 —— 它们摄入交错的 image + 文本（并越来越多地摄入音频/视频）并且*生成*，而不只是分类。融合问题依然活跃：多数 VLM 属于**中间融合**（冻结的视觉编码器 → 投影层 → 大语言模型的 token 流），是 §1.2 的直接后裔。**（2）公平性进入了 LLM 领域。** 危害依旧（文本中的刻板印象 —— “女护士 / 男医生”），现在用**偏差评测**（BBQ、BOLD、WinoBias、HELM 的公平性套件）与**红队测试**来度量，而 COMPAS 式的不可能性结论依然成立 —— 你仍然无法同时满足所有准则。**（3）机制可解释性**作为超出本文这些归因方法的新分支出现 —— **稀疏自编码器**、电路分析和特征引导试图解释*网络内部表示哪些概念*，而不是哪些输入起了作用，是一个更雄心勃勃的“为什么”。**（4）监管长出了牙齿。** **EU AI Act**（2024 年生效，2026–27 年分阶段实施）将招聘、信贷和生物识别系统列为**高风险**，强制要求文档、偏差评估和人工监督 —— 把 §2.2 中“证明招聘算法无偏的潜在法律要求”变成带真实罚则的强制性法律。美国的 **NIST AI 风险管理框架**扮演平行的自愿性角色。**（5）归因工具成熟了** —— SHAP 与 Captum（Integrated Gradients）已是标准配置，而本领域自己对启发式显著性不可靠性的警示（Adebayo 等人的“健全性检查”）如今已成共识。本讲的核心警告经受住了时间考验：融合仍是需要判断的取舍，公平性仍无法归结为单一数字，解释仍必须指向 *为什么*。


<details>
<summary>English original</summary>

> **The explainability summary:** simplicity, approximate simplicity, local simplicity; conditioning and backdoors; axiomatic methods (SHAP, Integrated Gradients); heuristics. Prefer axioms over heuristics, sample from marginals to dodge backdoors, and never forget the goal is *why*, not just *what*.

---

> **2026 update:** Five shifts since the 2021 slides. **(1) Multimodal went mainstream and generative.** CLIP was the precursor; the era since is dominated by **multimodal LLMs** — **GPT-4V/4o**, **Gemini**, **Claude with vision**, and the open **LLaVA** family — that ingest interleaved image + text (and increasingly audio/video) and *generate*, not just classify. The fusion question is alive and well: most VLMs are **intermediate fusion** (a frozen vision encoder → a projection → the LLM's token stream), the direct descendant of §1.2. **(2) Fairness moved into LLMs.** The harms are the same (stereotypes in text — "female nurse / male doctor"), now measured by **bias evals** (BBQ, BOLD, WinoBias, HELM's fairness suite) and **red-teaming**, and the COMPAS-style impossibility result still binds — you still cannot satisfy every criterion at once. **(3) Mechanistic interpretability** emerged as a new branch beyond the attribution methods here — **sparse autoencoders**, circuit analysis, and feature steering try to explain *what concepts a network represents internally* rather than which inputs mattered, a more ambitious "why." **(4) Regulation got teeth.** The **EU AI Act** (in force 2024, phasing in through 2026–27) classifies hiring, credit, and biometric systems as **high-risk**, mandating documentation, bias assessment, and human oversight — turning the §2.2 "potential legal requirement to show hiring algorithms are unbiased" into binding law with real penalties. The US **NIST AI Risk Management Framework** plays a parallel voluntary role. **(5) Attribution tooling matured** — SHAP and Captum (Integrated Gradients) are standard, and the field's own caution about heuristic-saliency unreliability (Adebayo et al.'s "sanity checks") is now received wisdom. The lecture's core warnings aged well: fusion is still a judgment call, fairness still cannot be reduced to one number, and explanations still must aim at *why*.

</details>

> **硬件视角：** 每个主题都有一本部署算力账。**多模态推理**比纯文本更重 —— VLM 要跑一个视觉编码器*和*一个 LLM，图像 token（高分辨率下每张图常有数百到数千个）会撑大 **KV cache** 和 prefill（首字前的整段计算）成本；要经济地做推理服务，就要做与任何 LLM 相同的批处理、量化和缓存工作，外加跨一轮对话的编码器复用。**可解释性在大规模下代价高昂** —— 精确的 SHAP 是 `O(2^|N|)`，而即便是 Integrated Gradients，沿路径积分也需要*每个被解释实例*数十次前向/反向 pass，因此对高流量模型做归因是一笔真实的计算开销，这也是 TreeSHAP/DeepSHAP 近似与批量 IG 存在的原因。**公平性审计是持续的计算** —— 讲座里那句「部署后继续测试」意味着要搭建监控，在线上流量上按群体切分指标，这是持续成本，而非一次性检查。第 02 讲的吞吐教训再次出现：负责任部署的机制（多模态编码器、逐实例归因、按群体切分的公平性监控）会与模型争夺加速器周期，为它做预算，是交付一个公平、可解释*且*成本可接受的系统的一部分。

---


<details>
<summary>English original</summary>

> **Hardware lens:** Each topic has a deployment-compute story. **Multimodal inference** is heavier than text alone — a VLM runs a vision encoder *and* an LLM, and the image tokens (often hundreds to thousands per image at high resolution) inflate the **KV cache** and prefill cost; serving them economically drives the same batching, quantization, and caching work as any LLM, plus encoder reuse across a conversation. **Explainability is expensive at scale** — exact SHAP is `O(2^|N|)`, and even Integrated Gradients needs tens of forward/backward passes *per explained instance* along the path integral, so attribution for a high-traffic model is a real compute line item, which is why TreeSHAP/DeepSHAP approximations and batched IG exist. **Fairness auditing is continuous compute** — the lecture's "keep testing after deployment" means standing up monitoring that slices metrics by group on live traffic, an ongoing cost, not a one-time check. The throughput lesson from Lecture 02 recurs: responsible-deployment machinery (multimodal encoders, per-instance attributions, sliced fairness monitors) competes with the model for accelerator cycles, and budgeting for it is part of shipping a system that is fair, explainable, *and* affordable.

---

</details>

## 截至

写于 2026 年 6 月。**原始 CS329P 内容**——多模态 fusion（early/late/intermediate、CLIP 与 VideoBERT 两个例子、0.662 对 0.667 的 benchmark）、fairness 主线（harm 例子、美国/欧盟法律、风险分布与 outcome-test 陷阱、形式化判据、Kleinberg–Chouldechova / “Pokémon” 不可能性结论，以及 pre/in/post-processing 的实践说明）、可解释性主线（intrinsic 与 post-hoc、global 与 local 策略，backdoor/Clever-Hans 问题与 `do` 算子修正，SHAP 与 Integrated Gradients 的公理，以及启发式 saliency 方法及其不可靠性）——之所以先讲，是因为它仍是正确的工作心智模型，并且与 2021 年的 slides 一一对应。**refresh layer** 标出了此后发生变动的内容：作为融合主导应用的多模态 LLM（GPT-4V/Gemini/LLaVA）、LLM 偏见评测与 red-teaming、作为新解释分支的 **mechanistic interpretability**，以及把 fairness 从最佳实践变成具约束力的高风险系统法律的 **EU AI Act**。凡是 2021 年的表述已过时之处，先呈现原文，再给出更新；不做任何悄无声息的改写。

*改编自 [Stanford CS329P](https://c.d2l.ai/stanford-cs329p) — Huang, Li & Smola，CC-BY-SA-4.0。*


<details>
<summary>English original</summary>

**Current as of**

Written June 2026. The **original CS329P content** — multimodal fusion (early/late/intermediate, the CLIP and VideoBERT examples, the 0.662 vs. 0.667 benchmark), the fairness arc (harm examples, US/EU law, risk distributions and the outcome-test trap, the formal criteria, the Kleinberg–Chouldechova / "Pokémon" impossibility result, and the pre/in/post-processing practice notes), and the explainability arc (intrinsic vs. post-hoc and global vs. local strategies, the backdoor/Clever-Hans problem and the `do`-operator fix, the SHAP and Integrated Gradients axioms, and the heuristic saliency methods with their unreliability) — is taught first because it remains the correct working mental model and maps one-to-one onto the 2021 slides. The **refresh layer** flags what moved since: multimodal LLMs (GPT-4V/Gemini/LLaVA) as the dominant fusion application, LLM bias evals and red-teaming, **mechanistic interpretability** as a new explanation branch, and the **EU AI Act** turning fairness from best practice into binding high-risk-system law. Where 2021 framing is dated, the original is presented before the update; nothing is silently rewritten.

*Adapted from [Stanford CS329P](https://c.d2l.ai/stanford-cs329p) — Huang, Li & Smola, CC-BY-SA-4.0.*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/4. ML Engineering and MLOps/Practical Machine Learning (CS329P)/Lecture-11.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/4.%20ML%20Engineering%20and%20MLOps/Practical%20Machine%20Learning%20%28CS329P%29/Lecture-11.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
