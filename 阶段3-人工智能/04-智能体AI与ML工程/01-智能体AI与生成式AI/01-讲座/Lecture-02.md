---
title: 第 02 讲 - 什么是 AI 智能体 harness？模型周围的 runtime
description: 第 02 讲 - 什么是 AI 智能体 harness？模型周围的 runtime
---

# 第 02 讲 - 什么是 AI 智能体 harness？模型周围的 runtime

**课程：** [AI 智能体开发 2026](../Guide.md) | **上一讲：** [第 01 讲](Lecture-01.md) | **下一讲：** [第 03 讲](Lecture-03.md)

---

单独的大型语言模型是一个**无状态函数**：

```text
prompt + tools spec  ->  text + tool calls
```

那不是一个 agent。

一个 **agent** 出现，当模型周围的某物：

- 决定模型能看到哪些工具
- 在模型请求时调用那些工具
- 将结果反馈到下一轮
- 决定何时停止、总结或移交
- 维护工作区、文件、身份和预算
- 强制执行模型允许和不允许接触的内容

那个“模型周围的某物”就是 **harness**。

本讲定义 harness，列出它拥有的职责，走过三个具体的生产 harness，并解释为什么硬件方向的工程师应该关心。

---

## 学习目标

本讲结束时，你应该能够：

1. 定义什么是 AI 智能体 harness，以及为什么它与模型分离。
2. 列出 harness 必须拥有的六项职责。
3. 解释为什么每项职责都不能留给模型。
4. 识别 Claude Code、Cursor 和 OpenAI Codex 中的 harness layer。
5. 阅读一份记录，识别哪些动作来自模型，哪些来自 harness。
6. 当 harness 驱动推理引擎时，从硬件感知的角度推理吞吐、批处理和局部性。
7. 识别常见的 harness 反模式：“一切都在提示词中”的陷阱、无监督的工具使用、上下文膨胀和隐藏状态。
8. 为你自己的项目勾勒一个最小 harness。

---

## 1. 心智模型：模型是 CPU，harness 是 OS

单独的模型更接近 **CPU**，而不是计算机。

CPU 执行指令，但自身不能：

- 决定加载哪些程序
- 仲裁对磁盘、网络或 GPU 的访问
- 在内存耗尽时交换上下文
- 强制执行权限
- 从故障中恢复
- 在重启之间保持状态

操作系统做这些事情。

模型和有用的 agent 之间存在同样的差距：

```text
+----------------------------------------------------+
|                   user / product                   |
+----------------------------------------------------+
|                       harness                      |   <- this lecture
|   tools | memory | context | planning | policy ... |
+----------------------------------------------------+
|                       model                        |
+----------------------------------------------------+
```

模型进行推理。
harness 运行。

如果你的产品行为不可靠，原因**几乎总是在 harness 中**，而不是在模型权重中。

---

## 2. harness 拥有的六件事

一个严肃的 harness 拥有六项关注点。跳过其中任何一项，系统在生产中就不再可用。

```text
1. Tool dispatch          (the device-driver layer)
2. State and memory       (RAM, files, sessions)
3. Context construction   (what fits in the prompt this turn)
4. Planning and recovery  (turn loop, retries, sub-agents)
5. Policy and permission  (what tools, paths, networks are allowed)
6. Extensibility          (skills, MCP servers, plugins, channels)
```

每一项都表现为你必须编写或购买的代码。

### 2.1 工具调度

模型发出结构化的工具调用。harness 必须：

- 验证模式
- 确定要运行哪个实现
- 执行它（进程内、子进程、MCP 服务器、远程 RPC）
- 捕获 stdout、stderr、退出码、返回值
- 截断嘈杂输出而不丢失信号
- 返回模型可读的规范化工具结果

没有这一层，模型可以请求工具，但**什么也不会发生**。

### 2.2 状态和记忆

三个时间范围需要单独的机制：

- **轮次状态。** 当前工具调用队列、部分输出、锁。
- **会话状态。** 本次运行的对话历史，加上所触及文件和所做决策的工作记忆。
- **跨会话记忆。** agent 带到下一个对话的持久事实：用户配置文件、项目约定、先前的决策。

模型没有记忆；**harness 伪造它**，通过将先前状态塞入下一个提示词或将其作为工具暴露。

### 2.3 上下文构建

每一轮，harness 组装一个新的提示词：

- 系统提示词和身份
- 工具目录（完整、紧凑或无）
- 引导文件和项目上下文
- 技能描述
- 判断为相关的记忆条目
- 对话记录，可能被压缩
- 提供商特定的覆盖（缓存标记、beta 标头）

这是整个系统中**对上下文窗口最敏感的工作**。

总是发送完整记录的 harness 会**让你破产并降低输出质量**。盲目压缩的 harness 会默默丢弃关键细节。

参见 [第 37 讲 - 系统提示词架构](Lecture-37.md) 以深入了解一种生产方法。


<details>
<summary>English original</summary>

**Lecture 02 - What Is an AI Agent Harness? The Runtime Around the Model**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 01](Lecture-01.md) | **Next:** [Lecture 03](Lecture-03.md)

---

A large language model on its own is a **stateless function**:

```text
prompt + tools spec  ->  text + tool calls
```

That is not an agent.

An **agent** appears when something around the model:

- decides which tools the model is allowed to see
- calls those tools when the model asks
- feeds the results back into the next turn
- decides when to stop, summarize, or hand off
- keeps a workspace, files, identities, and budgets straight
- enforces what the model is and is not allowed to touch

That "something around the model" is the **harness**.

This lecture defines the harness, lists what it owns, walks through three concrete production harnesses, and explains why hardware-track engineers should care.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Define what an AI agent harness is and why it is separate from the model.
2. List the six responsibilities a harness must own.
3. Explain why each responsibility cannot be left to the model.
4. Recognize the harness layer in Claude Code, Cursor, and OpenAI Codex.
5. Read a transcript and identify which actions came from the model and which came from the harness.
6. Reason about throughput, batching, and locality from a hardware-aware perspective when a harness drives an inference engine.
7. Identify common harness anti-patterns: the "everything in the prompt" trap, unsupervised tool use, context bloat, and hidden state.
8. Sketch a minimal harness for a project of your own.

---

**1. Mental model: model is a CPU, harness is the OS**

A model alone is closer to a **CPU** than to a computer.

A CPU executes instructions but cannot, by itself:

- decide which programs to load
- arbitrate access to disk, network, or GPU
- swap context when memory runs out
- enforce permissions
- recover from a fault
- keep state across reboots

An operating system does those things.

The same gap exists between a model and a useful agent:

```text
+----------------------------------------------------+
|                   user / product                   |
+----------------------------------------------------+
|                       harness                      |   <- this lecture
|   tools | memory | context | planning | policy ... |
+----------------------------------------------------+
|                       model                        |
+----------------------------------------------------+
```

The model reasons.
The harness runs.

If your product behavior is unreliable, the cause is **almost always in the harness**, not in the model weights.

---

**2. The six things a harness owns**

A serious harness owns six concerns. Skip any of them and the system stops being usable in production.

```text
1. Tool dispatch          (the device-driver layer)
2. State and memory       (RAM, files, sessions)
3. Context construction   (what fits in the prompt this turn)
4. Planning and recovery  (turn loop, retries, sub-agents)
5. Policy and permission  (what tools, paths, networks are allowed)
6. Extensibility          (skills, MCP servers, plugins, channels)
```

Each one shows up as code you have to write or buy.

**2.1 Tool dispatch**

The model emits a structured tool call. The harness must:

- validate the schema
- resolve which implementation to run
- execute it (in-process, subprocess, MCP server, remote RPC)
- capture stdout, stderr, exit codes, return values
- truncate noisy output without losing the signal
- return a normalized tool result the model can read

Without this layer the model can ask for tools but **nothing happens**.

**2.2 State and memory**

Three time horizons need separate machinery:

- **Turn state.** The current tool call queue, partial outputs, locks.
- **Session state.** Conversation history for this run, plus working memory of files touched and decisions made.
- **Cross-session memory.** Persistent facts the agent carries to the next conversation: user profile, project conventions, prior decisions.

Models do not have memory; the **harness fakes it** by stuffing prior state into the next prompt or by exposing it as a tool.

**2.3 Context construction**

Every turn the harness assembles a fresh prompt:

- system prompt and identity
- tool catalog (full, compact, or none)
- bootstrap files and project context
- skill descriptions
- memory entries judged relevant
- conversation transcript, possibly compacted
- provider-specific overlays (cache markers, beta headers)

This is the **most context-window-sensitive job** in the whole system.

A harness that always sends the full transcript will **bankrupt you and degrade output**. A harness that compacts blindly will silently drop load-bearing detail.

See [Lecture 37 - System Prompt Architecture](Lecture-37.md) for an in-depth look at one production approach.

</details>

### 2.4 规划与恢复

harness（agent 运行时框架）运行循环：

```text
loop:
  build prompt
  call model
  if model returns tool calls -> dispatch, capture results, continue
  if model returns text       -> stream to user, decide if turn is done
  if error                    -> classify, retry or surface
  if budget exceeded          -> stop with partial result
```


它还决定：

- 是否为并行工作派生 sub-agent
- 何时需要人工批准
- 如何从畸形的工具调用中恢复
- 何时放弃计划并重新规划

### 2.5 策略与权限

模型**没有良知，也不感知影响**。

harness 强制执行：

- 哪些工具可见
- 哪些文件路径可读、可写或被拒绝
- 哪些网络目标可达
- 哪些命令运行前需要用户确认
- 哪些密钥可达、哪些被掩码
- 哪些操作记入审计日志

这必须是 **runtime 强制执行**，而不是系统提示词中的建议性文字。凡是你只是请求模型去做的事，模型最终都会跳过。

见 [Lecture 24 - Runtime Discipline & AI Runtime Security](Lecture-24.md)。

### 2.6 可扩展性

**真实的 agent 会成长。** 用户添加技能，组织添加 MCP server，产品添加通道。

harness 需要一个稳定的插件面，以便：

- 新工具落地无需重写循环
- 新技能可被模型发现
- 新传输通道（聊天 UI、终端、IDE、语音）复用同一内核

如果扩展只能通过修改内核来添加，harness 会在**几个月内僵化**。

---

## 3. 三个真实世界的 harness 并排对比

看清 harness 是什么，最清楚的方式就是看三个实例。

### 3.1 Claude Code

一个封装 Anthropic Messages API 的**终端 harness**。

负责：

- `Read`、`Edit`、`Write`、`Glob`、`Grep`、`Bash`、sub-`Agent` 工具
- 权限系统，在首次使用危险 shell 命令时提示
- 对话接近模型上限时进行上下文压缩
- 技能与 MCP server 作为扩展层
- 项目作用域的 CLAUDE.md 自动加载为引导上下文
- 后台任务、定时任务与 hook

模型**从不直接打开文件或运行进程**。是 Claude Code harness 在做。

### 3.2 Cursor

一个封装多个模型提供方的 **IDE harness**。

负责：

- 感知编辑器的工具（多文件编辑、代码库搜索、lint 集成）
- `.cursor/rules/` 文件作为 runtime 注入的指引
- 用于可重复领域工作流的 Skills 系统
- 用于外部工具服务器的 MCP
- 内联 diff 以及与编辑器 UI 绑定的应用/回退循环

这里的 harness 就是**编辑器本身**。剥掉编辑器，就没有 agent。

### 3.3 OpenAI Codex（CLI）

一个封装 OpenAI 模型的**编码任务 harness**。

负责：

- 面向大型代码库的 repo 索引
- 用于执行命令的沙箱 shell
- 以 patch 形式应用于工作树的编辑
- 危险操作的批准模式
- 周期性的上下文清理 pass

同一形态，不同默认值：同样六个关注点，针对非交互式编码任务调优。

### 3.4 三者之间的相同点是什么？

```text
              Claude Code     Cursor          Codex CLI
tools         shell + files   editor + tools  shell + patches
memory        CLAUDE.md +     rules + chat    repo index +
              session         history         scratch
context       compaction +    rule injection  cleanup pass
              skills
planning      sub-agents      single loop +   approval modes
              + hooks         apply
policy        per-tool        rule files +    approval modes
              prompts                         sandbox
extension     MCP + skills +  MCP + rules +   plugins
              hooks           skills
```


不同的表层，**同样的六项职责**。

---


<details>
<summary>English original</summary>

**2.4 Planning and recovery**

The harness runs the loop:

```text
loop:
  build prompt
  call model
  if model returns tool calls -> dispatch, capture results, continue
  if model returns text       -> stream to user, decide if turn is done
  if error                    -> classify, retry or surface
  if budget exceeded          -> stop with partial result
```

It also decides:

- whether to spawn a sub-agent for parallel work
- when to require human approval
- how to recover from a malformed tool call
- when to abandon a plan and replan

**2.5 Policy and permission**

The model has **no conscience and no awareness of impact**.

The harness enforces:

- which tools are even visible
- which file paths are readable, writable, or denied
- which network destinations are reachable
- which commands need user confirmation before running
- which secrets are reachable and which are masked
- which actions get audit-logged

This must be **runtime enforcement**, not advisory text in the system prompt. Anything you only ask the model to do, the model will eventually skip.

See [Lecture 24 - Runtime Discipline & AI Runtime Security](Lecture-24.md).

**2.6 Extensibility**

**Real agents grow.** Users add skills, organizations add MCP servers, products add channels.

The harness needs a stable plug-in surface so:

- new tools land without rewriting the loop
- new skills become discoverable to the model
- new transports (chat UI, terminal, IDE, voice) reuse the same core

If extensions can only be added by editing the core, the harness will **calcify within months**.

---

**3. Three real-world harnesses, side by side**

The clearest way to see what a harness is is to look at three of them.

**3.1 Claude Code**

A **terminal harness** wrapping the Anthropic Messages API.

Owns:

- `Read`, `Edit`, `Write`, `Glob`, `Grep`, `Bash`, sub-`Agent` tools
- a permission system that prompts on first use of risky shell commands
- context compaction once the conversation approaches the model limit
- skills and MCP servers as the extensibility layer
- a project-scoped CLAUDE.md auto-loaded as bootstrap context
- background tasks, scheduled tasks, and hooks

The model **never opens a file or runs a process directly**. The Claude Code harness does.

**3.2 Cursor**

An **IDE harness** wrapping multiple model providers.

Owns:

- editor-aware tools (multi-file edits, codebase search, lint integration)
- `.cursor/rules/` files as runtime-injected guidance
- a Skills system for repeatable domain workflows
- MCP for external tool servers
- inline diffs and an apply/revert loop tied to the editor's UI

The harness here is the **editor itself**. Strip away the editor and there is no agent.

**3.3 OpenAI Codex (CLI)**

A **coding-task harness** wrapping OpenAI models.

Owns:

- repo indexing for large codebases
- a sandboxed shell for command execution
- patch-style edits applied to the working tree
- approval modes for risky actions
- a periodic context-cleanup pass

Same shape, different defaults: same six concerns, tuned for non-interactive coding tasks.

**3.4 What is the same across all three?**

```text
              Claude Code     Cursor          Codex CLI
tools         shell + files   editor + tools  shell + patches
memory        CLAUDE.md +     rules + chat    repo index +
              session         history         scratch
context       compaction +    rule injection  cleanup pass
              skills
planning      sub-agents      single loop +   approval modes
              + hooks         apply
policy        per-tool        rule files +    approval modes
              prompts                         sandbox
extension     MCP + skills +  MCP + rules +   plugins
              hooks           skills
```

Different surface, **same six responsibilities**.

---

</details>

## 4. 为什么硬件方向的工程师必须关心

这份路线图讲的是硬件。那为什么要花一讲来讲软件 harness（agent 运行时框架）？

因为**真正打到硬件上的是 harness**。

当你构建：

- 一个托管在 Jetson 上的边缘推理服务
- 一个跑在 CPU shim 之下的 FPGA 加速器
- 一个跑在 H100 上的私有 vLLM 集群
- 一个针对批处理 decode（逐 token 生成阶段）优化的 CUDA kernel

你的客户**几乎肯定是一个 harness**，而不是敲键盘的人。

只有 harness 才会告诉你、但会改变你硬件设计的一些事：

- **批的形状。** 会扇出并行 sub-agent 的 harness 会产生大规模并发批。单循环的 harness 一次只发一个请求。你的调度器与 KV 缓存布局取决于此。
- **提示词缓存复用。** 在多轮之间保持系统提示词稳定的 harness，可以用提示词缓存换来 5-10 倍的吞吐。每轮都改动系统提示词的 harness 则做不到。
- **工具延迟预算。** harness 决定在超时之前愿意等一个工具多久。这决定了你的硬件工具后端有 200 ms 还是 30 s 的余量。
- **流式与完整响应。** 驱动聊天 UI 的 harness 走流式；驱动 CI 任务的 harness 做缓冲。两种情况对你的推理服务器的内存压力不同。
- **局部性。** “local harness substrate”（见 [Lecture 03](Lecture-03.md)）希望模型就在同一台机器上。网关型 harness 会把许多用户多路复用到集群上。边缘与数据中心的设计从这一点开始分道扬镳。

如果你只考虑 FLOPs 和字节，就会**为错误的工作负载做优化**。

---

## 5. 用伪代码写的最小 harness

剥去生产环境的种种考量，一个 harness 大约 80 行就够：

```python
class MinimalHarness:
    def __init__(self, model, tools, policy, memory):
        self.model = model
        self.tools = {t.name: t for t in tools}
        self.policy = policy
        self.memory = memory

    def run(self, user_input, max_turns=20, token_budget=200_000):
        history = self.memory.load_session()
        history.append({"role": "user", "content": user_input})

        for turn in range(max_turns):
            prompt = self.build_prompt(history)
            if self.token_count(prompt) > token_budget:
                history = self.compact(history)
                prompt = self.build_prompt(history)

            response = self.model.call(prompt, tools=self.visible_tools())

            if response.tool_calls:
                results = []
                for call in response.tool_calls:
                    if not self.policy.allow(call):
                        results.append(self.deny_result(call))
                        continue
                    results.append(self.dispatch(call))
                history.append({"role": "assistant", "content": response})
                history.append({"role": "tool", "content": results})
                continue

            history.append({"role": "assistant", "content": response.text})
            self.memory.save_session(history)
            return response.text

        raise RuntimeError("turn budget exceeded")

    def visible_tools(self):
        return [t.spec for t in self.tools.values() if self.policy.visible(t)]

    def dispatch(self, call):
        tool = self.tools[call.name]
        try:
            return {"ok": True, "data": tool(**call.args)}
        except Exception as e:
            return {"ok": False, "error": str(e)}
```

注意模型里**没有**什么：

- 循环本身
- 工具派发
- token 预算与压缩
- 策略检查
- 会话持久化

所有这些都属于 **harness**。模型只负责「给定这条提示词，产出文本或工具调用」。

---

## 6. 常见的 harness 错误

每个团队的第一套 agent 系统里都会出现这些错误。

### 6.1 把策略写进提示词

```text
"Never run rm -rf without asking the user first."
```

模型会遵守 99 次。**第 100 次不会**。

策略属于 **dispatch**，不属于文字表述。

### 6.2 让对话记录无限增长

没有压缩或摘要，提示词会**随轮次数线性增长**。延迟、成本与性能退化一起上升。几个小时后 agent 变得不可用，而没人知道原因。

从第一天起就把压缩做进去，哪怕是最朴素的实现。

### 6.3 隐藏状态

如果 harness 改动了文件、环境变量或外部服务，却没有把改动记入记忆，下一轮的模型就会基于**过时的世界视图**推理。于是它会以令人费解的方式“出错”。

**每一个副作用**都应出现在下一轮提示词里，或能按需取回。

### 6.4 没有回放

如果 harness 没有逐轮的 `(prompt, model output, tool calls, tool results)` 日志，就**无法调试**。把 trace 当作**一等产物**，而不是事后补的东西。


<details>
<summary>English original</summary>

**4. Why hardware-track engineers must care**

This roadmap is about hardware. So why a lecture on software harnesses?

Because **the harness is what hits your hardware**.

When you build:

- a Jetson-hosted edge inference service
- an FPGA accelerator under a CPU shim
- a private vLLM cluster on H100s
- a CUDA kernel optimized for batched decode

your customer is **almost certainly a harness**, not a human typing.

Things only a harness can tell you, but that change your hardware design:

- **Batch shape.** A harness that fans out parallel sub-agents creates large concurrent batches. A single-loop harness sends one request at a time. Your scheduler and KV-cache layout depend on this.
- **Prompt cache reuse.** Harnesses that keep system prompts stable across turns can use prompt caching for 5-10x throughput. Harnesses that mutate the system prompt every turn cannot.
- **Tool latency budget.** The harness picks how long it will wait for a tool before timing out. That decides whether your hardware tool back-end has 200 ms or 30 s of headroom.
- **Streaming vs full-response.** A harness driving a chat UI streams; a harness driving a CI job buffers. Memory pressure on your inference server is different in each case.
- **Locality.** A "local harness substrate" (see [Lecture 03](Lecture-03.md)) wants its model on the same machine. A gateway harness multiplexes many users across a cluster. Edge vs datacenter design diverges from this point.

If you only think about FLOPs and bytes, you will **optimize for the wrong workload**.

---

**5. A minimal harness in pseudocode**

Strip away the production concerns and a harness fits in roughly 80 lines:

```python
class MinimalHarness:
    def __init__(self, model, tools, policy, memory):
        self.model = model
        self.tools = {t.name: t for t in tools}
        self.policy = policy
        self.memory = memory

    def run(self, user_input, max_turns=20, token_budget=200_000):
        history = self.memory.load_session()
        history.append({"role": "user", "content": user_input})

        for turn in range(max_turns):
            prompt = self.build_prompt(history)
            if self.token_count(prompt) > token_budget:
                history = self.compact(history)
                prompt = self.build_prompt(history)

            response = self.model.call(prompt, tools=self.visible_tools())

            if response.tool_calls:
                results = []
                for call in response.tool_calls:
                    if not self.policy.allow(call):
                        results.append(self.deny_result(call))
                        continue
                    results.append(self.dispatch(call))
                history.append({"role": "assistant", "content": response})
                history.append({"role": "tool", "content": results})
                continue

            history.append({"role": "assistant", "content": response.text})
            self.memory.save_session(history)
            return response.text

        raise RuntimeError("turn budget exceeded")

    def visible_tools(self):
        return [t.spec for t in self.tools.values() if self.policy.visible(t)]

    def dispatch(self, call):
        tool = self.tools[call.name]
        try:
            return {"ok": True, "data": tool(**call.args)}
        except Exception as e:
            return {"ok": False, "error": str(e)}
```

Notice what is **not** in the model:

- the loop itself
- tool dispatch
- token budgeting and compaction
- policy checks
- session persistence

All of that is the **harness**. The model only handles "given this prompt, produce text or tool calls."

---

**6. Common harness mistakes**

These appear in every team's first agent system.

**6.1 Putting policy in the prompt**

```text
"Never run rm -rf without asking the user first."
```

The model will obey 99 times. The **100th time it will not**.

Policy belongs in **dispatch**, not in prose.

**6.2 Letting the transcript grow forever**

Without compaction or summarization, the prompt **grows linearly with turn count**. Latency, cost, and degradation all rise together. After a few hours the agent becomes unusable and nobody knows why.

Build compaction in from day one, even a naive one.

**6.3 Hidden state**

If the harness mutates files, environment variables, or external services without recording the change in memory, the next turn's model will reason from a **stale view of the world**. It will then be "wrong" in confusing ways.

**Every side effect** should appear in the next prompt or be retrievable on demand.

**6.4 No replay**

A harness with no log of `(prompt, model output, tool calls, tool results)` per turn is **impossible to debug**. Treat the trace as a **first-class artifact**, not an afterthought.

</details>

### 6.5 工具太多

每个工具 spec 都要**消耗 token，并干扰工具选择**。一次暴露 80 个工具的 harness（agent 运行时框架），输出效果会逊于同一个 harness 只暴露 8 个与上下文相关的工具。

技能系统的存在正是为了解决这一点：**按需加载工具**。

---

## 7. 动手构建：读懂你自己的 harness

挑一个你日常使用的 harness（Claude Code、Cursor、Codex、Continue、Aider，或者你自己的）。在构建自己的 harness 之前，先找出这些问题的答案：

1. 主轮次循环在哪？跟踪一次迭代。
2. 它如何检测模型想要调用工具？
3. 它如何分发工具？
4. 它在何处记录结果？
5. 什么会触发上下文压缩？哪些内容会被丢弃？
6. 权限检查在哪？它们是建议性的还是强制性的？
7. 会话如何在重启后持久化？
8. 扩展面是什么（MCP、插件、技能、规则）？

如果你对每天都在用的 harness 答不上其中任何一个问题，那就是需要读源码去填补的缺口。

---

## 8. 交付

产物：一页纸的架构草图，描绘你用过的或设计过的 harness。图中必须标注：

- 模型边界
- 工具分发路径
- 记忆存储
- 上下文构建步骤
- 策略执行点
- 可扩展面

审阅者应当能够指着 agent 任何一个用户可见的行为，说出是哪个方框负责的。如果做不到，这张图就不完整。

---

## 关键要点

- 模型是一个函数。agent 是模型加上 harness。
- harness 负责六件事：工具分发、记忆、上下文、规划、策略、可扩展性。
- 漏掉其中任何一项，系统在生产环境就会失败。
- Claude Code、Cursor 和 Codex 是同一套六项职责之上的不同界面。
- 策略必须在分发环节强制执行，而不是在提示词里请求。
- 上下文构建是系统里对上下文窗口最敏感的代码。
- 硬件工程师应当关心这一点，因为真正打到推理硬件上的工作负载是 harness，而不是用户。
- 最小但正确的 harness 很小。生产级 harness 基本上就是本讲列出的这些东西，只是写得更仔细。

---

## 参考文献

- bswen — *What Is an AI Agent Harness? The Operating System for Autonomous Coding Agents*: [https://docs.bswen.com/blog/2026-03-25-ai-agent-harness-explained/](https://docs.bswen.com/blog/2026-03-25-ai-agent-harness-explained/)
- Anthropic — Claude Code 文档：[https://docs.claude.com/en/docs/claude-code](https://docs.claude.com/en/docs/claude-code)
- Cursor — Rules and Skills 文档：[https://docs.cursor.com/](https://docs.cursor.com/)
- OpenAI — Codex CLI：[https://github.com/openai/codex](https://github.com/openai/codex)
- Model Context Protocol (MCP) 规范：[https://modelcontextprotocol.io/](https://modelcontextprotocol.io/)
- [Lecture 24 - Runtime Discipline & AI Runtime Security](Lecture-24.md)
- [Lecture 37 - OpenClaw System Prompt Architecture](Lecture-37.md)
- [Lecture 03 - OpenCoven: Local Harness Substrate](Lecture-03.md)

---

*下一讲：[Lecture 03](Lecture-03.md)*


<details>
<summary>English original</summary>

**6.5 Too many tools**

Every tool spec **costs tokens and confuses tool selection**. A harness that exposes 80 tools at once will produce worse output than the same harness exposing 8 contextually relevant ones.

Skill systems exist to solve this: **load tools on demand**.

---

**7. Build it: read your own harness**

Pick the harness you use day to day (Claude Code, Cursor, Codex, Continue, Aider, your own). Find the answers to these questions before you build your own:

1. Where is the main turn loop? Trace one iteration.
2. How does it detect that the model wants to call a tool?
3. How does it dispatch the tool?
4. Where does it record the result?
5. What triggers context compaction, and what gets dropped?
6. Where are the permission checks? Are they advisory or enforced?
7. How is a session persisted across restarts?
8. What is the extension surface (MCP, plugins, skills, rules)?

If you cannot answer one of these for a harness you use every day, that is the gap to read source code into.

---

**8. Ship it**

Artifact: a one-page architecture sketch of a harness you have used or designed. It must label:

- the model boundary
- the tool dispatch path
- the memory store
- the context-construction step
- the policy enforcement points
- the extensibility surface

A reviewer should be able to point at any user-visible behavior of the agent and say which box was responsible. If they cannot, the diagram is incomplete.

---

**Key takeaways**

- A model is a function. An agent is a model plus a harness.
- A harness owns six things: tool dispatch, memory, context, planning, policy, extensibility.
- Skip any one of them and the system fails in production.
- Claude Code, Cursor, and Codex are different surfaces over the same six responsibilities.
- Policy must be enforced at dispatch, not asked for in the prompt.
- Context construction is the most context-window-sensitive code in the system.
- Hardware engineers should care because the harness, not the user, is the actual workload that hits inference hardware.
- A minimal but correct harness is small. A production harness is mostly the things this lecture lists, written carefully.

---

**References**

- bswen — *What Is an AI Agent Harness? The Operating System for Autonomous Coding Agents*: [https://docs.bswen.com/blog/2026-03-25-ai-agent-harness-explained/](https://docs.bswen.com/blog/2026-03-25-ai-agent-harness-explained/)
- Anthropic — Claude Code documentation: [https://docs.claude.com/en/docs/claude-code](https://docs.claude.com/en/docs/claude-code)
- Cursor — Rules and Skills documentation: [https://docs.cursor.com/](https://docs.cursor.com/)
- OpenAI — Codex CLI: [https://github.com/openai/codex](https://github.com/openai/codex)
- Model Context Protocol (MCP) specification: [https://modelcontextprotocol.io/](https://modelcontextprotocol.io/)
- [Lecture 24 - Runtime Discipline & AI Runtime Security](Lecture-24.md)
- [Lecture 37 - OpenClaw System Prompt Architecture](Lecture-37.md)
- [Lecture 03 - OpenCoven: Local Harness Substrate](Lecture-03.md)

---

*Next: [Lecture 03](Lecture-03.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
