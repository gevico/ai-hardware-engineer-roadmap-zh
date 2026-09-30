---
title: Lecture 01 - 为什么存在 MCP：M×N 问题与协议
description: Lecture 01 - 为什么存在 MCP：M×N 问题与协议
---

# Lecture 01 - 为什么存在 MCP：M×N 问题与协议

**Collection:** [MCP for AI Agents](README.md) | **Previous:** [← Course index](README.md) | **Next:** [Lecture 02](Lecture-02.md)

---

你构建的每个 agent 最终都会撞上同一堵墙。模型足够聪明；demo 能跑通；然后有人让它从 Postgres 读数据、在 GitHub 提 issue、再查 PagerDuty 的 on-call 排班——接下来两周你都在写胶水代码。每个工具都有自己的 auth、自己的 payload 结构、自己的错误语义，而你把这些全部手工接线进你这一个 agent。随后第二个团队构建第二个 agent，又把*同样*的胶水重写一遍，因为你的集成逻辑长在你的 harness（agent 运行时框架）里，没人能复用它。这就是 agent 时代撞上的集成税，也正是 **Model Context Protocol (MCP)** 被创造出来要消除的问题。

MCP 由 Anthropic 在 **2024 年 11 月**开源，它让「一个工具」变成你针对某个 wire protocol 发布*一次*的东西，而不是要接线进某一个 app 的东西。为 GitHub 写一次 server，每个支持 MCP 的 host——Claude Desktop、Claude Code、Cursor、你的内部 agent——都能用它，而不需要了解任何 GitHub API 的知识。构建一次 host，它就能与别人已经写好的整个 server 目录对话。这一注押的是：一套共享协议会胜过一千个定制集成；到 2026 年，它已经全面兑现：OpenAI、Google DeepMind 和 Microsoft 都在 2025 年采纳了 MCP，`mcp` PyPI 包月下载量突破 **~97M**，官方注册表列出了**超过 2,000 个 server**。

第一讲论证的是*为什么需要协议*，尚未涉及如何构建协议。我们会精确地数一遍集成的爆炸式增长，点明 MCP 究竟标准化了什么（基于 JSON-RPC 2.0 的 wire protocol——不是框架，也不是 agent），梳理你必须在工作中锁定版本的采纳时间线与 spec 修订，把 MCP 与你已经熟悉的手工接线 function calling 做对比，最后以心智模型收尾——host、client、server 以及六个原语——课程的其余部分都建立在这个模型之上。

---

## 学习目标

1. 量化 **M×N 集成爆炸**，并精确解释 MCP 如何将其压缩为 **M + N**。

2. 说明 MCP 标准化了什么——一套用于发现和调用 server 的 tools、resources 和 prompts 的 **JSON-RPC 2.0 wire protocol**——以及它刻意*不*标准化什么（框架或 agent）。

3. 复述**采纳时间线与 spec 修订**（2024 年 11 月发布 → 2025-03-26 → 2025-06-18 → 2025-11-25 stable → 2026-07-28 RC），并解释为什么一旦所有人都说同一种协议，它就会凭**网络效应**胜出。

4. 针对给定系统判断：**什么时候 MCP 优于**朴素的 function calling，什么时候朴素的 function calling 才是正确选择。

5. 勾勒 **host / client / server** 模型，说出**六个原语**以及各自由哪个角色控制，并前向引用第 02–04 讲。

---


<details>
<summary>English original</summary>

**Lecture 01 - Why MCP Exists: The M×N Problem & the Protocol**

**Collection:** [MCP for AI Agents](README.md) | **Previous:** [← Course index](README.md) | **Next:** [Lecture 02](Lecture-02.md)

---

Every agent you build eventually hits the same wall. The model is smart enough; the demo works; then someone asks it to read from Postgres, file an issue in GitHub, and check the on-call rotation in PagerDuty — and you spend the next two weeks writing glue. Each tool has its own auth, its own payload shape, its own error semantics, and you hand-wire all of it into your one agent. Then a second team builds a second agent and writes the *same* glue again, because your integration lived inside your harness and nobody can reuse it. That is the integration tax the agent era ran into, and it is the problem the **Model Context Protocol (MCP)** was created to remove.

MCP, open-sourced by Anthropic in **November 2024**, makes "a tool" a thing you publish *once* against a wire protocol instead of a thing you wire into one app. Write a server for GitHub once and every MCP-capable host — Claude Desktop, Claude Code, Cursor, your internal agent — can use it without knowing anything about GitHub's API. Build a host once and it speaks to the entire catalog of servers other people already wrote. The bet was that a shared protocol would beat a thousand bespoke integrations, and by 2026 it has paid off comprehensively: OpenAI, Google DeepMind, and Microsoft all adopted MCP through 2025, the `mcp` PyPI package crosses **~97M monthly downloads**, and the official registry lists **more than 2,000 servers**.

This first lecture makes the case for *why a protocol*, not yet how to build one. We will count the integration explosion precisely, name what MCP actually standardizes (a wire protocol over JSON-RPC 2.0 — not a framework, not an agent), walk the adoption timeline and the spec revisions you must pin your work to, compare MCP against the hand-wired function calling you already know, and close with the mental model — host, client, server, and the six primitives — that the rest of the course builds on.

---

**Learning objectives**

1. Quantify the **M×N integration explosion** and explain precisely how MCP collapses it to **M + N**.
2. State what MCP standardizes — a **JSON-RPC 2.0 wire protocol** for discovering and invoking a server's tools, resources, and prompts — and what it deliberately does *not* (a framework or an agent).
3. Recount the **adoption timeline and spec revisions** (Nov 2024 launch → 2025-03-26 → 2025-06-18 → 2025-11-25 stable → 2026-07-28 RC) and explain why a protocol wins on **network effects** once everyone speaks it.
4. Decide, for a given system, **when MCP wins** over plain function calling and when plain function calling is the right call.
5. Sketch the **host / client / server** model and name the **six primitives** and the actor that controls each, forward-referencing Lectures 02–04.

---

</details>

## 1. M×N 集成爆炸

把痛苦量化。假设你有 **M 个宿主**——各不相同的 LLM 应用：Claude Desktop、Cursor、一个客服 agent、一个内部数据 agent。还有 **N 个工具或数据源**——GitHub、Postgres、本地文件系统、Slack、一个支付 API。在 MCP 之前，把它们连起来就是一个矩阵：每个宿主都需要定制代码才能与每个工具通信，因为每个宿主声明工具的方式各不相同，每个工具也各有自己的 API。这就是 **M × N** 个定制集成。4 个宿主和 5 个工具就是 20 个集成；一旦加入第 6 个工具，你就要再多做 6 个——每个宿主一个。更糟的是，集成逻辑活在每个宿主*内部*，所以没有任何一部分可复用，而且全部会各自腐坏。

MCP 把这个矩阵压缩成两份清单。每个宿主**一次**实现该协议（这就是 M）。每个工具被包进一个**服务器**，由它**一次**实现该协议（这就是 N）。现在任何宿主都能通过同一接口与任何服务器通信，成本是 **M + N**，而不是 M × N。把 GitHub 服务器写一次 → 四个宿主全都获得 GitHub。把新宿主构建一次 → 它第一天就能与全部五个服务器（以及注册表中另外约 2,000 个）通信。

```text
        BEFORE: M × N bespoke integrations          AFTER: M + N (write each once)

   Hosts                Tools                     Hosts          MCP          Servers
   ┌──────────┐        ┌──────────┐               ┌──────────┐   bus    ┌──────────────┐
   │ Claude   │━━━━━━━▶│ GitHub   │               │ Claude   │──┐    ┌──│ GitHub server│
   │ Desktop  │━━┓ ┏━━▶│ Postgres │               │ Desktop  │  │    │  └──────────────┘
   └──────────┘  ┃ ┃   └──────────┘               └──────────┘  │    │  ┌──────────────┐
   ┌──────────┐  ┃ ┃   ┌──────────┐               ┌──────────┐  ├────┼──│ Postgres srv │
   │ Cursor   │━━╋━╋━━▶│ Filesystem│              │ Cursor   │──┤ MCP│  └──────────────┘
   └──────────┘  ┃ ┃   └──────────┘               └──────────┘  │    │  ┌──────────────┐
   ┌──────────┐  ┃ ┃   ┌──────────┐               ┌──────────┐  ├────┼──│ Filesystem   │
   │ Support  │━━┛ ┗━━▶│ Slack    │               │ Support  │──┤    │  └──────────────┘
   │ agent    │━━━━━━━▶│ Payments │               │ agent    │──┘    └──│ Slack server │
   └──────────┘        └──────────┘               └──────────┘          └──────────────┘
     M = 4               N = 5                       4 + 5 = 9 implementations,
     4 × 5 = 20 integrations to build & maintain     each written once and reused
```


两个类比能让这个形状扎根。第一个是生态圈在用的那个：MCP 是 **“AI 的 USB-C”**。在 USB-C 之前，每个设备都有自己的接头，你得随身带一抽屉适配器；USB-C 定义了一份物理与电气契约，于是现在任何合规外设都能插进任何合规端口。MCP 就是那份把模型连接到工具和数据的契约——一个标准插头，取代一抽屉集成。

第二个类比是直接的技术先例，它比 USB-C 这个意象更有价值，因为它已经证明了这道算术成立。微软在 2016 年提出的 **Language Server Protocol (LSP)**，在另一个领域面对了完全相同的爆炸：**E 个编辑器 × L 种语言**，每个编辑器都要为每种语言的自动补全、跳转到定义和诊断手写一套集成。LSP 在编辑器与“语言服务器”之间定义了 JSON-RPC 协议，于是一种语言只需作为服务器实现一次，每个会说 LSP 的编辑器就都能获得它。E × L 变成了 E + L，生态随之爆发，编辑器也不再比拼“你支持哪些语言”。MCP 就是把 LSP 的思路应用到 agent 和工具上——而且和 LSP 一样，它建立在 JSON-RPC 之上。

---


<details>
<summary>English original</summary>

**1. The M×N integration explosion**

Put numbers on the pain. Say you have **M hosts** — distinct LLM applications: Claude Desktop, Cursor, a customer-support agent, an internal data agent. And **N tools or data sources** — GitHub, Postgres, the local filesystem, Slack, a payments API. Before MCP, connecting them is a matrix: every host needs custom code to talk to every tool, because each host has its own way of declaring tools and each tool has its own API. That is **M × N** bespoke integrations. Four hosts and five tools is twenty integrations; the moment you add the sixth tool, you owe six more — one per host. Worse, the integration lives *inside* each host, so none of it is reusable and all of it rots independently.

MCP collapses the matrix into two lists. Each host implements the protocol **once** (that is the M). Each tool is wrapped in a **server** that speaks the protocol **once** (that is the N). Now any host talks to any server through the same interface, and the cost is **M + N**, not M × N. Write the GitHub server once → all four hosts get GitHub. Build a new host once → it speaks to all five servers (and the other ~2,000 in the registry) on day one.

```text
        BEFORE: M × N bespoke integrations          AFTER: M + N (write each once)

   Hosts                Tools                     Hosts          MCP          Servers
   ┌──────────┐        ┌──────────┐               ┌──────────┐   bus    ┌──────────────┐
   │ Claude   │━━━━━━━▶│ GitHub   │               │ Claude   │──┐    ┌──│ GitHub server│
   │ Desktop  │━━┓ ┏━━▶│ Postgres │               │ Desktop  │  │    │  └──────────────┘
   └──────────┘  ┃ ┃   └──────────┘               └──────────┘  │    │  ┌──────────────┐
   ┌──────────┐  ┃ ┃   ┌──────────┐               ┌──────────┐  ├────┼──│ Postgres srv │
   │ Cursor   │━━╋━╋━━▶│ Filesystem│              │ Cursor   │──┤ MCP│  └──────────────┘
   └──────────┘  ┃ ┃   └──────────┘               └──────────┘  │    │  ┌──────────────┐
   ┌──────────┐  ┃ ┃   ┌──────────┐               ┌──────────┐  ├────┼──│ Filesystem   │
   │ Support  │━━┛ ┗━━▶│ Slack    │               │ Support  │──┤    │  └──────────────┘
   │ agent    │━━━━━━━▶│ Payments │               │ agent    │──┘    └──│ Slack server │
   └──────────┘        └──────────┘               └──────────┘          └──────────────┘
     M = 4               N = 5                       4 + 5 = 9 implementations,
     4 × 5 = 20 integrations to build & maintain     each written once and reused
```

Two analogies make the shape stick. The first is the one the ecosystem uses: MCP is **"USB-C for AI."** Before USB-C, every device had its own connector and you carried a drawer of adapters; USB-C defined one physical-and-electrical contract, and now any compliant peripheral plugs into any compliant port. MCP is that contract for connecting models to tools and data — one standard plug instead of a drawer of integrations.

The second analogy is the direct technical precedent, and it is worth more than the USB-C image because it already proved the math works. The **Language Server Protocol (LSP)**, introduced by Microsoft in 2016, faced the identical explosion in a different domain: **E editors × L languages**, each editor needing a hand-built integration for each language's autocomplete, go-to-definition, and diagnostics. LSP defined a JSON-RPC protocol between editors and "language servers" so that a language is implemented once as a server and every LSP-speaking editor gets it. E × L became E + L, the ecosystem exploded, and editors stopped competing on "which languages do you support." MCP is LSP's idea applied to agents and tools — and, like LSP, it is built on JSON-RPC.

---

</details>

## 2. MCP 究竟标准化了什么

对名词要抠准，因为最常见的误解是把 MCP 当成一个框架或一个 agent。两者都不是。**MCP 是一种 wire protocol**——即对两个程序之间交换的 JSON 消息的一份规范——构建在 **JSON-RPC 2.0** 之上。它不运行你的 agent loop，不选择调用哪个工具，也不附带 runtime。它只标准化一件事：**一个 host 如何在一个 transport 之上、按确定的生命周期，发现并调用一个 server 暴露的 tools、resources 和 prompts**。

具体来说，协议把消息的形态钉死。host 一侧的 client 列出 server 提供了什么，并用 JSON-RPC 请求调用它；server 用 JSON-RPC 结果作答。工具在 wire 上的发现与调用如下所示：

```json
// client → server: discover what the server exposes
{ "jsonrpc": "2.0", "id": 1, "method": "tools/list" }

// server → client: the catalog (name, human description, JSON-Schema for inputs)
{ "jsonrpc": "2.0", "id": 1, "result": { "tools": [
  { "name": "get_issue",
    "description": "Fetch a GitHub issue by number.",
    "inputSchema": { "type": "object",
      "properties": { "repo": {"type":"string"}, "number": {"type":"integer"} },
      "required": ["repo","number"] } } ] } }

// client → server: invoke it
{ "jsonrpc": "2.0", "id": 2, "method": "tools/call",
  "params": { "name": "get_issue", "arguments": { "repo": "anthropics/sdk", "number": 42 } } }
```

*只*标准化 wire 的回报是**解耦**：工具作者和 host 作者永远不需要协调。写 GitHub server 的人从没听说过你的 agent；你也从没读过他们的代码。你们只需在 `tools/list` 和 `tools/call` 以及夹在中间的 JSON-Schema 上达成一致，这就是全部契约。这才是 “M + N” 真正带来的东西——不只是更少的代码行数，而是去掉了当初让 M × N 矩阵变得昂贵的人力协调。

协议*不*做的事，对本课程余下部分同样关键。它不决定*是否*调用 `get_issue`——那是模型在 host 内部的职责。它不实现 GitHub 调用——那是 server 作者的职责。MCP 是夹在中间的契约，而正是把它收得这么窄，才让如此多彼此独立的参与方能够这么快采用它。

---

## 3. 采用与势头

一个协议只有赢了才值得被当作标准，而 MCP 的发展轨迹就是论据。时间线：

| 日期 | 里程碑 |
|---|---|
| **2024 年 11 月** | Anthropic 开源 MCP；首个规范、stdio transport、核心原语。 |
| **2025-03-26** | 加入 **Streamable HTTP** transport 和一个**授权**框架（OAuth）；弃用旧的 HTTP+SSE transport。 |
| **2025-06-18** | 规范修订，细化协议面（elicitation、结构化工具输出、安全指引）。 |
| **2025-11-25** | **最新稳定修订版**——今天生产工作要锁定到的版本。 |
| **2026-07-28** | **Release candidate**：无状态核心、**MCP Apps**（server 渲染的 UI）、面向长时任务的 **Tasks** 扩展，以及与 OAuth 更紧密的对齐。 |

在这段时间里，协议从“某个实验室发布的有趣玩意儿”变成了行业默认。**OpenAI、Google DeepMind 和 Microsoft** 都在 2025 年采用了 MCP——当另外三家最大的模型与平台厂商支持一个竞争对手的协议时，这不是客气，而是承认共享标准比专有标准更有价值。规模数字与采用情况同步：`mcp` PyPI 包现在**每月约 97M 下载量**，官方注册表收录的 server 已超过 **2,000 个**。

背后的机制是**网络效应**，这也是为什么一个协议一旦有足够多的参与方使用它就会赢。每一个新的支持 MCP 的 host 都让所有已有的 server 更有价值（多了一个运行的地方），而每一个新 server 都让所有 host 更有价值（免费多了一项能力）。价值在 M + N 的两侧同时复利。定制集成只帮到一对 host–tool；一个 MCP server 一次性帮到所有现在和未来的 host。过了临界点之后——MCP 在 2025 年跨过了它——*不*说这个协议才是昂贵的选择，因为你放弃了整个生态。“它能说 MCP 吗？”成了一个真实的采购问题，而这是一个协议已经胜出最可靠的标志。

---


<details>
<summary>English original</summary>

**2. What MCP actually standardizes**

Be precise about the noun, because the most common misconception is that MCP is a framework or an agent. It is neither. **MCP is a wire protocol** — a specification of the JSON messages two programs exchange — built on **JSON-RPC 2.0**. It does not run your agent loop, does not choose which tool to call, does not ship a runtime. It standardizes exactly one thing: **how a host discovers and invokes the tools, resources, and prompts a server exposes**, over a transport, with a defined lifecycle.

Concretely, the protocol pins down the message shapes. A host's client lists what a server offers and calls into it with JSON-RPC requests; the server answers with JSON-RPC results. Discovery and invocation of a tool look like this on the wire:

```json
// client → server: discover what the server exposes
{ "jsonrpc": "2.0", "id": 1, "method": "tools/list" }

// server → client: the catalog (name, human description, JSON-Schema for inputs)
{ "jsonrpc": "2.0", "id": 1, "result": { "tools": [
  { "name": "get_issue",
    "description": "Fetch a GitHub issue by number.",
    "inputSchema": { "type": "object",
      "properties": { "repo": {"type":"string"}, "number": {"type":"integer"} },
      "required": ["repo","number"] } } ] } }

// client → server: invoke it
{ "jsonrpc": "2.0", "id": 2, "method": "tools/call",
  "params": { "name": "get_issue", "arguments": { "repo": "anthropics/sdk", "number": 42 } } }
```

The payoff of standardizing *only* the wire is **decoupling**: the tool author and the host author never coordinate. The person who wrote the GitHub server has never heard of your agent; you have never read their code. You agree on `tools/list` and `tools/call` and the JSON-Schema in between, and that is the entire contract. This is what "M + N" actually buys you — not just fewer lines of code, but the removal of the human coordination that made the M × N matrix expensive in the first place.

What the protocol does *not* do is equally load-bearing for the rest of this course. It does not decide *whether* to call `get_issue` — that is the model's job, inside the host. It does not implement the GitHub call — that is the server author's job. MCP is the contract in the middle, and keeping it that narrow is exactly why so many independent parties could adopt it so fast.

---

**3. Adoption & momentum**

A protocol is only worth standardizing on if it wins, and MCP's trajectory is the argument. The timeline:

| Date | Milestone |
|---|---|
| **Nov 2024** | Anthropic open-sources MCP; first spec, stdio transport, the core primitives. |
| **2025-03-26** | Adds **Streamable HTTP** transport and an **authorization** framework (OAuth); deprecates the legacy HTTP+SSE transport. |
| **2025-06-18** | Spec revision refining the protocol surface (elicitation, structured tool output, security guidance). |
| **2025-11-25** | **Latest stable revision** — what you pin production work to today. |
| **2026-07-28** | **Release candidate**: a stateless core, **MCP Apps** (server-rendered UI), a **Tasks** extension for long-running work, and tighter OAuth alignment. |

Across that window the protocol went from "interesting thing one lab published" to industry default. **OpenAI, Google DeepMind, and Microsoft** all adopted MCP through 2025 — when the three other largest model and platform vendors back a competitor's protocol, that is not politeness, it is recognition that a shared standard is worth more than a proprietary one. The scale numbers track the adoption: the `mcp` PyPI package now does **~97M downloads a month**, and the official registry passed **2,000 servers**.

The mechanism behind this is **network effects**, and it is why a protocol wins once enough parties speak it. Every new MCP-capable host makes every existing server more valuable (one more place it runs), and every new server makes every host more valuable (one more capability it gains for free). Value compounds on both sides of the M + N. A bespoke integration helps exactly one host–tool pair; an MCP server helps every current and future host at once. Past a tipping point — which MCP crossed in 2025 — *not* speaking the protocol is the expensive choice, because you forfeit the entire ecosystem. "Can it speak MCP?" became a real procurement question, and that is the surest sign a protocol has won.

---

</details>

## 4. MCP vs 手工接线的函数调用

你已经从 [Tool Use & Function Calling](../01-讲座/Lecture-08.md)：你定义一个工具，给出名称、描述和 JSON-Schema；模型发出结构化调用；你的代码执行它并返回结果。MCP 并不取代那个循环 —— 它**位于其之上**。MCP 工具以普通函数调用工具的形式暴露给模型；区别在于*工具定义来自哪里、谁能复用它*。用普通函数调用时，定义硬编码在你的应用里。用 MCP 时，它在 runtime 从任何 host 都能连接的 server 中发现。

| 维度 | 手工接线的函数调用 | MCP |
|---|---|---|
| **工具存放位置** | 硬编码在某个 host 的源码里 | 在独立 server 中，任何 host 都可复用 |
| **跨 host 复用** | 无 —— 每个应用重新实现 | 写一次，每个 host 都能连接 |
| **发现** | 静态，构建时固化 | 动态 —— runtime 时`tools/list` |
| **团队耦合** | 工具作者*就是* host 作者 | 工具作者与 host 作者从不需要协调 |
| **更换 host** | 重写所有集成 | 把新 host 指向同一批 server |
| **每次调用开销** | 直接的进程内函数调用 | 增加一次协议跳转（transport 上的 JSON-RPC） |
| **反向能力** | 不可用 | Sampling、Roots、Elicitation（server → client） |

**MCP 胜出的场景：** 你希望同一个工具能在多个 host 上使用；你想从现有 server 的生态中取用，而不是什么都自己造；你需要**动态发现**（工具集变化时无需重新部署 host）；工具团队与 host 团队是不同的人，本不该需要互相协调；或者你预期会**更换 host** 并保留你的集成。

**普通函数调用够用的场景：** 单个应用、只有少量私有工具、没有复用需求，工具作者与应用作者是同一个人 —— 协议跳转和 server 脚手架什么也换不来。还有**对延迟敏感的内层循环**，此时 transport 上多出的 JSON-RPC 往返是你承受不起的开销：进程内函数调用比任何协议都快，如果一个工具运行在你循环中最热的部分且从不共享，就直接调用它。诚实的说法是：MCP 用少量每次调用开销和一些搭建成本，换取复用、解耦和生态接入；在这些价值显现的门槛之下，更简单的工具胜出。

---

## 5. 心智模型预览

本课程余下的内容都系于一张图和六个名词。架构包含三个角色。**Host** 是 LLM 应用 —— Claude Desktop、Claude Code、Cursor，或你内部的 agent。host 运行一个或多个 **Client**，每个 client 持有**一条到恰好一个 Server 的 1:1 连接**。server 封装一项能力 —— GitHub、Postgres、你的文件系统 —— 并通过协议将其暴露出来。（[Lecture 02](Lecture-02.md) 详细介绍了这一点以及 `initialize → operate → shutdown` 生命周期。）

```text
   ┌──────────────── Host (LLM app: Claude Code, Cursor, …) ─────────────┐
   │   Client A ──────1:1──────▶ Server (GitHub)                         │
   │   Client B ──────1:1──────▶ Server (Postgres)                       │
   │   Client C ──────1:1──────▶ Server (filesystem)                     │
   └────────────────────────────────────────────────────────────────────┘
```

一条连接所承载的能力就是**六个原语**，按方向划分。三个是 **server → client**（server 向 host 提供的东西）：**Tools** 是模型控制的操作，由模型选择调用；**Resources** 是应用控制的、以 URI 寻址的数据，由 host 拉取作为上下文；**Prompts** 是用户控制的模板，由用户主动选择。（[Lecture 03](Lecture-03.md) 讲述这种三分法。）另外三个走*相反*方向，即让 MCP 具备双向性的 **server → client requests**：**Sampling** 让 server 请 host 代其运行一次 LLM 补全；**Roots** 是 client 授予 server 在其中操作的文件系统或 URI 边界；**Elicitation** 让 server 在任务中途向用户索取结构化输入。（[Lecture 04](Lecture-04.md) 讲述这些反向原语。）记住每个原语的方向和控制方 —— 那张表是后续一切内容的脊梁。

---


<details>
<summary>English original</summary>

**4. MCP vs hand-wired function calling**

You already know function calling from [Tool Use & Function Calling](../01-讲座/Lecture-08.md): you define a tool with a name, a description, and a JSON-Schema; the model emits a structured call; your code runs it and returns the result. MCP does not replace that loop — it sits **on top of it**. An MCP tool is surfaced to the model as an ordinary function-calling tool; the difference is *where the tool definition comes from and who can reuse it*. With plain function calling the definition is hardcoded in your app. With MCP it is discovered at runtime from a server that any host can connect to.

| Dimension | Hand-wired function calling | MCP |
|---|---|---|
| **Where tools live** | Hardcoded in one host's source | In standalone servers, reusable by any host |
| **Reuse across hosts** | None — re-implement per app | Write once, every host connects |
| **Discovery** | Static, baked in at build time | Dynamic — `tools/list` at runtime |
| **Team coupling** | Tool author *is* host author | Tool and host authors never coordinate |
| **Swapping the host** | Rewrite all integrations | Point a new host at the same servers |
| **Per-call overhead** | Direct in-process function call | Adds a protocol hop (JSON-RPC over a transport) |
| **Reverse capabilities** | Not available | Sampling, Roots, Elicitation (server → client) |

**When MCP wins:** you want the same tool usable across multiple hosts; you want to pull from the ecosystem of existing servers instead of building everything; you need **dynamic discovery** (the tool set changes without redeploying the host); the tool team and the host team are different people who should not have to coordinate; or you expect to **swap hosts** and keep your integrations.

**When plain function calling is fine:** a single application with a handful of private tools, no reuse story, where the tool author and app author are the same person — the protocol hop and the server scaffolding buy you nothing. And **latency-critical inner loops**, where the extra JSON-RPC round trip over a transport is overhead you cannot afford: an in-process function call is faster than any protocol, and if a tool runs in the hottest part of your loop and is never shared, call it directly. The honest framing is that MCP trades a little per-call overhead and some setup for reuse, decoupling, and ecosystem access; below the threshold where those matter, the simpler tool wins.

---

**5. The mental model preview**

The rest of this course hangs on one diagram and six nouns. The architecture is three roles. A **Host** is the LLM application — Claude Desktop, Claude Code, Cursor, your internal agent. The host runs one or more **Clients**, and each client holds a **1:1 connection to exactly one Server**. A server wraps a capability — GitHub, Postgres, your filesystem — and exposes it through the protocol. ([Lecture 02](Lecture-02.md) details this and the `initialize → operate → shutdown` lifecycle.)

```text
   ┌──────────────── Host (LLM app: Claude Code, Cursor, …) ─────────────┐
   │   Client A ──────1:1──────▶ Server (GitHub)                         │
   │   Client B ──────1:1──────▶ Server (Postgres)                       │
   │   Client C ──────1:1──────▶ Server (filesystem)                     │
   └────────────────────────────────────────────────────────────────────┘
```

The capabilities a connection carries are the **six primitives**, split by direction. Three go **server → client** (what a server offers the host): **Tools** are model-controlled actions the model chooses to invoke; **Resources** are app-controlled, URI-addressed data the host pulls in for context; **Prompts** are user-controlled templates a user deliberately selects. ([Lecture 03](Lecture-03.md) covers this trichotomy.) Three run the *other* way, **server → client requests** that make MCP bidirectional: **Sampling** lets a server ask the host to run an LLM completion on its behalf; **Roots** are the filesystem or URI boundaries the client grants a server to operate within; **Elicitation** lets a server ask the user for structured input mid-task. ([Lecture 04](Lecture-04.md) covers these reverse primitives.) Hold onto the directions and the controlling actor for each — that table is the spine of everything that follows.

---

</details>

## 当前版本

本讲座内容截至 **2026 年 6 月**，并锁定 **2025-11-25** 稳定版 MCP 规范——这是当前生产工作应针对的修订版。协议仍基于 JSON-RPC 2.0，采用 host/client/server 架构和上文所述的六个原语；采用率和规模数据（OpenAI、Google DeepMind 和 Microsoft 的跨厂商采用；每月约 97M `mcp` 下载量；2,000+ 注册服务器）反映了 2026 年初的报道。关注 **2026-07-28 发布候选版**，它引入了无状态核心、MCP Apps（服务器渲染 UI）、用于长时间运行工作的 Tasks 扩展，以及更紧密的 OAuth 对齐——这些都不会改变 M×N 论证或此处的思维模型，但会影响传输和部署讲座。将 SDK 接口视为快照，并对照已安装版本进行验证；上述协议事实是稳定的基础。


<details>
<summary>English original</summary>

**Current as of**

This lecture is current as of **June 2026** and pins to the **2025-11-25** stable MCP specification — the revision to target for production work today. The protocol remains built on JSON-RPC 2.0 with the host/client/server architecture and six primitives described above; the adoption and scale figures (cross-vendor adoption by OpenAI, Google DeepMind, and Microsoft; ~97M monthly `mcp` downloads; 2,000+ registry servers) reflect early-2026 reporting. Watch the **2026-07-28 release candidate**, which introduces a stateless core, MCP Apps (server-rendered UI), a Tasks extension for long-running work, and tighter OAuth alignment — none of which changes the M×N argument or the mental model here, but which will shape the transport and deployment lectures. Treat SDK surfaces as snapshots and verify against your installed version; the protocol facts above are the stable ground.

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/MCP for AI Agents/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/MCP%20for%20AI%20Agents/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
