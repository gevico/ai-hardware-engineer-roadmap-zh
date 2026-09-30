---
title: 第 39 讲 - OpenClaw 案例研究：网关 RPC 协议
description: 第 39 讲 - OpenClaw 案例研究：网关 RPC 协议
---

# 第 39 讲 - OpenClaw 案例研究：网关 RPC 协议

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [第 38 讲](Lecture-38.md) | **下一讲：** [第 40 讲](Lecture-40.md)

---

第 38 讲从外部视角讲解了 App SDK。

本讲再向下深入一层：

```text
App SDK / CLI / UI / node
  -> Gateway WebSocket RPC
  -> OpenClaw control plane
```

网关 RPC 是**稳定的协议边界**，让客户端、SDK、自动化工具与节点能够与 OpenClaw 通信，而不必抓取私有的 runtime 内部细节。

核心思想：

> 生产级 agent 系统需要的是一个有类型、经过认证、按 scope 门控的控制平面，而不是随机的临时 HTTP 端点和终端输出解析

---

## 学习目标

学完本讲后，你应当能够：

1. 解释网关 RPC 的帧模型：`req`、`res` 和 `event`。
2. 描述 connect 握手与 `hello-ok` 响应。
3. 理解角色、scope 与方法级访问控制。
4. 解释设备配对、设备 token 与节点认证。
5. 理解为什么特性发现位于 `hello-ok.features`。
6. 解释广播的 scope 划分与按客户端的事件顺序。
7. 描述为什么有副作用的方法需要幂等键。
8. 理解共享密钥认证、可信代理认证、私有入口模式与设备 token 重连行为。
9. 设计一个新的 RPC 方法，既不泄漏密钥，也不绕过策略。

---

## 1. 网关 RPC 是什么

网关 RPC 是**基于 WebSocket 的控制平面**。

使用方包括：

- CLI 客户端
- 桌面应用
- Web UI
- 自动化工具
- App SDK 客户端
- 伴随节点
- 无头节点宿主

它承载：

- 请求/响应 RPC 调用
- agent 与工具事件
- 节点传输帧
- presence 更新
- 配对状态
- 诊断信息
- 管理操作

简单来说：

```text
Gateway RPC = OpenClaw's command bus
```

它不只是一条聊天流。

它是**整个 runtime 的控制平面**。

---

## 2. 帧模型

网关 RPC 使用承载 JSON 的 WebSocket 文本帧。

有三种核心帧形态。

### 请求

```json
{
  "type": "req",
  "id": "req-123",
  "method": "agents.list",
  "params": {}
}
```

### 响应

```json
{
  "type": "res",
  "id": "req-123",
  "ok": true,
  "payload": {
    "agents": []
  }
}
```

错误响应：

```json
{
  "type": "res",
  "id": "req-123",
  "ok": false,
  "error": {
    "type": "FORBIDDEN",
    "message": "Missing required scope"
  }
}
```

### 事件

```json
{
  "type": "event",
  "event": "agent.delta",
  "payload": {},
  "seq": 42,
  "stateVersion": 7
}
```

心智模型：

```text
req/res = ask the Gateway to do or return something
event   = Gateway tells you something happened
```

---

## 3. 为什么用 WebSocket 而非纯 REST

REST 很适合**简单的请求/响应 API**。

agent 系统需要的更多：

- 实时的 assistant 增量
- 工具进度
- 审批请求
- 节点 presence
- 配对事件
- 流生命周期事件
- 重连行为
- 按客户端的事件过滤

WebSocket 为 OpenClaw 提供了一条**长生命周期的双向通道**：

```text
client -> Gateway: requests
Gateway -> client: responses and events
node -> Gateway: capabilities and command results
Gateway -> node: node.invoke commands
```

这就是网关能够同时服务以下两者的原因：

- 操作员客户端
- 节点传输

二者都在同一个协议族上。

---

## 4. connect 握手

第一个帧必须是 **connect 请求**。

示例形态：

```json
{
  "type": "req",
  "id": "connect-1",
  "method": "connect",
  "params": {
    "minProtocol": 3,
    "maxProtocol": 3,
    "client": {
      "id": "cli",
      "version": "1.2.3",
      "platform": "macos",
      "mode": "operator"
    },
    "role": "operator",
    "scopes": ["operator.read", "operator.write"],
    "auth": {
      "token": "..."
    },
    "device": {
      "id": "device_fp",
      "publicKey": "...",
      "signature": "...",
      "signedAt": 1737264000,
      "nonce": "..."
    }
  }
}
```

关键部分：

- 协议版本范围
- 客户端元数据
- 请求的角色
- 请求的 scope
- 认证材料
- 设备身份
- 签名 nonce

**签名的 nonce** 很关键，因为服务器需要确认客户端确实掌握设备密钥。

如果没有 nonce 签名，被复制的设备 ID 会**太容易被伪造**。

---


<details>
<summary>English original</summary>

**Lecture 39 - OpenClaw Case Study: Gateway RPC Protocol**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 38](Lecture-38.md) | **Next:** [Lecture 40](Lecture-40.md)

---

Lecture 38 explained the App SDK from the outside.

This lecture goes one layer lower:

```text
App SDK / CLI / UI / node
  -> Gateway WebSocket RPC
  -> OpenClaw control plane
```

The Gateway RPC is the **stable protocol boundary** that lets clients, SDKs, automation, and nodes talk to OpenClaw without scraping private runtime internals.

The core idea:

> a production agent system needs a typed, authenticated, scope-gated control plane, not random ad-hoc HTTP endpoints and terminal output parsing

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Explain the Gateway RPC frame model: `req`, `res`, and `event`.
2. Describe the connect handshake and `hello-ok` response.
3. Understand roles, scopes, and method-level access control.
4. Explain device pairing, device tokens, and node authentication.
5. Understand why feature discovery lives in `hello-ok.features`.
6. Explain broadcast scoping and per-client event ordering.
7. Describe why side-effecting methods need idempotency keys.
8. Understand shared-secret auth, trusted-proxy auth, private-ingress mode, and device-token reconnect behavior.
9. Design a new RPC method without leaking secrets or bypassing policy.

---

**1. What the Gateway RPC is**

The Gateway RPC is a **WebSocket-based control plane**.

It is used by:

- CLI clients
- desktop apps
- web UIs
- automation tools
- App SDK clients
- companion nodes
- headless node hosts

It carries:

- request/response RPC calls
- agent and tool events
- node transport frames
- presence updates
- pairing state
- diagnostics
- admin operations

In simple terms:

```text
Gateway RPC = OpenClaw's command bus
```

It is not just a chat stream.

It is the **control plane for the whole runtime**.

---

**2. The frame model**

Gateway RPC uses WebSocket text frames containing JSON.

There are three core frame shapes.

**Request**

```json
{
  "type": "req",
  "id": "req-123",
  "method": "agents.list",
  "params": {}
}
```

**Response**

```json
{
  "type": "res",
  "id": "req-123",
  "ok": true,
  "payload": {
    "agents": []
  }
}
```

Error response:

```json
{
  "type": "res",
  "id": "req-123",
  "ok": false,
  "error": {
    "type": "FORBIDDEN",
    "message": "Missing required scope"
  }
}
```

**Event**

```json
{
  "type": "event",
  "event": "agent.delta",
  "payload": {},
  "seq": 42,
  "stateVersion": 7
}
```

Mental model:

```text
req/res = ask the Gateway to do or return something
event   = Gateway tells you something happened
```

---

**3. Why WebSocket instead of plain REST**

REST works well for **simple request/response APIs**.

Agent systems need more:

- live assistant deltas
- tool progress
- approval requests
- node presence
- pairing events
- stream lifecycle events
- reconnect behavior
- per-client event filtering

WebSocket gives OpenClaw one **long-lived bidirectional channel**:

```text
client -> Gateway: requests
Gateway -> client: responses and events
node -> Gateway: capabilities and command results
Gateway -> node: node.invoke commands
```

This is why the Gateway can serve both:

- operator clients
- node transports

on the same protocol family.

---

**4. Connect handshake**

The first frame must be a **connect request**.

Example shape:

```json
{
  "type": "req",
  "id": "connect-1",
  "method": "connect",
  "params": {
    "minProtocol": 3,
    "maxProtocol": 3,
    "client": {
      "id": "cli",
      "version": "1.2.3",
      "platform": "macos",
      "mode": "operator"
    },
    "role": "operator",
    "scopes": ["operator.read", "operator.write"],
    "auth": {
      "token": "..."
    },
    "device": {
      "id": "device_fp",
      "publicKey": "...",
      "signature": "...",
      "signedAt": 1737264000,
      "nonce": "..."
    }
  }
}
```

The important parts:

- protocol version range
- client metadata
- requested role
- requested scopes
- authentication material
- device identity
- signed nonce

The **signed nonce** matters because the server needs to know the client controls the device key.

Without nonce signing, a copied device ID would be **too easy to fake**.

---

</details>

## 5. `hello-ok` 响应

**握手成功**会返回 `hello-ok`。

概念上它包含：

```text
protocol version
server metadata
connection id
auth outcome
role
granted scopes
device token, if issued
features.methods
features.events
policy limits
snapshot state
```

示例结构：

```json
{
  "type": "res",
  "id": "connect-1",
  "ok": true,
  "payload": {
    "type": "hello-ok",
    "protocol": 3,
    "server": {
      "version": "x.y.z",
      "connId": "conn-abc"
    },
    "auth": {
      "role": "operator",
      "scopes": ["operator.read", "operator.write"],
      "deviceToken": "..."
    },
    "features": {
      "methods": ["agents.list", "sessions.create", "agent.wait"],
      "events": ["agent.delta", "session.updated"]
    },
    "policy": {
      "tickIntervalMs": 30000,
      "maxPayload": 1048576,
      "maxBufferedBytes": 4194304
    }
  }
}
```

关键经验：

> 客户端应信任 `hello-ok.features`，而不是硬编码方法可用性

这与第 38 讲是同一条原则：

```text
feature detection beats version guessing
```

---

## 6. 启动时不可用状态

启动期间，网关**可能尚未就绪**。

它可能返回可重试的不可用错误，例如：

```json
{
  "type": "res",
  "id": "connect-1",
  "ok": false,
  "error": {
    "type": "UNAVAILABLE",
    "reason": "startup-sidecars",
    "retryable": true
  }
}
```

客户端行为：

- 不要永久崩溃
- 退避
- 重试连接
- 在 UI 中提示 "Gateway starting"

这是**确定性的启动行为**的一部分。

---

## 7. 角色

两类主要角色是：

```text
operator
node
```

### 操作员

操作员是**控制面客户端**。

示例：

- CLI
- 管理 UI
- 操作员模式下的 macOS app
- App SDK 自动化
- 仪表盘

操作员请求网关执行：

- 列出 agent
- 创建会话
- 启动运行
- 处理审批
- 检查状态
- 在允许时管理配置

### 节点

节点是**能力宿主**。

示例：

- macOS 节点模式
- iOS 配套设备
- Android 配套设备
- 无头节点宿主

节点暴露如下能力：

- `canvas.*`
- `camera.*`
- `screen.*`
- `device.*`
- `notifications.*`
- `system.*`

重要边界：

```text
operator = controls Gateway
node     = exposes device capability to Gateway
```

节点**不是网关**。

---

## 8. 作用域

角色表明这是**哪类客户端**。

作用域表明**该客户端被允许做什么**。

常见操作员作用域：

| 作用域 | 含义 |
|---|---|
| `operator.read` | 读取状态、会话、agent、事件 |
| `operator.write` | 创建或变更常规 runtime 资源 |
| `operator.admin` | 管理操作，如 config/update/exec 策略 |
| `operator.approvals` | 审批管理 |
| `operator.pairing` | 设备与节点配对操作 |
| `operator.talk.secrets` | 涉及密钥敏感的 talk 操作 |

保留的管理前缀应要求管理级访问权限：

```text
config.*
exec.approvals.*
update.*
```

好的规则：

> 方法级访问是第一道门，而非唯一一道门

部分方法会附加更深的检查。

示例：

```text
config.get    -> read
config.patch  -> admin
node.invoke   -> role/scope check plus node command policy
```

---

## 9. 设备身份与配对

设备身份的存在，是为了让网关能够**随时间识别客户端**。

发起连接的客户端可以提供：

- 设备 ID
- 公钥
- 签名
- 时间戳
- nonce

若设备尚未获批，网关会创建一个配对请求。

操作员流程：

```bash
openclaw devices list
openclaw devices approve <requestId>
openclaw devices reject <requestId>
```

一旦获批，网关即可签发设备 token。

客户端应**持久化设备 token**，并在重连时复用。

这一点为何重要：

```text
shared password/token:
  useful for bootstrap

device token:
  useful for durable least-privilege reconnects
```

节点应携带一个稳定的 `device.id`，由密钥对指纹派生。

网关 token 按如下维度签发：

```text
device + role + approved scope set
```

除非启用了严格限定范围的本地自动批准路径，否则新设备 ID 都需要配对审批。

安全默认值：

```text
new device ID
  -> pairing request
  -> operator approval
  -> device token issuance
```

配对自动批准应以直接的本地 loopback 连接为核心。

同主机的 tailnet 或 LAN 连接仍应**视为远程**，除非通过配置显式信任。

存在少数无设备的操作员例外，但应严格限定：

- 仅限 localhost 的不安全 Control UI 兼容模式，需显式启用
- 通过可信代理完成的操作员 Control UI 认证
- break-glass `dangerouslyDisableDeviceAuth`，这是一种严重降级
- 使用共享网关 token/密码认证的直接 loopback 后端 RPC

规则：

> 若客户端不在严格显式的信任路径内，则要求设备身份与配对

---


<details>
<summary>English original</summary>

**5. The `hello-ok` response**

A **successful handshake** returns `hello-ok`.

Conceptually it contains:

```text
protocol version
server metadata
connection id
auth outcome
role
granted scopes
device token, if issued
features.methods
features.events
policy limits
snapshot state
```

Example shape:

```json
{
  "type": "res",
  "id": "connect-1",
  "ok": true,
  "payload": {
    "type": "hello-ok",
    "protocol": 3,
    "server": {
      "version": "x.y.z",
      "connId": "conn-abc"
    },
    "auth": {
      "role": "operator",
      "scopes": ["operator.read", "operator.write"],
      "deviceToken": "..."
    },
    "features": {
      "methods": ["agents.list", "sessions.create", "agent.wait"],
      "events": ["agent.delta", "session.updated"]
    },
    "policy": {
      "tickIntervalMs": 30000,
      "maxPayload": 1048576,
      "maxBufferedBytes": 4194304
    }
  }
}
```

The key lesson:

> clients should trust `hello-ok.features`, not hard-code method availability

This is the same principle from Lecture 38:

```text
feature detection beats version guessing
```

---

**6. Startup unavailable state**

During startup, the Gateway **may not be ready**.

It can return a retryable unavailable error such as:

```json
{
  "type": "res",
  "id": "connect-1",
  "ok": false,
  "error": {
    "type": "UNAVAILABLE",
    "reason": "startup-sidecars",
    "retryable": true
  }
}
```

Client behavior:

- do not crash permanently
- back off
- retry connection
- surface "Gateway starting" in UI

This is part of **deterministic startup behavior**.

---

**7. Roles**

The two main roles are:

```text
operator
node
```

**Operator**

An operator is a **control-plane client**.

Examples:

- CLI
- admin UI
- macOS app in operator mode
- App SDK automation
- dashboard

Operators ask the Gateway to:

- list agents
- create sessions
- start runs
- resolve approvals
- inspect status
- manage config if allowed

**Node**

A node is a **capability host**.

Examples:

- macOS node mode
- iOS companion device
- Android companion device
- headless node host

Nodes expose capabilities such as:

- `canvas.*`
- `camera.*`
- `screen.*`
- `device.*`
- `notifications.*`
- `system.*`

The important boundary:

```text
operator = controls Gateway
node     = exposes device capability to Gateway
```

Nodes are **not gateways**.

---

**8. Scopes**

Roles say **what kind of client** this is.

Scopes say **what that client is allowed to do**.

Common operator scopes:

| Scope | Meaning |
|---|---|
| `operator.read` | Read status, sessions, agents, events |
| `operator.write` | Create or mutate normal runtime resources |
| `operator.admin` | Admin operations such as config/update/exec policy |
| `operator.approvals` | Approval management |
| `operator.pairing` | Device and node pairing operations |
| `operator.talk.secrets` | Secret-sensitive talk operations |

Reserved admin prefixes should require admin-level access:

```text
config.*
exec.approvals.*
update.*
```

Good rule:

> method-level access is the first gate, not the only gate

Some methods add deeper checks.

Example:

```text
config.get    -> read
config.patch  -> admin
node.invoke   -> role/scope check plus node command policy
```

---

**9. Device identity and pairing**

Device identity exists so the Gateway can **recognize clients over time**.

A connecting client may present:

- device ID
- public key
- signature
- timestamp
- nonce

If the device is not approved yet, the Gateway creates a pairing request.

Operator flow:

```bash
openclaw devices list
openclaw devices approve <requestId>
openclaw devices reject <requestId>
```

Once approved, the Gateway can issue a device token.

The client should **persist the device token** and reuse it for reconnects.

Why this matters:

```text
shared password/token:
  useful for bootstrap

device token:
  useful for durable least-privilege reconnects
```

Nodes should include a stable `device.id` derived from a keypair fingerprint.

Gateway tokens are issued per:

```text
device + role + approved scope set
```

Pairing approvals are required for new device IDs unless a tightly scoped local auto-approval path is enabled.

The safe default:

```text
new device ID
  -> pairing request
  -> operator approval
  -> device token issuance
```

Pairing auto-approval should be centered on direct local loopback connects.

Same-host tailnet or LAN connects should still be **treated as remote** unless explicitly trusted by configuration.

There are a few device-less operator exceptions, but they should be narrow:

- localhost-only insecure Control UI compatibility, if explicitly enabled
- successful trusted-proxy operator Control UI auth
- break-glass `dangerouslyDisableDeviceAuth`, which is a severe downgrade
- direct-loopback backend RPCs authenticated with the shared Gateway token/password

The rule:

> if a client is not in a narrow explicit trust path, require device identity and pairing

---

</details>

## 10. 设备 token 生命周期

设备 token 是**一等凭证**。

网关可以轮换或吊销它们：

```text
device.token.rotate
device.token.revoke
```

安全行为：

- 非管理员调用者只能管理自己的设备条目
- 配对 scope 规则仍然适用
- token 轮换不得提升角色
- token 吊销应让后续重连失败
- token 变更不能针对配对审批从未授予的设备角色
- 非管理员调用者不能轮换或吊销一个比其自身已持有的更宽泛的 operator token

这给 OpenClaw 带来了比到处使用长期共享 token **更清晰的安全模型**。

### 持久化设备 token

在任何成功连接之后，客户端应持久化主 token：

```text
hello-ok.auth.deviceToken
```

重连时，存储的设备 token 应复用该 token 已批准的 scope 集合。

为何重要：

```text
first connect:
  approved scopes = operator.read + operator.write

reconnect:
  reuse stored device token
  preserve approved read/write access
```

糟糕的重连行为：

```text
client reconnects with stored token
  -> silently collapses to narrower implicit scope
  -> status/probe/read UI breaks
```

良好的重连行为：

```text
stored device token
  -> approved role + scope set restored
```

如果调用者提供了显式 scope 或显式设备 token，则以调用者请求的 scope 集合为准。

仅当客户端复用所存储的按设备 token 时，才会复用缓存 scope。

### 轮换行为

`device.token.rotate` 返回轮换元数据。

它应仅对已用该设备 token 认证的同设备调用回显替换用 bearer token。

这使仅用 token 的客户端能在重连前持久化其替换 token。

共享/管理员轮换不应回显 bearer token。

原因：

```text
same-device token rotation:
  client needs the replacement token to keep working

shared/admin rotation:
  should not leak bearer tokens to broader control-plane clients
```

---

## 11. 认证路径

网关认证可支持多条路径：

- 共享 token
- 共享密码
- 设备 token
- bootstrap token
- 可信代理头
- 私有入口 / 无，仅在有意私有部署中

### 共享密钥认证

共享密钥网关认证使用以下之一：

```text
connect.params.auth.token
connect.params.auth.password
```

取决于所配置的认证模式。

在客户端侧，密码与 token 并不等同：

```text
auth.password:
  orthogonal
  forwarded when set

auth.token:
  selected by priority
```

Token 选择优先级：

```text
1. explicit shared token
2. explicit deviceToken
3. stored per-device token keyed by deviceId + role
```

Bootstrap token 行为：

```text
auth.bootstrapToken is sent only when no auth.token was resolved
```

这意味着共享 token 或任何已解析的设备 token 会抑制 bootstrap 认证。

### 可信代理与私有入口

携带身份的模式可以从请求头而非 `connect.params.auth.*` 满足连接认证。

示例：

```text
gateway.auth.allowTailscale = true
gateway.auth.mode = "trusted-proxy"
```

这些模式适用于**上游层已完成身份认证**的部署。

私有入口模式：

```text
gateway.auth.mode = "none"
```

跳过共享密钥连接认证。

仅在可信私有入口之后使用它。

不要在**公共或不受信网络**上暴露私有入口模式。

### Bootstrap 交接 token

`hello-ok.auth.deviceTokens` 可包含额外的 bootstrap 交接 token。

仅当连接在可信传输上使用了 bootstrap 认证时才持久化它们，例如：

```text
wss:// with appropriate trust
loopback / local pairing path
```

不要盲目持久化来自**不受信公共连接**的交接 token。

客户端应通过恢复逻辑处理认证失败。

有用的错误提示包括：

```text
canRetryWithDeviceToken
recommendedNextStep
nonce/signature diagnostic code
```

实际的客户端行为：

```text
auth failed
  -> check whether device token exists
  -> retry if server suggests it
  -> otherwise show pairing/login guidance
```


<details>
<summary>English original</summary>

**10. Device token lifecycle**

Device tokens are **first-class credentials**.

The Gateway can rotate or revoke them:

```text
device.token.rotate
device.token.revoke
```

Safe behavior:

- non-admin callers can only manage their own device entries
- pairing scope rules still apply
- token rotation must not upgrade roles
- token revocation should make future reconnect fail
- token mutation cannot target a device role that pairing approval never granted
- non-admin callers cannot rotate or revoke a broader operator token than they already hold

This gives OpenClaw a **cleaner security model** than long-lived shared tokens everywhere.

**Persisting device tokens**

After any successful connect, clients should persist the primary token:

```text
hello-ok.auth.deviceToken
```

On reconnect, the stored device token should reuse the approved scope set for that token.

Why this matters:

```text
first connect:
  approved scopes = operator.read + operator.write

reconnect:
  reuse stored device token
  preserve approved read/write access
```

Bad reconnect behavior:

```text
client reconnects with stored token
  -> silently collapses to narrower implicit scope
  -> status/probe/read UI breaks
```

Good reconnect behavior:

```text
stored device token
  -> approved role + scope set restored
```

If the caller supplies explicit scopes or an explicit device token, that caller-requested scope set stays authoritative.

Cached scopes are only reused when the client is reusing the stored per-device token.

**Rotation behavior**

`device.token.rotate` returns rotation metadata.

It should echo the replacement bearer token only for same-device calls already authenticated with that device token.

That lets token-only clients persist their replacement before reconnecting.

Shared/admin rotations should not echo the bearer token.

Why:

```text
same-device token rotation:
  client needs the replacement token to keep working

shared/admin rotation:
  should not leak bearer tokens to broader control-plane clients
```

---

**11. Auth paths**

Gateway auth may support multiple paths:

- shared token
- shared password
- device token
- bootstrap token
- trusted proxy headers
- private-ingress / none, only in intentionally private deployments

**Shared-secret auth**

Shared-secret Gateway auth uses one of:

```text
connect.params.auth.token
connect.params.auth.password
```

depending on configured auth mode.

On the client side, password and token are not identical:

```text
auth.password:
  orthogonal
  forwarded when set

auth.token:
  selected by priority
```

Token selection priority:

```text
1. explicit shared token
2. explicit deviceToken
3. stored per-device token keyed by deviceId + role
```

Bootstrap token behavior:

```text
auth.bootstrapToken is sent only when no auth.token was resolved
```

That means a shared token or any resolved device token suppresses bootstrap auth.

**Trusted proxy and private ingress**

Identity-bearing modes can satisfy connect auth from request headers rather than `connect.params.auth.*`.

Examples:

```text
gateway.auth.allowTailscale = true
gateway.auth.mode = "trusted-proxy"
```

These modes are for deployments where an **upstream layer already authenticates identity**.

Private-ingress mode:

```text
gateway.auth.mode = "none"
```

skips shared-secret connect auth.

Use it only behind trusted private ingress.

Do not expose private-ingress mode on **public or untrusted networks**.

**Bootstrap handoff tokens**

`hello-ok.auth.deviceTokens` can contain additional bootstrap handoff tokens.

Persist them only when the connection used bootstrap auth on a trusted transport such as:

```text
wss:// with appropriate trust
loopback / local pairing path
```

Do not blindly persist handoff tokens from an **untrusted public connection**.

The client should handle auth failures with recovery logic.

Useful error hints include:

```text
canRetryWithDeviceToken
recommendedNextStep
nonce/signature diagnostic code
```

Practical client behavior:

```text
auth failed
  -> check whether device token exists
  -> retry if server suggests it
  -> otherwise show pairing/login guidance
```

</details>

### `AUTH_TOKEN_MISMATCH`

对于 `AUTH_TOKEN_MISMATCH`，受信客户端可以用缓存的 per-device token 尝试一次有界重试。

受信意味着：

```text
loopback
or
wss:// with pinned tlsFingerprint
```

未做 pinning 的公开 `wss://` **不符合**自动 token 提升的条件。

如果重试失败：

```text
stop automatic reconnect loop
surface operator action guidance
```

不要用**错误凭据**无限空转。

恢复提示可以包括：

| Field | Purpose |
|---|---|
| `error.details.code` | 稳定的机器可读认证失败码 |
| `error.details.canRetryWithDeviceToken` | 设备 token 重试是否有帮助 |
| `error.details.recommendedNextStep` | 建议的客户端/运维操作 |

示例推荐后续步骤：

```text
retry_with_device_token
update_auth_configuration
update_auth_credentials
wait_then_retry
review_auth_configuration
```

---

## 12. Device auth migration diagnostics

所有连接都应**对服务端提供的** `connect.challenge` nonce 进行签名。

旧版客户端可能仍使用挑战前签名行为。

对于这些客户端，Gateway 认证应返回稳定的 `DEVICE_AUTH_*` 详情码。

| Message | details.code | details.reason | Meaning |
|---|---|---|---|
| device nonce required | `DEVICE_AUTH_NONCE_REQUIRED` | `device-nonce-missing` | 客户端遗漏 `device.nonce` 或发送了空值 |
| device nonce mismatch | `DEVICE_AUTH_NONCE_MISMATCH` | `device-nonce-mismatch` | 客户端用过期或错误的 nonce 签名 |
| device signature invalid | `DEVICE_AUTH_SIGNATURE_INVALID` | `device-signature` | 签名载荷与预期载荷不匹配 |
| device signature expired | `DEVICE_AUTH_SIGNATURE_EXPIRED` | `device-signature-stale` | 签名时间戳超出允许偏移 |
| device identity mismatch | `DEVICE_AUTH_DEVICE_ID_MISMATCH` | `device-id-mismatch` | `device.id` 与公钥指纹不匹配 |
| device public key invalid | `DEVICE_AUTH_PUBLIC_KEY_INVALID` | `device-public-key` | 公钥格式或规范化失败 |

迁移目标：

```text
1. wait for connect.challenge
2. sign the payload that includes the server nonce
3. send the same nonce in connect.params.device.nonce
```

首选签名载荷：

```text
v3 signature payload
  binds platform
  binds deviceFamily
  binds device/client/role/scopes/token/nonce fields
```

旧版 v2 签名可以出于兼容性继续接受，但配对设备元数据在重连时仍应控制命令策略。

---

## 13. TLS and pinning

Gateway WebSocket 连接可以使用 **TLS**。

客户端可以选择性地**pin Gateway 证书指纹**。

相关配置/CLI 概念：

```text
gateway.tls
gateway.remote.tlsFingerprint
--tls-fingerprint
```

Pinning 之所以重要，是因为它把「加密连接」升级为「我确切知道我所期望的是哪个 Gateway 证书」。

这就是为什么自动提升已存储设备 token 应限于：

```text
loopback
or
wss:// with pinned fingerprint
```

不做 pinning 时，公开 `wss://` 端点**虽有加密但不受信**，不足以进行激进的凭据回退。

---

## 14. Feature discovery

`hello-ok.features` 是**发现面**。

它通告：

```text
methods
events
```

客户端应用它来决定是否显示 UI。

示例：

```ts
if (features.methods.includes("artifacts.list")) {
  showArtifactsPanel();
} else {
  hideArtifactsPanel();
}
```

不要假设：

```text
OpenClaw version x.y.z means method exists
```

应假设：

```text
method exists only if hello-ok advertises it
```

这使客户端在**版本偏差**下更安全。

---

## 15. Size limits and payload safety

连接之前，帧**被严格限制上限**。

所提供的摘要描述了连接前的上限：

```text
64 KiB
```

握手之后，客户端应遵守：

```text
hello-ok.policy.maxPayload
hello-ok.policy.maxBufferedBytes
hello-ok.policy.tickIntervalMs
```

如果客户端发送超大尺寸数据，Gateway 可以发出如下诊断：

```text
payload.large
```

然后它可以丢弃或关闭连接。

实用规则：

> 不要把大文件、视频或截图作为任意 JSON 帧推送

应使用：

- 产物元数据
- 下载句柄
- 分块
- 媒体附件
- 显式文件 API

---

## 16. Events and broadcast scoping

事件**不会被盲目广播**给所有人。

它们是**按范围门控的**。

示例：

```text
chat / agent / tool frames:
  require operator.read

plugin broadcasts:
  default to operator.write or operator.admin depending on registration

status / heartbeat / presence / tick:
  generally not scope-restricted
```

安全规则：

> 如果某个客户端不应看到某个 session、run、task、artifact、approval 或与 secret 相关的事件，就不要把它广播给该客户端

Gateway 还保持每个 socket 上的顺序**单调递增**。

这意味着每个客户端在过滤之后得到自己的序列视图。

这一点很重要，因为否则范围过滤可能使事件顺序变得有歧义。

---


<details>
<summary>English original</summary>

**`AUTH_TOKEN_MISMATCH`**

For `AUTH_TOKEN_MISMATCH`, trusted clients may attempt one bounded retry with a cached per-device token.

Trusted means:

```text
loopback
or
wss:// with pinned tlsFingerprint
```

Public `wss://` without pinning **does not qualify** for automatic token promotion.

If the retry fails:

```text
stop automatic reconnect loop
surface operator action guidance
```

Do not spin forever with **bad credentials**.

Recovery hints may include:

| Field | Purpose |
|---|---|
| `error.details.code` | Stable machine-readable auth failure code |
| `error.details.canRetryWithDeviceToken` | Whether a device-token retry may help |
| `error.details.recommendedNextStep` | Suggested client/operator action |

Example recommended next steps:

```text
retry_with_device_token
update_auth_configuration
update_auth_credentials
wait_then_retry
review_auth_configuration
```

---

**12. Device auth migration diagnostics**

All connections should **sign the server-provided** `connect.challenge` nonce.

Legacy clients may still use pre-challenge signing behavior.

For those clients, Gateway auth should return stable `DEVICE_AUTH_*` detail codes.

| Message | details.code | details.reason | Meaning |
|---|---|---|---|
| device nonce required | `DEVICE_AUTH_NONCE_REQUIRED` | `device-nonce-missing` | Client omitted `device.nonce` or sent it blank |
| device nonce mismatch | `DEVICE_AUTH_NONCE_MISMATCH` | `device-nonce-mismatch` | Client signed with stale or wrong nonce |
| device signature invalid | `DEVICE_AUTH_SIGNATURE_INVALID` | `device-signature` | Signature payload does not match expected payload |
| device signature expired | `DEVICE_AUTH_SIGNATURE_EXPIRED` | `device-signature-stale` | Signed timestamp is outside allowed skew |
| device identity mismatch | `DEVICE_AUTH_DEVICE_ID_MISMATCH` | `device-id-mismatch` | `device.id` does not match public key fingerprint |
| device public key invalid | `DEVICE_AUTH_PUBLIC_KEY_INVALID` | `device-public-key` | Public key format or canonicalization failed |

Migration target:

```text
1. wait for connect.challenge
2. sign the payload that includes the server nonce
3. send the same nonce in connect.params.device.nonce
```

Preferred signing payload:

```text
v3 signature payload
  binds platform
  binds deviceFamily
  binds device/client/role/scopes/token/nonce fields
```

Legacy v2 signatures may remain accepted for compatibility, but paired-device metadata should still control command policy on reconnect.

---

**13. TLS and pinning**

Gateway WebSocket connections can use **TLS**.

Clients may optionally **pin the Gateway certificate fingerprint**.

Relevant configuration/CLI concepts:

```text
gateway.tls
gateway.remote.tlsFingerprint
--tls-fingerprint
```

Pinning matters because it upgrades "encrypted connection" into "I know exactly which Gateway certificate I expected."

This is why automatic stored-device-token promotion should be limited to:

```text
loopback
or
wss:// with pinned fingerprint
```

Without pinning, a public `wss://` endpoint is **encrypted but not trusted** enough for aggressive credential fallback.

---

**14. Feature discovery**

`hello-ok.features` is the **discovery surface**.

It advertises:

```text
methods
events
```

The client should use this to decide whether to show UI.

Example:

```ts
if (features.methods.includes("artifacts.list")) {
  showArtifactsPanel();
} else {
  hideArtifactsPanel();
}
```

Do not assume:

```text
OpenClaw version x.y.z means method exists
```

Assume:

```text
method exists only if hello-ok advertises it
```

This makes clients safer across **version skew**.

---

**15. Size limits and payload safety**

Before connect, frames are **capped tightly**.

The provided summary describes a pre-connect cap of:

```text
64 KiB
```

After handshake, clients should honor:

```text
hello-ok.policy.maxPayload
hello-ok.policy.maxBufferedBytes
hello-ok.policy.tickIntervalMs
```

If a client sends oversized data, the Gateway can emit diagnostics such as:

```text
payload.large
```

Then it may drop or close the connection.

Practical rule:

> do not push large files, videos, or screenshots as arbitrary JSON frames

Use:

- artifact metadata
- download handles
- chunking
- media attachments
- explicit file APIs

---

**16. Events and broadcast scoping**

Events are **not broadcast blindly** to everyone.

They are **scope-gated**.

Examples:

```text
chat / agent / tool frames:
  require operator.read

plugin broadcasts:
  default to operator.write or operator.admin depending on registration

status / heartbeat / presence / tick:
  generally not scope-restricted
```

The secure rule:

> if a client should not see a session, run, task, artifact, approval, or secret-adjacent event, do not broadcast it to that client

The Gateway also keeps ordering **monotonic per socket**.

That means each client gets its own sequence view after filtering.

This matters because scope filtering could otherwise make event ordering ambiguous.

---

</details>

## 17. 常见 RPC 方法族

网关有许多**方法族**。

要按**类别**来思考。

| 方法族 | 示例 | 用途 |
|---|---|---|
| System | `health`, `status`, `system-presence` | 存活性与 runtime 状态 |
| Config | `config.get`, `config.patch`, `config.apply`, `config.schema` | 受控配置 |
| Update | `update.run`, `update.status` | runtime 更新流程 |
| Agents | `agents.list`, `agents.create`, `agents.update` | agent 管理 |
| Sessions | `sessions.list`, `sessions.create`, `sessions.send`, `sessions.abort`, `sessions.compact` | 会话状态 |
| Chat | `chat.history`, `chat.send` | 面向聊天的操作 |
| Runs | `agent.wait` | 等待 run 生命周期 |
| Models | `models.list` | 模型目录与选择器支持 |
| Usage | `usage.status`, `usage.cost` | 成本与用量上报 |
| Channels | `channels.status`, `web.login.start`, `web.login.wait`, `channels.logout` | 外部消息入口 |
| Nodes | `node.invoke`, `node.pair.*`, `node.pending.*` | 设备与节点传输 |
| Approvals | `exec.approval.request`, `exec.approval.list`, `exec.approval.resolve` | 人工审批流程 |
| Automation | `cron.*`, `wake` | 定时与唤醒驱动的执行 |
| Skills | `skills.*` | 技能发现与管理 |
| Tools | `tools.catalog`, `tools.effective` | 工具可见性 |

不需要记住每一个方法。

需要理解这个模式：

```text
typed method
  -> schema
  -> scope gate
  -> handler
  -> discovery in hello-ok.features.methods
```

---

## 18. 幂等性

有副作用的方法需要**幂等键**。

为什么？

因为真实客户端会重试。

错误行为：

```text
mobile reconnects
  -> repeats sessions.send
  -> Gateway starts two runs
```

正确行为：

```text
mobile reconnects
  -> repeats same request with same idempotency key
  -> Gateway returns same accepted operation
```

应当使用幂等性的方法：

- create session
- send message
- start run
- cancel run
- approve action
- rotate token
- invoke side-effecting node command
- patch config
- schedule cron job

幂等性**不是润色**。

它是**可靠的分布式客户端**的必要条件。

---

## 19. TypeBox schema 与生成的客户端

网关 RPC 的形态应当**归 schema 所有**。

OpenClaw 使用 **TypeBox 风格的 schema** 来描述规范协议形态。

工作流：

```text
define schema
  -> generate validators/types
  -> register method handler
  -> advertise method
  -> update SDK wrapper
  -> test client/server parity
```

原始材料中提到的命令：

```bash
pnpm protocol:gen
pnpm protocol:check
```

工程目标：

> 客户端与服务端不应对方法形态产生静默分歧

---

## 20. 机密安全

诊断与发现**不得泄漏机密**。

不要暴露：

- 诊断快照中的原始聊天正文
- webhook 请求正文
- token
- cookie
- 机密值
- 原始授权头

改为暴露摘要：

```text
capability: configured
auth: token-present
channel: connected
lastSeenAtMs: 1737264000
```

这是控制面的核心规则：

> 可观测性是必要的，但原始机密不应出现在状态载荷中

---

## 21. 通过网关 RPC 的节点

节点连接的是同一个网关 WebSocket 协议，但带有：

```json
{ "role": "node" }
```

节点声明：

- 设备 ID
- 角色
- 作用域
- 能力
- 命令
- 权限

网关把这些视为**声称**。

仅有**声称**还不够。

网关仍然执行服务端策略：

```text
node declared command
  + gateway allowlist permits command
  + caller has scope
  + plugin policy permits command, if present
  -> node.invoke allowed
```

presence 方法可以暴露：

- `deviceId`
- 角色
- 作用域
- `lastSeenAtMs`
- 原因
- 能力

但同样：

> 能力上报不应暴露机密

---

## 22. 模型列表视图

模型列表是**一个方法多种视图**的一个有用示例。

`models.list` 接受一个 `view`。

| 视图 | 含义 |
|---|---|
| 省略 / `default` | runtime 允许的目录，遵循默认模型策略 |
| `configured` | 适配选择器的已配置模型 |
| `all` | 用于诊断的完整网关目录 |

这比创建**三个互不相关的方法**要好。

它给客户端一份清晰的契约：

```text
normal UI:
  default or configured

diagnostics:
  all
```

---


<details>
<summary>English original</summary>

**17. Common RPC families**

The Gateway has many **method families**.

Think in **categories**.

| Family | Examples | Purpose |
|---|---|---|
| System | `health`, `status`, `system-presence` | Liveness and runtime status |
| Config | `config.get`, `config.patch`, `config.apply`, `config.schema` | Controlled configuration |
| Update | `update.run`, `update.status` | Runtime update workflow |
| Agents | `agents.list`, `agents.create`, `agents.update` | Agent management |
| Sessions | `sessions.list`, `sessions.create`, `sessions.send`, `sessions.abort`, `sessions.compact` | Conversation state |
| Chat | `chat.history`, `chat.send` | Chat-facing operations |
| Runs | `agent.wait` | Wait for run lifecycle |
| Models | `models.list` | Model catalog and picker support |
| Usage | `usage.status`, `usage.cost` | Cost and usage reporting |
| Channels | `channels.status`, `web.login.start`, `web.login.wait`, `channels.logout` | External message surfaces |
| Nodes | `node.invoke`, `node.pair.*`, `node.pending.*` | Device and node transport |
| Approvals | `exec.approval.request`, `exec.approval.list`, `exec.approval.resolve` | Human approval flow |
| Automation | `cron.*`, `wake` | Scheduled and wake-based execution |
| Skills | `skills.*` | Skill discovery and management |
| Tools | `tools.catalog`, `tools.effective` | Tool visibility |

You do not need to memorize every method.

You need to understand the pattern:

```text
typed method
  -> schema
  -> scope gate
  -> handler
  -> discovery in hello-ok.features.methods
```

---

**18. Idempotency**

Side-effecting methods need **idempotency keys**.

Why?

Because real clients retry.

Bad behavior:

```text
mobile reconnects
  -> repeats sessions.send
  -> Gateway starts two runs
```

Good behavior:

```text
mobile reconnects
  -> repeats same request with same idempotency key
  -> Gateway returns same accepted operation
```

Methods that should use idempotency:

- create session
- send message
- start run
- cancel run
- approve action
- rotate token
- invoke side-effecting node command
- patch config
- schedule cron job

Idempotency is **not polish**.

It is required for **reliable distributed clients**.

---

**19. TypeBox schemas and generated clients**

Gateway RPC shapes should be **schema-owned**.

OpenClaw uses **TypeBox-style schemas** for canonical protocol shapes.

The workflow:

```text
define schema
  -> generate validators/types
  -> register method handler
  -> advertise method
  -> update SDK wrapper
  -> test client/server parity
```

Commands mentioned in the source material:

```bash
pnpm protocol:gen
pnpm protocol:check
```

The engineering goal:

> client and server should not silently disagree about method shapes

---

**20. Secret safety**

Diagnostics and discovery must **not leak secrets**.

Do not expose:

- raw chat bodies in diagnostic snapshots
- webhook request bodies
- tokens
- cookies
- secret values
- raw authorization headers

Expose summaries instead:

```text
capability: configured
auth: token-present
channel: connected
lastSeenAtMs: 1737264000
```

This is a core rule for control planes:

> observability is necessary, but raw secrets do not belong in status payloads

---

**21. Nodes over Gateway RPC**

Nodes connect to the same Gateway WebSocket protocol, but with:

```json
{ "role": "node" }
```

Nodes declare:

- device ID
- roles
- scopes
- capabilities
- commands
- permissions

The Gateway treats these as **claims**.

Claims are **not enough**.

The Gateway still enforces server-side policy:

```text
node declared command
  + gateway allowlist permits command
  + caller has scope
  + plugin policy permits command, if present
  -> node.invoke allowed
```

Presence methods can expose:

- `deviceId`
- roles
- scopes
- `lastSeenAtMs`
- reason
- capabilities

But again:

> capability reporting should not expose secrets

---

**22. Model listing views**

Model listing is a useful example of **one method with multiple views**.

`models.list` accepts a `view`.

| View | Meaning |
|---|---|
| omitted / `default` | runtime-allowed catalog, respecting default model policy |
| `configured` | picker-sized configured models |
| `all` | full Gateway catalog for diagnostics |

This is better than creating **three unrelated methods**.

It gives clients a clear contract:

```text
normal UI:
  default or configured

diagnostics:
  all
```

---

</details>

## 23. 重连与超时

客户端需要**可预测的超时行为**。

源材料中的典型值：

```text
per-RPC request timeout:
  30,000 ms

default tick interval before handshake:
  30,000 ms

reconnect backoff:
  initial 1s
  max 30s
```

握手之后，使用服务端策略：

```text
hello-ok.policy.tickIntervalMs
```

客户端行为：

- 不要忙循环重连
- 退避
- 仅在协议表明安全时才更快重置
- 将协议不匹配视为硬失败
- 将启动不可用视为可重试
- 在有界 device-token 重试失败后，停止自动重连循环

---

## 24. 扩展 RPC 接口面

新增方法时，使用这份**检查清单**。

### 1. 明确方法用途

反例：

```text
platform.doEverything
```

正例：

```text
artifacts.list
artifacts.get
artifacts.download
```

让方法保持**窄**。

### 2. 添加 schema

用 TypeBox 定义请求与响应结构。

### 3. 添加 scope 门禁

示例：

```text
read-only discovery:
  operator.read

mutation:
  operator.write

admin:
  operator.admin

pairing:
  operator.pairing
```

### 4. 若有副作用则添加幂等性

如果重试请求可能重复执行工作，则要求 idempotency key。

### 5. 在 `hello-ok.features.methods` 中公布

客户端应当能发现方法，而不是靠猜。

### 6. 按需添加事件

事件族同样要做 scope 门禁。

### 7. 保护机密

绝不要在状态、诊断或发现信息中包含原始机密值。

### 8. 添加 SDK 封装与生成的原生模型

只有在客户端能安全使用之后，公共 API 才**算完整**。

---

## 25. Gateway 协议与 App SDK 对比

第 38 讲关注的是：

```text
App SDK
  oc.agents
  oc.sessions
  oc.runs
  oc.models
  oc.approvals
```

本讲关注的是：

```text
Gateway RPC
  req/res/event frames
  handshake
  scopes
  pairing
  features
  policy limits
  node transport
```

关系：

```text
Gateway protocol = wire contract
App SDK          = developer-friendly wrapper
```

App 开发者通常应当使用 **SDK**。

SDK 开发者与平台工程师必须理解 **Gateway 协议**。

---

## 26. 设计练习

设计一个新的 RPC 族：

```text
artifacts.*
```

回答：

1. 应当存在哪些方法？
2. 哪些方法是只读的？
3. 需要哪些 scope？
4. 是否有方法需要 idempotency key？
5. 应由哪个事件族通告产物变更？
6. 大文件应如何避免违反 `maxPayload`？
7. `hello-ok.features.methods` 中应当出现什么？
8. 什么绝不能出现在诊断中？

然后对以下对象重复同样的设计：

```text
environments.*
```

比较纯发现型 API 与变更型 API 之间的差异。

---

## 关键要点

- Gateway RPC 是 OpenClaw 的 WebSocket 控制平面与节点传输层。
- wire 模型很小：`req`、`res` 与 `event`。
- connect 握手协商协议、角色、scope、feature 与策略。
- `hello-ok.features.methods` 与 `hello-ok.features.events` 构成发现接口面。
- 角色标识客户端类型；scope 授权具体操作。
- 设备配对与 device token 使持久化的已认证客户端成为可能。
- 广播必须经过 scope 门禁，并按客户端 socket 保序。
- 有副作用的方法需要 idempotency key。
- TypeBox schema 让客户端与服务端的协议结构保持对齐。
- 诊断必须概括状态而不泄露机密。
- 节点是同一 Gateway 协议之上的能力宿主，而不是独立的网关。
- App SDK 封装该协议；它不应取代协议契约。

---

## 参考文献

- OpenClaw Gateway 协议：[https://openclaw.knidal.com/gateway-protocol](https://openclaw.knidal.com/gateway-protocol)
- OpenClaw App SDK：[https://openclaw.knidal.com/openclaw-app-sdk](https://openclaw.knidal.com/openclaw-app-sdk)
- OpenClaw Nodes：[https://openclaw.knidal.com/nodes](https://openclaw.knidal.com/nodes)
- OpenClaw Tools Invoke API：[https://openclaw.knidal.com/tools-invoke-api](https://openclaw.knidal.com/tools-invoke-api)
- 案例研究源码仓库：[OpenClaw](https://github.com/openclaw/openclaw)

---

*下一讲：[第 40 讲](Lecture-40.md)*


<details>
<summary>English original</summary>

**23. Reconnect and timeouts**

Clients need **predictable timeout behavior**.

Typical values from the source material:

```text
per-RPC request timeout:
  30,000 ms

default tick interval before handshake:
  30,000 ms

reconnect backoff:
  initial 1s
  max 30s
```

After handshake, use the server policy:

```text
hello-ok.policy.tickIntervalMs
```

Client behavior:

- do not busy-loop reconnects
- back off
- reset faster only when the protocol says it is safe
- treat protocol mismatch as hard failure
- treat startup unavailable as retryable
- stop automatic reconnect loops after a failed bounded device-token retry

---

**24. Extending the RPC surface**

When adding a new method, use this **checklist**.

**1. Define the method purpose**

Bad:

```text
platform.doEverything
```

Good:

```text
artifacts.list
artifacts.get
artifacts.download
```

Keep the method **narrow**.

**2. Add schema**

Define request and response shapes with TypeBox.

**3. Add scope gate**

Examples:

```text
read-only discovery:
  operator.read

mutation:
  operator.write

admin:
  operator.admin

pairing:
  operator.pairing
```

**4. Add idempotency if side-effecting**

If retrying the request could duplicate work, require an idempotency key.

**5. Advertise in `hello-ok.features.methods`**

Clients should discover the method, not guess it.

**6. Add events if needed**

Scope-gate event families too.

**7. Protect secrets**

Never include raw secret values in status, diagnostics, or discovery.

**8. Add SDK wrappers and generated native models**

The public API is **not complete** until clients can use it safely.

---

**25. Gateway protocol versus App SDK**

Lecture 38 focused on:

```text
App SDK
  oc.agents
  oc.sessions
  oc.runs
  oc.models
  oc.approvals
```

This lecture focused on:

```text
Gateway RPC
  req/res/event frames
  handshake
  scopes
  pairing
  features
  policy limits
  node transport
```

Relationship:

```text
Gateway protocol = wire contract
App SDK          = developer-friendly wrapper
```

App authors should normally use the **SDK**.

SDK authors and platform engineers must understand the **Gateway protocol**.

---

**26. Design exercise**

Design a new RPC family:

```text
artifacts.*
```

Answer:

1. Which methods should exist?
2. Which methods are read-only?
3. Which scopes are required?
4. Do any methods need idempotency keys?
5. Which event family should announce artifact changes?
6. How should large files avoid `maxPayload` violations?
7. What should appear in `hello-ok.features.methods`?
8. What must never appear in diagnostics?

Then repeat the same design for:

```text
environments.*
```

Compare the difference between discovery-only APIs and mutation APIs.

---

**Key takeaways**

- Gateway RPC is OpenClaw's WebSocket control plane and node transport.
- The wire model is small: `req`, `res`, and `event`.
- The connect handshake negotiates protocol, role, scopes, features, and policy.
- `hello-ok.features.methods` and `hello-ok.features.events` are the discovery surface.
- Roles identify the client type; scopes authorize specific actions.
- Device pairing and device tokens make durable authenticated clients possible.
- Broadcasts must be scope-gated and ordered per client socket.
- Side-effecting methods need idempotency keys.
- TypeBox schemas keep client and server protocol shapes aligned.
- Diagnostics must summarize state without leaking secrets.
- Nodes are capability hosts over the same Gateway protocol, not separate gateways.
- The App SDK wraps this protocol; it should not replace the protocol contract.

---

**References**

- OpenClaw Gateway protocol: [https://openclaw.knidal.com/gateway-protocol](https://openclaw.knidal.com/gateway-protocol)
- OpenClaw App SDK: [https://openclaw.knidal.com/openclaw-app-sdk](https://openclaw.knidal.com/openclaw-app-sdk)
- OpenClaw Nodes: [https://openclaw.knidal.com/nodes](https://openclaw.knidal.com/nodes)
- OpenClaw Tools Invoke API: [https://openclaw.knidal.com/tools-invoke-api](https://openclaw.knidal.com/tools-invoke-api)
- Case-study source repo: [OpenClaw](https://github.com/openclaw/openclaw)

---

*Next: [Lecture 40](Lecture-40.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-39.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-39.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
