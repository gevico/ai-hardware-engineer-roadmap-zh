---
title: Part 3 — Blackwell 上的 MoE 推理
description: Part 3 — Blackwell 上的 MoE 推理
---

# Part 3 — Blackwell 上的 MoE 推理

面向现代混合专家模型（MoE）的 Blackwell 级生产推理栈。共五讲，围绕 2025 年两大主流开放权重 MoE 家族的并排对比展开：

* **DeepSeek V3.1** — 671B 总参数 / 37B 激活参数，每层 **256 个路由专家 + 1 个共享专家**，top-8 路由，**MLA**（多头潜在注意力）搭配压缩 KV，原生 **多 token 预测（MTP）** 头。
* **Qwen3-MoE 235B-A22B** — 235B 总参数 / 22B 激活参数，每层 **128 个路由专家 + 0 个共享专家**，top-8 路由，标准分组查询注意力（GQA），无原生 MTP。

这一对的组合格外有用：同一时代、同一年份，在 vLLM/SGLang/TRT-LLM 中均获良好支持，但在架构上有三处与教学相关的差异 —— attention 类型（MLA vs GQA）、共享专家设计、原生投机。每个概念都落到两个具体的可部署系统上。

**硬件目标：** B200（192 GB HBM3e）、B300（288 GB HBM3e），以及主要的 **GB200 NVL72** —— 72-GPU 的 NVLink 域，即 2026 年万亿参数 MoE 推理服务的生产目标。

读完 Part 3，你应当能在多 Blackwell 部署上把任一种 MoE 交付到生产环境，论证 FP4 + FP8 KV 下的精度 recipe，论证 EP/TP 切分方案，并交付一份可复现的 benchmark，其 `$/MTok` 数字是在真实硬件上实测得到的。

## 讲座

<div class="lecture-map" markdown>

| # | 标题 | 核心问题 |
|---|-------|---------------|
| 01 | [现代 MoE 解剖 —— DeepSeek V3.1 与 Qwen3-MoE 235B-A22B](Lecture-01.md) | 哪些相同、哪些不同，每处差异又如何改变推理成本？ |
| 02 | [Blackwell 硬件全景 —— B200、B300、GB200 NVL72、TE2、FP4](Lecture-02.md) | Blackwell 硅片提供了哪些 Hopper 不具备的能力，NVL72 有多大？ |
| 03 | [专家并行（EP）与 gating 热路径](Lecture-03.md) | MoE 如何在众多 GPU 间切分，all-to-all 开销在何处占主导？ |
| 04 | [分离式 prefill（首字前的整段计算）/ decode（逐 token 生成阶段）—— Mooncake、Splitwise、DistServe](Lecture-04.md) | 把 prefill GPU 与 decode GPU 分开，何时才划算？ |
| 05 | [生产级 MoE 推理服务 —— MTP 投机、受限 decode、成本模型](Lecture-05.md) | 完整的生产 recipe 是什么，在 GB200 NVL72 规模下 $/MTok 是多少？ |

</div>

## Part 3 的交付物

在 Part 1 与 Part 2 的 benchmark 仓库基础上扩展：

* 一套可复现的 bench harness（agent 运行时框架），以 `--model {deepseek-v3.1, qwen3-moe-235b-a22b}` × `--runtime {sglang, vllm, trt-llm}` × `--precision {bf16, fp8, fp4}` × `--ep {2, 4, 8, 16}` × `--p-d-mode {colocated, disaggregated}` 为参数。
* 每个模型在 FP4 + FP8 KV 下的精度一致性报告，在 MMLU / GSM8K / HumanEval / BFCL / RULER 上验证。
* 专家负载均衡测量，给出每专家 token 数分布与 gating 计算开销。
* EP=8 与 EP=16 下，all-to-all 通信时间占 step 时间的比例。
* 至少对其中一个模型，给出一份实测的分离式 P/D 运行结果，展示其与同置基线的成本经济性交叉点。
* 最终成本模型：在 B200 与 GB200 NVL72 上，每个 (model, runtime, precision, EP, mode) 单元的 `$/MTok`。

## 达成标准

你能够做到以下全部：

* 用三句话解释 MLA 的 KV 压缩机制，并计算其相对 GQA 的每 token KV 字节数。
* 画出 MoE EP 的 all-to-all 通信模式，并解释它为何比 TP 的全规约更难。
* 基于实测数据论证 DeepSeek V3.1 在 GB200 NVL72 上选 EP=8 还是 EP=16。
* 预测分离式 P/D 在何种 MoE 工作负载下占优，并用一次实测验证。
* 说出 GB200 NVL72 上某个 (model, runtime, precision, EP) 单元的 `$/MTok`，并逐步讲解公式。

当以上各项都能用 benchmark 仓库中的数字加以论证时，你即完成本课程。


<details>
<summary>English original</summary>

**Part 3 — MoE Inference at Blackwell**

The Blackwell-class production inference stack for modern Mixture-of-Experts models. Five lectures, anchored on a side-by-side comparison of the two dominant 2025 open-weights MoE families:

* **DeepSeek V3.1** — 671B total / 37B active params, **256 routed experts + 1 shared** per layer, top-8 routing, **MLA** (Multi-head Latent Attention) with compressed KV, native **multi-token prediction (MTP)** head.
* **Qwen3-MoE 235B-A22B** — 235B total / 22B active params, **128 routed experts + 0 shared** per layer, top-8 routing, standard GQA attention, no native MTP.

The pair is uniquely useful: same era, same year, both well-supported in vLLM/SGLang/TRT-LLM, but architecturally different in three teaching-relevant ways — attention type (MLA vs GQA), shared-expert design, and native speculation. Every concept lands on two concrete deployable systems.

**Hardware target:** B200 (192 GB HBM3e), B300 (288 GB HBM3e), and primarily **GB200 NVL72** — the 72-GPU NVLink domain that is the production target for trillion-parameter MoE serving in 2026.

By the end of Part 3 you should be able to ship either MoE to production on a multi-Blackwell deployment, defend the precision recipe at FP4 + FP8 KV, defend the EP/TP partition, and ship a reproducible benchmark with `$/MTok` numbers measured on real hardware.

**Lectures**

<div class="lecture-map" markdown>

| # | Title | Core question |
|---|-------|---------------|
| 01 | [Anatomy of a modern MoE — DeepSeek V3.1 and Qwen3-MoE 235B-A22B](Lecture-01.md) | What's the same, what differs, and how does each difference change inference cost? |
| 02 | [Blackwell hardware story — B200, B300, GB200 NVL72, TE2, FP4](Lecture-02.md) | What does Blackwell silicon provide that Hopper doesn't, and how big is NVL72? |
| 03 | [Expert parallelism (EP) and the gating hot path](Lecture-03.md) | How is an MoE partitioned across many GPUs, and where does the all-to-all cost dominate? |
| 04 | [Disaggregated prefill / decode — Mooncake, Splitwise, DistServe](Lecture-04.md) | When does separating prefill GPUs from decode GPUs pay for itself? |
| 05 | [Production MoE serving — MTP speculation, constrained decode, cost model](Lecture-05.md) | What's the full production recipe, and what's the $/MTok at GB200 NVL72 scale? |

</div>

**What you ship from Part 3**

Extending the benchmark repo from Parts 1 and 2:

* A reproducible bench harness parametric over `--model {deepseek-v3.1, qwen3-moe-235b-a22b}` × `--runtime {sglang, vllm, trt-llm}` × `--precision {bf16, fp8, fp4}` × `--ep {2, 4, 8, 16}` × `--p-d-mode {colocated, disaggregated}`.
* Precision-parity reports for each model at FP4 + FP8 KV, validated on MMLU / GSM8K / HumanEval / BFCL / RULER.
* Expert-load-balance measurements showing tokens-per-expert distribution and the gating compute cost.
* All-to-all communication time as a fraction of step time at EP=8 and EP=16.
* For at least one of the models, a measured disaggregated P/D run showing the cost-economics crossover with the colocated baseline.
* Final cost model: `$/MTok` for each (model, runtime, precision, EP, mode) cell on B200 and GB200 NVL72.

**Exit criteria**

You can do all of:

* Explain MLA's KV-compression mechanism in three sentences and compute its per-token KV bytes against GQA.
* Sketch the all-to-all communication pattern for MoE EP and explain why it's harder than TP's all-reduce.
* Defend EP=8 vs EP=16 for DeepSeek V3.1 on GB200 NVL72 from a measurement.
* Predict where disaggregated P/D wins for an MoE workload and verify with one measurement.
* State your `$/MTok` for one (model, runtime, precision, EP) cell on GB200 NVL72 and walk the formula.

You have completed the course when these are all defended by numbers in your benchmark repo.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 3 - MoE at Blackwell/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%203%20-%20MoE%20at%20Blackwell/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
