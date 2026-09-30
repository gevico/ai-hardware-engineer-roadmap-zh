---
title: Lecture 26 - 会话作为唯一事实来源：事件溯源的 agent 状态
description: Lecture 26 - 会话作为唯一事实来源：事件溯源的 agent 状态
---

# Lecture 26 - 会话作为唯一事实来源：事件溯源的 agent 状态

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [Lecture 25](Lecture-25.md) | **下一讲：** [Lecture 27](Lecture-27.md)

---

[Lecture 02](Lecture-02.md) 把「状态与记忆」列为 harness（agent 运行时框架）所拥有的六件事之一。那只是个占位符。本讲把这一关注点单独拎出来，让它成为本讲的主题，因为 agent 系统里几乎每一个可靠性 bug 都可追溯到同一个错误：

> 把 **上下文窗口** 当成 **会话**

模型的上下文是覆盖一段对话的 200K token 滑动视图。会话是对实际发生之事的持久记录。二者并非同一回事，把它们混为一谈正是以下问题的根源：

- 崩溃时丢失的生成结果
- 「agent 忘了自己正在做什么」
- 无法重放的事故
- 悄悄丢状态的分支
- 重启后的恢复其实并没有恢复

本讲界定这一区分，给出一份事件 schema，并完整走查一套 `wake(sessionId)` 协议，它让无状态的 harness 能精确地从上一个实例终止之处接着跑。

---

## 学习目标

学完本讲，你应当能够：

1. 用一句话说清会话与上下文窗口的区别。
2. 解释为什么事件溯源是 agent 状态的正确架构。
3. 为 agent runtime 设计一套仅追加的事件 schema。
4. 实现一个 `buildContext(session, strategy)` 函数，从会话日志推导出上下文窗口。
5. 勾勒一套 `wake(sessionId)` 恢复协议。
6. 识别会破坏崩溃恢复的 runtime 内部隐藏状态。
7. 把流式生成持久化为事件流，使部分输出能在网关崩溃后存活。
8. 设计会话存储时，能就重放、分支与审计进行推演。

---

## 1. 心智模型：数据库 vs 查询结果

两层，生命周期差异极大。

```text
+----------------------------------------------------+
|     SESSION   (ground truth, durable, append-only) |
|     -> message received                            |
|     -> tool invoked                                |
|     -> stream chunk                                |
|     -> generation complete                         |
+----------------------------------------------------+
                       |
                       v  buildContext(session, strategy)
+----------------------------------------------------+
|     CONTEXT   (ephemeral view, fits in 200K)       |
|     -> system prompt                               |
|     -> tool catalog                                |
|     -> compacted transcript                        |
|     -> retrieved memory snippets                   |
+----------------------------------------------------+
```

| 层 | 类比 | 生命周期 | 是否事实来源？ |
|-------|---------|----------|------------------|
| 会话 | 数据库表 | 无限期 | 是 |
| 上下文 | `SELECT` 结果 | 一次模型调用 | 否 |

模型永远只看到上下文。harness 永远只写入会话。

一旦内化这一点，本讲其余部分基本就是机械操作。

---

## 2. 为什么这是事件溯源

状态不是被存储的。状态是从事件日志中*推导*出来的。

这正是数据库社区称为 **事件溯源** 的模式：权威记录是一条仅追加的事实流，任何视图（当前余额、当前购物车、当前 agent 记忆）都是对这条流的一次折叠。

```text
session_log = [event_0, event_1, event_2, ..., event_n]

current_state(t) = fold(session_log[0..t], reducer)
```

把它套用到 agent 上，就得到：

- 会话日志是权威记录。
- 上下文窗口是该日志的一种可能投影。
- 摘要记忆是另一种投影。
- 面向用户的 transcript 是第三种投影。
- 三种不同的投影，一个事实来源。

这就解锁了四项用其他方式无法合理获得的能力：

| 能力 | 事件溯源为何能带来它 |
|------------|------------------------------------|
| 重放 | 在日志上离线重跑 reducer |
| 分支 | 在事件 N 处 fork 日志，跑两条时间线 |
| 可恢复性 | 崩溃后折叠日志 → 推导状态 → 继续 |
| 审计 | 日志本身就是审计轨迹，无需额外附加 |

如果你的 agent 现在一项都给不了，那你就存在「把上下文当记忆」的问题。

---


<details>
<summary>English original</summary>

**Lecture 26 - Session as Source of Truth: Event-Sourced Agent State**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 25](Lecture-25.md) | **Next:** [Lecture 27](Lecture-27.md)

---

[Lecture 02](Lecture-02.md) listed "state and memory" as one of the six things a harness owns. That was a placeholder. This lecture takes that single concern and makes it the lecture, because almost every reliability bug in agent systems traces back to one mistake:

> treating the **context window** as if it were the **session**

A model context is a 200K-token sliding view over a conversation. A session is the durable record of what actually happened. They are not the same thing, and confusing them is the root of:

- generation lost on crash
- "the agent forgot what it was doing"
- impossible-to-replay incidents
- branching that silently loses state
- resume-after-restart that doesn't actually resume

This lecture defines the split, gives you an event schema, and walks through a `wake(sessionId)` protocol that lets a stateless harness pick up exactly where the previous instance died.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. State the difference between a session and a context window in one sentence.
2. Explain why event sourcing is the correct architecture for agent state.
3. Design an append-only event schema for an agent runtime.
4. Implement a `buildContext(session, strategy)` function that derives the context window from the session log.
5. Sketch a `wake(sessionId)` recovery protocol.
6. Identify hidden in-runtime state that breaks crash recovery.
7. Persist a streaming generation as an event stream so partial output survives a gateway crash.
8. Reason about replay, branching, and audit when designing a session store.

---

**1. The mental model: database vs query result**

Two layers, very different lifetimes.

```text
+----------------------------------------------------+
|     SESSION   (ground truth, durable, append-only) |
|     -> message received                            |
|     -> tool invoked                                |
|     -> stream chunk                                |
|     -> generation complete                         |
+----------------------------------------------------+
                       |
                       v  buildContext(session, strategy)
+----------------------------------------------------+
|     CONTEXT   (ephemeral view, fits in 200K)       |
|     -> system prompt                               |
|     -> tool catalog                                |
|     -> compacted transcript                        |
|     -> retrieved memory snippets                   |
+----------------------------------------------------+
```

| Layer | Analogy | Lifetime | Source of truth? |
|-------|---------|----------|------------------|
| Session | Database table | Indefinite | Yes |
| Context | `SELECT` result | One model call | No |

The model only ever sees the context. The harness only ever writes to the session.

Once you internalize this, the rest of the lecture is mostly mechanical.

---

**2. Why this is event sourcing**

State is not stored. State is *derived* from a log of events.

This is exactly the pattern called **event sourcing** in the database community: the canonical record is an append-only stream of facts, and any view (current balance, current cart, current agent memory) is a fold over that stream.

```text
session_log = [event_0, event_1, event_2, ..., event_n]

current_state(t) = fold(session_log[0..t], reducer)
```

Apply that to an agent and you get:

- The session log is the canonical record.
- The context window is one possible projection of that log.
- A summary memory is another projection.
- A user-facing transcript is a third projection.
- Three different projections, one source of truth.

This unlocks four capabilities you cannot reasonably get any other way:

| Capability | Why event sourcing gives it to you |
|------------|------------------------------------|
| Replay | Re-run reducer on the log offline |
| Branching | Fork the log at event N, run two timelines |
| Resumability | After a crash, fold the log → derive state → continue |
| Auditing | The log is already the audit trail; nothing to bolt on |

If your agent gives you none of those today, you have a context-as-memory problem.

---

</details>

## 3. 事件 schema

一个可行的起步 schema 是六种事件类型。保持它们小而稳定；之后无法在不破坏 replay 的前提下重写事件。

```jsonl
{"ts":"2026-05-05T01:00:00Z","type":"MESSAGE_RECEIVED","id":"evt_001","payload":{"role":"user","text":"summarize this PR"}}
{"ts":"2026-05-05T01:00:01Z","type":"LLM_CALLED","id":"evt_002","payload":{"model":"your-agent-model-id","prompt_tokens":12450,"context_strategy":"sliding_window_50"}}
{"ts":"2026-05-05T01:00:02Z","type":"GEN_START","id":"evt_003","payload":{"msg_id":"msg_42","stream":true}}
{"ts":"2026-05-05T01:00:02Z","type":"GEN_CHUNK","id":"evt_004","payload":{"msg_id":"msg_42","seq":0,"delta":"Looking at"}}
{"ts":"2026-05-05T01:00:02Z","type":"GEN_CHUNK","id":"evt_005","payload":{"msg_id":"msg_42","seq":1,"delta":" the diff"}}
{"ts":"2026-05-05T01:00:03Z","type":"TOOL_INVOKED","id":"evt_006","payload":{"call_id":"tc_7","name":"git_diff","args":{"ref":"HEAD~1"}}}
{"ts":"2026-05-05T01:00:04Z","type":"TOOL_RESULT","id":"evt_007","payload":{"call_id":"tc_7","ok":true,"data_ref":"blob://abc123","bytes":4200}}
{"ts":"2026-05-05T01:00:05Z","type":"GEN_COMPLETE","id":"evt_008","payload":{"msg_id":"msg_42","reason":"stop"}}
{"ts":"2026-05-05T01:00:05Z","type":"GEN_SENT","id":"evt_009","payload":{"msg_id":"msg_42","channel":"chat-ui"}}
```

关于这个 schema 的几点说明，每一条都重要：

- **仅追加。** 事件一经写入，永不修改。
- **单调 id。** 用 ULID 或每会话计数器。崩溃后排序所必需。
- **`ts` 是 wall-clock；排序按 id。** 高负载下 wall-clock 会说谎。
- **大 payload 放进 blob store，而不是 log。** 工具结果、截图、音频：把字节存到别处，在事件里放一个 `data_ref`。log 保持足够小，便于扫描和 replay。
- 输出采用**两阶段发送**：`GEN_COMPLETE`（模型完成）与 `GEN_SENT`（用户／渠道实际收到）分开。没有这一拆分，崩溃之后你就无法判断：究竟是欠用户一次重复发送，还是什么都不欠。

起步阶段磁盘上的 JSONL 就够了。当需要跨会话查询，或规模提出要求时，再迁移到数据库。schema 不变；变的只是存储。

---

## 4. harness（agent 运行时框架）变成无状态解释器

事件一旦持久化，harness 就坍缩成一个小循环：

```python
def step(session_id):
    log = session_store.load(session_id)
    if is_terminated(log):
        return

    context = build_context(log, strategy="sliding_window_50")
    response = model.call(context, tools=visible_tools(log))

    session_store.append(session_id, {"type": "LLM_CALLED", ...})

    if response.tool_calls:
        for call in response.tool_calls:
            session_store.append(session_id, {"type": "TOOL_INVOKED", ...})
            result = dispatch(call)
            session_store.append(session_id, {"type": "TOOL_RESULT", ...})
        return  # caller decides whether to step again

    # streaming text generation
    session_store.append(session_id, {"type": "GEN_START", ...})
    for delta in response.stream:
        session_store.append(session_id, {"type": "GEN_CHUNK", ...})
        channel.send(delta)
    session_store.append(session_id, {"type": "GEN_COMPLETE", ...})
    channel.flush()
    session_store.append(session_id, {"type": "GEN_SENT", ...})
```

值得注意的点：

- 该函数接收 session id，而不是 state 对象。**轮次之间，process 内存中不保留任何东西。**
- 每一个可观测的副作用，之前或之后都有一次事件写入。
- 崩溃之后，这段完全相同的代码可以用同一个 session id 重新进入并恢复。

这就是人们所说的「基于事件日志的无状态 harness」。

---

## 5. `wake(sessionId)` 协议

`wake` 是恢复流程，由每个 harness 实例在启动时运行，也由任何想要恢复会话的 cron / scheduler / 外部触发器运行。

```text
wake(sessionId):
  1. log    = session_store.load(sessionId)
  2. last   = last_event(log)
  3. switch on last.type:
       MESSAGE_RECEIVED        -> step(sessionId)               # never got to model
       LLM_CALLED              -> step(sessionId)               # crashed before generation
       TOOL_INVOKED            -> reissue_tool_or_fail(last)    # tool may have run
       TOOL_RESULT             -> step(sessionId)               # safe to continue
       GEN_START / GEN_CHUNK   -> resume_or_replace(last)       # see below
       GEN_COMPLETE            -> redeliver_if_unsent(last)
       GEN_SENT                -> done; idle
       SESSION_TERMINATED      -> noop
```

两个有意思的分支是 `TOOL_INVOKED` 和 `GEN_*`。


<details>
<summary>English original</summary>

**3. The event schema**

A workable starting schema is six event types. Keep them small and stable; you cannot rewrite events later without breaking replay.

```jsonl
{"ts":"2026-05-05T01:00:00Z","type":"MESSAGE_RECEIVED","id":"evt_001","payload":{"role":"user","text":"summarize this PR"}}
{"ts":"2026-05-05T01:00:01Z","type":"LLM_CALLED","id":"evt_002","payload":{"model":"your-agent-model-id","prompt_tokens":12450,"context_strategy":"sliding_window_50"}}
{"ts":"2026-05-05T01:00:02Z","type":"GEN_START","id":"evt_003","payload":{"msg_id":"msg_42","stream":true}}
{"ts":"2026-05-05T01:00:02Z","type":"GEN_CHUNK","id":"evt_004","payload":{"msg_id":"msg_42","seq":0,"delta":"Looking at"}}
{"ts":"2026-05-05T01:00:02Z","type":"GEN_CHUNK","id":"evt_005","payload":{"msg_id":"msg_42","seq":1,"delta":" the diff"}}
{"ts":"2026-05-05T01:00:03Z","type":"TOOL_INVOKED","id":"evt_006","payload":{"call_id":"tc_7","name":"git_diff","args":{"ref":"HEAD~1"}}}
{"ts":"2026-05-05T01:00:04Z","type":"TOOL_RESULT","id":"evt_007","payload":{"call_id":"tc_7","ok":true,"data_ref":"blob://abc123","bytes":4200}}
{"ts":"2026-05-05T01:00:05Z","type":"GEN_COMPLETE","id":"evt_008","payload":{"msg_id":"msg_42","reason":"stop"}}
{"ts":"2026-05-05T01:00:05Z","type":"GEN_SENT","id":"evt_009","payload":{"msg_id":"msg_42","channel":"chat-ui"}}
```

Notes on this schema, all of which matter:

- **Append-only.** Once written, an event is never modified.
- **Monotonic ids.** Either ULIDs or a per-session counter. Required for ordering after a crash.
- **`ts` is wall-clock; ordering is by id.** Wall-clocks lie under load.
- **Large payloads go to a blob store, not the log.** Tool results, screenshots, audio: store the bytes elsewhere and put a `data_ref` in the event. The log stays small enough to scan and replay.
- **Two-phase send** for outputs: `GEN_COMPLETE` (model finished) is separate from `GEN_SENT` (user / channel actually received). Without this split you cannot tell, after a crash, whether you owe the user a duplicate or nothing.

JSONL on disk is fine to start. Move to a database when you need queries across sessions or when scale demands it. The schema does not change; only the storage does.

---

**4. The harness becomes a stateless interpreter**

Once events are durable, the harness collapses to a small loop:

```python
def step(session_id):
    log = session_store.load(session_id)
    if is_terminated(log):
        return

    context = build_context(log, strategy="sliding_window_50")
    response = model.call(context, tools=visible_tools(log))

    session_store.append(session_id, {"type": "LLM_CALLED", ...})

    if response.tool_calls:
        for call in response.tool_calls:
            session_store.append(session_id, {"type": "TOOL_INVOKED", ...})
            result = dispatch(call)
            session_store.append(session_id, {"type": "TOOL_RESULT", ...})
        return  # caller decides whether to step again

    # streaming text generation
    session_store.append(session_id, {"type": "GEN_START", ...})
    for delta in response.stream:
        session_store.append(session_id, {"type": "GEN_CHUNK", ...})
        channel.send(delta)
    session_store.append(session_id, {"type": "GEN_COMPLETE", ...})
    channel.flush()
    session_store.append(session_id, {"type": "GEN_SENT", ...})
```

Things to notice:

- The function takes a session id, not a state object. **Nothing is held in process memory between turns.**
- Every observable side effect is preceded or followed by an event write.
- After a crash, this exact code can be re-entered with the same session id and recover.

This is what people mean by "stateless harness over an event log."

---

**5. The `wake(sessionId)` protocol**

`wake` is the recovery procedure run by every harness instance on startup, and by any cron / scheduler / external trigger that wants to resume a session.

```text
wake(sessionId):
  1. log    = session_store.load(sessionId)
  2. last   = last_event(log)
  3. switch on last.type:
       MESSAGE_RECEIVED        -> step(sessionId)               # never got to model
       LLM_CALLED              -> step(sessionId)               # crashed before generation
       TOOL_INVOKED            -> reissue_tool_or_fail(last)    # tool may have run
       TOOL_RESULT             -> step(sessionId)               # safe to continue
       GEN_START / GEN_CHUNK   -> resume_or_replace(last)       # see below
       GEN_COMPLETE            -> redeliver_if_unsent(last)
       GEN_SENT                -> done; idle
       SESSION_TERMINATED      -> noop
```

The two interesting branches are `TOOL_INVOKED` and `GEN_*`.

</details>

### 5.1 工具调用恢复

如果 harness（agent 运行时框架）在写入 `TOOL_INVOKED` 之后、`TOOL_RESULT` 之前挂掉，你无法知道工具是否运行过。三种正确做法，按优先级排列：

1. **幂等工具。** 每次工具调用都携带一个 `call_id`。工具实现要么能安全重跑，要么能识别出重复并返回先前的结果。这是唯一可扩展的做法。
2. **补偿动作。** 如果工具非幂等（发了消息、扣了卡），记录 `TOOL_FAILED_UNCERTAIN` 并把它呈现给用户。
3. **拒绝自动恢复。** 把会话标记为 needs-human-attention。

绝不要静默重试非幂等工具。一次好过两次。

### 5.2 流式生成恢复（OpenClaw #40712 案例）

用户向模型提问。模型开始流式输出。三个 chunk 落进了日志。然后网关崩溃了。接下来怎么办？

两种策略：

- **续写（Resume）。** 把到目前为止的 chunk 作为已经 prefill（首字前的整段计算）过的 assistant 轮次重新调用模型，并要求它继续。适用于服务方支持 prefill / 续写的情况。更便宜、更快，但续写内容在风格上可能偏离用户已经看到的部分。
- **替换（Replace）。** 丢弃这些不完整的 chunk，标记为已失效，从头重新生成。总是可行。消耗更多 token。用户会看到回答从头开始。

两种都对。无论选哪个，选择都会体现在日志里：

```jsonl
{"type":"GEN_RESUMED","payload":{"msg_id":"msg_42","strategy":"resume","prior_seq":2}}
```

错误的策略是什么都不做——把一条只发了一半的消息留在 channel 里，再也不去完成它。这正是「把上下文当记忆」产生的后果。

---

## 6. 这套架构消除的两种反模式

### 6.1 把上下文当记忆

```python
class BadHarness:
    def __init__(self):
        self.history = []   # this is the bug
    def step(self, msg):
        self.history.append({"role":"user","content":msg})
        r = model.call(self.history)
        self.history.append({"role":"assistant","content":r})
        return r
```

`self.history` 存放在进程内存里。重启进程，它就没了。在负载均衡器后面跑两个副本，它们会各说各话。热重载代码，对话就丢了。每个这样起步的长生命周期 agent 系统，最终都会因此被 pager 呼叫。

修复办法不是「把 `self.history` 持久化到磁盘」。修复办法是**根本不要有 `self.history`**。让 harness 每一步都从会话日志加载。

### 6.2 runtime 内部的隐藏状态

如果 harness 改动了任何不在会话日志里的东西——计数器、功能开关、工具有状态的 client——下一个实例就会基于过期的世界视图来做推理。

规则：模型在*下一*轮要用来推理的每一个事实，都必须要么在会话日志里，要么能从模型可调用的工具推导出来。其他任何东西都是隐藏状态，最终会给你惊喜。

---

## 7. 动手构建

挑一个做完。重点在于感受这套架构，而不是做出一个产品。

**入门。** 改造一个最小 harness（[Lecture 02, §5](Lecture-02.md)），把六种事件类型写成 JSONL 日志落盘。在对话中途杀掉进程。重启并指向同一个日志文件。确认对话像什么都没发生一样继续下去。

**进阶。** 加上流式。在 `GEN_START` 和 `GEN_COMPLETE` 之间杀掉进程。用**替换**策略实现 `wake(sessionId)`。验证用户看到的是回答干净地重新开始，而不是一条半截消息。

**高级。** 用幂等工具实现工具调用恢复。工具的第一个动作是在旁路日志里查自己的 `call_id`；如果查到，就直接返回先前的结果而不重跑。在 `TOOL_INVOKED` 和 `TOOL_RESULT` 之间杀掉进程，确认工具恰好只运行一次。

---

## 8. 交付

产物：一次真实对话的会话日志，外加一页纸的说明，回答：

1. 你的 harness 发出了哪些事件类型？
2. 哪些 payload 内联存放，哪些放到了 blob store？
3. 你用了什么 `buildContext` 策略？把函数贴出来。
4. 用户开一个「new chat」时日志会怎样？（提示：不应该删除任何东西。）
5. 你处理了工具幂等性吗？怎么处理的？
6. 给出一个带时间戳的序列，展示 harness 崩溃后 `wake(sessionId)` 如何恢复。

评审者应该能拿你的日志，通过你的 `step` 函数重放，逐字节复现用户的体验。如果做不到，说明 harness 还在某处藏着状态。

---


<details>
<summary>English original</summary>

**5.1 Tool-call recovery**

If the harness died after writing `TOOL_INVOKED` but before `TOOL_RESULT`, you do not know whether the tool ran. Three correct options, in order of preference:

1. **Idempotent tools.** Every tool call carries a `call_id`. Tool implementation either re-runs safely or detects the duplicate and returns the prior result. This is the only option that scales.
2. **Compensating action.** If the tool was non-idempotent (sent a message, charged a card), record `TOOL_FAILED_UNCERTAIN` and surface it to the user.
3. **Refuse to recover automatically.** Mark the session needs-human-attention.

Never silently retry a non-idempotent tool. Once is better than twice.

**5.2 Streaming-generation recovery (the OpenClaw #40712 case)**

The user asked the model. The model started streaming. Three chunks landed in the log. Then the gateway crashed. What now?

Two strategies:

- **Resume.** Re-call the model with the chunks-so-far as a prefilled assistant turn and ask it to continue. Works when the provider supports prefill / continuation. Cheaper, faster, but the continuation may diverge stylistically from what the user already saw.
- **Replace.** Discard the partial chunks, mark them invalidated, regenerate from scratch. Always works. Costs more tokens. The user sees the response start over.

Either is correct. Whichever you pick, the choice is visible in the log:

```jsonl
{"type":"GEN_RESUMED","payload":{"msg_id":"msg_42","strategy":"resume","prior_seq":2}}
```

The wrong strategy is to do nothing — leave a half-emitted message in the channel and never finish it. That is what "context as memory" produces.

---

**6. Two anti-patterns that this architecture eliminates**

**6.1 Context as memory**

```python
class BadHarness:
    def __init__(self):
        self.history = []   # this is the bug
    def step(self, msg):
        self.history.append({"role":"user","content":msg})
        r = model.call(self.history)
        self.history.append({"role":"assistant","content":r})
        return r
```

`self.history` lives in process memory. Restart the process and it is gone. Run two replicas behind a load balancer and they disagree. Hot-reload code and you lose the conversation. Every long-lived agent system that starts this way eventually gets paged for it.

The fix is not "persist `self.history` to disk." The fix is to **not have `self.history`**. Make the harness load from the session log on every step.

**6.2 Hidden in-runtime state**

If the harness mutates anything that is not in the session log — a counter, a feature flag, a tool's stateful client — the next instance reasons from a stale view of the world.

The rule: every fact the model will reason about on the *next* turn must be either in the session log or derivable from a tool the model can call. Anything else is hidden state and will eventually surprise you.

---

**7. Build it**

Pick one of these and finish it. The point is to feel the architecture, not to build a product.

**Beginner.** Modify a minimal harness (the one in [Lecture 02, §5](Lecture-02.md)) to write a JSONL log of the six event types to disk. Kill the process mid-conversation. Restart it pointed at the same log file. Confirm the conversation continues as if nothing happened.

**Intermediate.** Add streaming. Kill the process between `GEN_START` and `GEN_COMPLETE`. Implement `wake(sessionId)` with the **replace** strategy. Verify the user sees a clean restart of the response, not a half-message.

**Advanced.** Implement tool-call recovery with idempotent tools. The tool's first action is to look up its own `call_id` in a side log; if found, return the prior result without re-running. Kill the process between `TOOL_INVOKED` and `TOOL_RESULT` and confirm the tool runs exactly once.

---

**8. Ship it**

Artifact: a session log of one real conversation, plus a one-page write-up answering:

1. Which event types did your harness emit?
2. Which payloads went inline and which went to a blob store?
3. What `buildContext` strategy did you use? Show the function.
4. What happens to the log when the user starts a "new chat"? (Hint: it should not delete anything.)
5. Did you handle tool idempotency? How?
6. Show one timestamped sequence where the harness crashed and `wake(sessionId)` recovered.

A reviewer should be able to take your log, replay it through your `step` function, and reproduce the user's experience byte-for-byte. If they cannot, the harness is still hiding state somewhere.

---

</details>

## 9. 硬件主线的衔接

为什么这一讲会出现在硬件路线图里：

- **推理批处理偏爱无状态 harness（agent 运行时框架）。** 无状态的 step function 可以被池中任意 worker 调用，这就让推理后端能够跨会话批处理，而不是在单个会话内串行执行。KV-cache 复用变成按请求的决策，而不是按进程的决策。
- **边缘可恢复性很重要。** 远端站点的一台 Jetson 夜间断电后，应当在启动时从任务中途恢复。没有事件溯源的会话，这做不到；有了它，这只是 systemd unit 上的一个 `wake(sessionId)`。
- **对安全攸关的工作，审计胜过可观测性。** 当 agent 作用于硬件（机械臂、电源开关、车辆）时，你希望权威记录是事件日志，而不是一个已经死掉的进程的 OpenTelemetry trace。

---

## 关键要点

- 会话与上下文窗口属于不同的层。会话是数据库；上下文是查询结果。
- 会话是只追加的。状态是推导出来的，而非存储的。
- 这是把事件溯源应用到认知之上。给你可重放银行系统的那套工程纪律，同样给你可恢复的 agent。
- 一套可用的 schema 从六种事件起步：`MESSAGE_RECEIVED`、`LLM_CALLED`、`TOOL_INVOKED`、`TOOL_RESULT`、`GEN_START` / `GEN_CHUNK`、`GEN_COMPLETE`、`GEN_SENT`。大 payload 放到 blob store，事件中保留引用。
- `GEN_COMPLETE` 与 `GEN_SENT` 必须是独立的事件。没有这一拆分，崩溃恢复就无法判断用户是否已经看到响应。
- harness 变成一个无状态的解释器。每一步都加载日志、推导出上下文、运行模型，并追加新事件。
- `wake(sessionId)` 是通用的恢复与续跑入口。Cron、重试和崩溃恢复都走它。
- 「网关崩溃导致流式输出丢失」这个 bug 不是流式的问题，而是把上下文当内存的 bug。把生成过程持久化为事件，这个 bug 就消失了。
- 工具调用恢复要求工具幂等，或者升级给人处理。绝不要静默重试非幂等的工具。
- 要根除的两个反模式：进程内历史，以及任何未在日志中体现的副作用。

---

## 参考文献

- bswen — *What Is an AI Agent Harness?*：[https://docs.bswen.com/blog/2026-03-25-ai-agent-harness-explained/](https://docs.bswen.com/blog/2026-03-25-ai-agent-harness-explained/)
- Martin Fowler — *Event Sourcing*：[https://martinfowler.com/eaaDev/EventSourcing.html](https://martinfowler.com/eaaDev/EventSourcing.html)
- Greg Young — *CQRS and Event Sourcing*（演讲）：[https://www.youtube.com/watch?v=JHGkaShoyNs](https://www.youtube.com/watch?v=JHGkaShoyNs)
- Anthropic — Messages API 与流式：[https://docs.claude.com/en/api/messages-streaming](https://docs.claude.com/en/api/messages-streaming)
- Model Context Protocol — Tool 规范：[https://modelcontextprotocol.io/specification](https://modelcontextprotocol.io/specification)
- [Lecture 24 - Runtime 纪律与 AI Runtime 安全](Lecture-24.md)
- [Lecture 27 - AI 智能体系统的确定性启动](Lecture-27.md)
- [Lecture 35 - OpenClaw agent 循环](Lecture-35.md)
- [Lecture 02 - 什么是 AI 智能体 harness？](Lecture-02.md)
- [Lecture 03 - OpenCoven：本地 harness 基座](Lecture-03.md)

---

*下一讲：[Lecture 27](Lecture-27.md)*


<details>
<summary>English original</summary>

**9. Hardware-track tie-in**

Why this lecture lives in a hardware roadmap:

- **Inference batching loves stateless harnesses.** A stateless step function can be invoked from any worker in a pool, which lets your inference back-end batch across sessions instead of serializing within one. KV-cache reuse becomes a per-request decision, not a per-process one.
- **Edge resumability matters.** A Jetson at a remote site that loses power overnight should resume mid-task on boot. Without an event-sourced session that is impossible; with one it is a `wake(sessionId)` on the systemd unit.
- **Audit beats observability for safety-critical work.** When an agent acts on hardware (a robot arm, a power switch, a vehicle), you want the canonical record to be the event log, not OpenTelemetry traces of a now-dead process.

---

**Key takeaways**

- Session and context window are different layers. Session is the database; context is a query result.
- The session is append-only. State is derived, not stored.
- This is event sourcing applied to cognition. The same engineering discipline that gives you replayable banking systems gives you resumable agents.
- A workable schema starts at six events: `MESSAGE_RECEIVED`, `LLM_CALLED`, `TOOL_INVOKED`, `TOOL_RESULT`, `GEN_START` / `GEN_CHUNK`, `GEN_COMPLETE`, `GEN_SENT`. Large payloads go to a blob store with a reference in the event.
- `GEN_COMPLETE` and `GEN_SENT` must be separate events. Without that split, crash recovery cannot tell whether the user already saw the response.
- The harness becomes a stateless interpreter. Every step loads the log, derives the context, runs the model, and appends new events.
- `wake(sessionId)` is the universal recovery and resume entry point. Cron, retries, and crash recovery all use it.
- The "streaming output lost on gateway crash" bug is not a streaming bug. It is a context-as-memory bug. Persist generation as events and the bug disappears.
- Tool-call recovery requires idempotent tools or human escalation. Never silently retry a non-idempotent tool.
- Two anti-patterns to root out: in-process history, and any side effect not represented in the log.

---

**References**

- bswen — *What Is an AI Agent Harness?*: [https://docs.bswen.com/blog/2026-03-25-ai-agent-harness-explained/](https://docs.bswen.com/blog/2026-03-25-ai-agent-harness-explained/)
- Martin Fowler — *Event Sourcing*: [https://martinfowler.com/eaaDev/EventSourcing.html](https://martinfowler.com/eaaDev/EventSourcing.html)
- Greg Young — *CQRS and Event Sourcing* (talk): [https://www.youtube.com/watch?v=JHGkaShoyNs](https://www.youtube.com/watch?v=JHGkaShoyNs)
- Anthropic — Messages API and streaming: [https://docs.claude.com/en/api/messages-streaming](https://docs.claude.com/en/api/messages-streaming)
- Model Context Protocol — Tool spec: [https://modelcontextprotocol.io/specification](https://modelcontextprotocol.io/specification)
- [Lecture 24 - Runtime Discipline & AI Runtime Security](Lecture-24.md)
- [Lecture 27 - Deterministic Startup for AI Agent Systems](Lecture-27.md)
- [Lecture 35 - OpenClaw Agent Loop](Lecture-35.md)
- [Lecture 02 - What Is an AI Agent Harness?](Lecture-02.md)
- [Lecture 03 - OpenCoven: Local Harness Substrate](Lecture-03.md)

---

*Next: [Lecture 27](Lecture-27.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-26.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-26.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
