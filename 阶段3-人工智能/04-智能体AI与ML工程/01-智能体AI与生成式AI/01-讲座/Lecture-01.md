---
title: 第 01 讲 - 2026 年的现代 AI Agent：什么变了
description: 第 01 讲 - 2026 年的现代 AI Agent：什么变了
---

# 第 01 讲 - 2026 年的现代 AI Agent：什么变了

**Course：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [课程指南](../Guide.md) | **下一讲：** [第 02 讲](Lecture-02.md)

---

这是 **AI Agent Development 2026** 的开篇。在动任何代码之前，先设定框架："现代 AI agent"在 2026 年到底意味着什么，是什么变化让 agent 如今得以工作、而它们在 2023 年大多不行，以及为什么有意思的工程已经*从模型内部转移到了围绕它的 harness（agent 运行时框架）中*。

本讲涵盖：

1. 一句话的定义——以及什么*不是* agent。
2. 2023 到 2026 之间究竟变了什么。
3. **harness 时代**——工程如今所在之处。
4. 两个值得研究的参考系统。
5. 真实 agent 与 demo 的分界。
6. 本课程的组织方式。

---

## 1. 什么是现代 AI agent？

> **agent 是一个能替你独立完成多步任务的系统**——它用大语言模型在护栏内驱动对工具的控制流。

承载核心的词是**独立**。工作流是朝向某个目标的一系列步骤（处理工单、交付代码变更、核对发票）。传统软件让人*执行*该工作流更快。agent 则**替他们运行它**：它决定下一步，调用工具收集上下文并采取行动，察觉自己何时完成或卡住，并纠正或交回控制权。

**什么*不是* agent：**聊天机器人、单轮大语言模型调用、情感分类器、RAG（检索增强生成）式的"与你的文档对话"框。它们用了模型；但没让模型*控制执行*。把一个 completion 包起来并不构成能动性。（我们会在[第 03 讲](Lecture-03.md)中把这一点讲精确。）

---

## 2. 什么变了（2023 → 2026）

agent 在 2023 年做过演示，但在生产中大多翻了车。是若干项相互独立的转变——而不是某一次单一突破——让它们可靠到足以交付。这些都与某一次具体的模型发布无关；它们是技术栈中持久性的变化。

<div class="lecture-map" markdown>

| 维度 | ~2023 | 2026 |
|------|-------|------|
| **推理** | 单次完成；多步脆弱 | 能规划、自检并在任务中途恢复的**推理模型** |
| **接口** | 输入一轮对话 → 输出一个答案 | **run loop**——模型持续多步，直到满足退出条件 |
| **工具** | 定制的、按应用分别实现的 function calling | **标准工具协议（MCP）**；对无 API 的遗留 UI 使用 computer-use |
| **上下文** | 4K–8K token | **100K–1M token**——整个仓库、长会话（按真实 KV-cache 成本计） |
| **模态** | 仅文本 | **多模态**——视觉/音频感知作为子 agent |
| **部署** | 一个托管的聊天页面 | **持久化、本地优先的控制平面**；端侧 / 边缘 |
| **经济性** | 长会话成本过高 | 便宜到足以支撑**长时间运行、工具密集的后台工作** |

</div>

综合效果是：模型现在可以在**几十步和工具调用**中持续停留在同一任务上，其所处的上下文大到足以容纳真实工作集，成本又低到可以持续运行。这正是聊天 demo 与会开 PR 的编码 agent 之间的差别。

> **时效性提示。**具体的模型名称、上下文上限和价格每隔几个月就会变化——本课程刻意教授的是*稳定*的那一层（run loop、工具协议、会话、护栏）。把你看看到的任何版本号都当作某一时刻的快照。

---

## 3. harness 时代——工程如今所在之处

以下是本课程中最关键的一次思维转变：

> **agent 的难点不在模型——而在围绕它的 harness。**

模型提供推理与工具选择能力。而让 agent 变得*可靠*的一切，都是包在它外面的 runtime：

* **run loop**（何时继续、何时停止、最大轮数、错误处理），
* **会话**与持久化状态（这样流中途崩溃不会丢掉任务），
* **工具接线**与权限，
* **记忆**与检索，
* **护栏**（相关性、安全、PII、工具风险、人在回路），
* **遥测**与恢复。

这就是人们所说的**"harness AI"**。两个跑*同一个*模型的 agent，可靠性可以相差极大，纯粹因为它们的 harness 不同。本课程余下的大部分内容，都是关于构建一个好的 harness——这也是为什么模块 1 会直接进入[第 02 讲——*什么是 AI agent harness？*](Lecture-02.md)。

---


<details>
<summary>English original</summary>

**Lecture 01 - The Modern AI Agent in 2026: What Changed**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Course Guide](../Guide.md) | **Next:** [Lecture 02](Lecture-02.md)

---

This is the opener for **AI Agent Development 2026**. Before any code, we set the frame: what a "modern AI agent" actually means in 2026, what changed to make agents work now when they mostly didn't in 2023, and why the interesting engineering has moved *out of the model and into the harness around it*.

This lecture covers:

1. The one-sentence definition — and what is *not* an agent.
2. What actually changed between 2023 and 2026.
3. The **harness era** — where the engineering lives now.
4. Two reference systems worth studying.
5. What separates a real agent from a demo.
6. How this course is organized.

---

**1. What is a modern AI agent?**

> **An agent is a system that independently accomplishes a multi-step task on your behalf** — using an LLM to drive control flow over tools, within guardrails.

The load-bearing word is **independently**. A workflow is a sequence of steps toward a goal (resolve a ticket, ship a code change, reconcile an invoice). Conventional software lets a person *run* that workflow faster. An agent **runs it for them**: it decides the next step, calls tools to gather context and take action, notices when it's done or stuck, and corrects or hands back control.

**What is *not* an agent:** a chatbot, a single-turn LLM call, a sentiment classifier, a RAG "chat with your docs" box. They use a model; they don't let the model *control execution*. Wrapping a completion is not agency. (We make this precise in [Lecture 03](Lecture-03.md).)

---

**2. What changed (2023 → 2026)**

Agents were demoed in 2023 and mostly fell over in production. Several independent shifts — not one breakthrough — made them reliable enough to ship. None of these are about a single model release; they're durable changes in the stack.

<div class="lecture-map" markdown>

| Axis | ~2023 | 2026 |
|------|-------|------|
| **Reasoning** | One-pass completion; brittle multi-step | **Reasoning models** that plan, self-check, and recover mid-task |
| **Interface** | One chat turn in → one answer out | **Run loops** — the model takes many steps until an exit condition |
| **Tools** | Bespoke, per-app function calling | **Standard tool protocols (MCP)**; computer-use for legacy UIs with no API |
| **Context** | 4K–8K tokens | **100K–1M tokens** — whole repos, long sessions (at real KV-cache cost) |
| **Modality** | Text only | **Multimodal** — vision/audio perception as sub-agents |
| **Deployment** | A hosted chat page | **Persistent, local-first control planes**; on-device / edge |
| **Economics** | Too expensive for long sessions | Cheap enough for **long-running, tool-rich background work** |

</div>

The combined effect: a model can now stay on a task across **dozens of steps and tool calls**, over a context large enough to hold the real working set, cheaply enough to run continuously. That is the difference between a chat demo and a coding agent that opens a PR.

> **Currency caveat.** Specific model names, context limits, and prices move every few months — this course deliberately teaches the *stable* layer (run loops, tool protocols, sessions, guardrails). Treat any version number you see as a snapshot.

---

**3. The harness era — where the engineering lives now**

Here is the single most important mental shift in this course:

> **The hard part of an agent is not the model — it's the harness around it.**

The model gives you reasoning and tool selection. Everything that makes an agent *reliable* is the runtime wrapped around it:

* the **run loop** (when to keep going, when to stop, max turns, error handling),
* **sessions** and durable state (so a crash mid-stream doesn't lose the task),
* **tool wiring** and permissions,
* **memory** and retrieval,
* **guardrails** (relevance, safety, PII, tool-risk, human-in-the-loop),
* **telemetry** and recovery.

This is what people mean by **"harness AI."** Two agents on the *same* model can differ enormously in reliability purely because of their harness. The rest of this course is, in large part, about building a good harness — which is why Module 1 continues straight into [Lecture 02 — *What is an AI agent harness?*](Lecture-02.md).

---

</details>

## 4. 两个值得研究的参考系统

要理解现代 agent，最有效的途径是看那些在类生产使用中真实运行的系统，而不是 demo：

* **编码 agent**（如 Claude Code）—— 读取 repo、规划跨文件改动、运行测试、提交、开 PR、通过 MCP 连接工具，并在 CI 中运行的 agent。这是 agent 成为 *软件工作者* 这一品类最清晰的例证。
* **本地优先的个人助理**（如 OpenClaw）—— 一个长期存活的**控制平面**，在众多交互面上掌管渠道、会话、工具和事件，把入站消息视为不可信输入。**模块 4** 会逐件拆解 OpenClaw。

两者揭示同一个道理：产品 *就是* harness（agent 运行时框架）。

---

## 5. 真实 agent 与 demo 的区别

demo 优化的是顺利路径下的对话记录。生产级 agent 要按四个维度评判 —— 与本路线图其余部分相同的工程准则：

* **可靠性** —— 它能把任务做完吗？做不完时能安全失败吗？
* **延迟** —— 在多步循环中，首 token 时间和每步耗时是多少？
* **成本** —— 单独看 $/task across all the tool calls and tokens, not $/token。
* **安全** —— 它能否抵御 prompt injection、遵守权限，并把高风险动作升级给人类处理？

Agent 是**系统工程**，不是 prompt 手工艺。Prompt 很重要，但一个没有护栏、没有 eval、没有恢复机制的聪明 prompt 是负债，不是产品。

---

## 6. 本课程如何组织

**AI Agent Development 2026** 是一条理论先行的路径（完整模块图见[课程指南](../Guide.md)）：

<div class="lecture-map" markdown>

| 模块 | 你将学到什么 |
|--------|----------------|
| **1 · 从这里开始** | 发生了什么变化、harness、以及如何构建 agent（模型/工具/指令 → 编排/护栏） |
| **2 · 基础** | 模型如何工作，以及你如何与它对话 |
| **3 · 核心构建块** | 工具、记忆、RAG、编排、多模态、技能、eval —— 逐一讲解 |
| **4 · 生产与 runtime** | 安全、持久状态、启动、runtime 选型、部署 |
| **5 · 示例：OpenClaw** | 一个真实 harness，逐件拆开 |
| **6 · 实践：genie-claw** | 端到端构建你自己的最小 harness |

</div>

结课项目 **genie-claw** 就是你自己的最小 agent harness —— 一个本地 LLM runtime 加一个 OpenClaw 风格的网关，run loop、工具和护栏都由你掌控。从这里到那里之间的所有内容，都是为构建它服务的。

---

## 关键要点

* **现代 agent 自主运行多步工作流**，靠的是让 LLM 在护栏内控制工具使用 —— 而不是包装过的 completion。
* Agent 在 2026 年之所以成立，是因为**多重转变的汇聚**：推理模型、run loop、标准工具协议（MCP）、长上下文、多模态、廉价推理，以及本地优先部署。
* 工程重心已经从**模型转移到 harness**。可靠性、延迟、成本和安全都是 *harness* 的属性。
* 这是**系统工程**；本课程一路构建到属于你自己的 harness，**genie-claw**。

---

## 自查

1. 给出一句话的 agent 定义，并解释为什么一个 RAG「与你的文档聊天」的盒子不算合格。
2. 说出 2023 年以来让 agent 具备生产可行性的三项转变，以及每项为什么重要。
3. 「难的是 harness，不是模型」是什么意思？列出 harness 的三项职责。
4. 两个团队用*同一个*模型交付 agent，其中一个可靠性高得多。差异来自哪里？
5. 四个生产维度（可靠性/延迟/成本/安全）中，哪一个在 demo 中最常被忽略，后果是什么？

---

## 参考资料

* OpenAI —— *A Practical Guide to Building Agents*（agent 定义、何时构建、基础）—— 在[讲座 03](Lecture-03.md) / [讲座 04](Lecture-04.md) 中展开。
* 参考系统：**Claude Code**（智能体化编码）· **OpenClaw**（本地优先控制平面）—— 在模块 4 中剖析。
* 交叉引用：[讲座 02 —— 什么是 AI agent harness？](Lecture-02.md) · [课程指南 —— 完整课程体系](../Guide.md)

---

*下一篇：[讲座 02](Lecture-02.md)*


<details>
<summary>English original</summary>

**4. Two reference systems worth studying**

Modern agents are easiest to understand through systems that actually run in production-like usage, not demos:

* **Coding agents** (e.g., Claude Code) — agents that read a repo, plan changes across files, run tests, commit, open PRs, connect tools over MCP, and run in CI. The clearest example of agents becoming a *software-worker* category.
* **Local-first personal assistants** (e.g., OpenClaw) — a long-lived **control plane** that owns channels, sessions, tools, and events across many surfaces, treating inbound messages as untrusted input. **Module 4** dissects OpenClaw piece by piece.

Both show the same lesson: the product *is* the harness.

---

**5. A real agent vs a demo**

A demo optimizes for a happy-path transcript. A production agent is judged on four axes — the same engineering discipline as the rest of this roadmap:

* **Reliability** — does it finish the task, and fail safely when it can't?
* **Latency** — time-to-first-token and per-step time, across a multi-step loop.
* **Cost** — $/task across all the tool calls and tokens, not $/token in isolation.
* **Safety** — does it resist prompt injection, respect permissions, and escalate high-risk actions to a human?

Agents are **systems engineering**, not prompt-craft. Prompts matter, but a clever prompt with no guardrails, no eval, and no recovery is a liability, not a product.

---

**6. How this course is organized**

**AI Agent Development 2026** is a theory-first path (full module map in the [course Guide](../Guide.md)):

<div class="lecture-map" markdown>

| Module | What you learn |
|--------|----------------|
| **1 · Start here** | What changed, the harness, and how to build an agent (model/tools/instructions → orchestration/guardrails) |
| **2 · Fundamentals** | How the model works and how you talk to it |
| **3 · Core building blocks** | Tools, memory, RAG, orchestration, multimodal, skills, eval — one by one |
| **4 · Production & runtime** | Security, durable state, startup, runtime choice, deployment |
| **5 · Example: OpenClaw** | A real harness, taken apart |
| **6 · Practice: genie-claw** | Build your own minimal harness end-to-end |

</div>

The capstone, **genie-claw**, is your own minimal agent harness — a local LLM runtime plus an OpenClaw-style gateway, with the run loop, tools, and guardrails you control. Everything between here and there is in service of building it.

---

**Key takeaways**

* A **modern agent independently runs a multi-step workflow** by letting an LLM control tool use within guardrails — not a wrapped completion.
* Agents work in 2026 because of **converging shifts**: reasoning models, run loops, standard tool protocols (MCP), long context, multimodality, cheap inference, and local-first deployment.
* The engineering has moved **from the model to the harness**. Reliability, latency, cost, and safety are *harness* properties.
* This is **systems engineering**; the course builds toward your own harness, **genie-claw**.

---

**Self-check**

1. Give the one-sentence definition of an agent, and explain why a RAG "chat with your docs" box doesn't qualify.
2. Name three shifts since 2023 that made agents production-viable, and why each matters.
3. What does "the hard part is the harness, not the model" mean? List three harness responsibilities.
4. Two teams ship agents on the *same* model; one is far more reliable. Where does that difference come from?
5. Which of the four production axes (reliability / latency / cost / safety) is most often ignored in demos, and what's the consequence?

---

**References**

* OpenAI — *A Practical Guide to Building Agents* (agent definition, when-to-build, foundations) — expanded in [Lecture 03](Lecture-03.md) / [Lecture 04](Lecture-04.md).
* Reference systems: **Claude Code** (agentic coding) · **OpenClaw** (local-first control plane) — dissected in Module 4.
* Cross-reference: [Lecture 02 — What is an AI agent harness?](Lecture-02.md) · [Course Guide — full curriculum](../Guide.md)

---

*Next: [Lecture 02](Lecture-02.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
