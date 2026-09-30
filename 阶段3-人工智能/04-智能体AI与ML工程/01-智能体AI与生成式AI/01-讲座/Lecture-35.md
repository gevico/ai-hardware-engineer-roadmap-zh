---
title: 第 35 讲 - OpenClaw 案例研究：agent 循环
description: 第 35 讲 - OpenClaw 案例研究：agent 循环
---

# 第 35 讲 - OpenClaw 案例研究：agent 循环

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [第 34 讲](Lecture-34.md) | **下一讲：** [第 36 讲](Lecture-36.md)

---

## 为什么要有这一讲

agent 循环是 agent 系统的**核心执行引擎**。

一旦理解了循环，周边的各个部分就更容易推理：

- cron 决定**何时**运行
- hooks 决定**在哪里拦截**
- 工具决定**有哪些动作可用**
- 会话决定**哪些状态可见**
- agent 循环决定**一次完整运行如何执行**

本讲以 OpenClaw 的 agent 循环作为案例研究。

简短的定义：

> agent 循环是 AI agent 一次完整且受控的执行，从输入到动作再到最终输出，同时保持一致、安全、流式与会话状态

它不只是一次 LLM 调用。

它是一个**有状态的、使用工具的、流式的工作流流水线**。

---

## 学习目标

学完本讲后，你将能够：

1. 解释一次 agent 运行的完整生命周期。
2. 理解 OpenClaw 为何按会话串行化运行。
3. 描述会话锁、队列、流式、工具、hooks 与持久化各自的作用。
4. 解释 `agent`、`agent.wait`、CLI 运行、cron 与 hooks 如何接入同一个核心 runtime。
5. 指出失败、超时、compaction 与提前退出分别发生在哪里。
6. 为你的生产级 assistant 设计一个 agent 循环。

---

## 1. agent 循环究竟是什么

从高层看：

```text
input -> validate -> prepare context -> run model -> call tools -> stream output -> finalize -> persist
```


这看起来很简单，但每一步背后都藏着真实的工程问题。

循环必须回答：

- 这次运行用的是哪个会话？
- 应由哪个模型和 auth profile 来执行？
- 允许使用哪些工具？
- 谁可以写入 transcript？
- 部分回复如何流式输出？
- 工具失败时会怎样？
- 上下文过大时会怎样？
- 用户最终应该看到什么输出？
- 下一轮会保存哪些内容？

所以更准确的定义是：

> agent 循环是带工具与记忆的 AI agent 的一条确定性的、串行化的、可观测的执行流水线

---

## 2. 完整生命周期

OpenClaw 的循环可以理解为如下序列：

```text
1. Intake request
2. Validate parameters
3. Resolve session
4. Queue by session lane
5. Prepare workspace, skills, and bootstrap context
6. Acquire session write lock
7. Assemble prompt
8. Resolve model and auth
9. Run model with streaming
10. Execute tools when requested
11. Shape final reply
12. Persist transcript and metadata
13. Emit lifecycle end or error
```


关键点在于：

> 循环具有系统可以观测到的开始、中间与结束

正因如此，OpenClaw 才能支持 UI 实时更新、`agent.wait`、cron 运行历史、hooks 与调试。

---

## 3. 入口点

同一个循环可以从多个地方启动。

| 入口点 | 示例 | 含义 |
|---|---|---|
| Gateway RPC | `agent` | 将一次 agent 运行入队并快速返回 |
| Gateway RPC wait | `agent.wait` | 等待某次特定运行的生命周期完成 |
| CLI | `openclaw agent ...` | 本地命令行调用 |
| Cron | `openclaw cron run <job-id>` | 定时任务触发一次 agent 运行 |
| Hook | webhook、Gmail、message hook | 事件驱动的触发启动或修改一次运行 |

重要的设计选择是：这些入口点应当**汇聚到同一条 runtime 路径**。

这为产品带来一致的行为：

- 相同的工具策略
- 相同的会话语义
- 相同的日志
- 相同的流式事件
- 相同的失败处理

---

## 4. 接入与校验

在边界处，gateway **校验请求**并解析运行元数据。

它需要确定：

- 目标 agent
- session key 或 session id
- 消息体
- 模型与 thinking 覆盖项
- trace/verbose 设置
- 投递上下文或调用方上下文
- 超时行为

随后它可以返回一个已接受的响应，例如：

```json
{
  "runId": "run_...",
  "acceptedAt": "2026-04-29T12:00:00Z"
}
```


这体现了一种异步优先的设计：

> 接受一次运行并不等于完成一次运行

Gateway 可以**接受工作、将其排队、流式推送进度**，并让客户端另行等待。

---

## 5. 排队与串行化

OpenClaw 最重要的设计规则之一：

> 每条会话 lane 同一时刻只应运行一个 agent 循环

为什么这一点重要：

- 防止两次运行并发写入同一份 transcript
- 避免工具结果交错
- 保持对话历史是确定性的
- 避免重复或自相矛盾的最终回复

心智模型：

```text
session A: run 1 -> run 2 -> run 3
session B: run 1 -> run 2
session C: run 1
```


每条会话 lane 都是**串行**的。

不同的会话 lane 仍可以**各自独立**推进，但受全局并发控制约束。

这就解释了为什么隔离的 cron 会话与 subagent 会话很重要：它们让后台工作得以推进，而**不阻塞用户的主会话 lane**。

---


<details>
<summary>English original</summary>

**Lecture 35 - OpenClaw Case Study: The Agent Loop**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 34](Lecture-34.md) | **Next:** [Lecture 36](Lecture-36.md)

---

**Why this lecture exists**

The agent loop is the **core execution engine** of an agent system.

Once you understand the loop, the surrounding pieces become easier to reason about:

- cron decides **when** to run
- hooks decide **where to intercept**
- tools decide **what actions are available**
- sessions decide **what state is visible**
- the agent loop decides **how one complete run executes**

This lecture uses OpenClaw's agent loop as the case study.

The short definition:

> an agent loop is one complete, controlled execution of an AI agent from input to actions to final output, while preserving consistency, safety, streaming, and session state

It is not only an LLM call.

It is a **stateful, tool-using, streaming workflow pipeline**.

---

**Learning objectives**

By the end of this lecture you will be able to:

1. Explain the full lifecycle of an agent run.
2. Understand why OpenClaw serializes runs per session.
3. Describe the role of session locks, queues, streaming, tools, hooks, and persistence.
4. Explain how `agent`, `agent.wait`, CLI runs, cron, and hooks connect to the same core runtime.
5. Identify where failures, timeouts, compaction, and early exits happen.
6. Design an agent loop for your own production assistant.

---

**1. What an agent loop really is**

At a high level:

```text
input -> validate -> prepare context -> run model -> call tools -> stream output -> finalize -> persist
```

That looks simple, but each step hides real engineering concerns.

The loop must answer:

- which session is this run using?
- what model and auth profile should execute it?
- what tools are allowed?
- who can write to the transcript?
- how are partial replies streamed?
- what happens if a tool fails?
- what happens if context is too large?
- what final output should the user see?
- what gets saved for the next turn?

So a more accurate definition is:

> the agent loop is a deterministic, serialized, observable execution pipeline for AI agents with tools and memory

---

**2. The full lifecycle**

OpenClaw's loop can be understood as this sequence:

```text
1. Intake request
2. Validate parameters
3. Resolve session
4. Queue by session lane
5. Prepare workspace, skills, and bootstrap context
6. Acquire session write lock
7. Assemble prompt
8. Resolve model and auth
9. Run model with streaming
10. Execute tools when requested
11. Shape final reply
12. Persist transcript and metadata
13. Emit lifecycle end or error
```

The key point:

> the loop has a beginning, middle, and end that the system can observe

That is why OpenClaw can support live UI updates, `agent.wait`, cron run history, hooks, and debugging.

---

**3. Entry points**

The same loop can start from several places.

| Entry point | Example | Meaning |
|---|---|---|
| Gateway RPC | `agent` | enqueue an agent run and return quickly |
| Gateway RPC wait | `agent.wait` | wait for lifecycle completion of a specific run |
| CLI | `openclaw agent ...` | local command-line invocation |
| Cron | `openclaw cron run <job-id>` | scheduled job triggers an agent run |
| Hook | webhook, Gmail, message hook | event-driven trigger starts or modifies a run |

The important design choice is that these entry points should **converge into one runtime path**.

That gives the product consistent behavior:

- same tool policy
- same session semantics
- same logging
- same streaming events
- same failure handling

---

**4. Intake and validation**

At the boundary, the gateway **validates the request** and resolves run metadata.

It needs to determine:

- target agent
- session key or session id
- message body
- model and thinking overrides
- trace/verbose settings
- delivery or caller context
- timeout behavior

Then it can return an accepted response such as:

```json
{
  "runId": "run_...",
  "acceptedAt": "2026-04-29T12:00:00Z"
}
```

This reflects an async-first design:

> accepting a run is not the same as finishing a run

The Gateway can **accept work, queue it, stream progress**, and let clients wait separately.

---

**5. Queueing and serialization**

One of the most important OpenClaw design rules:

> only one agent loop should run per session lane at a time

Why this matters:

- prevents two runs from writing the same transcript concurrently
- avoids interleaved tool results
- keeps conversation history deterministic
- avoids duplicate or contradictory final replies

The mental model:

```text
session A: run 1 -> run 2 -> run 3
session B: run 1 -> run 2
session C: run 1
```

Each session lane is **serial**.

Different session lanes can still make progress **independently**, subject to global concurrency controls.

This explains why isolated cron and subagent sessions matter: they let background work proceed without **blocking the user's main session lane**.

---

</details>

## 6. 会话写锁

排队处理的是 **逻辑顺序**。

锁处理的是 **实际的文件/状态变更**。

OpenClaw 用 **进程感知的会话写锁** 保护 transcript 写入。

这一点很重要，因为可能存在多个进程：

- 网关进程
- CLI 进程
- 维护或 doctor 命令
- 测试 worker
- 自动化脚本

规则是：

> 任何 transcript 的写入、重写、压缩或截断，都必须获取同一把会话写锁

默认情况下，该锁应 **不可重入**。有意嵌套同一把锁的代码必须显式选择启用。

这是一条重要的生产经验：

> 会话状态是共享可变状态，因此需要真正的并发边界

---

## 7. 会话与工作区准备

模型被调用之前，loop 会准备运行环境。

典型的准备包括：

- 解析工作区
- 按需创建工作区
- 在沙箱化时应用沙箱工作区根目录
- 加载技能快照
- 解析 bootstrap 上下文文件
- 准备环境变量
- 准备会话管理器

正是在这里，“agent 行为” 变得 **不只是 prompt**。

模型通过以下内容观察世界：

- 工作区
- 工具
- 技能
- bootstrap 上下文
- 会话历史
- 策略与 runtime 设置

**准备不当** 会导致后续 agent 行为令人困惑。

---

## 8. Prompt 组装

模型看到的并不是一个叫做 “the prompt” 的字符串。

它看到的是 **组装后的上下文**。

OpenClaw 风格的 prompt 素材包括：

```text
base system prompt
+ skills prompt
+ bootstrap context
+ session history
+ per-run overrides
+ hook-injected context
```


runtime 还必须考虑：

- 特定模型的 token 上限
- 压缩预留 token
- 截断行为
- 工具 schema
- 推理或 thinking 配置

重要规则：

> prompt 组装是 runtime 行为，而不只是写 prompt

生产系统应当能够回答：

- 哪个模型看到了哪些指令？
- 当时生效的是哪个技能快照？
- 注入了哪些 bootstrap 文件？
- 哪个 hook 修改了 prompt？
- 上下文为什么被压缩？

---

## 9. 模型执行

在 OpenClaw 的架构中，内嵌的 agent runner 负责模型执行。

从概念上讲，这个阶段会做：

- 解析 provider 与模型
- 解析 auth profile
- 发起模型请求
- 订阅模型与工具事件
- 强制执行中止与超时行为
- 返回最终 payload 与用量元数据

这是 “thinking” 阶段，但它仍然由 **runtime 控制**。

模型可能会：

- 输出 assistant 文本
- 请求工具
- 在支持时流式输出推理片段
- 触发空闲超时
- 触发模型切换行为
- 因 provider 或网络错误而失败

loop 把这种不确定性包裹在一份 **稳定的契约** 中。

---

## 10. 流式事件

OpenClaw 流式传输的内容不止最终文本。

一种有用的事件模型是：

| 流 | 承载内容 |
|---|---|
| `lifecycle` | `start`、`end`、`error` |
| `assistant` | text delta、block reply、可选 reasoning chunk |
| `tool` | 工具 start、update、result、end |

这正是实时 UI 与 channel 更新得以实现的原因。

例如：

```text
lifecycle:start
assistant:delta "I will check..."
tool:start read_file
tool:end read_file
assistant:delta "The file shows..."
lifecycle:end
```


流式传输很重要，因为长时间运行的 agent 工作不应让人感觉像 **黑盒**。

它也为运维人员提供了调试手段，用来定位一次运行卡在哪里：

- 没有 lifecycle start 意味着接入/排队问题
- 有 lifecycle start 但没有 assistant delta 意味着模型或 prompt 问题
- 有 tool start 但没有 tool end 意味着工具执行问题
- lifecycle error 意味着 runtime 遇到了终态故障

---

## 11. 工具执行

当模型请求工具时，loop 就变成一个 **动作引擎**。

基本的工具路径：

```text
model requests tool
-> emit tool start
-> validate tool call
-> run tool
-> sanitize result
-> emit tool result
-> feed result back to model
```


工具结果应当：

- 受大小限制
- 经过净化
- 可安全持久化
- 可安全流式传输
- 与发起它的调用相关联

有些工具可以直接向用户发送消息。

这会引入一个回复塑形问题：

如果某个消息工具已经发出了有用的答案，那么最终的 assistant 确认可能就是多余的。

OpenClaw 会跟踪消息工具的发送，从而抑制 **重复确认**。

---


<details>
<summary>English original</summary>

**6. Session write locks**

Queueing handles **logical order**.

Locks handle **actual file/state mutation**.

OpenClaw protects transcript writes with a **process-aware session write lock**.

That matters because multiple processes may exist:

- Gateway process
- CLI process
- maintenance or doctor commands
- test workers
- automation scripts

The rule:

> any transcript write, rewrite, compaction, or truncation must acquire the same session write lock

By default, the lock should **not be reentrant**. Code that intentionally nests the same lock must opt in explicitly.

This is a strong production lesson:

> session state is shared mutable state, so it needs a real concurrency boundary

---

**7. Session and workspace preparation**

Before the model is called, the loop prepares the run environment.

Typical preparation includes:

- resolving workspace
- creating workspace if needed
- applying sandbox workspace root when sandboxed
- loading a skills snapshot
- resolving bootstrap context files
- preparing environment variables
- preparing the session manager

This is where "agent behavior" becomes **more than a prompt**.

The model sees the world through:

- workspace
- tools
- skills
- bootstrap context
- session history
- policy and runtime settings

**Bad preparation** leads to confusing agent behavior later.

---

**8. Prompt assembly**

The model does not see one string called "the prompt."

It sees **assembled context**.

OpenClaw-style prompt material includes:

```text
base system prompt
+ skills prompt
+ bootstrap context
+ session history
+ per-run overrides
+ hook-injected context
```

The runtime must also account for:

- model-specific token limits
- compaction reserve tokens
- truncation behavior
- tool schemas
- reasoning or thinking configuration

The important rule:

> prompt assembly is runtime behavior, not just prompt writing

A production system should be able to answer:

- what model saw which instructions?
- which skill snapshot was active?
- which bootstrap files were injected?
- which hook changed the prompt?
- why was context compacted?

---

**9. Model execution**

In OpenClaw's architecture, the embedded agent runner handles model execution.

Conceptually, this phase does:

- resolve provider and model
- resolve auth profile
- start the model request
- subscribe to model and tool events
- enforce abort and timeout behavior
- return final payloads and usage metadata

This is the "thinking" phase, but it is still **runtime-controlled**.

The model may:

- emit assistant text
- request tools
- stream reasoning chunks when supported
- hit idle timeout
- trigger model switch behavior
- fail with provider or network errors

The loop wraps that uncertainty in a **stable contract**.

---

**10. Streaming events**

OpenClaw streams more than final text.

A useful event model is:

| Stream | What it carries |
|---|---|
| `lifecycle` | `start`, `end`, `error` |
| `assistant` | text deltas, block replies, optional reasoning chunks |
| `tool` | tool start, update, result, end |

This is what makes live UI and channel updates possible.

For example:

```text
lifecycle:start
assistant:delta "I will check..."
tool:start read_file
tool:end read_file
assistant:delta "The file shows..."
lifecycle:end
```

Streaming matters because long-running agent work should not feel like a **black box**.

It also gives operators a way to debug where a run got stuck:

- no lifecycle start means intake/queue issue
- lifecycle start but no assistant deltas means model or prompt issue
- tool start with no tool end means tool execution issue
- lifecycle error means the runtime saw a terminal failure

---

**11. Tool execution**

When the model requests a tool, the loop becomes an **action engine**.

The basic tool path:

```text
model requests tool
-> emit tool start
-> validate tool call
-> run tool
-> sanitize result
-> emit tool result
-> feed result back to model
```

Tool results should be:

- size-limited
- sanitized
- safe to persist
- safe to stream
- connected to the originating call

Some tools can send messages directly to users.

That introduces a reply-shaping issue:

if a messaging tool already sent the useful answer, the final assistant confirmation may be redundant.

OpenClaw tracks messaging tool sends so **duplicate confirmations** can be suppressed.

---

</details>

## 12. Hooks 作为拦截点

Hooks 让产品能**拦截 loop**。

OpenClaw 有内部 Gateway hooks 和 plugin hooks。关键概念是 hooks 可以运行在不同的生命周期点。

有用的 hook 分类：

| Hook 区域 | 示例 | 用途 |
|---|---|---|
| 模型选择 | `before_model_resolve` | 选择或覆盖 provider/model |
| Prompt 组装 | `before_prompt_build` | 注入上下文或系统提示词素材 |
| 回复控制 | `before_agent_reply` | 认领、覆盖或静默某一轮 |
| 工具策略 | `before_tool_call` | 阻断或修改工具调用 |
| 工具输出 | `after_tool_call`、`tool_result_persist` | 审计或转换工具结果 |
| 消息收发 | `message_received`、`message_sending`、`message_sent` | 路由、取消或审计消息 |
| 生命周期 | `agent_end`、`gateway_start`、`gateway_stop` | 观察系统状态 |
| 压缩 | `before_compaction`、`after_compaction` | 检查摘要行为 |

Terminal hook 决策很重要。

示例：

```json
{ "block": true }
```

或：

```json
{ "cancel": true }
```

它们会终止其 hook 链中优先级更低的处理。

生产经验：

> hooks 之所以强大，是因为它们能改变 runtime 行为，因此其顺序与 terminal 语义必须显式定义

---

## 13. 回复整形

一次 run 结束时，runtime 决定什么是**用户可见**的。

最终 payload 组装可能包含：

- assistant 文本
- block 回复
- 在 verbose 且允许时的工具摘要
- 需要时的错误消息

随后 runtime 施加清理规则：

- 抑制诸如 `NO_REPLY` 或 `no_reply` 这类精确的静默 token
- 移除消息工具的重复确认
- 若已无任何可渲染输出且用户尚未看到回复，则发出兜底的工具错误回复
- 当存在更好的后继结果时，避免重放仅含确认的陈旧文本

这个阶段存在，是因为模型产出的文本常常**并不是正确的最终用户输出**。

agent loop 必须把输出整形为**产品行为**。

---

## 14. 持久化

run 结束后，系统写入：

- 用户消息
- assistant 消息
- 工具调用与工具结果
- 元数据
- 用量信息
- 生命周期状态

持久化必须在 **session 写锁**下进行。

这保护 transcript 免受**竞态**影响，并让后续轮次获得一致的历史。

实用规则：

> 若某内容会影响未来上下文，就要谨慎持久化

这也是为什么 streaming 与持久化是两件不同的事。用户可能在最终 transcript 完全提交之前就看到了部分输出。

---

## 15. `agent.wait`

`agent` RPC 启动一次 run。

`agent.wait` RPC 等待一次 run 到达生命周期 `end` 或 `error`。

等待结果在概念上形如：

```json
{
  "status": "ok",
  "startedAt": "2026-04-29T12:00:00Z",
  "endedAt": "2026-04-29T12:00:10Z"
}
```

或：

```json
{
  "status": "timeout"
}
```

重要区别：

> `agent.wait` 超时并不一定停止 agent run

它只意味着**等待方停止等待**。

这跟观察后台任务是一个概念：任务仍在继续，而你的终端可能已超时。

---

## 16. 压缩与重试

agent 上下文会增长。

最终 transcript 可能超出模型上下文窗口，或超出配置的预留预算。

此时 loop 可能触发压缩：

```text
detect context pressure
-> emit compaction event
-> summarize or rewrite context
-> retry the run
```

重试时，runtime 必须重置内存缓冲区和工具摘要，以免输出重复。

这一点很重要，因为压缩**不只是存储任务**。

它影响：

- 模型看到什么
- 用户看到什么
- 哪些消息保持详细
- 重跑是否重复旧输出

---

## 17. 超时与中止

OpenClaw 风格的系统有多层超时。

| 超时 | 影响对象 |
|---|---|
| agent runtime 超时 | agent run 的最长持续时间 |
| LLM 空闲超时 | 无 chunk 到达时中止模型请求 |
| `agent.wait` 超时 | 调用方等待多久 |
| cron 外层超时 | 调度任务级控制 |

OpenClaw 有文档记载的默认值包括：

- agent runtime 默认值可通过 `agents.defaults.timeoutSeconds` 配置，文档示例约为长达 48 小时的 run
- `agent.wait` 默认是一个很短的等待窗口
- LLM 空闲超时可单独配置
- cron 触发的 run 在未显式设置 LLM/agent 超时时，可能依赖外层 cron 控制

经验：

> 超时语义必须说明哪些会被取消、哪些只是停止等待

缺少这一区分，运维人员就会**误读系统行为**。

---


<details>
<summary>English original</summary>

**12. Hooks as interception points**

Hooks let the product **intercept the loop**.

OpenClaw has internal Gateway hooks and plugin hooks. The important concept is that hooks can run at different lifecycle points.

Useful hook categories:

| Hook area | Example | Purpose |
|---|---|---|
| Model selection | `before_model_resolve` | choose or override provider/model |
| Prompt assembly | `before_prompt_build` | inject context or system prompt material |
| Reply control | `before_agent_reply` | claim, override, or silence a turn |
| Tool policy | `before_tool_call` | block or modify a tool call |
| Tool output | `after_tool_call`, `tool_result_persist` | audit or transform tool results |
| Messaging | `message_received`, `message_sending`, `message_sent` | route, cancel, or audit messages |
| Lifecycle | `agent_end`, `gateway_start`, `gateway_stop` | observe system state |
| Compaction | `before_compaction`, `after_compaction` | inspect summarization behavior |

Terminal hook decisions are important.

Examples:

```json
{ "block": true }
```

or:

```json
{ "cancel": true }
```

These stop lower-priority handling in their hook chain.

The production lesson:

> hooks are powerful because they can change runtime behavior, so their ordering and terminal semantics must be explicit

---

**13. Reply shaping**

At the end of a run, the runtime decides what is **user-visible**.

Final payload assembly may include:

- assistant text
- block replies
- tool summaries when verbose and allowed
- error messages when needed

Then the runtime applies cleanup rules:

- suppress exact silent tokens such as `NO_REPLY` or `no_reply`
- remove messaging-tool duplicate confirmations
- emit a fallback tool error reply if no renderable output remains and the user has not already seen a reply
- avoid replaying stale acknowledgement-only text when a better descendant result exists

This phase exists because models often produce text that is **not the right final user output**.

The agent loop must shape output into **product behavior**.

---

**14. Persistence**

After the run, the system writes:

- user message
- assistant message
- tool calls and tool results
- metadata
- usage information
- lifecycle state

Persistence must happen under the **session write lock**.

This protects the transcript from **races** and gives future turns a coherent history.

The practical rule:

> if it affects future context, persist it carefully

This is also why streaming and persistence are different concerns. A user can see partial output before the final transcript is fully committed.

---

**15. `agent.wait`**

The `agent` RPC starts a run.

The `agent.wait` RPC waits for a run to reach lifecycle `end` or `error`.

A wait result looks conceptually like:

```json
{
  "status": "ok",
  "startedAt": "2026-04-29T12:00:00Z",
  "endedAt": "2026-04-29T12:00:10Z"
}
```

or:

```json
{
  "status": "timeout"
}
```

Important distinction:

> `agent.wait` timeout does not necessarily stop the agent run

It only means the **waiter stopped waiting**.

This is the same concept as watching a background job: your terminal can time out while the job continues.

---

**16. Compaction and retry**

Agent context grows.

Eventually the transcript may become too large for the model context window or for the configured reserve budget.

When that happens, the loop may trigger compaction:

```text
detect context pressure
-> emit compaction event
-> summarize or rewrite context
-> retry the run
```

On retry, the runtime must reset in-memory buffers and tool summaries so output does not duplicate.

This matters because compaction is **not just a storage task**.

It affects:

- what the model sees
- what the user sees
- which messages remain detailed
- whether the rerun repeats old output

---

**17. Timeouts and aborts**

OpenClaw-style systems have multiple timeout layers.

| Timeout | What it affects |
|---|---|
| agent runtime timeout | maximum agent run duration |
| LLM idle timeout | aborts a model request when no chunks arrive |
| `agent.wait` timeout | how long the caller waits |
| cron outer timeout | scheduled-job-level control |

OpenClaw's documented defaults include:

- agent runtime default can be configured through `agents.defaults.timeoutSeconds`, with documented examples around long 48-hour runs
- `agent.wait` defaults to a short wait window
- LLM idle timeout can be configured separately
- cron-triggered runs may rely on outer cron control when no explicit LLM/agent timeout is set

The lesson:

> timeout semantics must say what gets cancelled and what only stops waiting

Without that distinction, operators **misread system behavior**.

---

</details>

## 18. run 可能提前结束的情形

agent loop 可能因以下原因提前结束：

- agent runtime 超时
- abort 信号
- gateway 断开
- RPC 等待超时
- 模型失败
- 工具失败
- hook 阻止或取消决定
- compaction 失败

良好的 runtime 设计仍会尝试发出 lifecycle 事件：

```text
lifecycle:error
```

这样客户端能拿到最终状态，也有助于 `agent.wait`、UI、cron 和日志对发生的事情达成一致。

---

## 19. 这与 cron 如何关联

现在，前面关于 cron 的那一讲就更容易理解了。

cron 本身并不做 **agent 工作**。

cron **调度这份工作**：

```text
cron schedule
-> due job
-> agent RPC
-> agent loop
-> delivery/logging
```

所以：

- cron 回答 **何时**
- agent loop 回答 **如何**
- 工具回答 **做哪些动作**
- hook 回答 **策略在何处介入**
- session 回答 **什么状态**

这就是严肃的持久化 agent 背后的架构模式。

---

## 20. 示例：一次 OpenClaw 风格的 run

设想用户发送：

> 检查该 repo，并总结今天失败的测试。

loop 的行为可能如下：

```text
1. Gateway receives message.
2. Gateway resolves session key.
3. Run enters that session lane queue.
4. Session lock is acquired.
5. Workspace and skills are prepared.
6. Prompt is assembled with history and bootstrap context.
7. Model starts streaming.
8. Model calls a shell/test-inspection tool.
9. Tool result is sanitized and streamed.
10. Model writes a final summary.
11. Duplicate tool-send confirmations are suppressed.
12. Transcript and metadata are persisted.
13. lifecycle:end is emitted.
14. The chat channel sends the final response.
```

用户感受到的是 **一条回复**。

系统执行的是一条 **受控的、类事务的 runtime 路径**。

---

## 21. 设计练习

为一个本地工程助手设计 agent loop。

填写下表：

| Area | Your design |
|---|---|
| Entry points | CLI、Web UI、Slack、cron |
| Session lane rule | 每个 session 一次只允许一个 run |
| Global queue | 最多 2 个并发模型 run |
| Write lock | transcript 写入和 compaction 必需 |
| Prompt inputs | base prompt、技能、repo context、session history |
| Tools | read、search、test runner、issue lookup |
| Tool policy | write 和 deploy 需要审批 |
| Hooks | 拦截来自未配对渠道的 deploy |
| Streaming | lifecycle、assistant、tool |
| Final reply shaping | 抑制 `NO_REPLY`，移除重复确认 |
| Timeout model | wait timeout 与 runtime timeout 分离 |
| Persistence | transcript、工具输出、用量、run 元数据 |

这个练习的价值在于，它迫使你把 agent 当作 **基础设施**，而不是一次 API 调用。

---

## 要点

- agent loop 是 agent 产品的执行引擎。
- 它是一条从输入到持久化最终状态的串行、可观测流水线。
- 按 session 排队可避免 transcript 与工具之间的竞争。
- session 写锁跨进程保护持久状态。
- prompt 组装、模型执行、工具执行、流式输出、回复整形和持久化是各自独立的 runtime 关注点。
- hook 是 loop 内部的策略点和扩展点。
- `agent.wait` 等待 lifecycle 完成；它并不定义整个 run。
- cron 触发 agent loop，但 cron 不是 agent loop。

---

## 参考资料

- OpenClaw agent loop：[https://openclaw.knidal.com/agent-loop](https://openclaw.knidal.com/agent-loop)
- 案例研究源 repo：[OpenClaw](https://github.com/openclaw/openclaw)
- OpenClaw 概念：
  - `docs/automation/cron-jobs.md`
  - `docs/cli/cron.md`
  - `docs/tools/subagents.md`
  - `docs/concepts/session.md`
  - `docs/reference/session-management-compaction.md`

---

*下一讲：[Lecture 36](Lecture-36.md)*


<details>
<summary>English original</summary>

**18. Where runs can end early**

An agent loop may end early because of:

- agent runtime timeout
- abort signal
- gateway disconnect
- RPC wait timeout
- model failure
- tool failure
- hook block or cancel decision
- compaction failure

Good runtime design still attempts to emit lifecycle events:

```text
lifecycle:error
```

That gives clients a final state and helps `agent.wait`, UI, cron, and logs agree about what happened.

---

**19. How this connects to cron**

Now the previous lecture on cron becomes easier.

Cron does not do the **agent work** itself.

Cron **schedules the work**:

```text
cron schedule
-> due job
-> agent RPC
-> agent loop
-> delivery/logging
```

So:

- cron answers **when**
- the agent loop answers **how**
- tools answer **what actions**
- hooks answer **where policy intervenes**
- sessions answer **what state**

This is the architecture pattern behind serious persistent agents.

---

**20. Example: one OpenClaw-style run**

Imagine a user sends:

> Check the repo and summarize today's failing tests.

The loop might behave like this:

```text
1. Gateway receives message.
2. Gateway resolves session key.
3. Run enters that session lane queue.
4. Session lock is acquired.
5. Workspace and skills are prepared.
6. Prompt is assembled with history and bootstrap context.
7. Model starts streaming.
8. Model calls a shell/test-inspection tool.
9. Tool result is sanitized and streamed.
10. Model writes a final summary.
11. Duplicate tool-send confirmations are suppressed.
12. Transcript and metadata are persisted.
13. lifecycle:end is emitted.
14. The chat channel sends the final response.
```

The user experiences **one reply**.

The system executed a **controlled transaction-like runtime path**.

---

**21. Design exercise**

Design an agent loop for a local engineering assistant.

Fill in this table:

| Area | Your design |
|---|---|
| Entry points | CLI, Web UI, Slack, cron |
| Session lane rule | one run at a time per session |
| Global queue | max 2 concurrent model runs |
| Write lock | required for transcript writes and compaction |
| Prompt inputs | base prompt, skills, repo context, session history |
| Tools | read, search, test runner, issue lookup |
| Tool policy | write and deploy require approval |
| Hooks | block deploy from unpaired channels |
| Streaming | lifecycle, assistant, tool |
| Final reply shaping | suppress `NO_REPLY`, remove duplicate confirmations |
| Timeout model | wait timeout separate from runtime timeout |
| Persistence | transcript, tool outputs, usage, run metadata |

The value of this exercise is that it forces you to treat the agent as **infrastructure**, not as a single API call.

---

**Key takeaways**

- The agent loop is the execution engine of an agent product.
- It is a serialized, observable pipeline from input to persisted final state.
- Per-session queueing prevents transcript and tool races.
- Session write locks protect durable state across processes.
- Prompt assembly, model execution, tool execution, streaming, reply shaping, and persistence are separate runtime concerns.
- Hooks are policy and extension points inside the loop.
- `agent.wait` waits for lifecycle completion; it does not define the entire run.
- Cron triggers agent loops, but cron is not the agent loop.

---

**References**

- OpenClaw agent loop: [https://openclaw.knidal.com/agent-loop](https://openclaw.knidal.com/agent-loop)
- Case-study source repo: [OpenClaw](https://github.com/openclaw/openclaw)
- OpenClaw concepts:
  - `docs/automation/cron-jobs.md`
  - `docs/cli/cron.md`
  - `docs/tools/subagents.md`
  - `docs/concepts/session.md`
  - `docs/reference/session-management-compaction.md`

---

*Next: [Lecture 36](Lecture-36.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-35.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-35.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
