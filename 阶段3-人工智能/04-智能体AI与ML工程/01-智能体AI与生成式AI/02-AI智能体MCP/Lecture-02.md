---
title: Lecture 02 - 架构与生命周期：Host、Client、Server
description: Lecture 02 - 架构与生命周期：Host、Client、Server
---

# Lecture 02 - 架构与生命周期：Host、Client、Server

**合集：** [MCP for AI Agents](README.md) | **上一篇：** [← Lecture 01](Lecture-01.md) | **下一篇：** [Lecture 03](Lecture-03.md)

---

Lecture 01 阐释了 Model Context Protocol *为什么* 存在：Anthropic 于 2024 年 11 月开源 MCP，目的就是用一套 wire protocol 取代为每个集成单独定制的胶水代码，让任何 LLM 应用都能与任意能力提供方对话。本讲讨论该协议的 *形态* —— 它定义的三种角色、它从 JSON-RPC 2.0 借来的消息帧，以及每个 MCP 会话从 `initialize` 到关闭所经历的连接生命周期。

首先要立住的心智模型：MCP 是一种 **客户端-服务器协议，且严格遵循 1:1 连接规则**。单个应用 —— 即 Host —— 可以同时与多个 server 通信，但做法是为每个 server 运行一个 Client，而每个 Client 恰好持有一条到恰好一个 Server 的连接。单个 Client 内部没有扇出，没有共享 socket，也没有在两个 server 之间复用同一个 Client。一旦你真正内化「Host 是 *一群 Client*，而不是单个连接管理器」这一点，架构的其余部分便顺理成章地展开。

本讲的一切都锚定最新稳定规范 **2025-11-25**。凡是行为带版本的地方 —— 能力协商、传输、协议版本握手 —— 我都会指明规范修订号，因为 MCP 的 wire 契约在各修订版之间发生了实质变化，“这取决于版本”往往才是资深工程师的正确回答。

---

## 学习目标

本讲结束时，你应当能够：

- 区分 MCP 的三种角色 —— Host、Client、Server —— 并解释为什么 Client↔Server 关系严格保持 1:1。
- 读写 JSON-RPC 2.0 的三种消息类型（request、response、notification），并说出各自携带哪些字段。
- 解释能力协商：Host 与 Server 如何在 `initialize` 中声明各自特性，以及为什么未被声明的能力永远不会被调用。
- 走通完整的连接生命周期 —— `initialize` → `notifications/initialized` → operation → shutdown —— 并描述协议版本协商。
- 阐明为什么 MCP 会话在逻辑上是有状态的、为什么这会让水平扩展复杂化，以及 2026-07-28 RC 的无状态核心如何改变这一局面。

---


<details>
<summary>English original</summary>

**Lecture 02 - Architecture & Lifecycle: Host, Client, Server**

**Collection:** [MCP for AI Agents](README.md) | **Previous:** [← Lecture 01](Lecture-01.md) | **Next:** [Lecture 03](Lecture-03.md)

---

Lecture 01 framed *why* the Model Context Protocol exists: Anthropic open-sourced MCP in November 2024 to replace bespoke, per-integration glue code with one wire protocol that any LLM application can speak to any capability provider. This lecture is about the *shape* of that protocol — the three roles it defines, the message framing it borrows from JSON-RPC 2.0, and the connection lifecycle that every MCP session walks through from `initialize` to shutdown.

The mental model worth fixing before anything else: MCP is a **client-server protocol with a strict 1:1 connection rule**. A single application — the Host — can talk to many servers at once, but it does so by running one Client per server, and each Client holds exactly one connection to exactly one Server. There is no fan-out inside a single Client, no shared socket, no multiplexing of two servers over one Client. Once you internalize that the Host is a *fleet of Clients* rather than a single connection manager, the rest of the architecture falls out cleanly.

Everything in this lecture pins to the latest stable spec, **2025-11-25**. Where behavior is versioned — capability negotiation, transports, the protocol-version handshake — I name the spec revision, because MCP's wire contract has changed materially across revisions and "it depends on the version" is the correct senior-engineer answer more often than not.

---

**Learning objectives**

By the end of this lecture you should be able to:

- Distinguish the three MCP roles — Host, Client, Server — and explain why the Client↔Server relationship is strictly 1:1.
- Read and write the three JSON-RPC 2.0 message types (request, response, notification) and identify which fields each carries.
- Explain capability negotiation: how Host and Server advertise features in `initialize` and why an undeclared capability is never invoked.
- Walk the full connection lifecycle — `initialize` → `notifications/initialized` → operation → shutdown — and describe protocol-version negotiation.
- Articulate why an MCP session is logically stateful, why that complicates horizontal scaling, and how the 2026-07-28 RC stateless core changes the picture.

---

</details>

## 1. 三种角色：Host、Client、Server

MCP 定义了恰好三种角色。它们不可互换，真实部署中总是三者齐备。

**Host** 是用户实际交互的大语言模型应用——Claude Desktop、Claude Code、像 Cursor 这样的 IDE，或任何你自己构建的 agent runtime。Host 拥有所有*非*能力的东西：它持有语言模型（或调用模型的 API key），它掌控面向用户的界面，而且——对安全性至关重要——它掌控**批准 UX**。当 server 提供一个删除文件的工具时，是 Host 决定在该工具运行前是否弹出确认提示。Host 是信任边界；server 是 wire 另一侧默认不可信的代码。

**Client** 是位于 Host *内部*的协议连接器。它不是你要部署的独立进程；而是 Host 实例化来与一个 server 讲 MCP 的组件。Client 处理 JSON-RPC 管道——封装请求、将响应与请求 ID 匹配、分发通知——并且它持有**恰好一个到恰好一个 Server 的连接**。如果 Host 想与三个 server（文件系统 server、GitHub server、数据库 server）通信，它就运行三个 Client。

**Server** 是能力提供者。它通过连接暴露 Tools、Resources 和 Prompts（server→client 原语）的某种组合。Server 可以是 Host 启动并通过 stdio 与之通信的**本地子进程**，也可以是 Host 通过 HTTP 访问的**远程服务**。两种情况下协议都相同——只有传输方式不同。

```text
                          ┌──────────────────────────────────────────────┐
                          │                   HOST                        │
                          │   (LLM app: Claude Desktop / Code / Cursor)   │
                          │   owns: the model, user, API keys, approvals  │
                          │                                                │
                          │   ┌──────────┐   ┌──────────┐   ┌──────────┐  │
                          │   │ Client A │   │ Client B │   │ Client C │  │
                          │   └────┬─────┘   └────┬─────┘   └────┬─────┘  │
                          └────────┼──────────────┼──────────────┼────────┘
                                   │ 1:1          │ 1:1          │ 1:1
                                   ▼              ▼              ▼
                            ┌──────────┐   ┌──────────┐   ┌──────────┐
                            │ Server A │   │ Server B │   │ Server C │
                            │ (stdio   │   │ (remote  │   │ (remote  │
                            │ subproc) │   │  HTTP)   │   │  HTTP)   │
                            └──────────┘   └──────────┘   └──────────┘
```

需要牢记的最重要不变量：**一个 Client : 一个 Server**。一个 Client 从不桥接两个 server，一个 Server 也从不被同一个 Host 会话内的两个 Client 共享。Host 是唯一能看到全貌的组件；每个 Client 只看到它自己的 server。

---


<details>
<summary>English original</summary>

**1. The three roles: Host, Client, Server**

MCP defines exactly three roles. They are not interchangeable, and a real deployment always has all three.

The **Host** is the LLM application the user actually interacts with — Claude Desktop, Claude Code, an IDE like Cursor, or any agent runtime you build yourself. The Host owns everything that is *not* a capability: it holds the language model (or the API keys to call one), it owns the user-facing surface, and — critically for security — it owns the **approval UX**. When a server offers a tool that deletes files, it is the Host that decides whether to surface a confirmation prompt before that tool runs. The Host is the trust boundary; servers are untrusted-by-default code on the other side of a wire.

The **Client** is the protocol connector that lives *inside* the Host. It is not a separate process you deploy; it is the component the Host instantiates to speak MCP to one server. The Client handles the JSON-RPC plumbing — framing requests, matching responses to request IDs, dispatching notifications — and it holds **exactly one connection to exactly one Server**. If a Host wants to talk to three servers (a filesystem server, a GitHub server, a database server), it runs three Clients.

The **Server** is the capability provider. It exposes some combination of Tools, Resources, and Prompts (the server→client primitives) over the connection. A Server can be a **local subprocess** the Host launches and talks to over stdio, or a **remote service** the Host reaches over HTTP. The protocol is identical either way — only the transport differs.

```text
                          ┌──────────────────────────────────────────────┐
                          │                   HOST                        │
                          │   (LLM app: Claude Desktop / Code / Cursor)   │
                          │   owns: the model, user, API keys, approvals  │
                          │                                                │
                          │   ┌──────────┐   ┌──────────┐   ┌──────────┐  │
                          │   │ Client A │   │ Client B │   │ Client C │  │
                          │   └────┬─────┘   └────┬─────┘   └────┬─────┘  │
                          └────────┼──────────────┼──────────────┼────────┘
                                   │ 1:1          │ 1:1          │ 1:1
                                   ▼              ▼              ▼
                            ┌──────────┐   ┌──────────┐   ┌──────────┐
                            │ Server A │   │ Server B │   │ Server C │
                            │ (stdio   │   │ (remote  │   │ (remote  │
                            │ subproc) │   │  HTTP)   │   │  HTTP)   │
                            └──────────┘   └──────────┘   └──────────┘
```

The single most important invariant to carry forward: **one Client : one Server**. A Client never bridges two servers, and a Server is never shared by two Clients within the same Host session. The Host is the only component that sees the whole picture; each Client sees exactly its own server.

---

</details>

## 2. JSON-RPC 2.0 基础

MCP 没有自创 wire format。它构建在 **JSON-RPC 2.0** 之上，由此获得三种消息类型和一个小巧且广为人知的消息封装。

**request** 期望得到响应。它携带 `jsonrpc`（始终为 `"2.0"`）、一个 `id`（由发送方选择的字符串或数字，用于关联最终返回的响应）、一个 `method`（操作名，例如 `tools/list`），以及可选的 `params`。

**response** 恰好回答一个 request。它回显相同的 `id`，并携带 `result`（成功时）或 `error`（失败时）——两者绝不会同时出现。

**notification** 是单向消息，**不期望任何响应**。它看起来像 request，但**没有 `id`**——正是缺失的 `id` 把它标记为 fire-and-forget。接收方处理它，不回送任何内容。

下面是一个真实的 `tools/list` request，Client 用它向 Server 询问其暴露了哪些 tools：

```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "tools/list",
  "params": {}
}
```

以及 Server 的响应，通过与之匹配的 `id` 相关联：

```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "result": {
    "tools": [
      {
        "name": "get_weather",
        "description": "Get the current weather for a city",
        "inputSchema": {
          "type": "object",
          "properties": {
            "city": { "type": "string" }
          },
          "required": ["city"]
        }
      }
    ]
  }
}
```

把这个往返与 notification 做个对照。当 Client 完成初始化（第 4 节）后，它发送：

```json
{
  "jsonrpc": "2.0",
  "method": "notifications/initialized"
}
```

没有 `id`，就没有响应。Server 只是转换自身状态，然后继续。`id` 的有无就是全部区别：带 `id` 的消息是 request，*会*被应答；不带的消息是 notification，*不会*被应答。Server 也会在反方向使用 notification——例如，`notifications/tools/list_changed` 消息告知 Client tool 集合已变更、应当重新拉取，同样不期望响应。

---

## 3. 能力协商

MCP 刻意设计为一种*协商式*协议，而非默认假设式。除非另一方在 `initialize` 期间声明支持某项特性，否则任一方都不得使用该特性。正因如此，一个最小化 server（仅 tools）和一个功能丰富的 server（tools、resources、prompts、logging）才能共存于同一个 Host 之后，而无需 Host 去猜测。

协商是对称的。**Server 声明它提供什么**；**Client 声明它能回馈什么**。连接随后只使用两者的交集。

Server 声明的能力对应三个 server→client 原语，外加若干运行特性：

| Capability (server) | What it enables |
| --- | --- |
| `tools` | Server 暴露可调用的 tools（`tools/list`、`tools/call`） |
| `resources` | Server 暴露可读的 resources（`resources/list`、`resources/read`） |
| `prompts` | Server 暴露 prompt 模板（`prompts/list`、`prompts/get`） |
| `logging` | Server 可以向 client 发出结构化 log notification |

Client 声明的能力对应三个反向（client→server）原语：

| Capability (client) | What it enables |
| --- | --- |
| `sampling` | Server 可以要求 client 的 LLM 生成补全 |
| `roots` | Server 可以查询 client 已授予的文件系统 roots |
| `elicitation` | Server 可以通过 client 向用户请求额外输入 |

由此得出的规则很严格，值得直说：**未声明 `tools` 的 server 永远不会被发送 `tools/list`**，而未声明 `sampling` 的 client 永远不会收到 sampling 请求。能力不是提示或偏好——它们是契约。为未声明的能力发起请求属于协议违规，正确的对端会拒绝它。这就是六个原语干净地分成两个方向的原因：Tools/Resources/Prompts 沿 server→client 流动，而 Sampling/Roots/Elicitation 沿 client→server 流动，每一端只宣告自己这一侧。

---



---


<details>
<summary>English original</summary>

**2. The JSON-RPC 2.0 foundation**

MCP does not invent a wire format. It is built on **JSON-RPC 2.0**, which gives it three message types and a small, well-understood envelope.

A **request** expects a response. It carries `jsonrpc` (always `"2.0"`), an `id` (a string or number the sender chooses to correlate the eventual response), a `method` (the operation name, e.g. `tools/list`), and optional `params`.

A **response** answers exactly one request. It echoes the same `id` and carries either a `result` (on success) or an `error` (on failure) — never both.

A **notification** is a one-way message that expects **no response**. It looks like a request but has **no `id`** — that missing `id` is precisely what marks it as fire-and-forget. The receiver processes it and sends nothing back.

Here is a real `tools/list` request the Client sends to ask the Server what tools it exposes:

```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "method": "tools/list",
  "params": {}
}
```

And the Server's response, correlated by the matching `id`:

```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "result": {
    "tools": [
      {
        "name": "get_weather",
        "description": "Get the current weather for a city",
        "inputSchema": {
          "type": "object",
          "properties": {
            "city": { "type": "string" }
          },
          "required": ["city"]
        }
      }
    ]
  }
}
```

Contrast that round-trip with a notification. When the Client finishes initialization (Section 4), it sends:

```json
{
  "jsonrpc": "2.0",
  "method": "notifications/initialized"
}
```

No `id`, no response. The Server simply transitions its own state and moves on. The presence or absence of `id` is the whole distinction: a message with an `id` is a request and *will* be answered; a message without one is a notification and *will not* be. Servers also use notifications in the other direction — for example, a `notifications/tools/list_changed` message tells the Client the tool set changed and it should re-fetch, again with no response expected.

---

**3. Capability negotiation**

MCP is deliberately a *negotiated* protocol, not an assumed one. Neither side may use a feature unless the other side has declared support for it during `initialize`. This is what lets a minimal server (tools only) and a rich server (tools, resources, prompts, logging) coexist behind the same Host without the Host guessing.

The negotiation is symmetric. The **Server declares what it offers**; the **Client declares what it can provide back**. The connection then uses only the intersection.

Server-declared capabilities map to the three server→client primitives, plus operational features:

| Capability (server) | What it enables |
| --- | --- |
| `tools` | Server exposes callable tools (`tools/list`, `tools/call`) |
| `resources` | Server exposes readable resources (`resources/list`, `resources/read`) |
| `prompts` | Server exposes prompt templates (`prompts/list`, `prompts/get`) |
| `logging` | Server can emit structured log notifications to the client |

Client-declared capabilities map to the three reverse (client→server) primitives:

| Capability (client) | What it enables |
| --- | --- |
| `sampling` | Server may ask the client's LLM to generate a completion |
| `roots` | Server may query the filesystem roots the client has granted |
| `elicitation` | Server may request additional input from the user via the client |

The rule that follows is strict and worth stating plainly: **a server that did not declare `tools` will never be sent `tools/list`**, and a client that did not declare `sampling` will never receive a sampling request. Capabilities are not hints or preferences — they are the contract. Code that issues a request for an undeclared capability is a protocol violation, and a correct peer will reject it. This is why the six primitives split cleanly into two directions: Tools/Resources/Prompts flow server→client, while Sampling/Roots/Elicitation flow client→server, and each end advertises only its own side.

---

</details>

## 4. 生命周期

每个 MCP 连接都经过一个确定的生命周期。没有捷径 —— 握手完成之前无法调用工具。

各阶段是：**initialize**（请求/响应）、**initialized**（通知）、**运行**（工作阶段）、**关闭**。

**阶段 1 — `initialize` 请求。** 客户端通过发送一个 `initialize` 请求打开连接，该请求携带三样东西：它想使用的 `protocolVersion`（一个日期字符串）、它的 `clientInfo`，以及它的 `capabilities`。

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "initialize",
  "params": {
    "protocolVersion": "2025-11-25",
    "clientInfo": { "name": "ExampleHost", "version": "1.4.0" },
    "capabilities": {
      "sampling": {},
      "roots": { "listChanged": true },
      "elicitation": {}
    }
  }
}
```

**阶段 2 — 服务器响应。** 服务器用*自己的* `protocolVersion`、它的 `serverInfo` 和它的 `capabilities` 回复那一个请求。正是这单次交换，让双方了解对方支持什么。

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "protocolVersion": "2025-11-25",
    "serverInfo": { "name": "WeatherServer", "version": "2.0.1" },
    "capabilities": {
      "tools": { "listChanged": true },
      "resources": {},
      "logging": {}
    }
  }
}
```

**阶段 3 — `notifications/initialized`。** 一旦客户端拿到服务器的 capabilities，它就发送 `notifications/initialized` 通知（见第 2 节）。这是 fire-and-forget —— 它表示“握手完成，我已准备好开始运行”。只有在此之后，任意一方才可以发出操作性请求。

**阶段 4 — 运行。** 这是工作阶段：`tools/list`、`tools/call`、`resources/read`、`prompts/get`，以及协商出的 capabilities 所允许的任何反向 `sampling`/`elicitation` 请求。在会话的整个生命周期内，请求和通知双向流动。

**阶段 5 — 关闭。** 连接被关闭。对于 stdio 服务器，宿主通常关闭输入流，子进程随之退出；对于 HTTP 服务器，会话被拆除（其 `Mcp-Session-Id` 也被回收）。不存在专用的 `shutdown` JSON-RPC 方法 —— 关闭是传输层动作。

**协议版本协商**完全发生在阶段 1/2。版本是**日期字符串**（`2025-11-25`、`2025-06-18` 等）。客户端在 `initialize` 中提出一个版本；服务器回以它实际将使用的版本。如果服务器支持所请求的版本，它就回显该版本。如果不支持，它就回以一个它*确实*支持的版本，由客户端决定自己能否使用该版本。不愿降级的客户端应把这种不匹配视为连接失败，而不是在一个不受支持的约定上继续。在任何操作性流量*之前*协商版本，正是避免两个对端各说各话的关键。

---


<details>
<summary>English original</summary>

**4. The lifecycle**

Every MCP connection moves through a defined lifecycle. There are no shortcuts — you cannot call a tool before the handshake completes.

The phases are: **initialize** (request/response), **initialized** (notification), **operation** (the working phase), and **shutdown**.

**Phase 1 — `initialize` request.** The Client opens the connection by sending an `initialize` request carrying three things: the `protocolVersion` it wants to speak (a date string), its `clientInfo`, and its `capabilities`.

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "initialize",
  "params": {
    "protocolVersion": "2025-11-25",
    "clientInfo": { "name": "ExampleHost", "version": "1.4.0" },
    "capabilities": {
      "sampling": {},
      "roots": { "listChanged": true },
      "elicitation": {}
    }
  }
}
```

**Phase 2 — the Server responds.** The Server replies to that one request with its *own* `protocolVersion`, its `serverInfo`, and its `capabilities`. This single exchange is where both sides learn what the other supports.

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "protocolVersion": "2025-11-25",
    "serverInfo": { "name": "WeatherServer", "version": "2.0.1" },
    "capabilities": {
      "tools": { "listChanged": true },
      "resources": {},
      "logging": {}
    }
  }
}
```

**Phase 3 — `notifications/initialized`.** Once the Client has the Server's capabilities, it sends the `notifications/initialized` notification (shown in Section 2). This is fire-and-forget — it signals "handshake complete, I am ready to operate." Only after this point may either side issue operational requests.

**Phase 4 — operation.** This is the working phase: `tools/list`, `tools/call`, `resources/read`, `prompts/get`, and any reverse-direction `sampling`/`elicitation` requests the negotiated capabilities permit. Requests and notifications flow in both directions for the life of the session.

**Phase 5 — shutdown.** The connection is closed. For a stdio server the Host typically closes the input stream and the subprocess exits; for an HTTP server the session is torn down (and its `Mcp-Session-Id` retired). There is no dedicated `shutdown` JSON-RPC method — shutdown is a transport-level action.

**Protocol-version negotiation** happens entirely in Phase 1/2. Versions are **date strings** (`2025-11-25`, `2025-06-18`, and so on). The Client proposes a version in `initialize`; the Server responds with the version it will actually use. If the Server supports the requested version, it echoes it. If it does not, it responds with a version it *does* support, and the Client decides whether it can speak that. A Client unwilling to downgrade should treat the mismatch as a failed connection rather than proceeding on an unsupported contract. Negotiating the version *before* any operational traffic is what keeps two peers from talking past each other.

---

</details>

## 5. 有状态 vs 无状态

MCP 会话在**逻辑上是有状态的**。`initialize` 握手建立协商后的能力，这些能力在整个会话期间有效；资源订阅与工具列表订阅会持续存在；在 Streamable HTTP 上，会话绑定到一个 `Mcp-Session-Id`，Client 在后续每个请求中都会返回它。Server 需要记住“本会话协商了这些能力，并持有这些订阅”。

这种有状态性是长连接的自然模型，但它会让**横向扩展**变复杂。如果会话状态存放在某个 server 实例的内存里，那么该会话的每个请求都必须落到同一个实例上——即 sticky routing——否则就必须把状态外置到所有实例都会查询的共享存储中。两者都是实实在在的运维成本：sticky routing 与负载均衡器相冲突，并让故障转移变复杂；共享存储则增加延迟和一项新依赖。对于服务大量 agent 的高扇出部署，按会话保持状态的 server 就是扩展瓶颈。

这正是 **2026-07-28 release candidate** 用**无状态核心**所填补的缺口。该 RC 定义了一种核心协议模式，不携带任何必需的按会话划分的 server 状态，从而让 server 可以依托普通 HTTP 基础设施扩展——任何实例都能处理任何请求，因为没有需要粘滞的会话内存。（同一个 RC 还增加了 MCP Apps 与 Tasks 扩展，叠加在该无状态核心之上。）注意这里的传输层历史：MCP 的两种传输是 **stdio** 和 **Streamable HTTP**（带 `Mcp-Session-Id` 会话）；更早的 **HTTP+SSE** 传输自 **2025-03-26 起已废弃**。无状态核心是这一演进脉络中的下一步——把协议推向完全不需要会话亲和性的部署。

无论运行哪种模式，架构上的不变之处都是一样的：**Host 在模型与各个 server 之间中转一切。** 模型从不直接与 server 通信。Host 接收模型的意图，经相应的 Client 将其路由到正确的 Server，施加自己的审批 UX，并把结果回传给模型。这种中转角色——Host 作为同时掌握模型、用户、密钥和信任边界的单一点——正是配套讲义 [`../Lectures/Lecture-02.md`](../01-讲座/Lecture-02.md) 中展开的 agent-harness（agent 运行时框架）模式：MCP 是 harness 站在 LLM 与外部世界之间的一种具体实现。

---

## 时效说明

本讲义内容截至 **2026 年 6 月**，锚定最新稳定版 MCP 规范 **2025-11-25**。**2026-07-28 release candidate** 引入了**无状态核心**（允许 server 在普通 HTTP 上扩展，无需按会话亲和），以及 MCP Apps 与 Tasks 扩展；在 RC 获批之前，这些都应视为尚未正式发布。撰写本文时的传输层状态：**stdio** 和 **Streamable HTTP** 为当前版本；**HTTP+SSE** 自 **2025-03-26** 起已废弃。


<details>
<summary>English original</summary>

**5. Stateful vs stateless**

An MCP session is **logically stateful**. The `initialize` handshake establishes negotiated capabilities that hold for the whole session; resource and tool-list subscriptions persist; and over Streamable HTTP the session is bound to an `Mcp-Session-Id` that the Client returns on every subsequent request. The Server is expected to remember "this session negotiated these capabilities and holds these subscriptions."

That statefulness is the natural model for a long-lived connection, but it complicates **horizontal scaling**. If session state lives in the memory of one server instance, then every request for that session must land on that same instance — sticky routing — or the state must be externalized to a shared store that all instances consult. Both are real operational costs: sticky routing fights load balancers and complicates failover; a shared store adds latency and a new dependency. For a high-fan-out deployment serving many agents, a per-session-stateful server is a scaling bottleneck.

This is the gap the **2026-07-28 release candidate** addresses with a **STATELESS core**. The RC defines a core protocol mode that carries no required per-session server state, which lets a server scale on ordinary HTTP infrastructure — any instance can handle any request because there is no session memory to be sticky to. (The same RC also adds the MCP Apps and Tasks extensions, layered on top of that stateless core.) Note the transport history here: MCP's two transports are **stdio** and **Streamable HTTP** (with `Mcp-Session-Id` sessions); the older **HTTP+SSE** transport has been **deprecated since 2025-03-26**. The stateless core is the next step in that arc — moving the protocol toward deployments that don't need session affinity at all.

Whichever mode you run, the architectural constant is the same: **the Host mediates everything between the model and the servers.** The model never speaks to a server directly. The Host receives the model's intent, routes it through the appropriate Client to the right Server, applies its approval UX, and feeds results back to the model. That mediation role — the Host as the single point that owns the model, the user, the keys, and the trust boundary — is exactly the agent-harness pattern developed in the companion lecture, [`../Lectures/Lecture-02.md`](../01-讲座/Lecture-02.md): MCP is one concrete realization of a harness standing between an LLM and the outside world.

---

**Current as of**

This lecture is current as of **June 2026**, pinned to the latest stable MCP specification, **2025-11-25**. The **2026-07-28 release candidate** introduces a **stateless core** (allowing servers to scale on ordinary HTTP without per-session affinity), along with the MCP Apps and Tasks extensions; treat those as forthcoming until the RC is ratified. Transport status as of this writing: **stdio** and **Streamable HTTP** are current; **HTTP+SSE** has been deprecated since **2025-03-26**.

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/MCP for AI Agents/Lecture-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/MCP%20for%20AI%20Agents/Lecture-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
