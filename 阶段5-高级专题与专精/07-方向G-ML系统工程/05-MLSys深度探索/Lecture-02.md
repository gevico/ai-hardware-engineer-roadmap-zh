---
title: Lecture 02 - Kernel 语言大爆发：tile 成为新的 ISA
description: Lecture 02 - Kernel 语言大爆发：tile 成为新的 ISA
---

# Lecture 02 - Kernel 语言大爆发：tile 成为新的 ISA

**合集：** [MLSys Deep Dives](README.md) | **上一讲：** [← Lecture 01](Lecture-01.md) | **下一讲：** [Lecture 03](Lecture-03.md)

---

第 1 讲说过，每个 token 的成本由 tokens-per-second 决定，而 tokens-per-second 的起点是 kernel。那么：2026 年，一个快速的 GPU kernel 究竟是怎么*写*出来的？这个答案在过去三年里的变化，比之前十年的变化还要大，而且方向只有一个——**从标量线程转向 tile**。

本讲就是这场变化的地图：为什么 tile 成了 GPU 编程的单位，以及对那些争夺 tile 主导权的语言的一次实战巡览——**Triton、CUTLASS/CuTe/cuTile、ThunderKittens、TileLang**——并按唯一真正区分它们的那个轴来排列：它们交给你多少控制权，相对于它们替你做多少决定。

---

## 学习目标

本讲结束时，你应该能够：

1. 解释为什么标量 SIMT（以线程为中心）模型会失效，以及为什么 **tile** 是 Tensor Core 硬件上的自然单位。
2. 把主要的 kernel 工具放到 **易用 ↔ 控制** 的谱系上，并说出每个工具替你决定了什么、又暴露了什么。
3. 编写并读懂一个 **Triton** tile kernel，并解释它在 `torch.compile`/Inductor 下的作用。
4. 区分 NVIDIA 的三条入门路径——**CUTLASS C++ + CuTe**、**CuTe DSL**、**cuTile**——以及 "Tile IR" 是什么。
5. 说明 **ThunderKittens** 和 **TileLang** 各自优化的是什么，以及什么时候你会越过 Triton 去选它们。
6. 把一次 kernel 选型联系回第 1 讲的 tokens/s 和 `$/Mtok`。

---

## 1. 线程模型为何失效

十年来，CUDA 的心智模型是**标量线程**：你写出一个线程做什么，启动数以百万计的线程，并手工推理 warp、共享内存和同步。这个模型与硬件是匹配的——大量简单的标量 lane。

然后硬件不再是标量 lane。现代加速器主要由以下部分主导：

```text
   Tensor Cores       do a whole MATRIX-TILE multiply-accumulate per instruction (e.g. 16×16×16)
   TMA                (Tensor Memory Accelerator) moves whole TILES async between HBM and SRAM
   warp specialization different warps run producer (load) vs consumer (compute) pipelines
   async pipelines    overlap tile-load and tile-compute across many stages
```


面对这样的硬件，标量线程是*错误的抽象*。工作的自然单位不再是"一个线程算什么"——而是**"这个 tile 上发生什么"**：加载一个 tile、对它做矩阵乘累加、存储一个 tile。把这件事写成数千个协同的标量线程，是编译器本该处理的易错样板。于是整个领域独立而明确地收敛到 **tile**，把它作为编程原语。

```text
   tile abstraction:  you describe TILES + a GRID of them.
                      the compiler handles thread partitioning, shared memory,
                      data movement (TMA), and (often) the pipeline.

   lineage:  CUDA C++ (2007) → Triton (2019 paper) → CuTe (2023)
                → ThunderKittens (2024) → TileLang (Jan 2025) → cuTile (2025)
```


这是当下 GPU 编程中最重要的单一趋势，也正是*为什么*突然出现了五种相互竞争的 kernel 语言：它们全都在争相主导 tile。

---

## 2. 组织一切的谱系

不要把五个工具当作一张平铺的清单来死记。把它们放到**一个轴——易用/生产力 vs. 控制/峰值性能**——上，整个图景就立刻清晰了。

```text
  EASE / PRODUCTIVITY  ◄──────────────────────────────────────────►  CONTROL / PEAK PERF
  ┌──────────┬───────────────────────────┬───────────────────────────────┬─────────────────┐
  │ TensorRT │ cuTile · Triton · Pallas   │ TileLang · CuTe DSL · TKittens │ CUTLASS C++/CUDA│
  │ (closed, │ (compiler decides thread   │ (you annotate scheduling &      │ (you write every│
  │  builds  │  & memory layout for you)  │  layout; near hand-tuned perf)  │  thread/barrier)│
  │ engine)  │                            │                                 │                 │
  └──────────┴───────────────────────────┴───────────────────────────────┴─────────────────┘
        ▲
        └── and ALONGSIDE the DSLs: autoscheduling compilers (TVM, IREE) that SEARCH the
            schedule space instead of asking you to choose — covered in Lecture 03.
```


这笔取舍始终相同：编译器替你决定的越多，你交付得越快、可移植性越好——但你也越贴近别人设定的性能天花板。你控制的越多，就越接近峰值——代价是工作量和目标锁定。资深工程师会*针对每个 kernel 在这个轴上选一个点*，而不是一辈子只用一种工具。95% 情况下的那个 kernel 用 Triton；而那个主导你成本预算的 attention kernel，可能用 ThunderKittens 或 CuTe DSL。

---


<details>
<summary>English original</summary>

**Lecture 02 - The Kernel-Language Explosion: Tiles as the New ISA**

**Collection:** [MLSys Deep Dives](README.md) | **Previous:** [← Lecture 01](Lecture-01.md) | **Next:** [Lecture 03](Lecture-03.md)

---

Lecture 1 said every token's cost is set by tokens-per-second, and tokens-per-second starts at the kernel. So: how does a fast GPU kernel actually get *written* in 2026? The answer changed more in the last three years than in the prior decade, and it changed in one direction — **away from scalar threads and toward tiles**.

This lecture is the map of that change: why the tile became the unit of GPU programming, and a working tour of the languages fighting to own it — **Triton, CUTLASS/CuTe/cuTile, ThunderKittens, TileLang** — arranged on the one axis that actually distinguishes them: how much control they hand you, versus how much they decide for you.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Explain why the scalar SIMT (thread-centric) model broke down, and why the **tile** is the natural unit on Tensor-Core hardware.
2. Place the major kernel tools on the **ease ↔ control** spectrum and name what each decides for you vs. exposes.
3. Write and read a **Triton** tile kernel, and explain its role under `torch.compile`/Inductor.
4. Distinguish NVIDIA's three on-ramps — **CUTLASS C++ + CuTe**, **CuTe DSL**, **cuTile** — and what "Tile IR" is.
5. Say what **ThunderKittens** and **TileLang** each optimize for, and when you'd reach for them over Triton.
6. Connect a kernel choice back to tokens/s and `$/Mtok` from Lecture 1.

---

**1. Why the thread model broke**

For a decade, CUDA's mental model was the **scalar thread**: you wrote what one thread does, launched millions, and reasoned about warps, shared memory, and synchronization by hand. That model matched the hardware — lots of simple scalar lanes.

Then the hardware stopped being scalar lanes. Modern accelerators are dominated by:

```text
   Tensor Cores       do a whole MATRIX-TILE multiply-accumulate per instruction (e.g. 16×16×16)
   TMA                (Tensor Memory Accelerator) moves whole TILES async between HBM and SRAM
   warp specialization different warps run producer (load) vs consumer (compute) pipelines
   async pipelines    overlap tile-load and tile-compute across many stages
```

Against that hardware, the scalar thread is the *wrong abstraction*. The natural unit of work is no longer "what one thread computes" — it is **"what happens to this tile"**: load a tile, matmul-accumulate it, store a tile. Writing that as thousands of coordinated scalar threads is error-prone boilerplate the compiler should handle. So the field converged, independently and explicitly, on the **tile** as the programming primitive.

```text
   tile abstraction:  you describe TILES + a GRID of them.
                      the compiler handles thread partitioning, shared memory,
                      data movement (TMA), and (often) the pipeline.

   lineage:  CUDA C++ (2007) → Triton (2019 paper) → CuTe (2023)
                → ThunderKittens (2024) → TileLang (Jan 2025) → cuTile (2025)
```

This is the single most important trend in GPU programming right now, and it is *why* there are suddenly five competing kernel languages: they are all racing to own the tile.

---

**2. The spectrum that organizes everything**

Do not memorize five tools as a flat list. Arrange them on **one axis — ease/productivity vs. control/peak-performance** — and the whole landscape snaps into place.

```text
  EASE / PRODUCTIVITY  ◄──────────────────────────────────────────►  CONTROL / PEAK PERF
  ┌──────────┬───────────────────────────┬───────────────────────────────┬─────────────────┐
  │ TensorRT │ cuTile · Triton · Pallas   │ TileLang · CuTe DSL · TKittens │ CUTLASS C++/CUDA│
  │ (closed, │ (compiler decides thread   │ (you annotate scheduling &      │ (you write every│
  │  builds  │  & memory layout for you)  │  layout; near hand-tuned perf)  │  thread/barrier)│
  │ engine)  │                            │                                 │                 │
  └──────────┴───────────────────────────┴───────────────────────────────┴─────────────────┘
        ▲
        └── and ALONGSIDE the DSLs: autoscheduling compilers (TVM, IREE) that SEARCH the
            schedule space instead of asking you to choose — covered in Lecture 03.
```

The trade is always the same: the more the compiler decides, the faster you ship and the more portable you are — but the closer you sit to a performance ceiling someone else set. The more you control, the closer to peak you can get — at the cost of effort and target lock-in. A senior engineer picks a *point on this axis per kernel*, not one tool for life. The 95%-of-cases kernel goes in Triton; the one attention kernel that dominates your cost budget might go in ThunderKittens or CuTe DSL.

---

</details>

## 3. Triton —— 心智份额领先者

**Triton**（源于 Philippe Tillet 2019 年的 MAPL 论文；2021 年起由 OpenAI 托管并公开；2.0 起基于 MLIR）就是大多数人说出「我写了个 kernel」时所指的那种分块语言。你写一个用 `@triton.jit` 装饰的 Python 函数，作用于 **block 级分块**；编译器负责合并访存、共享内存和 block 内调度。

```python
import triton
import triton.language as tl

@triton.jit
def fused_add_relu(x_ptr, y_ptr, out_ptr, n, BLOCK: tl.constexpr):
    pid  = tl.program_id(0)                    # which tile this program instance owns
    offs = pid * BLOCK + tl.arange(0, BLOCK)   # the TILE of indices, not one element
    mask = offs < n
    x = tl.load(x_ptr + offs, mask=mask)       # load a whole tile
    y = tl.load(y_ptr + offs, mask=mask)
    out = tl.maximum(x + y, 0.0)               # fused add + ReLU, on the tile
    tl.store(out_ptr + offs, out, mask=mask)
```

注意你*没有*写的东西：没有线程索引，没有 `__shared__`，没有 `__syncthreads()`。你描述了一个分块；Triton 把它映射到线程上。

那个逐元素 kernel 展示了这套模型；真正赚钱的算子是 matmul，它直接体现了分块论点 —— 循环结构*就是* §1 里的分块示意图：

```python
@triton.jit
def matmul(a_ptr, b_ptr, c_ptr, M, N, K,
           s_am, s_ak, s_bk, s_bn, s_cm, s_cn,
           BM: tl.constexpr, BN: tl.constexpr, BK: tl.constexpr):
    pid_m, pid_n = tl.program_id(0), tl.program_id(1)   # this instance owns tile (pid_m, pid_n) of C
    rm = pid_m * BM + tl.arange(0, BM)                  # the BM rows of C this tile covers
    rn = pid_n * BN + tl.arange(0, BN)                  # the BN cols
    acc = tl.zeros((BM, BN), dtype=tl.float32)
    for k in range(0, K, BK):                            # march down K, one BK-wide tile at a time
        rk = k + tl.arange(0, BK)
        a = tl.load(a_ptr + rm[:, None] * s_am + rk[None, :] * s_ak)   # (BM, BK) tile of A
        b = tl.load(b_ptr + rk[:, None] * s_bk + rn[None, :] * s_bn)   # (BK, BN) tile of B
        acc += tl.dot(a, b)                              # → Tensor Cores. the tile IS the instruction.
    tl.store(c_ptr + rm[:, None] * s_cm + rn[None, :] * s_cn, acc)
    # (boundary masks omitted for clarity — production kernels mask the edge tiles)
```

读懂这里的分工：**你**选分块形状（`BM × BN × BK`）和 dataflow（沿 K 累加）；**Triton** 选线程布局，把分块经由共享内存做暂存，并对循环做软件流水线（深度 `num_stages`），使分块 *k+1* 在分块 *k* 相乘时就开始加载。这条流水线正是 §1 里的 TMA/warp 特化模式 —— 只不过你从来不必自己写。`@triton.autotune` 随后针对每个 shape 扫描 `BM/BN/BK`、`num_warps` 和 `num_stages` 来挑选配置。

为什么 Triton 能领先心智份额，具体来说：

* **它是 `torch.compile` 的代码生成目标。** PyTorch 的 TorchInductor（默认后端）为 GPU **生成 Triton**。每当有人运行 `torch.compile`，底下产出的都是 Triton kernel。这使它成为主流 PyTorch 事实上的 kernel IR。
* **它是真正跨厂商的。** NVIDIA、AMD（ROCm）、Intel（XPU），外加一个实验性的 CPU 后端 —— 一份 kernel，多个目标。NVIDIA 甚至发布了 **Triton 的 Tile-IR 后端**，使其可下降到 NVIDIA 新的 tile 虚拟 ISA（下一节详述）。

坦诚的批评（Chris Lattner / Modular 大声提出）：Triton 是*编译器说了算*的模型，独立测量显示，在最难的 kernel 上它与手工优化的 CUDA 存在可观差距 —— 量级约为 **H100 上的 ~20%** —— 并且跨 GPU 代际的可移植性较弱，对最新特性（FP8、TMA）的支持也要等编译器跟上。另一些研究则宽容些（跨平台达到 cuBLAS 的 62–101%，且零架构特定调优）。真相介于两者之间，而这正是 §5–6 中控制端工具存在的原因。

---


<details>
<summary>English original</summary>

**3. Triton — the mindshare leader**

**Triton** (born as Philippe Tillet's 2019 MAPL paper; OpenAI-stewarded and public since 2021; MLIR-based since 2.0) is the tile language most people mean when they say "I wrote a kernel." You write a Python function decorated with `@triton.jit` that operates on **block-level tiles**; the compiler handles coalescing, shared memory, and intra-block scheduling.

```python
import triton
import triton.language as tl

@triton.jit
def fused_add_relu(x_ptr, y_ptr, out_ptr, n, BLOCK: tl.constexpr):
    pid  = tl.program_id(0)                    # which tile this program instance owns
    offs = pid * BLOCK + tl.arange(0, BLOCK)   # the TILE of indices, not one element
    mask = offs < n
    x = tl.load(x_ptr + offs, mask=mask)       # load a whole tile
    y = tl.load(y_ptr + offs, mask=mask)
    out = tl.maximum(x + y, 0.0)               # fused add + ReLU, on the tile
    tl.store(out_ptr + offs, out, mask=mask)
```

Note what you did *not* write: no thread indices, no `__shared__`, no `__syncthreads()`. You described a tile; Triton mapped it to threads.

That elementwise kernel shows the model; the op that pays the bills is matmul, and it shows the tile thesis directly — the loop structure *is* the tiling diagram from §1:

```python
@triton.jit
def matmul(a_ptr, b_ptr, c_ptr, M, N, K,
           s_am, s_ak, s_bk, s_bn, s_cm, s_cn,
           BM: tl.constexpr, BN: tl.constexpr, BK: tl.constexpr):
    pid_m, pid_n = tl.program_id(0), tl.program_id(1)   # this instance owns tile (pid_m, pid_n) of C
    rm = pid_m * BM + tl.arange(0, BM)                  # the BM rows of C this tile covers
    rn = pid_n * BN + tl.arange(0, BN)                  # the BN cols
    acc = tl.zeros((BM, BN), dtype=tl.float32)
    for k in range(0, K, BK):                            # march down K, one BK-wide tile at a time
        rk = k + tl.arange(0, BK)
        a = tl.load(a_ptr + rm[:, None] * s_am + rk[None, :] * s_ak)   # (BM, BK) tile of A
        b = tl.load(b_ptr + rk[:, None] * s_bk + rn[None, :] * s_bn)   # (BK, BN) tile of B
        acc += tl.dot(a, b)                              # → Tensor Cores. the tile IS the instruction.
    tl.store(c_ptr + rm[:, None] * s_cm + rn[None, :] * s_cn, acc)
    # (boundary masks omitted for clarity — production kernels mask the edge tiles)
```

Read the division of labor: **you** chose the tile shape (`BM × BN × BK`) and the dataflow (accumulate over K); **Triton** chose the thread layout, staged the tiles through shared memory, and software-pipelined the loop (`num_stages` deep) so tile *k+1* loads while tile *k* multiplies. That pipeline is exactly the TMA/warp-specialization pattern from §1 — you just never had to write it. `@triton.autotune` then sweeps `BM/BN/BK`, `num_warps`, and `num_stages` per shape to pick the config.

Why Triton leads mindshare, concretely:

* **It is the codegen target of `torch.compile`.** PyTorch's TorchInductor (the default backend) **generates Triton** for GPU. Every time someone runs `torch.compile`, Triton kernels are produced underneath. That makes it the de-facto kernel IR of mainstream PyTorch.
* **It is genuinely cross-vendor.** NVIDIA, AMD (ROCm), Intel (XPU), and an experimental CPU backend — one kernel, many targets. NVIDIA even shipped a **Tile-IR backend for Triton** so it can lower to NVIDIA's new tile virtual-ISA (more on that next).

The honest critique (made loudly by Chris Lattner / Modular): Triton is a *compiler-decides* model, and independent measurements have shown a meaningful gap — on the order of **~20% on H100** — versus hand-optimized CUDA for the hardest kernels, with weaker portability across GPU generations and lagging support for the newest features (FP8, TMA) until the compiler catches up. Other studies are kinder (62–101% of cuBLAS across platforms with zero arch-specific tuning). The truth is in between, and it is exactly why the control-end tools in §5–6 exist.

---

</details>

## 4. NVIDIA 的答案：CUTLASS、CuTe、CuTe DSL 与 cuTile

NVIDIA 对“所有人都在非 CUDA 里写分块”的回应，是提供*官方*的分块入口 —— 而且令人困惑地有好几个，你必须分清楚。

* **CUTLASS** —— 长期存在的开源 C++ 模板库，用于在 Tensor Core 上打到 GEMM/attention 的峰值性能。NVIDIA 自己就是用它写快 kernel 的。极致的控制力，极致的投入。
* **CuTe** —— CUTLASS 3.x 核心的 **layout 代数**：可组合的 `Layout`/`Tensor`/"atom" 对象，以 shape + 步长描述线程与数据的层级。它是较新的 DSL 复用的形式化词汇。听到 “CuTe” 时，想到的是*谁拥有哪个元素的数学*。
* **CuTe DSL**（CUTLASS 4.0 新增，GTC 2025）—— NVIDIA 首个 **Python** kernel DSL，**底层**，且与 CuTe C++ 完全一致。你在 Python 里就能获得完整的线程/数据/layout 控制，经 MLIR + ptxas JIT 编译，号称具备 **与 C++ 同等的性能** 和 **比 C++ 模板快约 100× 的编译速度**。这是*控制的那一端*，用 Python 实现。
* **cuTile**（又名 CUDA Tile，GTC 2025，随 CUDA 13.1 发布）—— 一个**独立的、更高层的** Python 分块 DSL。你写分块 kernel；编译器把 block 并行、访存搬运和线程划分抽象掉。这是*生产力那一端* —— 而且被普遍解读为**对 Triton 的直接回应**。（一位前 CUDA 架构师：“很难不怀疑 cuTile 就是为了对抗 Triton 而开发的。”）

cuTile 之下是 **Tile IR** —— 一套新的**面向分块编程的虚拟 ISA**，实质上就是“分块界的 PTX”。关键在于，NVIDIA *也为 Triton* 建了 Tile-IR 后端，因此 Triton 可以经它做 lower。战略解读是：NVIDIA 正试图**把分块抽象收回到 CUDA 平台之内**，同时提供控制侧入口（CuTe DSL）与生产力侧入口（cuTile），两者在 Blackwell 上都是一等公民。

问题也很明显：这一切都是 **NVIDIA 专属**。你用 Triton 的可移植性，换来更深的硬件集成，以及在 CuTe DSL 那一端接近 C++ 的峰值性能。

---

## 5. ThunderKittens —— 最小的分块，最大的 attention

**ThunderKittens**（Stanford Hazy Research，2024）押的是另一注：留在 **CUDA C++ 之内**，做成一个小型嵌入式 DSL（一个 header/模板库），并追问*分块抽象能小到什么程度，还依然能打到 SOTA？*

它的原语就是一个**尺寸对齐 Tensor Core 的分块**。该库把重复的管线工作抽象掉 —— 分块 layout、共享内存分配、寄存器 fragment、TMA tensor map、Tensor Core descriptor —— 同时让你仍然贴近硬件，可以自己推理数据搬运与调度。

```text
   ThunderKittens mental model:
   ┌──────────────────────────────────────────────────────────┐
   │  declare register/shared TILES (16×16-ish, TC-shaped)      │
   │  TMA-load input tiles  →  mma(acc, a_tile, b_tile)  →  store│
   │  you still schedule the pipeline; TK removes the boilerplate│
   └──────────────────────────────────────────────────────────┘
```


它以**快速 attention** 著称 —— 最初的动机是“FlashAttention 要 ~1200 行；这个能压缩到多紧凑？”—— 并且被 **Together AI、Jump Trading 和 Cursor** 用于生产环境。**ThunderKittens 2.0**（2026 年初）带来了完整的 Blackwell 支持与低精度 **MXFP8 / NVFP4**。甚至还有 Metal/MLX 移植版（“ThunderMittens”）。

什么时候用它：当有*某一个* kernel 主导你的成本（通常是 attention），你既想要接近手工调优的性能和完整的调度控制，又不愿意写 1200 行裸 CUTLASS。它以研究为主导，生态比 Triton 小 —— 是一把手术刀，而不是默认选项。

---


<details>
<summary>English original</summary>

**4. NVIDIA's answer: CUTLASS, CuTe, CuTe DSL, and cuTile**

NVIDIA's response to "everyone is writing tiles in not-CUDA" was to provide *official* tile on-ramps — and there are confusingly several, which you must keep straight.

* **CUTLASS** — the long-standing open C++ template library for peak GEMM/attention on Tensor Cores. This is how NVIDIA itself writes fast kernels. Max control, max effort.
* **CuTe** — the **layout algebra** at the heart of CUTLASS 3.x: composable `Layout`/`Tensor`/"atom" objects describing the thread-and-data hierarchy as shapes + strides. It is the formal vocabulary the newer DSLs reuse. When you hear "CuTe," think *the math of who-owns-which-element*.
* **CuTe DSL** (new with CUTLASS 4.0, GTC 2025) — NVIDIA's first **Python** kernel DSL, **low-level** and fully consistent with CuTe C++. You get full thread/data/layout control from Python, JIT-compiled through MLIR + ptxas, with claimed **C++-parity performance** and **~100× faster compilation** than C++ templates. This is the *control end*, in Python.
* **cuTile** (a.k.a. CUDA Tile, GTC 2025, shipping with CUDA 13.1) — a **separate, higher-level** Python tile DSL. You write tile kernels; the compiler abstracts block parallelism, memory movement, and thread partitioning. This is the *productivity end* — and it is widely read as a **direct response to Triton**. (One ex-CUDA architect: "it's hard not to suspect cuTile was developed directly to counter Triton.")

Underneath cuTile is **Tile IR** — a new **virtual ISA for tile programming**, effectively "PTX for tiles." Crucially, NVIDIA built a Tile-IR backend *for Triton too*, so Triton can lower through it. The strategic read: NVIDIA is trying to **reclaim the tile abstraction inside the CUDA platform**, offering both a control on-ramp (CuTe DSL) and a productivity on-ramp (cuTile), both first-class on Blackwell.

The catch is the obvious one: all of it is **NVIDIA-only**. You trade Triton's portability for deeper hardware integration and (at the CuTe DSL end) near-C++ peak.

---

**5. ThunderKittens — minimal tiles, maximal attention**

**ThunderKittens** (Stanford Hazy Research, 2024) takes a different bet: stay **inside CUDA C++** as a small embedded DSL (a header/template library), and ask *how small can the tile abstraction be and still hit SOTA?*

Its primitive is literally a **tile sized to the Tensor Core**. The library abstracts the repetitive plumbing — tile layouts, shared-memory allocation, register fragments, TMA tensor maps, Tensor Core descriptors — while keeping you close enough to reason about data movement and scheduling yourself.

```text
   ThunderKittens mental model:
   ┌──────────────────────────────────────────────────────────┐
   │  declare register/shared TILES (16×16-ish, TC-shaped)      │
   │  TMA-load input tiles  →  mma(acc, a_tile, b_tile)  →  store│
   │  you still schedule the pipeline; TK removes the boilerplate│
   └──────────────────────────────────────────────────────────┘
```

It is **known for fast attention** — its origin motivation was "FlashAttention is ~1200 lines; how compact can this be?" — and it is used in production by **Together AI, Jump Trading, and Cursor**. **ThunderKittens 2.0** (early 2026) brought full Blackwell support and low-precision **MXFP8 / NVFP4**. There is even a Metal/MLX port ("ThunderMittens").

When to reach for it: the *one* kernel that dominates your cost (usually attention), where you want near-hand-tuned performance and full scheduling control but refuse to write 1200 lines of raw CUTLASS. It is research-led with a smaller ecosystem than Triton — a scalpel, not a default.

---

</details>

## 6. TileLang — 将 schedule 与 dataflow 解耦

**TileLang**（2025 年 1 月开源；出自 `tile-ai` 团队，与 Microsoft Research 和 Peking University 合作；构建**在 Apache TVM 的 TIR 基础设施之上**）把 control 与 productivity 之间的取舍在一个语言内部变得*显式且可调*。

其核心思想：**把 dataflow（计算什么）与调度空间（线程绑定、内存布局、`tensorize`、software pipelining）分离，并把调度暴露为可覆写的注解**——未指定之处由自动布局推断补齐。

```text
   Triton:    you write dataflow; the compiler picks ALL scheduling.        (less control)
   CUTLASS:   you write dataflow AND every scheduling/layout detail.         (all control, all effort)
   TileLang:  you write dataflow; you OVERRIDE the scheduling you care        (control where it pays,
              about and let inference handle the rest.                         automation where it doesn't)
```

这使它在定位上刻意居于 Triton（生产力）与 CUTLASS（控制）之间，并面向广泛的后端集合——**CUDA、ROCm/HIP、Metal、WebGPU 和 CPU**——2025 年末又加入了 CuTe DSL 后端。它属于同一团队更大技术栈的一部分：**TileLang**（编写 kernel）+ **TileScale** + **TileRT** runtime（你会在 Lecture 3 见到它，Lecture 6 还会再见，作为某个显著吞吐里程碑背后的引擎）。

何时该用它：需要**比 Triton 更强的调度控制、比 CUTLASS 更好的可移植性**，例如把同一个手工调优的 kernel 同时发布到 NVIDIA*和*AMD*和*Apple。代价是生态更小，依赖（基于 TVM）比 Triton 更重。

*(荣誉提名：**JAX Pallas**——同样的 tile + `grid` + `BlockSpec` 思路，JAX 原生，在 GPU 上 lower 到 Triton、在 TPU 上 lower 到 Mosaic。如果你身处 JAX/TPU 阵营，Pallas 就是你的 tile DSL。)*

---

## 7. 选择——一张资深工程师的表

| 工具 | 归属方 | 模型 | 所在位置 | 目标平台 | 何时该用它 |
|---|---|---|---|---|---|
| **Triton** | OpenAI | block/tile、`@jit` Python | 在 `torch.compile` 之下 | NV / AMD / Intel / CPU | 默认之选——可移植、生产力高，覆盖 95% 的 kernel |
| **cuTile** | NVIDIA | Python tile、Tile IR | CUDA 13.1 | NVIDIA | 仅限 NVIDIA，想要 Triton 般的易用性 + 深度 CUDA 集成 |
| **CuTe DSL** | NVIDIA | Python、与 CuTe 一致 | CUTLASS 4.x | NVIDIA | 想从 Python 拿到与 C++ 同级的峰值，快速迭代 |
| **CUTLASS C++** | NVIDIA | C++ 模板 + CuTe | 手写 | NVIDIA | 构建库级 kernel，需要绝对峰值 |
| **ThunderKittens** | Stanford | tile = TC 形状，写在 CUDA 里 | C++ header lib | NV（Blackwell）、Metal | 成本占主导的那个 attention kernel |
| **TileLang** | tile-ai / MSR / PKU | 分块，schedule⊥dataflow | 基于 TVM | NV / AMD / Metal / WebGPU / CPU | 既手工调优*又*跨厂商可移植 |
| **Pallas** | Google | tile + `BlockSpec` | JAX | GPU（Triton）/ TPU（Mosaic）| 你在用 JAX / 跑在 TPU 上 |

还有一个元层面的观点，它存在争议，两边都值得了解：围绕 tile 的收敛**并非**统一整合。NVIDIA 的 cuTile/Tile-IR 试图把这一抽象拉回 CUDA；Modular 的 Lattner 则认为结果是*碎片化*——「1,001 种用 Python 写 CUDA kernel 的方式」，那些「看起来像 Python 其实不是」的 Python eDSL——这正是为把 **Mojo 作为一门真正的语言**、而非 eDSL 所做的宣传（下一讲）。两种解读都站得住脚。作为工程师，你的工作不是挑出胜出的意识形态，而是把每个 kernel 放在 §2 坐标轴的合适位置上。

---

## 8. 度量它——把 kernel 拴回 token

kernel 能跑起来不算完成；知道它对 tokens/s 的影响才算完成。循环如下：

```text
   write tile kernel  →  benchmark GFLOP/s and % of roofline peak
                      →  swap it into the model's hot path
                      →  measure end-to-end tokens/s delta
                      →  recompute $/Mtok  (Lecture 1, §6)
```

快 1.5× 的 attention kernel 有意思；而一个快 1.5×、把端到端 decode（逐 token 生成阶段）的 tokens/s 提升 1.2×、并把 `$/Mtok` 从 $0.56 to $0.47 降下来的 attention kernel，才是**可交付、经得起辩护的工作**。永远把数字带到最后一步。kernel 是手段；token 才是目的。

---


<details>
<summary>English original</summary>

**6. TileLang — decoupling schedule from dataflow**

**TileLang** (open-sourced Jan 2025; from the `tile-ai` group with Microsoft Research and Peking University; built **on Apache TVM's TIR infrastructure**) makes the control-vs-productivity trade *explicit and adjustable* inside one language.

Its core idea: **separate the dataflow (what is computed) from the scheduling space (thread binding, memory layout, `tensorize`, software pipelining), and expose the scheduling as overridable annotations** — with automatic layout inference filling in what you don't specify.

```text
   Triton:    you write dataflow; the compiler picks ALL scheduling.        (less control)
   CUTLASS:   you write dataflow AND every scheduling/layout detail.         (all control, all effort)
   TileLang:  you write dataflow; you OVERRIDE the scheduling you care        (control where it pays,
              about and let inference handle the rest.                         automation where it doesn't)
```

That positions it deliberately between Triton (productivity) and CUTLASS (control), and it targets a wide backend set — **CUDA, ROCm/HIP, Metal, WebGPU, and CPU** — with a CuTe DSL backend added late 2025. It is part of a broader stack from the same group: **TileLang** (author kernels) + **TileScale** + the **TileRT** runtime (which you'll meet in Lecture 3 and again in Lecture 6 as the engine behind a notable throughput milestone).

When to reach for it: you need **more scheduling control than Triton and more portability than CUTLASS**, e.g. shipping the same hand-tuned kernel across NVIDIA *and* AMD *and* Apple. The cost is a smaller ecosystem and a heavier (TVM-based) dependency than Triton.

*(Honorable mention: **JAX Pallas** — the same tile + `grid` + `BlockSpec` idea, JAX-native, lowering to Triton on GPU and Mosaic on TPU. If you live in JAX/TPU land, Pallas is your tile DSL.)*

---

**7. Choosing — a senior engineer's table**

| Tool | Owner | Model | Lives where | Targets | Reach for it when |
|---|---|---|---|---|---|
| **Triton** | OpenAI | block/tile, `@jit` Python | under `torch.compile` | NV / AMD / Intel / CPU | default — portable, productive, 95% of kernels |
| **cuTile** | NVIDIA | Python tile, Tile IR | CUDA 13.1 | NVIDIA | NVIDIA-only, want Triton-like ease + deep CUDA integration |
| **CuTe DSL** | NVIDIA | Python, CuTe-consistent | CUTLASS 4.x | NVIDIA | want C++-parity peak from Python, fast iterate |
| **CUTLASS C++** | NVIDIA | C++ templates + CuTe | hand-written | NVIDIA | building a library kernel, need absolute peak |
| **ThunderKittens** | Stanford | tile = TC-shaped, in CUDA | C++ header lib | NV (Blackwell), Metal | the one attention kernel that dominates cost |
| **TileLang** | tile-ai / MSR / PKU | tiled, schedule⊥dataflow | on TVM | NV / AMD / Metal / WebGPU / CPU | hand-tuned *and* multi-vendor portable |
| **Pallas** | Google | tile + `BlockSpec` | JAX | GPU (Triton) / TPU (Mosaic) | you're in JAX / on TPU |

And the meta-point, which is contested and worth knowing both sides of: the convergence on tiles is **not** consolidation. NVIDIA's cuTile/Tile-IR is an attempt to pull the abstraction back into CUDA; Modular's Lattner argues the result is *fragmentation* — "1,001 ways to write CUDA kernels in Python," Python-eDSLs that "look like Python but aren't" — which is the pitch for **Mojo as a real language** instead of an eDSL (next lecture). Both readings are defensible. As an engineer, your job is not to pick the winning ideology; it is to put each kernel at the right point on the §2 axis.

---

**8. Measure it — tie the kernel back to the token**

A kernel is not done when it runs; it is done when you know its effect on tokens/s. The loop:

```text
   write tile kernel  →  benchmark GFLOP/s and % of roofline peak
                      →  swap it into the model's hot path
                      →  measure end-to-end tokens/s delta
                      →  recompute $/Mtok  (Lecture 1, §6)
```

A 1.5× faster attention kernel is interesting; a 1.5× faster attention kernel that lifts end-to-end decode tokens/s by 1.2× and drops `$/Mtok` from $0.56 to $0.47 is **shippable, defensible work**. Always carry the number to the last step. The kernel is the means; the token is the end.

---

</details>

## 9. Mini-lab：一个 kernel，轴上的三个点

选一个关键的单算子（fused `add+RMSNorm`，或一个小矩阵乘）。

1. **生产力点：** 用 **Triton** 配合 `@triton.autotune` 编写它。记录 GFLOP/s 和 roofline（性能上界模型）占比。
2. **控制点：** 如果有相应硬件，用 **CuTe DSL** 或 **ThunderKittens**（或手写 CUDA）重新实现。记录同样的指标。
3. **编译器点（第 3 讲预览）：** 让 `torch.compile` 为同一算子生成一个 Triton kernel 并比较。
4. **连接：** 把最佳 kernel 放进模型的热路径，并测量相对于框架默认实现的 **端到端 tokens/s 与 `$/Mtok`** 差值。

交付物：一张 `{Triton, control-tool, torch.compile}` × `{GFLOP/s, % peak, end-to-end tokens/s, $/Mtok}` 的表，外加一段文字，说明每个点位于易用性↔控制轴的哪个位置，以及控制是否值得付出努力。最后这个判断——*控制是否值得*——正是本讲的全部技能。

---

## 关键要点

- **标量线程模型崩溃了**，因为 Tensor Core、TMA 和 warp 特化流水线让 **分块** 成为自然的工作单元。所有现代 kernel 语言都在竞相掌控分块。
- 把工具组织在 **一个轴上：易用性/生产力 ↔ 控制/峰值**。为每个 kernel 选一个点，而不是一辈子只用一个工具。
- **Triton** 占据主流心智：Python 中的分块，`torch.compile` 的代码生成目标，真正跨厂商——与手工调优的 CUDA 相比，在最难的 kernel 上约有 20% 的差距。
- **NVIDIA 的入口路径**：CUTLASS C++/CuTe（峰值）、**CuTe DSL**（从 Python 获得 C++ 精度一致性）、**cuTile**（类 Triton 的生产力）——全部仅限 NVIDIA，构建在新的 **Tile IR**（“面向分块的 PTX”）之上。
- **ThunderKittens** = CUDA 中的最小分块，attention 手术刀（Together、Cursor 使用）。**TileLang** = 将调度与 dataflow 解耦，可手工调优 *又* 跨多厂商，构建在 TVM 上。
- 当测量了 kernel 对 **端到端 tokens/s 与 `$/Mtok`** 的影响时，它才算完成——而不是在“它能跑”时。

---

## 参考文献

- OpenAI Triton：[https://openai.com/index/triton/](https://openai.com/index/triton/) · repo [https://github.com/triton-lang/triton](https://github.com/triton-lang/triton)
- NVIDIA，“Achieve CUTLASS C++ performance with Python — CuTe DSL”：[https://developer.nvidia.com/blog/achieve-cutlass-c-performance-with-python-apis-using-cute-dsl/](https://developer.nvidia.com/blog/achieve-cutlass-c-performance-with-python-apis-using-cute-dsl/)
- NVIDIA，“Simplify GPU programming with CUDA Tile (cuTile) in Python”：[https://developer.nvidia.com/blog/simplify-gpu-programming-with-nvidia-cuda-tile-in-python/](https://developer.nvidia.com/blog/simplify-gpu-programming-with-nvidia-cuda-tile-in-python/)
- ThunderKittens（Stanford Hazy Research）：[https://github.com/HazyResearch/ThunderKittens](https://github.com/HazyResearch/ThunderKittens) · 博客 [https://hazyresearch.stanford.edu/blog/2024-05-12-tk](https://hazyresearch.stanford.edu/blog/2024-05-12-tk)
- TileLang：[https://github.com/tile-ai/tilelang](https://github.com/tile-ai/tilelang) · 论文 arXiv 2504.17577 [https://arxiv.org/abs/2504.17577](https://arxiv.org/abs/2504.17577)
- Modular，“Democratizing AI Compute, Part 7 — Triton and Python eDSLs”：[https://www.modular.com/blog/democratizing-ai-compute-part-7-what-about-triton-and-python-edsls](https://www.modular.com/blog/democratizing-ai-compute-part-7-what-about-triton-and-python-edsls)
- *TVM Deep Dives* — [第 02 讲——TensorIR 与调度空间](../06-TVM深度探索/Lecture-02.md)，关于 TileLang 暴露的调度原语。

---

## 更新至

2026-06。版本锁定：Triton 3.x（MLIR、Tile-IR 后端）、CUTLASS 4.x（CuTe DSL，GTC 2025）、cuTile / Tile IR 与 CUDA 13.1、ThunderKittens 2.0（Blackwell + MXFP8/NVFP4）、TileLang 于 2025 年 1 月在 TVM 上开源。约 20% 的 Triton 与 CUDA 差距是一个有争议、依赖 kernel 和代际的数字——仅作说明，在你的硬件上重新测量。

---

*下一篇：[第 03 讲——编译器与 runtime](Lecture-03.md)*


<details>
<summary>English original</summary>

**9. Mini-lab: one kernel, three points on the axis**

Pick a single op that matters (fused `add+RMSNorm`, or a small matmul).

1. **Productivity point:** write it in **Triton** with `@triton.autotune`. Record GFLOP/s and % of roofline.
2. **Control point:** if you have the hardware, reimplement in **CuTe DSL** or **ThunderKittens** (or hand-CUDA). Record the same.
3. **Compiler point (preview of Lec 3):** let `torch.compile` generate a Triton kernel for the same op and compare.
4. **Connect:** put the best kernel in a model's hot path and measure the **end-to-end tokens/s and `$/Mtok`** delta versus the framework default.

Deliverable: a table of `{Triton, control-tool, torch.compile}` × `{GFLOP/s, % peak, end-to-end tokens/s, $/Mtok}`, plus one paragraph on where each sat on the ease↔control axis and whether the control was worth the effort. That last judgment — *was the control worth it* — is the entire skill of this lecture.

---

**Key takeaways**

- The **scalar thread model broke** because Tensor Cores, TMA, and warp-specialized pipelines made the **tile** the natural unit of work. Every modern kernel language is a race to own the tile.
- Organize the tools on **one axis: ease/productivity ↔ control/peak**. Pick a point per kernel, not a tool for life.
- **Triton** leads mindshare: tile-in-Python, the codegen target of `torch.compile`, genuinely cross-vendor — at a ~20%-on-the-hardest-kernels gap vs hand-tuned CUDA.
- **NVIDIA's on-ramps**: CUTLASS C++/CuTe (peak), **CuTe DSL** (C++-parity from Python), **cuTile** (Triton-like productivity) — all NVIDIA-only, over the new **Tile IR** ("PTX for tiles").
- **ThunderKittens** = minimal tiles in CUDA, the attention scalpel (used by Together, Cursor). **TileLang** = decouples scheduling from dataflow, hand-tuned *and* multi-vendor, built on TVM.
- A kernel is finished when you've measured its effect on **end-to-end tokens/s and `$/Mtok`** — not at "it runs."

---

**References**

- OpenAI Triton: [https://openai.com/index/triton/](https://openai.com/index/triton/) · repo [https://github.com/triton-lang/triton](https://github.com/triton-lang/triton)
- NVIDIA, "Achieve CUTLASS C++ performance with Python — CuTe DSL": [https://developer.nvidia.com/blog/achieve-cutlass-c-performance-with-python-apis-using-cute-dsl/](https://developer.nvidia.com/blog/achieve-cutlass-c-performance-with-python-apis-using-cute-dsl/)
- NVIDIA, "Simplify GPU programming with CUDA Tile (cuTile) in Python": [https://developer.nvidia.com/blog/simplify-gpu-programming-with-nvidia-cuda-tile-in-python/](https://developer.nvidia.com/blog/simplify-gpu-programming-with-nvidia-cuda-tile-in-python/)
- ThunderKittens (Stanford Hazy Research): [https://github.com/HazyResearch/ThunderKittens](https://github.com/HazyResearch/ThunderKittens) · blog [https://hazyresearch.stanford.edu/blog/2024-05-12-tk](https://hazyresearch.stanford.edu/blog/2024-05-12-tk)
- TileLang: [https://github.com/tile-ai/tilelang](https://github.com/tile-ai/tilelang) · paper arXiv 2504.17577 [https://arxiv.org/abs/2504.17577](https://arxiv.org/abs/2504.17577)
- Modular, "Democratizing AI Compute, Part 7 — Triton and Python eDSLs": [https://www.modular.com/blog/democratizing-ai-compute-part-7-what-about-triton-and-python-edsls](https://www.modular.com/blog/democratizing-ai-compute-part-7-what-about-triton-and-python-edsls)
- *TVM Deep Dives* — [Lecture 02 — TensorIR & the schedule space](../06-TVM深度探索/Lecture-02.md), for the scheduling primitives TileLang exposes.

---

**Current as of**

2026-06. Pins: Triton 3.x (MLIR, Tile-IR backend), CUTLASS 4.x (CuTe DSL, GTC 2025), cuTile / Tile IR with CUDA 13.1, ThunderKittens 2.0 (Blackwell + MXFP8/NVFP4), TileLang open-sourced Jan 2025 on TVM. The ~20% Triton-vs-CUDA gap is a contested, kernel-and-generation-dependent figure — treat as illustrative, re-measure on your hardware.

---

*Next: [Lecture 03 — Compilers & runtimes](Lecture-03.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/MLSys Deep Dives/Lecture-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/MLSys%20Deep%20Dives/Lecture-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
