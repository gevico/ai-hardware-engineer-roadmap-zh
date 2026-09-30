---
title: 第 20 讲 - Nemotron 3 Nano Omni：面向 agent 系统的多模态感知子智能体
description: 第 20 讲 - Nemotron 3 Nano Omni：面向 agent 系统的多模态感知子智能体
---

# 第 20 讲 - Nemotron 3 Nano Omni：面向 agent 系统的多模态感知子智能体

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [Lecture 19](Lecture-19.md) | **下一讲：** [Lecture 21](Lecture-21.md)

---

许多 agent 系统仍然长这样：

```text
screen model
  -> OCR/document model
  -> audio transcription model
  -> video understanding model
  -> text reasoning model
  -> planner/executor
```

这样能跑，但会造成**碎片化的感知栈**：

- 更多推理跳数
- 更多编排代码
- 更多上下文交接
- 更多故障点
- 更弱的跨模态一致性
- 持续工作负载下更高的成本

**NVIDIA Nemotron 3 Nano Omni** 值得关注，因为它主张一种不同的角色：

```text
one efficient multimodal model
  -> perception and context sub-agent
  -> planner/executor gets cleaner structured context
```

长期有效的结论不是“永远用这个确切的模型”。

长期有效的结论是：

```text
multimodal perception should be an explicit sub-agent role,
not an accidental chain of unrelated vision, audio, OCR, and text calls.
```

---

## 学习目标

本讲结束时，你应当能够：

1. 解释碎片化的多模态链为何会带来编排和成本问题。
2. 把 Nemotron 3 Nano Omni 描述为多模态感知/上下文子智能体。
3. 在系统层面理解 30B-A3B 混合 MoE（混合专家模型）设计。
4. 解释 Mamba 层、Transformer 层、EVS、3D 卷积和模态编码器为何重要。
5. 把多模态模型特性映射到 agent 工作负载：视频、音频、截图、文档和 computer-use 上下文。
6. 解释为何在固定交互性阈值下的吞吐，比单纯的原始并发数是更好的推理服务指标。
7. 设计一种 OpenClaw 风格的架构，使用多模态感知子智能体，但不让它充当规划器或执行器。
8. 明确为生产环境选择多模态模型之前要 benchmark 哪些内容。

---

## 1. 为什么多模态链很难

智能体化的系统越来越需要**跨模态推理**：

- 截图
- 文档
- 表单
- 图表
- 视频
- 音频
- 语音
- 屏上文本
- 用户消息
- 工具结果

朴素的方案会使用彼此独立的模型：

```text
ASR model for audio
OCR model for text in images
VLM for screenshots
video captioner for video
LLM for reasoning
planner for actions
```

这会带来三个问题。

### 成本

每一跳都要付出**推理时间和编排开销**。

### 上下文漂移

每个模型压缩其输入的方式不同。重要的**跨模态细节**可能丢失。

### 工程复杂度

harness（agent 运行时框架）必须把时间戳、帧、转写文本、OCR 框、图像区域和文本摘要拼接起来。

**统一的多模态模型**试图减少这种碎片化。

---

## 2. Nemotron 3 Nano Omni 是什么

NVIDIA 将 Nemotron 3 Nano Omni 描述为一个用于统一视频、音频、图像和文本推理的**开放模型**。

重要的系统定位是：

```text
Nemotron 3 Nano Omni = multimodal perception and context sub-agent
```

它被设计为位于更大的 agent 系统内部：

```text
multimodal inputs
  -> Nemotron 3 Nano Omni
  -> perception/context output
  -> planner/reasoning model
  -> tool/action layer
```

这一点很重要，因为多模态模型**不应自动拥有 agent 的全部权限**。

它通常应当回答：

```text
What is in this video/audio/document/screenshot?
What evidence supports that?
What context should the planner receive?
```

执行器仍应由**结构化工具、策略和审批门控**控制。

---

## 3. 架构主张

NVIDIA 将 Nemotron 3 Nano Omni 描述为：

```text
30B-A3B hybrid mixture-of-experts model
```

解读：

- 参数总容量约 30B
- 每个 token/任务的激活参数约 3B
- 专家根据任务和模态被激活

为何重要：

```text
large total capability
  + smaller active compute path
  -> higher throughput potential
```

**MoE 是一种推理服务取舍**。

它可以减少激活计算量，但会引入**布线、专家布局、内存和 kernel 复杂度**。

对硬件工程师而言，这不只是模型设计细节。

它影响：

- GPU 内存布局
- 专家并行
- 批处理行为
- 量化策略
- 推理服务引擎支持
- 互连压力

---


<details>
<summary>English original</summary>

**Lecture 20 - Nemotron 3 Nano Omni: Multimodal Perception Sub-Agents for Agent Systems**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 19](Lecture-19.md) | **Next:** [Lecture 21](Lecture-21.md)

---

Many agent systems still look like this:

```text
screen model
  -> OCR/document model
  -> audio transcription model
  -> video understanding model
  -> text reasoning model
  -> planner/executor
```

That works, but it creates a **fragmented perception stack**:

- more inference hops
- more orchestration code
- more context handoffs
- more failure points
- weaker cross-modal consistency
- higher cost under sustained workloads

**NVIDIA Nemotron 3 Nano Omni** is interesting because it argues for a different role:

```text
one efficient multimodal model
  -> perception and context sub-agent
  -> planner/executor gets cleaner structured context
```

The durable lesson is not "always use this exact model."

The durable lesson is:

```text
multimodal perception should be an explicit sub-agent role,
not an accidental chain of unrelated vision, audio, OCR, and text calls.
```

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Explain why fragmented multimodal chains create orchestration and cost problems.
2. Describe Nemotron 3 Nano Omni as a multimodal perception/context sub-agent.
3. Understand the 30B-A3B hybrid MoE design at a system level.
4. Explain why Mamba layers, transformer layers, EVS, 3D convolutions, and modality encoders matter.
5. Map multimodal model features to agent workloads: video, audio, screenshots, documents, and computer-use context.
6. Explain why throughput at a fixed interactivity threshold is a better serving metric than raw concurrency alone.
7. Design an OpenClaw-style architecture that uses a multimodal perception sub-agent without making it the planner or executor.
8. Identify what to benchmark before choosing a multimodal model for production.

---

**1. Why multimodal chains are hard**

Agentic systems increasingly need to **reason across** modalities:

- screenshots
- documents
- forms
- charts
- video
- audio
- speech
- on-screen text
- user messages
- tool results

A naive design uses separate models:

```text
ASR model for audio
OCR model for text in images
VLM for screenshots
video captioner for video
LLM for reasoning
planner for actions
```

This creates three problems.

**Cost**

Every hop costs **inference time and orchestration overhead**.

**Context drift**

Each model compresses its input differently. Important **cross-modal details** can be lost.

**Engineering complexity**

The harness must stitch together timestamps, frames, transcripts, OCR boxes, image regions, and textual summaries.

**Unified multimodal models** try to reduce this fragmentation.

---

**2. What Nemotron 3 Nano Omni is**

NVIDIA describes Nemotron 3 Nano Omni as an **open model** for unified video, audio, image, and text reasoning.

The important system framing:

```text
Nemotron 3 Nano Omni = multimodal perception and context sub-agent
```

It is designed to sit inside a larger agent system:

```text
multimodal inputs
  -> Nemotron 3 Nano Omni
  -> perception/context output
  -> planner/reasoning model
  -> tool/action layer
```

That matters because a multimodal model should **not automatically own all agent authority**.

It should usually answer:

```text
What is in this video/audio/document/screenshot?
What evidence supports that?
What context should the planner receive?
```

The executor should still be controlled by **structured tools, policies, and approval gates**.

---

**3. The architecture claim**

NVIDIA describes Nemotron 3 Nano Omni as:

```text
30B-A3B hybrid mixture-of-experts model
```

Interpretation:

- total parameter capacity is around 30B
- active parameters per token/task are around 3B
- experts are activated depending on the task and modality

Why this matters:

```text
large total capability
  + smaller active compute path
  -> higher throughput potential
```

**MoE is a serving tradeoff**.

It can reduce active compute but introduces **routing, expert placement, memory, and kernel complexity**.

For hardware engineers, this is not just a model design detail.

It affects:

- GPU memory placement
- expert parallelism
- batching behavior
- quantization strategy
- serving engine support
- interconnect pressure

---

</details>

## 4. 混合 Mamba + Transformer 核心

NVIDIA 表示，该模型结合了：

- Mamba layer，用于序列与内存效率
- Transformer layer，用于精确推理

系统级直觉：

| Layer 家族 | 优势 |
|---|---|
| Mamba/状态空间风格 layer | 高效的长序列处理与内存行为 |
| Transformer layer | 强大的 token 到 token 推理与灵活的 attention |

为什么这对多模态 agent 很重要：

```text
video + audio + documents
  -> long contexts
  -> expensive attention if handled naively
```

**混合架构**试图在保留推理能力的同时，降低持续长上下文感知的成本。

你仍然应该 **benchmark 实际工作负载**。

架构上的说法有用，但 **推理服务测量结果决定部署**。

---

## 5. 视频：3D 卷积与高效视频采样

视频 **不只是图像序列**。

模型需要 **运动与时间结构**。

NVIDIA 描述了两个相关机制：

| 机制 | 目的 |
|---|---|
| 3D 卷积 | 跨帧捕捉空间与时间模式 |
| 高效视频采样（EVS） | 将高密度视觉 token 压缩成更小集合，供 LLM 使用 |

为什么这很重要：

```text
raw frames are too dense
agent context windows are finite
video tokens can overwhelm the model
```

EVS 很重要，因为它把视频从：

```text
many frames -> too many tokens
```

变成：

```text
sampled/compressed video evidence -> tractable multimodal context
```

这直接连接到第 09 讲：

```text
screenshots and video are expensive inputs
so perception layers must compress them before planning/action loops
```

---

## 6. 音频与视觉编码器

NVIDIA 这篇博文描述的音频集成围绕 **NVIDIA Parakeet** 和专门数据集构建，超越了简单的转写。

它还描述了使用 **C-RADIOv4-H** 进行视觉处理，以实现高分辨率图像理解和 OCR 敏感细节。

系统视角：

```text
audio encoder
  -> speech, sound, temporal cues

vision encoder
  -> images, documents, screenshots, video patches

text decoder
  -> unified reasoning/output space
```

关键设计问题是：

```text
Does the model only transcribe/caption?
Or does it preserve enough multimodal structure for reasoning?
```

对于智能体化系统，**仅靠 captioning 通常不够**。

你需要 **有依据的上下文**：

- 可见内容是什么
- 何时发生
- 它出现在文档或帧中的什么位置
- 还存在哪些不确定性
- 哪些细节应传给工具或 planner

---

## 7. 训练规模与开放性

NVIDIA 表示，此次发布包含对 **权重、数据集和训练 recipe** 的访问。

博客报告称：

- 跨混合模态的适配器/编码器训练
- 监督微调将上下文长度从 16K 扩展到 49K，再到 262K
- 跨 25 种环境配置的 SFT 后强化学习
- 超过 2.3M 次环境 rollout
- 大约 127B 混合模态适配器/编码器训练 token
- 大约 124M 条精选多模态后训练样本
- 面向多模态任务的 20 个 RL 数据集，覆盖 25 种环境
- 合成数据流水线贡献约 11.4M 对视觉 QA

对本课程而言，重要的是：

```text
multimodal agent behavior is trained, evaluated, and aligned as a system,
not only assembled from pretrained single-modality parts.
```

对于企业和研究用户，开放权重和 recipe 很重要，因为它们支持：

- 私有部署
- 领域适配
- 审计数据/模型假设
- 可复现性工作
- 本地或本地化部署变体

**许可证和数据条款** 在商业部署前仍需审查。

---

## 8. 推理服务与硬件效率

NVIDIA 报告支持：

- NVIDIA Ampere 架构
- NVIDIA Hopper
- NVIDIA Blackwell
- vLLM
- TensorRT-LLM
- FP8
- NVFP4
- 优化 kernel

博客报告称，在固定交互性阈值下：

- 视频推理可维持的有效系统容量最高约为替代开放 omni 模型的 9.2 倍
- 多文档推理可维持的有效系统容量最高约为替代开放 omni 模型的 7.4 倍

**“固定交互性阈值”** 这个说法很重要。

它意味着该比较保持 **每用户响应性不变**，并衡量系统总共能维持多少工作。

对于 agent 来说，这比仅看 **原始最大吞吐** 是更好的推理服务指标。

Agent 用户关心的是：

```text
does my interaction stay responsive?
how many simultaneous agents can the system support at that responsiveness?
```

---


<details>
<summary>English original</summary>

**4. Hybrid Mamba + transformer core**

NVIDIA says the model combines:

- Mamba layers for sequence and memory efficiency
- transformer layers for precise reasoning

System-level intuition:

| Layer family | Strength |
|---|---|
| Mamba/state-space style layers | efficient long-sequence processing and memory behavior |
| Transformer layers | strong token-to-token reasoning and flexible attention |

Why this matters for multimodal agents:

```text
video + audio + documents
  -> long contexts
  -> expensive attention if handled naively
```

A **hybrid architecture** is trying to preserve reasoning while reducing the cost of sustained long-context perception.

You should still **benchmark the actual workload**.

Architecture claims are useful, but **serving measurements decide deployment**.

---

**5. Video: 3D convolution and efficient video sampling**

Video is **not just a sequence of images**.

The model needs **motion and temporal structure**.

NVIDIA describes two relevant mechanisms:

| Mechanism | Purpose |
|---|---|
| 3D convolution | captures spatial and temporal patterns across frames |
| Efficient Video Sampling (EVS) | compresses high-density visual tokens into a smaller set for the LLM |

Why this matters:

```text
raw frames are too dense
agent context windows are finite
video tokens can overwhelm the model
```

EVS is important because it turns video from:

```text
many frames -> too many tokens
```

into:

```text
sampled/compressed video evidence -> tractable multimodal context
```

This connects directly to Lecture 09:

```text
screenshots and video are expensive inputs
so perception layers must compress them before planning/action loops
```

---

**6. Audio and visual encoders**

The NVIDIA post describes audio integration built around **NVIDIA Parakeet** and specialized datasets, moving beyond simple transcription.

It also describes visual processing using **C-RADIOv4-H** for high-resolution image understanding and OCR-sensitive detail.

System view:

```text
audio encoder
  -> speech, sound, temporal cues

vision encoder
  -> images, documents, screenshots, video patches

text decoder
  -> unified reasoning/output space
```

The key design question:

```text
Does the model only transcribe/caption?
Or does it preserve enough multimodal structure for reasoning?
```

For agentic systems, **captioning alone is often insufficient**.

You need **grounded context**:

- what was visible
- when it happened
- where in the document or frame it appeared
- what uncertainty remains
- which details should be passed to tools or planner

---

**7. Training scale and openness**

NVIDIA says the release includes access to **weights, datasets, and training recipes**.

The blog reports:

- adapter/encoder training across mixed modalities
- supervised fine-tuning that expands context length from 16K to 49K to 262K
- post-SFT reinforcement learning across 25 environment configurations
- more than 2.3M environment rollouts
- roughly 127B mixed-modality adapter/encoder training tokens
- roughly 124M curated multimodal post-training examples
- 20 RL datasets across 25 environments for multimodal tasks
- synthetic-data pipelines contributing about 11.4M visual QA pairs

What matters for this course:

```text
multimodal agent behavior is trained, evaluated, and aligned as a system,
not only assembled from pretrained single-modality parts.
```

For enterprise and research users, open weights and recipes matter because they enable:

- private deployment
- domain adaptation
- audit of data/model assumptions
- reproducibility work
- local or on-premise variants

**License and data terms** still need review before commercial deployment.

---

**8. Serving and hardware efficiency**

NVIDIA reports support for:

- NVIDIA Ampere
- NVIDIA Hopper
- NVIDIA Blackwell
- vLLM
- TensorRT-LLM
- FP8
- NVFP4
- optimized kernels

The blog reports that, at fixed interactivity thresholds:

- video reasoning can sustain up to about 9.2x higher effective system capacity than alternative open omni models
- multi-document reasoning can sustain up to about 7.4x higher effective system capacity than alternative open omni models

The phrase **"fixed interactivity threshold"** is important.

It means the comparison holds **per-user responsiveness constant** and measures how much total work the system can sustain.

This is a better serving metric for agents than **raw maximum throughput** alone.

Agent users care about:

```text
does my interaction stay responsive?
how many simultaneous agents can the system support at that responsiveness?
```

---

</details>

## 9. 它在 agent 架构中的位置

推荐架构：

```text
raw multimodal input
  -> multimodal perception sub-agent
  -> structured context + citations/evidence
  -> planner/reasoning agent
  -> structured tools / Gateway RPC
  -> execution policy and audit
```

**不要把一切都收拢**进多模态模型。

**保持角色分离**：

| 角色 | 职责 |
|---|---|
| 感知子 agent | 理解视频/音频/图像/文档输入 |
| 规划器 | 决定任务分解 |
| 工具执行器 | 在策略约束下调用带类型的工具 |
| 验证器 | 检查结果与证据 |
| 网关/harness（agent 运行时框架） | 落实身份、会话、审批、日志 |

这是本课程反复出现的一条经验：

```text
model capability does not replace harness discipline
```

---

## 10. OpenClaw 映射

在 OpenClaw 风格的系统里：

```text
OpenClaw Gateway
  -> session and task state
  -> tool policy and approvals
  -> node/device inputs
  -> multimodal perception agent
  -> planner/executor agent
  -> artifacts and evidence
```

潜在用例：

| 用例 | 感知子 agent 的输出 |
|---|---|
| 视频会议分析 | 时间线、发言人、幻灯片、视觉事件、行动项 |
| 技术视频问答 | 带引用的视觉/音频证据与帧范围 |
| 屏幕录制调试 | UI 序列、出错点、可见日志 |
| 文档智能 | 表格、OCR、图表、跨文档事实 |
| 语音 + 屏幕助手 | 由语音请求与可见 UI 得到的统一状态 |
| 机器人/边缘 AI | 供下游规划器使用的场景/音频上下文 |

规划器应当收到 **结构化上下文**，而不是未经处理的无限量视频 token。

输出形状示例：

```json
{
  "summary": "...",
  "evidence": [
    {
      "modality": "video",
      "time_range": "00:02:10-00:02:42",
      "observation": "Chart shows revenue decline in Q3",
      "confidence": 0.86
    }
  ],
  "open_questions": ["..."],
  "recommended_next_tool": "query_document_index"
}
```

---

## 11. 与结构化工具 vs 计算机操作的关系

第 09 讲主张：

```text
structured tools beat screenshots when an interface exists
```

本讲补充：

```text
when raw multimodal perception is unavoidable,
use a perception model to compress and ground context before planning
```

层级变为：

```text
structured API/tool call
  -> direct CLI/exec
  -> DOM/accessibility
  -> multimodal perception model
  -> raw vision clicking loop
```

Nemotron 3 Nano Omni 属于 **感知层**。

它应当帮助 agent 理解多模态输入里有什么。

当存在结构化 API 时，它不应成为 agent 盲目到处点击的原因。

---

## 12. 采用前要 benchmark 什么

在选择任何多模态模型之前，先 **测量自己的实际工作负载**。

要 benchmark：

| 指标 | 为什么重要 |
|---|---|
| 输入模态组合 | 纯图像、视频、音频、文档、截图 |
| 上下文长度 | 长文档与视频时间线会给内存带来压力 |
| 首 token 时间 | 用户感知的交互性 |
| tokens/sec/user | 响应速度 |
| 总吞吐 | 并发 agent 的数量 |
| 每任务成本 | 模型与基础设施的经济性 |
| grounding 质量 | 答案是否引用了正确的帧/页/片段 |
| 幻觉率 | 尤其是视频与图表推理场景 |
| 工具交接质量 | 传给规划器的结构化上下文是否干净 |
| 部署目标 | 工作站、Jetson、数据中心、云 |

不要只 benchmark 准确率。

对 agent 系统，要 benchmark：

```text
accuracy
latency
throughput
cost
evidence quality
handoff quality
failure mode
```

---

## 13. 硬件工程师视角

对 GPU 与边缘工程师而言，Nemotron 3 Nano Omni 凸显了几条 **工作负载趋势**：

- hybrid MoE（混合专家模型）推理服务
- 激活参数效率
- 使用 FP8/NVFP4 的低精度推理
- 长上下文多模态推理服务
- 视频 token 压缩
- 多模态批处理
- KV 缓存压力
- MoE 专家布局
- 分离式推理服务与布线

要问的问题：

```text
Where are the experts placed?
How does routing behave under mixed modalities?
How large is the KV cache for video/document workloads?
Can the serving stack batch across modality mixes?
How does quantization affect OCR and audio reasoning?
What is the failure mode on smaller GPUs?
```

这就是多模态 agent 成为硬件相关话题的原因。

它们对 **内存、带宽、调度与推理服务引擎** 造成的压力与纯文本对话不同。

---


<details>
<summary>English original</summary>

**9. Where this fits in an agent architecture**

Recommended architecture:

```text
raw multimodal input
  -> multimodal perception sub-agent
  -> structured context + citations/evidence
  -> planner/reasoning agent
  -> structured tools / Gateway RPC
  -> execution policy and audit
```

**Do not collapse everything** into the multimodal model.

**Keep roles separate**:

| Role | Responsibility |
|---|---|
| Perception sub-agent | understand video/audio/image/document inputs |
| Planner | decide task decomposition |
| Tool executor | call typed tools under policy |
| Verifier | check results and evidence |
| Gateway/harness | enforce identity, sessions, approvals, logs |

This is the same lesson repeated across this course:

```text
model capability does not replace harness discipline
```

---

**10. OpenClaw mapping**

In an OpenClaw-style system:

```text
OpenClaw Gateway
  -> session and task state
  -> tool policy and approvals
  -> node/device inputs
  -> multimodal perception agent
  -> planner/executor agent
  -> artifacts and evidence
```

Potential use cases:

| Use case | Perception sub-agent output |
|---|---|
| video meeting analysis | timeline, speakers, slides, visual events, action items |
| technical video QA | cited visual/audio evidence and frame ranges |
| screen recording debug | UI sequence, error point, visible logs |
| document intelligence | tables, OCR, charts, cross-document facts |
| voice + screen assistant | unified state from spoken request and visible UI |
| robotics/edge AI | scene/audio context for downstream planner |

The planner should receive **structured context**, not raw unbounded video tokens.

Example output shape:

```json
{
  "summary": "...",
  "evidence": [
    {
      "modality": "video",
      "time_range": "00:02:10-00:02:42",
      "observation": "Chart shows revenue decline in Q3",
      "confidence": 0.86
    }
  ],
  "open_questions": ["..."],
  "recommended_next_tool": "query_document_index"
}
```

---

**11. Relation to structured tools vs computer use**

Lecture 09 argued:

```text
structured tools beat screenshots when an interface exists
```

This lecture adds:

```text
when raw multimodal perception is unavoidable,
use a perception model to compress and ground context before planning
```

The hierarchy becomes:

```text
structured API/tool call
  -> direct CLI/exec
  -> DOM/accessibility
  -> multimodal perception model
  -> raw vision clicking loop
```

Nemotron 3 Nano Omni belongs in the **perception layer**.

It should help the agent understand what is in multimodal inputs.

It should not be the reason the agent clicks around blindly when a structured API exists.

---

**12. What to benchmark before adopting**

Before choosing any multimodal model, **measure your actual workload**.

Benchmark:

| Metric | Why it matters |
|---|---|
| input modality mix | image-only, video, audio, docs, screenshots |
| context length | long documents and video timelines stress memory |
| time-to-first-token | perceived interactivity |
| tokens/sec/user | responsiveness |
| aggregate throughput | number of concurrent agents |
| cost per task | model and infrastructure economics |
| grounding quality | whether answers cite the correct frame/page/clip |
| hallucination rate | especially for video and chart reasoning |
| tool handoff quality | whether structured planner context is clean |
| deployment target | workstation, Jetson, data center, cloud |

Do not benchmark only accuracy.

For agent systems, benchmark:

```text
accuracy
latency
throughput
cost
evidence quality
handoff quality
failure mode
```

---

**13. Hardware engineer view**

For GPU and edge engineers, Nemotron 3 Nano Omni highlights several **workload trends**:

- hybrid MoE serving
- active-parameter efficiency
- low-precision inference with FP8/NVFP4
- long-context multimodal serving
- video token compression
- multimodal batching
- KV-cache pressure
- MoE expert placement
- disaggregated serving and routing

Questions to ask:

```text
Where are the experts placed?
How does routing behave under mixed modalities?
How large is the KV cache for video/document workloads?
Can the serving stack batch across modality mixes?
How does quantization affect OCR and audio reasoning?
What is the failure mode on smaller GPUs?
```

This is why multimodal agents are a hardware-relevant topic.

They stress **memory, bandwidth, scheduling, and serving engines** differently from text-only chat.

---

</details>

## 14. Mini-lab：设计一个多模态 sub-agent

为以下某一工作流设计一个感知 sub-agent：

1. 视频讲座摘要
2. 屏幕录制调试
3. 医疗/工业文档审阅
4. 机器人场景/音频上下文
5. 会议 + 幻灯片分析

定义：

```text
input modalities
output schema
evidence format
planner handoff
tool handoff
latency target
privacy boundary
deployment target
fallback model
verification method
```

然后回答：

```text
What should this sub-agent decide?
What must it never decide?
What evidence should it preserve?
When should it call structured tools instead of looking at pixels?
```

---

## 关键要点

- Nemotron 3 Nano Omni 最合适的定位，是面向更大 agent 系统的多模态感知/上下文 sub-agent。
- 该模型被描述为一个 30B-A3B 混合 MoE（混合专家模型），结合了 Mamba 与 Transformer layer。
- 它统一了文本、图像、视频与音频输入，以减少碎片化的多模态链路。
- 高效的视频采样、3D 视觉处理、模态编码器、FP8/NVFP4 以及优化的推理服务引擎，才是真正重要的系统级细节。
- NVIDIA 报告称，在固定的交互性阈值下，视频与多文档工作负载的有效系统容量更高；采用之前请在自己的工作负载上验证这些说法。
- 把感知、规划、执行、验证与策略保持为独立的 harness（agent 运行时框架）角色。
- 对 OpenClaw 风格的系统，多模态模型应为规划器产出结构化的上下文与证据，而不是绕过结构化工具或审批。

---

## 参考文献

- NVIDIA Technical Blog，"NVIDIA Nemotron 3 Nano Omni Powers Multimodal Agent Reasoning in a Single Efficient Open Model"：[https://developer.nvidia.com/blog/nvidia-nemotron-3-nano-omni-powers-multimodal-agent-reasoning-in-a-single-efficient-open-model/](https://developer.nvidia.com/blog/nvidia-nemotron-3-nano-omni-powers-multimodal-agent-reasoning-in-a-single-efficient-open-model/)
- 在 Hugging Face 上的 NVIDIA Nemotron 3 Nano Omni：[https://huggingface.co/nvidia/Nemotron-3-Nano-Omni-30B-A3B](https://huggingface.co/nvidia/Nemotron-3-Nano-Omni-30B-A3B)
- NVIDIA Nemotron 模型系列：[https://huggingface.co/collections/nvidia/nemotron-3-68f03d30beec3d477b293a12](https://huggingface.co/collections/nvidia/nemotron-3-68f03d30beec3d477b293a12)
- Lecture 28 - Runtime Strategy：[Lecture-28.md](Lecture-28.md)
- Lecture 09 - Structured Tools Beat Computer Use：[Lecture-09.md](Lecture-09.md)

---

*下一章：[Lecture 21](Lecture-21.md)*


<details>
<summary>English original</summary>

**14. Mini-lab: design a multimodal sub-agent**

Design a perception sub-agent for one workflow:

1. video lecture summarization
2. screen recording debug
3. medical/industrial document review
4. robotics scene/audio context
5. meeting + slide analysis

Define:

```text
input modalities
output schema
evidence format
planner handoff
tool handoff
latency target
privacy boundary
deployment target
fallback model
verification method
```

Then answer:

```text
What should this sub-agent decide?
What must it never decide?
What evidence should it preserve?
When should it call structured tools instead of looking at pixels?
```

---

**Key takeaways**

- Nemotron 3 Nano Omni is best understood as a multimodal perception/context sub-agent for larger agent systems.
- The model is described as a 30B-A3B hybrid MoE that combines Mamba and transformer layers.
- It unifies text, image, video, and audio inputs to reduce fragmented multimodal chains.
- Efficient video sampling, 3D visual processing, modality encoders, FP8/NVFP4, and optimized serving engines are the system-level details that matter.
- NVIDIA reports higher effective system capacity at fixed interactivity thresholds for video and multi-document workloads; validate claims on your own workload before adopting.
- Keep perception, planning, execution, verification, and policy as separate harness roles.
- For OpenClaw-style systems, multimodal models should produce structured context and evidence for the planner, not bypass structured tools or approvals.

---

**References**

- NVIDIA Technical Blog, "NVIDIA Nemotron 3 Nano Omni Powers Multimodal Agent Reasoning in a Single Efficient Open Model": [https://developer.nvidia.com/blog/nvidia-nemotron-3-nano-omni-powers-multimodal-agent-reasoning-in-a-single-efficient-open-model/](https://developer.nvidia.com/blog/nvidia-nemotron-3-nano-omni-powers-multimodal-agent-reasoning-in-a-single-efficient-open-model/)
- NVIDIA Nemotron 3 Nano Omni on Hugging Face: [https://huggingface.co/nvidia/Nemotron-3-Nano-Omni-30B-A3B](https://huggingface.co/nvidia/Nemotron-3-Nano-Omni-30B-A3B)
- NVIDIA Nemotron model family: [https://huggingface.co/collections/nvidia/nemotron-3-68f03d30beec3d477b293a12](https://huggingface.co/collections/nvidia/nemotron-3-68f03d30beec3d477b293a12)
- Lecture 28 - Runtime Strategy: [Lecture-28.md](Lecture-28.md)
- Lecture 09 - Structured Tools Beat Computer Use: [Lecture-09.md](Lecture-09.md)

---

*Next: [Lecture 21](Lecture-21.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-20.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-20.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
