---
title: Capstone —— TurboQuant：构建策略引擎
description: Capstone —— TurboQuant：构建策略引擎
---

# Capstone —— TurboQuant：构建策略引擎

**合集：** [硬件感知 LLM 量化](README.md) | **上一篇：** [← Module 12](Lecture-12.md) | **下一篇：** [课程索引](README.md)

---

十二个模块产出了一套方法。这个 capstone 把它变成**其他工程师能在你从未见过的模型上运行的可复用子系统** —— 路线图[产物阶梯](../../../Curriculum-Authoring-Guide.md)上的 Level 5。

**要打败的 benchmark：** 在 RTX 5090 上、接受长度 `2.886` 时的 `155.75 tok/s`，来自已发布的 [DSpark NVFP4 build](https://huggingface.co/gittensor-model-hub/Qwen3.8-27B-DSpark-NVFP4)。不是靠把文件缩小。而是靠**正确地分配精度，并修复账本指出的真正起约束作用之处。**

---

## 1. 余量在哪里

在构建任何东西之前，先用 Module 04 和 10 的工具把目标分解开。从非投机测量出发：

```text
   BW_eff / B_token  =  1296 / 15.88  =  81.6 tok/s        (non-speculative, verified)
```

然后把投机加速模型对照观测到的 155.75 反解：

```text
                 τ                              81.6 × 2.886
   tok/s  =  base × ──────────   ⟹   1 + K·c  =  ────────────  =  1.512
                 1 + K·c                            155.75

   at K = 3   ⟹   c  =  0.171
```

现在把它与字节账本所说的 drafter *应该* 付出的代价做对比：

```text
   MTP head = 0.85 GB,  target = 15.88 GB   ⟹   c_bandwidth  =  0.054

   measured c = 0.171     ≈  3.2× MORE EXPENSIVE than its bytes justify
```

```text
   ┌──────────────────────────────────────────────────────────────────┐
   │  The draft step costs 17 % of a target pass while moving only     │
   │  5 % of the bytes. That gap is launch overhead and kernel         │
   │  inefficiency in the drafting path — not physics.                 │
   │                                                                   │
   │  Fixing it alone:  81.6 × 2.886 / (1 + 3×0.054)  =  202.9 tok/s   │
   └──────────────────────────────────────────────────────────────────┘
```

**这是一个仅靠算术推导出的 +30 % 发现，还没碰过任何一个权重。** 这也是账本第二次发现并非量化问题的余量 —— 第一次是 [Module 04 的](Lecture-04.md) 72 % 对 92 % 带宽差距。

### 分阶段目标阶梯

| 阶段 | 变更 | 系数 | 累计 |
|---|---|---:|---:|
| — | 基线 | — | **155.8 tok/s** |
| 1 | kernel 效率：72 % → 92 % 实测 BW（[Mod 03](Lecture-03.md)、[04](Lecture-04.md)） | ×1.273 | 198.3 |
| 2 | `lm_head` BF16 → NVFP4（[Mod 04](Lecture-04.md)、[11](Lecture-11.md)） | ×1.130 | 224.1 |
| 3 | drafter 路径：`c` 0.171 → 0.08（[Mod 10](Lecture-10.md)） | ×1.220 | 273.5 |
| 4 | 重新分配恢复接受率（[Mod 11](Lecture-11.md)） | ×1.022 | **279.5** |

**把承诺目标定在阶段 2（~224 tok/s），把挑战目标定在阶段 4（~280）。** 阶段 1 和 3 属于 kernel 工作，可能受限于你的 runtime 能暴露什么；阶段 2 和 4 无论如何都归你。

注意*没有*出现在这个阶梯上的东西：进一步量化 Transformer 主体。它已经是 NVFP4，其中 58 % 是 MLP，而这已经是正确的选择，且降到 4 bit 以下被 [Module 03](Lecture-03.md) 排除。**剩下的收益在别处，而这正是账本告诉你的。**

---


<details>
<summary>English original</summary>

**Capstone — TurboQuant: Build the Policy Engine**

**Collection:** [Hardware-Aware LLM Quantization](README.md) | **Previous:** [← Module 12](Lecture-12.md) | **Next:** [Course index](README.md)

---

The twelve modules produced a method. This capstone turns it into a **reusable subsystem another engineer can run on a model you have never seen** — Level 5 on the roadmap's [artifact ladder](../../../Curriculum-Authoring-Guide.md).

**The benchmark to beat:** `155.75 tok/s` at acceptance length `2.886` on an RTX 5090, from the published [DSpark NVFP4 build](https://huggingface.co/gittensor-model-hub/Qwen3.8-27B-DSpark-NVFP4). Not by shrinking the file. By **allocating precision correctly and fixing what the ledger says is actually binding.**

---

**1. Where the headroom is**

Before building anything, decompose the target with the tools from Modules 04 and 10. Start from the non-speculative measurement:

```text
   BW_eff / B_token  =  1296 / 15.88  =  81.6 tok/s        (non-speculative, verified)
```

Then invert the speculative speedup model against the observed 155.75:

```text
                 τ                              81.6 × 2.886
   tok/s  =  base × ──────────   ⟹   1 + K·c  =  ────────────  =  1.512
                 1 + K·c                            155.75

   at K = 3   ⟹   c  =  0.171
```

Now compare that against what the byte ledger says the drafter *should* cost:

```text
   MTP head = 0.85 GB,  target = 15.88 GB   ⟹   c_bandwidth  =  0.054

   measured c = 0.171     ≈  3.2× MORE EXPENSIVE than its bytes justify
```

```text
   ┌──────────────────────────────────────────────────────────────────┐
   │  The draft step costs 17 % of a target pass while moving only     │
   │  5 % of the bytes. That gap is launch overhead and kernel         │
   │  inefficiency in the drafting path — not physics.                 │
   │                                                                   │
   │  Fixing it alone:  81.6 × 2.886 / (1 + 3×0.054)  =  202.9 tok/s   │
   └──────────────────────────────────────────────────────────────────┘
```

**That is a +30 % finding derived from arithmetic, before touching a single weight.** It is also the second time the ledger has found headroom that is not a quantization problem — the first was [Module 04's](Lecture-04.md) 72 %-vs-92 % bandwidth gap.

**The staged target ladder**

| Stage | Change | Factor | Cumulative |
|---|---|---:|---:|
| — | baseline | — | **155.8 tok/s** |
| 1 | kernel efficiency: 72 % → 92 % achieved BW ([Mod 03](Lecture-03.md), [04](Lecture-04.md)) | ×1.273 | 198.3 |
| 2 | `lm_head` BF16 → NVFP4 ([Mod 04](Lecture-04.md), [11](Lecture-11.md)) | ×1.130 | 224.1 |
| 3 | drafter path: `c` 0.171 → 0.08 ([Mod 10](Lecture-10.md)) | ×1.220 | 273.5 |
| 4 | reallocation recovers acceptance ([Mod 11](Lecture-11.md)) | ×1.022 | **279.5** |

**Set your commitment at Stage 2 (~224 tok/s) and your stretch at Stage 4 (~280).** Stages 1 and 3 are kernel work and may be gated by what your runtime exposes; stages 2 and 4 are yours regardless.

Note what is *not* on this ladder: quantizing the transformer body further. It is already NVFP4, it is 58 % MLP which is already the right choice, and going below 4 bits is ruled out by [Module 03](Lecture-03.md). **The remaining wins are elsewhere, and the ledger is what told you so.**

---

</details>

## 2. 系统架构

```text
   ┌─────────────────────────────────────────────────────────────────────┐
   │                            TurboQuant                                │
   ├─────────────────────────────────────────────────────────────────────┤
   │                                                                      │
   │   [1] LEDGER          checkpoint ──▶ traffic classes, B_token,      │
   │       (Module 04)                     predicted ceiling, gap check   │
   │              │                                                       │
   │              ▼                                                       │
   │   [2] PROBE           per-group activation stats  (Module 06)        │
   │                       per-group ΔKL / Δτ sweep     (Modules 07, 08)  │
   │                       additivity check             (Module 11 §5)    │
   │              │                                                       │
   │              ▼                                                       │
   │   [3] SOLVER          multiple-choice knapsack over {BF16,FP8,NVFP4} │
   │       (Module 11)     hardware guard rails         (Module 03)       │
   │              │        ε sweep → B_token(ε) curve                     │
   │              ▼                                                       │
   │   [4] BUILDER         emit the quantized checkpoint + manifest       │
   │       (Module 05)     AWQ / GPTQ per the error-mode diagnosis        │
   │              │                                                       │
   │              ▼                                                       │
   │   [5] VERIFIER        locked clocks, interleaved, paired  (Mod 12)   │
   │       (Modules 8,12)  tok/s · τ · α · KL · p99 · agreement           │
   │                       measured vs PREDICTED ── bug check             │
   └─────────────────────────────────────────────────────────────────────┘
```

各阶段之间的契约正是其可复用性的来源：

```python
@dataclass
class Ledger:      # [1] → [3]
    groups: dict[str, int]          # name → parameter count
    traffic_class: dict[str, str]   # A_streamed / B_gathered / C_conditional / D_state
    b_token_GB: float
    predicted_tps: float
    measured_tps: float | None
    gap: float | None               # predicted / measured — >1.2 ⇒ profile, do not quantize

@dataclass
class Sensitivity: # [2] → [3]
    d_kl:   dict[tuple[str, str], float]   # (group, format) → ΔKL vs BF16
    d_tau:  dict[tuple[str, str], float]   # (group, format) → Δ acceptance length
    additivity_ratio: float                # ~1.0 ⇒ solver assumption holds

@dataclass
class Allocation:  # [3] → [4]
    fmt: dict[str, str]
    predicted_b_token_GB: float
    predicted_kl: float
    predicted_tps: float            # [5] MUST check against this
```

---

## 3. 构建顺序

### 阶段 1 — 账本与差距检查 *(始终从这里开始)*

实现 [Module 04 的](Lecture-04.md) 分析器。计算 `B_token`、预测上限与差距。

```text
   gap = predicted / measured

   ≈ 1.0   →  you are at the bandwidth wall; proceed to Phase 2
   > 1.2   →  STOP. Profile kernels first (Module 03 §5).
              For the case study, gap = 1.27 — Stage 1 of the ladder.
```

**交付物：** 账本表、差距，以及一份写明下一步动作的书面判定。

### 阶段 2 — 探针

运行 [Module 06 的](Lecture-06.md) 激活值 profiler（诊断 → 方法选择）与 [Module 07 的](Lecture-07.md) 留一扫描（逐组 ΔKL 与 Δτ）。在你价值最高的五组上检查可加性。

**交付物：** 敏感度表，其中 Q/K 需在短上下文与最大上下文下均测量（即 [Module 07 §2](Lecture-07.md) 的预测）。

### 阶段 3 — 求解

用你的实测值运行 [Module 11](Lecture-11.md) 求解器。枚举所有 `3^n` 以确认贪心最优性。扫描 `ε` 并绘制 `B_token(ε)`；标出拐点。

**交付物：** 分配方案、以 GB/nat 为单位的求解器 trace、`B_token(ε)` 曲线。

### 阶段 4 — 构建

用 [Module 05 §5](Lecture-05.md) 的 manifest 导出检查点。**验证构建产物的实际 `B_token` 与求解器的预测一致** —— 工具链静默忽略逐组格式请求的情况很常见，会让下游一切失效。

**交付物：** 检查点、manifest、预测值 vs 实际值 `B_token`。

### 阶段 5 — 验证

运行 [Module 12 的](Lecture-12.md) 协议。锁定时钟、交错、配对、每个配置 ≥8,000 个 draft token、完整上报标准。

**交付物：** 消融表，以及 bug 检查（`measured ≫ predicted` ⇒ bug）。


<details>
<summary>English original</summary>

**2. System architecture**

```text
   ┌─────────────────────────────────────────────────────────────────────┐
   │                            TurboQuant                                │
   ├─────────────────────────────────────────────────────────────────────┤
   │                                                                      │
   │   [1] LEDGER          checkpoint ──▶ traffic classes, B_token,      │
   │       (Module 04)                     predicted ceiling, gap check   │
   │              │                                                       │
   │              ▼                                                       │
   │   [2] PROBE           per-group activation stats  (Module 06)        │
   │                       per-group ΔKL / Δτ sweep     (Modules 07, 08)  │
   │                       additivity check             (Module 11 §5)    │
   │              │                                                       │
   │              ▼                                                       │
   │   [3] SOLVER          multiple-choice knapsack over {BF16,FP8,NVFP4} │
   │       (Module 11)     hardware guard rails         (Module 03)       │
   │              │        ε sweep → B_token(ε) curve                     │
   │              ▼                                                       │
   │   [4] BUILDER         emit the quantized checkpoint + manifest       │
   │       (Module 05)     AWQ / GPTQ per the error-mode diagnosis        │
   │              │                                                       │
   │              ▼                                                       │
   │   [5] VERIFIER        locked clocks, interleaved, paired  (Mod 12)   │
   │       (Modules 8,12)  tok/s · τ · α · KL · p99 · agreement           │
   │                       measured vs PREDICTED ── bug check             │
   └─────────────────────────────────────────────────────────────────────┘
```

The contract between stages is what makes it reusable:

```python
@dataclass
class Ledger:      # [1] → [3]
    groups: dict[str, int]          # name → parameter count
    traffic_class: dict[str, str]   # A_streamed / B_gathered / C_conditional / D_state
    b_token_GB: float
    predicted_tps: float
    measured_tps: float | None
    gap: float | None               # predicted / measured — >1.2 ⇒ profile, do not quantize

@dataclass
class Sensitivity: # [2] → [3]
    d_kl:   dict[tuple[str, str], float]   # (group, format) → ΔKL vs BF16
    d_tau:  dict[tuple[str, str], float]   # (group, format) → Δ acceptance length
    additivity_ratio: float                # ~1.0 ⇒ solver assumption holds

@dataclass
class Allocation:  # [3] → [4]
    fmt: dict[str, str]
    predicted_b_token_GB: float
    predicted_kl: float
    predicted_tps: float            # [5] MUST check against this
```

---

**3. Build order**

**Phase 1 — Ledger and the gap check *(start here, always)***

Implement [Module 04's](Lecture-04.md) analyzer. Compute `B_token`, predicted ceiling, and the gap.

```text
   gap = predicted / measured

   ≈ 1.0   →  you are at the bandwidth wall; proceed to Phase 2
   > 1.2   →  STOP. Profile kernels first (Module 03 §5).
              For the case study, gap = 1.27 — Stage 1 of the ladder.
```

**Deliverable:** ledger table, gap, and a written verdict naming your next action.

**Phase 2 — Probe**

Run [Module 06's](Lecture-06.md) activation profiler (diagnosis → method selection) and [Module 07's](Lecture-07.md) leave-one-out sweep (per-group ΔKL and Δτ). Check additivity on your five highest-value pairs.

**Deliverable:** the sensitivity table, with Q/K measured at both short and maximum context (the [Module 07 §2](Lecture-07.md) prediction).

**Phase 3 — Solve**

Run the [Module 11](Lecture-11.md) solver with your measured values. Enumerate all `3^n` to confirm greedy optimality. Sweep `ε` and plot `B_token(ε)`; mark the knee.

**Deliverable:** allocation, solver trace in GB/nat, `B_token(ε)` curve.

**Phase 4 — Build**

Emit the checkpoint with the manifest from [Module 05 §5](Lecture-05.md). **Verify the built artifact's actual `B_token` matches the solver's prediction** — a toolkit that silently ignores a per-group format request is common and will invalidate everything downstream.

**Deliverable:** checkpoint, manifest, predicted-vs-actual `B_token`.

**Phase 5 — Verify**

Run [Module 12's](Lecture-12.md) protocol. Locked clocks, interleaved, paired, ≥8,000 draft tokens per configuration, full reporting standard.

**Deliverable:** the ablation table, and the bug check (`measured ≫ predicted` ⇒ bug).

---

</details>

## 4. 必需的消融网格

不是搜索——而是一个**网格，其设计使每一行隔离课程中的一个论断**：

| # | 配置 | 隔离项 | 预测来源 |
|---|---|---|---|
| 0 | 出货基线 | 参考 | — |
| 1 | + `lm_head` → FP8 | 访存流量 ≠ 体积 | [Mod 04](Lecture-04.md) |
| 2 | + `lm_head` → NVFP4 | 格式阶梯 | [Mod 02](Lecture-02.md) |
| 3 | Q、K → FP8（由 NVFP4） | 接受率恢复 | [Mod 07](Lecture-07.md), [10](Lecture-10.md) |
| 4 | embeddings → FP8 | **必须约为 0 tok/s**（对照） | [Mod 01](Lecture-01.md) |
| 5 | vision tower 移出 | **必须约为 0 tok/s，−0.86 GiB**（对照） | [Mod 01](Lecture-01.md) |
| 6 | 求解器分配 | 核心论点 | [Mod 11](Lecture-11.md) |
| 7 | 262 K 上下文下的第 6 行 | 长上下文反转 | [Mod 09](Lecture-09.md) |
| 8 | 固定量化下的 `K` 扫描 | `K*` 重新调优 | [Mod 10](Lecture-10.md) |

**第 4 行和第 5 行是网格中最重要的行。**它们是阴性对照：课程*预测*它们产生零吞吐变化。如果不是这样，你的测量装置就是坏的，其他每一行都可疑。没有对照的网格只是演示。

每一行报告：`B_token`、tok/s、预测 tok/s、τ、α、平均 KL、p99 KL、top-1 一致率，以及来自 [Module 10](Lecture-10.md) 的**接受税**。

---

## 5. 成功标准

**最低（课程奏效）：**

- [ ] 台账复现常驻字节数误差在 1 % 以内，并预测非投机 tok/s 误差在 10 % 以内。
- [ ] 阴性对照（第 4、5 行）无吞吐变化——证明装置有效。
- [ ] 求解器分配在**严格更低的 KL 下达到 ≤ 出货的 `B_token`**（[Module 11](Lecture-11.md) 的结果，在你的硬件上复现）。
- [ ] 每一项论断都符合 [Module 12](Lecture-12.md) 的报告标准。

**目标（产物达到作品集水准）：**

- [ ] 在接受率 **≥ 2.886**、平均 KL **≤ 0.05** 时达到 **≥ 224 tok/s**（Stage 2）。
- [ ] 标出拐点的 `B_token(ε)` 曲线，以及有充分论证的 `ε`。
- [ ] 262 K 下的长上下文行，附 [Module 09](Lecture-09.md) 的可行性分析。
- [ ] 至少一个**有记录的阴性结果**，并解释其机理。

**拓展（真正的贡献）：**

- [ ] **≥ 273 tok/s**（Stage 3）——需要修好 drafter 路径。
- [ ] 确认并修复 drafter 效率的发现：`c` 从 0.171 向 0.054 推进。
- [ ] Q/K 敏感度对上下文的曲线，检验 [Module 07 §2](Lecture-07.md) 的 RoPE 预测。
- [ ] TurboQuant 在**第二个、不同的模型**上无需改代码即可端到端运行。

最后一项正是案例研究与工具的分水岭。

---

## 6. 交付物

```text
   turboquant/
   ├── README.md                 the finding, up front, in three sentences
   ├── ledger.py                 [1]  Module 04
   ├── probe.py                  [2]  Modules 06, 07, 08
   ├── solver.py                 [3]  Module 11
   ├── builder.py                [4]  Module 05
   ├── verify.py                 [5]  Modules 08, 12
   ├── configs/                  one YAML per ablation row (Module 12 §6 standard)
   ├── results/
   │   ├── ledger.md             traffic classes, B_token, gap
   │   ├── sensitivity.md        per-group ΔKL/Δτ, additivity check
   │   ├── ablation.csv          the grid, raw
   │   ├── b_token_vs_eps.png    the ε curve with the knee marked
   │   └── traces/               Nsight reports for the top kernels
   └── REPORT.md                 hypotheses, predictions, measurements, negatives
```

`REPORT.md` 才是会被阅读的产物。按如下结构组织：

```text
   1. The claim, in one sentence, with the number.
   2. The ledger: where the bytes are and where the headroom was.
   3. The allocation and why the solver chose it (the GB/nat trace).
   4. The grid, including the negative controls.
   5. What did NOT work, and the mechanism.
   6. The next hypothesis.
```

---

## 7. 关于阴性结果

案例研究中最有价值的发现是一个阴性结果：

```text
   Quantizing Q/K:  +1.9 % throughput,  −8.8 % acceptance,
                    +0.082 TV distance,  84 % acceptance tax.
                    ⟹ DO NOT SHIP.
```

没人会发表它，而这正是如此多的人以昂贵的方式重新发现它的原因。你的报告应至少包含一个这样平实陈述的结果。

本课程仅靠算术得出的两个余量发现同样如此——**72 % 对 92 % 的带宽缺口**和**贵了 3.2× 的 draft 步骤**。二者都不是量化结果。它们带来的吞吐收益，都超过这个模型上任何可用的量化决策。

> 这才是课程真正的教益：**字节台账比它所取代的直觉是更好的仪器**，而它最常证明自己价值的方式，是告诉你*不要*量化。

---


<details>
<summary>English original</summary>

**4. The required ablation grid**

Not a search — a **grid designed so each row isolates one claim from the course**:

| # | Configuration | Isolates | Predicted from |
|---|---|---|---|
| 0 | shipped baseline | reference | — |
| 1 | + `lm_head` → FP8 | traffic ≠ size | [Mod 04](Lecture-04.md) |
| 2 | + `lm_head` → NVFP4 | format ladder | [Mod 02](Lecture-02.md) |
| 3 | Q, K → FP8 (from NVFP4) | acceptance recovery | [Mod 07](Lecture-07.md), [10](Lecture-10.md) |
| 4 | embeddings → FP8 | **must be ~0 tok/s** (control) | [Mod 01](Lecture-01.md) |
| 5 | vision tower evicted | **must be ~0 tok/s, −0.86 GiB** (control) | [Mod 01](Lecture-01.md) |
| 6 | solver allocation | the thesis | [Mod 11](Lecture-11.md) |
| 7 | row 6 at 262 K context | long-context inversion | [Mod 09](Lecture-09.md) |
| 8 | `K` sweep at fixed quantization | `K*` re-tuning | [Mod 10](Lecture-10.md) |

**Rows 4 and 5 are the most important rows in the grid.** They are negative controls: the course *predicts* they produce zero throughput change. If they do not, your measurement apparatus is broken and every other row is suspect. A grid without controls is a demo.

For each row report: `B_token`, tok/s, predicted tok/s, τ, α, mean KL, p99 KL, top-1 agreement, and the **acceptance tax** from [Module 10](Lecture-10.md).

---

**5. Success criteria**

**Minimum (the course worked):**

- [ ] Ledger reproduces resident bytes to within 1 % and predicts non-speculative tok/s within 10 %.
- [ ] Negative controls (rows 4, 5) show no throughput change — proving the apparatus.
- [ ] Solver allocation achieves **≤ the shipped `B_token` at strictly lower KL** ([Module 11](Lecture-11.md)'s result, reproduced on your hardware).
- [ ] Every claim meets the [Module 12](Lecture-12.md) reporting standard.

**Target (the artifact is portfolio-grade):**

- [ ] **≥ 224 tok/s** (Stage 2) at acceptance **≥ 2.886** and mean KL **≤ 0.05**.
- [ ] The `B_token(ε)` curve with the knee identified and a defended `ε`.
- [ ] Long-context row at 262 K, with the feasibility analysis from [Module 09](Lecture-09.md).
- [ ] At least one **documented negative result** with its mechanism explained.

**Stretch (a real contribution):**

- [ ] **≥ 273 tok/s** (Stage 3) — requires fixing the drafter path.
- [ ] The drafter-efficiency finding confirmed and fixed: `c` from 0.171 toward 0.054.
- [ ] Q/K sensitivity-vs-context curve, testing [Module 07 §2](Lecture-07.md)'s RoPE prediction.
- [ ] TurboQuant runs end-to-end on a **second, different model** with no code changes.

That last one is what separates a case study from a tool.

---

**6. Deliverables**

```text
   turboquant/
   ├── README.md                 the finding, up front, in three sentences
   ├── ledger.py                 [1]  Module 04
   ├── probe.py                  [2]  Modules 06, 07, 08
   ├── solver.py                 [3]  Module 11
   ├── builder.py                [4]  Module 05
   ├── verify.py                 [5]  Modules 08, 12
   ├── configs/                  one YAML per ablation row (Module 12 §6 standard)
   ├── results/
   │   ├── ledger.md             traffic classes, B_token, gap
   │   ├── sensitivity.md        per-group ΔKL/Δτ, additivity check
   │   ├── ablation.csv          the grid, raw
   │   ├── b_token_vs_eps.png    the ε curve with the knee marked
   │   └── traces/               Nsight reports for the top kernels
   └── REPORT.md                 hypotheses, predictions, measurements, negatives
```

`REPORT.md` is the artifact that gets read. Structure it as:

```text
   1. The claim, in one sentence, with the number.
   2. The ledger: where the bytes are and where the headroom was.
   3. The allocation and why the solver chose it (the GB/nat trace).
   4. The grid, including the negative controls.
   5. What did NOT work, and the mechanism.
   6. The next hypothesis.
```

---

**7. On negative results**

The most valuable finding in the case study is a negative one:

```text
   Quantizing Q/K:  +1.9 % throughput,  −8.8 % acceptance,
                    +0.082 TV distance,  84 % acceptance tax.
                    ⟹ DO NOT SHIP.
```

Nobody publishes that, which is exactly why so many people rediscover it the expensive way. Your report should contain at least one such result stated as plainly.

The same goes for the two headroom findings this course produced by arithmetic alone — **the 72 %-vs-92 % bandwidth gap** and **the 3.2×-too-expensive draft step**. Neither is a quantization result. Both are worth more throughput than any quantization decision available on this model.

> That is the real lesson of the course: **the byte ledger is a better instrument than the intuition it replaces**, and it earns its keep most often by telling you *not* to quantize.

---

</details>

## 8. 达成标准

完成本课程的标准是：你能在陌生的硅片上拿到陌生的 checkpoint，并只用一个下午：

1. 产出它的字节账本，并把 batch-1 的 decode（逐 token 生成阶段）上限预测到 10 % 以内。
2. 判断它是否带宽受限，若不受限就拒绝量化。
3. 说出价值最高的三个张量，分别给出流量与敏感度依据。
4. 说出在该硅片上有原生路径的格式，以及其他任何格式的盈亏平衡阈值。
5. 在跑实验**之前**，用 KL 与验收口径说明行为预算。
6. 解出分配方案，实现它，并用预测来验证结果。
7. 识别出加速其实来自 bug 的情形。

如果你能背出量化算法，却说不出一个 token 读了哪些字节，那你只有词汇表。**本课程的要义是分配决策——以及证明它的纪律。**

---

## 时效性

* **恒久不变：**五阶段架构、构建顺序、带对照的消融设计、达成标准。
* **案例研究锚点：**155.75 tok/s @ τ 2.886；在 `K = 3` 与 `c_bandwidth = 0.054` 下推导出的 `1 + K·c = 1.512`、`c = 0.171`；通向约 280 tok/s 的分级台阶。台阶的分级系数是**课程模型的预测，而非实测值**——验证或证伪它们是本课程的收官任务。
* **相关：**[AI Inference Engineer 2026 — Part 4](../01-AI推理工程师2026/04-真实引擎优化/README.md) 在 8× H200 引擎上对约 96 个 PR 施加同样的纪律，是这门收官课最好的配套读物。

---

*课程完结。[← 返回课程索引](README.md) · [阶段 5 — ML Systems Engineering Guide](../Guide.md)*


<details>
<summary>English original</summary>

**8. Exit criteria**

You have completed this course when you can take an unfamiliar checkpoint on unfamiliar silicon and, in a single afternoon:

1. Produce its byte ledger and predict the batch-1 decode ceiling within 10 %.
2. Say whether it is bandwidth-bound, and refuse to quantize if it is not.
3. Name the three highest-value tensors, citing traffic and sensitivity separately.
4. Name the formats with a native path on that silicon, and the break-even threshold for anything else.
5. State the behavior budget in KL and acceptance terms **before** running the experiment.
6. Solve the allocation, build it, and verify the result against the prediction.
7. Detect the case where your speedup came from a bug.

If you can recite quantization algorithms but cannot say which bytes a token reads, you have vocabulary. **The point of this course is the allocation decision — and the discipline to prove it.**

---

**Current as of**

* **Timeless:** the five-stage architecture, the build order, the ablation-with-controls design, the exit criteria.
* **Case-study pins:** 155.75 tok/s @ τ 2.886; derived `1 + K·c = 1.512`, `c = 0.171` at `K = 3` versus `c_bandwidth = 0.054`; the staged ladder to ~280 tok/s. The ladder's stage factors are **predictions from the course's models, not measurements** — verifying or refuting them is the capstone.
* **Related:** [AI Inference Engineer 2026 — Part 4](../01-AI推理工程师2026/04-真实引擎优化/README.md) runs the same discipline over ~96 PRs on an 8× H200 engine, and is the best companion read for this capstone.

---

*Course complete. [← Back to the course index](README.md) · [Phase 5 — ML Systems Engineering Guide](../Guide.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/Hardware-Aware LLM Quantization/Lecture-13.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/Hardware-Aware%20LLM%20Quantization/Lecture-13.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
