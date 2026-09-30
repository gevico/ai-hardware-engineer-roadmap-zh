---
title: Part 2 · 第 07 讲 —— 通信层内部：NCCL、自定义 All-Reduce 与 vLLM 通信器栈
description: Part 2 · 第 07 讲 —— 通信层内部：NCCL、自定义 All-Reduce 与 vLLM 通信器栈
---

# Part 2 · 第 07 讲 —— 通信层内部：NCCL、自定义 All-Reduce 与 vLLM 通信器栈

## 概述

第 04 讲已经确立：张量并行每 layer 运行**两次 all-reduce**，而在 `TP=8` 上，这些集合通信可吃掉约 25% 的 step 时间。本讲撬开这一事实之下的黑箱：**生产级 runtime 究竟如何在 GPU 之间搬运字节**、为何它不会只调一下 NCCL 就收手，以及哪条路径在哪个区间胜出。

我们用 **vLLM 的 `distributed/device_communicators/` layer** 作为贯穿全文的示例，因为它是这一思想被阅读最多的开源实现。确切的文件名与 API 会随版本漂移 —— 把它们当作*结构*的示意图，而非钉死的规范 —— 但该架构在各 runtime 之间是稳定的（SGLang 和 TRT-LLM 以同样的方式对各自的集合通信分层）。

本讲涵盖：

1. 三层通信器架构。
2. 后端原语 —— NCCL、CUDA 胶水层与共享内存。
3. 为何 all-reduce 有*多种*实现，以及小消息问题。
4. 优化路径 —— 自定义 one-shot/two-shot all-reduce、融合的 all-reduce+norm，以及回退阶梯。
5. 路由大脑 —— runtime 如何在运行时*选择*一条路径。
6. all2all —— MoE（混合专家模型）集合通信（前向指向 Part 3）。
7. 心智模型：runtime 是一台**动态通信优化器**。
8. 推理工程要点 —— 诊断与调优集合通信路径。

学完本讲，你应能看懂一份多 GPU decode（逐 token 生成阶段）trace，识别出正在运行的是哪条集合通信路径，并说明它对该消息大小与拓扑是否恰当。

---

## 1. 三层架构

推理服务 runtime 不会把 “NCCL” 暴露给模型。它暴露的是一个**抽象** —— `tensor_model_parallel_all_reduce(x)` —— 并让该调用穿过三层：

<div class="lecture-map" markdown>

| 层 | 角色 | 示例模块 |
|-------|------|-----------------|
| **编排 / 路由** | 为*当前*张量、拓扑与预算挑选最佳路径；不可用时回退 | `cuda_communicator.py`（dispatcher） |
| **通信引擎** | 集合通信*如何*发生 | `custom_all_reduce.py`、`flashinfer_all_reduce.py`、`quick_all_reduce.py`、`all2all.py` |
| **后端原语** | 直接与硬件 / 库打交道 | `pynccl.py`（NCCL）、`cuda_wrapper.py`（CUDA runtime/streams）、`shm_*`（host shared memory） |

</div>

核心思想：**引擎是按调用选择的，不是在启动时一次性选定。** 同一批 GPU 上的 prefill（首字前的整段计算）all-reduce 与 decode all-reduce 可能走不同路径，因为它们的消息大小相差 100×。

---

## 2. 后端原语

这些是最底层 —— 薄薄的绑定，不含任何策略。

* **`pynccl` —— NCCL 绑定。** 直接访问 `ncclAllReduce`、`ncclAllGather`、`ncclReduceScatter`，外加分组 `ncclSend`/`ncclRecv`（NCCL 没有原生 all-to-all 集合通信；它由 send/recv 对组合而成）。NCCL 是稳定、到处都能跑的基线（第 04 讲 §2）。vLLM 在热路径上直接封装它，而不经过 `torch.distributed`，从而自行控制流并避开框架开销。
* **`cuda_wrapper` —— CUDA 胶水层。** 流的创建、`cudaMemcpyAsync`、事件/句柄管理、设备上下文。这就是“Python 安全地驱动 CUDA 流”；集合通信被发射到这些流上，使通信能与计算重叠。
* **`shm_*` —— 主机共享内存。** *worker 进程*之间的 CPU↔CPU 传输（引擎/驱动与其 TP worker，或 Ray actor）。这**不是** GPU 热路径 —— 它在同一节点上的进程之间搬运控制消息、小份元数据以及 CPU-offload 张量。对启动和调度重要，与每 token 延迟无关。

> 这一分层很重要：当你在 profile 中看到一次集合通信，它几乎总是某种 GPU 原语（NCCL 或自定义 kernel）。`shm` 的流量在主机侧，出现在 CPU 时间线上，而非 GPU 时间线上。

---

## 3. 为何 all-reduce 有多种实现 —— 小消息问题

NCCL 的 **ring all-reduce** 是*带宽最优*的：对一条 `N` 字节、跨 `P` 块 GPU 的消息，它每 GPU 搬运 `~2N(P−1)/P` 字节，并吃满 NVLink。对**大**消息来说它是正确选择 —— 即 **prefill**，此时 all-reduce 承载 `[many tokens × hidden]`。

但 **decode 不同。** 在批大小为 1、单个 token 时，每 layer 的 all-reduce 消息极小 —— `[1 × 8192] × 2 B ≈ 16 KB`。对这么小的消息：

* Ring all-reduce 是**受延迟约束，而非受带宽约束。** 为了搬运 16 KB，你要付出 `2(P−1)` 次顺序跳转的 kernel 启动 + 握手延迟。链路几乎空闲；你付的是*往返*的钱，不是*字节*的钱。
* 在 `TP=8` 下，即每次 all-reduce 14 跳 × 每 layer 2 次 × 80 layer = **每 token 2,240 次受延迟约束的跳** —— 正是第 04 讲中“TP=8 decode 时受通信约束”的结论，现在有了机理层面的解释。

这正是 runtime 不止于 NCCL 的全部原因：**小消息延迟。**

---


<details>
<summary>English original</summary>

**Part 2 · Lecture 07 — Inside the Communication Layer: NCCL, Custom All-Reduce, and the vLLM Communicator Stack**

**Overview**

Lecture 04 established that tensor parallelism runs **two all-reduces per layer** and that on `TP=8` those collectives can eat ~25% of step time. This lecture opens the box underneath that fact: **how a production runtime actually moves bytes between GPUs**, why it does not just call NCCL and stop, and which path wins in which regime.

We use **vLLM's `distributed/device_communicators/` layer** as the worked example because it is the most-read open-source implementation of this idea. The exact filenames and APIs drift between releases — treat them as a map of the *structure*, not a pinned spec — but the architecture is stable across runtimes (SGLang and TRT-LLM layer their collectives the same way).

This lecture covers:

1. The three-layer communicator architecture.
2. The backend primitives — NCCL, the CUDA glue, and shared memory.
3. Why all-reduce has *many* implementations, and the small-message problem.
4. The optimized paths — custom one-shot/two-shot all-reduce, fused all-reduce+norm, and the fallback ladder.
5. The routing brain — how a runtime *chooses* a path at runtime.
6. all2all — the MoE collective (a forward pointer to Part 3).
7. The mental model: a runtime is a **dynamic communication optimizer**.
8. Inference-engineering takeaways — diagnosing and tuning the collective path.

By the end you should be able to look at a multi-GPU decode trace, identify which collective path is running, and explain whether it is the right one for that message size and topology.

---

**1. The three-layer architecture**

A serving runtime does not expose "NCCL" to the model. It exposes an **abstraction** — `tensor_model_parallel_all_reduce(x)` — and routes that call through three layers:

<div class="lecture-map" markdown>

| Layer | Role | Example modules |
|-------|------|-----------------|
| **Orchestration / router** | Pick the best path for *this* tensor, topology, and budget; fall back if unavailable | `cuda_communicator.py` (the dispatcher) |
| **Communication engines** | *How* the collective happens | `custom_all_reduce.py`, `flashinfer_all_reduce.py`, `quick_all_reduce.py`, `all2all.py` |
| **Backend primitives** | Talk to hardware / libraries directly | `pynccl.py` (NCCL), `cuda_wrapper.py` (CUDA runtime/streams), `shm_*` (host shared memory) |

</div>

The key idea: **the engine is chosen per call, not once at startup.** A prefill all-reduce and a decode all-reduce on the same GPUs may take different paths because their message sizes differ by 100×.

---

**2. The backend primitives**

These are the bottom layer — thin bindings, no policy.

* **`pynccl` — NCCL bindings.** Direct access to `ncclAllReduce`, `ncclAllGather`, `ncclReduceScatter`, plus grouped `ncclSend`/`ncclRecv` (NCCL has no native all-to-all collective; it is composed from send/recv pairs). NCCL is the stable, works-everywhere baseline (Lecture 04 §2). vLLM wraps it directly rather than going through `torch.distributed` for the hot path, so it controls streams and avoids framework overhead.
* **`cuda_wrapper` — the CUDA glue.** Stream creation, `cudaMemcpyAsync`, event/handle management, device context. This is "Python safely driving CUDA streams"; the collectives launch onto these streams so communication can overlap compute.
* **`shm_*` — host shared memory.** CPU↔CPU transfer between *worker processes* (the engine/driver and its TP workers, or Ray actors). This is **not** the GPU hot path — it carries control messages, small metadata, and CPU-offload tensors between processes on one node. Important for startup and scheduling, irrelevant to per-token latency.

> The split matters: when you see a collective in a profile, it is almost always a GPU primitive (NCCL or a custom kernel). `shm` traffic is host-side and shows up on the CPU timeline, not the GPU one.

---

**3. Why all-reduce has many implementations — the small-message problem**

NCCL's **ring all-reduce** is *bandwidth-optimal*: for a message of `N` bytes across `P` GPUs it moves `~2N(P−1)/P` bytes per GPU and saturates NVLink. It is the right choice for **large** messages — i.e., **prefill**, where the all-reduce carries `[many tokens × hidden]`.

But **decode is different.** At batch=1, one token, the per-layer all-reduce message is tiny — `[1 × 8192] × 2 B ≈ 16 KB`. For a message that small:

* Ring all-reduce is **latency-bound, not bandwidth-bound.** You pay `2(P−1)` sequential hops of kernel-launch + handshake latency to move 16 KB. The links are nearly idle; you are paying for *round-trips*, not *bytes*.
* At `TP=8` that is 14 hops per all-reduce × 2 per layer × 80 layers = **2,240 latency-bound hops per token** — exactly the "comm-bound at TP=8 decode" result from Lecture 04, now explained mechanistically.

This is the entire reason a runtime ships more than NCCL: **small-message latency.**

---

</details>

## 4. 优化路径与回退阶梯

### 4.1 自定义 all-reduce（小消息优势）

最主要的优化。对于具有 **完整 peer-to-peer / NVLink** 连接的 GPU 上的小消息，自定义 CUDA kernel 执行 **one-shot**（或 two-shot）all-reduce：每个 GPU 通过 NVLink 直接读取其对等方的缓冲区，并在 **单次 kernel 启动** 中进行归约，而非 NCCL 的多跳 ring。

* **One-shot：** 每个 GPU 读取所有对等方 → 进行归约 → 写入其结果。最适合最小的消息（decode，逐 token 生成阶段）。
* **Two-shot（reduce-scatter + all-gather）：** 随消息增大效果更好，但仍低于 ring 的交叉点。
* **需要 P2P/NVLink。** 在仅 PCIe 的机器上（无完整 peer access），它无法运行，runtime 回退到 NCCL。**拓扑决定路径。**

这是对 **多 GPU decode 延迟** 最重要的单项集合通信优化。在 vLLM 中，支持时默认开启（`VLLM_USE_CUSTOM_ALL_REDUCE`），当 decode 延迟看起来受通信限制时，禁用它便是首个 A/B 测试。

### 4.2 融合 all-reduce + norm（FlashInfer）

下一步通过将 all-reduce 与周围的逐点操作（残差相加 + RMSNorm）**融合**，消除 *kernel 启动和 HBM 往返*。而非：

```text
all_reduce(x) → write HBM → read HBM → residual+RMSNorm → write HBM
```

融合 kernel 在一次 pass 中完成 **reduce → residual → norm → write-back**。 [FlashInfer](https://arxiv.org/abs/2501.01005)（来自 Lecture 05 §2.4 的 kernel 引擎）提供这条 `allreduce_fusion` 路径。它受以下条件门控：与自定义 all-reduce 相同的条件，外加 **workspace 预算**（见 §5）和固定张量布局（连续的 `[tokens, hidden]`）。当它不适用时，runtime 回退。

### 4.3 阶梯

<div class="lecture-map" markdown>

| 优先级 | 路径 | 适用条件 |
|----------|------|-----------|
| 1 | **融合 all-reduce+norm**（FlashInfer） | 小/中消息，连续，适配 workspace，有 P2P |
| 2 | **自定义 one-shot/two-shot all-reduce** | 小消息（decode），完整 NVLink/P2P |
| 3 | **NCCL ring**（`pynccl`） | 大消息（prefill，首字前的整段计算），或无 P2P，或不支持的 shape/dtype |
| 4 | **共享内存 / 主机回退** | 跨进程控制 + CPU 张量（非 GPU 热路径） |

</div>

对于繁重的 workspace 设置不划算的小规模归约，还存在一种 "quick" / 轻量级 reduce 路径；可将其视为自定义 kernel 与 NCCL 之间的轻量捷径。

---

## 5. 路由大脑 — 在 runtime 选择路径

每个优化引擎都暴露一个谓词 — 概念上为 `should_use_this_path(tensor)` — **每次调用** 检查。检查始终是以下项的子集：

* **是否为 CUDA 张量、连续，且具有预期的 rank/shape**（`[tokens, hidden]`）？
* **消息是否在 workspace 预算内？** 融合 kernel 预分配一个按最大 token 数确定大小的暂存缓冲区：`max_tokens ≈ workspace_bytes / (hidden × dtype_bytes)`。比该值更大的 batch 无法在单次启动中融合 → 回退。
* **后端是否已安装，且 P2P/NVLink 是否可用？** 如果 FlashInfer 缺失或 peer access 关闭，该路径在初始化时禁用自身。
* **world_size > 1**（单 GPU 上无需集合通信）。

如果 **任一** 检查失败，路由器降至阶梯的下一级。这就是为什么同一模型在 prefill 与 decode 中，或在 NVLink 机器与 PCIe 机器上，可能表现出不同的集合通信 kernel — *路由是动态且拓扑感知的。*

---

## 6. all2all — MoE（混合专家模型）集合通信（前向指向 Part 3）

上述所有内容都是 **张量并行** 集合通信集合（all-reduce / all-gather / reduce-scatter）。混合专家模型增加了一种不同的集合通信：**all2all**，用于 **专家分发与合并**。

```text
tokens (after routing)
  GPU0 → experts {1,3}     GPU1 → experts {2,5}     GPU2 → experts {0,4}
        └──────────────── all2all exchange ────────────────┘
  each GPU now holds the tokens routed to ITS experts
```

每个 token 被发送到持有其选定专家的 GPU（dispatch），计算，然后发送回（combine）。all2all 是 **专家并行的主要通信开销**，其行为与 all-reduce 截然不同（不规则、依赖 payload 的量）。Part 3 — Blackwell 上的 MoE — 深入讨论；这里只需知道它位于 **同一通信层**（`all2all.py`），并遵循相同的回退规则。

---


<details>
<summary>English original</summary>

**4. The optimized paths and the fallback ladder**

**4.1 Custom all-reduce (the small-message win)**

The headline optimization. For small messages on GPUs with **full peer-to-peer / NVLink** connectivity, a custom CUDA kernel does a **one-shot** (or two-shot) all-reduce: each GPU reads its peers' buffers directly over NVLink and reduces in a **single kernel launch**, instead of NCCL's multi-hop ring.

* **One-shot:** every GPU reads all peers → reduces → writes its result. Best for the smallest messages (decode).
* **Two-shot (reduce-scatter + all-gather):** better as the message grows but still below the ring's crossover.
* **Requires P2P/NVLink.** On PCIe-only boxes (no full peer access), it cannot run and the runtime falls back to NCCL. **Topology decides the path.**

This is the single most important collective optimization for **multi-GPU decode latency**. In vLLM it is on by default when supported (`VLLM_USE_CUSTOM_ALL_REDUCE`), and disabling it is the first A/B test when decode latency looks comm-bound.

**4.2 Fused all-reduce + norm (FlashInfer)**

The next step removes *kernel launches and HBM round-trips* by **fusing** the all-reduce with the surrounding pointwise work (residual add + RMSNorm). Instead of:

```text
all_reduce(x) → write HBM → read HBM → residual+RMSNorm → write HBM
```

a fused kernel does **reduce → residual → norm → write-back** in one pass. [FlashInfer](https://arxiv.org/abs/2501.01005) (the kernel engine from Lecture 05 §2.4) provides this `allreduce_fusion` path. It is gated on the same conditions as custom all-reduce plus a **workspace budget** (see §5) and a fixed tensor layout (contiguous `[tokens, hidden]`). When it does not apply, the runtime falls back.

**4.3 The ladder**

<div class="lecture-map" markdown>

| Priority | Path | Wins when |
|----------|------|-----------|
| 1 | **Fused all-reduce+norm** (FlashInfer) | small/medium msg, contiguous, fits workspace, P2P available |
| 2 | **Custom one-shot/two-shot all-reduce** | small msg (decode), full NVLink/P2P |
| 3 | **NCCL ring** (`pynccl`) | large msg (prefill), or no P2P, or unsupported shape/dtype |
| 4 | **Shared-memory / host fallback** | cross-process control + CPU tensors (not the GPU hot path) |

</div>

A "quick" / lightweight reduce path also exists for small reductions where heavy workspace setup is not worth it; think of it as a thin shortcut between the custom kernel and NCCL.

---

**5. The routing brain — choosing a path at runtime**

Every optimized engine exposes a predicate — conceptually `should_use_this_path(tensor)` — checked **per call**. The checks are always some subset of:

* **Is it a CUDA tensor, contiguous, and the expected rank/shape** (`[tokens, hidden]`)?
* **Is the message within the workspace budget?** A fused kernel pre-allocates a scratch buffer sized for a maximum token count: `max_tokens ≈ workspace_bytes / (hidden × dtype_bytes)`. A batch larger than that cannot be fused in one launch → fall back.
* **Is the backend installed and is P2P/NVLink available?** If FlashInfer is absent or peer access is off, the path disables itself at init.
* **world_size > 1** (no collective needed on a single GPU).

If **any** check fails, the router drops to the next rung of the ladder. This is why the same model can show different collective kernels in prefill vs decode, or on an NVLink box vs a PCIe box — *the routing is dynamic and topology-aware.*

---

**6. all2all — the MoE collective (forward pointer to Part 3)**

Everything above is the **tensor-parallel** collective set (all-reduce / all-gather / reduce-scatter). Mixture-of-Experts adds a different one: **all2all**, used for **expert dispatch and combine**.

```text
tokens (after routing)
  GPU0 → experts {1,3}     GPU1 → experts {2,5}     GPU2 → experts {0,4}
        └──────────────── all2all exchange ────────────────┘
  each GPU now holds the tokens routed to ITS experts
```

Each token is shipped to whichever GPU holds its chosen expert (dispatch), computed, then shipped back (combine). all2all is the **dominant communication cost of expert parallelism** and behaves very differently from all-reduce (irregular, payload-dependent volume). Part 3 — MoE at Blackwell — treats it in depth; here, just register that it lives in the **same communicator layer** (`all2all.py`) and goes through the same fallback discipline.

---

</details>

## 7. 心智模型

> 推理服务 runtime **并不是“在使用 NCCL”。** 它是一个 **动态通信优化器**，针对每个集合通信，根据消息大小、dtype、layout 和 GPU 拓扑，选择最廉价且正确的路径——融合 kernel → 自定义 all-reduce → NCCL ring → 主机回退——并在快速路径的前提条件不满足时优雅降级。

把它和第 1 部分的 roofline（性能上界模型）心智模型放在一起看：正如计算有带宽受限与算力受限两种模式，**通信也有延迟受限（小消息）与带宽受限（大消息）两种模式**，runtime 跨越该边界切换集合通信算法，就像它切换 GEMV（矩阵-向量乘）与 GEMM（矩阵-矩阵乘）一样。

---

## 8. 推理工程要点

* **集合通信路径在 `TP ≥ 4` decode（逐 token 生成阶段）时是真正的延迟杠杆。** 小消息 all-reduce 是延迟受限的；自定义一次性 kernel 相比 NCCL ring 能大幅削减每 layer 通信。这是第 04 讲“TP=8 牺牲约 25% 给通信”背后的具体机制。
* **拓扑决定快速路径能否启用。** 自定义/融合 all-reduce 需要完整的 P2P/NVLink。在仅 PCIe 或部分连接的机器上，它会静默回退到 NCCL——因此 *相同* 的 `TP=8` 配置在不同机箱上可能有截然不同的 decode 延迟。在归咎于模型之前，先验证 NVLink/NVSwitch（第 04 讲 §3）。
* **通过 trace 诊断。** 在 Nsight Systems 中，健康的小消息 decode 显示的是 **自定义 all-reduce kernel**（或融合的 allreduce-norm），*而不是* 每 layer 一连串 NCCL ring kernel。NCCL ring kernel 主导 decode 时间线 = 快速路径被禁用（无 P2P、不支持的 dtype/shape，或被人为关闭）。
* **需要了解的标志：** `VLLM_USE_CUSTOM_ALL_REDUCE`（默认开启；禁用以与 NCCL 进行 A/B 对比）、`NCCL_DEBUG=INFO`（确认 NCCL 拓扑/算法），以及第 05 讲的 FlashInfer attention/融合后端选择器。像对待其他所有版本一样，在 bench harness（agent 运行时框架）中固定并记录这些标志。
* **不要在 prefill（首字前的整段计算）上过度投入。** 对于大 prefill 消息，普通的 NCCL ring 已经接近最优——自定义/融合路径在那里收益甚微。收益集中在 **decode**，而你的 $/MTok 正来源于此。

---

## 实验 — 观察集合通信路径切换

扩展第 2 部分的 bench harness：

1. **在 `TP=8` 下以推理服务方式运行 Llama 3.3 70B FP8**，在 NVLink 机器上捕获 **decode** trace（Nsight Systems，约 50 步）。识别每 layer 的 all-reduce kernel——自定义 AR 还是 NCCL ring？
2. **禁用自定义路径**（`VLLM_USE_CUSTOM_ALL_REDUCE=0`），重新 trace，并测量 **TPOT delta**。将其归因于 NCCL ring 的小消息延迟。
3. **对 prefill 重复**（长 prompt），并展示 delta 小得多——此时消息是带宽受限的，NCCL ring 具有竞争力。
4. **（如果可用）** 在仅 PCIe 的机器上运行相同配置，并展示快速路径从未启用。

通过标准：一份一页的报告，用 trace 证据说明 prefill 与 decode 中分别运行了哪条集合通信路径，关闭自定义 AR 的代价是多少，以及为何两个阶段存在差异。

---

## 自检

1. 既然 NCCL 已经实现了 all-reduce，为什么 runtime 还要提供自定义 all-reduce？请从消息大小以及 decode 时实际付出的代价角度回答。
2. 一位同事报告 `TP=8` decode 在新服务器上很慢，但在旧服务器上很快，模型和 vLLM 版本相同。说出你会首先检查什么，以及为什么。
3. 将 all-reduce 与 RMSNorm 融合具体节省了什么（数一数融合前后的 HBM 往返次数）？
4. 为什么 all2all 本质上比 all-reduce 更难优化？（思考每个 rank 的有效载荷大小是否提前已知。）
5. 在 Nsight decode trace 中，你看到每 layer 有一连串 NCCL ring kernel，而不是单个自定义 AR kernel。列出三个原因。

---

## 参考文献

* vLLM 分布式通信器 — [github.com/vllm-project/vllm](https://github.com/vllm-project/vllm)（`vllm/distributed/device_communicators/`）
* NCCL — [docs.nvidia.com/deeplearning/nccl](https://docs.nvidia.com/deeplearning/nccl/user-guide/docs/index.html)（ring/tree 算法，all-reduce/all2all）
* FlashInfer（融合 attention + 集合通信 kernel）— [arXiv:2501.01005](https://arxiv.org/abs/2501.01005)
* 交叉引用：[第 04 讲 — 张量并行](Lecture-04.md)（all-reduce 成本模型）· [第 05 讲 §2.4 — FlashInfer](Lecture-05.md) · [第 3 部分 — Blackwell 上的 MoE（混合专家模型）](../03-Blackwell-MoE/README.md)（all2all / 专家并行）

---

## 截至 2026-06

反映截至 0.22 代系的 vLLM `device_communicators` 架构（自定义 all-reduce、FlashInfer 融合 all-reduce、NCCL 基线、用于 EP 的 all2all）。确切的模块名称和融合路径的可用性在不同版本间会变化——请针对安装的 vLLM 版本重新固定。当新的节点内集合通信原语（例如 NVLink 原生交换机集合通信、NVLS）成为默认路径时，请更新。

---


<details>
<summary>English original</summary>

**7. The mental model**

> A serving runtime is **not "using NCCL."** It is a **dynamic communication optimizer** that picks, per collective, the cheapest correct path for the message size, dtype, layout, and GPU topology — fused kernel → custom all-reduce → NCCL ring → host fallback — and degrades gracefully when the fast path's preconditions are not met.

Hold this next to the roofline mental model from Part 1: just as compute has a memory-bound vs compute-bound regime, **communication has a latency-bound (small message) vs bandwidth-bound (large message) regime**, and the runtime switches collective algorithms across that boundary the same way it switches GEMV vs GEMM.

---

**8. Inference-engineering takeaways**

* **The collective path is a real latency lever at `TP ≥ 4` decode.** Small-message all-reduce is latency-bound; the custom one-shot kernel can materially cut per-layer comm vs NCCL ring. This is the concrete mechanism behind Lecture 04's "TP=8 sacrifices ~25% to communication."
* **Topology gates the fast path.** Custom/fused all-reduce needs full P2P/NVLink. On a PCIe-only or partially-connected box it silently falls back to NCCL — so the *same* `TP=8` config can have very different decode latency on different chassis. Verify NVLink/NVSwitch (Lecture 04 §3) before blaming the model.
* **Diagnose by trace.** In Nsight Systems, a healthy small-message decode shows a **custom all-reduce kernel** (or a fused allreduce-norm), *not* a chain of NCCL ring kernels. NCCL ring kernels dominating the decode timeline = the fast path is disabled (no P2P, unsupported dtype/shape, or it was turned off).
* **The flags to know:** `VLLM_USE_CUSTOM_ALL_REDUCE` (on by default; disable to A/B against NCCL), `NCCL_DEBUG=INFO` (confirms NCCL topology/algorithm), and the FlashInfer attention/fusion backend selector from Lecture 05. Pin and record these in the bench harness, like every other version.
* **Don't over-index on prefill.** For large prefill messages, plain NCCL ring is already near-optimal — the custom/fused paths buy little there. The win is concentrated in **decode**, which is where your $/MTok lives.

---

**Lab — see the collective path switch**

Extend the Part 2 bench harness:

1. **Serve Llama 3.3 70B FP8 at `TP=8`** on an NVLink box and capture a **decode** trace (Nsight Systems, ~50 steps). Identify the per-layer all-reduce kernel — custom AR or NCCL ring?
2. **Disable the custom path** (`VLLM_USE_CUSTOM_ALL_REDUCE=0`), re-trace, and measure the **TPOT delta**. Attribute it to the NCCL ring's small-message latency.
3. **Repeat for prefill** (long prompt) and show the delta is much smaller — the message is now bandwidth-bound and NCCL ring is competitive.
4. **(If available)** run the same config on a PCIe-only box and show the fast path never engages.

Pass criterion: a one-page report that states, with trace evidence, which collective path ran in prefill vs decode, what the custom-AR-off penalty was, and why it differed between phases.

---

**Self-check**

1. Why does a runtime ship a custom all-reduce when NCCL already implements all-reduce? Answer in terms of message size and what's actually being paid for at decode.
2. A teammate reports that `TP=8` decode is slow on a new server but fast on the old one, same model and vLLM version. Name the first thing you'd check and why.
3. What does fusing all-reduce with RMSNorm save, concretely (count the HBM round-trips before and after)?
4. Why is all2all fundamentally harder to optimize than all-reduce? (Think about whether the per-rank payload size is known ahead of time.)
5. In an Nsight decode trace you see a chain of NCCL ring kernels per layer instead of a single custom-AR kernel. List three causes.

---

**References**

* vLLM distributed communicators — [github.com/vllm-project/vllm](https://github.com/vllm-project/vllm) (`vllm/distributed/device_communicators/`)
* NCCL — [docs.nvidia.com/deeplearning/nccl](https://docs.nvidia.com/deeplearning/nccl/user-guide/docs/index.html) (ring/tree algorithms, all-reduce/all2all)
* FlashInfer (fused attention + collective kernels) — [arXiv:2501.01005](https://arxiv.org/abs/2501.01005)
* Cross-reference: [Lecture 04 — Tensor parallelism](Lecture-04.md) (the all-reduce cost model) · [Lecture 05 §2.4 — FlashInfer](Lecture-05.md) · [Part 3 — MoE at Blackwell](../03-Blackwell-MoE/README.md) (all2all / expert parallelism)

---

**Current as of 2026-06**

Reflects the vLLM `device_communicators` architecture (custom all-reduce, FlashInfer fused all-reduce, NCCL baseline, all2all for EP) as of the 0.22-era lineage. Exact module names and the fused-path availability shift between releases — re-pin against the installed vLLM version. Refresh when a new intra-node collective primitive (e.g., NVLink-native switch collectives, NVLS) becomes the default path.

---

</details>

## Part 2 结束

现在已经看过 Hopper 上的 dense 栈，从模型结构一直到单个 collective 的 kernel。harness（agent 运行时框架）、precision recipe、TP 扩展纪律、serving 调参，以及通信路径这一视角都会沿用下去——Part 3 会复用它们，并扩展到 Blackwell 上的稀疏 MoE，在那里 all2all 成为占主导地位的 collective。

* 下一篇：[Part 3 — Blackwell 上的 MoE](../03-Blackwell-MoE/README.md) — DeepSeek V3.1 + Qwen3-MoE 235B-A22B 锚点、EP、FP4、分离式 P/D
* 上一篇：[Lecture 06 — Hopper 上的 128K 长上下文](Lecture-06.md)
* 上级：[Part 2 — Hopper 上的 dense](README.md)


<details>
<summary>English original</summary>

**End of Part 2**

You have now seen the dense-at-Hopper stack from model anatomy down to the per-collective kernel. The harness, precision recipes, TP-scaling discipline, serving knobs, and the communication-path lens all carry forward — Part 3 reuses them and extends to sparse MoE on Blackwell, where all2all becomes the dominant collective.

* Next: [Part 3 — MoE at Blackwell](../03-Blackwell-MoE/README.md) — DeepSeek V3.1 + Qwen3-MoE 235B-A22B anchors, EP, FP4, disaggregated P/D
* Previous: [Lecture 06 — Long context at 128K on Hopper](Lecture-06.md)
* Up: [Part 2 — Dense at Hopper](README.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/AI Inference Engineer 2026/Part 2 - Dense at Hopper/Lecture-07.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/AI%20Inference%20Engineer%202026/Part%202%20-%20Dense%20at%20Hopper/Lecture-07.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
