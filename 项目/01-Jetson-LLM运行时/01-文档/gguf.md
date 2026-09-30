---
title: GGUF 格式解析
description: GGUF 格式解析
---

# GGUF 格式解析

## GGUF 文件结构

```
┌──────────────────────────────┐
│ Header (24 bytes)            │
│   magic: 0x46475547 "GGUF"  │
│   version: uint32            │
│   n_tensors: uint64          │
│   n_kv: uint64               │
├──────────────────────────────┤
│ Metadata KV pairs (variable) │
│   For each KV:               │
│     key_len: uint64          │
│     key: string              │
│     value_type: uint32       │
│     value: (type-dependent)  │
├──────────────────────────────┤
│ Tensor info (variable)       │
│   For each tensor:           │
│     name_len: uint64         │
│     name: string             │
│     n_dims: uint32           │
│     shape[n_dims]: int64[]   │
│     type: uint32 (GGMLType)  │
│     offset: uint64           │
├──────────────────────────────┤
│ Alignment padding            │
├──────────────────────────────┤
│ Tensor data                  │
│   (raw weight bytes at       │
│    offsets specified above)   │
└──────────────────────────────┘
```

## 解析内容

### 1. 模型配置（`load_gguf_config`）

读取匹配以下 key 的元数据 KV 对：

| GGUF Key | 映射到 | 示例 |
|----------|---------|---------|
| `*.block_count` | `n_layers` | 22 |
| `*.head_count` | `n_heads` | 32 |
| `*.head_count_kv` | `n_kv_heads` | 4 |
| `*.embedding_length` | `hidden_dim` | 2048 |
| `*.feed_forward_length` | `intermediate_dim` | 5632 |
| `*.vocab_size` | `vocab_size` | 32000 |
| `*.context_length` | `max_seq_len` | 2048 |
| `*.rope.freq_base` | `rope_theta` | 10000.0 |
| `general.name` | `name` | "TinyLlama" |

### 2. tokenizer（`Tokenizer::load_from_gguf`）

读取以下元数据条目：

| Key | 类型 | 描述 |
|-----|------|-------------|
| `tokenizer.ggml.tokens` | array of strings | 词表（id → token 文本） |
| `tokenizer.ggml.bos_token_id` | uint32 | 序列起始 token |
| `tokenizer.ggml.eos_token_id` | uint32 | 序列结束 token |

加载后构建 `token_to_id_` 哈希表，实现 O(1) 编码。

### 3. 张量信息（`parse_tensor_infos`）

对每个张量读取：
- `name` — 例如 "blk.5.attn_q.weight"
- `n_dims` — 1 或 2
- `shape[]` — 维度
- `type` — 量化类型（GGMLType 枚举）
- `offset` — 相对 data 段起始的字节偏移

### 4. 权重映射（`load_and_map_weights`）

把张量名映射到 `LayerWeights` 结构体指针：

| 张量名模式 | 结构体字段 |
|--------------------|-------------|
| `token_embd.weight` | `ModelWeights.tok_embd` |
| `output_norm.weight` | `ModelWeights.output_norm` |
| `output.weight` | `ModelWeights.output` |
| `blk.{N}.attn_q.weight` | `layers[N].wq` |
| `blk.{N}.attn_k.weight` | `layers[N].wk` |
| `blk.{N}.attn_v.weight` | `layers[N].wv` |
| `blk.{N}.attn_output.weight` | `layers[N].wo` |
| `blk.{N}.ffn_gate.weight` | `layers[N].w_gate` |
| `blk.{N}.ffn_up.weight` | `layers[N].w_up` |
| `blk.{N}.ffn_down.weight` | `layers[N].w_down` |
| `blk.{N}.attn_norm.weight` | `layers[N].rms_attn` |
| `blk.{N}.ffn_norm.weight` | `layers[N].rms_ffn` |

指针按如下方式计算：`(char*)mmap_blob + data_offset + tensor_offset`

## GGUF 值类型

KV 跳过逻辑必须正确处理全部 13 种 GGUF 类型：

| Type ID | 名称 | 大小 |
|---------|------|------|
| 0 | UINT8 | 1 |
| 1 | INT8 | 1 |
| 2 | UINT16 | 2 |
| 3 | INT16 | 2 |
| 4 | UINT32 | 4 |
| 5 | INT32 | 4 |
| 6 | FLOAT32 | 4 |
| 7 | BOOL | 1 |
| 8 | STRING | 8（长度）+ N（数据） |
| 9 | ARRAY | 4（元素类型）+ 8（数量）+ 元素 |
| 10 | UINT64 | 8 |
| 11 | INT64 | 8 |
| 12 | FLOAT64 | 8 |

## 量化类型

| 类型 | 块大小 | 字节/块 | 位/权重 | 质量 |
|------|-----------|-------------|-------------|---------|
| Q4_K_M | 256 | 144 | ~4.5 | 好（推荐） |
| Q5_K_M | 256 | 176 | ~5.5 | 很好 |
| Q6_K | 256 | 210 | ~6.5 | 优秀 |
| Q8_0 | 32 | 34 | ~8.5 | 接近 FP16 |
| Q4_0 | 32 | 18 | ~4.5 | 可接受 |
| Q3_K | 256 | 110 | ~3.4 | 退化 |
| Q2_K | 256 | 84 | ~2.6 | 较差 |
| F16 | 1 | 2 | 16 | 无损 |

## 调试张量名

若权重映射失败（0/N 个 layer 映射成功），检查 GGUF：

```bash
pip install gguf
python3 -c "
from gguf import GGUFReader
r = GGUFReader('model.gguf')
for t in r.tensors:
    print(f'{t.name:50s} {t.tensor_type.name:10s} {list(t.shape)}')
" | head -20
```

把输出的名称与 `load_and_map_weights()` 中的模式对比。


<details>
<summary>English original</summary>

**GGUF Format Parsing**

**GGUF File Structure**

```
┌──────────────────────────────┐
│ Header (24 bytes)            │
│   magic: 0x46475547 "GGUF"  │
│   version: uint32            │
│   n_tensors: uint64          │
│   n_kv: uint64               │
├──────────────────────────────┤
│ Metadata KV pairs (variable) │
│   For each KV:               │
│     key_len: uint64          │
│     key: string              │
│     value_type: uint32       │
│     value: (type-dependent)  │
├──────────────────────────────┤
│ Tensor info (variable)       │
│   For each tensor:           │
│     name_len: uint64         │
│     name: string             │
│     n_dims: uint32           │
│     shape[n_dims]: int64[]   │
│     type: uint32 (GGMLType)  │
│     offset: uint64           │
├──────────────────────────────┤
│ Alignment padding            │
├──────────────────────────────┤
│ Tensor data                  │
│   (raw weight bytes at       │
│    offsets specified above)   │
└──────────────────────────────┘
```

**What We Parse**

**1. Model Config (`load_gguf_config`)**

Reads metadata KV pairs matching these keys:

| GGUF Key | Maps to | Example |
|----------|---------|---------|
| `*.block_count` | `n_layers` | 22 |
| `*.head_count` | `n_heads` | 32 |
| `*.head_count_kv` | `n_kv_heads` | 4 |
| `*.embedding_length` | `hidden_dim` | 2048 |
| `*.feed_forward_length` | `intermediate_dim` | 5632 |
| `*.vocab_size` | `vocab_size` | 32000 |
| `*.context_length` | `max_seq_len` | 2048 |
| `*.rope.freq_base` | `rope_theta` | 10000.0 |
| `general.name` | `name` | "TinyLlama" |

**2. Tokenizer (`Tokenizer::load_from_gguf`)**

Reads these metadata entries:

| Key | Type | Description |
|-----|------|-------------|
| `tokenizer.ggml.tokens` | array of strings | Vocabulary (id → token text) |
| `tokenizer.ggml.bos_token_id` | uint32 | Beginning of sequence token |
| `tokenizer.ggml.eos_token_id` | uint32 | End of sequence token |

After loading, builds a `token_to_id_` hash map for O(1) encoding.

**3. Tensor Info (`parse_tensor_infos`)**

For each tensor, reads:
- `name` — e.g., "blk.5.attn_q.weight"
- `n_dims` — 1 or 2
- `shape[]` — dimensions
- `type` — quantization type (GGMLType enum)
- `offset` — byte offset from data section start

**4. Weight Mapping (`load_and_map_weights`)**

Maps tensor names to `LayerWeights` struct pointers:

| Tensor Name Pattern | Struct Field |
|--------------------|-------------|
| `token_embd.weight` | `ModelWeights.tok_embd` |
| `output_norm.weight` | `ModelWeights.output_norm` |
| `output.weight` | `ModelWeights.output` |
| `blk.{N}.attn_q.weight` | `layers[N].wq` |
| `blk.{N}.attn_k.weight` | `layers[N].wk` |
| `blk.{N}.attn_v.weight` | `layers[N].wv` |
| `blk.{N}.attn_output.weight` | `layers[N].wo` |
| `blk.{N}.ffn_gate.weight` | `layers[N].w_gate` |
| `blk.{N}.ffn_up.weight` | `layers[N].w_up` |
| `blk.{N}.ffn_down.weight` | `layers[N].w_down` |
| `blk.{N}.attn_norm.weight` | `layers[N].rms_attn` |
| `blk.{N}.ffn_norm.weight` | `layers[N].rms_ffn` |

Pointers are computed as: `(char*)mmap_blob + data_offset + tensor_offset`

**GGUF Value Types**

The KV skip logic must handle all 13 GGUF types correctly:

| Type ID | Name | Size |
|---------|------|------|
| 0 | UINT8 | 1 |
| 1 | INT8 | 1 |
| 2 | UINT16 | 2 |
| 3 | INT16 | 2 |
| 4 | UINT32 | 4 |
| 5 | INT32 | 4 |
| 6 | FLOAT32 | 4 |
| 7 | BOOL | 1 |
| 8 | STRING | 8 (length) + N (data) |
| 9 | ARRAY | 4 (elem type) + 8 (count) + elements |
| 10 | UINT64 | 8 |
| 11 | INT64 | 8 |
| 12 | FLOAT64 | 8 |

**Quantization Types**

| Type | Block size | Bytes/block | Bits/weight | Quality |
|------|-----------|-------------|-------------|---------|
| Q4_K_M | 256 | 144 | ~4.5 | Good (recommended) |
| Q5_K_M | 256 | 176 | ~5.5 | Very good |
| Q6_K | 256 | 210 | ~6.5 | Excellent |
| Q8_0 | 32 | 34 | ~8.5 | Near-FP16 |
| Q4_0 | 32 | 18 | ~4.5 | Acceptable |
| Q3_K | 256 | 110 | ~3.4 | Degraded |
| Q2_K | 256 | 84 | ~2.6 | Poor |
| F16 | 1 | 2 | 16 | Lossless |

**Debugging Tensor Names**

If weight mapping fails (0/N layers mapped), inspect the GGUF:

```bash
pip install gguf
python3 -c "
from gguf import GGUFReader
r = GGUFReader('model.gguf')
for t in r.tensors:
    print(f'{t.name:50s} {t.tensor_type.name:10s} {list(t.shape)}')
" | head -20
```

Compare output names with the patterns in `load_and_map_weights()`.

</details>

---

> 原文：[`Projects/jetson-llm-runtime/docs/gguf.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/docs/gguf.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
