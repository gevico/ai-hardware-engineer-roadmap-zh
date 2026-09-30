---
title: Lecture 32 - OpenClaw 案例研究：Channel、Routing 与 Session 设计
description: Lecture 32 - OpenClaw 案例研究：Channel、Routing 与 Session 设计
---

# Lecture 32 - OpenClaw 案例研究：Channel、Routing 与 Session 设计

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 31](Lecture-31.md) | **Next:** [Lecture 33](Lecture-33.md)

---

## 本讲存在的意义

一旦超出单个聊天窗口，agent 设计就变成一个**routing 问题**。

你必须决定：

- 哪条入站消息交给哪个 agent
- 哪个 session 承载 context
- 两条消息何时应共享状态
- 两个用户何时必须隔离
- 回复如何回到正确的 channel

OpenClaw 是一个很好的例子，因为它把这些决策显式化。

本讲以 OpenClaw 为例，讲授 agent 系统中最重要的一条实践经验：

> session 设计就是产品设计

如果 session 边界划错，整个 agent 体验就会变得**不安全或令人困惑**。

---

## 学习目标

学完本讲后，你将能够：

1. 解释 channel、account、agent 与 session 之间的区别。
2. 理解为什么 routing 规则是 agent 设计的一等公民。
3. 用通俗的语言解释 session key 这一概念。
4. 判断 DM 何时应共享 context、何时必须隔离。
5. 为多入口的 agent 产品设计一套 routing 策略。

---

## 1. 学生常混淆的四样东西

阅读真实的 agent 系统时，学生常常混淆：

- channel
- account
- agent
- session

OpenClaw 很有用，因为它把这几者清晰分开。

### Channel

**channel** 是通信入口。

例如：

- Telegram
- WhatsApp
- Slack
- Discord
- WebChat

### Account

**account** 是某个 channel 上的特定身份。

例如：

- 一个 Telegram bot token
- 一个 WhatsApp 号码
- 一次 Slack app 安装

同一种 channel 类型上可以有多个 account。

### Agent

**agent** 是隔离的大脑：

- workspace
- instructions
- tools
- sessions
- auth profiles

一个 gateway 可以承载多个 agent。

### Session

**session** 是一段对话或工作流的 context 桶。

它决定哪些消息**共享记忆**与对话记录历史。

这一区分至关重要。

同一个用户可能通过多个 channel 联系同一个 agent，但这些对话是否共享 context 是一个**设计选择**。

---

## 2. Routing 问题

一条入站消息不只是「给模型的文本」。

它首先引出一个 routing 问题：

> 这条消息应归哪个 agent、哪个 session 所有？

这个问题的答案取决于：

- channel
- sender
- account
- group 或 room
- thread
- 已配置的 binding

这就是 OpenClaw 使用**显式 binding**的原因。

不让模型来决定，而由**宿主配置**来决定。

这才是正确的设计。

模型不应选择：

- 它在为哪个人服务
- 它在用哪个 workspace
- 应由哪个 account 回复

那些都是**控制面决策**。

---

## 3. 通俗解释 session key

OpenClaw 文档给出了 **session key** 的概念。

最容易理解的解释是：

> session key 就是对话桶上的标签

桶标签相同的消息**共享 context**。

桶标签不同的消息**保持隔离**。

OpenClaw 模型中的例子：

- 一个私信 session
- 每个群聊一个 session
- 每个 room 一个 session
- 每个 thread 一个 session

这极其实用。

这意味着系统可以这样规定：

- 这个 Slack thread 中的所有消息归在一起
- 这个 Telegram group 不得与那个 Discord room 混在一起
- 这些 DM 应共享一个私有 session
- 这两个用户绝不能共享 context

---

## 4. 在多用户系统中，DM 隔离不是可选项

OpenClaw 的 session 文档在这一点上讲得很清楚：

如果许多人都能给这个 bot 发消息，默认的共享 DM 行为可能导致**context 泄漏**。

这是一个重要的教学点。

初学者可能会想：

> 一个助手就应该有一大块记忆

但在多用户产品中，这往往是错的。

一个糟糕设计的例子：

```text
Alice messages the assistant privately.
Bob messages the same assistant privately.
Both share one DM session.
The assistant now carries Alice's context into Bob's chat.
```

这不只是别扭，还可能是一个**隐私问题**。

这就是 OpenClaw 支持 DM 作用域设置的原因，比如：

- 一个共享的主 session
- 按 peer 隔离
- 按 channel-peer 隔离

对于严肃的产品，session 作用域是一项**安全与 UX 特性**。

---

## 5. Channel routing 作为产品决策

OpenClaw 的 routing 规则表明，agent 的归属可以取决于：

- 确切的 peer
- account
- channel
- group
- team
- guild
- role

这意味着 routing 不仅仅是**技术管道**。

它是**产品行为**。

例如：

```text
Telegram support bot -> support agent
Slack engineering room -> engineering agent
WhatsApp family group -> family assistant agent
Discord moderator room -> moderation agent
```

这些是在同一个 gateway 里运行的不同的产品。

因此，routing 设计就是把一个 runtime 变成多种有用行为的方式。

---


<details>
<summary>English original</summary>

**Lecture 32 - OpenClaw Case Study: Channels, Routing, and Session Design**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 31](Lecture-31.md) | **Next:** [Lecture 33](Lecture-33.md)

---

**Why this lecture exists**

Once you move beyond a single chat window, agent design becomes a **routing problem**.

You must decide:

- which inbound message goes to which agent
- which session should hold the context
- when two messages should share state
- when two users must be isolated
- how replies return to the correct channel

OpenClaw is a strong example because it makes these decisions explicit.

This lecture uses OpenClaw to teach one of the most important practical lessons in agent systems:

> session design is product design

If session boundaries are wrong, the whole agent experience becomes **unsafe or confusing**.

---

**Learning objectives**

By the end of this lecture you will be able to:

1. Explain the difference between channel, account, agent, and session.
2. Understand why routing rules are a first-class part of agent design.
3. Explain the session-key idea in simple terms.
4. Decide when DMs should share context and when they must be isolated.
5. Design a routing policy for a multi-surface agent product.

---

**1. Four things students often mix up**

When reading a real agent system, students often mix up:

- channel
- account
- agent
- session

OpenClaw is useful because it separates them clearly.

**Channel**

A **channel** is the communication surface.

Examples:

- Telegram
- WhatsApp
- Slack
- Discord
- WebChat

**Account**

An **account** is a specific identity on a channel.

Examples:

- one Telegram bot token
- one WhatsApp number
- one Slack app installation

You can have multiple accounts on the same channel type.

**Agent**

An **agent** is the isolated brain:

- workspace
- instructions
- tools
- sessions
- auth profiles

One gateway can host multiple agents.

**Session**

A **session** is the context bucket for a conversation or workflow.

It decides which messages **share memory** and transcript history.

This distinction is essential.

One user may contact the same agent through many channels, but whether those conversations share context is a **design choice**.

---

**2. The routing problem**

An inbound message is not just "text for the model."

It first raises a routing question:

> Which agent and which session should own this message?

That question depends on:

- channel
- sender
- account
- group or room
- thread
- configured bindings

This is why OpenClaw uses **explicit bindings**.

Instead of letting the model decide, the **host configuration** decides.

That is the right design.

The model should not choose:

- which human it is serving
- which workspace it is using
- which account should answer

Those are **control-plane decisions**.

---

**3. Session keys in simple language**

OpenClaw documents the idea of a **session key**.

The easiest explanation is:

> a session key is the label on the conversation bucket

Messages with the same bucket label **share context**.

Messages with different bucket labels **stay isolated**.

Examples from the OpenClaw model:

- one direct-message session
- one session per group chat
- one session per room
- one session per thread

This is extremely practical.

It means the system can say:

- all messages in this Slack thread belong together
- this Telegram group must not mix with that Discord room
- these DMs should share one private session
- these two users must never share context

---

**4. DM isolation is not optional in multi-user systems**

OpenClaw's session docs are very clear here:

If many people can message the bot, default shared-DM behavior can **leak context**.

That is a big teaching point.

A beginner might think:

> one assistant should have one big memory

But in a multi-user product, that is often wrong.

Example of a bad design:

```text
Alice messages the assistant privately.
Bob messages the same assistant privately.
Both share one DM session.
The assistant now carries Alice's context into Bob's chat.
```

This is not just awkward. It can be a **privacy issue**.

That is why OpenClaw supports DM scope settings like:

- one shared main session
- per-peer isolation
- per-channel-peer isolation

For a serious product, session scoping is a **security and UX feature**.

---

**5. Channel routing as a product decision**

OpenClaw's routing rules show that agent assignment can depend on:

- exact peer
- account
- channel
- group
- team
- guild
- role

That means routing is not merely **technical plumbing**.

It is **product behavior**.

Example:

```text
Telegram support bot -> support agent
Slack engineering room -> engineering agent
WhatsApp family group -> family assistant agent
Discord moderator room -> moderation agent
```

These are different products running inside one gateway.

So routing design is how you turn one runtime into many useful behaviors.

---

</details>

## 6. 示例：同一个 agent 跨多个界面

假设你希望一个个人助理存在于：

- Telegram DM
- WebChat
- 移动节点语音输入

现在你有一个设计选择：

### 选项 A - 一个共享会话

优点：

- 跨界面的连续性
- agent 在各处都记得上下文

缺点：

- 上下文可能变得混乱
- 一个界面上的一条意外消息会影响其他界面

### 选项 B - 每个界面隔离的会话

优点：

- 更清晰的历史记录
- 更易于调试
- 更少的意外跨界面污染

缺点：

- 连续性较弱

这就是为什么会话设计是一个 **产品取舍**，而不是一个可以忽略的默认值。

---

## 7. 示例配置模式

这是一个简化的 OpenClaw 风格模式：

```json5
{
  agents: {
    list: [
      { id: "support", workspace: "~/.openclaw/workspace-support" },
      { id: "personal", workspace: "~/.openclaw/workspace-personal" }
    ]
  },
  bindings: [
    { match: { channel: "slack", teamId: "T123" }, agentId: "support" },
    { match: { channel: "telegram", peer: { kind: "direct", id: "user_42" } }, agentId: "personal" }
  ],
  session: {
    dmScope: "per-channel-peer"
  }
}
```

你不需要记住确切的配置。

要点是：

- agent 是显式的
- 绑定是显式的
- 会话策略是显式的

这是好的架构。

---

## 8. 群组、线程和房间

一个严肃的 agent 产品必须理解：

- 一条直接消息不等同于一个群组
- 一个群组不等同于一个线程
- 一个房间不等同于一对一聊天

OpenClaw 通过将许多这些会话类型隔离开来对此建模。

这是协作系统的正确默认值。

为什么？

因为线程通常代表 **单独的子对话**。

如果它们全部塌缩到一个桶里，agent 会变得 **嘈杂且不可靠**。

这是你在构建以下内容时应该应用的同一要点：

- 支持 agent
- 研究 agent
- 团队 copilot
- 个人助理

---

## 9. 回复布线应该是确定性的

OpenClaw 的通道布线文档提出了一个重要观点：

> 模型不选择通道

那个选择属于 **宿主系统**。

这是一个好的专业规则。

模型应该帮助决定：

- 说什么
- 使用哪个工具
- 如何总结

控制平面应该决定：

- 在哪里回复
- 使用哪个账户
- 变更哪个会话
- 哪个 agent 在范围内

这减少了一整类失败，即模型 **发明错误的操作路径**。

---

## 10. 设计练习

为这个产品设计布线：

> 一个家庭助理和一个工程助理运行在同一台主机上。

需求：

- 来自家庭成员的 Telegram DM 转到家庭助理。
- 工程工作区中的 Slack 消息转到工程助理。
- WebChat 应只与工程助理对话。
- DM 不得跨用户共享上下文。

填写此表：

| 决策领域 | 你的答案 |
|---|---|
| Agents | `family`, `engineering` |
| 通道 | Telegram、Slack、WebChat |
| DM 范围 | `per-channel-peer` |
| 支持绑定 | Slack workspace -> `engineering` |
| 家庭绑定 | Telegram direct peers -> `family` |
| Web 绑定 | WebChat -> `engineering` |

这是一个比“为一个有帮助的助理写提示词”更好的系统设计练习。

---

## 关键要点

- 通道、账户、agent 和会话是不同的概念，在你的头脑中应该保持分开。
- 会话设计决定谁与谁共享上下文。
- 布线是一个产品特性，而不仅仅是后端管道。
- 在多用户系统中，DM 通常应该被隔离。
- 回复布线应该是确定性的且由宿主控制，而不是由模型选择。
- OpenClaw 是一个强有力的例子，展示了真实的 agent 产品如何将布线和会话状态视为一等公民。

---

## 参考文献

- 案例研究源仓库： [OpenClaw](https://github.com/openclaw/openclaw)
- OpenClaw 概念：
  - `docs/channels/channel-routing.md`
  - `docs/concepts/session.md`
  - `docs/concepts/multi-agent.md`
  - `docs/channels/pairing.md`

---

*下一篇： [Lecture 33](Lecture-33.md)*


<details>
<summary>English original</summary>

**6. Example: the same agent across multiple surfaces**

Suppose you want one personal assistant to exist in:

- Telegram DM
- WebChat
- mobile node voice input

You now have a design choice:

**Option A - one shared session**

Pros:

- continuity across surfaces
- the agent remembers context everywhere

Cons:

- context can become messy
- one accidental message on one surface affects the others

**Option B - isolated sessions per surface**

Pros:

- cleaner history
- easier debugging
- less accidental cross-surface contamination

Cons:

- weaker continuity

This is why session design is a **product tradeoff**, not a default you should ignore.

---

**7. Example config pattern**

Here is a simplified OpenClaw-style pattern:

```json5
{
  agents: {
    list: [
      { id: "support", workspace: "~/.openclaw/workspace-support" },
      { id: "personal", workspace: "~/.openclaw/workspace-personal" }
    ]
  },
  bindings: [
    { match: { channel: "slack", teamId: "T123" }, agentId: "support" },
    { match: { channel: "telegram", peer: { kind: "direct", id: "user_42" } }, agentId: "personal" }
  ],
  session: {
    dmScope: "per-channel-peer"
  }
}
```

You do not need to memorize the exact config.

The lesson is:

- agents are explicit
- bindings are explicit
- session policy is explicit

That is good architecture.

---

**8. Groups, threads, and rooms**

A serious agent product must understand that:

- a direct message is not the same as a group
- a group is not the same as a thread
- a room is not the same as a one-to-one chat

OpenClaw models this by keeping many of these session types isolated.

That is the right default for collaborative systems.

Why?

Because threads often represent **separate sub-conversations**.

If they all collapse into one bucket, the agent becomes **noisy and unreliable**.

This is the same lesson you should apply when building:

- support agents
- research agents
- team copilots
- personal assistants

---

**9. Reply routing should be deterministic**

OpenClaw's channel-routing docs make an important point:

> the model does not choose the channel

That choice belongs to the **host system**.

This is a good professional rule.

The model should help decide:

- what to say
- which tool to use
- how to summarize

The control plane should decide:

- where to reply
- which account to use
- which session to mutate
- which agent is in scope

This reduces a whole class of failure where the model **invents the wrong operational path**.

---

**10. Design exercise**

Design routing for this product:

> One family assistant and one engineering assistant run on the same host.

Requirements:

- Telegram DMs from family members go to the family assistant.
- Slack messages in the engineering workspace go to the engineering assistant.
- WebChat should talk only to the engineering assistant.
- DMs must not share context across users.

Fill this table:

| Decision area | Your answer |
|---|---|
| Agents | `family`, `engineering` |
| Channels | Telegram, Slack, WebChat |
| DM scope | `per-channel-peer` |
| Support binding | Slack workspace -> `engineering` |
| Family binding | Telegram direct peers -> `family` |
| Web binding | WebChat -> `engineering` |

This is a better system design exercise than "write a prompt for a helpful assistant."

---

**Key takeaways**

- Channel, account, agent, and session are different concepts and should stay separate in your mind.
- Session design decides who shares context with whom.
- Routing is a product feature, not just backend plumbing.
- DMs should usually be isolated in multi-user systems.
- Reply routing should be deterministic and host-controlled, not model-chosen.
- OpenClaw is a strong example of how real agent products treat routing and session state as first-class concerns.

---

**References**

- Case-study source repo: [OpenClaw](https://github.com/openclaw/openclaw)
- OpenClaw concepts:
  - `docs/channels/channel-routing.md`
  - `docs/concepts/session.md`
  - `docs/concepts/multi-agent.md`
  - `docs/channels/pairing.md`

---

*Next: [Lecture 33](Lecture-33.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-32.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-32.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
