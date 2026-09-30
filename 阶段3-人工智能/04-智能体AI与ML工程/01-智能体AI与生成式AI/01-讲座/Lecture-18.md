---
title: 第 18 讲 - OpenAI Agents SDK：原生沙箱与持久化 Agent harness
description: 第 18 讲 - OpenAI Agents SDK：原生沙箱与持久化 Agent harness
---

# 第 18 讲 - OpenAI Agents SDK：原生沙箱与持久化 Agent harness

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [第 17 讲](Lecture-17.md) | **下一讲：** [第 19 讲](Lecture-19.md)

---

**Agent runtime 基础设施**正在成为产品基础设施。

OpenAI 在 2026 年 4 月发布的 Agents SDK 更新中，重要信号**不仅在于 SDK 能调用工具**。

重要信号在于，**基线 agent 平台**如今需要：

- 文件系统工作区
- 沙箱执行
- shell 工具
- patch 工具
- manifest
- 可恢复状态
- 技能
- MCP
- 仓库指令
- 审批与中断接口
- provider 感知的编排

这正是本课程一路通过 OpenClaw 追踪的方向：

```text
model
  -> harness
  -> tools
  -> sandbox
  -> state
  -> audit and recovery
```

**单靠模型已不再是产品**。

**harness（agent 运行时框架）正在成为产品的一部分**。

---

## 学习目标

学完本讲后，你应当能够：

1. 解释为何面向文件/工具的长期运行 agent 需要 harness。
2. 描述更新后的 OpenAI Agents SDK 沙箱模型。
3. 理解 manifest 作为可移植工作区契约的作用。
4. 解释为何将 harness 与计算分离能提升安全性、持久性与规模。
5. 理解 shell、`apply_patch`、MCP、技能以及 `AGENTS.md` 如何融入 agent runtime。
6. 比较 OpenAI 的 SDK 方向与 OpenClaw 的网关与 node 架构。
7. 指出 provider 原生 harness 在哪些地方有帮助，以及哪些地方仍需应用自身的策略。
8. 为沙箱化的持久化 agent 工作流设计一份最小评估方案。

---

## 1. 转变：从对话调用到 agent harness

早期的 LLM 集成看起来像：

```text
prompt
  -> model
  -> response
```

使用工具的 agent 改变了这一点：

```text
prompt
  -> model
  -> tool call
  -> tool result
  -> model
  -> final response
```

长期运行的 agent 提出了更多要求：

```text
workspace
  -> inspect files
  -> run commands
  -> edit files
  -> checkpoint state
  -> recover after interruption
  -> continue work safely
```

这已不再**只是模型调用**。

这是一个 **runtime 系统**。

OpenAI Agents SDK 的这次更新，把该 runtime 层**产品化**，服务于 OpenAI 模型工作流。

---

## 2. Agents SDK 有哪些变化

OpenAI 称，更新后的 Agents SDK 为处理文档、文件和系统的 agent 增加了一个**能力更强的 harness**。

值得注意的原语：

- 可配置记忆
- 沙箱感知的编排
- 面向文件系统的工具
- MCP 工具使用
- 技能
- `AGENTS.md` 指令
- shell 执行
- `apply_patch` 文件编辑
- 沙箱 manifest
- 快照与重建
- 中断后的可恢复状态

这些正是生产级编码 agent 中出现的**同一批原语**。

其模式是：

```text
agent task
  -> controlled workspace
  -> explicit instructions
  -> bounded tools
  -> durable run state
  -> reviewed artifacts
```

对 OpenClaw 风格架构而言，这验证了一条关键设计命题：

```text
agent systems need a harness, not just a chat loop
```

---

## 3. 原生沙箱执行

更新后的 Agents SDK **原生支持沙箱执行**。

沙箱为 agent 提供一个受控环境，在其中它可以：

- 读取文件
- 写入文件
- 运行代码
- 使用已安装的依赖
- 产生产物
- 在不直接共享应用宿主的情况下运行

OpenAI 的文档把 `SandboxAgent` 描述为这条路径上的 agent 抽象。

简化后的流程：

```text
Manifest
  -> SandboxAgent
  -> sandbox client
  -> Runner.run(...)
  -> artifacts and result state
```

这一点很重要，因为许多有用的 agent 都需要一个**工作区**。

例如：

- 审计数据室
- 检查仓库
- 修补代码
- 聚类支持导出
- 运行测试
- 由挂载的输入生成报告

没有沙箱时，团队往往**自行临时搭建文件系统与执行层**。

这**有风险且不一致**。

---

## 4. manifest 作为工作区契约

SDK 的 `Manifest` 描述初始沙箱工作区。

它可以指定：

- 输入文件
- 目录
- 仓库
- 挂载存储
- 输出目录
- 环境变量值
- 在支持的情况下，沙箱本地的用户与组

关键设计思路：

```text
manifest = fresh-session workspace contract
```

它未必是每个活动沙箱的完整事实来源，因为一次运行可能从以下状态继续：

- 活动沙箱会话
- 序列化的会话状态
- runtime 时选定的快照
- 持久存储

良好的 manifest 设计：

- 把输入产物放进 manifest
- 把输出目录放进 manifest
- 路径保持相对于工作区
- 避免绝对路径与 `..` 逃逸
- 将挂载存储限定在任务范围内
- 不让密钥进入持久化的工作区状态

这相当于沙箱层面的 **API schema**。

它让 agent 的环境**显式且可审查**。

---


<details>
<summary>English original</summary>

**Lecture 18 - OpenAI Agents SDK: Native Sandbox and Durable Agent Harness**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 17](Lecture-17.md) | **Next:** [Lecture 19](Lecture-19.md)

---

**Agent runtime infrastructure** is becoming product infrastructure.

The important signal in OpenAI's April 2026 Agents SDK update is **not only that the SDK can call tools**.

The important signal is that **baseline agent platforms** now need:

- filesystem workspaces
- sandbox execution
- shell tools
- patch tools
- manifests
- resumable state
- skills
- MCP
- repo instructions
- approval and interruption surfaces
- provider-aware orchestration

That is the same direction this course has been tracking through OpenClaw:

```text
model
  -> harness
  -> tools
  -> sandbox
  -> state
  -> audit and recovery
```

The **model alone is no longer the product**.

The **harness is becoming part of the product**.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Explain why file/tool-oriented long-running agents need a harness.
2. Describe the updated OpenAI Agents SDK sandbox model.
3. Understand manifests as portable workspace contracts.
4. Explain why separating harness from compute improves security, durability, and scale.
5. Understand how shell, `apply_patch`, MCP, skills, and `AGENTS.md` fit into an agent runtime.
6. Compare OpenAI's SDK direction with OpenClaw's Gateway and node architecture.
7. Identify where provider-native harnesses help and where application-owned policy is still required.
8. Design a minimal evaluation plan for sandboxed durable agent workflows.

---

**1. The shift: from chat calls to agent harnesses**

Early LLM integrations looked like:

```text
prompt
  -> model
  -> response
```

Tool-using agents changed that:

```text
prompt
  -> model
  -> tool call
  -> tool result
  -> model
  -> final response
```

Long-running agents add more requirements:

```text
workspace
  -> inspect files
  -> run commands
  -> edit files
  -> checkpoint state
  -> recover after interruption
  -> continue work safely
```

That is no longer **just model invocation**.

It is a **runtime system**.

The OpenAI Agents SDK update **productizes that runtime layer** for OpenAI model workflows.

---

**2. What changed in the Agents SDK**

OpenAI describes the updated Agents SDK as adding a **more capable harness** for agents that work with documents, files, and systems.

The notable primitives:

- configurable memory
- sandbox-aware orchestration
- filesystem-oriented tools
- MCP tool use
- skills
- `AGENTS.md` instructions
- shell execution
- `apply_patch` file edits
- sandbox manifests
- snapshotting and rehydration
- resumable state after interruptions

These are the **same primitives** that show up in production coding agents.

The pattern:

```text
agent task
  -> controlled workspace
  -> explicit instructions
  -> bounded tools
  -> durable run state
  -> reviewed artifacts
```

For OpenClaw-style architecture, this validates a key design thesis:

```text
agent systems need a harness, not just a chat loop
```

---

**3. Native sandbox execution**

The updated Agents SDK supports **sandbox execution natively**.

The sandbox gives the agent a controlled environment where it can:

- read files
- write files
- run code
- use installed dependencies
- produce artifacts
- operate without directly sharing the application host

OpenAI's docs describe `SandboxAgent` as the agent abstraction for this path.

The simplified flow:

```text
Manifest
  -> SandboxAgent
  -> sandbox client
  -> Runner.run(...)
  -> artifacts and result state
```

This is important because many useful agents need a **workspace**.

Examples:

- audit a data room
- inspect a repository
- patch code
- cluster support exports
- run tests
- generate a report from mounted inputs

Without a sandbox, teams often **improvise their own filesystem and execution layer**.

That is **risky and inconsistent**.

---

**4. Manifest as workspace contract**

The SDK's `Manifest` describes the initial sandbox workspace.

It can specify:

- input files
- directories
- repositories
- mounted storage
- output directories
- environment values
- sandbox-local users and groups where supported

The key design idea:

```text
manifest = fresh-session workspace contract
```

It is not necessarily the full source of truth for every live sandbox, because a run may continue from:

- a live sandbox session
- serialized session state
- a snapshot chosen at runtime
- persistent storage

Good manifest design:

- put input artifacts in the manifest
- put output directories in the manifest
- keep paths workspace-relative
- avoid absolute paths and `..` escapes
- scope mounted storage to the task
- keep secrets out of persisted workspace state

This is the sandbox equivalent of an **API schema**.

It makes the agent's environment **explicit and reviewable**.

---

</details>

## 5. 文件系统工具、shell 与 apply_patch

OpenAI 的文章点名了 Codex 式文件系统工具。

对编码与文档 agent 而言，有两个原语至关重要：

```text
shell:
  run commands in a sandboxed environment

apply_patch:
  make structured file edits
```


这是一个**重大的产品信号**。

agent 平台正在**收敛到同一套底层工具集**：

- 查看文件
- 搜索文件
- 执行 shell 命令
- 通过 patch 操作编辑文件
- 测试改动
- 产出产物

为什么 `apply_patch` 重要：

```text
free-form file writes are hard to review
patch operations are easier to audit, replay, and constrain
```


为什么 shell 重要：

```text
many real tasks require execution:
tests, scripts, linters, data transforms, builds, profilers
```


但 shell 必须受**策略管控**。

沙箱能缩小**爆炸半径**。

它**并不能免除**对审批、allowlist、日志与网络控制的需求。

---

## 6. 技能与 AGENTS.md

更新后的 SDK 还把编码 agent 实践中已在使用的两类指令面标准化了。

### 技能

OpenAI 的 skills 文档把技能描述为带版本的文件包，外加一份 `SKILL.md` manifest。

技能把流程固化为：

- 风格指南
- 电子表格工作流
- 维护工作流
- 分析 recipe
- 公司规范
- 多步工具使用

模型能看到技能元数据，并在需要时加载完整的 `SKILL.md` 指令。

安全提醒：

```text
skills influence planning, tool use, and command execution
```


应把它们视为**与代码相邻的特权行为**。

### AGENTS.md

`AGENTS.md` 是一种仓库本地的指令文件。

它让工作区能告诉 agent：

- 项目规范
- 测试命令
- 代码风格
- 仓库专属规则
- 安全说明
- 贡献流程

关键的架构要点：

```text
global prompt
  -> agent policy
  -> skill instructions
  -> repo-local instructions
  -> task prompt
```


每一层都需要清晰的**优先级与信任规则**。

---

## 7. MCP 与外部工具

在更新后的 SDK 中，MCP 作为连接工具的标准方式出现。

设计要点：

```text
agent harness
  -> MCP server
  -> external system tools/resources
```


MCP 有助于**把 harness（agent 运行时框架）与每一个工具实现解耦**。

但它也制造了一条**信任边界**。

需要问的问题：

- 谁控制 MCP server？
- 它暴露哪些 scope？
- 它能读取 secret 吗？
- 它能写入外部系统吗？
- 工具 schema 是否精确？
- 破坏性操作是否受审批管控？
- 输出是否被视为不可信？
- 调用是否留有日志？

MCP 是一种**接口**。

它本身**不是安全策略**。

---

## 8. 持久执行：状态、快照与 rehydration

长时间运行的 agent 会失败。

容器会过期。

审批会暂停。

网络会中断。

运行需要人工审查。

更新后的 SDK 强调**持久执行**，做法是把状态与沙箱计算环境分离。

官方文章描述了一种模型：通过快照与 rehydration，把状态外置后，运行可在沙箱失败或过期后继续。

Agents SDK 的 results 文档也暴露了用于中断与审批的状态面：

```text
interruptions:
  pending tool calls that need a decision

state / to_state():
  resumable snapshot to pass back after approval or rejection
```


runtime 设计原则：

```text
compute is replaceable
state is durable
```


这与第 26 讲中的**事件溯源会话模型**一致。

如果沙箱死亡，应用不应丢失：

- 用户意图
- 工具调用历史
- 待处理的审批
- 工作区产物
- agent 归属
- 继续执行状态

---

## 9. 将 harness 与计算分离

OpenAI 明确把 **harness/计算分离** 定位为安全性、持久性与扩展性上的改进。

安全性：

```text
keep credentials out of environments where model-generated code executes
```


持久性：

```text
if a sandbox dies, restore state into a fresh environment
```


扩展性：

```text
use one sandbox or many,
invoke sandboxes only when needed,
route subagents to isolated environments,
parallelize work across containers
```


这应当成为 agent 平台的默认设计原则。

**不要把所有东西都塞进一个进程**，共用一套文件系统和一组凭据。

优先采用：

```text
harness:
  owns identity, policy, state, approvals, routing

compute sandbox:
  owns temporary execution, files, dependencies, artifacts
```


沙箱应当是**一次性的**。

harness 应当是**可审计的**。

---


<details>
<summary>English original</summary>

**5. Filesystem tools, shell, and apply_patch**

The OpenAI article calls out Codex-like filesystem tools.

Two primitives matter for coding and document agents:

```text
shell:
  run commands in a sandboxed environment

apply_patch:
  make structured file edits
```

This is a **major product signal**.

Agent platforms are **converging on the same low-level tool set**:

- inspect files
- search files
- run shell commands
- edit files through patch operations
- test changes
- produce artifacts

Why `apply_patch` matters:

```text
free-form file writes are hard to review
patch operations are easier to audit, replay, and constrain
```

Why shell matters:

```text
many real tasks require execution:
tests, scripts, linters, data transforms, builds, profilers
```

But shell must be **policy-gated**.

A sandbox reduces **blast radius**.

It does **not remove** the need for approvals, allowlists, logs, and network controls.

---

**6. Skills and AGENTS.md**

The updated SDK also standardizes two instruction surfaces that coding agents already use in practice.

**Skills**

OpenAI's skills docs describe a skill as a versioned bundle of files plus a `SKILL.md` manifest.

Skills codify procedures:

- style guides
- spreadsheet workflows
- maintenance workflows
- analysis recipes
- company conventions
- multi-step tool use

The model sees skill metadata and can load the full `SKILL.md` instructions when needed.

Security caveat:

```text
skills influence planning, tool use, and command execution
```

Treat them as **privileged code-adjacent behavior**.

**AGENTS.md**

`AGENTS.md` is a repo-local instruction file.

It lets the workspace tell the agent:

- project conventions
- test commands
- code style
- repository-specific rules
- safety notes
- contribution workflow

The important architecture point:

```text
global prompt
  -> agent policy
  -> skill instructions
  -> repo-local instructions
  -> task prompt
```

You need clear **priority and trust rules** for each layer.

---

**7. MCP and external tools**

MCP appears in the updated SDK as a standard way to connect tools.

The design point:

```text
agent harness
  -> MCP server
  -> external system tools/resources
```

MCP helps **decouple the harness** from every tool implementation.

But it creates a **trust boundary**.

Questions to ask:

- Who controls the MCP server?
- What scopes does it expose?
- Can it read secrets?
- Can it write external systems?
- Are tool schemas precise?
- Are destructive actions approval-gated?
- Are outputs treated as untrusted?
- Are calls logged?

MCP is an **interface**.

It is **not a security policy** by itself.

---

**8. Durable execution: state, snapshots, and rehydration**

Long-running agents fail.

Containers expire.

Approvals pause.

Networks break.

Runs need human review.

The updated SDK emphasizes **durable execution** by separating state from the sandbox compute environment.

The official article describes a model where externalized state allows a run to continue after a sandbox fails or expires, using snapshotting and rehydration.

The Agents SDK results docs also expose state surfaces for interruptions and approvals:

```text
interruptions:
  pending tool calls that need a decision

state / to_state():
  resumable snapshot to pass back after approval or rejection
```

The runtime design principle:

```text
compute is replaceable
state is durable
```

This matches the **event-sourced session model** in Lecture 26.

If the sandbox dies, the application should not lose:

- user intent
- tool-call history
- pending approvals
- workspace artifacts
- agent ownership
- continuation state

---

**9. Separating harness from compute**

OpenAI explicitly frames **harness/compute separation** as a security, durability, and scale improvement.

Security:

```text
keep credentials out of environments where model-generated code executes
```

Durability:

```text
if a sandbox dies, restore state into a fresh environment
```

Scale:

```text
use one sandbox or many,
invoke sandboxes only when needed,
route subagents to isolated environments,
parallelize work across containers
```

This should be a default design principle for agent platforms.

Do **not put everything in one process** with one filesystem and one credential set.

Prefer:

```text
harness:
  owns identity, policy, state, approvals, routing

compute sandbox:
  owns temporary execution, files, dependencies, artifacts
```

The sandbox should be **disposable**.

The harness should be **auditable**.

---

</details>

## 10. OpenAI SDK 与 OpenClaw 网关

OpenAI Agents SDK 与 OpenClaw 解决的是有重叠但层次不同的问题。

OpenAI Agents SDK：

- provider 原生的模型 harness（agent 运行时框架）
- 沙箱感知的编排
- OpenAI 模型对齐
- OpenAI 工具原语
- 新沙箱特性以 Python 优先发布
- 托管式 API 定价与工具用量

OpenClaw 网关：

- 本地优先的控制平面
- channel 与会话
- 设备配对
- 远程节点
- 多 provider 布线
- 本地工具与 app SDK
- 显式的 Gateway RPC 协议
- 用户自有的部署边界

对比：

```text
OpenAI Agents SDK:
  strong provider-native harness for OpenAI model workflows

OpenClaw:
  broader local control plane for multi-channel, multi-node, user-owned agent operation
```

有用的方向**未必是替代**。

而是**集成与架构层面的学习**。

OpenClaw 风格的系统可以学习 SDK 中产品化的沙箱抽象。

OpenAI SDK 应用则可以学习 OpenClaw 显式的网关、配对、威胁模型与节点架构。

---

## 11. 什么成为了基线基础设施

本次发布是一个有用的**市场信号**。

下列能力对严肃的 agent 而言已不再是“高级附加项”：

```text
filesystem workspace
sandbox boundary
shell execution
patch-based edits
tool registry
skills
repo-local instructions
state snapshots
human approval interruptions
observability
artifact review
MCP integration
secrets separation
```

如果 agent 平台缺少这些能力，它多半只是**原型或窄封装**。

生产环境的用户会期望：

- 可靠的续接
- 安全执行
- 可检查的产物
- 有边界的工具访问
- 可回放的历史
- 策略强制执行
- 与自有存储和沙箱 provider 的集成

---

## 12. 安全影响

原生沙箱很有用，但它**并不是完整的安全模型**。

仍然存在的威胁：

- 恶意技能
- 经由文件的 prompt 注入
- 恶意仓库指令
- 不安全的 shell 命令
- 产物外泄
- 过宽的挂载
- 密钥被复制进工作区
- MCP server 被攻陷
- 工具结果注入
- 审批 prompt 操纵

控制措施：

- 最小化 manifest
- 限定范围的挂载
- 尽可能把密钥放在沙箱之外
- 高影响操作需显式审批
- 网络出口策略
- 技能审查
- `AGENTS.md` 的信任规则
- 输出产物审查
- 审计日志
- 快照脱敏
- 确定性的工具策略

把这一点与第 40 讲联系起来：

```text
sandbox is a boundary,
not a trust substitute
```

---

## 13. 评估计划

要评估一个持久化的沙箱化 agent harness，**不要只问它能否完成单个任务**。

评估：

```text
task success:
  correct final artifact

durability:
  can resume after interruption?

sandbox safety:
  can it only access mounted files?

tool correctness:
  did shell/apply_patch calls match policy?

state quality:
  is continuation faithful after rehydration?

artifact quality:
  are outputs inspectable and reproducible?

security:
  does prompt injection fail to cross authority boundaries?

latency/cost:
  does sandbox startup or snapshotting dominate?
```

示例测试：

```text
1. Mount a small repo and task file.
2. Ask the agent to fix a bug.
3. Require it to run tests.
4. Interrupt before a high-impact command.
5. Serialize state.
6. Resume in a fresh sandbox.
7. Verify final patch, logs, and artifacts.
8. Confirm no files outside the manifest were read.
```

这才是新基线应有的测试层级。

---

## 14. 硬件与系统视角

对 AI 硬件工程师而言，本次发布之所以重要，是因为它改变了工作负载的形态。

长时间运行的沙箱化 agent 会带来：

- 突发式推理
- 工具密集的停顿
- 后台模式运行
- 模型调用前后的文件 I/O
- 模型服务端之外的 shell 执行
- 检查点与快照事件
- 并行 subagent 工作负载
- 来自文件与历史记录的更大上下文

GPU 并不是唯一的瓶颈。

完整系统包括：

```text
model serving latency
sandbox startup time
filesystem throughput
network egress
tool runtime
snapshot size
orchestration queueing
approval latency
```

这意味着 agent 性能必须端到端地度量。

使用模型指标，但还要收集：

- 沙箱生命周期时序
- 工具调用时序
- 产物大小
- 恢复时间
- 排队延迟
- token 用量
- 错误/重试次数

---


<details>
<summary>English original</summary>

**10. OpenAI SDK versus OpenClaw Gateway**

OpenAI Agents SDK and OpenClaw solve overlapping but different layers.

OpenAI Agents SDK:

- provider-native model harness
- sandbox-aware orchestration
- OpenAI model alignment
- OpenAI tool primitives
- Python-first release for new sandbox features
- managed API pricing and tool usage

OpenClaw Gateway:

- local-first control plane
- channels and sessions
- device pairing
- remote nodes
- multi-provider routing
- local tools and app SDKs
- explicit Gateway RPC protocol
- user-owned deployment boundary

Comparison:

```text
OpenAI Agents SDK:
  strong provider-native harness for OpenAI model workflows

OpenClaw:
  broader local control plane for multi-channel, multi-node, user-owned agent operation
```

The useful direction is **not necessarily replacement**.

It is **integration and architectural learning**.

An OpenClaw-style system can learn from the SDK's productized sandbox abstractions.

An OpenAI SDK app can learn from OpenClaw's explicit gateway, pairing, threat model, and node architecture.

---

**11. What becomes baseline infrastructure**

This release is a useful **market signal**.

The following are no longer "advanced extras" for serious agents:

```text
filesystem workspace
sandbox boundary
shell execution
patch-based edits
tool registry
skills
repo-local instructions
state snapshots
human approval interruptions
observability
artifact review
MCP integration
secrets separation
```

If an agent platform lacks these, it is probably a **prototype or a narrow wrapper**.

Production users will expect:

- reliable continuation
- safe execution
- inspectable artifacts
- bounded tool access
- replayable history
- policy enforcement
- integration with their own storage and sandbox providers

---

**12. Security implications**

Native sandboxing is useful, but it is **not a complete security model**.

Threats remain:

- malicious skills
- prompt injection through files
- malicious repository instructions
- unsafe shell commands
- artifact exfiltration
- overbroad mounts
- secrets copied into workspace
- MCP server compromise
- tool-result injection
- approval prompt manipulation

Controls:

- minimal manifests
- scoped mounts
- secrets outside sandbox when possible
- explicit approval for high-impact actions
- network egress policy
- skill review
- `AGENTS.md` trust rules
- output artifact review
- audit logs
- snapshot redaction
- deterministic tool policy

Tie this back to Lecture 40:

```text
sandbox is a boundary,
not a trust substitute
```

---

**13. Evaluation plan**

To evaluate a durable sandboxed agent harness, do **not only ask whether it can complete one task**.

Evaluate:

```text
task success:
  correct final artifact

durability:
  can resume after interruption?

sandbox safety:
  can it only access mounted files?

tool correctness:
  did shell/apply_patch calls match policy?

state quality:
  is continuation faithful after rehydration?

artifact quality:
  are outputs inspectable and reproducible?

security:
  does prompt injection fail to cross authority boundaries?

latency/cost:
  does sandbox startup or snapshotting dominate?
```

Example test:

```text
1. Mount a small repo and task file.
2. Ask the agent to fix a bug.
3. Require it to run tests.
4. Interrupt before a high-impact command.
5. Serialize state.
6. Resume in a fresh sandbox.
7. Verify final patch, logs, and artifacts.
8. Confirm no files outside the manifest were read.
```

That is the right level of test for the new baseline.

---

**14. Hardware and systems view**

For AI hardware engineers, this release matters because it changes workload shape.

Long-running sandboxed agents create:

- bursty inference
- tool-heavy pauses
- background mode runs
- file I/O around model calls
- shell execution outside the model server
- checkpoint and snapshot events
- parallel subagent workloads
- larger context from files and histories

The GPU is not the only bottleneck.

The full system includes:

```text
model serving latency
sandbox startup time
filesystem throughput
network egress
tool runtime
snapshot size
orchestration queueing
approval latency
```

That means agent performance must be measured end-to-end.

Use model metrics, but also collect:

- sandbox lifecycle timing
- tool-call timing
- artifact size
- resume time
- queue delay
- token usage
- error/retry counts

---

</details>

## Mini-lab：持久化沙箱 agent 设计

使用更新后的 harness（agent 运行时框架）模型设计一个小型 agent 应用。

场景：

```text
An agent reviews a repository, edits one file, runs tests,
and produces a patch plus a short report.
```

指定：

```text
Manifest:
  mounted repo:
  output directory:
  task file:
  environment:

Tools:
  shell:
  apply_patch:
  MCP:
  skills:

Policy:
  allowed commands:
  denied commands:
  approval-required commands:
  network policy:

Durability:
  snapshot trigger:
  state serialization:
  rehydration test:

Security:
  untrusted files:
  secrets boundary:
  prompt-injection test:

Evidence:
  logs:
  final patch:
  test output:
  artifact review:
```

然后编写验收标准：

```text
Task passes only if:
  tests pass
  patch is minimal
  state resumes correctly after interruption
  no out-of-scope file access occurs
  artifact report cites exact files changed
```

---

## 关键要点

- OpenAI Agents SDK 的这次更新，把严肃 agent 系统本就必需的基础设施做成了产品。
- 原生沙箱执行让 agent 拥有受控工作区，用于文件、工具、依赖与产物。
- Manifest 让沙箱工作区变得显式、可移植、可审查。
- Shell 与 `apply_patch` 正成为面向文件的 agent 的基线工具。
- 技能、MCP 与 `AGENTS.md` 把可复用指令和工具生态标准化。
- 长时间运行的工作需要持久化状态、中断、快照与 rehydration。
- 把 harness 与 compute 分离，可提升安全性、持久性与规模。
- provider 原生 harness 很有用，但应用自有的策略、威胁建模与评测仍不可少。
- OpenClaw 与 OpenAI Agents SDK 指向同一个基线：agent 需要模型之外有一个真正的 runtime。

---

## 参考文献

- OpenAI，"The next evolution of the Agents SDK"：[https://openai.com/index/the-next-evolution-of-the-agents-sdk/](https://openai.com/index/the-next-evolution-of-the-agents-sdk/)
- OpenAI Agents SDK 指南：[https://developers.openai.com/api/docs/guides/agents](https://developers.openai.com/api/docs/guides/agents)
- OpenAI Sandbox Agents 指南：[https://developers.openai.com/api/docs/guides/agents/sandboxes](https://developers.openai.com/api/docs/guides/agents/sandboxes)
- OpenAI Results and state 指南：[https://developers.openai.com/api/docs/guides/agents/results](https://developers.openai.com/api/docs/guides/agents/results)
- OpenAI 技能指南：[https://developers.openai.com/api/docs/guides/tools-skills](https://developers.openai.com/api/docs/guides/tools-skills)
- Lecture 26 - Event-Sourced Agent State：[Lecture-26.md](Lecture-26.md)
- Lecture 21 - Agent Skills：[Lecture-21.md](Lecture-21.md)
- Lecture 40 - OpenClaw Threat Model：[Lecture-40.md](Lecture-40.md)

---

*下一步：[Lecture 19](Lecture-19.md)*


<details>
<summary>English original</summary>

**Mini-lab: durable sandbox agent design**

Design a small agent app using the updated harness model.

Scenario:

```text
An agent reviews a repository, edits one file, runs tests,
and produces a patch plus a short report.
```

Specify:

```text
Manifest:
  mounted repo:
  output directory:
  task file:
  environment:

Tools:
  shell:
  apply_patch:
  MCP:
  skills:

Policy:
  allowed commands:
  denied commands:
  approval-required commands:
  network policy:

Durability:
  snapshot trigger:
  state serialization:
  rehydration test:

Security:
  untrusted files:
  secrets boundary:
  prompt-injection test:

Evidence:
  logs:
  final patch:
  test output:
  artifact review:
```

Then write the acceptance criteria:

```text
Task passes only if:
  tests pass
  patch is minimal
  state resumes correctly after interruption
  no out-of-scope file access occurs
  artifact report cites exact files changed
```

---

**Key takeaways**

- The OpenAI Agents SDK update productizes infrastructure that serious agent systems already need.
- Native sandbox execution gives agents controlled workspaces for files, tools, dependencies, and artifacts.
- Manifests make sandbox workspaces explicit, portable, and reviewable.
- Shell and `apply_patch` are becoming baseline tools for file-oriented agents.
- Skills, MCP, and `AGENTS.md` standardize reusable instructions and tool ecosystems.
- Durable state, interruptions, snapshotting, and rehydration are required for long-running work.
- Separating harness from compute improves security, durability, and scale.
- Provider-native harnesses are useful, but application-owned policy, threat modeling, and evals remain necessary.
- OpenClaw and OpenAI Agents SDK point toward the same baseline: agents need a real runtime around the model.

---

**References**

- OpenAI, "The next evolution of the Agents SDK": [https://openai.com/index/the-next-evolution-of-the-agents-sdk/](https://openai.com/index/the-next-evolution-of-the-agents-sdk/)
- OpenAI Agents SDK guide: [https://developers.openai.com/api/docs/guides/agents](https://developers.openai.com/api/docs/guides/agents)
- OpenAI Sandbox Agents guide: [https://developers.openai.com/api/docs/guides/agents/sandboxes](https://developers.openai.com/api/docs/guides/agents/sandboxes)
- OpenAI Results and state guide: [https://developers.openai.com/api/docs/guides/agents/results](https://developers.openai.com/api/docs/guides/agents/results)
- OpenAI Skills guide: [https://developers.openai.com/api/docs/guides/tools-skills](https://developers.openai.com/api/docs/guides/tools-skills)
- Lecture 26 - Event-Sourced Agent State: [Lecture-26.md](Lecture-26.md)
- Lecture 21 - Agent Skills: [Lecture-21.md](Lecture-21.md)
- Lecture 40 - OpenClaw Threat Model: [Lecture-40.md](Lecture-40.md)

---

*Next: [Lecture 19](Lecture-19.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-18.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-18.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
