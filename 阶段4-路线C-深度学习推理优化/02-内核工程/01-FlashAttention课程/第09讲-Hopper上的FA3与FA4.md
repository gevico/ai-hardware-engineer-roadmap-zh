---
title: Lecture 9 — Hopper / FlashAttention-3 / FlashAttention-4
description: Lecture 9 — Hopper / FlashAttention-3 / FlashAttention-4
---

# Lecture 9 — Hopper / FlashAttention-3 / FlashAttention-4

**Parent:** [FlashAttention Course](Guide.md)

**One-line purpose:** 理解 FA3 / FA4 所利用的 Hopper 专属（以及面向 Blackwell）特性 —— WGMMA、TMA、warp 特化、ping-pong 调度、FP8 —— 以及在源码中何处能找到它们。

**Prerequisites:** Lecture 1–8。熟悉 FA2 源码。

**Artifact:** H100 / H200 上的 FA3 vs FA2 benchmark，外加一份带注释的源码地图，标出 `csrc/flash_attn_hopper/` 中的 WGMMA、TMA 和 warp 特化流水线阶段。

---

## Why it matters

FA3 是第一个在关键形状（long-context training 和 prefill）上达到 H100/H200 理论峰值 5–10% 以内的 attention kernel。原因不在算法 —— 而是把 FA2 算法调度在 Hopper 的新指令和新内存原语之上。FA4（目前只有一篇博客和原型）把同样的思路带到 Blackwell 上，用 FP4/FP6。理解了 FA3 的改动，FA4 就是「同样的打法，更好的硬件」。

要读懂 TransformerEngine 的 attention 路径以及 cuDNN 中的 Hopper / Blackwell 变体，你也需要这份理解。

---

## Mental model

### Hopper hardware that FA3 actually uses

| Feature | What it does | Why FA3 cares |
|---------|--------------|---------------|
| **WGMMA** (`wgmma.mma_async`) | Warpgroup（4 个 warp = 128 线程）发出异步 MMA，通过 descriptor 读取共享内存操作数。 | 每条指令的 matmul 更大；异步意味着计算与下一次 load 重叠。 |
| **TMA** (Tensor Memory Accelerator) | 一条指令把多维 tile 从 HBM → 共享内存，内建 swizzle。 | 取代复杂的 `cp.async` 样板代码，释放寄存器，且等待是 barrier 而非逐线程。 |
| **Distributed shared memory** | 同一 cluster 中一个 CTA 的线程可以读另一个 CTA 的 shared memory。 | 支撑 warp 特化流水线在交换 tile 时无需绕道 HBM。 |
| **Async PTX barriers** (`mbarrier`) | 面向 producer / consumer warp 的轻量 barrier。 | 协调 WGMMA producer 与 softmax consumer，无需 `__syncthreads()`。 |
| **FP8 tensor cores** | E4M3 / E5M2 MMA，速率为 fp16 的 2×。 | 让 FA3 在 prefill GEMM 上每次 MMA 消耗更少周期，为 softmax 留出余量。 |

### Warp specialisation in FA3

FA2 中同构的 warp 全都执行相同的 MMA + softmax 序列，而 FA3 把 block 内的 warp 拆成不同角色：

- **Producer warps**：发出下一个 `K, V` tile 的 TMA load。等待 `mbarrier`。交给 consumer。
- **Consumer warps (math)**：执行 `WGMMA(Q, Kᵀ)`、softmax（rescale、exp、rowmax/rowsum）以及 `WGMMA(P, V)`。等待 producer barrier 提供下一个 tile。

这正是 CUTLASS 用于 GEMM 的 producer/consumer 流水线模式。它把内存延迟隐藏在 *block 内部*，而不只是跨 block 隐藏。

### Ping-pong scheduling

两个 consumer warpgroup 交替：warpgroup A 为 tile `j` 做 softmax + epilogue 时，warpgroup B 为 tile `j+1` 做 WGMMA。由于 softmax / WGMMA 使用不同的流水线（CUDA 核心 vs 张量核心），它们可以在同一个 SM 上并发运行。结果：张量核心约 95% 的时间保持忙碌，而 FA2 只有约 60%。

### Overlapping softmax with GEMM

softmax 的非 matmul FLOPs（指数、rescale）过去会让 warp 停顿。在 Hopper 上，借助 ping-pong + warp 特化，这些 FLOPs 在 CUDA 核心上运行，与张量核心上运行的 WGMMA 并行。对足够长的 KV tile 而言，softmax 的开销实际上消失了。

### FP8

FA3 对 prefill `Q · Kᵀ` 和 `P · V` matmul 支持 FP8。累加使用 fp32，以保持 softmax 的数值。代价是 calibration（per-tensor scaling factor `s_q`、`s_k`、`s_v`）；收益是在 H100 上相同 shape 相比 bf16 约 1.8× 加速。

### CuTeDSL

FA3 是用 CUTLASS 的 CuTe DSL 写的，不是纯 CUDA C++。CuTe 提供可组合的 layout algebra（`Layout`、`Stride`、`Shape`）、TMA descriptor 构造器，以及可编译成正确 WGMMA 指令的 MMA atom 选择器。读 FA3 源码 = 读 CuTe 代码。这一点无法回避。

FA3 中最主要的 CuTe 原语：

- `cute::Tensor` —— 一个「逻辑张量」，其 layout 可能位于寄存器 / SMEM / GMEM。
- `cute::copy(...)` —— 针对该 layout 触发正确的 load/store 指令（TMA、`ldmatrix`、`cp.async`）。
- `cute::gemm(...)` —— 触发正确的 WGMMA/MMA atom。
- `cute::TiledMma` —— 描述 warpgroup 内的 warp 如何划分 MMA 结果。


<details>
<summary>English original</summary>

**Lecture 9 — Hopper / FlashAttention-3 / FlashAttention-4**

**Parent:** [FlashAttention Course](Guide.md)

**One-line purpose:** Understand the Hopper-specific (and Blackwell-targeted) features that FA3 / FA4 exploit — WGMMA, TMA, warp specialisation, ping-pong scheduling, FP8 — and where to find each one in the source.

**Prerequisites:** Lectures 1–8. Strong familiarity with FA2 source.

**Artifact:** FA3 vs FA2 benchmark on H100 / H200, plus an annotated source map identifying the WGMMA, TMA, and warp-specialised pipeline stages in `csrc/flash_attn_hopper/`.

---

**Why it matters**

FA3 is the first attention kernel that gets within 5–10% of theoretical peak on H100/H200 for the shapes that matter (long-context training and prefill). The reason is not algorithmic — it is the FA2 algorithm scheduled on top of Hopper's new instructions and new memory primitives. FA4 (currently a blog post and prototype) pushes the same ideas onto Blackwell with FP4/FP6. If you understand the FA3 changes, FA4 is "the same play on better hardware."

You will also need this understanding to read TransformerEngine's attention path and the Hopper / Blackwell variants in cuDNN.

---

**Mental model**

**Hopper hardware that FA3 actually uses**

| Feature | What it does | Why FA3 cares |
|---------|--------------|---------------|
| **WGMMA** (`wgmma.mma_async`) | Warpgroup (4 warps = 128 threads) issues an async MMA reading shared-memory operands via descriptors. | Bigger matmul per instruction; async means math overlaps with the next load. |
| **TMA** (Tensor Memory Accelerator) | One instruction copies a multi-dim tile from HBM → shared memory with built-in swizzle. | Replaces complex `cp.async` boilerplate, frees registers, and the wait is a barrier rather than per-thread. |
| **Distributed shared memory** | Threads in one CTA can read another CTA's shared memory in the same cluster. | Enables warp-specialised pipelines that exchange tiles without round-tripping through HBM. |
| **Async PTX barriers** (`mbarrier`) | Lightweight barriers for producer / consumer warps. | Coordinates WGMMA producers and softmax consumers without `__syncthreads()`. |
| **FP8 tensor cores** | E4M3 / E5M2 MMA at 2× the rate of fp16. | Lets FA3 burn fewer cycles per MMA on the prefill GEMM, leaving headroom for softmax. |

**Warp specialisation in FA3**

Where FA2 had homogeneous warps all doing the same MMA + softmax sequence, FA3 splits warps in a block into roles:

- **Producer warps**: issue TMA loads of the next `K, V` tile. Wait on `mbarrier`. Hand off to consumer.
- **Consumer warps (math)**: do `WGMMA(Q, Kᵀ)`, the softmax (rescale, exp, rowmax/rowsum), and `WGMMA(P, V)`. Wait on producer barrier for the next tile.

This is exactly the producer/consumer pipeline pattern that CUTLASS uses for GEMM. It hides memory latency *inside the block*, not just across blocks.

**Ping-pong scheduling**

Two consumer warpgroups alternate: while warpgroup A does its softmax + epilogue for tile `j`, warpgroup B does the WGMMA for tile `j+1`. Because softmax / WGMMA use different pipelines (CUDA cores vs tensor cores), they can run concurrently on the same SM. The result: tensor cores stay busy ~95% of the time instead of ~60% for FA2.

**Overlapping softmax with GEMM**

The non-matmul FLOPs of softmax (the exponentials, the rescales) used to stall the warp. On Hopper, with ping-pong + warp specialisation, those FLOPs run on the CUDA cores in parallel with the WGMMAs running on tensor cores. The cost of softmax effectively disappears for long-enough KV tiles.

**FP8**

FA3 supports FP8 for the prefill `Q · Kᵀ` and `P · V` matmuls. Accumulation is in fp32 to preserve numerics for the softmax. The cost is calibration (per-tensor scaling factors `s_q`, `s_k`, `s_v`); the benefit is ~1.8× speedup over bf16 for the same shape on H100.

**CuTeDSL**

FA3 is written using CUTLASS's CuTe DSL, not plain CUDA C++. CuTe gives you composable layout algebra (`Layout`, `Stride`, `Shape`), TMA descriptor builders, and MMA atom selectors that compile down to the right WGMMA instructions. Reading FA3 source = reading CuTe code. There is no avoiding it.

The headline CuTe primitives in FA3:

- `cute::Tensor` — a "logical tensor" with a layout that may live in registers / SMEM / GMEM.
- `cute::copy(...)` — fires the right load/store instruction (TMA, `ldmatrix`, `cp.async`) for the layout.
- `cute::gemm(...)` — fires the right WGMMA/MMA atom.
- `cute::TiledMma` — describes how warps within a warpgroup partition the MMA result.

</details>

### FA4 草图（Blackwell）

FA4 目前还只是一篇博客；公开仓库仍是 FA3 代码库，新的 dispatch 路径尚在开发中。主要押注：

- 支持 FP4 / FP6 的 **TCGen5** 张量核心 → 如果数值上能接受，相比 FP8 还能再快 1.5–2×。
- **更大的簇共享内存** → 每个 CTA 簇能容纳更大的 block，复用更多。
- **更激进的 warp 特化**，配三级流水线（producer、math、epilogue）。

在 FA4 公开稳定之前，先把它当作预览。心智模型可以沿用。

---

## 动手构建

### 阅读任务

打开 `csrc/flash_attn_hopper/`。定位：

1. **TMA 描述符构建** —— 搜索 `Sm90_TMA_LOAD` 或 `make_tma_copy(...)`。Q、K、V 的加载就在这里接好。
2. **WGMMA 实例化** —— 搜索 `wgmma` 或 `SM90_64x*x16_F32BF16BF16_SS`（SM90 上 bf16 → fp32 的 MMA atom）。consumer warp 内部的 `cute::gemm` 调用负责发射这些指令。
3. **warp 特化** —— 搜索 `warpgroup_idx` 或 `cooperative_warp_specialize_blockwise`。`if (warpgroup_idx == ProducerWarpGroup) { ... } else { ... }` 代码块就是 producer/consumer 的拆分点。
4. **屏障** —— `producer_acquire` / `consumer_wait` 中 `mbarrier` 的用法。这就是 FA3 的 ping-pong 机制。
5. **FP8 路径** —— 搜索 `e4m3` 或 `fp8`。FP8 有独立的 fwd kernel，带自己的 dispatch 表。

把源码地图写成 `fa3_code_notes.md`。与 Lecture 6 中你的 FA2 地图逐行对比 —— 这个 diff 就是 FA3 的贡献所在。

### Benchmark 任务

```python
# fa3_vs_fa2.py
import torch, time
from flash_attn import flash_attn_func                  # FA2
from flash_attn.flash_attn_interface import flash_attn_func_v3  # FA3 (Hopper only)

shapes = [(2, 16, N, 128) for N in [1024, 4096, 8192, 16384, 32768]]
def time_fn(fn, *args, **kw):
    for _ in range(5): fn(*args, **kw)
    torch.cuda.synchronize()
    s = torch.cuda.Event(enable_timing=True); e = torch.cuda.Event(enable_timing=True)
    s.record()
    for _ in range(20): fn(*args, **kw)
    e.record(); e.synchronize()
    return s.elapsed_time(e) / 20

for (B, H, N, D) in shapes:
    q = torch.randn(B, N, H, D, device="cuda", dtype=torch.bfloat16)
    k = torch.randn_like(q); v = torch.randn_like(q)
    t2 = time_fn(flash_attn_func, q, k, v, causal=True)
    t3 = time_fn(flash_attn_func_v3, q, k, v, causal=True)
    print(f"N={N:>5}  FA2={t2:7.3f} ms  FA3={t3:7.3f} ms  speedup={t2/t3:.2f}x")
```

在 H100/H200 上，长序列长度下应能看到约 1.5–2.0× 的加速。加速比随 `N` 增大，因为内层 KV 循环越长，FA3 的 ping-pong 调度越关键。如果能用 FP8（sm89+），再用 FP8 路径重复一遍。

---

## 在真实栈中使用

- **TransformerEngine**（NVIDIA）：在 Hopper 训练中封装 FA3。阅读 `transformer_engine/pytorch/attention.py` 查看 dispatch。
- **带 `--attention-backend FLASH_ATTN_VLLM_V1` 的 vLLM** 在 Hopper 上会自动选 FA3。
- **cuDNN 的 SDPA** 有一条为 Hopper 调优的 attention 路径，独立于 FA3 使用了其中许多原语。

推理场景下，Hopper 上应选 FA3 的 prefill kernel；FA 的 decode（`with_kvcache`）仍在使用 FA2 代码库，但截至 2026 年年中正在逐步移植到 FA3。

---

## 测量

对每个（FA2、FA3、FP8）行：

- kernel 时间（ms）。
- 实测 TFLOPS（`4 · B · H · N² · D / time`）。
- 达到 GPU 峰值的百分比（H100/H200 上 bf16 约 989 TFLOPs，FP8 约 1978 TFLOPs）。
- 实测 HBM 带宽（应远低于峰值 —— FA3 在大 N 下是算力受限）。

对 FA3 跑 Nsight Compute，关注：

- WGMMA 指令数（`smsp__inst_executed_pipe_tensor_op_wgmma.sum`）—— 应占主导。
- TMA 加载次数（`tma_load` 指标）—— 只有在 Hopper 上才非零。
- Tensor 流水线利用率 → 长 N 下 90%+。

---

## 交付

放入 `flash-attn-course/`：

1. `fa3_code_notes.md` —— TMA / WGMMA / warp 特化 / FP8 路径的源码地图。
2. `fa3_vs_fa2.csv` 和 `fa3_vs_fa2.png`，展示加速比曲线。
3. 一份最佳 FA3 shape（`fa3_ncu_32k_d128.ncu-rep`）下的 Nsight Compute 报告，突出 WGMMA + TMA 指标。

这些也是 capstone 的输入。

---

## 相关页面

- [Lecture 6 — FlashAttention-2](第06讲-FlashAttention-2.md)
- [Lecture 8 — Inference path](第08讲-推理路径的KV-cache与decode.md)
- [Lecture 10 — Capstone](第10讲-综合实践.md)
- FA3 博客：<https://tridao.me/blog/2024/flash3/>
- CUTLASS / CuTe DSL：<https://github.com/NVIDIA/cutlass>
- PTX ISA：<https://docs.nvidia.com/cuda/parallel-thread-execution/>


<details>
<summary>English original</summary>

**FA4 sketch (Blackwell)**

FA4 is currently a blog post; the public repo is the FA3 codebase with new dispatch paths in progress. The main bets:

- **TCGen5** tensor cores with FP4 / FP6 support → another 1.5–2× over FP8 if you can stomach the numerics.
- **Larger cluster shared memory** → larger blocks per CTA cluster, more reuse.
- **More aggressive warp specialisation** with three-stage pipelines (producer, math, epilogue).

Until FA4 is public-stable, treat this as a preview. The mental model carries over.

---

**Build it**

**Reading task**

Open `csrc/flash_attn_hopper/`. Locate:

1. **TMA descriptor construction** — search for `Sm90_TMA_LOAD` or `make_tma_copy(...)`. This is where Q, K, V loads are wired up.
2. **WGMMA instantiation** — search for `wgmma` or `SM90_64x*x16_F32BF16BF16_SS` (the SM90 MMA atom for bf16 → fp32). The `cute::gemm` call inside the consumer warps fires these.
3. **Warp specialisation** — search for `warpgroup_idx` or `cooperative_warp_specialize_blockwise`. The `if (warpgroup_idx == ProducerWarpGroup) { ... } else { ... }` block is the producer/consumer split.
4. **Barriers** — `mbarrier` usage in `producer_acquire` / `consumer_wait`. This is the FA3 ping-pong machinery.
5. **FP8 paths** — search `e4m3` or `fp8`. There is a separate fwd kernel for FP8 with its own dispatch table.

Write your source map as `fa3_code_notes.md`. Compare it line-for-line to your FA2 map from Lecture 6 — that diff is the FA3 contribution.

**Benchmark task**

```python
# fa3_vs_fa2.py
import torch, time
from flash_attn import flash_attn_func                  # FA2
from flash_attn.flash_attn_interface import flash_attn_func_v3  # FA3 (Hopper only)

shapes = [(2, 16, N, 128) for N in [1024, 4096, 8192, 16384, 32768]]
def time_fn(fn, *args, **kw):
    for _ in range(5): fn(*args, **kw)
    torch.cuda.synchronize()
    s = torch.cuda.Event(enable_timing=True); e = torch.cuda.Event(enable_timing=True)
    s.record()
    for _ in range(20): fn(*args, **kw)
    e.record(); e.synchronize()
    return s.elapsed_time(e) / 20

for (B, H, N, D) in shapes:
    q = torch.randn(B, N, H, D, device="cuda", dtype=torch.bfloat16)
    k = torch.randn_like(q); v = torch.randn_like(q)
    t2 = time_fn(flash_attn_func, q, k, v, causal=True)
    t3 = time_fn(flash_attn_func_v3, q, k, v, causal=True)
    print(f"N={N:>5}  FA2={t2:7.3f} ms  FA3={t3:7.3f} ms  speedup={t2/t3:.2f}x")
```

On H100/H200 you should see ~1.5–2.0× speedup at long sequence lengths. The speedup grows with `N` because FA3's ping-pong scheduling matters more when the inner KV loop is long. If you have access to FP8 (sm89+), repeat with the FP8 path.

---

**Use it in the real stack**

- **TransformerEngine** (NVIDIA): wraps FA3 for Hopper training. Read `transformer_engine/pytorch/attention.py` to see the dispatch.
- **vLLM with `--attention-backend FLASH_ATTN_VLLM_V1`** picks FA3 on Hopper automatically.
- **cuDNN's SDPA** has a Hopper-tuned attention path that uses many of the same primitives independently of FA3.

For inference, the FA3 prefill kernel is the right choice on Hopper; the FA decode (`with_kvcache`) is still using the FA2 codebase but is being ported to FA3 incrementally as of mid-2026.

---

**Measure it**

For each (FA2, FA3, FP8) row:

- Kernel time (ms).
- Achieved TFLOPS (`4 · B · H · N² · D / time`).
- Achieved % of GPU peak (bf16 ~989 TFLOPs on H100/H200, FP8 ~1978 TFLOPs).
- Achieved HBM bandwidth (should be far below peak — FA3 is compute-bound at large N).

Run Nsight Compute on FA3 and look for:

- WGMMA instruction count (`smsp__inst_executed_pipe_tensor_op_wgmma.sum`) — should be dominant.
- TMA load count (`tma_load` metrics) — non-zero only on Hopper.
- Tensor pipe utilization → 90%+ at long N.

---

**Ship it**

Drop into `flash-attn-course/`:

1. `fa3_code_notes.md` — source map for TMA / WGMMA / warp-specialisation / FP8 paths.
2. `fa3_vs_fa2.csv` and `fa3_vs_fa2.png` showing the speedup curve.
3. One Nsight Compute report at the best FA3 shape (`fa3_ncu_32k_d128.ncu-rep`) with WGMMA + TMA metrics highlighted.

These are also the inputs to the capstone.

---

**Related pages**

- [Lecture 6 — FlashAttention-2](第06讲-FlashAttention-2.md)
- [Lecture 8 — Inference path](第08讲-推理路径的KV-cache与decode.md)
- [Lecture 10 — Capstone](第10讲-综合实践.md)
- FA3 blog: <https://tridao.me/blog/2024/flash3/>
- CUTLASS / CuTe DSL: <https://github.com/NVIDIA/cutlass>
- PTX ISA: <https://docs.nvidia.com/cuda/parallel-thread-execution/>

</details>

---

> 原文：[`Phase 4 - Track C - DL Inference Optimization/02 - Kernel Engineering/FlashAttention Course/Lecture 09 - Hopper FA3 FA4.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20C%20-%20DL%20Inference%20Optimization/02%20-%20Kernel%20Engineering/FlashAttention%20Course/Lecture%2009%20-%20Hopper%20FA3%20FA4.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
