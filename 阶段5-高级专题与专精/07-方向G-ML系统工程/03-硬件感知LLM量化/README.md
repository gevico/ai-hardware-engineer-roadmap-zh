---
title: 硬件感知的 LLM 量化与推理优化
description: 硬件感知的 LLM 量化与推理优化
---

# 硬件感知的 LLM 量化与推理优化

<div class="course-identity mlsys" markdown="1">
<div class="course-identity__icon">NVFP4</div>
<div markdown="1">
<p class="course-identity__eyebrow">阶段 5 · ML 系统工程 · 研究型工程课程</p>
<p class="course-identity__title">该去掉哪些 bit，从哪些张量上，用什么方法去掉，才能在不改变模型行为的前提下拿到真实的 tok/s？</p>
<p class="course-identity__meta">产物：一个硬件感知的量化策略引擎 + 一份实测消融报告 · 度量：tok/s、bytes/token、相对参考的 KL、投机接受长度</p>
</div>
</div>

> *更小的检查点不等于更快的模型。更快的模型不等于行为未被改变的模型。本课程讲的正是这一区别。*

大多数量化资料回答的是「怎么把文件变小？」对于现代芯片上 decode（逐 token 生成阶段）受限的 LLM 来说，这是个错误的问题。正确的问题由四个部分融合成一个，也正是这整门课程要回答的那句话：

```text
   Which bits should I remove   ──▶  precision allocation, not uniform bit-width
   from which tensors           ──▶  runtime traffic, not parameter count
   using which method           ──▶  calibration matched to the error mode
   to gain real tok/s           ──▶  hardware-native formats, not nominal bits
   without changing behavior?   ──▶  KL and acceptance length, not file size
```

漏掉其中任何一个分句，你交付的就是一次看起来像胜利的退化。一个比 4-bit *更小且更慢* 的 3-bit 模型。一个量化了却在 decode 上零收益的 vision tower。一个换来 2 tok/s、却让你损失四分之一投机接受率的 Q/K 投影。

**目标平台：** NVIDIA Blackwell、GeForce RTX 5090（GB202，`sm_120`）、32 GB GDDR7 @ 1792 GB/s。
**主要案例研究：** 一个约 27 B 的稠密多模态模型，采用 NVFP4 并带 MTP 投机头 —— 全篇依据 [`Qwen3.8-27B-NVFP4-RTX5090`](https://huggingface.co/gittensor-model-hub/Qwen3.8-27B-NVFP4-RTX5090) 与 [`Qwen3.8-27B-DSpark-NVFP4`](https://huggingface.co/gittensor-model-hub/Qwen3.8-27B-DSpark-NVFP4) 已发表的测量结果重建。
**级别：** 资深推理工程师 / 研究工程师。本课程的起点 *高于* 入门级的 PyTorch 量化。

**层级映射：** L4–L6 —— 数值格式层，模型数学、kernel 派发与内存带宽在此交汇。它位于推理服务 runtime 之下、CUDA kernel 之上。

**目标岗位：** 推理系统工程师 · 模型压缩工程师 · GPU Runtime 工程师 · LLM Runtime 优化工程师 · 研究工程师（效率方向）

---

## 前置要求

| 前置要求 | 为什么需要它 |
|---|---|
| [对数概率、困惑度与 KL 散度](../04-Logprobs-困惑度与KL散度/README.md) | 模块 8 假定你已经会读 `H(p,q) = H(p) + D_KL(p‖q)`，并能用平均 KLD 和 top-token 一致度来评估一个量化。本课程 *使用* 这些工具；那门课程 *推导* 它们。 |
| [AI Inference Engineer 2026 — Part 1](../01-AI推理工程师2026/01-基础/README.md) | roofline（性能上界模型）、精度栈、runtime 全景。本课程的模块 1 深入讲某一条具体的 roofline；Part 1 给你通用版本。 |
| [阶段 4 — 量化与低精度推理](../../../阶段4-路线C-深度学习推理优化/04-量化/Guide.md) | PTQ/QAT 术语、per-tensor 与 per-channel scale、TensorRT/ONNX 工具链。那一页是 *通用* 入门；本课程是针对 LLM 与 Blackwell 的研究性处理。 |
| 熟练使用 CUDA | 你必须能读懂 Nsight Compute 报告，并知道带宽受限的 kernel 长什么样。 |

**搭配课程：** [MLSys Deep Dives](../05-MLSys深度探索/README.md)（把投机解码与 kernel 语言层当作系统来看）以及 [AI Inference Engineer 2026 — Part 4](../01-AI推理工程师2026/04-真实引擎优化/README.md)（同样的「先度量」纪律，应用于一台 8× H200 引擎）。

---


<details>
<summary>English original</summary>

**Hardware-Aware LLM Quantization & Inference Optimization**

<div class="course-identity mlsys" markdown="1">
<div class="course-identity__icon">NVFP4</div>
<div markdown="1">
<p class="course-identity__eyebrow">Phase 5 · ML Systems Engineering · Research-Engineering Course</p>
<p class="course-identity__title">Which bits should I remove, from which tensors, using which method, to gain real tok/s without changing model behavior?</p>
<p class="course-identity__meta">Artifact: a hardware-aware quantization policy engine + measured ablation report · Measure: tok/s, bytes/token, KL vs reference, speculative acceptance length</p>
</div>
</div>

> *A smaller checkpoint is not a faster model. A faster model is not a preserved model. This course is about the difference.*

Most quantization material answers "how do I make the file smaller?" That is the wrong question for a decode-bound LLM on modern silicon. The right question has four parts fused into one, and it is the sentence this entire course exists to answer:

```text
   Which bits should I remove   ──▶  precision allocation, not uniform bit-width
   from which tensors           ──▶  runtime traffic, not parameter count
   using which method           ──▶  calibration matched to the error mode
   to gain real tok/s           ──▶  hardware-native formats, not nominal bits
   without changing behavior?   ──▶  KL and acceptance length, not file size
```

Miss any one clause and you ship a regression that looks like a win. A 3-bit model that is *smaller and slower* than a 4-bit one. A vision tower you quantized for zero decode benefit. A Q/K projection that bought 2 tok/s and cost you a quarter of your speculative acceptance rate.

**Target platform:** NVIDIA Blackwell, GeForce RTX 5090 (GB202, `sm_120`), 32 GB GDDR7 @ 1792 GB/s.
**Primary case study:** a ~27 B dense multimodal model in NVFP4 with an MTP speculation head — reconstructed throughout from published measurements of [`Qwen3.8-27B-NVFP4-RTX5090`](https://huggingface.co/gittensor-model-hub/Qwen3.8-27B-NVFP4-RTX5090) and [`Qwen3.8-27B-DSpark-NVFP4`](https://huggingface.co/gittensor-model-hub/Qwen3.8-27B-DSpark-NVFP4).
**Level:** senior inference engineer / research engineer. This course starts *above* introductory PyTorch quantization.

**Layer mapping:** L4–L6 — the numerical-format layer where model math, kernel dispatch, and memory bandwidth meet. It sits below the serving runtime and above the CUDA kernel.

**Role targets:** Inference Systems Engineer · Model-Compression Engineer · GPU Runtime Engineer · LLM Runtime Optimization Engineer · Research Engineer (efficiency)

---

**Prerequisites**

| Prerequisite | Why you need it |
|---|---|
| [Logprobs, Perplexity & KL Divergence](../04-Logprobs-困惑度与KL散度/README.md) | Module 8 assumes you can already read `H(p,q) = H(p) + D_KL(p‖q)` and grade a quant with mean KLD and top-token agreement. This course *uses* those instruments; that course *derives* them. |
| [AI Inference Engineer 2026 — Part 1](../01-AI推理工程师2026/01-基础/README.md) | Roofline, the precision stack, the runtime landscape. Module 1 here goes deeper on one specific roofline; Part 1 gives you the general one. |
| [Phase 4 — Quantization & Low-Precision Inference](../../../阶段4-路线C-深度学习推理优化/04-量化/Guide.md) | PTQ/QAT vocabulary, per-tensor vs per-channel scales, TensorRT/ONNX tooling. That page is the *general* introduction; this course is the LLM-and-Blackwell-specific research treatment. |
| CUDA fluency | You must be able to read a Nsight Compute report and know what a memory-bound kernel looks like. |

**Pairs with:** [MLSys Deep Dives](../05-MLSys深度探索/README.md) (speculative decoding and the kernel-language layer as systems) and [AI Inference Engineer 2026 — Part 4](../01-AI推理工程师2026/04-真实引擎优化/README.md) (the same measure-first discipline applied to an 8× H200 engine).

---

</details>

## 整门课程所系的那个方程

对于带宽受限 GPU 上批大小为 1 的 decode（逐 token 生成阶段）：

```text
                 BW_effective                    ← what the memory system actually delivers
   tok/s  ≈  ──────────────────
                   B_token                       ← bytes that MUST be fetched per generated token
```

每个模块都是用不同方式攻击这两项中的一项，或者证明你在这么做时没有把模型弄坏：

| 模块 | 攻击对象 | 方式 |
|---|---|---|
| 01, 04 | `B_token` | 找出哪些字节真正处在每 token 的关键路径上 |
| 02, 03 | `B_token` + `BW_eff` | 选一种既更小 *又* 能被原生执行的格式 |
| 05, 06, 07 | 行为 | 在模型承受得起的地方去掉比特 |
| 08 | 行为 | 证明你没有把它弄坏 |
| 09 | `B_token` | 随上下文增长的那一项 KV |
| 10 | 方程本身 | 投机解码每次权重读取发出多个 token |
| 11, 12 | 全部 | 最优地分配精度并证明结果 |

---

## 课程地图（12 个模块 + capstone）

<div class="lecture-map" markdown>

| # | 模块 | 主线 |
|---|--------|-----------|
| [01](Lecture-01.md) | **推理物理学** — 算术强度、带宽上限、为什么检查点大小 ≠ 每 token 字节数，以及 `Traffic × Compressibility × HardwareSpeedup × BehaviorTolerance` 机会框架 | 为什么压缩会变成速度 |
| [02](Lecture-02.md) | **量化数学** — BF16 → FP8 E4M3 → NVFP4 E2M1、块缩放、16 元素分组，以及手工把一个激活值沿 NVFP4 网格走一遍 | 数值机制 |
| [03](Lecture-03.md) | **Blackwell 硬件** — `sm_120` 究竟加速了什么、块缩放的 `mma.sync` vs `tcgen05`、各精度的 ridge point，以及为什么 3-bit 可能比 4-bit 更慢 | 硬件的边界 |
| [04](Lecture-04.md) | **模型解剖** — 从其字节重建一个 27 B 检查点、每 token 流量账本，以及那些占用 VRAM 但不占带宽的张量 | 找出真正要紧的字节 |
| [05](Lecture-05.md) | **校准** — absmax vs 百分位 vs MSE 最优截断、AWQ、GPTQ/Hessian 方法、SmoothQuant，以及哪一种对应哪一种误差模式 | 选择缩放因子 |
| [06](Lecture-06.md) | **激活值离群** — 系统性通道离群 vs 罕见 token 尖峰、attention sink、巨量激活值，以及为什么 W4A4 是最难的那个 | 为什么激活值会反击 |
| [07](Lecture-07.md) | **Layer 敏感度** — 解释为什么 Q/K 崩掉而 O/MLP 能存活的 softmax 放大论证、RoPE 相位误差，以及为什么敏感度 ≠ 离群幅度 | 哪些 layer 付得起代价 |
| [08](Lecture-08.md) | **行为保持** — 从 MSE 到 benchmark 的指标阶梯，以及把投机接受长度当作你手头最锐利的廉价行为探针 | 证明你没有弄坏它 |
| [09](Lecture-09.md) | **KV cache 与长上下文** — GQA 的 KV 数学、FP8/FP4 KV、权重流量/KV 流量的交叉点，以及 32 GB 卡上 262 K 上下文的成本账 | 会增长的那一项 |
| [10](Lecture-10.md) | **投机解码** — 拒绝采样接受规则、`τ/(1+K·c)` 模型、MTP 头，以及量化目标模型如何悄悄地给接受率加税 | 把上限乘起来 |
| [11](Lecture-11.md) | **硬件感知 AutoQuant** — 把精度分配当作约束优化、敏感度/流量比，以及带硬件原生护栏的贪心求解器 | 分配算法 |
| [12](Lecture-12.md) | **研究方法论** — 受控消融、锁频、配对比较的纪律，以及那些产出大多数已发表量化结论的无效比较 | 证明某个东西 |
| [13](Lecture-13.md) | **Capstone：TurboQuant** — 构建策略引擎、跑消融网格，在不损失行为的前提下超过 155.75 tok/s @ 2.886 接受率 | 产物 |

</div>

---


<details>
<summary>English original</summary>

**The equation the whole course hangs from**

For batch-1 decode on a bandwidth-bound GPU:

```text
                 BW_effective                    ← what the memory system actually delivers
   tok/s  ≈  ──────────────────
                   B_token                       ← bytes that MUST be fetched per generated token
```

Every module is a different way of attacking one of those two terms, or of proving you did not break the model while doing it:

| Module | Attacks | How |
|---|---|---|
| 01, 04 | `B_token` | find which bytes are actually on the per-token critical path |
| 02, 03 | `B_token` + `BW_eff` | pick a format that is both smaller *and* natively executable |
| 05, 06, 07 | behavior | remove bits where the model can afford it |
| 08 | behavior | prove you did not break it |
| 09 | `B_token` | the KV term that grows with context |
| 10 | the equation itself | speculation emits multiple tokens per weight-read |
| 11, 12 | all of it | allocate precision optimally and prove the result |

---

**Course Map (12 modules + capstone)**

<div class="lecture-map" markdown>

| # | Module | The thread |
|---|--------|-----------|
| [01](Lecture-01.md) | **Inference Physics** — arithmetic intensity, the bandwidth ceiling, why checkpoint size ≠ bytes/token, and the `Traffic × Compressibility × HardwareSpeedup × BehaviorTolerance` opportunity framework | why compression becomes speed |
| [02](Lecture-02.md) | **Quantization Mathematics** — BF16 → FP8 E4M3 → NVFP4 E2M1, block scaling, the 16-element group, and one activation walked through the NVFP4 grid by hand | the numerical machinery |
| [03](Lecture-03.md) | **Blackwell Hardware** — what `sm_120` actually accelerates, block-scaled `mma.sync` vs `tcgen05`, ridge points per precision, and why 3-bit can be slower than 4-bit | the hardware boundary |
| [04](Lecture-04.md) | **Model Anatomy** — reconstructing a 27 B checkpoint from its bytes, the per-token traffic ledger, and the tensors that cost VRAM but not bandwidth | find the bytes that matter |
| [05](Lecture-05.md) | **Calibration** — absmax vs percentile vs MSE-optimal clipping, AWQ, GPTQ/Hessian methods, SmoothQuant, and which one matches which error mode | choosing the scales |
| [06](Lecture-06.md) | **Activation Outliers** — systematic channel outliers vs rare token spikes, attention sinks, massive activations, and why W4A4 is the hard one | why activations fight back |
| [07](Lecture-07.md) | **Layer Sensitivity** — the softmax amplification argument for why Q/K break while O/MLP survive, RoPE phase error, and why sensitivity ≠ outlier magnitude | which layers can pay |
| [08](Lecture-08.md) | **Behavior Preservation** — the metric ladder from MSE to benchmarks, and speculative acceptance length as the sharpest cheap behavioral probe you have | proving you didn't break it |
| [09](Lecture-09.md) | **KV Cache & Long Context** — GQA KV math, FP8/FP4 KV, the weight-traffic/KV-traffic crossover, and 262 K-context economics on a 32 GB card | the term that grows |
| [10](Lecture-10.md) | **Speculative Decoding** — the rejection-sampling acceptance rule, the `τ/(1+K·c)` model, MTP heads, and how quantizing the target silently taxes acceptance | multiplying the ceiling |
| [11](Lecture-11.md) | **Hardware-Aware AutoQuant** — precision allocation as constrained optimization, the sensitivity/traffic ratio, and a greedy solver with hardware-native guard rails | the allocation algorithm |
| [12](Lecture-12.md) | **Research Methodology** — controlled ablations, clock locking, the paired-comparison discipline, and the invalid comparisons that produce most published quantization claims | proving something |
| [13](Lecture-13.md) | **Capstone: TurboQuant** — build the policy engine, run the ablation grid, and beat 155.75 tok/s @ 2.886 acceptance without losing behavior | the artifact |

</div>

---

</details>

## 课程目标

学完后你应能：

* 仅凭测量结果就能判断一个 decode（逐 token 生成阶段）工作负载是 **带宽受限、算力受限，还是启动受限**——并且在弄清楚之前，拒绝量化任何东西。
* 为任意检查点构建 **per-token byte ledger**，把常驻字节与每 token 读取字节区分开，并解释在典型多模态模型上两者为何相差 20% 或更多。
* 手工 **编码与解码一个 NVFP4 block**，说明其有效 bits-per-weight（4.5，而非 4），并解释为何带 E4M3 scale 的 16 元素分组优于 MXFP4 的 32 元素 E8M0 分组。
* 说出 `sm_120` **原生**执行什么、又通过 **解包为更宽类型** 执行什么，并预测哪些“更小”的格式会更慢。
* 依据 **误差模式** 而非流行度来选择校准方法——clipping 还是 rounding，权重离群值还是激活值离群值。
* 解释 **softmax 放大机制**：它使 Q/K 量化造成不成比例的损害，并说明为何这种损害无法由离群值幅度预测。
* 用 **KL 散度与投机接受长度** 评估量化，并论证接受长度是比困惑度更敏感的仪器。
* 针对给定配置计算 **KV/权重流量交叉上下文长度**，并说明长上下文工作何时会改变优化目标。
* 将跨张量的精度分配视为一项 **约束优化**——在行为预算、VRAM 预算与硬件原生格式约束下最大化 tok/s。
* 运行一个 **其他工程师会信服的消融实验**。

---

## 本课程不是什么

* 不是量化入门。如果需要解释 per-tensor 与 per-channel scale 的区别，请从 [阶段 4 — 量化](../../../阶段4-路线C-深度学习推理优化/04-量化/Guide.md) 开始。
* 不是对每一种已发表方法的综述。GPTQ、AWQ、SmoothQuant 及其同类是作为 **按误差模式选择的工具** 出现的，而不是一次文献巡礼。
* 不是断言更低 bit-width 更好。有若干模块的存在就是为了证明它并非如此。

---

## 时效性 / 刷新纪律

* **不随时间变化：** roofline（性能上界模型）论证、算术强度、softmax 放大机制、拒绝采样接受规则、block-scaling 数学。
* **随硬件变化：** `sm_120` 和 `sm_100` 原生加速哪些内容；ridge point；给定 bit-width 是否有 tensor-core 路径。模块 03 是刷新面。
* **随工具链变化：** TensorRT Model Optimizer、llm-compressor、vLLM/TensorRT-LLM 量化 kernel 覆盖。模块 05 和 11 是刷新面。
* 每个模块都以一条 **`## Current as of`** 注释收尾，区分已定论的数学与 2026 年的工具链。

---

## 达成标准

当你能在陌生的芯片上处理陌生的检查点、且无需查阅任何资料时，即告达标：

* 产出其 per-token byte ledger，并在约 10% 误差内预测其 batch-1 decode 上限。
* 说出应首先量化哪三个张量及其原因——依据流量而非尺寸。
* 说出你会采用的格式，并证明它在该芯片上具有原生 tensor-core 路径。
* 在运行实验 *之前*，以 KL 和接受长度口径说明行为预算。
* 运行消融实验，如实报告，并正确识别出你的加速源于 bug 的那种情况。

如果你能背诵量化算法，却说不出一个 token 实际读取哪些字节，那你拥有的只是词汇。本课程的重点是分配决策。

---

*相关：[对数概率、困惑度与 KL 散度](../04-Logprobs-困惑度与KL散度/README.md) · [AI Inference Engineer 2026](../01-AI推理工程师2026/README.md) · [MLSys Deep Dives](../05-MLSys深度探索/README.md) · [阶段 5 — ML 系统工程指南](../Guide.md)*


<details>
<summary>English original</summary>

**Course Outcomes**

By the end you should be able to:

* Decide, from measurements alone, whether a decode workload is **bandwidth-bound, compute-bound, or launch-bound** — and refuse to quantize anything until you know.
* Build a **per-token byte ledger** for any checkpoint that separates resident bytes from per-token-read bytes, and explain why the two differ by 20 % or more on a typical multimodal model.
* Encode and decode an **NVFP4 block by hand**, state its effective bits-per-weight (4.5, not 4), and explain why the 16-element group with an E4M3 scale beats MXFP4's 32-element E8M0 group.
* Name what `sm_120` executes **natively** and what it executes by **unpacking to a wider type**, and predict which "smaller" formats will be slower.
* Choose a calibration method by **error mode** rather than by popularity — clipping vs rounding, weight outliers vs activation outliers.
* Explain the **softmax-amplification mechanism** that makes Q/K quantization disproportionately damaging, and why that damage is not predicted by outlier magnitude.
* Grade a quantization with **KL divergence and speculative acceptance length**, and defend acceptance length as a more sensitive instrument than perplexity.
* Compute the **KV/weight traffic crossover context length** for a given config and say when long-context work changes the optimization target.
* Allocate precision across tensors as a **constrained optimization** — maximize tok/s subject to a behavior budget, a VRAM budget, and a hardware-native-format constraint.
* Run an **ablation another engineer would believe**.

---

**What this course is not**

* Not an introduction to quantization. Start at [Phase 4 — Quantization](../../../阶段4-路线C-深度学习推理优化/04-量化/Guide.md) if you need per-tensor vs per-channel scales explained.
* Not a survey of every published method. GPTQ, AWQ, SmoothQuant and friends appear as **tools selected by error mode**, not as a literature tour.
* Not a claim that lower bit-width is better. Several modules exist specifically to show it is not.

---

**Currency / Refresh Discipline**

* **Timeless:** the roofline argument, arithmetic intensity, the softmax amplification mechanism, the rejection-sampling acceptance rule, block-scaling mathematics.
* **Moves with hardware:** what `sm_120` and `sm_100` accelerate natively; ridge points; whether a given bit-width has a tensor-core path. Module 03 is the refresh surface.
* **Moves with tooling:** TensorRT Model Optimizer, llm-compressor, vLLM/TensorRT-LLM quantized kernel coverage. Modules 05 and 11 are the refresh surface.
* Every module closes with a **`## Current as of`** note separating settled math from 2026 tooling.

---

**Exit Criteria**

You are done when you can take an unfamiliar checkpoint on unfamiliar silicon and, without looking anything up:

* Produce its per-token byte ledger and predict its batch-1 decode ceiling within ~10 %.
* Say which three tensors to quantize first and why — citing traffic, not size.
* Name the format you would use and prove it has a native tensor-core path on that silicon.
* State the behavior budget in KL and acceptance-length terms *before* running the experiment.
* Run the ablation, report it honestly, and correctly identify the case where your speedup came from a bug.

If you can recite quantization algorithms but cannot say which bytes a token actually reads, you have vocabulary. The point of this course is the allocation decision.

---

*Related: [Logprobs, Perplexity & KL Divergence](../04-Logprobs-困惑度与KL散度/README.md) · [AI Inference Engineer 2026](../01-AI推理工程师2026/README.md) · [MLSys Deep Dives](../05-MLSys深度探索/README.md) · [Phase 5 — ML Systems Engineering Guide](../Guide.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/Hardware-Aware LLM Quantization/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/Hardware-Aware%20LLM%20Quantization/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
