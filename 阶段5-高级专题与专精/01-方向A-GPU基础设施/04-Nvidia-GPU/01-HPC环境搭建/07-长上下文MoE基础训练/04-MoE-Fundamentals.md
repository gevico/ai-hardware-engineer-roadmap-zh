---
title: Module 04 — MoE（混合专家模型）基础
description: Module 04 — MoE（混合专家模型）基础
---

# Module 04 — MoE（混合专家模型）基础

**父模块：** [长上下文 MoE 基础训练](README.md)

**一句话目的：** 从数学、训练稳定性以及防止 router 坍缩的辅助损失层面理解混合专家模型 FFN 层。

**前置要求：** 熟悉稠密 Transformer FFN 块。熟悉 softmax + 交叉熵。

**产物：** 一个在 PyTorch 中可工作的 top-k MoE FFN 层（单 GPU 即可），并带有一个消融，展示辅助损失权重如何改变每专家利用率。

---

## 为什么重要

MoE 是在不为每个 token 支付完整稠密计算的情况下扩展模型容量的主流方式。现代前沿级模型（DeepSeek-V2/V3、Mixtral、Qwen-MoE、GPT-OSS 变体）都是 MoE。如果不理解 router 数学和负载均衡失效模式，就会遇到训练不稳定，看起来像 kernel 坏了，但实际上是布线病态。

MoE 的系统侧（专家并行、all-to-all、规模化下的容量因子）是 Module 05。本模块是系统工作所假设的**算法**基础。

---

## 心智模型

### 稠密 FFN 块

```
x ∈ ℝ^H
h = gelu(x · W_up)        # W_up ∈ ℝ^{H × 4H}
y = h · W_down           # W_down ∈ ℝ^{4H × H}
```

每个 token 都会触及每个参数。每个 FFN 块的参数：`8 H²`。每个 token 的 FLOPs：`16 H²`。

### 一个 top-k MoE FFN 块

将单个 FFN 替换为 `E` 个独立的专家 FFN，以及一个 router，其每个 token 选择其中 `k` 个。

```
x ∈ ℝ^H
logits = x · W_r          # W_r ∈ ℝ^{H × E}, router weights
g = softmax(logits)       # routing distribution
topk_indices = argtopk(g, k)
topk_weights = g[topk_indices]                  # gate values for chosen experts
topk_weights = topk_weights / topk_weights.sum() # renormalize over selected
y = Σ_{i in topk_indices} topk_weights[i] · Expert_i(x)
```

每个 FFN 块的参数：`E · 8H² + H · E ≈ E · 8H²`。每个 token 的 FLOPs（计算侧）：`k · 16 H²`。**参数数量是稠密块的 `E`×；计算仅为稠密块的 `k`×。**

示例：Mixtral 8×7B 有 `E = 8, k = 2`。每个 FFN 的总参数 ≈ 8× 稠密；每个 token 的激活参数 ≈ 2× 稠密。

### 为什么 MoE 不稳定

router 只是对专家索引的 softmax。如果它早期学到“专家 3 很好”，就会把所有东西都送给专家 3，专家 3 得到训练，其他专家则挨饿。若不干预，这会坍缩为一个在一个专家上稠密、并带有多余死权重的模型。

修复办法是**辅助损失**，推动布线分布趋向均匀。

### 负载均衡辅助损失

定义：

- `f_i = fraction of tokens routed to expert i`（每个微批）
- `P_i = mean routing probability for expert i = mean over tokens of g[i]`

损失项：`L_aux = α · E · Σ_i f_i · P_i`。在均匀布线下，`f_i = 1/E` 和 `P_i = 1/E`，因此 `L_aux = α`。在坍缩布线下，`f_3 = 1` 和 `P_3 ≈ 1`，因此 `L_aux = α · E`——大得多。梯度将 router 推向均匀使用。

典型的 `α ∈ [0.001, 0.01]`。太低：坍缩。太高：布线变得随机，模型失去专业化带来的收益。

### Router z-loss

来自 Switch Transformer 论文的第二个稳定器。router 的 logits 可能漂移到很大的幅度，从而破坏 softmax 的稳定性。添加：

```
L_z = β · mean_over_tokens( log(Σ_i exp(logits_i))² )
```

使用小的 `β`（例如 `1e-3`）。保持 router 的 logsumexp 有界。

### 容量因子与丢弃的 token

当批处理许多 token 时，专家 `i` 会接收一定数量的 token。为实现高效的批处理专家执行，将每专家容量上限设为：

```
capacity = ceil(capacity_factor · tokens_per_batch · k / E)
```

`capacity_factor` 通常为 `1.0` 到 `1.5`。超出其所选专家容量的 token 会被**丢弃**——它们跳过 FFN，仅带着残差通过（在某些实现中）或带着全零通过（在另一些实现中）。丢弃 token 率是关键诊断指标：应为 0–5%。如果更高，则负载均衡器没有尽到职责，或者容量太紧。

### 两种布线风格

- **Token-choice top-k**（本模块默认）：每个 token 选择其 top `k` 个专家。
- **Expert-choice**：每个专家选择其 top `M` 个 token。反转了瓶颈；token 可能去往零个或多个专家。被一些近期 MoE 变体使用。权衡：没有丢弃的 token，但不保证每个 token 的 FFN 覆盖。

Token-choice 更常见，并且是大多数生产 MoE 实现开箱即用所支持的唯一风格。在笔记中提及 expert-choice，但在本课程的其他所有内容中使用 token-choice。


<details>
<summary>English original</summary>

**Module 04 — MoE Fundamentals**

**Parent:** [Long-Context MoE Foundation Training](README.md)

**One-line purpose:** Understand the Mixture-of-Experts FFN layer at the level of math, training stability, and the auxiliary losses that keep the router from collapsing.

**Prerequisites:** Comfortable with the dense transformer FFN block. Familiarity with softmax + cross-entropy.

**Artifact:** A working top-k MoE FFN layer in PyTorch (single-GPU is fine), with an ablation showing how the auxiliary-loss weight changes per-expert utilization.

---

**Why it matters**

MoE is the dominant way to scale model capacity without paying full dense compute per token. Modern frontier-class models (DeepSeek-V2/V3, Mixtral, Qwen-MoE, GPT-OSS variants) are MoE. If you do not understand the router math and the load-balancing failure modes, you will hit training instability that looks like the kernel is broken but is actually a routing pathology.

The systems-side of MoE (expert parallelism, all-to-all, capacity factor at scale) is Module 05. This module is the **algorithmic** foundation that the systems work assumes.

---

**Mental model**

**A dense FFN block**

```
x ∈ ℝ^H
h = gelu(x · W_up)        # W_up ∈ ℝ^{H × 4H}
y = h · W_down           # W_down ∈ ℝ^{4H × H}
```

Every token touches every parameter. Parameters per FFN block: `8 H²`. FLOPs per token: `16 H²`.

**A top-k MoE FFN block**

Replace the single FFN with `E` independent expert FFNs and a router that picks `k` of them per token.

```
x ∈ ℝ^H
logits = x · W_r          # W_r ∈ ℝ^{H × E}, router weights
g = softmax(logits)       # routing distribution
topk_indices = argtopk(g, k)
topk_weights = g[topk_indices]                  # gate values for chosen experts
topk_weights = topk_weights / topk_weights.sum() # renormalize over selected
y = Σ_{i in topk_indices} topk_weights[i] · Expert_i(x)
```

Parameters per FFN block: `E · 8H² + H · E ≈ E · 8H²`. FLOPs per token (compute side): `k · 16 H²`. **Parameter count is `E`× the dense block; compute is only `k`× the dense block.**

Example: Mixtral 8×7B has `E = 8, k = 2`. Total parameters per FFN ≈ 8× dense; activated parameters per token ≈ 2× dense.

**Why MoE is unstable**

The router is just a softmax over expert indices. If it learns early that "expert 3 is good", it sends everything to expert 3, expert 3 gets trained, the others starve. Without intervention this collapses to a dense-on-one-expert model with extra dead weights.

The fix is **auxiliary losses** that push the routing distribution toward uniformity.

**Load-balancing auxiliary loss**

Define:

- `f_i = fraction of tokens routed to expert i` (per micro-batch)
- `P_i = mean routing probability for expert i = mean over tokens of g[i]`

Loss term: `L_aux = α · E · Σ_i f_i · P_i`. With uniform routing, `f_i = 1/E` and `P_i = 1/E`, so `L_aux = α`. With collapsed routing, `f_3 = 1` and `P_3 ≈ 1`, so `L_aux = α · E` — much larger. The gradient pushes the router toward uniform usage.

Typical `α ∈ [0.001, 0.01]`. Too low: collapse. Too high: routing becomes random, model loses the benefit of specialization.

**Router z-loss**

A second stabilizer from the Switch Transformer paper. The router's logits can drift to large magnitudes that destabilize the softmax. Add:

```
L_z = β · mean_over_tokens( log(Σ_i exp(logits_i))² )
```

With small `β` (e.g. `1e-3`). Keeps the router's logsumexp bounded.

**Capacity factor and dropped tokens**

When you batch many tokens, expert `i` receives some number of tokens. To enable efficient batched expert execution, you cap the per-expert capacity at:

```
capacity = ceil(capacity_factor · tokens_per_batch · k / E)
```

`capacity_factor` is typically `1.0` to `1.5`. Tokens beyond capacity for their chosen expert are **dropped** — they skip the FFN and go through with just the residual (in some implementations) or with all-zeros (in others). Dropped-token rate is a key diagnostic: should be 0–5%. If higher, the load balancer isn't doing its job or capacity is too tight.

**Two routing styles**

- **Token-choice top-k** (this module's default): each token picks its top `k` experts.
- **Expert-choice**: each expert picks its top `M` tokens. Inverts the bottleneck; tokens may go to zero or many experts. Used by some recent MoE variants. Trade-offs: no dropped tokens, but no per-token guarantee of FFN coverage.

Token-choice is more common and the only style most production MoE implementations support out of the box. Mention expert-choice in your notes but use token-choice for everything else in this course.

</details>

### 为什么 MoE 契合长上下文训练

- 长上下文本身就已偏重计算；你负担不起 `2×` 的 dense 扩容。
- MoE 只用 `2×` 的计算量，就给你 `8×` 的容量。
- 专家可以在长文本模式（代码、散文、数学、对话）上特化——这很有用，因为长上下文数据按定义就是异构的。
- 系统层面的取舍是通信：all-to-all dispatch 开销随 token 数增长，而长上下文产生的 token 恰恰很多。这就是 Module 05。

---

## 动手实现

一个自包含的 PyTorch 实现，小到能通读，又大到足以暴露 routing 的病态行为。

```python
# minimal_moe.py
import torch, torch.nn as nn, torch.nn.functional as F

class TopKMoE(nn.Module):
    def __init__(self, H, expert_dim, num_experts=8, top_k=2,
                 aux_weight=0.01, z_weight=1e-3, capacity_factor=1.25):
        super().__init__()
        self.E, self.k = num_experts, top_k
        self.router = nn.Linear(H, num_experts, bias=False)
        self.experts = nn.ModuleList(
            nn.Sequential(nn.Linear(H, expert_dim), nn.GELU(), nn.Linear(expert_dim, H))
            for _ in range(num_experts)
        )
        self.aux_weight, self.z_weight = aux_weight, z_weight
        self.capacity_factor = capacity_factor

    def forward(self, x):
        # x: [B, S, H]
        B, S, H = x.shape
        flat = x.reshape(-1, H)             # [T, H], T = B*S
        T = flat.size(0)

        logits = self.router(flat)          # [T, E]
        g = F.softmax(logits, dim=-1)
        topk_w, topk_idx = g.topk(self.k, dim=-1)    # [T, k]
        topk_w = topk_w / topk_w.sum(dim=-1, keepdim=True)

        # Capacity per expert
        cap = int(self.capacity_factor * T * self.k / self.E + 1)
        out = torch.zeros_like(flat)
        dropped = 0
        for e in range(self.E):
            # tokens that picked expert e in any slot, ordered by gate value
            mask = (topk_idx == e)                    # [T, k] bool
            tok_pos = mask.any(-1).nonzero(as_tuple=True)[0]
            # priority by max gate weight among the k slots for that token
            scores = (topk_w * mask).sum(-1)[tok_pos]
            order = scores.argsort(descending=True)
            keep = tok_pos[order][:cap]
            dropped += max(0, tok_pos.size(0) - cap)
            if keep.numel() == 0:
                continue
            xe = flat[keep]
            ye = self.experts[e](xe)
            # weight: pick the matching gate value from topk_w
            w_e = (topk_w * mask).sum(-1)[keep].unsqueeze(-1)
            out.index_add_(0, keep, w_e * ye)

        # Aux loss (load balancing)
        f = torch.zeros(self.E, device=x.device)
        for e in range(self.E):
            f[e] = (topk_idx == e).any(-1).float().mean()
        P = g.mean(0)                                  # [E]
        aux_loss = self.E * (f * P).sum() * self.aux_weight

        # Router z-loss
        z = torch.logsumexp(logits, dim=-1)
        z_loss = (z ** 2).mean() * self.z_weight

        return out.reshape(B, S, H), aux_loss + z_loss, dropped / max(T, 1)

if __name__ == "__main__":
    torch.manual_seed(0)
    moe = TopKMoE(H=256, expert_dim=1024).cuda()
    x = torch.randn(4, 128, 256, device="cuda")
    y, aux, drop_rate = moe(x)
    print("out", y.shape, "aux_loss", aux.item(), "dropped%", drop_rate * 100)
```

接着构建训练循环消融实验：

```python
# moe_ablation.py
# Train this MoE on a toy task (e.g. learn to reconstruct shuffled MNIST flattened patches)
# Sweep aux_weight in {0, 1e-4, 1e-3, 1e-2, 1e-1}.
# For each, log per-expert token-share over training.
# Plot per-expert usage curves.
```

预期结果：

- `aux_weight = 0`：几百步后一两个专家占据主导；使用率曲线塌缩。
- `aux_weight = 1e-3`：使用率大致保持均匀，只有小幅漂移。
- `aux_weight = 1e-1`：使用率被人为压平，但模型 loss 停滞，因为 routing 实质上是随机的。

把图保存下来。当 MoE 训练出问题时，这是你手里最有用的诊断工具。

---

## 在真实技术栈中使用

- **Megatron-LM MoE**：`--num-experts`、`--moe-router-topk`、`--moe-aux-loss-coeff`、`--moe-z-loss-coeff`、`--moe-expert-capacity-factor`。文档见 <https://github.com/NVIDIA/Megatron-LM/blob/main/megatron/core/transformer/moe/README.md>。
- **DeepSpeed-MoE**：<https://www.deepspeed.ai/tutorials/mixture-of-experts/>。概念类似，命名不同。
- **`transformers` MoE 类**：`MixtralSparseMoeBlock`、`Qwen2MoeSparseMoeBlock`、`DeepseekV3MoE`。读 forward 方法——它们实现的正是你写的内容，另加专家并行所需的系统侧 gather/scatter。

至少完整读一遍其中一个 MoE forward。把每个变量对应到上面的数学。如果某个变量的用途不清楚，那就是你需要补齐的缺口。

---


<details>
<summary>English original</summary>

**Why MoE matches long-context training**

- Long context is already compute-heavy; you cannot afford a `2×` dense scale-up.
- MoE gives you `8×` capacity with only `2×` compute.
- Experts can specialize on long-form patterns (code, prose, math, dialog) — useful because long-context data is heterogeneous by definition.
- The systems trade-off is communication: all-to-all dispatch costs grow with token count, which is exactly what long context produces a lot of. That's Module 05.

---

**Build it**

A self-contained PyTorch implementation that's small enough to read and large enough to show the routing pathologies.

```python
# minimal_moe.py
import torch, torch.nn as nn, torch.nn.functional as F

class TopKMoE(nn.Module):
    def __init__(self, H, expert_dim, num_experts=8, top_k=2,
                 aux_weight=0.01, z_weight=1e-3, capacity_factor=1.25):
        super().__init__()
        self.E, self.k = num_experts, top_k
        self.router = nn.Linear(H, num_experts, bias=False)
        self.experts = nn.ModuleList(
            nn.Sequential(nn.Linear(H, expert_dim), nn.GELU(), nn.Linear(expert_dim, H))
            for _ in range(num_experts)
        )
        self.aux_weight, self.z_weight = aux_weight, z_weight
        self.capacity_factor = capacity_factor

    def forward(self, x):
        # x: [B, S, H]
        B, S, H = x.shape
        flat = x.reshape(-1, H)             # [T, H], T = B*S
        T = flat.size(0)

        logits = self.router(flat)          # [T, E]
        g = F.softmax(logits, dim=-1)
        topk_w, topk_idx = g.topk(self.k, dim=-1)    # [T, k]
        topk_w = topk_w / topk_w.sum(dim=-1, keepdim=True)

        # Capacity per expert
        cap = int(self.capacity_factor * T * self.k / self.E + 1)
        out = torch.zeros_like(flat)
        dropped = 0
        for e in range(self.E):
            # tokens that picked expert e in any slot, ordered by gate value
            mask = (topk_idx == e)                    # [T, k] bool
            tok_pos = mask.any(-1).nonzero(as_tuple=True)[0]
            # priority by max gate weight among the k slots for that token
            scores = (topk_w * mask).sum(-1)[tok_pos]
            order = scores.argsort(descending=True)
            keep = tok_pos[order][:cap]
            dropped += max(0, tok_pos.size(0) - cap)
            if keep.numel() == 0:
                continue
            xe = flat[keep]
            ye = self.experts[e](xe)
            # weight: pick the matching gate value from topk_w
            w_e = (topk_w * mask).sum(-1)[keep].unsqueeze(-1)
            out.index_add_(0, keep, w_e * ye)

        # Aux loss (load balancing)
        f = torch.zeros(self.E, device=x.device)
        for e in range(self.E):
            f[e] = (topk_idx == e).any(-1).float().mean()
        P = g.mean(0)                                  # [E]
        aux_loss = self.E * (f * P).sum() * self.aux_weight

        # Router z-loss
        z = torch.logsumexp(logits, dim=-1)
        z_loss = (z ** 2).mean() * self.z_weight

        return out.reshape(B, S, H), aux_loss + z_loss, dropped / max(T, 1)

if __name__ == "__main__":
    torch.manual_seed(0)
    moe = TopKMoE(H=256, expert_dim=1024).cuda()
    x = torch.randn(4, 128, 256, device="cuda")
    y, aux, drop_rate = moe(x)
    print("out", y.shape, "aux_loss", aux.item(), "dropped%", drop_rate * 100)
```

Now build the training-loop ablation:

```python
# moe_ablation.py
# Train this MoE on a toy task (e.g. learn to reconstruct shuffled MNIST flattened patches)
# Sweep aux_weight in {0, 1e-4, 1e-3, 1e-2, 1e-1}.
# For each, log per-expert token-share over training.
# Plot per-expert usage curves.
```

Expected outcomes:

- `aux_weight = 0`: one or two experts dominate after a few hundred steps; usage curve collapses.
- `aux_weight = 1e-3`: usage stays approximately uniform with small drift.
- `aux_weight = 1e-1`: usage is artificially flat, but model loss stalls because routing is essentially random.

Save the plot. It is the most useful diagnostic you will own when MoE training goes wrong.

---

**Use it in the real stack**

- **Megatron-LM MoE**: `--num-experts`, `--moe-router-topk`, `--moe-aux-loss-coeff`, `--moe-z-loss-coeff`, `--moe-expert-capacity-factor`. Documented at <https://github.com/NVIDIA/Megatron-LM/blob/main/megatron/core/transformer/moe/README.md>.
- **DeepSpeed-MoE**: <https://www.deepspeed.ai/tutorials/mixture-of-experts/>. Similar concepts, different naming.
- **`transformers` MoE classes**: `MixtralSparseMoeBlock`, `Qwen2MoeSparseMoeBlock`, `DeepseekV3MoE`. Read the forward methods — they implement exactly what you wrote, with the systems-side gather/scatter for expert parallelism.

Read at least one of these MoE forwards end to end. Map each variable to the math above. If a variable's purpose is unclear, you have a gap to close.

---

</details>

## 度量

在你的消融过程中：

- 训练过程中的 **每专家 token 占比**（折线图，每个专家一条线）。
- 每步的 **丢弃 token 率**（在 `capacity_factor = 1.25` 下应 < 5%）。
- **辅助损失值** —— 若布线均衡，应接近 `α`；若发生坍缩，则大得多。
- **任务损失** —— 可确认过度正则化的 router（`α` 过大）会损害学习。

健康的 MoE 具有大致均匀的每专家使用率、接近 `α` 的辅助损失、较低的丢弃 token 率，以及一条与相同激活参数量的 dense 基线持平或更优的任务损失曲线。

---

## 交付

写入 `lcm-course/`：

1. `minimal_moe.py` 与 `moe_ablation.py`，并附日志。
2. `moe_expert_usage.png` —— aux 权重扫描中的每专家使用率曲线。
3. `moe_notes.md` —— 就 top-k 布线数学、aux loss、z-loss、容量因子各写一段，并至少写出一个你在消融中实际触发的具名失效模式（例如“aux_weight=0 时，到第 500 步 expert 2 收到全部 token 的 87%”）。

---

## 相关页面

- [模块 05 —— MoE 系统与基础设施](05-MoE-Systems-Infrastructure.md)
- [模块 08 —— 长上下文与 MoE 的结合](08-Combining-LongContext-and-MoE.md)
- Switch Transformer 论文（router z-loss + 负载均衡）：<https://arxiv.org/abs/2101.03961>
- Mixtral of Experts 论文：<https://arxiv.org/abs/2401.04088>
- DeepSeek-V2 MoE 设计：<https://arxiv.org/abs/2405.04434>


<details>
<summary>English original</summary>

**Measure it**

During your ablation:

- **Per-expert token share** over training (line plot, one line per expert).
- **Dropped-token rate** per step (should be < 5% with `capacity_factor = 1.25`).
- **Aux loss value** — should be near `α` if routing is balanced; much larger if collapsing.
- **Task loss** — confirms that an over-regularized router (huge `α`) hurts learning.

A healthy MoE has roughly uniform per-expert usage, near-`α` aux loss, low dropped-token rate, and a task loss curve that matches or beats a dense baseline with the same activated parameter count.

---

**Ship it**

Drop into `lcm-course/`:

1. `minimal_moe.py` and `moe_ablation.py` with logs.
2. `moe_expert_usage.png` — the per-expert usage curves across the aux-weight sweep.
3. `moe_notes.md` — one paragraph each on top-k routing math, aux loss, z-loss, capacity factor, and at least one named failure mode you actually triggered in your ablation (e.g. "with aux_weight=0, expert 2 received 87% of all tokens by step 500").

---

**Related pages**

- [Module 05 — MoE systems and infrastructure](05-MoE-Systems-Infrastructure.md)
- [Module 08 — Combining long-context and MoE](08-Combining-LongContext-and-MoE.md)
- Switch Transformer paper (router z-loss + load balancing): <https://arxiv.org/abs/2101.03961>
- Mixtral of Experts paper: <https://arxiv.org/abs/2401.04088>
- DeepSeek-V2 MoE design: <https://arxiv.org/abs/2405.04434>

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Long-Context-MoE-Foundation-Training/04-MoE-Fundamentals.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Long-Context-MoE-Foundation-Training/04-MoE-Fundamentals.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
