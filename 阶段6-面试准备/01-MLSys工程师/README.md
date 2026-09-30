---
title: MLSys Engineer — 面试准备
description: MLSys Engineer — 面试准备
---

# MLSys Engineer — 面试准备

<div class="course-identity" markdown="1">
<div class="course-identity__icon">ML</div>
<div markdown="1">
<p class="course-identity__eyebrow">阶段 6 · MLSys Engineer</p>
<p class="course-identity__title">面向 ML Systems 工程师的资深级面试准备 —— 推理、kernel、KV cache、投机解码、架构集成。</p>
<p class="course-identity__meta">目标岗位：Inference Engineer · ML Systems Engineer · CUDA Kernel Engineer · LLM Serving Engineer · Staff ML Infra Engineer</p>
</div>
</div>

---

## 公司实际考察什么

资深 MLSys 面试考察三件事：

1. **系统推理能力** —— 你能否识别瓶颈（compute vs memory vs latency），用数字支撑结论，并围绕它做设计？不是问「什么是 roofline」，而是问「roofline 告诉你这个特定 kernel 应该怎么做才不一样」。

2. **实现深度** —— 你是否真的写过融合 kernel、debug 过量化回归、移植过新架构？判断依据是具体程度：真实实现有失效模式、坑点和实测数字。泛泛的描述没有。

3. **取舍的担当** —— 你能否为一个决定辩护？「我们对 KV 用了 INT8，因为 X；但 Gemma 的 V 保留 FP16，因为 Y」是资深级回答。「INT8 省内存」不是。

---

## 主题地图

| 主题领域 | 问题 | 文件 |
|------------|-----------|------|
| KV cache 与内存管理 | PagedAttention、KV 优化、前缀缓存 | [01 — Inference Systems](01-Inference-Systems-QA.md) |
| attention kernel | FlashAttention 分块、HBM 访存分析、融合 kernel 设计 | [01 — Inference Systems](01-Inference-Systems-QA.md) |
| 投机解码 | TRT-LLM 中的 EAGLE-3、边缘调度、接受长度经济学 | [01 — Inference Systems](01-Inference-Systems-QA.md) |
| 延迟优化 | TTFT 降低、prefill 吞吐、前缀缓存 | [01 — Inference Systems](01-Inference-Systems-QA.md) |
| 架构集成 | 把新模型移植到 TRT-LLM、差异检查清单 | [01 — Inference Systems](01-Inference-Systems-QA.md) |

---

## 自评量表

面试前，给每个主题打 1–3 分：

```text
1 = I know the concept and can explain it
2 = I've implemented or debugged something in this area
3 = I have shipped this in production and can defend tradeoffs with numbers
```

目标：最强的两个主题拿 3 分，其余拿 2 分，最多一个拿 1 分。资深级别面试官期望你至少在两个领域有深度。

---

## 如何准备

**第 1 周（广度）：** 把 8 个问答通读一遍。找出哪两个对你最自然 —— 那是你的锚点主题。找出哪两个最弱 —— 那需要补。

**第 2 周（弱项深挖）：** 对每个弱项主题动手做点东西：写一个玩具级融合 attention kernel，用 Python 实现一个 PagedAttention block table，跑投机解码 benchmark。从代码出发讲胜过从笔记出发讲。

**第 3 周（表达）：** 按面试语速出声练答案（每问 3–4 分钟）。录一次自己的音。资深级别最常见的失败是技术上正确，但讲得太慢、来不及说到关键洞见 —— 面试官会在你说到之前打断。

**面试前一天：** 回顾你自己项目的数字。面试官会问「那你实际测到的加速是多少？」要记得自己的数字。

---

## 本节的文件夹

| File | Content |
|------|---------|
| [01-Inference-Systems-QA.md](01-Inference-Systems-QA.md) | 8 个深度问答：PagedAttention、EAGLE-3、KV 优化、融合 attention kernel、FlashAttention HBM 分析、Jetson 上的投机 decode、Orin Nano 上的 TTFT、TRT-LLM 中的新架构集成 |

---

*Up: [Interview Preparation](../README.md)*


<details>
<summary>English original</summary>

**MLSys Engineer — Interview Preparation**

<div class="course-identity" markdown="1">
<div class="course-identity__icon">ML</div>
<div markdown="1">
<p class="course-identity__eyebrow">Phase 6 · MLSys Engineer</p>
<p class="course-identity__title">Senior-level interview preparation for ML Systems engineers — inference, kernels, KV cache, speculative decoding, architecture integration.</p>
<p class="course-identity__meta">Target roles: Inference Engineer · ML Systems Engineer · CUDA Kernel Engineer · LLM Serving Engineer · Staff ML Infra Engineer</p>
</div>
</div>

---

**What companies actually test**

A senior MLSys interview probes three things:

1. **Systems reasoning** — can you identify the bottleneck (compute vs memory vs latency), back it with numbers, and design around it? Not "what is roofline" but "what does the roofline tell you this specific kernel should do differently."

2. **Implementation depth** — have you actually written a fused kernel, debugged a quantization regression, ported a new architecture? The tell is specificity: real implementations have failure modes, gotchas, and measured numbers. Generic descriptions do not.

3. **Tradeoff ownership** — can you defend a decision? "We used INT8 KV because X but we left V in FP16 for Gemma because Y" is a senior answer. "INT8 saves memory" is not.

---

**Topic map**

| Topic area | Questions | File |
|------------|-----------|------|
| KV cache & memory management | PagedAttention, KV optimization, prefix caching | [01 — Inference Systems](01-Inference-Systems-QA.md) |
| Attention kernels | FlashAttention tiling, HBM traffic analysis, fused kernel design | [01 — Inference Systems](01-Inference-Systems-QA.md) |
| Speculative decoding | EAGLE-3 in TRT-LLM, edge scheduling, accept-length economics | [01 — Inference Systems](01-Inference-Systems-QA.md) |
| Latency optimization | TTFT reduction, prefill throughput, prefix caching | [01 — Inference Systems](01-Inference-Systems-QA.md) |
| Architecture integration | Porting a new model to TRT-LLM, divergence checklist | [01 — Inference Systems](01-Inference-Systems-QA.md) |

---

**Self-assessment rubric**

Before the interview, score yourself 1–3 on each topic:

```text
1 = I know the concept and can explain it
2 = I've implemented or debugged something in this area
3 = I have shipped this in production and can defend tradeoffs with numbers
```

Target: 3 on your two strongest topics, 2 on the rest, 1 on at most one. Interviewers at senior level expect depth on at least two areas.

---

**How to prepare**

**Week 1 (breadth):** Read all 8 Q&As once. Identify which two feel most natural to you — those are your anchor topics. Identify which two feel weakest — those need work.

**Week 2 (depth on weak areas):** For each weak topic, build something: write a toy fused attention kernel, implement a PagedAttention block table in Python, run speculative decoding benchmarks. Talking from code beats talking from notes.

**Week 3 (delivery):** Practice saying answers out loud at interview pace (3–4 minutes per question). Record yourself once. The most common failure at senior level is being technically correct but too slow to hit the key insight — interviewers interrupt before you get there.

**Day before:** Review your own project numbers. The interviewer will ask "and what speedup did you actually measure?" Know your numbers.

---

**Files in this section**

| File | Content |
|------|---------|
| [01-Inference-Systems-QA.md](01-Inference-Systems-QA.md) | 8 deep-dive Q&As: PagedAttention, EAGLE-3, KV optimization, fused attention kernel, FlashAttention HBM analysis, speculative decode on Jetson, TTFT on Orin Nano, new architecture integration in TRT-LLM |

---

*Up: [Interview Preparation](../README.md)*

</details>

---

> 原文：[`Phase 6 - Interview Preparation/1. MLSys Engineer/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%206%20-%20Interview%20Preparation/1.%20MLSys%20Engineer/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
