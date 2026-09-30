---
title: 模块 07 — 长上下文评估
description: 模块 07 — 长上下文评估
---

# 模块 07 — 长上下文评估

**父模块：** [长上下文 MoE 基础训练](README.md)

**一句话目的：** 构建一个诚实的评估 harness（agent 运行时框架），衡量真正重要的东西——逐位置检索、多跳推理、长文档综合、代码库任务——并解释为什么在长上下文下仅看困惑度会产生误导。

**前置要求：** 模块 01–06。熟悉标准 LLM 评测工具（`lm-evaluation-harness`、`vllm` 等）。

**产物：** 一个可复现的 harness，产出跨 RULER、LongBench 以及至少一个 needle-in-haystack 变体的按长度、按任务的细分结果；一份书面总结，说明模型在哪些地方真正用上了长上下文、哪些地方没有。

---

## 为什么重要

一个在长上下文 benchmark 上**平均**得分不错的模型，仍可能在特定位置或特定任务类型上灾难性地失败。没有逐位置、逐任务的细分，你就无法判断模型是「支持 256K 上下文」还是「支持 4K 而且运气好」。

你在本课程中做出的每一次架构和数据改动，都应配上一个评估 delta。本模块就是这些 delta 的基础。

---

## 心智模型

### 为什么长上下文下的困惑度会骗人

困惑度是每 token 平均负对数似然。在长上下文下，逐位置的似然被**简单**的局部语法 token 主导。承载答案的位置上少数几个灾难性错误的 token 对平均值几乎毫无贡献。困惑度曲线看着没问题，检索却可能已经坏了。

**规则**：绝不要仅凭困惑度发布「长上下文模型」的结论。始终配一个下游任务。

### 必须跑的 benchmark 家族

#### Needle-in-a-Haystack（NIAH）

最简单的探针。在一篇由无关文本构成的长文档中，于受控位置插入一个小事实（「密码是 `BLUE123`」），然后要求模型检索它。

变体：

- **单针**：一个事实，一个位置。
- **多针**：多个事实，必须全部检索到。
- **干扰密集**：上下文中散布着看似相似但错误的事实。

指标：按（针位置，总长度）的准确率。画成热力图；「迷失在中间」表现为一条更暗的对角带。

#### RULER

一个现代长上下文 benchmark，覆盖 13 种任务类型：NIAH 变体、变量追踪、常见词提取、高频词提取、多键 NIAH、多值 NIAH、多查询 NIAH、QA 等。在多个上下文长度（4K、8K、16K、32K、64K、128K）上评估。

RULER 是当前长上下文模型结论的默认参考。来源：<https://github.com/hsiehjackson/RULER>。

#### LongBench

贴近真实任务的 benchmark，含 21 个任务，覆盖多文档 QA、摘要、few-shot 学习、合成任务、代码补全。比 RULER 更少合成感，更贴近「用户实际会问什么」。来源：<https://github.com/THUDM/LongBench>。

#### LongBench-v2 / InfiniteBench / 1M 上下文探针

推进到 128K 以上。截至 2026 年，这些对营销级长上下文结论（Gemini 2.0、Claude-4 long-context）有用；它们比 RULER 噪声更大，但覆盖了 RULER 尚未触及的区间。

#### 代码专用长上下文

- **RepoQA / CrossCodeEval**：跨文件代码推理。
- **SWE-Bench**：在真实 repo 上做智能体化的补丁生成。

如果你的目标用例涉及代码库，这些比 RULER 更重要。

#### 多跳 QA

- **HotpotQA / MuSiQue**：在长输入中跨远距离证据的推理链。

长上下文最难的一端——同时要求检索与组合。

### 广泛评估还应覆盖什么

一个在短上下文任务上退化的长上下文模型是坏的。始终要包含：

- **MMLU / MMLU-Pro**：知识。
- **GPQA Diamond**：困难推理。
- **HumanEval / MBPP / LiveCodeBench**：代码。
- **IFEval / Arena-Hard**：指令遵循。
- **HellaSwag / WinoGrande / Arc-Challenge**：常识（对现代模型信息量较低，但跑起来便宜）。

健康的长上下文微调是什么形状：长上下文指标提升，短上下文指标保持在 `±1%` 以内。

### 诚实的汇报

一份长上下文评估报告应始终包含：

1. **逐位置热力图**，用于 NIAH 类探针（不只是聚合准确率）。
2. **按上下文长度的曲线**，用于 RULER 任务（`L = 4K, 8K, ..., 128K`）。
3. **按任务细分**，而不是一个 LongBench 平均值。
4. **相邻的广泛 benchmark**，以表明短上下文没有退化。
5. **置信区间**：长上下文评估的 N 很小（每格常常只有 100–200 个样本）。bootstrap CI 是必须的。

一个孤零零的「RULER 平均 = 73%」数字几乎说明不了什么。

---

## 动手构建


<details>
<summary>English original</summary>

**Module 07 — Long-Context Evaluation**

**Parent:** [Long-Context MoE Foundation Training](README.md)

**One-line purpose:** Build an honest evaluation harness that measures what matters — per-position retrieval, multi-hop reasoning, long-document synthesis, codebase tasks — and explain why perplexity alone is misleading at long context.

**Prerequisites:** Modules 01–06. Familiarity with standard LLM eval tooling (`lm-evaluation-harness`, `vllm`, etc.).

**Artifact:** A reproducible harness producing a per-length, per-task breakdown across RULER, LongBench, and at least one needle-in-haystack variant; a written summary of where your model is and is not actually using long context.

---

**Why it matters**

A model that scores well on a long-context benchmark **on average** can still fail catastrophically at specific positions or specific task types. Without a per-position, per-task breakdown you cannot tell whether your model "supports 256K context" or "supports 4K and gets lucky."

Every architectural and data change you make in this course should be paired with an eval delta. This module is the foundation for those deltas.

---

**Mental model**

**Why perplexity at long context lies**

Perplexity is the average negative log-likelihood per token. At long context, the per-position likelihood is dominated by **easy** local-syntax tokens. A few catastrophically wrong tokens at the answer-bearing position contribute almost nothing to the average. The perplexity curve can look fine while retrieval is broken.

**Rule**: never publish a "long-context model" claim based on perplexity alone. Always pair with a downstream task.

**The benchmark families you must run**

**Needle-in-a-Haystack (NIAH)**

The simplest probe. Insert a small fact ("the secret code is `BLUE123`") at a controlled position inside a long document of unrelated text, then ask the model to retrieve it.

Variants:

- **Single needle**: one fact, one position.
- **Multi-needle**: several facts, must retrieve all.
- **Distractor-heavy**: similar-looking but wrong facts scattered through the context.

Metric: per-(needle position, total length) accuracy. Plot as a heatmap; "lost in the middle" appears as a darker diagonal band.

**RULER**

A modern long-context benchmark covering 13 task types: NIAH variants, variable tracking, common-words extraction, frequent-words extraction, multi-key NIAH, multi-value NIAH, multi-query NIAH, QA, and more. Evaluates at multiple context lengths (4K, 8K, 16K, 32K, 64K, 128K).

RULER is the current default reference for long-context model claims. Source: <https://github.com/hsiehjackson/RULER>.

**LongBench**

Realistic-task benchmark with 21 tasks across multi-doc QA, summarization, few-shot learning, synthetic, code completion. Less synthetic than RULER, more "what would users actually ask." Source: <https://github.com/THUDM/LongBench>.

**LongBench-v2 / InfiniteBench / 1M-context probes**

Push beyond 128K. As of 2026 these are useful for marketing-grade long-context claims (Gemini 2.0, Claude-4 long-context); they are noisier than RULER but cover the regime that RULER does not yet exercise.

**Code-specific long-context**

- **RepoQA / CrossCodeEval**: cross-file code reasoning.
- **SWE-Bench**: agentic patch generation across a real repo.

If your target use case involves codebases, these matter more than RULER.

**Multi-hop QA**

- **HotpotQA / MuSiQue**: chains of reasoning across distant evidence in long inputs.

The hardest end of long context — requires both retrieval and composition.

**What broad evaluation should also cover**

A long-context model that regresses on short-context tasks is broken. Always include:

- **MMLU / MMLU-Pro**: knowledge.
- **GPQA Diamond**: hard reasoning.
- **HumanEval / MBPP / LiveCodeBench**: code.
- **IFEval / Arena-Hard**: instruction following.
- **HellaSwag / WinoGrande / Arc-Challenge**: common-sense (less informative for modern models but cheap to run).

The shape of a healthy long-context fine-tune: long-context metrics improve, short-context metrics hold within `±1%`.

**Honest reporting**

A long-context evaluation report should always include:

1. **Per-position heatmap** for NIAH-style probes (not just aggregate accuracy).
2. **Per-context-length curve** for RULER tasks (`L = 4K, 8K, ..., 128K`).
3. **Per-task breakdown**, not a single LongBench average.
4. **Adjacent broad benchmarks** to show short-context did not regress.
5. **Confidence intervals**: long-context evals have small N (often 100–200 examples per cell). Bootstrap CIs are mandatory.

A single "RULER avg = 73%" number tells you almost nothing.

---

**Build it**

</details>

### 1. NIAH 探针

最小版本：

```python
# niah.py
import random
from pathlib import Path

def make_haystack(filler_text, target_len_tokens, tokenizer):
    # Repeat filler until length target met
    out = []
    while len(tokenizer(" ".join(out)).input_ids) < target_len_tokens:
        out.append(filler_text)
    return " ".join(out)

def insert_needle(haystack, needle, pos_fraction, tokenizer):
    ids = tokenizer(haystack).input_ids
    needle_ids = tokenizer(needle).input_ids
    insert_at = int(len(ids) * pos_fraction)
    new = ids[:insert_at] + needle_ids + ids[insert_at:]
    return tokenizer.decode(new)

def probe_one(model, tokenizer, context_len, pos_fraction, needle, question, filler):
    haystack = make_haystack(filler, context_len, tokenizer)
    text = insert_needle(haystack, needle, pos_fraction, tokenizer)
    prompt = f"{text}\n\nQ: {question}\nA:"
    out = model.generate(prompt, max_tokens=64, temperature=0.0)
    return needle.lower() in out.lower()
```

扫描 `context_len ∈ {4K, 8K, 16K, 32K, 64K, 128K}` 和 `pos_fraction ∈ {0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0}`。每个单元格 10 个 needle，加上 bootstrap CI，共得到约 70 个单元格 × 10 = 700 次生成——在一块 GPU 上一小时内可完成。

绘制为热力图，x 轴为 `pos_fraction`，y 轴为 `context_len`。

### 2. RULER

克隆官方仓库。它自带生成与评分脚本：

```
git clone https://github.com/hsiehjackson/RULER
cd RULER
# Configure model endpoint (vLLM, HuggingFace, or your own)
bash run.sh <model> <context_length>
```

产出每个上下文长度下的各任务分数。保存该 CSV。

### 3. LongBench

```
git clone https://github.com/THUDM/LongBench
cd LongBench
python pred.py --model <your_model>
python eval.py
```

各任务分数在 `result.json` 中。保存它。

### 4. 广泛的短上下文组合

使用 `lm-evaluation-harness`：

```
lm_eval --model vllm --model_args pretrained=<your_model> \
    --tasks mmlu,mmlu_pro,gpqa_diamond,humaneval,mbpp,ifeval,arc_challenge \
    --batch_size auto --output_path eval_short.json
```

### 5. 汇总

一个单一的 dashboard：

```
| Task        | Context | Score | CI    | Notes              |
|-------------|---------|-------|-------|--------------------|
| NIAH mean   | 32K     | 91.2  | ±1.4  |                    |
| NIAH worst-pos | 32K  | 76.0  | ±3.1  | pos_fraction=0.5   |
| RULER avg   | 32K     | 78.5  | ±0.8  | 13-task mean       |
| LongBench   | 32K     | 49.2  | ±1.1  | 21-task mean       |
| MMLU        | 4K      | 67.8  | ±0.5  | (unchanged)        |
| HumanEval   | 4K      | 71.2  | ±2.4  | (unchanged)        |
```

这才是你要报告的东西。而不只是均值。

---

## 在真实技术栈中使用

vLLM 和 SGLang 都暴露 OpenAI 风格的端点，上述所有 harness（agent 运行时框架）都指向它。把模型部署在其中之一后面，再针对该端点运行 RULER / LongBench / `lm-evaluation-harness`。

对于长上下文推理，为 vLLM 配置匹配的位置编码缩放（RoPE base / YaRN）。如果模型 config 中已编码该设置，vLLM 会自动识别；否则通过 CLI 传入：`--rope-scaling '{"type":"yarn","factor":4,"original_max_position_embeddings":32768}'`。

对于你参与过的 cacheon-sglang-miner 项目，同样的外部 benchmark harness 能提供内部 validator 无法给出的“超出其训练分布”信号。

---

## 度量它

评估本身是有成本的。跟踪：

- 每次 eval 运行生成的总 token 数。
- 每个 benchmark 的 wall-clock（128K 下的 RULER 可能耗时数小时）。
- 每次 eval 迭代的 API/算力成本。

一个目标节奏：每次迭代跑一次完整 eval 组合，在两次完整运行之间做更小的每日 smoke eval（单一长度下的单个 RULER 任务、一个小型 NIAH 网格）。

---

## 交付它

放入 `lcm-course/`：

1. `niah.py` 及其热力图 PNG。
2. `ruler_results.csv`（各任务、各长度）。
3. `longbench_results.json`。
4. 来自 `lm-evaluation-harness` 的 `eval_short.json`。
5. `eval_dashboard.md` —— 把上表按你的某次运行填写好，附上 bootstrap CI，并写一段文字说明该模型在哪些地方真正用到了长上下文、哪些地方没有。

---

## 相关页面

- [模块 03 —— 位置编码](03-Position-Encoding.md)
- [模块 06 —— 自适应数据流水线](06-Adaptive-Data-Pipelines.md)
- [模块 10 —— 从实验到通用模型](10-General-Purpose-Model.md)
- RULER：<https://github.com/hsiehjackson/RULER>
- LongBench：<https://github.com/THUDM/LongBench>
- lm-evaluation-harness：<https://github.com/EleutherAI/lm-evaluation-harness>
- “Lost in the Middle” 论文：<https://arxiv.org/abs/2307.03172>


<details>
<summary>English original</summary>

**1. NIAH probe**

The minimal version:

```python
# niah.py
import random
from pathlib import Path

def make_haystack(filler_text, target_len_tokens, tokenizer):
    # Repeat filler until length target met
    out = []
    while len(tokenizer(" ".join(out)).input_ids) < target_len_tokens:
        out.append(filler_text)
    return " ".join(out)

def insert_needle(haystack, needle, pos_fraction, tokenizer):
    ids = tokenizer(haystack).input_ids
    needle_ids = tokenizer(needle).input_ids
    insert_at = int(len(ids) * pos_fraction)
    new = ids[:insert_at] + needle_ids + ids[insert_at:]
    return tokenizer.decode(new)

def probe_one(model, tokenizer, context_len, pos_fraction, needle, question, filler):
    haystack = make_haystack(filler, context_len, tokenizer)
    text = insert_needle(haystack, needle, pos_fraction, tokenizer)
    prompt = f"{text}\n\nQ: {question}\nA:"
    out = model.generate(prompt, max_tokens=64, temperature=0.0)
    return needle.lower() in out.lower()
```

Sweep `context_len ∈ {4K, 8K, 16K, 32K, 64K, 128K}` and `pos_fraction ∈ {0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0}`. With 10 needles per cell and bootstrap CIs, you get ~70 cells × 10 = 700 generations — feasible on one GPU in an hour.

Plot as a heatmap with `pos_fraction` on the x-axis and `context_len` on the y-axis.

**2. RULER**

Clone the official repo. It ships generation and scoring scripts:

```
git clone https://github.com/hsiehjackson/RULER
cd RULER
# Configure model endpoint (vLLM, HuggingFace, or your own)
bash run.sh <model> <context_length>
```

Produces per-task scores at each context length. Save the CSV.

**3. LongBench**

```
git clone https://github.com/THUDM/LongBench
cd LongBench
python pred.py --model <your_model>
python eval.py
```

Per-task scores in `result.json`. Save it.

**4. Broad short-context basket**

Use `lm-evaluation-harness`:

```
lm_eval --model vllm --model_args pretrained=<your_model> \
    --tasks mmlu,mmlu_pro,gpqa_diamond,humaneval,mbpp,ifeval,arc_challenge \
    --batch_size auto --output_path eval_short.json
```

**5. Bring it together**

A single dashboard:

```
| Task        | Context | Score | CI    | Notes              |
|-------------|---------|-------|-------|--------------------|
| NIAH mean   | 32K     | 91.2  | ±1.4  |                    |
| NIAH worst-pos | 32K  | 76.0  | ±3.1  | pos_fraction=0.5   |
| RULER avg   | 32K     | 78.5  | ±0.8  | 13-task mean       |
| LongBench   | 32K     | 49.2  | ±1.1  | 21-task mean       |
| MMLU        | 4K      | 67.8  | ±0.5  | (unchanged)        |
| HumanEval   | 4K      | 71.2  | ±2.4  | (unchanged)        |
```

This is what you report. Not just the mean.

---

**Use it in the real stack**

vLLM and SGLang both expose an OpenAI-style endpoint that all these harnesses point at. Spin up your model behind one of those, then run RULER / LongBench / `lm-evaluation-harness` against the endpoint.

For long-context inference, configure vLLM with the matching position-encoding scaling (RoPE base / YaRN). If your model config encodes it, vLLM picks it up; otherwise pass via CLI: `--rope-scaling '{"type":"yarn","factor":4,"original_max_position_embeddings":32768}'`.

For the cacheon-sglang-miner project you worked on, the same external benchmark harnesses give you the "outside-its-training-distribution" signal that internal validators cannot provide.

---

**Measure it**

The evaluation itself has costs. Track:

- Total tokens generated per eval run.
- Wall-clock per benchmark (RULER at 128K can take many hours).
- API/compute cost per eval iteration.

A target cadence: a full eval basket per iteration, with smaller daily smoke evals (one RULER task at one length, a small NIAH grid) between full runs.

---

**Ship it**

Drop into `lcm-course/`:

1. `niah.py` and its heatmap PNG.
2. `ruler_results.csv` (per-task, per-length).
3. `longbench_results.json`.
4. `eval_short.json` from `lm-evaluation-harness`.
5. `eval_dashboard.md` — the table above filled in for one of your runs, with bootstrap CIs and a written paragraph identifying where this model genuinely uses long context vs where it does not.

---

**Related pages**

- [Module 03 — Position encoding](03-Position-Encoding.md)
- [Module 06 — Adaptive data pipelines](06-Adaptive-Data-Pipelines.md)
- [Module 10 — From experiment to general-purpose model](10-General-Purpose-Model.md)
- RULER: <https://github.com/hsiehjackson/RULER>
- LongBench: <https://github.com/THUDM/LongBench>
- lm-evaluation-harness: <https://github.com/EleutherAI/lm-evaluation-harness>
- "Lost in the Middle" paper: <https://arxiv.org/abs/2307.03172>

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Long-Context-MoE-Foundation-Training/07-Long-Context-Evaluation.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Long-Context-MoE-Foundation-Training/07-Long-Context-Evaluation.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
