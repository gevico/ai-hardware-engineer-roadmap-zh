---
title: Lecture 41 - Pi (pi-mono)：最小编码 agent 的细读
description: Lecture 41 - Pi (pi-mono)：最小编码 agent 的细读
---

# Lecture 41 - Pi (pi-mono)：最小编码 agent 的细读

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 40](Lecture-40.md) | **Next:** [Lecture 42](Lecture-42.md)

---

本讲是对 Pi 实际仓库的精读：[github.com/badlogic/pi-mono](https://github.com/badlogic/pi-mono)。Pi 是位于 OpenClaw 以及若干其他 agent 产品之下的**编码 agent 基座**。本讲不描述表面，而是拆开仓库，展示每个设计决策如何落到代码里：有哪些 package，哪些工具是内置的，扩展如何注册，会话如何存成可分支的 JSONL，hot reload 实际重载什么，以及 `No MCP` 作为一种配上具体变通方案的真实工程立场是什么样子。

主要来源：

- [`badlogic/pi-mono`](https://github.com/badlogic/pi-mono) —— 仓库本身，MIT 许可，当前为 v0.73.0，有 212 个 release 和 44.9k stars。
- [`packages/coding-agent` README](https://github.com/badlogic/pi-mono/blob/main/packages/coding-agent/README.md) —— CLI / TUI 界面。
- [`packages/agent` README](https://github.com/badlogic/pi-mono/tree/main/packages/agent) —— agent runtime 框架。
- Armin Ronacher，*Pi: The Minimal Agent Within OpenClaw*（2026 年 1 月 31 日）—— 背景与动机。

本讲引用具体细节之处（slash command、文件路径、命令行 flag、CLI 工具名），均来自撰写时项目自身的 README。

---

## 学习目标

学完本讲，你应能：

1. 说出 pi-mono 中的五个 package，并解释各自的作用。
2. 列出 Pi 的四个内置工具，以及通过 CLI flag 可用的三个可选工具。
3. 读懂 Pi 会话 JSONL 文件，并解释 `id` / `parentId` 如何生成一棵树。
4. 从语义上解释 `/tree`、`/fork` 与 `/clone` 之间的区别。
5. 写一个最小的 Pi 扩展，注册一个工具、一个 slash command 和一个事件处理器。
6. 解释 `/reload` 会重载什么、什么会自动 hot reload、什么完全不重载。
7. 向初学者说明 Pi 为什么没有 MCP，以及当确实需要 MCP 时该用什么替代。
8. 把每个 Pi 设计决策对应回 Lecture 02（harness 关注点）和 Lecture 26（event-sourced 会话）中的某一节。

---

## 1. Pi 究竟是什么

Pi（`pi-mono`）是一个 TypeScript monorepo，其主要产品是一个用一条命令即可安装的编码 agent CLI：

```bash
npm install -g @mariozechner/pi-coding-agent
export ANTHROPIC_API_KEY=sk-ant-...
pi
```

认证也可以为基于订阅的 provider 使用 `/login`；此时 CLI 以交互式 TUI 运行。该 package 是 monorepo 五个之一，全部 MIT 许可，全部位于 `@mariozechner/*` npm scope 之下。

仓库描述：*"Tools for building AI agents."* 这比把 Pi 称为"编码 agent"更准确 —— Pi 是一个 **agent runtime 套件**，其中编码 agent CLI 是最显眼的产品，但并非唯一。

---

## 2. Monorepo，逐个 package 看

```
pi-mono/
  packages/
    ai/               @mariozechner/pi-ai             multi-provider LLM API
    agent/            @mariozechner/pi-agent-core     agent runtime + tool calling
    coding-agent/     @mariozechner/pi-coding-agent   the `pi` CLI / TUI
    tui/              @mariozechner/pi-tui            differential-rendered TUI library
    web-ui/           @mariozechner/pi-web-ui         web components for chat UI
  .pi/                                                 self-config for development
  .github/                                             CI workflows
  scripts/                                             build / release scripts
```

自上而下读，这是一个**栈**：`ai` 是模型抽象，`agent` 是调用模型并分发工具的 runtime，`coding-agent` 是把 `agent` 接到 TUI 上的 CLI，`tui` 是渲染库，`web-ui` 是面向浏览器的等价界面。每个 package 都**独立发布**；使用方可以按需挑选所需的层。

一个推论：**非编码 agent 的产品（Slack bot、Telegram bot、OpenClaw 自身）直接消费 `@mariozechner/pi-agent-core` 和 `@mariozechner/pi-ai`**，并提供自己的前端。编码 agent CLI 只是 runtime 的一个消费方，不是唯一的。

---


<details>
<summary>English original</summary>

**Lecture 41 - Pi (pi-mono): A Detail Reading of a Minimal Coding Agent**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 40](Lecture-40.md) | **Next:** [Lecture 42](Lecture-42.md)

---

This lecture is a close reading of the actual Pi repository: [github.com/badlogic/pi-mono](https://github.com/badlogic/pi-mono). Pi is the **coding-agent substrate** that sits beneath OpenClaw and several other agent products. Rather than describe the surface, this lecture pulls apart the repo and shows you how each design decision is wired in code: which packages exist, which tools are built in, how extensions register, how sessions are stored as branchable JSONL, what hot reload actually reloads, and what `No MCP` looks like as a real engineering stance with a concrete workaround.

Primary sources:

- [`badlogic/pi-mono`](https://github.com/badlogic/pi-mono) — the repository itself, MIT-licensed, currently at v0.73.0 with 212 releases and 44.9k stars.
- [`packages/coding-agent` README](https://github.com/badlogic/pi-mono/blob/main/packages/coding-agent/README.md) — the CLI / TUI surface.
- [`packages/agent` README](https://github.com/badlogic/pi-mono/tree/main/packages/agent) — the agent runtime framework.
- Armin Ronacher, *Pi: The Minimal Agent Within OpenClaw* (Jan 31, 2026) — context and motivation.

Where this lecture quotes specifics (slash commands, file paths, command-line flags, CLI tool names), those come from the project's own README at the time of writing.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Name the five packages in pi-mono and explain what each one does.
2. List Pi's four built-in tools and the three optional ones available via CLI flags.
3. Read a Pi session JSONL file and explain how `id` / `parentId` produce a tree.
4. Explain the difference between `/tree`, `/fork`, and `/clone` semantically.
5. Write a minimal Pi extension that registers one tool, one slash command, and one event handler.
6. Explain what `/reload` reloads, what hot-reloads automatically, and what does not reload at all.
7. Tell a beginner why Pi has no MCP and what to do instead when MCP is required.
8. Map every Pi design decision back to a section of Lecture 02 (harness concerns) and Lecture 26 (event-sourced session).

---

**1. What Pi actually is**

Pi (`pi-mono`) is a TypeScript monorepo that ships, as its primary product, a coding-agent CLI installed with one command:

```bash
npm install -g @mariozechner/pi-coding-agent
export ANTHROPIC_API_KEY=sk-ant-...
pi
```

Authentication can also use `/login` for subscription-based providers; the CLI then runs as an interactive TUI. The package is one of five in the monorepo, all MIT-licensed, all under the `@mariozechner/*` npm scope.

The repo description: *"Tools for building AI agents."* That is more accurate than calling Pi "a coding agent" — Pi is an **agent runtime kit**, of which the coding-agent CLI is the most visible product but not the only one.

---

**2. The monorepo, package by package**

```
pi-mono/
  packages/
    ai/               @mariozechner/pi-ai             multi-provider LLM API
    agent/            @mariozechner/pi-agent-core     agent runtime + tool calling
    coding-agent/     @mariozechner/pi-coding-agent   the `pi` CLI / TUI
    tui/              @mariozechner/pi-tui            differential-rendered TUI library
    web-ui/           @mariozechner/pi-web-ui         web components for chat UI
  .pi/                                                 self-config for development
  .github/                                             CI workflows
  scripts/                                             build / release scripts
```

Read top-down, this is a **stack**: `ai` is the model abstraction, `agent` is the runtime that calls models and dispatches tools, `coding-agent` is the CLI that wires `agent` to a TUI, `tui` is the rendering library, `web-ui` is the equivalent surface for a browser. Each package is **independently published**; consumers can pick the layer they need.

A consequence: **a non-coding-agent product (a Slack bot, a Telegram bot, OpenClaw itself) consumes `@mariozechner/pi-agent-core` and `@mariozechner/pi-ai` directly** and supplies its own front-end. The coding-agent CLI is one consumer of the runtime, not the only one.

---

</details>

## 3. 内置工具面

Pi 的**默认**工具集是四个：

| 工具 | 用途 |
|---|---|
| `read` | 读取文件内容 |
| `write` | 创建或覆盖文件 |
| `edit` | 对已有文件做结构化编辑 |
| `bash` | 执行 shell 命令 |

另外三个可通过 CLI flag 使用，但默认不启用：

| 工具 | 用途 |
|---|---|
| `grep` | 在整个工作区内做文本搜索 |
| `find` | 文件名搜索 |
| `ls` | 列出目录 |

README 的表述很精确：*“默认情况下，pi 给模型四个工具：`read`、`write`、`edit` 和 `bash`。”* 这三个可选工具存在的意义，是在模型否则要耗 token 去 spawn `bash -c "grep ..."` 的场景下提供便利；它们只是便利，而非能力扩展。

把默认值保持在四个的结构性论据是：**大多数文件系统与 shell 能力都能由 `bash` 组合出来**。一个能写脚本并运行脚本的模型，就能做 `grep`、`find`、`ls`、`git`、`curl`、`npm` 以及任何其他 CLI，而不必让每一个都成为单独注册、schema 常驻 prompt 的工具。这与 Lecture 02 §6.5 是同一条原则：工具太多会干扰选择并浪费 token。

---

## 4. slash 命令面

Pi 自带约二十个内置 slash 命令。它们归入几类明确的类别。

**认证与身份**

```
/login          OAuth flow for subscription-based providers
/logout         Drop credentials
/model          Switch the active model
/scoped-models  Mark which models cycle under Ctrl+P
/settings       Thinking level, theme, message delivery, transport
```

**会话控制**

```
/new            Start a new session
/resume         Pick from previous sessions
/name <name>    Set the current session's display name
/session        Show session info
/quit           Quit
```

**树导航（有意思的部分）**

```
/tree           Jump to any point in the current session's tree
/fork           Create a NEW session file from a previous user message
/clone          Duplicate the current active branch into a new session file
/compact        Manually compact context (with optional prompt)
/reload         Reload keybindings, extensions, skills, prompts, context files
```

**输出与分享**

```
/copy           Copy last assistant message to clipboard
/export [file]  Export session to HTML
/share          Upload as a private GitHub gist
/changelog      Show version history
/hotkeys        Show all keyboard shortcuts
```

**扩展面**

```
/skill:<name>   Invoke a registered skill
/<templatename> Expand a prompt template
```

最后一类很重要：**任何非内置的东西，都能通过同一套 `/` 语法触达**。扩展注册自己的命令和模板，它们会直接出现在这里，无需额外手续。

---

## 5. 系统提示词组装

Pi 用**分层文件**组装其系统提示词，采用覆盖与追加语义。

```
priority order (highest to lowest):

  .pi/SYSTEM.md                 project-level full replacement
  ~/.pi/agent/SYSTEM.md         global-level full replacement
  default system prompt         shipped in the binary

  + .pi/APPEND_SYSTEM.md        project-level appended after replacement target
  + ~/.pi/agent/APPEND_SYSTEM.md  global appended after replacement target

  + AGENTS.md / CLAUDE.md       walked from cwd upward to root, all concatenated
  + skill files                 all matching files concatenated
```

这是 Lecture 37（OpenClaw System Prompt Architecture）中的提示词组装模式，且更加简化：一份很小的固定默认值，可替换，带有用于项目特定指引的追加 hook，再加上向上查找的上下文文件（`AGENTS.md`、`CLAUDE.md`），这正是近来每个 agent 都采用的定式。

`CLAUDE.md` 这个文件名与 `AGENTS.md` 一并被承认，是一个刻意的**兼容性动作**：已经为 Claude Code 配置好的工作区，无需重命名文件即可干净地落入 Pi。

---


<details>
<summary>English original</summary>

**3. The built-in tool surface**

Pi's **default** tool set is four:

| Tool | Purpose |
|---|---|
| `read` | Read a file's contents |
| `write` | Create or overwrite a file |
| `edit` | Structural edit on an existing file |
| `bash` | Execute a shell command |

Three more are available through CLI flags but are not enabled by default:

| Tool | Purpose |
|---|---|
| `grep` | Text search across the workspace |
| `find` | File-name search |
| `ls` | Directory listing |

The README's framing is exact: *"by default, pi gives the model four tools: `read`, `write`, `edit`, and `bash`."* The optional three exist for cases where the model would otherwise burn tokens spawning `bash -c "grep ..."`; they are convenience, not capability expansion.

The structural argument for keeping the default at four: **most filesystem and shell capabilities compose from `bash`**. A model that can write a script and run it can do `grep`, `find`, `ls`, `git`, `curl`, `npm`, and any other CLI without each one having to be a separately registered tool whose schema lives in the prompt. This is the same principle as Lecture 02 §6.5: too many tools confuse selection and waste tokens.

---

**4. The slash command surface**

Pi ships approximately twenty built-in slash commands. They group into clear categories.

**Authentication and identity**

```
/login          OAuth flow for subscription-based providers
/logout         Drop credentials
/model          Switch the active model
/scoped-models  Mark which models cycle under Ctrl+P
/settings       Thinking level, theme, message delivery, transport
```

**Session control**

```
/new            Start a new session
/resume         Pick from previous sessions
/name <name>    Set the current session's display name
/session        Show session info
/quit           Quit
```

**Tree navigation (the interesting part)**

```
/tree           Jump to any point in the current session's tree
/fork           Create a NEW session file from a previous user message
/clone          Duplicate the current active branch into a new session file
/compact        Manually compact context (with optional prompt)
/reload         Reload keybindings, extensions, skills, prompts, context files
```

**Output and sharing**

```
/copy           Copy last assistant message to clipboard
/export [file]  Export session to HTML
/share          Upload as a private GitHub gist
/changelog      Show version history
/hotkeys        Show all keyboard shortcuts
```

**Extension surface**

```
/skill:<name>   Invoke a registered skill
/<templatename> Expand a prompt template
```

That last category is important: **anything not built in is reachable through the same `/` syntax**. Extensions register their own commands and templates and they appear here without ceremony.

---

**5. System prompt assembly**

Pi assembles its system prompt from **layered files**, with override and append semantics.

```
priority order (highest to lowest):

  .pi/SYSTEM.md                 project-level full replacement
  ~/.pi/agent/SYSTEM.md         global-level full replacement
  default system prompt         shipped in the binary

  + .pi/APPEND_SYSTEM.md        project-level appended after replacement target
  + ~/.pi/agent/APPEND_SYSTEM.md  global appended after replacement target

  + AGENTS.md / CLAUDE.md       walked from cwd upward to root, all concatenated
  + skill files                 all matching files concatenated
```

This is the prompt-assembly pattern from Lecture 37 (OpenClaw System Prompt Architecture) made even simpler: a small fixed default, replaceable, with append hooks for project-specific guidance, plus walk-up context files (`AGENTS.md`, `CLAUDE.md`) the way every recent agent has settled on.

The `CLAUDE.md` filename being honored alongside `AGENTS.md` is a deliberate **compatibility move**: a workspace already configured for Claude Code drops cleanly into Pi without renaming files.

---

</details>

## 6. 会话模型

会话是存放在 `~/.pi/agent/sessions/` 下的 **JSONL 文件**，按工作目录组织。磁盘上的形态就是最简单的事件日志：

```jsonl
{"id":"a","parentId":null,"role":"user","content":"refactor this fn"}
{"id":"b","parentId":"a","role":"assistant","content":"…"}
{"id":"c","parentId":"b","role":"toolResult","name":"read","data":"…"}
{"id":"d","parentId":"c","role":"assistant","content":"…"}
{"id":"e","parentId":"a","role":"user","content":"actually try a different approach"}
{"id":"f","parentId":"e","role":"assistant","content":"…"}
```

每一行带一个 `id` 和一个 `parentId`。大多数行延伸当前分支（其 `parentId` 是上一行的 `id`）。当用户开启一条支线时，会写入一行，其 `parentId` **不是**上一行——而是某个更早的祖先。结果就是一棵树，装在一个文件里：

```
                    a (user: refactor this fn)
                   / \
                  b   e (user: actually try a different approach)
                  |   |
                  c   f
                  |
                  d
```

分支 `[a → b → c → d]` 和 `[a → e → f]` 都存在于同一个 JSONL 文件中。活跃分支由 runtime 认为哪个叶子是当前叶子来决定。

该格式以最低成本实现了 **Lecture 26 的「session is source of truth」原则**：一个仅追加文件、一个 `parentId` 字段、不需要单独的数据库。这也是 replay 能如此简单的原因：重新读取文件，沿所选分支折叠事件，由此导出上下文窗口。

---

## 7. `/tree`、`/fork` 与 `/clone` —— 三件不同的事

命名很重要。摘自 README：

| 命令 | 效果 | 影响的文件 |
|---|---|---|
| `/tree` | 跳转到当前会话树中的任意点，就地切换分支 | 一个文件（当前会话）；仅改变活跃叶子 |
| `/fork` | 从先前的用户消息创建**新的会话文件**；复制到该点为止的活跃路径；所选 prompt 放入编辑器以便修改 | 两个文件（原文件不动；新建会话） |
| `/clone` | 将当前活跃分支复制到当前位置的**新的会话文件**中；保留完整的活跃路径历史；以空编辑器打开 | 两个文件（原文件不动；新建会话） |

心智模型：

- `/tree` 是单个会话内的*导航*。
- `/fork` 是*从过去某个 prompt 出发的 what-if*，该 prompt 会被加载以供编辑——你在说「我想换个问法」。
- `/clone` 是*把当前状态快照到一个新会话*——你在说「我想把这段对话带到别处，而不污染原会话」。

这三个原语覆盖了现实中支线设计的空间：

- 对旧 prompt 尝试不同思路 → 从该 prompt 执行 `/fork`。
- 去探究某个正交的问题又不丢失主线 → `/clone`，在副本里干活，再回来。
- 在已存在的分支之间移动 → `/tree`。

一个**纯线性的会话日志**没有额外机制就做不到其中任何一件。

---


<details>
<summary>English original</summary>

**6. The session model**

Sessions are **JSONL files** stored under `~/.pi/agent/sessions/`, organized by working directory. The on-disk shape is the simplest possible event log:

```jsonl
{"id":"a","parentId":null,"role":"user","content":"refactor this fn"}
{"id":"b","parentId":"a","role":"assistant","content":"…"}
{"id":"c","parentId":"b","role":"toolResult","name":"read","data":"…"}
{"id":"d","parentId":"c","role":"assistant","content":"…"}
{"id":"e","parentId":"a","role":"user","content":"actually try a different approach"}
{"id":"f","parentId":"e","role":"assistant","content":"…"}
```

Each line carries an `id` and a `parentId`. Most lines extend the active branch (their `parentId` is the previous line's `id`). When the user takes a side-quest, a new line is written whose `parentId` is **not** the previous line — it is some earlier ancestor. The result is a tree, in one file:

```
                    a (user: refactor this fn)
                   / \
                  b   e (user: actually try a different approach)
                  |   |
                  c   f
                  |
                  d
```

Both branches `[a → b → c → d]` and `[a → e → f]` live in the same JSONL file. The active branch is determined by which leaf the runtime considers current.

The format is **Lecture 26's "session is source of truth" principle** at minimum cost: one append-only file, one `parentId` field, no separate database. This is also why replay works trivially: re-read the file, fold events along whichever branch you select, derive the context window from there.

---

**7. `/tree`, `/fork`, and `/clone` — three different things**

The naming matters. From the README:

| Command | Effect | Files affected |
|---|---|---|
| `/tree` | Jump to any point in the current session's tree, switch branches in place | One file (the current session); just changes the active leaf |
| `/fork` | Create a **new session file** from a previous user message; copies the active path up to that point; selected prompt placed in the editor for modification | Two files (original untouched; new session created) |
| `/clone` | Duplicate the current active branch into a **new session file** at the current position; full active-path history kept; opens with empty editor | Two files (original untouched; new session created) |

The mental model:

- `/tree` is *navigation* within one session.
- `/fork` is *what-if from a past prompt*, with that prompt loaded for editing — you are saying "I want to ask this differently."
- `/clone` is *snapshot at the current state into a new session* — you are saying "I want to take this conversation somewhere else without polluting the original."

These three primitives cover the realistic side-quest design space:

- Try a different approach to an old prompt → `/fork` from that prompt.
- Go investigate something orthogonal without losing the main thread → `/clone`, work in the clone, come back.
- Move between branches that already exist → `/tree`.

A **linear-only session log** can do none of these without additional machinery.

---

</details>

## 8. 扩展 API

扩展位于两个众所周知的位置：

```
~/.pi/agent/extensions/    global, available everywhere
.pi/extensions/            project-local
```

第三条路径是 "pi packages"（可通过标准 Node 解析链发现的 npm 包）。扩展可通过 `--no-extensions` 按次运行禁用，或通过 `-e` 显式加载。

每个扩展都是一个 TypeScript 模块，带默认导出：一个接收 `ExtensionAPI` 对象并注册其所需一切内容的函数：

```typescript
export default function (pi: ExtensionAPI) {
  pi.registerTool({
    name: "deploy",
    label: "Deploy",
    description: "Deploy the current branch to staging",
    parameters: Type.Object({
      target: Type.String({ description: "staging | production" })
    }),
    execute: async (toolCallId, params, signal, onUpdate) => {
      // ...
    }
  });

  pi.registerCommand("stats", {
    description: "Show cost and token usage for this session",
    handler: async (ctx) => { /* ... */ }
  });

  pi.on("tool_call", async (event, ctx) => {
    // observe every tool call, with full context
  });
}
```

`ExtensionAPI` 暴露的内容（摘自 README）：

- **`registerTool()`** — 添加一个 LLM 可见的工具（Lecture 02 §2.1 分发面）
- **`registerCommand()`** — 添加一个斜杠命令（脱离上下文，用户可见）
- **`on(event, handler)`** — 订阅 runtime 事件，例如 `tool_call`
- 自定义 UI 组件、状态行、页眉、页脚、就地编辑器
- 异步扩展工厂（使扩展可以执行启动工作）

关于该 API 的两点结构性说明：

1. **扩展面与 Lecture 02 的两个扩展面相同**（上下文内的 LLM 工具 vs 上下文外的 TUI）。`registerTool()` 属于前者；`registerCommand()` 与 UI 钩子属于后者。Lecture 02 §5.3 的决策规则直接适用。

2. **事件处理器是一等公民。** 这正是让 "agent 扩展自身" 变得可行的原因：扩展可以观察工具调用、对它们作出反应、修改它们、记录它们、门控它们。审计 / 安全 / 遥测会使用的那套钩子面，正是扩展所看到的那套。

---

## 9. Agent runtime —— `pi-agent-core`

如果说 coding-agent CLI 是可见的表层，那么 `pi-agent-core` 就是**基底**。当 OpenClaw、某个 Telegram bot 或你自己的前端想要不借助 CLI 获得 Pi 风格的 agent 行为时，消费的就是它。

对外暴露的形态：

```typescript
agent.state.systemPrompt  = "..."
agent.state.model         = getModel(...)
agent.state.tools         = [tool1, tool2]
agent.state.messages      = [...]    // top-level array; copied on assign

await agent.waitForIdle()
agent.abort()
agent.reset()

// read-only:
agent.isStreaming
agent.streamingMessage
agent.pendingToolCalls
agent.errorMessage
```

工具定义使用基于 TypeBox 的接口：

```typescript
const readFileTool: AgentTool = {
  name:           "read_file",
  label:          "Read File",
  description:    "Read a file's contents",
  parameters:     Type.Object({ path: Type.String() }),
  executionMode:  "sequential",   // or default parallel
  execute: async (toolCallId, params, signal, onUpdate) => {
    // tool body
    // returns a result, or throws on failure
    // may include `terminate: true` to skip the next LLM call
  }
};
```

两个关键的具体点：

- **`executionMode: "sequential"`** — 对不可并发运行的工具，选择退出并行工具执行。默认情况下 runtime 可以并行执行互不冲突的工具调用。
- **`terminate: true`** — 工具结果可以标记 agent 跳过自动的后续 LLM 调用。适用于结果本身就是答案的工具（例如一个成功执行的 `commit` 工具；无需再多说什么）。

自定义消息类型通过在 `CustomAgentMessages` 接口上做**声明合并**添加，随后由 `convertToLlm()` 从面向 LLM 的子集中过滤掉。这就是 §6 中 "会话日志里的自定义消息" 模式在类型层面的落地。

---

## 10. 热重载 —— 究竟重载了什么

Pi 的热重载方案比 "编辑并保存" 要具体得多。

**`/reload`** 是一个手动命令，会重新读取：

- 键位绑定
- 扩展
- 技能
- prompt
- 上下文文件（`AGENTS.md` / `CLAUDE.md`）

**主题会自动热重载** —— 修改当前激活的主题文件，改动立即生效，无需 `/reload`。

**runtime 中不会重载的内容：**

- 底层的 agent runtime / TUI 本身（需要重启进程）
- 已在运行的工具执行（尤其是串行的那些）
- 模型 API 状态（缓存前缀等）

结构性结论：agent runtime 中的热重载*并非* "每一层都可热重载"。被热重载的恰恰是*配置层与扩展层*；runtime 核心保持稳定。这正是一条划得恰到好处的界线 —— 它带来 "agent 扩展自身" 的闭环，又不会引入工具在执行中途被半重载这一类 bug。

---


<details>
<summary>English original</summary>

**8. The Extension API**

Extensions live in two well-known locations:

```
~/.pi/agent/extensions/    global, available everywhere
.pi/extensions/            project-local
```

A third path is "pi packages" (npm packages discoverable through the normal Node resolution chain). Extensions can be disabled per-run with `--no-extensions` or explicitly loaded with `-e`.

Each extension is a TypeScript module with a default export: a function that takes an `ExtensionAPI` object and registers everything it wants:

```typescript
export default function (pi: ExtensionAPI) {
  pi.registerTool({
    name: "deploy",
    label: "Deploy",
    description: "Deploy the current branch to staging",
    parameters: Type.Object({
      target: Type.String({ description: "staging | production" })
    }),
    execute: async (toolCallId, params, signal, onUpdate) => {
      // ...
    }
  });

  pi.registerCommand("stats", {
    description: "Show cost and token usage for this session",
    handler: async (ctx) => { /* ... */ }
  });

  pi.on("tool_call", async (event, ctx) => {
    // observe every tool call, with full context
  });
}
```

What `ExtensionAPI` exposes (from the README):

- **`registerTool()`** — add an LLM-visible tool (Lecture 02 §2.1 dispatch surface)
- **`registerCommand()`** — add a slash command (out-of-context, user-visible)
- **`on(event, handler)`** — subscribe to runtime events such as `tool_call`
- Custom UI components, status lines, headers, footers, in-place editors
- Async extension factories (so extensions can do startup work)

Two structural notes on this API:

1. **The extension surface is the same as Lecture 02's two extension surfaces** (in-context LLM tools vs out-of-context TUI). `registerTool()` is the former; `registerCommand()` and the UI hooks are the latter. The decision rule from Lecture 02 §5.3 applies directly.

2. **Event handlers are first-class.** This is what makes "the agent extends itself" practical: an extension can observe tool calls, react to them, modify them, log them, gate them. The same hook surface that audit / security / telemetry would use is the one extensions see.

---

**9. The Agent runtime — `pi-agent-core`**

If the coding-agent CLI is the visible surface, `pi-agent-core` is the **substrate**. It is what OpenClaw, a Telegram bot, or your own front-end consumes when they want Pi-style agent behavior without the CLI.

The exposed shape:

```typescript
agent.state.systemPrompt  = "..."
agent.state.model         = getModel(...)
agent.state.tools         = [tool1, tool2]
agent.state.messages      = [...]    // top-level array; copied on assign

await agent.waitForIdle()
agent.abort()
agent.reset()

// read-only:
agent.isStreaming
agent.streamingMessage
agent.pendingToolCalls
agent.errorMessage
```

Tool definitions use a TypeBox-based interface:

```typescript
const readFileTool: AgentTool = {
  name:           "read_file",
  label:          "Read File",
  description:    "Read a file's contents",
  parameters:     Type.Object({ path: Type.String() }),
  executionMode:  "sequential",   // or default parallel
  execute: async (toolCallId, params, signal, onUpdate) => {
    // tool body
    // returns a result, or throws on failure
    // may include `terminate: true` to skip the next LLM call
  }
};
```

Two specifics that matter:

- **`executionMode: "sequential"`** — opt-out of parallel tool execution for tools that must not run concurrently. By default the runtime can execute non-conflicting tool calls in parallel.
- **`terminate: true`** — a tool result can flag the agent to skip the automatic follow-up LLM call. Useful for tools whose result is the answer (e.g., a `commit` tool that succeeded; nothing more to say).

Custom message types are added by **declaration merging** on a `CustomAgentMessages` interface, then filtered out of the LLM-bound subset by `convertToLlm()`. This is the "custom messages in the session log" pattern from §6 made operational at the type level.

---

**10. Hot reload — what actually reloads**

Pi's hot-reload story is more specific than "edit and save."

**`/reload`** is a manual command that re-reads:

- keybindings
- extensions
- skills
- prompts
- context files (`AGENTS.md` / `CLAUDE.md`)

**Themes hot-reload automatically** — modify the active theme file and the change applies immediately, no `/reload` needed.

**What does NOT reload at runtime:**

- the underlying agent runtime / TUI itself (requires process restart)
- already-running tool executions (sequential ones especially)
- model API state (cache prefixes etc.)

The structural lesson: hot reload in an agent runtime is *not* "every layer is hot-reloadable." It is specifically the *configuration and extension layers* that are hot-reloaded; the runtime core stays stable. This is exactly the right line to draw — it gives you the "agent extends itself" loop without inviting the bug class where a tool is half-reloaded mid-execution.

---

</details>

## 11. 配置路径

| 路径 | 用途 | 范围 |
|---|---|---|
| `~/.pi/agent/settings.json` | 设置（主题、thinking level、transport 等） | 全局 |
| `.pi/settings.json` | 设置覆盖 | 项目 |
| `~/.pi/agent/SYSTEM.md` | 系统提示词整体替换 | 全局 |
| `.pi/SYSTEM.md` | 系统提示词整体替换 | 项目 |
| `~/.pi/agent/APPEND_SYSTEM.md` | 系统提示词追加 | 全局 |
| `.pi/APPEND_SYSTEM.md` | 系统提示词追加 | 项目 |
| `~/.pi/agent/extensions/` | 扩展 | 全局 |
| `.pi/extensions/` | 扩展 | 项目 |
| `~/.pi/agent/sessions/` | 会话 JSONL 文件（按 cwd 组织） | 全局内按 cwd 划分 |
| `~/.pi/agent/skills/` | 技能 | 全局 |
| `.pi/skills/` | 技能 | 项目 |
| `AGENTS.md`、`CLAUDE.md` | 上下文文件 | 从 cwd 逐级向上查找 |

整棵配置树都可通过环境变量 `PI_CODING_AGENT_DIR` 覆盖，这对测试以及运行多个相互隔离的 Pi 实例很有用。

约定是那条被踩烂的老路：项目里一个隐藏的点目录（`.pi`），全局配置对应一个 `~/.pi/agent/`，其余所有情况用一个环境变量兜底。没有意外。

---

## 12. 「不要 MCP」的立场，落到实处

README 说得很直接：*「No MCP. Build CLI tools with READMEs (see Skills), or build an extension that adds MCP support.」*

结构性论证已在第 02 讲 §2.1 和第 25 讲 §14.6 预告过。落到 Pi 的语境里：

- Pi 期望**在会话中途改动工具面**（扩展注册工具；技能按需加载；`/reload` 会重新读取一切）。
- MCP 在多数提供方的部署方式下，期望工具目录**在整个会话期间保持稳定**，以便它能待在缓存的提示词前缀里。
- 这两者直接冲突。Pi 的解法是在协议层面不采用 MCP。

替代做法：

1. **构建 CLI 工具，并给它们配上 README。** Pi 的 `bash` 工具按名字找到它们；README 才是教模型怎么用它们的东西。技能让这件事变得地道——一个技能就是一个目录，里面有几个 markdown 文件和支持脚本。
2. **构建一个 MCP 桥接扩展**，前提是你确实需要 MCP。该扩展可以拉起 `mcporter`（或类似进程），转换调用，把这些方法动态注册为 Pi 工具，并在退出时清理。

这个教训比 Pi 更宽泛：**harness（agent 运行时框架）的工具面可变性与它的协议选择，是相互耦合的架构决策**。你不能同时选择「工具以 MCP 定义、在会话开始时挂载」和「agent 通过写工具来扩展自己」，而不在某个地方为这冲突付出代价。

---

## 13. 键盘快捷键作为 UX 原语

Pi 提供的是键盘优先的 TUI。这些快捷键不是装饰；它们就是真正的交互模型。

以下摘自 README：

| 按键 | 动作 |
|---|---|
| `Ctrl+C` | 清空编辑器（单次按下） |
| `Ctrl+C` × 2 | 退出 |
| `Escape` | 取消 / 中止 |
| `Escape` × 2 | 打开 `/tree` |
| `Ctrl+L` | 打开模型选择器 |
| `Ctrl+P` / `Shift+Ctrl+P` | 在作用域模型间向前 / 向后循环 |
| `Shift+Tab` | 循环切换 thinking level |
| `Ctrl+O` | 折叠 / 展开工具输出 |
| `Ctrl+T` | 折叠 / 展开 thinking 块 |
| `Shift+Enter` | 多行编辑器（Windows Terminal 上为 `Ctrl+Enter`） |
| `Tab` | 路径补全 |
| `Ctrl+V` | 粘贴图片（Windows 上为 `Alt+V`） |
| `Enter` | 排队 steering 消息（流中途） |
| `Alt+Enter` | 排队后续消息 |
| `Ctrl+G` | 打开外部编辑器 |

其中三项是 agent 特有的创新，值得单独点出：

- **`Enter` 在流运行期间排队一条 steering 消息。** 不必中止；在它工作时告诉它一些事，它会在下一个安全点接住这条消息。
- **`Alt+Enter` 排队一条后续消息。** 思路相同，但作用于当前轮次完成之后，而不是流中途。
- **`Escape` × 2 跳转到 `/tree`。** 分支导航在任何位置都只差一个按键。

这些都反映了同一个设计选择：**人始终在回路中**，而不只是在轮次边界上。TUI 提供了在流中途行动的 affordances，而 agent runtime 的构建方式则能接收这些信号而不崩溃。

---


<details>
<summary>English original</summary>

**11. Configuration paths**

| Path | Purpose | Scope |
|---|---|---|
| `~/.pi/agent/settings.json` | Settings (theme, thinking level, transport, etc.) | Global |
| `.pi/settings.json` | Settings overrides | Project |
| `~/.pi/agent/SYSTEM.md` | System prompt full replacement | Global |
| `.pi/SYSTEM.md` | System prompt full replacement | Project |
| `~/.pi/agent/APPEND_SYSTEM.md` | System prompt append | Global |
| `.pi/APPEND_SYSTEM.md` | System prompt append | Project |
| `~/.pi/agent/extensions/` | Extensions | Global |
| `.pi/extensions/` | Extensions | Project |
| `~/.pi/agent/sessions/` | Session JSONL files (organized by cwd) | Per-cwd within global |
| `~/.pi/agent/skills/` | Skills | Global |
| `.pi/skills/` | Skills | Project |
| `AGENTS.md`, `CLAUDE.md` | Context files | Walked up from cwd |

The whole config tree is overridable via the `PI_CODING_AGENT_DIR` environment variable, which is useful for testing and for running multiple isolated Pi instances.

The convention is the well-trodden one: a hidden dotted directory (`.pi`) in the project, a corresponding `~/.pi/agent/` for globals, and a single env var for everything-else cases. No surprises.

---

**12. The "No MCP" stance, made concrete**

The README is direct: *"No MCP. Build CLI tools with READMEs (see Skills), or build an extension that adds MCP support."*

The structural argument was previewed in Lecture 02 §2.1 and Lecture 25 §14.6. In Pi-specific terms:

- Pi expects to **mutate the tool surface mid-session** (extensions register tools; skills are loaded on demand; `/reload` re-reads everything).
- MCP, as deployed across most providers, expects the tool catalog to be **stable for the session** so it can sit in the cached prompt prefix.
- These two are in direct tension. Pi resolves it by not adopting MCP at the protocol level.

What you do instead:

1. **Build CLI tools and put a README on them.** Pi's `bash` tool reaches them by name; the README is what teaches the model how to use them. Skills make this idiomatic — a skill is a directory with a few markdown files and supporting scripts.
2. **Build an MCP-bridge extension** if you genuinely need MCP. The extension can spawn `mcporter` (or similar), translate calls, register the methods as Pi tools dynamically, and clean up on exit.

The lesson is broader than Pi: **a harness's tool-surface mutability and its protocol choice are coupled architectural decisions**. You cannot pick "tools defined as MCP, mounted at session start" and "agent extends itself by writing tools" without paying for the conflict somewhere.

---

**13. Keyboard shortcuts as a UX primitive**

Pi ships a keyboard-first TUI. The shortcuts are not decoration; they are the actual interaction model.

Selected from the README:

| Key | Action |
|---|---|
| `Ctrl+C` | Clear editor (single press) |
| `Ctrl+C` × 2 | Quit |
| `Escape` | Cancel / abort |
| `Escape` × 2 | Open `/tree` |
| `Ctrl+L` | Open model selector |
| `Ctrl+P` / `Shift+Ctrl+P` | Cycle scoped models forward / backward |
| `Shift+Tab` | Cycle thinking level |
| `Ctrl+O` | Collapse / expand tool output |
| `Ctrl+T` | Collapse / expand thinking blocks |
| `Shift+Enter` | Multi-line editor (`Ctrl+Enter` on Windows Terminal) |
| `Tab` | Path completion |
| `Ctrl+V` | Paste images (`Alt+V` on Windows) |
| `Enter` | Queue steering message (mid-stream) |
| `Alt+Enter` | Queue follow-up message |
| `Ctrl+G` | Open external editor |

Three of these are agent-specific innovations worth calling out:

- **`Enter` queues a steering message during a running stream.** You do not have to abort; you tell the agent something while it is working and it picks the message up at the next safe point.
- **`Alt+Enter` queues a follow-up.** Same idea but applies after the current turn completes rather than mid-stream.
- **`Escape` × 2 jumps to `/tree`.** Branch navigation is one keystroke away from anywhere.

These all reflect the same design choice: **the human is in the loop continuously**, not just at turn boundaries. The TUI gives you the affordances to act mid-stream, and the agent runtime is built to accept those signals without breaking.

---

</details>

## 14. pi-mono 生态

该 repo 提到了两个值得了解的姊妹项目：

- **[`badlogicgames/pi-share-hf`](https://github.com/badlogicgames/pi-share-hf)** — 把 Pi 会话发布到 Hugging Face。模式是“会话即产物”：一旦有了树形结构的事件日志，分享它就只是发布文件而已。
- **[`earendil-works/pi-chat`](https://github.com/earendil-works/pi-chat)** — 构建在 Pi 之上的 Slack/聊天自动化工作流。Pi 是 runtime；这是若干前端之一，用以说明 `pi-agent-core` 本就该被嵌入。

品牌域名是 `pi.dev`。

44.9k star 数与 v0.73.0（2026 年 5 月）的 212 个 release 表明，这是一个发布频繁、且已积累起可观用户基础的项目。对学习者而言：**版本节奏本身就是一种信号** —— 本讲中的架构选择并非纸上谈兵，它们每周都要在生产环境中经受用户的检验。

---

## 15. 把 Pi 的每个决策映射回第 24 讲与 24b 讲

Pi 是本课程通用 harness 理论最具体的公开实例。逐一走一遍映射：

| Pi 决策 | 第 02 讲 / 24b 讲 原则 |
|---|---|
| 四个内置工具（read、write、edit、bash） | §2.1 分发面；§6.5 工具过多反模式 |
| `~/.pi/agent/sessions/*.jsonl` 只追加 | 第 26 讲 §1 会话作为真相来源 |
| `id` + `parentId` 树 | 第 26 讲 §2 面向认知的事件溯源 |
| 通过 declaration merging 自定义消息类型 | 第 26 讲 §3 schema；§10 反模式被消除 |
| `/reload` 用于扩展，主题自动重载 | §2.6 可扩展性；§5 无状态解释器模式 |
| `.pi/SYSTEM.md` 覆盖 + `APPEND_SYSTEM.md` | §2.3 上下文构造；第 37 讲提示词组装 |
| `registerCommand`（TUI） vs `registerTool`（LLM） | §5.1 / §5.2 / §5.3 决策规则 |
| `executionMode: "sequential"` 选择退出 | §2.4 规划与恢复；工具调用顺序 |
| `terminate: true` 在工具结果中 | §2.4 轮次循环控制 |
| 向上查找 `AGENTS.md` / `CLAUDE.md` | §2.3 上下文构造 |
| `PI_CODING_AGENT_DIR` 环境变量 | §2.5 实例间的策略与权限隔离 |
| 不用 MCP，改用 `bash` 与 CLI 的变通方案 | §2.1 分发边界；§2.6 可扩展性取舍 |
| `/tree`、`/fork`、`/clone` | 第 26 讲 §2 能力（branching 是其中被解锁的那一个） |
| 流中途 `Enter` 以施加引导 | §2.4 规划与恢复；human-in-the-loop |

这张表回答了“我该不该把 Pi 的设计决策照搬到自己的 harness 里？”该，除了不用 MCP 那一条 —— 它取决于你是否也采用会话中途可变性。要么两条都采用，要么都不采用。

---

## 16. 与硬件方向的衔接

对走 Jetson / 边缘 AI 方向的学习者来说，Pi 在结构上有三处特别值得注意：

**最小核心，最小冷启动。** 四工具 harness 相比功能臃肿的替代方案，prompt 缓存面更小，冷启动更快。在统一内存受限的 Jetson AGX 上（Lecture VLA Deployment on Edge GPUs §5），系统提示词每多约 10 KB，都会在首轮增加 decode 延迟。Pi 的最小集足够小，不至于成为主导因素。

**与 provider 无关的 SDK 支持混合路由。** `pi-ai` 对模型 provider 的抽象足够干净，一个会话可以混用远程 Anthropic 调用与本地端侧模型（vLLM、llama.cpp、ONNX Runtime、来自 `vla-deploy-jetson` 的 VLA 栈）。会话日志按轮次记录 provider 元数据，因此 replay 仍然可用。对云边混合部署模式而言，这是正确的原语。

**树形结构会话作为多 agent 边缘原语。** 两个机器人从共享的父会话分叉，即可表达协同，而无需强加主从顺序。Jetson 上的 Pi 机群有天然的方式做到这一点，而不必在外面硬接一层编排。

阶段 4 / 方向 B / ML and AI / `vla-deploy-jetson` 中的 VLA 部署指南是最接近的姊妹篇：**相同的工程姿态（最小基底、可热重载的组合方式、边缘友好的设计），不同的目标工作负载**。

---


<details>
<summary>English original</summary>

**14. The pi-mono ecosystem**

The repo names two sister projects worth knowing:

- **[`badlogicgames/pi-share-hf`](https://github.com/badlogicgames/pi-share-hf)** — publish a Pi session to Hugging Face. The pattern is "session as artifact": once you have a tree-structured event log, sharing it is just publishing the file.
- **[`earendil-works/pi-chat`](https://github.com/earendil-works/pi-chat)** — Slack/chat automation workflows on top of Pi. Pi is the runtime; this is one of several front-ends that demonstrate `pi-agent-core` is meant to be embedded.

The branding domain is `pi.dev`.

The 44.9k-star count and 212 releases at v0.73.0 (May 2026) suggest a project that ships frequently and has reached a substantial user base. For a learner: **the version cadence is itself a signal** that the architectural choices in this lecture are not theoretical — they have to survive contact with users in production weekly.

---

**15. Mapping every Pi decision back to Lectures 24 and 24b**

Pi is the most concrete public instantiation of the general harness theory in this course. Walking the mapping:

| Pi decision | Lecture 02 / 24b principle |
|---|---|
| Four built-in tools (read, write, edit, bash) | §2.1 dispatch surface; §6.5 too-many-tools anti-pattern |
| `~/.pi/agent/sessions/*.jsonl` append-only | Lecture 26 §1 session as source of truth |
| `id` + `parentId` tree | Lecture 26 §2 event sourcing for cognition |
| Custom message types via declaration merging | Lecture 26 §3 schema; §10 anti-pattern eliminated |
| `/reload` for extensions, themes auto-reload | §2.6 extensibility; §5 stateless interpreter pattern |
| `.pi/SYSTEM.md` overrides + `APPEND_SYSTEM.md` | §2.3 context construction; Lecture 37 prompt assembly |
| `registerCommand` (TUI) vs `registerTool` (LLM) | §5.1 / §5.2 / §5.3 the decision rule |
| `executionMode: "sequential"` opt-out | §2.4 planning and recovery; tool-call ordering |
| `terminate: true` in tool result | §2.4 turn-loop control |
| Walk-up `AGENTS.md` / `CLAUDE.md` | §2.3 context construction |
| `PI_CODING_AGENT_DIR` env var | §2.5 policy and permission isolation between instances |
| No MCP, with `bash`-and-CLI workaround | §2.1 dispatch boundary; §2.6 extensibility tradeoff |
| `/tree`, `/fork`, `/clone` | Lecture 26 §2 capabilities (branching is the unlocked one) |
| Mid-stream `Enter` to steer | §2.4 planning and recovery; human-in-the-loop |

This table is the answer to "should I copy Pi's design decisions into my own harness?" Yes, except the No-MCP one — that is contingent on whether you also adopt mid-session mutability. Adopt both or neither.

---

**16. Hardware-track tie-in**

For learners on the Jetson / edge AI track, Pi is structurally interesting in three specific ways:

**Minimal core, minimal cold start.** A four-tool harness has a smaller prompt-cache surface and a faster cold start than feature-bloated alternatives. On a Jetson AGX with limited unified memory (Lecture VLA Deployment on Edge GPUs §5), every ~10 KB of system prompt costs decode latency on the first turn. Pi's minimum is small enough that it does not dominate.

**Provider-agnostic SDK enables hybrid routing.** `pi-ai` abstracts model providers cleanly enough that one session can mix a remote Anthropic call with a local on-device model (vLLM, llama.cpp, ONNX Runtime, the VLA stack from `vla-deploy-jetson`). The session log captures provider metadata per turn so replay still works. This is the right primitive for the hybrid cloud-vs-edge deployment pattern.

**Tree-structured sessions as a multi-agent edge primitive.** Two robots branching from a shared parent session expresses coordination without forcing a master-slave ordering. A Pi-on-Jetson fleet has a natural way to do this without bolting orchestration on top.

The VLA deploy guide in Phase 4 / Track B / ML and AI / `vla-deploy-jetson` is the closest sibling: **same engineering posture (minimal substrate, hot-reloadable composition, edge-aware design), different target workload**.

---

</details>

## 17. 动手构建

两个具体产物，难度递增。

**入门 — 写一个 Pi 扩展。** 挑一个你真正想要的能力：一个 `/diff` 命令，在 TUI 浮层中显示当前未提交的 diff；一个 `/cost` 命令，展示会话成本与 token 用量；一个 `/scratchpad` 命令，打开外部编辑器写一条简短笔记，作为自定义消息追加到会话中。注册一个斜杠命令、一个事件处理器，以及（可选）一个 LLM 工具。让它落到你自己工作树中的 `.pi/extensions/` 里。

**进阶 — 把 `pi-agent-core` 嵌入非 CLI 前端。** 构建一个小型 Discord bot、Slack bot 或 web 聊天应用，直接使用 `@mariozechner/pi-ai` 和 `@mariozechner/pi-agent-core`。实现默认的四个工具（或其中一个子集）。以相同的 `id` / `parentId` 结构把会话持久化为 JSONL。要点在于：向自己证明，这套底座不依赖 CLI 也能被消费。

**高级 — 用另一种语言写一个自己的最小 harness（agent 运行时框架）**，把 §15 映射表中的每一条原则都用上。同样的四个内置工具，同样的带 `parentId` 树的 JSONL 会话格式，同样的两个扩展面。当你的 harness 能承载一个由模型写出的扩展、热重载它，并在之后继续同一个会话，你就知道自己已经理解这套架构了。

---

## 关键要点

- pi-mono 是一个包含五个包的 TypeScript monorepo：`pi-ai`、`pi-agent-core`、`pi-coding-agent`、`pi-tui`、`pi-web-ui`。CLI 只是其中一个产品；runtime 的设计目标是可嵌入。
- 默认工具集恰好是四个：`read`、`write`、`edit`、`bash`。另外三个（`grep`、`find`、`ls`）可通过 CLI 开关切换。
- 会话是以 `id` 和 `parentId` 为键的 JSONL 文件 —— 一棵树装在一个文件里。`/tree` 负责导航，`/fork` 从过去的 prompt 分叉，`/clone` 把当前活动分支快照成一个新会话。
- 扩展位于 `~/.pi/agent/extensions/` 或 `.pi/extensions/`，通过一个 `ExtensionAPI` 注册，该接口同时暴露 LLM 工具注册与 TUI / 斜杠命令注册。事件处理器（`pi.on("tool_call", ...)`）让扩展成为一等观察者。
- 热重载是有针对性的，而非全局：`/reload` 会重新读取键位绑定、扩展、技能、prompt 和上下文文件；主题自动热重载；runtime 核心不会重载。
- 「不用 MCP」是一个由 Pi 对会话中途可变性的要求所驱动的结构性选择，而不是路线图上的缺口。使用带 README 的 CLI 工具，或写一个扩展来桥接 MCP，或用 `bash` 调用 `mcporter`。
- 自定义消息类型（通过声明合并进 `CustomAgentMessages`）是扩展把状态持久化进同一个只追加会话日志的方式。这就是把第 26 讲的原则付诸实施。
- 流中途引导（`Enter` 将一条消息入队；`Alt+Enter` 将后续消息入队；`Escape × 2` 打开 `/tree`）是一个 UX 原语，建立在这样一个假设之上：人持续在回路中，而不仅仅在轮次边界处。
- Pi 的每一项重大设计决策都能对应回第 02 讲（harness 关注点）与第 26 讲（事件溯源会话）的某一节。Pi 是这些原则在已发布代码中最干净的公开实例。
- 对硬件方向的学习者而言：最小内核、与提供商无关的 SDK、树状结构的会话，正是边缘 AI 部署所需要的底座属性。

---

## 参考资料

### 一手来源

- pi-mono 仓库 — [https://github.com/badlogic/pi-mono](https://github.com/badlogic/pi-mono)
- `packages/coding-agent` README — [https://github.com/badlogic/pi-mono/blob/main/packages/coding-agent/README.md](https://github.com/badlogic/pi-mono/blob/main/packages/coding-agent/README.md)
- `packages/agent` README — [https://github.com/badlogic/pi-mono/tree/main/packages/agent](https://github.com/badlogic/pi-mono/tree/main/packages/agent)
- npm: `@mariozechner/pi-coding-agent` — [https://www.npmjs.com/package/@mariozechner/pi-coding-agent](https://www.npmjs.com/package/@mariozechner/pi-coding-agent)
- npm: `@mariozechner/pi-agent-core` — [https://www.npmjs.com/package/@mariozechner/pi-agent-core](https://www.npmjs.com/package/@mariozechner/pi-agent-core)
- npm: `@mariozechner/pi-ai` — [https://www.npmjs.com/package/@mariozechner/pi-ai](https://www.npmjs.com/package/@mariozechner/pi-ai)
- npm: `@mariozechner/pi-tui` — [https://www.npmjs.com/package/@mariozechner/pi-tui](https://www.npmjs.com/package/@mariozechner/pi-tui)

### 姊妹项目与消费方项目

- `badlogicgames/pi-share-hf` — 将会话发布到 Hugging Face：[https://github.com/badlogicgames/pi-share-hf](https://github.com/badlogicgames/pi-share-hf)
- `earendil-works/pi-chat` — Pi 上的 Slack / 聊天工作流：[https://github.com/earendil-works/pi-chat](https://github.com/earendil-works/pi-chat)
- OpenClaw — 把 Pi 作为 runtime 消费的多通道 agent 平台：[https://github.com/openclaw/openclaw](https://github.com/openclaw/openclaw)
- pi.dev — 项目的主域名。


<details>
<summary>English original</summary>

**17. Build it**

Two concrete artifacts, in increasing difficulty.

**Beginner — write a Pi extension.** Pick a capability you actually want: a `/diff` command that shows the current uncommitted diff in a TUI overlay, a `/cost` command that surfaces session cost and tokens, a `/scratchpad` command that opens an external editor for a quick note appended to the session as a custom message. Register one slash command, one event handler, and (optionally) one LLM tool. Land it in `.pi/extensions/` in your own working tree.

**Intermediate — embed `pi-agent-core` in a non-CLI front-end.** Build a small Discord bot, Slack bot, or web chat that uses `@mariozechner/pi-ai` and `@mariozechner/pi-agent-core` directly. Implement the four-tool default (or a subset). Persist sessions to JSONL with the same `id` / `parentId` shape. The point: prove to yourself that the substrate is consumable independent of the CLI.

**Advanced — write your own minimal harness in a different language**, applying every principle from the §15 mapping table. Same four built-in tools, same JSONL session format with `parentId` tree, same two extension surfaces. You will know you have understood the architecture when your harness can host an extension written by a model, hot-reload it, and continue the same session afterward.

---

**Key takeaways**

- pi-mono is a TypeScript monorepo of five packages: `pi-ai`, `pi-agent-core`, `pi-coding-agent`, `pi-tui`, `pi-web-ui`. The CLI is one product; the runtime is meant to be embedded.
- The default tool set is exactly four: `read`, `write`, `edit`, `bash`. Three more (`grep`, `find`, `ls`) are CLI-toggleable.
- Sessions are JSONL files keyed by `id` and `parentId` — a tree in one file. `/tree` navigates, `/fork` branches from a past prompt, `/clone` snapshots the active branch into a new session.
- Extensions live in `~/.pi/agent/extensions/` or `.pi/extensions/` and register through an `ExtensionAPI` that exposes both LLM-tool registration and TUI / slash-command registration. Event handlers (`pi.on("tool_call", ...)`) make extensions first-class observers.
- Hot reload is targeted, not universal: `/reload` re-reads keybindings, extensions, skills, prompts, and context files; themes hot-reload automatically; the runtime core does not reload.
- "No MCP" is a structural choice driven by Pi's mid-session mutability requirement, not a roadmap gap. Use CLI tools with READMEs, or write an extension to bridge MCP, or use `bash` to invoke `mcporter`.
- Custom message types (declaration-merged into `CustomAgentMessages`) are how extensions persist state into the same append-only session log. This is Lecture 26's principle made operational.
- Mid-stream steering (`Enter` queues a message; `Alt+Enter` queues follow-up; `Escape × 2` opens `/tree`) is a UX primitive built on the assumption that the human is in the loop continuously, not only at turn boundaries.
- Every major Pi design decision maps back to a section of Lecture 02 (harness concerns) and Lecture 26 (event-sourced session). Pi is the cleanest public instance of those principles in shipping code.
- For hardware-track learners: minimal cores, provider-agnostic SDKs, and tree-structured sessions are exactly the substrate properties edge AI deployments need.

---

**References**

**Primary sources**

- pi-mono repository — [https://github.com/badlogic/pi-mono](https://github.com/badlogic/pi-mono)
- `packages/coding-agent` README — [https://github.com/badlogic/pi-mono/blob/main/packages/coding-agent/README.md](https://github.com/badlogic/pi-mono/blob/main/packages/coding-agent/README.md)
- `packages/agent` README — [https://github.com/badlogic/pi-mono/tree/main/packages/agent](https://github.com/badlogic/pi-mono/tree/main/packages/agent)
- npm: `@mariozechner/pi-coding-agent` — [https://www.npmjs.com/package/@mariozechner/pi-coding-agent](https://www.npmjs.com/package/@mariozechner/pi-coding-agent)
- npm: `@mariozechner/pi-agent-core` — [https://www.npmjs.com/package/@mariozechner/pi-agent-core](https://www.npmjs.com/package/@mariozechner/pi-agent-core)
- npm: `@mariozechner/pi-ai` — [https://www.npmjs.com/package/@mariozechner/pi-ai](https://www.npmjs.com/package/@mariozechner/pi-ai)
- npm: `@mariozechner/pi-tui` — [https://www.npmjs.com/package/@mariozechner/pi-tui](https://www.npmjs.com/package/@mariozechner/pi-tui)

**Sister and consumer projects**

- `badlogicgames/pi-share-hf` — session publishing to Hugging Face: [https://github.com/badlogicgames/pi-share-hf](https://github.com/badlogicgames/pi-share-hf)
- `earendil-works/pi-chat` — Slack / chat workflows on Pi: [https://github.com/earendil-works/pi-chat](https://github.com/earendil-works/pi-chat)
- OpenClaw — multi-channel agent platform consuming Pi as a runtime: [https://github.com/openclaw/openclaw](https://github.com/openclaw/openclaw)
- pi.dev — the project's primary domain.

</details>

### 背景

- Armin Ronacher，*Pi: The Minimal Agent Within OpenClaw*（2026 年 1 月 31 日）——引出本讲主题的框架性文章。

### 课程交叉引用

- [Lecture 37 - OpenClaw System Prompt Architecture](Lecture-37.md)
- [Lecture 02 - What Is an AI Agent Harness?](Lecture-02.md)
- [Lecture 26 - Session as Source of Truth](Lecture-26.md)
- [Lecture 03 - OpenCoven Local Harness Substrate](Lecture-03.md)
- [Lecture 04 - OpenKnots Trustworthy Agent Interfaces](Lecture-04.md)
- [Lecture 25 - AI Agent Security Engineer Roadmap](Lecture-25.md)
- [Phase 4 / Track B / VLA Deployment on Edge GPUs](../../../../阶段4-路线B-Nvidia-Jetson/05-应用开发/05-机器学习与人工智能/06-Jetson-VLA部署/Guide.md) — 面向 VLA 工作负载的姊妹级最小基底工程姿态。

---

*Next: [Lecture 42](Lecture-42.md)*


<details>
<summary>English original</summary>

**Context**

- Armin Ronacher, *Pi: The Minimal Agent Within OpenClaw* (Jan 31, 2026) — the framing essay that introduced this lecture's subject.

**Curriculum cross-references**

- [Lecture 37 - OpenClaw System Prompt Architecture](Lecture-37.md)
- [Lecture 02 - What Is an AI Agent Harness?](Lecture-02.md)
- [Lecture 26 - Session as Source of Truth](Lecture-26.md)
- [Lecture 03 - OpenCoven Local Harness Substrate](Lecture-03.md)
- [Lecture 04 - OpenKnots Trustworthy Agent Interfaces](Lecture-04.md)
- [Lecture 25 - AI Agent Security Engineer Roadmap](Lecture-25.md)
- [Phase 4 / Track B / VLA Deployment on Edge GPUs](../../../../阶段4-路线B-Nvidia-Jetson/05-应用开发/05-机器学习与人工智能/06-Jetson-VLA部署/Guide.md) — sibling minimal-substrate engineering posture for VLA workloads.

---

*Next: [Lecture 42](Lecture-42.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-41.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-41.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
