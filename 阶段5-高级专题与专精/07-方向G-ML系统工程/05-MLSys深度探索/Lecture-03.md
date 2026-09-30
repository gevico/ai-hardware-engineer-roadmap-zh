---
title: 第 03 讲 - 编译器与 runtime：从自动调度到 megakernel
description: 第 03 讲 - 编译器与 runtime：从自动调度到 megakernel
---

# 第 03 讲 - 编译器与 runtime：从自动调度到 megakernel

**合集：** [MLSys Deep Dives](README.md) | **上一讲：** [← 第 02 讲](Lecture-02.md) | **下一讲：** [第 04 讲](Lecture-04.md)

---

第 2 讲讲的是*编写*一个 kernel——由你来选择分块。本讲讲的是另一层：当你*不想*自己选，或者当有一万个 kernel 和整张图需要调度、融合并常驻在设备上时，由这一层来决定会发生什么。这就是 **编译器**（负责挑选或搜索调度）和 **runtime**（负责决定生成的 kernel 在大规模下如何实际执行）的职责。

我们巡览四个处于不同位置的编译器——**TVM**（搜索）、**Mojo/MAX**（一门完整语言）、**TensorRT-LLM**（闭源厂商）、**IREE**（可移植 MLIR）——然后转向 runtime 这条轴线，它正是 2026 年大量吞吐增益悄然来源之处：从逐算子发射转向**常驻 megakernel**，其代表是 **TileRT**。

---

## 学习目标

学完本讲，你应当能够：

1. 区分*手写 DSL*（第 2 讲）与*编译器选择的*调度，以及编译器做选择的三种方式：启发式、搜索、整门语言。
2. 概括 **TVM**（Relax/TensorIR/MetaSchedule/BYOC/MLC-LLM）作为自动调度这一极，并知道去哪里深入。
3. 把 **Mojo/MAX** 解释为“一门真正的语言，而非 eDSL”，以及其背后的碎片化论点。
4. 把 **TensorRT-LLM** 定位为闭源厂商这一极，并描述其 2025 年向 PyTorch 原生编写 + Dynamo 的转变。
5. 描述 **runtime 轴线**——eager → CUDA Graph → **常驻 megakernel**——以及为什么 **TileRT** 的 megakernel 设计在开销上取胜。
6. 针对给定工作负载，决定是手写、编译/搜索，还是购买闭源引擎。

---

## 1. 谁选择调度？

一个 kernel 的性能主要取决于它的**调度**——分块大小、循环顺序、共享内存中缓存什么、流水线如何重叠。第 2 讲的 DSL 把这一选择交到*你*手中（Triton 隐藏了一部分；ThunderKittens/CUTLASS 全部暴露）。编译器则把这一选择从你手中*拿走*，方式有三：

```text
   ① HEURISTIC   compiler applies expert default schedules, no search
                 (XLA fusion, TVM's dlight, torch.compile's Inductor)        → instant, ~good
   ② SEARCH      compiler measures many schedules on real hardware, keeps best
                 (TVM MetaSchedule, Ansor)                                    → slow, ~peak
   ③ WHOLE LANG  you write ONE language spanning kernel→graph→serving
                 (Mojo/MAX)                                                   → unified, new ecosystem
```

这是第 2 讲易用↔控制这条轴线的*另一半*。DSL 问的是“你想怎么调度？”；编译器则回答“让我来搞定”——通过启发式（快、可移植、约 80–90% 峰值）或通过搜索（慢、达峰值、针对特定目标）。两者没有绝对优劣；它们是同一个努力-vs-峰值权衡上的不同点，如今被自动化了。

---


<details>
<summary>English original</summary>

**Lecture 03 - Compilers and Runtimes: From Autoscheduling to Megakernels**

**Collection:** [MLSys Deep Dives](README.md) | **Previous:** [← Lecture 02](Lecture-02.md) | **Next:** [Lecture 04](Lecture-04.md)

---

Lecture 2 was about *writing* a kernel — you, choosing tiles. This lecture is about the layer that decides what happens when you *don't* want to choose, or when there are ten thousand kernels and a whole graph to schedule, fuse, and keep resident on the device. That is the job of **compilers** (which pick or search for schedules) and **runtimes** (which decide how the resulting kernels actually execute at scale).

We tour four compilers that sit at different points — **TVM** (search), **Mojo/MAX** (a whole language), **TensorRT-LLM** (closed vendor), **IREE** (portable MLIR) — and then the runtime axis that is quietly where a lot of 2026's throughput gains are coming from: the move from per-op launches to **persistent megakernels**, exemplified by **TileRT**.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Distinguish *hand-written DSL* (Lec 2) from *compiler-chosen* schedules, and the three ways compilers choose: heuristic, search, whole-language.
2. Summarize **TVM** (Relax/TensorIR/MetaSchedule/BYOC/MLC-LLM) as the autoscheduling pole, and know where to go for depth.
3. Explain **Mojo/MAX** as "a real language, not an eDSL," and the fragmentation argument behind it.
4. Place **TensorRT-LLM** as the closed-vendor pole and describe its 2025 shift toward PyTorch-native authoring + Dynamo.
5. Describe the **runtime axis** — eager → CUDA graph → **persistent megakernel** — and why **TileRT**'s megakernel design wins on overhead.
6. Decide, for a given workload, whether to hand-write, compile/search, or buy the closed engine.

---

**1. Who chooses the schedule?**

A kernel's performance is mostly its **schedule** — tile sizes, loop order, what's cached in shared memory, how the pipeline overlaps. Lecture 2's DSLs put that choice in *your* hands (Triton hides some; ThunderKittens/CUTLASS expose all). Compilers take the choice *away* from you, in one of three ways:

```text
   ① HEURISTIC   compiler applies expert default schedules, no search
                 (XLA fusion, TVM's dlight, torch.compile's Inductor)        → instant, ~good
   ② SEARCH      compiler measures many schedules on real hardware, keeps best
                 (TVM MetaSchedule, Ansor)                                    → slow, ~peak
   ③ WHOLE LANG  you write ONE language spanning kernel→graph→serving
                 (Mojo/MAX)                                                   → unified, new ecosystem
```

This is the *other half* of Lecture 2's ease↔control axis. The DSLs ask "how do you want this scheduled?"; the compilers answer "let me figure it out" — by heuristic (fast, portable, ~80–90% of peak) or by search (slow, peak, target-specific). Neither is strictly better; they're different points on the same effort-vs-peak trade, now automated.

---

</details>

## 2. TVM —— 自动调度这一极

**Apache TVM** 是 *搜索* 路线最完整的体现，也是最广覆盖的多目标开源编译器。一段话概括它的形态（本课程有一门完整的配套课程 —— [TVM Deep Dives](../06-TVM深度探索/README.md) —— 所以此处只保持高层视角）：

```text
   import model → Relax (graph IR, dynamic shapes) → lower to TensorIR (loop IR)
        → MetaSchedule SEARCHES schedules, measured on the real device
        → codegen (CUDA/LLVM/Metal/Vulkan/WebGPU/C) → runtime
        + BYOC: offload subgraphs to external codegen (CUTLASS/TensorRT/a custom NPU)
        + MLC-LLM: the whole stack pointed at LLMs, cross-platform
```

TVM 对本课程的论点为何重要：

* **它自己去搜索，而不是来问你。** MetaSchedule 生成候选 TensorIR 调度，用学习到的成本模型对其排序，在真实 GPU 上实测有希望的那些（如果是边缘板卡则通过 RPC），并保留胜出者。你用调优 *时间* 换取一个针对 *你的* 精确 shape 与硅片调优好的 kernel —— 这正是 TVM 能在厂商没有预料到的 shape 上击败厂商库的原因。
* **它是更新工具脚下的地基。** **TileLang**（第 2 讲）与 **TileRT**（§6）构建在 TVM 基础设施之上；**MLC-LLM** 则是把 TVM Unity 指向 LLM，横跨 CUDA/Metal/Vulkan/WebGPU。学会 TVM，你就学会了 2026 年技术栈中相当一部分的底层基质。
* **dlight** 是它的 *启发式* 模式：**无需** 调优的默认 GPU 调度，MLC-LLM 之所以用它，正是因为不可能在每个用户的手机 GPU 上跑搜索。TVM 同时包含两极 —— 启发式（dlight）与搜索（MetaSchedule）—— 而资深工程师的做法是知道在何处用哪一个。

当你需要 **跨众多目标的移植性**（包括边缘与 web），并愿意为峰值性能投入调优时间时，就用 TVM —— 或者当你在为 **新硬件** 打通后端时（它的 BYOC + autoschedule 路径就是入口）。要了解动手构建的细节，请上 [TVM Deep Dives](../06-TVM深度探索/README.md) 课程。

---

## 3. Mojo / MAX —— 一门真正的语言，而非 eDSL

**Modular** 的赌注（由 Chris Lattner 创立 —— LLVM、Swift、MLIR）是：第 2 讲里那整套「1,001 个 Python eDSL」的局面，只是一个缺失部件的症状：**一门面向加速器的真正的系统语言**。那门语言就是 **Mojo** —— 直接构建在 **MLIR** 之上的 Python 超集，设计目标是「Python 般易用，C++/Rust 般快」，用一套语法覆盖 CPU/GPU/加速器。

公平地陈述这一论点：Python eDSL（Triton、cuTile）「看起来像 Python，其实不是」—— 它是一个受限子集，会 trace 成 kernel，有自己的规则与失效模式。Mojo 则是一门 *完整* 的语言，因此同一份代码可以表达 Tensor Core kernel、一个模型以及围绕它的胶水代码，而无需跨越语言边界。自 2025 年年中起，Mojo 的 **标准库** 直接提供可移植的 GPU 编程（`gpu` 模块），Modular 还声称拥有最大的开放 CPU+GPU kernel 仓库之一。

**MAX** 是其上的产品：Modular 的 **推理引擎 + 图编译器 + 推理服务** 栈，自带用 *Mojo* 写成的 **FlashAttention/MLA** kernel，面向 NVIDIA Hopper/Blackwell 与 AMD MI 系列，声称在 Blackwell 上性能有竞争力。

开源时间线很重要，因为它是采用与否的主要问题：

```text
   2024-03   Mojo stdlib open-sourced (Apache 2.0)
   2025-11   all MAX Python API modules open-sourced (v25.7)
   2026-fall Modular's public commitment to open-source the Mojo LANGUAGE/compiler
```

在编译器开源之前，Mojo/MAX 是这里 **对 Modular 锁定最深** 的选项 —— 这就是现实风险。但这一论点（「别再造 eDSL 了，去造一门语言」）是对分块 DSL 碎片化最干净的反驳，即便你不采用它，也值得理解。当你想要 **从 kernel 到推理服务只用一门语言**，并且乐意把筹码押在 Modular 的栈与时间线上时，就选它。

---


<details>
<summary>English original</summary>

**2. TVM — the autoscheduling pole**

**Apache TVM** is the most complete embodiment of the *search* approach, and the broadest multi-target open compiler. The one-paragraph shape (this course has a whole companion course on it — [TVM Deep Dives](../06-TVM深度探索/README.md) — so we stay at altitude here):

```text
   import model → Relax (graph IR, dynamic shapes) → lower to TensorIR (loop IR)
        → MetaSchedule SEARCHES schedules, measured on the real device
        → codegen (CUDA/LLVM/Metal/Vulkan/WebGPU/C) → runtime
        + BYOC: offload subgraphs to external codegen (CUTLASS/TensorRT/a custom NPU)
        + MLC-LLM: the whole stack pointed at LLMs, cross-platform
```

What makes TVM matter for this course's thesis:

* **It searches instead of asking you.** MetaSchedule generates candidate TensorIR schedules, ranks them with a learned cost model, measures the promising ones on the actual GPU (over RPC if it's an edge board), and keeps the winner. You trade tuning *time* for a kernel tuned to *your* exact shape and silicon — which is how TVM can beat vendor libraries on shapes the vendor didn't anticipate.
* **It is the foundation under newer tools.** **TileLang** (Lec 2) and **TileRT** (§6) are built on TVM infrastructure; **MLC-LLM** is TVM Unity pointed at LLMs across CUDA/Metal/Vulkan/WebGPU. When you learn TVM you learn the substrate of a chunk of the 2026 stack.
* **dlight** is its *heuristic* mode: default GPU schedules with **no** tuning, used by MLC-LLM precisely because you cannot run a search on every user's phone GPU. TVM contains both poles — heuristic (dlight) and search (MetaSchedule) — and the senior move is knowing which to use where.

Reach for TVM when you need **portability across many targets** (including edge and web) and are willing to invest tuning time for peak — or when you're bringing up a backend for **new hardware** (its BYOC + autoschedule path is the on-ramp). For the build-it details, take the [TVM Deep Dives](../06-TVM深度探索/README.md) course.

---

**3. Mojo / MAX — a real language, not an eDSL**

**Modular's** bet (founded by Chris Lattner — LLVM, Swift, MLIR) is that the whole "1,001 Python eDSLs" situation from Lecture 2 is a symptom of a missing piece: **a real systems language for accelerators**. That language is **Mojo** — a Python-superset built directly on **MLIR**, designed to be "Python-easy, C++/Rust-fast," spanning CPU/GPU/accelerators in one syntax.

The argument, stated fairly: a Python eDSL (Triton, cuTile) "looks like Python but isn't" — it's a restricted subset that traces to a kernel, with its own rules and failure modes. Mojo is instead a *complete* language, so the same code can express a Tensor-Core kernel, a model, and the glue around it without crossing a language boundary. Since mid-2025 Mojo's **standard library** provides portable GPU programming directly (the `gpu` module), and Modular claims one of the largest open repos of CPU+GPU kernels.

**MAX** is the product on top: Modular's **inference engine + graph compiler + serving** stack, with its own FlashAttention/MLA kernels written *in Mojo*, targeting NVIDIA Hopper/Blackwell and AMD MI-series, claiming competitive Blackwell performance.

The open-source timeline matters because it's the main adoption question:

```text
   2024-03   Mojo stdlib open-sourced (Apache 2.0)
   2025-11   all MAX Python API modules open-sourced (v25.7)
   2026-fall Modular's public commitment to open-source the Mojo LANGUAGE/compiler
```

Until the compiler opens, Mojo/MAX is the **most Modular-locked** option here — that is the live risk. But the thesis ("stop writing eDSLs, write a language") is the cleanest counter-argument to the tile-DSL fragmentation, and worth understanding even if you don't adopt it. Reach for it when you want **one language from kernel to serving** and are comfortable betting on Modular's stack and timeline.

---

</details>

## 4. TensorRT-LLM —— 闭源厂商这一极

与 TVM 的开放搜索处于相反一端的，是 **TensorRT / TensorRT-LLM**（NVIDIA）。TensorRT 是一个闭源 **builder**：把模型喂进去，它就用 NVIDIA 的专有 kernel 生成硬件特定的 *engine*（fusion、精度校准、kernel 自动选择）。**TensorRT-LLM** 把这一能力专门化到 LLM 推理服务上：自定义 attention kernel、**in-flight（连续）批处理**、**paged KV cache**、量化（**FP8/FP4/INT4-AWQ/INT8 SmoothQuant**）、投机解码、多 GPU/多节点。

2025 年有两个变化值得了解，因为它们改变了它的使用方式：

* **PyTorch 原生编写。** TRT-LLM 从旧的「构建一个不透明 engine」流程，转向 **PyTorch 原生模型定义** + 模块化 Python runtime + 稳定的生产 API，并新增 **AutoDeploy**，以更少的人工转换来部署 PyTorch 模型。闭源的 kernel 依然闭源；*编写*环节则开放得多。
* **Dynamo + FlashInfer 上游化。** 它与 **NVIDIA Dynamo**（数据中心级分布式推理）集成，而且——值得注意的是——NVIDIA 现在**把自家顶尖的 LLM kernel 发布到开放的 FlashInfer 库**中（第 6 讲），供 vLLM/SGLang 复用。「在 NVIDIA 上想快只能用 TRT-LLM」这条护城河正在被侵蚀，因为开放技术栈（vLLM、SGLang）+ FlashInfer 正在缩小差距。

当部署是 **NVIDIA-only，且你希望以最少的 kernel 投入拿到同类最佳延迟**，并能接受闭源、不可移植、不可检视的 kernel 时，就选 TRT-LLM。*如果*你从不离开 NVIDIA，它就是生产力与峰值性能兼得的那一角——与 TVM 恰好相反的取舍。

---

## 5. IREE 与 MLIR 底座

第 2、3 讲中的几乎一切，往下都压着一层 **MLIR**——LLVM 项目的多级 IR，Triton、cuTile 的 Tile IR、Mojo、TVM 周边的工具链以及 IREE 都构建在其上。它是底座，不是 kernel 语言。

**IREE**（「Intermediate Representation Execution Environment」）是 MLIR 生态中的**可重定向编译器*兼* runtime**（属于 OpenXLA 的一部分）：AOT + JIT，跨度从数据中心 GPU 一直到移动/边缘，后端涵盖 CPU/GPU/加速器。它与 TVM「可移植编译器 + runtime」的目标重叠，但依赖的是 MLIR/linalg 方言栈，而不是 TVM 以 autoschedule 为中心的设计。如果你的组织已押注 MLIR（或以 JAX/OpenXLA 为中心），IREE 就是自然的编译器；如果你想要成熟的基于搜索的自动调优和 LLM 路线，TVM/MLC 更成熟。（而第 2 讲提到的 JAX 的 **Pallas/Mosaic**，正是同一生态中 TPU 一侧的 tile 路径。）

实际结论是：你很少会直接「选择 MLIR」——你选择的是编译器（TVM、IREE、Mojo、Triton），由它在底层选择 MLIR。知道它是共享底座，就能解释这些工具为何能互操作，以及 Tile-IR-for-Triton 后端当初为何可能实现。

---


<details>
<summary>English original</summary>

**4. TensorRT-LLM — the closed-vendor pole**

At the opposite end from TVM's open search sits **TensorRT / TensorRT-LLM** (NVIDIA). TensorRT is a closed **builder**: feed it a model, it emits a hardware-specific *engine* using NVIDIA's proprietary kernels (fusion, precision calibration, kernel auto-selection). **TensorRT-LLM** specializes that for LLM serving: custom attention kernels, **in-flight (continuous) batching**, **paged KV cache**, quantization (**FP8/FP4/INT4-AWQ/INT8 SmoothQuant**), speculative decoding, multi-GPU/multi-node.

Two 2025 shifts you should know, because they change how it's used:

* **PyTorch-native authoring.** TRT-LLM moved away from the old "build an opaque engine" flow toward **PyTorch-native model definitions** + a modular Python runtime + a stable production API, and added **AutoDeploy** to deploy PyTorch models with less manual conversion. The closed kernels stay closed; the *authoring* got far more open.
* **Dynamo + FlashInfer upstreaming.** It integrates with **NVIDIA Dynamo** (datacenter-scale distributed inference) and — notably — NVIDIA is now **publishing its top LLM kernels into the open FlashInfer library** (Lec 6) for reuse by vLLM/SGLang. The "only way to go fast on NVIDIA is TRT-LLM" moat is eroding as open stacks (vLLM, SGLang) + FlashInfer close the gap.

Reach for TRT-LLM when the deployment is **NVIDIA-only and you want best-in-class latency with least kernel effort**, and you accept closed, non-portable, non-inspectable kernels. It is the productivity-and-peak corner *if* you never leave NVIDIA — the opposite trade from TVM.

---

**5. IREE and the MLIR substrate**

One layer under almost everything in Lectures 2–3 is **MLIR** — the LLVM-project multi-level IR that Triton, cuTile's Tile IR, Mojo, TVM-adjacent tooling, and IREE all build on. It's the substrate, not a kernel language.

**IREE** ("Intermediate Representation Execution Environment") is the MLIR-ecosystem's **retargetable compiler *and* runtime** (part of OpenXLA): AOT + JIT, scaling from datacenter GPUs down to mobile/edge, CPU/GPU/accelerator backends. It overlaps TVM's "portable compiler + runtime" goal but leans on the MLIR/linalg dialect stack rather than TVM's autoschedule-centric design. If your organization is MLIR-committed (or JAX/OpenXLA-centric), IREE is the natural compiler; if you want mature search-based autotuning and the LLM path, TVM/MLC is more developed. (And JAX's **Pallas/Mosaic**, from Lec 2, is the TPU-side tile path in the same ecosystem.)

The practical takeaway: you rarely "choose MLIR" directly — you choose a compiler (TVM, IREE, Mojo, Triton) and it chooses MLIR underneath. Knowing it's the shared substrate explains why these tools interoperate and why a Tile-IR-for-Triton backend was even possible.

---

</details>

## 6. runtime 轴 —— 以及为何 megakernel 正在胜出

编译器产出 kernel；**runtime** 决定它们如何执行。2026 年相当可观的吞吐就藏在这里，因为朴素的执行模型在 kernel 之间白白浪费了 GPU。

```text
   ① EAGER (per-op launch)     [launch][op][launch][op][launch][op]...     ← gap before every op
                                                                              kernel-launch overhead
                                                                              + HBM round-trip per op
   ② CUDA GRAPH / GRAPH EXEC   [──── replay a captured graph, far fewer launches ────]
                                                                              amortizes launch cost
   ③ PERSISTENT MEGAKERNEL     [──────── one resident kernel; warps specialized ───────]
                                load / compute / comm OVERLAPPED inside; µs-scale overhead,
                                pipeline stays GPU-resident across the whole forward pass
```

趋势向右。在小 batch、短 decode 步长下 —— 正是 LLM decode（逐 token 生成阶段）所处的带宽受限区间 —— **kernel 启动开销与算子间的 HBM 往返会占据步长时间的一大部分**。把它量化，因为在你量化之前这个说法听起来很抽象：

```text
   batch-1 decode step, 70B-class transformer (illustrative):
   ~80 layers × ~8–10 unfused kernels/layer  ≈  600–800 launches per token
   × ~2–5 µs launch + teardown each          ≈  1.5–4 ms of pure gap per step
   vs a TPOT budget of ~10–20 ms             →  10–30% of the step is nobody-computing time
   …and at every kernel boundary the activations spill to HBM and come back,
   paying bandwidth the roofline never required.
```

捕获图（CUDA Graphs）能收回大部分 *launch* 开销；把*整个*模型融合成**单个常驻的“megakernel”**还能收回*往返*开销，因为算子之间数据从不离开片上内存，启动间隙也随之消失。注意这种区间依赖性：在 batch 128 的 prefill（首字前的整段计算）下，同样的间隙不过是巨大 GEMM（矩阵-矩阵乘）背后的噪声 —— 这正是 megakernel 属于 *decode/延迟*叙事、而非普适方案的原因。

**TileRT**（出自 `tile-ai` 团队，与 TileLang 同源）是 2026 年的范例：一个**常驻 megakernel runtime**，把 LLM 算子分解成**细粒度的分块级任务**，并在多 GPU 间动态地**让计算、I/O 和通信重叠**，同时用 **warp 特化**让整条流水线常驻、开销维持在微秒量级。它是你在第 6 讲会遇到的那个头条吞吐结果背后的 runtime（一个 1 万亿参数模型在单台 8-GPU 通用节点上突破 **1000 tokens/s**）。眼下先记住这条原则：**一旦 kernel 够快，下一个瓶颈就是 kernel 之间的间隙，而 megakernel 能把这些间隙合上。**

---

## 7. 选择 —— 决策表

| 你想要… | 该选 | 为什么 |
|---|---|---|
| 跨多种目标的移植性 + 靠调优榨取峰值 | **TVM** (MetaSchedule) | 按 shape/device 搜索调度；边缘→web→LLM |
| 在任何 backend 上即时得到不错的调度，无需调优 | **TVM dlight** / `torch.compile` | 启发式调度；马上能用 |
| 从 kernel → 模型 → 推理服务只用一种语言 | **Mojo / MAX** | 一门真正的语言，而非 eDSL（目前锁定在 Modular 上） |
| 最佳的 NVIDIA 延迟、最少的 kernel 工作量、仅限 NV | **TensorRT-LLM** | 闭源厂商 kernel + 推理服务特性 |
| 与 MLIR/OpenXLA 对齐的可移植编译器 + runtime | **IREE** | 可重定向，server→边缘，MLIR 原生 |
| 消灭快 kernel 之间的间隙 | **megakernel runtime (TileRT)** | 常驻、重叠、µs 级开销 |

而元决策 —— **手写（第 2 讲）vs. 编译/搜索 vs. 买闭源引擎** —— 归结为三个问题：*我有多少种 shape/目标？*（多 → 编译），*我的成本有多少集中在单个 kernel 上？*（集中 → 手写那一个），*我是否只用 NVIDIA 且对延迟敏感？*（是 → TRT-LLM 很难被击败）。成熟的栈三者混用：推理服务用 TRT-LLM 或 vLLM，热点路径用手写的 ThunderKittens attention kernel，零散目标用 TVM/MLC，再用 megakernel runtime 合上间隙。

---

## 8. 测量它

与第 2 讲同样的纪律，现在放到模型层面。把一个模型用两种方式编译，并把数字一路带到美元：

```text
   path A: torch.compile (Inductor → Triton)   → tokens/s, $/Mtok
   path B: TVM + MetaSchedule (tuned)           → tokens/s, $/Mtok, + tuning time
   path C: TensorRT-LLM (built engine)          → tokens/s, $/Mtok, TTFT
   (optional) wrap the winner in a megakernel/graph runtime → tokens/s delta from killing gaps
```

对每条路径报告 tokens/s、TTFT、TPOT 和 `$/Mtok`，再加上每条路径向你收取的**一次性成本**（TVM 的调优工时、TRT-LLM 的构建 + NVIDIA 锁定、Mojo 的生态赌注）。正确答案很少是“某个编译器”，而是“工作负载的这一部分走这条路径”，并由那张表来论证。

---


<details>
<summary>English original</summary>

**6. The runtime axis — and why megakernels are winning**

Compilers produce kernels; the **runtime** decides how they execute. This is where a surprising amount of 2026 throughput is hiding, because the naive execution model wastes the GPU between kernels.

```text
   ① EAGER (per-op launch)     [launch][op][launch][op][launch][op]...     ← gap before every op
                                                                              kernel-launch overhead
                                                                              + HBM round-trip per op
   ② CUDA GRAPH / GRAPH EXEC   [──── replay a captured graph, far fewer launches ────]
                                                                              amortizes launch cost
   ③ PERSISTENT MEGAKERNEL     [──────── one resident kernel; warps specialized ───────]
                                load / compute / comm OVERLAPPED inside; µs-scale overhead,
                                pipeline stays GPU-resident across the whole forward pass
```

The trend is rightward. At small batch and short decode steps — exactly the memory-bound regime where LLM decode lives — **kernel-launch overhead and inter-op HBM round-trips become a large fraction of the step time**. Put numbers on it, because the claim sounds abstract until you do:

```text
   batch-1 decode step, 70B-class transformer (illustrative):
   ~80 layers × ~8–10 unfused kernels/layer  ≈  600–800 launches per token
   × ~2–5 µs launch + teardown each          ≈  1.5–4 ms of pure gap per step
   vs a TPOT budget of ~10–20 ms             →  10–30% of the step is nobody-computing time
   …and at every kernel boundary the activations spill to HBM and come back,
   paying bandwidth the roofline never required.
```

Capturing the graph (CUDA Graphs) reclaims most of the *launch* cost; fusing the *entire* model into a **single persistent "megakernel"** reclaims the *round-trips* too, because the data never leaves on-chip memory between ops and the launch gaps vanish. Note the regime-dependence: at batch 128 prefill those same gaps are noise behind big GEMMs — which is why megakernels are a *decode/latency* story, not a universal one.

**TileRT** (from the `tile-ai` group, same lineage as TileLang) is the 2026 exemplar: a **persistent-megakernel runtime** that decomposes LLM operators into **fine-grained tile-level tasks** and dynamically **overlaps compute, I/O, and communication** across GPUs, with **warp specialization** keeping the whole pipeline resident and overhead at microsecond scale. It is the runtime behind a headline throughput result you'll meet in Lecture 6 (a 1-trillion-parameter model pushed past **1000 tokens/s** on a single 8-GPU commodity node). For now, hold the principle: **once kernels are fast, the next bottleneck is the gaps between them, and megakernels close the gaps.**

---

**7. Choosing — the decision table**

| You want… | Reach for | Why |
|---|---|---|
| Portability across many targets + peak via tuning | **TVM** (MetaSchedule) | searches schedules per shape/device; edge→web→LLM |
| Instant good schedules, no tuning, on any backend | **TVM dlight** / `torch.compile` | heuristic schedules; ship now |
| One language from kernel → model → serving | **Mojo / MAX** | a real language, not an eDSL (Modular-locked for now) |
| Best NVIDIA latency, least kernel effort, NV-only | **TensorRT-LLM** | closed vendor kernels + serving features |
| MLIR/OpenXLA-aligned portable compiler+runtime | **IREE** | retargetable, server→edge, MLIR-native |
| Kill the gaps between fast kernels | **megakernel runtime (TileRT)** | persistent, overlapped, µs overhead |

And the meta-decision — **hand-write (Lec 2) vs. compile/search vs. buy-the-closed-engine** — comes down to three questions: *How many shapes/targets?* (many → compile), *How much of my cost is in one kernel?* (concentrated → hand-write that one), *Am I NVIDIA-only and latency-critical?* (yes → TRT-LLM is hard to beat). A mature stack mixes all three: TRT-LLM or vLLM for serving, a hand-written ThunderKittens attention kernel for the hot path, TVM/MLC for the odd target, a megakernel runtime to close the gaps.

---

**8. Measure it**

Same discipline as Lecture 2, now at the model level. Compile one model two ways and carry the number to dollars:

```text
   path A: torch.compile (Inductor → Triton)   → tokens/s, $/Mtok
   path B: TVM + MetaSchedule (tuned)           → tokens/s, $/Mtok, + tuning time
   path C: TensorRT-LLM (built engine)          → tokens/s, $/Mtok, TTFT
   (optional) wrap the winner in a megakernel/graph runtime → tokens/s delta from killing gaps
```

Report tokens/s, TTFT, TPOT, and `$/Mtok` for each, plus the **one-time cost** each path charged you (TVM's tuning hours, TRT-LLM's build + NVIDIA lock-in, Mojo's ecosystem bet). The right answer is rarely "one compiler" — it's "this path for this part of the workload," justified by the table.

---

</details>

## 9. Mini-lab：三个编译器，一个模型

拿一个能端到端跑起来的小模型。

1. **基线：** eager PyTorch。记录 tokens/s、TTFT、TPOT、`$/Mtok`。
2. **启发式编译：** `torch.compile`。记录增量；注意底下的 kernel 是 Triton（Lec 2）。
3. **搜索式编译：** 用 **TVM MetaSchedule** 调优模型（或对即时路径应用 dlight）。记录增量*以及*你花掉的调优时间。
4. **闭源引擎（如果在 NVIDIA 上）：** 构建一个 **TensorRT-LLM** 引擎。记录增量和 TTFT。
5. **对差距做推理：** profile 最快的那条路径；估算 kernel *之间*的时间（launch + idle）占多少。这个差距就是 megakernel 的机会。

交付物：一张 `{eager, torch.compile, TVM, TRT-LLM}` × `{tokens/s, TTFT, TPOT, $/Mtok, one-time cost}` 表，外加一段说明：你会 ship 哪条路径、为什么，以及 kernel 间空隙还有多少余量。这个综合就是本讲。

---

## 关键要点

- DSL（Lec 2）把 schedule 放到*你*手里；**编译器替你选它**——靠**启发式**（即时，约等于好）、靠**搜索**（慢，约等于峰值），或者给你一**整门语言**（Mojo）。
- **TVM** 是自动调度这一极：Relax + TensorIR + **MetaSchedule**（搜索）与 **dlight**（启发式），目标广泛，是 TileLang/TileRT/MLC-LLM 底下的基座。深度内容在配套的 [TVM Deep Dives](../06-TVM深度探索/README.md) 课程里。
- **Mojo/MAX** 用「一门真语言」回答 eDSL 碎片化问题，覆盖 kernel→模型→推理服务——强大、基于 MLIR，但在编译器开放前锁定 Modular（已承诺 2026 年秋）。
- **TensorRT-LLM** 是闭源厂商这一极：NVIDIA 上的峰值延迟、最少的 kernel 工作量、仅限 NV；2025 年把编写方式变成 PyTorch 原生，并开始把 kernel 上游贡献到开源的 FlashInfer。
- **runtime 轴**从 eager → CUDA Graph → **persistent megakernel** 演进。一旦 kernel 变快，它们之间的空隙就占主导；**TileRT** 的 megakernel 设计把这些空隙合上（1000-tok/s 里程碑背后的引擎，Lec 6）。
- 没有单一赢家——把手写 kernel、编译器、闭源引擎和 megakernel runtime 混着用，各自用 tokens/s 和 `$/Mtok` 来说明理由。

---

## 参考文献

- Apache TVM（Relax/Unity、MetaSchedule、MLC-LLM）：[https://tvm.apache.org/docs/](https://tvm.apache.org/docs/) ——以及配套的 [TVM Deep Dives](../06-TVM深度探索/README.md) 课程。
- Modular Mojo & MAX：[https://www.modular.com/open-source/mojo](https://www.modular.com/open-source/mojo) · MAX changelog [https://docs.modular.com/max/changelog/](https://docs.modular.com/max/changelog/)
- NVIDIA TensorRT-LLM：[https://github.com/NVIDIA/TensorRT-LLM](https://github.com/NVIDIA/TensorRT-LLM) · overview [https://nvidia.github.io/TensorRT-LLM/overview.html](https://nvidia.github.io/TensorRT-LLM/overview.html)
- IREE（OpenXLA，MLIR 编译器 + runtime）：[https://github.com/iree-org/iree](https://github.com/iree-org/iree)
- TileRT（tile-ai，persistent-megakernel runtime）：[https://github.com/tile-ai/TileRT](https://github.com/tile-ai/TileRT)
- NVIDIA，《从 NVIDIA 使用 FlashInfer 运行高性能 LLM 推理 kernel》：https://developer.nvidia.com/blog/run-high-performance-llm-inference-kernels-from-nvidia-using-flashinfer/](https://developer.nvidia.com/blog/run-high-performance-llm-inference-kernels-from-nvidia-using-flashinfer/)

---

## 截至

2026-06。锁定：TVM Unity/Relax mainline（MetaSchedule + dlight）；Mojo stdlib 开源（2024），MAX Python 模块开源（v25.7，2025 年 11 月），Mojo 语言开源已承诺「2026 年秋」；TensorRT-LLM PyTorch 原生 + AutoDeploy + Dynamo；TileRT 预览版（tile-ai）。Megakernel 的吞吐声明依工作负载而异——锁定的 MiMo/TileRT 数字及其厂商报告的注意事项见 Lecture 6。

---

*下一讲：[Lecture 04 — Beyond the dense transformer](Lecture-04.md)*


<details>
<summary>English original</summary>

**9. Mini-lab: three compilers, one model**

Take a small model you can run end to end.

1. **Baseline:** eager PyTorch. Record tokens/s, TTFT, TPOT, `$/Mtok`.
2. **Heuristic compile:** `torch.compile`. Record the deltas; note that the kernels underneath are Triton (Lec 2).
3. **Search compile:** tune the model with **TVM MetaSchedule** (or apply dlight for the instant path). Record deltas *and* the tuning time you spent.
4. **Closed engine (if on NVIDIA):** build a **TensorRT-LLM** engine. Record deltas and TTFT.
5. **Reason about gaps:** profile the fastest path; estimate how much time is *between* kernels (launch + idle). That gap is the megakernel opportunity.

Deliverable: a `{eager, torch.compile, TVM, TRT-LLM}` × `{tokens/s, TTFT, TPOT, $/Mtok, one-time cost}` table, plus a paragraph on which path you'd ship and why, and how much headroom the inter-kernel gaps still hold. That synthesis is the lecture.

---

**Key takeaways**

- DSLs (Lec 2) put the schedule in *your* hands; **compilers choose it for you** — by **heuristic** (instant, ~good), **search** (slow, ~peak), or by giving you a **whole language** (Mojo).
- **TVM** is the autoscheduling pole: Relax + TensorIR + **MetaSchedule** (search) and **dlight** (heuristic), broad targets, the substrate under TileLang/TileRT/MLC-LLM. Depth lives in the companion [TVM Deep Dives](../06-TVM深度探索/README.md) course.
- **Mojo/MAX** answers eDSL fragmentation with "a real language" spanning kernel→model→serving — powerful, MLIR-based, but Modular-locked until the compiler opens (committed fall 2026).
- **TensorRT-LLM** is the closed-vendor pole: peak NVIDIA latency, least kernel effort, NV-only; 2025 made authoring PyTorch-native and began upstreaming kernels into open FlashInfer.
- The **runtime axis** moves eager → CUDA graph → **persistent megakernel**. Once kernels are fast, the gaps between them dominate; **TileRT**'s megakernel design closes them (the engine behind a 1000-tok/s milestone, Lec 6).
- There's no single winner — mix hand-written kernels, a compiler, a closed engine, and a megakernel runtime, each justified by tokens/s and `$/Mtok`.

---

**References**

- Apache TVM (Relax/Unity, MetaSchedule, MLC-LLM): [https://tvm.apache.org/docs/](https://tvm.apache.org/docs/) — and the companion [TVM Deep Dives](../06-TVM深度探索/README.md) course.
- Modular Mojo & MAX: [https://www.modular.com/open-source/mojo](https://www.modular.com/open-source/mojo) · MAX changelog [https://docs.modular.com/max/changelog/](https://docs.modular.com/max/changelog/)
- NVIDIA TensorRT-LLM: [https://github.com/NVIDIA/TensorRT-LLM](https://github.com/NVIDIA/TensorRT-LLM) · overview [https://nvidia.github.io/TensorRT-LLM/overview.html](https://nvidia.github.io/TensorRT-LLM/overview.html)
- IREE (OpenXLA, MLIR compiler + runtime): [https://github.com/iree-org/iree](https://github.com/iree-org/iree)
- TileRT (tile-ai, persistent-megakernel runtime): [https://github.com/tile-ai/TileRT](https://github.com/tile-ai/TileRT)
- NVIDIA, "Run high-performance LLM inference kernels from NVIDIA using FlashInfer": [https://developer.nvidia.com/blog/run-high-performance-llm-inference-kernels-from-nvidia-using-flashinfer/](https://developer.nvidia.com/blog/run-high-performance-llm-inference-kernels-from-nvidia-using-flashinfer/)

---

**Current as of**

2026-06. Pins: TVM Unity/Relax mainline (MetaSchedule + dlight); Mojo stdlib open (2024), MAX Python modules open (v25.7, Nov 2025), Mojo language open-source committed "fall 2026"; TensorRT-LLM PyTorch-native + AutoDeploy + Dynamo; TileRT preview (tile-ai). Megakernel throughput claims are workload-specific — see Lecture 6 for the pinned MiMo/TileRT figures and their vendor-reported caveat.

---

*Next: [Lecture 04 — Beyond the dense transformer](Lecture-04.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/MLSys Deep Dives/Lecture-03.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/MLSys%20Deep%20Dives/Lecture-03.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
