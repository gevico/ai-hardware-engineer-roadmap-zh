---
title: 第 10 讲 - 模型压缩：剪枝、量化、蒸馏
description: 第 10 讲 - 模型压缩：剪枝、量化、蒸馏
---

# 第 10 讲 - 模型压缩：剪枝、量化、蒸馏

**合集：** [Practical Machine Learning (CS329P)](README.md) | **上一讲：** [← 第 09 讲](Lecture-09.md) | **下一讲：** [第 11 讲](Lecture-11.md)

---

这门课里你迄今训练过的每一个模型，几乎肯定都是*过参数化*的。通用逼近定理指出，即便只有单个隐藏层的 MLP 也能逼近任意连续函数——可我们却惯常训练带有数十亿参数的神经网络，去拟合内在复杂度远小于此的问题。我们是有意为之：得益于 SGD 与模型结构之间的相互作用，更大的模型*更容易优化*、泛化也更好（这里的理论仍在活跃发展中）。这份余量是训练期的馈赠。到了部署阶段，它就成了一张账单。

CS329P 把部署框定为一组硬性预算，值得直接引用，因为正是它们构成了本讲存在的全部理由。在生产环境中你会面对：**内存**——你常常要与其他进程共享内存，所以模型拿不到整台机器。**延迟**——有些应用要求实时响应（广告排序、实时字幕与翻译、自动驾驶）。**成本**——性能更强的机器更贵，而你每开通一台都要付钱。还有**能耗**——*计算和访问内存*都会消耗可观的能量，这对电池供电设备是决定性的。幻灯片给出的直白结论：部署大模型很难，而且绝大多数时候，没人会原封不动地部署完整的多十亿参数 Transformer。

**模型压缩**是一组把那些预算买回来的技术：在不（显著）损害预测性能的前提下，减小模型体积与计算开销。三条经典杠杆是**剪枝**（把权重元素置 0，这样既不存储也不计算它们）、**量化**（每个权重用更少的 bit，例如 float32 → int8）和**知识蒸馏**（把大教师学到的东西迁移到小学生身上）。这正是 ML 工程把模型交接给硬件的精确接缝：剪枝和量化只有在*硅片*能跳过零、能跑低位宽运算时才能兑现，而这一约束反过来决定了你选哪种压缩。把本讲视作通向阶段 5 的桥梁——在这里，「把模型做小」变成「让芯片跑快」，也正是 [MLSys Deep Dives](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/README.md) 课程和 [Gemma 4 Edge Deployment](../../../../阶段5-高级专题与专精/03-方向C-边缘AI/04-Gemma-4边缘部署/README.md) 材料以完整硬件深度接续故事的地方。

---

## 学习目标

学完本讲，你应当能够：

1. **从部署预算论证压缩的必要性**——解释过参数化模型为何必须压缩，点名内存 / 延迟 / 成本 / 能耗这几项约束，并说明为什么*内存访问*能耗与计算同样重要。
2. **跑通剪枝→重训练循环**，区分非结构化与结构化（通道/滤波器）剪枝，应用幅度准则，并就稀疏度与准确率的权衡进行推理。
3. **量化一个模型**，从 FP32 降到 INT8 及更低——推导 scale/zero-point 映射，选择对称还是非对称，并在给定准确率预算下选 PTQ 还是 QAT。
4. **把教师蒸馏到学生**，使用带温度的软目标，并解释为何 softmax 尾部的「暗知识」让学生变得可训练。
5. **把每种技术映射到加速它的硬件**——INT8 Tensor Core、2:4 结构化稀疏、受内存带宽约束的 decode——并解释非结构化稀疏为何难以加速。
6. **把 2021 年的图景翻译到 2026 年的 LLM 时代**——INT4/GPTQ/AWQ、NF4+QLoRA、FP8、SmoothQuant、现代蒸馏——并知道该往哪里深入。

---


<details>
<summary>English original</summary>

**Lecture 10 - Model Compression: Pruning, Quantization, Distillation**

**Collection:** [Practical Machine Learning (CS329P)](README.md) | **Previous:** [← Lecture 09](Lecture-09.md) | **Next:** [Lecture 11](Lecture-11.md)

---

Every model you have trained so far in this course has been, almost certainly, *overparameterized*. The universal approximation theorem says even a single-hidden-layer MLP can approximate any continuous function — yet we routinely train networks with billions of parameters to fit problems whose intrinsic complexity is far smaller. We do this on purpose: a larger model is *easier to optimize* and generalizes better, thanks to the interaction of SGD with the model's structure (the theory here is still under active development). The slack is a training-time gift. At deployment it becomes a bill.

CS329P frames deployment as a set of hard budgets, and it is worth quoting them directly because they are the entire reason this lecture exists. In production you face: **Memory** — you often share memory with other processes, so your model does not get the whole machine. **Latency** — some applications require realtime responses (ads ranking, live captioning and translation, self-driving). **Cost** — power machines are more expensive, and you pay for every one you provision. And **Energy** — *both computation and accessing memory* consume significant energy, which is decisive for battery-powered devices. The slide's blunt conclusion: deploying big models is hard, and by far only a little of the time does anyone deploy the full multi-billion-parameter transformer as-is.

**Model compression** is the set of techniques that buy back those budgets: reduce model size and compute cost without (significantly) hurting predictive performance. The three classical levers are **pruning** (set weight elements to 0 so you neither store nor compute them), **quantization** (use fewer bits per weight, e.g. float32 → int8), and **knowledge distillation** (transfer what a big teacher learned into a small student). This is the precise seam where ML engineering hands the model off to hardware: pruning and quantization only pay off if the *silicon* can skip the zeros and run the low-bit math, and that constraint shapes which compression you choose. Treat this lecture as the bridge into Phase 5 — the place where "make the model smaller" becomes "make the chip go faster," and where the [MLSys Deep Dives](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/README.md) course and the [Gemma 4 Edge Deployment](../../../../阶段5-高级专题与专精/03-方向C-边缘AI/04-Gemma-4边缘部署/README.md) material pick up the story at full hardware depth.

---

**Learning objectives**

By the end of this lecture you should be able to:

1. **Justify compression from the deployment budget** — explain why overparameterized models must be compressed, naming the memory / latency / cost / energy constraints and why *memory access* energy matters as much as compute.
2. **Run the prune→retrain loop** and distinguish unstructured from structured (channel/filter) pruning, applying a magnitude criterion and reasoning about the sparsity-vs-accuracy trade-off.
3. **Quantize a model** from FP32 to INT8 and below — derive the scale/zero-point mapping, choose symmetric vs asymmetric, and pick PTQ vs QAT given an accuracy budget.
4. **Distill a teacher into a student** using soft targets with temperature, and explain why "dark knowledge" in the softmax tail makes the student trainable.
5. **Map each technique to the hardware that accelerates it** — INT8 tensor cores, 2:4 structured sparsity, memory-bandwidth-bound decode — and explain why unstructured sparsity is hard to accelerate.
6. **Translate the 2021 picture to the 2026 LLM era** — INT4/GPTQ/AWQ, NF4+QLoRA, FP8, SmoothQuant, modern distillation — and know where to go deeper.

---

</details>

## 1. 为什么要压缩：过参数化撞上部署预算

先从这种不对称性说起。训练只发生一次，在数据中心里，你乐意烧参数，因为参数让优化更容易、泛化更好。推理会发生*数百万乃至数十亿次*，往往在共享的或电池供电的机器上，那些参数在前向传播中每一次都要再付一次代价。训练起来便宜的模型，服务起来很贵。

CS329P 的部署挑战幻灯片列举了你实际要权衡的四项代价：

| 约束 | 幻灯片怎么说 | 为什么致命 |
|---|---|---|
| **Memory** | "often share memory with others" | 你的模型要和 OS、其他租户、KV cache 抢内存。塞不进快内存的权重，每次调用都得从慢内存里取。 |
| **Latency** | "some requires realtime (ads, live captioning/translation, self-driving)" | 40 ms 的 SLA 没有商量余地；错过它的更大模型，无论准确率多高都不可用。 |
| **Cost** | "power machines are more expensive" | 你要按峰值 QPS 来配置资源。模型成本减半，机器规模就可能减半。 |
| **Energy** | "both computation and accessing memory needs a significant amount energy, especially for devices powered by batteries" | 在手机或 Jetson 上，电池才是真正的预算——而搬运字节比计算字节更贵。 |

最后一行是硬件工程师必须内化的，所以单独给它一个提示框。

> **硬件视角：** 幻灯片明确把*内存访问*与计算并列列为一项能耗——而在真实硅片上，内存访问才是主导。一次 32 位浮点乘加大约耗费几皮焦；而为它从片外 DRAM 读取操作数要耗费*数百*皮焦。相比数据搬运，算术几乎免费。这一条事实重组了整个领域：量化之所以有效，主要原因不是 INT8 运算更快（虽然确实更快），而是一个 INT8 权重比一个 FP32 权重*搬运起来小 4 倍*。压缩首先是一场带宽和能耗的博弈。下面每一项技术都请记住这一点。

> **硬件视角——roofline 前瞻。** 一个 kernel 是*算力受限*还是*带宽受限*，决定了哪种压缩有帮助。GPU 上大规模批处理的矩阵乘是算力受限——剪枝/量化算术部分收益大。自回归 LLM 的 *decode*（逐 token 生成阶段；一次一个 token，批大小为 1）是**内存带宽受限**：为了产出一个 token 要重读整个权重矩阵，Tensor Core 大部分时间闲置，每 token 的时间取决于你能多快把权重从 HBM 里流出来。这时，把*权重*缩小（INT8/INT4）是唯一能动延迟的手段。让这件事变得严谨的 roofline 模型（性能上界模型）在 [MLSys Deep Dives](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/README.md) 里。

---

## 2. 剪枝——让权重变稀疏

### 2.1 剪枝→重训练循环

幻灯片给出的高层算法很短，十年来基本没变：

```text
1. Train a network to convergence.
2. Assign each weight element a score (an importance estimate).
3. Set some elements to 0 based on their scores.
4. Fine-tune the model to recover accuracy.
   (optionally repeat 2–4 — iterative pruning)
```

微调这一步不是可有可无的修饰；把权重置零会扰动函数，几轮重训练让幸存下来的权重做出补偿。幻灯片给出的三个旋钮定义了一种剪枝方法：

- **Score** —— 什么样的权重算「重要」。最简单的是**绝对值**（幅值）：小权重贡献小，剪掉即可。更丰富的评分用权重对激活值或对梯度的贡献。评分可以**局部**比较（在层内排序，每层剪掉最低的 k%），也可以**全局**比较（整网一个阈值，让某些层保持稠密、另一些变得非常稀疏）。
- **Scheduling** —— **一次性**剪到目标稀疏度（one-shot），或者**分多次迭代**剪，每轮之间做重训练。迭代更慢，但在同等准确率下能达到更高的稀疏度。
- **Fine-tuning** —— 置零之后，是**复用训练好的权重**（标准且稳健的选择），还是对幸存连接**随机重新初始化**？第二个选项正是 lottery ticket 的来处（见下文）。


<details>
<summary>English original</summary>

**1. Why compress: overparameterization meets the deployment budget**

Start from the asymmetry. Training happens once, in a data center, where you are happy to burn parameters because they make optimization easier and generalization better. Inference happens *millions or billions of times*, often on a shared or battery-powered machine, where every one of those parameters is paid for again on each forward pass. The model that was cheap to train is expensive to serve.

CS329P's deployment-challenges slide enumerates the four costs you are actually trading against:

| Constraint | What the slide says | Why it bites |
|---|---|---|
| **Memory** | "often share memory with others" | Your model competes with the OS, other tenants, the KV cache. Weights that don't fit in fast memory get fetched from slow memory, every call. |
| **Latency** | "some requires realtime (ads, live captioning/translation, self-driving)" | A 40 ms SLA is not negotiable; a bigger model that misses it is unusable regardless of accuracy. |
| **Cost** | "power machines are more expensive" | You provision for peak QPS. Halving model cost can halve your fleet. |
| **Energy** | "both computation and accessing memory needs a significant amount energy, especially for devices powered by batteries" | On a phone or a Jetson, the battery is the real budget — and moving bytes costs more than crunching them. |

That last row is the one hardware engineers must internalize, so it gets its own callout.

> **Hardware lens:** The slide explicitly lists *memory access* alongside computation as an energy cost — and on real silicon, memory access dominates. A 32-bit floating-point multiply-add costs on the order of a picojoule; reading the operands for it from off-chip DRAM costs *hundreds* of picojoules. The arithmetic is nearly free next to the data movement. This single fact reorganizes the whole field: the reason quantization works is not mainly that INT8 math is faster (though it is), it is that an INT8 weight is *4× smaller to move* than an FP32 weight. Compression is, first and foremost, a bandwidth and energy play. Keep that in mind for every technique below.

> **Hardware lens — the roofline preview.** Whether a kernel is *compute-bound* or *memory-bound* decides which compression helps. A large batched matmul on a GPU is compute-bound — pruning/quantizing the math wins. Autoregressive LLM *decode* (one token at a time, batch 1) is **memory-bandwidth-bound**: you re-read the entire weight matrix to produce a single token, the tensor cores sit mostly idle, and time-per-token is set by how fast you can stream weights out of HBM. There, shrinking the *weights* (INT8/INT4) is the only thing that moves latency. The roofline model that makes this rigorous lives in the [MLSys Deep Dives](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/README.md).

---

**2. Pruning — make the weights sparse**

**2.1 The prune→retrain loop**

The high-level algorithm from the slide is short and has stayed essentially unchanged for a decade:

```text
1. Train a network to convergence.
2. Assign each weight element a score (an importance estimate).
3. Set some elements to 0 based on their scores.
4. Fine-tune the model to recover accuracy.
   (optionally repeat 2–4 — iterative pruning)
```

The fine-tune step is not optional polish; zeroing weights perturbs the function, and a few epochs of retraining let the surviving weights compensate. Three knobs from the slide define a pruning method:

- **Score** — what makes a weight "important." Simplest is the **absolute value** (magnitude): small weights contribute little, prune them. Richer scores use a weight's contribution to activations or to gradients. You can compare scores **locally** (rank within a layer, prune the bottom k% of each) or **globally** (one threshold across the whole network, letting some layers stay dense and others go very sparse).
- **Scheduling** — prune **all at once** (one-shot) to a target sparsity, or prune a **fraction iteratively**, retraining between rounds. Iterative is slower but reaches higher sparsity at the same accuracy.
- **Fine-tuning** — after zeroing, do you **re-use the trained weights** (the standard, robust choice) or **randomly re-initialize** the surviving connections? That second option is where the lottery ticket comes in (below).

</details>

### 2.2 非结构化 vs 结构化

对硬件工程师而言，这一区分是整个主题的关键，因此请仔细阅读幻灯片给出的两个定义：

- **非结构化剪枝**把*单个权重*置为 0。在给定稀疏度下它能带来最高的准确率，因为剔除的正是最无用的连接。但它“会导致稀疏矩阵/张量，其计算往往效率更低”——这些零散落各处，毫无规律。
- **结构化剪枝**把*整个单元 / 通道 / 块*置为 0——整个卷积滤波器、权重矩阵的整行、一个 attention head。

```text
Unstructured (90% sparse)          Structured (prune whole channels)
weight matrix:                     weight matrix:
  0  .3  0   0  .1                   .3 .2  0  .1  .4    <- kept
 .2  0   0  .4  0                    .1 .5  0  .2  .3    <- kept
  0  0  .5  0   0                     0  0  0   0   0    <- pruned channel
 .1  0   0  0  .2                    .4 .1  0  .3  .2    <- kept
=> 90% of cells are 0 but           => the zero column is removed entirely;
   in random positions                 the matrix literally gets smaller
```

这张图正是下一个提示框存在的全部原因。

> **硬件视角——为什么非结构化稀疏难以加速。** 稠密矩阵乘对 GPU 而言是最理想的工作负载：完全规整、每条 lane 都在忙碌、操作数以连续块的形式流式输入。若*随机*把 90% 的权重打成零，只会让情况更糟而非更好——现在需要存储索引元数据来标明幸存权重的位置，内存访问从连续变成 gather-scattered，而 tensor cores（它需要稠密分块）也无法利用这种结构。在 GPU 上，一个 90% 稀疏的非结构化 layer 往往比原始的稠密版本运行得*更慢*。除非硬件能够*跳过*这些零，否则它们不会带来任何收益，而要高效跳过，稀疏就必须是**结构化的**。相比之下，结构化剪枝删除整行/整通道，产出真正更小的*稠密*张量，而每个加速器本来就能快速运行它——这正是生产环境中卷积神经网络压缩压倒性地采用通道剪枝的原因。

> **硬件视角——2:4 结构化稀疏（对硬件友好的折中方案）。** NVIDIA Ampere 架构（A100，2020）以及此后每一款数据中心 GPU 都加入了 **Sparse Tensor Cores**，它们接受一种特定的*细粒度结构化*模式：**2:4 稀疏**——每个连续的 4 个权重块中，恰好有 2 个为零。这已经足够规整，硬件只需存储 2 个幸存权重外加一个 2-bit 索引，并以大约 **2× 吞吐**将其送入计算，同时又足够细粒度，能保留非结构化剪枝的大部分准确率。这正是幻灯片中那两类方法所趋近的折中：把稀疏结构化到*刚好*能被硅片利用的程度。（`torch.sparse` + cuSPARSELt，或 TensorRT，会压缩 2:4 模型以真正实现加速。）

### 2.3 结果，以及彩票

幻灯片给出的经验性结论（来自 Blalock 等人，MLSys'20，对该领域的综述）令人清醒，值得直白陈述：

- 剪枝算法**优于随机稀疏化**——选择丢弃*哪些*权重是有讲究的。
- **先训练大模型再剪枝，可能胜过直接训练小模型**——即使事后将其丢弃，过参数化在训练期间仍然有帮助。
- 但剪枝“**带来的收益不如换用更好的架构**”。压缩是收尾手段，不能替代好模型。如果能采用更高效的 backbone，就应先这么做。

**彩票假设**（Lottery Ticket Hypothesis，Frankle 与 Carbin）正是潜藏在微调这一旋钮背后的研究思想：在一个大型已训练网络内部，存在一个稀疏子网络——一张“中奖彩票”——*若将其重置回原始初始化*并单独训练，其准确率能与完整网络匹敌。这暗示稠密网络的部分职责，就是找到那个子网络的幸运初始化。它在思想上很重要，也是一条活跃的研究路线；但它*尚不*是可靠的生产配方，这正是幻灯片默认的微调选择是“复用已训练网络的权重”、而非“随机重新初始化”的原因。

---

## 3. 量化——用更少的比特


<details>
<summary>English original</summary>

**2.2 Unstructured vs structured**

This distinction is the crux of the whole topic for a hardware engineer, so read the slide's two definitions carefully:

- **Unstructured pruning** sets *individual weights* to 0. It gives you the highest accuracy at a given sparsity, because you remove exactly the least-useful connections. But it "leads to sparse matrices/tensors that are often less efficient to compute" — the zeros are scattered with no pattern.
- **Structured pruning** sets a *whole unit / channel / block* to 0 — an entire convolutional filter, a whole row of a weight matrix, an attention head.

```text
Unstructured (90% sparse)          Structured (prune whole channels)
weight matrix:                     weight matrix:
  0  .3  0   0  .1                   .3 .2  0  .1  .4    <- kept
 .2  0   0  .4  0                    .1 .5  0  .2  .3    <- kept
  0  0  .5  0   0                     0  0  0   0   0    <- pruned channel
 .1  0   0  0  .2                    .4 .1  0  .3  .2    <- kept
=> 90% of cells are 0 but           => the zero column is removed entirely;
   in random positions                 the matrix literally gets smaller
```

That picture is the entire reason the next callout exists.

> **Hardware lens — why unstructured sparsity is hard to accelerate.** A dense matmul is the best-case workload for a GPU: perfectly regular, every lane busy, operands streamed in contiguous blocks. Punch 90% of the weights to zero *at random* and you have made it worse, not better — you now store index metadata to say where the survivors are, your memory accesses are gather-scattered instead of contiguous, and the tensor cores (which want dense tiles) cannot use the structure. A 90%-sparse unstructured layer routinely runs *slower* than the dense original on a GPU. The zeros save you nothing unless the hardware can *skip* them, and to skip them efficiently the sparsity must be **structured**. Structured pruning, by contrast, deletes whole rows/channels and yields a genuinely smaller *dense* tensor that every accelerator already runs fast — which is why production CNN compression overwhelmingly uses channel pruning.

> **Hardware lens — 2:4 structured sparsity (the hardware-friendly middle ground).** NVIDIA Ampere (A100, 2020) and every datacenter GPU since added **Sparse Tensor Cores** that accept a specific *fine-grained structured* pattern: **2:4 sparsity** — in every contiguous block of 4 weights, exactly 2 are zero. This is regular enough that the hardware stores only the 2 survivors plus a 2-bit index and feeds them through at roughly **2× throughput**, while being fine-grained enough to keep most of the accuracy of unstructured pruning. It is the compromise the slide's two categories were converging toward: structure the sparsity *just enough* that silicon can exploit it. (`torch.sparse` + cuSPARSELt, or TensorRT, will compress a 2:4 model to actually realize the speedup.)

**2.3 Results, and the lottery ticket**

The slide's empirical takeaways (from Blalock et al., MLSys'20, surveying the field) are sobering and worth stating plainly:

- Pruning algorithms **outperform random sparsification** — choosing *which* weights to drop matters.
- **Train-big-then-prune can beat training the small model directly** — the overparameterization helps during training even if you throw it away after.
- But pruning "**does not help as much as switching to a better architecture**." Compression is a finishing move, not a substitute for a good model. If you can use a more efficient backbone, do that first.

The **Lottery Ticket Hypothesis** (Frankle & Carbin) is the research idea lurking behind the fine-tuning knob: inside a large trained network there exists a sparse subnetwork — a "winning ticket" — that, *if reset to its original initialization* and trained in isolation, matches the full network's accuracy. It suggests the dense network's job was partly to find that subnetwork's lucky initialization. It is intellectually important and an active research line; it is *not yet* a reliable production recipe, which is exactly why the slide's default fine-tuning choice is "re-use weights from the trained network," not "randomly re-initialize."

---

**3. Quantization — use fewer bits**

</details>

### 3.1 为什么要用低位宽，以及奖励低位宽的硬件

量化用低位宽数值来加速推理（有时也加速训练）：内存占用更少，还能走特殊的硬件通路。它并不会太损伤准确率，*除非*在极低位宽（1–2 bit）下，此时准确率会断崖式下跌。该幻灯片用原始吞吐数字来论证这一点 —— 现代硬件在低位宽运算上就是要快得多。在 NVIDIA A100 上：

| 数据类型 | A100 吞吐 | 备注 |
|---|---|---|
| IEEE float32 (FP32) | 19.5 TFLOPS | 基线 |
| TF32 | 156 TFLOPS | 内部 19-bit，FP32 范围 |
| IEEE float16 (FP16) | 312 TFLOPS | 半精度 |
| Bfloat16 (BF16) | 312 TFLOPS | FP32 指数范围，尾数位更少 |
| **INT8** | **624 TOPS** | 2× FP16 |
| **INT4** | **1248 TOPS** | 2× INT8 |

规律是：位宽每减半，算术吞吐大致翻倍，*同时*搬运的字节数减半。幻灯片给出两条实用规则：**训练用浮点，推理用整数**（梯度需要的动态范围比前向传播更大），以及**只量化重负载的 layer** —— conv 和 dense —— 而把激活值、权重更新之类留在默认的 FP32。要在 FLOPs 和字节数集中的地方做量化，而不是到处都量化。

> **硬件视角 —— INT8 张量核心是主力。** 那个 624-TOPS 的 INT8 数字并不是小众路径；它是量产推理加速器的看家本领。TensorRT、ONNX Runtime、TFLite，以及每一个边缘 NPU/DSP 都以 INT8 稠密矩阵乘为核心。相对 FP32 的 4× 内存缩减，正是让模型能塞进手机缓存或 Jetson 有限 DRAM 的原因；相对 FP16 的 2× 算力则是附赠的好处。当有人说“我们把模型量化后部署”时，未加说明的默认所指就是 INT8。

### 3.2 两大流派：低位宽浮点 vs 整数

**低位宽浮点**（FP16、BF16、TF32）。把 FP32 向下转换“很直接 —— 裁掉小数/指数位”。问题在于*指数*位，它决定动态范围。FP16 的*指数位比 FP32 少*，因此小的激活值和梯度可能下溢为精确的 0。幻灯片给出的解法是 **loss scaling**：训练时使用 `λ·loss(ŷ, y)`，配一个可调的 `λ > 0`，它把激活值和梯度放大 `λ×` 倍，让接近零的数值能在 FP16 下存活；在优化器步进前再缩回来。BF16 则绕开了这个问题，它保留 FP32 的指数范围（代价是牺牲尾数精度），这也是它成为大模型默认训练类型的原因。

**整数量化**是更激进、面向推理时的路径。幻灯片给出的最简单方案：

```text
XY  ≈  σx·σy · clip(round(X / σx)) · clip(round(Y / σy))
                └──────────── integer matrix multiply ────────────┘

X, Y    : the FP32 weight / activation matrices
σx, σy  : per-tensor scales, computed from the data in X (and Y)
round   : map the scaled real value to the nearest integer
clip    : saturate into the integer range, e.g. [-128, 127] for INT8
```

具体而言，实数值 `r` 与其量化后的整数 `q` 之间的仿射映射为：

```text
q = round(r / scale) + zero_point        (quantize)
r ≈ scale · (q − zero_point)             (dequantize)

scale       = (r_max − r_min) / (q_max − q_min)     # FP32, the step size
zero_point  = integer that the real value 0 maps to # keeps 0 exactly representable
```

- **对称**量化固定 `zero_point = 0`，并使用诸如 `[−127, 127]` 的范围。它更便宜（零点项在矩阵乘中被消掉），也是*权重*的标准做法，因为权重大致以零为中心。
- **非对称**量化让 `zero_point ≠ 0`，以适配不居中的范围 —— 这是 **ReLU 之后的激活值**的自然选择，因为它们全都 ≥ 0，把一半整数范围花在负数上会浪费分辨率。


<details>
<summary>English original</summary>

**3.1 Why low bits, and the hardware that rewards them**

Quantization uses low-bit numbers to accelerate inference (and sometimes training): less memory, and access to special hardware paths. It does not hurt accuracy much *except* at extremely low bit-widths (1–2 bit), where it can fall off a cliff. The slide motivates it with raw throughput numbers — modern hardware is simply much faster at low-bit ops. On an NVIDIA A100:

| Data type | A100 throughput | Note |
|---|---|---|
| IEEE float32 (FP32) | 19.5 TFLOPS | the baseline |
| TF32 | 156 TFLOPS | 19-bit internal, FP32-range |
| IEEE float16 (FP16) | 312 TFLOPS | half precision |
| Bfloat16 (BF16) | 312 TFLOPS | FP32 exponent range, fewer mantissa bits |
| **INT8** | **624 TOPS** | 2× FP16 |
| **INT4** | **1248 TOPS** | 2× INT8 |

The pattern: each halving of bit-width roughly doubles arithmetic throughput *and* halves the bytes you move. Two practical rules from the slide: use **floats for training, integers for inference** (gradients need a larger dynamic range than a forward pass does), and **only quantize the heavy layers** — conv and dense — while leaving activations, weight updates, and the like in the default FP32. You quantize where the FLOPs and bytes are, not everywhere.

> **Hardware lens — INT8 tensor cores are the workhorse.** That 624-TOPS INT8 number is not a niche path; it is the bread and butter of production inference accelerators. TensorRT, ONNX Runtime, TFLite, and every edge NPU/DSP center on INT8 dense matmul. The 4× memory shrink vs FP32 is what gets a model into a phone's cache or a Jetson's limited DRAM; the 2× compute over FP16 is the bonus. When someone says "we quantized the model for deployment," the unmarked default they mean is INT8.

**3.2 Two families: low-bit floats vs integers**

**Low-bit floating-point** (FP16, BF16, TF32). Casting FP32 down is "straightforward — trim the fraction/exponent bits." The catch is *exponent* bits, which set the dynamic range. FP16 has *fewer exponent bits* than FP32, so small activations and gradients can underflow to exactly 0. The fix the slide gives is **loss scaling**: train with `λ·loss(ŷ, y)` for a tunable `λ > 0`, which scales activations and gradients up by `λ×` so values near zero survive FP16; you unscale before the optimizer step. BF16 sidesteps this by keeping FP32's exponent range (trading away mantissa precision instead), which is why it became the default training type for large models.

**Integer quantization** is the more aggressive, inference-time path. The simplest scheme the slide gives:

```text
XY  ≈  σx·σy · clip(round(X / σx)) · clip(round(Y / σy))
                └──────────── integer matrix multiply ────────────┘

X, Y    : the FP32 weight / activation matrices
σx, σy  : per-tensor scales, computed from the data in X (and Y)
round   : map the scaled real value to the nearest integer
clip    : saturate into the integer range, e.g. [-128, 127] for INT8
```

Concretely, the affine mapping between a real value `r` and its quantized integer `q` is:

```text
q = round(r / scale) + zero_point        (quantize)
r ≈ scale · (q − zero_point)             (dequantize)

scale       = (r_max − r_min) / (q_max − q_min)     # FP32, the step size
zero_point  = integer that the real value 0 maps to # keeps 0 exactly representable
```

- **Symmetric** quantization fixes `zero_point = 0` and uses a range like `[−127, 127]`. It is cheaper (the zero-point term drops out of the matmul) and is the standard for *weights*, which are roughly zero-centered.
- **Asymmetric** quantization lets `zero_point ≠ 0` to fit a range that isn't centered — the natural choice for **activations after a ReLU**, which are all ≥ 0, so spending half the integer range on negatives would waste resolution.

</details>

### 3.3 PTQ vs QAT、校准，以及准确率死在哪里

幻灯片警告：“直接量化训练好的权重可能降低准确率。”由此分出两种范式：

- **训练后量化（PTQ）**——取一个训练完成的 FP32 模型，不做进一步训练直接量化。为选取 scale，要让几百条代表性输入跑过模型并记录观测到的范围——这就是**校准**。快（分钟级）、对数据需求小，但损失更大。
- **量化感知训练（QAT）**——幻灯片的说法是：“在训练中执行 clip/round，但保持 float32。”你在前向传播中模拟舍入/截断（一个 “fake-quant” 算子），同时保留权重的全精度主副本，并让梯度穿透过去（通过 straight-through estimator）。网络*学会对自身的量化保持鲁棒*，以一次训练为代价找回大部分丢失的准确率。

```python
# Sketch of the QAT idea: round in the forward pass, but keep FP32 weights
# and pass gradients straight through the non-differentiable round().
def fake_quant(x, scale, zero_point, qmin=-128, qmax=127):
    q = torch.clamp(torch.round(x / scale) + zero_point, qmin, qmax)
    x_hat = (q - zero_point) * scale          # de-quantized approximation
    # straight-through estimator: forward uses x_hat, backward uses dL/dx
    return x + (x_hat - x).detach()
```

**准确率究竟丢在哪里？** 在**离群值**上。一个权重张量，或者更常见的是一个激活值张量，其取值大多很小、却夹着少数极端元素，这就迫使 `scale` 大到足以覆盖这些极值——从而压碎留给众多小数值的分辨率，而大部分信号恰恰就住在这些小数值里。少数大数偷走了动态范围。正是这一种失效模式，驱动了下面 2026 更新中几乎每一项现代进展。

> **硬件视角——为了拿到 “INT8 推理”，你真正要付出什么代价。** 吞吐是白送的；*准确率*才是要下功夫的工程。per-tensor scale 对 kernel 最便宜，但准确率最低；**per-channel**（权重矩阵每个输出通道一个 scale）是标准的量产折中——准确率好得多，且仍是干净的矩阵乘。硬件以 INT8 做乘累加，但**以 INT32 累加**以避免溢出，随后再把结果 requantize 回低位。知道累加器位宽和 requantize 步骤处在什么位置，就是模型达到准确率目标与模型悄悄劣化之间的差别。

---

## 4. 知识蒸馏——训练一个小模型去模仿大模型

第三个杠杆不去碰固定模型的权重；它训练一个*不同的、更小的*模型，使其行为像大模型。幻灯片的例子横跨整个 ML：Random forest → decision tree；ResNet-152 → ResNet-34；BERT-Base → BERT-mini。学生模型“比直接训练更好”，因为教师模型以一种比原始数据更易拟合的形式告诉它*自己学到了什么*，并实际上用伪标签对数据做了增强。

### 4.1 函数逼近视角（为什么它竟然行得通）

CS329P 把理论讲得很干净。教师 `f` 是在有限数据集 `Dₙ` 上通过经验风险最小化学到的，该数据集由从真实分布 `p` 中采样的 `n` 个点组成。随后我们学一个学生 `g`，使其在某个距离 `d` 下接近 `f`：

```text
ℱ(f, g, Dₙ) = (1/n) Σᵢ d( f(xᵢ), g(xᵢ) )
```

这里有个微妙而重要的点：只在 `Dₙ` 上训练学生，意味着我们**为采样的统计误差付了两次钱**——一次是 `f` 从 `Dₙ` 学习时，另一次是 `g` 在*同一批* `n` 个点上从 `f` 蒸馏时。但教师 `f` 是一个*可以在任意处查询的函数*，而不只是在 `n` 这些带标签的点上。所以**从满足 `m ≫ n` 的分布 `q` 中采样出一个代理集 `D′ₘ`**，用教师模型给它打标签，再在其上蒸馏。幻灯片给出的泛化界把这些权衡讲得很明确：

```text
ℱ(f, g*, p)  ≤  ℱ(f, g*, D′ₘ)  +  √((V − log δ)/m)  +  ‖p − q‖₁
 (generalized       (training        (shrinks as m         (q should be
    error)            error)        grows — use lots         close to p)
                                      of surrogate data)
```

解读是：更多代理数据（`m`）缩小中间项，而接近真实 `p` 的代理分布 `q` 缩小最后一项。这就是*为什么*蒸馏比直接训练小模型泛化更好——你通过用教师标注的数据做增强，跳出了那 `n` 个点的样本。具体地，你通过**增强**来生成 `q`：表格数据用 Gibbs 采样特征（FAST-DAD），图像用常见的图像增强，文本则用预训练 BERT 填充被 mask 的 token，外加回译和 mixup。


<details>
<summary>English original</summary>

**3.3 PTQ vs QAT, calibration, and where accuracy dies**

The slide warns: "directly quantizing the trained weights may decrease accuracy." That gives the two regimes:

- **Post-Training Quantization (PTQ)** — take a finished FP32 model and quantize it with no further training. To pick the scales you run a few hundred representative inputs through the model and record the observed ranges — this is **calibration**. Fast (minutes) and data-light, but lossier.
- **Quantization-Aware Training (QAT)** — the slide's phrasing: "performs clip/round during training, but keeps float32." You simulate the rounding/clipping in the forward pass (a "fake-quant" op) while keeping a full-precision master copy of the weights and letting gradients flow through (via a straight-through estimator). The network *learns to be robust* to its own quantization, recovering most of the lost accuracy at the price of a training run.

```python
# Sketch of the QAT idea: round in the forward pass, but keep FP32 weights
# and pass gradients straight through the non-differentiable round().
def fake_quant(x, scale, zero_point, qmin=-128, qmax=127):
    q = torch.clamp(torch.round(x / scale) + zero_point, qmin, qmax)
    x_hat = (q - zero_point) * scale          # de-quantized approximation
    # straight-through estimator: forward uses x_hat, backward uses dL/dx
    return x + (x_hat - x).detach()
```

**Where does accuracy actually get lost?** In the **outliers**. A weight or (more often) activation tensor whose values are mostly small but with a few extreme entries forces `scale` to be large enough to cover the extremes — which crushes the resolution available to the many small values, where most of the signal lives. The few big numbers steal the dynamic range. This single failure mode is what motivates almost every modern advance in the 2026 update below.

> **Hardware lens — what "INT8 inference" really costs you to get.** The throughput is free; the *accuracy* is the engineering. Per-tensor scales are cheapest for the kernel but least accurate; **per-channel** (a scale per output channel of a weight matrix) is the standard production trade-off — far better accuracy, still a clean matmul. The hardware multiply-accumulates in INT8 but **accumulates in INT32** to avoid overflow, then requantizes the result back down. Knowing where the accumulator width and the requantize step sit is the difference between a model that hits its accuracy target and one that silently degrades.

---

**4. Knowledge distillation — train a small model to imitate a big one**

The third lever does not touch the weights of a fixed model; it trains a *different, smaller* model to behave like the big one. The slide's examples span all of ML: Random forest → decision tree; ResNet-152 → ResNet-34; BERT-Base → BERT-mini. The student is "better than training directly" because the teacher tells it *what it learned* in a form easier to fit than the raw data, and effectively augments the data with pseudo-labels.

**4.1 The function-approximation view (why it can work at all)**

CS329P gives the theory cleanly. The teacher `f` was learned by empirical risk minimization on a finite dataset `Dₙ` of `n` points sampled from the true distribution `p`. We then learn a student `g` to be close to `f` under some distance `d`:

```text
ℱ(f, g, Dₙ) = (1/n) Σᵢ d( f(xᵢ), g(xᵢ) )
```

Here is the subtle, important point: training the student only on `Dₙ` means we **pay twice for the statistical error of sampling** — once when `f` learned from `Dₙ`, again when `g` distills from `f` on the *same* `n` points. But the teacher `f` is a *function we can query anywhere*, not just on the `n` labeled points. So **sample a surrogate set `D′ₘ` from a distribution `q` with `m ≫ n`**, label it with the teacher, and distill on that. The generalization bound the slide shows makes the trade-offs explicit:

```text
ℱ(f, g*, p)  ≤  ℱ(f, g*, D′ₘ)  +  √((V − log δ)/m)  +  ‖p − q‖₁
 (generalized       (training        (shrinks as m         (q should be
    error)            error)        grows — use lots         close to p)
                                      of surrogate data)
```

The reading: more surrogate data (`m`) shrinks the middle term, and a surrogate distribution `q` close to the real `p` shrinks the last. This is *why* distillation generalizes better than training the small model directly — you have escaped the `n`-point sample by augmenting with teacher-labeled data. Concretely you generate `q` by **augmentation**: Gibbs-sampling features for tabular (FAST-DAD), the usual image augmentations, and for text, using a pretrained BERT to fill masked tokens, plus back-translation and mixup.

</details>

### 4.2 软目标与温度 —— “暗知识”

让蒸馏不止于重新打标签的机制在于：**负类的 softmax 输出携带了硬标签所没有的信息。** 给 teacher 看一张狗的照片，它可能输出 `{dog: 0.9, wolf: 0.08, cat: 0.015, car: 0.005}`。one-hot 标签只说了“狗”；teacher 还额外说了*“这看起来有点像狼，完全不像车。”*这种关于错误答案的关系结构 —— Hinton 的**“暗知识”** —— 是远比单个 1 更丰富的训练信号。

为了把它暴露出来，我们在 softmax 中用**温度** `T` 软化分布：

```text
                exp(xᵢ / T)
S_T(x)ᵢ  =  ─────────────────────
              Σⱼ exp(xⱼ / T)
```

`T` 越大，分布越平坦，负类上的小概率被放大，使 student 能真正看到并拟合它们。幻灯片给出的蒸馏损失，既把 student 的*软*输出与 teacher 的对齐，也把 student 的硬输出与真实标签对齐：

```text
  CE( S_T(g(x)), S_T(f(x)) )   +   λ · CE( S₁(g(x)), y )
  └──── soft term (temperature T) ────┘     └─── normal classification ───┘
```

幻灯片补充两点：当 `T → ∞` 时，软项趋近 `MSE(g(x), f(x))`（匹配原始 logits）；而尽管理论如此，实践中 **`T = 1` 往往效果很好** —— 先做简单的。

### 4.3 不止于输出：中间表示，以及蒸馏集成模型

不必只匹配最后一层。隐藏激活值携带更丰富的信息，因此可以把 **student 的层与 teacher 的层对齐**（若宽度不同，加一个小型稠密投影），用 MSE / L2 / 甚至可学习的损失。**TinyBERT** 正是这样构建的：幻灯片报告了把 BERT-base（110M 参数）蒸馏为 TinyBERT（14M）—— 约 8× 的压缩 —— 并保留了 GLUE 的大部分成绩（BERT-base 平均 79.6 → TinyBERT 75.1），消融实验显示中间表示匹配、层匹配策略、输出损失与数据增强各自都有贡献。

> **回扣 Lecture 05。** 蒸馏最优雅的用法是与**集成（ensembling）**（Lecture 05）形成闭环。集成能提升准确率，但推理成本会按成员数成倍增加 —— 这正是本讲所要对抗的部署成本。蒸馏让你可以**先训练一个强集成（或一套很重的 AutoGluon stack），再把它蒸馏成单个小网络**，以单个模型的成本继承集成的大部分准确率。teacher 可以是多个模型的*组合*；student 是一个廉价的网络。AutoGluon 中的 `predictor.distill()` 对表格数据正是这么做的。**自蒸馏** —— student 与 teacher 共享同一架构，student 用 teacher 的软标签训练 —— 是退化却出奇有效的特例。

---

## 5. 选择一种技术 —— 三根杠杆并排比较

三者是**互补**的，而非竞争关系：一个已部署的模型常常同时被蒸馏*和*剪枝*和*量化。但它们的权衡方式各不相同：

| | **剪枝** | **量化** | **蒸馏** |
|---|---|---|---|
| **改变了什么** | 把权重置为 0（删除连接/通道） | 每个数用更少的比特（FP32→INT8/4） | 训练一个*新的、更小的*模型来模仿 teacher |
| **节省了什么** | 模型大小；仅当稀疏可被利用时才省计算 | 内存带宽 + 存储（INT8 下约 4×）**以及**计算 | 全部 —— student 是真正更小的稠密模型 |
| **准确率风险** | 中等稀疏度下低；高非结构化稀疏度下断崖式下跌 | INT8 下低；低于约 4-bit 后急剧上升，由离群值驱动 | 通常很小；有足够代理数据时 student 可逼近 teacher |
| **需要的硬件支持** | 需要**结构化 / 2:4** 稀疏张量核心才能获得真实加速；非结构化在 GPU 上很少有用 | 低位张量核心（INT8/INT4/FP8）—— 在现代加速器上普遍可用 | **无需特殊支持** —— 输出是标准的小型稠密网络，随处可跑 |
| **何时需要训练** | 是 —— 剪枝后微调 | PTQ：否（仅需校准）；QAT：是 | 是 —— 训练 student |
| **最佳适用场景** | 通道冗余（CNN）；你有支持 2:4 的硬件 | 受内存/带宽限制（LLM decode、边缘） | 你想要更小的*架构*，或想压缩集成 |

硬件这一列是工程师的决策准则：**蒸馏和 INT8 量化是可移植的收益**（任何设备都能受益），而**剪枝的回报取决于硅片** —— 只有当你能以结构化/2:4 稀疏硬件或稀疏感知 runtime 为目标时，它才值得做。

---


<details>
<summary>English original</summary>

**4.2 Soft targets and temperature — the "dark knowledge"**

The mechanism that makes distillation more than relabeling: **the softmax outputs of the negative classes carry information that hard labels do not.** A teacher shown a photo of a dog might output `{dog: 0.9, wolf: 0.08, cat: 0.015, car: 0.005}`. The one-hot label says only "dog"; the teacher additionally says *"this looks somewhat like a wolf, not at all like a car."* That relational structure over the wrong answers — Hinton's **"dark knowledge"** — is a far richer training signal than a single 1.

To expose it we soften the distribution with a **temperature** `T` in the softmax:

```text
                exp(xᵢ / T)
S_T(x)ᵢ  =  ─────────────────────
              Σⱼ exp(xⱼ / T)
```

A larger `T` flattens the distribution, amplifying the small probabilities on the negative classes so the student can actually see and fit them. The distillation loss the slide gives matches the student's *soft* outputs to the teacher's *and* the student's hard outputs to the true label:

```text
  CE( S_T(g(x)), S_T(f(x)) )   +   λ · CE( S₁(g(x)), y )
  └──── soft term (temperature T) ────┘     └─── normal classification ───┘
```

Two notes the slide adds: as `T → ∞` the soft term approaches `MSE(g(x), f(x))` (matching raw logits), and despite the theory, **`T = 1` often works well** in practice — start simple.

**4.3 Beyond outputs: intermediate representations, and distilling ensembles**

You need not match only the final layer. Hidden activations carry richer information, so you can **match student layers to teacher layers** (add a small dense projection if their widths differ), with an MSE / L2 / even-learned loss. This is exactly how **TinyBERT** is built: the slide reports distilling BERT-base (110M params) into TinyBERT (14M) — an ~8× shrink — keeping most of GLUE (BERT-base 79.6 → TinyBERT 75.1 average), with the ablation showing intermediate-representation matching, layer-matching strategy, output loss, and data augmentation each contributing.

> **Tie-back to Lecture 05.** The most elegant use of distillation closes the loop with **ensembling** (Lecture 05). Ensembles win accuracy but multiply inference cost by the number of members — exactly the deployment cost this lecture is fighting. Distillation lets you **train a strong ensemble (or a heavy AutoGluon stack), then distill it into a single small network** that inherits much of the ensemble's accuracy at one model's cost. The teacher can be a *combination* of models; the student is one cheap net. `predictor.distill()` in AutoGluon does precisely this for tabular. **Self-distillation** — student and teacher share an architecture, the student trained on the teacher's soft labels — is the degenerate, surprisingly effective special case.

---

**5. Choosing a technique — the three levers side by side**

The three are **complementary**, not competing: a deployed model is frequently distilled *and* pruned *and* quantized. But they trade off differently:

| | **Pruning** | **Quantization** | **Distillation** |
|---|---|---|---|
| **What it changes** | Sets weights to 0 (removes connections/channels) | Fewer bits per number (FP32→INT8/4) | Trains a *new, smaller* model to imitate a teacher |
| **What it saves** | Model size; compute *only if* the sparsity is exploitable | Memory bandwidth + storage (≈4× at INT8) **and** compute | Everything — student is a genuinely smaller dense model |
| **Accuracy risk** | Low at moderate sparsity; cliff at high unstructured sparsity | Low at INT8; rises sharply below ~4-bit, driven by outliers | Usually small; student can approach teacher with enough surrogate data |
| **Hardware support needed** | **Structured / 2:4** sparse tensor cores to get real speedup; unstructured rarely helps on GPU | Low-bit tensor cores (INT8/INT4/FP8) — ubiquitous on modern accelerators | **None special** — output is a standard small dense net, runs anywhere |
| **When training is required** | Yes — fine-tune after pruning | PTQ: no (just calibration); QAT: yes | Yes — train the student |
| **Best when** | Channels are redundant (CNNs); you have 2:4-capable HW | You are memory/bandwidth-bound (LLM decode, edge) | You want a smaller *architecture*, or to collapse an ensemble |

The hardware column is the engineer's decision rule: **distillation and INT8 quantization are portable wins** (any device benefits), while **pruning's payoff is gated on the silicon** — only worth it when you can target structured/2:4 sparsity hardware or a sparse-aware runtime.

---

</details>

## 6. 2026 更新 — 模型压缩的 LLM 时代

> **2026 更新 —— 原始思想，映射到当下的技术栈上。** CS329P 在 2021 年的表述*完全正确*，至今仍是基础 —— 但重心已从 CNN 转移到 LLM，后者在 decode（逐 token 生成阶段）时受内存带宽约束（第 1 节），压缩不再是可选项。先学会上面的原始方法；下面是它们各自的去向。
>
> **量化下探到 INT8 以下，并具备了离群值感知能力。** 幻灯片“极低比特会损害准确率”的警告*对朴素的 PTQ* 而言是对的 —— 该领域通过直接处理第 3.3 节的离群值跨越了它：
>
> - **GPTQ** 和 **AWQ** 是*仅权重*的 INT4 PTQ 方法，能把 LLM 量化到 4 bit 且质量接近 FP16。GPTQ 利用二阶（Hessian）信息逐层补偿舍入误差；AWQ（“激活值感知”）按激活值幅度缩放权重通道，使*重要*通道保持精度。两者都让 INT4 成为 7B–70B 模型推理服务的生产默认。
> - **SmoothQuant** 是激活值离群问题的直接答案：它通过逐通道缩放把难点从激活值*迁移*到权重中，从而两者都能量化到 INT8 —— 实现真正的 INT8 *激活值*量化（W8A8），而不只是权重。
> - **NF4 + QLoRA** 把冻结的基座模型量化成 4-bit“NormalFloat”类型，并在其上训练微小的 LoRA 适配器，在*单张 GPU* 上微调 65B 模型。这是 QAT 的精神（训练时感知量化）在 LLM 规模上变得实用。
> - **FP8** 是 **Hopper (H100)** 和 **Blackwell (B100/B200)** 上新的训练/推理类型 —— 一种 8-bit *浮点*（E4M3 / E5M2），保留了可用的指数范围，因此在*激活值*上比 INT8 更能容忍离群值。Blackwell 进一步推进到 **FP4/MXFP4** 微缩放格式。幻灯片“低比特浮点转换起来很直接”的直觉可以一路下探到 8 位和 4 位。
> - **GGUF + llama.cpp** 是这一切抵达边缘的途径：一种打包 K-quant INT4/INT5 权重的文件格式，可在 CPU、Apple Silicon 和手机上运行 LLM —— “为电池供电设备做量化”的实际落脚点。
>
> **蒸馏从 BERT 走到了 GPT-4。** **DistilBERT**（小 40%，快 60%，达到 BERT 的约 97%）是第 4 节的经典实现。今天的主流形式是**把一个巨大的前沿模型蒸馏进一个小模型**：用强教师（如 GPT-4 级别）生成高质量输出，再在其上微调一个小的开放学生 —— 即第 4.1 节代理数据的论证，其中 `q` 是“教师会给出的任何答案”。大多数能力强的小型开放模型都是这样做出来的。（注意从闭源 API 蒸馏的许可与政策约束 —— 这如今是工程*与*法律的双重决策。）
>
> **LLM 规模的剪枝**正如第 2.2 节所预言的那样依然困难。**SparseGPT** 和 **Wanda** 无需重训练即可一次性把 LLM 剪掉约 50%；持久的教训依然成立 —— 要获得*速度*，在 Ampere 架构及以上 / Hopper 上仍需要 **2:4 结构化**稀疏，因为散落的零仍然无法被加速。**混合专家模型（MoE）** 是架构层面的近亲：把每个 token 路由到众多专家 FFN 中的少数几个，于是*总*参数量巨大的模型，每个 token 只激活其中一小*部分* —— 以条件计算实现压缩，也是若干前沿模型推理服务的成本远低于其参数量所暗示这一点的原因。
>
> 深入下去的地方：[MLSys Deep Dives](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/README.md) 课程涵盖 INT4/FP8 kernel、paged-KV 和 roofline（性能上界模型）数学；[Gemma 4 Edge Deployment](../../../../阶段5-高级专题与专精/03-方向C-边缘AI/04-Gemma-4边缘部署/README.md) 与 [Edge LLM Inference Internals](../../../../阶段5-高级专题与专精/03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md) 材料端到端讲解 GGUF 量化与端侧推理服务。

---


<details>
<summary>English original</summary>

**6. 2026 update — the LLM era of model compression**

> **2026 update — the original ideas, mapped onto today's stack.** CS329P's 2021 framing is *exactly right* and still the foundation — but the center of gravity has moved from CNNs to LLMs, where the model is memory-bandwidth-bound at decode (Section 1) and compression is no longer optional. Learn the originals above first; here is where each one went.
>
> **Quantization went sub-INT8 and got outlier-aware.** The slide's "extremely low-bit hurts accuracy" warning was right *for naive PTQ* — the field beat it by handling the outliers from Section 3.3 directly:
>
> - **GPTQ** and **AWQ** are *weight-only* INT4 PTQ methods that quantize an LLM to 4 bits with near-FP16 quality. GPTQ uses second-order (Hessian) information to compensate for rounding error layer by layer; AWQ ("activation-aware") scales weight channels by their activation magnitude so the *important* channels keep precision. Both make INT4 a production default for serving 7B–70B models.
> - **SmoothQuant** is the direct answer to the activation-outlier problem: it *migrates* the difficulty from activations into weights by a per-channel scaling, so both can be quantized to INT8 — enabling true INT8 *activation* quantization (W8A8), not just weights.
> - **NF4 + QLoRA** quantizes a frozen base model to a 4-bit "NormalFloat" type and trains tiny LoRA adapters on top, fine-tuning a 65B model on a *single GPU*. This is QAT's spirit (train-aware-of-quantization) made practical at LLM scale.
> - **FP8** is the new training/inference type on **Hopper (H100)** and **Blackwell (B100/B200)** — an 8-bit *float* (E4M3 / E5M2) that keeps a usable exponent range, so it tolerates outliers far better than INT8 for *activations*. Blackwell pushes further to **FP4/MXFP4** microscaling formats. The slide's "low-bit floats are straightforward to cast" intuition scales right down to 8 and 4 bits.
> - **GGUF + llama.cpp** is how all of this reaches the edge: a file format packing K-quant INT4/INT5 weights that runs LLMs on CPUs, Apple Silicon, and phones — the practical home of "quantize for a battery-powered device."
>
> **Distillation went from BERT to GPT-4.** **DistilBERT** (40% smaller, 60% faster, ~97% of BERT) is the canonical realization of Section 4. Today the dominant form is **distilling a giant frontier model into a small one**: prompt a strong teacher (e.g. GPT-4-class) to generate high-quality outputs, then fine-tune a small open student on them — the surrogate-data argument of Section 4.1, where `q` is "whatever the teacher will answer." This is how most capable small open models are made. (Note the licensing and policy constraints on distilling from a closed API — that is now an engineering *and* legal decision.)
>
> **Pruning at LLM scale** stayed hard exactly as Section 2.2 predicted. **SparseGPT** and **Wanda** prune LLMs to ~50% in one shot without retraining; the durable lesson holds — to get *speed* you still need **2:4 structured** sparsity on Ampere+/Hopper, because scattered zeros remain unaccelerable. **Mixture-of-Experts (MoE)** is the architectural cousin: route each token to a few of many expert FFNs, so a model with huge *total* parameters activates only a small *fraction* per token — conditional computation as compression, and the reason several frontier models are far cheaper to serve than their parameter count suggests.
>
> Where this goes deep: the [MLSys Deep Dives](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/README.md) course covers INT4/FP8 kernels, paged-KV, and the roofline math; the [Gemma 4 Edge Deployment](../../../../阶段5-高级专题与专精/03-方向C-边缘AI/04-Gemma-4边缘部署/README.md) and [Edge LLM Inference Internals](../../../../阶段5-高级专题与专精/03-方向C-边缘AI/03-边缘LLM推理内部机制/Lecture-01.md) material walk GGUF quantization and on-device serving end to end.

---

</details>

## 摘要

模型是过参数化的——这是训练期的优势，到了部署期却变成一张账单，要对抗四项硬预算：内存、延迟、成本与能耗（其中*搬运*字节比在其上做计算更贵）。三种经典手段能把预算买回来：

- **剪枝**把权重置零；对硬件而言的坑在于：*非结构化*稀疏几乎无法加速，真正的加速来自硅片能跳过的*结构化* / 2:4 模式。
- **量化**用更少的比特——INT8 是通用的主力，准确率的损失主要来自*离群值*——在 PTQ（快，只做校准）与 QAT（一次训练运行来恢复准确率）之间做选择。
- **蒸馏**让一个小学生模型在教师的*软目标*上训练，其经温度软化的、关于负类别的“暗知识”让学生模型可训练，并且能把整个集成模型压成一个廉价网络。

它们可以组合，而一项技术所需要的*硬件支持*就是工程师的决策依据。这里是 ML 工程交接给硬件的节点——阶段 5 就是它落到金属上的地方。

---

## 本文时效

2026 年 6 月。斯坦福 CS329P（2021）中经典的剪枝/量化/蒸馏核心未变，且仍是基础；本讲针对 LLM 时代大幅刷新了**量化**内容——INT4 仅权重 PTQ（GPTQ/AWQ）、NF4+QLoRA、Hopper 与 Blackwell 上的 FP8/FP4、针对激活值离群值的 SmoothQuant，以及边缘侧的 GGUF/llama.cpp——外加现代前沿模型蒸馏，以及把 MoE 作为条件计算压缩。INT8 仍是无标记的生产默认值；INT4 权重量化的 LLM 推理服务如今已是主流。

*改编自 [Stanford CS329P](https://c.d2l.ai/stanford-cs329p)——Huang、Li & Smola，CC-BY-SA-4.0。*


<details>
<summary>English original</summary>

**Summary**

Models are overparameterized — a training-time advantage that becomes a deployment-time bill against four hard budgets: memory, latency, cost, and energy (where *moving* bytes costs more than computing on them). Three classical levers buy the budget back:

- **Pruning** zeros weights; the catch for hardware is that *unstructured* sparsity is rarely accelerable, so real speedups come from *structured* / 2:4 patterns the silicon can skip.
- **Quantization** uses fewer bits — INT8 as the portable workhorse, with accuracy lost mainly to *outliers* — choosing PTQ (fast, calibration-only) vs QAT (a training run that recovers accuracy).
- **Distillation** trains a small student on a teacher's *soft targets*, whose temperature-softened "dark knowledge" over the negative classes makes the student trainable, and which can collapse a whole ensemble into one cheap net.

They compose, and the *hardware support* a technique needs is the engineer's decision rule. This is the handoff point from ML engineering to hardware — Phase 5 is where it goes to the metal.

---

**Current as of**

June 2026. The classical pruning/quantization/distillation core from Stanford CS329P (2021) is unchanged and foundational; this lecture refreshes the **quantization** material heavily for the LLM era — INT4 weight-only PTQ (GPTQ/AWQ), NF4+QLoRA, FP8/FP4 on Hopper and Blackwell, SmoothQuant for activation outliers, and GGUF/llama.cpp on the edge — plus modern frontier-model distillation and MoE as conditional-computation compression. INT8 remains the unmarked production default; INT4 weight-quantized LLM serving is now mainstream.

*Adapted from [Stanford CS329P](https://c.d2l.ai/stanford-cs329p) — Huang, Li & Smola, CC-BY-SA-4.0.*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/4. ML Engineering and MLOps/Practical Machine Learning (CS329P)/Lecture-10.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/4.%20ML%20Engineering%20and%20MLOps/Practical%20Machine%20Learning%20%28CS329P%29/Lecture-10.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
