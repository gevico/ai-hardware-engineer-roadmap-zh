---
title: FlashAttention — 系统 / kernel 课程
description: FlashAttention — 系统 / kernel 课程
---

# FlashAttention — 系统 / kernel 课程

<div class="course-identity auto-course" style="--course-accent: #16a34a; --course-accent-rgb: 22, 163, 74;" markdown="1">
<div class="course-identity__icon">FSKC</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入 · 编译器方向</p>
<p class="course-identity__title">FlashAttention — 系统 / kernel 课程的专属课程标识。</p>
<p class="course-identity__meta">产物：编译器/推理优化 · 度量：算子数、内存、延迟</p>
</div>
</div>


**上级：** [02 — Kernel 工程](../Guide.md)

**形式：** 10 讲。理论 → repo / 代码阅读 → 小实验。每讲都会交付一个产物（notebook、benchmark 脚本、kernel 草图、正确性测试或 patch）。

**本课程为何存在。** 网上大多数 "FlashAttention" 材料止步于「分块让它更快」。这不足以交付 attention kernel。到本课程结束时，你将能够阅读 FlashAttention 仓库、解释 IO 数学、调试正确性、对 kernel 做 benchmark，并为真实的 LLM 推理与训练修改或集成 attention 路径。

---

## 结束时你将能做到

- 推导标准 attention 与 FlashAttention 的 IO 复杂度，并预测在给定 GPU 上各自是计算受限还是带宽受限。
- 用纯 NumPy 实现 online-softmax 递推，并证明其与对完整 attention 矩阵做一次性 softmax 在数值上等价。
- 阅读 `flash-attention/csrc`，并追踪一次 `flash_attn_func(...)` 调用从 Python 到已启动 CUDA kernel 的过程。
- 解释 FA1、FA2 和 FA3 之间在 warp / block / sequence / head 划分上的差异。
- 搭建一个正确性 harness，在受控容差范围内将你的 kernel 与 PyTorch 的参考 SDPA 比较。
- 修补一条聚焦的路径（例如 varlen 掩码、小的数值修复、替代 dispatch），并产出 benchmark + 报告。

---

## 前置要求

- 阶段 4 方向 B — Jetson、CUDA 基础、TensorRT。
- 阶段 4 方向 C 单元 01 — 图与算子优化。
- 熟悉多 head attention、softmax 和矩阵乘层面的线性代数。
- 一台至少有一块 NVIDIA GPU 的工作站、CUDA 12.x、针对该 CUDA 构建的 PyTorch，以及 `Dao-AILab/flash-attention` 的克隆。

如果本地没有 GPU，云实例中的单块 L4 / A10 / 3090 就足以完成第 1–7 讲。第 8–10 讲若能用上 H100/H200，将更有利于 FA3 / FP8 实验。

---

## 教学大纲

<div class="lecture-map" markdown>

| # | 讲 | 实验产物 |
|---|---------|--------------|
| 1 | [Attention 瓶颈 + roofline（性能上界模型）](第01讲-注意力瓶颈与roofline.md) | Notebook：按 shape 的算术强度表；单块 GPU 的 roofline 图 |
| 2 | [Online softmax + 数值正确性](第02讲-在线softmax与数值正确性.md) | NumPy 脚本证明 blockwise-LSE 在 fp64 下与一次性 softmax 逐 bit 相等；fp16 / bf16 的容差图 |
| 3 | [FlashAttention-1 算法](第03讲-FlashAttention-1算法.md) | 伪代码走读 + 与 FA1 论文 IO 复杂度界匹配的 HBM 字节计数器 |
| 4 | [GPU kernel 性能基础](第04讲-GPU-kernel性能基础.md) | 对合并 vs 非合并 copy kernel 的 Nsight Compute 抓取；warp 归约微基准 |
| 5 | [Repo 剖析 + Python / CUDA API](第05讲-仓库结构与Python-CUDA-API.md) | 带注释的 `flash_attn_func` 调用 trace，从 Python 到已启动 kernel；最小的 `setup.py` 风格构建 recipe |
| 6 | [FlashAttention-2](第06讲-FlashAttention-2.md) | Benchmark 表：在 seqlen / head-dim 扫描下的 FA1 vs FA2 vs PyTorch SDPA |
| 7 | [反向传播 + 验证](第07讲-反向传播与数值验证.md) | 将 dQ/dK/dV 与参考实现按文档化容差比较的正确性 harness |
| 8 | [推理路径（KV cache、decode（逐 token 生成阶段））](第08讲-推理路径的KV-cache与decode.md) | 在有/无 paged KV 下 decode 步延迟的微基准；RoPE / GQA 健全性检查 |
| 9 | [Hopper / FA3 / FA4](第09讲-Hopper上的FA3与FA4.md) | 在 H100 / H200 上的 FA3 vs FA2 benchmark；在源码中识别 WGMMA + TMA + warp-specialization 路径 |
| 10 | [Capstone](第10讲-综合实践.md) | 一项聚焦的 kernel 路径修改 + benchmark + 正确性 + 撰写报告 |

</div>

---

## 如何使用本课程

- 按顺序学习各讲。每一讲都会构建下一讲所假定的原语。
- 在阅读每讲的 "Build it" 部分之前，先阅读 **repo 内**列出的源文件。这些讲是导览，不是替代品。
- 把所有实验产物放在同一个 `flash-attn-course/` 工作目录中，这样结束时你就有个人 benchmark 归档。
- 把正确性当作一等输出：每个 benchmark 都必须有配套的正确性测试。

---


<details>
<summary>English original</summary>

**FlashAttention — A Systems / Kernel Course**

<div class="course-identity auto-course" style="--course-accent: #16a34a; --course-accent-rgb: 22, 163, 74;" markdown="1">
<div class="course-identity__icon">FSKC</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Compiler Track</p>
<p class="course-identity__title">Specialized course identity for FlashAttention — A Systems / Kernel Course.</p>
<p class="course-identity__meta">Artifact: compiler/inference optimization · Measure: op count, memory, latency</p>
</div>
</div>


**Parent:** [02 — Kernel Engineering](../Guide.md)

**Format:** 10 lectures. Theory → repo / code reading → small lab. Every lecture ships an artifact (notebook, benchmark script, kernel sketch, correctness test, or patch).

**Why this course exists.** Most "FlashAttention" material online stops at "tiling makes it faster." That is not enough to ship attention kernels. By the end of this course you can read the FlashAttention repository, explain the IO math, debug correctness, benchmark kernels, and modify or integrate attention paths for real LLM inference and training.

---

**What you will be able to do at the end**

- Derive the IO complexity of standard attention vs FlashAttention and predict where each one is compute- or memory-bound on a given GPU.
- Implement the online-softmax recurrence in plain NumPy and prove numerical equivalence to a one-shot softmax over the full attention matrix.
- Read `flash-attention/csrc` and trace a `flash_attn_func(...)` call from Python into the launched CUDA kernel.
- Explain the warp / block / sequence / head partitioning differences between FA1, FA2, and FA3.
- Stand up a correctness harness that compares your kernel against PyTorch's reference SDPA with controlled tolerance bands.
- Patch one focused path (e.g. a varlen mask, a small numerics fix, an alternative dispatch) and produce a benchmark + report.

---

**Prerequisites**

- Phase 4 Track B — Jetson, CUDA basics, TensorRT.
- Phase 4 Track C Unit 01 — Graph and operator optimization.
- Comfort with linear algebra at the level of multi-head attention, softmax, and matrix multiply.
- A workstation with at least one NVIDIA GPU, CUDA 12.x, PyTorch built against that CUDA, and a clone of `Dao-AILab/flash-attention`.

If you do not have a GPU locally, a single L4 / A10 / 3090 in a cloud instance is enough for lectures 1–7. Lectures 8–10 benefit from H100/H200 access for the FA3 / FP8 lab.

---

**Syllabus**

<div class="lecture-map" markdown>

| # | Lecture | Lab artifact |
|---|---------|--------------|
| 1 | [Attention bottleneck + roofline](第01讲-注意力瓶颈与roofline.md) | Notebook: per-shape arithmetic intensity table; roofline plot for one GPU |
| 2 | [Online softmax + numerical correctness](第02讲-在线softmax与数值正确性.md) | NumPy script proving blockwise-LSE equals one-shot softmax bit-for-bit in fp64; tolerance plot for fp16 / bf16 |
| 3 | [FlashAttention-1 algorithm](第03讲-FlashAttention-1算法.md) | Pseudocode walkthrough + an HBM-byte counter that matches the FA1 paper's IO complexity bound |
| 4 | [GPU kernel performance basics](第04讲-GPU-kernel性能基础.md) | Nsight Compute capture of a coalesced vs uncoalesced copy kernel; warp-reduction microbenchmark |
| 5 | [Repo anatomy + Python / CUDA API](第05讲-仓库结构与Python-CUDA-API.md) | Annotated `flash_attn_func` call trace from Python to launched kernel; minimal `setup.py`-style build recipe |
| 6 | [FlashAttention-2](第06讲-FlashAttention-2.md) | Benchmark table: FA1 vs FA2 vs PyTorch SDPA across seqlen / head-dim sweep |
| 7 | [Backward pass + validation](第07讲-反向传播与数值验证.md) | Correctness harness that compares dQ/dK/dV against a reference with documented tolerances |
| 8 | [Inference path (KV cache, decode)](第08讲-推理路径的KV-cache与decode.md) | Microbenchmark of decode-step latency with and without paged KV; RoPE / GQA sanity check |
| 9 | [Hopper / FA3 / FA4](第09讲-Hopper上的FA3与FA4.md) | FA3 vs FA2 benchmark on H100 / H200; identify the WGMMA + TMA + warp-specialization paths in the source |
| 10 | [Capstone](第10讲-综合实践.md) | One focused kernel-path change + benchmark + correctness + write-up |

</div>

---

**How to use this course**

- Do the lectures in order. Each one builds primitives the next assumes.
- Read the listed source files **inside the repo** before reading the lecture's "Build it" section. The lectures are tour guides, not replacements.
- Keep every lab artifact in a single `flash-attn-course/` working directory so you have a personal benchmark archive at the end.
- Treat correctness as a first-class output: every benchmark must have a paired correctness test.

---

</details>

## 核心资料

- Repo: <https://github.com/Dao-AILab/flash-attention>
- FA1 论文: <https://arxiv.org/abs/2205.14135>
- FA2 论文: <https://tridao.me/publications/flash2/flash2.pdf>
- FA3 博客: <https://tridao.me/blog/2024/flash3/>
- CUTLASS / CuTe: <https://github.com/NVIDIA/cutlass>
- PTX ISA: <https://docs.nvidia.com/cuda/parallel-thread-execution/>
- Nsight Compute: <https://docs.nvidia.com/nsight-compute/>

---

## 角色映射

- **MTS Kernels / DL 推理优化工程师** — 技能直接对口。capstone 产物以招聘经理能读懂的形式呈现。
- **HPC / 分布式 AI 工程师** — 理解 NCCL 一类库所依托的 kernel layer 所必需。
- **LLM 推理工程师（vLLM、TensorRT-LLM、SGLang）** — Lecture 8–10 直接对应这些栈所交付的 decode（逐 token 生成阶段）路径优化。

---

## 本课程不是什么

- 不是从零讲 CUDA 的课程。Lecture 4 只复习严格必需的最小集合；更深的内容一律指向方向 B 和 NVIDIA 官方文档。
- 不是 Transformer 架构课程。默认你知道 Q、K、V 以及 softmax(QKᵀ/√d)·V 是什么；我们聚焦于如何让这行数学在真实硬件上跑得快。
- 不追求穷尽。非 NVIDIA 硬件上的 FlashAttention 分支以及纯量化变体，我们有意略过。学完本课程，你应当能自己读懂那些 repo。


<details>
<summary>English original</summary>

**Core sources**

- Repo: <https://github.com/Dao-AILab/flash-attention>
- FA1 paper: <https://arxiv.org/abs/2205.14135>
- FA2 paper: <https://tridao.me/publications/flash2/flash2.pdf>
- FA3 blog: <https://tridao.me/blog/2024/flash3/>
- CUTLASS / CuTe: <https://github.com/NVIDIA/cutlass>
- PTX ISA: <https://docs.nvidia.com/cuda/parallel-thread-execution/>
- Nsight Compute: <https://docs.nvidia.com/nsight-compute/>

---

**Role mapping**

- **MTS Kernels / DL Inference Optimization Engineer** — direct skill match. The capstone artifact is in the form a hiring manager can read.
- **HPC / Distributed AI Engineer** — needed for understanding the kernel layer that NCCL and friends sit on top of.
- **LLM Inference Engineer (vLLM, TensorRT-LLM, SGLang)** — Lectures 8–10 map directly to the decode-path optimizations these stacks ship.

---

**What this course is not**

- Not a CUDA-from-scratch course. Lecture 4 reviews the strict minimum; everything deeper is referenced to Track B and to NVIDIA's own docs.
- Not a transformer-architecture course. We assume you know what Q, K, V, and softmax(QKᵀ/√d)·V are; we focus on how to make that line of math run fast on real hardware.
- Not exhaustive. We intentionally skip FlashAttention forks for non-NVIDIA hardware and quantized-only variants. After this course you should be able to read those repos on your own.

</details>

---

> 原文：[`Phase 4 - Track C - DL Inference Optimization/02 - Kernel Engineering/FlashAttention Course/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20C%20-%20DL%20Inference%20Optimization/02%20-%20Kernel%20Engineering/FlashAttention%20Course/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
