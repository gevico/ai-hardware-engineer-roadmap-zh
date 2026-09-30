---
title: 测试
description: 测试
---

# 测试

## 测试套件概览

| 测试 | 文件 | 需要模型？ | 需要 GPU？ | 测试内容 |
|------|------|-------------|-----------|-------|
| `test_memory` | `tests/test_memory.cpp` | No | Yes (cudaMallocHost) | 预算探测、OOM 防护、scratch pool |
| `test_kernels` | `tests/test_kernels.cu` | No | Yes | Softmax、RoPE、RMSNorm、FP16→INT8、SwiGLU |
| `test_model_load` | `tests/test_model_load.cpp` | Yes | Yes | 配置、tokenizer、权重、内存 |
| `test_plan.sh` | `scripts/test_plan.sh` | 自动下载 | Yes | 以上全部 + 推理 + server |

## 运行测试

```bash
# Unit tests (no model needed)
./build/test_memory
./build/test_kernels

# Model loading test (needs GGUF file)
./build/test_model_load models/tinyllama.gguf

# Full automated test plan (33 tests, 7 phases)
./scripts/test_plan.sh
```

## test_memory — 内存子系统

**3 个测试，无需模型：**

### Test 1: probe_system_memory

- 读取 `/proc/meminfo`
- 断言 `total_mb > 0` 和 `free_mb() > 0`
- 打印内存预算表

### Test 2: OOMGuard

- 创建带 256 MB 安全余量的防护
- 断言 `real_free_mb() > 0`
- 打印实际空闲内存

### Test 3: ScratchPool

- 通过 `cudaMallocHost` 分配 64 MB pool
- 获取两个缓冲区（1024 和 2048 字节）
- 验证两者非空且不同
- 验证 `used()` 返回正确的总数（3072）
- 验证 `reset()` 将 `used()` 置为 0

## test_kernels — CUDA kernel 正确性

**5 个测试，需要 GPU，无需模型：**

### Test 1: Softmax

- 输入：1024 个值，线性递增
- 预期：所有值 ≥ 0，和为 1.0（误差在 1e-4 以内）

### Test 2: RoPE（位置 0 恒等）

- 输入：全 1.0，位置 = 0
- 预期：输出 ≈ 1.0（cos(0)=1，sin(0)=0 → 恒等）

### Test 3: Fused RMSNorm

- 输入：x = 1.0，residual = 0.0，weight = 1.0
- 预期：输出 ≈ 1.0（全 1 的 RMS = 1.0，归一化后 = 1.0）

### Test 4: FP16 → INT8

- 输入：一行 0.5
- 预期：scale ≈ 0.5/127 ≈ 0.00394

### Test 5: SwiGLU

- 输入：gate = 1.0，up = 2.0
- 预期：silu(1.0) × 2.0 = 0.7311 × 2.0 ≈ 1.4621

## test_model_load — 完整加载流水线

**8 个测试，需要 GGUF 文件：**

### Test 1: 系统探测
- 验证 `probe_jetson()` 返回有效信息
- 打印 L4T 版本、CUDA 版本、SM 数量、RAM

### Test 2: 内存预算
- 验证预算读取的是 `/proc/meminfo` 的真实值
- 断言 `total_mb > 0`、`free_mb() > 0`

### Test 3: GGUF 配置解析
- 从 GGUF 元数据读取模型架构
- 断言 `n_layers > 0`、`n_heads > 0`、`hidden_dim > 0`、`vocab_size > 0`
- 打印所有配置值

### Test 4: 权重大小估算
- 计算估算的权重字节数
- 检查模型能否装进可用内存

### Test 5: KV cache 上下文计算
- 计算 FP16 和 INT8 KV cache 的最大上下文
- 断言 INT8 上下文 ≥ FP16 上下文

### Test 6: Tokenizer
- 从 GGUF 加载词表
- 断言词表大小 > 0
- 测试编码 "Hello" 并打印 token ID
- 测试解码回文本

### Test 7: 权重加载与映射
- `load_and_map_weights()` — mmap + cudaHostRegister + 张量映射
- 检查 `tok_embd`、`output_norm`、`output` 非空
- 统计已映射 QKV 权重的 layer 数

### Test 8: 功耗与热管理
- 读取功耗模式和 GPU 频率
- 读取温度
- 检查降频建议

## test_plan.sh — 自动化完整测试

7 个阶段共 33 个测试，全自动。完整说明见 `TESTING.md`。

```
Phase 0: System (6 tests)   — Jetson? CUDA? GPU? RAM? Power? Temperature?
Phase 1: Build (3 tests)    — cmake? make? binaries?
Phase 2: Unit (3 tests)     — memory, kernels, budget values
Phase 3: Model (3 tests)    — download/verify GGUF
Phase 4: Loading (6 tests)  — config, tokenizer, weights, memory
Phase 5: Inference (6 tests) — generation, tok/s, OOM, CUDA errors, stability
Phase 6: Server (4 tests)   — health, models, chat completion
Phase 7: Thermal (2 tests)  — temperature, throttling
```

## 调试失败

### 输出乱码（随机 token）

1. 检查张量偏移：`./build/test_model_load model.gguf` — 参见 Test 7
2. 检查实际张量名与预期模式是否一致（见 `docs/gguf.md`）
3. Profile：`nsys profile ./build/jetson-llm -m model.gguf -p "Hi" -n 5`

### 段错误

1. 检查空权重指针：Test 7 的输出会对未映射的张量显示 `(nil)`
2. 用以下方式运行：`cuda-memcheck ./build/jetson-llm -m model.gguf -p "Hi" -n 5`

### OOM

1. 检查内存预算：`./build/test_memory` — `FREE` 是否 > 模型大小 + 1 GB？
2. 关闭 GUI：`sudo systemctl set-default multi-user.target`
3. 减少 CMA：向 kernel 命令行添加 `cma=256M`

### token 数量错误

1. 将 tokenizer 输出与参考对比：
   ```python
   from transformers import AutoTokenizer
   t = AutoTokenizer.from_pretrained("TinyLlama/TinyLlama-1.1B-Chat-v1.0")
   print(t.encode("Hello"))
   ```
2. 检查 BOS/EOS ID 是否与 GGUF 元数据一致


<details>
<summary>English original</summary>

**Testing**

**Test Suite Overview**

| Test | File | Needs model? | Needs GPU? | Tests |
|------|------|-------------|-----------|-------|
| `test_memory` | `tests/test_memory.cpp` | No | Yes (cudaMallocHost) | Budget probe, OOM guard, scratch pool |
| `test_kernels` | `tests/test_kernels.cu` | No | Yes | Softmax, RoPE, RMSNorm, FP16→INT8, SwiGLU |
| `test_model_load` | `tests/test_model_load.cpp` | Yes | Yes | Config, tokenizer, weights, memory |
| `test_plan.sh` | `scripts/test_plan.sh` | Auto-downloads | Yes | All of the above + inference + server |

**Running Tests**

```bash
# Unit tests (no model needed)
./build/test_memory
./build/test_kernels

# Model loading test (needs GGUF file)
./build/test_model_load models/tinyllama.gguf

# Full automated test plan (33 tests, 7 phases)
./scripts/test_plan.sh
```

**test_memory — Memory Subsystem**

**3 tests, no model needed:**

**Test 1: probe_system_memory**

- Reads `/proc/meminfo`
- Asserts `total_mb > 0` and `free_mb() > 0`
- Prints memory budget table

**Test 2: OOMGuard**

- Creates guard with 256 MB safety margin
- Asserts `real_free_mb() > 0`
- Prints actual free memory

**Test 3: ScratchPool**

- Allocates 64 MB pool via `cudaMallocHost`
- Gets two buffers (1024 and 2048 bytes)
- Verifies they're non-null and different
- Verifies `used()` returns correct total (3072)
- Verifies `reset()` returns `used()` to 0

**test_kernels — CUDA Kernel Correctness**

**5 tests, needs GPU, no model:**

**Test 1: Softmax**

- Input: 1024 values, linear ramp
- Expected: all values ≥ 0, sum = 1.0 (within 1e-4)

**Test 2: RoPE (position 0 identity)**

- Input: all 1.0, position = 0
- Expected: output ≈ 1.0 (cos(0)=1, sin(0)=0 → identity)

**Test 3: Fused RMSNorm**

- Input: x = 1.0, residual = 0.0, weight = 1.0
- Expected: output ≈ 1.0 (RMS of all-ones = 1.0, normalized = 1.0)

**Test 4: FP16 → INT8**

- Input: row of 0.5 values
- Expected: scale ≈ 0.5/127 ≈ 0.00394

**Test 5: SwiGLU**

- Input: gate = 1.0, up = 2.0
- Expected: silu(1.0) × 2.0 = 0.7311 × 2.0 ≈ 1.4621

**test_model_load — Full Loading Pipeline**

**8 tests, needs GGUF file:**

**Test 1: System probe**
- Verifies `probe_jetson()` returns valid info
- Prints L4T version, CUDA version, SM count, RAM

**Test 2: Memory budget**
- Verifies budget reads real values from `/proc/meminfo`
- Asserts `total_mb > 0`, `free_mb() > 0`

**Test 3: GGUF config parsing**
- Reads model architecture from GGUF metadata
- Asserts `n_layers > 0`, `n_heads > 0`, `hidden_dim > 0`, `vocab_size > 0`
- Prints all config values

**Test 4: Weight size estimate**
- Calculates estimated weight bytes
- Checks if model fits in available memory

**Test 5: KV cache context calculation**
- Computes max context for FP16 and INT8 KV cache
- Asserts INT8 context ≥ FP16 context

**Test 6: Tokenizer**
- Loads vocabulary from GGUF
- Asserts vocab size > 0
- Test encodes "Hello" and prints token IDs
- Test decodes back to text

**Test 7: Weight loading and mapping**
- `load_and_map_weights()` — mmap + cudaHostRegister + tensor mapping
- Checks `tok_embd`, `output_norm`, `output` are non-null
- Counts layers with QKV weights mapped

**Test 8: Power and thermal**
- Reads power mode and GPU frequency
- Reads temperatures
- Checks backoff recommendation

**test_plan.sh — Automated Full Test**

33 tests across 7 phases, automated. See `TESTING.md` for full description.

```
Phase 0: System (6 tests)   — Jetson? CUDA? GPU? RAM? Power? Temperature?
Phase 1: Build (3 tests)    — cmake? make? binaries?
Phase 2: Unit (3 tests)     — memory, kernels, budget values
Phase 3: Model (3 tests)    — download/verify GGUF
Phase 4: Loading (6 tests)  — config, tokenizer, weights, memory
Phase 5: Inference (6 tests) — generation, tok/s, OOM, CUDA errors, stability
Phase 6: Server (4 tests)   — health, models, chat completion
Phase 7: Thermal (2 tests)  — temperature, throttling
```

**Debugging Failures**

**Garbage output (random tokens)**

1. Check tensor offsets: `./build/test_model_load model.gguf` — look at Test 7
2. Inspect actual tensor names vs expected patterns (see `docs/gguf.md`)
3. Profile: `nsys profile ./build/jetson-llm -m model.gguf -p "Hi" -n 5`

**Segfault**

1. Check null weight pointers: Test 7 output shows `(nil)` for unmapped tensors
2. Run with: `cuda-memcheck ./build/jetson-llm -m model.gguf -p "Hi" -n 5`

**OOM**

1. Check memory budget: `./build/test_memory` — is `FREE` > model size + 1 GB?
2. Disable GUI: `sudo systemctl set-default multi-user.target`
3. Reduce CMA: add `cma=256M` to kernel command line

**Wrong token count**

1. Compare tokenizer output with reference:
   ```python
   from transformers import AutoTokenizer
   t = AutoTokenizer.from_pretrained("TinyLlama/TinyLlama-1.1B-Chat-v1.0")
   print(t.encode("Hello"))
   ```
2. Check BOS/EOS IDs match GGUF metadata

</details>

---

> 原文：[`Projects/jetson-llm-runtime/docs/testing.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/docs/testing.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
