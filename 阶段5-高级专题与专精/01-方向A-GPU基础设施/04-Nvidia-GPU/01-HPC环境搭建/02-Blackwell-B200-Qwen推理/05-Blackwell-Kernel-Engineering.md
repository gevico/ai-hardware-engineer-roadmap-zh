---
title: 第 5 章：面向 Qwen 推理的 Blackwell kernel 工程
description: 第 5 章：面向 Qwen 推理的 Blackwell kernel 工程
---

# 第 5 章：面向 Qwen 推理的 Blackwell kernel 工程

## 概述

让 Qwen 级推理在 Blackwell 上跑得快的 kernel，与跑在 Hopper 上的 kernel 并不是同一批。新的指令集（`tcgen05.mma`）、重新设计的异步拷贝引擎（TMA-2）、更宽的第五代 WGMMA 分块形状，以及利用 FP4/FP6 的 warp 特化模式——这些都是 Blackwell 专有的。为 Hopper 编译的 vLLM 或 TRT-LLM 构建版本能在 Blackwell 上运行，但会白白丢掉一大截吞吐。

本章面向需要阅读、编写或调优生产 runtime 所用 kernel 的工程师。在 B200 上部署 Qwen 并不需要你手写 WGMMA 汇编，但要想排查 30% 的性能差距，就必须理解这些 kernel 在做什么。

读完本章，你应当能够：

* 区分第五代 WGMMA 与 Hopper 的 WGMMA，并判断某个 kernel 用的是哪一种。
* 读懂 TMA-2 描述符，并说明它们能实现哪些异步拷贝模式。
* 理解 persistent kernel 架构，以及它为何主导 Blackwell 时代的 LLM 推理服务。
* 识别 FlashAttention-3 相对 FA-2 的 Blackwell 适配。
* 使用 CUTLASS 4 编写面向 FP4 混合精度的自定义 kernel。

---

## 1. 指令层面的第五代 Tensor Core

Hopper 上的 Tensor Core 暴露了 `wgmma.mma_async`——一种异步 warp-group MMA，作用于由 4 个 warp（128 个线程）组成的 warp group。Blackwell 新增了 `tcgen05.mma`，它：

* 支持把 MX-FP4/FP6 作为原生操作数类型（Hopper 的 WGMMA 没有这些）。
* 具备为新格式优化的新分块形状（FP4 用 `m64n16k64`，而 FP16 用 `m64n16k16`）。
* 降低 MX 路径的寄存器压力——这很重要，因为 FP4 GEMM 使用较小的累加器分块，编译器在 Hopper 上经常出现寄存器溢出。
* 新增对**共享缩放因子**作为第三操作数的支持，无需逐块反量化即可真正按 MX 块格式消费数据。

Blackwell 上 MX-FP4 GEMM 分块的简化 PTX 片段：

```
.reg .b32 a_desc, b_desc;            // matrix descriptors
.reg .b64 scale_desc;                // shared scale factor descriptor
.reg .b32 accum<8>;                  // FP32 accumulator tile

// New Blackwell instruction:
tcgen05.mma.async.aligned.m64n16k64.row.col.f32.e2m1.e2m1.ue8m0
    {accum0, accum1, ..., accum7},
    a_desc, b_desc, scale_desc;
```


`e2m1.e2m1.ue8m0` 这个三元组表示：操作数 A 是 MX-FP4（E2M1），操作数 B 是 MX-FP4，缩放因子是 UE8M0。累加器是 FP32（标准的 MMA 累加器类型）。

这不需要手写。CUTLASS 4 模板和 Triton 3.x 会生成它。但在 `cuobjdump` 输出中看到 `tcgen05.mma`，就是确认 Blackwell 路径已启用的方式。

### 1.1 如果在 Blackwell 上看到 `wgmma.mma_async`

它能跑起来——Hopper 指令向前兼容——但对于 MX-FP4 工作负载，你会丢掉约 30–40% 的峰值吞吐。编译器选择了更安全/更旧的指令。常见原因：

* CUDA toolkit < 13。
* CUTLASS < 4.0。
* TRT-LLM < 0.20。
* 自定义 kernel 用 `-arch=sm_90` 而不是 `sm_100` 构建。

用面向 Blackwell 的工具链重新构建，并重新反汇编。

---

## 2. TMA-2：异步拷贝引擎

Hopper 引入了 **Tensor Memory Accelerator（TMA）**——一种拷贝引擎，异步地把多维分块数据从 HBM 搬到共享内存，从而在加载进行时释放 warp 去做计算。Blackwell 将其扩展为 **TMA-2**，具备：

* **多播**——一次 TMA 加载可以把同一个分块同时投递到多个 SM 的共享内存。对于 K、V 分块会被许多 SM 读取的 attention kernel 而言至关重要。
* **写回时归约**——TMA 能以类原子行为把写入累加到 HBM，适用于跨 SM 的部分归约。
* **更大的描述符**——最多支持 5D 分块描述符（Hopper 是 4D），更易表达复杂的 paged attention KV 布局。
* **更低延迟的完成信号**——SM 拿到完成屏障所需的周期数约为原来的一半。

具体到 Qwen 推理，TMA-2 多播显著改变了 attention kernel：

```
FlashAttention on Hopper:
  Each SM loads its own K and V tiles independently.
  Aggregate HBM read traffic = N_SMs × (K + V tile size).

FlashAttention-3 on Blackwell with TMA-2 multicast:
  One TMA load broadcasts K (or V) to all SMs that need it.
  Aggregate HBM read traffic = 1 × (K + V tile size).
```


对于长上下文下的 Qwen2.5-72B，这大约能把 attention 期间的 KV cache 带宽压力**减半**——这是 decode（逐 token 生成阶段）中第二大的带宽负载（仅次于权重）。

---


<details>
<summary>English original</summary>

**Chapter 5: Blackwell Kernel Engineering for Qwen Inference**

**Overview**

The kernels that make Qwen-class inference fast on Blackwell are not the same kernels that ran on Hopper. The new instruction set (`tcgen05.mma`), the redesigned async copy engine (TMA-2), the broader 5th-gen WGMMA tile shapes, and the warp specialization patterns that exploit FP4/FP6 — all of these are Blackwell-specific. A vLLM or TRT-LLM build compiled for Hopper will run on Blackwell, but will leave a large factor of throughput on the table.

This chapter is for engineers who need to read, write, or tune the kernels that production runtimes use. You don't have to write WGMMA assembly to deploy Qwen on B200, but you do have to understand what the kernels are doing if you want to debug a 30% performance gap.

By the end you should be able to:

* Distinguish 5th-gen WGMMA from Hopper's WGMMA and identify which one a kernel uses.
* Read TMA-2 descriptors and explain what async copy patterns they enable.
* Understand persistent-kernel architecture and why it dominates Blackwell-era LLM serving.
* Recognize FlashAttention-3's Blackwell adaptations vs FA-2.
* Use CUTLASS 4 for custom kernels that target FP4 mixed precision.

---

**1. 5th-Generation Tensor Cores at the Instruction Level**

Tensor cores on Hopper exposed `wgmma.mma_async` — an async warp-group MMA that operated on warp groups of 4 warps (128 threads). Blackwell adds `tcgen05.mma`, which:

* Supports MX-FP4/FP6 as native operand types (Hopper's WGMMA didn't have these).
* Has new tile shapes optimized for the new formats (`m64n16k64` for FP4 vs `m64n16k16` for FP16).
* Reduces register pressure for the MX paths — important because FP4 GEMMs use small accumulator tiles and the compiler often spills on Hopper.
* Adds support for **shared scale factors** as a third operand, enabling true MX block-format consumption without per-block dequant.

A simplified PTX snippet for an MX-FP4 GEMM tile on Blackwell:

```
.reg .b32 a_desc, b_desc;            // matrix descriptors
.reg .b64 scale_desc;                // shared scale factor descriptor
.reg .b32 accum<8>;                  // FP32 accumulator tile

// New Blackwell instruction:
tcgen05.mma.async.aligned.m64n16k64.row.col.f32.e2m1.e2m1.ue8m0
    {accum0, accum1, ..., accum7},
    a_desc, b_desc, scale_desc;
```

The `e2m1.e2m1.ue8m0` triplet says: operand A is MX-FP4 (E2M1), operand B is MX-FP4, scale factors are UE8M0. The accumulator is FP32 (the standard MMA accumulator type).

You won't write this by hand. CUTLASS 4 templates and Triton 3.x emit it. But seeing `tcgen05.mma` in your `cuobjdump` output is how you confirm the Blackwell path is active.

**1.1 If you see `wgmma.mma_async` on Blackwell**

It runs — Hopper instructions are forward-compatible — but you're leaving ~30–40% of peak throughput on the table for MX-FP4 workloads. The compiler chose the safer/older instruction. Common causes:

* CUDA toolkit < 13.
* CUTLASS < 4.0.
* TRT-LLM < 0.20.
* Custom kernel built with `-arch=sm_90` instead of `sm_100`.

Rebuild with Blackwell-targeted toolchain and re-disassemble.

---

**2. TMA-2: The Async Copy Engine**

Hopper introduced the **Tensor Memory Accelerator (TMA)** — a copy engine that moves multi-dimensional tile chunks from HBM to shared memory asynchronously, freeing warps to do compute while the load happens. Blackwell extends it to **TMA-2** with:

* **Multicast** — one TMA load can deliver the same tile to multiple SMs' shared memory simultaneously. Crucial for attention kernels where K and V tiles are read by many SMs.
* **Reduction-on-store** — TMA can accumulate stores into HBM with atomic-like behavior, useful for cross-SM partial reductions.
* **Larger descriptors** — supports up to 5D tile descriptors (vs Hopper's 4D), making it easier to express complex paged-attention KV layouts.
* **Lower-latency completion signals** — the SM gets the completion barrier in ~half the cycles.

For Qwen inference specifically, TMA-2 multicast changes attention kernels significantly:

```
FlashAttention on Hopper:
  Each SM loads its own K and V tiles independently.
  Aggregate HBM read traffic = N_SMs × (K + V tile size).

FlashAttention-3 on Blackwell with TMA-2 multicast:
  One TMA load broadcasts K (or V) to all SMs that need it.
  Aggregate HBM read traffic = 1 × (K + V tile size).
```

For Qwen2.5-72B at long context, this can roughly **halve** the KV cache bandwidth pressure during attention — the second-biggest bandwidth load in decode (after weights).

---

</details>

## 3. 持久化 kernel 模式

Blackwell 上占主导的推理 kernel 架构是**持久化 kernel**。它不是为每个矩阵乘启动一个 kernel（即便有 CUDA Graphs，每次启动的开销也真实存在），而是：

* 启动时只启动一次，grid 大小与 SM 数量匹配。
* 每个 SM 运行一个无限循环，从队列读取工作项。
* 工作项描述「在这些输入上做这个矩阵乘，并把输出写到这里」。
* 当 host 要计算第 N 层的 QKV 时，把工作项推入队列；持久化 kernel 取走它。

在 Blackwell 上的收益：

* 每个算子的 kernel 启动开销为零。
* SM 保持热态；算子之间不冲刷指令缓存。
* 跨算子的计算与内存重叠更好 —— kernel 可以在计算当前算子的同时预取下一个算子的 TMA load。
* 与 CUDA Graphs 兼容，可用于 host 侧派发。

对 Qwen 的 decode（逐 token 生成阶段）热路径而言，持久化 kernel 消除了每 token 约 150 µs 的启动开销（分布在约 470 个未融合 kernel 上，即使它们都很小），否则在 Blackwell 上，其他一切都很快，这会限制 tok/s 的上限。

### 3.1 持久化 kernel 设计的骨架

```c++
// Pseudocode for a persistent GEMM kernel on Blackwell
__launch_bounds__(256, 1)
__global__ void persistent_gemm(WorkQueue* queue) {
    while (true) {
        WorkItem item = queue->try_pop();
        if (item.type == WorkType::SHUTDOWN) break;
        if (item.type == WorkType::IDLE) {
            __nanosleep(100);
            continue;
        }

        // Execute the matmul described by item
        if (item.dtype == DType::MX_FP4) {
            gemm_tile_mx_fp4(item.A, item.B, item.scale, item.C,
                             item.M, item.N, item.K);
        } else if (item.dtype == DType::MX_FP8) {
            gemm_tile_mx_fp8(item.A, item.B, item.scale, item.C, ...);
        }

        // Signal completion
        atomicAdd(&item.completion_counter, 1);
    }
}
```

实践中，TRT-LLM、CUTLASS 4 PerformanceKernel 和 vLLM 的 Blackwell 后端都使用该模式的变体。持久化 kernel 架构也是 TRT-LLM 以更高灵活性（不受捕获形状锁定）取得与 CUDA Graph 相当的稳态效率的方式。

---

## 4. Blackwell 上的 FlashAttention-3

FlashAttention-2（Hopper）曾是 Llama/Qwen 等推理的经典 attention kernel。FlashAttention-3 是它在 Blackwell 上的演进，有几处实质性变化：

| Aspect | FA-2 (Hopper) | FA-3 (Blackwell) |
|---|---|---|
| Tensor Core 指令 | `wgmma.mma_async` | `tcgen05.mma.async` |
| 用到的 TMA 特性 | TMA（load/store） | TMA-2（multicast、reduce-on-store） |
| Warp 特化 | 生产者/消费者拆分 | 生产者/消费者 + 归约 warp |
| 累加器精度 | FP32 | FP32（不变） |
| 最大分块尺寸 | 128 × 64 | 256 × 64（每个 SM 寄存器更多） |
| Q 精度 | FP16/BF16 | FP16/BF16/FP8 |
| K、V 精度 | FP16/BF16 | FP16/BF16/FP8/FP4 |
| 异步流水线深度 | 2-3 | 4-6 级 |
| 同架构下相对 FA-2 的加速比 | 基线 | MX-FP4/FP8 路径上 1.6–2.2× |

在 Blackwell 上，TRT-LLM 默认使用 FA-3。如果你的 kernel trace 显示的是 `flash_attention_v2_decode_kernel` 而不是 `fa3_decode_*_sm100`，说明你走的是旧路径 —— 启用 FA-3 重新构建。

### 4.1 decode 专用变体

对于自回归 decode（每个 query 的 seq_len=1），有一个专用 kernel：**FlashDecoding-3**。它：

* 沿 KV cache 维度并行（而不是 Q 维度，Q 维度为 1）。
* 使用 TMA-2 multicast 把单个 Q 广播到所有 SM。
* 通过原子累加的 TMA store 在 SM 之间归约部分结果。

结果是：在单张 B200 上，Qwen2.5-72B 一个 decode 步的 attention kernel 在 ctx=4k 时耗时约 0.5 ms，在 ctx=32k 时约 3 ms。这就是第 3 章 §3 中每 token 延迟背后的 kernel 级开销。

---


<details>
<summary>English original</summary>

**3. The Persistent-Kernel Pattern**

The dominant inference-kernel architecture on Blackwell is the **persistent kernel**. Instead of launching one kernel per matmul (the per-launch overhead is real even with CUDA Graphs), a persistent kernel:

* Launches once at startup, with grid size matching SM count.
* Each SM runs an infinite loop reading work items from a queue.
* Work items describe "do this matmul on these inputs and write output here."
* When the host wants to compute layer N's QKV, it pushes a work item to the queue; the persistent kernel picks it up.

Benefits on Blackwell:

* Zero per-op kernel launch overhead.
* SMs stay warm; instruction caches don't get flushed between ops.
* Better overlap of compute and memory across operations — the kernel can prefetch the next op's TMA load while computing the current.
* Compatible with CUDA Graphs for the host-side dispatch.

For the Qwen decode hot path, persistent kernels eliminate the ~150 µs of launch overhead per token (across ~470 unfused kernels, even tiny) that would otherwise cap tok/s on Blackwell where everything else is fast.

**3.1 Skeleton of a persistent-kernel design**

```c++
// Pseudocode for a persistent GEMM kernel on Blackwell
__launch_bounds__(256, 1)
__global__ void persistent_gemm(WorkQueue* queue) {
    while (true) {
        WorkItem item = queue->try_pop();
        if (item.type == WorkType::SHUTDOWN) break;
        if (item.type == WorkType::IDLE) {
            __nanosleep(100);
            continue;
        }

        // Execute the matmul described by item
        if (item.dtype == DType::MX_FP4) {
            gemm_tile_mx_fp4(item.A, item.B, item.scale, item.C,
                             item.M, item.N, item.K);
        } else if (item.dtype == DType::MX_FP8) {
            gemm_tile_mx_fp8(item.A, item.B, item.scale, item.C, ...);
        }

        // Signal completion
        atomicAdd(&item.completion_counter, 1);
    }
}
```

In practice TRT-LLM, CUTLASS 4 PerformanceKernel, and vLLM's Blackwell backend all use variants of this pattern. The persistent-kernel architecture is also how TRT-LLM achieves CUDA-Graph-comparable steady-state efficiency with more flexibility (no captured-shape lock-in).

---

**4. FlashAttention-3 on Blackwell**

FlashAttention-2 (Hopper) was the canonical attention kernel for Llama/Qwen/etc. inference. FlashAttention-3 is its Blackwell evolution, with several substantive changes:

| Aspect | FA-2 (Hopper) | FA-3 (Blackwell) |
|---|---|---|
| Tensor core instruction | `wgmma.mma_async` | `tcgen05.mma.async` |
| TMA features used | TMA (load/store) | TMA-2 (multicast, reduce-on-store) |
| Warp specialization | Producer/consumer split | Producer/consumer + reduction warp |
| Accumulator precision | FP32 | FP32 (unchanged) |
| Max tile size | 128 × 64 | 256 × 64 (more registers per SM) |
| Q precision | FP16/BF16 | FP16/BF16/FP8 |
| K, V precision | FP16/BF16 | FP16/BF16/FP8/FP4 |
| Async pipeline depth | 2-3 | 4-6 stages |
| Speedup vs FA-2 same arch | baseline | 1.6–2.2× on MX-FP4/FP8 paths |

FA-3 is what TRT-LLM uses by default on Blackwell. If your kernel trace shows `flash_attention_v2_decode_kernel` rather than `fa3_decode_*_sm100`, you're on the older path — rebuild with FA-3 enabled.

**4.1 Decode-specific variants**

For autoregressive decode (seq_len=1 per query), there's a specialized kernel: **FlashDecoding-3**. It:

* Parallelizes across the KV cache dimension (not the Q dimension, which is 1).
* Uses TMA-2 multicast to broadcast the single Q to all SMs.
* Reduces partial results across SMs via atomic-accumulating TMA stores.

The result: a Qwen2.5-72B decode step's attention kernel runs in ~0.5 ms on a single B200 at ctx=4k, ~3 ms at ctx=32k. That's the kernel-level cost behind the per-token latencies in Chapter 3 §3.

---

</details>

## 5. 面向自定义 kernel 的 CUTLASS 4

如果 runtime 自带 kernel 不覆盖你的场景（自定义融合、新型量化、特殊 attention pattern），就会转向 CUTLASS。CUTLASS 4 是感知 Blackwell 的迭代版本。

心智模型：CUTLASS 是一个 C++ 模板库，通过选择以下内容来组合分块级 matmul kernel：

* 分块尺寸（`m`、`n`、`k`）。
* 操作数 dtype（FP4 / FP6 / FP8 / FP16）。
* 累加器 dtype（FP32 或 FP16）。
* Epilogue（激活函数、缩放、存储模式）。
* 调度（kernel 架构 —— persistent、cooperative 等）。

面向 MX-FP4 混合 Qwen FFN 的最小 CUTLASS 4 GEMM（矩阵-矩阵乘）模板：

```cpp
using ElementA = cutlass::float_e2m1_t;      // MX-FP4
using ElementB = cutlass::float_e2m1_t;
using ElementAccumulator = float;            // FP32
using ElementC = cutlass::bfloat16_t;        // BF16 output

using GemmKernel = cutlass::gemm::collective::CollectiveBuilder<
    cutlass::arch::Sm100,                     // Blackwell
    cutlass::arch::OpClassTensorOp,
    ElementA, cutlass::layout::RowMajor, 16,
    ElementB, cutlass::layout::ColumnMajor, 16,
    ElementAccumulator,
    Shape<_128, _128, _128>,                  // tile size
    Shape<_2, _1, _1>,                        // cluster shape
    cutlass::gemm::collective::StageCountAutoCarveout<
        sizeof(typename CollectiveMainloop::SharedStorage)>,
    cutlass::gemm::KernelTmaWarpSpecializedFP4Pingpong
>::CollectiveOp;

// ... epilogue ...

using GemmOperator = cutlass::gemm::device::GemmUniversalAdapter<GemmKernel>;
```

这能提供：

* 一个面向 Blackwell、使用 TMA-2、采用 MX-FP4 ping-pong 调度的 GEMM kernel。
* 分块尺寸 128×128×128，采用 warp 专用化的 producer/consumer。
* 通过 `Pingpong` 调度实现的 persistent-kernel 架构。

当生产 runtime 不具备模型所需的精确 dtype/shape 组合时，这就是你要操作的层级。

---

## 6. Blackwell 上的 warp 专用化

Hopper 引入了 warp 专用化 —— CTA 内不同 warp 执行不同程序（producer warp 执行 TMA 加载，consumer warp 执行 MMA）。Blackwell 在此基础上扩展出：

* **归约 warp** —— 在 SM 内累加部分和的专用 warp，可用于 attention 的 softmax 阶段。
* **更紧密的 producer/consumer 平衡** —— TMA-2 更低的完成延迟意味着 producer warp 花更少时间等待、更多时间预取。
* **更多 warp-group MMA 流水线级数** —— 每个 warp group 有 4-6 级在途 MMA（相比 Hopper 的 2-3 级），降低每级寄存器压力。

Blackwell 上 Qwen2.5-72B 的简化 persistent decode-kernel 布局（decode 即逐 token 生成阶段）：

```
Per SM (Blackwell has ~160 SMs per B200):
  Warp group 0 (warps 0-3):  TMA-2 producer
    - Issues async loads of weight tiles and KV tiles
    - Waits on completion barriers
  Warp group 1 (warps 4-7):  MMA consumer
    - Runs tcgen05.mma on incoming tiles
    - Accumulates into FP32 register tile
  Warp group 2 (warps 8-11): Reduction warp group
    - Accumulates across SM via shared memory
    - Issues TMA-2 reduce-on-store to global memory
```

这种三 warp-group 模式现在是 Blackwell 上 LLM kernel 的标准。旧式的双 warp-group 设计（Hopper 时代）把归约工作留给某个 consumer group，并且使 SM 利用不足。

---

## 7. Blackwell 上的 Triton 3.x

对于更高层级的 kernel 工作，Triton 3.x 增加了 Blackwell 支持：

```python
import triton
import triton.language as tl

@triton.jit
def qwen_ffn_mx_fp4_kernel(
    X_ptr, W_gate_ptr, W_up_ptr, W_down_ptr,
    Out_ptr, scale_ptr,
    M, N, K,
    BLOCK_M: tl.constexpr,
    BLOCK_N: tl.constexpr,
    BLOCK_K: tl.constexpr,
):
    # Triton 3.x exposes MX-FP4 via tl.dot with format hints
    pid_m = tl.program_id(0)
    pid_n = tl.program_id(1)

    # Compute SwiGLU FFN as one fused kernel
    offs_m = pid_m * BLOCK_M + tl.arange(0, BLOCK_M)
    offs_n = pid_n * BLOCK_N + tl.arange(0, BLOCK_N)

    gate = tl.zeros((BLOCK_M, BLOCK_N), dtype=tl.float32)
    up   = tl.zeros((BLOCK_M, BLOCK_N), dtype=tl.float32)

    for k in range(0, K, BLOCK_K):
        offs_k = k + tl.arange(0, BLOCK_K)
        x  = tl.load(X_ptr + offs_m[:, None]*K + offs_k[None, :])
        wg = tl.load(W_gate_ptr + offs_n[:, None]*K + offs_k[None, :],
                     dtype=tl.float_e2m1)         # MX-FP4 hint
        wu = tl.load(W_up_ptr   + offs_n[:, None]*K + offs_k[None, :],
                     dtype=tl.float_e2m1)
        gate += tl.dot(x, wg.T)                   # uses tcgen05.mma
        up   += tl.dot(x, wu.T)

    swiglu = tl.sigmoid(gate) * gate * up         # SwiGLU
    # Then a second matmul into W_down...
```

Triton 通过 MLIR 后端生成 Blackwell SASS。2026 年，很多“小型自定义 kernel 工作”就是这么完成的 —— 无需编写 PTX 即可获得完整的 WGMMA 级控制。编译时间可能很慢（FP4 下自动调优扫描会激增，因为分块空间更大），但对许多 shape 来说，runtime 可与手工调优的 CUTLASS 竞争。


<details>
<summary>English original</summary>

**5. CUTLASS 4 for Custom Kernels**

If a runtime's stock kernels don't cover your case (custom fusion, novel quantization, special attention pattern), you'll go to CUTLASS. CUTLASS 4 is the Blackwell-aware iteration.

The mental model: CUTLASS is a C++ template library that lets you compose tile-level matmul kernels by picking:

* Tile sizes (`m`, `n`, `k`).
* Operand dtypes (FP4 / FP6 / FP8 / FP16).
* Accumulator dtype (FP32 or FP16).
* Epilogue (activation, scaling, store pattern).
* Schedule (kernel architecture — persistent, cooperative, etc.).

A minimal CUTLASS 4 GEMM template for MX-FP4 mixed Qwen FFN:

```cpp
using ElementA = cutlass::float_e2m1_t;      // MX-FP4
using ElementB = cutlass::float_e2m1_t;
using ElementAccumulator = float;            // FP32
using ElementC = cutlass::bfloat16_t;        // BF16 output

using GemmKernel = cutlass::gemm::collective::CollectiveBuilder<
    cutlass::arch::Sm100,                     // Blackwell
    cutlass::arch::OpClassTensorOp,
    ElementA, cutlass::layout::RowMajor, 16,
    ElementB, cutlass::layout::ColumnMajor, 16,
    ElementAccumulator,
    Shape<_128, _128, _128>,                  // tile size
    Shape<_2, _1, _1>,                        // cluster shape
    cutlass::gemm::collective::StageCountAutoCarveout<
        sizeof(typename CollectiveMainloop::SharedStorage)>,
    cutlass::gemm::KernelTmaWarpSpecializedFP4Pingpong
>::CollectiveOp;

// ... epilogue ...

using GemmOperator = cutlass::gemm::device::GemmUniversalAdapter<GemmKernel>;
```

What this gives you:

* A Blackwell-targeted, TMA-2-using, MX-FP4 ping-pong-scheduled GEMM kernel.
* Tile size 128×128×128, warp-specialized producer/consumer.
* Persistent-kernel architecture via the `Pingpong` schedule.

This is the level you operate at when production runtimes don't have the exact dtype/shape combo your model wants.

---

**6. Warp Specialization on Blackwell**

Hopper introduced warp specialization — different warps within a CTA execute different programs (producer warps do TMA loads, consumer warps do MMA). Blackwell extends this with:

* **Reduction warps** — dedicated warps that accumulate partial sums across the SM, useful for attention's softmax stage.
* **Tighter producer/consumer balance** — TMA-2's lower-latency completion means producer warps spend less time waiting, more time prefetching.
* **More warp-group MMA pipeline stages** — 4-6 stages of in-flight MMA per warp group (vs 2-3 on Hopper), reducing register pressure per stage.

A simplified persistent decode-kernel layout for Qwen2.5-72B on Blackwell:

```
Per SM (Blackwell has ~160 SMs per B200):
  Warp group 0 (warps 0-3):  TMA-2 producer
    - Issues async loads of weight tiles and KV tiles
    - Waits on completion barriers
  Warp group 1 (warps 4-7):  MMA consumer
    - Runs tcgen05.mma on incoming tiles
    - Accumulates into FP32 register tile
  Warp group 2 (warps 8-11): Reduction warp group
    - Accumulates across SM via shared memory
    - Issues TMA-2 reduce-on-store to global memory
```

This three-warp-group pattern is now standard for LLM kernels on Blackwell. Older two-warp-group designs (Hopper-era) leave the reduction work to one of the consumer groups and underutilize the SM.

---

**7. Triton 3.x on Blackwell**

For higher-level kernel work, Triton 3.x added Blackwell support:

```python
import triton
import triton.language as tl

@triton.jit
def qwen_ffn_mx_fp4_kernel(
    X_ptr, W_gate_ptr, W_up_ptr, W_down_ptr,
    Out_ptr, scale_ptr,
    M, N, K,
    BLOCK_M: tl.constexpr,
    BLOCK_N: tl.constexpr,
    BLOCK_K: tl.constexpr,
):
    # Triton 3.x exposes MX-FP4 via tl.dot with format hints
    pid_m = tl.program_id(0)
    pid_n = tl.program_id(1)

    # Compute SwiGLU FFN as one fused kernel
    offs_m = pid_m * BLOCK_M + tl.arange(0, BLOCK_M)
    offs_n = pid_n * BLOCK_N + tl.arange(0, BLOCK_N)

    gate = tl.zeros((BLOCK_M, BLOCK_N), dtype=tl.float32)
    up   = tl.zeros((BLOCK_M, BLOCK_N), dtype=tl.float32)

    for k in range(0, K, BLOCK_K):
        offs_k = k + tl.arange(0, BLOCK_K)
        x  = tl.load(X_ptr + offs_m[:, None]*K + offs_k[None, :])
        wg = tl.load(W_gate_ptr + offs_n[:, None]*K + offs_k[None, :],
                     dtype=tl.float_e2m1)         # MX-FP4 hint
        wu = tl.load(W_up_ptr   + offs_n[:, None]*K + offs_k[None, :],
                     dtype=tl.float_e2m1)
        gate += tl.dot(x, wg.T)                   # uses tcgen05.mma
        up   += tl.dot(x, wu.T)

    swiglu = tl.sigmoid(gate) * gate * up         # SwiGLU
    # Then a second matmul into W_down...
```

Triton emits Blackwell SASS via the MLIR backend. It's how a lot of "small custom kernel work" gets done in 2026 — full WGMMA-level control without writing PTX. The compile times can be slow (auto-tuning sweeps blow up at FP4 because the tile space is larger), but the runtime is competitive with hand-tuned CUTLASS for many shapes.

---

</details>

## 8. 性能诊断——你的 kernel 在 Blackwell 路径上吗？

```bash
# 1. Disassemble the kernel and look for tcgen05.mma
cuobjdump --dump-sass your_kernel.cubin | grep -E "tcgen05|wgmma" | head
# Expect tcgen05.mma; if only wgmma, you're on the Hopper path.

# 2. Confirm TMA-2 multicast in attention kernels
cuobjdump --dump-sass fa3_kernel.cubin | grep "TMA.LDGSTS.MULTICAST"
# Expect at least one MULTICAST instruction per K/V load.

# 3. Profile with Nsight Compute
ncu --set full --kernel-name "gemm_mx_fp4_sm100" your_app
# Look at:
#   - Achieved Occupancy (target: > 60%)
#   - TC Throughput (target: > 50% of peak FP4)
#   - DRAM Bandwidth (target: > 70% of peak)
#   - L2 Cache Hit Rate (target: > 30% for repeated weight loads)

# 4. Confirm persistent-kernel pattern
ncu --metrics launch__waves_per_multiprocessor your_app
# Persistent: ~1 wave per SM (kernel runs continuously)
# Non-persistent: many waves (kernel launches per op)
```

如果缺少 `tcgen05.mma`，先修构建。如果缺少 TMA-2 MULTICAST，说明你在 FA-2 而不是 FA-3 上。如果 occupancy 偏低，说明 tile 大小或寄存器压力不理想——通过 Triton 的 autotune 或 CUTLASS 模板参数来调。

---

## 9. 真实世界中的栈映射

2026 年中期各层所处的位置：

| layer | 工具 | 用途 |
|---|---|---|
| 面向用户的 API | OpenAI 兼容 REST | `/v1/chat/completions` |
| 推理服务前端 | TRT-LLM Triton frontend / vLLM | 请求批处理、调度 |
| 引擎 | TensorRT engine（已编译） | 编译一次的计算图 |
| kernel 库 | TRT-LLM kernels、FA-3、自定义 CUTLASS | 逐算子实现 |
| tile 级 GEMM | CUTLASS 4（模板化） | 可复用的 tile kernel |
| 自定义 GPU 代码 | Triton 3.x | 快速融合 kernel、探索 |
| PTX 汇编 | 手写 / cuobjdump 输出 | 仅用于性能调试 |
| SASS | NVCC 后端 / SM SASS | SM 上实际运行的代码 |

在常规 LLM 部署中，你很少触碰最底下的三层。当现成 kernel 不支持你的 dtype/shape 时，你会用到 CUTLASS 4。原型验证新的融合时，你会用到 Triton。大多数时候你在最上面三层，底层会自行运作——*前提是*你已经用上面的诊断验证了正确的 kernel 路径已激活。

---

## 关键要点

| 要点 | 为什么重要 |
|---|---|
| `tcgen05.mma` 是新指令；`wgmma` 是回退方案 | 在反汇编中查找它以确认 Blackwell 路径 |
| TMA-2 multicast 将 attention 中的 KV 带宽减半 | 对长上下文 decode 至关重要 |
| persistent kernel 主导 Blackwell LLM 推理服务 | 零启动开销、热缓存、工作队列派发 |
| FlashAttention-3 是标准 attention；FA-2 是遗留方案 | 在 MX-FP4 路径上有 1.6–2.2× 加速 |
| CUTLASS 4 是编写自定义 Blackwell kernel 的层 | 模板化 tile 级组合，FP4 为一等公民 |
| Triton 3.x 通过 `tl.dot` 和 dtype 提示处理 MX-FP4 | 无需 PTX 即可快速探索 |
| 诊断：反汇编显示 `tcgen05`，ncu 显示 >60% occupancy | 每次部署都要验证，不只是第一次 |

---

## 资源

* **[NVIDIA CUDA 13 Programming Guide](https://docs.nvidia.com/cuda/cuda-c-programming-guide/)：** `tcgen05.mma`，TMA-2 参考。
* **[CUTLASS 4 GitHub](https://github.com/NVIDIA/cutlass)：** 感知 Blackwell 的 tile kernel 模板。
* **[FlashAttention-3 论文](https://arxiv.org/abs/2407.08608)：** attention kernel 设计。
* **[Triton 3.x 文档](https://triton-lang.org/)：** 更高层的 kernel DSL。
* **[Blackwell 上的 Nsight Compute](https://docs.nvidia.com/nsight-compute/)：** 性能剖析。
* **[阶段 5 — CUDA 高级优化](../03-CUDA高级优化/README.md)：** persistent kernel、warp specialization（Hopper 基础）。
* **[Qwen 推理 — 第 6 讲（Batched GEMM）](../../../../03-方向C-边缘AI/05-Qwen推理优化/Lecture-06.md)：** 与本讲对应的 cuBLAS 内容。
* **[第 6 章 — Blackwell 上的生产级推理服务](06-Production-Serving-on-Blackwell.md)：** 把 kernel 投入生产。


<details>
<summary>English original</summary>

**8. Performance Diagnostics — Is Your Kernel on the Blackwell Path?**

```bash
# 1. Disassemble the kernel and look for tcgen05.mma
cuobjdump --dump-sass your_kernel.cubin | grep -E "tcgen05|wgmma" | head
# Expect tcgen05.mma; if only wgmma, you're on the Hopper path.

# 2. Confirm TMA-2 multicast in attention kernels
cuobjdump --dump-sass fa3_kernel.cubin | grep "TMA.LDGSTS.MULTICAST"
# Expect at least one MULTICAST instruction per K/V load.

# 3. Profile with Nsight Compute
ncu --set full --kernel-name "gemm_mx_fp4_sm100" your_app
# Look at:
#   - Achieved Occupancy (target: > 60%)
#   - TC Throughput (target: > 50% of peak FP4)
#   - DRAM Bandwidth (target: > 70% of peak)
#   - L2 Cache Hit Rate (target: > 30% for repeated weight loads)

# 4. Confirm persistent-kernel pattern
ncu --metrics launch__waves_per_multiprocessor your_app
# Persistent: ~1 wave per SM (kernel runs continuously)
# Non-persistent: many waves (kernel launches per op)
```

If `tcgen05.mma` is absent, fix the build. If TMA-2 MULTICAST is absent, you're on FA-2 not FA-3. If occupancy is low, your tile size or register pressure is suboptimal — tune via Triton's autotune or CUTLASS template parameters.

---

**9. The Real-World Stack Map**

Where each layer lives in mid-2026:

| Layer | Tool | Purpose |
|---|---|---|
| User-facing API | OpenAI-compatible REST | `/v1/chat/completions` |
| Serving frontend | TRT-LLM Triton frontend / vLLM | Request batching, scheduling |
| Engine | TensorRT engine (compiled) | The compiled-once graph |
| Kernel library | TRT-LLM kernels, FA-3, custom CUTLASS | The per-op implementations |
| Tile-level GEMM | CUTLASS 4 (templated) | Reusable tile kernels |
| Custom GPU code | Triton 3.x | Quick fused kernels, exploration |
| PTX assembly | Manual / cuobjdump output | Performance debugging only |
| SASS | NVCC backend / SM SASS | What actually runs on the SM |

You rarely touch the bottom three layers in normal LLM deployment. You touch CUTLASS 4 when stock kernels are missing your dtype/shape. You touch Triton when prototyping a new fusion. Most of the time you're at the top three layers and the bottom is taking care of itself — *if* you've validated with the diagnostics above that the right kernel paths are active.

---

**Key Takeaways**

| Takeaway | Why it matters |
|---|---|
| `tcgen05.mma` is the new instruction; `wgmma` is the fallback | Look for it in disassembly to confirm Blackwell path |
| TMA-2 multicast halves KV bandwidth in attention | Crucial for long-context decode |
| Persistent kernels dominate Blackwell LLM serving | Zero launch overhead, warm caches, work-queue dispatch |
| FlashAttention-3 is the canonical attention; FA-2 is legacy | 1.6–2.2× speedup on the MX-FP4 path |
| CUTLASS 4 is the layer for custom Blackwell kernels | Templated tile-level composition with FP4 first-class |
| Triton 3.x handles MX-FP4 with `tl.dot` and dtype hints | Quick exploration without PTX |
| Diagnostics: disasm shows `tcgen05`, ncu shows >60% occupancy | Validate every deployment, not just first one |

---

**Resources**

* **[NVIDIA CUDA 13 Programming Guide](https://docs.nvidia.com/cuda/cuda-c-programming-guide/):** `tcgen05.mma`, TMA-2 reference.
* **[CUTLASS 4 GitHub](https://github.com/NVIDIA/cutlass):** Blackwell-aware tile kernel templates.
* **[FlashAttention-3 paper](https://arxiv.org/abs/2407.08608):** The attention kernel design.
* **[Triton 3.x documentation](https://triton-lang.org/):** Higher-level kernel DSL.
* **[Nsight Compute on Blackwell](https://docs.nvidia.com/nsight-compute/):** Profiling.
* **[Phase 5 — CUDA Advanced Optimization](../03-CUDA高级优化/README.md):** Persistent kernels, warp specialization (Hopper foundation).
* **[Qwen Inference — Lecture 6 (Batched GEMM)](../../../../03-方向C-边缘AI/05-Qwen推理优化/Lecture-06.md):** cuBLAS counterpart to this lecture.
* **[Chapter 6 — Production Serving on Blackwell](06-Production-Serving-on-Blackwell.md):** Putting the kernels into production.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Blackwell-B200-Qwen-Inference/05-Blackwell-Kernel-Engineering.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Blackwell-B200-Qwen-Inference/05-Blackwell-Kernel-Engineering.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
