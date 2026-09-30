---
title: Lecture 06 - 传输、远程服务器与 OAuth 2.1
description: Lecture 06 - 传输、远程服务器与 OAuth 2.1
---

# Lecture 06 - 传输、远程服务器与 OAuth 2.1

**Collection:** [MCP for AI Agents](README.md) | **Previous:** [← Lecture 05](Lecture-05.md) | **Next:** [Lecture 07](Lecture-07.md)

---

到目前为止，你构建的每个 server 都以本地子进程形式运行：Claude Desktop（或 MCP Inspector）启动你的 Python 进程，通过其 stdin/stdout 传输 JSON-RPC，并隐式信任它，因为它是在*你的*机器上运行的*你的*进程。该模型对单用户在自用笔记本上运行 server 的场景极为合适，也正是 `stdio` 存在的全部理由。但一旦你希望一个 server 支撑多个用户 —— 托管版 GitHub server、全团队 agent 都要访问的内部数据工具、对外暴露 MCP 的 SaaS 产品 —— 子进程模型就崩溃了。没有人会去启动你的进程；他们会从互联网另一头发来 HTTP 请求。

仅仅这一处变化 —— *client 不再负责启动 server* —— 就带来了两个棘手问题。第一是 **transport**：HTTP 是请求/响应式的，而 MCP 是双向、长生命周期的会话，还带有 server 发起的消息（sampling、elicitation、notification），因此需要一种 wire 格式，能在单个 HTTP endpoint 上承载消息流，并把它们关联到某个会话。第二是 **authorization**：一个对外可执行工具、可随意访问的 HTTP endpoint 就是在开门揖盗，所以你必须先弄清楚*谁*在调用，再决定做什么。本讲将严格按规范覆盖这两方面，因为任何一方稍有偏差，都会让 MCP server 被攻破。

贯穿主线：**stdio server 继承本地用户的身份，无需网络认证；远程 HTTP server 必须作为 Resource Server 实现 OAuth 2.1，把身份认证委托给独立的 Authorization Server。** 我们会随文标注规范日期，因为 transport 与 auth 两部分在 2025 年都发生了实质性变化，而一次无状态重写将在 2026 年落地。

---

## 学习目标

学完本讲后，你应能够：

- **比较** `stdio` 与 Streamable HTTP 两种 transport 在本地性、进程归属权、流式、会话、扩展性和 auth 方面的差异 —— 并解释单 endpoint 设计与 `Mcp-Session-Id` header。
- **在 Streamable HTTP 上运行** 一个 FastMCP server，并从原始 HTTP header 中读懂一次真实的会话建立交互。
- **解释** 为什么远程 MCP endpoint 需要授权，并说明为何选择 OAuth 2.1 而非共享 API key。
- **画出** 完整的 MCP 授权流程 —— `401` → RFC 9728 Protected Resource Metadata → Authorization Server discovery → authorization-code + PKCE → `Bearer` token —— 并指出三个角色各自的名称。
- **论证** 通过 Resource Indicators（RFC 8707）实现的受众绑定 token 的必要性，并将其与 [Lecture 07](Lecture-07.md) 中的 confused-deputy 与 token-passthrough 攻击联系起来。
- **在 server 上校验** 传入的 `Bearer` token（issuer、audience、scopes、expiry），并说明为什么 `stdio` server 完全跳过这些校验。

---


<details>
<summary>English original</summary>

**Lecture 06 - Transports, Remote Servers & OAuth 2.1**

**Collection:** [MCP for AI Agents](README.md) | **Previous:** [← Lecture 05](Lecture-05.md) | **Next:** [Lecture 07](Lecture-07.md)

---

Up to now every server you built ran as a local subprocess: Claude Desktop (or MCP Inspector) launched your Python process, piped JSON-RPC over its stdin/stdout, and trusted it implicitly because it was *your* process on *your* machine. That model is perfect for a single user with a server on their own laptop, and it is the entire reason `stdio` exists. But the moment you want one server to back many users — a hosted GitHub server, an internal data tool your whole team's agents reach, a SaaS product that exposes MCP — the subprocess model collapses. Nobody is going to launch your process; they are going to send it an HTTP request from the other side of the internet.

That single change — *the client no longer launches the server* — drags two hard problems in behind it. First, **transport**: HTTP is request/response, but MCP is a bidirectional, long-lived session with server-initiated messages (sampling, elicitation, notifications), so you need a wire format that can carry a stream of messages over one HTTP endpoint and correlate them to a session. Second, **authorization**: an open HTTP endpoint that runs tools is an open invitation, so you need to know *who* is calling before you do anything. This lecture covers both, spec-accurately, because getting either one subtly wrong is how MCP servers get breached.

The throughline: **stdio servers inherit the local user's identity and need no network auth; remote HTTP servers must implement OAuth 2.1 as a Resource Server, delegating identity to a separate Authorization Server.** We will name spec dates as we go, because the transport and auth stories both changed materially in 2025, and a stateless rewrite is landing in 2026.

---

**Learning objectives**

By the end of this lecture you should be able to:

- **Compare** the `stdio` and Streamable HTTP transports across locality, process ownership, streaming, sessions, scaling, and auth — and explain the single-endpoint design and the `Mcp-Session-Id` header.
- **Serve** a FastMCP server over Streamable HTTP and read a real session-establishment exchange in raw HTTP headers.
- **Explain** why a remote MCP endpoint requires authorization and motivate OAuth 2.1 over a shared API key.
- **Diagram** the full MCP authorization flow — `401` → RFC 9728 Protected Resource Metadata → Authorization Server discovery → authorization-code + PKCE → `Bearer` token — naming each of the three roles.
- **Justify** audience-bound tokens via Resource Indicators (RFC 8707), and connect them forward to the confused-deputy and token-passthrough attacks of [Lecture 07](Lecture-07.md).
- **Validate** an incoming `Bearer` token on the server (issuer, audience, scopes, expiry) and state why `stdio` servers skip all of it.

---

</details>

## 1. 传输方式：stdio 与 Streamable HTTP

MCP 将 **协议**（JSON-RPC 2.0 消息、生命周期、原语）与 **传输**（这些字节如何在客户端与服务器之间移动）分开。规范定义了你应当使用的两种传输方式，以及一种绝不能基于其上构建的传输方式。

| 维度 | **stdio** | **Streamable HTTP** |
|---|---|---|
| 位置 | 本地——同一主机 | 远程——跨网络 |
| 谁启动谁 | **客户端启动服务器** 作为子进程 | 服务器独立运行；**客户端连接** 到一个 URL |
| 传输格式 | 通过 stdin/stdout 的 JSON-RPC，**以换行分隔** | 通过 HTTP POST 到 **单个端点** 的 JSON-RPC |
| 服务器→客户端流式传输 | 原生（它不过是另一条管道） | 服务器 **可将响应升级为 SSE** 以承载多条消息 |
| 会话 | 隐式——一个进程就是一个会话 | 显式——`Mcp-Session-Id` HTTP 头 |
| 扩展性 | 每个客户端一个进程；不共享 | 一个服务，多个客户端；可水平扩展 |
| 需要认证 | 否——使用 **本地 / 环境** 凭据 | **是**——OAuth 2.1（下文介绍） |
| 最适合 | 本地单用户（例如 Claude Desktop 启动服务器） | 托管 / 多用户 / SaaS 服务器 |

**单端点设计。** 早期基于 HTTP 的 MCP 使用两个端点（一个用于 POST 请求，一个用于保持打开的 SSE 流以接收响应）。Streamable HTTP 将其压缩为 **一个端点**。客户端向它 POST 一条 JSON-RPC 消息；服务器随后自行选择如何回复：

- 对于简单的请求/响应（例如立即返回的 `tools/call`），它以 **单个 JSON 响应** 回复——`Content-Type: application/json`——交互即结束。
- 对于任何需要回传 *多条* 消息的情况（进度通知、服务器发起的采样/elicitation、流式结果），它 **将同一响应升级为 SSE 流**——`Content-Type: text/event-stream`——并发出 `event:`/`data:` 帧序列，直至完成。

一个 URL，两种可能的响应形态，由服务器逐请求选择。客户端不预先决定是否需要流；它只管 POST，然后读取返回的任何内容类型。

**会话与 `Mcp-Session-Id`。** 由于多个客户端共享一个端点，服务器需要区分各自的对话。在对 `initialize` 请求的响应中，有状态服务器返回一个 **`Mcp-Session-Id`** HTTP 头。客户端随后 **在每一个后续请求中回显该头**。这就是整个会话绑定机制——一个头，在 initialize 时分配一次，此后重复使用。（对比 stdio：其中 OS 进程 *就是* 会话，无需 id。）

**已弃用的传输——不要基于它构建。** 最初的双端点 **HTTP+SSE** 传输自 **规范修订版 2025-03-26 起已被弃用**。它仍出现在较旧的服务器和教程中。提及它只是为了让你能识别并避开它：**不要基于 HTTP+SSE 构建新服务器。** Streamable HTTP 才是受支持的远程传输。

**2026 方向——无状态。** 上述粘性会话模型（服务器持有以 `Mcp-Session-Id` 为键的每会话状态）使水平扩展变得棘手：一个会话的每个请求都必须到达持有其状态的节点，因此需要粘性负载均衡。**2026-07-28 release candidate** 推动 **无状态** 核心，使服务器无需粘性会话即可运行在普通 HTTP 基础设施和无服务器平台上——任何节点都能服务任何请求。把它当作演进方向来跟踪；2025-11-25 稳定版规范才是你当前构建所依据的版本。

---


<details>
<summary>English original</summary>

**1. Transports: stdio vs Streamable HTTP**

MCP separates the **protocol** (JSON-RPC 2.0 messages, the lifecycle, the primitives) from the **transport** (how those bytes move between client and server). The spec defines two transports you should use, and one you must not build on.

| Dimension | **stdio** | **Streamable HTTP** |
|---|---|---|
| Locality | Local — same host | Remote — across a network |
| Who launches whom | **Client launches the server** as a subprocess | Server runs independently; **client connects** to a URL |
| Wire format | JSON-RPC over stdin/stdout, **newline-delimited** | JSON-RPC over HTTP POST to a **single endpoint** |
| Server→client streaming | Native (it's just the other pipe) | Server **may upgrade a response to SSE** for multiple messages |
| Sessions | Implicit — one process is one session | Explicit — `Mcp-Session-Id` HTTP header |
| Scaling | One process per client; not shared | One service, many clients; horizontally scalable |
| Auth needed | No — uses **local / ambient** credentials | **Yes** — OAuth 2.1 (covered below) |
| Best for | Local single-user (e.g. Claude Desktop launching a server) | Hosted / multi-user / SaaS servers |

**The single-endpoint design.** Older HTTP-based MCP used two endpoints (one to POST requests, one to hold open an SSE stream for responses). Streamable HTTP collapses that to **one endpoint**. The client POSTs a JSON-RPC message to it; the server then chooses how to reply:

- For a simple request/response (e.g. `tools/call` that returns immediately), it replies with a **single JSON response** — `Content-Type: application/json` — and the exchange is over.
- For anything that needs to send *multiple* messages back (progress notifications, server-initiated sampling/elicitation, a streamed result), it **upgrades the same response to an SSE stream** — `Content-Type: text/event-stream` — and emits a sequence of `event:`/`data:` frames until done.

One URL, two possible response shapes, chosen per request by the server. The client does not decide in advance whether it wants a stream; it POSTs and reads whatever content type comes back.

**Sessions and `Mcp-Session-Id`.** Because many clients share one endpoint, the server needs to tell their conversations apart. On the response to the `initialize` request, a stateful server returns an **`Mcp-Session-Id`** HTTP header. The client then **echoes that header on every subsequent request**. That is the entire session-binding mechanism — a single header, assigned once at initialize, repeated thereafter. (Contrast stdio, where the OS process *is* the session and no id is needed.)

**The deprecated transport — do not build on it.** The original two-endpoint **HTTP+SSE** transport has been **deprecated since spec revision 2025-03-26**. It still appears in older servers and tutorials. Name it only so you recognize and avoid it: **do not build new servers on HTTP+SSE.** Streamable HTTP is the supported remote transport.

**The 2026 direction — stateless.** The sticky-session model above (a server holds per-session state keyed by `Mcp-Session-Id`) makes horizontal scaling awkward: every request for a session must reach the node that holds its state, so you need sticky load balancing. The **2026-07-28 release candidate** pushes a **stateless** core, so servers can run on ordinary HTTP infrastructure and serverless platforms without sticky sessions — any node can serve any request. Track it as the direction of travel; the 2025-11-25 stable spec is what you build against today.

---

</details>

## 2. 运行远程服务器

FastMCP（官方 Python SDK 的高层服务器，见 [Lecture 05](Lecture-05.md)）只要改一行运行方式，就能说 Streamable HTTP。tool、resource 和 prompt 的定义与你的 stdio 服务器完全相同——变的只有 transport。

最简单的路径是 `mcp.run(transport="streamable-http")`：

```python
from mcp.server.fastmcp import FastMCP

mcp = FastMCP("weather")

@mcp.tool()
def get_forecast(city: str) -> str:
    """Return a short forecast for a city."""
    return f"{city}: clear, 22°C"

if __name__ == "__main__":
    # Serves Streamable HTTP on the default host/port at the /mcp path.
    mcp.run(transport="streamable-http")
```

真正部署时，通常希望把服务器**挂载为 ASGI 应用**，这样才能在生产服务器（Uvicorn/Gunicorn）里、置于反向代理之后运行它，加上中间件，并在它前面放一层授权：

```python
import uvicorn
from mcp.server.fastmcp import FastMCP

mcp = FastMCP("weather")

@mcp.tool()
def get_forecast(city: str) -> str:
    """Return a short forecast for a city."""
    return f"{city}: clear, 22°C"

# The Streamable HTTP endpoint as a mountable ASGI application.
app = mcp.streamable_http_app()

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)
```

**在 HTTP header 中交换 `Mcp-Session-Id`。** 下面是会话建立在实际传输中的样子。客户端把 `initialize` POST 到单一端点，此时还没有会话 id：

```http
POST /mcp HTTP/1.1
Host: weather.example.com
Content-Type: application/json
Accept: application/json, text/event-stream

{"jsonrpc":"2.0","id":1,"method":"initialize","params":{ ... }}
```

服务器创建会话，并在响应 header 中返回其 id：

```http
HTTP/1.1 200 OK
Content-Type: application/json
Mcp-Session-Id: 1868a90c-7f3b-4e2a-9d11-5c0e2f8a4b6d

{"jsonrpc":"2.0","id":1,"result":{ ... }}
```

从此以后，在会话存续期间，客户端**在每个请求上都回传该 header**：

```http
POST /mcp HTTP/1.1
Host: weather.example.com
Content-Type: application/json
Accept: application/json, text/event-stream
Mcp-Session-Id: 1868a90c-7f3b-4e2a-9d11-5c0e2f8a4b6d

{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"get_forecast","arguments":{"city":"Lisbon"}}}
```

注意每个请求上的 `Accept` header 会**同时**列出 `application/json` 和 `text/event-stream`——客户端在告诉服务器「单个 JSON 响应或一条 SSE 流我都能接受」，由服务器按请求选择（第 1 节）。这就是单一端点设计的客户端侧。

---

## 3. 远程为什么需要认证

`stdio` 服务器只对一方可达：启动它的那个用户。它的安全边界就是操作系统。没有什么需要认证，因为在允许你启动进程时，OS 已经确定了你是谁——服务器以用户**环境中现有的本地凭据**运行，并信任它们。

远程 Streamable HTTP 服务器没有这样的边界。一旦把它绑定到公网地址，*任何能访问到该 URL 的人都能向它 POST。* 而 MCP 服务器不是只读的数据源——它们暴露 **tool**，而 tool 会**做事**：查询数据库、代表调用方访问下游 API、发送消息、转移资金。未认证的 MCP 端点就是一处披着 JSON-RPC 外衣的远程代码执行面。

最朴素的做法——把共享 API key 放在环境变量里，每个请求都检查一下——在多用户服务器上会失败，失败的方式和共享密钥一贯的失败方式一样：

- **没有按用户的身份。** 每个调用方都是同一个匿名的持钥者，于是服务器无法限定每个用户能做什么，无法审计谁做了什么，也无法在不给所有人换 key 的情况下吊销某一个用户。
- **服务器最终持有所有人的密钥。** 如果服务器需要按用户去访问下游服务，单个共享 key 就意味着它以*所有*用户的身份对该服务行事——这是一个迟早会发生的 confused deputy 隐患（第 5 节，以及 [Lecture 07](Lecture-07.md)）。
- **分发与轮换无解。** 把长期有效的密钥安全地送进每一个客户端，并在泄露后轮换它，正是 OAuth 生来要解决掉的问题。

所以 MCP 不自己发明认证。对于 HTTP transport，它采用 **OAuth 2.1**，即 OAuth 现代、加固过安全的 profile——短生命周期 bearer token、强制 PKCE、没有 implicit grant——并给每一方分配标准的 OAuth 角色。（`stdio` 服务器有自己的本地边界，**跳过这一切**——见第 6 节。）

---


<details>
<summary>English original</summary>

**2. Running a remote server**

FastMCP (the official Python SDK's high-level server, covered in [Lecture 05](Lecture-05.md)) speaks Streamable HTTP with a one-line change to how you run it. The tool, resource, and prompt definitions are identical to your stdio server — only the transport changes.

The simplest path is `mcp.run(transport="streamable-http")`:

```python
from mcp.server.fastmcp import FastMCP

mcp = FastMCP("weather")

@mcp.tool()
def get_forecast(city: str) -> str:
    """Return a short forecast for a city."""
    return f"{city}: clear, 22°C"

if __name__ == "__main__":
    # Serves Streamable HTTP on the default host/port at the /mcp path.
    mcp.run(transport="streamable-http")
```

For real deployments you usually want the server **mounted as an ASGI app** so you can run it under a production server (Uvicorn/Gunicorn) behind a reverse proxy, add middleware, and put authorization in front of it:

```python
import uvicorn
from mcp.server.fastmcp import FastMCP

mcp = FastMCP("weather")

@mcp.tool()
def get_forecast(city: str) -> str:
    """Return a short forecast for a city."""
    return f"{city}: clear, 22°C"

# The Streamable HTTP endpoint as a mountable ASGI application.
app = mcp.streamable_http_app()

if __name__ == "__main__":
    uvicorn.run(app, host="0.0.0.0", port=8000)
```

**The `Mcp-Session-Id` exchange in HTTP headers.** Here is what session establishment looks like on the wire. The client POSTs `initialize` to the single endpoint with no session id yet:

```http
POST /mcp HTTP/1.1
Host: weather.example.com
Content-Type: application/json
Accept: application/json, text/event-stream

{"jsonrpc":"2.0","id":1,"method":"initialize","params":{ ... }}
```

The server creates a session and returns its id in the response header:

```http
HTTP/1.1 200 OK
Content-Type: application/json
Mcp-Session-Id: 1868a90c-7f3b-4e2a-9d11-5c0e2f8a4b6d

{"jsonrpc":"2.0","id":1,"result":{ ... }}
```

From now on, the client **echoes that header on every request** for the life of the session:

```http
POST /mcp HTTP/1.1
Host: weather.example.com
Content-Type: application/json
Accept: application/json, text/event-stream
Mcp-Session-Id: 1868a90c-7f3b-4e2a-9d11-5c0e2f8a4b6d

{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"get_forecast","arguments":{"city":"Lisbon"}}}
```

Note the `Accept` header on every request lists **both** `application/json` and `text/event-stream` — the client is telling the server "I can take either a single JSON response or an SSE stream," and the server picks per request (Section 1). That is the client side of the single-endpoint design.

---

**3. Why remote needs auth**

A `stdio` server is reachable by exactly one party: the user who launched it. Its security boundary is the operating system. There is nothing to authenticate, because the OS already decided who you are when it let you start the process — the server runs with the user's **ambient, local credentials** and trusts them.

A remote Streamable HTTP server has no such boundary. The instant you bind it to a public address, *anyone who can reach the URL can POST to it.* And MCP servers are not read-only data feeds — they expose **tools**, which **do things**: query a database, hit a downstream API on the caller's behalf, send a message, move money. An unauthenticated MCP endpoint is a remote-code-execution surface wearing a JSON-RPC costume.

The naive fix — a shared API key in an environment variable, checked on each request — fails for a multi-user server in the ways shared secrets always fail:

- **No per-user identity.** Every caller is the same anonymous key-bearer, so the server cannot scope what each user may do, cannot audit who did what, and cannot revoke one user without rotating the key for everyone.
- **The server ends up holding everyone's secret.** If the server needs to act on a downstream service per user, a single shared key means it impersonates *all* users to that service — a confused-deputy waiting to happen (Section 5, and [Lecture 07](Lecture-07.md)).
- **Distribution and rotation are unsolved.** Getting a long-lived secret safely into every client, and rotating it after a leak, is exactly the problem OAuth was built to retire.

So MCP does not invent its own auth. For the HTTP transport it adopts **OAuth 2.1**, the modern, security-hardened profile of OAuth — short-lived bearer tokens, mandatory PKCE, no implicit grant — and assigns each party a standard OAuth role. (`stdio` servers, with their local boundary, **skip all of this** — see Section 6.)

---

</details>

## 4. MCP 中的 OAuth 2.1 模型

MCP 授权（仅限 HTTP transport）完全按标准 OAuth 角色来定义。关键架构决策——落在 **2025-06 spec revision** 中——是：MCP server **不是**给用户登录的东西。它只*校验* token。签发 token 的是另一个独立的身份提供方。

三个角色：

| Role | Who plays it | OAuth role | Responsibility |
|---|---|---|---|
| **MCP client** | host/agent（Claude、某个 agent runtime） | **OAuth 2.1 client** | 运行 authorization-code + PKCE 流程；获取并发送 token |
| **MCP server** | 你的 Streamable HTTP server | **OAuth 2.0 Resource Server** | 对外声明*从哪里*获取 token；在每个请求上校验 `Bearer` token |
| **Authorization Server (IdP)** | 一个**独立的**服务（Auth0、Entra、Keycloak……） | **Authorization Server** | 认证用户并**签发** access token |

MCP server 的职责缩小为两件事：**告诉客户端它的 Authorization Server 在哪**，以及**校验传入的 token**。它永远看不到用户的密码，也永远不签发 token。正是这种分离使得 MCP 授权可组合——你可以用你所在组织已在使用的任意 IdP 来挡在 server 前面。

**声明 Authorization Server —— RFC 9728。** 客户端到达你的 server 时，并不知道该用哪个 IdP。MCP server **MUST** 实现 **RFC 9728 Protected Resource Metadata (PRM)**——一份小型 JSON 文档，列出为该资源签发 token 的 Authorization Server，托管在一个 well-known URI 上。

**完整的发现 + 授权流程。** 把三个角色合在一起，下面是客户端首次连接到受保护 server 时的端到端时序：


<details>
<summary>English original</summary>

**4. The OAuth 2.1 model in MCP**

MCP authorization (HTTP transport only) is defined entirely in terms of standard OAuth roles. The key architectural decision — which landed in the **2025-06 spec revision** — is that the MCP server is **not** the thing that logs users in. It only *checks* tokens. A separate identity provider issues them.

The three roles:

| Role | Who plays it | OAuth role | Responsibility |
|---|---|---|---|
| **MCP client** | The host/agent (Claude, an agent runtime) | **OAuth 2.1 client** | Runs the authorization-code + PKCE flow; obtains and sends the token |
| **MCP server** | Your Streamable HTTP server | **OAuth 2.0 Resource Server** | Advertises *where* to get tokens; validates the `Bearer` token on every request |
| **Authorization Server (IdP)** | A **separate** service (Auth0, Entra, Keycloak, …) | **Authorization Server** | Authenticates the user and **issues** access tokens |

The MCP server's job shrinks to two things: **tell clients where its Authorization Server lives**, and **validate incoming tokens**. It never sees the user's password and never mints a token. That separation is what makes MCP auth composable — you front your server with whatever IdP your org already uses.

**Advertising the Authorization Server — RFC 9728.** A client arriving at your server does not know which IdP to use. The MCP server **MUST** implement **RFC 9728 Protected Resource Metadata (PRM)** — a small JSON document that names the Authorization Server(s) that issue tokens for this resource, served at a well-known URI.

**The full discovery + authorization flow.** Putting the three roles together, here is the end-to-end sequence the first time a client connects to a protected server:

</details>

```text
 MCP client (OAuth 2.1)        MCP server (Resource Server)      Authorization Server (IdP)
        │                               │                                  │
        │ 1. POST /mcp (no token)       │                                  │
        │──────────────────────────────▶                                  │
        │                               │                                  │
        │ 2. 401 Unauthorized           │                                  │
        │    WWW-Authenticate: ...      │                                  │
        │      resource_metadata=...    │                                  │
        │◀──────────────────────────────                                  │
        │                               │                                  │
        │ 3. GET Protected Resource Metadata (RFC 9728)                    │
        │──────────────────────────────▶                                  │
        │    { authorization_servers: [ AS ] }                            │
        │◀──────────────────────────────                                  │
        │                               │                                  │
        │ 4. GET AS metadata, then run OAuth 2.1 authorization-code        │
        │    + PKCE  (PKCE REQUIRED)     │                                  │
        │─────────────────────────────────────────────────────────────────▶
        │    user authenticates / consents; client exchanges code          │
        │◀─────────────────────────────────────────────────────────────────
        │    access token (audience-bound to THIS server — RFC 8707)       │
        │                               │                                  │
        │ 5. POST /mcp                  │                                  │
        │    Authorization: Bearer <tok>│                                  │
        │──────────────────────────────▶                                  │
        │    server validates token, serves the request                    │
        │◀──────────────────────────────                                  │
```

逐步来看：客户端不带 token 访问服务器（1），得到一个 **`401 Unauthorized`**，其中携带 **`WWW-Authenticate`** 头，指向 resource-metadata 文档（2）。客户端获取该 **Protected Resource Metadata**（3），读出应使用哪个 Authorization Server，并针对 IdP 执行 **OAuth 2.1 authorization-code flow with PKCE**（4）——**PKCE 是必需的**，不是可选项。它带着一个 access token 返回，该 token 的 audience 绑定到*这个*服务器，最后用该 token 作为 **`Bearer`** 凭据重试最初的请求（5）。

**`WWW-Authenticate` 头（第 2 步）。** `401` 通过指向元数据来告诉客户端*如何*认证：

```http
HTTP/1.1 401 Unauthorized
WWW-Authenticate: Bearer resource_metadata="https://weather.example.com/.well-known/oauth-protected-resource"
```

**Protected Resource Metadata 文档（第 3 步）。** 客户端 GET 该 URL，并至少读出哪些 Authorization Server 为该资源签发 token：

```json
{
  "resource": "https://weather.example.com/mcp",
  "authorization_servers": [
    "https://login.example-idp.com"
  ],
  "bearer_methods_supported": ["header"],
  "scopes_supported": ["mcp:tools", "mcp:resources"]
}
```

`authorization_servers` 数组是承重字段：一个除了你的 URL 之外一无所知的客户端，正是靠它发现 IdP 并引导整个流程。此后客户端获取 Authorization Server *自己的*元数据（标准 OAuth/OIDC discovery），以找到其 authorization 和 token 端点，并执行 code+PKCE 交换。

---


<details>
<summary>English original</summary>

Step by step: the client hits the server with no token (1) and gets a **`401 Unauthorized`** carrying a **`WWW-Authenticate`** header that points at the resource-metadata document (2). The client fetches that **Protected Resource Metadata** (3), reads which Authorization Server to use, and runs the **OAuth 2.1 authorization-code flow with PKCE** against the IdP (4) — **PKCE is required**, not optional. It comes back with an access token whose audience is bound to *this* server, and finally retries the original request with that token as a **`Bearer`** credential (5).

**The `WWW-Authenticate` header (step 2).** The `401` tells the client *how* to authenticate by pointing at the metadata:

```http
HTTP/1.1 401 Unauthorized
WWW-Authenticate: Bearer resource_metadata="https://weather.example.com/.well-known/oauth-protected-resource"
```

**The Protected Resource Metadata document (step 3).** The client GETs that URL and reads, at minimum, which Authorization Server(s) issue tokens for this resource:

```json
{
  "resource": "https://weather.example.com/mcp",
  "authorization_servers": [
    "https://login.example-idp.com"
  ],
  "bearer_methods_supported": ["header"],
  "scopes_supported": ["mcp:tools", "mcp:resources"]
}
```

The `authorization_servers` array is the load-bearing field: it is how a client that knows nothing but your URL discovers the IdP and bootstraps the whole flow. From there the client fetches the Authorization Server's *own* metadata (standard OAuth/OIDC discovery) to find its authorization and token endpoints, and runs the code+PKCE exchange.

---

</details>

## 5. Resource Indicators 与 audience 绑定（RFC 8707）

上文第 4 步以“一个 audience 绑定到 *此* 服务器的 access token”收尾。这个短语承担着关键的安全作用，其背后的机制是 **Resource Indicators for OAuth 2.0 — RFC 8707**。

当客户端向授权服务器请求 token 时，会包含一个 `resource` 参数，用于指名它打算调用的特定 MCP 服务器。授权服务器随后会将该身份写入 token 的 **audience**（`aud`）。结果就是一个仅 *对此单个服务器* 有效的 token——它实际上表示：“持有者被授权调用 `https://weather.example.com/mcp`，且不能调用其他任何地方。”

为什么这很重要：没有 audience 绑定，token 就是通用的 bearer 凭证——任何收到它的服务器都可能针对 *不同的* 下游服务器 **重放它**，而该下游服务器接受同一授权服务器的 token，从而冒充用户。这就是 **confused-deputy / token-passthrough** 问题：一个服务器将它收到的 token 转发给其他某个 API，而该 API 看到一个看似有效的 token 后，便依据它执行操作。audience 绑定关上了这扇门：正确验证的下游服务器会拒绝任何 `aud` 不是自身的 token，因此被透传的 token 一到就失效。

这是 MCP 服务器必须对每个 token **验证 audience**（第 6 节），并且 **绝不** 将它收到的 token 转发给第三方的唯一最重要的原因。攻击本身——token 透传与 confused deputy——会在 [Lecture 07](Lecture-07.md) 中深入讨论；现在，请记住这条规则：**token 绑定到你的服务器，你的服务器检查该绑定，且你的服务器不将 token 继续传递下去。**

---


<details>
<summary>English original</summary>

**5. Resource Indicators & audience binding (RFC 8707)**

Step 4 above ended with "an access token whose audience is bound to *this* server." That phrase is doing critical security work, and the mechanism behind it is **Resource Indicators for OAuth 2.0 — RFC 8707**.

When the client asks the Authorization Server for a token, it includes a `resource` parameter naming the specific MCP server it intends to call. The Authorization Server then stamps that identity into the token's **audience** (`aud`). The result is a token that is only valid *for this one server* — it says, in effect, "the bearer is authorized to call `https://weather.example.com/mcp`, and nowhere else."

Why this matters: without audience binding, a token is a generic bearer credential — any server that receives it could **replay it** against a *different* downstream server that accepts the same Authorization Server's tokens, impersonating the user. That is the **confused-deputy / token-passthrough** problem: a server forwards a token it received to some other API, and that API, seeing a valid-looking token, acts on it. Audience binding shuts the door: a correctly validating downstream server rejects any token whose `aud` is not itself, so a passed-through token is dead on arrival.

This is the single most important reason an MCP server must **validate the audience** of every token (Section 6) and must **never** forward a token it received to a third party. We treat the attack itself — token passthrough and the confused deputy — in depth in [Lecture 07](Lecture-07.md); for now, hold onto the rule: **the token is bound to your server, your server checks that binding, and your server does not pass tokens onward.**

---

</details>

## 6. 实战：用真实 IdP 前置服务器

不要自己实现授权服务器。部署一个真实的 IdP——**Auth0**、**Microsoft Entra ID** 或 **Keycloak** 是常见选择——在其中把你的 MCP server 注册为受保护资源 / API，让它负责认证、同意、token 签发与刷新。你的 MCP server 的全部认证职责就是第 4 节里的两件事：**提供受保护资源元数据**（让客户端发现 IdP），以及**在每个请求上校验 `Bearer` token。**

校验不是「这个 header 非空就行」。正确的检查会在每个请求上验证四件事：

1. **签名与签发者** — token 由你信任的授权服务器签名（用其公开的 JWKS 验证），且其 `iss` 与该 IdP 匹配。
2. **受众** — `aud` 是*本*服务器（第 5 节中的 RFC 8707 绑定）。正是这项检查挡住了 token 透传。
3. **作用域** — token 携带所请求操作需要的作用域（例如调用工具所需的 `mcp:tools`）。
4. **过期时间** — `exp` 在未来（且 `nbf`/`iat` 合理）。

一个 token 校验骨架，作为中间件跑在第 2 节的 Streamable HTTP 应用前面：

```python
import time
import jwt  # PyJWT
from jwt import PyJWKClient

ISSUER = "https://login.example-idp.com/"
AUDIENCE = "https://weather.example.com/mcp"   # MUST equal this server's identity
JWKS_URL = "https://login.example-idp.com/.well-known/jwks.json"

_jwks = PyJWKClient(JWKS_URL)   # fetches & caches the IdP's signing keys


def validate_bearer(auth_header: str | None) -> dict:
    """Validate an incoming Bearer token. Raises on any failure → caller returns 401/403."""
    if not auth_header or not auth_header.startswith("Bearer "):
        raise PermissionError("missing bearer token")          # → 401 + WWW-Authenticate

    token = auth_header.removeprefix("Bearer ").strip()
    signing_key = _jwks.get_signing_key_from_jwt(token).key

    # Signature + issuer + audience + expiry are all enforced here.
    claims = jwt.decode(
        token,
        signing_key,
        algorithms=["RS256"],
        issuer=ISSUER,            # checks iss
        audience=AUDIENCE,        # checks aud — defeats token passthrough (RFC 8707)
        options={"require": ["exp", "iss", "aud"]},
    )

    # Expiry is verified by jwt.decode; this is a defensive belt-and-suspenders check.
    if claims["exp"] <= time.time():
        raise PermissionError("token expired")

    # Scope check — gate the operation on the granted scopes.
    granted = set(claims.get("scope", "").split())
    if "mcp:tools" not in granted:
        raise PermissionError("insufficient scope")            # → 403

    return claims   # identity + scopes for per-user authorization / audit
```

token 缺失或格式错误时，返回 `401 Unauthorized`，**并在 `WWW-Authenticate` header 中指向你的资源元数据**（第 4 节），让客户端知道如何恢复并启动流程。token 有效但作用域不足时，返回 `403 Forbidden`。用返回的 `claims`（subject、scopes）做按用户授权与审计——这是共享 API key 永远给不了的按用户身份。

**stdio server 可以跳过这一切。** 第 3–6 节的所有内容只适用于 **HTTP transport**。`stdio` server 没有 `WWW-Authenticate`，没有 PRM，也没有 token 校验，因为它没有网络边界，也没有远端调用方——它作为子进程运行在本地用户下，并使用该用户的**本地 / 环境凭证**（即启动进程已有的那些：env vars、本地配置、OS keychain）。给 stdio server 硬套 OAuth，就像给一扇只有主人能开的门查票。认证是走向远端的代价——这正是你要刻意做出 stdio 与 HTTP 之选（第 1 节）的原因。

---

## 时效截至

2026 年 6 月 —— 锚定 **2025-11-25** 稳定版 MCP 规范修订。涉及的关键日期：旧的双端点 **HTTP+SSE** transport **自 2025-03-26 起已废弃**（不要在其上构建）；**Resource Server / Authorization Server 拆分**以及 OAuth 2.1 + RFC 9728 + RFC 8707 授权模型在 **2025-06** 修订中落地。**2026-07-28 release candidate** 将核心推向 **stateless** 设计，使 Streamable HTTP server 能在普通 HTTP 与 serverless 基础设施上扩展，而无需 sticky session。SDK 层面的接口（FastMCP `run(transport=...)` / ASGI 挂载）与 IdP 集成细节都当作快照看待，并以你实际安装的 `mcp` 包版本和你授权服务器的当前元数据为准。


<details>
<summary>English original</summary>

**6. Practical: front the server with a real IdP**

You do not implement an Authorization Server. You stand up a real IdP — **Auth0**, **Microsoft Entra ID**, or **Keycloak** are the common choices — register your MCP server as a protected resource / API in it, and let it do authentication, consent, token issuance, and refresh. Your MCP server's entire auth responsibility is two things from Section 4: **serve the Protected Resource Metadata** (so clients discover the IdP), and **validate the `Bearer` token on every request.**

Validation is not "is this header non-empty." A correct check verifies four things on every request:

1. **Signature & issuer** — the token is signed by the Authorization Server you trust (verify against its published JWKS), and its `iss` matches that IdP.
2. **Audience** — `aud` is *this* server (the RFC 8707 binding from Section 5). This is the check that defeats token passthrough.
3. **Scopes** — the token carries the scope the requested operation requires (e.g. `mcp:tools` to call a tool).
4. **Expiry** — `exp` is in the future (and `nbf`/`iat` are sane).

A token-validation skeleton you would run as middleware in front of the Streamable HTTP app from Section 2:

```python
import time
import jwt  # PyJWT
from jwt import PyJWKClient

ISSUER = "https://login.example-idp.com/"
AUDIENCE = "https://weather.example.com/mcp"   # MUST equal this server's identity
JWKS_URL = "https://login.example-idp.com/.well-known/jwks.json"

_jwks = PyJWKClient(JWKS_URL)   # fetches & caches the IdP's signing keys


def validate_bearer(auth_header: str | None) -> dict:
    """Validate an incoming Bearer token. Raises on any failure → caller returns 401/403."""
    if not auth_header or not auth_header.startswith("Bearer "):
        raise PermissionError("missing bearer token")          # → 401 + WWW-Authenticate

    token = auth_header.removeprefix("Bearer ").strip()
    signing_key = _jwks.get_signing_key_from_jwt(token).key

    # Signature + issuer + audience + expiry are all enforced here.
    claims = jwt.decode(
        token,
        signing_key,
        algorithms=["RS256"],
        issuer=ISSUER,            # checks iss
        audience=AUDIENCE,        # checks aud — defeats token passthrough (RFC 8707)
        options={"require": ["exp", "iss", "aud"]},
    )

    # Expiry is verified by jwt.decode; this is a defensive belt-and-suspenders check.
    if claims["exp"] <= time.time():
        raise PermissionError("token expired")

    # Scope check — gate the operation on the granted scopes.
    granted = set(claims.get("scope", "").split())
    if "mcp:tools" not in granted:
        raise PermissionError("insufficient scope")            # → 403

    return claims   # identity + scopes for per-user authorization / audit
```

On a missing or malformed token, respond `401 Unauthorized` **with the `WWW-Authenticate` header pointing at your resource metadata** (Section 4) so the client knows how to recover and start the flow. On a valid token with insufficient scope, respond `403 Forbidden`. Use the returned `claims` (subject, scopes) for per-user authorization and audit — this is the per-user identity a shared API key could never give you.

**stdio servers skip all of this.** Everything in Sections 3–6 applies to the **HTTP transport only**. A `stdio` server has no `WWW-Authenticate`, no PRM, no token validation, because it has no network boundary and no remote callers — it runs as a subprocess under the local user and uses that user's **local / ambient credentials** (whatever the launching process already has: env vars, local config, the OS keychain). Bolting OAuth onto a stdio server would be checking a ticket for a door that only its owner can open. Auth is the cost of going remote — which is exactly why you make the stdio-vs-HTTP decision (Section 1) deliberately.

---

**Current as of**

June 2026 — pinned to the **2025-11-25** stable MCP specification revision. Key dates referenced: the legacy two-endpoint **HTTP+SSE** transport has been **deprecated since 2025-03-26** (do not build on it); the **Resource Server / Authorization Server split** and the OAuth 2.1 + RFC 9728 + RFC 8707 authorization model landed in the **2025-06** revision. The **2026-07-28 release candidate** moves the core toward a **stateless** design so Streamable HTTP servers scale on ordinary HTTP and serverless infrastructure without sticky sessions. Treat SDK surfaces (FastMCP `run(transport=...)` / ASGI mounting) and IdP integration details as snapshots and verify against your installed `mcp` package version and your Authorization Server's current metadata.

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/MCP for AI Agents/Lecture-06.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/MCP%20for%20AI%20Agents/Lecture-06.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
