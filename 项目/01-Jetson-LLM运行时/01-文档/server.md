---
title: HTTP 服务器
description: HTTP 服务器
---

# HTTP 服务器

## 概述

基于裸 POSIX socket 的 OpenAI 兼容 REST API —— 无外部依赖。顺序处理请求（一次只跑一个推理，适合单 GPU 的 Jetson）。

### 源码：`src/server/http_server.cpp`、`src/main_server.cpp`

## 启动服务器

```bash
./build/jetson-llm-server -m models/model.gguf -p 8080

# Options:
#   -m PATH      GGUF model (required)
#   -p PORT      HTTP port (default: 8080)
#   -c INT       Context length (0 = auto)
#   --fp16-kv    Use FP16 KV cache (default: INT8)
```

## 端点

### GET /health

返回 Jetson 特有的系统健康状态。

```bash
curl http://jetson:8080/health
```

响应：
```json
{
  "status": "ok",
  "model": "TinyLlama-1.1B-Chat-v1.0",
  "memory": {
    "total_mb": 7633,
    "free_mb": 4200,
    "model_mb": 669,
    "kv_mb": 45
  },
  "thermal": {
    "gpu_c": 48.5,
    "cpu_c": 47.0,
    "throttling": false
  },
  "power": {
    "mode": "25W",
    "gpu_mhz": 1300
  },
  "gpu_util_pct": 75
}
```

### GET /v1/models

列出已加载的模型（OpenAI 兼容）。

```bash
curl http://jetson:8080/v1/models
```

响应：
```json
{
  "data": [
    {
      "id": "TinyLlama-1.1B-Chat-v1.0",
      "object": "model"
    }
  ]
}
```

### POST /v1/chat/completions

OpenAI 兼容的 chat completion。

```bash
curl http://jetson:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "messages": [
      {"role": "user", "content": "What is 2+2?"}
    ],
    "max_tokens": 64
  }'
```

响应：
```json
{
  "id": "jllm-1712345678",
  "object": "chat.completion",
  "model": "TinyLlama-1.1B-Chat-v1.0",
  "choices": [
    {
      "index": 0,
      "message": {
        "role": "assistant",
        "content": "2+2 equals 4."
      },
      "finish_reason": "stop"
    }
  ],
  "usage": {
    "prompt_tokens": 5,
    "completion_tokens": 8,
    "total_tokens": 13
  },
  "jetson": {
    "decode_tok_s": 28.4,
    "peak_mem_mb": 1823,
    "peak_temp_c": 48.2
  }
}
```

注意：`jetson` 字段是非标准扩展，包含 Jetson 特有的性能指标。

## 客户端示例

### Python

```python
import requests

r = requests.post("http://jetson:8080/v1/chat/completions", json={
    "messages": [{"role": "user", "content": "Hello"}],
    "max_tokens": 64
})
print(r.json()["choices"][0]["message"]["content"])
```

### OpenAI SDK（兼容）

```python
from openai import OpenAI

client = OpenAI(base_url="http://jetson:8080/v1", api_key="unused")
response = client.chat.completions.create(
    model="tinyllama",
    messages=[{"role": "user", "content": "Hello"}],
    max_tokens=64
)
print(response.choices[0].message.content)
```

### curl（监控）

```bash
# Health check every 5 seconds
watch -n5 'curl -s http://jetson:8080/health | python3 -m json.tool'
```

## 实现说明

- **单线程请求处理** —— 一次只跑一个推理。Jetson 只有一块 GPU；并发推理会争抢内存和带宽。
- **裸 POSIX socket** —— 无外部 HTTP 库依赖。只处理 OpenAI 兼容客户端的正常路径。
- **JSON 输出** —— 用 `snprintf` 手工构造（无 JSON 库依赖）。字符串对引号、反斜杠和控制字符做转义。
- **CORS 头** —— 为浏览器客户端启用 `Access-Control-Allow-Origin: *`。
- **顺序执行** —— `accept()` → `handle_client()` → `close()` → `accept()`。无线程。

## 后续增强

- 流式 SSE（Server-Sent Events），逐 token 输出
- 请求超时处理
- 连接 keep-alive
- 请求排队，正在推理时返回忙响应（503）


<details>
<summary>English original</summary>

**HTTP Server**

**Overview**

OpenAI-compatible REST API using raw POSIX sockets — no external dependencies. Sequential request handling (one inference at a time, appropriate for single-GPU Jetson).

**Source: `src/server/http_server.cpp`, `src/main_server.cpp`**

**Starting the Server**

```bash
./build/jetson-llm-server -m models/model.gguf -p 8080

# Options:
#   -m PATH      GGUF model (required)
#   -p PORT      HTTP port (default: 8080)
#   -c INT       Context length (0 = auto)
#   --fp16-kv    Use FP16 KV cache (default: INT8)
```

**Endpoints**

**GET /health**

Returns Jetson-specific system health.

```bash
curl http://jetson:8080/health
```

Response:
```json
{
  "status": "ok",
  "model": "TinyLlama-1.1B-Chat-v1.0",
  "memory": {
    "total_mb": 7633,
    "free_mb": 4200,
    "model_mb": 669,
    "kv_mb": 45
  },
  "thermal": {
    "gpu_c": 48.5,
    "cpu_c": 47.0,
    "throttling": false
  },
  "power": {
    "mode": "25W",
    "gpu_mhz": 1300
  },
  "gpu_util_pct": 75
}
```

**GET /v1/models**

Lists loaded model (OpenAI-compatible).

```bash
curl http://jetson:8080/v1/models
```

Response:
```json
{
  "data": [
    {
      "id": "TinyLlama-1.1B-Chat-v1.0",
      "object": "model"
    }
  ]
}
```

**POST /v1/chat/completions**

OpenAI-compatible chat completion.

```bash
curl http://jetson:8080/v1/chat/completions \
  -H "Content-Type: application/json" \
  -d '{
    "messages": [
      {"role": "user", "content": "What is 2+2?"}
    ],
    "max_tokens": 64
  }'
```

Response:
```json
{
  "id": "jllm-1712345678",
  "object": "chat.completion",
  "model": "TinyLlama-1.1B-Chat-v1.0",
  "choices": [
    {
      "index": 0,
      "message": {
        "role": "assistant",
        "content": "2+2 equals 4."
      },
      "finish_reason": "stop"
    }
  ],
  "usage": {
    "prompt_tokens": 5,
    "completion_tokens": 8,
    "total_tokens": 13
  },
  "jetson": {
    "decode_tok_s": 28.4,
    "peak_mem_mb": 1823,
    "peak_temp_c": 48.2
  }
}
```

Note: the `jetson` field is a non-standard extension with Jetson-specific performance metrics.

**Client Examples**

**Python**

```python
import requests

r = requests.post("http://jetson:8080/v1/chat/completions", json={
    "messages": [{"role": "user", "content": "Hello"}],
    "max_tokens": 64
})
print(r.json()["choices"][0]["message"]["content"])
```

**OpenAI SDK (compatible)**

```python
from openai import OpenAI

client = OpenAI(base_url="http://jetson:8080/v1", api_key="unused")
response = client.chat.completions.create(
    model="tinyllama",
    messages=[{"role": "user", "content": "Hello"}],
    max_tokens=64
)
print(response.choices[0].message.content)
```

**curl (monitoring)**

```bash
# Health check every 5 seconds
watch -n5 'curl -s http://jetson:8080/health | python3 -m json.tool'
```

**Implementation Notes**

- **Single-threaded request handling** — one inference at a time. Jetson has one GPU; concurrent inference would compete for memory and bandwidth.
- **Raw POSIX sockets** — no external HTTP library dependency. Handles the happy path for OpenAI-compatible clients.
- **JSON output** — hand-built with `snprintf` (no JSON library dependency). Strings are escaped for quotes, backslashes, and control characters.
- **CORS headers** — `Access-Control-Allow-Origin: *` enabled for browser clients.
- **Sequential** — `accept()` → `handle_client()` → `close()` → `accept()`. No threading.

**Future Enhancements**

- Streaming SSE (Server-Sent Events) for token-by-token output
- Request timeout handling
- Connection keep-alive
- Request queue with busy response (503) when already inferring

</details>

---

> 原文：[`Projects/jetson-llm-runtime/docs/server.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/docs/server.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
