---
title: Module 12 — 研究方法论：真正能证明点什么的消融实验
description: Module 12 — 研究方法论：真正能证明点什么的消融实验
---

# Module 12 — 研究方法论：真正能证明点什么的消融实验

**Collection：** [Hardware-Aware LLM Quantization](README.md) | **Previous：** [← Module 11](Lecture-11.md) | **Next：** [Capstone →](Lecture-13.md)

---

本课程中的每一个数字都来自一次测量，而每一次测量都可能以看起来与结果一模一样的方式出错。本模块讨论的正是区分二者的那套纪律。

这件事在这里比在大多数性能工作中都更重要，因为量化结果**存在双重混淆风险**：你在同时改变一个速度变量和一个质量变量，而且是在时钟会自行漂移的硬件上，用带有真实方差的指标来评测。一个草率的实验方案，仅凭热漂移就能造出看起来可以发表的加速比。

---

## 学习目标

学完本模块，你应当能够：

1. 把 GPU 锁定到可复现的测量状态。
2. 设计一次只改一个变量的消融实验，并能识别出自己何时违反了这一原则。
3. 报告结果时，一并给出使其可被解读的那些变量。
4. 识别出五类无效对比，它们占了已发表量化论断的绝大多数。
5. 识别出 **bug 让 benchmark 变好** 这一失效模式。

---

## 1. 先锁定机器

未锁定的 GPU 不是测量仪器。时钟会随温度、功耗和邻近负载而变动，而这种漂移轻易就能大于你要找的那个效应。

```bash
# Persistence mode: keep the driver resident (avoids first-call initialization skew)
sudo nvidia-smi -pm 1

# Lock clocks. Pick a value the card can sustain INDEFINITELY, not a boost peak.
nvidia-smi -q -d SUPPORTED_CLOCKS | head -40
sudo nvidia-smi --lock-gpu-clocks=2400,2400
sudo nvidia-smi --lock-memory-clocks=14001,14001      # GDDR7: check your part's value

# Verify under load, not at idle:
nvidia-smi --query-gpu=clocks.sm,clocks.mem,temperature.gpu,power.draw,\
clocks_throttle_reasons.active --format=csv -l 1
```

```text
   THERMAL STATE IS A VARIABLE.
   ────────────────────────────────────────────────────────────────
   run 1 (cold, 42 °C)  :  84.1 tok/s
   run 2 (warm, 71 °C)  :  81.6 tok/s          ← a 3 % "regression" from physics
   run 3 (hot,  79 °C)  :  78.9 tok/s          ← now you are throttling

   Fix: fixed warmup, then measure. Same thermal state for EVERY configuration.
```

协议：

```python
BENCH = dict(
    warmup_iters   = 20,        # discard; brings clocks and caches to steady state
    measure_iters  = 100,
    repeats        = 5,         # separate PROCESSES, not just loops
    interleave     = True,      # A,B,A,B,A,B — never AAAA then BBBB
    report         = "median + IQR",   # not mean; one outlier should not move it
)
```

**`interleave = True` 是这里价值最高的一行。** 先跑完配置 A 的全部实验、再跑配置 B 的全部实验，会把你的对比与所有随时间漂移的东西混淆在一起——温度、后台进程、内存碎片、其他用户的作业。交错执行把漂移混淆转化为噪声，而噪声可以用平均来对付。

---

## 2. 一次只改一个变量

```text
   INVALID                                    VALID
   ────────────────────────────────           ──────────────────────────────
   baseline:  BF16, K=3, ctx 4K, vLLM         baseline:  NVFP4, K=3, ctx 4K, vLLM
   candidate: NVFP4, K=5, ctx 8K, TRT-LLM     candidate: NVFP4, K=5, ctx 4K, vLLM
              ▲      ▲       ▲       ▲                          ▲
              └──── four changes ────┘                    one change
        "NVFP4 gave us 2.3×"  ← attributable to nothing
```

在量化对比中必须**保持不变**的变量，以及每个变量为何会造成干扰：

| 变量 | 为何会混淆 |
|---|---|
| **草稿深度 `K`** | 直接改变 τ（[Module 10](Lecture-10.md)） |
| **草稿模型本身** | 接受率衡量的是 target 与 drafter 的对比；同时改动两者无法解读（[Module 08](Lecture-08.md)） |
| **上下文长度** | 吞吐最多波动 45 %（[Module 09](Lecture-09.md)） |
| **批大小 / 并发度** | 会让你沿 roofline 移动（[Module 01](Lecture-01.md)） |
| **Runtime + 版本** | kernel 覆盖范围会随发布版本变化（[Module 03](Lecture-03.md)） |
| **采样参数** | temperature 与 top-p 会改变接受率 |
| **Prompt 集合** | 接受率强烈依赖内容 |
| **时钟 / 热状态** | §1 |

**草稿模型这一项是最微妙的，也是最常被违反的。** 如果你把整个检查点（包括 MTP head）都量化，然后把接受率与 BF16 基线相比，你就同时改变了 `p` 和 `q`。接受率差值不再衡量目标模型的漂移，而 [Module 08](Lecture-08.md) 的恒等式 `α = 1 − TV(p,q)` 也不再能隔离出任何东西。

---


<details>
<summary>English original</summary>

**Module 12 — Research Methodology: Ablations That Actually Prove Something**

**Collection:** [Hardware-Aware LLM Quantization](README.md) | **Previous:** [← Module 11](Lecture-11.md) | **Next:** [Capstone →](Lecture-13.md)

---

Every number in this course came from a measurement, and every measurement can be wrong in ways that look exactly like a result. This module is about the discipline that separates the two.

It matters more here than in most performance work, because quantization results are **doubly confoundable**: you are changing both a speed variable and a quality variable at once, on hardware whose clocks move under you, evaluated with metrics that have real variance. A sloppy protocol will produce a publishable-looking speedup from thermal drift alone.

---

**Learning objectives**

By the end of this module you should be able to:

1. Lock a GPU into a reproducible measurement state.
2. Design a one-variable-at-a-time ablation and recognize when you have violated it.
3. Report a result with the variables that make it interpretable.
4. Identify the five invalid comparisons that account for most published quantization claims.
5. Recognize the failure mode where a **bug improves the benchmark**.

---

**1. Lock the machine first**

An unlocked GPU is not a measurement instrument. Clocks move with temperature, power, and neighbouring work, and the drift is easily larger than the effect you are hunting.

```bash
# Persistence mode: keep the driver resident (avoids first-call initialization skew)
sudo nvidia-smi -pm 1

# Lock clocks. Pick a value the card can sustain INDEFINITELY, not a boost peak.
nvidia-smi -q -d SUPPORTED_CLOCKS | head -40
sudo nvidia-smi --lock-gpu-clocks=2400,2400
sudo nvidia-smi --lock-memory-clocks=14001,14001      # GDDR7: check your part's value

# Verify under load, not at idle:
nvidia-smi --query-gpu=clocks.sm,clocks.mem,temperature.gpu,power.draw,\
clocks_throttle_reasons.active --format=csv -l 1
```

```text
   THERMAL STATE IS A VARIABLE.
   ────────────────────────────────────────────────────────────────
   run 1 (cold, 42 °C)  :  84.1 tok/s
   run 2 (warm, 71 °C)  :  81.6 tok/s          ← a 3 % "regression" from physics
   run 3 (hot,  79 °C)  :  78.9 tok/s          ← now you are throttling

   Fix: fixed warmup, then measure. Same thermal state for EVERY configuration.
```

The protocol:

```python
BENCH = dict(
    warmup_iters   = 20,        # discard; brings clocks and caches to steady state
    measure_iters  = 100,
    repeats        = 5,         # separate PROCESSES, not just loops
    interleave     = True,      # A,B,A,B,A,B — never AAAA then BBBB
    report         = "median + IQR",   # not mean; one outlier should not move it
)
```

**`interleave = True` is the single highest-value line here.** Running all of configuration A and then all of configuration B confounds your comparison with everything that drifts over time — temperature, background processes, memory fragmentation, another user's job. Interleaving converts a drift confound into noise, which averaging can handle.

---

**2. One variable at a time**

```text
   INVALID                                    VALID
   ────────────────────────────────           ──────────────────────────────
   baseline:  BF16, K=3, ctx 4K, vLLM         baseline:  NVFP4, K=3, ctx 4K, vLLM
   candidate: NVFP4, K=5, ctx 8K, TRT-LLM     candidate: NVFP4, K=5, ctx 4K, vLLM
              ▲      ▲       ▲       ▲                          ▲
              └──── four changes ────┘                    one change
        "NVFP4 gave us 2.3×"  ← attributable to nothing
```

The variables that must be **held fixed** across a quantization comparison, and the reason each one bites:

| Variable | Why it confounds |
|---|---|
| **Draft depth `K`** | changes τ directly ([Module 10](Lecture-10.md)) |
| **The drafter itself** | acceptance measures target-vs-drafter; moving both is uninterpretable ([Module 08](Lecture-08.md)) |
| **Context length** | up to 45 % throughput swing ([Module 09](Lecture-09.md)) |
| **Batch size / concurrency** | moves you along the roofline ([Module 01](Lecture-01.md)) |
| **Runtime + version** | kernel coverage changes between releases ([Module 03](Lecture-03.md)) |
| **Sampling parameters** | temperature and top-p change acceptance |
| **Prompt set** | acceptance is strongly content-dependent |
| **Clocks / thermal state** | §1 |

**The drafter one is the subtle one and the most commonly violated.** If you quantize the whole checkpoint including the MTP head and then compare acceptance to the BF16 baseline, you have changed both `p` and `q`. The acceptance delta no longer measures target drift, and the [Module 08](Lecture-08.md) identity `α = 1 − TV(p,q)` no longer isolates anything.

---

</details>

## 3. 配对比较与效应量

在 **完全相同的 prompt、完全相同的顺序** 下运行配置，并逐 prompt 比较，而不是聚合比较。配对消除了 prompt 难度方差，这通常是最大的噪声来源：

```python
def paired_compare(cfg_a, cfg_b, prompts, repeats=5):
    """Per-prompt paired deltas. Returns effect size and a confidence interval."""
    deltas = []
    for p in prompts:
        for r in range(repeats):
            a = run(cfg_a, p, seed=r)          # same seed, same prompt, both configs
            b = run(cfg_b, p, seed=r)
            deltas.append(b.tok_s - a.tok_s)   # PAIRED delta
    d = np.array(deltas)
    se = d.std(ddof=1) / np.sqrt(len(d))
    return {
        "mean_delta": d.mean(),
        "ci95":       (d.mean() - 1.96 * se, d.mean() + 1.96 * se),
        "significant": abs(d.mean()) > 1.96 * se,
        "n":          len(d),
    }
```

并且要遵守来自 [Module 08 §5](Lecture-08.md) 的验收样本量：

```text
   claim a 1-point acceptance difference    →  ≥ ~8,000 draft tokens per config
   claim a 0.4-point difference             →  ≥ ~47,000 draft tokens per config

   Below that, "2.886 → 2.871" is a coin flip with a decimal point.
```

---

## 4. 五种无效比较

这些构成了你将读到的大多数量化结论 —— 包括你自己不小心得出的那些。

### 4.1 与配置糟糕的基线比较

```text
   "Our NVFP4 build is 3.1× faster than BF16."
   ...where the BF16 baseline used a naive kernel, no CUDA graphs, and batch 1
      while the NVFP4 build had all three.
```

**修复：** 基线必须与候选方案一样经过充分优化。如果调优了一个，就两个都调优 —— 或者直接说明你没有。

### 4.2 报告吞吐时缺少上下文长度

[Module 09](Lecture-09.md)：在 0 上下文时 81.6 tok/s，在 262 K 时 45.1 tok/s，同一模型。没有上下文长度的 tok/s 数字不是一次测量。

**修复：** 报告 tok/s 时要给出 p50 **和** p99 推理服务上下文。

### 4.3 仅在短上下文下测量质量

[Module 08](Lecture-08.md) 中的每个指标通常都在 2–4 K 下运行。[Module 07 的](Lecture-07.md) RoPE 论证预测 Q/K 损伤会随上下文 *增长*。在 4 K 下通过的配置可能在 262 K 下失败，而短上下文评估中没有任何东西会警告你。

**修复：** 在门禁中纳入长上下文探测。始终。

### 4.4 比较检查点大小而不是 `B_token`

```text
   "We cut the model 34 % and got 4 % more throughput. Quantization
    doesn't deliver what it promises."
```

它完全兑现了账本预测的结果。那 34 % 包含了 embedding 和一个视觉塔，它们在 decode（逐 token 生成阶段）期间从不会被读取（[Module 04](Lecture-04.md)）。

**修复：** 报告 `B_token`，并将达到的带宽报告为 `B_token × tok/s`。

### 4.5 忽略接受税

```text
   "+1.9 % throughput" — while τ fell from 2.792 to 2.546.
```

[Module 10](Lecture-10.md)：字节收益为 11.8 %，而接受税消耗了其中的 84 %。只报告净吞吐会掩盖两个事实：这一改动的价值远高于它实际带来的收益，*并且* 它损害了模型。

**修复：** 将 `B_token`、τ 和 tok/s 一起报告。始终三者都报。

---


<details>
<summary>English original</summary>

**3. Paired comparison and effect size**

Run configurations on **identical prompts in identical order** and compare per-prompt, not in aggregate. Pairing removes prompt-difficulty variance, which is usually the largest noise source:

```python
def paired_compare(cfg_a, cfg_b, prompts, repeats=5):
    """Per-prompt paired deltas. Returns effect size and a confidence interval."""
    deltas = []
    for p in prompts:
        for r in range(repeats):
            a = run(cfg_a, p, seed=r)          # same seed, same prompt, both configs
            b = run(cfg_b, p, seed=r)
            deltas.append(b.tok_s - a.tok_s)   # PAIRED delta
    d = np.array(deltas)
    se = d.std(ddof=1) / np.sqrt(len(d))
    return {
        "mean_delta": d.mean(),
        "ci95":       (d.mean() - 1.96 * se, d.mean() + 1.96 * se),
        "significant": abs(d.mean()) > 1.96 * se,
        "n":          len(d),
    }
```

And hold yourself to the acceptance sample sizes from [Module 08 §5](Lecture-08.md):

```text
   claim a 1-point acceptance difference    →  ≥ ~8,000 draft tokens per config
   claim a 0.4-point difference             →  ≥ ~47,000 draft tokens per config

   Below that, "2.886 → 2.871" is a coin flip with a decimal point.
```

---

**4. The five invalid comparisons**

These account for most quantization claims you will read — including, if you are not careful, your own.

**4.1 Comparing against a badly-configured baseline**

```text
   "Our NVFP4 build is 3.1× faster than BF16."
   ...where the BF16 baseline used a naive kernel, no CUDA graphs, and batch 1
      while the NVFP4 build had all three.
```

**Fix:** the baseline must be as well-optimized as the candidate. If you tuned one, tune both — or state plainly that you did not.

**4.2 Reporting throughput without context length**

[Module 09](Lecture-09.md): 81.6 tok/s at 0 context, 45.1 tok/s at 262 K, same model. A tok/s number without a context length is not a measurement.

**Fix:** report tok/s at your p50 **and** p99 serving context.

**4.3 Measuring quality at short context only**

Every metric in [Module 08](Lecture-08.md) is typically run at 2–4 K. [Module 07's](Lecture-07.md) RoPE argument predicts Q/K damage *grows* with context. A configuration that passes at 4 K can fail at 262 K, and nothing in a short-context evaluation will warn you.

**Fix:** include a long-context probe in the gate. Always.

**4.4 Comparing checkpoint sizes instead of `B_token`**

```text
   "We cut the model 34 % and got 4 % more throughput. Quantization
    doesn't deliver what it promises."
```

It delivered exactly what the ledger predicted. The 34 % included embeddings and a vision tower that are never read during decode ([Module 04](Lecture-04.md)).

**Fix:** report `B_token`, and report achieved bandwidth as `B_token × tok/s`.

**4.5 Ignoring the acceptance tax**

```text
   "+1.9 % throughput" — while τ fell from 2.792 to 2.546.
```

[Module 10](Lecture-10.md): the byte win was 11.8 % and the acceptance tax consumed 84 % of it. Reporting only the net throughput hides both that the change was worth far more than it delivered *and* that it damaged the model.

**Fix:** report `B_token`, τ, and tok/s together. Always all three.

---

</details>

## 5. 当 bug 让 benchmark 更好看时

这是能躲过上述所有流程的失效模式，因为数字是真的——它们只是测了一套你以为之外的计算。

```text
   Symptoms of a benchmark improved by a bug:
   ───────────────────────────────────────────────────────────────
   ✗ speedup EXCEEDS the byte-ledger prediction        ← physics violated
   ✗ throughput improves and NOTHING got smaller
   ✗ acceptance length goes UP after quantizing the target
   ✗ perplexity improves after quantization
   ✗ long-context results improve while short-context are unchanged
```

每种症状背后的真实原因：

| 症状 | 常见原因 |
|---|---|
| 超过 byte-ledger 给出的界限 | 某些 layer 被静默跳过；shape 不匹配导致退回 no-op |
| 量化 target 之后 acceptance 反而上升 | draft 与 target 意外共享权重 → 二者轻易就一致 |
| 困惑度变好 | 评估被校准数据污染；或者评估跑的是 *参考* 模型 |
| 长上下文变好 | 缓存被静默截断；并未真正用到完整上下文 |
| 什么都没变小却更快 | 输出 token 变短了（EOS 提前发出）——要看生成的 token 数，而不只是速率 |

**防线就是 ledger。** [Module 04](Lecture-04.md) 给出一个 *先验* 界限：

```text
   predicted_tok/s  =  BW_peak × 0.92 / B_token

   If measured > predicted, you have not found an optimization.
   You have found a bug. Physics does not have a fast path.
```

这个检查不存在值得担心的假阴性，成本为零，也正是要在跑实验之前、而不是之后构建 ledger 的原因。

第二个更省事的检查：**始终核对输出 token 数与实际生成文本的抽样。** 一个会立刻吐出 `<eos>` 的模型，tok/s 好看得惊人。

---

## 6. 报告标准

ablation 表里的每个配置都要带上这些字段。缺了任一字段，该行就无法解读：

```yaml
# ─── identity ────────────────────────────────────────────
config_name:      mixed-fp8-nvfp4-v3
git_commit:       a1b2c3d
quant_manifest:   sha256:...          # Module 05 §5

# ─── the model ───────────────────────────────────────────
allocation:       {mlp: NVFP4, o: NVFP4, q: FP8, kv: FP8, lm_head: NVFP4}
B_token_GB:       15.81               # Module 04 — REQUIRED
checkpoint_GB:    20.19               # for reference; NOT the throughput driver

# ─── the environment ─────────────────────────────────────
gpu:              RTX 5090 (sm_120)
driver / cuda:    ...
runtime:          vllm 0.x.y          # Module 03: kernel coverage moves with version
sm_clock_MHz:     2400 (locked)
mem_clock_MHz:    14001 (locked)
temp_steady_C:    68

# ─── the workload ────────────────────────────────────────
context_length:   4096                # Module 09 — REQUIRED
batch_size:       1
draft_depth_K:    3                   # Module 10 — REQUIRED
drafter:          mtp_head @ BF16 (FIXED across all configs)
sampling:         {temperature: 0.7, top_p: 0.95}
prompt_set:       sha256:...
n_draft_tokens:   48000               # Module 08 §5 — REQUIRED for acceptance claims

# ─── results ─────────────────────────────────────────────
tok_s_median:     82.0
tok_s_iqr:        [81.4, 82.7]
predicted_tok_s:  82.0                # from B_token — must BOUND the measurement
acceptance_tau:   2.871
alpha:            0.783
mean_kl:          0.038
p99_kl:           0.21
top1_agreement:   0.984
achieved_BW_pct:  72.3                # B_token × tok/s / peak
```

有三个字段承担了主要作用：**`B_token`** 让速度声明可核查，**`predicted_tok_s`** 让 bug 可被发现，**`n_draft_tokens`** 让 acceptance 声明可信。多数报告三个全都不写。

---

## 7. 实验流程，端到端

```text
   1. WRITE THE HYPOTHESIS DOWN FIRST
      "Moving Q from NVFP4 to FP8 will cost 1.81 GB of B_token (+11 % traffic)
       and recover ≥ 0.02 nats of KL and ≥ 0.15 of acceptance length."
      ↑ Specific and falsifiable. If you cannot write this, you are not running
        an experiment — you are browsing.

   2. PREDICT THE OUTCOME from the ledger and the solver.  (Modules 04, 11)

   3. FIX EVERY OTHER VARIABLE.                             (§2)

   4. RUN INTERLEAVED, WITH ENOUGH SAMPLES.                 (§1, §3)

   5. CHECK AGAINST THE PREDICTION.
      measured ≈ predicted   →  the model holds; you understand the system
      measured ≪ predicted   →  something else is binding; profile   (Module 03)
      measured ≫ predicted   →  BUG. Do not celebrate.               (§5)

   6. REPORT ALL THREE AXES: B_token, tok/s, and behavior. Never one alone.

   7. WRITE THE NEGATIVE RESULTS DOWN TOO.
      "Q → NVFP4 gains 1.9 % and costs 8.8 % acceptance" is one of the most
      valuable findings in this entire course, and it is a negative result.
```

---


<details>
<summary>English original</summary>

**5. When a bug makes the benchmark better**

This is the failure mode that survives every protocol above, because the numbers are real — they are just measuring a different computation than you think.

```text
   Symptoms of a benchmark improved by a bug:
   ───────────────────────────────────────────────────────────────
   ✗ speedup EXCEEDS the byte-ledger prediction        ← physics violated
   ✗ throughput improves and NOTHING got smaller
   ✗ acceptance length goes UP after quantizing the target
   ✗ perplexity improves after quantization
   ✗ long-context results improve while short-context are unchanged
```

Real causes behind each:

| Symptom | Common cause |
|---|---|
| Beats the byte-ledger bound | some layers silently skipped; a shape mismatch fell back to a no-op |
| Acceptance rises after quantizing the target | draft and target accidentally sharing weights → they agree trivially |
| Perplexity improves | evaluation contaminated by calibration data; or the eval is running the *reference* model |
| Long context improves | the cache is silently truncating; you are not attending to the full context |
| Faster with nothing smaller | output tokens got shorter (EOS emitted early) — check tokens generated, not just rate |

**The defence is the ledger.** [Module 04](Lecture-04.md) gives you an *a priori* bound:

```text
   predicted_tok/s  =  BW_peak × 0.92 / B_token

   If measured > predicted, you have not found an optimization.
   You have found a bug. Physics does not have a fast path.
```

That check has no false negatives worth worrying about, costs nothing, and is the reason to build the ledger before running the experiment rather than after.

A second, cheaper check: **always verify output token counts and a sample of the actual generated text.** A model that emits `<eos>` immediately has spectacular tok/s.

---

**6. The reporting standard**

Every configuration in your ablation table carries these fields. If a field is missing, the row is not interpretable:

```yaml
# ─── identity ────────────────────────────────────────────
config_name:      mixed-fp8-nvfp4-v3
git_commit:       a1b2c3d
quant_manifest:   sha256:...          # Module 05 §5

# ─── the model ───────────────────────────────────────────
allocation:       {mlp: NVFP4, o: NVFP4, q: FP8, kv: FP8, lm_head: NVFP4}
B_token_GB:       15.81               # Module 04 — REQUIRED
checkpoint_GB:    20.19               # for reference; NOT the throughput driver

# ─── the environment ─────────────────────────────────────
gpu:              RTX 5090 (sm_120)
driver / cuda:    ...
runtime:          vllm 0.x.y          # Module 03: kernel coverage moves with version
sm_clock_MHz:     2400 (locked)
mem_clock_MHz:    14001 (locked)
temp_steady_C:    68

# ─── the workload ────────────────────────────────────────
context_length:   4096                # Module 09 — REQUIRED
batch_size:       1
draft_depth_K:    3                   # Module 10 — REQUIRED
drafter:          mtp_head @ BF16 (FIXED across all configs)
sampling:         {temperature: 0.7, top_p: 0.95}
prompt_set:       sha256:...
n_draft_tokens:   48000               # Module 08 §5 — REQUIRED for acceptance claims

# ─── results ─────────────────────────────────────────────
tok_s_median:     82.0
tok_s_iqr:        [81.4, 82.7]
predicted_tok_s:  82.0                # from B_token — must BOUND the measurement
acceptance_tau:   2.871
alpha:            0.783
mean_kl:          0.038
p99_kl:           0.21
top1_agreement:   0.984
achieved_BW_pct:  72.3                # B_token × tok/s / peak
```

Three fields do the heavy lifting: **`B_token`** makes the speed claim checkable, **`predicted_tok_s`** makes a bug detectable, and **`n_draft_tokens`** makes the acceptance claim believable. Most reports omit all three.

---

**7. The experiment protocol, end to end**

```text
   1. WRITE THE HYPOTHESIS DOWN FIRST
      "Moving Q from NVFP4 to FP8 will cost 1.81 GB of B_token (+11 % traffic)
       and recover ≥ 0.02 nats of KL and ≥ 0.15 of acceptance length."
      ↑ Specific and falsifiable. If you cannot write this, you are not running
        an experiment — you are browsing.

   2. PREDICT THE OUTCOME from the ledger and the solver.  (Modules 04, 11)

   3. FIX EVERY OTHER VARIABLE.                             (§2)

   4. RUN INTERLEAVED, WITH ENOUGH SAMPLES.                 (§1, §3)

   5. CHECK AGAINST THE PREDICTION.
      measured ≈ predicted   →  the model holds; you understand the system
      measured ≪ predicted   →  something else is binding; profile   (Module 03)
      measured ≫ predicted   →  BUG. Do not celebrate.               (§5)

   6. REPORT ALL THREE AXES: B_token, tok/s, and behavior. Never one alone.

   7. WRITE THE NEGATIVE RESULTS DOWN TOO.
      "Q → NVFP4 gains 1.9 % and costs 8.8 % acceptance" is one of the most
      valuable findings in this entire course, and it is a negative result.
```

---

</details>

## Checkpoint

现在应当能够：

1. 把 GPU 锁定到可复现状态，并在负载下验证。
2. 列出必须保持固定的八个变量，并解释 drafter 那一个。
3. 运行配对比较，并说明某个 delta 是否显著。
4. 说出五种无效比较，以及各自的修正方法。
5. 用 ledger bound 检测伪装成加速的 bug。
6. 为一种配置完整填写 reporting standard。

---

## 交付

拿一个你已经相信的结果——来自本课程或你自己的工作——**在本协议下重跑一遍**。锁定时钟、交错、配对、充分采样、完整报告。

然后诚实回答：**它扛住了吗？** 如果扛住了，你现在有一个能站得住的结果。如果没有，你学到的东西比原来的数字更有价值，而且你是在别人之前发现的。

---

## 截至

* **不随时间变化：** 全部内容。测量纪律不依赖硬件代际。
* **2026 工具链固定项：** `nvidia-smi --lock-gpu-clocks` / `--lock-memory-clocks` 语法；memory-clock 取值因产品而异——查 `SUPPORTED_CLOCKS`，不要照抄示例。
* **相关：** [AI Inference Engineer 2026 — Part 4, Lecture 02](../01-AI推理工程师2026/04-真实引擎优化/Lecture-02.md) 构建了一个无法被钻空子的记分板，而 [Lecture 10](../01-AI推理工程师2026/04-真实引擎优化/Lecture-10.md) 在真实引擎上讲了 bug-改进-benchmark 这类失败。

---

**Next:** [Capstone — TurboQuant →](Lecture-13.md)


<details>
<summary>English original</summary>

**Checkpoint**

You should now be able to:

1. Lock a GPU into a reproducible state and verify it under load.
2. List the eight variables that must be held fixed, and explain the drafter one.
3. Run a paired comparison and state whether a delta is significant.
4. Name the five invalid comparisons and the fix for each.
5. Use the ledger bound to detect a bug masquerading as a speedup.
6. Fill out the reporting standard completely for one configuration.

---

**Ship it**

Take one result you already believe — from this course or your own work — and **re-run it under this protocol**. Locked clocks, interleaved, paired, adequately sampled, fully reported.

Then answer honestly: **did it survive?** If it did, you now have a result you can defend. If it did not, you have learned something more valuable than the original number, and you found it before someone else did.

---

**Current as of**

* **Timeless:** all of it. Measurement discipline does not depend on hardware generation.
* **2026 tooling pins:** `nvidia-smi --lock-gpu-clocks` / `--lock-memory-clocks` syntax; memory-clock values are part-specific — query `SUPPORTED_CLOCKS` rather than copying the example.
* **Related:** [AI Inference Engineer 2026 — Part 4, Lecture 02](../01-AI推理工程师2026/04-真实引擎优化/Lecture-02.md) builds a scoreboard that cannot be gamed, and [Lecture 10](../01-AI推理工程师2026/04-真实引擎优化/Lecture-10.md) covers the bug-improves-the-benchmark failure on a real engine.

---

**Next:** [Capstone — TurboQuant →](Lecture-13.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/Hardware-Aware LLM Quantization/Lecture-12.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/Hardware-Aware%20LLM%20Quantization/Lecture-12.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
