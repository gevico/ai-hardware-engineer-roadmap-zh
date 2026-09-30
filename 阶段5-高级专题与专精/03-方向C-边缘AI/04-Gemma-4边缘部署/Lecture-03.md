---
title: 第 03 讲 — PDL Runtime 栈：Jetson 上的 LiteRT、llama.cpp、MLC-LLM 与 TensorRT-LLM
description: 第 03 讲 — PDL Runtime 栈：Jetson 上的 LiteRT、llama.cpp、MLC-LLM 与 TensorRT-LLM
---

# 第 03 讲 — PDL Runtime 栈：Jetson 上的 LiteRT、llama.cpp、MLC-LLM 与 TensorRT-LLM

**合集：** [Gemma 4 边缘部署](README.md) | **上一讲：** [← 第 02 讲](Lecture-02.md) | **下一讲：** [第 04 讲 →](Lecture-04.md)

---

你已有一个量化后的 Gemma 4 检查点。现在需要一个能在 Jetson 上运行它的 runtime。值得认真考虑的方案有四种，它们彼此不可互换 —— 每一种都占据吞吐、可移植性、工具链锁定与开发投入这一取舍空间中的不同位置。本讲逐一切开每种 runtime，给出具体的部署命令，测量 Orin 与 Thor 上的延迟与吞吐画像，并构建用于拍板的选择矩阵。

**PDL — Portable Deployment and Loading** 是本讲的框架：其中的 "P"（跨目标可移植性）、"D"（部署简洁性）与 "L"（加载优化 —— paged KV、权重流式加载、CUDA graphs）在每种 runtime 中的处理方式各不相同。

---

## 学习目标

1. 描述 **LiteRT**（Google AI Edge）、**llama.cpp**、**MLC-LLM** 与 **TensorRT-LLM** 的架构和目标用例，以及每种方案为何存在。
2. 用每种 runtime 在 Jetson Orin 上部署 Gemma 4 4B INT4，并测量 batch=1 时的 tokens/s。
3. 解释 **paged attention**、**CUDA graphs** 与 **连续批处理**，并指出哪些 runtime 分别实现了它们。
4. 为给定部署场景构建 runtime 选择决策矩阵。
5. 用 `ncu`/`nsys` 对 runtime 部署做性能剖析，并识别主导瓶颈。

---

## 1. Runtime 全景概览

```text
Four runtimes for Gemma 4 on Jetson:

  ┌───────────────────┬──────────────────────────────────────────────────┐
  │ Runtime           │ Primary optimization target                       │
  ├───────────────────┼──────────────────────────────────────────────────┤
  │ LiteRT            │ Portable: Android, iOS, Jetson DLA, browser       │
  │ (Google AI Edge)  │ NOT for Jetson CUDA peak — for cross-platform     │
  ├───────────────────┼──────────────────────────────────────────────────┤
  │ llama.cpp         │ Ubiquitous CPU+CUDA runtime, GGUF format          │
  │                   │ Best: simplicity, portability, community support  │
  ├───────────────────┼──────────────────────────────────────────────────┤
  │ MLC-LLM           │ TVM-compiled kernels, cross-backend (CUDA/Vulkan/ │
  │                   │ WebGPU/Metal/ROCm). Best: tuned kernels + portability │
  ├───────────────────┼──────────────────────────────────────────────────┤
  │ TensorRT-LLM      │ NVIDIA-closed, maximum CUDA throughput.           │
  │                   │ Best: production throughput on Jetson CUDA        │
  └───────────────────┴──────────────────────────────────────────────────┘
```

---

## 2. LiteRT — Google AI Edge runtime

**LiteRT**（Light Runtime，前身为 TensorFlow Lite）是 Google 的可移植推理 runtime。它消费 `.tflite` FlatBuffer 文件，可运行在 CPU、GPU delegate 以及设备专有加速器（Jetson DLA、Coral Edge TPU、Qualcomm AI Engine）上。

### 2.1 架构

```text
LiteRT inference pipeline for Gemma 4 4B:

  .tflite file → FlatBuffer deserialize → Operation graph
                                              │
                              ┌───────────────┼────────────────┐
                              ▼               ▼                ▼
                         CPU delegate    GPU delegate     DLA delegate
                         (ARM Neon)     (OpenGL ES/CL)   (Jetson INT8)
                              └───────────────┴────────────────┘
                                              │
                                         Output tensor
```

### 2.2 在 Jetson 上用 LiteRT 部署 Gemma 4

```bash
# Install AI Edge Torch and LiteRT Python bindings:
pip install ai-edge-litert ai-edge-torch

# Python inference with LiteRT:
python3 - <<'EOF'
import numpy as np
import ai_edge_litert.interpreter as litert

# Load the Gemma 4 4B INT4 tflite model:
interpreter = litert.Interpreter(
    model_path="gemma4-4b-it-int4.tflite",
    experimental_delegates=[
        litert.load_delegate("libdelegate.so")   # GPU delegate (if available)
    ]
)
interpreter.allocate_tensors()

# Input: token IDs of shape (1, seq_len)
input_details  = interpreter.get_input_details()
output_details = interpreter.get_output_details()

tokens = np.array([[2, 1, 5678, 3]], dtype=np.int32)  # BOS + "Hello"
interpreter.set_tensor(input_details[0]["index"], tokens)
interpreter.invoke()
logits = interpreter.get_tensor(output_details[0]["index"])  # (1, seq_len, vocab)
print(f"Next token: {np.argmax(logits[0, -1])}")
EOF
```


<details>
<summary>English original</summary>

**Lecture 03 — The PDL Runtime Stack: LiteRT, llama.cpp, MLC-LLM, and TensorRT-LLM on Jetson**

**Collection:** [Gemma 4 Edge Deployment](README.md) | **Previous:** [← Lecture 02](Lecture-02.md) | **Next:** [Lecture 04 →](Lecture-04.md)

---

You have a quantized Gemma 4 checkpoint. Now you need a runtime that runs it on Jetson. Four serious options exist, and they are not interchangeable — each occupies a different point in the tradeoff space of throughput, portability, toolchain lock-in, and development effort. This lecture dissects each runtime, gives you concrete deployment commands, measures the latency and throughput profiles on Orin and Thor, and builds the selection matrix you use to make the call.

**PDL — Portable Deployment and Loading** is the framing for this lecture: the "P" (portability across targets), the "D" (deployment simplicity), and the "L" (loading optimization — paged KV, weight streaming, CUDA graphs) are each addressed differently by each runtime.

---

**Learning objectives**

1. Describe the architecture and target use-case of **LiteRT** (Google AI Edge), **llama.cpp**, **MLC-LLM**, and **TensorRT-LLM** and why each exists.
2. Deploy Gemma 4 4B INT4 on Jetson Orin using each runtime and measure tokens/s at batch=1.
3. Explain **paged attention**, **CUDA graphs**, and **continuous batching** and identify which runtimes implement each.
4. Build the runtime-selection decision matrix for a given deployment scenario.
5. Profile a runtime deployment with `ncu`/`nsys` and identify the dominant bottleneck.

---

**1. Runtime landscape overview**

```text
Four runtimes for Gemma 4 on Jetson:

  ┌───────────────────┬──────────────────────────────────────────────────┐
  │ Runtime           │ Primary optimization target                       │
  ├───────────────────┼──────────────────────────────────────────────────┤
  │ LiteRT            │ Portable: Android, iOS, Jetson DLA, browser       │
  │ (Google AI Edge)  │ NOT for Jetson CUDA peak — for cross-platform     │
  ├───────────────────┼──────────────────────────────────────────────────┤
  │ llama.cpp         │ Ubiquitous CPU+CUDA runtime, GGUF format          │
  │                   │ Best: simplicity, portability, community support  │
  ├───────────────────┼──────────────────────────────────────────────────┤
  │ MLC-LLM           │ TVM-compiled kernels, cross-backend (CUDA/Vulkan/ │
  │                   │ WebGPU/Metal/ROCm). Best: tuned kernels + portability │
  ├───────────────────┼──────────────────────────────────────────────────┤
  │ TensorRT-LLM      │ NVIDIA-closed, maximum CUDA throughput.           │
  │                   │ Best: production throughput on Jetson CUDA        │
  └───────────────────┴──────────────────────────────────────────────────┘
```

---

**2. LiteRT — Google AI Edge runtime**

**LiteRT** (Light Runtime, formerly TensorFlow Lite) is Google's portable inference runtime. It consumes `.tflite` FlatBuffer files and runs on CPU, GPU delegate, and device-specific accelerators (Jetson DLA, Coral Edge TPU, Qualcomm AI Engine).

**2.1 Architecture**

```text
LiteRT inference pipeline for Gemma 4 4B:

  .tflite file → FlatBuffer deserialize → Operation graph
                                              │
                              ┌───────────────┼────────────────┐
                              ▼               ▼                ▼
                         CPU delegate    GPU delegate     DLA delegate
                         (ARM Neon)     (OpenGL ES/CL)   (Jetson INT8)
                              └───────────────┴────────────────┘
                                              │
                                         Output tensor
```

**2.2 Deploying Gemma 4 with LiteRT on Jetson**

```bash
# Install AI Edge Torch and LiteRT Python bindings:
pip install ai-edge-litert ai-edge-torch

# Python inference with LiteRT:
python3 - <<'EOF'
import numpy as np
import ai_edge_litert.interpreter as litert

# Load the Gemma 4 4B INT4 tflite model:
interpreter = litert.Interpreter(
    model_path="gemma4-4b-it-int4.tflite",
    experimental_delegates=[
        litert.load_delegate("libdelegate.so")   # GPU delegate (if available)
    ]
)
interpreter.allocate_tensors()

# Input: token IDs of shape (1, seq_len)
input_details  = interpreter.get_input_details()
output_details = interpreter.get_output_details()

tokens = np.array([[2, 1, 5678, 3]], dtype=np.int32)  # BOS + "Hello"
interpreter.set_tensor(input_details[0]["index"], tokens)
interpreter.invoke()
logits = interpreter.get_tensor(output_details[0]["index"])  # (1, seq_len, vocab)
print(f"Next token: {np.argmax(logits[0, -1])}")
EOF
```

</details>

### 2.3 LiteRT 在 Jetson Orin 上的吞吐

```text
Gemma 4 4B INT4 (.tflite) on Jetson AGX Orin:
  CPU-only (ARM, 12 cores):    ~3–5 tok/s   (slow — CPU matmul)
  GPU delegate (OpenGL CL):    ~15–20 tok/s  (better, but GPU delegate is not CUDA)
  DLA (where applicable):      ~10–15 tok/s  (INT8 only, not all ops)

vs llama.cpp CUDA (same model):  ~45–65 tok/s
vs TRT-LLM:                      ~70–90 tok/s

Gap: LiteRT on Jetson CUDA is 3–5× slower than CUDA-optimized runtimes.
```

**为什么还要在 Jetson 上用 LiteRT：**

1. **跨平台部署**：同一个 `.tflite` 无需重新编译即可跑在 Jetson（DLA/GPU delegate）、Android（NNAPI/GPU）、iOS（Core ML delegate）以及嵌入式 Linux 上。
2. **DLA 利用率**：Jetson 的深度学习加速器（DLA）是固定功能的 INT8 引擎。LiteRT 可以把算子划分到 DLA，把以 GEMM 为主的 layer 放在 DLA 上，其余放在 CPU/GPU 上——能效优于纯 CUDA。
3. **合规/审计路径**：在汽车/医疗边缘场景，拥有一个固定的可移植二进制（`.tflite`）比 runtime 编译的 TRT engine 更容易通过认证。

---

## 3. llama.cpp —— 通用的 Jetson runtime

**llama.cpp** 是社区标准的 C++ 大语言模型推理引擎。它读取 GGUF 文件，具备生产级 CUDA kernel，能在各种 OS 上运行，并支持拥有 256K 词表的 Gemma 4。

### 3.1 架构

```text
llama.cpp inference path for Gemma 4 on Jetson:

  GGUF → model load → layer-by-layer CUDA kernel dispatch
           │
           ├─ Weight decompression (K-quant dequant in CUDA)
           ├─ GEMV (batch=1 decode, weights × token vector) via CUDA GEMV kernels
           ├─ GEMM (batch>1 or prefill, weights × token matrix) via cuBLAS
           ├─ KV cache (paged or standard, CUDA device memory)
           ├─ Attention (FlashAttention or baseline CUDA kernel)
           └─ Sampling (argmax / top-p, CUDA)
```

### 3.2 为 Jetson 构建 llama.cpp

```bash
# On Jetson AGX Orin / Thor:
git clone https://github.com/ggml-org/llama.cpp && cd llama.cpp

# Build with CUDA support (Orin: sm_87, Thor: sm_90):
cmake -B build -DGGML_CUDA=ON \
    -DCMAKE_CUDA_ARCHITECTURES="87"   # Orin = Ampere SM 8.7
    # For Thor: use "90" (Hopper SM 9.0)

cmake --build build -j$(nproc)

# Basic inference:
./build/bin/llama-cli \
    -m gemma4-4b-Q4_K_M.gguf \
    -p "Describe a robot arm picking a red cube." \
    -n 200 \
    --n-gpu-layers 9999 \     # offload all layers to CUDA
    --ctx-size 4096

# Benchmark (tokens/s at batch=1):
./build/bin/llama-bench \
    -m gemma4-4b-Q4_K_M.gguf \
    -p 512 -n 200 \           # 512-token prompt, 200 tokens to generate
    --n-gpu-layers 9999

# Expected on Orin (204 GB/s): ~45–65 tok/s (Q4_K_M, batch=1)
```

### 3.3 用 OpenAI 兼容 API 做 llama.cpp 推理服务

```bash
# Start an OpenAI-compatible server (llama-server):
./build/bin/llama-server \
    -m gemma4-4b-Q4_K_M.gguf \
    --n-gpu-layers 9999 \
    --host 0.0.0.0 --port 8080 \
    --ctx-size 8192 \
    --parallel 4 \            # up to 4 concurrent users (continuous batching)
    --cont-batching           # enable continuous batching

# Query:
curl http://localhost:8080/v1/chat/completions \
    -H "Content-Type: application/json" \
    -d '{
    "model": "gemma4-4b",
    "messages": [{"role": "user", "content": "Hello from Jetson!"}],
    "max_tokens": 100, "stream": true
    }'
```

**llama.cpp 分页 KV cache：**

```bash
# Enable paged KV (reduces memory for long contexts, allows slot reuse):
./build/bin/llama-server \
    -m gemma4-4b-Q4_K_M.gguf \
    --n-gpu-layers 9999 \
    --ctx-size 131072 \       # 128K context (possible on Orin INT4 due to local attention!)
    --parallel 2 \
    -ub 512 \                 # unbatch size: process in chunks of 512
    --kv-cache-type q8_0      # KV cache in INT8 (halves KV memory: 2.7 GB → 1.35 GB)
```

### 3.4 llama.cpp 在 Jetson 上的吞吐画像

```text
Gemma 4 4B Q4_K_M on Jetson AGX Orin (204 GB/s):
  Prefill (pp):   512 tokens → ~180–220 tok/s   (compute-bound: GEMM)
  Decode  (tg):   batch=1   → ~45–65  tok/s     (memory-bound: GEMV)
  Decode  (tg):   batch=4   → ~70–90  tok/s     (GEMM kicking in, better utilization)

Gemma 4 12B Q4_K_M on Jetson AGX Orin:
  Decode (tg): batch=1 → ~18–22 tok/s

Gemma 4 4B Q4_K_M on Jetson AGX Thor (273 GB/s):
  Decode (tg): batch=1 → ~60–85 tok/s   (43% BW gain from H100 translates ~1:1)
```

---

## 4. MLC-LLM —— 为 Jetson 编译 TVM kernel

**MLC-LLM**（Machine Learning Compilation for LLMs）是 TVM 生态的生产部署引擎。它用 **dlight** 默认调度（Matmul、GEMV、归约、RMSNorm）把 Gemma 4 编译成优化后的 CUDA kernel——无需调优，即可达到约 80–90% 的峰值吞吐。


<details>
<summary>English original</summary>

**2.3 LiteRT throughput on Jetson Orin**

```text
Gemma 4 4B INT4 (.tflite) on Jetson AGX Orin:
  CPU-only (ARM, 12 cores):    ~3–5 tok/s   (slow — CPU matmul)
  GPU delegate (OpenGL CL):    ~15–20 tok/s  (better, but GPU delegate is not CUDA)
  DLA (where applicable):      ~10–15 tok/s  (INT8 only, not all ops)

vs llama.cpp CUDA (same model):  ~45–65 tok/s
vs TRT-LLM:                      ~70–90 tok/s

Gap: LiteRT on Jetson CUDA is 3–5× slower than CUDA-optimized runtimes.
```

**Why to use LiteRT on Jetson anyway:**

1. **Cross-deployment**: same `.tflite` runs on Jetson (DLA/GPU delegate), Android (NNAPI/GPU), iOS (Core ML delegate), and embedded Linux with no recompilation.
2. **DLA utilization**: Jetson's Deep Learning Accelerator (DLA) is a fixed-function INT8 engine. LiteRT can partition ops to DLA, running the GEMM-heavy layers on DLA and the rest on CPU/GPU — better power efficiency than pure CUDA.
3. **Regulatory / audit path**: in automotive/medical edge, having a fixed portable binary (`.tflite`) is simpler for certification than a runtime-compiled TRT engine.

---

**3. llama.cpp — the universal Jetson runtime**

**llama.cpp** is the community-standard C++ LLM inference engine. It reads GGUF files, has production-grade CUDA kernels, runs on every OS, and supports Gemma 4 with its 256K vocabulary.

**3.1 Architecture**

```text
llama.cpp inference path for Gemma 4 on Jetson:

  GGUF → model load → layer-by-layer CUDA kernel dispatch
           │
           ├─ Weight decompression (K-quant dequant in CUDA)
           ├─ GEMV (batch=1 decode, weights × token vector) via CUDA GEMV kernels
           ├─ GEMM (batch>1 or prefill, weights × token matrix) via cuBLAS
           ├─ KV cache (paged or standard, CUDA device memory)
           ├─ Attention (FlashAttention or baseline CUDA kernel)
           └─ Sampling (argmax / top-p, CUDA)
```

**3.2 Building llama.cpp for Jetson**

```bash
# On Jetson AGX Orin / Thor:
git clone https://github.com/ggml-org/llama.cpp && cd llama.cpp

# Build with CUDA support (Orin: sm_87, Thor: sm_90):
cmake -B build -DGGML_CUDA=ON \
    -DCMAKE_CUDA_ARCHITECTURES="87"   # Orin = Ampere SM 8.7
    # For Thor: use "90" (Hopper SM 9.0)

cmake --build build -j$(nproc)

# Basic inference:
./build/bin/llama-cli \
    -m gemma4-4b-Q4_K_M.gguf \
    -p "Describe a robot arm picking a red cube." \
    -n 200 \
    --n-gpu-layers 9999 \     # offload all layers to CUDA
    --ctx-size 4096

# Benchmark (tokens/s at batch=1):
./build/bin/llama-bench \
    -m gemma4-4b-Q4_K_M.gguf \
    -p 512 -n 200 \           # 512-token prompt, 200 tokens to generate
    --n-gpu-layers 9999

# Expected on Orin (204 GB/s): ~45–65 tok/s (Q4_K_M, batch=1)
```

**3.3 llama.cpp serving with OpenAI-compatible API**

```bash
# Start an OpenAI-compatible server (llama-server):
./build/bin/llama-server \
    -m gemma4-4b-Q4_K_M.gguf \
    --n-gpu-layers 9999 \
    --host 0.0.0.0 --port 8080 \
    --ctx-size 8192 \
    --parallel 4 \            # up to 4 concurrent users (continuous batching)
    --cont-batching           # enable continuous batching

# Query:
curl http://localhost:8080/v1/chat/completions \
    -H "Content-Type: application/json" \
    -d '{
    "model": "gemma4-4b",
    "messages": [{"role": "user", "content": "Hello from Jetson!"}],
    "max_tokens": 100, "stream": true
    }'
```

**llama.cpp paged KV cache:**

```bash
# Enable paged KV (reduces memory for long contexts, allows slot reuse):
./build/bin/llama-server \
    -m gemma4-4b-Q4_K_M.gguf \
    --n-gpu-layers 9999 \
    --ctx-size 131072 \       # 128K context (possible on Orin INT4 due to local attention!)
    --parallel 2 \
    -ub 512 \                 # unbatch size: process in chunks of 512
    --kv-cache-type q8_0      # KV cache in INT8 (halves KV memory: 2.7 GB → 1.35 GB)
```

**3.4 llama.cpp throughput profile on Jetson**

```text
Gemma 4 4B Q4_K_M on Jetson AGX Orin (204 GB/s):
  Prefill (pp):   512 tokens → ~180–220 tok/s   (compute-bound: GEMM)
  Decode  (tg):   batch=1   → ~45–65  tok/s     (memory-bound: GEMV)
  Decode  (tg):   batch=4   → ~70–90  tok/s     (GEMM kicking in, better utilization)

Gemma 4 12B Q4_K_M on Jetson AGX Orin:
  Decode (tg): batch=1 → ~18–22 tok/s

Gemma 4 4B Q4_K_M on Jetson AGX Thor (273 GB/s):
  Decode (tg): batch=1 → ~60–85 tok/s   (43% BW gain from H100 translates ~1:1)
```

---

**4. MLC-LLM — TVM-compiled kernels for Jetson**

**MLC-LLM** (Machine Learning Compilation for LLMs) is the production deployment engine of the TVM ecosystem. It compiles Gemma 4 to optimized CUDA kernels using **dlight** default schedules (Matmul, GEMV, Reduction, RMSNorm) — no tuning required, ~80–90% of peak throughput.

</details>

### 4.1 架构

```text
MLC-LLM pipeline for Gemma 4 on Jetson:

  HuggingFace weights → mlc_llm convert_weight → MLC weight format
  config.json         → mlc_llm gen_config    → MLC config
                      → mlc_llm compile       → compiled .so / DSO
                                                  │
                    TVM Relax IR (dynamic shapes) │
                    → dlight GPU schedules        │
                    → CUDA codegen               │
                    → .so runtime library        │
                                                  ▼
                              MLCEngine (Python/C++ API)
                              → async streamed generation
```

### 4.2 用 MLC-LLM 编译 Gemma 4

```bash
# Install MLC-LLM (Jetson with CUDA):
pip install --pre mlc-llm -f https://mlc.ai/wheels

# Convert Gemma 4 weights to MLC format:
mlc_llm convert_weight \
    google/gemma-4-4b-it \
    --quantization q4f16_1 \    # INT4 weights, FP16 activations
    --output  gemma4-4b-mlc/

# Generate runtime config:
mlc_llm gen_config \
    google/gemma-4-4b-it \
    --quantization q4f16_1 \
    --prefill-chunk-size 1024 \
    --output gemma4-4b-mlc/

# Compile for Jetson (Orin: sm_87, Thor: sm_90):
mlc_llm compile \
    gemma4-4b-mlc/mlc-chat-config.json \
    --device cuda \
    --opt O3 \
    --output gemma4-4b-mlc/lib.so
```

### 4.3 用 MLC-LLM Python API 做推理

```python
from mlc_llm import MLCEngine
import asyncio

# Initialize with the compiled model:
engine = MLCEngine(
    model="./gemma4-4b-mlc",
    model_lib="./gemma4-4b-mlc/lib.so",
    device="cuda",
    mode="server",         # enable paged KV + continuous batching
)

# Simple synchronous generation:
response = engine.chat.completions.create(
    messages=[{"role": "user", "content": "Explain KV cache in one paragraph."}],
    model="gemma4-4b-mlc",
    max_tokens=200,
    stream=False,
)
print(response.choices[0].message.content)

# Streaming:
for chunk in engine.chat.completions.create(
    messages=[{"role": "user", "content": "Hello from Jetson Thor!"}],
    model="gemma4-4b-mlc", max_tokens=100, stream=True,
):
    print(chunk.choices[0].delta.content, end="", flush=True)

engine.terminate()
```

### 4.4 MLC-LLM 量化模式

| 模式 | 说明 | 大小（4B） | tok/s（Orin） |
|------|-------------|-----------|--------------|
| `q0f16` | 不量化，FP16 激活值 | 8.6 GB | ~20 tok/s |
| `q0f32` | 不量化，FP32 | 17.2 GB | ~10 tok/s |
| `q4f16_1` | **INT4 权重，FP16 激活值（推荐）** | **2.2 GB** | **~55 tok/s** |
| `q4f16_awq` | AWQ INT4，FP16 激活值 | 2.2 GB | ~58 tok/s |
| `q3f16_1` | INT3 权重，FP16 | 1.7 GB | ~60 tok/s（质量较低） |
| `q8f16_1` | INT8 权重，FP16 | 4.3 GB | ~35 tok/s |

### 4.5 MLC-LLM 与 llama.cpp 吞吐对比

```text
Gemma 4 4B q4f16_1 on Jetson AGX Orin:
  MLC-LLM:    ~55–70 tok/s (dlight schedules, CUDA GEMV kernel tuned for Orin sm_87)
  llama.cpp:  ~45–65 tok/s (CUDA GEMV kernel, hand-written)
  Delta:      MLC-LLM ~10–15% faster due to dlight schedule selection
              (more pronounced on 12B+ where tile choices matter more)

MLC-LLM advantage: cross-backend (CUDA → Vulkan → WebGPU) with ONE compile step.
llama.cpp advantage: zero compilation, GGUF file-based, instant swap.
```

---

## 5. TensorRT-LLM —— Jetson 上的 CUDA 峰值吞吐

**TensorRT-LLM** 是 NVIDIA 的推理 runtime，构建在 TensorRT 引擎优化之上。它闭源，但以开放库的形式发布。对 Jetson Orin/Thor 而言，它通过 WGMMA（Hopper）或融合 GEMV（矩阵-向量乘）kernel（Ampere 架构/Orin）提供可达的最高 CUDA 吞吐。

### 5.1 TRT-LLM 架构

```text
TRT-LLM pipeline for Gemma 4 on Jetson:

  GPTQ safetensors → trtllm-build → TRT engine (.plan file)
                         │
                    Layer fusion (QKV proj fusion, FFN fusion)
                    INT4 weight-only GEMM kernels (FP16/BF16 act)
                    Paged KV cache (PagedAttention)
                    Continuous batching (in-flight batching)
                    CUDA graphs (low overhead decode loops)
                         │
                    TRT engine (.plan)
                         │
                    tensorrt_llm.runtime.GenerationSession
```


<details>
<summary>English original</summary>

**4.1 Architecture**

```text
MLC-LLM pipeline for Gemma 4 on Jetson:

  HuggingFace weights → mlc_llm convert_weight → MLC weight format
  config.json         → mlc_llm gen_config    → MLC config
                      → mlc_llm compile       → compiled .so / DSO
                                                  │
                    TVM Relax IR (dynamic shapes) │
                    → dlight GPU schedules        │
                    → CUDA codegen               │
                    → .so runtime library        │
                                                  ▼
                              MLCEngine (Python/C++ API)
                              → async streamed generation
```

**4.2 Compiling Gemma 4 with MLC-LLM**

```bash
# Install MLC-LLM (Jetson with CUDA):
pip install --pre mlc-llm -f https://mlc.ai/wheels

# Convert Gemma 4 weights to MLC format:
mlc_llm convert_weight \
    google/gemma-4-4b-it \
    --quantization q4f16_1 \    # INT4 weights, FP16 activations
    --output  gemma4-4b-mlc/

# Generate runtime config:
mlc_llm gen_config \
    google/gemma-4-4b-it \
    --quantization q4f16_1 \
    --prefill-chunk-size 1024 \
    --output gemma4-4b-mlc/

# Compile for Jetson (Orin: sm_87, Thor: sm_90):
mlc_llm compile \
    gemma4-4b-mlc/mlc-chat-config.json \
    --device cuda \
    --opt O3 \
    --output gemma4-4b-mlc/lib.so
```

**4.3 Inference with MLC-LLM Python API**

```python
from mlc_llm import MLCEngine
import asyncio

# Initialize with the compiled model:
engine = MLCEngine(
    model="./gemma4-4b-mlc",
    model_lib="./gemma4-4b-mlc/lib.so",
    device="cuda",
    mode="server",         # enable paged KV + continuous batching
)

# Simple synchronous generation:
response = engine.chat.completions.create(
    messages=[{"role": "user", "content": "Explain KV cache in one paragraph."}],
    model="gemma4-4b-mlc",
    max_tokens=200,
    stream=False,
)
print(response.choices[0].message.content)

# Streaming:
for chunk in engine.chat.completions.create(
    messages=[{"role": "user", "content": "Hello from Jetson Thor!"}],
    model="gemma4-4b-mlc", max_tokens=100, stream=True,
):
    print(chunk.choices[0].delta.content, end="", flush=True)

engine.terminate()
```

**4.4 MLC-LLM quantization modes**

| Mode | Description | Size (4B) | tok/s (Orin) |
|------|-------------|-----------|--------------|
| `q0f16` | No quant, FP16 activations | 8.6 GB | ~20 tok/s |
| `q0f32` | No quant, FP32 | 17.2 GB | ~10 tok/s |
| `q4f16_1` | **INT4 weights, FP16 act (recommended)** | **2.2 GB** | **~55 tok/s** |
| `q4f16_awq` | AWQ INT4, FP16 act | 2.2 GB | ~58 tok/s |
| `q3f16_1` | INT3 weights, FP16 | 1.7 GB | ~60 tok/s (lower quality) |
| `q8f16_1` | INT8 weights, FP16 | 4.3 GB | ~35 tok/s |

**4.5 MLC-LLM throughput vs llama.cpp**

```text
Gemma 4 4B q4f16_1 on Jetson AGX Orin:
  MLC-LLM:    ~55–70 tok/s (dlight schedules, CUDA GEMV kernel tuned for Orin sm_87)
  llama.cpp:  ~45–65 tok/s (CUDA GEMV kernel, hand-written)
  Delta:      MLC-LLM ~10–15% faster due to dlight schedule selection
              (more pronounced on 12B+ where tile choices matter more)

MLC-LLM advantage: cross-backend (CUDA → Vulkan → WebGPU) with ONE compile step.
llama.cpp advantage: zero compilation, GGUF file-based, instant swap.
```

---

**5. TensorRT-LLM — peak CUDA throughput on Jetson**

**TensorRT-LLM** is NVIDIA's inference runtime, built on TensorRT engine optimization. It is closed-source but ships as an open library. For Jetson Orin/Thor, it provides the highest achievable CUDA throughput through WGMMA (Hopper) or fused GEMV (Ampere/Orin) kernels.

**5.1 TRT-LLM architecture**

```text
TRT-LLM pipeline for Gemma 4 on Jetson:

  GPTQ safetensors → trtllm-build → TRT engine (.plan file)
                         │
                    Layer fusion (QKV proj fusion, FFN fusion)
                    INT4 weight-only GEMM kernels (FP16/BF16 act)
                    Paged KV cache (PagedAttention)
                    Continuous batching (in-flight batching)
                    CUDA graphs (low overhead decode loops)
                         │
                    TRT engine (.plan)
                         │
                    tensorrt_llm.runtime.GenerationSession
```

</details>

### 5.2 在 Jetson Orin 上为 Gemma 4 构建 TRT engine

```bash
# Install TensorRT-LLM (Jetson arm64 build — check NVIDIA's Jetson package):
pip install tensorrt-llm   # or from NVIDIA's JetPack wheel

# Convert GPTQ weights to TRT-LLM format:
python convert_checkpoint.py \
    --model_dir google/gemma-4-4b-it \
    --quant_ckpt_path ./gemma4-4b-gptq-int4 \
    --dtype float16 \
    --use_weight_only \
    --weight_only_precision int4_gptq \
    --group_size 128 \
    --output_dir ./gemma4-4b-trt-ckpt/

# Build TRT engine (this takes 5–15 minutes on Orin):
trtllm-build \
    --checkpoint_dir ./gemma4-4b-trt-ckpt/ \
    --output_dir ./gemma4-4b-engine/ \
    --gemm_plugin float16 \
    --max_input_len 8192 \
    --max_output_len 2048 \
    --max_batch_size 4 \
    --paged_kv_cache enable \
    --remove_input_padding enable

# Run inference:
python run.py \
    --engine_dir ./gemma4-4b-engine/ \
    --tokenizer_dir google/gemma-4-4b-it \
    --input_text "Hello from Jetson Thor!" \
    --max_output_len 200
```

### 5.3 Jetson 上的 TRT-LLM 吞吐

```text
Gemma 4 4B INT4 GPTQ on Jetson AGX Orin (204 GB/s, sm_87):
  Decode (batch=1):   ~70–90 tok/s    (best-in-class for CUDA on Orin)
  Decode (batch=4):   ~120–160 tok/s  (paged attention, continuous batching)
  Prefill (pp):       ~250–350 tok/s  (fused GEMM, attention)

Gemma 4 4B INT4 GPTQ on Jetson AGX Thor (273 GB/s, sm_90):
  Decode (batch=1):   ~90–120 tok/s   (WGMMA tensor cores, TMA)
  Decode (batch=8):   ~200–280 tok/s  (batched decode)
```

**TRT-LLM 为何快于 llama.cpp：**

1. **融合 QKV 投影**：不再调用三次独立的 GEMM（Q、K、V 投影），TRT-LLM 将它们融合为一次 GEMM，输出宽度为 3×。
2. **INT4 GEMM kernel**，借助 TensorRT 的优化搜索，按 SM 架构逐个调优。
3. **CUDA graphs**：整个 decode（逐 token 生成阶段）循环（4B 模型上 30–60 次 kernel 调用）被捕获为一个 CUDA graph 并以近乎为零的启动开销重放。llama.cpp 默认不使用 CUDA graphs。
4. **in-flight batching（连续批处理）**：新请求被插入正在进行的 decode 批中，无需等待其完成，从而最大化 GPU 利用率。

---

## 6. 边缘侧的 paged attention —— 为何在 128K 下至关重要

标准 KV cache 在启动时即按 `max_seq_len` 个 token 预分配。对 128K 上下文的 4B 模型（BF16 下 KV 为 2.7 GB，INT8 KV 下为 1.35 GB），若多数请求都很短，哪怕只预分配一个槽位也是浪费内存。

**PagedAttention**（vLLM、TRT-LLM 和 MLC-LLM 中所采用）以 **page**（16–32 个 token 的块）为单位管理 KV 内存，并按需分配 page：

```text
Standard KV: allocate 128K slots × all KV dimensions at request start
  → wastes ~1.35 GB if the actual conversation is 1K tokens

PagedAttention: allocate 32-token pages on demand
  → 1K conversation = 32 pages × (32 tokens × KV_size) = small
  → Only grow when needed; reclaim when request ends
  → Enables 4× more concurrent conversations at the same memory budget
```

在 Jetson Orin（64 GB）上，配合 Gemma 4 4B INT4（2.2 GB 权重）+ paged INT8 KV：

```text
Available for KV: 64 GB - 2.2 GB weights - 1 GB OS = ~60 GB
KV page size (16 tokens, all layers, INT8): 
  = 16 × (29 × 2 × 1024/16 × 4 × 256 + 5 × 2 × 16 × 4 × 256) × 1 byte
  ≈ varies with local window fill — practical: ~4–8 MB per page for Gemma 4 4B

Concurrent users at 4K context each (with paged KV INT8):
  Per user KV: ~206 MB (as computed in Lecture 01) × 0.5 (INT8) ≈ 103 MB
  Available: 60,000 MB / 103 MB ≈ 580 concurrent user slots theoretically
  Practical (contention, overhead): ~20–40 concurrent users at 4K context
```

---


<details>
<summary>English original</summary>

**5.2 Building a TRT engine for Gemma 4 on Jetson Orin**

```bash
# Install TensorRT-LLM (Jetson arm64 build — check NVIDIA's Jetson package):
pip install tensorrt-llm   # or from NVIDIA's JetPack wheel

# Convert GPTQ weights to TRT-LLM format:
python convert_checkpoint.py \
    --model_dir google/gemma-4-4b-it \
    --quant_ckpt_path ./gemma4-4b-gptq-int4 \
    --dtype float16 \
    --use_weight_only \
    --weight_only_precision int4_gptq \
    --group_size 128 \
    --output_dir ./gemma4-4b-trt-ckpt/

# Build TRT engine (this takes 5–15 minutes on Orin):
trtllm-build \
    --checkpoint_dir ./gemma4-4b-trt-ckpt/ \
    --output_dir ./gemma4-4b-engine/ \
    --gemm_plugin float16 \
    --max_input_len 8192 \
    --max_output_len 2048 \
    --max_batch_size 4 \
    --paged_kv_cache enable \
    --remove_input_padding enable

# Run inference:
python run.py \
    --engine_dir ./gemma4-4b-engine/ \
    --tokenizer_dir google/gemma-4-4b-it \
    --input_text "Hello from Jetson Thor!" \
    --max_output_len 200
```

**5.3 TRT-LLM throughput on Jetson**

```text
Gemma 4 4B INT4 GPTQ on Jetson AGX Orin (204 GB/s, sm_87):
  Decode (batch=1):   ~70–90 tok/s    (best-in-class for CUDA on Orin)
  Decode (batch=4):   ~120–160 tok/s  (paged attention, continuous batching)
  Prefill (pp):       ~250–350 tok/s  (fused GEMM, attention)

Gemma 4 4B INT4 GPTQ on Jetson AGX Thor (273 GB/s, sm_90):
  Decode (batch=1):   ~90–120 tok/s   (WGMMA tensor cores, TMA)
  Decode (batch=8):   ~200–280 tok/s  (batched decode)
```

**Why TRT-LLM is faster than llama.cpp:**

1. **Fused QKV projection**: instead of three separate GEMM calls (Q, K, V projections), TRT-LLM fuses them into one GEMM with 3× output width.
2. **INT4 GEMM kernels** tuned per-SM-architecture with TensorRT's optimization search.
3. **CUDA graphs**: the entire decode loop (30–60 kernel calls on 4B) is captured as a CUDA graph and replayed with near-zero launch overhead. llama.cpp does NOT use CUDA graphs by default.
4. **In-flight batching (continuous batching)**: new requests are inserted into ongoing decode batches without waiting for completion, maximizing GPU utilization.

---

**6. Paged attention on edge — why it matters at 128K**

Standard KV cache is pre-allocated for `max_seq_len` tokens at launch time. For a 4B model at 128K context (2.7 GB KV at BF16, 1.35 GB at INT8 KV), pre-allocating even one slot wastes memory if most requests are short.

**PagedAttention** (as used in vLLM, TRT-LLM, and MLC-LLM) manages KV memory in **pages** (blocks of 16–32 tokens) and allocates pages on demand:

```text
Standard KV: allocate 128K slots × all KV dimensions at request start
  → wastes ~1.35 GB if the actual conversation is 1K tokens

PagedAttention: allocate 32-token pages on demand
  → 1K conversation = 32 pages × (32 tokens × KV_size) = small
  → Only grow when needed; reclaim when request ends
  → Enables 4× more concurrent conversations at the same memory budget
```

On Jetson Orin (64 GB), with Gemma 4 4B INT4 (2.2 GB weights) + paged INT8 KV:

```text
Available for KV: 64 GB - 2.2 GB weights - 1 GB OS = ~60 GB
KV page size (16 tokens, all layers, INT8): 
  = 16 × (29 × 2 × 1024/16 × 4 × 256 + 5 × 2 × 16 × 4 × 256) × 1 byte
  ≈ varies with local window fill — practical: ~4–8 MB per page for Gemma 4 4B

Concurrent users at 4K context each (with paged KV INT8):
  Per user KV: ~206 MB (as computed in Lecture 01) × 0.5 (INT8) ≈ 103 MB
  Available: 60,000 MB / 103 MB ≈ 580 concurrent user slots theoretically
  Practical (contention, overhead): ~20–40 concurrent users at 4K context
```

---

</details>

## 7. Runtime 选择矩阵

```text
                    │ LiteRT     │ llama.cpp   │ MLC-LLM    │ TRT-LLM
────────────────────┼────────────┼─────────────┼────────────┼────────────
Peak CUDA tok/s     │ ~~         │ ✓ (good)    │ ✓ (good)   │ ✓✓ (best)
Jetson DLA support  │ ✓✓ (best)  │ ✗           │ ✗          │ limited
Cross-platform      │ ✓✓ (best)  │ ✓ (GGUF)    │ ✓ (TVM)    │ ✗ (NVIDIA only)
Compilation needed  │ optional   │ ✗ (zero)    │ ✓ (TVM)    │ ✓✓ (full TRT)
GGUF format         │ ✗          │ ✓✓ (native) │ ✗          │ ✗
Google AI Edge SDK  │ ✓✓ (native)│ ✗           │ ✗          │ ✗
Paged attention     │ limited    │ ✓ (added)   │ ✓ (native) │ ✓✓ (best)
CUDA graphs         │ ✗          │ ✗ (default) │ limited     │ ✓✓ (native)
Continuous batching │ ✗          │ ✓ (server)  │ ✓ (server) │ ✓✓ (native)
Ease of deployment  │ ✓✓         │ ✓✓          │ ✓           │ ✓ (complex)
Community support   │ ✓ (Google) │ ✓✓ (best)   │ ✓           │ ✓ (NVIDIA)
Best for            │ DLA/mobile │ general use │ tuned CUDA │ peak prod
```

**决策规则：**

```text
USE LiteRT when:
  ├─ Deploying to Jetson DLA + Android + iOS from one model file
  ├─ Power budget forces DLA usage (lower watt-per-token than CUDA)
  └─ Regulatory / certification requires portable binary

USE llama.cpp when:
  ├─ Quick prototyping, zero compilation
  ├─ GGUF swap without recompile (testing different quant levels)
  └─ Community ecosystem (many tools, easy debugging)

USE MLC-LLM when:
  ├─ Want TVM-tuned kernels without TRT-LLM complexity
  ├─ Need Vulkan/WebGPU backend (e.g., cross-platform embedded)
  └─ Using MLC-LLM's Python API for streaming production service

USE TRT-LLM when:
  ├─ Production service on Jetson Orin/Thor with CUDA
  ├─ Need paged KV + continuous batching + CUDA graphs together
  └─ Throughput is the primary metric and compilation cost is acceptable
```

---

## 8. 在 Jetson 上对你的 runtime 做性能剖析

无论哪种 runtime，性能剖析的工作流都是：

```bash
# 1. nsys for timeline (which kernels fire, any idle gaps):
nsys profile --trace=cuda,nvtx \
    ./your_inference_command \
    --output gemma4_profile

nsys stats gemma4_profile.nsys-rep

# 2. ncu for per-kernel metrics (achieved BW, SM utilization):
ncu --target-processes all \
    --metrics \
        gpu__compute_memory_throughput.avg.pct_of_peak_sustained_elapsed,\
        sm__throughput.avg.pct_of_peak_sustained_elapsed \
    ./your_inference_command

# 3. What to look for:
#  - Decode step: gpu__compute_memory_throughput > 60% → good (BW-bound, expected)
#  - Long idle gaps between kernels → CUDA-graph capture would help
#  - Low BW utilization → kernel choice is wrong (GEMM vs GEMV mismatch)
```

---

## 关键要点

- **四种 runtime，四种取舍**：LiteRT 主打可移植性/DLA，llama.cpp 主打简单，MLC-LLM 主打 TVM 调优的 CUDA，TRT-LLM 主打生产级峰值性能。
- **TRT-LLM 在 Jetson CUDA 上最快**：70–90 tok/s（4B，Orin），而 llama.cpp 为 45–65，得益于融合 projection、INT4 GEMM 调优和 CUDA graphs。
- **paged attention** 对 Jetson 上的 128K 上下文至关重要：它把每个请求 1.35 GB 的 KV 预留变成按需页分配，在典型对话长度下可支持 20–40 倍更多的并发槽位。
- **CUDA graphs** 消除了 kernel 启动开销（Orin 上每个 decode 步 300–500 µs）——TRT-LLM 原生使用它们；llama.cpp 可通过打补丁来使用。
- **MLC-LLM 的 dlight 调度**在 Gemma 4 上比 llama.cpp 手写 kernel 的 GEMV 吞吐高约 10–15%，因为 dlight 根据 SM 架构选择分块尺寸。
- **Jetson CUDA 上的 LiteRT 慢 3–5 倍**，不及 CUDA 系 runtime——仅当 DLA 或跨平台可移植性足以抵偿吞吐损失时才使用它。

---

## 参考资料

- llama.cpp（GGUF、CUDA 后端、llama-server）——[github.com/ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp)
- MLC-LLM（TVM Unity、dlight 调度、跨平台）——[github.com/mlc-ai/mlc-llm](https://github.com/mlc-ai/mlc-llm)
- TensorRT-LLM（CUDA graphs、paged KV、连续批处理）——[github.com/NVIDIA/TensorRT-LLM](https://github.com/NVIDIA/TensorRT-LLM)
- Google AI Edge LiteRT 文档——[ai.google.dev/edge/litert](https://ai.google.dev/edge/litert)
- vLLM PagedAttention 论文（Kwon 等，2023）——arXiv:2309.06180
- NVIDIA CUDA graphs 文档——[docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#cuda-graphs](https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#cuda-graphs)
- *TVM Deep Dives → Lecture 05 — Shipping it: MLC-LLM and dlight*——[../../../7. ML Systems Engineering/TVM Deep Dives/Lecture-05.md](../../07-方向G-ML系统工程/06-TVM深度探索/Lecture-05.md)

---


<details>
<summary>English original</summary>

**7. Runtime selection matrix**

```text
                    │ LiteRT     │ llama.cpp   │ MLC-LLM    │ TRT-LLM
────────────────────┼────────────┼─────────────┼────────────┼────────────
Peak CUDA tok/s     │ ~~         │ ✓ (good)    │ ✓ (good)   │ ✓✓ (best)
Jetson DLA support  │ ✓✓ (best)  │ ✗           │ ✗          │ limited
Cross-platform      │ ✓✓ (best)  │ ✓ (GGUF)    │ ✓ (TVM)    │ ✗ (NVIDIA only)
Compilation needed  │ optional   │ ✗ (zero)    │ ✓ (TVM)    │ ✓✓ (full TRT)
GGUF format         │ ✗          │ ✓✓ (native) │ ✗          │ ✗
Google AI Edge SDK  │ ✓✓ (native)│ ✗           │ ✗          │ ✗
Paged attention     │ limited    │ ✓ (added)   │ ✓ (native) │ ✓✓ (best)
CUDA graphs         │ ✗          │ ✗ (default) │ limited     │ ✓✓ (native)
Continuous batching │ ✗          │ ✓ (server)  │ ✓ (server) │ ✓✓ (native)
Ease of deployment  │ ✓✓         │ ✓✓          │ ✓           │ ✓ (complex)
Community support   │ ✓ (Google) │ ✓✓ (best)   │ ✓           │ ✓ (NVIDIA)
Best for            │ DLA/mobile │ general use │ tuned CUDA │ peak prod
```

**Decision rules:**

```text
USE LiteRT when:
  ├─ Deploying to Jetson DLA + Android + iOS from one model file
  ├─ Power budget forces DLA usage (lower watt-per-token than CUDA)
  └─ Regulatory / certification requires portable binary

USE llama.cpp when:
  ├─ Quick prototyping, zero compilation
  ├─ GGUF swap without recompile (testing different quant levels)
  └─ Community ecosystem (many tools, easy debugging)

USE MLC-LLM when:
  ├─ Want TVM-tuned kernels without TRT-LLM complexity
  ├─ Need Vulkan/WebGPU backend (e.g., cross-platform embedded)
  └─ Using MLC-LLM's Python API for streaming production service

USE TRT-LLM when:
  ├─ Production service on Jetson Orin/Thor with CUDA
  ├─ Need paged KV + continuous batching + CUDA graphs together
  └─ Throughput is the primary metric and compilation cost is acceptable
```

---

**8. Profiling your runtime on Jetson**

Regardless of runtime, the profiling workflow is:

```bash
# 1. nsys for timeline (which kernels fire, any idle gaps):
nsys profile --trace=cuda,nvtx \
    ./your_inference_command \
    --output gemma4_profile

nsys stats gemma4_profile.nsys-rep

# 2. ncu for per-kernel metrics (achieved BW, SM utilization):
ncu --target-processes all \
    --metrics \
        gpu__compute_memory_throughput.avg.pct_of_peak_sustained_elapsed,\
        sm__throughput.avg.pct_of_peak_sustained_elapsed \
    ./your_inference_command

# 3. What to look for:
#  - Decode step: gpu__compute_memory_throughput > 60% → good (BW-bound, expected)
#  - Long idle gaps between kernels → CUDA-graph capture would help
#  - Low BW utilization → kernel choice is wrong (GEMM vs GEMV mismatch)
```

---

**Key takeaways**

- **Four runtimes, four tradeoffs**: LiteRT for portability/DLA, llama.cpp for simplicity, MLC-LLM for TVM-tuned CUDA, TRT-LLM for peak production.
- **TRT-LLM is fastest on Jetson CUDA**: 70–90 tok/s (4B, Orin) vs 45–65 (llama.cpp) due to fused projections, INT4 GEMM tuning, and CUDA graphs.
- **Paged attention** is critical for 128K context on Jetson: it turns a 1.35 GB per-request KV reservation into on-demand page allocation, enabling 20–40× more concurrent slots at typical conversation lengths.
- **CUDA graphs** eliminate kernel-launch overhead (300–500 µs per decode step on Orin) — TRT-LLM uses them natively; llama.cpp can be patched to use them.
- **MLC-LLM's dlight schedules** give ~10–15% better GEMV throughput than llama.cpp's hand-written kernels on Gemma 4, because dlight selects tile sizes based on SM architecture.
- **LiteRT on Jetson CUDA is 3–5× slower** than CUDA runtimes — use it only when DLA or cross-platform portability justifies the throughput cost.

---

**References**

- llama.cpp (GGUF, CUDA backend, llama-server) — [github.com/ggml-org/llama.cpp](https://github.com/ggml-org/llama.cpp)
- MLC-LLM (TVM Unity, dlight schedules, cross-platform) — [github.com/mlc-ai/mlc-llm](https://github.com/mlc-ai/mlc-llm)
- TensorRT-LLM (CUDA graphs, paged KV, continuous batching) — [github.com/NVIDIA/TensorRT-LLM](https://github.com/NVIDIA/TensorRT-LLM)
- Google AI Edge LiteRT documentation — [ai.google.dev/edge/litert](https://ai.google.dev/edge/litert)
- vLLM PagedAttention paper (Kwon et al., 2023) — arXiv:2309.06180
- NVIDIA CUDA graphs documentation — [docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#cuda-graphs](https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#cuda-graphs)
- *TVM Deep Dives → Lecture 05 — Shipping it: MLC-LLM and dlight* — [../../../7. ML Systems Engineering/TVM Deep Dives/Lecture-05.md](../../07-方向G-ML系统工程/06-TVM深度探索/Lecture-05.md)

---

</details>

## 截至 2026-06

llama.cpp b3500+ 支持 Gemma 4；MLC-LLM v0.17+ 支持 Orin/Thor 上的 q4f16_1；TensorRT-LLM 0.14+；LiteRT 2.16+。Jetson JetPack 6.x 是 sm_87（Orin）CUDA 12.x 支持所必需的；JetPack 7.x 用于 Thor sm_90。构建 engine 前务必确认 `trtllm-build` 支持 Gemma 4 的 interleaved attention（sliding-window）layer——不支持的 attention 模式可能静默回退到 global attention，导致 KV cache 膨胀。

---

*Previous: [← Lecture 02](Lecture-02.md) · Up: [Gemma 4 Edge Deployment](README.md) · Next: [Lecture 04 — Speculative Decoding with Gemma 4](Lecture-04.md)*


<details>
<summary>English original</summary>

**Current as of 2026-06**

llama.cpp b3500+ for Gemma 4 support; MLC-LLM v0.17+ for q4f16_1 on Orin/Thor; TensorRT-LLM 0.14+; LiteRT 2.16+. Jetson JetPack 6.x required for sm_87 (Orin) CUDA 12.x support; JetPack 7.x for Thor sm_90. Always verify `trtllm-build` supports Gemma 4's interleaved attention (sliding-window) layers before building an engine — unsupported attention patterns may silently fall back to global attention, inflating KV cache.

---

*Previous: [← Lecture 02](Lecture-02.md) · Up: [Gemma 4 Edge Deployment](README.md) · Next: [Lecture 04 — Speculative Decoding with Gemma 4](Lecture-04.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track C - Edge AI/Gemma 4 Edge Deployment/Lecture-03.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20C%20-%20Edge%20AI/Gemma%204%20Edge%20Deployment/Lecture-03.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
