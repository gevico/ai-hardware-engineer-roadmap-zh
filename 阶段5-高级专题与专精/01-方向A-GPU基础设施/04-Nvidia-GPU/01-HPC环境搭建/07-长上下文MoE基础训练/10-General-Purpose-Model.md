---
title: 模块 10 — 从实验到通用模型
description: 模块 10 — 从实验到通用模型
---

# 模块 10 — 从实验到通用模型

**Parent:** [Long-Context MoE Foundation Training](README.md)

**一句话目的：** 验证你训练出的模型在其训练与内部评估的狭窄分布**之外**确实有用——方式是诚实的外部 benchmark、真实任务 harness（agent 运行时框架）、公开对比，以及对什么可交付给出明确决策。

**前置要求：** 模块 01–09。一个训练好的模型检查点，以及模块 07 的评测 harness。

**产物：** 一份收官报告，在训练分布**之外**的任务上把你的模型与一个公开开源基线做对比，诚实写明“哪里赢、哪里输、哪里打平”，并给出可交付的决策。

---

## 为什么重要

完全有可能——而且很常见——训练出的模型在内部验证、自研评估 harness，甚至一些公开 benchmark 上得分很高，却在真实用户实际会跑的任务上表现很**差**。这种失效模式有几个名字：分布过拟合、benchmark 刷分、奖励黑客。解法是在从未进入训练期任何反馈回路的任务上做评估，并与其他团队按自身激励训练出的模型做对比。

本模块讲的这套纪律，把“我们交付了一条训练流水线”与“我们交付了一个有用的模型”区分开来。

---

## 心智模型

### 需要防范的三种失效模式

#### 1. 评测污染

训练数据里意外包含了评测数据，或评测数据的改写。模型靠记忆而非推理“赢下” benchmark。

防御：对每个训练分片与每个评测集做文档哈希重叠检查。标准工具：`BigBench-Hard-Contamination`、`lm-evaluation-harness` 的污染标记。

#### 2. 分布过拟合

训练数据与内部评测数据都落在同一个狭窄分布里（例如“针对 Wikipedia 段落的 QA”），模型为此变得专门化。一旦遇到邻近但更真实的任务（不同文档风格、不常见的问题格式），模型就失败。

防御：在模型**可证明**从未被针对过的任务上做评估。把团队并非为本项目挑选的公开 benchmark 当作对抗性的。

#### 3. 奖励黑客

如果训练反馈回路是一个标量奖励（RLHF、自定义 validator、内部指标），模型就能找到最大化奖励信号的路径而不真正解决底层任务。即便是预训练回路也有这事的影子版本——自适应数据回路过度瞄准单一指标，就是奖励黑客的另一种叫法。

防御：轮换评估目标，绝不在评测集上训练，盯住宽口径指标以防附带损伤。

### “训练分布之外”到底意味着什么

严格版：任务的数据是在你的训练数据截止**之后**，由不知道你这个项目的人生成的。

务实版：团队在回路期间没有针对它优化的公开 benchmark。模型从未见过它的样例，也没有因为在该 benchmark 上失败而补入针对性数据。

两个版本都需要。严格版防的是隐蔽泄漏。务实版则是招聘经理或外部用户能认出来的那种。

### 面向通用实用性的 benchmark 篮子

这是在模块 07 的长上下文篮子之上额外增加的。

#### 通用推理与知识

- **MMLU-Pro** — 比 MMLU 更难，对现代前沿模型更有区分度。
- **GPQA Diamond** — 研究生水平的科学问题，设计上抗检索。
- **BBH (BigBench Hard)** — 多步推理任务。

#### 代码

- **HumanEval / MBPP** — 入门级。
- **LiveCodeBench** — 定期刷新，抗污染。
- **SWE-Bench / SWE-Bench-Verified** — 全仓库的智能体化补丁。

#### 指令遵循

- **IFEval** — 指令遵循，细到“恰好用 3 句话回答，并以句号结尾”这种程度。
- **MT-Bench** / **Arena-Hard** — 用 judge 模型评的多轮对话。

#### 工具使用与 agent

- **WebArena / VisualWebArena** — 智能体化浏览。
- **OSWorld** — 桌面 / OS agent。
- **τ-bench** — 函数调用的真实度。

#### 长文本生成质量

- 在真实 prompt 上，与强基线做成对的人工（或 LLM-judge）比较。

#### 多语言

- **XCOPA / XTREME / Belebele** — 若模型声称支持多语言，就必须拿得出来。

这些不会全部跑。挑选与目标部署场景匹配的那个篮子。


<details>
<summary>English original</summary>

**Module 10 — From Experiment to General-Purpose Model**

**Parent:** [Long-Context MoE Foundation Training](README.md)

**One-line purpose:** Validate that the model you trained is genuinely useful **outside** the narrow distribution of its training and internal evaluation — through honest external benchmarking, real-task harnesses, public comparison, and an explicit decision on what is shippable.

**Prerequisites:** Modules 01–09. A trained model checkpoint and the eval harness from Module 07.

**Artifact:** A capstone report comparing your model against a public open-source baseline on tasks **outside** your training distribution, with honest "where we win, where we lose, where we draw" and a shippable decision.

---

**Why it matters**

It is possible — and common — to train a model that scores well on internal validation, your custom evaluation harness, and even some public benchmarks, while being **bad** at the tasks real users would actually run on it. The failure mode has a few names: distribution overfitting, benchmark gaming, reward hacking. The cure is to evaluate on tasks that were never part of any feedback loop during training, and to compare against models trained by other teams under their own incentives.

This module is the discipline that separates "we shipped a training pipeline" from "we shipped a useful model."

---

**Mental model**

**The three failure modes you are guarding against**

**1. Eval contamination**

The training data accidentally contains the evaluation data, or paraphrases of it. The model "wins" the benchmark by recall rather than reasoning.

Defense: hash-of-document overlap checks between every training shard and every eval set. Standard tools: `BigBench-Hard-Contamination`, `lm-evaluation-harness` contamination flags.

**2. Distribution overfitting**

The training data and the internal eval data live in a narrow distribution (e.g. "QA over Wikipedia passages") that the model becomes specialized for. The model fails on adjacent but realistic tasks (a different document style, an unusual question format).

Defense: evaluate on tasks the model has **provably** never been targeted at. Treat public benchmarks the team did not pick for the project as adversarial.

**3. Reward hacking**

If the training feedback loop is a scalar reward (RLHF, custom validators, internal metrics), the model can find ways to maximize the reward signal without solving the underlying task. Even pretraining loops have shadow versions of this — adaptive-data loops over-targeting one metric is reward hacking by another name.

Defense: rotate evaluation targets, never train on the evaluation set, watch broad-basket metrics for collateral damage.

**What "outside the training distribution" really means**

Strict version: a task whose data was generated **after** your training data cutoff, by people who did not know about your project.

Practical version: a public benchmark that the team did not optimize for during the loop. The model has never seen its examples, no targeted data has been added because of failures on it.

You need both versions. The strict version protects against subtle leakage. The practical version is what a hiring manager or external user will recognize.

**A benchmark basket for general usefulness**

This is on top of the long-context basket from Module 07.

**General reasoning and knowledge**

- **MMLU-Pro** — harder than MMLU, more discriminative for modern frontier models.
- **GPQA Diamond** — graduate-level science questions designed to resist search.
- **BBH (BigBench Hard)** — multi-step reasoning tasks.

**Coding**

- **HumanEval / MBPP** — entry-level.
- **LiveCodeBench** — refreshed periodically, contamination-resistant.
- **SWE-Bench / SWE-Bench-Verified** — full-repo agentic patches.

**Instruction following**

- **IFEval** — instruction-following at the level of "respond in exactly 3 sentences, ending with a period."
- **MT-Bench** / **Arena-Hard** — multi-turn conversation with judge model.

**Tool use and agents**

- **WebArena / VisualWebArena** — agentic browsing.
- **OSWorld** — desktop / OS agent.
- **τ-bench** — function-calling realism.

**Long-form generation quality**

- Pairwise human (or LLM-judge) comparisons against a strong baseline on real prompts.

**Multilingual**

- **XCOPA / XTREME / Belebele** — if your model claims multilingual support, you must show it.

You will not run all of these. Pick the basket that matches your target deployment.

</details>

### 对比 harness（agent 运行时框架）

为每个 benchmark 配一个强的公开基线：

- **Dense 基线**：Llama 3.1 70B Instruct、Qwen 2.5 72B Instruct。
- **MoE（混合专家模型）基线**：Mixtral 8×22B Instruct、Qwen 2.5 MoE A14B、DeepSeek-V3。
- **长上下文基线**：Gemini 2.0 Pro long-context、Claude 4 long-context、Llama 3.1 405B Instruct。

每个 benchmark 报告：

- 你的模型得分（含 bootstrap CI）。
- 基线得分（含 bootstrap CI）。
- Delta 与显著性。
- 一行解读：明显胜出 / 明显落败 / 在噪声范围内。

### 诚实的“可发布”决策

对比之后，写可发布决策一节：

- **模型有竞争力的地方**——点名具体的 benchmark 与幅度。
- **模型没有竞争力的地方**——点名。
- **由此推出的部署形态**——例如“这个模型适合长文档 QA 和代码仓库推理；它不是通用聊天替代品；不要这样宣传它。”

没有这一节的模型会被过度吹嘘，让作者难堪。

### 超越 benchmark：真实世界 dogfooding

benchmark 是必要的，但不充分。在宣称一个模型可用之前：

- **内部团队 dogfooding**，至少在真实工作任务上跑几天。
- **目标部署规模下的延迟 / 吞吐**（目标上下文下 KV cache 是否装得下，目标批大小下的 TPS）。
- **真实 prompt 上的失效模式调研**：从目标用例中收集 100 条 prompt，对输出分类（正确 / 部分正确 / 错误 / 拒答），报告分布。

这类证据才能支撑对外发布。

---

## 构建它

### 1. 污染检查

```bash
# Using lm-eval-harness's contamination check
python -m lm_eval --tasks mmlu_pro \
    --decontamination ngram_size=13 \
    --decontamination_ngrams_path /path/to/your/train/ngrams \
    --model dummy --output_path contamination.json
```

或者自己实现训练分片与评测 prompt 之间的 MinHash 重叠检测。标记并移除与训练文档 MinHash 相似度 ≥ 50% 的评测项。

### 2. 跑这套组合

从上述类别中挑选 8–12 个 benchmark。标准的用 `lm-evaluation-harness` 跑，其余的用项目专属 harness（SWE-Bench 有自己的一套；WebArena 有自己的一套）。

同时让 **同一个基线模型跑过相同的 harness**。分数精度一致性很重要：只有“你的模型在 MMLU 上得 67 分”，却没有基线在你的 harness 上的分数，是没有意义的。

### 3. 长上下文外部扫描

复用 Module 07 的 RULER + LongBench 运行结果。在基线上跑同样的 benchmark。绘制逐任务配对分数图。

### 4. 人工 / 大语言模型评委正面对比

对于长文本生成：

- 从目标用例中采样 50 条真实 prompt。
- 用你的模型和基线分别生成。
- 对输出做匿名处理；让至少三名人工评分者（或用一段精心设计的 judge prompt 交给一个强 judge 模型）对成对偏好打分。
- 报告胜/负/平的数量，附 bootstrap CI。

### 5. 收官报告

使用以下模板：

```
# <Project name> — capstone report

## Model
- Architecture: dense / MoE (E experts, top-k)
- Context: trained at N, evaluated up to N
- Training tokens: T
- Data mix: <summary>

## Headline numbers
| Benchmark         | Our model | Baseline    | Δ      | Note |
|-------------------|-----------|-------------|--------|------|
| MMLU-Pro          | ...       | Llama 3.1 ..|  +0.x  | win  |
| HumanEval         | ...       |             |        |      |
| RULER 32K avg     | ...       |             |        |      |
| LongBench         | ...       |             |        |      |
| SWE-Bench Verified| ...       |             |        |      |

## Where we win
<one paragraph>

## Where we lose
<one paragraph, including a hypothesis>

## Within-noise / draws
<one paragraph>

## Contamination check
<one paragraph with overlap stats>

## Real-task survey
<bullet summary of the 100-prompt classification>

## Shippable decision
"This model is fit for <task list>. It is not fit for <task list>.
 Marketing language: <one sentence>.
 Known limitations: <bullet list>.
 Next investment: <one paragraph>."
```

---

## 在真实技术栈中运用

当团队发布 model card 时，这份报告就是用来填“评估”一节的。看看近期 Llama / Qwen / Mistral / DeepSeek 的 model card；好的几乎都包含这套结构（benchmark 组合、基线对比、定性 dogfooding、明确列出的局限）。差的只公布一个平均数。

用于内部跟踪时，每个重大训练里程碑都应产出一份类似的报告，做版本控制，并从该次运行的 wandb / mlflow 记录里链接过去。

---


<details>
<summary>English original</summary>

**The comparison harness**

Pair every benchmark with a strong public baseline:

- **Dense baselines**: Llama 3.1 70B Instruct, Qwen 2.5 72B Instruct.
- **MoE baselines**: Mixtral 8×22B Instruct, Qwen 2.5 MoE A14B, DeepSeek-V3.
- **Long-context baselines**: Gemini 2.0 Pro long-context, Claude 4 long-context, Llama 3.1 405B Instruct.

For each benchmark, report:

- Your model's score (with bootstrap CI).
- Baseline's score (with bootstrap CI).
- Delta and significance.
- A one-line interpretation: clear win / clear loss / within noise.

**Honest "shippable" decision**

After the comparison, write the shippable-decision section:

- **Where the model is competitive** — name the specific benchmarks and the margin.
- **Where the model is not competitive** — name them.
- **The deployment shape this implies** — e.g. "this model is good for long-document QA and code-repository reasoning; it is not a general chat replacement; do not market it as such."

Models without this section get over-claimed and embarrass their authors.

**Beyond benchmarks: real-world dogfooding**

Benchmarks are necessary but insufficient. Before declaring a model usable:

- **Internal team dogfooding** for at least a few days on real work tasks.
- **Latency / throughput at target deployment scale** (KV cache fit at target context, TPS at target batch size).
- **Failure-mode survey on real prompts**: collect 100 prompts from your target use case, classify the outputs (correct / partial / wrong / refusal), report the distribution.

This is the kind of evidence that justifies an external launch.

---

**Build it**

**1. Contamination check**

```bash
# Using lm-eval-harness's contamination check
python -m lm_eval --tasks mmlu_pro \
    --decontamination ngram_size=13 \
    --decontamination_ngrams_path /path/to/your/train/ngrams \
    --model dummy --output_path contamination.json
```

Or roll your own MinHash overlap between training shards and eval prompts. Flag and remove any eval items with ≥ 50% MinHash similarity to a training document.

**2. Run the basket**

Pick 8–12 benchmarks across the categories above. Run them through `lm-evaluation-harness` for the standard ones, project-specific harnesses for the rest (SWE-Bench has its own; WebArena has its own).

Run **the same baseline model through the same harnesses** at the same time. Score parity matters: a "your model scores 67 on MMLU" with no baseline number on your harness is meaningless.

**3. Long-context external sweep**

Reuse your RULER + LongBench runs from Module 07. Run the same benchmarks on the baseline. Plot pair-wise per-task scores.

**4. Human / LLM-judge head-to-head**

For long-form generation:

- Sample 50 realistic prompts from your target use case.
- Generate from your model and from the baseline.
- Anonymize the outputs; have at least three human raters (or a careful judge prompt to a strong judge model) score pairwise preference.
- Report win/loss/tie counts with bootstrap CIs.

**5. The capstone report**

Use this template:

```
# <Project name> — capstone report

## Model
- Architecture: dense / MoE (E experts, top-k)
- Context: trained at N, evaluated up to N
- Training tokens: T
- Data mix: <summary>

## Headline numbers
| Benchmark         | Our model | Baseline    | Δ      | Note |
|-------------------|-----------|-------------|--------|------|
| MMLU-Pro          | ...       | Llama 3.1 ..|  +0.x  | win  |
| HumanEval         | ...       |             |        |      |
| RULER 32K avg     | ...       |             |        |      |
| LongBench         | ...       |             |        |      |
| SWE-Bench Verified| ...       |             |        |      |

## Where we win
<one paragraph>

## Where we lose
<one paragraph, including a hypothesis>

## Within-noise / draws
<one paragraph>

## Contamination check
<one paragraph with overlap stats>

## Real-task survey
<bullet summary of the 100-prompt classification>

## Shippable decision
"This model is fit for <task list>. It is not fit for <task list>.
 Marketing language: <one sentence>.
 Known limitations: <bullet list>.
 Next investment: <one paragraph>."
```

---

**Use it in the real stack**

When a team publishes a model card, this report is what fills the "evaluation" section. Look at recent Llama / Qwen / Mistral / DeepSeek model cards; the good ones include almost exactly this structure (basket of benchmarks, baseline comparison, qualitative dogfooding, explicit limitations). The weak ones publish a single average number.

For internal tracking, a similar report should be produced at every major training milestone, version-controlled, and linked from the run's wandb / mlflow record.

---

</details>

## 度量

- **覆盖度**：benchmark 组合中涵盖了多少个 benchmark 族？
- **基线精度一致性**：每个指标都至少在同一 harness（agent 运行时框架）上与一个可信基线配对？
- **置信区间**：每个报告的数字都带 CI？
- **污染**：已显式完成重叠检查，并记录残余风险？
- **自用验证**：至少 N 条真实 prompt 经过调研并做了输出分类？

一份对全部五项都回答“yes”的结课报告是可交付的。只对两项回答“yes”的不是。

---

## 交付

在 `lcm-course/capstone/` 中：

1. `capstone_report.md` — 上文填写完整的模板。
2. `benchmark_basket.csv` — 你的模型 + 基线 + delta + CI 的逐 benchmark 得分。
3. `contamination_report.md` — 方法与重叠统计。
4. `dogfood_survey.csv` — 100 条 prompt 的分类及评分者备注。
5. `headline_plot.png` — 一张构成头图视觉的柱状图 / 雷达图。

这个包就是交付物：招聘经理、CTO 或未来的用户用 10 分钟读完，就能明白你的模型是做什么用的。

---

## 本课程之后往哪走

- **前沿训练研究**：scaling law、MoE（混合专家模型）路由创新、长上下文架构（状态空间混合、ring-attention 变体）、训练期 RL。
- **后训练**：SFT、DPO、RLHF、GRPO、智能体化微调、工具使用后训练。
- **推理系统**：vLLM / SGLang / TensorRT-LLM 内部实现、cacheon 风格的生产推理服务栈、FP8 / FP4 推理、投机解码。
- **评估深度**：构建按客户定制的评估 harness、judge 模型对齐、红队 / 安全评估、智能体化任务评估设计。
- **硬件协同设计**：面向 Blackwell 级硬件的训练、FP4 数值、面向超大模型的分布式检查点格式、容错训练研究。

前沿跑得很快。本课程给出跟进所需的系统与方法基础。

---

## 相关页面

- [Module 06 — 自适应数据流水线](06-Adaptive-Data-Pipelines.md)
- [Module 07 — 长上下文评估](07-Long-Context-Evaluation.md)
- [Module 09 — 分布式训练基础设施](09-Distributed-Training-Infrastructure.md)
- [README — 课程概览](README.md)
- lm-evaluation-harness: <https://github.com/EleutherAI/lm-evaluation-harness>
- SWE-Bench: <https://github.com/princeton-nlp/SWE-bench>
- LiveCodeBench: <https://github.com/LiveCodeBench/LiveCodeBench>
- Arena-Hard: <https://github.com/lmarena/arena-hard-auto>


<details>
<summary>English original</summary>

**Measure it**

- **Coverage**: how many benchmark families are represented in the basket?
- **Baseline parity**: every metric paired with at least one credible baseline on the same harness?
- **Confidence intervals**: every reported number with a CI?
- **Contamination**: explicit overlap check completed, with documented residual risk?
- **Dogfooding**: at least N realistic prompts surveyed with output classification?

A capstone report that answers "yes" to all five is shippable. One that answers "yes" to two is not.

---

**Ship it**

In `lcm-course/capstone/`:

1. `capstone_report.md` — the filled-in template above.
2. `benchmark_basket.csv` — per-benchmark scores for your model + baseline + delta + CI.
3. `contamination_report.md` — methodology and overlap stats.
4. `dogfood_survey.csv` — the 100-prompt classification with rater notes.
5. `headline_plot.png` — a single bar / radar chart making the headline visual.

This package is the deliverable a hiring manager, a CTO, or a future user can read in 10 minutes and understand what your model is for.

---

**Where to go after this course**

- **Frontier training research**: scaling laws, MoE routing innovations, long-context architectures (state-space hybrids, ring-attention variants), training-time RL.
- **Post-training**: SFT, DPO, RLHF, GRPO, agentic fine-tuning, tool-use post-training.
- **Inference systems**: vLLM / SGLang / TensorRT-LLM internals, the cacheon-style production serving stack, FP8 / FP4 inference, speculative decoding.
- **Evaluation depth**: building per-customer eval harnesses, judge-model alignment, red-team / safety eval, agentic-task eval design.
- **Hardware co-design**: training for Blackwell-class hardware, FP4 numerics, distributed checkpoint formats for very large models, fault-tolerant training research.

The frontier moves fast. This course gives you the systems-and-method foundation needed to follow along.

---

**Related pages**

- [Module 06 — Adaptive data pipelines](06-Adaptive-Data-Pipelines.md)
- [Module 07 — Long-context evaluation](07-Long-Context-Evaluation.md)
- [Module 09 — Distributed training infrastructure](09-Distributed-Training-Infrastructure.md)
- [README — course overview](README.md)
- lm-evaluation-harness: <https://github.com/EleutherAI/lm-evaluation-harness>
- SWE-Bench: <https://github.com/princeton-nlp/SWE-bench>
- LiveCodeBench: <https://github.com/LiveCodeBench/LiveCodeBench>
- Arena-Hard: <https://github.com/lmarena/arena-hard-auto>

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Long-Context-MoE-Foundation-Training/10-General-Purpose-Model.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Long-Context-MoE-Foundation-Training/10-General-Purpose-Model.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
