---
title: Module 11 — Hardware-Aware AutoQuant
description: Module 11 — Hardware-Aware AutoQuant
---

# Module 11 — Hardware-Aware AutoQuant

**合集：** [Hardware-Aware LLM Quantization](README.md) | **上一篇：** [← Module 10](Lecture-10.md) | **下一篇：** [Module 12 →](Lecture-12.md)

---

十个模块已经产出了测量结果。本模块把它们变成一个决策。

问题——*哪个张量用哪种格式？*——到目前为止都是临时凑合着回答（“量化 MLP，保留 Q/K 宽”）。它实际上是一个适定的**多选背包问题**，而把它正确地表述出来，能得到再多直觉也找不到的分配方案。本模块结尾的结果就是这门课的核心论点，一句话：**相同的吞吐，行为损伤不到一半，靠的是重新分配精度，而不是增加精度。**

---

## 学习目标

学完本模块后，你应当能够：

1. 将精度分配表述为带有正确目标与约束的约束优化问题。
2. 解释为什么格式集合是**离散**的，以及为什么这是优点而不是限制。
3. 实现一个贪心拉格朗日求解器，并知道它何时最优。
4. 验证求解器所依赖的可加性假设。
5. 读懂求解器的 trace，并依据 Module 04–10 解释其中的每个决策。

---

## 1. 把问题正确地表述出来

```text
   maximize     BW_eff      τ(α)
                ────────  ×  ──────────
                B_token      1 + K·c

   over         f_i ∈ F   for each tensor group i

   subject to   Σ_i  ΔKL_i(f_i)   ≤  ε            behavior budget   (Module 08)
                Σ_i  n_i·b(f_i) + KV(L) ≤ V       VRAM budget       (Module 09)
                F = {BF16, FP8, NVFP4}             hardware-native   (Module 03)

   where        B_token = Σ_{i ∈ streamed}  n_i · b(f_i)             (Module 04)
```

每一项都可回溯到更早的模块。各个部分如下：

| 符号 | 含义 | 来源 |
|---|---|---|
| `n_i` | 组 `i` 中的参数 | Module 04 台账 |
| `b(f)` | 格式 `f` 的 bytes/param | Module 02（BF16 2.0、FP8 1.0、NVFP4 0.5625） |
| `ΔKL_i(f)` | 格式 `f` 在组 `i` 上的行为代价 | Module 07 扫描，Module 08 评分 |
| `ε` | 以 nats 计的行为预算 | 你的产品决策 |
| `τ(α)` | 接受长度 | Module 10 |

**关键的建模选择在于 `F` 是离散的且很小。** [Module 03](Lecture-03.md) 排除了所有非原生格式，从而把连续的“多少 bit？”搜索坍缩成每组的三选一。正是这一点让该问题能在毫秒级精确求解，而不是在 GPU-days 里近似求解。

---

## 2. 为什么贪心在这里有效

这是一个**多选背包问题**：每组恰好选择一种格式。它一般是 NP-hard 的，但两个事实让它在实践中变得容易：

* **实例极小。** 5–8 个组 × 3 种格式。穷举搜索是 `3^8 = 6561` 次评估——不到一秒。**想要可证明的最优性，直接枚举即可。**
* **贪心拉格朗日解近似最优且可解释。** 它按降序处理候选，排序依据是

```text
                    ΔB_token_i(f)        bytes saved
   value_i(f)  =  ─────────────────  =  ───────────────      [GB per nat]
                     ΔKL_i(f)           behavior spent
```

这个比值就是**速度与行为之间的汇率**，用这些单位来读求解器的 trace，就是培养“哪些取舍是好取舍”判断力的方式。

> 为最终答案做枚举；跑贪心是为了*理解*答案。trace 比分配结果更有价值。

---


<details>
<summary>English original</summary>

**Module 11 — Hardware-Aware AutoQuant**

**Collection:** [Hardware-Aware LLM Quantization](README.md) | **Previous:** [← Module 10](Lecture-10.md) | **Next:** [Module 12 →](Lecture-12.md)

---

Ten modules have produced measurements. This one turns them into a decision.

The question — *which tensor gets which format?* — has been answered ad hoc up to now ("quantize MLP, keep Q/K wide"). It is actually a well-posed **multiple-choice knapsack problem**, and posing it properly produces allocations that no amount of intuition finds. The result at the end of this module is the course's thesis in a single line: **the same throughput at less than half the behavioral damage, by reallocating precision rather than adding more of it.**

---

**Learning objectives**

By the end of this module you should be able to:

1. State precision allocation as a constrained optimization with the right objective and constraints.
2. Explain why the format set is **discrete** and why that is a feature, not a limitation.
3. Implement a greedy Lagrangian solver and know when it is optimal.
4. Validate the additivity assumption the solver depends on.
5. Read a solver trace and explain each decision from Modules 04–10.

---

**1. The problem, stated properly**

```text
   maximize     BW_eff      τ(α)
                ────────  ×  ──────────
                B_token      1 + K·c

   over         f_i ∈ F   for each tensor group i

   subject to   Σ_i  ΔKL_i(f_i)   ≤  ε            behavior budget   (Module 08)
                Σ_i  n_i·b(f_i) + KV(L) ≤ V       VRAM budget       (Module 09)
                F = {BF16, FP8, NVFP4}             hardware-native   (Module 03)

   where        B_token = Σ_{i ∈ streamed}  n_i · b(f_i)             (Module 04)
```

Every term traces to an earlier module. The pieces:

| Symbol | Meaning | Source |
|---|---|---|
| `n_i` | parameters in group `i` | Module 04 ledger |
| `b(f)` | bytes/param for format `f` | Module 02 (BF16 2.0, FP8 1.0, NVFP4 0.5625) |
| `ΔKL_i(f)` | behavioral cost of format `f` on group `i` | Module 07 sweep, Module 08 grading |
| `ε` | behavior budget in nats | your product decision |
| `τ(α)` | acceptance length | Module 10 |

**The critical modelling choice is that `F` is discrete and small.** [Module 03](Lecture-03.md) eliminated every non-native format, which collapses a continuous "how many bits?" search into a three-way choice per group. That is what makes the problem exactly solvable in milliseconds instead of approximately solvable in GPU-days.

---

**2. Why greedy works here**

This is a **multiple-choice knapsack**: each group picks exactly one format. It is NP-hard in general, but two facts make it easy in practice:

* **The instance is tiny.** 5–8 groups × 3 formats. Exhaustive search is `3^8 = 6561` evaluations — under a second. **Just enumerate it if you want provable optimality.**
* **The greedy Lagrangian solution is near-optimal and interpretable.** It processes candidates in descending order of

```text
                    ΔB_token_i(f)        bytes saved
   value_i(f)  =  ─────────────────  =  ───────────────      [GB per nat]
                     ΔKL_i(f)           behavior spent
```

That ratio is the **exchange rate between speed and behavior**, and reading the solver's trace in those units is how you develop judgement about which trades are good ones.

> Enumerate for the final answer; run greedy to *understand* the answer. The trace is more valuable than the allocation.

---

</details>

## 3. 求解器

```python
BPP = {"BF16": 2.0, "FP8": 1.0, "NVFP4": 0.5625}     # Module 02

def allocate(groups, budget_kl, norms_GB=0.06, formats=("BF16", "FP8", "NVFP4")):
    """groups: {name: (params_billions, {format: dKL_vs_BF16})}
       Greedy Lagrangian over a multiple-choice knapsack."""
    alloc = {g: "BF16" for g in groups}
    b_token = lambda a: sum(groups[g][0] * BPP[f] for g, f in a.items()) + norms_GB
    total_kl = lambda a: sum(groups[g][1][f] for g, f in a.items())

    trace = []
    while True:
        best = None
        for g, (n, kls) in groups.items():
            for f in formats:
                if BPP[f] >= BPP[alloc[g]]:
                    continue                                  # only downgrades
                dB = n * (BPP[alloc[g]] - BPP[f])             # GB saved
                dK = kls[f] - kls[alloc[g]]                   # nats spent
                if total_kl(alloc) + dK > budget_kl:
                    continue                                  # would blow the budget
                value = dB / max(dK, 1e-9)
                if best is None or value > best[0]:
                    best = (value, g, f, dB, dK)
        if best is None:
            break                                             # nothing affordable left
        value, g, f, dB, dK = best
        alloc[g] = f
        trace.append(dict(group=g, fmt=f, dB=dB, dK=dK, value=value,
                          b_token=b_token(alloc), kl=total_kl(alloc)))
    return alloc, trace
```

任何生产版本都应具备的三道护栏：

```python
FORBIDDEN = {
    # Module 03: no native path on sm_120 → never propose it
    "any": {"INT3", "INT2", "FP6_nonnative"},
    # Module 07: measured, not assumed — but a sane default floor
    "k_proj": {"NVFP4"},
    # Module 10: the drafter's only job is agreeing with the target
    "mtp_head": {"NVFP4"},
}
```

---

## 4. 一次完整的分配算例

输入：各 group 大小取自 [Module 04](Lecture-04.md) 的重建；`ΔKL` 的取值，其 *排序* 沿用 [Module 07](Lecture-07.md) 中确立的（K > Q ≫ MLP > lm_head > O > V）。**下面的量级仅作示意 —— 你必须测量自己的数据。** 预算为 `ε = 0.05` nats。

| 组 | 参数量 | ΔKL → FP8 | ΔKL → NVFP4 |
|---|---:|---:|---:|
| MLP | 16.36 B | 0.0015 | 0.012 |
| O | 3.22 B | 0.0008 | 0.006 |
| Q | 3.22 B | 0.0040 | 0.030 |
| K + V | 0.81 B | 0.0060 | 0.045 |
| lm_head | 1.27 B | 0.0012 | 0.010 |

**求解器 trace**（从全 BF16 出发，`B_token = 49.81 GB`）：

| 步骤 | 动作 | 节省 | 代价 | `B_token` | Σ KL | **价值（GB/nat）** |
|---:|---|---:|---:|---:|---:|---:|
| 1 | MLP → FP8 | −16.36 GB | +0.0015 | 33.45 | 0.0015 | **10,907** |
| 2 | O → FP8 | −3.22 GB | +0.0008 | 30.23 | 0.0023 | 4,025 |
| 3 | lm_head → FP8 | −1.27 GB | +0.0012 | 28.96 | 0.0035 | 1,058 |
| 4 | Q → FP8 | −3.22 GB | +0.0040 | 25.74 | 0.0075 | 805 |
| 5 | MLP → NVFP4 | −7.16 GB | +0.0105 | 18.58 | 0.0180 | 682 |
| 6 | O → NVFP4 | −1.41 GB | +0.0052 | 17.18 | 0.0232 | 271 |
| 7 | K+V → FP8 | −0.81 GB | +0.0060 | 16.37 | 0.0292 | 134 |
| 8 | lm_head → NVFP4 | −0.56 GB | +0.0088 | 15.81 | 0.0380 | 63 |

```text
   FINAL:  MLP → NVFP4   O → NVFP4   lm_head → NVFP4   Q → FP8   K+V → FP8

   B_token  =  15.81 GB        Σ ΔKL  =  0.0380
```

### 真正重要的对比

| 配置 | `B_token` | Σ ΔKL | tok/s @ 1296 GB/s |
|---|---:|---:|---:|
| 全 BF16 | 49.81 GB | 0.000 | 26.0 |
| **已发布**（主体 NVFP4，`lm_head` BF16） | **15.88 GB** | **0.093** | **81.6** |
| **求解器**（混合） | **15.81 GB** | **0.038** | **82.0** |

```text
   ┌──────────────────────────────────────────────────────────────────┐
   │   Same throughput (82.0 vs 81.6 tok/s).                          │
   │   LESS THAN HALF the behavioral damage (0.038 vs 0.093 nats).    │
   │   Zero additional hardware. Zero additional bits.                 │
   │   Pure REALLOCATION.                                              │
   └──────────────────────────────────────────────────────────────────┘
```

**整门课都浓缩在这一张表里。** 「格式统一」的本能 ——「主体是 NVFP4，那就把主体量化」—— 把行为预算花在最承受不起的 tensor（Q、K）上，同时让 2.54 GB 的 BF16 `lm_head` 一直卡在关键路径上。


<details>
<summary>English original</summary>

**3. The solver**

```python
BPP = {"BF16": 2.0, "FP8": 1.0, "NVFP4": 0.5625}     # Module 02

def allocate(groups, budget_kl, norms_GB=0.06, formats=("BF16", "FP8", "NVFP4")):
    """groups: {name: (params_billions, {format: dKL_vs_BF16})}
       Greedy Lagrangian over a multiple-choice knapsack."""
    alloc = {g: "BF16" for g in groups}
    b_token = lambda a: sum(groups[g][0] * BPP[f] for g, f in a.items()) + norms_GB
    total_kl = lambda a: sum(groups[g][1][f] for g, f in a.items())

    trace = []
    while True:
        best = None
        for g, (n, kls) in groups.items():
            for f in formats:
                if BPP[f] >= BPP[alloc[g]]:
                    continue                                  # only downgrades
                dB = n * (BPP[alloc[g]] - BPP[f])             # GB saved
                dK = kls[f] - kls[alloc[g]]                   # nats spent
                if total_kl(alloc) + dK > budget_kl:
                    continue                                  # would blow the budget
                value = dB / max(dK, 1e-9)
                if best is None or value > best[0]:
                    best = (value, g, f, dB, dK)
        if best is None:
            break                                             # nothing affordable left
        value, g, f, dB, dK = best
        alloc[g] = f
        trace.append(dict(group=g, fmt=f, dB=dB, dK=dK, value=value,
                          b_token=b_token(alloc), kl=total_kl(alloc)))
    return alloc, trace
```

Three guard rails that belong in any production version:

```python
FORBIDDEN = {
    # Module 03: no native path on sm_120 → never propose it
    "any": {"INT3", "INT2", "FP6_nonnative"},
    # Module 07: measured, not assumed — but a sane default floor
    "k_proj": {"NVFP4"},
    # Module 10: the drafter's only job is agreeing with the target
    "mtp_head": {"NVFP4"},
}
```

---

**4. A worked allocation**

Inputs: group sizes from the [Module 04](Lecture-04.md) reconstruction; `ΔKL` values with the *ordering* established in [Module 07](Lecture-07.md) (K > Q ≫ MLP > lm_head > O > V). **The magnitudes below are illustrative — you must measure your own.** Budget `ε = 0.05` nats.

| Group | Params | ΔKL → FP8 | ΔKL → NVFP4 |
|---|---:|---:|---:|
| MLP | 16.36 B | 0.0015 | 0.012 |
| O | 3.22 B | 0.0008 | 0.006 |
| Q | 3.22 B | 0.0040 | 0.030 |
| K + V | 0.81 B | 0.0060 | 0.045 |
| lm_head | 1.27 B | 0.0012 | 0.010 |

**Solver trace** (starting from all-BF16, `B_token = 49.81 GB`):

| Step | Move | Saved | Cost | `B_token` | Σ KL | **Value (GB/nat)** |
|---:|---|---:|---:|---:|---:|---:|
| 1 | MLP → FP8 | −16.36 GB | +0.0015 | 33.45 | 0.0015 | **10,907** |
| 2 | O → FP8 | −3.22 GB | +0.0008 | 30.23 | 0.0023 | 4,025 |
| 3 | lm_head → FP8 | −1.27 GB | +0.0012 | 28.96 | 0.0035 | 1,058 |
| 4 | Q → FP8 | −3.22 GB | +0.0040 | 25.74 | 0.0075 | 805 |
| 5 | MLP → NVFP4 | −7.16 GB | +0.0105 | 18.58 | 0.0180 | 682 |
| 6 | O → NVFP4 | −1.41 GB | +0.0052 | 17.18 | 0.0232 | 271 |
| 7 | K+V → FP8 | −0.81 GB | +0.0060 | 16.37 | 0.0292 | 134 |
| 8 | lm_head → NVFP4 | −0.56 GB | +0.0088 | 15.81 | 0.0380 | 63 |

```text
   FINAL:  MLP → NVFP4   O → NVFP4   lm_head → NVFP4   Q → FP8   K+V → FP8

   B_token  =  15.81 GB        Σ ΔKL  =  0.0380
```

**The comparison that matters**

| Configuration | `B_token` | Σ ΔKL | tok/s @ 1296 GB/s |
|---|---:|---:|---:|
| All BF16 | 49.81 GB | 0.000 | 26.0 |
| **Shipped** (body NVFP4, `lm_head` BF16) | **15.88 GB** | **0.093** | **81.6** |
| **Solver** (mixed) | **15.81 GB** | **0.038** | **82.0** |

```text
   ┌──────────────────────────────────────────────────────────────────┐
   │   Same throughput (82.0 vs 81.6 tok/s).                          │
   │   LESS THAN HALF the behavioral damage (0.038 vs 0.093 nats).    │
   │   Zero additional hardware. Zero additional bits.                 │
   │   Pure REALLOCATION.                                              │
   └──────────────────────────────────────────────────────────────────┘
```

**This is the entire course in one table.** The uniform-format instinct — "the body is NVFP4, so quantize the body" — spends its behavior budget on the tensors least able to afford it (Q, K) while leaving a 2.54 GB BF16 `lm_head` sitting on the critical path.

</details>

### 解读 trace

每一个决策都可以回溯到更早的模块：

* **Steps 1–2（MLP、O → FP8）几乎零代价**——10,907 与 4,025 GB/nat。它们就是 [Module 07 的](Lecture-07.md) Path A 张量：误差随 `η/√K` 平均化下降。立刻采纳。
* **Step 3 把 `lm_head` 排到 Q 前面**——正是 [Module 04 的](Lecture-04.md) 结论。它占 `B_token` 的 16 %，且在已发布的配置中未被触碰。
* **Step 5（MLP → NVFP4）是最大的单笔字节收益**，−7.16 GB，且仍能返回 682 GB/nat。MLP 占账本的 58 %；它应当是模型中量化最激进的张量。
* **Q 与 K+V 停在 FP8，从未到达 NVFP4。** 它们的 NVFP4 价值比（约 107 与约 18 GB/nat）远低于其他所有项，因此预算在求解器触及它们之前就已耗尽。**求解器仅凭数字就重新发现了 [Module 07 的](Lecture-07.md) 规则**——没有人告诉它 Q/K 是敏感的。
* **Step 8 把预算的最后一点花在 `lm_head` → NVFP4 上**，为 63 GB/nat，这是一笔边际交易。在投机式部署中，你很可能会停在 step 7：[Module 10 的](Lecture-10.md) 接受税意味着最后那 0.56 GB 有一部分会通过 α 偿还。

---

## 5. 可加性假设，以及如何检验它

求解器假设 **`ΔKL` 的值在各组之间可加**：

```text
   ΔKL(quantize A and B)  ≈  ΔKL(A)  +  ΔKL(B)
```

这是一阶近似，并不完全成立——误差可能会累积（受损的 Q 会放大受损的 K），也可能部分抵消。**要验证它，不要假设它：**

```python
def check_additivity(groups, fmt, budget_pairs=10):
    """Compare measured joint KL against the additive prediction."""
    solo = {g: measure_kl(quantize(model, {g: fmt})) for g in groups}
    rows = []
    for a, b in itertools.islice(itertools.combinations(groups, 2), budget_pairs):
        joint     = measure_kl(quantize(model, {a: fmt, b: fmt}))
        predicted = solo[a] + solo[b]
        rows.append((a, b, joint, predicted, joint / predicted))
    return rows                     # ratio ≈ 1.0 → additive; > 1.2 → compounding
```

| 观测到的比值 | 解读 | 动作 |
|---|---|---|
| 0.9 – 1.1 | 可加 | 贪心求解器可信 |
| > 1.2 | 误差累积 | 调小 `ε`，或用实测的联合代价做枚举 |
| < 0.8 | 误差部分抵消 | 你偏保守；可以多花预算 |

在实践中，可加性对处于*不同* layer 的张量成立得很好，对处于*同一* attention block 的张量则较差。由于 Q 与 K 位于同一个 block，而它们又是你最希望正确建模的两个，**要显式测量 Q/K 的联合代价**，而不是把二者相加。

---

## 6. 把预算换算成产品口径

`ε` 不是一个可以推导出来的数字——它是一个产品决策。把它锚定到 [Module 08 的](Lecture-08.md) 阈值上：

| `ε`（nats） | 预期 top-1 一致率 | 适用场景 |
|---:|---|---|
| 0.01 | > 99 % | 质量关键；与参考无法区分 |
| 0.05 | ~97–99 % | 通用推理服务——**默认值** |
| 0.15 | ~93–97 % | 吞吐优先、可容忍的工作负载 |

然后对它做扫描。`B_token(ε)` 曲线才是你的产品团队真正能据以推理的产物：

```text
   B_token
     ▲
  50 │●  all BF16
     │ ╲
     │  ╲
     │   ╲___
  20 │       ╲──●────●─────●──────────────  knee: the cheap wins are gone
     │            0.02  0.04   0.10
     └──────────────────────────────────────▶  ε (nats)

   Ship at the KNEE. Past it you are buying single-digit
   throughput percentages with real behavioral cost.
```

---

## 检查点

现在你应当能够：

1. 陈述分配问题的目标、变量以及全部三个约束。
2. 解释为什么离散的原生格式集合使问题变得可解。
3. 实现贪心求解器，并说明何时应改用枚举。
4. 解读以 GB/nat 计的价值比，并用它来接受或拒绝一次移动。
5. 检验可加性假设，并对结果作出反应。
6. 解释为什么在相同的 `B_token` 下，求解器的分配优于均匀分配。

---

## 上线

在你的模型上跑完整的流水线：

1. 把各个组记入账本（[Module 04](Lecture-04.md)）。
2. 测量每个组、每种格式的 `ΔKL`（[Modules 07](Lecture-07.md), [08](Lecture-08.md)）。
3. 对价值最高的 5 对检查可加性（§5）。
4. 求解，并**用枚举确认**贪心答案是最优的。
5. 扫描 `ε` 并画出 `B_token(ε)` 曲线；标出拐点。
6. 构建胜出的配置，并**用实测校验预测的 `B_token` 与 tok/s。**

Step 6 不是可选项。如果构建出的产物与求解器的预测不符，那么要么是账本错了，要么是工具链的实际导出错了——而查清是哪一个，比这次分配本身更有价值。

---


<details>
<summary>English original</summary>

**Reading the trace**

Every decision traces back to an earlier module:

* **Steps 1–2 (MLP, O → FP8) are nearly free** — 10,907 and 4,025 GB/nat. These are [Module 07's](Lecture-07.md) Path A tensors: error averages down as `η/√K`. Take them immediately.
* **Step 3 puts `lm_head` on the list before Q** — exactly [Module 04's](Lecture-04.md) finding. It is 16 % of `B_token` and was untouched in the shipped config.
* **Step 5 (MLP → NVFP4) is the single biggest byte win**, −7.16 GB, and still returns 682 GB/nat. The MLP is 58 % of the ledger; it should be the most aggressively quantized tensor in the model.
* **Q and K+V stop at FP8 and never reach NVFP4.** Their NVFP4 value ratios (~107 and ~18 GB/nat) are far below everything else, so the budget is exhausted before the solver reaches them. **The solver rediscovers [Module 07's](Lecture-07.md) rule from the numbers alone** — nobody told it that Q/K are sensitive.
* **Step 8 spends the last of the budget on `lm_head` → NVFP4** at 63 GB/nat, which is a marginal trade. In a speculative deployment you would likely stop at step 7: [Module 10's](Lecture-10.md) acceptance tax means the final 0.56 GB is partly paid back through α.

---

**5. The additivity assumption, and how to check it**

The solver assumes **`ΔKL` values add across groups**:

```text
   ΔKL(quantize A and B)  ≈  ΔKL(A)  +  ΔKL(B)
```

This is a first-order approximation and it is not exactly true — errors can compound (a damaged Q amplifies a damaged K) or partially cancel. **Verify it, do not assume it:**

```python
def check_additivity(groups, fmt, budget_pairs=10):
    """Compare measured joint KL against the additive prediction."""
    solo = {g: measure_kl(quantize(model, {g: fmt})) for g in groups}
    rows = []
    for a, b in itertools.islice(itertools.combinations(groups, 2), budget_pairs):
        joint     = measure_kl(quantize(model, {a: fmt, b: fmt}))
        predicted = solo[a] + solo[b]
        rows.append((a, b, joint, predicted, joint / predicted))
    return rows                     # ratio ≈ 1.0 → additive; > 1.2 → compounding
```

| Observed ratio | Interpretation | Action |
|---|---|---|
| 0.9 – 1.1 | additive | greedy solver is trustworthy |
| > 1.2 | errors compound | shrink `ε`, or enumerate with measured joint costs |
| < 0.8 | errors partially cancel | you are being conservative; you can spend more |

In practice additivity holds well for tensors in *different* layers and less well for tensors in the *same* attention block. Since Q and K live in the same block and are the two you most want to model correctly, **measure the Q/K joint cost explicitly** rather than summing.

---

**6. Putting the budget in product terms**

`ε` is not a number you can derive — it is a product decision. Anchor it to [Module 08's](Lecture-08.md) thresholds:

| `ε` (nats) | Expected top-1 agreement | Use case |
|---:|---|---|
| 0.01 | > 99 % | quality-critical; indistinguishable from reference |
| 0.05 | ~97–99 % | general serving — **the default** |
| 0.15 | ~93–97 % | throughput-first, tolerant workloads |

Then sweep it. The `B_token(ε)` curve is the artifact your product team can actually reason about:

```text
   B_token
     ▲
  50 │●  all BF16
     │ ╲
     │  ╲
     │   ╲___
  20 │       ╲──●────●─────●──────────────  knee: the cheap wins are gone
     │            0.02  0.04   0.10
     └──────────────────────────────────────▶  ε (nats)

   Ship at the KNEE. Past it you are buying single-digit
   throughput percentages with real behavioral cost.
```

---

**Checkpoint**

You should now be able to:

1. State the allocation problem with objective, variables, and all three constraints.
2. Explain why a discrete native-format set makes the problem tractable.
3. Implement the greedy solver and say when to enumerate instead.
4. Interpret a value ratio in GB/nat and use it to accept or reject a move.
5. Test the additivity assumption and react to the result.
6. Explain why the solver's allocation beats the uniform one at equal `B_token`.

---

**Ship it**

Run the full pipeline on your model:

1. Ledger the groups ([Module 04](Lecture-04.md)).
2. Measure `ΔKL` per group per format ([Modules 07](Lecture-07.md), [08](Lecture-08.md)).
3. Check additivity on the 5 highest-value pairs (§5).
4. Solve, and **enumerate to confirm** the greedy answer is optimal.
5. Sweep `ε` and plot the `B_token(ε)` curve; mark the knee.
6. Build the winning configuration and **verify the predicted `B_token` and tok/s against measurement.**

Step 6 is not optional. If the built artifact does not match the solver's prediction, either the ledger or the toolkit's actual export is wrong — and finding out which is worth more than the allocation.

---

</details>

## 截至

* **不随时间变化：** 优化命题、多选背包的表述框架、价值比交换率、可加性检验。
* **案例研究锚点：** 来自模块 04 重建的分组规模。**§4 中的 `ΔKL` 表仅作示意** —— 其选取是为了复现模块 07 中测得的排序，而非实测值。结论（在相同 `B_token` 下，重新分配优于均匀量化）对量级是稳健的；具体分配方案则不然。
* **刷新面：** `F` 由 [模块 03](Lecture-03.md) 定义。若未来某个 runtime 在 `sm_120` 上加入原生 FP6 路径，将其加入 `F` 并重新求解 —— 求解器不变。

---

**下一篇：** [模块 12 — 研究方法论 →](Lecture-12.md)


<details>
<summary>English original</summary>

**Current as of**

* **Timeless:** the optimization statement, the multiple-choice knapsack framing, the value-ratio exchange rate, the additivity test.
* **Case-study pins:** group sizes from the Module 04 reconstruction. **The `ΔKL` table in §4 is illustrative** — chosen to reproduce the ordering measured in Module 07, not measured values. The conclusion (reallocation beats uniform quantization at equal `B_token`) is robust to the magnitudes; the specific allocation is not.
* **Refresh surface:** `F` is defined by [Module 03](Lecture-03.md). If a future runtime adds a native FP6 path on `sm_120`, add it to `F` and re-solve — the solver is unchanged.

---

**Next:** [Module 12 — Research Methodology →](Lecture-12.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/Hardware-Aware LLM Quantization/Lecture-11.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/Hardware-Aware%20LLM%20Quantization/Lecture-11.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
