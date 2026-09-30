---
title: Part 2 — Hopper 上的稠密 Decoder-Only 推理
description: Part 2 — Hopper 上的稠密 Decoder-Only 推理
---

# Part 2 — Hopper 上的稠密 Decoder-Only 推理

Hopper 级硬件（H100 / H200）上 70B 级稠密模型的端到端生产推理栈。七讲，以 2025–2026 年部署最广的两个稠密模型的并排比较为锚点：

* **Llama 3.3 70B Instruct** — 西方典型的稠密主力模型，8192 隐藏维，28672 FFN，分组查询注意力（GQA，64 Q / 8 KV），无偏置 attention。
* **Qwen 2.5 72B Instruct** — 中国典型的稠密对应模型，维度上几乎相同（8192 隐藏维，*略*宽的 29568 FFN，相同的分组查询注意力 64 Q / 8 KV），区别在于 QKV 偏置（存在）和更大的多语言 tokenizer（152K 词表）。*（注意二手资料可能将 Qwen 误写为 12288 / 49152 — 讲座 01 §2.1 说明了为什么这无法通过参数量检查。）*

两个模型共享相同的家族形态（80 层、128K 上下文、分组查询注意力、RoPE、RMSNorm、SwiGLU）**以及相同的核心维度**。它们仅在 tokenizer/词表、约 3% 更宽的 FFN 以及一个微小的架构细节（QKV 偏置）上不同。这一对是稠密空间中信息量最大的教学锚点，因为*每个概念都落在两个具体可部署系统上* — 并且几乎相同的几何结构隔离出真正重要的差异。

到 Part 2 结束时，你应该能够在 4–8× H200 上将任一模型交付到生产环境，论证精度 recipe，论证 runtime 选择，并为首 token 时延（TTFT） / 每输出 token 耗时（TPOT） / 吞吐 / $/MTok / 与参考实现的精度一致性生成可复现的 benchmark。

## 讲座

<div class="lecture-map" markdown>

| # | 标题 | 核心问题 |
|---|-------|---------------|
| 01 | [70B 级稠密模型的解剖 — Llama 3.3 70B vs Qwen 2.5 72B](Lecture-01.md) | 这两者之间哪些保持不变，哪些发生变化？每个差异的代价或收益是什么？ |
| 02 | [Hopper 硬件故事 — H100、H200、Transformer Engine、FP8](Lecture-02.md) | Hopper 实际提供了哪些 Ampere 架构没有的东西，H200 相比 H100 又增加了什么？ |
| 03 | [量化 Llama 3.3 70B 与 Qwen 2.5 72B — AWQ、GPTQ、QuaRot、SpinQuant、FP8](Lecture-03.md) | 每个模型交付什么精度 recipe，并由精度一致性数字支撑？ |
| 04 | [单节点多 GPU 推理服务 — 8× H100/H200 上的张量并行](Lecture-04.md) | TP 如何扩展，集合通信在哪些环节占主导，runtime 特定的配置是什么？ |
| 05 | [现代推理服务栈 — 连续批处理、分页 KV、前缀缓存、推测](Lecture-05.md) | 在这类硬件、这些模型上，哪些旋钮影响哪个指标？ |
| 06 | [Hopper 上的 128K 长上下文 — KV scaling、YaRN、chunked prefill（首字前的整段计算）、前缀共享](Lecture-06.md) | 在 128K 时什么会崩掉，该上下文下的精度 recipe 是什么？ |
| 07 | [通信层内部 — NCCL、自定义全规约、vLLM communicator 栈](Lecture-07.md) | runtime 实际上如何在 GPU 之间移动字节，在 decode（逐 token 生成阶段）时哪条集合通信路径胜出？ |

</div>

## 你从 Part 2 交付什么

一个单独的 benchmark 仓库，扩展 Part 1 的 harness（agent 运行时框架），包含：

* 一个可复现的 bench harness，参数化于 `--model {llama-3.3-70b, qwen-2.5-72b}` × `--runtime {vllm, sglang, trt-llm}` × `--precision {fp16, fp8, awq-int4}` × `--tp {2, 4, 8}` × `--context {4k, 32k, 128k}`。
* 每个模型的精度 recipe，附带精度一致性报告（MMLU / BFCL / GSM8K / RULER 子集）。
* 一张 TP 扩展图表，标注 NCCL 全规约时间。
* 一个长上下文 bench，展示 128K 下 FP8 KV 与 FP16 KV 的对比。
* 针对每个 (model, runtime, hardware, precision) 单元的 `$/MTok` 成本模型。

## 达成标准

你能做到以下全部：

* 画出 Llama 3.3 70B 与 Qwen 2.5 72B 的推理图，表明它们在维度上几乎相同（相同的 8192 隐藏维、每 token 相同的 KV 成本），并指出真正的差异所在 — 词表/tokenizer、约 3% 更宽的 FFN 以及 QKV 偏置。
* 用两句话论证这些模型选择 AWQ-INT4 而非 GPTQ，并引用相关的 arXiv 异常。
* 讲解 8× H100 上张量并行中的全规约步骤，并解释为什么 ring 与 tree NCCL 有影响。
* 预测在 H200 上 32K 上下文启用 chunked prefill 会带来什么首 token 时延（TTFT）变化，然后验证。
* 说出 H200 上一个 (model, precision, TP) 单元的 $/MTok，并给出其来源公式。

如果其中任何一项不牢靠，在进入 Part 3 之前重读对应的讲座。


<details>
<summary>English original</summary>

**Part 2 — Dense Decoder-Only Inference at Hopper**

The end-to-end production inference stack for 70B-class dense models on Hopper-class hardware (H100 / H200). Seven lectures, anchored on a side-by-side comparison of two of the most-deployed dense models in 2025–2026:

* **Llama 3.3 70B Instruct** — the canonical Western dense workhorse, 8192 hidden, 28672 FFN, GQA (64 Q / 8 KV), bias-free attention.
* **Qwen 2.5 72B Instruct** — the canonical Chinese dense counterpart, dimensionally near-identical (8192 hidden, *slightly* wider 29568 FFN, same GQA 64 Q / 8 KV), differing in QKV bias (present) and a larger multilingual tokenizer (152K vocab). *(Beware secondary sources that misquote Qwen as 12288 / 49152 — Lecture 01 §2.1 shows why that fails a parameter-count check.)*

Both models share the same family-shape (80 layers, 128K context, GQA, RoPE, RMSNorm, SwiGLU) **and the same core dimensions**. They differ only in the tokenizer/vocab, a ~3% wider FFN, and one tiny architectural detail (QKV bias). The pair is the highest-information teaching anchor in the dense space because *every concept lands on two concrete deployable systems* — and the near-identical geometry isolates the differences that actually matter.

By the end of Part 2 you should be able to ship either model to production on 4–8× H200, defend the precision recipe, defend the runtime choice, and produce reproducible benchmarks for TTFT / TPOT / throughput / $/MTok / parity-vs-reference.

**Lectures**

<div class="lecture-map" markdown>

| # | Title | Core question |
|---|-------|---------------|
| 01 | [Anatomy of a 70B-class dense model — Llama 3.3 70B vs Qwen 2.5 72B](Lecture-01.md) | What stays the same between these two and what changes? What does each difference cost or buy? |
| 02 | [Hopper hardware story — H100, H200, Transformer Engine, FP8](Lecture-02.md) | What does Hopper actually provide that Ampere doesn't, and what does H200 add over H100? |
| 03 | [Quantizing Llama 3.3 70B and Qwen 2.5 72B — AWQ, GPTQ, QuaRot, SpinQuant, FP8](Lecture-03.md) | What precision recipe ships for each model, defended by parity numbers? |
| 04 | [Single-node multi-GPU serving — tensor parallelism on 8× H100/H200](Lecture-04.md) | How does TP scale, where do the collectives dominate, and what's the runtime-specific config? |
| 05 | [Modern serving stack — continuous batching, paged KV, prefix cache, speculation](Lecture-05.md) | Which knobs move which metric, on this hardware, on these models? |
| 06 | [Long context at 128K on Hopper — KV scaling, YaRN, chunked prefill, prefix sharing](Lecture-06.md) | What breaks at 128K and what is the precision recipe at that context? |
| 07 | [Inside the communication layer — NCCL, custom all-reduce, the vLLM communicator stack](Lecture-07.md) | How does a runtime actually move bytes between GPUs, and which collective path wins at decode? |

</div>

**What you ship from Part 2**

A single benchmark repo, extending the harness from Part 1, that contains:

* A reproducible bench harness parametric over `--model {llama-3.3-70b, qwen-2.5-72b}` × `--runtime {vllm, sglang, trt-llm}` × `--precision {fp16, fp8, awq-int4}` × `--tp {2, 4, 8}` × `--context {4k, 32k, 128k}`.
* A precision recipe per model with parity report (MMLU / BFCL / GSM8K / RULER subset).
* A TP-scaling chart with NCCL all-reduce time annotated.
* A long-context bench showing FP8 KV vs FP16 KV at 128K.
* A `$/MTok` cost model for each (model, runtime, hardware, precision) cell.

**Exit criteria**

You can do all of:

* Sketch the inference graphs of Llama 3.3 70B and Qwen 2.5 72B, show they are dimensionally near-identical (same 8192 hidden, same KV cost per token), and name where the real differences live — vocab/tokenizer, a ~3% wider FFN, and QKV bias.
* Defend AWQ-INT4 over GPTQ for these models in two sentences, citing the relevant arXiv anomaly.
* Walk the all-reduce step in tensor parallelism on 8× H100 and explain why ring vs tree NCCL matters.
* Predict the TTFT change from enabling chunked prefill at 32K context on H200, then verify.
* State your $/MTok for one (model, precision, TP) cell on H200, with the formula it came from.

If any of these is shaky, re-read the matching lecture before moving to Part 3.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 2 - Dense at Hopper/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%202%20-%20Dense%20at%20Hopper/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
