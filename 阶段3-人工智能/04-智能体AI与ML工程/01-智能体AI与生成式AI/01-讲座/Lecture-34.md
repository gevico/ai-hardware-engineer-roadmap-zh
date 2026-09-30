---
title: 第 34 讲 - OpenClaw 案例研究：持久化 agent 系统的运维与安全
description: 第 34 讲 - OpenClaw 案例研究：持久化 agent 系统的运维与安全
---

# 第 34 讲 - OpenClaw 案例研究：持久化 agent 系统的运维与安全

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [第 33 讲](Lecture-33.md) | **下一讲：** [第 35 讲](Lecture-35.md)

---

## 为什么要有这一讲

很多 agent 教学到以下内容就结束了：

- 提示词
- 工具
- 记忆
- 也许还有编排

但真实的 agent 产品还必须**保持存活、保持安全、保持可运维**。

OpenClaw 是一个有用的案例研究，因为它记录了：

- 网关的启动与健康
- 监管
- 配对
- 沙箱化
- 工具策略
- 提权执行
- 远程访问

本讲把它上升为一个更大的结论：

> 运维 agent 系统是构建 agent 系统的一部分

---

## 学习目标

学完本讲，你将能够：

1. 解释为什么持久化 agent 需要 day-1 与 day-2 运维。
2. 理解启动、状态、健康与监管之间的区别。
3. 把配对解释为一道审批边界。
4. 理解沙箱、工具策略与提权执行之间的区别。
5. 为 always-on 的 agent 产品设计更安全的运维模型。

---

## 1. 一个 always-on 进程改变一切

OpenClaw 的网关 runbook 给出了一条重要经验：

很多 agent 系统**并不是短命的任务**。

它们是：

- 长生命周期进程
- always-on 服务
- 消息路由器
- 控制面端点

这意味着**工程思维方式**要变。

此时你会关心：

- 启动顺序
- 监管
- 健康
- 重载
- 重启
- 日志
- 密钥
- 配对
- 远程访问

这直接关联到前面几讲中的：

- runtime 纪律
- 确定性启动

这些理念不是纸上谈兵，而是 always-on agent 在生产环境中存活所必需的。

---

## 2. Day-1 与 day-2 运维

这是一个简单但有用的区分。

### Day 1

让系统跑起来：

- 安装
- 配置
- 启动网关
- 接入渠道
- 验证健康状态

### Day 2

让系统保持可靠：

- 安全重启
- 检查日志
- 轮换密钥
- 配对新设备
- 恢复故障渠道
- 检查审计
- 更新配置
- 监控健康

学生往往只学 **Day 1**。

真正的 agent 工程师还必须学 **Day 2**。

---

## 3. 健康不等于「进程存在」

OpenClaw 的 runbook 使用 status 以及面向健康的命令。

这体现了一个成熟的理念：

> 进程在运行并不等于服务是健康的

你需要知道：

- 网关进程是否存活？
- RPC 接口是否响应？
- 渠道是否真的连通？
- agent 是否已加载？
- 后台服务是否健康？

这与确定性启动那一讲里的 **readiness vs liveness** 是同一个理念。

持久化 agent 系统需要：

- 启动检查
- runtime 健康检查
- 可恢复性

没有这些，你只会在**用户抱怨之后**才发现故障。

---

## 4. 配对作为一道审批边界

OpenClaw 的配对模型是这个仓库里最好的教学示例之一。

它把配对用于：

1. **DM 配对** —— 谁被允许和 bot 对话
2. **节点配对** —— 哪些设备被允许加入网关

这是一条有力的经验，因为它说明：

并非每个消息发送者或设备都应被**自动信任**。

通俗地讲：

> 配对是把未知参与者变成被允许参与者的显式审批步骤

这对 agent 产品来说是一个非常通用的模式。

你可以把它用于：

- 聊天发送者
- 移动节点
- 浏览器
- 自动化客户端
- 请求控制权限的设备

这远比下面这种做法好：

> 任何能触达端点的人都能使用 agent

---

## 5. 为什么配对对 AI 系统很重要

在普通的聊天 demo 里，没人会考虑配对。

在真实的持久化 agent 里，它很重要，因为 agent 可能拥有：

- 记忆
- 工具
- 设备控制
- 文件访问
- 对外发消息的能力

所以「谁能和 agent 对话」真正问的是：

> 谁能消耗 agent 的 attention，并可能触发它的权限

这让配对成为一道**安全边界**，而不是 UX 细节。

---

## 6. 沙箱 vs 工具策略 vs 提权执行

这是 OpenClaw 中最有价值的运维经验之一。

这三样东西听起来相似，但其实不是。

### 沙箱

沙箱控制**工具在哪里运行**。

示例：

- 在宿主机上
- 在沙箱容器里

这是一道**执行环境边界**。

### 工具策略

工具策略控制**哪些工具被允许**。

示例：

- `read` 允许
- `write` 拒绝
- `exec` 拒绝

这是一道**可用性边界**。

### 提权执行

提权执行是在常规沙箱规则之外处理 `exec` 风格工作的特殊路径。

这是一道**逃生舱边界**。

最重要的教学点是：

> 这是三个不同的控制层

不要混淆：

- 「工具存在」
- 「工具被允许」
- 「工具在安全的地方运行」

这些是彼此独立的问题。

---


<details>
<summary>English original</summary>

**Lecture 34 - OpenClaw Case Study: Operating and Securing a Persistent Agent System**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 33](Lecture-33.md) | **Next:** [Lecture 35](Lecture-35.md)

---

**Why this lecture exists**

A lot of agent education stops after:

- prompting
- tools
- memory
- maybe orchestration

But a real agent product also has to **stay alive, stay safe, and stay operable**.

OpenClaw is a useful case study because it documents:

- gateway startup and health
- supervision
- pairing
- sandboxing
- tool policy
- elevated execution
- remote access

This lecture turns that into a bigger lesson:

> operating an agent system is part of building an agent system

---

**Learning objectives**

By the end of this lecture you will be able to:

1. Explain why persistent agents need day-1 and day-2 operations.
2. Understand the difference between startup, status, health, and supervision.
3. Explain pairing as an approval boundary.
4. Understand the difference between sandbox, tool policy, and elevated execution.
5. Design a safer operational model for an always-on agent product.

---

**1. One always-on process changes everything**

OpenClaw's gateway runbook teaches an important lesson:

many agent systems are **not short-lived jobs**.

They are:

- long-lived processes
- always-on services
- message routers
- control-plane endpoints

That means the **engineering mindset** changes.

You now care about:

- startup order
- supervision
- health
- reloads
- restarts
- logs
- secrets
- pairing
- remote access

This connects directly to the earlier lectures on:

- runtime discipline
- deterministic startup

Those ideas are not theoretical. They are what an always-on agent needs to survive in production.

---

**2. Day-1 vs day-2 operations**

This is a simple but useful distinction.

**Day 1**

Getting the system up:

- install
- configure
- start the gateway
- connect channels
- verify health

**Day 2**

Keeping the system reliable:

- restart safely
- inspect logs
- rotate secrets
- pair new devices
- recover broken channels
- check audits
- update configuration
- monitor health

Students often learn **Day 1** only.

Real agent engineers must learn **Day 2** as well.

---

**3. Health is not just "the process exists"**

OpenClaw's runbook uses status and health-oriented commands.

That reflects a mature idea:

> a running process is not automatically a healthy service

You need to know:

- is the gateway process alive?
- is the RPC surface responding?
- are channels actually connected?
- are agents loaded?
- are background services healthy?

This is the same idea as **readiness vs liveness** from the deterministic startup lecture.

Persistent agent systems need:

- startup checks
- runtime health checks
- recoverability

Without them, you only notice failure **after users complain**.

---

**4. Pairing as an approval boundary**

OpenClaw's pairing model is one of the best teaching examples in the repo.

It uses pairing for:

1. **DM pairing** — who is allowed to talk to the bot
2. **Node pairing** — which devices are allowed to join the gateway

This is a strong lesson because it shows:

not every message sender or device should be **trusted automatically**.

In plain English:

> pairing is the explicit approval step that turns an unknown actor into an allowed actor

That is a very useful general pattern for agent products.

You can apply it to:

- chat senders
- mobile nodes
- browsers
- automation clients
- devices that request control authority

This is far better than:

> anyone who can reach the endpoint can use the agent

---

**5. Why pairing matters for AI systems**

In a normal chat demo, no one thinks about pairing.

In a real persistent agent, it matters because the agent may have:

- memory
- tools
- device control
- file access
- outbound messaging ability

So "who can talk to the agent" is really:

> who can spend the agent's attention and possibly trigger its authority

That makes pairing a **security boundary**, not a UX detail.

---

**6. Sandbox vs tool policy vs elevated execution**

This is one of the highest-value operational lessons in OpenClaw.

These three things sound similar, but they are not.

**Sandbox**

Sandbox controls **where tools run**.

Example:

- on host
- in a sandboxed container

This is an **execution-environment boundary**.

**Tool policy**

Tool policy controls **which tools are allowed**.

Example:

- `read` allowed
- `write` denied
- `exec` denied

This is an **availability boundary**.

**Elevated execution**

Elevated execution is a special path for `exec`-style work outside the normal sandbox rules.

This is an **escape-hatch boundary**.

The big teaching point is:

> these are three different control layers

Do not confuse:

- "the tool exists"
- "the tool is allowed"
- "the tool runs in a safe place"

Those are separate questions.

---

</details>

## 7. 为什么这一区分很重要

设想一个 coding agent。

你可能会想：

> 如果它被沙箱隔离，就是安全的

但这并不完整。

被沙箱隔离的 agent 仍可能拥有：

- 过多工具
- 通过 bind 获得过多文件访问权限
- 危险的提权路径

或者你可能会想：

> 如果 `exec` 被拒绝，我们就安全了

但 agent 仍可能拥有强大的非 exec 工具。

因此正确的心智模型是**分层**的：

| Layer | Question |
|---|---|
| Sandbox | 执行发生在哪里？ |
| Tool policy | 允许调用什么？ |
| Elevated | 是否存在超出正常边界的例外路径？ |

这正是学生需要尽早掌握的那种专业区分。

---

## 8. 远程访问与信任

OpenClaw 的 gateway 文档推荐使用受控的远程访问方式，例如：

- Tailscale
- VPN
- SSH tunnel

更深层的教训不是「用某个特定的隧道」。

教训是：

> 远程便利性绝不应绕过信任模型

这意味着：

- 认证仍然重要
- 配对仍然重要
- 身份仍然重要
- 日志仍然重要

这对 **local-first 助手**和 **边缘 AI 系统**高度相关。

许多团队错误地假设：

> 它在我本地网络上，所以它是可信的

这不是一个强的安全假设。

---

## 9. 一个好的运维模型

借助 OpenClaw 案例研究，一个成熟的常驻 agent 系统应当具备：

### 启动

- 显式配置加载
- 确定性的启动阶段
- ready/not-ready 状态

### Runtime 健康

- status 端点或命令
- 日志
- channel 就绪检查
- 服务监督

### 安全边界

- 针对发送方和设备的配对
- 沙箱配置
- 工具 allow/deny 策略
- 显式的提权路径控制

### 恢复

- 重启流程
- 密钥重载流程
- 故障 channel 诊断
- 安全的降级行为

这更接近**基础设施工程**，而非玩具式的 prompt engineering。

---

## 10. 示例：Jetson 上的本地家庭助手

假设你在家中的一台 Jetson 上运行一个本地家庭助手。

它支持：

- Telegram 消息
- WebChat
- 一个移动节点
- 笔记搜索
- 日历查询
- 家庭自动化

现在套用 OpenClaw 风格的运维问题：

| Area | Good design choice |
|---|---|
| Startup | gateway 受监督，检查就绪状态 |
| Access | 仅允许已配对的 Telegram 发送方 |
| Devices | 仅允许已批准的移动节点连接 |
| Tools | 允许家庭控制工具，拒绝裸 shell |
| Sandbox | 高风险工具隔离 |
| Elevated | 默认禁用 |
| Remote access | 仅 VPN/Tailscale |
| Logs | 审计操作与 routing 决策 |

这才是思考一个常开 agent 设备应有的方式。

---

## 11. 设计练习

你正在为一个小团队构建一个常驻工程助手。

它具备：

- Slack channel 访问
- Web UI
- 一条 coding 工具链
- 一个部署工具
- 一个用于向操作员告警的移动节点

填写下表：

| Operational area | Your policy |
|---|---|
| Who may message it? | 仅已配对的 Slack workspace 用户 |
| Who may attach devices? | 仅显式批准的节点 |
| Where do tools run? | 默认使用沙箱 |
| Which tools are high-risk? | 部署与 exec 工具 |
| Is elevated execution enabled? | 仅用于可信的操作员路径 |
| How do you inspect health? | gateway status + 日志 + channel 探测 |
| How do you restart safely? | 受监督的服务重启 |

这个练习的价值在于，它迫使你像**操作员**一样思考，而不只是像 prompt 写作者那样思考。

---

## Key takeaways

- 常驻 agent 需要运维纪律，而不只是模型质量。
- 一个正在运行的 process 不等于一个健康的 agent 服务。
- 配对是面向用户和设备的批准边界。
- 沙箱、工具策略与提权执行解决的是不同问题，不应混淆。
- 远程访问必须保持信任模型，而不是绕过它。
- 对于 day-1 和 day-2 的 agent 运维究竟该是什么样，OpenClaw 是一个很有力的案例研究。

---

## References

- 案例研究源仓库：[OpenClaw](https://github.com/openclaw/openclaw)
- OpenClaw 概念：
  - `docs/gateway/index.md`
  - `docs/channels/pairing.md`
  - `docs/gateway/sandbox-vs-tool-policy-vs-elevated.md`
  - `docs/gateway/health.md`

---

*Next: [Lecture 35](Lecture-35.md)*


<details>
<summary>English original</summary>

**7. Why this distinction matters**

Imagine a coding agent.

You might think:

> if it is sandboxed, it is safe

But that is incomplete.

A sandboxed agent may still have:

- too many tools
- too much file access through binds
- dangerous elevated paths

Or you might think:

> if `exec` is denied, we are safe

But the agent might still have powerful non-exec tools.

So the correct mental model is **layered**:

| Layer | Question |
|---|---|
| Sandbox | where does execution happen? |
| Tool policy | what is allowed to be called? |
| Elevated | is there an exception path outside normal boundaries? |

This is exactly the kind of professional distinction students need early.

---

**8. Remote access and trust**

OpenClaw's gateway docs recommend controlled remote access like:

- Tailscale
- VPN
- SSH tunnel

The deeper lesson is not "use this specific tunnel."

The lesson is:

> remote convenience should never bypass the trust model

That means:

- authentication still matters
- pairing still matters
- identity still matters
- logging still matters

This is highly relevant for **local-first assistants** and **edge AI systems**.

Many teams wrongly assume:

> it is on my local network, so it is trusted

That is not a strong security assumption.

---

**9. A good operational model**

Using the OpenClaw case study, a mature persistent agent system should have:

**Startup**

- explicit config loading
- deterministic startup phases
- ready/not-ready status

**Runtime health**

- status endpoint or command
- logs
- channel readiness checks
- service supervision

**Security boundaries**

- pairing for senders and devices
- sandbox configuration
- tool allow/deny policy
- explicit elevated path controls

**Recovery**

- restart procedures
- secrets reload procedures
- broken-channel diagnostics
- safe degraded behavior

This is much closer to **infrastructure engineering** than to toy prompt engineering.

---

**10. Example: a local family assistant on Jetson**

Suppose you run a local family assistant on a Jetson box at home.

It supports:

- Telegram messages
- WebChat
- one mobile node
- note search
- calendar lookup
- home automation

Now apply the OpenClaw-style operational questions:

| Area | Good design choice |
|---|---|
| Startup | gateway supervised, readiness checked |
| Access | only paired Telegram senders allowed |
| Devices | only approved mobile node may connect |
| Tools | home-control tools allowed, raw shell denied |
| Sandbox | risky tools isolated |
| Elevated | disabled by default |
| Remote access | VPN/Tailscale only |
| Logs | audit actions and routing decisions |

This is the right way to think about an always-on agent appliance.

---

**11. Design exercise**

You are building a persistent engineering assistant for a small team.

It has:

- Slack channel access
- Web UI
- one coding toolchain
- one deployment tool
- one mobile node for operator alerts

Fill in this table:

| Operational area | Your policy |
|---|---|
| Who may message it? | paired Slack workspace users only |
| Who may attach devices? | explicitly approved nodes only |
| Where do tools run? | sandbox by default |
| Which tools are high-risk? | deployment and exec tools |
| Is elevated execution enabled? | only for trusted operator paths |
| How do you inspect health? | gateway status + logs + channel probe |
| How do you restart safely? | supervised service restart |

The value of this exercise is that it forces you to think like an **operator**, not only like a prompt writer.

---

**Key takeaways**

- Persistent agents need operational discipline, not only model quality.
- A running process is not the same as a healthy agent service.
- Pairing is an approval boundary for users and devices.
- Sandbox, tool policy, and elevated execution solve different problems and should not be confused.
- Remote access must preserve the trust model, not bypass it.
- OpenClaw is a strong case study for what day-1 and day-2 agent operations really look like.

---

**References**

- Case-study source repo: [OpenClaw](https://github.com/openclaw/openclaw)
- OpenClaw concepts:
  - `docs/gateway/index.md`
  - `docs/channels/pairing.md`
  - `docs/gateway/sandbox-vs-tool-policy-vs-elevated.md`
  - `docs/gateway/health.md`

---

*Next: [Lecture 35](Lecture-35.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-34.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-34.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
