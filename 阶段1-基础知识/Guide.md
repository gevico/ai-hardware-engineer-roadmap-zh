---
title: 阶段 1：数字基础
description: 阶段 1：数字基础
---

# 阶段 1：数字基础

<div class="course-identity digital-foundations" markdown="1">
<div class="course-identity__icon">P1</div>
<div markdown="1">
<p class="course-identity__eyebrow">阶段 1 · 数字基础</p>
<p class="course-identity__title">逻辑、体系结构、操作系统与并行执行构成整条路线图的基础。</p>
<p class="course-identity__meta">产物：RTL 模块 + 性能分析器笔记 · 度量：时序、带宽、利用率</p>
</div>
</div>


> *在把 AI 部署到真实硬件之前，先理解计算如何被表示、执行、调度与加速。*

**层映射：** 主要对应 **L5**（硬件体系结构）与 **L6**（RTL / 逻辑设计），并通过操作系统与并行计算与 **L3**（runtime 行为）之间建立重要衔接。

**角色目标：** RTL 设计工程师 · FPGA 工程师 · GPU Runtime 工程师 · AI 编译器工程师 · AI 加速器架构师

**前置要求：** 熟悉基本命令行工具，并具备可用的开发环境

**后续内容：** [阶段 2 — 嵌入式系统](../阶段2-嵌入式系统/Guide.md)、[阶段 3 — 人工智能](../阶段3-人工智能/Guide.md)，然后是阶段 4 的其中一条轨道：[Xilinx FPGA](../阶段4-路线A-Xilinx-FPGA/01-Xilinx-FPGA开发/Guide.md)、[NVIDIA Jetson](../阶段4-路线B-Nvidia-Jetson/01-Nvidia-Jetson平台/Guide.md) 或 [ML Compiler](../阶段4-路线C-深度学习推理优化/Guide.md)

---

## 为什么有这个阶段

后续每个阶段都假定你已经理解计算的运行机制：

- 逻辑如何转化为硬件行为
- 处理器如何执行指令
- 操作系统如何管理内存与设备
- 并行程序如何把工作映射到 CPU 与 GPU 硬件上

跳过这个阶段，后续主题就只会变成工具使用，而不是工程。

---

## 阶段结构

| # | 模块 | 学习内容 | 为什么重要 |
|---|--------|----------------|----------------|
| **1** | [数字设计与 HDL](01-数字设计与HDL/Guide.md) | 布尔逻辑、时序系统、Verilog、testbench | 用于描述硬件的语言 |
| **2** | [计算机体系结构](02-计算机体系结构与硬件/Guide.md) | ISA、流水线、缓存、内存系统、吞吐 vs 延迟 | CPU、GPU 与 NPU 背后的设计逻辑 |
| **3** | [操作系统](03-操作系统/Guide.md) | 进程、内存、调度、同步、驱动 | 管理硬件资源的软件层 |
| **4** | [C++ 与并行计算](04-C++与并行计算/Guide.md) | SIMD、OpenMP、oneTBB、CUDA、HIP、SYCL | 现代 AI 系统使用的执行模型 |

**推荐顺序：** `1 → 2 → 3 → 4`

如果已经掌握数字逻辑，可以加快模块 1 的进度。如果已经了解 OS 基础，仍要仔细完成模块 4；它是通往 AI 硬件工作最重要的桥梁。

---

## 应该产出什么

这个阶段结束时，你应该留下可见的底层产物，而不只是笔记。

- 一个小型 Verilog 模块加一个 testbench
- 一份针对 CPU vs GPU vs 加速器设计的体系结构讲解或对比笔记
- 一份围绕内存、调度或同步行为的调试记录
- 至少一个经过测量的并行程序，最好包含一份 CUDA 或 GPU 性能剖析产物

把这些输出记录在简单的工程日志、项目 README 或 benchmark 笔记中，让工作保持可见、可评审。

---

## 达成标准

当你能做到以下几点时，就可以继续了：

- 读懂基本 RTL，并说明它对应什么硬件
- 对缓存、内存带宽与流水线瓶颈进行推理
- 说明 OS 如何影响设备访问与并发行为
- 对简单的并行工作负载做性能剖析，并说明它是算力受限、带宽受限还是同步受限

这是后续路线图的最低基础。

---

## 谁应优先完成这个阶段

- **硬件优先的学习者：** 按顺序完成整个阶段
- **向下走的 ML 工程师：** 重点放在模块 2 和模块 4
- **嵌入式工程师：** 即使熟悉模块 1，也要彻底完成模块 2、3、4

---

## 下一步

→ [**阶段 2 — 嵌入式系统**](../阶段2-嵌入式系统/Guide.md) · [**阶段 3 — 人工智能**](../阶段3-人工智能/Guide.md)


<details>
<summary>English original</summary>

**Phase 1: Digital Foundations**

<div class="course-identity digital-foundations" markdown="1">
<div class="course-identity__icon">P1</div>
<div markdown="1">
<p class="course-identity__eyebrow">Phase 1 · Digital Foundations</p>
<p class="course-identity__title">Logic, architecture, operating systems, and parallel execution form the base of the whole roadmap.</p>
<p class="course-identity__meta">Artifact: RTL block + profiler note · Measure: timing, bandwidth, utilization</p>
</div>
</div>


> *Learn how computation is represented, executed, scheduled, and accelerated before you try to deploy AI on real hardware.*

**Layer mapping:** Primarily **L5** (hardware architecture) and **L6** (RTL / logic design), with an important bridge into **L3** (runtime behavior) through operating systems and parallel computing.

**Role targets:** RTL Design Engineer · FPGA Engineer · GPU Runtime Engineer · AI Compiler Engineer · AI Accelerator Architect

**Prerequisites:** comfort with basic command-line tooling and a working development environment

**What comes after:** [Phase 2 — Embedded Systems](../阶段2-嵌入式系统/Guide.md), [Phase 3 — Artificial Intelligence](../阶段3-人工智能/Guide.md), then one of the Phase 4 tracks: [Xilinx FPGA](../阶段4-路线A-Xilinx-FPGA/01-Xilinx-FPGA开发/Guide.md), [NVIDIA Jetson](../阶段4-路线B-Nvidia-Jetson/01-Nvidia-Jetson平台/Guide.md), or [ML Compiler](../阶段4-路线C-深度学习推理优化/Guide.md)

---

**Why This Phase Exists**

Every later phase assumes you already understand the mechanics of computation:

- how logic turns into hardware behavior
- how processors execute instructions
- how operating systems manage memory and devices
- how parallel programs map work onto CPU and GPU hardware

If you skip this phase, later topics become tool usage instead of engineering.

---

**Phase Structure**

| # | Module | What you learn | Why it matters |
|---|--------|----------------|----------------|
| **1** | [Digital Design & HDL](01-数字设计与HDL/Guide.md) | Boolean logic, sequential systems, Verilog, testbenches | The language used to describe hardware |
| **2** | [Computer Architecture](02-计算机体系结构与硬件/Guide.md) | ISA, pipelines, caches, memory systems, throughput vs latency | The design logic behind CPUs, GPUs, and NPUs |
| **3** | [Operating Systems](03-操作系统/Guide.md) | processes, memory, scheduling, synchronization, drivers | The software layer that manages hardware resources |
| **4** | [C++ and Parallel Computing](04-C++与并行计算/Guide.md) | SIMD, OpenMP, oneTBB, CUDA, HIP, SYCL | The execution models used by modern AI systems |

**Recommended order:** `1 → 2 → 3 → 4`

If you already know digital logic, you can move faster through Module 1. If you already know OS fundamentals, still do Module 4 carefully; it is the most important bridge into AI hardware work.

---

**What You Should Produce**

This phase should leave you with visible low-level artifacts, not just notes.

- a small Verilog block plus a testbench
- an architecture explainer or comparison note for CPU vs GPU vs accelerator design
- a debugging write-up around memory, scheduling, or synchronization behavior
- at least one measured parallel program, ideally including a CUDA or GPU profiling artifact

Record those outputs in a simple engineering log, project README, or benchmark note so the work stays visible and reviewable.

---

**Exit Criteria**

You are ready to move on when you can:

- read basic RTL and explain what hardware it implies
- reason about cache, memory bandwidth, and pipeline bottlenecks
- explain how the OS affects device access and concurrency behavior
- profile a simple parallel workload and describe whether it is compute-bound, memory-bound, or synchronization-bound

That is the minimum base for the rest of the roadmap.

---

**Who Should Prioritize This Phase**

- **Hardware-first learners:** do the whole phase in order
- **ML engineers moving downward:** focus especially on Modules 2 and 4
- **Embedded engineers:** do Modules 2, 3, and 4 thoroughly even if Module 1 is familiar

---

**Next**

→ [**Phase 2 — Embedded Systems**](../阶段2-嵌入式系统/Guide.md) · [**Phase 3 — Artificial Intelligence**](../阶段3-人工智能/Guide.md)

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
