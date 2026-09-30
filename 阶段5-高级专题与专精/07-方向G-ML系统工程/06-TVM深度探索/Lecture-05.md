---
title: 第 05 讲 - 交付：runtime、microTVM，以及使用 MLC-LLM 的大语言模型
description: 第 05 讲 - 交付：runtime、microTVM，以及使用 MLC-LLM 的大语言模型
---

# 第 05 讲 - 交付：runtime、microTVM，以及使用 MLC-LLM 的大语言模型

**合集：** [TVM Deep Dives](README.md) | **上一讲：** [← 第 04 讲](Lecture-04.md) | **下一讲：** [TVM Deep Dives 索引](README.md)

---

只在 Python 调优脚本中运行的编译后的 kernel 没有交付任何东西。编译器的全部意义——相对于框架而言——在于：输出是**独立产物**，你可以将其部署到 Python、CUDA 和 4 GB runtime 无法涉足的地方：C++ 推理服务器、拥有 256 KB SRAM 的 Cortex-M 微控制器、浏览器标签页、手机。

本讲闭合这个循环：TVM 的输出如何成为可部署的东西，覆盖从数据中心到裸机的完整范围，以及整个技术栈最重要的应用——**MLC-LLM**，通用大语言模型部署——如何把第 1–4 讲的所有内容组装成能在没有厂商为其编写大语言模型 kernel 的硬件上运行 Qwen 模型的东西。

它也是课程的顶点项目。到本讲结束时，你应该能够拿到一个模型，编译它，调优它，并以数据交付它。

---

## 学习目标

到本讲结束时，你应该能够：

1. 在三种 runtime——**GraphExecutor**、**Relax VM**、**AOT**——之间做出选择，并导出可加载模块 + 参数。
2. 从**非 Python** 环境（C++ runtime）加载并运行编译后的 TVM 模块。
3. 解释 **microTVM** 路径：AOT + C 代码生成、Model Library Format、部署到裸机 MCU，以及通过 BYOC 进行 CMSIS-NN / Ethos-U 卸载。
4. 端到端描述 **MLC-LLM** 流水线：`relax.frontend.nn` 模型 → 量化 → Relax/TIR 优化 → **dlight** 调度 → 按后端代码生成 → `MLCEngine`。
5. 分析 **dlight 与 MetaSchedule**——即时可移植调度与调优峰值——并正确选择。
6. 跨后端编译并运行一个大语言模型，以及在 MCU 上运行一个小模型，并报告延迟 / 内存 / 大小。

---

## 1. 三种 runtime 与产物

第 1 讲命名了它们；现在我们用它们来交付。runtime 是*编译后的图如何执行*，而选择由部署目标决定。

| Runtime | 执行模型 | 开销 | 动态形状 / 控制流 | 交付至 |
|---|---|---|---|---|
| **GraphExecutor** | 静态图，预规划内存 | 低 | 否 | 固定形状的服务器/边缘推理 |
| **Relax VM** | 字节码 VM | 每算子开销小 | **是** | 大语言模型、动态模型、任何带有 `if`/循环的东西 |
| **AOT** | 提前编译，无解释器，C 入口 | **接近零** | 有限 | 微控制器、裸机、无操作系统 |

**产物**在三种 runtime 中都是同一个概念：编译后的模块加参数。导出它，你就不再需要编译器——只需轻量级 runtime。

```python
import tvm
from tvm import relax

ex = relax.build(mod, target="cuda")          # the Executable from Lecture 1
ex.export_library("model_cuda.so")            # ← the shippable artifact: a shared library

# later, anywhere, with only the TVM runtime present:
loaded = tvm.runtime.load_module("model_cuda.so")
vm = relax.VirtualMachine(loaded, tvm.cuda(0))
out = vm["main"](x)
```

对于经典静态路径，它是在导出的 `.so` + 参数 blob 之上的 `GraphModule`；对于嵌入式，它是 **Model Library Format** 中的 `.tar`（后续章节）。原则是不变的：**一次编译，生成自包含的产物，无需工具链即可部署。**

---

## 2. 无 Python 部署

TVM runtime 是一个小型 C++ 库（"minimal runtime" 可以只有几百 KB）。这正是产物可移植的原因：你从 C++、Rust、Java 或 JavaScript 加载 `.so`，部署机器上没有 Python，也没有编译器。

```cpp
// minimal C++ deployment — the shape of every production TVM integration
#include <tvm/runtime/module.h>
#include <tvm/runtime/registry.h>

tvm::runtime::Module mod = tvm::runtime::Module::LoadFromFile("model_cuda.so");
auto vm_load = tvm::runtime::Registry::Get("relax.VirtualMachine");
// set device, set inputs as NDArrays, call the "main" function, read outputs
```

这是框架无法干净跨越、而编译器生来就要跨越的边界。你的推理服务器是 C++；它加载产物并像调用函数一样调用它。同一个产物，经交叉编译后，可在 ARM SBC 上运行。runtime 是唯一的依赖。

> **再说 RPC。** 你在第 3 讲用于*调优*的同一个 RPC 系统，也是一个*部署与性能剖析*工具：将交叉编译的产物推送到远程板卡，运行它，并拉回时序数据——而无需在设备上安装工具链。通过 RPC 调优，然后通过 RPC 部署和性能剖析。一种机制，覆盖整个边缘生命周期。

---


<details>
<summary>English original</summary>

**Lecture 05 - Shipping It: Runtime, microTVM, and LLMs with MLC-LLM**

**Collection:** [TVM Deep Dives](README.md) | **Previous:** [← Lecture 04](Lecture-04.md) | **Next:** [TVM Deep Dives index](README.md)

---

A compiled kernel that only runs inside a Python tuning script has shipped nothing. The whole point of a compiler — versus a framework — is that the output is a **standalone artifact** you deploy where Python, CUDA, and a 4 GB runtime cannot go: a C++ inference server, a Cortex-M microcontroller with 256 KB of SRAM, a browser tab, a phone.

This lecture closes the loop: how TVM's output becomes a deployable thing, across the full range from datacenter to bare metal, and how the most important application of the entire stack — **MLC-LLM**, universal LLM deployment — assembles everything from Lectures 1–4 into something that runs a Qwen model on hardware no vendor wrote an LLM kernel for.

It is also the course capstone. By the end you should be able to take a model, compile it, tune it, and ship it with numbers.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Choose among the three runtimes — **GraphExecutor**, **Relax VM**, **AOT** — and export a loadable module + params.
2. Load and run a compiled TVM module from a **non-Python** environment (the C++ runtime).
3. Explain the **microTVM** path: AOT + C codegen, the Model Library Format, deployment to a bare-metal MCU, and CMSIS-NN / Ethos-U offload via BYOC.
4. Describe the **MLC-LLM** pipeline end to end: `relax.frontend.nn` model → quantization → Relax/TIR opt → **dlight** schedules → per-backend codegen → `MLCEngine`.
5. Reason about **dlight vs MetaSchedule** — instant portable schedules vs tuned peak — and choose correctly.
6. Compile and run an LLM across backends, and a tiny model on an MCU, and report latency / memory / size.

---

**1. The three runtimes, and the artifact**

Lecture 1 named them; now we ship with them. The runtime is *how the compiled graph executes*, and the choice is dictated by the deployment target.

| Runtime | Execution model | Overhead | Dynamic shapes / control flow | Ship it to |
|---|---|---|---|---|
| **GraphExecutor** | static graph, pre-planned memory | low | no | fixed-shape server/edge inference |
| **Relax VM** | bytecode VM | small per-op | **yes** | LLMs, dynamic models, anything with `if`/loops |
| **AOT** | compiled ahead, no interpreter, C entry | **near-zero** | limited | microcontrollers, bare metal, no-OS |

The **artifact** is the same idea in all three: a compiled module plus the parameters. Export it, and you no longer need the compiler — only the lightweight runtime.

```python
import tvm
from tvm import relax

ex = relax.build(mod, target="cuda")          # the Executable from Lecture 1
ex.export_library("model_cuda.so")            # ← the shippable artifact: a shared library

# later, anywhere, with only the TVM runtime present:
loaded = tvm.runtime.load_module("model_cuda.so")
vm = relax.VirtualMachine(loaded, tvm.cuda(0))
out = vm["main"](x)
```

For the classic static path it is `GraphModule` over an exported `.so` + a params blob; for embedded it is a `.tar` in **Model Library Format** (next sections). The principle is invariant: **compile once, produce a self-contained artifact, deploy it without the toolchain.**

---

**2. Deploying without Python**

The TVM runtime is a small C++ library (the "minimal runtime" can be a few hundred KB). That is what makes the artifact portable: you load the `.so` from C++, Rust, Java, or JavaScript, with no Python and no compiler on the deployment box.

```cpp
// minimal C++ deployment — the shape of every production TVM integration
#include <tvm/runtime/module.h>
#include <tvm/runtime/registry.h>

tvm::runtime::Module mod = tvm::runtime::Module::LoadFromFile("model_cuda.so");
auto vm_load = tvm::runtime::Registry::Get("relax.VirtualMachine");
// set device, set inputs as NDArrays, call the "main" function, read outputs
```

This is the boundary that frameworks cannot cross cleanly and compilers are built for. Your inference server is C++; it loads the artifact and calls it like a function. The same artifact, cross-compiled, runs on an ARM SBC. The runtime is the only dependency.

> **RPC, again.** The same RPC system you used for *tuning* in Lecture 3 is also a *deployment and profiling* tool: push the cross-compiled artifact to a remote board, run it, and pull timings back — without installing a toolchain on the device. Tune over RPC, then deploy and profile over RPC. One mechanism, the whole edge lifecycle.

---

</details>

## 3. microTVM：深入到裸机

现在一路向下——到一颗没有操作系统、没有 `malloc`、SRAM 以 **KB** 计的 Cortex-M 微控制器。这就是 **microTVM**，也正是编译器路线回报最显著的地方：这里没有 PyTorch，没有 CUDA，往往也没有 Linux。模型*就是* C 代码，且是静态规划的。

这条路径相对于服务器侧流程换掉了两样东西：

```text
   server:   target "cuda"   + Relax VM/GraphExecutor + .so + dynamic alloc
   micro:    target "c"      + AOT executor           + MLF + STATIC memory plan
```

* **`c` 代码生成**为各 kernel 产出可移植的 C 源码——设备侧不需要 LLVM 后端，只需要厂商的 C 编译器。
* **AOT 执行器**把*计算图本身*编译成 C（没有解释器，没有 runtime 遍历图）——单一 `tvmgen_default_run()` 入口。
* **静态内存规划**是强制的：每个 buffer 都在编译期被排布进一块固定的 workspace arena，因为没有堆。编译器必须把整个模型的激活值装进 SRAM 预算——并报告这个数字，好让你知道装不装得下。
* 输出是 **Model Library Format (MLF)**——一个 `.tar`，内含生成的 C、参数，以及 **Project API** 集成用来把模型塞进 Zephyr、Arduino 或 CMSIS 构建所需的元数据。

```python
# conceptual microTVM build: AOT + C runtime, exported as Model Library Format
import tvm
from tvm import relax
from tvm.micro import export_model_library_format

target = tvm.target.Target("c -keys=arm_cpu -mcpu=cortex-m55")
# build with the embedded C runtime + AOT executor (no OS, static workspace),
# then package for project generation:
export_model_library_format(built_module, "model_cortex_m.tar")
# → feed the .tar to the Project API to generate a Zephyr/Arduino firmware project, flash, run
```

而 BYOC（第 4 讲）也延伸到这里：在 Arm MCU 上把计算卸载给 **CMSIS-NN**（手工优化的 DSP/SIMD kernel），在带 **Ethos-U** 微 NPU 的芯片上则卸载给 Ethos-U 代码生成——TVM 切分计算图，把 conv/matmul 发给 NPU，其余保留为生成的 C。TinyML 的部署故事——关键词识别、异常检测、纽扣电池预算下的视觉唤醒词——正是这条流水线。

在 MCU 上真正要紧的指标不是 GFLOP/s，而是**装不装得下、赶不赶得上截止时间**：flash 大小、SRAM 峰值（即 workspace arena），以及功耗预算下的推理延迟。microTVM 在编译期就报告这三项，这正是它胜过手工移植的原因：烧录之前你就知道了。

---


<details>
<summary>English original</summary>

**3. microTVM: down to bare metal**

Now go all the way down — to a Cortex-M microcontroller with no operating system, no `malloc`, and SRAM measured in **kilobytes**. This is **microTVM**, and it is where the compiler approach pays off most dramatically: there is no PyTorch here, no CUDA, often no Linux. The model *is* C code, statically planned.

The path swaps two things from the server flow:

```text
   server:   target "cuda"   + Relax VM/GraphExecutor + .so + dynamic alloc
   micro:    target "c"      + AOT executor           + MLF + STATIC memory plan
```

* **`c` codegen** emits portable C source for the kernels — no LLVM backend needed for the device, just the vendor's C compiler.
* **AOT executor** compiles the *graph itself* to C (no interpreter, no runtime graph walk) — a single `tvmgen_default_run()` entry point.
* **Static memory planning** is mandatory: every buffer is laid out at compile time into a fixed workspace arena, because there is no heap. The compiler must fit the whole model's activations into the SRAM budget — and report the number so you know if it fits.
* The output is the **Model Library Format (MLF)** — a `.tar` containing the generated C, the params, and the metadata a **Project API** integration uses to drop the model into a Zephyr, Arduino, or CMSIS build.

```python
# conceptual microTVM build: AOT + C runtime, exported as Model Library Format
import tvm
from tvm import relax
from tvm.micro import export_model_library_format

target = tvm.target.Target("c -keys=arm_cpu -mcpu=cortex-m55")
# build with the embedded C runtime + AOT executor (no OS, static workspace),
# then package for project generation:
export_model_library_format(built_module, "model_cortex_m.tar")
# → feed the .tar to the Project API to generate a Zephyr/Arduino firmware project, flash, run
```

And BYOC (Lecture 4) reaches down here too: on Arm MCUs you offload to **CMSIS-NN** (hand-optimized DSP/SIMD kernels) and, on parts with the **Ethos-U** micro-NPU, to the Ethos-U codegen — TVM partitions the graph, sends conv/matmul to the NPU, and keeps the rest as generated C. The TinyML deployment story — keyword spotting, anomaly detection, vision wake-words on a coin-cell budget — is exactly this pipeline.

The metric that matters on an MCU is not GFLOP/s; it is **does it fit and does it hit the deadline**: flash size, SRAM peak (the workspace arena), and inference latency under the power budget. microTVM reports all three at compile time, which is why it beats hand-porting: you know before you flash.

---

</details>

## 4. MLC-LLM：整个栈，直指 LLM

至此，前面的一切都在这里汇合。**MLC-LLM**（Machine Learning Compilation for LLMs）是一个把 Llama / Qwen / Phi / Gemma 级模型编译到 **CUDA、ROCm、Metal、Vulkan、OpenCL 和 WebGPU** 上运行的项目——服务器 GPU、Mac、iPhone、Android 和浏览器标签页——并且它**构建在 TVM Unity 之上**。当你理解 MLC-LLM 时，你就理解了整门课程一直在构建的目标：一个真实、已发布、可通用部署的系统，它的每个阶段都是你已经学过的一讲。

```text
   HF model (Llama / Qwen / Phi / Gemma ...)
        │  ① define architecture with relax.frontend.nn   (PyTorch-like module API)
        ▼
   Relax IRModule  —  SYMBOLIC shapes: seq_len, batch, kv-cache length   [Lec 4]
        │  ② quantize weights:  q4f16_1 (4-bit group) / q4f16_awq / q0f16 ...
        ▼
   Relax GRAPH optimization  —  fusion incl. FuseDequantizeMatmulEwise    [Lec 4]
        │  ③ legalize → lower to TIR                                       [Lec 1]
        ▼
   TIR optimization                                                       [Lec 2]
        │  ④ dlight default schedules: GEMV, matmul, attention, RMSNorm  — NO tuning
        ▼
   ⑤ codegen per backend:  CUDA | ROCm | Metal | Vulkan | WebGPU | OpenCL [Lec 1]
        ▼
   model library (.so / .dylib / .wasm)  +  MLCEngine runtime (OpenAI-compatible API)
        ▼
   runs on: server GPU · Mac · iPhone · Android · browser tab
```

把每个编号阶段对应到你学过的地方：**①** 是 `relax.frontend.nn` 构建 Relax 图；**②** 量化喂入第 4 讲的 dequant-fusion；**③/⑤** 是第 1 讲的 import/lower/代码生成流程；**④** 是第 2 讲的 TensorIR 调度——只不过由 **dlight** 完成，而非手工完成。MLC-LLM 不是另一个系统。它*就是*这个系统，产品化后的形态。

定义模型使用 `relax.frontend.nn` API——刻意采用 PyTorch 风格，让模型作者感到顺手，但它构建的是一个 Relax `IRModule`：

```python
from tvm.relax.frontend import nn

class MLP(nn.Module):
    def __init__(self):
        self.fc1 = nn.Linear(784, 256)
        self.fc2 = nn.Linear(256, 10)
    def forward(self, x):
        return self.fc2(nn.op.relu(self.fc1(x)))

# export to a Relax IRModule + params — the same module type from Lectures 1–4
mod, params = MLP().export_tvm(
    spec={"forward": {"x": nn.spec.Tensor((1, 784), "float32")}}
)
```

真实的 LLM 定义只是同一思路的规模化版本：带 KV 缓存的 attention、旋转位置编码（RoPE）、RMSNorm、SwiGLU FFN——全部由 `nn` 算子构建，全部产出一个符号形状的 Relax 模块，供流水线其余部分优化。

---

## 5. dlight：零调优获得好调度——以及 MLC 为何需要它

这就是 MLC-LLM 解决的设计张力，也是一堂真正重要的系统课。MetaSchedule（第 3 讲）产生*峰值* kernel——但它通过在**目标设备上测量**来调优，耗时从几分钟到几小时。你无法**在每个用户的手机 GPU 上**运行 2000 次试验的调优任务，也无法**在浏览器标签页中**运行，或在你并不实际拥有的 iPhone 上运行。可移植性与逐设备调优直接冲突。

**dlight** 就是解法：一个**手写默认调度规则**库，每个重要算子类别一条（decode（逐 token 生成阶段）用 GEMV（矩阵-向量乘），prefill（首字前的整段计算）用矩阵乘，归约、attention、RMSNorm），它们能**即时、解析地、无需测量**地产生*好的* GPU 调度。不是峰值——但在任何后端上都能立刻达到峰值的 80–90%。

```python
import tvm.dlight as dl

with tvm.target.Target("vulkan"):              # or metal, webgpu, cuda, rocm ...
    mod = dl.ApplyDefaultSchedule(             # apply default rules, NO tuning loop
        dl.gpu.Matmul(),
        dl.gpu.GEMV(),                         # the decode-phase workhorse
        dl.gpu.Reduction(),
        dl.gpu.GeneralReduction(),
        dl.gpu.Fallback(),                     # a safe default for anything unmatched
    )(mod)
```

用一张表说清取舍——而知道该用哪个，是资深工程师的判断：

| | **dlight** | **MetaSchedule** |
|---|---|---|
| 方式 | 每个算子类别的解析默认规则 | 搜索 + 端侧测量 |
| 得到 kernel 的时间 | 即时 | 分钟–小时 |
| 质量 | 好（约峰值的 80–90%） | 峰值 |
| 需要目标设备？ | **否** | **是**（在其上测量） |
| 跨后端可移植性 | 即时 | 每个目标重新调优 |
| 适用于 | **MLC-LLM**：发布到你无法调优的手机/web/Mac | 你掌控并可一次性调优的服务器 kernel |

所以规则是：**当你无法调优部署目标时用 dlight**（消费级设备、GPU 长尾、浏览器）——这正是 MLC-LLM 的处境。**当你拥有芯片时用 MetaSchedule**，可以把一次性调优任务摊销成峰值吞吐（你的数据中心推理服务集群）。成熟部署两者都用——dlight 负责可移植性，MetaSchedule 负责他们掌控的硬件上的 kernel。

---


<details>
<summary>English original</summary>

**4. MLC-LLM: the whole stack, pointed at LLMs**

Everything so far converges here. **MLC-LLM** (Machine Learning Compilation for LLMs) is the project that compiles Llama / Qwen / Phi / Gemma-class models to run on **CUDA, ROCm, Metal, Vulkan, OpenCL, and WebGPU** — server GPUs, Macs, iPhones, Android, and browser tabs — and it is **built on TVM Unity**. When you understand MLC-LLM, you understand what the entire course was building toward: a real, shipping, universal-deployment system whose every stage is a lecture you've already done.

```text
   HF model (Llama / Qwen / Phi / Gemma ...)
        │  ① define architecture with relax.frontend.nn   (PyTorch-like module API)
        ▼
   Relax IRModule  —  SYMBOLIC shapes: seq_len, batch, kv-cache length   [Lec 4]
        │  ② quantize weights:  q4f16_1 (4-bit group) / q4f16_awq / q0f16 ...
        ▼
   Relax GRAPH optimization  —  fusion incl. FuseDequantizeMatmulEwise    [Lec 4]
        │  ③ legalize → lower to TIR                                       [Lec 1]
        ▼
   TIR optimization                                                       [Lec 2]
        │  ④ dlight default schedules: GEMV, matmul, attention, RMSNorm  — NO tuning
        ▼
   ⑤ codegen per backend:  CUDA | ROCm | Metal | Vulkan | WebGPU | OpenCL [Lec 1]
        ▼
   model library (.so / .dylib / .wasm)  +  MLCEngine runtime (OpenAI-compatible API)
        ▼
   runs on: server GPU · Mac · iPhone · Android · browser tab
```

Map each numbered stage to where you learned it: **①** is `relax.frontend.nn` building a Relax graph; **②** quantization feeds the dequant-fusion of Lecture 4; **③/⑤** are the import/lower/codegen flow of Lecture 1; **④** is TensorIR scheduling from Lecture 2 — except done by **dlight** instead of by hand. MLC-LLM is not a different system. It is *this* system, productized.

Defining a model uses the `relax.frontend.nn` API — deliberately PyTorch-shaped so model authors feel at home, but it builds a Relax `IRModule`:

```python
from tvm.relax.frontend import nn

class MLP(nn.Module):
    def __init__(self):
        self.fc1 = nn.Linear(784, 256)
        self.fc2 = nn.Linear(256, 10)
    def forward(self, x):
        return self.fc2(nn.op.relu(self.fc1(x)))

# export to a Relax IRModule + params — the same module type from Lectures 1–4
mod, params = MLP().export_tvm(
    spec={"forward": {"x": nn.spec.Tensor((1, 784), "float32")}}
)
```

A real LLM definition is the same idea at scale: attention with a KV-cache, RoPE, RMSNorm, a SwiGLU FFN — all built from `nn` ops, all producing a symbolic-shape Relax module the rest of the pipeline optimizes.

---

**5. dlight: good schedules with zero tuning — and why MLC needs it**

Here is the design tension MLC-LLM resolves, and it is a genuinely important systems lesson. MetaSchedule (Lecture 3) produces *peak* kernels — but it tunes by **measuring on the target device**, for minutes to hours. You cannot run a 2000-trial tuning job **on every user's phone GPU**, or **in a browser tab**, or on an iPhone you don't physically have. Portability and per-device tuning are in direct conflict.

**dlight** is the resolution: a library of **hand-written default schedule rules**, one per important op class (GEMV for decode, matmul for prefill, reductions, attention, RMSNorm), that produce *good* GPU schedules **instantly, analytically, with no measurement**. Not peak — but 80–90% of it, on any backend, immediately.

```python
import tvm.dlight as dl

with tvm.target.Target("vulkan"):              # or metal, webgpu, cuda, rocm ...
    mod = dl.ApplyDefaultSchedule(             # apply default rules, NO tuning loop
        dl.gpu.Matmul(),
        dl.gpu.GEMV(),                         # the decode-phase workhorse
        dl.gpu.Reduction(),
        dl.gpu.GeneralReduction(),
        dl.gpu.Fallback(),                     # a safe default for anything unmatched
    )(mod)
```

The tradeoff in one table — and knowing which to reach for is a senior-engineer judgment call:

| | **dlight** | **MetaSchedule** |
|---|---|---|
| How | analytical default rules per op class | search + on-device measurement |
| Time to a kernel | instant | minutes–hours |
| Quality | good (~80–90% of peak) | peak |
| Needs the target device? | **no** | **yes** (measures on it) |
| Portability across backends | immediate | re-tune per target |
| Right for | **MLC-LLM**: ship to phones/web/Macs you can't tune on | server kernels you control and tune once |

So the rule: **dlight when you cannot tune the deployment target** (consumer devices, the long tail of GPUs, the browser) — which is exactly MLC-LLM's situation. **MetaSchedule when you own the silicon** and can amortize a one-time tuning job into peak throughput (your datacenter serving fleet). Mature deployments use both — dlight for portability, MetaSchedule for the kernels on hardware they control.

---

</details>

## 6. 动手：跨后端编译并运行 LLM

MLC-LLM CLI 用三条命令走完 §4 的流水线，随后由 `MLCEngine` 以 OpenAI 兼容 API 提供结果。

```bash
# ① convert + quantize weights (4-bit group quant, fp16 activations)
mlc_llm convert_weight ./models/Qwen2.5-7B-Instruct/ \
    --quantization q4f16_1 -o ./dist/Qwen2.5-7B-q4f16_1-MLC

# ② generate runtime config (chat template, context window, KV settings)
mlc_llm gen_config ./models/Qwen2.5-7B-Instruct/ \
    --quantization q4f16_1 --conv-template qwen2 -o ./dist/Qwen2.5-7B-q4f16_1-MLC

# ③ compile to a model library for a target backend  (swap --device to retarget)
mlc_llm compile ./dist/Qwen2.5-7B-q4f16_1-MLC/mlc-chat-config.json \
    --device cuda -o ./dist/libs/Qwen2.5-7B-q4f16_1-cuda.so
#   --device metal | vulkan | webgpu | rocm | android | iphone   ← same model, different silicon
```

运行：

```python
from mlc_llm import MLCEngine

engine = MLCEngine(model="./dist/Qwen2.5-7B-q4f16_1-MLC",
                   model_lib="./dist/libs/Qwen2.5-7B-q4f16_1-cuda.so")
for chunk in engine.chat.completions.create(
        messages=[{"role": "user", "content": "Explain operator fusion in one paragraph."}],
        stream=True):
    print(chunk.choices[0].delta.content, end="", flush=True)
```

量化模式是你现在已从内部理解的部署杠杆（第 4 讲的反量化融合正是它快的原因）：

| 模式 | 位宽 / 格式 | 用途 |
|---|---|---|
| `q0f16` | fp16，无量化 | 质量参考 / 大显存服务器 |
| `q4f16_1` | 4-bit 分组权重，fp16 激活值 | 主力方案 —— 笔记本、手机、大多数 GPU |
| `q4f16_awq` | 4-bit AWQ 权重，fp16 激活值 | 4-bit 下准确率更好（激活感知） |
| `q3f16_1` | 3-bit 分组 | 内存最紧张时的最后手段 |

---

## 7. 测量

拿不出数字就等于没交付。不同目标的产物不同，但准则完全一致：说出该目标真正关心的指标。

| 目标 | 真正重要的指标 |
|---|---|
| 服务器 GPU（Relax VM / MetaSchedule） | 延迟、吞吐、相对 roofline（性能上界模型）的 GFLOP/s、$/inference |
| 通过 MLC-LLM 的 LLM | **tokens/s**（prefill（首字前的整段计算） + decode（逐 token 生成阶段））、峰值 VRAM、磁盘上的模型大小、首 token 时间 |
| 边缘板（RPC 调优） | 功耗上限下的延迟、内存、设备端调优 vs 桌面端调优的差值 |
| MCU（微控制器，microTVM） | **flash 大小、SRAM 峰值（workspace arena）、延迟、单次推理能耗** —— 装得下吗，赶得上截止时间吗 |

具体到 LLM，要在相同量化下至少报告两个后端（例如 CUDA 与 Metal，或 CUDA 与 Vulkan）的 tokens/s，外加 VRAM 与磁盘占用。跨后端的差异就是这一课的重点：**同一个 Relax 模型、同一套 dlight 调度、不同的硅片** —— 由此可以看出哪个后端的代码生成把性能留在了桌上。

---

## 8. 课程综合项目

这个产物用来证明整门课所学。挑一个你在意的模型，产出一个**编译后模型仓库**：

1. **导入与构建**（第 1 讲）：把模型引入 Relax；在 Relax VM 上构建并运行；与框架参考实现做精度一致性检查。
2. **调度与调优**（第 2–3 讲）：针对**两个目标**做 MetaSchedule 调优（例如 x86 + CUDA，或 CUDA + 通过 RPC 连接的边缘板）。把调优数据库保留在仓库里。
3. **优化计算图**（第 4 讲）：展示融合（kernel 数量前后对比），并做**一次 BYOC 实验**（把一个子图卸载到 CUTLASS/TensorRT；若目标是 MCU 则卸载到 CMSIS-NN），报告被卸载子图占整图的比例。
4. **交付**（第 5 讲）：导出产物，并在 Python 之外加载它（C++ runtime；若是 LLM 则用 MLC-LLM `MLCEngine`；若是 MCU 则用 microTVM MLF）。
5. **benchmark 表格**：框架基线 vs 未调优 TVM vs 调优后 TVM vs（调优 + BYOC），每一行都给出延迟、GFLOP/s、**roofline 峰值的百分比**以及精度一致性。

那个仓库就是一件 Level-5 产物：另一位工程师克隆它、运行你的脚本，就能在同一档硬件上复现你调优后的数字。它同时也是一件作品集条目，毫不含糊地说明：*我能把一个模型从框架一路带到 metal，并证明提速效果* —— 而这正是全部工作。

---


<details>
<summary>English original</summary>

**6. Hands-on: compile and run an LLM across backends**

The MLC-LLM CLI walks the pipeline of §4 as three commands, then `MLCEngine` serves the result with an OpenAI-compatible API.

```bash
# ① convert + quantize weights (4-bit group quant, fp16 activations)
mlc_llm convert_weight ./models/Qwen2.5-7B-Instruct/ \
    --quantization q4f16_1 -o ./dist/Qwen2.5-7B-q4f16_1-MLC

# ② generate runtime config (chat template, context window, KV settings)
mlc_llm gen_config ./models/Qwen2.5-7B-Instruct/ \
    --quantization q4f16_1 --conv-template qwen2 -o ./dist/Qwen2.5-7B-q4f16_1-MLC

# ③ compile to a model library for a target backend  (swap --device to retarget)
mlc_llm compile ./dist/Qwen2.5-7B-q4f16_1-MLC/mlc-chat-config.json \
    --device cuda -o ./dist/libs/Qwen2.5-7B-q4f16_1-cuda.so
#   --device metal | vulkan | webgpu | rocm | android | iphone   ← same model, different silicon
```

Run it:

```python
from mlc_llm import MLCEngine

engine = MLCEngine(model="./dist/Qwen2.5-7B-q4f16_1-MLC",
                   model_lib="./dist/libs/Qwen2.5-7B-q4f16_1-cuda.so")
for chunk in engine.chat.completions.create(
        messages=[{"role": "user", "content": "Explain operator fusion in one paragraph."}],
        stream=True):
    print(chunk.choices[0].delta.content, end="", flush=True)
```

The quantization mode is a deployment lever you now understand from the inside (Lecture 4's dequant fusion is what makes it fast):

| Mode | Bits / format | Use |
|---|---|---|
| `q0f16` | fp16, no quant | quality reference / big-VRAM server |
| `q4f16_1` | 4-bit group weights, fp16 act | the workhorse — laptops, phones, most GPUs |
| `q4f16_awq` | 4-bit AWQ weights, fp16 act | better accuracy at 4-bit (activation-aware) |
| `q3f16_1` | 3-bit group | last resort for the tightest memory |

---

**7. Measure it**

Ship with numbers or you didn't ship. The deliverables differ by target but the discipline is identical: name the metric the target actually cares about.

| Target | Metrics that matter |
|---|---|
| Server GPU (Relax VM / MetaSchedule) | latency, throughput, GFLOP/s vs roofline, $/inference |
| LLM via MLC-LLM | **tokens/s** (prefill + decode), peak VRAM, model size on disk, time-to-first-token |
| Edge board (RPC-tuned) | latency under power cap, memory, device-tuned vs desktop-tuned delta |
| MCU (microTVM) | **flash size, SRAM peak (workspace arena), latency, energy/inference** — does it fit, does it hit the deadline |

For the LLM specifically, report tokens/s on at least two backends (e.g. CUDA and Metal, or CUDA and Vulkan) at the same quantization, plus the VRAM and disk footprint. The cross-backend spread is the lesson: **same Relax model, same dlight schedules, different silicon** — and you can read which backend's codegen is leaving performance on the table.

---

**8. Course capstone**

This is the artifact that proves the whole course. Pick one model you care about and produce a **compiled-model repo**:

1. **Import & build** (Lec 1): bring the model into Relax; build and run on the Relax VM; parity-check vs the framework reference.
2. **Schedule & tune** (Lec 2–3): MetaSchedule-tune it for **two targets** (e.g. x86 + CUDA, or CUDA + an edge board over RPC). Keep the tuning database in the repo.
3. **Optimize the graph** (Lec 4): show fusion (kernel-count before/after) and run **one BYOC experiment** (offload a subgraph to CUTLASS/TensorRT, or to CMSIS-NN if your target is an MCU), reporting the fraction of the graph offloaded.
4. **Ship** (Lec 5): export the artifact and load it from outside Python (C++ runtime, or MLC-LLM `MLCEngine` if it's an LLM, or microTVM MLF if it's an MCU).
5. **Benchmark table**: framework baseline vs un-tuned TVM vs tuned TVM vs (tuned + BYOC), with latency, GFLOP/s, **% of roofline peak**, and parity at every row.

That repo is a Level-5 artifact: another engineer clones it, runs your script, and reproduces your tuned numbers on the same hardware class. It is also a portfolio piece that says, unambiguously, *I can take a model from framework to metal and prove the speedup* — which is the entire job.

---

</details>

## 9. 课程达成标准

当你能做到以下各项时，即视为完成 **TVM Deep Dives**：

* 在每一层读懂 `IRModule`——Relax 图、TIR `PrimFunc`、生成的 CUDA/C——并说清每个 pass 改了什么（Lec 1）。
* 手工调度 kernel，并用 **roofline**（性能上界模型）的视角解释其中每个原语，包括 `tensorize` 到 Tensor Core roof（Lec 2）。
* 用 MetaSchedule 调优 kernel 和模型，读懂调优曲线的拐点，并通过 RPC 在**目标设备**上实测（Lec 3）。
* 用 **BYOC** 融合图并卸载子图，并说明厂商要让一个新的加速器拥有一套软件栈需要实现什么（Lec 4）。
* 把结果交付到非 Python 的目标上——服务器 `.so`、MCU（微控制器）固件，或用 MLC-LLM 跨后端跑 LLM——并**拿 roofline 来支撑这些数字**（Lec 5）。

如果只能让 TVM *跑通*一个模型，那你造出来的只是个转译器。如果能让它*跑赢基线并在两个目标上证明这一点*，你就是一名 ML 编译器工程师。这才是全部意义所在。

---

## 关键要点

- 编译器的价值在于**独立产物**：`export_library` → 一个 `.so`/`.tar`，只需带上很小的 TVM runtime，就能从 C++/Rust/JS 加载——设备上不需要 Python，也不需要工具链。
- 三种 runtime，三种部署形态：**GraphExecutor**（固定 shape）、**Relax VM**（动态/LLM）、**AOT**（裸机）。
- **microTVM** 用 **AOT** executor 和**静态内存规划**把模型编译成 **C**，打包成供 MCU 使用的 **Model Library Format**；BYOC 可以一直下探到 **CMSIS-NN** 和 **Ethos-U**。衡量指标是*装得下 + 赶得上截止时间*，而不是 GFLOP/s。
- **MLC-LLM** 是把整条栈对准 LLM，构建在 TVM Unity 之上：`relax.frontend.nn` 模型 → 量化 → Relax/TIR 优化（含 dequant 融合）→ **dlight** 调度 → 各后端代码生成 → `MLCEngine`。每个阶段都是前面某一讲的内容。
- **dlight vs MetaSchedule**：dlight 无需端侧调优就能给出即时、可移植、约 80–90% 的调度（当你要交付到无法调优的手机/网页端时，这一点至关重要）；MetaSchedule 在你掌握芯片本身时能给出峰值性能。成熟的栈两者都用。
- 交付时要给出与目标匹配的数字——LLM 用 tokens/s + VRAM，MCU 用 flash + SRAM + latency——并且始终拿 roofline 来支撑这些数字。

---

## 参考资料

- TVM 部署与 runtime 文档（`export_library`、GraphExecutor、Relax VM、C++ 部署）：[https://tvm.apache.org/docs/how_to/deploy/](https://tvm.apache.org/docs/how_to/deploy/)
- microTVM（AOT、Model Library Format、Project API、Zephyr/Arduino）：[https://tvm.apache.org/docs/topic/microtvm/index.html](https://tvm.apache.org/docs/topic/microtvm/index.html)
- 通过 TVM BYOC 使用 Arm Ethos-U 与 CMSIS-NN：[https://tvm.apache.org/docs/](https://tvm.apache.org/docs/)
- MLC-LLM 文档（编译模型、`MLCEngine`、量化模式）：[https://llm.mlc.ai/docs/](https://llm.mlc.ai/docs/)
- MLC-LLM 源码与支持的模型/后端矩阵：[https://github.com/mlc-ai/mlc-llm](https://github.com/mlc-ai/mlc-llm)
- `tvm.dlight`（默认 GPU 调度）：[https://tvm.apache.org/docs/](https://tvm.apache.org/docs/)
- *AI Inference Engineer 2026*——量化与推理服务相关课程，作为本章内容所服务的生产级 LLM 背景。

---

*返回：[TVM Deep Dives 索引](README.md)*


<details>
<summary>English original</summary>

**9. Course exit criteria**

You have completed **TVM Deep Dives** when you can:

* Read an `IRModule` at every level — Relax graph, TIR `PrimFunc`, generated CUDA/C — and say what each pass changed (Lec 1).
* Hand-schedule a kernel and explain every primitive in **roofline** terms, including `tensorize` to a Tensor Core roof (Lec 2).
* Tune a kernel and a model with MetaSchedule, read the tuning curve to the knee, and measure on the **target device** over RPC (Lec 3).
* Fuse a graph and offload a subgraph via **BYOC**, and explain what a vendor implements to give a new accelerator a software stack (Lec 4).
* Ship the result to a non-Python target — server `.so`, MCU firmware, or an LLM across backends with MLC-LLM — and **defend the numbers against the roofline** (Lec 5).

If you can only make TVM *run* a model, you built a transpiler. If you can make it *beat the baseline and prove it on two targets*, you are an ML compiler engineer. That was the whole point.

---

**Key takeaways**

- The compiler's value is the **standalone artifact**: `export_library` → a `.so`/`.tar` you load from C++/Rust/JS with only the small TVM runtime — no Python, no toolchain on the device.
- Three runtimes, three deployments: **GraphExecutor** (fixed-shape), **Relax VM** (dynamic/LLM), **AOT** (bare metal).
- **microTVM** compiles the model to **C** with an **AOT** executor and a **static memory plan**, packaged as **Model Library Format** for MCUs; BYOC reaches down to **CMSIS-NN** and **Ethos-U**. The metric is *fit + deadline*, not GFLOP/s.
- **MLC-LLM** is the whole stack pointed at LLMs, built on TVM Unity: `relax.frontend.nn` model → quantize → Relax/TIR opt (incl. dequant fusion) → **dlight** schedules → per-backend codegen → `MLCEngine`. Every stage is a prior lecture.
- **dlight vs MetaSchedule**: dlight gives instant, portable, ~80–90% schedules with no on-device tuning (essential when you ship to phones/web you can't tune on); MetaSchedule gives peak when you own the silicon. Mature stacks use both.
- Ship with numbers matched to the target — tokens/s + VRAM for an LLM, flash + SRAM + latency for an MCU — and always defend them against the roofline.

---

**References**

- TVM deploy & runtime docs (`export_library`, GraphExecutor, Relax VM, C++ deploy): [https://tvm.apache.org/docs/how_to/deploy/](https://tvm.apache.org/docs/how_to/deploy/)
- microTVM (AOT, Model Library Format, Project API, Zephyr/Arduino): [https://tvm.apache.org/docs/topic/microtvm/index.html](https://tvm.apache.org/docs/topic/microtvm/index.html)
- Arm Ethos-U & CMSIS-NN via TVM BYOC: [https://tvm.apache.org/docs/](https://tvm.apache.org/docs/)
- MLC-LLM documentation (compile models, `MLCEngine`, quantization modes): [https://llm.mlc.ai/docs/](https://llm.mlc.ai/docs/)
- MLC-LLM source & supported model/backend matrix: [https://github.com/mlc-ai/mlc-llm](https://github.com/mlc-ai/mlc-llm)
- `tvm.dlight` (default GPU schedules): [https://tvm.apache.org/docs/](https://tvm.apache.org/docs/)
- *AI Inference Engineer 2026* — quantization and serving lectures, for the production-LLM context this feeds.

---

*Back to: [TVM Deep Dives index](README.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/TVM Deep Dives/Lecture-05.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/TVM%20Deep%20Dives/Lecture-05.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
