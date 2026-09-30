---
title: MLSys Engineer — 推理系统 Q&A
description: MLSys Engineer — 推理系统 Q&A
---

# MLSys Engineer — 推理系统 Q&A

**所属合集：** [MLSys Engineer](README.md) | **上级：** [Interview Preparation](../README.md)

`#kv-cache` `#attention` `#speculative-decode` `#latency` `#trt-llm` `#edge`

---

> 这些答案按 senior/staff 级别撰写。每一条都包含问题陈述、机制、真正的工程洞察，以及至少一个生产环境的坑。在实际面试中，每个答案控制在 3–4 分钟——长到足以讲到关键取舍，短到面试官不会在你讲到重点之前打断你。

---

## Q1. PagedAttention 如何工作？

**它解决的问题：** 朴素的推理服务在分配时为每个请求预留一块连续的 `max_seq_len` KV 缓冲区。这会以两种方式浪费内存：内部碎片（预留块中未被填充的槽位）与外部碎片（请求之间的空间无法重新打包）。vLLM 在该模型下测得 60–80% 的有效内存浪费。

**机制：** PagedAttention 把操作系统的虚拟内存分页机制用到 KV cache 上。缓存被划分为固定大小的 **block**（通常每块 16 个 token 的 K 和 V）。每个序列有一张 **block table**——从逻辑 token 位置 → 物理 block 索引的映射——由 CPU 侧的 block manager 维护。attention kernel 被改写为通过这层间接寻址来收集 K 和 V，而不是访问连续缓冲区。

**为什么有用：**

- 内部碎片被限制在一个 block 以内（每个序列浪费 ≤15 个 token 槽位，而不是最多 `max_seq_len`）
- block 在物理内存中无需连续 → 分配器可以复用零散的页
- 内容相同的 block 可以 **copy-on-write 共享**：beam search 的各分支在分叉之前共享前缀；并行采样共享 prompt；常驻的 system prompt 前缀在所有请求之间共享（**prefix caching**）
- KV 增长完全动态——无需预先承诺序列长度

**kernel 改写：** 核心变化在 attention kernel 的 K/V 访问模式。连续 kernel 索引的是 `K[seq_offset + i]`，而分页 kernel 解引用的是 `K[block_table[i // block_size] * block_stride + (i % block_size)]`。这层间接寻址要额外消耗几个寄存器，但相对 HBM 带宽这一瓶颈可以忽略不计。

**生产注意事项——prefix caching 的有效性：** 缓存条目以 token-id 哈希为键。只要 BOS/EOS 或 system prompt 有一处不同，整个前缀就会失效。在多租户推理服务中，缓存命中率高度依赖工作负载——在声称 prefix cache 有加速之前，先用你的实际流量做 benchmark。

**边缘场景：** 在单流边缘设备上，多租户带来的收益更小。我把预分配池的大小设为 `actual_context_budget`，而没有采用分页 block。但前缀共享的思路可以直接迁移：我的跨轮次常驻 KV 缓冲区（warm start 时从磁盘加载填充）就是同一个洞察——不要对已经算过的前缀重新执行 prefill（首字前的整段计算）。

---


<details>
<summary>English original</summary>

**MLSys Engineer — Inference Systems Q&A**

**Collection:** [MLSys Engineer](README.md) | **Up:** [Interview Preparation](../README.md)

`#kv-cache` `#attention` `#speculative-decode` `#latency` `#trt-llm` `#edge`

---

> These answers are written at senior/staff level. Each one has a problem statement, a mechanism, the real engineering insight, and at least one production gotcha. In an actual interview, aim for 3–4 minutes per answer — long enough to hit the key tradeoff, short enough that the interviewer doesn't interrupt before you get there.

---

**Q1. How does PagedAttention work?**

**The problem it solves:** naive serving reserves a contiguous `max_seq_len` KV buffer per request at allocation time. That wastes memory two ways: internal fragmentation (unfilled slots in the reserved block) and external fragmentation (you can't repack the space between requests). vLLM measured 60–80% effective memory waste in this model.

**The mechanism:** PagedAttention applies OS virtual-memory paging to the KV cache. The cache is partitioned into fixed-size **blocks** (typically 16 tokens of K and V per block). Each sequence has a **block table** — a mapping from logical token position → physical block index — that is maintained by a CPU-side block manager. The attention kernel is rewritten to gather K and V through that indirection rather than accessing a contiguous buffer.

**Why this helps:**

- Internal fragmentation is bounded by one block (≤15 wasted token slots per sequence instead of up to `max_seq_len`)
- Blocks need not be contiguous in physical memory → the allocator can reuse scattered pages
- Blocks with identical content can be **copy-on-write shared**: beam search branches share prefixes until they diverge; parallel samples share the prompt; persistent system-prompt prefixes are shared across all requests (**prefix caching**)
- KV growth is fully dynamic — no pre-commitment to a sequence length

**The kernel rewrite:** the core change is in the attention kernel's K/V access pattern. Where a contiguous kernel indexes `K[seq_offset + i]`, a paged kernel dereferences `K[block_table[i // block_size] * block_stride + (i % block_size)]`. This indirection costs a few registers but is negligible against the HBM bandwidth bound.

**Production caveat — prefix caching validity:** cache entries are keyed by token-id hashes. A single BOS/EOS or system-prompt variation invalidates the entire prefix. In multi-tenant serving, cache hit rate is heavily workload-dependent — benchmark it on your actual traffic before claiming prefix-cache speedup.

**Edge context:** on a single-stream edge device the multi-tenant win is smaller. I sized a pre-allocated pool to `actual_context_budget` instead of adopting paged blocks. But the prefix-sharing idea transfers directly: my persistent cross-turn KV buffer (hydrated from disk on warm start) is the same insight — don't re-prefill a prefix you already computed.

---

</details>

## Q2. 如何在 TensorRT-LLM 中实现 EAGLE-3？

**EAGLE-3 做了什么：** EAGLE 在特征层面而非 token 层面做 draft。一个很小的 draft head 预测目标模型下一位置的**隐藏状态**，然后从 draft logits 中采样，构建一棵候选 token 树。目标模型用 tree attention mask 在一次前向传播中验证整棵树。EAGLE-3 特别去掉了 EAGLE-1/2 的特征回归 loss 约束，改为把目标模型低层、中层和末层的隐藏状态融合起来作为 draft 输入——这提升了接受长度，同时没有回归约束带来的不稳定性。

**在 TRT-LLM 中具体分阶段实现：**

**阶段 1 —— 暴露目标模型的隐藏状态。** 修改 base model 的 TRT engine，使其把三层（靠前 ≈ L/4、中间 ≈ L/2、接近末层）的隐藏状态作为额外输出。在 TRT-LLM 的 Eagle 实现（`eagle_base`）中，这就是 `emit_hidden_states` flag——base engine 会把这些检查点拼接后的隐藏状态与 logits 一并导出。

**阶段 2 —— draft head。** 一个 1–2 层的小 Transformer（即 EAGLE draft head）以拼接后的隐藏状态为输入（不做 context 重编码），自回归地构建一棵 draft token 树。树的宽度和深度可配置；典型配置：深度 1 有 4–6 个候选，深度 2 有 2–3 个（共 10–20 个节点）。

**阶段 3 —— tree attention。** 验证时目标模型一次对所有树节点同时运行。每个节点只 attend 它在树中的祖先——实现方式是自定义 `attention_mask`（在树内为上三角，并具备相应的父子结构）加自定义 `attention_pos_id`，使每个节点的位置编码与其在因果前缀中的深度一致。TRT-LLM 的 attention plugin 正是为这种情况提供了 `attention_mask` 和 `position_ids` 覆盖。

**阶段 4 —— 接受 + KV 压缩。** 从根节点开始遍历树，接受最长的匹配前缀（与标准 spec-decode 相同的随机接受规则），只保留被接受节点的 KV 条目，丢弃被拒绝的分支。把树状 KV 压缩成线性 KV 是最麻烦的部分：它是对 KV buffer 的一次原地 gather，以被接受 token 的树路径为索引。

**阶段 5 —— 注册为 drafter。** 接入 TRT-LLM 的 spec-decode 调度循环：draft N 棵树 → 验证批 → 接受 → 继续。在 batch=1 时收益很大，因为目标模型的前向传播受权重读取限制（weight-read-bound）——验证 K 个 token 消耗的 HBM 与 decode 1 个 token（逐 token 生成阶段）大致相同，因此在接受率良好时，约 1 个 token 的带宽代价就能换来 K 个被接受的 token。

**坑点：** EAGLE 对特征的依赖意味着 `verify(t)` 完成前 `draft(t+1)` 无法启动（它需要 t 时刻目标模型的隐藏状态）。与 token 级的外部 draft（1B 模型）不同，无法跨步骤让 draft 与 verify 重叠。在并行度低的边缘硬件上，这会降低实际耗时上的收益。

---


<details>
<summary>English original</summary>

**Q2. How would you implement EAGLE-3 in TensorRT-LLM?**

**What EAGLE-3 does:** EAGLE drafts at the feature level rather than the token level. A tiny draft head predicts the target's **hidden state** for the next position, then samples from the draft logits to build a tree of candidate tokens. The target verifies the entire tree in one forward pass using a tree attention mask. EAGLE-3 specifically drops the feature-regression loss constraint of EAGLE-1/2 and instead fuses hidden states from low, mid, and final target layers as the draft input — this raises acceptance length without the regression constraint's instability.

**Concretely in TRT-LLM, phased:**

**Phase 1 — expose target hidden states.** Modify the base model's TRT engine to emit hidden states from three layers (early ≈ L/4, mid ≈ L/2, near-final) as additional outputs. In TRT-LLM's Eagle implementation (`eagle_base`), this is the `emit_hidden_states` flag — the base engine exports concatenated hidden states from those checkpoints alongside the logits.

**Phase 2 — draft head.** A small 1–2 layer transformer (the EAGLE draft head) takes the concatenated hidden states as input (no context re-encoding) and autoregressively builds a draft token tree. The tree has configurable width and depth; typical: 4–6 candidates at depth 1, 2–3 at depth 2 (total 10–20 nodes).

**Phase 3 — tree attention.** Verification runs the target once on all tree nodes simultaneously. Each node attends only to its ancestors in the tree — implemented as a custom `attention_mask` (upper-triangular within the tree, with the appropriate parent-child structure) plus custom `attention_pos_id` so that each node's position encoding matches its depth in the causal prefix. TRT-LLM's attention plugin accepts `attention_mask` and `position_ids` overrides for exactly this case.

**Phase 4 — accept + KV compaction.** Walk the tree from root, accept the longest matching prefix (same stochastic acceptance rule as standard spec-decode), keep only the accepted nodes' KV entries, discard rejected branches. Compacting the tree KV → linear KV is the fiddly part: it's an in-place gather over the KV buffer, keyed by the accepted token's tree path.

**Phase 5 — register as a drafter.** Wire into TRT-LLM's spec-decode scheduling loop: draft N trees → verify batch → accept → continue. At batch=1 the payoff is large because the target forward is weight-read-bound — verifying K tokens costs roughly the same HBM as decoding 1, so you get K accepted tokens for ~1 token's bandwidth cost (at good acceptance rates).

**Gotcha:** EAGLE's feature dependence means `draft(t+1)` cannot start until `verify(t)` completes (it needs the target's hidden states from t). Unlike token-level external draft (1B model), you cannot overlap draft and verify across steps. On edge hardware with low parallelism this reduces the wall-clock benefit.

---

</details>

## Q3. 如何在内存受限的设备上优化 KV cache？

**背景：** 我的 Orin 工作面向 8 GB 共享 LPDDR5 与 Gemma 4 4B —— 每个字节都很关键。

**优先级顺序：**

**1. 把 KV 量化到 INT8。** 逐 token 缩放（scale = `max(|K_row|) / 127`），在 attention softmax 之前反量化。相比 FP16 内存减少约 50%，且在典型激活值上的漂移有 FP16-ULP 边界。我把它作为大多数模型的默认配置交付。

*Gemma 4 例外：* Gemma 4 的 V 激活值在某些 layer 上有约 10× 的 RMS 离群值。逐行 INT8 会把它们压成截断值，导致长上下文摘要上出现可见的质量下降。我对 Gemma 4 强制使用 FP16 KV，并记录在案。教训：离群值感知方案（保留少量逐通道 FP16 槽位）能推广 INT8 做法 —— 但要按架构逐一验证。

**2. 利用分组查询注意力/MQA。** 每减少一个 KV head，KV 内存就线性下降。Gemma 4 4B 使用 4 个 KV head（对比 8 个 Q head）→ 在任何量化之前，架构层面就带来 2× 的 KV 缩减。分组查询注意力是 KV 内存上杠杆率最高的单一架构旋钮。

**3. 计入 KV 共享 layer。** Gemma 4 有 34 个 transformer layer，其中只有 15 个 **拥有**自己的 KV（末尾 19 个 layer 通过 `kv_shared_layers` 映射复用前面某个 layer 的 K/V）。按 15 个 layer 而非 34 个 layer 分配 KV pool —— 相比朴素估算，pool 大小减少 57%。

**4. 限制滑动窗口 KV。** Gemma 4 的局部 attention layer 只需要最后 W=1024 个 token，而不是完整上下文。把 KV buffer 实现为每个局部 layer 一个含 W 项的环。局部 layer 的 KV 与上下文长度无关，恒为 O(1)。

**5. 预分配 pool + OOM 防护。** 在边缘，绝不让 KV 无界增长 —— 启动时按硬上限一次性分配完整 pool，pool 满时拒绝新请求，而不是冒生成中途 OOM 的风险。需要计入：

```text
pool_bytes = num_own_layers × num_kv_heads × max_ctx_tokens × head_dim × dtype_bytes
           + overhead (block tables, metadata)
```

**6. 持久化前缀（前缀缓存）。** 热启动时从磁盘加载 system prompt / 持久上下文的 KV。这能降低第 2–N 轮的首 token 时延，并避免对已经算过的上下文重新做 prefill（首字前的整段计算）。实测收益：在 1261 token 上下文上，冷启动首 token 时延 877 ms → 热启动 444 ms。

**7. 为合并访问而布局。** 按 `[layer, head, token, dim]` 顺序存放 KV，使单个 warp 能连续读取一个 head 的 token 切片。`cache_head_dim` 步长字段用于处理不规整的 head 维度（Gemma 4 的 256 维滑动 K 为对齐存放在 512 宽的槽位中）。

**8. 驱逐（最后手段）。** 面向极端上下文，采用 StreamingLLM 式的 sink+recent 驱逐（保留前 S 个 token + 后 W 个 token）或 H2O（heavy hitters）。这些做法会降低输出质量 —— 记录取舍并实测。

---


<details>
<summary>English original</summary>

**Q3. How do you optimize KV cache on memory-constrained devices?**

**Context:** my Orin work targeted 8 GB shared LPDDR5 with Gemma 4 4B — every byte matters.

**Priority order:**

**1. Quantize KV to INT8.** Per-token scaling (scale = `max(|K_row|) / 127`), dequantize before attention softmax. ~50% memory reduction vs FP16, and FP16-ULP-bounded drift on typical activations. I shipped it as default for most models.

*Gemma 4 exception:* Gemma 4's V activations have ~10× RMS outliers in certain layers. Per-row INT8 collapses these into clipped values, causing visible quality degradation on long-context summarization. I force FP16 KV for Gemma 4 and document it. Lesson: outlier-aware schemes (keeping a few per-channel FP16 slots) generalize the INT8 approach — but validate per-architecture.

**2. Exploit GQA/MQA.** Each KV head reduction cuts KV memory linearly. Gemma 4 4B uses 4 KV heads (vs 8 Q heads) → 2× KV reduction at the architecture level, before any quantization. GQA is the single highest-leverage architectural knob for KV memory.

**3. Account for KV-sharing layers.** Gemma 4 has 34 transformer layers, of which only 15 **own** their own KV (the trailing 19 layers reuse a prior layer's K/V via the `kv_shared_layers` mapping). Allocate the KV pool for 15 layers, not 34 — a 57% reduction in pool size vs naive accounting.

**4. Cap sliding-window KV.** Gemma 4's local-attention layers only need the last W=1024 tokens, not the full context. Implement the KV buffer as a ring of W entries per local layer. Local layer KV is O(1) regardless of context length.

**5. Pre-allocate the pool + OOM guard.** On edge, never let KV grow unbounded — allocate the full pool at startup with a hard limit, refuse new requests if the pool is full rather than risking mid-generation OOM. Account for:

```text
pool_bytes = num_own_layers × num_kv_heads × max_ctx_tokens × head_dim × dtype_bytes
           + overhead (block tables, metadata)
```

**6. Persist prefixes (prefix caching).** Hydrate the system-prompt / persistent context KV from disk on warm start. This cuts TTFT on turns 2–N and avoids re-prefilling context you already computed. My measured win: 877 ms cold TTFT → 444 ms warm TTFT on a 1261-token context.

**7. Layout for coalesced access.** Store KV in `[layer, head, token, dim]` order so that a single warp reads one head's token slice contiguously. A `cache_head_dim` stride field handles ragged head dims (Gemma 4's 256-d sliding K stored in 512-wide slots for alignment).

**8. Eviction (last resort).** StreamingLLM-style sink+recent eviction (keep first S tokens + last W tokens) or H2O (heavy hitters) for extreme context. These degrade output quality — document the tradeoff and measure it.

---

</details>

## Q4. 如何编写一个融合 attention kernel？

**核心思路（FlashAttention 分块）：** 绝不实际生成完整的 N×N attention score 矩阵。对 Q 分块；循环遍历 K/V 分块；对每个分块：在寄存器或 SRAM 中计算 `S = Q·Kᵀ`，应用 online softmax，累加 `O += P·V`，最后只写一次 O。

**Online softmax：** 为每个 query 维护运行最大值 `m` 和运行分母 `l`。当新的 K/V 分块产生新的最大值 `m_new > m_old` 时，在加入新分块的贡献之前，用 `exp(m_old - m_new)` 重新缩放现有的 O 累加器和 `l`。这样无需实际生成 S 即可保持数值正确性。

**在真实硬件上至关重要的工程细节：**

**Tensor Core（MMA）：** 使用 `m16n8k16` 或 `m16n8k32` MMA atom（fp16/bf16 输入，fp32 累加）来完成 `Q·Kᵀ` 和 `P·V` 两者。将 softmax 中间结果和 O 累加器保持为 fp32。

*我在 Gemma 4 上踩到的精度陷阱：* `P·V` 矩阵乘中的 fp16 累加会在 V 离群值上出错——累加和会被截断。fp16 输入/fp32 累加没问题（即使在 20× 离群值幅度下，实测 rel-RMS 为 3e-4）。规则：始终用 fp32 累加；只有 MMA 输入可以为 fp16/bf16。

**双缓冲 K/V 分块：** 在分块 `t` 的 MMA 执行时，为分块 `t+1` 发起 `cp.async` 加载。这样就把全局内存延迟（HBM 访问）与计算重叠起来。在 Orin 上 D=512、4K 上下文下实测影响：1289ms → 348ms——这是我 kernel 中迄今为止最大的单项延迟收益。

**跳过完全被掩码的分块：** 对于滑动窗口 attention，最早相关的 K 分块从 `kt_start = ((q_pos + 1 - W) / KT) * KT` 开始。对于完全在窗口之外的分块，连加载都不要发起——它们对输出的贡献为零，还能节省 HBM 带宽和 MMA 周期。

**Occupancy：** 将 O 累加器放在**寄存器**中，而不是共享内存。共享内存中的 O 会将 occupancy 限制到每个 SM 约 1 个活跃 block，因为每个 block 的共享内存分配太大。代价是 online-softmax 的重新缩放必须直接作用于 MMA 寄存器 fragment——你需要知道特定 MMA atom 的 lane 到输出行的映射（`lane gid` 拥有行 `{gid, gid+8}`，列 `{2t, 2t+1}`）。

**先看 roofline（性能上界模型）：** 在 Orin 的 8-SM iGPU 上，我测得我的 attention kernel 是带宽受限的（算术强度 ≈ 16–32 FLOP/byte，远低于约 190 FLOP/byte 的 ridge point）。真正的收益来自 **queries-per-block**：启动 16 个 query 共享每个加载的 K/V 分块 = 对 DRAM 流量进行 16× 摊销，而不是更快的 MMA。当瓶颈是字节数时，不要动辄使用张量核心。

---

## Q5. FlashAttention 为什么能减少 HBM 流量？

**朴素 attention 的做法：** 将 `S = QKᵀ`（N×N fp16 矩阵）写入 HBM → 读回来做 softmax → 写入 `P`（N×N）→ 读取 `P` 用于 `P·V`。在序列长度 N=4096、D=128 时，仅一层的 attention 矩阵就产生约 4 GB 的 HBM 流量——而 softmax 在它上面纯粹是带宽受限的。

**FlashAttention 的做法：** 对计算分块，使 `S` 和 `P` 分块从不离开 SRAM/寄存器。kernel 从 HBM 中基本只读取一次 Q、K、V，并只写一次 O。HBM 流量从 O(N² · d) 降至 O(N · d)。

**取舍：** 更多 FLOPs。online-softmax 的重新缩放是额外的算术；反向 pass 会重新计算 S 分块，而不是把它们存起来。但关键洞察是，在典型的 N 和 D 下 attention 是**带宽受限的**，因此用充裕的 FLOP 换稀缺的带宽总是正确的。瓶颈从 HBM 带宽上移开。

**FlashAttention 何时不再有帮助：** 非常短的序列（N < 512），此时 N×N 矩阵小到足以放进缓存，或者极端的 `D`（head_dim=256 或更大），此时 Q·Kᵀ 的算术强度最终推向算力受限。Gemma 4 的 head_dim=256 正好处在边缘——在 N=4096 时它仍是带宽受限的，但在 N=512、D=256 时，分块计算开始占主导。

**反向 pass：** 延续同样的思路。反向时不再存储 P（N×N 激活值），Flash 会在反向 pass 期间在 SRAM 中重新计算 S。反向的内存占用从 O(N²) 降至 O(N)。这也是 FlashAttention 能大幅提升长上下文训练内存效率的原因。

---


<details>
<summary>English original</summary>

**Q4. How would you write a fused attention kernel?**

**Core idea (FlashAttention tiling):** never materialize the full N×N attention score matrix. Tile Q; loop over K/V tiles; for each tile: compute `S = Q·Kᵀ` in registers or SRAM, apply online softmax, accumulate `O += P·V`, write O once at the end.

**Online softmax:** maintain per-query running max `m` and running denominator `l`. When a new K/V tile produces a new max `m_new > m_old`, rescale the existing O accumulator and `l` by `exp(m_old - m_new)` before adding the new tile's contribution. This keeps numerical correctness without materializing S.

**Engineering details that matter on real hardware:**

**Tensor cores (MMA):** use `m16n8k16` or `m16n8k32` MMA atoms (fp16/bf16 input, fp32 accumulate) for both `Q·Kᵀ` and `P·V`. Keep the softmax intermediates and O accumulator in fp32.

*Precision trap I hit on Gemma 4:* fp16 accumulation in the `P·V` matmul breaks on V-outliers — the accumulated sum clips. fp16-inputs/fp32-accumulate is fine (measured rel-RMS 3e-4 even at 20× outlier amplitudes). Rule: always accumulate in fp32; only the MMA inputs can be fp16/bf16.

**Double-buffer K/V tiles:** issue `cp.async` loads for tile `t+1` while tile `t`'s MMA is executing. This overlaps global-memory latency (HBM access) with compute. Measured impact on D=512, 4K context on Orin: 1289ms → 348ms — by far the single biggest latency win in my kernel.

**Skip fully-masked tiles:** for sliding-window attention, the earliest relevant K tile starts at `kt_start = ((q_pos + 1 - W) / KT) * KT`. Don't even issue a load for tiles that are entirely outside the window — they contribute zero to the output and you save both HBM bandwidth and MMA cycles.

**Occupancy:** keep the O accumulator in **registers**, not shared memory. Shared-memory O limits occupancy to ~1 active block per SM because the shared allocation per block is too large. The cost is that the online-softmax rescale must operate directly on MMA register fragments — you need to know the lane-to-output-row mapping for your specific MMA atom (`lane gid` owns rows `{gid, gid+8}`, columns `{2t, 2t+1}`).

**Roofline first:** on Orin's 8-SM iGPU I measured my attention kernel as bandwidth-bound (arithmetic intensity ≈ 16–32 FLOP/byte, far below the ~190 FLOP/byte ridge point). The real win was **queries-per-block**: launching 16 queries that share each loaded K/V tile = 16× amortization of DRAM traffic, not faster MMA. Don't reach for tensor cores when the bottleneck is bytes.

---

**Q5. Why does FlashAttention reduce HBM traffic?**

**What naive attention does:** write `S = QKᵀ` (N×N fp16 matrix) to HBM → read it back for softmax → write `P` (N×N) → read `P` for `P·V`. At sequence length N=4096, D=128, that's ~4 GB of HBM traffic just for the attention matrix on one layer — and softmax is purely memory-bound over it.

**What FlashAttention does:** tile the computation so that `S` and `P` tiles never leave SRAM/registers. The kernel reads Q, K, V essentially once from HBM and writes O once. HBM traffic drops from O(N² · d) to O(N · d).

**The tradeoff:** more FLOPs. The online-softmax rescale is extra arithmetic; the backward pass recomputes S tiles instead of stashing them. But the key insight is that attention is **memory-bound at typical N and D**, so trading abundant FLOP for scarce bandwidth is always correct. The bottleneck moves off HBM bandwidth.

**When FlashAttention stops helping:** very short sequences (N < 512) where the N×N matrix is small enough to fit in cache anyway, or extreme `D` (head_dim=256 or larger) where the Q·Kᵀ arithmetic intensity finally pushes toward compute-bound. Gemma 4's head_dim=256 is right at the edge — at N=4096 it's still bandwidth-bound, but at N=512 with D=256 the tile compute starts to dominate.

**Backward pass:** extends the same idea. Instead of storing P (N×N activations) for the backward, Flash recomputes S in SRAM during the backward pass. Memory for backward drops from O(N²) to O(N). This is why FlashAttention dramatically improves long-context training memory efficiency too.

---

</details>

## Q6. 如何在 Jetson 上调度投机解码？

**机会点：** Jetson 上 batch=1 的 decode（逐 token 生成阶段）是权重读取受限 —— GPU 在流式读取权重时空等 LPDDR5 带宽。在一次 target 前向中验证 K 个 draft token 所消耗的 HBM 与解码 1 个 token 大致相当，因为两种情况下权重读取瓶颈都一样。如果平均接受 K 个 token，就能以约 1× 带宽代价换来 K× 吞吐。

**draft 模型选择：** 选一个与 target 共享 tokenizer 的极小 draft。1–2 层的 EAGLE feature-draft 或 Medusa heads 优于单独的 1B 模型 —— 完整的第二模型会争抢同一个 LPDDR5 带宽池，侵蚀收益。共享的 LPDDR5（CPU + draft + target）是 Jetson 上的关键约束，这在数据中心（NVLink 隔离的 HBM）并不存在。

**经济性核算：** `net_win = accept_len × target_cost - (draft_cost + verify_cost)`。只有当 `accept_len > 1 + draft_cost / target_cost` 时才有净收益。先在实际任务分布上测量接受率；再按实测值调整树的深度与宽度。对代码/技术文本，接受数通常为 3–4；对推理/数学则更低。

**内层循环：**

```text
while generating:
    draft K tokens (chain or tree) using tiny draft head
    one target forward with tree mask (verify all K in parallel)
    accept longest matching prefix
    repeat
```

**CUDA graphs：** 把 target 的 decode 步骤捕获为 CUDA graph，以消除 kernel 启动开销（Orin 上每步约 100–200 µs）。问题在于：每步的接受长度都在变化，因此该 graph 存在变长的 KV 更新。解决办法是用填充到固定长度的树 + mask-out（被接受的路径控制 KV 写入），或者按接受数量分别维护捕获好的 graph。我在 Gemma 4 的逐 token PLE（Per-Layer Embeddings）上关闭 graph 捕获，因为 PLE 依赖位置，而动态位置会破坏静态 graph 捕获。

**功耗/热管理：** 投机解码会提高 GPU 利用率 —— draft + verify 两步都要用 GPU，而纯 decode 在两次 HBM 取数之间让 GPU 处于访存空闲状态。在 Jetson 15–25W 的功耗预算下，这可能引发热降频。始终报告持续锁定频率（`jetson_clocks --store; jetson_clocks`）下的 tok/s，而非峰值突发值。

**实际数字：** 在良好接受率下，2–4B 的带宽受限模型可获约 1.5–2.5× 的 decode 吞吐。Orin 上 Gemma 4 4B + E2B drafter：实测约 90 tok/s，基线约 55 tok/s。

---


<details>
<summary>English original</summary>

**Q6. How would you schedule speculative decoding on Jetson?**

**The opportunity:** Jetson decode at batch=1 is weight-read-bound — the GPU idles on LPDDR5 bandwidth while streaming weights. Verifying K draft tokens in one target forward costs roughly the same HBM as decoding 1 token, because the weight-read bottleneck is the same either way. If K tokens are accepted on average, you get K× throughput for ~1× bandwidth cost.

**Draft model selection:** a tiny draft that shares the target's tokenizer. A 1–2-layer EAGLE feature-draft or Medusa heads are better than a separate 1B model — a full second model competes for the same LPDDR5 bandwidth pool, eroding the gain. The shared LPDDR5 (CPU + draft + target) is the key constraint on Jetson that doesn't exist in datacenter (NVLink-isolated HBM).

**Economics check:** `net_win = accept_len × target_cost - (draft_cost + verify_cost)`. Net win only if `accept_len > 1 + draft_cost / target_cost`. Measure acceptance rate first on your actual task distribution; tune tree depth and width to the measured value. For code/technical text, acceptance is typically 3–4; for reasoning/math it's lower.

**The inner loop:**

```text
while generating:
    draft K tokens (chain or tree) using tiny draft head
    one target forward with tree mask (verify all K in parallel)
    accept longest matching prefix
    repeat
```

**CUDA graphs:** capture the target decode step as a CUDA graph to eliminate kernel-launch overhead (~100–200 µs on Orin per step). The problem: accepted length varies each step, so the graph has variable-length KV updates. Work around it with a padded fixed-length tree + mask-out (accepted path gates the KV write), or maintain separate captured graphs per accept-count. I guard graph capture off for Gemma 4's per-token PLE (Per-Layer Embeddings) because PLE is position-dependent and the dynamic position breaks static graph capture.

**Power/thermal:** speculative decode increases GPU utilization — the draft + verify steps both use the GPU, versus decode-only which leaves the GPU memory-idle between HBM fetches. On Jetson's 15–25W budget this can cause thermal throttling. Always report tok/s at sustained locked clock (`jetson_clocks --store; jetson_clocks`), not peak burst.

**Realistic numbers:** ~1.5–2.5× decode throughput on a 2–4B memory-bound model at good acceptance rates. For Gemma 4 4B + E2B drafter on Orin: measured ~90 tok/s vs ~55 tok/s baseline.

---

</details>

## Q7. 如何降低 Orin Nano 上的 TTFT？

**TTFT = 模型加载时间 + prompt prefill 时间。** 两者都是可优化的杠杆；对非简单 prompt，prefill 占主导。

**Prefill 吞吐（主要杠杆）：**

我发现并修复的关键故障：对超过 1024 token 的 prompt 存在 **逐 token 回退**。系统在长 prompt 上循环调用 `decode_one_token()` 而不是 `prefill_batch()` —— 那是 O(N) 次串行前向传播，而非一次批处理的前向传播。修复方式是把任意 prompt 切成 `scratch_budget` 大小的片段，每片作为一个批传入。实测结果：在 1261 token 的 Gemma 4 prompt 上 152 → 620 tok/s。

其他 prefill 收益：
- **Tensor Core GEMM：** 量化投影（Q、K、V、out、gate、up、down）用 INT8 MMQ，attention 用 fp16/fp32。这把 GEMM 从标量 CUDA 核心移到张量核心。
- **Tensor Core attention：** 用 cuBLAS 批处理 GEMM（fp16 输入，fp32 累加）替换标量 `QKᵀ` 循环。prefill 时 batch=1 但 N>512，这一项占主导。
- **批处理 RoPE+KV 存储：** 在一次 kernel 调用中对所有 prompt token 施加 RoPE，并一趟存完所有 K/V。避免在 Python 循环里逐 token 做 RoPE。
- **跳过被完全 mask 的滑动窗口分块**（见 Q4）。

**前缀缓存（聊天场景单项最大收益）：**

从磁盘 hydrate 系统提示词 / 对话的 KV，而不是每一轮重新 prefill。我的实测：1261 token 上下文下，冷 TTFT 877 ms → 热 TTFT 444 ms。实现方式：prefill 之后把 KV 序列化到内存映射文件；热启动时 mmap + `cudaHostRegister`，直接从 page cache 喂给 GPU（页命中即零拷贝）。

**模型加载时间：**

`mmap` GGUF 文件，`cudaHostRegister` 权重页 → GPU 可直接从映射文件读取（零拷贝 DMA）。冷加载受限于 NVMe；热加载（进程重启但 page cache 是热的）在 Gemma 4 4B INT4 上约 1.3 s。让推理服务进程保持常驻 —— 不要在请求之间重启。

**时钟：**

锁定 MAXN_SUPER + `jetson_clocks`，避免首次推理突发时的 DVFS 升频。不锁时钟的话，首次 prefill 要付出约 200ms 的 DVFS 升频代价，而 llama-bench 的 warm-up 阶段会把它丢掉 —— 使 benchmark 看起来比生产环境更好。

**Prompt 压缩：**

把 persona / 系统 / 工具定义内容烘进 LoRA 权重（适配器），让更少的 token 进入 prefill 路径。500 token 的系统提示词在 Orin 上要花约 50ms；编码进 LoRA 适配器后，prefill token 数为 0。

**务必测量真实的冷路径** —— `llama-bench pp` 会丢掉一个 warmup 步骤，但你的用户付出的是真实的冷启动。用 `nsys profile --trace cuda,osrt` 从冷进程开始 profile。

---


<details>
<summary>English original</summary>

**Q7. How would you reduce TTFT on Orin Nano?**

**TTFT = model load time + prompt prefill time.** Both are levers; prefill dominates for non-trivial prompts.

**Prefill throughput (primary lever):**

The key failure I found and fixed: a **per-token fallback** for prompts over 1024 tokens. The system was calling `decode_one_token()` in a loop instead of `prefill_batch()` for long prompts — that's O(N) sequential forward passes instead of one batched pass. Fixed by chunking any prompt into `scratch_budget`-sized pieces and passing each as a batch. Measured result: 152 → 620 tok/s on a 1261-token Gemma 4 prompt.

Other prefill wins:
- **Tensor-core GEMMs:** INT8 MMQ for quantized projections (Q, K, V, out, gate, up, down), fp16/fp32 for attention. This moves the GEMM from scalar CUDA cores to tensor cores.
- **Tensor-core attention:** replace the scalar `QKᵀ` loop with cuBLAS batched GEMM (fp16-in, fp32-accumulate). At prefill batch=1 but N>512, this dominates.
- **Batched RoPE+KV-store:** apply RoPE to all prompt tokens in one kernel call and store all K/V in one pass. Avoid per-token RoPE in a Python loop.
- **Skip fully-masked sliding-window tiles** (see Q4).

**Prefix caching (biggest single win for chat):**

Hydrate the system-prompt / conversation KV from disk instead of re-prefilling on every turn. My measurement: 877 ms cold TTFT → 444 ms warm TTFT on a 1261-token context. Implementation: after prefill, serialize KV to a memory-mapped file; on warm start, mmap + `cudaHostRegister` to feed GPU directly from the page cache (zero-copy if pages are hot).

**Model load time:**

`mmap` the GGUF file and `cudaHostRegister` the weight pages → GPU can read directly from the mapped file (zero-copy DMA). Cold load is NVMe-bound; warm load (process restarts with hot page cache) is ~1.3 s for Gemma 4 4B INT4. Keep the serving process warm — don't restart between requests.

**Clocks:**

Lock MAXN_SUPER + `jetson_clocks` to avoid DVFS ramp-up on the first inference burst. Without clock locking, the first prefill pays a ~200ms DVFS ramp that llama-bench's warm-up pass discards — making benchmarks look better than production.

**Prompt compression:**

Bake persona/system/tool-definition content into LoRA weights (adapter) so fewer tokens hit the prefill path. A 500-token system prompt costs ~50ms on Orin; encoded in a LoRA adapter it costs 0 prefill tokens.

**Always measure the actual cold path** — `llama-bench pp` discards a warmup step, but your users pay a real cold start. Profile with `nsys profile --trace cuda,osrt` from a cold process.

---

</details>

## Q8. 如何把一个新的模型架构集成进 TensorRT-LLM？

**背景：** 这正是我完成的 Gemma 4 → TensorRT-Edge-LLM 移植，包括其中那些有差异的特性：逐 layer 类型的 head dim、dual RoPE、QK+V norm、unit attention scaling、Per-Layer Embeddings、KV-sharing、GeGLU、soft-capped logits。

**阶段 1 — 识别 + config 解析。**

把 `config.json` `model_type` 映射为一个解析后的 config 对象。字段检查清单：head dim（可能逐 layer 类型不同）、RoPE 参数（`rope_theta`、`rope_scaling`，是 partial 还是 full）、layer 类型标注（交错的 local/global 用 `attention_type` 列表）、norm 类型（RMSNorm 还是 LayerNorm，以及布局）、KV-sharing（`kv_shared_layers` 映射）、soft-cap（`attn_logit_softcapping`）。

注册 `model_type → model_class`。遇到无法识别的特性要直接报错 —— 一个被悄悄按 Llama 错误构建出来的检查点会照常运行、产出垃圾，而且要花几小时才能 debug 出来。

**阶段 2 — Python 模型定义。**

用框架的 module 组装模型：`QuantizedLinear`、attention plugin、RoPE op、RMSNorm。只在架构确实有分歧的地方写自定义代码。

*Gemma 4 的差异点及其解决方案：*

| 特性 | 解决方案 |
|---------|---------|
| 逐 layer 类型的 head dim（local：256，global：512） | 在 attention plugin 调用中参数化 `head_dim`；传入 `layer_type` 索引 |
| Dual RoPE（两张独立的 cos/sin 表） | 两个模型输入；在 runtime 初始化时预计算两张表 |
| QK-norm + V-norm | 在 attention 点积之前对 Q 和 K 插入 RMSNorm；对投影之后的 V 做 RMSNorm |
| Unit attention scaling | 在 RoPE 之前把 Q 预缩放 `√head_dim`，以抵消 plugin 内置的 `1/√head_dim`；验证 RoPE 与该标量可交换（确实可交换 —— RoPE 是旋转，缩放是标量乘法） |
| Per-Layer Embeddings | 第二条 embedding 通路：一个额外的整数输入（PLE token）+ 图内 embedding 查表 + 加到残差 stream 上 |
| KV-sharing（尾部 layer 复用此前的 KV） | 传入源 layer 的物理 KV buffer 地址，而不是分配新的 KV；在 pool 中通过 `kv_shared_layers` 别名来实现 |
| GeGLU | gate proj + up proj 的双倍宽线性层，然后在单个融合 kernel 中做 `gate ⊙ gelu(up)` |
| Soft-cap | 在 lm_head 之后应用 `logits = tanh(logits / cap) * cap` |

**阶段 3 — 导出契约。**

对 ONNX→TRT 这一类栈：定义前向签名 —— 输入、dtype、动态轴、名称。新的架构特性会变成 runtime 必须提供的新图输入（我加了第二个 RoPE 表输入、一个 PLE 整数输入，以及图中作为常量存在的 `layer_type`）。用 dynamo exporter 配合自定义 TRT op 转换表导出；在写下第一行 runtime C++ 之前先做结构校验（实例化 + 导出 → 用 `onnx.checker.check_model` 检查 ONNX 图）。

**阶段 4 — Plugin + runtime。**

把新 op 映射到 TRT plugin：dual-RoPE 预计算（扩展 RoPE fuser）、PLE 查表（新的 embedding plugin）、KV-share 别名（扩展 cache 管理器）。每个新 plugin 都是一个风险点 —— 要对着参考实现（HuggingFace 或 numpy）单独对每个 plugin 做单元测试。

**阶段 5 — 权重加载。**

把 HuggingFace 检查点的 key 名映射到 TRT-LLM 的参数名。注意：tied embeddings（lm_head 共享 `embed_tokens.weight`）、逐 layer 类型的权重形状（local head != global head）、KV-sharing layer 上被丢弃的权重（这些 layer 在检查点里没有 KV 投影）。

**阶段 6 — 验证。**

贪心一致（greedy-identical）校验：在 5 条事实性 prompt 上用 `temperature=0` 与 HuggingFace 逐 token 比对。逐 layer 张量比对以定位回归（`model.forward(return_all_hidden_states=True)` 比对是最快的 debug 工具）。在最大支持长度下做 long-context 测试。然后看性能：prefill（首字前的整段计算）吞吐、decode（逐 token 生成阶段）tok/s、KV 内存统计。

**降风险策略：** 我先在一个更简单的 GGUF runtime 里做到与 `llama.cpp` 贪心一致。这样就在不依赖 TRT 机制的前提下验证了数学部分。之后 TRT 移植的风险就是机械性的（图构建、plugin 接线）而非数学性的 —— debug 起来容易得多。

**每个 PR 只处理一件事：** 识别 → 建模+导出 → runtime → 性能。不要把权重加载的 bug 和 attention plugin 的 bug 混在一起 —— 那样无法把它们区分开。

---


<details>
<summary>English original</summary>

**Q8. How would you integrate a new model architecture into TensorRT-LLM?**

**Context:** this is exactly the Gemma 4 → TensorRT-Edge-LLM port I completed, including divergent features: per-layer-type head dims, dual RoPE, QK+V norm, unit attention scaling, Per-Layer Embeddings, KV-sharing, GeGLU, soft-capped logits.

**Phase 1 — Recognition + config parsing.**

Map `config.json` `model_type` → a parsed config object. Field checklist: head dims (may differ per layer type), RoPE params (`rope_theta`, `rope_scaling`, whether partial or full), layer type annotations (`attention_type` list for interleaved local/global), norm types (RMSNorm vs LayerNorm, placement), KV-sharing (`kv_shared_layers` mapping), soft-cap (`attn_logit_softcapping`). Register `model_type → model_class`. Fail loud on unrecognized features — a checkpoint that silently mis-builds as Llama will run, produce garbage, and take hours to debug.

**Phase 2 — Python model definition.**

Compose the model from the framework's modules: `QuantizedLinear`, the attention plugin, RoPE op, RMSNorm. Only write custom code where the arch genuinely diverges.

*Gemma 4 divergences and their solutions:*

| Feature | Solution |
|---------|---------|
| Per-layer-type head dims (local: 256, global: 512) | Parameterize `head_dim` in the attention plugin call; pass `layer_type` index |
| Dual RoPE (two separate cos/sin tables) | Two model inputs; precompute both tables at runtime init |
| QK-norm + V-norm | Insert RMSNorm on Q and K before attention dot-product; RMSNorm on V after projection |
| Unit attention scaling | Pre-scale Q by `√head_dim` before RoPE to cancel the plugin's built-in `1/√head_dim`; verify RoPE commutes with this scalar (it does — RoPE is a rotation, scaling is a scalar multiply) |
| Per-Layer Embeddings | Second embedding pathway: an extra integer input (PLE token) + in-graph embedding lookup + add to the residual stream |
| KV-sharing (trailing layers reuse prior KV) | Pass the physical KV buffer address of the source layer instead of allocating new KV; implement via `kv_shared_layers` aliasing in the pool |
| GeGLU | Gate proj + up proj double-wide linear, then `gate ⊙ gelu(up)` in a single fused kernel |
| Soft-cap | `logits = tanh(logits / cap) * cap` applied after the lm_head |

**Phase 3 — Export contract.**

For ONNX→TRT stacks: define the forward signature — inputs, dtypes, dynamic axes, names. New arch features become new graph inputs that the runtime must supply (I added a second RoPE table input, a PLE integer input, and `layer_type` as a constant in the graph). Export via the dynamo exporter with the custom TRT op translation table; structurally validate (instantiate + export → check the ONNX graph with `onnx.checker.check_model`) before writing a single line of runtime C++.

**Phase 4 — Plugins + runtime.**

Map new ops to TRT plugins: dual-RoPE precompute (extend the RoPE fuser), PLE lookup (new embedding plugin), KV-share aliasing (extend the cache manager). Each new plugin is a risk — unit-test each one in isolation against a reference (HuggingFace or numpy).

**Phase 5 — Weight loading.**

Map HuggingFace checkpoint key names → TRT-LLM parameter names. Watch for: tied embeddings (lm_head shares `embed_tokens.weight`), per-layer-type weight shapes (local head != global head), dropped weights on KV-sharing layers (those layers have no KV projections in the checkpoint).

**Phase 6 — Validate.**

Greedy-identical check: compare token-by-token against HuggingFace on 5 factual prompts with `temperature=0`. Per-layer tensor comparison to isolate regressions (a `model.forward(return_all_hidden_states=True)` comparison is the fastest debug tool). Long-context test at max supported length. Then perf: prefill throughput, decode tok/s, KV memory accounting.

**De-risk strategy:** I got greedy-identical to `llama.cpp` in a simpler GGUF runtime first. That validated the math independently of TRT mechanics. The TRT port's risk then was mechanical (graph construction, plugin wiring) not mathematical — much easier to debug.

**One concern per PR:** recognition → modeling+export → runtime → perf. Don't mix weight-loading bugs with attention-plugin bugs — you won't be able to isolate them.

---

</details>

## 快问快答校准题

这些题考察你在 30 秒内给出精确答案的能力 —— 相当于面试中检查单位。

**问：decode（逐 token 生成阶段）阶段的 GEMM 算术强度是多少，它意味着什么？**

答：在 batch=1（M=1）时 AI = `2·M·K·N / (M·K + M·N + K·N) bytes`：`2·K·N / (K + N + K·N) ≈ 2 / (1/K + 1/N)`。当 K=N=4096 时，AI ≈ 2 FLOP/byte。H100 的 ridge point 约为 300 FLOP/byte —— decode GEMM 比 ridge 低约 150×。它完全受内存带宽限制。含义：更快的计算单元没有帮助；更大的 HBM 带宽或权重量化（需要 stream 的字节更少）才有帮助。

**问：张量并行与流水线并行的区别是什么？**

答：TP 把每个 layer 的权重矩阵切分到各设备上（MLP 用列并行 + 行并行；attention 把 Q/K/V head 分到各设备）—— 每个 layer 都在所有设备上运行，并在每个 layer 之后用 `AllReduce` 通信。对延迟友好，带宽开销大。PP 按模型深度切分 —— 不同 layer 位于不同设备上，通过点对点 `send`/`recv` 连接。通信量更低，但会引入流水线气泡（设备在等待本 stage 输入时空闲）。对于大批量推理服务，PP 降低每步的 AllReduce 压力；对于延迟关键的单个请求路径，TP 通常更优，因为它没有流水线空闲。

**问：为什么 GQA 的 KV 内存比 MHA 小？**

答：MHA 为每个 query head 存一个 K 和 V head。GQA 把 G 个 query head 分为一组共享一个 K/V head。内存：MHA → `num_heads × 2 × seq_len × head_dim`；GQA → `num_kv_heads × 2 × seq_len × head_dim`，其中 `num_kv_heads = num_heads / G`。对于 Gemma 4 4B（G=2）：KV 内存是 MHA 的一半。吞吐：每步的 KV HBM 读取量减半 → 仅凭带宽就能把 decode 吞吐上限提高 2×。

**问：GGUF 中 Q8_0 与 Q4_K_M 的区别是什么？**

答：Q8_0 是 8-bit 整数分块量化，每 32 元素块配一个 FP32 scale。每个权重有效约 8.5 bits。质量高，压缩适中。Q4_K_M 是一种 k-quant：4-bit（M = mixed，部分关键 layer 使用 6-bit），按每 256 元素块分组 scale 和 min，scale 本身用 6-bit 子量化。每个权重有效约 4.5 bits。由于 scale 子量化，大多数模型上的质量损失低得出人意料。Q4_K_M 是边缘部署的标准推荐 —— 它能把 Gemma 4 4B 装进 2.2 GB，而 BF16 需要 8.2 GB。

---

*上一级：[MLSys Engineer](README.md) | 返回：[Interview Preparation](../README.md)*


<details>
<summary>English original</summary>

**Quick-fire calibration questions**

These test your ability to give a precise 30-second answer — the interview equivalent of checking your units.

**Q: What is the arithmetic intensity of a decode-phase GEMM, and what does it imply?**

A: AI = `2·M·K·N / (M·K + M·N + K·N) bytes` at batch=1 (M=1): `2·K·N / (K + N + K·N) ≈ 2 / (1/K + 1/N)`. For K=N=4096, AI ≈ 2 FLOP/byte. The H100's ridge point is ~300 FLOP/byte — decode GEMM is ~150× below the ridge. It's entirely memory-bandwidth-bound. Implication: faster math units do not help; more HBM bandwidth or weight quantization (fewer bytes to stream) does.

**Q: What's the difference between tensor parallelism and pipeline parallelism?**

A: TP splits each layer's weight matrices across devices (column-parallel + row-parallel for MLP; Q/K/V heads across devices for attention) — every layer runs on all devices, communicating with `AllReduce` after each layer. Latency-friendly, bandwidth-expensive. PP splits the model depth — different layers live on different devices, connected by point-to-point `send`/`recv`. Lower communication volume but adds pipeline bubbles (devices idle while waiting for their stage's input). For large-batch serving, PP reduces the AllReduce pressure per step; for latency-critical single-request paths, TP typically wins because it has no pipeline idle.

**Q: Why is GQA KV memory smaller than MHA?**

A: MHA stores one K and V head per query head. GQA groups G query heads to share one K/V head. Memory: MHA → `num_heads × 2 × seq_len × head_dim`; GQA → `num_kv_heads × 2 × seq_len × head_dim` where `num_kv_heads = num_heads / G`. For Gemma 4 4B (G=2): half the KV memory vs MHA. Throughput: the KV HBM read per step is halved → 2× decode throughput ceiling from bandwidth alone.

**Q: What is the difference between Q8_0 and Q4_K_M in GGUF?**

A: Q8_0 is 8-bit integer per-block quantization with one FP32 scale per 32-element block. ~8.5 bits per weight effective. High quality, moderate compression. Q4_K_M is a k-quant: 4-bit (M = mixed, some critical layers use 6-bit), grouped scales and mins per 256-element block using a 6-bit sub-quantization for the scales. ~4.5 bits per weight effective. Quality loss is surprisingly low on most models because of the scale sub-quantization. Q4_K_M is the standard recommendation for edge deployment — it fits Gemma 4 4B in 2.2 GB vs 8.2 GB BF16.

---

*Up: [MLSys Engineer](README.md) | Back to: [Interview Preparation](../README.md)*

</details>

---

> 原文：[`Phase 6 - Interview Preparation/1. MLSys Engineer/01-Inference-Systems-QA.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%206%20-%20Interview%20Preparation/1.%20MLSys%20Engineer/01-Inference-Systems-QA.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
