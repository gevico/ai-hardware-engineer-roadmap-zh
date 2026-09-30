---
title: 第 33 讲 - OpenClaw 案例研究：多 agent 隔离、workspace 与记忆
description: 第 33 讲 - OpenClaw 案例研究：多 agent 隔离、workspace 与记忆
---

# 第 33 讲 - OpenClaw 案例研究：多 agent 隔离、workspace 与记忆

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [第 32 讲](Lecture-32.md) | **下一讲：** [第 34 讲](Lecture-34.md)

---

## 为什么会有这一讲

只要你在一个产品里跑不止一个 agent，**隔离**就成了一个核心设计问题。

你需要决定：

- 每个 agent 能看到什么
- 每个 agent 把状态存在哪里
- 哪些文件属于哪个 agent
- 记忆能否跨 agent 传递
- 凭证是共享还是分离

OpenClaw 是一个很好的案例，因为它把一个 agent 当作**有作用域的大脑**，拥有自己的：

- workspace
- 状态目录
- auth profile
- 会话存储

这一讲用 OpenClaw 来教一条重要的生产经验：

> 多 agent 系统不只是关于编排，也是关于边界。

---

## 学习目标

学完这一讲，你将能够：

1. 解释在真实的多 agent runtime 中，"agent" 意味着什么。
2. 理解 workspace 隔离为什么重要。
3. 解释 workspace、state 与 sessions 之间的区别。
4. 理解记忆如何成为一个边界问题。
5. 为一个严肃的产品设计干净的多 agent 布局。

---

## 1. 一个 agent 究竟是什么？

OpenClaw 的多 agent 文档给出了非常实用的答案。

一个 agent 不仅仅是：

- 一个 prompt
- 一个模型
- 一个角色字符串

一个 agent 是一个完整的有作用域单元，拥有自己的：

- workspace
- 规则与 persona 文件
- auth profile
- 状态目录
- 会话历史
- 记忆上下文

这比大多数教程使用的定义要强得多。

这很重要，因为一旦接受这个定义，你就不会再把这些 agent 设计成**松散的 prompt 模板**。

你会开始把它们设计成**隔离的应用单元**。

---

## 2. workspace 不只是一个文件夹

OpenClaw 的 workspace 文档在这里很有用。

workspace 是：

- 默认工作目录
- 存放 agent bootstrap 文件的地方
- 存放 agent 人格与指令的地方
- agent 记忆与身份的一部分

重要的一课：

> workspace 不只是存储，它是 agent 运行上下文的一部分。

典型的 workspace 文件包括：

- `AGENTS.md`
- `SOUL.md`
- `USER.md`
- `IDENTITY.md`
- `TOOLS.md`
- 记忆文件

即便在 OpenClaw 之外，这也是一个有用的模式。

它告诉我们，agent 行为不应只存在于代码里，还应该有**结构化的运行文件**。

---

## 3. workspace 不等于 sandbox

这是 OpenClaw 带来的最实用的经验之一。

workspace 是默认的 cwd。

它**并不会自动成为硬 sandbox**。

这意味着：

- 相对路径可能留在 workspace 内
- 除非启用 sandbox，否则绝对路径仍可能逃逸

这正是学生想到下面这句话时容易忽略的细节：

> "我给 agent 一个 workspace，所以它是隔离的。"

不对。

隔离需要**显式的 sandbox**，或等价的 runtime 控制。

这是一个有力的教学例子，因为它迫使学生区分：

- **便利性边界**
- **安全性边界**

这两者不是一回事。

---

## 4. 状态目录 vs 会话存储 vs workspace

这三者很容易混淆。

### Workspace

agent 的家与运行上下文。

### 状态目录

存放 agent 专属 runtime 数据的地方，例如 auth profile 和配置状态。

### 会话存储

对话历史与路由状态所在的地方。

这种分离是**良好的工程实践**，因为它避免了所有东西都塌缩进一个含糊的文件夹。

当学生构建自己的 agent 时，应当照搬这种分离：

| 区域 | 用途 |
|---|---|
| workspace | 可人工编辑的运行文件与 agent 上下文 |
| state | runtime 配置、provider 状态、凭证引用 |
| sessions | 对话记录、历史、路由状态 |

仅这一点就能让系统更易于调试和备份。

---

## 5. 多 agent 隔离的核心是避免冲突

OpenClaw 警告不要在多个 agent 之间复用同一个 `agentDir`。

这一警告给出了一条重要的通用经验：

> 如果两个 agent 共享太多状态，它们就不再是彼此独立的 agent

冲突可能发生在：

- auth profile
- 会话
- workspace
- 记忆集合
- 工具配置
- 文件输出

如果想要彼此独立的 agent，就给它们**各自独立的家**。

糟糕的设计示例：

```text
agent A and agent B
  -> same workspace
  -> same auth file
  -> same session store
```


到这一步，你基本上只是有一个贴着混乱标签的 agent。

更好的设计示例：

```text
agent A
  -> workspace A
  -> auth A
  -> sessions A

agent B
  -> workspace B
  -> auth B
  -> sessions B
```


现在系统才真正有了结构。

---


<details>
<summary>English original</summary>

**Lecture 33 - OpenClaw Case Study: Multi-Agent Isolation, Workspaces, and Memory**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 32](Lecture-32.md) | **Next:** [Lecture 34](Lecture-34.md)

---

**Why this lecture exists**

As soon as you run more than one agent in one product, **isolation** becomes a core design problem.

You need to decide:

- what each agent can see
- where each agent stores state
- which files belong to which agent
- whether memories can cross between agents
- whether credentials are shared or separated

OpenClaw is a strong case study because it treats an agent as a **scoped brain** with its own:

- workspace
- state directory
- auth profiles
- session store

This lecture uses OpenClaw to teach an important production lesson:

> multi-agent systems are not only about orchestration. They are also about boundaries.

---

**Learning objectives**

By the end of this lecture you will be able to:

1. Explain what an "agent" means in a real multi-agent runtime.
2. Understand why workspace isolation matters.
3. Explain the difference between workspace, state, and sessions.
4. Understand how memory becomes a boundary problem.
5. Design a clean multi-agent layout for a serious product.

---

**1. What is one agent, really?**

OpenClaw's multi-agent docs give a very practical answer.

One agent is not merely:

- one prompt
- one model
- one role string

One agent is a full scoped unit with its own:

- workspace
- rules and persona files
- auth profile
- state directory
- session history
- memory context

That is a much stronger definition than most tutorials use.

This matters because once you accept that definition, you stop designing agents as **loose prompt templates**.

You start designing them as **isolated application units**.

---

**2. Workspace is not just a folder**

OpenClaw's workspace docs are useful here.

The workspace is:

- the default working directory
- the place for agent bootstrap files
- the place for agent personality and instructions
- part of the agent's memory and identity

Important lesson:

> The workspace is not only storage. It is part of the agent's operating context.

Typical workspace files include:

- `AGENTS.md`
- `SOUL.md`
- `USER.md`
- `IDENTITY.md`
- `TOOLS.md`
- memory files

This is a useful pattern even outside OpenClaw.

It teaches that agent behavior should not live only in code. It should also have **structured operating files**.

---

**3. Workspace is not the same as sandbox**

This is one of the most practical lessons from OpenClaw.

The workspace is the default cwd.

It is **not automatically a hard sandbox**.

That means:

- relative paths may stay inside the workspace
- absolute paths may still escape unless sandboxing is enabled

This is exactly the kind of nuance students miss when they think:

> "I gave the agent a workspace, so it is isolated."

No.

Isolation requires **explicit sandboxing** or equivalent runtime controls.

This is a powerful teaching example because it forces students to distinguish:

- **convenience boundary**
- **security boundary**

Those are not the same thing.

---

**4. State directory vs session store vs workspace**

These three are easy to confuse.

**Workspace**

The agent's home and operating context.

**State directory**

The place for agent-specific runtime data such as auth profiles and configuration state.

**Session store**

The place where conversation history and routing state live.

This separation is **good engineering** because it prevents everything from collapsing into one unclear folder.

When students build their own agents, they should copy this separation:

| Area | Purpose |
|---|---|
| workspace | human-editable operating files and agent context |
| state | runtime config, provider state, credentials references |
| sessions | transcripts, history, routing state |

That alone makes systems easier to debug and back up.

---

**5. Multi-agent isolation is about avoiding collisions**

OpenClaw warns against reusing the same `agentDir` across agents.

That warning teaches an important general lesson:

> if two agents share too much state, they stop being separate agents

Collisions can happen in:

- auth profiles
- sessions
- workspaces
- memory collections
- tool configuration
- file outputs

If you want separate agents, give them **separate homes**.

Example bad design:

```text
agent A and agent B
  -> same workspace
  -> same auth file
  -> same session store
```

At that point, you mostly have one agent with confusing labels.

Example better design:

```text
agent A
  -> workspace A
  -> auth A
  -> sessions A

agent B
  -> workspace B
  -> auth B
  -> sessions B
```

Now the system is actually structured.

---

</details>

## 6. 内存是一个边界问题

OpenClaw 在这里特别有用，因为它展示了多种内存思路：

- 普通会话历史
- 长期记忆文件
- 活跃记忆插件
- 跨会话检索
- 显式配置后的跨 agent 检索

现代 agent 系统的行为正是如此：

内存**不是单一的东西**。

它包含：

- 工作记忆
- 会话记录记忆
- 长期记忆
- 检索到的记忆
- 可选的跨 agent 记忆

生产环境的教训是：

> 每一条记忆路径都应当是显式的

如果允许跨 agent 记忆，它就应当被**有意配置**。

如果不是有意的，它就不该**意外发生**。

---

## 7. 活跃记忆作为一种设计模式

OpenClaw 的 `active-memory` 概念很有教学价值。

思路很简单：

与其等待主 agent 决定何时检索记忆，不如让一个**有界的内存 sub-agent** 在主回复之前运行，并把相关记忆呈现出来。

这带来两个重要教训。

### 教训 1：记忆可以是主动的

多数学生认为记忆意味着：

> 保存消息，之后再检索

活跃记忆展示了另一种模式：

> 记忆可以是一个独立的 runtime 组件，在主生成发生之前就改善下一次回复

### 教训 2：辅助 agent 应当有界

OpenClaw 的内存 helper 不只是「另一个自由 agent」。

它是：

- 可选的
- 有作用域的
- 有计时的
- 有界的

这是好设计。

如果你要加辅助 agent，它们应当有**紧凑的作用域和目的**。

---

## 8. 示例：两个 agent，一台主机

假设你在一台机器上运行两个 agent：

- `coding`
- `family`

好的设计：

| Agent | Workspace | Sessions | Auth | Memory |
|---|---|---|---|---|
| coding | `workspace-coding` | `sessions-coding` | coding auth profile | coding memory only |
| family | `workspace-family` | `sessions-family` | family auth profile | family memory only |

为什么这是好的：

- coding 任务不会污染个人记忆
- 个人信息不会出现在工程会话里
- 工具权限可以不同
- 模型提供商可以不同
- 备份更容易

这正是严肃产品需要的那种 layout。

---

## 9. 示例配置模式

下面是一个简单的 OpenClaw 风格模式：

```json5
{
  agents: {
    list: [
      {
        id: "coding",
        workspace: "~/.openclaw/workspace-coding"
      },
      {
        id: "family",
        workspace: "~/.openclaw/workspace-family"
      }
    ]
  }
}
```


再次强调，重点不是背配置语法。

重点是理解设计：

- 每个 agent 有自己的运行主目录
- 每个 agent 有自己的会话
- 每个 agent 可以有不同的 auth 和记忆行为

这就是在一个 runtime 里构建多个真实 agent 的方法。

---

## 10. 隔离检查清单

如果你在构建多 agent 产品，问这些问题。

### Workspace

- 每个 agent 都有自己的 workspace 吗？
- agent 指令文件是分开的吗？

### Credentials

- 每个 agent 都有自己的 auth profile 吗？
- 敏感能力被隔离开了吗？

### Sessions

- 每个 agent 都有自己的会话存储吗？
- 一个 agent 会意外读到另一个 agent 的记录吗？

### Memory

- 跨 agent 记忆访问是显式的吗？
- 它默认是关闭的吗？

### Tools

- 每个 agent 只能访问它该用的工具吗？
- 危险工具是否被排除在低信任 agent 之外？

这就是真实的多 agent 工程的样子。

---

## 11. 设计练习

为一家小创业公司设计一个三 agent 系统：

- `support`
- `research`
- `ops`

对每一个，定义：

| Agent | Workspace | Tools | Memory rule | Risk note |
|---|---|---|---|---|
| support | customer support workspace | docs search, ticketing | no cross-agent memory | must avoid exposing internal ops data |
| research | analyst workspace | web search, notes | may read shared research library | external information risk |
| ops | automation workspace | deployment tools, logs | isolated | high-risk action space |

这个练习应当让一件事变清楚：

多 agent 设计主要关乎**边界与职责**。

---

## 关键要点

- 在真实 runtime 中，一个 agent 是一个有作用域的单位，拥有自己的 workspace、状态、auth 和会话。
- workspace 是运行上下文，并不自动就是安全沙箱。
- workspace、状态与会话存储应当在概念上保持分离。
- 内存是边界问题，而不只是检索特性。
- 跨 agent 记忆访问应当是显式的，而非偶然的。
- OpenClaw 是构建严肃多 agent 系统的有力案例。

---

## References

- Case-study source repo: [OpenClaw](https://github.com/openclaw/openclaw)
- OpenClaw concepts:
  - `docs/concepts/multi-agent.md`
  - `docs/concepts/agent-workspace.md`
  - `docs/concepts/active-memory.md`
  - `docs/concepts/session.md`

---

*Next: [Lecture 34](Lecture-34.md)*


<details>
<summary>English original</summary>

**6. Memory as a boundary problem**

OpenClaw is especially useful here because it shows several memory ideas:

- normal session history
- long-term memory files
- active memory plugin
- cross-session search
- cross-agent search when explicitly configured

This is exactly how modern agent systems behave:

memory is **not one thing**.

There is:

- working memory
- session transcript memory
- long-term memory
- retrieved memory
- optional cross-agent memory

The production lesson is:

> every memory path should be explicit

If cross-agent memory is allowed, it should be **configured intentionally**.

If it is not intentional, it should not happen **by accident**.

---

**7. Active memory as a design pattern**

OpenClaw's `active-memory` concept is very educational.

The idea is simple:

Instead of waiting for the main agent to decide when to search memory, a **bounded memory sub-agent** runs before the main reply and surfaces relevant memory.

This teaches two important lessons.

**Lesson 1: memory can be proactive**

Most students think memory means:

> save messages and search them later

Active memory shows a different pattern:

> memory can be a separate runtime component that improves the next reply before the main generation happens

**Lesson 2: helper agents should be bounded**

OpenClaw's memory helper is not just "another free agent."

It is:

- optional
- scoped
- timed
- bounded

That is good design.

If you add helper agents, they should have **tight scope and purpose**.

---

**8. Example: two agents, one host**

Suppose you run two agents on one machine:

- `coding`
- `family`

Good design:

| Agent | Workspace | Sessions | Auth | Memory |
|---|---|---|---|---|
| coding | `workspace-coding` | `sessions-coding` | coding auth profile | coding memory only |
| family | `workspace-family` | `sessions-family` | family auth profile | family memory only |

Why this is good:

- coding tasks do not pollute personal memory
- personal messages do not appear in engineering sessions
- tool permissions can differ
- model providers can differ
- backups are easier

This is exactly the kind of layout serious products need.

---

**9. Example config pattern**

Here is a simple OpenClaw-style pattern:

```json5
{
  agents: {
    list: [
      {
        id: "coding",
        workspace: "~/.openclaw/workspace-coding"
      },
      {
        id: "family",
        workspace: "~/.openclaw/workspace-family"
      }
    ]
  }
}
```

Again, the point is not memorizing config syntax.

The point is understanding the design:

- each agent gets its own operating home
- each agent gets its own sessions
- each agent can have different auth and memory behavior

That is how you build multiple real agents in one runtime.

---

**10. Isolation checklist**

If you are building a multi-agent product, ask these questions.

**Workspace**

- Does each agent have its own workspace?
- Are agent instruction files separate?

**Credentials**

- Does each agent have its own auth profile?
- Are sensitive capabilities separated?

**Sessions**

- Does each agent have its own session store?
- Can one agent accidentally read another agent's transcript?

**Memory**

- Is cross-agent memory access explicit?
- Is it disabled by default?

**Tools**

- Can each agent access only the tools it should use?
- Are dangerous tools excluded from low-trust agents?

This is what real multi-agent engineering looks like.

---

**11. Design exercise**

Design a three-agent system for a small startup:

- `support`
- `research`
- `ops`

For each one, define:

| Agent | Workspace | Tools | Memory rule | Risk note |
|---|---|---|---|---|
| support | customer support workspace | docs search, ticketing | no cross-agent memory | must avoid exposing internal ops data |
| research | analyst workspace | web search, notes | may read shared research library | external information risk |
| ops | automation workspace | deployment tools, logs | isolated | high-risk action space |

This exercise should make one thing clear:

multi-agent design is mostly about **boundaries and responsibilities**.

---

**Key takeaways**

- In a real runtime, one agent is a scoped unit with its own workspace, state, auth, and sessions.
- The workspace is an operating context, not automatically a security sandbox.
- Workspace, state, and session storage should stay conceptually separate.
- Memory is a boundary problem, not only a retrieval feature.
- Cross-agent memory access should be explicit, not accidental.
- OpenClaw is a strong case study for how to structure serious multi-agent systems.

---

**References**

- Case-study source repo: [OpenClaw](https://github.com/openclaw/openclaw)
- OpenClaw concepts:
  - `docs/concepts/multi-agent.md`
  - `docs/concepts/agent-workspace.md`
  - `docs/concepts/active-memory.md`
  - `docs/concepts/session.md`

---

*Next: [Lecture 34](Lecture-34.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-33.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-33.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
