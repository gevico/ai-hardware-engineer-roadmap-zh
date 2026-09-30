---
title: 面向 AI 智能体的 MCP —— 深入解析 Model Context Protocol
description: 面向 AI 智能体的 MCP —— 深入解析 Model Context Protocol
---

# 面向 AI 智能体的 MCP —— 深入解析 Model Context Protocol

<div class="course-identity" style="--course-accent: #ea580c; --course-accent-rgb: 234, 88, 12;" markdown="1">
<div class="course-identity__icon">MCP</div>
<div markdown="1">
<p class="course-identity__eyebrow">阶段 3 · 智能体化 AI · 专题课程</p>
<p class="course-identity__title">连接 agent 与世界的开放协议 —— 架构、六大原语、构建并加固真实服务器、用于远程部署的 OAuth 2.1，以及生产技术栈，同步至 2025-11-25 规范与 2026 候选发布版。</p>
<p class="course-identity__meta">产物：一个经过测试、通过认证、加固过的 MCP 服务器，具备 tools、resources 与 prompts · 衡量：符合规范的生命周期、通过 MCP Inspector、作用域受限的认证，以及一次干净的安全审查</p>
</div>
</div>

> *Function calling 让单个模型使用你手工接好的一套工具。MCP 把“工具”变成一个协议 —— 于是任何 agent 都能通过一个标准接口与任何工具、任何数据源、任何系统对话。它是 agent 时代一直缺失的集成层。*

Anthropic 于 2024 年 11 月开源 **Model Context Protocol** 时，agent 生态存在一个 M×N 问题：每个 host（Claude、Cursor、某个内部 agent）都需要为每个工具（GitHub、Postgres、你的文件系统、某个 SaaS API）做定制集成。MCP 把它压缩为 **M + N** —— 针对协议把工具服务器写 *一次*，每个支持 MCP 的 host 都能用；把 host 构建 *一次*，它就能与整个生态对话。到 2026 年初，这个押注已全面兑现：OpenAI、Google DeepMind 和 Microsoft 都采用了 MCP，官方 `mcp` 包的月下载量突破 **~97M**，公共 registry 突破 **2,000 个社区服务器**。MCP 如今是 agent 触达世界的默认方式，“它能说 MCP 吗？”会是你在工作中要回答的问题。

本课程是该协议的工程师手册。它不是对某一次 SDK 调用的巡览 —— 而是从零构建心智模型（host / client / server、六大原语、生命周期），然后是动手技能（用 FastMCP 写一个服务器、用 Inspector 测试、通过 Streamable HTTP 配合 OAuth 2.1 部署），接着是那部分被跳过、也让人被攻破的内容（MCP 攻击面以及如何将其关闭），最后是生产技术栈（网关、registry、tool-context 预算，以及用 MCPMark 做评估）。

**层映射：** agent 技术栈中的 **tool-protocol 层** —— [agent runtime](../01-讲座/Lecture-02.md) 与它触及的每一个外部系统之间的标准接口。它直接位于 [Tool Use & Function Calling](../01-讲座/Lecture-08.md) 之上，位于 agent 实际 *做* 的一切之下。

**岗位目标：** AI 智能体工程师 · Agent 平台 / 基础设施工程师 · MCP 服务器开发者 · AI 安全工程师 · 开发者工具工程师。

**前置要求：**

* [AI Agent Development 2026 —— Lecture 08（Tool Use & Function Calling）](../01-讲座/Lecture-08.md) 以及 [Lecture 09（Structured Tools Beat Computer Use）](../01-讲座/Lecture-09.md) —— MCP 正是这些思想的 *协议化*。
* [Lecture 02（What Is an AI Agent Harness?）](../01-讲座/Lecture-02.md) —— MCP 所接入的 host / runtime（harness：agent 运行时框架）。
* 熟悉 Python（服务器相关课程使用官方 SDK / FastMCP）、JSON 和基础 HTTP。熟悉 OAuth 对 Lecture 06 有帮助，但该内容会从零讲起。

**配套内容：** agent 安全相关课程（[24 —— Runtime Discipline](../01-讲座/Lecture-24.md)、[40 —— OpenClaw Threat Model / MITRE ATLAS](../01-讲座/Lecture-40.md)）以及 [Lecture 23 §8 中的 MCPMark benchmark](../01-讲座/Lecture-23.md) —— 本课程的评测课以其为基础。

---

## 本课程为何这样组织

MCP 看起来很简单 —— “暴露一个工具，agent 调用它” —— 而这种简单掩盖了三件会在生产中咬人的事：协议有 **六个原语，而不是一个**（其中两个还是 *反向* 运行的，server→client）；远程服务器需要 **真正的授权**，而不是环境变量里的一个 API key；而协议的开放性同时也是它的 **攻击面**。八节课正是沿着这条梯度攀升：

```text
   understand it          build it              ship it safely
   ┌───────────────┐    ┌──────────────┐    ┌────────────────────┐
   01 why it exists     05 write a server    07 the attack surface
   02 architecture      06 transports +      08 production: gateways,
   03 core primitives      OAuth 2.1            registry, eval, scale
   04 reverse primitives
```

离开时，你对协议的理解将足以 *读懂规范*、构建一个值得部署的服务器，并抵御那些已经在真实环境中发生的攻击。

---


<details>
<summary>English original</summary>

**MCP for AI Agents — The Model Context Protocol in Depth**

<div class="course-identity" style="--course-accent: #ea580c; --course-accent-rgb: 234, 88, 12;" markdown="1">
<div class="course-identity__icon">MCP</div>
<div markdown="1">
<p class="course-identity__eyebrow">Phase 3 · Agentic AI · Special Course</p>
<p class="course-identity__title">The open protocol that connects agents to the world — architecture, the six primitives, building and securing real servers, OAuth 2.1 for remote deployment, and the production stack, current to the 2025-11-25 spec and the 2026 release candidate.</p>
<p class="course-identity__meta">Artifact: a tested, authenticated, hardened MCP server with tools, resources, and prompts · Measure: spec-compliant lifecycle, passing MCP Inspector, scoped auth, and a clean security review</p>
</div>
</div>

> *Function calling lets one model use one set of tools you wired in by hand. MCP turns "tools" into a protocol — so any agent can talk to any tool, any data source, any system, through one standard interface. It is the integration layer the agent era was missing.*

When Anthropic open-sourced the **Model Context Protocol** in November 2024, the agent ecosystem had an M×N problem: every host (Claude, Cursor, an internal agent) needed a bespoke integration for every tool (GitHub, Postgres, your filesystem, a SaaS API). MCP collapses that to **M + N** — write a tool server *once* against the protocol, and every MCP-capable host can use it; build a host *once*, and it speaks to the whole ecosystem. By early 2026 that bet had paid off comprehensively: OpenAI, Google DeepMind, and Microsoft all adopted MCP, the official `mcp` package crossed **~97M monthly downloads**, and the public registry passed **2,000 community servers**. MCP is now the default way agents reach the world, and "can it speak MCP?" is a question you will answer on the job.

This course is the engineer's manual for that protocol. Not a tour of one SDK call — a ground-up build of the mental model (host / client / server, the six primitives, the lifecycle), then the hands-on skills (write a server with FastMCP, test it with Inspector, deploy it over Streamable HTTP with OAuth 2.1), then the part that gets skipped and gets people breached (the MCP attack surface and how to shut it down), and finally the production stack (gateways, the registry, tool-context budgets, and evaluation with MCPMark).

**Layer mapping:** the **tool-protocol layer** of the agent stack — the standard interface between the [agent runtime](../01-讲座/Lecture-02.md) and every external system it touches. It sits directly on top of [Tool Use & Function Calling](../01-讲座/Lecture-08.md) and underneath everything an agent actually *does*.

**Role targets:** AI Agent Engineer · Agent Platform / Infrastructure Engineer · MCP Server Developer · AI Security Engineer · Developer-Tools Engineer.

**Prerequisites:**

* [AI Agent Development 2026 — Lecture 08 (Tool Use & Function Calling)](../01-讲座/Lecture-08.md) and [Lecture 09 (Structured Tools Beat Computer Use)](../01-讲座/Lecture-09.md) — MCP is the *protocolization* of exactly those ideas.
* [Lecture 02 (What Is an AI Agent Harness?)](../01-讲座/Lecture-02.md) — the host/runtime MCP plugs into.
* Comfort with Python (the server lectures use the official SDK / FastMCP), JSON, and basic HTTP. OAuth familiarity helps for Lecture 06 but is taught from scratch.

**Pairs with:** the agent-security lectures ([24 — Runtime Discipline](../01-讲座/Lecture-24.md), [40 — OpenClaw Threat Model / MITRE ATLAS](../01-讲座/Lecture-40.md)) and the [MCPMark benchmark in Lecture 23 §8](../01-讲座/Lecture-23.md) — which this course's evaluation lecture builds on.

---

**Why this course is structured the way it is**

MCP looks simple — "expose a tool, the agent calls it" — and that simplicity hides three things that bite in production: the protocol has **six primitives, not one** (and two of them run *backwards*, server→client); remote servers need **real authorization**, not an API key in an env var; and the protocol's openness is also its **attack surface**. The eight lectures climb that exact gradient:

```text
   understand it          build it              ship it safely
   ┌───────────────┐    ┌──────────────┐    ┌────────────────────┐
   01 why it exists     05 write a server    07 the attack surface
   02 architecture      06 transports +      08 production: gateways,
   03 core primitives      OAuth 2.1            registry, eval, scale
   04 reverse primitives
```

You leave understanding the protocol well enough to *read the spec*, build a server worth deploying, and defend it against the attacks that are already happening in the wild.

---

</details>

## 课程地图（8 讲）

<div class="lecture-map" markdown>

| # | 讲 | 主线 |
|---|---------|-----------|
| [01](Lecture-01.md) | **MCP 为何存在 —— M×N 问题与协议** —— 集成的爆炸式增长、"AI 的 USB-C"、JSON-RPC 2.0、采用时间线，以及何时 MCP 胜过手工接线的 function calling | 协议的理由 |
| [02](Lecture-02.md) | **架构与生命周期 —— Host、Client、Server** —— 三种角色、能力协商、initialize → operate → shutdown 生命周期，以及 JSON-RPC 消息类型 | 一个连接的样子 |
| [03](Lecture-03.md) | **核心原语 —— Tools、Resources、Prompts** —— 模型/应用/用户控制的三分法、JSON-Schema 工具定义、URI 寻址的 resources，以及 prompt 模板 | server 暴露什么 |
| [04](Lecture-04.md) | **反向原语 —— Sampling、Roots、Elicitation** —— server→client 方向：借用 host 的 LLM、文件系统边界，以及在任务中途向用户索取输入 | MCP 的双向性来自何处 |
| [05](Lecture-05.md) | **构建 MCP Server（FastMCP）** —— 官方 Python SDK 端到端：tools、resources、prompts、context、lifespan、结构化输出；用 MCP Inspector 测试 | 上手敲键盘 |
| [06](Lecture-06.md) | **传输、远程 Server 与 OAuth 2.1** —— stdio 对 Streamable HTTP、会话、SSE 的弃用，以及授权（client / resource server / auth server，RFC 9728） | 从 localhost 到互联网 |
| [07](Lecture-07.md) | **MCP 安全 —— agent 攻击面** —— 工具投毒、经工具结果发起的 prompt injection、confused deputy、token passthrough、致命三要素，以及防御措施 | 被跳过的那部分 |
| [08](Lecture-08.md) | **生产环境中的 MCP —— 网关、Registry、评测与综合项目** —— 组合与网关、registry、tool-context 预算、可观测性、MCPMark 评估，以及 2026 路线图 | 规模化交付 |

</div>

---

## 课程收获

学完本课程，你应当能够：

* 解释 **host / client / server** 架构与完整的 **initialize → operate → shutdown** 生命周期，并读懂原始的 JSON-RPC MCP 交互报文。
* 正确使用全部**六种原语** —— Tools、Resources、Prompts（server→client）与 Sampling、Roots、Elicitation（反向）—— 并说明每种由哪个角色控制。
* 用官方 Python SDK / FastMCP **构建、测试并打包**一个真实的 MCP server，并以 MCP Inspector 验证。
* 部署一个**基于 Streamable HTTP 的远程 server**，配以符合规范的 **OAuth 2.1** 授权（resource server 元数据、带 scope 的 token、auth server 与 resource server 的分离）。
* 识别并缓解 **MCP 攻击面** —— 工具投毒、经工具结果的 prompt injection、confused deputy、token passthrough、供应链 —— 并通过基本的安全审查。
* 在**生产环境**运行 server：把多个 server 组合在网关之后、管理 tool-context 预算、发布到 registry，并针对 MCPMark 做评估。

---

## 时效性 / 刷新纪律

MCP 很年轻且演进很快 —— 把事实锚定到某个 spec 日期：

* 本课程以 **2025-11-25** 稳定修订版为准，并标注 **2026-07-28 release candidate** 改了什么（无状态核心、用于 server 渲染 UI 的 **MCP Apps**、面向长时间运行任务的 **Tasks** 扩展，以及与 OAuth/OIDC 更紧密的对齐）。
* 传输：**stdio** 与 **Streamable HTTP** 是当前方案；旧的 **HTTP+SSE** 传输自 2025-03-26 起已**弃用** —— 每讲都使用当前传输，提及已弃用的那个只是为了让你避开它。
* SDK 接口（FastMCP、正走向 v2 的官方 `mcp` 包）每个版本都在变；把代码当作快照，并对照已安装的版本核实。每讲结尾都有一则 **`## Current as of`** 提示。

---

## 达成标准

当你能够端到端搭起一个**生产级 MCP server** 时，这门课就算学完了：

* 以正确的 schema 暴露 tools、resources 与 prompts，并在 MCP Inspector 中演示其为绿色通过。
* 通过 Streamable HTTP 把它远程提供服务，置于 OAuth 2.1 之后，使用带 scope 且绑定 audience 的 token。
* 向审计人员完整讲解其威胁模型 —— 工具投毒、经结果发起的注入、confused deputy、token passthrough —— 以及阻止每一项的控制措施。
* 把它注册，与其他 server 一起置于网关之后且不超出 tool-context 预算，并报告一次 agent 驱动它的 MCPMark 风格评估。

如果你能调用一个 tool 却既无法为 server 辩护、也无法把它部署到让别人放心信任，那你只是有个 demo。本课程的重点是别人能安全依赖的 server。

---

*相关：[Tool Use & Function Calling](../01-讲座/Lecture-08.md) · [Evaluation 与 MCP benchmark 阶梯（L23 §8）](../01-讲座/Lecture-23.md) · [OpenClaw 威胁模型 —— MITRE ATLAS](../01-讲座/Lecture-40.md) · 官方 spec：[modelcontextprotocol.io](https://modelcontextprotocol.io)*


<details>
<summary>English original</summary>

**Course Map (8 lectures)**

<div class="lecture-map" markdown>

| # | Lecture | The thread |
|---|---------|-----------|
| [01](Lecture-01.md) | **Why MCP Exists — The M×N Problem & the Protocol** — the integration explosion, "USB-C for AI," JSON-RPC 2.0, the adoption timeline, and when MCP beats hand-wired function calling | the case for a protocol |
| [02](Lecture-02.md) | **Architecture & Lifecycle — Host, Client, Server** — the three roles, capability negotiation, the initialize → operate → shutdown lifecycle, and JSON-RPC message types | the shape of a connection |
| [03](Lecture-03.md) | **Core Primitives — Tools, Resources, Prompts** — the model-/app-/user-controlled trichotomy, JSON-Schema tool definitions, URI-addressed resources, and prompt templates | what a server exposes |
| [04](Lecture-04.md) | **Reverse Primitives — Sampling, Roots, Elicitation** — the server→client direction: borrowing the host's LLM, filesystem boundaries, and asking the user for input mid-task | what makes MCP bidirectional |
| [05](Lecture-05.md) | **Building an MCP Server (FastMCP)** — the official Python SDK end to end: tools, resources, prompts, context, lifespan, structured output; testing with MCP Inspector | hands on the keyboard |
| [06](Lecture-06.md) | **Transports, Remote Servers & OAuth 2.1** — stdio vs Streamable HTTP, sessions, the SSE deprecation, and authorization (client / resource server / auth server, RFC 9728) | from localhost to the internet |
| [07](Lecture-07.md) | **MCP Security — The Agent Attack Surface** — tool poisoning, prompt injection via tool results, the confused deputy, token passthrough, the lethal trifecta, and the defenses | the part that gets skipped |
| [08](Lecture-08.md) | **Production MCP — Gateways, Registry, Eval & Capstone** — composition and gateways, the registry, tool-context budgets, observability, MCPMark evaluation, and the 2026 roadmap | shipping at scale |

</div>

---

**Course Outcomes**

By the end you should be able to:

* Explain the **host / client / server** architecture and the full **initialize → operate → shutdown** lifecycle, and read a raw JSON-RPC MCP exchange.
* Use all **six primitives** correctly — Tools, Resources, Prompts (server→client) and Sampling, Roots, Elicitation (the reverse direction) — and say which actor controls each.
* **Build, test, and package** a real MCP server with the official Python SDK / FastMCP, validated against the MCP Inspector.
* Deploy a **remote server over Streamable HTTP** with spec-compliant **OAuth 2.1** authorization (resource-server metadata, scoped tokens, the auth/resource server split).
* Identify and mitigate the **MCP attack surface** — tool poisoning, prompt injection via tool results, confused-deputy, token passthrough, supply-chain — and pass a basic security review.
* Run a server in **production**: compose servers behind a gateway, manage the tool-context budget, publish to the registry, and evaluate against MCPMark.

---

**Currency / Refresh Discipline**

MCP is young and moving fast — pin your facts to a spec date:

* This course tracks the **2025-11-25** stable revision and flags what the **2026-07-28 release candidate** changes (a stateless core, **MCP Apps** for server-rendered UIs, a **Tasks** extension for long-running work, and tighter OAuth/OIDC alignment).
* Transports: **stdio** and **Streamable HTTP** are current; the legacy **HTTP+SSE** transport is **deprecated** (since 2025-03-26) — every lecture uses the current transport and names the deprecated one only to warn you off it.
* SDK surfaces (FastMCP, the official `mcp` package heading to a v2) change release-to-release; treat code as a snapshot and verify against the installed version. Every lecture closes with a **`## Current as of`** note.

---

**Exit Criteria**

You are done with this course when you can stand up a **production-grade MCP server** end to end:

* Expose tools, resources, and prompts with correct schemas, and demonstrate it green in MCP Inspector.
* Serve it remotely over Streamable HTTP behind OAuth 2.1 with scoped, audience-bound tokens.
* Walk an auditor through its threat model — tool poisoning, injection-via-results, confused deputy, token passthrough — and the control that stops each.
* Register it, put it behind a gateway with other servers without blowing the tool-context budget, and report an MCPMark-style evaluation of an agent driving it.

If you can call one tool but can't defend the server or deploy it for someone else to trust, you have a demo. The point of this course is the server other people can safely depend on.

---

*Related: [Tool Use & Function Calling](../01-讲座/Lecture-08.md) · [Evaluation & the MCP benchmark ladder (L23 §8)](../01-讲座/Lecture-23.md) · [OpenClaw Threat Model — MITRE ATLAS](../01-讲座/Lecture-40.md) · official spec: [modelcontextprotocol.io](https://modelcontextprotocol.io)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/MCP for AI Agents/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/MCP%20for%20AI%20Agents/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
