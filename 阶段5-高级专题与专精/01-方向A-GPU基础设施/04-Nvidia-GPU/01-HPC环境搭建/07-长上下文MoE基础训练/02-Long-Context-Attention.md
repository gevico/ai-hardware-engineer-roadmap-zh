---
title: 模块 02 — 长上下文 attention 机制
description: 模块 02 — 长上下文 attention 机制
---

# 模块 02 — 长上下文 attention 机制

**Parent:** [长上下文 MoE（混合专家模型）基础训练](README.md)

**一句话目的：** 让长上下文 attention 在物理上可落地：内层 kernel 用 FlashAttention，块间 reduction 用序列并行，把一条序列切分到多张 GPU 上用上下文并行，再加上作为最后手段的激活重计算。

**前置要求：** 模块 01。FlashAttention 课程第 1–3 讲。

**产物：** 在单台 8-GPU 节点上跑通带上下文并行 attention 的 FlashAttention benchmark，外加一张跨序列长度的激活显存曲线图，对比 (no CP) 与 (CP=2) 与 (CP=4) 与 (CP=4 + recompute)。

---

## 为什么重要

要训练长上下文模型，必须有三件事协同工作：一个不会把 `N²` scores 物化出来的 kernel，一种能把激活值切分到多张 GPU 上又不让计算串行化的方法，以及一种在激活值仍然过大时多花一点算力塞进 HBM 的办法。这三者各自是独立的技术，各有各的失效模式。本模块以调试真实训练任务所需的深度覆盖这三者。

---

## 心智模型

### Layer 1 — FlashAttention 消去 `N²` 显存项

这些内容在 FlashAttention 课程里已详细讲过。本模块的要点总结：

- 前向：把 `Q, K, V` 分块，使 `S = QKᵀ` 留在 SRAM 中；按 query 行维护 running `(m, ℓ)`；直接产出 `O`。
- 反向：用保存下来的 `LSE` 逐分块重算 `S, P`；HBM 上界同样是 `O(N²d²/M)`。
- 对于长上下文训练，FlashAttention 是必需的 —— 没有它，仅 attention 自身的激活显存就会在 `N ≥ 16K` 时超出 HBM。

实践中，定长 batch 用 `flash_attn_func`，打包后的变长序列用 `flash_attn_varlen_func`。两者都能与序列并行配合使用。

### Layer 2 — 序列并行（Megatron 风格）

对 Transformer 做张量并行时，你会把隐藏维度 `H` 切分到各张 TP GPU 上。而 LayerNorm + dropout + 残差这几条路径**没有**做张量并行 —— 默认情况下它们会在各 TP rank 上复制计算和激活值。

**序列并行（SP）**改变了这一点：把这些 `O(N·H)` 非矩阵乘激活值沿序列维度 `N` 切分，而不是复制。这样每个 TP rank 存的是 `N/TP × H` 的激活值，而不是 `N × H`。代价是列并行算子之前多一次 all-gather、行并行算子之后多一次 reduce-scatter，但激活显存的节省占主导。

在显存意义上，SP 是在 TP 之上的「免费」收益。任何 TP 训练都应为 `N ≥ 8K` 启用它。

### Layer 3 — 上下文并行（CP）：切分一条序列

SP 把非矩阵乘激活值切分到各 TP rank。它**不**切分 attention 计算本身 —— 到了 attention 层，每个 TP rank 看到的仍是完整序列。当 `N` 大到连一个 head 的 attention 激活值都放不下时，就需要把序列本身切分到更多 GPU 上。

这就是**上下文并行**（CP），有时也称为序列维度模型并行。其变体有：

- **Ring attention**：每个 CP rank 持有序列 KV 的一段连续分块。计算 attention 时，KV 分块像环一样在各 rank 间轮转；每个 rank 计算自己的 `Q` 分块与轮访到本地的 `KV` 分块之间的 attention；部分结果用 FlashAttention 的 online-softmax 递推式合并。
- **Striped attention**（一种改进）：不用连续分块，而是交错划分，让每个 rank 拿到条带状的 position 子集。可改善 causal masking 下的负载均衡。
- **DistFlashAttn / Sequence-Parallel FlashAttention**：思路类似，调度与 overlap 模式略有不同。

CP 与 FlashAttention 结合使用：每个 rank 对本地 `Q_local` 与每个轮访来的 `KV_chunk` 跑标准 FlashAttention 前向，然后用与 FlashAttention 课程模块 02 中相同的 `(m, ℓ)` 重缩放来合并。数学上是精确的。

通信开销：每轮 O(N · H)，每层 O(CP_size − 1) 轮。当 CP_size 较小且分块较大时，通信可被计算掩盖。

### Layer 4 — 激活重计算（选择性重计算与完全重计算）

如果还是放不下，就重算。两种做法：

- **选择性重计算**：只存特定的激活值（通常是 attention 的 `Q, K, V, O, LSE`），其余在反向时重算。代价低 —— 约多花 10–20% 的前向 FLOPs。
- **完全重计算**：只存 layer 输入；反向时重算整个 layer 的前向。代价高 —— 约多 30–40% FLOPs。

Megatron 通过 `--recompute-granularity selective|full` 支持这两种方式。在长上下文下，配合 SP+CP，选择性重计算通常就够了。

### 什么时候该用什么

| 症状 | 对策 |
|--------|-----------|
| TP=8、N=8K、不开 SP 时 OOM | 序列并行 |
| TP=8 + SP、N=64K 时 OOM | 上下文并行 CP=2 |
| TP=8 + SP + CP=2、N=256K 时 OOM | 提高 CP，再加选择性重计算 |
| 启用 CP=8 后 TFLOPs 骤降 | 通信没有被掩盖 —— 检查 overlap 设置，增大分块 |

---


<details>
<summary>English original</summary>

**Module 02 — Long-Context Attention Mechanics**

**Parent:** [Long-Context MoE Foundation Training](README.md)

**One-line purpose:** Make long-context attention physically tractable: FlashAttention for the inner kernel, sequence parallelism for the inter-block reduction, context parallelism for splitting one sequence across many GPUs, plus activation recomputation as the last-resort memory tool.

**Prerequisites:** Module 01. FlashAttention course Lectures 1–3.

**Artifact:** A working benchmark of FlashAttention with context-parallel attention on one 8-GPU node, plus an activation-memory plot across sequence lengths comparing (no CP) vs (CP=2) vs (CP=4) vs (CP=4 + recompute).

---

**Why it matters**

You cannot train a long-context model without three things working together: a kernel that does not materialize `N²` scores, a way to split activations across many GPUs without serializing the math, and a way to spend a little extra compute to fit in HBM when activations are still too big. Each of these is a separate technique with separate failure modes. This module covers all three at the level needed to debug a real training run.

---

**Mental model**

**Layer 1 — FlashAttention removes the `N²` memory term**

You covered this in detail in the FlashAttention course. The summary for this module:

- Forward: tile `Q, K, V` so `S = QKᵀ` stays in SRAM; track running `(m, ℓ)` per query row; produce `O` directly.
- Backward: recompute `S, P` per tile using the saved `LSE`; same `O(N²d²/M)` HBM bound.
- For long-context training, FlashAttention is mandatory — without it the activation memory of attention alone exceeds your HBM at `N ≥ 16K`.

In practice you use `flash_attn_func` for fixed-length batches and `flash_attn_varlen_func` for packed variable-length sequences. Both work with sequence parallelism.

**Layer 2 — Sequence parallelism (Megatron-style)**

When you tensor-parallelize a transformer, you split the hidden dimension `H` across TP GPUs. The LayerNorm + dropout + residual paths are **not** tensor-parallelized — by default they replicate computation and activations across TP ranks.

**Sequence parallel (SP)** changes that: split those `O(N·H)` non-matmul activations along the sequence dimension `N` instead of replicating. Now each TP rank stores `N/TP × H` activations instead of `N × H`. You pay an extra all-gather before column-parallel ops and an extra reduce-scatter after row-parallel ops, but the activation memory savings dominate.

SP is "free" memory-wise on top of TP. Enable it on any TP run for `N ≥ 8K`.

**Layer 3 — Context parallelism (CP) for splitting one sequence**

SP splits non-matmul activations across TP. It does **not** split the attention computation itself — every TP rank still sees the full sequence at the attention layer. For `N` so large that even one head's worth of attention activations does not fit, you need to split the sequence itself across additional GPUs.

That is **context parallel** (CP), sometimes called sequence-dimension model parallelism. Variants:

- **Ring attention**: each CP rank owns a contiguous chunk of the sequence's KV. During attention, KV chunks rotate through ranks like a ring; each rank computes attention between its `Q` chunk and the visiting `KV` chunk; partial results are combined with the FlashAttention online-softmax recurrence.
- **Striped attention** (a refinement): instead of contiguous chunks, interleave so each rank has a striped subset of positions. Improves load balance under causal masking.
- **DistFlashAttn / Sequence-Parallel FlashAttention**: similar idea, slightly different scheduling and overlap patterns.

CP combines with FlashAttention: each rank runs the standard FlashAttention forward over its `Q_local` against each visiting `KV_chunk`, then merges via the same `(m, ℓ)` rescale you used in Module 02 of the FlashAttention course. The math is exact.

Communication cost: O(N · H) per round, O(CP_size − 1) rounds per layer. Hidden behind compute if CP_size is small and the chunk is large.

**Layer 4 — Activation recomputation (selective and full)**

When you still cannot fit, recompute. Two flavors:

- **Selective recomputation**: store only specific activations (typically attention's `Q, K, V, O, LSE`), recompute the rest in backward. Cheap — costs ~10–20% extra forward FLOPs.
- **Full recomputation**: store only the layer input; recompute the entire layer forward in backward. Expensive — ~30–40% extra FLOPs.

Megatron supports both via `--recompute-granularity selective|full`. At long context, selective recomputation is usually enough alongside SP+CP.

**When to reach for what**

| Symptom | Reach for |
|--------|-----------|
| OOM at TP=8, N=8K, no SP | Sequence parallel |
| OOM at TP=8 + SP, N=64K | Context parallel CP=2 |
| OOM at TP=8 + SP + CP=2, N=256K | Increase CP, then add selective recomputation |
| TFLOPs collapse after enabling CP=8 | Communication is unhidden — check overlap settings, increase chunk size |

---

</details>

## 构建

### 1. 单台 8-GPU 节点上的 FlashAttention

```bash
# Megatron-LM example, 8-GPU node, TP=8, no PP, no DP
torchrun --nproc-per-node=8 pretrain_gpt.py \
  --num-layers 32 --hidden-size 4096 --num-attention-heads 32 \
  --seq-length 32768 --max-position-embeddings 32768 \
  --tensor-model-parallel-size 8 \
  --use-flash-attn --bf16 \
  --micro-batch-size 1 --global-batch-size 8 \
  --train-iters 20 --log-interval 1 \
  --data-path mock --tokenizer-type Llama3Tokenizer
```

记录每次迭代的耗时和每 GPU 峰值内存。这就是你的基线。

### 2. 加入序列并行

```bash
# Add to the command above
  --sequence-parallel
```

在 N=32K 时峰值内存应下降约 20–30%。每次迭代耗时应在噪声范围内。

### 3. 加入上下文并行

```bash
# Reduce TP to 4, add CP=2 — keeps TP×CP=8
  --tensor-model-parallel-size 4 \
  --context-parallel-size 2 \
  --sequence-parallel
```

把序列长度继续推高（`--seq-length 131072`）。没有 CP 就会 OOM。CP=2 时应该放得下。测量每次迭代耗时和峰值内存。与相同 N 下无 CP 的点对比作图（前提是无 CP 的运行此时还跑得起来）。

### 4. 加入选择性重计算

```bash
  --recompute-granularity selective \
  --recompute-method block
```

应能再换来 20–30% 的内存，代价是约 10–15% 的额外前向时间。只在 SP+CP 不够用时使用。

### 5. 激活值内存曲线

对每个试过的 (N, CP, recompute) 组合，记录 GPU 峰值内存。作图：

```
y = peak memory (GB)
x = sequence length
lines = (CP=1 no recompute, CP=1 recompute, CP=4, CP=4 recompute)
```

应当看到无 CP 的曲线最先变陡（在更小的 N 就 OOM），CP=4 保持平坦的时间长得多，而 recompute 在固定 N 下把两条线都压低。

---

## 在真实技术栈中使用

- **NeMo Megatron Bridge** 把这一切封装起来 —— 其长上下文技能页给出了按模型规模的 `seq-length × CP × recompute × precision` 组合 recipe。
- **Megatron-LM** 是底层库，包含实际的 `--context-parallel-size` flag 和 ring-attention 实现。
- **DeepSpeed-Ulysses** 是另一种序列并行方案，沿 **head** 维度而非序列维度切分；某些 shape 下通信量更低，在极长上下文下有不同的权衡。

此前参与的 cacheon-sglang-miner 项目在*推理*侧长上下文使用 FlashInfer。训练侧版本（Megatron CP）结构相似，但针对反向和更大的 micro-batch 做了调优。

---

## 测量

对 sweep 中的每个配置：

- 每次迭代的 wall-clock（10 次预热后取最后 10 次迭代的中位数）。
- 每 GPU 分配的峰值 HBM。
- 达到的每 GPU TFLOPS（`model_FLOPs / wall_clock / GPUs`）。
- 通信时间占 step 时间的比例（开启 `--log-throughput` 时 Megatron 日志会报告）。

健康的 CP=4 长上下文运行，在相同 N 下每 GPU TFLOPS 应保持在 CP=1 基线的 20% 以内。若通信超过 step 时间的 30%，说明 CP chunk 太小，或 `NCCL_P2P_LEVEL` / `NCCL_NVLS_ENABLE` 配置有误。

---

## 交付

提交到 `lcm-course/`：

1. `attention_bench.sh` —— 你的三条命令及由此得到的 timing/内存日志。
2. `activation_memory.csv` 和 `activation_memory.png` —— 上面描述的曲线。
3. `notes_attention.md` —— 就 FlashAttention、SP、CP、选择性重计算各写一段，附上运行中的 OOM 表。

现在你有了具体数字，清楚每个长上下文手段在你硬件上的代价。

---

## 相关页面

- [Module 01 — 为什么长上下文很难](01-Long-Context-Bottlenecks.md)
- [Module 03 — 位置编码](03-Position-Encoding.md)
- [Module 09 — 分布式训练基础设施](09-Distributed-Training-Infrastructure.md)
- [FlashAttention 课程 — 第 3 讲](../../../../../阶段4-路线C-深度学习推理优化/02-内核工程/01-FlashAttention课程/第03讲-FlashAttention-1算法.md)
- Megatron-LM：<https://github.com/NVIDIA/Megatron-LM>
- Ring Attention 论文：<https://arxiv.org/abs/2310.01889>


<details>
<summary>English original</summary>

**Build it**

**1. FlashAttention on a single 8-GPU node**

```bash
# Megatron-LM example, 8-GPU node, TP=8, no PP, no DP
torchrun --nproc-per-node=8 pretrain_gpt.py \
  --num-layers 32 --hidden-size 4096 --num-attention-heads 32 \
  --seq-length 32768 --max-position-embeddings 32768 \
  --tensor-model-parallel-size 8 \
  --use-flash-attn --bf16 \
  --micro-batch-size 1 --global-batch-size 8 \
  --train-iters 20 --log-interval 1 \
  --data-path mock --tokenizer-type Llama3Tokenizer
```

Capture the per-iteration time and the per-GPU peak memory. This is your baseline.

**2. Add sequence parallel**

```bash
# Add to the command above
  --sequence-parallel
```

Peak memory should drop ~20–30% at N=32K. Per-iter time should be within noise.

**3. Add context parallel**

```bash
# Reduce TP to 4, add CP=2 — keeps TP×CP=8
  --tensor-model-parallel-size 4 \
  --context-parallel-size 2 \
  --sequence-parallel
```

Push the sequence length further (`--seq-length 131072`). Without CP this would OOM. With CP=2 it should fit. Measure per-iter time and peak memory. Plot vs the no-CP point at the same N (if the no-CP run is now feasible at all).

**4. Add selective recomputation**

```bash
  --recompute-granularity selective \
  --recompute-method block
```

Should buy another 20–30% memory at the cost of ~10–15% extra forward time. Use only when SP+CP is not enough.

**5. The activation memory curve**

For each (N, CP, recompute) combination you tried, record peak GPU memory. Plot:

```
y = peak memory (GB)
x = sequence length
lines = (CP=1 no recompute, CP=1 recompute, CP=4, CP=4 recompute)
```

You should see the no-CP line going vertical first (OOMs at smaller N), CP=4 staying flat much longer, and recompute pushing both lines down at a fixed N.

---

**Use it in the real stack**

- **NeMo Megatron Bridge** wraps all of this — its long-context skill page gives recipes for `seq-length × CP × recompute × precision` combinations per model size.
- **Megatron-LM** is the underlying library with the actual `--context-parallel-size` flag and ring-attention implementation.
- **DeepSpeed-Ulysses** is an alternative sequence-parallel scheme that splits along the **head** dimension instead of the sequence dimension; lower comm volume for some shapes, different trade-offs at very long context.

The cacheon-sglang-miner project we worked on uses FlashInfer for *inference*-side long context. The training-side version (Megatron CP) is structurally similar but tuned for backward + bigger micro-batches.

---

**Measure it**

For each configuration in your sweep:

- Per-iter wall-clock (median over the last 10 iterations after 10 warmup).
- Per-GPU peak HBM allocated.
- Achieved per-GPU TFLOPS (`model_FLOPs / wall_clock / GPUs`).
- Communication time as fraction of step time (Megatron logs report this when `--log-throughput` is on).

A healthy long-context run with CP=4 should hold per-GPU TFLOPS within 20% of the CP=1 baseline at the same N. If communication exceeds 30% of step time, your CP chunk size is too small or `NCCL_P2P_LEVEL` / `NCCL_NVLS_ENABLE` is misconfigured.

---

**Ship it**

Drop into `lcm-course/`:

1. `attention_bench.sh` — your three commands and the resulting timing/memory log.
2. `activation_memory.csv` and `activation_memory.png` — the curve described above.
3. `notes_attention.md` — one paragraph each on FlashAttention, SP, CP, selective recompute, plus the OOM table from your runs.

You now have, in concrete numbers, the cost of each long-context lever on your hardware.

---

**Related pages**

- [Module 01 — Why long context is hard](01-Long-Context-Bottlenecks.md)
- [Module 03 — Position encoding](03-Position-Encoding.md)
- [Module 09 — Distributed training infrastructure](09-Distributed-Training-Infrastructure.md)
- [FlashAttention Course — Lecture 3](../../../../../阶段4-路线C-深度学习推理优化/02-内核工程/01-FlashAttention课程/第03讲-FlashAttention-1算法.md)
- Megatron-LM: <https://github.com/NVIDIA/Megatron-LM>
- Ring Attention paper: <https://arxiv.org/abs/2310.01889>

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Long-Context-MoE-Foundation-Training/02-Long-Context-Attention.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Long-Context-MoE-Foundation-Training/02-Long-Context-Attention.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
