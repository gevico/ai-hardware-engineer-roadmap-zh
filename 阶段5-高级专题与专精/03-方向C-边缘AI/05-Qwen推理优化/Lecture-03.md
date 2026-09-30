---
title: 第 3 讲：Jetson Orin Nano 上 Qwen3-4B 的 decode（逐 token 生成阶段）优化
description: 第 3 讲：Jetson Orin Nano 上 Qwen3-4B 的 decode（逐 token 生成阶段）优化
---

# 第 3 讲：Jetson Orin Nano 上 Qwen3-4B 的 decode（逐 token 生成阶段）优化

## 概览

现在你手上有了 Qwen3-4B-Q4_K_M GGUF，以及一个能加载它的 runtime。之前的 JLLM 日志显示它以 **0.2 tok/s** 运行。Orin Nano 的 roofline（性能上界模型）表明，这个模型应该能跑到 **~14–20 tok/s**。本讲按收益从大到小的顺序逐一讲解各项优化，把这个 70–100× 的差距补上：

1. 修好平台配置（`nvpmodel`、`jetson_clocks`）。
2. 确认实际走的是 CUDA 路径。
3. 融合 QKV 与 gate/up，把 kernel 启动次数减半。
4. 用 CUDA Graphs 压掉每 token 的启动开销。
5. 上下文变长后把 KV cache 量化到 INT8。
6. 加上投机解码，拿下最后的 1.5×。

每一步都很具体：形状算术、kernel 形状变化，以及该测什么。

学完本讲，你应该能够：

* 拿到一份 JLLM 风格的 trace，找出收益最大的那一项改动。
* 估算每 token 带宽，在运行之前就预测出 tok/s。
* 配置融合 QKV 路径，并在 `nsys` 中确认 kernel 数量下降。
* 把 CUDA Graphs 用到 decode 热路径上，并量化收益。

---

## 1. 平台配置 —— 第一个 50× 收益

动手之前，先**把平台定下来**。Orin Nano 8 GB 有三种功耗模式；对内存带宽受限的工作负载，模式 0（15 W）与模式 1（7 W）之间的 tok/s 差约 **3×**。

```bash
# Step 1: maximum performance
sudo nvpmodel -m 0
sudo jetson_clocks

# Step 2: confirm
sudo jetson_clocks --show
# Expect:
#   GPU MinFreq=624000000 MaxFreq=624000000   ← pinned (Nano max) or 1020 MHz on AGX
#   EMC MinFreq=2133000000 MaxFreq=2133000000 ← max memory clock
#   CPU Cluster 0: MinFreq=N MaxFreq=N         ← pinned

# Step 3: watch during inference
tegrastats --interval 500
# Expect during decode:
#   GR3D_FREQ 80–100% (GPU 3D engine busy)
#   EMC_FREQ  near 100% (you're bandwidth-bound)
```

如果推理期间 `GR3D_FREQ` 一直是 0%，说明 GPU 根本没被用上。这说明问题出在 CUDA 构建，而不是配置 —— 回到第 0 步。

如果 `GR3D_FREQ` 很高而 `EMC_FREQ` 远低于 100%，说明你的 kernel 算力受限的原因不是权重读取 —— 这对 4B Q4 模型并不常见，通常意味着激活值工作集很大（长上下文、大的中间缓冲区）。

**仅做完这一步就应该立刻看到的结果：** 在 Qwen3-4B-Q4_K_M 上 0.2 tok/s → ~10 tok/s。

---

## 2. decode 热路径

Qwen3-4B 上用朴素（未融合）GEMV（矩阵-向量乘）派发，生成单个 token：

```
Per layer (× 36):
  1.  fused_rmsnorm_residual   (attn input norm)
  2.  gemv_q4k  Q              M=4096, K=2560, ~2.6 MB weight read
  3.  gemv_q4k  K              M=1024, K=2560, ~0.7 MB
  4.  gemv_q6k  V              M=1024, K=2560, ~0.9 MB
  5.  rope_kernel              ~0
  6.  kv append                ~0
  7.  flash_attention_decode   reads KV cache slice
  8.  gemv_q4k  O              M=2560, K=4096, ~2.6 MB
  9.  fused_rmsnorm_residual   (ffn norm)
  10. gemv_q4k  gate           M=6912, K=2560, ~4.4 MB
  11. gemv_q4k  up             M=6912, K=2560, ~4.4 MB
  12. swiglu_kernel            ~0
  13. gemv_q6k  down           M=2560, K=6912, ~6.0 MB

Then once:
  - output_norm
  - gemv (LM head, tied = 2560 × 151936 Q6_K) ~310 MB
  - softmax
  - sample
```

每层 kernel 数：**13**。乘以 36 层 + 末尾约 3 个 = **每 token 471 次 kernel 启动**。在 Orin 上每次启动约 10 µs，即每 token 约 4.7 ms 的纯启动开销 —— 即便 kernel 本身的计算不要钱，上限也只有 ~210 tok/s。实际上在 Orin Nano 上上限会低得多，因为其中有些 kernel 相对启动开销来说太小了。

每层从 DRAM 读取的字节数：

```
weights:   2.6 + 0.7 + 0.9 + 2.6 + 4.4 + 4.4 + 6.0 = ~21.6 MB/layer
× 36 layers + LM head 310 MB                       = ~1.09 GB/token

Plus KV cache reads — at 4 k context filled:
   2 × 36 layers × 8 KV heads × 128 dim × 4096 ctx × 2 B = 576 MB
   read once per token through flash-attention      = ~576 MB
```

合计：**每 token 约 1.66 GB 的 DRAM 流量**。按 Orin Nano 实测约 50 GB/s 的有效带宽：**短上下文上限 ~30 tok/s，完整 4 k 上下文 ~16 tok/s**。这就是你要逼近的数字。

---


<details>
<summary>English original</summary>

**Lecture 3: Decode Optimization for Qwen3-4B on Jetson Orin Nano**

**Overview**

You now have a Qwen3-4B-Q4_K_M GGUF and a runtime that loads it. The previous JLLM log showed it running at **0.2 tok/s**. The Orin Nano roofline says you should be able to hit **~14–20 tok/s** for this model. This lecture closes that 70–100× gap by walking the optimizations in order of impact:

1. Fix platform configuration (`nvpmodel`, `jetson_clocks`).
2. Verify CUDA path is actually in use.
3. Fuse QKV and gate/up to halve kernel launches.
4. Apply CUDA Graphs to collapse per-token launch overhead.
5. Quantize the KV cache to INT8 once context grows.
6. Add speculative decoding for the last 1.5×.

Every step is concrete: shape arithmetic, kernel-shape changes, and what to measure.

By the end you should be able to:

* Take a JLLM-style trace and identify the single highest-impact fix.
* Estimate per-token bandwidth and predict tok/s before running.
* Configure a fused QKV path and verify the kernel count drops in `nsys`.
* Apply CUDA Graphs to the decode hot path and quantify the win.

---

**1. Platform Configuration — The First 50× Win**

Before anything else, **lock down the platform**. The Orin Nano 8 GB has three power modes; the difference between mode 0 (15 W) and mode 1 (7 W) is roughly **3× in tok/s** for memory-bandwidth-bound workloads.

```bash
# Step 1: maximum performance
sudo nvpmodel -m 0
sudo jetson_clocks

# Step 2: confirm
sudo jetson_clocks --show
# Expect:
#   GPU MinFreq=624000000 MaxFreq=624000000   ← pinned (Nano max) or 1020 MHz on AGX
#   EMC MinFreq=2133000000 MaxFreq=2133000000 ← max memory clock
#   CPU Cluster 0: MinFreq=N MaxFreq=N         ← pinned

# Step 3: watch during inference
tegrastats --interval 500
# Expect during decode:
#   GR3D_FREQ 80–100% (GPU 3D engine busy)
#   EMC_FREQ  near 100% (you're bandwidth-bound)
```

If `GR3D_FREQ` stays at 0% during inference, the GPU isn't being used. That points to a CUDA build issue, not a configuration issue — go back to step 0.

If `EMC_FREQ` stays well below 100% while `GR3D_FREQ` is high, your kernels are compute-bound on something other than weight reads — unusual for a 4B Q4 model and usually indicates large activation working sets (long context, big intermediate buffers).

**Result you should see immediately after this step alone:** 0.2 tok/s → ~10 tok/s on Qwen3-4B-Q4_K_M.

---

**2. The Decode Hot Path**

A single decoded token on Qwen3-4B with naive (unfused) GEMV dispatch:

```
Per layer (× 36):
  1.  fused_rmsnorm_residual   (attn input norm)
  2.  gemv_q4k  Q              M=4096, K=2560, ~2.6 MB weight read
  3.  gemv_q4k  K              M=1024, K=2560, ~0.7 MB
  4.  gemv_q6k  V              M=1024, K=2560, ~0.9 MB
  5.  rope_kernel              ~0
  6.  kv append                ~0
  7.  flash_attention_decode   reads KV cache slice
  8.  gemv_q4k  O              M=2560, K=4096, ~2.6 MB
  9.  fused_rmsnorm_residual   (ffn norm)
  10. gemv_q4k  gate           M=6912, K=2560, ~4.4 MB
  11. gemv_q4k  up             M=6912, K=2560, ~4.4 MB
  12. swiglu_kernel            ~0
  13. gemv_q6k  down           M=2560, K=6912, ~6.0 MB

Then once:
  - output_norm
  - gemv (LM head, tied = 2560 × 151936 Q6_K) ~310 MB
  - softmax
  - sample
```

Per-layer kernel count: **13**. Times 36 layers + ~3 final = **471 kernel launches per token**. At ~10 µs per launch on Orin, that's ~4.7 ms of pure launch overhead per token — even if kernel work itself were free, you'd cap at ~210 tok/s. In practice on Orin Nano you'd cap much lower because some of those kernels are tiny relative to launch cost.

Per-layer bytes read from DRAM:

```
weights:   2.6 + 0.7 + 0.9 + 2.6 + 4.4 + 4.4 + 6.0 = ~21.6 MB/layer
× 36 layers + LM head 310 MB                       = ~1.09 GB/token

Plus KV cache reads — at 4 k context filled:
   2 × 36 layers × 8 KV heads × 128 dim × 4096 ctx × 2 B = 576 MB
   read once per token through flash-attention      = ~576 MB
```

Total: **~1.66 GB/token of DRAM traffic**. At Orin Nano's measured ~50 GB/s effective bandwidth: **~30 tok/s ceiling for short context, ~16 tok/s for full 4 k context**. That's the number you're racing toward.

---

</details>

## 3. 融合 #1 — QKV 拼接

三个 GEMV（矩阵-向量乘）从 DRAM 读取 `x` 三次。拼接矩阵：

```
W_QKV = [W_Q | W_K | W_V]    # shape: K × (M_Q + M_K + M_V) = 2560 × (4096+1024+1024)
                              #      = 2560 × 6144
```

一个 GEMV：`M=6144, K=2560`。输出切片到 Q、K、V。

```cuda
// Before: three kernels, three reads of x
gemv_q4k_kernel<<<...>>>(q_out, w_q, x, 4096, 2560);
gemv_q4k_kernel<<<...>>>(k_out, w_k, x, 1024, 2560);
gemv_q6k_kernel<<<...>>>(v_out, w_v, x, 1024, 2560);

// After: one kernel, one read of x
gemv_q4k_q6k_mixed_kernel<<<...>>>(qkv_out, w_qkv, x, 6144, 2560);
// then in subsequent kernels treat qkv_out[0:4096], [4096:5120], [5120:6144]
```

等一下 — Qwen 对 Q (Q4_K) 和 V (Q6_K) 使用不同的量化类型。直接融合将它们全部存储为同一类型。选项：

1. **将所有内容量化为 Q4_K 并接受质量下降。** ~0.1 困惑度损失。
2. **保留单独的矩阵，但在同一流上启动并重叠执行。** 节省启动开销，不节省 input-x 的重复读取。
3. **编写一个在一次启动内处理混合量化的 kernel。** 大多数生产 runtime 都这样做。

对于 Qwen3-4B-Q4_K_M，JLLM/llama.cpp 默认采用选项 2；vLLM/TRT-LLM（正在开发 AWQ-int4）免费获得选项 3，因为 AWQ 对整个矩阵使用均匀量化。

**预期收益：** 在 Orin Nano 上 tok/s 提升约 15–25%。

---

## 4. 融合 #2 — Gate 和 Up

对 FFN 使用同样的技巧：

```
W_gu = [W_gate | W_up]    # shape: 2560 × (6912 + 6912) = 2560 × 13824
```

一个 GEMV 产生两者，然后按如下方式应用 SwiGLU：

```
out = silu(gu_out[0:6912]) * gu_out[6912:13824]
```

这是一个干净的收益，因为两个矩阵具有相同的量化类型（在标准 Q4_K_M recipe 中都是 Q4_K）。没有混合类型的问题。

**预期收益：** 额外提升约 10–15% tok/s。

---

## 5. CUDA Graphs — 合并启动

融合后，每个 token 的 kernel 数量从约 470 降至约 250。仍然很多。在 Orin 上每次启动约 5–10 µs。

CUDA Graphs 让你**一次性捕获整个 decode（逐 token 生成阶段）单 token 计算**，并作为单个 graph 节点重新启动：

```c++
cudaGraph_t graph;
cudaGraphExec_t graph_exec;

// Capture mode: first iteration
cudaStreamBeginCapture(stream, cudaStreamCaptureModeGlobal);
decode_one_token(stream, /* ... */);   // launches all 250 kernels
cudaStreamEndCapture(stream, &graph);

cudaGraphInstantiate(&graph_exec, graph, nullptr, nullptr, 0);

// Steady state: launch the whole graph as one operation
for (int t = 0; t < n_tokens; t++) {
    update_kv_indices_and_token_id(stream);   // tiny CPU-side prep
    cudaGraphLaunch(graph_exec, stream);
    sample_token(stream);
}
```

需要了解的约束：
- **形状必须在捕获时固定。** 这对 decode 来说没问题（batch=1，seq_len=1）。
- **指针必须稳定。** 使用持久化的 scratch arena，不要在捕获区域内 malloc。
- **KV cache 索引每个 token 都会变化。** 要么使用带有 `cudaGraphExecKernelNodeSetParams` 的 graph 来更新索引，要么为整个生成过程预计算索引数组。

**预期收益：** 在 Orin Nano 上特别明显，约 30–50% — iGPU 的启动开销占总 kernel 时间的比例比独立 GPU 更大。

---

## 6. 用于 Attention 块的 FlashAttention-Decode

你在构建日志中看到的 `flash_attention_decode_kernel` 是 attention 步骤的**标准优化**。思路：不物化 `[seq_len × seq_len]` attention 分数矩阵，而是**跨 KV cache 分块**并在片上累加 `softmax · V`。

具体到 decode，操作是：

```
q:    [n_heads, head_dim]                = [32, 128]
K:    [seq_len, n_kv_heads, head_dim]    = [ctx, 8, 128]
V:    [seq_len, n_kv_heads, head_dim]    = [ctx, 8, 128]

output[h] = softmax(q[h] · K[:, h//4, :]ᵀ / √128) · V[:, h//4, :]
```

优化后的 kernel：
1. 每个线程块处理一个 Q head。
2. 将 K cache 分块到共享内存，每块约 64 个 token。
3. 流式处理 Q · Kᵀ，增量应用 softmax（来自 FlashAttention-2 的 online softmax 技巧）。
4. 针对 V 分块进行累加。
5. 写入一个 `head_dim` 大小的输出向量。

相对于朴素实现的收益是巨大的 — 由于重复的 KV cache 传递，朴素实现在 Orin 上处理长上下文时很容易慢 5 倍。

如果你的 runtime 没有合适的 FlashAttention-decode kernel，那么在其他任何东西之前，你应该聚焦于此。MLC-LLM 和 llama.cpp 的实现是可用的参考。

---


<details>
<summary>English original</summary>

**3. Fusion #1 — QKV Concatenation**

Three GEMVs read `x` from DRAM three times. Concatenate the matrices:

```
W_QKV = [W_Q | W_K | W_V]    # shape: K × (M_Q + M_K + M_V) = 2560 × (4096+1024+1024)
                              #      = 2560 × 6144
```

One GEMV: `M=6144, K=2560`. Output slices to Q, K, V.

```cuda
// Before: three kernels, three reads of x
gemv_q4k_kernel<<<...>>>(q_out, w_q, x, 4096, 2560);
gemv_q4k_kernel<<<...>>>(k_out, w_k, x, 1024, 2560);
gemv_q6k_kernel<<<...>>>(v_out, w_v, x, 1024, 2560);

// After: one kernel, one read of x
gemv_q4k_q6k_mixed_kernel<<<...>>>(qkv_out, w_qkv, x, 6144, 2560);
// then in subsequent kernels treat qkv_out[0:4096], [4096:5120], [5120:6144]
```

Wait — Qwen has different quant types for Q (Q4_K) and V (Q6_K). The straightforward fusion stores them all as the same type. Options:

1. **Quantize everything to Q4_K and live with the quality drop.** ~0.1 perplexity penalty.
2. **Keep separate matrices but launch them on the same stream and overlap.** Saves launch overhead, doesn't save the input-x re-read.
3. **Write a kernel that handles mixed quant inside one launch.** Most production runtimes do this.

For Qwen3-4B-Q4_K_M the JLLM/llama.cpp default is option 2; vLLM/TRT-LLM (working on AWQ-int4) get option 3 for free because AWQ uses uniform quantization across the whole matrix.

**Expected win:** ~15–25% tok/s improvement on Orin Nano.

---

**4. Fusion #2 — Gate and Up**

Same trick on the FFN:

```
W_gu = [W_gate | W_up]    # shape: 2560 × (6912 + 6912) = 2560 × 13824
```

One GEMV produces both, then SwiGLU is applied as:

```
out = silu(gu_out[0:6912]) * gu_out[6912:13824]
```

This is a clean win because both matrices have the same quant type (both Q4_K in the standard Q4_K_M recipe). No mixed-type concerns.

**Expected win:** ~10–15% additional tok/s.

---

**5. CUDA Graphs — Collapse the Launches**

After fusion the per-token kernel count drops from ~470 to ~250. Still a lot. Each launch is ~5–10 µs on Orin.

CUDA Graphs let you **capture the entire decode-one-token computation once** and re-launch as a single graph node:

```c++
cudaGraph_t graph;
cudaGraphExec_t graph_exec;

// Capture mode: first iteration
cudaStreamBeginCapture(stream, cudaStreamCaptureModeGlobal);
decode_one_token(stream, /* ... */);   // launches all 250 kernels
cudaStreamEndCapture(stream, &graph);

cudaGraphInstantiate(&graph_exec, graph, nullptr, nullptr, 0);

// Steady state: launch the whole graph as one operation
for (int t = 0; t < n_tokens; t++) {
    update_kv_indices_and_token_id(stream);   // tiny CPU-side prep
    cudaGraphLaunch(graph_exec, stream);
    sample_token(stream);
}
```

Constraints to know:
- **Shapes must be fixed at capture time.** This is fine for decode (batch=1, seq_len=1).
- **Pointers must be stable.** Use a persistent scratch arena, don't malloc inside the captured region.
- **KV-cache indices change per token.** Either use a graph with `cudaGraphExecKernelNodeSetParams` to update indices, or precompute index arrays for the whole generation.

**Expected win:** ~30–50% on Orin Nano specifically — the iGPU's launch overhead is a larger fraction of total kernel time than on discrete GPUs.

---

**6. FlashAttention-Decode for the Attention Block**

The `flash_attention_decode_kernel` you saw in the build log is the **canonical optimization** for the attention step. The idea: instead of materializing the `[seq_len × seq_len]` attention-score matrix, **tile across the KV cache** and accumulate `softmax · V` on-chip.

For decode specifically the operation is:

```
q:    [n_heads, head_dim]                = [32, 128]
K:    [seq_len, n_kv_heads, head_dim]    = [ctx, 8, 128]
V:    [seq_len, n_kv_heads, head_dim]    = [ctx, 8, 128]

output[h] = softmax(q[h] · K[:, h//4, :]ᵀ / √128) · V[:, h//4, :]
```

The optimized kernel:
1. Each thread block handles one Q head.
2. Tiles the K cache in shared memory in chunks of ~64 tokens.
3. Streams through Q · Kᵀ, applies softmax incrementally (online softmax trick from FlashAttention-2).
4. Accumulates against V tiles.
5. Writes one `head_dim`-sized output vector.

The win over a naive implementation is huge — naive can easily be 5× slower for long contexts on Orin because of repeated KV-cache passes.

If your runtime doesn't have a proper FlashAttention-decode kernel, that's where you should focus before touching anything else. The MLC-LLM and llama.cpp implementations are usable references.

---

</details>

## 7. INT8 KV cache——上下文何时变得重要

一旦上下文超过约 4 k，KV cache 就成为 decode（逐 token 生成阶段）带宽中不可忽视的一部分。把它量化：

```
FP16 KV: 4096 bytes/token/layer  → 576 MB at 4k ctx
INT8 KV: 2048 bytes/token/layer  → 288 MB at 4k ctx (-50%)
INT4 KV: 1024 bytes/token/layer  → 144 MB at 4k ctx (-75%)
```

实证有效的量化布局：

* 每 head 的 scale（FP16），在 prefill（首字前的整段计算）期间每 N 个 token 计算一次（例如每 64-token 块一次）。
* INT8 的 K 与 V 分开存储（两者统计特性不同——V 的离群点更多）。
* 在 flash-attention-decode 内部于 shared memory 中即时反量化。

对 Qwen3-4B 的质量影响：INT8 KV 基本是白送的（困惑度下降 < 0.1）。INT4 KV 在超过约 16 k 上下文后开始出现退化。在 prefill 期间配合 per-channel 校准的 INT4 KV，在约 64 k 上下文以内可与 INT8 KV 相媲美。

大多数生产级 runtime（vLLM、SGLang、TRT-LLM）都把 INT8 KV 作为单 flag 选项提供。llama.cpp 中它对应 `--cache-type-k q8_0 --cache-type-v q8_0`。

---

## 8. 投机解码——最后的 1.5×

解码出的序列是**自回归的**：每个 token 都依赖前一个。投机解码通过以下方式打破这一点：

1. 用一个**小的 draft 模型**（比如 Q4 的 Qwen3-0.5B）生成接下来 K 个 token。
2. 用**目标模型**（Qwen3-4B-Q4_K_M）对这 K 个候选**并行**计算（一次 seq_len = K 的前向传播）。
3. 接受目标模型本来也会产生的那段候选前缀，外加一个白送的 token。

具体到 Orin Nano，账是这样算的：

```
Naive: 36-layer Qwen3-4B forward = 1.66 GB DRAM / token → ~12 tok/s

Spec dec (K=4):
  - Draft model 0.5B at Q4 → ~400 MB DRAM / token, fast
  - Target model forward pass with seq_len=4
    → still ~1.66 GB DRAM (weights dominate, batch dimension is free for bandwidth)
    → so the target step costs ~83 ms wall
  - With 60% acceptance rate, you get 2.4 tokens out per target step
  - Effective rate: 2.4 / 83ms = 29 tok/s
```

需要注意的坑：
- draft 模型必须在**大多数时候与目标模型一致**，投机解码才能赢。用 Qwen3-0.5B 作为 Qwen3-4B 的 draft 效果尚可（约 50-65% 接受率）。随便找个小模型不行。
- 两个模型要多占 DRAM。在 Orin Nano 8 GB 上这很紧张——通常两个模型都跑 Q4，并接受更小的 cache 预算。

大多数边缘 runtime 尚未提供投机解码。截至 2026 年，它在 vLLM/SGLang 中是标配，在 MLC-LLM 中可选，在 llama.cpp 和大多数嵌入式路径中尚缺。

---

## 9. 汇总——Orin Nano 上 Qwen3-4B 的一份预算

| 步骤 | 操作 | 累计 tok/s |
|---|---|---|
| 基线（来自 JLLM log） | 未改动，默认 DVFS | 0.2 |
| + `nvpmodel -m 0 && jetson_clocks` | 锁定最大功耗与频率 | 8–10 |
| + 确认 CUDA 路径已启用 | 未落到 CPU 回退 | 10–12 |
| + QKV 融合 | 一次 GEMV（矩阵-向量乘）代替三次 | 12–14 |
| + gate+up 融合 | 一次 GEMV 代替两次 | 14–16 |
| + 对逐 token decode 套 CUDA Graphs | 把 250 次 launch 收敛成一次 | 18–22 |
| + FlashAttention-decode（正确实现） | 若尚未用上 | 20–24 |
| + INT8 KV（仅 >4k 上下文） | 随 ctx 增长保住性能 | 20–24（更长 ctx） |
| + 投机解码（Qwen3-0.5B draft） | 用额外 DRAM 换接受率 | 28–35 |

一个工程做得扎实的 runtime，在 Orin Nano 上短上下文跑 Qwen3-4B-Q4_K_M 应落在 25–35 tok/s 区间。这就是目标。如果只能跑到 8–12，说明缺了融合或 CUDA Graphs。如果只有 1–3，说明还卡在配置问题或 CPU 回退上。

---

## 10. 诊断你自己的 trace

从 JLLM log 看：

```
Power: 0W mode, GPU @ 0 MHz
[engine] Prefill: 18 tokens in 110400 ms (0.2 tok/s)
[engine] Decode:  16 tokens in 100064 ms (0.2 tok/s)
```

逐步解读：
1. **0.2 tok/s，GPU @ 0 MHz** → DVFS 停摆。本讲第 1 步可解决。
2. 执行 `jetson_clocks` 后，预期约 10 tok/s。若能达到，说明 runtime 本身可用，继续沿优化清单往下走。
3. **可见三次 GEMV**（Q、K、V 的 `#0 #1 #2`）→ 没有 QKV 融合。见第 3 步。
4. **prefill 与 decode 速度相同** → JLLM 在 prefill 期间跑的是逐 token GEMV，而不是 batched GEMM（矩阵-矩阵乘）。改用 GEMM 可换来 prefill 的大幅收益（TTFT 提升一个数量级，不影响 decode 速率）。

JLLM 的 runtime 在结构上没问题——只是缺了标准优化。按 §9 的顺序逐项施加即可。

---


<details>
<summary>English original</summary>

**7. INT8 KV Cache — When Context Matters**

Once you push past ~4 k context, the KV cache becomes a meaningful fraction of decode bandwidth. Quantize it:

```
FP16 KV: 4096 bytes/token/layer  → 576 MB at 4k ctx
INT8 KV: 2048 bytes/token/layer  → 288 MB at 4k ctx (-50%)
INT4 KV: 1024 bytes/token/layer  → 144 MB at 4k ctx (-75%)
```

The quantization layout that works empirically:

* Per-head scale (FP16) computed every N tokens during prefill (e.g., per 64-token chunk).
* INT8 K and V stored separately (different statistical properties — V has more outliers).
* On-the-fly dequant in shared memory inside flash-attention-decode.

Quality impact for Qwen3-4B: INT8 KV is essentially free (< 0.1 perplexity drop). INT4 KV starts showing degradation past ~16 k context. INT4 KV with per-channel calibration during prefill is competitive with INT8 KV up to ~64 k context.

Most production runtimes (vLLM, SGLang, TRT-LLM) ship INT8 KV as a one-flag option. llama.cpp has it as `--cache-type-k q8_0 --cache-type-v q8_0`.

---

**8. Speculative Decoding — The Last 1.5×**

The decoded sequence is **autoregressive**: every token depends on the previous. Speculative decoding breaks this by:

1. Running a **small draft model** (say, Qwen3-0.5B at Q4) for the next K tokens.
2. Running the **target model** (Qwen3-4B-Q4_K_M) on those K candidates **in parallel** (one forward pass with seq_len = K).
3. Accepting the prefix of candidates that the target would have produced, plus one extra free token.

On Orin Nano specifically, the math:

```
Naive: 36-layer Qwen3-4B forward = 1.66 GB DRAM / token → ~12 tok/s

Spec dec (K=4):
  - Draft model 0.5B at Q4 → ~400 MB DRAM / token, fast
  - Target model forward pass with seq_len=4
    → still ~1.66 GB DRAM (weights dominate, batch dimension is free for bandwidth)
    → so the target step costs ~83 ms wall
  - With 60% acceptance rate, you get 2.4 tokens out per target step
  - Effective rate: 2.4 / 83ms = 29 tok/s
```

Catches:
- The draft model needs to **agree with the target most of the time** for spec dec to win. Qwen3-0.5B as a draft for Qwen3-4B works reasonably (~50-65% acceptance). Random tiny models do not work.
- You pay extra DRAM for two models. On Orin Nano 8 GB this is tight — typically you'd run both models at Q4 and accept the smaller cache budget.

Most edge runtimes don't ship speculative decoding yet. As of 2026 it's standard in vLLM/SGLang, optional in MLC-LLM, missing in llama.cpp and most embedded paths.

---

**9. Putting It Together — A Budget for Qwen3-4B on Orin Nano**

| Step | Action | Cumulative tok/s |
|---|---|---|
| Baseline (from JLLM log) | unmodified, default DVFS | 0.2 |
| + `nvpmodel -m 0 && jetson_clocks` | lock max power and clocks | 8–10 |
| + Confirm CUDA path active | not on CPU fallback | 10–12 |
| + Fused QKV | one GEMV instead of three | 12–14 |
| + Fused gate+up | one GEMV instead of two | 14–16 |
| + CUDA Graphs over the per-token decode | collapse 250 launches into one | 18–22 |
| + FlashAttention-decode (proper impl) | if not already there | 20–24 |
| + INT8 KV (>4k context only) | preserves perf as ctx grows | 20–24 (longer ctx) |
| + Speculative decoding (Qwen3-0.5B draft) | trade extra DRAM for accept rate | 28–35 |

A well-engineered runtime should be in the 25–35 tok/s band on Orin Nano for Qwen3-4B-Q4_K_M at short context. That's the target. If you're shipping 8–12, you're missing fusion or graphs. If you're shipping 1–3, you're still on a config issue or CPU fallback.

---

**10. Diagnosing Your Specific Trace**

From the JLLM log:

```
Power: 0W mode, GPU @ 0 MHz
[engine] Prefill: 18 tokens in 110400 ms (0.2 tok/s)
[engine] Decode:  16 tokens in 100064 ms (0.2 tok/s)
```

Walkthrough:
1. **0.2 tok/s, GPU @ 0 MHz** → DVFS-parked. Step 1 of this lecture fixes that.
2. After `jetson_clocks`, expect ~10 tok/s. If you get that, the runtime itself is functional and you continue down the optimization list.
3. **Three GEMVs visible** (`#0 #1 #2` for Q, K, V) → no QKV fusion. Step 3.
4. **Prefill same speed as decode** → JLLM is running per-token GEMV during prefill instead of batched GEMM. Big prefill win available by switching to GEMM (TTFT improves an order of magnitude, doesn't affect decode rate).

The JLLM runtime is structurally fine — it's missing the standard optimizations. Apply them in order from §9.

---

</details>

## 动手练习

1. **优化前后对照表。** 在同一台 Orin Nano 上，用 llama.cpp 运行 Qwen3-4B-Q4_K_M，分别采用：
   （a）默认配置，
   （b）应用 `jetson_clocks` 之后，
   （c）启用 `--mlock`（pin weights），
   （d）启用 CUDA Graphs（较新的 llama.cpp 构建已开放该选项）。
   记录每种配置的 tok/s 和 `tegrastats` 快照。产出四行表格。

2. **roofline（性能上界模型）图。** 针对 Qwen3-4B-Q4_K_M，绘制 tok/s 随上下文长度从 256 到 4096 变化的曲线。叠加带宽受限的理论曲线。找出偏离点及其原因（KV 开销上升、attention 计算开始主导等）。

3. **FlashAttention 检查。** 通过检查 `nsys` trace 中的 kernel 名称，判断你的 runtime 是否在使用融合 attention kernel。如果没有，切换到带该 kernel 的构建/分支，并重新测量 §1 的 roofline 图。

4. **长上下文下的 INT8 KV。** 生成一个 16 k token 的 prompt（分块的代码补全是不错的来源）。先用 FP16 KV decode（逐 token 生成阶段）256 个新 token，再用 INT8 KV 做一次。比较 tok/s 与实际的生成文本。量化带宽节省与可感知的质量差异。

5. **用 Qwen3-0.5B 做投机解码。** 下载 Qwen3-0.5B（或 1.7B），量化到 Q4_K_M，并在 vLLM（或支持该功能的 MLC-LLM）中配置投机解码。测量在 chat prompt 和代码 prompt 上的接受率。报告 Orin Nano 上实际的端到端 tok/s 收益。

6. **「到底有没有用上 CUDA？」的健全性检查。** 找一个你怀疑发生了 CPU 回退的 runtime。跑一次 32 token 的 decode。查看 `tegrastats`。如果 `GR3D_FREQ` 为 0% 且 CPU 占用为 100%，说明你的 runtime 跑在 CPU 上。修复构建（用 `LLAMA_CUDA=1` 或等效选项重新编译）后重测。

---

## 关键要点

| 要点 | 为什么重要 |
|---|---|
| `nvpmodel` + `jetson_clocks` 是最大的单一调节项 | 最差与最佳配置之间相差 50× —— 而且这些都是免费的 |
| 每 token 471 次 kernel 启动是未融合的基线 | 仅 kernel 启动开销就可能主导 decode |
| 融合 QKV 与融合 gate+up 可将启动次数减半 | 配置调优之后应首先落地的两项优化 |
| CUDA Graphs 在 Orin 上价值尤为突出 | 启动开销占比高于独立 GPU |
| 上下文超过约 1k 后，FlashAttention-decode 必不可少 | 朴素 attention 可能主导 KV 缓存带宽 |
| 在 Qwen3-4B 上，INT8 KV 基本不损失质量 | 只要上下文 > 4 k 就应使用 |
| 投机解码需要与目标模型一致的 draft | Qwen3-0.5B 搭配 Qwen3-4B 是合理的组合 |

---

## 资源

* **[NVIDIA Jetson Linux Developer Guide — Power Modes](https://docs.nvidia.com/jetson/archives/r36.2/DeveloperGuide/)：** `nvpmodel` 与 `jetson_clocks` 的权威文档。
* **[CUDA Graphs — Programming Guide](https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#cuda-graphs)：** 捕获机制、参数更新。
* **[FlashAttention-2 paper](https://arxiv.org/abs/2307.08691)：** decode kernel 所用的 online-softmax + 分块原语。
* **[FlashDecoding](https://crfm.stanford.edu/2023/10/12/flashdecoding.html)：** 面向 decode 的变体，跨 KV 块并行。
* **[Speculative Decoding paper (Leviathan et al.)](https://arxiv.org/abs/2211.17192)：** 投机解码的原始分析。
* **[Medusa: Multiple decoding heads](https://arxiv.org/abs/2401.10774)：** 内联投机解码变体；若想避免双模型开销则值得关注。
* **[MLC-LLM Qwen example](https://llm.mlc.ai/docs/)：** Jetson 上融合 kernel 的参考实现。
* **[llama.cpp Qwen3 support](https://github.com/ggerganov/llama.cpp)：** 默认参考 runtime。
* **[阶段 5 — Edge LLM Inference Internals](../03-边缘LLM推理内部机制/Lecture-01.md)：** 前置内容，含 roofline 计算。


<details>
<summary>English original</summary>

**Hands-On Exercises**

1. **The before-and-after table.** On the same Orin Nano, run Qwen3-4B-Q4_K_M through llama.cpp with:
   (a) default config,
   (b) after `jetson_clocks`,
   (c) with `--mlock` (pin weights),
   (d) with CUDA Graphs enabled (recent llama.cpp builds expose this).
   Record tok/s and `tegrastats` snapshots for each. Produce the four-row table.

2. **Roofline plot.** For Qwen3-4B-Q4_K_M, plot tok/s vs context length from 256 to 4096. Overlay the bandwidth-bound theoretical curve. Identify where you deviate and why (KV cost rising, attention compute taking over, etc.).

3. **FlashAttention check.** Determine whether your runtime is using a fused attention kernel by inspecting the kernel names in `nsys` traces. If it isn't, switch to a build/branch that has it and re-measure §1's roofline plot.

4. **INT8 KV at long context.** Generate a 16 k-token prompt (chunked code completion is a good source). Decode 256 new tokens with FP16 KV, then with INT8 KV. Compare tok/s and the actual generated text. Quantify the bandwidth saving and the perceived quality difference.

5. **Speculative decoding with Qwen3-0.5B.** Download Qwen3-0.5B (or 1.7B), quantize to Q4_K_M, and set up speculative decoding in vLLM (or MLC-LLM, where supported). Measure acceptance rate on chat prompts and on code prompts. Report the actual end-to-end tok/s win on Orin Nano.

6. **The "is it even using CUDA?" sanity check.** Take a runtime where you suspect CPU fallback. Run a 32-token decode. Read `tegrastats`. If `GR3D_FREQ` is 0% and CPU load is 100%, your runtime is on CPU. Fix the build (rebuild with `LLAMA_CUDA=1` or equivalent) and re-test.

---

**Key Takeaways**

| Takeaway | Why it matters |
|---|---|
| `nvpmodel` + `jetson_clocks` is the single biggest knob | 50× delta between worst and best config — and they're free |
| 471 kernel launches per token is the unfused baseline | Launch overhead alone can dominate decode |
| Fused QKV and fused gate+up halve the launch count | First two optimizations to ship after configuration |
| CUDA Graphs are uniquely valuable on Orin | Higher launch-overhead fraction than discrete GPUs |
| FlashAttention-decode is non-negotiable past ~1k context | Naive attention can dominate KV-cache bandwidth |
| INT8 KV is essentially free quality-wise on Qwen3-4B | Use it whenever context > 4 k |
| Speculative decoding needs a draft that agrees with the target | Qwen3-0.5B for Qwen3-4B is a reasonable pairing |

---

**Resources**

* **[NVIDIA Jetson Linux Developer Guide — Power Modes](https://docs.nvidia.com/jetson/archives/r36.2/DeveloperGuide/):** Canonical `nvpmodel` and `jetson_clocks` documentation.
* **[CUDA Graphs — Programming Guide](https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#cuda-graphs):** Capture mechanics, parameter updates.
* **[FlashAttention-2 paper](https://arxiv.org/abs/2307.08691):** The online-softmax + tiling primitive used in the decode kernel.
* **[FlashDecoding](https://crfm.stanford.edu/2023/10/12/flashdecoding.html):** Decode-specific variant that parallelizes across KV blocks.
* **[Speculative Decoding paper (Leviathan et al.)](https://arxiv.org/abs/2211.17192):** The original spec-dec analysis.
* **[Medusa: Multiple decoding heads](https://arxiv.org/abs/2401.10774):** Inline-spec-dec variant; relevant if you want to avoid two-model overhead.
* **[MLC-LLM Qwen example](https://llm.mlc.ai/docs/):** Reference for fused kernels on Jetson.
* **[llama.cpp Qwen3 support](https://github.com/ggerganov/llama.cpp):** Default reference runtime.
* **[Phase 5 — Edge LLM Inference Internals](../03-边缘LLM推理内部机制/Lecture-01.md):** Prereq with the roofline math.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track C - Edge AI/Qwen Inference Optimization/Lecture-03.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20C%20-%20Edge%20AI/Qwen%20Inference%20Optimization/Lecture-03.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
