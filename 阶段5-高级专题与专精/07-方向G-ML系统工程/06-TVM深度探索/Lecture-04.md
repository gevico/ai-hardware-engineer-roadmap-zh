---
title: Lecture 04 - Relax 深入：动态形状、算子融合与自带代码生成（BYOC）
description: Lecture 04 - Relax 深入：动态形状、算子融合与自带代码生成（BYOC）
---

# Lecture 04 - Relax 深入：动态形状、算子融合与自带代码生成（BYOC）

**合集：** [TVM Deep Dives](README.md) | **上一讲：** [← Lecture 03](Lecture-03.md) | **下一讲：** [Lecture 05](Lecture-05.md)

---

第 2 讲和第 3 讲*深入 kernel 内部* —— 一个算子，经过调度和调优。本讲把视角拉回到**图**层面，因为真实部署中最大的两个收益并不在任何单个 kernel 内部：

* **算子融合** —— 将一串算子折叠进一个 kernel，消除了它们*之间*的 DRAM 往返和启动开销。在带宽受限的模型上，这往往比调优任何单个算子收益更大。
* **自带代码生成（BYOC）** —— 将图的一部分交给外部库或*自定义加速器*的编译器，而 TVM 处理其余部分。这是新芯片获得软件栈的机制。

两者都位于 **Relax** 层，并且都依赖于 Relax 的定义性能力：**一等符号形状**，它让一个编译产物能够服务可变批大小或不断增长的大语言模型序列。依次讨论这三者，再将其组合起来 —— 因为在带融合的动态形状图上做 BYOC，具体而言，就是为现代模型启动加速器的样子。

---

## 学习目标

本讲结束时，你应该能够：

1. 深入阅读 Relax 函数：dataflow 块、绑定、`call_tir`、struct info、纯与不纯。
2. 编写并推理**符号/动态形状**（`R.Tensor(("n", 4096), ...)`），并解释为什么大语言模型需要它们。
3. 运行图级优化 pass —— **`FuseOps` → `FuseTIR`**、legalize、layout transform、memory planning —— 并解释每个带来什么收益。
4. 从 **roofline（性能上界模型） / DRAM 流量**角度解释融合为何重要，包括让量化大语言模型变快的 dequant-matmul-epilogue 融合。
5. 执行 **BYOC** 流程：`partition_for_<backend>` → `MergeCompositeFunctions` → `RunCodegen`，将子图卸载到 CUTLASS/TensorRT。
6. 描述芯片厂商为了让其加速器拥有 BYOC 后端必须实现什么 —— L4 加速器软件的工作。

---

## 1. 细看 Relax

一个 Relax 函数是有类型的数据流程序。下面这个标注了关键特性：

```python
from tvm.script import relax as R, tir as T

@R.function
def main(x: R.Tensor(("n", 4096), "float16"),          # ← symbolic dim "n"
         w1: R.Tensor((4096, 11008), "float16"),
         w2: R.Tensor((11008, 4096), "float16")) -> R.Tensor(("n", 4096), "float16"):
    n = T.int64()                                       # the symbolic shape variable
    with R.dataflow():                                  # a pure, optimizer-owned region
        lv0  = R.matmul(x, w1)                          # (n, 11008)
        lv1  = R.nn.silu(lv0)                           # elementwise
        lv2  = R.matmul(lv1, w2)                        # (n, 4096)
        out  = R.add(x, lv2)                            # residual
        R.output(out)
    return out
```

解读其结构：

* **Struct info** 是 Relax 的类型系统：`R.Tensor(shape, dtype)` 将形状和 dtype *两者*都作为类型的一部分携带。编译器尽可能静态地推理形状 —— 这正是融合和内存规划得以实现的原因。
* **Dataflow 块**（`R.dataflow()`）标记一个**纯的、无副作用**区域。在其内部，优化器可以自由重排、融合、消除和重写绑定。`R.output(...)` 行指明哪些值逃逸出块。有副作用的操作（修改 KV-cache、I/O）位于 dataflow 块*之外*，这就是 Relax 如何清晰地将“可优化的数学”与“有状态的效果”分开。
* **绑定**（`lv0 = ...`）是单赋值的。图是由这些构成的 DAG。
* **`call_tir`**（来自第 1 讲）在高层算子被 lowering 后出现 —— 图向下延伸到已调度的 TIR。

这种清晰的分离 —— 有类型的形状、纯 dataflow 区域、显式效果 —— 使 Relax 可以被激进地优化，而原始的命令式 trace 则不能。

---


<details>
<summary>English original</summary>

**Lecture 04 - Relax in Depth: Dynamic Shapes, Operator Fusion, and Bring Your Own Codegen (BYOC)**

**Collection:** [TVM Deep Dives](README.md) | **Previous:** [← Lecture 03](Lecture-03.md) | **Next:** [Lecture 05](Lecture-05.md)

---

Lectures 2 and 3 lived *inside a kernel* — one operator, scheduled and tuned. This lecture zooms back out to the **graph**, because two of the largest wins in a real deployment are not inside any single kernel:

* **Operator fusion** — collapsing a chain of ops into one kernel kills the DRAM round-trips and launch overhead *between* them. On memory-bound models this is often a bigger win than tuning any individual op.
* **Bring Your Own Codegen (BYOC)** — handing parts of the graph to an external library or a *custom accelerator's* compiler, while TVM handles the rest. This is the mechanism by which a new chip gets a software stack.

Both live at the **Relax** level, and both depend on Relax's defining capability: **first-class symbolic shapes**, the thing that lets one compiled artifact serve a variable batch size or a growing LLM sequence. We take all three in turn, then put them together — because BYOC on a dynamic-shape graph with fusion is, concretely, what bringing up an accelerator for modern models looks like.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Read a Relax function in depth: dataflow blocks, bindings, `call_tir`, struct info, pure vs impure.
2. Write and reason about **symbolic / dynamic shapes** (`R.Tensor(("n", 4096), ...)`), and explain why LLMs require them.
3. Run the graph-level optimization passes — **`FuseOps` → `FuseTIR`**, legalize, layout transform, memory planning — and explain what each buys.
4. Explain why fusion matters in **roofline / DRAM-traffic** terms, including the dequant-matmul-epilogue fusion that makes quantized LLMs fast.
5. Execute the **BYOC** flow: `partition_for_<backend>` → `MergeCompositeFunctions` → `RunCodegen`, offloading a subgraph to CUTLASS/TensorRT.
6. Describe what a chip vendor must implement to give their accelerator a BYOC backend — the L4 accelerator-software job.

---

**1. Relax up close**

A Relax function is a typed dataflow program. Here is one with the features that matter named:

```python
from tvm.script import relax as R, tir as T

@R.function
def main(x: R.Tensor(("n", 4096), "float16"),          # ← symbolic dim "n"
         w1: R.Tensor((4096, 11008), "float16"),
         w2: R.Tensor((11008, 4096), "float16")) -> R.Tensor(("n", 4096), "float16"):
    n = T.int64()                                       # the symbolic shape variable
    with R.dataflow():                                  # a pure, optimizer-owned region
        lv0  = R.matmul(x, w1)                          # (n, 11008)
        lv1  = R.nn.silu(lv0)                           # elementwise
        lv2  = R.matmul(lv1, w2)                        # (n, 4096)
        out  = R.add(x, lv2)                            # residual
        R.output(out)
    return out
```

Read the structure:

* **Struct info** is Relax's type system: `R.Tensor(shape, dtype)` carries *both* shape and dtype as part of the type. The compiler reasons about shapes statically wherever it can — that is what makes fusion and memory planning possible.
* **Dataflow block** (`R.dataflow()`) marks a **pure, side-effect-free** region. Inside it, the optimizer may reorder, fuse, eliminate, and rewrite bindings freely. The `R.output(...)` line names which values escape the block. Operations with side effects (mutating a KV-cache, I/O) live *outside* dataflow blocks, which is how Relax cleanly separates "optimizable math" from "stateful effects."
* **Bindings** (`lv0 = ...`) are single-assignment. The graph is a DAG of these.
* **`call_tir`** (from Lecture 1) appears once high-level ops are lowered — the graph reaching down into scheduled TIR.

That clean separation — typed shapes, a pure dataflow region, explicit effects — is what makes Relax aggressively optimizable where a raw imperative trace is not.

---

</details>

## 2. 符号化 shape：LLM 形态的问题

视觉模型通常只有一个固定输入 shape；你可以针对 `(1, 3, 224, 224)` 编译一次就完事。**LLM 做不到**。在 decode（逐 token 生成阶段）过程中序列长度逐 token 增长；批大小随请求而变；KV-cache 长度每一步都不同。为每个序列长度编译一个单独的二进制文件是荒谬的。

Relax 的答案是**符号化 shape 变量**。上面 `R.Tensor(("n", 4096), ...)` 中的 `"n"` 不是常量——它是在 runtime 解析的变量。Relax VM 在程序运行时跟踪 `n` 的具体值，并把它贯穿到每一次 shape 计算中。

```text
   static-shape compiler            symbolic-shape compiler (Relax)
   ───────────────────              ───────────────────────────────
   compile for n = 1                compile ONCE for n = "n"
   compile for n = 2                VM binds n = 7 this call,
   compile for n = 4                          n = 8 next call,
   ... (impossible for LLMs)                  n = 512 for a prefill
```

有两个机制支撑它：

* **Shape 表达式**——shape 可以是对符号变量做算术运算（`(n, n + 1, 4096)`），因此编译器可以推理，例如长度为 `n` 的 KV-cache 变成 `n + 1`。
* **`R.match_cast`**——在 shape 动态可知的边界处（如 `unique` 这类数据相关的 op，或外部调用），`match_cast` 会把一个不透明 shape 细化为带具名符号变量的 shape，使下游代码能再次被优化。

这正是 MLC-LLM（第 5 讲）构建在 Relax 而非 Relay 上的*根本*原因：变长序列、KV-cache 和 ragged 批都是动态 shape 问题，而 Relax 把动态 shape 做成了 IR 的一等公民，而不是事后补丁。当你听到「TVM Unity 让 LLM 跑起来了」，他们指的就是这个具体能力。

---

## 3. 图级 pass：免费收益所在之处

在任何 kernel 运行之前，一连串 **graph pass** 会重写 Relax 模块。默认的 `relax.get_pipeline("zero")` 打包了一个合理的顺序；在生产环境中你自行组装。真正起作用的有这几个：

| Pass | 做什么 | 为什么重要 |
|---|---|---|
| `LegalizeOps` | 高层 op → `call_tir` 下沉到 TIR `PrimFunc` | 打通图与 kernel 层（第 1 讲） |
| `AnnotateTIROpPattern` | 给每个 TIR func 打标签：injective / reduction / 等 | 告诉 fuser 哪些合并是安全的 |
| **`FuseOps`** | 把 op 链分组成融合后的 Relax 子函数 | 融合的*决策* |
| **`FuseTIR`** | 把一个融合组的 TIR 合并成**一个** `PrimFunc` | 融合的*实现*——一个 kernel |
| `ConvertLayout` / layout transform | 重写 NCHW↔NHWC↔blocked | 匹配目标所需的 layout |
| `FoldConstant` | 在编译期求值常量子图 | 折叠 scale，折叠权重的 reshape |
| `StaticPlanBlockMemory` | 规划并复用中间 buffer | 降低峰值内存，复用分配 |
| `DeadCodeElimination` / canonicalize | 剪枝、归一化 | 为后续 pass 清理图 |

把它们作为流水线来应用：

```python
from tvm import relax
seq = tvm.transform.Sequential([
    relax.transform.LegalizeOps(),
    relax.transform.AnnotateTIROpPattern(),
    relax.transform.FuseOps(),
    relax.transform.FuseTIR(),
    relax.transform.StaticPlanBlockMemory(),
    relax.transform.FoldConstant(),
])
mod = seq(mod)
mod.show()        # count the PrimFuncs before vs after — fusion collapsed several into one
```

---


<details>
<summary>English original</summary>

**2. Symbolic shapes: the LLM-shaped problem**

A vision model often has one fixed input shape; you can compile for `(1, 3, 224, 224)` and be done. An **LLM cannot**. The sequence length grows token by token during decode; the batch size varies per request; the KV-cache length is different every step. Compiling a separate binary per sequence length is absurd.

Relax's answer is the **symbolic shape variable**. The `"n"` in `R.Tensor(("n", 4096), ...)` above is not a constant — it is a variable resolved at runtime. The Relax VM tracks the concrete value of `n` as the program runs and threads it through every shape computation.

```text
   static-shape compiler            symbolic-shape compiler (Relax)
   ───────────────────              ───────────────────────────────
   compile for n = 1                compile ONCE for n = "n"
   compile for n = 2                VM binds n = 7 this call,
   compile for n = 4                          n = 8 next call,
   ... (impossible for LLMs)                  n = 512 for a prefill
```

Two mechanisms support it:

* **Shape expressions** — shapes can be arithmetic over symbolic vars (`(n, n + 1, 4096)`), so the compiler reasons about, e.g., a KV-cache of length `n` becoming `n + 1`.
* **`R.match_cast`** — at a boundary where a shape is dynamically known (a data-dependent op like `unique`, or an external call), `match_cast` refines an opaque shape into one with named symbolic vars so downstream code can be optimized again.

This is *the* reason MLC-LLM (Lecture 5) is built on Relax and not Relay: variable-length sequences, KV-caches, and ragged batches are dynamic-shape problems, and Relax made dynamic shapes a first-class citizen of the IR rather than an afterthought. When you hear "TVM Unity made LLMs work," this is the concrete capability they mean.

---

**3. Graph-level passes: where the free wins live**

Before any kernel runs, a sequence of **graph passes** rewrites the Relax module. The default `relax.get_pipeline("zero")` bundles a sane order; in production you assemble your own. The ones that move the needle:

| Pass | What it does | Why it matters |
|---|---|---|
| `LegalizeOps` | high-level op → `call_tir` into a TIR `PrimFunc` | bridges graph to kernel level (Lecture 1) |
| `AnnotateTIROpPattern` | tags each TIR func: injective / reduction / etc. | tells the fuser what is safe to merge |
| **`FuseOps`** | groups op chains into fused Relax sub-functions | the fusion *decision* |
| **`FuseTIR`** | merges a fused group's TIR into **one** `PrimFunc` | the fusion *realization* — one kernel |
| `ConvertLayout` / layout transform | rewrite NCHW↔NHWC↔blocked | match the layout the target wants |
| `FoldConstant` | evaluate constant subgraphs at compile time | fold scales, fold reshapes of weights |
| `StaticPlanBlockMemory` | plan & reuse intermediate buffers | shrink peak memory, reuse allocations |
| `DeadCodeElimination` / canonicalize | prune, normalize | clean graph for later passes |

You apply them as a pipeline:

```python
from tvm import relax
seq = tvm.transform.Sequential([
    relax.transform.LegalizeOps(),
    relax.transform.AnnotateTIROpPattern(),
    relax.transform.FuseOps(),
    relax.transform.FuseTIR(),
    relax.transform.StaticPlanBlockMemory(),
    relax.transform.FoldConstant(),
])
mod = seq(mod)
mod.show()        # count the PrimFuncs before vs after — fusion collapsed several into one
```

---

</details>

## 4. 为什么融合往往是单项收益最大的优化

以 §1 的 SiLU 门控 FFN 为例：`matmul → silu → matmul → add`。不融合时，它是四个 kernel，每两个之间中间张量都要**写入 DRAM 再读回**：

```text
   UNFUSED                                     FUSED (epilogue into the matmul)
   matmul ─► [write 11008·n to DRAM]           matmul ─┐
   silu   ─► [read it, write it back]                  ├─ silu + add done in the
   matmul ─► [read it, write 4096·n]                   │  matmul's epilogue, in registers
   add    ─► [read, write]                      matmul ─┘  → intermediates never touch DRAM
   = 4 launches, several DRAM round-trips      = fewer launches, DRAM traffic slashed
```

对于**带宽受限**的模型（decode（逐 token 生成阶段）阶段的 LLM 无疑属于这一类——参见 Edge LLM 与 AI Inference Engineer 课程），逐元素算子（`silu`、`add`、norm、激活函数）若拆成独立 kernel，在 FLOPs 上几乎不花成本，在 DRAM 流量上却要付出全部代价。把它们融合进矩阵乘的 epilogue，意味着中间结果**从不离开寄存器**。用 roofline（性能上界模型）来解读：融合通过*删除*中间结果的内存流量抬高算术强度，让融合后的算子向右朝计算上界移动。

对 2026 年的 LLM 来说，最重要的融合模式是**反量化-矩阵乘-epilogue**。4-bit 权重在进入矩阵乘前必须先反量化成 fp16；若把它做成单独的 pass，就等于把全精度权重写进 DRAM——彻底毁掉 4-bit 存储的全部意义。所以 MLC-LLM 的流水线（第 5 讲）把**反量化 + 矩阵乘 + bias/激活**融合进单个 kernel（`FuseDequantizeMatmulEwise`）：权重*在寄存器中即时*反量化，立即被矩阵乘消费，从不以全精度实体化。这一处融合，正是 4-bit 的 7B 模型能在笔记本 GPU 上跑得快的重要原因。

这就是为什么资深工程师做 profile 时看的是 kernel *数量*与 *DRAM 流量*，而不只是单 kernel 的 GFLOP/s。十个调得完美、本应合成一个融合 kernel 的 kernel，跑出来的模型比一个调得尚可的融合 kernel 更慢。

---

## 5. BYOC：把图交给别人的编译器

有时最好的 kernel 并不是 TVM 该生成的那个。NVIDIA 的 CUTLASS 有久经考验的融合 attention 与 GEMM kernel；TensorRT 有多年的 tactic 调优；某款自研 NPU 有一条 TVM 一无所知的指令。**Bring Your Own Codegen** 让 TVM *切分*图，把匹配上的部分交给外部 codegen，其余部分继续自己生成代码——最后拼成一个可运行的模块。

流程分三步：

```text
   full Relax graph
   ┌──────────────────────────────────────────────┐
   │ conv → bn → relu → matmul → softmax → add ... │
   └──────────────────────────────────────────────┘
        │  ① partition_for_<backend>   (pattern-match subgraphs the backend supports)
        ▼
   ┌────────────────────────┬─────────────────────┐
   │ composite fn → CUTLASS │  rest → TVM codegen  │
   │  matmul (+epilogue)    │  softmax, odd ops    │
   └────────────────────────┴─────────────────────┘
        │  ② MergeCompositeFunctions   (group adjacent offloadable regions)
        │  ③ RunCodegen                (invoke the external compiler per region)
        ▼
   ONE runtime module: external kernels + TVM kernels, glued by the Relax VM
```


在代码层面，高层便捷路径（此处展示 CUTLASS；TensorRT、cuDNN、DNNL/oneDNN 同理）：

```python
from tvm import relax
from tvm.relax.backend.contrib.cutlass import partition_for_cutlass

# ① tag every subgraph that matches a CUTLASS pattern as an offloadable composite function
mod = partition_for_cutlass(mod)

# ②③ merge adjacent offload regions, then run the external codegen on each
mod = relax.transform.RunCodegen()(mod)

# build as usual — TVM generates the rest, the VM links the external kernels in
ex = relax.build(mod, target="cuda")
vm = relax.VirtualMachine(ex, tvm.cuda(0))
```


便捷函数之下是**通用**机制，当你要接一个 TVM 尚未自带的后端时就用它：

```python
from tvm.relax.dpl.pattern import is_op, wildcard

# define the patterns your backend can handle (e.g. matmul + bias + relu)
pat = is_op("relax.matmul")(wildcard(), wildcard())
mod = relax.transform.FuseOpsByPattern(
    [("my_npu.matmul_bias_relu", pat)],
    annotate_codegen=True,            # mark the fused fn for an external codegen named "my_npu"
)(mod)
mod = relax.transform.MergeCompositeFunctions()(mod)
mod = relax.transform.RunCodegen()(mod)   # dispatches "my_npu" regions to your registered codegen
```


后端匹配不上的部分，仍是普通的 TVM 生成 kernel。**图是被切分，而不是被交出。** 这是关键性质：你不必支持*全部*算子集才有用——把自己擅长的部分卸载出去，让 TVM 兜住长尾（并用 MetaSchedule 调优）。

---


<details>
<summary>English original</summary>

**4. Why fusion is often the biggest single win**

Take the SiLU-gated FFN from §1: `matmul → silu → matmul → add`. Unfused, that is four kernels, and between each one the intermediate tensor is **written to DRAM and read back**:

```text
   UNFUSED                                     FUSED (epilogue into the matmul)
   matmul ─► [write 11008·n to DRAM]           matmul ─┐
   silu   ─► [read it, write it back]                  ├─ silu + add done in the
   matmul ─► [read it, write 4096·n]                   │  matmul's epilogue, in registers
   add    ─► [read, write]                      matmul ─┘  → intermediates never touch DRAM
   = 4 launches, several DRAM round-trips      = fewer launches, DRAM traffic slashed
```

For a **memory-bound** model (which decode-phase LLMs emphatically are — see the Edge LLM and AI Inference Engineer courses), the elementwise ops (`silu`, `add`, norms, activations) cost almost nothing in FLOPs but everything in DRAM traffic if they are separate kernels. Fusing them into a matmul's epilogue means the intermediate **never leaves registers**. The roofline reading: fusion raises arithmetic intensity by *deleting* the memory traffic of the intermediates, moving the fused op rightward toward the compute roof.

The most important fused pattern for 2026 LLMs is **dequantize-matmul-epilogue**. A 4-bit weight has to be dequantized to fp16 before the matmul; doing that as a separate pass would write the full-precision weights to DRAM — defeating the entire point of 4-bit storage. So MLC-LLM's pipeline (Lecture 5) fuses **dequant + matmul + bias/activation** into a single kernel (`FuseDequantizeMatmulEwise`): the weights are dequantized *in registers, on the fly*, consumed immediately by the matmul, and never materialized in full precision. That one fusion is a large part of why a 7B model in 4-bit runs fast on a laptop GPU.

This is why a senior engineer profiles for kernel *count* and *DRAM traffic*, not just per-kernel GFLOP/s. Ten perfectly-tuned kernels that should have been one fused kernel is a slower model than one decently-tuned fused kernel.

---

**5. BYOC: handing the graph to someone else's compiler**

Sometimes the best kernel is not one TVM should generate. NVIDIA's CUTLASS has battle-hardened fused-attention and GEMM kernels; TensorRT has years of tactic tuning; a custom NPU has an instruction TVM knows nothing about. **Bring Your Own Codegen** lets TVM *partition* the graph, hand the matching parts to an external codegen, and keep generating code for the rest — then stitch them into one runnable module.

The flow has three moves:

```text
   full Relax graph
   ┌──────────────────────────────────────────────┐
   │ conv → bn → relu → matmul → softmax → add ... │
   └──────────────────────────────────────────────┘
        │  ① partition_for_<backend>   (pattern-match subgraphs the backend supports)
        ▼
   ┌────────────────────────┬─────────────────────┐
   │ composite fn → CUTLASS │  rest → TVM codegen  │
   │  matmul (+epilogue)    │  softmax, odd ops    │
   └────────────────────────┴─────────────────────┘
        │  ② MergeCompositeFunctions   (group adjacent offloadable regions)
        │  ③ RunCodegen                (invoke the external compiler per region)
        ▼
   ONE runtime module: external kernels + TVM kernels, glued by the Relax VM
```

In code, the high-level convenience path (CUTLASS shown; TensorRT, cuDNN, DNNL/oneDNN are analogous):

```python
from tvm import relax
from tvm.relax.backend.contrib.cutlass import partition_for_cutlass

# ① tag every subgraph that matches a CUTLASS pattern as an offloadable composite function
mod = partition_for_cutlass(mod)

# ②③ merge adjacent offload regions, then run the external codegen on each
mod = relax.transform.RunCodegen()(mod)

# build as usual — TVM generates the rest, the VM links the external kernels in
ex = relax.build(mod, target="cuda")
vm = relax.VirtualMachine(ex, tvm.cuda(0))
```

Under the convenience function is the **general** mechanism, which is what you use for a backend TVM doesn't already ship:

```python
from tvm.relax.dpl.pattern import is_op, wildcard

# define the patterns your backend can handle (e.g. matmul + bias + relu)
pat = is_op("relax.matmul")(wildcard(), wildcard())
mod = relax.transform.FuseOpsByPattern(
    [("my_npu.matmul_bias_relu", pat)],
    annotate_codegen=True,            # mark the fused fn for an external codegen named "my_npu"
)(mod)
mod = relax.transform.MergeCompositeFunctions()(mod)
mod = relax.transform.RunCodegen()(mod)   # dispatches "my_npu" regions to your registered codegen
```

Anything the backend can't match stays as ordinary TVM-generated kernels. **The graph is split, not surrendered.** That is the crucial property: you never have to support the *whole* op set to be useful — you offload what you do well and let TVM cover the long tail (and tune it with MetaSchedule).

---

</details>

## 6. BYOC 是新加速器获得软件栈的方式

这部分让第 4 讲成为一节*硬件*课，也是加速器软件工程师的日常工作。

芯片公司造出一颗快 NPU。它不想写——也跟不上——面向 PyTorch、ONNX、JAX 以及每一种模型架构的前端。有了 BYOC，它就不必如此。它只需实现**一次**：

1. 一张**模式表**：NPU 的 kernel 覆盖的算子模式（它的 GEMM、它的 conv、它的 attention、它的激活函数集合）。
2. 一个**代码生成**：给定一个被卸载的子图，生成对 NPU runtime / 驱动（或它自己的编译器）的调用。
3. 一个 **runtime 模块**：在设备上加载并执行这些 kernel，其余部分与 TVM 的 runtime 互操作。

作为回报，它**免费**得到任何 TVM 前端能导入的模型。TVM 负责导入、动态 shape、留在 host/GPU 上那部分的融合、内存规划以及胶水代码；NPU 负责自己擅长的算子。同一套机制也正是 TVM 自带的开放加速器 **VTA**（Versatile Tensor Accelerator）接入的方式，以及 `tensorize`（第 2 讲）向下扩展到自定义指令的方式。

```text
   vendor implements ONCE:                  gets for free:
   ┌─────────────────────┐                  ┌──────────────────────────┐
   │ pattern table       │                  │ PyTorch / ONNX / JAX      │
   │ codegen → driver    │ ── BYOC ──►       │ import + dynamic shape    │
   │ runtime module      │                  │ fusion + memory planning  │
   └─────────────────────┘                  │ MetaSchedule for the rest │
                                            └──────────────────────────┘
```

当面试官问“如何在不从零写编译器的情况下，为一颗新加速器 bring up 一套软件栈”时，**TVM 上的 BYOC 就是答案**，而 §5 是具体方案：模式表、代码生成、runtime 模块、切分图、卸载你覆盖的部分、让 TVM 兜住剩下的尾巴。

---

## 7. 动手实践 + 测量

在一个真实模型上把它串起来（CUTLASS/TensorRT 用一个小 CNN，或者用 Transformer 块来讲融合的故事）。

**融合：**

```python
mod_unfused = relax.transform.LegalizeOps()(mod)          # lowered, NOT fused
mod_fused   = relax.get_pipeline("zero")(mod)             # includes FuseOps + FuseTIR

def count_prim_funcs(m):
    return sum(1 for gv in m.functions if isinstance(m[gv], tvm.tir.PrimFunc))

print("kernels unfused:", count_prim_funcs(mod_unfused))
print("kernels fused:  ", count_prim_funcs(mod_fused))    # fewer — chains collapsed
```

**BYOC 卸载：**

```python
mod_byoc = partition_for_cutlass(mod)           # or partition_for_tensorrt
mod_byoc = relax.transform.RunCodegen()(mod_byoc)
# build all three, time end-to-end on the Relax VM, verify parity vs the framework reference
```

报告一张表：

| 构建 | 端到端延迟 | # kernel 数 | 图卸载占比 | 精度一致性 |
|---|---|---|---|---|
| TVM，未融合 | — | 多 | 0% | ref |
| TVM，融合（`zero`） | — | 更少 | 0% | ✓ |
| TVM + 调优（MetaSchedule） | — | 更少 | 0% | ✓ |
| TVM + BYOC（CUTLASS/TRT） | — | — | __% | ✓ |

三个能说明问题的数字：**融合**应能减少带宽受限部分的 kernel 数和延迟；**图卸载占比**告诉你 BYOC 后端捕获了模型的多少（以及 TVM 还必须生成什么）；**精度一致性**必须在每一行都成立——融合或卸载后的图如果改变了数值，那是 bug，不是优化。

---

## 8. 迷你实验：融合、卸载，并推理覆盖率

1. 导入一个模型；用三种方式构建——未融合、融合（`zero` 流水线）、融合 + 调优（第 3 讲的 MetaSchedule）。记录延迟和 kernel 数。在 TVMScript 中找出一个融合组，并说出它消除了哪些 DRAM 往返。
2. 给输入加上符号化的批/序列维度后重新编译。确认一个二进制在 Relax VM 上能服务多种输入尺寸。（这是微缩版的 LLM 能力。）
3. 用 `partition_for_cutlass` 或 `partition_for_tensorrt` 卸载。测量延迟并计算进入外部后端的**图占比**（卸载的 kernel / 总数）。写两句话说明后端*没有*接哪些算子，以及 TVM 为什么必须保留它们。

交付物：§7 的表、指出名称的融合组、动态 shape 的证明，以及 BYOC 覆盖率分析。最后那项分析——“后端捕获了 70% 的 FLOPs，但把这三个算子留给了 TVM”——正是加速器软件工程师在界定新后端范围时写的报告。

---


<details>
<summary>English original</summary>

**6. BYOC is how a new accelerator gets a software stack**

This is the part that makes Lecture 4 a *hardware* lecture, and it is the day job of an accelerator-software engineer.

A chip company builds a fast NPU. It does not want to write — and cannot keep up with — a frontend for PyTorch, ONNX, JAX, and every model architecture. With BYOC it doesn't have to. It implements, **once**:

1. A **pattern table**: the op patterns the NPU's kernels cover (its GEMM, its conv, its attention, its activation set).
2. A **codegen**: given an offloaded subgraph, emit calls into the NPU's runtime / driver (or its own compiler).
3. A **runtime module**: load and execute those kernels on the device, interoperating with TVM's runtime for the rest.

In return it gets, **for free**, every model any TVM frontend can import. TVM handles import, dynamic shapes, fusion of the parts that stay on host/GPU, memory planning, and the glue; the NPU handles the ops it's good at. The same mechanism is how TVM's own open accelerator, **VTA** (Versatile Tensor Accelerator), plugs in, and how `tensorize` (Lecture 2) extends downward to custom instructions.

```text
   vendor implements ONCE:                  gets for free:
   ┌─────────────────────┐                  ┌──────────────────────────┐
   │ pattern table       │                  │ PyTorch / ONNX / JAX      │
   │ codegen → driver    │ ── BYOC ──►       │ import + dynamic shape    │
   │ runtime module      │                  │ fusion + memory planning  │
   └─────────────────────┘                  │ MetaSchedule for the rest │
                                            └──────────────────────────┘
```

When an interviewer asks "how would you bring up a software stack for a new accelerator without writing a compiler from scratch," **BYOC on TVM is the answer**, and §5 is the concrete plan: pattern table, codegen, runtime module, partition the graph, offload what you cover, let TVM carry the tail.

---

**7. Hands-on + Measure it**

Put it together on a real model (a small CNN for CUTLASS/TensorRT, or a transformer block for the fusion story).

**Fusion:**

```python
mod_unfused = relax.transform.LegalizeOps()(mod)          # lowered, NOT fused
mod_fused   = relax.get_pipeline("zero")(mod)             # includes FuseOps + FuseTIR

def count_prim_funcs(m):
    return sum(1 for gv in m.functions if isinstance(m[gv], tvm.tir.PrimFunc))

print("kernels unfused:", count_prim_funcs(mod_unfused))
print("kernels fused:  ", count_prim_funcs(mod_fused))    # fewer — chains collapsed
```

**BYOC offload:**

```python
mod_byoc = partition_for_cutlass(mod)           # or partition_for_tensorrt
mod_byoc = relax.transform.RunCodegen()(mod_byoc)
# build all three, time end-to-end on the Relax VM, verify parity vs the framework reference
```

Report a table:

| Build | End-to-end latency | # kernels | % graph offloaded | Parity |
|---|---|---|---|---|
| TVM, unfused | — | many | 0% | ref |
| TVM, fused (`zero`) | — | fewer | 0% | ✓ |
| TVM + tuned (MetaSchedule) | — | fewer | 0% | ✓ |
| TVM + BYOC (CUTLASS/TRT) | — | — | __% | ✓ |

The three numbers that tell the story: **fusion** should cut kernel count and latency on the memory-bound parts; **% of graph offloaded** tells you how much of the model the BYOC backend captured (and what TVM still had to generate); **parity** must hold at every row — a fused or offloaded graph that changed the numbers is a bug, not an optimization.

---

**8. Mini-lab: fuse, offload, and reason about coverage**

1. Import a model; build it three ways — unfused, fused (`zero` pipeline), and fused+tuned (MetaSchedule from Lecture 3). Record latency and kernel count. Identify one fused group in the TVMScript and name the DRAM round-trips it eliminated.
2. Add a symbolic batch/sequence dimension to the input and recompile. Confirm one binary serves multiple input sizes on the Relax VM. (This is the LLM capability in miniature.)
3. Offload with `partition_for_cutlass` or `partition_for_tensorrt`. Measure latency and compute the **fraction of the graph** that went to the external backend (offloaded kernels / total). Write two sentences on which ops the backend *didn't* take and why TVM had to keep them.

Deliverable: the §7 table, the named fused group, the dynamic-shape proof, and the BYOC coverage analysis. That last analysis — "the backend captured 70% of the FLOPs but left these three ops to TVM" — is exactly the report an accelerator-software engineer writes when scoping a new backend.

---

</details>

## 关键要点

- 最大的部署收益往往出现在 **图** 层级，而非某个 kernel 内部：**融合**与 **BYOC**。
- Relax 是带类型的 dataflow IR：struct info 承载 shape+dtype，**dataflow block** 标记可优化的纯区域，副作用位于其外。正是这种干净性，才使激进的图重写成为可能。
- **符号形状**（`R.Tensor(("n", ...))`）让同一份二进制服务可变的批大小/序列长度 —— 这是 LLM 必需的能力，也是 MLC-LLM 构建在 Relax 之上的原因。
- **`FuseOps` → `FuseTIR`** 把算子链折叠成单个 kernel，消除其间的 DRAM 往返。在带宽受限的模型上，这胜过逐 kernel 调优。**反量化-矩阵乘-epilogue 融合**正是 4-bit LLM 跑得快的原因。
- **BYOC** 切分图，把匹配的子图卸载到外部代码生成（CUTLASS / TensorRT / cuDNN / 自定义 NPU），其余部分由 TVM 生成并调优：`partition_for_<backend>` → `MergeCompositeFunctions` → `RunCodegen`。
- BYOC 是**新加速器获得软件栈的方式**：实现一次 pattern table、一份代码生成和一个 runtime 模块，即可免费获得所有能导入 TVM 的模型。这正是 L4 加速器软件工程师的职责。

---

## 参考文献

- Relax / TVM Unity 文档（struct info、dataflow、动态 shape）：[https://tvm.apache.org/docs/reference/api/python/relax/relax.html](https://tvm.apache.org/docs/reference/api/python/relax/relax.html)
- Lai 等人，"Relax: Composable Abstractions for End-to-End Dynamic Machine Learning," 2023：[https://arxiv.org/abs/2311.02103](https://arxiv.org/abs/2311.02103)
- TVM "Bring Your Own Codegen to TVM" 开发者指南：[https://tvm.apache.org/docs/dev/how_to/relay_bring_your_own_codegen.html](https://tvm.apache.org/docs/dev/how_to/relay_bring_your_own_codegen.html)
- Relax 的 BYOC 后端（`partition_for_cutlass`、`partition_for_tensorrt`），位于 `tvm.relax.backend.contrib`：[https://tvm.apache.org/docs/](https://tvm.apache.org/docs/)
- VTA（Versatile Tensor Accelerator）—— TVM 的开源加速器 + BYOC/tensorize 示例：[https://tvm.apache.org/docs/topic/vta/index.html](https://tvm.apache.org/docs/topic/vta/index.html)
- MLC-LLM 编译器 pass 流水线（`FuseDequantizeMatmulEwise` 与 dlight 路径）：[https://llm.mlc.ai/docs/](https://llm.mlc.ai/docs/)

---

*下一讲：[Lecture 05 — Shipping it: runtime, microTVM, and LLMs with MLC-LLM](Lecture-05.md)*


<details>
<summary>English original</summary>

**Key takeaways**

- The largest deployment wins often live at the **graph** level, not inside any kernel: **fusion** and **BYOC**.
- Relax is a typed dataflow IR: struct info carries shape+dtype, **dataflow blocks** mark pure optimizable regions, effects live outside them. This cleanliness is what enables aggressive graph rewriting.
- **Symbolic shapes** (`R.Tensor(("n", ...))`) let one binary serve variable batch/sequence sizes — the capability LLMs require and the reason MLC-LLM is built on Relax.
- **`FuseOps` → `FuseTIR`** collapse op chains into single kernels, deleting the DRAM round-trips between them. On memory-bound models this beats per-kernel tuning. **Dequant-matmul-epilogue fusion** is why 4-bit LLMs run fast.
- **BYOC** partitions the graph and offloads matching subgraphs to an external codegen (CUTLASS / TensorRT / cuDNN / a custom NPU) while TVM generates and tunes the rest: `partition_for_<backend>` → `MergeCompositeFunctions` → `RunCodegen`.
- BYOC is **how a new accelerator gets a software stack**: implement a pattern table, a codegen, and a runtime module once; receive every TVM-importable model for free. This is the L4 accelerator-software engineer's job.

---

**References**

- Relax / TVM Unity documentation (struct info, dataflow, dynamic shape): [https://tvm.apache.org/docs/reference/api/python/relax/relax.html](https://tvm.apache.org/docs/reference/api/python/relax/relax.html)
- Lai et al., "Relax: Composable Abstractions for End-to-End Dynamic Machine Learning," 2023: [https://arxiv.org/abs/2311.02103](https://arxiv.org/abs/2311.02103)
- TVM "Bring Your Own Codegen to TVM" developer guide: [https://tvm.apache.org/docs/dev/how_to/relay_bring_your_own_codegen.html](https://tvm.apache.org/docs/dev/how_to/relay_bring_your_own_codegen.html)
- Relax BYOC backends (`partition_for_cutlass`, `partition_for_tensorrt`) in `tvm.relax.backend.contrib`: [https://tvm.apache.org/docs/](https://tvm.apache.org/docs/)
- VTA (Versatile Tensor Accelerator) — TVM's open accelerator + BYOC/tensorize example: [https://tvm.apache.org/docs/topic/vta/index.html](https://tvm.apache.org/docs/topic/vta/index.html)
- MLC-LLM compiler pass pipeline (`FuseDequantizeMatmulEwise` and the dlight path): [https://llm.mlc.ai/docs/](https://llm.mlc.ai/docs/)

---

*Next: [Lecture 05 — Shipping it: runtime, microTVM, and LLMs with MLC-LLM](Lecture-05.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/TVM Deep Dives/Lecture-04.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/TVM%20Deep%20Dives/Lecture-04.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
