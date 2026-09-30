---
title: Lecture 21 - Agent 技能：可靠编码 agent 的工作流纪律
description: Lecture 21 - Agent 技能：可靠编码 agent 的工作流纪律
---

# Lecture 21 - Agent 技能：可靠编码 agent 的工作流纪律

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [Lecture 20](Lecture-20.md) | **下一讲：** [Lecture 22](Lecture-22.md)

---

现代编码 agent 能**快速生成代码**。

这与**正确地做工程工作**不是一回事。

有用的思维模型：

```text
Agents optimize for "done."
Senior engineers optimize for correct, reviewable, and safe.
Agent skills encode the missing senior-engineering process.
```

本讲以 Addy Osmani 的 Agent Skills 工作作为参考模式，并将其适配到 OpenClaw 风格的 harness（agent 运行时框架）、local-first agent、端侧 AI 和硬件 bring-up（上电点亮/调通）工作流。

---

## 学习目标

学完本讲，你应当能够：

1. 解释为什么 agent 技能是工作流，而不是知识堆砌。
2. 设计一个技能，包含触发条件、检查点、证据和达成标准。
3. 使用反合理化表格来防止走捷径的行为。
4. 应用渐进式披露，使 agent 只加载相关的工作流。
5. 将软性的技能指导与硬性的 runtime 强制区分开。
6. 把技能映射到 OpenClaw 风格的 prompt、hook、tool、会话和产物中。
7. 为编码、硬件 bring-up 和端侧 AI 工作编写技能。

---

## 1. 为什么 agent 在实践中会失败

agent 常常失败，是因为跳过了**看不见的工程工作**：

| 缺失的纪律 | 失效模式 |
|---|---|
| Spec | agent 解决的是错误的问题 |
| Constraints | agent 改动了超出范围的代码文件或行为 |
| Tests | agent 在没有证据的情况下宣称成功 |
| Reviewability | 最终 diff 范围过大，无法信任 |
| Runtime evidence | 代码能编译，但在真实环境中失败 |
| Safety boundary | prompt 说“小心”，但工具仍允许造成破坏 |

这就像一个干活很快的初级工程师：

```text
can produce output
but may skip assumptions, tests, and review shape
```

agent 技能存在的意义，就是让**缺失的流程显式化**。

---

## 2. agent 技能究竟是什么

有用的技能不是：

```text
"Follow best practices."
```

有用的技能是：

```text
a small workflow
with specific steps
and a concrete completion signal
```

弱指令：

```text
Use TDD where appropriate.
```

技能形态的指令：

```text
1. Identify the behavior contract.
2. Write the smallest failing test.
3. Run it and capture the failure.
4. Implement the smallest fix.
5. Run the targeted test and capture the pass.
6. Run the relevant broader check.
7. Finalize only with evidence.
```

第一种版本给出的是**建议**。

第二种版本建立的是一个**循环**。

---

## 3. 技能在 agent 技术栈中的位置

技能是 harness 中的一层：

```text
Model
  -> system prompt
  -> skill router
  -> active skill workflow
  -> tools
  -> hooks and policy
  -> logs and artifacts
  -> final answer
```

用 OpenClaw 的说法：

```text
Gateway
  -> agent loop
  -> prompt assembly
  -> skills / bootstrap context
  -> tool execution
  -> hooks and approvals
  -> session log
  -> artifacts and delivery
```

重要区分：

```text
Skill = workflow instruction
Hook = deterministic interception
Tool policy = authority boundary
Artifact = durable evidence
```

不要让技能去承担 **policy 的职责**。

技能可以说“删除文件前先询问”。

runtime 仍应**拒绝不安全的删除类工具**，除非 policy 允许。

---

## 4. 流程优先于空谈

agent 能总结规则，却**不去应用它们**。

所以技能应更偏向**行动步骤，而不是长篇论述**。

弱写法：

```text
Be careful with production changes.
```

更好的写法：

```text
Before editing production code:
1. Identify the production boundary.
2. Identify rollback path.
3. List files allowed to change.
4. List tests or runtime checks required.
5. Stop if required evidence cannot be produced.
```

技能设计规则：

```text
If the agent cannot act on it, it is reference material, not a skill.
```

---

## 5. 反合理化表格

agent 很擅长给出**看起来合理的借口**。

例子：

| 走捷径的说法 | 必须给出的反驳 |
|---|---|
| “这个改动太小，不需要 spec。” | 小改动同样需要验收标准。写出尽可能小的 spec。 |
| “我稍后再补测试。” | “稍后”通常意味着永远不补。现在就加上最小验证。 |
| “代码能编译，所以它能工作。” | 编译只是一个信号，不是行为证明。运行相关检查。 |
| “顺手把这个邻近的重构做掉很有用。” | 有用不等于被要求。除非范围扩大已获批准，否则保持 diff 在既定范围内。 |
| “工具的输出大概已经够好了。” | 可变状态必须在定稿前实时检查。 |
| “这只是本地运行，所以安全无所谓。” | 本地 agent 往往持有密钥和文件系统权限。应用最小权限原则。 |

这种做法**成本低且有效**。

目标是**预先写好**针对模型可能采取的捷径的回应。

---



---


<details>
<summary>English original</summary>

**Lecture 21 - Agent Skills: Workflow Discipline for Reliable Coding Agents**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 20](Lecture-20.md) | **Next:** [Lecture 22](Lecture-22.md)

---

Modern coding agents can **generate code quickly**.

That is not the same as **doing engineering work correctly**.

The useful mental model:

```text
Agents optimize for "done."
Senior engineers optimize for correct, reviewable, and safe.
Agent skills encode the missing senior-engineering process.
```

This lecture uses Addy Osmani's Agent Skills work as a reference pattern and adapts it to OpenClaw-style harnesses, local-first agents, on-device AI, and hardware bring-up workflows.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Explain why agent skills are workflows, not knowledge dumps.
2. Design a skill with triggers, checkpoints, evidence, and exit criteria.
3. Use anti-rationalization tables to prevent shortcut behavior.
4. Apply progressive disclosure so agents load only relevant workflows.
5. Separate soft skill guidance from hard runtime enforcement.
6. Map skills into OpenClaw-style prompts, hooks, tools, sessions, and artifacts.
7. Write skills for coding, hardware bring-up, and on-device AI work.

---

**1. Why agents fail in practice**

Agents often fail because they skip **invisible engineering work**:

| Missing discipline | Failure mode |
|---|---|
| Spec | the agent solves the wrong problem |
| Constraints | the agent changes files or behavior outside scope |
| Tests | the agent declares success without proof |
| Reviewability | the final diff is too broad to trust |
| Runtime evidence | code compiles but fails in the real environment |
| Safety boundary | the prompt says "be careful" but tools still allow damage |

This resembles a fast junior engineer:

```text
can produce output
but may skip assumptions, tests, and review shape
```

Agent skills exist to make the **missing process explicit**.

---

**2. What an agent skill actually is**

A useful skill is not:

```text
"Follow best practices."
```

A useful skill is:

```text
a small workflow
with specific steps
and a concrete completion signal
```

Weak instruction:

```text
Use TDD where appropriate.
```

Skill-shaped instruction:

```text
1. Identify the behavior contract.
2. Write the smallest failing test.
3. Run it and capture the failure.
4. Implement the smallest fix.
5. Run the targeted test and capture the pass.
6. Run the relevant broader check.
7. Finalize only with evidence.
```

The first version gives **advice**.

The second version creates a **loop**.

---

**3. Where skills sit in the agent stack**

Skills are one layer in the harness:

```text
Model
  -> system prompt
  -> skill router
  -> active skill workflow
  -> tools
  -> hooks and policy
  -> logs and artifacts
  -> final answer
```

In OpenClaw language:

```text
Gateway
  -> agent loop
  -> prompt assembly
  -> skills / bootstrap context
  -> tool execution
  -> hooks and approvals
  -> session log
  -> artifacts and delivery
```

Important distinction:

```text
Skill = workflow instruction
Hook = deterministic interception
Tool policy = authority boundary
Artifact = durable evidence
```

Do not ask a skill to do the **job of policy**.

A skill can say "ask before deleting files."

The runtime should still **deny unsafe delete tools** unless policy allows them.

---

**4. Process over prose**

Agents can summarize rules **without applying them**.

So a skill should prefer **action steps over essays**.

Weak:

```text
Be careful with production changes.
```

Better:

```text
Before editing production code:
1. Identify the production boundary.
2. Identify rollback path.
3. List files allowed to change.
4. List tests or runtime checks required.
5. Stop if required evidence cannot be produced.
```

Skill design rule:

```text
If the agent cannot act on it, it is reference material, not a skill.
```

---

**5. Anti-rationalization tables**

Agents are good at **plausible excuses**.

Examples:

| Shortcut claim | Required rebuttal |
|---|---|
| "This is too small for a spec." | Small changes still need acceptance criteria. Write the smallest possible spec. |
| "I will add tests later." | Later usually means never. Add the minimal verification now. |
| "The code compiles, so it works." | Compilation is one signal, not behavior proof. Run the relevant check. |
| "This nearby refactor is useful." | Useful is not requested. Keep the diff scoped unless scope expansion is approved. |
| "The tool output is probably good enough." | Mutable state must be checked live before finalizing. |
| "This is local only, so security does not matter." | Local agents often hold secrets and filesystem authority. Apply least privilege. |

This is **cheap and effective**.

The goal is to **pre-write the response** to the shortcuts the model is likely to take.

---

</details>

## 6. 验证是强制性的

技能应以**证据**收尾。

证据示例：

| 任务类型 | 证据 |
|---|---|
| 代码变更 | 测试输出、lint 输出、构建输出 |
| UI 变更 | 截图、视觉 diff、响应式检查 |
| API 变更 | schema diff、契约测试、兼容性说明 |
| Runtime 变更 | 健康检查、日志摘录、冒烟测试 |
| 安全变更 | 拒绝路径测试、权限审计、策略检查 |
| 文档变更 | 文档构建、链接检查、渲染预览 |
| 硬件 bring-up（上电点亮/调通）| kernel 日志、总线扫描、命令输出、波形抓取 |

规则：

```text
No evidence, no completion.
```

这对**长时间运行的 agent** 更为重要。

微小的捷径会在**长会话中不断累积**。

---

## 7. 渐进式披露

不要**把每个 workflow 都加载进每一次运行**。

这会带来：

- token 膨胀
- attention 变弱
- 推理变慢
- 压缩压力更大
- 无关指令冲突

更好的模式：

```text
small router
  -> load only relevant skill
  -> load deeper references only when needed
```

示例：

```text
Bug fix request
  -> load test-driven-bugfix
  -> maybe load runtime-debug
  -> do not load deployment, frontend, and release skills unless needed
```

这对**端侧 AI** 尤其重要，因为其中的上下文、延迟、内存和散热预算都很关键。

---

## 8. 范围纪律

可靠的编码 agent 必须让变更保持**可审查**。

编辑之前：

```text
- list intended files
- list non-goals
- identify protected areas
- ask before broadening scope
```

给出最终答案之前：

```text
- list files changed
- state whether scope expanded
- explain why any expansion was necessary
- provide verification evidence
```

可审查性**不是表面功夫**。

它是人类**对生成产物保有主导权**的方式。

---

## 9. 技能解剖

一个实用的 `SKILL.md` 应当简短且有结构：

```markdown
---
name: test-driven-bugfix
description: Use for bug fixes where behavior must be proven with tests or runtime evidence.
---

# Test-Driven Bug Fix

## When to use

Use when fixing a bug, regression, failing test, or runtime error.

## Workflow

1. Reproduce the bug or failing behavior.
2. Record the exact failure output.
3. Identify the smallest behavior contract.
4. Add or update the minimal failing test.
5. Run the test and confirm failure.
6. Implement the smallest fix.
7. Run the targeted test and confirm pass.
8. Run the relevant broader check.
9. Review the diff for unrelated changes.

## Anti-rationalization

| Claim | Response |
|---|---|
| "This is obvious." | Obvious fixes still need evidence. |
| "There is no test harness." | Use the smallest available runtime or command-level check. |
| "The failure is intermittent." | Capture logs and state what was and was not reproduced. |

## Exit criteria

- Failure was reproduced or explicitly marked unreproducible.
- Fix is scoped to the bug.
- Verification command and result are recorded.
- No unrelated files were changed.
```

这样**足够紧凑便于加载**，也足够具体便于审计。

---

## 10. 硬件 bring-up 技能

agent 技能对嵌入式和硬件工作很有用，因为 bring-up **充满可变状态**。

示例：

```markdown
---
name: hardware-bringup-debug
description: Use for Jetson, ESP32, I2S, SPI, UART, kernel, driver, and device-tree debugging.
---

# Hardware Bring-Up Debug

## Workflow

1. Identify board, OS image, kernel version, and exact hardware path.
2. Record the expected signal or interface contract.
3. Capture current observable state.
4. Separate host, wiring, firmware, driver, and userspace hypotheses.
5. Test one hypothesis at a time.
6. Do not change kernel, device tree, firmware, and userspace simultaneously.
7. Preserve raw command outputs for evidence.
8. Summarize blocker and next physical or software check.

## Anti-rationalization

| Claim | Response |
|---|---|
| "It is probably wiring." | Prove host and software state before blaming wiring. |
| "It is probably software." | Check voltage, pinmux, and physical bus assumptions. |
| "Let's rebuild everything." | Change one layer at a time or the result is not diagnosable. |
```

这直接适用于：

- Jetson I2S 麦克风采集
- ESP32-C6 RCP/NCP bring-up
- OpenThread attach 调试
- Zigbee 协调器测试
- camera sensor bring-up
- audio codec device-tree 工作

这个技能可以防止典型的失败：

```text
change five variables, then no one knows which one mattered
```

---


<details>
<summary>English original</summary>

**6. Verification is mandatory**

A skill should end with **evidence**.

Evidence examples:

| Task type | Evidence |
|---|---|
| Code change | test output, lint output, build output |
| UI change | screenshot, visual diff, responsive check |
| API change | schema diff, contract test, compatibility note |
| Runtime change | health check, log excerpt, smoke test |
| Security change | denied-path test, permission audit, policy check |
| Documentation change | docs build, link check, rendered preview |
| Hardware bring-up | kernel log, bus scan, command output, waveform capture |

Rule:

```text
No evidence, no completion.
```

This matters more for **long-running agents**.

Small shortcuts **compound over long sessions**.

---

**7. Progressive disclosure**

Do not load **every workflow into every run**.

That creates:

- token bloat
- weaker attention
- slower inference
- more compaction pressure
- irrelevant instruction conflicts

Better pattern:

```text
small router
  -> load only relevant skill
  -> load deeper references only when needed
```

Example:

```text
Bug fix request
  -> load test-driven-bugfix
  -> maybe load runtime-debug
  -> do not load deployment, frontend, and release skills unless needed
```

This is especially important for **on-device AI** where context, latency, memory, and thermal budget matter.

---

**8. Scope discipline**

A reliable coding agent must keep changes **reviewable**.

Before editing:

```text
- list intended files
- list non-goals
- identify protected areas
- ask before broadening scope
```

Before final answer:

```text
- list files changed
- state whether scope expanded
- explain why any expansion was necessary
- provide verification evidence
```

Reviewability is **not a cosmetic concern**.

It is how humans **keep authority over generated work**.

---

**9. Skill anatomy**

A practical `SKILL.md` should be short and structured:

```markdown
---
name: test-driven-bugfix
description: Use for bug fixes where behavior must be proven with tests or runtime evidence.
---

# Test-Driven Bug Fix

## When to use

Use when fixing a bug, regression, failing test, or runtime error.

## Workflow

1. Reproduce the bug or failing behavior.
2. Record the exact failure output.
3. Identify the smallest behavior contract.
4. Add or update the minimal failing test.
5. Run the test and confirm failure.
6. Implement the smallest fix.
7. Run the targeted test and confirm pass.
8. Run the relevant broader check.
9. Review the diff for unrelated changes.

## Anti-rationalization

| Claim | Response |
|---|---|
| "This is obvious." | Obvious fixes still need evidence. |
| "There is no test harness." | Use the smallest available runtime or command-level check. |
| "The failure is intermittent." | Capture logs and state what was and was not reproduced. |

## Exit criteria

- Failure was reproduced or explicitly marked unreproducible.
- Fix is scoped to the bug.
- Verification command and result are recorded.
- No unrelated files were changed.
```

This is **compact enough to load** and specific enough to audit.

---

**10. Hardware bring-up skill**

Agent skills are useful for embedded and hardware work because bring-up is **full of mutable state**.

Example:

```markdown
---
name: hardware-bringup-debug
description: Use for Jetson, ESP32, I2S, SPI, UART, kernel, driver, and device-tree debugging.
---

# Hardware Bring-Up Debug

## Workflow

1. Identify board, OS image, kernel version, and exact hardware path.
2. Record the expected signal or interface contract.
3. Capture current observable state.
4. Separate host, wiring, firmware, driver, and userspace hypotheses.
5. Test one hypothesis at a time.
6. Do not change kernel, device tree, firmware, and userspace simultaneously.
7. Preserve raw command outputs for evidence.
8. Summarize blocker and next physical or software check.

## Anti-rationalization

| Claim | Response |
|---|---|
| "It is probably wiring." | Prove host and software state before blaming wiring. |
| "It is probably software." | Check voltage, pinmux, and physical bus assumptions. |
| "Let's rebuild everything." | Change one layer at a time or the result is not diagnosable. |
```

This applies directly to:

- Jetson I2S microphone capture
- ESP32-C6 RCP/NCP bring-up
- OpenThread attach debugging
- Zigbee coordinator testing
- camera sensor bring-up
- audio codec device-tree work

The skill prevents the classic failure:

```text
change five variables, then no one knows which one mattered
```

---

</details>

## 11. 端侧 AI 技能

端侧 agent 有**额外的约束**：

- 内存压力
- 热管理预算
- 本地隐私
- 更小的上下文窗口
- 网络间歇可用
- 模型回退行为
- 硬件权限

示例：

```markdown
---
name: on-device-agent-change
description: Use when modifying an agent that runs on a laptop, Jetson, phone, or local gateway.
---

# On-Device Agent Change

## Workflow

1. Identify target device and runtime limits.
2. Identify local-only data and privacy boundaries.
3. Check startup path and readiness gates.
4. Keep prompt/context additions minimal.
5. Prefer deterministic checks over model judgment.
6. Validate behavior with network unavailable if relevant.
7. Record CPU/GPU/memory impact when measurable.

## Exit criteria

- startup remains deterministic
- local permissions are unchanged or explicitly reviewed
- context growth is bounded
- fallback behavior is documented
- verification was run on or representative of the target device
```

这适用于 OpenClaw、Jetson 以及本地优先的助手系统。

---

## 12. runtime 强制执行模式

使用两层：

```text
1. Soft guidance: skill workflow
2. Hard enforcement: harness policy
```

示例：

| 工作流要求 | runtime 强制执行 |
|---|---|
| 定稿前运行测试 | final-answer hook 检查测试证据 |
| 不得越界修改 | 文件系统策略或 diff 检查器 |
| 危险命令前先询问 | exec 审批门 |
| 密钥不得进入日志 | 日志脱敏与拒绝列表路径 |
| 使用小上下文 | 提示词预算与上下文检查器 |
| 保留证据 | 产物 API 或会话附件 |

提示词帮助塑造行为。

它们并不强制执行授权。

---

## 13. 证据账本

对生产环境的 agent，保留 run 级的证据账本：

```text
task id
skill used
files touched
tools called
approval decisions
tests run
logs captured
artifacts created
scope changes
known gaps
```

它支撑：

- 评审
- 调试
- 事件响应
- 可审计性
- 未来的技能改进

OpenClaw 风格的系统可将其存储于：

- 会话记录
- run 事件
- 产物
- Gateway RPC 任务状态
- 外部仪表盘

原则：

```text
If the agent claims success, the system should know why.
```

---

## 14. 实用实现检查清单

从五项技能开始：

| 技能 | 为什么重要 |
|---|---|
| `spec-first` | 防止针对错误目标做实现 |
| `small-plan` | 强制产出可评审的分块 |
| `test-driven-bugfix` | 生成行为证据 |
| `runtime-safety-review` | 捕获工具、权限与数据风险 |
| `hardware-bringup-debug` | 防止多变量调试的混乱 |

对每项技能，定义：

```text
name
description
when to use
workflow
anti-rationalization table
exit criteria
evidence format
references, if needed
```

然后加上：

- 一个小型 router
- 一个 final-answer 证据检查
- 一个 diff 范围检查
- 一种检查哪项技能运行过的方式
- 为可复现性做技能版本管理

---

## Mini-lab

为一个痛点工作流创建一项本地技能。

推荐选择：

- Jetson 音频调试
- ESP32-C6 射频 bring-up（上电点亮/调通）
- OpenClaw 插件调试
- App SDK 冒烟测试
- 模型 runtime 回归
- 文档构建失败

手动测试它：

1. 给 agent 一个应当触发该技能的任务。
2. 检查它是否遵循该工作流。
3. 检查它是否产出证据。
4. 检查最终答案是否可评审。
5. 在 agent 跳过或自我合理化之处修订该技能。

---

## 关键要点

- Agent 技能把资深工程纪律变成可复用的工作流。
- 有用的技能是流程，不是散文。
- 技能需要检查点、反合理化机制和达成标准。
- 验证必须产出证据。
- 渐进式披露让上下文保持小而相关。
- 范围纪律让 agent 输出可评审。
- 技能不替代 hooks、审批、沙箱或工具策略。

---

## 参考文献

- Addy Osmani, "Agent Skills": [https://addyosmani.com/blog/agent-skills/](https://addyosmani.com/blog/agent-skills/)
- Agent Skills repository: [https://github.com/addyosmani/agent-skills](https://github.com/addyosmani/agent-skills)
- Lecture 35 - OpenClaw Agent Loop: [Lecture-35.md](Lecture-35.md)
- Lecture 37 - System Prompt Architecture: [Lecture-37.md](Lecture-37.md)
- Lecture 41 - Pi: [Lecture-41.md](Lecture-41.md)

---

*下一步：[Lecture 22](Lecture-22.md)*


<details>
<summary>English original</summary>

**11. On-device AI skill**

On-device agents have **additional constraints**:

- memory pressure
- thermal budget
- local privacy
- smaller context windows
- intermittent network
- model fallback behavior
- hardware permissions

Example:

```markdown
---
name: on-device-agent-change
description: Use when modifying an agent that runs on a laptop, Jetson, phone, or local gateway.
---

# On-Device Agent Change

## Workflow

1. Identify target device and runtime limits.
2. Identify local-only data and privacy boundaries.
3. Check startup path and readiness gates.
4. Keep prompt/context additions minimal.
5. Prefer deterministic checks over model judgment.
6. Validate behavior with network unavailable if relevant.
7. Record CPU/GPU/memory impact when measurable.

## Exit criteria

- startup remains deterministic
- local permissions are unchanged or explicitly reviewed
- context growth is bounded
- fallback behavior is documented
- verification was run on or representative of the target device
```

This fits OpenClaw, Jetson, and local-first assistant systems.

---

**12. Runtime enforcement pattern**

Use two layers:

```text
1. Soft guidance: skill workflow
2. Hard enforcement: harness policy
```

Examples:

| Workflow requirement | Runtime enforcement |
|---|---|
| Run tests before finalizing | final-answer hook checks for test evidence |
| Do not edit outside scope | filesystem policy or diff checker |
| Ask before dangerous command | exec approval gate |
| Keep secrets out of logs | log redaction and denylisted paths |
| Use small context | prompt budget and context inspectors |
| Preserve evidence | artifact API or session attachment |

Prompts help behavior.

They do not enforce authority.

---

**13. Evidence ledger**

For production agents, keep a run-level evidence ledger:

```text
task id
skill used
files touched
tools called
approval decisions
tests run
logs captured
artifacts created
scope changes
known gaps
```

This supports:

- review
- debugging
- incident response
- auditability
- future skill improvement

OpenClaw-style systems can store this across:

- session transcript
- run events
- artifacts
- Gateway RPC task state
- external dashboards

Principle:

```text
If the agent claims success, the system should know why.
```

---

**14. Practical implementation checklist**

Start with five skills:

| Skill | Why it matters |
|---|---|
| `spec-first` | prevents wrong-target implementation |
| `small-plan` | forces reviewable chunks |
| `test-driven-bugfix` | creates behavior evidence |
| `runtime-safety-review` | catches tool, permission, and data risks |
| `hardware-bringup-debug` | prevents multi-variable debugging chaos |

For each skill, define:

```text
name
description
when to use
workflow
anti-rationalization table
exit criteria
evidence format
references, if needed
```

Then add:

- a small router
- a final-answer evidence check
- a diff-scope check
- a way to inspect which skill ran
- skill versioning for reproducibility

---

**Mini-lab**

Create one local skill for a painful workflow.

Recommended choices:

- Jetson audio debug
- ESP32-C6 radio bring-up
- OpenClaw plugin debugging
- App SDK smoke test
- model runtime regression
- documentation build failure

Test it manually:

1. Give the agent a task that should trigger the skill.
2. Check whether it follows the workflow.
3. Check whether it produces evidence.
4. Check whether the final answer is reviewable.
5. Revise the skill where the agent skipped or rationalized.

---

**Key takeaways**

- Agent skills turn senior-engineering discipline into reusable workflows.
- A useful skill is process, not prose.
- Skills need checkpoints, anti-rationalization, and exit criteria.
- Verification must produce evidence.
- Progressive disclosure keeps context small and relevant.
- Scope discipline makes agent output reviewable.
- Skills do not replace hooks, approvals, sandboxing, or tool policy.

---

**References**

- Addy Osmani, "Agent Skills": [https://addyosmani.com/blog/agent-skills/](https://addyosmani.com/blog/agent-skills/)
- Agent Skills repository: [https://github.com/addyosmani/agent-skills](https://github.com/addyosmani/agent-skills)
- Lecture 35 - OpenClaw Agent Loop: [Lecture-35.md](Lecture-35.md)
- Lecture 37 - System Prompt Architecture: [Lecture-37.md](Lecture-37.md)
- Lecture 41 - Pi: [Lecture-41.md](Lecture-41.md)

---

*Next: [Lecture 22](Lecture-22.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-21.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-21.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
