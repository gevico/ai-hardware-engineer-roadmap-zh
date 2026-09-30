---
title: 第 07 讲 - 边缘与物理 AI 前沿：1000-TOPS 硬件、端侧模型与 capstone
description: 第 07 讲 - 边缘与物理 AI 前沿：1000-TOPS 硬件、端侧模型与 capstone
---

# 第 07 讲 - 边缘与物理 AI 前沿：1000-TOPS 硬件、端侧模型与 capstone

**合集：** [MLSys Deep Dives](README.md) | **上一讲：** [← 第 06 讲](Lecture-06.md) | **下一讲：** [MLSys Deep Dives 索引](README.md)

---

本课程一直置身于数据中心。最后一讲将 *同样的* MLSys 纪律带到另一种预算 —— **边缘**，这里的约束不是每 GPU 小时多少美元，而是**瓦特**，也是 AI 在机器人、汽车和嵌入式设备中与物理世界相遇之处。然后它闭合循环：第 1–6 讲的架构、kernel、编译器和 decode（逐 token 生成阶段）算法层是**一个协同设计的系统**，而 capstone 就是用一个真实模型和一个数字来证明它。

本节从厘清该领域常引发的混淆开始 —— 两个不同的“1000” —— 因为分清它们本身就是资深工程师的标志。

---

## 学习目标

本讲结束时，你应该能够：

1. 区分 **1000 TOPS**（边缘硬件*容量*）与 **1000 tokens/s**（数据中心推理服务*吞吐*）—— 并以正确的精度/稀疏性怀疑态度解读 TOPS 数字。
2. 描述 **1000-TOPS 级边缘硬件**（Jetson AGX Thor、DRIVE Thor）及其真正的约束：**内存带宽和功耗**，而非峰值 TOPS。
3. 解释为何边缘推理是在**功耗受限**预算下的*同样的* MLSys 纪律，以及为何 **perf/watt** 是绑定性指标。
4. 为边缘目标选择端侧模型（小/混合）和 runtime（TensorRT、MLC-LLM、llama.cpp）。
5. 画出将全部七讲连接到 tokens/s 和 tokens/s/watt 的**协同设计循环**。
6. 执行 **capstone**：一份优化阶梯报告，将每一级阶梯与 roofline（性能上界模型）上界和一个成本数字关联起来。

---

## 1. 两个“1000”

你会听到“1000”以两种截然不同的方式与 AI 系统关联，混淆它们会显得像新手。厘清它们：

```text
   "1000 TOPS"   = edge HARDWARE compute capacity   (Jetson Thor, DRIVE Thor)
                   operations/second the chip CAN do — a SUPPLY-side spec
                   ⚠ almost always quoted at the LOWEST precision WITH sparsity (FP4 sparse);
                     the honest dense number is roughly half (FP8)

   "1000 tok/s"  = datacenter SERVING throughput     (MiMo + TileRT, Lecture 6)
                   tokens/second a STACK actually DELIVERS — a DEMAND-side result
                   produced by architecture + quantization + spec-decode + runtime, stacked
```

一个是芯片 *可能* 做到的事；另一个是软件栈 *实际* 达到的事。前者是数据手册上的数字；后者是整个课程的输出。这里嵌入的教训是 **TOPS 怀疑论**：每当你看到 TOPS 数字，都要问 *在什么精度下，稠密还是稀疏？* —— 因为厂商引用的是最讨喜的组合（FP4 + 结构化稀疏），而你在真实工作负载上实际能维持的数字往往只是它的一部分。这与第 1 讲中“印刷吞吐是过时的锚点”规则是同一纪律，只是应用于供给侧。

---


<details>
<summary>English original</summary>

**Lecture 07 - The Edge and Physical-AI Frontier: 1000-TOPS Hardware, On-Device Models, and the Capstone**

**Collection:** [MLSys Deep Dives](README.md) | **Previous:** [← Lecture 06](Lecture-06.md) | **Next:** [MLSys Deep Dives index](README.md)

---

The course has lived in the datacenter. This final lecture takes the *same* MLSys discipline to the other budget — the **edge**, where the constraint is not dollars-per-GPU-hour but **watts**, and where AI meets the physical world in robots, cars, and embedded devices. Then it closes the loop: the architecture, kernel, compiler, and decode-algorithm layers of Lectures 1–6 are **one co-designed system**, and the capstone is to prove it on a real model with a number.

We start by untangling a confusion the field invites — the two different "1000s" — because keeping them straight is itself a senior-engineer signal.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Distinguish **1000 TOPS** (edge hardware *capacity*) from **1000 tokens/s** (datacenter serving *throughput*) — and read TOPS numbers with the right precision/sparsity skepticism.
2. Describe **1000-TOPS-class edge hardware** (Jetson AGX Thor, DRIVE Thor) and its real constraint: **memory bandwidth and power**, not peak TOPS.
3. Explain why edge inference is the *same* MLSys discipline at a **power-capped** budget, and why **perf/watt** is the binding metric.
4. Choose on-device models (small/hybrid) and runtimes (TensorRT, MLC-LLM, llama.cpp) for an edge target.
5. Draw the **co-design loop** connecting all seven lectures to tokens/s and tokens/s/watt.
6. Execute the **capstone**: an optimization-ladder report tying each rung to a roofline bound and a cost number.

---

**1. The two "1000s"**

You will hear "1000" attached to AI systems in two completely different ways, and conflating them marks a novice. Untangle them:

```text
   "1000 TOPS"   = edge HARDWARE compute capacity   (Jetson Thor, DRIVE Thor)
                   operations/second the chip CAN do — a SUPPLY-side spec
                   ⚠ almost always quoted at the LOWEST precision WITH sparsity (FP4 sparse);
                     the honest dense number is roughly half (FP8)

   "1000 tok/s"  = datacenter SERVING throughput     (MiMo + TileRT, Lecture 6)
                   tokens/second a STACK actually DELIVERS — a DEMAND-side result
                   produced by architecture + quantization + spec-decode + runtime, stacked
```

One is what the silicon *could* do; the other is what a software stack *actually* achieved. The first is a number on a datasheet; the second is the output of this entire course. The lesson embedded here is **TOPS skepticism**: whenever you see a TOPS figure, ask *at what precision, dense or sparse?* — because vendors quote the most flattering combination (FP4 + structured sparsity), and the number you can actually sustain on a real workload is often a fraction of it. This is the same discipline as the "printed throughput is a dated anchor" rule from Lecture 1, applied to the supply side.

---

</details>

## 2. 1000 TOPS 级边缘硬件

“1000 TOPS”在 2025–2026 年对应的具体器件是 **NVIDIA Jetson AGX Thor** 这一档（以及它的车规姊妹型号 **DRIVE Thor**），为 **physical AI** 打造 —— 机器人、自主机器、具身 agent。

| 规格 | Jetson AGX Thor |
|---|---|
| GPU | **Blackwell**，2560 个 CUDA 核心，96 个第五代 Tensor Core |
| AI 性能 | **~2070 FP4 TFLOPS（稀疏）** / **~1035 FP8 TFLOPS（稠密）** |
| CPU | 14× Arm Neoverse-V3AE |
| 内存 | **128 GB LPDDR5X，273 GB/s** |
| 功耗 | **75–130 W** |
| 开发套件 | $3,499（2025 年 11 月）；相比 AGX Orin AI 性能约 7.5×，能效约 3.5× |

注意这个头条数字落在哪里，以及*真正*的约束在哪里：

* “2070 TOPS”是 **FP4 加稀疏**；诚实的持续值是 **~1035 FP8 TFLOPS 稠密**。（对 TOPS 的怀疑，见 §1。）
* 对 LLM decode（逐 token 生成阶段）而言，绑定约束**不是** TOPS —— 而是 **273 GB/s 内存带宽**。拿它和数据中心 GPU 的**数 TB/s HBM** 相比：边缘的带宽少了约 10–30×。由于 decode 是带宽受限的（Lecture 1、6），**边缘比数据中心更缺带宽** —— 这让量化、混合架构和投机解码在这里*更*重要，而不是更不重要。
* **128 GB 统一内存**很宽裕（这是机器人的整颗大脑），但 **75–130 W** 是硬墙。功耗封顶，就这样。

对这台机器跑一遍 Lecture 1 的**带宽上限检查**，整个边缘的故事就能从三行算术里推出来：

```text
   batch-1 decode ceiling on Thor (273 GB/s):
       7B  @ FP16  ≈ ~14  GB weights   →   273 / 14    ≈  ~19 tok/s
       7B  @ INT4  ≈ ~3.5 GB weights   →   273 / 3.5   ≈  ~78 tok/s
       70B @ INT4  ≈ ~35  GB weights   →   273 / 35    ≈   ~8 tok/s
   add speculative decoding at τ ≈ 3 (Lec 6) → the 7B-INT4 box clears ~200 tok/s in theory
```

注意那套算术里*没有*出现什么：2070 TOPS 这个头条数字。带宽和字节决定了一切 —— 这正是上面那条要点的具体证明，也说明了为什么在边缘**量化不是锦上添花，而是可用机器人和 19-tok/s 的机器人之间的差别。**

竞争产品（DRIVE Thor、Qualcomm Snapdragon Ride Flex）处在同一个 **1000–2000 TOPS、130 W 以下**的区间，面向集中式汽车/机器人计算。这类硬件上，推理模型必须在*功耗预算之内*、实时地、紧挨着传感器运行。

---

## 3. 边缘是同一套方法，只是功耗封顶

这是整门课的统一论断。Lecture 1 的成本方程 ——

```text
   value delivered  =        tokens per second
                      ───────────────────────────────
                       budget you are capped against

   datacenter:  budget = $/GPU-hour   →  optimize TOK/$  (TCO/Mtok)
   edge:        budget = WATTS         →  optimize tokens/s/WATT  (and fit in memory)
```

—— 是*同一个方程*；只是分母的单位变了。而且**杠杆完全相同**：量化到更少的位（FP4/INT4），选择混合架构压缩 KV cache 让模型装进带宽，用投机解码在一次内存读取中拿到更多 token，用好 kernel/编译器和 megakernel runtime 来削减浪费。Lecture 2–6 里的一切在边缘原样适用 —— 只是评分标准从 **perf/dollar** 变成了 **perf/watt**。

所以 MLSys 工程师从云端转到机器人时并不换学科。他们把*同一套*方法重新对准功耗预算。一个 7B 推理模型在 Jetson 上于 40 W 以内跑到可接受的 tokens/s，和数据中心里 1T MoE（混合专家模型）跑到 1000 tok/s 是同一种胜利 —— 两者都是 tokens-per-(capped resource)，都由同一套栈搭出来。

---


<details>
<summary>English original</summary>

**2. 1000-TOPS-class edge hardware**

The concrete device behind "1000 TOPS" in 2025–2026 is the **NVIDIA Jetson AGX Thor** class (and its automotive sibling **DRIVE Thor**), built for **physical AI** — robotics, autonomous machines, embodied agents.

| Spec | Jetson AGX Thor |
|---|---|
| GPU | **Blackwell**, 2560 CUDA cores, 96 5th-gen Tensor Cores |
| AI perf | **~2070 FP4 TFLOPS (sparse)** / **~1035 FP8 TFLOPS (dense)** |
| CPU | 14× Arm Neoverse-V3AE |
| Memory | **128 GB LPDDR5X, 273 GB/s** |
| Power | **75–130 W** |
| Dev kit | $3,499 (Nov 2025); ~7.5× AI perf, ~3.5× efficiency vs AGX Orin |

Note where the headline number lands and where the *real* constraint is:

* The "2070 TOPS" is **FP4 with sparsity**; the honest sustained figure is **~1035 FP8 TFLOPS dense**. (TOPS skepticism, §1.)
* The binding constraint for LLM decode is **not** the TOPS — it's the **273 GB/s memory bandwidth**. Compare that to a datacenter GPU's **multiple TB/s of HBM**: the edge has ~10–30× less bandwidth. Since decode is memory-bound (Lecture 1, 6), **the edge is even more bandwidth-starved than the datacenter** — which makes quantization, hybrid architectures, and speculative decoding *more* important here, not less.
* **128 GB of unified memory** is generous (it's a robot's whole brain), but **75–130 W** is the hard wall. You are power-capped, full stop.

Run Lecture 1's **bandwidth-ceiling check** on this box and the whole edge story falls out of three lines of arithmetic:

```text
   batch-1 decode ceiling on Thor (273 GB/s):
       7B  @ FP16  ≈ ~14  GB weights   →   273 / 14    ≈  ~19 tok/s
       7B  @ INT4  ≈ ~3.5 GB weights   →   273 / 3.5   ≈  ~78 tok/s
       70B @ INT4  ≈ ~35  GB weights   →   273 / 35    ≈   ~8 tok/s
   add speculative decoding at τ ≈ 3 (Lec 6) → the 7B-INT4 box clears ~200 tok/s in theory
```

Notice what *didn't* appear in that math: the 2070-TOPS headline. Bandwidth and bytes decided everything — which is the concrete proof of the bullet above, and of why on the edge **quantization is not a nice-to-have, it is the difference between a usable robot and a 19-tok/s one.**

The competitive set (DRIVE Thor, Qualcomm Snapdragon Ride Flex) lives in the same **1000–2000 TOPS, sub-130 W** band, targeting centralized automotive/robotics compute. This is the hardware where a reasoning model has to run *inside a power budget*, in real time, next to sensors.

---

**3. The edge is the same discipline, power-capped**

Here is the unifying claim of the whole course. The cost equation from Lecture 1 —

```text
   value delivered  =        tokens per second
                      ───────────────────────────────
                       budget you are capped against

   datacenter:  budget = $/GPU-hour   →  optimize TOK/$  (TCO/Mtok)
   edge:        budget = WATTS         →  optimize tokens/s/WATT  (and fit in memory)
```

— is *the same equation*; only the denominator's units change. And the **levers are identical**: quantize to fewer bits (FP4/INT4), pick a hybrid architecture to shrink the KV cache so the model fits the bandwidth, use speculative decoding to get more tokens per memory pass, use good kernels/compilers and megakernel runtimes to cut waste. Everything in Lectures 2–6 applies unchanged at the edge — it just gets graded on **perf/watt** instead of **perf/dollar**.

So an MLSys engineer does not switch disciplines moving from cloud to robot. They re-point the *same* discipline at a power budget. A 7B reasoning model that runs at acceptable tokens/s inside 40 W on a Jetson is the same kind of win as a 1T MoE at 1000 tok/s in a datacenter — both are tokens-per-(capped resource), both built from the same stack.

---

</details>

## 4. 端侧模型与 runtime

一台 1000-TOPS、273-GB/s、100-W 的盒子能跑什么？模型选择直接对应到 Lec 4–5：

* **小型稠密推理模型** —— **MiMo-7B**、Nemotron Nano、Phi 级别。7B 在 INT4 下内存和带宽都绰绰有余；MiMo 的 MTP 头（Lec 5）白送端侧投机解码。
* **混合 / SSM 模型** —— **Falcon-H1-1.5B/3B**、小型 Mamba 混合模型。它们的**内存随上下文保持平坦**（Lec 4）在带宽成为瓶颈时价值翻倍 —— 恒定状态模型不会随上下文增长去反复冲击 273 GB/s 总线。
* **激进的量化是必须的**，不是可选项：FP4/INT4 权重既能塞进内存，*也*能成倍提升有效带宽（每个 token 流式读取时每参数摊到的字节数更少）。边缘正是精度下限被压得最狠的地方。

runtime：

| Runtime | Edge fit |
|---|---|
| **TensorRT / TensorRT-LLM** | 在 NVIDIA Jetson/DRIVE 上最佳；闭源，但性能达峰 |
| **MLC-LLM**（TVM Unity） | 跨平台 —— 同一模型可到 Jetson、手机 GPU、浏览器；量化、dlight 调度（见 [TVM Deep Dives](../06-TVM深度探索/README.md)） |
| **llama.cpp** | 无处不在的 CPU/边缘 GGUF runtime，最适合最小的目标设备 |

这个选择就是 Lec 3 的决策（闭源厂商峰值 vs 可移植编译器）在功耗预算下被重新问了一遍。在 Jetson 上你往往为了峰值跑 TensorRT-LLM；而对于还必须同时命中手机和浏览器的模型，MLC-LLM 的一模型多目标路径胜出。

---

## 5. 协同设计闭环闭合

退后一步，把整门课看成一张图。每一讲都是同一个分母上的不同杠杆，而且它们会**复合放大**：

```text
   THE CO-DESIGN LOOP  (the whole course, as one system)
   ┌──────────────────────────────────────────────────────────────────┐
   │  ARCHITECTURE   (Lec 4–5)  MoE · MLA · SSM/hybrid                  │  ← less work per token
   │       │                                                            │
   │  KERNELS+COMPILERS (Lec 2–3)  tiles · fusion · megakernels          │  ← each op faster, gaps gone
   │       │                                                            │
   │  INFERENCE ALGOS (Lec 6)  spec decode (EAGLE-3/DFlash) · Flash      │  ← more tokens / memory pass
   │       │                                                            │
   │  HARDWARE+PRECISION (Lec 7)  FP4 · right chip · right batch         │  ← right bits, right silicon
   └───────────────────────────────┬──────────────────────────────────┘
                                   ▼
            tokens/s ↑   AND   tokens/s/watt ↑   →   $/Mtok ↓   (Lecture 1)
```

MiMo + TileRT 的结果（Lec 6）就是这个环路完整堆叠起来的样子：sparse MoE（架构）+ MXFP4（精度）+ DFlash（算法）+ TileRT megakernel（runtime）。没有哪一层单独产生了 1000 tok/s；是它们的*乘积*做到的。这就是资深 MLSys 的世界观：**你不是优化单层，而是协同设计整个技术栈**，并且在顶端用 tokens/s 和美元（或瓦特）来衡量这个复合结果。

---


<details>
<summary>English original</summary>

**4. On-device models and runtimes**

What runs on a 1000-TOPS, 273-GB/s, 100-W box? The model choices map directly onto Lectures 4–5:

* **Small dense reasoning models** — **MiMo-7B**, Nemotron Nano, Phi-class. 7B at INT4 fits comfortably in memory and bandwidth; MiMo's MTP heads (Lec 5) give on-device speculative decoding for free.
* **Hybrid / SSM models** — **Falcon-H1-1.5B/3B**, small Mamba hybrids. Their **flat-in-context memory** (Lec 4) is doubly valuable when bandwidth is the wall — a constant-state model doesn't thrash the 273 GB/s bus as context grows.
* **Aggressive quantization is mandatory**, not optional: FP4/INT4 weights to fit memory *and* to multiply effective bandwidth (fewer bytes per parameter streamed per token). The edge is where the precision floor gets pushed hardest.

The runtimes:

| Runtime | Edge fit |
|---|---|
| **TensorRT / TensorRT-LLM** | best on NVIDIA Jetson/DRIVE; closed but peak |
| **MLC-LLM** (TVM Unity) | cross-platform — the same model to Jetson, phone GPU, browser; quantized, dlight schedules (see [TVM Deep Dives](../06-TVM深度探索/README.md)) |
| **llama.cpp** | ubiquitous CPU/edge GGUF runtime, great for the smallest targets |

The choice is the Lecture-3 decision (closed-vendor-peak vs portable-compiler) re-asked under a power budget. On a Jetson you'll often run TensorRT-LLM for peak; for a model that must *also* hit a phone and a browser, MLC-LLM's one-model-many-targets path wins.

---

**5. The co-design loop closes**

Step back and see the whole course as one diagram. Every lecture was a different lever on the same denominator, and they **compound**:

```text
   THE CO-DESIGN LOOP  (the whole course, as one system)
   ┌──────────────────────────────────────────────────────────────────┐
   │  ARCHITECTURE   (Lec 4–5)  MoE · MLA · SSM/hybrid                  │  ← less work per token
   │       │                                                            │
   │  KERNELS+COMPILERS (Lec 2–3)  tiles · fusion · megakernels          │  ← each op faster, gaps gone
   │       │                                                            │
   │  INFERENCE ALGOS (Lec 6)  spec decode (EAGLE-3/DFlash) · Flash      │  ← more tokens / memory pass
   │       │                                                            │
   │  HARDWARE+PRECISION (Lec 7)  FP4 · right chip · right batch         │  ← right bits, right silicon
   └───────────────────────────────┬──────────────────────────────────┘
                                   ▼
            tokens/s ↑   AND   tokens/s/watt ↑   →   $/Mtok ↓   (Lecture 1)
```

The MiMo + TileRT result (Lec 6) was this loop, fully stacked: sparse MoE (architecture) + MXFP4 (precision) + DFlash (algorithm) + TileRT megakernel (runtime). No single layer produced 1000 tok/s; the *product* did. That is the senior-MLSys worldview: **you do not optimize one layer, you co-design the stack**, and you measure the compound at the top in tokens/s and dollars (or watts).

---

</details>

## 6. 毕业项目：优化阶梯

课程产物。选**一个模型与一个目标平台**（数据中心 GPU *或*一块边缘板卡——纪律相同），沿优化阶梯逐级下行，**测量每一级**：

```text
   THE OPTIMIZATION LADDER  — one model, one target, a number at every rung
   ─────────────────────────────────────────────────────────────────────────────
   rung 0  baseline (eager, FP16)                          tokens/s · $/Mtok or tok/s/W · TTFT/TPOT
   rung 1  + quantize (FP8 / FP4 / INT4)                   Δ + which roofline bound moved
   rung 2  + better kernels / compiler (Triton/TVM/TRT)    Δ + GFLOP/s vs roofline
   rung 3  + speculative decoding (EAGLE-3 / DFlash)        Δ + acceptance length τ + outputs-identical?
   rung 4  + (if applicable) hybrid arch / megakernel       Δ + KV-cache or gap reduction
   ─────────────────────────────────────────────────────────────────────────────
   for EACH rung: name the roofline bound it moved, and the $/Mtok (or tok/s/W) delta.
```

使其成为真正的工程、而非 recipe 日志的规则：

* **测量，不要估算。** 每一级都要有实测的 tokens/s 和重算的成本（Lecture 1 的模型）。测不出来的一级等于没发生。
* **点明受限类型。** 对每一级，说明它推动了哪个 roofline 区间——带宽受限 → 量化/混合；算力受限 → 更好的 kernel；launch/gap-bound → megakernel；serial-decode-bound → 投机解码。若说不出受限类型，就还没理解这一级为何有效。
* **每一级都要保证精度一致性。** 量化以及（尤其是）投机解码必须在预算内保持输出一致——spec decode 必须*无损*。更快的错误模型就是退步。
* **以复合效应收尾。** 头条是整条阶梯的结果：基线 `$/Mtok`（或 tok/s/W）→ 最终，以及倍数。那一个数字就是你的作品集。

这是一个 **Level-5 产物**：另一位工程师 clone 你的 repo、跑一遍这条阶梯，就能在同类硬件上复现你的数字。这也是你理解 MLSys 的最诚实的证明——因为它迫使课程的每一层都以可测量、可辩护、与成本挂钩的步骤出现。

---

## 7. 迷你实验（与课程收尾）

如果完整毕业项目太大，就做三级的版本：**基线 → 量化 → 投机解码**，在你跑得动的任意模型+目标平台上，逐级测量 tokens/s 和 `$/Mtok`（或 tok/s/W），并点明 roofline 受限类型、确认精度一致性。

然后用文字回答课程的收尾问题：*为什么 Jetson 上 40 W 的混合 7B 与数据中心里 1000 tok/s 的 1T MoE 是同一种 MLSys 纪律？* 如果你的答案是「因为两者都通过协同设计架构、kernel、编译器与 decode 算法，最大化每（受限资源）token 数，且都用成本方程来衡量」——那你就拥有了这门课程要建立的世界观。

---

## 关键要点

- **两个「1000」**：1000 **TOPS** = 边缘硬件的*容量*（供给侧，标称的是 FP4-稀疏——要保持怀疑）；1000 **tok/s** = 数据中心推理服务的*吞吐*（需求侧，整条技术栈的产出）。
- **1000-TOPS 级边缘**（Jetson/DRIVE Thor）受限于**内存带宽（~273 GB/s）与功耗（75–130 W）**，而非峰值 TOPS。边缘比数据中心*更*缺带宽，因此 Lec 2–6 的杠杆作用*更大*。
- **边缘是同一套纪律，只是被功耗封顶**：`value = tokens/s ÷ budget`，预算单位是瓦而非美元。**perf/watt** 是决定性指标；杠杆（量化、混合架构、spec decode、好 kernel）不变。
- 端侧：小型稠密（MiMo-7B）与混合（Falcon-H1）模型，**激进的 FP4/INT4** 量化，runtime 选 TensorRT-LLM（NVIDIA 峰值）/ MLC-LLM（跨平台）/ llama.cpp（最小）。
- **协同设计闭环**把七讲串在一起：架构 × kernel/编译器 × 推理算法 × 硬件/精度，**复利**成 tokens/s 与 tokens/s/watt。你协同设计整条栈；不是只优化某一层。
- **毕业项目**就是一条优化阶梯——基线 → 量化 → kernel/编译器 → spec decode →（混合/megakernel）——每一级都有实测的成本数字和点明的 roofline 受限类型。这就是你理解 MLSys 的证明。

---


<details>
<summary>English original</summary>

**6. Capstone: the optimization ladder**

The course artifact. Pick **one model and one target** (a datacenter GPU *or* an edge board — the discipline is the same) and walk it down an optimization ladder, **measuring every rung**:

```text
   THE OPTIMIZATION LADDER  — one model, one target, a number at every rung
   ─────────────────────────────────────────────────────────────────────────────
   rung 0  baseline (eager, FP16)                          tokens/s · $/Mtok or tok/s/W · TTFT/TPOT
   rung 1  + quantize (FP8 / FP4 / INT4)                   Δ + which roofline bound moved
   rung 2  + better kernels / compiler (Triton/TVM/TRT)    Δ + GFLOP/s vs roofline
   rung 3  + speculative decoding (EAGLE-3 / DFlash)        Δ + acceptance length τ + outputs-identical?
   rung 4  + (if applicable) hybrid arch / megakernel       Δ + KV-cache or gap reduction
   ─────────────────────────────────────────────────────────────────────────────
   for EACH rung: name the roofline bound it moved, and the $/Mtok (or tok/s/W) delta.
```

Rules that make it real engineering, not a recipe log:

* **Measure, don't estimate.** Every rung gets a measured tokens/s and a recomputed cost (Lecture 1's model). A rung you can't measure didn't happen.
* **Name the bound.** For each rung, state which roofline regime it moved — memory-bound → quantization/hybrid; compute-bound → better kernels; launch/gap-bound → megakernel; serial-decode-bound → speculation. If you can't name the bound, you don't yet understand why the rung helped.
* **Parity at every rung.** Quantization and (especially) speculative decoding must preserve outputs within budget — spec decode *losslessly*. A faster wrong model is a regression.
* **End with the compound.** The headline is the full-ladder result: baseline `$/Mtok` (or tok/s/W) → final, and the multiplier. That single number is your portfolio.

This is a **Level-5 artifact**: another engineer clones your repo, runs the ladder, and reproduces your numbers on the same hardware class. It is also the most honest possible demonstration that you understand MLSys — because it forces every layer of the course to show up as a measured, defended, cost-connected step.

---

**7. Mini-lab (and course wrap)**

If the full capstone is too large, do a three-rung version: **baseline → quantize → speculative decode**, on any model+target you can run, measuring tokens/s and `$/Mtok` (or tok/s/W) at each, with the roofline bound named and parity confirmed.

Then answer the course's closing question in writing: *Why is a hybrid 7B at 40 W on a Jetson and a 1T MoE at 1000 tok/s in a datacenter the same MLSys discipline?* If your answer is "because both maximize tokens-per-(capped resource) by co-designing architecture, kernels, compilers, and decode algorithms, and both are measured against the cost equation" — you have the worldview this course exists to build.

---

**Key takeaways**

- **Two "1000s"**: 1000 **TOPS** = edge hardware *capacity* (supply-side, quoted FP4-sparse — be skeptical); 1000 **tok/s** = datacenter serving *throughput* (demand-side, the output of the whole stack).
- **1000-TOPS-class edge** (Jetson/DRIVE Thor) is bounded by **memory bandwidth (~273 GB/s) and power (75–130 W)**, not peak TOPS. The edge is *more* bandwidth-starved than the datacenter, so Lec 2–6's levers matter *more*.
- The **edge is the same discipline, power-capped**: `value = tokens/s ÷ budget`, with budget = watts instead of dollars. **perf/watt** is the binding metric; the levers (quantize, hybrid arch, spec decode, good kernels) are unchanged.
- On-device: small dense (MiMo-7B) and hybrid (Falcon-H1) models, **aggressive FP4/INT4** quantization, runtimes TensorRT-LLM (peak NVIDIA) / MLC-LLM (cross-platform) / llama.cpp (smallest).
- The **co-design loop** ties all seven lectures together: architecture × kernels/compilers × inference algorithms × hardware/precision **compound** into tokens/s and tokens/s/watt. You co-design the stack; you don't optimize one layer.
- The **capstone** is an optimization ladder — baseline → quantize → kernels/compiler → spec decode → (hybrid/megakernel) — with a measured cost number and a named roofline bound at every rung. That is the proof you understand MLSys.

---

</details>

## 参考文献

- NVIDIA Jetson AGX Thor（physical AI 平台）：[https://developer.nvidia.com/blog/introducing-nvidia-jetson-thor-the-ultimate-platform-for-physical-ai/](https://developer.nvidia.com/blog/introducing-nvidia-jetson-thor-the-ultimate-platform-for-physical-ai/)
- Jetson AGX Thor 开发套件规格（2070 TOPS FP4 / 1035 TFLOPS FP8，128 GB，273 GB/s）：[https://www.cnx-software.com/2025/08/19/3499-nvidia-jetson-agx-thor-developer-kit-2070-tops-jetson-t5000-som-for-robotics-and-edge-ai/](https://www.cnx-software.com/2025/08/19/3499-nvidia-jetson-agx-thor-developer-kit-2070-tops-jetson-t5000-som-for-robotics-and-edge-ai/)
- MLC-LLM（跨平台端侧大语言模型，TVM Unity）：[https://github.com/mlc-ai/mlc-llm](https://github.com/mlc-ai/mlc-llm)
- llama.cpp：[https://github.com/ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp)
- SemiAnalysis InferenceMAX / InferenceX（perf/$、perf/watt）：[https://newsletter.semianalysis.com/p/inferencemax-open-source-inference](https://newsletter.semianalysis.com/p/inferencemax-open-source-inference)
- *TVM Deep Dives* — [Lecture 05 — Shipping it: runtime, microTVM, MLC-LLM](../06-TVM深度探索/Lecture-05.md)，对应端侧部署路径。
- *MLSys Deep Dives* — [Lecture 01 — the cost equation](Lecture-01.md)，本 capstone 以此为衡量基准。

---

## 数据截至

2026-06。版本固定为：Jetson AGX Thor（Blackwell，约 2070 FP4 / 约 1035 FP8 TFLOPS，128 GB LPDDR5X @ 273 GB/s，75–130 W，2025 年 11 月开发套件 $3,499），DRIVE Thor / Snapdragon Ride 处于 1000–2000 TOPS 区间。TOPS 数字是 FP4-sparse 营销数字——持续 dense FP8 大约只有一半；务必重新核对 precision/sparsity，并针对实际 workload 重新跑 benchmark。

---

*返回：[MLSys Deep Dives 索引](README.md)*


<details>
<summary>English original</summary>

**References**

- NVIDIA Jetson AGX Thor (physical AI platform): [https://developer.nvidia.com/blog/introducing-nvidia-jetson-thor-the-ultimate-platform-for-physical-ai/](https://developer.nvidia.com/blog/introducing-nvidia-jetson-thor-the-ultimate-platform-for-physical-ai/)
- Jetson AGX Thor dev kit specs (2070 TOPS FP4 / 1035 TFLOPS FP8, 128 GB, 273 GB/s): [https://www.cnx-software.com/2025/08/19/3499-nvidia-jetson-agx-thor-developer-kit-2070-tops-jetson-t5000-som-for-robotics-and-edge-ai/](https://www.cnx-software.com/2025/08/19/3499-nvidia-jetson-agx-thor-developer-kit-2070-tops-jetson-t5000-som-for-robotics-and-edge-ai/)
- MLC-LLM (cross-platform on-device LLM, TVM Unity): [https://github.com/mlc-ai/mlc-llm](https://github.com/mlc-ai/mlc-llm)
- llama.cpp: [https://github.com/ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp)
- SemiAnalysis InferenceMAX / InferenceX (perf/$, perf/watt): [https://newsletter.semianalysis.com/p/inferencemax-open-source-inference](https://newsletter.semianalysis.com/p/inferencemax-open-source-inference)
- *TVM Deep Dives* — [Lecture 05 — Shipping it: runtime, microTVM, MLC-LLM](../06-TVM深度探索/Lecture-05.md), for the on-device deployment path.
- *MLSys Deep Dives* — [Lecture 01 — the cost equation](Lecture-01.md), which this capstone measures against.

---

**Current as of**

2026-06. Pins: Jetson AGX Thor (Blackwell, ~2070 FP4 / ~1035 FP8 TFLOPS, 128 GB LPDDR5X @ 273 GB/s, 75–130 W, $3,499 dev kit Nov 2025), DRIVE Thor / Snapdragon Ride in the 1000–2000 TOPS band. TOPS figures are FP4-sparse marketing numbers — sustained dense FP8 is roughly half; always re-check precision/sparsity and re-benchmark on the actual workload.

---

*Back to: [MLSys Deep Dives index](README.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/MLSys Deep Dives/Lecture-07.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/MLSys%20Deep%20Dives/Lecture-07.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
