---
title: 第 17 讲 - Agent SDK 与 Runtime API
description: 第 17 讲 - Agent SDK 与 Runtime API
---

# 第 17 讲 - Agent SDK 与 Runtime API

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 16](Lecture-16.md) | **Next:** [Lecture 18](Lecture-18.md)

---

## 学习目标

本讲结束时，你将能够：

- 说明原始模型 API、agent SDK、workflow runtime、MCP 与 agent 网关之间的区别。
- 为模型调用、工具调用、handoffs、流式输出与日志设计一个轻量的、provider 中立的 runtime 契约。
- 判断何时使用托管 SDK、何时自己掌控循环、何时把编排移入图或网关。
- 把工具、MCP server 与 subagent 视为安全边界，而不只是便利封装。
- 添加调试、评估与审计所需的最小 runtime 遥测。

---

## 1. 为什么本讲取代了「单一厂商 SDK」

早期的 agent 教程通常只教一种模式：

1. 把 prompt 发给模型。
2. 如果模型请求某个工具，就调用该函数。
3. 把工具结果发回。
4. 重复，直到模型停止。

这个循环依然真实存在，但**生产环境的 agent 系统**如今有了更多结构。现代的 **agent stack** 通常包含若干层：

| Layer | 简单含义 | 示例 |
|-------|----------------|----------|
| Model API | 直接的推理接口 | responses/messages API、结构化输出、工具调用 |
| Agent SDK | 围绕模型调用的托管循环 | agents、tools、handoffs、sessions、guardrails、tracing |
| Workflow runtime | 持久化的控制流 | graph 执行、checkpoints、人工审核、重试 |
| Tool protocol | 标准化的外部能力 | MCP tools、resources、prompts |
| Gateway/control plane | 产品级路由与会话 | OpenClaw 风格的 channels、sessions、agents、nodes |
| Runtime policy | 执行期的安全与治理 | authorization、allowlists、audit logs、approval gates |

重要的技能不是记住某个包名，而是知道**哪一层承担哪项职责**。

---

## 2. 2026 年的 Agent Runtime 地图

选择架构时可用这张图。

| 场景 | 最佳起点 | 原因 |
|-----------|--------------------|-----|
| 单个短任务，不用工具 | 原生 provider API | 复杂度最低 |
| 一个带少量函数工具的助手 | Agent SDK | 内置循环、工具分发、流式输出、trace |
| 带重试与审核的多步 workflow | Workflow runtime | 持久化执行与显式状态转换 |
| 大量外部工具或应用 | MCP | 标准的 tool/resource/prompt 集成边界 |
| 多 channel 助手或 local-first 产品 | 网关/控制平面 | 路由、会话、鉴权、配对、设备/channel 隔离 |
| 受监管或高风险操作 | runtime 策略层 | 在 LLM 之外做确定性的授权 |

具体示例：

- OpenAI Agents SDK 的文档涵盖 agents、tools、handoffs、guardrails、sessions、tracing 与 MCP 集成。
- 当 agent 是需要检查点与 human-in-the-loop 控制的长时间运行、有状态 workflow 时，LangGraph 很有用。
- 当你希望工具与上下文 server 能在 IDE、聊天应用、本地助手与 agent runtime 之间复用时，MCP 很有用。
- OpenClaw 是网关型助手的一个有用案例：channels、sessions、路由、agent 归属与 local-first 控制。

---

## 3. 你应该教给代码库的 runtime 契约

在挑选任何 SDK 之前，先定义你的系统所能理解的工作形态。

```python
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Literal


@dataclass
class ToolSpec:
    name: str
    description: str
    input_schema: dict[str, Any]
    risk: Literal["read", "write", "external", "destructive"] = "read"
    requires_approval: bool = False


@dataclass
class ToolCall:
    call_id: str
    name: str
    arguments: dict[str, Any]


@dataclass
class ToolResult:
    call_id: str
    content: str
    is_error: bool = False


@dataclass
class AgentRequest:
    session_id: str
    user_id: str
    messages: list[dict[str, Any]]
    tools: list[ToolSpec] = field(default_factory=list)
    max_steps: int = 12
    budget_usd: float = 1.00


@dataclass
class RuntimeEvent:
    type: Literal["model_start", "model_delta", "tool_call", "tool_result", "handoff", "policy_block", "done"]
    session_id: str
    payload: dict[str, Any]


@dataclass
class AgentResponse:
    final_text: str
    events: list[RuntimeEvent]
    input_tokens: int = 0
    output_tokens: int = 0
    tool_calls: list[ToolCall] = field(default_factory=list)
```

这个契约很重要，因为 **provider API 的变化速度**快于你的产品架构应有的变化速度。把**厂商特有的响应格式**留在适配器内部。保持你的产品语义稳定。

---


<details>
<summary>English original</summary>

**Lecture 17 - Agent SDKs and Runtime APIs**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 16](Lecture-16.md) | **Next:** [Lecture 18](Lecture-18.md)

---

**Learning Objectives**

By the end of this lecture you will be able to:

- Explain the difference between a raw model API, an agent SDK, a workflow runtime, MCP, and an agent gateway.
- Design a small provider-neutral runtime contract for model calls, tool calls, handoffs, streaming, and logs.
- Decide when to use a managed SDK, when to own the loop yourself, and when to move orchestration into a graph or gateway.
- Treat tools, MCP servers, and subagents as security boundaries instead of just convenience wrappers.
- Add the minimum runtime telemetry needed for debugging, evaluation, and audit.

---

**1. Why This Lecture Replaces "One Vendor SDK"**

Early agent tutorials usually taught one pattern:

1. Send a prompt to a model.
2. If the model asks for a tool, call the function.
3. Send the tool result back.
4. Repeat until the model stops.

That loop is still real, but **production agent systems** now have more structure. A modern **agent stack** usually has several layers:

| Layer | Simple meaning | Examples |
|-------|----------------|----------|
| Model API | The direct inference surface | responses/messages APIs, structured output, tool calls |
| Agent SDK | A managed loop around model calls | agents, tools, handoffs, sessions, guardrails, tracing |
| Workflow runtime | Durable control flow | graph execution, checkpoints, human review, retries |
| Tool protocol | Standardized external capabilities | MCP tools, resources, prompts |
| Gateway/control plane | Product-level routing and sessions | OpenClaw-style channels, sessions, agents, nodes |
| Runtime policy | Safety and governance at execution time | authorization, allowlists, audit logs, approval gates |

The important skill is not memorizing one package name. The important skill is knowing **which layer owns which responsibility**.

---

**2. The 2026 Agent Runtime Map**

Use this map when choosing architecture.

| Situation | Best starting point | Why |
|-----------|--------------------|-----|
| One short task, no tools | Raw provider API | Lowest complexity |
| One assistant with a few function tools | Agent SDK | Built-in loop, tool dispatch, streaming, traces |
| Multi-step workflow with retries and review | Workflow runtime | Durable execution and explicit state transitions |
| Many external tools or apps | MCP | Standard tool/resource/prompt integration boundary |
| Multi-channel assistant or local-first product | Gateway/control plane | Routing, sessions, auth, pairing, device/channel isolation |
| Regulated or high-risk actions | Runtime policy layer | Deterministic authorization outside the LLM |

Concrete examples:

- OpenAI Agents SDK documents agents, tools, handoffs, guardrails, sessions, tracing, and MCP integration.
- LangGraph is useful when the agent is a long-running, stateful workflow that needs checkpointing and human-in-the-loop controls.
- MCP is useful when you want tools and context servers to be reusable across IDEs, chat apps, local assistants, and agent runtimes.
- OpenClaw is a useful case study for gateway-based assistants: channels, sessions, routing, agent ownership, and local-first control.

---

**3. The Runtime Contract You Should Teach Your Codebase**

Before picking any SDK, define the shape of the work your system understands.

```python
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any, Literal


@dataclass
class ToolSpec:
    name: str
    description: str
    input_schema: dict[str, Any]
    risk: Literal["read", "write", "external", "destructive"] = "read"
    requires_approval: bool = False


@dataclass
class ToolCall:
    call_id: str
    name: str
    arguments: dict[str, Any]


@dataclass
class ToolResult:
    call_id: str
    content: str
    is_error: bool = False


@dataclass
class AgentRequest:
    session_id: str
    user_id: str
    messages: list[dict[str, Any]]
    tools: list[ToolSpec] = field(default_factory=list)
    max_steps: int = 12
    budget_usd: float = 1.00


@dataclass
class RuntimeEvent:
    type: Literal["model_start", "model_delta", "tool_call", "tool_result", "handoff", "policy_block", "done"]
    session_id: str
    payload: dict[str, Any]


@dataclass
class AgentResponse:
    final_text: str
    events: list[RuntimeEvent]
    input_tokens: int = 0
    output_tokens: int = 0
    tool_calls: list[ToolCall] = field(default_factory=list)
```

This contract matters because **provider APIs change faster** than your product architecture should. Keep **vendor-specific response formats** inside adapters. Keep your product semantics stable.

---

</details>

## 4. 掌控适配器边界

干净的**适配器**把供应商特有的响应转换成你的 **runtime 契约**。

```python
import os
from typing import Protocol


class ModelAdapter(Protocol):
    def run_turn(
        self,
        messages: list[dict],
        tools: list[ToolSpec],
        model: str,
    ) -> tuple[str, list[ToolCall], dict]:
        """Return text, tool calls, and usage metadata."""
        ...


class AgentRuntime:
    def __init__(self, adapter: ModelAdapter, tool_registry: dict[str, callable]):
        self.adapter = adapter
        self.tool_registry = tool_registry

    def run(self, request: AgentRequest) -> AgentResponse:
        model = os.environ.get("AGENT_MODEL", "default-agent-model")
        messages = list(request.messages)
        events: list[RuntimeEvent] = []
        all_tool_calls: list[ToolCall] = []
        input_tokens = 0
        output_tokens = 0

        for _step in range(request.max_steps):
            text, tool_calls, usage = self.adapter.run_turn(messages, request.tools, model)
            input_tokens += int(usage.get("input_tokens", 0))
            output_tokens += int(usage.get("output_tokens", 0))

            if not tool_calls:
                events.append(RuntimeEvent("done", request.session_id, {"text": text}))
                return AgentResponse(text, events, input_tokens, output_tokens, all_tool_calls)

            all_tool_calls.extend(tool_calls)
            messages.append({"role": "assistant", "content": text, "tool_calls": tool_calls})

            for call in tool_calls:
                tool = next((t for t in request.tools if t.name == call.name), None)
                if tool is None:
                    result = ToolResult(call.call_id, f"Unknown tool: {call.name}", is_error=True)
                elif tool.requires_approval or tool.risk in {"write", "destructive"}:
                    events.append(RuntimeEvent("policy_block", request.session_id, {"tool": call.name}))
                    result = ToolResult(call.call_id, "Blocked: approval required", is_error=True)
                else:
                    handler = self.tool_registry[call.name]
                    result = ToolResult(call.call_id, str(handler(**call.arguments)))

                events.append(RuntimeEvent("tool_result", request.session_id, result.__dict__))
                messages.append({"role": "tool", "tool_call_id": call.call_id, "content": result.content})

        return AgentResponse(
            final_text="Stopped: max_steps reached before completion.",
            events=events,
            input_tokens=input_tokens,
            output_tokens=output_tokens,
            tool_calls=all_tool_calls,
        )
```

适配器可以调用 OpenAI、Anthropic、本地模型或路由网关。runtime 代码不应关心这些。

---

## 5. 工具边界与 MCP

**MCP** 最好理解为一种标准方式，让 AI 应用连接到**外部上下文与能力**。

| MCP 角色 | 通俗解释 |
|----------|---------------|
| Host | 用户正在使用的应用，例如 IDE、桌面助手或聊天客户端 |
| Client | 宿主内部与某个 MCP server 通信的连接器 |
| Server | 暴露工具、资源和 prompt 的服务 |
| Tool | 模型可能请求执行的动作 |
| Resource | 模型或用户可以读取的上下文或数据 |
| Prompt | 可复用的工作流或消息模板 |

MCP 并未消除对授权的需求。它让集成形态更干净，但宿主和 runtime 仍需决定：

- 哪个 server 可信？
- 哪个用户在发起请求？
- 请求的是哪个工具？
- 该工具是只读、可写、外部还是破坏性的？
- 用户是否需要批准这次调用？
- 哪些数据会离开本地边界？

**工程规则：** 把工具描述、检索到的资源和 MCP server 输出视为不可信输入。恶意的工具描述或检索到的文档，可能像恶意用户 prompt 一样试图操纵 agent。

---


<details>
<summary>English original</summary>

**4. Own the Adapter Boundary**

A clean **adapter** converts provider-specific responses into your **runtime contract**.

```python
import os
from typing import Protocol


class ModelAdapter(Protocol):
    def run_turn(
        self,
        messages: list[dict],
        tools: list[ToolSpec],
        model: str,
    ) -> tuple[str, list[ToolCall], dict]:
        """Return text, tool calls, and usage metadata."""
        ...


class AgentRuntime:
    def __init__(self, adapter: ModelAdapter, tool_registry: dict[str, callable]):
        self.adapter = adapter
        self.tool_registry = tool_registry

    def run(self, request: AgentRequest) -> AgentResponse:
        model = os.environ.get("AGENT_MODEL", "default-agent-model")
        messages = list(request.messages)
        events: list[RuntimeEvent] = []
        all_tool_calls: list[ToolCall] = []
        input_tokens = 0
        output_tokens = 0

        for _step in range(request.max_steps):
            text, tool_calls, usage = self.adapter.run_turn(messages, request.tools, model)
            input_tokens += int(usage.get("input_tokens", 0))
            output_tokens += int(usage.get("output_tokens", 0))

            if not tool_calls:
                events.append(RuntimeEvent("done", request.session_id, {"text": text}))
                return AgentResponse(text, events, input_tokens, output_tokens, all_tool_calls)

            all_tool_calls.extend(tool_calls)
            messages.append({"role": "assistant", "content": text, "tool_calls": tool_calls})

            for call in tool_calls:
                tool = next((t for t in request.tools if t.name == call.name), None)
                if tool is None:
                    result = ToolResult(call.call_id, f"Unknown tool: {call.name}", is_error=True)
                elif tool.requires_approval or tool.risk in {"write", "destructive"}:
                    events.append(RuntimeEvent("policy_block", request.session_id, {"tool": call.name}))
                    result = ToolResult(call.call_id, "Blocked: approval required", is_error=True)
                else:
                    handler = self.tool_registry[call.name]
                    result = ToolResult(call.call_id, str(handler(**call.arguments)))

                events.append(RuntimeEvent("tool_result", request.session_id, result.__dict__))
                messages.append({"role": "tool", "tool_call_id": call.call_id, "content": result.content})

        return AgentResponse(
            final_text="Stopped: max_steps reached before completion.",
            events=events,
            input_tokens=input_tokens,
            output_tokens=output_tokens,
            tool_calls=all_tool_calls,
        )
```

The adapter can call OpenAI, Anthropic, a local model, or a routed gateway. The runtime code should not care.

---

**5. Tool Boundaries and MCP**

**MCP** is best understood as a standard way for an AI application to connect to **external context and capabilities**.

| MCP role | Plain English |
|----------|---------------|
| Host | The app the user is using, such as an IDE, desktop assistant, or chat client |
| Client | The connector inside the host that talks to one MCP server |
| Server | The service that exposes tools, resources, and prompts |
| Tool | An action the model may ask to execute |
| Resource | Context or data the model or user may read |
| Prompt | A reusable workflow or message template |

MCP does not remove the need for authorization. It makes the integration shape cleaner, but the host and runtime still need to decide:

- Which server is trusted?
- Which user is asking?
- Which tool is being requested?
- Is the tool read-only, write-capable, external, or destructive?
- Does the user need to approve this call?
- What data will leave the local boundary?

**Engineering rule:** Treat tool descriptions, retrieved resources, and MCP server output as untrusted input. A malicious tool description or retrieved document can try to steer the agent just like a malicious user prompt.

---

</details>

## 6. 交接与 Subagent

**多 agent 系统**中最常见的设计错误，是用「agent」这一个词指代**三种不同的东西**。

| 模式 | 委派之后，谁拥有用户对话？ | 上下文模型 | 适用场景 |
|---------|-----------------------------------------------|---------------|----------|
| Agent 作为工具 | 父 agent 保留所有权 | 隔离，仅请求/响应 | 无状态的专家能力 |
| Subagent | 父 agent 保留所有权 | 通常为经过过滤或摘要的上下文 | 复杂但有界的小问题 |
| 交接 | 所有权转移到另一个 agent/状态 | 跨轮次共享状态 | 多阶段的对话流程 |

通俗地说：

- **Agent 作为工具**的意思是「执行这一项专家功能然后返回」。
- **Subagent** 的意思是「接下这个有界的任务，处理它，然后带结果回来」。
- **交接**的意思是「接下来这部分对话现在归你负责」。

如果没有**明确定义所有权**，就会造成重复劳动、token 膨胀，或产生没有任何 agent 知道该由谁回答用户的死胡同流程。

### 6.1 选择正确的模式

先使用这张决策表。它能避免大多数过度设计的 agent 系统。

| 如果任务看起来像这样 | 使用 | 原因 |
|-----------------------------|-----|-----|
| “为此 schema 生成 SQL。” | Agent 作为工具 | 原子化、可复用、严格的输入/输出 |
| “调研三家供应商并做比较。” | Subagent | 多步但有界；父 agent 仍应负责综合 |
| “收集账户详情，然后转给退款专员。” | 交接 | 顺序的、有状态的对话，并解锁相应能力 |
| “同时搜索航班、酒店和景点。” | 并行 subagent 或 router | 相互独立的工作可以并发运行 |
| “在用户明确确认后删除生产资源。” | 交接加审批门 | 所有权与风险必须明确 |

两条实用规则：

1. 如果专家必须与用户对话若干轮，优先用交接。
2. 如果父 agent 应继续充当协调者，只需要拿回一个结果，优先用 subagent。

### 6.2 所有权才是真正的契约

当所有权明确时，subagent 很有用。当它们变成一个含糊的群聊时，就变得危险。

好的交接契约：

```python
from dataclasses import dataclass


@dataclass
class HandoffSpec:
    target_agent: str
    reason: str
    allowed_tools: list[str]
    max_steps: int
    expected_output_schema: dict


def choose_handoff(task: str) -> HandoffSpec | None:
    if "PCB" in task or "schematic" in task:
        return HandoffSpec(
            target_agent="hardware_reviewer",
            reason="Needs hardware design review",
            allowed_tools=["read_repo", "search_datasheets"],
            max_steps=6,
            expected_output_schema={
                "type": "object",
                "properties": {
                    "findings": {"type": "array"},
                    "risk_level": {"type": "string"},
                },
                "required": ["findings", "risk_level"],
            },
        )
    return None
```

差的交接契约：

```text
Ask another agent to think about this and see what it says.
```

差的版本没有所有者、没有权限、没有停止条件，也没有可验证的输出。


<details>
<summary>English original</summary>

**6. Handoffs and Subagents**

The most common design mistake in **multi-agent systems** is using one word, "agent," for **three different things**.

| Pattern | Who owns the user conversation after delegation? | Context model | Best for |
|---------|-----------------------------------------------|---------------|----------|
| Agent as tool | Parent keeps ownership | Isolated, request/response only | Stateless specialist capability |
| Subagent | Parent keeps ownership | Usually filtered or summarized context | Complex bounded sub-problem |
| Handoff | Ownership moves to another agent/state | Shared state across turns | Multi-stage conversational flow |

Plain language:

- **Agent as tool** means "do this one expert function and return."
- **Subagent** means "take this bounded mission, work on it, and come back with a result."
- **Handoff** means "you now own the next part of the conversation."

If you do not **define ownership explicitly**, you will create duplicate work, token bloat, or dead-end flows where no agent knows who should answer the user.

**6.1 Choosing the right pattern**

Use this decision table first. It prevents most over-engineered agent systems.

| If the task looks like this | Use | Why |
|-----------------------------|-----|-----|
| "Generate SQL for this schema." | Agent as tool | Atomic, reusable, strict input/output |
| "Research three vendors and compare them." | Subagent | Multi-step but bounded; parent should still synthesize |
| "Collect account details, then transfer to refund specialist." | Handoff | Sequential stateful conversation with capability unlocking |
| "Search flights, hotels, and attractions at the same time." | Parallel subagents or router | Independent work can run concurrently |
| "Delete production resources after explicit user confirmation." | Handoff plus approval gate | Ownership and risk must be explicit |

Two practical rules:

1. If the specialist must talk to the user for several turns, prefer a handoff.
2. If the parent should remain the coordinator and only needs a result back, prefer a subagent.

**6.2 Ownership is the real contract**

Subagents are useful when ownership is explicit. They become dangerous when they turn into a vague group chat.

Good handoff contract:

```python
from dataclasses import dataclass


@dataclass
class HandoffSpec:
    target_agent: str
    reason: str
    allowed_tools: list[str]
    max_steps: int
    expected_output_schema: dict


def choose_handoff(task: str) -> HandoffSpec | None:
    if "PCB" in task or "schematic" in task:
        return HandoffSpec(
            target_agent="hardware_reviewer",
            reason="Needs hardware design review",
            allowed_tools=["read_repo", "search_datasheets"],
            max_steps=6,
            expected_output_schema={
                "type": "object",
                "properties": {
                    "findings": {"type": "array"},
                    "risk_level": {"type": "string"},
                },
                "required": ["findings", "risk_level"],
            },
        )
    return None
```

Bad handoff contract:

```text
Ask another agent to think about this and see what it says.
```

The bad version has no owner, no permissions, no stopping condition, and no verifiable output.

</details>

### 6.3 上下文胶囊优于全量历史转储

第二种主要失效模式是 **上下文管理**。不要把所有对话都塞进每一次 subagent 调用。

改用经过过滤的上下文胶囊：

```python
from dataclasses import dataclass


@dataclass
class ContextCapsule:
    user_goal: str
    relevant_facts: list[str]
    constraints: list[str]
    accepted_decisions: list[str]
    allowed_tools: list[str]
    expected_output_schema: dict
    max_steps: int = 6
    trace_id: str = ""


@dataclass
class SubagentSpec:
    name: str
    mission: str
    read_only: bool = True
    can_run_in_parallel: bool = True


def build_capsule(task: str) -> ContextCapsule:
    return ContextCapsule(
        user_goal=task,
        relevant_facts=[
            "Board target: Jetson Orin Nano carrier",
            "Constraint: no BOM changes this sprint",
        ],
        constraints=[
            "Do not edit unrelated files",
            "Use only read-only inspection tools",
        ],
        accepted_decisions=[
            "Use UART for first RCP bring-up",
        ],
        allowed_tools=["read_repo", "search_datasheets"],
        expected_output_schema={
            "type": "object",
            "properties": {
                "findings": {"type": "array"},
                "recommended_action": {"type": "string"},
            },
            "required": ["findings", "recommended_action"],
        },
    )
```

胶囊里通常应该放什么：

- 用一句话写清用户目标。
- 只放与该专家相关的信息。
- 不可协商的约束。
- 已经接受的决策，避免 agent 重新翻出已定案的问题。
- 工具权限。
- 输出 schema 与预算。

通常应该排除什么：

- 原始完整聊天历史。
- 内部思维链。
- 无关的工具 trace。
- 父 agent 碰过的所有文件。

### 6.4 顺序交接 vs 并行 subagent

从控制流的角度看，交接与 subagent 不可互换。

| 问题 | 交接 | Subagent |
|----------|---------|----------|
| 它能否拥有下一轮用户对话？ | 能 | 不能，通常由父 agent 继续 |
| 它天然是顺序的吗？ | 是 | 有时是，但往往可以并行 |
| 它是否需要共享的会话状态？ | 通常需要 | 通常不需要；传过滤后的上下文 |
| 集中式编排是否保留？ | 较弱 | 保留 |

当能力按顺序解锁时，用 **顺序交接**：

```text
triage -> collect details -> eligibility check -> refund specialist
```

当各工作流之间互不依赖时，用 **并行 subagent**：

```text
research agent
security reviewer
cost estimator
        -> parent synthesizer
```

并行 subagent 往往比一个巨型通用 agent 更便宜，因为每个工作单元只看到自己需要的上下文。当每个工作单元都需要同一份大型共享会话状态，并且必须直接与用户对话时，它们就是糟糕的选择。

### 6.5 真正有效的安全模式

subagent 也可以用作信任边界。

好的用法：

- 用于处理不可信网页内容的只读研究 subagent。
- 在执行前检查 planner 输出的 verifier subagent。
- 拦截高风险工具请求的红队或策略 subagent。
- 只在显式批准后才激活的财务或生产写入 agent。

坏的用法：

- 给每个 subagent 都开 shell 权限，"以防万一"。
- 让 reviewer subagent 在只需查看时直接改写源文件。
- 把密钥传给只需要摘要的 agent。

委派边界应当收窄权限，而不是放宽权限。

### 6.6 实现检查清单

在添加交接或 subagent 之前，先回答这六个问题：

1. 谁负责下一条面向用户的回复？
2. 究竟传了哪些上下文？
3. 允许使用哪些工具？
4. 停止条件是什么？
5. 结果必须是什么形态？
6. 被委派方失败或超时会发生什么？

如果答不上来，说明设计还没准备好。

### 6.7 失效模式

| 失效 | 表现 | 修复 |
|---------|--------------------|-----|
| 归属缺口 | 两个 agent 互相等待，或都去回答用户 | 为每一步指定唯一的回复负责人 |
| 上下文膨胀 | 每个 subagent 都拿到完整记录 | 传胶囊或摘要，而不是原始历史 |
| 交接无闭环 | 工具调用发生了，但历史格式不正确 | 记录交接对或等价的转换产物 |
| 过度派生 | 一次简单查询就创建五个 agent | 先用一个工具或单个 agent |
| 隐藏副作用 | 委派既路由又修改数据 | 把路由工具与写入工具分开 |
| 验证缺口 | planner 输出未经审查就执行 | 在执行前加入 reviewer 或审批关卡 |


<details>
<summary>English original</summary>

**6.3 Context capsules beat full history dumps**

The second major failure mode is **context management**. Do not dump the full conversation into every subagent call.

Use a filtered context capsule instead:

```python
from dataclasses import dataclass


@dataclass
class ContextCapsule:
    user_goal: str
    relevant_facts: list[str]
    constraints: list[str]
    accepted_decisions: list[str]
    allowed_tools: list[str]
    expected_output_schema: dict
    max_steps: int = 6
    trace_id: str = ""


@dataclass
class SubagentSpec:
    name: str
    mission: str
    read_only: bool = True
    can_run_in_parallel: bool = True


def build_capsule(task: str) -> ContextCapsule:
    return ContextCapsule(
        user_goal=task,
        relevant_facts=[
            "Board target: Jetson Orin Nano carrier",
            "Constraint: no BOM changes this sprint",
        ],
        constraints=[
            "Do not edit unrelated files",
            "Use only read-only inspection tools",
        ],
        accepted_decisions=[
            "Use UART for first RCP bring-up",
        ],
        allowed_tools=["read_repo", "search_datasheets"],
        expected_output_schema={
            "type": "object",
            "properties": {
                "findings": {"type": "array"},
                "recommended_action": {"type": "string"},
            },
            "required": ["findings", "recommended_action"],
        },
    )
```

What should usually go into a capsule:

- The user goal in one sentence.
- Only the facts relevant to this specialist.
- Non-negotiable constraints.
- Already accepted decisions, so agents do not reopen settled issues.
- Tool permissions.
- Output schema and budget.

What should usually stay out:

- Raw full chat history.
- Internal chain-of-thought.
- Unrelated tool traces.
- Every file ever touched by the parent.

**6.4 Sequential handoffs vs parallel subagents**

Handoffs and subagents are not interchangeable from a control-flow standpoint.

| Question | Handoff | Subagent |
|----------|---------|----------|
| Can it own the next user turn? | Yes | No, parent usually resumes |
| Is it naturally sequential? | Yes | Sometimes, but can often be parallel |
| Does it need shared conversational state? | Often yes | Usually no; pass filtered context |
| Is centralized orchestration preserved? | Less so | Yes |

Use **sequential handoffs** when capabilities unlock in order:

```text
triage -> collect details -> eligibility check -> refund specialist
```

Use **parallel subagents** when work streams do not depend on each other:

```text
research agent
security reviewer
cost estimator
        -> parent synthesizer
```

Parallel subagents are often cheaper than one giant generalist agent because each worker sees only the context it needs. They are a bad choice when every worker needs the same large shared conversational state and must keep talking to the user directly.

**6.5 Safety patterns that actually help**

Subagents are also useful as trust boundaries.

Good uses:

- A read-only research subagent for untrusted web content.
- A verifier subagent that checks a planner's output before execution.
- A red-team or policy subagent that blocks risky tool requests.
- A financial or production-write agent that only activates after explicit approval.

Bad uses:

- Giving every subagent shell access "just in case."
- Letting a reviewer subagent rewrite source files directly when it only needs to inspect.
- Passing secrets to agents that only need summaries.

The delegation boundary should narrow permissions, not widen them.

**6.6 Implementation checklist**

Before you add a handoff or subagent, answer these six questions:

1. Who owns the next user-facing response?
2. What exact context is being passed?
3. Which tools are allowed?
4. What is the stopping condition?
5. What shape must the result have?
6. What happens if the delegate fails or times out?

If you cannot answer those, the design is not ready.

**6.7 Failure modes**

| Failure | What it looks like | Fix |
|---------|--------------------|-----|
| Ownership gap | Both agents wait for each other or both answer the user | Define a single response owner per step |
| Context bloat | Every subagent gets the full transcript | Pass a capsule or summary, not raw history |
| Handoff without closure | Tool call happens but history is malformed | Record the handoff pair or equivalent transition artifact |
| Over-spawning | Five agents created for a simple lookup | Start with a tool or a single agent |
| Hidden side effects | Delegate both routes and mutates data | Separate routing tools from write tools |
| Verification gap | Planner output executes without review | Add a reviewer or approval gate before action |

</details>

### 6.8 设计规则

使用能保持正确性的最小委派机制：

- 从工具开始。
- 当工作是多步骤或领域专精时，改用 subagent。
- 当对话的所有权必须跨轮次变更时，改用 handoff。

---

## 7. 流式输出不只是 token

对生产环境的 agent，要流式输出 **runtime 事件**，而不只是生成的文本。

有用的事件类型：

| Event | Why it helps |
|-------|--------------|
| `model_start` | 显示当前生效的模型与策略配置 |
| `model_delta` | 流式输出生成的文本 |
| `tool_call` | 让隐藏的动作请求可见 |
| `tool_result` | 展示执行之后发生了什么 |
| `handoff` | 记录哪个 agent 取得了所有权 |
| `policy_block` | 解释某个动作为何被拒绝 |
| `done` | 给出最终输出与用量汇总 |

这让系统更易调试、运行更安全。如果 UI 只流式输出文字，用户就看不到 agent 何时调用工具或变更所有权。

---

## 8. 护栏应位于 Prompt 之外

Prompt 指令有帮助，但它们**不是安全边界**。**runtime 控制**必须位于 LLM 之外。

最小控制集：

| 控制 | 示例 |
|---------|---------|
| 输入校验 | 拒绝不支持的文件类型或过大的 prompt |
| 工具允许清单 | 财务 agent 可以读取发票，但不能执行 shell 命令 |
| 身份绑定 | 工具调用以请求用户的身份执行，而非全局管理员 |
| 人工审批 | 删除文件、发送消息或购买商品时暂停以等待复核 |
| 输出校验 | JSON schema、引用检查、PII 扫描、不安全输出过滤 |
| 审计日志 | 谁发起请求、用了什么上下文、运行了哪些工具、触发了哪条策略 |

这与现代 agent 安全指南的主要结论一致：风险出现在执行期间，尤其是当工具、权限、内存与外部数据参与其中时。

---

## 9. SDK vs Graph vs 网关

在设计评审中使用这张决策表。

| 选择 | 适用场景 |
|--------|------|
| 裸 provider API | 想要完全控制，且工作流较短 |
| Agent SDK | 想要托管的工具循环、会话、handoff、护栏和链路追踪 |
| LangGraph 风格的图 | 需要持久化状态、重试、分支、评审门禁和可恢复性 |
| MCP | 想要跨多个 AI host 复用工具/资源/prompt |
| OpenClaw 风格的网关 | 需要持久会话、channel、配对、布线、节点或本地优先运行 |

大多数严肃的产品会使用不止一种。例如：

```text
Web/mobile/voice channels
        |
Gateway: session, auth, routing, audit
        |
Workflow runtime: graph, retries, human review
        |
Agent SDK or owned model loop
        |
MCP/tools: files, shell, browser, database, devices
```

---

## 10. 硬件与系统影响

agent runtime 的选择会影响**基础设施需求**：

- 更多工具循环意味着更多小模型调用，而不只是一次大调用。
- 长会话会增加上下文、KV-cache 压力与成本。
- 流式输出要求低 time-to-first-token 和稳定的尾延迟。
- 网关会产生常驻工作负载，看起来更像服务而非批处理作业。
- 本地优先的助手会提升对边缘推理、音频流水线和设备内存的需求。
- runtime 遥测成为工作负载的一部分，因为每次工具调用和 handoff 都需要记录日志。

对硬件工程师而言，这正是 agent 工作负载与一次性 chatbot 工作负载不同的原因。

---

## 关键要点

1. 现代 agent 开发是 runtime 架构问题，而不只是 prompt 问题。
2. 把 provider 相关的细节留在适配器内；保持产品契约稳定。
3. MCP 标准化了工具、资源与 prompt 的暴露方式，但它不能替代授权。
4. subagent 需要明确的所有权、工具权限、预算和输出 schema。
5. 安全的生产级 agent 需要 runtime 策略与遥测。

---

## 练习

1. 为某个 provider 实现一个 `ModelAdapter`，并让它返回本讲中用到的 `ToolCall` 对象。
2. 添加一个 `requires_approval=True` 工具，并让 runtime 返回 `policy_block` 事件而不是执行它。
3. 为硬件助手设计一份 MCP server 清单。将每个 server 标记为只读、可写、外部或破坏性。
4. 画出一个 OpenClaw 风格助手的架构，它能接收来自 Telegram 的消息、路由到硬件 agent、调用数据手册搜索工具，并返回带引用的答案。

---


<details>
<summary>English original</summary>

**6.8 Design rule**

Use the smallest delegation mechanism that preserves correctness:

- Start with a tool.
- Move to a subagent when the work is multi-step or domain-specialized.
- Move to a handoff when conversational ownership must change across turns.

---

**7. Streaming Is More Than Tokens**

For production agents, stream **runtime events**, not only generated text.

Useful event types:

| Event | Why it helps |
|-------|--------------|
| `model_start` | Shows which model and policy profile is active |
| `model_delta` | Streams generated text |
| `tool_call` | Makes hidden action requests visible |
| `tool_result` | Shows what happened after execution |
| `handoff` | Records which agent took ownership |
| `policy_block` | Explains why an action was denied |
| `done` | Gives final output and usage summary |

This makes the system easier to debug and safer to operate. If the UI only streams words, users cannot see when the agent is calling tools or changing ownership.

---

**8. Guardrails Belong Outside the Prompt**

Prompt instructions help, but they are **not a security boundary**. **Runtime controls** must sit outside the LLM.

Minimum control set:

| Control | Example |
|---------|---------|
| Input validation | Reject unsupported file types or oversized prompts |
| Tool allowlist | A finance agent can read invoices but cannot execute shell commands |
| Identity binding | Tool calls execute as the requesting user, not as a global admin |
| Human approval | Deleting files, sending messages, or purchasing items pauses for review |
| Output validation | JSON schema, citation checks, PII scan, unsafe output filter |
| Audit log | Who asked, what context was used, what tools ran, what policy fired |

This matches the main lesson from modern agent security guidance: risk appears during execution, especially when tools, permissions, memory, and external data are involved.

---

**9. SDK vs Graph vs Gateway**

Use this decision table in design reviews.

| Choose | When |
|--------|------|
| Raw provider API | You want full control and the workflow is short |
| Agent SDK | You want managed tool loops, sessions, handoffs, guardrails, and tracing |
| LangGraph-style graph | You need durable state, retries, branches, review gates, and resumability |
| MCP | You want reusable tools/resources/prompts across multiple AI hosts |
| OpenClaw-style gateway | You need persistent sessions, channels, pairing, routing, nodes, or local-first operation |

Most serious products use more than one. For example:

```text
Web/mobile/voice channels
        |
Gateway: session, auth, routing, audit
        |
Workflow runtime: graph, retries, human review
        |
Agent SDK or owned model loop
        |
MCP/tools: files, shell, browser, database, devices
```

---

**10. Hardware and Systems Implications**

Agent runtime choices affect **infrastructure demand**:

- More tool loops mean more small model calls, not just one large call.
- Long sessions increase context, KV-cache pressure, and cost.
- Streaming requires low time-to-first-token and stable tail latency.
- Gateways create always-on workloads that look more like services than batch jobs.
- Local-first assistants increase demand for edge inference, audio pipelines, and device memory.
- Runtime telemetry becomes part of the workload because every tool call and handoff needs logging.

For hardware engineers, this is why agent workloads are not the same as one-shot chatbot workloads.

---

**Key Takeaways**

1. Modern agent development is a runtime architecture problem, not just a prompt problem.
2. Keep provider-specific details inside adapters; keep your product contract stable.
3. MCP standardizes how tools, resources, and prompts are exposed, but it does not replace authorization.
4. Subagents need explicit ownership, tool permissions, budgets, and output schemas.
5. Runtime policy and telemetry are required for safe production agents.

---

**Exercises**

1. Implement a `ModelAdapter` for one provider and make it return the `ToolCall` objects used in this lecture.
2. Add a `requires_approval=True` tool and make the runtime return a `policy_block` event instead of executing it.
3. Design an MCP server list for a hardware assistant. Mark each server as read-only, write-capable, external, or destructive.
4. Draw an architecture for an OpenClaw-style assistant that can receive a message from Telegram, route to a hardware agent, call a datasheet search tool, and return an answer with citations.

---

</details>

## 参考文献

- [OpenAI Agents SDK](https://openai.github.io/openai-agents-python/)
- [OpenAI API Agents guide](https://platform.openai.com/docs/guides/agents)
- [Model Context Protocol specification](https://modelcontextprotocol.io/specification/2025-11-25)
- [LangGraph overview](https://docs.langchain.com/oss/python/langgraph/overview)
- [LangChain handoffs documentation](https://docs.langchain.com/oss/python/langchain/multi-agent/handoffs)
- [LangChain multi-agent architecture guide](https://www.langchain.com/blog/choosing-the-right-multi-agent-architecture)
- [Google Cloud ADK: sub-agents versus agents as tools](https://cloud.google.com/blog/topics/developers-practitioners/where-to-use-sub-agents-versus-agents-as-tools)
- [OWASP Top 10 for LLM Applications](https://owasp.org/www-project-top-10-for-large-language-model-applications/)
- [NIST AI RMF Generative AI Profile](https://www.nist.gov/publications/artificial-intelligence-risk-management-framework-generative-artificial-intelligence)

---

**Previous:** [Lecture 16](Lecture-16.md) | **Next:** [Lecture 18](Lecture-18.md)


<details>
<summary>English original</summary>

**References**

- [OpenAI Agents SDK](https://openai.github.io/openai-agents-python/)
- [OpenAI API Agents guide](https://platform.openai.com/docs/guides/agents)
- [Model Context Protocol specification](https://modelcontextprotocol.io/specification/2025-11-25)
- [LangGraph overview](https://docs.langchain.com/oss/python/langgraph/overview)
- [LangChain handoffs documentation](https://docs.langchain.com/oss/python/langchain/multi-agent/handoffs)
- [LangChain multi-agent architecture guide](https://www.langchain.com/blog/choosing-the-right-multi-agent-architecture)
- [Google Cloud ADK: sub-agents versus agents as tools](https://cloud.google.com/blog/topics/developers-practitioners/where-to-use-sub-agents-versus-agents-as-tools)
- [OWASP Top 10 for LLM Applications](https://owasp.org/www-project-top-10-for-large-language-model-applications/)
- [NIST AI RMF Generative AI Profile](https://www.nist.gov/publications/artificial-intelligence-risk-management-framework-generative-artificial-intelligence)

---

**Previous:** [Lecture 16](Lecture-16.md) | **Next:** [Lecture 18](Lecture-18.md)

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-17.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-17.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
