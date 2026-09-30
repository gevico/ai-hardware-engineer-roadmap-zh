---
title: 第 02 讲 - TensorIR 与调度空间：把计算定义变成映射到硬件的 kernel
description: 第 02 讲 - TensorIR 与调度空间：把计算定义变成映射到硬件的 kernel
---

# 第 02 讲 - TensorIR 与调度空间：把计算定义变成映射到硬件的 kernel

**合集：** [TVM Deep Dives](README.md) | **上一篇：** [← 第 01 讲](Lecture-01.md) | **下一篇：** [第 03 讲](Lecture-03.md)

---

第 1 讲提过这个技术栈的核心不变量：**调度改变循环顺序和内存映射，但绝不改变结果。** 本讲要讲的就是这句话在实践中意味着什么。

计算定义 —— `C[i,j] = sum_k A[i,k] * B[k,j]` —— 只说明*要算什么*，对*怎么算*只字不提：哪个循环在最外层、哪些被分块以放进 L1、什么驻留在寄存器里、哪条轴绑定到哪个 GPU 线程、内层 kernel 是标量乘加还是 Tensor Core 指令。**调度**才是*怎么算*。TensorIR 把*怎么算*变成一等公民式的、可变换的程序，而所有合法「怎么算」构成的空间，就是**调度空间**——你将用整个职业生涯在其中穿行。

这是编译器的核心。掌握它，第 3 讲（让机器搜索这个空间）和第 5 讲（dlight 为 LLM 预置的调度）就只是把这里手工做的事自动化而已。

---

## 学习目标

学完本讲，你应当能够：

1. 从 Tensor Expression (TE) 生成 TIR `PrimFunc`，并读懂它的 block / iter-var 结构。
2. 解释 TIR **block**：空间轴与归约轴（`T.axis.remap("SSR", ...)`）、`init` 区域，以及读/写区域。
3. 使用 `tir.Schedule` API：`get_block`、`get_loops`、`split`、`reorder`、`bind`、`cache_read`、`cache_write`、`compute_at`、`vectorize`、`unroll`、`parallel`、`decompose_reduction`。
4. 为 **CPU** 手工调度一个 matmul（分块 → 向量化 → 并行），为 **GPU** 手工调度一个 matmul（block/thread 绑定 → 共享内存缓存 → 协作式读取）。
5. 把每个原语映射到它的 **roofline**（性能上界模型）效果——即它为何能把你推向（或推向更高的）性能上界。
6. `tensorize` 内层 block 到硬件 intrinsic（Tensor Core MMA），并测量前后的 GFLOP/s。

---

## 1. TE 与 TIR：抵达同一循环嵌套的两条路

得到 TIR `PrimFunc` 有两条路。你可以在 TVMScript 里**直接写出来**（如第 1 讲），也可以用 Tensor Expression **描述数学**，让 TVM 生成循环嵌套。TE 是平缓的入门坡道：你声明式地陈述计算，TVM 把循环落实出来。

```python
import tvm
from tvm import te, tir

def matmul_te(M, N, K, dtype="float32"):
    A = te.placeholder((M, K), name="A", dtype=dtype)
    B = te.placeholder((K, N), name="B", dtype=dtype)
    k = te.reduce_axis((0, K), name="k")              # the reduction axis
    C = te.compute(
        (M, N),
        lambda i, j: te.sum(A[i, k] * B[k, j], axis=k),
        name="C",
    )
    return te.create_prim_func([A, B, C])             # TE  →  TIR PrimFunc

PrimFunc = matmul_te(1024, 1024, 1024)
PrimFunc.show()
```

`te.create_prim_func` 是桥梁：输入声明式 TE，输出可调度的 TensorIR。它打印出来的，就是第 1 讲见过的那种 block 化循环嵌套——而*那*正是要调度的对象。从这往后，TE 的任务就算完成；接下来都在 TIR 上做。

> 为什么两者都要有？TE 便于表达标准算子；直接写 TIR（TVMScript）则用于需要精确控制的场合，也是 auto-tuning 操作的对象，更是 importer 生成的产物。资深工程师两种都会读会写，并把 TE 当作 TIR 的生成器——而非另一个独立世界。

---

## 2. TIR block 剖析

TensorIR 中一切可调度的东西都住在 **block** 里。block 是调度器推理的单位，它的标注是一份契约，保证每次变换都合法。

```python
for i, j, k in T.grid(1024, 1024, 1024):
    with T.block("C"):
        vi, vj, vk = T.axis.remap("SSR", [i, j, k])   # ← the contract
        with T.init():
            C[vi, vj] = T.float32(0)                  # reduction init
        C[vi, vj] = C[vi, vj] + A[vi, vk] * B[vk, vj]  # reduction update
```

读这份契约：

* **`T.axis.remap("SSR", [i, j, k])`** 声明了三个 block 迭代变量。字符串 `"SSR"` 为它们标注类型：`vi`、`vj` 是 **S**patial（相互独立的输出坐标——可安全并行化、随意重排），`vk` 是 **R**eduction（累加到同一个输出——*仅仅因为*标注为可结合，重排它在数值上才不改变任何东西）。
* **`T.init()`** 是归约的初始化区域：每个 `(vi, vj)` 都必须*在*任何 `k` 之前发生一次的 `C = 0`。把它单独标出来，调度器才能安全地提升或并行化归约。
* block 还带有**读/写区域**（它触及了 `A`、`B`、`C` 的哪些元素），由 body 推断得到。调度器据此知道哪些缓存、移动或融合是安全的。

这正是 TIR 能被激进变换、而裸 C 不能的原因：block *精确地*告诉编译器哪些是空间性的、哪些是归约性的、以及触及了哪些内存。调度原语不过是这套结构的合法重写。

---


<details>
<summary>English original</summary>

**Lecture 02 - TensorIR and the Schedule Space: Turning a Compute Definition into a Hardware-Mapped Kernel**

**Collection:** [TVM Deep Dives](README.md) | **Previous:** [← Lecture 01](Lecture-01.md) | **Next:** [Lecture 03](Lecture-03.md)

---

In Lecture 1 we said the central invariant of the stack: **scheduling changes loop order and memory mapping, never the result.** This lecture is what that sentence means in practice.

A compute definition — `C[i,j] = sum_k A[i,k] * B[k,j]` — is a statement of *what* to compute. It says nothing about *how*: which loop is outermost, what is tiled to fit in L1, what lives in registers, which axis is bound to which GPU thread, whether the inner kernel is a scalar multiply-add or a Tensor Core instruction. The **schedule** is the *how*. TensorIR makes the *how* a first-class, transformable program, and the space of all legal hows is the **schedule space** you will spend your career navigating.

This is the heart of the compiler. Get it, and Lecture 3 (letting a machine search this space) and Lecture 5 (dlight's pre-built schedules for LLMs) are just automation of what you do here by hand.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Generate a TIR `PrimFunc` from a Tensor Expression (TE), and read its block / iter-var structure.
2. Explain a TIR **block**: spatial vs reduction axes (`T.axis.remap("SSR", ...)`), the `init` region, and read/write regions.
3. Drive the `tir.Schedule` API: `get_block`, `get_loops`, `split`, `reorder`, `bind`, `cache_read`, `cache_write`, `compute_at`, `vectorize`, `unroll`, `parallel`, `decompose_reduction`.
4. Hand-schedule a matmul for a **CPU** (tile → vectorize → parallel) and a **GPU** (block/thread bind → shared-memory cache → cooperative fetch).
5. Map each primitive to its **roofline** effect — why it moves you toward (or to a higher) performance roof.
6. `tensorize` an inner block onto a hardware intrinsic (Tensor Core MMA), and measure GFLOP/s before and after.

---

**1. TE and TIR: two ways to reach the same loop nest**

You get a TIR `PrimFunc` two ways. You can **write it directly** in TVMScript (as in Lecture 1), or you can **describe the math** with a Tensor Expression and let TVM generate the loop nest. TE is the gentle on-ramp: you state the computation declaratively, TVM materializes the loops.

```python
import tvm
from tvm import te, tir

def matmul_te(M, N, K, dtype="float32"):
    A = te.placeholder((M, K), name="A", dtype=dtype)
    B = te.placeholder((K, N), name="B", dtype=dtype)
    k = te.reduce_axis((0, K), name="k")              # the reduction axis
    C = te.compute(
        (M, N),
        lambda i, j: te.sum(A[i, k] * B[k, j], axis=k),
        name="C",
    )
    return te.create_prim_func([A, B, C])             # TE  →  TIR PrimFunc

PrimFunc = matmul_te(1024, 1024, 1024)
PrimFunc.show()
```

`te.create_prim_func` is the bridge: declarative TE in, schedulable TensorIR out. What it prints is the same kind of block-structured loop nest you saw in Lecture 1 — and *that* is the thing we schedule. From here on, TE has done its job; we work on the TIR.

> Why have both? TE is convenient for expressing standard ops; direct TIR (TVMScript) is what you write when you need exact control, what auto-tuning manipulates, and what the importer emits. Senior engineers read and write both, and treat TE as a generator for TIR — never as a separate world.

---

**2. Anatomy of a TIR block**

Everything schedulable in TensorIR lives inside a **block**. The block is the unit the scheduler reasons about, and its annotations are a contract that keeps every transformation legal.

```python
for i, j, k in T.grid(1024, 1024, 1024):
    with T.block("C"):
        vi, vj, vk = T.axis.remap("SSR", [i, j, k])   # ← the contract
        with T.init():
            C[vi, vj] = T.float32(0)                  # reduction init
        C[vi, vj] = C[vi, vj] + A[vi, vk] * B[vk, vj]  # reduction update
```

Read the contract:

* **`T.axis.remap("SSR", [i, j, k])`** declares three block iteration variables. The string `"SSR"` types them: `vi`, `vj` are **S**patial (independent output coordinates — safe to parallelize, reorder freely), `vk` is a **R**eduction (accumulates into one output — reordering it changes nothing numerically *only because* it is marked associative).
* **`T.init()`** is the reduction's initialization region: the `C = 0` that must happen once per `(vi, vj)` *before* any `k`. Marking it separately is what lets the scheduler hoist or parallelize the reduction safely.
* The block also carries **read/write regions** (which elements of `A`, `B`, `C` it touches), inferred from the body. The scheduler uses these to know what is safe to cache, move, or fuse.

This is why TIR can be aggressively transformed where raw C cannot: the block tells the compiler *exactly* what is spatial, what is reduction, and what memory is touched. The schedule primitives are just legal rewrites of this structure.

---

</details>

## 3. 调度对象与原语

通过在该模块上构造一个 `tir.Schedule` 并发出原语来完成调度。每个原语都会就地改写 IR；`sch.mod.show()` 展示了结果。

```python
sch = tir.Schedule(PrimFunc)         # a mutable schedule over the TIR
block_C = sch.get_block("C")         # grab the block by name
i, j, k = sch.get_loops(block_C)     # its loops, outer→inner
```

核心原语，按其物理行为分组：

| 原语 | 调用 | 物理行为 |
|---|---|---|
| **split** | `i0, i1 = sch.split(i, [None, 32])` | 把一个循环切成两层的嵌套（分块） |
| **reorder** | `sch.reorder(i0, j0, i1, j1)` | 改变循环嵌套顺序 |
| **fuse** | `sch.fuse(i, j)` | 把两个循环合并为一个 |
| **bind** | `sch.bind(i0, "blockIdx.x")` | 把一个循环映射到 GPU 的 grid/block 轴上 |
| **cache_read** | `sch.cache_read(blk, 0, "shared")` | 把输入 buffer 暂存到更快的存储中 |
| **cache_write** | `sch.cache_write(blk, 0, "local")` | 在寄存器中累加输出，最后一次性写回 |
| **compute_at** | `sch.compute_at(prod, loop)` | 把生产者块移入消费者循环内（局部性） |
| **vectorize** | `sch.vectorize(i1)` | 在该循环上生成 SIMD |
| **unroll** | `sch.unroll(i1)` | 展开以提升 ILP / 减少循环开销 |
| **parallel** | `sch.parallel(i0)` | 把该循环分摊到多个 CPU 核上运行 |
| **decompose_reduction** | `sch.decompose_reduction(blk, k0)` | 把 `init` 拆到归约循环之上 |
| **tensorize** | `sch.tensorize(i1, INTRIN)` | 用硬件 intrinsic 替换内层区域 |

这些原语可以组合。真实的 schedule 是一段由二十到上百次此类调用构成的*程序*。其中的门道在于知道哪种调用序列能把该循环嵌套映射到目标机的存储层次和执行单元上——这恰恰是 roofline（性能上界模型）告诉你的事，也恰恰是第 3 讲所自动化的内容。

---

## 4. 实例演练：为 CPU 调度 matmul

从朴素循环嵌套出发，把它变换成 cache 分块、向量化、多线程的 kernel。朴素版本结果正确，却让机器 >90% 的能力闲置：它从 DRAM 流式读取 `B`，毫无复用，并且只用了一个核、一条 lane。

```python
sch = tir.Schedule(matmul_te(1024, 1024, 1024))
C = sch.get_block("C")
i, j, k = sch.get_loops(C)

# 1) Tile i and j so a block of C stays hot in cache; split k for an inner accumulate loop
i0, i1 = sch.split(i, factors=[None, 32])      # 32×32 output tile
j0, j1 = sch.split(j, factors=[None, 32])
k0, k1 = sch.split(k, factors=[None, 4])

# 2) Reorder: tiles outermost, reduction in the middle, the hot 32×32×4 micro-kernel innermost
sch.reorder(i0, j0, k0, i1, k1, j1)

# 3) Accumulate the output tile in registers, not back to DRAM every k-step
C_local = sch.cache_write(C, 0, "local")
sch.reverse_compute_at(C_local, j0)

# 4) SIMD over the contiguous j1, multi-thread over the outer tile
sch.vectorize(j1)
sch.parallel(i0)

sch.mod.show()                                  # read the transformed nest — this is the kernel
```

对照存储层次逐条走一遍意图：

* **split + reorder** 造出一个能装进 L1/寄存器的 `32×32` 输出分块，于是 `A` 和 `B` 的每个已加载元素都在该分块内被**复用**，而不是重新取一遍。这是最大的一根杠杆——它提升了**算术强度**。
* **cache_write 到 "local"** 让部分和在 `k` 循环全程留在寄存器中；DRAM 每个分块只写一次，而不是每次乘加写一次。
* **vectorize(j1)** 把内层宽度为 32 的循环变成 AVX/NEON SIMD。
* **parallel(i0)** 把各个分块摊到多个核上。

现在编译并**实测**（只有这个算数）：

```python
import numpy as np
target, dev = "llvm -mcpu=native", tvm.cpu(0)
func = tvm.build(sch.mod, target=target)

M = N = K = 1024
a = tvm.nd.array(np.random.rand(M, K).astype("float32"), dev)
b = tvm.nd.array(np.random.rand(K, N).astype("float32"), dev)
c = tvm.nd.array(np.zeros((M, N), "float32"), dev)

ev = func.time_evaluator(func.entry_name, dev, number=50)
t = ev(a, b, c).mean
print(f"{t*1e3:.2f} ms   {2*M*N*K/t/1e9:.1f} GFLOP/s")
```

在典型的桌面核上，你会看到朴素嵌套落在几十 GFLOP/s 的低位，而调度后的版本要高出好几**倍**——差距来自 schedule，而非数学。把你的数字与机器的峰值（核数 × SIMD 宽度 × FMA × 时钟）比较，看看还剩多少屋顶空间。

---


<details>
<summary>English original</summary>

**3. The schedule object and the primitives**

You schedule by constructing a `tir.Schedule` over the module and issuing primitives. Each primitive mutates the IR in place; `sch.mod.show()` shows the result.

```python
sch = tir.Schedule(PrimFunc)         # a mutable schedule over the TIR
block_C = sch.get_block("C")         # grab the block by name
i, j, k = sch.get_loops(block_C)     # its loops, outer→inner
```

The core primitives, grouped by what they physically do:

| Primitive | Call | Physically |
|---|---|---|
| **split** | `i0, i1 = sch.split(i, [None, 32])` | cut one loop into a nest of two (tiling) |
| **reorder** | `sch.reorder(i0, j0, i1, j1)` | change loop nesting order |
| **fuse** | `sch.fuse(i, j)` | merge two loops into one |
| **bind** | `sch.bind(i0, "blockIdx.x")` | map a loop onto a GPU grid/block axis |
| **cache_read** | `sch.cache_read(blk, 0, "shared")` | stage an input buffer in faster memory |
| **cache_write** | `sch.cache_write(blk, 0, "local")` | accumulate the output in registers, write back once |
| **compute_at** | `sch.compute_at(prod, loop)` | move a producer block inside a consumer loop (locality) |
| **vectorize** | `sch.vectorize(i1)` | emit SIMD over this loop |
| **unroll** | `sch.unroll(i1)` | unroll for ILP / less loop overhead |
| **parallel** | `sch.parallel(i0)` | run this loop across CPU cores |
| **decompose_reduction** | `sch.decompose_reduction(blk, k0)` | split `init` out above the reduction loop |
| **tensorize** | `sch.tensorize(i1, INTRIN)` | replace an inner region with a hardware intrinsic |

These compose. A real schedule is a *program* of twenty to a hundred of these calls. The art is knowing which sequence maps the loop nest onto the target's memory hierarchy and execution units — which is exactly what the roofline tells you, and exactly what Lecture 3 automates.

---

**4. Worked example: scheduling matmul for a CPU**

Start from the naive nest and transform it into a cache-blocked, vectorized, multi-threaded kernel. The naive version is correct but leaves >90% of the machine on the floor: it streams `B` from DRAM with no reuse and uses one core and one lane.

```python
sch = tir.Schedule(matmul_te(1024, 1024, 1024))
C = sch.get_block("C")
i, j, k = sch.get_loops(C)

# 1) Tile i and j so a block of C stays hot in cache; split k for an inner accumulate loop
i0, i1 = sch.split(i, factors=[None, 32])      # 32×32 output tile
j0, j1 = sch.split(j, factors=[None, 32])
k0, k1 = sch.split(k, factors=[None, 4])

# 2) Reorder: tiles outermost, reduction in the middle, the hot 32×32×4 micro-kernel innermost
sch.reorder(i0, j0, k0, i1, k1, j1)

# 3) Accumulate the output tile in registers, not back to DRAM every k-step
C_local = sch.cache_write(C, 0, "local")
sch.reverse_compute_at(C_local, j0)

# 4) SIMD over the contiguous j1, multi-thread over the outer tile
sch.vectorize(j1)
sch.parallel(i0)

sch.mod.show()                                  # read the transformed nest — this is the kernel
```

Walk the intent against the memory hierarchy:

* **split + reorder** create a `32×32` output tile that fits in L1/registers, so each loaded element of `A` and `B` is **reused** across the tile instead of re-fetched. This is the single biggest lever — it raises **arithmetic intensity**.
* **cache_write to "local"** keeps the partial sums in registers across the `k` loop; DRAM is written once per tile, not once per multiply-add.
* **vectorize(j1)** turns the inner 32-wide loop into AVX/NEON SIMD.
* **parallel(i0)** spreads tiles across cores.

Now compile and **measure** (the only thing that counts):

```python
import numpy as np
target, dev = "llvm -mcpu=native", tvm.cpu(0)
func = tvm.build(sch.mod, target=target)

M = N = K = 1024
a = tvm.nd.array(np.random.rand(M, K).astype("float32"), dev)
b = tvm.nd.array(np.random.rand(K, N).astype("float32"), dev)
c = tvm.nd.array(np.zeros((M, N), "float32"), dev)

ev = func.time_evaluator(func.entry_name, dev, number=50)
t = ev(a, b, c).mean
print(f"{t*1e3:.2f} ms   {2*M*N*K/t/1e9:.1f} GFLOP/s")
```

On a typical desktop core you will see the naive nest land in the low tens of GFLOP/s and the scheduled version several **times** higher — the gap is the schedule, not the math. Compare your number to the machine's peak (cores × SIMD width × FMA × clock) to see how much roof is left.

---

</details>

## 5. 完整示例：为 GPU 调度矩阵乘

GPU 调度回答的是不同的问题：哪个循环是 **grid**，哪个是 **线程块**，什么被**协作加载**到共享内存。原语相同；`bind` 和 `cache_read` 的目标变了。

```python
sch = tir.Schedule(matmul_te(1024, 1024, 1024))
C = sch.get_block("C")
i, j, k = sch.get_loops(C)

# 1) Two-level tile on the spatial axes: outer → thread blocks, inner → threads
bi, ti = sch.split(i, factors=[None, 16])      # 16×16 threads per block
bj, tj = sch.split(j, factors=[None, 16])
sch.reorder(bi, bj, ti, tj, k)

# 2) Map loops onto the CUDA grid
sch.bind(bi, "blockIdx.y")
sch.bind(bj, "blockIdx.x")
sch.bind(ti, "threadIdx.y")
sch.bind(tj, "threadIdx.x")

# 3) Stage A and B tiles in shared memory; the threads in a block fetch them cooperatively
k0, k1 = sch.split(k, factors=[None, 8])       # K-tile of 8
A_sh = sch.cache_read(C, 0, "shared")
B_sh = sch.cache_read(C, 1, "shared")
sch.compute_at(A_sh, k0)                        # load the A tile once per K-step, shared by the block
sch.compute_at(B_sh, k0)

target, dev = "cuda", tvm.cuda(0)
func = tvm.build(sch.mod, target=target)
print(func.imported_modules[0].get_source())   # ← read the generated CUDA C!
```

心智模型：

```text
   grid of thread blocks            each block owns a 16×16 tile of C
   ┌──────┬──────┬──────┐
   │ blk  │ blk  │ blk  │           inside a block, 256 threads each own one C element
   ├──────┼──────┼──────┤           A-tile and B-tile staged in SHARED memory,
   │ blk  │ blk  │ blk  │           loaded cooperatively, reused by all 256 threads
   └──────┴──────┴──────┘           → DRAM traffic cut by the tile width
```

`func.imported_modules[0].get_source()` 打印出 **生成的 CUDA C**。必须读它：你会看到 `__shared__` 数组、`__syncthreads()`、按线程索引的加载。生成的源码就是调度按你意图执行的证明——也是没按意图执行时你调试的地方。

要达到与厂商竞争的 kernel 还需做的工作——寄存器分块、共享内存加载的双缓冲、向量化的 `float4` 全局加载、避免 bank conflict——是*更多同类原语*。这正是那条又长又针对特定目标的长尾，你**不**想为每种 shape 手工调优。这也正是第 3 讲的整个动机。

---

## 6. 每个原语都是一次 roofline（性能上界模型）移动

资深工程师能调度而不乱试的原因：每个原语对 roofline 都有已知的影响。你不是在猜——你是在把点移向某个上界，或者跳到更高的上界。

| 原语 | 物理上改变了什么 | Roofline 影响 |
|---|---|---|
| split / reorder / tile | blocking、循环顺序 | 通过缓存/寄存器**复用** ↑ 算术强度 → 向右移向算力上界 |
| cache_read / cache_write | 把数据暂存到共享内存 / 寄存器 | ↓ DRAM 流量 → ↑ AI，并无停顿地喂给计算单元 |
| bind (block/thread) | 暴露 GPU 并行性 | 让你至少能**触达**这些上界（没有并行，就没有吞吐） |
| vectorize | SIMD 通道 | ↑ 每条指令实际达到的计算吞吐 |
| unroll | ILP、更少的循环开销 | 隐藏延迟 → 更接近算力上界 |
| parallel | 多核 | 随核心数扩展 **CPU** 算力上界 |
| **tensorize** | 把内层块映射到矩阵乘累加 intrinsic | **直接跳到更高的算力上界**（Tensor Cores） |

所以诊断循环是：profile → “我是带宽受限还是算力受限？” → 选择能移动*那个*受限项的原语。带宽受限？分块更狠、缓存更多、合并访存。在 FP32 核上算力受限，但 Tensor Cores 空闲？`tensorize`。

这也是你用来*读*一份调优过的调度（第 3 讲）或 dlight 调度（第 5 讲）的语言：你能辨认出分块大小、共享内存阶段、tensorize 调用，并知道每个在追哪个上界。

---


<details>
<summary>English original</summary>

**5. Worked example: scheduling matmul for a GPU**

The GPU schedule answers different questions: which loop is the **grid**, which is the **thread block**, what gets **cooperatively loaded** into shared memory. The primitives are the same; the targets of `bind` and `cache_read` change.

```python
sch = tir.Schedule(matmul_te(1024, 1024, 1024))
C = sch.get_block("C")
i, j, k = sch.get_loops(C)

# 1) Two-level tile on the spatial axes: outer → thread blocks, inner → threads
bi, ti = sch.split(i, factors=[None, 16])      # 16×16 threads per block
bj, tj = sch.split(j, factors=[None, 16])
sch.reorder(bi, bj, ti, tj, k)

# 2) Map loops onto the CUDA grid
sch.bind(bi, "blockIdx.y")
sch.bind(bj, "blockIdx.x")
sch.bind(ti, "threadIdx.y")
sch.bind(tj, "threadIdx.x")

# 3) Stage A and B tiles in shared memory; the threads in a block fetch them cooperatively
k0, k1 = sch.split(k, factors=[None, 8])       # K-tile of 8
A_sh = sch.cache_read(C, 0, "shared")
B_sh = sch.cache_read(C, 1, "shared")
sch.compute_at(A_sh, k0)                        # load the A tile once per K-step, shared by the block
sch.compute_at(B_sh, k0)

target, dev = "cuda", tvm.cuda(0)
func = tvm.build(sch.mod, target=target)
print(func.imported_modules[0].get_source())   # ← read the generated CUDA C!
```

The mental model:

```text
   grid of thread blocks            each block owns a 16×16 tile of C
   ┌──────┬──────┬──────┐
   │ blk  │ blk  │ blk  │           inside a block, 256 threads each own one C element
   ├──────┼──────┼──────┤           A-tile and B-tile staged in SHARED memory,
   │ blk  │ blk  │ blk  │           loaded cooperatively, reused by all 256 threads
   └──────┴──────┴──────┘           → DRAM traffic cut by the tile width
```

`func.imported_modules[0].get_source()` prints the **generated CUDA C**. Reading it is non-negotiable: you will see the `__shared__` arrays, the `__syncthreads()`, the thread-indexed loads. That generated source is the proof the schedule did what you intended — and the place you debug when it didn't.

The remaining work to reach a vendor-competitive kernel — register tiling, double-buffering the shared loads, vectorized `float4` global loads, avoiding bank conflicts — is *more of the same primitives*. This is precisely the long, target-specific tail that you do **not** want to hand-tune for every shape. Which is the entire motivation for Lecture 3.

---

**6. Every primitive is a roofline move**

The reason a senior engineer can schedule without flailing: each primitive has a known effect on the roofline. You are not guessing — you are moving a point toward a roof, or jumping to a higher roof.

| Primitive | What it changes physically | Roofline effect |
|---|---|---|
| split / reorder / tile | blocking, loop order | ↑ arithmetic intensity via cache/register **reuse** → move right toward the compute roof |
| cache_read / cache_write | stage data in shared / registers | ↓ DRAM traffic → ↑ AI, and feed the compute units without stalling |
| bind (block/thread) | expose GPU parallelism | lets you **reach** the roofs at all (no parallelism, no throughput) |
| vectorize | SIMD lanes | ↑ achieved compute throughput per instruction |
| unroll | ILP, less loop overhead | hide latency → get closer to the compute roof |
| parallel | multicore | scale the **CPU** compute roof with core count |
| **tensorize** | map inner block to an MMA intrinsic | **jump to a higher compute roof entirely** (Tensor Cores) |

So the diagnostic loop is: profile → "am I memory-bound or compute-bound?" → pick the primitive that moves *that* bound. Memory-bound? Tile harder, cache more, coalesce. Compute-bound on FP32 cores but there are Tensor Cores idle? `tensorize`.

This is also the language you use to *read* a tuned schedule (Lecture 3) or a dlight schedule (Lecture 5): you recognize the tile sizes, the shared-memory stages, the tensorize call, and you know what roof each one is chasing.

---

</details>

## 7. `tensorize`：跳到 Tensor Core 上界

CUDA 核心上的 FP32 SIMD 有一个计算上界。**Tensor Core** 的上界要高得多——但只针对其 MMA（矩阵乘累加）指令的特定 shape 与 dtype（例如 `16×16×16` 的 fp16 输入 / fp32 累加分块）。`tensorize` 就是那个原语：把匹配的内层 block 替换为该硬件 intrinsic。

该模式：

1. 调度时让最内层 block 的 shape **与 intrinsic 的匹配**（例如把 `i,j,k` 分块到 `16,16,16`，把 fragment 加载到正确的 memory scope）。
2. 把那个内层 block `tensorize` 到已注册的 **tensor intrinsic** 上。

```python
# Conceptually (Tensor Core path — requires fp16 inputs, fp32 accumulate, the right scopes):
from tvm.tir.tensor_intrin import cuda as cuda_intrin   # predefined wmma/mma intrinsics

# ... schedule down to a 16×16×16 inner block `mma`, with A/B in wmma.matrix_a/b scope ...
sch.tensorize(mma, cuda_intrin.WMMA_SYNC_16x16x16_f16f16f32_INTRIN)
```

TVM 自带一个已注册 intrinsic 的库（`tvm.tir.tensor_intrin`），面向 NVIDIA 的 wmma/mma，你也可以 **注册自己的**（`tvm.tir.TensorIntrin.register`）——这正是你瞄准 *自定义* 加速器的 GEMM 指令或 TVM 的开源 VTA 加速器的方式。这种可扩展性是从「编译器」通向「为尚不存在的硬件而写的编译器」的桥梁，并直接引向第 4 讲的 BYOC。

收益很大，值得直接测量：一个正确 tensorize 的 fp16 矩阵乘会从 CUDA 核心的 FP32 上界移到 Tensor Core 上界——通常是数倍，而不是几个百分点。在 `tensorize` 调用前后打印 GFLOP/s；这个跃升本身就是这一课。

---

## 8. 测量它

调度是一个假设。`time_evaluator` 就是你检验它的手段。现在就养成这个纪律，因为后面每一讲都用数字给你打分。

```python
def gflops(func, shapes, dev, number=50):
    M, N, K = shapes
    a = tvm.nd.array(np.random.rand(M, K).astype("float32"), dev)
    b = tvm.nd.array(np.random.rand(K, N).astype("float32"), dev)
    c = tvm.nd.array(np.zeros((M, N), "float32"), dev)
    ev = func.time_evaluator(func.entry_name, dev, number=number)
    t = ev(a, b, c).mean
    return 2 * M * N * K / t / 1e9, t

# Always verify correctness before celebrating speed:
ref = a.numpy() @ b.numpy()
np.testing.assert_allclose(c.numpy(), ref, rtol=1e-3)
```

永远报告三个数字：**正确性**（与参考实现的精度一致性——一个跑得飞快但算错的 kernel 毫无价值）、**GFLOP/s**，以及 **roofline 峰值的百分比**。第三个数字才告诉你该继续调度还是收手。到了峰值的 85%，收工。到了 15%，说明你手上是一个带宽受限的 kernel，§6 的表格会告诉你该伸手拿哪个原语。

---

## 9. 迷你实验：用三种方式调度一个 kernel

拿矩阵乘来做（或者，如果你想要更难的归约结构，就用 2D 卷积）。

1. **基线：** 搭出朴素循环嵌套，记录 GFLOP/s 与 CPU 峰值的百分比。
2. **CPU 调度：** 分块 → `cache_write` local → `vectorize` → `parallel`。在 `{8, 16, 32, 64}` 上扫分块尺寸，画出 GFLOP/s 随分块变化的曲线。找出缓存断崖。
3. **GPU 调度：** block/thread 绑定 → shared memory `cache_read`，配合 `compute_at`。打印生成的 CUDA，找出 `__shared__` 数组和 `__syncthreads()`。记录 GFLOP/s 与 GPU FP32 峰值的百分比。
4. **（进阶）tensorize：** fp16 输入，调度到 `16×16×16` 内层 block，`tensorize` 到 wmma intrinsic。记录向 Tensor Core 上界的跃升。

交付物：一张表——`{baseline, cpu, gpu, tensorized}` × `{GFLOP/s, % peak, parity}`——外加每行两句说明，点出该调度所追逐的 roofline 上界。那张表就是一份 Level-4 产物：数字，加上原始数据与解读。

---

## 关键要点

- 计算定义是 *what*；**调度**是 *how*。TensorIR 让 *how* 成为一等公民，且构造即合法的程序。
- 一个 **TIR block** 声明空间轴与归约轴（`"SSR"`）、一个 `init` 区域，以及读/写区域。这些标注就是契约，保证每个原语都是一次数值等价的改写。
- 这些原语——`split`、`reorder`、`bind`、`cache_read/write`、`compute_at`、`vectorize`、`unroll`、`parallel`、`decompose_reduction`、`tensorize`——组合成一个调度，把循环映射到存储层次与执行单元上。
- CPU 调度是 分块 → 缓存进寄存器 → 向量化 → 并行化。GPU 调度是 block/thread 绑定 → 协作式 shared memory 暂存。原语相同，`bind`/scope 目标不同。
- **每个原语都是一次 roofline 移动。** 做 profile，点明受限在哪，挑出能推动它的原语。`tensorize` 跳到 *更高* 的上界（Tensor Core / 自定义 MMA）。
- 读生成的源码（`get_source()`），并始终测量 正确性 + GFLOP/s + 峰值的百分比。在 evaluator 确认之前，调度只是一个假设。
- 为每种 shape/target 手工调度，正是第 3 讲要自动化掉的苦活——但你必须先亲手弄懂它，才能信任（并调试）机器。

---


<details>
<summary>English original</summary>

**7. `tensorize`: jumping to the Tensor Core roof**

FP32 SIMD on the CUDA cores has one compute roof. The **Tensor Cores** have a far higher one — but only for the specific shape and dtype of their MMA (matrix-multiply-accumulate) instruction (e.g. a `16×16×16` fp16-in / fp32-accumulate tile). `tensorize` is the primitive that replaces a matching inner block with that hardware intrinsic.

The pattern:

1. Schedule so the innermost block's shape **matches the intrinsic's** (e.g. tile `i,j,k` down to `16,16,16`, load fragments to the right memory scopes).
2. `tensorize` that inner block against a registered **tensor intrinsic**.

```python
# Conceptually (Tensor Core path — requires fp16 inputs, fp32 accumulate, the right scopes):
from tvm.tir.tensor_intrin import cuda as cuda_intrin   # predefined wmma/mma intrinsics

# ... schedule down to a 16×16×16 inner block `mma`, with A/B in wmma.matrix_a/b scope ...
sch.tensorize(mma, cuda_intrin.WMMA_SYNC_16x16x16_f16f16f32_INTRIN)
```

TVM ships a library of registered intrinsics (`tvm.tir.tensor_intrin`) for NVIDIA wmma/mma, and you can **register your own** (`tvm.tir.TensorIntrin.register`) — which is exactly how you'd target a *custom* accelerator's GEMM instruction or TVM's open VTA accelerator. That extensibility is the bridge from "compiler" to "compiler for hardware that does not exist yet," and it leads directly into BYOC in Lecture 4.

The payoff is large and worth measuring directly: a correctly tensorized fp16 matmul moves from the CUDA-core FP32 roof to the Tensor Core roof — typically a multiple, not a few percent. Print the GFLOP/s before and after the `tensorize` call; the jump is the lesson.

---

**8. Measure it**

A schedule is a hypothesis. The `time_evaluator` is how you test it. Build the discipline now, because every later lecture grades you by the number.

```python
def gflops(func, shapes, dev, number=50):
    M, N, K = shapes
    a = tvm.nd.array(np.random.rand(M, K).astype("float32"), dev)
    b = tvm.nd.array(np.random.rand(K, N).astype("float32"), dev)
    c = tvm.nd.array(np.zeros((M, N), "float32"), dev)
    ev = func.time_evaluator(func.entry_name, dev, number=number)
    t = ev(a, b, c).mean
    return 2 * M * N * K / t / 1e9, t

# Always verify correctness before celebrating speed:
ref = a.numpy() @ b.numpy()
np.testing.assert_allclose(c.numpy(), ref, rtol=1e-3)
```

Report three numbers, always: **correctness** (parity vs a reference — a fast wrong kernel is worthless), **GFLOP/s**, and **% of roofline peak**. The third is what tells you whether to keep scheduling or stop. At 85% of peak, go home. At 15%, you have a memory-bound kernel and the table in §6 tells you which primitive to reach for.

---

**9. Mini-lab: schedule a kernel three ways**

Take matmul (or a 2D convolution if you want a harder reduction structure).

1. **Baseline:** build the naive nest, record GFLOP/s and % of CPU peak.
2. **CPU schedule:** tile → `cache_write` local → `vectorize` → `parallel`. Sweep the tile size over `{8, 16, 32, 64}` and plot GFLOP/s vs tile. Find the cache cliff.
3. **GPU schedule:** block/thread bind → shared-memory `cache_read` with `compute_at`. Print the generated CUDA, find the `__shared__` arrays and `__syncthreads()`. Record GFLOP/s and % of GPU FP32 peak.
4. **(Stretch) tensorize:** fp16 inputs, schedule to a `16×16×16` inner block, `tensorize` to a wmma intrinsic. Record the jump to the Tensor Core roof.

Deliverable: one table — `{baseline, cpu, gpu, tensorized}` × `{GFLOP/s, % peak, parity}` — and a two-sentence note per row naming the roofline bound each schedule was chasing. That table is a Level-4 artifact: numbers, with raw data and interpretation.

---

**Key takeaways**

- A compute definition is *what*; the **schedule** is *how*. TensorIR makes the *how* a first-class, legal-by-construction program.
- A **TIR block** declares spatial vs reduction axes (`"SSR"`), an `init` region, and read/write regions. Those annotations are the contract that keeps every primitive a numerically-identical rewrite.
- The primitives — `split`, `reorder`, `bind`, `cache_read/write`, `compute_at`, `vectorize`, `unroll`, `parallel`, `decompose_reduction`, `tensorize` — compose into a schedule that maps loops onto the memory hierarchy and execution units.
- CPU scheduling is tile → cache in registers → vectorize → parallelize. GPU scheduling is block/thread bind → cooperative shared-memory staging. Same primitives, different `bind`/scope targets.
- **Every primitive is a roofline move.** Profile, name the bound, pick the primitive that moves it. `tensorize` jumps to a *higher* roof (Tensor Cores / custom MMA).
- Read the generated source (`get_source()`) and always measure correctness + GFLOP/s + % of peak. A schedule is a hypothesis until the evaluator confirms it.
- Hand-scheduling for every shape/target is exactly the toil Lecture 3 automates — but you must understand it by hand first to trust (and debug) the machine.

---

</details>

## 参考资料

- TensorIR 深入剖析 —— block、轴、调度原语：[https://tvm.apache.org/docs/deep_dive/tensor_ir/index.html](https://tvm.apache.org/docs/deep_dive/tensor_ir/index.html)
- “Blitz Course to TensorIR”教程：[https://tvm.apache.org/docs/tutorial/tensor_ir_blitz_course.html](https://tvm.apache.org/docs/tutorial/tensor_ir_blitz_course.html)
- `tvm.tir.schedule` API 参考：[https://tvm.apache.org/docs/reference/api/python/tir.html](https://tvm.apache.org/docs/reference/api/python/tir.html)
- “Use Tensorize to Leverage Hardware Intrinsics”：[https://tvm.apache.org/docs/how_to/work_with_schedules/tensorize.html](https://tvm.apache.org/docs/how_to/work_with_schedules/tensorize.html)
- Feng 等，“TensorIR: An Abstraction for Automatic Tensorized Program Optimization”，ASPLOS 2023：[https://arxiv.org/abs/2207.04296](https://arxiv.org/abs/2207.04296)
- *TVM Deep Dives* —— [Lecture 03 — Auto-tuning](Lecture-03.md)，它自动搜索的正是这个调度空间。

---

*下一讲：[Lecture 03 — Auto-tuning: AutoTVM → Ansor → MetaSchedule](Lecture-03.md)*


<details>
<summary>English original</summary>

**References**

- TensorIR deep dive — blocks, axes, schedule primitives: [https://tvm.apache.org/docs/deep_dive/tensor_ir/index.html](https://tvm.apache.org/docs/deep_dive/tensor_ir/index.html)
- "Blitz Course to TensorIR" tutorial: [https://tvm.apache.org/docs/tutorial/tensor_ir_blitz_course.html](https://tvm.apache.org/docs/tutorial/tensor_ir_blitz_course.html)
- `tvm.tir.schedule` API reference: [https://tvm.apache.org/docs/reference/api/python/tir.html](https://tvm.apache.org/docs/reference/api/python/tir.html)
- "Use Tensorize to Leverage Hardware Intrinsics": [https://tvm.apache.org/docs/how_to/work_with_schedules/tensorize.html](https://tvm.apache.org/docs/how_to/work_with_schedules/tensorize.html)
- Feng et al., "TensorIR: An Abstraction for Automatic Tensorized Program Optimization," ASPLOS 2023: [https://arxiv.org/abs/2207.04296](https://arxiv.org/abs/2207.04296)
- *TVM Deep Dives* — [Lecture 03 — Auto-tuning](Lecture-03.md), which searches this exact schedule space automatically.

---

*Next: [Lecture 03 — Auto-tuning: AutoTVM → Ansor → MetaSchedule](Lecture-03.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/TVM Deep Dives/Lecture-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/TVM%20Deep%20Dives/Lecture-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
