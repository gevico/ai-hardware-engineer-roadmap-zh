---
title: 讲座 31 - OpenClaw 案例研究：为什么真实 agent 需要网关
description: 讲座 31 - OpenClaw 案例研究：为什么真实 agent 需要网关
---

# 讲座 31 - OpenClaw 案例研究：为什么真实 agent 需要网关

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [讲座 30](Lecture-30.md) | **下一讲：** [讲座 32](Lecture-32.md)

---

## 为什么有这一讲

许多 agent 教程仍在教同一个简单模式：

```text
user -> prompt -> model -> answer
```

这对学习很有用，但太小了，无法解释现代 agent 产品实际如何工作。

真实 agent 系统需要处理：

- 许多用户
- 许多渠道
- 长期存活的会话
- 工具
- 记忆
- 设备客户端
- 后台工作
- 健康检查与运维
- 路由与身份

这正是 **OpenClaw** 作为一个有用案例研究的地方。

OpenClaw 不只是“一个调用大语言模型的应用”。它是面向**持久化 agent** 的**控制平面**。

本讲用 OpenClaw 讲一个更贴近现实的系统模型：

> agent 不只是一次模型调用。它是一个长期运行的服务，具备路由、状态、安全和运维。

---

## 学习目标

本讲结束时，你将能够：

1. 解释为什么真实的 agent 产品往往需要网关或控制平面。
2. 用简单的语言描述 OpenClaw 的高层架构。
3. 区分渠道、客户端、节点和 agent。
4. 解释一次性推理调用与 agent 循环之间的区别。
5. 理解为什么会话串行化很重要。
6. 解释为什么“一个长期存活的网关”不同于“一个模型端点”。

---

## 1. 简单的心智模型

理解 OpenClaw 最简单的方式是这样：

```text
OpenClaw Gateway = a central station for agent traffic
```

消息和控制请求从许多地方到达：

- Telegram
- WhatsApp
- Slack
- Discord
- Web UI
- CLI
- 移动设备

网关是这样一个地方：

- 接收消息
- 决定哪个 agent 应当处理它
- 加载正确的会话
- 运行 agent 循环
- 流式传输工具和助手事件
- 存储状态
- 通过正确的渠道把回复发回

所以不是：

```text
web app -> model API
```

而是：

```text
channel/client/node
  -> gateway
  -> session + routing
  -> agent loop
  -> tools + memory
  -> reply back to the same surface
```

对于生产环境的 agent 来说，这是好得多的心智模型。

---

## 2. 网关为什么会存在

人们第一次看到网关风格的系统时，往往会问：

> 为什么不让每个客户端直接调用模型？

因为一旦你需要**共享行为**，直接调用就会崩溃。

网关让你有一个统一的地方来处理：

- 会话归属
- 身份校验
- 渠道集成
- 工具策略
- 模型路由
- 日志
- 流式事件
- 记忆访问
- 健康检查
- 运维

没有网关，每个客户端都必须重新实现这些关注点。

这会导致：

- 重复的逻辑
- 不一致的安全规则
- 不匹配的会话状态
- 调试困难
- 可审计性差

网关通过**集中化 agent runtime** 来解决这一点。

---

## 3. 用大白话讲 OpenClaw 架构

根据本地的 OpenClaw 文档，核心设计是：

- 一个长期存活的 **Gateway**
- 许多**渠道**
- 许多**客户端**
- 可选的**节点**
- 一个或多个 agent
- 每个 agent 都有自己的工作区和会话存储

### 网关

网关是**长期存活的守护进程**。

它：

- 拥有消息界面
- 暴露 WebSocket 和 HTTP API
- 校验请求
- 发出事件
- 管理会话
- 运行 agent 循环

### 客户端

客户端是面向操作者的工具，例如：

- CLI
- Web 管理界面
- macOS 配套应用

它们不拥有 agent 状态。它们连接到网关。

### 节点

节点是挂接的设备，例如：

- iOS 节点
- Android 节点
- 无头设备节点

它们连接到同一个网关，但把自己标识为具备能力的设备。

### 渠道

渠道是消息界面，例如：

- Telegram
- WhatsApp
- Slack
- Discord
- Signal
- WebChat

渠道负责把消息送进送出。它们与 agent 不是一回事。

### Agents

**agent** 是处理一段对话或工作流的“大脑”。

在 OpenClaw 中，一个网关可以承载：

- 一个默认 agent
- 或者许多相互隔离的 agent 并排运行

这是一个重要的生产理念：

> 一个 runtime 进程可以承载许多相互隔离的 agent 人格和工作区

---


<details>
<summary>English original</summary>

**Lecture 31 - OpenClaw Case Study: Why Real Agents Need a Gateway**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 30](Lecture-30.md) | **Next:** [Lecture 32](Lecture-32.md)

---

**Why this lecture exists**

Many agent tutorials still teach the same simple pattern:

```text
user -> prompt -> model -> answer
```

That is useful for learning, but it is too small to explain how modern agent products actually work.

Real agent systems need to handle:

- many users
- many channels
- long-lived sessions
- tools
- memory
- device clients
- background work
- health and operations
- routing and identity

That is where **OpenClaw** becomes a useful case study.

OpenClaw is not just "an app that calls an LLM." It is a **control plane** for **persistent agents**.

This lecture uses OpenClaw to teach a more realistic system model:

> an agent is not only a model call. It is a long-running service with routing, state, safety, and operations.

---

**Learning objectives**

By the end of this lecture you will be able to:

1. Explain why a real agent product often needs a gateway or control plane.
2. Describe OpenClaw's high-level architecture in simple terms.
3. Separate channels, clients, nodes, and agents.
4. Explain the difference between a one-shot inference call and an agent loop.
5. Understand why session serialization matters.
6. Explain why "one long-lived gateway" is different from "one model endpoint."

---

**1. The simple mental model**

The easiest way to understand OpenClaw is this:

```text
OpenClaw Gateway = a central station for agent traffic
```

Messages and control requests arrive from many places:

- Telegram
- WhatsApp
- Slack
- Discord
- Web UI
- CLI
- mobile devices

The gateway is the place that:

- receives the message
- decides which agent should handle it
- loads the right session
- runs the agent loop
- streams tool and assistant events
- stores state
- sends the reply back through the correct channel

So instead of:

```text
web app -> model API
```

you get:

```text
channel/client/node
  -> gateway
  -> session + routing
  -> agent loop
  -> tools + memory
  -> reply back to the same surface
```

That is a much better mental model for production agents.

---

**2. Why a gateway exists at all**

When people first see a gateway-style system, they often ask:

> Why not let every client call the model directly?

Because direct calls break down once you need **shared behavior**.

A gateway gives you one place for:

- session ownership
- identity checks
- channel integrations
- tool policy
- model routing
- logging
- streaming events
- memory access
- health checks
- operations

Without a gateway, each client must reimplement these concerns.

That leads to:

- duplicated logic
- inconsistent safety rules
- mismatched session state
- difficult debugging
- poor auditability

The gateway solves this by **centralizing the agent runtime**.

---

**3. The OpenClaw architecture in plain English**

Based on the local OpenClaw docs, the core design is:

- one long-lived **Gateway**
- many **channels**
- many **clients**
- optional **nodes**
- one or more **agents**
- each agent with its own workspace and session store

**Gateway**

The gateway is the **long-lived daemon process**.

It:

- owns messaging surfaces
- exposes WebSocket and HTTP APIs
- validates requests
- emits events
- manages sessions
- runs the agent loop

**Clients**

Clients are operator-facing tools such as:

- CLI
- web admin UI
- macOS companion app

They do not own the agent state. They connect to the gateway.

**Nodes**

Nodes are attached devices, such as:

- iOS node
- Android node
- headless device node

They connect to the same gateway but identify themselves as devices with capabilities.

**Channels**

Channels are message surfaces like:

- Telegram
- WhatsApp
- Slack
- Discord
- Signal
- WebChat

Channels deliver messages in and out. They are not the same as agents.

**Agents**

An **agent** is the "brain" that handles a conversation or workflow.

In OpenClaw, one gateway can host:

- one default agent
- or many isolated agents side by side

That is an important production idea:

> one runtime process can host many isolated agent personalities and workspaces

---

</details>

## 4. 最重要的架构转变

OpenClaw 带来的最大教训是：

> 一个严肃的 agent 系统不只是模型推理。它是消息布线加状态加执行加运维。

这听起来很抽象，所以直接对比这两个世界。

| 简单的 demo 应用 | 真实的 agent 系统 |
|---|---|
| 一个聊天框 | 多个渠道与客户端 |
| 一个 prompt | 多个 prompt、system 文件与上下文来源 |
| 一次请求-响应 | 长生命周期的会话 |
| 无状态后端 | 持久化状态与记忆 |
| 无工具编排 | 工具调用与设备能力 |
| 无会话归属 | 会话布线与隔离 |
| 仅有模型调用日志 | 完整的 runtime 事件与运维可见性 |

这就是研究真实系统的意义所在。

如果只研究 notebook demo，你的心智模型会一直太小。

---

## 5. OpenClaw 的 agent loop

OpenClaw 明确记录了 **agent loop**。

其高层形态是：

```text
intake
  -> context assembly
  -> model inference
  -> tool execution
  -> streaming
  -> persistence
```

这比“调用模型并打印响应”是一个好得多的教学模型。

### 为什么这很重要

每一步都有各自不同的工程关注点：

| 步骤 | 工程关注点 |
|---|---|
| 接入 | 校验请求、为会话布线、识别发送方 |
| 上下文组装 | 构建 prompt、加载 workspace 文件、记忆与工具 |
| 推理 | 模型选择、成本、延迟、超时 |
| 工具执行 | 策略、安全、重试、串行化 |
| 流式 | 用户体验与可观测性 |
| 持久化 | 会话历史、重放、恢复、审计 |

这意味着 agent loop 不只有模型。

它是从入站事件到持久化结果的完整 runtime 路径。

---

## 6. 为什么会话串行化很重要

OpenClaw 的文档强调，各次 run 是**按会话串行化**的。

这意味着同一个会话不应有多个相互重叠的 agent run 同时修改它。

为什么？

因为 agent 中的并发 bug 很隐蔽。

设想这样一个出错的场景：

```text
message A arrives
message B arrives one second later
both runs share the same session
both read old context
both call tools
both write memory
both reply
```

于是你会得到：

- 重复的工具调用
- 混杂的回复
- 不一致的状态
- 损坏的摘要
- 混乱的记忆

会话串行化可以避免这些。

这是一条关键的生产经验：

> agent 的正确性往往取决于控制并发，而不只是改进 prompt

---

## 7. 一个网关，多个入口

OpenClaw 之所以有用，是因为它展示了**一个 agent runtime** 如何支撑**多种通信入口**。

单个 agent 可以通过以下方式访问：

- Telegram
- Slack
- WebChat
- 移动节点
- CLI

这意味着“the agent”并不等同于“the UI”。

这是学生需要完成的最重要的设计认知升级之一。

糟糕的初学者心智模型：

> agent 就是聊天应用

更好的生产级心智模型：

> agent 是一项服务，聊天应用只是连到它的入口

这会带来更好的架构决策：

- agent 负责逻辑
- 渠道负责传输
- 客户端负责交互方式
- 网关负责编排

---

## 8. 示例：从 Telegram 消息到最终回复

下面是一个简化的 OpenClaw 风格流程：

```text
Telegram user sends a message
  -> Telegram channel adapter receives it
  -> gateway validates the inbound event
  -> routing logic picks an agent
  -> session key is resolved
  -> session state is loaded
  -> workspace files and prompt context are assembled
  -> model runs
  -> tool is called if needed
  -> assistant text streams
  -> session transcript is updated
  -> reply goes back to Telegram
```

这里重要的不是具体的传输方式。

真正重要的是：

- 渠道选择
- agent 选择
- 会话选择
- 工具执行
- 持久化

都是显式的 runtime 步骤。

这才是真正的 agent 工程。

---

## 9. 为什么 OpenClaw 是一个出色的教学示例

OpenClaw 对本路线图很有用，因为它**不局限于某一种狭窄的模式**。

它结合了：

- 消息渠道
- 持久化会话
- 多 agent 布线
- 设备节点
- 技能与插件
- 网关运维
- 安全边界
- 长生命周期的 local-first 控制

这使它成为一个高信噪比的示例，展示了 2026 年的 agent 产品实际是什么样子。

它教给学生的是：

- agent 可以是一个系统，而不只是一个函数
- local-first 设计很重要
- 渠道就是产品入口
- 会话是一等状态
- runtime 的安全与运维是核心特性

---


<details>
<summary>English original</summary>

**4. The most important architecture shift**

The biggest lesson from OpenClaw is this:

> A serious agent system is not just model inference. It is message routing plus state plus execution plus operations.

That sounds abstract, so compare the two worlds directly.

| Simple demo app | Real agent system |
|---|---|
| one chat box | many channels and clients |
| one prompt | multiple prompts, system files, and context sources |
| one request-response | long-lived sessions |
| stateless backend | persistent state and memory |
| no tool orchestration | tool calls and device capabilities |
| no session ownership | session routing and isolation |
| model call logs only | full runtime events and operational visibility |

This is why studying real systems matters.

If you only study notebook demos, your mental model stays too small.

---

**5. The OpenClaw agent loop**

OpenClaw documents the **agent loop** explicitly.

The high-level shape is:

```text
intake
  -> context assembly
  -> model inference
  -> tool execution
  -> streaming
  -> persistence
```

This is a much better teaching model than "call the model and print the response."

**Why this matters**

Each step has a distinct engineering concern:

| Step | Engineering concern |
|---|---|
| intake | validate request, route session, identify sender |
| context assembly | build prompts, load workspace files, memory, and tools |
| inference | model selection, cost, latency, timeouts |
| tool execution | policy, safety, retries, serialization |
| streaming | user experience and observability |
| persistence | session history, replay, recovery, audit |

That means the agent loop is not only about the model.

It is the full runtime path from inbound event to durable result.

---

**6. Why session serialization matters**

OpenClaw's docs emphasize that runs are **serialized per session**.

That means one session should not have many overlapping agent runs mutating it at the same time.

Why?

Because concurrency bugs in agents are subtle.

Imagine this broken case:

```text
message A arrives
message B arrives one second later
both runs share the same session
both read old context
both call tools
both write memory
both reply
```

Now you get:

- duplicated tool calls
- mixed replies
- inconsistent state
- broken summaries
- confusing memory

Session serialization avoids that.

This is a key production lesson:

> agent correctness often depends on controlling concurrency, not just improving prompts

---

**7. One gateway, many surfaces**

OpenClaw is useful because it shows how **one agent runtime** can support **many communication surfaces**.

A single agent can be reachable through:

- Telegram
- Slack
- WebChat
- mobile node
- CLI

That means "the agent" is not the same thing as "the UI."

This is one of the most important design upgrades students need to make.

Bad beginner mental model:

> the agent is the chat app

Better production mental model:

> the agent is a service, and chat apps are only surfaces connected to it

This leads to better architecture decisions:

- the agent owns logic
- channels own transport
- clients own interaction style
- the gateway owns orchestration

---

**8. Example: from Telegram message to final reply**

Here is a simplified OpenClaw-style flow:

```text
Telegram user sends a message
  -> Telegram channel adapter receives it
  -> gateway validates the inbound event
  -> routing logic picks an agent
  -> session key is resolved
  -> session state is loaded
  -> workspace files and prompt context are assembled
  -> model runs
  -> tool is called if needed
  -> assistant text streams
  -> session transcript is updated
  -> reply goes back to Telegram
```

What is important here is not the specific transport.

What matters is that:

- channel selection
- agent selection
- session selection
- tool execution
- persistence

are all explicit runtime steps.

That is real agent engineering.

---

**9. Why OpenClaw is a strong teaching example**

OpenClaw is useful for this roadmap because it is **not limited to one narrow pattern**.

It combines:

- message channels
- persistent sessions
- multi-agent routing
- device nodes
- skills and plugins
- gateway operations
- safety boundaries
- long-lived local-first control

That makes it a high-signal example of what a 2026 agent product actually looks like.

It teaches students that:

- an agent can be a system, not just a function
- local-first designs matter
- channels are product surfaces
- sessions are first-class state
- runtime safety and operations are core features

---

</details>

## 10. 一个最小的 OpenClaw 风格设计练习

设想你正在用 OpenClaw 架构模式构建一个家庭 AI 助手。

它应当：

- 回答聊天问题
- 接收 Telegram 消息
- 接收来自移动节点的语音留言
- 搜索本地笔记
- 控制几台已授权的设备

你的架构草图应当包含：

| Part | Your decision |
|---|---|
| Gateway | Jetson 上一个常开进程 |
| Agent | 一个默认的个人助理 agent |
| Channels | Telegram + WebChat |
| Nodes | 一个手机节点 |
| Session policy | 私聊按发送者隔离 |
| Tools | 笔记搜索、日程查询、设备控制 |
| Safety | 配对、工具策略、审计日志 |

这已经给了你一个远强于下面这句话的设计：

> "我有一个带 prompt 的 chatbot。"

---

## 11. 需要记住什么

主要教训很简单：

> 真正的 agent 产品需要一种 runtime 形态。

OpenClaw 给出了这种形态的一个有力示例：

- 长生命周期的网关
- 多个入口
- 路由后的会话
- 显式的 agent 循环
- 持久化状态
- 运行可见性

如果你理解了这个模型，就比只懂 prompting 离构建严肃的 agent 近得多。

---

## Key takeaways

- 网关之所以存在，是因为真正的 agent 需要对布线、会话、工具和运维进行共享控制。
- OpenClaw 是一个有用的案例研究，因为它把 agent 当作长生命周期的服务，而不是一次性的 prompt 调用。
- Channel、client、node 和 agent 是系统中不同的角色。
- agent 循环是一条完整的 runtime 路径：接入、上下文、推理、工具、流式输出、持久化。
- 会话串行化是生产要求，不是可选的优化。
- 严肃的 agent 产品是控制平面加 runtime，而不只是一个模型端点。

---

## References

- 案例研究源码仓库：[OpenClaw](https://github.com/openclaw/openclaw)
- 实践者参考：[The OpenClaw Book](https://openclawconsultant.com/openclaw-book/)
- OpenClaw 概念：
  - `docs/concepts/architecture.md`
  - `docs/concepts/agent-loop.md`
  - `docs/concepts/features.md`
  - `docs/gateway/index.md`

---

*下一讲：[Lecture 32](Lecture-32.md)*


<details>
<summary>English original</summary>

**10. A minimal OpenClaw-style design exercise**

Imagine you are building a home AI assistant using the OpenClaw architecture pattern.

It should:

- answer chat questions
- receive Telegram messages
- receive voice notes from a mobile node
- search local notes
- control a few approved devices

Your architecture sketch should include:

| Part | Your decision |
|---|---|
| Gateway | one always-on process on Jetson |
| Agent | one default personal assistant agent |
| Channels | Telegram + WebChat |
| Nodes | one phone node |
| Session policy | DMs isolated per sender |
| Tools | note search, calendar lookup, device control |
| Safety | pairing, tool policy, audit logs |

This already gives you a much stronger design than:

> "I have a chatbot with a prompt."

---

**11. What to remember**

The main lesson is simple:

> A real agent product needs a runtime shape.

OpenClaw gives you a strong example of that shape:

- long-lived gateway
- many surfaces
- routed sessions
- explicit agent loop
- persistent state
- operational visibility

If you understand this model, you are much closer to building serious agents than if you only know prompting.

---

**Key takeaways**

- A gateway exists because real agents need shared control over routing, sessions, tools, and operations.
- OpenClaw is a useful case study because it treats the agent as a long-lived service, not a one-shot prompt call.
- Channels, clients, nodes, and agents are different roles in the system.
- The agent loop is a full runtime path: intake, context, inference, tools, streaming, persistence.
- Session serialization is a production requirement, not an optional optimization.
- A serious agent product is a control plane plus runtime, not only a model endpoint.

---

**References**

- Case-study source repo: [OpenClaw](https://github.com/openclaw/openclaw)
- Practitioner reference: [The OpenClaw Book](https://openclawconsultant.com/openclaw-book/)
- OpenClaw concepts:
  - `docs/concepts/architecture.md`
  - `docs/concepts/agent-loop.md`
  - `docs/concepts/features.md`
  - `docs/gateway/index.md`

---

*Next: [Lecture 32](Lecture-32.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-31.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-31.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
