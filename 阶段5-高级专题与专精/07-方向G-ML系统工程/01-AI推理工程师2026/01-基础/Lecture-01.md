---
title: 第 1 部分 · 第 01 讲 — 2026 年推理工程师的心智模型
description: 第 1 部分 · 第 01 讲 — 2026 年推理工程师的心智模型
---

# 第 1 部分 · 第 01 讲 — 2026 年推理工程师的心智模型

## 概览

2026 年的 **AI 推理工程师**既不是模型工程师，也不是提示词工程师，更不是通用型 MLOps 从业者。这个角色比上述任何一种都更窄、更具体：

> **给定一个模型、一个工作负载和一个硬件目标，交付满足延迟与准确率 SLO 的最低成本推理服务配置——并用其他工程师可复现的测量结果来*证明*它。**

本课程的一切都源自这句话。**模型是固定的**（或在项目开始时就已选定）。**工作负载**——chat、agent、batch、embedding——决定了关键指标。**硬件目标**——从单块 Jetson Orin Nano 到 GB200 NVL72——决定了物理上限。这份工作就是其间的配置，并用**可复现的数字**来支撑。

本讲分五个 pass 建立这一心智模型：

1. **四种推理形态**，以及各自优化的目标。
2. 决定一次推理部署好坏的**指标**。
3. **诊断流程**——优秀的工程师如何在动手改动之前弄清系统*为什么*慢。
4. **阅读 model card**，在启动 GPU 之前预测成本。
5. **角色契约**——资深推理工程师被期望交付什么。

到本讲结束时，你应该能够面对一个新的「模型 + 工作负载 + GPU」三元组，在白板上预测出：(a) 哪个指标会成为瓶颈，(b) 你会首先尝试哪个精度下限，(c) 你会在哪个 runtime 上做原型。第 2 部分和第 3 部分则分别用两个具体的模型族来深化这些内容。

---

## 1. 四种推理形态

每一个生产环境的推理工作负载都可归为**四种形态**之一。它们的瓶颈不同，所需的优化也不同。**误判形态**是昂贵的优化工作却带来零指标提升的最常见原因。

### 1.1 Chat

```text
user message → prefill (1–4K tokens) → decode (50–500 tokens) → user
                                       ↑
                                 repeat in a turn loop with growing KV cache
```

特征：

* 长生命周期的会话，KV cache 随轮次增长。
* TTFT（首 token 时延）很重要，因为有人在等着。
* 每轮的墙钟时间由 decode（逐 token 生成阶段）主导（大量小步骤）。
* **Prefix cache 是最大的单项收益**——系统提示词与之前的轮次被复用。
* 批大小偏低到中等（单个副本上 1–32 个并发用户）。

你要优化的：**TTFT、p99 token 间延迟、prefix cache 命中率。**

### 1.2 Agent 循环

```text
prompt + tool catalog → short prefill → short decode (tool call) → tool run → result → repeat
                                        ↑
                                 1–16 token bursts, JSON-structured
```

特征：

* 大量短轮次，每轮的 prefix 都近乎全新（系统提示词 + 工具 + 历史）。
* **decode 受制于结构化格式约束**（JSON、function calling）。
* 工具调用会用非 LLM 的延迟拉长 LLM 步骤之间的对话。
* **工具调用准确率**（见 [BFCL 评估讲义](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md)）与延迟同等重要。

你要优化的：**每轮 TTFT、结构化输出吞吐、所选精度下的工具调用准确率。**

### 1.3 Batch（离线）

```text
N prompts (10K–10M) → maximize tokens/sec/GPU → write results
```

特征：

* 延迟无关紧要，吞吐才重要。
* 大批大小，通常跨多块 GPU 做拆分。
* 若各 prompt 共享结构（RAG（检索增强生成）重编码、带共享指令的分类），prefix cache 收益极大。
* 这正是 **解耦的 prefill（首字前的整段计算）/ decode** 的价值所在（见第 3 部分）。

你要优化的：**tokens/sec/GPU、$/MTok、调度器利用率。**

### 1.4 Embedding / 检索

```text
N input documents → encoder-only forward → vectors out
```

特征：

* 没有自回归；**prefill 就是全部工作**。
* 没有 KV cache。
* 纯 **算力受限的矩阵乘**，非常适合低精度下的张量核心。
* 批大小受 HBM 限制（激活值很大）。

你要优化的：**tokens/sec、峰值 HBM、每批延迟。**

### 1.5 为什么形态很重要

同一个模型跑在同一块 GPU 上，在不同形态下表现会截然不同：

| 形态 | 主导成本 | 收益来源 |
|-------|---------------|-----------|
| Chat | decode 带宽 + KV cache | prefix cache、paged KV、投机 |
| Agent | TTFT + 结构化输出约束 | prefix cache、语法约束 decode、快速工具分发 |
| Batch | 吞吐 | 连续批处理、P/D 解耦、大批大小 |
| Embedding | 张量核心矩阵乘吞吐 | 低精度（FP8/INT8）、打包批 |

如果为一个 chat 产品做 batch 风格的优化（连续批处理、大批大小），你会把 TTFT 拖垮。如果为一个 batch 作业做 chat 风格的优化（小批、prefix cache），你会烧掉 3–5 倍的成本。**先确定形态。**


<details>
<summary>English original</summary>

**Part 1 · Lecture 01 — The 2026 Inference Engineer's Mental Model**

**Overview**

An **AI inference engineer** in 2026 is not a model engineer, not a prompt engineer, and not a generalist MLOps practitioner. The role is narrower and more specific than any of those:

> **Given a model, a workload, and a hardware target, ship the lowest-cost serving configuration that meets the latency and accuracy SLO — and *prove* it with measurements another engineer can reproduce.**

Everything in this course follows from that sentence. The **model is fixed** (or chosen at the top of the project). The **workload** — chat, agent, batch, embedding — sets the metric that matters. The **hardware target** — single Jetson Orin Nano up to GB200 NVL72 — sets the physical ceiling. The job is the configuration in between, defended by **reproducible numbers**.

This lecture builds that mental model in five passes:

1. The **four inference shapes** and what each one optimizes.
2. The **metrics** that decide whether an inference deployment is good.
3. The **diagnostic flow** — how a good engineer figures out *why* a system is slow before changing anything.
4. **Reading a model card** to predict cost before you spin a GPU.
5. The **role contract** — what a senior inference engineer is expected to ship.

By the end you should be able to look at a new model + workload + GPU triple and, on a whiteboard, predict (a) which metric will be the bottleneck, (b) which precision floor you would try first, (c) which runtime you would prototype on. Part 2 and Part 3 then deepen this with two concrete model families each.

---

**1. The four inference shapes**

Every production inference workload reduces to one of **four shapes**. They have different bottlenecks and want different optimizations. **Mis-identifying the shape** is the most common reason expensive optimization work produces zero metric movement.

**1.1 Chat**

```text
user message → prefill (1–4K tokens) → decode (50–500 tokens) → user
                                       ↑
                                 repeat in a turn loop with growing KV cache
```

Characteristics:

* Long-lived sessions, growing KV cache across turns.
* TTFT (time to first token) matters because the human is waiting.
* Decode dominates wall-clock per turn (lots of small steps).
* **Prefix cache is the single biggest win** — the system prompt and prior turns are reused.
* Batch size is low to medium (1–32 concurrent users on a single replica).

What you optimize: **TTFT, p99 inter-token latency, prefix-cache hit rate.**

**1.2 Agent loop**

```text
prompt + tool catalog → short prefill → short decode (tool call) → tool run → result → repeat
                                        ↑
                                 1–16 token bursts, JSON-structured
```

Characteristics:

* Many short turns, each with a fresh-ish prefix (system + tools + history).
* **Decode is dominated by the structural format constraint** (JSON, function calling).
* Tool calls expand the conversation between LLM steps with non-LLM latency.
* **Tool-call accuracy** (see the [BFCL evaluation lecture](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md)) matters as much as latency.

What you optimize: **TTFT per turn, structured-output throughput, tool-call accuracy at the chosen precision.**

**1.3 Batch (offline)**

```text
N prompts (10K–10M) → maximize tokens/sec/GPU → write results
```

Characteristics:

* Latency does not matter; throughput does.
* Large batch sizes, often disaggregated across many GPUs.
* Prefix cache wins enormously if prompts share structure (RAG re-encoding, classification with shared instructions).
* This is where **disaggregated prefill / decode** earns its keep (see Part 3).

What you optimize: **tokens/sec/GPU, $/MTok, scheduler utilization.**

**1.4 Embedding / retrieval**

```text
N input documents → encoder-only forward → vectors out
```

Characteristics:

* No autoregression; **prefill is the whole job**.
* No KV cache.
* Pure **compute-bound matmul**, ideal for tensor cores at low precision.
* Batch size limited by HBM (large activations).

What you optimize: **tokens/sec, peak HBM, latency per batch.**

**1.5 Why the shape matters**

The same model on the same GPU will look different in each shape:

| Shape | Dominant cost | Wins from |
|-------|---------------|-----------|
| Chat | Decode bandwidth + KV cache | Prefix cache, paged KV, speculation |
| Agent | TTFT + structured-output enforcement | Prefix cache, grammar-constrained decode, fast tool dispatch |
| Batch | Throughput | Continuous batching, P/D disaggregation, large batch sizes |
| Embedding | Tensor-core matmul throughput | Low precision (FP8/INT8), packed batches |

If you optimize batch-style (continuous batching, big batch sizes) for a chat product you will tank TTFT. If you optimize chat-style (low batch, prefix cache) for a batch job you will burn 3–5× the cost. **Pick the shape first.**

---

</details>

## 2. 判定工作好坏的指标

资深工程师拿钱是为了优化*正确的指标*，并用*数字为这个选择辩护*。**真正重要的五个**：

### 2.1 TTFT —— 首 token 时延

从请求被接受到第一个解码出的输出 token 到达客户端之间的墙钟时间。

* 对 chat：这是人感知到的「响应速度」。
* 对 agent：它决定了整个工具调用循环的时钟。
* 主要由 prefill（首字前的整段计算）成本 + 排队等待时间 + （对流式响应）第一个 decode step（逐 token 生成阶段）决定。
* 生产中常见的目标：H200 上短 prompt 的 chat <300 ms；<512 token prompt 的快速 agent 循环 <50 ms。

### 2.2 TPOT —— 每输出 token 耗时

第一个 token 之后每个解码 token 的墙钟时间。有时称为 token 间延迟（ITL）。

* 对 chat：它决定流式输出感觉「快」还是「卡顿」。
* 目标：人类阅读速度的 chat 每 token 10–50 ms（与阅读速度匹配）；单次响应很短的 agent 循环 5–15 ms。
* 主要由 **HBM 带宽**决定（decode 是带宽受限）。这就是 Hopper H200（4.8 TB/s HBM3e）在 decode 上以大致等于带宽比的幅度胜过 H100（3.35 TB/s HBM3）的原因。

### 2.3 吞吐 —— tokens / sec / GPU

**成本的分母**。每 GPU 每秒在所有并发请求上产出的输出 token 总数。

* 对批处理：这*就是*优化目标。
* 对 chat + agent：它决定 $/MTok，以及一块 GPU 能服务多少用户。
* 随批大小（直至内存上限）与推测（每步有效 token 数 > 1）而提升。

### 2.4 p50 / p95 / p99 延迟

尾部。**均值是谎言**；尾部才是 SLO。

* 多数生产 SLO 表述为「p95 TTFT < X ms」或「p99 TPOT < Y ms」。
* p50 与 p99 之间的差距说明调度器是否均衡，或者是否有一个卡住的请求拖垮了其余请求。
* 当 p99 爆掉而 p50 保持平稳时，看这些：prefill 突发饿死 decode、KV-cache 驱逐风暴、NCCL all-reduce 掉队者。

### 2.5 $/MTok —— 每百万输出 token 成本

决定产品是否可行的指标。

```text
$/MTok = (replica_$/hour) / (output_tokens/sec/replica) × (10^6 / 3600)
```

* CFO 看得懂的数字。
* 决定你的优化是否真的推动了业务。
* 用 2× H100 替换 1× H200 才换来的 30% 吞吐提升，*不是* $/MTok 上的胜利——重新算一遍账。

### 2.6 你无法造假的指标 —— 准确率精度一致性

本课程的每一项优化（量化、推测、prefix cache、KV 压缩）里都藏着一个精度一致性问题：*模型是否仍然给出相同的答案？*

* 每个工作负载类别使用固定的评测集。chat 用 MMLU 子集 + 一个领域集。agent 用 [BFCL](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md)。代码用 HumanEval / MBPP。长上下文用 RULER 或 needle-in-haystack。
* 在 BFCL 上损失 3 pp 换来的 50% 吞吐收益不是胜利——它带来的是线上事故。
* 这一模式与 [VLA（视觉-语言-动作模型）Action-Parity Harness](../../../04-方向D-机器人/05-VLA优化与动作对齐测试/Lecture-02.md) 相同，只是作用于 LLM：每一项优化都需要一个精度一致性门禁。

---

## 3. 诊断流程

**糟糕的推理工程**先改后测。**优秀的推理工程**遵循这样的循环：

```text
observe ──► hypothesize ──► isolate ──► benchmark ──► profile ──► explain ──► change ──► verify
   ▲                                                                                       │
   └───────────────────────────────────────────────────────────────────────────────────────┘
```

具体来说：

1. **观察出问题的指标。**「decode 慢」不算观察；「p99 TPOT 是 47 ms，目标是 25 ms」才算观察。
2. **假设所处的工作区间。**这是算力受限、带宽受限、调度器受限还是通信受限？Lecture 02 会构建这套判定方法。
3. **隔离。**削减工作负载，直到只剩可疑对象。以 batch=1 运行，无其他流量，固定 seed，固定 prompt。
4. **benchmark。**在至少 50 次迭代加 warmup 的情况下取得稳定基线。报告 p50/p95/p99，而不是均值。
5. **性能分析。**用 Nsight Systems 看时间线，用 Nsight Compute 做 kernel 级分析。如果钻不到 Python 之下，就用 PyTorch profiler。
6. **解释。**用一句话写下你认为正在发生什么。「decode kernel A 在 HBM3e 峰值的 78% 处带宽受限；其余是 launch 开销。」
7. **只改一处。**不是三处。
8. **验证。**重新 benchmark。如果指标没有按假设可预测地变化，说明解释错了——回到第 2 步。

**头号错误**是跳过第 6 步。如果你说不出某件事*为什么*慢，你就说不出该*改什么*。

---


<details>
<summary>English original</summary>

**2. The metrics that decide whether the work was good**

A senior engineer is paid to optimize *the right metric* and to *defend the choice in numbers*. The **five that matter**:

**2.1 TTFT — Time to First Token**

The wall-clock from request acceptance to the first decoded output token reaching the client.

* For chat: this is what the human perceives as "responsiveness."
* For agent: this gates the entire tool-call loop's clock.
* Dominated by prefill cost + queue wait time + (for streamed responses) the first decode step.
* Targets you see in production: <300 ms for chat with short prompts on H200; <50 ms for fast agent loops at <512-token prompt.

**2.2 TPOT — Time Per Output Token**

Wall-clock per decoded token after the first. Sometimes called inter-token latency (ITL).

* For chat: this is what makes streamed output feel "fast" or "stuttery."
* Targets: 10–50 ms per token for human-readable chat (matches reading speed); 5–15 ms for agent loops where a single response is short.
* Dominated by **HBM bandwidth** (decode is bandwidth-bound). This is why Hopper H200 (4.8 TB/s HBM3e) beats H100 (3.35 TB/s HBM3) at decode by roughly the bandwidth ratio.

**2.3 Throughput — tokens / sec / GPU**

The **denominator of cost**. Total output tokens emitted across all concurrent requests per second per GPU.

* For batch: this *is* the optimization target.
* For chat + agent: this is what determines $/MTok and how many users one GPU serves.
* Improves with batch size (up to a memory ceiling) and with speculation (effective tokens-per-step > 1).

**2.4 p50 / p95 / p99 latency**

The tail. The **mean is a lie**; the tail is the SLO.

* Most production SLOs are stated as "p95 TTFT < X ms" or "p99 TPOT < Y ms".
* The gap between p50 and p99 tells you whether the scheduler is well-balanced or whether one stuck request poisons the rest.
* When p99 explodes while p50 stays flat, look at: prefill-bursts starving decode, KV-cache eviction storms, NCCL all-reduce stragglers.

**2.5 $/MTok — cost per million output tokens**

The metric that decides whether the product is viable.

```text
$/MTok = (replica_$/hour) / (output_tokens/sec/replica) × (10^6 / 3600)
```

* The number a CFO understands.
* Determines whether your optimization actually moved the business needle.
* A 30% throughput improvement that requires switching from 1× H200 to 2× H100 is *not* a $/MTok win — re-check the math.

**2.6 The metric you cannot fake — accuracy parity**

Every optimization in this course (quantization, speculation, prefix cache, KV compression) has a parity question hidden in it: *did the model still produce the same answers?*

* Use a fixed eval set per workload class. For chat, MMLU subset + a domain set. For agent, [BFCL](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md). For code, HumanEval / MBPP. For long context, RULER or needle-in-haystack.
* A 50% throughput win that costs 3 pp on BFCL is not a win — it ships incidents.
* The pattern is the same as the [VLA Action-Parity Harness](../../../04-方向D-机器人/05-VLA优化与动作对齐测试/Lecture-02.md), applied to LLMs: every optimization needs a parity gate.

---

**3. The diagnostic flow**

**Bad inference engineering** changes things first and measures after. **Good inference engineering** looks like this loop:

```text
observe ──► hypothesize ──► isolate ──► benchmark ──► profile ──► explain ──► change ──► verify
   ▲                                                                                       │
   └───────────────────────────────────────────────────────────────────────────────────────┘
```

Concretely:

1. **Observe the metric that is bad.** "Decode is slow" is not observed; "p99 TPOT is 47 ms, target is 25 ms" is observed.
2. **Hypothesize the regime.** Is this compute-bound, memory-bound, scheduler-bound, or comm-bound? Lecture 02 builds the test.
3. **Isolate.** Reduce the workload until only the suspect remains. Run at batch=1, no other traffic, fixed seed, fixed prompt.
4. **Benchmark.** Get a stable baseline with at least 50 iterations and warmup. Report p50/p95/p99, not mean.
5. **Profile.** Nsight Systems for the timeline, Nsight Compute for kernel-level. PyTorch profiler if you cannot get below Python.
6. **Explain.** Write down in one sentence what you believe is happening. "Decode kernel A is bandwidth-bound at 78% of HBM3e peak; the rest is launch overhead."
7. **Change one thing.** Not three.
8. **Verify.** Re-bench. If the metric did not move predictably from the hypothesis, the explanation was wrong — back to step 2.

The **number-one mistake** is skipping step 6. If you cannot say *why* something is slow, you cannot say *what* to change.

---

</details>

## 4. 阅读模型卡以预测成本

在启动 GPU 之前，仅凭模型的 `config.json`（或模型卡），就能提取出任何现代模型的**推理成本形态**。**五个字段**几乎完成了全部工作。

| 字段 | 它告诉你什么 |
|-------|-------------------|
| `num_hidden_layers` (L) | Transformer 块总数；几乎使每项成本成倍增加 |
| `hidden_size` (d) | 激活值宽度；决定 FFN GEMM（矩阵-矩阵乘）大小和每 token 的 KV 字节数 |
| `intermediate_size` (d_ff) | FFN 扩展；通常为隐藏维度的 2.5–4×，主导 FLOPs |
| `num_attention_heads` (h_q) 和 `num_key_value_heads` (h_kv) | 分组查询注意力比例；决定 KV cache 大小（h_kv 越小 = KV 越小） |
| `head_dim` | 每头维度；现代模型通常为 128 |

仅凭这些就能计算：

**每 token 的 KV cache 字节数（每请求）：**

```text
kv_bytes_per_token = 2 (K and V) × L × h_kv × head_dim × bytes_per_element
```

对于 FP16 KV 下的 Llama 3.3 70B：2 × 80 × 8 × 128 × 2 = **327 KB/token**。
在 128K 上下文下：327 KB × 128 × 1024 ≈ **42 GB**。每请求。这就是为什么**长上下文推理服务**需要 FP8 KV（减半）或 INT4 KV（降到约 10 GB）。

**每 token 的近似 FLOPs（decode，逐 token 生成阶段）：**

```text
flops_per_token ≈ 2 × P
```

其中 P 是激活参数量（对 dense，= 总参数量；对 MoE（混合专家模型），= total/expert × experts_active）。因子 2 对应乘加运算。

对于 70B dense 模型：decode 时约 140 GFLOPs/token。在 H200（约 990 BF16 TFLOPs 峰值）上，如果能让 kernel 保持峰值，就能做到约 7000 tokens/sec——但做不到，因为 **decode 受带宽限制**，而不是算力受限。

**Decode 带宽上限（batch=1 时的真实上限）：**

```text
decode_tps_ceiling ≈ HBM_bandwidth / (model_size_in_bytes)
```

对于 H200（4.8 TB/s）上 FP16（140 GB）的 Llama 3.3 70B：4800 / 140 ≈ **34 tokens/sec**。H200 上真实的 vLLM 数字在 batch=1 时约为 30 tok/s，这吻合。

在 INT4（35 GB）下：4800 / 35 ≈ **137 tokens/sec 上限**。真实数字：约 110 tok/s。

结论：仅凭模型卡和 HBM 规格，你就能**预测带宽受限的上限**。超过该上限的任何收益都来自**批处理**（在更多 token 间共享权重读取）、**投机**（每次权重读取接受更多 token）或精度下降。

---

## 5. 角色契约——资深 AI 推理工程师交付什么

这一角色的资深工程师会持续交付：

* **可辩护的配置。** 不是“我们用 vLLM，batch=64”，而是“vLLM 0.22 搭配 TP=4、batch=64、prefix cache 开启、AWQ-INT4、FP8 KV，在 MMLU=82.1 上验证了精度一致性，对照 FP16 参考 82.4（Δ=0.3 pp 在预算内）。”
* **可复现的 benchmark。** 一个 harness（agent 运行时框架），另一位工程师克隆后能在同一硬件类别上得到 ±5% 以内相同的数字。
* **瓶颈解释。** 带有主导成本注释的 profile trace。“Decode 占 step 时间的 78%；其中 91% 是 HBM 权重读取。”
* **成本模型。** 所选配置下的 $/MTok，以及在 2× 规模和 10× 规模下会如何变化。
* **精度一致性门禁。** 一个回归测试：如果未来的任何优化使工作负载的指标评测集回退超出预算，就让构建失败。

这一角色的初级工程师交付单个修复。结构性差异在于**可复现性层**。

资深工程师*不*拿钱做的事：选模型。选产品。选 SLO。这些来自上层；工程师为其中可实现的内容辩护。

---

## Lab — 建立你的 benchmark 模板

目标：一个你在第 1–3 部分都会用到的 repo。上限一天。

1. **选一个小模型**（Qwen3-4B Instruct AWQ-INT4 是不错的默认选择——能装进 16 GB GPU）。
2. **选一个 runtime**（推荐 vLLM 0.22+）。
3. **构建一个 benchmark CLI**，它接受 `--prompt-length`、`--output-length`、`--concurrency`、`--iters`、`--warmup`，每次运行输出一行 JSON，包含：硬件（GPU 型号 + 驱动 + CUDA）、软件（runtime 版本）、git commit、p50/p95/p99 首 token 时延（TTFT）、p50/p95/p99 TPOT、吞吐、峰值 HBM。
4. **增加一个精度一致性检查**——针对你关心的工作负载类别（chat / agent / batch / embedding），选一个固定的评测集，并输出单个准确率数字。
5. **接入你的 `$/MTok` 公式**，放在一个 `cost.py` 中，它接受 $/hour 并从 benchmark 输出读取吞吐。

通过标准：你能运行 `bench --model qwen3-4b --shape chat --concurrency 4 --iters 200` 并生成单个 JSON 文件，另一名工程师在同一 GPU 上克隆该 repo 后能在 ±5% 以内复现。

你将用这个 harness 跑遍第 2 和第 3 部分中的每一种模型 + runtime + 硬件组合。

---


<details>
<summary>English original</summary>

**4. Reading a model card to predict cost**

Before spinning up a GPU, you can extract the **inference cost shape** of any modern model from its `config.json` (or model card) alone. **Five fields** do almost all of the work.

| Field | What it tells you |
|-------|-------------------|
| `num_hidden_layers` (L) | Total transformer blocks; multiplies almost every cost |
| `hidden_size` (d) | Width of activations; sets FFN GEMM size and KV bytes per token |
| `intermediate_size` (d_ff) | FFN expansion; usually 2.5–4× hidden, dominates FLOPs |
| `num_attention_heads` (h_q) and `num_key_value_heads` (h_kv) | GQA ratio; sets KV cache size (smaller h_kv = smaller KV) |
| `head_dim` | Per-head dimension; usually 128 in modern models |

From these alone you can compute:

**KV cache bytes per token (per request):**

```text
kv_bytes_per_token = 2 (K and V) × L × h_kv × head_dim × bytes_per_element
```

For Llama 3.3 70B at FP16 KV: 2 × 80 × 8 × 128 × 2 = **327 KB/token**.
At 128K context: 327 KB × 128 × 1024 ≈ **42 GB**. Per request. This is why **long-context serving** needs FP8 KV (cuts it in half) or INT4 KV (cuts it to ~10 GB).

**Approximate FLOPs per token (decode):**

```text
flops_per_token ≈ 2 × P
```

where P is the active parameter count (= total params for dense, = total/expert × experts_active for MoE). The factor of 2 captures multiply + accumulate.

For a 70B dense model: ~140 GFLOPs/token at decode. On H200 (~990 BF16 TFLOPs peak), if you could keep the kernel at peak you would do ~7000 tokens/sec — but you cannot, because **decode is bandwidth-bound**, not compute-bound.

**Decode bandwidth ceiling (the real ceiling at batch=1):**

```text
decode_tps_ceiling ≈ HBM_bandwidth / (model_size_in_bytes)
```

For Llama 3.3 70B at FP16 (140 GB) on H200 (4.8 TB/s): 4800 / 140 ≈ **34 tokens/sec**. Real vLLM numbers on H200 are around 30 tok/s at batch=1, which matches.

At INT4 (35 GB): 4800 / 35 ≈ **137 tokens/sec ceiling**. Real numbers: ~110 tok/s.

The takeaway: you can **predict the bandwidth-bound ceiling** from the model card and the HBM spec alone. Anything you do above that ceiling came from **batching** (sharing the weight read across more tokens), **speculation** (more accepted tokens per weight read), or precision drop.

---

**5. The role contract — what a senior AI inference engineer ships**

A senior in this role ships, recurrently:

* **Defended configurations.** Not "we use vLLM with batch=64" but "vLLM 0.22 with TP=4, batch=64, prefix cache on, AWQ-INT4, FP8 KV, parity verified at MMLU=82.1 vs FP16 reference 82.4 (Δ=0.3 pp within budget)."
* **Reproducible benchmarks.** A harness another engineer clones and gets the same numbers within ±5% on the same hardware class.
* **Bottleneck explanations.** Profile traces with annotated dominant cost. "Decode is 78% of step time; of that, 91% is HBM weight read."
* **Cost models.** $/MTok at the chosen configuration, with what would change at 2× scale and at 10× scale.
* **Parity gates.** A regression test that fails the build if any future optimization regresses the workload's metric eval set beyond budget.

A junior in this role ships individual fixes. The structural difference is the **reproducibility layer**.

What a senior is *not* paid to do: pick the model. Pick the product. Choose the SLO. These come from above; the engineer defends what is achievable inside them.

---

**Lab — set up your benchmark template**

Goal: a repo you will use throughout Parts 1–3. Cap of one day.

1. **Pick one small model** (Qwen3-4B Instruct AWQ-INT4 is a good default — fits on a 16 GB GPU).
2. **Pick one runtime** (vLLM 0.22+ recommended).
3. **Build a benchmark CLI** that takes `--prompt-length`, `--output-length`, `--concurrency`, `--iters`, `--warmup` and emits a JSON line per run with: hardware (GPU model + driver + CUDA), software (runtime version), git commit, p50/p95/p99 TTFT, p50/p95/p99 TPOT, throughput, peak HBM.
4. **Add a parity check** — for the workload class you care about (chat / agent / batch / embedding), pick one fixed eval set and emit a single accuracy number.
5. **Wire to your `$/MTok` formula** in a `cost.py` that takes a $/hour and reads throughput from the benchmark output.

Pass criterion: you can run `bench --model qwen3-4b --shape chat --concurrency 4 --iters 200` and produce a single JSON file that another engineer on the same GPU could reproduce within ±5% by cloning the repo.

You will run this harness against every model + runtime + hardware combination in Parts 2 and 3.

---

</details>

## 自查

1. 你在 H100 80GB 上部署一个客服聊天产品，产品团队要求「平均响应时间 < 1 秒」。你实际需要承诺的是哪两个 TTFT 与 TPOT 形态的指标，正确的分位数又是多少？
2. 同事提议，对 concurrency=8 的聊天工作负载，把连续批处理改为分离式 prefill（首字前的整段计算）/decode（逐 token 生成阶段）。在不实际运行的情况下：预测 $/MTok 是否会改善。为什么？
3. 给定一个具有 `num_hidden_layers=80`、`num_key_value_heads=8`、`head_dim=128` 和 FP16 KV 的模型，单个请求在 32K token 下的 KV cache 开销（以 MB 计）是多少？在 256K 下呢？
4. 你把 70B 模型的权重从 FP16 改为 FP8，TPOT 从 38 ms 改善到 22 ms。团队想直接上线。在同意之前，你要求给出哪一个数字？
5. 在 L40S 上，使用 INT4 权重、batch=1 时 decode 的 TPOT 为 60 ms。模型卡给出的预测带宽上限是 18 ms。为解释这一差距，你会按顺序检查哪三件事？

---

## 参考文献

* vLLM 文档 — [docs.vllm.ai](https://docs.vllm.ai/)
* SGLang 文档 — [sgl-project.github.io](https://sgl-project.github.io/)
* NVIDIA 推理调优指南 — [docs.nvidia.com/deeplearning/tensorrt/](https://docs.nvidia.com/deeplearning/tensorrt-llm/)
* 阅读清单（本讲的配套材料）：
  * "Efficient Memory Management for Large Language Model Serving with PagedAttention" — [arXiv:2309.06180](https://arxiv.org/abs/2309.06180)
  * "SGLang: Efficient Execution of Structured Language Model Programs" — [arXiv:2312.07104](https://arxiv.org/abs/2312.07104)
  * "DistServe: Disaggregating Prefill and Decoding for Goodput-optimized Large Language Model Serving" — [arXiv:2401.09670](https://arxiv.org/abs/2401.09670)

本路线图中的交叉引用：

* [阶段 5 → 边缘 AI → 基于 BFCL 的 agent 工具分派评估](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md) — agent 形态的精度一致性纪律
* [阶段 5 → 机器人 → VLA（视觉-语言-动作模型）动作精度一致性 harness（agent 运行时框架）](../../../04-方向D-机器人/05-VLA优化与动作对齐测试/Lecture-02.md) — 同一套门控纪律应用于具身策略
* [阶段 5 → ML 系统工程 → 阶段 0 度量纪律](../../Guide.md) — benchmark harness 模式

---

## 截至 2026-06 有效

锁定版本：vLLM 0.22.x（V1 engine）、SGLang 0.5.x、TensorRT-LLM 1.3.x、llama.cpp 2026-04 之后的版本、H100 / H200 / B200 硬件。当 V1 稳定，或某个 runtime 发布破坏性 API 变更时更新。

---

## 后续

* 下一节：[第 02 讲 — Transformer 执行，从 token 到比特](Lecture-02.md)
* 上一级：[第 1 部分 — 基础](README.md)


<details>
<summary>English original</summary>

**Self-check**

1. You are deploying a customer-support chat product on H100 80GB and the product team wants "average response time < 1 second." What two TTFT-and-TPOT-shaped metrics do you actually need to commit to, and what is the right percentile?
2. A teammate proposes switching from continuous batching to disaggregated prefill/decode for a chat workload at concurrency=8. Without running it: predict whether $/MTok will improve. Why?
3. Given a model with `num_hidden_layers=80`, `num_key_value_heads=8`, `head_dim=128`, and FP16 KV, what is the KV cache cost (in MB) for one request at 32K tokens? At 256K?
4. You change FP16 → FP8 weights on a 70B model and TPOT improves from 38 ms to 22 ms. The team wants to ship. What single number do you require before agreeing?
5. Decode TPOT on an L40S is 60 ms at batch=1 with INT4 weights. Predicted bandwidth ceiling from the model card was 18 ms. What three things would you check, in order, to explain the gap?

---

**References**

* vLLM documentation — [docs.vllm.ai](https://docs.vllm.ai/)
* SGLang documentation — [sgl-project.github.io](https://sgl-project.github.io/)
* NVIDIA Inference Tuning Guide — [docs.nvidia.com/deeplearning/tensorrt/](https://docs.nvidia.com/deeplearning/tensorrt-llm/)
* Reading list (companion to this lecture):
  * "Efficient Memory Management for Large Language Model Serving with PagedAttention" — [arXiv:2309.06180](https://arxiv.org/abs/2309.06180)
  * "SGLang: Efficient Execution of Structured Language Model Programs" — [arXiv:2312.07104](https://arxiv.org/abs/2312.07104)
  * "DistServe: Disaggregating Prefill and Decoding for Goodput-optimized Large Language Model Serving" — [arXiv:2401.09670](https://arxiv.org/abs/2401.09670)

Cross-references in this roadmap:

* [Phase 5 → Edge AI → Agent Tool-Dispatch Evaluation with BFCL](../../../03-方向C-边缘AI/02-智能体工具调度评估BFCL/Lecture-01.md) — agent-shape parity discipline
* [Phase 5 → Robotics → VLA Action-Parity Harness](../../../04-方向D-机器人/05-VLA优化与动作对齐测试/Lecture-02.md) — the same gating discipline applied to embodied policies
* [Phase 5 → ML Systems Engineering → Stage 0 Measurement Discipline](../../Guide.md) — the benchmark harness pattern

---

**Current as of 2026-06**

Pinned: vLLM 0.22.x (V1 engine), SGLang 0.5.x, TensorRT-LLM 1.3.x, llama.cpp post-2026-04, H100 / H200 / B200 hardware. Update when V1 stabilizes or a runtime ships a breaking API change.

---

**Next**

* Next: [Lecture 02 — Transformer execution, from tokens to bits](Lecture-02.md)
* Up: [Part 1 — Fundamentals](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 1 - Fundamentals/Lecture-01.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%201%20-%20Fundamentals/Lecture-01.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
