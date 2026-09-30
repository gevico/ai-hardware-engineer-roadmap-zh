---
title: Module 06 — 自适应数据流水线
description: Module 06 — 自适应数据流水线
---

# Module 06 — 自适应数据流水线

**Parent:** [长上下文 MoE 基础训练](README.md)

**一句话目的：** 构建一条闭环数据流水线，把评估中发现的失效模式转化为下一轮训练的样本，并具备长上下文 MoE 训练真正需要的数据质量、长度课程与去重规范。

**前置要求：** Module 01–05。熟悉 HuggingFace `datasets`、tokenizer 流水线、去重工具（`text-dedup`、MinHash）。

**产物：** 一个完整闭环 —— 训练基线、评估、定位一个失效模式、生成或筛选定向数据、重新训练、重新评估，并记录 delta。

---

## 为什么重要

静态的“抓取 + 过滤”数据集只能把你带到所有用类似配比训练的模型的平均水平。要更进一步，数据必须回应模型实际的弱点。这就是靠巧合在 benchmark 上拿高分的模型，与在你真正在意的事情上稳定表现良好的模型之间的差别。

具体到 long-context + MoE，数据规范比 dense 短上下文训练更难，原因在于：长文档稀缺，长度分布很重要（课程），并且只有当数据内部异质性足够大到能区分时，MoE 专家才会特化。

---

## 心智模型

### 闭环

```
[base model]
     │
     ▼
[training run on dataset_v_i]
     │
     ▼
[evaluation harness — broad + targeted]
     │
     ▼
[failure-mode triage]    ←─ explicit categorization
     │
     ▼
[targeted data generation / curation / filtering]
     │
     ▼
[dataset_v_{i+1} = mix(dataset_v_i, targeted_additions)]
     │
     └──────────────────► back to training
```

两个设计决策把严肃的闭环与 notebook demo 区分开来：

1. **失效模式分诊是显式的。**“评估掉了”不是一个类别。“40K–80K 位置的逐位置检索准确率下降”才是。
2. **定向新增是有界的。** 你不是替换数据集，而是混入 `5–15%` 针对该失效模式的新数据，由一个可回滚的 config 控制。

### 数据质量基础（基本门槛层）

在做任何自适应之前，你需要标准的数据卫生。跳过这些，意味着你之后的“改进”会被噪声混淆。

| 步骤 | 工具族 | 原因 |
|------|-------------|-----|
| 文档级去重 | MinHash + LSH、`text-dedup` | 防止记忆重复的样板文本 |
| 子串去重 | 后缀数组（Google 的 `deduplicate-text-datasets`） | 移除骗过 MinHash 的近似重复片段 |
| 质量过滤 | KenLM 困惑度 + 简单启发式规则 | 移除机器生成的 SEO 垃圾 |
| 语言识别 + 过滤 | fastText / GlotLID | 丢弃语言不匹配的文档 |
| PII 清洗 | 基于模式 + 分类器 | 合规与下游安全 |
| 毒性过滤 | 按领域设阈值 | 与目标用例对齐 |

MoE 与长上下文这一层建立在其之上。脏语料不会因为混入自适应新增数据就变好。

### 长度课程

从 4K 直接跳到 256K 训练序列是浪费的：模型在早期梯度步中大部分时间都花在摸索位置模式上，而这些模式本可以先在更短的输入上更快学到。

一个典型的时间表：

| 阶段 | Tokens | 序列长度 | 位置编码设置 |
|-------|--------|-----------------|---------------------------|
| 基础预训练 | 1T+ | 4K | base RoPE θ = 10000 |
| 扩展 I | 50–100B | 32K | RoPE θ → 500_000（或 YaRN factor=4） |
| 扩展 II | 20–50B | 128K | YaRN factor=16 |
| 扩展 III | 5–20B | 256K–1M | YaRN factor=32+，数据需极其谨慎 |

token 数量只是示意；原则是：每个阶段使用的 token 比上一阶段少约 5–10×，因为它们的单 token 成本逐步升高。

**关键在于**，每个扩展阶段的数据必须包含真正需要更长上下文的文档。用随机文本把短文档填充到 256K，会教模型认为长位置就是噪声。

### 长上下文丰富数据的来源

- **书籍**：长篇、连贯性高。Books3 在争议之前是标准来源；可复现的替代包括公有领域语料、ArXiv 全文、Project Gutenberg。
- **代码库**：文件级 + 仓库级。若结构得当，可支持多文件推理。
- **长网页的网络抓取**：文档、wiki、法律文书、科学论文。
- **对话日志 / 多轮 agent trace**：真正用上先验上下文才算数的地方。
- **合成的长上下文任务**：链式推理、检索增强的文档、多跳 QA。教模型主动使用上下文所必需；质量低则有风险。

对于真实的长上下文 MoE 基础训练，配比通常是 `~30%` 代码、`~30%` 书籍/论文、`~20%` 精挑的网页、`~10%` 对话、`~10%` 合成数据 —— 具体切分按评估失效模式来调。


<details>
<summary>English original</summary>

**Module 06 — Adaptive Data Pipelines**

**Parent:** [Long-Context MoE Foundation Training](README.md)

**One-line purpose:** Build a closed-loop data pipeline that turns evaluation failure modes into next-iteration training examples, with the data-quality, length-curriculum, and deduplication discipline that long-context MoE training actually needs.

**Prerequisites:** Modules 01–05. Familiarity with HuggingFace `datasets`, tokenizer pipelines, deduplication tools (`text-dedup`, MinHash).

**Artifact:** One full closed loop — train baseline, evaluate, identify a failure mode, generate or filter targeted data, retrain, re-evaluate, and document the delta.

---

**Why it matters**

A static, "scraped + filtered" dataset will get you to the average performance of every model trained on a similar mix. To go further, the data must respond to the model's actual weaknesses. This is the difference between a model that scores well on benchmarks by coincidence and a model that is reliably good at the things you care about.

For long-context + MoE specifically, the data discipline is harder than for dense short-context training because: long documents are scarce, length distribution matters (curriculum), and MoE experts only specialize if the data has enough internal heterogeneity to differentiate.

---

**Mental model**

**The closed loop**

```
[base model]
     │
     ▼
[training run on dataset_v_i]
     │
     ▼
[evaluation harness — broad + targeted]
     │
     ▼
[failure-mode triage]    ←─ explicit categorization
     │
     ▼
[targeted data generation / curation / filtering]
     │
     ▼
[dataset_v_{i+1} = mix(dataset_v_i, targeted_additions)]
     │
     └──────────────────► back to training
```

Two design decisions distinguish a serious loop from a notebook demo:

1. **Failure-mode triage is explicit.** "The eval went down" is not a category. "Per-position retrieval accuracy in positions 40K–80K dropped" is.
2. **Targeted additions are bounded.** You do not replace the dataset; you mix in `5–15%` of new data targeting the failure mode, controlled by a config you can roll back.

**Data quality fundamentals (the table-stakes layer)**

Before anything adaptive, you need standard data hygiene. Skipping these means your "improvements" later are confounded by noise.

| Step | Tool family | Why |
|------|-------------|-----|
| Document-level dedup | MinHash + LSH, `text-dedup` | Prevents memorization of duplicated boilerplate |
| Substring dedup | Suffix arrays (Google's `deduplicate-text-datasets`) | Removes near-duplicate spans that fool MinHash |
| Quality filter | KenLM perplexity + simple heuristic rules | Removes machine-generated SEO sludge |
| Language ID + filter | fastText / GlotLID | Drops mismatched-language documents |
| PII scrub | Pattern-based + classifier | Compliance and downstream safety |
| Toxicity filter | Per-domain threshold | Aligns with target use cases |

The MoE-and-long-context layer comes on top of this. A dirty corpus does not get better by being mixed with adaptive additions.

**Length curriculum**

A direct jump from 4K to 256K training sequences is wasteful: the model spends most of its early gradient steps figuring out positional patterns it could have learned faster on shorter inputs first.

A typical schedule:

| Stage | Tokens | Sequence length | Position-encoding setting |
|-------|--------|-----------------|---------------------------|
| Base pretrain | 1T+ | 4K | base RoPE θ = 10000 |
| Extension I | 50–100B | 32K | RoPE θ → 500_000 (or YaRN factor=4) |
| Extension II | 20–50B | 128K | YaRN factor=16 |
| Extension III | 5–20B | 256K–1M | YaRN factor=32+, very careful data |

The token counts are illustrative; the principle is: each stage uses ~5–10× fewer tokens than the previous, because they are progressively more expensive per token.

**Crucially**, each extension stage's data must contain documents that genuinely need the longer context. Padding short documents to 256K with random text teaches the model that long positions are noise.

**Sources of long-context-rich data**

- **Books**: long-form, high coherence. Books3 was the canonical source pre-controversy; reproducible alternatives include public-domain corpora, ArXiv full-text, Project Gutenberg.
- **Codebases**: file-level + repo-level. Allows multi-file reasoning if structured properly.
- **Web crawls of long pages**: documentation, wikis, legal filings, scientific papers.
- **Conversational logs / multi-turn agent traces**: where actually-using-prior-context matters.
- **Synthetic long-context tasks**: chained reasoning, retrieval-augmented documents, multi-hop QA. Necessary for teaching active context use; risky if quality is low.

For a real long-context MoE foundation training, the mix is typically `~30%` code, `~30%` books/papers, `~20%` curated web, `~10%` conversations, `~10%` synthetic — with the exact split tuned per evaluation failure.

</details>

### "自适应"到底改变了什么

自适应并不意味着"模型自己挑数据"（那很危险：reward hacking、分布收窄）。它意味着**由人控制的流水线把评估中的失效路由为有针对性的数据补充**。

失效 → 定向数据 映射（示例）：

| 失效模式 | 定向补充 |
|--------------|-------------------|
| "Lost in the middle"：检索准确率在位置 30–70% 处下降 | 构造成需要中段上下文检索的文档；干扰项密集的合成数据 |
| 代码多文件推理薄弱 | 以 repo 为范围的任务："给定文件 A–G，修补文件 X 中的 bug" |
| 长思维链超过第 12 步后崩塌 | 带已验证长步骤链的数学/证明 trace |
| 多语言长上下文相对英文退化 | 具有平行结构的长篇非英文文档 |
| 某一领域的 MoE 专家坍缩 | 在混合中做领域重平衡 |

每次补充都是**有界的**、**有版本的**、**被度量的**。如果定向补充在下次评估中没有推动目标指标，就把它移除。

### 合成数据 —— 何时用、怎么用

在长上下文这一端，合成数据是必需的（带有信息性依赖关系的真实 1M token 文档很稀少）。它同时也很危险：由 LLM 生成，就会继承并放大该 LLM 的偏差与错误。

规则：

- **尽可能做机械验证。** 数学：重跑证明。代码：编译并测试。多跳 QA：对照给定上下文复核答案。
- **限定合成数据占比。** 通常占任一扩展阶段混合的 `<= 15%`。
- **让生成器多样化。** 如果所有合成数据都来自同一个生成器模型，你就是在把模型对齐到它的怪癖上。

### 会毁掉整条流水线的反模式

- **在评估集上训练。** 灾难性，且常见得令人尴尬。在文档哈希层面用显式的"不重叠"约束切分评估分片与训练分片。
- **为 benchmark X 做的定向补充推动了 benchmark X，却拖垮了 benchmark Y。** 每一轮都要盯一篮子宽泛指标，而不只是目标指标。
- **加入扩展数据后做激进去重。** 如果扩展数据与基础数据相似，MinHash 可能把扩展数据删掉。要按阶段去重，不要跨阶段去重。
- **不做 RoPE/YaRN 调整的长度课程。** 你教给模型 32K，然后用一个从未见过那些位置的位置编码在 256K 上评估。

---

## 动手搭建

一个真实的闭环，范围限定为一次迭代：

### 步骤 0 —— 基线训练与评估

取一个现成的 8B 级长上下文基础模型。运行评估 harness（agent 运行时框架）（Module 07 详述）。保存按任务、按位置的分解结果。

### 步骤 1 —— 失效模式分诊

打开评估报告。找出最大的单个失效。首次迭代常见的失效有：

- "needle 任务在位置 32K–48K 的准确率是 65%，而位置 8K 处是 92%。"
- "多文件 Python 编辑任务的通过率是 18%。"
- "长文摘要的连贯性在输入超过 16K 后下降。"

挑一个。写一句假设："*模型对上下文后半段位置的利用不足，因为训练混合中答案相关跨度落在靠后部分的文档太少。*"

### 步骤 2 —— 定向数据生成 / 整理

针对上面的例子：

- 拉取 ≥ 40K token 的长篇文档（书籍、论文）。
- 对每一篇，生成一对合成 QA，其中承载答案的跨度在各位置均匀采样，并交错插入干扰项。
- 验证：答案必须能从该文档重建。
- 过滤：用启发式规则 + 一个小型 judge 模型剔除低质量生成。

定向数据目标占比 `~100M tokens` —— 足以产生影响，又不至于主导。

### 步骤 3 —— 混合并再训练

构造 `dataset_v_{i+1}` = `0.9 · dataset_v_i + 0.1 · targeted_data`。在固定 token 预算下继续训练（通常 `~5–20B` tokens —— 足以看到效果，又短到能快速迭代）。

使用 Megatron-LM 的 `--data-blend` 或 HuggingFace `datasets.interleave_datasets`，配以合适的比例。

### 步骤 4 —— 重新评估

运行同一个评估 harness。比较：

- **目标指标**：应当提升（否则说明补充选错了）。
- **相邻指标**：不应有实质性回退。
- **宽泛指标**：应保持或提升。

### 步骤 5 —— 记录差异

写一份简短报告：

```
What failure: positions 32K-48K retrieval accuracy 65% → ?
What hypothesis: under-coverage of mid-context relevant spans
What data: 100M synthetic QA tokens, position-uniform answer placement
What result: 65% → 84% on targeted; -1.2% on summary; +0.3% on broad average
Decision: keep this addition; iterate on summary next
```

这份文档是你这个闭环的记忆。没有它，闭环就只是随机训练。

---


<details>
<summary>English original</summary>

**What "adaptive" actually changes**

Adaptive does not mean "the model picks its own data" (that is dangerous: reward hacking, distribution narrowing). It means **the human-controlled pipeline routes evaluation failures into targeted data additions**.

Failure → targeted data mapping (examples):

| Failure mode | Targeted addition |
|--------------|-------------------|
| "Lost in the middle": retrieval accuracy dips at positions 30–70% | Documents structured to require mid-context retrieval; distractor-heavy synthetic data |
| Code multi-file reasoning weak | Repo-scoped tasks: "patch the bug in file X given files A–G" |
| Long chain-of-thought collapses past step 12 | Math/proof traces with verified long step chains |
| Multilingual long-context degrades vs English | Long-form non-English documents with parallel structure |
| MoE expert collapse on a domain | Domain rebalancing in the mix |

Each addition is **bounded**, **versioned**, and **measured**. If the targeted addition does not move the targeted metric in the next eval, you remove it.

**Synthetic data — when and how**

Synthetic data is necessary at the long-context end (real 1M-token documents with informative dependencies are rare). It is also dangerous: generated by an LLM, it inherits and amplifies that LLM's biases and errors.

Rules:

- **Verify mechanically when possible.** Math: re-run the proof. Code: compile and test. Multi-hop QA: re-check answer against the provided context.
- **Bound the synthetic fraction.** Typically `<= 15%` of any extension stage's mix.
- **Diversify generators.** If all synthetic data comes from one generator model, you are aligning your model to its quirks.

**Anti-patterns that wreck this whole pipeline**

- **Training on the evaluation set.** Catastrophic and embarrassingly common. Set up your eval and training shards with explicit "no overlap" enforcement at hash-of-document level.
- **Targeted addition for benchmark X moves benchmark X but tanks benchmark Y.** Watch a basket of broad metrics every iteration, not just the targeted one.
- **Aggressive deduplication after extension data is added.** If extension data is similar to base data, MinHash may delete the extension. Run dedup per-stage, not across stages.
- **Length curriculum without RoPE/YaRN adjustment.** You teach the model 32K, then evaluate at 256K with a position encoding that has never seen those positions.

---

**Build it**

A real closed loop, scoped to one iteration:

**Step 0 — Baseline training and eval**

Take an existing 8B-class long-context base model. Run the eval harness (Module 07 details). Save the per-task, per-position breakdown.

**Step 1 — Failure-mode triage**

Open the eval report. Identify the largest single failure. Common ones for first iterations:

- "Position 32K–48K accuracy on needle is 65%, vs 92% at position 8K."
- "Multi-file Python edit task pass-rate is 18%."
- "Long-form summary coherence drops past 16K input."

Pick one. Write a one-sentence hypothesis: "*The model under-uses positions in the latter half of the context because the training mix has too few documents where the answer-relevant span is in the late part.*"

**Step 2 — Targeted data generation / curation**

For the example above:

- Pull long-form documents (books, papers) ≥ 40K tokens.
- For each, generate a synthetic QA pair where the answer-bearing span is sampled uniformly across positions, with distractors interleaved.
- Verify: the answer must be reconstructable from the document.
- Filter: drop low-quality generations using a heuristic + a small judge model.

Aim for `~100M tokens` of targeted data — enough to matter, not enough to dominate.

**Step 3 — Mix and retrain**

Construct `dataset_v_{i+1}` = `0.9 · dataset_v_i + 0.1 · targeted_data`. Continue training for a fixed token budget (typically `~5–20B` tokens — enough to see effect, short enough to iterate).

Use Megatron-LM's `--data-blend` or HuggingFace `datasets.interleave_datasets` with the appropriate ratio.

**Step 4 — Re-evaluate**

Run the same eval harness. Compare:

- **Targeted metric**: should improve (otherwise the addition was wrong).
- **Adjacent metrics**: should not regress meaningfully.
- **Broad metrics**: should hold or improve.

**Step 5 — Document the delta**

Write a short report:

```
What failure: positions 32K-48K retrieval accuracy 65% → ?
What hypothesis: under-coverage of mid-context relevant spans
What data: 100M synthetic QA tokens, position-uniform answer placement
What result: 65% → 84% on targeted; -1.2% on summary; +0.3% on broad average
Decision: keep this addition; iterate on summary next
```

This document is your loop's memory. Without it the loop is just random training.

---

</details>

## 在真实技术栈中使用

- **Megatron-LM data-blend**：`--data-path 0.9 /path/to/v_i 0.1 /path/to/targeted`。每个权重是一个比例；权重之和不必为 1.0（会自动归一化）。
- **HuggingFace `datasets.interleave_datasets`** 配合 `stopping_strategy="all_exhausted"` 可生成一致的 mix。
- **去重**：`text-dedup`（<https://github.com/ChenghaoMou/text-dedup>）用于 MinHash + LSH；Google 的 `deduplicate-text-datasets` 用于子串去重。
- **质量过滤**：CCNet（<https://github.com/facebookresearch/cc_net>）是经典 pipeline，作为 KenLM + 启发式组合的参考仍然有用。

合成数据验证：

- **代码**：起一个沙箱化的 `pytest` runner。
- **数学**：用 SymPy + Lean / Isabelle 做证明验证。
- **QA**：judge-model 配 temperature-0 的严格 prompt，问 "is this answer derivable from this passage."

---

## 度量

每轮循环迭代：

- 目标指标的 pre/post。
- 相邻指标的 pre/post。
- 宽泛 benchmark 组合的 pre/post。
- 该轮迭代的 wall-clock 开销（训练 token、GPU-hours）。
- 单位指标提升的成本。

正是单位成本指标告诉你：该继续在同一个失败上迭代，还是转向下一个。

---

## 交付

写入 `lcm-course/`：

1. `data_pipeline_loop.md` — 按 Step 5 模板写的完整单轮迭代报告。
2. `dataset_v_i.yaml` / `dataset_v_{i+1}.yaml` — 实际的 mix 配置。
3. `eval_before.csv` 和 `eval_after.csv` — 成对的评估输出。
4. 一张 Markdown 表格总结决策：保留 / 丢弃 / 迭代。

---

## 相关页面

- [模块 03 — 位置编码](03-Position-Encoding.md)
- [模块 07 — 长上下文评估](07-Long-Context-Evaluation.md)
- [模块 10 — 从实验到通用模型](10-General-Purpose-Model.md)
- CCNet pipeline：<https://github.com/facebookresearch/cc_net>
- text-dedup：<https://github.com/ChenghaoMou/text-dedup>
- ICML 2025 长上下文 FM workshop（数据部分）：<https://longcontextfm.github.io/>


<details>
<summary>English original</summary>

**Use it in the real stack**

- **Megatron-LM data-blend**: `--data-path 0.9 /path/to/v_i 0.1 /path/to/targeted`. Each weight is a fraction; weights need not sum to 1.0 (they are normalized).
- **HuggingFace `datasets.interleave_datasets`** with `stopping_strategy="all_exhausted"` produces a consistent mix.
- **Deduplication**: `text-dedup` (<https://github.com/ChenghaoMou/text-dedup>) for MinHash + LSH; Google's `deduplicate-text-datasets` for substring dedup.
- **Quality filtering**: CCNet (<https://github.com/facebookresearch/cc_net>) is the canonical pipeline, still useful as a reference for KenLM + heuristic combinations.

For synthetic data verification:

- **Code**: spin up a sandboxed `pytest` runner.
- **Math**: SymPy + Lean / Isabelle for proof verification.
- **QA**: judge-model with a temperature-0 strict prompt asking "is this answer derivable from this passage."

---

**Measure it**

Per loop iteration:

- Targeted metric pre/post.
- Adjacent metrics pre/post.
- Broad benchmark basket pre/post.
- Wall-clock cost of the iteration (training tokens, GPU-hours).
- Cost per unit of metric improvement.

The cost-per-unit metric is what tells you whether to keep iterating on the same failure or move to the next one.

---

**Ship it**

Drop into `lcm-course/`:

1. `data_pipeline_loop.md` — the full one-iteration report following the Step 5 template.
2. `dataset_v_i.yaml` / `dataset_v_{i+1}.yaml` — the actual mix configs.
3. `eval_before.csv` and `eval_after.csv` — paired evaluation outputs.
4. One Markdown table summarizing the decision: keep / discard / iterate.

---

**Related pages**

- [Module 03 — Position encoding](03-Position-Encoding.md)
- [Module 07 — Long-context evaluation](07-Long-Context-Evaluation.md)
- [Module 10 — From experiment to general-purpose model](10-General-Purpose-Model.md)
- CCNet pipeline: <https://github.com/facebookresearch/cc_net>
- text-dedup: <https://github.com/ChenghaoMou/text-dedup>
- ICML 2025 Long Context FM workshop (data section): <https://longcontextfm.github.io/>

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Long-Context-MoE-Foundation-Training/06-Adaptive-Data-Pipelines.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Long-Context-MoE-Foundation-Training/06-Adaptive-Data-Pipelines.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
