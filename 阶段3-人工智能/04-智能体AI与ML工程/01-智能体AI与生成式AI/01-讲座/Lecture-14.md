---
title: 第 14 讲 - 高效本地 RAG 栈：Qwen3.5-4B INT4 与 Granite Embeddings
description: 第 14 讲 - 高效本地 RAG 栈：Qwen3.5-4B INT4 与 Granite Embeddings
---

# 第 14 讲 - 高效本地 RAG 栈：Qwen3.5-4B INT4 与 Granite Embeddings

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [第 13 讲](Lecture-13.md) | **下一讲：** [第 15 讲](Lecture-15.md)

---

**高效本地 RAG** 并不在于使用你能装下的最大模型。

而在于把内存和算力花在**能提升回答质量的地方**。

一个强大的边缘 RAG 栈如下所示：

```text
User query
  -> Granite embedding model
  -> Qdrant or pgvector
  -> top-k retrieval
  -> optional reranker
  -> compact retrieved context
  -> Qwen3.5-4B INT4 generator
  -> grounded answer
```

该架构适用于：

- Jetson Orin
- 本地/私有 AI
- 边缘 agent
- 编程助手
- 多语言 RAG
- 低功耗推理
- 小型办公室知识库
- 工厂或机器人文档助手

核心思想是：

```text
retrieval precision + small fast generator
beats weak retrieval + huge generator
```

大多数 RAG 失效**并非模型规模的失效**。

而是**检索、分块、上下文和内存预算的失效**。

---

## 学习目标

学完本讲，你应能够：

1. 围绕 4B 级生成器和紧凑 embedding 模型设计本地 RAG 栈。
2. 解释为什么 Granite 97M Multilingual R2 对边缘检索有吸引力。
3. 在 Qdrant 和 pgvector 之间选择本地向量搜索方案。
4. 估算 INT4 生成器部署的内存压力。
5. 为代码、文档、PDF 和手册选择分块大小。
6. 解释为什么 reranking 比把大量块塞进 prompt 更重要。
7. 比较 llama.cpp、vLLM 和 TensorRT-LLM 在本地/边缘 RAG 中的表现。
8. 使用 prompt 约束、KV cache 优化、前缀缓存和投机解码来改进小模型 RAG。
9. 避免常见的高效 RAG 失效模式。

---

## 1. 目标架构

推荐的栈：

```text
generator:
  Qwen/Qwen3.5-4B

generator quantization:
  INT4 class quantization
  AWQ / GPTQ / GGUF Q4_K_M depending on runtime

embedding:
  ibm-granite/granite-embedding-97m-multilingual-r2

vector database:
  Qdrant for edge-first service
  pgvector if PostgreSQL integration is already required

retrieval:
  top_k = 8
  rerank to 3
  compress before generation

runtime:
  llama.cpp for embedded/low-RAM
  vLLM for server and batching
  TensorRT-LLM for maximum NVIDIA optimization work
```

这不是唯一有效的栈。

它是一个**强默认选项**，因为它让每个组件都小到足以推理。

目标是：

```text
good retrieval quality
  + low VRAM
  + short prompts
  + fast decode
  + private/local operation
```

---

## 2. 生成器：Qwen3.5-4B

Qwen3.5-4B 是一个可在 Hugging Face 上获取的 **4B 级 Qwen 模型**。

模型卡包含 Transformers、vLLM、SGLang 和 Docker model runner 的用法示例。

对本讲而言，它有趣的原因在于规模/能力的取舍：

- 小到足以进行本地和边缘实验
- 比许多更老的 sub-7B 模型更强
- 适用于编码和多语言任务
- 在检索良好时适合生成有依据的回答

重要提醒：

```text
Always verify the exact model variant, chat template, modality mode,
license, quantization artifact, and serving backend before deployment.
```

Hugging Face 上 `Qwen/Qwen3.5-4B` 的模型卡围绕 image-text-to-text 用途打标，而许多本地 RAG 栈使用纯文本的 chat/completion 路径。

这意味着你应该验证：

- tokenizer 行为
- chat template
- thinking mode 或 reasoning 控制
- vLLM 支持
- 如果使用 GGUF，则验证 llama.cpp/GGUF 支持
- 在目标上下文长度下的内存占用
- 模型在你的文档上的回答质量

**不要假设所有 Qwen3.5-4B 变体行为相同**。

---

## 3. 量化策略

对于边缘 RAG，**INT4 级量化**通常是正确的起点。

常见格式：

| 格式 | 典型 runtime | 备注 |
|---|---|---|
| AWQ | vLLM / TensorRT-LLM | 良好的面向服务器的仅权重量化路径 |
| GPTQ | ExLlama / vLLM | 成熟的本地/服务器量化路径 |
| GGUF Q4_K_M | llama.cpp | 实用的低内存本地部署格式 |
| FP8 | Hopper/Blackwell 级路径 | 在支持的 GPU 上有用，但不是默认的 Jetson 路径 |

Qwen3.5-4B INT4 的规划估算：

| 组件 | 粗略内存 |
|---|---:|
| 权重 | 2.2-2.8 GB |
| KV cache | 0.5-3 GB |
| runtime 开销 | 0.5-1 GB |

典型活动 VRAM 规划范围：

| 上下文 | 粗略 VRAM |
|---|---:|
| 4K | 4-5 GB |
| 8K | 5-7 GB |
| 16K | 8-10 GB |

这些是**规划数字，而非保证**。

在你的实际栈上测量：

```text
model revision
quantization format
backend
batch size
context length
KV dtype
GPU memory allocator
embedding placement
vector DB placement
```

---


<details>
<summary>English original</summary>

**Lecture 14 - Efficient Local RAG Stack: Qwen3.5-4B INT4 and Granite Embeddings**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 13](Lecture-13.md) | **Next:** [Lecture 15](Lecture-15.md)

---

**Efficient local RAG** is not about using the largest model you can fit.

It is about spending memory and compute **where they improve answer quality**.

A strong edge RAG stack looks like:

```text
User query
  -> Granite embedding model
  -> Qdrant or pgvector
  -> top-k retrieval
  -> optional reranker
  -> compact retrieved context
  -> Qwen3.5-4B INT4 generator
  -> grounded answer
```

This architecture is useful for:

- Jetson Orin
- local/private AI
- edge agents
- coding assistants
- multilingual RAG
- low-power inference
- small office knowledge bases
- factory or robotics documentation assistants

The central idea:

```text
retrieval precision + small fast generator
beats weak retrieval + huge generator
```

Most RAG failures are **not model-size failures**.

They are **retrieval, chunking, context, and memory-budget failures**.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Design a local RAG stack around a 4B-class generator and compact embedding model.
2. Explain why Granite 97M Multilingual R2 is attractive for edge retrieval.
3. Choose between Qdrant and pgvector for local vector search.
4. Estimate memory pressure for INT4 generator deployment.
5. Choose chunk sizes for code, docs, PDFs, and manuals.
6. Explain why reranking matters more than dumping many chunks into the prompt.
7. Compare llama.cpp, vLLM, and TensorRT-LLM for local/edge RAG.
8. Use prompt constraints, KV-cache optimization, prefix caching, and speculative decoding to improve small-model RAG.
9. Avoid common efficient-RAG failure modes.

---

**1. Target architecture**

The recommended stack:

```text
generator:
  Qwen/Qwen3.5-4B

generator quantization:
  INT4 class quantization
  AWQ / GPTQ / GGUF Q4_K_M depending on runtime

embedding:
  ibm-granite/granite-embedding-97m-multilingual-r2

vector database:
  Qdrant for edge-first service
  pgvector if PostgreSQL integration is already required

retrieval:
  top_k = 8
  rerank to 3
  compress before generation

runtime:
  llama.cpp for embedded/low-RAM
  vLLM for server and batching
  TensorRT-LLM for maximum NVIDIA optimization work
```

This is not the only valid stack.

It is a **strong default** because it keeps each component small enough to reason about.

The goal is:

```text
good retrieval quality
  + low VRAM
  + short prompts
  + fast decode
  + private/local operation
```

---

**2. Generator: Qwen3.5-4B**

Qwen3.5-4B is a **4B-class Qwen model** available on Hugging Face.

The model card includes examples for Transformers, vLLM, SGLang, and Docker model runner usage.

For this lecture, the reason it is interesting is the size/capability tradeoff:

- small enough for local and edge experiments
- stronger than many older sub-7B models
- useful for coding and multilingual tasks
- suitable for grounded answer generation when retrieval is good

Important caveat:

```text
Always verify the exact model variant, chat template, modality mode,
license, quantization artifact, and serving backend before deployment.
```

The Hugging Face card for `Qwen/Qwen3.5-4B` is tagged around image-text-to-text usage, while many local RAG stacks use text-only chat/completion paths.

That means you should validate:

- tokenizer behavior
- chat template
- thinking mode or reasoning controls
- vLLM support
- llama.cpp/GGUF support if using GGUF
- memory use at your target context length
- answer quality on your documents

Do **not assume all Qwen3.5-4B variants behave identically**.

---

**3. Quantization strategy**

For edge RAG, **INT4-class quantization** is usually the right starting point.

Common formats:

| Format | Typical runtime | Notes |
|---|---|---|
| AWQ | vLLM / TensorRT-LLM | good server-oriented weight-only quantization path |
| GPTQ | ExLlama / vLLM | mature local/server quantization path |
| GGUF Q4_K_M | llama.cpp | practical low-RAM local deployment format |
| FP8 | Hopper/Blackwell-class paths | useful on supported GPUs, not the default Jetson path |

Planning estimates for Qwen3.5-4B INT4:

| Component | Rough memory |
|---|---:|
| weights | 2.2-2.8 GB |
| KV cache | 0.5-3 GB |
| runtime overhead | 0.5-1 GB |

Typical active VRAM planning range:

| Context | Rough VRAM |
|---|---:|
| 4K | 4-5 GB |
| 8K | 5-7 GB |
| 16K | 8-10 GB |

These are **planning numbers, not guarantees**.

Measure on your exact stack:

```text
model revision
quantization format
backend
batch size
context length
KV dtype
GPU memory allocator
embedding placement
vector DB placement
```

---

</details>

## 4. Embedding 模型：Granite 97M Multilingual R2

使用：

```text
ibm-granite/granite-embedding-97m-multilingual-r2
```

IBM 的模型卡将其描述为**97M 参数的稠密 embedding 模型**，具备：

- 384 维 embedding
- 最高 32,768 token 上下文
- 多语言支持
- 代码检索支持
- Apache-2.0 许可
- ONNX 与 OpenVINO 部署路径
- vLLM embedding 推理服务支持
- 面向 llama.cpp 风格 embedding 的 GGUF 转换选项

为什么它很适合边缘场景：

- 比 311M 的 Granite 多语言版本小得多
- 内存压力更低
- 延迟更低
- 更易做批处理
- 在同等规模下多语言检索质量好
- 适合 Jetson 与 CPU 侧 embedding 路径

在更大的服务器部署中，311M 版本可能带来质量提升。

对边缘 RAG（检索增强生成）而言，97M 模型通常是**更好的默认选择**。

---

## 5. 检索比生成器规模更重要

**只要 prompt 中包含正确的证据**，小生成器也能回答得很好。

大生成器**在检索不佳时依然会失败**。

检索不佳会导致：

- 幻觉答案
- 对缺失信息过度自信
- 无关引用
- prompt 过长
- decode（逐 token 生成阶段）慢
- 上下文窗口浪费

可用的准则：

```text
better top-3 evidence
  > bigger model reading 20 weak chunks
```

好的检索流水线：

```text
embed query
  -> vector search top 8
  -> metadata filter
  -> rerank top 8
  -> keep top 3
  -> optionally compress
  -> generate answer
```

**不要把检索到的所有 chunk 都塞进**生成器。

小模型需要**强信号**。

---

## 6. 分块策略

分块的重要性常常**超过模型选型**。

推荐的起始范围：

| 内容类型 | chunk 大小 |
|---|---:|
| code | 256-512 tokens |
| documentation | 512-1024 tokens |
| PDFs/manuals | 768-1536 tokens |

重叠量：

```text
10-20%
```

常见起点：

```text
chunk_size = 512 tokens
overlap = 64 tokens
```

为什么不用超大 chunk？

```text
giant chunks reduce retrieval precision
```

如果每个 chunk 包含的主题过多，向量检索**无法定位到确切的相关段落**。

分块不佳的症状：

- 检索到的 chunk 大体相关，但不包含答案
- 需要很多 chunk 才能回答
- reranker 难以抉择
- 模型看到过多无关上下文
- 引用指向泛泛的章节

使用结构感知的分块：

- 保留标题
- 保留代码块
- 保留函数/类边界
- 包含文件路径元数据
- 包含章节标题元数据
- 包含文档版本元数据

---

## 7. 向量数据库选择

### Qdrant

在以下情况使用 Qdrant：

- 对边缘友好的向量检索
- Rust 实现
- HNSW 稠密向量索引
- payload 元数据过滤
- 独立服务部署
- 混合检索选项
- 面向本地 agent 的简洁 API

在尚不需要 PostgreSQL 时，Qdrant 通常是 Jetson/本地部署更好的默认选择。

### pgvector

在以下情况使用 pgvector：

- PostgreSQL 已是产品的一部分
- SQL join 与关系型元数据很重要
- 希望只用一个运维数据库
- 企业应用集成比纯粹的向量专用部署更重要

这个选择无关理念。

使用：

```text
Qdrant:
  edge-first vector service

pgvector:
  SQL-first application integration
```

---

## 8. 重排

重排常常是**杠杆最大的质量改进**。

向量检索得到的是**候选**。

reranker 挑出**最佳证据**。

推荐模式：

```text
retrieve top 8
rerank top 8
keep top 3
```

小型 reranker 候选：

| Reranker | 适用场景 |
|---|---|
| bge-reranker-base | 通用质量强 |
| Granite reranker | IBM/Granite 生态 |
| Jina reranker | 多语言工作流 |

如果延迟紧张：

- 只对 top 8 或 top 10 做重排
- GPU 留给生成器时，reranker 跑在 CPU 上
- 缓存重复查询的重排结果
- 元数据精确命中时跳过 reranker

如果质量重要：

```text
rerank before increasing generator size
```

---

## 9. 小模型的上下文管理

小模型**对带噪 prompt 敏感**。

推荐的最终 prompt 结构：

```text
system instructions
  -> compact retrieved context
  -> user question
  -> answer format requirements
```

检索上下文的目标：

```text
~2K tokens for many edge RAG tasks
```

不要这样：

```text
retrieve 20 chunks
send everything
hope the model finds the answer
```

要这样：

```text
retrieve 8
rerank to 3
compress
answer with citations
```

压缩可以包括：

- 只抽取包含答案的段落
- 去掉样板文字
- 保留标题与引用
- 保持代码片段完整
- 对重复段落去重

小模型**会奖励自律**。

---


<details>
<summary>English original</summary>

**4. Embedding model: Granite 97M Multilingual R2**

Use:

```text
ibm-granite/granite-embedding-97m-multilingual-r2
```

IBM's model card describes it as a **97M-parameter dense embedding model** with:

- 384-dimensional embeddings
- up to 32,768-token context
- multilingual support
- code retrieval support
- Apache-2.0 license
- ONNX and OpenVINO deployment paths
- vLLM embedding serving support
- GGUF conversion option for llama.cpp-style embedding

Why it is a strong edge fit:

- much smaller than the 311M Granite multilingual variant
- lower memory pressure
- lower latency
- easier batching
- good multilingual retrieval quality for its size
- practical for Jetson and CPU-side embedding paths

The 311M version may improve quality in larger server deployments.

For edge RAG, the 97M model is usually the **better default**.

---

**5. Retrieval matters more than generator size**

A small generator can answer well **if the prompt contains the right evidence**.

A large generator can **still fail if retrieval is poor**.

Bad retrieval causes:

- hallucinated answers
- overconfident missing information
- irrelevant citations
- excessive prompt length
- slow decode
- context window waste

The working rule:

```text
better top-3 evidence
  > bigger model reading 20 weak chunks
```

Good retrieval pipeline:

```text
embed query
  -> vector search top 8
  -> metadata filter
  -> rerank top 8
  -> keep top 3
  -> optionally compress
  -> generate answer
```

Do **not dump all retrieved chunks** into the generator.

Small models need **high signal**.

---

**6. Chunking strategy**

Chunking often matters **more than model selection**.

Recommended starting ranges:

| Content type | Chunk size |
|---|---:|
| code | 256-512 tokens |
| documentation | 512-1024 tokens |
| PDFs/manuals | 768-1536 tokens |

Overlap:

```text
10-20%
```

Common starting point:

```text
chunk_size = 512 tokens
overlap = 64 tokens
```

Why not giant chunks?

```text
giant chunks reduce retrieval precision
```

If every chunk contains too many topics, vector search **cannot identify the exact relevant passage**.

Bad chunking symptoms:

- retrieved chunks are broadly related but not answer-bearing
- answer requires many chunks
- reranker struggles to choose
- model sees too much irrelevant context
- citations point to generic sections

Use structure-aware chunking:

- preserve headings
- preserve code blocks
- preserve function/class boundaries
- include file path metadata
- include section title metadata
- include document version metadata

---

**7. Vector database choice**

**Qdrant**

Use Qdrant when you want:

- edge-friendly vector search
- Rust implementation
- HNSW dense vector index
- payload metadata filtering
- standalone service deployment
- hybrid search options
- clean API for local agents

Qdrant is usually the better Jetson/local default when you do not already need PostgreSQL.

**pgvector**

Use pgvector when:

- PostgreSQL is already part of the product
- SQL joins and relational metadata matter
- you want one operational database
- enterprise app integration matters more than raw vector-specialized deployment

The choice is not ideological.

Use:

```text
Qdrant:
  edge-first vector service

pgvector:
  SQL-first application integration
```

---

**8. Reranking**

Reranking is often the **highest-leverage quality improvement**.

Vector search gets **candidates**.

The reranker picks the **best evidence**.

Recommended pattern:

```text
retrieve top 8
rerank top 8
keep top 3
```

Small reranker candidates:

| Reranker | Good fit |
|---|---|
| bge-reranker-base | strong general quality |
| Granite reranker | IBM/Granite ecosystem |
| Jina reranker | multilingual workflows |

If latency is tight:

- rerank only top 8 or top 10
- run reranker on CPU if GPU is reserved for generator
- cache rerank results for repeated queries
- skip reranker for exact metadata hits

If quality matters:

```text
rerank before increasing generator size
```

---

**9. Context management for small models**

Small models are **sensitive to noisy prompts**.

Recommended final prompt structure:

```text
system instructions
  -> compact retrieved context
  -> user question
  -> answer format requirements
```

Target retrieved context:

```text
~2K tokens for many edge RAG tasks
```

Instead of:

```text
retrieve 20 chunks
send everything
hope the model finds the answer
```

Do:

```text
retrieve 8
rerank to 3
compress
answer with citations
```

Compression can be:

- extract only answer-bearing paragraphs
- remove boilerplate
- preserve headings and citations
- keep code snippets intact
- deduplicate repeated passages

Small models **reward discipline**.

---

</details>

## 10. 提示词设计

使用**严格有据的提示词**。

示例：

```text
You are a retrieval-grounded assistant.
Answer only from the retrieved context.
If the context does not contain enough evidence, say "insufficient information."
Include citations using the provided source IDs.
Do not use outside knowledge unless explicitly asked.
```

这样做的收益：

- 减少幻觉
- 强制表达不确定性
- 改善引用行为
- 防止过度作答
- 让失败更容易被发现

用于代码 RAG：

```text
Use only the provided repository snippets.
If a function or file is not present in context, say which file is missing.
Do not invent APIs.
```

用于多语言 RAG：

```text
Answer in the user's language unless the task requests otherwise.
Preserve technical identifiers exactly.
```

---

## 11. Runtime 选择

### llama.cpp

适用场景：

- Jetson 或嵌入式部署
- 内存小
- GGUF 量化
- CPU/GPU 混合执行
- 简单的本地服务器
- 离线/私有部署

最适合：

```text
Qwen3.5-4B Q4_K_M
4K-8K context
single-user local RAG
```

### vLLM

适用场景：

- 服务器部署
- 连续批处理
- 多用户
- OpenAI 兼容 API
- 支持 embedding 端点
- 更高并发下的模型推理服务

最适合：

```text
Qwen3.5-4B AWQ/GPTQ
Granite embedding endpoint
multi-user local server
```

### TensorRT-LLM

适用场景：

- NVIDIA 专属的极致性能
- 生产环境 CUDA 优化
- Tensor Core 路径至关重要
- 偏静态的部署配置
- kernel 调优值得其复杂度

最适合：

```text
Orin optimization work
L4/Hopper/Blackwell server optimization
latency-sensitive production inference
```

runtime 的选择**会改变整个系统**。

**定型之前先做 benchmark。**

---

## 12. 面向 Jetson 的部署

不错的嵌入式默认方案：

```text
generator:
  Qwen3.5-4B GGUF Q4_K_M

embedding:
  Granite 97M Multilingual R2
  ONNX/OpenVINO/Transformers depending on hardware path

vector DB:
  Qdrant

context:
  4K-8K

retrieval:
  top_k = 8
  rerank = 3

runtime:
  llama.cpp or carefully tested vLLM path
```

预期的活跃内存规划：

```text
5-7 GB active VRAM for many 4K-8K configurations
```

但在 Jetson 上，还要考虑统一内存压力：

- 模型权重
- KV cache
- embedding 模型
- 向量索引
- 操作系统与桌面服务
- Python runtime
- Qdrant 内存
- 缓冲区与临时张量

在 Jetson 上，**不要把每个组件都跑在 GPU 上**。

通常：

```text
GPU:
  generator

CPU / optimized runtime:
  embedding
  vector search
  reranker if latency allows
```

---

## 13. 面向服务器的部署

不错的小型 GPU 服务器技术栈：

```text
generator:
  Qwen3.5-4B AWQ or GPTQ

embedding:
  Granite 311M if quality matters and memory allows
  Granite 97M if latency/cost matters

runtime:
  vLLM

vector DB:
  Qdrant

optimization:
  FlashInfer backend where applicable
  continuous batching
  prefix caching
  KV-cache optimization
```

适用条件：

- 用户多
- 并发的本地 agent
- 需要 OpenAI 兼容端点
- 更大的上下文窗口
- 多个模型在路由之后对外推理服务

服务器技术栈应度量：

- 吞吐
- p95 延迟
- TTFT
- ITL
- GPU 内存
- 检索延迟
- reranker 延迟
- 提示词 token 数
- 答案正确性

---

## 14. 进阶优化

### KV cache 量化

KV cache 可能**主导长上下文 decode（逐 token 生成阶段）的内存占用**。

可选方案：

- INT8 KV
- 在支持的硬件/后端上使用 FP8 KV
- 分页 KV cache

适用场景：

- 上下文长度增长
- 并发重要
- decode 带宽受限

FP8 KV cache 的取舍参见第 43 讲。

### 前缀缓存

agent 经常复用：

- 系统提示词
- 工具指令
- 安全策略
- RAG 答案格式

当 runtime 支持时，缓存稳定的前缀。

这样可避免反复**重算相同的提示词前缀**。

### 投机解码

使用：

```text
small draft model
  -> larger verifier model
```

对于 4B 的 verifier，若接受率足够高，0.5B-1B 的 draft 可以提升吞吐。

投机解码**并非没有代价**。

度量：

- 接受率
- 额外内存
- 增加的复杂度
- 延迟分布

---

## 15. 评估方案

高效的 RAG 需要**检索与生成两方面的评估**。

度量检索：

- recall@k
- MRR
- nDCG
- 精确来源命中率
- 多语言检索准确率
- 代码符号检索准确率

度量答案质量：

- 有据性
- 引用正确性
- 上下文不足时弃答
- 多语言答案质量
- 代码正确性
- 幻觉率

度量系统性能：

- 查询 embedding 延迟
- 向量检索延迟
- rerank 延迟
- 提示词组装时间
- TTFT
- ITL
- 总响应延迟
- VRAM
- RAM
- 若在 Jetson 上则看瓦数

决策规则：

```text
optimize retrieval before increasing model size
```

---


<details>
<summary>English original</summary>

**10. Prompt design**

Use **strict grounded prompts**.

Example:

```text
You are a retrieval-grounded assistant.
Answer only from the retrieved context.
If the context does not contain enough evidence, say "insufficient information."
Include citations using the provided source IDs.
Do not use outside knowledge unless explicitly asked.
```

Why this helps:

- reduces hallucination
- forces uncertainty
- improves citation behavior
- prevents over-answering
- makes failures easier to detect

For code RAG:

```text
Use only the provided repository snippets.
If a function or file is not present in context, say which file is missing.
Do not invent APIs.
```

For multilingual RAG:

```text
Answer in the user's language unless the task requests otherwise.
Preserve technical identifiers exactly.
```

---

**11. Runtime choices**

**llama.cpp**

Use when:

- Jetson or embedded deployment
- low RAM
- GGUF quantization
- CPU/GPU mixed execution
- simple local server
- offline/private deployment

Best for:

```text
Qwen3.5-4B Q4_K_M
4K-8K context
single-user local RAG
```

**vLLM**

Use when:

- server deployment
- continuous batching
- multiple users
- OpenAI-compatible API
- embedding endpoint support
- model serving at higher concurrency

Best for:

```text
Qwen3.5-4B AWQ/GPTQ
Granite embedding endpoint
multi-user local server
```

**TensorRT-LLM**

Use when:

- NVIDIA-specific maximum performance
- production CUDA optimization
- Tensor Core paths matter
- static-ish deployment configuration
- kernel tuning is worth the complexity

Best for:

```text
Orin optimization work
L4/Hopper/Blackwell server optimization
latency-sensitive production inference
```

The runtime choice **changes the whole system**.

**Benchmark before committing.**

---

**12. Jetson-oriented deployment**

Good embedded default:

```text
generator:
  Qwen3.5-4B GGUF Q4_K_M

embedding:
  Granite 97M Multilingual R2
  ONNX/OpenVINO/Transformers depending on hardware path

vector DB:
  Qdrant

context:
  4K-8K

retrieval:
  top_k = 8
  rerank = 3

runtime:
  llama.cpp or carefully tested vLLM path
```

Expected active memory planning:

```text
5-7 GB active VRAM for many 4K-8K configurations
```

But on Jetson, also consider unified memory pressure:

- model weights
- KV cache
- embedding model
- vector index
- OS and desktop services
- Python runtime
- Qdrant memory
- buffers and temporary tensors

For Jetson, do **not run every component on GPU**.

Often:

```text
GPU:
  generator

CPU / optimized runtime:
  embedding
  vector search
  reranker if latency allows
```

---

**13. Server-oriented deployment**

Good small GPU server stack:

```text
generator:
  Qwen3.5-4B AWQ or GPTQ

embedding:
  Granite 311M if quality matters and memory allows
  Granite 97M if latency/cost matters

runtime:
  vLLM

vector DB:
  Qdrant

optimization:
  FlashInfer backend where applicable
  continuous batching
  prefix caching
  KV-cache optimization
```

Good when:

- many users
- concurrent local agents
- OpenAI-compatible endpoint desired
- larger context windows
- multiple models served behind routing

The server stack should measure:

- throughput
- p95 latency
- TTFT
- ITL
- GPU memory
- retrieval latency
- reranker latency
- prompt token count
- answer correctness

---

**14. Advanced optimization**

**KV-cache quantization**

KV cache can **dominate long-context decode memory**.

Options:

- INT8 KV
- FP8 KV on supported hardware/backend
- paged KV cache

Use when:

- context length grows
- concurrency matters
- decode is memory-bound

Connect to Lecture 43 for FP8 KV-cache tradeoffs.

**Prefix caching**

Agents often reuse:

- system prompt
- tool instructions
- safety policy
- RAG answer format

Cache stable prefixes when runtime supports it.

This avoids **recomputing the same prompt prefix** repeatedly.

**Speculative decoding**

Use:

```text
small draft model
  -> larger verifier model
```

For a 4B verifier, a 0.5B-1B draft can improve throughput if acceptance rate is high.

Speculative decoding is **not free**.

Measure:

- acceptance rate
- extra memory
- added complexity
- latency distribution

---

**15. Evaluation plan**

Efficient RAG needs **both retrieval and generation evals**.

Measure retrieval:

- recall@k
- MRR
- nDCG
- exact source hit rate
- multilingual retrieval accuracy
- code symbol retrieval accuracy

Measure answer quality:

- groundedness
- citation correctness
- abstention when context is insufficient
- multilingual answer quality
- code correctness
- hallucination rate

Measure systems performance:

- query embedding latency
- vector search latency
- rerank latency
- prompt assembly time
- TTFT
- ITL
- total response latency
- VRAM
- RAM
- watts if on Jetson

The decision rule:

```text
optimize retrieval before increasing model size
```

---

</details>

## 16. 最大的错误

避免：

- 超大分块
- 检索到的分块过多
- 无 reranking
- 超长 prompt
- 处处 FP16
- 无缓存
- 无元数据过滤
- 无引用检查
- 无弃答行为
- 只评测最终答案
- 忽略检索指标
- 在 GPU 上同时跑 embedding、vector DB、reranker 和 generator，却不测量压力

**最佳工程洞见**：

```text
efficient RAG is mostly memory bandwidth and context-quality engineering,
not raw parameter count
```

胜出的系统优化的是：

- 检索精度
- KV cache
- prompt 大小
- 分块质量
- 批处理
- token 效率
- 缓存复用
- 元数据过滤

---

## 迷你实验：设计一个 Jetson RAG 技术栈

为 Jetson Orin 16GB 上的技术文档设计一套本地 RAG 系统。

填写以下内容：

```text
Generator:
Quantization:
Context length:
Embedding model:
Embedding runtime:
Vector DB:
Chunk size:
Overlap:
Top-K:
Rerank strategy:
Prompt budget:
Backend:
Expected VRAM:
Expected RAM:
Latency target:
Evaluation set:
Failure threshold:
```

然后写出一条决策：

```text
Use Qwen3.5-4B INT4 because:
Use Granite 97M because:
Use Qdrant because:
Need reranking because:
Do not increase context beyond:
First optimization to try:
First metric to monitor:
```

---

## 要点

- 高效的本地 RAG 是检索质量工程加上内存纪律。
- Qwen3.5-4B INT4 是一个可行的小型生成器目标，但确切的变体/后端行为必须验证。
- Granite 97M Multilingual R2 是强力的边缘 embedding 模型，因为它紧凑、多语言且面向检索。
- Qdrant 通常是好的边缘优先向量数据库；当 PostgreSQL 集成占主导时，pgvector 更合适。
- 分块与 reranking 对质量的提升，往往大于增大生成器规模。
- 小模型需要紧凑、高信号的检索上下文，以及严格有据的 prompt。
- llama.cpp 在嵌入式 GGUF 部署上很强；vLLM 在服务端批处理上很强；TensorRT-LLM 用于更深入的 NVIDIA 优化。
- KV cache 优化、前缀缓存和投机解码可以提升本地 agent 的吞吐。
- 在宣称该技术栈「高效」之前，先测量检索、答案质量、延迟、VRAM、RAM 和功耗。

---

## 参考文献

- Qwen/Qwen3.5-4B 模型卡：[https://huggingface.co/Qwen/Qwen3.5-4B](https://huggingface.co/Qwen/Qwen3.5-4B)
- Granite 97M Multilingual R2 模型卡：[https://huggingface.co/ibm-granite/granite-embedding-97m-multilingual-r2](https://huggingface.co/ibm-granite/granite-embedding-97m-multilingual-r2)
- IBM Granite Embedding 文档：[https://www.ibm.com/us-en/granite/docs/models/embedding](https://www.ibm.com/us-en/granite/docs/models/embedding)
- Qdrant 索引文档：[https://qdrant.tech/documentation/manage-data/indexing/](https://qdrant.tech/documentation/manage-data/indexing/)
- llama.cpp：[https://github.com/ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp)
- vLLM 文档：[https://docs.vllm.ai](https://docs.vllm.ai)
- TensorRT-LLM：[https://github.com/NVIDIA/TensorRT-LLM](https://github.com/NVIDIA/TensorRT-LLM)
- vLLM 中的 FP8 KV cache → [MLSys Deep Dives · Lecture 02](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/Lecture-02.md)
- MLSys 2026 Kernel 大赛 → [MLSys Deep Dives · Lecture 06](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/Lecture-06.md)

---

*下一篇：[Lecture 15](Lecture-15.md)*


<details>
<summary>English original</summary>

**16. Biggest mistakes**

Avoid:

- giant chunks
- too many retrieved chunks
- no reranking
- huge prompts
- FP16 everywhere
- no caching
- no metadata filtering
- no citation checks
- no abstention behavior
- evaluating only final answers
- ignoring retrieval metrics
- running embedding, vector DB, reranker, and generator all on the GPU without measuring pressure

The **best engineering insight**:

```text
efficient RAG is mostly memory bandwidth and context-quality engineering,
not raw parameter count
```

The winning systems optimize:

- retrieval precision
- KV cache
- prompt size
- chunk quality
- batching
- token efficiency
- cache reuse
- metadata filtering

---

**Mini-lab: design a Jetson RAG stack**

Design a local RAG system for technical documentation on Jetson Orin 16GB.

Fill this out:

```text
Generator:
Quantization:
Context length:
Embedding model:
Embedding runtime:
Vector DB:
Chunk size:
Overlap:
Top-K:
Rerank strategy:
Prompt budget:
Backend:
Expected VRAM:
Expected RAM:
Latency target:
Evaluation set:
Failure threshold:
```

Then write a decision:

```text
Use Qwen3.5-4B INT4 because:
Use Granite 97M because:
Use Qdrant because:
Need reranking because:
Do not increase context beyond:
First optimization to try:
First metric to monitor:
```

---

**Key takeaways**

- Efficient local RAG is retrieval-quality engineering plus memory discipline.
- Qwen3.5-4B INT4 is a plausible small generator target, but exact variant/backend behavior must be validated.
- Granite 97M Multilingual R2 is a strong edge embedding model because it is compact, multilingual, and retrieval-oriented.
- Qdrant is usually a good edge-first vector database; pgvector is better when PostgreSQL integration dominates.
- Chunking and reranking often improve quality more than increasing generator size.
- Small models need compact, high-signal retrieved context and strict grounded prompts.
- llama.cpp is strong for embedded GGUF deployment; vLLM is strong for server batching; TensorRT-LLM is for deeper NVIDIA optimization.
- KV-cache optimization, prefix caching, and speculative decoding can improve local agent throughput.
- Measure retrieval, answer quality, latency, VRAM, RAM, and power before declaring the stack "efficient."

---

**References**

- Qwen/Qwen3.5-4B model card: [https://huggingface.co/Qwen/Qwen3.5-4B](https://huggingface.co/Qwen/Qwen3.5-4B)
- Granite 97M Multilingual R2 model card: [https://huggingface.co/ibm-granite/granite-embedding-97m-multilingual-r2](https://huggingface.co/ibm-granite/granite-embedding-97m-multilingual-r2)
- IBM Granite Embedding docs: [https://www.ibm.com/us-en/granite/docs/models/embedding](https://www.ibm.com/us-en/granite/docs/models/embedding)
- Qdrant indexing documentation: [https://qdrant.tech/documentation/manage-data/indexing/](https://qdrant.tech/documentation/manage-data/indexing/)
- llama.cpp: [https://github.com/ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp)
- vLLM documentation: [https://docs.vllm.ai](https://docs.vllm.ai)
- TensorRT-LLM: [https://github.com/NVIDIA/TensorRT-LLM](https://github.com/NVIDIA/TensorRT-LLM)
- FP8 KV-Cache in vLLM → [MLSys Deep Dives · Lecture 02](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/Lecture-02.md)
- MLSys 2026 Kernel Contest → [MLSys Deep Dives · Lecture 06](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/Lecture-06.md)

---

*Next: [Lecture 15](Lecture-15.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-14.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-14.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
