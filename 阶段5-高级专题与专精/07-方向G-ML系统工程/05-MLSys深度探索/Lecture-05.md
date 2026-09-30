---
title: 第 05 讲 - 作为系统产物的 2026 前沿：Qwen3、Nemotron Ultra、MiMo、DeepSeek 与 MoE
description: 第 05 讲 - 作为系统产物的 2026 前沿：Qwen3、Nemotron Ultra、MiMo、DeepSeek 与 MoE
---

# 第 05 讲 - 作为系统产物的 2026 前沿：Qwen3、Nemotron Ultra、MiMo、DeepSeek 与 MoE

**合集：** [MLSys Deep Dives](README.md) | **上一讲：** [← 第 04 讲](Lecture-04.md) | **下一讲：** [第 06 讲](Lecture-06.md)

---

模型卡是一份伪装的系统规格书。到 2026 年，标题上的参数量几乎说明不了成本 —— 一个「235B」模型每 token 的算力开销可能只相当于 22B，却要在 VRAM 中驻留 235B 的权重，并且每一层都要求一次 all-to-all。本讲要建立的技能是**以 MLSys 工程师的方式解读前沿模型**：不是「它有多聪明」，而是「它的成本是什么形状，它会对我的 GPU 做什么」。

本讲把 2026 前沿中的四个模型当作系统产物来读 —— **Qwen3、Llama Nemotron Ultra 253B、Xiaomi MiMo、DeepSeek V3/R1** —— 借助如今在约 30B 以上占据主导的那一种结构：**Mixture of Experts**（混合专家模型），以及随它一同出现的效率技巧（MLA、MTP）。

---

## 学习目标

本讲结束时，你应当能够：

1. 解释 **MoE**：总参数与激活参数之别，它为何把容量与算力解耦，以及它把什么成本转嫁到**内存与互连**上。
2. 把 **Qwen3**（dense + MoE，统一思考模式）、**Nemotron Ultra 253B**（NAS 压缩的 dense）、**MiMo**（MTP、小型 reasoning）、**DeepSeek V3/R1**（MoE + MLA + MTP + FP8）当作系统规格来读。
3. 识别**2026 年的标准效率栈**，以及每个部件分别由哪个模型作为范例。
4. 从模型卡中提取模型的**系统指纹**：激活参数、KV 行为、routing、precision、draft 机制、context。
5. 从该指纹预测模型的**成本形状**（prefill（首字前的整段计算）vs decode（逐 token 生成阶段）、内存受限还是通信受限、粗略的 `$/Mtok`）。

---


<details>
<summary>English original</summary>

**Lecture 05 - The 2026 Frontier as Systems Artifacts: Qwen3, Nemotron Ultra, MiMo, DeepSeek, and MoE**

**Collection:** [MLSys Deep Dives](README.md) | **Previous:** [← Lecture 04](Lecture-04.md) | **Next:** [Lecture 06](Lecture-06.md)

---

A model card is a systems spec in disguise. By 2026, the headline parameter count tells you almost nothing about cost — a "235B" model might cost you 22B-worth of compute per token, hold 235B-worth of weights in VRAM, and demand an all-to-all every layer. The skill this lecture builds is **reading a frontier model the way an MLSys engineer reads it**: not "how smart is it" but "what shape is its cost, and what will it do to my GPUs."

We read four of the 2026 frontier as systems artifacts — **Qwen3, Llama Nemotron Ultra 253B, Xiaomi MiMo, DeepSeek V3/R1** — through the one structure that now dominates above ~30B: **Mixture of Experts**, plus the efficiency tricks (MLA, MTP) that travel with it.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Explain **MoE**: total vs active params, why it decouples capacity from compute, and what cost it shifts onto **memory and interconnect**.
2. Read **Qwen3** (dense + MoE, unified thinking mode), **Nemotron Ultra 253B** (NAS-compressed dense), **MiMo** (MTP, small reasoning), and **DeepSeek V3/R1** (MoE + MLA + MTP + FP8) as systems specs.
3. Identify the **canonical 2026 efficiency stack** and which model exemplifies each piece.
4. Extract a model's **systems fingerprint** from its card: active params, KV behavior, routing, precision, draft mechanism, context.
5. Predict a model's **cost shape** (prefill vs decode, memory- vs comm-bound, rough `$/Mtok`) from that fingerprint.

---

</details>

## 1. MoE：吞掉前沿的结构

稠密模型对 **每个** token 都运行 **全部** 参数。而 **Mixture-of-Experts** 模型每层有多个 “expert” FFN，以及一个 **router**，把每个 token 只送给其中少数几个。由此得到 2026 年模型推理服务中最重要的那个数字：

```text
   DENSE:  token → ALL weights run         compute/token ∝ total params
   MoE:    token → router picks K of N experts → only K run
           ┌─────────────────────────────────────────────────────────┐
           │ TOTAL params  = capacity (must live in VRAM)              │
           │ ACTIVE params = FLOPs per token  (≪ total)               │
           └─────────────────────────────────────────────────────────┘
   e.g.  Qwen3-235B-A22B  → 235B total / 22B active  (~10:1)
         DeepSeek-V3      → 671B total / 37B active  (~18:1)
```

这就是 MoE 在 ~30B 以上占据主导的原因（据报道 **占 2025 年开源发布量的 60% 以上**，且智能排行榜顶端几乎每个模型都是 MoE）：你以一个小模型的 *每 token 算力* 拿到了一个大模型的 *质量*。

但没有什么是免费的——MoE **把成本从 FLOPs 转移到了内存和互连**：

```text
   the bill MoE hands the systems engineer:
   • MEMORY:      all experts must be resident → large aggregate VRAM (you hold 235B/671B of weights)
   • INTERCONNECT: experts are scattered across GPUs (expert parallelism) → an ALL-TO-ALL
                   token shuffle (dispatch + combine) on EVERY MoE layer, every forward pass
   • LOAD BALANCE: hot experts create stragglers; routing must stay balanced
```

所以稠密模型是算力受限的地方，MoE 模型是**内存受限与通信受限**。稠密模型的扩展买单在 FLOPs；MoE 的扩展买单在 VRAM 和 all-to-all 带宽（NVLink/InfiniBand）。超过 ~30B 之后，这笔交易就划算了——这正是 2026 年前沿模型是稀疏的、也正是 **专家并行** 与快速 all-to-all 成为核心推理服务技能的原因。

有一个细节决定了你*如何*服务 MoE，值得精确把握——**专家内存访存量取决于批大小**：

```text
   batch 1:    each token touches only K experts → ~ACTIVE-param bytes stream per step
               (DeepSeek-V3: ~37B of 671B) → the batch-1 bandwidth ceiling (Lec 1, §6)
               looks like a 37B model's, not a 671B model's
   big batch:  different tokens route to DIFFERENT experts → collectively most of the
               N experts are touched every layer → traffic per step approaches TOTAL
               params — but it is now shared across the whole batch → per-token cost collapses
   the middle: enough tokens to touch most experts, too few to amortize them —
               you stream most of 671B for a handful of tokens. the worst regime.
```

所以 MoE 想被服务在两个极端上：**极小批**（廉价的单条流——延迟关键场景）或 **深批**（专家被摊薄——吞吐场景），而尴尬的中间地带正是朴素部署失血的地方。这就是 MoE 推理服务如此死磕批深度与专家并行的原因，也是 MoE 在小批边缘部署上很少说得通的原因（第 7 讲的小型稠密/混合模型占据这一区间）。当你对 MoE 跑第 1 讲的带宽上限检查时，要用 **批大小为 1 时的活跃字节数** 和 **接近饱和时的总字节数** ——只引用其中任何一个，正是厂商宣传材料误导你的方式。

---

## 2. Qwen3——稠密与 MoE，带一个思考开关

**Qwen3**（Alibaba，2025 年 4 月）以 *系列* 形式发布：稠密（0.6B → 32B）与 MoE——**Qwen3-30B-A3B**（30B/3B 激活）以及旗舰 **Qwen3-235B-A22B**（235B/22B 激活，**128 个专家、激活 8 个**），在约 **36T tokens** 上预训练，128K 上下文。

两个与系统相关的事实：

* **MoE 旗舰模型以每 token ~22B 激活算力做推理**，但必须在专家并行部署中容纳 235B 的权重。所以它的 *延迟/算力* 成本大致是一个 22B 稠密模型的；它的 *内存/互连* 成本是一个 235B 模型的。这种分裂正是整个 MoE 故事的具象化。
* **统一的思考模式。** *单个* 检查点通过一个 `enable_thinking` 标志或内联的 `/think` / `/no_think` 标签在 **Thinking 与 Non-Thinking** 之间切换，并配有可配置的 **thinking budget**。不存在单独的推理模型。对系统工程师来说，这是一个 *作用在 token 数量上的 runtime 旋钮*：thinking 模式输出多得多的 token（推理 trace），因此用延迟和 `$/request` 换准确率——在请求时、按请求生效。你会以不同方式调度这两种模式，并为之定不同的价。

---


<details>
<summary>English original</summary>

**1. MoE: the structure that ate the frontier**

A dense model runs **every** parameter for **every** token. A **Mixture-of-Experts** model has many "expert" FFNs per layer and a **router** that sends each token to only a few of them. The consequence is the most important number in 2026 model serving:

```text
   DENSE:  token → ALL weights run         compute/token ∝ total params
   MoE:    token → router picks K of N experts → only K run
           ┌─────────────────────────────────────────────────────────┐
           │ TOTAL params  = capacity (must live in VRAM)              │
           │ ACTIVE params = FLOPs per token  (≪ total)               │
           └─────────────────────────────────────────────────────────┘
   e.g.  Qwen3-235B-A22B  → 235B total / 22B active  (~10:1)
         DeepSeek-V3      → 671B total / 37B active  (~18:1)
```

This is why MoE dominates above ~30B (reportedly **>60% of 2025 open releases**, and essentially every model atop the intelligence leaderboards): you get the *quality* of a huge model at the *per-token compute* of a small one.

But nothing is free — MoE **shifts the cost from FLOPs to memory and interconnect**:

```text
   the bill MoE hands the systems engineer:
   • MEMORY:      all experts must be resident → large aggregate VRAM (you hold 235B/671B of weights)
   • INTERCONNECT: experts are scattered across GPUs (expert parallelism) → an ALL-TO-ALL
                   token shuffle (dispatch + combine) on EVERY MoE layer, every forward pass
   • LOAD BALANCE: hot experts create stragglers; routing must stay balanced
```

So an MoE model is **memory- and comm-bound where a dense model is compute-bound**. Dense scaling pays in FLOPs; MoE scaling pays in VRAM and all-to-all bandwidth (NVLink/InfiniBand). Past ~30B, that trade wins — which is why the 2026 frontier is sparse, and why **expert parallelism** and fast all-to-all are core serving skills.

One subtlety decides *how* you serve MoE, and it is worth holding precisely — **expert memory traffic depends on batch size**:

```text
   batch 1:    each token touches only K experts → ~ACTIVE-param bytes stream per step
               (DeepSeek-V3: ~37B of 671B) → the batch-1 bandwidth ceiling (Lec 1, §6)
               looks like a 37B model's, not a 671B model's
   big batch:  different tokens route to DIFFERENT experts → collectively most of the
               N experts are touched every layer → traffic per step approaches TOTAL
               params — but it is now shared across the whole batch → per-token cost collapses
   the middle: enough tokens to touch most experts, too few to amortize them —
               you stream most of 671B for a handful of tokens. the worst regime.
```

So MoE wants to be served at the extremes: **tiny batch** (cheap single stream — the latency-critical case) or **deep batch** (experts amortized — the throughput case), and the awkward middle is where naive deployments bleed money. This is why MoE serving pushes so hard on batch depth and expert parallelism, and why MoE rarely makes sense for small-batch edge deployment (Lecture 7's small dense/hybrid models own that regime). When you run Lecture 1's bandwidth-ceiling check on an MoE, use **active bytes at batch 1** and **total bytes near saturation** — quoting either one alone is how vendor decks mislead you.

---

**2. Qwen3 — dense and MoE, with a thinking switch**

**Qwen3** (Alibaba, Apr 2025) ships as a *family*: dense (0.6B → 32B) and MoE — **Qwen3-30B-A3B** (30B/3B active) and the flagship **Qwen3-235B-A22B** (235B/22B active, **128 experts, 8 activated**), pretrained on ~**36T tokens**, 128K context.

Two systems-relevant facts:

* **The MoE flagship serves at ~22B-active compute per token** but must hold 235B of weights across an expert-parallel deployment. So its *latency/compute* cost is roughly a 22B dense model's; its *memory/interconnect* cost is a 235B model's. That split is the whole MoE story made concrete.
* **Unified thinking mode.** A *single* checkpoint toggles **Thinking vs Non-Thinking** via an `enable_thinking` flag or inline `/think` / `/no_think` tags, with a configurable **thinking budget**. There's no separate reasoning model. For the systems engineer this is a *runtime knob on token count*: thinking mode emits far more tokens (reasoning traces), so it trades latency and `$/request` for accuracy — at request time, per request. You schedule and price the two modes differently.

---

</details>

## 3. Llama Nemotron Ultra 253B —— 用架构搜索塞进 8 张 GPU

**Llama-3.1-Nemotron-Ultra-253B**（NVIDIA，2025 年 4 月）就是 "Nemotron Ultra" 背后的模型，它是一件纯粹的 MLSys 产物：**由 Meta 的 Llama-3.1-405B 经神经架构搜索（NAS）+ 剪枝/蒸馏派生而来**，目标明确 —— **把一个 405B 质量的模型塞进单个 8×H100 节点**。

NAS 产出了一个**非均匀、不规则**的网络（162 层，不重复），所用的每一项改动本身就是一堂协同设计课：

```text
   skip-attention:   some blocks drop attention entirely (or replace it with one linear layer)
   variable FFN:     different expansion ratios per block
   FFN fusion:       where attention was skipped, consecutive FFNs are merged into fewer, wider FFNs
   → 405B → 253B, engineered to land inside an 8×H100 memory/latency envelope
```

它是**稠密**模型（不是 MoE），128K 上下文，通过系统提示词提供**推理开关**（"detailed thinking on/off"），开启时输出 `<think>` trace。FP8 变体进一步压缩内存。系统层面的教训：**架构搜索是一种部署工具** —— 可以把模型 NAS *到给定的硬件预算*，用手工设计的均匀堆叠换取一个能塞进你实际拥有的 GPU 的不规则堆叠。这与 Lecture 4 是同一种协同设计哲学，只不过应用于打包而非 KV cache。

---

## 4. Xiaomi MiMo —— 小巧、能推理，且天生适合做 draft

**MiMo-7B**（Xiaomi，2025 年）就是你听说过的那个 "MiMo"，其推理能力远超 7B 的量级（据称其 RL 变体在数学/代码上超过了 o1-mini）。两个与系统相关的设计选择：

* **预训练中的多 token 预测（MTP）。** MiMo 被训练去预测未来*若干个* token，而不只是下一个。这既加密了训练信号，**也**——这才是关键所在——让模型**内置了用于投机解码的 draft head**（Lecture 6）。该架构自带加速器。
* **小且单 GPU。** 7B 的规模服务成本低，单 GPU 即可容纳，与 235B/671B 的 MoE 处在光谱的两端 —— 这也是它天然适合边缘/端侧（Lecture 7）的原因。此外还有 **MiMo-VL**（视觉语言）和 **MiMo-Audio** 变体。

MiMo 也为本课程的压轴戏做了铺垫：**MiMo-V2.5-Pro** 系列正是 2026 年一项吞吐里程碑的演示对象 —— 在单个 8-GPU 节点上，把一个 **1 万亿参数的 MoE 推过了约 1000 tokens/s**，靠的是叠加 MXFP4 量化 + **DFlash** 投机解码 + **TileRT** megakernel runtime。我们在 Lecture 6 中剖析这一结果；这里只需注意，它的*第一味原料是架构*（对 MTP 友好、MoE），其余则是来自 Lecture 2–3 和 6 的系统栈。

---

## 5. DeepSeek V3 / R1 —— 教科书式的效率栈

**DeepSeek-V3**（2024 年 12 月）是"一次性集齐 2026 年所有效率技巧"最干净的例子，值得当作模板记住：**671B 总参数 / 37B 激活**的 MoE（**256 个路由专家 + 1 个共享专家，激活 8 个路由专家**），14.8T token。它叠加的四项技术直接对应本课程：

```text
   MoE         671B/37B → compute of a 37B, capacity of a 671B            (this lecture, §1)
   MLA         multi-head LATENT attention: cache a low-rank latent        (Lecture 4, §6)
               instead of full per-head K/V → much smaller KV cache
   MTP         multi-token prediction → denser signal + spec-decode draft  (this lecture §4, Lecture 6)
   FP8         trained and largely served in FP8 → memory & throughput     (precision floor)
   + auxiliary-loss-FREE load balancing (bias-based routing) → no aux-loss quality/throughput hit
```

**R1** 是在 V3 基座上构建的推理模型（用 RL 做长思维链）。DeepSeek-V3 能以**约 $0.14/Mtok** 的成本提供前沿质量（Lecture 1），原因正在于这套栈：MoE 削减了计算，MLA 削减了 KV 内存，MTP 加速了 decode（逐 token 生成阶段），FP8 削减了字节数。这就是模型–系统协同设计产出数量级成本结果的范例 —— 整门课的论点浓缩在一张模型卡中。

---


<details>
<summary>English original</summary>

**3. Llama Nemotron Ultra 253B — architecture search to fit 8 GPUs**

**Llama-3.1-Nemotron-Ultra-253B** (NVIDIA, Apr 2025) is the model behind "Nemotron Ultra," and it is a pure MLSys artifact: it was **derived from Meta's Llama-3.1-405B via Neural Architecture Search (NAS) + pruning/distillation**, with one explicit goal — **fit a 405B-quality model onto a single 8×H100 node**.

The NAS produced a **non-uniform, irregular** network (162 layers, non-repeating), using moves that are themselves a lecture in co-design:

```text
   skip-attention:   some blocks drop attention entirely (or replace it with one linear layer)
   variable FFN:     different expansion ratios per block
   FFN fusion:       where attention was skipped, consecutive FFNs are merged into fewer, wider FFNs
   → 405B → 253B, engineered to land inside an 8×H100 memory/latency envelope
```

It is **dense** (not MoE), 128K context, with a **reasoning toggle** via system prompt ("detailed thinking on/off"), emitting `<think>` traces when on. An FP8 variant cuts memory further. The systems lesson: **architecture search is a deployment tool** — you can NAS a model *to a hardware budget*, trading a hand-designed uniform stack for an irregular one that fits the GPUs you actually have. This is the same co-design philosophy as Lecture 4, applied to packing rather than to the KV cache.

---

**4. Xiaomi MiMo — small, reasoning, and built to be drafted**

**MiMo-7B** (Xiaomi, 2025) is the "MiMo" you've heard about, and it punches far above 7B on reasoning (its RL variant reportedly surpasses o1-mini on math/code). Two systems-relevant design choices:

* **Multi-Token Prediction (MTP) in pretraining.** MiMo is trained to predict *several* future tokens, not just the next one. This densifies the training signal **and** — the part that matters here — leaves the model with **built-in draft heads for speculative decoding** (Lecture 6). The architecture ships its own accelerator.
* **Small and single-GPU.** At 7B it is cheap to serve and fits one GPU, the opposite end of the spectrum from the 235B/671B MoEs — and the reason it's a natural fit for edge/on-device (Lecture 7). There are also **MiMo-VL** (vision-language) and **MiMo-Audio** variants.

MiMo also sets up the course's payoff: the **MiMo-V2.5-Pro** line is what a 2026 throughput milestone was demonstrated on — a **1-trillion-parameter MoE pushed past ~1000 tokens/s** on a single 8-GPU node, by stacking MXFP4 quantization + **DFlash** speculative decoding + the **TileRT** megakernel runtime. We dissect that result in Lecture 6; note here that its *first ingredient is the architecture* (MTP-friendly, MoE), and the rest is the systems stack from Lectures 2–3 and 6.

---

**5. DeepSeek V3 / R1 — the canonical efficiency stack**

**DeepSeek-V3** (Dec 2024) is the cleanest single example of "every 2026 efficiency trick at once," and worth memorizing as a template: **671B total / 37B active** MoE (**256 routed + 1 shared expert, 8 routed activated**), 14.8T tokens. Its four stacked techniques map directly onto this course:

```text
   MoE         671B/37B → compute of a 37B, capacity of a 671B            (this lecture, §1)
   MLA         multi-head LATENT attention: cache a low-rank latent        (Lecture 4, §6)
               instead of full per-head K/V → much smaller KV cache
   MTP         multi-token prediction → denser signal + spec-decode draft  (this lecture §4, Lecture 6)
   FP8         trained and largely served in FP8 → memory & throughput     (precision floor)
   + auxiliary-loss-FREE load balancing (bias-based routing) → no aux-loss quality/throughput hit
```

**R1** is the reasoning model built on the V3 base (RL for long chain-of-thought). The reason DeepSeek-V3 could be served at frontier quality for **~$0.14/Mtok** (Lecture 1) is precisely this stack: MoE cut the compute, MLA cut the KV memory, MTP sped decode, FP8 cut the bytes. It is the worked example of model–systems co-design producing an order-of-magnitude cost result — the whole thesis of this course in one model card.

---

</details>

## 6. 阅读模型卡以获取其系统指纹

以下是可复用技能。给定任意 2026 模型卡，提取**六个字段**，就能在不运行它的情况下预测其成本形态：

```text
   ① total / active params  → compute/token (active) AND memory footprint (total)
   ② attention type         → KV-cache behavior:  MHA/GQA = O(L) big · MLA = O(L) small · SSM/hybrid = O(1)
   ③ routing                → dense (no all-to-all) vs MoE (#experts/#active → all-to-all cost)
   ④ dominant precision     → BF16 / FP8 / FP4 / 4-bit → bytes per param, throughput
   ⑤ draft mechanism        → MTP heads / EAGLE / none → decode-speed headroom (Lecture 6)
   ⑥ context length         → KV scaling, prefill cost
   ───────────────────────────────────────────────────────────────────────────────────
   ⇒ COST SHAPE: prefill- vs decode-dominated, memory- vs compute- vs comm-bound, rough $/Mtok class
```

应用于四个模型：

| 模型 | ① 总/激活 | ② attention/KV | ③ 布线 | ④ 精度 | ⑤ draft | ⑥ 上下文 | 成本形态 |
|---|---|---|---|---|---|---|---|
| **Qwen3-235B-A22B** | 235B / 22B | GQA, O(L) | MoE 128/8 | BF16·FP8 | — | 128K | 内存+通信受限（MoE），22B 计算/token |
| **Nemotron Ultra 253B** | 253B 稠密 | skip-attn（NAS） | 稠密 | BF16·FP8 | — | 128K | 算力受限稠密，经 NAS 适配 8×H100 |
| **MiMo-7B** | 7B 稠密 | GQA, O(L) | 稠密 | BF16 | **MTP** | 32K | 小模型，单 GPU，可加速 decode |
| **DeepSeek-V3/R1** | 671B / 37B | **MLA**，O(L) 小 | MoE 256+1/8 | **FP8** | **MTP** | 128K | 完整技术栈：尽管 671B，每 token 仍便宜 |

这张表*就是*这堂课。面试官问“推理服务模型 X 要多少成本”，就是在让你填出这一行并据此推理。

---

## 7. 动手实践 / 测量它

1. **为三个模型建立指纹。** 拉取 Qwen3-235B-A22B、DeepSeek-V3（或 R1）以及一个小型/混合模型（MiMo-7B 或 Falcon-H1）的真实模型卡。为每个模型填写六字段指纹。
2. **预测成本形态。** 对每个模型说明：prefill（首字前的整段计算）主导还是 decode（逐 token 生成阶段）主导？内存受限、算力受限还是通信受限？然后估计主导资源——例如，对 MoE，容纳所有专家的总 VRAM 以及每层的 all-to-all 通信量；对稠密推理模型，FLOPs/token 以及“thinking”模式额外生成的 token。
3. **换算成美元。** 使用第 1 讲的成本模型以及每个模型的合理 tokens/s（取自公开 benchmark——注明日期），对每个模型给出粗略的 `$/Mtok` 区间，并指出哪个杠杆（MoE 计算、MLA 内存、MTP decode、FP8 字节数）贡献最大。

交付物：三份填好的指纹、三种预测的成本形态，以及三个标出主导杠杆的 `$/Mtok` 区间。如果你的预测与已发布的 benchmark 不一致，解释这个差距——该差距通常是你未建模的推理服务栈细节（批处理、专家并行），而找出它才是重点。

---

## 8. 迷你实验

构建一份一页的**“系统指纹”速查表**，供你在余下职业生涯中复用：来自 §6 的六字段检查清单，并为每种架构原型配一个示例——稠密模型（Llama/Nemotron Ultra）、MoE（Qwen3/DeepSeek）、混合模型（Nemotron-H/Falcon-H1），以及小型端侧模型（MiMo）。对每种架构原型，写出预测其成本形态的那一句话。在你看到的*下一个*模型发布上测试它：仅凭模型卡填写指纹，预测成本形态，然后对照首个公开发布的 benchmark 检查。

交付物：速查表 + 在一张新模型卡上做一次“预测 vs 现实”检查。擅长此事——在任何人 benchmark 之前根据模型卡给模型定价——是资深 MLSys 的标志性技能。

---

## 关键要点

- **MoE** 将容量（总参数，驻留在 VRAM）与计算（激活参数，FLOPs/token）解耦——这是 >30B 的主导模式。它**将成本从 FLOPs 转移到内存 + all-to-all 互连**：MoE 是内存/通信受限，而稠密是算力受限。
- **Qwen3** —— 稠密 + MoE（235B/22B，128/8 专家），带有一个**统一的 thinking 开关**，它是 token 数量和成本上的 runtime 旋钮。
- **Nemotron Ultra 253B** —— 从 Llama-405B **经 NAS 压缩**（skip-attention、FFN 融合）以**适配 8×H100**：架构搜索作为部署适配预算的工具。
- **MiMo-7B** —— 小模型、具备推理能力，且**经 MTP 预训练**，因此自带内置的 speculative-decode draft heads；这是 1000-tok/s 里程碑背后的架构（Lec 6）。
- **DeepSeek V3/R1** —— **经典效率栈**：MoE（计算）+ **MLA**（KV 内存）+ **MTP**（decode）+ **FP8**（字节）→ 前沿质量，约 $0.14/Mtok。
- 可复用技能是**六字段系统指纹**（总/激活、attention/KV、布线、精度、draft、上下文）→ 预测的**成本形态**。根据模型卡为模型定价；这就是资深做法。

---


<details>
<summary>English original</summary>

**6. Reading a model card for its systems fingerprint**

Here is the reusable skill. Given any 2026 model card, extract **six fields** and you can predict its cost shape without running it:

```text
   ① total / active params  → compute/token (active) AND memory footprint (total)
   ② attention type         → KV-cache behavior:  MHA/GQA = O(L) big · MLA = O(L) small · SSM/hybrid = O(1)
   ③ routing                → dense (no all-to-all) vs MoE (#experts/#active → all-to-all cost)
   ④ dominant precision     → BF16 / FP8 / FP4 / 4-bit → bytes per param, throughput
   ⑤ draft mechanism        → MTP heads / EAGLE / none → decode-speed headroom (Lecture 6)
   ⑥ context length         → KV scaling, prefill cost
   ───────────────────────────────────────────────────────────────────────────────────
   ⇒ COST SHAPE: prefill- vs decode-dominated, memory- vs compute- vs comm-bound, rough $/Mtok class
```

Applied to the four models:

| Model | ① total/active | ② attention/KV | ③ routing | ④ precision | ⑤ draft | ⑥ ctx | Cost shape |
|---|---|---|---|---|---|---|---|
| **Qwen3-235B-A22B** | 235B / 22B | GQA, O(L) | MoE 128/8 | BF16·FP8 | — | 128K | memory+comm-bound (MoE), 22B compute/tok |
| **Nemotron Ultra 253B** | 253B dense | skip-attn (NAS) | dense | BF16·FP8 | — | 128K | compute-bound dense, NAS-fit to 8×H100 |
| **MiMo-7B** | 7B dense | GQA, O(L) | dense | BF16 | **MTP** | 32K | small, single-GPU, decode-accelerable |
| **DeepSeek-V3/R1** | 671B / 37B | **MLA**, O(L) small | MoE 256+1/8 | **FP8** | **MTP** | 128K | the full stack: cheap/tok despite 671B |

That table *is* the lecture. An interviewer who asks "what would it cost to serve model X" is asking you to fill in this row and reason from it.

---

**7. Hands-on / Measure it**

1. **Fingerprint three models.** Pull the real cards for Qwen3-235B-A22B, DeepSeek-V3 (or R1), and one small/hybrid model (MiMo-7B or Falcon-H1). Fill the six-field fingerprint for each.
2. **Predict the cost shape.** For each, state: prefill- or decode-dominated? memory-, compute-, or comm-bound? Then estimate the dominant resource — e.g. for the MoE, the aggregate VRAM to hold all experts and the all-to-all volume per layer; for the dense reasoning model, the FLOPs/token and the extra tokens "thinking" mode emits.
3. **To dollars.** Using Lecture 1's cost model and a reasonable tokens/s for each (from a public benchmark — date it), put a rough `$/Mtok` band on each, and note which lever (MoE compute, MLA memory, MTP decode, FP8 bytes) is doing the most work.

Deliverable: three filled fingerprints, three predicted cost shapes, and three `$/Mtok` bands with the dominant lever named. If your prediction disagrees with a published benchmark, explain the gap — that gap is usually a serving-stack detail (batching, expert parallelism) you didn't model, and finding it is the point.

---

**8. Mini-lab**

Build a one-page **"systems fingerprint" cheat sheet** you'll reuse for the rest of your career: the six-field checklist from §6, with a worked example for each archetype — a dense model (Llama/Nemotron Ultra), an MoE (Qwen3/DeepSeek), a hybrid (Nemotron-H/Falcon-H1), and a small on-device model (MiMo). For each archetype, write the one sentence that predicts its cost shape. Test it on the *next* model release you see: fill the fingerprint from the card alone, predict the cost shape, then check against the first published benchmark.

Deliverable: the cheat sheet + one "prediction vs reality" check on a fresh model card. Getting good at this — pricing a model from its card before anyone benchmarks it — is a senior MLSys signature skill.

---

**Key takeaways**

- **MoE** decouples capacity (total params, in VRAM) from compute (active params, FLOPs/token) — the dominant >30B pattern. It **shifts cost from FLOPs to memory + all-to-all interconnect**: MoE is memory/comm-bound where dense is compute-bound.
- **Qwen3** — dense + MoE (235B/22B, 128/8 experts), with a **unified thinking switch** that's a runtime knob on token count and cost.
- **Nemotron Ultra 253B** — **NAS-compressed** from Llama-405B (skip-attention, FFN fusion) to **fit 8×H100**: architecture search as a deployment-to-budget tool.
- **MiMo-7B** — small, reasoning, and **MTP-pretrained** so it ships built-in speculative-decode draft heads; the architecture behind a 1000-tok/s milestone (Lec 6).
- **DeepSeek V3/R1** — the **canonical efficiency stack**: MoE (compute) + **MLA** (KV memory) + **MTP** (decode) + **FP8** (bytes) → frontier quality at ~$0.14/Mtok.
- The reusable skill is the **six-field systems fingerprint** (total/active, attention/KV, routing, precision, draft, context) → predicted **cost shape**. Price a model from its card; that's the senior move.

---

</details>

## References

- Qwen3 (Alibaba)：[https://qwenlm.github.io/blog/qwen3/](https://qwenlm.github.io/blog/qwen3/)
- Llama-3.1-Nemotron-Ultra-253B (NVIDIA)：[https://huggingface.co/nvidia/Llama-3_1-Nemotron-Ultra-253B-v1](https://huggingface.co/nvidia/Llama-3_1-Nemotron-Ultra-253B-v1)
- Xiaomi MiMo，arXiv 2505.07608：[https://arxiv.org/abs/2505.07608](https://arxiv.org/abs/2505.07608) · repo [https://github.com/XiaomiMiMo/MiMo](https://github.com/XiaomiMiMo/MiMo)
- DeepSeek-V3，arXiv 2412.19437：[https://arxiv.org/abs/2412.19437](https://arxiv.org/abs/2412.19437)
- Mixture-of-Experts 基础设施与专家并行综述：[https://www.digitalocean.com/community/tutorials/expert-parallelism-in-deep-learning](https://www.digitalocean.com/community/tutorials/expert-parallelism-in-deep-learning)
- *MLSys Deep Dives* —— [Lecture 04](Lecture-04.md)（MLA、hybrid）与 [Lecture 06](Lecture-06.md)（MTP → 投机解码）。

---

## 数据截至

2026-06。版本锁定：Qwen3（2025 年 4 月，235B-A22B / 30B-A3B，128 experts/8）、Nemotron-Ultra-253B（2025 年 4 月，基于 Llama-3.1-405B 做 NAS，8×H100）、MiMo-7B（2025，MTP）、DeepSeek-V3（2024 年 12 月，671B/37B，MLA+MTP+FP8）。MoE 采用率统计（">60% of 2025 releases"）来自行业博客，仅供参考，未经同行评审。后续的 Qwen / Nemotron / MiMo 版本可能取代这些 —— 请重新拉取模型卡。

---

*下一讲：[Lecture 06 — Making decode fast](Lecture-06.md)*


<details>
<summary>English original</summary>

**References**

- Qwen3 (Alibaba): [https://qwenlm.github.io/blog/qwen3/](https://qwenlm.github.io/blog/qwen3/)
- Llama-3.1-Nemotron-Ultra-253B (NVIDIA): [https://huggingface.co/nvidia/Llama-3_1-Nemotron-Ultra-253B-v1](https://huggingface.co/nvidia/Llama-3_1-Nemotron-Ultra-253B-v1)
- Xiaomi MiMo, arXiv 2505.07608: [https://arxiv.org/abs/2505.07608](https://arxiv.org/abs/2505.07608) · repo [https://github.com/XiaomiMiMo/MiMo](https://github.com/XiaomiMiMo/MiMo)
- DeepSeek-V3, arXiv 2412.19437: [https://arxiv.org/abs/2412.19437](https://arxiv.org/abs/2412.19437)
- Mixture-of-Experts infrastructure & expert parallelism overview: [https://www.digitalocean.com/community/tutorials/expert-parallelism-in-deep-learning](https://www.digitalocean.com/community/tutorials/expert-parallelism-in-deep-learning)
- *MLSys Deep Dives* — [Lecture 04](Lecture-04.md) (MLA, hybrids) and [Lecture 06](Lecture-06.md) (MTP → speculative decoding).

---

**Current as of**

2026-06. Pins: Qwen3 (Apr 2025, 235B-A22B / 30B-A3B, 128 experts/8), Nemotron-Ultra-253B (Apr 2025, NAS from Llama-3.1-405B, 8×H100), MiMo-7B (2025, MTP), DeepSeek-V3 (Dec 2024, 671B/37B, MLA+MTP+FP8). MoE-adoption stats (">60% of 2025 releases") are from an industry blog, illustrative not peer-reviewed. Later Qwen / Nemotron / MiMo releases may supersede these — re-pull the cards.

---

*Next: [Lecture 06 — Making decode fast](Lecture-06.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/MLSys Deep Dives/Lecture-05.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/MLSys%20Deep%20Dives/Lecture-05.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
