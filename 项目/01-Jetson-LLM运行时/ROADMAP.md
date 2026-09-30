---
title: jetson-llm 路线图
description: jetson-llm 路线图
---

# jetson-llm 路线图

## 当前状态：v0.1-alpha（代码已完成，待硬件测试）

```
4,500+ lines | 31 files | 0 known bugs | builds clean on Jetson Orin Nano Super
```

---

## v0.1 — 首批 token（目标：第 1 周）

**目标：** 在 Jetson 硬件上用真实 GGUF 模型生成连贯文本。

```
□ Build on Jetson (cmake + nvcc SM 8.7)
□ Run test_memory — verify budget reads /proc/meminfo correctly
□ Run test_kernels — all 5 kernel correctness tests pass
□ Download TinyLlama 1.1B Q4_K_M (669 MB)
□ Run test_model_load — config, tokenizer, weight mapping all pass
□ Run jetson-llm -m tinyllama.gguf -p "What is 2+2?" -n 32
□ Output is coherent English (not garbage/random tokens)
□ No segfaults, no OOM, no CUDA errors
```

**若输出为乱码：** 调试张量偏移映射（model.cpp parse_tensor_infos）。用 Python gguf 库验证张量名与我们的模式是否匹配。

**若 segfault：** 检查 test_model_load 输出中的空权重指针。以 `cuda-memcheck` 运行。

**交付物：** Jetson 上首次连贯生成的截图。

---

## v0.2 — Benchmark 基线（目标：第 2 周）

**目标：** 测量性能并与 llama.cpp 对比。

```
□ Install llama.cpp on same Jetson for baseline comparison
    git clone https://github.com/ggerganov/llama.cpp
    cmake -B build -DGGML_CUDA=ON && cmake --build build -j$(nproc)

□ Run bench.sh with TinyLlama 1.1B — record:
    Prompt eval: ___ tok/s
    Decode:      ___ tok/s
    Peak memory: ___ MB
    Peak temp:   ___ °C

□ Run same model on llama.cpp for comparison:
    ./llama-bench -m tinyllama.gguf -ngl 99

□ Run profile.sh — identify top 3 kernel bottlenecks:
    #1: _____ (___% of time)
    #2: _____ (___% of time)
    #3: _____ (___% of time)

□ Graduate to Llama 3.2 3B Q4_K_M — repeat benchmarks
□ Test context lengths: 512, 1024, 2048 — record tok/s at each
□ Test power modes: 7W, 15W, 25W — record tok/s and tokens/joule
```

**交付物：** 性能对比表（jetson-llm vs llama.cpp）。

---

## v0.3 — Kernel 优化（目标：第 3–4 周）

**目标：** 在 Llama 3.2 3B 上，decode（逐 token 生成阶段）比原版 llama.cpp 快 20% 以上。

```
Based on profiling results from v0.2:

□ Optimize #1 bottleneck kernel (likely gemv_q4):
    □ Tune thread block size (try 64, 128, 256)
    □ Tune elements per thread (try 4, 8, 16)
    □ Add vectorized loads (float4 / int4)
    □ Profile: register count, occupancy, memory throughput

□ Optimize #2 bottleneck kernel (likely attention):
    □ Tune ATTN_TILE_KV (try 32, 64, 128)
    □ Test INT8 vs FP16 KV cache performance difference
    □ Profile shared memory utilization

□ Optimize #3 bottleneck kernel (likely fused_norm):
    □ Verify fusion is working (compare 1-kernel vs 3-kernel time)
    □ Try different block sizes for different hidden_dim

□ Enable CUDA graphs for decode loop:
    □ Verify graph capture works (no host-side ops inside capture)
    □ Measure launch overhead reduction (before/after)

□ Re-run bench.sh — measure improvement:
    Before: ___ tok/s (decode)
    After:  ___ tok/s (decode)
    Speedup: ___×
```

**交付物：** 相对 v0.2 的 decode 提速 >20%，以及性能剖析证据。

---

## v0.4 — 内存稳定性（目标：第 5 周）

**目标：** 稳定运行 1000+ token 且内存无增长。

```
□ Generate 1000 tokens continuously — monitor memory:
    tegrastats --interval 1000 | tee stability.log &
    ./jetson-llm -m model.gguf -p "Write a long essay..." -n 1000

□ Verify no memory growth:
    Plot RAM usage over time from stability.log
    Delta between token 10 and token 1000 should be < 5 MB

□ Test KV cache eviction:
    Set context limit to 512, generate 1000 tokens
    Verify overflow pool works (eviction messages in stderr)

□ Test OOM guard:
    Load Llama 3.3 8B Q4_K_M (4.6 GB — tight fit)
    Verify OOM guard stops gracefully (no crash, no OOM killer)
    Verify message: "[oom_guard] Stopping at token N"

□ Test thermal stability:
    Run 30 minutes continuous generation at 25W
    Monitor temperature — should stay below 85°C with active cooling
    Verify thermal backoff activates if temp exceeds 80°C

□ Stress test: rapid start/stop:
    for i in {1..100}; do
      ./jetson-llm -m model.gguf -p "Hi" -n 10 2>/dev/null
    done
    Verify no memory leak (free -m before and after)
```

**交付物：** 稳定性报告 —— 内存曲线、热管理曲线、OOM 测试结果。

---


<details>
<summary>English original</summary>

**jetson-llm Roadmap**

**Current State: v0.1-alpha (code-complete, needs hardware testing)**

```
4,500+ lines | 31 files | 0 known bugs | builds clean on Jetson Orin Nano Super
```

---

**v0.1 — First Tokens (Target: Week 1)**

**Goal:** generate coherent text from a real GGUF model on Jetson hardware.

```
□ Build on Jetson (cmake + nvcc SM 8.7)
□ Run test_memory — verify budget reads /proc/meminfo correctly
□ Run test_kernels — all 5 kernel correctness tests pass
□ Download TinyLlama 1.1B Q4_K_M (669 MB)
□ Run test_model_load — config, tokenizer, weight mapping all pass
□ Run jetson-llm -m tinyllama.gguf -p "What is 2+2?" -n 32
□ Output is coherent English (not garbage/random tokens)
□ No segfaults, no OOM, no CUDA errors
```

**If output is garbage:** debug tensor offset mapping (model.cpp parse_tensor_infos). Use Python gguf library to verify tensor names match our patterns.

**If segfault:** check null weight pointers in test_model_load output. Run with `cuda-memcheck`.

**Deliverable:** screenshot of first coherent generation on Jetson.

---

**v0.2 — Benchmark Baseline (Target: Week 2)**

**Goal:** measure performance and compare against llama.cpp.

```
□ Install llama.cpp on same Jetson for baseline comparison
    git clone https://github.com/ggerganov/llama.cpp
    cmake -B build -DGGML_CUDA=ON && cmake --build build -j$(nproc)

□ Run bench.sh with TinyLlama 1.1B — record:
    Prompt eval: ___ tok/s
    Decode:      ___ tok/s
    Peak memory: ___ MB
    Peak temp:   ___ °C

□ Run same model on llama.cpp for comparison:
    ./llama-bench -m tinyllama.gguf -ngl 99

□ Run profile.sh — identify top 3 kernel bottlenecks:
    #1: _____ (___% of time)
    #2: _____ (___% of time)
    #3: _____ (___% of time)

□ Graduate to Llama 3.2 3B Q4_K_M — repeat benchmarks
□ Test context lengths: 512, 1024, 2048 — record tok/s at each
□ Test power modes: 7W, 15W, 25W — record tok/s and tokens/joule
```

**Deliverable:** performance comparison table (jetson-llm vs llama.cpp).

---

**v0.3 — Kernel Optimization (Target: Week 3–4)**

**Goal:** >20% faster decode than stock llama.cpp on Llama 3.2 3B.

```
Based on profiling results from v0.2:

□ Optimize #1 bottleneck kernel (likely gemv_q4):
    □ Tune thread block size (try 64, 128, 256)
    □ Tune elements per thread (try 4, 8, 16)
    □ Add vectorized loads (float4 / int4)
    □ Profile: register count, occupancy, memory throughput

□ Optimize #2 bottleneck kernel (likely attention):
    □ Tune ATTN_TILE_KV (try 32, 64, 128)
    □ Test INT8 vs FP16 KV cache performance difference
    □ Profile shared memory utilization

□ Optimize #3 bottleneck kernel (likely fused_norm):
    □ Verify fusion is working (compare 1-kernel vs 3-kernel time)
    □ Try different block sizes for different hidden_dim

□ Enable CUDA graphs for decode loop:
    □ Verify graph capture works (no host-side ops inside capture)
    □ Measure launch overhead reduction (before/after)

□ Re-run bench.sh — measure improvement:
    Before: ___ tok/s (decode)
    After:  ___ tok/s (decode)
    Speedup: ___×
```

**Deliverable:** >20% decode speedup over v0.2, profiling evidence.

---

**v0.4 — Memory Stability (Target: Week 5)**

**Goal:** stable operation for 1000+ tokens with no memory growth.

```
□ Generate 1000 tokens continuously — monitor memory:
    tegrastats --interval 1000 | tee stability.log &
    ./jetson-llm -m model.gguf -p "Write a long essay..." -n 1000

□ Verify no memory growth:
    Plot RAM usage over time from stability.log
    Delta between token 10 and token 1000 should be < 5 MB

□ Test KV cache eviction:
    Set context limit to 512, generate 1000 tokens
    Verify overflow pool works (eviction messages in stderr)

□ Test OOM guard:
    Load Llama 3.3 8B Q4_K_M (4.6 GB — tight fit)
    Verify OOM guard stops gracefully (no crash, no OOM killer)
    Verify message: "[oom_guard] Stopping at token N"

□ Test thermal stability:
    Run 30 minutes continuous generation at 25W
    Monitor temperature — should stay below 85°C with active cooling
    Verify thermal backoff activates if temp exceeds 80°C

□ Stress test: rapid start/stop:
    for i in {1..100}; do
      ./jetson-llm -m model.gguf -p "Hi" -n 10 2>/dev/null
    done
    Verify no memory leak (free -m before and after)
```

**Deliverable:** stability report — memory graph, thermal graph, OOM test results.

---

</details>

## v0.5 — 服务器 + 流式传输（目标：第 6 周）

**目标：** 可投入生产的 HTTP API，带流式传输。

```
□ Implement streaming SSE in http_server.cpp:
    POST /v1/chat/completions with "stream": true
    Returns: data: {"choices":[{"delta":{"content":"token"}}]}\n\n
    Final: data: [DONE]\n\n

□ Add chat template formatting:
    Llama: <|begin_of_text|><|start_header_id|>user<|end_header_id|>\n{prompt}<|eot_id|>
    Generic: <|user|>\n{prompt}\n<|assistant|>\n

□ Add request timeout (60 second default)

□ Test with real clients:
    □ curl with streaming: curl -N http://jetson:8080/v1/chat/completions ...
    □ Python OpenAI SDK: client.chat.completions.create(stream=True)
    □ Browser fetch with ReadableStream

□ Add systemd service file:
    /etc/systemd/system/jetson-llm.service
    Restart=always, After=network.target
    EnvironmentFile for model path and port

□ Auto-start on boot, auto-restart on crash

□ Health endpoint enhanced:
    Add: uptime, total_requests, avg_tok_s, kv_cache_tokens_used
```

**交付物：** 流式 API 可与 OpenAI SDK 配合工作，systemd 服务运行中。

---

## v0.6 — 多模型支持（目标：第 7–8 周）

**目标：** 测试并记录所有 Tier 1–2 模型的性能。

```
□ Test each model, record in performance table:

    Model              | Q4_K_M | Decode tok/s | Prompt tok/s | Peak RAM | Max ctx
    ────────────────────────────────────────────────────────────────────────────
    TinyLlama 1.1B     | 669 MB |    ___       |    ___       |   ___    |  ___
    Llama 3.2 1B       | 750 MB |    ___       |    ___       |   ___    |  ___
    Gemma 2 2B          | 1.5 GB |    ___       |    ___       |   ___    |  ___
    Qwen 3 1.7B        | 1.0 GB |    ___       |    ___       |   ___    |  ___
    Llama 3.2 3B       | 1.8 GB |    ___       |    ___       |   ___    |  ___
    Phi-4 Mini 3.8B    | 2.3 GB |    ___       |    ___       |   ___    |  ___
    Gemma 3 4B          | 2.5 GB |    ___       |    ___       |   ___    |  ___
    Llama 3.3 8B       | 4.6 GB |    ___       |    ___       |   ___    |  ___

□ Verify tokenizer works for each model family:
    □ Llama tokenizer (BPE)
    □ Gemma tokenizer (SentencePiece)
    □ Qwen tokenizer (tiktoken-style)
    □ Phi tokenizer

□ Document any model-specific issues:
    □ Different tensor name patterns?
    □ Different RoPE theta values?
    □ GQA group sizes?

□ Update README with tested models + performance table
```

**交付物：** 覆盖 8 个以上模型的性能表，全部验证可用。

---

## v0.7 — 投机解码（目标：第 9–10 周）

**目标：** 使用 draft model 使 decode（逐 token 生成阶段）提速 1.5–2×。

```
□ Implement speculative decode:
    Draft model: TinyLlama 1.1B (fast, ~65 tok/s)
    Target model: Llama 3.2 3B (slow, ~25 tok/s)

    Algorithm:
    1. Draft generates N candidate tokens (N=4–8)
    2. Target verifies all N in one forward pass
    3. Accept matching tokens, reject from first mismatch
    4. Expected: accept 60–80% → 1.5–2× speedup

□ Memory budget for both models:
    Draft:  ~670 MB (TinyLlama 1.1B Q4_K_M)
    Target: ~1.8 GB (Llama 3.2 3B Q4_K_M)
    KV caches × 2: ~200 MB
    Total:  ~2.7 GB → fits in 5.5 GB available ✓

□ Implement draft-verify loop in engine
□ Share tokenizer between draft and target
□ Measure acceptance rate at draft lengths 2, 4, 6, 8
□ Measure effective tok/s vs single-model decode
□ Add --draft-model CLI flag
```

**交付物：** 投机解码可用，加速比已实测。

---

## v0.8 — 多轮对话（目标：第 11 周）

**目标：** 跨轮次保持 KV cache 持久化。

```
□ Implement conversation state:
    Keep KV cache between turns (don't re-prefill system prompt)
    Only prefill new user message each turn

□ System prompt support:
    Pre-load system prompt into KV cache at startup
    New turns only process user message + generate response

□ Context window management:
    When KV cache exceeds limit:
    Option A: truncate oldest messages (sliding window)
    Option B: summarize old context (future)

□ Conversation API:
    POST /v1/chat/completions with messages array
    Server maintains conversation_id → KV cache state

□ Test: 10-turn conversation
    Verify later turns reference earlier context
    Verify memory stable across turns
    Measure: time-to-first-token for turn 1 vs turn 10
```

**交付物：** 多轮对话可用，KV 复用已验证。

---


<details>
<summary>English original</summary>

**v0.5 — Server + Streaming (Target: Week 6)**

**Goal:** production-ready HTTP API with streaming.

```
□ Implement streaming SSE in http_server.cpp:
    POST /v1/chat/completions with "stream": true
    Returns: data: {"choices":[{"delta":{"content":"token"}}]}\n\n
    Final: data: [DONE]\n\n

□ Add chat template formatting:
    Llama: <|begin_of_text|><|start_header_id|>user<|end_header_id|>\n{prompt}<|eot_id|>
    Generic: <|user|>\n{prompt}\n<|assistant|>\n

□ Add request timeout (60 second default)

□ Test with real clients:
    □ curl with streaming: curl -N http://jetson:8080/v1/chat/completions ...
    □ Python OpenAI SDK: client.chat.completions.create(stream=True)
    □ Browser fetch with ReadableStream

□ Add systemd service file:
    /etc/systemd/system/jetson-llm.service
    Restart=always, After=network.target
    EnvironmentFile for model path and port

□ Auto-start on boot, auto-restart on crash

□ Health endpoint enhanced:
    Add: uptime, total_requests, avg_tok_s, kv_cache_tokens_used
```

**Deliverable:** streaming API working with OpenAI SDK, systemd service running.

---

**v0.6 — Multi-Model Support (Target: Week 7–8)**

**Goal:** test and document performance across all Tier 1–2 models.

```
□ Test each model, record in performance table:

    Model              | Q4_K_M | Decode tok/s | Prompt tok/s | Peak RAM | Max ctx
    ────────────────────────────────────────────────────────────────────────────
    TinyLlama 1.1B     | 669 MB |    ___       |    ___       |   ___    |  ___
    Llama 3.2 1B       | 750 MB |    ___       |    ___       |   ___    |  ___
    Gemma 2 2B          | 1.5 GB |    ___       |    ___       |   ___    |  ___
    Qwen 3 1.7B        | 1.0 GB |    ___       |    ___       |   ___    |  ___
    Llama 3.2 3B       | 1.8 GB |    ___       |    ___       |   ___    |  ___
    Phi-4 Mini 3.8B    | 2.3 GB |    ___       |    ___       |   ___    |  ___
    Gemma 3 4B          | 2.5 GB |    ___       |    ___       |   ___    |  ___
    Llama 3.3 8B       | 4.6 GB |    ___       |    ___       |   ___    |  ___

□ Verify tokenizer works for each model family:
    □ Llama tokenizer (BPE)
    □ Gemma tokenizer (SentencePiece)
    □ Qwen tokenizer (tiktoken-style)
    □ Phi tokenizer

□ Document any model-specific issues:
    □ Different tensor name patterns?
    □ Different RoPE theta values?
    □ GQA group sizes?

□ Update README with tested models + performance table
```

**Deliverable:** performance table with 8+ models, all verified working.

---

**v0.7 — Speculative Decoding (Target: Week 9–10)**

**Goal:** 1.5–2× faster decode using draft model.

```
□ Implement speculative decode:
    Draft model: TinyLlama 1.1B (fast, ~65 tok/s)
    Target model: Llama 3.2 3B (slow, ~25 tok/s)

    Algorithm:
    1. Draft generates N candidate tokens (N=4–8)
    2. Target verifies all N in one forward pass
    3. Accept matching tokens, reject from first mismatch
    4. Expected: accept 60–80% → 1.5–2× speedup

□ Memory budget for both models:
    Draft:  ~670 MB (TinyLlama 1.1B Q4_K_M)
    Target: ~1.8 GB (Llama 3.2 3B Q4_K_M)
    KV caches × 2: ~200 MB
    Total:  ~2.7 GB → fits in 5.5 GB available ✓

□ Implement draft-verify loop in engine
□ Share tokenizer between draft and target
□ Measure acceptance rate at draft lengths 2, 4, 6, 8
□ Measure effective tok/s vs single-model decode
□ Add --draft-model CLI flag
```

**Deliverable:** speculative decoding working, measured speedup.

---

**v0.8 — Multi-Turn Conversation (Target: Week 11)**

**Goal:** KV cache persistence across turns.

```
□ Implement conversation state:
    Keep KV cache between turns (don't re-prefill system prompt)
    Only prefill new user message each turn

□ System prompt support:
    Pre-load system prompt into KV cache at startup
    New turns only process user message + generate response

□ Context window management:
    When KV cache exceeds limit:
    Option A: truncate oldest messages (sliding window)
    Option B: summarize old context (future)

□ Conversation API:
    POST /v1/chat/completions with messages array
    Server maintains conversation_id → KV cache state

□ Test: 10-turn conversation
    Verify later turns reference earlier context
    Verify memory stable across turns
    Measure: time-to-first-token for turn 1 vs turn 10
```

**Deliverable:** multi-turn chat working, KV reuse verified.

---

</details>

## v1.0 — 生产发布（目标：第 12 周）

**目标：** 稳定、有文档、可部署。

```
□ 24-hour stability test:
    Continuous requests every 5 seconds for 24 hours
    No OOM, no crash, no thermal shutdown
    Memory delta < 10 MB over 24 hours

□ Documentation complete:
    □ README: features, quickstart, performance table
    □ docs/: all 10 documents up to date
    □ TESTING.md: reflects actual test results
    □ CHANGELOG.md: all versions documented

□ Packaging:
    □ Single tar.gz with binary + scripts + docs
    □ Or: Dockerfile for Jetson (l4t-base image)

□ Release:
    □ Tag v1.0.0
    □ GitHub release with binary + docs
    □ Blog post: "Building a Memory-First LLM Runtime for Jetson"
```

**交付物：** 打上 v1.0.0 标签的发布版本，24 小时稳定性已验证。

---

## 后续规划（v1.0 之后）

| 特性 | 描述 | 工作量 |
|---------|-------------|--------|
| **INT4 KV cache** | KV 内存减半 → 上下文翻倍 | 1 周 |
| **Tensor Core prefill** | prefill（首字前的整段计算）期间用 WMMA 做批量矩阵乘 | 2 周 |
| **视觉-语言模型** | Gemma 3 4B 图像+文本 | 2 周 |
| **DLA 卸载** | 部分 layer 跑在 DLA 上，把 GPU 让给其他 layer | 2 周 |
| **多模型并发** | 无需重启即可热切换模型 | 1 周 |
| **WebSocket API** | 实时双向流式传输 | 1 周 |
| **ONNX Runtime 回退** | 支持非 GGUF 模型 | 2 周 |
| **Jetson Orin NX 16 GB** | 扩展到更大的 Jetson，跑 7B 模型 | 1 周 |
| **Benchmark 面板** | 实时 Grafana/Prometheus 指标 | 1 周 |
| **端侧模型量化** | 直接在 Jetson 上把 FP16 量化到 INT4 | 2 周 |

---

## 时间线总结

```
Week 1:   v0.1  First Tokens         □ build → test → first coherent output
Week 2:   v0.2  Benchmark            □ measure → profile → compare vs llama.cpp
Week 3-4: v0.3  Kernel Optimization  □ tune top 3 kernels → >20% speedup
Week 5:   v0.4  Memory Stability     □ 1000 tokens stable → OOM guard → thermal
Week 6:   v0.5  Server + Streaming   □ SSE → chat templates → systemd
Week 7-8: v0.6  Multi-Model          □ test 8+ models → performance table
Week 9-10:v0.7  Speculative Decode   □ draft+target → 1.5-2× speedup
Week 11:  v0.8  Multi-Turn           □ KV persistence → conversation state
Week 12:  v1.0  Production Release   □ 24-hour test → package → release
```


<details>
<summary>English original</summary>

**v1.0 — Production Release (Target: Week 12)**

**Goal:** stable, documented, deployable.

```
□ 24-hour stability test:
    Continuous requests every 5 seconds for 24 hours
    No OOM, no crash, no thermal shutdown
    Memory delta < 10 MB over 24 hours

□ Documentation complete:
    □ README: features, quickstart, performance table
    □ docs/: all 10 documents up to date
    □ TESTING.md: reflects actual test results
    □ CHANGELOG.md: all versions documented

□ Packaging:
    □ Single tar.gz with binary + scripts + docs
    □ Or: Dockerfile for Jetson (l4t-base image)

□ Release:
    □ Tag v1.0.0
    □ GitHub release with binary + docs
    □ Blog post: "Building a Memory-First LLM Runtime for Jetson"
```

**Deliverable:** tagged v1.0.0 release, 24-hour stability proven.

---

**Future (Post v1.0)**

| Feature | Description | Effort |
|---------|-------------|--------|
| **INT4 KV cache** | Halve KV memory → 2× more context | 1 week |
| **Tensor Core prefill** | WMMA for batch matmul during prefill | 2 weeks |
| **Vision-language models** | Gemma 3 4B image+text | 2 weeks |
| **DLA offload** | Run some layers on DLA, free GPU for others | 2 weeks |
| **Multiple concurrent models** | Hot-swap models without restart | 1 week |
| **WebSocket API** | Real-time bidirectional streaming | 1 week |
| **ONNX Runtime fallback** | Support non-GGUF models | 2 weeks |
| **Jetson Orin NX 16 GB** | Extend to larger Jetson for 7B models | 1 week |
| **Benchmark dashboard** | Live Grafana/Prometheus metrics | 1 week |
| **Model quantization on-device** | Quantize FP16→INT4 directly on Jetson | 2 weeks |

---

**Timeline Summary**

```
Week 1:   v0.1  First Tokens         □ build → test → first coherent output
Week 2:   v0.2  Benchmark            □ measure → profile → compare vs llama.cpp
Week 3-4: v0.3  Kernel Optimization  □ tune top 3 kernels → >20% speedup
Week 5:   v0.4  Memory Stability     □ 1000 tokens stable → OOM guard → thermal
Week 6:   v0.5  Server + Streaming   □ SSE → chat templates → systemd
Week 7-8: v0.6  Multi-Model          □ test 8+ models → performance table
Week 9-10:v0.7  Speculative Decode   □ draft+target → 1.5-2× speedup
Week 11:  v0.8  Multi-Turn           □ KV persistence → conversation state
Week 12:  v1.0  Production Release   □ 24-hour test → package → release
```

</details>

---

> 原文：[`Projects/jetson-llm-runtime/ROADMAP.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/ROADMAP.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
