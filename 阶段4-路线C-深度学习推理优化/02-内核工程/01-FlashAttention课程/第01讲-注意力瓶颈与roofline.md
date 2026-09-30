---
title: Lecture 1 — Attention 瓶颈与 roofline（性能上界模型）
description: Lecture 1 — Attention 瓶颈与 roofline（性能上界模型）
---

# Lecture 1 — Attention 瓶颈与 roofline（性能上界模型）

**上级：** [FlashAttention Course](Guide.md)

**一句话目的：** 构建 IO/roofline 模型，解释为什么朴素 attention 是带宽受限的，以及一个“精确但 IO-aware”的算法需要改变什么。

**前置要求：** 熟悉标准 self-attention。能熟练进行基本的渐近分析。

**产物：** 一个简短的 Jupyter notebook，在固定 head 配置下绘制算术强度随序列长度的变化，并叠加 GPU 的 roofline。

---

## 为什么重要

attention 有三个步骤会读写大张量：

1. **分数：** `S = Q · Kᵀ / √d`，形状 `[N, N]`。
2. **Softmax：** `P = softmax(S)`，同样为 `[N, N]`。
3. **输出：** `O = P · V`，回到 `[N, d]`。

朴素实现会在 HBM 中物化完整的 `[N, N]` 矩阵 `S` 和 `P`。对于 bf16 下的 `N = 8192`，仅 `S` 就每个 head、每个 layer 要 128 MiB，`P` 还要再占 128 MiB。在 Hopper 级 GPU 上可提供约 3 TB/s 的 HBM 带宽，所以在做任何计算之前，光是搬运这两个矩阵就已经要花掉每个 head 约 90 µs。当 head 和 layer 数量很多时，模型大部分时间都在把 softmax 中间结果搬过 HBM，而不是在做矩阵乘。

这正是这样一类工作负载：**roofline 分析**会告诉你瓶颈是带宽而不是算力，而一次“IO-aware”的重写能在不改变结果的前提下带来大幅加速。

---

## 心智模型

### 两个矩阵乘与逐元素阶段

对每个 attention head，序列长度为 `N`、head 维度为 `d`：

| 步骤 | 操作 | FLOPs | 读取字节（bf16） | 写入字节（bf16） |
|------|----|-------|--------------------|----------------------|
| `S = QKᵀ` | 矩阵乘 | `2 N² d` | `2 (2 N d)` | `2 N²` |
| `softmax(S)` | 逐元素 | ~`5 N²` | `2 N²` | `2 N²` |
| `O = PV` | 矩阵乘 | `2 N² d` | `2 (N² + N d)` | `2 N d` |

总计算量：`4 N² d + O(N²)` FLOPs。
总 HBM 流量（朴素实现，所有中间结果都物化）：`O(N² + N d)` 字节。

### 算术强度

算术强度 `I = FLOPs / bytes`。roofline 指出，一个 kernel 最多只能达到 `min(peak_FLOPs, I · peak_bytes_per_sec)` FLOPs/s。

对于朴素 attention，由于 `S` 和 `P`，字节数随 `N²` 缩放。所以 `I ≈ 4 N² d / N² = O(d)` —— 与 `N` 无关。在 `d = 64` 与 bf16（2 字节）下，`I ≈ 128` FLOPs/byte。对比：

| GPU | bf16 峰值（TFLOPS） | HBM 带宽（TB/s） | Ridge point I（FLOPs/byte） |
|-----|--------------------|----------------|------------------------------|
| A100 80GB SXM | ~312 | 2.0 | ~156 |
| H100 SXM | ~989 | 3.35 | ~295 |
| H200 SXM | ~989 | 4.8 | ~206 |
| B200 SXM | ~2250 (bf16) | 8.0 | ~280 |

如果 kernel 的 `I` 低于 ridge，就是带宽受限。在 H100 上，`d = 64` 时的朴素 attention 其 `I ≈ 128`，ridge 约为 295 → 带宽受限。到 `d = 128` 时，`I ≈ 256` → 刚好略低于 ridge，仍以带宽受限为主。这就是为什么在长序列上主要开销是 softmax 矩阵的 HBM 流量。

### “精确但 IO-aware”的含义

数学上无法减少：结果仍然是 `softmax(QKᵀ/√d)·V`，逐 bit 一致（浮点归约顺序除外）。能改变的是*如何*遍历这些计算：在不把完整的 `[N, N]` 矩阵写入 HBM 的前提下产出 `O`。如果能在流式读取 `Q`、`K`、`V` 的同时，把 `S` 和 `P` 的分块保留在 SRAM（共享内存）中，就能把 HBM 流量从 `O(N²)` 降到 `O(N · d)` —— 在大多数现代 GPU 上，这会让你越过 ridge，进入算力受限区间。

这就是 FlashAttention 的全部前提。课程后续内容讲的是如何让这种遍历变得正确、快速且通用。

---


<details>
<summary>English original</summary>

**Lecture 1 — Attention Bottleneck and Roofline**

**Parent:** [FlashAttention Course](Guide.md)

**One-line purpose:** Build the IO/roofline model that explains why naive attention is memory-bound and what an "exact but IO-aware" algorithm needs to change.

**Prerequisites:** Familiarity with standard self-attention. Comfort with basic asymptotic analysis.

**Artifact:** A short Jupyter notebook that, for a fixed head config, plots arithmetic intensity vs sequence length and overlays the GPU's roofline.

---

**Why it matters**

Attention has three steps that read or write large tensors:

1. **Scores:** `S = Q · Kᵀ / √d`, shape `[N, N]`.
2. **Softmax:** `P = softmax(S)`, also `[N, N]`.
3. **Output:** `O = P · V`, back to `[N, d]`.

Naive implementations materialize the full `[N, N]` matrices `S` and `P` in HBM. For `N = 8192` with bf16 that is 128 MiB per head per layer just for `S`, and another 128 MiB for `P`. On a Hopper-class GPU you can deliver ~3 TB/s of HBM bandwidth, so moving those two matrices already costs around 90 µs per head before you do any math. With many heads and layers, the model spends most of its life moving softmax intermediates through HBM rather than doing matmuls.

This is exactly the kind of workload where **roofline analysis** tells you the bottleneck is bandwidth, not compute, and where an "IO-aware" rewrite gives big speedups without changing the answer.

---

**Mental model**

**The two matmuls and the elementwise stage**

Per attention head with sequence length `N` and head dim `d`:

| Step | Op | FLOPs | Bytes read (bf16) | Bytes written (bf16) |
|------|----|-------|--------------------|----------------------|
| `S = QKᵀ` | matmul | `2 N² d` | `2 (2 N d)` | `2 N²` |
| `softmax(S)` | elementwise | ~`5 N²` | `2 N²` | `2 N²` |
| `O = PV` | matmul | `2 N² d` | `2 (N² + N d)` | `2 N d` |

Total compute: `4 N² d + O(N²)` FLOPs.
Total HBM traffic (naive, all intermediates materialized): `O(N² + N d)` bytes.

**Arithmetic intensity**

Arithmetic intensity `I = FLOPs / bytes`. The roofline says a kernel can hit at most `min(peak_FLOPs, I · peak_bytes_per_sec)` FLOPs/s.

For naive attention, the bytes scale with `N²` because of `S` and `P`. So `I ≈ 4 N² d / N² = O(d)` — independent of `N`. With `d = 64` and bf16 (2 bytes), `I ≈ 128` FLOPs/byte. Compare:

| GPU | Peak bf16 (TFLOPS) | HBM BW (TB/s) | Ridge point I (FLOPs/byte) |
|-----|--------------------|----------------|------------------------------|
| A100 80GB SXM | ~312 | 2.0 | ~156 |
| H100 SXM | ~989 | 3.35 | ~295 |
| H200 SXM | ~989 | 4.8 | ~206 |
| B200 SXM | ~2250 (bf16) | 8.0 | ~280 |

If your kernel's `I` is below the ridge, you are memory-bound. Naive attention at `d = 64` on H100 has `I ≈ 128`, ridge is ~295 → memory-bound. At `d = 128`, `I ≈ 256` → just under the ridge, still mostly memory-bound. This is why the dominant cost on long sequences is HBM traffic on the softmax matrix.

**What "exact but IO-aware" means**

You cannot reduce the math: the answer is still `softmax(QKᵀ/√d)·V`, bit-for-bit (modulo floating-point reduction order). What you can change is *how* you traverse the math: produce `O` without ever writing the full `[N, N]` matrix to HBM. If you can keep `S` and `P` tiles inside SRAM (shared memory) while you stream `Q`, `K`, `V`, you cut the HBM traffic from `O(N²)` to `O(N · d)` — which moves you above the ridge and into compute-bound territory on most modern GPUs.

That is the entire premise of FlashAttention. The rest of the course is about how to make that traversal correct, fast, and general.

---

</details>

## 动手实现

阅读 FA1 论文第 2 节（Background）和第 3.1 节（Standard attention IO complexity）。然后做这个 notebook：

```python
# attention_roofline.py
import numpy as np
import matplotlib.pyplot as plt

def naive_attention_bytes(N, d, dtype_bytes=2):
    # Q, K, V reads
    qkv_read = 3 * N * d * dtype_bytes
    # S = QK^T write, P = softmax(S) read+write, P read for PV
    s_write = N * N * dtype_bytes
    p_rw = 2 * N * N * dtype_bytes
    p_read_for_pv = N * N * dtype_bytes
    # O write
    o_write = N * d * dtype_bytes
    return qkv_read + s_write + p_rw + p_read_for_pv + o_write

def attention_flops(N, d):
    return 4 * N * N * d  # the two matmuls dominate

def intensity(N, d):
    return attention_flops(N, d) / naive_attention_bytes(N, d)

def flash_attention_bytes(N, d, dtype_bytes=2):
    # Stream Q, K, V from HBM; write O; never materialize S or P.
    return (3 * N * d + N * d) * dtype_bytes

GPUS = {
    "A100 80GB":  (312e12, 2.0e12),
    "H100 SXM":   (989e12, 3.35e12),
    "H200 SXM":   (989e12, 4.8e12),
}

d = 128
Ns = [256, 512, 1024, 2048, 4096, 8192, 16384, 32768]
print(f"head_dim={d}, dtype=bf16")
print(f"{'N':>6} {'naive_I':>10} {'fa_I':>10}")
for N in Ns:
    fa_I = attention_flops(N, d) / flash_attention_bytes(N, d)
    print(f"{N:>6} {intensity(N, d):>10.1f} {fa_I:>10.1f}")
```

运行它，保存表格，并针对每块 GPU 记录：在 naive 布局与 FlashAttention 布局下，哪些 `(N, d)` 组合会落到 ridge 之下。

---

## 在真实技术栈中使用

打开 `torch.nn.functional.scaled_dot_product_attention`（PyTorch 的 `SDPA`）。它有三个后端：`math`（naive、物化实现版本）、`flash`（FlashAttention）和 `efficient`（xFormers）。`math` 后端存在的意义正是让你对照它做正确性比较；它也是 IO 复杂度与你在 roofline（性能上界模型）上的「naive」线相吻合的那个 kernel。

```python
import torch
import torch.nn.functional as F

q = torch.randn(1, 8, 4096, 64, device="cuda", dtype=torch.bfloat16)
k = torch.randn_like(q); v = torch.randn_like(q)

with torch.nn.attention.sdpa_kernel([torch.nn.attention.SDPBackend.MATH]):
    o_math = F.scaled_dot_product_attention(q, k, v)
with torch.nn.attention.sdpa_kernel([torch.nn.attention.SDPBackend.FLASH_ATTENTION]):
    o_flash = F.scaled_dot_product_attention(q, k, v)

print("max abs diff:", (o_math - o_flash).abs().max().item())
```

你应该看到正确性落在 bf16 容差范围内，以及很大的延迟差距。对两者都计时，并把差值与你在实验中算出的字节数比值做比较。

---

## 度量它

- 用 `torch.cuda.Event` 计时。
- 用 fp16 / bf16 运行，绝不要 fp32 —— 这是现实中 LLM 训练/推理唯一会用的 dtype。
- 计时前先对 kernel 做 warm up。
- 报告 `tokens/sec`（或 kernel 时间），而不只是绝对毫秒数。

对每个 shape，计算：

`achieved_bandwidth ≈ kernel_bytes / kernel_time`

并与你的 GPU 峰值 HBM 带宽比较。对 naive attention，你应该接近 HBM 峰值。对 FlashAttention，你应该远低于 HBM 峰值（因为读得更少），但 TFLOPS 利用率很高。

---

## 交付它

把这个 notebook 提交到你的 `flash-attn-course/` 目录。它应该产出：

1. 一张打印出来的 `(N, d, naive_I, fa_I)` 表格，至少覆盖三个 `d` 取值。
2. 一张 matplotlib roofline 图，针对你实际拥有 / 租用的一块 GPU，在 `N ∈ {1024, 4096, 16384}` 处标出 naive 与 FA attention 的标记点。
3. 一个两句话的书面结论：「Naive attention 在 d={...} 时于 N={...} 处跌到 ridge 之下；FA 在整个扫描范围内都保持在 ridge 之上。」

如果你能做出这些，你就拥有了课程后续唯一需要的思维模型。

---

## 相关页面

- [02 — Kernel Engineering](../Guide.md)
- [Lecture 2 — Online softmax and numerical correctness](第02讲-在线softmax与数值正确性.md)


<details>
<summary>English original</summary>

**Build it**

Read the FA1 paper, Section 2 (Background) and Section 3.1 (Standard attention IO complexity). Then make this notebook:

```python
# attention_roofline.py
import numpy as np
import matplotlib.pyplot as plt

def naive_attention_bytes(N, d, dtype_bytes=2):
    # Q, K, V reads
    qkv_read = 3 * N * d * dtype_bytes
    # S = QK^T write, P = softmax(S) read+write, P read for PV
    s_write = N * N * dtype_bytes
    p_rw = 2 * N * N * dtype_bytes
    p_read_for_pv = N * N * dtype_bytes
    # O write
    o_write = N * d * dtype_bytes
    return qkv_read + s_write + p_rw + p_read_for_pv + o_write

def attention_flops(N, d):
    return 4 * N * N * d  # the two matmuls dominate

def intensity(N, d):
    return attention_flops(N, d) / naive_attention_bytes(N, d)

def flash_attention_bytes(N, d, dtype_bytes=2):
    # Stream Q, K, V from HBM; write O; never materialize S or P.
    return (3 * N * d + N * d) * dtype_bytes

GPUS = {
    "A100 80GB":  (312e12, 2.0e12),
    "H100 SXM":   (989e12, 3.35e12),
    "H200 SXM":   (989e12, 4.8e12),
}

d = 128
Ns = [256, 512, 1024, 2048, 4096, 8192, 16384, 32768]
print(f"head_dim={d}, dtype=bf16")
print(f"{'N':>6} {'naive_I':>10} {'fa_I':>10}")
for N in Ns:
    fa_I = attention_flops(N, d) / flash_attention_bytes(N, d)
    print(f"{N:>6} {intensity(N, d):>10.1f} {fa_I:>10.1f}")
```

Run it, save the table, and write down for each GPU which `(N, d)` combinations fall below the ridge with the naive layout vs the FlashAttention layout.

---

**Use it in the real stack**

Open `torch.nn.functional.scaled_dot_product_attention` (PyTorch's `SDPA`). It has three backends: `math` (the naive, materialised version), `flash` (FlashAttention), and `efficient` (xFormers). The `math` backend exists precisely so you can compare against it for correctness; it is also the kernel whose IO complexity matches your "naive" line on the roofline.

```python
import torch
import torch.nn.functional as F

q = torch.randn(1, 8, 4096, 64, device="cuda", dtype=torch.bfloat16)
k = torch.randn_like(q); v = torch.randn_like(q)

with torch.nn.attention.sdpa_kernel([torch.nn.attention.SDPBackend.MATH]):
    o_math = F.scaled_dot_product_attention(q, k, v)
with torch.nn.attention.sdpa_kernel([torch.nn.attention.SDPBackend.FLASH_ATTENTION]):
    o_flash = F.scaled_dot_product_attention(q, k, v)

print("max abs diff:", (o_math - o_flash).abs().max().item())
```

You should see correctness within bf16 tolerance and a large latency gap. Time both, and compare the difference to the byte-count ratio you computed in the lab.

---

**Measure it**

- Use `torch.cuda.Event` for timing.
- Run at fp16 / bf16, never fp32 — these are the only realistic LLM training/inference dtypes.
- Warm up the kernel before timing.
- Report `tokens/sec` (or kernel time), not just absolute milliseconds.

For each shape, compute:

`achieved_bandwidth ≈ kernel_bytes / kernel_time`

and compare to your GPU's peak HBM bandwidth. For naive attention you should be close to peak HBM. For FlashAttention you should be far below HBM peak (because you read less) but high on TFLOPS utilisation.

---

**Ship it**

Commit the notebook to your `flash-attn-course/` directory. It should produce:

1. A printed table of `(N, d, naive_I, fa_I)` for at least three `d` values.
2. A matplotlib roofline plot for one GPU you actually own / rent, with markers for naive and FA attention at `N ∈ {1024, 4096, 16384}`.
3. A two-sentence written conclusion: "Naive attention at d={...} crosses below the ridge at N={...}; FA stays above the ridge across the entire sweep."

If you can produce that, you have the only mental model you need for the rest of the course.

---

**Related pages**

- [02 — Kernel Engineering](../Guide.md)
- [Lecture 2 — Online softmax and numerical correctness](第02讲-在线softmax与数值正确性.md)

</details>

---

> 原文：[`Phase 4 - Track C - DL Inference Optimization/02 - Kernel Engineering/FlashAttention Course/Lecture 01 - Attention Bottleneck and Roofline.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20C%20-%20DL%20Inference%20Optimization/02%20-%20Kernel%20Engineering/FlashAttention%20Course/Lecture%2001%20-%20Attention%20Bottleneck%20and%20Roofline.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
