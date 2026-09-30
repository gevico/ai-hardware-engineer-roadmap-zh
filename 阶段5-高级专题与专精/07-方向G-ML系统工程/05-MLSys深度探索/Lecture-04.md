---
title: 第 04 讲 - 超越稠密 Transformer：Mamba、SSM 与混合浪潮
description: 第 04 讲 - 超越稠密 Transformer：Mamba、SSM 与混合浪潮
---

# 第 04 讲 - 超越稠密 Transformer：Mamba、SSM 与混合浪潮

**合集：** [MLSys Deep Dives](README.md) | **上一讲：** [← 第 03 讲](Lecture-03.md) | **下一讲：** [第 05 讲](Lecture-05.md)

---

到目前为止，*kernel* 和 *编译器* 已经提速，而模型本身未动。本讲从另一侧攻击成本：**重新设计架构本身，使每个 token 的工作量更少。** 这是模型–系统协同设计，并且在 2024–2026 年间产生了自 Transformer 以来最大的结构性转变——转向 **状态空间模型、线性 attention 与混合架构**。

故事的反派是一种数据结构：**KV cache**。理解它为何增长以及什么能消灭它，你就能理解为什么 Mamba、Jamba、Nemotron-H、Falcon-H1 和 MiniMax 存在——以及为什么 2026 年的长上下文模型内部看起来与 2022 年的完全不同。

---

## 学习目标

在本讲结束时，你应当能够：

1. 解释 **KV cache 问题**：为什么稠密 attention 的计算量为 O(L²)，内存以 O(L) 增长，以及为什么这会限制长上下文吞吐。
2. 描述 **SSM**（Mamba）：恒定大小的循环状态，每个 token 的 O(1) 内存/计算，以及 **选择** 机制。
3. 解释 **Mamba-2 / SSD** ——状态空间 ↔ attention 对偶性——以及为什么它使 SSM 对 tensor core 友好，从而成为混合构建块。
4. 将 **线性 attention** 家族（RWKV、RetNet、GLA）置于相同的“恒定内存循环”理念上。
5. 解释 **混合浪潮**（约 1 层 attention : 约 7–10 层 SSM 的模式），并将 Jamba、Nemotron-H、Falcon-H1、MiniMax-01 视为实例。
6. 将每种架构与一个 **KV/内存数值** 联系起来，并因此与批深度、tokens/s 和 `$/Mtok` 联系起来。

---

## 1. 反派：KV cache

稠密自注意力有一个对质量极好、对系统极具破坏性的特性：生成 token `t` 时，它要对 **所有先前 token** 进行 attention。因此每个 token 的 key 和 value 都必须保留——即 **KV cache**——并且它随序列增长。

```python
# Dense attention: you must keep ALL past keys/values around
K_cache, V_cache = [], []
for x_t in sequence:
    q_t, k_t, v_t = project(x_t)
    K_cache.append(k_t); V_cache.append(v_t)        # the cache GROWS every single token
    y_t = softmax(q_t @ stack(K_cache).T) @ stack(V_cache)
# memory is O(sequence_length) per layer, per request — this is what eats your VRAM
```

具体而言，KV cache 大小为：

```text
   KV bytes = 2 (K,V) × layers × kv_heads × head_dim × seq_len × dtype_bytes × batch
                                                        ▲                       ▲
                                              grows with context        grows with batch
```

对于 128K 上下文的 70B 级模型，这达到 **几十 GB**——通常比激活值还大，并且它同时随 *上下文长度* 和 *批大小* 缩放。这就是那堵墙。这就是为什么长上下文推理服务代价高昂，为什么批深度（以及由此决定的 TOK/$，来自 Lecture 1）被限制，以及为什么 decode（逐 token 生成阶段）随着对话增长而变慢。存在两族架构来拆除这堵墙：**用循环替代 attention**（SSM，§2–4），或者 **压缩缓存**（MLA，§6）。

---


<details>
<summary>English original</summary>

**Lecture 04 - Beyond the Dense Transformer: Mamba, SSMs, and the Hybrid Wave**

**Collection:** [MLSys Deep Dives](README.md) | **Previous:** [← Lecture 03](Lecture-03.md) | **Next:** [Lecture 05](Lecture-05.md)

---

So far we have made the *kernels* and *compilers* faster without touching the model. This lecture attacks the cost from the other side: **redesigning the architecture itself so there is less work to do per token.** This is model–systems co-design, and in 2024–2026 it produced the biggest structural shift since the transformer — the move to **state-space models, linear attention, and hybrids**.

The villain of the story is one data structure: the **KV cache**. Understand why it grows and what kills it, and you understand why Mamba, Jamba, Nemotron-H, Falcon-H1, and MiniMax exist — and why a 2026 long-context model looks nothing like a 2022 one on the inside.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Explain the **KV-cache problem**: why dense attention is O(L²) compute and O(L) growing memory, and why that caps long-context throughput.
2. Describe an **SSM** (Mamba): a constant-size recurrent state, O(1) memory/compute per token, and the **selection** mechanism.
3. Explain **Mamba-2 / SSD** — the state-space ↔ attention duality — and why it made SSMs tensor-core-friendly and thus the hybrid building block.
4. Place the **linear-attention** family (RWKV, RetNet, GLA) on the same "constant-memory recurrence" idea.
5. Explain the **hybrid wave** (the ~1-attention : ~7–10-SSM pattern) and read Jamba, Nemotron-H, Falcon-H1, MiniMax-01 as instances.
6. Connect each architecture to a **KV/memory number** and therefore to batch depth, tokens/s, and `$/Mtok`.

---

**1. The villain: the KV cache**

Dense self-attention has a property that is wonderful for quality and ruinous for systems: to generate token `t`, it attends over **all previous tokens**. So every token's key and value must be kept around — the **KV cache** — and it grows with the sequence.

```python
# Dense attention: you must keep ALL past keys/values around
K_cache, V_cache = [], []
for x_t in sequence:
    q_t, k_t, v_t = project(x_t)
    K_cache.append(k_t); V_cache.append(v_t)        # the cache GROWS every single token
    y_t = softmax(q_t @ stack(K_cache).T) @ stack(V_cache)
# memory is O(sequence_length) per layer, per request — this is what eats your VRAM
```

The KV-cache size, concretely:

```text
   KV bytes = 2 (K,V) × layers × kv_heads × head_dim × seq_len × dtype_bytes × batch
                                                        ▲                       ▲
                                              grows with context        grows with batch
```

For a 70B-class model at 128K context this is **tens of gigabytes** — often larger than the activations, and it scales with *both* the context length and the batch size. That is the wall. It is why long-context serving is expensive, why batch depth (and therefore TOK/$, from Lecture 1) is capped, and why decode slows as the conversation grows. Two families of architecture exist to tear this wall down: **replace attention with recurrence** (SSMs, §2–4), or **compress the cache** (MLA, §6).

---

</details>

## 2. 状态空间模型：用缓存换固定状态

**状态空间模型（SSM）** 做的是 RNN 做的事 —— 向前传递固定大小的 **状态** —— 但采用一种能在 GPU 上高效训练的形式。全部历史被压缩成一个常量大小的向量；**没有不断增长的缓存**。

```python
# SSM recurrence (conceptual): the whole past compressed into a FIXED-size state h
h = zeros(d_state)
for x_t in sequence:
    h   = A * h + B * x_t          # update fixed-size state — memory does NOT grow
    y_t = C @ h                    # produce the output for this token
# memory is O(d_state), INDEPENDENT of sequence length
```

经典 SSM 的问题在于 `A, B, C` 是 **固定的**（与输入无关），因此模型无法根据 *内容* 有选择地记住或遗忘 —— 这对语言是致命的。**Mamba**（Albert Gu & Tri Dao，2023 年 12 月）用 **选择机制** 解决了这一点：让 `Δ, B, C` 成为 **输入的函数** `x_t`，于是模型可以逐 token 决定保留什么、丢弃什么。

`Δ`（**步长**）就是让这一切成立的参数，值得花点篇幅解释。SSM 定义在连续时间上；当递推被 **离散化** 时（`Ā = exp(Δ_t·A)`，`B̄ ≈ Δ_t·B`），`Δ_t` 就是这个 token 代表多大的“时间步”。让它依赖输入，就把它变成一个学到的、逐 token 的 **记住还是跳过的门控**：

```text
   Δ_t → 0      Ā → I       state coasts through unchanged  →  token is IGNORED
   Δ_t large    Ā → 0       state resets toward this token   →  token OVERWRITES memory
```

这就是 SSM 对 attention 的“关注还是不关注”的回答 —— 只不过这个决策被压缩进固定大小的状态，而不是让缓存不断增长。仅此一处改动，就让 SSM 在语言任务上可与 Transformer 竞争，同时保住系统层面的优势：

```text
   attention:  O(L²) compute,  O(L) memory (growing KV cache)
   SSM/Mamba:  O(L)  compute,  O(1) memory per token (fixed state)  ← the whole point
```

MLSys 层面的影响是巨大的：**内存与逐 token 计算量对上下文长度而言是平的。** Mamba 模型的 tokens/s 不会随对话增长而下降，它在 1M token 时的内存占用看起来和 1K 时一样。这完全是另一条成本曲线。

---

## 3. Mamba-2 与让混合模型成为可能的对偶性

Mamba-1 有一个系统层面的瑕疵：它的 selective scan 无法干净地映射到 Tensor Core（矩阵乘的硬件机制）上，因而未能充分利用 GPU。**Mamba-2**（Dao & Gu，2024 年 5 月）用一个深刻的结论解决了这个问题 —— **结构化状态空间对偶（SSD）**：

> 一个选择性 SSM 通过结构化（半可分）矩阵，*在数学上等价于* 某种 **masked attention**。因此同一个 layer 既可以按线性递推计算，**也可以** 按一块矩阵乘计算。

正是这个对偶性让 Mamba-2 对本课程至关重要。通过把 SSM 表达为 **对 Tensor Core 友好的矩阵乘**（一种分块分解），Mamba-2 比 Mamba-1 快 **2–8×**，并终于能像 Transformer 那样使用 GPU。它把状态矩阵 `A` 简化为标量乘单位阵，并加入了 **多头** 结构（类似 attention head）。其成果成为下文每个混合模型的 **标准构建块** —— 你既得到常量状态推理的优势，*又* 获得良好的训练期硬件利用率。

```text
   SSD:  one layer, two ways to compute it
        recurrence form  →  O(1) state, great for INFERENCE (decode)
        matmul form      →  tensor-core-friendly, great for TRAINING (parallel)
```

---

## 4. 线性 attention 家族（同样的思路，不同的门控）

Mamba 最出名，但它属于 **亚二次 / 线性 attention** 方法家族，这些方法共享同一个系统卖点 —— **常量内存的递推，而非不断增长的 KV cache** —— 主要区别在于 *如何门控* 状态：

| 方法 | 门控 / 衰减 | 一句话系统要点 |
|---|---|---|
| **RWKV** | 按通道的时间衰减 | 训练时像 Transformer 一样并行，推理时作为 O(1) 的 RNN 运行 |
| **RetNet** | 固定指数衰减（“retention”） | 三种模式：parallel（训练）、recurrent（O(1) 推理）、chunkwise（长序列） |
| **GLA** | **数据相关** 的矩阵门控 | 输入自适应的遗忘 + 硬件高效的 chunkwise kernel |
| **Mamba-2** | **输入相关** 的选择性 Δ | 选择机制 + SSD 对偶（矩阵乘形式） |

要记住的谱系是：**固定衰减**（RWKV、RetNet —— 更简单、更便宜）→ **输入相关的门控**（GLA、Mamba —— 自适应、表达能力更强）。它们都给出同一个核心结论：**随上下文增长，内存与吞吐保持平坦。** 相比 full attention，它们放弃的是一些精确的长程 *召回* —— 精确查找某个更早 token 的能力。而这正是混合模型要补上的缺口。

---


<details>
<summary>English original</summary>

**2. State-space models: trade the cache for a fixed state**

A **state space model (SSM)** does what an RNN does — carry a fixed-size **state** forward — but in a form that trains efficiently on GPUs. The entire history is compressed into a constant-size vector; there is **no growing cache**.

```python
# SSM recurrence (conceptual): the whole past compressed into a FIXED-size state h
h = zeros(d_state)
for x_t in sequence:
    h   = A * h + B * x_t          # update fixed-size state — memory does NOT grow
    y_t = C @ h                    # produce the output for this token
# memory is O(d_state), INDEPENDENT of sequence length
```

The problem with classic SSMs was that `A, B, C` were **fixed** (input-invariant), so the model couldn't selectively remember or forget based on *content* — fatal for language. **Mamba** (Albert Gu & Tri Dao, Dec 2023) fixed this with the **selection mechanism**: make `Δ, B, C` **functions of the input** `x_t`, so the model decides, per token, what to keep and what to drop.

`Δ` (the **step size**) is the parameter that makes this click, and it deserves a beat of explanation. The SSM is defined in continuous time; `Δ_t` is how big a "time step" this token represents when the recurrence is **discretized** (`Ā = exp(Δ_t·A)`, `B̄ ≈ Δ_t·B`). Making it input-dependent turns it into a learned, per-token **remember-vs-skip gate**:

```text
   Δ_t → 0      Ā → I       state coasts through unchanged  →  token is IGNORED
   Δ_t large    Ā → 0       state resets toward this token   →  token OVERWRITES memory
```

That is the SSM's answer to attention's "attend or don't" — except the decision compresses into a fixed-size state instead of growing a cache. That single change made SSMs competitive with transformers on language while keeping the systems win:

```text
   attention:  O(L²) compute,  O(L) memory (growing KV cache)
   SSM/Mamba:  O(L)  compute,  O(1) memory per token (fixed state)  ← the whole point
```

The MLSys consequence is enormous: **memory and per-token compute are flat in context length.** A Mamba model's tokens/s does not degrade as the conversation grows, and its memory footprint at 1M tokens looks like its footprint at 1K. That is a different cost curve entirely.

---

**3. Mamba-2 and the duality that made hybrids possible**

Mamba-1 had one systems wart: its selective scan didn't map cleanly onto Tensor Cores (the matmul machinery), so it under-utilized the GPU. **Mamba-2** (Dao & Gu, May 2024) fixed that with a deep result — **Structured State Space Duality (SSD)**:

> A selective SSM is *mathematically equivalent* to a form of **masked attention**, via structured (semiseparable) matrices. So the same layer can be computed either as a linear recurrence **or** as a block of matmuls.

That duality is why Mamba-2 matters for this course. By expressing the SSM as **tensor-core-friendly matmuls** (a block decomposition), Mamba-2 runs **2–8× faster** than Mamba-1 and finally uses the GPU the way a transformer does. It simplified the state matrix `A` to a scalar-times-identity and added **multi-head** structure (like attention heads). The result became the **standard building block** for every hybrid below — you get the constant-state inference win *and* good training-time hardware utilization.

```text
   SSD:  one layer, two ways to compute it
        recurrence form  →  O(1) state, great for INFERENCE (decode)
        matmul form      →  tensor-core-friendly, great for TRAINING (parallel)
```

---

**4. The linear-attention family (same idea, different gates)**

Mamba is the most prominent, but it sits in a family of **sub-quadratic / linear-attention** methods that all share the systems pitch — **constant-memory recurrence instead of a growing KV cache** — and differ mainly in *how they gate* the state:

| Method | Gate / decay | One-line systems point |
|---|---|---|
| **RWKV** | channel-wise time decay | trains parallel like a transformer, runs as an O(1) RNN at inference |
| **RetNet** | fixed exponential decay ("retention") | three modes: parallel (train), recurrent (O(1) infer), chunkwise (long-seq) |
| **GLA** | **data-dependent** matrix gating | input-adaptive forget + a hardware-efficient chunkwise kernel |
| **Mamba-2** | **input-dependent** selective Δ | selection + SSD duality (the matmul form) |

The spectrum to remember: **fixed decay** (RWKV, RetNet — simpler, cheaper) → **input-dependent gating** (GLA, Mamba — adaptive, more expressive). All of them give the same headline: **flat memory and throughput as context grows.** What they give up versus full attention is some exact long-range *recall* — the ability to look up a specific earlier token precisely. Which is exactly the gap hybrids close.

---

</details>

## 5. 混合架构浪潮——2025 年的主导范式

纯 SSM 会损失一点精确召回能力；纯 attention 则撞上 KV 墙。2025 年的答案——如今无处不在——是**把两者混合**：保留*少量* full-attention layer 用于精确的上下文检索，把*其余*换成 SSM/linear。经验上反复出现的比例约为**每 7–10 个 recurrent layer 配 1 个 attention layer**。

```text
   a hybrid stack (schematic, ~1 : 7):
   [SSM][SSM][SSM][SSM][SSM][SSM][SSM][ATTN][SSM][SSM]...[ATTN]
        └──── constant-state, cheap, flat-in-context ────┘  └ a little exact recall ┘

   net effect:  KV cache ≈ (attention-layer fraction) × full-attention KV
                so ~1:7  →  KV cache ~1/8 the size  →  ~deeper batching, 2-3× long-ctx throughput
                quality stays near full-attention
```

你应该认得的实例：

* **Jamba**（AI21，2024 年 3 月）——首个生产级 hybrid：**52B 总参数 / 12B 激活**（它*同时*也是 MoE），**每 8 层夹 1 个 Transformer layer**，256K 上下文，**单张 80GB GPU 即可装下 140K tokens**，**长上下文吞吐约为 Mixtral 8×7B 的 3 倍**。两种 KV 缩减手段叠加：hybrid *和* MoE。
* **NVIDIA Nemotron-H**（2025 年 4 月）——**8B / 47B / 56B**，大部分 self-attention 被 **Mamba-2** 取代，attention 约占**层数的 8%**。47B **在 65K 上下文下比 Qwen-2.5-72B / Llama-3.1-70B 快约 2.9×**；用 **FP8** 训练，与 BF16 相比质量差异 <0.1%。这是 NVIDIA 的参考 hybrid，并喂给了后续的 Nemotron 推理模型。
* **Falcon-H1**（TII，2025 年 5 月）——**这就是你听说过的那个 “Falcon AI”。**它的新意在于**并行 hybrid-head** 设计：attention 与 Mamba-2 head **在同一 block 内并行**运行（而非作为独立的交错层），并带有**可独立调节的 attention:SSM 比例**。据称 34B 可媲美 70B 级模型；256K 上下文。可调的比例让内存/召回之间的取舍变成了一个*旋钮*。
* **IBM Bamba**（9B）——把 Mamba-2 与 attention 交错，在 vLLM 中**吞吐约 2.5×**——并带来最重要的一课：该加速**受制于推理栈对 SSM 状态管理的支持。**只有当*推理服务栈*（vLLM kernel、状态处理）支持它时，架构才真正兑现收益。架构上的胜利需要系统层面做工作；它们不是免费的。
* **MiniMax-01**（2025 年 1 月）——**456B 总参数 / 45.9B 激活**的 MoE，采用 **Lightning Attention**（一种 I/O 感知的 linear attention），**每 7 个 lightning layer 配 1 个 softmax-attention layer**，训练上下文 **1M token，推理时最高 4M**。它证明了**数百万 token 的上下文只有在 linear/hybrid attention 下才具备经济可服务性**——4M token 的 full-attention KV cache 会荒谬得离谱。

---

## 6. 另一根杠杆：压缩缓存（MLA）

混合架构*移除* attention layer。与之互补的做法是保留 attention，但**缩小每层的 KV**——DeepSeek 的 **Multi-head Latent Attention（MLA，多头潜在注意力）**（详见第 5 讲）。MLA 缓存的是**低秩 latent 向量**，而非完整的逐 head K 和 V，然后即时重建它们：

```text
   standard KV cache:  store full K, V per head      → big, O(L)
   MLA:                store a compressed LATENT      → much smaller constant factor, still O(L)
                       reconstruct K,V from it per step
```

所以这两个流派以不同方式攻击同一个敌人：**SSM/hybrid** 让大多数层无状态（O(1)）；**MLA** 让 attention layer 的缓存更便宜（更小的 O(L)）。现代前沿模型常常把这两种思路与 MoE 结合起来——这正是下一讲的主题。

---

## 7. 系统影响对照表

整讲内容，作为一份你应当能复现的参考：

| 架构 | 机制 | 内存随上下文 | 吞吐随上下文 | 代表模型 |
|---|---|---|---|---|
| **Dense attention** | 对全部历史做 softmax | **O(L)** 持续增长的 KV | 随 L 增大而退化 | Llama、Qwen-dense |
| **SSM (Mamba-2)** | 选择性递归 + SSD | **O(1)** 固定状态 | 平坦 | Mamba |
| **Linear attention** | 门控递归 | **O(1)** 固定状态 | 平坦 | RWKV、RetNet、GLA |
| **Hybrid（约 1:7–10）** | 少量 attn + 大量 SSM | ~若干分之一 × attn KV | 长上下文下 2–3× | Jamba、Nemotron-H、Falcon-H1、MiniMax-01 |
| **MLA** | 低秩 latent KV | 更小的 O(L) | 更深的 batch | DeepSeek V3/R1 |

以及把它与第 1 讲串起来的那句话：**KV cache 更小 → VRAM 中能放下更多请求 → 批处理更深 → 聚合 tokens/s 更高 → `$/Mtok` 更低。**架构协同设计不是关于准确率的故事，而是关于*成本*的故事。把 KV cache 砍掉 8× 的 hybrid，能让你在长上下文下把 batch 做深约 8×，这在成本公式的分母上是一个接近一个数量级的变动。

---


<details>
<summary>English original</summary>

**5. The hybrid wave — the dominant 2025 pattern**

Pure SSMs lose a little exact-recall ability; pure attention has the KV wall. The 2025 answer, now everywhere, is to **mix them**: keep a *few* full-attention layers for precise in-context retrieval, and make the *rest* SSM/linear. The empirically recurring ratio is about **1 attention layer per 7–10 recurrent layers**.

```text
   a hybrid stack (schematic, ~1 : 7):
   [SSM][SSM][SSM][SSM][SSM][SSM][SSM][ATTN][SSM][SSM]...[ATTN]
        └──── constant-state, cheap, flat-in-context ────┘  └ a little exact recall ┘

   net effect:  KV cache ≈ (attention-layer fraction) × full-attention KV
                so ~1:7  →  KV cache ~1/8 the size  →  ~deeper batching, 2-3× long-ctx throughput
                quality stays near full-attention
```

The instances you should recognize:

* **Jamba** (AI21, Mar 2024) — the first production-grade hybrid: **52B total / 12B active** (it's *also* MoE), **1 transformer layer per 8**, 256K context, fits **140K tokens on a single 80GB GPU**, ~**3× long-context throughput vs Mixtral 8×7B**. Two KV-reducers stacked: hybrid *and* MoE.
* **NVIDIA Nemotron-H** (Apr 2025) — **8B / 47B / 56B**, most self-attention replaced by **Mamba-2**, attention ≈ **8% of layers**. The 47B is **~2.9× faster than Qwen-2.5-72B / Llama-3.1-70B at 65K context**; trained in **FP8** with <0.1% quality difference vs BF16. This is NVIDIA's reference hybrid and it feeds later Nemotron reasoning models.
* **Falcon-H1** (TII, May 2025) — **this is the "Falcon AI" you've heard about.** Its twist is a **parallel hybrid-head** design: attention and Mamba-2 heads run **in parallel within the same block** (not as separate interleaved layers), with an **independently tunable attention:SSM ratio**. The 34B reportedly rivals 70B-class models; 256K context. The tunable ratio makes the memory/recall trade a *knob*.
* **IBM Bamba** (9B) — interleaves Mamba-2 with attention, **~2.5× throughput** in vLLM — and carries the most important lesson: the speedup is **gated by inference-stack support for SSM state management.** The architecture only pays off once the *serving stack* (vLLM kernels, state handling) supports it. Architecture wins require systems work; they are not free.
* **MiniMax-01** (Jan 2025) — **456B total / 45.9B active** MoE with **Lightning Attention** (an I/O-aware linear attention), **1 softmax-attention layer per 7** lightning layers, trained at **1M-token context, up to 4M at inference**. It demonstrates that **multi-million-token context is only economically serveable with linear/hybrid attention** — a full-attention KV cache at 4M tokens would be absurd.

---

**6. The other lever: compress the cache (MLA)**

Hybrids *remove* attention layers. The complementary approach keeps attention but **shrinks each layer's KV** — **Multi-head Latent Attention (MLA)**, from DeepSeek (detailed in Lecture 5). MLA caches a **low-rank latent vector** instead of the full per-head K and V, then reconstructs them on the fly:

```text
   standard KV cache:  store full K, V per head      → big, O(L)
   MLA:                store a compressed LATENT      → much smaller constant factor, still O(L)
                       reconstruct K,V from it per step
```

So the two families attack the same villain differently: **SSM/hybrid** makes most layers stateless (O(1)); **MLA** makes attention layers cheaper to cache (smaller O(L)). Modern frontier models often combine both ideas with MoE — which is exactly the subject of the next lecture.

---

**7. Systems-implication table**

The whole lecture, as a reference you should be able to reproduce:

| Architecture | Mechanism | Memory vs context | Throughput vs context | Exemplars |
|---|---|---|---|---|
| **Dense attention** | softmax over all past | **O(L)** growing KV | degrades as L grows | Llama, Qwen-dense |
| **SSM (Mamba-2)** | selective recurrence + SSD | **O(1)** fixed state | flat | Mamba |
| **Linear attention** | gated recurrence | **O(1)** fixed state | flat | RWKV, RetNet, GLA |
| **Hybrid (~1:7–10)** | few attn + many SSM | ~fraction × attn KV | 2–3× long-context | Jamba, Nemotron-H, Falcon-H1, MiniMax-01 |
| **MLA** | low-rank latent KV | smaller O(L) | deeper batch | DeepSeek V3/R1 |

And the line that ties it to Lecture 1: **a smaller KV cache → more requests fit in VRAM → deeper batching → higher aggregate tokens/s → lower `$/Mtok`.** Architecture co-design is not an accuracy story; it is a *cost* story. A hybrid that cuts the KV cache 8× can let you batch ~8× deeper at long context, which is a near-order-of-magnitude move on the denominator of the cost equation.

---

</details>

## 8. 动手 / 实测：把墙画出来

让 KV-cache 这堵墙变得可见，再看每种架构如何把它削平。

```python
def kv_cache_gb(layers, kv_heads, head_dim, seq_len, batch, dtype_bytes=2):
    return 2 * layers * kv_heads * head_dim * seq_len * batch * dtype_bytes / 1e9

# dense 70B-ish: watch it explode with context
for L in (4_000, 32_000, 128_000):
    gb = kv_cache_gb(layers=80, kv_heads=8, head_dim=128, seq_len=L, batch=1)
    print(f"dense  ctx={L:>7}: {gb:6.1f} GB KV")

# hybrid at ~1:7 → only ~1/8 the attention layers carry a KV cache
for L in (4_000, 32_000, 128_000):
    gb = kv_cache_gb(layers=10, kv_heads=8, head_dim=128, seq_len=L, batch=1)  # ~10 attn layers
    print(f"hybrid ctx={L:>7}: {gb:6.1f} GB KV   (SSM layers add a flat, tiny state)")

# and "flat, tiny" deserves a number — the SSM state, fixed regardless of context:
def ssm_state_gb(layers, heads, head_dim, d_state, batch, dtype_bytes=2):
    return layers * heads * head_dim * d_state * batch * dtype_bytes / 1e9

gb = ssm_state_gb(layers=64, heads=64, head_dim=64, d_state=128, batch=1)  # Mamba-2-ish 7B
print(f"SSM 'cache': {gb:.3f} GB  — at 1K context, at 128K, and at 1M. it does not move.")
```

稠密那一列攀升到几十 GB（70B 级那一行在 128K 下达到约 42 GB）；hybrid 那一列只是它的一小部分；而 SSM state 打印出来是 **~0.07 GB** —— 7B 级 Mamba-2 的整个“cache”比一层稠密 KV 还小，并且在 1K 和 1M token 时完全相同。作为参照，一个稠密 7B 级 Transformer（32 layer、8 个 KV head）在 128K 下携带约 17 GB 的 KV —— 约为 Mamba state 的 **250×**。买来 batch 深度的正是这个比值，而不是渐近的 O 记号。接着把省下的内存拿来计算 **batch 能加深多少**，以及这对聚合 tokens/s 和 `$/Mtok` 意味着什么（Lecture 1，§6）。这条链路 —— KV 字节 → batch 深度 → tokens/s → 美元 —— 就是交付物。

**推理服务上的注意点（Bamba 的教训）：** 只有当你的推理栈*实现*了 SSM state 管理时，这些收益才是真的。在承诺 8× 批处理收益之前，先确认 vLLM/SGLang（或你的 runtime）确实支持该架构的 state 处理 —— 否则你就是画了一张硬件无法兑现的图。

---

## 9. 迷你实验

1. **把墙画出来：** 对稠密模型、hybrid（调整 attention layer 数量）以及（概念上）MLA 复现 §8。把三者的 KV-GB 随 context 变化画成一张图。
2. **Batch 数学：** 在固定 GPU 内存预算下，计算每种架构在 128K context 下的最大 batch size，然后估算聚合 tokens/s 与 `$/Mtok` 的增量。
3. **读一个真实的：** 拉取 **Nemotron-H** 或 **Falcon-H1** 的 config，找到 attention:SSM 的 layer 比例，并预测它相对同规模稠密模型的 KV 占用。把你的预测与 model card 的 context/吞吐声明对照。

交付物：KV-对-context 图、batch 深度表，以及一段话：你会用哪种架构在 128K context 下提供推理服务，为什么，用 `$/Mtok` 的措辞来讲。这个论证就是 co-design 能力。

---

## 关键要点

- **KV cache** —— 随 context *和* batch 增长的 O(L) 内存 —— 是让长上下文推理服务昂贵、并限制 batch 深度（进而限制 TOK/$）的那堵墙。
- **SSM（Mamba）** 用**常量大小的循环状态**取代不断增长的 cache：每 token 的 O(1) 内存/计算，在 context 上**平坦**。**选择机制**（输入相关的动态）让它们能与 attention 竞争。
- **Mamba-2 / SSD** 证明了 SSM 与掩码 attention 对偶，给出了 **tensor-core 友好的矩阵乘形式** —— 快 2–8×，也是 SSM 成为**混合构建块**的原因。
- **linear-attention 家族**（RWKV、RetNet、GLA）共享恒内存循环的优势，区别在门控（固定衰减 → 输入相关）。
- **混合浪潮**（约 1 attention : 7–10 SSM）保留少量精确召回，同时把 KV cache 削减到约等于 attention layer 所占比例：**2–3× 长上下文吞吐**，且接近全 attention 的质量（Jamba、Nemotron-H、Falcon-H1、MiniMax-01）。
- **MLA** 是互补的杠杆 —— 压缩每个 attention layer 的 KV。更小的 KV（无论走哪条路）→ **更深的批处理 → 更多 tokens/s → 更低的 `$/Mtok`** —— 但前提是**推理服务栈支持该架构**（Bamba 的教训）。

---


<details>
<summary>English original</summary>

**8. Hands-on / Measure it: plot the wall**

Make the KV-cache wall visible, then watch each architecture flatten it.

```python
def kv_cache_gb(layers, kv_heads, head_dim, seq_len, batch, dtype_bytes=2):
    return 2 * layers * kv_heads * head_dim * seq_len * batch * dtype_bytes / 1e9

# dense 70B-ish: watch it explode with context
for L in (4_000, 32_000, 128_000):
    gb = kv_cache_gb(layers=80, kv_heads=8, head_dim=128, seq_len=L, batch=1)
    print(f"dense  ctx={L:>7}: {gb:6.1f} GB KV")

# hybrid at ~1:7 → only ~1/8 the attention layers carry a KV cache
for L in (4_000, 32_000, 128_000):
    gb = kv_cache_gb(layers=10, kv_heads=8, head_dim=128, seq_len=L, batch=1)  # ~10 attn layers
    print(f"hybrid ctx={L:>7}: {gb:6.1f} GB KV   (SSM layers add a flat, tiny state)")

# and "flat, tiny" deserves a number — the SSM state, fixed regardless of context:
def ssm_state_gb(layers, heads, head_dim, d_state, batch, dtype_bytes=2):
    return layers * heads * head_dim * d_state * batch * dtype_bytes / 1e9

gb = ssm_state_gb(layers=64, heads=64, head_dim=64, d_state=128, batch=1)  # Mamba-2-ish 7B
print(f"SSM 'cache': {gb:.3f} GB  — at 1K context, at 128K, and at 1M. it does not move.")
```

The dense column climbs into tens of GB (the 70B-class row hits ~42 GB at 128K); the hybrid column is a fraction of it; and the SSM state prints **~0.07 GB** — the entire "cache" of a 7B-class Mamba-2 is smaller than one layer's worth of dense KV, and it is identical at 1K and at 1M tokens. For calibration, a dense 7B-class transformer (32 layers, 8 KV heads) carries ~17 GB of KV at 128K — roughly **250×** the Mamba state. That ratio, not the asymptotic O-notation, is what buys you batch depth. Then take the freed memory and compute how much **deeper you can batch**, and what that does to aggregate tokens/s and `$/Mtok` (Lecture 1, §6). That chain — KV bytes → batch depth → tokens/s → dollars — is the deliverable.

**The serving caveat (the Bamba lesson):** these wins are only real if your inference stack *implements* SSM state management. Before promising an 8× batching win, confirm vLLM/SGLang (or your runtime) actually supports the architecture's state handling — or you've drawn a chart the hardware can't cash.

---

**9. Mini-lab**

1. **Plot the wall:** reproduce §8 for a dense model, a hybrid (adjust the attention-layer count), and (conceptually) MLA. Make one chart of KV-GB vs context for all three.
2. **Batch math:** for a fixed GPU memory budget, compute max batch size at 128K context for each architecture, then estimate aggregate tokens/s and `$/Mtok` deltas.
3. **Read a real one:** pull the config of **Nemotron-H** or **Falcon-H1**, find the attention:SSM layer ratio, and predict its KV footprint relative to a same-size dense model. Check your prediction against the model card's context/throughput claims.

Deliverable: the KV-vs-context chart, the batch-depth table, and one paragraph: which architecture would you serve at 128K context and why, in `$/Mtok` terms. That argument is the co-design skill.

---

**Key takeaways**

- The **KV cache** — O(L) memory that grows with context *and* batch — is the wall that makes long-context serving expensive and caps batch depth (and therefore TOK/$).
- **SSMs (Mamba)** replace the growing cache with a **constant-size recurrent state**: O(1) memory/compute per token, **flat** in context. **Selection** (input-dependent dynamics) made them competitive with attention.
- **Mamba-2 / SSD** proved SSMs are dual to masked attention, giving a **tensor-core-friendly matmul form** — 2–8× faster, and the reason SSMs became the **hybrid building block**.
- The **linear-attention family** (RWKV, RetNet, GLA) shares the constant-memory-recurrence win, differing by gate (fixed decay → input-dependent).
- The **hybrid wave** (~1 attention : 7–10 SSM) keeps a little exact recall while cutting the KV cache to ~the attention-layer fraction: **2–3× long-context throughput** at near-full-attention quality (Jamba, Nemotron-H, Falcon-H1, MiniMax-01).
- **MLA** is the complementary lever — compress the KV per attention layer. Smaller KV (either way) → **deeper batching → more tokens/s → lower `$/Mtok`** — but only if the **serving stack supports the architecture** (the Bamba lesson).

---

</details>

## References

- Gu & Dao, "Mamba: Linear-Time Sequence Modeling with Selective State Spaces," arXiv 2312.00752: [https://arxiv.org/abs/2312.00752](https://arxiv.org/abs/2312.00752)
- Dao & Gu, "Transformers are SSMs (SSD / Mamba-2)," arXiv 2405.21060: [https://arxiv.org/abs/2405.21060](https://arxiv.org/abs/2405.21060)
- AI21, Jamba, arXiv 2403.19887: [https://arxiv.org/abs/2403.19887](https://arxiv.org/abs/2403.19887)
- NVIDIA, Nemotron-H, arXiv 2504.03624 · [https://research.nvidia.com/labs/adlr/nemotronh/](https://research.nvidia.com/labs/adlr/nemotronh/)
- TII, Falcon-H1: [https://falcon-lm.github.io/blog/falcon-h1/](https://falcon-lm.github.io/blog/falcon-h1/)
- IBM, Bamba (vLLM 中的 SSM-Transformer): [https://research.ibm.com/blog/bamba-ssm-transformer-model](https://research.ibm.com/blog/bamba-ssm-transformer-model)
- MiniMax-01 (Lightning Attention), arXiv 2501.08313: [https://arxiv.org/pdf/2501.08313](https://arxiv.org/pdf/2501.08313)

---

## 截至

2026-06。已锁定架构：Mamba（2023 年 12 月）、Mamba-2/SSD（2024 年 5 月）、Jamba（2024 年 3 月）、Nemotron-H 8/47/56B（2025 年 4 月）、Falcon-H1（2025 年 5 月）、Bamba-9B、MiniMax-01（2025 年 1 月）。吞吐倍数（如 Nemotron-H 47B 在 65K 下约 2.9×）是特定上下文长度下的模型卡数据——依赖之前需对照当前推理服务栈的支持情况重新核实。

---

*Next: [Lecture 05 — The 2026 frontier as systems artifacts](Lecture-05.md)*


<details>
<summary>English original</summary>

**References**

- Gu & Dao, "Mamba: Linear-Time Sequence Modeling with Selective State Spaces," arXiv 2312.00752: [https://arxiv.org/abs/2312.00752](https://arxiv.org/abs/2312.00752)
- Dao & Gu, "Transformers are SSMs (SSD / Mamba-2)," arXiv 2405.21060: [https://arxiv.org/abs/2405.21060](https://arxiv.org/abs/2405.21060)
- AI21, Jamba, arXiv 2403.19887: [https://arxiv.org/abs/2403.19887](https://arxiv.org/abs/2403.19887)
- NVIDIA, Nemotron-H, arXiv 2504.03624 · [https://research.nvidia.com/labs/adlr/nemotronh/](https://research.nvidia.com/labs/adlr/nemotronh/)
- TII, Falcon-H1: [https://falcon-lm.github.io/blog/falcon-h1/](https://falcon-lm.github.io/blog/falcon-h1/)
- IBM, Bamba (SSM-Transformer in vLLM): [https://research.ibm.com/blog/bamba-ssm-transformer-model](https://research.ibm.com/blog/bamba-ssm-transformer-model)
- MiniMax-01 (Lightning Attention), arXiv 2501.08313: [https://arxiv.org/pdf/2501.08313](https://arxiv.org/pdf/2501.08313)

---

**Current as of**

2026-06. Architectures pinned: Mamba (Dec 2023), Mamba-2/SSD (May 2024), Jamba (Mar 2024), Nemotron-H 8/47/56B (Apr 2025), Falcon-H1 (May 2025), Bamba-9B, MiniMax-01 (Jan 2025). Throughput multipliers (e.g. Nemotron-H 47B ~2.9× at 65K) are model-card figures at specific context lengths — re-verify against current serving-stack support before relying on them.

---

*Next: [Lecture 05 — The 2026 frontier as systems artifacts](Lecture-05.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/MLSys Deep Dives/Lecture-04.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/MLSys%20Deep%20Dives/Lecture-04.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
