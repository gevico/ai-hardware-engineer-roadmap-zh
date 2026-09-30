---
title: MLSys 深度剖析 —— 2026 机器学习系统全景
description: MLSys 深度剖析 —— 2026 机器学习系统全景
---

# MLSys 深度剖析 —— 2026 机器学习系统全景

<div class="course-identity mlsys" markdown="1">
<div class="course-identity__icon">MLS</div>
<div markdown="1">
<p class="course-identity__eyebrow">阶段 5 · ML 系统工程 · 专题课程</p>
<p class="course-identity__title">把整个现代 MLSys 栈看作一个彼此连接的系统：kernel 语言、编译器与 runtime、后 Transformer 架构、推理加速算法，以及把它们串在一起的经济学。</p>
<p class="course-identity__meta">产物：针对单个模型 + 目标平台的优化阶梯报告 · 度量：tokens/s、TTFT/TPOT、TOK/$、perf/watt、接受长度</p>
</div>
</div>

> *模型决定什么有可能实现。系统决定要付出多少成本。到 2026 年，系统本身就是产品。*

前沿模型是一个研究成果。而一个**以有人愿意支付的价格提供推理服务**的前沿模型，则是机器学习系统成果——这两者之间的差距，正是本领域所在之处。2023 年至 2026 年间，GPT-3.5 级智能的价格下降了约 **280×**，GPT-4 级的输入价格下降了 **超过 99%**。而这几乎都不是来自更大的 GPU。它来自 MLSys：更好的 kernel、更聪明的编译器、更精简的架构，以及三年前尚不存在的 decode（逐 token 生成阶段）算法。

本课程是对 2026 年这一栈现状的一次连通式巡览。它不是彼此孤立主题的综述——而是一个统一的论证，用七讲讲出来：**kernel 层、编译器层、架构层与推理算法层是一个协同设计的系统**，其中任何一层的每一次收益，都会直接、可测量地体现在每秒 token 数与每 token 成本上。

**层级映射：** L3–L8 —— kernel、代码生成、编译器、runtime、调度，以及其上的经济学。这就是 MLSys 工程师的全部视野。

**目标岗位：** MLSys 工程师 · ML 编译器工程师 · AI 推理工程师 · GPU kernel 工程师 · 模型-系统协同设计工程师 · 边缘/物理 AI 工程师。

**前置要求：**

* 阶段 5 —— ML 系统工程 —— [指南](../Guide.md) 以及 [AI Inference Engineer 2026](../01-AI推理工程师2026/README.md) 课程（与本课程配套的推理服务栈课程）。
* 阶段 5 —— 边缘 AI —— [Edge LLM Inference Internals](../../03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md) —— roofline（性能上界模型）、GEMV（矩阵-向量乘）与 GEMM（矩阵-矩阵乘）之辨，以及为什么 decode 是带宽受限的。本课程的每一处“Measure it”都以此为前提。
* 能顺畅阅读 Python、带 CUDA/Triton 风格的 kernel 代码以及模型卡。无需编译器内部原理的先备知识。

**搭配课程：** [TVM Deep Dives](../06-TVM深度探索/README.md) 课程（对某一个编译器的动手构建式深入巡览）与 [AI Inference Engineer 2026](../01-AI推理工程师2026/README.md)（生产级推理服务栈）。本课程讲的是*全景*；那两门课则是对其中若干局部的*深度探针*。

---

## 本课程为什么这样组织

大多数 MLSys 内容都是一堆缩写词。本课程把它们串到一条主线上——**经济价值**——因为真正决定哪项技术能落地的正是它：

```text
   price per token  =  ( energy  +  capital )  /  tokens-per-second
                          └── hardware ──┘        └──── MLSys ────┘

   every kernel, compiler pass, architecture choice, and decode trick
   in this course is a lever on the denominator. that is why it exists.
```

因此这条弧线从度量出发，向下穿过整个栈，再回升到部署边缘：

1. **经济学** —— 价值层、各项度量，以及为什么 MLSys *就是*如今的产品。
2. **Kernel 语言** —— 2026 年一个快 kernel 是怎么写出来的（分块革命）。
3. **编译器与 runtime** —— kernel 如何被调度、融合，并在规模化下保持常驻。
4. **架构，第 1 部分** —— *模型*本身如何被重新设计，使其推理服务成本更低（SSM、混合架构）。
5. **架构，第 2 部分** —— 把 2026 年的前沿模型当作系统产物来读（MoE（混合专家模型）、MLA、MTP）。
6. **推理算法** —— 在不改动模型的前提下让 decode 变快（投机解码、Flash kernel）。
7. **边缘与顶点项目** —— 1000-TOPS 硬件、端侧模型，以及用数字验证整条阶梯。

---


<details>
<summary>English original</summary>

**MLSys Deep Dives — The 2026 Machine-Learning-Systems Landscape**

<div class="course-identity mlsys" markdown="1">
<div class="course-identity__icon">MLS</div>
<div markdown="1">
<p class="course-identity__eyebrow">Phase 5 · ML Systems Engineering · Special Course</p>
<p class="course-identity__title">The whole modern MLSys stack as one connected system: kernel languages, compilers and runtimes, post-transformer architectures, inference-acceleration algorithms, and the economics that ties them together.</p>
<p class="course-identity__meta">Artifact: an optimization-ladder report for one model + target · Measure: tokens/s, TTFT/TPOT, TOK/$, perf/watt, acceptance length</p>
</div>
</div>

> *The model decides what's possible. The system decides what it costs. In 2026, the system is the product.*

A frontier model is a research result. A frontier model **served at a price someone will pay** is a machine-learning-systems result — and the gap between those two is where this field lives. Between 2023 and 2026 the price of GPT-3.5-class intelligence fell roughly **280×**, and GPT-4-class input dropped **over 99%**. Almost none of that came from a bigger GPU. It came from MLSys: better kernels, smarter compilers, leaner architectures, and decode algorithms that didn't exist three years ago.

This course is a connected tour of that stack as it stands in 2026. Not a survey of disconnected topics — a single argument, told in seven lectures, that the **kernel layer, the compiler layer, the architecture layer, and the inference-algorithm layer are one co-designed system**, and that every win in any of them is a direct, measurable move on tokens-per-second and cost-per-token.

**Layer mapping:** L3–L8 — kernels, codegen, compilers, runtime, scheduling, and the economics on top. This is the MLSys-engineer's whole field of view.

**Role targets:** MLSys Engineer · ML Compiler Engineer · AI Inference Engineer · GPU Kernel Engineer · Model-Systems Co-design Engineer · Edge/Physical-AI Engineer.

**Prerequisites:**

* Phase 5 — ML Systems Engineering — [Guide](../Guide.md) and the [AI Inference Engineer 2026](../01-AI推理工程师2026/README.md) course (the serving-stack companion to this one).
* Phase 5 — Edge AI — [Edge LLM Inference Internals](../../03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md) — the roofline, GEMV-vs-GEMM, and why decode is memory-bound. Every "Measure it" here assumes it.
* Comfort reading Python, CUDA/Triton-flavored kernel code, and model cards. No prior compiler-internals knowledge needed.

**Pairs with:** the [TVM Deep Dives](../06-TVM深度探索/README.md) course (a deep build-it tour of one compiler) and [AI Inference Engineer 2026](../01-AI推理工程师2026/README.md) (the production serving stack). This course is the *landscape*; those two are *depth probes* into pieces of it.

---

**Why this course is structured the way it is**

Most MLSys content is a pile of acronyms. This course threads them onto one spine — **economic value** — because that is what actually decides which technique ships:

```text
   price per token  =  ( energy  +  capital )  /  tokens-per-second
                          └── hardware ──┘        └──── MLSys ────┘

   every kernel, compiler pass, architecture choice, and decode trick
   in this course is a lever on the denominator. that is why it exists.
```

So the arc moves from the metric down through the stack and back up to the deployment edge:

1. **The economics** — the value layer, the metrics, why MLSys *is* the product now.
2. **Kernel languages** — how a fast kernel gets written in 2026 (the tile revolution).
3. **Compilers & runtimes** — how kernels get scheduled, fused, and kept resident at scale.
4. **Architectures, part 1** — how the *model* itself was redesigned to be cheap to serve (SSMs, hybrids).
5. **Architectures, part 2** — the 2026 frontier models read as systems artifacts (MoE, MLA, MTP).
6. **Inference algorithms** — making decode fast without changing the model (speculative decoding, Flash kernels).
7. **The edge & the capstone** — 1000-TOPS hardware, on-device models, and proving the whole ladder with numbers.

---

</details>

## 课程地图（7 讲）

<div class="lecture-map" markdown>

| # | 讲 | 主线 |
|---|---------|-----------|
| [01](Lecture-01.md) | **MLSys 作为经济价值层** —— 推理成本的崩塌、指标栈（tokens/s、TOK/$、TCO/Mtok、perf/watt），以及为什么系统工作*就是*产品 | 主干 |
| [02](Lecture-02.md) | **kernel 语言的爆发** —— 分块作为新的 ISA：Triton、CUTLASS/CuTe/cuTile、ThunderKittens、TileLang，以及易用↔可控谱系 | kernel 是怎么写出来的 |
| [03](Lecture-03.md) | **编译器与 runtime** —— TVM（自动调度）、Mojo/MAX（一门真正的语言）、TensorRT-LLM（闭源厂商方案）、IREE，以及 TileRT（megakernel runtime） | kernel 如何大规模运行 |
| [04](Lecture-04.md) | **超越稠密 Transformer** —— Mamba/SSM、线性 attention，以及混合架构浪潮：Nemotron-H、Jamba、Falcon-H1、MiniMax-01 | 把模型做便宜 |
| [05](Lecture-05.md) | **作为系统产物的 2026 前沿** —— Qwen3、Llama Nemotron Ultra 253B、Xiaomi MiMo、DeepSeek V3/R1，以及 MoE（混合专家模型） + MLA + MTP 栈 | 读懂模型卡 |
| [06](Lecture-06.md) | **让 decode 变快** —— 投机解码（EAGLE-3、Medusa、Sequoia）、DFlash、Flash kernel（FA-3、FlashInfer），以及 Together AI 的研究路线 | 算法层 |
| [07](Lecture-07.md) | **边缘与物理 AI 前沿** —— 1000-TOPS 硬件（Jetson/DRIVE Thor）对比 1000-tok/s 里程碑（MiMo + TileRT）、端侧模型，以及结课项目 | 闭合协同设计回路 |

</div>

---

## 课程产出

学完后你应当能够：

* 读懂任意一份 2026 年的模型卡或推理服务配置，并预测其**成本形态** —— tokens/s 区间、KV-cache 行为、稠密与稀疏计算、主导精度、draft 机制。
* 用数据手册中的**带宽上限**（`tokens/s ≤ HBM GB/s ÷ bytes per token`）预测任意模型的 batch-1 decode 速度，并用它校验任何厂商的吞吐宣称。
* 把任意 kernel 工具（Triton、CUTLASS/cuTile、ThunderKittens、TileLang、TVM、Mojo、TensorRT）放到**易用↔可控谱系**上，并为某个工作负载选出一个，且给出站得住脚的理由。
* 解释为什么**SSM/混合架构与 MLA** 能消灭 KV-cache 增长，为什么 **MoE** 把容量与计算解耦，以及各自在内存与互连上让你付出什么代价。
* 搭起**投机解码**（并解释 EAGLE-3、DFlash、Medusa 的差别），读懂接受长度 / 加速比之间的取舍。
* 把每一项优化 —— kernel、编译器、架构、decode 算法 —— 都落回到**TOK/$ 与 tokens/s/watt** 的数字上，并能为之辩护。

---

## 时效性 / 刷新纪律

这个领域每周都在变，这里若干主题（DFlash、MiMo + TileRT 的 1000-tok/s 里程碑、CuTe DSL / cuTile、Falcon-H1R、Qwen3）都是 2025–2026 年的新进展。因此本课程遵循与 [AI Inference Engineer 2026](../01-AI推理工程师2026/README.md) 相同的纪律：

* 每一讲都以一个 **`## Current as of`** 日期结尾，并列出所锚定的具体版本 / 宣称。
* **已有定论的研究**（Mamba、FlashAttention-3、EAGLE-3、DeepSeek MLA）直接陈述；**极新或厂商自报**的数字（MiMo 的约 1200 tok/s、Together Inference Engine 的 benchmark）会明确标注并给出出处。
* 把印出来的吞吐数字当作**教学锚点**，而不是部署时的真相 —— 软件栈的进步每月都会推动它们变化。要获取实时的跨栈数字，请使用持续更新的公开 benchmark，例如 **[SemiAnalysis InferenceMAX / InferenceX](https://newsletter.semianalysis.com/p/inferencemax-open-source-inference)**。

---

## 达成标准

当你能做到以下各点时，本课程就算完成：

* 凭记忆画出 `price = (energy + capital) / tokens-per-second` 图，并把每一讲的主题作为一根杠杆放到图上。
* 拿一个模型，让它沿**优化阶梯**走一遍 —— 基线 → 量化 → 投机解码 → 调优的 kernel/编译器 —— 在每一级测量 tokens/s 与 TOK/$，并解释每一级挪动了哪条 roofline（性能上界模型）边界。
* 面不改色地论证：1000-TOPS 边缘盒子上的混合架构 7B，与数据中心里 1000 tok/s 的 1T 参数 MoE，为什么是**同一套 MLSys 学科**指向两种预算。

如果你能叫出这些技术的名字，却无法把其中任何一个连到成本数字上，那你手里只是卡片。本课程的重点正是这种连接。

---

*相关：[TVM Deep Dives](../06-TVM深度探索/README.md) · [AI Inference Engineer 2026](../01-AI推理工程师2026/README.md) · [阶段 5 — ML Systems Engineering Guide](../Guide.md)*


<details>
<summary>English original</summary>

**Course Map (7 lectures)**

<div class="lecture-map" markdown>

| # | Lecture | The thread |
|---|---------|-----------|
| [01](Lecture-01.md) | **MLSys as the economic-value layer** — the inference-cost collapse, the metric stack (tokens/s, TOK/$, TCO/Mtok, perf/watt), why systems work *is* the product | the spine |
| [02](Lecture-02.md) | **The kernel-language explosion** — tiles as the new ISA: Triton, CUTLASS/CuTe/cuTile, ThunderKittens, TileLang, and the ease↔control spectrum | how a kernel is written |
| [03](Lecture-03.md) | **Compilers & runtimes** — TVM (autoscheduling), Mojo/MAX (a real language), TensorRT-LLM (closed vendor), IREE, and TileRT (the megakernel runtime) | how kernels run at scale |
| [04](Lecture-04.md) | **Beyond the dense transformer** — Mamba/SSMs, linear attention, and the hybrid wave: Nemotron-H, Jamba, Falcon-H1, MiniMax-01 | the model made cheap |
| [05](Lecture-05.md) | **The 2026 frontier as systems artifacts** — Qwen3, Llama Nemotron Ultra 253B, Xiaomi MiMo, DeepSeek V3/R1, and the MoE + MLA + MTP stack | reading a model card |
| [06](Lecture-06.md) | **Making decode fast** — speculative decoding (EAGLE-3, Medusa, Sequoia), DFlash, Flash kernels (FA-3, FlashInfer), and the Together AI research line | the algorithm layer |
| [07](Lecture-07.md) | **The edge & physical-AI frontier** — 1000-TOPS hardware (Jetson/DRIVE Thor) vs the 1000-tok/s milestone (MiMo + TileRT), on-device models, and the capstone | closing the co-design loop |

</div>

---

**Course Outcomes**

By the end you should be able to:

* Read any 2026 model card or serving config and predict its **cost shape** — tokens/s regime, KV-cache behavior, dense vs sparse compute, dominant precision, draft mechanism.
* Predict any model's batch-1 decode speed from a datasheet with the **bandwidth ceiling** (`tokens/s ≤ HBM GB/s ÷ bytes per token`), and use it to sanity-check any vendor throughput claim.
* Place any kernel tool (Triton, CUTLASS/cuTile, ThunderKittens, TileLang, TVM, Mojo, TensorRT) on the **ease↔control spectrum** and pick one for a workload with a defensible reason.
* Explain why **SSM/hybrid architectures and MLA** kill KV-cache growth, why **MoE** decouples capacity from compute, and what each costs you in memory and interconnect.
* Stand up **speculative decoding** (and explain EAGLE-3 vs DFlash vs Medusa), and read the acceptance-length / speedup tradeoff.
* Tie every optimization — kernel, compiler, architecture, decode algorithm — back to a **TOK/$ and tokens/s/watt** number, and defend it.

---

**Currency / Refresh Discipline**

This field moves weekly, and several topics here (DFlash, the MiMo + TileRT 1000-tok/s milestone, CuTe DSL / cuTile, Falcon-H1R, Qwen3) are 2025–2026 developments. So this course follows the same discipline as [AI Inference Engineer 2026](../01-AI推理工程师2026/README.md):

* Every lecture closes with a **`## Current as of`** date and the specific versions / claims it pinned.
* **Established research** (Mamba, FlashAttention-3, EAGLE-3, DeepSeek MLA) is stated plainly; **very recent or vendor-reported** numbers (MiMo's ~1200 tok/s, Together Inference Engine benchmarks) are explicitly flagged as such, with the source.
* Treat printed throughput numbers as **teaching anchors**, not truth-at-deployment — software-stack gains move them monthly. For live cross-stack numbers, use a continuously-updated public benchmark such as **[SemiAnalysis InferenceMAX / InferenceX](https://newsletter.semianalysis.com/p/inferencemax-open-source-inference)**.

---

**Exit Criteria**

You are done with this course when you can:

* Draw the `price = (energy + capital) / tokens-per-second` diagram from memory and place every lecture's topic on it as a lever.
* Take one model and walk it down an **optimization ladder** — baseline → quantize → speculative decode → tuned kernels/compiler — measuring tokens/s and TOK/$ at each rung, and explain which roofline bound each rung moved.
* Argue, with a straight face, why a hybrid-architecture 7B on a 1000-TOPS edge box and a 1T-param MoE at 1000 tok/s in a datacenter are **the same MLSys discipline** pointed at two budgets.

If you can name the techniques but can't connect any of them to a cost number, you have flashcards. The point of this course is the connection.

---

*Related: [TVM Deep Dives](../06-TVM深度探索/README.md) · [AI Inference Engineer 2026](../01-AI推理工程师2026/README.md) · [Phase 5 — ML Systems Engineering Guide](../Guide.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/MLSys Deep Dives/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/MLSys%20Deep%20Dives/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
