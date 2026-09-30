---
title: 第 01 讲 - 数据 I：采集、抓取与标注
description: 第 01 讲 - 数据 I：采集、抓取与标注
---

# 第 01 讲 - 数据 I：采集、抓取与标注

**Collection:** [Practical Machine Learning (CS329P)](README.md) | **Previous:** [← Course index](README.md) | **Next:** [Lecture 02](Lecture-02.md)

---

CS329P 以一个看似简单的承诺开篇：讲授那些*重要却常被跳过*的机器学习主题。被跳过的内容，绝大多数是数据。这门课以一条主线组织 —— **数据 → 模型训练 → 部署** —— 并把前两讲全部花在第一个词上，因为那正是称职的 ML 工程师花掉大部分时间的地方。你可以用一行代码调用 `model.fit()`。但你无法用一行代码去采集、抓取、整合并标注一个数据集，而且无论做多少架构搜索，都救不了一个从你从未正确组装的数据上训练出来的模型。

本讲要讨论的碰撞，是机器学习的学术心智模型与工业心智模型之间的落差。在学术模型里，数据集是给定的 —— `mnist`、`imagenet`、教科书附带的 CSV —— 而有趣的工作是模型。在工业模型里，在你动手构建之前*根本不存在*数据集，而「构建一个数据集」是一个项目，可能在算出第一个梯度之前就牵涉多个团队、一条存储流水线、法务审查和隐私控制。本讲梳理数据实际到达的三种途径：**找到它**（已有数据集、benchmark、竞赛、原始数据湖）、**采集它**（大规模网页抓取）、以及**标注它**（半监督学习、主动学习、弱监督、众包）。

关于这门课如何讲数据，有一点说明：**探索性数据分析**配套材料是一份动手 Jupyter notebook，而不是一讲。在你决定*如何*获取更多数据之前，先加载你已有的数据，画出分布，找出缺失值和取值冲突，让数据自己告诉你它是什么。以下所有内容都假定你已经通过 EDA 与你的数据打过照面，并得出结论：你需要更多数据、更干净的标签，或者两者都要。

---

## 学习目标

1. 在**寻找**已有数据集与**构建**新数据集之间做决策，并在正确的维度（干净程度、规模、真实度、投入）上权衡学术数据、竞赛数据与原始工业数据。
2. 规划一次**网页抓取**任务：在爬取与抓取之间做选择，用无头浏览器替代 `curl`，估算云成本，并守在法律与 ToS 护栏之内。
3. 应用**半监督学习** —— 具体来说是带置信度阈值的自训练 —— 来利用一个小规模标注集加上一个大规模未标注池。
4. 使用**主动学习**查询策略（不确定性采样、委员会查询）把标注预算花在信息量最大的样本上。
5. 用**弱监督 / 数据编程**（标注函数，à la Snorkel）大规模生成带噪标签，并在恰当的**质量控制**下将其与众包结合。
6. 把整个决策表述成一张流程图 —— *数据够了吗？→ 有外部数据集吗？→ 能生成吗？→ 有标签吗？→ 有预算吗？→ 用弱标签吗？* —— 并知道每个分支指向哪个工具。

---

## 1. 数据采集：找到它、生成它，或构建它

CS329P 把采集表述成一张流程图。你启动一个 ML 应用并问：**我的数据够吗？** 如果不够：**有没有我可以发现或整合的外部数据集？** 如果还是没有：**我有没有数据生成方法？** 每一个「没有」都把你往右推一格，代价也往上抬一档。


<details>
<summary>English original</summary>

**Lecture 01 - Data I: Acquisition, Scraping & Labeling**

**Collection:** [Practical Machine Learning (CS329P)](README.md) | **Previous:** [← Course index](README.md) | **Next:** [Lecture 02](Lecture-02.md)

---

CS329P opens with a deceptively simple promise: teach the machine-learning topics *that matter but are often skipped*. The skipped material is, overwhelmingly, the data. The course is organized as a spine — **Data → Model training → Deployment** — and it spends its first two lectures entirely on the first word, because that is where a working ML engineer spends most of their time. You can call `model.fit()` in one line. You cannot acquire, scrape, integrate, and label a dataset in one line, and no amount of architecture search will rescue a model trained on data you never properly assembled.

The collision this lecture is about is the gap between the academic mental model of ML and the industrial one. In the academic model the dataset is a given — `mnist`, `imagenet`, a CSV from the textbook — and the interesting work is the model. In the industrial model there *is* no dataset until you build one, and "building one" is a project that can involve multiple teams, a storage pipeline, legal review, and privacy controls before a single gradient is computed. This lecture walks the three ways data actually arrives: **find it** (existing datasets, benchmarks, competitions, raw data lakes), **harvest it** (web scraping at scale), and **label it** (semi-supervised learning, active learning, weak supervision, crowdsourcing).

A note on how this course teaches data: the **Exploratory Data Analysis** companion is a hands-on Jupyter notebook, not a lecture. Before you decide *how* to acquire more data, you load what you have, plot the distributions, look for the missing values and the value conflicts, and let the data tell you what it is. Everything below assumes you have already met your data through EDA and concluded you need more of it, cleaner labels, or both.

---

**Learning objectives**

1. Decide between **finding** an existing dataset and **building** a new one, and weigh academic vs. competition vs. raw industrial data on the right axes (cleanliness, scale, realism, effort).
2. Plan a **web-scraping** job: choose between crawling and scraping, use a headless browser instead of `curl`, estimate cloud cost, and stay inside the legal and ToS guardrails.
3. Apply **semi-supervised learning** — specifically self-training with confidence thresholding — to exploit a small labeled set plus a large unlabeled pool.
4. Use **active learning** query strategies (uncertainty sampling, query-by-committee) to spend a labeling budget on the most informative examples.
5. Generate noisy labels at scale with **weak supervision / data programming** (labeling functions, à la Snorkel), and combine it with crowdsourcing under proper **quality control**.
6. Frame the whole decision as a flow chart — *have enough data? → external datasets? → generation? → labels? → budget? → weak labels?* — and know which tool each branch points to.

---

**1. Data acquisition: find it, generate it, or build it**

CS329P frames acquisition as a flow chart. You start an ML application and ask: **do I have enough data?** If not: **are there external datasets** I can discover or integrate? If not: **do I have a data-generation method**? Each "no" pushes you one box to the right and one notch up in cost.

</details>

### 1.1 寻找已有数据

最便宜的数据是别人已经收集好的数据。课程梳理了经典的 ML 数据集，以及每一个实际上*是*什么——来源很重要，因为它决定了偏差：

| 数据集 | 它是什么 | 模态 |
|---|---|---|
| MNIST | 由 US Census Bureau 员工手写的数字 | 图像 |
| ImageNet | 从图片搜索引擎抓取的数百万张图像 | 图像 |
| AudioSet | 用于声音分类的 YouTube 音频片段 | 音频 |
| LibriSpeech | 约 1000 小时朗读公共领域有声书的英语语音 | 语音 |
| Kinetics | 用于人体动作分类的 YouTube 视频片段 | 视频 |
| KITTI | 车载摄像头 + LiDAR（激光雷达）采集的交通场景 | 多传感器 |
| Amazon Review | 来自 Amazon 购物的用户评论 | 文本 |
| SQuAD | 由 Wikipedia 派生的问题–答案对 | 文本 |

注意这个规律：大多数「找到的」数据集本身也是*抓取或众包*得来的（ImageNet、Kinetics 来自搜索引擎和 YouTube；SQuAD 来自 Wikipedia + MTurk）。「寻找」与「构建」之间的界线比看上去要细得多。

去哪里找，大致按原始程度递增排列：

- **Papers With Code Datasets** — 附有 leaderboard 的学术数据集，动手之前就能知道当前最优水平。
- **Kaggle Datasets** — 数据科学家上传的数据集，通常附带 notebook。
- **Google Dataset Search** — 针对 web 上任意位置发布的数据集的搜索引擎。
- **框架 hub** — TensorFlow Datasets、Hugging Face `datasets` — 一行代码即可加载。
- **竞赛** — Kaggle 以及企业/会议举办的 ML 竞赛。
- **Open Data on AWS** — 100+ 个大规模*原始*数据集。
- **你自己组织的数据湖** — 最有价值，也最混乱。

### 1.2 构建与寻找的取舍

这三类数据在干净程度与真实度、投入之间做取舍：

| 来源 | 优点 | 缺点 |
|---|---|---|
| **学术数据集** | 干净，难度经过恰当标定 | 选择有限、过度简化、通常规模小 |
| **竞赛数据集** | 更贴近真实的 ML 应用 | 仍然被简化；只存在于热门话题上 |
| **原始数据** | 完全灵活 | 处理工作量巨大 |

幻灯片给出的实用结论：**在工业界，你几乎总是要处理原始数据**，而整理原始数据是一个*大工程*——处理流水线、存储、法务审查和隐私处理，往往横跨多个团队。学术数据集只是辅助轮；真正的工作是原始数据。

课程特别指出的一项原始数据技能是**数据集成**——把多个来源合并成一个连贯的数据集。产品数据存在于多张表中（一张表存房屋属性，一张存销售记录，一张存房源经纪人）。你需要**按 key 做 join**，key 通常是实体 ID，反复出现的痛点是：确定正确的 ID、缺失行、冗余列，以及两个来源对同一字段说法不一致的*值冲突*。

### 1.3 在无数据可用时生成数据

如果没有数据集可找，流程图最右边的分支就是**生成数据**：

- **GAN** — 合成逼真的样本。幻灯片引用了 `thispersondoesnotexist.com`（合成人脸）以及合成带家具房间的图像（Gadde et al., ICCV'21）。
- **仿真** — 从模拟器渲染出标注完美的数据，这是自动驾驶中稀有事件的主流做法（后文详述）。
- **数据增强** — 日常主力手段。廉价的保标签变换能让已有标注集成倍扩张：视觉用图像增强（裁剪、翻转、颜色抖动——例如 `imgaug` 库），文本用**回译**（翻译成另一种语言再翻回来）来做改写。

课程最终给出的总结：找到对的数据很难；工业界以原始数据为常态；数据集成把多个来源缝合在一起；数据增强是标准做法；而**合成数据正变得越来越流行**。最后这一句颇有先见之明。

> **2026 更新：**「合成数据正变得越来越流行」变成了 **data-centric AI** 运动，随后又成为默认做法。2021 年的工具箱是 GAN + 增强；到 2026 年，占主导的合成数据引擎是 **LLM**。团队通过 prompt 前沿模型来生成指令微调语料、分类器训练集和评测集，再按质量过滤——这是通过数据而非权重来做能力蒸馏。从业者的风险从*数据不够*转向**模型崩塌**（用自己的模型输出训练，直到多样性衰减）和**污染**（合成或抓取的文本把你的 benchmark 泄漏进训练集）。用 LLM 生成数据时，要保留一份人工核验过的种子集、度量多样性，并留出一份干净的、可证明未被污染的评测集。

---


<details>
<summary>English original</summary>

**1.1 Finding existing data**

The cheapest data is data someone already collected. The course catalogs the canonical ML datasets and what each one actually *is* — provenance matters because it determines bias:

| Dataset | What it is | Modality |
|---|---|---|
| MNIST | Digits handwritten by US Census Bureau employees | Image |
| ImageNet | Millions of images scraped from image search engines | Image |
| AudioSet | YouTube sound clips for sound classification | Audio |
| LibriSpeech | ~1000 hours of English read from public-domain audiobooks | Speech |
| Kinetics | YouTube clips for human-action classification | Video |
| KITTI | Traffic scenes from car-mounted cameras + LiDAR | Multi-sensor |
| Amazon Review | Customer reviews from Amazon shopping | Text |
| SQuAD | Question–answer pairs derived from Wikipedia | Text |

Notice the pattern: most of the "found" datasets were themselves *scraped or crowdsourced* (ImageNet, Kinetics from search engines and YouTube; SQuAD from Wikipedia + MTurk). The line between "finding" and "building" is thinner than it looks.

Where to look, in roughly increasing order of rawness:

- **Papers With Code Datasets** — academic datasets with a leaderboard attached, so you know the state of the art before you start.
- **Kaggle Datasets** — datasets uploaded by data scientists, often with notebooks.
- **Google Dataset Search** — a search engine over datasets published anywhere on the web.
- **Framework hubs** — TensorFlow Datasets, Hugging Face `datasets` — one-line loaders.
- **Competitions** — Kaggle and company/conference ML competitions.
- **Open Data on AWS** — 100+ large-scale *raw* datasets.
- **Your own organization's data lake** — the most valuable and the messiest.

**1.2 The build-vs-find tradeoff**

The three classes of data trade off cleanliness against realism and effort:

| Source | Pros | Cons |
|---|---|---|
| **Academic datasets** | Clean, properly calibrated difficulty | Limited choices, over-simplified, usually small scale |
| **Competition datasets** | Closer to real ML applications | Still simplified; only exist for hot topics |
| **Raw data** | Total flexibility | Enormous effort to process |

The practical takeaway from the slides: **in industry you almost always deal with raw data**, and curating it is a *big project* — a processing pipeline, storage, legal review, and privacy handling, frequently spanning multiple teams. Academic datasets are training wheels; the job is raw data.

A specific raw-data skill the course calls out is **data integration** — combining multiple sources into one coherent dataset. Product data lives in multiple tables (a table for house attributes, one for sales, one for listing agents). You **join on keys**, which are usually entity IDs, and the recurring pain is identifying the right IDs, missing rows, redundant columns, and *value conflicts* where two sources disagree about the same field.

**1.3 Generating data when none exists**

If there is no dataset to find, the rightmost branch of the flow chart is **generate it**:

- **GANs** — synthesize realistic samples. The slides cite `thispersondoesnotexist.com` (synthetic faces) and synthetic furnished-room imagery (Gadde et al., ICCV'21).
- **Simulation** — render perfectly labeled data from a simulator, the dominant approach for rare events in autonomous driving (more below).
- **Data augmentation** — the everyday workhorse. Cheap label-preserving transforms multiply an existing labeled set: image augmentation (crop, flip, color jitter — e.g. the `imgaug` library) for vision, and **back-translation** (translate to another language and back) to paraphrase text.

The summary the course lands on: finding the right data is hard; raw industrial data is the norm; data integration stitches sources together; augmentation is standard practice; and **synthesizing data is getting popular**. That last line was prescient.

> **2026 update:** "synthesizing data is getting popular" became the **data-centric AI** movement and then the default. The 2021 toolkit was GANs + augmentation; in 2026 the dominant synthetic-data engine is the **LLM**. Teams generate instruction-tuning corpora, classifier training sets, and eval suites by prompting frontier models, then filter for quality — distillation of capability through data rather than weights. The practitioner's risk shifts from *not enough data* to **model collapse** (training on your own model's outputs until diversity decays) and **contamination** (synthetic or scraped text leaking your benchmark into the training set). When you generate data with an LLM, keep a human-verified seed set, measure diversity, and hold out a clean, provably-uncontaminated eval split.

---

</details>

## 2. Web scraping：没有 API 时的大规模数据获取

既没有数据集也没有 API 时，就**抓取**。目标是从网站中提取数据——数据嘈杂、标签弱，有时还充斥垃圾，但它*规模*可观，许多里程碑式的数据集（ImageNet、Kinetics）正是这样诞生的。比价或价格追踪产品，本质上就是一个带 UI 的 scraper。

首先，课程强调的术语区分：

- **Crawling**——索引互联网上整页内容（搜索引擎做的事）。
- **Scraping**——从*特定*站点的页面中提取*特定字段*（通常你想要的）。

### 2.1 工具：为什么 `curl` 行不通

朴素的做法——对 URL 执行 `curl`、解析 HTML——*常常不管用*，因为站点所有者部署了反机器人防御。标准答案是 **headless browser**：一个无 GUI 驱动的真实浏览器（Chromium），因此它会执行 JavaScript，并像人类的浏览器那样渲染页面。用浏览器的 **Inspect** 工具定位字段，找到每个字段（价格、卧室数、面积）对应的 HTML 元素，逐字段重复。

大规模抓取还需要**大量 IP 地址**，因为单个 IP 猛刷一个站点会很快被封。幻灯片指出，可以从公有云租用 IP 多样性——在所有 IPv4 地址中，AWS 拥有约 1.75%，Azure 约 0.55%，GCP 约 0.25%——当某个实例的 IP 被封时，重启它就能拿到新 IP。

### 2.2 案例研究与成本：Zillow 房源

这个实例爬取 Stanford 附近售出的房源。该模式可推广：索引页列出房源 **ID**，通过 URL 中的数字分页（`.../sold/2-p/`）；从索引中收集 ID，然后按 ID 抓取每个**详情页**（`.../homedetails/<zpid>/`），并通过检视 HTML 提取字段。

经济学才是重点——抓取*在云上很便宜*：

```text
Instance:   AWS EC2 t3.small  (2 GB RAM, 2 vCPU, ~$0.02/hr)
            2 GB is required — the headless browser is memory-hungry;
            CPU and bandwidth are rarely the bottleneck.
Speed:      ~3 seconds per page
Scale:      crawl 1,000,000 houses  →  ~$16.6 in compute
            ~8.3 hours wall-clock with 100 instances in parallel
Extras:     storage + the cost of restarting instances on IP bans

Images:     a listing has ~20 images
            crawling all images: ~$300
            STORING them: ~$300 PER MONTH  ← storage, not compute, dominates
            mitigation: downscale resolution, or stream data back and discard
```

不那么显然的教训：对于图像抓取，**存储才是经常性成本，而不是爬取**。计算是一次性的约 $300; holding the images is ~$300 *每个月*。

### 2.3 robots.txt、礼貌与法律

负责任地抓取既是工程纪律，也是法律纪律。

**礼貌（工程）。** 尊重 `robots.txt`，即站点根目录下声明机器人可访问哪些路径、并通过 `Crawl-delay` 声明速率的文件。自我限速，用真实的 `User-Agent` 标识你的机器人，出错时退避，若存在官方 API 或数据转储则优先使用。无视这些的爬虫会被封——而且活该。

**法律（能让职业生涯终结的部分）。** 课程直言不讳：web scraping *本身并不违法*，**但**：

- **不要**抓取含**敏感信息**的数据——凭证（用户名/密码）、个人健康或医疗记录。
- **不要**抓取**受版权保护**的数据——YouTube 视频、Flickr 照片之类。
- **遵守服务条款。** 如果 ToS 明确禁止抓取，该禁令对你有约束力。
- 如果你抓取**用于营利，请咨询律师。**

> **2026 更新：** 对于*生成式* AI，法律地形急剧硬化。2021 年后的诉讼（Authors Guild v. OpenAI、Getty v. Stability、NYT v. OpenAI）把抓取所得训练数据的来源问题送上了法庭，而 EU AI Act 如今要求通用目的模型提供方公布训练数据摘要，并尊重机器可读的退出机制。2026 年的实用规则是：把 `robots.txt` 和 ToS 视为*下限*，而非上限；记录每一条抓取数据的**来源与许可**；并假定「我们抓取了它，因此可以拿它训练商业模型」这一主张，有朝一日可能要在法庭上辩护。CS329P 那句「营利就请咨询律师」已经老化为一项长期要求。

---

## 3. 数据标注：把数据变成带标签的数据

抓取让你得到*数据*；监督学习需要*带标签的*数据。课程把标注也画成一张流程图：**有数据吗？→ 改进标签/表示？→ 标签够启动吗？→ 预算够吗？→ 够做弱标签吗？** 每个分支指向不同技术——半监督学习、众包，或弱监督。


<details>
<summary>English original</summary>

**2. Web scraping: data at scale when there is no API**

When there is no dataset and no API, you **scrape**. The goal is to extract data from websites — it is noisy, the labels are weak and sometimes spammy, but it is available *at scale*, and many landmark datasets (ImageNet, Kinetics) were born this way. A price-comparison or price-tracking product is essentially a scraper with a UI.

First, the vocabulary distinction the course insists on:

- **Crawling** — indexing whole pages across the internet (what a search engine does).
- **Scraping** — extracting *particular fields* from the pages of a *specific* site (what you usually want).

**2.1 Tools: why `curl` fails**

The naive approach — `curl` the URL, parse the HTML — *often doesn't work*, because site owners deploy bot defenses. The standard answer is a **headless browser**: a real browser (Chromium) driven without a GUI, so it executes JavaScript and renders the page the way a human's browser would. You locate fields with the browser's **Inspect** tool, find the HTML element for each field (price, beds, square footage), and repeat per field.

Scraping at scale also needs **many IP addresses**, because a single IP hammering a site gets banned fast. The slides note you can rent IP diversity from the public clouds — of all IPv4 addresses, AWS owns ~1.75%, Azure ~0.55%, GCP ~0.25% — and when an instance's IP is banned you restart it to get a new one.

**2.2 Case study and cost: Zillow houses**

The worked example crawls houses sold near Stanford. The pattern generalizes: index pages list house **IDs**, paginated by a number in the URL (`.../sold/2-p/`); you collect IDs from the index, then fetch each **detail page** by ID (`.../homedetails/<zpid>/`) and extract fields by inspecting the HTML.

The economics are the point — scraping is *cheap on the cloud*:

```text
Instance:   AWS EC2 t3.small  (2 GB RAM, 2 vCPU, ~$0.02/hr)
            2 GB is required — the headless browser is memory-hungry;
            CPU and bandwidth are rarely the bottleneck.
Speed:      ~3 seconds per page
Scale:      crawl 1,000,000 houses  →  ~$16.6 in compute
            ~8.3 hours wall-clock with 100 instances in parallel
Extras:     storage + the cost of restarting instances on IP bans

Images:     a listing has ~20 images
            crawling all images: ~$300
            STORING them: ~$300 PER MONTH  ← storage, not compute, dominates
            mitigation: downscale resolution, or stream data back and discard
```

The non-obvious lesson: for image scraping, **storage, not crawling, is the recurring cost**. Compute is a one-time ~$300; holding the images is ~$300 *every month*.

**2.3 robots.txt, politeness, and the law**

Scraping responsibly is both an engineering and a legal discipline.

**Politeness (engineering).** Respect `robots.txt`, the file at a site's root that declares which paths bots may touch and, via `Crawl-delay`, how fast. Rate-limit yourself, identify your bot with a real `User-Agent`, back off on errors, and prefer an official API or data dump if one exists. A scraper that ignores these gets blocked — and deserves to.

**The law (the part that ends careers).** The course is blunt: web scraping *isn't illegal by itself*, **but**:

- Do **not** scrape data with **sensitive information** — credentials (username/password), personal health or medical records.
- Do **not** scrape **copyrighted** data — YouTube videos, Flickr photos, and the like.
- **Follow the Terms of Service.** If the ToS explicitly prohibits scraping, that prohibition is binding on you.
- If you are scraping **for profit, consult a lawyer.**

> **2026 update:** the legal terrain hardened sharply for *generative* AI. Post-2021 litigation (Authors Guild v. OpenAI, Getty v. Stability, NYT v. OpenAI) put scraped-training-data provenance on trial, and the EU AI Act now obliges general-purpose model providers to publish training-data summaries and respect machine-readable opt-outs. The practical rule for 2026: treat `robots.txt` and ToS as the *floor*, not the ceiling; record the **provenance and license** of every scraped item; and assume that "we scraped it, so we can train a commercial model on it" is a claim you may one day have to defend in court. CS329P's "consult a lawyer if you do it for profit" aged into a standing requirement.

---

**3. Data labeling: turning data into labeled data**

Scraping gets you *data*; supervised learning needs *labeled* data. The course frames labeling as another flow chart: **have data? → improve label/representation? → enough labels to start? → enough budget? → enough for weak labels?** Each branch points at a different technique — semi-supervised learning, crowdsourcing, or weak supervision.

</details>

### 3.1 半监督学习与自训练

**半监督学习（SSL）** 针对的是常见情形：*少量*有标注数据集加上*大量*无标注样本池。它的做法是对数据分布做出某种假设，从而让无标注样本变得有用：

- **连续性假设** —— 特征相似的样本很可能共享同一标签。
- **聚类假设** —— 数据具有聚类结构；同一聚类中的样本往往共享同一标签。
- **流形假设** —— 数据位于一个维度远低于输入空间的流形上。

SSL 的旗舰方法是 **自训练（self-training）**，它从模型自身引导出标签：

```text
Self-training loop
  1. TRAIN   a model on the (small) labeled data.
             You may use expensive models here — deep nets, ensembles/bagging.
  2. PREDICT on the unlabeled data  →  pseudo-labels.
  3. KEEP    only the HIGH-CONFIDENCE predictions.
  4. MERGE   those pseudo-labeled points into the labeled set.
  5. Repeat.
```

第 3 步中的置信度阈值就是关键所在 —— 全部保留，就会放大自身的错误；只保留高置信度的预测，每一轮才能带来真正的信号。

### 3.2 主动学习：把预算花在刀刃上

**主动学习**面对的是同样的少量标注／大量无标注场景，但引入了**人在回路**。它与自训练的对比十分清晰，值得记牢：

- **自训练：** 模型把标签传播给它*最有信心*的数据（成本低、自动、风险低）。
- **主动学习：** 模型挑出*最有趣／最不确定*的数据，请**人**来标注（成本高、人工、信息量大）。

课程提到的两种查询策略：

| 策略 | 选择的样本满足… | 直觉 |
|---|---|---|
| **不确定性采样（uncertainty sampling）** | 预测最没信心 —— 最高类别的分数接近随机（≈ 1/n） | 模型犹豫不决，因此一个标签能解决最多疑问 |
| **委员会查询（query-by-committee）** | 模型集成*出现分歧* | 分歧标示出当前假设类尚未确定的区域 |

实践中会**把主动学习与自训练结合起来**：训练，在无标注样本池上预测，把*最*有信心的预测自动接受为伪标签（自训练），并把最没信心的那些转给人工标注员（主动学习）。对无标注数据的一遍处理，同时喂给了模型和标注队列。

### 3.3 众包与质量控制

当需要的量级超出小团队的生产能力时，就要靠**众包**。最经典的例子是 **ImageNet**，通过 **Amazon Mechanical Turk** 完成数百万张图像的标注 —— 耗时*数年、耗资数百万美元*。SageMaker Ground Truth 给出的粗略 MTurk 价目表说明了任务设计为何重要：

| 任务 | 预估价格 |
|---|---|
| 图像／文本分类 | 每个标签 $0.012 |
| 边界框 | 每个框 $0.024 |
| 语义分割 | 每张图像 $0.84 |

标注团队反复遇到三类挑战：

- **简化交互** —— 任务简单、说明清晰、UI 简洁（MIT Places365 是被引用的说明文档设计范例）。复杂任务（例如标注医学图像）需要*合格*的标注员，而不是随便什么人都行。
- **成本** —— 总成本 ≈ *#任务数 × 单任务耗时*；两方面都要降。主动学习针对的是 `#tasks`；好的 UI 针对的是单任务耗时。
- **质量控制** —— 标注员会犯错，无论有意还是无意，也会误读说明。一个边界框标注回来可能过大、过小，或者框错了目标。

标准的质量控制机制是**冗余 + 多数投票**：把同一任务发给多个标注员，取多数标签。它是最简单的方法，也是最昂贵的方法。改进做法：对有争议的样本发送*更多*份副本，并通过跟踪谁与共识不一致来**剔除低质量标注员**。

### 3.4 弱监督／数据编程

不按标签逐个付钱给人的替代方案是**以编程方式生成标签**。**弱监督**（又称 **数据编程（data programming）**，这一思路由 **Snorkel** 推广）半自动地产生标签，这些标签*不如人工标签准确，但足以用于训练*：

- 把**领域特定的启发式规则**编码为**标注函数** —— 关键词搜索、模式匹配，或调用第三方模型。
- 幻灯片中的例子：判断一条 YouTube 评论是 **spam** 还是 **ham** 的规则。
- 每个标注函数都有噪声，也可能弃权；标签模型把它们的投票（以及估计的准确率）协调成一个概率化的训练标签。

这笔取舍很明确：用*一点点*标签准确率，换来*巨大的*规模，以及当目标任务定义变化时在数秒内重新标注整个数据集的能力。


<details>
<summary>English original</summary>

**3.1 Semi-supervised learning and self-training**

**Semi-supervised learning (SSL)** targets the common case: a *small* labeled set plus a *large* unlabeled pool. It works by assuming something about the data distribution so the unlabeled points become useful:

- **Continuity assumption** — points with similar features likely share a label.
- **Cluster assumption** — the data has cluster structure; points in a cluster tend to share a label.
- **Manifold assumption** — the data lies on a manifold of far lower dimension than the input space.

The flagship SSL method is **self-training**, which bootstraps labels from the model itself:

```text
Self-training loop
  1. TRAIN   a model on the (small) labeled data.
             You may use expensive models here — deep nets, ensembles/bagging.
  2. PREDICT on the unlabeled data  →  pseudo-labels.
  3. KEEP    only the HIGH-CONFIDENCE predictions.
  4. MERGE   those pseudo-labeled points into the labeled set.
  5. Repeat.
```

The confidence threshold in step 3 is the whole game — keep everything and you amplify your own errors; keep only the confident predictions and each round adds genuine signal.

**3.2 Active learning: spend the budget where it matters**

**Active learning** is the same small-labeled / large-unlabeled scenario, but with a **human in the loop**. The contrast with self-training is exact and worth memorizing:

- **Self-training:** the model propagates labels to the data it is *most confident* about (cheap, automatic, low-risk).
- **Active learning:** the model selects the *most interesting / most uncertain* data and asks a **human** to label it (expensive, manual, high-information).

The two query strategies the course names:

| Strategy | Selects examples where… | Intuition |
|---|---|---|
| **Uncertainty sampling** | the prediction is least confident — the top class score is near random (≈ 1/n) | the model is on the fence, so a label resolves the most doubt |
| **Query-by-committee** | an ensemble of models *disagrees* | disagreement marks the regions the current hypothesis class hasn't pinned down |

In practice **active learning and self-training are combined**: train, predict on the unlabeled pool, auto-accept the *most* confident predictions as pseudo-labels (self-training), and route the *least* confident ones to human labelers (active learning). One pass through the unlabeled data feeds both the model and the annotation queue.

**3.3 Crowdsourcing and quality control**

When you need volume that a small team can't produce, you **crowdsource**. The canonical example is **ImageNet**, labeled across millions of images via **Amazon Mechanical Turk** — it took *years and millions of dollars*. SageMaker Ground Truth's rough MTurk price card shows why task design matters:

| Task | Estimated price |
|---|---|
| Image / text classification | $0.012 per label |
| Bounding box | $0.024 per box |
| Semantic segmentation | $0.84 per image |

Three labeling-team challenges recur:

- **Simplify the interaction** — easy tasks, clear instructions, a simple UI (MIT Places365 is the cited example of a well-designed instruction sheet). Complex jobs (e.g. labeling medical images) need *qualified* workers, not just any worker.
- **Cost** — total cost ≈ *#tasks × time-per-task*; reduce both. Active learning attacks `#tasks`; good UI attacks time-per-task.
- **Quality control** — labelers make mistakes, honest or not, and misread instructions. A bounding box comes back too big, too small, or around the wrong object.

The standard quality-control mechanism is **redundancy + majority voting**: send the same task to multiple labelers and take the majority label. It is the simplest method and the most expensive. The refinements: send *more* copies for the controversial examples, and **prune low-quality labelers** by tracking who disagrees with the consensus.

**3.4 Weak supervision / data programming**

The alternative to paying humans per label is to **generate labels programmatically**. **Weak supervision** (a.k.a. **data programming**, the idea popularized by **Snorkel**) semi-automatically produces labels that are *less accurate than manual ones but good enough to train on*:

- Encode **domain-specific heuristics** as **labeling functions** — keyword search, pattern matching, or calls to a third-party model.
- Example from the slides: rules to decide whether a YouTube comment is **spam** or **ham**.
- Each labeling function is noisy and may abstain; a label model reconciles their votes (and their estimated accuracies) into a single probabilistic training label.

The trade is explicit: you exchange a *little* label accuracy for *enormous* scale and the ability to relabel the entire dataset in seconds when your definition of the target changes.

</details>

### 3.5 这一切都通向哪里：自动驾驶汽车

课程以最严苛的真实案例收尾标注部分。**Tesla 和 Waymo 都运营着庞大的内部标注团队**，标注类型层层叠加：2D 和 3D 边界框、图像语义分割、3D 激光雷达点云标注、视频标注。全套工具同时登场：

- **主动学习**，用于找出需要更多数据和标注的场景。
- **机器学习自动标注**，用于预标注并让人工修正。
- **仿真**，用于为无法在真实道路上安全采集的罕见且危险场景，制造*完美标注、无限量*的数据。

总结：获取标注的方式有**自训练**（迭代标注未标注池）、**众包**（全球标注员，人工）、以及**数据编程**（启发式程序，有噪声）。而元要点是——如果各种形式的标注都过于昂贵，那就重新考虑你是否真的需要它们：**无监督学习与自监督学习**完全绕开了标注问题。

> **2026 更新：** 自监督预训练赢得了该幻灯片暗示的框架。2026 年默认流水线是*在未标注数据上自监督预训练，然后仅为下游任务标注一小部分数据*——这正是 SSL 范式，但“未标注”阶段如今承担了大部分工作。程序化标注从 Snorkel 风格的函数泛化到 **大语言模型即标注器**：提示一个强模型进行标注，将其输出视为一个（非常能干的）有噪声标注函数，并依据廉价的人工抽查对其进行调和。经济学翻转了——瓶颈不再是*获取*标注，而是*信任*标注，这就是为什么质量控制（冗余、共识、审计自动标注器）如今是本节课中扩展性最差且最重要的部分。

---

## 截至当前

本讲的主干——采集流程图、数据集目录及学术-vs-竞赛-vs-原始数据的取舍、按键连接的数据集成、GAN/仿真/增强生成、带成本计算的 Zillow 抓取案例研究、robots.txt/ToS/法律警示，以及标注栈（自训练、带不确定性采样和委员会查询的主动学习、带多数投票质量控制的众包，以及 Snorkel 风格的数据编程）——按 **原始 Stanford CS329P (2021 Fall)** 材料讲授，并直接跟随幻灯片。**2026 刷新**层标出自那时以来发生的变化：大语言模型生成的合成数据和以数据为中心的 AI 运动、生成模型训练数据日益强化的法律环境（EU AI Act 和 2023–2025 年抓取诉讼），以及大语言模型即标注器的程序化标注及其新的信任与污染失效模式。探索性数据分析配套材料仍是动手 notebook，而不是一讲。审阅于 2026 年 6 月。

*改编自 [Stanford CS329P](https://c.d2l.ai/stanford-cs329p) — Huang, Li & Smola, CC-BY-SA-4.0。*


<details>
<summary>English original</summary>

**3.5 Where it all goes: self-driving cars**

The course closes labeling with the most demanding real example. **Tesla and Waymo both run large in-house labeling teams**, and the label types stack up: 2D and 3D bounding boxes, image semantic segmentation, 3D LiDAR point-cloud annotation, video annotation. The full toolkit appears at once:

- **Active learning** to find the scenarios that need more data and labels.
- **ML auto-labeling** to pre-label and let humans correct.
- **Simulation** to manufacture *perfectly labeled, unlimited* data for the rare and dangerous situations you can't safely collect on a real road.

The summary: the ways to get labels are **self-training** (iteratively label the unlabeled pool), **crowdsourcing** (global labelers, manual), and **data programming** (heuristic programs, noisy). And the meta-point — if labels are too expensive in every form, reconsider whether you need them at all: **unsupervised and self-supervised learning** sidestep the labeling problem entirely.

> **2026 update:** self-supervised pretraining won the framing the slide hints at. The default 2026 pipeline is *pretrain self-supervised on unlabeled data, then label only a small set for the downstream task* — exactly the SSL regime, but the "unlabeled" stage now does most of the work. Programmatic labeling generalized from Snorkel-style functions to **LLM-as-labeler**: prompt a strong model to annotate, treat its output as a (very capable) noisy labeling function, and reconcile it against cheap human spot-checks. The economics flipped — the bottleneck is no longer *getting* labels but *trusting* them, which is why quality control (redundancy, consensus, auditing the auto-labeler) is now the part of this lecture that scales worst and matters most.

---

**Current as of**

The spine of this lecture — the acquisition flow chart, the dataset catalog and the academic-vs-competition-vs-raw tradeoff, data integration by key joins, GAN/simulation/augmentation generation, the Zillow scraping case study with its cost arithmetic, the robots.txt/ToS/legal cautions, and the labeling stack (self-training, active learning with uncertainty sampling and query-by-committee, crowdsourcing with majority-vote quality control, and Snorkel-style data programming) — is taught as the **original Stanford CS329P (2021 Fall)** material and tracks the slides directly. The **2026 refresh** layer flags what moved since: LLM-generated synthetic data and the data-centric-AI movement, the hardened legal landscape for generative-model training data (the EU AI Act and the 2023–2025 scraping litigation), and LLM-as-labeler programmatic annotation with its new trust-and-contamination failure modes. The Exploratory Data Analysis companion remains a hands-on notebook, not a lecture. Reviewed June 2026.

*Adapted from [Stanford CS329P](https://c.d2l.ai/stanford-cs329p) — Huang, Li & Smola, CC-BY-SA-4.0.*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/4. ML Engineering and MLOps/Practical Machine Learning (CS329P)/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/4.%20ML%20Engineering%20and%20MLOps/Practical%20Machine%20Learning%20%28CS329P%29/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
