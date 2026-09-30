---
title: 第 29 讲 - 智能体化 SDLC：快速探索，安全交付
description: 第 29 讲 - 智能体化 SDLC：快速探索，安全交付
---

# 第 29 讲 - 智能体化 SDLC：快速探索，安全交付

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [第 28 讲](Lecture-28.md) | **下一讲：** [第 30 讲](Lecture-30.md)

---

第 21 讲聚焦于 agent 技能：

```text
agents skip discipline
  -> encode senior-engineering workflows
  -> require checkpoints, evidence, tests, and scope control
```

本讲从相反的一面切入：

```text
code is cheaper now
  -> use implementation as exploration
  -> preserve what matters: tests, intent, specs, security, and taste
```

这种张力正是关键所在：

```text
Explore fast.
Ship safely.
```

强大的 agent 系统**两者都支持**。

---

## 学习目标

学完本讲，你应当能够：

1. 解释为什么廉价的代码会改变软件流程的经济学。
2. 把探索代码与交付代码区分开。
3. 把测试与意图当作持久资产。
4. 解释为什么当 agent 能快速重写内部实现时，端到端行为测试更为重要。
5. 让 spec 与实现保持同步，而不是事先将其冻结。
6. 辨别哪些工作应当自动化，哪些工作仍需人的品味。
7. 设计双模式 agent 工作流：探索模式与稳定模式。
8. 将此工作流应用到 OpenClaw、端侧 AI 与硬件工程。

---

## 1. 核心转变

传统软件经济学假设：

| 旧世界 | 实际影响 |
|---|---|
| 写代码很贵 | 编码前仔细规划 |
| 重写很贵 | 避免大型实验 |
| 测试像是额外开销 | 实现压力缓解后才测试 |
| spec 是前置产物 | 写一次，然后实现 |

智能体化编码改变了成本结构：

| 智能体化世界 | 实际影响 |
|---|---|
| 写代码很便宜 | 以实现促学习 |
| 重建更便宜 | 并行尝试多种设计 |
| 测试成为资产 | 行为契约让内部实现可变更 |
| spec 是持续的 | 随学习进展更新意图 |

瓶颈从**写代码转向判断代码**：

```text
Do we know what is worth building?
Can we tell when it is correct?
Can we maintain what we generated?
Can we keep it safe?
```

---

## 2. 代码即探索

**“以实现促学习”**是关键理念。

有时，直到做出一个**粗糙版本**，你才知道正确的设计是什么。

在以下方面尤其如此：

- UI 工作流
- agent 循环
- 流式事件协议
- 检索质量
- 延迟路径
- 硬件 bring-up 脚本
- 部署自动化
- 开发者体验

原型代码成为一种**探针**：

```text
implementation -> feedback -> updated intent
```

你构建一个切片，用来发现：

- 遗漏的需求
- 隐藏状态
- 糟糕的抽象
- UX 摩擦
- 可测试性问题
- 性能瓶颈
- 安全假设

然后你决定**保留什么**。

---

## 3. 廉价的代码仍有昂贵的后果

廉价的生成并不能让软件**免费**。

它把成本**转移**到：

- 评审
- 验证
- 维护
- 支持
- 安全
- 事故响应
- 用户信任
- 文档
- 运维职责

实用规则是：

```text
Treat exploratory code as disposable.
Treat tests and intent as assets.
```

这就是为什么**没有契约的“vibe coding”**会很快失效。

agent 可以在**几分钟内**生成一个功能。

团队仍要**为这些 bug 负责数月**。

---

## 4. 与 Agent Skills 的综合

第 21 讲与本讲的结合方式如下：

| 关注点 | 智能体化 SDLC | Agent Skills |
|---|---|---|
| 探索 | 以实现促学习，频繁重建 | 不是主要关注点 |
| 纪律 | 维护是真实的 | 工作流检查点 |
| 测试 | 持久的行为契约 | 强制达成标准 |
| spec | 持续同步 | 结构化入口点 |
| 安全 | 廉价的代码不能消除风险 | 反合理化与策略闸门 |
| 人的角色 | 品味与经验成为瓶颈 | 评审、范围界定与验证纪律 |

组合循环：

```text
EXPLORE
  -> build cheap prototypes
  -> learn from behavior
  -> update intent

LOCK IN
  -> turn useful behavior into tests
  -> update specs
  -> define constraints

STABILIZE
  -> apply skills
  -> verify
  -> review diff

SHIP
  -> release with evidence
  -> monitor and maintain
```

这就是**智能体化 SDLC**。

---

## 5. 探索模式 vs 稳定模式

不要**对每个阶段套用同一套规则**。

### 探索模式

| 属性 | 规则 |
|---|---|
| 目标 | 快速学习 |
| 代码质量 | 粗糙可以接受 |
| 范围 | 允许更宽泛的实验 |
| 测试 | 轻量探针或黄金样例 |
| 输出 | 笔记、截图、trace、候选设计 |
| 人工评审 | 频繁的方向检查 |


<details>
<summary>English original</summary>

**Lecture 29 - Agentic SDLC: Explore Fast, Ship Safely**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 28](Lecture-28.md) | **Next:** [Lecture 30](Lecture-30.md)

---

Lecture 21 focused on agent skills:

```text
agents skip discipline
  -> encode senior-engineering workflows
  -> require checkpoints, evidence, tests, and scope control
```

This lecture starts from the opposite side:

```text
code is cheaper now
  -> use implementation as exploration
  -> preserve what matters: tests, intent, specs, security, and taste
```

The tension is the point:

```text
Explore fast.
Ship safely.
```

Strong agent systems **support both**.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Explain why cheap code changes software-process economics.
2. Separate exploration code from shipping code.
3. Treat tests and intent as persistent assets.
4. Explain why end-to-end behavior tests matter more when agents can rewrite internals quickly.
5. Keep specs synchronized with implementation instead of freezing them upfront.
6. Identify which work should be automated and which work still requires human taste.
7. Design a dual-mode agent workflow: explore mode and stabilize mode.
8. Apply this workflow to OpenClaw, on-device AI, and hardware engineering.

---

**1. The core shift**

Traditional software economics assumed:

| Old world | Practical effect |
|---|---|
| Writing code is expensive | plan carefully before coding |
| Rewriting is expensive | avoid large experiments |
| Tests feel like overhead | test after implementation pressure allows it |
| Specs are upfront artifacts | write once, then implement |

Agentic coding shifts the cost structure:

| Agentic world | Practical effect |
|---|---|
| Writing code is cheap | implement to learn |
| Rebuilding is cheaper | try parallel designs |
| Tests become the asset | behavior contracts let internals change |
| Specs are continuous | update intent as learning happens |

The bottleneck moves from **typing code to judging code**:

```text
Do we know what is worth building?
Can we tell when it is correct?
Can we maintain what we generated?
Can we keep it safe?
```

---

**2. Code as exploration**

**"Implement to learn"** is the key idea.

Sometimes you do not know the right design until you build a **rough version**.

This is especially true for:

- UI workflows
- agent loops
- streaming event protocols
- retrieval quality
- latency paths
- hardware bring-up scripts
- deployment automation
- developer experience

Prototype code becomes a **probe**:

```text
implementation -> feedback -> updated intent
```

You build a slice to discover:

- missing requirements
- hidden state
- bad abstractions
- UX friction
- testability problems
- performance bottlenecks
- security assumptions

Then you decide **what to keep**.

---

**3. Cheap code still has expensive consequences**

Cheap generation does not make software **free**.

It **moves cost** into:

- review
- verification
- maintenance
- support
- security
- incident response
- user trust
- documentation
- operational ownership

The practical rule:

```text
Treat exploratory code as disposable.
Treat tests and intent as assets.
```

This is why **"vibe coding" without contracts** breaks down quickly.

The agent can generate a feature **in minutes**.

The team still owns the bugs **for months**.

---

**4. The synthesis with Agent Skills**

Lecture 21 and this lecture fit together like this:

| Concern | Agentic SDLC | Agent Skills |
|---|---|---|
| Exploration | implement to learn, rebuild often | not the main focus |
| Discipline | maintenance is real | workflow checkpoints |
| Tests | persistent behavioral contracts | mandatory exit criteria |
| Specs | continuously synchronized | structured entry point |
| Safety | cheap code does not remove risk | anti-rationalization and policy gates |
| Human role | taste and experience become bottlenecks | review, scope, and verification discipline |

Combined loop:

```text
EXPLORE
  -> build cheap prototypes
  -> learn from behavior
  -> update intent

LOCK IN
  -> turn useful behavior into tests
  -> update specs
  -> define constraints

STABILIZE
  -> apply skills
  -> verify
  -> review diff

SHIP
  -> release with evidence
  -> monitor and maintain
```

This is the **agentic SDLC**.

---

**5. Explore mode vs stabilize mode**

Do not use the **same rules for every phase**.

**Explore mode**

| Property | Rule |
|---|---|
| Goal | learn quickly |
| Code quality | rough is acceptable |
| Scope | broader experiments allowed |
| Tests | lightweight probes or golden examples |
| Output | notes, screenshots, traces, candidate designs |
| Human review | frequent direction checks |

</details>

### 稳定模式

| 属性 | 规则 |
|---|---|
| 目标 | 让选定的行为可以安全发布 |
| 代码质量 | 可维护、可评审 |
| 范围 | 窄、明确、已批准 |
| 测试 | 必需的行为契约 |
| 产出 | 小 diff、证据、风险说明 |
| 人工审查 | 最终工程评审 |

示例：

```text
"Try three ways to implement local voice activity detection."
  -> explore mode

"Make the selected VAD implementation production-ready."
  -> stabilize mode
```

---

## 6. 测试作为稳定性层

当代码容易被重写时，测试变得**更重要**。

原因：

```text
tests preserve behavior while agents rewrite implementation
```

有用的智能体化测试往往是**行为级**的：

- 用户旅程测试
- API 契约测试
- CLI 冒烟测试
- 事件流契约测试
- 产物形态测试
- 与模型无关的 harness（agent 运行时框架）测试
- 硬件可观测状态测试

对于 OpenClaw 风格的系统：

| 领域 | 有用的契约 |
|---|---|
| Gateway RPC | 请求/响应 schema 与事件顺序 |
| App SDK | 规范化的事件形态与 wait/cancel 行为 |
| cron | 在创建 job 前拒绝无效调度 |
| node transport | node 命令必须已声明且被允许 |
| tool policy | 被拒绝的工具按 fail closed 处理 |
| 系统提示词 | 预期章节存在且不泄露机密 |

测试应回答：

```text
What must remain true if the implementation changes?
```

---

## 7. 意图文档

测试说明**什么能工作**。

代码说明**如何工作**。

规格说明**系统应该做什么**。

意图解释**为什么**。

agent 需要意图，因为除非你把它写下来，否则它们不具备**持久的产品判断力**。

好的意图文档包含：

- 为什么存在这个设计
- 被否决的替代方案
- 已接受的取舍
- 哪些东西不能被优化掉
- 哪些未来工作被有意推迟

示例：

```markdown
# Intent: Gateway RPC Event Normalization

We normalize raw Gateway frames in the App SDK because external apps need a
stable event contract. Apps should not parse internal runtime frames directly.

Rejected alternative:
- expose raw frames only

Reason:
- raw frames create fragile UI integrations and make runtime changes risky

Must preserve:
- unknown raw frames remain available for advanced users
- stable event envelope stays versioned
```

这对未来的 agent 是**高价值上下文**。

---

## 8. 规格必须演进

实现开始后，静态规格**往往是错的**。

智能体化开发会暴露出：

- API 边界情况
- 缺失的权限状态
- 可测试性约束
- 模型行为问题
- 未考虑到的 UI 状态
- 硬件时序问题

持续规格规则：

```text
Every meaningful implementation discovery should update:
- acceptance criteria
- non-goals
- constraints
- test plan
- open risks
```

这**不是官僚主义**。

它**保存学习成果**。

---

## 9. 人的品味成为限制因素

当代码到来的速度快于外部反馈时，**判断力成为瓶颈**。

品味意味着知道：

- 好的样子是什么
- 哪种复杂度不值得
- 原型什么时候在撒谎
- UX 何时显得别扭
- 抽象何时过早
- 测试何时过于脆弱
- 安全风险何时被含糊带过

agent **放大品味**。

它们**不能替代品味**。

更好的工程师能从 agent 身上获得更多，因为他们：

- 精确界定任务
- 约束搜索空间
- 更快发现薄弱答案
- 识别意外复杂度
- 指出缺失的验证

---

## 10. 把简单的事自动化

好的自动化对象：

- 格式化
- lint
- 测试选择
- 冒烟测试执行
- 依赖检查
- 文档构建检查
- API schema 生成
- 截图捕获
- 事件 fixture 回放
- 日志摘要

重复出现的经验教训应当变成：

```text
habit -> checklist -> skill -> hook -> CI gate
```

示例：

```text
Agent repeatedly forgets to run mkdocs build.
  -> add docs-build skill
  -> add final-answer evidence check
  -> add CI gate
```

---

## 11. 双模式 agent 设计

一个实用的编程 agent 应当支持**两种显式模式**。

### 探索模式

目的：

```text
learn quickly, compare options, surface hidden constraints
```

允许的行为：

- 构建一次性的原型
- 比较不同方案
- 运行快速探测
- 产出笔记与取舍表
- 在稳定化之前向人征求方向

必需的产出：

```text
what was tried
what was learned
which option is recommended
what evidence supports it
what should be discarded
```

### 稳定模式

目的：

```text
turn selected behavior into reviewable, maintainable code
```

必需的行为：

- 更新规格
- 添加或更新测试
- 保持 diff 范围受控
- 运行验证
- 记录剩余风险
- 产出可供评审的总结

必需的产出：

```text
files changed
tests run
evidence captured
scope changes
known risks
next action
```

---


<details>
<summary>English original</summary>

**Stabilize mode**

| Property | Rule |
|---|---|
| Goal | make selected behavior safe to ship |
| Code quality | maintainable and reviewable |
| Scope | narrow, explicit, approved |
| Tests | required behavior contracts |
| Output | small diff, evidence, risk note |
| Human review | final engineering review |

Example:

```text
"Try three ways to implement local voice activity detection."
  -> explore mode

"Make the selected VAD implementation production-ready."
  -> stabilize mode
```

---

**6. Tests as the stability layer**

When code is easy to rewrite, tests become **more important**.

Reason:

```text
tests preserve behavior while agents rewrite implementation
```

Useful agentic tests are often **behavior-level**:

- user journey tests
- API contract tests
- CLI smoke tests
- event-stream contract tests
- artifact-shape tests
- model-independent harness tests
- hardware observable-state tests

For OpenClaw-style systems:

| Area | Useful contract |
|---|---|
| Gateway RPC | request/response schema and event ordering |
| App SDK | normalized event shapes and wait/cancel behavior |
| cron | invalid schedules rejected before job creation |
| node transport | node command must be declared and allowed |
| tool policy | denied tools fail closed |
| system prompt | expected sections present without leaking secrets |

The test should answer:

```text
What must remain true if the implementation changes?
```

---

**7. Intent documentation**

Tests say **what works**.

Code says **how it works**.

Specs say **what the system should do**.

Intent explains **why**.

Agents need intent because they do not have **durable product judgment** unless you write it down.

Good intent docs include:

- why this design exists
- alternatives rejected
- tradeoffs accepted
- what must not be optimized away
- what future work is intentionally deferred

Example:

```markdown
# Intent: Gateway RPC Event Normalization

We normalize raw Gateway frames in the App SDK because external apps need a
stable event contract. Apps should not parse internal runtime frames directly.

Rejected alternative:
- expose raw frames only

Reason:
- raw frames create fragile UI integrations and make runtime changes risky

Must preserve:
- unknown raw frames remain available for advanced users
- stable event envelope stays versioned
```

This is **high-value context** for future agents.

---

**8. Specs must evolve**

A static spec is **often wrong** after implementation begins.

Agentic development reveals:

- API edge cases
- missing permission states
- testability constraints
- model behavior issues
- UI states not considered
- hardware timing problems

Continuous spec rule:

```text
Every meaningful implementation discovery should update:
- acceptance criteria
- non-goals
- constraints
- test plan
- open risks
```

This is **not bureaucracy**.

It **preserves learning**.

---

**9. Human taste becomes the limiter**

When code arrives faster than external feedback, **judgment becomes the bottleneck**.

Taste means knowing:

- what good looks like
- which complexity is not worth it
- when a prototype is lying
- when UX is awkward
- when an abstraction is premature
- when a test is too brittle
- when security risk is being hand-waved

Agents **amplify taste**.

They do **not replace it**.

Better engineers get more from agents because they:

- frame tasks precisely
- constrain the search space
- detect weak answers faster
- recognize accidental complexity
- identify missing verification

---

**10. Automate the easy stuff**

Good automation targets:

- formatting
- linting
- test selection
- smoke test execution
- dependency checks
- docs build checks
- API schema generation
- screenshot capture
- event fixture replay
- log summarization

Repeated lessons should become:

```text
habit -> checklist -> skill -> hook -> CI gate
```

Example:

```text
Agent repeatedly forgets to run mkdocs build.
  -> add docs-build skill
  -> add final-answer evidence check
  -> add CI gate
```

---

**11. Dual-mode agent design**

A practical coding agent should support **two explicit modes**.

**Explore mode**

Purpose:

```text
learn quickly, compare options, surface hidden constraints
```

Allowed behavior:

- build throwaway prototypes
- compare approaches
- run quick probes
- produce notes and tradeoff tables
- ask for human direction before stabilizing

Required output:

```text
what was tried
what was learned
which option is recommended
what evidence supports it
what should be discarded
```

**Stabilize mode**

Purpose:

```text
turn selected behavior into reviewable, maintainable code
```

Required behavior:

- update spec
- add or update tests
- keep diff scoped
- run verification
- document remaining risk
- produce review-ready summary

Required output:

```text
files changed
tests run
evidence captured
scope changes
known risks
next action
```

---

</details>

## 12. OpenClaw 映射

在 OpenClaw 风格的 runtime 中：

| SDLC 关注点 | runtime 原语 |
|---|---|
| 探索模式 | 隔离会话或沙箱工作区 |
| 稳定模式 | 带更严格工具的主项目会话 |
| 测试即契约 | 工具执行加上捕获的运行输出 |
| 意图文档 | 工作区引导文件或项目文档 |
| 规范同步 | 会话记忆与项目 markdown 更新 |
| 范围纪律 | 文件策略、diff 审查、审批钩子 |
| 证据 | 产物、日志、截图、运行事件 |
| 人的品味 | 审批 UI、仪表盘、审查界面 |
| 长时间运行的工作 | cron、会话、任务账本 |

有用的命令词汇：

```text
/explore "Try three possible implementations"
/choose "Select option B and explain why"
/stabilize "Make option B production-ready"
/verify "Run the contract checks"
/review "Inspect the diff and risks"
```

runtime 应在 run 元数据中记录模式。

审查者需要知道他们看的是实验输出还是可交付输出。

---

## 13. 端侧 AI 示例

任务：

```text
Improve wake-word responsiveness without increasing false positives.
```

探索模式：

```text
1. Try three VAD/wake-word pipeline variants.
2. Measure latency on short sample clips.
3. Track CPU/GPU usage.
4. Record false-positive behavior on noisy clips.
5. Recommend one candidate.
```

稳定模式：

```text
1. Update the selected pipeline only.
2. Add regression clips.
3. Add latency threshold test.
4. Add false-positive check.
5. Document runtime limits.
6. Run on target Jetson or representative device.
```

探索发现行为。

测试把发现变成契约。

稳定化防止原型债。

---

## 14. 硬件 bring-up（上电点亮/调通）示例

任务：

```text
Get ESP32-C6 Zigbee NCP talking to Jetson over UART.
```

探索模式：

```text
1. Confirm serial device candidates.
2. Try baud rates and flow-control assumptions.
3. Capture logs for each attempt.
4. Compare host-side and firmware-side symptoms.
5. Stop before changing firmware and kernel settings together.
```

稳定模式：

```text
1. Document working wiring and serial config.
2. Add a bring-up checklist.
3. Add a smoke command.
4. Save known-good logs.
5. Add troubleshooting table for common failure states.
```

来自 Lecture 21 的 agent 技能防止多变量混乱。

这套 SDLC 让你探索到足以学习。

---

## 15. 最小产物集

对于严肃的项目，保留：

```text
SPEC.md
INTENT.md
TEST_PLAN.md
DECISIONS.md
RISKS.md
RUNBOOK.md
```

最小版本：

```text
SPEC.md      what should be true
INTENT.md    why decisions were made
TESTS        executable behavior contracts
```

如果 agent 在改代码前只能读三样东西，给它：

```text
current spec
relevant tests
current intent
```

---

## 16. 失效模式

| 失效 | 发生了什么 | 修复 |
|---|---|---|
| 原型上线 | 探索代码进了生产 | 合并前要求稳定模式 |
| 规范漂移 | 实现教了新事实，文档却保持旧态 | 工作中更新规范 |
| 测试表演 | 测试只断言实现细节 | 写行为契约 |
| 无限探索 | agent 不断尝试想法却不收敛 | 限时并强制给出建议 |
| 过度流程 | agent 为微小任务写官僚流程 | 按风险缩放流程 |
| 品味薄弱 | agent 优化局部代码却让产品变差 | 由人审查 UX/架构/安全 |
| 隐藏维护 | 生成代码承担长期支持负担 | 记录负责人、风险和回滚路径 |
| 安全盲点 | 代码便宜，漏洞清理不便宜 | 执行策略并审查威胁路径 |

危险的混淆：

```text
fast generation != low total cost
```

---

## 迷你实验

向你的 agent 工作区添加两个命令或技能：

```text
/explore
/stabilize
```

`/explore` 输出：

```text
- options tried
- evidence gathered
- recommendation
- discarded ideas
- follow-up questions
```

`/stabilize` 输出：

```text
- updated spec/intent
- tests added or updated
- verification command output
- scoped diff summary
- risks and rollback
```

测试用：

```text
Explore three ways to improve OpenClaw App SDK event replay.
Then stabilize the best one.
```

---

## 关键要点

- 廉价的代码改变了软件流程，但不会消除工程成本。
- 实现可以是探索工具。
- 测试与意图是持久资产。
- 规范应随实现揭示现实而演进。
- 当代码到来更快时，人的品味与领域经验变得更加重要。
- agent 技能提供了仅靠探索所缺乏的稳定化纪律。
- 有用的模式是双模式：快速探索，然后用证据稳定化。

---


<details>
<summary>English original</summary>

**12. OpenClaw mapping**

In an OpenClaw-style runtime:

| SDLC concern | Runtime primitive |
|---|---|
| Explore mode | isolated session or sandbox workspace |
| Stabilize mode | main project session with stricter tools |
| Tests as contracts | tool execution plus captured run output |
| Intent docs | workspace bootstrap files or project docs |
| Spec sync | session memory and project markdown updates |
| Scope discipline | file policy, diff review, approval hook |
| Evidence | artifacts, logs, screenshots, run events |
| Human taste | approval UI, dashboard, review surfaces |
| Long-running work | cron, sessions, task ledger |

Useful command vocabulary:

```text
/explore "Try three possible implementations"
/choose "Select option B and explain why"
/stabilize "Make option B production-ready"
/verify "Run the contract checks"
/review "Inspect the diff and risks"
```

The runtime should record mode in run metadata.

Reviewers need to know whether they are looking at experiment output or ship-ready output.

---

**13. On-device AI example**

Task:

```text
Improve wake-word responsiveness without increasing false positives.
```

Explore mode:

```text
1. Try three VAD/wake-word pipeline variants.
2. Measure latency on short sample clips.
3. Track CPU/GPU usage.
4. Record false-positive behavior on noisy clips.
5. Recommend one candidate.
```

Stabilize mode:

```text
1. Update the selected pipeline only.
2. Add regression clips.
3. Add latency threshold test.
4. Add false-positive check.
5. Document runtime limits.
6. Run on target Jetson or representative device.
```

Exploration discovers behavior.

Tests turn discoveries into contracts.

Stabilization prevents prototype debt.

---

**14. Hardware bring-up example**

Task:

```text
Get ESP32-C6 Zigbee NCP talking to Jetson over UART.
```

Explore mode:

```text
1. Confirm serial device candidates.
2. Try baud rates and flow-control assumptions.
3. Capture logs for each attempt.
4. Compare host-side and firmware-side symptoms.
5. Stop before changing firmware and kernel settings together.
```

Stabilize mode:

```text
1. Document working wiring and serial config.
2. Add a bring-up checklist.
3. Add a smoke command.
4. Save known-good logs.
5. Add troubleshooting table for common failure states.
```

The agent skills from Lecture 21 prevent multi-variable chaos.

This SDLC lets you explore enough to learn.

---

**15. Minimal artifact set**

For serious projects, preserve:

```text
SPEC.md
INTENT.md
TEST_PLAN.md
DECISIONS.md
RISKS.md
RUNBOOK.md
```

Minimal version:

```text
SPEC.md      what should be true
INTENT.md    why decisions were made
TESTS        executable behavior contracts
```

If the agent can read only three things before changing code, give it:

```text
current spec
relevant tests
current intent
```

---

**16. Failure modes**

| Failure | What happened | Fix |
|---|---|---|
| Prototype shipped | exploration code went to production | require stabilize mode before merge |
| Spec drift | implementation taught new facts, docs stayed old | update spec during work |
| Test theater | tests assert implementation details only | write behavior contracts |
| Infinite exploration | agent keeps trying ideas without converging | timebox and force recommendation |
| Over-process | agent writes bureaucracy for tiny tasks | scale process to risk |
| Weak taste | agent optimizes local code but worsens product | human review for UX/architecture/security |
| Hidden maintenance | generated code owns long-term support burden | record owner, risks, and rollback path |
| Security blind spot | code is cheap, exploit cleanup is not | enforce policy and review threat paths |

Dangerous confusion:

```text
fast generation != low total cost
```

---

**Mini-lab**

Add two commands or skills to your agent workspace:

```text
/explore
/stabilize
```

`/explore` output:

```text
- options tried
- evidence gathered
- recommendation
- discarded ideas
- follow-up questions
```

`/stabilize` output:

```text
- updated spec/intent
- tests added or updated
- verification command output
- scoped diff summary
- risks and rollback
```

Test with:

```text
Explore three ways to improve OpenClaw App SDK event replay.
Then stabilize the best one.
```

---

**Key takeaways**

- Cheap code changes software process, but it does not remove engineering cost.
- Implementation can be an exploration tool.
- Tests and intent are durable assets.
- Specs should evolve as implementation reveals reality.
- Human taste and domain experience become more important when code arrives faster.
- Agent skills provide the stabilization discipline that exploration alone lacks.
- The useful pattern is dual-mode: explore fast, then stabilize with evidence.

---

</details>

## 参考文献

- Drew Breunig，"10 Lessons for Agentic Coding"：[https://www.dbreunig.com/2026/05/04/10-lessons-for-agentic-coding.html](https://www.dbreunig.com/2026/05/04/10-lessons-for-agentic-coding.html)
- Addy Osmani，"Agent Skills"：[https://addyosmani.com/blog/agent-skills/](https://addyosmani.com/blog/agent-skills/)
- 第 21 讲 - Agent Skills：[Lecture-21.md](Lecture-21.md)
- 第 35 讲 - OpenClaw Agent Loop：[Lecture-35.md](Lecture-35.md)
- 第 38 讲 - OpenClaw App SDK：[Lecture-38.md](Lecture-38.md)

---

*下一讲：[第 30 讲](Lecture-30.md)*


<details>
<summary>English original</summary>

**References**

- Drew Breunig, "10 Lessons for Agentic Coding": [https://www.dbreunig.com/2026/05/04/10-lessons-for-agentic-coding.html](https://www.dbreunig.com/2026/05/04/10-lessons-for-agentic-coding.html)
- Addy Osmani, "Agent Skills": [https://addyosmani.com/blog/agent-skills/](https://addyosmani.com/blog/agent-skills/)
- Lecture 21 - Agent Skills: [Lecture-21.md](Lecture-21.md)
- Lecture 35 - OpenClaw Agent Loop: [Lecture-35.md](Lecture-35.md)
- Lecture 38 - OpenClaw App SDK: [Lecture-38.md](Lecture-38.md)

---

*Next: [Lecture 30](Lecture-30.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-29.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-29.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
