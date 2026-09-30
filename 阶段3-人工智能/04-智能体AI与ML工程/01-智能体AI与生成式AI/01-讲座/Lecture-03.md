---
title: 第 03 讲 - 构建 Agent I：基础（模型、工具、指令）
description: 第 03 讲 - 构建 Agent I：基础（模型、工具、指令）
---

# 第 03 讲 - 构建 Agent I：基础（模型、工具、指令）

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [第 02 讲](Lecture-02.md) | **下一讲：** [第 04 讲](Lecture-04.md)

---

本讲及下一讲提炼**构建 agent 的实践者框架**——正是这些设计选择，把可靠的生产级 agent 与 demo 区分开来。它沿用了 OpenAI 关于构建 agent 的实用指南所推广开来的结构，并立足于众多真实部署中观察到的模式。

本讲覆盖**基础**：agent 究竟是什么，何时应当（以及不应当）构建 agent，以及每个 agent 都由其构成的三个组件——**模型、工具与指令**。第 04 讲覆盖 orchestration 与护栏。

---

## 1. 什么是 agent？

> **Agent 是能够独立代表你完成任务的系统。**

传统软件让用户*简化并自动化*一个工作流。Agent 则更进一步：它**代表用户、以高度独立性**执行该工作流。

**工作流**是为达成某个目标而必须执行的一系列步骤——解决一个支持工单、预订一次行程、提交一次代码变更、生成一份报告。

**什么*不*是 agent：**集成了 LLM 却不用它来*控制工作流执行*的应用——简单的聊天机器人、单轮 LLM 调用、情感分类器。包装一次模型调用并不构成 agency。

Agent 具备两个核心特征，使其能够可靠地代表用户行动：

1. **它使用 LLM 来管理工作流执行并做出决策。**它能识别工作流何时完成，能主动纠正自身动作，并且在失败时能够停止并**将控制权交还给用户**。
2. **它能访问工具**，以便与外部系统交互（既能收集上下文，也能采取行动），并针对当前状态**动态选择**合适的工具——始终处于**明确定义的护栏**之内。

---

## 2. 何时应当构建 agent？

构建 agent 意味着重新思考系统如何做出决策。Agent 恰恰在**确定性的、基于规则的自动化力所不及**之处大放异彩。

最典型的例子是**支付欺诈分析**。传统规则引擎的工作方式像一个*检查清单*，依据预设标准标记交易。LLM agent 的工作方式更像一名**经验丰富的调查员**——权衡上下文、考量细微模式，并在没有任何硬性规则被违反时仍能捕捉到可疑活动。正是这种细致入微的推理，让 agent 能够处理复杂、模糊的情形。

优先考虑那些**一直抗拒自动化**的工作流，尤其是传统方法遭遇阻力之处：

<div class="lecture-map" markdown>

| 信号 | 表现形态 | 示例 |
|--------|--------------------|---------|
| **复杂的决策** | 细致的判断、例外情况、对上下文敏感的决策 | 客服中的退款审批 |
| **难以维护的规则** | 规则集变得笨重难管；更新代价高或易出错 | 供应商安全审查 |
| **高度依赖非结构化数据** | 解读自然语言、从文档中抽取信息、对话式交互 | 处理家庭保险理赔 |

</div>

> **先做验证。**在决定投入 agent 之前，确认你的用例明确满足这些标准。如果不满足，一个**确定性方案可能就够了**——而且更便宜、更快、也更容易推理。（与推理课程同样的纪律：简单工具够用时，不要去拿重型工具。）

---

## 3. Agent 设计基础——三个组件

在最基本的形式下，agent 包含三个组件：

<div class="lecture-map" markdown>

| # | 组件 | 作用 |
|---|-----------|------|
| 01 | **模型** | 驱动 agent 推理与决策的 LLM |
| 02 | **工具** | agent 可用于采取行动的外部函数或 API |
| 03 | **指令** | 界定 agent 行为方式的明确指导方针与护栏 |

</div>

在代码中（使用某个 agents 框架），它小到只需：

```python
weather_agent = Agent(
    name="Weather agent",
    instructions="You are a helpful agent who can talk to users about the weather.",
    tools=[get_weather],
)
```

本讲余下部分将依次讲解各个组件。

---


<details>
<summary>English original</summary>

**Lecture 03 - Building Agents I: Foundations (Model, Tools, Instructions)**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 02](Lecture-02.md) | **Next:** [Lecture 04](Lecture-04.md)

---

This lecture and the next distill the **practitioner framework for building agents** — the design choices that separate a reliable production agent from a demo. It follows the structure popularized by OpenAI's practical guidance on building agents, grounded in patterns seen across many real deployments.

This lecture covers the **foundations**: what an agent actually is, when you should (and should not) build one, and the three components every agent is made of — **model, tools, and instructions**. Lecture 04 covers orchestration and guardrails.

---

**1. What is an agent?**

> **Agents are systems that independently accomplish tasks on your behalf.**

Conventional software lets a user *streamline and automate* a workflow. An agent goes further: it performs the workflow **on the user's behalf, with a high degree of independence**.

A **workflow** is a sequence of steps that must be executed to meet a goal — resolving a support ticket, booking a reservation, committing a code change, generating a report.

**What is *not* an agent:** applications that integrate an LLM but don't use it to *control workflow execution* — simple chatbots, single-turn LLM calls, sentiment classifiers. Wrapping a model call is not agency.

An agent has two core characteristics that let it act reliably on a user's behalf:

1. **It uses an LLM to manage workflow execution and make decisions.** It recognizes when a workflow is complete, can proactively correct its own actions, and — on failure — can halt and **transfer control back to the user**.
2. **It has access to tools** to interact with external systems (both to gather context and to take action) and **dynamically selects** the right tool for the current state — always within **clearly defined guardrails**.

---

**2. When should you build an agent?**

Building an agent means rethinking how your system makes decisions. Agents shine exactly where **deterministic, rule-based automation falls short**.

The canonical example is **payment fraud analysis**. A traditional rules engine works like a *checklist*, flagging transactions against preset criteria. An LLM agent works more like a **seasoned investigator** — weighing context, considering subtle patterns, and catching suspicious activity even when no hard rule is violated. That nuanced reasoning is what lets agents handle complex, ambiguous situations.

Prioritize workflows that have **resisted automation**, especially where traditional methods hit friction:

<div class="lecture-map" markdown>

| Signal | What it looks like | Example |
|--------|--------------------|---------|
| **Complex decision-making** | Nuanced judgment, exceptions, context-sensitive calls | Refund approval in customer service |
| **Difficult-to-maintain rules** | Rulesets grown unwieldy; updates costly or error-prone | Vendor security reviews |
| **Heavy reliance on unstructured data** | Interpreting natural language, extracting from documents, conversational interaction | Processing a home-insurance claim |

</div>

> **Validate first.** Before committing to an agent, confirm your use case clearly meets these criteria. If it doesn't, a **deterministic solution may suffice** — and will be cheaper, faster, and easier to reason about. (Same discipline as the inference course: don't reach for the heavy tool when a simple one fits.)

---

**3. Agent design foundations — the three components**

In its most fundamental form, an agent has three components:

<div class="lecture-map" markdown>

| # | Component | Role |
|---|-----------|------|
| 01 | **Model** | The LLM powering the agent's reasoning and decision-making |
| 02 | **Tools** | External functions or APIs the agent can use to take action |
| 03 | **Instructions** | Explicit guidelines and guardrails defining how the agent behaves |

</div>

In code (using an agents framework), this is as small as:

```python
weather_agent = Agent(
    name="Weather agent",
    instructions="You are a helpful agent who can talk to users about the weather.",
    tools=[get_weather],
)
```

The rest of this lecture takes each component in turn.

---

</details>

## 4. 选择模型

不同模型在**任务复杂度、延迟和成本**之间做权衡。并非每个任务都需要最聪明的模型 —— 简单的检索或意图分类步骤可以用更小、更快的模型跑，而一个困难决策（是否批准退款？）则适合用能力更强的模型。在一个 workflow 中，你常常会使用**多个模型的组合**。

行之有效的方法：**每个任务都先用能力最强的模型做原型，以建立性能基线。** 然后再换入更小的模型，看它们是否仍能达到你的准确率目标。这样你既不会过早给 agent 的能力设上限，又能准确了解小模型在哪些地方成功、在哪些地方失败。

选择模型的原则：

1. **搭好评测**，建立性能基线。
2. 用可用的最好模型**达到你的准确率目标**。
3. 在可能之处用更小的模型替换更大的模型，**优化成本与延迟**。

> 这正是推理课程核心论点在 agent 层的映射：模型是你要对着实测目标去调的旋钮，而不是你默认接受的设定。

---

## 5. 定义工具

工具通过调用底层应用的 **API** 来扩展 agent。对于**没有 API 的遗留系统**，agent 可以退回到**computer-use 模型**，直接操控 Web 和应用 UI —— 就像人一样。

每个工具都应有**标准化、文档完善、经过测试、可复用的定义**。这使工具与 agent 之间能建立灵活的多对多关系，提升可发现性，简化版本管理，并避免重复定义。

大体上，agent 需要三类工具：

<div class="lecture-map" markdown>

| 类型 | 作用 | 示例 |
|------|--------------|----------|
| **数据** | 检索执行 workflow 所需的上下文 | 查询事务 DB 或 CRM，读取 PDF，搜索 Web |
| **动作** | 与系统交互以*做*某事 | 发送邮件/短信，更新 CRM 记录，把工单转交人工 |
| **编排** | **作为工具使用的其他 agent**（见 Lecture 04 的 Manager 模式） | 退款 agent、研究 agent、写作 agent |

</div>

```python
from agents import Agent, WebSearchTool, function_tool

@function_tool
def save_results(output):
    db.insert({"output": output, "timestamp": datetime.time()})
    return "File saved"

search_agent = Agent(
    name="Search agent",
    instructions="Help the user search the internet and save results if asked.",
    tools=[WebSearchTool(), save_results],
)
```

随着所需工具数量增长，**考虑把任务拆分到多个 agent 上**（Lecture 04，编排）。

---

## 6. 配置指令

高质量的**指令**对任何 LLM 应用都必不可少，对 agent 而言*尤其*关键：清晰的指令能减少歧义、改善决策，并让执行更顺畅、错误更少。

**agent 指令的最佳实践：**

<div class="lecture-map" markdown>

| 实践 | 原因 |
|----------|-----|
| **复用现有文档** | 把 SOP、支持话术和政策文档转成 LLM 友好的 **routines**。在客户服务中，routines 大致对应单篇知识库文章。 |
| **引导 agent 拆解任务** | 从密集资料中拆出更小、更清晰的步骤，能最大程度减少歧义，帮助模型跟上。 |
| **定义清晰的行动** | 每一步都应映射到具体的动作或输出（索要订单号；调用 API）。把话说明确 —— 甚至包括面向用户消息的措辞 —— 能减少误解空间。 |
| **捕捉边缘情况** | 真实交互会产生决策点（信息不全、意外问题）。用条件步骤和分支预先覆盖常见变体。 |

</div>

一个实用的加速办法：用更高级的模型**从现有文档自动生成指令**，例如：

```text
You are an expert in writing instructions for an LLM agent. Convert the following
help-center document into a clear, unambiguous, numbered set of instructions,
written as directions for an agent. The document to convert is: {{help_center_doc}}
```

---

## 关键要点

* **agent 独立执行多步 workflow**，用 LLM 驱动控制流并配合工具 —— 而不只是一次包装过的模型调用。
* 当 workflow 需要**细致的判断、规则庞杂难用，或依赖非结构化数据**时，才构建 agent；否则优先选择确定性的方案。
* 每个 agent = **model + tools + instructions**。先用最强的模型建立基线，再逐步优化下调；给它**标准化的数据 / 动作 / 编排工具**；并编写**明确、基于 routine、能覆盖边缘情况的指令**。

---


<details>
<summary>English original</summary>

**4. Selecting your model**

Different models trade off **task complexity, latency, and cost**. Not every task needs the smartest model — a simple retrieval or intent-classification step can run on a smaller, faster model, while a hard decision (approve a refund?) benefits from a more capable one. You will often use **a mix of models** across one workflow.

The approach that works: **prototype with the most capable model for every task to establish a performance baseline.** Then swap in smaller models and see whether they still hit your accuracy target. This way you never prematurely cap the agent's ability, and you learn exactly where small models succeed or fail.

The principles for choosing a model:

1. **Set up evals** to establish a performance baseline.
2. **Meet your accuracy target** with the best models available.
3. **Optimize for cost and latency** by replacing larger models with smaller ones where possible.

> This is the agent-layer mirror of the inference course's whole thesis: the model is a knob you tune against a measured target, not a default you accept.

---

**5. Defining tools**

Tools extend an agent by calling the **APIs** of underlying applications. For **legacy systems without APIs**, agents can fall back on **computer-use models** that drive web and application UIs directly — just as a human would.

Each tool should have a **standardized, well-documented, tested, reusable definition**. That enables flexible many-to-many relationships between tools and agents, improves discoverability, simplifies versioning, and prevents redundant definitions.

Broadly, agents need three types of tools:

<div class="lecture-map" markdown>

| Type | What it does | Examples |
|------|--------------|----------|
| **Data** | Retrieve the context needed to execute the workflow | Query a transaction DB or CRM, read a PDF, search the web |
| **Action** | Interact with systems to *do* something | Send an email/text, update a CRM record, hand a ticket off to a human |
| **Orchestration** | Other **agents used as tools** (see the Manager pattern, Lecture 04) | Refund agent, research agent, writing agent |

</div>

```python
from agents import Agent, WebSearchTool, function_tool

@function_tool
def save_results(output):
    db.insert({"output": output, "timestamp": datetime.time()})
    return "File saved"

search_agent = Agent(
    name="Search agent",
    instructions="Help the user search the internet and save results if asked.",
    tools=[WebSearchTool(), save_results],
)
```

As the number of required tools grows, **consider splitting tasks across multiple agents** (Lecture 04, Orchestration).

---

**6. Configuring instructions**

High-quality **instructions** are essential for any LLM app and *especially* critical for agents: clear instructions reduce ambiguity, improve decision-making, and produce smoother execution with fewer errors.

**Best practices for agent instructions:**

<div class="lecture-map" markdown>

| Practice | Why |
|----------|-----|
| **Use existing documents** | Turn SOPs, support scripts, and policy docs into LLM-friendly **routines**. In customer service, routines roughly map to individual knowledge-base articles. |
| **Prompt agents to break down tasks** | Smaller, clearer steps from dense resources minimize ambiguity and help the model follow along. |
| **Define clear actions** | Every step should map to a specific action or output (ask for the order number; call an API). Being explicit — even about the wording of a user-facing message — leaves less room for misinterpretation. |
| **Capture edge cases** | Real interactions create decision points (incomplete info, unexpected questions). Anticipate common variations with conditional steps and branches. |

</div>

A practical accelerator: use an advanced model to **auto-generate instructions from existing documents**, e.g.:

```text
You are an expert in writing instructions for an LLM agent. Convert the following
help-center document into a clear, unambiguous, numbered set of instructions,
written as directions for an agent. The document to convert is: {{help_center_doc}}
```

---

**Key takeaways**

* An **agent independently executes a multi-step workflow** using an LLM to drive control flow plus tools — not just a wrapped model call.
* Build an agent when the workflow needs **nuanced judgment, has unwieldy rules, or leans on unstructured data**; otherwise prefer a deterministic solution.
* Every agent = **model + tools + instructions**. Baseline with the strongest model, then optimize down; give it **standardized data / action / orchestration tools**; and write **explicit, routine-based instructions that capture edge cases**.

---

</details>

## 自检

1. 举两个**不是** agent 的 LLM 应用示例，并说明它们缺了什么。
2. 你的团队想自动化退款审批，这项审批目前需要人工权衡各种例外情况。这是好的 agent 候选场景吗？三条「何时构建」信号中哪一条适用？
3. 为什么先用能力最强的模型做原型、之后再换成更小的模型，而不是一开始就用小模型？
4. 将下列各项归类为 data / action / orchestration 工具：`read_pdf`，`send_email`，`research_agent.as_tool()`。
5. 对 agent 而言，指令含糊有什么风险（相较于单轮 chatbot）？

---

## 参考文献

* OpenAI — *A Practical Guide to Building Agents*（本讲遵循的框架：agent 定义、何时构建的判定标准、模型 / 工具 / 指令）。
* 交叉参考：[Lecture 08 — Tool Use & Function Calling](Lecture-08.md) · [Lecture 17 — Agent SDKs and Runtime APIs](Lecture-17.md) · [Lecture 04 — Orchestration & Guardrails](Lecture-04.md)

---

*下一讲：[Lecture 04](Lecture-04.md)*


<details>
<summary>English original</summary>

**Self-check**

1. Give two examples of LLM applications that are **not** agents, and say what they're missing.
2. Your team wants to automate refund approvals that currently need a human to weigh exceptions. Is this a good agent candidate? Which of the three "when to build" signals applies?
3. Why prototype with the most capable model first, then swap smaller models in — rather than starting small?
4. Classify each as a data / action / orchestration tool: `read_pdf`, `send_email`, `research_agent.as_tool()`.
5. What is the risk of vague instructions for an agent specifically (vs a single-turn chatbot)?

---

**References**

* OpenAI — *A Practical Guide to Building Agents* (the framework this lecture follows: agent definition, when-to-build criteria, model / tools / instructions).
* Cross-reference: [Lecture 08 — Tool Use & Function Calling](Lecture-08.md) · [Lecture 17 — Agent SDKs and Runtime APIs](Lecture-17.md) · [Lecture 04 — Orchestration & Guardrails](Lecture-04.md)

---

*Next: [Lecture 04](Lecture-04.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-03.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-03.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
