---
title: 第 36 讲 - OpenClaw 案例研究：Cron、定时 agent 运行与自动化可靠性
description: 第 36 讲 - OpenClaw 案例研究：Cron、定时 agent 运行与自动化可靠性
---

# 第 36 讲 - OpenClaw 案例研究：Cron、定时 agent 运行与自动化可靠性

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [第 35 讲](Lecture-35.md) | **下一讲：** [第 37 讲](Lecture-37.md)

---

## 为什么需要这一讲

从外部看，Cron 很简单：

> 在预定的时间运行某个东西

但在 agent 系统中，调度**不只是一个定时器**。

被调度的任务可能需要：

- 一个模型
- 一个会话
- 一个 prompt
- 工具权限
- 投递路由
- 重试
- 日志
- 失败通知
- 清理

OpenClaw 是一个有用的案例，因为它的 cron 系统不只是经典的 Unix cron。它是**面向 agent 工作的调度器**。

更好的心智模型是：

```text
traditional cron:
  schedule -> run command -> exit

OpenClaw cron:
  schedule -> run agent task -> manage session -> deliver output -> retry -> log -> clean up
```


本讲要讲的是介于 “always-on gateway” 与 “agent 执行” 之间的**调度层**。

---

## 学习目标

学完本讲后，你将能够：

1. 解释 cron 表达式的含义。
2. 比较一次性、间隔和 cron 三种调度。
3. 解释 OpenClaw 如何把调度转换成 agent 运行。
4. 为定时自动化选择合适的会话模式。
5. 理解投递回退、失败告警、重试、日志和保留策略。
6. 调试一个触发了却没有可见输出的 cron 任务。
7. 解释为什么 cron 校验应放在任务创建之前。

---

## 1. 从第一性原理看 Cron

Cron 是一个**调度器**。

它的任务是回答一个问题：

> 这个任务应该在什么时候运行？

经典 Unix cron 有一个**后台守护进程**，它读取任务定义，并在匹配的时间启动 shell 命令。

示例：

```cron
0 7 * * *
```


它的含义是：

> 每天 07:00 运行

常见的 5 字段格式是：

```text
minute hour day-of-month month day-of-week
```


用图示表示：

```text
minute        0-59
| hour        0-23
| | day       1-31
| | | month   1-12
| | | | week  0-6 or names, depending on parser
| | | | |
* * * * *
```


常见示例：

| 表达式 | 含义 |
|---|---|
| `0 * * * *` | 每小时 |
| `*/10 * * * *` | 每 10 分钟 |
| `0 9 * * 1` | 每周一 09:00 |
| `0 0 1 * *` | 每月第一天 |
| `0 7 * * *` | 每天 07:00 |

OpenClaw 通过 Croner 支持 5 字段和 6 字段的 cron 表达式。6 字段表达式包含秒。

---

## 2. 第一个陷阱：cron 是一门语言，而不只是一个字符串

学生常把 cron 表达式当作无害的文本。

事实并非如此。

它是一门**小型调度语言**，带有各种边界情况：

- 时区解释
- 带秒与不带秒
- day-of-month 与 day-of-week 的行为
- 解析器特有的语法
- 无效范围
- 不可能存在的日期

OpenClaw 的文档特别指出 Croner 的一个行为：

当 day-of-month 和 day-of-week 都不是通配符时，Croner 遵循 Vixie cron 风格的 **OR** 逻辑。

示例：

```cron
0 9 15 * 1
```


很多人以为：

> 只有当 15 号是周一时，才在 09:00 运行

但 cron 的通常行为是：

> 每个 15 号的 09:00 都运行，且每个周一的 09:00 都运行

这是一条系统设计上的教训：

> 调度语法必须被当作可执行配置来对待

无效或出乎意料的调度应当被**尽早捕获**，在任务变成持久化状态之前。

---

## 3. OpenClaw cron 增加了什么

OpenClaw cron 运行在 Gateway 进程内部。

它持久化：

- `~/.openclaw/cron/jobs.json` 中的任务定义
- `~/.openclaw/cron/jobs-state.json` 中的 runtime 状态
- `~/.openclaw/cron/runs/` 下的运行历史

这一点很重要，因为定时任务应当能**在 Gateway 重启后存活**。

OpenClaw cron 还会创建后台任务记录，因此定时 agent 运行可以当作运维工作来查看，而不只是一个隐藏的定时器回调。

其 runtime 形态是：

```text
[ Gateway scheduler ]
        |
        v
[ Job definition + runtime state ]
        |
        v
[ Agent execution or system event ]
        |
        v
[ Delivery router ]
        |
        v
[ Run log + retry/failure policy ]
```


这就是为什么它更像一个**小型 workflow 系统**，而不是一个普通的 crontab。

---


<details>
<summary>English original</summary>

**Lecture 36 - OpenClaw Case Study: Cron, Scheduled Agent Runs, and Automation Reliability**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 35](Lecture-35.md) | **Next:** [Lecture 37](Lecture-37.md)

---

**Why this lecture exists**

Cron looks simple from the outside:

> run something at a scheduled time

But in an agent system, scheduling is **not just a timer**.

The scheduled job may need:

- a model
- a session
- a prompt
- tool permissions
- delivery routing
- retries
- logs
- failure notifications
- cleanup

OpenClaw is a useful case study because its cron system is not just classic Unix cron. It is a **scheduler for agent work**.

The better mental model is:

```text
traditional cron:
  schedule -> run command -> exit

OpenClaw cron:
  schedule -> run agent task -> manage session -> deliver output -> retry -> log -> clean up
```

This lecture teaches the **scheduling layer** that sits between "always-on gateway" and "agent execution."

---

**Learning objectives**

By the end of this lecture you will be able to:

1. Explain what cron expressions mean.
2. Compare one-shot, interval, and cron schedules.
3. Explain how OpenClaw turns schedules into agent runs.
4. Choose the right session mode for scheduled automation.
5. Understand delivery fallback, failure alerts, retries, logs, and retention.
6. Debug a cron job that fired but produced no visible output.
7. Explain why cron validation belongs before job creation.

---

**1. Cron from first principles**

Cron is a **scheduler**.

Its job is to answer one question:

> when should this task run?

Classic Unix cron has a **background daemon** that reads job definitions and launches shell commands at matching times.

Example:

```cron
0 7 * * *
```

This means:

> run at 07:00 every day

The common 5-field format is:

```text
minute hour day-of-month month day-of-week
```

Written as a diagram:

```text
minute        0-59
| hour        0-23
| | day       1-31
| | | month   1-12
| | | | week  0-6 or names, depending on parser
| | | | |
* * * * *
```

Common examples:

| Expression | Meaning |
|---|---|
| `0 * * * *` | every hour |
| `*/10 * * * *` | every 10 minutes |
| `0 9 * * 1` | every Monday at 09:00 |
| `0 0 1 * *` | first day of every month |
| `0 7 * * *` | every day at 07:00 |

OpenClaw supports 5-field and 6-field cron expressions through Croner. A 6-field expression includes seconds.

---

**2. The first trap: cron is a language, not just a string**

Students often treat cron expressions like harmless text.

They are not.

They are a **small scheduling language** with edge cases:

- timezone interpretation
- seconds vs no seconds
- day-of-month and day-of-week behavior
- parser-specific syntax
- invalid ranges
- impossible dates

OpenClaw's docs call out a specific Croner behavior:

when both day-of-month and day-of-week are non-wildcard, Croner follows Vixie cron-style **OR** logic.

Example:

```cron
0 9 15 * 1
```

Many people expect:

> run at 09:00 on the 15th only when it is Monday

But the usual cron behavior is:

> run at 09:00 on every 15th and at 09:00 on every Monday

That is a system-design lesson:

> schedule syntax must be treated as executable configuration

Invalid or surprising schedules should be **caught early**, before the job becomes durable state.

---

**3. What OpenClaw cron adds**

OpenClaw cron runs inside the Gateway process.

It persists:

- job definitions in `~/.openclaw/cron/jobs.json`
- runtime state in `~/.openclaw/cron/jobs-state.json`
- run history under `~/.openclaw/cron/runs/`

That matters because a scheduled task should **survive a Gateway restart**.

OpenClaw cron also creates background task records, so a scheduled agent run can be inspected as operational work, not just as a hidden timer callback.

The runtime shape is:

```text
[ Gateway scheduler ]
        |
        v
[ Job definition + runtime state ]
        |
        v
[ Agent execution or system event ]
        |
        v
[ Delivery router ]
        |
        v
[ Run log + retry/failure policy ]
```

This is why it is closer to a **small workflow system** than to a plain crontab.

---

</details>

## 4. OpenClaw 中的调度类型

OpenClaw 有三种调度类型。

| 类型 | CLI flag | 适用场景 |
|---|---|---|
| `at` | `--at` | 一次性提醒或一次性自动化 |
| `every` | `--every` | 固定间隔检查 |
| `cron` | `--cron` | 日历式调度 |

一次性示例：

```bash
openclaw cron add \
  --name "Calendar check" \
  --at "20m" \
  --session main \
  --system-event "Next heartbeat: check calendar." \
  --wake now
```

循环 cron 示例：

```bash
openclaw cron add \
  --name "Morning brief" \
  --cron "0 7 * * *" \
  --tz "America/Los_Angeles" \
  --session isolated \
  --message "Summarize overnight updates." \
  --announce
```

默认情况下，一次性任务在成功后即删除。若希望在任务完成后保留该任务，使用 `--keep-after-run`。

整点重复的调度可能会被错开，以削峰。需要精确的 cron 边界时使用 `--exact`，需要显式的分散窗口时使用 `--stagger 30s`。

---

## 5. 实际运行的是什么

传统 cron 通常运行一条命令：

```text
run this shell script at 07:00
```

OpenClaw 可以运行不同种类的调度工作。

两种重要模式：

| 工作类型 | 示例 | 含义 |
|---|---|---|
| System event | `--system-event "Reminder: check calendar"` | 向会话中入队一个事件 |
| Agent turn | `--message "Summarize overnight updates"` | 用 prompt 启动一次 agent run |

这一区分很重要。

主会话提醒更像是：

```text
put this reminder into the normal assistant flow
```

隔离的 cron 任务更像是：

```text
start a clean background agent task and send the result somewhere
```

这就是调度的 agent 工作需要 **会话设计** 的原因。

---

## 6. 会话模式

OpenClaw cron 支持多种会话目标。

| `--session` 取值 | 行为 | 适用场景 |
|---|---|---|
| `main` | 使用 agent 的主会话 | 提醒与常规唤醒 |
| `isolated` | 创建一个全新的 `cron:<jobId>` run 会话 | 报告、检查、后台杂务 |
| `current` | 在任务创建时绑定到当前活动会话 | 上下文感知的循环任务 |
| `session:<id>` | 使用持久化的命名会话 | 有意积累历史的工作流 |

其中最重要的是 `isolated`。

隔离的 cron run 每次运行都会获得 **全新的 transcript/session id**。它不会继承周遭的对话上下文，例如 channel routing、queue policy、elevation、origin 或过期的 runtime 绑定。

它仍可携带一些安全的偏好设置，例如：

- 选定的 model/auth 覆盖项
- thinking/fast/verbose 偏好
- labels

结论是：

> 当重复性自动化应当表现得像一个干净的任务、而非一段长对话时，使用隔离会话

合适的用途：

- 每日报告
- 监控检查
- 收件箱摘要
- 周期性项目巡检

不合适的用途：

- 刻意需要累积对话记忆的任务
- 昨天的结果应当影响今天运行的长周期工作流

这些场景请使用 `session:<id>`。

---

## 7. 投递是任务的一部分

在有人能看到结果、或系统有意抑制结果之前，一次调度的 agent run 都 **不算完成**。

OpenClaw 的投递模式有：

| 模式 | 含义 |
|---|---|
| `announce` | 若 agent 未直接发送，则将最终文本兜底投递到某个聊天目标 |
| `webhook` | 将完成的事件 payload POST 到某个 URL |
| `none` | 不做 runner 兜底投递 |

CLI 对应关系：

```bash
--announce     # enable announce fallback
--no-deliver   # delivery.mode = none
```

关键细节在于 **“fallback”**。

对于隔离任务，聊天投递由以下两者分担：

- agent 自身，当存在聊天路由时它可能使用 `message` 工具
- runner，若 agent 未发送，它可以播报最终回复

因此 `--announce` 并不意味着“总是重复发送消息”。它的含义是：

> 如果 agent 尚未把结果发送到目标，则投递最终回复

这可以避免常见的 **静默失败**。

示例：

```bash
openclaw cron add \
  --name "Morning brief" \
  --cron "0 7 * * *" \
  --session isolated \
  --message "Summarize overnight AI and hardware news." \
  --announce \
  --channel telegram \
  --to "-1001234567890"
```

---


<details>
<summary>English original</summary>

**4. Schedule types in OpenClaw**

OpenClaw has three schedule types.

| Kind | CLI flag | Best for |
|---|---|---|
| `at` | `--at` | one-shot reminder or one-time automation |
| `every` | `--every` | fixed interval checks |
| `cron` | `--cron` | calendar-style schedules |

Example one-shot:

```bash
openclaw cron add \
  --name "Calendar check" \
  --at "20m" \
  --session main \
  --system-event "Next heartbeat: check calendar." \
  --wake now
```

Example recurring cron:

```bash
openclaw cron add \
  --name "Morning brief" \
  --cron "0 7 * * *" \
  --tz "America/Los_Angeles" \
  --session isolated \
  --message "Summarize overnight updates." \
  --announce
```

One-shot jobs delete after success by default. Use `--keep-after-run` when you want to preserve the job after it completes.

Recurring top-of-hour schedules may be staggered to reduce load spikes. Use `--exact` for precise cron boundaries or `--stagger 30s` for an explicit spread window.

---

**5. What actually runs**

Traditional cron usually runs a command:

```text
run this shell script at 07:00
```

OpenClaw can run different kinds of scheduled work.

Two important modes:

| Work type | Example | Meaning |
|---|---|---|
| System event | `--system-event "Reminder: check calendar"` | enqueue an event into a session |
| Agent turn | `--message "Summarize overnight updates"` | start an agent run with a prompt |

This distinction matters.

A main-session reminder is more like:

```text
put this reminder into the normal assistant flow
```

An isolated cron job is more like:

```text
start a clean background agent task and send the result somewhere
```

That is why scheduled agent work needs **session design**.

---

**6. Session modes**

OpenClaw cron supports several session targets.

| `--session` value | Behavior | Use it for |
|---|---|---|
| `main` | use the agent's main session | reminders and ordinary wakeups |
| `isolated` | create a fresh `cron:<jobId>` run session | reports, checks, background chores |
| `current` | bind to the active session at job creation | context-aware recurring tasks |
| `session:<id>` | use a persistent named session | workflows that deliberately build history |

The most important one is `isolated`.

An isolated cron run gets a **fresh transcript/session id** for each run. It does not inherit ambient conversation context such as channel routing, queue policy, elevation, origin, or stale runtime bindings.

It may still carry safe preferences such as:

- selected model/auth overrides
- thinking/fast/verbose preferences
- labels

The lesson:

> use isolated sessions when repeated automation should behave like a clean task, not like a long conversation

Good uses:

- daily reports
- monitoring checks
- inbox summaries
- periodic project sweeps

Bad uses:

- jobs that intentionally need accumulated conversation memory
- long-running workflows where yesterday's result should shape today's run

For those, use `session:<id>`.

---

**7. Delivery is part of the job**

A scheduled agent run is **not complete** until someone can see the result or the system intentionally suppresses it.

OpenClaw's delivery modes are:

| Mode | Meaning |
|---|---|
| `announce` | fallback-deliver final text to a chat target if the agent did not send directly |
| `webhook` | POST the finished event payload to a URL |
| `none` | no runner fallback delivery |

CLI mapping:

```bash
--announce     # enable announce fallback
--no-deliver   # delivery.mode = none
```

The important detail is **"fallback."**

For isolated jobs, chat delivery is shared between:

- the agent itself, which may use the `message` tool when a chat route exists
- the runner, which can announce the final reply if the agent did not send it

So `--announce` does not mean "always duplicate the message." It means:

> if the agent did not already send the result to the target, deliver the final reply

This prevents common **silent failures**.

Example:

```bash
openclaw cron add \
  --name "Morning brief" \
  --cron "0 7 * * *" \
  --session isolated \
  --message "Summarize overnight AI and hardware news." \
  --announce \
  --channel telegram \
  --to "-1001234567890"
```

---

</details>

## 8. 失败投递

定时自动化必须**上报失败**。

否则 cron 就会变成：

> 系统什么都没做，也没人知道为什么

OpenClaw 按以下顺序解析失败通知：

1. job 专属的 `delivery.failureDestination`
2. 全局 `cron.failureDestination`
3. job 的主 announce 目标，前提是该 job 已使用 announce 投递

这给出一个安全的默认行为：

如果某个定时报告平时发到聊天里，失败可以回退到同一目标，除非配置了更具体的投递目标。

配置形态：

```json5
{
  cron: {
    failureDestination: {
      mode: "announce",
      channel: "last",
      to: "channel:C1234567890"
    }
  }
}
```

这是**运维设计**，不是 UI 打磨。

对于自主运行的定时 job，失败投递是一项**控制面需求**。

---

## 9. 重试行为

OpenClaw 有两套重试机制。

一次性 job 使用配置的瞬时错误重试：

```json5
{
  cron: {
    retry: {
      maxAttempts: 3,
      backoffMs: [30000, 60000, 300000],
      retryOn: ["rate_limit", "overloaded", "network", "timeout", "server_error"]
    }
  }
}
```

周期性 job 使用周期性失败退避模式：

```text
30s -> 1m -> 5m -> 15m -> 60m
```

退避会在下一次成功运行后**重置**。

这与**简单的 prompt 循环**有本质区别。

没有退避时：

- 已失效的本地模型 endpoint 会被反复猛打
- provider 故障会引发请求风暴
- 坏掉的 job 会刷爆用户或日志

对于目标为 Ollama 或 OpenAI 兼容本地 endpoint 这类本地 provider 的隔离 job，OpenClaw 还有 provider 预检行为。如果 endpoint 不可达，该次运行会被记为 `skipped`，而不是发起一次注定失败的模型调用。命中的失效 endpoint 会被短暂缓存，以免大量 job 反复打到同一个坏掉的本地服务。

---

## 10. 隔离 cron 的模型选择

定时任务需要**可预测的模型行为**。

OpenClaw 按以下顺序解析隔离 cron 的模型选择：

1. Gmail hook 的模型覆盖，前提是该次运行来自 Gmail 且允许覆盖
2. 每个 job 的 `--model`
3. 已存储的 cron 会话模型覆盖
4. agent/默认的模型选择

示例：

```bash
openclaw cron add \
  --name "Weekly deep analysis" \
  --cron "0 6 * * 1" \
  --session isolated \
  --message "Analyze project progress and risks." \
  --model "opus" \
  --thinking high \
  --announce
```

OpenClaw 把 `--model` 当作 job 主模型，而不是普通的聊天会话 `/model` 覆盖。

这意味着：

- 配置的 fallback 链仍然可以生效
- 每个 job 的 `fallbacks` 可以替换已配置的 fallback 列表
- `fallbacks: []` 会让该 job 变为严格模式
- 无效或不被允许的模型引用会明确失败，而不是静默换用另一个模型

教训是：

> 定时 job 应当明确表达模型意图，因为它们可能在人不在场时运行

---

## 11. 日志与保留

OpenClaw cron 保留运行历史。

常用命令：

```bash
openclaw cron list
openclaw cron show <job-id>
openclaw cron runs --id <job-id> --limit 50
```

运行历史包含投递诊断信息，例如：

- 预期目标
- 解析出的目标
- message-tool 发送
- fallback 使用情况
- 已投递/未投递状态

保留控制：

```json5
{
  cron: {
    sessionRetention: "24h",
    runLog: {
      maxBytes: "2mb",
      keepLines: 2000
    }
  }
}
```

这点很重要，因为隔离 cron job 会创建会话和 transcript。没有保留策略，自动化就会**永远堆积状态**。

生产环境的做法是：

> 保留足够历史以便调试，裁剪足够历史以避免状态膨胀

---

## 12. 手动执行

Cron job 应当无需等到下一个调度时间就能测试。

OpenClaw 支持：

```bash
openclaw cron run <job-id>
```

该操作默认强制运行，并在运行进入队列后返回。

成功的响应包含：

```json
{ "ok": true, "enqueued": true, "runId": "..." }
```

然后检查：

```bash
openclaw cron runs --id <job-id> --limit 50
```

使用：

```bash
openclaw cron run <job-id> --due
```

当你想要「仅在当前已到期时才运行」的行为时。

手动运行支持很重要，因为定时工作必须**可按需调试**。

---

## 13. Runtime 清理

Cron job 会触及工具和 runtime。

对于隔离 job，OpenClaw 包含如下清理行为：

- 对 cron 会话做尽力而为的浏览器清理
- 清理为该 job 创建的捆绑 MCP runtime 实例
- 抑制过期的仅确认回复
- 对执行拒绝元数据做结构化处理

这是一个重要的设计要点。

一次定时运行不应留下**隐藏的长期存活资源**。

如果一份日报打开了浏览器标签页或启动了 MCP 子进程，调度器就应当有清理路径。否则，定时自动化会慢慢变成**系统漂移**。

---


<details>
<summary>English original</summary>

**8. Failure delivery**

Scheduled automation must **report failures**.

Otherwise cron turns into:

> the system did nothing, and no one knows why

OpenClaw resolves failure notifications in this order:

1. job-specific `delivery.failureDestination`
2. global `cron.failureDestination`
3. the job's primary announce target, when the job already uses announce delivery

This gives you a safe default:

if a scheduled report normally posts to a chat, failures can fall back to the same target unless you configure a more specific destination.

Configuration shape:

```json5
{
  cron: {
    failureDestination: {
      mode: "announce",
      channel: "last",
      to: "channel:C1234567890"
    }
  }
}
```

This is **operational design**, not UI polish.

For autonomous scheduled jobs, failure delivery is a **control-plane requirement**.

---

**9. Retry behavior**

OpenClaw has two retry stories.

One-shot jobs use configured transient-error retry:

```json5
{
  cron: {
    retry: {
      maxAttempts: 3,
      backoffMs: [30000, 60000, 300000],
      retryOn: ["rate_limit", "overloaded", "network", "timeout", "server_error"]
    }
  }
}
```

Recurring jobs use a recurring failure backoff pattern:

```text
30s -> 1m -> 5m -> 15m -> 60m
```

The backoff **resets** after the next successful run.

This is a major difference from a **simple prompt loop**.

Without backoff:

- a dead local model endpoint can be hammered repeatedly
- provider outages can create request storms
- a broken job can spam users or logs

OpenClaw also has provider preflight behavior for isolated jobs that target local providers such as Ollama or OpenAI-compatible local endpoints. If the endpoint is unreachable, the run can be recorded as `skipped` rather than beginning a doomed model call. Matching dead endpoints are cached briefly to avoid many jobs hitting the same broken local service.

---

**10. Model selection for isolated cron**

Scheduled tasks need **predictable model behavior**.

OpenClaw resolves isolated cron model selection in this order:

1. Gmail-hook model override, when the run came from Gmail and the override is allowed
2. per-job `--model`
3. stored cron-session model override
4. agent/default model selection

Example:

```bash
openclaw cron add \
  --name "Weekly deep analysis" \
  --cron "0 6 * * 1" \
  --session isolated \
  --message "Analyze project progress and risks." \
  --model "opus" \
  --thinking high \
  --announce
```

OpenClaw treats `--model` as a job primary, not as a normal chat-session `/model` override.

That means:

- configured fallback chains can still apply
- per-job `fallbacks` can replace the configured fallback list
- `fallbacks: []` makes the job strict
- invalid or disallowed model refs fail clearly instead of silently using another model

The lesson:

> scheduled jobs should be explicit about model intent, because they may run when no human is watching

---

**11. Logging and retention**

OpenClaw cron keeps run history.

Useful commands:

```bash
openclaw cron list
openclaw cron show <job-id>
openclaw cron runs --id <job-id> --limit 50
```

Run history includes delivery diagnostics such as:

- intended target
- resolved target
- message-tool sends
- fallback use
- delivered/not delivered status

Retention controls:

```json5
{
  cron: {
    sessionRetention: "24h",
    runLog: {
      maxBytes: "2mb",
      keepLines: 2000
    }
  }
}
```

This matters because isolated cron jobs create sessions and transcripts. Without retention, automation creates **state forever**.

The production pattern is:

> keep enough history to debug, prune enough history to avoid state growth

---

**12. Manual execution**

Cron jobs should be testable without waiting for the next scheduled time.

OpenClaw supports:

```bash
openclaw cron run <job-id>
```

This force-runs by default and returns once the run is queued.

Successful responses include:

```json
{ "ok": true, "enqueued": true, "runId": "..." }
```

Then inspect:

```bash
openclaw cron runs --id <job-id> --limit 50
```

Use:

```bash
openclaw cron run <job-id> --due
```

when you want "run only if currently due" behavior.

Manual run support is important because scheduled work must be **debuggable on demand**.

---

**13. Runtime cleanup**

Cron jobs can touch tools and runtimes.

For isolated jobs, OpenClaw includes cleanup behavior such as:

- best-effort browser cleanup for the cron session
- cleanup of bundled MCP runtime instances created for the job
- suppression of stale acknowledgement-only replies
- structured handling of execution denial metadata

This is an important design point.

A scheduled run should not leave **hidden long-lived resources** behind.

If a daily report opens browser tabs or starts MCP child processes, the scheduler should have a cleanup path. Otherwise, scheduled automation slowly becomes **system drift**.

---

</details>

## 14. Debugging ladder

在猜测之前，先用一套**无聊的命令阶梯**。

```bash
openclaw status
openclaw gateway status
openclaw cron status
openclaw cron list
openclaw cron show <job-id>
openclaw cron runs --id <job-id> --limit 20
openclaw system heartbeat last
openclaw logs --follow
openclaw doctor
```

常见情形：

| 症状 | 可能区域 |
|---|---|
| job 从不触发 | `cron.enabled`、网关未运行、时区、错误的 schedule |
| 手动 `--due` 显示未到期 | schedule 有效但当前未到期 |
| job 运行了但没有聊天输出 | 投递模式、路由解析、静默 token、通道鉴权 |
| 本地模型 job 被跳过 | provider 预检失败 |
| 被阻止的命令报告失败 | 工具策略或执行拒绝 |
| 反复失败后变慢 | 周期性重试退避在正常工作 |

关键在于区分：

- schedule 问题
- 执行问题
- 模型问题
- 投递问题
- 留存/日志问题

---

## 15. 校验应在 job 创建之前

这是干净的分层规则：

> 无效的 schedule 应在持久化创建 job 之前就失败

为什么？

因为 job 一旦被持久化，系统就得回答更难的问题：

- 它应不应该出现在 `cron list` 中？
- 它应不应该可编辑？
- 它应不应该在每次网关启动时都失败？
- runtime 解析器应不应该稍后才抛错？
- `jobs-state.json` 应不应该跟踪它？

对 `--cron` 来说，校验应发生在 CLI/API 输入变成 job 定义的那个边界上。

同样的原则适用于：

- 错误的时区
- 无效的投递目标
- 不支持的会话目标
- 不被允许的模型
- 无效的工具限制

这是一条通用的生产经验：

> 配置校验应尽量贴近写入边界，而不是推迟到 runtime 循环里

runtime 代码仍可自我保护，但它不应是用户得知自己的 schedule 字符串无效的**第一处**。

---

## 16. 示例：晨间运维简报

命令：

```bash
openclaw cron add \
  --name "Morning Ops Brief" \
  --cron "0 7 * * 1-5" \
  --tz "America/Los_Angeles" \
  --session isolated \
  --message "Summarize overnight incidents, open deployment risks, and unresolved alerts. Keep it concise and include next actions." \
  --agent ops \
  --model "opus" \
  --thinking high \
  --announce \
  --channel slack \
  --to "channel:C1234567890"
```

发生的过程：

1. 网关调度器计算下一个到期时间。
2. 在工作日的 07:00，它创建一个 cron run。
3. 该 run 使用 `ops` agent。
4. 该 run 在隔离会话中启动。
5. 模型由 job 的模型选择解析得到。
6. agent 收到消息。
7. 如果 agent 直接发送到 Slack 目标，则跳过兜底 announce。
8. 如果 agent 未发送，则由 runner 播报最终回复。
9. 该 run 被记录日志。
10. 失败遵循 job/全局/主失败投递规则。

这并不只是**“定时 prompt”**。

这是带**路由、隔离、投递、重试与可审计性**的定时 agent 工作。

---

## 17. 设计练习

为一个常驻工程助手设计三个 job。

| Job | Schedule | 会话 | 投递 | 失败策略 |
|---|---|---|---|---|
| 晨间简报 | `0 7 * * 1-5` | `isolated` | Slack announce | 失败时发往 ops-alerts |
| 每周规划摘要 | `0 16 * * 5` | `session:weekly-planning` | webhook 到 dashboard | 失败时发往 Slack |
| 本地模型健康探测 | `*/30 * * * *` | `isolated` | `none` | 3 次失败后告警 |

对每个 job 回答：

- 这个 job 应不应该记住之前的运行？
- 谁应该看到输出？
- 它应被允许使用哪些工具？
- 如果模型 provider 宕机，会发生什么？
- run 日志应保留多久？
- 被跳过的 run 应不应该告警给谁？

这才是“加个 cron job”的**专业版**。

---

## 关键要点

- Cron 是一种调度语言，而不只是一个文本框。
- OpenClaw cron 在网关内运行，并持久化 job 定义、runtime 状态与运行历史。
- `--at`、`--every` 与 `--cron` 解决不同的调度问题。
- 隔离会话是干净的周期性 agent 工作的默认形态。
- 投递是 cron 的一等组成部分，因为定时产生的结果不应消失。
- 失败目的地、重试、日志与留存是可靠性特性，而非附加项。
- cron 的模型选择必须显式且可检查。
- Schedule 校验应在 job 创建之前完成。

---

## 参考文献

- 案例研究源仓库：[OpenClaw](https://github.com/openclaw/openclaw)
- OpenClaw 文档：
  - `docs/automation/cron-jobs.md`
  - `docs/cli/cron.md`
  - `docs/gateway/configuration-reference.md`
  - `docs/reference/session-management-compaction.md`

---

*下一篇：[Lecture 37](Lecture-37.md)*


<details>
<summary>English original</summary>

**14. Debugging ladder**

Use a **boring command ladder** before guessing.

```bash
openclaw status
openclaw gateway status
openclaw cron status
openclaw cron list
openclaw cron show <job-id>
openclaw cron runs --id <job-id> --limit 20
openclaw system heartbeat last
openclaw logs --follow
openclaw doctor
```

Common cases:

| Symptom | Likely area |
|---|---|
| job never fires | `cron.enabled`, Gateway not running, timezone, bad schedule |
| manual `--due` says not due | schedule is valid but not currently due |
| job runs but no chat output | delivery mode, route resolution, silent token, channel auth |
| local model job is skipped | provider preflight failed |
| blocked command reports failure | tool policy or execution denial |
| repeated failures slow down | recurring retry backoff is working |

The key is to separate:

- schedule problem
- execution problem
- model problem
- delivery problem
- retention/logging problem

---

**15. Validation belongs before job creation**

This is the clean layering rule:

> invalid schedules should fail before durable job creation

Why?

Because once a job is persisted, the system has to answer harder questions:

- should it appear in `cron list`?
- should it be editable?
- should it fail every Gateway startup?
- should the runtime parser throw later?
- should `jobs-state.json` track it?

For `--cron`, validation should happen at the boundary where CLI/API input becomes a job definition.

The same principle applies to:

- bad timezones
- invalid delivery targets
- unsupported session targets
- disallowed models
- invalid tool restrictions

This is a general production lesson:

> configuration validation should be closest to the write boundary, not deferred to the runtime loop

Runtime code can still defend itself, but it should not be the **first place** a user learns that their schedule string was invalid.

---

**16. Example: morning operations brief**

Command:

```bash
openclaw cron add \
  --name "Morning Ops Brief" \
  --cron "0 7 * * 1-5" \
  --tz "America/Los_Angeles" \
  --session isolated \
  --message "Summarize overnight incidents, open deployment risks, and unresolved alerts. Keep it concise and include next actions." \
  --agent ops \
  --model "opus" \
  --thinking high \
  --announce \
  --channel slack \
  --to "channel:C1234567890"
```

What happens:

1. Gateway scheduler computes the next due time.
2. At 07:00 on weekdays, it creates a cron run.
3. The run uses the `ops` agent.
4. The run starts in an isolated session.
5. The model is resolved from the job's model selection.
6. The agent receives the message.
7. If the agent sends directly to the Slack target, fallback announce is skipped.
8. If the agent does not send, the runner announces the final reply.
9. The run is logged.
10. Failures follow job/global/primary failure delivery rules.

This is not just **"scheduled prompting."**

It is scheduled agent work with **routing, isolation, delivery, retry, and auditability**.

---

**17. Design exercise**

Design three jobs for a persistent engineering assistant.

| Job | Schedule | Session | Delivery | Failure policy |
|---|---|---|---|---|
| Morning brief | `0 7 * * 1-5` | `isolated` | Slack announce | failure to ops-alerts |
| Weekly planning summary | `0 16 * * 5` | `session:weekly-planning` | webhook to dashboard | failure to Slack |
| Local model health probe | `*/30 * * * *` | `isolated` | `none` | alert after 3 failures |

For each job, answer:

- Should this job remember previous runs?
- Who should see the output?
- What tools should it be allowed to use?
- What happens if the model provider is down?
- How long should run logs be retained?
- Should a skipped run alert anyone?

That is the **professional version** of "add a cron job."

---

**Key takeaways**

- Cron is a scheduling language, not just a text field.
- OpenClaw cron runs inside the Gateway and persists job definitions, runtime state, and run history.
- `--at`, `--every`, and `--cron` solve different scheduling problems.
- Isolated sessions are the default shape for clean recurring agent work.
- Delivery is a first-class part of cron because scheduled results should not disappear.
- Failure destinations, retries, logs, and retention are reliability features, not extras.
- Model selection for cron must be explicit and inspectable.
- Schedule validation should happen before job creation.

---

**References**

- Case-study source repo: [OpenClaw](https://github.com/openclaw/openclaw)
- OpenClaw docs:
  - `docs/automation/cron-jobs.md`
  - `docs/cli/cron.md`
  - `docs/gateway/configuration-reference.md`
  - `docs/reference/session-management-compaction.md`

---

*Next: [Lecture 37](Lecture-37.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-36.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-36.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
