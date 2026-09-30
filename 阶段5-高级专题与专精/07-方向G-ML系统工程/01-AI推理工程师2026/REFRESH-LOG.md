---
title: 刷新日志 — AI Inference Engineer 2026
description: 刷新日志 — AI Inference Engineer 2026
---

# 刷新日志 — AI Inference Engineer 2026

本课程的每一次实质性更新都会标注日期并列在此处，让读者确切知道讲义锚定的是哪个版本的现实。刷新节奏：默认 6 个月；若有重大模型类别或硬件代际发布，则为 3 个月。

当某讲刷新时，更新其 `## Current as of YYYY-MM` 行，*并*在此处新增一条记录。

---

## 2026-08 — 新增部分：Part 4 — 优化一个真实引擎（10 讲）

新增 **Part 4**，一个实测案例研究，锚定于 [SparkInfer-K3](https://github.com/gittensor-ai-lab/sparkinfer-k3)（MIT）——一个从零手写的 CUDA 引擎，面向 **Kimi K3**（总参数 2.8T，93 层 = 24 层 MLA + 69 层 KDA，896 个路由专家 top-16，latent MoE（混合专家模型）位于 3584，`situ` 激活值，1M 上下文）——运行在单台 **8× H200** 节点上，`UD-IQ1_S`（553 GiB）。课程总讲数 17 → 27 讲。

**为什么这一部分与 Part 1–3 不同。** 那几部分锚定的是*教学锚点*——有代表性的数字，按节奏刷新。Part 4 锚定的则是*一段实测历史*，来源包括 pull request、`reference.lock` commit 历史、封存的评测凭证，以及一份把撤回的数字与真实数字并列记录的 changelog。这里没有任何内容会刷新；它是 2026-08 某一个模型在某一台机器上的历史，可迁移的内容是其中的推理过程。

**主线。** 在真实的 128k 计分上下文下，decode（逐 token 生成阶段）跨约 96 个 PR 从 **1.01 → 60.17 tok/s**，对照同一台机器、同一份权重上的 llama.cpp **18.44**——从落后 18× 到领先 3.26×。共记录 17 次前沿推进，其中单步最大为 4.5×，中位数约 7%：60× 是*乘积*，而非一次发现。Prefill（首字前的整段计算）在 32k 下从 **40.35 → 99.68**，对照 llama.cpp 的 143.88（仍落后 1.44×）。

**各讲内容。** 01 工作负载/基线/阶梯（为什么基线必须是一个固定版本的第三方 fork——上游 llama.cpp 断言 `n_expert ≤ 512`）；02 计分板（正确性门禁排在速度*之前*，top-1 ≥ 0.95 / 七个深度中最差的 mean KL ≤ 0.05，tier = `min(Δ/llama_ref, Δ/frontier)`，只升不降的前沿，以及约 40 个各自堵住一个测量可信度漏洞的已合并 PR）；03 跨四类上限的诊断（带宽 / occupancy / 启动 / 通信），结论是集合通信只占 token 的约 2%，而每层约 30 个未融合 kernel × 93 层才是开销所在；04 启动几何；05 融合 + 激活值量化；06 128k 下的 attention；07 专家分片与 Amdahl 陷阱；08 CUDA graph 常驻；09 批量 prefill；10 静默错误。

**三条超出该案例之外的发现。**（1）harness（agent 运行时框架）测得的上下文是 **64**，而该 slot 名为 `_128`、文档写的是 128k——在 64 下引擎落后参考实现 1.8×，在 128k 下落后 **18×**，于是数周的工作都瞄准了一个没人会跑的上下文。（2）**MoE 提速 3× 反而让 TP 扩展更差**，2.44× → 约 1.1×，因为它留下的复制式 attention 成了整个串行项（串行占比 0.33 → 0.90）。（3）一个 PR 测出 prefill **169.72 tok/s**，却带着两个现存缺陷——一对被转置的步长在 13 层以下被掩盖，以及一个文档标注为默认关闭、实际却按 opt-out 读取的 env gate——并附言*“被污染的遍历更快，因为它在读错误的行。”* 诚实的数字是 98.80。在带宽受限的引擎上，错误与速度正相关。

另外还把 Part 2 Lecture 07 注册进了站点导航，它此前已存在于磁盘上，却缺失于 `mkdocs.yml`。

---

## 2026-06 — 新增讲义：Part 2 Lecture 07（通信层）

新增 **Part 2 Lecture 07——“通信层内部：NCCL、自定义 All-Reduce 与 vLLM Communicator 栈”。** 推理服务 runtime 实际如何在 GPU 之间搬运字节，以 vLLM 的 `distributed/device_communicators/` 为例：三层架构（router → engines → 原语）、催生出 NCCL ring 之上自定义 one-shot/two-shot all-reduce 的小消息延迟问题、融合的 all-reduce+RMSNorm 路径（FlashInfer）、runtime 的 routing/fallback 阶梯，以及作为指向 Part 3 MoE 的前向指针的 all2all。从机制上解释了 Lecture 04 的“TP=8 的 decode 是 comm-bound”。Part 2 现为 7 讲（课程总数 16 → 17）；已更新 Part 2 README、课程 README 的计数/地图、Lecture 06 页脚，以及根目录 roadmap README。模块名固定到 vLLM 0.22 时代的谱系，并标注为版本相关。

---


<details>
<summary>English original</summary>

**Refresh Log — AI Inference Engineer 2026**

Every meaningful update to this course is dated and listed here so readers know exactly what version of reality the lectures are pinned to. Refresh cadence: 6 months default; 3 months if a major model class or hardware generation drops.

When a lecture is refreshed, update its `## Current as of YYYY-MM` line *and* add an entry here.

---

**2026-08 — New part: Part 4 — Optimizing a Real Engine (10 lectures)**

Added **Part 4**, a measured case study anchored on [SparkInfer-K3](https://github.com/gittensor-ai-lab/sparkinfer-k3) (MIT) — a from-scratch CUDA engine for **Kimi K3** (2.8T total, 93 layers = 24 MLA + 69 KDA, 896 routed experts top-16, latent MoE at 3584, `situ` activation, 1M context) on a single **8× H200** node at `UD-IQ1_S` (553 GiB). Course total 17 → 27 lectures.

**Why this part is different from Parts 1–3.** Those pin *teaching anchors* — representative numbers, refreshed on a cadence. Part 4 pins *one measured history*, sourced from pull requests, `reference.lock` commit history, sealed eval receipts, and a changelog that records retracted numbers alongside real ones. Nothing here is refreshed; it is 2026-08 history for one model on one box, and the transferable content is the reasoning.

**The spine.** Decode at the real 128k scored context went **1.01 → 60.17 tok/s** across ~96 PRs, against llama.cpp at **18.44** on the same box and the same weights — 18× behind to 3.26× ahead. Seventeen recorded frontier advances, of which the largest single step is 4.5× and the median is ~7%: the 60× is a *product*, not a discovery. Prefill went **40.35 → 99.68** at 32k against llama.cpp's 143.88 (still 1.44× behind).

**Lectures.** 01 the workload/baseline/ladder (why the baseline had to be a pinned third-party fork — upstream llama.cpp asserts `n_expert ≤ 512`); 02 the scoreboard (correctness gate ordered *before* speed, top-1 ≥ 0.95 / mean KL ≤ 0.05 worst-of-seven-depths, tier = `min(Δ/llama_ref, Δ/frontier)`, raise-only frontier, and ~40 merged PRs that each closed one measurement-trust hole); 03 diagnosis across four ceilings (bandwidth / occupancy / launch / comm) with the finding that the collective was ~2% of the token while ~30 unfused kernels per layer × 93 were the cost; 04 launch geometry; 05 fusion + activation quantization; 06 attention at 128k; 07 expert sharding and the Amdahl trap; 08 CUDA-graph residency; 09 batched prefill; 10 silent wrongness.

**Three findings that carry beyond the case study.** (1) The harness measured context **64** while the slot was named `_128` and the docs said 128k — at 64 the engine was 1.8× behind the reference, at 128k it was **18×** behind, so weeks of work were aimed at a context nobody runs. (2) A **3× MoE speedup made TP scaling worse**, 2.44× → ~1.1×, because the replicated attention it left behind became the whole serial term (serial fraction 0.33 → 0.90). (3) A PR measured **169.72 tok/s** prefill with two live defects — a transposed stride pair masked below 13 layers, and an env gate documented default-off that read opt-out — and *"the corrupted walk was faster because it was reading the wrong rows."* The honest number was 98.80. On a memory-bound engine, incorrectness and speed are positively correlated.

Also registered Part 2 Lecture 07 in the site nav, where it was present on disk but missing from `mkdocs.yml`.

---

**2026-06 — New lecture: Part 2 Lecture 07 (the communication layer)**

Added **Part 2 Lecture 07 — "Inside the Communication Layer: NCCL, Custom All-Reduce, and the vLLM Communicator Stack."** How a serving runtime actually moves bytes between GPUs, using vLLM's `distributed/device_communicators/` as the worked example: the three-layer architecture (router → engines → primitives), the small-message latency problem that motivates a custom one-shot/two-shot all-reduce over the NCCL ring, the fused all-reduce+RMSNorm path (FlashInfer), the runtime routing/fallback ladder, and all2all as a forward pointer to Part 3 MoE. Mechanistically explains Lecture 04's "TP=8 decode is comm-bound." Part 2 is now 7 lectures (course total 16 → 17); updated the Part 2 README, course README counts/maps, Lecture 06 footer, and the root roadmap README. Module names pinned to the vLLM 0.22-era lineage and hedged as version-dependent.

---

</details>

## 2026-06 — 新增 FlashInfer（共享 kernel 引擎）+ InferenceX（实时 benchmark 参考）

- **FlashInfer** — 加入 **Part 2 Lecture 05 §2.4**，作为 vLLM / SGLang / TensorRT-LLM / MLC-LLM 共同构建于其上的、JIT 编译的共享 attention + sampling kernel 引擎（paged/ragged attention、免排序 sampling、可定制变体）。定位为“用哪个 attention 后端”这一调节旋钮（`VLLM_ATTENTION_BACKEND=FLASHINFER`，SGLang `--attention-backend flashinfer`）。来源：[arXiv:2501.01005](https://arxiv.org/abs/2501.01005) (MLSys 2025), Apache-2.0, `flashinfer-ai`。
- **SemiAnalysis InferenceX** — 加入课程 **README 时效性小节**，作为推荐的 *实时* 跨栈 benchmark 参考（覆盖 H100→GB300 NVL72 / MI355X 的 tokens/s、perf/$、tokens/MW，以及 vLLM/SGLang/TRT-LLM）。Apache-2.0；实时看板见 inferencex.com。要点：讲义中的数字是教学锚点，看板才是部署时的真相。
- 审阅了 Together AI 的推理优化博客；其概念（FP8/FP4、投机解码、MTP、蒸馏）在本课程中已有带一手来源的覆盖，因此未引入该博客的数字（依据一手来源规则）。
- 审阅了三份配套学习资源，并加入 README 的 **Companion resources** 区块（按其性质标注；未引入其中内容 —— 它们属于同领域或更宽泛的教学资源，而非一手来源）：**mlabonne/llm-course**（免费，覆盖 LLM 全生命周期；SmoothQuant 等本课程已覆盖）、Maven 上的 **Vizuara Inference Workshop**（付费直播小班，同领域），以及 **BentoML Inference Optimization handbook**（免费厂商指南）。依据一手来源规则，仅提供指引。

---

## 2026-06 — 新增 FP8 block-scaling × tensor-parallel 对齐约束

新增了一个生产环境陷阱：**block-scaled FP8 权重要求每个 tensor-parallel 分片维度都是整数个量化块** —— 因此若某个 TP 规模会使 `dim / TP` 无法被块大小整除，加载就会失败（Qwen 2.5 72B 的 FFN `29568 = 128 × 231` 在 TP=2/4/8 下都不是 block-128 对齐的）。新增的 **Part 2 Lecture 03 §5.4** 解释了该机制与修复顺序（重新对齐 TP → pad → 粗化 scaling）；**Part 2 Lecture 04 §4.3** 将其交叉引用为一条可覆盖基于成本的 TP 选择的硬约束。这是持久的架构约束（而非 benchmark 数字），由一次 8×H200 Qwen 2.5 72B 优化分析中发现，并与 vLLM/TRT-LLM 的 block-FP8 行为一致。

---

## 2026-06 — 正确性修正：Qwen 2.5 72B 维度

修正了 **Part 2 Lecture 01** 与 **Part 2 README** 中 Qwen 2.5 72B 的架构数字，以及 **Part 3 Lecture 01** 中一处零散引用。早先的草稿将 Qwen 2.5 72B 写成 **12288 hidden / 49152 FFN** —— 这是一个常见的二手来源误引（12288 是 GPT-3 的宽度）。官方 `config.json` 为 **8192 hidden / 29568 FFN**，这使得 Qwen 2.5 72B 在 *维度上几乎等同于* Llama 3.3 70B（同样是 8192 hidden、同样是 GQA 64/8，FFN 宽约 3%）；72B 与 70B 的差距主要来自这宽出约 3% 的 FFN（≈1.8B 参数），而更大的 152K 词表另加约 0.4B。Lecture 01 §2.1/§4.2 现在以正确数字开头，并把“从已发布的 config 推导、用参数量做 sanity check”这一教训保留为提醒性注记，而非把错误数字当作事实呈现。同时修正了 Part 3 Lecture 01 中 DeepSeek V3 的 dense-FFN 宽度（18432，而非 12288）。该问题是在构建 [LLM Inference Visualizer](https://github.com/ai-hpc/llm-inference-viz) 时发现的，它渲染的是正确形状。

---

## 2026-06 — 全版本刷新（CES 2026 之后、DeepSeek V4 之后）

一次覆盖每一讲的重大更新。本课程最初是依据 2026 年 1 月的世界图景起草的；此后已发布多个定义生态的版本。


<details>
<summary>English original</summary>

**2026-06 — Add FlashInfer (shared kernel engine) + InferenceX (live benchmark reference)**

- **FlashInfer** — added to **Part 2 Lecture 05 §2.4** as the shared, JIT-compiled attention + sampling kernel engine that vLLM / SGLang / TensorRT-LLM / MLC-LLM build on (paged/ragged attention, sorting-free sampling, customizable variants). Framed as the "which attention backend" knob (`VLLM_ATTENTION_BACKEND=FLASHINFER`, SGLang `--attention-backend flashinfer`). Source: [arXiv:2501.01005](https://arxiv.org/abs/2501.01005) (MLSys 2025), Apache-2.0, `flashinfer-ai`.
- **SemiAnalysis InferenceX** — added to the course **README currency section** as the recommended *live* cross-stack benchmark reference (tokens/s, perf/$, tokens/MW across H100→GB300 NVL72 / MI355X, vLLM/SGLang/TRT-LLM). Apache-2.0; live dashboard at inferencex.com. The point: lecture numbers are teaching anchors, the dashboard is truth-at-deployment.
- Reviewed the Together AI inference-optimization blog; its concepts (FP8/FP4, speculative decoding, MTP, distillation) are already covered with primary sources, so no blog numbers were imported (per the primary-source rule).
- Reviewed three companion learning resources and added them to the README **Companion resources** block (labeled by nature; no content imported — they are same-domain or broader teaching resources, not primary sources): **mlabonne/llm-course** (free, broad LLM lifecycle; SmoothQuant etc. already covered here), **Vizuara Inference Workshop** on Maven (paid live cohort, same domain), and the **BentoML Inference Optimization handbook** (free vendor guide). Pointers only, per the primary-source rule.

---

**2026-06 — Add the FP8 block-scaling × tensor-parallel alignment constraint**

Added the production footgun where **block-scaled FP8 weights require each tensor-parallel shard dimension to be a whole number of quantization blocks** — so a TP size that leaves `dim / TP` not divisible by the block size fails to load (Qwen 2.5 72B's FFN `29568 = 128 × 231` is not block-128-aligned under TP=2/4/8). New **Part 2 Lecture 03 §5.4** explains the mechanism and the fix-order (re-align TP → pad → coarsen scaling); **Part 2 Lecture 04 §4.3** cross-references it as a hard constraint that can override the cost-based TP choice. This is a durable architectural constraint (not a benchmark figure), surfaced by an 8×H200 Qwen 2.5 72B optimization analysis and consistent with vLLM/TRT-LLM block-FP8 behaviour.

---

**2026-06 — Correctness fix: Qwen 2.5 72B dimensions**

Corrected the Qwen 2.5 72B architecture numbers in **Part 2 Lecture 01** and the **Part 2 README**, plus a stray reference in **Part 3 Lecture 01**. Earlier drafts quoted Qwen 2.5 72B as **12288 hidden / 49152 FFN** — a common secondary-source misquote (12288 is GPT-3's width). The official `config.json` is **8192 hidden / 29568 FFN**, which makes Qwen 2.5 72B *dimensionally near-identical* to Llama 3.3 70B (same 8192 hidden, same GQA 64/8, ~3% wider FFN); the 72B-vs-70B gap is mostly that ~3% wider FFN (≈1.8B params), with the larger 152K vocab adding ≈0.4B. Lecture 01 §2.1/§4.2 now lead with the correct figures and keep the "derive from the published config, sanity-check against the parameter count" lesson as a cautionary note instead of presenting the wrong numbers as fact. Also fixed DeepSeek V3's dense-FFN width in Part 3 Lecture 01 (18432, not 12288). Surfaced while building the [LLM Inference Visualizer](https://github.com/ai-hpc/llm-inference-viz), which renders the correct shapes.

---

**2026-06 — Full version refresh (post-CES 2026, post-DeepSeek V4)**

Major update spanning every lecture. The course was originally drafted against a January 2026 view of the world; multiple ecosystem-defining releases have shipped since.

</details>

### 生态中的变化

- **DeepSeek V4** 于 2026-04-24 发布：V4-Pro（1.6T 总量 / 49B 激活）和 V4-Flash（284B 总量 / 13B 激活），两者均为 1M token 上下文窗口。在 MLA 之上新增混合 CSA+HCA attention，保留 MTP。旧版 `deepseek-chat` / `deepseek-reasoner` 端点于 2026-07-24 停用。
- **Qwen3-235B-A22B-Instruct-2507**（2025 年 7 月后训练更新）是 Qwen3-MoE 旗舰的当前权威版本：相同的 235B/22B 架构，256K token 上下文。更新的 Qwen3.5 397B-A17B（2026-02）和 Qwen3.7-Max（2026-05）已发布，但尚未成为生态默认。
- **Llama 4**（2025 年 4 月）：Scout 17B 激活/16 专家，Maverick 17B 激活/128 专家。截至写作时，Llama 4 Behemoth 仍未发布。
- **Blackwell Ultra（B300、GB300 NVL72）** 于 2026 年 1 月大规模出货：288 GB HBM3e，每芯片 15 PFLOPS 稠密 FP4，GB300 NVL72 是 GB200 NVL72 性能的 1.5×。
- **NVIDIA Vera Rubin** 在 CES 2026 进入全面量产；2026 H2 可用。Rubin CPX 是专为超大上下文推理设计的新 GPU 类别。
- **FlashAttention 4** 于 2026 年 3 月发布：Blackwell 优化的 attention kernel，在 B200 上 BF16 下约 1605 TFLOPs/s（约 2.7× Triton，1.3× cuDNN 9.13）。在 FMA 上使用多项式逼近 exp()，重缩放操作减少约 10×。
- **CUDA Toolkit 13.3**（2026 年 5 月）取代 12.x 成为当前稳定版。从 CUDA 13.0 起，Blackwell 成为一等目标。
- **Transformer Engine 2.x**（截至 TE 2.x 主线）：通过 MX microscaling 原生支持 FP4，与 cuDNN 9 / FA4 路径集成。

### 本次刷新后固定的版本

**模型（Part 2 的锚定对 — 稠密）：**

| 模型 | 发布 | 为何固定 |
|-------|---------|------------|
| Llama 3.3 70B (Instruct) | 2024-12 | 西方权威稠密 70B 级；旗舰稠密工作已转向 MoE，但此模型仍是教学参考 |
| Qwen 2.5 72B (Instruct) | 2024-09 | 中国权威稠密 72B，FFN 略宽，QKV 偏置 |

**模型（Part 3 的锚定对 — MoE（混合专家模型））：**

| 模型 | 发布 | 为何固定 |
|-------|---------|------------|
| DeepSeek V3.1 | 2025-08 | 2025 年的权威 MoE，用作**教学锚点** — MLA、MTP、256+1 专家 |
| Qwen3-235B-A22B-Instruct-2507 | 2025-07 更新 | 当前标准 Qwen3-MoE — 235B/22B，256K 上下文 |

注意：DeepSeek V4（2026-04）现在是实际的生产前沿，但 V3.1 的架构仍是最清晰的教学锚点 — MLA、MTP 以及 EP 推理服务 recipe 可直接沿用到 V4。每个 Part 3 课程在数学或 recipe 发生变化的地方交叉引用 V4。

**Runtime（截至 2026-06 当前）：**

| Runtime | 版本 | 备注 |
|---------|---------|-------|
| vLLM | **0.22.0**（2026-05-29） | V1 engine 在 v0.7.0 发布 alpha，自 v0.8.x 起默认；当前发布节奏约每 2 周一次 |
| SGLang | **0.5.12.post1**（2026-05-26） | RadixAttention，完整 DeepSeek + Qwen3-MoE EP 支持，生产级 disaggregation |
| TensorRT-LLM | **1.3.0rc16**（2026-05-26） | 现处于 v1.x 主版本；FP4 路径在 Blackwell 上成熟 |
| llama.cpp | build **b9444**（2026-05-31） | 滚动发布；用于边缘的 GGUF + IQ-quants 参考 |
| MLX | **0.31.2**（2026-04-22） | Apple Silicon 参考 |
| DeepEP | **v1.2.1**（2025-09-16） | DeepSeek EP 通信库 |

**框架 / runtime 栈：**

| 组件 | 版本 | 备注 |
|-----------|---------|-------|
| CUDA Toolkit | **13.3**（2026-05） | Blackwell 成熟；12.8 是引入版本 |
| Transformer Engine | **2.15**（2026-05） | TE 2.x；通过 MX microscaling 支持 FP4 |
| FlashAttention | **4.0.0.beta15**（2026-05-27） | FA4 是 Blackwell 优化路径 |
| cuDNN | 9.x | Hopper + Blackwell attention 路径 |
| NCCL | **2.30.4-1**（2026-04） | 全规约 / all-to-all 原语 |

**硬件：**

| 硬件 | 备注 |
|----------|-------|
| NVIDIA H100（80 GB HBM3） | Hopper 基线；仍在广泛部署 |
| NVIDIA H200（141 GB HBM3e） | 用于 70B 级稠密模型的 Hopper 主力 |
| NVIDIA B200（192 GB HBM3e，8 TB/s，9 PFLOPs FP4） | Blackwell 基线 |
| NVIDIA B300 / Blackwell Ultra（288 GB HBM3e，15 PFLOPs FP4） | 2026-01 出货；当前 MoE 级单芯片 |
| NVIDIA GB200 NVL72 | 72-GPU NVLink 域，上一代 MoE 目标 |
| NVIDIA GB300 NVL72 | 1.5× GB200 NVL72；当前 2026 MoE 生产目标 |
| NVIDIA Vera Rubin（CPU+GPU 超级芯片） | 在 CES 2026 量产；2026 H2 广泛可用 — *未来关注* |
| NVIDIA Jetson Thor (AGX) | 边缘交叉参考 |

### 价格参考（云 spot，2026-06；在你自己的 bench 中复现）

| GPU 类别 | $/GPU-hour（典型） |
|-----------|----------------------|
| H100 SXM | ~$2.50 |
| H200 SXM | ~$3.50 |
| B200 SXM | ~$5.50 |
| B300 SXM | ~$6.80 (dedicated) – ~$2.45（spot） – $12-18（托管 DGX） |

参考：DeepSeek V4 API 定价 — V4-Flash $0.14 input / $0.28 输出每 MTok；V4-Pro $1.74 input / $3.48 输出每 MTok。


<details>
<summary>English original</summary>

**What changed in the ecosystem**

- **DeepSeek V4** released 2026-04-24: V4-Pro (1.6T total / 49B active) and V4-Flash (284B total / 13B active), both with 1M-token context window. Adds new hybrid CSA+HCA attention on top of MLA, retains MTP. Legacy `deepseek-chat` / `deepseek-reasoner` endpoints discontinued 2026-07-24.
- **Qwen3-235B-A22B-Instruct-2507** (July 2025 post-training update) is the current canonical of the Qwen3-MoE flagship: same 235B/22B architecture, 256K-token context. Newer Qwen3.5 397B-A17B (2026-02) and Qwen3.7-Max (2026-05) shipped but not yet ecosystem-default.
- **Llama 4** (April 2025): Scout 17B-active/16 experts, Maverick 17B-active/128 experts. Llama 4 Behemoth still not released as of writing.
- **Blackwell Ultra (B300, GB300 NVL72)** shipped in volume January 2026: 288 GB HBM3e, 15 PFLOPS dense FP4 per chip, GB300 NVL72 is 1.5× GB200 NVL72 performance.
- **NVIDIA Vera Rubin** entered full production at CES 2026; available H2 2026. Rubin CPX is a new GPU class specifically for massive-context inference.
- **FlashAttention 4** released March 2026: Blackwell-optimized attention kernel, ~1605 TFLOPs/s on B200 at BF16 (~2.7× Triton, 1.3× cuDNN 9.13). Polynomial-approximation exp() on FMA, ~10× fewer rescaling ops.
- **CUDA Toolkit 13.3** (May 2026) replaces 12.x as the current stable. CUDA 13.0 onward has Blackwell as a first-class target.
- **Transformer Engine 2.x** (mainline as of TE 2.x): FP4 native via MX microscaling, integrated with cuDNN 9 / FA4 paths.

**Pinned versions after this refresh**

**Models (anchor pair for Part 2 — dense):**

| Model | Release | Why pinned |
|-------|---------|------------|
| Llama 3.3 70B (Instruct) | 2024-12 | Canonical Western dense 70B-class; flagship dense work moved to MoE but this remains the teaching reference |
| Qwen 2.5 72B (Instruct) | 2024-09 | Canonical Chinese dense 72B with slightly wider FFN, QKV bias |

**Models (anchor pair for Part 3 — MoE):**

| Model | Release | Why pinned |
|-------|---------|------------|
| DeepSeek V3.1 | 2025-08 | The canonical 2025 MoE used as the **teaching anchor** — MLA, MTP, 256+1 experts |
| Qwen3-235B-A22B-Instruct-2507 | 2025-07 update | The current standard Qwen3-MoE — 235B/22B, 256K context |

Note: DeepSeek V4 (2026-04) is now the actual production frontier, but V3.1's architecture is still the cleanest teaching anchor — MLA, MTP, and the EP serving recipe carry over to V4 directly. Each Part 3 lecture cross-references V4 where the math or recipe shifts.

**Runtimes (current as of 2026-06):**

| Runtime | Version | Notes |
|---------|---------|-------|
| vLLM | **0.22.0** (2026-05-29) | V1 engine shipped alpha in v0.7.0, default since v0.8.x; current release cadence is roughly every 2 weeks |
| SGLang | **0.5.12.post1** (2026-05-26) | RadixAttention, full DeepSeek + Qwen3-MoE EP support, production disaggregation |
| TensorRT-LLM | **1.3.0rc16** (2026-05-26) | Now at the v1.x major; FP4 path mature on Blackwell |
| llama.cpp | build **b9444** (2026-05-31) | Rolling release; GGUF + IQ-quants reference for edge |
| MLX | **0.31.2** (2026-04-22) | Apple Silicon reference |
| DeepEP | **v1.2.1** (2025-09-16) | DeepSeek EP communication library |

**Framework / runtime stack:**

| Component | Version | Notes |
|-----------|---------|-------|
| CUDA Toolkit | **13.3** (2026-05) | Blackwell-mature; 12.8 was the introduction |
| Transformer Engine | **2.15** (2026-05) | TE 2.x; FP4 via MX microscaling |
| FlashAttention | **4.0.0.beta15** (2026-05-27) | FA4 is the Blackwell-optimized path |
| cuDNN | 9.x | Hopper + Blackwell attention paths |
| NCCL | **2.30.4-1** (2026-04) | All-reduce / all-to-all primitives |

**Hardware:**

| Hardware | Notes |
|----------|-------|
| NVIDIA H100 (80 GB HBM3) | Hopper baseline; still widely deployed |
| NVIDIA H200 (141 GB HBM3e) | Hopper workhorse for 70B-class dense |
| NVIDIA B200 (192 GB HBM3e, 8 TB/s, 9 PFLOPs FP4) | Blackwell baseline |
| NVIDIA B300 / Blackwell Ultra (288 GB HBM3e, 15 PFLOPs FP4) | Shipped 2026-01; the current MoE-class single chip |
| NVIDIA GB200 NVL72 | 72-GPU NVLink domain, the previous-gen MoE target |
| NVIDIA GB300 NVL72 | 1.5× GB200 NVL72; current 2026 MoE production target |
| NVIDIA Vera Rubin (CPU+GPU superchip) | In production at CES 2026; broad availability H2 2026 — *future-watch* |
| NVIDIA Jetson Thor (AGX) | Edge cross-reference |

**Pricing reference (cloud spot, 2026-06; replicate in your own bench)**

| GPU class | $/GPU-hour (typical) |
|-----------|----------------------|
| H100 SXM | ~$2.50 |
| H200 SXM | ~$3.50 |
| B200 SXM | ~$5.50 |
| B300 SXM | ~$6.80 (dedicated) – ~$2.45 (spot) – $12-18 (managed DGX) |

Reference: DeepSeek V4 API pricing — V4-Flash $0.14 input / $0.28 output per MTok; V4-Pro $1.74 input / $3.48 output per MTok.

</details>

### 本次刷新时固定的论文与一手资料

* DeepSeek V3 技术报告 — [arXiv:2412.19437](https://arxiv.org/abs/2412.19437)
* DeepSeek V4 预览发布 — [DeepSeek news 2026-04-24](https://api-docs.deepseek.com/news/news260424)
* Qwen 2.5 技术报告 — [arXiv:2412.15115](https://arxiv.org/abs/2412.15115)
* Qwen3 技术报告 — [Qwen3 发布说明](https://qwenlm.github.io/blog/qwen3/)
* Llama 3.3 模型卡 — [Hugging Face](https://huggingface.co/meta-llama/Llama-3.3-70B-Instruct)
* FlashAttention-4 — 2026 年 3 月发布
* PagedAttention (vLLM) — [arXiv:2309.06180](https://arxiv.org/abs/2309.06180)
* SGLang — [arXiv:2312.07104](https://arxiv.org/abs/2312.07104)
* EAGLE-3 — [arXiv:2503.01840](https://arxiv.org/abs/2503.01840)
* AWQ — [arXiv:2306.00978](https://arxiv.org/abs/2306.00978)
* GPTQ — [arXiv:2210.17323](https://arxiv.org/abs/2210.17323)
* QuaRot — [arXiv:2404.00456](https://arxiv.org/abs/2404.00456)
* SpinQuant — [arXiv:2405.16406](https://arxiv.org/abs/2405.16406)
* "The Uniqueness of LLaMA3-70B Series with Per-Channel Quantization" — [arXiv:2408.15301](https://arxiv.org/abs/2408.15301)
* Mooncake（P/D 分离）— [arXiv:2407.00079](https://arxiv.org/abs/2407.00079)
* DistServe — [arXiv:2401.09670](https://arxiv.org/abs/2401.09670)
* FlashInfer（attention/sampling kernel 引擎）— [arXiv:2501.01005](https://arxiv.org/abs/2501.01005) (MLSys 2025) · [github.com/flashinfer-ai/flashinfer](https://github.com/flashinfer-ai/flashinfer)
* SemiAnalysis InferenceX（实时跨栈 benchmark，Apache-2.0）— [github.com/SemiAnalysisAI/InferenceX](https://github.com/SemiAnalysisAI/InferenceX) · [inferencex.com](https://inferencex.com/)

---

## 2026-06 — 首次发布（已被上文取代）

课程上线。第 1、2、3 部分完成。固定版本（现已被取代）：vLLM 0.7.x、SGLang 0.4.x、TRT-LLM 0.18.x、CUDA 12.6+、TE 1.10+、FA3。

---

## 计划的刷新检查点

* **+3 个月（2026-09）** — 复审项：DeepSeek V4 生态成熟度、Llama 4 Behemoth 发布、Vera Rubin 全面上市、TRT-LLM 1.x 稳定化、vLLM 更新节奏。
* **+6 个月（2026-12）** — 全面复审，包括 dense-pair（第 2 部分）针对当前 vLLM/SGLang 的重新 benchmark，以及第 3 部分第 02 讲中 Vera Rubin 一节在部署之后从「未来观察」升级为实际硬件。
* **按需** — DeepSeek V5、Qwen 4、Llama 5 或任何 FP3 / FP6 硬件路径落地时。

---

## 如何刷新

更新一讲时：

1. 更新该讲正文。
2. 把其末尾的 `## Current as of YYYY-MM` 行更新为新日期。
3. 在本日志中新增一条记录，写明改了什么以及为什么改。
4. 把被取代的 benchmark 表格移入相关部分内带日期的 `archive/` 子文件，并附一行回指。
5. 保持一手资料链接有效 — 把失效链接替换为最接近的权威替代（模型卡 → HF 镜像；论文 → arXiv 稳定 URL；GitHub → release tag）。

内容漂移比缺漏主题更快摧毁技术可信度。刷新日志就是课程仍在维护的证明。


<details>
<summary>English original</summary>

**Papers and primary sources pinned at this refresh**

* DeepSeek V3 technical report — [arXiv:2412.19437](https://arxiv.org/abs/2412.19437)
* DeepSeek V4 preview release — [DeepSeek news 2026-04-24](https://api-docs.deepseek.com/news/news260424)
* Qwen 2.5 technical report — [arXiv:2412.15115](https://arxiv.org/abs/2412.15115)
* Qwen3 technical report — [Qwen3 release notes](https://qwenlm.github.io/blog/qwen3/)
* Llama 3.3 model card — [Hugging Face](https://huggingface.co/meta-llama/Llama-3.3-70B-Instruct)
* FlashAttention-4 — published March 2026
* PagedAttention (vLLM) — [arXiv:2309.06180](https://arxiv.org/abs/2309.06180)
* SGLang — [arXiv:2312.07104](https://arxiv.org/abs/2312.07104)
* EAGLE-3 — [arXiv:2503.01840](https://arxiv.org/abs/2503.01840)
* AWQ — [arXiv:2306.00978](https://arxiv.org/abs/2306.00978)
* GPTQ — [arXiv:2210.17323](https://arxiv.org/abs/2210.17323)
* QuaRot — [arXiv:2404.00456](https://arxiv.org/abs/2404.00456)
* SpinQuant — [arXiv:2405.16406](https://arxiv.org/abs/2405.16406)
* "The Uniqueness of LLaMA3-70B Series with Per-Channel Quantization" — [arXiv:2408.15301](https://arxiv.org/abs/2408.15301)
* Mooncake (P/D disaggregation) — [arXiv:2407.00079](https://arxiv.org/abs/2407.00079)
* DistServe — [arXiv:2401.09670](https://arxiv.org/abs/2401.09670)
* FlashInfer (attention/sampling kernel engine) — [arXiv:2501.01005](https://arxiv.org/abs/2501.01005) (MLSys 2025) · [github.com/flashinfer-ai/flashinfer](https://github.com/flashinfer-ai/flashinfer)
* SemiAnalysis InferenceX (live cross-stack benchmark, Apache-2.0) — [github.com/SemiAnalysisAI/InferenceX](https://github.com/SemiAnalysisAI/InferenceX) · [inferencex.com](https://inferencex.com/)

---

**2026-06 — Initial publication (superseded above)**

Course launched. Parts 1, 2, 3 complete. Pinned versions (now superseded): vLLM 0.7.x, SGLang 0.4.x, TRT-LLM 0.18.x, CUDA 12.6+, TE 1.10+, FA3.

---

**Planned refresh checkpoints**

* **+3 months (2026-09)** — review for: DeepSeek V4 ecosystem maturity, Llama 4 Behemoth release, Vera Rubin general availability, TRT-LLM 1.x stabilization, vLLM cadence updates.
* **+6 months (2026-12)** — full review including dense-pair (Part 2) re-bench against current vLLM/SGLang, and the Vera Rubin section in Part 3 Lecture 02 upgraded from "future-watch" to actual hardware once it's deployed.
* **As-needed** — landing of DeepSeek V5, Qwen 4, Llama 5, or any FP3 / FP6 hardware path.

---

**How to refresh**

When updating a lecture:

1. Update the lecture body.
2. Update its trailing `## Current as of YYYY-MM` line to the new date.
3. Add an entry to this log naming what changed and why.
4. Move superseded benchmark tables into a dated `archive/` subfile inside the relevant part, with a one-line back-pointer.
5. Keep primary-source links live — replace dead links with the closest authoritative survivor (model card → HF mirror; paper → arXiv stable URL; GitHub → release tag).

Drift kills technical credibility faster than missing topics. The refresh log is the proof the course is being maintained.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/REFRESH-LOG.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/REFRESH-LOG.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
