---
title: 第 5 讲：跨模型策略与生产推理服务——把 Qwen3-4B 与 Qwen2.5-72B 串在一起
description: 第 5 讲：跨模型策略与生产推理服务——把 Qwen3-4B 与 Qwen2.5-72B 串在一起
---

# 第 5 讲：跨模型策略与生产推理服务——把 Qwen3-4B 与 Qwen2.5-72B 串在一起

## 概述

第 3 讲和第 4 讲把边缘模型（Qwen3-4B-Q4）和数据中心模型（Qwen2.5-72B-FP16）当作两个彼此独立的世界。它们并不是。在生产中二者共存——通常是因为同一个产品既要边缘延迟，*又*要服务器级质量。本讲介绍横跨两者的架构：

* **用 Qwen3-4B 作草稿、面向 Qwen2.5-72B 的投机解码**——最直接的跨模型用法。
* **边缘↔云布线**——何时把哪些查询升级，以及该决策所需的可观测性。
* **级联推理**——小模型处理 80% 的流量，大模型处理其余部分。
* **生产可观测性与容量规划**——测什么、如何定容量、工作负载在哪里崩掉。

这是系统工程的一讲。kernel 数学更少，仪表盘和 on-call 决策更多。

到本讲结束时，你应该能够：

* 给定一对草稿/目标模型，计算投机解码带来的预期墙钟时间收益。
* 在成本约束下设计满足 p95 延迟预算的边缘↔云布线策略。
* 挑选能捕获最常见失效模式的生产可观测性指标。
* 针对给定用户负载，为 Qwen2.5-72B 推理服务集群定容量。

---

## 1. 投机解码：为 72B 目标模型配 4B 草稿

基本思路在第 3 讲已勾勒过。这里针对 Qwen 这一对模型具体展开。

### 1.1 数学

设 `t_draft` = 在草稿模型上 decode（逐 token 生成阶段）一个 token 的墙钟时间，`t_target_K` = 目标模型验证一个长度为 K 的 token 窗口的墙钟时间，`α` = 平均接受率（目标模型本会产出的草稿 token 所占比例）。

有效 tok/s：

```
tok_per_step = 1 + α + α² + … + α^(K-1) + α^K       (geometric + one free)
             = (1 - α^(K+1)) / (1 - α)              for K finite
             ≈ 1 / (1 - α)                           for K → ∞

wall_per_step = t_draft × K + t_target_K

tok_per_sec  = tok_per_step / wall_per_step
```

以 Qwen3-4B-Q4_K_M 为草稿、Qwen2.5-72B-FP16 为目标，在 4×H100 上：

```
t_draft (one Qwen3-4B token on a single H100 hosting draft) ≈ 5 ms
t_target_K (Qwen2.5-72B forward pass with seq_len=K)         ≈ 30 ms + 0.5 ms × K
α (Qwen3 → Qwen2.5 same-family acceptance)                   ≈ 0.55
```

当 K=5 时：

```
wall = 5 × 5 ms + 30 + 2.5 ms = 57.5 ms
tok_per_step = (1 - 0.55^6) / (1 - 0.55) = 2.18
tok/s = 2.18 / 0.0575 ≈ 38 tok/s
```

对比仅用基线目标模型：

```
30 ms per token → 33 tok/s
```

在此 α 和 K 下，**投机解码赢约 15%**。要拿到更大的收益，需要更高的 α（更好的草稿—目标对齐）或更低的草稿成本。

### 1.2 为什么 Qwen3-Qwen2.5 这一对很别扭

跨家族接受率中等（约 50–60%），因为：
- 不同的后训练（Qwen3 内嵌了 thinking 模式）。
- 不同的词表（151 936 对 152 064）——**光是 tokenizer 不匹配**就使朴素投机解码不成立。你需要词表投影，或者共享 tokenizer。

**实用 recipe：**用 **Qwen2.5-0.5B-Instruct** 或 **Qwen2.5-1.5B-Instruct** 作为 Qwen2.5-72B 的草稿模型。同家族、同 tokenizer，接受率约 0.7。这是生产中的标准搭配。

用 Qwen3-4B 配 Qwen2.5-72B 这一对**只**在你不关心跨家族质量时才有意思——也就是说，你把 4B 当作"意图大致相同的小型蒸馏版本"。算不上干净的收益。

### 1.3 EAGLE 与 Medusa——内联替代方案

双模型投机解码有**内存开销**（要加载两个模型）。**内联投机解码**变体可避免这一点：

* **Medusa** 给目标模型增添额外的解码头。每个头预测一个不同的未来 token 位置；你同时验证所有位置。
* **EAGLE / EAGLE-2** 在目标模型冻结的激活值之上训练一个小的自回归草稿头。内存比完整的草稿模型少，α 比朴素头更高。

在生产中，面向 Qwen2.5-72B 的 EAGLE-2 已广泛落地，在 α ≈ 0.8 时相对基线给出 1.6–2.0× 的加速——明显优于双模型投机解码。

---

## 2. 边缘↔云布线

产品层面的问题是：一个用户查询到来，是用 Qwen3-4B **在端侧**作答，还是**升级到云端**的 Qwen2.5-72B？

### 2.1 布线信号

| 信号 | 发往 72B 的条件 | 理由 |
|---|---|---|
| Prompt token 数 | > 8 k | 边缘模型的上下文处理退化更快 |
| 检测到的语言 | 不在边缘模型训练的前 3 名内 | 边缘模型最先牺牲多语言质量 |
| 任务类别（分类器） | 代码生成、数学推理、多步规划 | 这些正是 4B → 72B 差距最大之处 |
| 对话历史深度 | > N 轮 | 长会话的一致性更利于更大的模型 |
| 用户层级 | 高级 | 基于成本 |
| 用户明确请求 | "be thorough" / "think carefully" | 直接信号 |
| 网络可用性 | 云端可达 + 延迟预算允许 | 混合系统必须能处理离线 |


<details>
<summary>English original</summary>

**Lecture 5: Cross-Model Strategies and Production Serving — Tying Qwen3-4B and Qwen2.5-72B Together**

**Overview**

Lectures 3 and 4 treated the edge model (Qwen3-4B-Q4) and the datacenter model (Qwen2.5-72B-FP16) as separate worlds. They aren't. In production they coexist — usually because the same product wants edge latency *and* server-class quality. This lecture covers the architectures that span both:

* **Speculative decoding with Qwen3-4B drafting for Qwen2.5-72B** — the most direct cross-model use.
* **Edge↔cloud routing** — when to escalate which queries, observability needed for that decision.
* **Cascaded inference** — small model handles 80% of traffic, large model handles the rest.
* **Production observability and capacity planning** — what to measure, how to size, where workloads break.

This is the system-engineering lecture. Less kernel math, more dashboards and on-call decisions.

By the end you should be able to:

* Compute the expected wall-clock win from speculative decoding given a draft/target pair.
* Design an edge↔cloud routing policy that meets a p95 latency budget under cost constraint.
* Pick the production observability metrics that catch the most common failure modes.
* Size a Qwen2.5-72B serving fleet for a given user load.

---

**1. Speculative Decoding: 4B Draft for 72B Target**

The basic idea was sketched in Lecture 3. Here we make it concrete for the Qwen pair.

**1.1 The math**

Let `t_draft` = wall time to decode one token on the draft, `t_target_K` = wall time for the target to verify a window of K tokens, `α` = average acceptance rate (fraction of drafted tokens the target would have produced).

Effective tok/s:

```
tok_per_step = 1 + α + α² + … + α^(K-1) + α^K       (geometric + one free)
             = (1 - α^(K+1)) / (1 - α)              for K finite
             ≈ 1 / (1 - α)                           for K → ∞

wall_per_step = t_draft × K + t_target_K

tok_per_sec  = tok_per_step / wall_per_step
```

For Qwen3-4B-Q4_K_M as draft and Qwen2.5-72B-FP16 as target on 4×H100:

```
t_draft (one Qwen3-4B token on a single H100 hosting draft) ≈ 5 ms
t_target_K (Qwen2.5-72B forward pass with seq_len=K)         ≈ 30 ms + 0.5 ms × K
α (Qwen3 → Qwen2.5 same-family acceptance)                   ≈ 0.55
```

With K=5:

```
wall = 5 × 5 ms + 30 + 2.5 ms = 57.5 ms
tok_per_step = (1 - 0.55^6) / (1 - 0.55) = 2.18
tok/s = 2.18 / 0.0575 ≈ 38 tok/s
```

vs. baseline target alone:

```
30 ms per token → 33 tok/s
```

**Speculative dec wins ~15%** at this α and K. To get bigger wins you need higher α (better draft–target alignment) or lower draft cost.

**1.2 Why the Qwen3-Qwen2.5 pair is awkward**

The cross-family acceptance rate is moderate (~50–60%) because:
- Different post-training (Qwen3 has thinking mode embedded).
- Different vocabularies (151 936 vs 152 064) — the **tokenizer mismatch alone** disqualifies vanilla speculative decoding. You need a vocab projection or to share tokenizers.

**Practical recipe:** use **Qwen2.5-0.5B-Instruct** or **Qwen2.5-1.5B-Instruct** as the draft for Qwen2.5-72B. Same family, same tokenizer, ~0.7 acceptance rate. This is the standard production pairing.

The Qwen3-4B for Qwen2.5-72B pair is interesting **only** if you don't care about cross-family quality — i.e. you treat the 4B as a "small distilled version of approximately the same intent." Not a clean win.

**1.3 EAGLE and Medusa — inline alternatives**

Two-model speculative decoding has **memory overhead** (two models loaded). **Inline-speculative-decoding** variants avoid this:

* **Medusa** adds extra decoding heads to the target model. Each head predicts a different future token position; you verify all simultaneously.
* **EAGLE / EAGLE-2** trains a small autoregressive draft head on top of the target's frozen activations. Less memory than a full draft model, higher α than naïve heads.

For Qwen2.5-72B in production, EAGLE-2 has shipped widely and gives 1.6–2.0× over baseline at α ≈ 0.8 — substantially better than two-model spec dec.

---

**2. Edge↔Cloud Routing**

The product question: a user query arrives, do we answer **on-device** with Qwen3-4B or **escalate to cloud** Qwen2.5-72B?

**2.1 Routing signals**

| Signal | Send to 72B if | Reasoning |
|---|---|---|
| Prompt token count | > 8 k | Edge model context handling degrades faster |
| Detected language | Not in top-3 of edge model's training | Edge models drop multilingual quality first |
| Task category (classifier) | Code generation, math reasoning, multi-step planning | These are where 4B → 72B gap is largest |
| Conversation history depth | > N turns | Coherence over long sessions favors larger model |
| User tier | Premium | Cost-based |
| Explicit user request | "be thorough" / "think carefully" | Direct signal |
| Network availability | Cloud reachable + latency budget allows | Hybrid systems must handle offline |

</details>

### 2.2 两阶段决策

```
                     User query
                          │
              ┌───────────┴──────────┐
              ▼                       ▼
    fast classifier (50 ms)     simple length/lang check
              │                       │
              └───────────┬──────────┘
                          ▼
                  Route decision
                          │
              ┌───────────┴──────────┐
              ▼                       ▼
     Run on Qwen3-4B (Edge)   Run on Qwen2.5-72B (Cloud)
              │                       │
              ▼                       ▼
     If confidence < τ →   ──┐    Return answer
     escalate to 72B         │
                             ▼
                        Cloud call
```

“先自评、再升级”这条路径很强大，但会增加延迟。要克制使用——只用于“仅校验高风险输出”的策略。

### 2.3 成本与延迟曲线

一个简单的生产数据点（数字取自 2026 年中的典型部署）：

```
Pure 72B:   $0.50/M tokens   |  p50 = 600 ms TTFT, p95 = 2.5 s
Pure  4B:   $0.02/M tokens   |  p50 = 100 ms TTFT, p95 = 800 ms (on-device)
70/30 mix:  $0.16/M tokens   |  p50 = 250 ms TTFT, p95 = 2.0 s
```

70/30 的混合通常是正确的起点。根据 dogfood 中的质量投诉，再向 90/10 或 50/50 调整。

---

## 3. 生产环境可观测性

Qwen 推理服务的仪表盘上该放什么，按优先级排序：

### 3.1 延迟

* **TTFT（首 token 时延）** — p50、p95、p99。用户最能直接感知的指标。
* **ITL（token 间延迟）** — p50、p95。首 token 之后，后续 token 之间的间隔时长。
* **请求总耗时** — 仅为完整起见。

### 3.2 吞吐

* **每 GPU 每秒 decode 的 token 数**（decode：逐 token 生成阶段） — 饱和度指标。
* **整个集群每秒生成的 token 数** — 容量。
* **每 GPU 每秒 prefill 的 token 数**（prefill：首字前的整段计算） — 单列指标，因为路径不同。

### 3.3 利用率

* **GPU 计算利用率**（`nvidia-smi`） — 负载下应 ≥ 70%。
* **GPU 显存利用率**（占用的 HBM 百分比） — 权重 + KV 合计应达 85–92%。
* **KV cache occupancy**（vLLM 会暴露该指标） — 越高 = 连续批处理效率越好。
* **NCCL 集合通信时间**（占 step 时间的百分比） — > 20% 意味着要重新平衡 TP 或检查 fabric。

### 3.4 质量

* **采样拒绝率**（若使用 guided decoding） — 过高 = 模型在与你的 schema 对抗。
* **按响应长度的 EOS 率** — 突然下降说明模型无法正常终止。
* **拒答率** — 对经过安全微调的模型，该指标漂移意味着存在 bug 或发生了非预期的重训练。

### 3.5 成本

* **$ per 1k input tokens** and **$/1k 输出 token** — 你真实的成本账。
* **GPU 小时空闲时间** — 容量超配。

### 3.6 中等负载下，Qwen2.5-72B 在 4×H100 上“好”是什么样

| 指标 | 健康值 |
|---|---|
| TTFT p50 | 200–400 ms |
| TTFT p95 | < 2 s |
| ITL p50 | 25–40 ms |
| ITL p95 | < 80 ms |
| GPU 利用率 | 70–85% |
| HBM 利用率 | 85–92% |
| KV occupancy | 75–95% |
| NCCL 占比 | 5–10% |

---

## 4. 生产环境常见病症

一份简短的现场指南：“你会看到的现象，以及它们的含义”：

### 4.1 边缘

| 症状 | 原因 | 修复 |
|---|---|---|
| decode 只有 0.2 tok/s，GPU @ 0 MHz | DVFS 停留在低频档 | `nvpmodel -m 0; jetson_clocks` |
| 设备从挂起唤醒后 tok/s 掉 50% | 时钟被挂起流程解锁 | 在 resume hook 中重新运行 `jetson_clocks` |
| 首个 decode token 慢，其余很快 | CUDA Graph 尚未捕获 | 显式预热 |
| 8GB Orin Nano 上长 prompt 触发 OOM | KV cache 触顶 | 调低 `n_ctx`、改用 INT8 KV、缩小 batch |
| 超过第 10 个 token 就是乱码 | RoPE 变体选错（vanilla vs NeoX） | 核对 rope_kernel layout |
| 对无害 prompt 也拒答 | chat template 错误 | 用 tokenizer.apply_chat_template() 核对 |

### 4.2 数据中心

| 症状 | 原因 | 修复 |
|---|---|---|
| 比公开的 vLLM benchmark 慢 5× | NCCL 跑在 PCIe 而非 NVLink 上 | `NCCL_P2P_LEVEL=NVL` + 检查 `nvidia-smi topo -m` |
| TTFT 在随机时段出现尖峰 | 长 prompt 未分块 | `--enable-chunked-prefill` |
| 高并发下吞吐崩塌 | KV cache 碎片化 | 改用支持 prefix caching 的较新 vLLM；调高 `--gpu-memory-utilization` |
| 饱和时 GPU 利用率仅 50% | 静态批处理，不是连续批处理 | 确认 runtime 版本支持连续批处理 |
| 各副本答案不一致 | RoPE 或 YaRN 配置不同 | 固定 runtime 容器版本与配置 |
| batch=16、32 k 上下文时 OOM | KV block 耗尽 | 调小 batch 或缩短 `--max-model-len` |
| p99 延迟突然飙升 | 某条序列跑到了 `--max-model-len` | 限制输出长度，杀掉失控的生成 |

---


<details>
<summary>English original</summary>

**2.2 A two-stage decision**

```
                     User query
                          │
              ┌───────────┴──────────┐
              ▼                       ▼
    fast classifier (50 ms)     simple length/lang check
              │                       │
              └───────────┬──────────┘
                          ▼
                  Route decision
                          │
              ┌───────────┴──────────┐
              ▼                       ▼
     Run on Qwen3-4B (Edge)   Run on Qwen2.5-72B (Cloud)
              │                       │
              ▼                       ▼
     If confidence < τ →   ──┐    Return answer
     escalate to 72B         │
                             ▼
                        Cloud call
```

The "self-evaluate then escalate" path is powerful but adds latency. Use it sparingly — for a "verify high-stakes outputs only" policy.

**2.3 Cost vs latency curve**

A simple production data point (numbers from typical mid-2026 deployments):

```
Pure 72B:   $0.50/M tokens   |  p50 = 600 ms TTFT, p95 = 2.5 s
Pure  4B:   $0.02/M tokens   |  p50 = 100 ms TTFT, p95 = 800 ms (on-device)
70/30 mix:  $0.16/M tokens   |  p50 = 250 ms TTFT, p95 = 2.0 s
```

The 70/30 mix is usually the right starting point. Tune toward 90/10 or 50/50 based on quality complaints in dogfood.

---

**3. Production Observability**

What to put on a dashboard for Qwen serving, in priority order:

**3.1 Latency**

* **TTFT (time-to-first-token)** — p50, p95, p99. The most user-visible metric.
* **ITL (inter-token latency)** — p50, p95. After first token, how long between subsequent tokens.
* **Total request time** — for completeness.

**3.2 Throughput**

* **Decoded tokens per second per GPU** — saturation indicator.
* **Generated tokens per second cluster-wide** — capacity.
* **Prefill tokens per second per GPU** — separate metric because the path is different.

**3.3 Utilization**

* **GPU compute utilization** (`nvidia-smi`) — should be ≥ 70% under load.
* **GPU memory utilization** (% of HBM in use) — should be 85–92% on weights + KV combined.
* **KV cache occupancy** (vLLM exposes this) — higher = better continuous-batching efficiency.
* **NCCL collective time** (% of step time spent in NCCL) — > 20% means rebalance TP or check fabric.

**3.4 Quality**

* **Sampling rejection rate** (if using guided decoding) — too high = model fighting your schema.
* **EOS rate per response length** — sudden drops suggest the model is failing to terminate.
* **Refusal rate** — for safety-tuned models, drift in this metric indicates a bug or unintended retraining.

**3.5 Cost**

* **$ per 1k input tokens** and **$ per 1k output tokens** — your real economics.
* **GPU-hour idle time** — capacity overprovisioning.

**3.6 What "good" looks like for Qwen2.5-72B on 4×H100 at moderate load**

| Metric | Healthy |
|---|---|
| TTFT p50 | 200–400 ms |
| TTFT p95 | < 2 s |
| ITL p50 | 25–40 ms |
| ITL p95 | < 80 ms |
| GPU util | 70–85% |
| HBM util | 85–92% |
| KV occupancy | 75–95% |
| NCCL fraction | 5–10% |

---

**4. Common Production Pathologies**

A short field guide of "things you'll see, things they mean":

**4.1 Edge**

| Symptom | Cause | Fix |
|---|---|---|
| Decode at 0.2 tok/s, GPU @ 0 MHz | DVFS parked | `nvpmodel -m 0; jetson_clocks` |
| Tok/s drops 50% after device wakes from suspend | Clocks unlocked by suspend | Re-run `jetson_clocks` in resume hook |
| First-decode token slow, rest fast | CUDA Graph not captured yet | Warm up explicitly |
| OOM at long prompts on 8GB Orin Nano | KV cache hitting limit | Lower `n_ctx`, INT8 KV, smaller batch |
| Gibberish past token 10 | RoPE flavor wrong (vanilla vs NeoX) | Verify rope_kernel layout |
| Refuses on benign prompts | Wrong chat template | Verify against tokenizer.apply_chat_template() |

**4.2 Datacenter**

| Symptom | Cause | Fix |
|---|---|---|
| 5× slower than published vLLM benchmarks | NCCL on PCIe instead of NVLink | `NCCL_P2P_LEVEL=NVL` + check `nvidia-smi topo -m` |
| TTFT spike at random intervals | Long prompts not chunked | `--enable-chunked-prefill` |
| Throughput collapses under high concurrency | KV cache fragmentation | Newer vLLM with prefix-caching; raise `--gpu-memory-utilization` |
| 50% GPU util at saturation | Static batching, not continuous | Confirm runtime version supports it |
| Inconsistent answers across replicas | Different RoPE or YaRN config | Pin runtime container version and config |
| OOM at 32 k context with batch=16 | KV blocks exhausted | Lower batch or shorten `--max-model-len` |
| Sudden p99 latency spike | One sequence ran to `--max-model-len` | Cap output length, kill runaway generations |

---

</details>

## 5. 容量规划示例

目标：为一个具备以下特征的聊天产品提供推理服务：

* 1000 日活用户。
* 平均 50 轮/用户/天。
* 平均输入 300 tokens，输出 400 tokens。
* 70% 的查询走边缘侧 Qwen3-4B（免费），30% 走云端 Qwen2.5-72B。

云端负载：

```
Queries per day: 1000 × 50 × 0.30           = 15 000
Tokens per query in/out: 300 / 400
Output tokens per day: 15 000 × 400         = 6 000 000
Output tok/s averaged over 24h:             ≈ 70 tok/s
```

峰值系数（典型聊天流量在高峰小时约为平均值的 5 倍）：

```
Peak output tok/s: 70 × 5 ≈ 350 tok/s
```

Per Lecture 4：Qwen2.5-72B 跑在 4×H100 上，单流饱和时持续约 250 tok/s，batch=32 时约 560 tok/s。采用连续批处理、并发适中时，单台 4-H100 机器约 400 tok/s 是安全的规划数值。

**峰值时你需要一台 4-H100 机器**，再备一台作为 failover/突发容量。总 capex：~$200k for 8 × H100 SXM + servers (2026 pricing), or ~$25/hour × 24 × 365 × 2 = ~$440k/年云租用费用。

按摊销成本计算的每 1k tokens 成本：约 $0.20-0.40 —— 与本文写作时的 OpenAI API 相比有竞争力。

---

## 6. 混合架构的失效模式

边缘↔云端的拆分有其自身的病症：

1. **边缘侧 fail open** —— 云端不可达时，是返回（较差的）边缘响应，还是显式失败？两者都成立，但必须做出决定并加埋点。

2. **云端始终更好 —— 用户学会跳过边缘侧。** 如果你的 “be more thorough” 按钮总能产出更好的结果，用户就会对什么都点它，你的成本模型随之崩溃。解法：把边缘模型做得足够好，使大多数查询上的差异足够小。

3. **边缘更新之间的质量漂移。** 边缘模型随 app 一起发布，更新很慢；云端模型可以每天迭代。最终两者发生漂移；使用陈旧 app 构建的用户体验变差。增加服务端分析，按产出模型为响应打标签，并观察质量差值。

4. **隐私边界不匹配。** 边缘查询不离开设备；云端查询会离开。用户会假设边界落在“显而易见的那一处” —— 通常他们假设边缘模型处理“私密”查询。要明确哪些查询走哪里。

---

## 7. 展望 —— 到 2026 年末会发生什么变化

截至 2026 年 5 月，文献中可见的部分进展：

* **MoE 级 Qwen** —— 一个 “Qwen3-30B-A3B” MoE（混合专家模型）版本。总参数 30 B，每个 token 激活约 3 B。推理经济性截然不同 —— 计算量更接近 4B，质量更接近 30B。值得跟踪。

* **投机解码变得更容易** —— EAGLE-3 降低了 draft head 的训练成本，将各模型族的接受率推高到约 0.85。

* **超越 INT4 的 KV cache 压缩** —— H2O、StreamingLLM 式 sink、KIVI 等方法已从研究走向生产。预计长上下文 decode（逐 token 生成阶段）的带宽会再减半。

* **边缘芯片追赶上来** —— Apple M5 系列与 Qualcomm Snapdragon X75/X80 的后续产品把 4B 级模型的边缘吞吐推过 100 tok/s，侵蚀了云端独占的质量优势。

* **标准化的 agent harness（agent 运行时框架）** —— 模型推理服务栈逐渐内置对 tool-calling、流式部分响应与结构化输出的一等原语支持（vLLM 有 `--tool-call-parser`，SGLang 有编译期结构化生成）。

系统工程本身没有根本变化。roofline（性能上界模型）数学里的常数变好了，模型在相同参数量下更聪明了，而我们讨论过的生产可观测性仍然是该测量的那些东西。

---


<details>
<summary>English original</summary>

**5. Capacity Planning Example**

Goal: serve a chat product with these characteristics:

* 1000 daily active users.
* Average 50 turns/user/day.
* Average input 300 tokens, output 400 tokens.
* 70% of queries on Qwen3-4B edge (free), 30% on Qwen2.5-72B cloud.

Cloud load:

```
Queries per day: 1000 × 50 × 0.30           = 15 000
Tokens per query in/out: 300 / 400
Output tokens per day: 15 000 × 400         = 6 000 000
Output tok/s averaged over 24h:             ≈ 70 tok/s
```

Peak factor (typical chat traffic ~5× average during peak hour):

```
Peak output tok/s: 70 × 5 ≈ 350 tok/s
```

Per Lecture 4: Qwen2.5-72B on 4×H100 sustains ~250 tok/s single-stream-saturated and ~560 tok/s at batch=32. With continuous batching at moderate concurrency, ~400 tok/s is a safe planning number for one 4-H100 box.

**You need one 4-H100 box at peak**, with a second as failover/burst capacity. Total capex: ~$200k for 8 × H100 SXM + servers (2026 pricing), or ~$25/hour × 24 × 365 × 2 = ~$440k/year cloud rental.

Per-1k-tokens cost at amortized cost: ~$0.20-0.40 — competitive with the OpenAI API at the time of this writing.

---

**6. Hybrid Failure Modes**

The edge↔cloud split has its own pathologies:

1. **Edge fails open** — when the cloud is unreachable, do you serve the (worse) edge response, or fail explicitly? Both are valid, but you must decide and instrument.

2. **Cloud is consistently better — users learn to skip the edge.** If your "be more thorough" button always produces better output, users tap it for everything, and your cost model breaks. Solution: make the edge model good enough that the difference is small for most queries.

3. **Quality drift between edge updates.** Your edge model ships with the app. Updating it is slow. Your cloud model can move daily. Eventually they drift; users on stale app builds get worse experiences. Add server-side analytics that tag responses by which model produced them, and watch the quality delta.

4. **Privacy boundary mismatch.** Edge queries don't leave the device; cloud queries do. Users will assume the boundary is at "the obvious one" — usually they assume the edge model handles "private" queries. Be explicit about which queries go where.

---

**7. Looking Forward — What Changes by Late 2026**

Selected developments visible in the literature as of May 2026:

* **MoE-class Qwen** — A "Qwen3-30B-A3B" mixture-of-experts release. 30 B total params, ~3 B active per token. Inference economics radically different — much closer to 4B compute, much closer to 30B quality. Worth tracking.

* **Speculative decoding gets easier** — EAGLE-3 reduces draft-head training cost and pushes acceptance to ~0.85 across families.

* **KV cache compression beyond INT4** — methods like H2O, StreamingLLM-style sinks, and KIVI have moved from research to production. Expect long-context decode bandwidth to halve again.

* **Edge silicon catches up** — Apple's M5-series and Qualcomm Snapdragon X75/X80 successors push edge throughput past 100 tok/s for 4B-class models, eroding the cloud-only quality advantage.

* **Standardized agent harnesses** — model serving stacks growing in to expose tool-calling, streaming partial responses, and structured outputs as first-class primitives (vLLM has `--tool-call-parser`, SGLang has compile-time structured generation).

The systems engineering doesn't fundamentally change. The constants in the roofline math get better, the models get smarter at the same parameter count, and the production observability we've talked about stays the right things to measure.

---

</details>

## 动手练习

1. **构建 4B↔72B 路由器。** 用小型 embedding 模型 + 对 prompt 做逻辑回归，预测某个 query 是否“需要”72B（在留出集上按输出质量对比来打标）。测量不同置信度阈值下的路由准确率与节省的总成本。

2. **投机解码的墙钟时间测量。** 在 vLLM 中配置 Qwen2.5-1.5B 为 Qwen2.5-72B 做 draft。在三种 workload 上测量总 tok/s 与接受率：chat、code、math。讨论为何 α 随 workload 不同。

3. **生产环境的可观测性部署。** 以 `--engine-metrics-enable` 运行 vLLM 并抓取 Prometheus 端点。用 §3 的指标搭建 Grafana 看板。生成合成负载（例如 `vllm-benchmark`）。找出负载下最先变化的指标 —— 那就是你的饱和信号。

4. **故障注入。** 在 4×H100 vLLM 推理服务运行时，故意破坏 NVLink（`NCCL_P2P_DISABLE=1`）。测量吞吐的下降幅度。与基于集合通信带宽的预期值对比 —— 验证第 4 讲的诊断。

5. **容量规划。** 给定 §5 的场景，但 **峰值系数高 2×** 且 **40% 路由到 72B**，重做容量计算。需要多少个 4-H100 机器节点？每 token 成本是多少？

6. **边缘更新漂移研究。** 选定一个 query 集。连续 4 周每周在 Qwen3-4B（边缘）与 Qwen2.5-72B（云端）上运行（用模型快照模拟）。跟踪边缘与云端响应之间随时间变化的 BLEU/ROUGE/语义相似度。这类遥测正是能在早期捕获漂移的东西。

---

## 要点

| 要点 | 为何重要 |
|---|---|
| 跨家族投机解码很脆弱 | tokenizer + 后训练不匹配限制了 α 上限 |
| 内联投机解码（EAGLE-2/3）优于双模型方案 | 实践中内存更低 + α 更高 |
| 按 70/30 路由到 4B/72B 是稳妥的默认 | 依据质量投诉调参，而非凭审美 |
| TTFT、ITL、KV occupancy 是正确的顶层看板指标 | 它们最先捕获最常见的病症 |
| 病症清单很短且可复用 | 大多数生产环境的“性能诡异” bug 都在这份清单上 |
| 边缘/云混合有自身的失效模式 | 隐私边界、更新漂移、fail-open 行为 |
| MoE-Qwen（混合专家模型）将在 2026 年末再次改写这套计算 | 跟踪激活参数量，而非总参数量 |

---

## 资源

* **[Speculative Decoding 论文](https://arxiv.org/abs/2211.17192)：** 双模型投机解码的形式化分析。
* **[Medusa 论文](https://arxiv.org/abs/2401.10774)：** 通过额外 head 实现的内联投机解码。
* **[EAGLE 论文](https://arxiv.org/abs/2401.15077) 与 [EAGLE-2](https://arxiv.org/abs/2406.16858)：** 更优的内联投机解码；事实上的生产技术。
* **[vLLM 算子指标](https://docs.vllm.ai/en/latest/serving/metrics.html)：** 生产环境的可观测性接口。
* **[SGLang 结构化生成](https://github.com/sgl-project/sglang)：** 约束输出何时重要。
* **[H2O —— heavy-hitter KV 压缩](https://arxiv.org/abs/2306.14048)：** 降低长上下文 decode（逐 token 生成阶段）的带宽。
* **[StreamingLLM —— attention sinks](https://arxiv.org/abs/2309.17453)：** 长时间运行会话的 KV 管理。
* **[阶段 5 —— Qwen 推理优化系列](README.md)：** 本系列的其他讲次。
* **[阶段 5 —— 边缘 LLM 推理内幕](../03-边缘LLM推理内部机制/Lecture-01.md)：** 本系列所依托的 roofline（性能上界模型）基础讲。
* **[阶段 5 —— NCCL 深入剖析](../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/08-NCCL深度探索/README.md)：** 72B 路径的多 GPU 集合通信层。


<details>
<summary>English original</summary>

**Hands-On Exercises**

1. **Build a 4B↔72B router.** Use a small embedding model + logistic regression on prompts to predict whether a query "needs" 72B (label by comparing outputs by quality on a held-out set). Measure routing accuracy and total cost saved at various confidence thresholds.

2. **Speculative-decoding wall-clock measurement.** Set up Qwen2.5-1.5B drafting Qwen2.5-72B in vLLM. Measure aggregate tok/s and acceptance rate on three workloads: chat, code, math. Discuss why α differs by workload.

3. **Production observability deployment.** Run vLLM with `--engine-metrics-enable` and scrape the Prometheus endpoint. Build a Grafana dashboard with the §3 metrics. Generate synthetic load (e.g., `vllm-benchmark`). Identify which metrics move first under load — that's your saturation signal.

4. **Failure injection.** While serving from 4×H100 vLLM, deliberately break NVLink (`NCCL_P2P_DISABLE=1`). Measure the throughput drop. Compare to expected based on collective bandwidth — confirm the diagnosis from Lecture 4.

5. **Capacity sizing.** Given the §5 scenario but with **2× higher peak factor** and **40% routed to 72B**, redo the capacity math. How many 4-H100 boxes do you need? What's your per-token cost?

6. **Edge-update drift study.** Pick a query set. Run on Qwen3-4B (edge) and Qwen2.5-72B (cloud) every week for 4 weeks (simulate with model snapshots). Track BLEU/ROUGE/semantic similarity between edge and cloud responses over time. This is the kind of telemetry that catches drift early.

---

**Key Takeaways**

| Takeaway | Why it matters |
|---|---|
| Cross-family speculative decoding is fragile | Tokenizer + post-training mismatch caps α |
| Inline spec dec (EAGLE-2/3) beats two-model | Lower memory + higher α in practice |
| Routing 70/30 to 4B/72B is a strong default | Tune with quality complaints, not aesthetics |
| TTFT, ITL, KV occupancy are the right top-level dashboards | They catch the most common pathologies first |
| Pathology lists are short and repeatable | Most production "weird performance" bugs are on the same list |
| Hybrid edge/cloud has its own failure modes | Privacy boundary, update drift, fail-open behavior |
| MoE-Qwen will change the math again in late 2026 | Track active params, not total params |

---

**Resources**

* **[Speculative Decoding paper](https://arxiv.org/abs/2211.17192):** Two-model spec dec formal analysis.
* **[Medusa paper](https://arxiv.org/abs/2401.10774):** Inline spec dec via extra heads.
* **[EAGLE paper](https://arxiv.org/abs/2401.15077) and [EAGLE-2](https://arxiv.org/abs/2406.16858):** Better inline spec dec; the de-facto production technique.
* **[vLLM operator metrics](https://docs.vllm.ai/en/latest/serving/metrics.html):** Production observability surface.
* **[SGLang structured generation](https://github.com/sgl-project/sglang):** When constrained outputs matter.
* **[H2O — heavy-hitter KV compression](https://arxiv.org/abs/2306.14048):** Long-context decode bandwidth reduction.
* **[StreamingLLM — attention sinks](https://arxiv.org/abs/2309.17453):** Long-running session KV management.
* **[Phase 5 — Qwen Inference Optimization series](README.md):** Other lectures in this series.
* **[Phase 5 — Edge LLM Inference Internals](../03-边缘LLM推理内部机制/Lecture-01.md):** The foundational roofline lecture this series builds on.
* **[Phase 5 — NCCL Deep Dive](../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/08-NCCL深度探索/README.md):** Multi-GPU collective layer for the 72B path.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track C - Edge AI/Qwen Inference Optimization/Lecture-05.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20C%20-%20Edge%20AI/Qwen%20Inference%20Optimization/Lecture-05.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
