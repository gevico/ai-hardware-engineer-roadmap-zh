---
title: 第 8 讲 —— 推理路径：KV Cache、Decode（逐 token 生成阶段）、RoPE、GQA、分页 KV
description: 第 8 讲 —— 推理路径：KV Cache、Decode（逐 token 生成阶段）、RoPE、GQA、分页 KV
---

# 第 8 讲 —— 推理路径：KV Cache、Decode（逐 token 生成阶段）、RoPE、GQA、分页 KV

**上级：** [FlashAttention Course](Guide.md)

**一句话目的：** 理解 FlashAttention 的 decode 期变体 —— `q_len = 1`、kernel 内 KV cache 更新、分页 KV、GQA / MQA、rotary、ALiBi、滑动窗口 —— 以及 Qwen / vLLM / TensorRT-LLM 调用它的 API 形态。

**前置要求：** 第 1–7 讲。熟悉自回归大语言模型的 decode。

**产物：** 一个含分页 KV 与不含分页 KV 的 decode 步延迟 microbenchmark，外加一个小型 RoPE / GQA 正确性检查，用来确认 kernel 内 rotary 与独立的 rotary 参考实现一致。

---

## 为什么重要

训练由 *prefill（首字前的整段计算） / forward* 形态主导（`B × N × H × D`）。推理在 prompt 处理完之后，由 *decode* 形态主导（`B × 1 × H × D`）—— 每个 batch 元素一个 query token，对所有已生成的 KV 做 attention。kernel 设计不同：不再有填满一个 SM 的 `B_r × d` query 分块，只有单行，GPU 面临的挑战是每步读一次 KV cache，同时填满自身宽度。

推理服务栈（vLLM、SGLang、TensorRT-LLM、NVIDIA Triton）的成败系于这个 kernel。H200 上单请求的最佳 TPS，取决于能否以尽可能小的每 token 开销，干净地从 HBM 流式读取 KV。

---

## 心智模型

### Decode 形态与变化之处

| | Prefill | Decode |
|---|---------|--------|
| `q_len` | 完整 prompt（如 32k） | 1 |
| `kv_len` | 与 q_len 相同 | 每步增长 1 |
| 瓶颈 | 计算（矩阵乘受限） | 内存带宽（读 KV + 权重） |
| FA kernel | `flash_attn_func`、`flash_attn_varlen_func` | `flash_attn_with_kvcache` |

因为 `q_len = 1`，不再有填满一个 SM 的 `B_r × d` query 分块。kernel 转而并行于 **batch × head × KV-tile**，并使用小得多的 `(1, B_c)` 矩阵乘累加形态。张量核心偏好更大的矩阵乘，因此 decode 始终低于 GEMM（矩阵-矩阵乘）峰值 —— 即便实现得完美，通常也只有峰值 FLOPs 的 30–50%。

### kernel 内 KV cache 更新

每个 decode 步都为当前 token 计算新的 `k_new` 和 `v_new`，并写入 KV cache 的位置 `cur_pos`。融合推理 kernel 在 **kernel 内部**完成这一步：

```
flash_attn_with_kvcache(q, k_cache, v_cache, k_new, v_new, cache_seqlens, ...)
```

没有单独的 kv_append kernel 启动 —— 做 attention 的同一个 kernel 也会写入新 token。这样每个 token 省一次 kernel 启动。对 80 层、32 ms/token 的模型，即省下 80 次启动 × ~5 µs = 400 µs/token，在 50 TPS 下约合 10% TPS。

### 分页 KV cache

对大批量推理服务，无法为每个请求预分配连续的 KV 张量（长度可变会造成内存浪费）。KV 改为切分成固定大小的 **页**（如 16 token × `H × D`），每个请求用一张 **页表** 把逻辑位置映射到物理页。

attention kernel 从 `k_cache[block_indices[i]]` 而非连续内存块读取 —— 计算相同，只是间接寻址加载。FlashInfer 和 FA 的 `with_kvcache` 都支持这一点；页大小是 kernel 期常量（通常为 16、32 或 64）。

### MQA / GQA

- **MHA**（多头注意力）：每个 Q head 对应一个 K、一个 V。KV 大小 = `N × H × D`。
- **GQA**（分组查询注意力）：K 和 V 在若干组 Q head 之间共享。当 `H_kv = H / G` 时，KV 大小 = `N × H_kv × D`。KV 带宽降低 `G`。
- **MQA**（多查询注意力）：`H_kv = 1`。KV 带宽节省最多，质量最差。

Qwen2.5 和 Llama 3 使用 GQA，`G = 4` 或 `8`。attention kernel 通过在组内广播 K/V 来处理 GQA —— 同一个 K·V 分块与多个 Q head 计算。

### RoPE（旋转位置编码）

每个 Q 和 K 向量在点积前都会被一个位置相关的矩阵 `R(pos)` 旋转。实现正确时，它把位置信息烘焙进 attention 分数，无需加性偏置。

两种方案：

1. **外部 rotary kernel** —— 调用单独的 kernel 旋转 Q 和 K，再传给 attention。
2. **kernel 内 rotary** —— attention kernel 在加载 Q 和 K 时将其旋转。

`flash_attn_with_kvcache` 通过 `rotary_cos` / `rotary_sin` 参数支持方案 2。它每个 token 省一次 kernel 启动。代价是每个元素多几次 FMA —— 可忽略。

### ALiBi（Attention with Linear Biases）

给 attention 分数加上逐 head 的线性偏置 `-m_h · |i - j|`。位置不需要 K 矩阵。FA 通过 `alibi_slopes` 参数支持它。在现代大语言模型中不常见，但 MPT 和某些实验性架构用到。

### 滑动窗口 attention

每个 query 只 attend 最后 `window` 个 key。`flash_attn_func(window_size=(left, right))` 裁剪 attention 范围 —— 代码路径相同，只是掩码更紧。用于 Mistral 和某些 Gemma 变体。

---

## 动手实现


<details>
<summary>English original</summary>

**Lecture 8 — Inference Path: KV Cache, Decode, RoPE, GQA, Paged KV**

**Parent:** [FlashAttention Course](Guide.md)

**One-line purpose:** Understand the decode-time variant of FlashAttention — `q_len = 1`, in-kernel KV cache update, paged KV, GQA / MQA, rotary, ALiBi, sliding window — and the shape of the APIs that Qwen / vLLM / TensorRT-LLM use to call it.

**Prerequisites:** Lectures 1–7. Familiarity with autoregressive LLM decoding.

**Artifact:** A microbenchmark of decode-step latency with and without paged KV, plus a small RoPE / GQA sanity check that confirms the in-kernel rotary matches a separate-rotary reference.

---

**Why it matters**

Training is dominated by the *prefill / forward* shape (`B × N × H × D`). Inference, after the prompt is processed, is dominated by the *decode* shape (`B × 1 × H × D`) — one query token per batch element, attending over all previously-generated KV. The kernel design is different: you no longer have a `B_r × d` query tile, you have a single row, and the GPU's challenge is to fill its width while reading the KV cache once per step.

Serving stacks (vLLM, SGLang, TensorRT-LLM, NVIDIA Triton) live or die by this kernel. Single-request best-TPS on H200 is bounded by how cleanly you stream KV from HBM with the smallest possible overhead per token.

---

**Mental model**

**Decode shape and what changes**

| | Prefill | Decode |
|---|---------|--------|
| `q_len` | full prompt (e.g. 32k) | 1 |
| `kv_len` | same as q_len | grows by 1 per step |
| Bottleneck | compute (matmul-bound) | memory bandwidth (read KV + weights) |
| FA kernel | `flash_attn_func`, `flash_attn_varlen_func` | `flash_attn_with_kvcache` |

Because `q_len = 1`, you no longer have the `B_r × d` query tile that filled an SM. Instead, the kernel parallelises across **batch × head × KV-tile** and uses a much smaller `(1, B_c)` MMA shape. Tensor cores prefer larger matmuls, so decode is consistently below GEMM peak — typically 30–50% of peak FLOPs even when implemented perfectly.

**In-kernel KV cache update**

Every decode step you compute new `k_new` and `v_new` for the current token and write them into the KV cache at position `cur_pos`. The fused inference kernel does this **inside the kernel**:

```
flash_attn_with_kvcache(q, k_cache, v_cache, k_new, v_new, cache_seqlens, ...)
```

There is no separate kv_append kernel launch — the same kernel that does attention also writes the new tokens. That saves a kernel launch per token. For a model with 80 layers and 32 ms/token, that is 80 launches saved × ~5 µs = 400 µs/token, or ~10% TPS at 50 TPS.

**Paged KV cache**

For large-batch serving you cannot pre-allocate a contiguous KV tensor per request (memory waste from variable lengths). Instead, KV is split into fixed-size **pages** (e.g. 16 tokens × `H × D`) and a **page table** per request maps logical positions to physical pages.

The attention kernel reads from `k_cache[block_indices[i]]` rather than a contiguous slab — same compute, indirected loads. FlashInfer and FA's `with_kvcache` both support this; the page size is a kernel-time constant (typically 16, 32, or 64).

**MQA / GQA**

- **MHA** (multi-head attention): one K and one V per Q head. KV size = `N × H × D`.
- **GQA** (grouped-query attention): K and V are shared across groups of Q heads. With `H_kv = H / G`, KV size = `N × H_kv × D`. Reduces KV bandwidth by `G`.
- **MQA** (multi-query attention): `H_kv = 1`. Biggest KV bandwidth saving, worst quality.

Qwen2.5 and Llama 3 use GQA with `G = 4` or `8`. The attention kernel handles GQA by broadcasting K/V across the group — you compute the same K·V tile against multiple Q heads.

**RoPE (rotary position embedding)**

Each Q and K vector is rotated by a position-dependent matrix `R(pos)` before the dot product. Done correctly, it bakes positional information into the attention scores without an additive bias.

Two options:

1. **External rotary kernel** — call a separate kernel that rotates Q and K, then pass to attention.
2. **In-kernel rotary** — the attention kernel rotates Q and K as it loads them.

`flash_attn_with_kvcache` supports option 2 via `rotary_cos` / `rotary_sin` arguments. It saves a kernel launch per token. The cost is a few extra FMAs per element — negligible.

**ALiBi (Attention with Linear Biases)**

Adds a per-head linear bias `-m_h · |i - j|` to the attention scores. No K matrix needed for position. FA supports it via the `alibi_slopes` argument. Not common in modern LLMs but used in MPT and some experimental architectures.

**Sliding-window attention**

Each query attends only to the last `window` keys. `flash_attn_func(window_size=(left, right))` clips the attention range — same code path, just a tighter mask. Used in Mistral, some Gemma variants.

---

**Build it**

</details>

### 1. Decode 步延迟 benchmark

```python
# decode_bench.py
import torch
import torch.cuda
from flash_attn import flash_attn_with_kvcache

B, H, D = 1, 32, 128
H_kv = 8     # GQA group=4
N_max = 32768

q = torch.randn(B, 1, H, D, device="cuda", dtype=torch.bfloat16)
k_cache = torch.randn(B, N_max, H_kv, D, device="cuda", dtype=torch.bfloat16)
v_cache = torch.randn_like(k_cache)
k_new = torch.randn(B, 1, H_kv, D, device="cuda", dtype=torch.bfloat16)
v_new = torch.randn_like(k_new)

def step(cur_len):
    cache_seqlens = torch.tensor([cur_len], device="cuda", dtype=torch.int32)
    return flash_attn_with_kvcache(
        q, k_cache, v_cache, k=k_new, v=v_new,
        cache_seqlens=cache_seqlens, causal=True,
    )

# Warmup
for L in [128, 1024, 4096, 16384]:
    _ = step(L)
torch.cuda.synchronize()

start = torch.cuda.Event(enable_timing=True)
end = torch.cuda.Event(enable_timing=True)
for L in [128, 1024, 4096, 16384, 32000]:
    times = []
    for _ in range(50):
        start.record(); _ = step(L); end.record()
        end.synchronize()
        times.append(start.elapsed_time(end))
    print(f"L={L:>5}  median={sorted(times)[25]:.3f} ms")
```

应当看到 decode 延迟随 KV 长度大致线性增长（带宽受限：每步每 layer 读取 `L × H_kv × D × 2` 字节）。在 `L = 16k` 下、7B 模型配 `H_kv = 8, D = 128` 时，即每 layer 每步 32 MB —— 对 32 layer 的模型就是每 token 约 1 GB。在 4.8 TB/s 下，即每 token 约 200 µs 的纯 KV 流量。

### 2. Paged 与 contiguous 对比（使用 FlashInfer）

FlashInfer 的 `BatchDecodeWithPagedKVCacheWrapper` 是标准的 paged decode API：

```python
import flashinfer
import torch

B, H, D, page_size, N = 1, 32, 128, 16, 4096
H_kv = 8
num_pages = N // page_size

q = torch.randn(B, H, D, device="cuda", dtype=torch.bfloat16)
k_cache = torch.randn(num_pages, page_size, H_kv, D, device="cuda", dtype=torch.bfloat16)
v_cache = torch.randn_like(k_cache)
indices = torch.arange(num_pages, device="cuda", dtype=torch.int32)
indptr  = torch.tensor([0, num_pages], device="cuda", dtype=torch.int32)
last_page_len = torch.tensor([page_size], device="cuda", dtype=torch.int32)

wrapper = flashinfer.BatchDecodeWithPagedKVCacheWrapper(
    torch.empty(128*1024*1024, dtype=torch.uint8, device="cuda"),
    kv_layout="NHD",
)
wrapper.plan(indptr, indices, last_page_len, H, H_kv, D, page_size, dtype=torch.bfloat16)
o = wrapper.run(q, (k_cache, v_cache))
print(o.shape)
```

把每步延迟与第 1 步得到的 FA `with_kvcache` 基线对比。在 H100/H200 上，paged decode 与 contiguous decode 的差距应在 10–15% 以内；否则 page table 的间接寻址就是瓶颈，需要调 page size。

### 3. RoPE / GQA 正确性检查

```python
# rope_gqa_check.py
# Verifies that in-kernel rotary in flash_attn_with_kvcache produces the same
# output as separately-applied rotary + attention.
```

在外部施加 rotary（使用代码库中的 `rotary_embedding` 或 PyTorch），然后在不传 `rotary_cos`/`rotary_sin` 的情况下调用 attention。另外，在不施加外部 rotary 的情况下调用 `flash_attn_with_kvcache(rotary_cos=..., rotary_sin=...)`。用 `atol = 5e-3` 在 bf16 下比较输出。两者必须一致。

---

## 在真实技术栈中使用

- **vLLM**：decode 时 `vllm/attention/backends/flash_attn.py` 调用 `flash_attn_with_kvcache`，chunked prefill 时调用 `flash_attn_varlen_func`。
- **SGLang**：模式类似，另在其上叠加自有的 RadixAttention layer 用于前缀缓存。
- **TensorRT-LLM**：使用自有的 attention kernel（融合 MQA/GQA + RoPE），但形状上与 FA 所提供的完全一致。
- **曾参与的 cacheon-sglang-miner repo**：手写的 FlashInfer paged decode 封装见 `cuda/src/kernels/attention_flashinfer.cu`。这是该 API 在生产环境中的一个完整示例。

逐个略读即可。模式是重复的：prefill 走 varlen，decode 走 paged/with_kvcache，在 decode 步周围做 CUDA Graph 捕获以摊薄 launch 开销。

---

## 实测

decode benchmark 应报告：

- **TTFT**（首 token 时延）—— prefill 时延。
- **TPS**（tokens per second），针对长生成 —— 1 / decode 步时间。
- **实测 HBM 带宽**（在 `L_max` 下）：应达 GPU 峰值的 70–90%。若更低，说明存在 launch 或调度开销，而非带宽受限的 kernel。
- **KV cache 显存占用**，每 token 每 layer：`2 · H_kv · D · dtype_bytes`。在 32k context 下，它往往远超激活值显存。

如果你的推理服务栈使用 CUDA Graph（大多数都会用），务必在捕获 CUDA Graph **之后** 做 benchmark。对短序列，捕获前与捕获后的数字可能相差 2 倍。

---

## 交付

提交到 `flash-attn-course/`：

1. `decode_bench.py`，以及一个跨 `L ∈ {128, 1k, 4k, 16k, 32k}` 的 `decode_latency.csv`。
2. 在单一 shape 下对比 FA `with_kvcache` 与 FlashInfer paged decode 的 `paged_vs_contig.csv`。
3. `rope_gqa_check.py`，附带通过容差的报告。

有了这三项，你就能就世界上任何推理服务栈展开有依据的讨论。

---


<details>
<summary>English original</summary>

**1. Decode-step latency benchmark**

```python
# decode_bench.py
import torch
import torch.cuda
from flash_attn import flash_attn_with_kvcache

B, H, D = 1, 32, 128
H_kv = 8     # GQA group=4
N_max = 32768

q = torch.randn(B, 1, H, D, device="cuda", dtype=torch.bfloat16)
k_cache = torch.randn(B, N_max, H_kv, D, device="cuda", dtype=torch.bfloat16)
v_cache = torch.randn_like(k_cache)
k_new = torch.randn(B, 1, H_kv, D, device="cuda", dtype=torch.bfloat16)
v_new = torch.randn_like(k_new)

def step(cur_len):
    cache_seqlens = torch.tensor([cur_len], device="cuda", dtype=torch.int32)
    return flash_attn_with_kvcache(
        q, k_cache, v_cache, k=k_new, v=v_new,
        cache_seqlens=cache_seqlens, causal=True,
    )

# Warmup
for L in [128, 1024, 4096, 16384]:
    _ = step(L)
torch.cuda.synchronize()

start = torch.cuda.Event(enable_timing=True)
end = torch.cuda.Event(enable_timing=True)
for L in [128, 1024, 4096, 16384, 32000]:
    times = []
    for _ in range(50):
        start.record(); _ = step(L); end.record()
        end.synchronize()
        times.append(start.elapsed_time(end))
    print(f"L={L:>5}  median={sorted(times)[25]:.3f} ms")
```

You should see decode latency grow roughly linearly with KV length (memory-bound: each step reads `L × H_kv × D × 2` bytes per layer). At `L = 16k` for 7B with `H_kv = 8, D = 128`, that is 32 MB per layer per step — for a 32-layer model that is ~1 GB per token. At 4.8 TB/s, that is ~200 µs of pure KV traffic per token.

**2. Paged-vs-contiguous comparison (uses FlashInfer)**

FlashInfer's `BatchDecodeWithPagedKVCacheWrapper` is the canonical paged decode API:

```python
import flashinfer
import torch

B, H, D, page_size, N = 1, 32, 128, 16, 4096
H_kv = 8
num_pages = N // page_size

q = torch.randn(B, H, D, device="cuda", dtype=torch.bfloat16)
k_cache = torch.randn(num_pages, page_size, H_kv, D, device="cuda", dtype=torch.bfloat16)
v_cache = torch.randn_like(k_cache)
indices = torch.arange(num_pages, device="cuda", dtype=torch.int32)
indptr  = torch.tensor([0, num_pages], device="cuda", dtype=torch.int32)
last_page_len = torch.tensor([page_size], device="cuda", dtype=torch.int32)

wrapper = flashinfer.BatchDecodeWithPagedKVCacheWrapper(
    torch.empty(128*1024*1024, dtype=torch.uint8, device="cuda"),
    kv_layout="NHD",
)
wrapper.plan(indptr, indices, last_page_len, H, H_kv, D, page_size, dtype=torch.bfloat16)
o = wrapper.run(q, (k_cache, v_cache))
print(o.shape)
```

Compare the per-step latency to the FA `with_kvcache` baseline you produced in step 1. Paged decode should be within 10–15% of contiguous decode on H100/H200; if not, the page-table indirection is your bottleneck and you tune page size.

**3. RoPE / GQA sanity check**

```python
# rope_gqa_check.py
# Verifies that in-kernel rotary in flash_attn_with_kvcache produces the same
# output as separately-applied rotary + attention.
```

Apply rotary externally (using `rotary_embedding` from your codebase or PyTorch), then call attention without `rotary_cos`/`rotary_sin`. Separately, call `flash_attn_with_kvcache(rotary_cos=..., rotary_sin=...)` without external rotary. Compare outputs at bf16 with `atol = 5e-3`. They must match.

---

**Use it in the real stack**

- **vLLM**: `vllm/attention/backends/flash_attn.py` calls `flash_attn_with_kvcache` for decode, `flash_attn_varlen_func` for chunked prefill.
- **SGLang**: similar pattern, plus their own RadixAttention layer on top for prefix caching.
- **TensorRT-LLM**: uses its own attention kernels (fused with MQA/GQA + RoPE) but shape-wise identical to what FA provides.
- **The cacheon-sglang-miner repo we worked on**: see `cuda/src/kernels/attention_flashinfer.cu` for a hand-written wrapper around FlashInfer's paged decode. It is a worked example of the API in production.

Skim each one. The patterns repeat: prefill via varlen, decode via paged/with_kvcache, CUDA-graph capture around the decode step to amortise launch overhead.

---

**Measure it**

For decode benchmarks, report:

- **TTFT** (time to first token) — prefill latency.
- **TPS** (tokens per second) for a long generation — 1 / decode-step time.
- **Achieved HBM bandwidth** at `L_max`: should be 70–90% of GPU peak. If lower, you have launch or scheduling overhead, not a memory-bound kernel.
- **KV-cache memory footprint** per token per layer: `2 · H_kv · D · dtype_bytes`. For 32k context this often dwarfs activation memory.

Always benchmark **after** capturing a CUDA graph if your serving stack uses graphs (most do). Pre-graph and post-graph numbers can differ by 2× for short sequences.

---

**Ship it**

Drop into `flash-attn-course/`:

1. `decode_bench.py` and a `decode_latency.csv` over `L ∈ {128, 1k, 4k, 16k, 32k}`.
2. `paged_vs_contig.csv` comparing FA `with_kvcache` and FlashInfer paged decode at one shape.
3. `rope_gqa_check.py` with a passing tolerance report.

If you have those three, you can have an informed conversation about any inference-serving stack on the planet.

---

</details>

## 相关页面

- [Lecture 7 — 反向 pass 与验证](第07讲-反向传播与数值验证.md)
- [Lecture 9 — Hopper / FA3 / FA4](第09讲-Hopper上的FA3与FA4.md)
- [DL 推理 runtime 与部署](../../05-推理运行时与部署/Guide.md)
- FlashInfer: <https://github.com/flashinfer-ai/flashinfer>


<details>
<summary>English original</summary>

**Related pages**

- [Lecture 7 — Backward pass and validation](第07讲-反向传播与数值验证.md)
- [Lecture 9 — Hopper / FA3 / FA4](第09讲-Hopper上的FA3与FA4.md)
- [DL Inference Runtimes and Deployment](../../05-推理运行时与部署/Guide.md)
- FlashInfer: <https://github.com/flashinfer-ai/flashinfer>

</details>

---

> 原文：[`Phase 4 - Track C - DL Inference Optimization/02 - Kernel Engineering/FlashAttention Course/Lecture 08 - Inference Path KV Cache and Decode.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20C%20-%20DL%20Inference%20Optimization/02%20-%20Kernel%20Engineering/FlashAttention%20Course/Lecture%2008%20-%20Inference%20Path%20KV%20Cache%20and%20Decode.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
