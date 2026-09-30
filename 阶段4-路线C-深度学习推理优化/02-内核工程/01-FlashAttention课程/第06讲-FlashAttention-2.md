---
title: 第 6 讲 — FlashAttention-2
description: 第 6 讲 — FlashAttention-2
---

# 第 6 讲 — FlashAttention-2

**Parent:** [FlashAttention Course](Guide.md)

**一句话目的：** 解释为何在算法相同的情况下 FA2 比 FA1 快约 2× —— 关键在于理解工作划分上的改动：序列并行、按 head 并行、warp 划分，以及共享内存流量的减少。

**前置要求：** 第 1–5 讲。你应当已从头到尾读完 `flash_fwd_kernel.h`。

**产物：** 一张 benchmark 表，在你的 GPU 上对 PyTorch SDPA、FA2 和一个「FA1-style」参考实现（可用 Triton 实现伪造）做 `(seq_len × head_dim)` 扫描对比。

---

## 为什么重要

FA1 带来的是算法上的突破，FA2 带来的是工程上的突破。在典型 attention shape 下，FA2 能达到 **GPU 可提供的理论最大 GEMM 吞吐的 50–70%** —— 与 GEMM 之间的差距，正是剩余那点非 matmul FLOPs（softmax 指数、rescale）所在之处。理解了 FA2 所做的四处改动，你也就理解了 FA3 在 Hopper 上为何走那条路线。

---

## 心智模型

### FA1 留下了什么没做

FA1 的内层循环是：**对每个 query 分块，遍历所有 KV 分块，更新 `(m, ℓ, O)`**。这在真实 GPU 上有三处低效：

1. **独立的线程块太少。** Grid 是 `(num_q_tiles, num_heads, batch)`。对于小 batch × 短 seq × 少 head 的情形，GPU 填不满。
2. **每个块的归约都要经过共享内存。** 每个分块的 `m_ij` 和 `ℓ_ij` 归约都写入共享内存并同步线程。
3. **非 matmul FLOPs 与做 MMA 的是同一批 warp。** 指数 / rescale 的工作拖住了 matmul 流水线。

FA2 把这三处全修好了。

### FA2 改动 1：在序列维度上并行

在 FA1 中，对 `seq_len` 的处理被串行化在单个块内（外层的 `i` 循环）。FA2 把这个循环变成**一个 grid 维度**。现在 grid 是 `(num_q_tiles, num_heads, batch)` *它们全都贡献独立的线程块*，而对 KV 分块的内层循环则是唯一剩下的串行循环。

对于小 batch × 少 head × 长序列（微调 / 推理 prefill——首字前的整段计算——的典型情形），这是最大的一处收益。线程块从 8 个变成 128 个，GPU 就被填满了。

### FA2 改动 2：块内 warp 按 head/KV 并行

FA1 中一个块里的所有 warp 都在处理同一个 query 分块。FA2 把 warp 拆开，让同一块内不同的 warp 处理同一 query 分块的不同 KV 列。好处是：每个 warp 的 `MMA` 写入落到输出累加器的不同行，GEMM 部分**不需要 warp 间同步**。块级归约只在最后做一次，把各 warp 的部分 `(m, ℓ)` 合并起来。

### FA2 改动 3：softmax 统计量放在寄存器里，而非共享内存

FA1 在 rowmax 与 rescale 之间把 `m_ij`、`ℓ_ij` 溢出到共享内存。FA2 把两者都留在寄存器里，并用 warp shuffle（`__shfl_xor_sync`）做按行的归约。共享内存则留给 matmul 分块。

实际效果：共享内存压力下降，可以放下更大的 `B_r × d` 和 `B_c × d` 分块，warp 也不会因为要等 softmax 运算而在 `__syncthreads()` 上停顿。

### FA2 改动 4：减少 `O` 的 rescale 次数

FA1 每次内层迭代都用 `rescale_old = exp(m_old - m_new)` 对运行中的 `O_i` 累加器做 rescale。FA2 发现可以推迟 rescale：让 `O` 「处于错误的缩放」直到内层循环结束，再在 epilogue 里一次性 rescale。这样每次内层迭代就在整个 `O` 累加器上省掉一次乘法 —— 在 `d` 较大时很有意义。

数学上依然成立，因为：

```
O_final = exp(m_last - m_global) · O_accumulated_unrescaled
ℓ_final = … (same trick)
O = O_final / ℓ_final
```

每个输出行只需要最后一次除法和最后一次标量乘法。

### 净效果

这四处改动合起来，让 FA2 在 A100/H100 上、相同 `(M, N, K)` shape 下达到纯 GEMM 的约 70% —— 已接近实际天花板，因为 softmax 指数占了 `O(N·d)` 非 matmul FLOPs 中相当可观的一部分。

---

## 动手实现

你不必重新实现 FA2，只需阅读它并做 benchmark。

### 阅读任务

打开 `csrc/flash_attn/src/flash_fwd_kernel.h`，定位以下内容：

1. grid 的启动维度（在 `flash_fwd_launch_template.h` 中找 `dim3 grid(...)`）。确认它是 `(num_m_blocks, num_heads, batch)`。**这就是 FA2 改动 1。**
2. `S = QKᵀ` 累加器的 warp 划分（`tSrS`）。`Tiled_mma` 类型说明了 warp 是怎么拆分的 —— 在 `kernel_traits.h` 中搜索 `TiledMma` 和 `using` 别名。**这就是 FA2 改动 2。**
3. `softmax.h` 中的 `softmax_rescale_o_` 和 `softmax_template` —— 是 warp shuffle 归约，不是共享内存归约。**这就是 FA2 改动 3。**
4. kernel 末尾那个推迟的除以 `ℓ` 的最终操作 —— 搜索写出 `tOrO / lse` 的 epilogue 块。**这就是 FA2 改动 4。**

写一份简短的笔记，把 FA2 论文中的每处改动对应到一个具体的代码位置。没有这份笔记，你就没法调试 FA2 的补丁。


<details>
<summary>English original</summary>

**Lecture 6 — FlashAttention-2**

**Parent:** [FlashAttention Course](Guide.md)

**One-line purpose:** Explain why FA2 is ~2× faster than FA1 at the same algorithm, by understanding the work-partitioning changes — sequence parallelism, per-head parallelism, warp partitioning, and reduced shared-memory traffic.

**Prerequisites:** Lectures 1–5. You should have read `flash_fwd_kernel.h` end to end.

**Artifact:** A benchmark table comparing PyTorch SDPA, FA2, and a "FA1-style" reference (you can fake this with a Triton implementation) across a `(seq_len × head_dim)` sweep on your GPU.

---

**Why it matters**

FA1 had the algorithmic breakthrough; FA2 had the engineering breakthrough. On a typical attention shape, FA2 sits at **50–70% of the theoretical max GEMM throughput** the GPU can deliver — the gap to GEMM is where the few remaining non-matmul FLOPs (softmax exponentials, rescales) live. If you understand the four changes FA2 made you also understand why FA3 went the way it did on Hopper.

---

**Mental model**

**What FA1 left on the table**

FA1's inner loop was: **for each query tile, loop over all KV tiles, update `(m, ℓ, O)`**. That has three inefficiencies on real GPUs:

1. **Few independent thread blocks.** Grid was `(num_q_tiles, num_heads, batch)`. For small batch × short seq × few heads, you cannot fill the GPU.
2. **Per-block reductions hit shared memory.** Each tile's `m_ij` and `ℓ_ij` reductions wrote to shared memory and synced threads.
3. **Non-matmul FLOPs ran on the same warps that did MMAs.** The exponential / rescale work stalled the matmul pipeline.

FA2 fixes all three.

**FA2 change 1: parallelise over the sequence dimension**

In FA1, work over `seq_len` was serialised inside one block (the outer-`i` loop). FA2 makes that loop a **grid dimension**. Grid is now `(num_q_tiles, num_heads, batch)` *all of which contribute independent thread blocks*, plus the inner loop over KV tiles is the only remaining serial loop.

For small batch × few heads × long sequence (typical of fine-tuning / inference prefill), this is the single biggest win. You go from 8 blocks to 128 blocks; the GPU fills.

**FA2 change 2: parallelise warps within a block over heads/KV**

FA1 had all warps in a block working on the same query tile. FA2 splits warps so different warps within a block handle different KV columns of the same query tile. The advantage: each warp's `MMA` writes go to different rows of the output accumulator, **no inter-warp sync** is needed for the GEMM portion. The block-level reduction only happens at the end to combine partial `(m, ℓ)` across warps.

**FA2 change 3: keep softmax stats in registers, not shared memory**

FA1 spilled `m_ij`, `ℓ_ij` to shared memory between the rowmax and the rescale. FA2 keeps both in registers and uses warp shuffles (`__shfl_xor_sync`) to do the row-wise reductions. Shared memory is reserved for the matmul tiles.

The practical effect: shared memory pressure drops, you can fit larger `B_r × d` and `B_c × d` tiles, and the warps never stall on `__syncthreads()` waiting for the softmax math.

**FA2 change 4: rescale `O` less often**

FA1 rescaled the running `O_i` accumulator every inner iteration with `rescale_old = exp(m_old - m_new)`. FA2 observes that you can defer the rescale: just keep `O` "in the wrong scale" until the end of the inner loop and rescale once at the epilogue. This drops one multiply across the whole `O` accumulator per inner iteration — meaningful at large `d`.

The math still works because:

```
O_final = exp(m_last - m_global) · O_accumulated_unrescaled
ℓ_final = … (same trick)
O = O_final / ℓ_final
```

You only need one final divide and one final scalar multiply per output row.

**Net effect**

These four changes together get FA2 to ~70% of a pure GEMM at the same `(M, N, K)` shape on A100/H100 — close to the practical ceiling, because the softmax exponentials cost a non-trivial fraction of `O(N·d)` non-matmul FLOPs.

---

**Build it**

You will not reimplement FA2; you will read it and benchmark it.

**Reading task**

Open `csrc/flash_attn/src/flash_fwd_kernel.h`. Locate:

1. The grid launch dimensions (look in `flash_fwd_launch_template.h` for `dim3 grid(...)`). Confirm it is `(num_m_blocks, num_heads, batch)`. **This is FA2 change 1.**
2. The warp partitioning of the `S = QKᵀ` accumulator (`tSrS`). The `Tiled_mma` type tells you how warps are split — search for `TiledMma` and the `using` aliases in `kernel_traits.h`. **This is FA2 change 2.**
3. `softmax_rescale_o_` and `softmax_template` in `softmax.h` — the warp-shuffle reductions, not shared-memory reductions. **This is FA2 change 3.**
4. The deferred final divide-by-`ℓ` at the end of the kernel — search for the epilogue block that writes `tOrO / lse`. **This is FA2 change 4.**

Write a short note linking each FA2 paper change to a concrete code location. Without that note, you cannot debug FA2 patches.

</details>

### Benchmark 任务

使用仓库的 `benchmarks/benchmark_flash_attention.py`。在 fp16 下跑一轮 sweep：

```
for seqlen in 512 1024 2048 4096 8192 16384; do
  for headdim in 64 128; do
    python benchmarks/benchmark_flash_attention.py \
      --mode fwd --batch_size 2 --nheads 16 \
      --seqlen $seqlen --headdim $headdim \
      --dtype fp16 --causal
  done
done | tee fa2_sweep.txt
```

然后写 `plot_fa2_sweep.py`，生成一个带 `(seqlen, headdim, sdpa_ms, fa2_ms, fa2_tflops, sdpa_tflops)` 列的 CSV，以及每个 backend 的 TFLOPs vs seqlen 单图。

应当观察到：

- SDPA 的 `math` backend 呈二次方减慢。
- FA2 随 seqlen 增长保持平稳或略有提升（因为每次 launch 的开销被摊薄）。
- 在 `(seqlen ≥ 4096, headdim = 128)` 处 FA2 达到 GPU 峰值 fp16 TFLOPS 的 40–60%。

---

## 在真实技术栈中使用

在这些条件下 PyTorch 的 `scaled_dot_product_attention` 会选中 FA2：`dtype ∈ {fp16, bf16}`、head dim 为受支持的值（依版本为 `{64, 128, 256}`）、`dropout_p == 0`（推理期间）、除 causal 或 none 之外没有自定义 mask，以及运行在受支持的架构上（sm80+）。上述任一条件不满足时，SDPA 会静默回退到 math kernel。使用：

```python
from torch.nn.attention import sdpa_kernel, SDPBackend
with sdpa_kernel([SDPBackend.FLASH_ATTENTION]):
    ...
```

在条件不满足时强制失败并大声报错。把它与 benchmark 搭配使用，就能亲眼看到与 `math` 的差距。

---

## 测量

对每个 shape，报告：

- kernel 时间（10 次 warmup 运行后、≥ 20 次运行的中位数）。
- 实际达到的 TFLOPs（`4 · B · H · N² · d / time`）。
- 实际达到的 HBM 带宽（`bytes / time`）。
- 相对 GPU 峰值 fp16 TFLOPs 的比值。

至少对一个 shape，dump Nsight Compute 报告并验证：

- tensor-core 指令占主导（`smsp__inst_executed_pipe_tensor_op_hmma.sum` 数值很大）。
- DRAM 流量远低于 `O(N²)` 字节。
- occupancy 约为每个 SM 1–2 个 block — FA2 刻意使用较大的每线程寄存器分块，因此高 occupancy 并非目标。

---

## 交付

放入 `flash-attn-course/`：

1. `fa2_code_notes.md` — FA 仓库中四处 FA2 改动对应的 file:line。
2. `fa2_sweep.csv` 与 `fa2_sweep.png` — 覆盖 `(seqlen, headdim)` 的 benchmark 输出。
3. 一份单个 FA2 shape（`fa2_ncu_4096_d128.ncu-rep` 或类似）下的 Nsight Compute 报告，展示 tensor-core 利用率与 HBM 流量。

这些产物就是第 9 讲中与 FA3 做 diff 的对象。

---

## 相关页面

- [第 5 讲 — 仓库剖析](第05讲-仓库结构与Python-CUDA-API.md)
- [第 7 讲 — 反向传播与验证](第07讲-反向传播与数值验证.md)
- [第 9 讲 — Hopper / FA3 / FA4](第09讲-Hopper上的FA3与FA4.md)
- FA2 论文：<https://tridao.me/publications/flash2/flash2.pdf>


<details>
<summary>English original</summary>

**Benchmark task**

Use the repo's `benchmarks/benchmark_flash_attention.py`. Run a sweep at fp16:

```
for seqlen in 512 1024 2048 4096 8192 16384; do
  for headdim in 64 128; do
    python benchmarks/benchmark_flash_attention.py \
      --mode fwd --batch_size 2 --nheads 16 \
      --seqlen $seqlen --headdim $headdim \
      --dtype fp16 --causal
  done
done | tee fa2_sweep.txt
```

Then write `plot_fa2_sweep.py` to produce a CSV with `(seqlen, headdim, sdpa_ms, fa2_ms, fa2_tflops, sdpa_tflops)` columns and a single-figure plot of TFLOPs vs seqlen for each backend.

You should see:

- SDPA's `math` backend slowing down quadratically.
- FA2 staying flat or improving slightly as seqlen grows (because per-launch overhead dilutes).
- FA2 reaching 40–60% of your GPU's peak fp16 TFLOPS at `(seqlen ≥ 4096, headdim = 128)`.

---

**Use it in the real stack**

PyTorch's `scaled_dot_product_attention` will pick FA2 under these conditions: `dtype ∈ {fp16, bf16}`, head dim a supported value (`{64, 128, 256}` depending on version), `dropout_p == 0` (during inference), no custom mask other than causal or none, and on a supported architecture (sm80+). When any of those fail, SDPA silently falls back to the math kernel. Use:

```python
from torch.nn.attention import sdpa_kernel, SDPBackend
with sdpa_kernel([SDPBackend.FLASH_ATTENTION]):
    ...
```

to force-fail loudly if the conditions are not met. Pair this with a benchmark and you can see the gap to `math` for yourself.

---

**Measure it**

Per shape, report:

- Kernel time (median over ≥ 20 runs after 10 warmup runs).
- Achieved TFLOPs (`4 · B · H · N² · d / time`).
- Achieved HBM bandwidth (`bytes / time`).
- Ratio to GPU peak fp16 TFLOPs.

For at least one shape, dump the Nsight Compute report and verify:

- Tensor-core instructions dominate (`smsp__inst_executed_pipe_tensor_op_hmma.sum` is large).
- DRAM traffic is far below `O(N²)` bytes.
- Occupancy is ~1–2 blocks per SM — FA2 deliberately uses big per-thread register tiles, so high occupancy is not a target.

---

**Ship it**

Drop into `flash-attn-course/`:

1. `fa2_code_notes.md` — the four FA2 changes mapped to file:line in the FA repo.
2. `fa2_sweep.csv` and `fa2_sweep.png` — the benchmark output across `(seqlen, headdim)`.
3. One Nsight Compute report at a single FA2 shape (`fa2_ncu_4096_d128.ncu-rep` or similar) showing tensor-core utilisation and HBM traffic.

These artifacts are what you will diff against FA3 in Lecture 9.

---

**Related pages**

- [Lecture 5 — Repo anatomy](第05讲-仓库结构与Python-CUDA-API.md)
- [Lecture 7 — Backward pass and validation](第07讲-反向传播与数值验证.md)
- [Lecture 9 — Hopper / FA3 / FA4](第09讲-Hopper上的FA3与FA4.md)
- FA2 paper: <https://tridao.me/publications/flash2/flash2.pdf>

</details>

---

> 原文：[`Phase 4 - Track C - DL Inference Optimization/02 - Kernel Engineering/FlashAttention Course/Lecture 06 - FlashAttention-2.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20C%20-%20DL%20Inference%20Optimization/02%20-%20Kernel%20Engineering/FlashAttention%20Course/Lecture%2006%20-%20FlashAttention-2.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
