---
title: Lecture 09 - Structured Tools Beat Computer Use: Interface Hierarchy for Agents
description: Lecture 09 - Structured Tools Beat Computer Use: Interface Hierarchy for Agents
---

# Lecture 09 - Structured Tools Beat Computer Use: Interface Hierarchy for Agents

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 08](Lecture-08.md) | **Next:** [Lecture 10](Lecture-10.md)

---

**computer-use agent** 在视觉上很惊艳。

它们能看截图、点按钮、往输入框里打字，像人一样操作软件。

但这并不意味着 **vision** 就是 agent 正确的主接口。

Reflex benchmark 给出了一个有用的数据点：

```text
same app
same task
same model family
vision path: 53 ± 13 steps, ~551k input tokens, ~17 minutes
API path:    8 calls,       ~12k input tokens, ~20 seconds
```

结论不是「绝不要用 vision」。

结论是：

```text
Vision-based computer use is a fallback interface.
Structured tools are the primary interface when you control the system.
```

对 OpenClaw 风格的架构而言，这不是一个小优化。

它是一条 **工具设计原则**。

---

## 学习目标

学完本讲后，你应当能够：

1. 解释为什么截图驱动的 agent 昂贵且不确定。
2. 比较结构化 API、CLI/直接执行、DOM/无障碍接口与 vision 接口。
3. 解释 Reflex benchmark 及其局限。
4. 为 agent 工具设计接口层级。
5. 判断何时适合用 vision，何时是浪费。
6. 把结构化工具与验证、安全、可审计性联系起来。
7. 把该原则应用到 OpenClaw Gateway 工具、node 命令、exec 与 App SDK 接口面。

---

## 1. 直白地看这个 benchmark

Reflex 测试了 Claude Sonnet 操作同一个 admin panel 的 **两种方式**。

任务：

```text
find the customer named Smith with the most orders
locate their most recent pending order
accept all of their pending reviews
mark the order as delivered
```

路径 A：

```text
vision agent
  -> browser-use
  -> screenshots
  -> clicks
  -> rendered UI state
```

路径 B：

```text
API agent
  -> tool calls
  -> HTTP endpoints mapped to app handlers
  -> structured JSON responses
```

结果：

| 指标 | Vision agent | API agent |
|---|---:|---:|
| 步骤数 / 调用数 | 53 ± 13 | 8 ± 0 |
| 墙钟时间 | 1003s ± 254s | 19.7s ± 2.8s |
| 输入 token | 550,976 ± 178,849 | 12,151 ± 27 |
| 输出 token | 37,962 ± 10,850 | 934 ± 41 |

API 路径每次都只用 8 次调用就完成。

vision 路径**最初漏掉了工作**，因为并非所有待处理的 review 都显示在屏幕上。它需要 14 步的 UI 走查才成功完成。

这趟走查本身就是**工程成本**。

---

## 2. 为什么 vision 昂贵

vision agent 在**每一步都要为感知付账**。

每一轮循环看起来像：

```text
screenshot
  -> interpret pixels
  -> infer UI state
  -> decide action
  -> click/type
  -> wait
  -> screenshot again
```

成本会叠加，因为：

- 截图是很大的输入
- UI 状态必须反复重新发现
- 滚动/分页可能是不可见的
- 每个动作都是串行的
- 每次屏幕切换都会增加一次模型调用
- 模型必须从布局和像素中推断含义

结构化 API 能避开其中大部分。

API 循环：

```text
call tool
  -> receive JSON
  -> choose next tool
  -> verify result
```

agent **直接读取数据**，而不是从像素中重新推导。

---

## 3. 接口带宽层级

使用可用的**最高带宽接口**。

```text
Structured API / typed tool call       best
CLI / direct execution                 good
DOM / accessibility tree               acceptable
Vision / screenshots                   fallback
```

这个层级为何存在：

| 接口 | 信号质量 | 确定性 | 成本 | 安全形态 |
|---|---|---|---|---|
| 结构化 API | 高 | 高 | 低 | scoped 契约 |
| CLI/直接 exec | 命令稳定时高 | 中/高 | 低/中 | 需要命令策略 |
| DOM/无障碍 | 中 | 中 | 中 | 依赖 app/UI 状态 |
| Vision | 低/中 | 低 | 高 | 宽泛的可见权限 |

规则是：

```text
If a task can be expressed as a function, do not do it through screenshots.
```

---

## 4. 结构化工具不只是更便宜

成本只是**一个维度**。

结构化工具还能改善：

### 确定性

```json
{ "tool": "update_order", "args": { "id": 123, "status": "delivered" } }
```

比下面这个更确定：

```text
click the button below the order status field
```

### 可组合性

工具调用可以被**串联、重试、校验和记录**。

### 可验证性

你可以断言：

```text
response.status == "delivered"
```

并保留精确的证据。

### 安全性

结构化工具可以只暴露很窄的能力：

```text
list_customers
accept_review
update_order_status
```

vision 暴露的是 **agent 能看到、能点到的一切**。

### 可审计性

结构化日志会准确告诉你发生了什么：

```text
tool=update_order_status id=123 from=pending to=delivered actor=session-abc
```

截图则需要重建。

---


<details>
<summary>English original</summary>

**Lecture 09 - Structured Tools Beat Computer Use: Interface Hierarchy for Agents**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 08](Lecture-08.md) | **Next:** [Lecture 10](Lecture-10.md)

---

**Computer-use agents** are visually impressive.

They can look at screenshots, click buttons, type into fields, and operate software like a human.

That does not mean **vision** is the right primary interface for agents.

The Reflex benchmark gives a useful data point:

```text
same app
same task
same model family
vision path: 53 ± 13 steps, ~551k input tokens, ~17 minutes
API path:    8 calls,       ~12k input tokens, ~20 seconds
```

The conclusion is not "never use vision."

The conclusion is:

```text
Vision-based computer use is a fallback interface.
Structured tools are the primary interface when you control the system.
```

For OpenClaw-style architecture, this is not a small optimization.

It is a **tool-design principle**.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Explain why screenshot-driven agents are expensive and nondeterministic.
2. Compare structured APIs, CLI/direct execution, DOM/accessibility, and vision interfaces.
3. Explain the Reflex benchmark and its limitations.
4. Design an interface hierarchy for agent tools.
5. Identify when vision is appropriate and when it is wasteful.
6. Connect structured tools to verification, security, and auditability.
7. Apply the principle to OpenClaw Gateway tools, node commands, exec, and App SDK surfaces.

---

**1. The benchmark in plain terms**

Reflex tested **two ways** for Claude Sonnet to operate the same admin panel.

Task:

```text
find the customer named Smith with the most orders
locate their most recent pending order
accept all of their pending reviews
mark the order as delivered
```

Path A:

```text
vision agent
  -> browser-use
  -> screenshots
  -> clicks
  -> rendered UI state
```

Path B:

```text
API agent
  -> tool calls
  -> HTTP endpoints mapped to app handlers
  -> structured JSON responses
```

Result:

| Metric | Vision agent | API agent |
|---|---:|---:|
| Steps / calls | 53 ± 13 | 8 ± 0 |
| Wall-clock time | 1003s ± 254s | 19.7s ± 2.8s |
| Input tokens | 550,976 ± 178,849 | 12,151 ± 27 |
| Output tokens | 37,962 ± 10,850 | 934 ± 41 |

The API path completed in 8 calls every time.

The vision path **initially missed work** because not all pending reviews were visible on screen. It needed a 14-step UI walkthrough to complete successfully.

That walkthrough is itself **engineering cost**.

---

**2. Why vision is expensive**

A vision agent pays for **perception every step**.

Each loop looks like:

```text
screenshot
  -> interpret pixels
  -> infer UI state
  -> decide action
  -> click/type
  -> wait
  -> screenshot again
```

Costs compound because:

- screenshots are large inputs
- UI state must be rediscovered repeatedly
- scrolling/pagination may be invisible
- every action is sequential
- each screen transition adds another model call
- the model must infer meaning from layout and pixels

Structured APIs avoid most of that.

API loop:

```text
call tool
  -> receive JSON
  -> choose next tool
  -> verify result
```

The agent **reads the data directly** instead of re-deriving it from pixels.

---

**3. Interface bandwidth hierarchy**

Use the **highest-bandwidth interface** available.

```text
Structured API / typed tool call       best
CLI / direct execution                 good
DOM / accessibility tree               acceptable
Vision / screenshots                   fallback
```

Why this hierarchy exists:

| Interface | Signal quality | Determinism | Cost | Security shape |
|---|---|---|---|---|
| Structured API | high | high | low | scoped contracts |
| CLI/direct exec | high if command is stable | medium/high | low/medium | command policy required |
| DOM/accessibility | medium | medium | medium | app/UI state dependent |
| Vision | low/medium | low | high | broad visible authority |

The rule:

```text
If a task can be expressed as a function, do not do it through screenshots.
```

---

**4. Structured tools are more than cheaper**

Cost is only **one dimension**.

Structured tools also improve:

**Determinism**

```json
{ "tool": "update_order", "args": { "id": 123, "status": "delivered" } }
```

is more deterministic than:

```text
click the button below the order status field
```

**Composability**

Tool calls can be **chained, retried, validated, and logged**.

**Verifiability**

You can assert:

```text
response.status == "delivered"
```

and preserve exact evidence.

**Security**

Structured tools can expose narrow capabilities:

```text
list_customers
accept_review
update_order_status
```

Vision exposes **whatever the agent can see and click**.

**Auditability**

Structured logs tell you exactly what happened:

```text
tool=update_order_status id=123 from=pending to=delivered actor=session-abc
```

Screenshots require reconstruction.

---

</details>

## 5. 为什么 vision 路径先失败

该 benchmark 的首次 vision 尝试漏掉了可见折线之下的待处理 review。

这主要不是**模型智能问题**。

这是**接口问题**。

UI 呈现的是部分渲染状态。

API 返回的是结构化的分页与结果数据。

使用截图的 agent 只能去推断：

```text
is this all the data?
is there pagination?
should I scroll?
did the filter apply?
what changed after the click?
```

API agent 读到的是：

```text
page
total results
review status
order status
```

两条路径底层是**同一套应用逻辑**。

只是其中一条路径把它直接暴露了出来。

---

## 6. vision 在什么情况下仍然合理

不必彻底移除 computer use。

**vision 有用**的场景：

- 你无法控制目标系统
- 没有 API
- 工作流是第三方 SaaS UI
- 你在做 UX/QA 验证
- 你在逆向工程遗留工作流
- 任务本质上是视觉性的
- 你需要与人类同等的行为表现

好的用法：

```text
vision agent explores unknown workflow
  -> extract actions and state
  -> design structured tools
  -> structured agent executes at scale
```

差的用法：

```text
vision agent repeatedly operates an internal app you control
even though the app can expose handlers or endpoints
```

vision 是**探索与兜底层**。

对于自有系统，它不应是**默认执行层**。

---

## 7. OpenClaw 架构上的含义

OpenClaw 的 **Gateway/tool 方向**与这个 benchmark 一致。

首选路径：

```text
Agent
  -> typed tool schema
  -> Gateway RPC / tool invoke
  -> policy and approvals
  -> execution layer
  -> structured result
  -> session/artifact evidence
```

Vision/computer-use 应以如下身份接入：

```text
fallback tool: computer_use
```

而不是作为主路径。

接口优先级示例：

| 任务 | 首选接口 |
|---|---|
| 运行本地命令 | `exec` / node `system.run` 带策略 |
| 查询应用状态 | Gateway RPC / 结构化 API |
| 更新内部记录 | 类型化 tool call |
| 检查渲染后的 UI | DOM/accessibility 快照 |
| 操作未知的 SaaS UI | vision 兜底 |

稳定的原则：

```text
Agents should behave like infrastructure when infrastructure interfaces exist.
```

它们应当只在被迫时才表现得像人。

---

## 8. Tool schema 层

结构化优先的 agent 平台需要一个 **tool schema 层**。

示例：

```json
{
  "name": "update_order_status",
  "description": "Update one order's delivery status.",
  "input_schema": {
    "type": "object",
    "required": ["order_id", "status"],
    "properties": {
      "order_id": { "type": "integer" },
      "status": {
        "type": "string",
        "enum": ["pending", "delivered", "cancelled"]
      }
    }
  }
}
```

执行路径：

```text
tool schema
  -> validation
  -> auth scope check
  -> approval policy if needed
  -> handler call
  -> structured response
  -> audit event
```

这与截图自动化正好相反。

模型通过**窄范围的类型化动作**表达意图。

由 runtime 决定**该动作是否被允许**。

---

## 9. 自动生成 tool

Reflex 那篇文章之所以重要，部分原因在于 Reflex 0.9 可以**把事件处理器暴露为 HTTP 端点**，从而降低 API 面的工程成本。

通用模式：

```text
OpenAPI -> tool schemas
GraphQL -> tool schemas
internal service definitions -> tool schemas
CLI specs -> tool wrappers
typed app handlers -> agent-callable endpoints
```

当 **API 生成成本很低**时，结论就会反转。

旧假设：

```text
writing APIs is expensive
so use screenshots
```

新可能：

```text
generate structured APIs cheaply
so avoid screenshots
```

对于类 OpenClaw 的系统，这意味着：

- 优先使用 Gateway RPC 方法面
- 暴露类型化的 node 命令
- 从 API spec 生成 tool 包装层
- 把 vision 保留为兜底适配器

---

## 10. 安全边界对比

结构化 tool：

```text
tool name
input schema
scope requirement
approval rule
handler
audit log
```

Vision agent：

```text
screenshot
natural-language reasoning
click/type action
implicit UI permissions
harder-to-parse evidence
```

结构化 tool **更容易保障安全**，因为动作在执行前就是明确的。

你可以问：

```text
Who called this?
Which scope allowed it?
Was approval required?
Which object changed?
What was the before/after state?
```

而在截图方案中，动作往往只是一个**底层点击**。

**语义含义**必须事后重建。

这对**审计与事件响应**而言更弱。

---


<details>
<summary>English original</summary>

**5. Why the vision path failed first**

The benchmark's first vision attempt missed pending reviews below the visible fold.

That is not primarily a **model-intelligence problem**.

It is an **interface problem**.

The UI showed a partial rendered state.

The API returned structured pagination and result data.

The agent using screenshots had to infer:

```text
is this all the data?
is there pagination?
should I scroll?
did the filter apply?
what changed after the click?
```

The API agent read:

```text
page
total results
review status
order status
```

The **same application logic** existed underneath both paths.

Only one path exposed it directly.

---

**6. When vision still makes sense**

Do not remove computer use entirely.

**Vision is useful** when:

- you do not control the target system
- there is no API
- the workflow is a third-party SaaS UI
- you are doing UX/QA validation
- you are reverse-engineering a legacy workflow
- the task is inherently visual
- you need human-parity behavior

Good use:

```text
vision agent explores unknown workflow
  -> extract actions and state
  -> design structured tools
  -> structured agent executes at scale
```

Bad use:

```text
vision agent repeatedly operates an internal app you control
even though the app can expose handlers or endpoints
```

Vision is an **exploration and fallback layer**.

It should not be the **default execution layer** for owned systems.

---

**7. OpenClaw architecture implication**

OpenClaw's **Gateway/tool direction** is aligned with this benchmark.

The preferred path:

```text
Agent
  -> typed tool schema
  -> Gateway RPC / tool invoke
  -> policy and approvals
  -> execution layer
  -> structured result
  -> session/artifact evidence
```

Vision/computer-use should plug in as:

```text
fallback tool: computer_use
```

not as the primary route.

Example interface priority:

| Task | Preferred interface |
|---|---|
| run local command | `exec` / node `system.run` with policy |
| query app state | Gateway RPC / structured API |
| update internal record | typed tool call |
| inspect rendered UI | DOM/accessibility snapshot |
| operate unknown SaaS UI | vision fallback |

The stable principle:

```text
Agents should behave like infrastructure when infrastructure interfaces exist.
```

They should behave like humans **only when forced to**.

---

**8. Tool schema layer**

A structured-first agent platform needs a **tool schema layer**.

Example:

```json
{
  "name": "update_order_status",
  "description": "Update one order's delivery status.",
  "input_schema": {
    "type": "object",
    "required": ["order_id", "status"],
    "properties": {
      "order_id": { "type": "integer" },
      "status": {
        "type": "string",
        "enum": ["pending", "delivered", "cancelled"]
      }
    }
  }
}
```

Execution path:

```text
tool schema
  -> validation
  -> auth scope check
  -> approval policy if needed
  -> handler call
  -> structured response
  -> audit event
```

This is the opposite of screenshot automation.

The model expresses intent through a **narrow typed action**.

The runtime decides **whether the action is allowed**.

---

**9. Auto-generating tools**

The Reflex article matters partly because Reflex 0.9 can **expose event handlers as HTTP endpoints**, reducing the engineering cost of the API surface.

General pattern:

```text
OpenAPI -> tool schemas
GraphQL -> tool schemas
internal service definitions -> tool schemas
CLI specs -> tool wrappers
typed app handlers -> agent-callable endpoints
```

The decision flips when **API generation is cheap**.

Old assumption:

```text
writing APIs is expensive
so use screenshots
```

New possibility:

```text
generate structured APIs cheaply
so avoid screenshots
```

For OpenClaw-like systems, this suggests:

- prefer Gateway RPC method surfaces
- expose typed node commands
- generate tool wrappers from API specs
- keep vision as a fallback adapter

---

**10. Security boundary comparison**

Structured tools:

```text
tool name
input schema
scope requirement
approval rule
handler
audit log
```

Vision agent:

```text
screenshot
natural-language reasoning
click/type action
implicit UI permissions
harder-to-parse evidence
```

Structured tools are **easier to secure** because the action is explicit before execution.

You can ask:

```text
Who called this?
Which scope allowed it?
Was approval required?
Which object changed?
What was the before/after state?
```

With screenshots, the action is often a **low-level click**.

The **semantic meaning** must be reconstructed.

That is weaker for **audit and incident response**.

---

</details>

## 11. 验证与 Agent 技能

Lecture 21 论证道：

```text
No evidence, no completion.
```

结构化工具让**证据更容易获得**。

示例：

```json
{
  "tool": "update_order_status",
  "result": {
    "order_id": 123,
    "old_status": "pending",
    "new_status": "delivered",
    "updated_at": "2026-05-06T12:00:00Z"
  }
}
```

该结果可以：

- 记录
- 断言
- 重放
- 挂载到一个会话
- 在 App SDK UI 中展示
- 由 final-answer hook 检查

视觉证据更重：

- 截图
- 光标操作
- OCR
- 自然语言描述
- 脆弱的 UI 状态

只在**必要时**使用视觉。

当**结构化证据**可用时，不要选择它。

---

## 12. OpenClaw 工具的设计规则

采用这条规则：

```text
Every recurring agent action should graduate toward a structured tool.
```

生命周期：

```text
one-off human action
  -> vision exploration
  -> manual CLI/API discovery
  -> typed tool schema
  -> policy/approval
  -> test fixture
  -> production tool
```

示例：

```text
Use computer_use to learn how an admin workflow behaves.
Then build update_customer, list_reviews, approve_review, update_order tools.
Then stop using computer_use for that workflow.
```

结果是**更快、更便宜、更安全，也更易于审查**。

---

## 13. 迷你实验：把 UI 工作流转换为工具

选择一个内部工作流：

- 批准一个用户
- 创建设备配对
- 更新一个订单
- 执行一次部署
- 截取一张节点截图
- 重启一个服务

步骤 1：写出 UI 路径：

```text
screen -> click -> form -> submit -> verify
```

步骤 2：找出底层的状态转换：

```text
object
allowed states
required fields
side effects
permissions
```

步骤 3：设计工具：

```text
name
input schema
output schema
required scope
approval rule
audit event
test fixture
```

步骤 4：定义何时仍允许使用视觉：

```text
only if structured tool is unavailable
only for exploration
only with explicit operator approval
```

---

## 14. Benchmark 练习

在一个你可控的小应用上复现 Reflex 对比。

测量：

| 指标 | 视觉/DOM 路径 | 结构化工具路径 |
|---|---:|---:|
| 步骤 |
| 墙钟时间 |
| 输入 token |
| 输出 token |
| 失效模式 |
| 所需的 prompt 指令 |
| 审计质量 |

然后回答：

```text
What did the vision agent need to rediscover?
What did the structured tool expose directly?
What evidence was easy to capture?
Which path would you trust in production?
```

---

## 关键要点

- 基于视觉的 computer use 是兜底方案，而不是自有系统的主要接口。
- Reflex benchmark 测出了巨大差距：视觉路径约 551k 输入 token、53 步，而 API 路径约 12k 输入 token、8 次调用。
- 视觉路径需要 14 步的 walkthrough 才能成功；那段 prompt 是未被计价的工程成本。
- 正确的层级是结构化 API、CLI/直接执行、DOM/accessibility，最后才是视觉。
- 结构化工具更便宜、更快、更具确定性、更易加固，也更易审计。
- OpenClaw 的网关/工具方向与这一原则一致。
- 反复出现的工作流应当从视觉探索升级为带类型、受策略门控的工具。

---

## 参考文献

- Reflex，"Computer use is 45x More Expensive Than Structured APIs"：[https://reflex.dev/blog/computer-use-is-45x-more-expensive-than-structured-apis/](https://reflex.dev/blog/computer-use-is-45x-more-expensive-than-structured-apis/)
- Reflex benchmark 仓库：[https://github.com/reflex-dev/agent-benchmark](https://github.com/reflex-dev/agent-benchmark)
- Lecture 39 - Gateway RPC Protocol：[Lecture-39.md](Lecture-39.md)
- Lecture 21 - Agent Skills：[Lecture-21.md](Lecture-21.md)
- Lecture 28 - Runtime Strategy：[Lecture-28.md](Lecture-28.md)

---

*下一讲：[Lecture 10](Lecture-10.md)*


<details>
<summary>English original</summary>

**11. Verification and Agent Skills**

Lecture 21 argued:

```text
No evidence, no completion.
```

Structured tools make **evidence easier**.

Example:

```json
{
  "tool": "update_order_status",
  "result": {
    "order_id": 123,
    "old_status": "pending",
    "new_status": "delivered",
    "updated_at": "2026-05-06T12:00:00Z"
  }
}
```

That result can be:

- logged
- asserted
- replayed
- attached to a session
- shown in an App SDK UI
- checked by a final-answer hook

Vision evidence is heavier:

- screenshots
- cursor actions
- OCR
- natural-language descriptions
- brittle UI state

Use vision **when necessary**.

Do not choose it when **structured evidence** is available.

---

**12. Design rule for OpenClaw tools**

Adopt this rule:

```text
Every recurring agent action should graduate toward a structured tool.
```

Lifecycle:

```text
one-off human action
  -> vision exploration
  -> manual CLI/API discovery
  -> typed tool schema
  -> policy/approval
  -> test fixture
  -> production tool
```

Example:

```text
Use computer_use to learn how an admin workflow behaves.
Then build update_customer, list_reviews, approve_review, update_order tools.
Then stop using computer_use for that workflow.
```

The result is **faster, cheaper, safer, and more reviewable**.

---

**13. Mini-lab: convert a UI workflow into tools**

Pick one internal workflow:

- approve a user
- create a device pairing
- update an order
- run a deployment
- capture a node screenshot
- restart a service

Step 1: write the UI path:

```text
screen -> click -> form -> submit -> verify
```

Step 2: identify the underlying state transition:

```text
object
allowed states
required fields
side effects
permissions
```

Step 3: design the tool:

```text
name
input schema
output schema
required scope
approval rule
audit event
test fixture
```

Step 4: define when vision is still allowed:

```text
only if structured tool is unavailable
only for exploration
only with explicit operator approval
```

---

**14. Benchmark exercise**

Recreate the Reflex comparison on a small app you control.

Measure:

| Metric | Vision/DOM path | Structured tool path |
|---|---:|---:|
| steps |
| wall-clock time |
| input tokens |
| output tokens |
| failure modes |
| required prompt instructions |
| audit quality |

Then answer:

```text
What did the vision agent need to rediscover?
What did the structured tool expose directly?
What evidence was easy to capture?
Which path would you trust in production?
```

---

**Key takeaways**

- Vision-based computer use is a fallback, not the primary interface for owned systems.
- The Reflex benchmark measured a large gap: roughly 551k input tokens and 53 steps for vision versus about 12k input tokens and 8 calls for API.
- The vision path required a 14-step walkthrough to succeed; that prompt is unpaid engineering cost.
- The right hierarchy is structured API, CLI/direct execution, DOM/accessibility, then vision.
- Structured tools are cheaper, faster, more deterministic, easier to secure, and easier to audit.
- OpenClaw's Gateway/tool direction is aligned with this principle.
- Recurring workflows should graduate from vision exploration into typed, policy-gated tools.

---

**References**

- Reflex, "Computer use is 45x More Expensive Than Structured APIs": [https://reflex.dev/blog/computer-use-is-45x-more-expensive-than-structured-apis/](https://reflex.dev/blog/computer-use-is-45x-more-expensive-than-structured-apis/)
- Reflex benchmark repo: [https://github.com/reflex-dev/agent-benchmark](https://github.com/reflex-dev/agent-benchmark)
- Lecture 39 - Gateway RPC Protocol: [Lecture-39.md](Lecture-39.md)
- Lecture 21 - Agent Skills: [Lecture-21.md](Lecture-21.md)
- Lecture 28 - Runtime Strategy: [Lecture-28.md](Lecture-28.md)

---

*Next: [Lecture 10](Lecture-10.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-09.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-09.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
