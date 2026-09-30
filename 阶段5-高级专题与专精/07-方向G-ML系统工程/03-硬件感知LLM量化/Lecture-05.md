---
title: 模块 05 — 校准：选择 scale
description: 模块 05 — 校准：选择 scale
---

# 模块 05 — 校准：选择 scale

**合集：** [Hardware-Aware LLM Quantization](README.md) | **上一篇：** [← 模块 04](Lecture-04.md) | **下一篇：** [模块 06 →](Lecture-06.md)

---

[模块 02](Lecture-02.md) 以一个未解决的取舍收尾：按绝对值最大值设定 block 的 scale，小数值就会被抹掉；裁剪最大值，离群值又会被扭曲。不存在普适正确的答案——只有误差度量和对 scale 的搜索。

这个搜索就是**校准**，本模块讲的是如何把它做好。核心思路是：已发表的方法并不是排行榜上互相竞争的名次，而是**针对不同误差模式的答案**。要按诊断来选，而不是按流行度来选。

---

## 学习目标

学完本模块后，你应当能够：

1. 针对 27 B 模型，基于成本/收益在 PTQ 与 QAT 之间做选择。
2. 设计校准集——规模、领域、序列长度——并解释每一项选择。
3. 推导 MSE 最优的裁剪阈值，并解释为什么 absmax 很少就是它。
4. 用一句话分别解释 **AWQ**、**GPTQ** 和 **SmoothQuant** 的机制，并说明各自针对哪种误差模式。
5. 根据诊断而非 benchmark 表格来选择方法。

---

## 1. PTQ vs QAT——先把这件事定下来

```text
   PTQ (post-training quantization)
     calibrate on 128–1024 sequences · minutes to hours · no gradients · no training data needed

   QAT (quantization-aware training)
     fine-tune with fake-quant in the forward pass · GPU-days · needs training data + recipe
```

| | PTQ | QAT |
|---|---|---|
| 27 B 模型的成本 | ~1 GPU-hour | ~100s of GPU-hours |
| 所需数据 | 几百条无标注序列 | 真实训练语料 |
| 4-bit 下的典型恢复 | 用强方法即可达到良好 | 更好，有时提升显著 |
| 风险 | 对基础模型无风险 | 若 recipe 有误，可能*损害*其他能力 |

**对本课程的问题，用 PTQ。** 原因不是成本——而是 [模块 01](Lecture-01.md) 中的框架。进一步量化权重所能带来的总可用吞吐收益是有上限的（模块 04 把 `lm_head` 定为 +8.7 %）。花 100 GPU-hour 的 QAT，去在一项只值 8.7 % 吞吐的改动上找回最后 0.3 % 的行为，是糟糕的分配，尤其当 [模块 07](Lecture-07.md) 表明*选择不同的张量*比*在相同张量上训练得更狠*能找回更多行为时。

QAT 只在两种特定情形下才值回成本：你要降到 4 bits 以下（[模块 03](Lecture-03.md) 已在本芯片上排除了这一点），或者你要把激活值量化到 4 bits 并撞上离群值这堵墙（[模块 06](Lecture-06.md)）。

---

## 2. 校准集设计

校准估计的是你的 scale 必须覆盖的分布。分布搞错，下游每一个 scale 都会错。

```text
   SIZE          128–512 sequences is the standard working range.
                 Below ~64: scale estimates are noisy, especially percentile-based ones.
                 Above ~1024: returns flatten. This is a cheap parameter — sweep it once.

   LENGTH        Match your DEPLOYMENT sequence length distribution.
                 Calibrating at 512 tokens and serving at 262,144 is a real mismatch:
                 activation magnitudes and attention-sink behaviour both change with length
                 (Modules 06, 09).

   DOMAIN        Match deployment. Code-heavy serving → include code.
                 A general-web calibration set on a code model measurably misplaces scales.

   CONTAMINATION Never calibrate on your evaluation set. It is the quantization equivalent
                 of training on the test set, and it produces a quant that grades well and
                 serves badly.
```

**最常见的校准 bug 是长度不匹配**，而且它在短上下文评估中不可见。如果你要服务长上下文，就用长序列做校准，即便代价更高。

一个站得住脚的默认做法：

```python
CALIBRATION = dict(
    n_sequences   = 256,
    seq_length    = 4096,          # or your p50 deployment length, whichever is larger
    domains       = ["web", "code", "math", "chat"],   # weighted to your traffic mix
    seed          = 0,             # calibration is an experiment; it must be reproducible
    exclude       = ["wikitext-2", "your_eval_sets"],
)
```

---

## 3. 选择 scale：四种估计器

给定一个 block 或通道的值，scale 决定可表示的窗口。有四种选取方式：

### 3.1 Absmax

```text
   s = max(|x|) / q_max
```

构造上裁剪误差为零，舍入误差最大。**一个离群值就为整组设定了 scale**——这正是 [模块 02 §4](Lecture-02.md) 中演示的失败情形，那里 `0.9` 量化到 `0`，只因为有一个元素是 `23.7`。

对于分布规整的权重和小 block 还行。对激活值则很差。


<details>
<summary>English original</summary>

**Module 05 — Calibration: Choosing the Scales**

**Collection:** [Hardware-Aware LLM Quantization](README.md) | **Previous:** [← Module 04](Lecture-04.md) | **Next:** [Module 06 →](Lecture-06.md)

---

[Module 02](Lecture-02.md) ended on an unresolved trade: set a block's scale from its absolute maximum and the small values are annihilated; clip the maximum and the outlier is mangled. There is no universally correct answer — only an error metric and a search over scales.

That search is **calibration**, and this module is about running it well. The organizing idea is that the published methods are not competitors on a leaderboard; they are **answers to different error modes**. Pick by diagnosis, not by popularity.

---

**Learning objectives**

By the end of this module you should be able to:

1. Choose between PTQ and QAT on cost/benefit grounds for a 27 B model.
2. Design a calibration set — size, domain, sequence length — and explain each choice.
3. Derive the MSE-optimal clipping threshold and explain why absmax is rarely it.
4. Explain the mechanism of **AWQ**, **GPTQ**, and **SmoothQuant** in one sentence each, and state which error mode each addresses.
5. Select a method from a diagnosis rather than from a benchmark table.

---

**1. PTQ vs QAT — settle this first**

```text
   PTQ (post-training quantization)
     calibrate on 128–1024 sequences · minutes to hours · no gradients · no training data needed

   QAT (quantization-aware training)
     fine-tune with fake-quant in the forward pass · GPU-days · needs training data + recipe
```

| | PTQ | QAT |
|---|---|---|
| Cost for a 27 B model | ~1 GPU-hour | ~100s of GPU-hours |
| Data needed | a few hundred unlabeled sequences | real training corpus |
| Typical recovery at 4-bit | good with a strong method | better, sometimes materially |
| Risk | none to the base model | can *degrade* other capabilities if the recipe is wrong |

**For this course's problem, use PTQ.** The reason is not cost — it is the framework from [Module 01](Lecture-01.md). Your total available throughput win from further weight quantization is bounded (Module 04 put `lm_head` at +8.7 %). Spending 100 GPU-hours of QAT to recover the last 0.3 % of behavior on a change worth 8.7 % throughput is a bad allocation, especially when [Module 07](Lecture-07.md) shows that *choosing different tensors* recovers more behavior than *training harder on the same tensors*.

QAT earns its cost in two specific cases: you are going below 4 bits (which [Module 03](Lecture-03.md) already ruled out on this silicon), or you are quantizing activations to 4 bits and hitting the outlier wall ([Module 06](Lecture-06.md)).

---

**2. Calibration set design**

Calibration estimates the distributions your scales must cover. Get the distribution wrong and every downstream scale is wrong.

```text
   SIZE          128–512 sequences is the standard working range.
                 Below ~64: scale estimates are noisy, especially percentile-based ones.
                 Above ~1024: returns flatten. This is a cheap parameter — sweep it once.

   LENGTH        Match your DEPLOYMENT sequence length distribution.
                 Calibrating at 512 tokens and serving at 262,144 is a real mismatch:
                 activation magnitudes and attention-sink behaviour both change with length
                 (Modules 06, 09).

   DOMAIN        Match deployment. Code-heavy serving → include code.
                 A general-web calibration set on a code model measurably misplaces scales.

   CONTAMINATION Never calibrate on your evaluation set. It is the quantization equivalent
                 of training on the test set, and it produces a quant that grades well and
                 serves badly.
```

**The most common calibration bug is length mismatch**, and it is invisible in short-context evaluation. If you serve long context, calibrate on long sequences even though it costs more.

A defensible default:

```python
CALIBRATION = dict(
    n_sequences   = 256,
    seq_length    = 4096,          # or your p50 deployment length, whichever is larger
    domains       = ["web", "code", "math", "chat"],   # weighted to your traffic mix
    seed          = 0,             # calibration is an experiment; it must be reproducible
    exclude       = ["wikitext-2", "your_eval_sets"],
)
```

---

**3. Choosing a scale: four estimators**

Given a block or channel of values, the scale determines the representable window. Four ways to pick it:

**3.1 Absmax**

```text
   s = max(|x|) / q_max
```

Zero clipping error by construction, maximal rounding error. **One outlier sets the scale for the entire group** — this is exactly the failure demonstrated in [Module 02 §4](Lecture-02.md), where `0.9` quantized to `0` because one element was `23.7`.

Fine for weights with well-behaved distributions and small blocks. Poor for activations.

</details>

### 3.2 百分位数

```text
   s = percentile(|x|, p) / q_max          typical p ∈ [99.9, 99.99]
```

截掉尾部，为大量值保留分辨率。开销低，效果出奇地好。代价是 `p` 是一个必须扫描的超参数，而且合适的取值因张量类型而异。

### 3.3 MSE 最优

搜索使重建误差最小的裁剪阈值：

```text
   c*  =  argmin_c  E[ ( Q_c(x) − x )² ]

   where the error splits exactly into the two mechanisms from Module 02:

     E[e²]  =  ∫       (Q(x)−x)² p(x) dx     ← rounding, decreases as c decreases
               |x|≤c
            +  ∫       (c·sign(x) − x)² p(x) dx   ← clipping, increases as c decreases
               |x|>c
```

```text
   error
     ▲
     │╲                                    ╱
     │ ╲  clipping error                  ╱  rounding error
     │  ╲                                ╱
     │   ╲                              ╱
     │    ╲__                        __╱
     │       ╲──__            __──╱
     │             ╲──_  _──╱
     │                 ╲╱          ← c*, the MSE-optimal threshold
     └──────────────────┴──────────────────────▶  clipping threshold c
                                          absmax (c = max|x|)
```

每组对 `c ∈ [0.5, 1.0] × max|x|` 做 20–50 个点的网格搜索开销很低，是优秀 PTQ 流水线的主力。

```python
def mse_optimal_scale(x, q_grid, n_steps=40):
    """Grid-search the clipping ratio that minimizes reconstruction MSE."""
    amax, best, best_err = x.abs().max(), None, float("inf")
    for r in torch.linspace(0.5, 1.0, n_steps):
        s = (amax * r) / q_grid.max()
        xq = quantize_to_grid(x / s, q_grid) * s     # round-to-nearest + clamp
        err = ((xq - x) ** 2).mean().item()
        if err < best_err:
            best_err, best = err, s
    return best
```

### 3.4 KL / 基于熵

最小化量化前后数值分布之间的散度，而非逐点误差。这是 TensorRT 经典的 INT8 激活值校准器。它优化的是分布形状而非逐元素保真度，有时后者更重要——但对权重而言，MSE 通常是更好的代理指标，且推理起来便宜得多。

### 该用哪种

| 张量 | 推荐 | 原因 |
|---|---|---|
| 权重，小分块（NVFP4 group 16） | **MSE 最优**，absmax 可接受 | group 小；离群值损害被限制住 |
| 权重，大 group（≥128） | **MSE 最优** | 一个离群值会伤害许多值 |
| 激活值 | **百分位数或 MSE**，绝不用 absmax | 偶发 token 尖峰（[Module 06](Lecture-06.md)） |
| KV cache | 每个 head 用**百分位数** | 开销低，可在线运行，不能阻塞 decode |

---

## 4. 超越 scale：三种真正重要的方法

仅做 scale 选择是把每个张量孤立看待。下面三种方法利用**结构**——且各自针对不同的误差模式。

### 4.1 AWQ——保护显著的权重通道

**观察：** 并非所有权重通道都同等重要。与大激活值相乘的通道主导输出，约 1 % 的通道占据了不成比例的误差份额。

**机制：** 与其让这些通道保持更高精度（这会破坏 kernel 所需的统一布局），AWQ 在激活值与权重之间**迁移 scale**。对于逐通道因子 `s`：

```text
   y = (x / s) · (s · W)
       └──────┘   └─────┘
       activation  weight scaled UP before quantization
       scaled DOWN → occupies more of the E2M1 grid → quantizes more accurately
```

乘积在数学上不变；量化误差则不然。导出时该缩放被折叠进前一个 layernorm，因此**它在 runtime 中零开销**。

**针对的误差模式：** 量化误差被大激活值放大的*权重*通道。

### 4.2 GPTQ——逐层补偿误差

**观察：** 量化权重 `w_i` 会产生一个已知误差。同一层中其余尚未量化的权重可以被*调整*以吸收该误差。

**机制：** 利用该层的 Hessian `H = 2·E[x xᵀ]`（来自校准激活值），一次量化一个权重，并沿使该层输出误差最小的方向更新其余权重——即 Optimal Brain Surgeon 更新，通过按固定顺序处理并配合 Cholesky 分解使其可行：

```text
   for each column i:
       q_i   = quantize(w_i)
       err   = (w_i − q_i) / H⁻¹_ii
       w_{i+1:}  −=  err · H⁻¹_{i, i+1:}       ← remaining weights absorb the damage
```

**针对的误差模式：** *累积的*层输出误差。GPTQ 不关心单个权重的保真度；它最小化该层输出的误差，而这才是真正会传播的东西。


<details>
<summary>English original</summary>

**3.2 Percentile**

```text
   s = percentile(|x|, p) / q_max          typical p ∈ [99.9, 99.99]
```

Clip the tail, keep resolution for the bulk. Cheap and surprisingly effective. The cost is that `p` is a hyperparameter you must sweep, and the right value differs per tensor type.

**3.3 MSE-optimal**

Search the clipping threshold that minimizes reconstruction error:

```text
   c*  =  argmin_c  E[ ( Q_c(x) − x )² ]

   where the error splits exactly into the two mechanisms from Module 02:

     E[e²]  =  ∫       (Q(x)−x)² p(x) dx     ← rounding, decreases as c decreases
               |x|≤c
            +  ∫       (c·sign(x) − x)² p(x) dx   ← clipping, increases as c decreases
               |x|>c
```

```text
   error
     ▲
     │╲                                    ╱
     │ ╲  clipping error                  ╱  rounding error
     │  ╲                                ╱
     │   ╲                              ╱
     │    ╲__                        __╱
     │       ╲──__            __──╱
     │             ╲──_  _──╱
     │                 ╲╱          ← c*, the MSE-optimal threshold
     └──────────────────┴──────────────────────▶  clipping threshold c
                                          absmax (c = max|x|)
```

A 20–50 point grid search over `c ∈ [0.5, 1.0] × max|x|` per group is cheap and is the workhorse of good PTQ pipelines.

```python
def mse_optimal_scale(x, q_grid, n_steps=40):
    """Grid-search the clipping ratio that minimizes reconstruction MSE."""
    amax, best, best_err = x.abs().max(), None, float("inf")
    for r in torch.linspace(0.5, 1.0, n_steps):
        s = (amax * r) / q_grid.max()
        xq = quantize_to_grid(x / s, q_grid) * s     # round-to-nearest + clamp
        err = ((xq - x) ** 2).mean().item()
        if err < best_err:
            best_err, best = err, s
    return best
```

**3.4 KL / entropy-based**

Minimize the divergence between the pre- and post-quantization value distributions rather than the pointwise error. This is TensorRT's classic INT8 activation calibrator. It optimizes distribution shape over element-wise fidelity, which sometimes matters more — but for weights, MSE is usually the better proxy and is far cheaper to reason about.

**Which to use**

| Tensor | Recommended | Why |
|---|---|---|
| Weights, small blocks (NVFP4 group 16) | **MSE-optimal**, absmax acceptable | group is small; outlier damage is contained |
| Weights, large groups (≥128) | **MSE-optimal** | one outlier hurts many values |
| Activations | **percentile or MSE**, never absmax | rare token spikes ([Module 06](Lecture-06.md)) |
| KV cache | **percentile** per head | cheap, runs online, must not stall decode |

---

**4. Beyond scales: the three methods that matter**

Scale selection alone treats each tensor in isolation. The three methods below exploit **structure** — and each targets a different error mode.

**4.1 AWQ — protect the salient weight channels**

**Observation:** not all weight channels matter equally. The channels multiplied by large activations dominate the output, and ~1 % of channels account for a disproportionate share of the error.

**Mechanism:** rather than keeping those channels in higher precision (which breaks the uniform layout the kernels need), AWQ **migrates scale** between activations and weights. For a per-channel factor `s`:

```text
   y = (x / s) · (s · W)
       └──────┘   └─────┘
       activation  weight scaled UP before quantization
       scaled DOWN → occupies more of the E2M1 grid → quantizes more accurately
```

The product is mathematically unchanged; the quantization error is not. The scaling is folded into the preceding layernorm at export, so **it costs nothing at runtime**.

**Error mode addressed:** *weight* channels whose quantization error is amplified by large activations.

**4.2 GPTQ — compensate error layer by layer**

**Observation:** quantizing weight `w_i` produces a known error. The remaining un-quantized weights in the same layer can be *adjusted* to absorb it.

**Mechanism:** using the layer's Hessian `H = 2·E[x xᵀ]` (from calibration activations), quantize weights one at a time and update the rest along the direction that minimizes the layer's output error — the Optimal Brain Surgeon update, made tractable by processing in a fixed order with a Cholesky factorization:

```text
   for each column i:
       q_i   = quantize(w_i)
       err   = (w_i − q_i) / H⁻¹_ii
       w_{i+1:}  −=  err · H⁻¹_{i, i+1:}       ← remaining weights absorb the damage
```

**Error mode addressed:** *accumulated* layer-output error. GPTQ does not care about individual weight fidelity; it minimizes the error of the layer's output, which is the thing that actually propagates.

</details>

### 4.3 SmoothQuant —— 把难度从激活值转移到权重

**观察：** 激活值存在系统性的逐通道离群值；权重没有（[Module 06](Lecture-06.md)）。因此激活值比权重难量化得多。

**机制：** 用一个逐通道的平滑因子 `s_j = max|X_j|^α / max|W_j|^(1−α)` 把难度迁移过去：

```text
   Y = (X · diag(s)⁻¹) · (diag(s) · W)
        └────────────┘    └──────────┘
        activations        weights become
        become SMOOTHER    slightly harder
                           (they can afford it)
```

`α ≈ 0.5` 在两者之间做平衡；`α → 1` 把全部难度推到权重上。

**所针对的失效模式：** *激活值*的通道离群值。对 W8A8 和 W4A4 必不可少；对 **W4A16 则无关**，因为激活值若保持 BF16，就不存在需要平滑的激活值量化误差。

### 按诊断结果选择方法

```text
   What is your dominant error source?
   │
   ├── Weight channels amplified by large activations   ──▶  AWQ
   │
   ├── Accumulated layer-output error at low bit-width  ──▶  GPTQ
   │      (and AWQ + GPTQ compose — they address different things)
   │
   ├── Activation per-channel outliers                  ──▶  SmoothQuant
   │      ONLY if you are quantizing activations at all
   │
   └── Rare token-level activation spikes               ──▶  neither; see Module 06
          (these are not channel-structured; smoothing does not remove them)
```

> 就本案例研究而言 —— **在 `sm_120` 上的 W4A16 NVFP4，batch 1** —— 激活值保持 BF16，因此 SmoothQuant **不适用**。相关的方法是 AWQ 和 GPTQ。工程师常把 SmoothQuant 套用到 W4A16 流水线上，报告说没有收益，于是断定该方法很弱。方法本身没问题；它瞄准的是一个该配置并不具备的失效模式。

---

## 5. 校准是一次实验，所以要给它做版本管理

每个量化产物都必须携带其校准来源信息，否则你无法复现或调试它：

```yaml
quantization:
  format: NVFP4              # E2M1 + block16 + E4M3 scale + FP32 global
  scheme: W4A16
  scale_estimator: mse_optimal
  clip_grid: [0.5, 1.0, 40]
  methods: [awq, gptq]
  gptq:
    damping: 0.01
    actorder: true
calibration:
  n_sequences: 256
  seq_length: 4096
  domains: {web: 0.4, code: 0.3, math: 0.15, chat: 0.15}
  seed: 0
  dataset_hash: sha256:...
excluded_layers: [lm_head, layers.0., layers.47.]   # see Module 07
provenance:
  toolkit: llm-compressor 0.x / TensorRT Model Optimizer 0.x
  commit: ...
```

两条能真正省下时间的规则：

* **`dataset_hash` 不是可选项。**“在 256 条 web 序列上校准”不可复现；哈希才可以。
* **在多数好的 recipe 中，首层和末层默认被排除。** [Module 07](Lecture-07.md) 解释了原因，以及如何针对你自己的模型验证这一点，而不是把它当作民间传说照单全收。

---

## 检查点

现在你应当能够：

1. 用成本/收益的论证说明在这个问题上为何选 PTQ 而非 QAT，并说出 QAT 胜出的两种情形。
2. 说出四个校准集参数，以及每一项取错时的失效模式。
3. 画出裁剪/舍入误差曲线，并定位 `c*`。
4. 用一句话分别陈述 AWQ、GPTQ 和 SmoothQuant 的机制。
5. 解释为什么 SmoothQuant 对 W4A16 流水线毫无作用。
6. 列出可复现的量化 manifest 必须携带的字段。

---

## 交付

从真实模型中取一个线性层，产出一张**校准消融表**：

| 估计器 | 层输出 MSE | 被抹除占比 | 墙钟时间 |
|---|---|---|---|
| absmax | | | |
| percentile 99.9 | | | |
| percentile 99.99 | | | |
| MSE-optimal | | | |
| MSE-optimal + AWQ | | | |
| MSE-optimal + AWQ + GPTQ | | | |

然后用文字回答：**你会交付哪一行，以及什么会让你改变主意？** 后半句才是关键。

---

## 时效截至

* **不随时间变化：** 裁剪/舍入的分解、MSE 最优搜索、AWQ/GPTQ/SmoothQuant 的机制、由失效模式驱动的选型。
* **2026 年的工具链：** `llm-compressor`（vLLM 生态）和 **NVIDIA TensorRT Model Optimizer** 是两套支持 NVFP4 的主流 PTQ 工具包。两者都实现了 AWQ 和 GPTQ；请在你安装的版本中核实 NVFP4 + `sm_120` 的导出覆盖情况，因为它在不同 release 之间会变。
* **需要定期刷新的部分：** 哪些方法针对哪些格式有实现。§4 中的失效模式分类是稳定的；实现每一条的工具则不是。

---

**下一篇：** [Module 06 — 激活值离群值 →](Lecture-06.md)


<details>
<summary>English original</summary>

**4.3 SmoothQuant — move the difficulty from activations to weights**

**Observation:** activations have systematic per-channel outliers; weights do not ([Module 06](Lecture-06.md)). Activations are therefore much harder to quantize than weights.

**Mechanism:** migrate the difficulty with a per-channel smoothing factor `s_j = max|X_j|^α / max|W_j|^(1−α)`:

```text
   Y = (X · diag(s)⁻¹) · (diag(s) · W)
        └────────────┘    └──────────┘
        activations        weights become
        become SMOOTHER    slightly harder
                           (they can afford it)
```

`α ≈ 0.5` balances the two; `α → 1` pushes all difficulty onto weights.

**Error mode addressed:** *activation* channel outliers. Essential for W8A8 and W4A4; **irrelevant for W4A16**, because if activations stay in BF16 there is no activation quantization error to smooth.

**Method selection by diagnosis**

```text
   What is your dominant error source?
   │
   ├── Weight channels amplified by large activations   ──▶  AWQ
   │
   ├── Accumulated layer-output error at low bit-width  ──▶  GPTQ
   │      (and AWQ + GPTQ compose — they address different things)
   │
   ├── Activation per-channel outliers                  ──▶  SmoothQuant
   │      ONLY if you are quantizing activations at all
   │
   └── Rare token-level activation spikes               ──▶  neither; see Module 06
          (these are not channel-structured; smoothing does not remove them)
```

> For the case study — **W4A16 NVFP4 on `sm_120`, batch 1** — activations stay BF16, so SmoothQuant is **not applicable**. The relevant methods are AWQ and GPTQ. Engineers routinely apply SmoothQuant to W4A16 pipelines and report no benefit, then conclude the method is weak. The method is fine; it was aimed at an error mode that configuration does not have.

---

**5. Calibration is an experiment, so version it**

Every quantized artifact must carry its calibration provenance, or you cannot reproduce or debug it:

```yaml
quantization:
  format: NVFP4              # E2M1 + block16 + E4M3 scale + FP32 global
  scheme: W4A16
  scale_estimator: mse_optimal
  clip_grid: [0.5, 1.0, 40]
  methods: [awq, gptq]
  gptq:
    damping: 0.01
    actorder: true
calibration:
  n_sequences: 256
  seq_length: 4096
  domains: {web: 0.4, code: 0.3, math: 0.15, chat: 0.15}
  seed: 0
  dataset_hash: sha256:...
excluded_layers: [lm_head, layers.0., layers.47.]   # see Module 07
provenance:
  toolkit: llm-compressor 0.x / TensorRT Model Optimizer 0.x
  commit: ...
```

Two rules that will save you real time:

* **`dataset_hash` is not optional.** "Calibrated on 256 web sequences" is not reproducible; a hash is.
* **First and last layers are excluded by default in most good recipes.** [Module 07](Lecture-07.md) explains why, and how to verify it for your model rather than inheriting it as folklore.

---

**Checkpoint**

You should now be able to:

1. Justify PTQ over QAT for this problem with a cost/benefit argument, and name the two cases where QAT wins.
2. Name the four calibration-set parameters and the failure mode of getting each wrong.
3. Sketch the clipping/rounding error curves and locate `c*`.
4. State AWQ, GPTQ, and SmoothQuant's mechanisms in one sentence each.
5. Explain why SmoothQuant does nothing for a W4A16 pipeline.
6. List the fields a reproducible quantization manifest must carry.

---

**Ship it**

Take one linear layer from a real model and produce a **calibration ablation table**:

| Estimator | Layer output MSE | Fraction annihilated | Wall-clock |
|---|---|---|---|
| absmax | | | |
| percentile 99.9 | | | |
| percentile 99.99 | | | |
| MSE-optimal | | | |
| MSE-optimal + AWQ | | | |
| MSE-optimal + AWQ + GPTQ | | | |

Then answer in writing: **which row would you ship, and what would change your mind?** The second half is the part that matters.

---

**Current as of**

* **Timeless:** the clipping/rounding decomposition, the MSE-optimal search, the mechanisms of AWQ/GPTQ/SmoothQuant, error-mode-driven selection.
* **2026 tooling:** `llm-compressor` (vLLM ecosystem) and **NVIDIA TensorRT Model Optimizer** are the two mainstream PTQ toolkits with NVFP4 support. Both implement AWQ and GPTQ; verify NVFP4 + `sm_120` export coverage in the version you install, since it moves between releases.
* **Refresh surface:** which methods are implemented for which formats. The error-mode taxonomy in §4 is stable; the tool that implements each is not.

---

**Next:** [Module 06 — Activation Outliers →](Lecture-06.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/Hardware-Aware LLM Quantization/Lecture-05.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/Hardware-Aware%20LLM%20Quantization/Lecture-05.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
