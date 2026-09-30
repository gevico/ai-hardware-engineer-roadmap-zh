---
title: Engine
description: Engine
---

# Engine

## 生命周期

```
Engine engine;
engine.load("model.gguf", params);   // parse GGUF, mmap, allocate pools
engine.generate("Hello", params, cb); // tokenize → prefill → decode → stream
engine.unload();                      // free everything
```

## load()

1. 探测系统内存（`probe_system_memory()`）
2. 解析 GGUF 配置（`load_gguf_config()`）
3. 加载并映射权重（`load_and_map_weights()`）— mmap + cudaHostRegister + 张量名匹配
4. 根据剩余内存自动计算最大上下文
5. 分配 KV cache 池（pinned 快路径 + unpinned 溢出）
6. 分配 scratch 池（bump allocator）
7. 创建 CUDA 流
8. 从 GGUF 加载 tokenizer
9. 打印内存预算

## Transformer Layer

`transformer_layer(layer, pos, x)` — 每层 12 个操作：

```
Input: x [hidden_dim] — hidden state from previous layer

┌─ Attention Block ────────────────────────────────────┐
│  1. normed = RMSNorm(x) × attn_weight               │
│  2. Q = gemv_q4(W_q, normed)                         │
│  3. K = gemv_q4(W_k, normed)                         │
│  4. V = gemv_q4(W_v, normed)                         │
│  5. RoPE(Q, K, position)                              │
│  6. KV cache store (INT8 quantize if enabled)         │
│  7. attn_out = flash_attention(Q, K_cache, V_cache)   │
│  8. attn_proj = gemv_q4(W_o, attn_out)                │
│  9. x2 = x + attn_proj              ← residual #1    │
└──────────────────────────────────────────────────────┘

┌─ FFN Block ──────────────────────────────────────────┐
│  10. normed2 = RMSNorm(x2) × ffn_weight              │
│  11. gate = gemv_q4(W_gate, normed2)                  │
│  12. up = gemv_q4(W_up, normed2)                      │
│  13. swiglu_out = silu(gate) × up                     │
│  14. ffn_out = gemv_q4(W_down, swiglu_out)            │
│  15. x = x2 + ffn_out               ← residual #2    │
└──────────────────────────────────────────────────────┘

Output: x [hidden_dim] — input to next layer
```

所有中间缓冲区都从 `ScratchPool` 分配（每个 decode 步骤重置）。

## decode 步骤（逐 token 生成阶段）

`decode_step(pos)` — 一次 token 生成：

1. 从 scratch 取 hidden state 缓冲区
2. embedding 查表：`cudaMemcpyAsync(x, tok_embd + token_id × hidden_dim)`
3. 对全部 N 个 layer 运行 `transformer_layer()`
4. 最终 RMSNorm
5. logit 投影：`gemv_q4(W_output, normed)` → FP16
6. 在 GPU 上把 FP16 → FP32（`fp16_to_fp32` kernel）
7. 把 FP32 logits 拷贝到 CPU（`cudaMemcpy D2H`）
8. 采样：`sample_token(logits, vocab_size, params)`
9. 更新最近 token（用于重复惩罚）
10. 返回 token ID

## 生成循环

`generate(prompt, params, callback)`：

```
Tokenize prompt → token IDs
│
├── Prefill phase:
│   For each prompt token:
│     scratch.reset()
│     embedding lookup + all transformer layers
│     (builds KV cache, no sampling)
│
├── Decode phase:
│   For each output token (up to max_tokens):
│     check_memory_and_thermal()  ← OOM guard + thermal
│     scratch.reset()
│     token = decode_step(pos)
│     callback(detokenized_text, is_eos)
│     if EOS: break
│
└── Return GenStats
```

## CUDA Graphs

`build_cuda_graph(pos)` 把 GPU 侧的 decode 步骤捕获为 CUDA graph：

- 捕获全部 Transformer layer + 最终 norm + logit 投影
- 后续 token 用 `cudaGraphLaunch()` 重放
- 把每 token 的 kernel 启动开销从 ~1ms 降到 ~5μs
- 不捕获：embedding 查表（host→device）、采样（host 侧）
- KV cache 结构变化时必须重建 graph

## 采样

`sample_token()`（`src/engine/sample.cpp`）— CPU 侧 token 选择：

1. 应用重复惩罚（惩罚最近的 token）
2. 应用 temperature（logits 除以 T）
3. 若 T=0：贪心（argmax）
4. 在 CPU 上做 softmax（logits 很小：vocab_size × 4 bytes）
5. Top-K 过滤（保留最高的 K 个，partial sort）
6. Top-P 过滤（保留至累计概率 > P）
7. 从过滤后的分布中随机采样

## tokenizer

`Tokenizer`（`src/engine/tokenizer.cpp`）：

### 编码

使用 hash map `token_to_id_`，每个位置 O(max_token_len)：
1. 每个位置先尝试最长匹配（长度递减）
2. 对每个候选子串做 hash map 查找
3. 若无匹配：byte fallback（`<0x41>` → byte token）

### 解码

直接查表：`vocab[token_id]`。处理 byte token（`<0xNN>` → 实际 byte）。

## 停止机制

`engine.stop()` 设置 `stop_flag_ = true`。decode 循环每次迭代都检查它并干净退出。用于：
- SIGINT handler（CLI 中的 Ctrl+C）
- HTTP 请求取消
- 超时


<details>
<summary>English original</summary>

**Engine**

**Lifecycle**

```
Engine engine;
engine.load("model.gguf", params);   // parse GGUF, mmap, allocate pools
engine.generate("Hello", params, cb); // tokenize → prefill → decode → stream
engine.unload();                      // free everything
```

**load()**

1. Probe system memory (`probe_system_memory()`)
2. Parse GGUF config (`load_gguf_config()`)
3. Load and map weights (`load_and_map_weights()`) — mmap + cudaHostRegister + tensor name matching
4. Auto-calculate max context from remaining memory
5. Allocate KV cache pool (pinned fast + unpinned overflow)
6. Allocate scratch pool (bump allocator)
7. Create CUDA stream
8. Load tokenizer from GGUF
9. Print memory budget

**Transformer Layer**

`transformer_layer(layer, pos, x)` — 12 operations per layer:

```
Input: x [hidden_dim] — hidden state from previous layer

┌─ Attention Block ────────────────────────────────────┐
│  1. normed = RMSNorm(x) × attn_weight               │
│  2. Q = gemv_q4(W_q, normed)                         │
│  3. K = gemv_q4(W_k, normed)                         │
│  4. V = gemv_q4(W_v, normed)                         │
│  5. RoPE(Q, K, position)                              │
│  6. KV cache store (INT8 quantize if enabled)         │
│  7. attn_out = flash_attention(Q, K_cache, V_cache)   │
│  8. attn_proj = gemv_q4(W_o, attn_out)                │
│  9. x2 = x + attn_proj              ← residual #1    │
└──────────────────────────────────────────────────────┘

┌─ FFN Block ──────────────────────────────────────────┐
│  10. normed2 = RMSNorm(x2) × ffn_weight              │
│  11. gate = gemv_q4(W_gate, normed2)                  │
│  12. up = gemv_q4(W_up, normed2)                      │
│  13. swiglu_out = silu(gate) × up                     │
│  14. ffn_out = gemv_q4(W_down, swiglu_out)            │
│  15. x = x2 + ffn_out               ← residual #2    │
└──────────────────────────────────────────────────────┘

Output: x [hidden_dim] — input to next layer
```

All intermediate buffers allocated from `ScratchPool` (reset each decode step).

**Decode Step**

`decode_step(pos)` — one token generation:

1. Get hidden state buffer from scratch
2. Embedding lookup: `cudaMemcpyAsync(x, tok_embd + token_id × hidden_dim)`
3. Run `transformer_layer()` for all N layers
4. Final RMSNorm
5. Logit projection: `gemv_q4(W_output, normed)` → FP16
6. Convert FP16 → FP32 on GPU (`fp16_to_fp32` kernel)
7. Copy FP32 logits to CPU (`cudaMemcpy D2H`)
8. Sample: `sample_token(logits, vocab_size, params)`
9. Update recent tokens (for repeat penalty)
10. Return token ID

**Generation Loop**

`generate(prompt, params, callback)`:

```
Tokenize prompt → token IDs
│
├── Prefill phase:
│   For each prompt token:
│     scratch.reset()
│     embedding lookup + all transformer layers
│     (builds KV cache, no sampling)
│
├── Decode phase:
│   For each output token (up to max_tokens):
│     check_memory_and_thermal()  ← OOM guard + thermal
│     scratch.reset()
│     token = decode_step(pos)
│     callback(detokenized_text, is_eos)
│     if EOS: break
│
└── Return GenStats
```

**CUDA Graphs**

`build_cuda_graph(pos)` captures the GPU-side decode step as a CUDA graph:

- All transformer layers + final norm + logit projection captured
- Replayed with `cudaGraphLaunch()` for subsequent tokens
- Reduces kernel launch overhead from ~1ms to ~5μs per token
- Not captured: embedding lookup (host→device), sampling (host-side)
- Graph must be rebuilt if KV cache structure changes

**Sampling**

`sample_token()` (`src/engine/sample.cpp`) — CPU-side token selection:

1. Apply repeat penalty (penalize recent tokens)
2. Apply temperature (divide logits by T)
3. If T=0: greedy (argmax)
4. Softmax on CPU (logits are small: vocab_size × 4 bytes)
5. Top-K filter (keep K highest, partial sort)
6. Top-P filter (keep until cumulative probability > P)
7. Random sample from filtered distribution

**Tokenizer**

`Tokenizer` (`src/engine/tokenizer.cpp`):

**Encoding**

Uses hash map `token_to_id_` for O(max_token_len) per position:
1. At each position, try longest match first (decreasing length)
2. Hash map lookup for each candidate substring
3. If no match: byte fallback (`<0x41>` → byte token)

**Decoding**

Direct lookup: `vocab[token_id]`. Handles byte tokens (`<0xNN>` → actual byte).

**Stop Mechanism**

`engine.stop()` sets `stop_flag_ = true`. The decode loop checks this every iteration and breaks cleanly. Used for:
- SIGINT handler (Ctrl+C in CLI)
- HTTP request cancellation
- Timeout

</details>

## GenStats

生成结束后返回：

| 字段 | 描述 |
|-------|-------------|
| `prompt_tokens` | 已处理的 prompt token 数 |
| `completion_tokens` | 生成的 token 数 |
| `prompt_ms` | prefill（首字前的整段计算）阶段耗时 |
| `decode_ms` | decode（逐 token 生成阶段）阶段耗时 |
| `prompt_tok_per_sec` | Prefill 吞吐 |
| `decode_tok_per_sec` | Decode 吞吐 |
| `peak_memory_mb` | 观测到的最大内存占用 |
| `peak_thermal_c` | 观测到的最高 GPU 温度 |
| `oom_stops` | OOM guard 停止生成的次数 |
| `thermal_pauses` | thermal backoff 触发的次数 |


<details>
<summary>English original</summary>

**GenStats**

Returned after generation:

| Field | Description |
|-------|-------------|
| `prompt_tokens` | Number of prompt tokens processed |
| `completion_tokens` | Number of tokens generated |
| `prompt_ms` | Time for prefill phase |
| `decode_ms` | Time for decode phase |
| `prompt_tok_per_sec` | Prefill throughput |
| `decode_tok_per_sec` | Decode throughput |
| `peak_memory_mb` | Maximum memory usage observed |
| `peak_thermal_c` | Maximum GPU temperature observed |
| `oom_stops` | Times OOM guard stopped generation |
| `thermal_pauses` | Times thermal backoff triggered |

</details>

---

> 原文：[`Projects/jetson-llm-runtime/docs/engine.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/docs/engine.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
