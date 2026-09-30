---
title: 第 01 讲 - TVM 栈与编译流程：Relax、TensorIR 与统一的 IRModule
description: 第 01 讲 - TVM 栈与编译流程：Relax、TensorIR 与统一的 IRModule
---

# 第 01 讲 - TVM 栈与编译流程：Relax、TensorIR 与统一的 IRModule

**合集：** [TVM 深度剖析](README.md) | **上一篇：** [← TVM 深度剖析索引](README.md) | **下一篇：** [第 02 讲](Lecture-02.md)

---

框架会**派发**一张图。编译器会**重写**它。

当你在 PyTorch 中调用 `model(x)` 时，框架会逐算子遍历这张图，并对每个算子调用一个预编译的 kernel——cuDNN、cuBLAS、oneDNN、一个手写的 CUDA 文件。硬件所做的是**厂商库作者**所决定的，针对的是厂商库作者所预想的那些 shape。

Apache TVM 做的则不同。它把*整张*图当作数据，对其运行分析与重写 pass，把它 lower 成一个显式的循环表示，机械地搜索将这些循环映射到**你的**目标平台的 memory hierarchy 与执行单元上的好方法，并生成一个独立的产物。

本讲就是这台机器的地图。在你能调度一个 kernel（第 2 讲）或调优一个 kernel（第 3 讲）之前，你需要知道**有哪些层级、哪个产物承载它们、以及流程的每个阶段被允许改变什么**。

---

## 学习目标

学完本讲，你应该能够：

1. 解释张量编译器为何存在——shape × dtype × target × fusion 的组合爆炸，是厂商库无法覆盖的。
2. 说出 TVM 栈的各个层级——**Relax**（图 IR）、**TensorIR / TIR**（循环 IR）、目标代码生成——以及各自代表什么。
3. 描述 **`IRModule`** 作为同时持有*所有*层级的单一产物，以及这种「统一」为何重要。
4. 走通编译流程：导入 → 图优化 → lower → 调度 → 代码生成 → runtime。
5. 在历史上定位 Relay 与 Relax，并说明为何 Relax（TVM Unity）是前进方向。
6. 导入一个真实模型，打印每一个 IR 层级，构建它并运行它——并读懂每一步的输出。

---

## 1. 张量编译器为何存在

考虑一个算子：矩阵乘法，`C[M,N] = A[M,K] @ B[K,N]`。

*数学*只有四行。*快速实现*则不然，而且并不是只有**一个**快速实现。正确的代码取决于：

```text
shape      M, N, K — tall-skinny vs square vs batched-tiny behave totally differently
dtype      fp32 / fp16 / bf16 / int8 / fp8 / int4 — different vector widths, different cores
target     AVX-512 vs NEON vs SM80 Tensor Core vs SM90 vs Apple AMX vs a custom NPU
fusion     is a bias-add, a ReLU, a dequant fused onto the epilogue?
layout     row-major, column-major, NCHW, NHWC, blocked NC/16c
```

这个笛卡尔积极为庞大。一个厂商库会附带几千个手工调优的 kernel，覆盖该空间中**常见的**那些点。一旦偏离这张网格——一个奇怪的 shape、一种新的 dtype、一个没人预想过的融合模式、一款没人写过库的加速器——你就只能退回到一个缓慢的通用 kernel，或者什么都退回不了。

编译器的赌注恰恰相反：**把计算描述一次，把硬件映射描述成一个可变换的程序，然后针对你此刻所站的精确那一点生成 kernel。** 这就是 TVM 存在的全部理由。其余的一切——Relax、TIR、MetaSchedule、BYOC——都是为这个赌注服务的机器。

用硬件优先的说法来讲：框架给你的是厂商所暴露的硬件。编译器让你瞄准的是**你实际拥有的**硬件，在**你实际运行的** shape 上。


<details>
<summary>English original</summary>

**Lecture 01 - The TVM Stack and the Compilation Flow: Relax, TensorIR, and the Unified IRModule**

**Collection:** [TVM Deep Dives](README.md) | **Previous:** [← TVM Deep Dives index](README.md) | **Next:** [Lecture 02](Lecture-02.md)

---

A framework **dispatches** a graph. A compiler **rewrites** it.

When you call `model(x)` in PyTorch, the framework walks the graph op by op and, for each op, calls into a precompiled kernel — cuDNN, cuBLAS, oneDNN, a hand-written CUDA file. The hardware does what the **vendor library author** decided, for the shapes the vendor library author anticipated.

Apache TVM does something different. It takes the *whole* graph as data, runs analysis and rewriting passes over it, lowers it to an explicit loop representation, mechanically searches for a good way to map those loops onto the memory hierarchy and execution units of **your** target, and emits a standalone artifact.

This lecture is the map of that machine. Before you can schedule a kernel (Lecture 2) or tune one (Lecture 3), you need to know **what the levels are, what artifact carries them, and what each stage of the flow is allowed to change**.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Explain why a tensor compiler exists — the shape × dtype × target × fusion explosion that vendor libraries cannot cover.
2. Name the levels of the TVM stack — **Relax** (graph IR), **TensorIR / TIR** (loop IR), target codegen — and what each one represents.
3. Describe the **`IRModule`** as the single artifact that holds *all* levels at once, and why that "unity" matters.
4. Walk the compilation flow: import → graph optimization → lower → schedule → codegen → runtime.
5. Place Relay vs Relax historically and say why Relax (TVM Unity) is the forward direction.
6. Import a real model, print every IR level, build it, and run it — and read the output of each step.

---

**1. Why a tensor compiler exists**

Consider one operator: a matrix multiply, `C[M,N] = A[M,K] @ B[K,N]`.

The *math* is four lines. The *fast implementation* is not, and there is not **one** fast implementation. The right code depends on:

```text
shape      M, N, K — tall-skinny vs square vs batched-tiny behave totally differently
dtype      fp32 / fp16 / bf16 / int8 / fp8 / int4 — different vector widths, different cores
target     AVX-512 vs NEON vs SM80 Tensor Core vs SM90 vs Apple AMX vs a custom NPU
fusion     is a bias-add, a ReLU, a dequant fused onto the epilogue?
layout     row-major, column-major, NCHW, NHWC, blocked NC/16c
```

The cross product is enormous. A vendor library ships a few thousand hand-tuned kernels covering the **common** points of that space. Step off the grid — an odd shape, a new dtype, a fused pattern nobody anticipated, an accelerator nobody wrote a library for — and you fall back to a slow generic kernel, or to nothing.

A compiler's bet is the opposite: **describe the computation once, describe the hardware mapping as a transformable program, and generate the kernel for the exact point you are standing on.** That is the entire reason TVM exists. Everything else — Relax, TIR, MetaSchedule, BYOC — is machinery in service of that bet.

The hardware-first way to say it: a framework gives you the hardware the vendor exposed. A compiler lets you target the hardware **you actually have**, at the shape **you actually run**.

---

</details>

## 2. 栈的层级

TVM 是一个**多层级**编译器。其中两个层级最重要，而你两个层级都要长期打交道。

```text
        high level  │  Relax     — the dataflow graph: ops, tensors, control flow, shapes
                    │              "what computation, in what order"
   ─────────────────┼──────────────────────────────────────────────────────────
        low level   │  TensorIR  — the loop nest: for-loops, buffers, compute, reductions
            (TIR)   │              "exactly which element, in which order, into which memory"
   ─────────────────┼──────────────────────────────────────────────────────────
        target      │  CUDA C / LLVM IR / Metal / Vulkan SPIR-V / C   — emitted source
```

**Relax**（“Relax” 的缩写，意为 Relay 的 relaxation）是**图中间表示**。一个 Relax 函数看上去就是一串具有 dataflow 结构的张量运算——很像 FX 图或 ONNX 图，但带有作为一等公民的**符号 shape**，并且能直接向下调用 TIR。

**TensorIR（TIR）**是**循环级中间表示**。一个 TIR `PrimFunc` 是显式的 `for` 循环嵌套，循环体作用于 buffer，计算按元素逐一写出，外加标记可调度单元的 **block** 标注。*调度*就发生在这个层级——拆分循环、把循环绑定到 GPU 线程、缓存进共享内存、向量化。

TVM 的关键设计决策，也是它区别于 XLA（把循环层级藏起来）或 TensorRT（把一切都藏起来）之处在于：**循环层级是一等公民，可检视，且可编程地变换。** 你可以把它打印出来、重写它，而重写就是优化。

---

## 3. IRModule：一个产物，涵盖所有层级

为 “TVM Unity” 命名的正是下面这个想法：**单一容器 `IRModule` 同时承载所有层级的函数。** 图层的 Relax 函数与它所调用的循环级 TIR 函数位于*同一个*模块之中，可以被*一并*优化。

```python
import tvm
from tvm.script import ir as I, relax as R, tir as T

@I.ir_module
class MyModule:
    # ---- TIR level: an explicit loop nest, schedulable ----
    @T.prim_func
    def matmul(A: T.Buffer((128, 128), "float32"),
               B: T.Buffer((128, 128), "float32"),
               C: T.Buffer((128, 128), "float32")):
        for i, j, k in T.grid(128, 128, 128):
            with T.block("C"):
                vi, vj, vk = T.axis.remap("SSR", [i, j, k])   # S=spatial, R=reduction
                with T.init():
                    C[vi, vj] = T.float32(0)
                C[vi, vj] = C[vi, vj] + A[vi, vk] * B[vk, vj]

    # ---- Relax level: the dataflow graph, calls the TIR func above ----
    @R.function
    def main(x: R.Tensor((128, 128), "float32"),
             w: R.Tensor((128, 128), "float32")) -> R.Tensor((128, 128), "float32"):
        cls = MyModule
        with R.dataflow():
            # call_tir: invoke a TIR PrimFunc from the graph, declaring the output shape
            lv = R.call_tir(cls.matmul, (x, w), out_sinfo=R.Tensor((128, 128), "float32"))
            R.output(lv)
        return lv
```

仔细读一读，因为这就是整个栈，浓缩在二十行里：

* `@T.prim_func matmul` 是 **TIR**——循环嵌套。带 `T.axis.remap("SSR", ...)` 的 `T.block("C")` 声明了一个可调度 block，其 `i,j` 轴是*空间*轴，其 `k` 轴是*归约*轴。（第 2 讲完全发生在这类 block 内部。）
* `@R.function main` 是 **Relax**——图。`R.call_tir` 是桥梁：图向着*下方*调用 TIR kernel，并用 `out_sinfo` 声明输出 shape。
* `R.dataflow()` 标记一个 **dataflow block**——一块无副作用的区域，优化器可以自由重排、融合、重写。（第 4 讲就在这里。）

这种共居一地正是要点。图级 fuser 可以把两个算子折叠到一起，并为融合结果*生成一个新的 TIR 函数*。tuner 可以重写某个 TIR 函数，而图仍然指向它。在“图编译器”与另一个独立的“kernel 编译器”之间不存在有损交接——自上而下始终是同一个模块。

`mod.show()` 可以把任意 `IRModule` pretty-print 成这种 **TVMScript**——一种能与实际中间表示双向往返的 Python 语法。它是你的主要调试工具。早打印，勤打印。

---


<details>
<summary>English original</summary>

**2. The levels of the stack**

TVM is a **multi-level** compiler. Two levels matter most, and you will live in both.

```text
        high level  │  Relax     — the dataflow graph: ops, tensors, control flow, shapes
                    │              "what computation, in what order"
   ─────────────────┼──────────────────────────────────────────────────────────
        low level   │  TensorIR  — the loop nest: for-loops, buffers, compute, reductions
            (TIR)   │              "exactly which element, in which order, into which memory"
   ─────────────────┼──────────────────────────────────────────────────────────
        target      │  CUDA C / LLVM IR / Metal / Vulkan SPIR-V / C   — emitted source
```

**Relax** (short for "Relax", the relaxation of Relay) is the **graph IR**. A Relax function looks like a sequence of tensor operations with a dataflow structure — much like an FX graph or an ONNX graph, but with first-class **symbolic shapes** and the ability to call down into TIR directly.

**TensorIR (TIR)** is the **loop-level IR**. A TIR `PrimFunc` is an explicit nest of `for` loops over buffers, with the computation written out element-by-element, plus **block** annotations that mark the schedulable units. This is the level you *schedule* — split loops, bind them to GPU threads, cache into shared memory, vectorize.

The key TVM design decision, and the thing that distinguishes it from XLA (which hides the loop level) or TensorRT (which hides everything): **the loop level is first-class, inspectable, and programmatically transformable.** You can print it, rewrite it, and the rewrite is the optimization.

---

**3. The IRModule: one artifact, all levels**

Here is the idea that names "TVM Unity": **a single container, the `IRModule`, holds functions at every level simultaneously.** A graph-level Relax function and the loop-level TIR functions it calls live in the *same* module and can be optimized *together*.

```python
import tvm
from tvm.script import ir as I, relax as R, tir as T

@I.ir_module
class MyModule:
    # ---- TIR level: an explicit loop nest, schedulable ----
    @T.prim_func
    def matmul(A: T.Buffer((128, 128), "float32"),
               B: T.Buffer((128, 128), "float32"),
               C: T.Buffer((128, 128), "float32")):
        for i, j, k in T.grid(128, 128, 128):
            with T.block("C"):
                vi, vj, vk = T.axis.remap("SSR", [i, j, k])   # S=spatial, R=reduction
                with T.init():
                    C[vi, vj] = T.float32(0)
                C[vi, vj] = C[vi, vj] + A[vi, vk] * B[vk, vj]

    # ---- Relax level: the dataflow graph, calls the TIR func above ----
    @R.function
    def main(x: R.Tensor((128, 128), "float32"),
             w: R.Tensor((128, 128), "float32")) -> R.Tensor((128, 128), "float32"):
        cls = MyModule
        with R.dataflow():
            # call_tir: invoke a TIR PrimFunc from the graph, declaring the output shape
            lv = R.call_tir(cls.matmul, (x, w), out_sinfo=R.Tensor((128, 128), "float32"))
            R.output(lv)
        return lv
```

Read this carefully, because it is the whole stack in twenty lines:

* `@T.prim_func matmul` is **TIR** — the loop nest. The `T.block("C")` with `T.axis.remap("SSR", ...)` declares a schedulable block whose `i,j` axes are *spatial* and whose `k` axis is a *reduction*. (Lecture 2 lives entirely inside this kind of block.)
* `@R.function main` is **Relax** — the graph. `R.call_tir` is the bridge: the graph reaches *down* and calls the TIR kernel, declaring the output shape with `out_sinfo`.
* `R.dataflow()` marks a **dataflow block** — a side-effect-free region the optimizer is free to reorder, fuse, and rewrite. (Lecture 4 lives here.)

That co-residence is the point. The graph-level fuser can fold two ops together and *generate a new TIR function* for the fused result. The tuner can rewrite a TIR function and the graph still points at it. There is no lossy hand-off between a "graph compiler" and a separate "kernel compiler" — it is one module the whole way down.

`mod.show()` pretty-prints any `IRModule` as this **TVMScript** — Python-syntax that round-trips to and from the actual IR. It is your primary debugging tool. Print early, print often.

---

</details>

## 4. 编译流程

下面是 `import model` 与 `output = compiled(x)` 之间发生的事情。

```text
   PyTorch / ONNX / TF / JAX model
            │   ① import  (frontend → Relax)
            ▼
   ┌────────────────────────────────────┐
   │ IRModule                           │
   │   @R.function main      (Relax)    │
   │   @T.prim_func ...      (TIR)      │
   └────────────────────────────────────┘
            │   ② graph-level passes        [operates on Relax]
            │        fuse ops, transform layout, fold constants, plan memory
            │   ③ legalize + lower           [Relax → TIR]
            │        turn each high-level op into a TIR PrimFunc
            │   ④ schedule / tune            [operates on TIR]
            │        map loops → memory hierarchy & execution units  (Lec 2 & 3)
            │   ⑤ codegen                    [TIR → target source]
            ▼
   target module:  .ptx/.cubin (CUDA) | .o (LLVM) | .metallib | SPIR-V | .c
            │   ⑥ runtime
            ▼
   loaded module you call like a function   (GraphExecutor | Relax VM | AOT)
```

每个阶段都有一份严格的契约，规定**它可以改动什么**：

| 阶段 | 输入 → 输出 | 允许改动什么 | 必须保留什么 |
|---|---|---|---|
| ① 导入 | 框架图 → Relax | 仅表示形式 | 数值 |
| ② 图 pass | Relax → Relax | op 结构：融合、重布局、折叠、规划内存 | 函数的输入→输出映射 |
| ③ Legalize/lower | Relax → Relax+TIR | 用具体 TIR 循环嵌套替换抽象 op | 每个 op 的语义 |
| ④ 调度/tune | TIR → TIR | *循环顺序、分块、内存布局、线程化* | 计算结果，严格一致 |
| ⑤ 代码生成 | TIR → 目标源码 | 无语义改动——纯翻译 | 全部 |
| ⑥ Runtime | 模块 → 可调用对象 | 执行策略（静态图 vs VM vs AOT） | 结果 |

把加粗的那条内化：**调度只改变顺序和内存映射，绝不改变结果。** 正是这条不变量让自动调优变得安全——机器可以尝试一万种调度，而每一种按构造都是数值上完全相同的 kernel。第 3 讲完全建立在这一保证之上。

---

## 5. Relay vs Relax：这套栈为什么有历史

在 TVM 文档里你会看到两种图 IR，应当分得清哪个是哪个。

| | **Relay**（legacy） | **Relax**（TVM Unity，当前） |
|---|---|---|
| 年代 | 约 2018，第一代图 IR | 约 2023+，“Unity”方向 |
| 形状 | 基本静态；动态形状是后补的 | **符号化/动态形状是一等公民** |
| 与 TIR 的关系 | 独立的编译步骤，有损交接 | **同一个 IRModule**，`call_tir` 跨层级 |
| 优化 | 仅图级别 | 图 + 张量级别，**联合进行** |
| LLM / 动态模型 | 别扭 | 为它们而设计（MLC-LLM 就构建在其上） |
| 状态 | 仍然存在、仍在维护，但属 legacy | 是前进方向；新工作都以 Relax 为目标 |

简而言之：**Relay** 证明了图 IR + 自动调优这套思路可行，但把图层和张量层留在两个彼此隔离的世界里，两者之间只有单向交接。**Relax**（“Relax”= 更放松、更动态、跨层级的 IR）推倒了这堵墙——图和 TIR 共享同一个模块，动态形状是天生的，整个设计围绕那些让 Relay 撑不住的工作负载：带 KV-cache 的 LLM、变长序列和控制流。

本课程：**我们写 Relax。** Relay 只会在你读旧教程时出现。如果某份文档写的是 `relay.build`，现代等价写法是 `relax.build` / `tvm.compile`。

这不是纸上谈兵。MLC-LLM（第 5 讲）之所以能在手机 GPU 上跑 Qwen 模型，正是因为 Relax 把动态形状、跨层级编译做成了一等公民。这段历史*就是*这项能力。

---


<details>
<summary>English original</summary>

**4. The compilation flow**

Here is what happens between `import model` and `output = compiled(x)`.

```text
   PyTorch / ONNX / TF / JAX model
            │   ① import  (frontend → Relax)
            ▼
   ┌────────────────────────────────────┐
   │ IRModule                           │
   │   @R.function main      (Relax)    │
   │   @T.prim_func ...      (TIR)      │
   └────────────────────────────────────┘
            │   ② graph-level passes        [operates on Relax]
            │        fuse ops, transform layout, fold constants, plan memory
            │   ③ legalize + lower           [Relax → TIR]
            │        turn each high-level op into a TIR PrimFunc
            │   ④ schedule / tune            [operates on TIR]
            │        map loops → memory hierarchy & execution units  (Lec 2 & 3)
            │   ⑤ codegen                    [TIR → target source]
            ▼
   target module:  .ptx/.cubin (CUDA) | .o (LLVM) | .metallib | SPIR-V | .c
            │   ⑥ runtime
            ▼
   loaded module you call like a function   (GraphExecutor | Relax VM | AOT)
```

Each stage has a strict contract about **what it may change**:

| Stage | Input → Output | What it is allowed to change | What it must preserve |
|---|---|---|---|
| ① Import | framework graph → Relax | representation only | numerics |
| ② Graph passes | Relax → Relax | op structure: fuse, re-layout, fold, plan memory | the function's input→output mapping |
| ③ Legalize/lower | Relax → Relax+TIR | replace abstract ops with concrete TIR loop nests | semantics of each op |
| ④ Schedule/tune | TIR → TIR | *loop order, tiling, memory placement, threading* | the computed result, exactly |
| ⑤ Codegen | TIR → target src | nothing semantic — pure translation | everything |
| ⑥ Runtime | module → callable | execution policy (static graph vs VM vs AOT) | results |

Internalize the one in bold: **scheduling changes only the order and the memory mapping, never the result.** That invariant is what makes auto-tuning safe — the machine can try ten thousand schedules and every one of them is, by construction, numerically the same kernel. Lecture 3 is built entirely on that guarantee.

---

**5. Relay vs Relax: why the stack has a history**

You will see two graph IRs in TVM documentation and you should know which is which.

| | **Relay** (legacy) | **Relax** (TVM Unity, current) |
|---|---|---|
| Era | ~2018, first-gen graph IR | ~2023+, the "Unity" direction |
| Shapes | mostly static; dynamic shape bolted on | **symbolic / dynamic shapes first-class** |
| Relationship to TIR | separate compile step, lossy hand-off | **same IRModule**, `call_tir` cross-level |
| Optimization | graph-level only | graph + tensor level, **jointly** |
| LLM / dynamic models | awkward | designed for them (MLC-LLM is built on it) |
| Status | still present, maintained, but legacy | the forward direction; new work targets Relax |

The short version: **Relay** proved the graph-IR + auto-tuning idea but kept the graph and tensor levels in separate worlds with a one-way hand-off between them. **Relax** ("Relax" = a more relaxed, dynamic, cross-level IR) collapses that wall — graph and TIR share one module, dynamic shapes are native, and the whole thing was designed around the workloads that broke Relay: LLMs with KV-caches, variable sequence lengths, and control flow.

For this course: **we write Relax.** Relay appears only when you read an older tutorial. If a doc says `relay.build`, the modern equivalent is `relax.build` / `tvm.compile`.

This is not academic trivia. The reason MLC-LLM (Lecture 5) can run a Qwen model on a phone GPU is that Relax made dynamic-shape, cross-level compilation a first-class thing. The history *is* the capability.

---

</details>

## 6. 动手实践：导入、检查、构建、运行

拿一个真实模型完整走一遍。无论来源是 PyTorch 还是 ONNX，模式完全相同；前端不同，其后的一切都一样。

**来自 PyTorch（经由 `torch.export`）：**

```python
import torch, torch.nn as nn
import tvm
from tvm import relax
from tvm.relax.frontend.torch import from_exported_program

class MLP(nn.Module):
    def __init__(self):
        super().__init__()
        self.fc1 = nn.Linear(784, 256)
        self.fc2 = nn.Linear(256, 10)
    def forward(self, x):
        return self.fc2(torch.relu(self.fc1(x)))

model = MLP().eval()
example = (torch.randn(1, 784),)

# torch.export gives a clean, functional graph; TVM imports it into Relax
exported = torch.export.export(model, example)
mod = from_exported_program(exported, keep_params_as_input=False)
```

**来自 ONNX：**

```python
import onnx
from tvm.relax.frontend.onnx import from_onnx

onnx_model = onnx.load("mlp.onnx")
mod = from_onnx(onnx_model, keep_params_in_input=False)
```

两种方式下，你手里都是一个 Relax `IRModule`。**打印它**——正是这个习惯，把「用 TVM 的人」和「调试 TVM 的人」区分开来：

```python
mod.show()        # pretty-printed TVMScript: the Relax graph, ops still high-level
```

现在跑一遍**默认优化流水线**（融合、legalization 到 TIR，整套流程），然后再看一次：

```python
mod = relax.get_pipeline("zero")(mod)   # the "zero" pipeline: a sane default opt sequence
mod.show()                              # now you'll see TIR PrimFuncs appear — ops got lowered
```

你刚刚看到阶段 ② 和 ③ 发生：高层的 `R.matmul` / `R.nn.relu` 调用变成了 `R.call_tir`，落到具体的 `@T.prim_func` 循环嵌套，相邻的逐元素算子被融合。

**构建并运行。** `relax.build` 为某个 target 编译模块；得到的是一个 `Executable`，你把它加载到 **Relax Virtual Machine** 上：

```python
target = tvm.target.Target("llvm")        # or "cuda", "metal", "vulkan", ...
dev = tvm.cpu(0)                          # or tvm.cuda(0)

ex = relax.build(mod, target)            # ⑤ codegen → a loadable Executable
vm = relax.VirtualMachine(ex, dev)       # ⑥ runtime

x = tvm.nd.array(torch.randn(1, 784).numpy(), dev)
out = vm["main"](x)                      # call it like a function
print(out.numpy().shape)                 # (1, 10)
```

> **API 说明。** 近期的 TVM 暴露了一个统一入口 `tvm.compile(mod, target)`，它分发到 Relax 构建路径——就本文目的而言，两者等价。更早的教程用的是 `relax.build`；两者返回的对象都运行在 `relax.VirtualMachine` 上。如果你读的是 *Relay* 时代的教程，会看到 `relay.build(...)` 返回的是 `GraphModule`——思路相同，属于遗留路径。

这就是整条流程的实际执行：导入、检查、优化、再检查、构建、运行。后续每一讲都会放大到恰好是这条序列中的某一个阶段。

---

## 7. Target、代码生成与 runtime（预告）

流程底部的两个选择决定了其后的一切；这里先给它们命名，细节放在第 5 讲。

**Target** = 设备 + 代码生成后端。一个 `IRModule`，多个 target：

```text
"llvm"                         CPU via LLVM (x86, ARM, RISC-V — set -mtriple/-mcpu)
"cuda"                         NVIDIA GPU (PTX/cubin)
"rocm"                         AMD GPU
"metal"                        Apple GPU
"vulkan" / "opencl"            cross-vendor GPU
"webgpu"                       browser GPU  (this is how MLC-LLM runs in a tab)
"c"                            portable C source — the microTVM / bare-metal path
```

**Runtime** = 编译好的图如何被执行：

| Runtime | 模型 | 适用场景 |
|---|---|---|
| **GraphExecutor** | 静态图，提前内存规划 | 固定形状、经典推理；开销最低 |
| **Relax VM** | 字节码 VM，支持控制流与动态形状 | 大语言模型、动态模型，任何带 `if`/循环的 |
| **AOT** | 提前编译，无解释器，可被 C 调用 | 微控制器、嵌入式、无 Python 部署 |

固定形状的 ResNet 用 GraphExecutor。带不断增长的 KV-cache 的 Qwen 模型用 Relax VM。Cortex-M MCU 用 AOT。同一个编译器，三种 runtime——第 5 讲三种都会给出。

---


<details>
<summary>English original</summary>

**6. Hands-on: import, inspect, build, run**

Let's take a real model end to end. The pattern is identical whether the source is PyTorch or ONNX; the frontend differs, everything after is the same.

**From PyTorch (via `torch.export`):**

```python
import torch, torch.nn as nn
import tvm
from tvm import relax
from tvm.relax.frontend.torch import from_exported_program

class MLP(nn.Module):
    def __init__(self):
        super().__init__()
        self.fc1 = nn.Linear(784, 256)
        self.fc2 = nn.Linear(256, 10)
    def forward(self, x):
        return self.fc2(torch.relu(self.fc1(x)))

model = MLP().eval()
example = (torch.randn(1, 784),)

# torch.export gives a clean, functional graph; TVM imports it into Relax
exported = torch.export.export(model, example)
mod = from_exported_program(exported, keep_params_as_input=False)
```

**From ONNX:**

```python
import onnx
from tvm.relax.frontend.onnx import from_onnx

onnx_model = onnx.load("mlp.onnx")
mod = from_onnx(onnx_model, keep_params_in_input=False)
```

Either way you now hold a Relax `IRModule`. **Print it** — this is the habit that separates people who use TVM from people who debug it:

```python
mod.show()        # pretty-printed TVMScript: the Relax graph, ops still high-level
```

Now run the **default optimization pipeline** (fusion, legalization to TIR, the works) and look again:

```python
mod = relax.get_pipeline("zero")(mod)   # the "zero" pipeline: a sane default opt sequence
mod.show()                              # now you'll see TIR PrimFuncs appear — ops got lowered
```

You just watched stages ② and ③ happen: the high-level `R.matmul` / `R.nn.relu` calls became `R.call_tir` into concrete `@T.prim_func` loop nests, and adjacent elementwise ops got fused.

**Build and run.** `relax.build` compiles the module for a target; the result is an `Executable` you load onto the **Relax Virtual Machine**:

```python
target = tvm.target.Target("llvm")        # or "cuda", "metal", "vulkan", ...
dev = tvm.cpu(0)                          # or tvm.cuda(0)

ex = relax.build(mod, target)            # ⑤ codegen → a loadable Executable
vm = relax.VirtualMachine(ex, dev)       # ⑥ runtime

x = tvm.nd.array(torch.randn(1, 784).numpy(), dev)
out = vm["main"](x)                      # call it like a function
print(out.numpy().shape)                 # (1, 10)
```

> **API note.** Recent TVM exposes a unified front door, `tvm.compile(mod, target)`, that dispatches to the Relax build path — the two are equivalent for our purposes. Older tutorials use `relax.build`; both return something you run on `relax.VirtualMachine`. If you are reading a *Relay*-era tutorial you'll see `relay.build(...)` returning a `GraphModule` instead — same idea, legacy path.

That is the entire flow, executed. Import, inspect, optimize, inspect again, build, run. Every later lecture zooms into one stage of exactly this sequence.

---

**7. Targets, codegen, and runtimes (the preview)**

Two choices at the bottom of the flow shape everything downstream; we name them now and detail them in Lecture 5.

**Target** = the device + codegen backend. One `IRModule`, many targets:

```text
"llvm"                         CPU via LLVM (x86, ARM, RISC-V — set -mtriple/-mcpu)
"cuda"                         NVIDIA GPU (PTX/cubin)
"rocm"                         AMD GPU
"metal"                        Apple GPU
"vulkan" / "opencl"            cross-vendor GPU
"webgpu"                       browser GPU  (this is how MLC-LLM runs in a tab)
"c"                            portable C source — the microTVM / bare-metal path
```

**Runtime** = how the compiled graph is executed:

| Runtime | Model | Use it for |
|---|---|---|
| **GraphExecutor** | static graph, ahead-of-time memory plan | fixed-shape, classic inference; lowest overhead |
| **Relax VM** | bytecode VM, supports control flow & dynamic shape | LLMs, dynamic models, anything with `if`/loops |
| **AOT** | compiled-ahead, no interpreter, C callable | microcontrollers, embedded, no-Python deploy |

For a fixed-shape ResNet you want GraphExecutor. For a Qwen model with a growing KV-cache you want the Relax VM. For a Cortex-M MCU you want AOT. Same compiler, three runtimes — Lecture 5 ships all three.

---

</details>

## 8. TVM 在编译器版图中的位置

在任何 MLSys 面试中，你都会被问到：「为什么用 TVM 而不是 XLA / TensorRT / torch.compile？」诚实的答案在于**每个框架暴露了什么、又隐藏了什么**。

```text
                 exposes loop level?   open / multi-vendor?   auto-tuning?   edge+web+LLM path?
  TVM                    yes                   yes               yes (MS)            yes
  XLA                    no                    TPU+GPU           no                  partial
  TensorRT               no                    NVIDIA only       tactic search       no
  torch.compile          via Triton            GPU-centric       Triton autotune     no
  IREE/MLIR              yes (MLIR)            yes               limited             yes
```

TVM 的独特定位：它是唯一 (a) 把 **schedule** 作为一等、可调对象，(b) **开源且多目标**、向下覆盖到 MCU 和浏览器，(c) 具备成熟的**基于学习的调优器**（Lecture 3）与**新加速器接入通道**（BYOC + VTA，Lecture 4）的方案。代价是它对你的要求比 `torch.compile` 更高 —— 你更贴近底层硬件，而这正是本课程的意义。

这不是一场你能「赢」的竞赛。生产团队常常同时用 TensorRT 做 NVIDIA 专属的推理服务，*并且*用 TVM 覆盖边缘/冷门目标的长尾。了解 TVM 能让你掌握这一品类；其他的不过是其思想的子集。

---

## 9. 小实验：在单个模型上读通整条技术栈

任选一个小模型 —— 一个 MLP、一个小型 CNN，或单个 transformer block。

1. 将其导入 Relax（`from_exported_program` 或 `from_onnx`）。
2. 在任何 pass **之前** `mod.show()`。识别出高层算子。
3. 应用 `relax.get_pipeline("zero")`。**之后** `mod.show()`。找出一处两个算子被**融合**的地方，以及一个高层算子变成 `call_tir` 进入 `@T.prim_func` 的地方。
4. 为 `"llvm"` 构建，在 Relax VM 上运行，并将输出与 PyTorch 参考实现（`np.testing.assert_allclose`，`rtol=1e-3`）比对。
5. 将 target 改为 `"cuda"`（如果你有 GPU）并再次运行。**同一个 module，不同的 codegen** —— 注意你只改了一个字符串，别的什么都没动。

交付物：一份简短笔记，粘贴 TVMScript 的*改前*与*改后*，圈出融合后的算子和 `call_tir`，并附上两个 target 上精度一致性检查均通过的结果。这份笔记就是你读懂这条技术栈的证明 —— 也是为它做调度的前置要求。

---

## 关键要点

- 张量编译器之所以存在，是因为算子的快速实现取决于 shape × dtype × target × fusion × layout —— 这个空间太大，厂商库无法覆盖。TVM 为你所处的那个确切点生成 kernel。
- 这条栈有两个主要层级：**Relax**（图 IR —— 算什么）和 **TensorIR/TIR**（循环 IR —— 精确到哪个元素、哪块内存、什么顺序）。Target codegen 位于其下。
- **`IRModule`** 同时承载*所有*层级。图（Relax）与 kernel（TIR）函数共处一体、联合优化 —— 这就是「TVM Unity」，而 `R.call_tir` 是二者之间的桥梁。
- 流程是 import → graph passes → legalize/lower → schedule/tune → codegen → runtime。关键在于，**调度只改变循环顺序和内存映射，从不改变结果** —— 正是这一不变量让自动调优变得安全。
- **Relax** 取代了 **Relay**：符号 shape 一等公民、跨层级优化、专为 LLM 打造。写 Relax；把 `relay.build` 当作遗留产物。
- `mod.show()` 是你的显微镜。在每个 pass 前后都打印 module。

---

## 参考文献

- Apache TVM 文档 —— Overview 与「Quick Start」：[https://tvm.apache.org/docs/](https://tvm.apache.org/docs/)
- TVM Unity 愿景博文：[https://tvm.apache.org/2021/12/15/tvm-unity](https://tvm.apache.org/2021/12/15/tvm-unity)
- Relax / TVM Unity 文档（frontends、`relax.build`、VirtualMachine）：[https://tvm.apache.org/docs/reference/api/python/relax/relax.html](https://tvm.apache.org/docs/reference/api/python/relax/relax.html)
- TensorIR 深入剖析（blocks、`T.axis`、`call_tir`）：[https://tvm.apache.org/docs/deep_dive/tensor_ir/index.html](https://tvm.apache.org/docs/deep_dive/tensor_ir/index.html)
- Chen 等，"TVM: An Automated End-to-End Optimizing Compiler for Deep Learning"，OSDI 2018：[https://www.usenix.org/conference/osdi18/presentation/chen](https://www.usenix.org/conference/osdi18/presentation/chen)
- *AI Inference Engineer 2026* —— runtime 版图讲座，讲解 TVM 在推理服务栈中的位置。

---

*下一讲：[Lecture 02 —— TensorIR 与 schedule 空间](Lecture-02.md)*


<details>
<summary>English original</summary>

**8. Where TVM sits in the compiler landscape**

You will be asked, in any MLSys interview, "why TVM and not XLA / TensorRT / torch.compile?" The honest answer is about **what each one exposes and what it hides**.

```text
                 exposes loop level?   open / multi-vendor?   auto-tuning?   edge+web+LLM path?
  TVM                    yes                   yes               yes (MS)            yes
  XLA                    no                    TPU+GPU           no                  partial
  TensorRT               no                    NVIDIA only       tactic search       no
  torch.compile          via Triton            GPU-centric       Triton autotune     no
  IREE/MLIR              yes (MLIR)            yes               limited             yes
```

TVM's distinctive position: it is the one that (a) makes the **schedule** a first-class, tunable object, (b) is **open and multi-target** down to MCUs and browsers, and (c) carries a mature **learning-based tuner** (Lecture 3) and a **new-accelerator on-ramp** (BYOC + VTA, Lecture 4). The cost is that it asks more of you than `torch.compile` — you are closer to the metal, which is exactly the point of this course.

It is not a competition you "win." A production team often runs TensorRT for NVIDIA-only serving *and* TVM for the edge/odd-target tail. Knowing TVM teaches you the category; the others are subsets of its ideas.

---

**9. Mini-lab: read the whole stack on one model**

Pick any small model — an MLP, a small CNN, or a single transformer block.

1. Import it into Relax (`from_exported_program` or `from_onnx`).
2. `mod.show()` **before** any pass. Identify the high-level ops.
3. Apply `relax.get_pipeline("zero")`. `mod.show()` **after**. Find one place where two ops got **fused**, and one high-level op that became a `call_tir` into a `@T.prim_func`.
4. Build for `"llvm"`, run on the Relax VM, and check the output against the PyTorch reference (`np.testing.assert_allclose`, `rtol=1e-3`).
5. Change the target to `"cuda"` (if you have a GPU) and run again. **Same module, different codegen** — note that you changed one string and nothing else.

Deliverable: a short note pasting the *before* and *after* TVMScript with the fused op and the `call_tir` circled, plus the parity check passing on two targets. That note is proof you can read the stack — the prerequisite for scheduling it.

---

**Key takeaways**

- A tensor compiler exists because the fast implementation of an op depends on shape × dtype × target × fusion × layout — a space too large for vendor libraries to cover. TVM generates the kernel for the exact point you stand on.
- The stack has two main levels: **Relax** (graph IR — what computation) and **TensorIR/TIR** (loop IR — exactly which element, which memory, which order). Target codegen sits below.
- The **`IRModule`** holds *all* levels at once. Graph (Relax) and kernel (TIR) functions co-reside and are optimized together — this is "TVM Unity," and `R.call_tir` is the bridge.
- The flow is import → graph passes → legalize/lower → schedule/tune → codegen → runtime. Crucially, **scheduling changes only loop order and memory mapping, never the result** — the invariant that makes auto-tuning safe.
- **Relax** superseded **Relay**: symbolic shapes first-class, cross-level optimization, built for LLMs. Write Relax; treat `relay.build` as legacy.
- `mod.show()` is your microscope. Print the module before and after every pass.

---

**References**

- Apache TVM documentation — Overview & "Quick Start": [https://tvm.apache.org/docs/](https://tvm.apache.org/docs/)
- TVM Unity vision post: [https://tvm.apache.org/2021/12/15/tvm-unity](https://tvm.apache.org/2021/12/15/tvm-unity)
- Relax / TVM Unity docs (frontends, `relax.build`, VirtualMachine): [https://tvm.apache.org/docs/reference/api/python/relax/relax.html](https://tvm.apache.org/docs/reference/api/python/relax/relax.html)
- TensorIR deep dive (blocks, `T.axis`, `call_tir`): [https://tvm.apache.org/docs/deep_dive/tensor_ir/index.html](https://tvm.apache.org/docs/deep_dive/tensor_ir/index.html)
- Chen et al., "TVM: An Automated End-to-End Optimizing Compiler for Deep Learning," OSDI 2018: [https://www.usenix.org/conference/osdi18/presentation/chen](https://www.usenix.org/conference/osdi18/presentation/chen)
- *AI Inference Engineer 2026* — runtime landscape lecture, for where TVM sits among serving stacks.

---

*Next: [Lecture 02 — TensorIR and the schedule space](Lecture-02.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/TVM Deep Dives/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/TVM%20Deep%20Dives/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
