---
title: 第 38 讲 - OpenClaw 案例研究：App SDK dogfooding 与类型化 Gateway RPC
description: 第 38 讲 - OpenClaw 案例研究：App SDK dogfooding 与类型化 Gateway RPC
---

# 第 38 讲 - OpenClaw 案例研究：App SDK dogfooding 与类型化 Gateway RPC

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 37](Lecture-37.md) | **Next:** [Lecture 39](Lecture-39.md)

---

只有当外部应用无需了解私有内部实现就能使用 agent runtime 时，它才成为**平台**。

这正是 **OpenClaw App SDK** 的目的。

App SDK 以 `@openclaw/sdk` 发布，是运行在 OpenClaw 进程**之外**的应用所使用的公共客户端 API：

- 仪表盘
- 桌面客户端
- 移动客户端
- IDE 扩展
- CI 任务
- 管理工具
- 集成测试
- 诸如 OpenMeow 的配套应用

本讲阐述当前的蓝图：

> 让真实的外部应用通过类型化 Gateway RPC 使用 `@openclaw/sdk`，而不是强迫应用去抓取 CLI 输出、transcript 或 runtime 内部信息

---

## 学习目标

学完本讲后，你应该能够：

1. 解释 App SDK 与 Plugin SDK 的区别。
2. 描述真实外部应用使用 App SDK 的 happy path。
3. 理解为什么 Gateway WebSocket RPC 是正确的平台边界。
4. 解释 agent、会话、run、产物、工具、环境与任务如何融入面向应用的架构。
5. 理解当前 SDK 如今支持什么，以及什么仍属于明确保留的未来 surface。
6. 设计一个窄化的类型化 RPC surface，不混入无关职责。
7. 解释用真实应用做 dogfooding 如何稳定 SDK 契约。
8. 解释 node 如何在不变成 gateway 的前提下暴露远程设备与媒体能力。
9. 把权威的 runtime 状态与确定性的呈现元数据区分开。
10. 围绕归一化事件、等待、取消与不支持特性的错误来构建测试策略。

---

## 1. 整体图景

架构如下：

```text
External app / OpenMeow
        |
        v
@openclaw/sdk
        |
        v
Gateway WebSocket RPC
        |
        +-- agents / sessions / runs     # happy-path app control
        +-- artifacts.*                  # rich outputs: files/images/logs/etc.
        +-- tools.invoke                 # controlled tool execution
        +-- environments.*               # discover where work can run
        +-- task ledger                  # durable app-visible run/task state
```

关键的转变在于：

```text
Before:
  App knows internal Gateway/session/runtime details.

After:
  App uses typed SDK methods backed by discoverable Gateway RPCs.
```

OpenClaw 正是借此从「可用的内部系统」走向**「外部应用平台」**。

---

## 2. App SDK 与 Plugin SDK

OpenClaw 有两种不同的扩展 surface。

不要混用。

| SDK | 代码运行位置 | 用途 |
|---|---|---|
| App SDK | OpenClaw 之外 | 外部应用、仪表盘、脚本、CI 任务、IDE 客户端 |
| Plugin SDK | OpenClaw 之内 | provider、channel、hook、工具、runtime 插件 |

App SDK 连接到 Gateway。

Plugin SDK 从内部扩展 Gateway/runtime。

错误的思维模型：

```text
"SDK is SDK; app code and plugin code can share the same assumptions."
```

正确的思维模型：

```text
App SDK = remote client contract.
Plugin SDK = in-process extension contract.
```

这种分离对**认证、scope、错误处理、生命周期与兼容性**很重要。

---

## 3. `@openclaw/sdk` 中交付的内容

主入口为：

```ts
OpenClaw
```

它负责：

- transport
- 连接
- 请求/响应调用
- 事件处理
- 高层资源 helper

基础连接示例：

```ts
import { OpenClaw } from "@openclaw/sdk";

const oc = new OpenClaw({
  url: "ws://127.0.0.1:14565",
  token: process.env.OPENCLAW_GATEWAY_TOKEN,
  requestTimeoutMs: 30_000,
});

await oc.connect();
```

默认 transport 为：

```ts
GatewayClientTransport
```

测试可以传入一个实现 SDK transport 接口的自定义 transport，因此无需真实的 WebSocket 服务器即可测试集成逻辑。

---

## 4. 当前的高层 SDK helper

SDK 暴露资源 helper。

| Helper | 用途 |
|---|---|
| `oc.agents` | 列出 agent、获取 agent handle、从 agent 启动 run |
| `oc.runs` / `Run` | 创建、获取、等待、取消与流式传输 run |
| `oc.sessions` / `Session` | 创建会话、发送消息、patch、compact、abort |
| `oc.models` | 列出 model 并查看 model 认证状态 |
| `oc.tools` | 列出工具目录与生效的工具 |
| `oc.approvals` | 列出并处理审批请求 |
| `oc.rawEvents()` | 为高级场景检查原始 Gateway 帧 |

SDK 还导出诸如以下的类型：

- `AgentRunParams`
- `RunResult`
- `RunStatus`
- `OpenClawEvent`
- 相关的 RPC 与选择类型

设计目标是让应用作者使用这些 helper，而不是**手写 Gateway 帧**。

---


<details>
<summary>English original</summary>

**Lecture 38 - OpenClaw Case Study: App SDK Dogfooding and Typed Gateway RPCs**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 37](Lecture-37.md) | **Next:** [Lecture 39](Lecture-39.md)

---

An agent runtime becomes a **platform** only when external applications can use it without knowing private internals.

That is the purpose of the **OpenClaw App SDK**.

The App SDK, published as `@openclaw/sdk`, is the public client API for applications that run **outside** the OpenClaw process:

- dashboards
- desktop clients
- mobile clients
- IDE extensions
- CI jobs
- admin tools
- integration tests
- companion apps such as OpenMeow

This lecture explains the current blueprint:

> make `@openclaw/sdk` usable by real external apps through typed Gateway RPCs, instead of forcing apps to scrape CLI output, transcripts, or runtime internals

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Explain the difference between the App SDK and the Plugin SDK.
2. Describe the App SDK happy path for real external applications.
3. Understand why Gateway WebSocket RPC is the correct platform boundary.
4. Explain how agents, sessions, runs, artifacts, tools, environments, and tasks fit into the app-facing architecture.
5. Understand what the current SDK supports today versus what remains explicit future surface.
6. Design a narrow typed RPC surface without mixing unrelated responsibilities.
7. Explain how dogfooding with a real app stabilizes SDK contracts.
8. Explain how nodes expose remote device and media capabilities without becoming gateways.
9. Separate authoritative runtime state from deterministic presentation metadata.
10. Build a testing strategy around normalized events, waits, cancellation, and unsupported feature errors.

---

**1. Big picture**

The architecture is:

```text
External app / OpenMeow
        |
        v
@openclaw/sdk
        |
        v
Gateway WebSocket RPC
        |
        +-- agents / sessions / runs     # happy-path app control
        +-- artifacts.*                  # rich outputs: files/images/logs/etc.
        +-- tools.invoke                 # controlled tool execution
        +-- environments.*               # discover where work can run
        +-- task ledger                  # durable app-visible run/task state
```

This is the important shift:

```text
Before:
  App knows internal Gateway/session/runtime details.

After:
  App uses typed SDK methods backed by discoverable Gateway RPCs.
```

That is how OpenClaw moves from "working internal system" to **"external app platform."**

---

**2. App SDK versus Plugin SDK**

OpenClaw has two different extension surfaces.

Do not mix them.

| SDK | Where Code Runs | Use It For |
|---|---|---|
| App SDK | Outside OpenClaw | External apps, dashboards, scripts, CI jobs, IDE clients |
| Plugin SDK | Inside OpenClaw | Providers, channels, hooks, tools, runtime plugins |

The App SDK connects to a Gateway.

The Plugin SDK extends the Gateway/runtime from inside.

Wrong mental model:

```text
"SDK is SDK; app code and plugin code can share the same assumptions."
```

Correct mental model:

```text
App SDK = remote client contract.
Plugin SDK = in-process extension contract.
```

This separation matters for **auth, scopes, error handling, lifecycle, and compatibility**.

---

**3. What ships in `@openclaw/sdk`**

The main entry is:

```ts
OpenClaw
```

It owns:

- transport
- connection
- request/response calls
- event handling
- high-level resource helpers

Basic connection example:

```ts
import { OpenClaw } from "@openclaw/sdk";

const oc = new OpenClaw({
  url: "ws://127.0.0.1:14565",
  token: process.env.OPENCLAW_GATEWAY_TOKEN,
  requestTimeoutMs: 30_000,
});

await oc.connect();
```

The default transport is:

```ts
GatewayClientTransport
```

Tests can pass a custom transport implementing the SDK transport interface, so integration logic can be tested without a real WebSocket server.

---

**4. The current high-level SDK helpers**

The SDK exposes resource helpers.

| Helper | Purpose |
|---|---|
| `oc.agents` | List agents, get agent handles, start runs from agents |
| `oc.runs` / `Run` | Create, get, wait, cancel, and stream runs |
| `oc.sessions` / `Session` | Create sessions, send messages, patch, compact, abort |
| `oc.models` | List models and inspect model auth status |
| `oc.tools` | List tool catalog and effective tools |
| `oc.approvals` | List and resolve approval requests |
| `oc.rawEvents()` | Inspect raw Gateway frames for advanced cases |

The SDK also exports types such as:

- `AgentRunParams`
- `RunResult`
- `RunStatus`
- `OpenClawEvent`
- related RPC and selection types

The design goal is that app authors use these helpers instead of **hand-writing Gateway frames**.

---

</details>

## 5. SDK happy path

happy path 是**最小 app 流程**，必须可靠到无聊。

```text
Connect
  -> Discover
  -> Create or resume session
  -> Start run
  -> Stream events
  -> Wait for result
  -> Cancel if needed
  -> Surface approvals
```

这条路径重要，是因为大多数外部 app 需要的正是这个循环：

```text
User clicks "Run"
  -> app sends task
  -> assistant streams output
  -> tools emit progress
  -> approvals may be requested
  -> app shows final result
```

如果这条路径不稳定，每个外部 app 都会变成**一堆特例**。

---

## 6. 运行 agent

典型的高层 app 流程：

```ts
const agent = await oc.agents.get("default");

const run = await agent.run({
  message: "Summarize the current project status.",
  sessionKey: "main",
  model: "openai/gpt-5.5",
  timeoutMs: 30_000,
});

for await (const event of run.events()) {
  if (event.type === "assistant.delta") {
    process.stdout.write(String(event.data));
  }

  if (event.type === "run.completed") {
    break;
  }
}

const result = await run.wait();
```

重要的 SDK 行为：

- provider 限定的 model ref（如 `openai/gpt-5.5`）在发送给 Gateway 之前，会被拆分为 provider 和 model 覆盖项
- SDK `timeoutMs` 为毫秒级
- Gateway 超时值可能以秒为单位发送
- `Run.events()` 将事件过滤到单个 run
- `Run.events()` 可以为快速运行重放已见过的事件
- `Run.wait()` 将 Gateway 生命周期结果映射为稳定的 SDK 结果形态

app 不需要知道 **agent 内部循环实现**。

---

## 7. 会话

会话是**持久化的 transcript 持有者**。

它们为 app 提供**稳定的上下文**和会话亲和行为。

示例：

```ts
const session = await oc.sessions.create({
  agentId: "default",
  label: "Hardware debug session",
});

const run = await session.send({
  message: "Review the latest UART bring-up notes.",
});
```

会话句柄可支持以下操作：

- send
- abort
- patch
- compact
- 检查元数据

当 app 需要持久对话状态时，使用会话。

当 app 需要干净的一次性工作时，使用隔离的 run。

---

## 8. 事件流与归一化

外部 app 不应直接消费**原始 Gateway 内部结构**。

SDK 将 Gateway 事件归一化为稳定的信封：

```ts
type OpenClawEvent = {
  version: 1;
  id: string;
  ts: number;
  type: OpenClawEventType;
  runId?: string;
  sessionId?: string;
  sessionKey?: string;
  taskId?: string;
  agentId?: string;
  data: unknown;
  raw?: GatewayEvent;
};
```

常见的映射事件类型包括：

- `run.started`
- `run.completed`
- `run.failed`
- `run.cancelled`
- `run.timed_out`
- `assistant.delta`
- `assistant.message`
- `thinking.delta`
- `tool.call.*`
- `approval.requested`
- `approval.resolved`
- `session.created`
- `session.updated`
- `session.compacted`
- `task.updated`
- `artifact.updated`
- `raw`

`raw` 逃生舱口对高级客户端有用，但普通 app 应优先使用稳定的 SDK 事件类型。

---

## 9. 为什么事件泄漏很危险

如果原始 chat 或 runtime 事件通过 `run.events()` 泄漏出去，app 就会**与私有实现细节耦合**。

坏模式：

```text
UI reducer handles internal provider chunks, chat frames, and lifecycle frames directly.
```

好模式：

```text
Gateway event
  -> SDK normalizer
  -> stable OpenClawEvent
  -> app adapter
  -> UI reducer
```

如果内部事件泄漏，runtime 变更时客户端就会崩溃。

SDK 应当成为**兼容层**。

---

## 10. 等待与取消语义

Run 生命周期必须在三个表面上保持一致：

```text
Run.cancel()
Run.events()
Run.wait()
```

SDK 应避免矛盾状态。

坏状态：

```text
cancel() returns cancelled
events emit run.completed
wait() returns timed_out
```

好状态：

```text
cancel requested
events eventually emit run.cancelled
wait() returns cancelled
```

重要的细节：

`Run.cancel()` 是**停止工作的请求**。

真正的终止状态由 **runtime 确认**。

`Run.wait()` 应返回归一化的状态，例如：

- `completed`
- `failed`
- `cancelled`
- `timed_out`
- `accepted`

如果等待截止时间到期而 run 仍处于活动状态，SDK 应返回已接受或仍活动的结果，而不是假装 run 本身失败了。

---


<details>
<summary>English original</summary>

**5. The SDK happy path**

The happy path is the **minimum app flow** that must be boringly reliable.

```text
Connect
  -> Discover
  -> Create or resume session
  -> Start run
  -> Stream events
  -> Wait for result
  -> Cancel if needed
  -> Surface approvals
```

This path matters because most external apps need exactly this loop:

```text
User clicks "Run"
  -> app sends task
  -> assistant streams output
  -> tools emit progress
  -> approvals may be requested
  -> app shows final result
```

If this path is unstable, every external app becomes a **pile of special cases**.

---

**6. Running an agent**

A typical high-level app flow:

```ts
const agent = await oc.agents.get("default");

const run = await agent.run({
  message: "Summarize the current project status.",
  sessionKey: "main",
  model: "openai/gpt-5.5",
  timeoutMs: 30_000,
});

for await (const event of run.events()) {
  if (event.type === "assistant.delta") {
    process.stdout.write(String(event.data));
  }

  if (event.type === "run.completed") {
    break;
  }
}

const result = await run.wait();
```

Important SDK behavior:

- provider-qualified model refs such as `openai/gpt-5.5` are split into provider and model overrides before being sent to the Gateway
- SDK `timeoutMs` is milliseconds
- Gateway timeout values may be sent as seconds
- `Run.events()` filters events to one run
- `Run.events()` can replay already-seen events for fast runs
- `Run.wait()` maps Gateway lifecycle outcomes into stable SDK result shapes

The app does not need to know the **internal agent loop implementation**.

---

**7. Sessions**

Sessions are **durable transcript holders**.

They give apps **stable context** and session-affine behavior.

Example:

```ts
const session = await oc.sessions.create({
  agentId: "default",
  label: "Hardware debug session",
});

const run = await session.send({
  message: "Review the latest UART bring-up notes.",
});
```

Session handles can support operations such as:

- send
- abort
- patch
- compact
- inspect metadata

Use sessions when the app wants durable conversation state.

Use isolated runs when the app wants clean one-shot work.

---

**8. Event streaming and normalization**

External apps should not consume **raw Gateway internals** directly.

The SDK normalizes Gateway events into a stable envelope:

```ts
type OpenClawEvent = {
  version: 1;
  id: string;
  ts: number;
  type: OpenClawEventType;
  runId?: string;
  sessionId?: string;
  sessionKey?: string;
  taskId?: string;
  agentId?: string;
  data: unknown;
  raw?: GatewayEvent;
};
```

Common mapped event types include:

- `run.started`
- `run.completed`
- `run.failed`
- `run.cancelled`
- `run.timed_out`
- `assistant.delta`
- `assistant.message`
- `thinking.delta`
- `tool.call.*`
- `approval.requested`
- `approval.resolved`
- `session.created`
- `session.updated`
- `session.compacted`
- `task.updated`
- `artifact.updated`
- `raw`

The `raw` escape hatch is useful for advanced clients, but normal apps should prefer stable SDK event types.

---

**9. Why event leakage is dangerous**

If raw chat or runtime events leak through `run.events()`, the app becomes **coupled to private implementation details**.

Bad pattern:

```text
UI reducer handles internal provider chunks, chat frames, and lifecycle frames directly.
```

Good pattern:

```text
Gateway event
  -> SDK normalizer
  -> stable OpenClawEvent
  -> app adapter
  -> UI reducer
```

If internal events leak, clients break when the runtime changes.

The SDK should be the **compatibility layer**.

---

**10. Wait and cancellation semantics**

Run lifecycle must be consistent across three surfaces:

```text
Run.cancel()
Run.events()
Run.wait()
```

The SDK should avoid contradictory states.

Bad state:

```text
cancel() returns cancelled
events emit run.completed
wait() returns timed_out
```

Good state:

```text
cancel requested
events eventually emit run.cancelled
wait() returns cancelled
```

Important nuance:

`Run.cancel()` is a **request to stop work**.

The true terminal state is **confirmed by the runtime**.

`Run.wait()` should return normalized statuses such as:

- `completed`
- `failed`
- `cancelled`
- `timed_out`
- `accepted`

If the wait deadline expires while the run is still active, the SDK should return an accepted or still-active result rather than pretending the run itself failed.

---

</details>

## 11. 当前已支持的与未来的 SDK 接口面

成熟的 SDK 不应假装**缺失的 Gateway RPC**已经存在。

当前 App SDK 的做法是显式的：

| Namespace | 当前形态 |
|---|---|
| `oc.agents` | 面向 App 的 helper 接口面，用于 agent 与 agent handle |
| `oc.runs` | Run 的创建、等待、取消、stream |
| `oc.sessions` | 持久化 session 管理与发送 |
| `oc.models` | 模型列举与 auth 状态 |
| `oc.tools` | 工具目录与生效的工具 |
| `oc.approvals` | 审批的列举与处理 |
| `oc.tasks` | 在 Gateway API 出现前显式不支持 |
| `oc.artifacts` | 在 artifact RPC 出现前显式不支持 |
| `oc.environments` | 在 environment RPC 出现前显式不支持 |
| `oc.tools.invoke` | 在 Gateway 工具调用出现前显式不支持 |

这是**良好的 API 设计**。

它避免了向不安全默认值的**静默回退**。

如果调用方在 Gateway 支持之前就传入仅属于未来的字段，例如 workspace、runtime、environment 或 approval 参数，SDK 应在发送请求之前抛错。

这比假装该设置已生效**更安全**。

---

## 12. 蓝图：类型化的 Gateway RPC

每一项面向 App 的能力都应成为一个**类型化的 Gateway RPC**。

实现模式：

```text
1. Protocol schema
   src/gateway/protocol/schema/*.ts

2. Protocol exports + validators
   src/gateway/protocol/index.ts
   src/gateway/protocol/schema/protocol-schemas.ts

3. Gateway method handler
   src/gateway/server-methods/*.ts

4. Method discovery
   src/gateway/server-methods-list.ts

5. Scope gate
   src/gateway/method-scopes.ts

6. SDK wrapper
   packages/sdk/src/client.ts
   packages/sdk/src/types.ts
   packages/sdk/src/index.ts

7. Generated native protocol models
   apps/macos/Sources/OpenClawProtocol/GatewayModels.swift
   apps/shared/OpenClawKit/Sources/OpenClawProtocol/GatewayModels.swift

8. Docs + changelog
   docs/concepts/openclaw-sdk.md
   docs/gateway/protocol.md
   CHANGELOG.md
```

确切的文件名可能变化，但架构规则是稳定的：

> schema 第一，server handler 第二，discovery 第三，SDK wrapper 第四，在对外称为 public 之前先生成原生模型与文档

---

## 13. Gateway RPC 帧格式

Gateway RPC 使用 WebSocket JSON 帧。

请求：

```json
{
  "type": "req",
  "id": "req-1",
  "method": "runs.create",
  "params": {}
}
```

响应：

```json
{
  "type": "res",
  "id": "req-1",
  "ok": true,
  "payload": {}
}
```

事件：

```json
{
  "type": "event",
  "family": "runs",
  "name": "run.delta",
  "payload": {}
}
```

这为 SDK 带来：

- 请求-响应关联
- 流式事件
- 类型化错误
- 功能发现
- 传输限制
- auth 与 scope 强制
- 兼容性检查

---

## 14. 能力边界规则

让 RPC 家族保持窄小。

```text
environments.* = read-only discovery
artifacts.*    = read-only output access/download
tools.invoke   = controlled execution with policy/approval
tasks.*        = durable task state
sessions/runs  = core app execution path
```

不要把这些捆绑成一个宽泛的“SDK platform”方法。

窄小的 RPC 接口面更易于：

- 评审
- 测试
- 保障安全
- 编写文档
- 版本管理
- 在原生 SDK 中暴露

SDK 正是以这种方式成长，**而不会变得不稳定**。

---

## 15. 作为 App 可见输出的 artifact

App 需要的输出比文本记录**更丰富**。

artifact 表示：

- 生成的文件
- 图片
- 日志
- 下载的文档
- 报告
- 工具输出包

一个干净的 artifact API 接口面大致如下：

```text
artifacts.list
artifacts.get
artifacts.download
```

为什么这重要：

```text
Without artifacts:
  App parses transcript text to find output files.

With artifacts:
  App asks the Gateway for structured output metadata and downloads.
```

artifact API 应当：

- 类型化
- 除非明确需要变更，否则只读
- 受 scope 门控
- 感知 payload 限制
- 可在 `hello-ok.features.methods` 中发现
- 由 SDK wrapper 支撑
- 有测试覆盖

大型 artifact 内容不应被盲目塞进 **WebSocket 帧**。

在适当的地方使用元数据、下载句柄或分块行为。

---

## 16. environment 发现

App 需要知道**工作可以在哪里运行**。

environment 发现起初是只读的：

```text
environments.list
environments.status
```

它可以暴露：

- Gateway 本地的 runtime 候选
- 节点候选
- 能力
- 健康状态
- 可用性
- 标签

它起初不应做：

- 置备
- 创建/删除
- runtime 选择
- 远程变更

这条边界是**有意为之**的。

发现比控制**更安全**。

App 可以向用户展示工作可以在哪里运行，而不被允许创建或销毁 environment。


<details>
<summary>English original</summary>

**11. Current supported versus future SDK surface**

A mature SDK should not pretend **missing Gateway RPCs** exist.

The current App SDK approach is explicit:

| Namespace | Current Shape |
|---|---|
| `oc.agents` | App-facing helper surface for agents and agent handles |
| `oc.runs` | Run creation, wait, cancel, stream |
| `oc.sessions` | Durable session management and sending |
| `oc.models` | Model listing and auth status |
| `oc.tools` | Tool catalog and effective tools |
| `oc.approvals` | Approval listing and resolution |
| `oc.tasks` | Explicitly unsupported until Gateway APIs exist |
| `oc.artifacts` | Explicitly unsupported until artifact RPCs exist |
| `oc.environments` | Explicitly unsupported until environment RPCs exist |
| `oc.tools.invoke` | Explicitly unsupported until Gateway tool invocation exists |

This is **good API design**.

It prevents **silent fallback** to unsafe defaults.

If a caller passes future-only fields such as workspace, runtime, environment, or approval parameters before the Gateway supports them, the SDK should throw before sending the request.

That is **safer** than pretending the setting worked.

---

**12. Blueprint: typed Gateway RPCs**

Every app-facing capability should become a **typed Gateway RPC**.

The implementation pattern:

```text
1. Protocol schema
   src/gateway/protocol/schema/*.ts

2. Protocol exports + validators
   src/gateway/protocol/index.ts
   src/gateway/protocol/schema/protocol-schemas.ts

3. Gateway method handler
   src/gateway/server-methods/*.ts

4. Method discovery
   src/gateway/server-methods-list.ts

5. Scope gate
   src/gateway/method-scopes.ts

6. SDK wrapper
   packages/sdk/src/client.ts
   packages/sdk/src/types.ts
   packages/sdk/src/index.ts

7. Generated native protocol models
   apps/macos/Sources/OpenClawProtocol/GatewayModels.swift
   apps/shared/OpenClawKit/Sources/OpenClawProtocol/GatewayModels.swift

8. Docs + changelog
   docs/concepts/openclaw-sdk.md
   docs/gateway/protocol.md
   CHANGELOG.md
```

The exact file names may evolve, but the architecture rule is stable:

> schema first, server handler second, discovery third, SDK wrapper fourth, generated native models and docs before calling it public

---

**13. Gateway RPC framing**

Gateway RPC uses WebSocket JSON frames.

Request:

```json
{
  "type": "req",
  "id": "req-1",
  "method": "runs.create",
  "params": {}
}
```

Response:

```json
{
  "type": "res",
  "id": "req-1",
  "ok": true,
  "payload": {}
}
```

Event:

```json
{
  "type": "event",
  "family": "runs",
  "name": "run.delta",
  "payload": {}
}
```

This gives SDKs:

- request-response correlation
- streaming events
- typed errors
- feature discovery
- transport limits
- auth and scope enforcement
- compatibility checks

---

**14. Capability boundary rule**

Keep RPC families narrow.

```text
environments.* = read-only discovery
artifacts.*    = read-only output access/download
tools.invoke   = controlled execution with policy/approval
tasks.*        = durable task state
sessions/runs  = core app execution path
```

Do not bundle these into one broad "SDK platform" method.

Narrow RPC surfaces are easier to:

- review
- test
- secure
- document
- version
- expose in native SDKs

This is how the SDK grows **without becoming unstable**.

---

**15. Artifacts as app-visible outputs**

Apps need **richer outputs** than text transcripts.

Artifacts represent:

- generated files
- images
- logs
- downloaded documents
- reports
- tool output bundles

A clean artifact API surface looks like:

```text
artifacts.list
artifacts.get
artifacts.download
```

Why this matters:

```text
Without artifacts:
  App parses transcript text to find output files.

With artifacts:
  App asks the Gateway for structured output metadata and downloads.
```

Artifact APIs should be:

- typed
- read-only unless mutation is explicitly needed
- scope-gated
- payload-limit aware
- discoverable in `hello-ok.features.methods`
- backed by SDK wrappers
- covered by tests

Large artifact content should not be shoved blindly into **WebSocket frames**.

Use metadata, download handles, or chunked behavior where appropriate.

---

**16. Environment discovery**

Apps need to know **where work can run**.

Environment discovery is read-only at first:

```text
environments.list
environments.status
```

It can expose:

- Gateway-local runtime candidates
- node candidates
- capabilities
- health
- availability
- labels

It should not initially do:

- provisioning
- create/delete
- runtime selection
- remote mutation

That boundary is **intentional**.

Discovery is **safer than control**.

The app can show users where work can run without being allowed to create or destroy environments.

</details>

### 作为 UI 元数据的设备型号数据库

一个具体的配套应用示例是 OpenClaw macOS 设备型号数据库。

在 Instances UI 中，原始的 Apple 型号标识符，例如：

```text
iPad16,6
Mac16,6
```

对用户并不友好。

macOS 应用使用位于以下路径的已 vendor 的 JSON 文件，将它们映射为人类可读的 Apple 设备名称：

```text
apps/macos/Sources/OpenClaw/Resources/DeviceModels/
```

这**不是新的 runtime 权威源**。

这是**应用侧的参考元数据**。

这一区分很重要：

```text
Stable device identity:
  model identifier, node id, instance id, capability fields

Friendly UI label:
  "iPad Pro ..." or "MacBook Pro ..."
```

不要将友好名称用于鉴权、策略、布线或兼容性决策。

仅将其用于显示。

OpenClaw 从 MIT 许可的 `kyle-seongwoo-jun/apple-device-identifiers` 仓库 vendor 这份映射，并将 JSON 文件固定到特定的上游 commit。所固定的 commit 哈希记录在：

```text
apps/macos/Sources/OpenClaw/Resources/DeviceModels/NOTICE.md
```

构建层面的经验很重要：

> 确定性的应用应当固定外部元数据、随附许可证，并保持清晰的更新流程

安全的更新流程是：

```bash
IOS_COMMIT="<commit sha for ios-device-identifiers.json>"
MAC_COMMIT="<commit sha for mac-device-identifiers.json>"

curl -fsSL "https://raw.githubusercontent.com/kyle-seongwoo-jun/apple-device-identifiers/${IOS_COMMIT}/ios-device-identifiers.json" \
  -o apps/macos/Sources/OpenClaw/Resources/DeviceModels/ios-device-identifiers.json

curl -fsSL "https://raw.githubusercontent.com/kyle-seongwoo-jun/apple-device-identifiers/${MAC_COMMIT}/mac-device-identifiers.json" \
  -o apps/macos/Sources/OpenClaw/Resources/DeviceModels/mac-device-identifiers.json

swift build --package-path apps/macos
```

同时验证：

- `NOTICE.md` 记录了确切固定的 commit
- `LICENSE.apple-device-identifiers.txt` 仍与上游一致
- 未知型号标识符回退到原始标识符，而不是让 UI 出错
- UI 将该数据库视为可选的展示数据

这与带类型的 RPC 是同一种平台纪律：

```text
runtime state should be authoritative
presentation metadata should be deterministic, pinned, licensed, and replaceable
```

### 节点与媒体作为远程外设

节点是**配套设备**，它们以如下方式连接到 Gateway WebSocket：

```json
{ "role": "node" }
```

示例：

- 以 node 模式运行的 macOS 菜单栏应用
- iOS 配套设备
- Android 配套设备
- Linux、macOS 或 Windows 上的无头 node host

历史上可能仍存在遗留的 TCP JSONL 桥接传输，但当前的心智模型是通过 Gateway 协议的 WebSocket 节点连接。

关键规则：

> 节点是外设，不是网关

它们不运行 **Gateway 服务**。

来自 Telegram、WhatsApp、WebChat 或其他渠道的消息仍落在 Gateway 上。Gateway 持有模型、会话布线、工具调用和策略。节点暴露 Gateway 可调用的设备本地能力。

macOS 可以通过菜单栏应用作为节点运行。在该模式下，它为该 Mac 暴露本地 canvas 和 camera 命令。在远程 gateway 模式下，浏览器自动化应由 CLI node host 或已安装的 node service 处理，而不是假定原生应用节点拥有全部远程自动化能力。

原始调用形式是：

```text
node.invoke
```

节点可以声明如下命令族：

```text
canvas.*
camera.*
screen.*
location.*
device.*
notifications.*
system.*
```

实际示例：

```bash
openclaw nodes status
openclaw nodes describe --node <idOrNameOrIp>
openclaw nodes invoke --node <idOrNameOrIp> --command canvas.eval --params '{"javaScript":"location.href"}'
```

对于常见的媒体工作流，存在更高层的辅助方法：

```bash
openclaw nodes canvas snapshot --node <idOrNameOrIp> --format png
openclaw nodes camera list --node <idOrNameOrIp>
openclaw nodes camera snap --node <idOrNameOrIp> --facing front
openclaw nodes screen record --node <idOrNameOrIp> --duration 10s --fps 10
openclaw nodes location get --node <idOrNameOrIp>
```

面向应用的经验教训：

```text
Do not make the agent pretend the camera, screen, or canvas is local.
Represent them as node capabilities behind Gateway-mediated commands.
```


<details>
<summary>English original</summary>

**Device model database as UI metadata**

A concrete companion-app example is the OpenClaw macOS device model database.

In the Instances UI, raw Apple model identifiers such as:

```text
iPad16,6
Mac16,6
```

are not friendly for users.

The macOS app maps them to human-readable Apple device names using vendored JSON files under:

```text
apps/macos/Sources/OpenClaw/Resources/DeviceModels/
```

This is **not a new runtime authority**.

It is **app-side reference metadata**.

That distinction matters:

```text
Stable device identity:
  model identifier, node id, instance id, capability fields

Friendly UI label:
  "iPad Pro ..." or "MacBook Pro ..."
```

Do not use friendly names for auth, policy, routing, or compatibility decisions.

Use them for display.

OpenClaw vendors this mapping from the MIT-licensed `kyle-seongwoo-jun/apple-device-identifiers` repository and pins the JSON files to specific upstream commits. The pinned commit hashes are recorded in:

```text
apps/macos/Sources/OpenClaw/Resources/DeviceModels/NOTICE.md
```

The build lesson is important:

> deterministic apps should pin external metadata, vendor the license, and keep a clear update procedure

A safe update flow is:

```bash
IOS_COMMIT="<commit sha for ios-device-identifiers.json>"
MAC_COMMIT="<commit sha for mac-device-identifiers.json>"

curl -fsSL "https://raw.githubusercontent.com/kyle-seongwoo-jun/apple-device-identifiers/${IOS_COMMIT}/ios-device-identifiers.json" \
  -o apps/macos/Sources/OpenClaw/Resources/DeviceModels/ios-device-identifiers.json

curl -fsSL "https://raw.githubusercontent.com/kyle-seongwoo-jun/apple-device-identifiers/${MAC_COMMIT}/mac-device-identifiers.json" \
  -o apps/macos/Sources/OpenClaw/Resources/DeviceModels/mac-device-identifiers.json

swift build --package-path apps/macos
```

Also verify that:

- `NOTICE.md` records the exact pinned commits
- `LICENSE.apple-device-identifiers.txt` still matches upstream
- unknown model identifiers fall back to the raw identifier instead of breaking the UI
- the UI treats this database as optional presentation data

This is the same platform discipline as typed RPCs:

```text
runtime state should be authoritative
presentation metadata should be deterministic, pinned, licensed, and replaceable
```

**Nodes and media as remote peripherals**

Nodes are **companion devices** connected to the Gateway WebSocket with:

```json
{ "role": "node" }
```

Examples:

- macOS menubar app running in node mode
- iOS companion device
- Android companion device
- headless node host on Linux, macOS, or Windows

Legacy TCP JSONL bridge transport may still exist historically, but the current mental model is WebSocket node connection through the Gateway protocol.

The key rule:

> nodes are peripherals, not gateways

They do not run the **Gateway service**.

Messages from Telegram, WhatsApp, WebChat, or other channels still land on the Gateway. The Gateway owns the model, session routing, tool calls, and policy. Nodes expose device-local capabilities that the Gateway can invoke.

macOS can run as a node through the menubar app. In that mode it exposes local canvas and camera commands for that Mac. In remote gateway mode, browser automation should be handled by the CLI node host or installed node service, not by assuming the native app node owns all remote automation.

The raw invocation shape is:

```text
node.invoke
```

A node can declare command families such as:

```text
canvas.*
camera.*
screen.*
location.*
device.*
notifications.*
system.*
```

Practical examples:

```bash
openclaw nodes status
openclaw nodes describe --node <idOrNameOrIp>
openclaw nodes invoke --node <idOrNameOrIp> --command canvas.eval --params '{"javaScript":"location.href"}'
```

Higher-level helpers exist for common media workflows:

```bash
openclaw nodes canvas snapshot --node <idOrNameOrIp> --format png
openclaw nodes camera list --node <idOrNameOrIp>
openclaw nodes camera snap --node <idOrNameOrIp> --facing front
openclaw nodes screen record --node <idOrNameOrIp> --duration 10s --fps 10
openclaw nodes location get --node <idOrNameOrIp>
```

The app-facing lesson:

```text
Do not make the agent pretend the camera, screen, or canvas is local.
Represent them as node capabilities behind Gateway-mediated commands.
```

</details>

#### 配对与持久节点身份

WebSocket 节点使用设备配对。

节点在连接时出示设备身份。Gateway 为 `role: node` 创建配对请求。操作员批准或拒绝该请求：

```bash
openclaw devices list
openclaw devices approve <requestId>
openclaw devices reject <requestId>
openclaw nodes status
```

该批准即 **持久角色契约**。

Token 轮换必须留在该契约之内。轮换后的 token 不应静默地把节点升级为其他角色或更广的命令面。

若节点重连时认证信息、作用域、公钥或命令声明发生变化，应把旧的 pending 请求视为过期，并批准当前请求。

这就避免了如下不安全状态：

```text
operator approved old capability set
node later exposes broader capability set
gateway accidentally trusts it
```

避免一种常见的配对混淆：

```text
device pairing:
  gates the WebSocket node role and approved role contract

gateway-owned node pairing store:
  supports older nodes pending/approve/reject/remove/rename flows
  does not gate the WebSocket connect handshake
```

#### 节点命令策略

节点命令在被调用前应通过 **两道关卡**：

```text
1. The node declared the command at connect time.
2. Gateway policy allows that declared command.
```

这对隐私敏感的能力尤其重要。

在已知平台上，安全或低风险命令可以默认允许：

```text
canvas.*
camera.list
location.get
screen.snapshot
```

更敏感的命令应要求显式 opt-in：

```text
camera.snap
camera.clip
screen.record
sms.send
system.run
system.which
```

保守规则：

> 未知的节点平台意味着保守的 allowlist

若 Gateway 无法识别节点平台或设备系列，就不应假定 `system.run` 或其他强力命令是安全的。

#### 远程节点主机与 `system.run`

**无头节点主机**是远程执行所用的模式。

在以下情况下使用：

```text
Gateway host:
  receives messages, runs the model, routes tool calls

Node host:
  executes selected system commands on another machine
```

启动节点主机：

```bash
openclaw node run --host <gateway-host> --port 18789 --display-name "Build Node"
```

若 Gateway 绑定到 loopback，则通过 SSH 隧道连接：

```bash
ssh -N -L 18790:127.0.0.1:18789 user@gateway-host

export OPENCLAW_GATEWAY_TOKEN="<gateway-token>"
openclaw node run --host 127.0.0.1 --port 18790 --display-name "Build Node"
```

然后把 exec 绑定到该节点：

```bash
openclaw config set tools.exec.host node
openclaw config set tools.exec.security allowlist
openclaw config set tools.exec.node "<id-or-name>"
```

exec 审批位于节点主机上：

```text
~/.openclaw/exec-approvals.json
```

这是有意为之。执行命令的机器强制执行 **本地审批与 allowlist 状态**。

重要的安全边界：

```text
shell execution should go through the exec path
explicit device commands should go through node.invoke
```

这种分离让审批、allowlist 与审计行为保持可理解。

对于由审批背书的节点执行，应绑定精确的已准备命令上下文。审批之后，Gateway 应转发存储的计划，而不是调用方后来编辑过的命令、工作目录或会话字段。

#### 媒体载荷设计

媒体命令常返回大载荷：

- canvas 截图
- 摄像头照片
- 摄像头短片
- 屏幕录制
- 设备上的最新照片

不要强制每个 app 从 transcript 文本中解析 base64。

更好的 app 架构是：

```text
node media command
  -> Gateway result
  -> artifact record or MEDIA attachment
  -> SDK event
  -> app renderer
```

这直接关系到 artifact API 的讨论：

```text
node commands produce media
artifact APIs make media discoverable and downloadable
SDK events tell the UI what changed
```

对 app 开发者而言，规则很简单：

> 通过结构化附件或 artifact 展示媒体，而不是抓取 transcript

---

## 17. 受控的工具调用

直接工具调用 **强大且危险**。

将来面向 SDK 的方法可以照搬现有 HTTP 工具调用行为，形如：

```text
tools.invoke
```

示例：

```json
{
  "type": "req",
  "id": "req-1",
  "method": "tools.invoke",
  "params": {
    "tool": "sessions_list",
    "action": "json",
    "args": {},
    "sessionKey": "main"
  }
}
```

重要规则：

> SDK 工具调用必须复用与现有服务端路径相同的 Gateway 认证、工具策略、deny-list、审批语义以及 owner/actor 语义

它绝不能成为 **绕过策略的捷径**。

工具调用涉及：

- 工具 allow/deny 策略
- 会话作用域
- agent 作用域
- 审批状态
- 确认与拒绝状态
- 审计日志
- 安全边界

正因如此，它 **比只读方法更难**。

---


<details>
<summary>English original</summary>

**Pairing and durable node identity**

WebSocket nodes use device pairing.

The node presents a device identity during connect. The Gateway creates a pairing request for `role: node`. An operator approves or rejects that request:

```bash
openclaw devices list
openclaw devices approve <requestId>
openclaw devices reject <requestId>
openclaw nodes status
```

This approval is the **durable role contract**.

Token rotation must stay inside that contract. A rotated token should not silently upgrade a node into a different role or broader command surface.

If a node reconnects with changed auth details, scopes, public key, or command declarations, treat the old pending request as stale and approve the current request.

That avoids this unsafe state:

```text
operator approved old capability set
node later exposes broader capability set
gateway accidentally trusts it
```

Avoid a common pairing confusion:

```text
device pairing:
  gates the WebSocket node role and approved role contract

gateway-owned node pairing store:
  supports older nodes pending/approve/reject/remove/rename flows
  does not gate the WebSocket connect handshake
```

**Node command policy**

A node command should pass **two gates** before invocation:

```text
1. The node declared the command at connect time.
2. Gateway policy allows that declared command.
```

This matters for privacy-heavy capabilities.

Safe or low-risk commands may be allowed by default on known platforms:

```text
canvas.*
camera.list
location.get
screen.snapshot
```

More sensitive commands should require explicit opt-in:

```text
camera.snap
camera.clip
screen.record
sms.send
system.run
system.which
```

The conservative rule:

> unknown node platform means conservative allowlist

If the Gateway cannot recognize the node platform or device family, it should not assume that `system.run` or other powerful commands are safe.

**Remote node host and `system.run`**

The **headless node host** is the pattern for remote execution.

Use it when:

```text
Gateway host:
  receives messages, runs the model, routes tool calls

Node host:
  executes selected system commands on another machine
```

Start a node host:

```bash
openclaw node run --host <gateway-host> --port 18789 --display-name "Build Node"
```

If the Gateway is bound to loopback, connect through an SSH tunnel:

```bash
ssh -N -L 18790:127.0.0.1:18789 user@gateway-host

export OPENCLAW_GATEWAY_TOKEN="<gateway-token>"
openclaw node run --host 127.0.0.1 --port 18790 --display-name "Build Node"
```

Then bind exec to the node:

```bash
openclaw config set tools.exec.host node
openclaw config set tools.exec.security allowlist
openclaw config set tools.exec.node "<id-or-name>"
```

Exec approvals live on the node host:

```text
~/.openclaw/exec-approvals.json
```

That is intentional. The machine executing the command enforces the **local approval and allowlist state**.

The important security boundary:

```text
shell execution should go through the exec path
explicit device commands should go through node.invoke
```

That separation keeps approvals, allowlists, and audit behavior understandable.

For approval-backed node execution, bind the exact prepared command context. After approval, the Gateway should forward the stored plan, not a later caller-edited command, working directory, or session field.

**Media payload design**

Media commands often return large payloads:

- canvas screenshots
- camera photos
- camera clips
- screen recordings
- latest photos from a device

Do not force every app to parse base64 from transcript text.

A better app architecture is:

```text
node media command
  -> Gateway result
  -> artifact record or MEDIA attachment
  -> SDK event
  -> app renderer
```

This connects directly to the artifact API discussion:

```text
node commands produce media
artifact APIs make media discoverable and downloadable
SDK events tell the UI what changed
```

For app developers, the rule is simple:

> display media through structured attachments or artifacts, not transcript scraping

---

**17. Controlled tool invocation**

Direct tool invocation is **powerful and risky**.

A future SDK-facing method could mirror the existing HTTP tool invoke behavior as:

```text
tools.invoke
```

Example:

```json
{
  "type": "req",
  "id": "req-1",
  "method": "tools.invoke",
  "params": {
    "tool": "sessions_list",
    "action": "json",
    "args": {},
    "sessionKey": "main"
  }
}
```

The important rule:

> SDK tool invocation must reuse the same Gateway auth, tool policy, deny-list, approval semantics, and owner/actor semantics as the existing server path

It must not become a **shortcut around policy**.

Tool invocation touches:

- tool allow/deny policy
- session scoping
- agent scoping
- approval states
- confirmation and refusal states
- audit logs
- security boundaries

That is why it is **harder than read-only methods**.

---

</details>

## 18. 任务账本

事件流是**瞬时**的。

应用还需要**持久任务状态**。

任务账本 API 给 UI 提供一种稳定的方式来询问：

```text
What work exists?
What is running?
What completed?
What failed?
What was cancelled?
What artifacts belong to this work?
```

可能的接口面：

```text
tasks.list
tasks.get
tasks.cancel
```

关键设计要点：

> 事件流用于实时更新；任务账本 API 用于持久的、应用可见的状态

不要让应用仅从历史事件流重建持久状态。

---

## 19. 发现与能力协商

网关连接应**声明能力**。

SDK 可以检查：

```text
hello-ok.features.methods
hello-ok.policy.maxPayload
hello-ok.policy.maxBufferedBytes
hello-ok.policy.tickIntervalMs
```

然后应用可以决定：

- 是否存在产物 API
- 是否存在环境发现
- 是否存在工具调用
- payload 是否过大
- 显示还是隐藏 UI 功能

这避免了**硬编码版本假设**。

**功能检测**胜过猜测。

---

## 20. 认证、scope 与 fail-closed 事件

SDK 方法必须受 scope 约束。

示例：

- `operator.read`
- `operator.write`
- `operator.admin`
- 插件定义的 scope

**服务端检查**是强制性的。

SDK **不是安全边界**。

事件还应受可见性约束。

安全规则：

```text
If the client should not see a session, run, task, artifact, or approval, do not broadcast the event to that client.
```

对应用开发者而言，这意味着：

- 预期权限错误
- 围绕缺失能力设计 UI
- 把功能发现视为动态的
- 不要假设 owner 级访问权限

---

## 21. 幂等性

有副作用的方法需要**幂等键**。

示例：

- 启动一次 run
- 取消一次 run
- 批准一次工具调用
- 调用一个工具
- 创建或修改一个产物

为什么？

因为真实客户端会重试。

网络会故障。

移动客户端会重连。

用户会双击。

没有幂等性时：

```text
one user action -> two runs
```

有幂等性时：

```text
same request key -> same accepted operation
```

幂等性是 **SDK 契约的一部分**，不是优化手段。

---

## 22. 用 OpenMeow 做 dogfooding

OpenMeow 式的 dogfooding 很有价值，因为它迫使 SDK 表现得像一个**真实的产品依赖**。

dogfood 客户端应验证：

- 连接建立
- 功能发现
- agent 发现
- 会话创建
- run 启动
- 事件流式传输
- 等待行为
- 取消
- 审批处理
- 产物展示
- 不支持功能的错误

应用不应调用网关私有的内部实现。

它应使用与外部开发者相同的 SDK 接口面。

如果应用需要绕行方案，那**SDK 契约**多半需要改进。

---

## 23. 测试策略

强健的 SDK 测试 harness 使用 fixture。

测试以下路径：

- 连接并发现
- 列出 agent
- 创建会话
- 启动 run
- 流式传输助手增量
- 流式传输工具事件
- 审批被请求并解决
- run 完成
- run 失败
- run 被取消
- 等待截止时间到期而 run 仍处于活动状态
- 原始事件规范化
- 未知事件族
- 不支持的 `oc.artifacts.*`
- 不支持的 `oc.environments.*`
- 不支持的 `oc.tasks.*`
- 不支持的 `oc.tools.invoke`
- 对未知 Apple 型号标识的设备型号查找回退
- 节点配对审批与过期请求替换
- 声明的节点命令被网关策略允许还是拒绝
- 节点媒体事件创建结构化附件或产物

目标不只是**正确性**。

目标是**契约稳定性**。

当网关演进时，SDK fixture 告诉你外部应用是否会坏。

---

## 24. 应用实践指南

对外部应用：

- 用 `Run.events()` 获取进度，而不是轮询
- 用 `Run.wait()` 获取最终生命周期结果
- 用会话保存持久记录
- 仅在高级诊断时使用原始事件
- 显示 UI 前先做功能检测网关方法
- 把不支持的 SDK 命名空间视为有意为之
- 在测试中使用自定义传输
- 一旦有产物 API，就不要解析记录来找文件
- 不要假设可直接调用工具
- 不要把 App SDK 和 Plugin SDK 的假设混在一起
- 把节点视为远程外设，而非替代网关
- 通过结构化附件或产物渲染节点媒体，而非解析记录

对 SDK 实现者：

- 保持命名空间狭窄
- 对不支持的未来字段大声失败
- 从协议 schema 生成类型
- 把认证和 scope 检查留在服务端
- 在暴露事件前先规范化
- 让取消语义具有确定性
- 把节点命令策略留在服务端，对未知平台 fail closed
- 先加 fixture，再扩展接口面

---


<details>
<summary>English original</summary>

**18. Task ledger**

Event streams are **transient**.

Apps also need **durable task state**.

A task ledger API gives UIs a stable way to ask:

```text
What work exists?
What is running?
What completed?
What failed?
What was cancelled?
What artifacts belong to this work?
```

Potential surface:

```text
tasks.list
tasks.get
tasks.cancel
```

The key design point:

> event streams are for live updates; task ledger APIs are for durable app-visible state

Do not make apps reconstruct durable state only from historical event streams.

---

**19. Discovery and feature negotiation**

Gateway connections should **advertise capabilities**.

The SDK can inspect:

```text
hello-ok.features.methods
hello-ok.policy.maxPayload
hello-ok.policy.maxBufferedBytes
hello-ok.policy.tickIntervalMs
```

Then the app can decide:

- whether artifact APIs exist
- whether environment discovery exists
- whether tool invocation exists
- whether the payload is too large
- whether to show or hide UI features

This avoids **hard-coded version assumptions**.

**Feature detection** beats guessing.

---

**20. Auth, scopes, and fail-closed events**

SDK methods must be scope-gated.

Examples:

- `operator.read`
- `operator.write`
- `operator.admin`
- plugin-defined scopes

**Server-side checks** are mandatory.

The SDK is **not a security boundary**.

Events should also be gated by visibility.

The safe rule:

```text
If the client should not see a session, run, task, artifact, or approval, do not broadcast the event to that client.
```

For app developers this means:

- expect permission errors
- design UI around missing capabilities
- treat feature discovery as dynamic
- do not assume owner-level access

---

**21. Idempotency**

Side-effecting methods need **idempotency keys**.

Examples:

- start a run
- cancel a run
- approve a tool call
- invoke a tool
- create or mutate an artifact

Why?

Because real clients retry.

Networks fail.

Mobile clients reconnect.

Users double-click.

Without idempotency:

```text
one user action -> two runs
```

With idempotency:

```text
same request key -> same accepted operation
```

Idempotency is **part of the SDK contract**, not an optimization.

---

**22. Dogfooding with OpenMeow**

OpenMeow-style dogfooding is valuable because it forces the SDK to behave like a **real product dependency**.

The dogfood client should validate:

- connection setup
- feature discovery
- agent discovery
- session creation
- run start
- event streaming
- wait behavior
- cancellation
- approval handling
- artifact display
- unsupported feature errors

The app should not call private Gateway internals.

It should use the same SDK surface an external developer would use.

If the app needs a workaround, the **SDK contract** probably needs work.

---

**23. Testing strategy**

A strong SDK test harness uses fixtures.

Test these paths:

- connect and discover
- list agents
- create a session
- start a run
- stream assistant deltas
- stream tool events
- approval requested and resolved
- run completed
- run failed
- run cancelled
- wait deadline expires while run remains active
- raw event normalization
- unknown event family
- unsupported `oc.artifacts.*`
- unsupported `oc.environments.*`
- unsupported `oc.tasks.*`
- unsupported `oc.tools.invoke`
- device model lookup fallback for unknown Apple model identifiers
- node pairing approval and stale request replacement
- declared node command allowed versus denied by Gateway policy
- node media event creates a structured attachment or artifact

The goal is not just **correctness**.

The goal is **contract stability**.

When the Gateway evolves, the SDK fixtures tell you whether external apps will break.

---

**24. Practical app guidance**

For external apps:

- use `Run.events()` for progress instead of polling
- use `Run.wait()` for final lifecycle result
- use sessions for durable transcripts
- use raw events only for advanced diagnostics
- feature-detect Gateway methods before showing UI
- treat unsupported SDK namespaces as intentional
- use a custom transport in tests
- do not parse transcripts to find files once artifact APIs exist
- do not assume direct tool invocation is available
- do not mix App SDK and Plugin SDK assumptions
- treat nodes as remote peripherals, not as alternate gateways
- render node media through structured attachments or artifacts, not transcript parsing

For SDK implementers:

- keep namespaces narrow
- fail loudly on unsupported future fields
- generate types from protocol schemas
- keep auth and scope checks server-side
- normalize events before exposing them
- make cancellation semantics deterministic
- keep node command policy server-side and fail closed for unknown platforms
- add fixtures before expanding surface area

---

</details>

## 25. 设计练习

仅使用 App SDK 设计一个小型 OpenClaw 仪表盘。

该仪表盘必须：

- 连接到网关
- 列出 agent
- 列出模型
- 创建会话
- 启动一次运行
- 流式接收 assistant 和 tool 事件
- 显示审批提示
- 等待最终状态
- 取消运行
- 如果网关支持产物 API，则显示产物
- 如果网关不支持产物 API，则隐藏产物 UI

答案：

1. 目前需要哪些 SDK 命名空间？
2. 哪些未来的命名空间应做特性检测？
3. 哪些操作需要幂等键？
4. 哪些事件会更新 UI 状态 reducer？
5. 如果 `Run.wait()` 因等待截止时间已过而返回 accepted，会发生什么？
6. 如何在没有真实网关的情况下测试该应用？
7. 什么必须留在 SDK 适配器中，而不是放在 UI 组件里？

---

## 26. 可使用 App SDK 的五种应用

当应用想要 OpenClaw 的 agent runtime 而**不嵌入 OpenClaw 本身**时，App SDK 就很有用。

以下是五种现实的应用模式。

### 1. 个人桌面控制中心

一个 macOS、Windows 或 Linux **桌面应用**，让用户管理 agent、会话、模型、审批、节点、截图和长时间运行的工作。

核心用户流程：

```text
open app
  -> connect to Gateway
  -> list agents and sessions
  -> start a run
  -> stream assistant and tool events
  -> approve or reject risky actions
  -> show artifacts and node media
```

SDK 接口：

- `oc.agents`
- `oc.sessions`
- `oc.runs`
- `oc.models`
- `oc.approvals`
- 未来的 `oc.artifacts`
- 经特性检测的 node/media 事件

为什么合适：

```text
The app is a remote operator UI. It should not run the agent loop locally.
```

### 2. 面向实验的 AI 实验室仪表盘

一个 **web 仪表盘**，用于在重复运行中比较 prompt、模型、agent 和 tool 行为。

核心用户流程：

```text
select agent + model
  -> run experiment batch
  -> stream outputs
  -> collect artifacts/logs
  -> compare final results
  -> export report
```

SDK 接口：

- `oc.agents`
- `oc.models`
- `oc.runs`
- `Run.events()`
- `Run.wait()`
- 未来的 `oc.tasks`
- 未来的 `oc.artifacts`

为什么合适：

```text
The dashboard needs stable run lifecycle, normalized events, and durable result tracking.
It should not parse CLI output or transcripts to reconstruct experiment state.
```

### 3. CI 与代码审查自动化应用

一个与 GitHub/GitLab 相邻的**服务**，让 OpenClaw agent 审查变更、检查日志、运行已批准的检查并产生审查产物。

核心用户流程：

```text
pull request opened
  -> app creates or resumes review session
  -> starts review run
  -> streams progress into CI UI
  -> waits for final result
  -> uploads review summary/artifacts
```

SDK 接口：

- `oc.sessions`
- `oc.runs`
- `Run.wait()`
- `Run.events()`
- `oc.approvals`
- 未来的 `oc.artifacts`
- 未来的 `oc.tools.invoke`，仅在严格的策略门控下

为什么合适：

```text
CI needs deterministic wait/cancel behavior and clear approval boundaries.
It cannot depend on a human watching a terminal.
```

### 4. 智能设备与媒体伴侣

一个移动端或桌面端**伴侣应用**，通过节点将摄像头、屏幕、canvas、位置、通知和设备状态暴露给 OpenClaw。

核心用户流程：

```text
pair device as node
  -> Gateway sees declared capabilities
  -> user asks agent to inspect screen/camera/canvas
  -> node returns media
  -> app displays MEDIA attachment or artifact
```

SDK 接口：

- `oc.rawEvents()` 或归一化的 SDK node/media 事件
- 未来的 node 感知辅助函数
- 未来的 `oc.artifacts`
- `oc.approvals`，用于敏感操作
- 设备型号元数据，用于友好的实例名称

为什么合适：

```text
The app turns physical device capabilities into Gateway-mediated agent capabilities.
The node remains a peripheral; the Gateway remains the control plane.
```

### 5. 面向分布式 agent 基础设施的运维控制台

一个**管理应用**，供运行多个网关、节点主机、模型、agent 和执行环境的团队使用。

核心用户流程：

```text
connect to Gateway
  -> inspect agents/models/nodes/environments
  -> view active runs and approvals
  -> identify stuck tasks
  -> cancel or retry work
  -> audit artifacts and events
```

SDK 接口：

- `oc.agents`
- `oc.models`
- `oc.runs`
- `oc.approvals`
- 未来的 `oc.environments`
- 未来的 `oc.tasks`
- 未来的 `oc.artifacts`

为什么合适：

```text
Operations needs observability and control through typed APIs.
It should not SSH into machines and scrape logs as the primary interface.
```

这五者的共同模式：

```text
App owns UX.
Gateway owns agent runtime.
SDK owns the typed contract between them.
```

---


<details>
<summary>English original</summary>

**25. Design exercise**

Design a small OpenClaw dashboard using only the App SDK.

The dashboard must:

- connect to a Gateway
- list agents
- list models
- create a session
- start a run
- stream assistant and tool events
- show approval prompts
- wait for final state
- cancel a run
- show artifacts if the Gateway supports artifact APIs
- hide artifact UI if the Gateway does not support artifact APIs

Answer:

1. Which SDK namespaces do you need today?
2. Which future namespaces should be feature-detected?
3. Which operations need idempotency keys?
4. What events update the UI state reducer?
5. What happens if `Run.wait()` returns accepted because the wait deadline expired?
6. How do you test the app without a real Gateway?
7. What must stay in the SDK adapter instead of the UI component?

---

**26. Five apps that could use the App SDK**

The App SDK is useful when an application wants OpenClaw's agent runtime **without embedding OpenClaw itself**.

Here are five realistic app patterns.

**1. Personal desktop control center**

A macOS, Windows, or Linux **desktop app** that lets a user manage agents, sessions, models, approvals, nodes, screenshots, and long-running work.

Core user flow:

```text
open app
  -> connect to Gateway
  -> list agents and sessions
  -> start a run
  -> stream assistant and tool events
  -> approve or reject risky actions
  -> show artifacts and node media
```

SDK surfaces:

- `oc.agents`
- `oc.sessions`
- `oc.runs`
- `oc.models`
- `oc.approvals`
- future `oc.artifacts`
- feature-detected node/media events

Why it fits:

```text
The app is a remote operator UI. It should not run the agent loop locally.
```

**2. AI lab dashboard for experiments**

A **web dashboard** for comparing prompts, models, agents, and tool behavior across repeated runs.

Core user flow:

```text
select agent + model
  -> run experiment batch
  -> stream outputs
  -> collect artifacts/logs
  -> compare final results
  -> export report
```

SDK surfaces:

- `oc.agents`
- `oc.models`
- `oc.runs`
- `Run.events()`
- `Run.wait()`
- future `oc.tasks`
- future `oc.artifacts`

Why it fits:

```text
The dashboard needs stable run lifecycle, normalized events, and durable result tracking.
It should not parse CLI output or transcripts to reconstruct experiment state.
```

**3. CI and code-review automation app**

A GitHub/GitLab-adjacent **service** that asks OpenClaw agents to review changes, inspect logs, run approved checks, and produce review artifacts.

Core user flow:

```text
pull request opened
  -> app creates or resumes review session
  -> starts review run
  -> streams progress into CI UI
  -> waits for final result
  -> uploads review summary/artifacts
```

SDK surfaces:

- `oc.sessions`
- `oc.runs`
- `Run.wait()`
- `Run.events()`
- `oc.approvals`
- future `oc.artifacts`
- future `oc.tools.invoke` only if tightly policy-gated

Why it fits:

```text
CI needs deterministic wait/cancel behavior and clear approval boundaries.
It cannot depend on a human watching a terminal.
```

**4. Smart device and media companion**

A mobile or desktop **companion app** that exposes camera, screen, canvas, location, notifications, and device status to OpenClaw through nodes.

Core user flow:

```text
pair device as node
  -> Gateway sees declared capabilities
  -> user asks agent to inspect screen/camera/canvas
  -> node returns media
  -> app displays MEDIA attachment or artifact
```

SDK surfaces:

- `oc.rawEvents()` or normalized SDK node/media events
- future node-aware helpers
- future `oc.artifacts`
- `oc.approvals` for sensitive actions
- device model metadata for friendly instance names

Why it fits:

```text
The app turns physical device capabilities into Gateway-mediated agent capabilities.
The node remains a peripheral; the Gateway remains the control plane.
```

**5. Operations console for distributed agent infrastructure**

An **admin app** for teams running multiple Gateways, node hosts, models, agents, and execution environments.

Core user flow:

```text
connect to Gateway
  -> inspect agents/models/nodes/environments
  -> view active runs and approvals
  -> identify stuck tasks
  -> cancel or retry work
  -> audit artifacts and events
```

SDK surfaces:

- `oc.agents`
- `oc.models`
- `oc.runs`
- `oc.approvals`
- future `oc.environments`
- future `oc.tasks`
- future `oc.artifacts`

Why it fits:

```text
Operations needs observability and control through typed APIs.
It should not SSH into machines and scrape logs as the primary interface.
```

The common pattern across all five:

```text
App owns UX.
Gateway owns agent runtime.
SDK owns the typed contract between them.
```

---

</details>

## 关键要点

- `@openclaw/sdk` 是面向应用的契约，面向 OpenClaw 之外的代码。
- Plugin SDK 是另一套进程内扩展契约。
- 真正的外部应用应使用带类型的 Gateway RPC，而不是 CLI 输出或 runtime 内部实现。
- happy path 是 connect、discover、session、run、stream、wait、cancel 与 approvals。
- 当前 SDK 的 helper 覆盖 agents、runs、sessions、models、tools catalog、approvals、raw events 与 event normalization。
- 未来的面向应用接口面应当收窄：`artifacts.*`、`environments.*`、`tools.invoke` 与 `tasks.*`。
- Nodes 以伴随设备能力扩展 Gateway，例如 canvas、camera、screen、location、notifications 与受控的系统执行。
- Nodes 是外设，不是网关；消息、模型执行、会话、策略与布线仍归 Gateway 所有。
- 尚不支持的未来接口面应抛出明确错误，而不是假装可用。
- 用 OpenMeow 风格的客户端做 dogfooding，是 SDK 契约稳定到足以支撑外部应用的方式。
- 合适的 App SDK 用例包括桌面控制中心、实验看板、CI 自动化、智能设备伴侣与运维控制台。
- 友好的设备名称属于展示用元数据；原始设备标识符与能力字段仍是 runtime 决策的权威依据。

---

## 参考资料

- OpenClaw App SDK：[https://openclaw.knidal.com/openclaw-app-sdk](https://openclaw.knidal.com/openclaw-app-sdk)
- OpenClaw Gateway 协议：[https://openclaw.knidal.com/gateway-protocol](https://openclaw.knidal.com/gateway-protocol)
- OpenClaw RPC 适配器：[https://openclaw.knidal.com/rpc-adapters](https://openclaw.knidal.com/rpc-adapters)
- OpenClaw Tools Invoke API：[https://openclaw.knidal.com/tools-invoke-api](https://openclaw.knidal.com/tools-invoke-api)
- OpenClaw Nodes：[https://openclaw.knidal.com/nodes](https://openclaw.knidal.com/nodes)
- OpenClaw Node 故障排查：[https://openclaw.knidal.com/nodes/troubleshooting](https://openclaw.knidal.com/nodes/troubleshooting)
- Apple 设备标识符数据源：[kyle-seongwoo-jun/apple-device-identifiers](https://github.com/kyle-seongwoo-jun/apple-device-identifiers)
- 案例研究源码仓库：[OpenClaw](https://github.com/openclaw/openclaw)

---

*下一讲：[Lecture 39](Lecture-39.md)*


<details>
<summary>English original</summary>

**Key takeaways**

- `@openclaw/sdk` is the app-facing contract for code outside OpenClaw.
- The Plugin SDK is a separate in-process extension contract.
- Real external apps should use typed Gateway RPCs, not CLI output or runtime internals.
- The happy path is connect, discover, session, run, stream, wait, cancel, and approvals.
- Current SDK helpers cover agents, runs, sessions, models, tools catalog, approvals, raw events, and event normalization.
- Future app-facing surfaces should be narrow: `artifacts.*`, `environments.*`, `tools.invoke`, and `tasks.*`.
- Nodes extend the Gateway with companion-device capabilities such as canvas, camera, screen, location, notifications, and controlled system execution.
- Nodes are peripherals, not gateways; the Gateway still owns messages, model execution, sessions, policy, and routing.
- Unsupported future surfaces should throw explicit errors rather than pretending to work.
- Dogfooding with OpenMeow-style clients is how the SDK contract becomes stable enough for external apps.
- Good App SDK use cases include desktop control centers, experiment dashboards, CI automation, smart-device companions, and operations consoles.
- Friendly device names are presentation metadata; raw device identifiers and capability fields remain authoritative for runtime decisions.

---

**References**

- OpenClaw App SDK: [https://openclaw.knidal.com/openclaw-app-sdk](https://openclaw.knidal.com/openclaw-app-sdk)
- OpenClaw Gateway protocol: [https://openclaw.knidal.com/gateway-protocol](https://openclaw.knidal.com/gateway-protocol)
- OpenClaw RPC adapters: [https://openclaw.knidal.com/rpc-adapters](https://openclaw.knidal.com/rpc-adapters)
- OpenClaw Tools Invoke API: [https://openclaw.knidal.com/tools-invoke-api](https://openclaw.knidal.com/tools-invoke-api)
- OpenClaw Nodes: [https://openclaw.knidal.com/nodes](https://openclaw.knidal.com/nodes)
- OpenClaw Node troubleshooting: [https://openclaw.knidal.com/nodes/troubleshooting](https://openclaw.knidal.com/nodes/troubleshooting)
- Apple device identifiers data source: [kyle-seongwoo-jun/apple-device-identifiers](https://github.com/kyle-seongwoo-jun/apple-device-identifiers)
- Case-study source repo: [OpenClaw](https://github.com/openclaw/openclaw)

---

*Next: [Lecture 39](Lecture-39.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-38.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-38.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
