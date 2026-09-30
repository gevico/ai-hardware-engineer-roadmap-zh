---
title: Part 4 · Lecture 01 — 工作负载、基线与阶梯
description: Part 4 · Lecture 01 — 工作负载、基线与阶梯
---

# Part 4 · Lecture 01 — 工作负载、基线与阶梯

## 概述

在任何优化之前，有三个问题必须被具体回答：**什么是工作负载**、**你在与什么作比较**，以及**你现在处在什么位置**。其中任何一个答错，之后产出的每一个数字都只是装饰。

本讲针对该案例研究回答这三个问题，然后给出完整的实测阶梯 —— 每一次前沿推进，按顺序，正如仓库自身的 pinned 基线文件所记录的那样。

读完你应当能够在写下一行代码之前，读一份模型卡和一份节点规格并推导出推理问题的*形态*：权重需要多少 HBM、一个 token 要付出多少次集合通信、哪个阶段将占主导，以及当主流引擎无法加载你的模型时，可信的基线究竟是什么。

---

## 1. 模型 —— 把 Kimi K3 当作系统产物来读

Part 3 Lecture 01 教过如何阅读 MoE（混合专家模型）模型卡。Kimi K3 是这项练习的困难模式。

| | |
|---|---|
| 参数 | **2.8T** 总量，参考实现中标注类型为 `2.8T.A50B`（约 50B 激活） |
| 层数 | **93** —— **24 个 MLA**（full attention）+ **69 个 KDA**（线性、循环） |
| 隐藏维度 / 词表 | 7168 / 163,840 |
| 上下文 | 1,048,576 |
| MoE | **896 个路由专家**，top-16，**2 个共享专家** · latent MoE 为 **3584**，专家 FFN 3072 |
| Attention（full） | **MLA** —— `q_lora 1536`、`kv_lora 512`，**仅 NoPE**，sigmoid 输出门控 |
| Attention（linear） | **KDA** —— 96 头 × 128，conv kernel 4，满秩门控，`gate_lower_bound −5.0` |
| 激活函数 | `situ` 在所有位置取代 SwiGLU（`β 4.0`，线性 `β 25.0`） |
| 其他 | 跨层残差 attention，`block_size 12` |
| 视觉 | MoonViT-3d —— 27 层，宽 1024，非方形融合 QKV，patch 14 |

该表中有四件事在性质上改变了系统问题，而每一件都是与 Part 2 和 Part 3 中 dense-70B 与 671B-MoE 案例*不同*的一课。

**896 个专家已越过主流上限。** 上游 `llama.cpp` 断言 `n_expert <= LLAMA_MAX_EXPERTS`，而上限是 512。该模型不是加载得慢 —— 而是根本加载不了。§3 讨论这对基线这一概念意味着什么。

**混合 attention 栈有两条成本曲线，而不是一条。** 24 个 MLA 层随上下文增长 KV cache；69 个 KDA 层携带固定大小的循环状态。优化其中一个对另一个毫无作用，而对各层取平均的 profile 会掩盖哪个是哪个。这就是 [MLSys Deep Dives Lecture 04](../../05-MLSys深度探索/Lecture-04.md) 中的 Mamba/混合架构家族，只不过在这里它是工程约束，而不是综述条目。

**路由专家处在降维投影后的空间里。** `expert_latent_length` 是 **3584**，而不是 `hidden_size` 7168。若按 hidden 来确定专家 GEMM（矩阵-矩阵乘）的尺寸，每一个都会错 2 倍。这也是张量并行 all-reduce 宽度为 3584 的原因 —— [Lecture 07](Lecture-07.md)。

**896 选 top-16 是激进的稀疏度。** 从 2.8T 中激活约 50B，是 56:1 的比例。这对算术是*好事*，对权重读取的局部性则是*坏事*：每个 token 十六个专家，从 896 个中抽取，意味着 dispatch 模式接近于在 531 GiB 上的随机访问。

### 1.1 三个产生流畅垃圾的陷阱

仓库把这些记录为配置陷阱，三者共享一个值得立刻点明的性质，因为它是 [Lecture 10](Lecture-10.md) 的主题：**它们不会崩溃。它们产生看似合理的文本。**

```text
1.  full_attn_layers is 1-INDEXED.
    The converter tests  (il + 1) in full_attn_layers.
    Off by one  ->  you run KDA where MLA belongs, and vice versa.
    Result: fluent text, wrong model.

2.  MLA is STORED AS MQA.
    head_count_kv = 1,  key_length = kv_lora + qk_rope = 576.
    A per-layer head_count_kv == 0 is what marks a KDA layer.
    Read it as real MQA -> your shard math divides 1 by tp_size and gets 0.

3.  Routed experts are in a DOWN-PROJECTED space.
    expert_latent_length 3584,  NOT hidden_size 7168.
    Size expert GEMMs off hidden -> wrong by exactly 2x, everywhere.
```

一个编译错误让你损失一小时。一个静默出错的 `full_attn_layers` 让你花一周去 benchmark 一个并非目标模型的模型。

---

## 2. 节点与量化 —— 目标为何是现在这样


<details>
<summary>English original</summary>

**Part 4 · Lecture 01 — The Workload, the Baseline, and the Ladder**

**Overview**

Before any optimization, three questions have to be answered concretely: **what is the workload**, **what are you being compared against**, and **where are you now**. Get any of them wrong and every number you produce afterwards is decoration.

This lecture answers all three for the case study, then lays out the full measured ladder — every frontier advance, in order, as recorded in the repository's own pinned baseline file.

By the end you should be able to read a model card and a node spec and derive the *shape* of the inference problem before writing a line of code: how much HBM the weights need, how many collectives a token costs, which phase will dominate, and what a credible baseline even is when the mainstream engines cannot load your model.

---

**1. The model — Kimi K3 read as a systems artifact**

Part 3 Lecture 01 taught how to read an MoE model card. Kimi K3 is that exercise on hard mode.

| | |
|---|---|
| Parameters | **2.8T** total, typed `2.8T.A50B` by the reference implementation (~50B active) |
| Layers | **93** — **24 MLA** (full attention) + **69 KDA** (linear, recurrent) |
| Hidden / vocab | 7168 / 163,840 |
| Context | 1,048,576 |
| MoE | **896 routed experts**, top-16, **2 shared** · latent MoE at **3584**, expert FFN 3072 |
| Attention (full) | **MLA** — `q_lora 1536`, `kv_lora 512`, **NoPE-only**, sigmoid output gate |
| Attention (linear) | **KDA** — 96 heads × 128, conv kernel 4, full-rank gate, `gate_lower_bound −5.0` |
| Activation | `situ` replaces SwiGLU everywhere (`β 4.0`, linear `β 25.0`) |
| Extras | cross-layer residual attention, `block_size 12` |
| Vision | MoonViT-3d — 27 layers, 1024 wide, non-square fused QKV, patch 14 |

Four things in that table change the systems problem qualitatively, and each is a *different* lesson from the dense-70B and 671B-MoE cases in Parts 2 and 3.

**896 experts is past the mainstream limit.** Upstream `llama.cpp` asserts `n_expert <= LLAMA_MAX_EXPERTS`, and the cap is 512. The model does not load slowly — it does not load. §3 is about what that does to the idea of a baseline.

**The hybrid attention stack has two cost curves, not one.** 24 MLA layers grow a KV cache with context; 69 KDA layers carry fixed-size recurrent state. Optimizing one does nothing for the other, and a profile that averages over layers hides which is which. This is the Mamba/hybrid architecture family from [MLSys Deep Dives Lecture 04](../../05-MLSys深度探索/Lecture-04.md), but as an engineering constraint rather than a survey entry.

**The routed experts live in a down-projected space.** `expert_latent_length` is **3584**, not `hidden_size` 7168. Size the expert GEMMs off hidden and every one of them is wrong by 2×. It is also why the tensor-parallel all-reduce is 3584-wide — [Lecture 07](Lecture-07.md).

**Top-16 of 896 is aggressive sparsity.** ~50B active from 2.8T is a 56:1 ratio. That is *good* for arithmetic and *bad* for weight-read locality: sixteen experts per token, drawn from 896, means the dispatch pattern is close to random access over 531 GiB.

**1.1 Three traps that produce fluent garbage**

The repository documents these as configuration traps, and all three share one property worth naming immediately, because it is the theme of [Lecture 10](Lecture-10.md): **they do not crash. They produce plausible text.**

```text
1.  full_attn_layers is 1-INDEXED.
    The converter tests  (il + 1) in full_attn_layers.
    Off by one  ->  you run KDA where MLA belongs, and vice versa.
    Result: fluent text, wrong model.

2.  MLA is STORED AS MQA.
    head_count_kv = 1,  key_length = kv_lora + qk_rope = 576.
    A per-layer head_count_kv == 0 is what marks a KDA layer.
    Read it as real MQA -> your shard math divides 1 by tp_size and gets 0.

3.  Routed experts are in a DOWN-PROJECTED space.
    expert_latent_length 3584,  NOT hidden_size 7168.
    Size expert GEMMs off hidden -> wrong by exactly 2x, everywhere.
```

A compile error costs you an hour. A silently wrong `full_attn_layers` costs you a week of benchmarking a model that is not the model.

---

**2. The node and the quant — why the target is what it is**

</details>

### 2.1 刻意为之的 Hopper

项目明确给出的立场：**H200 是目标，不是通往 Blackwell 的跳板。** 理由是可获得性而非峰值 FLOPs —— 每卡 141 GB、八卡、**1128 GiB** HBM，硬件今天下午就能租到，且每 GB HBM 的成本以很大优势最低。一个只有在 B200/B300 上才划算的 runtime，是几乎没人跑得起来的 runtime。

这是个站得住脚的工程选择，值得与第 3 部分对照：后者面向 GB200/GB300 NVL72，因为万亿参数 MoE（混合专家模型）推理服务的规模化落地在那里。就各自的范围而言，两者都对。教训在于，“选哪块硬件”是一个产品决策，它随后约束你写的每一个 kernel —— `sm_90` 没有 FP4，也没有 TE2，所以这里的精度下限是整数量化，而不是 microscaling。

### 2.2 量化阶梯，以及“默认”与“目标”的区别

| quant | GiB | 8× H200 | top-1 vs lossless |
|---|---:|:-:|---:|
| **UD-IQ1_S** *（默认）* | **553** | ✅ 已实测 | 78.9% |
| UD-IQ2_XXS | 662 | ✅ | 84.1% |
| **UD-Q2_K_XL** *（准确率目标）* | **802** | ✅ 尚未运行 | **90.4%** |
| UD-Q4_K_XL | 1407 | ❌ | — |

两个不同的主张，repo 有意将它们分开：

* **UD-Q2_K_XL 是准确率拐点** —— 相对无损参考的 top-1 为 90.4% —— 项目称最终应当据以评判的正是它。
* **UD-IQ1_S 是默认值**，因为若默认触发一次 802 GiB 的下载、而任何机器上都没有这份数据，那么每次全新调用都会在真正开始做事之前就死掉。

这造出的陷阱是一个*测量*陷阱，其解法值得照搬。IQ1_S 更小，因此 decode（逐 token 生成阶段）更快。把一个 IQ1_S 的数字放进未加限定的基线槽位，此后每一项收益都是相对被抬高的参考值测出来的 —— 被低估，**永远**。所以槽位把量化写进名字里：`KIMI_K3_H200X8_IQ1S_LLAMA_128K`。在一个量化下测得的数字，永远不会被读成另一个量化的数字。

> **照搬这一条。** 任何含义依赖于某个配置轴的基线值，都应把该轴写在*标识符里*，而不是写在注释里。注释熬不过一次复制粘贴，名字可以。

### 2.3 强制所有 weight 驻留 HBM

553 GiB 分摊到 8 张卡上是每卡常驻约 123 GiB，而可用容量为 141 GB。这很紧，压力之下的诱惑是允许一点点溢出。harness（agent 运行时框架）拒绝这样做：`kimi_k3_check_fits` 会拒绝部分 offload 的配置，而不是去跑它，并且它**把 KV cache 计入成本**，而不是假设有平坦的余量（1M 上下文下为 27 GiB）。

原因是可复现性，而不是纯粹性。部分 offload 的一次运行，其速度取决于主机 RAM 速度、PCIe 争用和 page cache 状态 —— 这些都不在你的 commit 里。它不是更慢的数字；它是一个*不同的测量*，只是穿着同样的单位。

---

## 3. 为什么基线是一个 fork —— 以及当没有任何东西能跑你的模型时该怎么办

这个引擎家族中的其他每一个模型，都是在固定 commit 上与 `ggml-org/llama.cpp` 做 benchmark 的。K3 做不到：上游断言 `n_expert <= 512`，而且**没有可比对的上游数字。**

这是一个真实存在却少被讨论的局面。诚实的选项有：

| 选项 | 问题 |
|---|---|
| 与*另一个*模型比较 | 测的是模型，不是引擎。 |
| 与过去的自己比较 | 没有绝对锚点；慢引擎能永远制造出巨大的相对提升。 |
| 与上游能加载的更小量化比较 | 权重不同，bytes/token 也不同。不是同一个测量。 |
| **固定一个能加载它的 fork** | fork 可能在你脚下变动。需要 provenance 机制。 |

项目选了最后一项，并把那套机制建了起来。参考是 [`unslothai/llama.cpp`](https://github.com/unslothai/llama.cpp) PR #48，通过 repo + ref + commit + base commit 固定在 `bench/scripts/reference.lock` 中。那个 fork 里有四样东西是承重的，而不是装饰性的：

1. `LLAMA_MAX_EXPERTS 1024` —— 没有它，模型会在加载时断言失败。
2. `LLM_ARCH_KIMI_K3` 图 —— 混合 KDA + MLA、latent MoE、`situ`、跨层 attention 残差、MLA 输出门、full-rank KDA 门。
3. `graph_max_nodes = max(n_tokens × 160, 64 × n_tensors)` —— 其他混合架构共用的通用 `× 40` 预算在 ubatch 3840 处耗尽。
4. 四个**必须显式指定而非取默认值**的 KV key（`expert_latent_length`、`attn_res.block_size`，两个 `situ` beta）。悄悄给它们取默认值会干净地加载并输出垃圾 —— 这正是一个基线必须拒绝做的事。


<details>
<summary>English original</summary>

**2.1 Hopper, on purpose**

The project's stated position: **H200 is the target, not a stepping stone to Blackwell.** The reasoning is availability rather than peak FLOPs — 141 GB per card, eight cards, **1128 GiB** of HBM, on hardware rentable this afternoon and by a wide margin the cheapest per GB of HBM. A runtime that only pays off on B200/B300 is a runtime almost nobody can run.

That is a defensible engineering choice and worth contrasting with Part 3, which targets GB200/GB300 NVL72 because *that* is where trillion-parameter MoE serving lands at scale. Both are right for their scope. The lesson is that "which hardware" is a product decision that then constrains every kernel you write — `sm_90` has no FP4 and no TE2, so the precision floor here is integer quantization, not microscaling.

**2.2 The quant ladder, and the difference between "default" and "target"**

| quant | GiB | 8× H200 | top-1 vs lossless |
|---|---:|:-:|---:|
| **UD-IQ1_S** *(default)* | **553** | ✅ measured | 78.9% |
| UD-IQ2_XXS | 662 | ✅ | 84.1% |
| **UD-Q2_K_XL** *(accuracy target)* | **802** | ✅ not yet run | **90.4%** |
| UD-Q4_K_XL | 1407 | ❌ | — |

Two distinct claims, and the repo keeps them separate on purpose:

* **UD-Q2_K_XL is the accuracy knee** — 90.4% top-1 against a lossless reference — and is what the project says it should ultimately be judged on.
* **UD-IQ1_S is the default** because a default that triggers an 802 GiB download present on no machine means every fresh invocation dies before doing anything.

The trap this creates is a *measurement* trap, and the fix is worth stealing. IQ1_S is smaller and therefore decodes faster. Pin an IQ1_S number into an unqualified baseline slot, and every future gain is measured against an inflated reference — understated **forever**. So the slots carry the quant in the name: `KIMI_K3_H200X8_IQ1S_LLAMA_128K`. A number measured under one quant can never be read as the other.

> **Steal this.** Any baseline value whose meaning depends on a configuration axis should carry that axis *in its identifier*, not in a comment. Comments do not survive a copy-paste; names do.

**2.3 All weights in HBM, enforced**

553 GiB across 8 cards is ~123 GiB resident per card against 141 GB available. That is tight, and the temptation under pressure is to let a little spill. The harness refuses: `kimi_k3_check_fits` rejects a partially-offloaded configuration rather than running one, and it **prices the KV cache in** rather than assuming flat headroom (27 GiB at 1M context).

The reason is reproducibility, not purity. A partially-offloaded run's speed depends on host RAM speed, PCIe contention, and page-cache state — none of which are in your commit. It is not a slower number; it is a *different measurement* wearing the same units.

---

**3. Why the baseline is a fork — and what to do when nothing can run your model**

Every other model in this engine's family is benchmarked against `ggml-org/llama.cpp` at a pinned commit. K3 cannot be: upstream asserts `n_expert <= 512` and **there is no upstream number to compare against.**

This is a real and under-discussed situation. The honest options are:

| Option | Problem |
|---|---|
| Compare to a *different* model | Measures the model, not the engine. |
| Compare to yourself over time | No absolute anchor; a slow engine mints big relative wins forever. |
| Compare to a smaller quant that upstream loads | Different weights, different bytes/token. Not the same measurement. |
| **Pin a fork that can load it** | Fork can move under you. Requires provenance machinery. |

The project took the last and built the machinery. The reference is [`unslothai/llama.cpp`](https://github.com/unslothai/llama.cpp) PR #48, pinned by repo + ref + commit + base commit in `bench/scripts/reference.lock`. Four things in that fork are load-bearing rather than cosmetic:

1. `LLAMA_MAX_EXPERTS 1024` — without it the model asserts at load.
2. The `LLM_ARCH_KIMI_K3` graph — hybrid KDA + MLA, latent MoE, `situ`, cross-layer attention residual, MLA output gate, full-rank KDA gate.
3. `graph_max_nodes = max(n_tokens × 160, 64 × n_tensors)` — the generic `× 40` budget shared by other hybrid architectures is exhausted at ubatch 3840.
4. Four **required-not-defaulted** KV keys (`expert_latent_length`, `attn_res.block_size`, both `situ` betas). Silently defaulting them loads cleanly and emits garbage — precisely what a baseline must refuse to do.

</details>

### 3.1 溯源问题，以及一个值得复制的修复

别人 fork 上的 PR head 不是不可变的。它可以被 force-push。而且因为 pin 是一个 PR ref，**GitHub 拒绝 fetch-by-sha** —— 你无法直接请求你想要的 commit。

harness（agent 运行时框架）抓取 *ref*，然后**断言**它解析到 pinned commit。因此 force-push 会**导致运行失败**，而不是悄悄移动未来每次比较所用的基线。

同样的推理产生了一个每周 `pin-audit` job，因为基线所依赖且无法控制的两个外部事物，恰恰是那两样会漂移的东西：fork 的 PR 分支，以及别人 Hugging Face repo 里的量化权重（重新上传可能会改变 shard 数量）。按计划捕获漂移比在付费使用节点时才发现它更便宜。

> **一般规则。** 如果你的基线位于别人的仓库中，你并没有一个 pinned 基线 —— 你只有一个对它的 *请求* —— 直到你的 harness 中有什么东西在它移动时大声失败。

---

## 4. 起点

2026-07-31，首次测量，8× H200，UD-IQ1_S，两个引擎在同一台机器上：

```text
                    llama.cpp     sparkinfer-k3
  decode @ ctx 64      18.32          3.55        ~5.2x slower
```


该 repo 自己关于那个数字的说明值得引用，因为它是对糟糕首次结果的正确态度：

> *"对于一个 correctness-first 的 fp32 executor 来说，面对 llama.cpp 成熟的 CUDA 路径（integer dot products、fused ops、graph capture），这是预期中的形态 —— 但这意味着 K3 前沿远远落后。"*

参考实现拥有而新引擎没有的三样具体东西：**integer dot products**（llama.cpp 的量化 mat-vec 路径）、**fused ops**，以及 **graph capture**。这些并非偶然 —— 它们几乎恰好是 [Lecture 05](Lecture-05.md)、[Lecture 05](Lecture-05.md) 再次，以及 [Lecture 08](Lecture-08.md) 的主题。一个 correctness-first 的 f32 executor *本来就应该* 输给成熟的整数路径。知道自己缺哪三样东西是一份路线图，而不是借口。

### 4.1 那个掩盖了真实数字的数字

那个 `ctx 64` 数字不是工作负载。它是 harness 所测量的东西，因为 `kimi_k3_tp_bench` 硬编码了 `max_ctx=64` —— 而基线槽位命名为 `_128`，并且贡献指南写的是 128k。

当计分上下文被修正为真实的 131,072 时：

| 8× H200，UD-IQ1_S，相同权重 | ctx 64 | ctx 131,072 | 损失 |
|---|--:|--:|--:|
| llama.cpp | 18.32 | 18.44 | ~0% |
| sparkinfer-k3，首次测量时 | 10.34 | **1.00** | **−90%** |

llama.cpp 保留压缩的 MLA 缓存（`kv_lora` 512，f16），并且其速率基本随深度保持平坦。SparkInfer 在一个 **one block per head** 的 kernel 中，每个 token 要对超过 576 个 f32 值做规约 —— 因此它的成本随深度增长，而参考实现没有。

**在 64 上计分把一个 18× 的差距藏在了 1.8× 的差距背后，并且把激励指向了没人运行的上下文。** 这一句话就是 [Lecture 02](Lecture-02.md) 排在本部分每一节优化课之前的原因，也是 [Lecture 06](Lecture-06.md) 存在的原因。

---

## 5. 阶梯 —— 128k 下的 decode（逐 token 生成阶段）

`frontier` 是由密封评测轮在 `main` 上测得的最佳数字。它存储在 `reference.lock` 中，每轮由 bot 重写，并且 **只升不降** —— 因此慢的机器无法压低它，并为它后面的每个人铸造容易的 tier。它的 commit 历史因此是整个项目的审计轨迹。

以下是它，按顺序，在真实的 131,072-token 计分上下文下：

```text
   1.00 →  4.53 →  9.04 → 16.06 → 17.46 → 18.14 → 18.88 → 20.14 → 21.24
        → 22.12 → 26.09 → 29.93 → 33.58 → 35.63 → 40.02 → 44.85 → 46.48
                                                        ... then 56.82 → 60.17

   llama.cpp on the same box, same weights, throughout:  18.4435
```


十七次有记录的进步。从该序列中读出三件事：

**其中没有 60× 的变化。** 最大的单步是第一次（4.5×）和第二次（2.0×）；之后最大的是 16.06 → 17.46，增幅 +8.7%，而大多数是 3–12%。60× 是一个 *乘积*，而不是一个发现。这是关于真实优化工作的最重要结构性事实，也是奖励系统为边际收益付费的原因。

**它在 18.88 附近越过参考实现。** 在那之前的一切都是在追赶；之后的一切都是领先。tier 系统在那个交叉点改变性质 —— 参见 [Lecture 02 §4](Lecture-02.md)。

**参考实现本身移动过一次，而且它不是一次加速。** 在 26.09 和 29.93 之间有一个 commit，把 llama.cpp 的 pin 从 **16.7026 → 18.4435** 修正。旧值是在一台 *不同的机器* 上取得的单次重复；新值是在为每个 PR 计分的机器上取得的 3 次重复的中位数。没有人变快或变慢。尺子错了，而修正它使后续每个 tier 都缩小了约 9.4%。


<details>
<summary>English original</summary>

**3.1 The provenance problem, and a fix worth copying**

A PR head on someone else's fork is not immutable. It can be force-pushed. And because the pin is a PR ref, **GitHub refuses fetch-by-sha** — you cannot simply ask for the commit you want.

The harness fetches the *ref*, then **asserts** it resolves to the pinned commit. A force-push therefore **fails the run** instead of quietly moving the baseline underneath every future comparison.

The same reasoning produced a weekly `pin-audit` job, because the two external things the baseline depends on and cannot control are exactly the two things that drift: the fork's PR branch, and the quantized weights in someone else's Hugging Face repo (a re-upload can change the shard count). Catching drift on a schedule is cheaper than discovering it while paying for a node.

> **The general rule.** If your baseline lives in someone else's repository, you do not have a pinned baseline — you have a *request* for one — until something in your harness fails loudly when it moves.

---

**4. Where it started**

2026-07-31, first measurement, 8× H200, UD-IQ1_S, both engines on the same box:

```text
                    llama.cpp     sparkinfer-k3
  decode @ ctx 64      18.32          3.55        ~5.2x slower
```

The repo's own note on that number is worth quoting, because it is the correct attitude toward a bad first result:

> *"That is the expected shape for a correctness-first fp32 executor against llama.cpp's mature CUDA path (integer dot products, fused ops, graph capture) — but it means the K3 frontier starts far behind."*

Three specific things the reference had and the new engine did not: **integer dot products** (llama.cpp's quantized mat-vec path), **fused ops**, and **graph capture**. Those are not incidental — they are, almost exactly, the subjects of [Lecture 05](Lecture-05.md), [Lecture 05](Lecture-05.md) again, and [Lecture 08](Lecture-08.md). A correctness-first f32 executor is *supposed* to lose to a mature integer path. Knowing which three things you are missing is a roadmap, not an excuse.

**4.1 The number that was hiding the real one**

That `ctx 64` figure was not the workload. It was what the harness measured because `kimi_k3_tp_bench` hardcoded `max_ctx=64` — while the baseline slot was named `_128`, and the contribution guide said 128k.

When the scored context was corrected to a real 131,072:

| 8× H200, UD-IQ1_S, same weights | ctx 64 | ctx 131,072 | lost |
|---|--:|--:|--:|
| llama.cpp | 18.32 | 18.44 | ~0% |
| sparkinfer-k3, as first measured | 10.34 | **1.00** | **−90%** |

llama.cpp keeps a compressed MLA cache (`kv_lora` 512, f16) and holds its rate essentially flat with depth. SparkInfer reduced over 576 f32 values per token in a kernel with **one block per head** — so its cost grew with depth while the reference's did not.

**Scoring at 64 hid an 18× gap behind a 1.8× one, and pointed the incentive at a context nobody runs.** That single sentence is the reason [Lecture 02](Lecture-02.md) comes before every optimization lecture in this part, and the reason [Lecture 06](Lecture-06.md) exists at all.

---

**5. The ladder — decode at 128k**

The `frontier` is the best number measured on `main` by a sealed eval round. It is stored in `reference.lock`, rewritten by the bot each round, and **raise-only** — so a slow box cannot deflate it and mint easy tiers for everyone behind it. Its commit history is therefore an audit trail of the whole project.

Here it is, in order, at the real 131,072-token scored context:

```text
   1.00 →  4.53 →  9.04 → 16.06 → 17.46 → 18.14 → 18.88 → 20.14 → 21.24
        → 22.12 → 26.09 → 29.93 → 33.58 → 35.63 → 40.02 → 44.85 → 46.48
                                                        ... then 56.82 → 60.17

   llama.cpp on the same box, same weights, throughout:  18.4435
```

Seventeen recorded advances. Read three things off that sequence:

**There is no 60× change in it.** The largest single step is the first (4.5×) and the second (2.0×); after that the biggest is 16.06 → 17.46 at +8.7%, and most are 3–12%. The 60× is a *product*, not a discovery. This is the single most important structural fact about real optimization work and the reason the reward system pays marginal gains.

**It crosses the reference around 18.88.** Everything before that is catching up; everything after is a lead. The tier system changes character at that crossing — see [Lecture 02 §4](Lecture-02.md).

**The reference itself moved once, and it was not a speedup.** Between 26.09 and 29.93 sits a commit that corrected the llama.cpp pin from **16.7026 → 18.4435**. The old value was a single rep taken on a *different box*; the new one is a 3-rep median on the box that scores every PR. Nobody got faster or slower. The yardstick was wrong, and correcting it made every subsequent tier ~9.4% smaller.

</details>

### 5.1 最大的已合并步骤，以及它们各自声称的数字

frontier 是在合并之后于 `main` 上测得的；PR 自己的声称则是对另一棵树所做的独立测量。两者通常吻合得很紧 —— `#114` 声称 45.38，`main` 随后测得 44.85；`#115` 声称 46.49，`main` 测得 46.48 —— 但它们是不同的数字，把它们混为一谈，正是阶梯开始脱离现实的起点。

| PR | Tier | 以其自己的说法给出的声称 | Family |
|---|---|---|---|
| [#25](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/25) | `xl` | Q8_0 投影路径 —— 3.54 → 9.94 tok/s，逐位一致 | [L05](Lecture-05.md) |
| [#49](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/49) | `xl` | 按上下文切分 MLA decode —— **128k 时 4.49×** | [L06](Lecture-06.md) |
| [#57](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/57) | `xl` | 把 128k decode 砍半 —— 按 block 批量处理 MLA 头，加宽 Q8_0 投影 | [L06](Lecture-06.md) |
| [#63](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/63) | `xl` | 对两个 attention 带都做 head 分片，为切分上限做预算 | [L06](Lecture-06.md) |
| [#89](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/89) | `l` | 常驻设备的 decode —— 每 rank 的 CUDA graphs + 切分 MLA 合并（**+22.7%**） | [L08](Lecture-08.md) |
| [#96](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/96) | `l` | 二维 MoE 分片 —— expert 组 × FFN 带（**+12.1%**） | [L07](Lecture-07.md) |
| [#107](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/107) | `l` | warp 预算的投影层级、分带 LM head、单次 rendezvous 的全规约（**+15.9%**） | [L04](Lecture-04.md), [L07](Lecture-07.md) |
| [#114](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/114) | `l` | 在每个 decode layer 内重叠独立工作 —— 41.31 → 47.00 | [L08](Lecture-08.md) |
| [#127](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/127) | `xl` | 加宽 decode 路径的小 grid 启动 —— 48.87 → 59.26 | [L04](Lecture-04.md) |

注意出现了哪些 family：**attention 形状**（四个 `xl` 中的三个）、**启动几何**、**graph 常驻**、**量化投影路径**、**expert 分片**。其中没有一个属于「更好的矩阵乘」。这个分布正是 Lectures 04–08 的实际课程内容。

---

## 6. 阶梯 —— 32k 下的 prefill

2026-08-05，项目**改变了计分对象**：32k 下的 prefill 成为 tier 基准，而 128k 下的 decode 成为回归守卫。frontier 数字在那一刻明显*下降* —— 从 46.48 到 40.35 —— 因为它开始度量另一件事。

```text
  40.35 → 53.02 → 59.59 → 66.62 → 69.02  →  (98.80, unsealed)  →  99.68

  llama.cpp on the same box:  143.88 ± 0.23 tok/s  (±0.16%)
```

切换的理由来自贡献指南：当引擎在该项上落后 18× 时，decode 是值得计分的东西；而在领先 3.08× 之后，剩下的空间已经不大，**未被触及的差距是 prompt 摄入**。当时根本没有批量 prefill —— 每个 prompt token 都要走单 token 的 decode 步骤，因此摄入 32,768 个 token 耗时 812.2 秒。*摄入并不比生成更快。*

[Lecture 09](Lecture-09.md) 完整讲述了这段故事，包括那次陈旧种子的事故：40.35 是手工种下的，并被归到了错误的 commit 上，于是三个 PR 针对一个比现实低 31% 的数字做优化，报告出 +25.4%、+24.5% 和 +5.3% 的收益，而实际上它们是 **−4.6%、−5.3% 和 −19.8%**。

### 6.1 当前状态，如实陈述

| 8× H200 · UD-IQ1_S | llama.cpp | SparkInfer-K3 | |
|---|--:|--:|--:|
| decode @ 128k | 18.44 | **60.17** | **领先 3.26×** |
| prefill @ 32k | 143.88 | **99.68** | 落后 1.44× |

两行都属于头条。只报告自己赢的那一行的引擎是在做营销；真正有趣的工程问题 —— 还剩什么 —— 存在于它输的那一行里。

另请注意，99.68 是**真实但未封存**的：没有任何一轮评测测过它，因此固定的 frontier 有意停在 69.02，即最后一个经证实的值。*measured* 与 *attested* 之间的这一区别，正是 [Lecture 02](Lecture-02.md) 的主题。

---


<details>
<summary>English original</summary>

**5.1 The largest merged steps, with their own claimed numbers**

The frontier is measured on `main` after a merge; a PR's own claim is a separate measurement of a separate tree. They usually agree closely — `#114` claimed 45.38 and `main` then measured 44.85; `#115` claimed 46.49 and `main` measured 46.48 — but they are not the same number, and conflating them is how a ladder starts drifting from reality.

| PR | Tier | Claim, in its own words | Family |
|---|---|---|---|
| [#25](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/25) | `xl` | Q8_0 projection path — 3.54 → 9.94 tok/s, bit-identical | [L05](Lecture-05.md) |
| [#49](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/49) | `xl` | split MLA decode over context — **4.49× at 128k** | [L06](Lecture-06.md) |
| [#57](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/57) | `xl` | cut 128k decode in half — batch MLA heads per block, widen Q8_0 projections | [L06](Lecture-06.md) |
| [#63](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/63) | `xl` | head-shard both attention bands, budget the split cap | [L06](Lecture-06.md) |
| [#89](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/89) | `l` | device-resident decode — per-rank CUDA graphs + split MLA combine (**+22.7%**) | [L08](Lecture-08.md) |
| [#96](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/96) | `l` | 2-D MoE sharding — expert groups × FFN band (**+12.1%**) | [L07](Lecture-07.md) |
| [#107](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/107) | `l` | warp-budget projection tier, banded LM head, one-rendezvous all-reduce (**+15.9%**) | [L04](Lecture-04.md), [L07](Lecture-07.md) |
| [#114](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/114) | `l` | overlap independent work inside each decode layer — 41.31 → 47.00 | [L08](Lecture-08.md) |
| [#127](https://github.com/gittensor-ai-lab/sparkinfer-k3/pull/127) | `xl` | widen the decode path's small-grid launches — 48.87 → 59.26 | [L04](Lecture-04.md) |

Note which families appear: **attention shape** (three of the four `xl`s), **launch geometry**, **graph residency**, **quantized projection paths**, and **expert sharding**. Not one of them is "a better matmul." That distribution is the actual curriculum of Lectures 04–08.

---

**6. The ladder — prefill at 32k**

On 2026-08-05 the project **changed what it scored**: prefill at 32k became the tier basis, and decode at 128k became a regression guard. The frontier number visibly *drops* at that point — from 46.48 to 40.35 — because it started measuring a different thing.

```text
  40.35 → 53.02 → 59.59 → 66.62 → 69.02  →  (98.80, unsealed)  →  99.68

  llama.cpp on the same box:  143.88 ± 0.23 tok/s  (±0.16%)
```

The reason for the switch, from the contribution guide: decode was the right thing to score while the engine was 18× behind there; at 3.08× ahead the remaining headroom was small, and **the untouched gap was prompt ingestion**. There was no batched prefill at all — every prompt token went through the single-token decode step, so ingesting 32,768 tokens took 812.2 seconds. *Ingesting was not faster than generating.*

[Lecture 09](Lecture-09.md) is that story in full, including the stale-seed accident: 40.35 was hand-seeded and attributed to the wrong commit, so three PRs optimized against a number 31% below reality and reported gains of +25.4%, +24.5% and +5.3% that were actually **−4.6%, −5.3% and −19.8%**.

**6.1 The current state, honestly stated**

| 8× H200 · UD-IQ1_S | llama.cpp | SparkInfer-K3 | |
|---|--:|--:|--:|
| decode @ 128k | 18.44 | **60.17** | **3.26× ahead** |
| prefill @ 32k | 143.88 | **99.68** | 1.44× behind |

Both rows belong in the headline. An engine that reports only the row it wins is doing marketing; the interesting engineering question — what is left — lives in the row it loses.

Note also that 99.68 is **real but unsealed**: no eval round has measured it, so the pinned frontier deliberately holds at 69.02, the last attested value. That distinction between *measured* and *attested* is the subject of [Lecture 02](Lecture-02.md).

---

</details>

## 7. 读懂阶梯 —— 复利算术

由于增益是相乘的，多数工程师带进优化工作的直觉会以某种特定方式出错。小百分比并不小。

```text
   twenty changes, each +10%          1.10^20  =  6.7x
   twenty changes, each  +5%          1.05^20  =  2.7x
   ONE change of +200%, then nothing              3.0x

   the seventeen recorded decode advances, compounded:
       1.00 -> 46.48   =   46x        (mean step ~ +25%, median ~ +7%)
```

关于如何花掉一个月，有两个推论：

* **能落地的 5% 收益胜过落不了地的 40% 收益。** 七个 5% 收益是 1.41×；你从未做完的那 40% 是 1.0×。
* **奖励体系必须为边际增益付费，否则等于白付。** 这正是案例仓库按 `delta / frontier` 付费而不是按排名付费的原因："抄领先者 + ε" 得到的收益 ≈ ε。任何奖励 *位置* 而非 *增量* 的激励，都在资助对第一个优化的第二十次重新实现。

而对你如何 *汇报* 一个月，有一个推论："我们做到了 60×" 的诚实形式是阶梯，而不是比值。比值无法证伪；阶梯可以逐行审计，因此它才是本部分要求你产出的产物。

---

## 实验 —— 在做 profile 之前先推导出形状

目标：针对一个你确实能访问的模型和节点，复现本讲的 *推理*，并在做任何测量之前先把预测提交。

1. **挑一个工作负载。** 一个你能跑的模型，以及一个你能租到或借到的节点。它不必很大 —— 这套做法与规模无关。
2. **填写模型表**（§1），依据官方 config，而不是博客文章。层数、hidden、vocab、attention 类型及其 KV bytes/token、FFN 宽度、如有 MoE（混合专家模型）则填其路由。
3. **计算内存预算。** 你选定量化下的权重、目标上下文下的 KV 或循环状态、激活值与计算缓冲区。与总 HBM 比较。用数字写出你的余量。
4. **预测带宽上限。** `tokens/s ≤ HBM GB/s ÷ bytes-read-per-token`。对 MoE，只计入 *激活* 专家的字节数。把数字写下来；[Lecture 03](Lecture-03.md) 会用到它。
5. **统计集合通信操作。** 按你打算使用的并行方式，每 token 有多少次全规约，每次的宽度是多少？从非线性所在的位置推导，而不是从一张图。
6. **写明你的基线并把它固定住。** 仓库、commit、权重哈希、精确的命令行。如果你的基线加载不了你的模型，说明你打算改用什么 —— 以及那会让你付出什么代价（§3）。
7. **写下三个陷阱**，位于你的模型 config 中，它们会产生看似合理但错误的结果，而不是崩溃。如果你找不出三个，那说明你还没读过转换器。

通过标准：你的产物仓库里有一页 `SHAPE.md`，在你第一次 profile **之前**提交，其中包含预测的带宽上限和一个固定住的基线。[Lecture 03](Lecture-03.md) 会用一次实测来给你的预测打分。

---

## 自检

1. K3 有 896 个专家，top-16，被路由到的专家位于 3584 宽的 latent 空间，专家 FFN 为 3072。估算每 token 读取的 *激活* 专家权重字节数（约 1.6 bits/weight），再用 8× H200 的聚合 HBM 带宽来界定 decode（逐 token 生成阶段）的 tok/s 上界。你的上界高于 60.17 吗？如果没有，说明什么？
2. 该引擎在 ctx 64 时测得 10.34 tok/s，在 ctx 131,072 时测得 1.00 tok/s，而参考实现在同一区间内保持 18.32 → 18.44。仅凭这四个数字，你能就引擎在长上下文下把时间花在哪里得出什么结论 —— 你最先会去 profile 什么？
3. 同事的基线是第三方 fork 上固定住的某个 PR 分支。说出该基线可能静默变化的两种方式，以及各自对应的能抓出它的断言。
4. llama.cpp 的参考 pin 被从 16.7026 更正为 18.4435，使每个已授予的档位都缩小了约 9.4%。没有任何人的代码发生变化。请就"是否应当追溯性地重新给已合并的 PR 打分"给出支持或反对的论证。
5. 某引擎报告 "比参考实现快 3.26×"。在相信它适用于你的部署之前，你会问哪四个问题？
6. 为什么把一个 IQ1_S 测量值固定进一个不具备量化资格的基线槽位，会 *永久性地* 低估未来的收益，而不只是低估一次？


<details>
<summary>English original</summary>

**7. Reading a ladder — the compounding arithmetic**

Because gains multiply, the intuition most engineers carry into optimization work is wrong in a specific way. Small percentages are not small.

```text
   twenty changes, each +10%          1.10^20  =  6.7x
   twenty changes, each  +5%          1.05^20  =  2.7x
   ONE change of +200%, then nothing              3.0x

   the seventeen recorded decode advances, compounded:
       1.00 -> 46.48   =   46x        (mean step ~ +25%, median ~ +7%)
```

Two consequences for how you spend a month:

* **A 5% win that ships beats a 40% win that does not.** Seven 5% wins is 1.41×; the 40% you never finished is 1.0×.
* **The reward system has to pay marginal gains, or it pays for nothing.** This is why the case-study repo pays `delta / frontier` rather than rank: "copy the leader + ε" pays ≈ ε. Any incentive that rewards *position* rather than *increment* funds the twentieth reimplementation of the first optimization.

And one consequence for how you *report* a month: the honest form of "we got 60×" is the ladder, not the ratio. The ratio is unfalsifiable; the ladder can be audited row by row, which is why it is the artifact this part asks you to produce.

---

**Lab — derive the shape before you profile**

Goal: reproduce the *reasoning* of this lecture for a model and node you can actually access, and commit the predictions before you measure anything.

1. **Pick a workload.** A model you can run and a node you can rent or borrow. It does not need to be large — the discipline is scale-free.
2. **Fill the model table** (§1) from the official config, not a blog post. Layers, hidden, vocab, attention type and its KV bytes/token, FFN width, MoE routing if any.
3. **Compute the memory budget.** Weights at your chosen quantization, KV or recurrent state at your target context, activation and compute buffers. Compare to total HBM. State your headroom as a number.
4. **Predict the bandwidth ceiling.** `tokens/s ≤ HBM GB/s ÷ bytes-read-per-token`. For MoE, count only *active* expert bytes. Write the number down; [Lecture 03](Lecture-03.md) will use it.
5. **Count the collectives.** For your intended parallelism, how many all-reduces per token, and how wide is each? Derive it from where the non-linearities sit, not from a diagram.
6. **Name your baseline and pin it.** Repo, commit, weights hash, exact command line. If your baseline cannot load your model, say what you will do instead — and what that costs you (§3).
7. **Write down three traps** in your model's config that would produce plausible-but-wrong output rather than a crash. If you cannot find three, you have not read the converter.

Pass criterion: a one-page `SHAPE.md` in your artifact repo, committed **before** your first profile, containing a predicted bandwidth ceiling and a pinned baseline. [Lecture 03](Lecture-03.md) grades your prediction against a measurement.

---

**Self-check**

1. K3 has 896 experts, top-16, with routed experts in a 3584-wide latent space and expert FFN 3072. Estimate the *active* expert weight bytes read per token at ~1.6 bits/weight, then use 8× H200 aggregate HBM bandwidth to bound decode tok/s. Does your bound sit above 60.17? What does it mean if it does not?
2. The engine measured 10.34 tok/s at ctx 64 and 1.00 tok/s at ctx 131,072, while the reference held 18.32 → 18.44 across the same range. From those four numbers alone, what can you conclude about where the engine's time goes at depth — and what would you profile first?
3. A colleague's baseline is a pinned PR branch on a third-party fork. Name two ways that baseline can silently change, and the assertion that catches each.
4. The llama.cpp reference pin was corrected 16.7026 → 18.4435, making every awarded tier ~9.4% smaller. Nobody's code changed. Argue for or against retroactively re-scoring the already-merged PRs.
5. An engine reports "3.26× faster than the reference." What are the four questions you ask before believing it applies to your deployment?
6. Why does pinning an IQ1_S measurement into a quant-unqualified baseline slot understate future gains *permanently*, rather than just once?

---

</details>

## References

* **SparkInfer-K3** — [github.com/gittensor-ai-lab/sparkinfer-k3](https://github.com/gittensor-ai-lab/sparkinfer-k3) (MIT)。案例研究。`docs/technical.md` 是模型 + 分片策略参考；`bench/scripts/reference.lock` 是固定基线文件，本讲的注释与提交历史即取自该文件。
* **参考引擎** — [unslothai/llama.cpp](https://github.com/unslothai/llama.cpp) PR #48 (`kimi-k3-fullsize-vision`)，能够加载 896 个专家的分支。
* **Kimi K2 技术报告** — [arXiv:2507.20534](https://arxiv.org/abs/2507.20534) — Moonshot AI 已发表的架构沿革，K3 延续了该路线（MLA + 大专家数 MoE）。
* **DeepSeek V3 技术报告** — [arXiv:2412.19437](https://arxiv.org/abs/2412.19437) — MLA 的权威描述；K3 的 MLA 仅用 NoPE，并带 sigmoid 输出门。
* **Gated DeltaNet** — [arXiv:2412.06464](https://arxiv.org/abs/2412.06464) — KDA 所属的线性 attention 家族；解释了固定大小的循环状态。

交叉引用：

* [第 1 部分 第 03 讲 — Roofline、带宽与存储层次](../01-基础/Lecture-03.md) — 你在实验室中预测的性能上界。
* [第 3 部分 第 01 讲 — 现代 MoE 剖析](../03-Blackwell-MoE/Lecture-01.md) — 671B 下的 MLA 与专家布线，即 §1 的温和版本。
* [MLSys Deep Dives 第 04 讲 — 超越稠密 Transformer](../../05-MLSys深度探索/Lecture-04.md) — 混合线性/全 attention 栈为何存在。

---

## 截至 2026-08

SparkInfer-K3 为 `7689cc7`；参考 `unslothai/llama.cpp` PR #48 @ `efc8bc38`；权重 `Kimi-K3-UD-IQ1_S`（553 GiB，14 个分片）；节点 8× H200 SXM `sm_90`，CUDA 12.8+。decode 60.17 tok/s @ 128k，prefill 99.68 tok/s @ 32k，对照 18.44 / 143.88。这些是针对单机单引擎的冻结历史测量值——应更新其*推理*，而非数字。

---

## 下一讲

* 下一讲：[第 02 讲 — 记分牌：一个无法被套路的 benchmark](Lecture-02.md)
* 上级：[第 4 部分 — 优化真实引擎](README.md)


<details>
<summary>English original</summary>

**References**

* **SparkInfer-K3** — [github.com/gittensor-ai-lab/sparkinfer-k3](https://github.com/gittensor-ai-lab/sparkinfer-k3) (MIT). The case study. `docs/technical.md` is the model + shard-policy reference; `bench/scripts/reference.lock` is the pinned-baseline file whose comments and commit history this lecture draws on.
* **The reference engine** — [unslothai/llama.cpp](https://github.com/unslothai/llama.cpp) PR #48 (`kimi-k3-fullsize-vision`), the fork that can load 896 experts.
* **Kimi K2 technical report** — [arXiv:2507.20534](https://arxiv.org/abs/2507.20534) — the published Moonshot AI architecture lineage K3 continues (MLA + large-expert-count MoE).
* **DeepSeek V3 technical report** — [arXiv:2412.19437](https://arxiv.org/abs/2412.19437) — MLA's canonical description; K3's MLA is NoPE-only with a sigmoid output gate.
* **Gated DeltaNet** — [arXiv:2412.06464](https://arxiv.org/abs/2412.06464) — the linear-attention family KDA belongs to; explains the fixed-size recurrent state.

Cross-references:

* [Part 1 Lecture 03 — Roofline, bandwidth, and the memory hierarchy](../01-基础/Lecture-03.md) — the ceiling you predict in the lab.
* [Part 3 Lecture 01 — Anatomy of a modern MoE](../03-Blackwell-MoE/Lecture-01.md) — MLA and expert routing at 671B, the gentler version of §1.
* [MLSys Deep Dives Lecture 04 — Beyond the dense transformer](../../05-MLSys深度探索/Lecture-04.md) — why hybrid linear/full attention stacks exist.

---

**Current as of 2026-08**

SparkInfer-K3 at `7689cc7`; reference `unslothai/llama.cpp` PR #48 @ `efc8bc38`; weights `Kimi-K3-UD-IQ1_S` (553 GiB, 14 shards); node 8× H200 SXM `sm_90`, CUDA 12.8+. Decode 60.17 tok/s @ 128k, prefill 99.68 tok/s @ 32k, against 18.44 / 143.88. These are frozen historical measurements for one engine on one box — refresh the *reasoning*, not the numbers.

---

**Next**

* Next: [Lecture 02 — The scoreboard: a benchmark that cannot be gamed](Lecture-02.md)
* Up: [Part 4 — Optimizing a Real Engine](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 4 - Optimizing a Real Engine/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%204%20-%20Optimizing%20a%20Real%20Engine/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
