---
title: CUDA kernel
description: CUDA kernel
---

# CUDA kernel

所有 kernel 均针对 Orin SM 8.7 调优：48 KB 共享内存、128 线程 block、16 个 SM。

## gemv_q4 — INT4 反量化融合 GEMV（矩阵-向量乘）

**文件：** `src/kernels/gemv_q4.cu`
**时间占比：** 约占 decode（逐 token 生成阶段）时间的 38% — 头号优化目标。

### 功能

计算 `y[M] = W[M×K] × x[K]`，其中 W 为 4-bit 量化（每字节 2 个权重），并具有 FP16 逐组 scale。

### 为何融合反量化很重要

不融合：读 INT4 权重 → 将 FP16 权重写入 DRAM → 读 FP16 权重 → 计算。
融合：读 INT4 权重 → 在寄存器中反量化 → 计算。绝不将 FP16 权重写入 DRAM。

带宽：K/2 字节（INT4） vs K×2 字节（FP16）= **3.5× 减少**。

### Orin 调优

```
Grid:   (ceil(M / 4), 1)
Block:  128 threads = 4 warps
        Each warp handles one output row (M dimension)
        32 lanes stride across K dimension (coalesced uint32 loads)

Reduction: warp shuffle (__shfl_xor_sync) — no shared memory needed
Dequant:   8 INT4 values from one uint32, multiply by group scale
```

### 关键代码路径

```
1. Each lane loads W_packed[lane], W_packed[lane+32], ... (coalesced)
2. Dequantize 8 values per uint32 (shift + mask + scale)
3. Dot product with x[k0..k0+7] (x stays in L1/L2 cache)
4. Warp shuffle reduce (5 rounds: offset 16,8,4,2,1)
5. Lane 0 writes y[row]
```

## fused_norm — RMSNorm + 残差相加

**文件：** `src/kernels/fused_norm.cu`
**时间占比：** 约占 decode 时间的 11%。

### 功能

在单个 kernel 中计算 `output = RMSNorm(x) × weight`。

不融合：3 个 kernel，6 次 DRAM 访问。
融合：1 个 kernel，3 次 DRAM 访问（读 x、读权重、写输出）。

### 算法

```
Pass 1: Load x, compute sum of squares (variance)
  - Each thread handles hidden_dim/blockDim elements
  - Warp shuffle reduce for partial sums
  - Cross-warp reduce via shared memory (4 floats for 4 warps)
  - Compute rrms = rsqrt(variance/dim + eps)

Pass 2: Normalize and scale
  - normed = x * rrms * weight
  - Write output
```

### 共享内存用量

`hidden_dim × sizeof(float)` 用于中间值。对于 hidden_dim=2048：8 KB。对于 hidden_dim=3072：12 KB。两者均可容纳于 48 KB。

## attention — Flash Attention Decode

**文件：** `src/kernels/attention.cu`
**时间占比：** 约占 decode 时间的 28%。

### 功能

用于 decode 的单 query attention（一个新 token）。计算：
```
output = softmax(Q × K^T / sqrt(d)) × V
```

而不物化完整的 seq×seq attention 矩阵。

### 算法（online softmax）

```
For each KV tile (64 tokens):
  1. Compute Q×K^T for tile (each thread handles some time steps)
  2. Find tile max (warp reduce + block reduce via shared memory)
  3. Update running max, correct previous accumulators by exp(old_max - new_max)
  4. Exponentiate scores, accumulate sum
  5. Accumulate P × V into s_out[head_dim] in shared memory
Final: output = s_out / running_sum
```

### Orin 调优

```
Grid:   (n_heads, 1)  — one block per query head
Block:  128 threads
Shared: ATTN_TILE_KV (64) + head_dim floats for scores + output accumulator
Tile:   64 KV tokens per iteration

GQA: kv_head = head / (n_heads / n_kv_heads)
INT8 KV: dequantize on-the-fly in the dot product loop
```

### 内存访问模式

- Q：从全局内存读取一次，留在 L1（很小：128 × 2 = 256 字节）
- K：逐分块读取，每分块 64 × 128 × element_size
- V：逐分块读取，相同模式
- Scores：仅使用共享内存（从不写入 DRAM）
- Output：最后写入一次

## rope — 旋转位置编码

**文件：** `src/kernels/rope.cu`
**时间占比：** 约占 decode 时间的 4%。

### 功能

对 Q 和 K 原地应用旋转位置编码：
```
q'[2i]   = q[2i] × cos(θ) - q[2i+1] × sin(θ)
q'[2i+1] = q[2i] × sin(θ) + q[2i+1] × cos(θ)
where θ = position / (theta_base ^ (2i / head_dim))
```

### Orin 调优

```
One thread per dimension pair (both Q and K in same launch)
Total threads: (n_heads + n_kv_heads) × head_dim/2
cos/sin computed on-the-fly (cheaper than loading from table on bandwidth-limited Orin)
```

## convert — FP16↔INT8 + SwiGLU

**文件：** `src/kernels/convert.cu`

### fp16_to_int8

针对 KV cache 的按行 absmax 量化：
```
scale = max(|row|) / 127
int8_val = round(fp16_val / scale)
```

### fused_swiglu

计算 `output = silu(gate) × up`，其中 `silu(x) = x / (1 + exp(-x))`。
每个元素一个线程。融合可避免将中间 silu 结果写入 DRAM。

## softmax — Logit Softmax

**文件：** `src/kernels/softmax.cu`

仅用于最终的 logit→概率转换（vocab_size 个元素）。三个 pass：
1. 求最大值（数值稳定）
2. 指数化并求和
3. 归一化

单个 block，256 个线程。词表大小最高 128K。

## 工具 kernel（位于 decode.cu）

### vec_add

`out[i] = a[i] + b[i]` — 用于 attention 与 FFN 之间的残差连接。

### fp16_to_fp32

在用于采样的 D2H 拷贝之前，在 GPU 上将 FP16 logits 转换为 FP32。


<details>
<summary>English original</summary>

**CUDA Kernels**

All kernels are tuned for Orin SM 8.7: 48 KB shared memory, 128-thread blocks, 16 SMs.

**gemv_q4 — INT4 Dequant-Fused GEMV**

**File:** `src/kernels/gemv_q4.cu`
**Time share:** ~38% of decode time — the #1 optimization target.

**What it does**

Computes `y[M] = W[M×K] × x[K]` where W is 4-bit quantized (2 weights per byte) with FP16 per-group scales.

**Why fused dequant matters**

Without fusion: read INT4 weights → write FP16 weights to DRAM → read FP16 weights → compute.
With fusion: read INT4 weights → dequantize in registers → compute. Never writes FP16 weights to DRAM.

Bandwidth: K/2 bytes (INT4) vs K×2 bytes (FP16) = **3.5× reduction**.

**Orin tuning**

```
Grid:   (ceil(M / 4), 1)
Block:  128 threads = 4 warps
        Each warp handles one output row (M dimension)
        32 lanes stride across K dimension (coalesced uint32 loads)

Reduction: warp shuffle (__shfl_xor_sync) — no shared memory needed
Dequant:   8 INT4 values from one uint32, multiply by group scale
```

**Key code path**

```
1. Each lane loads W_packed[lane], W_packed[lane+32], ... (coalesced)
2. Dequantize 8 values per uint32 (shift + mask + scale)
3. Dot product with x[k0..k0+7] (x stays in L1/L2 cache)
4. Warp shuffle reduce (5 rounds: offset 16,8,4,2,1)
5. Lane 0 writes y[row]
```

**fused_norm — RMSNorm + Residual Add**

**File:** `src/kernels/fused_norm.cu`
**Time share:** ~11% of decode time.

**What it does**

Computes `output = RMSNorm(x) × weight` in one kernel.

Without fusion: 3 kernels, 6 DRAM accesses.
With fusion: 1 kernel, 3 DRAM accesses (read x, read weight, write output).

**Algorithm**

```
Pass 1: Load x, compute sum of squares (variance)
  - Each thread handles hidden_dim/blockDim elements
  - Warp shuffle reduce for partial sums
  - Cross-warp reduce via shared memory (4 floats for 4 warps)
  - Compute rrms = rsqrt(variance/dim + eps)

Pass 2: Normalize and scale
  - normed = x * rrms * weight
  - Write output
```

**Shared memory usage**

`hidden_dim × sizeof(float)` for intermediate values. For hidden_dim=2048: 8 KB. For hidden_dim=3072: 12 KB. Both fit in 48 KB.

**attention — Flash Attention Decode**

**File:** `src/kernels/attention.cu`
**Time share:** ~28% of decode time.

**What it does**

Single-query attention for decode (one new token). Computes:
```
output = softmax(Q × K^T / sqrt(d)) × V
```

without materializing the full seq×seq attention matrix.

**Algorithm (online softmax)**

```
For each KV tile (64 tokens):
  1. Compute Q×K^T for tile (each thread handles some time steps)
  2. Find tile max (warp reduce + block reduce via shared memory)
  3. Update running max, correct previous accumulators by exp(old_max - new_max)
  4. Exponentiate scores, accumulate sum
  5. Accumulate P × V into s_out[head_dim] in shared memory
Final: output = s_out / running_sum
```

**Orin tuning**

```
Grid:   (n_heads, 1)  — one block per query head
Block:  128 threads
Shared: ATTN_TILE_KV (64) + head_dim floats for scores + output accumulator
Tile:   64 KV tokens per iteration

GQA: kv_head = head / (n_heads / n_kv_heads)
INT8 KV: dequantize on-the-fly in the dot product loop
```

**Memory access pattern**

- Q: read once from global, stays in L1 (small: 128 × 2 = 256 bytes)
- K: read tile by tile, 64 × 128 × element_size per tile
- V: read tile by tile, same pattern
- Scores: shared memory only (never written to DRAM)
- Output: one write at the end

**rope — Rotary Position Embedding**

**File:** `src/kernels/rope.cu`
**Time share:** ~4% of decode time.

**What it does**

Applies rotary position encoding in-place to Q and K:
```
q'[2i]   = q[2i] × cos(θ) - q[2i+1] × sin(θ)
q'[2i+1] = q[2i] × sin(θ) + q[2i+1] × cos(θ)
where θ = position / (theta_base ^ (2i / head_dim))
```

**Orin tuning**

```
One thread per dimension pair (both Q and K in same launch)
Total threads: (n_heads + n_kv_heads) × head_dim/2
cos/sin computed on-the-fly (cheaper than loading from table on bandwidth-limited Orin)
```

**convert — FP16↔INT8 + SwiGLU**

**File:** `src/kernels/convert.cu`

**fp16_to_int8**

Per-row absmax quantization for KV cache:
```
scale = max(|row|) / 127
int8_val = round(fp16_val / scale)
```

**fused_swiglu**

Computes `output = silu(gate) × up` where `silu(x) = x / (1 + exp(-x))`.
One thread per element. Fusing avoids writing intermediate silu result to DRAM.

**softmax — Logit Softmax**

**File:** `src/kernels/softmax.cu`

Used only for final logit→probability conversion (vocab_size elements). Three passes:
1. Find max (numerically stable)
2. Exponentiate and sum
3. Normalize

Single block, 256 threads. Vocab sizes up to 128K.

**Utility Kernels (in decode.cu)**

**vec_add**

`out[i] = a[i] + b[i]` — used for residual connections between attention and FFN.

**fp16_to_fp32**

Converts FP16 logits to FP32 on GPU before D2H copy for sampling.

</details>

## 性能特征（Orin Nano Super）

| Kernel | 瓶颈 | 寄存器 | 共享内存 |
|--------|-----------|-----------|------------|
| gemv_q4 | 内存带宽 | 34 | 0 |
| fused_norm | 内存带宽 | 26 | hidden_dim × 4 |
| attention | 内存带宽 | 40 | (64 + head_dim) × 4 |
| rope | 计算（三角函数） | 13 | 0 |
| softmax | 内存带宽 | 23 | ~36 bytes |
| swiglu | 内存带宽 | 14 | 0 |
| fp16_to_int8 | 内存带宽 | 14 | 4 bytes |


<details>
<summary>English original</summary>

**Performance Characteristics (Orin Nano Super)**

| Kernel | Bottleneck | Registers | Shared mem |
|--------|-----------|-----------|------------|
| gemv_q4 | Memory bandwidth | 34 | 0 |
| fused_norm | Memory bandwidth | 26 | hidden_dim × 4 |
| attention | Memory bandwidth | 40 | (64 + head_dim) × 4 |
| rope | Compute (trig) | 13 | 0 |
| softmax | Memory bandwidth | 23 | ~36 bytes |
| swiglu | Memory bandwidth | 14 | 0 |
| fp16_to_int8 | Memory bandwidth | 14 | 4 bytes |

</details>

---

> 原文：[`Projects/jetson-llm-runtime/docs/kernels.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/docs/kernels.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
