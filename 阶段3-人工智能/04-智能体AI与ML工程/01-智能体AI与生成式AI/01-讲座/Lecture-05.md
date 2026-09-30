---
title: 第 05 讲 — 面向 agent 的大语言模型基础
description: 第 05 讲 — 面向 agent 的大语言模型基础
---

# 第 05 讲 — 面向 agent 的大语言模型基础

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [Lecture 04](Lecture-04.md) | **下一讲：** [Lecture 06](Lecture-06.md)

---

## 学习目标

学完本讲，你将能够：

- 从高层解释 Transformer 推理的工作方式（prefill（首字前的整段计算）vs. decode（逐 token 生成阶段））
- 计算 token 数量与上下文窗口开销
- 为给定的 agent 任务选择合适的模型
- 理解为什么延迟、吞吐和 TTFT 对智能体化的循环很重要

---

## 1. Transformer 如何生成文本

大语言模型只做一件事：给定一个 token 序列，**预测下一个 token**。**agent** 不过是一个不断调用这个函数的循环。

```
Input tokens → [Transformer] → Logits → Sample → Output token
                                                        ↓
                                              Append to context
                                                        ↓
                                              Repeat until stop
```

**推理的两个阶段：**

| 阶段 | 发生什么 | 受限类型 |
|-------|-------------|---------------|
| **Prefill** | 并行处理全部输入 token（矩阵乘） | 计算（FLOP 受限） |
| **Decode** | 一次生成一个 token（自回归） | 内存带宽 |

> **硬件含义：** Prefill 会打满 GPU 算力。Decode 的瓶颈在于你能以多快的速度从 HBM 中流式读取权重。这正是推理加速器（Groq、Etched）关注内存带宽、而不只是关注 FLOPS 的原因。

---

## 2. Token 与上下文窗口

**Token ≠ 单词**。经验法则：**1 token ≈ 0.75 个英文单词**（4 个字符）。

```python
import os
import anthropic

client = anthropic.Anthropic()

# Count tokens before sending
response = client.messages.count_tokens(
    model=os.environ.get("ANTHROPIC_MODEL", "your-model-id"),
    messages=[{"role": "user", "content": "Hello, how are you?"}]
)
print(response.input_tokens)  # → 10
```

**上下文窗口变化很快。按类别思考，而不是记住某一个快照：**

| 类别 | 典型用途 | 工程关注点 |
|----------|-------------|---------------------|
| 小/快的对话模型 | 布线、分类、短摘要 | 低延迟、低成本 |
| 均衡型 agent 模型 | 工具调用、JSON 提取、代码审查 | 结构可靠、推理良好 |
| 长上下文模型 | 代码仓库分析、大文档、多文件任务 | 上下文开销、检索质量、内存压力 |
| 本地/开放权重模型 | 边缘推理、隐私、离线演示 | VRAM、量化、吞吐 |
| Embedding 模型 | RAG（检索增强生成）索引与检索 | 向量维度、召回率、索引开销 |

围绕某个具体的窗口大小设计生产环境 agent 之前，务必在提供方文档中**核实当前的上下文上限**。

**为什么上下文大小对 agent 很重要：**
- 多步推理会快速累积 token
- 工具调用结果会落在上下文里
- 喂给 RAG agent 的长文档必须装得下

---

## 3. 推理参数

```python
import os

response = client.messages.create(
    model=os.environ.get("ANTHROPIC_MODEL", "your-model-id"),
    max_tokens=1024,
    temperature=0.0,    # 0 = deterministic (good for agents/tools)
                        # 1 = creative (good for writing)
    top_p=1.0,
    messages=[{"role": "user", "content": "What is 2+2?"}]
)
```

| 参数 | 作用 | agent 推荐值 |
|-----------|--------|---------------------|
| `temperature` | 采样的随机性 | 工具调用/推理用 0.0–0.3 |
| `top_p` | 核采样截断阈值 | 保持 1.0（让 temperature 起作用） |
| `max_tokens` | 输出硬上限 | 设得宽松些——截断会破坏 JSON |

> **实用技巧：** 对工具调用型 agent，始终使用 `temperature=0` 或接近它的值。函数调用生成中的随机性会导致 JSON 解析错误和不可预期的行为。

---

## 4. 一次 API 调用的结构

```python
import os
import anthropic

client = anthropic.Anthropic()  # reads ANTHROPIC_API_KEY from env

response = client.messages.create(
    model=os.environ.get("ANTHROPIC_MODEL", "your-model-id"),
    max_tokens=2048,
    system="You are a helpful assistant.",       # system prompt
    messages=[
        {"role": "user",    "content": "Tell me about CUDA."},
        {"role": "assistant","content": "CUDA is..."},  # prior turn
        {"role": "user",    "content": "How does it compare to ROCm?"},
    ]
)

print(response.content[0].text)
print(f"Input tokens:  {response.usage.input_tokens}")
print(f"Output tokens: {response.usage.output_tokens}")
print(f"Stop reason:   {response.stop_reason}")  # end_turn | tool_use | max_tokens
```

**agent 相关的 `stop_reason` 取值：**

| 取值 | 含义 |
|-------|---------|
| `end_turn` | 模型自然结束 |
| `tool_use` | 模型想调用工具——你的循环必须处理这种情况 |
| `max_tokens` | 触达上限——调大上限或优雅处理 |

---


<details>
<summary>English original</summary>

**Lecture 05 — LLM Fundamentals for Agents**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 04](Lecture-04.md) | **Next:** [Lecture 06](Lecture-06.md)

---

**Learning Objectives**

By the end of this lecture you will be able to:

- Explain how transformer inference works at a high level (prefill vs. decode)
- Calculate token counts and context window costs
- Choose the right model for a given agent task
- Understand why latency, throughput, and TTFT matter for agentic loops

---

**1. How Transformers Generate Text**

An LLM does one thing: given a sequence of tokens, **predict the next token**. An **agent** is just a loop that keeps calling this function.

```
Input tokens → [Transformer] → Logits → Sample → Output token
                                                        ↓
                                              Append to context
                                                        ↓
                                              Repeat until stop
```

**Two phases of inference:**

| Phase | What happens | Compute bound |
|-------|-------------|---------------|
| **Prefill** | Process all input tokens in parallel (matrix multiply) | Compute (FLOP-bound) |
| **Decode** | Generate one token at a time (autoregressive) | Memory bandwidth |

> **Hardware implication:** Prefill saturates GPU compute. Decode is bottlenecked by how fast you can stream weights from HBM. This is why inference accelerators (Groq, Etched) focus on memory bandwidth, not just FLOPS.

---

**2. Tokens and Context Windows**

**Tokens ≠ words**. Rule of thumb: **1 token ≈ 0.75 English words** (4 characters).

```python
import os
import anthropic

client = anthropic.Anthropic()

# Count tokens before sending
response = client.messages.count_tokens(
    model=os.environ.get("ANTHROPIC_MODEL", "your-model-id"),
    messages=[{"role": "user", "content": "Hello, how are you?"}]
)
print(response.input_tokens)  # → 10
```

**Context windows change quickly. Think in categories instead of memorizing one snapshot:**

| Category | Typical use | Engineering concern |
|----------|-------------|---------------------|
| Small/fast chat model | routing, classification, short summaries | low latency and low cost |
| Balanced agent model | tool use, JSON extraction, code review | reliable structure and good reasoning |
| Long-context model | repo analysis, large documents, multi-file tasks | context cost, retrieval quality, memory pressure |
| Local/open-weight model | edge inference, privacy, offline demos | VRAM, quantization, throughput |
| Embedding model | RAG indexing and retrieval | vector dimension, recall, index cost |

Always **verify current context limits** in the provider documentation before designing a production agent around a specific window size.

**Why context size matters for agents:**
- Multi-step reasoning accumulates tokens fast
- Tool call results land in context
- Long documents fed to RAG agent must fit

---

**3. Inference Parameters**

```python
import os

response = client.messages.create(
    model=os.environ.get("ANTHROPIC_MODEL", "your-model-id"),
    max_tokens=1024,
    temperature=0.0,    # 0 = deterministic (good for agents/tools)
                        # 1 = creative (good for writing)
    top_p=1.0,
    messages=[{"role": "user", "content": "What is 2+2?"}]
)
```

| Parameter | Effect | Agent recommendation |
|-----------|--------|---------------------|
| `temperature` | Randomness of sampling | 0.0–0.3 for tool use / reasoning |
| `top_p` | Nucleus sampling cutoff | Leave at 1.0 (let temperature do the work) |
| `max_tokens` | Hard output limit | Set generously — truncation breaks JSON |

> **Pro tip:** For tool-use agents, always use `temperature=0` or close to it. Randomness in function call generation causes JSON parse errors and unpredictable behavior.

---

**4. The Anatomy of an API Call**

```python
import os
import anthropic

client = anthropic.Anthropic()  # reads ANTHROPIC_API_KEY from env

response = client.messages.create(
    model=os.environ.get("ANTHROPIC_MODEL", "your-model-id"),
    max_tokens=2048,
    system="You are a helpful assistant.",       # system prompt
    messages=[
        {"role": "user",    "content": "Tell me about CUDA."},
        {"role": "assistant","content": "CUDA is..."},  # prior turn
        {"role": "user",    "content": "How does it compare to ROCm?"},
    ]
)

print(response.content[0].text)
print(f"Input tokens:  {response.usage.input_tokens}")
print(f"Output tokens: {response.usage.output_tokens}")
print(f"Stop reason:   {response.stop_reason}")  # end_turn | tool_use | max_tokens
```

**`stop_reason` values for agents:**

| Value | Meaning |
|-------|---------|
| `end_turn` | Model finished naturally |
| `tool_use` | Model wants to call a tool — your loop must handle this |
| `max_tokens` | Hit the limit — increase or handle gracefully |

---

</details>

## 5. agent 任务的模型选型

并非每个任务都需要最强的模型。**成本与延迟**会在**多步循环**中不断累积。

```python
# Router pattern: use fast/cheap model for simple steps
def route_model(task_type: str) -> str:
    fast_model = "provider-fast-model"
    balanced_model = "provider-balanced-agent-model"
    reasoning_model = "provider-reasoning-model"

    routing = {
        "classification": fast_model,
        "summarization": fast_model,
        "tool_use": balanced_model,
        "complex_reasoning": reasoning_model,
        "coding": balanced_model,
    }
    return routing.get(task_type, balanced_model)
```

| 任务 | 推荐模型类别 | 原因 |
|------|-------------------------|-----|
| 简单问答、布线 | 快速模型 | 低延迟、低成本 |
| 工具调用、JSON 抽取 | 均衡 agent 模型 | 结构化输出可靠 |
| 复杂推理、长上下文 | 推理模型或长上下文模型 | 规划更强、工作集更大 |
| Embedding | embedding 模型 | 专用向量表示 |

---

## 6. 面向响应式 agent 的流式传输

在智能体化 UI 中，**流式传输**能显著提升**感知响应性**。

```python
import os
import anthropic

client = anthropic.Anthropic()

with client.messages.stream(
    model=os.environ.get("ANTHROPIC_MODEL", "your-model-id"),
    max_tokens=1024,
    messages=[{"role": "user", "content": "Explain transformer attention."}]
) as stream:
    for text in stream.text_stream:
        print(text, end="", flush=True)

# Access final message with usage stats
final = stream.get_final_message()
print(f"\nTokens used: {final.usage.input_tokens} in, {final.usage.output_tokens} out")
```

---

## 7. 成本估算

```python
# Rough cost calculator.
# Do not hardcode provider prices in production. Load this from a config file
# maintained from the provider pricing page.
PRICING = {
    "provider-fast-model": {"input": 0.15, "output": 0.60},       # example only, per 1M tokens
    "provider-balanced-agent-model": {"input": 3.00, "output": 15.00},
    "provider-reasoning-model": {"input": 15.00, "output": 75.00},
}

def estimate_cost(model: str, input_tokens: int, output_tokens: int) -> float:
    p = PRICING[model]
    return (input_tokens * p["input"] + output_tokens * p["output"]) / 1_000_000

# A 10-step agent loop with a balanced agent model
steps = 10
per_step_input  = 2000   # context grows each step
per_step_output = 500
total = sum(
    estimate_cost("provider-balanced-agent-model", per_step_input * i, per_step_output)
    for i in range(1, steps + 1)
)
print(f"Estimated loop cost: ${total:.4f}")
```

> **关键洞察：** 在 10 步的 agent 循环中，上下文线性增长 —— 第 10 步的输入 token 是第 1 步的 10 倍。这正是上下文管理（摘要、剪枝）在生产环境中至关重要的原因。

---

## 关键要点

1. LLM 推理 = prefill（首字前的整段计算；算力受限）+ decode（逐 token 生成阶段；内存带宽受限）
2. 工具调用类 agent 使用 `temperature=0`；创意类任务再调高取值
3. 检查 `stop_reason` —— `tool_use` 意味着循环必须调用工具并继续
4. 尽可能把任务路由到更便宜的模型；在多步循环中成本会不断叠加
5. 上下文每一步都在增长 —— 长时间运行的 agent 要规划摘要或窗口化

---

## 练习

1. 编写一个脚本，在把 10 页 PDF 发送给 API 之前统计其 token 数。
2. 构建一个简单的成本记录器，包装 `client.messages.create` 并打印累计成本。
3. 实现一个模型路由器：输入 token 少于 200 的任务用快速模型，其余用均衡 agent 模型。

---

**下一讲：** [Lecture 07 — Prompt Engineering & Structured Output](Lecture-07.md)


<details>
<summary>English original</summary>

**5. Model Selection for Agent Tasks**

Not every task needs the most powerful model. **Cost and latency** add up in **multi-step loops**.

```python
# Router pattern: use fast/cheap model for simple steps
def route_model(task_type: str) -> str:
    fast_model = "provider-fast-model"
    balanced_model = "provider-balanced-agent-model"
    reasoning_model = "provider-reasoning-model"

    routing = {
        "classification": fast_model,
        "summarization": fast_model,
        "tool_use": balanced_model,
        "complex_reasoning": reasoning_model,
        "coding": balanced_model,
    }
    return routing.get(task_type, balanced_model)
```

| Task | Recommended model class | Why |
|------|-------------------------|-----|
| Simple Q&A, routing | fast model | low latency and cost |
| Tool use, JSON extraction | balanced agent model | reliable structured output |
| Complex reasoning, long context | reasoning or long-context model | stronger planning and larger working set |
| Embeddings | embedding model | specialized vector representation |

---

**6. Streaming for Responsive Agents**

In agentic UIs, **streaming** dramatically improves **perceived responsiveness**.

```python
import os
import anthropic

client = anthropic.Anthropic()

with client.messages.stream(
    model=os.environ.get("ANTHROPIC_MODEL", "your-model-id"),
    max_tokens=1024,
    messages=[{"role": "user", "content": "Explain transformer attention."}]
) as stream:
    for text in stream.text_stream:
        print(text, end="", flush=True)

# Access final message with usage stats
final = stream.get_final_message()
print(f"\nTokens used: {final.usage.input_tokens} in, {final.usage.output_tokens} out")
```

---

**7. Cost Estimation**

```python
# Rough cost calculator.
# Do not hardcode provider prices in production. Load this from a config file
# maintained from the provider pricing page.
PRICING = {
    "provider-fast-model": {"input": 0.15, "output": 0.60},       # example only, per 1M tokens
    "provider-balanced-agent-model": {"input": 3.00, "output": 15.00},
    "provider-reasoning-model": {"input": 15.00, "output": 75.00},
}

def estimate_cost(model: str, input_tokens: int, output_tokens: int) -> float:
    p = PRICING[model]
    return (input_tokens * p["input"] + output_tokens * p["output"]) / 1_000_000

# A 10-step agent loop with a balanced agent model
steps = 10
per_step_input  = 2000   # context grows each step
per_step_output = 500
total = sum(
    estimate_cost("provider-balanced-agent-model", per_step_input * i, per_step_output)
    for i in range(1, steps + 1)
)
print(f"Estimated loop cost: ${total:.4f}")
```

> **Key insight:** In a 10-step agent loop, context grows linearly — step 10 has 10× the input tokens of step 1. This is why context management (summarization, pruning) is critical in production.

---

**Key Takeaways**

1. LLM inference = prefill (compute-bound) + decode (memory-bandwidth-bound)
2. Use `temperature=0` for tool-use agents; reserve higher values for creative tasks
3. Check `stop_reason` — `tool_use` means your loop must call the tool and continue
4. Route tasks to cheaper models where possible; the cost compounds in multi-step loops
5. Context grows each step — plan for summarization or windowing in long-running agents

---

**Exercises**

1. Write a script that counts tokens for a 10-page PDF before sending it to the API.
2. Build a simple cost logger that wraps `client.messages.create` and prints cumulative cost.
3. Implement a model router that uses a fast model for tasks under 200 input tokens and a balanced agent model otherwise.

---

**Next:** [Lecture 07 — Prompt Engineering & Structured Output](Lecture-07.md)

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-05.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-05.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
