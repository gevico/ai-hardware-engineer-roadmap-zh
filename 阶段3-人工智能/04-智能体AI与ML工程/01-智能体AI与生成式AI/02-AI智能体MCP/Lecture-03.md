---
title: Lecture 03 - 核心原语：Tools、Resources、Prompts
description: Lecture 03 - 核心原语：Tools、Resources、Prompts
---

# Lecture 03 - 核心原语：Tools、Resources、Prompts

**合集：** [MCP for AI Agents](README.md) | **上一讲：** [← Lecture 02](Lecture-02.md) | **下一讲：** [Lecture 04](Lecture-04.md)

---

在 [Lecture 02](Lecture-02.md) 中，你看到一条连接建立起来：host 与 server 交换 `initialize`，协商 capabilities，并进入 operate 阶段。现在打开 operate 阶段，看看 *server 实际暴露了什么*。三个 server→client 原语 —— **Tools**、**Resources** 和 **Prompts** —— 就是连接这一侧 server 向 host 提供的全部内容。（反方向，即 server *向 host 索取* 东西，见 [Lecture 04](Lecture-04.md)。）MCP server 为 agent 做的每一件事，都恰好落在这三个桶中的一个。

从单纯的 function calling 过来，容易把这三者都看作「名字不同的 tools」。要抵制这种想法。协议基于 **由谁决定使用某个东西** —— model、application 还是 human —— 在它们之间划了一条刻意的界线。这唯一的区分，即 *控制三分法*，是本讲最重要的思想，因为正是它让 MCP 可以安全地接入 host UI。model 能自行触发的 tool，与 application 选择附加的文档或用户输入的 slash-command，在安全性和 UX 上是根本不同的对象。把三分法搞对，其余 —— schema、URI、template —— 都是细节。

本讲所有内容都是基于 [Lecture 02](Lecture-02.md) 的 transport 之上的 JSON-RPC 2.0，并锁定到 **2025-11-25** 稳定规范。我们会读实际的 wire 消息，而不是 SDK 的糖衣；[Lecture 05](Lecture-05.md) 会在其之上叠加 FastMCP。

---

## 学习目标

学完本讲后，你应当能够：

- 说出 **控制三分法** —— Tools 由 model 控制、Resources 由 application 控制、Prompts 由 user 控制 —— 并解释这种分离为何对 UX 与安全很重要。
- 用 JSON-Schema `inputSchema` 定义 tool，读懂 `tools/call` 请求与结果，并解释 content block、`outputSchema`/structured content 以及 `isError` 标志。
- 用 **resource URI** 和 **resource template**（RFC 6570）寻址数据，并通过 `resources/subscribe` 和 `notifications/resources/updated` 订阅资源变更。
- 用带类型的参数定义 **prompt**，并读懂 `prompts/get` 返回的消息列表。
- 把每个原语对应到 host *呈现* 它的方式 —— tools 作为可调用的函数、resources 作为可附加的上下文、prompts 作为用户命令。

---


<details>
<summary>English original</summary>

**Lecture 03 - Core Primitives: Tools, Resources, Prompts**

**Collection:** [MCP for AI Agents](README.md) | **Previous:** [← Lecture 02](Lecture-02.md) | **Next:** [Lecture 04](Lecture-04.md)

---

In [Lecture 02](Lecture-02.md) you watched a connection come up: host and server exchange `initialize`, negotiate capabilities, and settle into the operate phase. Now we open the operate phase and look at *what a server actually exposes*. The three server→client primitives — **Tools**, **Resources**, and **Prompts** — are the entirety of what a server offers a host on this side of the connection. (The reverse direction, server *asking the host* for things, is [Lecture 04](Lecture-04.md).) Everything an MCP server does for an agent lands in exactly one of these three buckets.

The temptation, coming from plain function calling, is to see all three as "tools with different names." Resist it. The protocol draws a deliberate line through them based on **who decides to use the thing** — the model, the application, or the human. That single distinction, the *control trichotomy*, is the most important idea in this lecture, because it is what makes MCP safe to wire into a host UI. A tool the model can fire on its own is a fundamentally different security and UX object from a document the application chose to attach or a slash-command the user typed. Get the trichotomy right and the rest — schemas, URIs, templates — is detail.

Everything here is JSON-RPC 2.0 over the transport from [Lecture 02](Lecture-02.md), pinned to the **2025-11-25** stable spec. We will read the actual wire messages, not SDK sugar; [Lecture 05](Lecture-05.md) puts FastMCP on top of them.

---

**Learning objectives**

By the end of this lecture you should be able to:

- State the **control trichotomy** — Tools are model-controlled, Resources are application-controlled, Prompts are user-controlled — and explain why that separation matters for UX and safety.
- Define a tool with a JSON-Schema `inputSchema`, read a `tools/call` request and result, and interpret content blocks, `outputSchema`/structured content, and the `isError` flag.
- Address data with **resource URIs** and **resource templates** (RFC 6570), and subscribe to resource changes via `resources/subscribe` and `notifications/resources/updated`.
- Define a **prompt** with typed arguments and read the message list returned by `prompts/get`.
- Map each primitive to how a host *surfaces* it — tools as callable functions, resources as attachable context, prompts as user commands.

---

</details>

## 1. 控制三分法 —— 关键心智模型

三种原语，三种不同的角色分别决定各自何时被使用。下面这张表必须记住：

| 原语 | 控制 | 谁决定使用它 | 只读？ | 典型示例 |
|-----------|---------|-----------------------|------------|-----------------|
| **Tools** | model-controlled | LLM 在生成中途选择调用它 | 否 —— 动作、副作用 | `create_issue`, `send_email`, `run_query` |
| **Resources** | application-controlled | 由宿主应用决定拉入哪些 context | 是 —— 只读数据 | 文件内容、DB 行、API 响应、日志 |
| **Prompts** | user-controlled | 由人主动调用（斜杠命令、菜单项） | 不适用 —— 模板 | `/summarize`、“Plan a trip”、代码审查模板 |

把每一行读成一个句子：*模型* 决定调用某个 **tool**；*应用* 决定呈现某个 **resource**；*用户* 决定调用某个 **prompt**。中间一列的角色才是关键所在。

协议为什么要费力这样拆分，而不是只提供一种通用的“能力”？两个原因，都至关重要。

**UX。** 每个角色在宿主中需要不同的控制面。Tools 变成提供给模型的函数。Resources 变成用户可以挂载、或应用可以自动纳入的东西 —— `@` 提及、文件选择器、context 面板。Prompts 变成用户触发的命令 —— 斜杠菜单、按钮。如果把这三种都坍缩成“tools”，宿主就没有原则性的方式去渲染它们，用户也就分不清 *模型做了这件事* 和 *我要求做这件事*。

**安全性。** 危险的原语是 **模型** 控制的那个。Tools 会执行动作并产生副作用，而模型是主动触发它们 —— 所以 tools 恰恰是宿主需要 **审批门禁**、审计轨迹，以及 §2 中讲的那些注解的地方。Resources 是只读的，而且是 *应用* 选择了它们，所以爆炸半径就是“模型看到了一些应用本来就决定共享的数据”。Prompts 是 *用户* 显式调用的惰性模板。这个三分法归根结底是在说明信任边界位于何处：**模型控制的 = 要设防；用户/应用控制的 = 它已经被人或宿主授权过了。** 第 07 讲就在这个基础上构建整个 MCP 威胁模型 —— 现在先记住，“谁控制它”是一个安全命题，而不是分类法上的便利。

还有一个容易让人绊倒的推论，§3 会再回到它：**resources 不会被自动注入。** 服务器 *提供* 某个 resource，并不会把它放进模型的 context。由应用决定。接下来我们逐一过每个原语时，请把这一点记在脑子里。

---

## 2. 深入 tools

Tools 是模型控制的原语：LLM 在生成过程中决定调用某一个。它们是你在 [Tool Use & Function Calling](../01-讲座/Lecture-08.md) 中见过的 function-calling 的 MCP 对应物 —— 但是被 *协议化* 了，因此任何宿主都能发现并调用它们，无需定制接线。

### 发现与调用

整个生命周期由两个方法承载：

- **`tools/list`** —— 宿主向服务器索取其 tools 目录。每个条目带有一个 `name`、一段人类可读的 `description`、一个 `inputSchema`，以及可选的 `outputSchema` 和 `annotations`。
- **`tools/call`** —— 宿主按 `name` 调用某个 tool，传入一个 `arguments` 对象，并取回结果。

### `inputSchema`

每个 tool 都在 `inputSchema` 中以 **JSON Schema** 对象声明自己的参数。正是这一点让模型能生成格式正确的 arguments，也让宿主能在调用发出之前校验它们。下面是一个来自 `tools/list` 的 tool 定义：

```json
{
  "name": "create_issue",
  "title": "Create GitHub Issue",
  "description": "Open a new issue in a repository.",
  "inputSchema": {
    "type": "object",
    "properties": {
      "repo":  { "type": "string", "description": "owner/name, e.g. octo/hello" },
      "title": { "type": "string" },
      "body":  { "type": "string" },
      "labels": {
        "type": "array",
        "items": { "type": "string" }
      }
    },
    "required": ["repo", "title"]
  },
  "annotations": {
    "title": "Create GitHub Issue",
    "readOnlyHint": false,
    "destructiveHint": false,
    "idempotentHint": false,
    "openWorldHint": true
  }
}
```

### `outputSchema` 与结构化内容（2025 新增）

最初，tool 结果只是一些非结构化的内容块（文本、图像）。2025 规范在 tool 定义上增加了一个可选的 **`outputSchema`**，并在结果中加入了 **结构化内容**，这样 tool 就能返回有类型、机器可读的 JSON，宿主可以拿它对照 schema 进行校验 —— 而不只是一坨文本让模型重新解析。当某个 tool 声明了 `outputSchema` 时，它的结果会带一个符合该 schema 的 `structuredContent` 字段。正是这一点让 tool 从“返回一个字符串”变成“返回一个 `WeatherReport` 对象”。


<details>
<summary>English original</summary>

**1. The control trichotomy — the key mental model**

Three primitives, three different actors deciding when each is used. This is the table to memorize:

| Primitive | Control | Who decides to use it | Read-only? | Typical example |
|-----------|---------|-----------------------|------------|-----------------|
| **Tools** | model-controlled | the LLM, mid-generation, chooses to call it | No — actions, side effects | `create_issue`, `send_email`, `run_query` |
| **Resources** | application-controlled | the host app decides what context to pull in | Yes — read-only data | a file's contents, a DB row, an API response, a log |
| **Prompts** | user-controlled | the human invokes it (slash command, menu item) | N/A — a template | `/summarize`, "Plan a trip", a code-review template |

Read each row as a sentence: *the model* decides to call a **tool**; *the application* decides to surface a **resource**; *the user* decides to invoke a **prompt**. The actor in the middle column is the whole point.

Why does the protocol bother splitting them this way instead of shipping one generic "capability"? Two reasons, both load-bearing.

**UX.** Each actor needs a different control surface in the host. Tools become functions offered to the model. Resources become things a user can attach or an app can auto-include — `@`-mentions, file pickers, context panels. Prompts become commands the user fires — slash-menus, buttons. If you collapse all three into "tools," the host has no principled way to render them, and the user loses the distinction between *the model did this* and *I asked for this*.

**Safety.** The dangerous primitive is the one the **model** controls. Tools take actions and have side effects, and the model fires them on its own initiative — so tools are exactly where a host needs an **approval gate**, an audit trail, and the annotations we cover in §2. Resources are read-only and the *application* chose them, so the blast radius is "the model saw some data the app already decided to share." Prompts are inert templates the *user* explicitly invoked. The trichotomy is, at bottom, a statement about where the trust boundary sits: **model-controlled = guard it; user/app-controlled = it was already authorized by a human or the host.** Lecture 07 builds the entire MCP threat model on this foundation — for now, internalize that "who controls it" is a security claim, not a taxonomy convenience.

One more consequence that trips people up, and which §3 returns to: **resources are not auto-injected.** A server *offering* a resource does not put it in the model's context. The application decides. Keep that in your head as we go through each primitive in turn.

---

**2. Tools in depth**

Tools are the model-controlled primitive: the LLM, while generating, decides to call one. They are the MCP analogue of the function-calling you saw in [Tool Use & Function Calling](../01-讲座/Lecture-08.md) — but *protocolized*, so any host can discover and invoke them without bespoke wiring.

**Discovery and invocation**

Two methods carry the whole lifecycle:

- **`tools/list`** — the host asks the server for its catalog of tools. Each entry carries a `name`, a human-readable `description`, an `inputSchema`, and optionally an `outputSchema` and `annotations`.
- **`tools/call`** — the host invokes one tool by `name` with an `arguments` object, and gets back a result.

**The `inputSchema`**

Every tool declares its parameters as a **JSON Schema** object in `inputSchema`. This is what lets the model produce well-formed arguments and what lets the host validate them before the call goes out. A tool definition from `tools/list`:

```json
{
  "name": "create_issue",
  "title": "Create GitHub Issue",
  "description": "Open a new issue in a repository.",
  "inputSchema": {
    "type": "object",
    "properties": {
      "repo":  { "type": "string", "description": "owner/name, e.g. octo/hello" },
      "title": { "type": "string" },
      "body":  { "type": "string" },
      "labels": {
        "type": "array",
        "items": { "type": "string" }
      }
    },
    "required": ["repo", "title"]
  },
  "annotations": {
    "title": "Create GitHub Issue",
    "readOnlyHint": false,
    "destructiveHint": false,
    "idempotentHint": false,
    "openWorldHint": true
  }
}
```

**`outputSchema` and structured content (2025 addition)**

Originally a tool result was just unstructured content blocks (text, images). The 2025 spec added an optional **`outputSchema`** on the tool definition plus **structured content** in the result, so a tool can return typed, machine-readable JSON that the host can validate against the schema — not just a blob of text the model has to re-parse. When a tool declares an `outputSchema`, its result carries a `structuredContent` field conforming to that schema. This is what turns a tool from "returns a string" into "returns a `WeatherReport` object."

</details>

### 工具注解 —— 以及宿主为何使用它们（前向引用：安全）

注解是关于工具行为的 **提示**，宿主可用它来驱动自己的审批 UX。它们是建议性的元数据，而非强制执行 —— 由服务器声明，谨慎的宿主会把它们当作来自服务器的不可信输入（Lecture 07）。四条标准提示如下：

| 注解 | 含义 | 宿主如何使用 |
|------------|---------|--------------------|
| `readOnlyHint` | 该工具不修改其环境 | 跳过审批提示；可自由调用 / 并行调用 |
| `destructiveHint` | 该工具可能执行不可逆更新（删除、覆写） | 要求显式确认；醒目告警 |
| `idempotentHint` | 相同参数重复调用不产生额外影响 | 超时重试安全，不会重复产生副作用 |
| `openWorldHint` | 该工具会接触外部实体（互联网、远程 API） | 标记网络外发；与数据外泄风险相关 |

还有一个 `title` 注解 —— 一个对人类友好的显示名，区别于机器使用的 `name`。这四条行为提示的意义全在于 **审批 UX**：知道 `delete_repo` 带有 `destructiveHint: true` 的宿主可以把它挡在确认对话框之后，而让像 `search_code` 这样的 `readOnlyHint: true` 工具无需打断用户即可运行。至于 *为什么宿主不能盲目信任这些提示*，Lecture 07 会再讨论 —— 恶意服务器可以撒谎 —— 但机制就在这里。

### 结果内容块与 `isError`

一个 `tools/call` 结果是一个 **内容块** 列表，外加一个 `isError` 标志。内容块可以是：

- **text** — 最常见的情况。
- **image** / **audio** — 带 MIME 类型的 base64 数据。
- **embedded or linked resources** — 工具结果可以回传一个 resource（内联或通过 URI），从而桥接进入 §3 的 Resources 世界。

**`isError`** 布尔值区分 *工具级* 错误（工具运行了但失败 —— 仓库名错误、API 404）与 *协议级* 错误（一个 JSON-RPC 错误对象，意味着调用本身格式非法）。这一区分很重要：带 `isError: true` 的工具级错误会被送 *回给模型*，让它能够反应并重试，而协议错误则是 host↔server 管道中的故障。下面是一个 `tools/call` 请求和一个成功的结果：

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "tools/call",
  "params": {
    "name": "create_issue",
    "arguments": {
      "repo": "octo/hello",
      "title": "Build is flaky on ARM",
      "labels": ["bug", "ci"]
    }
  }
}
```

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "content": [
      { "type": "text", "text": "Created issue #128: Build is flaky on ARM" }
    ],
    "structuredContent": {
      "issue_number": 128,
      "url": "https://github.com/octo/hello/issues/128",
      "state": "open"
    },
    "isError": false
  }
}
```

工具级失败会以相同的结构返回，带有 `isError: true` 和一个说明性的 text 块，这样模型就能读出哪里出错并做出调整。

---

## 3. 深入 Resources

Resources 是 **应用控制** 的只读原语：由服务器暴露的可寻址数据，由 *宿主应用* —— 而非模型 —— 决定是否拉入上下文。文件、数据库行、API 响应、日志、截图：任何模型可能需要 *读取* 但不应 *对其采取行动* 的东西。

### 发现与读取

- **`resources/list`** — 服务器枚举其可用资源，每个资源带有一个 `uri`、一个 `name`，可选地还有 `description` 和 `mimeType`。
- **`resources/read`** — 宿主按 URI 请求某个资源的内容，并取回其数据（文本或二进制）。

### URI 与 scheme

每个资源都由一个 **URI** 标识。scheme 表明它是哪一类东西：

- **`file://`** — 本地文件系统路径。
- **`https://`** — web 资源。
- **自定义 scheme** — 服务器可以自造，例如 `postgres://`、`screen://`、`git://`。scheme 由服务器定义；宿主只把 URI 当作一个不透明地址，可传给 `resources/read`。


<details>
<summary>English original</summary>

**Tool annotations — and why a host uses them (forward-ref: security)**

Annotations are **hints** about a tool's behavior that the host can use to drive its approval UX. They are advisory metadata, not enforcement — the server asserts them, and a careful host treats them as untrusted input from the server (Lecture 07). The four standard hints:

| Annotation | Meaning | How a host uses it |
|------------|---------|--------------------|
| `readOnlyHint` | The tool does not modify its environment | Skip the approval prompt; safe to call freely / in parallel |
| `destructiveHint` | The tool may perform irreversible updates (deletes, overwrites) | Require explicit confirmation; warn loudly |
| `idempotentHint` | Repeated calls with the same arguments have no additional effect | Safe to retry on timeout without duplicating side effects |
| `openWorldHint` | The tool touches external entities (the internet, a remote API) | Flag network egress; relevant to data-exfiltration risk |

There is also a `title` annotation — a human-friendly display name distinct from the machine `name`. The point of all four behavioral hints is the **approval UX**: a host that knows `delete_repo` carries `destructiveHint: true` can gate it behind a confirmation dialog, while letting a `readOnlyHint: true` tool like `search_code` run without interrupting the user. We return to *why a host must not blindly trust these hints* in Lecture 07 — a malicious server can lie — but the mechanism is here.

**Result content blocks and `isError`**

A `tools/call` result is a list of **content blocks** plus an `isError` flag. Content blocks can be:

- **text** — the common case.
- **image** / **audio** — base64 data with a MIME type.
- **embedded or linked resources** — a tool result can hand back a resource (inline or by URI), bridging into the Resources world from §3.

The **`isError`** boolean distinguishes a *tool-level* error (the tool ran but failed — bad repo name, API 404) from a *protocol-level* error (a JSON-RPC error object, meaning the call itself was malformed). This split matters: a tool-level error with `isError: true` is fed *back to the model* so it can react and retry, whereas a protocol error is a fault in the host↔server plumbing. Here is a `tools/call` request and a successful result:

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "tools/call",
  "params": {
    "name": "create_issue",
    "arguments": {
      "repo": "octo/hello",
      "title": "Build is flaky on ARM",
      "labels": ["bug", "ci"]
    }
  }
}
```

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "result": {
    "content": [
      { "type": "text", "text": "Created issue #128: Build is flaky on ARM" }
    ],
    "structuredContent": {
      "issue_number": 128,
      "url": "https://github.com/octo/hello/issues/128",
      "state": "open"
    },
    "isError": false
  }
}
```

A tool-level failure returns on the same shape with `isError: true` and an explanatory text block, so the model can read what went wrong and adjust.

---

**3. Resources in depth**

Resources are the **application-controlled**, read-only primitive: addressable data the server exposes, which the *host application* — not the model — chooses to pull into context. Files, database rows, API responses, logs, screenshots: anything the model might need to *read* but should not *act on*.

**Discovery and reading**

- **`resources/list`** — the server enumerates its available resources, each with a `uri`, a `name`, optionally a `description` and `mimeType`.
- **`resources/read`** — the host requests the contents of one resource by URI, and gets back its data (text or binary).

**URIs and schemes**

Every resource is identified by a **URI**. The scheme tells you what kind of thing it is:

- **`file://`** — local filesystem paths.
- **`https://`** — web resources.
- **custom schemes** — a server can mint its own, e.g. `postgres://`, `screen://`, `git://`. The scheme is server-defined; the host just treats the URI as an opaque address it can pass to `resources/read`.

</details>

### 资源模板（RFC 6570）

服务器很少会想把数据库里的*每一*行都当作静态资源枚举出来。它转而暴露一个**资源模板**——一个使用 **RFC 6570 URI Template** 语法的参数化 URI——由 host 填入变量，构造出一个具体 URI 供读取。模板条目如下所示：

```json
{
  "resourceTemplates": [
    {
      "uriTemplate": "postgres://db/users/{user_id}",
      "name": "User record",
      "description": "A single user row by id.",
      "mimeType": "application/json"
    },
    {
      "uriTemplate": "file:///logs/{date}/{service}.log",
      "name": "Service log",
      "description": "Daily log file for a given service.",
      "mimeType": "text/plain"
    }
  ]
}
```

`{user_id}`、`{date}`、`{service}` 是 RFC 6570 展开式占位符。host 替换其中的值——`postgres://db/users/42`——并对结果调用 `resources/read`。模板就是服务器表达“我能给你*任意*一个用户，而不是一份固定列表”的方式。

### 订阅与 `notifications/resources/updated`

资源会变化。日志会增长；某一行会被编辑。MCP 支持**订阅**，让 host 无需轮询就能保持已挂载的上下文为最新：

- **`resources/subscribe`**——host 订阅某个具体的资源 URI。
- **`notifications/resources/updated`**——当该资源发生变化时，服务器推送这条通知，host 可以重新 `read` 它。

```json
{
  "jsonrpc": "2.0",
  "method": "notifications/resources/updated",
  "params": {
    "uri": "file:///logs/2026-06-24/api.log"
  }
}
```

（还有一个 `notifications/resources/list_changed`，用于资源的*集合*发生变化时，与单个资源的内容变化不同。）

### 关键点：资源由应用控制，而非被静默注入

这一点是大多数 function-calling 老手都会搞错的地方。**服务器提供一个资源，并不意味着把它推入模型的上下文。**提供 ≠ 注入。模型能看到什么，由 host *决定*。应用可以把资源以 `@` mention 的形式呈现给用户挑选，也可以呈现为用户可浏览的文件树，还可以依据自身策略自动纳入一部分——但**这个决定属于 host，既不属于服务器，也不属于模型。**

这正是 Resources 与 Tools 分属不同原语的根本原因。Tool 是模型主动伸手去*做*某件事。Resource 是应用在决定*这份数据是相关的，把它展示给模型。*只读、由人/应用筛选，绝不会带来意外。如果你的设计要求模型*自主获取*任意数据，那它是**tool**（`fetch_url`），而不是 resource——因为此时做出决定的是模型，§1 中的信任边界也随之移动。

---

## 4. 深入剖析 Prompts

Prompts 是**由用户控制**的原语：可复用、模板化的消息序列，由*人*来调用——通常是以斜杠命令或菜单项的形式。Tool 是“模型能做 X”，resource 是“应用能展示 X”，而 prompt 则是“用户可以请求 X，且已预先打包”。

### 发现与检索

- **`prompts/list`**——服务器枚举自己的 prompts，每个 prompt 带有一个 `name`、一个 `description`，以及一个带类型的 `arguments` 列表。
- **`prompts/get`**——host 按名称请求某个具体的 prompt，为其参数传入取值，取回一个可用于初始化对话的**消息列表**。

### 带类型的参数

prompt 把它的参数声明为 `arguments`，每个参数带有一个 `name`、一个 `description`，以及一个 `required` 标志。host 把它们渲染成由用户填写的字段（或自动提供）。来自 `prompts/list` 的一个 prompt 定义：

```json
{
  "name": "code_review",
  "title": "Review a pull request",
  "description": "Generate a focused review of a code change.",
  "arguments": [
    { "name": "language", "description": "Programming language", "required": true },
    { "name": "diff", "description": "The unified diff to review", "required": true },
    { "name": "focus", "description": "e.g. security, performance", "required": false }
  ]
}
```


<details>
<summary>English original</summary>

**Resource templates (RFC 6570)**

A server rarely wants to enumerate *every* row in a database as a static resource. Instead it exposes a **resource template** — a parameterized URI using **RFC 6570 URI Template** syntax — and the host fills in the variables to construct a concrete URI to read. A template entry looks like this:

```json
{
  "resourceTemplates": [
    {
      "uriTemplate": "postgres://db/users/{user_id}",
      "name": "User record",
      "description": "A single user row by id.",
      "mimeType": "application/json"
    },
    {
      "uriTemplate": "file:///logs/{date}/{service}.log",
      "name": "Service log",
      "description": "Daily log file for a given service.",
      "mimeType": "text/plain"
    }
  ]
}
```

The `{user_id}`, `{date}`, `{service}` placeholders are RFC 6570 expansions. The host substitutes values — `postgres://db/users/42` — and calls `resources/read` on the result. Templates are how a server says "I can give you *any* user, not a fixed list."

**Subscriptions and `notifications/resources/updated`**

Resources can change. A log grows; a row is edited. MCP supports **subscriptions** so a host can keep its attached context fresh without polling:

- **`resources/subscribe`** — the host subscribes to a specific resource URI.
- **`notifications/resources/updated`** — the server pushes this notification when that resource changes, and the host can re-`read` it.

```json
{
  "jsonrpc": "2.0",
  "method": "notifications/resources/updated",
  "params": {
    "uri": "file:///logs/2026-06-24/api.log"
  }
}
```

(There is also a `notifications/resources/list_changed` for when the *set* of resources changes, distinct from the contents of one resource.)

**The crucial point: resources are application-controlled, not silently injected**

Here is the idea most function-calling veterans get wrong. **A server offering a resource does not push it into the model's context.** Offering ≠ injecting. The host *decides* what the model sees. The application might surface resources as `@`-mentions for the user to pick, as a file-tree the user browses, or it might auto-include some based on its own policy — but **that decision belongs to the host, not the server and not the model.**

This is the whole reason Resources are a separate primitive from Tools. A tool is the model reaching out and *doing* something on its own initiative. A resource is the application deciding *this data is relevant, show it to the model.* Read-only, human/app-curated, never a surprise. If your design needs the model to *autonomously fetch* arbitrary data, that is a **tool** (`fetch_url`), not a resource — because now the model is the one deciding, and the trust boundary from §1 moves accordingly.

---

**4. Prompts in depth**

Prompts are the **user-controlled** primitive: reusable, templated message sequences that the *human* invokes — typically as a slash command or a menu item. Where a tool is "the model can do X" and a resource is "the app can show X," a prompt is "the user can ask for X, pre-packaged."

**Discovery and retrieval**

- **`prompts/list`** — the server enumerates its prompts, each with a `name`, a `description`, and a list of typed `arguments`.
- **`prompts/get`** — the host requests a specific prompt by name, passing values for its arguments, and gets back a **list of messages** ready to seed the conversation.

**Typed arguments**

A prompt declares its parameters as `arguments`, each with a `name`, a `description`, and a `required` flag. The host renders these as fields the user fills in (or auto-supplies). A prompt definition from `prompts/list`:

```json
{
  "name": "code_review",
  "title": "Review a pull request",
  "description": "Generate a focused review of a code change.",
  "arguments": [
    { "name": "language", "description": "Programming language", "required": true },
    { "name": "diff", "description": "The unified diff to review", "required": true },
    { "name": "focus", "description": "e.g. security, performance", "required": false }
  ]
}
```

</details>

### 返回消息序列

`prompts/get` 并不返回单个字符串——它返回一个**消息列表**（每条消息带 `role` 和 `content`），宿主将其注入为对话的开头。这使得一个 prompt 能建立多轮框架（例如一条 system 风格指令加一个 user 轮次），而不只是一句话。一次 `prompts/get` 请求及其结果：

```json
{
  "jsonrpc": "2.0",
  "id": 7,
  "method": "prompts/get",
  "params": {
    "name": "code_review",
    "arguments": {
      "language": "Python",
      "focus": "security"
    }
  }
}
```

```json
{
  "jsonrpc": "2.0",
  "id": 7,
  "result": {
    "description": "A focused code review prompt",
    "messages": [
      {
        "role": "user",
        "content": {
          "type": "text",
          "text": "You are reviewing Python code. Focus on security. Flag injection risks, unsafe deserialization, and secrets in code. Be specific and cite line numbers."
        }
      }
    ]
  }
}
```

用户从菜单中选中 `code_review`；宿主调用 `prompts/get`；返回的消息成为对话的开场。注意其**用户控制**的特性：在人类选定这个 prompt 之前什么都不会发生。这就是其决定性属性，也是 prompt 表现为命令、而绝不由模型或应用自行触发的原因。

---

## 5. 宿主如何呈现每种原语

§1 中的三分法并不抽象——它精确规定了每种原语在真实宿主 UI 中出现的方式。三种控制模型，三种呈现方式：

| 原语 | 控制 | 宿主如何呈现 | 映射到 |
|-----------|---------|--------------------------|---------|
| **Tools** | 模型控制 | 在模型的工具列表中作为**可调用函数**提供；模型发出调用，宿主（可选地由 annotations 门控）执行 | function calling —— 见 [Tool Use & Function Calling](../01-讲座/Lecture-08.md) |
| **Resources** | 应用控制 | 作为**可附加的上下文**提供给用户/应用 —— `@` 提及、文件/资源选择器、上下文面板；由应用决定包含什么 | 只读上下文附加 |
| **Prompts** | 用户控制 | 作为**命令**提供给用户 —— 斜杠菜单项、按钮、人类点击的菜单项 | UI 可供性 / 命令面板 |

贯穿的主线是：**工具交给模型，资源交给应用的上下文组装，prompt 交给用户的命令界面。**在 [Lecture 05](Lecture-05.md) 中设计 MCP server 时，对每项能力的第一个问题是“由哪个角色控制它？”——答案会告诉你该用哪种原语，因而也说明每个合规宿主将如何渲染它。工具直接回溯到 [Tool Use & Function Calling](../01-讲座/Lecture-08.md) 中的 function-calling 机制；MCP 的贡献在于把这套机制变成一个*协议*，使模型能从任意 server 触达工具，并与应用精选的资源和用户的 prompt 命令并存。

掌握这三种正向原语后，[Lecture 04](Lecture-04.md) 把连接反转过来：Sampling、Roots 和 Elicitation —— 这些是 *server* 向*宿主*提出的请求，正是这个方向让 MCP 真正双向。

---

## 截至当前

- **日期：**2026 年 6 月
- **规范版本：**MCP **2025-11-25**（稳定版）。控制三分法（Tools = 模型控制、Resources = 应用控制、Prompts = 用户控制）、方法名（`tools/list`、`tools/call`、`resources/list`、`resources/read`、`resources/subscribe`、`prompts/list`、`prompts/get`）、`outputSchema`/结构化内容、工具 annotations（`readOnlyHint`、`destructiveHint`、`idempotentHint`、`openWorldHint`）、RFC 6570 资源模板以及 `notifications/resources/updated` 在本修订版中均为最新。在依赖确切字段名之前，请对照 [modelcontextprotocol.io](https://modelcontextprotocol.io) 上的规范和你已安装的 SDK 核实原语形态。


<details>
<summary>English original</summary>

**Returning a sequence of messages**

`prompts/get` does not return a single string — it returns a **list of messages** (each with a `role` and `content`), which the host injects as the start of the conversation. This lets a prompt set up a multi-turn frame (a system-style instruction plus a user turn, for instance), not just a one-liner. A `prompts/get` request and its result:

```json
{
  "jsonrpc": "2.0",
  "id": 7,
  "method": "prompts/get",
  "params": {
    "name": "code_review",
    "arguments": {
      "language": "Python",
      "focus": "security"
    }
  }
}
```

```json
{
  "jsonrpc": "2.0",
  "id": 7,
  "result": {
    "description": "A focused code review prompt",
    "messages": [
      {
        "role": "user",
        "content": {
          "type": "text",
          "text": "You are reviewing Python code. Focus on security. Flag injection risks, unsafe deserialization, and secrets in code. Be specific and cite line numbers."
        }
      }
    ]
  }
}
```

The user picked `code_review` from a menu; the host called `prompts/get`; the returned messages become the opening of the conversation. Note the **user-controlled** nature: nothing happened until the human chose this prompt. That is the defining property, and it is why prompts surface as commands, never as something the model or app fires on its own.

---

**5. How a host surfaces each primitive**

The trichotomy from §1 is not abstract — it dictates exactly how each primitive shows up in a real host UI. Three control models, three surfaces:

| Primitive | Control | How the host surfaces it | Maps to |
|-----------|---------|--------------------------|---------|
| **Tools** | model-controlled | Offered to the model as **callable functions** in its tool list; the model emits a call, the host (optionally gated by annotations) executes it | function calling — see [Tool Use & Function Calling](../01-讲座/Lecture-08.md) |
| **Resources** | application-controlled | Offered to the user/app as **attachable context** — `@`-mentions, file/resource pickers, context panels; the app decides what to include | read-only context attachment |
| **Prompts** | user-controlled | Offered to the user as **commands** — slash-menu entries, buttons, menu items the human clicks | UI affordances / command palette |

The through-line: **tools go to the model, resources go to the application's context-assembly, prompts go to the user's command surface.** When you design an MCP server in [Lecture 05](Lecture-05.md), the first question for every capability is "which actor controls this?" — and the answer tells you which primitive to use and, therefore, how every compliant host will render it. Tools tie directly back to the function-calling machinery in [Tool Use & Function Calling](../01-讲座/Lecture-08.md); MCP's contribution is making that machinery a *protocol* so the model can reach tools from any server, alongside the application's curated resources and the user's prompt commands.

With the three forward primitives in hand, [Lecture 04](Lecture-04.md) turns the connection around: Sampling, Roots, and Elicitation — the things a *server* asks of the *host*, the direction that makes MCP genuinely bidirectional.

---

**Current as of**

- **Date:** June 2026
- **Spec revision:** MCP **2025-11-25** (stable). The control trichotomy (Tools = model-controlled, Resources = application-controlled, Prompts = user-controlled), the method names (`tools/list`, `tools/call`, `resources/list`, `resources/read`, `resources/subscribe`, `prompts/list`, `prompts/get`), `outputSchema`/structured content, tool annotations (`readOnlyHint`, `destructiveHint`, `idempotentHint`, `openWorldHint`), RFC 6570 resource templates, and `notifications/resources/updated` are all current as of this revision. Verify primitive shapes against the spec at [modelcontextprotocol.io](https://modelcontextprotocol.io) and your installed SDK before relying on exact field names.

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/MCP for AI Agents/Lecture-03.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/MCP%20for%20AI%20Agents/Lecture-03.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
