---
title: 第 2 讲：动作精度一致性 harness（agent 运行时框架）
description: 第 2 讲：动作精度一致性 harness（agent 运行时框架）
---

# 第 2 讲：动作精度一致性 harness（agent 运行时框架）

## 概述

**没有测量的优化只是民间传说。** 第 1 讲列出了 VLA（视觉-语言-动作模型）优化的八级阶梯；其中每一级都可能**悄悄毁掉一个策略**。本讲介绍的测量框架，用来判定什么可以安全上线。

一个有用的动作精度一致性 harness 会按顺序回答四个问题，并且在前一个问题通过之前拒绝回答下一个：

1. **逐 tick 精度一致性** — 给定相同的观测，候选策略是否在你能辩护的容差内，输出与参考相同的动作？
2. **轨迹精度一致性** — 在已记录观测的开环回放上，候选的动作序列是否始终贴近参考？
3. **闭环仿真精度一致性** — 当候选真正驱动仿真器时，它是否达到与参考相同的任务成功率？
4. **真机精度一致性** — 在 canary 保护下，候选在留出任务集上的真机行为是否一致？

逐 tick 精度一致性**代价低、必要但不充分**。闭环仿真精度一致性才能抓住第 1 讲中量化可能引发的失败。真机精度一致性才能抓住**仿真无法建模**的失败。

学完本讲，你应该能够：

* 实现一个确定性的回放循环，把同一份观测磁带喂给 N 个候选策略，并输出逐步 diff
* 为动作 MSE 设定逐轴容差预算，并解释为什么这些预算来自*机器人*，而不是模型
* 用固定的种子列表在 LIBERO / RoboCasa / Isaac Lab 上跑闭环仿真精度一致性扫描
* 写一个 CI 门禁，让另一位工程师能在新的检查点上运行并得到 pass/fail 结论
* 设计一个 50 episode 的真机 canary，使其失效时*关闭*且不损坏硬件

---

## 1. 心智模型：参考、候选与 harness

```text
                ┌──────────────────────┐
   observation  │   Reference policy   │  ──►  a_ref(t)
   tape  ─────► │   (fp16/bf16, full)  │
                └──────────────────────┘
                                                ┌──────────────┐
                ┌──────────────────────┐        │              │
                │   Candidate A        │  ──►   │              │
                │   (TRT + INT4)       │        │   Harness    │  ──►  parity_report.csv
                └──────────────────────┘        │              │
                                                │  per-step Δ  │
                ┌──────────────────────┐        │  trajectory  │
                │   Candidate B        │  ──►   │  closed-loop │
                │   (INT4 + FP8 KV)    │        │  sim rollout │
                └──────────────────────┘        │              │
                                                └──────────────┘
                                                        │
                                                        ▼
                                                 pass / fail gate
```

三条原则：

* **参考就是算力无限时你真正会发布的那个策略。** 通常是已发布检查点的 fp16/bf16 版本。固定它的确切权重、确切 tokenizer、确切预处理流水线。参考就是契约。
* **确定性不是可选项。** 关闭 cuDNN 的非确定性，固定所有 RNG 种子，固定采样温度（或用 argmax 取代采样），固定 CUDA Graph 捕获，记录每一处随机性来源。如果参考的两次运行结果不一致，说明 harness 坏了。
* **每项指标的预算由*机器人工程师*定义，而不是你。**“0.01 rad 的关节误差”本身无所谓好坏；夹爪的任务容差才是答案。

---

## 2. 逐 tick 精度一致性（问题 1）

最廉价、最有用、也最被过度信任的检查。

### 2.1 harness 记录什么

对冻结观测磁带中的每个 tick `t`：

| 字段 | 类型 | 原因 |
|-------|------|-----|
| `t` | int | tick 索引 |
| `obs_hash` | sha256 | 保证两个策略看到的是同一输入 |
| `a_ref[t]` | float32[A] | 参考动作，长度 = 动作维度 |
| `a_cand[t]` | float32[A] | 候选动作 |
| `Δ[t] = a_cand[t] − a_ref[t]` | float32[A] | 逐轴误差 |
| `logits_kl[t]`（可选） | float32 | 若做了 token 化，动作 token 分布的 KL |
| `latency_ms[t]` | float32 | 候选的墙钟时间 |

harness 必须拒绝任何 `obs_hash` 在两次运行之间出现差异的 tick。如果哈希不一致，那是预处理 bug，而不是模型 bug；在修好之前，逐 tick 精度一致性毫无意义。


<details>
<summary>English original</summary>

**Lecture 2: The Action-Parity Harness**

**Overview**

**Optimization without measurement is folklore.** Lecture 1 listed eight rungs of VLA optimization; every one of them can **silently destroy a policy**. This lecture is the measurement framework that decides what is safe to ship.

A useful action-parity harness answers four questions, in order, and refuses to answer the next one until the previous one passes:

1. **Per-tick parity** — given the same observation, does the candidate policy emit the same action as the reference, within a tolerance you can defend?
2. **Trajectory parity** — over an open-loop replay of logged observations, does the candidate's action sequence stay close to the reference?
3. **Closed-loop sim parity** — when the candidate actually drives the simulator, does it reach the same task success rate as the reference?
4. **On-robot parity** — does the candidate behave the same on the real robot, under canary protection, across a held-out task set?

Per-tick parity is **cheap and necessary but not sufficient**. Closed-loop sim parity is what catches the failures Lecture 1's quantization can cause. On-robot parity is what catches the failures that **sim cannot model**.

By the end of this lecture you should be able to:

* implement a deterministic replay loop that feeds the same observation tape into N candidate policies and emits a per-step diff
* set per-axis tolerance budgets for action MSE, and explain why those budgets came from the *robot*, not the model
* run a closed-loop sim parity sweep on LIBERO / RoboCasa / Isaac Lab with a fixed seed list
* write a CI gate that another engineer can run on a new checkpoint and get a pass/fail
* design a 50-episode on-robot canary that fails *closed* without breaking hardware

---

**1. The mental model: reference, candidate, harness**

```text
                ┌──────────────────────┐
   observation  │   Reference policy   │  ──►  a_ref(t)
   tape  ─────► │   (fp16/bf16, full)  │
                └──────────────────────┘
                                                ┌──────────────┐
                ┌──────────────────────┐        │              │
                │   Candidate A        │  ──►   │              │
                │   (TRT + INT4)       │        │   Harness    │  ──►  parity_report.csv
                └──────────────────────┘        │              │
                                                │  per-step Δ  │
                ┌──────────────────────┐        │  trajectory  │
                │   Candidate B        │  ──►   │  closed-loop │
                │   (INT4 + FP8 KV)    │        │  sim rollout │
                └──────────────────────┘        │              │
                                                └──────────────┘
                                                        │
                                                        ▼
                                                 pass / fail gate
```

Three principles:

* **The reference is the policy you would actually ship if compute were free.** Usually fp16/bf16 of the published checkpoint. Pin its exact weights, exact tokenizer, exact preprocessing pipeline. The reference is the contract.
* **Determinism is non-optional.** Disable cuDNN nondeterminism, fix all RNG seeds, fix sampling temperature (or replace sampling with argmax), pin CUDA Graph capture, log every randomness source. If two runs of the reference disagree, the harness is broken.
* **Every metric has a budget *the roboticist* defines, not you.** "0.01 rad of joint error" is not inherently good or bad; the gripper's task tolerance is the answer.

---

**2. Per-tick parity (Question 1)**

The cheapest, most useful, most over-trusted check.

**2.1 What the harness records**

For each tick `t` in a frozen observation tape:

| Field | Type | Why |
|-------|------|-----|
| `t` | int | tick index |
| `obs_hash` | sha256 | guarantees both policies saw the same input |
| `a_ref[t]` | float32[A] | reference action, length = action dim |
| `a_cand[t]` | float32[A] | candidate action |
| `Δ[t] = a_cand[t] − a_ref[t]` | float32[A] | per-axis error |
| `logits_kl[t]` (optional) | float32 | KL of action-token distributions if tokenized |
| `latency_ms[t]` | float32 | candidate wall-clock |

The harness must reject any tick where `obs_hash` differs between runs. If hashes disagree, you have a preprocessing bug, not a model bug, and per-tick parity is meaningless until you fix it.

</details>

### 2.2 指标

* **动作 MSE，按轴：** 每个轴 `k` 的 `mean(Δ[:, k]^2)`。按轴的细分很重要——一个平移做得很好但夹爪闭合做得很差的模型，与一个均匀漂移的模型，是不同的部署问题。
* **动作最大误差，按轴：** `max(|Δ[:, k]|)`。对于安全论证，P99 比均值更有用。
* **Logit KL**（仅当动作头被 token 化时）：`mean(KL(p_ref || p_cand))`。对主干量化损伤最敏感的早期预警。
* **延迟 P50 / P95 / P99：** 部署叙事所必需；也用于淘汰那些通过精度一致性但超出控制预算的候选。

### 2.3 容差预算

这是 harness（agent 运行时框架）用作通过/失败门限的表。下表中的数字是*一个 7 自由度机械臂做桌面操作的示例*——你的机器人的数字会不同。

| 轴 | 单位 | MSE 预算 | P99 最大预算 | 数字来源 |
|------|------|------------|-----------------|----------------------|
| x, y, z（末端执行器） | m | 1e-5（≈3 mm RMS） | 8e-3（8 mm） | 夹爪手指半宽减去物体余量 |
| roll, pitch, yaw | rad | 1e-4（≈0.6° RMS） | 5e-2（≈3°） | 典型抓取的姿态容差 |
| 夹爪 | 归一化 [0, 1] | 1e-4 | 5e-2 | 超过该阈值“张开”变为“闭合” |
| 动作 token logit KL | nats | 0.01 均值，0.05 P99 | — | 经验值——高于此值与 rollout 漂移相关 |

如何推导*你的*预算：

1. 用不同的 RNG 种子和相同的观测磁带运行参考策略两次。参考自身的非确定性就是你的*下限*。
2. 将该下限乘以 3-5×，得到“候选与参考在统计上不可区分”的预算。
3. 独立地，向机器人集成方询问任务能容忍的最大关节 / 位姿误差（*物理*预算）。
4. 取统计预算与物理预算的 **min**。那就是你的通过阈值。

如果候选通过了第 4 步的预算，它就通过了问题 1。它还没有赢得驱动一台机器人的资格。

---

## 3. 轨迹一致性（问题 2）

逐 tick 的一致性具有误导性，因为 **tick 误差会累积**。同样的每 tick 5 mm 误差，可能积分成 5 cm，也可能积分成 5 mm，取决于策略是否**自我纠正**。

### 3.1 开环回放

逐 tick 地向候选喂入与参考相同的观测磁带，但通过对其动作积分来累积一个合成状态估计：

```text
state_cand(t+1) = forward_kinematics( state_cand(t) + a_cand(t) * dt )
state_ref(t+1)  = forward_kinematics( state_ref(t)  + a_ref(t)  * dt )

drift(t) = || state_cand(t) − state_ref(t) ||
```

按 episode 绘制 `drift(t)`。曲线的形状比其峰值更重要：

* **有界漂移**：候选在统计上等价。可以发布。
* **线性漂移**：候选中存在按轴的偏置。重新排查按轴 MSE；该偏置此前隐藏在一个很小的均值之下。
* **后期尖峰漂移**：候选在巡航阶段表现良好，但在精确阶段（最终接近、抓取闭合）失败。这是最常见的量化失效模式。修复方法通常是让动作头保持更高精度。

### 3.2 容差

用与 §2.3 相同的方式做预算：取参考对参考的漂移包络并乘以 3-5×；与物理任务包络取交集（例如“末端执行器必须处于参考策略在同一任务阶段本会放置的位置的 1 cm 以内”）。

### 3.3 为什么开环不够

开环回放假设候选的动作**不会改变它下一 tick 会看到什么**。在真实 rollout 中，每个动作都会改变观测，而每 tick 的小误差会被**闭环动力学放大**。那就是问题 3。

---

## 4. 闭环仿真一致性（问题 3）

真正能预测你优化后的策略是否会拿起杯子的指标。

### 4.1 仿真选择

harness（agent 运行时框架）应至少支持以下之一：

* **LIBERO**（Spatial / Object / Goal / Long）——快速、成功标准定义明确，是 OpenVLA 类策略事实上的评测。
* **RoboCasa**——厨房规模的任务，对 π₀ 类策略有用。
* **Isaac Lab** 搭配 OpenX 操作套件——较慢，但与真机动力学更匹配，支持域随机化。

无论你选哪个，**冻结仿真版本、资产集与种子列表**。harness 将这些作为 manifest 文件提交到仓库。针对未指定仿真构建生成的“一致性报告”毫无价值。


<details>
<summary>English original</summary>

**2.2 Metrics**

* **Action MSE, per-axis:** `mean(Δ[:, k]^2)` for each axis `k`. The per-axis breakdown matters — a model that is great on translation but bad on gripper-close is a different deployment problem from one that drifts uniformly.
* **Action max-error, per-axis:** `max(|Δ[:, k]|)`. P99 is more useful than mean for safety arguments.
* **Logit KL** (only if the action head is tokenized): `mean(KL(p_ref || p_cand))`. The most sensitive early-warning of backbone quantization damage.
* **Latency P50 / P95 / P99:** required for the deployment story; also used to reject candidates that pass parity but blow the control budget.

**2.3 Tolerance budgets**

This is the table the harness uses as a pass/fail gate. The numbers below are *examples for a 7-DoF arm doing tabletop manipulation* — your robot's numbers will differ.

| Axis | Unit | MSE budget | P99 max budget | Source of the number |
|------|------|------------|-----------------|----------------------|
| x, y, z (end-effector) | m | 1e-5 (≈3 mm RMS) | 8e-3 (8 mm) | gripper finger half-width minus object margin |
| roll, pitch, yaw | rad | 1e-4 (≈0.6° RMS) | 5e-2 (≈3°) | orientation tolerance of typical grasp |
| gripper | normalized [0, 1] | 1e-4 | 5e-2 | threshold beyond which "open" becomes "close" |
| action-token logit KL | nats | 0.01 mean, 0.05 P99 | — | empirical — values above this correlate with rollout drift |

How to derive *your* budgets:

1. Run the reference policy twice with different RNG seeds and the same observation tape. The non-determinism of the reference itself is your *floor*.
2. Multiply that floor by 3-5× to get a "candidate is statistically indistinguishable from reference" budget.
3. Independently, ask the robot integrator the maximum joint / pose error that the task can tolerate (the *physical* budget).
4. Take the **min** of the statistical budget and the physical budget. That is your pass threshold.

If your candidate passes step 4 budgets, it has passed Question 1. It has not yet earned the right to drive a robot.

---

**3. Trajectory parity (Question 2)**

Per-tick parity is misleading because **tick errors compound**. The same 5 mm per-tick error can integrate to 5 cm or to 5 mm depending on whether the policy **self-corrects**.

**3.1 Open-loop replay**

Feed the candidate the same observation tape as the reference, tick by tick, but accumulate a synthetic state estimate by integrating its actions:

```text
state_cand(t+1) = forward_kinematics( state_cand(t) + a_cand(t) * dt )
state_ref(t+1)  = forward_kinematics( state_ref(t)  + a_ref(t)  * dt )

drift(t) = || state_cand(t) − state_ref(t) ||
```

Plot `drift(t)` per episode. The shape of the curve matters more than its peak:

* **Bounded drift**: the candidate is statistically equivalent. Ship it.
* **Linear drift**: there is a per-axis bias in the candidate. Re-investigate per-axis MSE; the bias was hiding under a small mean.
* **Late-spike drift**: the candidate is fine on the cruise phase and fails on the precise phase (final approach, grasp closure). This is the most common quantization failure mode. The fix is usually keeping the action head at higher precision.

**3.2 Tolerance**

Budget the same way as in §2.3: take the reference-vs-reference drift envelope and multiply by 3-5×; intersect with the physical task envelope (e.g. "the end-effector must be within 1 cm of where the reference policy would have placed it at the same task phase").

**3.3 Why open-loop is not enough**

Open-loop replay assumes the candidate's actions **do not change what it would see next tick**. In a real rollout, every action changes the observation, and small per-tick errors get **amplified by closed-loop dynamics**. That is Question 3.

---

**4. Closed-loop sim parity (Question 3)**

The metric that actually predicts whether your optimized policy will pick up the cup.

**4.1 Sim choice**

The harness should support at least one of:

* **LIBERO** (Spatial / Object / Goal / Long) — fast, well-defined success criteria, the de facto eval for OpenVLA-class policies.
* **RoboCasa** — kitchen-scale tasks, useful for π₀-class policies.
* **Isaac Lab** with the OpenX manipulation suite — slower but matches real-robot dynamics better, supports domain randomization.

Whichever you pick, **freeze the sim version, asset set, and seed list**. The harness commits these to the repo as a manifest file. A "parity report" generated against an unspecified sim build is worthless.

</details>

### 4.2 协议

对每个候选（包括参考实现，重跑一遍作为合理性检查）：

1. 为每个任务固定一份 N 个 episode 的 seed 列表。N = 50 是可用的下限；N = 200 才是能在技术报告里发表的量级。
2. 在 sim 中 rollout 策略，记录成功/失败、time-to-success，以及完整的 action+observation trace。
3. 计算：
   * **成功率精度一致性**：每个任务的 `success_rate(cand) − success_rate(ref)`。接受标准：候选与参考的差距必须在 `Δ_SR` 之内（通常为 Δ_SR ≤ 2 个百分点绝对值，并报告每个任务的置信区间）。
   * **time-to-success 精度一致性**：episode 长度的中位数之差。能抓出「成功但很慢」的策略，这通常意味着量化把策略变胆小了。
   * **失效模式细分**：对每次失败分类（碰撞、物体掉落、抓取失败、超时）。成功率与参考相同但失效模式不同的候选很可疑，不应通过。

### 4.3 chunk 缓存与投机 decode（逐 token 生成阶段）模式

如果 Lecture 1 的第 5 或第 6 级台阶在起作用，harness（agent 运行时框架）必须开启 chunk 缓存，*单独*跑一轮 sim sweep。逐 tick 的精度一致性会轻松通过（缓存的动作是 bit-exact 的）；闭环成功率才是陈旧度暴露出来的地方。交付物是一张表，按任务给出 `K`（cache stride）与成功率精度一致性的对应关系。留在精度一致性预算之内最大的 `K` 就是部署设置。

### 4.4 统计置信度

在每个任务 N = 50 个 episode 且结果为二项分布的情况下，0.80 成功率的 95% CI 约为 ±11 pp。这意味着在 N = 50 时无法声称 2 pp 的精度一致性预算 —— 要在 2 pp 的紧区间内下结论，每个任务需要 N ≥ ~400，否则就得接受更宽的区间并如实报告。

harness 的通过/失败门控不应在置信度上撒谎。如果预算比 CI 更紧，门控应报告「N = 50 下不确定，请用 N = 200 重跑」，而不是「通过」。

---

## 5. 机器人上的 canary（问题 4）

最便宜的 sim **不是真机**。harness 必须定义一套 **fail closed** 的 canary 协议。

### 5.1 canary 集

从部署范围内挑选 5-10 个任务，要求：

* 覆盖你关心的失效模式（抓取、放置、移动、精细对齐）
* 至少包含一个*已知*接近策略能力边界的任务 —— 简单的任务什么都能通过，什么也说明不了
* 在策略失败时可恢复（不撞机、不掉落易碎物品、工作空间内没有人员）

### 5.2 协议

对每个候选：

1. 在 canary 集上跑**参考**策略，每个任务 M = 10-20 次试验，使用与候选相同的 safety wrapper。记录成功/失败以及任何安全停机事件。
2. 用相同的设置、相同的操作员、相同的场景复位协议跑**候选**策略，尽可能安排在同一天。
3. 按任务用二项检验或 Fisher 精确检验比较成功率。通过标准是：在任一单项任务上，候选在 α = 0.05 下统计上不差于参考，*并且*总体成功率与参考之差在 Δ_SR 之内。

### 5.3 safety wrapper

harness 假定机器人一侧具备：

* 力/力矩看门狗，在意外接触时触发受控停机
* 工作空间边界监视器，阻止策略把末端执行器指令到安全包络之外
* 动作变化率限制器，拒绝不连续指令
* 在整个 canary 过程中触手可及、人员可操作的急停按钮

以上任一项缺失，harness 都应拒绝为真机候选评分。这是代码级检查，不是检查清单条目。

### 5.4 “fail closed”的含义

harness 里的 bug、缺失的 seed、未定义的预算、sim 版本不匹配 —— 其中任何一项都应让门控判候选*失败*，绝不能悄悄放过。**未知情况的默认动作是“不要发布”。**

---


<details>
<summary>English original</summary>

**4.2 Protocol**

For each candidate (including the reference, re-run as a sanity check):

1. Fix a seed list of N episodes per task. N = 50 is a workable minimum; N = 200 is what you publish in a tech report.
2. Roll out the policy in sim, recording success/failure, time-to-success, and the full action+observation trace.
3. Compute:
   * **success-rate parity**: `success_rate(cand) − success_rate(ref)` per task. Acceptance: candidate must be within `Δ_SR` of reference (typically Δ_SR ≤ 2 percentage points absolute, with the per-task confidence interval reported).
   * **time-to-success parity**: median episode length difference. Catches policies that "succeed but slowly," which usually means a quantization that made the policy timid.
   * **failure-mode breakdown**: classify each failure (collision, dropped object, missed grasp, timeout). A candidate that has the same success rate as reference but different failure modes is suspicious and should not pass.

**4.3 Chunk-cache and speculative-decode modes**

If Lecture 1's rungs 5 or 6 are in play, the harness must run a *separate* sim sweep with chunk caching enabled. Per-tick parity will trivially pass (the cached actions are bit-exact); closed-loop success rate is where the staleness shows up. The deliverable is a table of `K` (cache stride) vs success-rate parity, per task. The largest `K` that stays inside the parity budget is the deployment setting.

**4.4 Statistical confidence**

With N = 50 episodes per task and a binomial outcome, the 95% CI on a 0.80 success rate is roughly ±11 pp. That means a 2 pp parity budget cannot be claimed at N = 50 — you need N ≥ ~400 per task for a tight 2 pp band, or you accept a wider band and report it.

The harness's pass/fail gate should not lie about confidence. If the budget is tighter than the CI, the gate reports "inconclusive at N = 50, re-run at N = 200" rather than "pass."

---

**5. On-robot canary (Question 4)**

The cheapest sim is **not the real robot**. The harness must define a canary protocol that **fails closed**.

**5.1 Canary set**

Pick 5-10 tasks from the deployment scope that:

* span the failure modes you care about (grasping, placing, transit, fine alignment)
* include at least one task that is *known* to be near the policy's capability edge — easy tasks pass everything and tell you nothing
* are recoverable if the policy fails (no crash, no dropped fragile object, no person in the workspace)

**5.2 Protocol**

For each candidate:

1. Run the **reference** policy on the canary set, M = 10-20 trials per task, behind the same safety wrapper you will use for the candidate. Record success/failure and any safety-stop events.
2. Run the **candidate** with the same setup, same operator, same scene reset protocol, on the same day if possible.
3. Compare success rates with a binomial test or Fisher's exact test per task. The pass criterion is: candidate is not statistically worse than reference at α = 0.05 on any individual task, *and* the aggregate success rate is within Δ_SR of reference.

**5.3 The safety wrapper**

The harness assumes the robot side has:

* a force / torque watchdog that triggers a controlled stop on unexpected contact
* a workspace-boundary monitor that prevents the policy from commanding the end-effector outside the safe envelope
* an action-rate-of-change limiter that rejects discontinuous commands
* a human-accessible e-stop within reach for the whole canary

If any of these are missing, the harness should refuse to grade an on-robot candidate. This is a code-level check, not a checklist item.

**5.4 What "fails closed" means**

A bug in the harness, a missing seed, an undefined budget, a sim version mismatch — any of these should cause the gate to *fail* the candidate, never to silently pass. The **default for unknown is "do not ship."**

---

</details>

## 6. 实现骨架

harness 是一个小型库加一个 CLI。形态用伪代码表示如下：

```text
harness/
├── manifest.yaml              # sim version, seed list, task suite, tolerance budgets
├── tape/                      # frozen observation tapes for per-tick + open-loop runs
│   ├── tape_001.npz
│   └── ...
├── adapters/
│   ├── openvla_ref.py         # loads reference checkpoint, exposes step(obs) -> action
│   ├── openvla_int4.py        # candidate
│   └── ...
├── metrics/
│   ├── per_tick.py            # MSE, max, logit-KL
│   ├── trajectory.py          # open-loop FK drift
│   ├── closed_loop.py         # sim rollout + success rate
│   └── canary.py              # on-robot grading with stats tests
├── gate.py                    # reads manifest, runs metrics, emits pass/fail
└── report.py                  # markdown + CSV + plots
```

CLI 接口面：

```text
parity tape --episodes 50 --out tape/                       # record reference observation tape
parity per-tick   --ref openvla_ref --cand openvla_int4     # answers Q1
parity trajectory --ref openvla_ref --cand openvla_int4     # answers Q2
parity sim        --suite libero-spatial --episodes 200     # answers Q3
parity canary     --robot panda-1 --tasks canary_set.yaml   # answers Q4
parity gate       --candidate openvla_int4 --manifest manifest.yaml  # CI entry point
parity report     --out reports/openvla_int4.md
```

有两个设计选择值得辩护：

* **适配器只暴露 `step(obs) -> action`，别无其他。** 没有 stream，也没有对 harness 可见的内部状态。正是这一点让你能用同一套代码评测 TRT 编译的策略与 vLLM 服务的策略。
* **gate 是 CI 唯一运行的东西。** 单项指标用于调试；gate 才是契约。gate 通过，该候选版本按 manifest 的定义即可交付。manifest 过松，那是 manifest 评审的问题，不是指标 bug。

---

## 7. CI 门禁

gate 脚本是让 harness 落地的产物。一份有用的契约：

```text
$ parity gate --candidate openvla_int4 --manifest manifest.yaml
[ok]   per_tick:    action_mse_within_budget=true   logit_kl=0.008
[ok]   trajectory:  drift_within_envelope=true
[ok]   sim:         success_rate_parity Δ=-0.7pp (CI ±2.1pp at N=200)
[skip] canary:      no robot configured (manifest.canary.required=false)
GATE: PASS
```

```text
$ parity gate --candidate openvla_int4_kv_fp8 --manifest manifest.yaml
[ok]   per_tick:    action_mse_within_budget=true   logit_kl=0.012
[FAIL] trajectory:  late-phase drift exceeds envelope on tasks: pick_butter, close_drawer
[skip] sim:         skipped because trajectory failed
GATE: FAIL — see reports/openvla_int4_kv_fp8.md
```

CI 钩子：

* 每个推送到模型注册表的检查点都会跑 gate
* `production` tag 必须具备 gate；没有通过 gate 的检查点不能晋级
* gate 针对*当前* manifest 运行；manifest 一变，所有生产候选都要重新过 gate

---

## 8. harness 旨在捕捉的失效模式

这些正是 Lecture 1 的优化在真实场景中实际造成的失效，以及 harness 如何捕捉它们：

| 失效 | 症状 | 捕捉方式 |
|---------|---------|-----------|
| Backbone INT4 过度抑制稀有动作 token | per-tick MSE 看起来正常，但需要边缘情形抓取的任务成功率下降 | sim parity（Question 3），而非 per-tick（Question 1） |
| Vision tower FP8 校准在错误分布上训练 | 在类校准场景下 per-tick 正常，光照变化时失败 | 在域随机化种子列表上做 sim parity |
| Chunk-cache K 过于激进 | per-tick 比特级一致，但在快速子轨迹（最终抓取闭合）上失败 | 启用 chunk-cache 模式做 sim parity |
| Action-head INT8 塌缩了某个连续轴 | per-axis MSE 均值正常，但某一轴的 P99 最大值爆掉 | per-tick 中的 per-axis P99 预算（Question 1） |
| 投机 decode（逐 token 生成阶段）拒绝了错误的 token | per-tick MSE 正常，延迟*反而*高于预期 | gate 中比较延迟 P95 与 logit-KL 差异 |
| sim parity 通过但真机失败 | 在 LIBERO 上成功，在 Panda 上掉点 | 真机金丝雀（Question 4）——sim 是必要条件而非充分条件 |
| 参考实现在你脚下漂移 | 同一份代码上候选的精度一致性漂移 | manifest 固定参考实现；若参考 hash 变化而 manifest 未升版，harness 拒绝评测 |

如果你的 harness 没能捕捉全部这些失效，请在 `manifest.yaml` 中列出遗漏了哪些，好让下一位工程师知道哪些*没有*被验证。

---


<details>
<summary>English original</summary>

**6. Implementation skeleton**

The harness is a small library plus a CLI. The shape, in pseudocode:

```text
harness/
├── manifest.yaml              # sim version, seed list, task suite, tolerance budgets
├── tape/                      # frozen observation tapes for per-tick + open-loop runs
│   ├── tape_001.npz
│   └── ...
├── adapters/
│   ├── openvla_ref.py         # loads reference checkpoint, exposes step(obs) -> action
│   ├── openvla_int4.py        # candidate
│   └── ...
├── metrics/
│   ├── per_tick.py            # MSE, max, logit-KL
│   ├── trajectory.py          # open-loop FK drift
│   ├── closed_loop.py         # sim rollout + success rate
│   └── canary.py              # on-robot grading with stats tests
├── gate.py                    # reads manifest, runs metrics, emits pass/fail
└── report.py                  # markdown + CSV + plots
```

CLI surface:

```text
parity tape --episodes 50 --out tape/                       # record reference observation tape
parity per-tick   --ref openvla_ref --cand openvla_int4     # answers Q1
parity trajectory --ref openvla_ref --cand openvla_int4     # answers Q2
parity sim        --suite libero-spatial --episodes 200     # answers Q3
parity canary     --robot panda-1 --tasks canary_set.yaml   # answers Q4
parity gate       --candidate openvla_int4 --manifest manifest.yaml  # CI entry point
parity report     --out reports/openvla_int4.md
```

Two design choices worth defending:

* **Adapters expose `step(obs) -> action`, nothing else.** No streaming, no internal state visible to the harness. This is what lets you grade a TRT-compiled policy and a vLLM-served policy with the same code.
* **The gate is the only thing CI runs.** Individual metrics are for debugging; the gate is the contract. If the gate passes, the candidate is shippable per the manifest's definitions. If the manifest is too lax, that is a manifest review, not a metric bug.

---

**7. CI gating**

The gate script is the artifact that makes the harness real. A useful contract:

```text
$ parity gate --candidate openvla_int4 --manifest manifest.yaml
[ok]   per_tick:    action_mse_within_budget=true   logit_kl=0.008
[ok]   trajectory:  drift_within_envelope=true
[ok]   sim:         success_rate_parity Δ=-0.7pp (CI ±2.1pp at N=200)
[skip] canary:      no robot configured (manifest.canary.required=false)
GATE: PASS
```

```text
$ parity gate --candidate openvla_int4_kv_fp8 --manifest manifest.yaml
[ok]   per_tick:    action_mse_within_budget=true   logit_kl=0.012
[FAIL] trajectory:  late-phase drift exceeds envelope on tasks: pick_butter, close_drawer
[skip] sim:         skipped because trajectory failed
GATE: FAIL — see reports/openvla_int4_kv_fp8.md
```

CI hooks:

* gate runs on every checkpoint pushed to the model registry
* gate is required for the `production` tag; a checkpoint without a passing gate cannot promote
* gate runs against the *current* manifest; if the manifest changes, all production candidates re-gate

---

**8. Failure modes the harness exists to catch**

These are the failures Lecture 1's optimizations actually produce in the wild, and how the harness catches them:

| Failure | Symptom | Caught by |
|---------|---------|-----------|
| Backbone INT4 over-suppresses rare action tokens | per-tick MSE looks fine but task success drops on tasks needing edge-case grips | sim parity (Question 3), not per-tick (Question 1) |
| Vision tower FP8 calibration trained on wrong distribution | per-tick fine on calibration-like scenes, fails on lighting changes | sim parity on a domain-randomized seed list |
| Chunk-cache K too aggressive | per-tick bit-exact, fails on fast sub-trajectories (final grasp closure) | sim parity with chunk-cache mode enabled |
| Action-head INT8 collapsed a continuous axis | per-axis MSE fine in mean, P99 max blows up on one axis | per-axis P99 budget in per-tick (Question 1) |
| Speculative decode rejects the wrong tokens | per-tick MSE fine, latency *worse* than expected | latency P95 vs. logit-KL diff in the gate |
| Sim parity passes but real-robot fails | success on LIBERO, drops on Panda | on-robot canary (Question 4) — sim is necessary not sufficient |
| Reference shifted under you | candidate parity drifts on the same code | manifest pins the reference; harness refuses to grade if the reference hash changed without a manifest bump |

If your harness does not catch all of these, list which ones it misses in `manifest.yaml` so the next engineer knows what is *not* validated.

---

</details>

## 9. Lab — 在 Lecture 1 的候选上把 harness 搭起来

直接接续 Lecture 1 的 lab。假定你已有一个 reference，以及至少一个候选检查点。

1. 为 reference 和一个候选实现 `adapters/`。二者各自暴露 `step(obs) -> action`。
2. 在 LIBERO-Spatial 上录制 reference 的 50 个 episode 观测 tape。提交该 tape（或指向稳定 URL 的 manifest）。
3. 用 §2.2 中的指标实现 `per_tick.py`。在候选上运行。提交 `reports/per_tick.csv`。
4. 用简单前向运动学 + 漂移累积实现 `trajectory.py`。逐 episode 绘制 `drift(t)`。提交 `reports/trajectory.png`。
5. 用一个固定的 200-seed 列表，针对 LIBERO-Spatial 实现 `closed_loop.py`。分别运行 reference 与候选。计算带 CI 的成功率精度一致性。提交 `reports/sim_parity.md`。
6. 编写 `manifest.yaml`，写入你的容差预算和 seed 列表。
7. 编写 `gate.py` 把所有环节串起来，pass 时以 0 退出，fail 时以非零退出。
8. 运行 gate。迭代候选直至其通过，或记录其无法通过的原因。

本 lab 的通过标准：另一位工程师能 clone 仓库、运行 `parity gate --candidate openvla_int4 --manifest manifest.yaml`，并在同一硬件等级上复现你的 pass/fail 结果。harness 的价值在于可复现性，而非指标本身。

---

## 自检

1. 你的候选在每个轴上 per-tick MSE 都在预算内，但闭环 sim 成功率比 reference 低 12 pp。说出两种可能导致该现象的物理机制，以及你会对 harness（而非候选）做何改动，以便下次更早发现它们。
2. 你的 gate 对 sim 精度一致性报告的是 `inconclusive at N=50`。团队想发货。正确答案是什么，真要把 gate 提到 `pass` 需要付出什么代价？
3. 你有每个任务 10 次试验的真机 canary 结果，且候选与 reference“看起来相似”。为什么“看起来相似”不允许写进 gate 的词汇表，在 N = 10 时你能做出的最小可辩护的统计陈述是什么？
4. 为什么 harness 要在 `manifest.yaml` 中钉住 sim 版本？不这么做会出什么问题？
5. 有同事提议“既然 sim 通过了，就跳过真机 canary”。用一个 canary 能捕获而 sim 不能捕获的具体失效模式，用两句话反驳。

---

## 参考文献

* OpenVLA evaluation protocol — [code](https://github.com/openvla/openvla/tree/main/experiments)
* LIBERO benchmark — [paper](https://arxiv.org/abs/2306.03310), [code](https://github.com/Lifelong-Robot-Learning/LIBERO)
* RoboCasa — [project](https://robocasa.ai/), [paper](https://arxiv.org/abs/2406.02523)
* Isaac Lab manipulation suite — [docs](https://isaac-sim.github.io/IsaacLab/main/source/overview/environments.html)
* "Evaluating Real-World Robot Manipulation Policies in Simulation" — [paper](https://arxiv.org/abs/2405.05941) — 支持问题 4 的 sim/real 相关性论证
* CalVin benchmark — [project](http://calvin.cs.uni-freiburg.de/) — 长时程任务的替代方案
* On binomial confidence intervals for small-N robot evaluation — Wilson score interval，见 [Brown, Cai & DasGupta (2001)](https://projecteuclid.org/euclid.ss/1009213286)

---

## 本专题课程的后续

* 上一节：[Lecture 1 — VLA Optimization for Real-Time Control](Lecture-01.md)
* 返回：[VLA Optimization and Action-Parity Harness — Overview](README.md)
* 上级：[Phase 5 — Robotics](../Guide.md)


<details>
<summary>English original</summary>

**9. Lab — Stand the harness up on Lecture 1's candidates**

Continues directly from Lecture 1's lab. Assumes you have a reference + at least one candidate checkpoint.

1. Implement `adapters/` for the reference and one candidate. Each exposes `step(obs) -> action`.
2. Record an observation tape of 50 episodes from the reference on LIBERO-Spatial. Commit the tape (or a manifest pointing to a stable URL).
3. Implement `per_tick.py` with the metrics in §2.2. Run it on the candidate. Commit `reports/per_tick.csv`.
4. Implement `trajectory.py` with simple forward kinematics + drift accumulation. Plot `drift(t)` per episode. Commit `reports/trajectory.png`.
5. Implement `closed_loop.py` against LIBERO-Spatial with a fixed 200-seed list. Run reference and candidate. Compute success-rate parity with CIs. Commit `reports/sim_parity.md`.
6. Write `manifest.yaml` with your tolerance budgets and the seed list.
7. Write `gate.py` that ties it all together and exits 0 on pass, non-zero on fail.
8. Run the gate. Iterate the candidate until it passes, or document why it cannot.

Pass criterion for the lab: another engineer can clone the repo, run `parity gate --candidate openvla_int4 --manifest manifest.yaml`, and reproduce your pass/fail result on the same hardware class. The harness's value is reproducibility, not the metrics themselves.

---

**Self-check**

1. Your candidate passes per-tick MSE within budget on every axis, but closed-loop sim success rate is 12 pp below the reference. Name two physical mechanisms that could cause this and the change you would make to the harness (not the candidate) to detect them earlier next time.
2. Your gate is reporting `inconclusive at N=50` for sim parity. The team wants to ship. What is the right answer, and what does it cost to actually move the gate to `pass`?
3. You have on-robot canary results for 10 trials per task and the candidate "looks similar" to the reference. Why is "looks similar" not allowed in the gate's vocabulary, and what is the smallest defensible statistical statement you can make at N = 10?
4. Why does the harness pin a sim version in `manifest.yaml`? What goes wrong if it does not?
5. A teammate proposes "let's skip on-robot canary because sim passed." Refute this in two sentences using one specific failure mode the canary catches that sim does not.

---

**References**

* OpenVLA evaluation protocol — [code](https://github.com/openvla/openvla/tree/main/experiments)
* LIBERO benchmark — [paper](https://arxiv.org/abs/2306.03310), [code](https://github.com/Lifelong-Robot-Learning/LIBERO)
* RoboCasa — [project](https://robocasa.ai/), [paper](https://arxiv.org/abs/2406.02523)
* Isaac Lab manipulation suite — [docs](https://isaac-sim.github.io/IsaacLab/main/source/overview/environments.html)
* "Evaluating Real-World Robot Manipulation Policies in Simulation" — [paper](https://arxiv.org/abs/2405.05941) — sim/real correlation arguments that justify Question 4
* CalVin benchmark — [project](http://calvin.cs.uni-freiburg.de/) — alternative for long-horizon tasks
* On binomial confidence intervals for small-N robot evaluation — Wilson score interval, see [Brown, Cai & DasGupta (2001)](https://projecteuclid.org/euclid.ss/1009213286)

---

**Next in this special course**

* Previous: [Lecture 1 — VLA Optimization for Real-Time Control](Lecture-01.md)
* Back: [VLA Optimization and Action-Parity Harness — Overview](README.md)
* Up: [Phase 5 — Robotics](../Guide.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track D - Robotics/VLA Optimization and Action-Parity Harness/Lecture-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20D%20-%20Robotics/VLA%20Optimization%20and%20Action-Parity%20Harness/Lecture-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
