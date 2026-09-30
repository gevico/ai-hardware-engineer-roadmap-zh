---
title: 第 08 讲 — 工具使用与函数调用
description: 第 08 讲 — 工具使用与函数调用
---

# 第 08 讲 — 工具使用与函数调用

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [第 07 讲](Lecture-07.md) | **下一讲：** [第 09 讲](Lecture-09.md)

---

## 学习目标

- 用精确的 schema 定义工具，以最大限度减少幻觉
- 正确实现工具使用循环（每个 agent 的核心）
- 优雅地处理工具错误
- 使用并行工具调用降低延迟
- 理解危险工具的安全边界

---

## 1. 什么是工具使用？

**工具使用**让大语言模型调用外部函数——搜索网页、运行代码、查询数据库、控制浏览器。模型并不执行代码；它输出一个**结构化的 JSON 调用**，由你的应用来执行。

```
User message
     ↓
  [LLM] → stop_reason: "tool_use" → tool call JSON
     ↓
Your code executes the tool
     ↓
Tool result sent back to LLM as "tool" role message
     ↓
  [LLM] → stop_reason: "end_turn" → final answer
```

---

## 2. 定义工具

**工具 schema** 是最需要做对的事。含糊的描述会导致错误的调用；错误的输入类型会导致解析错误。

```python
import anthropic

tools = [
    {
        "name": "get_gpu_specs",
        "description": (
            "Retrieve detailed specifications for an NVIDIA or AMD GPU by model name. "
            "Returns memory, bandwidth, compute, and TDP. "
            "Use this when the user asks about specific GPU hardware capabilities."
        ),
        "input_schema": {
            "type": "object",
            "properties": {
                "model": {
                    "type": "string",
                    "description": "GPU model name, e.g. 'H100 SXM5', 'A100 PCIe 80GB', 'RX 7900 XTX'"
                },
                "metric": {
                    "type": "string",
                    "enum": ["memory", "bandwidth", "compute", "tdp", "all"],
                    "description": "Which specification to retrieve. Use 'all' if unsure."
                }
            },
            "required": ["model"]
        }
    },
    {
        "name": "run_cuda_profiler",
        "description": (
            "Run Nsight Systems profiler on a CUDA kernel file and return performance metrics. "
            "Use this to identify bottlenecks: memory bound vs compute bound."
        ),
        "input_schema": {
            "type": "object",
            "properties": {
                "kernel_path": {
                    "type": "string",
                    "description": "Absolute path to the .cu file"
                },
                "num_iterations": {
                    "type": "integer",
                    "description": "Number of profiling iterations (default: 100)",
                    "default": 100
                }
            },
            "required": ["kernel_path"]
        }
    }
]
```

**编写 schema 的规则：**

| 规则 | 原因 |
|------|-----|
| 描述要说明*何时*调用，而不只是*做什么* | 模型据此决定是否调用 |
| 对受限的字符串取值使用 `enum` | 消除拼写错误和无效输入 |
| 用 `default` 标记真正可选的字段，不要放进 `required` | 避免不必要的调用 |
| 保持参数名简短清晰 | 模型会原样生成参数名 |

---


<details>
<summary>English original</summary>

**Lecture 08 — Tool Use & Function Calling**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 07](Lecture-07.md) | **Next:** [Lecture 09](Lecture-09.md)

---

**Learning Objectives**

- Define tools with precise schemas that minimize hallucination
- Implement the tool-use loop correctly (the core of every agent)
- Handle tool errors gracefully
- Use parallel tool calls to reduce latency
- Understand safety boundaries for dangerous tools

---

**1. What Is Tool Use?**

**Tool use** lets the LLM call external functions — search the web, run code, query a database, control a browser. The model doesn't execute code; it outputs a **structured JSON call** that your application executes.

```
User message
     ↓
  [LLM] → stop_reason: "tool_use" → tool call JSON
     ↓
Your code executes the tool
     ↓
Tool result sent back to LLM as "tool" role message
     ↓
  [LLM] → stop_reason: "end_turn" → final answer
```

---

**2. Defining Tools**

**Tool schemas** are the most important thing to get right. Vague descriptions cause wrong calls; wrong input types cause parse errors.

```python
import anthropic

tools = [
    {
        "name": "get_gpu_specs",
        "description": (
            "Retrieve detailed specifications for an NVIDIA or AMD GPU by model name. "
            "Returns memory, bandwidth, compute, and TDP. "
            "Use this when the user asks about specific GPU hardware capabilities."
        ),
        "input_schema": {
            "type": "object",
            "properties": {
                "model": {
                    "type": "string",
                    "description": "GPU model name, e.g. 'H100 SXM5', 'A100 PCIe 80GB', 'RX 7900 XTX'"
                },
                "metric": {
                    "type": "string",
                    "enum": ["memory", "bandwidth", "compute", "tdp", "all"],
                    "description": "Which specification to retrieve. Use 'all' if unsure."
                }
            },
            "required": ["model"]
        }
    },
    {
        "name": "run_cuda_profiler",
        "description": (
            "Run Nsight Systems profiler on a CUDA kernel file and return performance metrics. "
            "Use this to identify bottlenecks: memory bound vs compute bound."
        ),
        "input_schema": {
            "type": "object",
            "properties": {
                "kernel_path": {
                    "type": "string",
                    "description": "Absolute path to the .cu file"
                },
                "num_iterations": {
                    "type": "integer",
                    "description": "Number of profiling iterations (default: 100)",
                    "default": 100
                }
            },
            "required": ["kernel_path"]
        }
    }
]
```

**Schema writing rules:**

| Rule | Why |
|------|-----|
| Description explains *when* to call, not just *what* it does | Model decides whether to call based on this |
| Use `enum` for constrained string values | Eliminates typos and invalid inputs |
| Mark truly optional fields with `default`, don't put in `required` | Prevents unnecessary calls |
| Keep parameter names short and clear | Model generates parameter names verbatim |

---

</details>

## 3. 工具使用循环

```python
import anthropic
import json
from typing import Any

client = anthropic.Anthropic()

# Mock tool implementations
def get_gpu_specs(model: str, metric: str = "all") -> dict:
    specs_db = {
        "H100 SXM5": {"memory": "80GB HBM3", "bandwidth": "3.35 TB/s",
                       "compute": "989 TFLOPS FP16", "tdp": "700W"},
        "A100 PCIe 80GB": {"memory": "80GB HBM2e", "bandwidth": "1.935 TB/s",
                            "compute": "312 TFLOPS FP16", "tdp": "300W"},
    }
    data = specs_db.get(model, {"error": f"GPU '{model}' not found in database"})
    if metric == "all" or "error" in data:
        return data
    return {metric: data.get(metric, "unknown")}

def run_cuda_profiler(kernel_path: str, num_iterations: int = 100) -> dict:
    # In production, actually run nsys/ncu here
    return {
        "kernel": kernel_path,
        "avg_duration_ms": 2.34,
        "memory_throughput_pct": 78.5,
        "compute_throughput_pct": 31.2,
        "bottleneck": "memory_bound",
        "recommendation": "Improve memory access coalescing"
    }

TOOL_MAP = {
    "get_gpu_specs": get_gpu_specs,
    "run_cuda_profiler": run_cuda_profiler,
}

def run_agent(user_message: str) -> str:
    """Core agent loop with tool use."""
    messages = [{"role": "user", "content": user_message}]

    while True:
        response = client.messages.create(
            model="your-agent-model-id",
            max_tokens=2048,
            tools=tools,
            messages=messages
        )

        # Append assistant response to message history
        messages.append({"role": "assistant", "content": response.content})

        if response.stop_reason == "end_turn":
            # Extract final text response
            for block in response.content:
                if hasattr(block, "text"):
                    return block.text

        elif response.stop_reason == "tool_use":
            # Process all tool calls in this response
            tool_results = []

            for block in response.content:
                if block.type == "tool_use":
                    tool_name = block.name
                    tool_input = block.input

                    # Execute the tool
                    if tool_name in TOOL_MAP:
                        result = TOOL_MAP[tool_name](**tool_input)
                        tool_results.append({
                            "type": "tool_result",
                            "tool_use_id": block.id,
                            "content": json.dumps(result)
                        })
                    else:
                        tool_results.append({
                            "type": "tool_result",
                            "tool_use_id": block.id,
                            "content": f"Error: tool '{tool_name}' not found",
                            "is_error": True
                        })

            # Send tool results back to model
            messages.append({"role": "user", "content": tool_results})

        else:
            break  # Unexpected stop reason

    return "Agent completed without response."


# Test it
answer = run_agent(
    "Compare the H100 SXM5 and A100 PCIe 80GB memory bandwidth. "
    "Which is better for memory-bound kernels?"
)
print(answer)
```

---

## 4. 并行工具调用

当需要多个相互独立的工具时，Claude 可以**同时**调用它们——减少**往返次数**。

```python
# Claude may return multiple tool_use blocks in one response
# Your loop must handle ALL of them before sending results back

for block in response.content:
    if block.type == "tool_use":
        # This may execute multiple times per response
        result = TOOL_MAP[block.name](**block.input)
        tool_results.append({
            "type": "tool_result",
            "tool_use_id": block.id,
            "content": json.dumps(result)
        })

# Send ALL results in one message — critical!
messages.append({"role": "user", "content": tool_results})
```

**用 Python `asyncio` 实现真正的并行执行：**

```python
import asyncio
import anthropic

async def execute_tool_async(tool_name: str, tool_input: dict, tool_id: str) -> dict:
    # Run tool in thread pool (for blocking I/O tools)
    loop = asyncio.get_event_loop()
    result = await loop.run_in_executor(None, lambda: TOOL_MAP[tool_name](**tool_input))
    return {
        "type": "tool_result",
        "tool_use_id": tool_id,
        "content": json.dumps(result)
    }

async def process_tool_calls(tool_blocks: list) -> list:
    tasks = [
        execute_tool_async(b.name, b.input, b.id)
        for b in tool_blocks if b.type == "tool_use"
    ]
    return await asyncio.gather(*tasks)
```

> **Pro tip：** 对 I/O 密集的工具（HTTP 请求、数据库查询），并行异步执行可将多工具延迟降低 3–5×。

---

## 5. 错误处理

agent 必须**优雅地处理工具失败** —— 只要给出好的错误消息，LLM 就能恢复。

```python
def safe_tool_call(tool_name: str, tool_input: dict, tool_id: str) -> dict:
    """Execute a tool with error handling."""
    try:
        result = TOOL_MAP[tool_name](**tool_input)
        return {
            "type": "tool_result",
            "tool_use_id": tool_id,
            "content": json.dumps(result)
        }
    except KeyError as e:
        return {
            "type": "tool_result",
            "tool_use_id": tool_id,
            "content": f"Missing required parameter: {e}",
            "is_error": True
        }
    except Exception as e:
        return {
            "type": "tool_result",
            "tool_use_id": tool_id,
            "content": f"Tool execution failed: {type(e).__name__}: {e}",
            "is_error": True
        }
```

**当设置了 `is_error: True` 时**，Claude 通常会：
1. 确认错误
2. 尝试不同的方法（不同的 parameter、不同的工具）
3. 卡住时向用户请求澄清

---

## 6. 工具安全模式

某些工具是**危险的**（删除文件、发送邮件、执行 shell 命令）。要加上**确认门控**。

```python
DANGEROUS_TOOLS = {"delete_file", "send_email", "execute_shell", "git_push"}

def run_agent_with_confirmation(user_message: str) -> str:
    messages = [{"role": "user", "content": user_message}]

    while True:
        response = client.messages.create(
            model="your-agent-model-id",
            max_tokens=2048,
            tools=all_tools,
            messages=messages
        )

        messages.append({"role": "assistant", "content": response.content})

        if response.stop_reason == "end_turn":
            return next(b.text for b in response.content if hasattr(b, "text"))

        elif response.stop_reason == "tool_use":
            tool_results = []

            for block in response.content:
                if block.type != "tool_use":
                    continue

                # Human-in-the-loop for dangerous operations
                if block.name in DANGEROUS_TOOLS:
                    print(f"\n⚠️  Agent wants to call: {block.name}")
                    print(f"   Input: {json.dumps(block.input, indent=2)}")
                    confirm = input("   Allow? (y/n): ").strip().lower()

                    if confirm != "y":
                        tool_results.append({
                            "type": "tool_result",
                            "tool_use_id": block.id,
                            "content": "User denied this action.",
                            "is_error": True
                        })
                        continue

                result = safe_tool_call(block.name, block.input, block.id)
                tool_results.append(result)

            messages.append({"role": "user", "content": tool_results})
```

---

## 7. 配合 `tool_choice` 使用工具

强制或限制模型使用哪些工具：

```python
# Force the model to use a specific tool
response = client.messages.create(
    model="your-agent-model-id",
    max_tokens=512,
    tools=tools,
    tool_choice={"type": "tool", "name": "get_gpu_specs"},  # must call this
    messages=[{"role": "user", "content": "Tell me about the H100."}]
)

# Allow any tool (default)
tool_choice={"type": "auto"}

# Prevent any tool use
tool_choice={"type": "none"}
```

---

## 关键要点

1. 工具循环：调用 API → 检查 `stop_reason` → 执行工具 → 追加结果 → 重复
2. 在一条消息里发送**所有**工具结果 —— 绝不拆分
3. 对失败使用 `is_error: True`；模型会尝试恢复
4. 并行工具调用是自动的 —— 你的代码必须能处理单次响应中的多个块
5. 用 human-in-the-loop 确认来门控危险操作
6. `tool_choice` 强制或阻止工具使用 —— 对结构化抽取工作流很有用

---

## 练习

1. 构建一个天气 + 计算器双工具 agent。用一个需要在单次响应中同时用到两个工具的查询来测试它。
2. 为 `safe_tool_call` 加上重试逻辑 —— 遇到网络错误时以指数退避最多重试 3 次。
3. 实现一个 `max_tool_calls` 安全限制，在 N 次工具调用后停止 agent，防止无限循环。

---

**Previous:** [Lecture 07](Lecture-07.md) | **Next:** [Lecture 09](Lecture-09.md)


<details>
<summary>English original</summary>

**5. Error Handling**

Agents must **handle tool failures gracefully** — the LLM can recover if you give it good error messages.

```python
def safe_tool_call(tool_name: str, tool_input: dict, tool_id: str) -> dict:
    """Execute a tool with error handling."""
    try:
        result = TOOL_MAP[tool_name](**tool_input)
        return {
            "type": "tool_result",
            "tool_use_id": tool_id,
            "content": json.dumps(result)
        }
    except KeyError as e:
        return {
            "type": "tool_result",
            "tool_use_id": tool_id,
            "content": f"Missing required parameter: {e}",
            "is_error": True
        }
    except Exception as e:
        return {
            "type": "tool_result",
            "tool_use_id": tool_id,
            "content": f"Tool execution failed: {type(e).__name__}: {e}",
            "is_error": True
        }
```

**When `is_error: True` is set**, Claude will typically:
1. Acknowledge the error
2. Try a different approach (different parameters, different tool)
3. Ask the user for clarification if stuck

---

**6. Tool Safety Patterns**

Some tools are **dangerous** (delete files, send emails, execute shell commands). Add **confirmation gates**.

```python
DANGEROUS_TOOLS = {"delete_file", "send_email", "execute_shell", "git_push"}

def run_agent_with_confirmation(user_message: str) -> str:
    messages = [{"role": "user", "content": user_message}]

    while True:
        response = client.messages.create(
            model="your-agent-model-id",
            max_tokens=2048,
            tools=all_tools,
            messages=messages
        )

        messages.append({"role": "assistant", "content": response.content})

        if response.stop_reason == "end_turn":
            return next(b.text for b in response.content if hasattr(b, "text"))

        elif response.stop_reason == "tool_use":
            tool_results = []

            for block in response.content:
                if block.type != "tool_use":
                    continue

                # Human-in-the-loop for dangerous operations
                if block.name in DANGEROUS_TOOLS:
                    print(f"\n⚠️  Agent wants to call: {block.name}")
                    print(f"   Input: {json.dumps(block.input, indent=2)}")
                    confirm = input("   Allow? (y/n): ").strip().lower()

                    if confirm != "y":
                        tool_results.append({
                            "type": "tool_result",
                            "tool_use_id": block.id,
                            "content": "User denied this action.",
                            "is_error": True
                        })
                        continue

                result = safe_tool_call(block.name, block.input, block.id)
                tool_results.append(result)

            messages.append({"role": "user", "content": tool_results})
```

---

**7. Tool Use with `tool_choice`**

Force or restrict which tools the model uses:

```python
# Force the model to use a specific tool
response = client.messages.create(
    model="your-agent-model-id",
    max_tokens=512,
    tools=tools,
    tool_choice={"type": "tool", "name": "get_gpu_specs"},  # must call this
    messages=[{"role": "user", "content": "Tell me about the H100."}]
)

# Allow any tool (default)
tool_choice={"type": "auto"}

# Prevent any tool use
tool_choice={"type": "none"}
```

---

**Key Takeaways**

1. The tool loop: call API → check `stop_reason` → execute tools → append results → repeat
2. Send **all** tool results in one message — never split them
3. Use `is_error: True` for failures; the model will try to recover
4. Parallel tool calls are automatic — your code must handle multiple blocks per response
5. Gate dangerous operations with human-in-the-loop confirmation
6. `tool_choice` forces or prevents tool use — useful for structured extraction workflows

---

**Exercises**

1. Build a weather + calculator dual-tool agent. Test it with a query that requires both tools in one response.
2. Add retry logic to `safe_tool_call` — retry up to 3 times with exponential backoff on network errors.
3. Implement a `max_tool_calls` safety limit that stops the agent after N tool calls to prevent infinite loops.

---

**Previous:** [Lecture 07](Lecture-07.md) | **Next:** [Lecture 09](Lecture-09.md)

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-08.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-08.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
