---
title: 模块 08 —— 结合长上下文与 MoE
description: 模块 08 —— 结合长上下文与 MoE
---

# 模块 08 —— 结合长上下文与 MoE

**父级：** [Long-Context MoE Foundation Training](README.md)

**一句话目的：** 设计一套并行 mesh 布局，干净地结合张量并行、序列并行、上下文并行与专家并行，预测当你扩展上下文长度或专家数量时通信瓶颈会移向何处，并选出与你的硬件相匹配的权衡。

**前置要求：** 模块 02 和 05。熟悉 NCCL 集合通信与带宽计算。

**产物：** 一份针对真实 64×H200 集群的决策表，为某一种模型形状选择 TP × SP × CP × EP × DP，外加按集合通信类型划分的通信开销拆解。

---

## 为什么重要

长上下文与 MoE 都有可观的通信开销。若简单地把两者合在一起，这些开销会相互影响：EP 越大，每层 all-to-all 的通信量越大；CP 越大，每层 ring attention 的轮数越多；两者都需要 NVLink。若不刻意规划 mesh 布局，训练的大部分时间都会耗在 NCCL 里。

---

## 心智模型

### 完整的并行 mesh 维度

在现代 Megatron 风格的训练器中：

| 维度 | 切分维度 | 主导集合通信 | 适合跑在 |
|-----------|--------------|---------------------|--------------|
| 数据（DP） | 批 | all-reduce（梯度） | 任意链路，可被 overlap 掩盖 |
| 张量（TP） | 隐藏维 | 每个矩阵乘的 all-gather + reduce-scatter | 仅 NVLink |
| 序列（SP） | 序列（非矩阵乘） | 搭 TP 集合通信的便车 | 仅 NVLink |
| 上下文（CP） | 序列（attention） | KV 的 ring all-gather | `CP ≤ 8` 用 NVLink；CP 更大时 ring 可走 InfiniBand |
| 专家（EP） | 专家 | all-to-all（dispatch + combine） | 仅 NVLink；跨节点开销爆炸 |
| 流水线（PP） | 层 | 点对点激活值 | 任意链路，可被 overlap 掩盖 |

所有人都会撞上的约束：TP、EP 和 CP 都想要 NVLink；节点内 NVLink 宽度是有限的。你必须选择让 TP/EP/CP 中的哪两个共享节点。

### 各维度的通信量（每层、每个微批）

令 `T = tokens per rank per layer`（序列长度 × 微批）。

- **激活值的 TP all-gather**：每个矩阵乘 `T · H · (TP - 1) / TP` 字节（通常每层 4 个矩阵乘）。
- **KV 的 CP ring**：每层 ring 步 `T_local · 2 · H_kv · D · (CP - 1)` 字节。
- **EP all-to-all（dispatch + combine）**：每次 all-to-all `T · k · H · 2` × 每层 2 次。
- **梯度的 DP all-reduce**：每步 `model_params · 4` 字节（fp32 梯度累加器），在 `micro_batches_per_step` 上摊销。

对于 32K 上下文、64 专家、8B 级模型，在 `T = 32K, H = 4096, k = 2, H_kv = 8, D = 128, layers = 32` 下：

| 集合通信 | 字节 / 层 / 微批 | 合计 / 微批 |
|------------|------------------------------|----------------------|
| TP all-gather | ~31 MB × 4 = 124 MB | 4.0 GB |
| CP ring（CP=4） | ~24 MB | 0.77 GB |
| EP all-to-all（EP=8） | ~104 MB × 2 = 208 MB | 6.7 GB |
| DP all-reduce | (model_params × 4) / micro_batches | 视情况而定 |

EP all-to-all 占主导。CP 是第二梯队。TP 与计算 overlap 得很好。这一规律在大多数真实配置下都成立。

### Mesh 布局配方

#### 单个 8×H200 节点（无跨节点）

| 模型规模 | 序列长度 | 布局 |
|-----------|----------|--------|
| 7B dense | 32K | TP=8, SP, 无 CP, 无 EP |
| 8×7B MoE | 32K | TP=4, EP=2, SP |
| 7B dense | 128K | TP=4, CP=2, SP, recompute |
| 8×7B MoE | 128K | TP=2, EP=2, CP=2, SP（紧凑；考虑 FP8） |

#### 8 节点 × 8 = 64×H200 集群（节点内 NVSwitch，节点间 InfiniBand）

| 模型规模 | 序列长度 | 布局 |
|-----------|----------|--------|
| 70B dense | 32K | TP=8 节点内, PP=4 跨节点, DP=2 |
| 70B dense | 128K | TP=8, PP=4, CP=2（CP 在节点内轮转）, DP=1 |
| 64 专家 8×7B MoE | 32K | TP=4, EP=2 节点内, DP=8 跨节点 |
| 64 专家 70B MoE | 32K | TP=8, EP=8（各自节点内）, PP=4 跨节点, DP=2 |
| 64 专家 70B MoE | 128K | TP=8, EP=8, PP=4, CP=2 —— 通信紧张，考虑把 EP 降到 4 |

原则是：**跨节点跳数应当服务于 PP 和 DP，而不是 EP 或 CP**。PP 和 DP 的 overlap 很好；EP 和 CP 还不行（目前）。

### 规模扩展时瓶颈如何移动

固定模型规模，扩展**上下文**：

- 4K → 32K：TP+SP 就够了。
- 32K → 128K：引入 CP；激活值显存成为限制因素。
- 128K → 1M：CP 占主导，必须激进地做 overlap，考虑对 KV 用混合精度（FP8）。

固定模型规模，扩展**专家数**：

- 8 → 32 专家：EP 在节点内，all-to-all 仍然便宜。
- 32 → 256 专家：EP 跨节点，all-to-all 占主导。考虑专家共享技巧（DeepSeek-V3 的 shared expert）或替代 routing 方案。
- 256+ 专家：routing 多样性是比系统更大的问题；通常需要专家并行 + 拓扑感知的 routing。

**同时**扩展上下文与专家数：两者会把彼此推入更紧张的境地。一个现实的前沿 MoE 长上下文模型会在两者之间取得平衡 —— `~64–128 experts` 配 `~32–128K` 上下文，而非 `1024 experts + 1M context`。


<details>
<summary>English original</summary>

**Module 08 — Combining Long-Context and MoE**

**Parent:** [Long-Context MoE Foundation Training](README.md)

**One-line purpose:** Design a parallel-mesh layout that combines tensor, sequence, context, and expert parallelism cleanly, predict where the communication bottleneck moves as you scale either context length or expert count, and pick the trade-off that matches your hardware.

**Prerequisites:** Modules 02 and 05. Comfort with NCCL collectives and bandwidth math.

**Artifact:** A decision table for a realistic 64×H200 cluster choosing TP × SP × CP × EP × DP for one model shape, plus a communication-cost breakdown by collective type.

---

**Why it matters**

Long context and MoE both have substantial communication costs. Combined naively, those costs interact: more EP means more all-to-all volume per layer; more CP means more ring-attention rounds per layer; both want NVLink. If you do not lay out the mesh deliberately, you will spend most of your training time in NCCL.

---

**Mental model**

**The full parallel-mesh dimensions**

In a modern Megatron-style trainer:

| Dimension | Splits along | Dominant collective | Goes well on |
|-----------|--------------|---------------------|--------------|
| Data (DP) | batch | all-reduce (gradients) | any link, hidden by overlap |
| Tensor (TP) | hidden | all-gather + reduce-scatter per matmul | NVLink only |
| Sequence (SP) | sequence (non-matmul) | piggybacks on TP collectives | NVLink only |
| Context (CP) | sequence (attention) | ring all-gather of KV | NVLink for `CP ≤ 8`; InfiniBand viable for ring at larger CP |
| Expert (EP) | experts | all-to-all (dispatch + combine) | NVLink only; cross-node costs explode |
| Pipeline (PP) | layers | point-to-point activations | any link, hidden by overlap |

The constraint everyone hits: TP, EP, and CP all want NVLink; intra-node NVLink width is finite. You must choose which two of TP/EP/CP get to share the node.

**Communication volume by dimension (per layer, per micro-batch)**

Let `T = tokens per rank per layer` (sequence length × micro-batch).

- **TP all-gather of activations**: `T · H · (TP - 1) / TP` bytes per matmul (typically 4 matmuls per layer).
- **CP ring of KV**: `T_local · 2 · H_kv · D · (CP - 1)` bytes per layer for the ring step.
- **EP all-to-all (dispatch + combine)**: `T · k · H · 2` per all-to-all × 2 per layer.
- **DP all-reduce of gradients**: `model_params · 4` bytes per step (fp32 grad accumulator), amortized over `micro_batches_per_step`.

For a 32K-context, 64-expert, 8B-class model at `T = 32K, H = 4096, k = 2, H_kv = 8, D = 128, layers = 32`:

| Collective | Bytes / layer / micro-batch | Total / micro-batch |
|------------|------------------------------|----------------------|
| TP all-gather | ~31 MB × 4 = 124 MB | 4.0 GB |
| CP ring (CP=4) | ~24 MB | 0.77 GB |
| EP all-to-all (EP=8) | ~104 MB × 2 = 208 MB | 6.7 GB |
| DP all-reduce | (model_params × 4) / micro_batches | varies |

EP all-to-all dominates. CP is second-tier. TP is well-overlapped with compute. This pattern holds for most realistic configs.

**Mesh layout recipes**

**Single 8×H200 node (no cross-node)**

| Model size | Sequence | Layout |
|-----------|----------|--------|
| 7B dense | 32K | TP=8, SP, no CP, no EP |
| 8×7B MoE | 32K | TP=4, EP=2, SP |
| 7B dense | 128K | TP=4, CP=2, SP, recompute |
| 8×7B MoE | 128K | TP=2, EP=2, CP=2, SP (tight; consider FP8) |

**8-node × 8 = 64×H200 cluster (NVSwitch intra-node, InfiniBand inter-node)**

| Model size | Sequence | Layout |
|-----------|----------|--------|
| 70B dense | 32K | TP=8 intra-node, PP=4 across nodes, DP=2 |
| 70B dense | 128K | TP=8, PP=4, CP=2 (CP rotates within node), DP=1 |
| 64-expert 8×7B MoE | 32K | TP=4, EP=2 intra-node, DP=8 across nodes |
| 64-expert 70B MoE | 32K | TP=8, EP=8 (each intra-node), PP=4 across nodes, DP=2 |
| 64-expert 70B MoE | 128K | TP=8, EP=8, PP=4, CP=2 — communication-tight, consider reducing EP to 4 |

The principle: **cross-node hops should serve PP and DP, not EP or CP**. PP and DP are well-overlapped; EP and CP are not (yet).

**Where the bottleneck moves as you scale**

Hold model size fixed and scale **context**:

- 4K → 32K: TP+SP is enough.
- 32K → 128K: introduce CP; activation memory becomes the limiter.
- 128K → 1M: CP dominates, must overlap aggressively, consider mixed precision (FP8) for KV.

Hold model size fixed and scale **experts**:

- 8 → 32 experts: EP intra-node, all-to-all stays cheap.
- 32 → 256 experts: EP crosses nodes, all-to-all dominates. Look at expert-sharing tricks (DeepSeek-V3 shared expert) or alternative routing.
- 256+ experts: routing diversity is your bigger problem than systems; you usually need expert-parallel + topology-aware routing.

Scale **both** context and experts: each pushes the other into a tighter regime. A realistic frontier MoE long-context model balances them — `~64–128 experts` with `~32–128K` context, not `1024 experts + 1M context`.

</details>

### 重叠是杠杆

上面的通信成本数字都是上界——它们假设该集合通信位于关键路径上。在实践中：

- **TP all-gather** 与下一个矩阵乘重叠（Megatron 通过 `--tp-comm-overlap` 自动做到这一点）。
- **CP ring** 与 attention 计算重叠（ring 步骤被下一个 chunk 的 FlashAttention 隐藏）。
- **EP all-to-all** 可以与 router 计算部分重叠（DeepSpeed 的 `MoE_dispatch_async`）。
- **DP 全规约** 与下一 layer 的反向重叠（标准 ZeRO + 梯度分桶）。

当重叠健康时，**可见**的通信成本远小于原始字节数。当重叠被破坏时（graph-capture 问题、NCCL 流同步），它会表现为 TFLOPs 突然下降。

### 组合并行下的容错

切分的维度越多，出错的方式也越多。一条实用底线：

- 每 30–60 分钟保存一次检查点（模块 09）。
- 在启动长运行之前，用一个小 step 验证 mesh。
- 将每个集合通信的时间作为一等指标进行监控——大多数故障表现为单个集合通信挂起或变慢。

---

## 构建

### 1. Mesh 布局决策练习

针对目标模型 + 序列 + 集群，填写：

```
Cluster: 8 nodes × 8 H200 (64 GPUs total), NVSwitch intra-node, IB inter-node
Model: 64-expert (top-2) 7B base, hidden=4096, layers=32
Sequence: 32K

Choices:
- TP =      (must be ≤ GPUs/node)
- SP =      (always on if TP > 1)
- EP =      (must divide num_experts; usually ≤ GPUs/node)
- CP =      (must divide GPUs/node, leave room for TP and EP)
- PP =      (usually = num_nodes if model fits per stage)
- DP =      (= total_GPUs / (TP × EP × CP × PP))

Constraints:
- TP × EP × CP ≤ GPUs per node = 8
- Total = TP × EP × CP × PP × DP = 64
- Per-rank HBM usage ≤ 141 GB (with recomputation/offload headroom)
```

迭代找到一个满足约束并最小化 EP 跨节点跳数的布局。

### 2. 通信成本计算器

```python
# comm_cost.py
def per_layer_comm_bytes(T, H, H_kv, D, k, TP, CP, EP):
    tp_ag = T * H * (TP - 1) // TP * 4         # 4 matmuls per layer
    cp_ring = (T // CP) * 2 * H_kv * D * (CP - 1) if CP > 1 else 0
    ep_a2a = T * k * H * 2 * 2 if EP > 1 else 0  # dispatch + combine
    return dict(tp=tp_ag * 2, cp=cp_ring * 2, ep=ep_a2a)  # bytes (bf16)

def bw_for_collective(coll, intra_node_bw_GBs, inter_node_bw_GBs, crosses_node):
    return inter_node_bw_GBs if crosses_node else intra_node_bw_GBs

# Plug in your layout choices and see expected per-layer comm time
```

打印两三个候选布局的分解；选择每 layer 的 **最大** 集合通信时间最小的那个。

### 3. 健全性运行

如果拥有集群，用每个候选布局跑一次 20 步的训练迭代。对比：

- 每次迭代的墙钟时间。
- 每个集合通信的时间（Megatron-LM `--log-throughput --log-communication-volume`）。
- 每 GPU 达到的 TFLOPs。

预测通信量最小的布局应当与实测最快的布局一致。如果不一致，说明你的重叠假设在某处有误——在扩大规模之前先修正它。

---

## 在真实技术栈中使用

用于固定 mesh 的 Megatron-LM 标志：

```
--tensor-model-parallel-size <TP>
--pipeline-model-parallel-size <PP>
--context-parallel-size <CP>
--expert-model-parallel-size <EP>
--sequence-parallel
--tp-comm-overlap
--num-experts <E>
--moe-token-dispatcher-type alltoall
```

NeMo Megatron Bridge 将这些封装到更高层级的配置中；对于长上下文 MoE（混合专家模型），"MoE Long-Context Training" 技能页按模型规模 + 序列 + 集群给出参考配置。

DeepSpeed 的 API 不同，但维度相同；对于以 EP 为中心的设置，DeepSpeed-MoE 有时更简洁。

---

## 测量它

对每个候选布局：

- **预测与实测的每个集合通信时间**。
- **每 GPU 达到的 TFLOPs** 占理论峰值的比例。
- **按阶段划分的 step 时间分解**（前向计算、反向计算、tp 通信、ep 通信、cp 通信、dp 通信）。
- **每 rank 峰值 HBM**——必须留有余量地装下。

在 64×H200 上，健康的组合并行运行会将每 GPU TFLOPs 维持在峰值的约 45–55%。如果处在 30%，说明在集合通信上花了太多时间；如果达到 65%+，则可能没有用上所有可用的并行度。

---

## 交付它

放入 `lcm-course/`：

1. `mesh_decision_table.md` — 候选布局、预测通信成本、所选布局及理由。
2. `comm_cost.py` 及其 CSV 输出。
3. （如果有集群访问权限）`mesh_sanity_run.log`，包含每个布局的测量结果，以及一段识别主导集合通信的结论。


<details>
<summary>English original</summary>

**Overlap is the lever**

The communication-cost numbers above are upper bounds — they assume the collective is on the critical path. In practice:

- **TP all-gather** overlaps with the next matmul (Megatron does this automatically with `--tp-comm-overlap`).
- **CP ring** overlaps with attention compute (the ring step is hidden behind the next chunk's FlashAttention).
- **EP all-to-all** can partially overlap with router compute (DeepSpeed's `MoE_dispatch_async`).
- **DP all-reduce** overlaps with backward of next layer (standard ZeRO + gradient bucketing).

When overlap is healthy, the **visible** communication cost is far less than the raw bytes. When overlap breaks (graph-capture issue, NCCL stream sync), it shows up as a sudden TFLOPs drop.

**Fault tolerance under combined parallelism**

The more dimensions you slice, the more ways things can break. A practical floor:

- Checkpoint every 30–60 minutes (Module 09).
- Validate the mesh on a small step before launching a long run.
- Monitor per-collective time as a first-class metric — most failures appear as a single collective hanging or slowing.

---

**Build it**

**1. Mesh-layout decision exercise**

For a target model + sequence + cluster, fill out:

```
Cluster: 8 nodes × 8 H200 (64 GPUs total), NVSwitch intra-node, IB inter-node
Model: 64-expert (top-2) 7B base, hidden=4096, layers=32
Sequence: 32K

Choices:
- TP =      (must be ≤ GPUs/node)
- SP =      (always on if TP > 1)
- EP =      (must divide num_experts; usually ≤ GPUs/node)
- CP =      (must divide GPUs/node, leave room for TP and EP)
- PP =      (usually = num_nodes if model fits per stage)
- DP =      (= total_GPUs / (TP × EP × CP × PP))

Constraints:
- TP × EP × CP ≤ GPUs per node = 8
- Total = TP × EP × CP × PP × DP = 64
- Per-rank HBM usage ≤ 141 GB (with recomputation/offload headroom)
```

Iterate to find a layout that satisfies the constraints and minimizes EP cross-node hops.

**2. Communication-cost calculator**

```python
# comm_cost.py
def per_layer_comm_bytes(T, H, H_kv, D, k, TP, CP, EP):
    tp_ag = T * H * (TP - 1) // TP * 4         # 4 matmuls per layer
    cp_ring = (T // CP) * 2 * H_kv * D * (CP - 1) if CP > 1 else 0
    ep_a2a = T * k * H * 2 * 2 if EP > 1 else 0  # dispatch + combine
    return dict(tp=tp_ag * 2, cp=cp_ring * 2, ep=ep_a2a)  # bytes (bf16)

def bw_for_collective(coll, intra_node_bw_GBs, inter_node_bw_GBs, crosses_node):
    return inter_node_bw_GBs if crosses_node else intra_node_bw_GBs

# Plug in your layout choices and see expected per-layer comm time
```

Print the breakdown for two or three candidate layouts; pick the one with the smallest **maximum** collective time per layer.

**3. Sanity-run**

If you have the cluster, run a 20-step training iteration with each candidate layout. Compare:

- Per-iter wall-clock.
- Per-collective time (Megatron-LM `--log-throughput --log-communication-volume`).
- Per-GPU TFLOPs achieved.

The layout with the smallest predicted comm should match the measured fastest. If it does not, your overlap assumptions are wrong somewhere — fix that before scaling up.

---

**Use it in the real stack**

The Megatron-LM flags that pin a mesh:

```
--tensor-model-parallel-size <TP>
--pipeline-model-parallel-size <PP>
--context-parallel-size <CP>
--expert-model-parallel-size <EP>
--sequence-parallel
--tp-comm-overlap
--num-experts <E>
--moe-token-dispatcher-type alltoall
```

NeMo Megatron Bridge wraps these in a higher-level config; for long-context MoE the "MoE Long-Context Training" skill page gives reference configs by model size + sequence + cluster.

DeepSpeed has a different API but the same dimensions; for an EP-centric setup, DeepSpeed-MoE is sometimes cleaner.

---

**Measure it**

For each candidate layout:

- **Predicted vs measured per-collective time**.
- **Achieved TFLOPs per GPU** as fraction of theoretical peak.
- **Step time breakdown** by phase (fwd compute, bwd compute, tp comm, ep comm, cp comm, dp comm).
- **Per-rank peak HBM** — must fit with headroom.

A healthy combined-parallelism run on 64×H200 holds per-GPU TFLOPs at ~45–55% of peak. If you are at 30%, you are spending too much time in collectives; if at 65%+, you are probably not using all available parallelism.

---

**Ship it**

Drop into `lcm-course/`:

1. `mesh_decision_table.md` — candidate layouts, predicted comm costs, chosen layout with rationale.
2. `comm_cost.py` and its CSV output.
3. (If cluster access) `mesh_sanity_run.log` with per-layout measurements and a one-paragraph conclusion identifying the dominant collective.

---

</details>

## 相关页面

- [Module 02 — 长上下文 attention 机制](02-Long-Context-Attention.md)
- [Module 05 — MoE（混合专家模型）系统与基础设施](05-MoE-Systems-Infrastructure.md)
- [Module 09 — 分布式训练基础设施](09-Distributed-Training-Infrastructure.md)
- NeMo Megatron Bridge MoE 长上下文技能：<https://docs.nvidia.com/nemo/megatron-bridge/nightly/skills/perf-techniques/moe-long-context/SKILL.html>
- Megatron-LM mesh README：<https://github.com/NVIDIA/Megatron-LM/blob/main/megatron/core/transformer/README.md>


<details>
<summary>English original</summary>

**Related pages**

- [Module 02 — Long-context attention mechanics](02-Long-Context-Attention.md)
- [Module 05 — MoE systems and infrastructure](05-MoE-Systems-Infrastructure.md)
- [Module 09 — Distributed training infrastructure](09-Distributed-Training-Infrastructure.md)
- NeMo Megatron Bridge MoE long-context skill: <https://docs.nvidia.com/nemo/megatron-bridge/nightly/skills/perf-techniques/moe-long-context/SKILL.html>
- Megatron-LM mesh README: <https://github.com/NVIDIA/Megatron-LM/blob/main/megatron/core/transformer/README.md>

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Long-Context-MoE-Foundation-Training/08-Combining-LongContext-and-MoE.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Long-Context-MoE-Foundation-Training/08-Combining-LongContext-and-MoE.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
