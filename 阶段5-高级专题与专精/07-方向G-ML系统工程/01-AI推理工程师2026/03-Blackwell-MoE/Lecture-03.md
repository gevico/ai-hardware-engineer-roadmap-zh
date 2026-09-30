---
title: Part 3 · Lecture 03 — 专家并行（EP）与 gating 热路径
description: Part 3 · Lecture 03 — 专家并行（EP）与 gating 热路径
---

# Part 3 · Lecture 03 — 专家并行（EP）与 gating 热路径

## 概述

张量并行（Part 2 Lecture 04）把单次矩阵乘拆分到多块 GPU 上，再用 all-reduce 汇合结果。**专家并行则把 MoE（混合专家模型）层的专家拆分到多块 GPU 上，并用 all-to-all 汇合结果。** 这一个结构上的差异，就是推理图从 dense 转向 MoE 时最重大的变化。

本讲涵盖：

1. **EP 划分了什么**，以及 token 布线如何跨 GPU 工作。
2. **all-to-all 通信模式**——以及它为何比 TP 的 all-reduce 更难。
3. MoE 的 **TP × EP × PP 组合**——每条并行轴何时才值回票价。
4. **DeepEP** 与 DeepSeek 专用优化。
5. **专家负载均衡**——决定 EP 能否扩展的 runtime 决策。
6. **Gating 计算开销**——单 token 小，整簇大。
7. **NVL72 上的 token 级布线**——什么能扩展，什么不能。
8. Llama 式 dense TP 回退方案（小 MoE 下 EP 输给 TP 时）。

读完后，你应能在给定的 Blackwell 簇上为 DeepSeek V3.1 或 Qwen3-MoE 235B-A22B 选出正确的 TP × EP 组合，预估 all-to-all 开销，并诊断负载不均衡瓶颈。

---

## 1. EP 划分了什么

在专家并行中，**每块 GPU 持有一部分专家**。对于拥有 256 个 routed 专家的 DeepSeek V3.1，在 EP=8 时：

```text
GPU 0: experts 0..31
GPU 1: experts 32..63
GPU 2: experts 64..95
GPU 3: experts 96..127
GPU 4: experts 128..159
GPU 5: experts 160..191
GPU 6: experts 192..223
GPU 7: experts 224..255

Shared experts: replicated on every GPU
Gating network: replicated on every GPU
```


在某个 token 的 MoE 层上，gating 网络会产出 top-8 的专家 ID。这些 ID 可能落在任意一块 GPU 上。该 token 的 hidden state 必须**被派发到正确的 GPU**，在那里执行 FFN，结果再**被收集回来**。

这就是 **all-to-all 通信**。每块 GPU 都会根据 token 需要哪些专家，把 token 发给其他每一块 GPU。

### 1.1 派发表

在每个 MoE 层上，每步：

```text
Input: hidden states for all tokens in the batch — shape (B, hidden)
Gating: top-8 expert IDs per token — shape (B, 8)

For each (token, expert_id) pair:
  expert_owner_gpu = expert_id // (n_experts / EP_size)

Build per-source-rank send buffers:
  send[rank_r] = [token i, hidden_state_i for all tokens routed to expert on rank r]

NCCL all-to-all dispatch
Each rank receives: tokens needing local experts
Compute FFN per expert locally on received tokens
NCCL all-to-all combine: send back to original ranks

Original ranks combine outputs weighted by gating scores
```


### 1.2 为何这比 TP all-reduce 更难

TP all-reduce：每个 rank 贡献等大的 buffer，NCCL 以 ring 或 tree 方式规约。开销可预测。

EP all-to-all：每个 rank 把*不同大小*的 buffer 发往*不同目的地*（因为 token 到专家的布线并不均匀）。NCCL `alltoallv` 能处理可变大小，但通信效率取决于：

* **Token 分布**——若所有 token 都流向同一个专家，就只有一块 rank 见到流量。
* **NVLink 拓扑**——理想情况下 all-to-all 对称地使用该 fabric。
* **消息大小**——小消息会撞上延迟下限；大消息会打满带宽。

在低批大小下，EP all-to-all 是**延迟主导**的。在高批大小下，它转为**带宽主导**。两种情形有不同的优化旋钮。

---

## 2. all-to-all 的开销

对于 8× B200 上的 DeepSeek V3.1，EP=8、batch=64、decode（逐 token 生成阶段，每请求 1 个 token）：

```text
Per layer per step:
  - 64 tokens × 8 experts each = 512 token-expert pairs
  - Average tokens per expert: 512 / 256 = 2
  - Per token data: hidden_size × bytes = 7168 × 1 (FP8) = 7 KB

Send per source rank: 64 tokens × ~50% local = 32 tokens elsewhere
                    ≈ 32 × 7 KB / 8 destinations = ~28 KB per pair

NCCL alltoallv: ~28 KB to 7 destinations, asymmetric
  Latency-bound regime — bandwidth not the constraint
  Approx 100-200 μs per all-to-all

Per layer: 2 all-to-alls (dispatch + combine) ≈ 200-400 μs
Per token (58 MoE layers): ~12-23 ms in all-to-all alone
```


对比 B200 上受带宽约束的权重读取：

```text
Active weights: 37B × 0.5 bytes (FP4) = 18.5 GB
Per GPU at EP=8: 18.5 / 8 ≈ 2.3 GB (only the experts hit per token, summed over all layers)
HBM read time: 2.3 GB / 8 TB/s ≈ 0.29 ms per token total
Per MoE layer (58 of the 61 layers): ~5 μs in weight reads
```


因此在 EP=8 的部署中，**all-to-all 比权重读取高出一个数量级甚至更多**——每 token 约 12-23 ms，对约 0.3 ms。这正是 MoE 推理从根本上是一个**通信受限**问题的原因——甚至比 dense TP 更是如此——也是 NVLink 5 带宽翻倍能够见效的原因。


<details>
<summary>English original</summary>

**Part 3 · Lecture 03 — Expert Parallelism (EP) and the Gating Hot Path**

**Overview**

Tensor parallelism (Part 2 Lecture 04) splits a single matrix multiply across multiple GPUs and joins the result with an all-reduce. **Expert parallelism splits the experts of an MoE layer across multiple GPUs and joins the result with an all-to-all.** This one structural difference is the most consequential change in the inference graph going from dense to MoE.

This lecture covers:

1. **What EP partitions** and how token routing works across GPUs.
2. **The all-to-all communication pattern** — and why it's harder than TP's all-reduce.
3. **TP × EP × PP combinations** for MoE — when each parallelism axis pays its rent.
4. **DeepEP** and DeepSeek-specific optimizations.
5. **Expert load balancing** — the runtime decision that decides whether EP scales.
6. **Gating computation cost** — small per-token, large per-cluster.
7. **Token-level routing on NVL72** — what scales and what doesn't.
8. The Llama-style dense TP fallback (when EP loses to TP for small MoEs).

By the end you should be able to pick the right TP × EP combination for DeepSeek V3.1 or Qwen3-MoE 235B-A22B on a given Blackwell cluster, predict the all-to-all overhead, and diagnose load-imbalance bottlenecks.

---

**1. What EP partitions**

In expert parallelism, **each GPU holds a subset of the experts**. For DeepSeek V3.1 with 256 routed experts at EP=8:

```text
GPU 0: experts 0..31
GPU 1: experts 32..63
GPU 2: experts 64..95
GPU 3: experts 96..127
GPU 4: experts 128..159
GPU 5: experts 160..191
GPU 6: experts 192..223
GPU 7: experts 224..255

Shared experts: replicated on every GPU
Gating network: replicated on every GPU
```

At a token's MoE layer, the gating network produces top-8 expert IDs. These IDs may be on any GPU. The token's hidden state must be **dispatched to the right GPUs**, the FFN run there, and the results **gathered back**.

This is the **all-to-all communication**. Every GPU sends tokens to every other GPU according to which experts those tokens need.

**1.1 The dispatch table**

At each MoE layer, per step:

```text
Input: hidden states for all tokens in the batch — shape (B, hidden)
Gating: top-8 expert IDs per token — shape (B, 8)

For each (token, expert_id) pair:
  expert_owner_gpu = expert_id // (n_experts / EP_size)

Build per-source-rank send buffers:
  send[rank_r] = [token i, hidden_state_i for all tokens routed to expert on rank r]

NCCL all-to-all dispatch
Each rank receives: tokens needing local experts
Compute FFN per expert locally on received tokens
NCCL all-to-all combine: send back to original ranks

Original ranks combine outputs weighted by gating scores
```

**1.2 Why this is harder than TP all-reduce**

TP all-reduce: every rank contributes the same-sized buffer, NCCL reduces in a ring or tree. Predictable cost.

EP all-to-all: every rank sends *different-sized* buffers to *different* destinations (because tokens are routed unevenly to experts). NCCL `alltoallv` handles variable sizes but communication efficiency depends on:

* **Token distribution** — if all tokens go to one expert, only one rank sees traffic.
* **NVLink topology** — ideally all-to-all uses the fabric symmetrically.
* **Message size** — small messages hit the latency floor; large messages saturate bandwidth.

At low batch sizes, EP all-to-all is **latency-dominated**. At high batch sizes, it becomes **bandwidth-dominated**. Both regimes have different optimization knobs.

---

**2. The all-to-all cost**

For DeepSeek V3.1 at EP=8 on 8× B200, batch=64, decode (1 token per request):

```text
Per layer per step:
  - 64 tokens × 8 experts each = 512 token-expert pairs
  - Average tokens per expert: 512 / 256 = 2
  - Per token data: hidden_size × bytes = 7168 × 1 (FP8) = 7 KB

Send per source rank: 64 tokens × ~50% local = 32 tokens elsewhere
                    ≈ 32 × 7 KB / 8 destinations = ~28 KB per pair

NCCL alltoallv: ~28 KB to 7 destinations, asymmetric
  Latency-bound regime — bandwidth not the constraint
  Approx 100-200 μs per all-to-all

Per layer: 2 all-to-alls (dispatch + combine) ≈ 200-400 μs
Per token (58 MoE layers): ~12-23 ms in all-to-all alone
```

Compare to the bandwidth-bound weight read on B200:

```text
Active weights: 37B × 0.5 bytes (FP4) = 18.5 GB
Per GPU at EP=8: 18.5 / 8 ≈ 2.3 GB (only the experts hit per token, summed over all layers)
HBM read time: 2.3 GB / 8 TB/s ≈ 0.29 ms per token total
Per MoE layer (58 of the 61 layers): ~5 μs in weight reads
```

So in an EP=8 deployment, **all-to-all dominates weight reads by an order of magnitude or more** — ~12-23 ms against ~0.3 ms per token. This is why MoE inference is fundamentally a **communication-bound** problem — even more than dense TP — and why NVLink 5's bandwidth doubling pays off.

</details>

### 2.1 更大的批大小下

batch=512 时：

```text
Per layer: 512 tokens × 8 experts = 4096 token-expert pairs
Average tokens per expert: 4096 / 256 = 16
Per all-to-all message: ~30 tokens × 7 KB = ~200 KB per destination

Now bandwidth-dominated: 200 KB × 7 destinations ÷ 1.8 TB/s ≈ 1 μs (much less than latency floor)
But NCCL setup + small-message handling ≈ 50-100 μs

Per layer: ~100-200 μs all-to-all
Per token (58 MoE layers): ~6-12 ms
```

更大的批会**摊薄每步的 all-to-all 开销**。**MoE 比稠密模型更偏好大批大小。** 连续批处理必不可少。

---

## 3. TP × EP × PP 组合

对 MoE 模型，三条并行轴：

* **张量并行（TP）** —— 把每层的矩阵切分到多个 GPU 上（与稠密模型相同）。
* **专家并行（EP）** —— 把专家切分到多个 GPU 上。
* **流水线并行（PP）** —— 把 layer 切分到多个 GPU 上（推理中很少用，训练中常见）。

总并行度：每个副本 `TP × EP × PP` 个 GPU。

### 3.1 纯 EP（无 TP）

每块 GPU 持有一部分专家，外加 attention 模块的完整副本。

* 对 DeepSeek V3.1（attention 是稠密的，每层约 110M 参数）：每块 GPU 都有完整的 attention 权重 —— 开销很小。
* 只有 MoE 层需要 all-to-all。
* attention 本地执行 —— 无需 TP 的 all-reduce。

**纯 EP 是 2026 年 MoE 的标准 recipe。** 8× B200 用 EP=8，16× B200 用 EP=16，NVL72 用 EP=64。

### 3.2 TP + EP（组合）

针对激活参数极大或内存吃紧的部署：

* TP 切分 attention 模块和各专家的 FFN 矩阵。
* EP 切分专家。
* 组合起来：每块 GPU 持有每个专家权重的 1/TP，以及专家的 1/EP。

适用于：

* 高并发下 attention 本身成为瓶颈的 DeepSeek V3.1。
* TP=2、EP=8（共 16 块 GPU）的均衡切分。

代价：更多 all-reduce（TP）+ 更多 all-to-all（EP）。收益递减；除非 attention 计算是瓶颈，否则很少值得。

### 3.3 推荐表

| Model | Cluster | Recipe |
|-------|---------|--------|
| Qwen3-MoE 235B-A22B | 2× B200 | EP=2, FP4 weights, FP8 KV |
| Qwen3-MoE 235B-A22B | 8× B200 | EP=8, FP4 weights, FP8 KV |
| DeepSeek V3.1 | 8× B200 | EP=8, FP4 weights, BF16 MLA-KV |
| DeepSeek V3.1 | 16× B200 | EP=16, FP4 weights, FP8 KV |
| DeepSeek V3.1 | NVL72（单副本） | TP=2 × EP=32 = 64 GPU, FP4 |
| DeepSeek V3.1 | NVL72（多副本） | 4 副本 × 每个 16 GPU |

---

## 4. DeepEP 与 DeepSeek 专属优化

DeepSeek 开源了 **DeepEP**（[github.com/deepseek-ai/DeepEP](https://github.com/deepseek-ai/DeepEP)）—— 一个专为 DeepSeek V3 系列调优的 EP 通信库。

关键优化：

* **非对称 dispatch 与 combine** —— 直接使用 NVLink RDMA 和 CUDA IPC 原语，而不走 NCCL 集合通信。
* **两阶段 routing** —— 第一阶段 all-to-all 处理节点内，第二阶段处理节点间（当 NVL72 被切分时）。
* **token 分层再均衡** —— 在层之间重新打散 token，以均衡专家负载。

在 NVL72 上相对默认 NCCL `alltoallv` 的 benchmark 收益：

* 小批下 all-to-all 延迟快 1.5–2×。
* 大批下快 1.2–1.5×（该区间带宽已接近 NVLink 峰值）。

**SGLang 和 TRT-LLM 集成了 DeepEP**，用于 DeepSeek 系列部署。vLLM 在 DeepSeek 模型路径上做了集成。对 Qwen3-MoE 235B-A22B（非 DeepSeek），同样的技术适用，但**库集成情况各不相同**。

---

## 5. 专家负载均衡

MoE 推理中最微妙的一个问题。

### 5.1 问题

**并非所有专家被激活的程度相同。** 有些专家分到很多 token，有些分到很少。**最慢的专家决定步时长**（即“拖后腿者”模式）。

训练时，MoE 的损失中包含一个**负载均衡辅助项**，惩罚 routing 不均。推理时这一项已固定 —— 但训练时的不均衡会以自然分布偏置的形式持续存在。

实际情况：某次部署可能看到专家利用率相差 3-10×。负载最重的专家每步做的工作量是最轻专家的 3-10×。

### 5.2 应对方法

**token 级再均衡**（DeepEP、SGLang）：
* 逐层检测不均衡。
* 把一小部分 token 重新打散到负载较轻的专家，即便 gating 分数并非最优。
* 用轻微的质量损失换取可观的吞吐提升。

**专家复制**（预算允许时）：
* 在多个 GPU 上复制热点专家。
* 把 token 路由到任意一个副本。
* 代价：更多 HBM（在已经用 FP4 的情况下通常负担不起）。

**丢 token 策略**（较老，较少见）：
* 若某专家超出容量，丢弃分数最低的 token。
* 质量下降；推理中很少使用（训练中可接受）。


<details>
<summary>English original</summary>

**2.1 At larger batch sizes**

At batch=512:

```text
Per layer: 512 tokens × 8 experts = 4096 token-expert pairs
Average tokens per expert: 4096 / 256 = 16
Per all-to-all message: ~30 tokens × 7 KB = ~200 KB per destination

Now bandwidth-dominated: 200 KB × 7 destinations ÷ 1.8 TB/s ≈ 1 μs (much less than latency floor)
But NCCL setup + small-message handling ≈ 50-100 μs

Per layer: ~100-200 μs all-to-all
Per token (58 MoE layers): ~6-12 ms
```

Larger batch **amortizes the per-step all-to-all cost**. **MoE strongly prefers higher batch sizes than dense.** Continuous batching is essential.

---

**3. TP × EP × PP combinations**

For an MoE model, three parallelism axes:

* **Tensor parallelism (TP)** — splits each layer's matrices across GPUs (same as dense).
* **Expert parallelism (EP)** — splits experts across GPUs.
* **Pipeline parallelism (PP)** — splits layers across GPUs (rarely used in inference, common in training).

The total degree: `TP × EP × PP` GPUs per replica.

**3.1 Pure EP (no TP)**

Each GPU holds a subset of experts plus a full copy of the attention block.

* For DeepSeek V3.1 (attention is dense at ~110M params per layer): each GPU has the full attention weights — cheap.
* All-to-all only for MoE layers.
* Attention runs locally — no TP all-reduce needed.

**Pure EP is the standard 2026 MoE recipe.** EP=8 for 8× B200, EP=16 for 16× B200, EP=64 for the NVL72.

**3.2 TP + EP (combined)**

For very large active params or memory-tight deployments:

* TP partitions the attention block and the per-expert FFN matrices.
* EP partitions the experts.
* Combined: each GPU has 1/TP of each expert's weights and 1/EP of the experts.

This is used for:

* DeepSeek V3.1 at high concurrency where the attention itself is a bottleneck.
* TP=2, EP=8 (16 GPUs total) for a balanced split.

The cost: more all-reduces (TP) + more all-to-alls (EP). Diminishing returns; rarely worth it unless attention compute is the bottleneck.

**3.3 Recommendation table**

| Model | Cluster | Recipe |
|-------|---------|--------|
| Qwen3-MoE 235B-A22B | 2× B200 | EP=2, FP4 weights, FP8 KV |
| Qwen3-MoE 235B-A22B | 8× B200 | EP=8, FP4 weights, FP8 KV |
| DeepSeek V3.1 | 8× B200 | EP=8, FP4 weights, BF16 MLA-KV |
| DeepSeek V3.1 | 16× B200 | EP=16, FP4 weights, FP8 KV |
| DeepSeek V3.1 | NVL72 (single replica) | TP=2 × EP=32 = 64 GPUs, FP4 |
| DeepSeek V3.1 | NVL72 (multi-replica) | 4 replicas × 16 GPUs each |

---

**4. DeepEP and DeepSeek-specific optimizations**

DeepSeek released **DeepEP** ([github.com/deepseek-ai/DeepEP](https://github.com/deepseek-ai/DeepEP)) — an EP communication library specifically tuned for the DeepSeek V3 family.

Key optimizations:

* **Asymmetric dispatch and combine** — uses NVLink RDMA and CUDA IPC primitives directly instead of going through NCCL collectives.
* **Two-phase routing** — first all-to-all for intra-node, second for inter-node (when NVL72 is partitioned).
* **Token-tier rebalancing** — re-shuffles tokens between layers to balance expert load.

Benchmark gains over default NCCL `alltoallv` on NVL72:

* 1.5–2× faster all-to-all latency on small batches.
* 1.2–1.5× faster on large batches (the bandwidth regime is already close to NVLink peak).

**SGLang and TRT-LLM integrate DeepEP** for DeepSeek-family deployments. vLLM does so for the DeepSeek model path. For Qwen3-MoE 235B-A22B (which is not DeepSeek), the same techniques apply but the **library integration varies**.

---

**5. Expert load balancing**

The single most subtle MoE inference issue.

**5.1 The problem**

**Not all experts are activated equally.** Some experts get many tokens, some get few. **The slowest expert sets the step time** (a "straggler" pattern).

In training, MoE loss includes a **load-balancing auxiliary term** that penalizes uneven routing. At inference time, this is fixed — but training-time imbalance can persist as natural distribution bias.

Real-world: a deployment might see expert utilization vary 3-10×. The most-loaded expert does 3-10× more work per step than the least-loaded.

**5.2 Approaches**

**Token-level rebalancing** (DeepEP, SGLang):
* Detect imbalance per layer.
* Re-shuffle a small fraction of tokens to less-loaded experts even if the gating score is suboptimal.
* Trade slight quality loss for substantial throughput.

**Expert replication** (when budget allows):
* Replicate hot experts on multiple GPUs.
* Route tokens to either copy.
* Cost: more HBM (typically not affordable at FP4 already).

**Drop-token policies** (older, less common):
* If an expert is over capacity, drop the lowest-score tokens.
* Quality degradation; rarely used in inference (acceptable in training).

</details>

### 5.3 测量

生产环境 runtime（SGLang、vLLM）会暴露 expert 利用率指标：

```text
expert 0..7   on GPU 0: tokens_per_step = [180, 95, 110, 75, 140, 88, 200, 95]
expert 0..7   on GPU 1: ...
```

使用 Nsight Systems 的 MoE-aware 视图（较新的 Nsight 版本）或 runtime 指标端点。**留意 max/min 比值 > 3**——这是重平衡开始值得做的阈值。

---

## 6. Gating 计算开销

gating 网络是 `Linear(hidden, num_experts)`。对于 DeepSeek V3.1：

```text
Per token: 7168 × 256 = 1.83M FLOPs
Per layer × per batch: 1.83M × batch × seq
```

在 batch=64、seq=128（典型 chat 形状）下：

```text
Per gating call: 1.83M × 64 × 128 ≈ 15 GFLOPs
On B200 FP16 (2,250 TFLOPs peak): ~7 μs

Per layer: gating + topk + scatter overhead ≈ 20-50 μs
Per token (58 MoE layers): ~1.2-2.9 ms in gating-related ops
```

很小。**通常不是瓶颈**，除非 runtime 的 gating 实现未做优化。

微妙之处在于：gating *频繁*且*对延迟敏感*。即使单次开销很小，**糟糕的实现也会给总 step 时间增加 10-20%**。vLLM 0.22+、SGLang 0.5+ 和 TRT-LLM 都已有优化过的 gating 路径。

---

## 7. NVL72 上的 token 级 routing

在完整的 NVL72 规模（72 GPU）下，还有一些额外考量：

* **EP=64** 是可以实现的，由 8 个 GPU 承载共享基础设施（gating、attention、embed）。
* **跨域通信**——在 NVL72 fabric 内部，NVLink 是均匀的。超出之后（多机架），延迟会上升。
* **拓扑感知 routing**——DeepEP 和 SGLang 利用 NVLink 拓扑提示来优化。

对于 NVL72 单副本 DeepSeek V3.1：

* 每 GPU 持有的 expert：256 / 64 = 4 个 expert。
* 每 token routing：8 个 expert → 每 token 最多触及 8 个 GPU。
* all-to-all 通信的规模约为 O(EP × batch × hidden)，在 130 TB/s 的 NVLink 聚合带宽上仍然可处理。

### 7.1 EP 何时不划算

对于**小 batch**（并发 < 16），EP 开销可能超过带宽节省。对这类工作负载，**少 GPU 的 TP** 可能比多 GPU 的 EP 更快：

* Qwen3-MoE 235B-A22B 在并发 8、2× B200（TP=2、EP=2）下：TPOT ~12 ms。
* 同样配置在 8× B200（EP=8）下：TPOT ~10 ms——GPU 数量多 4 倍，却只快 17%。

对低 batch 的 chat 产品，**更小簇的部署往往更具成本效率**。

---

## 实验——在 Qwen3-MoE 上测试 EP 扩展

目标：测量 EP 扩展性，并找出 all-to-all 开销的交叉点。

1. **硬件**——2× B200、4× B200、8× B200（或 NVL72 分区）。
2. **模型**——Qwen3-MoE 235B-A22B FP4（若 FP4 路径未就绪则用 FP8）。
3. **Runtime**——SGLang 0.5+ V1（针对非 DeepSeek MoE 最佳的 DeepEP 风格集成）或 vLLM 0.22+。
4. **在三个 EP 并行度上做 bench**——EP=2、EP=4、EP=8。相同 batch=64、prompt=1024、output=256、iterations=100，预热 20 次。
5. **用 Nsight Systems 对一次 EP=4 运行做 profile**。找出 all-to-all 占 step 时间的比例。
6. **绘制**每副本吞吐、每 GPU 吞吐和 all-to-all 开销百分比。
7. **找出**扩展效率曲线。

通过标准：你能用实测数字论证，在并发 64 的 chat 产品上该选 EP=4 还是 EP=8。

---

## 自检

1. 对于 EP=8、batch=64 的 DeepSeek V3.1，预测其在 NVL72 NVLink 5 上每层的 all-to-all 时间（假设消息为 28 KB × 7 个目的地）。
2. 为什么 NVLink 5 的 2× 带宽提升，对 MoE EP all-to-all 的帮助要大于对 dense TP all-reduce 的帮助？
3. 同事测得 GPU 0 上的 expert 利用率为 [180, 95, 110, 75, 140, 88, 200, 95]。不平衡比值是多少？你是否应启用 token 级重平衡？
4. 对 DeepSeek V3.1，EP=8 与 TP=8 相比：哪个的跨 GPU 通信开销更大？为什么？
5. 在 NVL72 EP=64、batch=2 时，每层每步的 gating + all-to-all 开销约 80 μs，每层计算约 30 μs。问题出在哪，如何修复？

---


<details>
<summary>English original</summary>

**5.3 Measurement**

Production runtimes (SGLang, vLLM) expose expert utilization metrics:

```text
expert 0..7   on GPU 0: tokens_per_step = [180, 95, 110, 75, 140, 88, 200, 95]
expert 0..7   on GPU 1: ...
```

Use Nsight Systems' MoE-aware view (newer Nsight versions) or runtime metrics endpoints. **Watch for ratio max/min > 3** — this is the threshold where rebalancing becomes worthwhile.

---

**6. Gating computation cost**

The gating network is `Linear(hidden, num_experts)`. For DeepSeek V3.1:

```text
Per token: 7168 × 256 = 1.83M FLOPs
Per layer × per batch: 1.83M × batch × seq
```

At batch=64, seq=128 (typical chat-shape):

```text
Per gating call: 1.83M × 64 × 128 ≈ 15 GFLOPs
On B200 FP16 (2,250 TFLOPs peak): ~7 μs

Per layer: gating + topk + scatter overhead ≈ 20-50 μs
Per token (58 MoE layers): ~1.2-2.9 ms in gating-related ops
```

Small. **Usually not the bottleneck** unless the runtime's gating implementation is unoptimized.

The subtlety: gating is *frequent* and *latency-sensitive*. Even if individual cost is small, **poor implementation can add 10-20% to total step time**. vLLM 0.22+, SGLang 0.5+, and TRT-LLM all have optimized gating paths.

---

**7. Token-level routing on NVL72**

At full NVL72 scale (72 GPUs), some additional considerations:

* **EP=64** is achievable with 8 GPUs holding shared infrastructure (gating, attention, embed).
* **Cross-domain communication** — within NVL72 fabric, NVLink is uniform. Outside (multi-rack), latencies increase.
* **Topology-aware routing** — DeepEP and SGLang use NVLink topology hints to optimize.

For NVL72 single-replica DeepSeek V3.1:

* Per-GPU expert holding: 256 / 64 = 4 experts.
* Per-token routing: 8 experts → at most 8 GPUs touched per token.
* All-to-all communication scales like O(EP × batch × hidden), which is still tractable on 130 TB/s aggregate NVLink BW.

**7.1 When EP doesn't pay**

For **small batches** (concurrency < 16), EP overhead can exceed the bandwidth savings. For these workloads, **fewer-GPU TP** can be faster than many-GPU EP:

* Qwen3-MoE 235B-A22B at concurrency 8 on 2× B200 (TP=2, EP=2): TPOT ~12 ms.
* Same on 8× B200 (EP=8): TPOT ~10 ms — only 17% faster despite 4× the GPUs.

For low-batch chat products, **smaller-cluster deployments are often more cost-efficient**.

---

**Lab — bench EP scaling on Qwen3-MoE**

Goal: measure EP scaling and identify the all-to-all overhead crossover.

1. **Hardware** — 2× B200, 4× B200, 8× B200 (or NVL72 partition).
2. **Model** — Qwen3-MoE 235B-A22B FP4 (or FP8 if FP4 path not ready).
3. **Runtime** — SGLang 0.5+ V1 (best DeepEP-style integration for non-DeepSeek MoE) or vLLM 0.22+.
4. **Bench at three EP degrees** — EP=2, EP=4, EP=8. Same batch=64, prompt=1024, output=256, iterations=100 with 20 warmup.
5. **Profile one EP=4 run** with Nsight Systems. Identify all-to-all fraction of step time.
6. **Plot** per-replica throughput, per-GPU throughput, and all-to-all overhead percentage.
7. **Identify** the scaling-efficiency curve.

Pass criterion: you can defend EP=4 vs EP=8 for a chat product at concurrency 64 with measured numbers.

---

**Self-check**

1. For DeepSeek V3.1 at EP=8, batch=64, predict the per-layer all-to-all time on NVL72 NVLink 5 (assume 28 KB messages × 7 destinations).
2. Why does NVLink 5's 2× bandwidth improvement specifically help MoE EP all-to-all more than dense TP all-reduce?
3. A teammate measures expert utilization on GPU 0 as [180, 95, 110, 75, 140, 88, 200, 95]. What's the imbalance ratio? Should you enable token-level rebalancing?
4. EP=8 vs TP=8 for DeepSeek V3.1: which has more cross-GPU communication overhead? Why?
5. At batch=2 on NVL72 EP=64, the per-step gating + all-to-all overhead per layer is ~80 μs. Per layer compute is ~30 μs. What's wrong, and what's the fix?

---

</details>

## References

* DeepSeek V3 technical report（EP 章节）— [arXiv:2412.19437](https://arxiv.org/abs/2412.19437)
* DeepEP library — [github.com/deepseek-ai/DeepEP](https://github.com/deepseek-ai/DeepEP)
* DeepSeek-MoE paper — [arXiv:2401.06066](https://arxiv.org/abs/2401.06066)
* Switch Transformers（最早的 MoE EP 讨论）— [arXiv:2101.03961](https://arxiv.org/abs/2101.03961)
* "GShard: Scaling Giant Models with Conditional Computation and Automatic Sharding" — [arXiv:2006.16668](https://arxiv.org/abs/2006.16668) — EP 领域的奠基论文
* Tutel（MoE 通信库）— [github.com/microsoft/tutel](https://github.com/microsoft/tutel)
* SGLang DeepSeek 推理服务指南 — [sgl-project.github.io](https://sgl-project.github.io/)
* NCCL all-to-all 文档 — [docs.nvidia.com/deeplearning/nccl/](https://docs.nvidia.com/deeplearning/nccl/)

Cross-references：

* [Part 2 → Lecture 04 — Tensor parallelism](../02-Hopper稠密计算/Lecture-04.md) — 用于 TP 与 EP 的对比
* [Phase 5 → GPU Infrastructure → Long-Context-MoE-Foundation-Training → 05 MoE Systems & Infrastructure](../../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/07-长上下文MoE基础训练/05-MoE-Systems-Infrastructure.md)

---

## 更新至 2026-06

NCCL 2.30+、DeepEP 最新版、SGLang 0.5+ MoE 路径、vLLM 0.22+ V1 MoE 支持、NVL72。DeepEP 2.x 或后继版本发布时更新。

---

## Next

* Next: [Lecture 04 — Disaggregated prefill / decode](Lecture-04.md)
* Previous: [Lecture 02 — Blackwell hardware story](Lecture-02.md)
* Up: [Part 3 — MoE at Blackwell](README.md)


<details>
<summary>English original</summary>

**References**

* DeepSeek V3 technical report (EP section) — [arXiv:2412.19437](https://arxiv.org/abs/2412.19437)
* DeepEP library — [github.com/deepseek-ai/DeepEP](https://github.com/deepseek-ai/DeepEP)
* DeepSeek-MoE paper — [arXiv:2401.06066](https://arxiv.org/abs/2401.06066)
* Switch Transformers (original MoE EP discussion) — [arXiv:2101.03961](https://arxiv.org/abs/2101.03961)
* "GShard: Scaling Giant Models with Conditional Computation and Automatic Sharding" — [arXiv:2006.16668](https://arxiv.org/abs/2006.16668) — the foundational EP paper
* Tutel (MoE communication library) — [github.com/microsoft/tutel](https://github.com/microsoft/tutel)
* SGLang DeepSeek serving guide — [sgl-project.github.io](https://sgl-project.github.io/)
* NCCL all-to-all documentation — [docs.nvidia.com/deeplearning/nccl/](https://docs.nvidia.com/deeplearning/nccl/)

Cross-references:

* [Part 2 → Lecture 04 — Tensor parallelism](../02-Hopper稠密计算/Lecture-04.md) — for TP-vs-EP comparison
* [Phase 5 → GPU Infrastructure → Long-Context-MoE-Foundation-Training → 05 MoE Systems & Infrastructure](../../../01-方向A-GPU基础设施/04-Nvidia-GPU/01-HPC环境搭建/07-长上下文MoE基础训练/05-MoE-Systems-Infrastructure.md)

---

**Current as of 2026-06**

NCCL 2.30+, DeepEP latest, SGLang 0.5+ MoE path, vLLM 0.22+ V1 MoE support, NVL72. Refresh when DeepEP 2.x or successor lands.

---

**Next**

* Next: [Lecture 04 — Disaggregated prefill / decode](Lecture-04.md)
* Previous: [Lecture 02 — Blackwell hardware story](Lecture-02.md)
* Up: [Part 3 — MoE at Blackwell](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 3 - MoE at Blackwell/Lecture-03.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%203%20-%20MoE%20at%20Blackwell/Lecture-03.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
