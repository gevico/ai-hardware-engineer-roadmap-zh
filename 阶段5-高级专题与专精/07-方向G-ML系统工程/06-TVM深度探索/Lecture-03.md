---
title: Lecture 03 - Auto-Tuning: AutoTVM → Ansor → MetaSchedule, the Learning-Based Compiler
description: Lecture 03 - Auto-Tuning: AutoTVM → Ansor → MetaSchedule, the Learning-Based Compiler
---

# Lecture 03 - Auto-Tuning: AutoTVM → Ansor → MetaSchedule, the Learning-Based Compiler

**合集：** [TVM Deep Dives](README.md) | **上一讲：** [← Lecture 02](Lecture-02.md) | **下一讲：** [Lecture 04](Lecture-04.md)

---

Lecture 2 以一个令人不安的事实收尾：一个能与厂商库竞争的 GPU 矩阵乘需要寄存器分块、双缓冲、向量化加载、bank conflict 规避——而正确的组合**对每个 shape、dtype 和芯片都不同**。按算子、按 target、按硬件代际手工调度这一切，不是工程。那是一项无穷尽的工作。

TVM 的决定性思想——让它区别于此前所有编译器的那个思想——是把这项工作变成一个**搜索问题**。把计算描述一次。让机器枚举合法的 schedule，用学习得到的模型预测哪些快，在真实硬件上测量有前景的那些，保留最好的。编译器*学习*kernel，而不是由人写出来。

本讲讲的就是这台机器。追溯它的三代——**AutoTVM**、**Ansor/AutoScheduler**、**MetaSchedule**——因为每一代都消除了上一代留下的人工瓶颈，而这条谱系正是理解 MetaSchedule（当前系统，也是 MLC-LLM 调优所用的引擎）实际在做什么的方式。

---

## 学习目标

学完本讲，你应当能够：

1. 说明自动调优为何存在：schedule 空间太大，且过于 target-specific，无法按 shape 手工摸索。
2. 解释**调优循环**——空间生成 → cost model → 端侧测量 → 搜索 → 数据库——以及哪个资源才是昂贵的那个。
3. 区分三代系统：AutoTVM（**基于模板**）、Ansor（**无模板、自动生成空间**）、MetaSchedule（**TIR 上基于概率的 schedule 程序**，统一二者）。
4. 对 kernel 运行 **`ms.tune_tir`**，对整个模型运行 **`ms.tune_relax`**，然后应用调优数据库并构建。
5. 读懂**调优曲线**（最优延迟 vs 试验次数），并推理 cost model 与测量之间的取舍。
6. 搭起 **RPC 上的分布式调优**，从主机在真实边缘设备上测量。

---

## 1. 为什么搜索胜过手工调优

单个 GPU 矩阵乘 schedule 轻松就有十几个可调决策：`i`、`j`、`k` 上的分块大小（每个都是多路拆分）、共享内存暂存因子、向量化宽度、展开深度、是否使用 Tensor Core。合法的组合数以**百万**计。大多数都很慢。只有少数落在峰值几个百分点以内。人无法枚举它们；人能勉强猜出一个好的角落。

但这个空间有机器可以利用的结构：

```text
the schedule space is huge       → you cannot try everything
but most of it is obviously bad  → a cheap model can rank candidates
and "fast" is target-specific    → only real hardware gives ground truth
and you tune once, run forever   → amortize search cost over deployment
```

所以策略不言自明：**生成**候选，用廉价的学习得到的 cost model **预测**，只在实际设备上**测量**少数有前景的，从这些测量中**学习**，重复。昂贵的资源是端侧测量——每一次都是一次构建 + 运行，耗时从数百毫秒到数秒。cost model 存在的意义正是为了明智地花掉这些测量。

这就是“基于学习的编译器”。这也是为什么 TVM kernel 在非典型 shape 和非厂商 target 上，能*胜过*手写的厂商库：库作者只调了一次常见情形；而搜索调的是**你的**情形、在**你的**芯片上调。

---


<details>
<summary>English original</summary>

**Lecture 03 - Auto-Tuning: AutoTVM → Ansor → MetaSchedule, the Learning-Based Compiler**

**Collection:** [TVM Deep Dives](README.md) | **Previous:** [← Lecture 02](Lecture-02.md) | **Next:** [Lecture 04](Lecture-04.md)

---

Lecture 2 ended on an uncomfortable truth: a vendor-competitive GPU matmul needs register tiling, double-buffering, vectorized loads, bank-conflict avoidance — and the right combination is **different for every shape, dtype, and chip**. Hand-scheduling that, per operator, per target, per generation of hardware, is not engineering. It is an infinite job.

The defining idea of TVM — the one that separated it from every compiler before it — is to make that job a **search problem**. Describe the computation once. Let the machine enumerate legal schedules, predict which are fast with a learned model, measure the promising ones on real hardware, and keep the best. The compiler *learns* the kernel instead of a human writing it.

This lecture is that machine. We trace its three generations — **AutoTVM**, **Ansor/AutoScheduler**, **MetaSchedule** — because each one removed a human bottleneck the previous left in, and the lineage is how you understand what MetaSchedule (the current system, and the engine under MLC-LLM's tuning) actually does.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. State why auto-tuning exists: the schedule space is too large and too target-specific to hand-navigate per shape.
2. Explain the **tuning loop** — space generation → cost model → on-device measurement → search → database — and which resource is the expensive one.
3. Distinguish the three generations: AutoTVM (**template-based**), Ansor (**template-free, auto-generated space**), MetaSchedule (**probabilistic schedule programs on TIR**, unifying both).
4. Run **`ms.tune_tir`** on a kernel and **`ms.tune_relax`** on a whole model, then apply the tuning database and build.
5. Read a **tuning curve** (best latency vs trials) and reason about the cost-model-vs-measurement tradeoff.
6. Stand up **distributed tuning over RPC** to measure on a real edge device from a host.

---

**1. Why search beats hand-tuning**

A single GPU matmul schedule has, easily, a dozen tunable decisions: tile sizes on `i`, `j`, `k` (each a multi-way split), the shared-memory staging factor, the vectorization width, the unroll depth, whether to use Tensor Cores. The legal combinations number in the **millions**. Most are slow. A handful are within a few percent of peak. A human cannot enumerate them; a human can barely guess a good corner.

But the space has structure a machine can exploit:

```text
the schedule space is huge       → you cannot try everything
but most of it is obviously bad  → a cheap model can rank candidates
and "fast" is target-specific    → only real hardware gives ground truth
and you tune once, run forever   → amortize search cost over deployment
```

So the strategy writes itself: **generate** candidates, **predict** with a cheap learned cost model, **measure** only the promising few on the actual device, **learn** from those measurements, repeat. The expensive resource is the on-device measurement — each one is a build + run, hundreds of milliseconds to seconds. The cost model exists precisely to spend those measurements wisely.

This is "the learning-based compiler." It is also why TVM kernels can, on odd shapes and non-vendor targets, *beat* hand-written vendor libraries: the library author tuned the common case once; the search tunes **your** case, on **your** chip.

---

</details>

## 2. 调优循环

每一代 TVM 自动调优都是同一个循环的变体。吃透这个循环；各代之间的差别只在于 *搜索空间如何生成*。

```text
   ┌──────────────────────────── TUNING LOOP (per task) ────────────────────────────┐
   │                                                                                 │
   │   ① SPACE GENERATION ──────► candidate schedules                                │
   │      (templates, or sketch rules,        │                                      │
   │       or PostOrderApply schedule rules)  ▼                                      │
   │                                  ② COST MODEL  (XGBoost / MLP)                   │
   │                                  predicts latency, ranks cheaply                │
   │                                          │  keep top-k                          │
   │                                          ▼                                      │
   │                                  ③ MEASURE on real hardware                      │
   │                                  build + run  (local  OR  RPC → device)         │
   │                                          │  true latency                        │
   │                                          ▼                                      │
   │                                  ④ UPDATE cost model + DATABASE                  │
   │                                          │                                      │
   │                                  ⑤ SEARCH proposes next batch                    │
   │                                  (evolutionary / replay)                        │
   │                                          └────────────► repeat until budget     │
   └─────────────────────────────────────────────────────────────────────────────────┘
                                            │  apply best record
                                            ▼
                              tuned schedule → codegen → fast kernel
```

有两点要记住：

* **数据库才是资产。** 调优会产出一个由 `(workload → best schedule record)` 构成的数据库。你交付并反复使用的正是这个数据库；你只调优一次，构建阶段只是查表取最优解。像对待构建产物一样对待它——做版本管理，缓存它。
* **trial 就是预算。** `max_trials_global`（端侧测量的总次数）是那个用调优时间换取 kernel 质量的旋钮。cost model 的全部职责就是让每次 trial 都物有所值。

---


<details>
<summary>English original</summary>

**2. The tuning loop**

Every generation of TVM auto-tuning is a variation on one loop. Learn the loop; the generations differ only in *how the space is generated*.

```text
   ┌──────────────────────────── TUNING LOOP (per task) ────────────────────────────┐
   │                                                                                 │
   │   ① SPACE GENERATION ──────► candidate schedules                                │
   │      (templates, or sketch rules,        │                                      │
   │       or PostOrderApply schedule rules)  ▼                                      │
   │                                  ② COST MODEL  (XGBoost / MLP)                   │
   │                                  predicts latency, ranks cheaply                │
   │                                          │  keep top-k                          │
   │                                          ▼                                      │
   │                                  ③ MEASURE on real hardware                      │
   │                                  build + run  (local  OR  RPC → device)         │
   │                                          │  true latency                        │
   │                                          ▼                                      │
   │                                  ④ UPDATE cost model + DATABASE                  │
   │                                          │                                      │
   │                                  ⑤ SEARCH proposes next batch                    │
   │                                  (evolutionary / replay)                        │
   │                                          └────────────► repeat until budget     │
   └─────────────────────────────────────────────────────────────────────────────────┘
                                            │  apply best record
                                            ▼
                              tuned schedule → codegen → fast kernel
```

Two things to hold onto:

* **The database is the asset.** Tuning produces a database of `(workload → best schedule record)`. That database is what you ship and re-apply; you tune once and the build step just looks up the winner. Treat it like a build artifact — version it, cache it.
* **Trials are the budget.** `max_trials_global` (total on-device measurements) is the knob that trades tuning time for kernel quality. The cost model's whole job is to make each trial count.

---

</details>

## 3. 三代系统，三个瓶颈被消除

这段历史不是冷知识 —— 每一代都删掉了一笔具体的人力开销，知道删掉的是哪一笔，就知道还有什么必须自己动手。

| | **AutoTVM**（2018） | **Ansor / AutoScheduler**（2020） | **MetaSchedule**（2022，当前） |
|---|---|---|---|
| 搜索空间 | **人工编写的模板**，带可调旋钮 | **自动生成**（sketch + annotation） | 基于 TIR 上的 schedule 规则**自动生成**；也接受自定义的概率化 schedule |
| 每个算子的人力投入 | 编写模板（高） | 无 | 无（规则内置） |
| IR 层级 | TE schedule | TE / loop state | 直接操作 **TensorIR** |
| 代价模型 | 基于特征的 XGBoost | 学习得到（XGBoost/MLP） | XGBoost / MLP，基于特征 |
| 搜索方式 | 旋钮调优（grid/genetic） | 在 sketch 上进化搜索 | 在 schedule trace 上进化搜索 |
| Tensorization（Tensor Core） | 手动 | 有限 | **一等公民**（可在搜索空间中 tensorize） |
| Relax / Unity 集成 | 无（Relay 时代） | 部分 | **有**（`tune_relax`） |
| 状态 | 遗留 | 遗留（在 Unity 中已被取代） | **当前系统** |

这条演进弧线，各用一句话概括：

* **AutoTVM** 证明了搜索这条路走得通 —— 但要求*你*为每个算子编写 schedule 模板，用 `cfg.define_split` / `cfg.define_knob` 声明可调轴。功能强大，但写模板是实打实的劳动，而且每来一个新算子就得重写一份。

```python
# AutoTVM flavor (legacy): you author the template AND its knobs
@autotvm.template("demo/matmul")
def matmul(N, L, M, dtype):
    # ... build a TE schedule ...
    cfg = autotvm.get_config()
    cfg.define_split("tile_x", x, num_outputs=2)   # the human declares what is tunable
    cfg.define_split("tile_y", y, num_outputs=2)
    cfg.define_knob("unroll", [0, 16, 64])
    # ... apply cfg choices to the schedule ...
```

* **Ansor** 把模板删掉了。它从计算定义*生成*搜索空间（sketch generation = 粗粒度结构，随后 random annotation = 分块大小等），并用进化变异 + 学习到的代价模型做搜索。论文给出的结果：Ansor 在所有场景下都追平或超过 AutoTVM，报告的最高加速约 9×，而且关键在于**零模板**。这是概念上的跃迁 —— 人不再描述*如何调度*，只提供*要算什么*。

* **MetaSchedule**（又名 **AutoTensorIR**）是第三代，也是今天在用的这一代。它把前两代统一为 **TensorIR 上的概率化 schedule 程序**：内置的 schedule *规则*（通过 `PostOrderApply` 自底向上应用）像 Ansor 那样自动生成搜索空间，但你也可以像 AutoTVM 那样插入自定义的概率化 schedule 函数 —— 同一套框架，自动化程度由你选。它直接作用于 TIR（因此能与第 2 讲中的一切组合），把 **tensorization 提升为一等公民**（可以把 Tensor Core schedule 放进搜索空间），并与 **Relax** 集成，从而能调优整个模型，而不只是一个 kernel。

在本课程中：**使用 MetaSchedule。** AutoTVM 和 Ansor 会在你读旧代码时出现；MetaSchedule 才是真正重要的引擎 —— 往下游看，它也是 MLC-LLM 产出快速 kernel 所依赖的一环。

---

## 4. 动手：用 `ms.tune_tir` 调优一个 kernel

拿第 2 讲里的矩阵乘 `PrimFunc`，让 MetaSchedule 替你调度。不用模板，不用手写分块大小 —— 你只提供计算和试跑预算。

```python
import tvm
from tvm import meta_schedule as ms
from tvm import tir

mod = matmul_te(1024, 1024, 1024)          # the TIR PrimFunc from Lecture 2
target = tvm.target.Target("nvidia/geforce-rtx-3090")   # or "llvm -mcpu=native", a Jetson, etc.

database = ms.tune_tir(
    mod=mod,
    target=target,
    work_dir="./ms_work",                  # logs + the tuning database land here
    max_trials_global=1000,                # total on-device measurements (the budget)
    num_trials_per_iter=64,                # batch size per search iteration
)

# Pull the best schedule MetaSchedule found and build it
sch = ms.tir_integration.compile_tir(database, mod, target)
sch.mod.show()                             # ← read the machine-found schedule!
rt_mod = tvm.build(sch.mod, target)
```

资深工程师在这里要保持两个习惯：

1. **读懂它找到的 schedule。** `sch.mod.show()` 打印出的正是第 2 讲里你手写的那种分块、绑定、缓存、可能还做了 tensorize 的 TIR —— 只不过分块大小是机器选的。你能*认出*这个结构，是因为你先手动调度过。这正是第 2 讲先讲的原因。
2. **保留 `work_dir`。** 该目录存放数据库。用同一个 `work_dir` 重跑会续跑；把它交付出去，队友无需重新调优就能构建出调好的 kernel。它是构建产物。

---


<details>
<summary>English original</summary>

**3. Three generations, three bottlenecks removed**

The history is not trivia — each generation deleted a specific human cost, and knowing which tells you what you still have to do yourself.

| | **AutoTVM** (2018) | **Ansor / AutoScheduler** (2020) | **MetaSchedule** (2022, current) |
|---|---|---|---|
| Search space | **human-written template** with tunable knobs | **auto-generated** (sketch + annotation) | **auto-generated** via schedule rules on TIR; also accepts custom probabilistic schedules |
| Human effort per op | write a template (high) | none | none (rules built-in) |
| IR level | TE schedules | TE / loop state | **TensorIR** directly |
| Cost model | XGBoost on features | learned (XGBoost/MLP) | XGBoost / MLP, feature-based |
| Search | knob tuning (grid/genetic) | evolutionary over sketches | evolutionary over schedule traces |
| Tensorization (Tensor Cores) | manual | limited | **first-class** (tensorize in the space) |
| Relax / Unity integration | no (Relay-era) | partial | **yes** (`tune_relax`) |
| Status | legacy | legacy (superseded in Unity) | **the current system** |

The arc in one sentence each:

* **AutoTVM** proved search works — but made *you* write a schedule template per operator, with `cfg.define_split` / `cfg.define_knob` declaring the tunable axes. Powerful, but the template is real labor, and you write a new one for every new op.

```python
# AutoTVM flavor (legacy): you author the template AND its knobs
@autotvm.template("demo/matmul")
def matmul(N, L, M, dtype):
    # ... build a TE schedule ...
    cfg = autotvm.get_config()
    cfg.define_split("tile_x", x, num_outputs=2)   # the human declares what is tunable
    cfg.define_split("tile_y", y, num_outputs=2)
    cfg.define_knob("unroll", [0, 16, 64])
    # ... apply cfg choices to the schedule ...
```

* **Ansor** deleted the template. It *generates* the search space from the compute definition (sketch generation = coarse structure, then random annotation = tile sizes etc.), and searches with evolutionary mutation + a learned cost model. The published result: Ansor matched or beat AutoTVM everywhere, with reported speedups up to ~9× and, crucially, **zero templates**. This is the conceptual leap — the human stops describing *how to schedule* and only provides *what to compute*.

* **MetaSchedule** (a.k.a. **AutoTensorIR**) is the third generation and the one you use today. It unifies the two prior worlds as **probabilistic schedule programs over TensorIR**: built-in schedule *rules* (applied bottom-up via `PostOrderApply`) auto-generate the space like Ansor, but you can also drop in custom probabilistic schedule functions like AutoTVM — same framework, your choice of automation level. It works directly on TIR (so it composes with everything in Lecture 2), makes **tensorization first-class** (it can put Tensor Core schedules in the search space), and integrates with **Relax** so you can tune a whole model, not just a kernel.

For this course: **we use MetaSchedule.** AutoTVM and Ansor appear when you read older code; MetaSchedule is the engine that matters — including, downstream, as part of how MLC-LLM produces fast kernels.

---

**4. Hands-on: tune a kernel with `ms.tune_tir`**

Take the matmul `PrimFunc` from Lecture 2 and let MetaSchedule schedule it for you. No template, no hand-written tile sizes — you provide the computation and a trial budget.

```python
import tvm
from tvm import meta_schedule as ms
from tvm import tir

mod = matmul_te(1024, 1024, 1024)          # the TIR PrimFunc from Lecture 2
target = tvm.target.Target("nvidia/geforce-rtx-3090")   # or "llvm -mcpu=native", a Jetson, etc.

database = ms.tune_tir(
    mod=mod,
    target=target,
    work_dir="./ms_work",                  # logs + the tuning database land here
    max_trials_global=1000,                # total on-device measurements (the budget)
    num_trials_per_iter=64,                # batch size per search iteration
)

# Pull the best schedule MetaSchedule found and build it
sch = ms.tir_integration.compile_tir(database, mod, target)
sch.mod.show()                             # ← read the machine-found schedule!
rt_mod = tvm.build(sch.mod, target)
```

Two habits a senior engineer keeps here:

1. **Read the schedule it found.** `sch.mod.show()` prints exactly the kind of tiled, bound, cached, possibly tensorized TIR you hand-wrote in Lecture 2 — except the machine chose the tile sizes. You can *recognize* the structure because you scheduled by hand first. This is why Lecture 2 came first.
2. **Keep `work_dir`.** That directory holds the database. Re-running with the same `work_dir` resumes; shipping it lets a teammate build the tuned kernel without re-tuning. It is a build artifact.

---

</details>

## 5. 实战：用 `ms.tune_relax` 调优整个模型

调优一个 kernel 只是演示。真正的工作流是调优**模型中的每一个 kernel**，让端到端网络变快。MetaSchedule 的 Relax 集成会把每个可调优的 TIR 函数抽取为一个 task，在共享预算下对所有 task 一起调优，再把胜出者应用回图中。

```python
from tvm import relax
from tvm import meta_schedule as ms

# mod, params: a Relax IRModule + weights, e.g. imported in Lecture 1 and lowered
database = ms.relax_integration.tune_relax(
    mod=mod,
    params=params,
    target=target,
    work_dir="./ms_model",
    max_trials_global=20000,               # spread across all kernels in the model
    # task scheduler decides how to split the budget across tasks (gradient-based by default)
)

# Build the model with the tuning database applied
ex = ms.relax_integration.compile_relax(database, mod, target, params)
vm = relax.VirtualMachine(ex, tvm.cuda(0))
out = vm["main"](x)
```

底层发生了什么：MetaSchedule 找出可调优的 TIR 函数（矩阵乘、卷积、attention kernel），把每一个当作一个 **task**，并让 **task scheduler** 把 20 000 次 trial 分配到它们之间——对主导 runtime 的 kernel 投入更多（占网络 40% 的矩阵乘，拿到的 trial 比占 2% 的归一化多）。结果是一个端到端优化过的模型，而不是一堆各自很快的 kernel。

> **预算直觉。** 单看一个 kernel，有用的调优往往是几百到几千次 trial。一个含几十个不同 kernel 的完整模型则需要数万次。调优一个模型可能跑几分钟到几小时，取决于预算以及每次测量有多快——这正是下一节重要的原因。

---

## 6. cost model 与实测的取舍（读懂调优曲线）

最有用的单项诊断工具是**调优曲线**：最佳实测延迟随已花费 trial 数变化的函数。

```text
  latency
    │•
    │ •                      every point is one on-device measurement.
    │  ••                    the curve drops fast early (easy wins),
    │    •••                 then flattens (diminishing returns).
    │       ••••
    │           •••••••           ← the knee: where extra trials stop paying
    │                  ••••••••••••••••••••
    └────────────────────────────────────────► trials
```

读懂它就是技能所在：

* **早期陡降** → cost model 很快找到明显更优的 schedule；默认空间不错。
* **拐点** → 该停下的地方。越过拐点再加 trial 只换来零点几个百分点。下一次运行时把 `max_trials_global` 设在拐点附近。
* **从一开始就平坦 / 噪声大** → 要么该 kernel 已接近最优，要么 cost model 排序失准（少见），要么你的测量有噪声（修法：锁频、让机器空载、提高 `number`/`repeat`）。

更深一层：cost model 让你探索*数千*个候选，而只*实测*数百个。没有它，你就得实测所有候选，调优要花好几天。有了它，搜索会把测量集中在模型不确定或过于乐观的地方。**trial 就是钱；cost model 决定你怎么把钱花好。** 当有人问「TVM 调优要多久」，诚实的回答是「trial 预算 × 每次 trial 的测量时间那么久——而预算由曲线拐点在哪决定。」

---


<details>
<summary>English original</summary>

**5. Hands-on: tune a whole model with `ms.tune_relax`**

Tuning one kernel is a demo. The real workflow is tuning **every kernel in a model** so the end-to-end network is fast. MetaSchedule's Relax integration extracts each tunable TIR function as a task, tunes across all of them under a shared budget, and applies the winners back into the graph.

```python
from tvm import relax
from tvm import meta_schedule as ms

# mod, params: a Relax IRModule + weights, e.g. imported in Lecture 1 and lowered
database = ms.relax_integration.tune_relax(
    mod=mod,
    params=params,
    target=target,
    work_dir="./ms_model",
    max_trials_global=20000,               # spread across all kernels in the model
    # task scheduler decides how to split the budget across tasks (gradient-based by default)
)

# Build the model with the tuning database applied
ex = ms.relax_integration.compile_relax(database, mod, target, params)
vm = relax.VirtualMachine(ex, tvm.cuda(0))
out = vm["main"](x)
```

What happened under the hood: MetaSchedule found the tunable TIR functions (the matmuls, convs, attention kernels), treated each as a **task**, and let a **task scheduler** allocate the 20 000 trials across them — spending more on the kernels that dominate runtime (a matmul that is 40% of the network gets more trials than a normalization that is 2%). The result is an end-to-end-optimized model, not a bag of individually fast kernels.

> **Budget intuition.** Per-kernel, useful tuning is often hundreds to low-thousands of trials. A whole model with dozens of distinct kernels wants tens of thousands. Tuning a model can run minutes to hours depending on budget and how fast each measurement is — which is exactly why the next section matters.

---

**6. The cost-model vs measurement tradeoff (read the tuning curve)**

The single most useful diagnostic is the **tuning curve**: best measured latency as a function of trials spent.

```text
  latency
    │•
    │ •                      every point is one on-device measurement.
    │  ••                    the curve drops fast early (easy wins),
    │    •••                 then flattens (diminishing returns).
    │       ••••
    │           •••••••           ← the knee: where extra trials stop paying
    │                  ••••••••••••••••••••
    └────────────────────────────────────────► trials
```

Reading it is the skill:

* **Steep early drop** → the cost model is finding obviously-better schedules quickly; the default space is good.
* **The knee** → where you stop. More trials past the knee buy fractions of a percent. Set `max_trials_global` near the knee for the next run.
* **Flat from the start / noisy** → either the kernel is already near-optimal, the cost model is mis-ranking (rare), or your measurements are noisy (fix: pin clocks, idle the machine, raise `number`/`repeat`).

The deeper point: the cost model lets you explore *thousands* of candidates while only *measuring* hundreds. Without it you would measure everything and tuning would take days. With it, the search concentrates measurements where the model is unsure or optimistic. **Trials are money; the cost model is how you spend them well.** When someone asks "how long does TVM tuning take," the honest answer is "as long as the trial budget × per-trial measurement time — and you choose the budget by where the curve knees."

---

</details>

## 7. 基于 RPC 的分布式调优——在运行它的设备上调优

这部分让自动调优成为*硬件*工程，而不仅是编译器工程：**你必须在将运行 kernel 的芯片上进行测量。** 在 RTX 3090 上调优的调度对于 Jetson Orin、ARM Cortex-A 或嵌入式 NPU 是错误的——缓存大小不同、核心数不同、内存带宽不同。成本模型可以重新训练，但真值来自目标芯片。

TVM 用 **RPC 系统** 解决这个问题：设备运行一个服务器，向 **tracker** 注册；主机运行搜索，并通过网络将每次测量分派到设备。

```text
   HOST (laptop / CI box)                 RPC TRACKER                 DEVICE (Jetson / ARM board)
   ┌────────────────────┐                ┌──────────┐                ┌───────────────────────┐
   │ MetaSchedule search │── request ───►│  :9190   │◄── register ───│ rpc_server  key=jetson │
   │ cost model          │               │  key map │                │ runs the candidate     │
   │ builds candidate    │── cross-      │          │── dispatch ───►│ kernel, returns timing │
   │                     │   compile     └──────────┘                └───────────────────────┘
   └────────────────────┘◄──────────────── measured latency ─────────────────────┘
```


在设备上，启动一个指向 tracker 的服务器：

```bash
# on the Jetson / ARM board:
python -m tvm.exec.rpc_server --tracker=HOST_IP:9190 --key=jetson
```


在主机上，用 **RPC runner** 进行调优，使每次测量都在板卡上执行：

```python
from tvm import meta_schedule as ms

runner = ms.runner.RPCRunner(
    ms.runner.RPCConfig(
        tracker_host="HOST_IP", tracker_port=9190,
        tracker_key="jetson", session_timeout_sec=120,
    ),
    # how many times to repeat each measurement on-device for a stable number
)

database = ms.tune_tir(
    mod=mod,
    target=tvm.target.Target("nvidia/jetson-agx-orin"),   # the device's real target
    runner=runner,                                         # ← measure on the board, not the host
    work_dir="./ms_jetson",
    max_trials_global=2000,
)
```


主机生成候选并对其进行成本建模；*板卡*运行它们并报告真值。这就是如何从 CI 机器为边缘设备群进行调优——以及一旦新加速器能运行 TVM RPC 服务器，同一工作流如何扩展到它。对于太小而无法托管 Python 的微控制器，第 5 讲中的 AOT/microTVM 路径将同样的思路带到裸机。

---

## 8. 测量它

任何调优工作的交付物是**点名 roofline（性能上界模型）的前后对比**：

| 构建 | 延迟 | GFLOP/s | 峰值百分比 | 备注 |
|---|---|---|---|---|
| 框架基线（cuBLAS / oneDNN） | — | — | — | 需要超越的基准 |
| TVM，未调优（默认调度） | — | — | — | 无需调优即可获得 |
| TVM，MetaSchedule（N 次试验） | — | — | — | 调优带来的收益 |

三个数字证明工作成果：**相比未调优的加速比**（调优是否有帮助？）、**相比厂商库的加速比或精度一致性**（它是否超越了基准，或在厂商未覆盖的 shape 上接近？），以及**达到拐点的试验次数**（代价是多少）。调优后的 kernel 性能是未调优的 0.8 倍，意味着测量设置有问题或预算太小——进行调查，不要交付。

---

## 9. 迷你实验：调优一个 kernel 和一个模型，读懂曲线

1. **Kernel：** 在预算 `{200, 500, 1000, 2000}` 次试验下 `ms.tune_tir` 你第 2 讲的矩阵乘。绘制调优曲线并标出拐点。将最佳结果与第 2 讲的手工调度以及 cuBLAS/oneDNN 进行比较。
2. **Model：** 导入一个小型 CNN 或 Transformer 块，在 `max_trials_global ∈ {2000, 10000}` 下 `ms.tune_relax` 它。记录端到端延迟，与未调优的 TVM 构建和框架基线进行比较。
3. **（Stretch）RPC：** 如果有边缘板，搭建一个 tracker + `rpc_server`，并*在设备上*重新调优 kernel。将设备上调优的调度的分块大小与桌面调优的分块大小进行比较——它们会不同，而差异就是教训。

交付物：两条调优曲线、来自 §8 的前后对比表，以及一段文字：拐点在哪里，TVM 是否在任何 shape 上超越了厂商库，以及（如果你做了 RPC）设备上调优的调度与桌面调优的调度有何不同。那就是一个 Level-4 测量产物。

---


<details>
<summary>English original</summary>

**7. Distributed tuning over RPC — tuning on the device that will run it**

Here is the part that makes auto-tuning *hardware* engineering, not just compiler engineering: **you must measure on the chip that will run the kernel.** A schedule tuned on an RTX 3090 is wrong for a Jetson Orin, an ARM Cortex-A, or an embedded NPU — different cache sizes, different core counts, different memory bandwidth. The cost model can be retrained, but ground truth comes from the target silicon.

TVM solves this with an **RPC system**: the device runs a server that registers with a **tracker**; the host runs the search and dispatches each measurement to the device over the network.

```text
   HOST (laptop / CI box)                 RPC TRACKER                 DEVICE (Jetson / ARM board)
   ┌────────────────────┐                ┌──────────┐                ┌───────────────────────┐
   │ MetaSchedule search │── request ───►│  :9190   │◄── register ───│ rpc_server  key=jetson │
   │ cost model          │               │  key map │                │ runs the candidate     │
   │ builds candidate    │── cross-      │          │── dispatch ───►│ kernel, returns timing │
   │                     │   compile     └──────────┘                └───────────────────────┘
   └────────────────────┘◄──────────────── measured latency ─────────────────────┘
```

On the device, start a server pointed at the tracker:

```bash
# on the Jetson / ARM board:
python -m tvm.exec.rpc_server --tracker=HOST_IP:9190 --key=jetson
```

On the host, tune with an **RPC runner** so every measurement executes on the board:

```python
from tvm import meta_schedule as ms

runner = ms.runner.RPCRunner(
    ms.runner.RPCConfig(
        tracker_host="HOST_IP", tracker_port=9190,
        tracker_key="jetson", session_timeout_sec=120,
    ),
    # how many times to repeat each measurement on-device for a stable number
)

database = ms.tune_tir(
    mod=mod,
    target=tvm.target.Target("nvidia/jetson-agx-orin"),   # the device's real target
    runner=runner,                                         # ← measure on the board, not the host
    work_dir="./ms_jetson",
    max_trials_global=2000,
)
```

The host generates and cost-models candidates; the *board* runs them and reports truth. This is how you tune for an edge fleet from a CI machine — and how the same workflow extends to a brand-new accelerator the moment it can run a TVM RPC server. For a microcontroller too small to host Python, the AOT/microTVM path in Lecture 5 carries the same idea down to bare metal.

---

**8. Measure it**

The deliverable for any tuning effort is a **before/after with the roofline named**:

| Build | Latency | GFLOP/s | % of peak | Notes |
|---|---|---|---|---|
| Framework baseline (cuBLAS / oneDNN) | — | — | — | the bar to beat |
| TVM, un-tuned (default schedule) | — | — | — | what you get free |
| TVM, MetaSchedule (N trials) | — | — | — | what tuning bought |

Three numbers prove the work: the **speedup over un-tuned** (did tuning help?), the **speedup-or-parity vs the vendor library** (did it beat the bar, or get close on a shape the vendor didn't cover?), and the **trials to the knee** (what it cost). A tuned kernel that is 0.8× the un-tuned one means a broken measurement setup or too small a budget — investigate, do not ship.

---

**9. Mini-lab: tune a kernel and a model, read the curves**

1. **Kernel:** `ms.tune_tir` your Lecture-2 matmul at budgets `{200, 500, 1000, 2000}` trials. Plot the tuning curve and mark the knee. Compare the best to your hand-schedule from Lecture 2 and to cuBLAS/oneDNN.
2. **Model:** import a small CNN or transformer block, `ms.tune_relax` it at `max_trials_global ∈ {2000, 10000}`. Record end-to-end latency vs the un-tuned TVM build and the framework baseline.
3. **(Stretch) RPC:** if you have an edge board, stand up a tracker + `rpc_server` and re-tune the kernel *on the device*. Compare the on-device-tuned schedule's tile sizes to the desktop-tuned ones — they will differ, and the difference is the lesson.

Deliverable: the two tuning curves, the before/after table from §8, and a paragraph: where was the knee, did TVM beat the vendor library on any shape, and (if you did RPC) how did the device-tuned schedule differ from the desktop one. That is a Level-4 measurement artifact.

---

</details>

## 关键要点

- 按 shape × dtype × target 手工调度是无穷无尽的工作。TVM 的根本思路是把它变成一个**搜索**：生成 → 成本模型 → 在设备上测量 → 学习 → 重复。
- **调优循环**在各代之间是相同的；差异只在于搜索空间如何生成。**数据库**是可交付的资产；**trials**（在设备上的测量）是预算。
- **AutoTVM**（基于模板，由你编写可调旋钮）→ **Ansor**（无模板，自动生成搜索空间，相比 AutoTVM 最高约 9×，零模板）→ **MetaSchedule**（TensorIR 上的概率化调度程序，统一了两者，tensorization 是一等公民，与 Relax 集成）。用 **MetaSchedule**。
- `ms.tune_tir` 调优单个 kernel；`ms.tune_relax` 调优整个模型，由任务调度器把 trial 预算分配给占据运行时间主导地位的 kernel。
- **成本模型**让你在只测量几百个候选的情况下探索数千个候选——读**调优曲线**，在拐点处停止。
- **在目标芯片上调优。** RPC（server + tracker + RPCRunner）从主机在真实设备上测量候选——正是这一工作流让自动调优成为真正的硬件工程，并能扩展到边缘设备集群和新的加速器。

---

## 参考文献

- MetaSchedule 教程（“Search-Based Auto-Tuning”）：[https://tvm.apache.org/docs/deep_dive/tensor_ir/tutorials/meta_schedule.html](https://tvm.apache.org/docs/deep_dive/tensor_ir/tutorials/meta_schedule.html)
- Shao et al., “Tensor Program Optimization with Probabilistic Programs”（MetaSchedule），NeurIPS 2022：[https://arxiv.org/abs/2205.13603](https://arxiv.org/abs/2205.13603)
- Zheng et al., “Ansor: Generating High-Performance Tensor Programs for Deep Learning”，OSDI 2020：[https://www.usenix.org/conference/osdi20/presentation/zheng](https://www.usenix.org/conference/osdi20/presentation/zheng)
- Chen et al., “Learning to Optimize Tensor Programs”（AutoTVM），NeurIPS 2018：[https://arxiv.org/abs/1805.08166](https://arxiv.org/abs/1805.08166)
- TVM RPC / 跨设备调优文档：[https://tvm.apache.org/docs/how_to/tune_with_autotvm/](https://tvm.apache.org/docs/how_to/tune_with_autotvm/)
- *TVM Deep Dives* —— [Lecture 02](Lecture-02.md)（被搜索的空间）与 [Lecture 05](Lecture-05.md)（dlight：*无需*调优的快速调度，以及各自何时是正确选择）。

---

*下一讲：[Lecture 04 — Relax 深入：动态 shape、融合与 BYOC](Lecture-04.md)*


<details>
<summary>English original</summary>

**Key takeaways**

- Hand-scheduling per shape × dtype × target is infinite work. TVM's foundational idea is to make it a **search**: generate → cost-model → measure on device → learn → repeat.
- The **tuning loop** is the same across generations; they differ only in how the space is generated. The **database** is the shippable asset; **trials** (on-device measurements) are the budget.
- **AutoTVM** (template-based, you write the knobs) → **Ansor** (template-free, auto-generated space, ~up to 9× over AutoTVM, zero templates) → **MetaSchedule** (probabilistic schedule programs on TensorIR, unifies both, tensorization first-class, Relax-integrated). Use **MetaSchedule**.
- `ms.tune_tir` tunes a kernel; `ms.tune_relax` tunes a whole model, with a task scheduler splitting the trial budget toward the kernels that dominate runtime.
- The **cost model** lets you explore thousands of candidates while measuring only hundreds — read the **tuning curve**, stop at the knee.
- **Tune on the target silicon.** RPC (server + tracker + RPCRunner) measures candidates on the real device from a host — the workflow that makes auto-tuning genuine hardware engineering and scales to edge fleets and new accelerators.

---

**References**

- MetaSchedule tutorial ("Search-Based Auto-Tuning"): [https://tvm.apache.org/docs/deep_dive/tensor_ir/tutorials/meta_schedule.html](https://tvm.apache.org/docs/deep_dive/tensor_ir/tutorials/meta_schedule.html)
- Shao et al., "Tensor Program Optimization with Probabilistic Programs" (MetaSchedule), NeurIPS 2022: [https://arxiv.org/abs/2205.13603](https://arxiv.org/abs/2205.13603)
- Zheng et al., "Ansor: Generating High-Performance Tensor Programs for Deep Learning," OSDI 2020: [https://www.usenix.org/conference/osdi20/presentation/zheng](https://www.usenix.org/conference/osdi20/presentation/zheng)
- Chen et al., "Learning to Optimize Tensor Programs" (AutoTVM), NeurIPS 2018: [https://arxiv.org/abs/1805.08166](https://arxiv.org/abs/1805.08166)
- TVM RPC / cross-device tuning docs: [https://tvm.apache.org/docs/how_to/tune_with_autotvm/](https://tvm.apache.org/docs/how_to/tune_with_autotvm/)
- *TVM Deep Dives* — [Lecture 02](Lecture-02.md) (the space being searched) and [Lecture 05](Lecture-05.md) (dlight: fast schedules *without* tuning, and where each is the right choice).

---

*Next: [Lecture 04 — Relax in depth: dynamic shapes, fusion, and BYOC](Lecture-04.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/TVM Deep Dives/Lecture-03.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/TVM%20Deep%20Dives/Lecture-03.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
