---
title: 第 09 讲 - 迁移学习：CV、NLP 与 Prompting
description: 第 09 讲 - 迁移学习：CV、NLP 与 Prompting
---

# 第 09 讲 - 迁移学习：CV、NLP 与 Prompting

**合集：** [Practical Machine Learning (CS329P)](README.md) | **上一讲：** [← 第 08 讲](Lecture-08.md) | **下一讲：** [第 10 讲](Lecture-10.md)

---

如今几乎没有人再从头训练模型了。如果你今天加入一个 ML 团队，提议用随机权重和几千条自己的标注去初始化一个全新网络，你会被礼貌地反问：为什么不从预训练模型开始。默认的工作流——横跨视觉、语言、音频乃至更多领域——是**「从一个别人用海量数据训练好的模型出发，把它适配到你的任务上。」**这就是迁移学习，可以说它是现代 ML 中最大的一次实践转向。

原因在于经济性。深度网络*数据饥渴*，训练它们*代价高昂*。一个已经见过 120 万张标注图像、或数千亿词汇的模型，已经支付过高昂的前期成本，学会了边缘、纹理、形状、语法和语义长什么样。这些学到的特征并不专属于原始任务——它们会**迁移**。你的工作从「从零学会视觉」缩小为「告诉一个已经足够胜任的视觉模型，*你*的十个类别是什么」，而且用少 10–100× 的数据和一小部分算力就能做到。

本讲分三步走完这套范式。首先是理念本身——特征为什么会迁移，以及预训练 → 微调这一拆分是什么样子。然后是它占领的两个领域：**计算机视觉**（微调 ResNet、ViT 这类 ImageNet 骨干网络）和 **NLP**（微调 BERT 这类自监督模型）。最后是**基于 prompt 的学习**——GPT-3 发现，对于足够大的模型，你往往根本不需要更新*任何*权重，只要*用文本描述任务*即可。本讲忠实地呈现 2021 年的版本，再把每一部分映射到 2026 年它落在何处。

---

## 学习目标

本讲结束时，你应该能够：

* 解释在大数据集上学到的特征*为什么*能迁移到新任务，并阐述预训练 → 微调范式以及它带来的数据/算力节省。
* 微调预训练的 CV 骨干网络——在**特征提取**（冻结骨干，训练新的 head）与**全量微调**之间做选择，决定冻结哪些层，并为预训练权重设置一个合理的小学习率。
* 描述 NLP 模型背后的自监督预训练目标——**掩码语言建模**（BERT）和**自回归语言建模**（GPT）——以及下游微调如何复用它们的上下文 embedding。
* 解释**基于 prompt 的 / 上下文学习**：zero-shot、one-shot 和 few-shot prompting，为什么大 LM 不更新权重也能解决任务，以及从「微调权重」到「用文本做条件」的转变。
* 针对给定的任务、数据预算和算力预算，在**特征提取、微调与 prompting** 之间做选择。
* 把 2021 年的图景映射到 2026 年的技术栈：指令微调（RLHF/DPO）、参数高效微调（LoRA/QLoRA/适配器）以及检索增强生成（RAG）。

---


<details>
<summary>English original</summary>

**Lecture 09 - Transfer Learning: CV, NLP, and Prompting**

**Collection:** [Practical Machine Learning (CS329P)](README.md) | **Previous:** [← Lecture 08](Lecture-08.md) | **Next:** [Lecture 10](Lecture-10.md)

---

Almost nobody trains a model from scratch anymore. If you join an ML team today and propose initializing a fresh network with random weights and a few thousand of your own labels, you will be politely asked why you are not starting from a pretrained model. The default workflow — across vision, language, audio, and beyond — is **"start from a model someone else trained on a mountain of data, and adapt it to your task."** This is transfer learning, and it is arguably the single biggest practical shift in modern ML.

The reason is economic. Deep networks are *data-hungry* and training them is *expensive*. A model that has already seen 1.2 million labeled images, or hundreds of billions of words, has paid the steep up-front cost of learning what edges, textures, shapes, syntax, and meaning look like. Those learned features are not specific to the original task — they **transfer**. Your job shrinks from "learn vision from nothing" to "tell an already-competent vision model what *your* ten classes are," and you can do that with 10–100× less data and a fraction of the compute.

This lecture walks the paradigm in three movements. First the idea itself — why features transfer and what the pretrain → fine-tune split looks like. Then the two domains where it took over: **computer vision** (fine-tuning ImageNet backbones like ResNet and ViT) and **NLP** (fine-tuning self-supervised models like BERT). Finally **prompt-based learning** — GPT-3's discovery that for a large enough model you often do not need to update *any* weights at all; you just *describe the task in text*. We teach the 2021 version faithfully, then map each piece forward to where it landed by 2026.

---

**Learning objectives**

By the end of this lecture you should be able to:

* Explain *why* features learned on large datasets transfer to new tasks, and articulate the pretrain → fine-tune paradigm and the data/compute savings it buys.
* Fine-tune a pretrained CV backbone — choosing between **feature extraction** (freeze the backbone, train a new head) and **full fine-tuning**, deciding which layers to freeze, and setting a sensibly small learning rate for pretrained weights.
* Describe the self-supervised pretraining objectives behind NLP models — **masked language modeling** (BERT) and **autoregressive language modeling** (GPT) — and how downstream fine-tuning reuses their contextual embeddings.
* Explain **prompt-based / in-context learning**: zero-, one-, and few-shot prompting, why a large LM can solve tasks without weight updates, and the shift from "fine-tune weights" to "condition with text."
* Choose between **feature extraction, fine-tuning, and prompting** for a given task, data budget, and compute budget.
* Map the 2021 picture onto the 2026 stack: instruction tuning (RLHF/DPO), parameter-efficient fine-tuning (LoRA/QLoRA/adapters), and retrieval-augmented generation (RAG).

---

</details>

## 1. 为什么要迁移学习

动机一句话：**把一个任务上训练好的模型用于相关任务。** 它在深度学习中格外流行，正是因为 DNN 很吃数据，而从零开始训练的成本很高。既然昂贵的部分——学到好的特征——可以泛化，为什么还要为每个新问题重复这份成本？

“复用模型”有几种流派，先把它们命名清楚，才能知道本节讲的是哪一种：

* **特征提取**——把你的数据过一遍固定的预训练模型，用它内部的表示作为*另一个*下游模型的输入特征。经典例子：词的 Word2Vec embedding、图像的 ResNet-50 倒数第二 layer 特征、视频的 I3D 特征。预训练网络被冻结；它只是一个特征工厂。
* **在相关任务上训练再复用**——在标注*是*充足的任务上训练模型，再把它改用到你真正关心的任务上。
* **从预训练模型微调**——用预训练模型的权重初始化一个新模型，在你的数据上继续训练。**这是本节的重点。**

迁移学习还紧挨着几个相邻概念。它**与**半监督学习（少量有标注、大量无标注）**相关**，在极端情况下**与零样本 / 少样本学习相关**（几乎没有标注样本也要适配——这部分在 prompting 一节会讲到），还**与多任务学习相关**，即同时有若干任务、每个任务都有一些标注数据可用。

让后面所有内容都豁然开朗的心智模型：**训练好的神经网络是叠在一起的两部分。**

* 一个**特征提取器**（encoder）——网络的主体——把原始输入（像素、token）映射成一种表示，使数据变得*线性可分*。
* 一个**线性分类器**（decoder/head）——通常是最后 layer——读取该表示并做出决策。

**预训练模型**是在大规模、足够通用的数据集上训练出的网络。关键的经验事实是：它的*特征提取器泛化得很好*——能泛化到其他数据集（医学影像、卫星图像），甚至其他任务（检测、分割）。head 是任务专属的、可丢弃的；特征提取器才是可复用的资产。记住这个分解——它在 CV、NLP 和 prompting 里都是同一个思路。

---

## 2. 面向 CV 的微调

计算机视觉是观察迁移学习生效最干净的地方，因为有个幸运的巧合：**大规模带标注的 CV 数据集已经存在。** 尤其是图像分类，是最便宜的标注任务之一——人扫一眼图像、敲一个类别就行。ImageNet 提供了大约 **1.2M 张图像、1,000 个类别**。你的应用大概是 50K 张图像、100 个类别，或者 60K 张、10 个类别——小一到两个数量级。迁移学习正是弥合这一差距的办法。

### 预训练骨干网络

标准骨干网络是在 ImageNet 上预训练的卷积网络，如 **ResNet**（ResNet-18/50/...），以及越来越多的 **Vision Transformers (ViT)**。你几乎不会自己从零搭一个。两个常见来源：

* **TensorFlow Hub** (`tfhub.dev`) — 用户提交的 TensorFlow 模型。
* **TIMM** (`pytorch-image-models`，最初由 Ross Wightman 开发) — 一个庞大且维护良好的 PyTorch 图像模型集合。


<details>
<summary>English original</summary>

**1. Why transfer learning**

The motivation is one sentence: **exploit a model trained on one task for a related task.** It is especially popular in deep learning precisely because DNNs are data-hungry and the cost of training them from scratch is high. Why repeat that cost for every new problem when the expensive part — learning good features — generalizes?

There are several flavors of "reuse a model," and it helps to name them so you know which one this lecture is about:

* **Feature extraction** — run your data through a fixed pretrained model and use its internal representation as input features for a *separate* downstream model. Classic examples: Word2Vec embeddings for words, ResNet-50 penultimate-layer features for images, I3D features for video. The pretrained network is frozen; it is just a feature factory.
* **Train on a related task and reuse** — train a model on a task where labels *are* plentiful, then repurpose it for the task you actually care about.
* **Fine-tuning from a pretrained model** — initialize a new model with a pretrained model's weights and continue training on your data. **This is the focus of the lecture.**

Transfer learning also sits next to several adjacent ideas. It is **related to** semi-supervised learning (some labeled, lots of unlabeled), to **zero-shot / few-shot learning** in the extreme (adapt with almost no labeled examples — we get there in the prompting section), and to **multi-task learning**, where some labeled data is available for each of several tasks at once.

The mental model that makes everything downstream click: **a trained neural network is two parts stacked together.**

* A **feature extractor** (the encoder) — the bulk of the network — maps raw input (pixels, tokens) into a representation where the data becomes *linearly separable*.
* A **linear classifier** (the decoder/head) — typically the final layer — reads that representation and makes the decision.

A **pretrained model** is a network trained on a large-scale, general-enough dataset. The crucial empirical fact is that its *feature extractor generalizes well* — to other datasets (medical scans, satellite imagery), and even to other tasks (detection, segmentation). The head is task-specific and disposable; the feature extractor is the reusable asset. Hold onto that decomposition — it is the same idea in CV, in NLP, and in prompting.

---

**2. Fine-tuning for CV**

Computer vision is the cleanest place to see transfer learning work, because of a happy accident: **large-scale labeled CV datasets already exist.** Image classification in particular is among the cheapest things to label — a human glances at an image and types a class. ImageNet gives us roughly **1.2M images across 1,000 classes**. Your application probably has something like 50K images across 100 classes, or 60K across 10 — one or two orders of magnitude smaller. Transfer learning is precisely how you bridge that gap.

**Pretrained backbones**

The standard backbones are convolutional networks like **ResNet** (ResNet-18/50/...) and, increasingly, **Vision Transformers (ViT)**, pretrained on ImageNet. You almost never build one yourself. Two common sources:

* **TensorFlow Hub** (`tfhub.dev`) — user-submitted TensorFlow models.
* **TIMM** (`pytorch-image-models`, originally by Ross Wightman) — a large, well-maintained collection of PyTorch image models.

</details>

### 特征提取 vs 全量微调

使用预训练 backbone 有两种方式，区别在于*允许哪些参数发生变化。*

* **特征提取（冻结 backbone，训练 head）。** 冻结整个预训练特征提取器，只训练一个全新初始化的输出层（通常只是在冻结特征之上的一个线性分类器）。backbone 从不更新。这种方式快、需要的数据少，且正则化很强 —— 但它无法让特征本身适应任务。
* **全量微调。** 用预训练权重初始化特征提取器，*随机*初始化新的输出层，然后在你自己的数据上继续训练**整个网络**。因为你是从一个好的局部极小值附近出发，而不是从随机噪声出发，所以你用**很小的学习率**只训练**几个 epoch** —— 这会对搜索过程起正则化作用，并防止模型遗忘已经学到的东西。

用一幅图概括微调 recipe：把预训练层复制进目标模型，装上一个随机初始化的 head，再从那个已经不错的起点继续优化。

```text
   Source (pretrained)            Target (your task)
   ┌───────────────┐              ┌───────────────┐
   │ Output layer  │              │ Output layer  │  ← random init (new head)
   ├───────────────┤   copy ─────►├───────────────┤
   │  Layer L-1    │ ───────────► │  Layer L-1    │  ← copied weights
   │      ...      │ ───────────► │      ...      │  ← copied weights
   │   Layer 1     │ ───────────► │   Layer 1     │  ← copied weights
   └───────────────┘              └───────────────┘
```

### 冻结哪些层

神经网络学习的是**层级式特征**，而这种层级结构告诉你该冻结什么：

* **低层特征是通用的** —— 曲线、边缘、斑块。它们几乎能泛化到任何自然图像，所以没什么理由去动它们。
* **高层特征是任务和数据集特定的** —— 它们编码的内容接近原始分类标签。

所以标准做法是**冻结底层、训练顶层**。这样能保持通用的低层特征不变，把学习集中在任务特定的部分，并起到**强正则化**的作用 —— 可训练参数更少，意味着在小数据集上过拟合更少。冻结线画在哪里是一个旋钮：数据量很小或与源域非常相似时多冻一些；数据更多、或图像与 ImageNet 差异很大时少冻一些（或完全不冻）。

### 最小 PyTorch recipe

用 TIMM 的话，全量微调几乎不需要写代码 —— 加载一个预训练 backbone，把分类器 head 换成类别数与你匹配的那个，然后像任何普通任务一样训练它：

```python
import timm
from torch import nn

model = timm.create_model('resnet18', pretrained=True)
model.fc = nn.Linear(model.fc.in_features, n_classes)  # new head
# Train model as a normal training job (small LR, few epochs)
```

做特征提取的话，训练前还要额外冻结 backbone 的参数（对除 `model.fc` 之外的所有参数做 `requires_grad = False`）。

### 什么时候有用 —— 什么时候没用

对 ImageNet 预训练模型做微调在 CV 里随处可见：**检测与分割**（图像相似、目标不同）以及**医学/卫星影像**（任务相同、图像差异很大）。最可靠的收益是**收敛更快** —— 用少得多的 epoch 就能达到不错的准确率。

幻灯片里给出的坦诚提醒是：微调**并不总能提升最终准确率。** 如果你的目标数据集本身就很大，从零训练也能达到相近的准确率。迁移学习最大、最可靠的收益在*小数据*场景；随着数据量增长，与从零训练的差距会缩小。

**本节小结：** 在大数据集上预训练（通常是图像分类），用它初始化下游模型的权重，然后微调。这会加速收敛，并*有时*提升准确率 —— 当自有数据稀缺时最为明显。

---

## 3. NLP 的微调

NLP 的起点与 CV 在资源状况上正好相反。不存在可与 ImageNet 相比的**大规模有标注 NLP 数据集** —— 但有*海量的无标注文本*：Wikipedia、电子书、爬取的网页。突破点在于学会了如何在这种无标注文本上进行预训练。


<details>
<summary>English original</summary>

**Feature extraction vs full fine-tuning**

There are two ways to use a pretrained backbone, and the difference is *which parameters you allow to change.*

* **Feature extraction (freeze the backbone, train the head).** Freeze the entire pretrained feature extractor and train only a freshly initialized output layer (often just a linear classifier on top of the frozen features). The backbone never updates. This is fast, needs little data, and is strongly regularized — but it cannot adapt the features themselves.
* **Full fine-tuning.** Initialize the feature extractor from the pretrained weights, *randomly* initialize the new output layer, and then continue training the **whole network** on your data. Because you start near a good local minimum rather than from random noise, you train with a **small learning rate** for **just a few epochs** — this regularizes the search and keeps the model from forgetting what it learned.

The fine-tuning recipe in one picture: copy the pretrained layers into the target model, bolt on a randomly initialized head, and resume optimization from that already-good starting point.

```text
   Source (pretrained)            Target (your task)
   ┌───────────────┐              ┌───────────────┐
   │ Output layer  │              │ Output layer  │  ← random init (new head)
   ├───────────────┤   copy ─────►├───────────────┤
   │  Layer L-1    │ ───────────► │  Layer L-1    │  ← copied weights
   │      ...      │ ───────────► │      ...      │  ← copied weights
   │   Layer 1     │ ───────────► │   Layer 1     │  ← copied weights
   └───────────────┘              └───────────────┘
```

**Which layers to freeze**

Neural networks learn **hierarchical features**, and that hierarchy tells you what to freeze:

* **Low-level features are universal** — curves, edges, blobs. They generalize across almost any natural image, so there is little reason to disturb them.
* **High-level features are task- and dataset-specific** — they encode things close to the original classification labels.

So the standard move is to **freeze the bottom layers and train the top layers**. This keeps the universal low-level features intact, focuses learning on the task-specific part, and acts as a **strong regularizer** — fewer free parameters means less overfitting on a small dataset. Where you draw the freeze line is a knob: freeze more when your data is tiny or very similar to the source; freeze less (or nothing) when you have more data or your images differ a lot from ImageNet.

**The minimal PyTorch recipe**

With TIMM, full fine-tuning is almost no code — load a pretrained backbone, swap the classifier head for one with your number of classes, and train it like any normal job:

```python
import timm
from torch import nn

model = timm.create_model('resnet18', pretrained=True)
model.fc = nn.Linear(model.fc.in_features, n_classes)  # new head
# Train model as a normal training job (small LR, few epochs)
```

For feature extraction, you would additionally freeze the backbone's parameters (`requires_grad = False` on everything except `model.fc`) before training.

**When it helps — and when it doesn't**

Fine-tuning ImageNet-pretrained models is used everywhere in CV: **detection and segmentation** (similar images, different targets) and **medical/satellite imagery** (same task, very different images). The most reliable benefit is **faster convergence** — you reach good accuracy in far fewer epochs.

The honest caveat from the slides: fine-tuning **does not always improve final accuracy.** If your target dataset is itself large, training from scratch can reach a similar accuracy. Transfer learning's biggest, most dependable win is in the *small-data* regime; as your data grows, the gap to from-scratch training shrinks.

**Section summary:** pretrain on a large dataset (usually image classification), initialize your downstream model's weights from it, and fine-tune. This accelerates convergence and *sometimes* improves accuracy — most when your own data is scarce.

---

**3. Fine-tuning for NLP**

NLP starts from the opposite resource situation than CV. There is **no large-scale labeled NLP dataset** comparable to ImageNet — but there are *enormous quantities of unlabeled text*: Wikipedia, ebooks, crawled web pages. The breakthrough was learning how to pretrain on that unlabeled text.

</details>

### 自监督预训练

诀窍在于**自监督学习**：从原始文本本身生成一个“伪标签”，再用普通的监督学习对着它训练。无需人工标注——监督信号就藏在数据里。两种经典目标：

* **语言模型（LM）——预测下一个词。** 给定“I like your ___”，预测“hat”。这是**自回归**的：从左向右读。（这是 GPT 系列的目标。）
* **掩码语言模型（MLM）——预测一个被随机掩码的词。** 取“I like your hat”，遮住一个 token → “I like your `[MASK]`”，并从上下文的*两侧*预测缺失的词。（这是 BERT 的目标。）

回报是**上下文相关 embedding**：与静态查找表（Word2Vec/CBOW，其中每个词 `w` 得到固定向量，这些向量通过用其上下文词 embedding 之和预测该词而学得）不同，Transformer 会为每个词生成*取决于它周围句子*的表示。“Bank”靠近“river”和“bank”靠近“money”会得到不同的向量。迁移的正是这些表示。

### 按模型家族划分的预训练目标

| 模型 | 架构 | 预训练目标 |
|---|---|---|
| **Word2Vec**（CBOW） | 浅层 embedding | 用上下文词 embedding 之和预测一个词 |
| **BERT** | Transformer **编码器** | 掩码 token 预测 + 下一句预测 |
| **GPT** | Transformer **解码器** | 自回归下一 token 预测（见 §4） |
| **T5** | Transformer **编码器-解码器** | 从文档中填补被掩码的文本*片段* |

### BERT 及其微调 recipe

**BERT** 是一个巨大的 Transformer **编码器**，在 Wikipedia + BookCorpus（超过 30 亿词）上以两个任务预训练：**掩码 token 预测**和**下一句预测**。它发布了多个版本——base/large、英语/多语言、cased/uncased——并衍生出 **RoBERTa、ALBERT 和 ELECTRA** 等变体。

为下游任务微调 BERT 遵循一个一致的模式：**随机初始化一个新的最后 layer，并用小学习率训练几个 epoch。** 输入用特殊 token 格式化——开头是一个 `[CLS]` token，各片段之间用 `[SEP]` 分隔——而读取*哪些*隐藏状态取决于任务：

| 下游任务 | 从 BERT 读取什么 |
|---|---|
| **句子分类**（如情感） | `[CLS]` embedding → 一个稠密分类器 |
| **命名实体识别** | 每个 token 的隐藏状态 → 预测该 token 的实体标签 |
| **问答** | 句子 1 = 问题，句子 2 = 参考段落 → 预测参考段落中的答案*片段* |

BERT 的“在十一项 NLP 任务上取得新的最先进结果”包括语法性判断、影评情感、句对语义等价、文本蕴含和答案片段抽取。T5 以其文本到文本的框架，同样在摘要、QA 和分类 benchmark 上位居榜首。

### 实践中的注意事项

幻灯片指出了两个陷阱，都值得记住：

* **在小数据集上微调可能不稳定。** 两个常见原因：原始 BERT 去掉了 Adam 中的偏差校正步骤，以及人们训练的 epoch *太少*（3 个往往不够）。
* **重新初始化部分*顶层* Transformer layer 可能有帮助。** 最顶层 layer 的特征对预训练任务过于专门化；把它们扔掉并重新学习，能让模型自由适配。重置多少个 layer 取决于下游任务。

这一切的实用归宿是 **Hugging Face Transformers**——为 PyTorch 和 TensorFlow 提供预训练 Transformer 模型，背后是统一的 API：

```python
from transformers import AutoTokenizer, AutoModelForSequenceClassification

tokenizer = AutoTokenizer.from_pretrained("bert-base-cased")
inputs = tokenizer(sentences, padding="max_length", truncation=True)
model = AutoModelForSequenceClassification.from_pretrained(
    "bert-base-cased", num_labels=2)
# Train model on inputs as a normal training job
```

**本节小结：** 自监督预训练（一种（掩码）语言模型目标）让 NLP 能从无标注文本中学习；BERT 是一个巨大的 Transformer 编码器；下游任务以一致的低学习率、少 epoch 方式对它微调。

---


<details>
<summary>English original</summary>

**Self-supervised pretraining**

The trick is **self-supervised learning**: generate a "pseudo-label" from the raw text itself, then train with ordinary supervised learning against it. No human annotation required — the supervision is hidden in the data. The two canonical objectives:

* **Language model (LM) — predict the next word.** Given "I like your ___", predict "hat". This is **autoregressive**: it reads left to right. (This is the GPT family's objective.)
* **Masked language model (MLM) — predict a randomly masked word.** Take "I like your hat", hide a token → "I like your `[MASK]`", and predict the missing word from *both sides* of context. (This is BERT's objective.)

The payoff is **contextual embeddings**: unlike a static lookup table (Word2Vec/CBOW, where each word `w` gets fixed vectors learned by predicting it from the sum of its context words), a transformer produces a representation of each word *that depends on the sentence around it*. "Bank" near "river" and "bank" near "money" get different vectors. These representations are what transfer.

**Pretraining objectives, by model family**

| Model | Architecture | Pretraining objective |
|---|---|---|
| **Word2Vec** (CBOW) | shallow embeddings | predict a word from the sum of its context word embeddings |
| **BERT** | transformer **encoder** | masked-token prediction + next-sentence prediction |
| **GPT** | transformer **decoder** | autoregressive next-token prediction (covered in §4) |
| **T5** | transformer **encoder-decoder** | fill in a masked *span* of text from documents |

**BERT and its fine-tuning recipe**

**BERT** is a giant transformer **encoder**, pretrained on Wikipedia + BookCorpus (over 3 billion words) with two tasks: **masked token prediction** and **next-sentence prediction**. It ships in many versions — base/large, English/multilingual, cased/uncased — and spawned variants such as **RoBERTa, ALBERT, and ELECTRA**.

Fine-tuning BERT for a downstream task follows one consistent pattern: **randomly initialize a new last layer and train a few epochs with a small learning rate.** The input is formatted with special tokens — a `[CLS]` token at the front and `[SEP]` separators between segments — and *which* hidden states you read out depends on the task:

| Downstream task | What you read out of BERT |
|---|---|
| **Sentence classification** (e.g. sentiment) | the `[CLS]` embedding → a dense classifier |
| **Named-entity recognition** | each token's hidden state → predict that token's entity tag |
| **Question answering** | sentence 1 = question, sentence 2 = reference passage → predict the answer *span* in the reference |

BERT's "obtains new state-of-the-art results on eleven NLP tasks" included grammaticality judgments, movie-review sentiment, sentence-pair semantic equivalence, textual entailment, and answer-span extraction. T5, with its text-to-text framing, similarly topped summarization, QA, and classification benchmarks.

**Practical considerations**

Two gotchas the slides call out, both worth remembering:

* **Fine-tuning on small datasets can be unstable.** Two common culprits: the original BERT removed the bias-correction step in Adam, and people train for *too few* epochs (3 is often not enough).
* **Re-initializing some of the *top* transformer layers can help.** The topmost layers' features are too specialized to the pretraining tasks; throwing them away and relearning them frees the model to adapt. How many layers to reset depends on the downstream task.

The practical home for all of this is **Hugging Face Transformers** — pretrained transformer models for both PyTorch and TensorFlow, behind a uniform API:

```python
from transformers import AutoTokenizer, AutoModelForSequenceClassification

tokenizer = AutoTokenizer.from_pretrained("bert-base-cased")
inputs = tokenizer(sentences, padding="max_length", truncation=True)
model = AutoModelForSequenceClassification.from_pretrained(
    "bert-base-cased", num_labels=2)
# Train model on inputs as a normal training job
```

**Section summary:** self-supervised pretraining (a (masked) language-model objective) lets NLP learn from unlabeled text; BERT is a giant transformer encoder; downstream tasks fine-tune it in a consistent, low-LR, few-epoch manner.

---

</details>

## 4. 基于提示的学习

BERT 式微调有一个微妙的不匹配：其*预训练*任务（填充掩码）和其*下游*任务（对句子分类）形态不同，这正是它通常需要**数千个标注样本**才能微调好的部分原因。

**提示**通过**将下游任务转化为预训练任务本身**消除了这一不匹配——即一个语言建模问题。你把任务表述为模型可以直接续写的文本：

* 情感分析变为：`"I like this movie. It was ___"` → 模型填入 `great`（正面）或 `terrible`（负面）。
* 机器翻译变为：`"Hello world! => ___"` → 模型填入 `Bonjour le monde!`。

GPT 使这一范式流行起来。决定性的展示是 **GPT-3**——一个巨型 Transformer **解码器**（~175B 参数），在来自 CommonCrawl、WebText 和书籍的 500B+ token 上训练，据报道训练成本约为 \$12M。结果证明它是一个通用语言模型，具有出色的文本生成能力，并且关键在于**零样本 / 少样本学习**：它能*理解用自然语言给出的任务说明*，并在完全没有任何梯度更新的情况下执行任务。

### 零样本、单样本和少样本

“shots”仅仅是指*你在提示中放入多少个已解答示例*，然后才请求答案——模型在推理时会以它们为条件：

| 设置 | 提示中包含什么 | 权重更新 |
|---|---|---|
| **零样本** | 仅任务描述，然后是输入 | 无 |
| **单样本** | 描述 + **一个**示例，然后是输入 | 无 |
| **少样本** | 描述 + 几个（~10）示例，然后是输入 | 无 |

这就是**上下文学习**：模型从上下文窗口中即时“学习”任务，然后在请求结束的瞬间忘掉它。权重中不存储任何东西。凭借这一点，GPT-3 能根据描述写代码（“一张列出最富裕国家、包含列名和 GDP 的表”），根据经典示例生成思想实验，并支撑当时编目的数百个演示（搜索引擎、NPC 对话、诗歌）。

### 概念上的转变

这是整个讲座逐步引向的转折点：**从“微调权重”转向“用文本施加条件”。** 在 CV 和 BERT 微调中，适应任务意味着*改变参数*。在提示中，适应意味着*写出更好的输入*。模型被冻结；你的杠杆是提示——因此有了**提示工程**，即选择措辞、模板和示例标签以使模型按预期表现的技艺。

### 基于提示的微调（混合方法）

对于*中等规模*的 LM（例如 < 1B 参数），有一个中间地带：纯提示较弱，但完全微调又浪费。**基于提示的微调**设计任务专用的*提示模板*（以及它映射到的标签词），而不是接上一个全新的输出层，然后通过该模板微调模型权重。报告结果：它大约比标准微调**样本效率高 100×**。由于手工设计模板和标签词很麻烦，已有关于**自动提示搜索**的工作——自动选择模板和标签词（Gao et al., 2021）。

**章节总结：** 基于提示的学习以语言模型格式呈现下游任务。足够大的模型（GPT-3）直接使用其*预训练*权重完成下游任务，**不更新参数**；用于微调时，提示能带来好得多的样本效率。

---


<details>
<summary>English original</summary>

**4. Prompt-based learning**

BERT-style fine-tuning has a subtle mismatch: its *pretraining* task (fill in masks) and its *downstream* task (classify a sentence) are different shapes, which is part of why it typically needs **thousands of labeled examples** to fine-tune well.

**Prompting** removes the mismatch by **converting the downstream task into the pretraining task itself** — a language-modeling problem. You phrase your task as text the model can simply continue:

* Sentiment analysis becomes: `"I like this movie. It was ___"` → the model fills `great` (positive) or `terrible` (negative).
* Machine translation becomes: `"Hello world! => ___"` → the model fills `Bonjour le monde!`.

GPT made this paradigm popular. The decisive demonstration was **GPT-3** — a giant transformer **decoder** (~175B parameters), trained on 500B+ tokens from CommonCrawl, WebText, and books, at a training cost reported around \$12M. It turned out to be a general-purpose language model with striking text generation and, crucially, **zero-shot / few-shot learning**: it can *understand a task specification given in plain language* and perform the task with no gradient updates at all.

**Zero-, one-, and few-shot**

The "shots" are simply *how many worked examples you place in the prompt* before asking for the answer — the model conditions on them at inference time:

| Setting | What's in the prompt | Weight updates |
|---|---|---|
| **Zero-shot** | a task description only, then the input | none |
| **One-shot** | description + **one** example, then the input | none |
| **Few-shot** | description + a few (~10) examples, then the input | none |

This is **in-context learning**: the model "learns" the task from the context window on the fly, then forgets it the moment the request ends. Nothing is stored in the weights. With this, GPT-3 could write code from a description ("a table of the richest countries with column names and GDP"), generate thought experiments from classic examples, and power the hundreds of demos catalogued at the time (search engines, NPC dialogue, poetry).

**The conceptual shift**

This is the pivot the whole lecture builds to: **from "fine-tune weights" to "condition with text."** In CV and BERT fine-tuning, adapting to a task means *changing parameters*. In prompting, adapting means *writing a better input*. The model is frozen; your leverage is the prompt — hence **prompt engineering**, the craft of choosing the wording, the template, and the example labels that make the model behave.

**Prompt-based fine-tuning (the hybrid)**

There is a middle ground for *medium-sized* LMs (e.g. < 1B parameters), where pure prompting is weaker but full fine-tuning is wasteful. **Prompt-based fine-tuning** designs a task-specific *prompt template* (and the label words it maps to) instead of bolting on a brand-new output layer, then fine-tunes the model's weights through that template. Reported result: it is roughly **100× more example-efficient** than standard fine-tuning. Because hand-designing templates and label words is finicky, there is work on **automatic prompt search** — automatically selecting the template and the label words (Gao et al., 2021).

**Section summary:** prompt-based learning presents a downstream task in language-model format. A model large enough (GPT-3) uses its *pretrained* weights directly for downstream tasks **without updating parameters**; used in fine-tuning, prompting gives much better example efficiency.

---

</details>

## 5. 特征提取、微调与 prompting

这三种策略构成一条线：*权重更新递减*、*对预训练模型原始能力的依赖递增*。在它们之间做选择，主要取决于你有多少标注数据和算力，以及 base model 有多大、多强。

| | **特征提取** | **微调** | **prompting（上下文内）** |
|---|---|---|---|
| **改变什么** | 仅新增一个 head；backbone 冻结 | 新 head **+** 全部（或顶层）backbone 权重 | 什么都不变——权重冻结 |
| **所需数据** | 低（少量标注数据即可） | 中 → 高（常需数千条标注） | 极低——零样本/单样本/少样本 |
| **算力 / 内存** | 低（无 backbone 梯度） | 高（梯度 + 模型的优化器状态） | 训练无需；仅推理 |
| **会改变特征吗？** | 否 | 是 | 否（改变的是行为，不是特征） |
| **典型模型规模** | 任意 | 小–大 | 很大（能力随规模涌现） |
| **最适用场景** | 数据少、source ≈ target、快速得到基线 | 数据充足，且任务需要适配后的特征 | 已有强通用 LM，且标注稀缺 |

一个粗略的决策准则：**从能满足你准确率门槛的最便宜方案入手。** 先试特征提取或 prompting（几分钟、少量数据）；当特征确实需要向你的领域靠拢、且你有足够的标注来支撑时，再动用微调。


<details>
<summary>English original</summary>

**5. Feature extraction vs fine-tuning vs prompting**

The three strategies trace a line of *decreasing weight updates* and *increasing reliance on the pretrained model's raw capability*. Choosing among them is mostly a question of how much labeled data and compute you have, and how large/capable your base model is.

| | **Feature extraction** | **Fine-tuning** | **Prompting (in-context)** |
|---|---|---|---|
| **What changes** | only a new head; backbone frozen | new head **+** all (or top) backbone weights | nothing — weights frozen |
| **Data needed** | low (works with little labeled data) | medium → high (often 1000s of labels) | very low — zero/one/few examples |
| **Compute / memory** | low (no backbone gradients) | high (gradients + optimizer state for the model) | none for training; inference-only |
| **Adapts the features?** | no | yes | no (conditions behavior, not features) |
| **Typical model size** | any | small–large | very large (capability emerges with scale) |
| **Best when** | small data, source ≈ target, fast baseline | enough data and the task needs adapted features | a strong general LM exists and labels are scarce |

A rough decision rule: **start with the cheapest option that meets your accuracy bar.** Try feature extraction or prompting first (minutes, little data); reach for fine-tuning when the features genuinely need to move toward your domain and you have the labels to justify it.

</details>

> **2026 更新：** 2021 年那张题为「prompt-based learning」的幻灯片，到 2026 年已膨胀为应用 ML 中最大的三个子领域。先学最初的框架 —— *用文本条件化一个冻结模型* —— 再把它向前映射：
>
> 1. **指令微调（RLHF / DPO）。** GPT-3 原始的 few-shot prompting 很聪明但很笨重；你必须去哄基座模型。解决办法是用人类偏好数据**微调模型以遵循指令** —— **RLHF**（reinforcement learning from human feedback，InstructGPT → ChatGPT）及其更简单的后继 **DPO**（direct preference optimization）。今天 zero-shot prompting 「直接就能用」是这种微调的*结果*，而不是基座 LM 的固有属性。这仍然是微调 —— 只是改变了我们微调的*目标*（有用性/安全性，而非某个单一的下游任务）。
> 2. **参数高效微调（PEFT）：LoRA / QLoRA / adapters。** 「对一个巨型模型做全量微调」对多数团队已不可行，于是我们不再更新全部权重。**LoRA** 训练极小的低秩更新矩阵，基座权重保持冻结；**QLoRA** 在 4-bit 量化的基座模型之上做同样的事，从而能装进单张 GPU；**adapters** 在冻结的层之间插入小的可训练模块。这是 §2 中「冻结主干，只训练一小部分」的*直系后代* —— 如今应用到了十亿参数的 LLM 上。
> 3. **检索增强生成（RAG）。** 与其把任务知识放进权重*或*把每个示例手写进 prompt，不如在查询时**检索**相关文档塞进上下文。RAG 是 prompting 的逻辑终局：prompt 由知识库*程序化组装*而成，让冻结的模型无需重新训练就获得新鲜、有依据的事实。
>
> 本课程有一个姊妹篇，在真实模型上端到端走通现代 PEFT 路线：**[Qwen3.5-4B Unsloth Fine-Tuning](../../03-LLM应用开发/01-Qwen3-5-4B-Unsloth微调/Guide.md)** —— 用 Unsloth 库对小型 Qwen 模型做 LoRA/QLoRA 微调。它就是 §2 的「冻结大部分，只训练一点」思路在 2026 年 LLM 上的实现。


<details>
<summary>English original</summary>

> **2026 update:** The single slide titled "prompt-based learning" in 2021 has, by 2026, exploded into three of the largest subfields in applied ML. Learn the original framing first — *condition a frozen model with text* — then map it forward:
>
> 1. **Instruction tuning (RLHF / DPO).** GPT-3's raw few-shot prompting was clever but clunky; you had to coax the base model. The fix was to **fine-tune models to follow instructions** using human preference data — **RLHF** (reinforcement learning from human feedback, InstructGPT → ChatGPT) and its simpler successor **DPO** (direct preference optimization). Zero-shot prompting "just working" today is the *result* of this tuning, not a property of the base LM. This is still fine-tuning — it just changed *what* we fine-tune *for* (helpfulness/safety, not a single downstream task).
> 2. **Parameter-efficient fine-tuning (PEFT): LoRA / QLoRA / adapters.** "Full fine-tuning of a giant model" became infeasible for most teams, so we stopped updating all the weights. **LoRA** trains tiny low-rank update matrices while the base weights stay frozen; **QLoRA** does the same on top of a 4-bit-quantized base model so it fits on a single GPU; **adapters** insert small trainable modules between frozen layers. This is the *direct descendant* of "freeze the backbone, train only a small part" from §2 — now applied to billion-parameter LLMs.
> 3. **Retrieval-augmented generation (RAG).** Instead of putting task knowledge in the weights *or* hand-writing every example into the prompt, **retrieve** relevant documents at query time and stuff them into the context. RAG is prompting taken to its logical end: the prompt is *assembled programmatically* from a knowledge base, giving the frozen model fresh, grounded facts without retraining.
>
> This course has a sibling that walks the modern PEFT path end-to-end on a real model: **[Qwen3.5-4B Unsloth Fine-Tuning](../../03-LLM应用开发/01-Qwen3-5-4B-Unsloth微调/Guide.md)** — LoRA/QLoRA fine-tuning of a small Qwen model with the Unsloth library. It is §2's "freeze most of it, train a little" idea, realized for 2026 LLMs.

</details>

> **硬件视角：** PEFT 不只是优雅之举——它存在是因为**全量微调是带宽受限的。** 更新每一个权重意味着，对每个参数都要保存权重、其梯度以及优化器的动量估计（Adam 保存两个）——在实践中，fp32 左右的混合精度训练大约需要 **~16 bytes/parameter**。在加载一个批之前，7B 模型就已经超出单张消费级 GPU 的显存。**LoRA** 绕开了这一点，只训练几百万个低秩参数，因此梯度和优化器状态缩小了几个数量级。**QLoRA** 更进一步，**把冻结的基座量化到 4-bit（NF4）**，将静态权重占用削减约 4×，使得原本需要 A100 的模型现在能装进 24 GB 的卡——LoRA 适配器保持更高精度，因为它们才是真正被训练的部分。这与支配*推理*的内存带宽和占用推理相同；量化与内存层次机制见 **[阶段 5 — ML Systems Engineering](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/Guide.md)** 和 **[MLSys Deep Dives](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/README.md)**，而 QLoRA 的动手收益见上文 [Unsloth course](../../03-LLM应用开发/01-Qwen3-5-4B-Unsloth微调/Guide.md)。

---


<details>
<summary>English original</summary>

> **Hardware lens:** PEFT is not just an elegance — it exists because **full fine-tuning is memory-bound.** Updating every weight means holding, per parameter, the weight, its gradient, and the optimizer's moment estimates (Adam keeps two) — in practice **~16 bytes/parameter** in fp32-ish mixed-precision training. A 7B model blows past a single consumer GPU's memory before you have loaded a batch. **LoRA** sidesteps this by training only a few million low-rank parameters, so gradients and optimizer state shrink by orders of magnitude. **QLoRA** goes further by **quantizing the frozen base to 4-bit (NF4)**, cutting the static weight footprint ~4× so a model that needed an A100 now fits on a 24 GB card — the LoRA adapters stay in higher precision because they are what actually trains. This is the same memory-bandwidth and footprint reasoning that governs *inference*; the quantization and memory-hierarchy mechanics live in **[Phase 5 — ML Systems Engineering](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/Guide.md)** and the **[MLSys Deep Dives](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/README.md)**, and the hands-on QLoRA payoff is the [Unsloth course](../../03-LLM应用开发/01-Qwen3-5-4B-Unsloth微调/Guide.md) above.

---

</details>

## 内容时效

本讲的主干按原版 **Stanford CS329P（2021 Fall）** 材料讲授，并直接对应幻灯片：迁移学习动机与特征提取器／分类器拆分；ImageNet 骨干网络的 CV 微调（特征提取 vs 全量微调、冻结通用的底层、小 LR／少 epoch、TIMM 与 ResNet recipe）；NLP 自监督预训练（LM vs masked-LM 目标、上下文 embedding、BERT 编码器及其 `[CLS]`/`[SEP]` 下游模式与 Hugging Face）；以及基于 prompt 的学习（GPT-3、zero/one/few-shot 上下文学习、带自动 prompt 搜索的基于 prompt 的微调）。这些内容至今仍然准确，且属于基础。

发生变化的地方，以及 **2026 更新版** 所标注的内容，几乎全部位于最后那节 prompt 内容的下游。2021 年的“基于 prompt 的学习”幻灯片如今已成为三个成熟的子领域：**指令微调**（RLHF → DPO）是今天 zero-shot prompting 显得毫不费力的原因；**参数高效微调**（LoRA/QLoRA/适配器）是团队在通用硬件上实际适配十亿参数模型的方式——它是 CV 领域“冻结网络大部分”的直接继承者；而**检索增强生成**则是用程序化组装的、有知识依据的上下文来做 prompting。在 CV 侧，ViT 骨干网络如今与 ResNet 并列成为默认预训练模型，大型自监督／图文基础模型也已把“预训练骨干网络”的范围远远扩展到 ImageNet 分类之外。需要记住的主线：该领域*从更新权重转向对冻结模型做条件化*——而在仍需更新权重之处，也以廉价（LoRA）和低精度（QLoRA）的方式进行。审阅于 2026 年 6 月。

*改编自 [Stanford CS329P](https://c.d2l.ai/stanford-cs329p) — Huang, Li & Smola, CC-BY-SA-4.0.*


<details>
<summary>English original</summary>

**Current as of**

The spine of this lecture is taught as the original **Stanford CS329P (2021 Fall)** material and tracks the slides directly: the transfer-learning motivation and the feature-extractor / classifier decomposition; CV fine-tuning of ImageNet backbones (feature extraction vs full fine-tuning, freezing the universal low-level layers, small LR / few epochs, TIMM and the ResNet recipe); NLP self-supervised pretraining (LM vs masked-LM objectives, contextual embeddings, BERT's encoder with its `[CLS]`/`[SEP]` downstream patterns and Hugging Face); and prompt-based learning (GPT-3, zero/one/few-shot in-context learning, prompt-based fine-tuning with automatic prompt search). All of that remains accurate and foundational.

What has moved, and what the **2026 refresh** flags, is almost entirely downstream of that final prompting section. The 2021 "prompt-based learning" slide is now three mature subfields: **instruction tuning** (RLHF → DPO) is why zero-shot prompting feels effortless today; **parameter-efficient fine-tuning** (LoRA/QLoRA/adapters) is how teams actually adapt billion-parameter models on commodity hardware — the direct heir of CV's "freeze most of the network"; and **retrieval-augmented generation** is prompting with a programmatically assembled, knowledge-grounded context. On the CV side, ViT backbones now sit alongside ResNets as default pretrained models, and large self-supervised / image-text foundation models have broadened "pretrained backbone" well beyond ImageNet classification. The throughline to remember: the field moved *from updating weights toward conditioning frozen models* — and where it still updates weights, it does so cheaply (LoRA) and in low precision (QLoRA). Reviewed June 2026.

*Adapted from [Stanford CS329P](https://c.d2l.ai/stanford-cs329p) — Huang, Li & Smola, CC-BY-SA-4.0.*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/4. ML Engineering and MLOps/Practical Machine Learning (CS329P)/Lecture-09.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/4.%20ML%20Engineering%20and%20MLOps/Practical%20Machine%20Learning%20%28CS329P%29/Lecture-09.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
