---
title: Lecture 01 - 为什么用 agent 做芯片设计：2026 年的格局
description: Lecture 01 - 为什么用 agent 做芯片设计：2026 年的格局
---

# Lecture 01 - 为什么用 agent 做芯片设计：2026 年的格局

**课程：** [Agentic Chip Design 2026](../Guide.md) | **上一讲：** [课程指南](../Guide.md) | **下一讲：** Lecture 02 *(课程大纲)*

---

本讲为课程定下框架：agent 要攻克的设计生产力问题，大语言模型真正能帮上忙的地方与硅的经济性不允许的地方，证明其可行的先例，以及如何在这个快速演变的领域判断任何说法。

本讲涵盖：

1. 设计生产力缺口。
2. 大语言模型/agent 在流程中的位置——以及绝不该出现的位置。
3. 先例：ChipNeMo。
4. 2026 年任务全景（以及实时 leaderboard）。
5. 如何度量——因为在硬件里，评估就是一切。
6. 2026 年的硬件背景，以及瓶颈为何向上游迁移。
7. 与 harness（agent 运行时框架）的关联。

---

## 1. 设计生产力缺口

晶体管预算与产品节奏持续攀升；能编写并**验证** RTL 的工程师数量却没有。造芯片中昂贵而缓慢的部分不是敲 Verilog——而是**验证**：testbench、覆盖率收敛、调试与 signoff 通常要吃掉一次 tapeout 的大部分工程投入。

这种特征——判断密集、例外繁多、淹没在非结构化日志与规格文档里——恰恰是 [AI Agent Development · Building Agents I](../../../../阶段3-人工智能/04-智能体AI与ML工程/01-智能体AI与生成式AI/01-讲座/Lecture-03.md) 中「何时该构建 agent」的信号：决策复杂、靠人工维护的规则脆弱、非结构化数据量大。芯片设计是异常合适的场景，*也*是异常不容错的场景。

---

## 2. agent 适合的位置——以及绝不该出现的位置

RTL 到硅的流程，以及各阶段中实事求是看 agent 的机会：

<div class="lecture-map" markdown>

| 阶段 | agent 机会 | 风险 |
|-------|-------------------|------|
| **Spec → RTL** | 起草 module、重构、把意图转成 Verilog | 幻觉出的、看似合理但错误的逻辑 |
| **验证 / testbench** | 生成测试、追覆盖率、失败分诊 | 测试通过却掩盖了真实 bug |
| **综合 / lint** | 自动修 lint、给出时序修复建议 | 改动功能的修复 |
| **PPA 优化** | 提出面积/功耗/时序折衷 | 局部受益，全局回退 |
| **调试** | 汇总日志、定位 bug、提出补丁 | 自信的错误根因 |
| **Signoff** | 辅助评审，绝不决策 | — |

</div>

> **硅成本的不对称性。** 聊天回复里一个错误的 token 修正起来不花代价；一个到达 **mask set** 的错误 token 要付出数百万美元和数月时间。这就是为什么上述每个阶段都是 **human-in-the-loop 且以评估为闸门**——agent 提方案，确定性的检查器（simulator、formal tool、linter）与人来裁决。agent 是设计回路上的吞吐倍增器，不是自主的 tapeout 按钮。

---

## 3. 先例：ChipNeMo

NVIDIA 的 **ChipNeMo**（[arXiv:2311.00176](https://arxiv.org/abs/2311.00176)，2023）是该领域成长所依托的概念验证。通过把通用大语言模型**领域适配**到芯片设计组织的内部数据（设计、文档、bug 数据库），它在真实设计流程中交付了三个具体应用：

* 一个**工程助手**聊天机器人（架构/设计问答），
* **EDA 脚本生成**（用工具自己的语言驱动它们），
* **bug 报告摘要与分诊。**

可沿用的结论：**领域适配 + 工具接入 + 范围收紧** 胜过天真地使用更大的通用模型。这与 agent 课程中的 harness 论点相同，只是应用到了硅上。

---

## 4. 2026 年任务全景

该领域如今覆盖生成、验证与完整的智能体化流程。被度量最多的任务是 **RTL/Verilog 生成**，在 **[Chip-Design-LLM-Zoo](https://iprc-dip.github.io/Chip-Design-LLM-Zoo/)** 上公开追踪——这是一个实时 leaderboard，按共享 benchmark 上的 **pass@1 / correct-rate** 给模型排名，并区分**微调 vs base** 与**开放 vs 闭源**权重。

<div class="lecture-map" markdown>

| 任务族 | 覆盖内容 |
|-------------|----------------|
| **RTL 生成** | Spec/NL → Verilog module（Zoo 的重点） |
| **验证** | testbench 合成、覆盖率收敛、断言生成 |
| **智能体化 EDA** | 多 agent 的 spec→RTL→验证→调试回路，对 EDA 工具的工具调用 |
| **调试 / PPA** | 失败分诊、lint/时序修复、面积/功耗/时序折衷 |

</div>

我们把 Zoo 当作**阅读时刻的真相**（正如推理课程对待实时 benchmark 面板那样）。具体的「最佳模型」说法几个月就会腐烂——去看 leaderboard，不要凭记忆。

---


<details>
<summary>English original</summary>

**Lecture 01 - Why Agents for Chip Design: The 2026 Landscape**

**Course:** [Agentic Chip Design 2026](../Guide.md) | **Previous:** [Course Guide](../Guide.md) | **Next:** Lecture 02 *(curriculum)*

---

This opener frames the course: the design-productivity problem agents are meant to attack, where LLMs genuinely help versus where silicon economics forbid it, the precedent that proved it works, and how to judge any claim in this fast-moving field.

This lecture covers:

1. The design-productivity gap.
2. Where LLMs/agents fit in the flow — and where they must not.
3. The precedent: ChipNeMo.
4. The 2026 task landscape (and the live leaderboard).
5. How to measure — because in hardware, evaluation is everything.
6. The 2026 hardware backdrop and why the bottleneck moved upstream.
7. The harness connection.

---

**1. The design-productivity gap**

Transistor budgets and product cadence keep climbing; the number of engineers who can write and **verify** RTL does not. The expensive, slow part of building a chip is not typing Verilog — it is **verification**: testbenches, coverage closure, debugging, and signoff routinely consume the majority of a tapeout's engineering effort.

That profile — judgment-heavy, exception-laden, drowning in unstructured logs and specs — is precisely the "when to build an agent" signal from [AI Agent Development · Building Agents I](../../../../阶段3-人工智能/04-智能体AI与ML工程/01-智能体AI与生成式AI/01-讲座/Lecture-03.md): complex decisions, brittle hand-maintained rules, heavy unstructured data. Chip design is an unusually good fit *and* an unusually unforgiving one.

---

**2. Where agents fit — and where they must not**

The RTL-to-silicon flow, with the honest agent opportunity at each stage:

<div class="lecture-map" markdown>

| Stage | Agent opportunity | Risk |
|-------|-------------------|------|
| **Spec → RTL** | Draft modules, refactor, translate intent to Verilog | Hallucinated/plausible-but-wrong logic |
| **Verification / testbench** | Generate tests, chase coverage, triage failures | Tests that pass while masking real bugs |
| **Synthesis / lint** | Auto-fix lint, suggest timing fixes | Fixes that change function |
| **PPA optimization** | Propose area/power/timing trade-offs | Local wins, global regressions |
| **Debug** | Summarize logs, localize bugs, propose patches | Confident wrong root-cause |
| **Signoff** | Assist review, never decide | — |

</div>

> **The silicon-cost asymmetry.** A wrong token in a chat reply is free to fix; a wrong token that reaches a **mask set** costs millions and months. This is why every stage above is **human-in-the-loop and evaluation-gated** — the agent proposes, a deterministic checker (simulator, formal tool, linter) and a human dispose. The agent is a throughput multiplier on the design loop, not an autonomous tapeout button.

---

**3. The precedent: ChipNeMo**

NVIDIA's **ChipNeMo** ([arXiv:2311.00176](https://arxiv.org/abs/2311.00176), 2023) is the proof-of-concept the field grew from. By **domain-adapting** general LLMs to a chip-design org's internal data (designs, docs, bug databases) it delivered three concrete applications inside a real design flow:

* an **engineering assistant** chatbot (architecture/design Q&A),
* **EDA-script generation** (drive the tools in their own languages),
* **bug-report summarization and triage.**

The lesson that carries: **domain adaptation + tool access + tight scope** beat a bigger general model used naively. That is the same harness thesis as the agent course, applied to silicon.

---

**4. The 2026 task landscape**

The field now spans generation, verification, and full agentic flows. The most-measured task is **RTL/Verilog generation**, tracked publicly on the **[Chip-Design-LLM-Zoo](https://iprc-dip.github.io/Chip-Design-LLM-Zoo/)** — a live leaderboard ranking models by **pass@1 / correct-rate** across shared benchmarks, distinguishing **fine-tuned vs base** and **open vs closed** weights.

<div class="lecture-map" markdown>

| Task family | What it covers |
|-------------|----------------|
| **RTL generation** | Spec/NL → Verilog modules (the Zoo's focus) |
| **Verification** | Testbench synthesis, coverage closure, assertion generation |
| **Agentic EDA** | Multi-agent spec→RTL→verify→debug loops, tool-calling into EDA tools |
| **Debug / PPA** | Failure triage, lint/timing fixes, area/power/timing trade-offs |

</div>

We treat the Zoo as **truth-at-time-of-reading** (the way the inference course treats live benchmark dashboards). Specific "best model" claims rot in months — check the leaderboard, not your memory.

---

</details>

## 5. 如何度量 — 评估即一切

在硬件领域，漂亮的对话记录不能用来评判 agent。真正重要的 benchmark：

* **VerilogEval**（[arXiv:2309.07544](https://arxiv.org/abs/2309.07544)）— 从 spec 到 RTL，题库由机器与人工编写；报告 **pass@k**。
* **RTLLM**（[arXiv:2308.05345](https://arxiv.org/abs/2308.05345)）— RTL 生成按**语法** *和***功能**正确性打分。
* **RealBench** — 模块级与系统级任务，将语法与功能分开。

它们内含两条严酷事实：

1. **语法 ≠ 功能。** 能编译、能过 lint 的代码仍可能功能错误 —— 只有 testbench 或形式化检查能抓住它。语法通过率高而功能通过率低，是该领域反复出现的失效模式。
2. **pass@1 才是自主性上诚实的指标。** pass@5（5 个样本取最优）会让模型显得更好；对一个要真正行动的 agent 而言，第一次答案的正确性才算数。

---

## 6. 2026 年的硬件背景

Computex / GTC Taipei 2026（6 月 1–5 日）凸显了*为什么*设计回路如今成了瓶颈：NVIDIA 宣布 **Vera Rubin 全面量产**，Grace-Blackwell 机架约 5 分钟即可组装完成，此外还有 **Cosmos 3** —— 一个完全开放的全模态模型，将文本/图像/视频/音频映射为 action。制造与模型的节奏持续加速；稀缺资源是**经过验证的设计吞吐**。更快的硅让更快的*设计*回路更有价值，而非相反 —— 这正是整门课程的经济学依据。

---

## 7. 与 harness 的衔接

你在 **AI Agent Development 2026** 中构建的一切都适用于此，只需把目标换成硅：

* **run loop** → 生成 RTL，调用工具，检查，迭代至退出条件；
* **工具** → 仿真器、linter、形式化检查器、综合报告解析器；
* **护栏** → 绝不允许未验证的 RTL 往下走；高成本动作以人工为闸门；
* **评估** → pass@1 / 功能覆盖率，而不是感觉。

课程 capstone 正是如此：一个 **genie-claw** 风格的 harness（agent 运行时框架），生成 Verilog 模块，把仿真器/testbench 当作工具驱动，并在护栏内迭代 —— 按 VerilogEval 风格的任务打分。

---

## 关键要点

* 芯片设计的瓶颈是**验证吞吐**，而这类重判断、非结构化的工作非常适合 agent —— 前提是身处残酷的硅成本不对称之下，这要求 **human-in-the-loop + 确定性的检查器**。
* **ChipNeMo** 证明了领域适配的 LLM + 工具 + 范围限定在真实设计组织内可行。
* 一切以 **pass@1 与功能正确性**评判（语法不等于功能）；使用实时的 **Chip-Design-LLM-Zoo**，而不是过期的排行榜。
* 2026 年的硬件节奏（Vera Rubin 量产）让更快的设计回路更有价值 —— 这就是智能体化芯片设计的经济学依据。

---

## 自查

1. 为什么在流程中 agent 价值最高的一环是验证，而不是 RTL 生成？
2. 解释硅成本不对称，以及它对该领域的自主性意味着什么。
3. 某模型在 RTLLM 上语法通过率 90%、功能通过率 45%。这说明了什么，又靠什么抓住这个差距？
4. 对 *agent* 而言，为什么 pass@1 比 pass@5 是更诚实的指标？
5. 说出 ChipNeMo 的三个应用，以及它们对在专门领域部署 LLM 所给出的一般性教训。

---

## 参考文献

* **Chip-Design-LLM-Zoo**（实时排行榜）— [iprc-dip.github.io/Chip-Design-LLM-Zoo](https://iprc-dip.github.io/Chip-Design-LLM-Zoo/)
* **ChipNeMo** — [arXiv:2311.00176](https://arxiv.org/abs/2311.00176)
* **VerilogEval** — [arXiv:2309.07544](https://arxiv.org/abs/2309.07544) · **RTLLM** — [arXiv:2308.05345](https://arxiv.org/abs/2308.05345)
* NVIDIA Computex / GTC Taipei 2026 — [ServeTheHome 报道](https://www.servethehome.com/nvidia-computex-2026-keynote-live-coverage/)
* 衔接：[AI Agent Development 2026 — 何时该构建 agent](../../../../阶段3-人工智能/04-智能体AI与ML工程/01-智能体AI与生成式AI/01-讲座/Lecture-03.md)

---

*下一讲：Lecture 02 — RTL 到硅的流程，以及 agent 的落点（课程大纲）。*


<details>
<summary>English original</summary>

**5. How to measure — evaluation is everything**

In hardware you cannot judge an agent by a nice transcript. The benchmarks that matter:

* **VerilogEval** ([arXiv:2309.07544](https://arxiv.org/abs/2309.07544)) — spec-to-RTL with machine and human-written problem sets; reports **pass@k**.
* **RTLLM** ([arXiv:2308.05345](https://arxiv.org/abs/2308.05345)) — RTL generation scored on **syntax** *and* **functional** correctness.
* **RealBench** — module- and system-level tasks, separating syntax from functionality.

Two hard truths these encode:

1. **Syntax ≠ function.** Code that compiles and lints can still be functionally wrong — and only a testbench or formal check catches it. A high syntax-pass rate with a low functional-pass rate is the field's recurring failure mode.
2. **pass@1 is the honest metric for autonomy.** pass@5 (best of 5 samples) flatters a model; for an agent meant to act, the first answer's correctness is what counts.

---

**6. The 2026 hardware backdrop**

Computex / GTC Taipei 2026 (June 1–5) underlined *why* the design loop is now the bottleneck: NVIDIA announced **Vera Rubin in full production**, with a Grace-Blackwell rack assembled in ~5 minutes, plus **Cosmos 3**, a fully-open omnimodel mapping text/image/video/audio → action. The manufacturing and model cadence keeps accelerating; the scarce resource is **verified design throughput**. Faster silicon makes a faster *design* loop more valuable, not less — which is the economic case for this entire course.

---

**7. The harness connection**

Everything you built in **AI Agent Development 2026** applies here, retargeted to silicon:

* the **run loop** → generate RTL, call a tool, check, iterate to an exit condition;
* **tools** → a simulator, a linter, a formal checker, a synthesis report parser;
* **guardrails** → never let unverified RTL advance; gate high-cost actions on a human;
* **evaluation** → pass@1 / functional coverage, not vibes.

The course capstone is exactly this: a **genie-claw**-style harness that generates a Verilog module, drives a simulator/testbench as tools, and iterates within guardrails — scored on VerilogEval-style tasks.

---

**Key takeaways**

* The chip-design bottleneck is **verification throughput**, and that judgment-heavy, unstructured work is a strong agent fit — under a brutal silicon-cost asymmetry that mandates **human-in-the-loop + deterministic checkers**.
* **ChipNeMo** proved domain-adapted LLMs + tools + scope work inside a real design org.
* Judge everything by **pass@1 and functional correctness** (syntax is not function); use the live **Chip-Design-LLM-Zoo** rather than stale rankings.
* The 2026 hardware cadence (Vera Rubin in production) makes a faster design loop more valuable — the economic case for agentic chip design.

---

**Self-check**

1. Why is verification — not RTL generation — the part of the flow where agents add the most value?
2. Explain the silicon-cost asymmetry and what it implies for autonomy in this domain.
3. A model scores 90% syntax-pass but 45% functional-pass on RTLLM. What does that tell you, and what catches the gap?
4. Why is pass@1 the honest metric for an *agent*, versus pass@5?
5. Name the three ChipNeMo applications and the general lesson they teach about deploying LLMs in a specialized domain.

---

**References**

* **Chip-Design-LLM-Zoo** (live leaderboard) — [iprc-dip.github.io/Chip-Design-LLM-Zoo](https://iprc-dip.github.io/Chip-Design-LLM-Zoo/)
* **ChipNeMo** — [arXiv:2311.00176](https://arxiv.org/abs/2311.00176)
* **VerilogEval** — [arXiv:2309.07544](https://arxiv.org/abs/2309.07544) · **RTLLM** — [arXiv:2308.05345](https://arxiv.org/abs/2308.05345)
* NVIDIA Computex / GTC Taipei 2026 — [ServeTheHome coverage](https://www.servethehome.com/nvidia-computex-2026-keynote-live-coverage/)
* Bridge: [AI Agent Development 2026 — when to build an agent](../../../../阶段3-人工智能/04-智能体AI与ML工程/01-智能体AI与生成式AI/01-讲座/Lecture-03.md)

---

*Next: Lecture 02 — The RTL-to-silicon flow and where agents fit (curriculum).*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track F - AI Chip Design/Agentic Chip Design 2026/Lectures/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20F%20-%20AI%20Chip%20Design/Agentic%20Chip%20Design%202026/Lectures/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
