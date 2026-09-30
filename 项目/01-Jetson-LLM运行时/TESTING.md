---
title: 测试指南 — jetson-llm
description: 测试指南 — jetson-llm
---

# 测试指南 — jetson-llm

从首次构建到生产验证的分步测试。

---

## 步骤 0：推荐的测试模型

**TinyLlama 1.1B Q4_K_M** — 初始测试的最佳模型：

| 属性 | 值 | 适合测试的原因 |
|----------|-------|--------------------------|
| 参数量 | 1.1B | 足够小，加载快、易调试 |
| Q4_K_M 大小 | **669 MB** | 留出约 5 GB 空闲 — 无 OOM 风险 |
| 词表 | 32,000（Llama tokenizer） | 标准、久经测试的 GGUF 格式 |
| 架构 | Llama 风格 | tensor 名称与 Llama 3.2（目标模型）相同 |
| 上下文 | 2048 | 短 — prefill（首字前的整段计算）快 |
| 质量 | 对 1B 而言尚可 | 能区分连贯输出与乱码 |
| 下载 | ~670 MB | 通过 USB 传输快 |

```bash
# Download on internet-connected machine
wget -O TinyLlama-1.1B-Q4_K_M.gguf \
  "https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF/resolve/main/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf"

# Transfer to Jetson
scp TinyLlama-1.1B-Q4_K_M.gguf user@192.168.55.1:/opt/models/
```

**TinyLlama 跑通后**，再升级到真正的目标：

| 阶段 | 模型 | 大小 | 目的 |
|-------|-------|------|---------|
| **1. 首次启动** | TinyLlama 1.1B Q4_K_M | 669 MB | 验证加载、tokenizer、基本输出 |
| **2. 正确性** | Llama 3.2 1B Q4_K_M | 750 MB | 验证现代 GGUF 格式 + GQA |
| **3. 目标模型** | Llama 3.2 3B Q4_K_M | 1.8 GB | 实际部署模型 |
| **4. 压力测试** | Phi-3 Mini 3.8B Q4_K_M | 2.3 GB | 模型更大，内存更紧张 |
| **5. 极限测试** | Llama 3.3 8B Q4_K_M | 4.6 GB | 可容纳的边界 — 测试 OOM 防护 |

---

## 步骤 1：在 Jetson 上构建

```bash
# SSH into Jetson
ssh user@192.168.55.1

# Clone or copy the project
cd /opt
git clone <your-repo-url> jetson-llm-runtime
cd jetson-llm-runtime

# First-time setup (sets power mode, checks system)
./scripts/setup_jetson.sh

# Or manual build
cmake -B build \
    -DGGML_CUDA=ON \
    -DCMAKE_CUDA_ARCHITECTURES="87" \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

**预期输出：**

```
-- The C compiler identification is GNU 11.4.0
-- The CXX compiler identification is GNU 11.4.0
-- The CUDA compiler identification is NVIDIA 12.6
-- CUDA architectures: 87
...
[100%] Built target jetson-llm
```

**若构建失败：**
- `nvcc not found` → 安装：`sudo apt install cuda-toolkit-12-6`
- `aarch64 check fails` → 你在 x86 上交叉编译（不支持，请在 Jetson 上构建）
- `cuda_fp16.h not found` → CUDA toolkit 不完整：`sudo apt install cuda-*-12-6`

---

## 步骤 2：测试内存子系统（无需模型）

```bash
./build/test_memory
```

**预期输出：**

```
╔══════════════════════════════════╗
║   JLLM Memory Budget             ║
╠══════════════════════════════════╣
║ Total DRAM:      7633 MB         ║
║ OS + kernel:    - 500 MB         ║
║ CMA reserved:  - 768 MB         ║
║ CUDA context:  - 300 MB         ║
║ Model weights: -   0 MB         ║
║ KV cache:      -   0 MB         ║
║ Scratch:       -   0 MB         ║
║ Safety margin: - 256 MB         ║
╠══════════════════════════════════╣
║ FREE:           5809 MB         ║
╚══════════════════════════════════╝
PASS: probe_system_memory
Free: 5814 MB
PASS: OOMGuard
PASS: ScratchPool

All memory tests passed.
```

**需要检查的点：**
- `Total DRAM` 应约为 7633 MB（不是 8192 — 其余被保留区占用）
- 在默认配置的 Orin Nano Super 上（关闭 GUI），`FREE` 应 >5000 MB
- 若 `FREE` < 4000 MB：关闭 GUI、减小 CMA、杀掉不必要的服务

---

## 步骤 3：测试 CUDA kernel（无需模型）

```bash
./build/test_kernels
```

**预期输出：**

```
=== jetson-llm kernel tests ===

GPU: Orin (SM 8.7, 16 SMs)

PASS: softmax (sum = 1.000000)
PASS: rope (pos=0 identity, val[0]=1.0000)
PASS: fused_rmsnorm (val[0]=1.0000, expected ~1.0)
PASS: fp16_to_int8 (scale=0.003937, expected ~0.00394)
PASS: swiglu (val=1.4624, expected ~1.4621)

All kernel tests passed.
```

**若某个测试失败：**
- `CUDA error: no kernel image is available` → 架构不对；用 `-DCMAKE_CUDA_ARCHITECTURES="87"` 重新构建
- `CUDA error: out of memory` → 有其他东西在占用 GPU；检查 `tegrastats`
- 数值结果错误 → kernel bug；带上具体数值提交 issue

---


<details>
<summary>English original</summary>

**Testing Guide — jetson-llm**

Step-by-step testing from first build to production validation.

---

**Step 0: Recommended Test Model**

**TinyLlama 1.1B Q4_K_M** — the best model for initial testing:

| Property | Value | Why it's good for testing |
|----------|-------|--------------------------|
| Parameters | 1.1B | Small enough to load fast, debug easily |
| Q4_K_M size | **669 MB** | Leaves ~5 GB free — no OOM risk |
| Vocab | 32,000 (Llama tokenizer) | Standard, well-tested GGUF format |
| Architecture | Llama-style | Same tensor names as Llama 3.2 (our target) |
| Context | 2048 | Short — fast prefill |
| Quality | Decent for 1B | Can verify coherent output vs garbage |
| Download | ~670 MB | Fast transfer over USB |

```bash
# Download on internet-connected machine
wget -O TinyLlama-1.1B-Q4_K_M.gguf \
  "https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF/resolve/main/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf"

# Transfer to Jetson
scp TinyLlama-1.1B-Q4_K_M.gguf user@192.168.55.1:/opt/models/
```

**After TinyLlama works**, graduate to the real target:

| Stage | Model | Size | Purpose |
|-------|-------|------|---------|
| **1. First boot** | TinyLlama 1.1B Q4_K_M | 669 MB | Validate loading, tokenizer, basic output |
| **2. Correctness** | Llama 3.2 1B Q4_K_M | 750 MB | Verify modern GGUF format + GQA |
| **3. Target model** | Llama 3.2 3B Q4_K_M | 1.8 GB | The actual deployment model |
| **4. Stress test** | Phi-3 Mini 3.8B Q4_K_M | 2.3 GB | Larger model, tighter memory |
| **5. Limit test** | Llama 3.3 8B Q4_K_M | 4.6 GB | Edge of what fits — tests OOM guard |

---

**Step 1: Build on Jetson**

```bash
# SSH into Jetson
ssh user@192.168.55.1

# Clone or copy the project
cd /opt
git clone <your-repo-url> jetson-llm-runtime
cd jetson-llm-runtime

# First-time setup (sets power mode, checks system)
./scripts/setup_jetson.sh

# Or manual build
cmake -B build \
    -DGGML_CUDA=ON \
    -DCMAKE_CUDA_ARCHITECTURES="87" \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

**Expected output:**

```
-- The C compiler identification is GNU 11.4.0
-- The CXX compiler identification is GNU 11.4.0
-- The CUDA compiler identification is NVIDIA 12.6
-- CUDA architectures: 87
...
[100%] Built target jetson-llm
```

**If build fails:**
- `nvcc not found` → install: `sudo apt install cuda-toolkit-12-6`
- `aarch64 check fails` → you're cross-compiling on x86 (not supported, build on Jetson)
- `cuda_fp16.h not found` → CUDA toolkit incomplete: `sudo apt install cuda-*-12-6`

---

**Step 2: Test Memory Subsystem (No Model Needed)**

```bash
./build/test_memory
```

**Expected output:**

```
╔══════════════════════════════════╗
║   JLLM Memory Budget             ║
╠══════════════════════════════════╣
║ Total DRAM:      7633 MB         ║
║ OS + kernel:    - 500 MB         ║
║ CMA reserved:  - 768 MB         ║
║ CUDA context:  - 300 MB         ║
║ Model weights: -   0 MB         ║
║ KV cache:      -   0 MB         ║
║ Scratch:       -   0 MB         ║
║ Safety margin: - 256 MB         ║
╠══════════════════════════════════╣
║ FREE:           5809 MB         ║
╚══════════════════════════════════╝
PASS: probe_system_memory
Free: 5814 MB
PASS: OOMGuard
PASS: ScratchPool

All memory tests passed.
```

**What to check:**
- `Total DRAM` should be ~7633 MB (not 8192 — carveouts take the rest)
- `FREE` should be >5000 MB on stock Orin Nano Super (GUI disabled)
- If `FREE` < 4000 MB: disable GUI, reduce CMA, kill unnecessary services

---

**Step 3: Test CUDA Kernels (No Model Needed)**

```bash
./build/test_kernels
```

**Expected output:**

```
=== jetson-llm kernel tests ===

GPU: Orin (SM 8.7, 16 SMs)

PASS: softmax (sum = 1.000000)
PASS: rope (pos=0 identity, val[0]=1.0000)
PASS: fused_rmsnorm (val[0]=1.0000, expected ~1.0)
PASS: fp16_to_int8 (scale=0.003937, expected ~0.00394)
PASS: swiglu (val=1.4624, expected ~1.4621)

All kernel tests passed.
```

**If a test fails:**
- `CUDA error: no kernel image is available` → wrong arch; rebuild with `-DCMAKE_CUDA_ARCHITECTURES="87"`
- `CUDA error: out of memory` → something else is using GPU; check `tegrastats`
- Wrong numerical result → kernel bug; file an issue with the exact values

---

</details>

## 步骤 4：测试模型加载（需要 GGUF 文件）

```bash
./build/test_model_load /opt/models/TinyLlama-1.1B-Q4_K_M.gguf
```

**预期输出：**

```
=== jetson-llm model loading test ===

Test 1: System probe
╔══════════════════════════════════════╗
║   Jetson LLM Runtime v0.1            ║
║ L4T:    36.4       CUDA: 12.6       ║
║ SMs:    16          Cores: 1024      ║
║ RAM:    7633  MB    CMA: 768  MB    ║
╚══════════════════════════════════════╝
PASS

Test 2: Memory budget
PASS

Test 3: GGUF config parsing
  name:         TinyLlama-1.1B-Chat-v1.0
  n_layers:     22
  n_heads:      32
  n_kv_heads:   4
  head_dim:     64
  hidden_dim:   2048
  inter_dim:    5632
  vocab_size:   32000
  max_seq_len:  2048
  rope_theta:   10000
  GQA group:    8
PASS

Test 4: Weight size estimate
  Estimated: ~630 MB
  Will fit in 5809 MB free? YES
PASS

Test 5: KV cache context calculation
  Max context (FP16 KV): ~18000 tokens
  Max context (INT8 KV): ~36000 tokens
  KV per token (INT8):   11264 bytes
PASS

Test 6: Tokenizer
  Loaded: yes
  Vocab size: 32000
  BOS ID: 1
  EOS ID: 2
  'Hello' → 3 tokens: 1 15043 29871
  Decoded: 'Hello '
PASS

Test 7: Weight loading
  Loaded: yes (669 MB)
  tok_embd:    0x7f8a000000
  output_norm: 0x7f8a100000
  output:      0x7f8a200000
  Layers with QKV: 22 / 22
PASS

Test 8: Power and thermal
  Power mode: 25W, GPU @ 1300 MHz
  GPU: 42.5°C, CPU: 41.0°C, throttling: no
  Backoff: 0 µs
PASS

═══════════════════════════════════════
  All model loading tests passed!
═══════════════════════════════════════
```

**每项测试要检查什么：**

| Test | What it validates | Red flag |
|------|------------------|----------|
| 3 | GGUF 配置 | `n_layers=0` 或 `vocab_size=0` → parser 失败 |
| 5 | 上下文计算 | `Max context < 100` → 内存过于紧张 |
| 6 | Tokenizer | `Loaded: no` → GGUF 中找不到 vocab |
| 6 | 编码 | `'Hello' → 1 token` → tokenizer 异常（应为 2-3） |
| 7 | 权重加载 | `tok_embd: (nil)` → 张量映射失败 |
| 7 | 含 QKV 的 layer | `0 / 22` → 张量名不匹配 |

**若测试 7 显示映射了 `0 / 22` 个 layer：**
这意味着 GGUF 张量名与我们的模式（`blk.N.attn_q.weight`）不匹配。运行以下命令查看实际名称：

```bash
# Inspect tensor names in GGUF (using python)
pip install gguf
python3 -c "
from gguf import GGUFReader
reader = GGUFReader('/opt/models/TinyLlama-1.1B-Q4_K_M.gguf')
for t in reader.tensors:
    print(f'{t.name:50s} {t.tensor_type.name:10s} {t.shape}')
" | head -30
```

把输出的名称与 `load_and_map_weights()` 期望的名称对比，修正模式匹配。

---

## 步骤 5：首次生成测试

```bash
# Short generation — verify output is coherent (not garbage)
./build/jetson-llm \
    -m /opt/models/TinyLlama-1.1B-Q4_K_M.gguf \
    -p "What is 2+2?" \
    -n 32 \
    -v
```

**正常输出（文本连贯）：**
```
What is 2+2? The answer is 4.

--- Stats ---
Prompt:  5 tokens, 85.2 tok/s (58 ms)
Decode:  12 tokens, 28.4 tok/s (422 ms)
Memory:  peak 1823 MB
Thermal: peak 48.2°C
```

**异常输出（存在 bug）：**
```
What is 2+2?ÿÿÿÿÿÿÿÿÿÿ...        ← garbage: tensor offsets wrong (bug #1)
What is 2+2?                          ← no output: decode_step returns EOS immediately
What is 2+2? é«ÿ∂π∑...               ← random tokens: attention accumulator bug (#6)
Segfault                               ← null weight pointer: mapping failed
```

**输出有误时的调试步骤：**

```bash
# 1. Check tensor offsets are sane
./build/test_model_load model.gguf 2>&1 | grep -E "tensor|Mapped|WARNING"

# 2. Profile to see which kernels are running
nsys profile --trace=cuda -o debug \
    ./build/jetson-llm -m model.gguf -p "Hi" -n 5
nsys stats debug.nsys-rep

# 3. Check memory during inference
./build/jetson-llm -m model.gguf -p "Hi" -n 10 &
PID=$!; sleep 1; cat /proc/$PID/status | grep VmRSS; wait $PID

# 4. Compare tokenizer output with reference
python3 -c "
from transformers import AutoTokenizer
tok = AutoTokenizer.from_pretrained('TinyLlama/TinyLlama-1.1B-Chat-v1.0')
ids = tok.encode('What is 2+2?')
print('Reference tokens:', ids)
print('Decoded:', tok.decode(ids))
"
```

---

## 步骤 6：交互式对话测试

```bash
./build/jetson-llm -m /opt/models/TinyLlama-1.1B-Q4_K_M.gguf -i
```

```
> Hello, who are you?
I'm TinyLlama, a small language model...
[12 tokens, 28.4 tok/s, peak 1823 MB, 48.2°C]

> What is the capital of France?
Paris is the capital of France...
[15 tokens, 27.1 tok/s, peak 1830 MB, 49.1°C]

> quit
```

**要检查的内容：**
- tok/s 在多轮之间保持稳定（无衰减）
- 内存不增长（峰值 MB 大致恒定）
- 温度保持在 80°C 以下
- 无 OOM 警告

---


<details>
<summary>English original</summary>

**Step 4: Test Model Loading (Needs GGUF File)**

```bash
./build/test_model_load /opt/models/TinyLlama-1.1B-Q4_K_M.gguf
```

**Expected output:**

```
=== jetson-llm model loading test ===

Test 1: System probe
╔══════════════════════════════════════╗
║   Jetson LLM Runtime v0.1            ║
║ L4T:    36.4       CUDA: 12.6       ║
║ SMs:    16          Cores: 1024      ║
║ RAM:    7633  MB    CMA: 768  MB    ║
╚══════════════════════════════════════╝
PASS

Test 2: Memory budget
PASS

Test 3: GGUF config parsing
  name:         TinyLlama-1.1B-Chat-v1.0
  n_layers:     22
  n_heads:      32
  n_kv_heads:   4
  head_dim:     64
  hidden_dim:   2048
  inter_dim:    5632
  vocab_size:   32000
  max_seq_len:  2048
  rope_theta:   10000
  GQA group:    8
PASS

Test 4: Weight size estimate
  Estimated: ~630 MB
  Will fit in 5809 MB free? YES
PASS

Test 5: KV cache context calculation
  Max context (FP16 KV): ~18000 tokens
  Max context (INT8 KV): ~36000 tokens
  KV per token (INT8):   11264 bytes
PASS

Test 6: Tokenizer
  Loaded: yes
  Vocab size: 32000
  BOS ID: 1
  EOS ID: 2
  'Hello' → 3 tokens: 1 15043 29871
  Decoded: 'Hello '
PASS

Test 7: Weight loading
  Loaded: yes (669 MB)
  tok_embd:    0x7f8a000000
  output_norm: 0x7f8a100000
  output:      0x7f8a200000
  Layers with QKV: 22 / 22
PASS

Test 8: Power and thermal
  Power mode: 25W, GPU @ 1300 MHz
  GPU: 42.5°C, CPU: 41.0°C, throttling: no
  Backoff: 0 µs
PASS

═══════════════════════════════════════
  All model loading tests passed!
═══════════════════════════════════════
```

**What to check at each test:**

| Test | What it validates | Red flag |
|------|------------------|----------|
| 3 | GGUF config | `n_layers=0` or `vocab_size=0` → parser failed |
| 5 | Context calc | `Max context < 100` → memory too tight |
| 6 | Tokenizer | `Loaded: no` → vocab not found in GGUF |
| 6 | Encode | `'Hello' → 1 token` → tokenizer broken (should be 2-3) |
| 7 | Weight load | `tok_embd: (nil)` → tensor mapping failed |
| 7 | Layers with QKV | `0 / 22` → tensor names don't match |

**If Test 7 shows `0 / 22` layers mapped:**
This means the GGUF tensor names don't match our pattern (`blk.N.attn_q.weight`). Run this to see actual names:

```bash
# Inspect tensor names in GGUF (using python)
pip install gguf
python3 -c "
from gguf import GGUFReader
reader = GGUFReader('/opt/models/TinyLlama-1.1B-Q4_K_M.gguf')
for t in reader.tensors:
    print(f'{t.name:50s} {t.tensor_type.name:10s} {t.shape}')
" | head -30
```

Compare the output names with what `load_and_map_weights()` expects and fix the pattern matching.

---

**Step 5: First Generation Test**

```bash
# Short generation — verify output is coherent (not garbage)
./build/jetson-llm \
    -m /opt/models/TinyLlama-1.1B-Q4_K_M.gguf \
    -p "What is 2+2?" \
    -n 32 \
    -v
```

**Good output (coherent text):**
```
What is 2+2? The answer is 4.

--- Stats ---
Prompt:  5 tokens, 85.2 tok/s (58 ms)
Decode:  12 tokens, 28.4 tok/s (422 ms)
Memory:  peak 1823 MB
Thermal: peak 48.2°C
```

**Bad output (bugs present):**
```
What is 2+2?ÿÿÿÿÿÿÿÿÿÿ...        ← garbage: tensor offsets wrong (bug #1)
What is 2+2?                          ← no output: decode_step returns EOS immediately
What is 2+2? é«ÿ∂π∑...               ← random tokens: attention accumulator bug (#6)
Segfault                               ← null weight pointer: mapping failed
```

**Debug steps if output is wrong:**

```bash
# 1. Check tensor offsets are sane
./build/test_model_load model.gguf 2>&1 | grep -E "tensor|Mapped|WARNING"

# 2. Profile to see which kernels are running
nsys profile --trace=cuda -o debug \
    ./build/jetson-llm -m model.gguf -p "Hi" -n 5
nsys stats debug.nsys-rep

# 3. Check memory during inference
./build/jetson-llm -m model.gguf -p "Hi" -n 10 &
PID=$!; sleep 1; cat /proc/$PID/status | grep VmRSS; wait $PID

# 4. Compare tokenizer output with reference
python3 -c "
from transformers import AutoTokenizer
tok = AutoTokenizer.from_pretrained('TinyLlama/TinyLlama-1.1B-Chat-v1.0')
ids = tok.encode('What is 2+2?')
print('Reference tokens:', ids)
print('Decoded:', tok.decode(ids))
"
```

---

**Step 6: Interactive Chat Test**

```bash
./build/jetson-llm -m /opt/models/TinyLlama-1.1B-Q4_K_M.gguf -i
```

```
> Hello, who are you?
I'm TinyLlama, a small language model...
[12 tokens, 28.4 tok/s, peak 1823 MB, 48.2°C]

> What is the capital of France?
Paris is the capital of France...
[15 tokens, 27.1 tok/s, peak 1830 MB, 49.1°C]

> quit
```

**What to check:**
- tok/s is stable across turns (no degradation)
- Memory doesn't grow (peak MB stays roughly constant)
- Temperature stays below 80°C
- No OOM warnings

---

</details>

## 步骤 7：API Server 测试

```bash
# Terminal 1: start server
./build/jetson-llm-server -m /opt/models/TinyLlama-1.1B-Q4_K_M.gguf -p 8080

# Terminal 2: test endpoints
# Health check
curl -s http://localhost:8080/health | python3 -m json.tool

# Chat completion
curl -s http://localhost:8080/v1/chat/completions \
    -H "Content-Type: application/json" \
    -d '{"messages":[{"role":"user","content":"What is 2+2?"}]}' \
    | python3 -m json.tool

# Model list
curl -s http://localhost:8080/v1/models | python3 -m json.tool
```

**预期 /health 响应：**
```json
{
    "status": "ok",
    "model": "TinyLlama-1.1B-Chat-v1.0",
    "memory": {
        "total_mb": 7633,
        "free_mb": 4200,
        "model_mb": 669,
        "kv_mb": 45
    },
    "thermal": {
        "gpu_c": 48.5,
        "cpu_c": 47.0,
        "throttling": false
    },
    "power": {
        "mode": "25W",
        "gpu_mhz": 1300
    },
    "gpu_util_pct": 75
}
```

---

## 步骤 8：Benchmark

```bash
./scripts/bench.sh /opt/models/TinyLlama-1.1B-Q4_K_M.gguf
```

记录数值，并在不同改动之间对比：

| 指标 | 目标（TinyLlama 1.1B） | 你的结果 |
|--------|------------------------|-------------|
| Prompt 评测（512 tok） | >80 tok/s | _____ tok/s |
| Decode（128 tok） | >30 tok/s | _____ tok/s |
| 峰值内存 | <2500 MB | _____ MB |
| 峰值温度 | <75°C | _____ °C |

---

## 步骤 9：进阶到目标模型

TinyLlama 跑通后，测试真正的目标模型：

```bash
# Download Llama 3.2 3B
wget -O Llama-3.2-3B-Q4_K_M.gguf \
  "https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-Q4_K_M.gguf"

scp Llama-3.2-3B-Q4_K_M.gguf user@192.168.55.1:/opt/models/

# Run tests
./build/test_model_load /opt/models/Llama-3.2-3B-Q4_K_M.gguf
./build/jetson-llm -m /opt/models/Llama-3.2-3B-Q4_K_M.gguf -p "Hello" -n 32
./scripts/bench.sh /opt/models/Llama-3.2-3B-Q4_K_M.gguf
```

| 指标 | 目标（Llama 3.2 3B） | 你的结果 |
|--------|----------------------|-------------|
| Prompt 评测（512 tok） | >40 tok/s | _____ tok/s |
| Decode（128 tok） | >15 tok/s | _____ tok/s |
| 峰值内存 | <4000 MB | _____ MB |

---

## 步骤 10：压力测试

```bash
# Long generation — tests KV cache growth and thermal stability
./build/jetson-llm -m /opt/models/Llama-3.2-3B-Q4_K_M.gguf \
    -p "Write a very long and detailed essay about the history of computing." \
    -n 1024 -v

# Check for:
#   - Memory stable (no growth after initial load)
#   - No OOM guard triggers
#   - No thermal throttling
#   - tok/s consistent throughout

# Overnight stability (production validation)
while true; do
    curl -s http://localhost:8080/v1/chat/completions \
        -d '{"messages":[{"role":"user","content":"Tell me a joke"}],"max_tokens":64}' \
        | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('jetson',{}).get('decode_tok_s','?'), 'tok/s')"
    sleep 2
done
# Run for 24 hours. Check tegrastats for memory leaks.
```

---

## 快速参考：全部测试命令

```bash
# 1. No model needed
./build/test_memory
./build/test_kernels

# 2. Needs GGUF model
./build/test_model_load model.gguf

# 3. Generation
./build/jetson-llm -m model.gguf -p "Hello" -n 32
./build/jetson-llm -m model.gguf -i

# 4. Server
./build/jetson-llm-server -m model.gguf -p 8080
curl http://localhost:8080/health

# 5. Benchmark + profile
./scripts/bench.sh model.gguf
./scripts/profile.sh model.gguf
```


<details>
<summary>English original</summary>

**Step 7: API Server Test**

```bash
# Terminal 1: start server
./build/jetson-llm-server -m /opt/models/TinyLlama-1.1B-Q4_K_M.gguf -p 8080

# Terminal 2: test endpoints
# Health check
curl -s http://localhost:8080/health | python3 -m json.tool

# Chat completion
curl -s http://localhost:8080/v1/chat/completions \
    -H "Content-Type: application/json" \
    -d '{"messages":[{"role":"user","content":"What is 2+2?"}]}' \
    | python3 -m json.tool

# Model list
curl -s http://localhost:8080/v1/models | python3 -m json.tool
```

**Expected /health response:**
```json
{
    "status": "ok",
    "model": "TinyLlama-1.1B-Chat-v1.0",
    "memory": {
        "total_mb": 7633,
        "free_mb": 4200,
        "model_mb": 669,
        "kv_mb": 45
    },
    "thermal": {
        "gpu_c": 48.5,
        "cpu_c": 47.0,
        "throttling": false
    },
    "power": {
        "mode": "25W",
        "gpu_mhz": 1300
    },
    "gpu_util_pct": 75
}
```

---

**Step 8: Benchmark**

```bash
./scripts/bench.sh /opt/models/TinyLlama-1.1B-Q4_K_M.gguf
```

Record the numbers and compare across changes:

| Metric | Target (TinyLlama 1.1B) | Your result |
|--------|------------------------|-------------|
| Prompt eval (512 tok) | >80 tok/s | _____ tok/s |
| Decode (128 tok) | >30 tok/s | _____ tok/s |
| Peak memory | <2500 MB | _____ MB |
| Peak temperature | <75°C | _____ °C |

---

**Step 9: Graduate to Target Model**

Once TinyLlama works, test the actual target:

```bash
# Download Llama 3.2 3B
wget -O Llama-3.2-3B-Q4_K_M.gguf \
  "https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-Q4_K_M.gguf"

scp Llama-3.2-3B-Q4_K_M.gguf user@192.168.55.1:/opt/models/

# Run tests
./build/test_model_load /opt/models/Llama-3.2-3B-Q4_K_M.gguf
./build/jetson-llm -m /opt/models/Llama-3.2-3B-Q4_K_M.gguf -p "Hello" -n 32
./scripts/bench.sh /opt/models/Llama-3.2-3B-Q4_K_M.gguf
```

| Metric | Target (Llama 3.2 3B) | Your result |
|--------|----------------------|-------------|
| Prompt eval (512 tok) | >40 tok/s | _____ tok/s |
| Decode (128 tok) | >15 tok/s | _____ tok/s |
| Peak memory | <4000 MB | _____ MB |

---

**Step 10: Stress Test**

```bash
# Long generation — tests KV cache growth and thermal stability
./build/jetson-llm -m /opt/models/Llama-3.2-3B-Q4_K_M.gguf \
    -p "Write a very long and detailed essay about the history of computing." \
    -n 1024 -v

# Check for:
#   - Memory stable (no growth after initial load)
#   - No OOM guard triggers
#   - No thermal throttling
#   - tok/s consistent throughout

# Overnight stability (production validation)
while true; do
    curl -s http://localhost:8080/v1/chat/completions \
        -d '{"messages":[{"role":"user","content":"Tell me a joke"}],"max_tokens":64}' \
        | python3 -c "import sys,json; d=json.load(sys.stdin); print(d.get('jetson',{}).get('decode_tok_s','?'), 'tok/s')"
    sleep 2
done
# Run for 24 hours. Check tegrastats for memory leaks.
```

---

**Quick Reference: All Test Commands**

```bash
# 1. No model needed
./build/test_memory
./build/test_kernels

# 2. Needs GGUF model
./build/test_model_load model.gguf

# 3. Generation
./build/jetson-llm -m model.gguf -p "Hello" -n 32
./build/jetson-llm -m model.gguf -i

# 4. Server
./build/jetson-llm-server -m model.gguf -p 8080
curl http://localhost:8080/health

# 5. Benchmark + profile
./scripts/bench.sh model.gguf
./scripts/profile.sh model.gguf
```

</details>

---

> 原文：[`Projects/jetson-llm-runtime/TESTING.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/TESTING.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
