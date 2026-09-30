---
title: AI 智能体开发 2026
description: AI 智能体开发 2026
---

# AI 智能体开发 2026

<div class="course-identity auto-course" style="--course-accent: #0f766e; --course-accent-rgb: 15, 118, 110;" markdown="1">
<div class="course-identity__icon">AAD</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度解析 · AI 智能体</p>
<p class="course-identity__title">AI 智能体开发 2026 —— 从现代 agent harness（agent 运行时框架）到可运行的构建。</p>
<p class="course-identity__meta">产物：一个可运行的 agent（genie-claw） · 度量：可靠性、延迟、成本、安全</p>
</div>
</div>


**上级：** [阶段 3 —— 人工智能](../../Guide.md) · 方向 B

> *在大语言模型之上构建应用 —— agent、RAG、工具调用、GenAI 产品。*

**前置要求：** 模块 1（神经网络）、模块 2（框架 —— 理解 Transformer 与 PyTorch）。

**目标岗位：** 智能体化 AI 工程师 · GenAI 工程师 · AI 工程师

---

## 课程体系 —— 推荐学习顺序

一条**理论优先的路径**：从 2026 年现代 agent *是什么* 出发，学习基础，逐个构建核心部件，研究一个真实的 harness（**OpenClaw**），然后构建你自己的（**genie-claw**）。讲座*文件*保留其原始编号 —— 这是推荐的**阅读顺序**，而非重新编号。**[→ 扁平讲座索引](01-讲座/README.md)**

**时效性说明：** 模型名称、上下文窗口、SDK 特性和定价变化很快。本课程讲授的是*稳定*层 —— 模型 API、工具协议、运行循环、工作流图、网关、遥测和策略边界。在生产环境中复制模型 ID 或价格之前，务必查阅提供商文档。

### 模块 1 · 从这里开始 —— 现代 agent 是什么，以及如何构建一个

整门课的概念核心：2026 年变了什么、**harness**（模型周围的 runtime），以及*构建* agent 的两部分基础 —— 模型/工具/指令，然后是编排与护栏。

<div class="lecture-map" markdown>

| # | 讲座 |
|---|---------|
| [01](01-讲座/Lecture-01.md) | 2026 年的现代 AI agent —— 变了什么 *(从这里开始)* |
| [02](01-讲座/Lecture-02.md) | 什么是 AI agent harness？—— 模型周围的 runtime |
| [03](01-讲座/Lecture-03.md) | 构建 agent I —— 基础（模型、工具、指令） |
| [04](01-讲座/Lecture-04.md) | 构建 agent II —— 编排与护栏 |

</div>

### 模块 2 · 基础 —— 底层的模型

模型实际如何工作，以及如何与它对话。

<div class="lecture-map" markdown>

| # | 讲座 |
|---|---------|
| [05](01-讲座/Lecture-05.md) | 面向 agent 的大语言模型基础 |
| [06](01-讲座/Lecture-06.md) | 从零实现大语言模型 —— 模型机制 |
| [07](01-讲座/Lecture-07.md) | 提示工程与结构化输出 |

</div>

### 模块 3 · 核心构建块（逐个讲解）

按依赖顺序排列 agent 的每个部分：工具 → 记忆 → 检索 → 编排/框架 → 多模态 → 技能 → 评估。

<div class="lecture-map" markdown>

| # | 讲座 |
|---|---------|
| [08](01-讲座/Lecture-08.md) | 工具调用与函数调用 |
| [09](01-讲座/Lecture-09.md) | 结构化工具优于 computer use |
| [10](01-讲座/Lecture-10.md) | 记忆系统 |
| [11](01-讲座/Lecture-11.md) | RAG —— 数据摄取与 embedding |
| [12](01-讲座/Lecture-12.md) | RAG —— 检索与重排序 |
| [13](01-讲座/Lecture-13.md) | 向量库与 embedding 模型选择 |
| [14](01-讲座/Lecture-14.md) | 高效的本地 RAG 栈 |
| [15](01-讲座/Lecture-15.md) | agent 架构模式（ReAct、plan-execute、reflexion） |
| [16](01-讲座/Lecture-16.md) | LangGraph —— 有状态工作流 |
| [17](01-讲座/Lecture-17.md) | agent SDK 与 runtime API |
| [18](01-讲座/Lecture-18.md) | OpenAI Agents SDK |
| [19](01-讲座/Lecture-19.md) | 多 agent 系统 |
| [20](01-讲座/Lecture-20.md) | 多模态子 agent |
| [21](01-讲座/Lecture-21.md) | agent 技能 —— 工作流纪律 |
| [22](01-讲座/Lecture-22.md) | agent 技能评测 |
| [23](01-讲座/Lecture-23.md) | 评估与可观测性 |

</div>

### 模块 4 · 生产纪律与 runtime

让 agent 可靠、安全、可交付：安全、持久化状态、确定性启动、runtime 选型、生命周期和部署。

<div class="lecture-map" markdown>

| # | 讲座 |
|---|---------|
| [24](01-讲座/Lecture-24.md) | runtime 纪律与 AI runtime 安全 |
| [25](01-讲座/Lecture-25.md) | AI agent 安全工程师 |
| [26](01-讲座/Lecture-26.md) | 会话作为事实来源 —— 事件溯源的 agent 状态 |
| [27](01-讲座/Lecture-27.md) | 确定性启动 |
| [28](01-讲座/Lecture-28.md) | runtime 策略 —— Node、Bun、Rust 与边缘打包 |
| [29](01-讲座/Lecture-29.md) | 智能体化 SDLC |
| [30](01-讲座/Lecture-30.md) | 生产部署 |

</div>


<details>
<summary>English original</summary>

**AI Agent Development 2026**

<div class="course-identity auto-course" style="--course-accent: #0f766e; --course-accent-rgb: 15, 118, 110;" markdown="1">
<div class="course-identity__icon">AAD</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · AI Agents</p>
<p class="course-identity__title">AI Agent Development 2026 — from the modern agent harness to a working build.</p>
<p class="course-identity__meta">Artifact: a working agent (genie-claw) · Measure: reliability, latency, cost, safety</p>
</div>
</div>


**Parent:** [Phase 3 — Artificial Intelligence](../../Guide.md) · Track B

> *Build applications on top of large language models — agents, RAG, tool use, GenAI products.*

**Prerequisites:** Module 1 (Neural Networks), Module 2 (Frameworks — understand transformers and PyTorch).

**Role targets:** Agentic AI Engineer · GenAI Engineer · AI Engineer

---

**Curriculum — recommended learning order**

A **theory-first path**: start from what a modern agent *is* in 2026, learn the fundamentals, build the core parts one by one, study a real harness (**OpenClaw**), then build your own (**genie-claw**). Lecture *files* keep their original numbers — this is the recommended **reading order**, not a renumbering. **[→ Flat lecture index](01-讲座/README.md)**

**Currency note:** model names, context windows, SDK features, and pricing change fast. This course teaches the *stable* layer — model APIs, tool protocols, run loops, workflow graphs, gateways, telemetry, and policy boundaries. Always check provider docs before copying model IDs or prices into production.

**Module 1 · Start here — what a modern agent is, and how to build one**

The conceptual core of the whole course: what changed in 2026, the **harness** (the runtime around the model), and the two-part foundations of *building* an agent — model/tools/instructions, then orchestration and guardrails.

<div class="lecture-map" markdown>

| # | Lecture |
|---|---------|
| [01](01-讲座/Lecture-01.md) | The modern AI agent in 2026 — what changed *(start here)* |
| [02](01-讲座/Lecture-02.md) | What is an AI agent harness? — the runtime around the model |
| [03](01-讲座/Lecture-03.md) | Building agents I — foundations (model, tools, instructions) |
| [04](01-讲座/Lecture-04.md) | Building agents II — orchestration & guardrails |

</div>

**Module 2 · Fundamentals — the model underneath**

How the model actually works, and how you talk to it.

<div class="lecture-map" markdown>

| # | Lecture |
|---|---------|
| [05](01-讲座/Lecture-05.md) | LLM fundamentals for agents |
| [06](01-讲座/Lecture-06.md) | LLM from scratch — model mechanics |
| [07](01-讲座/Lecture-07.md) | Prompt engineering & structured output |

</div>

**Module 3 · Core building blocks (one by one)**

Each part of an agent in dependency order: tools → memory → retrieval → orchestration/frameworks → multimodal → skills → evaluation.

<div class="lecture-map" markdown>

| # | Lecture |
|---|---------|
| [08](01-讲座/Lecture-08.md) | Tool use & function calling |
| [09](01-讲座/Lecture-09.md) | Structured tools beat computer use |
| [10](01-讲座/Lecture-10.md) | Memory systems |
| [11](01-讲座/Lecture-11.md) | RAG — ingestion & embeddings |
| [12](01-讲座/Lecture-12.md) | RAG — retrieval & reranking |
| [13](01-讲座/Lecture-13.md) | Vector stores & embedding model selection |
| [14](01-讲座/Lecture-14.md) | Efficient local RAG stack |
| [15](01-讲座/Lecture-15.md) | Agent architecture patterns (ReAct, plan-execute, reflexion) |
| [16](01-讲座/Lecture-16.md) | LangGraph — stateful workflows |
| [17](01-讲座/Lecture-17.md) | Agent SDKs & runtime APIs |
| [18](01-讲座/Lecture-18.md) | OpenAI Agents SDK |
| [19](01-讲座/Lecture-19.md) | Multi-agent systems |
| [20](01-讲座/Lecture-20.md) | Multimodal sub-agents |
| [21](01-讲座/Lecture-21.md) | Agent skills — workflow discipline |
| [22](01-讲座/Lecture-22.md) | Agent skills eval |
| [23](01-讲座/Lecture-23.md) | Evaluation & observability |

</div>

**Module 4 · Production discipline & runtime**

Making an agent reliable, safe, and shippable: security, durable state, deterministic startup, runtime choice, lifecycle, and deployment.

<div class="lecture-map" markdown>

| # | Lecture |
|---|---------|
| [24](01-讲座/Lecture-24.md) | Runtime discipline & AI runtime security |
| [25](01-讲座/Lecture-25.md) | AI agent security engineer |
| [26](01-讲座/Lecture-26.md) | Session as source of truth — event-sourced agent state |
| [27](01-讲座/Lecture-27.md) | Deterministic startup |
| [28](01-讲座/Lecture-28.md) | Runtime strategy — Node, Bun, Rust, and edge packaging |
| [29](01-讲座/Lecture-29.md) | Agentic SDLC |
| [30](01-讲座/Lecture-30.md) | Production deployment |

</div>

</details>

### 模块 5 · 示例 — 真实 harness 的解剖（OpenClaw）

一个生产风格、本地优先的助手，逐件拆解 —— 模块 1–4 的概念在一个系统中落地。

<div class="lecture-map" markdown>

| # | 讲座 |
|---|---------|
| [31](01-讲座/Lecture-31.md) | 网关架构 |
| [32](01-讲座/Lecture-32.md) | 布线与会话 |
| [33](01-讲座/Lecture-33.md) | 多 agent 隔离 |
| [34](01-讲座/Lecture-34.md) | 运维与安全 |
| [35](01-讲座/Lecture-35.md) | agent 循环 |
| [36](01-讲座/Lecture-36.md) | Cron 与定时 agent 运行 |
| [37](01-讲座/Lecture-37.md) | 系统提示词架构 |
| [38](01-讲座/Lecture-38.md) | App SDK 与类型化 RPC |
| [39](01-讲座/Lecture-39.md) | 网关 RPC 协议 |
| [40](01-讲座/Lecture-40.md) | OpenClaw 威胁模型（面向 agent 安全的 MITRE ATLAS） |
| [41](01-讲座/Lecture-41.md) | Pi — OpenClaw 之下的最小 agent |

</div>

### 模块 6 · 实践 — 构建 **genie-claw**

顶点项目：应用模块 1–5 来构建 **genie-claw** —— 你自己的最小 agent harness，把本地 LLM runtime（GeniePod [`genie-ai-runtime`](https://github.com/GeniePod/genie-ai-runtime)）与 OpenClaw 风格的网关配对：会话、工具、护栏，以及一个你端到端掌控的运行循环。下面的实验是垫脚石。

<div class="lecture-map" markdown>

| 实验 | 构建内容 |
|-----|-------|
| [Lab 01](01-讲座/Lab-01-Research-Agent.md) | 带工具使用的研究 agent |
| [Lab 02](01-讲座/Lab-02-Multi-Agent-Pipeline.md) | 多 agent 代码评审流水线 |
| [Lab 03](01-讲座/Lab-03-Production-RAG.md) | 生产级 RAG 系统 |
| [Lab 04](01-讲座/Lab-04-TokenJuice-Output-Compaction.md) | TokenJuice 输出压缩 |
| [Lab 05](01-讲座/Lab-05-OpenMeow-App-SDK-Dogfood.md) | OpenMeow App SDK 自用验证（macOS） |
| [**Lab 06 · 顶点项目**](01-讲座/Lab-06-Genie-Claw-Capstone.md) | **genie-claw** —— 端到端构建你自己的最小 agent harness |

</div>

### 相关 —— MLSys 深度专题（已迁移）

偏推理与 kernel 的深度专题（GPU kernel、FP8 KV-cache、性能链路追踪、序列并行、小 MoE 推理）曾作为附录放在这里。它们属于 **ML Systems Engineering**，因此现在位于 **[MLSys Deep Dives](../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/README.md)** 合集（阶段 5）中，与 [AI Inference Engineer 2026](../../../阶段5-高级专题与专精/07-方向G-ML系统工程/01-AI推理工程师2026/README.md) 课程并列。

---

## 这对 AI 硬件为何重要

智能体化 AI 创造了驱动芯片设计的 **推理需求**：
- 长上下文 attention（128K+ token）→ L5：HBM 带宽、存储层次
- 多轮工具调用 → L3：低延迟 kernel 启动、流调度
- 批推理服务 → L2：TensorRT-LLM、in-flight 批处理优化
- RAG 向量检索 → L1：GPU 上的 cuVS/FAISS 加速

理解这些工作负载有助于 L2（编译器）与 L5（架构）工程师针对真实使用模式做设计。

---

## 当前 AI 应用趋势（2026）

最强的应用趋势是：AI 正从 **聊天界面** 转向 **持久、会使用工具的系统**，在真实工作流中采取行动。

两个有用的参考模式是：
- **智能体化编程系统**，例如 [Claude Code](https://code.claude.com/docs/en/overview)
- **本地优先个人助手系统**，例如 [OpenClaw](https://github.com/openclaw/openclaw)

它们值得研究，因为它们展示了现代 AI 应用在生产式使用中的真实样貌，而不只是 demo 里。

### 1. 编程 agent 正在取代单步“代码补全”

这是最清晰的转变。

像 Claude Code 这样的工具已不再局限于编辑器中的自动补全。它们更像软件工人：
- 阅读并浏览代码仓库
- 规划跨多个文件的改动
- 运行测试与验证循环
- 创建提交与 pull request
- 通过 MCP 连接外部工具
- 加载可复用的技能、hooks 与插件

Anthropic 的官方文档直接描述了这一点：Claude Code 可以自动化例行工程工作、与 git 协作、通过 MCP 连接工具、派生多个 agent，并在 CI/CD 工作流中运行。其公开仓库与插件系统使它成为很好的参考，说明编程 agent 正在成为一个真实的应用门类，而非新奇的玩具。

**这对硬件为何重要：**
- 编程 agent 产生的是长时间运行、工具密集的推理会话，而不是简短的聊天轮次
- 它们提升了对低延迟迭代循环、更大上下文窗口与更高后台推理量的需求
- 它们把 AI 产品推向开发者基础设施，在那里可靠性、权限管理与自动化与模型质量同等重要


<details>
<summary>English original</summary>

**Module 5 · Example — anatomy of a real harness (OpenClaw)**

A production-style, local-first assistant taken apart piece by piece — the concepts of Modules 1–4 made concrete in one system.

<div class="lecture-map" markdown>

| # | Lecture |
|---|---------|
| [31](01-讲座/Lecture-31.md) | Gateway architecture |
| [32](01-讲座/Lecture-32.md) | Routing & sessions |
| [33](01-讲座/Lecture-33.md) | Multi-agent isolation |
| [34](01-讲座/Lecture-34.md) | Operations & security |
| [35](01-讲座/Lecture-35.md) | The agent loop |
| [36](01-讲座/Lecture-36.md) | Cron & scheduled agent runs |
| [37](01-讲座/Lecture-37.md) | System prompt architecture |
| [38](01-讲座/Lecture-38.md) | App SDK & typed RPCs |
| [39](01-讲座/Lecture-39.md) | Gateway RPC protocol |
| [40](01-讲座/Lecture-40.md) | OpenClaw threat model (MITRE ATLAS for agent security) |
| [41](01-讲座/Lecture-41.md) | Pi — the minimal agent beneath OpenClaw |

</div>

**Module 6 · Practice — build **genie-claw****

The capstone: apply Modules 1–5 by building **genie-claw** — your own minimal agent harness that pairs a local LLM runtime (the GeniePod [`genie-ai-runtime`](https://github.com/GeniePod/genie-ai-runtime)) with an OpenClaw-style gateway: sessions, tools, guardrails, and a run loop you control end-to-end. The labs below are the stepping stones.

<div class="lecture-map" markdown>

| Lab | Build |
|-----|-------|
| [Lab 01](01-讲座/Lab-01-Research-Agent.md) | Research agent with tool use |
| [Lab 02](01-讲座/Lab-02-Multi-Agent-Pipeline.md) | Multi-agent code-review pipeline |
| [Lab 03](01-讲座/Lab-03-Production-RAG.md) | Production RAG system |
| [Lab 04](01-讲座/Lab-04-TokenJuice-Output-Compaction.md) | TokenJuice output compaction |
| [Lab 05](01-讲座/Lab-05-OpenMeow-App-SDK-Dogfood.md) | OpenMeow App SDK dogfood (macOS) |
| [**Lab 06 · Capstone**](01-讲座/Lab-06-Genie-Claw-Capstone.md) | **genie-claw** — build your own minimal agent harness end-to-end |

</div>

**Related — MLSys deep dives (relocated)**

The inference- and kernel-leaning deep dives (GPU kernels, FP8 KV-cache, performance tracing, sequence parallelism, small-MoE reasoning) used to live here as an appendix. They belong with **ML Systems Engineering**, so they now live in the **[MLSys Deep Dives](../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/README.md)** collection (Phase 5), alongside the [AI Inference Engineer 2026](../../../阶段5-高级专题与专精/07-方向G-ML系统工程/01-AI推理工程师2026/README.md) course.

---

**Why This Matters for AI Hardware**

Agentic AI creates the **inference demand** that drives chip design:
- Long-context attention (128K+ tokens) → L5: HBM bandwidth, memory hierarchy
- Multi-turn tool calling → L3: low-latency kernel launch, stream scheduling
- Batch inference serving → L2: TensorRT-LLM, in-flight batching optimization
- RAG vector search → L1: cuVS/FAISS acceleration on GPU

Understanding these workloads helps L2 (compiler) and L5 (architecture) engineers design for real usage patterns.

---

**Current AI Application Trends (2026)**

The strongest application trend is that AI is moving from **chat interfaces** to **persistent, tool-using systems** that act inside real workflows.

Two useful reference patterns are:
- **agentic coding systems** such as [Claude Code](https://code.claude.com/docs/en/overview)
- **local-first personal assistant systems** such as [OpenClaw](https://github.com/openclaw/openclaw)

These are worth studying because they show what modern AI applications actually look like in production-like usage, not just in demos.

**1. Coding Agents Are Replacing Single-Step “Code Completion”**

This is the clearest shift.

Tools like Claude Code are no longer limited to autocomplete in an editor. They act more like software workers:
- read and navigate a repository
- plan changes across multiple files
- run tests and verification loops
- create commits and pull requests
- connect to external tools through MCP
- load reusable skills, hooks, and plugins

Anthropic’s official docs describe this directly: Claude Code can automate routine engineering work, work with git, connect tools through MCP, spawn multiple agents, and run in CI/CD workflows. Its public repository and plugin system make it a good reference for how coding agents are becoming a real application category rather than a novelty.

**Why this matters for hardware:**
- coding agents create long-running, tool-rich inference sessions instead of short chat turns
- they increase demand for low-latency iteration loops, larger context windows, and higher background inference volume
- they push AI products into developer infrastructure, where reliability, permissioning, and automation matter as much as model quality

</details>

### 2. 个人 AI 正走向本地优先的控制平面

OpenClaw 是另一股趋势的有用参照：AI 助手不再是「一个网页配一个聊天框」，而是一个持续运行、连接众多界面的**控制平面**。

根据 OpenClaw 官方仓库与文档：
- 一个长期驻留的本地网关掌管渠道、会话、工具与事件
- 该助手可跨 WhatsApp、Telegram、Slack、Discord、WebChat 及其他渠道运行
- 支持语音、移动节点、实时 canvas UI 与多 agent 路由
- 它将入站消息视为不可信输入，并给出了具体的安全模型文档

这揭示了应用设计的走向：
- 持久驻留的助手，而非一次性的 prompt
- 多渠道交付，而非单一前端
- 本地或由运营者控制的基础设施，而非仅有云端托管的聊天
- agent 作为可路由的服务，拥有隔离的工作区与记忆

**这对硬件为何重要：**
- 常驻在线的助手带来持续稳定的推理需求，而非只是突发负载
- 多模态助手加大了对设备内存、流式处理与本地推理路径的压力
- 本地优先的设计让边缘硬件、Jetson 级设备、移动节点以及云/边混合部署更具现实意义

### 3. MCP、插件与 hooks 正成为真正的应用界面

另一个重大趋势是：仅有模型已不再是完整的产品。

现代 AI 系统越来越由以下要素定义：
- 连接外部系统的 **MCP 连接器**
- 用于可复用工作流的**插件**
- 围绕模型动作的 **hooks** 与自动化
- 面向领域特定行为的**技能**与自定义 agent

Claude Code 的文档明确将 MCP、插件、技能、hooks、监控器与自定义 agent 定位为一等扩展界面。OpenClaw 同样把工具、插件、渠道、节点与网关协议视为产品架构的一部分。

其含义很重要：应用层正在成为一个**工具与协议生态**，而不只是 prompt 模板。

### 4. Agent SDK 正在成为 runtime 层，而不只是 API 封装

现代 agent SDK 已位于原始模型调用之上。它们越来越多地管理：
- agent 循环
- 工具分发
- 专家之间的交接
- 会话与状态
- 护栏与人工审核
- 链路追踪与评估
- MCP server 集成

这一点很重要，因为生产级 agent 系统需要的远不止一次 `messages.create()` 调用。它们需要一份可复现的 runtime 契约：有哪些工具可用、由哪个身份执行、哪些动作需要审批、状态存放在哪里、以及记录什么日志。

本课程给出的实用设计准则是：

```text
Provider API details belong in adapters.
Product behavior belongs in your runtime contract.
Security decisions belong outside the LLM.
```

正因如此，Lecture 17 将 SDK 与 runtime API 作为通用层来讲授，而不是把某一家厂商的 SDK 当作架构。

### 5. 多 agent 结构正变得实用，而非停留在理论

业界已超越「一个模型、一个 prompt、一个答案」。

当前系统越来越多地采用：
- 一个主 agent 加若干 worker agent
- 按任务或用户隔离的工作区
- 显式路由规则
- 后台监控器与事件驱动触发器

Claude Code 暴露多 agent 工作流与自定义 agent。OpenClaw 暴露多 agent 路由，带隔离的工作区、会话存储，以及从入站渠道到特定 agent 的绑定。

这是一个务实趋势，因为它能很好地对应真实产品：
- 支持类工作流
- 开发者工作流
- 个人助手工作流
- 项目自动化与治理工作流

### 6. 安全与权限边界如今是核心产品特性

这是相较早期 LLM 应用最大的变化之一。

当前 AI 应用越来越多地自带：
- 配对与允许列表
- 工具权限边界
- 网关认证与签名连接
- 沙箱
- 安全输出策略
- 内容审核与 prompt 注入防御

OpenClaw 的文档强调配对审批、DM 安全、沙箱模式与网关认证。Claude Code 的插件系统与工具模型强调显式结构、限定作用域的扩展与运行安全。GitHub 的智能体化工作流材料同样将安全、权限与隔离执行置于核心位置，而非可选项。

这意味着「应用架构」如今包含信任边界，而不只是 prompt 与 UI。


<details>
<summary>English original</summary>

**2. Personal AI Is Moving Toward Local-First Control Planes**

OpenClaw is a useful reference for a different trend: AI assistants that are not “one web page with one chat box,” but a **control plane** that stays running and connects many surfaces.

From the official OpenClaw repo and docs:
- one long-lived local Gateway owns channels, sessions, tools, and events
- the assistant can operate across WhatsApp, Telegram, Slack, Discord, WebChat, and other channels
- it supports voice, mobile nodes, live canvas UI, and multi-agent routing
- it treats inbound messages as untrusted input and documents a concrete security model

This shows where application design is going:
- persistent assistants instead of one-off prompts
- multi-channel delivery instead of one frontend
- local or operator-controlled infrastructure instead of only cloud-hosted chat
- agents as routed services with isolated workspaces and memory

**Why this matters for hardware:**
- always-on assistants create steady inference demand, not just bursty usage
- multimodal assistants increase pressure on device memory, streaming, and local inference paths
- local-first designs make edge hardware, Jetson-class devices, mobile nodes, and hybrid cloud/edge deployment more relevant

**3. MCP, Plugins, and Hooks Are Becoming the Real Application Surface**

Another major trend is that the model alone is no longer the full product.

Modern AI systems are increasingly defined by:
- **MCP connectors** to external systems
- **plugins** for reusable workflows
- **hooks** and automations around model actions
- **skills** and custom agents for domain-specific behavior

Claude Code’s docs explicitly position MCP, plugins, skills, hooks, monitors, and custom agents as first-class extension surfaces. OpenClaw similarly treats tools, plugins, channels, nodes, and gateway protocols as part of the product architecture.

The implication is important: the application layer is becoming a **tool-and-protocol ecosystem**, not just a prompt template.

**4. Agent SDKs Are Becoming Runtime Layers, Not Just API Wrappers**

Modern agent SDKs now sit above raw model calls. They increasingly manage:
- agent loops
- tool dispatch
- handoffs between specialists
- sessions and state
- guardrails and human review
- tracing and evaluation
- MCP server integration

This matters because production agent systems need more than a `messages.create()` call. They need a repeatable runtime contract: what tools are available, which identity executes them, which actions require approval, where state is stored, and what gets logged.

The practical design rule for this course is:

```text
Provider API details belong in adapters.
Product behavior belongs in your runtime contract.
Security decisions belong outside the LLM.
```

This is why Lecture 17 teaches SDKs and runtime APIs as a general layer instead of treating one vendor SDK as the architecture.

**5. Multi-Agent Structure Is Becoming Practical, Not Theoretical**

The industry has moved beyond “one model, one prompt, one answer.”

Current systems increasingly use:
- a lead agent plus worker agents
- isolated workspaces per task or user
- explicit routing rules
- background monitors and event-driven triggers

Claude Code exposes multiple-agent workflows and custom agents. OpenClaw exposes multi-agent routing with isolated workspaces, session stores, and bindings from inbound channels to specific agents.

This is a practical trend because it maps well to real products:
- support workflows
- developer workflows
- personal assistant workflows
- project automation and governance workflows

**6. Security and Permission Boundaries Are Now Core Product Features**

This is one of the biggest changes from early LLM apps.

Current AI applications increasingly ship with:
- pairing and allowlists
- tool permission boundaries
- gateway auth and signed connections
- sandboxing
- safe output policies
- moderation and prompt-injection defenses

OpenClaw’s docs emphasize pairing approval, DM safety, sandbox modes, and gateway auth. Claude Code’s plugin system and tool model emphasize explicit structure, scoped extensions, and operational safety. GitHub’s agentic workflow material also frames security, permissions, and isolated execution as central rather than optional.

This means “application architecture” now includes trust boundaries, not just prompts and UI.

</details>

### 7. 从这些示例中应学到什么

不要只是因为 OpenClaw 和 Claude Code 流行就去研究它们。研究它们，是因为它们代表了两种高信号的应用模式：

- **Claude Code 模式：** AI 嵌入开发者工作流、代码仓库、CI、工具与评审闭环
- **OpenClaw 模式：** AI 嵌入消息、语音、移动节点、控制平面与个人自动化

它们共同表明，当前应用前沿是：
- 智能体化的
- 工具连接的
- 持久化的
- 权限化的
- 多界面的
- 运维可观测的

对本路线图而言，重要要点是，**方向 B 应教授真实 AI 产品正在趋同的工作负载与架构**，因为这些产品正是下游系统与硬件最终要服务的对象。

---

## 1. 面向工程师的大语言模型基础

* **Transformer 架构：** attention 机制、KV 缓存、位置编码
* **分词：** BPE、SentencePiece、词汇表大小对 embedding layer 的影响
* **推理机制：** prefill（首字前的整段计算，算力受限） vs decode（逐 token 生成阶段，带宽受限），自回归生成
* **Scaling laws：** 参数量 vs 数据集大小 vs 算力预算

---

## 2. 智能体化 AI

* **agent 是什么：** 大语言模型 + 工具 + 记忆 + 规划循环
* **agent runtime 与框架：** 原始提供商 API、OpenAI Agents SDK、LangGraph、MCP 服务器、OpenClaw 风格网关、用于实验的 CrewAI/AutoGen
* **工具使用：** 函数调用、API 集成、代码执行
* **安全边界：** 提示词注入、工具滥用、最小权限凭证、对高风险操作进行人工批准
* **记忆：** 对话历史、向量存储检索、工作记忆
* **规划：** 思维链、ReAct、思维树、自我反思
* **多 agent 系统：** 任务分解、agent 协作、编排

**项目：**
1. 构建一个使用工具（网页搜索、计算器、代码执行）来回答复杂问题的 agent。
2. 构建一个多步研究 agent：给定一个主题，搜索 → 综合 → 撰写报告。

---

## 3. RAG（检索增强生成）

* **架构：** 文档摄取 → 分块 → embedding → 向量存储 → 检索 → 生成
* **Embedding 模型：** sentence-transformers、OpenAI embeddings、Cohere
* **向量存储：** FAISS、Chroma、Pinecone、Weaviate、Milvus
* **分块策略：** 固定大小、递归、语义、文档结构感知
* **检索：** 相似度搜索、混合（稠密 + 稀疏）、重排序
* **RAG 安全：** 将文档视为不可信输入，筛查上传内容和检索到的分块，防御间接提示词注入
* **评估：** 忠实度、相关性、幻觉检测

**项目：**
1. 在技术文档语料上构建 RAG 流水线。评估检索质量。
2. 比较 FAISS（CPU） vs FAISS（GPU） vs cuVS 在 1M 文档下进行向量搜索的延迟。

---

## 4. GenAI 产品开发

* **提示词工程：** 系统提示词、few-shot、思维链、输出格式化
* **微调：** LoRA、QLoRA、在领域数据上的全量微调
* **评估：** 自动指标（ROUGE、BLEU）、大语言模型作为裁判、人工评估
* **护栏：** 输入审核、输出过滤、内容安全、提示词注入防御、幻觉缓解
* **生产部署：** API 设计、流式传输、速率限制、成本管理
* **runtime 纪律：** 实时遥测、工具调用策略门禁、最小权限的 agent 身份、审计跟踪与 runtime 事件响应
* **确定性的启动：** 启动契约、就绪检查、提示词/工具/策略版本化、记忆水合、可复现的 agent 启动
* **agent 控制平面：** 网关、会话、布线、多界面交付
* **定时 agent 执行：** cron 作业、隔离的运行会话、交付回退、故障布线、重试、运行日志与保留策略
* **OpenClaw 风格 agent 开发：** 多 agent 隔离、工作区设计、配对，以及面向持久化本地优先助手的运维

**项目：**
1. 在领域特定数据集上用 QLoRA 微调一个 7B 模型。衡量相比基座模型的提升。
2. 部署一个具备流式传输、安全护栏和成本跟踪的 GenAI 应用。
3. 构建一个安全的 agent 或 RAG 流水线，具备提示词攻击检测、工具约束和输出验证。


<details>
<summary>English original</summary>

**7. What To Learn From These Examples**

Do not study OpenClaw and Claude Code just because they are popular. Study them because they represent two high-signal application patterns:

- **Claude Code pattern:** AI embedded into developer workflows, repositories, CI, tools, and review loops
- **OpenClaw pattern:** AI embedded into messaging, voice, mobile nodes, control planes, and personal automation

Together they show that the current application frontier is:
- agentic
- tool-connected
- persistent
- permissioned
- multi-surface
- operationally observable

For this roadmap, the important takeaway is that **Track B should teach the workloads and architectures that real AI products are converging toward**, because those products are what downstream systems and hardware will ultimately serve.

---

**1. LLM Fundamentals for Engineers**

* **Transformer architecture:** attention mechanism, KV-cache, positional encoding
* **Tokenization:** BPE, SentencePiece, vocabulary size impact on embedding layer
* **Inference mechanics:** prefill (compute-bound) vs decode (memory-bound), autoregressive generation
* **Scaling laws:** parameter count vs dataset size vs compute budget

---

**2. Agentic AI**

* **What agents are:** LLM + tools + memory + planning loop
* **Agent runtimes and frameworks:** raw provider APIs, OpenAI Agents SDK, LangGraph, MCP servers, OpenClaw-style gateways, CrewAI/AutoGen for experiments
* **Tool use:** function calling, API integration, code execution
* **Security boundaries:** prompt injection, tool abuse, least-privilege credentials, human approval for risky actions
* **Memory:** conversation history, vector store retrieval, working memory
* **Planning:** chain-of-thought, ReAct, tree-of-thought, self-reflection
* **Multi-agent systems:** task decomposition, agent collaboration, orchestration

**Projects:**
1. Build an agent that uses tools (web search, calculator, code execution) to answer complex questions.
2. Build a multi-step research agent: given a topic, search → synthesize → write a report.

---

**3. RAG (Retrieval-Augmented Generation)**

* **Architecture:** document ingestion → chunking → embedding → vector store → retrieval → generation
* **Embedding models:** sentence-transformers, OpenAI embeddings, Cohere
* **Vector stores:** FAISS, Chroma, Pinecone, Weaviate, Milvus
* **Chunking strategies:** fixed-size, recursive, semantic, document-structure-aware
* **Retrieval:** similarity search, hybrid (dense + sparse), re-ranking
* **RAG security:** treat documents as untrusted input, screen uploads and retrieved chunks, defend against indirect prompt injection
* **Evaluation:** faithfulness, relevance, hallucination detection

**Projects:**
1. Build a RAG pipeline over a technical documentation corpus. Evaluate retrieval quality.
2. Compare FAISS (CPU) vs FAISS (GPU) vs cuVS for vector search latency at 1M documents.

---

**4. GenAI Product Development**

* **Prompt engineering:** system prompts, few-shot, chain-of-thought, output formatting
* **Fine-tuning:** LoRA, QLoRA, full fine-tuning on domain data
* **Evaluation:** automated metrics (ROUGE, BLEU), LLM-as-judge, human evaluation
* **Guardrails:** input moderation, output filtering, content safety, prompt-injection defense, hallucination mitigation
* **Production deployment:** API design, streaming, rate limiting, cost management
* **Runtime discipline:** live telemetry, tool-call policy gates, least-privilege agent identities, audit trails, and runtime incident response
* **Deterministic startup:** startup contracts, readiness checks, prompt/tool/policy versioning, memory hydration, and reproducible agent boot
* **Agent control planes:** gateways, sessions, routing, and multi-surface delivery
* **Scheduled agent execution:** cron jobs, isolated run sessions, delivery fallback, failure routing, retries, run logs, and retention
* **OpenClaw-style agent development:** multi-agent isolation, workspace design, pairing, and operations for persistent local-first assistants

**Projects:**
1. Fine-tune a 7B model with QLoRA on a domain-specific dataset. Measure improvement vs base model.
2. Deploy a GenAI application with streaming, safety guardrails, and cost tracking.
3. Build a secure agent or RAG pipeline with prompt-attack detection, tool constraints, and output validation.

---

</details>

## 资源

| 资源 | 涵盖内容 |
|----------|---------------|
| [LangChain 文档](https://python.langchain.com/) | agent 与 RAG（检索增强生成）框架 |
| [LangGraph 文档](https://docs.langchain.com/oss/python/langgraph/overview) | 持久化、有状态的 agent 工作流，human-in-the-loop、记忆与链路追踪 |
| [OpenAI Agents SDK](https://openai.github.io/openai-agents-python/) | agent 循环、工具、handoff、护栏、会话、链路追踪与 MCP 集成 |
| [OpenAI API Agents 指南](https://platform.openai.com/docs/guides/agents) | OpenAI 当前针对代码优先的 agent 应用、工具、编排与可观测性的指南 |
| [Model Context Protocol 规范](https://modelcontextprotocol.io/specification/2025-11-25) | 用于工具、资源、prompt、host、client、server 与安全考量的标准协议 |
| [Claude Code 概览](https://code.claude.com/docs/en/overview) | 智能体化编码工作流、MCP、多 agent 使用与 CI 模式 |
| [Claude Code 插件](https://code.claude.com/docs/en/plugins) | 技能、agent、hook、MCP server、插件结构与分发 |
| [Claude Code 仓库](https://github.com/anthropics/claude-code) | 公开实现面、示例、插件与项目结构 |
| [Anthropic Cookbook](https://github.com/anthropics/anthropic-cookbook) | Claude API 实用示例 |
| [OpenClaw 仓库](https://github.com/openclaw/openclaw) | local-first 助手架构、channel、网关模型与安全默认值 |
| [OpenClaw 网关架构](https://docs.openclaw.ai/concepts/architecture) | 长驻网关、WS 协议、node、配对与远程访问模型 |
| [OpenClaw 特性](https://docs.openclaw.ai/concepts/features) | 多 agent 路由、媒体、channel、工具、应用与 provider 支持 |
| [GitHub 智能体化工作流](https://github.github.com/gh-aw/slides/github-agentic-workflows.pdf) | GitHub 官方对智能体化 CI/CD、权限与安全输出的表述 |
| [OWASP Top 10 for LLM Applications](https://owasp.org/www-project-top-10-for-large-language-model-applications/) | prompt 注入、不安全的输出处理、插件/工具风险、过度自主性与 LLM 应用安全 |
| [NIST AI RMF Generative AI Profile](https://www.nist.gov/publications/artificial-intelligence-risk-management-framework-generative-artificial-intelligence) | 面向生成式 AI 系统的治理与风险管理框架 |
| [RAG 最佳实践](https://docs.llamaindex.ai/) | LlamaIndex 文档 |
| *Build a Large Language Model (From Scratch)*（Raschka） | LLM 内部原理 |

---

## 下一步

→ [**模块 4B — ML 工程与 MLOps**](../02-ML工程与MLOps/Guide.md)


<details>
<summary>English original</summary>

**Resources**

| Resource | What it covers |
|----------|---------------|
| [LangChain Documentation](https://python.langchain.com/) | Agent and RAG framework |
| [LangGraph Documentation](https://docs.langchain.com/oss/python/langgraph/overview) | Durable, stateful agent workflows, human-in-the-loop, memory, and tracing |
| [OpenAI Agents SDK](https://openai.github.io/openai-agents-python/) | Agent loops, tools, handoffs, guardrails, sessions, tracing, and MCP integration |
| [OpenAI API Agents Guide](https://platform.openai.com/docs/guides/agents) | Current OpenAI guidance for code-first agent apps, tools, orchestration, and observability |
| [Model Context Protocol Specification](https://modelcontextprotocol.io/specification/2025-11-25) | Standard protocol for tools, resources, prompts, hosts, clients, servers, and safety considerations |
| [Claude Code Overview](https://code.claude.com/docs/en/overview) | Agentic coding workflows, MCP, multi-agent use, and CI patterns |
| [Claude Code Plugins](https://code.claude.com/docs/en/plugins) | Skills, agents, hooks, MCP servers, plugin structure, and distribution |
| [Claude Code Repository](https://github.com/anthropics/claude-code) | Public implementation surface, examples, plugins, and project layout |
| [Anthropic Cookbook](https://github.com/anthropics/anthropic-cookbook) | Practical Claude API examples |
| [OpenClaw Repository](https://github.com/openclaw/openclaw) | Local-first assistant architecture, channels, gateway model, and security defaults |
| [OpenClaw Gateway Architecture](https://docs.openclaw.ai/concepts/architecture) | Long-lived gateway, WS protocol, nodes, pairing, and remote access model |
| [OpenClaw Features](https://docs.openclaw.ai/concepts/features) | Multi-agent routing, media, channels, tools, apps, and provider support |
| [GitHub Agentic Workflows](https://github.github.com/gh-aw/slides/github-agentic-workflows.pdf) | Official GitHub framing for agentic CI/CD, permissions, and safe outputs |
| [OWASP Top 10 for LLM Applications](https://owasp.org/www-project-top-10-for-large-language-model-applications/) | Prompt injection, insecure output handling, plugin/tool risk, excessive agency, and LLM app security |
| [NIST AI RMF Generative AI Profile](https://www.nist.gov/publications/artificial-intelligence-risk-management-framework-generative-artificial-intelligence) | Governance and risk-management framing for generative AI systems |
| [RAG best practices](https://docs.llamaindex.ai/) | LlamaIndex documentation |
| *Build a Large Language Model (From Scratch)* (Raschka) | LLM internals |

---

**Next**

→ [**Module 4B — ML Engineering & MLOps**](../02-ML工程与MLOps/Guide.md)

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
