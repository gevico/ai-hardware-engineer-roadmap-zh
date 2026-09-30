---
title: 第 06 讲 - 从零构建大语言模型：面向 agent 与 GPU 工程师的模型机制
description: 第 06 讲 - 从零构建大语言模型：面向 agent 与 GPU 工程师的模型机制
---

# 第 06 讲 - 从零构建大语言模型：面向 agent 与 GPU 工程师的模型机制

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [第 05 讲](Lecture-05.md) | **下一讲：** [第 07 讲](Lecture-07.md)

---

**Agent 工程师**通常工作在模型之上：

```text
prompt
  -> tool calls
  -> memory
  -> Gateway
  -> runtime
  -> UI
```

**GPU 与系统工程师**需要理解 API 之下发生了什么：

```text
tokens
  -> embeddings
  -> attention
  -> MLP
  -> logits
  -> sampling
```

`angelos-p/llm-from-scratch` 仓库很有用，因为它把问题简化成 **工作坊规模的 GPT**。该项目逐步讲解编写 tokenizer、Transformer 模型、训练循环和生成代码，然后在笔记本电脑级机器上训练一个小型莎士比亚风格模型。

本路线图的关键经验：

```text
If you understand the model loop, agent-runtime bottlenecks stop looking mysterious.
```

---

## 学习目标

到本讲结束时，你应该能够：

1. 解释一个小型 GPT 训练流水线包含什么。
2. 理解为什么 tokenization 会改变整个工作负载的形态。
3. 将 Transformer 块映射到 GPU kernel 与内存搬运。
4. 区分训练时成本与推理时成本。
5. 用实用术语解释 prefill（首字前的整段计算）、decode（逐 token 生成阶段）、KV cache、logits 和采样。
6. 将模型内部机制与 agent 系统行为联系起来：延迟、上下文增长、流式输出和批处理。
7. 用从零构建的大语言模型工作坊作为从 agent 工程到 GPU/kernel 工程的桥梁。

---

## 1. 为什么这属于 agent 课程

大多数 **agent 失败** 并非由 attention 数学引起。

它们由 **harness（agent 运行时框架）、工具、记忆、策略和产品问题** 引起。

尽管如此，模型机制仍然重要，因为 agent 会创建 **不寻常的推理工作负载**：

- 长上下文窗口
- 许多短回合
- 工具调用中断
- 流式 token
- 重试与修复循环
- 多个子 agent
- 后台 cron 运行
- 本地/边缘部署

如果不理解 API 之下的模型，就会 **误诊性能**。

示例：

| 症状 | 模型层解释 |
|---|---|
| 第一个 token 很慢 | 对完整 prompt/上下文做 prefill 开销很大 |
| 后续 token 稳定流式输出 | decode 复用 KV cache 并一次生成一个 token |
| 长会话变慢 | attention 和 KV 内存随上下文增长 |
| 批处理推理服务有助于吞吐 | 多个请求更高效地共享 GPU 工作 |
| 工具密集型 agent 感觉突发 | 执行在 CPU/IO 工具与 GPU 推理之间交替 |

Agent 系统是 **runtime 系统**，但其 runtime 行为由 **Transformer 推理** 塑造。

---

## 2. 参考仓库构建了什么

参考项目是一个动手工作坊，题为 **Train Your Own LLM From Scratch**。

它针对 **小型 GPT 风格模型**，而非生产规模的前沿模型。

该项目让学习者编写：

- 字符级 tokenizer
- Transformer 模型架构
- 训练循环
- 文本生成与采样
- 真实数据上的实验

仓库描述了三种工作坊模型规模：

| 配置 | 约参数 | 层数 | 头数 | Embedding 维度 | 示例训练时间 |
|---|---:|---:|---:|---:|---|
| Tiny | ~0.5M | 2 | 2 | 128 | 分钟 |
| Small | ~4M | 4 | 4 | 256 | 数十分钟 |
| Medium | ~10M | 6 | 6 | 384 | 在 M3 Pro 级机器上不到一小时 |

这个规模是刻意保持小的。

这正是重点。

你可以看到 **每个组件**，而不会有分布式训练、tokenizer 复杂性或集群基础设施掩盖基础知识。

---

## 3. 流水线概览

最小 GPT 流水线：

```text
raw text
  -> tokenizer
  -> token ids
  -> token embedding + position embedding
  -> repeated transformer blocks
  -> final layer norm
  -> linear projection to logits
  -> loss during training
  -> sampling during inference
```

训练路径：

```text
token batch
  -> forward pass
  -> logits
  -> cross-entropy loss
  -> backward pass
  -> optimizer step
```

推理路径：

```text
prompt tokens
  -> forward pass
  -> next-token logits
  -> sample/select token
  -> append token
  -> repeat
```

两条路径使用同一个模型。

工作负载不同。

训练在批上做前向和反向传播。

推理通常做一次 prefill，然后重复 decode。

---


<details>
<summary>English original</summary>

**Lecture 06 - LLM From Scratch: Model Mechanics for Agent and GPU Engineers**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 05](Lecture-05.md) | **Next:** [Lecture 07](Lecture-07.md)

---

**Agent engineers** usually work above the model:

```text
prompt
  -> tool calls
  -> memory
  -> Gateway
  -> runtime
  -> UI
```

**GPU and systems engineers** need to understand what happens below the API:

```text
tokens
  -> embeddings
  -> attention
  -> MLP
  -> logits
  -> sampling
```

The `angelos-p/llm-from-scratch` repository is useful because it strips the problem down to a **workshop-sized GPT**. The project walks through writing a tokenizer, transformer model, training loop, and generation code, then trains a small Shakespeare-style model on a laptop-class machine.

The key lesson for this roadmap:

```text
If you understand the model loop, agent-runtime bottlenecks stop looking mysterious.
```

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Explain what a small GPT training pipeline contains.
2. Understand why tokenization changes the shape of the whole workload.
3. Map transformer blocks to GPU kernels and memory movement.
4. Distinguish training-time cost from inference-time cost.
5. Explain prefill, decode, KV cache, logits, and sampling in practical terms.
6. Connect model internals to agent system behavior: latency, context growth, streaming, and batching.
7. Use an from-scratch LLM workshop as a bridge from agent engineering to GPU/kernel engineering.

---

**1. Why this belongs in an agent course**

Most **agent failures** are not caused by attention math.

They are caused by **harness, tool, memory, policy, and product issues**.

Still, model mechanics matter because agents create **unusual inference workloads**:

- long context windows
- many short turns
- tool-call interruptions
- streaming tokens
- retries and repair loops
- multiple subagents
- background cron runs
- local/edge deployment

If you do not understand the model under the API, you will **misdiagnose performance**.

Examples:

| Symptom | Model-level explanation |
|---|---|
| first token is slow | prefill over the full prompt/context is expensive |
| later tokens stream steadily | decode reuses KV cache and generates one token at a time |
| long sessions get slower | attention and KV memory grow with context |
| batch serving helps throughput | multiple requests share GPU work more efficiently |
| tool-heavy agents feel bursty | execution alternates between CPU/IO tools and GPU inference |

Agent systems are **runtime systems**, but their runtime behavior is shaped by **transformer inference**.

---

**2. What the reference repo builds**

The reference project is a hands-on workshop titled **Train Your Own LLM From Scratch**.

It targets a **small GPT-style model**, not a production-scale frontier model.

The project has learners write:

- character-level tokenizer
- transformer model architecture
- training loop
- text generation and sampling
- experiments on real data

The repository describes three workshop model sizes:

| Config | Approx params | Layers | Heads | Embedding dim | Example train time |
|---|---:|---:|---:|---:|---|
| Tiny | ~0.5M | 2 | 2 | 128 | minutes |
| Small | ~4M | 4 | 4 | 256 | tens of minutes |
| Medium | ~10M | 6 | 6 | 384 | under an hour on an M3 Pro-class machine |

This scale is intentionally small.

That is the point.

You can see **every component** without distributed training, tokenizer complexity, or cluster infrastructure hiding the basics.

---

**3. Pipeline overview**

A minimal GPT pipeline:

```text
raw text
  -> tokenizer
  -> token ids
  -> token embedding + position embedding
  -> repeated transformer blocks
  -> final layer norm
  -> linear projection to logits
  -> loss during training
  -> sampling during inference
```

Training path:

```text
token batch
  -> forward pass
  -> logits
  -> cross-entropy loss
  -> backward pass
  -> optimizer step
```

Inference path:

```text
prompt tokens
  -> forward pass
  -> next-token logits
  -> sample/select token
  -> append token
  -> repeat
```

The same model is used in both paths.

The workload is different.

Training does forward and backward over batches.

Inference usually does prefill once and decode repeatedly.

---

</details>

## 4. Tokenization 不是细节

该 workshop 用**字符级 tokenization** 处理 Shakespeare。

为什么？

因为数据集很小。

GPT-2 风格的 **BPE 词表**约有 50k 个 token。在很小的数据集上，很多 token 模式对小模型来说**太罕见**，学不到有用的结构。

字符级 tokenization 给出一个很小的词表：

```text
vocab_size ≈ tens of characters
```

取舍：

| tokenizer | 收益 | 代价 |
|---|---|---|
| 字符级 | 简单，适合小数据，易于检查 | 序列更长 |
| BPE/subword | 序列更短，接近生产环境 | 需要更大的数据和更多机制 |

硬件含义：

```text
tokenizer choice changes sequence length,
sequence length changes attention cost,
attention cost changes memory and latency.
```

对 agent 系统而言，tokenization 影响：

- 提示词大小
- 上下文窗口占用
- 检索 chunk 大小
- 成本核算
- KV cache 内存
- 延迟

---

## 5. Transformer block 剖析

一个基础 GPT block：

```text
x
  -> LayerNorm
  -> self-attention
  -> residual add
  -> LayerNorm
  -> MLP / feed-forward
  -> residual add
```

关键组件：

| 组件 | 职责 |
|---|---|
| token embedding | 把 token ID 映射为向量 |
| position embedding | 告诉模型 token 在序列中的位置 |
| Q/K/V 投影 | 为 attention 生成 query、key、value 向量 |
| attention 分数 | 决定哪些更早的 token 重要 |
| softmax | 把分数转成权重 |
| attention 输出 | 按权重混合 value 向量 |
| 多层感知机 | 逐 token 的非线性变换 |
| 残差 | 保留信息并稳定优化 |
| layer norm | 稳定激活值 |

GPU 视角：

```text
linear layers = matrix multiplies
attention = matmul + softmax + matmul
MLP = large matrix multiplies + activation
```

这正是 CUDA/TensorRT/kernel 工程师切入的地方。

---

## 6. 训练循环机制

最小训练循环：

```text
for each step:
  sample batch
  forward model
  compute cross-entropy loss
  zero gradients
  backward loss
  clip gradients if needed
  optimizer step
  update learning rate schedule
  periodically evaluate/generate sample text
```

关键部分：

| 部分 | 为何重要 |
|---|---|
| 批大小 | 影响吞吐和内存 |
| block size | 每个样本的序列长度 |
| 损失 | 告诉模型 next-token 预测错到什么程度 |
| AdamW | Transformer 训练常用的优化器 |
| 梯度裁剪 | 防止更新不稳定 |
| 学习率调度 | 避免收敛不良 |

训练不只是把推理重复一遍。

**反向传播与优化器状态**主导内存占用。

对小型 workshop 模型，这还能应付。

对生产规模的模型，这就变成一个**分布式系统问题**。

---

## 7. 推理与采样

生成循环：

```text
prompt tokens
  -> model
  -> logits for next token
  -> adjust logits with temperature/top-k
  -> sample next token
  -> append token
  -> repeat
```

重要概念：

| 概念 | 含义 |
|---|---|
| logits | 每个可能的下一个 token 的原始分数 |
| temperature | 控制随机性 |
| top-k | 把采样限制在 k 个最强的候选上 |
| autoregressive decoding | 生成的 token 成为下一步的输入 |

对 agent 的含义：

每个 assistant 响应都是一个 **decode 循环**（逐 token 生成阶段）。

**流式输出**不过是把这个循环逐 token 或逐 chunk 暴露出来。

工具调用会中断这个循环：

```text
model emits tool call
  -> runtime executes tool
  -> tool result enters context
  -> model continues
```

这就是为什么 agent 延迟一部分是**模型延迟**，一部分是 **harness/工具延迟**。

---

## 8. Prefill（首字前的整段计算）与 decode

推理有两个重要阶段：

### Prefill

模型处理已有的提示词/上下文：

```text
system prompt + history + retrieved context + user message
```

这一阶段通常**计算密集**，并随上下文长度增长。

### Decode

模型一次生成一个新 token，同时**复用 KV cache**。

这一阶段通常**对内存带宽敏感**。

与 agent runtime 的关联：

| Agent 行为 | 模型层面的影响 |
|---|---|
| 超大的系统提示词 | prefill 更大 |
| 很长的会话历史 | prefill 和 KV cache 更大 |
| 大量检索到的文档 | prefill 更大 |
| 冗长的工具输出 | 上下文膨胀 |
| 简洁的上下文压缩 | prefill 成本更低 |
| 流式响应 | 暴露 decode 阶段 |

这与前面几讲关于上下文卫生、TokenJuice、系统提示词和 agent 技能的讨论直接相关。

---


<details>
<summary>English original</summary>

**4. Tokenization is not a detail**

The workshop uses **character-level tokenization** for Shakespeare.

Why?

Because the dataset is small.

A GPT-2-style **BPE vocabulary** has roughly 50k tokens. On a tiny dataset, many token patterns are **too rare** for a small model to learn useful structure.

Character-level tokenization gives a tiny vocabulary:

```text
vocab_size ≈ tens of characters
```

Tradeoff:

| Tokenizer | Benefit | Cost |
|---|---|---|
| Character-level | simple, works on small data, easy to inspect | longer sequences |
| BPE/subword | shorter sequences, production-like | needs larger data and more machinery |

Hardware implication:

```text
tokenizer choice changes sequence length,
sequence length changes attention cost,
attention cost changes memory and latency.
```

For agent systems, tokenization affects:

- prompt size
- context-window usage
- retrieval chunk size
- cost accounting
- KV-cache memory
- latency

---

**5. Transformer block anatomy**

A basic GPT block:

```text
x
  -> LayerNorm
  -> self-attention
  -> residual add
  -> LayerNorm
  -> MLP / feed-forward
  -> residual add
```

Key components:

| Component | Job |
|---|---|
| token embedding | maps token IDs to vectors |
| position embedding | tells model where tokens are in sequence |
| Q/K/V projections | create query, key, value vectors for attention |
| attention scores | decide which earlier tokens matter |
| softmax | turns scores into weights |
| attention output | mixes value vectors according to weights |
| MLP | per-token nonlinear transformation |
| residuals | preserve information and stabilize optimization |
| layer norm | stabilizes activations |

GPU view:

```text
linear layers = matrix multiplies
attention = matmul + softmax + matmul
MLP = large matrix multiplies + activation
```

This is where CUDA/TensorRT/kernel engineers enter.

---

**6. Training loop mechanics**

Minimal training loop:

```text
for each step:
  sample batch
  forward model
  compute cross-entropy loss
  zero gradients
  backward loss
  clip gradients if needed
  optimizer step
  update learning rate schedule
  periodically evaluate/generate sample text
```

Important pieces:

| Piece | Why it matters |
|---|---|
| batch size | affects throughput and memory |
| block size | sequence length per sample |
| loss | tells model how wrong next-token prediction was |
| AdamW | common optimizer for transformer training |
| gradient clipping | prevents unstable updates |
| learning-rate schedule | avoids bad convergence |

Training is not just inference repeated.

**Backward pass and optimizer state** dominate memory.

For a small workshop model, that is manageable.

For production-scale models, it becomes a **distributed systems problem**.

---

**7. Inference and sampling**

Generation loop:

```text
prompt tokens
  -> model
  -> logits for next token
  -> adjust logits with temperature/top-k
  -> sample next token
  -> append token
  -> repeat
```

Important concepts:

| Concept | Meaning |
|---|---|
| logits | raw scores for each possible next token |
| temperature | controls randomness |
| top-k | restricts sampling to the k strongest candidates |
| autoregressive decoding | generated token becomes input for the next step |

Agent implication:

Every assistant response is a **decode loop**.

**Streaming** is just exposing that loop token-by-token or chunk-by-chunk.

Tool use interrupts the loop:

```text
model emits tool call
  -> runtime executes tool
  -> tool result enters context
  -> model continues
```

That is why agent latency is partly **model latency** and partly **harness/tool latency**.

---

**8. Prefill and decode**

Inference has two important phases:

**Prefill**

The model processes the existing prompt/context:

```text
system prompt + history + retrieved context + user message
```

This is usually **compute-heavy** and grows with context length.

**Decode**

The model generates one new token at a time while **reusing KV cache**.

This is often **memory-bandwidth-sensitive**.

Agent runtime connection:

| Agent behavior | Model-level effect |
|---|---|
| huge system prompt | larger prefill |
| long session history | larger prefill and KV cache |
| many retrieved docs | larger prefill |
| verbose tool outputs | context bloat |
| concise context compaction | lower prefill cost |
| streaming response | exposes decode phase |

This directly connects to previous lectures on context hygiene, TokenJuice, system prompts, and agent skills.

---

</details>

## 9. GPU/kernel 级视角

一个从零实现的小模型有助于你把 **Python 代码映射到 GPU 工作**。

常见热路径：

| 模型操作 | kernel 级关注点 |
|---|---|
| embedding 查找 | 内存访问模式 |
| 线性投影 | GEMM（矩阵-矩阵乘）吞吐 |
| attention 分数矩阵乘 | 序列长度伸缩 |
| softmax | 数值稳定性与内存带宽 |
| attention value 矩阵乘 | GEMM 加数据布局 |
| MLP 上/下投影 | 稠密矩阵乘 |
| GELU/ReLU | 逐元素 kernel 融合 |
| layer norm | 归约与内存带宽 |
| logits 投影 | 依赖词表大小的 GEMM |

这就是 Transformer 推理优化聚焦于以下方面的原因：

- 融合 kernel
- FlashAttention 风格的 attention kernel
- KV-cache 布局
- 量化
- 批处理
- 内存带宽
- 张量并行
- 图捕获

工作坊代码不会实现所有这些内容。

它提供理解它们所需的心智地图。

---

## 10. 小模型为何依然有用

不要把 10M 参数的 GPT 当作玩具而轻视。

它是一台 **显微镜**。

小模型让你能够：

- 检查每一个张量形状
- 快速看到损失曲线
- 测试 tokenizer 改动
- 理解采样行为
- 在本地对完整训练循环做性能分析
- 无需集群成本即可做实验

可迁移的内容：

- 架构概念
- 训练循环结构
- 推理循环结构
- 张量形状推理
- 性能直觉

不能直接迁移的内容：

- 分布式训练的复杂度
- 生产环境的 tokenizer/数据流水线
- 大规模优化器状态管理
- 高并发下的推理服务
- 前沿模型的行为

用小模型理解 **机制**。

用大系统理解 **规模化**。

---

## 11. 与 agent 技能及 SDLC 的关联

来自第 21 讲：

```text
skills require evidence
```

来自第 29 讲：

```text
tests and intent are durable assets
```

对于模型相关工作，证据的形态会发生变化：

| 工作项 | 证据 |
|---|---|
| tokenizer 改动 | 词表大小、样本编码/解码、序列长度分布 |
| 模型改动 | 参数数量、张量形状检查、损失曲线 |
| 训练循环改动 | 稳定的损失、梯度范数、评测损失 |
| 生成改动 | 样本输出、temperature/top-k 对比 |
| 性能改动 | tokens/sec、内存占用、性能分析器 trace |

以模型为核心的技能不应把 **"it trains"** 当作足够。

它应当追问：

```text
What changed?
What metric moved?
What got slower?
What got less stable?
What evidence proves the behavior?
```

---

## 12. 小实验：追踪一个 token 穿过模型

使用参考仓库或你自己的最小 GPT 代码。

追踪：

```text
input character
  -> token id
  -> embedding vector
  -> attention block
  -> logits
  -> sampled next token
```

记录：

- token ID
- 各阶段的张量形状
- 参数数量
- 序列长度
- 一个生成样本
- 短跑之后的训练/评测损失

然后回答：

```text
Which operation is most expensive?
Which tensor grows with context length?
Where would KV cache matter?
Where would quantization help?
```

---

## 13. 设计练习：从零实现到推理服务

取工作坊模型，设想把它放在 agent runtime 之后提供推理服务。

设计：

```text
HTTP/WebSocket API
request format
streaming token output
batching strategy
KV-cache ownership
context limit
tool-call interruption model
metrics
failure modes
```

然后对比：

```text
training script
  vs
serving runtime
  vs
agent harness
```

这种分离是 **核心架构教训**。

**训练脚本**产出权重。

**推理服务 runtime** 把权重变成 token。

**harness**（agent 运行时框架）把 token 与工具变成工作。

---

## 关键要点

- 理解 API 之下的模型循环，对 agent 工程师有益。
- 从零实现 GPT 的工作坊讲授 tokenizer、架构、训练与生成，而不隐藏基础。
- tokenization 改变序列长度，进而改变 attention 开销与 context 行为。
- 训练与推理对硬件的压力方式不同。
- prefill（首字前的整段计算）与 decode（逐 token 生成阶段）解释了 agent 延迟的很大一部分。
- GPU 优化直接对应 Transformer 运算：GEMM、softmax、layer norm、KV cache 与内存布局。
- 小模型有用，是因为它让机制变得可见。
- agent 技术栈是分层的：模型机理、推理服务 runtime、harness、工具与产品界面属于彼此不同的关注点。


<details>
<summary>English original</summary>

**9. GPU/kernel-level view**

A small from-scratch model helps you map **Python code to GPU work**.

Common hot paths:

| Model operation | Kernel-level concern |
|---|---|
| embedding lookup | memory access pattern |
| linear projection | GEMM throughput |
| attention score matmul | sequence-length scaling |
| softmax | numerical stability and memory bandwidth |
| attention value matmul | GEMM plus data layout |
| MLP up/down projection | dense matrix multiply |
| GELU/ReLU | elementwise kernel fusion |
| layer norm | reduction and memory bandwidth |
| logits projection | vocab-size-dependent GEMM |

This is why transformer inference optimization focuses on:

- fused kernels
- FlashAttention-style attention kernels
- KV-cache layout
- quantization
- batching
- memory bandwidth
- tensor parallelism
- graph capture

The workshop code will not implement all of those.

It gives you the mental map needed to understand them.

---

**10. Why small models are still useful**

Do not dismiss a 10M parameter GPT as a toy.

It is a **microscope**.

Small models let you:

- inspect every tensor shape
- see loss curves quickly
- test tokenizer changes
- understand sampling behavior
- profile a full training loop locally
- experiment without cluster cost

What transfers:

- architecture concepts
- training loop structure
- inference loop structure
- tensor shape reasoning
- performance intuition

What does not transfer directly:

- distributed training complexity
- production tokenizer/data pipelines
- large-scale optimizer state management
- serving at high concurrency
- frontier-model behavior

Use small models to understand **mechanisms**.

Use large systems to understand **scaling**.

---

**11. Connection to agent skills and SDLC**

From Lecture 21:

```text
skills require evidence
```

From Lecture 29:

```text
tests and intent are durable assets
```

For model work, evidence changes shape:

| Work item | Evidence |
|---|---|
| tokenizer change | vocab size, sample encoding/decoding, sequence length distribution |
| model change | parameter count, tensor shape checks, loss curve |
| training loop change | stable loss, gradient norms, eval loss |
| generation change | sample outputs, temperature/top-k comparison |
| performance change | tokens/sec, memory use, profiler trace |

A model-focused skill should not accept **"it trains"** as enough.

It should ask:

```text
What changed?
What metric moved?
What got slower?
What got less stable?
What evidence proves the behavior?
```

---

**12. Mini-lab: trace one token through the model**

Use the reference repo or your own minimal GPT code.

Trace:

```text
input character
  -> token id
  -> embedding vector
  -> attention block
  -> logits
  -> sampled next token
```

Record:

- token ID
- tensor shapes at each stage
- parameter count
- sequence length
- one generated sample
- training/eval loss after a short run

Then answer:

```text
Which operation is most expensive?
Which tensor grows with context length?
Where would KV cache matter?
Where would quantization help?
```

---

**13. Design exercise: from scratch to serving**

Take the workshop model and imagine serving it behind an agent runtime.

Design:

```text
HTTP/WebSocket API
request format
streaming token output
batching strategy
KV-cache ownership
context limit
tool-call interruption model
metrics
failure modes
```

Then compare:

```text
training script
  vs
serving runtime
  vs
agent harness
```

This separation is the **central architecture lesson**.

The **training script** creates weights.

The **serving runtime** turns weights into tokens.

The **harness** turns tokens and tools into work.

---

**Key takeaways**

- Agent engineers benefit from understanding the model loop underneath the API.
- A from-scratch GPT workshop teaches tokenizer, architecture, training, and generation without hiding the basics.
- Tokenization changes sequence length, which changes attention cost and context behavior.
- Training and inference stress hardware differently.
- Prefill and decode explain much of agent latency.
- GPU optimization maps directly to transformer operations: GEMM, softmax, layer norm, KV cache, and memory layout.
- Small models are useful because they make mechanisms visible.
- The agent stack is layered: model mechanics, serving runtime, harness, tools, and product interface are different concerns.

---

</details>

## 参考文献

- 从零开始训练你自己的大语言模型：[https://github.com/angelos-p/llm-from-scratch](https://github.com/angelos-p/llm-from-scratch)
- nanoGPT：[https://github.com/karpathy/nanoGPT](https://github.com/karpathy/nanoGPT)
- Attention Is All You Need：[https://arxiv.org/abs/1706.03762](https://arxiv.org/abs/1706.03762)
- GPT-2 论文：[https://cdn.openai.com/better-language-models/language_models_are_unsupervised_multitask_learners.pdf](https://cdn.openai.com/better-language-models/language_models_are_unsupervised_multitask_learners.pdf)
- TinyStories：[https://arxiv.org/abs/2305.07759](https://arxiv.org/abs/2305.07759)
- Lecture 05 - LLM Fundamentals：[Lecture-05.md](Lecture-05.md)
- Lecture 28 - Runtime Strategy：[Lecture-28.md](Lecture-28.md)

---

*下一节：[Lecture 07](Lecture-07.md)*


<details>
<summary>English original</summary>

**References**

- Train Your Own LLM From Scratch: [https://github.com/angelos-p/llm-from-scratch](https://github.com/angelos-p/llm-from-scratch)
- nanoGPT: [https://github.com/karpathy/nanoGPT](https://github.com/karpathy/nanoGPT)
- Attention Is All You Need: [https://arxiv.org/abs/1706.03762](https://arxiv.org/abs/1706.03762)
- GPT-2 paper: [https://cdn.openai.com/better-language-models/language_models_are_unsupervised_multitask_learners.pdf](https://cdn.openai.com/better-language-models/language_models_are_unsupervised_multitask_learners.pdf)
- TinyStories: [https://arxiv.org/abs/2305.07759](https://arxiv.org/abs/2305.07759)
- Lecture 05 - LLM Fundamentals: [Lecture-05.md](Lecture-05.md)
- Lecture 28 - Runtime Strategy: [Lecture-28.md](Lecture-28.md)

---

*Next: [Lecture 07](Lecture-07.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-06.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-06.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
