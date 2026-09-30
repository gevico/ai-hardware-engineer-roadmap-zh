---
title: TVM 深入解析 —— 面向 MLSys 工程师的 Apache TVM
description: TVM 深入解析 —— 面向 MLSys 工程师的 Apache TVM
---

# TVM 深入解析 —— 面向 MLSys 工程师的 Apache TVM

<div class="course-identity mlsys" markdown="1">
<div class="course-identity__icon">TVM</div>
<div markdown="1">
<p class="course-identity__eyebrow">阶段 5 · ML 系统工程 · 专题课程</p>
<p class="course-identity__title">把模型编译到硬件底层：Apache TVM 栈 —— Relax、TensorIR、MetaSchedule、BYOC 与 MLC-LLM —— 一位资深 MLSys 工程师实际使用它的方式。</p>
<p class="course-identity__meta">产物：一个调优并部署好的编译模型，附实测加速比报告 · 度量：延迟、GFLOP/s 对 roofline（性能上界模型）、调优试验次数、二进制体积、$/推理</p>
</div>
</div>

> *框架跑的是别人优化好的图。编译器让你成为那个“别人”。*

多数 ML 工程师止步于框架边界。他们调用 `model(x)`，框架分派给 cuDNN 或 oneDNN，硬件就按厂商库决定的方式去做。这没问题，直到你撞上一个 shape、一个 dtype、一个融合模式，或者一个**没有任何厂商库覆盖**的加速器 —— 自研 NPU、边缘 MCU（微控制器）、WebGPU 目标、没人交付过 kernel 的量化方案。在那个边界上，你不再是 kernel 的使用者，而成为它的作者。**Apache TVM 就是用来大规模做这件事的开源编译器栈**，横跨整个 target 动物园，无需为每个 shape、每台设备手写 kernel。

本课程讲的是 MLSys 工程师如何读懂、驱动、扩展并交付这套栈。课程**以构建为先**：每一讲都会 lower 一个真实计算，对它做调度、调优或部署，再从另一侧读出一个数字。

**层级映射：** L4（编译器 / 图优化）是主干，向下触及 L3（kernel / 代码生成），向上触及 L5–L6（runtime / 部署）。这是核心的 L4 编译器工程师技能组合。

**目标角色：** ML 编译器工程师 · MLSys 工程师 · AI 推理工程师 · 边缘 AI / TinyML 工程师 · 加速器软件工程师（为新芯片编写 BYOC 后端的那个人）。

**前置要求：**

* 阶段 4 — 方向 C — [ML 编译器与图优化](../../../阶段4-路线C-深度学习推理优化/Guide.md) —— 什么是图 IR、算子和 lowering pass。
* 阶段 5 — 边缘 AI — [边缘 LLM 推理内幕](../../03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md) —— GEMV（矩阵-向量乘）vs GEMM（矩阵-矩阵乘）、roofline、decode（逐 token 生成阶段）瓶颈。要读懂本文每一节“Measure it”，你需要 roofline。
* 熟悉 Python（整个 TVM 前端都是 Python），并能读懂 CUDA C 和 LLVM 风格的循环代码。不预设你具备编译器内部知识 —— 那正是本课程要建立的。

**后续产出：** 一个编译模型仓库 —— 一个网络，用 MetaSchedule 针对至少两个 target 完成 lower 与调优（如 x86 + CUDA，或 CUDA + 一块边缘板卡），并附一张 benchmark 表，对比框架基线、未调优的 TVM 构建和调优后的 TVM 构建相对硬件 roofline 的表现，外加一个 BYOC offload 实验。

---

## 为什么选 TVM，以及为什么是现在

TVM 是最早、也最通用的开源 ML 编译器（2017 年诞生于 UW），也是其思想 —— **可调度的张量 IR、基于学习的自动调优、自带代码生成（bring-your-own-codegen）** —— 传播到之后一切事物中的那一个。了解 TVM 是理解*整个*品类最快的途径，因为其他编译器在很大程度上只是它某些部分的重新表述。

| 编译器 | 图 IR | 张量 IR / 调度 | 自动调优 | 优势 | TVM 的差异点 |
|---|---|---|---|---|---|
| **Apache TVM** | Relax (Unity)；Relay (legacy) | TensorIR (TIR) + 调度 | MetaSchedule / Ansor / AutoTVM | target 覆盖面广、开放的加速器路径（BYOC、VTA）、edge → web → LLM | 本表的参照点 |
| **XLA** (JAX/TF) | HLO | 隐式（融合 + libnvjit） | 无（启发式） | TPU、全程序融合 | TVM 暴露调度；XLA 把它藏起来 |
| **TensorRT** | builder 图 | 闭源 kernel | builder tactics | NVIDIA 峰值性能 | 闭源、单一厂商；TVM 开放且多 target |
| **torch.compile / Inductor** | FX 图 | Triton + C++ | autotune（Triton configs） | PyTorch 原生、迭代快 | TVM 能把独立产物交付到非 Python、非 GPU 的 target |
| **IREE / MLIR** | linalg-on-tensors | 嵌套的 MLIR 方言 | 有限 | 移动/边缘、MLIR 生态 | 目标重叠；TVM 的调优 + LLM 路径（MLC）更成熟 |

在 2026 年专门学它的理由：**MLC-LLM** —— 那个在 Metal、Vulkan、WebGPU、ROCm 和手机上跑 Llama / Qwen / Phi 级模型的项目 —— 就*构建在 TVM Unity 之上*。你在第 4–5 讲学到的同一套 Relax + TIR + dlight 栈，正是编译那些模型的东西。TVM 是「我理解 Transformer 的执行」与「我能把这个 Transformer 交付到没人写过 kernel 的设备上」之间的桥梁。

---


<details>
<summary>English original</summary>

**TVM Deep Dives — Apache TVM for MLSys Engineers**

<div class="course-identity mlsys" markdown="1">
<div class="course-identity__icon">TVM</div>
<div markdown="1">
<p class="course-identity__eyebrow">Phase 5 · ML Systems Engineering · Special Course</p>
<p class="course-identity__title">Compile a model to the metal: the Apache TVM stack — Relax, TensorIR, MetaSchedule, BYOC, and MLC-LLM — as a senior MLSys engineer actually uses it.</p>
<p class="course-identity__meta">Artifact: a tuned, deployed compiled model with a measured speedup report · Measure: latency, GFLOP/s vs roofline, tuning trials, binary size, $/inference</p>
</div>
</div>

> *A framework runs the graph someone else optimized. A compiler lets you become that someone.*

Most ML engineers stop at the framework boundary. They call `model(x)`, the framework dispatches to cuDNN or oneDNN, and the hardware does what the vendor library decided it should do. That is fine until you hit a shape, a dtype, a fused pattern, or an accelerator that **no vendor library covers** — a custom NPU, an edge MCU, a WebGPU target, a quantization scheme nobody shipped a kernel for. At that boundary you stop being a user of kernels and start being an author of them. **Apache TVM is the open compiler stack for doing that at scale**, across the whole target zoo, without hand-writing a kernel per shape per device.

This course is how an MLSys engineer reads, drives, extends, and ships that stack. It is **build-first**: every lecture lowers a real computation, schedules it, tunes it, or deploys it, and reads a number off the other side.

**Layer mapping:** L4 (compiler / graph optimization) is the spine, reaching down into L3 (kernels / codegen) and up into L5–L6 (runtime / deployment). This is the core L4 compiler-engineer skill set.

**Role targets:** ML Compiler Engineer · MLSys Engineer · AI Inference Engineer · Edge AI / TinyML Engineer · Accelerator Software Engineer (the person who writes the BYOC backend for a new chip).

**Prerequisites:**

* Phase 4 — Track C — [ML Compiler and Graph Optimization](../../../阶段4-路线C-深度学习推理优化/Guide.md) — what a graph IR, an operator, and a lowering pass are.
* Phase 5 — Edge AI — [Edge LLM Inference Internals](../../03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md) — GEMV vs GEMM, the roofline, the decode bottleneck. You will need the roofline to read every "Measure it" section here.
* Comfort with Python (the whole TVM frontend is Python) and the ability to read CUDA C and LLVM-flavored loop code. No prior compiler-internals knowledge assumed — that is what this course builds.

**What comes after:** a compiled-model repo — one network, lowered and tuned with MetaSchedule for at least two targets (e.g. x86 + CUDA, or CUDA + an edge board), with a benchmark table comparing the framework baseline, the un-tuned TVM build, and the tuned TVM build against the hardware roofline, plus one BYOC offload experiment.

---

**Why TVM, and why now**

TVM is the oldest and most general of the open ML compilers (born at UW in 2017), and the one whose ideas — **a schedulable tensor IR, learning-based auto-tuning, bring-your-own-codegen** — propagated into everything that came after. Knowing TVM is the fastest way to understand the *whole* category, because the others are largely re-expressions of pieces of it.

| Compiler | Graph IR | Tensor IR / scheduling | Auto-tuning | Strength | Where TVM differs |
|---|---|---|---|---|---|
| **Apache TVM** | Relax (Unity); Relay (legacy) | TensorIR (TIR) + schedules | MetaSchedule / Ansor / AutoTVM | Breadth of targets, open accelerator path (BYOC, VTA), edge → web → LLM | The reference point for this table |
| **XLA** (JAX/TF) | HLO | implicit (fusion + libnvjit) | none (heuristics) | TPU, whole-program fusion | TVM exposes the schedule; XLA hides it |
| **TensorRT** | builder graph | closed kernels | builder tactics | NVIDIA peak perf | Closed, single-vendor; TVM is open + multi-target |
| **torch.compile / Inductor** | FX graph | Triton + C++ | autotune (Triton configs) | PyTorch-native, fast iteration | TVM ships standalone artifacts to non-Python, non-GPU targets |
| **IREE / MLIR** | linalg-on-tensors | nested MLIR dialects | limited | mobile/edge, MLIR ecosystem | Overlapping goals; TVM's tuning + LLM path (MLC) are more mature |

The 2026-relevant reason to learn it specifically: **MLC-LLM** — the project that runs Llama / Qwen / Phi-class models on Metal, Vulkan, WebGPU, ROCm, and phones — is *built on TVM Unity*. The same Relax + TIR + dlight stack you learn in Lectures 4–5 is what compiles those models. TVM is the bridge between "I understand transformer execution" and "I can ship that transformer to a device nobody wrote a kernel for."

---

</details>

## 课程地图（5 讲）

整条主线就是编译器本身，自上而下再自下而上：读懂整条栈 → 调度一个 kernel → 让机器去调度它 → 优化计算图并接入外部 codegen → 发布产物。

<div class="lecture-map" markdown>

| # | 讲 | 你构建 / 测量的内容 |
|---|---------|--------------------------|
| [01](Lecture-01.md) | **TVM 技术栈与编译流程** — Relax、TensorIR，以及统一的 `IRModule` | 导入一个模型，打印每一层 IR，构建并运行；厘清 import → optimize → lower → codegen → runtime 流水线 |
| [02](Lecture-02.md) | **TensorIR 与调度空间** — 把计算定义变成映射到硬件的 kernel | 为 CPU 调度一个 matmul（分块 + 向量化），为 GPU 调度一个 matmul（block/thread 绑定 + 共享内存缓存）；`tensorize` 到 Tensor Core intrinsic；前后的 GFLOP/s |
| [03](Lecture-03.md) | **自动调优** — AutoTVM → Ansor → MetaSchedule，基于学习的编译器 | `ms.tune_tir` 一个 kernel、`ms.tune_relax` 一个模型；读懂调优曲线；通过 RPC tracker 调优一块远程边缘开发板 |
| [04](Lecture-04.md) | **深入 Relax** — 动态 shape、算子融合，以及 Bring Your Own Codegen（BYOC） | 给模型做符号 shape；用 `FuseOps`/`FuseTIR` 做融合；把子图下推到 CUTLASS/TensorRT，并测量融合 vs 非融合、下推 vs 原生 |
| [05](Lecture-05.md) | **把它发布出去** — runtime、microTVM，以及用 MLC-LLM 跑 LLM | 打包一个模块（GraphExecutor / Relax VM / AOT）；用 microTVM 把一个小模型部署到裸机 MCU；用 MLC-LLM 在多个后端上编译 + 运行一个 LLM |

</div>

---

## 课程产出

学完之后，你应当能够：

* 在每一层读懂 TVM `IRModule` — Relax 计算图、TIR `PrimFunc`、生成的 CUDA/LLVM — 并解释每个 pass 改了什么、为什么改。
* 手工把一个 kernel 从计算定义调度到映射到硬件的实现，并用 roofline（性能上界模型）解释每一个原语（`split`、`bind`、`cache_read`、`compute_at`、`tensorize`）。
* 对一个 kernel 和整个模型跑 MetaSchedule，通过 RPC 把测量分发到真实设备上，并从调优曲线中读出代价模型与硬件之间的取舍。
* 优化 Relax 计算图（融合、layout、内存规划、动态 shape），并通过 BYOC 把子图下推到外部 codegen 路径 — 这正是把一个新的加速器后端 bring up 起来的技能。
* 把编译产物发布到非 Python 目标上：一个服务端 `.so`、一个 MCU C 库，或者一个通过 MLC-LLM 跨 CUDA/Metal/Vulkan/WebGPU 运行的 LLM — 并为这些数字答辩。

---

## 达成标准

当你能做到以下各条时，这门课就算完成：

* 拿一个框架本身已经能跑的模型，用 TVM 编译，**调优它，并在至少一个目标上打败框架基线** — 并展示你弥合的 roofline 差距。
* 看懂调优后 kernel 的性能分析器 trace，判断剩余差距是算力受限、带宽受限还是 launch 受限，以及哪个调度原语能推动它。
* 向一支加速器团队解释，**一个 BYOC 后端要下推他们的算子集，必须实现哪些东西**，以及它大致能覆盖图中多大比例。
* 带另一位工程师过一遍你的编译模型仓库，并让他在同一档硬件上复现你的调优数字，误差在 ±10% 以内。

如果你只能让 TVM *跑起来* 一个模型，那你手里只是个转译器。这门课的重点是让它 *优化* 一个模型 — 并且凭数字知道它确实优化了。

---

*相关：[MLSys Deep Dives](../05-MLSys深度探索/README.md) · [AI Inference Engineer 2026](../01-AI推理工程师2026/README.md) · [阶段 4 — 方向 C — ML Compiler and Graph Optimization](../../../阶段4-路线C-深度学习推理优化/Guide.md)*


<details>
<summary>English original</summary>

**Course Map (5 lectures)**

The arc is the compiler itself, top to bottom and back up: read the stack → schedule a kernel → let the machine schedule it → optimize the graph and reach external codegen → ship the artifact.

<div class="lecture-map" markdown>

| # | Lecture | What you build / measure |
|---|---------|--------------------------|
| [01](Lecture-01.md) | **The TVM stack and the compilation flow** — Relax, TensorIR, and the unified `IRModule` | Import a model, print every IR level, build and run; map the import → optimize → lower → codegen → runtime pipeline |
| [02](Lecture-02.md) | **TensorIR and the schedule space** — turning a compute definition into a hardware-mapped kernel | Schedule a matmul for CPU (tile + vectorize) and GPU (block/thread bind + shared-memory cache); `tensorize` to a Tensor Core intrinsic; GFLOP/s before/after |
| [03](Lecture-03.md) | **Auto-tuning** — AutoTVM → Ansor → MetaSchedule, the learning-based compiler | `ms.tune_tir` a kernel and `ms.tune_relax` a model; read the tuning curve; tune a remote edge board over an RPC tracker |
| [04](Lecture-04.md) | **Relax in depth** — dynamic shapes, operator fusion, and Bring Your Own Codegen (BYOC) | Symbolic-shape a model; fuse with `FuseOps`/`FuseTIR`; offload a subgraph to CUTLASS/TensorRT and measure fused-vs-unfused and offloaded-vs-native |
| [05](Lecture-05.md) | **Shipping it** — runtime, microTVM, and LLMs with MLC-LLM | Package a module (GraphExecutor / Relax VM / AOT); deploy a tiny model to a bare-metal MCU with microTVM; compile + run an LLM with MLC-LLM across backends |

</div>

---

**Course Outcomes**

By the end you should be able to:

* Read a TVM `IRModule` at every level — Relax graph, TIR `PrimFunc`, generated CUDA/LLVM — and explain what each pass changed and why.
* Hand-schedule a kernel from a compute definition to a hardware-mapped implementation, and explain every primitive (`split`, `bind`, `cache_read`, `compute_at`, `tensorize`) in roofline terms.
* Run MetaSchedule on a kernel and a whole model, distribute the measurement to real devices over RPC, and read the cost-model-vs-hardware tradeoff in the tuning curve.
* Optimize a Relax graph (fusion, layout, memory planning, dynamic shape) and offload a subgraph to an external codegen path via BYOC — the exact skill of bringing up a new accelerator backend.
* Ship a compiled artifact to a non-Python target: a server `.so`, an MCU C library, or an LLM across CUDA/Metal/Vulkan/WebGPU via MLC-LLM — and defend the numbers.

---

**Exit Criteria**

You are done with this course when you can:

* Take a model the framework already runs, compile it with TVM, **tune it, and beat the framework baseline** on at least one target — and show the roofline gap you closed.
* Look at a profiler trace of your tuned kernel and say whether the remaining gap is compute-bound, memory-bound, or launch-bound, and which schedule primitive would move it.
* Explain to an accelerator team **what a BYOC backend would have to implement** to offload their op set, and roughly how much of the graph it would capture.
* Walk another engineer through your compiled-model repo and have them reproduce your tuned numbers on the same hardware class within ±10%.

If you can only make TVM *run* a model, you have a transpiler. The point of this course is to make it *optimize* one — and to know, by the number, that it did.

---

*Related: [MLSys Deep Dives](../05-MLSys深度探索/README.md) · [AI Inference Engineer 2026](../01-AI推理工程师2026/README.md) · [Phase 4 — Track C — ML Compiler and Graph Optimization](../../../阶段4-路线C-深度学习推理优化/Guide.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/TVM Deep Dives/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/TVM%20Deep%20Dives/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
