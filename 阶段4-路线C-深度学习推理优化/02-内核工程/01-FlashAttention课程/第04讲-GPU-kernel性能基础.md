---
title: 第 4 讲 — GPU Kernel 性能基础
description: 第 4 讲 — GPU Kernel 性能基础
---

# 第 4 讲 — GPU Kernel 性能基础

**父级：** [FlashAttention Course](Guide.md)

**一句话目的：** 覆盖读懂 FlashAttention 源码而不至于迷失所必需的、最低限度的 GPU 性能硬件知识（存储层次、warp、张量核心、occupancy、合并访存）。

**前置要求：** 第 1–3 讲。阶段 4 方向 B 的基础 CUDA。

**产物：** 合并访存与未合并访存 copy kernel 的 Nsight Compute 报告，以及一个 warp shuffle 归约 microbenchmark。

---

## 为什么重要

FlashAttention 代码库里密布 CUDA 原语 —— `__shfl_xor_sync`、共享内存 bank 模式、寄存器分块尺寸、`cp.async`、WGMMA 描述符。如果这些词对你而言只是噪声，你就读不懂这个 kernel；那等同于在读一门外语。本讲就是那本词典。

你不需要从零编写 GEMM。你确实需要知道每一层存储的代价、warp 与张量核心如何执行，以及常见的失效模式有哪些。

---

## 心智模型

### 现代 NVIDIA GPU 上的存储层次

| 层级 | 容量（Hopper SXM） | 延迟 | 带宽 | 归属 |
|------|--------------------|---------|-----------|-------------|
| HBM（全局） | 80 GB | ~400-500 ns | ~3.35 TB/s | 全设备 |
| L2 | 50 MB | ~150-200 ns | ~12 TB/s | 全设备，分区 |
| 共享内存 / L1 | 228 KB / SM | ~30 ns | ~30 TB/s per SM | 每线程块 |
| 寄存器 | 64 K × 32-bit / SM | 0（编译器管理） | 无上限 | 每线程 |

高性能 kernel 的全部要义在于对数据做分级暂存，使内层循环只访问寄存器和共享内存，而 HBM 对每个元素只访问一次。FlashAttention 正是围绕这一原则构建的。

### warp 与 SM

- **warp** 是 32 个以锁步方式执行同一条指令的线程（SIMT）。所有性能推理都归结为“这个周期里 warp 在做什么”。
- **SM**（Streaming Multiprocessor）并发运行多个 warp。Hopper 的 SM 有 128 KB 寄存器文件和 228 KB 共享内存/L1。
- **Occupancy** = 一个 SM 上驻留多少个 warp。更高的 occupancy 能掩盖内存延迟，但会消耗每线程寄存器。FlashAttention 有意把 occupancy 保持在适中水平（例如每块 2–4 个 warp，每 SM 的块数 = 1–2），因为每线程的寄存器分块很大。

### 合并访存

当一个 warp 的 32 个线程发出的 load 落在 HBM 中连续的 128 字节区域时，硬件会把它们合并成一次事务。如果地址分散，每个线程各自触发一次事务，带宽就会崩塌。经验法则：按 `[batch, head, seq, dim]` 顺序（类 NHWC）布局张量，使变化最快的轴与 warp 的线程索引对应。

### 共享内存 bank 冲突

共享内存被组织为 32 个 4 字节字的 bank。如果同一 warp 中的两个线程读取同一 bank 内的不同地址，该访问就会被串行化。FlashAttention 通过对共享内存分块做仔细的 padding（在内层维度上加 `+8` 或 `+1`），以及在 Hopper 上使用 `ldmatrix` / `stmatrix` 指令来避免这一点。

### 张量核心

张量核心在一条指令中为每个 warp / warpgroup 执行小规模固定形状的矩阵乘：

- **HMMA**（Volta/Turing/Ampere 架构）：一个 warp 发出 `m16n8k16` fp16/bf16 MMA → 约 8 个周期内完成 256 次 FMA。
- **WGMMA**（Hopper）：一个 **warpgroup**（4 个 warp = 128 个线程）针对共享内存描述符发出异步的 `m64n{N}k16` MMA。
- **TCGen5**（Blackwell）：张量核心现在以 warpgroup 簇为粒度工作，并支持 FP4/FP6。

FlashAttention 是一个被 softmax 胶水层包裹的 Tensor Core 引擎。大部分 runtime 都耗在 MMA 指令上。其余则是围绕这些指令的 rescale + 指数运算。

### warp shuffle

`__shfl_sync`、`__shfl_xor_sync` 等允许一个 warp 内的线程在 1 个周期内读取彼此的寄存器 —— 无需共享内存。用于 warp 级归约（softmax 递推中的 `m`、`ℓ`）以及为 MMA 输入做 swizzle。

### `cp.async` 与 TMA

- `cp.async`（Ampere 架构及以后）发起一次从 HBM 到共享内存的 DMA，warp 之后可以等待它完成，从而与计算重叠。
- **TMA**（Hopper）是同一思路在更粗粒度上的体现：一条指令把多维分块从 HBM 复制到共享内存，整个 warp / warpgroup 免费拿到它，随后在 barrier 上 commit/wait。

FA2 依赖 `cp.async`。FA3 依赖 TMA。FA3 更快的原因很大程度上在于把这些复制与 MMA 重叠起来，使计算单元永不空转。

---

## 动手实现

两个 microbenchmark。二者都以独立 CUDA 文件的形式提供，放在你的课程工作目录中。


<details>
<summary>English original</summary>

**Lecture 4 — GPU Kernel Performance Basics**

**Parent:** [FlashAttention Course](Guide.md)

**One-line purpose:** Cover the strict minimum of GPU performance hardware (memory hierarchy, warps, tensor cores, occupancy, coalescing) needed to read FlashAttention source code without getting lost.

**Prerequisites:** Lectures 1–3. Basic CUDA from Phase 4 Track B.

**Artifact:** Nsight Compute reports for a coalesced vs uncoalesced copy kernel and a warp-shuffle reduction microbenchmark.

---

**Why it matters**

The FlashAttention codebase is dense with CUDA primitives — `__shfl_xor_sync`, shared-memory bank patterns, register tile sizes, `cp.async`, WGMMA descriptors. If those words are noise to you, you cannot read the kernel; you are reading a foreign language. This lecture is the dictionary.

You do not need to write GEMM from scratch. You do need to know what each layer of memory costs, how warps and tensor cores execute, and what the common failure modes are.

---

**Mental model**

**Memory hierarchy on a modern NVIDIA GPU**

| Level | Size (Hopper SXM) | Latency | Bandwidth | Who owns it |
|------|--------------------|---------|-----------|-------------|
| HBM (global) | 80 GB | ~400-500 ns | ~3.35 TB/s | device-wide |
| L2 | 50 MB | ~150-200 ns | ~12 TB/s | device-wide, partitioned |
| Shared / L1 | 228 KB / SM | ~30 ns | ~30 TB/s per SM | per thread block |
| Registers | 64 K × 32-bit / SM | 0 (compiler-managed) | unbounded | per thread |

The whole point of a high-performance kernel is to stage data so that the inner loop only touches registers and shared memory, and HBM is touched once per element. FlashAttention is built exactly around this principle.

**Warps and the SM**

- A **warp** is 32 threads that execute the same instruction in lockstep (SIMT). All performance reasoning is "what does the warp do this cycle."
- An **SM** (Streaming Multiprocessor) runs many warps concurrently. Hopper SM has 128 KB of register file and 228 KB shared/L1.
- **Occupancy** = how many warps are resident on an SM. Higher occupancy hides memory latency but costs registers per thread. FlashAttention deliberately keeps occupancy modest (e.g. 2–4 warps per block, blocks per SM = 1–2) because per-thread register tiles are large.

**Coalescing**

When the 32 threads of a warp issue loads that hit a contiguous 128-byte region of HBM, the hardware combines them into one transaction. If the addresses scatter, each thread triggers its own transaction and bandwidth collapses. Rule of thumb: lay out tensors in `[batch, head, seq, dim]` order (NHWC-like) so the fastest-changing axis matches the warp's thread index.

**Shared-memory bank conflicts**

Shared memory is organised into 32 banks of 4-byte words. If two threads in the same warp read different addresses in the same bank, the access serialises. FlashAttention avoids this by carefully padding shared-memory tiles (`+8` or `+1` on the inner dimension) and by using the `ldmatrix` / `stmatrix` instructions on Hopper.

**Tensor cores**

Tensor cores execute small fixed-shape matmuls per warp / warpgroup in one instruction:

- **HMMA** (Volta/Turing/Ampere): one warp issues `m16n8k16` fp16/bf16 MMA → 256 FMA in ~8 cycles.
- **WGMMA** (Hopper): one **warpgroup** (4 warps = 128 threads) issues an asynchronous `m64n{N}k16` MMA against shared memory descriptors.
- **TCGen5** (Blackwell): tensor cores now operate at warpgroup-cluster scale with FP4/FP6 support.

FlashAttention is a tensor-core engine wrapped in softmax glue. Most of the runtime is in MMA instructions. The rest is the rescale + exponentials around them.

**Warp shuffles**

`__shfl_sync`, `__shfl_xor_sync`, etc. let threads in a warp read each other's registers in 1 cycle — no shared memory needed. Used for warp-level reductions (`m`, `ℓ` in the softmax recurrence) and for swizzling MMA inputs.

**`cp.async` and TMA**

- `cp.async` (Ampere+) issues a DMA from HBM to shared memory that the warp can wait on later, overlapping with compute.
- **TMA** (Hopper) is the same idea at coarser granularity: a single instruction copies a multi-dimensional tile from HBM to shared memory, the whole warp / warpgroup gets it for free, and you commit/wait on a barrier.

FA2 leans on `cp.async`. FA3 leans on TMA. The reason FA3 is faster is largely about overlapping these copies with the MMA so the math units never stall.

---

**Build it**

Two microbenchmarks. Both ship as standalone CUDA files in your course working directory.

</details>

### 1. 合并拷贝 vs 非合并拷贝

```cpp
// coalesce.cu
__global__ void copy_coalesced(const float* __restrict__ x, float* __restrict__ y, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) y[idx] = x[idx];
}

__global__ void copy_strided(const float* __restrict__ x, float* __restrict__ y, int n, int stride) {
    int idx = (blockIdx.x * blockDim.x + threadIdx.x) * stride;
    if (idx < n) y[idx] = x[idx];
}
```

两者都在 `n = 2^24` 下运行。合并的那份应能跑到 HBM 峰值的约 85–90%。带 stride 的那份（例如 `stride = 32`）应下降 10–20×。在 `ncu --metrics gpu__time_duration.sum,dram__bytes.sum` 下比较。

### 2. Warp shuffle 归约

```cpp
// warp_reduce.cu
__inline__ __device__ float warp_sum(float v) {
    for (int off = 16; off > 0; off >>= 1)
        v += __shfl_down_sync(0xffffffff, v, off);
    return v;
}

__global__ void rowsum_via_shuffle(const float* x, float* out, int n) {
    int row = blockIdx.x;
    int tid = threadIdx.x;
    float acc = 0.f;
    for (int j = tid; j < n; j += blockDim.x) acc += x[row * n + j];
    acc = warp_sum(acc);
    if ((tid & 31) == 0) atomicAdd(&out[row], acc);
}
```

这段代码在结构上等同于 FlashAttention 用来在 warp 的各线程间归约 `m` 与 `ℓ`、然后再广播回去的代码。把它与朴素的 shared memory 归约对比计时；在 Hopper 上，对 warp 本地累加器而言 shuffle 版本应快约 2×。

---

## 在真实技术栈中使用

在 `flash-attention/csrc/flash_attn/src/` 中：

- `softmax.h`：搜索 `quad_shfl_xor_sync` / `Allreduce` —— 这些就是在 warp 内计算 `m` 与 `ℓ` 归约的 warp shuffle。
- `block_info.h` 与 `kernel_traits.h`：`kBlockM`、`kBlockN`、`kNWarps`、`kStages` —— 分块大小、每 block 的 warp 数，以及 `cp.async` 的 stage 数。
- 再次是 `mask.h` 与 `softmax.h`：每次 `tOrP` 之后对 `tOrO` 做重缩放并相加的更新 —— 这就是第 2 讲中那个递推关系的 FP 寄存器级实现。

挑一个 CUDA 文件，在你自己的笔记里标注出上述每一项出现的位置。在你能于真实源码中指着 MMA 调用、shuffle 归约和 `cp.async` 的 issue / commit / wait 之前，不要进入第 5 讲。

---

## 测量它

值得记住的标准 Nsight Compute 指标：

| 指标 | 含义 |
|--------|---------|
| `sm__throughput.avg.pct_of_peak_sustained_elapsed` | SM 整体利用率 |
| `dram__bytes.sum` | HBM 流量 |
| `lts__t_sectors.sum` | L2 流量 |
| `sm__warps_active.avg.pct_of_peak_sustained_active` | 实际达成的 occupancy |
| `smsp__inst_executed_pipe_tensor_op_hmma.sum` | Tensor core HMMA 指令数 |
| `smsp__sass_average_data_bytes_per_sector_mem_global.pct` | 合并访存效率 |

对这两个微基准，至少报告：kernel 时间、实际达成的 HBM 带宽、实际达成的 occupancy。对 warp shuffle 归约，还要报告 tensor core 指令数（应为零 —— 它是纯 CUDA core 的 kernel）。

---

## 交付

放入 `flash-attn-course/`：

1. `coalesce.cu` 和一个 `coalesce_bench.txt`，内含两个 kernel 的时间与带宽。
2. `warp_reduce.cu` 和一个 `reduce_bench.txt`，比较 shuffle 与 shared memory 归约。
3. 一页 Markdown 速查表，把 FlashAttention kernel 中的每个术语（`tOrO`、`tOrP`、`tOrS`、`cp.async.commit_group`、`__shfl_xor_sync`、`wgmma.mma_async`）映射为用你自己的话写成的一句话解释。

有了这三样，就可以进入第 5 讲。

---

## 相关页面

- [第 3 讲 —— FlashAttention-1 算法](第03讲-FlashAttention-1算法.md)
- [第 5 讲 —— 仓库剖析与 Python / CUDA API](第05讲-仓库结构与Python-CUDA-API.md)
- NVIDIA Nsight Compute 文档：<https://docs.nvidia.com/nsight-compute/>
- PTX ISA：<https://docs.nvidia.com/cuda/parallel-thread-execution/>


<details>
<summary>English original</summary>

**1. Coalesced vs uncoalesced copy**

```cpp
// coalesce.cu
__global__ void copy_coalesced(const float* __restrict__ x, float* __restrict__ y, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) y[idx] = x[idx];
}

__global__ void copy_strided(const float* __restrict__ x, float* __restrict__ y, int n, int stride) {
    int idx = (blockIdx.x * blockDim.x + threadIdx.x) * stride;
    if (idx < n) y[idx] = x[idx];
}
```

Run both at `n = 2^24`. The coalesced one should hit ~85–90% of HBM peak. The strided one (e.g. `stride = 32`) should drop by 10–20×. Compare under `ncu --metrics gpu__time_duration.sum,dram__bytes.sum`.

**2. Warp-shuffle reduction**

```cpp
// warp_reduce.cu
__inline__ __device__ float warp_sum(float v) {
    for (int off = 16; off > 0; off >>= 1)
        v += __shfl_down_sync(0xffffffff, v, off);
    return v;
}

__global__ void rowsum_via_shuffle(const float* x, float* out, int n) {
    int row = blockIdx.x;
    int tid = threadIdx.x;
    float acc = 0.f;
    for (int j = tid; j < n; j += blockDim.x) acc += x[row * n + j];
    acc = warp_sum(acc);
    if ((tid & 31) == 0) atomicAdd(&out[row], acc);
}
```

This is structurally the same code FlashAttention uses to reduce `m` and `ℓ` across threads of a warp before broadcasting back. Time it against a naive shared-memory reduction; on Hopper the shuffle version should be ~2× faster for warp-local accumulators.

---

**Use it in the real stack**

In `flash-attention/csrc/flash_attn/src/`:

- `softmax.h`: search for `quad_shfl_xor_sync` / `Allreduce` — these are the warp shuffles that compute `m` and `ℓ` reductions inside a warp.
- `block_info.h` and `kernel_traits.h`: `kBlockM`, `kBlockN`, `kNWarps`, `kStages` — the tile sizes, warps per block, and `cp.async` stage count.
- `mask.h` and `softmax.h` again: the rescale-and-add update of `tOrO` after each `tOrP` — this is the FP register-level realisation of the recurrence from Lecture 2.

Pick one CUDA file and annotate, in your own notes, where each of these is happening. Do not move on to Lecture 5 until you can point at the MMA call, the shuffle reduce, and the `cp.async` issue / commit / wait in real source.

---

**Measure it**

Standard Nsight Compute metrics worth memorising:

| Metric | Meaning |
|--------|---------|
| `sm__throughput.avg.pct_of_peak_sustained_elapsed` | Overall SM utilisation |
| `dram__bytes.sum` | HBM traffic |
| `lts__t_sectors.sum` | L2 traffic |
| `sm__warps_active.avg.pct_of_peak_sustained_active` | Achieved occupancy |
| `smsp__inst_executed_pipe_tensor_op_hmma.sum` | Tensor-core HMMA instructions |
| `smsp__sass_average_data_bytes_per_sector_mem_global.pct` | Coalescing efficiency |

For your two microbenchmarks, report at minimum: kernel time, achieved HBM bandwidth, achieved occupancy. For the warp-shuffle reduction, also report tensor-core instruction count (should be zero — it is a pure CUDA-core kernel).

---

**Ship it**

Drop into `flash-attn-course/`:

1. `coalesce.cu` and a `coalesce_bench.txt` with the two kernel times and bandwidths.
2. `warp_reduce.cu` and a `reduce_bench.txt` comparing shuffle vs shared-memory reduce.
3. A one-page Markdown cheat sheet mapping each term in the FlashAttention kernel (`tOrO`, `tOrP`, `tOrS`, `cp.async.commit_group`, `__shfl_xor_sync`, `wgmma.mma_async`) to a one-sentence explanation in your own words.

If you have those three, you are ready for Lecture 5.

---

**Related pages**

- [Lecture 3 — FlashAttention-1 algorithm](第03讲-FlashAttention-1算法.md)
- [Lecture 5 — Repo anatomy and Python / CUDA API](第05讲-仓库结构与Python-CUDA-API.md)
- NVIDIA Nsight Compute docs: <https://docs.nvidia.com/nsight-compute/>
- PTX ISA: <https://docs.nvidia.com/cuda/parallel-thread-execution/>

</details>

---

> 原文：[`Phase 4 - Track C - DL Inference Optimization/02 - Kernel Engineering/FlashAttention Course/Lecture 04 - GPU Kernel Performance Basics.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20C%20-%20DL%20Inference%20Optimization/02%20-%20Kernel%20Engineering/FlashAttention%20Course/Lecture%2004%20-%20GPU%20Kernel%20Performance%20Basics.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
