---
title: 第 22 讲 - Agent Skills Eval：对 SKILL.md 文件做 benchmark
description: 第 22 讲 - Agent Skills Eval：对 SKILL.md 文件做 benchmark
---

# 第 22 讲 - Agent Skills Eval：对 SKILL.md 文件做 benchmark

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [第 21 讲](Lecture-21.md) | **下一讲：** [第 23 讲](Lecture-23.md)

---

技能是**紧邻代码的基础设施**。

如果一项技能改变了 agent 的行为，它**就需要测试**。

Agent Skills 通过如下文件夹，为 agent 提供可移植、受版本控制的工作流：

```text
my-skill/
  SKILL.md
  references/
  scripts/
  assets/
  evals/
```

但一个 `SKILL.md` 文件在实践中可能**看起来很好，却依然失效**。

正确的问题不是：

```text
Does this skill read well?
```

正确的问题是：

```text
Does this skill measurably improve the agent on the task it claims to support?
```

`agent-skills-eval` 就是针对这个问题的**测试运行器**。

它把同一个评测 prompt 运行两次：

```text
with_skill
without_skill
```

然后由**评判模型**依据预期行为和断言给两份输出打分。

这样得到的是**有证据支撑的通过/失败判定**，而不是主观的 prompt 审阅。

---

## 学习目标

学完本讲，你应当能够：

1. 解释技能为什么需要回归测试。
2. 理解 `with_skill` 对比 `without_skill` 的基线模式。
3. 为一项技能编写基本的 `evals/evals.json` 用例。
4. 解读由评判模型打分的通过/失败结果。
5. 对工具调用类技能使用确定性断言。
6. 理解 `agent-skills-eval` 产生的产物布局。
7. 为 OpenClaw 风格的技能仓库设计 CI 门禁。
8. 识别 LLM 评判式技能评测中的常见失效模式。

---

## 1. 为什么技能评测很重要

Agent 技能封装的是**过程性知识**。

示例：

- 分诊 GitHub issue
- 汇总天气数据
- 操作 CRM
- 撰写发布说明
- 检查 GPU trace
- 遵循安全评审检查清单
- 在不同 DSL 之间转换 kernel

这很强大，因为技能是**可复用的**。

这也很危险，因为技能可能**悄无声息地退化**。

一个糟糕的技能可能：

- 引入无关上下文
- 过度约束模型
- 导致错误的工具调用
- 在质量没有提升的情况下增加 token 开销
- 让 agent 变慢
- 掩盖过时的指令
- 让任务表现比基线更差

没有评测，每个技能 PR 都会变成一场**品味之争**。

有了评测，讨论就变成：

```text
This skill improved 7/9 evals.
It regressed the pagination case.
The judge evidence points to missing tool-call criteria.
The report includes both outputs and timing.
```

这才是**更好的评审标准**。

---

## 2. 基线模式

核心设计很简单：

```text
same prompt
  -> target model without skill
  -> target model with skill
  -> judge compares both against assertions
```

这一点很重要，因为**只看输出的绝对质量是不够的**。

你需要知道的是**技能带来的提升**：

```text
output with skill passes
output without skill fails
  -> skill likely helps

both pass
  -> skill may be unnecessary for this eval

both fail
  -> skill or eval is insufficient

with skill fails, baseline passes
  -> skill regressed behavior
```

基线模式能避免一个常见错误：

```text
The skill produced a good answer, therefore the skill is useful.
```

也许模型**在没有这项技能**的情况下就已经给出了同样的答案。

评测需要**度量这个差值**。

---

## 3. 快速上手

用 `npx` 直接运行：

```bash
npx agent-skills-eval ./skills \
  --target gpt-4o-mini \
  --judge gpt-4o-mini \
  --baseline \
  --strict
```

若想在项目中使用，请安装：

```bash
npm install agent-skills-eval
```

关键参数：

```text
--target
  model being evaluated

--judge
  model grading outputs

--baseline
  run without_skill as comparison

--strict
  enforce skill/spec validation
```

对 OpenClaw 贡献者来说，这是一个有用的心智模型：

```text
skills are tested like code
eval artifacts are reviewed like logs
reports are attached to PRs
```

---


<details>
<summary>English original</summary>

**Lecture 22 - Agent Skills Eval: Benchmarking SKILL.md Files**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 21](Lecture-21.md) | **Next:** [Lecture 23](Lecture-23.md)

---

Skills are **code-adjacent infrastructure**.

If a skill changes how an agent behaves, it **needs tests**.

Agent Skills gives agents portable, version-controlled workflows through folders such as:

```text
my-skill/
  SKILL.md
  references/
  scripts/
  assets/
  evals/
```

But a `SKILL.md` file can **look good and still fail** in practice.

The right question is not:

```text
Does this skill read well?
```

The right question is:

```text
Does this skill measurably improve the agent on the task it claims to support?
```

`agent-skills-eval` is a **test runner** for that question.

It runs the same eval prompt twice:

```text
with_skill
without_skill
```

Then a **judge model** grades both outputs against expected behavior and assertions.

That gives you **evidence-backed pass/fail** rather than subjective prompt review.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Explain why skills need regression tests.
2. Understand the `with_skill` versus `without_skill` baseline pattern.
3. Write basic `evals/evals.json` cases for a skill.
4. Interpret judge-graded pass/fail results.
5. Use deterministic assertions for tool-call skills.
6. Understand the artifact layout produced by `agent-skills-eval`.
7. Design CI gates for OpenClaw-style skill repositories.
8. Identify common failure modes in LLM-judged skill evaluation.

---

**1. Why skill evaluation matters**

Agent skills package **procedural knowledge**.

Examples:

- triage GitHub issues
- summarize weather data
- operate a CRM
- write release notes
- inspect GPU traces
- follow a security review checklist
- translate kernels between DSLs

This is powerful because skills are **reusable**.

It is dangerous because skills can **silently degrade**.

A bad skill can:

- add irrelevant context
- over-constrain the model
- cause wrong tool calls
- increase token cost without quality lift
- make the agent slower
- hide stale instructions
- make a task worse than baseline

Without evaluation, every skill PR becomes a **taste debate**.

With evaluation, the conversation becomes:

```text
This skill improved 7/9 evals.
It regressed the pagination case.
The judge evidence points to missing tool-call criteria.
The report includes both outputs and timing.
```

That is a **better review standard**.

---

**2. The baseline pattern**

The core design is simple:

```text
same prompt
  -> target model without skill
  -> target model with skill
  -> judge compares both against assertions
```

This matters because **absolute output quality is not enough**.

You want to know **skill lift**:

```text
output with skill passes
output without skill fails
  -> skill likely helps

both pass
  -> skill may be unnecessary for this eval

both fail
  -> skill or eval is insufficient

with skill fails, baseline passes
  -> skill regressed behavior
```

The baseline mode prevents a common mistake:

```text
The skill produced a good answer, therefore the skill is useful.
```

Maybe the model already produced the same answer **without the skill**.

The eval needs to **measure the delta**.

---

**3. Quickstart**

Run directly with `npx`:

```bash
npx agent-skills-eval ./skills \
  --target gpt-4o-mini \
  --judge gpt-4o-mini \
  --baseline \
  --strict
```

Install if you want it in a project:

```bash
npm install agent-skills-eval
```

The key flags:

```text
--target
  model being evaluated

--judge
  model grading outputs

--baseline
  run without_skill as comparison

--strict
  enforce skill/spec validation
```

For OpenClaw contributors, this is the useful mental model:

```text
skills are tested like code
eval artifacts are reviewed like logs
reports are attached to PRs
```

---

</details>

## 4. 技能布局

一个最小的已评测技能如下所示：

```text
skills/
  weather-summary/
    SKILL.md
    evals/
      evals.json
```

示例 `SKILL.md`：

```markdown
---
name: weather-summary
description: Summarize weather forecasts and call out operational risks.
license: MIT
compatibility: Works with text-capable chat models.
---

When summarizing weather, identify the location, time range, precipitation risk,
temperature extremes, wind risk, and one practical recommendation.
```

示例 `evals/evals.json`：

```json
{
  "skill_name": "weather-summary",
  "evals": [
    {
      "id": "storm-risk",
      "name": "storm risk summary",
      "prompt": "Summarize this forecast for an outdoor robotics test: thunderstorms after 2pm, wind gusts to 35 mph, high 91F.",
      "expected_output": "The response should identify thunderstorm timing, wind risk, heat risk, and recommend moving the test earlier or indoors.",
      "assertions": [
        "The output mentions thunderstorm risk after 2pm.",
        "The output mentions wind gusts or wind risk.",
        "The output gives a practical scheduling or safety recommendation."
      ]
    }
  ]
}
```

断言就是**契约**。

如果它们含糊不清，评测也会**含糊不清**。

---

## 5. 产物布局

一次运行会创建类似这样的工作区：

```text
agent-skills-workspace/
  iteration-1/
    meta.json
    benchmark.json
    eval-basic/
      with_skill/
      without_skill/
    report/
      index.html
```

重要产物：

- `benchmark.json`：汇总的 pass/fail 结果
- `with_skill/`：输出、时序、评分
- `without_skill/`：基线输出、时序、评分
- `report/index.html`：供审阅的静态报告
- JSON/JSONL 事件：可用于仪表盘或 CI 历史记录

这种**产物优先的设计**很重要。

你可以**随时间对比多次运行**。

你可以把报告附到 pull request 上。

你可以在修改 `SKILL.md` 后检测回归。

---

## 6. Judge 模型评分

Judge 会看到：

- 评测 prompt
- 期望输出
- 断言
- 目标模型输出

然后它依据证据给出 pass/fail 评分。

这很有用，但它**并不完美**。

LLM Judge 可能会：

- 前后不一致
- 过度奖励流畅的回答
- 漏掉细微的工具调用失败
- 因期望输出的措辞而泄漏偏见
- 让满足字面措辞但不满足真实意图的输出通过
- 不同模型版本之间结论不一致

缓解措施：

- 将 judge temperature 保持在 `0`
- 使用显式断言
- 尽可能加入确定性检查
- 人工复核失败和意外通过的用例
- 尽可能固定 judge 与目标模型的版本
- 保留原始产物
- 在阻塞发布前重跑不稳定的评测

Judge 只是**工具，而非权威**。

---

## 7. 工具调用断言

许多有用的技能**并非纯文本生成技能**。

OpenClaw 风格的技能往往会影响工具行为：

- 调用 `gh` 命令
- 调用网关 RPC
- 查询天气 API
- 检查日志
- 读取文件
- 创建 issue
- 更新文档

对这类技能，**纯文本评分是薄弱的**。

你需要的是**确定性的工具调用断言**：

```text
Did the agent call the expected tool?
Did it use the expected method?
Did it pass safe arguments?
Did it avoid a destructive command?
Did it include the required idempotency key?
```

用 LLM 评判**语义质量**。

用确定性断言校验**协议行为**。

这种区分**至关重要**：

```text
semantic correctness:
  judge model

tool contract correctness:
  deterministic assertions
```

---

## 8. 用于 CI 的配置文件

需要反复运行时，使用配置文件：

```yaml
# agent-skills-eval.yaml
root: ./skills
workspace: ./agent-skills-workspace
baseline: true
target: gpt-4o-mini
judge: gpt-4o-mini
baseUrl: https://api.openai.com/v1
apiKeyEnv: OPENAI_API_KEY
include:
  - "skills/**"
exclude:
  - "**/draft-*"
concurrency: 4
layout: iteration
strict: true
report:
  enabled: true
  title: Agent Skills Report
targetParams:
  temperature: 0
judgeParams:
  temperature: 0
```

运行：

```bash
OPENAI_API_KEY=... npx agent-skills-eval --config agent-skills-eval.yaml
```

在 CI 中，**不要只依赖控制台输出**。

持久化保存：

- `benchmark.json`
- judge 评分文件
- 生成的报告
- JSONL 事件日志

这些产物就是**证据**。

---


<details>
<summary>English original</summary>

**4. Skill layout**

A minimal evaluated skill looks like:

```text
skills/
  weather-summary/
    SKILL.md
    evals/
      evals.json
```

Example `SKILL.md`:

```markdown
---
name: weather-summary
description: Summarize weather forecasts and call out operational risks.
license: MIT
compatibility: Works with text-capable chat models.
---

When summarizing weather, identify the location, time range, precipitation risk,
temperature extremes, wind risk, and one practical recommendation.
```

Example `evals/evals.json`:

```json
{
  "skill_name": "weather-summary",
  "evals": [
    {
      "id": "storm-risk",
      "name": "storm risk summary",
      "prompt": "Summarize this forecast for an outdoor robotics test: thunderstorms after 2pm, wind gusts to 35 mph, high 91F.",
      "expected_output": "The response should identify thunderstorm timing, wind risk, heat risk, and recommend moving the test earlier or indoors.",
      "assertions": [
        "The output mentions thunderstorm risk after 2pm.",
        "The output mentions wind gusts or wind risk.",
        "The output gives a practical scheduling or safety recommendation."
      ]
    }
  ]
}
```

The assertions are the **contract**.

If they are vague, the eval will be **vague**.

---

**5. Artifact layout**

A run creates a workspace similar to:

```text
agent-skills-workspace/
  iteration-1/
    meta.json
    benchmark.json
    eval-basic/
      with_skill/
      without_skill/
    report/
      index.html
```

Important artifacts:

- `benchmark.json`: rolled-up pass/fail results
- `with_skill/`: output, timing, grading
- `without_skill/`: baseline output, timing, grading
- `report/index.html`: static report for review
- JSON/JSONL events: useful for dashboards or CI history

This **artifact-first design** is important.

You can **diff runs over time**.

You can attach reports to pull requests.

You can detect regressions after changing `SKILL.md`.

---

**6. Judge model grading**

The judge sees:

- eval prompt
- expected output
- assertions
- target model output

Then it grades pass/fail with evidence.

This is useful, but it is **not perfect**.

LLM judges can:

- be inconsistent
- over-reward fluent answers
- miss subtle tool-call failures
- leak bias from expected output phrasing
- pass outputs that satisfy wording but not intent
- disagree across model versions

Mitigations:

- keep judge temperature at `0`
- use explicit assertions
- add deterministic checks where possible
- review failures and unexpected passes manually
- pin judge and target model versions when possible
- keep raw artifacts
- rerun flaky evals before blocking a release

The judge is a **tool, not an authority**.

---

**7. Tool-call assertions**

Many useful skills are **not pure text-generation skills**.

OpenClaw-style skills often affect tool behavior:

- call `gh` commands
- invoke gateway RPCs
- query weather APIs
- inspect logs
- read files
- create issues
- update docs

For those, **text-only grading is weak**.

You want **deterministic tool-call assertions**:

```text
Did the agent call the expected tool?
Did it use the expected method?
Did it pass safe arguments?
Did it avoid a destructive command?
Did it include the required idempotency key?
```

Use LLM judging for **semantic quality**.

Use deterministic assertions for **protocol behavior**.

That split is **critical**:

```text
semantic correctness:
  judge model

tool contract correctness:
  deterministic assertions
```

---

**8. Config file for CI**

For repeated runs, use a config file:

```yaml
# agent-skills-eval.yaml
root: ./skills
workspace: ./agent-skills-workspace
baseline: true
target: gpt-4o-mini
judge: gpt-4o-mini
baseUrl: https://api.openai.com/v1
apiKeyEnv: OPENAI_API_KEY
include:
  - "skills/**"
exclude:
  - "**/draft-*"
concurrency: 4
layout: iteration
strict: true
report:
  enabled: true
  title: Agent Skills Report
targetParams:
  temperature: 0
judgeParams:
  temperature: 0
```

Run:

```bash
OPENAI_API_KEY=... npx agent-skills-eval --config agent-skills-eval.yaml
```

In CI, do **not rely only on console output**.

Persist:

- `benchmark.json`
- judge grading files
- generated report
- JSONL event logs

Those artifacts are the **evidence**.

---

</details>

## 9. OpenClaw 技能测试工作流

OpenClaw 风格的系统可以将技能用于：

- GitHub issue 分类
- 发布说明生成
- 天气与日程规划
- 网关运行手册
- 节点故障排查
- 应用 SDK 测试
- 安全审查
- GPU 性能分析

一个实用工作流：

```text
1. Contributor edits SKILL.md.
2. Contributor adds or updates evals/evals.json.
3. CI runs agent-skills-eval with baseline.
4. Report is uploaded as an artifact.
5. PR review checks pass rate, regressions, and judge evidence.
6. Maintainer decides whether the behavior change is acceptable.
```

建议的 PR 规则：

```text
No skill behavior change without at least one eval proving the intended behavior.
No regression accepted without an explicit note explaining why.
```

这与**代码应当如何测试**相呼应。

---

## 10. 设计好的技能评测

好的评测是**窄**的。

它测试**一个行为**。

坏的评测：

```text
Prompt: "Use the GitHub skill to manage issues well."
Assertion: "The answer is good."
```

好的评测：

```text
Prompt: "Given these three issue titles and labels, identify which one is a bug, which one is a feature request, and which one needs more information."
Assertions:
  - The output classifies all three issues.
  - The output asks for reproduction steps for the ambiguous bug report.
  - The output does not propose closing any issue without evidence.
```

好的技能评测应覆盖：

- 正常路径
- 模糊输入
- 缺失数据
- 对抗性指令
- 不安全操作请求
- 工具调用行为
- 来自真实 bug 的回归用例

一开始**不要把评测套件做得很大**。

从**最可能破坏用户信任**的三个用例开始。

---

## 11. 常见失效模式

### 技能没有带来提升

`with_skill` 和 `without_skill` 都通过。

解读：

```text
The model may already know this task,
or the eval is too easy.
```

修复：

- 让评测更具体
- 测试领域特定约束
- 测试工具调用行为
- 测试边缘情况

### 技能让输出变差

基线通过，技能失败。

解读：

```text
The skill is too broad, stale, misleading, or over-prescriptive.
```

修复：

- 缩短技能
- 移除过时规则
- 改进触发描述
- 添加反例
- 拆分为更小的技能

### 裁判不可靠

重复运行结果不一致。

修复：

- 降低 temperature
- 让断言更精确
- 添加确定性的检查
- 使用更强的裁判
- 人工审查产物

### 评测测试格式而非行为

修复：

- 断言结果，而非行文风格
- 对结构化输出使用 schema 检查
- 将风格测试与正确性测试分开

---

## 12. 本节如何与早前讲座关联

第 21 讲将技能引入为**工作流纪律**。

本讲增加了**缺失的测试回路**：

```text
skill design
  -> eval prompt
  -> with/without comparison
  -> judge + deterministic assertions
  -> artifact review
  -> skill revision
```

第 29 讲论证了测试是智能体化软件开发中的**持久资产**。

技能评测是**针对 agent 行为的测试**。

第 42 讲展示了用于 GPU kernel 翻译的技能。

`agent-skills-eval` 是测试这些翻译规则是否真正改进输出的方式。

第 44 讲使用 trace 作为性能主张的证据。

技能评测是**prompt/workflow 主张的证据**。

同一原则：

```text
No evidence, no claim.
```

---

## Mini-lab：为一个 OpenClaw 风格技能添加评测

选择一个技能：

- GitHub issue 分类
- 天气规划
- 网关故障排查
- 节点配对运行手册
- 应用 SDK 测试
- GPU trace 分析

创建：

```text
SKILL.md
evals/evals.json
agent-skills-eval.yaml
```

运行：

```bash
npx agent-skills-eval ./skills \
  --target gpt-4o-mini \
  --judge gpt-4o-mini \
  --baseline \
  --strict
```

然后写一份简短报告：

```text
Skill:
Eval count:
Pass rate with skill:
Pass rate without skill:
Cases improved:
Cases regressed:
Judge concerns:
Deterministic assertions needed:
Decision:
```

如果技能**未超过基线**，不要按原样发布。

要么**改进技能**，要么承认该技能不必要。

---

## 关键要点

- 技能需要测试，因为它们会改变 agent 行为。
- `with_skill` 与 `without_skill` 的对比是衡量技能提升的核心模式。
- 大语言模型裁判与显式断言和存储的产物配合使用时很有用。
- 对于工具调用和协议行为，需要确定性的断言。
- 产物输出让技能审查可重复且对 CI 友好。
- OpenClaw 贡献者可以使用此模式在合并前验证技能变更。
- 好的评测测试具体行为、边缘情况、安全约束和回归。
- 未超过基线的技能并不自动值得保留。

---


<details>
<summary>English original</summary>

**9. OpenClaw skill testing workflow**

OpenClaw-style systems can use skills for:

- GitHub issue triage
- release note generation
- weather and schedule planning
- Gateway runbooks
- node troubleshooting
- app SDK testing
- security review
- GPU performance analysis

A practical workflow:

```text
1. Contributor edits SKILL.md.
2. Contributor adds or updates evals/evals.json.
3. CI runs agent-skills-eval with baseline.
4. Report is uploaded as an artifact.
5. PR review checks pass rate, regressions, and judge evidence.
6. Maintainer decides whether the behavior change is acceptable.
```

Suggested PR rule:

```text
No skill behavior change without at least one eval proving the intended behavior.
No regression accepted without an explicit note explaining why.
```

This mirrors **how code should be tested**.

---

**10. Designing good skill evals**

A good eval is **narrow**.

It tests **one behavior**.

Bad eval:

```text
Prompt: "Use the GitHub skill to manage issues well."
Assertion: "The answer is good."
```

Good eval:

```text
Prompt: "Given these three issue titles and labels, identify which one is a bug, which one is a feature request, and which one needs more information."
Assertions:
  - The output classifies all three issues.
  - The output asks for reproduction steps for the ambiguous bug report.
  - The output does not propose closing any issue without evidence.
```

Good skill evals should cover:

- happy path
- ambiguous input
- missing data
- adversarial instruction
- unsafe action request
- tool-call behavior
- regression case from a real bug

Do **not make the eval suite huge** at first.

Start with the three cases **most likely to break user trust**.

---

**11. Common failure modes**

**The skill adds no lift**

Both `with_skill` and `without_skill` pass.

Interpretation:

```text
The model may already know this task,
or the eval is too easy.
```

Fix:

- make the eval more specific
- test domain-specific constraints
- test tool-call behavior
- test edge cases

**The skill makes output worse**

Baseline passes, skill fails.

Interpretation:

```text
The skill is too broad, stale, misleading, or over-prescriptive.
```

Fix:

- shorten the skill
- remove stale rules
- improve trigger description
- add counterexamples
- split into smaller skills

**The judge is unreliable**

Repeated runs disagree.

Fix:

- lower temperature
- sharpen assertions
- add deterministic checks
- use a stronger judge
- manually review artifacts

**The eval tests formatting instead of behavior**

Fix:

- assert outcome, not prose style
- use schema checks for structured outputs
- separate style tests from correctness tests

---

**12. How this connects to earlier lectures**

Lecture 21 introduced skills as **workflow discipline**.

This lecture adds the **missing test loop**:

```text
skill design
  -> eval prompt
  -> with/without comparison
  -> judge + deterministic assertions
  -> artifact review
  -> skill revision
```

Lecture 29 argued that tests are the **durable asset** in agentic software development.

Skill evals are the **tests for agent behavior**.

Lecture 42 showed skills for GPU kernel translation.

`agent-skills-eval` is how you test whether those translation rules actually improve outputs.

Lecture 44 used traces as evidence for performance claims.

Skill evals are **evidence for prompt/workflow claims**.

Same principle:

```text
No evidence, no claim.
```

---

**Mini-lab: Add evals to one OpenClaw-style skill**

Pick one skill:

- GitHub issue triage
- weather planning
- Gateway troubleshooting
- node pairing runbook
- app SDK testing
- GPU trace analysis

Create:

```text
SKILL.md
evals/evals.json
agent-skills-eval.yaml
```

Run:

```bash
npx agent-skills-eval ./skills \
  --target gpt-4o-mini \
  --judge gpt-4o-mini \
  --baseline \
  --strict
```

Then write a short report:

```text
Skill:
Eval count:
Pass rate with skill:
Pass rate without skill:
Cases improved:
Cases regressed:
Judge concerns:
Deterministic assertions needed:
Decision:
```

If the skill does **not beat baseline**, do not ship it as-is.

Either **improve the skill** or admit the skill is unnecessary.

---

**Key takeaways**

- Skills need tests because they change agent behavior.
- `with_skill` versus `without_skill` is the core pattern for measuring skill lift.
- LLM judges are useful when paired with explicit assertions and stored artifacts.
- Deterministic assertions are required for tool-call and protocol behavior.
- Artifact output makes skill reviews repeatable and CI-friendly.
- OpenClaw contributors can use this pattern to validate skill changes before merge.
- Good evals test concrete behavior, edge cases, safety constraints, and regressions.
- A skill that does not beat baseline is not automatically worth carrying.

---

</details>

## 参考文献

- agent-skills-eval 仓库：[https://github.com/darkrishabh/agent-skills-eval](https://github.com/darkrishabh/agent-skills-eval)
- agent-skills-eval 文档：[https://darkrishabh.github.io/agent-skills-eval](https://darkrishabh.github.io/agent-skills-eval)
- Agent Skills 概览：[https://agentskills.io/home](https://agentskills.io/home)
- Lecture 21 - Agent Skills：[Lecture-21.md](Lecture-21.md)
- Lecture 29 - Agentic SDLC：[Lecture-29.md](Lecture-29.md)
- Agent Skills for GPU Kernel Translation → [MLSys Deep Dives · Lecture 01](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/Lecture-01.md)

---

*下一章：[Lecture 23](Lecture-23.md)*


<details>
<summary>English original</summary>

**References**

- agent-skills-eval repository: [https://github.com/darkrishabh/agent-skills-eval](https://github.com/darkrishabh/agent-skills-eval)
- agent-skills-eval documentation: [https://darkrishabh.github.io/agent-skills-eval](https://darkrishabh.github.io/agent-skills-eval)
- Agent Skills overview: [https://agentskills.io/home](https://agentskills.io/home)
- Lecture 21 - Agent Skills: [Lecture-21.md](Lecture-21.md)
- Lecture 29 - Agentic SDLC: [Lecture-29.md](Lecture-29.md)
- Agent Skills for GPU Kernel Translation → [MLSys Deep Dives · Lecture 01](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/Lecture-01.md)

---

*Next: [Lecture 23](Lecture-23.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-22.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-22.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
