---
title: 讲座：用 BFCL 评估 Agent 工具调用派发
description: 讲座：用 BFCL 评估 Agent 工具调用派发
---

# 讲座：用 BFCL 评估 Agent 工具调用派发

<div class="course-identity edge-ai" markdown="1">
<div class="course-identity__icon">BFCL</div>
<div markdown="1">
<p class="course-identity__eyebrow">阶段 5 · 边缘 AI · 讲座</p>
<p class="course-identity__title">衡量边缘 LLM agent 是否真的用正确的参数调用了正确的工具，并证明你的量化没有把它弄坏。</p>
<p class="course-identity__meta">产物：BFCL 风格 harness（agent 运行时框架）+ 分类准确率报告 · 度量：top-1 工具调用准确率、参数匹配率、相对 fp16 参考实现的精度一致性</p>
</div>
</div>

> *Jetson 上的 4-bit Qwen 只有在仍能选对工具时才有用。*

端侧 agent 能交付，要满足两件事：**它装得进设备，并且能正确调用工具**。阶段 5 的 [Qwen 推理优化](../05-Qwen推理优化/README.md) 覆盖了前一半——Jetson 上的量化、KV cache、decode（逐 token 生成阶段）优化。本讲座覆盖后一半——决定这些优化中每一项是否悄悄弄坏了 agent 的**度量框架**。

标准 benchmark 是 **BFCL**——**Berkeley Function-Calling Leaderboard**。它对 LLM 工具调用派发的关系，就像 LIBERO 对 VLA（视觉-语言-动作模型）动作精度一致性的关系：一个固定、公开、按类别拆分的评估，让你只在一条轴上把候选模型（或候选 runtime 配置）与参考实现对比——*函数调用是否与 ground truth 一致*。

本讲座把 BFCL 当作**硬件质量门禁**，而不是刷榜：你搭一个很小的 BFCL 风格 harness，对边缘模型跑一遍，并用**分类准确率表**来把关你的优化阶梯。

**层级映射：** L3-L5。位于推理 runtime（L3-L4）与其上层的 agent harness（L5）之间。

**岗位目标：** 边缘 AI 工程师 · 边缘推理优化工程师 · Agent Harness / Runtime 工程师 · 嵌入式 LLM 评测工程师。

**前置要求：**

* 阶段 5 — 边缘 AI — [边缘 LLM 推理内幕](../03-边缘LLM推理内部机制/Lecture-01.md)——你需要已经知道 Transformer 如何派发。
* 阶段 5 — 边缘 AI — [Qwen 推理优化](../05-Qwen推理优化/README.md)，特别是 [讲座 2 — 把 Qwen3-4B 量化到 Q4](../05-Qwen推理优化/Lecture-02.md)。BFCL 就是用来告诉你 AWQ-INT4 是否让你损失了工具调用准确率的东西。
* 阶段 4 方向 B — [Jetson 实时推理](../../../阶段4-路线B-Nvidia-Jetson/01-Nvidia-Jetson平台/11-Orin-Nano实时推理/Guide.md)，对应部署目标。

**后续内容：** [VLA 动作精度一致性 Harness](../../04-方向D-机器人/05-VLA优化与动作对齐测试/Lecture-02.md)——同样的思路应用到具身策略上，只不过那里的“action”是 7-DoF 指令，而不是 JSON 工具调用。

本讲座结束时，你应该能够：

* 解释 BFCL 的五个类别（simple、multiple、parallel、parallel_multiple、multi-turn），以及在边缘量化下哪一类最先失效
* 说清 AST 评分与可执行评分的区别，并选出你的 repo 真正需要的那一种
* 推导出一个工具调用参数匹配率，它能拆解成你可以着手处理的失败类型（工具错、参数名错、参数类型错、参数值错、幻觉工具、缺少必需参数）
* 针对你自己的 `ToolDispatcher` 目录搭起一个最小 BFCL 风格 harness，并产出一张分类准确率表
* 为“边缘模型允许交付”定义一个精度一致性预算，而不是靠猜
* 把某一类别的回归追溯到某个具体的 runtime 改动——量化 recipe、prompt 模板、KV-cache 精度、decode 采样器

---


<details>
<summary>English original</summary>

**Lecture: Agent Tool-Dispatch Evaluation with BFCL**

<div class="course-identity edge-ai" markdown="1">
<div class="course-identity__icon">BFCL</div>
<div markdown="1">
<p class="course-identity__eyebrow">Phase 5 · Edge AI · Lecture</p>
<p class="course-identity__title">Measure whether an edge LLM agent actually calls the right tool with the right arguments, and prove your quantization did not break it.</p>
<p class="course-identity__meta">Artifact: BFCL-style harness + category accuracy report · Measure: top-1 tool-call accuracy, argument-match rate, parity vs fp16 reference</p>
</div>
</div>

> *A 4-bit Qwen on a Jetson is only useful if it still picks the right tool.*

An on-device agent ships when two things are true: **it fits the device, and it calls tools correctly**. Phase 5's [Qwen Inference Optimization](../05-Qwen推理优化/README.md) covers the first half — quantization, KV cache, decode optimization on Jetson. This lecture covers the second half — the **measurement framework** that decides whether each of those optimizations silently broke the agent.

The standard benchmark is **BFCL** — the **Berkeley Function-Calling Leaderboard**. It is to LLM tool dispatch what LIBERO is to VLA action parity: a fixed, public, category-broken-down evaluation that lets you compare a candidate model (or a candidate runtime configuration) against a reference on exactly one axis — *does the function call match the ground truth*.

This lecture treats BFCL as a **hardware-quality gate**, not a leaderboard chase: you build a tiny BFCL-style harness, run it against your edge model, and use the **per-category accuracy table** to gate your optimization ladder.

**Layer mapping:** L3-L5. Sits between the inference runtime (L3-L4) and the agent harness above it (L5).

**Role targets:** Edge AI Engineer · Edge Inference Optimization Engineer · Agent Harness / Runtime Engineer · Embedded LLM Eval Engineer.

**Prerequisites:**

* Phase 5 — Edge AI — [Edge LLM Inference Internals](../03-边缘LLM推理内部机制/Lecture-01.md) — you need to already know how a transformer dispatches.
* Phase 5 — Edge AI — [Qwen Inference Optimization](../05-Qwen推理优化/README.md), particularly [Lecture 2 — Quantizing Qwen3-4B to Q4](../05-Qwen推理优化/Lecture-02.md). BFCL is what tells you whether AWQ-INT4 cost you tool-call accuracy.
* Phase 4 Track B — [Jetson Real-Time Inference](../../../阶段4-路线B-Nvidia-Jetson/01-Nvidia-Jetson平台/11-Orin-Nano实时推理/Guide.md) for the deployment target.

**What comes after:** the [VLA Action-Parity Harness](../../04-方向D-机器人/05-VLA优化与动作对齐测试/Lecture-02.md) — the same idea applied to embodied policies, where the "action" is a 7-DoF command instead of a JSON tool call.

By the end of this lecture you should be able to:

* explain the five BFCL categories (simple, multiple, parallel, parallel_multiple, multi-turn) and which one fails first under edge quantization
* tell the difference between AST scoring and executable scoring, and pick the one your repo actually needs
* derive a tool-call argument-match rate that decomposes into the failures you can act on (wrong tool, wrong arg name, wrong arg type, wrong arg value, hallucinated tool, missing required arg)
* stand up a minimal BFCL-style harness against your own `ToolDispatcher` catalog and produce a category accuracy table
* define a parity budget for "the edge model is allowed to ship" instead of guessing
* tie a regression in one category back to a specific runtime change — quantization recipe, prompt template, KV-cache precision, decode sampler

---

</details>

## 1. agent 工具分发循环，端到端

抛开营销话术，一个 LLM agent 的 tick 就是：

```text
user turn ──► prompt assembler ──► tokenizer ──► LLM forward pass
                  ▲                                     │
                  │                                     ▼
                  │                              sampling / decode
                  │                                     │
                  │                                     ▼
                  │                          raw output (text or JSON)
                  │                                     │
                  │                                     ▼
                  │                              tool-call parser
                  │                                     │
                  │                                     ▼
                  │                              argument validator
                  │                                     │
                  │                                     ▼
                  │                          dispatcher / ToolDef table
                  │                                     │
                  │                                     ▼
                  │                              tool invocation
                  │                                     │
                  └────────────── result + new turn ◄───┘
```

BFCL 评测能在哪些环节抓到回归：

| 阶段 | 评测应捕获的失效 | BFCL 信号 |
|-------|-------------------------------|-------------|
| 提示词组装器 | 工具目录漂移——提示词宣传了 dispatcher 未实现的工具，或反之 | 分类准确率下降，*且*失败明细中出现 `unknown_tool` 错误 |
| tokenizer | 模板变更后，工具起止标记的 special token 处理出现回归 | 模型未变而 AST 解析失败激增 |
| LLM 前向传播 | 量化或 KV cache 精度损失使函数调用 logits 产生偏置 | 各分类准确率下降，通常 `multiple` 和 `parallel` 最先受影响 |
| 采样 / decode（逐 token 生成阶段） | 非贪心采样器选中了一个替代但看似合法的调用 | temperature 0 时准确率正常，temperature > 0 时下降 |
| 工具调用解析器 | 解析器对空白 / JSON 引号 / 尾随逗号很脆弱 | 模型输出的 AST 分数正常，但可执行分数不正常 |
| 参数校验器 | 模型输出的参数形状正确但类型错误 | 参数匹配率低于工具名匹配率 |
| Dispatcher | runtime 工具目录已偏离提示模型时所用的 schema | 工具名匹配通过，参数匹配在某个枚举字段上失败 |

跑 BFCL——或你自己那套 BFCL 形状的 harness（agent 运行时框架）——的意义在于，能指着其中某一行说「动的是这一行」。

---

## 2. BFCL 具体是什么

Berkeley Function-Calling Leaderboard](https://gorilla.cs.berkeley.edu/leaderboard.html) 是 UC Berkeley Gorilla 项目的 **公开 benchmark**。每个测试用例是一个 `(user prompt, available functions, ground-truth call)` 元组。模型会看到提示词和函数 schema（用模型期望的提示风格——系统提示词、JSON、XML 等），评分依据是它发出的调用是否与 ground truth 匹配。

### 2.1 分类

在边缘硬件上，各分类的难度并不相同，且在量化下会按特征顺序失效。在 4B 级量化模型上，按实际难度从易到难：

| 分类 | 定义 | 压测点 | 最先失效于 |
|----------|------------|------------------|-------------------|
| `simple` | 一个用户提示词 → 从单个工具发出一次函数调用 | 基本的工具名 + 参数名 binding | 通常不会；这里回归说明健全性检查就挂了 |
| `multiple` | 一个提示词，目录中有多个工具 schema，必须选一个 | 工具选择精度 | LLM 骨干的激进量化 |
| `parallel` | 一个提示词 → 对同一工具多次调用 | 参数枚举与列表形状输出 | 低位宽 KV cache、过短的 decode 预算 |
| `parallel_multiple` | 一个提示词 → 跨多个工具多次调用 | 同时考察选择与枚举 | 精度下降叠加 |
| `multi_turn` / `live` | 依赖前序轮次状态的多轮工具对话 | prefix caching 正确性、KV cache 淘汰策略 | paged-attention 或 prefix-cache 的 bug |
| `irrelevance` / `relevance` | 正确答案是 *不调用任何工具* 或 *只调用这一个* | 拒答行为、工具选择精度 | 过于激进的提示词压缩，把目录丢掉了 |

当你看到「新量化下 BFCL 分数掉了 4 pp」却没有分类明细时，你面对的是一个掩盖了 §1 中哪一行真正动了的数字。务必按分类汇报。


<details>
<summary>English original</summary>

**1. The agent tool-dispatch loop, end to end**

Strip the marketing away and an LLM agent tick is:

```text
user turn ──► prompt assembler ──► tokenizer ──► LLM forward pass
                  ▲                                     │
                  │                                     ▼
                  │                              sampling / decode
                  │                                     │
                  │                                     ▼
                  │                          raw output (text or JSON)
                  │                                     │
                  │                                     ▼
                  │                              tool-call parser
                  │                                     │
                  │                                     ▼
                  │                              argument validator
                  │                                     │
                  │                                     ▼
                  │                          dispatcher / ToolDef table
                  │                                     │
                  │                                     ▼
                  │                              tool invocation
                  │                                     │
                  └────────────── result + new turn ◄───┘
```

Everywhere a BFCL eval can catch a regression:

| Stage | Failure the eval should catch | BFCL signal |
|-------|-------------------------------|-------------|
| Prompt assembler | tool catalog drift — the prompt advertises a tool the dispatcher does not implement, or vice versa | category accuracy drops *and* an `unknown_tool` error appears in failure breakdown |
| Tokenizer | special-token handling for tool start/end markers regresses after a template change | AST parse failures spike with no model change |
| LLM forward pass | quantization or KV-cache precision loss biases the function-call logits | per-category accuracy drops, often `multiple` and `parallel` first |
| Sampling / decode | non-greedy sampler picks an alternate but valid-looking call | accuracy looks fine at temperature 0, drops at temperature > 0 |
| Tool-call parser | parser is brittle to whitespace / JSON quoting / trailing commas | AST score is healthy on the model output but executable score is not |
| Argument validator | the model emits args of the right shape but wrong types | argument-match rate falls below tool-name-match rate |
| Dispatcher | runtime tool catalog has drifted from the schema the model was prompted with | tool-name-match passes, argument-match fails on an enum field |

The point of running BFCL — or your own BFCL-shaped harness — is to be able to point at one of those rows and say "this is the row that moved."

---

**2. What BFCL is, concretely**

The [Berkeley Function-Calling Leaderboard](https://gorilla.cs.berkeley.edu/leaderboard.html) is a **public benchmark** from the Gorilla project at UC Berkeley. Each test case is a `(user prompt, available functions, ground-truth call)` tuple. The model is shown the prompt and the function schemas (in the prompting style the model expects — system prompt, JSON, XML, etc.) and is graded on whether its emitted call matches the ground truth.

**2.1 Categories**

The categories are not equally hard on edge hardware, and they fail in characteristic orders under quantization. From easiest to hardest in practice on a 4B-class quantized model:

| Category | Definition | What it stresses | Fails first under |
|----------|------------|------------------|-------------------|
| `simple` | one user prompt → one function call from a single tool | basic tool-name + arg-name binding | nothing usually; a regression here is a sanity-check fail |
| `multiple` | one prompt, multiple tool schemas in the catalog, must pick one | tool selection precision | aggressive quant on the LLM backbone |
| `parallel` | one prompt → multiple calls to the same tool | argument enumeration and list-shape outputs | low-bit KV cache, short decode budgets |
| `parallel_multiple` | one prompt → multiple calls across multiple tools | both selection and enumeration at once | combined precision drops |
| `multi_turn` / `live` | multi-turn dialogues with tools that depend on prior turn state | prefix caching correctness, KV-cache eviction policy | paged-attention or prefix-cache bugs |
| `irrelevance` / `relevance` | the right answer is *do not call any tool* or *call exactly this one* | refusal behavior, tool-selection precision | over-aggressive prompt-compression that drops the catalog |

When you see "BFCL score dropped 4 pp on the new quantization" without a category breakdown, you are looking at a number that hides which row of §1 actually moved. Always report by category.

</details>

### 2.2 评分模式

BFCL 有两种评分模式，它们捕获的是不同的 bug：

* **AST scoring** — 把模型发出的调用解析成抽象语法树，将函数名与参数树同 ground truth 比较。快、确定性的、不需要实际运行工具。能捕获模型错误，但捕获不了 parser 错误。这是每次 commit 在 CI 里用的方式。
* **Executable scoring** — 真正调用工具，比较副作用或返回值。能捕获 parser 错误、dispatcher 漂移、schema 不匹配以及工具实现 bug。更慢、有状态。这是在提升一个检查点之前要跑的。

一个 AST 拿 88%、executable 拿 71% 的模型，有 **parser/dispatcher 漂移问题**，不是模型问题。一个 AST 拿 71%、executable 拿 71% 的模型，有 **模型问题**。知道自己属于哪一种，决定了是一小时修好，还是追着错误的环节查一周。

### 2.3 参数匹配分解

单个 AST 不匹配不具备可操作性。把它分解成你能修的失效模式：

| 失效模式 | 定义 | 最常见原因 |
|---------|------------|-------------------|
| `wrong_tool` | 调用了一个存在但不是 ground truth 的工具 | 量化之下工具选择崩掉，或两个工具名几乎相同 |
| `hallucinated_tool` | 调用了不在目录中的工具 | 提示词模板丢了目录部分，或模型在复述预训练里的工具 |
| `missing_required_arg` | 缺少一个必需参数 | 激进的提示词压缩丢掉了 schema 描述 |
| `extra_arg` | 出现了 schema 里没有的参数 | 模型在幻觉字段；通常是跨工具混淆的信号 |
| `wrong_arg_type` | 名字对、JSON 类型错（例如 `"true"` vs `true`） | 通常在 parser 一侧；检查 executable 得分 |
| `wrong_arg_value` | 类型对、值错（例如 `"living_room"` vs `"livingroom"`） | enum 归一化，或训练数据漂移 |
| `wrong_arg_name` | 值对、键错（例如 `room` vs `entity`） | 提示词与 runtime 之间的目录漂移 — 见 §6 |

本讲的交付产物是一份 CSV，每个测试用例一行，并填好这些列。按类别聚合成一张小表，维护者 30 秒就能读完。

---

## 3. 为什么这一讲放在边缘 AI 里

BFCL 是一种模型评估，但它所把关的动作是一个 *runtime 决策*：

* “我能不能在这台 Jetson 上以 AWQ-INT4 交付 Qwen3-4B，还是需要 Q5_K_M？”→ 按类别看 BFCL 准确率相对 fp16 参考的精度一致性。
* “我能不能打开 FP8 KV cache 来装下 4096-token 的上下文？”→ BFCL `multi_turn` 精度一致性，因为长前缀正是 KV cache 精度真正显形的地方。
* “我能不能删掉每个工具的例子来压缩系统提示词？”→ BFCL `multiple` 和 `irrelevance` 精度一致性，因为那正是这些例子换来的东西。
* “我能不能从已发布的提示词模板切到更紧凑的 ChatML 变体？”→ BFCL AST 精度一致性，因为 parser 往往是模板相关的。

这些问题没有一个能靠困惑度数字、MMLU 得分或感觉来回答。它们要靠在实际边缘产物上跑出的 BFCL 式类别准确率表来回答。

---


<details>
<summary>English original</summary>

**2.2 Scoring modes**

BFCL has two scoring modes, and they catch different bugs:

* **AST scoring** — parse the model's emitted call as an abstract syntax tree, compare the function name and argument tree to the ground truth. Fast, deterministic, no live tools needed. Catches model errors but not parser errors. This is what you use in CI on every commit.
* **Executable scoring** — actually invoke the tool, compare side effects or return values. Catches parser errors, dispatcher drift, schema mismatches, and tool implementation bugs. Slower and stateful. This is what you run before promoting a checkpoint.

A model that scores 88% AST and 71% executable has a **parser/dispatcher drift problem**, not a model problem. A model that scores 71% AST and 71% executable has a **model problem**. Knowing which one you have is the difference between fixing it in an hour and chasing the wrong stage for a week.

**2.3 Argument-match decomposition**

A single AST mismatch is not actionable. Decompose it into the failure modes you can fix:

| Failure | Definition | Most common cause |
|---------|------------|-------------------|
| `wrong_tool` | called a tool that exists but is not the ground truth | tool selection collapsed under quantization, or two tools have near-identical names |
| `hallucinated_tool` | called a tool not in the catalog | prompt template lost the catalog section, or the model is regurgitating a tool from pretraining |
| `missing_required_arg` | a required arg is absent | aggressive prompt compression dropped the schema description |
| `extra_arg` | an arg not in the schema is present | model is hallucinating fields; often a sign of cross-tool confusion |
| `wrong_arg_type` | right name, wrong JSON type (e.g. `"true"` vs `true`) | usually parser-side; check executable score |
| `wrong_arg_value` | right type, wrong value (e.g. `"living_room"` vs `"livingroom"`) | enum normalization, or training-data drift |
| `wrong_arg_name` | right value, wrong key (e.g. `room` vs `entity`) | catalog drift between prompt and runtime — see §6 |

The deliverable artifact for this lecture is a CSV with one row per test case and these columns populated. Aggregate per category to a tiny table the maintainer can read in 30 seconds.

---

**3. Why this lecture lives in Edge AI**

BFCL is a model evaluation, but the action it gates is a *runtime decision*:

* "Can I ship Qwen3-4B at AWQ-INT4 on this Jetson, or does it need to be Q5_K_M?" → BFCL accuracy parity vs fp16 reference, per category.
* "Can I enable FP8 KV cache to fit a 4096-token context?" → BFCL `multi_turn` parity, because long prefixes are where KV-cache precision actually shows up.
* "Can I compress the system prompt by removing the per-tool examples?" → BFCL `multiple` and `irrelevance` parity, because that is what the examples were buying you.
* "Can I switch from the published prompt template to a tighter ChatML variant?" → BFCL AST parity, because parsers tend to be template-specific.

None of those questions are answerable from a perplexity number, MMLU score, or vibes. They are answerable from a BFCL-style category accuracy table run on the actual edge artifact.

---

</details>

## 4. 极简的 BFCL 风格 harness

不必重新实现 Berkeley 的 harness（agent 运行时框架），也能拿到工程价值。一份 **200 行的 Python 脚本**足以给自己的优化阶梯设门控。骨架如下：

```text
harness/
├── manifest.yaml             # categories, seed list, tool catalog manifest, tolerance budgets
├── fixtures/
│   ├── simple/000.json       # {prompt, available_tools, ground_truth_call}
│   ├── multiple/...
│   ├── parallel/...
│   └── multi_turn/...
├── adapters/
│   ├── openai_compatible.py  # talks to vLLM, llama.cpp server, TRT-LLM, etc.
│   └── local_llama_cpp.py    # in-process for tiny models
├── parser/
│   └── ast.py                # tolerant JSON / function-call parser, returns ToolCall AST
├── score/
│   ├── ast_score.py          # ground-truth vs parsed AST → match + failure tag
│   └── exec_score.py         # invokes real ToolDef, compares side effects
├── report/
│   ├── per_case.csv          # one row per test, with all §2.3 failure tags
│   └── category_table.md     # the 30-second summary
└── gate.py                   # CI entry, exits non-zero if any category misses its budget
```

值得首批交付的一组类别是 **simple、multiple、parallel、irrelevance、multi_turn**。live-tool 类别留到 §5 再处理。

有五个设计选择值得一开始就守住：

1. **适配器只暴露 `complete(prompt, tool_schemas) -> raw_string`，别的不暴露。** 正是这一点让同一套 harness 能评测 vLLM、llama.cpp 和 TensorRT-LLM。
2. **解析器对*输入*宽容，对*输出*严格。** 接受模型的怪癖（尾随逗号、单引号、code fence 包裹）；输出一个严格、可供 scorer 比较的 `ToolCall` 值。把每一处宽容都打上 `parser_repair` 标记，以便发现模板回归。
3. **scorer 绝不伸手进模型内部。** logit 级检查有用，但属于另一套 harness；这里只对输出的字符串打分。
4. **容差预算按类别存放在 `manifest.yaml` 中。** 而不是一个总体数字。`simple` 掉 2 pp 与 `multi_turn` 掉 2 pp 是不同类型的失败。
5. **门控在失败时以非零码退出。** 契约与 [VLA action-parity gate](../../04-方向D-机器人/05-VLA优化与动作对齐测试/Lecture-02.md) 相同（VLA 即视觉-语言-动作模型）——manifest 条目缺失或参考 SHA 过期都会让门控*失败*；对 unknown 的默认处理是“不要发布”。

---

## 5. 针对真实 `ToolDef` catalog 的可执行打分

AST 打分足以捕获模型回归。可执行打分才能捕获那些**静默的集成 bug**——它们让生产环境中的 agent 行为失常，即便 BFCL 榜单数字看起来正常。

这一模式取自真实的边缘 agent 代码库：

1. runtime 维护一张带类型的 `ToolDef` 表——每个工具一项，含参数的 JSON schema 以及 Rust/Python 实现。
2. BFCL prompt 是*从*这张表生成的，而不是来自手工维护的列表。harness 断言 `len(prompt_catalog) == len(runtime_catalog)`，并在向模型宣传的工具没有实现（反之亦然）时产生 `catalog_drift` 失败。
3. dispatcher 暴露一个 `simulate(call: ToolCall) -> side_effects` 入口点。一次成功的可执行测试会调用 `simulate`，观察副作用，并与记录下来的参考 trace 比较。
4. 对类家庭自动化的工具而言，“假”provider 实现（不连真实网络、不做真实动作）是必需的，这样 BFCL 运行才是封闭的。

实践中它捕获的回归是：贡献者给某个带类型工具的 enum 加了一个新 action，更新了 runtime，却忘了更新用于 prompt 的 catalog。AST 分数正常——模型根本不知道有新 action。可执行分数在需要该新 action 的测试用例上下降，因为 runtime 会拒绝省略它的调用。反过来同理——从 runtime 中删掉一个 action、却把它留在 prompt 里，模型就会欣然发出一个 dispatcher 拒绝的调用。

在真实代码库中，这表现为一类反复出现的 bug。每个代码库的修法都一样：**从 runtime 派生 BFCL catalog，而不是从手工维护的常量派生，并写一个把两者锁在一起的回归测试。** 那一行测试比 5,000 个 BFCL 用例更有价值。

---


<details>
<summary>English original</summary>

**4. A minimal BFCL-style harness**

You do not need to reimplement Berkeley's harness to get the engineering value. A **200-line Python script** is enough to gate your own optimization ladder. The skeleton:

```text
harness/
├── manifest.yaml             # categories, seed list, tool catalog manifest, tolerance budgets
├── fixtures/
│   ├── simple/000.json       # {prompt, available_tools, ground_truth_call}
│   ├── multiple/...
│   ├── parallel/...
│   └── multi_turn/...
├── adapters/
│   ├── openai_compatible.py  # talks to vLLM, llama.cpp server, TRT-LLM, etc.
│   └── local_llama_cpp.py    # in-process for tiny models
├── parser/
│   └── ast.py                # tolerant JSON / function-call parser, returns ToolCall AST
├── score/
│   ├── ast_score.py          # ground-truth vs parsed AST → match + failure tag
│   └── exec_score.py         # invokes real ToolDef, compares side effects
├── report/
│   ├── per_case.csv          # one row per test, with all §2.3 failure tags
│   └── category_table.md     # the 30-second summary
└── gate.py                   # CI entry, exits non-zero if any category misses its budget
```

A useful first set of categories to ship is **simple, multiple, parallel, irrelevance, multi_turn**. Skip the live-tool categories until §5.

Five design choices worth defending up front:

1. **Adapters expose `complete(prompt, tool_schemas) -> raw_string`, nothing else.** This is what lets you grade vLLM, llama.cpp, and TensorRT-LLM with the same harness.
2. **The parser is tolerant on the *input* and strict on the *output*.** Accept the model's quirks (trailing commas, single quotes, code-fence wrappers); emit a strict `ToolCall` value the scorer can compare. Tag every leniency as a `parser_repair` so you can spot template regressions.
3. **The scorer never reaches into the model.** Logit-level checks are useful but belong in a different harness; here we grade emitted strings.
4. **Tolerance budgets live in `manifest.yaml`, per category.** Not a single overall number. A 2 pp drop in `simple` is a different failure from a 2 pp drop in `multi_turn`.
5. **The gate exits non-zero on failure.** Same contract as the [VLA action-parity gate](../../04-方向D-机器人/05-VLA优化与动作对齐测试/Lecture-02.md) — a missing manifest entry or a stale reference SHA *fails* the gate; default for unknown is "do not ship."

---

**5. Executable scoring against a real `ToolDef` catalog**

AST scoring is enough to catch model regressions. Executable scoring catches the **silent integration bugs** that make production agents misbehave even when BFCL leaderboard numbers look fine.

The pattern, drawn from real edge-agent codebases:

1. The runtime owns a typed `ToolDef` table — one entry per tool, with a JSON schema for arguments and a Rust/Python implementation.
2. The BFCL prompt is generated *from* that table, not from a hand-maintained list. The harness asserts `len(prompt_catalog) == len(runtime_catalog)` and emits a `catalog_drift` failure if a tool advertised to the model has no implementation, or vice versa.
3. The dispatcher exposes a `simulate(call: ToolCall) -> side_effects` entry point. A successful executable test calls `simulate`, observes the side effects, and compares to a recorded reference trace.
4. A "fake" provider implementation (no real network, no real actuation) is mandatory for the home-automation-like tools, so the BFCL run is hermetic.

The regression this catches in practice: a contributor adds a new action to a typed tool's enum, updates the runtime, and forgets to update the catalog the model is prompted with. AST score is fine — the model never learns about the new action. Executable score drops on the test cases that need the new action because the runtime rejects calls that omit it. Same pattern in reverse — drop an action from the runtime, leave it in the prompt, and the model happily emits a call that the dispatcher refuses.

In real codebases this shows up as a recurring class of bug. The fix is the same in every codebase: **derive the BFCL catalog from the runtime, not from a hand-maintained constant, and write the regression test that locks the two together.** That single line of test is worth more than 5,000 BFCL cases.

---

</details>

## 6. 边缘端出货的容差预算

与 [VLA（视觉-语言-动作模型）harness（agent 运行时框架）容差小节](../../04-方向D-机器人/05-VLA优化与动作对齐测试/Lecture-02.md) 相同的逻辑——从两个下限推导预算，并取最小值：

1. **Reference-vs-reference 下限。** 用不同 seed 跑两次 fp16 reference。差值就是你的噪声下限——超过它的 candidate 是统计上不同，而不只是采样不同。
2. **产品下限。** 询问 agent 集成方，产品在每个类别上能承受多大准确率下降。`simple` 下降 2 pp 可能是发布阻塞项；如果产品从不使用 `parallel_multiple`，其下降 2 pp 可能没问题。

Jetson 级家庭 agent 在 4096-token 上下文下出货的示例预算：

| 类别 | Reference fp16（200 个用例） | Candidate 预算（绝对 pp） | 原因 |
|----------|-----------------------------|-------------------------------|------|
| `simple` | 0.93 | -2 | one-call dispatch 是承重路径；任何下降都会导致事故上线 |
| `multiple` | 0.86 | -3 | tool selection；如果产品工具数 ≤8，适度下降可接受 |
| `parallel` | 0.78 | -4 | 多数产品实际不使用并行调用 |
| `parallel_multiple` | 0.62 | -5 | 理想目标；v1 出货不要求 |
| `irrelevance` | 0.81 | -2 | 拒绝正确性；误触发的工具调用对用户可见 |
| `multi_turn` | 0.70 | -3 | 长上下文——也由 KV-cache 回归套件单独覆盖 |

当 **所有** 行都在预算内时，candidate 才可出货。加权平均通过不算通过——它会掩盖出问题的那一行。始终按类别阅读。

---

## 7. harness 让你能讲的硬件侧故事

这些就是 BFCL harness 被构建来回答的优化阶梯问题。与 [Qwen Optimization Lecture 2](../05-Qwen推理优化/Lecture-02.md) 中相同的那组问题，现在附上了测量。

| 优化 | 你预期会动的指标 | 在 4B 级模型上实际会动的指标 |
|--------------|-------------------------|-----------------------------------------|
| bf16 → fp16 权重 | 没有 | 通常没有；如果 `multiple` 下降 >1 pp，怀疑易出现 Inf/NaN 的 head，并检查 norm 融合 |
| AWQ-INT4 backbone，LM head fp16 | 各处小幅下降 | 通常 `simple` 和 `multiple` 上 -1 到 -3 pp，`parallel_multiple` 上 -3 到 -5 pp；`multi_turn` 很少更差 |
| INT3 backbone | 更大下降 | `simple` 上 -4 到 -8 pp；对 agent 产品不值得 |
| FP8 KV cache | 仅在长上下文下降 | `multi_turn` -2 到 -5 pp，视上下文长度而定；`simple` 不变 |
| 投机解码，draft = 1B | 如果 accept-reject 正确则没有 | AST 上没有变化；延迟改善；如果 AST 下降，你的验证器就是错的 |
| 提示词模板变更（ChatML → 紧凑变体）| 如果 parser 能感知模板则没有 | AST 分数先于模型变动——几乎肯定是 parser 修复问题，修 parser |
| 通过删除工具示例压缩系统提示词 | `multiple` 和 `irrelevance` 上下降 | `irrelevance` 上 -3 到 -7 pp；`multiple` 上更小 |
| 将上下文窗口从 8192 降到 4096 | `multi_turn` 长用例上下降 | 长前缀子集上 -5 到 -10 pp；`simple` 上持平 |
| 采样器温度 0 → 0.3 | 小幅随机性下降 | 统一 -1 到 -2 pp；如果 `simple` 下降更多，你的模型在接近的并列项上过度自信但错误 |
| 用蒸馏 student 模型替换已发布模型 | 取决于 student 模型 | 重新跑 *所有* 类别；不要外推 |

硬件工程师的工作是以最低部署成本让 candidate 进入预算内。harness 就是让你不再猜测的东西。

---


<details>
<summary>English original</summary>

**6. Tolerance budgets for an edge ship**

Same logic as the [VLA harness tolerance section](../../04-方向D-机器人/05-VLA优化与动作对齐测试/Lecture-02.md) — derive the budget from two floors and take the min:

1. **Reference-vs-reference floor.** Run the fp16 reference twice with different seeds. The difference is your noise floor — a candidate that exceeds it is statistically different, not just sampled differently.
2. **Product floor.** Ask the agent integrator what accuracy drop the product can absorb on each category. A 2 pp drop in `simple` may be a release blocker; a 2 pp drop in `parallel_multiple` may be fine if the product never uses it.

Example budget for a Jetson-class home-agent shipping at 4096-token context:

| Category | Reference fp16 (200 cases) | Candidate budget (absolute pp) | Why |
|----------|-----------------------------|-------------------------------|------|
| `simple` | 0.93 | -2 | one-call dispatch is the load-bearing path; any drop ships incidents |
| `multiple` | 0.86 | -3 | tool selection; modest drop tolerable if product has ≤8 tools |
| `parallel` | 0.78 | -4 | most products do not actually use parallel calls |
| `parallel_multiple` | 0.62 | -5 | aspirational; not required for v1 ship |
| `irrelevance` | 0.81 | -2 | refusal correctness; spurious tool calls are user-visible |
| `multi_turn` | 0.70 | -3 | long context — covered separately by KV-cache regression suite too |

The candidate ships when **all** rows are inside budget. A pass on the weighted average is not a pass — it hides the row that broke. Read by category, always.

---

**7. Hardware-side stories the harness lets you tell**

These are the optimization-ladder questions the BFCL harness was built to answer. Same set as in [Qwen Optimization Lecture 2](../05-Qwen推理优化/Lecture-02.md), now with a measurement attached.

| Optimization | What you expect to move | What actually moves on a 4B-class model |
|--------------|-------------------------|-----------------------------------------|
| bf16 → fp16 weights | nothing | usually nothing; if `multiple` drops by >1 pp, suspect Inf/NaN-prone heads and check norm fusion |
| AWQ-INT4 backbone, LM head fp16 | small drop everywhere | typically -1 to -3 pp on `simple` and `multiple`, -3 to -5 pp on `parallel_multiple`; `multi_turn` rarely worse |
| INT3 backbone | larger drop | -4 to -8 pp on `simple`; not worth it for an agent product |
| FP8 KV cache | drop on long context only | `multi_turn` -2 to -5 pp depending on context length; `simple` unchanged |
| Speculative decoding, draft = 1B | none if accept-reject is correct | nothing on AST; latency improves; if AST drops, your verifier is wrong |
| Prompt-template change (ChatML → tight variant) | none if parser is template-aware | AST score moves before model — almost certainly a parser repair issue, fix the parser |
| Compress system prompt by dropping tool examples | drop on `multiple` and `irrelevance` | -3 to -7 pp on `irrelevance`; smaller on `multiple` |
| Reduce context window 8192 → 4096 | drop on `multi_turn` long cases | -5 to -10 pp on the long-prefix subset; flat on `simple` |
| Sampler temperature 0 → 0.3 | small randomness drop | -1 to -2 pp uniformly; if `simple` drops more, your model is over-confident-but-wrong on close ties |
| Replace published model with a distilled student | depends on student | re-run *all* categories; do not extrapolate |

The hardware engineer's job is to bring the candidate inside the budget at the lowest deployment cost. The harness is what lets you stop guessing.

---

</details>

## 8. 实验 — 搭建一个小型 BFCL 门禁

建议 1-2 天完成。

1. **选定目标。** 默认：fp16 的 Qwen3-4B-Instruct（参考）与 AWQ-INT4（候选），两者都在你的开发机上通过 vLLM 或 llama.cpp 提供服务。如果有可用的 Jetson AGX Orin，就把候选版放到 Jetson 上跑。
2. **手写 30 个 fixture。** 从 §2.1 的每个类别各取五个，再加上 `irrelevance` 集合。每个 fixture 是一个 JSON 文件，包含 `prompt`、`available_tools`、`ground_truth_call`。用你自己的 tool schema —— `home_control`、`set_timer`、`search` 等 —— 而不是某个公开列表。重点是评测*你的* agent，而不是 Berkeley 的。
3. **实现 §4 的 harness（agent 运行时框架）骨架。** 适配器、解析器、AST 评分器、逐用例 CSV、类别表。
4. **运行参考版与候选版。** 产出两份 CSV，每个模型一张类别表。
5. **计算失效分解。** 对每个未命中的用例，用 §2.3 的某个失效模式打标签。汇总成按类别、按模式的表。
6. **在 `manifest.yaml` 中设定容差预算**，由 §6 推导得到。运行 `gate.py`，确认它在至少一个故意做坏的候选版上以非零码退出（例如丢掉系统提示词中的 tool catalog，看着 `multiple` 崩掉）。
7. **（进阶）** 针对一个假的 home provider 加上可执行评分。证明一个故意漂移的 catalog（runtime 里有但提示词里没有的工具，或反之）在可执行评分上失败，而在 AST 评分上通过。

本实验的通过标准：另一位工程师能克隆你的 repo、运行 `python gate.py --candidate qwen3-4b-awq-int4 --manifest manifest.yaml`，并在同一硬件等级上复现你的通过/失败判定。产物是 harness + 报告，而不是一个数字。

---

## 9. 本讲如何与路线图其余部分衔接

| 你完成的内容 | 接下来去哪 |
|-----------------|--------------------|
| 边缘 agent 能跑起来，但工具调用准确率未知 | 本讲 |
| 你有了 fp16 vs INT4 的 BFCL 式类别表 | 反馈回 [Qwen 推理优化，第 2 讲](../05-Qwen推理优化/Lecture-02.md) 以挑选满足你预算的量化方案 |
| 你有了 BFCL 式 harness | 同样的形态可为具身策略的 [VLA 动作精度一致性 harness](../../04-方向D-机器人/05-VLA优化与动作对齐测试/Lecture-02.md) 把关 |
| 你需要把 harness 接进 CI | 见 [MLSys Stage 0 测量规范](../../07-方向G-ML系统工程/Guide.md) —— 同样的契约，不同的指标 |
| 你需要长上下文多轮精度一致性 | 与 [长上下文 MoE 基础训练](../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/07-长上下文MoE基础训练/07-Long-Context-Evaluation.md) 的评估模式结合 |

---

## 自检

1. 你的 AWQ-INT4 候选版总体得分 89%，而 fp16 参考版是 91%。维护者说「发吧」。在同意之前，你会让他看哪一张表？即使在总体数字过关的情况下，具体出现什么情况你会拒绝？
2. 某个候选版在从公开的 Qwen 提示词模板换成自定义 ChatML 变体后，AST 分数掉了 5 pp。权重没变。§1 表中最可能是哪一级发生了变化？为了确认，你在 harness 里——而不是模型里——最先改的是什么？
3. 启用 FP8 KV cache 后，你的 `multi_turn` 类别掉了 6 pp。`simple` 没变。为什么这与「罪魁祸首是 FP8 KV」一致？在 harness 里加哪一个额外实验就能在不重新量化的前提下锁定它？
4. 在 `multiple` 上，可执行评分比 AST 评分低 12 pp。这种模式指向哪一类 bug？你预计失效分解里哪个 §2.3 失效标签会占主导？
5. 一个贡献者的 PR 把 BFCL 总体数字抬高了 2 pp，但把 `irrelevance` 类别压低了 4 pp。这个 PR 是赢、是输，还是「看情况」——具体看什么？

---


<details>
<summary>English original</summary>

**8. Lab — Stand up a tiny BFCL gate**

Suggested 1-2 day lab.

1. **Pick a target.** Default: Qwen3-4B-Instruct at fp16 (reference) and AWQ-INT4 (candidate), both served via vLLM or llama.cpp on your dev machine. If you have a Jetson AGX Orin available, do the candidate on the Jetson.
2. **Hand-write 30 fixtures.** Five per category from §2.1, plus the `irrelevance` set. Each fixture is a JSON file with `prompt`, `available_tools`, `ground_truth_call`. Use your own tool schemas — `home_control`, `set_timer`, `search`, etc. — not a public list. The point is to grade *your* agent, not Berkeley's.
3. **Implement the harness skeleton in §4.** Adapter, parser, AST scorer, per-case CSV, category table.
4. **Run reference vs candidate.** Produce two CSVs, one category table per model.
5. **Compute the failure decomposition.** For every miss, tag with one of the §2.3 failure modes. Aggregate to a per-category, per-mode table.
6. **Set tolerance budgets** in `manifest.yaml` derived from §6. Run `gate.py` and confirm it exits non-zero on at least one intentionally-bad candidate (e.g. drop the system prompt's tool catalog and watch `multiple` collapse).
7. **(Stretch)** Add executable scoring against a fake home provider. Show that a deliberately-drifted catalog (a tool in the runtime but not in the prompt, or vice versa) fails executable scoring while AST scoring passes.

Pass criterion for the lab: another engineer can clone your repo, run `python gate.py --candidate qwen3-4b-awq-int4 --manifest manifest.yaml`, and reproduce your pass/fail decision on the same hardware class. The artifact is the harness + the report, not a single number.

---

**9. How this lecture connects to the rest of the roadmap**

| What you finish | Where it goes next |
|-----------------|--------------------|
| Edge agent runs but tool-call accuracy is unknown | this lecture |
| You have a BFCL-style category table for fp16 vs INT4 | feed back into [Qwen Inference Optimization, Lecture 2](../05-Qwen推理优化/Lecture-02.md) to pick the quantization that meets your budget |
| You have a BFCL-style harness | the same shape gates the [VLA action-parity harness](../../04-方向D-机器人/05-VLA优化与动作对齐测试/Lecture-02.md) for embodied policies |
| You need to wire the harness into CI | see the [MLSys Stage 0 measurement discipline](../../07-方向G-ML系统工程/Guide.md) — same contract, different metrics |
| You need long-context multi-turn parity | combine with the [Long-Context MoE Foundation Training](../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/07-长上下文MoE基础训练/07-Long-Context-Evaluation.md) evaluation patterns |

---

**Self-check**

1. Your AWQ-INT4 candidate scores 89% overall vs the fp16 reference's 91%. The maintainer says "ship it." What is the single table you ask them to look at before agreeing, and what specifically would make you refuse even though the overall number passed?
2. A candidate's AST score drops by 5 pp after switching from the published Qwen prompt template to a custom ChatML variant. The weights did not change. What is the most likely stage of §1's table that moved, and what is the first thing you change in the harness — not the model — to confirm it?
3. Your `multi_turn` category drops 6 pp after enabling FP8 KV cache. `simple` is unchanged. Why is this consistent with FP8 KV being the culprit, and what one extra experiment in the harness pins it down without re-quantizing?
4. The executable score is 12 pp below the AST score on `multiple`. What kind of bug does that pattern point at, and which §2.3 failure tag would you expect to dominate the failure breakdown?
5. A contributor PR moves the BFCL overall number up by 2 pp but moves the `irrelevance` category down by 4 pp. Is this PR a win, a loss, or "depends" — and what specifically does it depend on?

---

</details>

## 参考文献

* Berkeley Function-Calling Leaderboard — [排行榜](https://gorilla.cs.berkeley.edu/leaderboard.html), [Gorilla 项目](https://gorilla.cs.berkeley.edu/), [GitHub](https://github.com/ShishirPatil/gorilla)
* BFCL v1/v2/v3 数据集卡片 — [Hugging Face](https://huggingface.co/datasets/gorilla-llm/Berkeley-Function-Calling-Leaderboard)
* OpenAI 函数调用格式（许多模型据此训练的默认 schema）— [API 文档](https://platform.openai.com/docs/guides/function-calling)
* Anthropic 工具使用格式 — [API 文档](https://docs.anthropic.com/en/docs/build-with-claude/tool-use)
* "Gorilla: Large Language Model Connected with Massive APIs" — [论文](https://arxiv.org/abs/2305.15334)（该数据集家族的源头）
* vLLM 工具调用推理服务 — [文档](https://docs.vllm.ai/en/latest/features/tool_calling.html)
* TensorRT-LLM 工具调用路径 — [文档](https://nvidia.github.io/TensorRT-LLM/)
* llama.cpp 用于工具调用的语法约束采样 — [GBNF 语法指南](https://github.com/ggerganov/llama.cpp/blob/master/grammars/README.md)
* 关于 "BFCL catalog derived from the runtime `ToolDef`" 的真实案例参考 — 参见任何附带 typed dispatcher 的生产级边缘 agent 仓库；无论使用何种语言，该模式都相同。

---

## 本路线图的后续内容

* 同一 track 的上一篇：[讲座 — 边缘大语言模型推理内部机制](../03-边缘LLM推理内部机制/Lecture-01.md)
* 同级：[Qwen 推理优化](../05-Qwen推理优化/README.md) — 同一契约的优化侧
* 跨 track：[VLA（视觉-语言-动作模型）优化与动作精度一致性 harness（agent 运行时框架）](../../04-方向D-机器人/05-VLA优化与动作对齐测试/README.md) — 具身领域的对应版本
* 上级：[阶段 5 — 边缘 AI 指南](../Guide.md)


<details>
<summary>English original</summary>

**References**

* Berkeley Function-Calling Leaderboard — [leaderboard](https://gorilla.cs.berkeley.edu/leaderboard.html), [Gorilla project](https://gorilla.cs.berkeley.edu/), [GitHub](https://github.com/ShishirPatil/gorilla)
* BFCL v1/v2/v3 dataset cards — [Hugging Face](https://huggingface.co/datasets/gorilla-llm/Berkeley-Function-Calling-Leaderboard)
* OpenAI function-calling format (the default schema many models train against) — [API docs](https://platform.openai.com/docs/guides/function-calling)
* Anthropic tool-use format — [API docs](https://docs.anthropic.com/en/docs/build-with-claude/tool-use)
* "Gorilla: Large Language Model Connected with Massive APIs" — [paper](https://arxiv.org/abs/2305.15334) (origin of the dataset family)
* vLLM tool-call serving — [docs](https://docs.vllm.ai/en/latest/features/tool_calling.html)
* TensorRT-LLM tool-call paths — [docs](https://nvidia.github.io/TensorRT-LLM/)
* llama.cpp grammar-constrained sampling for tool calls — [GBNF grammars guide](https://github.com/ggerganov/llama.cpp/blob/master/grammars/README.md)
* For a real-world reference of "BFCL catalog derived from the runtime `ToolDef`" — see any production edge-agent repo that ships a typed dispatcher; the pattern is the same regardless of language.

---

**Next in this roadmap**

* Previous in track: [Lecture — Edge LLM Inference Internals](../03-边缘LLM推理内部机制/Lecture-01.md)
* Sibling: [Qwen Inference Optimization](../05-Qwen推理优化/README.md) — the optimization side of the same contract
* Cross-track: [VLA Optimization and Action-Parity Harness](../../04-方向D-机器人/05-VLA优化与动作对齐测试/README.md) — the embodied analog
* Up: [Phase 5 — Edge AI Guide](../Guide.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track C - Edge AI/Agent Tool-Dispatch Evaluation with BFCL/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20C%20-%20Edge%20AI/Agent%20Tool-Dispatch%20Evaluation%20with%20BFCL/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
