---
title: Lab 04 — 终端密集型 agent 的 TokenJuice 输出压缩
description: Lab 04 — 终端密集型 agent 的 TokenJuice 输出压缩
---

# Lab 04 — 终端密集型 agent 的 TokenJuice 输出压缩

**方向 B · Agentic AI & GenAI** | [← 索引](README.md) | [上一页 → Lab 03](Lab-03-Production-RAG.md) | [下一页 → Lab 05](Lab-05-OpenMeow-App-SDK-Dogfood.md)

---

## 概述

这是一个小巧有趣的系统 lab。

你将使用 [TokenJuice](https://github.com/vincentkoc/tokenjuice) 在嘈杂的终端输出进入 agent 记录之前对其进行压缩。

核心思想很简单：

```text
run command normally
  -> observe output
  -> deterministically reduce prompt-facing text
  -> keep raw output available only when explicitly needed
```

TokenJuice 不是大语言模型摘要器。

它是面向终端密集型工作流的规则驱动输出归约器，例如：

- `git status`
- `pnpm test`
- `docker build`
- `rg --files`
- `pnpm --help`
- build 日志
- lint 输出
- 包管理器噪声

本 lab 的目标是衡量压缩能否在不隐藏关键调试信息的前提下改善 agent 工作流质量。

**预计用时：** 45-75 分钟

**难度：** 初级到中级

---

## 为什么重要

agent 工作流把上下文浪费在终端输出上。

示例：

```text
agent runs pnpm test
  -> receives 800 lines
  -> only 25 lines matter
  -> transcript fills with noise
  -> next turn has less useful context
  -> agent reruns commands because it missed the important part
```

TokenJuice 通过让终端输出更精简来消除这类浪费。

关键设计特性：

- 命令语义保持不变
- 归约发生在执行之后
- 规则是可检视的 JSON
- 通过显式旁路可获得原始输出
- 宿主集成只是同一个归约器的薄封装
- 项目规则可以覆盖内置规则

对 agent 而言，这正是那种恰到好处的"无聊"基础设施。

---

## 学习目标

完成本 lab 后，你应能够：

1. 解释为什么终端输出对 agent 而言是 token 预算问题。
2. 使用 `tokenjuice reduce` 压缩已有日志。
3. 使用 `tokenjuice wrap` 运行命令并压缩其观测到的输出。
4. 在精确字节重要时，使用 `--raw`、`--full` 和产物存储。
5. 用 `reduce-json` 检视面向机器的输出。
6. 理解规则优先级模型。
7. 编写一个小型项目专属归约器。
8. 判断何时压缩是安全的、何时是危险的。
9. 将 TokenJuice 风格的归约器接入 OpenClaw、Codex、Claude Code 及其他 agent harness。

---

## 步骤 0 — 安全模型

在把 hook 安装到 agent 之前，先理解安全边界。

TokenJuice 不应：

- 静默改写命令
- 把有损输出伪装成完整输出
- 用大语言模型做摘要
- 在需要精确字节时隐藏原始输出
- 压缩精确的文件内容读取，如 `cat`、`sed`、`head` 或 `tail`

好的用法：

```text
inventory commands
test logs
build logs
package-manager output
lint summaries
```

有风险的用法：

```text
security logs
binary dumps
exact config file reads
one-off debugging where every byte matters
mixed shell sequences with side effects
```

必要时使用原始模式：

```bash
tokenjuice wrap --raw -- git status
tokenjuice wrap --full -- pnpm --help
```

---

## 步骤 1 — 安装 TokenJuice

用你偏好的包管理器安装：

```bash
npm install -g tokenjuice
```

或者：

```bash
pnpm add -g tokenjuice
```

或者，如果使用 Homebrew：

```bash
brew tap vincentkoc/tap
brew install tokenjuice
```

验证：

```bash
tokenjuice --version
tokenjuice --help
```

如果不想要全局安装，使用一个临时项目：

```bash
mkdir tokenjuice-lab
cd tokenjuice-lab
pnpm init
pnpm add -D tokenjuice
pnpm exec tokenjuice --version
```

在本 lab 的其余部分，如果使用本地安装，把 `tokenjuice` 替换为 `pnpm exec tokenjuice`。

---


<details>
<summary>English original</summary>

**Lab 04 — TokenJuice Output Compaction for Terminal-Heavy Agents**

**Track B · Agentic AI & GenAI** | [← Index](README.md) | [Previous → Lab 03](Lab-03-Production-RAG.md) | [Next → Lab 05](Lab-05-OpenMeow-App-SDK-Dogfood.md)

---

**Overview**

This is a small, fun systems lab.

You will use [TokenJuice](https://github.com/vincentkoc/tokenjuice) to compact noisy terminal output before it enters an agent transcript.

The core idea is simple:

```text
run command normally
  -> observe output
  -> deterministically reduce prompt-facing text
  -> keep raw output available only when explicitly needed
```

TokenJuice is not an LLM summarizer.

It is a rule-driven output reducer for terminal-heavy workflows such as:

- `git status`
- `pnpm test`
- `docker build`
- `rg --files`
- `pnpm --help`
- build logs
- lint output
- package-manager noise

The lab goal is to measure whether compaction improves agent workflow quality without hiding critical debugging information.

**Estimated time:** 45-75 minutes

**Difficulty:** Beginner to intermediate

---

**Why this matters**

Agent workflows waste context on terminal output.

Example:

```text
agent runs pnpm test
  -> receives 800 lines
  -> only 25 lines matter
  -> transcript fills with noise
  -> next turn has less useful context
  -> agent reruns commands because it missed the important part
```

TokenJuice attacks that waste by making terminal output leaner.

The key design properties:

- command semantics stay untouched
- reduction happens after execution
- rules are inspectable JSON
- raw output is available through explicit bypasses
- host integrations stay thin wrappers around the same reducer
- project rules can override built-in rules

This is the right kind of "boring" infrastructure for agents.

---

**Learning objectives**

By the end of this lab, you should be able to:

1. Explain why terminal output is a token-budget problem for agents.
2. Use `tokenjuice reduce` to compact existing logs.
3. Use `tokenjuice wrap` to run a command and compact its observed output.
4. Use `--raw`, `--full`, and artifact storage when exact bytes matter.
5. Inspect machine-facing output with `reduce-json`.
6. Understand the rule precedence model.
7. Write a small project-specific reducer.
8. Decide when compaction is safe and when it is dangerous.
9. Connect TokenJuice-style reducers to OpenClaw, Codex, Claude Code, and other agent harnesses.

---

**Step 0 — Safety model**

Before installing hooks into an agent, understand the safety boundary.

TokenJuice should not:

- rewrite commands silently
- pretend lossy output is complete
- summarize with an LLM
- hide raw output when exact bytes are required
- compact exact file-content reads such as `cat`, `sed`, `head`, or `tail`

Good use:

```text
inventory commands
test logs
build logs
package-manager output
lint summaries
```

Risky use:

```text
security logs
binary dumps
exact config file reads
one-off debugging where every byte matters
mixed shell sequences with side effects
```

Use raw mode when necessary:

```bash
tokenjuice wrap --raw -- git status
tokenjuice wrap --full -- pnpm --help
```

---

**Step 1 — Install TokenJuice**

Install with your preferred package manager:

```bash
npm install -g tokenjuice
```

or:

```bash
pnpm add -g tokenjuice
```

or, if using Homebrew:

```bash
brew tap vincentkoc/tap
brew install tokenjuice
```

Verify:

```bash
tokenjuice --version
tokenjuice --help
```

If you do not want a global install, use a scratch project:

```bash
mkdir tokenjuice-lab
cd tokenjuice-lab
pnpm init
pnpm add -D tokenjuice
pnpm exec tokenjuice --version
```

For the rest of the lab, replace `tokenjuice` with `pnpm exec tokenjuice` if using a local install.

---

</details>

## Step 2 — 创建带噪声的样例输出

创建实验文件夹：

```bash
mkdir -p tokenjuice-lab/logs
cd tokenjuice-lab
```

创建一个假测试日志：

```bash
cat > logs/test-output.txt <<'EOF'
RUN  v3.2.4 /repo

stdout | packages/core/test/reducer.test.ts > reducer keeps failure detail
loading config from /repo/tokenjuice.config.json
loading built-in rules from src/rules
loading user rules from ~/.config/tokenjuice/rules
loading project rules from .tokenjuice/rules

✓ packages/core/test/classify.test.ts (28 tests) 132ms
✓ packages/core/test/command.test.ts (42 tests) 188ms
✓ packages/core/test/artifacts.test.ts (17 tests) 96ms
✓ packages/hosts/test/codex.test.ts (18 tests) 120ms
✓ packages/hosts/test/claude-code.test.ts (21 tests) 140ms

stderr | packages/core/test/reducer.test.ts > reducer keeps failure detail
AssertionError: expected reducer to preserve "exit code 2"
  at test/core/reducer.test.ts:88:15
  at runTest test-runner.ts:55:7

FAIL packages/core/test/reducer.test.ts > reducer keeps failure detail
Expected preserved lines:
  exit code 2
  src/rules/fixtures/pnpm-test-failure.txt
Received:
  exit code omitted

Test Files  1 failed | 5 passed (6)
Tests       1 failed | 126 passed (127)
Duration    2.8s
EOF
```

检查原始大小：

```bash
wc -l logs/test-output.txt
wc -c logs/test-output.txt
```

---

## Step 3 — 缩减已有文本

运行：

```bash
tokenjuice reduce logs/test-output.txt
```

再与管道对比：

```bash
cat logs/test-output.txt | tokenjuice reduce
```

记录：

```text
raw line count:
reduced line count:
what details were preserved:
what details were removed:
```

重要的问题不是“它更短了吗？”

重要的问题是：

```text
Would an agent still know what to do next?
```

对于失败的测试，reducer 应保留足够细节以识别：

- 失败的文件
- 失败的测试名
- expected 与 received 线索
- 栈帧或源码位置
- 失败总数

---

## Step 4 — 包装真实命令

现在直接通过 TokenJuice 运行命令。

先做安全的清点：

```bash
tokenjuice wrap -- git status --short
tokenjuice wrap -- git ls-files
```

试一个噪声很大的 help 命令：

```bash
tokenjuice wrap -- pnpm --help
```

试一个可能失败的命令：

```bash
tokenjuice wrap -- bash -lc 'echo "compile start"; echo "error TS2322: Type string is not assignable"; exit 2'
```

观察：

- 执行后输出被压缩
- 命令仍正常运行
- 非零退出的保留细节应多于成功时的噪声

这是关键区别：

```text
TokenJuice is not a shell replacement.
It is an output reducer around normal command execution.
```

---

## Step 5 — 使用 raw 与 full 旁路

压缩有用，直到它不再有用。

运行：

```bash
tokenjuice wrap --raw -- pnpm --help
tokenjuice wrap --full -- git status
```

在以下情况使用 raw/full 模式：

- 需要精确文本
- 正在调试 reducer
- fixture 需要精确的期望输出
- agent 需要类文件响应的每一行
- 缩减后的输出隐藏了相关的中间部分

把这条规则写进你自己的 agent 实践：

```text
Default to compact for noisy terminal output.
Use raw when exact bytes are part of the task.
```

---

## Step 6 — 存储与检查产物

显式请求时，TokenJuice 可将原始输出存储为产物。

运行：

```bash
tokenjuice wrap --store -- bash -lc 'for i in $(seq 1 120); do echo "line $i"; done'
```

列出产物：

```bash
tokenjuice ls
```

检查其中一个：

```bash
tokenjuice cat <artifact-id>
```

为什么这很重要：

```text
prompt-facing output can be compact
while raw output remains recoverable by explicit action
```

对 agent 转录而言，这是正确的折中。

---

## Step 7 — 检查面向机器的 JSON

宿主集成需要稳定的机器输出。

创建 tool payload：

```bash
cat > payload.json <<'EOF'
{
  "toolName": "exec",
  "command": "pnpm test",
  "argv": ["pnpm", "test"],
  "combinedText": "RUN  v3.2.4 /repo\nFAIL packages/core/test/reducer.test.ts > reducer keeps failure detail\nAssertionError: expected reducer to preserve exit code 2\nTest Files 1 failed | 5 passed\nTests 1 failed | 126 passed\n",
  "exitCode": 1
}
EOF
```

运行：

```bash
tokenjuice reduce-json payload.json
```

查找：

- 缩减后的输出文本
- 命令分类
- 节省量信息
- 失败上下文是否被保留

这是宿主适配器应使用的接口。

面向人的 CLI 可以灵活。

适配器协议应当乏味且结构化。

---


<details>
<summary>English original</summary>

**Step 2 — Create noisy sample output**

Create a lab folder:

```bash
mkdir -p tokenjuice-lab/logs
cd tokenjuice-lab
```

Create a fake test log:

```bash
cat > logs/test-output.txt <<'EOF'
RUN  v3.2.4 /repo

stdout | packages/core/test/reducer.test.ts > reducer keeps failure detail
loading config from /repo/tokenjuice.config.json
loading built-in rules from src/rules
loading user rules from ~/.config/tokenjuice/rules
loading project rules from .tokenjuice/rules

✓ packages/core/test/classify.test.ts (28 tests) 132ms
✓ packages/core/test/command.test.ts (42 tests) 188ms
✓ packages/core/test/artifacts.test.ts (17 tests) 96ms
✓ packages/hosts/test/codex.test.ts (18 tests) 120ms
✓ packages/hosts/test/claude-code.test.ts (21 tests) 140ms

stderr | packages/core/test/reducer.test.ts > reducer keeps failure detail
AssertionError: expected reducer to preserve "exit code 2"
  at test/core/reducer.test.ts:88:15
  at runTest test-runner.ts:55:7

FAIL packages/core/test/reducer.test.ts > reducer keeps failure detail
Expected preserved lines:
  exit code 2
  src/rules/fixtures/pnpm-test-failure.txt
Received:
  exit code omitted

Test Files  1 failed | 5 passed (6)
Tests       1 failed | 126 passed (127)
Duration    2.8s
EOF
```

Check raw size:

```bash
wc -l logs/test-output.txt
wc -c logs/test-output.txt
```

---

**Step 3 — Reduce existing text**

Run:

```bash
tokenjuice reduce logs/test-output.txt
```

Also compare with a pipe:

```bash
cat logs/test-output.txt | tokenjuice reduce
```

Record:

```text
raw line count:
reduced line count:
what details were preserved:
what details were removed:
```

The important question is not "was it shorter?"

The important question is:

```text
Would an agent still know what to do next?
```

For a failing test, the reducer should preserve enough detail to identify:

- failing file
- failing test name
- expected versus received clue
- stack frame or source location
- total failure count

---

**Step 4 — Wrap real commands**

Now run commands through TokenJuice directly.

Start with safe inventory:

```bash
tokenjuice wrap -- git status --short
tokenjuice wrap -- git ls-files
```

Try a noisy help command:

```bash
tokenjuice wrap -- pnpm --help
```

Try a command that may fail:

```bash
tokenjuice wrap -- bash -lc 'echo "compile start"; echo "error TS2322: Type string is not assignable"; exit 2'
```

Observe:

- output is compacted after execution
- the command still runs normally
- non-zero exits should preserve more detail than success noise

This is the key distinction:

```text
TokenJuice is not a shell replacement.
It is an output reducer around normal command execution.
```

---

**Step 5 — Use raw and full bypasses**

Compaction is useful until it is not.

Run:

```bash
tokenjuice wrap --raw -- pnpm --help
tokenjuice wrap --full -- git status
```

Use raw/full modes when:

- exact text is required
- you are debugging a reducer
- a fixture needs exact expected output
- the agent needs every line of a file-like response
- the reduced output hides a relevant middle section

Write this rule into your own agent practice:

```text
Default to compact for noisy terminal output.
Use raw when exact bytes are part of the task.
```

---

**Step 6 — Store and inspect artifacts**

TokenJuice can store raw output as an artifact when explicitly requested.

Run:

```bash
tokenjuice wrap --store -- bash -lc 'for i in $(seq 1 120); do echo "line $i"; done'
```

List artifacts:

```bash
tokenjuice ls
```

Inspect one:

```bash
tokenjuice cat <artifact-id>
```

Why this matters:

```text
prompt-facing output can be compact
while raw output remains recoverable by explicit action
```

That is the right compromise for agent transcripts.

---

**Step 7 — Inspect machine-facing JSON**

Host integrations need stable machine output.

Create a tool payload:

```bash
cat > payload.json <<'EOF'
{
  "toolName": "exec",
  "command": "pnpm test",
  "argv": ["pnpm", "test"],
  "combinedText": "RUN  v3.2.4 /repo\nFAIL packages/core/test/reducer.test.ts > reducer keeps failure detail\nAssertionError: expected reducer to preserve exit code 2\nTest Files 1 failed | 5 passed\nTests 1 failed | 126 passed\n",
  "exitCode": 1
}
EOF
```

Run:

```bash
tokenjuice reduce-json payload.json
```

Look for:

- reduced output text
- command classification
- savings information
- whether failure context was preserved

This is the surface host adapters should use.

Human-facing CLIs can be flexible.

Adapter protocols should be boring and structured.

---

</details>

## Step 8 — 校验规则

TokenJuice 规则是 JSON。

内置规则位于包内。覆盖规则可位于：

```text
~/.config/tokenjuice/rules
.tokenjuice/rules
```

规则优先级：

```text
built-in rules
  -> user rules
  -> project rules
```

后出现的 layer 按 rule id 覆盖先前的 layer。

运行：

```bash
tokenjuice verify
```

如果有 fixture 可用：

```bash
tokenjuice verify --fixtures
```

这应检查：

- JSON 可解析
- schema 结构有效
- 正则表达式可编译
- 同一 layer 内重复的 id 会被拒绝
- fixture 预期仍与 reducer 匹配

---

## Step 9 — 编写一个微型项目 reducer

创建项目覆盖文件夹：

```bash
mkdir -p .tokenjuice/rules
```

为伪造的硬件 bring-up（上电点亮/调通）日志创建 reducer：

```bash
cat > logs/otbr-output.txt <<'EOF'
Apr 20 09:34:20 ubuntu otbr-agent[10839]: Attach attempt 8, AnyPartition
Apr 20 09:34:20 ubuntu otbr-agent[10839]: Send Parent Request to routers
Apr 20 09:34:22 ubuntu otbr-agent[10839]: Attach attempt 8 unsuccessful, will try again in 32.128 seconds
Apr 20 09:34:46 ubuntu otbr-agent[11149]: Running 0.3.0-1e957ca
Apr 20 09:34:46 ubuntu otbr-agent[11149]: Thread version: 1.4.0
Apr 20 09:34:46 ubuntu otbr-agent[11149]: Radio URL: spinel+hdlc+uart:///dev/ttyTHS1?uart-baudrate=460800
Apr 20 09:34:46 ubuntu otbr-agent[11149]: InitMulticastRouterSock() at multicast_routing.cpp:227: Protocol not available
Apr 20 09:34:47 ubuntu otbr-agent[11149]: TrelDiscoverer: DNS-SD service registered successfully
EOF
```

创建一条项目规则：

```bash
cat > .tokenjuice/rules/otbr-journal.json <<'EOF'
{
  "id": "otbr/journal",
  "family": "otbr-journal",
  "match": {
    "commandIncludes": ["journalctl", "otbr-agent"]
  },
  "transforms": {
    "trimEmptyEdges": true,
    "dedupeAdjacent": true
  },
  "filters": {
    "keepPatterns": [
      "Attach attempt",
      "unsuccessful",
      "Radio URL",
      "Thread version",
      "Protocol not available",
      "InitMulticastRouterSock"
    ],
    "skipPatterns": [
      "DNS-SD service registered successfully"
    ]
  },
  "summarize": {
    "head": 20,
    "tail": 8
  },
  "failure": {
    "preserveOnFailure": true,
    "head": 24,
    "tail": 12
  },
  "counters": [
    {
      "name": "attach_attempts",
      "pattern": "Attach attempt"
    },
    {
      "name": "kernel_missing_mroute",
      "pattern": "Protocol not available"
    }
  ]
}
EOF
```

验证：

```bash
tokenjuice verify
```

用 JSON 输入测试：

```bash
cat > otbr-payload.json <<'EOF'
{
  "toolName": "exec",
  "command": "journalctl -u otbr-agent -n 80 --no-pager",
  "argv": ["journalctl", "-u", "otbr-agent", "-n", "80", "--no-pager"],
  "combinedText": "",
  "exitCode": 0
}
EOF
```

注入日志内容：

```bash
python3 - <<'PY'
import json
from pathlib import Path

payload = json.loads(Path("otbr-payload.json").read_text())
payload["combinedText"] = Path("logs/otbr-output.txt").read_text()
Path("otbr-payload.json").write_text(json.dumps(payload, indent=2))
PY
```

运行：

```bash
tokenjuice reduce-json otbr-payload.json
```

如果规则不匹配，检查文档并简化 `match` 块。

学习目标不是记住确切的 schema。

学习目标是理解项目本地的 reducer 可以编码领域特定的信号。

---

## Step 10 — 测量节省

创建一个简单的测量脚本：

```bash
cat > measure.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

file="${1:?usage: ./measure.sh <file>}"
raw_bytes="$(wc -c < "$file" | tr -d ' ')"
reduced="$(tokenjuice reduce "$file")"
reduced_bytes="$(printf "%s" "$reduced" | wc -c | tr -d ' ')"

python3 - <<PY
raw = int("$raw_bytes")
reduced = int("$reduced_bytes")
savings = 0 if raw == 0 else (1 - reduced / raw) * 100
print(f"raw bytes:     {raw}")
print(f"reduced bytes: {reduced}")
print(f"savings:       {savings:.1f}%")
PY
EOF

chmod +x measure.sh
```

运行：

```bash
./measure.sh logs/test-output.txt
./measure.sh logs/otbr-output.txt
```

对于本实验，节省效果好还不够。

你还必须回答：

```text
Did the compacted output preserve the next action?
```

对于 OTBR 示例，压缩后的输出仍应保留：

- attach attempts failed
- Thread version
- Radio URL
- `Protocol not available`
- likely kernel multicast routing issue

如果这些消失，说明 reducer 过于激进。


<details>
<summary>English original</summary>

**Step 8 — Verify rules**

TokenJuice rules are JSON.

Built-in rules live in the package. Overrides can live in:

```text
~/.config/tokenjuice/rules
.tokenjuice/rules
```

Rule precedence:

```text
built-in rules
  -> user rules
  -> project rules
```

Later layers override earlier layers by rule id.

Run:

```bash
tokenjuice verify
```

If fixtures are available:

```bash
tokenjuice verify --fixtures
```

This should check:

- JSON parses
- schema shape is valid
- regexes compile
- duplicate ids are rejected inside the same layer
- fixture expectations still match reducers

---

**Step 9 — Write a tiny project reducer**

Create a project override folder:

```bash
mkdir -p .tokenjuice/rules
```

Create a reducer for a fake hardware bring-up log:

```bash
cat > logs/otbr-output.txt <<'EOF'
Apr 20 09:34:20 ubuntu otbr-agent[10839]: Attach attempt 8, AnyPartition
Apr 20 09:34:20 ubuntu otbr-agent[10839]: Send Parent Request to routers
Apr 20 09:34:22 ubuntu otbr-agent[10839]: Attach attempt 8 unsuccessful, will try again in 32.128 seconds
Apr 20 09:34:46 ubuntu otbr-agent[11149]: Running 0.3.0-1e957ca
Apr 20 09:34:46 ubuntu otbr-agent[11149]: Thread version: 1.4.0
Apr 20 09:34:46 ubuntu otbr-agent[11149]: Radio URL: spinel+hdlc+uart:///dev/ttyTHS1?uart-baudrate=460800
Apr 20 09:34:46 ubuntu otbr-agent[11149]: InitMulticastRouterSock() at multicast_routing.cpp:227: Protocol not available
Apr 20 09:34:47 ubuntu otbr-agent[11149]: TrelDiscoverer: DNS-SD service registered successfully
EOF
```

Create a project rule:

```bash
cat > .tokenjuice/rules/otbr-journal.json <<'EOF'
{
  "id": "otbr/journal",
  "family": "otbr-journal",
  "match": {
    "commandIncludes": ["journalctl", "otbr-agent"]
  },
  "transforms": {
    "trimEmptyEdges": true,
    "dedupeAdjacent": true
  },
  "filters": {
    "keepPatterns": [
      "Attach attempt",
      "unsuccessful",
      "Radio URL",
      "Thread version",
      "Protocol not available",
      "InitMulticastRouterSock"
    ],
    "skipPatterns": [
      "DNS-SD service registered successfully"
    ]
  },
  "summarize": {
    "head": 20,
    "tail": 8
  },
  "failure": {
    "preserveOnFailure": true,
    "head": 24,
    "tail": 12
  },
  "counters": [
    {
      "name": "attach_attempts",
      "pattern": "Attach attempt"
    },
    {
      "name": "kernel_missing_mroute",
      "pattern": "Protocol not available"
    }
  ]
}
EOF
```

Verify:

```bash
tokenjuice verify
```

Test with JSON input:

```bash
cat > otbr-payload.json <<'EOF'
{
  "toolName": "exec",
  "command": "journalctl -u otbr-agent -n 80 --no-pager",
  "argv": ["journalctl", "-u", "otbr-agent", "-n", "80", "--no-pager"],
  "combinedText": "",
  "exitCode": 0
}
EOF
```

Inject the log content:

```bash
python3 - <<'PY'
import json
from pathlib import Path

payload = json.loads(Path("otbr-payload.json").read_text())
payload["combinedText"] = Path("logs/otbr-output.txt").read_text()
Path("otbr-payload.json").write_text(json.dumps(payload, indent=2))
PY
```

Run:

```bash
tokenjuice reduce-json otbr-payload.json
```

If the rule does not match, inspect the docs and simplify the `match` block.

The learning goal is not to memorize the exact schema.

The learning goal is to understand that project-local reducers can encode domain-specific signal.

---

**Step 10 — Measure savings**

Create a simple measurement script:

```bash
cat > measure.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail

file="${1:?usage: ./measure.sh <file>}"
raw_bytes="$(wc -c < "$file" | tr -d ' ')"
reduced="$(tokenjuice reduce "$file")"
reduced_bytes="$(printf "%s" "$reduced" | wc -c | tr -d ' ')"

python3 - <<PY
raw = int("$raw_bytes")
reduced = int("$reduced_bytes")
savings = 0 if raw == 0 else (1 - reduced / raw) * 100
print(f"raw bytes:     {raw}")
print(f"reduced bytes: {reduced}")
print(f"savings:       {savings:.1f}%")
PY
EOF

chmod +x measure.sh
```

Run:

```bash
./measure.sh logs/test-output.txt
./measure.sh logs/otbr-output.txt
```

For this lab, good savings are not enough.

You must also answer:

```text
Did the compacted output preserve the next action?
```

For the OTBR example, the compacted output should still preserve:

- attach attempts failed
- Thread version
- Radio URL
- `Protocol not available`
- likely kernel multicast routing issue

If those disappear, the reducer is too aggressive.

---

</details>

## Step 11 — 连接到 agent 主机

TokenJuice 支持多种主机集成，包括 Codex CLI、Claude Code、Cursor、OpenCode、pi 和 OpenClaw。

对于 Codex CLI：

```bash
tokenjuice install codex
tokenjuice doctor codex
```

对于 Claude Code：

```bash
tokenjuice install claude-code
tokenjuice doctor claude-code
```

对于聚合 hook 状态：

```bash
tokenjuice doctor hooks
```

对于 OpenClaw，TokenJuice 支持由 OpenClaw 侧内置提供。上游文档建议启用该插件，而不是运行 `tokenjuice install openclaw`：

```bash
openclaw config set plugins.entries.tokenjuice.enabled true
```

这要求 OpenClaw `2026.4.22` 或更新版本。

在你并不了解 hook 行为的机器上，不要安装主机 hook。

先用 `doctor`，并保留回滚路径。

---

## Step 12 — 评测该实验

构建一个小表格：

| 命令或日志 | 原始字节数 | 缩减后字节数 | 节省 | 是否保留了下一步动作？ |
|---|---:|---:|---:|---|
| `logs/test-output.txt` |  |  |  |  |
| `logs/otbr-output.txt` |  |  |  |  |
| `git ls-files` |  |  |  |  |
| `pnpm --help` |  |  |  |  |

然后写一段简短结论：

```text
TokenJuice is useful for:

TokenJuice is risky for:

The reducer I would add next is:

The command family I would always keep raw is:
```

---

## 你应当学到什么

TokenJuice 之所以有趣，是因为它的产品话术很俏皮：“token 减重”。

但其背后的系统设计思想是严肃的：

```text
agent productivity depends on transcript hygiene.
```

重度依赖终端的 agent 需要：

- 更少的噪声
- 保留的失败线索
- 确定性的行为
- 显式的 raw 旁路
- 可恢复的产物
- 可检视的规则
- 可度量的节省

这只是一个小工具，却触及了一个真实的生产问题。

---

## 扩展

1. **为 Jetson 音频日志添加 reducer** — 保留 `arecord`、`aplay`、APE card、I2S2 和 ALSA 控制行。
2. **为 CUDA 性能分析器输出添加 reducer** — 保留 kernel 名称、occupancy、内存吞吐和警告。
3. **添加 fixture 测试** — 为你的项目 reducer 创建前后对比的预期输出。
4. **添加 OpenClaw 工作流说明** — 明确你的 agent 何时应使用 raw 输出、何时使用压缩输出。
5. **与 LLM 摘要对比** — 把同样的日志交给 LLM 摘要，比较确定性、成本和遗漏的细节。

---

## 参考资料

- TokenJuice repository: [https://github.com/vincentkoc/tokenjuice](https://github.com/vincentkoc/tokenjuice)
- TokenJuice spec: [https://github.com/vincentkoc/tokenjuice/blob/main/docs/spec.md](https://github.com/vincentkoc/tokenjuice/blob/main/docs/spec.md)
- TokenJuice rules: [https://github.com/vincentkoc/tokenjuice/blob/main/docs/rules.md](https://github.com/vincentkoc/tokenjuice/blob/main/docs/rules.md)
- TokenJuice integration playbook: [https://github.com/vincentkoc/tokenjuice/blob/main/docs/integration-playbook.md](https://github.com/vincentkoc/tokenjuice/blob/main/docs/integration-playbook.md)

---

*下一篇：[Lab 05 — OpenMeow App SDK Dogfood on macOS](Lab-05-OpenMeow-App-SDK-Dogfood.md)*


<details>
<summary>English original</summary>

**Step 11 — Connect to an agent host**

TokenJuice supports several host integrations, including Codex CLI, Claude Code, Cursor, OpenCode, pi, and OpenClaw.

For Codex CLI:

```bash
tokenjuice install codex
tokenjuice doctor codex
```

For Claude Code:

```bash
tokenjuice install claude-code
tokenjuice doctor claude-code
```

For aggregate hook state:

```bash
tokenjuice doctor hooks
```

For OpenClaw, TokenJuice support is bundled on the OpenClaw side. The upstream docs say to enable the plugin instead of running `tokenjuice install openclaw`:

```bash
openclaw config set plugins.entries.tokenjuice.enabled true
```

This requires OpenClaw `2026.4.22` or newer.

Do not install host hooks on machines where you do not understand the hook behavior.

Use `doctor` first, and keep a rollback path.

---

**Step 12 — Evaluate the lab**

Build a small table:

| Command or log | Raw bytes | Reduced bytes | Savings | Did it preserve next action? |
|---|---:|---:|---:|---|
| `logs/test-output.txt` |  |  |  |  |
| `logs/otbr-output.txt` |  |  |  |  |
| `git ls-files` |  |  |  |  |
| `pnpm --help` |  |  |  |  |

Then write a short conclusion:

```text
TokenJuice is useful for:

TokenJuice is risky for:

The reducer I would add next is:

The command family I would always keep raw is:
```

---

**What you should learn**

TokenJuice is funny because the product language is playful: "token weight loss."

But the underlying systems idea is serious:

```text
agent productivity depends on transcript hygiene.
```

Terminal-heavy agents need:

- less noise
- preserved failure clues
- deterministic behavior
- explicit raw bypass
- recoverable artifacts
- inspectable rules
- measurable savings

This is a small tool, but it touches a real production problem.

---

**Extensions**

1. **Add a reducer for Jetson audio logs** — Preserve `arecord`, `aplay`, APE card, I2S2, and ALSA control lines.
2. **Add a reducer for CUDA profiler output** — Preserve kernel names, occupancy, memory throughput, and warnings.
3. **Add fixture tests** — Create before/after expected outputs for your project reducer.
4. **Add OpenClaw workflow notes** — Define when your agents should use raw output versus compacted output.
5. **Compare with LLM summarization** — Run the same logs through an LLM summary and compare determinism, cost, and missed details.

---

**References**

- TokenJuice repository: [https://github.com/vincentkoc/tokenjuice](https://github.com/vincentkoc/tokenjuice)
- TokenJuice spec: [https://github.com/vincentkoc/tokenjuice/blob/main/docs/spec.md](https://github.com/vincentkoc/tokenjuice/blob/main/docs/spec.md)
- TokenJuice rules: [https://github.com/vincentkoc/tokenjuice/blob/main/docs/rules.md](https://github.com/vincentkoc/tokenjuice/blob/main/docs/rules.md)
- TokenJuice integration playbook: [https://github.com/vincentkoc/tokenjuice/blob/main/docs/integration-playbook.md](https://github.com/vincentkoc/tokenjuice/blob/main/docs/integration-playbook.md)

---

*Next: [Lab 05 — OpenMeow App SDK Dogfood on macOS](Lab-05-OpenMeow-App-SDK-Dogfood.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lab-04-TokenJuice-Output-Compaction.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lab-04-TokenJuice-Output-Compaction.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
