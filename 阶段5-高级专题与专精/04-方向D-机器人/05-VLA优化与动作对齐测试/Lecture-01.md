---
title: Lecture 1: 面向实时控制的 VLA 优化
description: Lecture 1: 面向实时控制的 VLA 优化
---

# Lecture 1: 面向实时控制的 VLA 优化

## 概述

从 runtime 的角度看，VLA（视觉-语言-动作模型）policy 是一种格外不听话的 Transformer：

* 每个控制 tick 都要吞下 **N 路相机流**（而不是只输入一次的文本 prompt）
* 它在**硬实时循环内端到端跑推理**（而非交互式运行）
* 它只吐出**一小串 action token 或去噪步**，不是长篇生成
* 它必须与感知、ROS 2、costmap 和遥操作 UI **共用同一块 Jetson 或工作站 GPU**

本讲的任务，是把一个已发布的 **VLA 检查点**——OpenVLA-7B、π₀（3.3B）、GR00T N1.5（2-3B 级别），或某个 Qwen2-VL 级别的自定义 policy——改造成能在**真实硬件上闭环控制**、且不改变机器人行为的东西。配套的测量框架见 [Lecture 2](Lecture-02.md)；假定它已存在，并能验证每一步。

读完后，你应当能够：

* 为特定目标平台上的特定 VLA 画出每 tick 的 FLOP 与带宽预算（例如 Jetson AGX Orin 64 GB 上、224×224 × 1 路相机的 OpenVLA-7B）
* 为 LLM 主干、vision tower 和 action head 分别挑对量化 recipe——这三者通常是三个不同的决定
* 解释为什么在 LLM 已经量化之后，vision tower 往往才是最拖后腿的那个
* 对以短自回归突发方式输出的 VLA 实现 action chunk 缓存与 KV cache 复用
* 判断什么时候该放弃量化，转而蒸馏到更小的主干

---

## 1. 每个控制 tick 都跑些什么

抛开宣传话术，一个 VLA 控制 tick 就是：

```text
                  ┌──────────────────────────────────────────┐
   sensors  ──►   │  preprocess: resize, normalize, layout    │  ~1-5 ms
                  └──────────────┬───────────────────────────┘
                                 ▼
                  ┌──────────────────────────────────────────┐
   vision tower   │  SigLIP / DINOv2 / EVA — frozen ViT       │  10-80 ms
                  │  N_cameras × 1-2 image tokens stream      │
                  └──────────────┬───────────────────────────┘
                                 ▼
                  ┌──────────────────────────────────────────┐
   projector      │  MLP or Perceiver resampler               │  <1 ms
                  └──────────────┬───────────────────────────┘
                                 ▼
                  ┌──────────────────────────────────────────┐
   LLM backbone   │  prefill (256-2048 tokens) + decode       │  20-300 ms
                  │  Llama-7B / Gemma-2B / Qwen2-VL class     │
                  └──────────────┬───────────────────────────┘
                                 ▼
                  ┌──────────────────────────────────────────┐
   action head    │  token detok / MLP regression / diffusion │  1-30 ms
                  └──────────────┬───────────────────────────┘
                                 ▼
                            7-DoF action  ──►  ROS 2 controller
```

有三件事要记牢：

1. **prompt 大时，prefill（首字前的整段计算）占主导。** 一些 OpenX 风格的 policy 每个 tick 都把任务描述 + N 路相机视图 + 本体感受状态序列化成 >1500 token 的前缀。此时 prefill 变成**算力受限的矩阵乘**；decode（逐 token 生成阶段）则是另一类**带宽受限**的问题。
2. **decode 很短。** OpenVLA 输出 7 个离散化 action token。π₀ 在 50 步 action chunk 上跑一个 10 步的 flow-matching head。RT-2 风格每条手臂输出几个 action token。几乎不会 decode 超过 ~32 个 token，所以那些对长文本 LLM 有效的逐 token decode 技巧（连续批处理、paged KV）在这里帮助有限。
3. **vision tower 每 tick 的计算量恒定，且不是文本形态。** 它无法从 KV cache 中获益。在 LLM 量化之后，它是最常见的**意外瓶颈**。

### 1.1 OpenVLA-7B 在 Jetson AGX Orin（64 GB，MAXN）上的具体预算

参考数值，已取整，单路 224×224 相机，prompt ~280 token，7 个 action token，fp16：

| 阶段 | ms | 访存量 | 备注 |
|-------|----|----------------|-------|
| 预处理（CPU） | 2 | — | resize + normalize，单相机 |
| Vision tower（SigLIP-So400m，~400M） | 25 | 权重 ~800 MB | 以 attention 矩阵乘为主，batch=1 |
| Projector | <1 | 很小 | 可忽略 |
| LLM prefill（Llama-2-7B，280 tok） | 110 | 权重 ~13 GB | 在该 prompt 长度下算力受限 |
| LLM decode（7 个 token） | 60 | 每 tick 的 KV ~0（已重置） | 带宽受限，~8.5 ms/token |
| Action 反 token 化 | <1 | — | 每轴 256 bin 查表 |
| **总计** | **~200** | | 超出 50 ms 控制预算 4 倍 |

这张表是你在动手优化*之前*就要写出来的。之后得到的每个数字都要与这一行对比。

---


<details>
<summary>English original</summary>

**Lecture 1: VLA Optimization for Real-Time Control**

**Overview**

A Vision-Language-Action policy is, from the runtime's point of view, an unusually inconvenient transformer:

* it eats **N camera streams** every control tick (not a text prompt typed once)
* it runs **inference end-to-end inside a hard real-time loop** (not interactively)
* it emits **a small burst of action tokens or denoising steps**, not a long generation
* it has to **share a Jetson or workstation GPU** with perception, ROS 2, costmaps, and a teleop UI

The job of this lecture is to convert a published **VLA checkpoint** — OpenVLA-7B, π₀ (3.3B), GR00T N1.5 (2-3B class), or a Qwen2-VL-class custom policy — into something that **closes the control loop on real hardware** without changing what the robot does. The companion measurement framework lives in [Lecture 2](Lecture-02.md); we will assume it exists and validates every step.

By the end you should be able to:

* draw the per-tick FLOP and bandwidth budget for a specific VLA on a specific target (e.g. OpenVLA-7B at 224×224 × 1 camera on Jetson AGX Orin 64 GB)
* pick the right quantization recipe for the LLM backbone, the vision tower, and the action head — these are usually three different decisions
* explain why the vision tower is often the worst offender once you have already quantized the LLM
* implement action-chunk caching and KV-cache reuse for VLAs that emit short autoregressive bursts
* know when to walk away from quantization and distill to a smaller backbone instead

---

**1. What runs every control tick**

Strip the marketing away and a VLA control tick is:

```text
                  ┌──────────────────────────────────────────┐
   sensors  ──►   │  preprocess: resize, normalize, layout    │  ~1-5 ms
                  └──────────────┬───────────────────────────┘
                                 ▼
                  ┌──────────────────────────────────────────┐
   vision tower   │  SigLIP / DINOv2 / EVA — frozen ViT       │  10-80 ms
                  │  N_cameras × 1-2 image tokens stream      │
                  └──────────────┬───────────────────────────┘
                                 ▼
                  ┌──────────────────────────────────────────┐
   projector      │  MLP or Perceiver resampler               │  <1 ms
                  └──────────────┬───────────────────────────┘
                                 ▼
                  ┌──────────────────────────────────────────┐
   LLM backbone   │  prefill (256-2048 tokens) + decode       │  20-300 ms
                  │  Llama-7B / Gemma-2B / Qwen2-VL class     │
                  └──────────────┬───────────────────────────┘
                                 ▼
                  ┌──────────────────────────────────────────┐
   action head    │  token detok / MLP regression / diffusion │  1-30 ms
                  └──────────────┬───────────────────────────┘
                                 ▼
                            7-DoF action  ──►  ROS 2 controller
```

Three things to internalize:

1. **Prefill dominates if the prompt is large.** Some OpenX-style policies serialize task description + N camera views + proprioceptive state into >1500 tokens of prefix every tick. Prefill becomes a **compute-bound matmul**; decode is a different, **bandwidth-bound** problem.
2. **Decode is short.** OpenVLA emits 7 discretized action tokens. π₀ runs a 10-step flow-matching head over a 50-step action chunk. RT-2-style emits a few action tokens per arm. You almost never decode more than ~32 tokens, so per-token decode tricks that help long-form LLMs (continuous batching, paged KV) help less here.
3. **Vision tower is constant per tick and not text-shaped.** It does not benefit from KV cache. It is the most common **surprise bottleneck** after the LLM is quantized.

**1.1 Concrete budget for OpenVLA-7B on Jetson AGX Orin (64 GB, MAXN)**

Reference numbers, rounded, single 224×224 camera, prompt ~280 tokens, 7 action tokens, fp16:

| Stage | ms | Memory traffic | Notes |
|-------|----|----------------|-------|
| Preprocess (CPU) | 2 | — | resize + normalize, single camera |
| Vision tower (SigLIP-So400m, ~400M) | 25 | weights ~800 MB | dominated by attention matmuls, batch=1 |
| Projector | <1 | small | negligible |
| LLM prefill (Llama-2-7B, 280 tok) | 110 | weights ~13 GB | compute-bound at this prompt length |
| LLM decode (7 tokens) | 60 | KV per tick ~0 (reset) | bandwidth-bound, ~8.5 ms/token |
| Action detokenize | <1 | — | 256-bin lookup per axis |
| **Total** | **~200** | | misses 50 ms control budget by 4× |

This is the table you write *before* you optimize anything. Every later number gets compared to this row.

---

</details>

## 2. 优化阶梯

从最廉价到侵入性最强，每一级都以动作精度一致性 harness（agent 运行时框架）作为门槛：

| 层级 | 技术 | Jetson AGX 上的典型加速 | 精度一致性风险 |
|------|-----------|-------------------------------|-------------|
| 0 | bf16 → fp16，融合 RMSNorm，kernel 选择 | 1.1-1.3× | 若数值经验证则为无 |
| 1 | 静态 KV cache + 用于 decode（逐 token 生成阶段）的 CUDA Graphs | 仅在 decode 上 1.2-1.5× | 无，确定性的 |
| 2 | 视觉塔 → TensorRT FP16 / FP8 | 视觉阶段 2-4× | 低；ViT 对 FP8 鲁棒 |
| 3 | LLM 主干仅权重 INT8 / INT4（AWQ / GPTQ） | 在 prefill（首字前的整段计算）+ decode 上 2-3× | 中；需要 action-MSE 检查 |
| 4 | FP8 KV cache，FP8 attention | 在 decode 上 1.2-1.5× | 中高；对长动作块影响更大 |
| 5 | 动作块缓存 / 时间复用 | 有效墙钟加速最高 5-10× | 取决于任务 |
| 6 | 投机动作解码（小型草稿模型） | 在 decode 上 1.5-2× | 若草稿模型蒸馏良好则为低 |
| 7 | 蒸馏到更小的主干（如 7B → 2-3B 级别） | 全面 2-3× | 高；需要完整重新评估 |
| 8 | 架构手术（去掉一个相机、更小的图像、更少的动作步数） | 1.5-3× | 高；改变策略类别 |

规则：选取仍满足来自 [Lecture 2](Lecture-02.md) 的精度一致性标准的**最廉价层级**，然后停下。大多数团队会做过头，交付一个**在连续第三次 rollout 时就漂移**的策略。

---

## 3. 层级 0-1：免费收益（数值 + 调度）

### 3.1 精度选择

* 视觉塔几乎总是以 bf16 / fp16 交付。在 Jetson 上保持 fp16 —— bf16 在 Ampere 架构 / 较旧的 Orin SKU 上缺少 tensor core 加速。
* 已发表的 VLA 中的 LLM 主干通常是 bf16。离线转换一次权重。在继续之前，用精度一致性 harness 在至少一个 episode 上验证。
* 动作头，如果它是回归多层感知机或 diffusion 头，**是要保持最高精度的地方**。这里的误差不会被 softmax 吸收；它们**直接作用到执行器上**。默认 fp16；只有当 Lecture 2 的逐轴误差预算允许时，才进一步降低。

### 3.2 静态 KV cache + CUDA Graphs

由于 decode 突发很短，且动作 token 数量固定（OpenVLA 总是输出 7 个），可以：

* 为 `max_prefix + max_action_tokens` 预分配 KV buffer
* 在首次 warmup tick 之后将 decode 步骤捕获为 CUDA Graph
* 避免在 Jetson 上主导 7-token decode 的逐 token Python 开销

这纯粹是调度。输出的比特不变。在继续之前，针对同一 prompt 对未做 Graph 化的版本运行位精确性检查。

### 3.3 Pinned memory + 从相机节点零拷贝

在 ROS 2 中，最慢的路径通常是 `sensor_msgs/Image` → 主机缓冲区 → CUDA 拷贝 → ViT 输入。使用带 CUDA 映射缓冲区池的 `image_transport`，或者如果你在支持 NITROS 的 Jetson 上，就用 NITROS / Isaac ROS GXF。每次 tick 节省 3-8 ms，且无任何数值代价。

---

## 4. 层级 2：将视觉塔 TensorRT 化

一旦 LLM 变小或量化，**ViT 就成为显见的开销**。在这项工作之前，384×384 × 2 个相机下的 SigLIP-So400m 在 Jetson AGX Orin 上可能要 60-90 ms。

Recipe：

1. 将视觉编码器导出为 ONNX，输入形状固定（batch, N_cameras, 3, H, W）。动态形状会拖累 TRT。
2. 用 FP16 构建 TensorRT engine；在支持 FP8 的 Orin 上，尝试 FP8，并用以约 200 张任务典型图像校准的逐 tensor scale。
3. 用一个轻量的 TRT wrapper 替换 PyTorch 编码器，该 wrapper 在同一个 CUDA 流上返回 projector 输入。
4. 验证 projector 输入 embedding 相对 PyTorch 参考的余弦相似度：目标均值 ≥ 0.999，最差情况 ≥ 0.99。

如果相似度跌破这些阈值，就不要继续 —— 在一个退化的视觉 embedding 之上量化 LLM，会在没有任何单个阶段看起来有错的情况下，悄无声息地摧毁任务成功率。

---

## 5. 层级 3：主干仅权重量化

与 [Qwen3-4B Q4 lecture](../../03-方向C-边缘AI/05-Qwen推理优化/Lecture-02.md) 属于同一 recipe 家族，应用于 VLA 的 LLM 部分。值得了解的差异：

* **校准数据必须是 VLA 形态的，而不是文本形态的。** 用 WikiText 校准集会产生一个主干，它幻觉出看似合理的英文补全，并在动作 token 上失败。使用来自任务套件的 256-1024 条轨迹 prompt 作为校准输入。
* **动作 token 的 logits 集中在一个小的子词表上**（对 OpenVLA 而言通常是 256 个 bin × 7 个轴）。逐通道权重量化可能会过度抑制这些 token。除了权重误差之外，还要用 harness 在动作 token 的 logit KL 散度上验证。
* **如果动作 token 来自 LM head，就不要量化 LM head。** LM head 的矩阵乘很小，所以保持其为 FP16 的吞吐代价可以忽略，而精度一致性收益很大。

AWQ 和 GPTQ 都可用；AWQ 在实践中往往更好地保留动作 token 的 logits，因为它对激活值感知。INT4 group-128 通常是最佳平衡点。在 7B 级别的主干上，INT3 很少值得精度一致性的损失。

---


<details>
<summary>English original</summary>

**2. The optimization ladder**

Cheapest to most invasive, with the action-parity harness as the gate at every rung:

| Rung | Technique | Typical speedup on Jetson AGX | Parity risk |
|------|-----------|-------------------------------|-------------|
| 0 | bf16 → fp16, fuse RMSNorm, kernel selection | 1.1-1.3× | none if numerics validated |
| 1 | Static KV-cache + CUDA Graphs for decode | 1.2-1.5× on decode only | none, deterministic |
| 2 | Vision tower → TensorRT FP16 / FP8 | 2-4× on vision stage | low; ViT is robust to FP8 |
| 3 | LLM backbone weight-only INT8 / INT4 (AWQ / GPTQ) | 2-3× on prefill + decode | medium; needs action-MSE check |
| 4 | FP8 KV cache, FP8 attention | 1.2-1.5× on decode | medium-high; affects long action chunks more |
| 5 | Action-chunk caching / temporal reuse | up to 5-10× wall-clock effective | task-dependent |
| 6 | Speculative action decoding (small draft model) | 1.5-2× on decode | low if draft is well-distilled |
| 7 | Distill to a smaller backbone (e.g. 7B → 2-3B class) | 2-3× across the board | high; full re-evaluation required |
| 8 | Architecture surgery (drop a camera, smaller image, fewer action steps) | 1.5-3× | high; changes the policy class |

The rule: take the **cheapest rung** that still meets the parity bar from [Lecture 2](Lecture-02.md), then stop. Most teams overshoot and ship a policy that **drifts at the third rollout** in a row.

---

**3. Rung 0-1: free wins (numerics + scheduling)**

**3.1 Precision selection**

* The vision tower is almost always shipped in bf16 / fp16. Stay in fp16 on Jetson — bf16 lacks tensor-core acceleration on Ampere / older Orin SKUs.
* The LLM backbone in published VLAs is usually bf16. Convert weights once, offline. Validate with the parity harness on at least one episode before going further.
* The action head, if it is a regression MLP or a diffusion head, is **the place you keep highest precision**. Errors here are not absorbed by softmax; they **hit the actuator directly**. Default fp16; only drop further if Lecture 2's per-axis error budget allows it.

**3.2 Static KV cache + CUDA Graphs**

Because decode bursts are short and the action-token count is fixed (OpenVLA always emits 7), you can:

* preallocate the KV buffer for `max_prefix + max_action_tokens`
* capture the decode step as a CUDA Graph after the first warmup tick
* avoid the per-token Python overhead that dominates a 7-token decode on Jetson

This is pure scheduling. The output bits are unchanged. Run a bit-exactness check against the un-graphed version on the same prompt before moving on.

**3.3 Pinned memory + zero-copy from the camera node**

In ROS 2 land, the slowest path is often `sensor_msgs/Image` → host buffer → CUDA copy → ViT input. Use `image_transport` with a CUDA-mapped buffer pool, or NITROS / Isaac ROS GXF if you are on a Jetson with NITROS support. Saves 3-8 ms per tick at zero numerical cost.

---

**4. Rung 2: TensorRT-ify the vision tower**

Once the LLM is small or quantized, the **ViT becomes the visible cost**. SigLIP-So400m at 384×384 × 2 cameras can be 60-90 ms on Jetson AGX Orin before this work.

Recipe:

1. Export the vision encoder to ONNX with fixed input shape (batch, N_cameras, 3, H, W). Dynamic shapes hurt TRT.
2. Build a TensorRT engine in FP16; on Orin with FP8 support, try FP8 with per-tensor scales calibrated on ~200 task-typical images.
3. Replace the PyTorch encoder with a thin TRT wrapper that returns the projector input on the same CUDA stream.
4. Validate the projector input embedding cosine similarity vs the PyTorch reference: target ≥ 0.999 mean, ≥ 0.99 worst-case.

If similarity drops below those thresholds, do not proceed — quantizing the LLM on top of a degraded vision embedding silently destroys task success without any one stage looking wrong.

---

**5. Rung 3: backbone weight-only quantization**

Same recipe family as the [Qwen3-4B Q4 lecture](../../03-方向C-边缘AI/05-Qwen推理优化/Lecture-02.md), applied to the LLM half of the VLA. Differences worth knowing:

* **Calibration data must be VLA-shaped, not text-shaped.** A WikiText calibration set will produce a backbone that hallucinates plausible English completions and fails on action tokens. Use 256-1024 trajectory prompts from your task suite as calibration input.
* **Action-token logits are concentrated on a small subvocab** (typically 256 bins × 7 axes for OpenVLA). Per-channel weight quantization can over-suppress these tokens. Validate with the harness on action-token logit KL divergence in addition to weight error.
* **Do not quantize the LM head if action tokens come from it.** The LM head matmul is small, so the throughput cost of keeping it FP16 is negligible, and the parity payoff is large.

AWQ and GPTQ both work; AWQ tends to preserve action-token logits better in practice because it is activation-aware. INT4 group-128 is the usual sweet spot. INT3 is rarely worth the parity hit on 7B-class backbones.

---

</details>

## 6. 阶梯 4：FP8 KV cache 与 FP8 attention

只有当 action chunk 足够长、以致 decode（逐 token 生成阶段）侧的带宽变得关键时才值得做——π₀ 风格的 flow-matching 策略会在 50 步 chunk 上重新 attention，因此受益；OpenVLA 的 7-token 突发则收益不明显。

如果确实启用了它：

* KV scale 保持 **per-head** 而非 per-tensor —— VLA 中存在专门处理 prefix 视觉 token 区域的 head，per-tensor scale 会让它们溢出
* 重跑逐 step 的 action MSE 检查；FP8 KV 最常见的问题是 chunk 中*靠后*的 token 出现漂移，而不是第一个 token

---

## 7. 阶梯 5：action-chunk 缓存与时间复用

这是 VLA 独有的阶梯，却常被忽视，因为它看起来像作弊。

观察：机器人的场景**并不会按相机帧率变化**。如果策略输出 N 步 action chunk（π₀ 的 50 步视野、ALOHA 风格的分块模仿、带 action chunking 的 RT-2），可以：

* 每 K 个 tick 跑一次完整 VLA（例如每 5 个 tick，即 10 Hz 而非 50 Hz）
* 中间执行缓存的 action chunk
* 若闭环监控（proprio 不匹配、力尖峰、视觉差异）触发，则提前重跑策略

有效控制频率不变；有效计算量降至 1/K。这就是「在 Jetson 上以 4 Hz 运行」与「在 Jetson 上以 30 Hz 有效运行」的差别。

风险在于：第 2 讲的精度一致性 harness（agent 运行时框架）必须包含 **chunk-cache rollout 模式**，而不只是逐 tick 的精度一致性。一个逐 tick 位精确的策略仍可能失败，因为缓存的 action 在快速运动的子轨迹（例如最后的抓取闭合）中会变陈旧。harness 的成功率精度一致性指标才能告诉你每个任务的安全 K 值。

---

## 8. 阶梯 6：投机 action 解码

把标准投机解码适配到 action token：

* 一个极小的 draft 模型（来自蒸馏后更小 VLA 的 action head，或 0.5B 级 LLM）提出接下来 k 个 action token
* 完整 VLA 并行验证，并接受最长的匹配 prefix

对 7-token 的 OpenVLA 突发，收益有限（约 1.3-1.7× decode）。对输出更长 chunk 的策略，或对双臂 14-DoF action 序列，收益是实打实的（约 2×）。

harness 必须验证：验证后的输出与未投机的 decode 输出位精确一致——这是本讲中唯一能在 *logit* 级别而非仅 action 级别证明精度一致性的优化。

---

## 9. 阶梯 7：量化用尽时的蒸馏

如果阶梯 0-6 都已用尽，仍达不到控制预算，下一步就是**更换 backbone**。做法：

* 保持 vision tower 冻结（它承担了繁重的感知工作）
* 把 7B LLM 换成 2-3B 级的（Gemma-2-2B、Qwen2.5-3B、Llama-3.2-3B）
* 用你的轨迹数据集在原始策略的 action 分布上做蒸馏
* 在新 backbone 的 hidden states 上从头重训 action head

这是另一个项目——此时是策略训练，而不是策略部署。但*部署侧*仍会在新的 student 上用上上面每一级阶梯。第 2 讲的精度一致性 harness 就是同一个 harness，只是以未蒸馏的 teacher 作为参考。

---

## 10. 阶梯 8：架构手术（最后手段）

有时为了适配机器人，必须改变策略类别：

* 去掉一个相机（3 → 2 或 2 → 1）——按任务类别测量对成功率的影响
* 降低图像分辨率（384 → 224 或 224 → 160）——对近距操作通常安全，对精细抓取是致命的
* 缩短 action chunk（50 → 20 步）——与阶梯 5 相互影响；harness 会告诉你何时会失效
* 去掉一种模态（proprioception 文本、力 token）——几乎从不安全；只有在用更快的编码替换它之后才重新考虑

上述每一项都要求 harness 重新建立基线，并针对未做架构手术的策略重新发布精度一致性数据。没有别的办法能知道。

---

## 11. 参考部署路径

明确覆盖两个目标，因为大多数读者会从中选一个：

### 11.1 Jetson AGX Orin 64 GB（边缘）

* Backbone：AWQ-INT4（group-128），LM head FP16
* Vision tower：TensorRT FP16（若 6.x JetPack 提供 FP8 路径则用 FP8）
* Action head：FP16
* Runtime：ARM64 版 vLLM + 视觉用 TRT，或若想要单一二进制则用 llama.cpp CUDA 后端
* 控制循环封装：以 30-50 Hz 发布 `JointTrajectory` 的 ROS 2 节点；策略本身以 chunk-cache K=5 运行
* 内存预算：策略约 14 GB，感知约 6 GB，ROS 2 + costmap + nav 约 4 GB

### 11.2 单 GPU 工作站（RTX 6000 Ada / L4 / 4090）

* Backbone：FP8（若为 Ada / Hopper）或 AWQ-INT4
* Vision tower：TRT FP8 或 torch.compile + flash-attn
* Action head：FP16 / BF16
* Runtime：vLLM、SGLang 或 TensorRT-LLM
* 用例：实验室机器人、遥操作辅助、sim-in-the-loop 开发；延迟预算更宽松（约 30 ms），但确定性仍然重要

---


<details>
<summary>English original</summary>

**6. Rung 4: FP8 KV cache and FP8 attention**

Worthwhile only if your action chunk is long enough that decode-side bandwidth matters — π₀-style flow-matching policies that re-attend over a 50-step chunk benefit; OpenVLA's 7-token burst does not noticeably.

If you do enable it:

* keep the KV scales **per-head** rather than per-tensor — VLAs have heads that specialize in the visual-token region of the prefix, and per-tensor scales overflow them
* re-run the per-step action MSE check; FP8 KV most often shows up as a drift on the *later* tokens of a chunk, not the first

---

**7. Rung 5: action-chunk caching and temporal reuse**

This is the rung that is unique to VLAs and gets neglected because it looks like cheating.

The observation: a robot's scene **does not change at the rate of the camera frame**. If the policy emits an N-step action chunk (π₀'s 50-step horizon, ALOHA-style chunked imitation, RT-2 with action chunking), you can:

* run the full VLA every K-th tick (e.g. every 5th, at 10 Hz instead of 50 Hz)
* execute the cached action chunk in between
* re-run the policy early if a closed-loop monitor (proprio mismatch, force spike, vision delta) triggers

Effective control rate is unchanged; effective compute drops by K. This is the difference between "runs on a Jetson at 4 Hz" and "runs on a Jetson at 30 Hz effective."

The hazard: the parity harness in Lecture 2 must include a **chunk-cache rollout mode**, not just per-tick parity. A policy that is bit-exact per tick can still fail because the cached actions become stale during a fast-moving sub-trajectory (e.g. final grasp closure). The harness's success-rate-parity metric is what tells you the safe K per task.

---

**8. Rung 6: speculative action decoding**

Standard speculative decoding adapted to action tokens:

* a tiny draft model (the action head from a distilled smaller VLA, or a 0.5B-class LLM) proposes the next k action tokens
* the full VLA verifies in parallel and accepts the longest matching prefix

For 7-token OpenVLA bursts, the wins are modest (~1.3-1.7× decode). For policies that emit longer chunks, or for bimanual 14-DoF action sequences, the wins are real (~2×).

The harness must validate that the verified outputs are bit-exact to the un-speculated decoded outputs — this is the one optimization in this lecture where you can prove parity at the *logit* level, not just the action level.

---

**9. Rung 7: distillation when quantization runs out**

If you have exhausted rungs 0-6 and still miss the control budget, the next step is to **swap the backbone**. The pattern:

* keep the vision tower frozen (it is doing the heavy perception work)
* swap the 7B LLM for a 2-3B class one (Gemma-2-2B, Qwen2.5-3B, Llama-3.2-3B)
* distill on the original policy's action distribution using your trajectory dataset
* re-train the action head from scratch on the new backbone's hidden states

This is a different project — it is now policy training, not policy deployment. But the *deployment side* still uses every rung above on the new student. The parity harness in Lecture 2 is the same harness, with the un-distilled teacher as the reference.

---

**10. Rung 8: architecture surgery (last resort)**

Sometimes you have to change the policy class to fit the robot:

* drop a camera (3 → 2 or 2 → 1) — measure success-rate impact per task category
* downscale images (384 → 224 or 224 → 160) — usually safe for short-range manipulation, fatal for fine grasping
* shorten the action chunk (50 → 20 steps) — interacts with rung 5; the harness will tell you when this breaks
* drop a modality (proprioception text, force tokens) — almost never safe; revisit only if you have replaced it with a faster encoding

Every one of these requires the harness to re-baseline and re-publish parity numbers against the un-surgery'd policy. There is no other way to know.

---

**11. Reference deployment paths**

Two targets covered explicitly because most readers will choose one of these:

**11.1 Jetson AGX Orin 64 GB (edge)**

* Backbone: AWQ-INT4 (group-128), LM head FP16
* Vision tower: TensorRT FP16 (FP8 if 6.x JetPack with FP8 path is available)
* Action head: FP16
* Runtime: vLLM build for ARM64 + TRT for vision, or llama.cpp CUDA backend if you want a single binary
* Control loop wrapper: ROS 2 node that publishes `JointTrajectory` at 30-50 Hz; policy itself runs at chunk-cache K=5
* Memory budget: ~14 GB policy, ~6 GB perception, ~4 GB ROS 2 + costmap + nav

**11.2 Single-GPU workstation (RTX 6000 Ada / L4 / 4090)**

* Backbone: FP8 (if Ada / Hopper) or AWQ-INT4
* Vision tower: TRT FP8 or torch.compile + flash-attn
* Action head: FP16 / BF16
* Runtime: vLLM, SGLang, or TensorRT-LLM
* Use case: lab robot, teleop assist, sim-in-the-loop development; latency budget is looser (~30 ms) but determinism still matters

---

</details>

## 12. 实验 — 构建一个基线 + 3 个候选方案

评分需要用到实验数据以及 [Lecture 2](Lecture-02.md) 中的精度一致性 harness（agent 运行时框架）。先搭好 harness，再做本实验。

1. **复现已发布的检查点。** 选择 OpenVLA-7B 或 π₀-base。按已发布的评估协议在 LIBERO-Spatial 或 LIBERO-Object 上跑 50 个 episode。记录动作、成功/失败以及每 tick 的 wall-clock。这是你的参考行。
2. **候选方案 A — 免费的收益。** fp16 + static KV + CUDA Graphs + pinned 相机输入。不做量化。用固定 seed 列表重跑同样的 50 个 episode。与参考行做 diff。
3. **候选方案 B — 量化的 backbone。** AWQ-INT4 backbone、TRT FP16 vision tower、fp16 head。同样的 50 个 episode。
4. **候选方案 C — 量化 + chunk 缓存。** 与 B 相同，另加 K=5 chunk cache（若策略支持分块动作）或投机解码（若不支持）。同样的 50 个 episode。
5. **产出精度一致性表。** 每步动作 MSE、每轴最大误差、成功率的精度一致性、P50/P95/P99 控制回路延迟。以 `parity_report.md` 的形式放进你的 repo。

本实验的通过标准：在目标硬件上，至少有一个候选方案跑赢控制回路的截止时间，**并且**留在你在 Lecture 2 中设定的精度一致性容差之内。若一个都达不到，答案要么是蒸馏，要么是重新审视预算 —— 而不是发布。

---

## 自查

1. 你把 LLM backbone 和 vision tower 都量化了，7-token VLA 的 decode（逐 token 生成阶段）延迟从 60 ms 降到 22 ms，但任务成功率从 78% 掉到 41%。你会先排查哪两个阶段，Lecture 2 的 harness 中哪个指标本可以在部署前就抓到这个问题？
2. 有同事提议上 FP8 KV cache，“因为它在我们的 7B chat 模型上管用”。为什么 π₀ flow-matching 策略的精度一致性风险与 OpenVLA-7B 不同，而精度一致性 harness 又具体需要补上什么才能公平地评估它？
3. 你的策略是每 tick 200 ms，控制预算是 50 ms。K=5 的动作分块缓存把你带到 40 ms 的*有效*值。为什么“有效”这两个字在这句话里承担了很重的分量，决定 K=5 对本任务是否真正安全的 harness 实验又是什么？
4. 为什么把 vision tower 用 FP8 做 TRT 通常是安全的，而把 action head 量化到 INT8 却很危险？

---

## 参考文献

* OpenVLA: An Open-Source Vision-Language-Action Model — [项目页](https://openvla.github.io/), [论文](https://arxiv.org/abs/2406.09246)
* π₀ / π0.5 (Physical Intelligence) — [技术报告](https://www.physicalintelligence.company/blog/pi0)
* NVIDIA GR00T N1 / N1.5 — [模型卡](https://huggingface.co/nvidia/GR00T-N1-2B), [Isaac Lab 集成](https://docs.omniverse.nvidia.com/isaacsim/latest/isaac_lab_tutorials/index.html)
* Open X-Embodiment dataset — [项目](https://robotics-transformer-x.github.io/)
* AWQ: Activation-aware Weight Quantization — [论文](https://arxiv.org/abs/2306.00978)
* RT-2 / RT-2-X — [论文](https://arxiv.org/abs/2307.15818)
* LIBERO benchmark — [论文](https://arxiv.org/abs/2306.03310), [代码](https://github.com/Lifelong-Robot-Learning/LIBERO)
* TensorRT for ViTs — [NVIDIA 技术博客](https://developer.nvidia.com/blog/tag/tensorrt/)
* CUDA Graphs for decode — 见 [阶段 5 — CUDA Advanced Optimization, Lecture 01](../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/03-CUDA高级优化/01-CUDA-Graphs.md)

---

## 本专题课程的下一节

* 下一节：[Lecture 2 — 动作精度一致性 harness](Lecture-02.md)
* 返回：[VLA 优化与动作精度一致性 harness — 概述](README.md)


<details>
<summary>English original</summary>

**12. Lab — Build a baseline + 3 candidates**

You will need the lab data and the parity harness from [Lecture 2](Lecture-02.md) to grade your work. Set up the harness first, then do this lab.

1. **Reproduce the published checkpoint.** Pick OpenVLA-7B or π₀-base. Run 50 episodes on LIBERO-Spatial or LIBERO-Object at the published evaluation protocol. Record actions, success/failure, and wall-clock per tick. This is your reference row.
2. **Candidate A — free wins.** fp16 + static KV + CUDA Graphs + pinned camera input. No quantization. Re-run the same 50 episodes with a fixed seed list. Diff against reference.
3. **Candidate B — quantized backbone.** AWQ-INT4 backbone, TRT FP16 vision tower, fp16 head. Same 50 episodes.
4. **Candidate C — quantized + chunk caching.** Same as B, plus K=5 chunk cache (if the policy supports chunked actions) or speculative decoding (if it does not). Same 50 episodes.
5. **Produce the parity table.** Per-step action MSE, per-axis max error, success-rate parity, P50/P95/P99 control-loop latency. This goes in your repo as `parity_report.md`.

Pass criterion for the lab: at least one candidate beats the control-loop deadline on your target hardware **and** stays inside the parity tolerances you set in Lecture 2. If none do, the answer is either to distill or to revisit the budget — not to ship.

---

**Self-check**

1. You quantized the LLM backbone and the vision tower and decode latency on a 7-token VLA dropped from 60 ms to 22 ms, but task success-rate fell from 78% to 41%. Which two stages do you investigate first, and which metric in Lecture 2's harness would have caught this before deployment?
2. A teammate proposes FP8 KV cache "because it worked for our 7B chat model." Why is the parity risk different for a π₀ flow-matching policy than for OpenVLA-7B, and what specifically does the parity harness need to add to evaluate it fairly?
3. You have a 200 ms-per-tick policy and a 50 ms control budget. Action-chunk caching with K=5 gets you to 40 ms *effective*. Why is "effective" doing a lot of work in that sentence, and what is the harness experiment that decides whether K=5 is actually safe for this task?
4. Why is it usually safe to TRT the vision tower in FP8 but dangerous to quantize the action head to INT8?

---

**References**

* OpenVLA: An Open-Source Vision-Language-Action Model — [project page](https://openvla.github.io/), [paper](https://arxiv.org/abs/2406.09246)
* π₀ / π0.5 (Physical Intelligence) — [tech report](https://www.physicalintelligence.company/blog/pi0)
* NVIDIA GR00T N1 / N1.5 — [model card](https://huggingface.co/nvidia/GR00T-N1-2B), [Isaac Lab integration](https://docs.omniverse.nvidia.com/isaacsim/latest/isaac_lab_tutorials/index.html)
* Open X-Embodiment dataset — [project](https://robotics-transformer-x.github.io/)
* AWQ: Activation-aware Weight Quantization — [paper](https://arxiv.org/abs/2306.00978)
* RT-2 / RT-2-X — [paper](https://arxiv.org/abs/2307.15818)
* LIBERO benchmark — [paper](https://arxiv.org/abs/2306.03310), [code](https://github.com/Lifelong-Robot-Learning/LIBERO)
* TensorRT for ViTs — [NVIDIA technical blog](https://developer.nvidia.com/blog/tag/tensorrt/)
* CUDA Graphs for decode — covered in [Phase 5 — CUDA Advanced Optimization, Lecture 01](../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/03-CUDA高级优化/01-CUDA-Graphs.md)

---

**Next in this special course**

* Next: [Lecture 2 — The Action-Parity Harness](Lecture-02.md)
* Back: [VLA Optimization and Action-Parity Harness — Overview](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track D - Robotics/VLA Optimization and Action-Parity Harness/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20D%20-%20Robotics/VLA%20Optimization%20and%20Action-Parity%20Harness/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
