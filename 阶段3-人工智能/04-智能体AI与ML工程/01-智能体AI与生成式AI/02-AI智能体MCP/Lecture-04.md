---
title: 第 04 讲 - 反向原语：采样、Roots、Elicitation
description: 第 04 讲 - 反向原语：采样、Roots、Elicitation
---

# 第 04 讲 - 反向原语：采样、Roots、Elicitation

**Collection:** [MCP for AI Agents](README.md) | **Previous:** [← 第 03 讲](Lecture-03.md) | **Next:** [第 05 讲](Lecture-05.md)

---

第 03 讲介绍了从 server *向外* 指的三种原语：Tools、Resources 和 Prompts。这三者中，client（或其模型）都是伸进 server 里把东西取出来 —— 调用 tool、读取 resource、获取 prompt。这正是人们听到 “MCP” 时脑中浮现的方向，也是协议大部分内容运行的方向。但这并非协议的全部，只用这三者搭起来的 server 是一个 *被动* 的 server：它能回答，却不能推理，不能提问，也无从得知自己的沙箱边界在哪里。

本讲要讲的三种原语方向相反。**采样**、**Roots** 和 **Elicitation** 是 *反向* 原语 —— 由 server 向 client/host 发起请求，由 host 应答。正是这种反转，才让 MCP 真正 *双向*，而不只是给 function calling 套了层花哨的 RPC 外壳。能回调 host 的 server，可以借用 host 的 LLM 在任务中途推理（采样），可以被交给一条它必须待在其中、最小权限的边界（Roots），也可以在做出不可逆操作前停下来、向人类提一个结构化的问题（Elicitation）。这些都不需要 server 自己持有 API key、自带 UI，或去猜用户的文件系统布局 —— host 三者都已有，server 只需 *委派* 给它。

这件事对智能体化系统之所以重要，在于控制权。被动的 tool server 是 agent *使用* 的东西；带反向原语的 server 则是 agent *协作* 的对象 —— 而且关键在于，每一次反向调用都被设计成让 host 保有最终决定权。host 选择模型并为其付费；host 划定文件系统边界；host 掌握用户的注意力。把这个反转搞清楚了，就能理解为什么 MCP 是 agent 的集成层，而不只是一条 tool 总线。

---

## 学习目标

学完本讲后，你应当能够：

* 解释 MCP 为什么需要 **server→client** 请求，并把它与第 03 讲的 **client→server** 原语精确对照。
* 用 `messages`、`modelPreferences`、`systemPrompt` 和 `maxTokens` 构造一个 **`sampling/createMessage`** 请求 —— 并说出 host *绝不* 交出的三个控制点。
* 把 **`roots/list`** 用作最小权限的 *边界*（而非发现机制），并处理 **`notifications/roots/list_changed`**。
* 用 JSON Schema 发起一个 **`elicitation/create`** 请求，并说明约束 server *何时* 可以发起 elicitation 的 **SEP-2260** 规则。
* 对这三者都做 **能力门控**，并在 client 未在 `initialize` 中声明该能力时优雅降级。

---


<details>
<summary>English original</summary>

**Lecture 04 - Reverse Primitives: Sampling, Roots, Elicitation**

**Collection:** [MCP for AI Agents](README.md) | **Previous:** [← Lecture 03](Lecture-03.md) | **Next:** [Lecture 05](Lecture-05.md)

---

Lecture 03 covered the three primitives that point *outward* from a server: Tools, Resources, and Prompts. In all three, the client (or its model) reaches into the server and pulls something out — calls a tool, reads a resource, fetches a prompt. That is the direction everyone pictures when they hear "MCP," and it is the direction most of the protocol runs. But it is not the whole protocol, and a server built only on those three is a *passive* server: it can answer, but it cannot reason, it cannot ask, and it cannot be told where its sandbox ends.

The three primitives in this lecture run the other way. **Sampling**, **Roots**, and **Elicitation** are *reverse* primitives — the server initiates a request *of the client/host*, and the host answers. This inversion is the thing that makes MCP genuinely *bidirectional* rather than a fancy RPC wrapper for function calling. A server that can call back to the host can borrow the host's LLM to reason mid-task (Sampling), it can be handed a least-privilege boundary it must stay inside (Roots), and it can pause and ask the human a structured question before it does something irreversible (Elicitation). None of those require the server to own an API key, ship a UI, or guess at the user's filesystem layout — the host already has all three, so the server *delegates* to it.

The reason this matters for agentic systems is control. A passive tool server is something the agent *uses*; a server with reverse primitives is something the agent *collaborates with* — and crucially, every reverse call is structured so the host keeps the final say. The host chooses the model and pays for it; the host draws the filesystem boundary; the host owns the user's attention. Get this inversion right and you understand why MCP is the integration layer for agents and not just a tool bus.

---

**Learning objectives**

By the end of this lecture you should be able to:

* Explain why MCP needs **server→client** requests, and contrast them precisely with the **client→server** primitives of Lecture 03.
* Construct a **`sampling/createMessage`** request with `messages`, `modelPreferences`, `systemPrompt`, and `maxTokens` — and name the three control points the host *never* surrenders.
* Use **`roots/list`** as a least-privilege *boundary* (not a discovery mechanism) and handle **`notifications/roots/list_changed`**.
* Issue an **`elicitation/create`** request with a JSON Schema, and state the **SEP-2260** rule that governs *when* a server is allowed to elicit.
* **Capability-gate** all three and degrade gracefully when the client did not advertise the capability in `initialize`.

---

</details>

## 1. 反转：每个原语指向哪一边？

六个 MCP 原语按*谁发起请求*干净地一分为二。Lecture 03 里的三个是 server→client 的*提供物*，由 client 拉取；这里的三个是 server 发起的*请求*，由 client/host 提供服务。

| 原语 | 方向 | 发起方 | 服务方 | Method | 控制方 |
|---|---|---|---|---|---|
| **Tools** | server → client | client/model | server | `tools/call` | 模型 |
| **Resources** | server → client | client/app | server | `resources/read` | 应用 |
| **Prompts** | server → client | client/user | server | `prompts/get` | 用户 |
| **Sampling** | **client ← server** | **server** | **host** | `sampling/createMessage` | host（模型、密钥、审批） |
| **Roots** | **client ← server** | **server** | **client** | `roots/list` | client（授予边界） |
| **Elicitation** | **client ← server** | **server** | **user** | `elicitation/create` | 用户（回答 / 拒绝） |

前三个朝同一方向流动；后三个反向流回。这种双向性正是全部要点：

```text
   CLIENT / HOST                           SERVER
   (owns model, keys,         tools/call ───────►  (owns tool logic,
    UI, filesystem)           resources/read ───►   data, business rules)
                              prompts/get ──────►
        ┌───────────────────────────────────────────────┐
        │           the inversion (this lecture)         │
        └───────────────────────────────────────────────┘
   (runs the LLM)    ◄─── sampling/createMessage   (wants to reason)
   (grants a sandbox)◄─── roots/list               (must stay in bounds)
   (owns the human)  ◄─── elicitation/create       (needs user input)
```

反向调用究竟为何存在？有三个需求是被动 server 无法独自满足的：

* **它想要推理** —— 总结文档、分类记录、起草文本 —— 但它不得自带模型或 API key。于是它*借用 host 的 LLM*（Sampling）。
* **它必须待在沙箱内** —— 只触碰用户真正授权的目录 —— 但它无法预先知道该布局。于是 *client 递给它一个边界*（Roots）。
* **它需要只有人类才掌握的信息** —— 一个缺失的参数、对破坏性步骤的是/否 —— 但它没有 UI。于是它*通过 host 提问*（Elicitation）。

每种情况下，server 缺少的能力（模型、文件系统边界、人类）都是 **host 已经拥有**的东西，因此 server 选择委托而非自行复制。这就是全部三个反向原语背后的设计原则。

---


<details>
<summary>English original</summary>

**1. The inversion: which way does each primitive point?**

The six MCP primitives split cleanly by *who initiates the request*. The three from Lecture 03 are server→client *offerings* that the client pulls from; the three here are server-initiated *requests* that the client/host services.

| Primitive | Direction | Initiated by | Serviced by | Method | Controlled by |
|---|---|---|---|---|---|
| **Tools** | server → client | client/model | server | `tools/call` | model |
| **Resources** | server → client | client/app | server | `resources/read` | application |
| **Prompts** | server → client | client/user | server | `prompts/get` | user |
| **Sampling** | **client ← server** | **server** | **host** | `sampling/createMessage` | host (model, keys, approval) |
| **Roots** | **client ← server** | **server** | **client** | `roots/list` | client (grants the boundary) |
| **Elicitation** | **client ← server** | **server** | **user** | `elicitation/create` | user (answers / declines) |

The first three flow one way; the last three flow back. That bidirectionality is the whole point:

```text
   CLIENT / HOST                           SERVER
   (owns model, keys,         tools/call ───────►  (owns tool logic,
    UI, filesystem)           resources/read ───►   data, business rules)
                              prompts/get ──────►
        ┌───────────────────────────────────────────────┐
        │           the inversion (this lecture)         │
        └───────────────────────────────────────────────┘
   (runs the LLM)    ◄─── sampling/createMessage   (wants to reason)
   (grants a sandbox)◄─── roots/list               (must stay in bounds)
   (owns the human)  ◄─── elicitation/create       (needs user input)
```

Why do reverse calls exist at all? Three needs that a passive server cannot meet on its own:

* **It wants to reason** — summarize a document, classify a record, draft text — but it must not ship its own model or API key. So it *borrows the host's LLM* (Sampling).
* **It must stay inside a sandbox** — touch only the directories the user actually authorized — but it cannot know that layout in advance. So the *client hands it a boundary* (Roots).
* **It needs information only the human has** — a missing argument, a yes/no on a destructive step — but it owns no UI. So it *asks through the host* (Elicitation).

In every case the capability the server lacks (a model, a filesystem boundary, a human) is something the **host already has**, so the server delegates instead of duplicating. That is the design principle behind all three reverse primitives.

---

</details>

## 2. Sampling —— 服务器借用宿主的 LLM

**Sampling**（`sampling/createMessage`）让服务器可以请求宿主*代表服务器*运行一次 LLM 补全。服务器构造它希望模型看到的对话；宿主用它自己掌控的模型来运行这段对话，并返回补全结果。正是这一点让服务器得以*智能体化*——它可以把 LLM 推理嵌套在一次工具调用内部（调用工具 → 通过 sampling 解读结果 → 再调用另一个工具），而自身始终不持有模型凭据。

一次 sampling 请求携带 `messages`、可选的 `modelPreferences`、一个可选的 `systemPrompt`，以及一个 `maxTokens` 上限：

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "sampling/createMessage",
  "params": {
    "messages": [
      {
        "role": "user",
        "content": {
          "type": "text",
          "text": "Classify the sentiment of this review as positive, negative, or neutral, and return only the single word:\n\n\"Shipping was slow but the product is exactly what I needed.\""
        }
      }
    ],
    "modelPreferences": {
      "hints": [{ "name": "claude-sonnet" }],
      "costPriority": 0.6,
      "speedPriority": 0.7,
      "intelligencePriority": 0.3
    },
    "systemPrompt": "You are a precise text classifier. Output exactly one word.",
    "maxTokens": 16
  }
}
```

关于 `modelPreferences`，有两点值得明确。三个优先级——`costPriority`、`speedPriority`、`intelligencePriority`——是*取值 0–1 的归一化提示*，并非硬性要求；它们告诉宿主如何在成本、延迟与能力之间做取舍。`hints` 则是*建议性的模型名子串*：服务器可以建议 `claude-sonnet`，但宿主可以自由地把它映射到自己实际拥有的模型上（只有 Gemini 访问权限的宿主，可以用一个能力相当的 Gemini 模型来满足 `sonnet` 提示）。服务器表达的是*偏好*；做*决定*的是宿主。

最后这句话正是 Sampling 的核心。宿主保留三个控制点，**绝不**把它们交给服务器：

| 控制点 | 宿主为何保留它 |
|---|---|
| **模型选择** | 宿主清楚自己有哪些模型、成本多少、上下文上限多大；服务器只能给出提示。 |
| **密钥与成本** | 补全消耗的是*宿主*的凭据、记在宿主的账上——服务器永远看不到 API key。 |
| **人工审批闸门** | 宿主应当呈现*服务器在要求模型做什么*，让用户在调用执行前检查、编辑或拒绝——并在结果返回前让用户审阅。 |

因此 Sampling **在设计上就是 human-in-the-loop**，而不是靠约定。推荐的宿主 UX 是一道双向闸门：用户既能看到并批准服务器构造的出站 prompt，也能在模型响应交还给服务器之前看到它。若宿主毫无可见性地把服务器提供的 prompt 直接丢给 LLM，那就是造出了一台 confused-deputy 机器——正是 Lecture 07 在 `roots` 与 `sampling` 的审批闸门语境下剖析的那类失效模式。

**典型用例。** 一个 “meeting-notes” 服务器通过工具调用收到一份原始转录文本，需要生成三条要点摘要。它*不会*内嵌 Anthropic 或 OpenAI 的 key 自己去调用——那会把凭据、计费和模型选择都放进第三方服务器里。相反，它发出一个 `sampling/createMessage`，在 `messages` 中放入转录文本，`costPriority` 偏向便宜，`maxTokens` 设得很紧。宿主在用户自己的模型上、在用户自己的审批下运行它，服务器拿回摘要，却不必拥有这整套机制。

---


<details>
<summary>English original</summary>

**2. Sampling — the server borrows the host's LLM**

**Sampling** (`sampling/createMessage`) lets a server ask the host to run an LLM completion *on the server's behalf*. The server constructs the conversation it wants the model to see; the host runs it against whatever model it controls and returns the completion. This is what lets a server be *agentic* — it can nest LLM reasoning inside a tool call (call a tool → sample to interpret the result → call another tool) without ever holding model credentials.

A sampling request carries `messages`, optional `modelPreferences`, an optional `systemPrompt`, and a `maxTokens` ceiling:

```json
{
  "jsonrpc": "2.0",
  "id": 42,
  "method": "sampling/createMessage",
  "params": {
    "messages": [
      {
        "role": "user",
        "content": {
          "type": "text",
          "text": "Classify the sentiment of this review as positive, negative, or neutral, and return only the single word:\n\n\"Shipping was slow but the product is exactly what I needed.\""
        }
      }
    ],
    "modelPreferences": {
      "hints": [{ "name": "claude-sonnet" }],
      "costPriority": 0.6,
      "speedPriority": 0.7,
      "intelligencePriority": 0.3
    },
    "systemPrompt": "You are a precise text classifier. Output exactly one word.",
    "maxTokens": 16
  }
}
```

Two things about `modelPreferences` are worth pinning down. The three priorities — `costPriority`, `speedPriority`, `intelligencePriority` — are *normalized hints in the range 0–1*, not hard requirements; they tell the host how to trade money against latency against capability. The `hints` are *advisory model-name substrings*: a server can suggest `claude-sonnet`, but the host is free to map that onto whatever it actually has (a host with only Gemini access might satisfy a `sonnet` hint with a comparable Gemini model). The server expresses a *preference*; the host makes the *decision*.

That last sentence is the heart of Sampling. The host keeps three control points and **never** surrenders them to the server:

| Control point | Why the host keeps it |
|---|---|
| **Model selection** | The host knows which models it has, their cost, and their context limits; the server only hints. |
| **Keys & cost** | The completion runs on the *host's* credentials and the host's bill — the server never sees an API key. |
| **Human-approval gate** | The host should surface *what the server is asking the model to do* and let the user inspect, edit, or deny it before the call runs — and let the user review the result before it is returned. |

Sampling is therefore **human-in-the-loop by design**, not by convention. The recommended host UX is a two-sided gate: the user can see and approve the outbound prompt the server constructed, and can see the model's response before it is handed back to the server. A host that fires server-supplied prompts at an LLM with no visibility has built a confused-deputy machine — exactly the failure mode Lecture 07 dissects in the context of approval gates for `roots` and `sampling`.

**Canonical use case.** A "meeting-notes" server receives a raw transcript via a tool call and needs a three-bullet summary. It does *not* embed an Anthropic or OpenAI key and call out itself — that would put credentials, billing, and model choice inside a third-party server. Instead it issues a `sampling/createMessage` with the transcript in `messages`, a `costPriority` leaning cheap, and a tight `maxTokens`. The host runs it on the user's own model under the user's own approval, and the server gets its summary back without owning any of the machinery.

---

</details>

## 3. Roots —— 客户端授予的边界

**Roots**（`roots/list`）是这样一种原语：*客户端*通过它告诉*服务器*，允许其在哪些文件系统路径或 URI 范围内操作。让人栽跟头的思维模型，是把 roots 当成一种*发现*机制（“有意思的文件都在这儿，去找吧”）。并非如此。Roots 是一条**最小权限边界**：客户端授予一个范围，而服务器的契约是*绝不越界操作*。

流程由服务器发起 —— 服务器向客户端询问当前 roots：

```json
{
  "jsonrpc": "2.0",
  "id": 7,
  "method": "roots/list"
}
```

```json
{
  "jsonrpc": "2.0",
  "id": 7,
  "result": {
    "roots": [
      { "uri": "file:///home/dev/project-alpha", "name": "Project Alpha" },
      { "uri": "file:///home/dev/shared/specs",  "name": "Shared specs" }
    ]
  }
}
```

每个 root 是一个 URI（常见的是 `file://` 路径，但规范允许其他 URI scheme），并带有可选的、人类可读的 `name`。服务器应当读取这个列表，把自身的所有文件访问限定在这些子树内，并拒绝任何解析后落在其外的内容 —— 针对上述 roots 提出的读取 `file:///etc/passwd` 的请求，必须由*服务器*拒绝，而不是悄悄尝试、交由 OS 去拒绝。

Roots 不是静态的。当用户打开新文件夹、关闭 workspace，或改变 agent 可触及的范围时，客户端会发出通知，服务器随即重新获取：

```json
{
  "jsonrpc": "2.0",
  "method": "notifications/roots/list_changed"
}
```

正确的服务器会把 `notifications/roots/list_changed` 视为再次调用 `roots/list` 并*重新推导其允许集合*的信号 —— 它绝不能永久缓存最初的 roots。这一安全框架直截了当，并在第 07 讲再次讨论：roots 是客户端*授予*的边界，而在所授予 roots 之外操作的服务器，要么有 bug，要么怀有恶意。把路径逃逸尝试（`../`、指向树外的符号链接、位于 root 之外的绝对路径）当作需要校验并拒绝的东西来处理，就像对待不可信输入那样 —— 因为在跨越信任边界之处，它们正是不可信输入。

---


<details>
<summary>English original</summary>

**3. Roots — the client grants the boundary**

**Roots** (`roots/list`) is the primitive by which the *client* tells the *server* which filesystem paths or URIs it is permitted to operate within. The mental model that trips people up is treating roots as a *discovery* mechanism ("here is where the interesting files are, go find them"). It is not. Roots is a **least-privilege boundary**: the client is granting a scope, and the server's contract is to *never act outside it*.

The flow is server-initiated — the server asks the client for the current roots:

```json
{
  "jsonrpc": "2.0",
  "id": 7,
  "method": "roots/list"
}
```

```json
{
  "jsonrpc": "2.0",
  "id": 7,
  "result": {
    "roots": [
      { "uri": "file:///home/dev/project-alpha", "name": "Project Alpha" },
      { "uri": "file:///home/dev/shared/specs",  "name": "Shared specs" }
    ]
  }
}
```

Each root is a URI (commonly a `file://` path, but the spec allows other URI schemes) with an optional human-readable `name`. The server should read this list, scope all of its file access to those subtrees, and refuse anything that resolves outside them — a request to read `file:///etc/passwd` against the roots above must be rejected by the *server*, not silently attempted and left to the OS to deny.

Roots are not static. When the user opens a new folder, closes a workspace, or changes what the agent may touch, the client emits a notification and the server re-fetches:

```json
{
  "jsonrpc": "2.0",
  "method": "notifications/roots/list_changed"
}
```

A correct server treats `notifications/roots/list_changed` as a signal to call `roots/list` again and *re-derive its allowed set* — it must not cache the original roots forever. The security framing is direct and is revisited in Lecture 07: roots are a boundary the client *grants*, and a server that operates outside its granted roots is either buggy or hostile. Treat path-escape attempts (`../`, symlinks out of the tree, absolute paths outside a root) as something to validate and reject, exactly as you would untrusted input — because across a trust boundary, that is what they are.

---

</details>

## 4. Elicitation — 服务器在任务中途向用户提问

**Elicitation**（`elicitation/create`）让服务器在处理请求的过程中向用户索取*结构化输入*。Sampling 借用宿主的*模型*、Roots 借用宿主的*文件系统边界*，而 Elicitation 借用的是宿主的*人*——它是协议中服务器用来表达“完成之前我还需要从这个人那里拿到一样东西”的标准方式，且服务器自身不拥有任何 UI。

请求用它想要的字段以 **JSON Schema** 描述，这样宿主就能渲染出正确的表单并校验答案：

```json
{
  "jsonrpc": "2.0",
  "id": 19,
  "method": "elicitation/create",
  "params": {
    "message": "Deploying to production. Confirm the target and window before I proceed.",
    "requestedSchema": {
      "type": "object",
      "properties": {
        "environment": {
          "type": "string",
          "enum": ["staging", "production"],
          "description": "Target environment"
        },
        "confirm": {
          "type": "boolean",
          "description": "I understand this is a production deploy"
        }
      },
      "required": ["environment", "confirm"]
    }
  }
}
```

用户填好表单，宿主返回结构化响应。`action` 字段告诉服务器用户是接受了、拒绝了还是忽略了这个提示——服务器必须处理全部三种情况，不能假定答案一定到达：

```json
{
  "jsonrpc": "2.0",
  "id": 19,
  "result": {
    "action": "accept",
    "content": {
      "environment": "production",
      "confirm": true
    }
  }
}
```

Elicitation 的**典型用例**是：agent 始终没有提供的**缺失必填参数**（主动询问而不是直接失败）；**在执行破坏性或不可逆操作之前的确认**（删除、部署、转账）；以及当一个参数匹配到多个对象时的**消歧**（“你指的是仓库 `api` 还是 `api-gateway`？”）。

现在说让 elicitation 变安全的那条规则——**SEP-2260**。服务器**只有在正在处理某个 client 请求时**才可以发出 elicitation 请求。每个 server→client 请求都*必须关联*到一个尚在进行中的 client→server 请求；服务器不能凭空、在两次调用之间、或按后台定时器发出 elicitation。其实际保证是用户**绝不会被凭空提示**：每一次 elicitation 都能追溯到用户或其 agent *刚刚发起*的某样东西。一个背后没有任何动作的提示，几乎按定义就是试图操纵用户，因此规范彻底禁止这种形态。

这现在也是一条*强制*规则，而不是一句客气建议。早先的修订版建议把 server 请求与某个 client 请求关联起来；近期的规范修订则对 elicitation 强制要求这一点——带外 elicitation 不符合规范。（SEP-2260 把这一点推广到广义的 server→client 请求；2026-07-28 RC 随后重做了该关联在无状态内核中究竟*如何*承载——见 §5 以及“Current as of”注记。）设计服务器时，检验标准很简单：如果指不出某次 elicitation 属于哪个 client 请求，就不被允许发送它。

---


<details>
<summary>English original</summary>

**4. Elicitation — the server asks the user, mid-task**

**Elicitation** (`elicitation/create`) lets a server request *structured input from the user* in the middle of handling a request. Where Sampling borrows the host's *model* and Roots borrows the host's *filesystem boundary*, Elicitation borrows the host's *human* — it is the protocol's standard way for a server to say "I need one more thing from the person before I can finish," without the server owning any UI of its own.

The request describes the fields it wants with a **JSON Schema**, so the host can render a proper form and validate the answer:

```json
{
  "jsonrpc": "2.0",
  "id": 19,
  "method": "elicitation/create",
  "params": {
    "message": "Deploying to production. Confirm the target and window before I proceed.",
    "requestedSchema": {
      "type": "object",
      "properties": {
        "environment": {
          "type": "string",
          "enum": ["staging", "production"],
          "description": "Target environment"
        },
        "confirm": {
          "type": "boolean",
          "description": "I understand this is a production deploy"
        }
      },
      "required": ["environment", "confirm"]
    }
  }
}
```

The user fills in the form and the host returns a structured response. The `action` field tells the server whether the user accepted, declined, or dismissed the prompt — and the server must handle all three, not assume an answer arrived:

```json
{
  "jsonrpc": "2.0",
  "id": 19,
  "result": {
    "action": "accept",
    "content": {
      "environment": "production",
      "confirm": true
    }
  }
}
```

**Canonical use cases** for elicitation are: a **missing required parameter** the agent never supplied (ask for it rather than failing); a **confirmation before a destructive or irreversible action** (delete, deploy, send money); and **disambiguation** when an argument matched more than one thing ("did you mean repo `api` or `api-gateway`?").

Now the rule that makes elicitation safe — **SEP-2260**. A server may issue an elicitation request **only while it is actively handling a client request**. Every server→client request must be *associated with* an in-flight client→server request; the server cannot send an elicitation out of nowhere, between calls, or on a background timer. The practical guarantee is that a user is **never prompted out of the blue**: every elicitation traces back to something the user or their agent *just started*. A prompt that appears with no action behind it is, almost by definition, an attempt to manipulate the user, so the spec forbids the shape entirely.

This is also a *required* rule now, not a polite suggestion. Earlier revisions recommended associating server requests with a client request; recent spec revisions made it mandatory for elicitation — out-of-band elicitation is non-conformant. (SEP-2260 generalizes this to server→client requests broadly; the 2026-07-28 RC then reworks exactly *how* that association is carried in a stateless core — see §5 and the "Current as of" note.) When you design a server, the test is simple: if you cannot point at the client request that an elicitation belongs to, you are not allowed to send it.

---

</details>

## 5. 能力门控与优雅降级

三个反向原语都是**能力门控的**。服务器不能假定客户端支持它们 —— 它必须检查客户端在 `initialize` 期间声明的能力并据此适配。能够服务这些能力的客户端会在 `capabilities` 下声明它们：

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "initialize",
  "params": {
    "protocolVersion": "2025-11-25",
    "capabilities": {
      "sampling": {},
      "roots": { "listChanged": true },
      "elicitation": {}
    },
    "clientInfo": { "name": "ExampleHost", "version": "3.1.0" }
  }
}
```

若某项能力**缺失**，规则很明确：服务器**绝不能**发送该请求，并且它**必须优雅降级** —— 绝不能挂起等待客户端永远不会处理的回复，也绝不能硬崩溃。门道在于回退方案，而每个原语都有一个合理的回退方案：

| 缺失的能力 | 错误行为 | 优雅降级 |
|---|---|---|
| **`sampling`** | 内嵌自己的 API key 并直接调用大语言模型 | 返回原始数据，让*宿主*的模型在另一端进行推理 |
| **`roots`** | 假定可以访问整个文件系统 | 回退到已配置/默认的工作目录，或对显式提供给你的内容以只读方式操作 |
| **`elicitation`** | 永远阻塞，或猜测缺失的值 | 快速失败，给出清晰、可操作的错误，并指明缺失的参数 |

在代码中，这一模式每次都是同样的形状 —— 在反向调用之前检查协商好的能力，然后分支：

```text
if client.supports("elicitation"):
    answer = elicitation_create(message="Confirm production deploy?", schema=...)
    if answer.action != "accept":
        return cancel("User declined the deploy.")
else:
    # capability not granted — never elicit; fail with a usable message
    raise ToolError("Missing required argument 'environment'. "
                    "Re-invoke this tool with environment set to 'staging' or 'production'.")
```

优雅降级契约让同一个服务器既能面向功能丰富的宿主（一个能采样、限定 roots 范围并弹出对话框的完整 IDE）运行，*也*能面向最简宿主（一个三种能力都不支持的无头客户端）运行，而无需两套代码路径，也绝不会让请求悬空。反向原语是向宿主请求一项宿主*可能*提供的能力 —— 编写每一个反向原语时都当作“有就用，没有也能工作。”

---

## 截至当前

* **日期：** 2026 年 6 月。
* **规范修订版：** 固定为 **2025-11-25** 稳定修订版 —— 它是采样（`sampling/createMessage`）、Roots（`roots/list`、`notifications/roots/list_changed`）和 Elicitation（`elicitation/create`），以及 `initialize` 中能力门控规则的当前权威来源。
* **SEP-2260**（server→client 请求必须与一个进行中的客户端请求关联 —— 该规则禁止带外 elicitation）已生效，并且在当前修订版中是*强制要求*，而不仅仅是建议。
* **注意：** **2026-07-28 候选发布版**重做了调用中途的 server→client 请求在**无状态核心**中如何流动；这三个原语的*契约*是稳定的，但把服务器请求与其发起的客户端请求关联起来的线路级机制，正是该 RC 重新审视的内容。在依赖这些细节之前，请对照 RC 重新核实请求关联的细节。


<details>
<summary>English original</summary>

**5. Capability gating & graceful degradation**

All three reverse primitives are **capability-gated**. The server cannot assume the client supports them — it must check what the client advertised during `initialize` and adapt. A client that can service these declares them under `capabilities`:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "initialize",
  "params": {
    "protocolVersion": "2025-11-25",
    "capabilities": {
      "sampling": {},
      "roots": { "listChanged": true },
      "elicitation": {}
    },
    "clientInfo": { "name": "ExampleHost", "version": "3.1.0" }
  }
}
```

If a capability is **absent**, the rule is firm: the server **must not** send that request, and it **must degrade gracefully** — never hang waiting for a reply that the client will never service, and never hard-crash. The art is in the fallback, and each primitive has a sensible one:

| Capability missing | Wrong behavior | Graceful degradation |
|---|---|---|
| **`sampling`** | Embed your own API key and call an LLM directly | Return the raw data and let the *host's* model do the reasoning on the other side |
| **`roots`** | Assume access to the whole filesystem | Fall back to a configured/default working directory, or operate read-only on what you were explicitly given |
| **`elicitation`** | Block forever, or guess the missing value | Fail fast with a clear, actionable error naming the missing argument |

In code, the pattern is the same shape every time — check the negotiated capability before the reverse call, and branch:

```text
if client.supports("elicitation"):
    answer = elicitation_create(message="Confirm production deploy?", schema=...)
    if answer.action != "accept":
        return cancel("User declined the deploy.")
else:
    # capability not granted — never elicit; fail with a usable message
    raise ToolError("Missing required argument 'environment'. "
                    "Re-invoke this tool with environment set to 'staging' or 'production'.")
```

The graceful-degradation contract is what lets the same server run against a rich host (a full IDE that can sample, scope roots, and pop dialogs) *and* a minimal one (a headless client that supports none of the three) without two code paths and without ever leaving a request dangling. A reverse primitive is a request to the host for a capability the host *might* offer — write every one of them as "use it if it's there, work without it if it isn't."

---

**Current as of**

* **Date:** June 2026.
* **Spec revision:** pinned to the **2025-11-25** stable revision — the current source of truth for Sampling (`sampling/createMessage`), Roots (`roots/list`, `notifications/roots/list_changed`), and Elicitation (`elicitation/create`), and for the capability-gating rules in `initialize`.
* **SEP-2260** (server→client requests must be associated with an in-flight client request — the rule that forbids out-of-band elicitation) is in force and is *required*, not merely recommended, in the current revision.
* **Watch:** the **2026-07-28 release candidate** reworks how mid-call server→client requests flow in a **stateless core**; the *contract* of these three primitives is stable, but the wire-level mechanics of associating a server request with its originating client request are exactly what that RC revisits. Re-verify the request-association details against the RC before relying on them.

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/MCP for AI Agents/Lecture-04.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/MCP%20for%20AI%20Agents/Lecture-04.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
