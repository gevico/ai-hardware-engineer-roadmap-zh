---
title: Lab 05 — 在 macOS 上用 OpenCoven OpenMeow Dogfood 适配器测试 OpenClaw App SDK
description: Lab 05 — 在 macOS 上用 OpenCoven OpenMeow Dogfood 适配器测试 OpenClaw App SDK
---

# Lab 05 — 在 macOS 上用 OpenCoven OpenMeow Dogfood 适配器测试 OpenClaw App SDK

**方向 B · Agentic AI & GenAI** | [← 目录](README.md) | [上一节 → Lab 04](Lab-04-TokenJuice-Output-Compaction.md)

---

## 概述

本实验从真实外部 macOS app 的角度测试 OpenClaw App SDK。

dogfood 客户端是 OpenCoven 的 `open-meow-sdk` 仓库。

OpenMeow 被描述为原生 macOS 刘海/inbox 客户端。`open-meow-sdk` 仓库是适配器与测试 harness（agent 运行时框架），它证明 app 可以在不导入 OpenClaw 内部实现的前提下使用 `@openclaw/sdk`。

核心边界：

```text
OpenMeow macOS app
  -> OpenMeow SDK adapter
  -> @openclaw/sdk
  -> OpenClaw Gateway RPC
  -> OpenClaw runtime
```

这是 app SDK 实验，不是 plugin SDK 实验。

这一区别很重要：

```text
App SDK:
  external app talks to Gateway

Plugin SDK:
  code runs inside OpenClaw
```

**预计用时：** 60-90 分钟

**难度：** 中级

---

## 你将测试什么

OpenMeow 的 dogfood 路径聚焦于 P0 app happy path：

```text
connect
  -> list agents
  -> create or reuse lane session
  -> send message
  -> stream normalized events
  -> wait for result
  -> cancel active run
  -> map state into app UI
```

本实验有两个层级。

| 层级 | 目标 | 是否需要实时 OpenClaw Gateway？ |
|---|---|---|
| fixture 测试 | 验证适配器、事件、wait/cancel、UI reducer 行为 | 否 |
| 实时 smoke 测试 | 将 OpenMeow 适配器连接到本地 Gateway | 是 |

先做 fixture 测试。

它们是确定性的，能在你调试网络之前捕获 app 契约的回归。

---

## 前置要求

在 macOS 上：

```bash
xcode-select --install
brew install git node pnpm
node --version
pnpm --version
```

建议：

- Node.js 20 或更新版本
- Git
- 仅在本地 agent 工作流需要时，才需要具有完全磁盘访问权限的终端
- 可选：本地 OpenClaw Gateway
- 可选：Coven，如果你还想测试本地 harness 会话

不要从原生 macOS UI 开始。

从适配器测试开始。app UI 应该是你最后才信任的层。

---

## 步骤 1 — 克隆 dogfood 适配器

```bash
mkdir -p ~/Developer/opencoven-labs
cd ~/Developer/opencoven-labs
git clone https://github.com/OpenCoven/open-meow-sdk.git
cd open-meow-sdk
```

检查仓库：

```bash
find . -maxdepth 3 -type f | sort
```

关键文件：

| 文件 | 重要性 |
|---|---|
| `src/index.js` | 基于 `@openclaw/sdk` 的 OpenMeow 侧适配器 |
| `src/index.d.ts` | 面向 app 的 TypeScript 接口面 |
| `test/openmeow-sdk-client.test.js` | 适配器行为的 Node 测试覆盖 |
| `fixtures/openclaw-events/*.jsonl` | 规范化后的 Gateway 事件 fixture |
| `scripts/validate-event-fixtures.mjs` | fixture schema 与终止事件校验 |
| `docs/openmeow-sdk-adapter-shape.md` | OpenMeow 所需的最小适配器 |
| `docs/gateway-rpc-gap-map.md` | SDK 方法与 Gateway RPC 就绪度的对比 |
| `docs/openmeow-dogfood-plan.md` | 分阶段的 dogfood 计划 |

---

## 步骤 2 — 运行确定性的测试

package 的 test 命令会运行 fixture 校验和 Node 测试：

```bash
npm test
```

预期形态：

```text
Validated N fixture events.
ok ...
```

如果想分别运行各部分：

```bash
node scripts/validate-event-fixtures.mjs
node --test
```

这些测试证明 OpenMeow 适配器可以：

- 包装 SDK 的 happy path
- 列出 agent
- 创建 lane 会话
- 发送消息
- 流式传输事件
- 等待结果
- 取消进行中的 run
- 将规范化事件映射为 UI 状态
- 让 composer 保持在发送或停止模式
- 区分 wait 截止时间与 runtime 超时
- 让未知事件仅用于调试

这是本实验最重要的部分。

如果这些测试失败，先不要调试 macOS UI。

---

## 步骤 3 — 阅读适配器契约

打开适配器定义：

```bash
sed -n '1,220p' docs/openmeow-sdk-adapter-shape.md
```

重要的接口面：

```ts
connect()
close()
listAgents()
getAgentIdentity(agentId)
createLaneSession({ agentId, label, sessionKey })
send(sessionKey, message)
events(runId)
wait(runId, timeoutMs)
cancel(runId, sessionKey)
effectiveTools(sessionKey)
```

它有意比完整的 OpenClaw Gateway 更小。

这是好事。

app 不应从所有内部能力开始。

而应从 UI 实际需要的操作开始。

---

## 步骤 4 — 理解 lane 模型

OpenMeow 使用 lane 的概念。

lane 是面向 app 的会话目标：

```text
Lane:
  agentId
  label
  sessionKey
  active run state
  streamed assistant draft
  compact tool activity
  approval cards
```

会话是持久的。

run 是进行中的工作。

lane 是 UI 容器。

心智模型：

```text
agent = who should do the work
session = where memory/transcript lives
run = this specific execution
lane = how the macOS app presents it
```

对桌面客户端而言，这种拆分是正确的。

不要让 UI 混淆会话身份与 run 身份。

---



---


<details>
<summary>English original</summary>

**Lab 05 — Test the OpenClaw App SDK on macOS with the OpenCoven OpenMeow Dogfood Adapter**

**Track B · Agentic AI & GenAI** | [← Index](README.md) | [Previous → Lab 04](Lab-04-TokenJuice-Output-Compaction.md)

---

**Overview**

This lab tests the OpenClaw App SDK from the point of view of a real external macOS app.

The dogfood client is OpenCoven's `open-meow-sdk` repository.

OpenMeow is described as a native macOS notch/inbox client. The `open-meow-sdk` repo is the adapter and test harness that proves an app can use `@openclaw/sdk` without importing OpenClaw internals.

The core boundary:

```text
OpenMeow macOS app
  -> OpenMeow SDK adapter
  -> @openclaw/sdk
  -> OpenClaw Gateway RPC
  -> OpenClaw runtime
```

This is an app SDK lab, not a plugin SDK lab.

That distinction matters:

```text
App SDK:
  external app talks to Gateway

Plugin SDK:
  code runs inside OpenClaw
```

**Estimated time:** 60-90 minutes

**Difficulty:** Intermediate

---

**What you will test**

The OpenMeow dogfood path focuses on the P0 app happy path:

```text
connect
  -> list agents
  -> create or reuse lane session
  -> send message
  -> stream normalized events
  -> wait for result
  -> cancel active run
  -> map state into app UI
```

The lab has two levels.

| Level | Goal | Requires live OpenClaw Gateway? |
|---|---|---|
| Fixture tests | Validate adapter, events, wait/cancel, UI reducer behavior | No |
| Live smoke test | Connect OpenMeow adapter to a local Gateway | Yes |

Do the fixture tests first.

They are deterministic and catch app-contract regressions before you debug networking.

---

**Prerequisites**

On macOS:

```bash
xcode-select --install
brew install git node pnpm
node --version
pnpm --version
```

Recommended:

- Node.js 20 or newer
- Git
- A terminal with full disk access only if your local agent workflows need it
- Optional: a local OpenClaw Gateway
- Optional: Coven if you also want to test local harness sessions

Do not start with the native macOS UI.

Start with the adapter tests. The app UI should be the last layer you trust.

---

**Step 1 — Clone the dogfood adapter**

```bash
mkdir -p ~/Developer/opencoven-labs
cd ~/Developer/opencoven-labs
git clone https://github.com/OpenCoven/open-meow-sdk.git
cd open-meow-sdk
```

Inspect the repo:

```bash
find . -maxdepth 3 -type f | sort
```

Key files:

| File | Why it matters |
|---|---|
| `src/index.js` | OpenMeow-side adapter over `@openclaw/sdk` |
| `src/index.d.ts` | app-facing TypeScript surface |
| `test/openmeow-sdk-client.test.js` | Node test coverage for adapter behavior |
| `fixtures/openclaw-events/*.jsonl` | normalized Gateway event fixtures |
| `scripts/validate-event-fixtures.mjs` | fixture schema and terminal-event validation |
| `docs/openmeow-sdk-adapter-shape.md` | smallest adapter OpenMeow needs |
| `docs/gateway-rpc-gap-map.md` | SDK method versus Gateway RPC readiness |
| `docs/openmeow-dogfood-plan.md` | phased dogfood plan |

---

**Step 2 — Run the deterministic tests**

The package test command runs fixture validation and Node tests:

```bash
npm test
```

Expected shape:

```text
Validated N fixture events.
ok ...
```

If you want to run the pieces separately:

```bash
node scripts/validate-event-fixtures.mjs
node --test
```

These tests prove that the OpenMeow adapter can:

- wrap the SDK happy path
- list agents
- create lane sessions
- send messages
- stream events
- wait for results
- cancel active runs
- map normalized events into UI state
- keep the composer in send-or-stop mode
- distinguish wait deadline from runtime timeout
- keep unknown events debug-only

This is the most important part of the lab.

If these tests fail, do not debug the macOS UI yet.

---

**Step 3 — Read the adapter contract**

Open the adapter shape:

```bash
sed -n '1,220p' docs/openmeow-sdk-adapter-shape.md
```

The important surface:

```ts
connect()
close()
listAgents()
getAgentIdentity(agentId)
createLaneSession({ agentId, label, sessionKey })
send(sessionKey, message)
events(runId)
wait(runId, timeoutMs)
cancel(runId, sessionKey)
effectiveTools(sessionKey)
```

This is intentionally smaller than the full OpenClaw Gateway.

That is good.

An app should not start with every internal capability.

It should start with the operations the UI actually needs.

---

**Step 4 — Understand the lane model**

OpenMeow uses the idea of lanes.

A lane is an app-facing conversation target:

```text
Lane:
  agentId
  label
  sessionKey
  active run state
  streamed assistant draft
  compact tool activity
  approval cards
```

The session is durable.

The run is active work.

The lane is the UI container.

Mental model:

```text
agent = who should do the work
session = where memory/transcript lives
run = this specific execution
lane = how the macOS app presents it
```

This is the right split for a desktop client.

Do not let the UI confuse session identity with run identity.

---

</details>

## Step 5 — 检查归一化事件 fixture

列出 fixture：

```bash
find fixtures/openclaw-events -type f -maxdepth 1 -print | sort
```

预览一个：

```bash
sed -n '1,20p' fixtures/openclaw-events/happy-path.jsonl
```

每个 fixture 事件都应具备：

- `version`
- `id`
- `ts`
- `type`
- `runId`
- `data`

validator 还要求恰好一个终端 run 事件：

- `run.completed`
- `run.failed`
- `run.cancelled`
- `run.timed_out`

这一点很关键，因为 UI reducer 需要一个确定性的终态。

如果流没有终端事件，app 可能永远空转。

如果流有多个终端事件，UI 状态就会变得不明确。

---

## Step 6 — trace UI reducer

打开测试：

```bash
sed -n '1,260p' test/openmeow-sdk-client.test.js
```

找到这些函数：

```text
mapOpenClawEventToOpenMeowUIEvent
initialOpenMeowUIState
reduceOpenMeowUIState
reduceOpenMeowRunState
markOpenMeowRunCancelling
reduceOpenMeowCancelResult
normalizeOpenMeowWaitResult
```

重要的行为：

| 输入事件 | UI 行为 |
|---|---|
| `run.started` | 激活 run 指示器，stop 可用 |
| `assistant.delta` | 流式 assistant 草稿 |
| `assistant.message` | 已定稿的 assistant 消息 |
| `tool.call.started` | 紧凑 tool 卡片 |
| `tool.call.delta` | tool 预览更新 |
| `tool.call.completed` | tool 卡片已完成 |
| `approval.requested` | 审批卡片 |
| `approval.resolved` | 审批卡片已处理 |
| `run.completed` | composer 回到空闲 |
| `run.cancelled` | composer 回到空闲并处于 cancelled 状态 |
| `run.timed_out` | 终端超时状态 |

UI 规则：

```text
send or stop, never both
```

这是一条实用的产品不变量。

---

## Step 7 — 编写 dogfood 检查清单

创建本地检查清单：

```bash
cat > DOGFOOD-CHECKLIST.md <<'EOF'
# OpenMeow App SDK Dogfood Checklist

## Fixture path

- [ ] `npm test` passes
- [ ] fixture validation passes
- [ ] each fixture has exactly one terminal run event
- [ ] unknown events remain debug-only
- [ ] send-or-stop composer invariant holds
- [ ] wait deadline and runtime timeout are distinct
- [ ] cancel failure leaves the run recoverable

## Live Gateway path

- [ ] Gateway reachable
- [ ] auth configured
- [ ] `agents.list` works
- [ ] session create/reuse works
- [ ] send returns stable `runId` and `sessionKey`
- [ ] events stream without losing early lifecycle events
- [ ] wait returns terminal result or accepted wait deadline
- [ ] cancel returns deterministic UI state
- [ ] approval request renders as a card

## Bugs to report upstream

- missing Gateway RPC:
- event ambiguity:
- wait/cancel mismatch:
- auth/discovery pain:
- native bridge pain:
EOF
```

这份检查清单就是你的测试计划。

Dogfooding 不是“它跑通过一次”。

Dogfooding 是针对真实 app 工作流的可重复压力测试。

---


<details>
<summary>English original</summary>

**Step 5 — Inspect normalized event fixtures**

List fixtures:

```bash
find fixtures/openclaw-events -type f -maxdepth 1 -print | sort
```

Preview one:

```bash
sed -n '1,20p' fixtures/openclaw-events/happy-path.jsonl
```

Every fixture event should have:

- `version`
- `id`
- `ts`
- `type`
- `runId`
- `data`

The validator also expects exactly one terminal run event:

- `run.completed`
- `run.failed`
- `run.cancelled`
- `run.timed_out`

This matters because UI reducers need a deterministic end state.

If a stream has no terminal event, the app may spin forever.

If a stream has multiple terminal events, the UI state becomes ambiguous.

---

**Step 6 — Trace the UI reducer**

Open the test:

```bash
sed -n '1,260p' test/openmeow-sdk-client.test.js
```

Find these functions:

```text
mapOpenClawEventToOpenMeowUIEvent
initialOpenMeowUIState
reduceOpenMeowUIState
reduceOpenMeowRunState
markOpenMeowRunCancelling
reduceOpenMeowCancelResult
normalizeOpenMeowWaitResult
```

The important behaviors:

| Input event | UI behavior |
|---|---|
| `run.started` | active run indicator, stop enabled |
| `assistant.delta` | streaming assistant draft |
| `assistant.message` | finalized assistant message |
| `tool.call.started` | compact tool card |
| `tool.call.delta` | tool preview update |
| `tool.call.completed` | tool card completed |
| `approval.requested` | approval card |
| `approval.resolved` | approval card resolved |
| `run.completed` | composer returns to idle |
| `run.cancelled` | composer returns to idle with cancelled state |
| `run.timed_out` | terminal timeout state |

The UI rule:

```text
send or stop, never both
```

That is a practical product invariant.

---

**Step 7 — Write the dogfood checklist**

Create a local checklist:

```bash
cat > DOGFOOD-CHECKLIST.md <<'EOF'
# OpenMeow App SDK Dogfood Checklist

## Fixture path

- [ ] `npm test` passes
- [ ] fixture validation passes
- [ ] each fixture has exactly one terminal run event
- [ ] unknown events remain debug-only
- [ ] send-or-stop composer invariant holds
- [ ] wait deadline and runtime timeout are distinct
- [ ] cancel failure leaves the run recoverable

## Live Gateway path

- [ ] Gateway reachable
- [ ] auth configured
- [ ] `agents.list` works
- [ ] session create/reuse works
- [ ] send returns stable `runId` and `sessionKey`
- [ ] events stream without losing early lifecycle events
- [ ] wait returns terminal result or accepted wait deadline
- [ ] cancel returns deterministic UI state
- [ ] approval request renders as a card

## Bugs to report upstream

- missing Gateway RPC:
- event ambiguity:
- wait/cancel mismatch:
- auth/discovery pain:
- native bridge pain:
EOF
```

This checklist is your test plan.

Dogfooding is not "it worked once."

Dogfooding is a repeatable pressure test against a real app workflow.

---

</details>

## Step 8 — 可选的 live 网关冒烟测试

仅在 fixture 测试通过后再执行此步骤。

你需要：

- 本地 OpenClaw Gateway
- 一个网关 URL
- auth token 或本地 auth 模式
- 你的环境中可用的 `@openclaw/sdk`

OpenMeow 示例记录了此目标形态：

```bash
pnpm add @openclaw/sdk
OPENCLAW_GATEWAY_URL=http://127.0.0.1:18789 pnpm tsx index.ts
```

将 URL 改为你实际的网关。

创建本地冒烟脚本：

```bash
cat > live-smoke.mjs <<'EOF'
import { OpenClaw } from "@openclaw/sdk";
import { createOpenMeowSDKClient } from "./src/index.js";

const url = process.env.OPENCLAW_GATEWAY_URL;
const token = process.env.OPENCLAW_GATEWAY_TOKEN;
const agentId = process.env.OPENMEOW_AGENT_ID || "default";

if (!url) {
  throw new Error("Set OPENCLAW_GATEWAY_URL first");
}

const oc = new OpenClaw({
  url,
  token,
  requestTimeoutMs: 30_000,
});

const client = createOpenMeowSDKClient({ openClaw: oc });

await client.connect();

try {
  const agents = await client.listAgents();
  console.log("agents:", agents.map((agent) => agent.id || agent.name || agent.label));

  const session = await client.createLaneSession({
    agentId,
    label: "OpenMeow dogfood lane",
  });
  console.log("session:", session);

  const sent = await client.send(
    session.sessionKey,
    "Reply with one short sentence: OpenMeow SDK smoke test is connected."
  );
  console.log("sent:", sent);

  for await (const event of client.events(sent.runId)) {
    console.log("event:", event.type, event.runId || "");
    if (["run.completed", "run.failed", "run.cancelled", "run.timed_out"].includes(event.type)) {
      break;
    }
  }

  const result = await client.wait(sent.runId, 30_000);
  console.log("wait:", result);
} finally {
  await client.close();
}
EOF
```

运行：

```bash
export OPENCLAW_GATEWAY_URL="http://127.0.0.1:18789"
export OPENCLAW_GATEWAY_TOKEN="<token-if-required>"
export OPENMEOW_AGENT_ID="default"
node live-smoke.mjs
```

预期行为：

- 连接到网关
- 列出 agent
- 创建 lane 会话
- 发送一条消息
- 接收事件
- 在一个 terminal run 事件后退出
- wait 结果可与 wait 截止时间区分

如果失败，对失败进行分类：

| 失败 | 可能的 layer |
|---|---|
| package 导入失败 | 本地 SDK 安装/package 问题 |
| 连接被拒绝 | 网关未运行或 URL 错误 |
| auth 被拒绝 | token/auth 模式不匹配 |
| `agents.list` 缺失 | 网关特性/发现不匹配 |
| 无事件 | 事件关联或流问题 |
| wait 一直显示 accepted | run 仍活跃或 wait 截止时间过短 |
| cancel 状态与事件不匹配 | cancel 契约 bug |

---

## Step 9 — 可选的 Coven 本地 harness 检查

此步骤不是 App SDK 验证所必需的，但如果你的 OpenMeow 工作流也想要本地 harness 可见性，则很有用。

Coven 是 OpenCoven 的本地 harness 底座。

它为 Codex、Claude Code 及未来的 harness 提供项目作用域、可观测、可附加的 PTY runtime。

克隆并构建：

```bash
cd ~/Developer/opencoven-labs
git clone https://github.com/OpenCoven/coven.git
cd coven
cargo build --workspace
```

从项目运行基本循环：

```bash
cd /path/to/your/project
coven doctor
coven daemon start
coven run codex "summarize this repository"
coven sessions
coven attach <session-id>
```

如果你未安装 Codex 或 Claude Code，`coven doctor` 仍应有助于识别缺失的 harness。

架构规则：

```text
OpenMeow consumes app state.
Coven supervises local harness sessions.
OpenClaw orchestrates agent runtime through Gateway.
Do not merge these authority boundaries.
```

---

## Step 10 — 报告 dogfood 发现

使用此 issue 模板：

```text
## OpenMeow App SDK Dogfood Finding

### Environment
- macOS version:
- Node version:
- OpenMeow SDK commit:
- OpenClaw version/commit:
- Gateway URL:
- Auth mode:

### Path
- fixture test / live Gateway / Coven harness / native UI

### Expected

### Actual

### Minimal repro

### Layer classification
- App adapter
- @openclaw/sdk
- Gateway RPC
- Gateway auth/discovery
- Event normalization
- wait/cancel semantics
- Approval semantics
- Native bridge
- Coven harness

### Proposed contract change
```

好的 dogfood 反馈应包含最小 repro。

差的反馈仅有：

```text
"the app feels broken"
```

好的反馈：

```text
"run.cancel() returns cancelled immediately, but the stream later emits run.completed.
Here is the fixture. The UI reducer enters idle twice."
```

---


<details>
<summary>English original</summary>

**Step 8 — Optional live Gateway smoke test**

Only do this after the fixture tests pass.

You need:

- a local OpenClaw Gateway
- a Gateway URL
- auth token or local auth mode
- `@openclaw/sdk` available in your environment

The OpenMeow example documents this target shape:

```bash
pnpm add @openclaw/sdk
OPENCLAW_GATEWAY_URL=http://127.0.0.1:18789 pnpm tsx index.ts
```

Adapt the URL to your actual Gateway.

Create a local smoke script:

```bash
cat > live-smoke.mjs <<'EOF'
import { OpenClaw } from "@openclaw/sdk";
import { createOpenMeowSDKClient } from "./src/index.js";

const url = process.env.OPENCLAW_GATEWAY_URL;
const token = process.env.OPENCLAW_GATEWAY_TOKEN;
const agentId = process.env.OPENMEOW_AGENT_ID || "default";

if (!url) {
  throw new Error("Set OPENCLAW_GATEWAY_URL first");
}

const oc = new OpenClaw({
  url,
  token,
  requestTimeoutMs: 30_000,
});

const client = createOpenMeowSDKClient({ openClaw: oc });

await client.connect();

try {
  const agents = await client.listAgents();
  console.log("agents:", agents.map((agent) => agent.id || agent.name || agent.label));

  const session = await client.createLaneSession({
    agentId,
    label: "OpenMeow dogfood lane",
  });
  console.log("session:", session);

  const sent = await client.send(
    session.sessionKey,
    "Reply with one short sentence: OpenMeow SDK smoke test is connected."
  );
  console.log("sent:", sent);

  for await (const event of client.events(sent.runId)) {
    console.log("event:", event.type, event.runId || "");
    if (["run.completed", "run.failed", "run.cancelled", "run.timed_out"].includes(event.type)) {
      break;
    }
  }

  const result = await client.wait(sent.runId, 30_000);
  console.log("wait:", result);
} finally {
  await client.close();
}
EOF
```

Run:

```bash
export OPENCLAW_GATEWAY_URL="http://127.0.0.1:18789"
export OPENCLAW_GATEWAY_TOKEN="<token-if-required>"
export OPENMEOW_AGENT_ID="default"
node live-smoke.mjs
```

Expected behavior:

- connects to Gateway
- lists agents
- creates a lane session
- sends one message
- receives events
- exits after one terminal run event
- wait result is distinguishable from a wait deadline

If this fails, classify the failure:

| Failure | Likely layer |
|---|---|
| package import fails | local SDK install/package issue |
| connection refused | Gateway not running or wrong URL |
| auth rejected | token/auth mode mismatch |
| `agents.list` missing | Gateway feature/discovery mismatch |
| no events | event correlation or stream issue |
| wait says accepted forever | run still active or wait deadline too short |
| cancel state mismatches events | cancel contract bug |

---

**Step 9 — Optional Coven local harness check**

This step is not required for App SDK validation, but it is useful if your OpenMeow workflow also wants local harness visibility.

Coven is the OpenCoven local harness substrate.

It gives Codex, Claude Code, and future harnesses a project-scoped, observable, attachable PTY runtime.

Clone and build:

```bash
cd ~/Developer/opencoven-labs
git clone https://github.com/OpenCoven/coven.git
cd coven
cargo build --workspace
```

Run the basic loop from a project:

```bash
cd /path/to/your/project
coven doctor
coven daemon start
coven run codex "summarize this repository"
coven sessions
coven attach <session-id>
```

If you do not have Codex or Claude Code installed, `coven doctor` should still help identify missing harnesses.

The architecture rule:

```text
OpenMeow consumes app state.
Coven supervises local harness sessions.
OpenClaw orchestrates agent runtime through Gateway.
Do not merge these authority boundaries.
```

---

**Step 10 — Report dogfood findings**

Use this issue template:

```text
## OpenMeow App SDK Dogfood Finding

### Environment
- macOS version:
- Node version:
- OpenMeow SDK commit:
- OpenClaw version/commit:
- Gateway URL:
- Auth mode:

### Path
- fixture test / live Gateway / Coven harness / native UI

### Expected

### Actual

### Minimal repro

### Layer classification
- App adapter
- @openclaw/sdk
- Gateway RPC
- Gateway auth/discovery
- Event normalization
- wait/cancel semantics
- Approval semantics
- Native bridge
- Coven harness

### Proposed contract change
```

Good dogfood feedback should include a minimal repro.

Bad feedback is only:

```text
"the app feels broken"
```

Good feedback:

```text
"run.cancel() returns cancelled immediately, but the stream later emits run.completed.
Here is the fixture. The UI reducer enters idle twice."
```

---

</details>

## Step 11 — 完成前要验证什么

满足以下条件即算完成：

- `npm test` 在 `open-meow-sdk` 中通过
- fixture 校验通过
- 你能解释 adapter API
- 你能解释 wait deadline 与 runtime timeout 的区别
- 你能解释 send-or-stop 状态
- 你能按 layer 归类至少三类可能的失败
- 可选：live Gateway 冒烟测试发出一次 run 并收到一个 terminal event
- 可选：Coven 能启动或诊断一个本地 harness（agent 运行时框架）会话

---

## 这个 Lab 为什么重要

这个 Lab 教的是一种生产级模式：

```text
Use a real app to test the SDK boundary.
```

只能在单元测试里跑通的 SDK 是不够的。

强迫 app 作者了解 Gateway 内部细节的 SDK 同样不够。

OpenMeow dogfood 适配器之所以有用，是因为它提出的是 app 形态的问题：

- 真实 app 能发现 agent 吗？
- 它能创建持久化的 lane 吗？
- 它能流式输出事件而不出现竞态条件吗？
- 它能确定性地停止一次 run 吗？
- 它能紧凑地展示 tool 与 approval 状态吗？
- 它能避免 import runtime 内部实现吗？

正是这些问题，让一个 agent runtime 成为一个平台。

---

## 扩展

1. **新增一个 fixture** — 为 `run.failed` 创建一个带 tool error 的 fixture，并验证 UI reducer 显示出可恢复的错误状态。
2. **新增 approval UI 逻辑** — 扩展 fixture 路径以测试多个未决 approval。
3. **构建一个精简的 Swift bridge 草图** — 把适配器方法映射为 Swift protocol 签名。
4. **新增一个 live cancel 测试** — 启动一个长时间运行的 run，取消它，并验证 stream/wait/cancel 三者收敛一致。
5. **接入 Coven 状态** — 向 OpenMeow lane UI 状态模型添加一张假的 Coven session-status 卡片。

---

## 参考资料

- OpenCoven organization: [https://github.com/OpenCoven](https://github.com/OpenCoven)
- OpenMeow SDK architecture: [https://github.com/OpenCoven/open-meow-sdk](https://github.com/OpenCoven/open-meow-sdk)
- OpenMeow dogfood plan: [https://github.com/OpenCoven/open-meow-sdk/blob/main/docs/openmeow-dogfood-plan.md](https://github.com/OpenCoven/open-meow-sdk/blob/main/docs/openmeow-dogfood-plan.md)
- OpenMeow SDK adapter shape: [https://github.com/OpenCoven/open-meow-sdk/blob/main/docs/openmeow-sdk-adapter-shape.md](https://github.com/OpenCoven/open-meow-sdk/blob/main/docs/openmeow-sdk-adapter-shape.md)
- Gateway RPC gap map: [https://github.com/OpenCoven/open-meow-sdk/blob/main/docs/gateway-rpc-gap-map.md](https://github.com/OpenCoven/open-meow-sdk/blob/main/docs/gateway-rpc-gap-map.md)
- Gateway RPC contract proposals: [https://github.com/OpenCoven/open-meow-sdk/blob/main/docs/rpc-contract-proposals.md](https://github.com/OpenCoven/open-meow-sdk/blob/main/docs/rpc-contract-proposals.md)
- TypeScript basic-run example: [https://github.com/OpenCoven/open-meow-sdk/tree/main/examples/typescript-basic-run](https://github.com/OpenCoven/open-meow-sdk/tree/main/examples/typescript-basic-run)
- Coven local harness substrate: [https://github.com/OpenCoven/coven](https://github.com/OpenCoven/coven)

---

*Lab 05 结束。返回 [Track Index](README.md)。*


<details>
<summary>English original</summary>

**Step 11 — What to verify before calling it done**

You are done when:

- `npm test` passes in `open-meow-sdk`
- fixture validation passes
- you can explain the adapter API
- you can explain the difference between wait deadline and runtime timeout
- you can explain send-or-stop state
- you can classify at least three possible failures by layer
- optional: live Gateway smoke test sends one run and receives one terminal event
- optional: Coven can launch or diagnose a local harness session

---

**Why this lab matters**

This lab teaches a production pattern:

```text
Use a real app to test the SDK boundary.
```

An SDK that works only in unit tests is not enough.

An SDK that forces app authors to know internal Gateway details is also not enough.

The OpenMeow dogfood adapter is useful because it asks app-shaped questions:

- Can a real app discover agents?
- Can it create durable lanes?
- Can it stream events without race conditions?
- Can it stop a run deterministically?
- Can it show tool and approval state compactly?
- Can it avoid importing runtime internals?

These are the questions that make an agent runtime become a platform.

---

**Extensions**

1. **Add one new fixture** — Create a fixture for `run.failed` with a tool error and verify the UI reducer shows a recoverable error state.
2. **Add approval UI logic** — Extend the fixture path to test multiple unresolved approvals.
3. **Build a tiny Swift bridge sketch** — Mirror the adapter methods as Swift protocol signatures.
4. **Add a live cancel test** — Start a long-running run, cancel it, and verify stream/wait/cancel all converge.
5. **Connect Coven state** — Add a fake Coven session-status card to the OpenMeow lane UI state model.

---

**References**

- OpenCoven organization: [https://github.com/OpenCoven](https://github.com/OpenCoven)
- OpenMeow SDK architecture: [https://github.com/OpenCoven/open-meow-sdk](https://github.com/OpenCoven/open-meow-sdk)
- OpenMeow dogfood plan: [https://github.com/OpenCoven/open-meow-sdk/blob/main/docs/openmeow-dogfood-plan.md](https://github.com/OpenCoven/open-meow-sdk/blob/main/docs/openmeow-dogfood-plan.md)
- OpenMeow SDK adapter shape: [https://github.com/OpenCoven/open-meow-sdk/blob/main/docs/openmeow-sdk-adapter-shape.md](https://github.com/OpenCoven/open-meow-sdk/blob/main/docs/openmeow-sdk-adapter-shape.md)
- Gateway RPC gap map: [https://github.com/OpenCoven/open-meow-sdk/blob/main/docs/gateway-rpc-gap-map.md](https://github.com/OpenCoven/open-meow-sdk/blob/main/docs/gateway-rpc-gap-map.md)
- Gateway RPC contract proposals: [https://github.com/OpenCoven/open-meow-sdk/blob/main/docs/rpc-contract-proposals.md](https://github.com/OpenCoven/open-meow-sdk/blob/main/docs/rpc-contract-proposals.md)
- TypeScript basic-run example: [https://github.com/OpenCoven/open-meow-sdk/tree/main/examples/typescript-basic-run](https://github.com/OpenCoven/open-meow-sdk/tree/main/examples/typescript-basic-run)
- Coven local harness substrate: [https://github.com/OpenCoven/coven](https://github.com/OpenCoven/coven)

---

*End of Lab 05. Return to the [Track Index](README.md).*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lab-05-OpenMeow-App-SDK-Dogfood.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lab-05-OpenMeow-App-SDK-Dogfood.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
