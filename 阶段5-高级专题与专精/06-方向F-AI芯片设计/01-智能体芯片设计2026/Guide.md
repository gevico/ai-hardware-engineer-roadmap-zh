---
title: 智能体化芯片设计 2026
description: 智能体化芯片设计 2026
---

# 智能体化芯片设计 2026

<div class="course-identity ai-chip-design" markdown="1">
<div class="course-identity__icon">ACD</div>
<div markdown="1">
<p class="course-identity__eyebrow">专题课程 · AI Chip Design × AI Agents</p>
<p class="course-identity__title">在 RTL 到硅的完整流程中使用 LLM 与 agent —— 生成、验证，以及智能体化 EDA。</p>
<p class="course-identity__meta">产物：一个 RTL 生成 + 自验证 agent · 度量：pass@1、功能覆盖率、PPA、$/已验证模块</p>
</div>
</div>

**上级课程：** [AI Chip Design](../Guide.md) · **衔接课程：** [AI Agent Development 2026](../../../阶段3-人工智能/04-智能体AI与ML工程/01-智能体AI与生成式AI/Guide.md) · [AI Inference Engineer 2026](../../07-方向G-ML系统工程/01-AI推理工程师2026/README.md)

> *把你在 AI Agent Development 中构建的 agent harness（agent 运行时框架）用到最难的验证问题上：设计芯片本身。*

本课程处于两条课程线的交叉点。它从 **AI Agent Development** 取来 harness —— run loop、工具、护栏、评估。它从 **AI Chip Design** 取来目标 —— RTL、验证、综合、PPA，以及硅毫不宽容的成本经济学。核心论点：随着硬件节奏加速（NVIDIA 在 Computex / GTC Taipei 2026 上让 **Vera Rubin 进入全面量产**，Grace-Blackwell 机架如今约 5 分钟即可组装完成），瓶颈向上游转移到**设计与验证吞吐** —— 而这正是 LLM agent 开始能帮上忙的地方。

**前置要求：** 熟悉 Verilog/RTL 与基本 ASIC 流程（[AI Chip Design](../Guide.md) 第 01–05 讲），以及 agent 基础知识（[AI Agent Development 2026](../../../阶段3-人工智能/04-智能体AI与ML工程/01-智能体AI与生成式AI/Guide.md) 模块 1–3：harness、工具、护栏、评估）。

**目标岗位：** 设计自动化 / AI-for-EDA 工程师 · RTL 工程师（agent 增强）· ML-for-hardware 研究员。

---

## 为什么开这门课，为什么是现在

* **设计生产力缺口。** 晶体管预算与产品节奏正在超出人类 RTL+验证的吞吐。tapeout 的主要成本是**验证**，而非生成，而这恰恰是那种重判断、非结构化数据的工作，正适合交给 agent（见 [AI Agent Development · "when to build an agent"](../../../阶段3-人工智能/04-智能体AI与ML工程/01-智能体AI与生成式AI/01-讲座/Lecture-03.md)）。
* **已有先例。** NVIDIA 的 **ChipNeMo**（2023）展示了经领域适配的 LLM 在生产设计组织内部做真正的芯片设计工作 —— 工程问答、EDA 脚本生成、bug 报告摘要。此后该领域迅速扩展到 RTL 生成、testbench 综合与多 agent EDA 流程。
* **一个活排行榜。** **[Chip-Design-LLM-Zoo](https://iprc-dip.github.io/Chip-Design-LLM-Zoo/)** 按 **pass@1 / correct-rate** 追踪 RTL 生成模型在共享 benchmark（VerilogEval、RTLLM、RealBench）上的表现，并区分微调与 base、开源与闭源权重。我们把它当作*阅读时刻的事实*benchmark，就像推理课程使用实时 benchmark 仪表盘那样。
* **2026 年的硬件背景。** Computex / GTC Taipei 2026（6 月 1–5 日）凸显了这一节奏：**Vera Rubin 全面量产**、**Cosmos 3**（一个完全开放的 omnimodel，把文本/图像/视频/音频映射为 action），以及 AI 向技术栈每一层的广泛推进。硅越快 ⇒ *设计*环路承受的压力越大。

> **诚实 / 时效性说明。** 模型排名、benchmark 分数和「最好的 RTL LLM」每几个月就变一次 —— 引用任何数字之前，务必查看实时 [Zoo 排行榜](https://iprc-dip.github.io/Chip-Design-LLM-Zoo/) 与原始论文。本课程讲的是*稳定*的那一层：流程、agent 在哪里切入、如何评估它们，以及让验证和 human-in-the-loop 不可省略的硅成本纪律。

---

## 课程大纲

### 模块 1 · 基础

<div class="lecture-map" markdown>

| # | 讲 |
|---|---------|
| [01](01-讲义/Lecture-01.md) | 为什么芯片设计要用 agent —— 2026 格局 *(从这里开始)* |
| 02 | RTL 到硅的流程，以及 agent 的切入点（spec → RTL → verify → synth → PPA → physical → signoff） |

</div>

### 模块 2 · 核心任务 —— RTL 生成及其评估

<div class="lecture-map" markdown>

| # | 讲 |
|---|---------|
| 03 | 用于 RTL / Verilog 生成的 LLM —— 模型、微调与 Chip-Design-LLM-Zoo |
| 04 | 评估纪律 —— VerilogEval、RTLLM、RealBench；pass@1、correct-rate、语法与功能之分 |

</div>


<details>
<summary>English original</summary>

**Agentic Chip Design 2026**

<div class="course-identity ai-chip-design" markdown="1">
<div class="course-identity__icon">ACD</div>
<div markdown="1">
<p class="course-identity__eyebrow">Special Course · AI Chip Design × AI Agents</p>
<p class="course-identity__title">Using LLMs and agents across the RTL-to-silicon flow — generation, verification, and agentic EDA.</p>
<p class="course-identity__meta">Artifact: an RTL-generation + self-verification agent · Measure: pass@1, functional coverage, PPA, $ / verified-module</p>
</div>
</div>

**Parent:** [AI Chip Design](../Guide.md) · **Bridges:** [AI Agent Development 2026](../../../阶段3-人工智能/04-智能体AI与ML工程/01-智能体AI与生成式AI/Guide.md) · [AI Inference Engineer 2026](../../07-方向G-ML系统工程/01-AI推理工程师2026/README.md)

> *Apply the agent harness you built in AI Agent Development to the hardest verification problem there is: designing the chip itself.*

This course sits at the intersection of two tracks. From **AI Agent Development** it takes the harness — run loops, tools, guardrails, evaluation. From **AI Chip Design** it takes the target — RTL, verification, synthesis, PPA, and the unforgiving economics of silicon. The thesis: as the hardware cadence accelerates (NVIDIA put **Vera Rubin into full production** at Computex / GTC Taipei 2026, with a Grace-Blackwell rack now assembled in ~5 minutes), the bottleneck moves upstream to **design and verification throughput** — exactly where LLM agents are starting to help.

**Prerequisites:** comfort with Verilog/RTL and the basic ASIC flow ([AI Chip Design](../Guide.md) lectures 01–05), and the agent fundamentals ([AI Agent Development 2026](../../../阶段3-人工智能/04-智能体AI与ML工程/01-智能体AI与生成式AI/Guide.md) Modules 1–3: harness, tools, guardrails, evaluation).

**Role targets:** Design-automation / AI-for-EDA Engineer · RTL Engineer (agent-augmented) · ML-for-hardware researcher.

---

**Why this course, and why now**

* **A design-productivity gap.** Transistor budgets and product cadence are outrunning human RTL+verification throughput. **Verification** — not generation — is the dominant cost of a tapeout, and it is exactly the kind of judgment-heavy, unstructured-data work that agents are suited to (see [AI Agent Development · "when to build an agent"](../../../阶段3-人工智能/04-智能体AI与ML工程/01-智能体AI与生成式AI/01-讲座/Lecture-03.md)).
* **Precedent exists.** NVIDIA's **ChipNeMo** (2023) showed domain-adapted LLMs doing real chip-design work — engineering Q&A, EDA-script generation, and bug-report summarization — inside a production design org. The field has since exploded into RTL generation, testbench synthesis, and multi-agent EDA flows.
* **A living leaderboard.** The **[Chip-Design-LLM-Zoo](https://iprc-dip.github.io/Chip-Design-LLM-Zoo/)** tracks RTL-generation models against shared benchmarks (VerilogEval, RTLLM, RealBench) by **pass@1 / correct-rate**, distinguishing fine-tuned vs base and open vs closed weights. We use it as the *truth-at-time-of-reading* benchmark, the way the inference course uses live benchmark dashboards.
* **The 2026 hardware backdrop.** Computex / GTC Taipei 2026 (June 1–5) underscored the cadence: **Vera Rubin in full production**, **Cosmos 3** (a fully-open omnimodel mapping text/image/video/audio → action), and a broad push of AI into every layer of the stack. Faster silicon ⇒ more pressure on the *design* loop.

> **Honesty / currency note.** Model rankings, benchmark scores, and "best RTL LLM" change every few months — always check the live [Zoo leaderboard](https://iprc-dip.github.io/Chip-Design-LLM-Zoo/) and primary papers before quoting a number. This course teaches the *stable* layer: the flow, where agents fit, how to evaluate them, and the silicon-cost discipline that makes verification and human-in-the-loop non-negotiable.

---

**Curriculum**

**Module 1 · Foundations**

<div class="lecture-map" markdown>

| # | Lecture |
|---|---------|
| [01](01-讲义/Lecture-01.md) | Why agents for chip design — the 2026 landscape *(start here)* |
| 02 | The RTL-to-silicon flow and where agents fit (spec → RTL → verify → synth → PPA → physical → signoff) |

</div>

**Module 2 · The core task — RTL generation and its evaluation**

<div class="lecture-map" markdown>

| # | Lecture |
|---|---------|
| 03 | LLMs for RTL / Verilog generation — models, fine-tuning, and the Chip-Design-LLM-Zoo |
| 04 | Evaluation discipline — VerilogEval, RTLLM, RealBench; pass@1, correct-rate, syntax vs functionality |

</div>

</details>

### Module 3 · Beyond generation

<div class="lecture-map" markdown>

| # | Lecture |
|---|---------|
| 05 | 用 agent 做验证与 testbench 生成——真正的瓶颈、覆盖率收敛 |
| 06 | 智能体化 EDA 流程——多 agent 的 spec→RTL→verify→debug 循环、对 EDA 工具的工具调用（harness 的应用） |
| 07 | 用 agent 做 PPA 优化与调试循环 |

</div>

### Module 4 · Systems & practice

<div class="lecture-map" markdown>

| # | Lecture |
|---|---------|
| 08 | 推理服务设计 agent——推理成本、Blackwell/Vera Rubin 上下文（衔接 AI Inference Engineer 2026） |
| 09 | **Capstone**——构建 RTL 生成 + 自验证的 agent 循环，在 VerilogEval 风格任务上评测 |

</div>

> Lecture 02–09 是待建设的课程体系；Lecture 01（开篇）已写完。Capstone 复用 AI Agent Development 中的 **genie-claw** harness 模式，但重新定向：生成 RTL → 把模拟器/linter 作为工具运行 → 对照 testbench 检查 → 在护栏内迭代。

---

## What you ship

一个小的 **agentic RTL pipeline**：给定模块 spec，agent 生成 Verilog，把模拟器/linter 作为工具驱动，对照 testbench 自检，并迭代——附上一份实测报告，覆盖 pass@1、功能覆盖率和每个已验证模块的成本，并诚实说明它在哪些地方失败、需要人工介入。

---

## Current as of 2026-06

以 Chip-Design-LLM-Zoo 排行榜（VerilogEval / RTLLM / RealBench）以及 Computex / GTC Taipei 2026 的硬件背景（Vera Rubin 量产、Cosmos 3）为锚点。RTL-LLM 排名变化很快——请以实时 Zoo 和一手论文为准来刷新。当出现新的 RTL 生成 SOTA 或智能体化 EDA 系统实质性改变排行榜时，予以刷新。

---

## References

* **Chip-Design-LLM-Zoo**（实时 RTL 生成排行榜）— [iprc-dip.github.io/Chip-Design-LLM-Zoo](https://iprc-dip.github.io/Chip-Design-LLM-Zoo/)
* **ChipNeMo: Domain-Adapted LLMs for Chip Design**（NVIDIA）— [arXiv:2311.00176](https://arxiv.org/abs/2311.00176)
* **VerilogEval** — [arXiv:2309.07544](https://arxiv.org/abs/2309.07544) · **RTLLM** — [arXiv:2308.05345](https://arxiv.org/abs/2308.05345)
* NVIDIA Computex / GTC Taipei 2026（Vera Rubin 量产、Cosmos 3）— [ServeTheHome live coverage](https://www.servethehome.com/nvidia-computex-2026-keynote-live-coverage/) · [NVIDIA GTC Taipei](https://www.nvidia.com/en-tw/gtc/taipei/computex/)
* 衔接课程：[AI Agent Development 2026](../../../阶段3-人工智能/04-智能体AI与ML工程/01-智能体AI与生成式AI/Guide.md) · [AI Inference Engineer 2026](../../07-方向G-ML系统工程/01-AI推理工程师2026/README.md)


<details>
<summary>English original</summary>

**Module 3 · Beyond generation**

<div class="lecture-map" markdown>

| # | Lecture |
|---|---------|
| 05 | Verification & testbench generation with agents — the real bottleneck, coverage closure |
| 06 | Agentic EDA flows — multi-agent spec→RTL→verify→debug loops, tool-calling into EDA tools (the harness, applied) |
| 07 | PPA optimization and debugging loops with agents |

</div>

**Module 4 · Systems & practice**

<div class="lecture-map" markdown>

| # | Lecture |
|---|---------|
| 08 | Serving design agents — inference cost, Blackwell/Vera Rubin context (bridge to AI Inference Engineer 2026) |
| 09 | **Capstone** — build an RTL-generation + self-verification agent loop, evaluated on VerilogEval-style tasks |

</div>

> Lectures 02–09 are the curriculum to be built out; Lecture 01 (the opener) is written. The capstone reuses the **genie-claw** harness pattern from AI Agent Development, retargeted: generate RTL → run a simulator/linter as a tool → check against a testbench → iterate within guardrails.

---

**What you ship**

A small **agentic RTL pipeline**: an agent that, given a module spec, generates Verilog, drives a simulator/linter as tools, self-checks against a testbench, and iterates — with a measured report on pass@1, functional coverage, and cost per verified module, plus an honest account of where it failed and needed a human.

---

**Current as of 2026-06**

Anchored on the Chip-Design-LLM-Zoo leaderboard (VerilogEval / RTLLM / RealBench) and the Computex / GTC Taipei 2026 hardware backdrop (Vera Rubin in production, Cosmos 3). RTL-LLM rankings move fast — refresh against the live Zoo and primary papers. Refresh when a new RTL-generation SOTA or agentic-EDA system materially shifts the leaderboard.

---

**References**

* **Chip-Design-LLM-Zoo** (live RTL-generation leaderboard) — [iprc-dip.github.io/Chip-Design-LLM-Zoo](https://iprc-dip.github.io/Chip-Design-LLM-Zoo/)
* **ChipNeMo: Domain-Adapted LLMs for Chip Design** (NVIDIA) — [arXiv:2311.00176](https://arxiv.org/abs/2311.00176)
* **VerilogEval** — [arXiv:2309.07544](https://arxiv.org/abs/2309.07544) · **RTLLM** — [arXiv:2308.05345](https://arxiv.org/abs/2308.05345)
* NVIDIA Computex / GTC Taipei 2026 (Vera Rubin production, Cosmos 3) — [ServeTheHome live coverage](https://www.servethehome.com/nvidia-computex-2026-keynote-live-coverage/) · [NVIDIA GTC Taipei](https://www.nvidia.com/en-tw/gtc/taipei/computex/)
* Bridge courses: [AI Agent Development 2026](../../../阶段3-人工智能/04-智能体AI与ML工程/01-智能体AI与生成式AI/Guide.md) · [AI Inference Engineer 2026](../../07-方向G-ML系统工程/01-AI推理工程师2026/README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track F - AI Chip Design/Agentic Chip Design 2026/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20F%20-%20AI%20Chip%20Design/Agentic%20Chip%20Design%202026/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
