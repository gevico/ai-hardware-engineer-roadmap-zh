---
title: 加速器平台评估：NVIDIA vs AMD vs Google 加速器
description: 加速器平台评估：NVIDIA vs AMD vs Google 加速器
---

# 加速器平台评估：NVIDIA vs AMD vs Google 加速器

<div class="course-identity auto-course" style="--course-accent: #2563eb; --course-accent-rgb: 37, 99, 235;" markdown="1">
<div class="course-identity__icon">APEN</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入解析 · 专项化</p>
<p class="course-identity__title">加速器平台评估：NVIDIA vs AMD vs Google 加速器的专项课程标识。</p>
<p class="course-identity__meta">产物：专项案例研究 · 度量：性能、可靠性、角色匹配度</p>
</div>
</div>


**父级：** [GPU Infrastructure](../Guide.md)

**时间线：** 首轮 1-2 周，之后每 6-12 个月复查一次，因为硬件、runtime 和价格都在变。

**前置要求：** 阶段 3 方向 B（ML 工程与 MLOps）、阶段 4 方向 C（编译器 + 推理优化），以及本阶段 5 小节中至少一个厂商专属方向。

---

## 为什么需要这份指南

大多数平台对比都太浅。

它们通常止步于：

- 峰值 FLOPS
- 厂商营销宣称
- 一张 benchmark 图表

真实团队并不是这样选基础设施的。

真实的平台选型通常由以下因素驱动：

- 你的框架栈是否已经兼容
- 多节点 bring-up（上电点亮/调通）有多痛苦
- 调试工具是否成熟
- 你的工作负载是带宽受限、网络受限还是编译器受限
- 你的团队是否真能让这个集群保持忙碌

本指南就是为这项决策写的研究与评估笔记。

它刻意保持务实：

- 训练和推理都重要
- 集群运维重要
- 调试与故障处理重要
- 工程师时间也是成本的一部分

---

## 决策框架

用本指南回答一个问题：

> 对我的工作负载和团队而言，哪个加速器平台能在性能、可运维性和成本之间给出最佳组合？

三条重要规则：

1. 不要只按理论 FLOPS 比较加速器。
2. 不要在不按可用吞吐或训练耗时归一化的情况下比较云上标价。
3. 不要忽视软件成熟度。便宜 15% 但要花 3 倍时间才能稳定的平台，实际往往更贵。

---

## 执行摘要

| 平台 | 最强之处 | 最弱之处 | 最佳适配 |
|---|---|---|---|
| **NVIDIA GPU** | 框架支持最广、底层工具最强、多节点 playbook 最成熟、自定义 CUDA 与生产推理的路径最快 | 通常是成本最高的路线，供货与集群可用性可能很麻烦，容易在高端系统上超支 | 需要在 PyTorch、TensorFlow、JAX、Triton、vLLM、自定义 kernel 和混合工作负载之间摩擦最小的团队 |
| **AMD GPU** | 单加速器内存大、PyTorch 与 vLLM 栈越来越可信、工具更开放、受支持模型能很好适配 ROCm 时性价比高 | 仍需更多工作负载验证、版本漂移风险更大、比 CUDA 生态更少“默认安全”的 recipe | 优化内存密集型 LLM 推理服务或训练、且愿意做更多栈验证的团队 |
| **Google 加速器（TPU）** | pod 规模训练故事强、XLA 原生工作负载的性能/TCO 高、JAX 适配极佳、Google 运营的机队模式成熟 | 技术栈约束最强、与云和编译器耦合更深、运维模式可移植性差、动态 shape 出错代价高昂 | 已经对齐 JAX/XLA，或在 Google Cloud 上跑大规模规整训练任务的团队 |

简版：

- 如果希望跨多种工作负载平稳走向生产环境的概率最高，选 **NVIDIA**。
- 如果想要一个越来越有力的替代方案，且内存占用是一等约束，把 **AMD** 列入候选。
- 如果团队已经按 XLA 的方式组织，任务规模大、规整且原生跑在 Google Cloud 上，**TPU** 可能在经济性和扩展性上是最佳选择。

---

## 1. 真实工作负载下的性能


<details>
<summary>English original</summary>

**Accelerator Platform Evaluation: NVIDIA vs AMD vs Google Accelerators**

<div class="course-identity auto-course" style="--course-accent: #2563eb; --course-accent-rgb: 37, 99, 235;" markdown="1">
<div class="course-identity__icon">APEN</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Specialization</p>
<p class="course-identity__title">Specialized course identity for Accelerator Platform Evaluation: NVIDIA vs AMD vs Google Accelerators.</p>
<p class="course-identity__meta">Artifact: specialization case study · Measure: performance, reliability, role fit</p>
</div>
</div>


**Parent:** [GPU Infrastructure](../Guide.md)

**Timeline:** 1-2 weeks for a first pass, then revisit every 6-12 months as hardware, runtimes, and pricing move.

**Prerequisites:** Phase 3 Track B (ML Engineering and MLOps), Phase 4 Track C (compiler + inference optimization), and at least one vendor-specific track in this Phase 5 section.

---

**Why this guide exists**

Most platform comparisons are too shallow.

They usually stop at:

- peak FLOPS
- vendor marketing claims
- one benchmark chart

That is not how real teams choose infrastructure.

Real platform selection is usually driven by:

- whether your framework stack is already compatible
- how painful multi-node bring-up is
- whether debugging tools are mature
- whether your workload is memory-bound, network-bound, or compiler-bound
- whether your team can actually keep the cluster busy

This guide is a research and evaluation note for that decision.

It is intentionally practical:

- training and inference both matter
- cluster operations matter
- debugging and failure handling matter
- engineer time is part of cost

---

**Decision frame**

Use this guide to answer one question:

> For my workload and team, which accelerator platform creates the best combination of performance, operability, and cost?

Three important rules:

1. Do not compare accelerators only by theoretical FLOPS.
2. Do not compare cloud list prices without normalizing for usable throughput or time-to-train.
3. Do not ignore software maturity. A platform that is 15% cheaper but takes 3x longer to stabilize is often more expensive in practice.

---

**Executive summary**

| Platform | Where it is strongest | Where it is weakest | Best fit |
|---|---|---|---|
| **NVIDIA GPU** | Broadest framework support, strongest low-level tooling, best-known multi-node playbooks, fastest path for custom CUDA and production inference | Usually the most expensive path, supply and cluster availability can be painful, easy to overspend on premium systems | Teams that need the least friction across PyTorch, TensorFlow, JAX, Triton, vLLM, custom kernels, and mixed workloads |
| **AMD GPU** | Large memory per accelerator, increasingly credible PyTorch and vLLM stack, more open tooling, strong value when supported models fit ROCm well | Still more workload qualification, more version-skew risk, fewer "default safe" recipes than CUDA ecosystems | Teams optimizing memory-heavy LLM serving or training and willing to do more stack validation |
| **Google accelerators (TPU)** | Strong pod-scale training story, high performance/TCO for XLA-native workloads, excellent JAX fit, mature Google-operated fleet model | Most opinionated stack, more cloud and compiler coupling, less portable operational model, dynamic-shape mistakes are expensive | Teams already aligned to JAX/XLA or large regular training jobs on Google Cloud |

Short version:

- If you want the highest probability of a smooth production path across many workloads, choose **NVIDIA**.
- If you want an increasingly serious alternative and memory footprint is a first-class constraint, shortlist **AMD**.
- If your team is already XLA-shaped and your jobs are large, regular, and Google Cloud-native, **TPU** can be the best economic and scaling choice.

---

**1. Performance in real workloads**

</details>

### LLM 训练

对大型 Transformer 训练而言，实际问题不只是原始数学吞吐。而是：

- 框架对你的图 lowering 得有多好
- 集合通信扩展得有多高效
- 在张量并行或流水线并行成为必须之前，你能拿到多少 HBM
- 每一步要付出多少 host 与编译器开销

**NVIDIA** 对 PyTorch 重度训练栈仍是最稳妥的通用答案，因为 CUDA、NCCL、Transformer Engine、Megatron-LM、TensorRT-LLM 以及更广泛的生态通常最先获得支持。

**AMD** 如今已是受支持的 Transformer 工作负载的有力竞争者，尤其当 MI300X 级系统的大内存占用让你可以避开一部分模型分片压力时。这一内存优势能简化模型布局并降低通信压力，在很多实际任务中比峰值数学算力更重要。

**TPU** 在代码库已按 JAX/XLA 或 TensorFlow/XLA 塑形、张量形状稳定且图结构规整时最强。在这种情形下，TPU pod 扩展干净，Google 也提供非常大的 slice 与 multislice 配置。如果你的工作负载反复重编译、动态形状用得不好，或依赖 CUDA 风格的自定义 kernel，TPU 的吸引力就大打折扣。

### LLM 推理

推理分化为两个不同的问题：

- **prefill 密集型吞吐**
- **decode 密集型 token 生成**

对长上下文与大模型推理，内存容量与内存带宽占主导。

**NVIDIA** 在你需要最成熟的推理服务栈与最广泛的模型支持时通常最强。TensorRT-LLM、Triton、vLLM 支持，以及性能剖析/调试工具，使其成为有严格 SLO 的生产部署最省力的路径。

**AMD** 在每 GPU 大 HBM 能减少碎片化、或让你以更低的分片强度装下模型时尤其有吸引力。实践中，这对 70B+ 与 405B 级模型影响很大。AMD 的推理表现比许多团队以为的要好，但具体模型与引擎版本的影响比在 NVIDIA 上更大。

**TPU** 在推理服务系统已与 TPU 原生 runtime 及 Google Cloud 部署模式对齐时，可以服务得很好。当你需要广泛的开源引擎精度一致性、异构推理服务栈，或跨云与裸机的低摩擦可移植性时，它就没那么有吸引力。

### CNN、视觉与较老的稠密工作负载

对 CNN 等传统稠密工作负载以及许多既有训练流水线：

- **TPU** 在图规整且对 XLA 友好时往往表现很好
- **NVIDIA** 在最大范围的框架与算子上，运维上始终最省心
- **AMD** 可行，但你应该验证具体的算子与框架路径，而不要假定精度一致

---

## 2. 软件栈成熟度

### NVIDIA

NVIDIA 的优势不只是硬件。而是栈的深度：

- CUDA
- cuDNN
- NCCL
- TensorRT
- TensorRT-LLM
- Nsight Systems / Nsight Compute
- 围绕 GPU Operator 与 NGC 的 Kubernetes 及容器工具链

这种成熟度以务实的方式改变了运维：

- 更多经过验证的 recipe
- 更快的问题分诊
- 更多分布式训练与推理的示例
- 更广泛的第三方厂商支持

它仍是其他生态拿来对标的参考栈。

### AMD

ROCm 已有大幅改进，尤其是在：

- ROCm 上的 PyTorch
- ROCm 上的 vLLM
- Megatron-LM 与部分训练容器
- 用 Omniperf、Omnitrace 与 rocProfiler 做性能剖析
- 通过 RCCL 做集合通信

实际的限制不在于 AMD 缺栈。而在于这个栈仍然更不容错：

- 受支持的模型矩阵更重要
- 精确的版本组合更重要
- 某些工作负载仍需更多验证或 workaround 调优

如果你对版本管理严格自律，并愿意在自己的真实模型上测试，ROCm 是可行的。如果你期待「在 CUDA 上能跑的都应该直接能跑」，你会失望。

### Google 加速器

当你接受 XLA 世界观时，Google 的栈最强：

- JAX
- TensorFlow/XLA
- PyTorch/XLA
- XProf
- 基于 XLA/HLO 的性能剖析与调试
- 在 Google 基础设施上做 slice 与 multislice TPU 部署

这个栈非常适合：

- 大型规整训练任务
- JAX 原生团队
- 已在 Google Cloud 上运营的团队

它在以下方面较弱：

- CUDA 风格的自定义底层 kernel 实验
- 需要最大化可移植性的组织
- 不希望编译器行为塑造模型代码模式的团队

---

## 3. 部署与运维难度


<details>
<summary>English original</summary>

**LLM training**

For large transformer training, the practical question is not only raw math throughput. It is:

- how well the framework lowers your graph
- how efficiently collectives scale
- how much HBM you get before tensor or pipeline parallelism becomes mandatory
- how much host and compiler overhead you pay every step

**NVIDIA** remains the safest general answer for PyTorch-heavy training stacks because CUDA, NCCL, Transformer Engine, Megatron-LM, TensorRT-LLM, and the broader ecosystem are usually supported first.

**AMD** is now a serious contender for supported transformer workloads, especially when the large memory footprint of MI300X-class systems lets you avoid some model sharding pressure. That memory advantage can simplify model placement and reduce communication pressure, which matters more than peak math in many real jobs.

**TPU** is strongest when the codebase is already shaped for JAX/XLA or TensorFlow/XLA, with stable tensor shapes and disciplined graph structure. In that regime, TPU pods scale cleanly and Google exposes very large slice and multislice configurations. If your workload repeatedly recompiles, uses dynamic shapes poorly, or depends on custom CUDA-style kernels, TPU becomes much less attractive.

**LLM inference**

Inference splits into two different problems:

- **prefill-heavy throughput**
- **decode-heavy token generation**

For long-context and large-model inference, memory capacity and memory bandwidth dominate.

**NVIDIA** is usually strongest when you need the most mature serving stack and the widest model support. TensorRT-LLM, Triton, vLLM support, and profiling/debugging tools make it the easiest path for production deployments with strict SLOs.

**AMD** becomes especially attractive when large HBM per GPU reduces fragmentation or lets you fit a model with less aggressive sharding. In practice, that can matter a lot for 70B+ and 405B-class models. AMD's inference story is better than many teams assume, but the exact model and engine version matter more than on NVIDIA.

**TPU** can work very well for serving when the serving system is already aligned with TPU-native runtimes and Google Cloud deployment patterns. It is less attractive when you need broad open-source engine parity, heterogeneous serving stacks, or low-friction portability across clouds and bare metal.

**CNNs, vision, and older dense workloads**

For conventional dense workloads such as CNNs and many established training pipelines:

- **TPU** often performs very well when the graph is regular and XLA-friendly
- **NVIDIA** stays easiest operationally across the largest set of frameworks and operators
- **AMD** is viable, but you should validate exact operator and framework paths rather than assuming parity

---

**2. Software stack maturity**

**NVIDIA**

The NVIDIA advantage is not just hardware. It is the stack depth:

- CUDA
- cuDNN
- NCCL
- TensorRT
- TensorRT-LLM
- Nsight Systems / Nsight Compute
- Kubernetes and container tooling around GPU Operator and NGC

That maturity changes operations in a practical way:

- more known-good recipes
- faster issue triage
- more examples for distributed training and inference
- broader third-party vendor support

This is still the reference stack other ecosystems are measured against.

**AMD**

ROCm has improved substantially, especially for:

- PyTorch on ROCm
- vLLM on ROCm
- Megatron-LM and selected training containers
- profiling with Omniperf, Omnitrace, and rocProfiler
- collectives via RCCL

The practical limitation is not that AMD lacks a stack. It is that the stack is still less forgiving:

- supported model matrices matter more
- exact version combinations matter more
- some workloads still need more validation or workaround tuning

If you are disciplined about versioning and willing to test on your real models, ROCm can be viable. If you expect "everything that works on CUDA should just work," you will be disappointed.

**Google accelerators**

Google's stack is strongest when you accept the XLA worldview:

- JAX
- TensorFlow/XLA
- PyTorch/XLA
- XProf
- XLA/HLO-based profiling and debugging
- slice and multislice TPU deployment on Google infrastructure

This stack is very good for:

- large regular training runs
- JAX-native teams
- teams already operating on Google Cloud

It is weaker for:

- custom low-level kernel experimentation in the CUDA style
- organizations that need maximum portability
- teams that do not want compiler behavior to shape model code patterns

---

**3. Deployment and operational difficulty**

</details>

### NVIDIA

NVIDIA 是最容易部署的平台，覆盖最广泛的环境：

- 本地集群
- 托管机房
- 所有主流云
- Kubernetes
- Slurm

在运维上，这让 NVIDIA 在混合机群和企业环境中拥有最大的优势。

NVIDIA 的典型痛点有：

- driver/container/CUDA 版本匹配
- NCCL 拓扑意外
- scale-out 的网络调优
- 高端系统的成本和可获得性

### AMD

AMD 的部署正在改善，但它仍然要求运维更有纪律性。

需要更密切关注：

- ROCm 版本匹配
- kernel 和驱动支持
- 经验证的容器
- 框架特定的支持说明

好处是这套栈更开放，文档也越来越完善。坏处是拥有深厚 ROCm 运维能力的团队更少，因此组织学习往往更慢。

### Google 加速器

如果你接受 Google Cloud 的运营模式，TPU 部署在概念上更简单。

这意味着：

- TPU VM 或基于 GKE 的 TPU 使用
- runtime 版本选择
- 排队资源或预留
- slice 和 multislice 编排

这可能比自己搭建裸金属集群更容易，但它并不是日常 GPU 意义上的“简单”。它是一种不同的运营模式，有不同的失效模式、配额和调度行为。

---

## 4. 扩展行为

### NVIDIA 的 scale-up 和 scale-out

NVIDIA 拥有最成熟的公开 playbook，覆盖：

- 用 NVLink 和 NVSwitch 进行节点内 scale-up
- 用 InfiniBand 和 GPUDirect RDMA 进行节点间 scale-out
- 通过 NCCL 实现拓扑感知的集合通信

这很重要，因为许多“硬件性能”问题实际上是通信问题。

当团队说 NVIDIA 扩展性更好时，他们通常是指：

- 框架默认值调优得更好
- 存在更多参考集群架构
- 调试集合通信更容易
- 更多工程师知道正常的失效长什么样

### AMD 的扩展行为

AMD 的扩展故事是真实的，但在整个行业中不够标准化。

核心要素都已具备：

- 节点内的 Infinity Fabric / xGMI
- 用于集合通信的 RCCL
- 基于 InfiniBand、RoCE 和 RDMA 的多节点通信

实际差异在于生态密度。经过实战检验的公开 runbook 更少，所以你的团队往往必须自行验证更多的扩展路径。

### TPU 的扩展行为

当工作负载适配该模型时，TPU 扩展是 Google 最强的卖点之一。

该平台提供：

- slice 内专用的芯片间互连
- 通过数据中心网络进行 multislice 训练
- 面向常规训练作业的超大规模配置

但 TPU 的扩展效率与以下因素紧密耦合：

- 分片策略
- 编译器行为
- shape 稳定性
- host/device 同步纪律

这些做得好时，TPU 扩展非常出色。做得不好时，故障通常更难被原生 GPU 团队推理清楚。

---

## 5. 工具、性能剖析与调试

| 领域 | NVIDIA | AMD | Google 加速器 |
|---|---|---|---|
| **kernel / timeline 性能剖析** | Nsight Systems, Nsight Compute | Omniperf, Omnitrace, rocProfiler | XProf, XLA trace, TensorBoard 集成 |
| **集合通信调试** | NCCL 日志、拓扑工具、DCGM 生态 | RCCL 日志和 ROCm 工具 | Megascale 统计、XProf multislice 分析 |
| **框架级调试** | 深度的 PyTorch 和 TensorRT 生态支持 | PyTorch 的 ROCm 支持在改善，但更依赖工作负载 | JAX/XLA 和 PyTorch/XLA 指标是核心 |
| **算子 lower / 编译器可见性** | 对 CUDA kernel 和常见框架栈很强 | 在改善，但对边缘情况覆盖不够广 | 如果你愿意用 XLA/HLO 术语推理，非常强 |

实际结论：

- **NVIDIA** 拥有最好的全方位调试体验。
- **AMD** 有可信的工具，但精通它们的工程师更少。
- **TPU** 有强大的编译器和性能剖析可见性，但心智模型更专门。

---

## 6. 可靠性与运维痛点

### NVIDIA：痛点通常是规模和复杂度

NVIDIA 的问题通常不是“它能用吗？”，而是：

- 如何在规模上保持稳定
- 如何调优通信
- 如何在升级后避免静默的性能回退
- 如何控制高端机群的成本

换句话说，栈很成熟，但系统很复杂。

### AMD：痛点通常是验证和版本纪律

AMD 的问题更多是：

- 精确的库兼容性
- 对给定模型路径缺失或较弱的支持
- 跨 ROCm 或框架版本的回归
- 需要自行验证更多假设

对于有纪律的基础设施团队来说，这可以应付。对于期望能即插即用替代 CUDA 的团队来说，则令人沮丧。


<details>
<summary>English original</summary>

**NVIDIA**

NVIDIA is the easiest platform to deploy across the widest range of environments:

- on-prem clusters
- colocation
- all major clouds
- Kubernetes
- Slurm

Operationally, this gives NVIDIA the biggest advantage in mixed fleets and enterprise settings.

Typical NVIDIA pain points are:

- driver/container/CUDA version matching
- NCCL topology surprises
- network tuning for scale-out
- cost and availability of premium systems

**AMD**

AMD deployment is improving, but it still rewards more operator discipline.

Expect to pay closer attention to:

- ROCm version matching
- kernel and driver support
- validated containers
- framework-specific support notes

The upside is that the stack is more open and increasingly better documented. The downside is that fewer teams have deep ROCm operations muscle, so organizational learning is often slower.

**Google accelerators**

TPU deployment is conceptually simpler if you accept the Google Cloud operating model.

That means:

- TPU VMs or GKE-based TPU use
- runtime-version selection
- queued resources or reservations
- slice and multislice orchestration

This can be easier than building your own bare-metal cluster, but it is not "simple" in the everyday GPU sense. It is a different operations model with different failure modes, quotas, and scheduling behavior.

---

**4. Scaling behavior**

**NVIDIA scale-up and scale-out**

NVIDIA has the most mature public playbook for:

- intra-node scale-up with NVLink and NVSwitch
- inter-node scale-out with InfiniBand and GPUDirect RDMA
- topology-aware collectives through NCCL

This matters because many "hardware performance" problems are really communication problems.

When teams say NVIDIA scales better, they often mean:

- framework defaults are better tuned
- more reference cluster architectures exist
- debugging collectives is easier
- more engineers know what normal failure looks like

**AMD scale behavior**

AMD's scale story is real, but less normalized across the industry.

The core ingredients are there:

- Infinity Fabric / xGMI inside nodes
- RCCL for collectives
- InfiniBand, RoCE, and RDMA-based multi-node communication

The practical difference is ecosystem density. There are fewer battle-tested public runbooks, so your team often has to validate more of the scaling path itself.

**TPU scale behavior**

TPU scaling is one of Google's strongest stories when the workload fits the model.

The platform exposes:

- dedicated inter-chip interconnect inside slices
- multislice training over the data-center network
- very large scaling configurations for regular training jobs

But TPU scale efficiency is tightly coupled to:

- sharding strategy
- compiler behavior
- shape stability
- host/device synchronization discipline

When those are good, TPU scale is excellent. When they are not, the failure is usually harder for GPU-native teams to reason about.

---

**5. Tooling, profiling, and debugging**

| Area | NVIDIA | AMD | Google accelerators |
|---|---|---|---|
| **Kernel / timeline profiling** | Nsight Systems, Nsight Compute | Omniperf, Omnitrace, rocProfiler | XProf, XLA traces, TensorBoard integrations |
| **Collective debugging** | NCCL logs, topology tools, DCGM ecosystem | RCCL logs and ROCm tooling | Megascale stats, XProf multislice analysis |
| **Framework-level debugging** | Deep PyTorch and TensorRT ecosystem support | PyTorch ROCm support improving, but more workload-dependent | JAX/XLA and PyTorch/XLA metrics are central |
| **Operator lowering / compiler visibility** | Strong for CUDA kernels and common framework stacks | Improving, but less broad for edge cases | Very strong if you are willing to reason in XLA/HLO terms |

Practical conclusion:

- **NVIDIA** has the best all-around debugging ergonomics.
- **AMD** has credible tools, but fewer engineers are fluent in them.
- **TPU** has powerful compiler and profiling visibility, but the mental model is more specialized.

---

**6. Reliability and operational pain points**

**NVIDIA: the pain is usually scale and complexity**

NVIDIA problems are often not "does it work?" but:

- how to keep it stable at scale
- how to tune communication
- how to avoid silent performance regressions after upgrades
- how to control cost on premium fleets

In other words, the stack is mature, but the systems are complex.

**AMD: the pain is usually qualification and version discipline**

AMD problems are more often:

- exact library compatibility
- missing or weaker support for a given model path
- regressions across ROCm or framework versions
- the need to validate more assumptions yourself

This is manageable for disciplined infra teams. It is frustrating for teams expecting drop-in CUDA equivalence.

</details>

### Google 加速器：痛点通常是编译器和平台耦合

TPU 的问题更常表现为：

- shape 漂移引发的重编译
- host/device 停顿
- XLA lowering 意外
- 配额、队列或 slice 分配摩擦
- 只能通过编译器和图产物调试，而非仅通过 kernel 调试

抽象地看，这并不比 GPU 运维更糟。只是所需技能栈不同。

---

## 7. 成本与性能取舍

不要把这个决策简化为标价。

真正的等式更接近：

```text
usable performance per dollar
  = (tokens/sec or time-to-train or served requests/sec)
    / (accelerator cost + network cost + storage cost + engineer time)
```

### NVIDIA

NVIDIA 通常在以下方面胜出：

- 首个可用系统的交付时间
- 支持的工作负载广度
- 推理引擎成熟度
- 组织风险低

它通常在以下方面落败：

- 采购成本
- 云实例价格
- 顶配系统的供货压力

如果工程师时间昂贵且部署速度重要，NVIDIA 往往仍具备最佳的总经济性。

### AMD

AMD 通常在以下情况胜出：

- 显存容量改变了推理服务或训练设计
- 工作负载已在 ROCm 上验证
- 组织愿意投入平台熟练度

AMD 可能在以下情况落败：

- 工作负载依赖不成熟或缺失的路径
- 团队没有 ROCm 调试经验
- 假设了可移植性但未实际测试

### Google 加速器

Google 公布 TPU 的每 chip-hour 定价，并将 v6e 定位为面向 Transformer、text-to-image、CNN 训练、微调和推理服务的高价值产品。实践中，TPU 的经济性在以下情况下最强：

- 工作负载在 XLA 下高效运行
- slice 利用率良好
- 团队已在 Google Cloud 上
- 训练任务规模大且规律，足以证明平台形态的合理性

TPU 的经济性在以下情况下较弱：

- 任务高度不规律
- 可移植性重要
- 团队大多是 PyTorch/CUDA 原生，需支付高额适配税

---

## 8. 各平台最强与最弱之处

### NVIDIA 最强的情况

- 需要最广泛的生产支持
- 依赖自定义 CUDA kernel 或 CUDA 邻近库
- 想要支持最好的多 GPU 和多节点方案
- 当下需要成熟的 LLM 推理工具链

### NVIDIA 最弱的情况

- 预算是主导约束
- 针对你的模型形态，每设备显存的经济性不佳
- 为中等工作负载过度采购高端基础设施

### AMD 最强的情况

- 大 HBM 容量显著简化模型布局
- 工作负载已在 ROCm 上验证
- 想要开放替代方案并愿意调优
- 关心受支持模型上的推理或训练价值

### AMD 最弱的情况

- 需要通用的框架精度一致性
- 团队没有 ROCm 运维经验
- 技术栈依赖 CUDA 优先的自定义 kernel

### Google 加速器最强的情况

- 已是 JAX/XLA 原生
- 工作负载规律且规模足够大，可利用 slice 级扩展
- 想要 Google 托管的加速器基础设施，而非自建簇
- 大型训练任务的成本/TCO 比可移植性更重要

### Google 加速器最弱的情况

- 需要最大的云或裸金属可移植性
- 代码库重度依赖动态 shape
- 组织需要 CUDA 风格的低层控制和调试习惯

---

## 9. 实用建议矩阵

选择 **NVIDIA 优先**，如果：

- 你的公司重度使用 PyTorch
- 需要最广泛的模型和工具兼容性
- 预期会有自定义 kernel、激进的推理优化，或研究生产混合工作负载

选择 **AMD 次之**，如果：

- 你有具体的显存密集型工作负载
- 你能在真实模型上 benchmark 后再做决定
- 你的团队愿意把 ROCm 当作需要验证的平台，而非盲目信任

选择 **TPU 优先**，如果：

- 你的团队已熟悉 JAX/XLA 或愿意变得熟悉
- 训练是重心
- Google Cloud 是可接受的基础设施锚点
- 你想要 pod 级效率，而非最大可移植性

三者都试点，如果：

- 你正在建设严肃的内部平台团队
- 年度加速器支出足够高，平台套利有意义
- 工作负载足够大，10-20% 的基础设施差异能快速收回成本

---


<details>
<summary>English original</summary>

**Google accelerators: the pain is usually compiler and platform coupling**

TPU problems more often look like:

- recompilation caused by shape drift
- host/device stalls
- XLA lowering surprises
- quota, queue, or slice-allocation friction
- debugging through compiler and graph artifacts rather than only through kernels

This is not worse than GPU operations in the abstract. It is just a different skills stack.

---

**7. Cost and performance tradeoffs**

Do not reduce this decision to list price.

The real equation is closer to:

```text
usable performance per dollar
  = (tokens/sec or time-to-train or served requests/sec)
    / (accelerator cost + network cost + storage cost + engineer time)
```

**NVIDIA**

NVIDIA usually wins on:

- time-to-first-working-system
- breadth of supported workloads
- inference engine maturity
- low organizational risk

It often loses on:

- acquisition cost
- cloud instance price
- availability pressure for top-end systems

If engineer time is expensive and deployment speed matters, NVIDIA often still has the best total economics.

**AMD**

AMD often wins when:

- memory capacity changes the serving or training design
- the workload is already validated on ROCm
- the organization is willing to invest in platform fluency

AMD can lose when:

- the workload depends on immature or missing paths
- the team has no ROCm debugging experience
- portability was assumed but not actually tested

**Google accelerators**

Google publishes TPU pricing per chip-hour and positions v6e as a high-value product for transformer, text-to-image, CNN training, fine-tuning, and serving. In practice, TPU economics are strongest when:

- the workload runs efficiently under XLA
- slices are well utilized
- the team is already on Google Cloud
- training jobs are large and regular enough to justify the platform shape

TPU economics are weaker when:

- jobs are highly irregular
- portability matters
- the team is mostly PyTorch/CUDA-native and pays a large adaptation tax

---

**8. Where each platform is strongest and weakest**

**NVIDIA is strongest when**

- you need the broadest production support
- you rely on custom CUDA kernels or CUDA-adjacent libraries
- you want the best-supported multi-GPU and multi-node playbooks
- you need mature LLM inference tooling today

**NVIDIA is weakest when**

- budget is the dominant constraint
- memory-per-device economics are poor for your model shape
- you are overbuying premium infrastructure for moderate workloads

**AMD is strongest when**

- large HBM capacity materially simplifies model placement
- your workload is already validated on ROCm
- you want an open alternative and are willing to tune
- you care about inference or training value on supported models

**AMD is weakest when**

- you need universal framework parity
- your team has no ROCm operations experience
- your stack depends on CUDA-first custom kernels

**Google accelerators are strongest when**

- you are already JAX/XLA-native
- the workload is regular and large enough to exploit slice-level scaling
- you want Google-managed accelerator infrastructure instead of building clusters
- cost/TCO on large training jobs matters more than portability

**Google accelerators are weakest when**

- you need maximum cloud or bare-metal portability
- your codebase is dynamic-shape heavy
- your organization needs low-level CUDA-style control and debugging habits

---

**9. Practical recommendation matrix**

Choose **NVIDIA first** if:

- your company is PyTorch-heavy
- you need the widest model and tool compatibility
- you expect custom kernels, aggressive inference optimization, or mixed research and production workloads

Choose **AMD second** if:

- you have a concrete memory-heavy workload
- you can benchmark on real models before committing
- your team is comfortable treating ROCm as a platform that needs validation, not blind trust

Choose **TPU first** if:

- your team is already comfortable with JAX/XLA or willing to become so
- training is the center of gravity
- Google Cloud is an acceptable infrastructure anchor
- you want pod-scale efficiency more than maximum portability

Pilot all three if:

- you are building a serious internal platform team
- your annual accelerator spend is high enough that platform arbitrage matters
- your workloads are large enough that 10-20% infrastructure differences pay back quickly

---

</details>

## 10. 团队推荐的评估方法

不要只跑厂商 demo。

跨平台使用同一套三阶段评估：

1. **Bring-up 测试**
   在团队已在使用的框架中跑一个已知可用的模型。
2. **代表性工作负载测试**
   使用真实模型、真实序列长度、真实批大小和真实并行度。
3. **运维测试**
   衡量簇搭建、故障恢复、性能剖析可见性、升级风险和 runbook 清晰度。

至少衡量：

- 首次成功运行所需时间
- tokens/sec 或 samples/sec
- 达到固定质量目标的 time-to-train
- 加速器内存余量
- 扩展效率
- 算子与编译器失败
- 调试一次失败运行的平均时间

这是大多数团队会跳过的一点。也正是真正的平台差异变得可见之处。

---

## 11. 资源

### NVIDIA

- [TensorRT-LLM documentation](https://docs.nvidia.com/tensorrt-llm/)
- [NCCL documentation](https://docs.nvidia.com/deeplearning/nccl/index.html)
- [NVIDIA H200 specifications](https://www.nvidia.com/en-in/data-center/h200/)

### AMD

- [ROCm PyTorch training documentation](https://rocm.docs.amd.com/en/develop/how-to/rocm-for-ai/training/benchmark-docker/pytorch-training.html)
- [ROCm PyTorch inference documentation](https://rocm.docs.amd.com/en/docs-6.4.0/how-to/rocm-for-ai/inference/pytorch-inference-benchmark.html)
- [RCCL documentation](https://rocm.docs.amd.com/projects/rccl/en/docs-6.2.0/)
- [Omniperf documentation](https://rocm.docs.amd.com/projects/omniperf/en/docs-6.2.0/what-is-omniperf.html)
- [AMD Instinct MI300X specifications](https://www.amd.com/en/products/accelerators/instinct/mi300/mi300x.html)

### Google 加速器

- [Cloud TPU v6e architecture](https://cloud.google.com/tpu/docs/v6e)
- [Cloud TPU pricing](https://cloud.google.com/tpu/pricing)
- [Profile your model on Cloud TPU VMs](https://cloud.google.com/tpu/docs/profile-tpu-vm)
- [Manage queued resources](https://cloud.google.com/tpu/docs/queued-resources)
- [Cloud TPU Multislice overview](https://cloud.google.com/tpu/docs/multislice-introduction)
- [PyTorch/XLA profiling](https://docs.pytorch.org/xla/release/r2.9/learn/xla-profiling.html)

### 跨厂商 benchmark 背景

- [MLPerf Training v5.0 results](https://mlcommons.org/2025/06/mlperf-training-v5-0-results/)
- [MLPerf Training v5.1 results](https://mlcommons.org/2025/11/training-v5-1-results/)
- [MLPerf Inference benchmark documentation](https://docs.mlcommons.org/inference/)


<details>
<summary>English original</summary>

**10. Recommended evaluation method for your team**

Do not run only vendor demos.

Use the same three-stage evaluation across platforms:

1. **Bring-up test**
   Run one known-good model in the framework your team already uses.
2. **Representative workload test**
   Use a real model, real sequence lengths, real batch sizes, and real parallelism.
3. **Operations test**
   Measure cluster setup, failure recovery, profiling visibility, upgrade risk, and runbook clarity.

Measure at least:

- time-to-first-successful run
- tokens/sec or samples/sec
- time-to-train for a fixed quality target
- accelerator memory headroom
- scaling efficiency
- operator and compiler failures
- mean time to debug a bad run

This is the point most teams skip. It is also where the real platform differences become visible.

---

**11. Resources**

**NVIDIA**

- [TensorRT-LLM documentation](https://docs.nvidia.com/tensorrt-llm/)
- [NCCL documentation](https://docs.nvidia.com/deeplearning/nccl/index.html)
- [NVIDIA H200 specifications](https://www.nvidia.com/en-in/data-center/h200/)

**AMD**

- [ROCm PyTorch training documentation](https://rocm.docs.amd.com/en/develop/how-to/rocm-for-ai/training/benchmark-docker/pytorch-training.html)
- [ROCm PyTorch inference documentation](https://rocm.docs.amd.com/en/docs-6.4.0/how-to/rocm-for-ai/inference/pytorch-inference-benchmark.html)
- [RCCL documentation](https://rocm.docs.amd.com/projects/rccl/en/docs-6.2.0/)
- [Omniperf documentation](https://rocm.docs.amd.com/projects/omniperf/en/docs-6.2.0/what-is-omniperf.html)
- [AMD Instinct MI300X specifications](https://www.amd.com/en/products/accelerators/instinct/mi300/mi300x.html)

**Google accelerators**

- [Cloud TPU v6e architecture](https://cloud.google.com/tpu/docs/v6e)
- [Cloud TPU pricing](https://cloud.google.com/tpu/pricing)
- [Profile your model on Cloud TPU VMs](https://cloud.google.com/tpu/docs/profile-tpu-vm)
- [Manage queued resources](https://cloud.google.com/tpu/docs/queued-resources)
- [Cloud TPU Multislice overview](https://cloud.google.com/tpu/docs/multislice-introduction)
- [PyTorch/XLA profiling](https://docs.pytorch.org/xla/release/r2.9/learn/xla-profiling.html)

**Cross-vendor benchmark context**

- [MLPerf Training v5.0 results](https://mlcommons.org/2025/06/mlperf-training-v5-0-results/)
- [MLPerf Training v5.1 results](https://mlcommons.org/2025/11/training-v5-1-results/)
- [MLPerf Inference benchmark documentation](https://docs.mlcommons.org/inference/)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Accelerator Platform Evaluation/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Accelerator%20Platform%20Evaluation/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
