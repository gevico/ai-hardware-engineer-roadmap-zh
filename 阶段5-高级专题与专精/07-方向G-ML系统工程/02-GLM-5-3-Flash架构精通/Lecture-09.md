---
title: 模块 09 — 8 GPU 内存模型
description: 模块 09 — 8 GPU 内存模型
---

# 模块 09 — 8 GPU 内存模型

**合集：** [GLM-5.3-Flash 架构精讲](README.md) | **上一篇：** [← 模块 08](Lecture-08.md) | **下一篇：** [模块 10 →](Lecture-10.md)

---

前面每个模块都单独推导了某一个机制的开销。本模块把它们加总起来 —— 针对一个具体部署：**8× RTX 5090、PCIe Gen4、无 P2P**，用 NVFP4 checkpoint 做推理服务（本课程 capstone 所指向的 `sparkinfer-frontier` 案例研究）。结果并不是「总内存 ÷ 8」。对 latent-attention 架构做张量并行推理服务时，存在一个特定的、有充分记载的复制陷阱，本模块的存在就是为了确保你在制定自己部署的预算时不要掉进去。

---

## 学习目标

学完本模块后，你应当能够：

1. 计算若干精度下的权重存储下界，并与真实报道的部署占用对齐。
2. 针对给定的上下文长度，计算每 GPU 的 KDA 循环状态与 MLA latent-cache 预算。
3. 精确说明张量并行下 MLA 的复制陷阱，并解释为什么朴素的 `÷8` 除法是错的。
4. 逐项构建完整的每 GPU 内存方程，把每一项正确归类为分片（sharded）、复制（replicated）、随请求变化（request-dependent）或瞬态（transient）。

---

## 1. 权重存储：先看下界，再看现实

对于标称 320B 参数，统一精度的下界：

```text
   2 bytes/parameter  (BF16/FP16)   :   596 GiB
   1 byte/parameter   (FP8)          :   298 GiB
   0.5 bytes/parameter (naive INT4)  :   149 GiB
```

```text
   ┌────────────────────────────────────────────────────────────────┐
   │  These are ARITHMETIC LOWER BOUNDS from parameter count alone —  │
   │  not complete checkpoint or runtime sizes. Real storage adds     │
   │  block scales, and most deployments keep selected tensors        │
   │  (embeddings, LM head, some attention projections) at higher     │
   │  precision than the bulk of the model — exactly the allocation   │
   │  decision Hardware-Aware LLM Quantization — Module 11 formalizes │
   │  as a knapsack problem rather than a single global bit-width.     │
   └────────────────────────────────────────────────────────────────┘
```

**与实际格式对齐。** [硬件感知 LLM 量化 — 模块 02](../03-硬件感知LLM量化/Lecture-02.md) 推导出 NVFP4 的*真实*开销是 4.5 bit —— 而非上面那种朴素的 4-bit / 0.5 字节假设 —— 因为它带有每 16 个元素的 E4M3 块缩放因子：

```text
   NVFP4, real (0.5625 bytes/param)  :  320e9 × 0.5625  =  180 GB  =  167.64 GiB
```

**与报道的部署交叉核对。** `sparkinfer-frontier` 仓库报告约 **185 GiB 模型数据**，并报告在 8 个 GPU 上每 GPU 驻留 **24.8 GB**。有两项一致性检查，在信任任何报道数字之前都值得跑一遍：

```text
   uniformity check:   185 GiB / 8  =  23.125 GiB/GPU  =  24.83 GB/GPU
                       ✓ matches the reported 24.8 GB/GPU almost exactly —
                         consistent with roughly uniform 8-way weight sharding

   format check:       185 GiB reported  ÷  167.64 GiB pure-NVFP4 floor
                       =  1.104×   (a 10.4% overhead)
                       ✓ plausible: block-scale metadata plus embeddings/LM-head/
                         selected attention tensors kept at a higher precision
                         than the bulk NVFP4 body
```

```text
   ┌────────────────────────────────────────────────────────────────┐
   │  These are figures the sparkinfer-frontier repository reports —  │
   │  not measurements taken for this course. Treat the arithmetic    │
   │  above as a CONSISTENCY CHECK on a third-party report, which is  │
   │  a different epistemic status than an independent measurement.   │
   │  The check passing is reassuring; it is not the same as having   │
   │  measured it yourself.                                            │
   └────────────────────────────────────────────────────────────────┘
```

---


<details>
<summary>English original</summary>

**Module 09 — The 8-GPU Memory Model**

**Collection:** [GLM-5.3-Flash Architecture Mastery](README.md) | **Previous:** [← Module 08](Lecture-08.md) | **Next:** [Module 10 →](Lecture-10.md)

---

Every prior module derived one mechanism's cost in isolation. This module adds them up — for a concrete deployment: **8× RTX 5090, PCIe Gen4, no P2P**, serving the NVFP4 checkpoint (the `sparkinfer-frontier` case study this course's capstone builds toward). The result is not "total memory ÷ 8." Tensor-parallel serving of a latent-attention architecture has a specific, well-documented replication trap, and this module exists to make sure you don't fall into it while building your own deployment's budget.

---

**Learning objectives**

By the end of this module you should be able to:

1. Compute lower-bound weight storage at several precisions and reconcile them against a real reported deployment footprint.
2. Compute the KDA recurrent-state and MLA latent-cache budgets for a stated context length, per GPU.
3. State the tensor-parallel MLA replication trap precisely, and explain why naive `÷8` division is wrong.
4. Build a complete per-GPU memory equation, term by term, correctly classifying each term as sharded, replicated, request-dependent, or transient.

---

**1. Weight storage: lower bounds, then reality**

For a nominal 320B parameters, uniform-precision lower bounds:

```text
   2 bytes/parameter  (BF16/FP16)   :   596 GiB
   1 byte/parameter   (FP8)          :   298 GiB
   0.5 bytes/parameter (naive INT4)  :   149 GiB
```

```text
   ┌────────────────────────────────────────────────────────────────┐
   │  These are ARITHMETIC LOWER BOUNDS from parameter count alone —  │
   │  not complete checkpoint or runtime sizes. Real storage adds     │
   │  block scales, and most deployments keep selected tensors        │
   │  (embeddings, LM head, some attention projections) at higher     │
   │  precision than the bulk of the model — exactly the allocation   │
   │  decision Hardware-Aware LLM Quantization — Module 11 formalizes │
   │  as a knapsack problem rather than a single global bit-width.     │
   └────────────────────────────────────────────────────────────────┘
```

**Reconcile against the actual format.** [Hardware-Aware LLM Quantization — Module 02](../03-硬件感知LLM量化/Lecture-02.md) derives NVFP4's *real* cost as 4.5 bits — not the naive 4-bit / 0.5-byte assumption above — because of its per-16-element E4M3 block scale:

```text
   NVFP4, real (0.5625 bytes/param)  :  320e9 × 0.5625  =  180 GB  =  167.64 GiB
```

**Cross-check against the reported deployment.** The `sparkinfer-frontier` repository reports approximately **185 GiB of model data**, with a reported residency of **24.8 GB per GPU** across 8 GPUs. Two consistency checks, both worth running on any reported figure before trusting it:

```text
   uniformity check:   185 GiB / 8  =  23.125 GiB/GPU  =  24.83 GB/GPU
                       ✓ matches the reported 24.8 GB/GPU almost exactly —
                         consistent with roughly uniform 8-way weight sharding

   format check:       185 GiB reported  ÷  167.64 GiB pure-NVFP4 floor
                       =  1.104×   (a 10.4% overhead)
                       ✓ plausible: block-scale metadata plus embeddings/LM-head/
                         selected attention tensors kept at a higher precision
                         than the bulk NVFP4 body
```

```text
   ┌────────────────────────────────────────────────────────────────┐
   │  These are figures the sparkinfer-frontier repository reports —  │
   │  not measurements taken for this course. Treat the arithmetic    │
   │  above as a CONSISTENCY CHECK on a third-party report, which is  │
   │  a different epistemic status than an independent measurement.   │
   │  The check passing is reassuring; it is not the same as having   │
   │  measured it yourself.                                            │
   └────────────────────────────────────────────────────────────────┘
```

---

</details>

## 2. KDA 状态：在上下文长度上固定，并非免费

来自 [Module 03 §1](Lecture-03.md)：64 heads × 128×128 状态矩阵，跨 34 个 KDA layer，FP32（循环状态对数值敏感——即使在其余部分都已量化的部署中也要保持较高精度）：

```text
   M_KDA  =  34 layers × 64 heads × 128 × 128 × 4 bytes
          =  142,606,336 bytes
          =  136.0 MiB                    ← for ONE request, ALL KDA layers, ALL heads
```

```text
   ┌────────────────────────────────────────────────────────────────┐
   │  This figure excludes convolution state (Module 04 §3),          │
   │  allocator overhead, and any extra snapshots a speculative-       │
   │  decoding rollback scheme keeps (Module 08 §3). Treat 136 MiB     │
   │  as a floor for one request's KDA state, not the complete cost.   │
   └────────────────────────────────────────────────────────────────┘
```

在 8 个 GPU 之间做理想 head 切分（每个 GPU 拥有 64 个 head 中的 8 个，每一层都如此）：

```text
   136 MiB / 8  =  17 MiB / GPU / request
```

单独看很小——但**并发会直接把它成倍放大**（100 个并发请求 → 仅 KDA 状态就要 1.7 GiB/GPU），而任何对多个投机前缀做检查点的回滚方案（[Module 08 §3](Lecture-08.md)）会再次把它放大。这一项要按你实际的目标并发量来估算，而不是按单个请求。

---

## 3. MLA 缓存：随上下文长度线性增长

来自 [Module 05 §2](Lecture-05.md)：512 宽 BF16 latent，每 token、每 MLA layer，跨全部 11 个 MLA layer：

```text
   M_MLA(T)  =  11 layers × T tokens × 512 × 2 bytes
```

| 上下文 `T` | 逻辑 latent 缓存，单请求 |
|---:|---:|
| 128K = 131,072 | **1.375 GiB** |
| 512K = 524,288 | **5.5 GiB** |
| 1M = 1,048,576（配置的最大值） | **11 GiB** |

```text
   ┌────────────────────────────────────────────────────────────────┐
   │  These are calculated LOGICAL cache sizes from the architecture, │
   │  before replication (§4), metadata, or allocator effects — the   │
   │  same "logical vs. actual resident" gap Module 09 of the         │
   │  Quantization course draws for the KV cache generally. Treat     │
   │  these numbers as the floor a real deployment must clear, not     │
   │  the number you'll actually observe in nvidia-smi.                │
   └────────────────────────────────────────────────────────────────┘
```

---

## 4. 陷阱：不能把每一项都除以 8

构建这份预算时最具破坏性的错误，就是假设统一的张量并行切分像适用于权重那样适用于每一项：

```text
   WRONG:   M_cache,per-GPU  =  M_cache,logical / 8
```

**为什么这对 MLA 尤其行不通。** 传统的 head 并行张量并行会把 *head* 切分到各 GPU——每个 GPU 拥有一部分 attention head，且只需要 *自己* 那些 head 的 K/V 数据。但 MLA 的全部内存优势（[Module 05 §2](Lecture-05.md)）来自只缓存一份 **共享 latent** `c_t`，每个 head 的 key 和 value 都*从*它重建而来——latent 不是某个 head 专属的。一个把 head 切分到 8 个 GPU 的 head 并行方案，可能要求 **每个参与的 GPU 都持有完整的共享 latent 表示**，因为取决于 attention 计算如何划分，任何 GPU 都可能需要从它重建任何 head 的 key/value。这种针对张量并行 latent attention 的复制要求，正是文献中针对这一特定扩展模式所记录的问题——它不是一个假设的边缘情况，而是朴素的 head 并行 MLA 实现的默认行为。

```text
   weights            →  genuinely shardable — each GPU holds a DISJOINT slice
                          (this is what makes the §1 ÷8 check work)

   MLA latent cache   →  can require FULL REPLICATION across GPUs under a
                          naive head-parallel scheme — NOT automatically ÷8

   KDA state          →  shardable by HEAD, similar to weights, IF the
                          implementation actually partitions heads across
                          GPUs rather than replicating the full state
                          (verify this against your actual runtime — don't
                          assume it from the weight-sharding pattern alone)
```

**在写内存预算之前，对每一项都要问的问题：** 它是切分的、复制的、依赖请求的，还是瞬态的？从权重如何切分去猜，正是这个陷阱之所以得名的错误。

---


<details>
<summary>English original</summary>

**2. KDA state: fixed in context length, not free**

From [Module 03 §1](Lecture-03.md): 64 heads × 128×128 state matrices, across 34 KDA layers, at FP32 (recurrent state is numerically sensitive — keep it at higher precision even in an otherwise-quantized deployment):

```text
   M_KDA  =  34 layers × 64 heads × 128 × 128 × 4 bytes
          =  142,606,336 bytes
          =  136.0 MiB                    ← for ONE request, ALL KDA layers, ALL heads
```

```text
   ┌────────────────────────────────────────────────────────────────┐
   │  This figure excludes convolution state (Module 04 §3),          │
   │  allocator overhead, and any extra snapshots a speculative-       │
   │  decoding rollback scheme keeps (Module 08 §3). Treat 136 MiB     │
   │  as a floor for one request's KDA state, not the complete cost.   │
   └────────────────────────────────────────────────────────────────┘
```

With ideal head-sharding across 8 GPUs (each GPU owns 8 of the 64 heads, for every layer):

```text
   136 MiB / 8  =  17 MiB / GPU / request
```

Small in isolation — but **concurrency multiplies it directly** (100 concurrent requests → 1.7 GiB/GPU just for KDA state), and any rollback scheme that checkpoints multiple speculative prefixes ([Module 08 §3](Lecture-08.md)) multiplies it again. Size this term against your actual target concurrency, not against one request.

---

**3. MLA cache: linear in context length**

From [Module 05 §2](Lecture-05.md): 512-wide BF16 latent, per token, per MLA layer, across all 11 MLA layers:

```text
   M_MLA(T)  =  11 layers × T tokens × 512 × 2 bytes
```

| Context `T` | Logical latent cache, one request |
|---:|---:|
| 128K = 131,072 | **1.375 GiB** |
| 512K = 524,288 | **5.5 GiB** |
| 1M = 1,048,576 (the configured max) | **11 GiB** |

```text
   ┌────────────────────────────────────────────────────────────────┐
   │  These are calculated LOGICAL cache sizes from the architecture, │
   │  before replication (§4), metadata, or allocator effects — the   │
   │  same "logical vs. actual resident" gap Module 09 of the         │
   │  Quantization course draws for the KV cache generally. Treat     │
   │  these numbers as the floor a real deployment must clear, not     │
   │  the number you'll actually observe in nvidia-smi.                │
   └────────────────────────────────────────────────────────────────┘
```

---

**4. The trap: you cannot divide every term by 8**

The single most damaging mistake in building this budget is assuming uniform tensor-parallel sharding applies to every term the way it applies to weights:

```text
   WRONG:   M_cache,per-GPU  =  M_cache,logical / 8
```

**Why this fails specifically for MLA.** Conventional head-parallel tensor parallelism shards *heads* across GPUs — each GPU owns a subset of attention heads and only needs the K/V data for *its* heads. But MLA's entire memory advantage ([Module 05 §2](Lecture-05.md)) comes from caching one **shared latent** `c_t` that every head's key and value get reconstructed *from* — the latent is not head-specific. A head-parallel scheme that shards heads across 8 GPUs can require **every participating GPU to hold the full shared latent representation**, because any GPU might need to reconstruct any head's key/value from it, depending on how the attention computation is partitioned. This replication requirement for tensor-parallel latent attention is exactly the problem documented in the literature on this specific scaling pattern — it is not a hypothetical edge case, it is the default behavior of a naive head-parallel MLA implementation.

```text
   weights            →  genuinely shardable — each GPU holds a DISJOINT slice
                          (this is what makes the §1 ÷8 check work)

   MLA latent cache   →  can require FULL REPLICATION across GPUs under a
                          naive head-parallel scheme — NOT automatically ÷8

   KDA state          →  shardable by HEAD, similar to weights, IF the
                          implementation actually partitions heads across
                          GPUs rather than replicating the full state
                          (verify this against your actual runtime — don't
                          assume it from the weight-sharding pattern alone)
```

**The question to ask for every single term, before writing a memory budget:** is it sharded, replicated, request-dependent, or transient? Guessing from how weights shard is exactly the mistake this trap is named for.

---

</details>

## 5. 完整的 per-GPU 方程

```text
   M_GPU  =  M_weights,local
           +  M_MLA,local
           +  M_KDA,local
           +  M_indexer
           +  M_workspace
           +  M_graphs
           +  M_allocator
```

| 项 | 典型分类 | 推导来源 |
|---|---|---|
| `M_weights,local` | 分片（每个 GPU 上不重叠的切片） | §1 |
| `M_MLA,local` | **需核实——可能是复制而非分片**（§4） | §3 |
| `M_KDA,local` | 按 head 分片 **如果你的 runtime 对 head 做了划分**；需核实 | §2 |
| `M_indexer` | 依赖请求——来自 [Module 06](Lecture-06.md) 的 DSA 选择元数据，按并发请求数缩放 | Module 06 |
| `M_workspace` | 瞬态——attention/FFN 中间 buffer，用后即释放 | — |
| `M_graphs` | 使用 CUDA Graph capture 做 decode（逐 token 生成阶段）时的固定开销 | — |
| `M_allocator` | 固定开销——碎片、预留块 | — |

```text
   ┌────────────────────────────────────────────────────────────────┐
   │  Write this equation in LOCAL (per-GPU) allocations, term by     │
   │  term — never start from a global total and divide. A budget     │
   │  built by dividing an aggregate is only correct for the terms     │
   │  that happen to shard uniformly, and §4 just showed you MLA's     │
   │  cache is not guaranteed to be one of them.                        │
   └────────────────────────────────────────────────────────────────┘
```

### 实战示例：1M 上下文下该部署的可行性

```text
   per-GPU weights (from §1's cross-check)         ≈  23.1 GiB
   MLA cache at 1M context, IF REPLICATED (§4)      =  11.0 GiB   ← on EVERY GPU, not ÷8
   KDA state, 100 concurrent requests (§2)          ≈   1.7 GiB
   indexer + workspace + graphs + allocator         ≈   (measure — don't guess)
   ─────────────────────────────────────────────────────────────
   running total, before workspace/indexer          ≈  35.8 GiB   >  32 GiB card capacity
```

这一行算术就是本模块的全部要点：**一个天真地把 MLA 缓存“除以 8”的预算版本，本会显示出充裕的余量**（11 GiB ÷ 8 ≈ 1.4 GiB，使累计总量停在 26.2 GiB 附近）。正确的、未做复制的核算表明，该部署**在 32 GB 卡上已经超预算，而此时 workspace、indexer 开销，乃至第二个并发请求所需的 KDA state 都还没有被计入。**你的实际 runtime 是否以这种方式复制 MLA 缓存，是一个关于其具体 tensor-parallel 实现的经验性问题——但一旦猜错，误差的方向始终相同：天真的除法会让不可行的部署看起来可行，而绝不会反过来。

---

## 检查点

现在你应该能够：

1. 在三种精度下计算权重存储下界，并把报告的真实世界数值与 NVFP4 特有的 0.5625 bytes/param 速率核对一致。
2. 针对给定的并发度计算 KDA state 预算，并针对给定的上下文长度计算 MLA 缓存。
3. 准确说明 MLA 的共享 latent 为何打破了权重所满足的那种天真 tensor-parallel `÷8` 假设。
4. 构建完整的 per-GPU 显存方程，并正确分类每一项。
5. 解释为什么在「复制」与「分片」之间猜错时，失败方向总是相同（低估用量），而绝不会是另一个方向。

---

## 交付

这是 **[capstone ladder](Lecture-12.md) 的 Stage 7**：为你自己的目标部署（上下文长度、并发度与 GPU 数量）构建完整的 per-GPU 显存方程，逐项预测 per-GPU 用量，然后在负载下测量实际的 per-GPU 分配。对于预测与实测之差超过细微幅度的每一项，**找出你搞错的是哪一种分类（分片/复制/依赖请求/瞬态）**——把这个差异解释清楚，比第一次就让预测对上更有价值。

---

## 时效性

* **不随时间变化：** 分片与复制的分类准则，以及 §4 中那个具体的 tensor-parallel latent-attention 复制陷阱。
* **与检查点及部署相关：** 320B/18B 的参数切分（[Module 01](Lecture-01.md)）、34-KDA/11-MLA 的 layer 切分，以及 `sparkinfer-frontier` 185 GiB / 24.8 GB-per-GPU 这两个数字，分别引自该检查点配置与文中具名的仓库——在依赖它们做容量决策之前，务必对照最新来源重新核实这两者，并且始终重新推导你具体的 runtime 是复制还是分片 MLA 缓存，而不是对任一种情形直接假设。

---

**下一章：** [Module 10 — Kernel Roofline（性能上界模型）与推理服务决策 →](Lecture-10.md)


<details>
<summary>English original</summary>

**5. The complete per-GPU equation**

```text
   M_GPU  =  M_weights,local
           +  M_MLA,local
           +  M_KDA,local
           +  M_indexer
           +  M_workspace
           +  M_graphs
           +  M_allocator
```

| Term | Typical classification | Where it's derived |
|---|---|---|
| `M_weights,local` | sharded (disjoint slice per GPU) | §1 |
| `M_MLA,local` | **verify — may be REPLICATED, not sharded** (§4) | §3 |
| `M_KDA,local` | sharded by head **if your runtime partitions heads**; verify | §2 |
| `M_indexer` | request-dependent — the DSA selection metadata from [Module 06](Lecture-06.md), scaled by concurrent requests | Module 06 |
| `M_workspace` | transient — attention/FFN intermediate buffers, freed after use | — |
| `M_graphs` | fixed overhead if using CUDA graph capture for decode | — |
| `M_allocator` | fixed overhead — fragmentation, reserved blocks | — |

```text
   ┌────────────────────────────────────────────────────────────────┐
   │  Write this equation in LOCAL (per-GPU) allocations, term by     │
   │  term — never start from a global total and divide. A budget     │
   │  built by dividing an aggregate is only correct for the terms     │
   │  that happen to shard uniformly, and §4 just showed you MLA's     │
   │  cache is not guaranteed to be one of them.                        │
   └────────────────────────────────────────────────────────────────┘
```

**Worked example: feasibility at 1M context, this deployment**

```text
   per-GPU weights (from §1's cross-check)         ≈  23.1 GiB
   MLA cache at 1M context, IF REPLICATED (§4)      =  11.0 GiB   ← on EVERY GPU, not ÷8
   KDA state, 100 concurrent requests (§2)          ≈   1.7 GiB
   indexer + workspace + graphs + allocator         ≈   (measure — don't guess)
   ─────────────────────────────────────────────────────────────
   running total, before workspace/indexer          ≈  35.8 GiB   >  32 GiB card capacity
```

That single arithmetic line is the entire point of this module: **a naive "divide the MLA cache by 8" version of this budget would have shown comfortable headroom** (11 GiB ÷ 8 ≈ 1.4 GiB, leaving the running total near 26.2 GiB). The correct, unreplicated accounting shows the deployment is **already over budget on a 32 GB card before workspace, indexer overhead, or a second concurrent request's worth of KDA state are even counted.** Whether your actual runtime replicates the MLA cache this way is an empirical question about its specific tensor-parallel implementation — but the direction of the error if you guess wrong is always the same: naive division makes an infeasible deployment look feasible, never the reverse.

---

**Checkpoint**

You should now be able to:

1. Compute weight-storage lower bounds at three precisions and reconcile a reported real-world figure against the NVFP4-specific 0.5625 bytes/param rate.
2. Compute the KDA state budget for a stated concurrency and the MLA cache for a stated context length.
3. State exactly why MLA's shared latent breaks the naive tensor-parallel `÷8` assumption that weights satisfy.
4. Build a complete per-GPU memory equation and correctly classify each term.
5. Explain why guessing "replicated" versus "sharded" wrong always fails in the same direction (understating usage), never the other.

---

**Ship it**

This is **Stage 7 of the [capstone ladder](Lecture-12.md)**: build the full per-GPU memory equation for your own target deployment (context length, concurrency, and GPU count), predict per-GPU usage term by term, then measure actual per-GPU allocation under load. For every term where predicted and measured disagree by more than a small margin, **identify which classification (sharded/replicated/request-dependent/transient) you got wrong** — that discrepancy, explained, is worth more than the prediction matching on the first try.

---

**Current as of**

* **Timeless:** the sharded-vs-replicated classification discipline, and the specific tensor-parallel latent-attention replication trap in §4.
* **Checkpoint-and-deployment-specific:** the 320B/18B parameter split ([Module 01](Lecture-01.md)), the 34-KDA/11-MLA layer split, and the `sparkinfer-frontier` 185 GiB / 24.8 GB-per-GPU figures are cited from the checkpoint configuration and the named repository respectively — re-verify both against current sources before relying on them for a capacity decision, and always re-derive whether your specific runtime replicates or shards the MLA cache rather than assuming either.

---

**Next:** [Module 10 — Kernel Roofline & Serving Decisions →](Lecture-10.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/GLM-5.3-Flash Architecture Mastery/Lecture-09.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/GLM-5.3-Flash%20Architecture%20Mastery/Lecture-09.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
