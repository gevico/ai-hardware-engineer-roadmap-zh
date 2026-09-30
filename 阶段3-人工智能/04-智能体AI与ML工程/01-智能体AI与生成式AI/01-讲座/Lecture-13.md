---
title: 第 13 讲 - Qdrant、pgvector 与 embedding 模型选型
description: 第 13 讲 - Qdrant、pgvector 与 embedding 模型选型
---

# 第 13 讲 - Qdrant、pgvector 与 embedding 模型选型

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [第 12 讲](Lecture-12.md) | **下一讲：** [第 14 讲](Lecture-14.md)

---

Qdrant 和 pgvector 解决的是同一类大问题：

```text
Given a query embedding, find the stored vectors that are most similar.
```

它们**不是同一类系统**。

Qdrant 是**专用的向量搜索引擎**。

pgvector 是**PostgreSQL 扩展**，为 Postgres 加上向量搜索能力。

实践中的准则是：

```text
Use pgvector when SQL integration is the main constraint.
Use Qdrant when retrieval performance and vector-search features are the main constraint.
```

同样的逻辑也适用于 embedding 模型。

**不存在通用的“最佳 embedding 模型”**。

只存在针对以下条件的最佳模型：

- 你的语料
- 你的语言
- 你的 chunk 大小
- 你的延迟预算
- 你的内存预算
- 你的部署目标
- 你的查询分布
- 你的相关性指标

对于本地和边缘 RAG，胜出的设计通常是：

```text
compact embedding model
  + strong chunking
  + good metadata filters
  + hybrid retrieval when needed
  + reranking
  + small grounded generator
```

而不是：

```text
giant embedding model
  + unfiltered top-k
  + huge prompt
  + hope the LLM fixes retrieval mistakes
```

---

## 学习目标

学完本讲后，你应当能够：

1. 解释向量数据库在 RAG 系统中的作用。
2. 解释 Qdrant 与 pgvector 的区别。
3. 描述稠密、稀疏与多向量检索。
4. 从实际系统层面解释 HNSW 与 IVFFlat。
5. 依据架构、规模、过滤与运维来选择 Qdrant 或 pgvector。
6. 将 Granite embeddings 与 BGE、E5、OpenAI、Cohere、Voyage 及其他替代方案做比较。
7. 设计 embedding 评估方案，而不是轻信通用排行榜。
8. 由 embedding 维度与数据类型估算向量存储压力。
9. 规划 embedding 迁移而不破坏检索质量。

---

## 1. RAG 的核心存储问题

RAG 系统有两类数据：

```text
source data:
  docs, markdown, PDFs, code, tickets, emails, manuals

retrieval data:
  chunks, embeddings, metadata, indexes, scores, citations
```

LLM **不直接搜索你的原始文档**。

典型的流水线是：

```text
document
  -> chunk
  -> embed each chunk
  -> store vector + chunk text + metadata
  -> embed user query
  -> search nearest vectors
  -> rerank/filter
  -> send selected evidence to LLM
```

向量存储负责这部分：

```text
stored vectors + metadata + nearest-neighbor index + query API
```

每个存储条目通常是一个“点”或一行：

```json
{
  "id": "doc-17#chunk-03",
  "vector": [0.012, -0.044, 0.331],
  "payload": {
    "document_id": "doc-17",
    "path": "manuals/orin/power.md",
    "section": "Thermals",
    "language": "en",
    "created_at": "2026-05-28",
    "source_hash": "..."
  },
  "text": "The chunk text may live here or in another store."
}
```

**元数据与向量同样重要**。

示例查询：

```text
"How do I reduce Jetson Orin power draw during idle?"
```

好的检索不只是问：

```text
Which chunks are semantically close?
```

它还会问：

```text
Which chunks are semantically close,
inside the right product docs,
in the right version,
in the right language,
from trusted sources,
and recent enough to answer safely?
```

这正是**向量搜索与过滤需要放在一起设计**的原因。

---

## 2. 什么是 Qdrant？

Qdrant 是一个**独立的向量数据库与搜索引擎**。

它的设计以点的集合为中心：

```text
collection
  -> points
      -> id
      -> vector or named vectors
      -> payload metadata
```

关键设计思路：

```text
Qdrant is optimized for vector-native retrieval first.
```

有用的特性：

- 稠密向量搜索
- 稀疏向量搜索
- 命名向量
- 多向量检索模式
- payload 元数据过滤
- payload 索引
- HNSW 索引
- 量化选项
- 通过分片与副本实现水平扩展
- HTTP/gRPC API
- 客户端库
- 本地、云、私有云与边缘部署模式

当检索是**产品关键路径**时，Qdrant 是好选择。

示例：

- 本地 RAG 服务
- 语义搜索 API
- 面向代码仓库的 AI 编码助手
- 推荐系统
- 技术文档上的混合搜索
- 多租户知识检索
- 带本地向量服务的边缘助手

重要的区分在于：

```text
Qdrant is not your relational database.
It is your vector retrieval engine.
```

你仍可以把权威业务数据放在 Postgres、SQLite、对象存储或文档存储中。

在这种设计中，Qdrant 存放：

```text
id + vector + retrieval metadata + optional text snippet
```

源系统存放：

```text
full document + permissions + owner + audit history + business state
```

这种分离是**正常的**。

---


<details>
<summary>English original</summary>

**Lecture 13 - Qdrant, pgvector, and Embedding Model Selection**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 12](Lecture-12.md) | **Next:** [Lecture 14](Lecture-14.md)

---

Qdrant and pgvector solve the same broad problem:

```text
Given a query embedding, find the stored vectors that are most similar.
```

They are **not the same kind of system**.

Qdrant is a **dedicated vector search engine**.

pgvector is a **PostgreSQL extension** that adds vector search to Postgres.

The practical rule:

```text
Use pgvector when SQL integration is the main constraint.
Use Qdrant when retrieval performance and vector-search features are the main constraint.
```

The same logic applies to embedding models.

There is **no universal "best embedding model."**

There is a best model for:

- your corpus
- your languages
- your chunk size
- your latency budget
- your memory budget
- your deployment target
- your query distribution
- your relevance metric

For local and edge RAG, the winning design is usually:

```text
compact embedding model
  + strong chunking
  + good metadata filters
  + hybrid retrieval when needed
  + reranking
  + small grounded generator
```

not:

```text
giant embedding model
  + unfiltered top-k
  + huge prompt
  + hope the LLM fixes retrieval mistakes
```

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Explain what a vector database does in a RAG system.
2. Explain the difference between Qdrant and pgvector.
3. Describe dense, sparse, and multi-vector retrieval.
4. Explain HNSW and IVFFlat at a practical systems level.
5. Choose Qdrant or pgvector based on architecture, scale, filters, and operations.
6. Compare Granite embeddings with BGE, E5, OpenAI, Cohere, Voyage, and other alternatives.
7. Design an embedding evaluation plan instead of trusting generic leaderboards.
8. Estimate vector storage pressure from embedding dimension and data type.
9. Plan an embedding migration without corrupting retrieval quality.

---

**1. The core RAG storage problem**

A RAG system has two kinds of data:

```text
source data:
  docs, markdown, PDFs, code, tickets, emails, manuals

retrieval data:
  chunks, embeddings, metadata, indexes, scores, citations
```

The LLM does **not search your raw documents directly**.

A typical pipeline is:

```text
document
  -> chunk
  -> embed each chunk
  -> store vector + chunk text + metadata
  -> embed user query
  -> search nearest vectors
  -> rerank/filter
  -> send selected evidence to LLM
```

The vector store owns this part:

```text
stored vectors + metadata + nearest-neighbor index + query API
```

Each stored item is usually a "point" or row:

```json
{
  "id": "doc-17#chunk-03",
  "vector": [0.012, -0.044, 0.331],
  "payload": {
    "document_id": "doc-17",
    "path": "manuals/orin/power.md",
    "section": "Thermals",
    "language": "en",
    "created_at": "2026-05-28",
    "source_hash": "..."
  },
  "text": "The chunk text may live here or in another store."
}
```

The **metadata matters as much as the vector**.

Example query:

```text
"How do I reduce Jetson Orin power draw during idle?"
```

Good retrieval does not just ask:

```text
Which chunks are semantically close?
```

It asks:

```text
Which chunks are semantically close,
inside the right product docs,
in the right version,
in the right language,
from trusted sources,
and recent enough to answer safely?
```

That is why **vector search and filtering need to be designed together**.

---

**2. What is Qdrant?**

Qdrant is a **standalone vector database and search engine**.

It is designed around collections of points:

```text
collection
  -> points
      -> id
      -> vector or named vectors
      -> payload metadata
```

The key design idea:

```text
Qdrant is optimized for vector-native retrieval first.
```

Useful features:

- dense vector search
- sparse vector search
- named vectors
- multi-vector retrieval patterns
- payload metadata filtering
- payload indexes
- HNSW indexing
- quantization options
- horizontal scaling through sharding and replication
- HTTP/gRPC APIs
- client libraries
- local, cloud, private-cloud, and edge deployment patterns

Qdrant is a good fit when retrieval is a **product-critical path**.

Examples:

- local RAG server
- semantic search API
- AI coding assistant over repositories
- recommendation systems
- hybrid search over technical docs
- multi-tenant knowledge retrieval
- edge assistant with a local vector service

The important distinction:

```text
Qdrant is not your relational database.
It is your vector retrieval engine.
```

You may still keep canonical business data in Postgres, SQLite, object storage, or a document store.

In that design, Qdrant stores:

```text
id + vector + retrieval metadata + optional text snippet
```

The source system stores:

```text
full document + permissions + owner + audit history + business state
```

That separation is **normal**.

---

</details>

## 3. 什么是 pgvector？

pgvector 是 **PostgreSQL 的一个扩展**。

它为 Postgres 增加了 **向量类型、距离算子和近似索引**。

关键设计思路：

```text
pgvector brings vector search into an existing SQL database.
```

一张最简表：

```sql
CREATE EXTENSION vector;

CREATE TABLE document_chunks (
  id bigserial PRIMARY KEY,
  document_id text NOT NULL,
  path text NOT NULL,
  chunk text NOT NULL,
  embedding vector(384)
);
```

一个基础的最近邻查询：

```sql
SELECT id, path, chunk
FROM document_chunks
ORDER BY embedding <=> $1
LIMIT 8;
```

`<=>` 是余弦距离。

最大的优势是 **架构简单**：

```text
same database
same backup path
same SQL permissions
same transactions
same app connection pool
same operational team
```

如果你的应用已经以 Postgres 为中心，这一点很有价值。

pgvector 适合以下情况：

- 已经在用 PostgreSQL
- 向量语料规模中等
- 希望让向量结果参与 SQL join
- 关系一致性很重要
- 运维简单比专门的检索特性更重要
- 向量搜索只是一个功能，而非整个产品

示例：

```sql
SELECT c.id, c.path, c.chunk, p.owner_id
FROM document_chunks c
JOIN projects p ON p.id = c.project_id
WHERE p.organization_id = $org_id
ORDER BY c.embedding <=> $query_embedding
LIMIT 8;
```

这条查询正是 pgvector 存在的理由。

你可以在一条 SQL 路径中把 **向量相似度与普通关系条件** 结合起来。

---

## 4. Qdrant vs pgvector：真正的对比

| 维度 | Qdrant | pgvector |
|---|---|---|
| 系统类型 | 专用向量数据库 | PostgreSQL 扩展 |
| 最佳默认用途 | 检索密集的 AI 系统 | 以 SQL 为主、补充向量搜索的应用 |
| API | HTTP/gRPC/客户端库 | SQL |
| 数据模型 | Collections、points、vectors、payloads | 表、行、向量列 |
| 扩展模型 | 独立服务或集群 | 扩展 PostgreSQL |
| 过滤 | payload 过滤与 payload 索引 | SQL `WHERE`、部分索引、分区 |
| 混合检索 | dense + sparse + 多表示模式 | 与 Postgres 全文检索及 rank fusion 结合 |
| 运维 | 需额外部署并监控一个服务 | 复用现有 Postgres 运维 |
| 优势 | 检索特性与向量原生性能 | 简单性与关系型集成 |
| 风险 | 与源数据库的数据同步 | Postgres 可能过载 |

这个决策 **无关意识形态**。

要问：

```text
Is vector retrieval a side feature of my SQL app,
or is it a core runtime service?
```

如果是附属功能：

```text
pgvector is often enough.
```

如果是核心 runtime 服务：

```text
Qdrant is usually the cleaner architecture.
```

---

## 5. 稠密、稀疏与多向量检索

向量搜索 **不是一件事**。

它包含 **多种检索表示**。

### 稠密向量

**稠密向量**是定长的浮点数组。

示例：

```text
384 dimensions
768 dimensions
1024 dimensions
1536 dimensions
3072 dimensions
```

它们捕捉语义相似度。

擅长：

- 同义改写
- 概念相似
- 多语言语义搜索
- 模糊文档检索
- 侧重「含义」而非字面词

不擅长：

- 精确标识符
- 错误码
- 零件编号
- 冷门 API 名称
- 非常精确的关键词约束

失败示例：

```text
Query: "NV_ERR_INVALID_STATE"
```

稠密模型可能召回泛泛的「invalid state」内容，却漏掉那个确切的错误码页面。

### 稀疏向量

**稀疏向量**是大多数取值为零的高维向量。

它们更接近 **关键词与词法检索**。

示例：

- BM25
- SPLADE
- learned sparse retriever

擅长：

- 精确关键词
- 标识符
- 名称
- 罕见词
- 技术符号

不擅长：

- 只靠改写的查询
- 跨语言语义匹配
- 词面不重叠时的概念检索

### 多向量检索

**多向量系统**为一个文档或 chunk 存储多个向量。

示例：

- 每个段落片段一个向量
- ColBERT 风格的 late interaction 向量
- 图像向量 + 文本向量
- 标题向量 + 正文向量
- 针对查询的表示

擅长：

- 长文档内部的精确匹配
- 单个池化向量会丢失细节的检索
- 复杂文档上的高质量搜索

代价：

- 存储更多
- 计算更多
- 排序更复杂
- 运维复杂度更高

### 混合检索

**混合检索**结合稠密与稀疏两路信号。

简单模式：

```text
dense top 20
+ sparse/BM25 top 20
-> reciprocal rank fusion
-> rerank top 10
-> keep top 3
```

为什么有效：

```text
dense catches meaning
sparse catches exact terms
reranker chooses final evidence
```

对于技术文档、代码库和企业手册，混合检索 **往往优于纯稠密检索**。

---


<details>
<summary>English original</summary>

**3. What is pgvector?**

pgvector is an **extension for PostgreSQL**.

It adds **vector types, distance operators, and approximate indexes** to Postgres.

The key design idea:

```text
pgvector brings vector search into an existing SQL database.
```

A minimal table:

```sql
CREATE EXTENSION vector;

CREATE TABLE document_chunks (
  id bigserial PRIMARY KEY,
  document_id text NOT NULL,
  path text NOT NULL,
  chunk text NOT NULL,
  embedding vector(384)
);
```

A basic nearest-neighbor query:

```sql
SELECT id, path, chunk
FROM document_chunks
ORDER BY embedding <=> $1
LIMIT 8;
```

`<=>` is cosine distance.

The biggest advantage is **architectural simplicity**:

```text
same database
same backup path
same SQL permissions
same transactions
same app connection pool
same operational team
```

This is valuable if your application is already centered on Postgres.

pgvector is a good fit when:

- you already use PostgreSQL
- your vector corpus is moderate
- you want SQL joins with vector results
- relational consistency matters
- operational simplicity matters more than specialized retrieval features
- vector search is a feature, not the whole product

Example:

```sql
SELECT c.id, c.path, c.chunk, p.owner_id
FROM document_chunks c
JOIN projects p ON p.id = c.project_id
WHERE p.organization_id = $org_id
ORDER BY c.embedding <=> $query_embedding
LIMIT 8;
```

That query is the reason pgvector exists.

You can combine **vector similarity with normal relational conditions** in one SQL path.

---

**4. Qdrant vs pgvector: the real comparison**

| Dimension | Qdrant | pgvector |
|---|---|---|
| System type | Dedicated vector database | PostgreSQL extension |
| Best default use | Retrieval-heavy AI systems | SQL-first apps adding vector search |
| API | HTTP/gRPC/client libraries | SQL |
| Data model | Collections, points, vectors, payloads | Tables, rows, vector columns |
| Scaling model | Standalone service or cluster | Scale PostgreSQL |
| Filtering | Payload filtering and payload indexes | SQL `WHERE`, partial indexes, partitioning |
| Hybrid search | Dense + sparse + multi-representation patterns | Combine with Postgres full-text search and rank fusion |
| Operations | Extra service to deploy and monitor | Uses existing Postgres operations |
| Strength | Search features and vector-native performance | Simplicity and relational integration |
| Risk | Data sync with source DB | Postgres can become overloaded |

The decision is **not ideological**.

Ask:

```text
Is vector retrieval a side feature of my SQL app,
or is it a core runtime service?
```

If it is a side feature:

```text
pgvector is often enough.
```

If it is a core runtime service:

```text
Qdrant is usually the cleaner architecture.
```

---

**5. Dense, sparse, and multi-vector retrieval**

Vector search is **not one thing**.

There are **several retrieval representations**.

**Dense vectors**

**Dense vectors** are fixed-length arrays of floats.

Example:

```text
384 dimensions
768 dimensions
1024 dimensions
1536 dimensions
3072 dimensions
```

They capture semantic similarity.

Good at:

- paraphrases
- conceptual similarity
- multilingual semantic search
- fuzzy document retrieval
- "meaning" rather than exact words

Bad at:

- exact identifiers
- error codes
- part numbers
- rare API names
- very precise keyword constraints

Example failure:

```text
Query: "NV_ERR_INVALID_STATE"
```

A dense model might retrieve generic "invalid state" content and miss the exact error-code page.

**Sparse vectors**

**Sparse vectors** are high-dimensional vectors where most values are zero.

They are closer to **keyword and lexical retrieval**.

Examples:

- BM25
- SPLADE
- learned sparse retrievers

Good at:

- exact keywords
- identifiers
- names
- rare terms
- technical symbols

Bad at:

- paraphrase-only queries
- cross-lingual semantic matching
- conceptual retrieval when words do not overlap

**Multi-vector retrieval**

**Multi-vector systems** store multiple vectors for one document or chunk.

Examples:

- one vector per passage segment
- ColBERT-style late interaction vectors
- image + text vectors
- title vector + body vector
- query-specific representations

Good at:

- precise matching inside long documents
- retrieval where one single pooled vector loses details
- high-quality search over complex docs

Cost:

- more storage
- more compute
- more complicated ranking
- more operational complexity

**Hybrid retrieval**

**Hybrid retrieval** combines dense and sparse signals.

Simple pattern:

```text
dense top 20
+ sparse/BM25 top 20
-> reciprocal rank fusion
-> rerank top 10
-> keep top 3
```

Why this works:

```text
dense catches meaning
sparse catches exact terms
reranker chooses final evidence
```

For technical docs, codebases, and enterprise manuals, hybrid retrieval is **often better than dense-only retrieval**.

---

</details>

## 6. HNSW 与 IVFFlat

最近邻搜索有两大模式：

```text
exact search:
  compare query against every vector

approximate search:
  use an index to find likely nearest neighbors faster
```

精确搜索召回率完美，但随着语料增长代价高昂。

近似最近邻搜索**用一部分召回率换速度**。

### HNSW

HNSW 意为 **Hierarchical Navigable Small World**。

实践层面：

```text
Build a graph where nearby vectors are connected.
Search by walking the graph toward better candidates.
```

优势：

- 速度/召回率取舍强
- 适用于许多生产搜索工作负载
- 无独立训练步骤
- 可在数据到达之前或到达过程中构建

代价：

- 比更简单的索引占用更多内存
- 索引构建比 IVFFlat 慢
- 参数敏感

重要参数：

```text
m:
  graph connectivity

ef_construction:
  candidate list size during build

ef_search:
  candidate list size during query
```

增大 `ef_search` 通常**提升召回率但增加延迟**。

### IVFFlat

IVFFlat 意为 **inverted file flat**。

实践层面：

```text
Cluster vectors into lists.
At query time, search only the closest lists.
```

优势：

- 构建比 HNSW 快
- 内存压力更低
- 索引大小受限时有用

代价：

- 速度/召回率取舍通常弱于 HNSW
- 建索引前需要代表性数据
- 需要调优 lists/probes

重要参数：

```text
lists:
  number of partitions

probes:
  number of lists searched per query
```

增大 `probes` 提升召回率但增加延迟。

### 实践指南

对大多数应用层 RAG：

```text
Start with HNSW.
Measure recall@k and latency.
Tune search width before changing database.
```

在以下情况使用 IVFFlat：

- 内存较紧张
- 构建时间要紧
- 数据基本静态
- 熟悉 list/probe 取舍的调优

---

## 7. 过滤是系统分出高下的地方

RAG **需要过滤**。

示例：

```text
only docs user can access
only version 2.1 docs
only English docs
only source = official_manual
only product = Jetson Orin
only updated after 2026-01-01
```

**糟糕的过滤会破坏检索。**

难点在于：

```text
Find nearest neighbors, but only among 2% of the corpus.
```

有三种常见策略：

```text
pre-filter:
  reduce candidate set first, then vector search

post-filter:
  vector search first, then filter results

integrated filtered ANN:
  search the vector index with filter awareness
```

后置过滤可能**无声地降低召回率**。

示例：

```text
top_k = 10
filter matches 10% of rows
approximate index returns 10 candidates
after filtering, only 1 candidate remains
```

这**不是语言模型的问题**。

这是**检索规划问题**。

Qdrant 的 payload 索引旨在让过滤向量搜索成为一等检索路径。

pgvector 使用 SQL 过滤、部分索引、分区、迭代扫描以及查询规划器行为来应对这一问题。

两者都能用。

但当你的产品重度依赖过滤后的 ANN 检索时，**务必显式测试这一点**。

---

## 8. Qdrant 架构模式

### 模式 A：本地 sidecar

适合本地 agent 与边缘 RAG。

```text
OpenClaw / local app
  -> Qdrant on localhost
  -> local embedding model
  -> local generator
```

收益：

- 网络边界简单
- 数据本地
- 检索性能好
- 易于替换应用数据库

风险：

- 多一个需要监管的进程
- 若权威文档在别处，需数据同步

### 模式 B：检索服务

适合生产级 AI 应用。

```text
agent runtime
  -> retrieval API
      -> Qdrant
      -> reranker
      -> citation formatter
```

收益：

- 单一检索契约
- 服务级缓存
- 集中式日志
- 更易做 A/B 测试

风险：

- 服务架构更复杂
- 需明确执行访问控制

### 模式 C：中心索引的边缘缓存

适合远程/本地优先系统。

```text
central corpus
  -> sync selected docs
  -> edge Qdrant collection
  -> local agent queries edge index
```

收益：

- 延迟更低
- 私有/离线运行
- 降低云端依赖

风险：

- 同步正确性
- 文档陈旧
- 权限漂移

---

## 9. pgvector 架构模式

### 模式 A：带向量搜索的 SQL 应用

适合 Postgres 已是事实来源的场景。

```text
web app
  -> Postgres
      -> relational tables
      -> pgvector columns
      -> full-text search
```

收益：

- 架构最简单
- join 方便
- 单一备份/恢复路径
- 单一权限模型

风险：

- 向量工作负载与 OLTP 工作负载争抢资源
- 索引调优可能影响数据库资源
- 水平向量扩展不如专用向量服务干净


<details>
<summary>English original</summary>

**6. HNSW and IVFFlat**

Nearest-neighbor search has two broad modes:

```text
exact search:
  compare query against every vector

approximate search:
  use an index to find likely nearest neighbors faster
```

Exact search has perfect recall but becomes expensive as the corpus grows.

Approximate nearest neighbor search **trades some recall for speed**.

**HNSW**

HNSW means **Hierarchical Navigable Small World**.

At a practical level:

```text
Build a graph where nearby vectors are connected.
Search by walking the graph toward better candidates.
```

Strengths:

- strong speed/recall tradeoff
- works well for many production search workloads
- no separate training step
- can be built before or as data arrives

Costs:

- more memory than simpler indexes
- slower index build than IVFFlat
- parameters matter

Important knobs:

```text
m:
  graph connectivity

ef_construction:
  candidate list size during build

ef_search:
  candidate list size during query
```

Increasing `ef_search` usually **improves recall but increases latency**.

**IVFFlat**

IVFFlat means **inverted file flat**.

At a practical level:

```text
Cluster vectors into lists.
At query time, search only the closest lists.
```

Strengths:

- faster build than HNSW
- lower memory pressure
- useful when index size matters

Costs:

- usually weaker speed/recall tradeoff than HNSW
- requires representative data before index creation
- needs tuning of lists/probes

Important knobs:

```text
lists:
  number of partitions

probes:
  number of lists searched per query
```

Increasing `probes` improves recall but increases latency.

**Practical guidance**

For most app-level RAG:

```text
Start with HNSW.
Measure recall@k and latency.
Tune search width before changing database.
```

Use IVFFlat when:

- memory is tighter
- build time matters
- data is mostly static
- you know how to tune list/probe tradeoffs

---

**7. Filtering is where systems diverge**

RAG **needs filters**.

Examples:

```text
only docs user can access
only version 2.1 docs
only English docs
only source = official_manual
only product = Jetson Orin
only updated after 2026-01-01
```

**Bad filtering can break retrieval.**

The hard case:

```text
Find nearest neighbors, but only among 2% of the corpus.
```

There are three common strategies:

```text
pre-filter:
  reduce candidate set first, then vector search

post-filter:
  vector search first, then filter results

integrated filtered ANN:
  search the vector index with filter awareness
```

Post-filtering can **silently reduce recall**.

Example:

```text
top_k = 10
filter matches 10% of rows
approximate index returns 10 candidates
after filtering, only 1 candidate remains
```

That is **not a language-model problem**.

That is a **retrieval planning problem**.

Qdrant's payload indexes are designed to make filtered vector search a first-class retrieval path.

pgvector uses SQL filtering, partial indexes, partitioning, iterative scans, and planner behavior to manage this problem.

Both can work.

But when your product depends heavily on filtered ANN retrieval, **test this explicitly**.

---

**8. Qdrant architecture patterns**

**Pattern A: local sidecar**

Good for local agents and edge RAG.

```text
OpenClaw / local app
  -> Qdrant on localhost
  -> local embedding model
  -> local generator
```

Benefits:

- simple network boundary
- local data
- good retrieval performance
- easy replacement of app database

Risks:

- another process to supervise
- data sync if canonical docs live elsewhere

**Pattern B: retrieval service**

Good for production AI apps.

```text
agent runtime
  -> retrieval API
      -> Qdrant
      -> reranker
      -> citation formatter
```

Benefits:

- one retrieval contract
- service-level caching
- centralized logging
- easier A/B tests

Risks:

- more service architecture
- need clear access-control enforcement

**Pattern C: edge cache of central index**

Good for remote/local-first systems.

```text
central corpus
  -> sync selected docs
  -> edge Qdrant collection
  -> local agent queries edge index
```

Benefits:

- lower latency
- private/offline operation
- reduced cloud dependency

Risks:

- sync correctness
- stale docs
- permission drift

---

**9. pgvector architecture patterns**

**Pattern A: SQL app with vector search**

Good when Postgres is already the source of truth.

```text
web app
  -> Postgres
      -> relational tables
      -> pgvector columns
      -> full-text search
```

Benefits:

- simplest architecture
- easy joins
- one backup/restore path
- one permission model

Risks:

- vector workload competes with OLTP workload
- index tuning can affect database resources
- horizontal vector scaling is not as clean as a dedicated vector service

</details>

### 模式 B：混合 SQL 检索

结合全文搜索与向量搜索。

```sql
-- Dense candidates
SELECT id, 1.0 / (60 + row_number() OVER ()) AS dense_score
FROM document_chunks
ORDER BY embedding <=> $query_embedding
LIMIT 50;

-- Text candidates
SELECT id, ts_rank_cd(textsearch, query) AS text_score
FROM document_chunks, plainto_tsquery($query_text) query
WHERE textsearch @@ query
LIMIT 50;
```

然后在 SQL 或应用代码中融合：

```text
reciprocal rank fusion
cross-encoder rerank
weighted score fusion
```

优点：

- 无需单独的搜索引擎
- 适合 SQL 密集型应用
- 易于按关系状态过滤

风险：

- 查询复杂度更高
- 规划器/索引调优很关键
- 召回率必须实测

---

## 10. embedding 模型选择

embedding 模型**将文本映射为向量**。

不同模型针对不同的取舍做优化：

```text
quality
latency
dimension
context length
license
language coverage
code retrieval
domain retrieval
image/document support
local deployability
cloud API convenience
```

**第一个错误**是问：

```text
What is the best embedding model?
```

而应该问：

```text
What is the best embedding model for this corpus and deployment budget?
```

---

## 11. Granite embeddings

Granite embeddings 是**IBM 的 embedding 模型**，用于检索与搜索。

第 14 讲中实用的本地/边缘目标：

```text
ibm-granite/granite-embedding-97m-multilingual-r2
```

它为何有吸引力：

- 紧凑的 97M 级别模型
- 多语言检索
- 代码检索支持
- 对 embedding 模型而言的长上下文
- Apache-2.0 许可证
- 实用的部署路径
- 对边缘 RAG 的内存/延迟适配良好

以下情况使用 Granite 97M：

- 本地/私有 RAG 重要
- 多语言检索重要
- 需要宽松许可证
- 内存受限
- 需要为 Jetson/edge 提供紧凑的默认选择

以下情况使用 Granite 311M：

- 有服务器资源可用
- 检索质量比低内存更重要
- 语料库更难或更多语言
- 延迟预算允许更大的编码器

要点：

```text
Granite 97M is a strong default for efficient local RAG,
not a universal winner for every retrieval task.
```

---

## 12. Granite 的开源替代方案

### BGE-M3

当需要单一模型支持以下能力时，`BAAI/bge-m3` 是**强大的开源模型**：

- 稠密检索
- 稀疏检索
- 多向量检索
- 多语言检索
- 最长约 8192 token 的较长输入

以下情况使用 BGE-M3：

- 混合检索重要
- 希望从同一模型族获得稠密 + 稀疏
- 多语言搜索重要
- 能承受比微型边缘模型更多的算力

取舍：

```text
more retrieval capability
usually more runtime cost than compact embedding models
```

### 多语言 E5

`intfloat/multilingual-e5-large` 是**成熟的多语言稠密 embedding 模型**。

以下情况使用 E5：

- 多语言文本检索是核心
- 需要广泛使用的基线
- 512 token 截断对分块可接受
- 能承受更大的编码器

取舍：

```text
strong baseline, but shorter context than long-context embedding models
```

### Nomic Embed

Nomic embedding 模型是有用的**开放权重基线**，尤其适合本地开发与可复现实验。

以下情况使用 Nomic 类模型：

- 需要本地推理
- 英文检索足够
- 需要简单的开放权重部署

### Jina embeddings

Jina embedding 模型在以下方面有用：

- 多语言检索
- 较新模型族中的多模态检索
- 代码/文档检索任务
- 部署灵活性

当语料库包含多样化的网页、代码或准多模态文档，且愿意测试模型特定行为时，使用 Jina。

### Snowflake Arctic Embed

Snowflake Arctic Embed 模型是另一个有用的开源检索家族。

以下情况使用它们：

- 需要强大的开源检索基线
- 目标是英文或面向企业的多语言检索
- 需要在同一 evaluation harness（agent 运行时框架）下比较多个开源模型

---

## 13. 基于 API 的 embedding 替代方案

当需要**质量与简洁性胜过本地控制**时，API embeddings 很有用。

### OpenAI embeddings

OpenAI 的 embedding 模型通过 API**易于运维**。

以下情况使用它们：

- 可接受使用云 API
- 需要强大的通用检索质量
- 不希望托管 embedding 基础设施
- 延迟与成本可接受

取舍：

- 依赖外部 API
- token 成本
- 隐私/合规审查
- 供应商锁定

### Cohere Embed

Cohere Embed v4 适用于：

- 多语言搜索
- 商业文档
- 图像/文档截图 embedding
- 可配置的输出维度
- 压缩输出类型

以下情况使用 Cohere：

- 企业文档检索重要
- 多模态文档界面重要
- 需要托管的 embedding 基础设施


<details>
<summary>English original</summary>

**Pattern B: hybrid SQL retrieval**

Combine full-text search and vector search.

```sql
-- Dense candidates
SELECT id, 1.0 / (60 + row_number() OVER ()) AS dense_score
FROM document_chunks
ORDER BY embedding <=> $query_embedding
LIMIT 50;

-- Text candidates
SELECT id, ts_rank_cd(textsearch, query) AS text_score
FROM document_chunks, plainto_tsquery($query_text) query
WHERE textsearch @@ query
LIMIT 50;
```

Then fuse in SQL or app code:

```text
reciprocal rank fusion
cross-encoder rerank
weighted score fusion
```

Benefits:

- no separate search engine
- strong for SQL-heavy apps
- easy to filter by relational state

Risks:

- more query complexity
- planner/index tuning matters
- recall must be measured

---

**10. Embedding model selection**

An embedding model **maps text to a vector**.

Different models optimize for different tradeoffs:

```text
quality
latency
dimension
context length
license
language coverage
code retrieval
domain retrieval
image/document support
local deployability
cloud API convenience
```

The **first mistake** is asking:

```text
What is the best embedding model?
```

Ask instead:

```text
What is the best embedding model for this corpus and deployment budget?
```

---

**11. Granite embeddings**

Granite embeddings are **IBM embedding models** for retrieval and search.

The useful local/edge target from Lecture 14:

```text
ibm-granite/granite-embedding-97m-multilingual-r2
```

Why it is attractive:

- compact 97M-class model
- multilingual retrieval
- code retrieval support
- long context for an embedding model
- Apache-2.0 license
- practical deployment paths
- good memory/latency fit for edge RAG

Use Granite 97M when:

- local/private RAG matters
- multilingual retrieval matters
- you want permissive licensing
- memory is constrained
- you want a compact default for Jetson/edge

Use Granite 311M when:

- server resources are available
- retrieval quality is more important than low memory
- the corpus is harder or more multilingual
- latency budget allows a larger encoder

The important point:

```text
Granite 97M is a strong default for efficient local RAG,
not a universal winner for every retrieval task.
```

---

**12. Open-source alternatives to Granite**

**BGE-M3**

`BAAI/bge-m3` is a **strong open model** when you want one model that supports:

- dense retrieval
- sparse retrieval
- multi-vector retrieval
- multilingual retrieval
- long-ish input up to 8192 tokens

Use BGE-M3 when:

- hybrid retrieval matters
- you want dense + sparse from one model family
- multilingual search matters
- you can afford more compute than a tiny edge model

Tradeoff:

```text
more retrieval capability
usually more runtime cost than compact embedding models
```

**Multilingual E5**

`intfloat/multilingual-e5-large` is a **mature multilingual dense embedding model**.

Use E5 when:

- multilingual text retrieval is central
- you want a widely used baseline
- 512-token truncation is acceptable for your chunks
- you can afford a larger encoder

Tradeoff:

```text
strong baseline, but shorter context than long-context embedding models
```

**Nomic Embed**

Nomic embedding models are useful **open-weight baselines**, especially for local development and reproducible experiments.

Use Nomic-style models when:

- you want local inference
- English retrieval is enough
- you need simple open-weight deployment

**Jina embeddings**

Jina embedding models are useful when you care about:

- multilingual retrieval
- multimodal retrieval in newer model families
- code/doc retrieval tasks
- deployment flexibility

Use Jina when the corpus includes varied web, code, or multimodal-ish documents and you are willing to test model-specific behavior.

**Snowflake Arctic Embed**

Snowflake Arctic Embed models are another useful open retrieval family.

Use them when:

- you want strong open retrieval baselines
- English or multilingual enterprise retrieval is the target
- you are comparing several open models under the same evaluation harness

---

**13. API-based embedding alternatives**

API embeddings are useful when you want **quality and simplicity more than local control**.

**OpenAI embeddings**

OpenAI's embedding models are **simple to operate** through an API.

Use them when:

- cloud API use is acceptable
- you want strong general-purpose retrieval quality
- you do not want to host embedding infrastructure
- latency and cost are acceptable

Tradeoffs:

- external API dependency
- token cost
- privacy/compliance review
- provider lock-in

**Cohere Embed**

Cohere Embed v4 is useful for:

- multilingual search
- business documents
- image/document screenshot embeddings
- configurable output dimensions
- compressed output types

Use Cohere when:

- enterprise document retrieval matters
- multimodal document surfaces matter
- you want managed embedding infrastructure

</details>

### Voyage embeddings

Voyage 模型适用于：

- 高质量托管检索
- 代码检索
- 金融/法律/领域特定检索
- 较新模型族中可配置的维度和输出 dtype

在以下情况使用 Voyage：

- 检索质量至关重要
- 可接受云 API
- 你的领域与其某个专用模型匹配

---

## 14. Embedding 推荐矩阵

把它当作起点，而非铁律。

| 用例 | 良好的起点模型 |
|---|---|
| Jetson/本地多语言 RAG | Granite 97M Multilingual R2 |
| 本地混合检索 | BGE-M3 |
| 成熟的多语言稠密基线 | Multilingual E5 |
| 服务端 IBM/开源企业栈 | Granite 311M Multilingual R2 |
| 云通用检索 | OpenAI embedding 模型或 Voyage 通用模型 |
| 云企业文档搜索 | Cohere Embed v4 |
| 代码密集型检索 | BGE-M3、Granite 或 Voyage 代码模型 |
| 图片丰富的 PDF/幻灯片/截图 | Cohere Embed v4 或专用多模态文档检索器 |
| 仅 SQL 原型 | 任意 embedding 模型 + pgvector |
| 检索产品/API | Embedding 模型 + Qdrant + reranker |

真正的答案应来自你的 **评测集**。

---

## 15. 向量数据库替代方案

Qdrant 和 pgvector 并非仅有的选项。

| 工具 | 最适用场景 |
|---|---|
| Milvus | 大规模向量基础设施、分布式检索 |
| Weaviate | 语义应用层、混合搜索、GraphQL/module 生态 |
| Pinecone | 托管向量数据库，运维开销低 |
| Elasticsearch/OpenSearch | 文本搜索优先，在既有搜索栈上叠加向量搜索 |
| LanceDB | 嵌入式/无服务式向量存储，数据/AI 工作流 |
| Chroma | 本地开发与原型 |
| FAISS | 库级向量索引，不是完整数据库 |

实践指引：

```text
Prototype:
  Chroma, LanceDB, pgvector, or local Qdrant

SQL app:
  pgvector

Production retrieval service:
  Qdrant, Milvus, Weaviate, Pinecone

Text-search-heavy app:
  Elasticsearch/OpenSearch or hybrid Qdrant

Lowest-level custom indexing:
  FAISS
```

就本路线图而言，优先聚焦 Qdrant 和 pgvector，因为它们代表两种最常见的架构选择：

```text
dedicated vector service vs SQL-integrated vector search
```

---

## 16. 存储与内存计算

Embedding 维度**直接影响存储**。

原始向量存储的近似值：

```text
float32 bytes = vector_count * dimension * 4
float16 bytes = vector_count * dimension * 2
int8 bytes    = vector_count * dimension * 1
binary bytes  = vector_count * dimension / 8
```

以 100 万个 chunk 为例：

| 维度 | float32 原始向量 | float16 原始向量 |
|---:|---:|---:|
| 384 | ~1.5 GB | ~0.75 GB |
| 768 | ~3.1 GB | ~1.5 GB |
| 1024 | ~4.1 GB | ~2.0 GB |
| 1536 | ~6.1 GB | ~3.1 GB |
| 3072 | ~12.3 GB | ~6.1 GB |

这不包含：

- HNSW 图内存
- payload 元数据
- 文本/chunk 存储
- 数据库开销
- WAL/复制
- 索引
- 缓存
- 快照/备份

结论：

```text
embedding dimension is an infrastructure decision,
not just a model-card detail.
```

对于边缘 RAG：

```text
384-dimensional embeddings can be a major advantage.
```

对于追求最高质量的服务端检索：

```text
larger embeddings may be worth the storage and memory cost.
```

**两者都要实测。**

---

## 17. 如何评测 embedding 模型

**不要凭感觉选**。

构建一个**检索评测集**。

最小数据集：

```text
100-300 representative questions
ground-truth relevant chunk ids or document ids
query language labels
query type labels
expected citation requirements
```

查询类型标签：

```text
conceptual
exact keyword
API name
error code
code search
cross-lingual
long-document
ambiguous
permission-sensitive
```

指标：

| 指标 | 含义 |
|---|---|
| recall@k | 相关 chunk 是否出现在 top-k 中？ |
| MRR | 第一个相关结果排得多高？ |
| nDCG | 排序质量是否匹配分级相关性？ |
| answer faithfulness | 生成是否始终基于检索到的上下文？ |
| citation accuracy | 引用的 chunk 是否真的支持该答案？ |
| p95 延迟 | 负载下检索是否足够快？ |
| 内存/RAM/VRAM | 是否满足部署约束？ |
| 索引构建时间 | 你能否在运维上刷新语料库？ |

评测循环：

```text
for each embedding model:
  ingest same chunks
  use same metadata
  build index
  run same queries
  measure recall@3, recall@8, MRR, latency
  rerank same candidate count
  run final answer eval
```

重要：

```text
Changing chunking changes the benchmark.
Changing embedding model changes the benchmark.
Changing top-k changes the benchmark.
Changing filters changes the benchmark.
```

每次只比较**一个主要变量**。

---


<details>
<summary>English original</summary>

**Voyage embeddings**

Voyage models are useful for:

- high-quality managed retrieval
- code retrieval
- finance/law/domain-specific retrieval
- configurable dimensions and output dtypes in newer model families

Use Voyage when:

- retrieval quality is critical
- cloud API is acceptable
- your domain matches one of their specialized models

---

**14. Embedding recommendation matrix**

Use this as a starting point, not a law.

| Use case | Good starting model |
|---|---|
| Jetson/local multilingual RAG | Granite 97M Multilingual R2 |
| Local hybrid retrieval | BGE-M3 |
| Mature multilingual dense baseline | Multilingual E5 |
| Server-side IBM/open enterprise stack | Granite 311M Multilingual R2 |
| Cloud general-purpose retrieval | OpenAI embedding model or Voyage general model |
| Cloud enterprise document search | Cohere Embed v4 |
| Code-heavy retrieval | BGE-M3, Granite, or Voyage code model |
| Image-rich PDFs/slides/screenshots | Cohere Embed v4 or a dedicated multimodal document retriever |
| SQL-only prototype | Any embedding model + pgvector |
| Retrieval product/API | Embedding model + Qdrant + reranker |

The real answer should come from your **evaluation set**.

---

**15. Vector database alternatives**

Qdrant and pgvector are not the only options.

| Tool | Best fit |
|---|---|
| Milvus | large-scale vector infrastructure, distributed retrieval |
| Weaviate | semantic app layer, hybrid search, GraphQL/module ecosystem |
| Pinecone | managed vector DB with low operational overhead |
| Elasticsearch/OpenSearch | text search first, vector search added to existing search stack |
| LanceDB | embedded/serverless-style vector storage, data/AI workflows |
| Chroma | local development and prototypes |
| FAISS | library-level vector indexing, not a full database |

Practical guidance:

```text
Prototype:
  Chroma, LanceDB, pgvector, or local Qdrant

SQL app:
  pgvector

Production retrieval service:
  Qdrant, Milvus, Weaviate, Pinecone

Text-search-heavy app:
  Elasticsearch/OpenSearch or hybrid Qdrant

Lowest-level custom indexing:
  FAISS
```

For this roadmap, focus on Qdrant and pgvector first because they represent the two most common architecture choices:

```text
dedicated vector service vs SQL-integrated vector search
```

---

**16. Storage and memory math**

Embedding dimension **affects storage directly**.

Approximate raw vector storage:

```text
float32 bytes = vector_count * dimension * 4
float16 bytes = vector_count * dimension * 2
int8 bytes    = vector_count * dimension * 1
binary bytes  = vector_count * dimension / 8
```

Example for 1 million chunks:

| Dimension | float32 raw vectors | float16 raw vectors |
|---:|---:|---:|
| 384 | ~1.5 GB | ~0.75 GB |
| 768 | ~3.1 GB | ~1.5 GB |
| 1024 | ~4.1 GB | ~2.0 GB |
| 1536 | ~6.1 GB | ~3.1 GB |
| 3072 | ~12.3 GB | ~6.1 GB |

This excludes:

- HNSW graph memory
- payload metadata
- text/chunk storage
- database overhead
- WAL/replication
- indexes
- cache
- snapshots/backups

The lesson:

```text
embedding dimension is an infrastructure decision,
not just a model-card detail.
```

For edge RAG:

```text
384-dimensional embeddings can be a major advantage.
```

For max-quality server retrieval:

```text
larger embeddings may be worth the storage and memory cost.
```

**Measure both.**

---

**17. How to evaluate embedding models**

Do **not choose from vibes**.

Build a **retrieval evaluation set**.

Minimum dataset:

```text
100-300 representative questions
ground-truth relevant chunk ids or document ids
query language labels
query type labels
expected citation requirements
```

Query type labels:

```text
conceptual
exact keyword
API name
error code
code search
cross-lingual
long-document
ambiguous
permission-sensitive
```

Metrics:

| Metric | Meaning |
|---|---|
| recall@k | Did the relevant chunk appear in top-k? |
| MRR | How high was the first relevant result? |
| nDCG | Did the ranking quality match graded relevance? |
| answer faithfulness | Did generation stay grounded in retrieved context? |
| citation accuracy | Did cited chunks actually support the answer? |
| p95 latency | Is retrieval fast enough under load? |
| memory/RAM/VRAM | Does it fit deployment constraints? |
| index build time | Can you refresh the corpus operationally? |

Evaluation loop:

```text
for each embedding model:
  ingest same chunks
  use same metadata
  build index
  run same queries
  measure recall@3, recall@8, MRR, latency
  rerank same candidate count
  run final answer eval
```

Important:

```text
Changing chunking changes the benchmark.
Changing embedding model changes the benchmark.
Changing top-k changes the benchmark.
Changing filters changes the benchmark.
```

Only compare **one major variable at a time**.

---

</details>

## 18. 重排序

embedding 检索是**第一阶段检索**。

重排序是**第二阶段检索**。

模式：

```text
vector search top 30
  -> reranker scores query + candidate text
  -> keep top 3 to 5
  -> send to LLM
```

为什么重排序有帮助：

- 稠密 embedding 粒度粗
- chunk 可能语义接近，但不能回答问题
- 交叉编码器同时检查 query 和候选
- 重排序器减少 prompt 浪费

常见重排序器选择：

- BGE 重排序器系列
- Granite 重排序器
- Jina 重排序器
- Cohere Rerank
- 用于领域特定搜索的自定义交叉编码器

何时加入重排序：

```text
if recall@20 is good but answer quality is weak,
add reranking before changing the generator.
```

如果 recall@20 很差，重排序**救不了你**。

修复：

- 分块
- embedding 模型
- 混合检索
- 元数据过滤
- 语料覆盖

---

## 19. 迁移规则：绝不随意混合 embedding

来自不同 embedding 模型的向量**不在同一个可比较空间**。

错误迁移：

```text
old chunks embedded with Model A
new chunks embedded with Model B
same vector column
same index
same distance metric
```

这会**破坏检索**。

正确迁移：

```text
create new collection or new vector column
backfill all chunks with new model
dual-write new chunks during migration
run retrieval eval against old and new
switch traffic gradually
keep rollback path
delete old index after confidence
```

Qdrant 模式：

```text
collection_docs_v1_granite97
collection_docs_v2_bge_m3
```

pgvector 模式：

```sql
ALTER TABLE document_chunks ADD COLUMN embedding_v2 vector(1024);
CREATE INDEX document_chunks_embedding_v2_hnsw
ON document_chunks USING hnsw (embedding_v2 vector_cosine_ops);
```

保留模型元数据：

```text
embedding_model
embedding_dimension
embedding_normalization
embedding_created_at
chunker_version
source_hash
```

没有这些，调试检索回归就变成**猜谜**。

---

## 20. 安全与权限

向量数据库**如果过滤错误会泄漏数据**。

常见故障：

```text
query embeds user request
vector search retrieves private chunks
LLM summarizes private chunks to unauthorized user
```

**不要依赖大语言模型来实施访问控制**。

访问控制属于**生成之前**：

```text
authorized document ids
  -> retrieval filter
  -> rerank only authorized candidates
  -> prompt only authorized context
```

最小元数据：

```text
tenant_id
organization_id
project_id
visibility
source
document_id
version
deleted_at
```

对于 Qdrant：

```text
use payload filters and payload indexes
```

对于 pgvector：

```text
use SQL WHERE clauses, row-level security if appropriate,
partial indexes, and partitioning where needed
```

**绝不把未授权的 chunk 发送**给模型，并指望 prompt 救你。

---

## 21. 决策框架

### 选择 Qdrant 如果：

- 向量检索是产品核心
- 需要高性能的过滤向量搜索
- 想要稠密 + 稀疏 + 混合检索
- 想要专用检索服务
- 需要水平扩展选项
- 正在构建边缘/本地 RAG（检索增强生成）即服务
- 预期有大量检索流量
- 想要让检索独立于应用数据库演进

### 选择 pgvector 如果：

- 应用已经使用 PostgreSQL
- 向量附加在关系实体上
- SQL join 和事务很重要
- 想要一个运维系统
- 语料规模适中
- 向量搜索不是主要工作负载
- 团队在 Postgres 上比在向量数据库运维上更强

### 选择 Granite 97M 如果：

- 本地/边缘推理很重要
- 内存受限
- 多语言检索很重要
- Apache-2.0 许可很重要
- 紧凑 embedding 是战略优势

### 选择 BGE-M3 如果：

- 混合稠密/稀疏检索很重要
- 想要一个开放模型支持多种检索模式
- 多语言和较长文档很重要
- 能承受更多检索计算

### 选择 API embedding 如果：

- 想要最少托管工作
- 可接受云数据流
- 质量和上市速度比本地控制更重要
- 提供商成本可接受

---

## 22. 推荐默认值

### 本地 Jetson 风格 RAG

```text
embedding:
  Granite 97M Multilingual R2

vector DB:
  Qdrant local service

retrieval:
  dense top 8
  rerank top 3 if latency allows

generator:
  Qwen3.5-4B INT4 or similar 4B-class model

reason:
  compact, private, low memory, good enough to iterate
```

### Postgres 上的现有 SaaS 应用

```text
embedding:
  OpenAI / Cohere / Voyage / Granite depending on policy

vector DB:
  pgvector

retrieval:
  SQL WHERE filters
  HNSW index
  optional Postgres full-text search + rank fusion

reason:
  one database and simple app integration
```


<details>
<summary>English original</summary>

**18. Reranking**

Embedding retrieval is **first-stage retrieval**.

Reranking is **second-stage retrieval**.

Pattern:

```text
vector search top 30
  -> reranker scores query + candidate text
  -> keep top 3 to 5
  -> send to LLM
```

Why reranking helps:

- dense embeddings are coarse
- chunks can be semantically close but not answer the question
- cross-encoders inspect query and candidate together
- rerankers reduce prompt waste

Common reranker choices:

- BGE reranker family
- Granite reranker
- Jina reranker
- Cohere Rerank
- custom cross-encoder for domain-specific search

When to add reranking:

```text
if recall@20 is good but answer quality is weak,
add reranking before changing the generator.
```

If recall@20 is bad, reranking **will not save you**.

Fix:

- chunking
- embedding model
- hybrid retrieval
- metadata filters
- corpus coverage

---

**19. Migration rule: never mix embeddings casually**

Vectors from different embedding models do **not live in the same comparable space**.

Bad migration:

```text
old chunks embedded with Model A
new chunks embedded with Model B
same vector column
same index
same distance metric
```

This **corrupts retrieval**.

Correct migration:

```text
create new collection or new vector column
backfill all chunks with new model
dual-write new chunks during migration
run retrieval eval against old and new
switch traffic gradually
keep rollback path
delete old index after confidence
```

Qdrant pattern:

```text
collection_docs_v1_granite97
collection_docs_v2_bge_m3
```

pgvector pattern:

```sql
ALTER TABLE document_chunks ADD COLUMN embedding_v2 vector(1024);
CREATE INDEX document_chunks_embedding_v2_hnsw
ON document_chunks USING hnsw (embedding_v2 vector_cosine_ops);
```

Keep model metadata:

```text
embedding_model
embedding_dimension
embedding_normalization
embedding_created_at
chunker_version
source_hash
```

Without this, debugging retrieval regressions becomes **guesswork**.

---

**20. Security and permissions**

Vector databases can **leak data if filtering is wrong**.

Common failure:

```text
query embeds user request
vector search retrieves private chunks
LLM summarizes private chunks to unauthorized user
```

Do **not rely on the LLM to enforce access control**.

Access control belongs **before generation**:

```text
authorized document ids
  -> retrieval filter
  -> rerank only authorized candidates
  -> prompt only authorized context
```

Minimum metadata:

```text
tenant_id
organization_id
project_id
visibility
source
document_id
version
deleted_at
```

For Qdrant:

```text
use payload filters and payload indexes
```

For pgvector:

```text
use SQL WHERE clauses, row-level security if appropriate,
partial indexes, and partitioning where needed
```

**Never send unauthorized chunks** to the model and expect a prompt to save you.

---

**21. Decision framework**

**Choose Qdrant if:**

- vector retrieval is central to the product
- you need high-performance filtered vector search
- you want dense + sparse + hybrid retrieval
- you want a dedicated retrieval service
- you need horizontal scaling options
- you are building edge/local RAG as a service
- you expect heavy retrieval traffic
- you want to evolve retrieval independently from the app database

**Choose pgvector if:**

- your app already uses PostgreSQL
- vectors are attached to relational entities
- SQL joins and transactions matter
- you want one operational system
- the corpus is modest or moderate
- vector search is not the dominant workload
- your team is stronger in Postgres than vector DB operations

**Choose Granite 97M if:**

- local/edge inference matters
- memory is constrained
- multilingual retrieval matters
- Apache-2.0 licensing matters
- compact embeddings are a strategic advantage

**Choose BGE-M3 if:**

- hybrid dense/sparse retrieval matters
- you want one open model for multiple retrieval modes
- multilingual and long-ish documents matter
- you can afford more retrieval compute

**Choose API embeddings if:**

- you want minimal hosting work
- cloud data flow is acceptable
- quality and speed-to-market matter more than local control
- provider cost is acceptable

---

**22. Recommended defaults**

**Local Jetson-style RAG**

```text
embedding:
  Granite 97M Multilingual R2

vector DB:
  Qdrant local service

retrieval:
  dense top 8
  rerank top 3 if latency allows

generator:
  Qwen3.5-4B INT4 or similar 4B-class model

reason:
  compact, private, low memory, good enough to iterate
```

**Existing SaaS app on Postgres**

```text
embedding:
  OpenAI / Cohere / Voyage / Granite depending on policy

vector DB:
  pgvector

retrieval:
  SQL WHERE filters
  HNSW index
  optional Postgres full-text search + rank fusion

reason:
  one database and simple app integration
```

</details>

### 搜索密集型 AI 产品

```text
embedding:
  evaluate Granite, BGE-M3, OpenAI, Cohere, Voyage

vector DB:
  Qdrant

retrieval:
  dense + sparse hybrid
  metadata filters
  reranker
  retrieval telemetry

reason:
  retrieval quality and latency are product features
```

### 代码库助手

```text
embedding:
  BGE-M3, Granite, Voyage code model, or a code-specialized model

vector DB:
  Qdrant if repo search is a service
  pgvector if it is part of a Postgres-backed app

retrieval:
  hybrid search
  path/language filters
  symbol-aware chunking
  reranking
```

---

## Mini-lab：选择向量存储与 embedding 模型

为以下场景之一设计一套 RAG（检索增强生成）栈：

- 基于硬件手册的 Jetson 本地助手
- 面向 monorepo 的编码助手
- 公司内部知识库
- 多语言支持机器人
- 以 PDF 为主的企业搜索工具

填写以下内容：

```text
Corpus:
Languages:
Chunk types:
Estimated chunks:
Average chunk tokens:
Strict metadata filters:
Permission model:
Latency target:
Memory target:
Embedding candidates:
Vector DB candidates:
Reranker candidates:
Evaluation query count:
Primary metric:
Secondary metric:
```

然后回答：

```text
I choose Qdrant/pgvector because:
I choose this embedding model because:
I reject the alternatives because:
My first recall@k target is:
My p95 retrieval latency target is:
My migration plan is:
```

若无法用实测数据支撑该选择，你**仍然只是在猜**。

---

## 关键要点

- Qdrant 是专用的向量检索引擎；pgvector 是 PostgreSQL 内部的向量搜索。
- 当检索本身就是产品主线时，Qdrant 通常更优；当 SQL 集成是产品主线时，pgvector 通常更优。
- 稠密检索捕捉语义；稀疏检索捕捉精确词项；对技术文档而言混合检索往往胜出。
- HNSW 是常见的默认 ANN 索引；当内存/构建时间的取舍重要时，IVFFlat 可能有价值。
- 过滤不是细节。权限与元数据过滤是检索正确性的核心。
- Granite 97M 是一款出色的紧凑型本地/边缘 embedding 模型，但视语料与约束而定，BGE-M3、E5、OpenAI、Cohere、Voyage、Jina 等也可能胜出。
- embedding 维度直接影响存储、RAM、索引大小与边缘可行性。
- 未经受控迁移，不要把不同模型的 embedding 混在同一向量空间中。
- 对「哪个最好？」唯一可靠的答案，是在你自己的数据上做检索评测。

---

## 参考文献

- Qdrant 概览：[https://qdrant.tech/documentation/overview/](https://qdrant.tech/documentation/overview/)
- Qdrant 索引：[https://qdrant.tech/documentation/concepts/indexing/](https://qdrant.tech/documentation/concepts/indexing/)
- Qdrant 搜索文档：[https://qdrant.tech/documentation/search/](https://qdrant.tech/documentation/search/)
- pgvector README：[https://github.com/pgvector/pgvector](https://github.com/pgvector/pgvector)
- IBM Granite Embedding 文档：[https://www.ibm.com/granite/docs/models/embedding](https://www.ibm.com/granite/docs/models/embedding)
- Granite 97M Multilingual R2 模型卡：[https://huggingface.co/ibm-granite/granite-embedding-97m-multilingual-r2](https://huggingface.co/ibm-granite/granite-embedding-97m-multilingual-r2)
- BGE-M3 模型卡：[https://huggingface.co/BAAI/bge-m3](https://huggingface.co/BAAI/bge-m3)
- 多语言 E5 模型卡：[https://huggingface.co/intfloat/multilingual-e5-large](https://huggingface.co/intfloat/multilingual-e5-large)
- OpenAI embedding 模型文档：[https://developers.openai.com/api/docs/models/text-embedding-3-large](https://developers.openai.com/api/docs/models/text-embedding-3-large)
- Cohere embeddings 文档：[https://docs.cohere.com/docs/embeddings](https://docs.cohere.com/docs/embeddings)
- Voyage embeddings 文档：[https://docs.voyageai.com/docs/embeddings](https://docs.voyageai.com/docs/embeddings)
- Lecture 14 - Efficient Local RAG Stack：[Lecture-14.md](Lecture-14.md)

---

*下一节：[Lecture 14](Lecture-14.md)*


<details>
<summary>English original</summary>

**Search-heavy AI product**

```text
embedding:
  evaluate Granite, BGE-M3, OpenAI, Cohere, Voyage

vector DB:
  Qdrant

retrieval:
  dense + sparse hybrid
  metadata filters
  reranker
  retrieval telemetry

reason:
  retrieval quality and latency are product features
```

**Codebase assistant**

```text
embedding:
  BGE-M3, Granite, Voyage code model, or a code-specialized model

vector DB:
  Qdrant if repo search is a service
  pgvector if it is part of a Postgres-backed app

retrieval:
  hybrid search
  path/language filters
  symbol-aware chunking
  reranking
```

---

**Mini-lab: choose a vector store and embedding model**

Design a RAG stack for one of these:

- Jetson local assistant over hardware manuals
- coding assistant over a monorepo
- internal company knowledge base
- multilingual support bot
- PDF-heavy enterprise search tool

Fill this out:

```text
Corpus:
Languages:
Chunk types:
Estimated chunks:
Average chunk tokens:
Strict metadata filters:
Permission model:
Latency target:
Memory target:
Embedding candidates:
Vector DB candidates:
Reranker candidates:
Evaluation query count:
Primary metric:
Secondary metric:
```

Then answer:

```text
I choose Qdrant/pgvector because:
I choose this embedding model because:
I reject the alternatives because:
My first recall@k target is:
My p95 retrieval latency target is:
My migration plan is:
```

If you cannot justify the choice with measurements, you are **still guessing**.

---

**Key takeaways**

- Qdrant is a dedicated vector retrieval engine; pgvector is vector search inside PostgreSQL.
- Qdrant is usually better when retrieval is the product path; pgvector is usually better when SQL integration is the product path.
- Dense retrieval captures meaning; sparse retrieval captures exact terms; hybrid retrieval often wins for technical docs.
- HNSW is the common default ANN index; IVFFlat can be useful when memory/build-time tradeoffs matter.
- Filtering is not a detail. Permission and metadata filters are core retrieval correctness.
- Granite 97M is a strong compact local/edge embedding model, but BGE-M3, E5, OpenAI, Cohere, Voyage, Jina, and others can win depending on corpus and constraints.
- Embedding dimension directly affects storage, RAM, index size, and edge viability.
- Do not mix embeddings from different models in one vector space without a controlled migration.
- The only reliable answer to "what is best?" is a retrieval eval on your own data.

---

**References**

- Qdrant overview: [https://qdrant.tech/documentation/overview/](https://qdrant.tech/documentation/overview/)
- Qdrant indexing: [https://qdrant.tech/documentation/concepts/indexing/](https://qdrant.tech/documentation/concepts/indexing/)
- Qdrant search docs: [https://qdrant.tech/documentation/search/](https://qdrant.tech/documentation/search/)
- pgvector README: [https://github.com/pgvector/pgvector](https://github.com/pgvector/pgvector)
- IBM Granite Embedding docs: [https://www.ibm.com/granite/docs/models/embedding](https://www.ibm.com/granite/docs/models/embedding)
- Granite 97M Multilingual R2 model card: [https://huggingface.co/ibm-granite/granite-embedding-97m-multilingual-r2](https://huggingface.co/ibm-granite/granite-embedding-97m-multilingual-r2)
- BGE-M3 model card: [https://huggingface.co/BAAI/bge-m3](https://huggingface.co/BAAI/bge-m3)
- Multilingual E5 model card: [https://huggingface.co/intfloat/multilingual-e5-large](https://huggingface.co/intfloat/multilingual-e5-large)
- OpenAI embedding model docs: [https://developers.openai.com/api/docs/models/text-embedding-3-large](https://developers.openai.com/api/docs/models/text-embedding-3-large)
- Cohere embeddings docs: [https://docs.cohere.com/docs/embeddings](https://docs.cohere.com/docs/embeddings)
- Voyage embeddings docs: [https://docs.voyageai.com/docs/embeddings](https://docs.voyageai.com/docs/embeddings)
- Lecture 14 - Efficient Local RAG Stack: [Lecture-14.md](Lecture-14.md)

---

*Next: [Lecture 14](Lecture-14.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-13.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-13.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
