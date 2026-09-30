---
title: AI 推理工程师 2026 — 专题课程
description: AI 推理工程师 2026 — 专题课程
---

# AI 推理工程师 2026 — 专题课程

<div class="course-identity mlsys" markdown="1">
<div class="course-identity__icon">INF</div>
<div markdown="1">
<p class="course-identity__eyebrow">阶段 5 · ML 系统工程 · 专题课程</p>
<p class="course-identity__title">从 Transformer 执行基础，到 Hopper 上的稠密 70B，再到 Blackwell 上的 MoE-672B（混合专家模型）—— 现代推理栈，端到端。</p>
<p class="course-identity__meta">产物：可复现的推理 benchmark · 度量：TTFT、TPOT、吞吐、$/MTok、与参考实现的精度一致性</p>
</div>
</div>

> *保持最新不是附加要求。它就是纪律本身。*

推理层在过去十二个月里的变化，超过此前三年的总和。FP4 从研究走向了原生硅片。解耦式 prefill（首字前的整段计算）/ decode（逐 token 生成阶段）从论文走进生产。MoE 从“有意思”变成“30B 以上的默认架构”。任何课程若不从 2025–2026 年实际交付的东西讲起，就已经在把工程师教偏了。

本课程分为 **四个部分**，可独立阅读，也可按顺序读。第 1–3 部分各自成立；合起来，它们把精度下限逐级压低（FP16 → FP8 → FP4），把架构从稠密引向稀疏（Llama / Qwen → DeepSeek / Qwen3-MoE），把硬件逐级抬高（单 GPU → 8× Hopper → GB200 NVL72 Blackwell）。**第 4 部分是另一种章节**：一个引擎、一个节点、约 96 个 pull request，以及实测的 60× —— 优化这门功夫真实发生的样子，每个数字都能追溯到产生它的那次 diff。

**层次映射：** L3–L8。runtime / 调度器 / kernel / 集合通信 / fabric / 可观测性 —— 适用于推理的完整 ML 系统栈。

**目标岗位：** AI 推理工程师 · GPU Runtime 工程师 · LLM Runtime 优化工程师 · 生产推理工程师 · MLSys 工程师。

**前置要求：**

* 阶段 5 —— ML 系统工程 —— [Stage 0–3](../Guide.md) —— 度量纪律、runtime 基础、Transformer 执行内部机制、GPU kernel。
* 阶段 5 —— 边缘 AI —— [Edge LLM Inference Internals](../../03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md) —— GEMV（矩阵-向量乘）vs GEMM（矩阵-矩阵乘）、roofline（性能上界模型）基础、decode 瓶颈。
* 阶段 3 —— [Neural Networks → Transformer Fundamentals](../../../阶段3-人工智能/01-神经网络/01-Transformer基础/Lecture-01.md) —— Q/K/V、attention、多头、完整 block。
* 能顺畅阅读 Rust 或 C++ 以做 kernel 工作，阅读 Python 以处理 runtime 与 benchmark 的黏合代码。

**后续产出：** 一个可复现的推理 benchmark 仓库，针对你自选的模型 + runtime + 硬件目标，附一份与已发表参考实现的精度一致性报告，以及一条实测的 $/MTok 成本曲线。

---

## 🧠 交互式配套：LLM Inference Visualizer

本课程的 3D 实操配套：**[LLM Inference Visualizer](https://github.com/ai-hpc/llm-inference-viz)** —— 走一遍稠密 decoder-only Transformer 的前向传播（**Qwen 2.5 7B/72B**、**Llama 3.3 70B**），在 **NVIDIA H200 roofline** 上观察每个阶段落在**带宽受限还是算力受限**，并把模型切分到 **TP = 1/2/4/8** 张 GPU 上，看权重如何分片、all-reduce 开销如何增长。

它把本课程的核心要点变得可触可感 —— 尤其是：

* **第 1 部分 · [Lecture 03 — Roofline、带宽与存储层次](01-基础/Lecture-03.md)** —— roofline 图，decode 落在带宽受限的一侧。
* **第 2 部分 · [Lecture 01 — 70B 级稠密模型剖析](02-Hopper稠密计算/Lecture-01.md)** —— 分组查询注意力（GQA）、旋转位置编码（RoPE）、RMSNorm、SwiGLU，在 Llama/Qwen 这对模型上按比例渲染。
* **第 2 部分 · [Lecture 04 — 单节点多 GPU 推理服务（张量并行）](02-Hopper稠密计算/Lecture-04.md)** —— TP 分片与 all-reduce 集合通信，可视化呈现。

本地运行：`git clone https://github.com/ai-hpc/llm-inference-viz && cd llm-inference-viz && npm install && npm run dev` → 打开 `http://localhost:3002/llm`。

---

## 课程地图（4 个部分，27 讲）

### 🧭 第 1 部分 —— AI 推理 / MLSys 基础（5 讲）

心智模型、度量指标、数学与 runtime 全景。读完第 1 部分的人，就能读懂 2026 年的任何 model card，并预判其推理成本形态。

<div class="lecture-map" markdown>

| # | 标题 |
|---|-------|
| 01 | [2026 年推理工程师的心智模型](01-基础/Lecture-01.md) |
| 02 | [Transformer 执行 —— 从 token 到比特](01-基础/Lecture-02.md) |
| 03 | [Roofline、带宽与存储层次](01-基础/Lecture-03.md) |
| 04 | [精度栈 —— FP16 → FP8 → FP4 → INT4](01-基础/Lecture-04.md) |
| 05 | [runtime 全景 —— vLLM、SGLang、TensorRT-LLM、llama.cpp、MLX](01-基础/Lecture-05.md) |

</div>


<details>
<summary>English original</summary>

**AI Inference Engineer 2026 — Special Course**

<div class="course-identity mlsys" markdown="1">
<div class="course-identity__icon">INF</div>
<div markdown="1">
<p class="course-identity__eyebrow">Phase 5 · ML Systems Engineering · Special Course</p>
<p class="course-identity__title">From transformer-execution fundamentals to dense-70B on Hopper to MoE-672B on Blackwell — the modern inference stack, end to end.</p>
<p class="course-identity__meta">Artifact: reproducible inference benchmark · Measure: TTFT, TPOT, throughput, $/MTok, parity vs reference</p>
</div>
</div>

> *Up-to-date is not a side requirement. It is the discipline.*

The inference layer has moved more in the last twelve months than the previous three years combined. FP4 went from research to native silicon. Disaggregated prefill/decode went from paper to production. MoE went from "interesting" to "the default architecture above 30B." Any course that does not lead with what shipped in 2025–2026 is already mis-training engineers.

This course is structured as **four parts** that can be read independently or as a sequence. Parts 1–3 stand on their own; together they walk the precision floor down (FP16 → FP8 → FP4), the architecture from dense to sparse (Llama / Qwen → DeepSeek / Qwen3-MoE), and the hardware up (single-GPU → 8× Hopper → GB200 NVL72 Blackwell). **Part 4 is a different kind of chapter**: one engine, one node, ~96 pull requests, and a measured 60× — the discipline of optimization as it actually happens, with every number traceable to the diff that produced it.

**Layer mapping:** L3–L8. Runtime / scheduler / kernels / collectives / fabric / observability — the full ML Systems stack as it applies to inference.

**Role targets:** AI Inference Engineer · GPU Runtime Engineer · LLM Runtime Optimization Engineer · Production Inference Engineer · MLSys Engineer.

**Prerequisites:**

* Phase 5 — ML Systems Engineering — [Stage 0–3](../Guide.md) — measurement discipline, runtime foundations, transformer execution internals, GPU kernels.
* Phase 5 — Edge AI — [Edge LLM Inference Internals](../../03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md) — GEMV vs GEMM, roofline basics, the decode bottleneck.
* Phase 3 — [Neural Networks → Transformer Fundamentals](../../../阶段3-人工智能/01-神经网络/01-Transformer基础/Lecture-01.md) — Q/K/V, attention, multi-head, the full block.
* Comfort reading Rust or C++ for kernel work, Python for runtime and benchmark glue.

**What comes after:** a reproducible inference benchmark repo for a model + runtime + hardware target of your choice, with a parity report against a published reference and a measured $/MTok cost line.

---

**🧠 Interactive companion: LLM Inference Visualizer**

A 3D, hands-on companion to this course: **[LLM Inference Visualizer](https://github.com/ai-hpc/llm-inference-viz)** — walk the forward pass of a dense decoder-only transformer (**Qwen 2.5 7B/72B**, **Llama 3.3 70B**), see each stage land **memory-bound vs compute-bound** on an **NVIDIA H200 roofline**, and slice the model across **TP = 1/2/4/8** GPUs to watch the weights shard and the all-reduce cost grow.

It makes the core lessons of this course tangible — especially:

* **Part 1 · [Lecture 03 — Roofline, bandwidth, and the memory hierarchy](01-基础/Lecture-03.md)** — the roofline chart, decode on the memory-bound side.
* **Part 2 · [Lecture 01 — Anatomy of a 70B-class dense model](02-Hopper稠密计算/Lecture-01.md)** — GQA, RoPE, RMSNorm, SwiGLU rendered to scale on the Llama/Qwen pair.
* **Part 2 · [Lecture 04 — Single-node multi-GPU serving (tensor parallelism)](02-Hopper稠密计算/Lecture-04.md)** — TP sharding and the all-reduce collectives, visualized.

Run it locally: `git clone https://github.com/ai-hpc/llm-inference-viz && cd llm-inference-viz && npm install && npm run dev` → open `http://localhost:3002/llm`.

---

**Course Map (4 parts, 27 lectures)**

**🧭 Part 1 — Fundamentals of AI Inference / MLSys (5 lectures)**

The mental model, the metrics, the math, and the runtime landscape. Anyone who finishes Part 1 can read any model card in 2026 and predict its inference cost shape.

<div class="lecture-map" markdown>

| # | Title |
|---|-------|
| 01 | [The 2026 inference engineer's mental model](01-基础/Lecture-01.md) |
| 02 | [Transformer execution — from tokens to bits](01-基础/Lecture-02.md) |
| 03 | [Roofline, bandwidth, and the memory hierarchy](01-基础/Lecture-03.md) |
| 04 | [The precision stack — FP16 → FP8 → FP4 → INT4](01-基础/Lecture-04.md) |
| 05 | [The runtime landscape — vLLM, SGLang, TensorRT-LLM, llama.cpp, MLX](01-基础/Lecture-05.md) |

</div>

</details>

### ⚙️ Part 2 — Hopper 上的稠密 decoder-only 推理（7 讲）

面向 70B 级稠密模型的端到端 Hopper 技术栈。以 **Llama 3.3 70B ↔ Qwen 2.5 72B** 的对比为锚点，让每个概念都落在两个可实际部署的系统上。

<div class="lecture-map" markdown>

| # | 标题 |
|---|-------|
| 01 | [70B 级稠密模型的解剖 — Llama 3.3 70B vs Qwen 2.5 72B](02-Hopper稠密计算/Lecture-01.md) |
| 02 | [Hopper 硬件故事 — H100、H200、Transformer Engine、FP8](02-Hopper稠密计算/Lecture-02.md) |
| 03 | [量化 Llama 3.3 70B 与 Qwen 2.5 72B — AWQ、GPTQ、QuaRot、SpinQuant、FP8](02-Hopper稠密计算/Lecture-03.md) |
| 04 | [单节点多 GPU 推理服务 — 8× H100/H200 上的张量并行](02-Hopper稠密计算/Lecture-04.md) |
| 05 | [现代推理服务栈 — 连续批处理、paged KV、前缀缓存、推测](02-Hopper稠密计算/Lecture-05.md) |
| 06 | [Hopper 上的 128K 长上下文 — KV 扩展、YaRN、chunked prefill、前缀共享](02-Hopper稠密计算/Lecture-06.md) |
| 07 | [通信层内部 — NCCL、自定义 all-reduce、vLLM 通信器栈](02-Hopper稠密计算/Lecture-07.md) |

</div>

### 🧬 Part 3 — Blackwell 上的 MoE 推理（5 讲）

面向现代 MoE 的 Blackwell 技术栈 — DeepSeek V3.1（含 MLA + MTP）与 Qwen3-MoE（235B-A22B）— 在 GB200 NVL72 上以 FP4 运行。

<div class="lecture-map" markdown>

| # | 标题 |
|---|-------|
| 01 | [现代 MoE 的解剖 — DeepSeek V3.1 与 Qwen3-MoE 235B-A22B](03-Blackwell-MoE/Lecture-01.md) |
| 02 | [Blackwell 硬件故事 — B200、B300、GB200 NVL72、Transformer Engine 2、FP4](03-Blackwell-MoE/Lecture-02.md) |
| 03 | [专家并行（EP）与 gating 热路径](03-Blackwell-MoE/Lecture-03.md) |
| 04 | [分离式 prefill / decode — Mooncake、Splitwise、DistServe](03-Blackwell-MoE/Lecture-04.md) |
| 05 | [生产环境 MoE 推理服务 — MTP 推测、受限 decode、成本模型](03-Blackwell-MoE/Lecture-05.md) |

</div>

### 🔬 Part 4 — 优化一个真实引擎（10 讲）

一个基于实测的案例研究。Part 1–3 钉住的是*教学锚点*；本部分钉住的是**一个引擎的历史**：[SparkInfer-K3](https://github.com/gittensor-ai-lab/sparkinfer-k3) 在单台 **8× H200** 节点上运行 **Kimi K3**（2.8T 参数、896 个专家、混合 MLA + KDA），128k 下的 decode 从 **1.01 → 60.17 tok/s**，历经约 96 个 pull request — 对照同机同权重的 llama.cpp 的 18.44。

主题不是那些数字 — 它们属于 2026-08 某台机器上的某个模型。主题是这套**方法论**：优化之前先建好计分板，写 kernel 之前先找到真正的上限，并识别出「bug 反而让 benchmark *更好*」这一失效模式。

<div class="lecture-map" markdown>

| # | 标题 |
|---|-------|
| 01 | [工作负载、基线与阶梯](04-真实引擎优化/Lecture-01.md) |
| 02 | [计分板 — 一个无法被钻空子的 benchmark](04-真实引擎优化/Lecture-02.md) |
| 03 | [诊断 — launch-bound、带宽受限，还是通信受限？](04-真实引擎优化/Lecture-03.md) |
| 04 | [Launch 几何 — grid、occupancy 与每个 token 的 327 个 norm](04-真实引擎优化/Lecture-04.md) |
| 05 | [融合与激活值量化的纪律](04-真实引擎优化/Lecture-05.md) |
| 06 | [128k 下的 attention — 按上下文切分，按 head 切分](04-真实引擎优化/Lecture-06.md) |
| 07 | [切分 896 个专家 — 以及随之而来的 Amdahl 陷阱](04-真实引擎优化/Lecture-07.md) |
| 08 | [图常驻的 decode — 彻底消除 launch 开销](04-真实引擎优化/Lecture-08.md) |
| 09 | [被忽略的那个阶段 — 批处理 prefill](04-真实引擎优化/Lecture-09.md) |
| 10 | [静默出错 — 推理引擎独有的失效模式](04-真实引擎优化/Lecture-10.md) |

</div>

---


<details>
<summary>English original</summary>

**⚙️ Part 2 — Dense Decoder-Only Inference at Hopper (7 lectures)**

The end-to-end Hopper stack for 70B-class dense models. Anchored on a **Llama 3.3 70B ↔ Qwen 2.5 72B** comparison so every concept lands on two concrete deployable systems.

<div class="lecture-map" markdown>

| # | Title |
|---|-------|
| 01 | [Anatomy of a 70B-class dense model — Llama 3.3 70B vs Qwen 2.5 72B](02-Hopper稠密计算/Lecture-01.md) |
| 02 | [Hopper hardware story — H100, H200, Transformer Engine, FP8](02-Hopper稠密计算/Lecture-02.md) |
| 03 | [Quantizing Llama 3.3 70B and Qwen 2.5 72B — AWQ, GPTQ, QuaRot, SpinQuant, FP8](02-Hopper稠密计算/Lecture-03.md) |
| 04 | [Single-node multi-GPU serving — tensor parallelism on 8× H100/H200](02-Hopper稠密计算/Lecture-04.md) |
| 05 | [Modern serving stack — continuous batching, paged KV, prefix cache, speculation](02-Hopper稠密计算/Lecture-05.md) |
| 06 | [Long context at 128K on Hopper — KV scaling, YaRN, chunked prefill, prefix sharing](02-Hopper稠密计算/Lecture-06.md) |
| 07 | [Inside the communication layer — NCCL, custom all-reduce, the vLLM communicator stack](02-Hopper稠密计算/Lecture-07.md) |

</div>

**🧬 Part 3 — MoE Inference at Blackwell (5 lectures)**

The Blackwell stack for modern MoE — DeepSeek V3.1 (with MLA + MTP) and Qwen3-MoE (235B-A22B) — at FP4 on GB200 NVL72.

<div class="lecture-map" markdown>

| # | Title |
|---|-------|
| 01 | [Anatomy of a modern MoE — DeepSeek V3.1 and Qwen3-MoE 235B-A22B](03-Blackwell-MoE/Lecture-01.md) |
| 02 | [Blackwell hardware story — B200, B300, GB200 NVL72, Transformer Engine 2, FP4](03-Blackwell-MoE/Lecture-02.md) |
| 03 | [Expert parallelism (EP) and the gating hot path](03-Blackwell-MoE/Lecture-03.md) |
| 04 | [Disaggregated prefill / decode — Mooncake, Splitwise, DistServe](03-Blackwell-MoE/Lecture-04.md) |
| 05 | [Production MoE serving — MTP speculation, constrained decode, cost model](03-Blackwell-MoE/Lecture-05.md) |

</div>

**🔬 Part 4 — Optimizing a Real Engine (10 lectures)**

A measured case study. Parts 1–3 pin *teaching anchors*; this part pins **one engine's history**: [SparkInfer-K3](https://github.com/gittensor-ai-lab/sparkinfer-k3) running **Kimi K3** (2.8T params, 896 experts, hybrid MLA + KDA) on a single **8× H200** node, from **1.01 → 60.17 tok/s** decode at 128k across ~96 pull requests — measured against llama.cpp at 18.44 on the same box and the same weights.

The subject is not the numbers, which belong to one model on one box in 2026-08. It is the **discipline**: build the scoreboard before the optimization, find the binding ceiling before writing kernels, and recognize the failure mode where a bug makes the benchmark *better*.

<div class="lecture-map" markdown>

| # | Title |
|---|-------|
| 01 | [The workload, the baseline, and the ladder](04-真实引擎优化/Lecture-01.md) |
| 02 | [The scoreboard — a benchmark that cannot be gamed](04-真实引擎优化/Lecture-02.md) |
| 03 | [Diagnosis — launch-bound, bandwidth-bound, or comm-bound?](04-真实引擎优化/Lecture-03.md) |
| 04 | [Launch geometry — grids, occupancy, and 327 norms per token](04-真实引擎优化/Lecture-04.md) |
| 05 | [Fusion and the activation-quantization discipline](04-真实引擎优化/Lecture-05.md) |
| 06 | [Attention at 128k — split over context, split over heads](04-真实引擎优化/Lecture-06.md) |
| 07 | [Sharding 896 experts — and the Amdahl trap that followed](04-真实引擎优化/Lecture-07.md) |
| 08 | [Graph-resident decode — killing the launch bill for good](04-真实引擎优化/Lecture-08.md) |
| 09 | [The phase you forgot — batched prefill](04-真实引擎优化/Lecture-09.md) |
| 10 | [Silently wrong — the failure mode unique to inference engines](04-真实引擎优化/Lecture-10.md) |

</div>

---

</details>

## 课程产出

三部分全部学完后，你应当能够：

* 读懂任何 2026 年代的 model card，并预测其推理成本形态（KV 增长、prefill vs decode 谁占主导、dense vs MoE routing、预期的主导精度）。
* 针对一个工作负载 + 硬件 + SLO 挑出 runtime（vLLM / SGLang / TRT-LLM / llama.cpp / MLX），并为该选择辩护。
* 量化一个 70B 级 dense 或 200B+ MoE 模型，并在给定预算内验证与参考实现的精度一致性。
* 搭起 8× Hopper 的 TP 推理服务，带连续批处理、paged KV、prefix cache 和推测，并说明哪个旋钮推动了哪个指标。
* 搭起面向 MoE 的 Blackwell EP 推理服务，带 token 级 routing、MTP，以及（在适用处）分离式 P/D。
* 交付一套可复现的 benchmark，含 TTFT、TPOT、吞吐、p99 和一条有据可依的 $/MTok 成本线。
* **拆解任何优化主张** —— 在讨论技术之前，先说出三种测量可能在自我美化 的方式 —— 并为自己的工作负载建一道 gate，让它能扛住一位诚实的工程师、一位怀有敌意的贡献者和非确定性的硬件（Part 4）。

---

## 时效 / 刷新纪律

与时俱进是这门课的差异化所在。这套纪律是内建的：

* 每讲结尾都用 `## Current as of YYYY-MM` 写明内容的撰写日期，以及它锁定的具体模型 / runtime / 硬件版本。
* [`REFRESH-LOG.md`](REFRESH-LOG.md) 追踪讲座与 benchmark 数据的每一次带日期的更新。
* 带版本的 benchmark 数据：当某个模型或 runtime 发布新版本时，此前的 benchmark 数字会保留在带日期的子文件中以备考古；最新的锁定在每讲顶部。
* **只用一手来源** —— model card、技术报告、GitHub release、官方 benchmark 页面。博客文章（会悄悄腐烂）是最后手段，且须注明日期。
* **实时 benchmark 参照：** 跨硬件（H100 → B200 → GB200/GB300 NVL72、MI355X）与 runtime（vLLM / SGLang / TRT-LLM）的当前跨栈数字（tokens/s、perf/$、tokens/MW、交互性），请使用持续更新的公开 benchmark，如 **[SemiAnalysis InferenceX](https://github.com/SemiAnalysisAI/InferenceX)**（[live dashboard](https://inferencex.com/)，Apache-2.0），而不要用讲座里印出的任何固定数字 —— 软件栈的收益每周都在推动这些数字变化。把讲座里的数字当作*教学锚点*；把实时 dashboard 当作*部署时刻的真相*。
* **刷新节奏：** 默认六个月；若某一大类模型发布（如 DeepSeek V4、Llama 5、Qwen 4）或某一代硬件落地（B300 → Vera Rubin 等），则改为三个月。

---

## 你应当产出什么

一个单一的 repo，到 Part 2 结束时其中包含：

* 一套可复现的 benchmark harness（对模型 / runtime / 硬件参数化）。
* 一条完整 pipeline 在三个量化级别（FP16 参考 → FP8 → AWQ-INT4）下的结果，附精度一致性报告。
* 8× H100 或 H200 上的 TP 扩展数字，附 NCCL 时序拆解。
* 连续批处理 + prefix cache + 推测的数字，且每个旋钮单独隔离。
* 128K 上下文的 bench，对比 FP8 KV 与 FP16 KV。
* 一个成本模型：所选硬件上各配置的 $/MTok。

到 Part 3 结束时，同一套 harness 扩展到 Blackwell 上的 MoE，带 EP、MTP 推测，以及（在簇允许处）分离式 P/D 测量。

Part 4 把该 harness 变成可审计的东西：一份带 drift 断言的锁定参考、一道排在速度 gate *之前* 的正确性 gate、一条只增不减的前沿线且其每个值都可追溯到一次已提交的测量、一份点明约束瓶颈并为每个候选给出数字的诊断、一架至少含五项改动的优化阶梯**包括那些什么都没测出来的改动**，以及一套刻意损坏测试集，证明该 gate 能抓出一个让引擎*变快*的 bug。

---

## 达成标准

当你能做到以下各点时，这门课就算学完了：

* 在白板上解释为什么 decode 是带宽受限的，以及什么能让一个工作负载逃离该状态。
* 向一位不信任量化的机器人学家论证精度下限（FP16 vs FP8 vs INT4 vs FP4）。
* 仅凭一份 profile trace 判断一个工作负载是算力受限、带宽受限、通信受限还是调度器受限 —— 以及该改什么来验证。
* 与另一位工程师一起过一遍你的 benchmark repo，对方能在同一硬件档次上把数字复现到 ±5% 以内。
* 把你的 harness 交给一个怀有敌意的人，对方只找得出你早已记录在案的漏洞。

如果这五条你一条都做不到，那你手里的是一本 recipe 笔记本，而不是一份推理工程工作。重跑 benchmark。


<details>
<summary>English original</summary>

**Course Outcomes**

By the end of all three parts you should be able to:

* Read any 2026-era model card and predict its inference cost shape (KV growth, prefill vs decode dominance, dense vs MoE routing, expected dominant precision).
* Pick a runtime (vLLM / SGLang / TRT-LLM / llama.cpp / MLX) for a workload + hardware + SLO and defend the choice.
* Quantize a 70B-class dense or 200B+ MoE model and validate parity against a reference within a defined budget.
* Stand up 8× Hopper TP serving with continuous batching, paged KV, prefix cache, and speculation — and explain which knobs moved which metric.
* Stand up Blackwell EP serving for MoE with token-level routing, MTP, and (where applicable) disaggregated P/D.
* Ship a reproducible benchmark with TTFT, TPOT, throughput, p99, and a defended $/MTok cost line.
* **Take any optimization claim apart** — name three ways the measurement could be flattering itself before arguing about the technique — and build a gate for your own workload that survives an honest engineer, a hostile contributor, and non-deterministic hardware (Part 4).

---

**Currency / Refresh Discipline**

Up-to-date is the differentiator of this course. The discipline is baked in:

* Every lecture closes with `## Current as of YYYY-MM` stating the date the content was written and the specific model / runtime / hardware versions it pinned.
* [`REFRESH-LOG.md`](REFRESH-LOG.md) tracks every dated update to lectures and benchmark data.
* Versioned benchmark data: when a model or runtime ships a new version, prior benchmark numbers are kept in dated subfiles for archaeology; the latest pinned at the top of each lecture.
* **Primary sources only** — model cards, technical reports, GitHub releases, official benchmark pages. Blog posts (which silently rot) are last-resort and dated.
* **Live benchmark reference:** for current cross-stack numbers (tokens/s, perf/$, tokens/MW, interactivity) across hardware (H100 → B200 → GB200/GB300 NVL72, MI355X) and runtimes (vLLM / SGLang / TRT-LLM), use a continuously-updated public benchmark such as **[SemiAnalysis InferenceX](https://github.com/SemiAnalysisAI/InferenceX)** ([live dashboard](https://inferencex.com/), Apache-2.0) rather than any fixed number printed in a lecture — software-stack gains move these weekly. Treat the lecture numbers as *teaching anchors*; treat the live dashboard as *truth at time of deployment*.
* **Refresh cadence:** six months default; three months if a major model class drops (e.g. DeepSeek V4, Llama 5, Qwen 4) or a hardware generation lands (B300 → Vera Rubin etc.).

---

**What You Should Produce**

A single repo that, by the end of Part 2, contains:

* A reproducible benchmark harness (parametric over model / runtime / hardware).
* One full pipeline at three quantization levels (FP16 reference → FP8 → AWQ-INT4) with parity report.
* TP-scaling numbers on 8× H100 or H200 with NCCL timing breakdown.
* Continuous-batching + prefix-cache + speculation numbers with each knob isolated.
* 128K-context bench with FP8 KV vs FP16 KV.
* A cost model: $/MTok across configurations on the chosen hardware.

By the end of Part 3, the same harness extends to MoE on Blackwell with EP, MTP speculation, and (where the cluster allows) disaggregated P/D measurements.

Part 4 turns that harness into something auditable: a pinned reference with a drift assertion, a correctness gate ordered *before* the speed gate, a raise-only frontier whose every value traces to a committed measurement, a diagnosis naming the binding ceiling with a number per candidate, an optimization ladder of at least five changes **including the ones that measured nothing**, and a deliberate-corruption suite proving the gate catches a bug that makes the engine *faster*.

---

**Exit Criteria**

You are done with this course when you can:

* Explain, on a whiteboard, why decode is bandwidth-bound and what makes a workload escape that regime.
* Defend a precision floor (FP16 vs FP8 vs INT4 vs FP4) to a roboticist who does not trust quantization.
* Tell, from a profile trace alone, whether a workload is compute-bound, memory-bound, comm-bound, or scheduler-bound — and what to change to verify.
* Walk through your benchmark repo with another engineer and they reproduce your numbers on the same hardware class within ±5%.
* Hand your harness to someone hostile and have them find only holes you already documented.

If you cannot do all five, you have a notebook of recipes, not a body of inference-engineering work. Re-run the benchmarks.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
