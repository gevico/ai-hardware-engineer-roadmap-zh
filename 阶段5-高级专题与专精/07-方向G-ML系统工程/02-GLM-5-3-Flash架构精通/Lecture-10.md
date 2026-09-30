---
title: Module 10 — Kernel Roofline（性能上界模型）与推理服务决策
description: Module 10 — Kernel Roofline（性能上界模型）与推理服务决策
---

# Module 10 — Kernel Roofline（性能上界模型）与推理服务决策

**合集：** [GLM-5.3-Flash Architecture Mastery](README.md) | **上一模块：** [← Module 09](Lecture-09.md) | **下一模块：** [Module 11 →](Lecture-11.md)

---

模块 01–09 给了你*预测*开销的算术。本模块讲的是把这些预测转化为一份性能剖析计划——以关于模型七个不同区域的可证伪假设来组织，因为「优化 GLM-5.3-Flash」不是一个实验，而「MoE（混合专家模型）dispatch 路径在批大小 4 时是否受 kernel 启动限制」才是。

---

## 学习目标

读完本模块，你应当能够：

1. 写出逐算子下界 roofline，并在任何性能剖析之前应用它。
2. 为该架构引入的七个区域各自陈述一个具体、可证伪的假设。
3. 解释为什么网络的延迟由其关键路径决定，而不是把每 GPU 的能力数字相加。
4. 解释对这种特定的混合状态，prefill（首字前的整段计算）/decode（逐 token 生成阶段）分离相比传统 Transformer 还需要什么。
5. 解释为什么减少填充计算可能完全无法改变延迟，以及何时应当减少。

---

## 1. 下界模型，先于任何性能剖析

对任意单个算子：

```text
   t_operator  ≳  max( operations / effective_compute_rate ,  bytes_moved / effective_bandwidth )
```

这与 [Hardware-Aware LLM Quantization — Module 01](../03-硬件感知LLM量化/Lecture-01.md) 中完整展开的 roofline 论证相同——在接触性能分析器*之前*先算出预测，然后用性能分析器解释预测与实测之间的差距，而不是从头发现预测。逐区域应用：

```text
   MoE expert GEMM             :  compute-bound at large batch (many tokens per expert),
                                   bandwidth-bound at batch 1 (Module 02's traffic argument)

   KDA recurrent update         :  the state is tiny (128×128/head) — almost certainly
                                   bandwidth/launch-bound, not compute-bound, at any
                                   realistic batch size

   MLA absorbed attention       :  depends on regime — Module 05 §5 already flagged
                                   this as compute-bound in prefill, bandwidth-bound
                                   in decode; profile BOTH, don't assume one

   DSA indexer scoring          :  scales with T/4 candidates (Module 06 §5) —
                                   likely bandwidth-bound on the candidate-latent read
```

然后检查 launch、依赖、同步和暴露的通信。**与实测相符的预测告诉你，你所建模的机制就是真正的瓶颈。与实测不符的预测告诉你，某个你未建模的东西——launch 开销、同步停滞、fallback kernel 路径——才是真正起约束作用的**，这比确认本身更有价值。

---

## 2. 七个区域，七个假设

围绕具体、可证伪的论断来组织性能剖析——而不是「让模型更快」：

| 区域 | 待检验假设 |
|---|---|
| **MoE 投影** | 专家行 occupancy 和填充决定 GEMM 效率；一个批的布线冲突之间的权重复用决定实际流量（[Module 02 §5](Lecture-02.md)） |
| **KDA decode** | 状态读/写流量、寄存器压力和溢出占主导——而非递推本身的算术（[Module 03](Lecture-03.md)） |
| **KDA prefill** | 分块利用率和复合转移计算带来的中间存储开销（[Module 04 §1](Lecture-04.md)）决定，分块在你的 chunk size 下是否真的胜过朴素循环 |
| **DSA indexer** | 候选池扫描流量和 top-k 选择开销随 `T/4` 缩放，而非随固定选择预算 `K` 缩放（[Module 06 §5](Lecture-06.md)） |
| **Sparse MLA** | Gather 局部性（被选中位置的 latent 在缓存中是连续还是分散？）和 latent 复用决定吸收形式计算上达到的带宽（[Module 05 §3](Lecture-05.md)） |
| **mHC + normalization** | 来自许多廉价坍缩/混合操作的小 kernel 启动开销和冗余激活值流量会累积，即便每个单独操作都很廉价（[Module 07 §4](Lecture-07.md)） |
| **多 GPU 执行** | 暴露的（未重叠的）集合通信、CPU 暂存和 NUMA 布局——而非每 GPU 的原始算力或带宽——决定这个特定 8-GPU、PCIe Gen4、无 P2P 拓扑上的端到端延迟 |

这些中的每一个都可追溯到本课程前面的某个具体推导——正是这种可追溯性，把「对模型做性能剖析」变成一份你真正能执行和解读的计划。

---


<details>
<summary>English original</summary>

**Module 10 — Kernel Roofline & Serving Decisions**

**Collection:** [GLM-5.3-Flash Architecture Mastery](README.md) | **Previous:** [← Module 09](Lecture-09.md) | **Next:** [Module 11 →](Lecture-11.md)

---

Modules 01–09 gave you the arithmetic to *predict* cost. This module is about turning those predictions into a profiling plan — organized as falsifiable hypotheses about seven distinct regions of the model, because "optimize GLM-5.3-Flash" is not an experiment and "is the MoE dispatch path launch-bound at batch size 4" is.

---

**Learning objectives**

By the end of this module you should be able to:

1. Write the per-operator lower-bound roofline and apply it before profiling anything.
2. State a specific, falsifiable hypothesis for each of the seven regions this architecture introduces.
3. Explain why a network's latency is governed by its critical path, not by summing per-GPU capability numbers.
4. Explain what prefill/decode disaggregation requires for this specific hybrid state, beyond what a conventional transformer needs.
5. Explain why reducing padded work can fail to move latency at all, and when it should.

---

**1. The lower-bound model, before you profile anything**

For any single operator:

```text
   t_operator  ≳  max( operations / effective_compute_rate ,  bytes_moved / effective_bandwidth )
```

This is the same roofline argument [Hardware-Aware LLM Quantization — Module 01](../03-硬件感知LLM量化/Lecture-01.md) develops in full — compute a prediction *before* touching a profiler, then use the profiler to explain the gap between prediction and measurement, not to discover the prediction from scratch. Apply it region by region:

```text
   MoE expert GEMM             :  compute-bound at large batch (many tokens per expert),
                                   bandwidth-bound at batch 1 (Module 02's traffic argument)

   KDA recurrent update         :  the state is tiny (128×128/head) — almost certainly
                                   bandwidth/launch-bound, not compute-bound, at any
                                   realistic batch size

   MLA absorbed attention       :  depends on regime — Module 05 §5 already flagged
                                   this as compute-bound in prefill, bandwidth-bound
                                   in decode; profile BOTH, don't assume one

   DSA indexer scoring          :  scales with T/4 candidates (Module 06 §5) —
                                   likely bandwidth-bound on the candidate-latent read
```

Then inspect launches, dependencies, synchronization, and exposed communication. **A prediction that matches measurement tells you the mechanism you modeled is the real bottleneck. A prediction that doesn't match tells you something you didn't model — launch overhead, synchronization stalls, a fallback kernel path — is what's actually binding**, which is a more valuable finding than confirmation would have been.

---

**2. Seven regions, seven hypotheses**

Organize profiling around specific, falsifiable claims — not "make the model faster":

| Region | Hypothesis to test |
|---|---|
| **MoE projections** | Expert-row occupancy and padding determine GEMM efficiency; weight reuse across a batch's routing collisions determines actual traffic ([Module 02 §5](Lecture-02.md)) |
| **KDA decode** | State read/write traffic, register pressure, and spills dominate — not the arithmetic of the recurrence itself ([Module 03](Lecture-03.md)) |
| **KDA prefill** | Chunk utilization and intermediate-storage overhead from the composed-transition computation ([Module 04 §1](Lecture-04.md)) determine whether chunking actually beats a naive loop at your chunk size |
| **DSA indexer** | Candidate-pool scan traffic and top-k selection cost scale with `T/4`, not with the fixed selection budget `K` ([Module 06 §5](Lecture-06.md)) |
| **Sparse MLA** | Gather locality (are selected positions' latents contiguous or scattered in cache?) and latent reuse determine achieved bandwidth on the absorbed-form computation ([Module 05 §3](Lecture-05.md)) |
| **mHC + normalization** | Small-kernel launch overhead and redundant activation traffic from many cheap collapse/mix operations accumulate even though each individual operation is inexpensive ([Module 07 §4](Lecture-07.md)) |
| **Multi-GPU execution** | Exposed (non-overlapped) collectives, CPU staging, and NUMA placement — not raw per-GPU compute or bandwidth — determine end-to-end latency on this specific 8-GPU, PCIe Gen4, no-P2P topology |

Every one of these traces to a specific derivation earlier in this course — that traceability is what turns "profile the model" into a plan you can actually execute and interpret.

---

</details>

## 3. 关键路径，而非能力总和

```text
   WRONG:   "8 GPUs × 1792 GB/s each = 14.3 TB/s of aggregate bandwidth,
             so the model should serve at roughly 14.3TB/s ÷ bytes/token."

   RIGHT:   latency is governed by the DEPENDENCY CHAIN through the
            computation — including every point where one GPU must
            wait for data from another before it can proceed.
```

这一点在所述部署拓扑上尤为关键：**PCIe Gen4，无 P2P**。在没有 GPU 到 GPU 的 peer-to-peer 传输的情况下，任何需要跨设备数据的操作（tensor 并行 attention 之后的全规约、MoE 专家分发的 all-to-all、[Module 09 §4](Lecture-09.md) 中的 MLA 潜在复制模式——如果你的实现需要它）的 GPU 间通信，都会走一条比 P2P 或 NVLink 所能提供的更慢的路径——而如果该通信没有与其他 GPU 上的计算 **重叠**，它就直接落在关键路径上，成为纯粹的额外延迟，在只累加单 GPU 能力的任何计算中都不可见。

```text
   ┌────────────────────────────────────────────────────────────────┐
   │  On a no-P2P, PCIe Gen4 topology specifically, ask for every     │
   │  cross-GPU operation: is this communication OVERLAPPED with      │
   │  useful compute on other devices, or is it EXPOSED — sitting     │
   │  on the critical path with nothing else happening while it       │
   │  waits? Exposed communication is where "aggregate bandwidth"     │
   │  arithmetic silently stops applying.                              │
   └────────────────────────────────────────────────────────────────┘
```

---

## 4. 范围界定正确的 prefill/decode 分离

分离部署把 prefill（首字前的整段计算）与 decode（逐 token 生成阶段）工作负载放到彼此独立的推理服务资源上，使二者可以被各自独立地供给和调度——SGLang 将其实现为一种推理服务系统能力，而这一通用模式是大规模推理服务的标准做法。有两件针对该架构的特性会改变正确实现所需的要素：

**交接传递的不只是 KV cache。** [Module 08 §3](Lecture-08.md) 已经建立了回滚必须对齐的完整状态清单；prefill→decode 交接必须原样传递的正是这同一份清单：

```text
   handoff payload  =  MLA latent cache (token-indexed, truncatable — Module 05)
                     +  KDA recurrent state (per-head, NOT token-indexed — Module 03)
                     +  convolution state (Module 04 §3)
                     +  DSA indexer selection metadata (Module 06)

   A disaggregation design that only moves "the KV cache" between prefill
   and decode workers is repeating the same incomplete-state mistake
   Module 08 named for speculative rollback — just at a different point
   in the serving pipeline.
```

**角色拆分并不意味着布局是白得的。** 对于你当前完整的八 GPU 布局，把 prefill 和 decode 角色拆到（比如）各 4 个 GPU 上 **并不会产生两个独立常驻的完整模型副本** ——你这是把一份布局的资源分给了两个角色，而不是把容量翻倍。完整 8-GPU 布局的两个真正独立的副本需要 16 个 GPU。把“我们现在有了独立的 prefill 和 decode 服务”与“我们现在有了更多推理服务容量”混为一谈，是一种容量规划错误，其形态与 [Module 09](Lecture-09.md) 的 `÷8` 陷阱相同：一个看似把某种资源翻倍、实际上只是重新分配某项固定资源的设计决策。

---

## 5. 把每个假设都当作实验

两个结果可能朝任一方向走的预测实例，在此重申：§2 表格的意义在于 *运行* 实验，而不是预设其结果：

```text
   "Reducing padded MoE arithmetic will speed up decode."

     Can be TRUE:  if MoE GEMM efficiency is the binding constraint at
                   your batch size.
     Can be FALSE: if the request is actually launch-bound or
                   communication-bound — removing padded FLOPs doesn't
                   touch either of those, and latency barely moves.


   "Increasing batch size will make [format/kernel choice] the better one."

     Batch size changes expert-routing collision rate (Module 02 §5),
     which changes achieved weight reuse, which can flip which kernel
     design is actually faster at the new operating point — a change
     in REGIME, not just in scale.
```

抽象地说，两种结果都谈不上更“正确”——二者都是真实的可能性，而哪一个适用于你的具体部署，正是性能剖析要回答的问题。报告假设、测量值和实际结果，包括结果为“无可测量影响”的情况——对于调试这一架构的团队而言，一份记录在案的、说明瓶颈 *不在* 何处的负面结果，与说明瓶颈在何处的正面结果完全同等有价值。

---


<details>
<summary>English original</summary>

**3. Critical path, not summed capability**

```text
   WRONG:   "8 GPUs × 1792 GB/s each = 14.3 TB/s of aggregate bandwidth,
             so the model should serve at roughly 14.3TB/s ÷ bytes/token."

   RIGHT:   latency is governed by the DEPENDENCY CHAIN through the
            computation — including every point where one GPU must
            wait for data from another before it can proceed.
```

This matters specifically on the stated deployment topology: **PCIe Gen4, no P2P**. Without peer-to-peer GPU-to-GPU transfers, inter-GPU communication for any operation requiring cross-device data (an all-reduce after tensor-parallel attention, an all-to-all for MoE expert dispatch, the MLA latent-replication pattern from [Module 09 §4](Lecture-09.md) if your implementation needs it) routes through a slower path than P2P or NVLink would provide — and if that communication is not **overlapped** with compute on other GPUs, it sits directly on the critical path as pure added latency, invisible to any calculation that only sums per-GPU capability.

```text
   ┌────────────────────────────────────────────────────────────────┐
   │  On a no-P2P, PCIe Gen4 topology specifically, ask for every     │
   │  cross-GPU operation: is this communication OVERLAPPED with      │
   │  useful compute on other devices, or is it EXPOSED — sitting     │
   │  on the critical path with nothing else happening while it       │
   │  waits? Exposed communication is where "aggregate bandwidth"     │
   │  arithmetic silently stops applying.                              │
   └────────────────────────────────────────────────────────────────┘
```

---

**4. Prefill/decode disaggregation, correctly scoped**

Disaggregation places prefill and decode workloads on separate serving resources so each can be provisioned and scheduled independently — SGLang implements this as a serving-system capability, and the general pattern is standard practice for large-scale serving. Two things specific to this architecture change what a correct implementation needs:

**The handoff carries more than a KV cache.** [Module 08 §3](Lecture-08.md) already established the full state inventory a rollback must reconcile; the exact same inventory is what a prefill→decode handoff must transfer intact:

```text
   handoff payload  =  MLA latent cache (token-indexed, truncatable — Module 05)
                     +  KDA recurrent state (per-head, NOT token-indexed — Module 03)
                     +  convolution state (Module 04 §3)
                     +  DSA indexer selection metadata (Module 06)

   A disaggregation design that only moves "the KV cache" between prefill
   and decode workers is repeating the same incomplete-state mistake
   Module 08 named for speculative rollback — just at a different point
   in the serving pipeline.
```

**Placement is not free just because roles are split.** For your current full eight-GPU placement, splitting prefill and decode roles onto (say) 4 GPUs each **does not create two independently-resident full replicas of the model** — you have divided one placement's resources between two roles, not doubled your capacity. Two genuinely independent replicas of the full 8-GPU placement would require 16 GPUs. Confusing "we now have separate prefill and decode services" with "we now have more serving capacity" is a capacity-planning error with the same shape as [Module 09](Lecture-09.md)'s `÷8` trap: a design decision that looks like it multiplies a resource, when it actually just reallocates a fixed one.

---

**5. Treat every hypothesis as an experiment**

Two concrete examples of predictions that can go either way, stated as a reminder that the point of §2's table is to *run* the experiments, not to assume their outcomes:

```text
   "Reducing padded MoE arithmetic will speed up decode."

     Can be TRUE:  if MoE GEMM efficiency is the binding constraint at
                   your batch size.
     Can be FALSE: if the request is actually launch-bound or
                   communication-bound — removing padded FLOPs doesn't
                   touch either of those, and latency barely moves.


   "Increasing batch size will make [format/kernel choice] the better one."

     Batch size changes expert-routing collision rate (Module 02 §5),
     which changes achieved weight reuse, which can flip which kernel
     design is actually faster at the new operating point — a change
     in REGIME, not just in scale.
```

Neither outcome is more "correct" in the abstract — both are real possibilities, and which one holds for your specific deployment is exactly what profiling is for. Report the hypothesis, the measurement, and the actual outcome, including when the outcome was "no measurable effect" — a documented negative result about where the bottleneck *isn't* is exactly as valuable to a team debugging this architecture as a positive result about where it is.

---

</details>

## 检查点

现在应当能够：

1. 在运行性能分析器之前，对七个区域中至少三个应用逐算子的 roofline（性能上界模型）下界。
2. 对 §2 中七个区域各给出一个具体、可证伪的假设。
3. 解释为什么在 no-P2P 拓扑上，把八块 GPU 的带宽或算力相加会高估可达吞吐。
4. 列出正确的 prefill（首字前的整段计算）/decode（逐 token 生成阶段）分离为这一架构必须传输的完整状态交接载荷。
5. 解释为什么把一个 8-GPU 布局的角色拆分，并不等同于增加容量。

---

## 交付

为自己的部署产出一份 **性能剖析假设日志**：§2 表格中每个区域一行，写明你预测的瓶颈（来自 §1 的 roofline 模型）、实际测量值、一致或不一致，以及——关键在于——至少一行假设是 **错误** 的，并解释由此了解到的真实瓶颈。一份没有任何错误假设的日志，说明你剖析的都是自己早已理解的东西，而不是一套优化良好的系统。

---

## 内容时效

* **不受时效影响：** 逐算子的 roofline 下界、能力不能相加而应看关键路径的论点、通用分离模式及其容量规划陷阱。
* **与部署相关：** "PCIe Gen4, no P2P, 8× RTX 5090" 拓扑是本课程点名的案例研究——在复用本模块关于通信在何处成为瓶颈的结论之前，对任何不同的互连（NVLink、InfiniBand）重新推导暴露与重叠通信的分析。

---

**下一步：** [Module 11 — Correctness as Architecture Mastery →](Lecture-11.md)


<details>
<summary>English original</summary>

**Checkpoint**

You should now be able to:

1. Apply the per-operator roofline lower bound before running a profiler, for at least three of the seven regions.
2. State a specific, falsifiable hypothesis for each of the seven regions in §2.
3. Explain why summing eight GPUs' bandwidth or compute overstates achievable throughput on a no-P2P topology.
4. List the full state-handoff payload a correct prefill/decode disaggregation must transfer for this architecture.
5. Explain why splitting one 8-GPU placement's roles is not the same as adding capacity.

---

**Ship it**

Produce a **profiling hypothesis log** for your own deployment: one row per region from §2's table, with your predicted bottleneck (from the roofline model in §1), the actual measurement, agreement or disagreement, and — critically — at least one row where the hypothesis was **wrong**, with an explanation of what you learned about the real bottleneck instead. A log with zero wrong hypotheses is a sign you profiled things you already understood, not a sign of a well-optimized system.

---

**Current as of**

* **Timeless:** the per-operator roofline lower bound, the critical-path-not-summed-capability argument, the general disaggregation pattern and its capacity-planning trap.
* **Deployment-specific:** the "PCIe Gen4, no P2P, 8× RTX 5090" topology is this course's named case study — re-derive the exposed-versus-overlapped communication analysis for any different interconnect (NVLink, InfiniBand) before reusing this module's conclusions about where communication becomes a bottleneck.

---

**Next:** [Module 11 — Correctness as Architecture Mastery →](Lecture-11.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/GLM-5.3-Flash Architecture Mastery/Lecture-10.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/GLM-5.3-Flash%20Architecture%20Mastery/Lecture-10.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
