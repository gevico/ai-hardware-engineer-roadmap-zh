---
title: 边缘 GPU 上的 VLA（视觉-语言-动作模型）部署 — 栈选型、压缩与 profile 驱动的优化
description: 边缘 GPU 上的 VLA（视觉-语言-动作模型）部署 — 栈选型、压缩与 profile 驱动的优化
---

# 边缘 GPU 上的 VLA（视觉-语言-动作模型）部署 — 栈选型、压缩与 profile 驱动的优化

<div class="course-identity auto-course" style="--course-accent: #be123c; --course-accent-rgb: 190, 18, 60;" markdown="1">
<div class="course-identity__icon">VDOE</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度剖析 · Jetson 专题</p>
<p class="course-identity__title">VLA 部署到边缘 GPU 的专属课程标识——栈选型、压缩与 profile 驱动的优化。</p>
<p class="course-identity__meta">产物：Jetson 集成 demo · 度量：延迟、内存、功耗、日志</p>
</div>
</div>


**父级：** [ML and AI](../Guide.md)

> **视觉-语言-动作模型不是 LLM。**把它们当作 LLM 来对待——paged KV、in-flight batching、FP8 attention 插件——只会浪费硅片和工程时间。本指南是一份面向实践的综述，梳理该领域实际如何把 VLA 部署到边缘 GPU、NPU 和百克级机器人上，并给出带出处的数字与具体取舍。

本指南用到的来源：

- **EdgeVLA / EVLA**（Budzianowski et al., 2025）— [arXiv 2507.14049](https://arxiv.org/abs/2507.14049)
- **AsyncVLA**（Hirose et al., 2025）— [asyncvla.github.io](https://asyncvla.github.io/)
- **LiteVLA-Edge**（Williams et al., 2026）
- **NVlabs/vla-perf** — [github.com/NVlabs/vla-perf](https://github.com/NVlabs/vla-perf)
- **NXP i.MX 95 + SmolVLA**（HuggingFace + NXP, 2026 年 3 月）— [HF blog post](https://huggingface.co/blog/nxp/bringing-robotics-ai-to-embedded-platforms)
- **reflex-vla** — [github.com/rylinjames/reflex-vla](https://github.com/rylinjames/reflex-vla)
- **RoboECC**（多因素边云协同部署，2026）
- **OmniVLA / OmniVLA-edge**（Hirose et al., 2025）

---

## 1. 为什么 VLA 边缘推理自成一门学科

VLA 模型接收像素 + 状态 + 一条语言指令，输出一个动作——连续的关节指令、末端执行器位姿增量、一段未来动作，或夹爪开合信号。其架构通常由以下部分构成：

```
camera frames ─┐
                ├─▶ vision encoder ─┐
state vector  ─┘                    │
                                    ▼
                            language-model backbone ──▶ action head ──▶ action(s)
                                    ▲                     │
                language token  ────┘                     │
                                                          ▼
                                              robot at real-time control rate
```

与在批大小 64 下输出 4 K token 回复的聊天机器人相比，机器人的一次推理调用处在完全不同的工作点：

| 工作负载维度 | LLM 推理服务（聊天机器人） | VLA 推理（机器人） |
|---|---|---|
| 批大小 | 1–256，常为动态 | **1**（每个进程一个机器人） |
| Decode 长度（逐 token 生成阶段） | 256–8192 token | **50 步动作 chunk**，常展开为约 10 个融合 pass |
| 序列模式 | 长自回归 | **异构：**encoder + prefill（首字前的整段计算）+ diffusion / flow-matching |
| KV cache 压力 | 主导 | **可忽略** |
| 优化目标 | 单 GPU 吞吐 | **单请求 p50 / p95 延迟** |
| 硬件 | 数据中心（H100、L40S） | **边缘（Jetson Orin / Thor、NXP i.MX 95）+ 云端** |
| 失效模式 | 聊天变慢 | **机器人撞墙** |

最后一行不是玩笑。聊天机器人上 100 ms 的尾部延迟只是体验上的小瑕疵。30 Hz 视觉运动控制器上 100 ms 的尾部延迟则意味着错过控制截止时间。

本指南其它所有经验背后更深层的原则：

> **几乎没有哪项 LLM 推理服务优化能在 batch=1、固定 decode 的 VLA 工作负载上收回成本。**收益来自别处。

---

## 2. 延迟预算——推理实际需要多快？

两个数字决定了整个设计。

**控制环频率。**不同的机器人任务要求不同的速率：

| 任务类别 | 典型控制频率 | 可容忍的推理延迟 |
|---|---|---|
| 慢速桌面操作 | 5–10 Hz | < 100 ms |
| 标准操作（LIBERO 级） | 15–30 Hz | < 33 ms |
| 反应式抓取、双臂 | 30–50 Hz | < 20 ms |
| 双足 / 腿足全身 | 100–500 Hz | < 5 ms（通常是在 VLA 目标位姿下做经典控制） |
| 无人机 / 车辆 | 50–200 Hz | < 10 ms |

**动作 chunk 长度。**VLA 可以借此摊销成本。如果模型每次推理调用输出 *k* 个未来动作，有效推理速率就变为 `control_rate / k`。SmolVLA、π0、π0.5 输出 50 步的 chunk；OpenVLA 每次调用输出一个动作。这是该领域最大的一根推理速率杠杆，且运行时不付出任何代价——只在训练时改变。

```
single-step VLA at 30 Hz:           inference must finish in 33 ms,    every step
50-action-chunk VLA at 30 Hz:       inference must finish in 1.66 s,   every 50 steps
                                    + adapter / smoother runs at 30 Hz onboard
```

1.66 s 的预算在 Jetson 上极为宽裕。对于任何参数量超过 100 M 的策略，**正是动作 chunking 让边缘 VLA 推理成为可能**。

---


<details>
<summary>English original</summary>

**VLA Deployment on Edge GPUs — Stack Selection, Compression, and Profile-Driven Optimization**

<div class="course-identity auto-course" style="--course-accent: #be123c; --course-accent-rgb: 190, 18, 60;" markdown="1">
<div class="course-identity__icon">VDOE</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Jetson Track</p>
<p class="course-identity__title">Specialized course identity for VLA Deployment on Edge GPUs — Stack Selection, Compression, and Profile-Driven Optimization.</p>
<p class="course-identity__meta">Artifact: Jetson integration demo · Measure: latency, memory, power, logs</p>
</div>
</div>


**Parent:** [ML and AI](../Guide.md)

> **Vision-Language-Action models are not LLMs.** Treating them as if they were — paged KV, in-flight batching, FP8 attention plugins — wastes silicon and engineering time. This guide is a working survey of how the field is actually deploying VLAs to edge GPUs, NPUs, and 100-gram robots, with cited numbers and concrete tradeoffs.

Sources used in this guide:

- **EdgeVLA / EVLA** (Budzianowski et al., 2025) — [arXiv 2507.14049](https://arxiv.org/abs/2507.14049)
- **AsyncVLA** (Hirose et al., 2025) — [asyncvla.github.io](https://asyncvla.github.io/)
- **LiteVLA-Edge** (Williams et al., 2026)
- **NVlabs/vla-perf** — [github.com/NVlabs/vla-perf](https://github.com/NVlabs/vla-perf)
- **NXP i.MX 95 + SmolVLA** (HuggingFace + NXP, Mar 2026) — [HF blog post](https://huggingface.co/blog/nxp/bringing-robotics-ai-to-embedded-platforms)
- **reflex-vla** — [github.com/rylinjames/reflex-vla](https://github.com/rylinjames/reflex-vla)
- **RoboECC** (multi-factor edge-cloud collaborative deployment, 2026)
- **OmniVLA / OmniVLA-edge** (Hirose et al., 2025)

---

**1. Why VLA edge inference is its own discipline**

A VLA model takes pixels + state + a language instruction and emits an action — a continuous joint command, an end-effector pose delta, a chunk of future actions, or a gripper toggle. The architecture typically composes:

```
camera frames ─┐
                ├─▶ vision encoder ─┐
state vector  ─┘                    │
                                    ▼
                            language-model backbone ──▶ action head ──▶ action(s)
                                    ▲                     │
                language token  ────┘                     │
                                                          ▼
                                              robot at real-time control rate
```

Compared to a chatbot serving a 4 K-token reply at batch 64, a robot inference call has a completely different operating point:

| Workload axis | LLM serving (chatbot) | VLA inference (robot) |
|---|---|---|
| Batch size | 1–256, often dynamic | **1** (one robot per process) |
| Decode length | 256–8192 tokens | **50-step action chunk**, often unrolled into ~10 fused passes |
| Sequence pattern | Long autoregressive | **Heterogeneous:** encoder + prefill + diffusion / flow-matching |
| KV cache pressure | Dominant | **Negligible** |
| Optimization target | Throughput per GPU | **p50 / p95 latency per request** |
| Hardware | Datacenter (H100, L40S) | **Edge (Jetson Orin / Thor, NXP i.MX 95) + cloud** |
| Failure mode | Slow chat | **Robot crashes into a wall** |

That last row is not a joke. A 100 ms tail latency on a chatbot is a UX wart. A 100 ms tail latency on a 30 Hz visuomotor controller is a missed control deadline.

The deeper principle behind every other lesson in this guide:

> **Almost no LLM-serving optimization pays for itself on batch=1, fixed-decode VLA workloads.** The wins come from somewhere else.

---

**2. The latency budget — how fast does inference actually have to be?**

Two numbers set the whole design.

**Control-loop frequency.** Different robot tasks demand different rates:

| Task class | Typical control rate | Tolerable inference latency |
|---|---|---|
| Slow tabletop manipulation | 5–10 Hz | < 100 ms |
| Standard manipulation (LIBERO-class) | 15–30 Hz | < 33 ms |
| Reactive grasping, dual-arm | 30–50 Hz | < 20 ms |
| Bipedal / legged whole-body | 100–500 Hz | < 5 ms (often classical control under VLA target poses) |
| Drone / vehicle | 50–200 Hz | < 10 ms |

**Action-chunk length.** A VLA can amortize this. If the model emits *k* future actions per inference call, the effective inference rate becomes `control_rate / k`. SmolVLA, π0, π0.5 emit 50-step chunks; OpenVLA emits one action per call. This is the single biggest inference-rate lever the field has, and it costs nothing at run time — only training-time changes.

```
single-step VLA at 30 Hz:           inference must finish in 33 ms,    every step
50-action-chunk VLA at 30 Hz:       inference must finish in 1.66 s,   every 50 steps
                                    + adapter / smoother runs at 30 Hz onboard
```

A 1.66 s budget is enormous on a Jetson. **Action chunking is what makes edge VLA inference feasible at all** for anything more than a 100 M-parameter policy.

---

</details>

## 3. 该领域实际使用的七种技术

每个已发表的 VLA（视觉-语言-动作模型）边缘部署系统都会采用其中某个子集。顺序很重要：列表中越靠前的通常越先见效。

### 3.1 量化

第一个杠杆，最便宜，并且在边缘上具有最大的单一乘数。

**实践中的位宽：**

- **FP16 / BF16：** 在 TRT EP 上舒适的默认值，对大多数 VLA 主干没有准确率损失。在 Ada / Hopper / Thor 上更倾向于 BF16，因为它具有与 FP16 相同的吞吐，但数值余量大得多——在轨迹起点 dx/dt 最大处，流匹配速度场对 FP16 敏感。
- **INT8：** 对视觉编码器和 LM 主干进行训练后量化；通常将动作头保持在更高精度，因为扩散 / 流匹配去噪循环会逐步累积误差。
- **INT4 / 4-bit（AWQ、GPTQ、NF4）：** 激进但如今已成为 LM 主干的标配。HuggingFace + NXP 团队报告称，将 SmolVLA 的视觉编码器和大语言模型 prefill（首字前的整段计算）从“8-bit 混合精度到 4-bit”，同时将动作专家保持在 FP32，以保留迭代去噪路径。
- **FP8：** Hopper / Blackwell / Thor 一等公民。在 ORT 路径中不太成熟；在 TRT-LLM 中是一等公民，但很少是正确的首选 VLA 栈。

**来自现场的真实数字。** SmolVLA 在 NXP i.MX 95（6× Cortex-A55 + Mali GPU + eIQ Neutron NPU）上：

```
ONNX FP32 baseline             29.1 s  per inference
Selective quantization +        6.15 s per inference         (≈ 4.7× speedup)
decomposition (vision / LLM
to 4-bit, action expert FP32)
```

在同一篇博客中作为对比，一个 ACT 模型（不同的架构，更简单的动作头）从 2.86 s FP32 优化到 0.32 s——延迟降低 88.8%，全局准确率下降约 7%。ACT 的结果显示了代价：在非 VLA 架构上进行激进的全模型量化损失了真实的准确率。SmolVLA 的结果显示了纪律：将迭代去噪器保持在高精度，只量化那些能容忍量化的组件。

**从这些结果得出的经验法则：** 量化编码器和 LM prefill；不要量化迭代扩散 / 流匹配动作头，直到你对照 FP32 参考测量了准确率增量，并在真实任务套件上接受了它。

### 3.2 蒸馏和小语言模型主干

第二个杠杆，并且是重新架构模型的杠杆。公开文献中有两种形式：

**架构收缩——EdgeVLA（EVLA），Budzianowski 等人 2025。** 保留 OpenVLA 的视觉流水线，但做出两处结构改变：

1. **消除自回归末端执行器位置预测**——不再一次生成一个动作 token，而是联合预测 7-DoF 位姿。据报道，仅这一处改变本身就带来 **7× 推理加速**。
2. **用小型语言模型替换 7B 级大语言模型主干**——大幅降低计算和内存。

作者报告了与 OpenVLA 相当的训练特性，同时显著降低了推理成本。教训：自回归动作假设继承自“VLA = 带动作 token 的大语言模型”，移除它不会损失能力，同时大幅缩短推理时间。

**轻量级专用策略——OmniVLA-edge。** Hirose 等人 2025 发布了他们更大的 OmniVLA 系统的 108 M 参数边缘变体，专门用于导航任务，在这些任务中完整 7B 级主干是过度的。要点：根据任务选择模型类别，而不是“可用的最大模型”。

**实践要点。** 如果你在部署来自研究论文的 VLA，而没有考虑 EdgeVLA 风格的架构改变，你可能白白放弃了仅靠量化无法带来的 5–10× 加速。

### 3.3 动作分块

上面已经介绍过；值得单独一节，因为它与其他所有内容相互作用。

**权衡。** 每次调用预测 `k` 个未来动作，将所需的推理速率除以 `k`，但模型现在必须处理更长时域的预测，并且 rollout 在块之间对环境变化变得脆弱。实践中：

- `k = 1`（OpenVLA）：推理必须达到控制速率。在边缘上很难。
- `k = 8–16`（中档）：许多操作任务的标准。
- `k = 50`（SmolVLA、π0、π0.5）：在 30 Hz 下整整一秒的动作。在 Jetson 上可行。
- `k = 100`（HF/NXP SmolVLA 在 i.MX 95 上，块大小阈值 0.2 + 加权平均聚合）。

**复杂之处。** 使用长动作块时，世界在执行期间漂移。两种正确的响应，均用于已发表系统：

1. **机会主义地重新规划**——在块完成之前启动下一次推理调用，以便在需要时准备好新计划。这就是异步推理思想（下一节）。
2. **聚合 / 平滑**——HF/NXP 部署使用 `weighted_average` 聚合器混合重叠块，权重由每个块的上下文有多新决定。机器人从不逐字执行过期的块。


<details>
<summary>English original</summary>

**3. The seven techniques the field is actually using**

Every published VLA edge-deployment system reaches for some subset of these. Order matters: the earlier ones in the list usually pay off first.

**3.1 Quantization**

The first lever, the cheapest, and the one with the biggest single multiplier on edge.

**Bit widths in practice:**

- **FP16 / BF16:** the comfortable default on TRT EP, no accuracy loss for most VLA backbones. BF16 is preferable on Ada / Hopper / Thor where it has the same throughput as FP16 with much better numerical headroom — flow-matching velocity fields are FP16-sensitive at trajectory start where dx/dt is largest.
- **INT8:** post-training quant on the vision encoder and LM backbone; usually leaves the action head at higher precision because diffusion / flow-matching denoising loops accumulate error per step.
- **INT4 / 4-bit (AWQ, GPTQ, NF4):** aggressive but standard now for the LM backbone. The HuggingFace + NXP team report taking SmolVLA's vision encoder and LLM prefill from "8-bit mixed precision to 4-bit" while keeping the action expert at FP32 to preserve the iterative denoising path.
- **FP8:** Hopper / Blackwell / Thor first-class. Less mature in the ORT path; first-class in TRT-LLM but rarely the right primary VLA stack.

**A real number from the field.** SmolVLA on the NXP i.MX 95 (6× Cortex-A55 + Mali GPU + eIQ Neutron NPU):

```
ONNX FP32 baseline             29.1 s  per inference
Selective quantization +        6.15 s per inference         (≈ 4.7× speedup)
decomposition (vision / LLM
to 4-bit, action expert FP32)
```

For comparison on the same blog, an ACT model (different architecture, simpler action head) went from 2.86 s FP32 to 0.32 s optimized — an 88.8 % latency reduction with a ~7 % drop in global accuracy. The ACT result shows the cost: aggressive whole-model quant on a non-VLA architecture lost real accuracy. The SmolVLA result shows the discipline: keep the iterative denoiser at high precision, quantize only the components that tolerate it.

**Rule of thumb derived from these results:** quantize the encoder and the LM prefill; do not quantize the iterative diffusion / flow-matching action head until you have measured the accuracy delta against an FP32 reference and accepted it on a real task suite.

**3.2 Distillation and small-language-model backbones**

The second lever, and the one that re-architects the model. Two flavors in the public literature:

**Architectural shrink — EdgeVLA (EVLA), Budzianowski et al. 2025.** Keeps OpenVLA's vision pipeline but makes two structural changes:

1. **Eliminates autoregressive end-effector position prediction** — instead of generating action tokens one at a time, predicts the 7-DoF pose jointly. This single change is reported to give a **7× inference speedup** by itself.
2. **Replaces the 7B-class LLM backbone with a Small Language Model** — substantially reduces both compute and memory.

The authors report comparable training characteristics to OpenVLA at significantly reduced inference cost. The lesson: the autoregressive-action assumption was inherited from "VLA = LLM with action tokens," and removing it costs nothing in capability while collapsing inference time.

**Lightweight specialized policies — OmniVLA-edge.** Hirose et al. 2025 ship a 108 M-parameter edge variant of their larger OmniVLA system specifically for navigation tasks where the full 7B-class backbone is overkill. The point: pick the model class for the task, not "the biggest available."

**Practical takeaway.** If you are deploying a VLA from a research paper without considering EdgeVLA-style architectural changes, you are probably leaving a 5–10× speedup on the table that quantization alone cannot give you.

**3.3 Action chunking**

Already introduced above; deserves its own section because it interacts with everything else.

**The trade.** Predicting `k` future actions per call divides the required inference rate by `k`, but the model now has to handle the longer-horizon prediction and the rollout becomes brittle to environmental change between chunks. In practice:

- `k = 1` (OpenVLA): inference must hit control rate. Hard on edge.
- `k = 8–16` (mid-range): standard for many manipulation tasks.
- `k = 50` (SmolVLA, π0, π0.5): a full second of actions at 30 Hz. Tractable on Jetson.
- `k = 100` (HF/NXP SmolVLA on i.MX 95 with chunk-size threshold 0.2 + weighted-average aggregation).

**The complication.** With a long action chunk, the world drifts during execution. Two correct responses, both used in published systems:

1. **Re-plan opportunistically** — start the next inference call before the chunk completes, so a fresh plan is ready when needed. This is the asynchronous-inference idea (next section).
2. **Aggregate / smooth** — the HF/NXP deployment mixes overlapping chunks with a `weighted_average` aggregator, weighted by how recent each chunk's context was. The robot never executes a stale chunk verbatim.

</details>

### 3.4 异步推理与边缘适配器模式

第三个主要手段，也是让大型远程 VLA 在真实机器人上可用的手段。

**问题。** 一个 7B 级 VLA 主干网络在远程工作站上可能需要 200 ms–6 s，具体取决于网络与负载。而 30 Hz 的机器人等不起。

**AsyncVLA（Hirose 等，2025）的解决方案。** 将语义推理与反应式执行解耦：

```
robot                                     remote workstation
─────                                     ──────────────────
camera frames ──┬────────────────────────▶ base VLA
                │                          (OmniVLA: SigLIP + DINOv2 + LLaMA2-7B)
                │                                  │
                │                                  ▼
                │                          coarse action tokens
                │                                  │
                │                                  ▼
                │                          token projector
                │                          (8×4×4096  →  1×1024)
                │                                  │
                │                                  ▼  ◄── high-latency network
                │                          ─────────────
                │                          edge adapter
                │                          (2 MLP-ResNet blocks, onboard)
                │                                  │
                ▼                                  ▼
            latest observation     ───▶    action refinement (fast)
                                                   │
                                                   ▼
                                               robot at real-time rate
```

边缘适配器小到足以在机器人的板载控制器上运行，它看到最新观测，并对来自远程 VLA 的陈旧但信息丰富的动作 token 进行精修。AsyncVLA 报告，在 0.2 s、2.0 s 和 5.0 s 的人为延迟下，**成功率比最先进的基线高 40 %**，并可容忍高达 6 s 的真实延迟。

**大规模边缘-云协同推理：RoboECC**（2026）报告，通过将流水线的正确部分路由到正确的层级（延迟敏感的放边缘，计算密集的放云），VLA 任务执行获得了 **3.28× 加速**。同样的总体模式，不同的切分策略。

**结构性洞见：** 如果 VLA 必须足够大才有能力，而机器人必须足够敏捷才安全，那么二者无法共存于同一设备。异步 / 边缘适配器模式就是出路。

### 3.5 边缘-云协同推理

是一个谱系，不是二元的。五个工作点，按延迟关键性递增：

| 模式 | VLA 运行于 | 机器人动作时回路运行于 | 何时使用 |
|---|---|---|---|
| **仅云** | 数据中心 | 数据中心 | 演示、仿真，回路中没有真实机器人 |
| **云 + 边缘适配器** | 数据中心 | 板载（AsyncVLA） | 大型 VLA、网络可接受、故障安全 |
| **边缘 GPU + 板载** | 边缘 GPU 盒子（附近的 Jetson AGX） | 板载 MCU | 无线电范围内有工作站的移动机器人 |
| **仅板载** | 板载 Jetson / NPU | 板载 | 无人机、无系绳机器人、安全关键 |
| **板载微型 + 云端辅助** | 板载微型模型，困难情况用云 | 板载 | 电池受限、连接时断时续 |

正确的模式是三个相互独立因素的函数：模型大小、网络可靠性、故障后果。不存在普适的正确选择。

### 3.6 Runtime 栈选择

本指南早期草稿中的原始框架，经过完善。

**推理路径的三个实际选项。** 它们不可互换。


<details>
<summary>English original</summary>

**3.4 Asynchronous inference and the edge-adapter pattern**

The third major lever, and the one that makes large remote VLAs usable on real robots.

**Problem.** A 7B-class VLA backbone may take 200 ms–6 s on a remote workstation depending on network and load. A robot at 30 Hz cannot wait.

**AsyncVLA (Hirose et al., 2025) solution.** Decouple semantic reasoning from reactive execution:

```
robot                                     remote workstation
─────                                     ──────────────────
camera frames ──┬────────────────────────▶ base VLA
                │                          (OmniVLA: SigLIP + DINOv2 + LLaMA2-7B)
                │                                  │
                │                                  ▼
                │                          coarse action tokens
                │                                  │
                │                                  ▼
                │                          token projector
                │                          (8×4×4096  →  1×1024)
                │                                  │
                │                                  ▼  ◄── high-latency network
                │                          ─────────────
                │                          edge adapter
                │                          (2 MLP-ResNet blocks, onboard)
                │                                  │
                ▼                                  ▼
            latest observation     ───▶    action refinement (fast)
                                                   │
                                                   ▼
                                               robot at real-time rate
```

The edge adapter is small enough to run on the robot's onboard controller, sees the latest observation, and refines whatever stale-but-rich action token came in from the remote VLA. AsyncVLA reports a **40 % higher success rate than state-of-the-art baselines** under artificial delays of 0.2 s, 2.0 s, and 5.0 s, and tolerates real-world delays up to 6 s.

**Edge-cloud collaborative inference at large scale: RoboECC** (2026) reports a **3.28× speedup** on VLA task execution by routing the right pieces of the pipeline to the right tier (edge for latency-sensitive, cloud for compute-heavy). Same general pattern, different splitting policy.

**The structural insight:** if a VLA must be large to be capable, and a robot must be reactive to be safe, you cannot have both on one device. The async / edge-adapter pattern is the way out.

**3.5 Edge-cloud collaborative inference**

A spectrum, not a binary. Five operating points, in increasing latency-criticality:

| Mode | Where the VLA runs | Where the robot's action-time loop runs | When to use |
|---|---|---|---|
| **Cloud-only** | Datacenter | Datacenter | Demos, simulation, no real robot in loop |
| **Cloud + edge adapter** | Datacenter | Onboard (AsyncVLA) | Large VLA, tolerable network, safe failures |
| **Edge GPU + onboard** | Edge GPU box (Jetson AGX nearby) | Onboard MCU | Mobile robots with workstation in radio range |
| **Onboard only** | Onboard Jetson / NPU | Onboard | Drones, tetherless robots, safety-critical |
| **Onboard tiny + cloud assist** | Tiny model onboard, cloud for hard cases | Onboard | Battery-constrained, intermittent connectivity |

The right mode is a function of three independent things: model size, network reliability, and failure consequence. There is no universally correct choice.

**3.6 Runtime stack selection**

The original framing from this guide's earlier draft, refined.

**Three real options for the inference path.** They are not interchangeable.

</details>

#### 方案 A — ONNX Runtime + TensorRT Execution Provider + CUDA Graphs

```
Trained VLA (PyTorch)
   │
   ▼  torch.onnx.export
ONNX graph (vision encoder, LM, action head, sampler loop unrolled)
   │
   ▼  ORT session
ORT session
   ├──▶ TensorRT EP   ── compiles eligible subgraph into a TRT engine
   ├──▶ CUDA EP       ── fallback for unsupported ops
   └──▶ CPU EP        ── last-resort fallback
   │
   ▼  cudaGraphCapture / cudaGraphLaunch
Replayable CUDA graph for batch=1 fixed-shape inference
```


**为什么这是 VLA 的正确主架构：**

- 对异构友好。视觉编码器 + LM + action head 可以作为 ONNX 子图共存。
- CUDA Graphs 消除了 launch 开销 — 对 batch=1 的固定 shape 至关重要。
- 跨架构：在 Ampere 架构 / Ada / Hopper / Jetson Orin / Thor 上走同一条代码路径。
- 严格的精度一致性可行。ONNX 是确定性的；在机器 epsilon 量级做 FP32 参考比对是一道真正的正确性门禁。

经验依据：[reflex-vla](https://github.com/rylinjames/reflex-vla)项目报告称，在 A10G/SmolVLA 上用 TRT EP 时 p50 为 19.49 ms，用 CUDA EP 时 p50 为 108.11 ms — 在该技术栈上实测 **5.55× 加速**，batch=1，算法未做任何改动。这一比值与 TRT-EP 融合在 flow-matching velocity-field unroll 上应给出的结果一致，而在该场景下 CUDA EP 是逐个 kernel 派发的。

#### 方案 B — TensorRT-LLM

围绕 LLM 作为 decoder 的工作负载构建：paged KV cache、in-flight 连续批处理、投机解码、FP8 attention。

**它值回成本的地方：**batch ≥ 8、decode（逐 token 生成阶段）长度 ≥ 512 token、数据中心推理服务。

**为什么它不适合作为 VLA 的主 runtime：**

- 它的收益全都随 batch × decode 长度缩放。VLA 推理是 batch=1、约 10 次 decode pass。
- 它的模型定义面是“Transformer decoder”。把 Eagle 2.5 + Qwen3 + DiT 掰成那个形状，胶水代码的成本超过从 kernel 上收回的收益。
- 两套模型定义面（非 LLM 用 ONNX、LLM 用 TRT-LLM）是永久的维护税。

**在 VLA 部署中 TRT-LLM 的正确用法：**窄范围的架构专用回退路径（例如过渡期 ORT 打包缺口期间走 Blackwell sm_100 路径）。它不应作为主 runtime。

#### 方案 C — 裸 CUDA / 手写 kernel

控制力最大，维护负担也最大。

**部署层规则：**不要为 velocity field、attention、RMSNorm 或旋转位置编码（RoPE）写手写 kernel。TRT 把这些做得很好，kernel 的演进速度快过你的跟进速度，每次改动都会破坏精度一致性。

**在 VLA 部署层里，裸 CUDA 能正当取胜的两个地方：**

1. ONNX 无法表达、且 TRT 无法高效融合的某个特定 op。封装成 TRT plugin，而不是独立的 runtime。
2. 流 / event 编排 — 在多个 CUDA 流上重叠视觉编码器、LM prefill（首字前的整段计算）和 action head。这是不写 kernel 的“裸 CUDA 式思考”。

#### NPU 专用 runtime

在 NVIDIA 技术栈之外，runtime 的形态因厂商而异，变化也更频繁。截至写作时：

- **NXP eIQ + ONNX Runtime / TFLite** — 用于 i.MX 95 SmolVLA 部署。
- **Hailo-8 / Hailo-15** — 专有编译器；适用于基于 ViT 的编码器；超过约 1B 参数的 LM 主干通常放不下。
- **Qualcomm SNPE / QNN** — 在 Snapdragon 上是一等公民；导入 ONNX 是务实做法。
- **Apple ANE / Core ML** — 对小型 VLA 可行；多用于原型验证。

ORT + EP 架构对以上所有情形都通用 — 这里的“execution provider”抽象在真正发挥作用。


<details>
<summary>English original</summary>

**Option A — ONNX Runtime + TensorRT Execution Provider + CUDA Graphs**

```
Trained VLA (PyTorch)
   │
   ▼  torch.onnx.export
ONNX graph (vision encoder, LM, action head, sampler loop unrolled)
   │
   ▼  ORT session
ORT session
   ├──▶ TensorRT EP   ── compiles eligible subgraph into a TRT engine
   ├──▶ CUDA EP       ── fallback for unsupported ops
   └──▶ CPU EP        ── last-resort fallback
   │
   ▼  cudaGraphCapture / cudaGraphLaunch
Replayable CUDA graph for batch=1 fixed-shape inference
```

**Why this is the right primary architecture for VLA:**

- Heterogeneity-friendly. Vision encoder + LM + action head all coexist as ONNX subgraphs.
- CUDA Graphs collapse launch overhead — critical for batch=1 fixed-shape.
- Cross-architecture: same code path on Ampere / Ada / Hopper / Jetson Orin / Thor.
- Strict parity is feasible. ONNX is deterministic; FP32 reference comparison at machine epsilon is a real correctness gate.

Empirical anchor: the [reflex-vla](https://github.com/rylinjames/reflex-vla) project reports 19.49 ms p50 on A10G/SmolVLA with TRT EP vs 108.11 ms p50 with CUDA EP — a measured **5.55× speedup** on this stack, batch=1, no algorithmic change. That ratio is consistent with what TRT-EP fusion should give on a flow-matching velocity-field unroll where CUDA EP dispatches kernel-by-kernel.

**Option B — TensorRT-LLM**

Built around the LLM-as-decoder workload: paged KV cache, in-flight continuous batching, speculative decoding, FP8 attention.

**Where it pays for itself:** batch ≥ 8, decode length ≥ 512 tokens, datacenter serving.

**Why it is wrong as the primary VLA runtime:**

- Its wins all scale with batch × decode length. VLA inference is batch=1, ~10 decode passes.
- The model-definition surface is "transformer decoder." Bending Eagle 2.5 + Qwen3 + DiT into that shape costs more in glue than you recover in kernels.
- Two model-definition surfaces (ONNX for non-LLM, TRT-LLM for LLM) is a permanent maintenance tax.

**When TRT-LLM is correctly used in VLA deployment:** narrow architecture-specific fallback (e.g., a Blackwell sm_100 path during a transitional ORT packaging gap). It should not be the primary runtime.

**Option C — Raw CUDA / hand-written kernels**

Maximum control, maximum maintenance burden.

**Deploy-layer rule:** do not write hand kernels for the velocity field, attention, RMSNorm, or RoPE. TRT does these well, the kernels evolve faster than you can keep up, every touch breaks parity.

**The two places raw CUDA legitimately wins in a VLA deploy layer:**

1. A specific op that ONNX cannot represent and TRT cannot fuse efficiently. Wrap as a TRT plugin, not a separate runtime.
2. Stream / event orchestration — overlapping vision encoder, LM prefill, and action head across multiple CUDA streams. This is "raw CUDA thinking" without writing kernels.

**NPU-specific runtimes**

Outside the NVIDIA stack, the runtime story is per-vendor and changes more often. As of writing:

- **NXP eIQ + ONNX Runtime / TFLite** — used in the i.MX 95 SmolVLA deployment.
- **Hailo-8 / Hailo-15** — proprietary compiler; works for ViT-based encoders; LM backbones over ~1B parameters typically don't fit.
- **Qualcomm SNPE / QNN** — first-class on Snapdragon; ONNX import is pragmatic.
- **Apple ANE / Core ML** — feasible for small VLAs; mostly used for prototyping.

The ORT + EP architecture generalizes across all of these — the "execution provider" abstraction is doing real work here.

</details>

### 3.7 性能分析驱动的优化

第七项也是最后一项技术：**先测量，再改动**。

标准入口是：NVIDIA 目标用 Nsight Systems，NPU 用厂商性能分析器：

```bash
nsys profile \
    --trace=cuda,nvtx,osrt \
    --output=vla-trace.qdrep \
    python -m your_serve_module --model your-model
# fire one inference call, then SIGINT
```

打开 trace，按顺序回答：

| 问题 | 说明什么 | 若为“是”，则这样做 |
|---|---|---|
| 每个 `/act` 是一次 CUDA Graph 启动还是多次？ | 多次 → 外部采样器循环或图捕获已失效 | 把循环固化进 ONNX |
| `Run()` 返回与下一个 CUDA op 之间的间隔？ | 不可忽略 → ORT 会话开销或缓冲区分配 | 使用 IO 绑定 |
| 推理期间是否存在主机-设备传输？ | 是 → 回退 op 位于 TRT 子图之外，或做了 CPU 预处理 | 把 op 移到 GPU 或可入图的子图 |
| 多引擎流水线上的流并发 | 串行 → 把编码器与解码器切到不同流 | 增加 CUDA stream + event 同步 |
| 内存带宽利用率（特指 Orin） | 高 → 属带宽受限，而非算力受限 | 先量化，再融合 |

**Jetson 上的优化顺序**（几乎总是按此顺序）：

1. 降低权重带宽：FP16 → BF16 → INT8 → INT4 量化
2. 消除主机-设备传输：预处理常驻 GPU、IO 绑定
3. 捕获更大的 CUDA Graphs：把采样器循环固化进 ONNX
4. 改进 kernel 融合：为编译器无法融合的热点 op 编写 TRT plugin
5. 多流并发：重叠 encode / decode

在 Jetson 上先做第 4 步再做第 1 步是白费功夫。

---

## 4. 模型库

你会遇到的具体 VLA，以及部署时真正重要的推理特性。

| 模型 | 参数量 | 视觉编码器 | LM 主干 | 动作头 | 动作块 | 备注 |
|---|---|---|---|---|---|---|
| **OpenVLA** | 7.5 B | 双 SigLIP + DINOv2 | Llama-2 7B | 自回归 token | 1 | “VLA = 带动作 token 的 LLM”这一基线。原样很难部署到边缘。 |
| **SmolVLA**（LeRobot） | 450 M | 小型 ViT | 小型 LM | flow-matching，10 步 Euler | 50 | 可装入 Orin Nano 8 GB。小型 VLA 的参考实现。 |
| **π0**（LeRobot） | 3.5 B | SigLIP | PaliGemma | flow-matching，10 步 Euler | 50 | 需要 Orin 16 GB 以上。 |
| **π0.5**（LeRobot） | 3.62 B | SigLIP | PaliGemma | flow-matching，10 步 Euler | 50 | 适合分解导出。 |
| **GR00T N1.6**（NVIDIA） | 3.29 B | SigLIP | Qwen3（Eagle 2.5 VLM） | DiT，4 步 DDPM | 随模型而定 | 部署栈中为双 ONNX 链。 |
| **EdgeVLA / EVLA** | 更小 | OpenVLA 风格 | SLM | 非自回归联合位姿 | 1 | 通过非自回归头 + SLM，推理相比 OpenVLA 提速 7×。 |
| **OmniVLA** | 7B+ | SigLIP + DINOv2 | LLaMA-2 7B | token + 适配器 | 随模型而定 | 在 AsyncVLA 中作为远端 VLA 使用。 |
| **OmniVLA-edge** | 108 M | 更小 | 更小 | 任务专用 | 随模型而定 | 轻量导航策略。 |
| **CogACT** | 随模型而定 | ViT | 随模型而定 | 动作分块 | 随模型而定 | 有 MulticoreWare 的部署文章可供参考。 |

两个值得注意的模式：

- 7B 级单步自回归家族（OpenVLA、OmniVLA）在边缘部署时需要异步 / 边缘适配器。
- 0.5–4 B 级分块 flow-matching / 扩散家族（SmolVLA、π0/.5、GR00T、EVLA）可直接跑在 Jetson 上。

---

## 5. 硬件库

会改变优化顺序的各目标平台约束。

| 目标平台 | 算力 | 内存 | 变化点 |
|---|---|---|---|
| **NXP i.MX 95** | Mali GPU + eIQ Neutron NPU | 与 CPU 共享，随配置而定 | NPU 量化优先；按组件选择性量化；action expert 回退 CPU。采用 4-bit 编码器/LLM + FP32 action expert 时，SmolVLA 每次推理约 6 s 可实现。 |
| **Jetson Orin Nano 8 GB**（sm_8.7） | 约 40 TOPS INT8 | 8 GB 统一内存，约 50 GB/s | 仅限 SmolVLA 级别；必须激进量化；预处理放 GPU 不可妥协。 |
| **Jetson Orin AGX 64 GB**（sm_8.7） | 约 275 TOPS INT8 | 64 GB 统一内存，约 204 GB/s | π0 / π0.5 / GR00T 可装入；默认 FP16；需要数值余量时用 BF16。 |
| **Jetson Thor 128 GB**（sm_10） | 约 2 PFLOPS FP4 | 128 GB 统一内存 | FP8 为一等公民；单设备多 VLA 可行。 |
| **A10G**（sm_8.6） | 约 31 TFLOPS FP32 | 24 GB GDDR6，约 600 GB/s | 云端参考目标；非带宽受限；以 CUDA Graph + TRT 融合为主。 |
| **RTX 4090**（sm_8.9） | 约 83 TFLOPS FP32 | 24 GB GDDR6X，约 1 TB/s | 工作站参考目标；常用于 vla-perf 研究。 |
| **H100**（sm_9.0） | 约 989 TFLOPS BF16 | 80 GB HBM3，约 3 TB/s | FP8 attention 表现突出；适合机群仿真。 |
| **B100 / Blackwell**（sm_10.0） | 约 20 PFLOPS FP4 | 192 GB HBM3e | 由 vla-perf 建模；runtime 栈支持在部分工具链中目前处于过渡阶段。 |

Orin Nano 这一列是驱动“真实”机器人部署中大多数架构决策的设计约束。**如果你的 VLA 装不进 8 GB 统一内存，就无法部署到世界上数量最多的机器人平台上。**

---


<details>
<summary>English original</summary>

**3.7 Profile-driven optimization**

The seventh and last technique: **measure before changing anything**.

The canonical entry point is Nsight Systems for NVIDIA targets and the vendor profiler for NPUs:

```bash
nsys profile \
    --trace=cuda,nvtx,osrt \
    --output=vla-trace.qdrep \
    python -m your_serve_module --model your-model
# fire one inference call, then SIGINT
```

Open the trace and answer, in order:

| Question | What it tells you | If "yes," do this |
|---|---|---|
| Is each `/act` one CUDA Graph launch or many? | Many → external sampler loop or graph capture is broken | Bake the loop into the ONNX |
| Gap between `Run()` returning and next CUDA op? | Non-trivial → ORT session overhead or buffer allocation | Use IO binding |
| Any host-device transfers during inference? | Yes → fallback op outside TRT subgraph or CPU preprocessing | Move op to GPU or graph-able subgraph |
| Stream concurrency on multi-engine pipelines | Serial → switch encoder and decoder to different streams | Add CUDA stream + event sync |
| Memory-bandwidth utilization (Orin specifically) | High → memory-bound, not compute-bound | Quantize first, fuse second |

**Optimization order on Jetson** (almost always in this sequence):

1. Reduce weight bandwidth: FP16 → BF16 → INT8 → INT4 quantization
2. Eliminate host-device transfers: GPU-resident preprocessing, IO binding
3. Capture larger CUDA Graphs: bake sampler loops into the ONNX
4. Improve kernel fusion: TRT plugins for hot ops the compiler cannot fuse
5. Multi-stream concurrency: overlap encode / decode

Doing step 4 before step 1 on a Jetson is wasted work.

---

**4. The model zoo**

Concrete VLAs you will encounter, with the inference characteristics that matter for deployment.

| Model | Params | Vision encoder | LM backbone | Action head | Action chunk | Notes |
|---|---|---|---|---|---|---|
| **OpenVLA** | 7.5 B | dual SigLIP + DINOv2 | Llama-2 7B | autoregressive token | 1 | The "VLA = LLM with action tokens" baseline. Hard to deploy on edge as-is. |
| **SmolVLA** (LeRobot) | 450 M | small ViT | small LM | flow-matching, 10-step Euler | 50 | Fits Orin Nano 8 GB. The reference small VLA. |
| **π0** (LeRobot) | 3.5 B | SigLIP | PaliGemma | flow-matching, 10-step Euler | 50 | Needs Orin 16 GB+. |
| **π0.5** (LeRobot) | 3.62 B | SigLIP | PaliGemma | flow-matching, 10-step Euler | 50 | Decomposed-export-friendly. |
| **GR00T N1.6** (NVIDIA) | 3.29 B | SigLIP | Qwen3 (Eagle 2.5 VLM) | DiT, 4-step DDPM | varies | Two-ONNX chain in deploy stacks. |
| **EdgeVLA / EVLA** | smaller | OpenVLA-style | SLM | non-AR joint pose | 1 | 7× inference speedup vs OpenVLA via non-AR head + SLM. |
| **OmniVLA** | 7B+ | SigLIP + DINOv2 | LLaMA-2 7B | tokens + adapter | varies | Used as the remote VLA in AsyncVLA. |
| **OmniVLA-edge** | 108 M | smaller | smaller | task-specific | varies | Lightweight navigation policy. |
| **CogACT** | varies | ViT | varies | action chunking | varies | MulticoreWare deployment writeups available. |

Two patterns to notice:

- The 7B-class single-step autoregressive (OpenVLA, OmniVLA) family wants async / edge-adapter for edge deployment.
- The 0.5–4 B-class chunked flow-matching / diffusion (SmolVLA, π0/.5, GR00T, EVLA) family fits on Jetson directly.

---

**5. The hardware zoo**

Per-target constraints that change the optimization order.

| Target | Compute | Memory | What changes |
|---|---|---|---|
| **NXP i.MX 95** | Mali GPU + eIQ Neutron NPU | shared with CPU, varies | NPU-quant-first; selective per-component quant; CPU fallback for action expert. SmolVLA achievable at ~6 s per inference with 4-bit encoder/LLM + FP32 action expert. |
| **Jetson Orin Nano 8 GB** (sm_8.7) | ~40 TOPS INT8 | 8 GB unified, ~50 GB/s | SmolVLA-class only; aggressive quant mandatory; preprocessing on GPU non-negotiable. |
| **Jetson Orin AGX 64 GB** (sm_8.7) | ~275 TOPS INT8 | 64 GB unified, ~204 GB/s | π0 / π0.5 / GR00T fit; FP16 default; BF16 if numerical headroom needed. |
| **Jetson Thor 128 GB** (sm_10) | ~2 PFLOPS FP4 | 128 GB unified | FP8 first-class; multi-VLA per device feasible. |
| **A10G** (sm_8.6) | ~31 TFLOPS FP32 | 24 GB GDDR6, ~600 GB/s | Cloud reference target; not memory-bound; CUDA Graph + TRT fusion dominates. |
| **RTX 4090** (sm_8.9) | ~83 TFLOPS FP32 | 24 GB GDDR6X, ~1 TB/s | Workstation reference; commonly used in vla-perf studies. |
| **H100** (sm_9.0) | ~989 TFLOPS BF16 | 80 GB HBM3, ~3 TB/s | FP8 attention shines; useful for fleet emulation. |
| **B100 / Blackwell** (sm_10.0) | ~20 PFLOPS FP4 | 192 GB HBM3e | Modeled by vla-perf; runtime stack support is currently transitional in some toolchains. |

The Orin Nano column is the design constraint that drives most architectural decisions for "real" robot deployment. **If your VLA cannot fit on 8 GB unified memory, it cannot deploy onto the most numerous robot platform in the world.**

---

</details>

## 6. 参考部署栈

三个公开可见的 VLA 部署实践，并排对比。这并非背书 —— 它们是有用的设计参考点。

| 项目 | 主要技术栈 | 目标 | 核心主张 | 优势 | 值得注意的选择 |
|---|---|---|---|---|---|
| **reflex-vla** | ORT + TRT EP + CUDA Graphs | x86 NVIDIA + Jetson Orin / Thor | 在 A10G/SmolVLA 上 CUDA EP → TRT EP 提速 5.55×；cos=+1 / max_abs ≈ 6e-7 的严格精度一致性 | 跨架构、CI 门控的精度一致性、成熟的算子原语（`reflex doctor`） | 放弃分解式 ONNX，改用单体式 + 烘焙采样循环。 |
| **HF + NXP i.MX 95** | ONNX Runtime + eIQ NPU | i.MX 95（无 GPU） | 29.1 s → 6.15 s SmolVLA | 首个严肃的 NPU 端 VLA 成文记录；按组件的量化策略 | action expert 有意保留 FP32；vision + LLM 降到 4-bit。 |
| **AsyncVLA / OmniVLA** | 异步边缘适配器；基础 VLA 跑在工作站上 | 机器人板载 MCU + 远端 GPU | 0.2–6 s 延迟下导航成功率提升 40 % | 带 token 压缩的具体异步架构 | 边缘适配器只有 2 个 MLP-ResNet 模块。压缩：8×4×4096 → 1×1024。 |

这三者并非竞争关系 —— 它们面向不同的工作点（云侧锚定 + 小边缘、完全端侧 GPU、完全端侧 NPU）。严肃的部署实践最终会三者兼取。

---

## 7. 可复现的精度一致性门禁模式

与具体工具无关，严肃的 VLA 部署层应当：

```python
# 1. Run the reference forward pass in PyTorch FP32 with seeded inputs.
ref = run_pytorch_fp32(model, fixture, seed=0)

# 2. Run the deployed engine on the same seeded inputs.
got = run_deployed(engine, fixture)

# 3. Strict comparison against machine epsilon (or reported tolerance).
cos = cosine_similarity(ref.flatten(), got.flatten())
max_abs = (ref - got).abs().max()

assert cos >= 1.0 - 1e-7,  f"cos parity failed: {cos}"
assert max_abs < 1e-4,      f"max_abs parity failed: {max_abs}"
```

这个模式的重点不在具体容差。重点在于，**你可以重构引擎、更换 EP、更换导出流水线，而门禁要么守住、要么立刻崩掉**。一旦接受「1e-2 已经够接近了」，你就失去了安全改动任何东西的能力。

对于**量化**路径，严格相等在构造上就无法成立。替代门禁是在留出的 fixture 套件上的**任务成功率精度一致性**，并带有有界的接受偏差：

```python
ref_tasks  = simulate(reference_model, task_suite, n=100)
quant_tasks = simulate(quantized_model, task_suite, n=100)

assert quant_tasks.success_rate >= ref_tasks.success_rate * 0.95
```

HF + NXP 团队的 ACT 结果（FP32 全局 0.96 → 优化后 0.89，偏差 –7 %）大致是通常可容忍的边界。任务成功率下降超过约 10 %，就说明量化过度了。

---

## 8. 用 vla-perf 做 benchmark

[NVlabs/vla-perf](https://github.com/NVlabs/vla-perf) 是该领域最接近标准化分析型 benchmark 的东西。它构建在 GenZ LLM Analyzer 之上，在给定以下条件时估算 VLA 推理延迟：

- 架构（Pi0、OpenVLA，以及面向新 VLA 的扩展钩子），
- 目标硬件（A100、H100、B100、RTX 4090，以及整个 Jetson 家族 —— Thor、AGX Orin、Orin NX、Orin Nano、AGX Xavier、Xavier NX），
- 精度（FP32 / FP16 / BF16 / FP8 / INT8 / INT4），
- 并行策略（张量并行 / 流水线并行）。

它是**分析型**的，而非实测的。输出是每个流水线阶段的 roofline 式估计（vision encoder prefill、VLM backbone、action expert / DiT decode），并附带 CSV、图表和 LaTeX 表格。

**如何用好它：**

1. 用于规模评估 —— 「π0 在 Orin AGX 64 上以 FP16 能达到可接受的延迟吗？」—— 在导出 ONNX 之前。
2. 用于比较硬件档位 —— Thor vs Orin AGX vs A10G —— 在采购 / 选型之前。
3. 不要用它替代实测的 Nsight trace。分析模型会漏掉 launch 开销、EP 回退、host-device 传输以及 CUDA Graph 捕获带来的收益。

**工作流：**

```
research / spec phase    →   vla-perf estimates       (will it fit at all?)
implementation phase     →   nsys measured traces     (what is the actual bottleneck?)
optimization phase       →   parity gate + nsys diff  (did the change work?)
release phase            →   bench harness in CI      (does it still work?)
```

---


<details>
<summary>English original</summary>

**6. Reference deploy stacks**

Three publicly visible VLA deployment efforts, side by side. These are not endorsements — they are useful design reference points.

| Project | Primary stack | Targets | Key claim | Strength | Notable choice |
|---|---|---|---|---|---|
| **reflex-vla** | ORT + TRT EP + CUDA Graphs | x86 NVIDIA + Jetson Orin / Thor | 5.55× CUDA EP → TRT EP on A10G/SmolVLA; cos=+1 / max_abs ≈ 6e-7 strict parity | Cross-arch, CI-gated parity, mature ops primitives (`reflex doctor`) | Abandoned decomposed ONNX in favor of monolithic + baked sampler loop. |
| **HF + NXP i.MX 95** | ONNX Runtime + eIQ NPU | i.MX 95 (no GPU) | 29.1 s → 6.15 s SmolVLA | First serious VLA-on-NPU writeup; per-component quant policy | Action expert kept FP32 deliberately; vision + LLM dropped to 4-bit. |
| **AsyncVLA / OmniVLA** | Async edge adapter; base VLA on workstation | Robot onboard MCU + remote GPU | 40 % nav success-rate improvement under 0.2–6 s delays | Concrete asynchronous architecture with token compression | Edge adapter is just 2 MLP-ResNet blocks. Compression: 8×4×4096 → 1×1024. |

The three are not in competition — they target different operating points (cloud-anchor + small edge, fully on-device GPU, fully on-device NPU). A serious deployment effort eventually borrows from all three.

---

**7. The reproducible parity-gate pattern**

Independent of any specific tool, a serious VLA deploy layer should:

```python
# 1. Run the reference forward pass in PyTorch FP32 with seeded inputs.
ref = run_pytorch_fp32(model, fixture, seed=0)

# 2. Run the deployed engine on the same seeded inputs.
got = run_deployed(engine, fixture)

# 3. Strict comparison against machine epsilon (or reported tolerance).
cos = cosine_similarity(ref.flatten(), got.flatten())
max_abs = (ref - got).abs().max()

assert cos >= 1.0 - 1e-7,  f"cos parity failed: {cos}"
assert max_abs < 1e-4,      f"max_abs parity failed: {max_abs}"
```

The point of this pattern is not the exact tolerances. The point is that **you can refactor the engine, change EPs, change the export pipeline, and the gate either holds or breaks immediately**. Once you accept "1e-2 is close enough," you have lost the ability to change anything safely.

For **quantized** paths, strict equality breaks by construction. The replacement gate is **task-success-rate parity** on a held-out fixture suite, with a bounded acceptance delta:

```python
ref_tasks  = simulate(reference_model, task_suite, n=100)
quant_tasks = simulate(quantized_model, task_suite, n=100)

assert quant_tasks.success_rate >= ref_tasks.success_rate * 0.95
```

The HF + NXP team's ACT result (FP32 0.96 global → optimized 0.89, a –7 % delta) is roughly the boundary of what is generally tolerable. Beyond ~10 % task-success drop, you have over-quantized.

---

**8. Benchmarking with vla-perf**

[NVlabs/vla-perf](https://github.com/NVlabs/vla-perf) is the closest thing the field has to a standardized analytical benchmark. Built on top of the GenZ LLM Analyzer, it estimates VLA inference latency given:

- architecture (Pi0, OpenVLA, plus extension hooks for new VLAs),
- target hardware (A100, H100, B100, RTX 4090, the full Jetson family — Thor, AGX Orin, Orin NX, Orin Nano, AGX Xavier, Xavier NX),
- precision (FP32 / FP16 / BF16 / FP8 / INT8 / INT4),
- parallelism strategy (tensor / pipeline parallel).

It is **analytical**, not measured. The output is a roofline-style estimate per pipeline stage (vision encoder prefill, VLM backbone, action expert / DiT decode), with CSVs, plots, and LaTeX tables.

**How to use it well:**

1. Use it for sizing — "will π0 fit at acceptable latency on Orin AGX 64 with FP16?" — before exporting an ONNX.
2. Use it to compare hardware tiers — Thor vs Orin AGX vs A10G — before buying / spec'ing.
3. Do not use it as a substitute for measured Nsight traces. Analytical models miss launch overhead, EP fallbacks, host-device transfers, and CUDA Graph capture wins.

**Workflow:**

```
research / spec phase    →   vla-perf estimates       (will it fit at all?)
implementation phase     →   nsys measured traces     (what is the actual bottleneck?)
optimization phase       →   parity gate + nsys diff  (did the change work?)
release phase            →   bench harness in CI      (does it still work?)
```

---

</details>

## 9. 应避免的反模式

**1. 在没有精度一致性门禁的情况下先做量化。** FP16 / BF16 / INT8 / FP8 从构造上就已打破严格相等。如果没有先建立 FP32 门禁，就无法判断量化后的回归是可接受的精度损失，还是 bug。

**2. 把 TRT-LLM 当作主要 VLA runtime 来跑。** 工作负载维度选错了。只应把它当作针对某一特定架构的窄范围回退方案。

**3. 对迭代式 diffusion / flow-matching 动作头做激进量化。** 单步误差会累积。HF + NXP 的做法（encoder + LLM 4-bit，action expert FP32）才是正确的形态；反过来做，就会看到 rollout 发散。

**4. 在部署层手写 kernel。** TRT 的演进速度超出你的跟进能力。如果性能剖析确实要求，就把某一个具体瓶颈封装成 TRT plugin；不要维护一套平行的 kernel 库。

**5. 在 Jetson 上做 CPU 侧预处理。** 即便在统一内存设备上，约束依然是带宽。把 resize / normalize / CHW-swap 挪到 GPU 上。

**6. 对动态 shape 过于乐观。** CUDA Graphs 要求固定 shape。要么每次部署都固定图像分辨率和 action-chunk 长度，要么承担 graph 重建的开销。二者不可兼得。

**7. “差不多就行”的精度一致性容差。** 一旦 `1e-2` 被视为可接受，每一次重构都会渗入数值漂移。把门禁钉死在 FP32 机器 epsilon 上，让低精度显式地把门禁打破。

**8. 在 7B 级别的远程 VLA 上做同步推理。** 如果模型跑在工作站上、网络往返 > 100 ms，那么推理期间机器人处于开环状态。要么采用 AsyncVLA 边缘适配器模式，要么把模型搬到机载（连同随之而来的全部量化 / 蒸馏工作）。

**9. 把动作头和 LM 一视同仁。** 二者的精度敏感度不同。应施加不同的优化策略。

**10. 量化之后没有任务套件门禁。** 严格相等无法在精度变化后存活。必须用留出的任务成功率门禁来替代它。

---

## 10. 动手构建 —— 证明正确性的产物

如果你在构建自己的 VLA 部署层，以下产物可以证明你做对了：

- **`bench`：** 每个目标平台（cloud GPU、Jetson AGX、Jetson Nano，如相关还有 NPU）上 batch=1 的 p50 / p95 延迟，含与不含各 EP/runtime 变体两种情况。flow-matching VLA 在 A10G 上 TRT-EP 与 CUDA-EP 的比值应在 3–6× 区间；低于 2× 说明 EP 接线有问题。
- **`validate`：** FP32 导出的严格相等精度一致性报告，外加每个低精度变体的任务成功率报告。每次 push 都由 CI 门禁把关。
- **`doctor`：** 运行健康检查，验证 EP 已加载、库可达、fixture 在容差内通过精度一致性。
- **`nsys` trace**：每个目标平台一次推理调用的 trace，并标注出每个 `/act` 对应一次 CUDA Graph 启动（如果数量很多，则附一张说明原因的未关闭工单）。
- **内存预算报告**：针对最小的目标平台（通常是 Orin Nano 8 GB 或 i.MX 95），逐 token 的内存核算，证明模型放得下，且为动作历史缓冲区和 KV cache 留有余量。
- **量化 recipe**：记录哪些组件量化到什么精度，并给出相对 FP32 的任务套件差值。

---

## 11. 好的结果长什么样

| 领域 | 弱结果 | 强结果 |
|---|---|---|
| 技术栈选型 | “LM 用 vLLM，剩下的走一步看一步” | 以 ORT + TRT-EP + CUDA Graphs 为主；TRT plugin 升级路径有文档；NPU runtime 可插拔 |
| 精度一致性 | “输出看起来没问题” | 在 CI 中对照 FP32 参考的 `cos = 1 - 1e-7`、`max_abs < 1e-4`，再加上量化的任务成功率门禁 |
| 硬件支持 | “在我的 A100 上能跑” | A10G + Orin AGX + Orin Nano + i.MX 95 的 benchmark，并给出各目标平台专属的优化顺序 |
| 优化顺序 | “多融合一些 kernel” | “先量化；在 Orin 上我们是带宽受限” |
| 精度策略 | “到处都是 FP16” | “Encoder + LLM 4-bit；action expert FP32；有文档且已验证” |
| 异步 | “机器人等推理” | “边缘适配器 30 Hz；远程 VLA 5 Hz；AsyncVLA 式精修” |
| 维护 | “需要时更新 kernel” | “我们不维护 kernel 库；一切都是上游 TRT 或一个有文档的 plugin” |

---


<details>
<summary>English original</summary>

**9. Anti-patterns to avoid**

**1. Quantizing before having a parity gate.** FP16 / BF16 / INT8 / FP8 each break strict equality by construction. Without an FP32 gate first, you cannot tell whether a quantized regression is acceptable precision loss or a bug.

**2. Running TRT-LLM as the primary VLA runtime.** Wrong workload axis. Use it as a narrow fallback for a specific architecture only.

**3. Quantizing the iterative diffusion / flow-matching action head aggressively.** Per-step error compounds. The HF + NXP discipline (encoder + LLM 4-bit, action expert FP32) is the right shape; reverse it and watch the rollouts diverge.

**4. Hand-rolled kernels in the deploy layer.** TRT evolves faster than you can keep up. Wrap one specific bottleneck as a TRT plugin if profiling demands it; do not maintain a parallel kernel library.

**5. CPU-side preprocessing on Jetson.** Even on a unified-memory device, bandwidth is the constraint. Move resize / normalize / CHW-swap onto the GPU.

**6. Optimistic dynamic shapes.** CUDA Graphs require fixed shapes. Either commit to a fixed image resolution and action-chunk length per deployment, or pay the cost of graph rebuilds. You cannot have both.

**7. "Close enough" parity tolerances.** Once `1e-2` is acceptable, every refactor leaks numerical drift. Pin to FP32 machine epsilon and let lower precision break the gate explicitly.

**8. Synchronous inference on a 7B-class remote VLA.** If the model lives on a workstation and the network round-trip is > 100 ms, the robot is open-loop during inference. Adopt the AsyncVLA edge-adapter pattern or move the model onboard (with all the associated quantization / distillation work).

**9. Treating the action head and the LM the same way.** They have different precision sensitivities. Apply different optimization strategies.

**10. No task-suite gate after quantization.** Strict equality cannot survive precision change. A held-out task-success-rate gate must replace it.

---

**10. Build it — artifacts that prove correctness**

If you are working on a VLA deploy layer of your own, the artifacts that prove you have done it correctly:

- **`bench`:** p50 / p95 latency on each target (cloud GPU, Jetson AGX, Jetson Nano, NPU if relevant), batch=1, with and without each EP/runtime variant. The TRT-EP-vs-CUDA-EP ratio for a flow-matching VLA on A10G should be in the 3–6× range; below 2× indicates an EP wiring problem.
- **`validate`:** strict-equality parity report for FP32 export, plus task-success-rate report for each lower-precision variant. CI-gated on every push.
- **`doctor`:** operational health check verifying EP loaded, libraries reachable, fixture passes parity within tolerance.
- **`nsys` trace** of one inference call per target, annotated to show one CUDA Graph launch per `/act` (or, if there are many, an open ticket explaining why).
- **Memory-budget report** for the smallest target (typically Orin Nano 8 GB or i.MX 95), with token-by-token memory accounting demonstrating the model fits with headroom for the action history buffer and KV cache.
- **Quantization recipe** documenting which components are quantized to what precision, with the task-suite delta vs FP32.

---

**11. What good outcomes look like**

| Area | Weak outcome | Strong outcome |
|---|---|---|
| Stack selection | "We use vLLM for the LM and figure out the rest" | ORT + TRT-EP + CUDA Graphs primary; TRT plugin escalation path documented; NPU runtime pluggable |
| Parity | "Outputs look right" | `cos = 1 - 1e-7`, `max_abs < 1e-4` against FP32 reference, in CI, plus task-success-rate gate for quant |
| Hardware support | "Works on my A100" | A10G + Orin AGX + Orin Nano + i.MX 95 benchmarks, with target-specific optimization order |
| Optimization order | "Fuse more kernels" | "Quantize first; we are memory-bound on Orin" |
| Precision policy | "FP16 everywhere" | "Encoder + LLM 4-bit; action expert FP32; documented and validated" |
| Async | "Robot waits for inference" | "Edge adapter at 30 Hz; remote VLA at 5 Hz; AsyncVLA-style refinement" |
| Maintenance | "Update kernels when needed" | "We do not maintain a kernel library; everything is upstream TRT or one documented plugin" |

---

</details>

## 12. 值得关注的开放研究方向

截至 2026 年中，这些问题尚无定论；预计未来 12 个月内答案会有所变化。

1. **Hopper / Thor 上面向 VLA 的原生 FP8 attention。** TRT-LLM 已具备；ORT 路线正在追赶。第一个把 FP8 作为一等公民并带精度一致性门禁的 VLA 部署层，将树立新的标杆。
2. **NPU 原生的 VLA 编译器。** NXP、Hailo、Qualcomm 和 Apple 都在此角逐。"ONNX + EP" 之下的 runtime 格局正在快速碎片化，且很可能围绕少数几个编译器重新收敛。
3. **投机动作解码。** 把 LLM 推理服务中的投机解码借用到 VLA 的自回归子集（OpenVLA 风格）。截至撰写时，公开工作有限。
4. **边缘-云端协同训练**（不只是推理）。RoboECC 式的切分用于在线微调，将解锁集群规模的机器人学习，而无需逐台机器人上传完整示范数据。
5. **动作块长度作为任务置信度的函数。** 当前系统静态地选取 `k`。以置信度为条件的 `k` 对于难度多变的任务会是干净的推理期收益。
6. **超越 LIBERO 的标准化 benchmark。** vla-perf 是迈向分析式标准化的一步；跨硬件档位的实测 benchmark 仍然缺失。

---

## 参考文献

### 主要论文与项目

- **EdgeVLA / EVLA** — Budzianowski et al., 2025. *EdgeVLA: Efficient Vision-Language-Action Models.* arXiv: [2507.14049](https://arxiv.org/abs/2507.14049).
- **AsyncVLA** — Hirose et al., 2025. *An Asynchronous VLA for Fast and Robust Navigation on the Edge.* [asyncvla.github.io](https://asyncvla.github.io/).
- **LiteVLA-Edge** — Williams et al., 2026. *LiteVLA-Edge: Quantized On-Device Multimodal Control for Jetson Orin-class Hardware.*
- **OmniVLA / OmniVLA-edge** — Hirose et al., 2025.
- **RoboECC** (2026) — *Multi-Factor-Aware Edge-Cloud Collaborative Deployment for VLA Models.*
- **NVlabs/vla-perf** — 分析式性能建模工具：[github.com/NVlabs/vla-perf](https://github.com/NVlabs/vla-perf).
- **CogACT** — 在 MulticoreWare 的部署文章中被引用。

### 参考部署

- **reflex-vla** — 开源 VLA 部署层：[github.com/rylinjames/reflex-vla](https://github.com/rylinjames/reflex-vla).
- **HuggingFace + NXP — Bringing Robotics AI to Embedded Platforms**（2026 年 3 月）：[HF blog](https://huggingface.co/blog/nxp/bringing-robotics-ai-to-embedded-platforms).
- **MulticoreWare — Deploying VLA AI Models on Edge**（2025 年 7 月）.
- **deepsense.ai — Embodied AI on a 100 g Device**（2025 年 8 月）.

### 模型实现

- **LeRobot (SmolVLA, π0, π0.5)**：[github.com/huggingface/lerobot](https://github.com/huggingface/lerobot).
- **NVIDIA Isaac GR00T N1 / N1.6**：[developer.nvidia.com/isaac/gr00t](https://developer.nvidia.com/isaac/gr00t).
- **OpenVLA**：[openvla.github.io](https://openvla.github.io/).

### Runtime / 工具链

- **ONNX Runtime — TensorRT Execution Provider**：[onnxruntime.ai/docs](https://onnxruntime.ai/docs/execution-providers/TensorRT-ExecutionProvider.html).
- **TensorRT-LLM**（作为背景，不作为 VLA 主力）：[github.com/NVIDIA/TensorRT-LLM](https://github.com/NVIDIA/TensorRT-LLM).
- **CUDA Graphs API**：[docs.nvidia.com/cuda](https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#cuda-graphs).
- **Nsight Systems**：[developer.nvidia.com/nsight-systems](https://developer.nvidia.com/nsight-systems).
- **NXP eIQ / i.MX 95**：NXP 开发者文档.

### 同级路线图模块

- [ML and AI on Jetson — 概览](../Guide.md)
- [Jetson LLM Runtime](../01-Jetson-LLM运行时/Guide.md)
- [Jetson 上的 LLM 优化](../02-Jetson-LLM优化/Guide.md)
- [Robotics — 高级感知与 AI](../../../../阶段5-高级专题与专精/04-方向D-机器人/01-机器人高级感知与AI/Lecture-01.md)


<details>
<summary>English original</summary>

**12. Open research directions worth tracking**

These are unsettled as of mid-2026; expect the answers to move in the next 12 months.

1. **Native FP8 attention for VLAs on Hopper / Thor.** TRT-LLM has it; the ORT path is catching up. The first VLA deploy layer with first-class FP8 + parity gate will set a new bar.
2. **NPU-native VLA compilers.** NXP, Hailo, Qualcomm, and Apple are all racing here. The runtime story below "ONNX + EP" is rapidly fragmenting and will probably re-converge around a small number of compilers.
3. **Speculative action decoding.** Borrowing speculative decoding from LLM serving for the autoregressive subset of VLAs (OpenVLA-style). Limited public work as of writing.
4. **Edge-cloud collaborative training** (not just inference). RoboECC-style splits for online fine-tuning would unlock fleet-scale robot learning without per-robot upload of full demonstrations.
5. **Action chunk length as a function of task confidence.** Current systems pick `k` statically. Confidence-conditioned `k` would be a clean inference-time win for variable-difficulty tasks.
6. **Standardized benchmarks beyond LIBERO.** vla-perf is a step toward analytical standardization; measured benchmarks across hardware tiers are still missing.

---

**References**

**Primary papers and projects**

- **EdgeVLA / EVLA** — Budzianowski et al., 2025. *EdgeVLA: Efficient Vision-Language-Action Models.* arXiv: [2507.14049](https://arxiv.org/abs/2507.14049).
- **AsyncVLA** — Hirose et al., 2025. *An Asynchronous VLA for Fast and Robust Navigation on the Edge.* [asyncvla.github.io](https://asyncvla.github.io/).
- **LiteVLA-Edge** — Williams et al., 2026. *LiteVLA-Edge: Quantized On-Device Multimodal Control for Jetson Orin-class Hardware.*
- **OmniVLA / OmniVLA-edge** — Hirose et al., 2025.
- **RoboECC** (2026) — *Multi-Factor-Aware Edge-Cloud Collaborative Deployment for VLA Models.*
- **NVlabs/vla-perf** — analytical performance modeling tool: [github.com/NVlabs/vla-perf](https://github.com/NVlabs/vla-perf).
- **CogACT** — referenced in MulticoreWare deployment writeup.

**Reference deployments**

- **reflex-vla** — open-source VLA deploy layer: [github.com/rylinjames/reflex-vla](https://github.com/rylinjames/reflex-vla).
- **HuggingFace + NXP — Bringing Robotics AI to Embedded Platforms** (Mar 2026): [HF blog](https://huggingface.co/blog/nxp/bringing-robotics-ai-to-embedded-platforms).
- **MulticoreWare — Deploying VLA AI Models on Edge** (Jul 2025).
- **deepsense.ai — Embodied AI on a 100 g Device** (Aug 2025).

**Model implementations**

- **LeRobot (SmolVLA, π0, π0.5)**: [github.com/huggingface/lerobot](https://github.com/huggingface/lerobot).
- **NVIDIA Isaac GR00T N1 / N1.6**: [developer.nvidia.com/isaac/gr00t](https://developer.nvidia.com/isaac/gr00t).
- **OpenVLA**: [openvla.github.io](https://openvla.github.io/).

**Runtime / tooling**

- **ONNX Runtime — TensorRT Execution Provider**: [onnxruntime.ai/docs](https://onnxruntime.ai/docs/execution-providers/TensorRT-ExecutionProvider.html).
- **TensorRT-LLM** (for context, not as VLA primary): [github.com/NVIDIA/TensorRT-LLM](https://github.com/NVIDIA/TensorRT-LLM).
- **CUDA Graphs API**: [docs.nvidia.com/cuda](https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#cuda-graphs).
- **Nsight Systems**: [developer.nvidia.com/nsight-systems](https://developer.nvidia.com/nsight-systems).
- **NXP eIQ / i.MX 95**: NXP developer documentation.

**Sibling roadmap modules**

- [ML and AI on Jetson — overview](../Guide.md)
- [Jetson LLM Runtime](../01-Jetson-LLM运行时/Guide.md)
- [LLM Optimization on Jetson](../02-Jetson-LLM优化/Guide.md)
- [Robotics — Advanced Perception and AI](../../../../阶段5-高级专题与专精/04-方向D-机器人/01-机器人高级感知与AI/Lecture-01.md)

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/5. Application Development/5. ML and AI/vla-deploy-jetson/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/5.%20Application%20Development/5.%20ML%20and%20AI/vla-deploy-jetson/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
