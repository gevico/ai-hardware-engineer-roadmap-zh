---
title: Jetson 上的大语言模型优化 —— 从云端技术到边缘现实
description: Jetson 上的大语言模型优化 —— 从云端技术到边缘现实
---

# Jetson 上的大语言模型优化 —— 从云端技术到边缘现实

<div class="course-identity auto-course" style="--course-accent: #7c3aed; --course-accent-rgb: 124, 58, 237;" markdown="1">
<div class="course-identity__icon">LOOJ</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度剖析 · Jetson Track</p>
<p class="course-identity__title">「Jetson 上的大语言模型优化 —— 从云端技术到边缘现实」的专属课程标识。</p>
<p class="course-identity__meta">产物：Jetson 集成演示 · 测量：延迟、内存、功耗、日志</p>
</div>
</div>


**父级：** [机器学习与 AI](../Guide.md)

> **目标：** 把云端大语言模型平台（vLLM、TensorRT-LLM、RunInfra 等）所用的优化技术，适配到 Jetson Orin Nano Super 8 GB 的极端约束下 —— 102 GB/s 带宽、67 TOPS GPU + 约 10 TOPS DLA（深度学习加速器）≈ 77 TOPS 总量、共享内存、7–25W 功耗。

---

## 预检：启动前的系统检查

在开展任何大语言模型工作之前，先验证 Jetson 的软件栈。**JetPack 版本决定了你拥有哪些 CUDA、TensorRT 和 cuDNN 版本** —— 以及哪些大语言模型工具与之兼容。

### 快速系统审计（复制粘贴此内容）

```bash
#!/bin/bash
echo "═══════════════════════════════════════════════"
echo "  Jetson System Audit for LLM Deployment"
echo "═══════════════════════════════════════════════"

echo ""
echo "▸ JetPack / L4T version:"
cat /etc/nv_tegra_release 2>/dev/null || echo "  (not found — check dpkg)"
dpkg-query --show nvidia-l4t-core 2>/dev/null | awk '{print "  L4T:", $2}'

echo ""
echo "▸ CUDA version:"
nvcc --version 2>/dev/null | grep release || echo "  nvcc not found"

echo ""
echo "▸ TensorRT version:"
dpkg -l | grep tensorrt | head -1 | awk '{print " ", $3}'

echo ""
echo "▸ cuDNN version:"
dpkg -l | grep cudnn | head -1 | awk '{print " ", $3}'

echo ""
echo "▸ Python version:"
python3 --version

echo ""
echo "▸ Total RAM:"
free -m | awk '/Mem:/ {print "  " $2 " MB total"}'
echo "▸ Free RAM:"
free -m | awk '/Mem:/ {print "  " $7 " MB available"}'

echo ""
echo "▸ CMA allocation:"
grep Cma /proc/meminfo | awk '{print "  " $0}'

echo ""
echo "▸ GPU info:"
cat /sys/devices/17000000.ga10b/devfreq/17000000.ga10b/cur_freq 2>/dev/null \
    | awk '{print "  GPU freq: " $1/1000000 " MHz"}'

echo ""
echo "▸ Power mode:"
sudo nvpmodel -q 2>/dev/null | head -2 | sed 's/^/  /'

echo ""
echo "▸ Disk space:"
df -h / | tail -1 | awk '{print "  Root: " $4 " free of " $2}'
df -h /dev/nvme0n1p1 2>/dev/null | tail -1 | awk '{print "  NVMe: " $4 " free of " $2}'

echo ""
echo "▸ Thermal:"
cat /sys/devices/virtual/thermal/thermal_zone*/temp 2>/dev/null | head -3 \
    | awk '{print "  Zone: " $1/1000 "°C"}'

echo "═══════════════════════════════════════════════"
```

**示例输出（Orin Nano Super，JetPack 6.1）：**

```
═══════════════════════════════════════════════
  Jetson System Audit for LLM Deployment
═══════════════════════════════════════════════

▸ JetPack / L4T version:
  # R36 (release), REVISION: 4.0
  L4T: 36.4.0-20241031080721

▸ CUDA version:
  Cuda compilation tools, release 12.6, V12.6.77

▸ TensorRT version:
  10.3.0.30-1+cuda12.6

▸ cuDNN version:
  9.3.0.75-1+cuda12.6

▸ Python version:
  Python 3.10.12

▸ Total RAM:
  7633 MB total
▸ Free RAM:
  5814 MB available

▸ CMA allocation:
  CmaTotal:      786432 kB
  CmaFree:       654321 kB

▸ GPU info:
  GPU freq: 624 MHz

▸ Power mode:
  NV Power Mode: MAXN
  Power Mode: 25W

▸ Disk space:
  Root: 42G free of 100G

▸ Thermal:
  Zone: 38.5°C

═══════════════════════════════════════════════
```

### JetPack → CUDA → TensorRT 兼容性矩阵

此矩阵决定了哪些大语言模型工具能在你的 Jetson 上运行：

| JetPack | L4T | CUDA | TensorRT | cuDNN | Python | llama.cpp | Ollama | TRT-LLM |
|---------|-----|------|----------|-------|--------|-----------|--------|---------|
| **6.1** | R36.4 | **12.6** | **10.3** | 9.3 | 3.10 | 是 | 是 | 是（0.15+） |
| **6.0** | R36.3 | **12.2** | **8.6** | 8.9 | 3.10 | 是 | 是 | 是（0.9+） |
| 5.1.3 | R35.5 | 11.4 | 8.5 | 8.6 | 3.8 | 是 | 是 | 受限 |
| 5.1.1 | R35.3 | 11.4 | 8.5 | 8.6 | 3.8 | 是 | 较旧 | 否 |
| 5.0.2 | R35.1 | 11.4 | 8.4 | 8.4 | 3.8 | 是 | 否 | 否 |

> **建议：** 用 **JetPack 6.1**（R36.4）做大语言模型工作。它带有 CUDA 12.6、TensorRT 10.3 和完整的 TensorRT-LLM 支持。如果你在 JetPack 5.x 上，考虑升级 —— CUDA 12.x 生态（llama.cpp、Ollama、PyTorch 2.x）要好得多。


<details>
<summary>English original</summary>

**LLM Optimization on Jetson — From Cloud Techniques to Edge Reality**

<div class="course-identity auto-course" style="--course-accent: #7c3aed; --course-accent-rgb: 124, 58, 237;" markdown="1">
<div class="course-identity__icon">LOOJ</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Jetson Track</p>
<p class="course-identity__title">Specialized course identity for LLM Optimization on Jetson — From Cloud Techniques to Edge Reality.</p>
<p class="course-identity__meta">Artifact: Jetson integration demo · Measure: latency, memory, power, logs</p>
</div>
</div>


**Parent:** [ML and AI](../Guide.md)

> **Goal:** Take the optimization techniques used by cloud LLM platforms (vLLM, TensorRT-LLM, RunInfra, etc.) and adapt them to the extreme constraints of Jetson Orin Nano Super 8 GB — 102 GB/s bandwidth, 67 TOPS GPU + ~10 TOPS DLA ≈ 77 TOPS total, shared memory, 7–25W power.

---

**Pre-Flight: System Check Before Starting**

Before any LLM work, verify your Jetson's software stack. **JetPack version determines which CUDA, TensorRT, and cuDNN versions you have** — and which LLM tools are compatible.

**Quick System Audit (copy-paste this)**

```bash
#!/bin/bash
echo "═══════════════════════════════════════════════"
echo "  Jetson System Audit for LLM Deployment"
echo "═══════════════════════════════════════════════"

echo ""
echo "▸ JetPack / L4T version:"
cat /etc/nv_tegra_release 2>/dev/null || echo "  (not found — check dpkg)"
dpkg-query --show nvidia-l4t-core 2>/dev/null | awk '{print "  L4T:", $2}'

echo ""
echo "▸ CUDA version:"
nvcc --version 2>/dev/null | grep release || echo "  nvcc not found"

echo ""
echo "▸ TensorRT version:"
dpkg -l | grep tensorrt | head -1 | awk '{print " ", $3}'

echo ""
echo "▸ cuDNN version:"
dpkg -l | grep cudnn | head -1 | awk '{print " ", $3}'

echo ""
echo "▸ Python version:"
python3 --version

echo ""
echo "▸ Total RAM:"
free -m | awk '/Mem:/ {print "  " $2 " MB total"}'
echo "▸ Free RAM:"
free -m | awk '/Mem:/ {print "  " $7 " MB available"}'

echo ""
echo "▸ CMA allocation:"
grep Cma /proc/meminfo | awk '{print "  " $0}'

echo ""
echo "▸ GPU info:"
cat /sys/devices/17000000.ga10b/devfreq/17000000.ga10b/cur_freq 2>/dev/null \
    | awk '{print "  GPU freq: " $1/1000000 " MHz"}'

echo ""
echo "▸ Power mode:"
sudo nvpmodel -q 2>/dev/null | head -2 | sed 's/^/  /'

echo ""
echo "▸ Disk space:"
df -h / | tail -1 | awk '{print "  Root: " $4 " free of " $2}'
df -h /dev/nvme0n1p1 2>/dev/null | tail -1 | awk '{print "  NVMe: " $4 " free of " $2}'

echo ""
echo "▸ Thermal:"
cat /sys/devices/virtual/thermal/thermal_zone*/temp 2>/dev/null | head -3 \
    | awk '{print "  Zone: " $1/1000 "°C"}'

echo "═══════════════════════════════════════════════"
```

**Example output (Orin Nano Super, JetPack 6.1):**

```
═══════════════════════════════════════════════
  Jetson System Audit for LLM Deployment
═══════════════════════════════════════════════

▸ JetPack / L4T version:
  # R36 (release), REVISION: 4.0
  L4T: 36.4.0-20241031080721

▸ CUDA version:
  Cuda compilation tools, release 12.6, V12.6.77

▸ TensorRT version:
  10.3.0.30-1+cuda12.6

▸ cuDNN version:
  9.3.0.75-1+cuda12.6

▸ Python version:
  Python 3.10.12

▸ Total RAM:
  7633 MB total
▸ Free RAM:
  5814 MB available

▸ CMA allocation:
  CmaTotal:      786432 kB
  CmaFree:       654321 kB

▸ GPU info:
  GPU freq: 624 MHz

▸ Power mode:
  NV Power Mode: MAXN
  Power Mode: 25W

▸ Disk space:
  Root: 42G free of 100G

▸ Thermal:
  Zone: 38.5°C

═══════════════════════════════════════════════
```

**JetPack → CUDA → TensorRT Compatibility Matrix**

This matrix determines which LLM tools work on your Jetson:

| JetPack | L4T | CUDA | TensorRT | cuDNN | Python | llama.cpp | Ollama | TRT-LLM |
|---------|-----|------|----------|-------|--------|-----------|--------|---------|
| **6.1** | R36.4 | **12.6** | **10.3** | 9.3 | 3.10 | Yes | Yes | Yes (0.15+) |
| **6.0** | R36.3 | **12.2** | **8.6** | 8.9 | 3.10 | Yes | Yes | Yes (0.9+) |
| 5.1.3 | R35.5 | 11.4 | 8.5 | 8.6 | 3.8 | Yes | Yes | Limited |
| 5.1.1 | R35.3 | 11.4 | 8.5 | 8.6 | 3.8 | Yes | Older | No |
| 5.0.2 | R35.1 | 11.4 | 8.4 | 8.4 | 3.8 | Yes | No | No |

> **Recommendation:** Use **JetPack 6.1** (R36.4) for LLM work. It has CUDA 12.6, TensorRT 10.3, and full TensorRT-LLM support. If you're on JetPack 5.x, consider upgrading — the CUDA 12.x ecosystem (llama.cpp, Ollama, PyTorch 2.x) is significantly better.

</details>

### 前置检查清单

```
Before deploying any LLM on Jetson:

□ JetPack version
  □ JetPack 6.0+ for TensorRT-LLM
  □ JetPack 5.1+ minimum for llama.cpp / Ollama

□ Available memory
  □ Run: free -m → note "available" column
  □ Expect ~5.5–6 GB free on stock Orin Nano Super 8 GB
  □ If < 5 GB: disable GUI (sudo systemctl set-default multi-user.target)
  □ If < 4 GB: reduce CMA, disable unnecessary services

□ Storage
  □ NVMe recommended (models are 1–5 GB each)
  □ SD card works but slower model loading
  □ At least 20 GB free for models + build cache

□ Power mode
  □ sudo nvpmodel -m 0  (MAXN = 25W for Orin Nano Super)
  □ sudo jetson_clocks   (lock to max frequency)

□ Thermal
  □ Active cooling attached (fan or heatsink with fan)
  □ Ambient temperature < 35°C for sustained workloads
  □ Monitor: tegrastats --interval 1000
```

### 禁用 GUI 以释放约 500 MB 内存

对于无头 LLM 部署，禁用桌面环境：

```bash
# Switch to text-only mode (saves ~500 MB RAM)
sudo systemctl set-default multi-user.target
sudo reboot

# To re-enable GUI later:
sudo systemctl set-default graphical.target
sudo reboot

# Verify RAM freed:
free -m   # "available" should increase by ~500 MB
```

仅此一项改动，就可能决定 3B 模型是舒适放下，还是内存耗尽。

---

## 0. 优化栈 —— 云端 vs Jetson

RunInfra、Together AI、Fireworks AI 等云平台采用分层优化栈来部署 LLM。每种技术在 Jetson 上都有对应做法——但优先级正好颠倒，因为 Jetson 严重受限于内存带宽，而非算力受限。

```
Cloud GPU (H100 80GB, 3,350 GB/s, 989 TFLOPS):
  ┌─────────────────────────────────────────────┐
  │ 1. Quantization (FP8, AWQ 4-bit)           │ ← saves VRAM, improves throughput
  │ 2. FlashAttention-2                         │ ← saves SRAM, fuses memory ops
  │ 3. Fused Kernels (RMSNorm, rotary, SwiGLU) │ ← fewer kernel launches
  │ 4. PagedAttention (vLLM)                    │ ← KV cache memory efficiency
  │ 5. Speculative Decoding                     │ ← higher tokens/sec
  │ 6. Batching (continuous batching)           │ ← amortize compute over requests
  │ 7. Tensor Parallelism (multi-GPU)          │ ← scale beyond 1 GPU
  └─────────────────────────────────────────────┘

Jetson Orin Nano Super 8GB (102 GB/s, 67 TOPS GPU + ~10 TOPS DLA ≈ 77 TOPS):
  ┌─────────────────────────────────────────────┐
  │ 1. Quantization (INT4/INT8) ★★★★★          │ ← MANDATORY: model must fit in 5 GB
  │ 2. Model Selection ★★★★★                    │ ← choose models that fit (≤3B params)
  │ 3. KV Cache Management ★★★★                │ ← memory is the #1 constraint
  │ 4. FlashAttention / Fused Ops ★★★★         │ ← reduce bandwidth pressure
  │ 5. TensorRT-LLM Engine ★★★                 │ ← compiled, optimized execution
  │ 6. Speculative Decoding ★★★                │ ← higher tokens/sec within power budget
  │ 7. Batching ★★                              │ ← limited by memory, not compute
  └─────────────────────────────────────────────┘
  ★ = importance on Jetson (more ★ = more critical)
```

---

## 1. 量化 —— 最重要的优化

在云端 GPU 上，量化是可选项（节省成本）。在 Jetson 上，**量化是强制要求**——没有它，什么都放不下。

### 1.1 为什么量化在 Jetson 上更重要

```
Model: Llama 3.2 3B parameters

FP16:  3B × 2 bytes = 6.0 GB   ← won't fit (only ~5 GB free after OS/CMA)
INT8:  3B × 1 byte  = 3.0 GB   ← fits, but tight
INT4:  3B × 0.5 byte = 1.5 GB  ← fits comfortably, room for KV cache

Model: Phi-3 Mini 3.8B parameters

FP16:  3.8B × 2 bytes = 7.6 GB  ← impossible on 8 GB
INT4:  3.8B × 0.5 byte = 1.9 GB ← fits with room for context
```

### 1.2 适用于 Jetson 的量化方法排名

| 方法 | 位宽 | 质量损失 | Jetson 上的速度 | 适用场景 |
|--------|------|-------------|-----------------|-------------|
| **AWQ (Activation-Aware Weight)** | 4-bit | 极低 | 快（INT4 GEMM） | Jetson 上质量/体积比最佳 |
| **GPTQ** | 4-bit | 低 | 快 | AWQ 的替代方案，支持良好 |
| **INT8 训练后量化 (TensorRT)** | 8-bit | 极小 | 最快 | 模型能以 INT8 放下时 |
| **FP8 (E4M3)** | 8-bit | 极小 | 快（Ampere 架构及以上） | 需要接近 FP 的质量时 |
| **GGUF (llama.cpp)** | 2–8 bit | 混合精度 | 好（CPU+GPU） | 部署简单，任意模型 |
| **SqueezeLLM** | 3-4 bit | 低 | 中等 | 极致压缩 |


<details>
<summary>English original</summary>

**Pre-Flight Checklist**

```
Before deploying any LLM on Jetson:

□ JetPack version
  □ JetPack 6.0+ for TensorRT-LLM
  □ JetPack 5.1+ minimum for llama.cpp / Ollama

□ Available memory
  □ Run: free -m → note "available" column
  □ Expect ~5.5–6 GB free on stock Orin Nano Super 8 GB
  □ If < 5 GB: disable GUI (sudo systemctl set-default multi-user.target)
  □ If < 4 GB: reduce CMA, disable unnecessary services

□ Storage
  □ NVMe recommended (models are 1–5 GB each)
  □ SD card works but slower model loading
  □ At least 20 GB free for models + build cache

□ Power mode
  □ sudo nvpmodel -m 0  (MAXN = 25W for Orin Nano Super)
  □ sudo jetson_clocks   (lock to max frequency)

□ Thermal
  □ Active cooling attached (fan or heatsink with fan)
  □ Ambient temperature < 35°C for sustained workloads
  □ Monitor: tegrastats --interval 1000
```

**Disable GUI to Free ~500 MB RAM**

For headless LLM deployment, disable the desktop environment:

```bash
# Switch to text-only mode (saves ~500 MB RAM)
sudo systemctl set-default multi-user.target
sudo reboot

# To re-enable GUI later:
sudo systemctl set-default graphical.target
sudo reboot

# Verify RAM freed:
free -m   # "available" should increase by ~500 MB
```

This single change can be the difference between a 3B model fitting comfortably and running out of memory.

---

**0. The Optimization Stack — Cloud vs Jetson**

Cloud platforms like RunInfra, Together AI, and Fireworks AI deploy LLMs using a layered optimization stack. Every technique has a Jetson equivalent — but the priorities are reversed because Jetson is severely memory-bandwidth-bound rather than compute-bound.

```
Cloud GPU (H100 80GB, 3,350 GB/s, 989 TFLOPS):
  ┌─────────────────────────────────────────────┐
  │ 1. Quantization (FP8, AWQ 4-bit)           │ ← saves VRAM, improves throughput
  │ 2. FlashAttention-2                         │ ← saves SRAM, fuses memory ops
  │ 3. Fused Kernels (RMSNorm, rotary, SwiGLU) │ ← fewer kernel launches
  │ 4. PagedAttention (vLLM)                    │ ← KV cache memory efficiency
  │ 5. Speculative Decoding                     │ ← higher tokens/sec
  │ 6. Batching (continuous batching)           │ ← amortize compute over requests
  │ 7. Tensor Parallelism (multi-GPU)          │ ← scale beyond 1 GPU
  └─────────────────────────────────────────────┘

Jetson Orin Nano Super 8GB (102 GB/s, 67 TOPS GPU + ~10 TOPS DLA ≈ 77 TOPS):
  ┌─────────────────────────────────────────────┐
  │ 1. Quantization (INT4/INT8) ★★★★★          │ ← MANDATORY: model must fit in 5 GB
  │ 2. Model Selection ★★★★★                    │ ← choose models that fit (≤3B params)
  │ 3. KV Cache Management ★★★★                │ ← memory is the #1 constraint
  │ 4. FlashAttention / Fused Ops ★★★★         │ ← reduce bandwidth pressure
  │ 5. TensorRT-LLM Engine ★★★                 │ ← compiled, optimized execution
  │ 6. Speculative Decoding ★★★                │ ← higher tokens/sec within power budget
  │ 7. Batching ★★                              │ ← limited by memory, not compute
  └─────────────────────────────────────────────┘
  ★ = importance on Jetson (more ★ = more critical)
```

---

**1. Quantization — The Most Important Optimization**

On cloud GPUs, quantization is optional (saves cost). On Jetson, **quantization is mandatory** — without it, nothing fits.

**1.1 Why Quantization Matters More on Jetson**

```
Model: Llama 3.2 3B parameters

FP16:  3B × 2 bytes = 6.0 GB   ← won't fit (only ~5 GB free after OS/CMA)
INT8:  3B × 1 byte  = 3.0 GB   ← fits, but tight
INT4:  3B × 0.5 byte = 1.5 GB  ← fits comfortably, room for KV cache

Model: Phi-3 Mini 3.8B parameters

FP16:  3.8B × 2 bytes = 7.6 GB  ← impossible on 8 GB
INT4:  3.8B × 0.5 byte = 1.9 GB ← fits with room for context
```

**1.2 Quantization Methods Ranked for Jetson**

| Method | Bits | Quality loss | Speed on Jetson | When to use |
|--------|------|-------------|-----------------|-------------|
| **AWQ (Activation-Aware Weight)** | 4-bit | Very low | Fast (INT4 GEMM) | Best quality/size for Jetson |
| **GPTQ** | 4-bit | Low | Fast | Alternative to AWQ, well-supported |
| **INT8 PTQ (TensorRT)** | 8-bit | Minimal | Fastest | If model fits at INT8 |
| **FP8 (E4M3)** | 8-bit | Minimal | Fast (Ampere+) | When you need FP-like quality |
| **GGUF (llama.cpp)** | 2–8 bit | Mixed-precision | Good (CPU+GPU) | Easy deployment, any model |
| **SqueezeLLM** | 3-4 bit | Low | Moderate | Extreme compression |

</details>

### 1.3 AWQ 量化（推荐用于 Jetson）

AWQ 通过保护显著权重通道来保持质量——对输出质量最重要的 1% 权重。

```python
# Quantize on your workstation (not on Jetson — too slow)
from awq import AutoAWQForCausalLM
from transformers import AutoTokenizer

model_name = "microsoft/Phi-3-mini-4k-instruct"
quant_path = "phi3-mini-awq-int4"

model = AutoAWQForCausalLM.from_pretrained(model_name)
tokenizer = AutoTokenizer.from_pretrained(model_name)

# AWQ calibration (needs ~128 samples)
model.quantize(
    tokenizer,
    quant_config={
        "zero_point": True,
        "q_group_size": 128,
        "w_bit": 4,           # 4-bit weights
        "version": "GEMM"     # optimized GEMM kernel
    }
)
model.save_quantized(quant_path)
tokenizer.save_pretrained(quant_path)
```

### 1.4 基于 llama.cpp 的 GGUF（最简路径）

llama.cpp 在 Jetson 上借助 CUDA 支持运行，并处理混合精度量化：

```bash
# On Jetson: install llama.cpp with CUDA
git clone https://github.com/ggerganov/llama.cpp
cd llama.cpp
cmake -B build -DGGML_CUDA=ON
cmake --build build --config Release -j$(nproc)

# Download a pre-quantized GGUF model
# (Llama 3.2 3B in Q4_K_M = ~2 GB, good quality/size balance)
wget https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/\
Llama-3.2-3B-Instruct-Q4_K_M.gguf

# Run inference
./build/bin/llama-cli \
    -m Llama-3.2-3B-Instruct-Q4_K_M.gguf \
    -ngl 99 \           # offload all layers to GPU
    -c 2048 \           # context length
    -p "Explain how Jetson unified memory works:"
```

**Jetson 的 GGUF 量化级别：**

| 量化 | 位宽 | 大小（3B 模型） | 质量 | 是否推荐？ |
|-------------|------|-----------------|---------|-------------|
| Q2_K | 2.6 | ~1.0 GB | 差 | 仅当别无选择时 |
| Q3_K_M | 3.4 | ~1.3 GB | 可接受 | 内存极度受限的部署 |
| **Q4_K_M** | **4.5** | **~1.8 GB** | **良好** | **Jetson 的最佳平衡** |
| Q5_K_M | 5.5 | ~2.2 GB | 很好 | 内存允许时 |
| Q6_K | 6.6 | ~2.5 GB | 极佳 | 可容纳的最佳质量 |
| Q8_0 | 8.0 | ~3.0 GB | 接近 FP16 | 仅用于小模型 |

---

## 2. 模型选择——完整图景

最重要的“优化”是选择合适的模型。量化良好的小模型每次都胜过适配不佳的大模型。

### 2.1 完整模型目录（Q4_K_M 量化，Orin Nano Super 8 GB）

所有值均为 Q4_K_M——一种 4-bit 混合精度量化，可将内存减少约 75%，且质量损失极小。

**Tier 1——运行从容（< 3 GB，可为长上下文 + KV cache 留出空间）**

| 模型 | 参数量 | Q4_K_M 大小 | 上下文 | 生成 tok/s（估计） | 最适合 |
|-------|--------|-------------|---------|-------------------|----------|
| **TinyLlama 1.1B** | 1.1B | 0.6 GB | 2K | ~65 | 超轻量，用于投机解码的草稿模型 |
| **Llama 3.2 1B** | 1.3B | 0.7 GB | 128K | ~55 | 轻量聊天、分类、工具调用 |
| **StableLM 2 1.6B** | 1.6B | 0.9 GB | 4K | ~45 | 紧凑、快速的边缘助手 |
| **Gemma 3 1B** | 1B | 0.6 GB | 32K | ~60 | 多语言、Google 生态 |
| **Gemma 2 2B** | 2.6B | 1.5 GB | 8K | ~35 | 多语言、通用 |
| **Qwen 3 1.7B** | 1.7B | 1.0 GB | 32K | ~50 | 中英文、思考模式 |
| **SmolLM2 1.7B** | 1.7B | 1.0 GB | 8K | ~50 | Hugging Face、紧凑、训练良好 |
| **Llama 3.2 3B** | 3.2B | 1.8 GB | 128K | ~25 | 通用聊天、摘要、工具使用 |
| **Qwen 2.5 3B** | 3B | 1.7 GB | 32K | ~28 | 双语通用 |

**Tier 2——放得下但紧张（3–4.5 GB，建议使用更短上下文）**

| 模型 | 参数量 | Q4_K_M 大小 | 上下文 | 生成 tok/s（估计） | 最适合 |
|-------|--------|-------------|---------|-------------------|----------|
| **Phi-4 Mini** | 3.8B | 2.3 GB | 4K/128K | ~20 | 推理、数学、代码 |
| **Phi-3 Mini** | 3.8B | 2.2 GB | 4K/128K | ~20 | 推理、代码 |
| **Qwen 3 4B** | 4B | 2.4 GB | 32K | ~18 | 思考 + 非思考模式 |
| **Gemma 3 4B** | 4B | 2.5 GB | 128K | ~17 | 视觉 + 语言（多模态） |
| **Llama 3.3 8B** | 8B | 4.6 GB | 128K | ~10 | 通用（需要短上下文） |
| **Mistral 7B v0.3** | 7B | 4.1 GB | 32K | ~11 | 聊天、函数调用 |
| **Qwen 3 8B** | 8B | 4.9 GB | 32K | ~9 | 双语、思考模式 |

**Tier 3——勉强放得下（4.5+ GB，严重受限）**

| 模型 | 参数量 | Q4_K_M 大小 | 最大上下文 | 生成 tok/s（估计） | 备注 |
|-------|--------|-------------|---------|-------------------|-------|
| **Llama 3.1 8B** | 8B | 4.7 GB | 512–1K | ~9 | 可用但局促 |
| **Mistral Nemo 12B** | 12B | 7.0 GB | 256 | ~5 | 需要 Q3_K 或 Q2_K 才能放下 |
| **Phi-4 14B** | 14B | 8.4 GB | — | 放不下 | 使用 Orin NX 16 GB |

> **经验法则：** 模型 Q4_K_M 大小 + 2.5 GB（OS/CMA/CUDA）+ KV cache 必须 < 8 GB。对于 Tier 2 模型，将上下文限制在 1–2K token。对于 Tier 3，考虑 Q3_K_M 或 Q2_K 量化。


<details>
<summary>English original</summary>

**1.3 AWQ Quantization (Recommended for Jetson)**

AWQ preserves quality by protecting salient weight channels — the 1% of weights that matter most for output quality.

```python
# Quantize on your workstation (not on Jetson — too slow)
from awq import AutoAWQForCausalLM
from transformers import AutoTokenizer

model_name = "microsoft/Phi-3-mini-4k-instruct"
quant_path = "phi3-mini-awq-int4"

model = AutoAWQForCausalLM.from_pretrained(model_name)
tokenizer = AutoTokenizer.from_pretrained(model_name)

# AWQ calibration (needs ~128 samples)
model.quantize(
    tokenizer,
    quant_config={
        "zero_point": True,
        "q_group_size": 128,
        "w_bit": 4,           # 4-bit weights
        "version": "GEMM"     # optimized GEMM kernel
    }
)
model.save_quantized(quant_path)
tokenizer.save_pretrained(quant_path)
```

**1.4 GGUF with llama.cpp (Easiest Path)**

llama.cpp runs on Jetson with CUDA support and handles mixed-precision quantization:

```bash
# On Jetson: install llama.cpp with CUDA
git clone https://github.com/ggerganov/llama.cpp
cd llama.cpp
cmake -B build -DGGML_CUDA=ON
cmake --build build --config Release -j$(nproc)

# Download a pre-quantized GGUF model
# (Llama 3.2 3B in Q4_K_M = ~2 GB, good quality/size balance)
wget https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/\
Llama-3.2-3B-Instruct-Q4_K_M.gguf

# Run inference
./build/bin/llama-cli \
    -m Llama-3.2-3B-Instruct-Q4_K_M.gguf \
    -ngl 99 \           # offload all layers to GPU
    -c 2048 \           # context length
    -p "Explain how Jetson unified memory works:"
```

**GGUF quantization levels for Jetson:**

| Quantization | Bits | Size (3B model) | Quality | Recommended? |
|-------------|------|-----------------|---------|-------------|
| Q2_K | 2.6 | ~1.0 GB | Poor | Only if nothing else fits |
| Q3_K_M | 3.4 | ~1.3 GB | Acceptable | Memory-critical deployments |
| **Q4_K_M** | **4.5** | **~1.8 GB** | **Good** | **Best balance for Jetson** |
| Q5_K_M | 5.5 | ~2.2 GB | Very good | If memory allows |
| Q6_K | 6.6 | ~2.5 GB | Excellent | Best quality that fits |
| Q8_0 | 8.0 | ~3.0 GB | Near-FP16 | Only for small models |

---

**2. Model Selection — The Complete Landscape**

The most important "optimization" is choosing the right model. A well-quantized small model beats a poorly-fitting large model every time.

**2.1 Full Model Catalog (Q4_K_M Quantization, Orin Nano Super 8 GB)**

All values at Q4_K_M — a 4-bit mixed-precision quantization that reduces memory by ~75% with minimal quality loss.

**Tier 1 — Runs comfortably (< 3 GB, room for long context + KV cache)**

| Model | Params | Q4_K_M size | Context | Gen. tok/s (est.) | Best for |
|-------|--------|-------------|---------|-------------------|----------|
| **TinyLlama 1.1B** | 1.1B | 0.6 GB | 2K | ~65 | Ultra-lightweight, draft model for speculative decoding |
| **Llama 3.2 1B** | 1.3B | 0.7 GB | 128K | ~55 | Lightweight chat, classification, tool calling |
| **StableLM 2 1.6B** | 1.6B | 0.9 GB | 4K | ~45 | Compact, fast edge assistant |
| **Gemma 3 1B** | 1B | 0.6 GB | 32K | ~60 | Multilingual, Google ecosystem |
| **Gemma 2 2B** | 2.6B | 1.5 GB | 8K | ~35 | Multilingual, general purpose |
| **Qwen 3 1.7B** | 1.7B | 1.0 GB | 32K | ~50 | Chinese + English, thinking mode |
| **SmolLM2 1.7B** | 1.7B | 1.0 GB | 8K | ~50 | Hugging Face, compact, well-trained |
| **Llama 3.2 3B** | 3.2B | 1.8 GB | 128K | ~25 | General chat, summarization, tool use |
| **Qwen 2.5 3B** | 3B | 1.7 GB | 32K | ~28 | Bilingual general purpose |

**Tier 2 — Fits but tight (3–4.5 GB, shorter context recommended)**

| Model | Params | Q4_K_M size | Context | Gen. tok/s (est.) | Best for |
|-------|--------|-------------|---------|-------------------|----------|
| **Phi-4 Mini** | 3.8B | 2.3 GB | 4K/128K | ~20 | Reasoning, math, code |
| **Phi-3 Mini** | 3.8B | 2.2 GB | 4K/128K | ~20 | Reasoning, code |
| **Qwen 3 4B** | 4B | 2.4 GB | 32K | ~18 | Thinking + non-thinking modes |
| **Gemma 3 4B** | 4B | 2.5 GB | 128K | ~17 | Vision + language (multimodal) |
| **Llama 3.3 8B** | 8B | 4.6 GB | 128K | ~10 | General purpose (needs short ctx) |
| **Mistral 7B v0.3** | 7B | 4.1 GB | 32K | ~11 | Chat, function calling |
| **Qwen 3 8B** | 8B | 4.9 GB | 32K | ~9 | Bilingual, thinking mode |

**Tier 3 — Barely fits (4.5+ GB, heavily constrained)**

| Model | Params | Q4_K_M size | Max ctx | Gen. tok/s (est.) | Notes |
|-------|--------|-------------|---------|-------------------|-------|
| **Llama 3.1 8B** | 8B | 4.7 GB | 512–1K | ~9 | Functional but cramped |
| **Mistral Nemo 12B** | 12B | 7.0 GB | 256 | ~5 | Needs Q3_K or Q2_K to fit |
| **Phi-4 14B** | 14B | 8.4 GB | — | Won't fit | Use Orin NX 16 GB |

> **Rule of thumb:** Model Q4_K_M size + 2.5 GB (OS/CMA/CUDA) + KV cache must be < 8 GB. For Tier 2 models, cap context at 1–2K tokens. For Tier 3, consider Q3_K_M or Q2_K quantization.

</details>

### 2.2 需要更大 Jetson 硬件的模型

供参考 —— 在更大的 Jetson 模块上能跑什么：

| 模型 | 参数 | Q4_K_M | 最低 Jetson 模块 | 备注 |
|-------|--------|--------|-------------------|-------|
| Phi-4 14B | 14B | 8.4 GB | **Orin NX 16 GB** | 推理能力好 |
| Qwen 2.5 Coder 14B | 14B | 8.6 GB | **Orin NX 16 GB** | 代码生成 |
| Mistral Small 24B | 24B | 14 GB | **AGX Orin 32 GB** | 通用能力强 |
| DeepSeek-R1 Distill 32B | 32B | 19 GB | **AGX Orin 32 GB** | 推理链 |
| Llama 3.3 70B | 70B | 42 GB | **AGX Orin 64 GB** | 接近 GPT-4 的质量 |
| Qwen 3 235B (MoE) | 235B | ~60 GB | **多 GPU / 云端** | 22B 激活参数 |
| DeepSeek V3 671B (MoE) | 671B | ~380 GB | **多 GPU 集群** | 37B 激活参数 |

### 2.3 值得关注的最新模型（2025–2026）

边缘 LLM 格局变化很快。以下模型在发布后值得评测：

| 模型系列 | 对 Jetson 的意义 |
|-------------|--------------------------|
| **Qwen 3 (0.6B–235B)** | 思考 + 非思考模式，小尺寸变体（1.7B、4B）优秀 |
| **Gemma 3 (1B–27B)** | 多模态（视觉+文本），1B 和 4B 适合边缘 |
| **Phi-4 Mini (3.8B)** | 微软在小尺寸上的强推理 |
| **Llama 4 Scout/Maverick** | MoE 架构 —— 激活参数可能适合边缘 |
| **SmolLM2 (135M–1.7B)** | Hugging Face 的超紧凑系列 |
| **Step 3.5 Flash (196B MoE，11B 激活)** | StepFun 的开源 MoE 推理模型 —— 每 token 仅激活 11B（能装进 Jetson！），262K 上下文，针对速度优化 |
| **MiMo-V2-Pro (1T+ MoE)** | 小米的旗舰 —— 智能体化场景，兼容 OpenClaw，1M 上下文，接近 Opus-4.6 的质量。直接跑在 Jetson 上太大，但**蒸馏版/更小的 MiMo 变体**是边缘目标 |
| **MiMo（系列，1.5B–7B）** | 小米的边缘优化模型 —— MiMo-7B 在数学/代码 benchmark 上表现强，1.5B 可轻松装进 Jetson |
| **DeepSeek-R1 Distill (1.5B–70B)** | 蒸馏推理 —— 1.5B 变体可轻松装入 |
| **Nemotron Nano（系列）** | NVIDIA 自家的边缘优化模型 |

#### MoE 模型 —— 为什么激活参数对 Jetson 很重要

MoE（混合专家模型）每个 token 只激活总参数中的一小部分。这改变了内存的计算：

```
Dense model (Llama 3.2 3B):
  Total params = Active params = 3B
  Q4_K_M size: 1.8 GB  (must load ALL weights per token)
  Memory bandwidth per token: 1.8 GB

MoE model (Step 3.5 Flash 196B, 11B active):
  Total params: 196B → Q4_K_M: ~110 GB  (won't fit in 8 GB!)
  BUT active params per token: 11B → needs ~6.5 GB of the 110 GB

  Challenge: even though only 11B are active, the FULL 110 GB model
  must be in memory because different tokens may route to different experts.
  → MoE models need the full weight set loaded, not just active params.

  For Jetson: MoE only helps if the TOTAL model fits.
  Step 3.5 Flash (110 GB) → won't fit on 8 GB Jetson
  A hypothetical MoE with 16B total / 4B active → would fit and be fast!
```

**可能能在 Jetson 上运行的 MoE 模型（如果存在小总参数版本）：**

| 模型 | 总参数 | 激活参数 | Q4_K_M 总计 | 能装进 8 GB？ |
|-------|-------------|---------------|-------------|------------|
| Mixtral 8x0.5B（假设） | 4B | 0.5B | ~2.4 GB | 能 |
| Llama 4 Scout 小尺寸变体 | 待定 | 待定 | 待定 | 关注小型 MoE 的发布 |
| Step Flash 蒸馏版 | 待定 | 待定 | 待定 | 如果 StepFun 发布更小的变体 |

边缘 MoE 的真正机会：**总参数 <8B** 且**激活参数 <2B** 的模型 —— 具备稠密模型的内存占用，同时有大型模型的路由质量。这是一个活跃的研究方向。

### 2.4 选择合适的模型 —— 决策流程图

```
What's your use case?
│
├── Simple classification / extraction / tool calling
│   └── Llama 3.2 1B or Qwen 3 1.7B  (< 1 GB, ~50+ tok/s)
│
├── General chat / assistant
│   └── Llama 3.2 3B or Gemma 2 2B  (1.5–1.8 GB, ~25–35 tok/s)
│
├── Reasoning / math / code
│   └── Phi-4 Mini 3.8B or Qwen 3 4B  (2.3–2.5 GB, ~18–20 tok/s)
│
├── Multimodal (image + text)
│   └── Gemma 3 4B  (2.5 GB, supports image input)
│
├── Bilingual (Chinese + English)
│   └── Qwen 3 4B or Qwen 2.5 3B  (1.7–2.4 GB)
│
├── Code generation
│   └── Phi-4 Mini or Qwen 2.5 Coder 3B  (if available at 3B)
│
├── Maximum quality (willing to accept slower speed)
│   └── Llama 3.3 8B Q4_K_M with 1K context  (4.6 GB, ~10 tok/s)
│
└── Need long context (8K+ tokens)
    └── Llama 3.2 3B (128K native) or Gemma 3 1B (32K)
        Cap actual context to fit KV cache budget
```


<details>
<summary>English original</summary>

**2.2 Models That Need Larger Jetson Hardware**

For reference — what runs on bigger Jetson modules:

| Model | Params | Q4_K_M | Min Jetson module | Notes |
|-------|--------|--------|-------------------|-------|
| Phi-4 14B | 14B | 8.4 GB | **Orin NX 16 GB** | Good reasoning |
| Qwen 2.5 Coder 14B | 14B | 8.6 GB | **Orin NX 16 GB** | Code generation |
| Mistral Small 24B | 24B | 14 GB | **AGX Orin 32 GB** | Strong general purpose |
| DeepSeek-R1 Distill 32B | 32B | 19 GB | **AGX Orin 32 GB** | Reasoning chains |
| Llama 3.3 70B | 70B | 42 GB | **AGX Orin 64 GB** | Near-GPT-4 quality |
| Qwen 3 235B (MoE) | 235B | ~60 GB | **Multi-GPU / cloud** | 22B active params |
| DeepSeek V3 671B (MoE) | 671B | ~380 GB | **Multi-GPU cluster** | 37B active params |

**2.3 Latest Models Worth Watching (2025–2026)**

The edge LLM landscape moves fast. Models to evaluate as they release:

| Model family | Why it matters for Jetson |
|-------------|--------------------------|
| **Qwen 3 (0.6B–235B)** | Thinking + non-thinking modes, great small variants (1.7B, 4B) |
| **Gemma 3 (1B–27B)** | Multimodal (vision+text), good 1B and 4B for edge |
| **Phi-4 Mini (3.8B)** | Microsoft's strong reasoning at small size |
| **Llama 4 Scout/Maverick** | MoE architecture — active params may fit edge |
| **SmolLM2 (135M–1.7B)** | Hugging Face's ultra-compact series |
| **Step 3.5 Flash (196B MoE, 11B active)** | StepFun's open-source MoE reasoning model — only 11B active per token (fits Jetson!), 262K context, speed-optimized |
| **MiMo-V2-Pro (1T+ MoE)** | Xiaomi's flagship — agentic scenarios, OpenClaw-compatible, 1M context, approaches Opus-4.6 quality. Too large for Jetson directly, but **distilled/smaller MiMo variants** are edge targets |
| **MiMo (series, 1.5B–7B)** | Xiaomi's edge-optimized models — MiMo-7B has strong math/code benchmarks, 1.5B fits Jetson easily |
| **DeepSeek-R1 Distill (1.5B–70B)** | Distilled reasoning — 1.5B variant fits easily |
| **Nemotron Nano (series)** | NVIDIA's own edge-optimized models |

**MoE Models — Why Active Parameters Matter for Jetson**

MoE (Mixture of Experts) models activate only a fraction of their total parameters per token. This changes the memory math:

```
Dense model (Llama 3.2 3B):
  Total params = Active params = 3B
  Q4_K_M size: 1.8 GB  (must load ALL weights per token)
  Memory bandwidth per token: 1.8 GB

MoE model (Step 3.5 Flash 196B, 11B active):
  Total params: 196B → Q4_K_M: ~110 GB  (won't fit in 8 GB!)
  BUT active params per token: 11B → needs ~6.5 GB of the 110 GB

  Challenge: even though only 11B are active, the FULL 110 GB model
  must be in memory because different tokens may route to different experts.
  → MoE models need the full weight set loaded, not just active params.

  For Jetson: MoE only helps if the TOTAL model fits.
  Step 3.5 Flash (110 GB) → won't fit on 8 GB Jetson
  A hypothetical MoE with 16B total / 4B active → would fit and be fast!
```

**MoE models that could work on Jetson (if available in small total size):**

| Model | Total params | Active params | Q4_K_M total | Fits 8 GB? |
|-------|-------------|---------------|-------------|------------|
| Mixtral 8x0.5B (hypothetical) | 4B | 0.5B | ~2.4 GB | Yes |
| Llama 4 Scout small variant | TBD | TBD | TBD | Watch for small MoE releases |
| Step Flash distilled | TBD | TBD | TBD | If StepFun releases smaller variant |

The real edge MoE opportunity: models with **<8B total parameters** and **<2B active** — giving dense-model memory footprint with large-model routing quality. This is an active research area.

**2.4 Choosing the Right Model — Decision Flowchart**

```
What's your use case?
│
├── Simple classification / extraction / tool calling
│   └── Llama 3.2 1B or Qwen 3 1.7B  (< 1 GB, ~50+ tok/s)
│
├── General chat / assistant
│   └── Llama 3.2 3B or Gemma 2 2B  (1.5–1.8 GB, ~25–35 tok/s)
│
├── Reasoning / math / code
│   └── Phi-4 Mini 3.8B or Qwen 3 4B  (2.3–2.5 GB, ~18–20 tok/s)
│
├── Multimodal (image + text)
│   └── Gemma 3 4B  (2.5 GB, supports image input)
│
├── Bilingual (Chinese + English)
│   └── Qwen 3 4B or Qwen 2.5 3B  (1.7–2.4 GB)
│
├── Code generation
│   └── Phi-4 Mini or Qwen 2.5 Coder 3B  (if available at 3B)
│
├── Maximum quality (willing to accept slower speed)
│   └── Llama 3.3 8B Q4_K_M with 1K context  (4.6 GB, ~10 tok/s)
│
└── Need long context (8K+ tokens)
    └── Llama 3.2 3B (128K native) or Gemma 3 1B (32K)
        Cap actual context to fit KV cache budget
```

</details>

### 2.5 Jetson 上的 Ollama —— 最简部署

[Ollama](https://ollama.com) 在 Jetson 上原生运行，支持 CUDA：

```bash
# Install Ollama on Jetson
curl -fsSL https://ollama.com/install.sh | sh

# Pull and run a model (automatically selects Q4_K_M)
ollama pull llama3.2:3b
ollama run llama3.2:3b

# Or for smaller/faster:
ollama pull qwen3:1.7b
ollama run qwen3:1.7b

# Serve as API (compatible with OpenAI format)
ollama serve &
curl http://localhost:11434/v1/chat/completions \
  -d '{"model": "llama3.2:3b", "messages": [{"role": "user", "content": "Hello"}]}'
```

**Ollama 在 Jetson 上的优势：**
- 自动检测 CUDA 并卸载到 GPU
- 内置模型管理（pull、delete、list）
- 兼容 OpenAI 的 API 端点
- 自动处理量化
- 一条命令安装，无 Python 依赖

```bash
# Check GPU utilization while running
tegrastats --interval 1000

# List downloaded models and sizes
ollama list

# Remove a model to free space
ollama rm llama3.2:3b
```

### 2.6 离线模型传输 —— 无互联网的 Jetson

生产环境中的 Jetson 设备常常没有互联网访问（气隙隔离、DNS 问题、工厂车间）。通过 USB 或 LAN 从联网机器传输模型。

**步骤 1 —— 在工作站/VM 上下载：**

```bash
# On internet-connected machine (workstation, cloud VM, etc.)
cd /tmp

# Download GGUF model from Hugging Face
wget "https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/\
Llama-3.2-3B-Instruct-Q4_K_M.gguf"

# Or for Nemotron (NVIDIA's edge model):
wget "https://huggingface.co/bartowski/Nemotron-Mini-4B-Instruct-GGUF/resolve/main/\
Nemotron-Mini-4B-Instruct-Q4_K_M.gguf"
```

**步骤 2 —— 通过 USB 或 LAN 传输到 Jetson：**

```bash
# Method A: USB device-mode (Jetson acts as USB Ethernet at 192.168.55.1)
scp Llama-3.2-3B-Instruct-Q4_K_M.gguf user@192.168.55.1:/opt/models/

# Method B: Ethernet/WiFi LAN (replace with Jetson's IP)
scp Llama-3.2-3B-Instruct-Q4_K_M.gguf user@192.168.1.100:/opt/models/

# Method C: USB flash drive (if no network)
# Mount USB drive on workstation, copy model, unmount, plug into Jetson
cp Llama-3.2-3B-Instruct-Q4_K_M.gguf /media/usb_drive/
# On Jetson:
sudo mount /dev/sda1 /mnt
cp /mnt/Llama-3.2-3B-Instruct-Q4_K_M.gguf /opt/models/
```

**步骤 3 —— 在 Jetson 上验证：**

```bash
# Check file exists and size is correct
ls -lh /opt/models/Llama-3.2-3B-Instruct-Q4_K_M.gguf
# Expected: ~1.8 GB for 3B Q4_K_M

# Verify integrity (optional — compare SHA256 with HuggingFace page)
sha256sum /opt/models/Llama-3.2-3B-Instruct-Q4_K_M.gguf

# Check disk space
df -h /opt/models/
```

**步骤 4 —— 运行推理：**

```bash
# With llama.cpp:
./llama-cli -m /opt/models/Llama-3.2-3B-Instruct-Q4_K_M.gguf -ngl 99 -c 2048

# With Ollama (import local GGUF):
# Create a Modelfile
echo 'FROM /opt/models/Llama-3.2-3B-Instruct-Q4_K_M.gguf' > Modelfile
ollama create my-llama -f Modelfile
ollama run my-llama
```

**USB 设备模式设置（如果不工作）：**

```bash
# Check if USB device-mode service is running
systemctl status nv-l4t-usb-device-mode

# Enable if disabled
sudo systemctl enable --now nv-l4t-usb-device-mode

# Verify Jetson has USB IP
ip addr show usb0    # should show 192.168.55.1

# On workstation, verify connectivity
ping 192.168.55.1
```

**传输速度参考：**

| 方式 | 速度 | 2 GB 模型所需时间 |
|--------|-------|---------------------|
| USB 2.0 设备模式 | ~30 MB/s | ~67 秒 |
| USB 3.0 闪存盘 | ~100 MB/s | ~20 秒 |
| 千兆以太网 | ~110 MB/s | ~18 秒 |
| WiFi (802.11ac) | ~40 MB/s | ~50 秒 |

---

## 3. KV Cache 管理 —— 隐藏的内存消耗者

在自回归生成过程中，KV cache 为上下文中的每个 token 存储 key/value 张量。这可能消耗比模型本身更多的内存。

### 3.1 KV Cache 大小计算

```
KV cache size = 2 × num_layers × num_kv_heads × head_dim × context_length × bytes_per_element

Llama 3.2 3B (INT8 KV cache, 2048 context):
  = 2 × 26 layers × 8 kv_heads × 128 head_dim × 2048 tokens × 1 byte
  = 2 × 26 × 8 × 128 × 2048
  = ~109 MB

Same model at 8192 context:
  = ~435 MB  ← significant on 8 GB!

Same model at 128K context:
  = ~6.8 GB  ← impossible, exceeds total free memory
```


<details>
<summary>English original</summary>

**2.5 Ollama on Jetson — Easiest Deployment**

[Ollama](https://ollama.com) runs natively on Jetson with CUDA support:

```bash
# Install Ollama on Jetson
curl -fsSL https://ollama.com/install.sh | sh

# Pull and run a model (automatically selects Q4_K_M)
ollama pull llama3.2:3b
ollama run llama3.2:3b

# Or for smaller/faster:
ollama pull qwen3:1.7b
ollama run qwen3:1.7b

# Serve as API (compatible with OpenAI format)
ollama serve &
curl http://localhost:11434/v1/chat/completions \
  -d '{"model": "llama3.2:3b", "messages": [{"role": "user", "content": "Hello"}]}'
```

**Ollama advantages on Jetson:**
- Automatic CUDA detection and GPU offloading
- Built-in model management (pull, delete, list)
- OpenAI-compatible API endpoint
- Handles quantization automatically
- One-command install, no Python dependencies

```bash
# Check GPU utilization while running
tegrastats --interval 1000

# List downloaded models and sizes
ollama list

# Remove a model to free space
ollama rm llama3.2:3b
```

**2.6 Offline Model Transfer — Jetson Without Internet**

Production Jetson devices often have no internet access (air-gapped, DNS issues, factory floor). Transfer models from an internet-connected machine via USB or LAN.

**Step 1 — Download on your workstation/VM:**

```bash
# On internet-connected machine (workstation, cloud VM, etc.)
cd /tmp

# Download GGUF model from Hugging Face
wget "https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/\
Llama-3.2-3B-Instruct-Q4_K_M.gguf"

# Or for Nemotron (NVIDIA's edge model):
wget "https://huggingface.co/bartowski/Nemotron-Mini-4B-Instruct-GGUF/resolve/main/\
Nemotron-Mini-4B-Instruct-Q4_K_M.gguf"
```

**Step 2 — Transfer to Jetson via USB or LAN:**

```bash
# Method A: USB device-mode (Jetson acts as USB Ethernet at 192.168.55.1)
scp Llama-3.2-3B-Instruct-Q4_K_M.gguf user@192.168.55.1:/opt/models/

# Method B: Ethernet/WiFi LAN (replace with Jetson's IP)
scp Llama-3.2-3B-Instruct-Q4_K_M.gguf user@192.168.1.100:/opt/models/

# Method C: USB flash drive (if no network)
# Mount USB drive on workstation, copy model, unmount, plug into Jetson
cp Llama-3.2-3B-Instruct-Q4_K_M.gguf /media/usb_drive/
# On Jetson:
sudo mount /dev/sda1 /mnt
cp /mnt/Llama-3.2-3B-Instruct-Q4_K_M.gguf /opt/models/
```

**Step 3 — Verify on Jetson:**

```bash
# Check file exists and size is correct
ls -lh /opt/models/Llama-3.2-3B-Instruct-Q4_K_M.gguf
# Expected: ~1.8 GB for 3B Q4_K_M

# Verify integrity (optional — compare SHA256 with HuggingFace page)
sha256sum /opt/models/Llama-3.2-3B-Instruct-Q4_K_M.gguf

# Check disk space
df -h /opt/models/
```

**Step 4 — Run inference:**

```bash
# With llama.cpp:
./llama-cli -m /opt/models/Llama-3.2-3B-Instruct-Q4_K_M.gguf -ngl 99 -c 2048

# With Ollama (import local GGUF):
# Create a Modelfile
echo 'FROM /opt/models/Llama-3.2-3B-Instruct-Q4_K_M.gguf' > Modelfile
ollama create my-llama -f Modelfile
ollama run my-llama
```

**USB device-mode setup (if not working):**

```bash
# Check if USB device-mode service is running
systemctl status nv-l4t-usb-device-mode

# Enable if disabled
sudo systemctl enable --now nv-l4t-usb-device-mode

# Verify Jetson has USB IP
ip addr show usb0    # should show 192.168.55.1

# On workstation, verify connectivity
ping 192.168.55.1
```

**Transfer speed reference:**

| Method | Speed | Time for 2 GB model |
|--------|-------|---------------------|
| USB 2.0 device-mode | ~30 MB/s | ~67 sec |
| USB 3.0 flash drive | ~100 MB/s | ~20 sec |
| Gigabit Ethernet | ~110 MB/s | ~18 sec |
| WiFi (802.11ac) | ~40 MB/s | ~50 sec |

---

**3. KV Cache Management — The Hidden Memory Consumer**

During autoregressive generation, the KV cache stores key/value tensors for every token in the context. This can consume more memory than the model itself.

**3.1 KV Cache Size Calculation**

```
KV cache size = 2 × num_layers × num_kv_heads × head_dim × context_length × bytes_per_element

Llama 3.2 3B (INT8 KV cache, 2048 context):
  = 2 × 26 layers × 8 kv_heads × 128 head_dim × 2048 tokens × 1 byte
  = 2 × 26 × 8 × 128 × 2048
  = ~109 MB

Same model at 8192 context:
  = ~435 MB  ← significant on 8 GB!

Same model at 128K context:
  = ~6.8 GB  ← impossible, exceeds total free memory
```

</details>

### 3.2 KV cache 优化技术

| 技术 | 内存节省 | 质量影响 | Jetson 支持 |
|-----------|-------------|----------------|----------------|
| **INT8 KV cache** | 对比 FP16 节省 2× | 极小 | llama.cpp、TensorRT-LLM |
| **INT4 KV cache** | 对比 FP16 节省 4× | 小 | llama.cpp（实验性） |
| **GQA（分组查询注意力）** | 对比 MHA 节省 4–8× | 无（模型层面） | 内置于现代模型 |
| **滑动窗口 attention** | 有界 | 丢失长上下文 | Mistral、部分模型 |
| **KV cache 驱逐** | 有界 | 丢失旧上下文 | 自定义实现 |
| **PagedAttention（vLLM）** | 无碎片浪费 | 无 | 不支持 Jetson（vLLM = 服务器） |

**GQA 最重要：** Llama 3.2 使用 GQA，含 8 个 KV head（对比 32 个 query head）。这意味着 KV cache 比传统 MHA 小 4×。在 Jetson 上始终优先选择 GQA 模型。

### 3.3 上下文长度预算

```
Orin Nano 8GB Memory Budget for LLM:

Total DRAM:                    8.0 GB
  - Firmware carveouts:       -0.4 GB
  - OS + kernel:              -0.5 GB
  - CMA:                      -0.5 GB (reduced for LLM workload)
  - CUDA runtime:             -0.3 GB
  ────────────────────────────────────
  Available for LLM:           6.3 GB

Model (Llama 3.2 3B INT4):   -1.5 GB
KV cache (INT8, ctx=2048):   -0.1 GB
Activation memory:            -0.2 GB
  ────────────────────────────────────
  Remaining:                   4.5 GB  ← room for longer context or larger model

Model (Phi-3 Mini INT4):     -1.9 GB
KV cache (INT8, ctx=4096):   -0.3 GB
Activation memory:            -0.3 GB
  ────────────────────────────────────
  Remaining:                   3.8 GB  ← still comfortable
```

---

## 4. FlashAttention 与融合 kernel

### 4.1 为什么融合 kernel 在 Jetson 上重要

在 H100 上，融合 kernel 节省 SRAM 带宽并提升 Tensor Core 利用率。在 Jetson 上，它们节省 **DRAM 带宽** —— 头号瓶颈。

```
Unfused attention (naive):
  Q × K^T → write attention scores to DRAM → read back → softmax → write →
  read back → multiply by V → write output

  Total DRAM traffic: ~4× the minimum

FlashAttention-2 (fused):
  Q × K^T → softmax → × V  (all in SRAM/registers, ONE read + ONE write)

  Total DRAM traffic: ~1× the minimum → 4× less bandwidth used
```

在 Jetson Orin Nano Super 的 102 GB/s 带宽下，这就是 20 tokens/sec 与 50 tokens/sec 的差别。

### 4.2 Jetson 上的 FlashAttention

```bash
# llama.cpp automatically uses FlashAttention when available
./llama-cli -m model.gguf -ngl 99 -fa  # -fa enables FlashAttention

# TensorRT-LLM compiles FlashAttention into the engine
trtllm-build --model_dir ./model --output_dir ./engine \
    --use_fused_mlp \
    --use_flash_attn \
    --max_batch_size 1 \
    --max_input_len 2048 \
    --max_seq_len 2560
```

### 4.3 其他融合操作

每个融合操作都消除一次 DRAM 往返：

| 融合操作 | 组合内容 | 节省带宽 |
|----------------|-----------------|-----------------|
| **融合 RMSNorm** | 归一化 + 缩放合为一个 kernel | 流量减少约 2× |
| **融合 SwiGLU** | 门控 + 激活 + 相乘 | 流量减少约 3× |
| **融合 Rotary Embedding** | 位置编码 + Q/K 投影 | 流量减少约 2× |
| **融合 Add + Norm** | 残差连接 + layer norm | 流量减少约 2× |

TensorRT-LLM 会自动启用这些。llama.cpp 内置许多融合 CUDA kernel。

---

## 5. Jetson 上的 TensorRT-LLM

TensorRT-LLM 是 NVIDIA 面向 LLM 的优化推理引擎。它把模型编译为带融合 kernel、量化和 Tensor Core 使用的 TensorRT engine。

### 5.1 为 Jetson 构建 TensorRT-LLM engine

```bash
# Install TensorRT-LLM (JetPack 6.x includes TensorRT, add LLM extension)
pip install tensorrt-llm --extra-index-url https://pypi.nvidia.com

# Convert Hugging Face model to TensorRT-LLM checkpoint
python convert_checkpoint.py \
    --model_dir ./Llama-3.2-3B-Instruct \
    --output_dir ./checkpoint \
    --dtype float16 \
    --tp_size 1          # single GPU on Jetson

# Build optimized engine
trtllm-build \
    --checkpoint_dir ./checkpoint \
    --output_dir ./engine \
    --gemm_plugin float16 \
    --max_batch_size 1 \
    --max_input_len 1024 \
    --max_seq_len 2048 \
    --max_num_tokens 2048 \
    --use_fused_mlp enable \
    --use_flash_attn enable \
    --strongly_typed

# Run inference
python run.py \
    --engine_dir ./engine \
    --tokenizer_dir ./Llama-3.2-3B-Instruct \
    --max_output_len 256 \
    --input_text "How does Jetson unified memory work?"
```

### 5.2 最小内存的 INT4 AWQ engine

```bash
# Build with INT4 AWQ quantization
trtllm-build \
    --checkpoint_dir ./checkpoint-awq \
    --output_dir ./engine-int4 \
    --gemm_plugin auto \
    --max_batch_size 1 \
    --max_input_len 1024 \
    --max_seq_len 2048 \
    --use_fused_mlp enable \
    --weight_only_precision int4_awq
```


<details>
<summary>English original</summary>

**3.2 KV Cache Optimization Techniques**

| Technique | Memory saving | Quality impact | Jetson support |
|-----------|-------------|----------------|----------------|
| **INT8 KV cache** | 2× vs FP16 | Minimal | llama.cpp, TensorRT-LLM |
| **INT4 KV cache** | 4× vs FP16 | Small | llama.cpp (experimental) |
| **GQA (Grouped Query Attention)** | 4–8× vs MHA | None (model-level) | Built into modern models |
| **Sliding window attention** | Bounded | Loses long context | Mistral, some models |
| **KV cache eviction** | Bounded | Loses old context | Custom implementation |
| **PagedAttention (vLLM)** | No fragmentation waste | None | Not on Jetson (vLLM = server) |

**GQA is the most important:** Llama 3.2 uses GQA with 8 KV heads (vs 32 query heads). This means the KV cache is 4× smaller than traditional MHA. Always prefer GQA models on Jetson.

**3.3 Context Length Budget**

```
Orin Nano 8GB Memory Budget for LLM:

Total DRAM:                    8.0 GB
  - Firmware carveouts:       -0.4 GB
  - OS + kernel:              -0.5 GB
  - CMA:                      -0.5 GB (reduced for LLM workload)
  - CUDA runtime:             -0.3 GB
  ────────────────────────────────────
  Available for LLM:           6.3 GB

Model (Llama 3.2 3B INT4):   -1.5 GB
KV cache (INT8, ctx=2048):   -0.1 GB
Activation memory:            -0.2 GB
  ────────────────────────────────────
  Remaining:                   4.5 GB  ← room for longer context or larger model

Model (Phi-3 Mini INT4):     -1.9 GB
KV cache (INT8, ctx=4096):   -0.3 GB
Activation memory:            -0.3 GB
  ────────────────────────────────────
  Remaining:                   3.8 GB  ← still comfortable
```

---

**4. FlashAttention and Fused Kernels**

**4.1 Why Fused Kernels Matter on Jetson**

On H100, fused kernels save SRAM bandwidth and improve Tensor Core utilization. On Jetson, they save **DRAM bandwidth** — the #1 bottleneck.

```
Unfused attention (naive):
  Q × K^T → write attention scores to DRAM → read back → softmax → write →
  read back → multiply by V → write output

  Total DRAM traffic: ~4× the minimum

FlashAttention-2 (fused):
  Q × K^T → softmax → × V  (all in SRAM/registers, ONE read + ONE write)

  Total DRAM traffic: ~1× the minimum → 4× less bandwidth used
```

On Jetson Orin Nano Super's 102 GB/s bandwidth, this is the difference between 20 tokens/sec and 50 tokens/sec.

**4.2 FlashAttention on Jetson**

```bash
# llama.cpp automatically uses FlashAttention when available
./llama-cli -m model.gguf -ngl 99 -fa  # -fa enables FlashAttention

# TensorRT-LLM compiles FlashAttention into the engine
trtllm-build --model_dir ./model --output_dir ./engine \
    --use_fused_mlp \
    --use_flash_attn \
    --max_batch_size 1 \
    --max_input_len 2048 \
    --max_seq_len 2560
```

**4.3 Other Fused Operations**

Each fused operation eliminates a DRAM round-trip:

| Fused operation | What it combines | Bandwidth saved |
|----------------|-----------------|-----------------|
| **Fused RMSNorm** | Norm + scale in one kernel | ~2× less traffic |
| **Fused SwiGLU** | Gate + activation + multiply | ~3× less traffic |
| **Fused Rotary Embedding** | Position encoding + Q/K projection | ~2× less traffic |
| **Fused Add + Norm** | Residual connection + layer norm | ~2× less traffic |

TensorRT-LLM enables these automatically. llama.cpp has many fused CUDA kernels built in.

---

**5. TensorRT-LLM on Jetson**

TensorRT-LLM is NVIDIA's optimized inference engine for LLMs. It compiles the model into a TensorRT engine with fused kernels, quantization, and Tensor Core usage.

**5.1 Build a TensorRT-LLM Engine for Jetson**

```bash
# Install TensorRT-LLM (JetPack 6.x includes TensorRT, add LLM extension)
pip install tensorrt-llm --extra-index-url https://pypi.nvidia.com

# Convert Hugging Face model to TensorRT-LLM checkpoint
python convert_checkpoint.py \
    --model_dir ./Llama-3.2-3B-Instruct \
    --output_dir ./checkpoint \
    --dtype float16 \
    --tp_size 1          # single GPU on Jetson

# Build optimized engine
trtllm-build \
    --checkpoint_dir ./checkpoint \
    --output_dir ./engine \
    --gemm_plugin float16 \
    --max_batch_size 1 \
    --max_input_len 1024 \
    --max_seq_len 2048 \
    --max_num_tokens 2048 \
    --use_fused_mlp enable \
    --use_flash_attn enable \
    --strongly_typed

# Run inference
python run.py \
    --engine_dir ./engine \
    --tokenizer_dir ./Llama-3.2-3B-Instruct \
    --max_output_len 256 \
    --input_text "How does Jetson unified memory work?"
```

**5.2 INT4 AWQ Engine for Minimum Memory**

```bash
# Build with INT4 AWQ quantization
trtllm-build \
    --checkpoint_dir ./checkpoint-awq \
    --output_dir ./engine-int4 \
    --gemm_plugin auto \
    --max_batch_size 1 \
    --max_input_len 1024 \
    --max_seq_len 2048 \
    --use_fused_mlp enable \
    --weight_only_precision int4_awq
```

</details>

### 5.3 Jetson 上的 TensorRT-LLM vs llama.cpp

| | TensorRT-LLM | llama.cpp |
|---|-------------|-----------|
| **配置复杂度** | 高（需构建 engine） | 低（下载 GGUF 直接跑） |
| **性能** | 最佳（编译后的 kernel） | 很好（手工调优 CUDA） |
| **量化** | FP16、INT8、INT4 AWQ/GPTQ | Q2–Q8 混合精度（GGUF） |
| **灵活性** | 固定 engine（有改动需重建） | 动态（运行时可变） |
| **内存效率** | 极佳（预分配） | 良好（动态分配） |
| **模型支持** | 主流模型（Llama、Phi、Mistral 等） | 几乎 HF 上所有模型 |
| **适用场景** | 生产部署 | 原型验证 + 生产 |

**建议：** 原型验证从 **llama.cpp** 起步（5 分钟即可完成配置）。需要最高 tokens/sec 时，生产环境切换到 **TensorRT-LLM**。

---

## 6. 投机解码 —— 免费的速度提升

投机解码用小 **草稿模型** 猜出 N 个 token，再由大的 **目标模型** 在一次前向传播中校验这 N 个 token。猜测正确时，用 1 个 token 的代价换到 N 个 token。

```
Without speculative decoding:
  Target model: generate token 1 → token 2 → token 3 → token 4
  Time: 4 forward passes × 50ms = 200ms

With speculative decoding (draft model guesses 4 tokens):
  Draft model:  generate 4 candidate tokens (fast, ~5ms total)
  Target model: verify all 4 in ONE forward pass (~55ms)
  If 3/4 accepted: 3 tokens in 60ms instead of 150ms → 2.5× faster

Speedup: 1.5–3× depending on acceptance rate
```

### 6.1 在 Jetson 上

```bash
# llama.cpp supports speculative decoding
./llama-speculative \
    -m Llama-3.2-3B-Q4_K_M.gguf \       # target model (3B)
    -md TinyLlama-1.1B-Q4_K_M.gguf \    # draft model (1.1B)
    -ngl 99 \
    --draft 8 \                           # speculate 8 tokens ahead
    -p "Write a comprehensive guide to..."
```

**投机解码的内存预算：**
- 目标模型：Llama 3.2 3B INT4 = 1.5 GB
- 草稿模型：TinyLlama 1.1B INT4 = 0.6 GB
- 合计：2.1 GB —— 在 8 GB 上轻松放下

**Jetson 上不要用的情形：** 如果草稿模型让内存超出预算，投机解码就弊大于利。

---

## 7. runtime 优化

### 7.1 功耗模式选择

```bash
# Check available power modes
sudo nvpmodel -q --verbose

# Set to maximum performance (15W on Orin Nano, 25W on Orin NX)
sudo nvpmodel -m 0
sudo jetson_clocks    # lock GPU/CPU at max frequency

# Or set power-efficient mode (7W)
sudo nvpmodel -m 1   # fewer CPU cores, lower GPU clock
```

更高的功耗模式 = 更高的时钟 = 更多 tokens/sec。但热管理设计必须支撑得住。

### 7.2 GPU 频率与内存时钟

```bash
# Check current clocks
tegrastats --interval 500

# Lock GPU to max clock (prevents dynamic frequency scaling during inference)
sudo jetson_clocks

# Check GPU clock
cat /sys/devices/17000000.ga10b/devfreq/17000000.ga10b/cur_freq
```

动态调频会引入延迟抖动。要获得一致的推理延迟，就锁定时钟。

### 7.3 NUMA 感知分配（AGX Orin）

在内存更大的 AGX Orin 上，确保 CUDA 分配使用最优的内存控制器：

```bash
# Pin process to specific CPU cores close to memory controller
taskset -c 0-5 ./llama-cli -m model.gguf -ngl 99
```

### 7.4 用 Swap / zram 应对紧急溢出

如果模型就差一点点放不下，zram（压缩的内存 swap）可以帮上忙：

```bash
# Enable 4 GB zram (compressed in-memory swap)
sudo zramctl --find --size 4G --algorithm zstd
sudo mkswap /dev/zram0
sudo swapon /dev/zram0 -p 5

# Now models slightly over RAM can run (with performance penalty)
```

zram 在内存中压缩页 —— 对模型权重约 2:1 的压缩比。6 GB 的模型在 5.5 GB 可用内存上或许能靠 zram 跑起来，但因压缩/解压开销会有 30–50% 的速度损失。

---

## 8. Kernel 级优化 —— 真正的收益在这里

第 1–7 节讲的是模型级和系统级优化。本节再深入一层 —— 进入 **GPU kernel 本身**。RightNow Forge 和 RunInfra 这类云平台相对基线推理拿到的 3–7× 加速，正出自这里。


<details>
<summary>English original</summary>

**5.3 TensorRT-LLM vs llama.cpp on Jetson**

| | TensorRT-LLM | llama.cpp |
|---|-------------|-----------|
| **Setup complexity** | High (build engine) | Low (download GGUF, run) |
| **Performance** | Best (compiled kernels) | Very good (hand-tuned CUDA) |
| **Quantization** | FP16, INT8, INT4 AWQ/GPTQ | Q2–Q8 mixed precision (GGUF) |
| **Flexibility** | Fixed engine (rebuild for changes) | Dynamic (change at runtime) |
| **Memory efficiency** | Excellent (preallocated) | Good (dynamic allocation) |
| **Model support** | Major models (Llama, Phi, Mistral, etc.) | Almost everything on HF |
| **Best for** | Production deployment | Prototyping + production |

**Recommendation:** Start with **llama.cpp** for prototyping (5-minute setup). Switch to **TensorRT-LLM** for production when you need maximum tokens/sec.

---

**6. Speculative Decoding — Free Speed**

Speculative decoding uses a small **draft model** to guess N tokens, then the large **target model** verifies all N in one forward pass. If the guess is correct, you get N tokens for the price of 1.

```
Without speculative decoding:
  Target model: generate token 1 → token 2 → token 3 → token 4
  Time: 4 forward passes × 50ms = 200ms

With speculative decoding (draft model guesses 4 tokens):
  Draft model:  generate 4 candidate tokens (fast, ~5ms total)
  Target model: verify all 4 in ONE forward pass (~55ms)
  If 3/4 accepted: 3 tokens in 60ms instead of 150ms → 2.5× faster

Speedup: 1.5–3× depending on acceptance rate
```

**6.1 On Jetson**

```bash
# llama.cpp supports speculative decoding
./llama-speculative \
    -m Llama-3.2-3B-Q4_K_M.gguf \       # target model (3B)
    -md TinyLlama-1.1B-Q4_K_M.gguf \    # draft model (1.1B)
    -ngl 99 \
    --draft 8 \                           # speculate 8 tokens ahead
    -p "Write a comprehensive guide to..."
```

**Memory budget for speculative decoding:**
- Target: Llama 3.2 3B INT4 = 1.5 GB
- Draft: TinyLlama 1.1B INT4 = 0.6 GB
- Total: 2.1 GB — fits easily on 8 GB

**When NOT to use on Jetson:** if the draft model pushes you over the memory budget, speculative decoding hurts more than it helps.

---

**7. Runtime Optimizations**

**7.1 Power Mode Selection**

```bash
# Check available power modes
sudo nvpmodel -q --verbose

# Set to maximum performance (15W on Orin Nano, 25W on Orin NX)
sudo nvpmodel -m 0
sudo jetson_clocks    # lock GPU/CPU at max frequency

# Or set power-efficient mode (7W)
sudo nvpmodel -m 1   # fewer CPU cores, lower GPU clock
```

Higher power mode = higher clock = more tokens/sec. But thermal design must support it.

**7.2 GPU Frequency and Memory Clock**

```bash
# Check current clocks
tegrastats --interval 500

# Lock GPU to max clock (prevents dynamic frequency scaling during inference)
sudo jetson_clocks

# Check GPU clock
cat /sys/devices/17000000.ga10b/devfreq/17000000.ga10b/cur_freq
```

Dynamic frequency scaling adds latency jitter. For consistent inference latency, lock clocks.

**7.3 NUMA-Aware Allocation (AGX Orin)**

On AGX Orin with larger memory, ensure CUDA allocations use the optimal memory controller:

```bash
# Pin process to specific CPU cores close to memory controller
taskset -c 0-5 ./llama-cli -m model.gguf -ngl 99
```

**7.4 Swap / zram for Emergency Overflow**

If a model barely doesn't fit, zram (compressed RAM swap) can help:

```bash
# Enable 4 GB zram (compressed in-memory swap)
sudo zramctl --find --size 4G --algorithm zstd
sudo mkswap /dev/zram0
sudo swapon /dev/zram0 -p 5

# Now models slightly over RAM can run (with performance penalty)
```

zram compresses pages in memory — ~2:1 ratio for model weights. A 6 GB model on 5.5 GB available might work via zram, but with 30–50% speed penalty due to compression/decompression overhead.

---

**8. Kernel-Level Optimization — Where the Real Gains Are**

Sections 1–7 cover model-level and system-level optimizations. This section goes deeper — into the **GPU kernels themselves**. This is where cloud platforms like RightNow Forge and RunInfra achieve their 3–7× speedups over baseline inference.

</details>

### 8.1 GPU 利用率问题

多数 AI 推理浪费了 80%+ 的可用 GPU 周期：

```
Typical unoptimized LLM inference on GPU:

SM·00░·····█░░░░·····█░░░░·····█░     █ = compute
SM·01····██░░░·····██░░░·····██░░     ░ = memory I/O (waiting for DRAM)
SM·02···█░░░░·····█░░░░·····█░░░░     · = idle (nothing scheduled)
SM·03·██░░░·····██░░░·····██░░░··
  ~16% SM utilization

After kernel optimization:

SM·00██·█████░·█████░████████████     Same hardware, 5× more useful work
SM·01░░████████████████·██████·██
SM·02██████████··█████░██████░███
SM·03███·█████░░████████████████·
  ~88% SM utilization
```

**为什么会这样：** 默认的 PyTorch/ONNX kernel 是通用的 —— 它们在任何 GPU 上都能跑，但对哪个 GPU 都没做优化。每个算子（attention、norm、量化 GEMM）都启动一个独立的 kernel，从 DRAM 读取、计算、再写回。GPU 大部分时间都在等内存。

**在 Jetson 上这一点更要紧：** 102 GB/s 的共享带宽（H100 上是 3,350 GB/s）意味着 GPU 频繁处于数据饥饿状态。kernel 优化直接决定 tokens/sec。

### 8.2 kernel 优化的三个层次

```
Level 1: Operator Fusion (easiest, biggest win)
  Combine multiple operations into one kernel → fewer DRAM round-trips
  Example: RMSNorm + residual add + SwiGLU → one kernel, one read, one write

Level 2: Hardware-Specific Tuning (moderate difficulty)
  Tune tile sizes, thread block dimensions, shared memory usage for YOUR specific GPU
  Example: Orin Nano Ampere SM has different optimal tile size than H100 Hopper SM

Level 3: Custom Kernel Generation (hardest, maximum performance)
  Write or generate Triton/CUDA kernels specifically for your model + GPU + precision
  Example: INT4 dequantize-fused-GEMM kernel for Ampere with 128-thread blocks
```

### 8.3 性能剖析 —— 先找到瓶颈

**绝不要盲目优化。** 用性能剖析找出耗时最多的 kernel：

```bash
# On Jetson: profile with Nsight Systems
nsys profile --trace=cuda,nvtx -o llm_profile ./llama-cli -m model.gguf -ngl 99 -p "test"

# Analyze the trace
nsys stats llm_profile.nsys-rep

# Example output (typical LLM breakdown):
# Kernel                          Time%    Time       Calls
# ─────────────────────────────────────────────────────────
# attention_fwd                   41.2%    18.4ms     26      ← #1 bottleneck
# quantized_gemm_w4a16           22.8%    10.2ms     78
# rmsnorm_kernel                  14.1%     6.3ms     52
# silu_mul_kernel                  8.3%     3.7ms     26
# rotary_embedding                 5.1%     2.3ms     52
# others                           8.5%     3.8ms     ...
```

**在 Jetson 上，attention 的占比比服务器 GPU 还要高**，因为从 DRAM 加载 Q、K、V 的带宽开销占比更高。

### 8.4 Triton kernel —— 编写自定义优化算子

Triton 是 NVIDIA 基于 Python 的 GPU kernel 语言。它比手写 CUDA 容易得多，且能生成接近最优的代码。

**示例：融合 RMSNorm + 残差相加（常见的 LLM 瓶颈）：**

```python
import triton
import triton.language as tl

@triton.jit
def fused_rmsnorm_residual_kernel(
    X_ptr, Residual_ptr, Weight_ptr, Out_ptr,
    N: tl.constexpr, eps: tl.constexpr,
    BLOCK_SIZE: tl.constexpr
):
    """RMSNorm(X + Residual) * Weight — one kernel, one DRAM read, one write."""
    row = tl.program_id(0)
    offsets = tl.arange(0, BLOCK_SIZE)
    mask = offsets < N

    # Load X and Residual (one DRAM read each)
    x = tl.load(X_ptr + row * N + offsets, mask=mask, other=0.0)
    r = tl.load(Residual_ptr + row * N + offsets, mask=mask, other=0.0)

    # Fused: add residual + compute RMS norm + scale by weight
    h = x + r                                          # residual add
    mean_sq = tl.sum(h * h, axis=0) / N               # variance
    rrms = 1.0 / tl.sqrt(mean_sq + eps)               # reciprocal RMS
    w = tl.load(Weight_ptr + offsets, mask=mask, other=1.0)
    out = h * rrms * w                                 # normalize + scale

    # One DRAM write
    tl.store(Out_ptr + row * N + offsets, out, mask=mask)
```

**不做融合：** 3 个独立 kernel（残差相加、RMSNorm、权重相乘）= 6 次 DRAM 访问。
**做融合：** 1 个 kernel = 2 次 DRAM 访问。**内存流量减少 3×。**


<details>
<summary>English original</summary>

**8.1 The GPU Utilization Problem**

Most AI inference wastes 80%+ of available GPU cycles:

```
Typical unoptimized LLM inference on GPU:

SM·00░·····█░░░░·····█░░░░·····█░     █ = compute
SM·01····██░░░·····██░░░·····██░░     ░ = memory I/O (waiting for DRAM)
SM·02···█░░░░·····█░░░░·····█░░░░     · = idle (nothing scheduled)
SM·03·██░░░·····██░░░·····██░░░··
  ~16% SM utilization

After kernel optimization:

SM·00██·█████░·█████░████████████     Same hardware, 5× more useful work
SM·01░░████████████████·██████·██
SM·02██████████··█████░██████░███
SM·03███·█████░░████████████████·
  ~88% SM utilization
```

**Why this happens:** Default PyTorch/ONNX kernels are generic — they work on any GPU but optimize for none. Each operation (attention, norm, quantized GEMM) launches a separate kernel, reads from DRAM, computes, writes back. The GPU spends most of its time waiting for memory.

**On Jetson this matters even more:** 102 GB/s shared bandwidth (vs 3,350 GB/s on H100) means the GPU is frequently starved for data. Kernel optimization directly determines tokens/sec.

**8.2 The Three Levels of Kernel Optimization**

```
Level 1: Operator Fusion (easiest, biggest win)
  Combine multiple operations into one kernel → fewer DRAM round-trips
  Example: RMSNorm + residual add + SwiGLU → one kernel, one read, one write

Level 2: Hardware-Specific Tuning (moderate difficulty)
  Tune tile sizes, thread block dimensions, shared memory usage for YOUR specific GPU
  Example: Orin Nano Ampere SM has different optimal tile size than H100 Hopper SM

Level 3: Custom Kernel Generation (hardest, maximum performance)
  Write or generate Triton/CUDA kernels specifically for your model + GPU + precision
  Example: INT4 dequantize-fused-GEMM kernel for Ampere with 128-thread blocks
```

**8.3 Profiling — Find the Bottleneck First**

**Never optimize blind.** Profile to find which kernels consume the most time:

```bash
# On Jetson: profile with Nsight Systems
nsys profile --trace=cuda,nvtx -o llm_profile ./llama-cli -m model.gguf -ngl 99 -p "test"

# Analyze the trace
nsys stats llm_profile.nsys-rep

# Example output (typical LLM breakdown):
# Kernel                          Time%    Time       Calls
# ─────────────────────────────────────────────────────────
# attention_fwd                   41.2%    18.4ms     26      ← #1 bottleneck
# quantized_gemm_w4a16           22.8%    10.2ms     78
# rmsnorm_kernel                  14.1%     6.3ms     52
# silu_mul_kernel                  8.3%     3.7ms     26
# rotary_embedding                 5.1%     2.3ms     52
# others                           8.5%     3.8ms     ...
```

**On Jetson, attention dominates even more** than on server GPUs because the memory-bandwidth cost of loading Q, K, V from DRAM is proportionally higher.

**8.4 Triton Kernels — Writing Custom Optimized Ops**

Triton is NVIDIA's Python-based GPU kernel language. It's much easier than raw CUDA and generates near-optimal code.

**Example: Fused RMSNorm + Residual Add (common LLM bottleneck):**

```python
import triton
import triton.language as tl

@triton.jit
def fused_rmsnorm_residual_kernel(
    X_ptr, Residual_ptr, Weight_ptr, Out_ptr,
    N: tl.constexpr, eps: tl.constexpr,
    BLOCK_SIZE: tl.constexpr
):
    """RMSNorm(X + Residual) * Weight — one kernel, one DRAM read, one write."""
    row = tl.program_id(0)
    offsets = tl.arange(0, BLOCK_SIZE)
    mask = offsets < N

    # Load X and Residual (one DRAM read each)
    x = tl.load(X_ptr + row * N + offsets, mask=mask, other=0.0)
    r = tl.load(Residual_ptr + row * N + offsets, mask=mask, other=0.0)

    # Fused: add residual + compute RMS norm + scale by weight
    h = x + r                                          # residual add
    mean_sq = tl.sum(h * h, axis=0) / N               # variance
    rrms = 1.0 / tl.sqrt(mean_sq + eps)               # reciprocal RMS
    w = tl.load(Weight_ptr + offsets, mask=mask, other=1.0)
    out = h * rrms * w                                 # normalize + scale

    # One DRAM write
    tl.store(Out_ptr + row * N + offsets, out, mask=mask)
```

**Without fusion:** 3 separate kernels (residual add, RMSNorm, weight multiply) = 6 DRAM accesses.
**With fusion:** 1 kernel = 2 DRAM accesses. **3× less memory traffic.**

</details>

### 8.5 Autokernel — 自动化 kernel 生成

[RightNow Autokernel](https://github.com/RightNow-AI/autokernel) 自动化了为特定 GPU 硬件生成优化 Triton/CUDA kernel 的过程：

```bash
# Install autokernel
pip install autokernel

# Generate optimized kernels for your model + GPU
autokernel optimize \
    --model "Llama-3.2-3B" \
    --gpu "orin-nano" \
    --precision "int4" \
    --output ./optimized_kernels/
```

**autokernel 做什么：**
1. **剖析**你的模型，找出最慢的 kernel（attention、GEMM、norm）
2. **生成**不同 tile size、线程配置的 Triton kernel 变体
3. **在你特定的 GPU 上对全部变体跑 benchmark**
4. **选出**最快的配置
5. **验证**数值正确性是否与参考实现一致

**关键洞察：**最优 kernel 参数在不同 GPU 之间差异极大：

| 参数 | H100 (Hopper) | A100 (Ampere 架构) | Orin Nano (Ampere 架构) |
|-----------|--------------|---------------|-------------------|
| 最佳 GEMM 分块 | 256×128 | 128×128 | **64×64**（SM 更小） |
| 线程块 | 256 线程 | 256 线程 | **128 线程**（warp 更少） |
| 共享内存用量 | 164 KB | 164 KB | **48 KB**（每个 SM 更少） |
| 最优批大小 | 64+ | 32+ | **1–4**（受内存限制） |

为 H100 调优的 kernel，在 Orin Nano 上可能比为 Orin Nano 专门调优的 kernel **慢 2–3×**。

### 8.6 RightNow Forge — 企业级 kernel 优化

[RightNow Forge](https://www.rightnowai.co/forge) 是自动化整个 kernel 优化流水线的企业级平台：

```
Input:  model = "Llama-3.2-3B"
        gpu = "Jetson Orin Nano"
        baseline = "llama.cpp default"

Forge pipeline:
  1. Profile all kernels on target GPU
  2. Identify bottlenecks:
     ▲ attention       41% of total → generate FlashAttention variant for Ampere
     ▲ quantized GEMM  23% of total → generate INT4 dequant-fused GEMM
     ▲ rmsnorm         14% of total → generate fused RMSNorm+residual
  3. Compile optimized Triton kernels for Orin Nano SM
  4. Verify correctness (bit-accurate vs reference)
  5. Output: drop-in replacement kernels

Result:
  TTFT (Time to First Token): 320ms → 42ms (7.6× faster)
  Throughput: 15 tok/s → 45 tok/s (3× faster)
  SM utilization: 16% → 72%
```

### 8.7 手动 kernel 优化检查清单

如果你要自己编写优化过的 kernel（阶段 5F 的领域），以下是 Jetson 上的优先级顺序：

```
Priority 1: Reduce DRAM traffic (Jetson's #1 bottleneck)
  □ Fuse consecutive elementwise ops (norm + add + activation)
  □ Fuse dequantize into GEMM (don't write dequantized weights to DRAM)
  □ Use FlashAttention (fuse Q×K + softmax + ×V into one kernel)
  □ Compute in registers/shared memory, write final result once

Priority 2: Maximize Tensor Core utilization
  □ Use WMMA/MMA instructions for matrix multiply (not scalar CUDA cores)
  □ Pad matrices to multiples of 16 for Tensor Core alignment
  □ Keep data in FP16/INT8 format that Tensor Cores consume directly

Priority 3: Tune for Orin Nano's specific SM
  □ Smaller tile sizes (64×64 vs 128×128 on server GPUs)
  □ Fewer threads per block (128 vs 256 — fewer warps available)
  □ Account for 48 KB shared memory limit per SM
  □ Fewer SMs (16 on Orin Nano vs 132 on H100) — fewer blocks in flight

Priority 4: Minimize kernel launch overhead
  □ Fuse small kernels into larger ones
  □ Use CUDA graphs to batch kernel launches
  □ Pre-allocate all buffers (no cudaMalloc during inference)
```

### 8.8 CUDA Graphs — 消除启动开销

每次 CUDA kernel 启动约有 5–10 µs 开销。一次包含 100+ 次 kernel 启动的 LLM 前向传播，仅启动开销就浪费约 1 ms。CUDA graphs 捕获整个序列，并在一次调用中重放：

```cpp
// Capture the inference graph once
cudaGraph_t graph;
cudaStreamBeginCapture(stream, cudaStreamCaptureModeGlobal);

// All kernel launches are recorded, not executed
attention_kernel<<<grid, block, 0, stream>>>(q, k, v, out);
rmsnorm_kernel<<<grid, block, 0, stream>>>(out, norm_out);
ffn_kernel<<<grid, block, 0, stream>>>(norm_out, ffn_out);
// ... all layers ...

cudaStreamEndCapture(stream, &graph);
cudaGraphInstantiate(&instance, graph, NULL, NULL, 0);

// Replay the entire forward pass with ONE launch
for (int token = 0; token < max_tokens; token++) {
    update_input_pointers(token);  // update KV cache pointers
    cudaGraphLaunch(instance, stream);
    cudaStreamSynchronize(stream);
}
// Launch overhead: ~5µs total instead of ~1ms
```

TensorRT-LLM 内部使用 CUDA graphs。llama.cpp 的 CUDA graph 支持仍属实验性。

---


<details>
<summary>English original</summary>

**8.5 Autokernel — Automated Kernel Generation**

[RightNow Autokernel](https://github.com/RightNow-AI/autokernel) automates the process of generating optimized Triton/CUDA kernels for specific GPU hardware:

```bash
# Install autokernel
pip install autokernel

# Generate optimized kernels for your model + GPU
autokernel optimize \
    --model "Llama-3.2-3B" \
    --gpu "orin-nano" \
    --precision "int4" \
    --output ./optimized_kernels/
```

**What autokernel does:**
1. **Profiles** your model to find the slowest kernels (attention, GEMM, norm)
2. **Generates** Triton kernel variants with different tile sizes, thread configurations
3. **Benchmarks** all variants on your specific GPU
4. **Selects** the fastest configuration
5. **Verifies** numerical correctness against reference implementation

**The key insight:** optimal kernel parameters differ dramatically between GPUs:

| Parameter | H100 (Hopper) | A100 (Ampere) | Orin Nano (Ampere) |
|-----------|--------------|---------------|-------------------|
| Best GEMM tile | 256×128 | 128×128 | **64×64** (smaller SMs) |
| Thread block | 256 threads | 256 threads | **128 threads** (fewer warps) |
| Shared mem usage | 164 KB | 164 KB | **48 KB** (less per SM) |
| Optimal batch | 64+ | 32+ | **1–4** (memory limited) |

A kernel tuned for H100 can be **2–3× slower** on Orin Nano than a kernel tuned for Orin Nano specifically.

**8.6 RightNow Forge — Enterprise Kernel Optimization**

[RightNow Forge](https://www.rightnowai.co/forge) is the enterprise platform that automates the full kernel optimization pipeline:

```
Input:  model = "Llama-3.2-3B"
        gpu = "Jetson Orin Nano"
        baseline = "llama.cpp default"

Forge pipeline:
  1. Profile all kernels on target GPU
  2. Identify bottlenecks:
     ▲ attention       41% of total → generate FlashAttention variant for Ampere
     ▲ quantized GEMM  23% of total → generate INT4 dequant-fused GEMM
     ▲ rmsnorm         14% of total → generate fused RMSNorm+residual
  3. Compile optimized Triton kernels for Orin Nano SM
  4. Verify correctness (bit-accurate vs reference)
  5. Output: drop-in replacement kernels

Result:
  TTFT (Time to First Token): 320ms → 42ms (7.6× faster)
  Throughput: 15 tok/s → 45 tok/s (3× faster)
  SM utilization: 16% → 72%
```

**8.7 Manual Kernel Optimization Checklist**

If you're writing your own optimized kernels (Phase 5F territory), here's the priority order for Jetson:

```
Priority 1: Reduce DRAM traffic (Jetson's #1 bottleneck)
  □ Fuse consecutive elementwise ops (norm + add + activation)
  □ Fuse dequantize into GEMM (don't write dequantized weights to DRAM)
  □ Use FlashAttention (fuse Q×K + softmax + ×V into one kernel)
  □ Compute in registers/shared memory, write final result once

Priority 2: Maximize Tensor Core utilization
  □ Use WMMA/MMA instructions for matrix multiply (not scalar CUDA cores)
  □ Pad matrices to multiples of 16 for Tensor Core alignment
  □ Keep data in FP16/INT8 format that Tensor Cores consume directly

Priority 3: Tune for Orin Nano's specific SM
  □ Smaller tile sizes (64×64 vs 128×128 on server GPUs)
  □ Fewer threads per block (128 vs 256 — fewer warps available)
  □ Account for 48 KB shared memory limit per SM
  □ Fewer SMs (16 on Orin Nano vs 132 on H100) — fewer blocks in flight

Priority 4: Minimize kernel launch overhead
  □ Fuse small kernels into larger ones
  □ Use CUDA graphs to batch kernel launches
  □ Pre-allocate all buffers (no cudaMalloc during inference)
```

**8.8 CUDA Graphs — Eliminate Launch Overhead**

Each CUDA kernel launch has ~5–10 µs overhead. An LLM forward pass with 100+ kernel launches wastes ~1 ms just on launch overhead. CUDA graphs capture the entire sequence and replay it in one call:

```cpp
// Capture the inference graph once
cudaGraph_t graph;
cudaStreamBeginCapture(stream, cudaStreamCaptureModeGlobal);

// All kernel launches are recorded, not executed
attention_kernel<<<grid, block, 0, stream>>>(q, k, v, out);
rmsnorm_kernel<<<grid, block, 0, stream>>>(out, norm_out);
ffn_kernel<<<grid, block, 0, stream>>>(norm_out, ffn_out);
// ... all layers ...

cudaStreamEndCapture(stream, &graph);
cudaGraphInstantiate(&instance, graph, NULL, NULL, 0);

// Replay the entire forward pass with ONE launch
for (int token = 0; token < max_tokens; token++) {
    update_input_pointers(token);  // update KV cache pointers
    cudaGraphLaunch(instance, stream);
    cudaStreamSynchronize(stream);
}
// Launch overhead: ~5µs total instead of ~1ms
```

TensorRT-LLM uses CUDA graphs internally. llama.cpp has experimental CUDA graph support.

---

</details>

## 9. 完整优化检查清单

```
Before deployment — run through this checklist:

□ Model Selection
  □ Model fits in INT4 with room for KV cache
  □ GQA-based model preferred (smaller KV cache)
  □ Context length budgeted against available memory

□ Quantization
  □ AWQ or GPTQ 4-bit for best quality/size
  □ GGUF Q4_K_M for llama.cpp deployment
  □ Calibration data representative of production input

□ KV Cache
  □ INT8 KV cache enabled
  □ Maximum context length capped to fit memory
  □ GQA model chosen to minimize KV memory

□ Inference Engine
  □ llama.cpp with -ngl 99 (full GPU offload)
  □ Or TensorRT-LLM engine compiled for target batch/context
  □ FlashAttention enabled

□ Kernel Optimization
  □ Profile with nsys to find top 3 bottleneck kernels
  □ Fused ops enabled (RMSNorm+residual, SwiGLU, rotary)
  □ CUDA graphs enabled for decode loop (reduce launch overhead)
  □ Tile sizes appropriate for Orin Nano SM (64×64, not 256×128)
  □ Consider autokernel/Forge for automated kernel tuning

□ System Configuration
  □ nvpmodel set to appropriate power mode
  □ jetson_clocks to lock frequencies
  □ CMA reduced (LLM doesn't need large CMA)
  □ Unnecessary services disabled (GUI, bluetooth)

□ Profiling
  □ tegrastats monitored during inference
  □ Tokens/sec measured at steady state
  □ Memory usage verified (no slow growth / leak)
  □ Thermal verified (no throttling under sustained load)
```

---

## 10. Benchmark 参考

Orin Nano Super 8 GB 上的预期性能（25W 模式，Q4_K_M，llama.cpp）：

| 模型 | 参数量 | GGUF 大小 | Prompt eval | 生成 | 上下文 |
|-------|--------|-----------|-------------|-----------|---------|
| TinyLlama 1.1B | 1.1B | 0.6 GB | ~200 tok/s | ~65 tok/s | 2048 |
| Llama 3.2 1B | 1.3B | 0.7 GB | ~170 tok/s | ~55 tok/s | 2048 |
| Gemma 2 2B | 2.6B | 1.5 GB | ~85 tok/s | ~35 tok/s | 2048 |
| Llama 3.2 3B | 3.2B | 1.8 GB | ~65 tok/s | ~25 tok/s | 2048 |
| Phi-3 Mini 3.8B | 3.8B | 2.2 GB | ~50 tok/s | ~20 tok/s | 2048 |

> 这些是在 25W 下 Orin Nano Super 的估算值。相比原版 Orin Nano 约 1.7× 的提升来自 102 GB/s 带宽（对比 68 GB/s）与更高的时钟频率。实际性能取决于功耗模式、热管理设计、上下文长度与 prompt 内容。务必针对你的具体配置跑 benchmark。

---

## 11. 项目

| # | 项目 | 你能学到什么 |
|---|---------|---------------|
| 1 | **Jetson 上的 llama.cpp** | 下载 Llama 3.2 3B Q4_K_M，用 CUDA 构建 llama.cpp，测量不同上下文长度下的 tokens/sec |
| 2 | **量化对比** | 用 Q2_K、Q4_K_M、Q6_K、Q8_0 跑同一模型。测量 tokens/sec、内存与输出质量（困惑度） |
| 3 | **TensorRT-LLM engine** | 为 Phi-3 Mini INT4 构建 TRT-LLM engine。在相同 prompt 下与 llama.cpp 对比延迟 |
| 4 | **投机解码** | 把 TinyLlama 设为 Llama 3.2 3B 的 draft 模型。测量接受率与加速比 |
| 5 | **内存预算审计** | 推理期间运行 tegrastats。逐 MB 映射：OS、CMA、模型、KV cache、激活值。对照 3.3 节核验 |
| 6 | **功耗与性能** | 在 7W、15W、25W 下 benchmark 同一模型（若为 Orin NX）。绘制 tokens/sec 对功耗的曲线。计算 tokens/joule |
| 7 | **上下文长度扩展** | 测量上下文 512、1024、2048、4096 下的 tokens/sec。绘图。找出 KV cache 压力导致性能下降的拐点 |
| 8 | **生产级 chatbot** | 用 llama.cpp server 模式在 Jetson 上构建服务 Llama 3.2 3B 的 REST API。测量并发请求下的 P50/P95 延迟 |
| 9 | **Nsight profile 分析** | 用 `nsys` 对 llama.cpp 推理做 profile。找出耗时前 3 的 kernel。计算 SM 利用率。区分带宽受限与算力受限的 kernel |
| 10 | **融合 RMSNorm Triton kernel** | 编写 8.4 节中的融合 RMSNorm+residual kernel。与未融合的 PyTorch 版本 benchmark。测量 DRAM 流量降幅 |
| 11 | **Jetson 上的 autokernel** | 用 autokernel 为 Orin Nano 上的小模型生成优化 kernel。对比优化前后的吞吐。记录哪些 kernel 发生了变化 |
| 12 | **CUDA graphs** | 把简单 Transformer 的 decode 循环包进 CUDA graph。测量优化前后的 kernel 启动开销。目标：每 token 总启动开销 <10 µs |

---


<details>
<summary>English original</summary>

**9. Complete Optimization Checklist**

```
Before deployment — run through this checklist:

□ Model Selection
  □ Model fits in INT4 with room for KV cache
  □ GQA-based model preferred (smaller KV cache)
  □ Context length budgeted against available memory

□ Quantization
  □ AWQ or GPTQ 4-bit for best quality/size
  □ GGUF Q4_K_M for llama.cpp deployment
  □ Calibration data representative of production input

□ KV Cache
  □ INT8 KV cache enabled
  □ Maximum context length capped to fit memory
  □ GQA model chosen to minimize KV memory

□ Inference Engine
  □ llama.cpp with -ngl 99 (full GPU offload)
  □ Or TensorRT-LLM engine compiled for target batch/context
  □ FlashAttention enabled

□ Kernel Optimization
  □ Profile with nsys to find top 3 bottleneck kernels
  □ Fused ops enabled (RMSNorm+residual, SwiGLU, rotary)
  □ CUDA graphs enabled for decode loop (reduce launch overhead)
  □ Tile sizes appropriate for Orin Nano SM (64×64, not 256×128)
  □ Consider autokernel/Forge for automated kernel tuning

□ System Configuration
  □ nvpmodel set to appropriate power mode
  □ jetson_clocks to lock frequencies
  □ CMA reduced (LLM doesn't need large CMA)
  □ Unnecessary services disabled (GUI, bluetooth)

□ Profiling
  □ tegrastats monitored during inference
  □ Tokens/sec measured at steady state
  □ Memory usage verified (no slow growth / leak)
  □ Thermal verified (no throttling under sustained load)
```

---

**10. Benchmark Reference**

Expected performance on Orin Nano Super 8 GB (25W mode, Q4_K_M, llama.cpp):

| Model | Params | GGUF size | Prompt eval | Generation | Context |
|-------|--------|-----------|-------------|-----------|---------|
| TinyLlama 1.1B | 1.1B | 0.6 GB | ~200 tok/s | ~65 tok/s | 2048 |
| Llama 3.2 1B | 1.3B | 0.7 GB | ~170 tok/s | ~55 tok/s | 2048 |
| Gemma 2 2B | 2.6B | 1.5 GB | ~85 tok/s | ~35 tok/s | 2048 |
| Llama 3.2 3B | 3.2B | 1.8 GB | ~65 tok/s | ~25 tok/s | 2048 |
| Phi-3 Mini 3.8B | 3.8B | 2.2 GB | ~50 tok/s | ~20 tok/s | 2048 |

> These are estimates for Orin Nano Super at 25W. The ~1.7× improvement over the original Orin Nano comes from 102 GB/s bandwidth (vs 68 GB/s) and higher clock speeds. Actual performance depends on power mode, thermal design, context length, and prompt content. Always benchmark your specific configuration.

---

**11. Projects**

| # | Project | What you learn |
|---|---------|---------------|
| 1 | **llama.cpp on Jetson** | Download Llama 3.2 3B Q4_K_M, build llama.cpp with CUDA, measure tokens/sec at different context lengths |
| 2 | **Quantization comparison** | Run same model at Q2_K, Q4_K_M, Q6_K, Q8_0. Measure tokens/sec, memory, and output quality (perplexity) |
| 3 | **TensorRT-LLM engine** | Build a TRT-LLM engine for Phi-3 Mini INT4. Compare latency with llama.cpp on same prompts |
| 4 | **Speculative decoding** | Set up TinyLlama as draft for Llama 3.2 3B. Measure acceptance rate and speedup |
| 5 | **Memory budget audit** | Run tegrastats during inference. Map every MB: OS, CMA, model, KV cache, activations. Verify against Section 3.3 |
| 6 | **Power vs performance** | Benchmark same model at 7W, 15W, 25W (if Orin NX). Plot tokens/sec vs power. Calculate tokens/joule |
| 7 | **Context length scaling** | Measure tokens/sec at context 512, 1024, 2048, 4096. Plot. Identify where KV cache pressure causes degradation |
| 8 | **Production chatbot** | Build a REST API serving Llama 3.2 3B on Jetson using llama.cpp server mode. Measure P50/P95 latency under concurrent requests |
| 9 | **Nsight profile analysis** | Profile llama.cpp inference with `nsys`. Identify top 3 kernels by time. Calculate SM utilization. Find memory-bound vs compute-bound kernels |
| 10 | **Fused RMSNorm Triton kernel** | Write the fused RMSNorm+residual kernel from Section 8.4. Benchmark against unfused PyTorch version. Measure DRAM traffic reduction |
| 11 | **Autokernel on Jetson** | Use autokernel to generate optimized kernels for a small model on Orin Nano. Compare throughput before/after. Document which kernels changed |
| 12 | **CUDA graphs** | Wrap the decode loop of a simple transformer in a CUDA graph. Measure kernel launch overhead before/after. Target: <10 µs total launch per token |

---

</details>

## 12. Resources

| 资源 | 涵盖内容 |
|----------|---------------|
| [llama.cpp](https://github.com/ggerganov/llama.cpp) | 边缘端最佳开源大语言模型推理引擎 |
| [TensorRT-LLM](https://github.com/NVIDIA/TensorRT-LLM) | NVIDIA 优化的 LLM 引擎 |
| [NVIDIA Jetson AI Lab](https://www.jetson-ai-lab.com/) | 面向 Jetson 上 LLM 的预构建容器与教程 |
| [Jetson Generative AI Playground](https://developer.nvidia.com/embedded/generative-ai) | NVIDIA 的 Jetson LLM 部署指南 |
| [AutoAWQ](https://github.com/casper-hansen/AutoAWQ) | AWQ 量化库 |
| [FlashAttention-2 paper](https://arxiv.org/abs/2307.08691) | 融合 attention 背后的算法 |
| [Speculative Decoding paper](https://arxiv.org/abs/2302.01318) | 投机解码原始论文 |
| [vLLM paper (PagedAttention)](https://arxiv.org/abs/2309.06180) | KV cache 内存管理（服务端参考） |
| [Orin Nano Memory Architecture](../../../01-Nvidia-Jetson平台/07-Orin-Nano内存架构/Guide.md) | 统一内存深入剖析（本路线图） |
| [RightNow Autokernel](https://github.com/RightNow-AI/autokernel) | 开源自动化 GPU kernel 优化 |
| [RightNow Forge](https://www.rightnowai.co/forge) | 企业级 kernel 优化平台（profile → generate → verify） |
| [Triton Language](https://triton-lang.org/) | 基于 Python 的 GPU kernel 语言（比 CUDA 更易上手） |
| [CUDA Graphs Programming Guide](https://docs.nvidia.com/cuda/cuda-c-programming-guide/#cuda-graphs) | 捕获并重放 kernel 序列以降低启动开销 |
| [RunInfra](https://www.runinfra.com/) | 云端 LLM 优化平台（技术参考） |

---

## Next

→ 返回 [ML and AI hub](../Guide.md)


<details>
<summary>English original</summary>

**12. Resources**

| Resource | What it covers |
|----------|---------------|
| [llama.cpp](https://github.com/ggerganov/llama.cpp) | Best open-source LLM inference engine for edge |
| [TensorRT-LLM](https://github.com/NVIDIA/TensorRT-LLM) | NVIDIA's optimized LLM engine |
| [NVIDIA Jetson AI Lab](https://www.jetson-ai-lab.com/) | Pre-built containers and tutorials for LLMs on Jetson |
| [Jetson Generative AI Playground](https://developer.nvidia.com/embedded/generative-ai) | NVIDIA's LLM deployment guides for Jetson |
| [AutoAWQ](https://github.com/casper-hansen/AutoAWQ) | AWQ quantization library |
| [FlashAttention-2 paper](https://arxiv.org/abs/2307.08691) | Algorithm behind fused attention |
| [Speculative Decoding paper](https://arxiv.org/abs/2302.01318) | Original speculative decoding paper |
| [vLLM paper (PagedAttention)](https://arxiv.org/abs/2309.06180) | KV cache memory management (server reference) |
| [Orin Nano Memory Architecture](../../../01-Nvidia-Jetson平台/07-Orin-Nano内存架构/Guide.md) | Unified memory deep dive (this roadmap) |
| [RightNow Autokernel](https://github.com/RightNow-AI/autokernel) | Open-source automated GPU kernel optimization |
| [RightNow Forge](https://www.rightnowai.co/forge) | Enterprise kernel optimization platform (profile → generate → verify) |
| [Triton Language](https://triton-lang.org/) | Python-based GPU kernel language (easier than CUDA) |
| [CUDA Graphs Programming Guide](https://docs.nvidia.com/cuda/cuda-c-programming-guide/#cuda-graphs) | Capture and replay kernel sequences for reduced launch overhead |
| [RunInfra](https://www.runinfra.com/) | Cloud LLM optimization platform (reference for techniques) |

---

**Next**

→ Back to [ML and AI hub](../Guide.md)

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/5. Application Development/5. ML and AI/llm-optimization-jetson/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/5.%20Application%20Development/5.%20ML%20and%20AI/llm-optimization-jetson/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
