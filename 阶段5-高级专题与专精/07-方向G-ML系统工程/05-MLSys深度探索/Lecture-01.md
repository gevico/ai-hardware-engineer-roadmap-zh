---
title: 第 01 讲 - MLSys 作为经济价值层
description: 第 01 讲 - MLSys 作为经济价值层
---

# 第 01 讲 - MLSys 作为经济价值层

**合集：** [MLSys Deep Dives](README.md) | **上一讲：** [← MLSys Deep Dives 索引](README.md) | **下一讲：** [第 02 讲](Lecture-02.md)

---

在任何 kernel、编译器或架构之前，有一个数字统御着整个课程：**token 的成本**。MLSys 工程师做的每一件事 —— 每个融合 kernel、每种量化方案、每个投机解码头、每个混合架构 layer —— 都是为了改变这个数字。因此就从这里开始，因为如果无法把一项技术与 token 的成本关联起来，你就无法排定优先级、无法为其辩护，甚至无法识别真正重要的工作。

本讲搭建其余六讲所依附的脊柱：**为什么系统工作现在是产品**、衡量它的指标，以及把 kernel 加速比变成美元数字的那一个方程。

---

## 学习目标

学完本讲后，你应该能够：

1. 量化 2023–2026 年推理成本的暴跌，并解释其驱动因素（是系统，而非硅片）。
2. 将 **每 token 价格** 拆解为硬件与 MLSys 因素，并指出每个课程主题拉动的是哪根杠杆。
3. 正确使用指标栈：**tokens/s、首 token 时延、TPOT、TOK/$、TCO/Mtok、perf/watt** —— 并说明哪个属于用户、哪个属于运营方。
4. 读懂 perf/$ benchmark（SemiAnalysis 风格），并解释为什么印出来的吞吐数字只是教学锚点，而非部署真相。
5. 根据 GPU 租赁成本与 tokens/s，粗略计算 **$/Mtok**，并说明系统侧 2× 收益如何使其减半。

---

## 1. 暴跌

2022 年 11 月，GPT-3.5 级智能的成本约为 **每百万 $20 per million tokens**. By late 2024 the same capability was available near **$0.07** —— 约 **两年内便宜 280×**。GPT-4 级输入价格从 **$30/Mtok** at launch (March 2023) to **$2.50**（GPT-4o，2024）降至 **~$0.10**（nano 级，2025）—— 降幅超过 **99%**。DeepSeek-V3 于 2024 年 12 月面世，以约 **$0.14/Mtok** 提供前沿级质量 —— 约为 GPT-4 发布价的百分之一。

Epoch AI 的严格版本，在固定 *能力* 的前提下：为保持固定的 benchmark 分数，价格每年下降 **9× 到 900×，中位数约 50×/年**，而对 2024 年以来最便宜的模型，下降更快（中位数约 200×/年）。这些下降**并不均衡** —— 廉价、常见的任务下降最快；困难推理的前沿下降较慢。

以下是与你职业生涯相关的部分：**其中几乎没有一项来自更便宜的硬件。** H100 并没有便宜 280×。这场暴跌来自 MLSys ——

```text
   FlashAttention & better kernels    → more tokens/s per GPU
   quantization (FP16→FP8→FP4/INT4)   → more model per byte, more throughput
   continuous batching, paged KV      → higher GPU utilization
   speculative decoding               → more tokens per memory pass
   MoE + MLA + hybrid architectures   → less compute / less KV per token
   compiler fusion & scheduling       → less wasted memory traffic
```

其中每一项都是本课程的一讲。这场暴跌 *就是* 这个领域。当有人问 MLSys 工程师做什么时，诚实的答案是：**智能之所以便宜 280×，原因在我们，而且我们还没做完。**

---

## 2. 那个方程

把推理经济学剥离到核心，得到：

```text
                    energy  +  capital            $ per second to run the box
   price per token = ──────────────────  =  ───────────────────────────────────
                       tokens per second        tokens per second it produces
                       │                          │
                       └── set by hardware ───────┘
                              + power + utilization        ← MLSys lives here too
```

降低 token 价格有两种方式：让机器更便宜（硬件、电力、融资 —— 大多不是你的职责），或者让机器产出 **更多 tokens/s**（kernel、编译器、架构、decode（逐 token 生成阶段）算法 —— *完全* 是你的职责）。分母就是 MLSys 工程师的整个世界。

这就是课程如此排序的原因。每个 layer 都是增大分母的不同方式：

| layer | 课程讲次 | 如何提升 tokens/s |
|---|---|---|
| **Kernel** | 02 | 更快的 GEMM/attention kernel 用更少时间完成同样的工作 |
| **编译器 / runtime** | 03 | 融合减少内存流量；megakernel 减少启动开销 |
| **架构** | 04–05 | SSM/MLA 缩小 KV cache；MoE（混合专家模型）减少每 token 计算量 |
| **推理算法** | 06 | 投机解码每次内存遍历输出多个 token |
| **硬件 / 部署** | 07 | 在合适的 batch 下，用合适的精度跑在合适的硅片上 |

记住这个方程。本课程每引入一项技术，都把它定位到方程上。无法在方程上定位的技术，就是你尚未理解的技术。

---


<details>
<summary>English original</summary>

**Lecture 01 - MLSys as the Economic-Value Layer**

**Collection:** [MLSys Deep Dives](README.md) | **Previous:** [← MLSys Deep Dives index](README.md) | **Next:** [Lecture 02](Lecture-02.md)

---

Before any kernel, compiler, or architecture, one number governs this entire course: **the cost of a token**. Everything an MLSys engineer does — every fused kernel, every quantization scheme, every speculative-decoding head, every hybrid-architecture layer — exists to move that number. So we start there, because if you cannot connect a technique to the cost of a token, you cannot prioritize, defend, or even recognize the work that matters.

This lecture builds the spine the other six hang from: **why systems work is now the product**, the metrics that measure it, and the one equation that turns a kernel speedup into a dollar figure.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Quantify the 2023–2026 collapse in inference cost and explain what drove it (systems, not silicon).
2. Decompose **price-per-token** into hardware and MLSys factors, and name the lever each course topic pulls.
3. Use the metric stack correctly: **tokens/s, TTFT, TPOT, TOK/$, TCO/Mtok, perf/watt** — and say which is the user's and which is the operator's.
4. Read a perf/$ benchmark (SemiAnalysis-style) and explain why a printed throughput number is a teaching anchor, not deployment truth.
5. Compute a back-of-envelope **$/Mtok** from GPU rental cost and tokens/s, and show how a 2× systems win halves it.

---

**1. The collapse**

In November 2022, GPT-3.5-class intelligence cost roughly **$20 per million tokens**. By late 2024 the same capability was available near **$0.07 per million** — about **280× cheaper in two years**. GPT-4-class input fell from **$30/Mtok** at launch (March 2023) to **$2.50** (GPT-4o, 2024) to **~$0.10** (nano-class, 2025) — over **99%**. DeepSeek-V3 arrived in December 2024 at roughly **$0.14/Mtok** for frontier-class quality — about one-hundredth of GPT-4's launch price.

Epoch AI's rigorous version, holding *capability* fixed: to keep a fixed benchmark score, price falls **between 9× and 900× per year, median ~50×/year**, and faster (median ~200×/year) for the cheapest models since 2024. The declines are **uneven** — cheap, common tasks drop fastest; the frontier of hard reasoning drops slower.

Here is the part that matters for your career: **almost none of that came from cheaper hardware.** An H100 did not get 280× cheaper. The collapse came from MLSys —

```text
   FlashAttention & better kernels    → more tokens/s per GPU
   quantization (FP16→FP8→FP4/INT4)   → more model per byte, more throughput
   continuous batching, paged KV      → higher GPU utilization
   speculative decoding               → more tokens per memory pass
   MoE + MLA + hybrid architectures   → less compute / less KV per token
   compiler fusion & scheduling       → less wasted memory traffic
```

Every one of those is a lecture in this course. The collapse *is* the field. When someone asks what an MLSys engineer does, the honest answer is: **we are why intelligence got 280× cheaper, and we are not done.**

---

**2. The one equation**

Strip inference economics to its core and you get this:

```text
                    energy  +  capital            $ per second to run the box
   price per token = ──────────────────  =  ───────────────────────────────────
                       tokens per second        tokens per second it produces
                       │                          │
                       └── set by hardware ───────┘
                              + power + utilization        ← MLSys lives here too
```

Two ways to lower the price of a token: make the box cheaper (hardware, power, financing — mostly not your job), or make the box produce **more tokens per second** (kernels, compilers, architectures, decode algorithms — *entirely* your job). The denominator is the MLSys engineer's whole world.

This is why the course is ordered the way it is. Each layer is a different way to grow the denominator:

| Layer | Course lectures | How it grows tokens/s |
|---|---|---|
| **Kernel** | 02 | a faster GEMM/attention kernel does the same work in less time |
| **Compiler / runtime** | 03 | fusion cuts memory traffic; megakernels cut launch overhead |
| **Architecture** | 04–05 | SSM/MLA shrink the KV cache; MoE cuts compute per token |
| **Inference algorithm** | 06 | speculative decoding emits multiple tokens per memory pass |
| **Hardware / deployment** | 07 | the right precision on the right silicon at the right batch |

Memorize the equation. Every time this course introduces a technique, locate it on the equation. A technique you cannot place there is a technique you do not yet understand.

---

</details>

## 3. 指标栈

"tokens/s" 太粗，不足以用来做工程设计。可用的指标分为两类：**用户感受到的**，和**运营方付出的**。

```text
   USER-FACING (latency / interactivity)      OPERATOR-FACING (cost / efficiency)
   ─────────────────────────────────────      ──────────────────────────────────
   TTFT  time to first token (prefill)         $/GPU-hour   (capital + power + colo)
   TPOT  time per output token (decode)        throughput   tokens/s per GPU (or node)
   tokens/s  per-request interactivity         TOK/$   = throughput ÷ ($/s)
   p50/p99   tail latency                       TCO/Mtok = 1e6 ÷ TOK/$
                                                perf/watt   tokens/s ÷ watts
```

二者存在张力，而管理这种张力就是本职。几乎总能用延迟换吞吐（TOK/$）（加大批，让更多请求共享每次权重加载）——直到 TPOT 违反交互性 SLO。所以真正的目标从来不是一个数字：

```text
   the SLO frontier:  maximize tokens/s (and TOK/$)
                      subject to  TTFT < X ms  and  TPOT < Y ms
```

两个让新手栽跟头的事实：

* **prefill（首字前的整段计算）和 decode（逐 token 生成阶段）是两种不同的机器。** prefill（处理 prompt）是算力受限且并行的——TTFT 随 prompt 长度增长。decode（生成 token）是**内存带宽受限**的——一次一个 token，把整个权重集从 HBM 流式读出。本课程的大部分技巧都针对 decode，因为在长文本生成中，时间和成本都集中在 decode。
* **perf/watt 正在成为绑定约束。** 在数据中心规模上，先受限的是功耗，而不是空间。tokens/s/watt 正越来越成为决定超大规模厂商部署哪套技术栈的数字——这就是为什么 FP4 芯片和节能 kernel 的意义超出其原始速度。

---

## 4. 读懂 perf/$ benchmark

业界现在有了开放、持续更新的跨栈 benchmark——最著名的是 **SemiAnalysis InferenceMAX**（开源）及其 **InferenceX** TCO 计算器。它们做了本讲坚持要做的事：用**总拥有成本归一化吞吐**，并把结果表示为**每百万 token 的 TCO**，针对不同运营方类型（超大规模厂商 vs neocloud vs 自建）绘制成与交互性的关系图。

它们展示了什么，以及你应该内化什么：

* TCO 不只是 GPU。它是**服务器 capex + 电力（perf/watt）+ 托管/电费 + 资金成本**。更便宜但更耗电的 GPU，可能在 TCO/Mtok 上落败。
* 代际硬件跃升幅度很大，*而且*会被软件放大：据报道，在 reasoning 工作负载上，GB200 NVL72 的**每百万 token 成本比上一代低约 15×**——但这个数字已经把硅片之上一年份的 kernel 与 runtime 改进算进去了。
* **数字每月都在变。** 任何讲义（包括本课程）印出的吞吐数字都是一个*教学锚点*——形状正确，量级过时。到了部署时，你读的是实时看板，不是教科书。

最后这一点是一种纪律，不是免责声明。在设计评审里引用 tokens/s 数字时，要说明日期、技术栈版本和硬件——否则你引用的就是一个已经腐烂的数字。

---

## 5. 「便宜到无需计量」论——以及它为何让 MLSys 成为本职

有一种论点，化用原子时代的「电力便宜到无需计量」承诺，认为 token 的边际成本趋于可忽略。证据就是 §1 的坍塌：一个工作负载以前要花 **$10,000/month in 2023 can run for under $200，如今**。无论这条斜率是否延续，结构性结论都成立，而这正是本课程存在的理由：

> **一旦某种能力被商品化，唯一剩下的差异化因素就是交付它的成本。而这个成本由 MLSys 决定。**

当每个认真的实验室都有可比的模型时，竞争就转向**谁能以最低成本、最快速度提供服务**——这是一场 kernel、编译器、架构协同设计与 decode 算法的竞赛。模型质量改进的经济价值会随着竞争对手追上来而衰减；系统改进的经济价值则是**即时且复利的**，因为它会把 TOK/$ 乘到公司未来服务的每一个 token 上。这就是为什么在 2026 年，推理团队是利润中心，而系统工程师是把研究转化为利润空间的人。

---


<details>
<summary>English original</summary>

**3. The metric stack**

"Tokens per second" is too coarse to engineer with. The working metrics split into what the **user feels** and what the **operator pays**.

```text
   USER-FACING (latency / interactivity)      OPERATOR-FACING (cost / efficiency)
   ─────────────────────────────────────      ──────────────────────────────────
   TTFT  time to first token (prefill)         $/GPU-hour   (capital + power + colo)
   TPOT  time per output token (decode)        throughput   tokens/s per GPU (or node)
   tokens/s  per-request interactivity         TOK/$   = throughput ÷ ($/s)
   p50/p99   tail latency                       TCO/Mtok = 1e6 ÷ TOK/$
                                                perf/watt   tokens/s ÷ watts
```

The two are in tension, and managing that tension is the job. You can almost always buy throughput (TOK/$) with latency (batch harder, more requests share each weight-load) — up to the point where TPOT violates the interactivity SLO. So the real target is never one number:

```text
   the SLO frontier:  maximize tokens/s (and TOK/$)
                      subject to  TTFT < X ms  and  TPOT < Y ms
```

Two facts that trip up newcomers:

* **Prefill and decode are different machines.** Prefill (processing the prompt) is compute-bound and parallel — TTFT scales with prompt length. Decode (generating tokens) is **memory-bandwidth-bound** — one token at a time streams the whole weight set from HBM. Most of this course's tricks target decode, because decode is where the time and the cost live for long generations.
* **perf/watt is becoming the binding constraint.** At datacenter scale you are power-limited before you are space-limited. tokens/s/watt is increasingly the number that decides which stack a hyperscaler deploys — which is why FP4 silicon and energy-frugal kernels matter beyond their raw speed.

---

**4. Reading a perf/$ benchmark**

The industry now has open, continuously-updated cross-stack benchmarks — most notably **SemiAnalysis InferenceMAX** (open-source) and its **InferenceX** TCO calculator. They do the thing this lecture insists on: normalize **throughput by total cost of ownership**, and express the result as **TCO per million tokens** plotted against interactivity, for different operator types (hyperscaler vs neocloud vs self-host).

What they show, and what you should internalize:

* TCO is not just the GPU. It is **server capex + power (perf/watt) + colocation/electricity + cost-of-capital**. A cheaper GPU that burns more watts can lose on TCO/Mtok.
* Generational hardware jumps are large *and* software-multiplied: a GB200 NVL72 has been reported at roughly **15× lower cost-per-million-tokens than the prior generation** on reasoning workloads — but that figure already bakes in a year of kernel and runtime improvements on top of the silicon.
* **The numbers move monthly.** A throughput figure printed in any lecture (including this course) is a *teaching anchor* — correct in shape, stale in magnitude. At deployment time, you read the live dashboard, not the textbook.

That last point is a discipline, not a disclaimer. When you quote a tokens/s number in a design review, state the date, the stack version, and the hardware — or you are quoting a number that has already rotted.

---

**5. The "too cheap to meter" thesis — and why it makes MLSys the job**

There is a thesis, riffing on the Atomic-Age promise of "electricity too cheap to meter," that the marginal cost of a token trends toward negligible. The evidence is the §1 collapse: a workload that cost **$10,000/month in 2023 can run for under $200 now**. Whether or not the slope continues, the structural conclusion holds and it is the reason this course exists:

> **Once a capability is commoditized, the only remaining differentiator is the cost of delivering it. That cost is set by MLSys.**

When every serious lab has a comparable model, the competition moves to **who serves it cheapest and fastest** — which is a kernel, compiler, architecture-co-design, and decode-algorithm contest. The economic value of a model-quality improvement decays as competitors catch up; the economic value of a systems improvement is **immediate and compounding**, because it multiplies TOK/$ across every token the company will ever serve. That is why, in 2026, the inference team is a profit center and the systems engineer is the person turning research into margin.

---

</details>

## 6. 动手实践：构建成本模型

你会在后续每一讲的“Measure it”中复用这个计算。一次就把它建好，而且要建对。

给定一个部署：租用 `G` 块 GPU，价格为 `C` dollars/GPU-hour，并产出 `S` 总 tokens/second：

```python
def usd_per_mtok(gpus, usd_per_gpu_hr, agg_tokens_per_sec):
    usd_per_sec   = gpus * usd_per_gpu_hr / 3600.0
    tokens_per_sec = agg_tokens_per_sec
    tok_per_usd   = tokens_per_sec / usd_per_sec
    return 1e6 / tok_per_usd            # $ per million tokens

# Illustrative anchors (verify live $/GPU-hr and tokens/s at deployment):
node = dict(gpus=8, usd_per_gpu_hr=2.50)      # an 8-GPU node at ~$20/hr

for S in (2_000, 5_000, 10_000, 20_000):
    print(f"{S:>6} tok/s  ->  ${usd_per_mtok(agg_tokens_per_sec=S, **node):.2f}/Mtok")
```

输出就是整个论点，共四行：

```text
  2000 tok/s  ->  $2.78/Mtok
  5000 tok/s  ->  $1.11/Mtok
 10000 tok/s  ->  $0.56/Mtok
 20000 tok/s  ->  $0.28/Mtok
```

硬件成本（`$20/hr`）从未改变。每一次下降都来自**分母**——来自 tokens/s，也就是说来自 MLSys。kernel 工程师把 attention 吞吐翻倍，一种架构把 KV cache 减半从而让批处理深度翻倍，一个投机解码器每次内存 pass 发出两个 token：每一个都会让你*沿那一列往下走*，而那一列就是美元。

> **一个让你保持诚实的提醒：** 总 tokens/s 取决于批大小，而批处理会与 TPOT 相互权衡。上面的成本模型仅在*固定交互性 SLO* 下有效。始终把 `$/Mtok` 与测量它时的 TPOT 一起报出——没人愿意等待的便宜 token 并不便宜，而是卖不出去。

### 预测分母：带宽上限

成本模型*测量* tokens/s；你也应该能在碰 GPU 之前**预测**它。因为 decode（逐 token 生成阶段）是带宽受限的（§3），每个生成的 token 都必须从 HBM 将模型权重（外加其 KV 切片）流式读取一次——所以带宽除以字节数就是硬上限：

```text
   batch-1 decode ceiling:   tokens/s  ≤  HBM bandwidth  /  bytes streamed per token
                                          (weights touched + KV read for that token)
```

演算示例——一个 70B 级稠密模型，采用 FP16（约 140 GB 权重），在单块 H100 SXM 上（3.35 TB/s）：

```text
   3350 GB/s ÷ 140 GB  ≈  24 tokens/s    ← the ceiling, batch 1, before any cleverness
```

现在用那一个公式来读整个课程。每项主要技术都是对其两个项之一的攻击：

* **量化 FP16 → INT4** → ~140 GB 变为 ~35 GB → 上限 ~96 tok/s。*每个 token 的字节数更少。*（第 7 讲）
* **批处理 32 个请求** → 同一份权重流由 32 个 token 共享 → 总上限约高 32×（每个请求仍要承担自己的 KV 读取）。*每个字节更多 token。* 这就是为什么批处理是最大的 TOK/$ 杠杆——也是为什么限制批深度的 KV cache 如此重要。（第 4 讲）
* **接受率 τ ≈ 3 下的投机解码** → 每次对权重做 pass 约发出 3 个 token → ~3×。（第 6 讲）
* **MoE（混合专家模型）671B-total / 37B-active** → 在小批大小下，每个 token 只有约 37B 权重被流式读取 → 一个 671B 模型，却只有 37B 规模的上限。（第 5 讲）

这个公式也是你在设计评审中的第一道防线：如果某个被引用的 tokens/s 声明针对所述模型、精度和批大小*高于*带宽上限，那么就有未说明的因素在起作用——批处理、投机、稀疏化——而现在你确切知道该问哪些问题。

---

## 7. 迷你实验：把领域放到方程上

一个由两部分组成的练习，为整个课程做铺垫。

1. **成本模型。** 拿一个你能运行的真实模型 + runtime（或一个已发布的 benchmark）。在固定 TPOT 下测量或读取其总 tokens/s，并用 §6 计算 `$/Mtok`。还要计算你的模型 + 精度 + GPU 的**带宽上限**，并将测得的 tokens/s 报告为**上限的百分比**——这一个比值告诉你，本课程剩余部分还能争取多少余量。将 TTFT、TPOT、tokens/s、TOK/$、`$/Mtok` 以及 %-of-ceiling 记录为你的基线行——你会在后续每一讲中向这个表里增加梯级。
2. **地图。** 在一页纸的顶部写下 §2 方程。在分母下面，列出本课程的七个讲座主题，并为每个主题写一句它*如何*提升 tokens/s。如果你现在还写不出那句话，那一讲就是你将学到它的地方——但方程上的位置即便现在也应该显而易见。

交付物：一行基线成本模型记录，以及带注释的方程。两者都保留；从某种意义上说，本课程就是填满那一页的练习。

---


<details>
<summary>English original</summary>

**6. Hands-on: build the cost model**

You will reuse this calculation in every later lecture's "Measure it." Build it once, properly.

Given a deployment that rents `G` GPUs at `C` dollars/GPU-hour and produces `S` aggregate tokens/second:

```python
def usd_per_mtok(gpus, usd_per_gpu_hr, agg_tokens_per_sec):
    usd_per_sec   = gpus * usd_per_gpu_hr / 3600.0
    tokens_per_sec = agg_tokens_per_sec
    tok_per_usd   = tokens_per_sec / usd_per_sec
    return 1e6 / tok_per_usd            # $ per million tokens

# Illustrative anchors (verify live $/GPU-hr and tokens/s at deployment):
node = dict(gpus=8, usd_per_gpu_hr=2.50)      # an 8-GPU node at ~$20/hr

for S in (2_000, 5_000, 10_000, 20_000):
    print(f"{S:>6} tok/s  ->  ${usd_per_mtok(agg_tokens_per_sec=S, **node):.2f}/Mtok")
```

The output is the entire thesis in four lines:

```text
  2000 tok/s  ->  $2.78/Mtok
  5000 tok/s  ->  $1.11/Mtok
 10000 tok/s  ->  $0.56/Mtok
 20000 tok/s  ->  $0.28/Mtok
```

The hardware cost (`$20/hr`) never changed. Every drop came from the **denominator** — from tokens/s, which is to say from MLSys. A kernel engineer who doubles attention throughput, an architecture that halves the KV cache so you can batch twice as deep, a speculative decoder that emits two tokens per memory pass: each one walks you *down that column*, and the column is dollars.

> **One caveat to keep you honest:** aggregate tokens/s depends on batch size, and batching trades against TPOT. The cost model above is only valid *at a fixed interactivity SLO*. Always quote `$/Mtok` together with the TPOT it was measured at — a cheap token nobody will wait for is not cheap, it is unsold.

**Predicting the denominator: the bandwidth ceiling**

The cost model *measures* tokens/s; you should also be able to **predict** it before touching a GPU. Because decode is memory-bound (§3), every generated token must stream the model's weights (plus its KV slice) from HBM once — so bandwidth divided by bytes is a hard ceiling:

```text
   batch-1 decode ceiling:   tokens/s  ≤  HBM bandwidth  /  bytes streamed per token
                                          (weights touched + KV read for that token)
```

Worked example — a 70B-class dense model in FP16 (~140 GB of weights) on one H100 SXM (3.35 TB/s):

```text
   3350 GB/s ÷ 140 GB  ≈  24 tokens/s    ← the ceiling, batch 1, before any cleverness
```

Now read the whole course against that one formula. Every major technique is an attack on one of its two terms:

* **Quantize FP16 → INT4** → ~140 GB becomes ~35 GB → ceiling ~96 tok/s. *Fewer bytes per token.* (Lecture 7)
* **Batch 32 requests** → the same weight stream is shared by 32 tokens → aggregate ceiling ~32× higher (each request still pays its own KV reads). *More tokens per byte.* This is why batching is the biggest TOK/$ lever — and why the KV cache, which caps batch depth, matters so much. (Lecture 4)
* **Speculative decoding at acceptance τ ≈ 3** → ~3 tokens emitted per pass over the weights → ~3×. (Lecture 6)
* **MoE 671B-total / 37B-active** → only ~37B of weights stream per token at small batch → a 671B model with a 37B-sized ceiling. (Lecture 5)

The formula is also your first line of defense in a design review: if a quoted tokens/s claim sits *above* the bandwidth ceiling for the stated model, precision, and batch size, something unstated is doing the work — batching, speculation, sparsity — and now you know exactly which questions to ask.

---

**7. Mini-lab: place the field on the equation**

A two-part exercise that sets up the whole course.

1. **The cost model.** Take a real model + runtime you can run (or a published benchmark). Measure or read its aggregate tokens/s at a fixed TPOT, and compute `$/Mtok` with §6. Also compute the **bandwidth ceiling** for your model + precision + GPU and report measured tokens/s as a **% of ceiling** — that one ratio tells you how much headroom the rest of this course can still claim. Record TTFT, TPOT, tokens/s, TOK/$, `$/Mtok`, and %-of-ceiling as your baseline row — you will add rungs to this table in every later lecture.
2. **The map.** Write the §2 equation at the top of a page. Under the denominator, list the seven lecture topics of this course and, for each, one sentence on *how* it grows tokens/s. If you cannot write the sentence yet, that lecture is where you'll learn it — but the slot on the equation should be obvious even now.

Deliverable: one baseline cost-model row, and the annotated equation. Keep both; the course is, in a sense, the exercise of filling in that page.

---

</details>

## 关键要点

- 2023–2026 年间，推理成本下降约 **280×**（GPT-3.5 级）和 **>99%**（GPT-4 级），在能力固定的前提下中位数约 **50×/年**——驱动力是 **MLSys，而非更便宜的硅**。
- 支配方程：**price/token = (energy + capital) / tokens-per-second**。硬件决定分子；**MLSys 做大分母**，这就是全部工作。
- 指标分为**面向用户**（TTFT、TPOT、tokens/s、p99）和**面向运维**（TOK/$、TCO/Mtok、perf/watt）。目标是 **SLO 前沿**：在延迟约束下最大化吞吐——绝不是单一数字。
- **prefill（首字前的整段计算）是算力受限；decode（逐 token 生成阶段）是带宽受限。** 多数加速针对 decode，因为长生成的时间与成本都在这里。
- **带宽天花板**——`tokens/s ≤ HBM bandwidth ÷ bytes streamed per token`——能从数据手册预测 batch-1 decode 速度。量化缩小字节数；批处理共享字节；投机每个流发出更多 token；MoE（混合专家模型）只流式加载激活的专家。整门课都是在攻击这一个比值。
- 读 perf/$ benchmark（SemiAnalysis InferenceMAX/InferenceX）要看 **TCO/Mtok**，并把任何印出来的吞吐当作有时效的教学锚点，而非部署真相。
- 一旦能力商品化，**服务成本就是差异化所在**——所以每一项系统层面的胜利都是即时且复利的经济价值。这就是 MLSys 即产品的原因。

---

## 参考文献

- Epoch AI, "LLM inference price trends"（中位数约 50×/年）：[https://epoch.ai/data-insights/llm-inference-price-trends](https://epoch.ai/data-insights/llm-inference-price-trends)
- Token 成本 / AI 价格指数（GPT-3.5 约 280×，GPT-4 >99%）：[https://tokencost.app/blog/ai-price-index](https://tokencost.app/blog/ai-price-index)
- SemiAnalysis, "InferenceMAX — open-source inference benchmarking"：[https://newsletter.semianalysis.com/p/inferencemax-open-source-inference](https://newsletter.semianalysis.com/p/inferencemax-open-source-inference)
- SemiAnalysis InferenceX TCO 计算器：[https://inferencex.semianalysis.com/calculator](https://inferencex.semianalysis.com/calculator)
- Introl, "Inference unit economics — true cost per million tokens"：[https://introl.com/blog/inference-unit-economics-true-cost-per-million-tokens-guide](https://introl.com/blog/inference-unit-economics-true-cost-per-million-tokens-guide)
- *AI Inference Engineer 2026*——本课程配套的生产推理栈读物。

---

## 数据截至

2026-06。成本数据：GPT-3.5 级约 280×（2022 年 11 月→2024 年 10 月），GPT-4 级 >99%（2023→2025），DeepSeek-V3 约 $0.14/Mtok（2024 年 12 月），Epoch 中位数约 50×/年。`$/Mtok` 的算例采用示例性的 $2.50/GPU-hr——**部署时请通过 InferenceMAX/InferenceX 核实实时 GPU 租用价格与 tokens/s**；这些数字每月都在变。

---

*下一讲：[Lecture 02 — The kernel-language explosion](Lecture-02.md)*


<details>
<summary>English original</summary>

**Key takeaways**

- Inference cost fell ~**280×** (GPT-3.5-class) and **>99%** (GPT-4-class) from 2023–2026, a **median ~50×/year** at fixed capability — driven by **MLSys, not cheaper silicon**.
- The governing equation: **price/token = (energy + capital) / tokens-per-second**. Hardware sets the numerator; **MLSys grows the denominator**, and that is the whole job.
- Metrics split into **user-facing** (TTFT, TPOT, tokens/s, p99) and **operator-facing** (TOK/$, TCO/Mtok, perf/watt). The target is the **SLO frontier**: max throughput subject to latency bounds — never a single number.
- **Prefill is compute-bound; decode is memory-bound.** Most acceleration targets decode, because that is where long-generation time and cost live.
- The **bandwidth ceiling** — `tokens/s ≤ HBM bandwidth ÷ bytes streamed per token` — predicts batch-1 decode speed from a datasheet. Quantization shrinks the bytes; batching shares them; speculation emits more tokens per stream; MoE streams only active experts. The whole course is an attack on that one ratio.
- Read perf/$ benchmarks (SemiAnalysis InferenceMAX/InferenceX) by **TCO/Mtok**, and treat any printed throughput as a dated teaching anchor, not deployment truth.
- Once capability commoditizes, **cost-to-serve is the differentiator** — so every systems win is immediate, compounding economic value. That is why MLSys is the product.

---

**References**

- Epoch AI, "LLM inference price trends" (median ~50×/year): [https://epoch.ai/data-insights/llm-inference-price-trends](https://epoch.ai/data-insights/llm-inference-price-trends)
- Token cost / AI price index (GPT-3.5 ~280×, GPT-4 >99%): [https://tokencost.app/blog/ai-price-index](https://tokencost.app/blog/ai-price-index)
- SemiAnalysis, "InferenceMAX — open-source inference benchmarking": [https://newsletter.semianalysis.com/p/inferencemax-open-source-inference](https://newsletter.semianalysis.com/p/inferencemax-open-source-inference)
- SemiAnalysis InferenceX TCO calculator: [https://inferencex.semianalysis.com/calculator](https://inferencex.semianalysis.com/calculator)
- Introl, "Inference unit economics — true cost per million tokens": [https://introl.com/blog/inference-unit-economics-true-cost-per-million-tokens-guide](https://introl.com/blog/inference-unit-economics-true-cost-per-million-tokens-guide)
- *AI Inference Engineer 2026* — the production serving-stack companion to this course.

---

**Current as of**

2026-06. Cost figures: GPT-3.5-class ~280× (Nov 2022→Oct 2024), GPT-4-class >99% (2023→2025), DeepSeek-V3 ~$0.14/Mtok (Dec 2024), Epoch median ~50×/yr. `$/Mtok` worked example uses an illustrative $2.50/GPU-hr — **verify live GPU rental rates and tokens/s at deployment** via InferenceMAX/InferenceX; the numbers move monthly.

---

*Next: [Lecture 02 — The kernel-language explosion](Lecture-02.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/MLSys Deep Dives/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/MLSys%20Deep%20Dives/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
