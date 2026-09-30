---
title: 第 30 讲 — 生产部署
description: 第 30 讲 — 生产部署
---

# 第 30 讲 — 生产部署

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [第 29 讲](Lecture-29.md) | **下一讲：** [第 31 讲](Lecture-31.md)

---

## 学习目标

本讲结束时，你将能够：

1. 把 AI 智能体封装为生产级 FastAPI 端点。
2. 使用 Server-Sent Events（SSE）向客户端流式发送 token。
3. 用 Redis 和 embedding 实现语义缓存，削减重复查询的成本。
4. 根据查询复杂度把请求路由到快速/廉价或更强的模型。
5. 对第三方 API 调用施加限流与指数退避重试。
6. 为安全过滤输出：内容审核与 PII 检测。
7. 增加健康检查端点并处理优雅关闭。

---

## 1. agent 端点的 FastAPI 封装

```python
# pip install fastapi uvicorn pydantic openai python-dotenv

from __future__ import annotations

import os
import time
import uuid
from contextlib import asynccontextmanager
from typing import AsyncGenerator

from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field
from openai import AsyncOpenAI

# ── Models ────────────────────────────────────────────────────────────────────
class ChatRequest(BaseModel):
    message: str = Field(..., min_length=1, max_length=4096)
    session_id: str = Field(default_factory=lambda: uuid.uuid4().hex)
    model: str = Field(default_factory=lambda: os.environ.get("OPENAI_MODEL", "your-model-id"))

class ChatResponse(BaseModel):
    session_id: str
    answer: str
    model_used: str
    latency_ms: float
    tokens_used: int

# ── Application state ─────────────────────────────────────────────────────────
class AppState:
    client: AsyncOpenAI | None = None
    ready: bool = False

state = AppState()

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup
    state.client = AsyncOpenAI(api_key=os.environ.get("OPENAI_API_KEY", "sk-fake"))
    state.ready = True
    print("Agent service started")
    yield
    # Shutdown
    state.ready = False
    print("Agent service shutting down gracefully")

app = FastAPI(title="AI Agent API", version="1.0.0", lifespan=lifespan)

# ── Health checks ─────────────────────────────────────────────────────────────
@app.get("/health")
async def health():
    return {"status": "ok" if state.ready else "starting", "timestamp": time.time()}

@app.get("/ready")
async def readiness():
    if not state.ready:
        raise HTTPException(status_code=503, detail="Service not ready")
    return {"status": "ready"}

# ── Chat endpoint ─────────────────────────────────────────────────────────────
SYSTEM_PROMPT = (
    "You are a helpful AI assistant specializing in hardware engineering. "
    "Answer questions clearly and concisely."
)

@app.post("/chat", response_model=ChatResponse)
async def chat(req: ChatRequest):
    start = time.perf_counter()

    response = await state.client.chat.completions.create(
        model=req.model,
        messages=[
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user",   "content": req.message},
        ],
        temperature=0.2,
    )

    answer = response.choices[0].message.content
    latency_ms = (time.perf_counter() - start) * 1000

    return ChatResponse(
        session_id=req.session_id,
        answer=answer,
        model_used=req.model,
        latency_ms=round(latency_ms, 1),
        tokens_used=response.usage.total_tokens,
    )
```

运行方式：

```bash
uvicorn agent_api:app --host 0.0.0.0 --port 8000 --reload
```

---


<details>
<summary>English original</summary>

**Lecture 30 — Production Deployment**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 29](Lecture-29.md) | **Next:** [Lecture 31](Lecture-31.md)

---

**Learning Objectives**

By the end of this lecture you will be able to:

1. Wrap an AI agent in a production-grade FastAPI endpoint.
2. Stream tokens to the client using Server-Sent Events (SSE).
3. Implement semantic caching with Redis and embeddings to cut repeat-query cost.
4. Route requests to fast/cheap or powerful models based on query complexity.
5. Apply rate limiting and exponential-backoff retry to third-party API calls.
6. Filter outputs for safety: content moderation and PII detection.
7. Add health-check endpoints and handle graceful shutdown.

---

**1. FastAPI Wrapper for an Agent Endpoint**

```python
# pip install fastapi uvicorn pydantic openai python-dotenv

from __future__ import annotations

import os
import time
import uuid
from contextlib import asynccontextmanager
from typing import AsyncGenerator

from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field
from openai import AsyncOpenAI

# ── Models ────────────────────────────────────────────────────────────────────
class ChatRequest(BaseModel):
    message: str = Field(..., min_length=1, max_length=4096)
    session_id: str = Field(default_factory=lambda: uuid.uuid4().hex)
    model: str = Field(default_factory=lambda: os.environ.get("OPENAI_MODEL", "your-model-id"))

class ChatResponse(BaseModel):
    session_id: str
    answer: str
    model_used: str
    latency_ms: float
    tokens_used: int

# ── Application state ─────────────────────────────────────────────────────────
class AppState:
    client: AsyncOpenAI | None = None
    ready: bool = False

state = AppState()

@asynccontextmanager
async def lifespan(app: FastAPI):
    # Startup
    state.client = AsyncOpenAI(api_key=os.environ.get("OPENAI_API_KEY", "sk-fake"))
    state.ready = True
    print("Agent service started")
    yield
    # Shutdown
    state.ready = False
    print("Agent service shutting down gracefully")

app = FastAPI(title="AI Agent API", version="1.0.0", lifespan=lifespan)

# ── Health checks ─────────────────────────────────────────────────────────────
@app.get("/health")
async def health():
    return {"status": "ok" if state.ready else "starting", "timestamp": time.time()}

@app.get("/ready")
async def readiness():
    if not state.ready:
        raise HTTPException(status_code=503, detail="Service not ready")
    return {"status": "ready"}

# ── Chat endpoint ─────────────────────────────────────────────────────────────
SYSTEM_PROMPT = (
    "You are a helpful AI assistant specializing in hardware engineering. "
    "Answer questions clearly and concisely."
)

@app.post("/chat", response_model=ChatResponse)
async def chat(req: ChatRequest):
    start = time.perf_counter()

    response = await state.client.chat.completions.create(
        model=req.model,
        messages=[
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user",   "content": req.message},
        ],
        temperature=0.2,
    )

    answer = response.choices[0].message.content
    latency_ms = (time.perf_counter() - start) * 1000

    return ChatResponse(
        session_id=req.session_id,
        answer=answer,
        model_used=req.model,
        latency_ms=round(latency_ms, 1),
        tokens_used=response.usage.total_tokens,
    )
```

Run with:

```bash
uvicorn agent_api:app --host 0.0.0.0 --port 8000 --reload
```

---

</details>

## 2. 基于 Server-Sent Events (SSE) 的流式传输

**SSE** 让客户端在 token 生成的同时即刻接收，使长响应变得**可交互**。

```python
# pip install sse-starlette

from sse_starlette.sse import EventSourceResponse
from fastapi import FastAPI
from pydantic import BaseModel
import asyncio

class StreamRequest(BaseModel):
    message: str
    session_id: str = ""

@app.post("/chat/stream")
async def chat_stream(req: StreamRequest):
    async def token_generator() -> AsyncGenerator[dict, None]:
        try:
            stream = await state.client.chat.completions.create(
                model=os.environ.get("OPENAI_MODEL", "your-model-id"),
                messages=[
                    {"role": "system", "content": SYSTEM_PROMPT},
                    {"role": "user",   "content": req.message},
                ],
                stream=True,
                temperature=0.2,
            )
            async for chunk in stream:
                delta = chunk.choices[0].delta
                if delta.content:
                    yield {"event": "token", "data": delta.content}
            yield {"event": "done", "data": "[DONE]"}
        except Exception as e:
            yield {"event": "error", "data": str(e)}

    return EventSourceResponse(token_generator())
```

**客户端 JavaScript（供参考）：**

```javascript
const evtSource = new EventSource("/chat/stream?message=What+is+HBM3");
evtSource.addEventListener("token", (e) => process.stdout.write(e.data));
evtSource.addEventListener("done", () => evtSource.close());
```

**Python 客户端：**

```python
import httpx

with httpx.stream("POST", "http://localhost:8000/chat/stream",
                  json={"message": "Explain HBM3"}) as response:
    for line in response.iter_lines():
        if line.startswith("data:"):
            token = line[5:].strip()
            if token != "[DONE]":
                print(token, end="", flush=True)
print()
```

---

## 3. 基于 Redis + embedding 的语义缓存

**语义缓存**存储 (query_embedding → answer) 对。对于新查询，如果它与某个已缓存查询的相似度在 **相似度阈值** 内，则直接返回缓存的答案，无需调用 LLM。

```python
# pip install redis sentence-transformers numpy

import json
import time
import numpy as np
import redis
from sentence_transformers import SentenceTransformer

class SemanticCache:
    """
    Redis-backed semantic cache.
    Keys: 'cache:emb:{id}' (vector as JSON) and 'cache:ans:{id}' (answer string).
    """

    def __init__(
        self,
        redis_url: str = "redis://localhost:6379",
        model_name: str = "all-MiniLM-L6-v2",
        similarity_threshold: float = 0.95,
        ttl_seconds: int = 3600,
    ):
        self.redis = redis.from_url(redis_url)
        self.model = SentenceTransformer(model_name)
        self.threshold = similarity_threshold
        self.ttl = ttl_seconds
        self._index_key = "cache:index"  # list of all cache entry ids

    def _embed(self, text: str) -> np.ndarray:
        return self.model.encode(text, normalize_embeddings=True)

    def get(self, query: str) -> str | None:
        """Return cached answer if a sufficiently similar query exists."""
        q_vec = self._embed(query)
        ids = self.redis.lrange(self._index_key, 0, -1)

        best_sim = -1.0
        best_id = None
        for id_bytes in ids:
            cache_id = id_bytes.decode()
            emb_json = self.redis.get(f"cache:emb:{cache_id}")
            if not emb_json:
                continue
            cached_vec = np.array(json.loads(emb_json))
            sim = float(np.dot(q_vec, cached_vec))
            if sim > best_sim:
                best_sim = sim
                best_id = cache_id

        if best_sim >= self.threshold and best_id:
            print(f"[Cache HIT] similarity={best_sim:.4f}")
            answer = self.redis.get(f"cache:ans:{best_id}")
            return answer.decode() if answer else None

        print(f"[Cache MISS] best_similarity={best_sim:.4f}")
        return None

    def set(self, query: str, answer: str):
        """Store a new cache entry."""
        cache_id = f"{time.time():.0f}_{hash(query) % 100000}"
        q_vec = self._embed(query)
        self.redis.setex(f"cache:emb:{cache_id}", self.ttl, json.dumps(q_vec.tolist()))
        self.redis.setex(f"cache:ans:{cache_id}", self.ttl, answer)
        self.redis.rpush(self._index_key, cache_id)
        print(f"[Cache SET] id={cache_id}")

    def stats(self) -> dict:
        n = self.redis.llen(self._index_key)
        return {"cached_entries": n, "ttl_seconds": self.ttl, "threshold": self.threshold}


# Integrate with the FastAPI endpoint
cache = SemanticCache(similarity_threshold=0.95)

async def cached_chat(message: str) -> tuple[str, bool]:
    """Return (answer, from_cache)."""
    cached = cache.get(message)
    if cached:
        return cached, True

    response = await state.client.chat.completions.create(
        model=os.environ.get("OPENAI_MODEL", "your-model-id"),
        messages=[{"role": "user", "content": message}],
        temperature=0,
    )
    answer = response.choices[0].message.content
    cache.set(message, answer)
    return answer, False
```

---

## 4. 模型布线

将廉价/简单的查询布线到 **快速模型**，将复杂查询布线到 **能力更强的模型**。

```python
import os
import re

# Routing rules
FAST_MODEL = os.environ.get("OPENAI_FAST_MODEL", "provider-fast-model")
SMART_MODEL = os.environ.get("OPENAI_SMART_MODEL", "provider-reasoning-model")

COMPLEXITY_SIGNALS = [
    r"\bcompare\b", r"\bdifference\b", r"\bwhy\b", r"\bexplain\b",
    r"\banalyze\b", r"\bimplement\b", r"\bdesign\b", r"\barchitecture\b",
    r"\btradeoff\b", r"\bpros and cons\b",
]

def estimate_complexity(message: str) -> str:
    """
    Returns 'simple' or 'complex' based on heuristics.
    For production, replace with a small classifier LLM call.
    """
    msg_lower = message.lower()
    word_count = len(message.split())

    # Rule 1: very short messages are simple
    if word_count <= 5:
        return "simple"

    # Rule 2: complexity signal keywords
    for pattern in COMPLEXITY_SIGNALS:
        if re.search(pattern, msg_lower):
            return "complex"

    # Rule 3: long messages tend to be complex
    if word_count > 50:
        return "complex"

    return "simple"


def select_model(message: str) -> str:
    complexity = estimate_complexity(message)
    model = FAST_MODEL if complexity == "simple" else SMART_MODEL
    print(f"[Router] complexity={complexity} → {model}")
    return model


@app.post("/chat/smart", response_model=ChatResponse)
async def smart_chat(req: ChatRequest):
    start = time.perf_counter()
    model = select_model(req.message)

    response = await state.client.chat.completions.create(
        model=model,
        messages=[
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user",   "content": req.message},
        ],
        temperature=0.2,
    )
    answer = response.choices[0].message.content
    latency_ms = (time.perf_counter() - start) * 1000

    return ChatResponse(
        session_id=req.session_id,
        answer=answer,
        model_used=model,
        latency_ms=round(latency_ms, 1),
        tokens_used=response.usage.total_tokens,
    )
```

---

## 5. 限流与带指数退避的重试

```python
# pip install tenacity

import asyncio
from tenacity import (
    retry,
    stop_after_attempt,
    wait_exponential,
    retry_if_exception_type,
    before_sleep_log,
)
import logging
from openai import RateLimitError, APITimeoutError, APIConnectionError

logger = logging.getLogger(__name__)

RETRYABLE_ERRORS = (RateLimitError, APITimeoutError, APIConnectionError)

@retry(
    retry=retry_if_exception_type(RETRYABLE_ERRORS),
    wait=wait_exponential(multiplier=1, min=1, max=60),
    stop=stop_after_attempt(5),
    before_sleep=before_sleep_log(logger, logging.WARNING),
    reraise=True,
)
async def resilient_completion(client: AsyncOpenAI, messages: list[dict], model: str) -> str:
    """LLM call with automatic retry on transient errors."""
    response = await client.chat.completions.create(
        model=model,
        messages=messages,
        temperature=0,
        timeout=30.0,
    )
    return response.choices[0].message.content


# Per-IP rate limiting using a token bucket
from collections import defaultdict

class RateLimiter:
    """Simple in-memory token bucket rate limiter."""

    def __init__(self, requests_per_minute: int = 60):
        self.rpm = requests_per_minute
        self._buckets: dict[str, list[float]] = defaultdict(list)

    def is_allowed(self, client_id: str) -> bool:
        now = time.time()
        window = 60.0
        timestamps = self._buckets[client_id]
        # Remove timestamps older than 1 minute
        self._buckets[client_id] = [t for t in timestamps if now - t < window]
        if len(self._buckets[client_id]) >= self.rpm:
            return False
        self._buckets[client_id].append(now)
        return True


rate_limiter = RateLimiter(requests_per_minute=30)

from fastapi import Header

@app.post("/chat/limited", response_model=ChatResponse)
async def rate_limited_chat(req: ChatRequest, x_client_id: str = Header(default="anonymous")):
    if not rate_limiter.is_allowed(x_client_id):
        raise HTTPException(status_code=429, detail="Rate limit exceeded. Try again in 60 seconds.")

    start = time.perf_counter()
    answer = await resilient_completion(
        state.client,
        [{"role": "user", "content": req.message}],
        model=req.model,
    )
    latency_ms = (time.perf_counter() - start) * 1000

    return ChatResponse(
        session_id=req.session_id,
        answer=answer,
        model_used=req.model,
        latency_ms=round(latency_ms, 1),
        tokens_used=0,  # usage not available without full response object
    )
```

---

## 6. 安全过滤器

### 6.1 内容审核

```python
async def check_moderation(text: str) -> dict:
    """Use OpenAI moderation API to flag unsafe content."""
    response = await state.client.moderations.create(input=text)
    result = response.results[0]
    return {
        "flagged": result.flagged,
        "categories": {k: v for k, v in result.categories.__dict__.items() if v},
    }


@app.post("/chat/safe", response_model=ChatResponse)
async def safe_chat(req: ChatRequest):
    # Moderate the input
    moderation = await check_moderation(req.message)
    if moderation["flagged"]:
        raise HTTPException(
            status_code=400,
            detail=f"Input flagged by content moderation: {moderation['categories']}",
        )

    start = time.perf_counter()
    answer = await resilient_completion(
        state.client,
        [{"role": "system", "content": SYSTEM_PROMPT}, {"role": "user", "content": req.message}],
        model=req.model,
    )

    # Moderate the output as well
    output_mod = await check_moderation(answer)
    if output_mod["flagged"]:
        answer = "I'm unable to provide that response."

    latency_ms = (time.perf_counter() - start) * 1000
    return ChatResponse(
        session_id=req.session_id,
        answer=answer,
        model_used=req.model,
        latency_ms=round(latency_ms, 1),
        tokens_used=0,
    )
```

### 6.2 PII 检测

```python
# pip install presidio-analyzer presidio-anonymizer

from presidio_analyzer import AnalyzerEngine
from presidio_anonymizer import AnonymizerEngine

analyzer = AnalyzerEngine()
anonymizer = AnonymizerEngine()

PII_ENTITIES = ["PERSON", "EMAIL_ADDRESS", "PHONE_NUMBER", "CREDIT_CARD", "US_SSN", "IP_ADDRESS"]

def detect_pii(text: str) -> list[dict]:
    """Return detected PII entities."""
    results = analyzer.analyze(text=text, language="en", entities=PII_ENTITIES)
    return [
        {"type": r.entity_type, "score": r.score, "start": r.start, "end": r.end}
        for r in results
    ]

def anonymize_pii(text: str) -> str:
    """Replace PII with placeholder tokens."""
    results = analyzer.analyze(text=text, language="en", entities=PII_ENTITIES)
    if not results:
        return text
    anonymized = anonymizer.anonymize(text=text, analyzer_results=results)
    return anonymized.text


# Example
text = "My name is John Smith. Email me at john@example.com or call 555-123-4567."
pii_found = detect_pii(text)
print(f"PII detected: {pii_found}")

clean_text = anonymize_pii(text)
print(f"Anonymized: {clean_text}")
# Output: "My name is <PERSON>. Email me at <EMAIL_ADDRESS> or call <PHONE_NUMBER>."
```

---

## 7. 健康检查与优雅关闭

```python
# ── Full application with all features ────────────────────────────────────────
import signal
import asyncio
from contextlib import asynccontextmanager

# Track background tasks for graceful cleanup
background_tasks: set[asyncio.Task] = set()

@asynccontextmanager
async def lifespan(app: FastAPI):
    # ── Startup ────────────────────────────────────────────────────────────────
    state.client = AsyncOpenAI(api_key=os.environ.get("OPENAI_API_KEY", "sk-fake"))
    state.ready = True
    print("Startup complete. Service is ready.")

    yield  # application runs here

    # ── Shutdown ───────────────────────────────────────────────────────────────
    state.ready = False
    print("Shutdown signal received. Draining in-flight requests...")

    # Cancel and await all background tasks
    for task in background_tasks:
        task.cancel()
    if background_tasks:
        await asyncio.gather(*background_tasks, return_exceptions=True)

    print("Graceful shutdown complete.")


@app.get("/health/detailed")
async def health_detailed():
    """Detailed health check for orchestrators (k8s liveness probe)."""
    checks = {
        "api_client": state.client is not None,
        "ready": state.ready,
    }
    # Check Redis connectivity
    try:
        cache.redis.ping()
        checks["cache"] = True
    except Exception:
        checks["cache"] = False

    all_ok = all(checks.values())
    return JSONResponse(
        content={"status": "ok" if all_ok else "degraded", "checks": checks},
        status_code=200 if all_ok else 503,
    )


# Kubernetes probe endpoints
@app.get("/livez")
async def liveness():
    """k8s liveness probe — process is alive."""
    return {"alive": True}

@app.get("/readyz")
async def readiness_probe():
    """k8s readiness probe — ready to serve traffic."""
    if not state.ready:
        raise HTTPException(status_code=503)
    return {"ready": True}
```

### 7.1 整合起来

```python
# Complete minimal production agent service
# Save as: agent_service.py
# Run with: uvicorn agent_service:app --host 0.0.0.0 --port 8000

"""
Agent service startup checklist:
  1. Set OPENAI_API_KEY environment variable
  2. Start Redis: docker run -d -p 6379:6379 redis:alpine
  3. uvicorn agent_service:app --host 0.0.0.0 --port 8000 --workers 4

Endpoints:
  GET  /health           — basic health check
  GET  /health/detailed  — full dependency health check
  GET  /livez            — k8s liveness probe
  GET  /readyz           — k8s readiness probe
  POST /chat             — synchronous chat (JSON response)
  POST /chat/stream      — streaming chat (SSE)
  POST /chat/smart       — chat with automatic model routing
  POST /chat/safe        — chat with content moderation + PII detection
  POST /chat/limited     — chat with per-client rate limiting
"""

# Test with curl:
# curl -X POST http://localhost:8000/chat \
#   -H "Content-Type: application/json" \
#   -d '{"message": "What is HBM3?"}'

# Streaming test:
# curl -X POST http://localhost:8000/chat/stream \
#   -H "Content-Type: application/json" \
#   -H "Accept: text/event-stream" \
#   -d '{"message": "Explain NVLink 4.0"}'
```

---

## 关键要点

- **FastAPI + async OpenAI** 是标准的生产技术栈。用 `lifespan` 实现干净的启动/关闭，用 `pydantic` 做请求校验。
- **SSE 流式**能显著改善长响应的感知延迟 —— 从第一天就把它做进去。
- 相似度阈值取 0.95 的**语义缓存**，在典型的客服/问答工作负载上可减少 30–60% 的 LLM 调用。
- **模型路由**把简单查询发给便宜模型，可省下 10–20 倍成本。哪怕只是基于规则的分类器，也是个不错的起点。
- 配合 `tenacity` 的**指数退避**能从容处理瞬态 API 错误。务必设置 `max_attempts` 上限。
- **输入与输出都要审核。** 输入审核防滥用；输出审核防责任。
- 把用户数据发送给外部 LLM API 之前做 **PII 匿名化**，在大多数司法辖区是合规要求。
- **三个探针端点**（`/health`, `/livez`, `/readyz`）是 Kubernetes 部署的最低要求。

---

## 练习

### 练习 1 —— 流式聊天客户端

用 `httpx` 库构建一个 Python 命令行聊天客户端，连接到 `/chat/stream` 端点。客户端应在本地维护一份历史消息列表，并在每次请求中把它们前置为对话历史。客户端应在 token 流入时逐个打印，并在每次响应后显示总延迟和 token 数。

### 练习 2 —— 缓存 benchmark

搭建 `SemanticCache`（如有需要，可用基于 dict 的内存存储 mock 掉 Redis）。构造 20 条查询 —— 10 条互不相同，另 10 条是前 10 条的近似改写。对每一对查询，测量第一次调用（缓存未命中，走 LLM 调用）与第二次调用（缓存命中，不走 LLM 调用）的延迟。画出延迟分布，并报告缓存命中率与平均加速比。

### 练习 3 —— 生产环境加固

从基础的 `/chat` 端点起步，逐条加入以下加固特性：
1. 请求 ID 头（`X-Request-ID`），并在响应中回显。
2. 请求超时 —— 若 LLM 调用耗时超过 10 秒，返回 HTTP 504。
3. 熔断器 —— 连续 5 次 LLM 失败后，直接返回 HTTP 503 而不再调用 API，直到 30 秒冷却期过去。
编写集成测试（使用 `httpx.AsyncClient` 和 `pytest-asyncio`）以验证每一种行为。

---

*下一讲：[Lecture 31](Lecture-31.md)*


<details>
<summary>English original</summary>

**7.1 Putting It All Together**

```python
# Complete minimal production agent service
# Save as: agent_service.py
# Run with: uvicorn agent_service:app --host 0.0.0.0 --port 8000

"""
Agent service startup checklist:
  1. Set OPENAI_API_KEY environment variable
  2. Start Redis: docker run -d -p 6379:6379 redis:alpine
  3. uvicorn agent_service:app --host 0.0.0.0 --port 8000 --workers 4

Endpoints:
  GET  /health           — basic health check
  GET  /health/detailed  — full dependency health check
  GET  /livez            — k8s liveness probe
  GET  /readyz           — k8s readiness probe
  POST /chat             — synchronous chat (JSON response)
  POST /chat/stream      — streaming chat (SSE)
  POST /chat/smart       — chat with automatic model routing
  POST /chat/safe        — chat with content moderation + PII detection
  POST /chat/limited     — chat with per-client rate limiting
"""

# Test with curl:
# curl -X POST http://localhost:8000/chat \
#   -H "Content-Type: application/json" \
#   -d '{"message": "What is HBM3?"}'

# Streaming test:
# curl -X POST http://localhost:8000/chat/stream \
#   -H "Content-Type: application/json" \
#   -H "Accept: text/event-stream" \
#   -d '{"message": "Explain NVLink 4.0"}'
```

---

**Key Takeaways**

- **FastAPI + async OpenAI** is the standard production stack. Use `lifespan` for clean startup/shutdown and `pydantic` for request validation.
- **SSE streaming** dramatically improves perceived latency for long responses — implement it from day one.
- **Semantic caching** with a 0.95 similarity threshold can reduce LLM calls by 30–60% on typical support/QA workloads.
- **Model routing** saves 10–20x on cost by sending simple queries to cheap models. Even a rule-based classifier is a good start.
- **Exponential backoff** with `tenacity` handles transient API errors gracefully. Always set a `max_attempts` ceiling.
- **Moderate both input and output.** Input moderation prevents abuse; output moderation prevents liability.
- **PII anonymization** before sending user data to external LLM APIs is a compliance requirement in most jurisdictions.
- **Three probe endpoints** (`/health`, `/livez`, `/readyz`) are the minimum for Kubernetes deployment.

---

**Exercises**

**Exercise 1 — Streaming Chat Client**

Build a Python command-line chat client that connects to the `/chat/stream` endpoint using the `httpx` library. The client should maintain a list of previous messages locally and prepend them as conversation history in each request. The client should print tokens as they stream in and show the total latency and token count after each response.

**Exercise 2 — Cache Benchmark**

Set up the `SemanticCache` (you can mock Redis with a dict-based in-memory store if needed). Create 20 queries — 10 unique and 10 that are near-paraphrases of the first 10. Measure the latency of the first call (cache miss, LLM call) vs the second call (cache hit, no LLM call) for each pair. Plot the latency distribution and report the cache hit rate and average speedup ratio.

**Exercise 3 — Production Hardening**

Start with the basic `/chat` endpoint and add the following hardening features one by one:
1. Request ID header (`X-Request-ID`) that is echoed in the response.
2. Request timeout — if the LLM call takes more than 10 seconds, return HTTP 504.
3. Circuit breaker — after 5 consecutive LLM failures, return HTTP 503 without calling the API until a 30-second cooldown passes.
Write integration tests (using `httpx.AsyncClient` and `pytest-asyncio`) that verify each behavior.

---

*Next: [Lecture 31](Lecture-31.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-30.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-30.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
