---
title: 第 37 讲 - OpenClaw 案例研究：系统提示词架构
description: 第 37 讲 - OpenClaw 案例研究：系统提示词架构
---

# 第 37 讲 - OpenClaw 案例研究：系统提示词架构

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [第 36 讲](Lecture-36.md) | **下一讲：** [第 38 讲](Lecture-38.md)

---

现代 agent 系统不是由 **一段简短的提示词** 控制的。

它们由一套 **提示词装配系统** 控制。

这套系统决定：

- agent 获得什么身份
- 它知道如何使用哪些工具
- 注入什么工作区上下文
- 存在哪些安全指引
- 追加哪些 provider 专属调优
- 哪些内容应当保持稳定以利于提示词缓存
- 哪些内容应当留在提示词之外，由 runtime 策略强制执行

本讲以 OpenClaw 的系统提示词设计作为案例研究。

关键结论是：

> 生产环境的 agent 不应依赖某家 provider 随手给的默认提示词；它应当拥有一套由应用 runtime 装配的、可检查、可版本化的提示词

---

## 学习目标

学完本讲，你应当能够：

1. 解释 OpenClaw 为什么自己掌控系统提示词，而不是使用 provider 默认值。
2. 描述 OpenClaw 风格系统提示词的主要组成部分。
3. 理解 full、minimal 和 none 三种提示词模式。
4. 解释 bootstrap 文件如何成为 Project Context。
5. 理解技能为什么以紧凑方式列出并按需加载。
6. 区分提示词层面的建议性安全约束与 runtime 的硬性强制执行。
7. 设计对缓存友好的 provider overlay。
8. 在调试 agent 时检查提示词体积与上下文注入。

---

## 1. 简单的思维模型

把严肃的 agent 想象成一名 **现场工程师**。

工程师开工之前，你不会只说：

```text
Be helpful.
```


你会给他们一本操作手册：

```text
Who you are.
How to talk to users.
How to use tools.
Which workspace you are in.
What files contain project context.
What policies you must follow.
When to ask for help.
How to report completion.
```


在 OpenClaw 中，**系统提示词** 就是那本操作手册。

它在每次 agent 运行之前装配完成。

```text
agent request
  -> resolve agent/session/workspace
  -> assemble OpenClaw-owned system prompt
  -> inject bootstrap context and skills list
  -> apply provider-specific small overlays
  -> run model
```


模型不会自己发明这本操作手册。

OpenClaw 负责构建它。

---

## 2. 为什么 OpenClaw 自己掌控系统提示词

provider 默认提示词对 **通用聊天产品** 有用。

但对具备以下能力的产品来说远远不够：

- 工具
- 会话
- 工作区
- 子 agent
- cron 任务
- 长时间运行的进程
- provider 插件
- 本地文档
- 记忆文件
- 沙箱行为
- 网关命令
- 用户专属人格文件

OpenClaw 自己掌控提示词，从而让行为 **在不同模型和 provider 之间保持一致**。

系统可以声明：

- “OpenClaw 的 agent 是这样使用工具的。”
- “本地文档放在这里。”
- “这就是工作区。”
- “子 agent 应当这样使用。”
- “注入的 bootstrap 上下文是这些。”
- “在这个 runtime 里，安全意味着这些。”

provider 依然重要，但 **agent 契约** 归产品所有。

---

## 3. 固定的提示词骨架

OpenClaw 使用紧凑、结构化的分节。

具体措辞可能演变，但架构是稳定的。

OpenClaw 风格的提示词大致如下：

```text
Identity
Tooling
Execution Bias
Safety
Skills
OpenClaw Self-Update
Workspace
Documentation
Project Context
Sandbox
Current Date & Time
Reply Tags
Heartbeats
Runtime
Reasoning
Provider additions
```


重点不是把提示词写长。

重点是让它 **可预测**。

每一节只承担一项职责。

---

## 4. Tooling 部分

Tooling 部分教会模型在这个 runtime 内 **工作应当如何开展**。

它涵盖这样一些模式：

- 使用结构化工具，而不是假装已经行动
- 后续跟进优先使用 `cron`，而不是 sleep 循环
- 长时间运行的命令与日志使用进程工具
- 较大的并行工作使用子 agent
- 不要以紧密循环轮询子 agent
- 仅在启用且有用时才使用 `update_plan`

这一节很重要，因为使用工具的 agent 常常以很无聊的方式失败：

- 声称做了某事，却没调用工具
- 启动了一个长时间运行的进程却丢掉了日志
- 在命令里 sleep，而不是调度后续工作
- 简单工作却派生过多 agent
- 反复更新计划却不做实际工作

Tooling 部分把产品预期转化为 **面向模型的习惯**。

---


<details>
<summary>English original</summary>

**Lecture 37 - OpenClaw Case Study: System Prompt Architecture**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 36](Lecture-36.md) | **Next:** [Lecture 38](Lecture-38.md)

---

Modern agent systems are not controlled by **one short prompt**.

They are controlled by a **prompt assembly system**.

That system decides:

- what identity the agent receives
- what tools it knows how to use
- what workspace context is injected
- what safety guidance is present
- what provider-specific tuning is added
- what should stay stable for prompt caching
- what should stay out of the prompt and be enforced by runtime policy

This lecture uses OpenClaw's system prompt design as the case study.

The important lesson is:

> a production agent should not depend on a random provider default prompt; it should have an owned, inspectable, versioned prompt assembled by the application runtime

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Explain why OpenClaw owns the system prompt instead of using provider defaults.
2. Describe the major sections of an OpenClaw-style system prompt.
3. Understand full, minimal, and none prompt modes.
4. Explain how bootstrap files become Project Context.
5. Understand why skills are listed compactly and loaded on demand.
6. Separate advisory prompt safety from hard runtime enforcement.
7. Design a cache-aware provider overlay.
8. Inspect prompt size and context injection when debugging an agent.

---

**1. Simple mental model**

Think of a serious agent as a **field engineer**.

Before the engineer starts work, you do not just say:

```text
Be helpful.
```

You give them an operating binder:

```text
Who you are.
How to talk to users.
How to use tools.
Which workspace you are in.
What files contain project context.
What policies you must follow.
When to ask for help.
How to report completion.
```

In OpenClaw, the **system prompt** is that operating binder.

It is assembled before each agent run.

```text
agent request
  -> resolve agent/session/workspace
  -> assemble OpenClaw-owned system prompt
  -> inject bootstrap context and skills list
  -> apply provider-specific small overlays
  -> run model
```

The model does not invent the operating binder.

OpenClaw builds it.

---

**2. Why OpenClaw owns the system prompt**

A provider default prompt is useful for a **generic chat product**.

It is not enough for a product that has:

- tools
- sessions
- workspaces
- sub-agents
- cron jobs
- long-running processes
- provider plugins
- local docs
- memory files
- sandbox behavior
- gateway commands
- user-specific persona files

OpenClaw owns the prompt so that behavior is **consistent across models and providers**.

The system can say:

- "This is how OpenClaw agents use tools."
- "This is where local docs live."
- "This is the workspace."
- "This is how sub-agents should be used."
- "This is what bootstrap context was injected."
- "This is what safety means in this runtime."

The provider still matters, but the product owns the **agent contract**.

---

**3. The fixed prompt spine**

OpenClaw uses compact, structured sections.

The exact wording may evolve, but the architecture is stable.

An OpenClaw-style prompt looks like this:

```text
Identity
Tooling
Execution Bias
Safety
Skills
OpenClaw Self-Update
Workspace
Documentation
Project Context
Sandbox
Current Date & Time
Reply Tags
Heartbeats
Runtime
Reasoning
Provider additions
```

The point is not to make the prompt long.

The point is to make it **predictable**.

Each section has one job.

---

**4. Tooling section**

The Tooling section teaches the model **how work should be done** inside this runtime.

It covers patterns such as:

- use structured tools instead of pretending to act
- prefer `cron` for future follow-ups instead of sleep loops
- use process tools for long-running commands and logs
- use sub-agents for larger parallel work
- do not poll sub-agents in tight loops
- use `update_plan` only when it is enabled and useful

This section is important because tool-using agents often fail in boring ways:

- they say they did something but did not call the tool
- they start a long process and lose the logs
- they sleep inside a command instead of scheduling future work
- they spawn too many agents for simple work
- they update a plan repeatedly without doing work

The Tooling section converts product expectations into **model-facing habits**.

---

</details>

## 5. 执行偏向小节

执行偏向是**“把活干完”**小节。

它给出紧凑的贯彻指引：

- 请求可执行时，就在当前轮次内动手
- 持续推进，直到完成或确实受阻
- 工具结果不理想时进行恢复
- 实时检查可变状态，而不是凭空假设
- 定稿前先验证

这一小节之所以存在，是因为许多模型默认给出**建议**。

生产环境中的工程 agent 往往需要的是**行动**。

对比：

```text
Weak behavior:
"You could run the tests."

OpenClaw-style behavior:
"Run the tests, inspect the failure, patch the issue, rerun verification, then summarize."
```

执行偏向并不意味着鲁莽的自动化。

它的意思是，当 agent 拥有足够的权限与上下文时，应当**把工作贯穿整个 runtime 循环**推进下去。

---

## 6. 安全小节

安全小节刻意写得很短。

它告诉模型不要绕过监督、不当地提权、隐藏有风险的行为，或谋求超出用户意图的控制权。

但这是一条关键的生产经验：

> 提示词层面的安全是建议性的；runtime 层面的强制才是必须的

系统提示词可以要求模型安全行事。

硬性强制必须来自：

- 工具策略
- exec 审批
- 沙箱
- 文件系统边界
- 通道允许列表
- 身份与权限校验
- 审计日志

如果某条命令绝对不能运行，不要只在提示词里写一句“不要运行它”。

要在**工具层**把它拦住。

---

## 7. 技能小节

OpenClaw 可以注入一份紧凑的可用技能列表。

提示词不会把每个技能都粘贴进上下文。

它列出：

- 技能名称
- 简短描述
- 位置

然后指示模型只在需要时才读取相关的 `SKILL.md`。

这才是正确的架构。

错误模式：

```text
Paste every skill file into every run.
```

正确模式：

```text
List available skills compactly.
Load the matching skill on demand.
```

这样可以避免 **token 膨胀**，并防止无关技能污染本次运行。

技能有各自的尺寸预算：

- 全局默认：`skills.limits.maxSkillsPromptChars`
- 按 agent 覆盖：`agents.list[].skillsLimits.maxSkillsPromptChars`

这与 runtime 的其他上下文限制是分开的。

这种分离很重要，因为技能列表与工作区 bootstrap 文件是不同种类的上下文。

---

## 8. OpenClaw 自更新小节

OpenClaw 可以暴露一些工具，用于安全地检视和修改自身配置。

提示词给出一条受控路径：

```text
config.schema.lookup
config.patch
config.apply
update.run
```

模型应在修改配置前先检视 schema。

它应当做窄范围补丁。

它应当通过受支持的 gateway 工具来应用。

它不应随意重写受保护的执行策略键。

对任何自我修改的 agent 系统来说，这都是一个有价值的模式：

> 自更新应当是 schema 驱动、范围窄、有日志、有防护的

不要给 agent **直接改写配置文件**的能力，然后指望提示词让它保持安全。

---

## 9. 工作区与文档小节

工作区小节告诉 agent 它在哪里操作。

它通常反映：

```text
agents.defaults.workspace
```

文档小节告诉 agent 本地 OpenClaw 文档放在哪里，并鼓励优先查阅它们。

这是一个细微但重要的生产模式。

对于工具丰富的本地助手，本地文档往往**比记忆更准确**。

提示词应把模型指向本地的事实来源：

```text
If you need OpenClaw behavior, inspect local docs first.
Then act.
```

---

## 10. 项目上下文与 bootstrap 文件

OpenClaw 会把选定的工作区文件追加到项目上下文之下。

这些是 bootstrap 文件，例如：

- `AGENTS.md`
- `SOUL.md`
- `TOOLS.md`
- `IDENTITY.md`
- `USER.md`
- `HEARTBEAT.md`
- `BOOTSTRAP.md`
- `MEMORY.md`

目标很简单：

> 重要的身份信息与项目上下文应当直接呈现，无需模型记得去读取

示例：

```text
Project Context
  AGENTS.md -> project rules
  SOUL.md -> personality and voice
  TOOLS.md -> custom workspace tool guidance
  USER.md -> user preferences
  MEMORY.md -> durable compact memory
```

这很强大，但有**代价**。

大型 bootstrap 文件会增加：

- 提示词体积
- 压缩压力
- 延迟
- 缓存失效风险
- 无关上下文暴露

因此 OpenClaw 会对注入做裁剪。

文档记载的默认值包括：

- 单文件上限：`agents.defaults.bootstrapMaxChars`，默认 `12000`
- 注入总量上限：`agents.defaults.bootstrapTotalMaxChars`，默认 `60000`
- 截断告警：`agents.defaults.bootstrapPromptTruncationWarning`，默认 `once`

实用准则是：

> bootstrap 文件应当是精简的运行上下文，而不是杂物堆放场

---


<details>
<summary>English original</summary>

**5. Execution Bias section**

Execution Bias is the **"finish the work"** section.

It gives compact follow-through guidance:

- act within the current turn when the request is actionable
- continue until done or genuinely blocked
- recover when a tool result is weak
- check mutable state live instead of assuming
- verify before finalizing

This section exists because many models default to **advice**.

A production engineering agent often needs **action**.

Compare:

```text
Weak behavior:
"You could run the tests."

OpenClaw-style behavior:
"Run the tests, inspect the failure, patch the issue, rerun verification, then summarize."
```

Execution Bias does not mean reckless automation.

It means the agent should **carry work through the runtime loop** when it has enough permission and context.

---

**6. Safety section**

The Safety section is intentionally brief.

It tells the model not to bypass oversight, escalate privileges improperly, hide risky behavior, or seek control outside the user intent.

But this is a key production lesson:

> prompt safety is advisory; runtime enforcement is mandatory

The system prompt can ask the model to behave safely.

Hard enforcement must come from:

- tool policy
- exec approvals
- sandboxing
- filesystem boundaries
- channel allowlists
- identity and permission checks
- audit logs

If a command must never run, do not merely write "do not run it" in a prompt.

Block it in the **tool layer**.

---

**7. Skills section**

OpenClaw can inject a compact list of available skills.

The prompt does not paste every skill into the context.

It lists:

- skill name
- short description
- location

Then the model is instructed to read the relevant `SKILL.md` only when needed.

That is the correct architecture.

Bad pattern:

```text
Paste every skill file into every run.
```

Good pattern:

```text
List available skills compactly.
Load the matching skill on demand.
```

This avoids **token bloat** and keeps unrelated skills from polluting the run.

Skills have their own sizing budget:

- global default: `skills.limits.maxSkillsPromptChars`
- per-agent override: `agents.list[].skillsLimits.maxSkillsPromptChars`

This is separate from other runtime context limits.

That separation matters because a skills list and a workspace bootstrap file are different kinds of context.

---

**8. OpenClaw Self-Update section**

OpenClaw can expose tools for safely inspecting and changing its own configuration.

The prompt teaches a controlled path:

```text
config.schema.lookup
config.patch
config.apply
update.run
```

The model should inspect the schema before changing config.

It should patch narrowly.

It should apply through the supported gateway tool.

It should not rewrite protected execution policy keys casually.

This is a useful pattern for any self-modifying agent system:

> self-update should be schema-driven, narrow, logged, and guarded

Do not give an agent **raw config file mutation** and hope the prompt keeps it safe.

---

**9. Workspace and documentation sections**

The Workspace section tells the agent where it is operating.

It usually reflects:

```text
agents.defaults.workspace
```

The Documentation section tells the agent where local OpenClaw docs live and encourages consulting them first.

This is a subtle but important production pattern.

For a tool-rich local assistant, local docs are often **more accurate than memory**.

The prompt should point the model to the local source of truth:

```text
If you need OpenClaw behavior, inspect local docs first.
Then act.
```

---

**10. Project Context and bootstrap files**

OpenClaw appends selected workspace files under Project Context.

These are bootstrap files such as:

- `AGENTS.md`
- `SOUL.md`
- `TOOLS.md`
- `IDENTITY.md`
- `USER.md`
- `HEARTBEAT.md`
- `BOOTSTRAP.md`
- `MEMORY.md`

The goal is simple:

> important identity and project context should be present without requiring the model to remember to read it

Example:

```text
Project Context
  AGENTS.md -> project rules
  SOUL.md -> personality and voice
  TOOLS.md -> custom workspace tool guidance
  USER.md -> user preferences
  MEMORY.md -> durable compact memory
```

This is powerful, but it has a **cost**.

Large bootstrap files increase:

- prompt size
- compaction pressure
- latency
- cache invalidation risk
- irrelevant context exposure

So OpenClaw trims injection.

Documented defaults include:

- per-file max: `agents.defaults.bootstrapMaxChars`, default `12000`
- total injected max: `agents.defaults.bootstrapTotalMaxChars`, default `60000`
- truncation warning: `agents.defaults.bootstrapPromptTruncationWarning`, default `once`

The practical rule:

> bootstrap files should be concise operating context, not a dumping ground

---

</details>

## 11. 记忆文件

`MEMORY.md` 可作为 bootstrap context 注入。

`memory/*.md` 下的每日记忆文件则不同。

它们默认通常不会被注入。

它们通过如下记忆工具按需访问：

```text
memory_search
memory_get
```

这让正常运行保持**轻量**。

最近的每日记忆可能会在特殊的 bare `/new` 或 `/reset` 轮次中前置一次，但并非每次运行的默认行为。

这是正确的取舍：

- 持久而紧凑的记忆可以作为提示词上下文
- 庞大的每日日志只在相关时才检索

---

## 12. 提示词模式

OpenClaw 支持多种提示词模式。

### Full 模式

Full 模式为默认值。

它包含以下主要章节：

- Tooling
- Execution Bias
- Safety
- Skills
- OpenClaw Self-Update
- Workspace
- Documentation
- Project Context
- Sandbox
- Current Date and Time
- Reply Tags
- Heartbeats
- Runtime
- Reasoning

主交互 agent 使用 Full 模式。

### Minimal 模式

Minimal 模式用于**子 agent**。

它保留**必要的 runtime 契约**，但省略会让被委派 worker 膨胀或困惑的上下文。

它省略如下章节：

- Skills
- Memory Recall
- OpenClaw Self-Update
- Model Aliases
- User Identity
- Reply Tags
- Messaging
- Silent Replies
- Heartbeats

它保留如下章节：

- Tooling
- Safety
- Workspace
- Sandbox
- Current Date and Time
- Runtime
- 注入的上下文

在 Minimal 模式下，注入的提示词标记为 **Subagent Context**，而非 **Group Chat Context**。

思路是：

> 子 agent 只需要足够完成其受限任务的上下文，不需要主助手的完整人格与记忆系统

### None 模式

None 模式只返回**基础身份行**。

极少使用它。

它适用于测试，或调用方几乎不想要任何 runtime 提示词内容的场景。

---

## 13. 子 agent bootstrap 行为

子 agent 会话注入的上下文更少。

OpenClaw 将子 agent bootstrap 限制为：

- `AGENTS.md`
- `TOOLS.md`

这是有意为之。

子 agent 应了解项目规则与工具规则。

它通常不需要完整的用户资料、记忆、心跳行为或自更新指令。

这让被委派的工作**更便宜、噪声更少**。

---

## 14. 时间处理与提示词缓存稳定性

Current Date and Time 章节是经过精心设计的。

它包含时区与时间格式指引。

它避免把实时时钟注入每一次提示词。

为什么？

因为实时时间戳**每次运行都会变化**。

每次运行都变化会**损害提示词缓存**。

因此 OpenClaw 保持提示词缓存边界稳定，并告诉模型在需要时如何获取精确时间。

当精确时间戳重要时，使用：

```text
session_status
```

相关配置键：

- `agents.defaults.userTimezone`
- `agents.defaults.timeFormat`

这是一条重要的生产经验：

> 不要把高频变化的值放进稳定提示词，除非模型每一轮都确实需要它们

---

## 15. Provider 调优与缓存感知 overlay

允许 Provider 插件贡献少量补充内容。

它们不应替换整个 OpenClaw 系统提示词。

Provider 插件可以：

- 替换具名核心章节，例如 `interaction_style`
- 替换 `tool_call_style`
- 替换 `execution_bias`
- 在提示词缓存边界之上注入稳定前缀
- 在提示词缓存边界之下注入动态后缀

这样就能实现**按模型族调优**，同时不失去产品控制权。

示例：

```text
OpenClaw core prompt:
  stable runtime behavior

Provider overlay:
  small model-family guidance for GPT-5, Claude, local models, etc.
```

OpenAI GPT-5 系列 overlay 的做法是保持核心执行规则精简，同时加入如下指引：

- 人格锁定
- 简洁输出
- 工具纪律
- 并行查找
- 交付物覆盖
- 验证
- 缺失上下文处理
- 终端工具卫生

架构规则是：

> Provider 调优应当是一个小 overlay，而不是对产品提示词的恶意接管

---

## 16. 遗留 hook 与 Provider 贡献的对比

OpenClaw 仍支持遗留的 `before_prompt_build` hook。

它可以在全局注入或修改提示词内容。

但就模型族行为而言，更推荐 Provider 贡献。

为什么？

因为 Provider 贡献可以做到**缓存感知**，并限定在模型族范围内。

使用正确的层：

| 需求 | 更合适的机制 |
|---|---|
| 工作区专属上下文 | Bootstrap 文件 |
| 全局提示词修改 | `before_prompt_build` hook |
| 模型族调优 | Provider 贡献 |
| Runtime 安全 | 工具策略与沙箱 |
| 用户人格 | `SOUL.md` |

---


<details>
<summary>English original</summary>

**11. Memory files**

`MEMORY.md` can be injected as bootstrap context.

Daily memory files under `memory/*.md` are different.

They are not normally injected by default.

They are accessed on demand through memory tools such as:

```text
memory_search
memory_get
```

This keeps normal runs **small**.

Recent daily memory may be prepended once in special bare `/new` or `/reset` turns, but it is not the default for every run.

This is the right tradeoff:

- durable compact memory can be prompt context
- large daily logs should be retrieved only when relevant

---

**12. Prompt modes**

OpenClaw supports multiple prompt modes.

**Full mode**

Full mode is the default.

It includes the main sections:

- Tooling
- Execution Bias
- Safety
- Skills
- OpenClaw Self-Update
- Workspace
- Documentation
- Project Context
- Sandbox
- Current Date and Time
- Reply Tags
- Heartbeats
- Runtime
- Reasoning

Use full mode for the main interactive agent.

**Minimal mode**

Minimal mode is used for **sub-agents**.

It keeps the **essential runtime contract** but omits context that would bloat or confuse a delegated worker.

It omits sections like:

- Skills
- Memory Recall
- OpenClaw Self-Update
- Model Aliases
- User Identity
- Reply Tags
- Messaging
- Silent Replies
- Heartbeats

It keeps sections like:

- Tooling
- Safety
- Workspace
- Sandbox
- Current Date and Time
- Runtime
- injected context

In minimal mode, injected prompts are labeled **Subagent Context** instead of **Group Chat Context**.

The idea is:

> a sub-agent needs enough context to do its bounded task, not the entire personality and memory system of the main assistant

**None mode**

None mode returns only the **base identity line**.

Use it rarely.

It is useful for tests or cases where the caller wants almost no runtime prompt material.

---

**13. Sub-agent bootstrap behavior**

Sub-agent sessions inject less context.

OpenClaw limits sub-agent bootstrap to:

- `AGENTS.md`
- `TOOLS.md`

This is intentional.

A sub-agent should know project rules and tool rules.

It usually does not need the full user profile, memory, heartbeat behavior, or self-update instructions.

This keeps delegated work **cheaper and less noisy**.

---

**14. Time handling and prompt-cache stability**

The Current Date and Time section is designed carefully.

It includes timezone and time-format guidance.

It avoids injecting a live clock into every prompt.

Why?

Because a live timestamp **changes every run**.

Changing every run can **hurt prompt caching**.

So OpenClaw keeps the prompt-cache boundary stable and tells the model how to get exact time when needed.

When the exact timestamp matters, use:

```text
session_status
```

Relevant config keys:

- `agents.defaults.userTimezone`
- `agents.defaults.timeFormat`

This is a strong production lesson:

> do not put high-churn values into the stable prompt unless the model truly needs them every turn

---

**15. Provider tuning and cache-aware overlays**

Provider plugins are allowed to contribute small additions.

They should not replace the whole OpenClaw system prompt.

Provider plugins can:

- replace named core sections such as `interaction_style`
- replace `tool_call_style`
- replace `execution_bias`
- inject a stable prefix above the prompt-cache boundary
- inject a dynamic suffix below the prompt-cache boundary

This gives **model-family tuning** without losing product control.

Example:

```text
OpenClaw core prompt:
  stable runtime behavior

Provider overlay:
  small model-family guidance for GPT-5, Claude, local models, etc.
```

The OpenAI GPT-5 family overlay is described as keeping core execution rules small while adding guidance such as:

- persona latching
- concise output
- tool discipline
- parallel lookup
- deliverable coverage
- verification
- missing context handling
- terminal-tool hygiene

The architecture rule is:

> provider tuning should be a small overlay, not a hostile takeover of the product prompt

---

**16. Legacy hooks versus provider contributions**

OpenClaw still supports a legacy `before_prompt_build` hook.

It can inject or mutate prompt material globally.

But for model-family behavior, provider contributions are preferred.

Why?

Because provider contributions can be **cache-aware** and scoped to the model family.

Use the right layer:

| Need | Better mechanism |
|---|---|
| Workspace-specific context | Bootstrap files |
| Global prompt mutation | `before_prompt_build` hook |
| Model-family tuning | Provider contribution |
| Runtime security | Tool policy and sandbox |
| User personality | `SOUL.md` |

---

</details>

## 17. Reply Tags 与 Heartbeats

Reply Tags 是可选的、provider 相关的语法指导。

它们帮助模型以 runtime 可解析或路由的格式组织回复。

Heartbeats 同样是可选的。

启用后，OpenClaw 可以包含 heartbeat 提示词与确认行为。

但在以下情况下，正常运行时心跳会被省略：

- 默认 agent 的心跳被禁用
- `agents.defaults.heartbeat.includeSystemPromptSection` 为 false

这样在心搏行为未激活时，提示词保持干净。

---

## 18. Runtime 与 Reasoning 小节

Runtime 小节提供执行环境的一行式紧凑摘要。

它可以包含：

- host
- OS
- Node.js 版本
- 所选模型
- 检测到的 repo root
- thinking level

Reasoning 小节说明可见性级别，并可提示一个 `/reasoning` 开关。

这部分不追求冗长。

它是一个紧凑的 runtime 信号，让模型知道自己运行在何处。

---

## 19. 诊断：检查上下文

当 agent 行为异常时，检查**它实际收到了什么**。

OpenClaw 支持以下命令：

```text
/context list
/context detail
```

用它们检查：

- 注入了哪些文件
- 原始大小与注入大小的对比
- 是否发生了截断
- tool schema 的开销
- 哪个 context 来源在提示词中占主导

这对调试至关重要。

许多「模型问题」实际上是**上下文问题**：

- 某个 bootstrap 文件过大
- 注入了过期的 memory 文件
- 缺少某条项目规则
- provider overlay 过强
- sub-agent 收到的是完整上下文而非最小上下文

在归咎于模型**之前**，先检查提示词装配。

---

## 20. 示例：智能音箱工程 agent

设想一个为基于 Jetson 的产品实验室打造的、由 OpenClaw 驱动的智能音箱助手。

该 agent 可能需要：

- 查阅硬件笔记
- 安排后续测试
- 运行 shell 命令
- 派生一个 sub-agent 去调研 codec
- 记住用户偏好
- 避免不安全的 GPIO 或电源命令
- 汇总日志

一个 OpenClaw 风格的系统提示词可能这样装配：

```text
Tooling:
  Use process tools for long-running audio tests.
  Use cron for future lab reminders.
  Spawn sub-agents for isolated research.

Execution Bias:
  Run available checks before answering.
  Verify file paths and hardware state live.

Safety:
  Do not bypass exec policy.
  Do not change protected power or network settings without approval.

Workspace:
  /home/lab/smart-speaker

Project Context:
  AGENTS.md: lab rules
  TOOLS.md: audio test commands
  USER.md: preferred report format
  MEMORY.md: concise project memory

Runtime:
  Jetson Orin Nano, Linux, local model provider, reasoning medium
```

注意提示词中**没有**什么：

- 每一份历史实验室日志
- 每一个可用的 skill 文件
- 每一个每日 memory 文件
- 策略的原始实现
- 机密

提示词把**运行契约**交给模型。

runtime 执行**危险边界**。

---

## 21. 常见设计错误

### 错误 1：把所有东西都塞进系统提示词

大提示词感觉很有力，但会变得**又慢又嘈杂**。

应改用 retrieval、skills、memory search 和本地文档。

### 错误 2：只依赖提示词安全

如果某个工具动作是危险的，就在 tool 层加以门控。

提示词措辞**不是权限系统**。

### 错误 3：给 sub-agent 完整上下文

sub-agent 应收到有界上下文，用于有界工作。

完整身份与 memory 会分散它们的注意力。

### 错误 4：把实时时间戳放在缓存边界之上

高变更频率的值会降低缓存稳定性。

需要时通过工具暴露精确时间。

### 错误 5：让 provider 插件替换产品提示词

provider overlay 应当做微调。

它们不应拥有产品契约。

---

## 22. 设计练习

为一个本地 AI 硬件实验室助手设计系统提示词策略。

该助手可以：

- 回答问题
- 查阅本地 Markdown 文档
- 运行安全的 shell 命令
- 安排后续事项
- 用 sub-agent 做调研
- 记住硬件清单

回答以下问题：

1. 哪些小节应放进完整提示词？
2. 哪些小节应从 sub-agent 的最小提示词中省略？
3. 哪些文件应通过 bootstrap 注入？
4. 哪些信息应按需检索，而不是注入？
5. 哪些安全要求必须由工具而非提示词文本来强制执行？
6. 哪些 provider 相关的指导应作为小型 overlay？
7. 你将如何检查提示词大小与截断？

如果你回答不了这些问题，你的 agent 系统**尚未达到可运维的成熟度**。

---


<details>
<summary>English original</summary>

**17. Reply Tags and Heartbeats**

Reply Tags are optional provider-specific syntax guidance.

They help models format replies in a way the runtime can parse or route.

Heartbeats are also optional.

When enabled, OpenClaw can include heartbeat prompt and acknowledgement behavior.

But heartbeats are omitted from normal runs when:

- heartbeats are disabled for the default agent
- `agents.defaults.heartbeat.includeSystemPromptSection` is false

This keeps the prompt clean when heartbeat behavior is not active.

---

**18. Runtime and Reasoning sections**

The Runtime section provides a compact one-line summary of the execution environment.

It can include:

- host
- OS
- Node.js version
- selected model
- repo root if detected
- thinking level

The Reasoning section explains visibility level and can hint at a `/reasoning` toggle.

This is not meant to be verbose.

It is a compact runtime signal so the model knows where it is operating.

---

**19. Diagnostics: inspecting context**

When an agent behaves strangely, inspect **what it actually received**.

OpenClaw supports commands such as:

```text
/context list
/context detail
```

Use these to inspect:

- which files were injected
- raw size versus injected size
- whether truncation happened
- tool schema overhead
- which context source dominates the prompt

This is critical for debugging.

Many "model problems" are actually **context problems**:

- a bootstrap file is too large
- a stale memory file is injected
- a project rule is missing
- a provider overlay is too strong
- a sub-agent received full context instead of minimal context

Inspect the prompt assembly **before blaming the model**.

---

**20. Example: smart speaker engineering agent**

Imagine an OpenClaw-powered smart speaker assistant for a Jetson-based product lab.

The agent may need to:

- inspect hardware notes
- schedule follow-up tests
- run shell commands
- spawn a sub-agent to research codecs
- remember user preferences
- avoid unsafe GPIO or power commands
- summarize logs

An OpenClaw-style system prompt might assemble this:

```text
Tooling:
  Use process tools for long-running audio tests.
  Use cron for future lab reminders.
  Spawn sub-agents for isolated research.

Execution Bias:
  Run available checks before answering.
  Verify file paths and hardware state live.

Safety:
  Do not bypass exec policy.
  Do not change protected power or network settings without approval.

Workspace:
  /home/lab/smart-speaker

Project Context:
  AGENTS.md: lab rules
  TOOLS.md: audio test commands
  USER.md: preferred report format
  MEMORY.md: concise project memory

Runtime:
  Jetson Orin Nano, Linux, local model provider, reasoning medium
```

Notice what is **not** in the prompt:

- every past lab log
- every available skill file
- every daily memory file
- raw policy implementation
- secrets

The prompt gives the model the **operating contract**.

The runtime enforces the **dangerous boundaries**.

---

**21. Common design mistakes**

**Mistake 1: putting everything in the system prompt**

Large prompts feel powerful, but they become **slow and noisy**.

Use retrieval, skills, memory search, and local docs instead.

**Mistake 2: relying on prompt safety alone**

If a tool action is dangerous, gate it in the tool layer.

Prompt wording is **not a permission system**.

**Mistake 3: giving sub-agents full context**

Sub-agents should receive bounded context for bounded work.

Full identity and memory can distract them.

**Mistake 4: putting live timestamps above the cache boundary**

High-churn values reduce cache stability.

Expose exact time through tools when needed.

**Mistake 5: letting provider plugins replace the product prompt**

Provider overlays should tune.

They should not own the product contract.

---

**22. Design exercise**

Design a system prompt strategy for a local AI hardware lab assistant.

The assistant can:

- answer questions
- inspect local Markdown docs
- run safe shell commands
- schedule follow-ups
- use a sub-agent for research
- remember hardware inventory

Answer these:

1. Which sections belong in the full prompt?
2. Which sections should be omitted from sub-agent minimal prompts?
3. Which files should be bootstrap-injected?
4. Which information should be retrieved on demand instead of injected?
5. Which safety requirements must be enforced by tools rather than prompt text?
6. Which provider-specific guidance should be a small overlay?
7. How will you inspect prompt size and truncation?

If you cannot answer these, your agent system is **not yet operationally mature**.

---

</details>

## 关键要点

- OpenClaw 负责为每次 agent 运行持有并组装系统提示词。
- 系统提示词是一份 runtime 契约，而不是通用的聊天指令。
- Provider 插件应添加小的缓存感知叠加层，而不是替换整个提示词。
- full 模式用于主 agent；minimal 模式用于有边界的子 agent；none 模式用于少见的低提示词场景。
- Bootstrap 文件提供 Project Context，但必须保持简洁。
- 技能应紧凑列出，并按需加载。
- 需要精确时间时应从工具获取，以便稳定的提示词保持缓存友好。
- 提示词安全是建议性的；硬性安全归属 runtime 控制。
- 上下文检查是一等的调试技能。

---

## 参考

- OpenClaw 系统提示词：[https://openclaw.knidal.com/system-prompt](https://openclaw.knidal.com/system-prompt)
- 案例研究源码仓库：[OpenClaw](https://github.com/openclaw/openclaw)
- 相关 OpenClaw 概念：
  - agent 循环
  - cron 任务
  - 会话与工作区
  - 技能
  - runtime 配置

---

*下一讲：[第 38 讲](Lecture-38.md)*


<details>
<summary>English original</summary>

**Key takeaways**

- OpenClaw owns and assembles the system prompt for each agent run.
- The system prompt is a runtime contract, not a generic chat instruction.
- Provider plugins should add small cache-aware overlays, not replace the full prompt.
- Full mode is for main agents; minimal mode is for bounded sub-agents; none mode is for rare low-prompt cases.
- Bootstrap files provide Project Context, but they must stay concise.
- Skills should be listed compactly and loaded on demand.
- Exact time should come from tools when needed so the stable prompt remains cache-friendly.
- Prompt safety is advisory; hard safety belongs in runtime controls.
- Context inspection is a first-class debugging skill.

---

**References**

- OpenClaw system prompt: [https://openclaw.knidal.com/system-prompt](https://openclaw.knidal.com/system-prompt)
- Case-study source repo: [OpenClaw](https://github.com/openclaw/openclaw)
- Related OpenClaw concepts:
  - agent loop
  - cron jobs
  - sessions and workspaces
  - skills
  - runtime configuration

---

*Next: [Lecture 38](Lecture-38.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-37.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-37.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
