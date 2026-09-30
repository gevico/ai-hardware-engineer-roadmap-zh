---
title: 第 05 讲 —— 物理 AI 与多模态 Gemma 4：SigLIP、VLM 流水线与 Jetson 上的机器人感知
description: 第 05 讲 —— 物理 AI 与多模态 Gemma 4：SigLIP、VLM 流水线与 Jetson 上的机器人感知
---

# 第 05 讲 —— 物理 AI 与多模态 Gemma 4：SigLIP、VLM 流水线与 Jetson 上的机器人感知

**合集：** [Gemma 4 边缘部署](README.md) | **上一篇：** [← 第 04 讲](Lecture-04.md)

---

前四讲把 Gemma 4 当作文本模型来讨论 —— 架构、量化、runtime、投机 decode（逐 token 生成阶段）。本讲收尾：接上摄像头、在机器人上部署多模态 Gemma 4 时会发生什么？Gemma 4 的 **视觉变体**（4B-IT-VL、12B-IT-VL、27B-IT-VL）使用 SigLIP-400M 视觉编码器生成视觉 token，这些 token 被前置到 Gemma 文本解码器中。合并后的模型是一个 Vision-Language Model（VLM），能够回答关于所见内容的空间问题、描述场景，并驱动智能体化的动作 —— 这些任务正是 **物理 AI** 在 Jetson 上的定义。

本讲涵盖 SigLIP-400M 编码器架构、VLM 流水线的延迟预算、Gemma 4-VL 在 Jetson Orin 上的部署、面向视觉技术栈的量化，以及真实的机器人感知用例。

---

## 学习目标

1. 描述 **SigLIP-400M 编码器** 架构，以及它如何为 Gemma 4 的解码器编码视觉 token。
2. 计算 Jetson Orin 上 Gemma 4 4B-VL 的 **端到端 VLM 延迟预算**（image encode → prefill（首字前的整段计算） → decode）。
3. 使用 Ollama（视觉支持）、llama.cpp mmproj 或 HuggingFace Transformers 在 Jetson 上部署 Gemma 4-VL。
4. 解释 **视觉编码器特有的量化挑战**，以及为什么要把视觉编码器与文本解码器当作独立的量化目标。
5. 设计一条 **机器人感知流水线**（camera → VLM → action），并为闭环控制设定可测量的首 token 时延目标。

---

## 1. SigLIP-400M —— Gemma 4 的视觉编码器

### 1.1 架构概览

SigLIP（Sigmoid Loss for Language-Image Pre-training，Zhai 等，2023）是 Google 在视觉-语言对齐方面对 CLIP 的接替者：

```text
CLIP vs SigLIP:
  CLIP objective:     softmax contrastive loss (global normalization across batch)
                      numerically stable but requires large batch sizes (>4096)
  SigLIP objective:   sigmoid binary loss per image-text pair (local, per-pair)
                      stable at smaller batches, scales better, stronger at retrieval

SigLIP-400M for Gemma 4:
  Architecture:  ViT-L/14 variant (Vision Transformer Large with 14×14 patches)
  Parameters:    ~400M (400 million)
  Input:         images resized to 224×224 (baseline) or 448×448 (high-res variant)
  Patch size:    14×14 pixels → 256 patches at 224px, 1024 patches at 448px
  Embedding dim: 1024
  Transformer:   24 layers, 16 attention heads, FFN dim 4096
  Output:        256 or 1024 visual tokens (each 1024-dim)
  Projection:    linear proj 1024 → Gemma 4 hidden_dim (e.g., 2048 for 4B)
```

### 1.2 图像 token 化流水线

```python
from transformers import AutoProcessor, PaliGemmaForConditionalGeneration
import PIL.Image

# Gemma 4-VL uses the PaliGemma/SigLIP processor for image tokenization:
processor = AutoProcessor.from_pretrained("google/gemma-4-4b-it")

# Load an image (e.g., from Jetson camera feed):
image = PIL.Image.open("/dev/video0")  # or file path

# Processor tokenizes both image and text:
inputs = processor(
    images=image,
    text="<image>\nDescribe what the robot arm is holding.",
    return_tensors="pt"
)

# inputs["pixel_values"]: (1, 3, 224, 224) — resized + normalized image
# inputs["input_ids"]:    (<img> token × 256) + text tokens

# SigLIP encodes pixel_values → 256 visual tokens of dim 1024
# Linear projection → Gemma 4 hidden_dim tokens
# These 256 tokens are PREPENDED to the text token sequence
# Gemma 4 decoder processes all (256 + text_tokens) as the prefill
```

### 1.3 视觉 token 数量与 prefill 开销

图像分辨率决定前置多少视觉 token：

```text
224×224 (standard):  256 visual tokens  → prefill adds 256 tokens
448×448 (high-res):  1024 visual tokens → prefill adds 1024 tokens

PREFILL COST on Orin (Gemma 4 4B INT4, 204 GB/s):
  Standard (256 tokens): prefill throughput ~500 tok/s (compute-bound)
                         latency: 256 / 500 = ~0.5 s prefill
                         (faster than text prefill because attention is local
                          for sliding-window layers)
  High-res (1024 tokens): latency: ~2.0 s prefill

  PLUS SigLIP encoding:  224×224 → ~80 ms on GPU (FP16 ViT-L/14)
                         448×448 → ~200 ms on GPU

Total TTFT (time-to-first-token):
  Standard:  80 ms (SigLIP) + 500 ms (prefill) + 16 ms (first decode) = ~600 ms
  High-res:  200 ms (SigLIP) + 2000 ms (prefill) + 16 ms = ~2.2 s
```

对于 **实时机器人感知**（目标 ≤ 500 ms 首 token 时延），必须使用标准的 224×224 分辨率。高分辨率适用于离线分析或对时间不那么敏感的任务。

---

## 2. Gemma 4-VL 在 Jetson 上的部署路径


<details>
<summary>English original</summary>

**Lecture 05 — Physical AI and Multimodal Gemma 4: SigLIP, VLM Pipeline, and Robot Perception on Jetson**

**Collection:** [Gemma 4 Edge Deployment](README.md) | **Previous:** [← Lecture 04](Lecture-04.md)

---

The previous four lectures covered Gemma 4 as a text model — architecture, quantization, runtimes, speculative decode. This lecture closes the loop: what happens when you attach a camera and deploy Gemma 4 multimodal on a robot? Gemma 4's **vision variants** (4B-IT-VL, 12B-IT-VL, 27B-IT-VL) use a SigLIP-400M vision encoder to produce visual tokens, which are prefixed into the Gemma text decoder. The combined model is a Vision-Language Model (VLM) capable of answering spatial questions about what it sees, describing scenes, and driving agentic actions — tasks that define **physical AI** on Jetson.

This lecture covers the SigLIP-400M encoder architecture, the VLM pipeline's latency budget, deployment of Gemma 4-VL on Jetson Orin, quantization for the vision stack, and real robot perception use cases.

---

**Learning objectives**

1. Describe the **SigLIP-400M encoder** architecture and how it encodes visual tokens for Gemma 4's decoder.
2. Compute the **end-to-end VLM latency budget** (image encode → prefill → decode) for Gemma 4 4B-VL on Jetson Orin.
3. Deploy Gemma 4-VL on Jetson using Ollama (vision support), llama.cpp mmproj, or HuggingFace Transformers.
4. Explain the **quantization challenges specific to vision encoders** and why you treat the vision encoder and text decoder as separate quantization targets.
5. Design a **robot perception pipeline** (camera → VLM → action) with a measured TTFT latency target for closed-loop control.

---

**1. SigLIP-400M — the vision encoder for Gemma 4**

**1.1 Architecture overview**

SigLIP (Sigmoid Loss for Language-Image Pre-training, Zhai et al., 2023) is Google's successor to CLIP for vision-language alignment:

```text
CLIP vs SigLIP:
  CLIP objective:     softmax contrastive loss (global normalization across batch)
                      numerically stable but requires large batch sizes (>4096)
  SigLIP objective:   sigmoid binary loss per image-text pair (local, per-pair)
                      stable at smaller batches, scales better, stronger at retrieval

SigLIP-400M for Gemma 4:
  Architecture:  ViT-L/14 variant (Vision Transformer Large with 14×14 patches)
  Parameters:    ~400M (400 million)
  Input:         images resized to 224×224 (baseline) or 448×448 (high-res variant)
  Patch size:    14×14 pixels → 256 patches at 224px, 1024 patches at 448px
  Embedding dim: 1024
  Transformer:   24 layers, 16 attention heads, FFN dim 4096
  Output:        256 or 1024 visual tokens (each 1024-dim)
  Projection:    linear proj 1024 → Gemma 4 hidden_dim (e.g., 2048 for 4B)
```

**1.2 Image tokenization pipeline**

```python
from transformers import AutoProcessor, PaliGemmaForConditionalGeneration
import PIL.Image

# Gemma 4-VL uses the PaliGemma/SigLIP processor for image tokenization:
processor = AutoProcessor.from_pretrained("google/gemma-4-4b-it")

# Load an image (e.g., from Jetson camera feed):
image = PIL.Image.open("/dev/video0")  # or file path

# Processor tokenizes both image and text:
inputs = processor(
    images=image,
    text="<image>\nDescribe what the robot arm is holding.",
    return_tensors="pt"
)

# inputs["pixel_values"]: (1, 3, 224, 224) — resized + normalized image
# inputs["input_ids"]:    (<img> token × 256) + text tokens

# SigLIP encodes pixel_values → 256 visual tokens of dim 1024
# Linear projection → Gemma 4 hidden_dim tokens
# These 256 tokens are PREPENDED to the text token sequence
# Gemma 4 decoder processes all (256 + text_tokens) as the prefill
```

**1.3 Visual token count and prefill cost**

The image resolution determines how many visual tokens are prepended:

```text
224×224 (standard):  256 visual tokens  → prefill adds 256 tokens
448×448 (high-res):  1024 visual tokens → prefill adds 1024 tokens

PREFILL COST on Orin (Gemma 4 4B INT4, 204 GB/s):
  Standard (256 tokens): prefill throughput ~500 tok/s (compute-bound)
                         latency: 256 / 500 = ~0.5 s prefill
                         (faster than text prefill because attention is local
                          for sliding-window layers)
  High-res (1024 tokens): latency: ~2.0 s prefill

  PLUS SigLIP encoding:  224×224 → ~80 ms on GPU (FP16 ViT-L/14)
                         448×448 → ~200 ms on GPU

Total TTFT (time-to-first-token):
  Standard:  80 ms (SigLIP) + 500 ms (prefill) + 16 ms (first decode) = ~600 ms
  High-res:  200 ms (SigLIP) + 2000 ms (prefill) + 16 ms = ~2.2 s
```

For **real-time robot perception** (target ≤ 500 ms TTFT), you must use standard 224×224 resolution. High-res is for offline analysis or less time-sensitive tasks.

---

**2. Deployment paths for Gemma 4-VL on Jetson**

</details>

### 2.1 Ollama（推荐用于快速 bring-up，即上电点亮/调通）

Ollama v0.4+ 原生支持 Gemma 4 视觉模型：

```bash
# Pull and run Gemma 4 4B vision model:
ollama pull gemma4:4b-instruct-vision-q4_K_M  # ~2.2 GB

# Interactive vision query:
ollama run gemma4:4b-instruct-vision-q4_K_M

# Programmatic with image:
curl http://localhost:11434/api/generate -d '{
  "model": "gemma4:4b-instruct-vision-q4_K_M",
  "prompt": "What objects are on the table?",
  "images": ["'$(base64 -w0 /tmp/camera_frame.jpg)'"],
  "stream": false
}'

# Expected throughput on Orin (INT4, SigLIP 224px):
# encode:  ~80 ms
# prefill: ~500 ms
# decode:  ~55 tok/s
# TTFT:    ~600 ms for a typical scene description prompt
```

### 2.2 带 mmproj 的 llama.cpp（多模态投影）

llama.cpp 通过其 mmproj（多模态投影）系统处理 Gemma 4-VL：

```bash
# You need TWO files:
# 1. The main GGUF (text model weights):
#    gemma4-4b-it-q4_k_m.gguf
# 2. The vision projection GGUF (SigLIP + projection):
#    mmproj-gemma4-4b-f16.gguf  (keep FP16 — quantizing mmproj loses accuracy)

# Interactive VLM CLI:
./build/bin/llama-cli \
    --model      gemma4-4b-it-q4_k_m.gguf \
    --mmproj     mmproj-gemma4-4b-f16.gguf \
    --n-gpu-layers 9999 \
    --image      /tmp/robot_view.jpg \
    -p "Describe what the gripper is touching."

# Server mode for API access:
./build/bin/llama-server \
    --model      gemma4-4b-it-q4_k_m.gguf \
    --mmproj     mmproj-gemma4-4b-f16.gguf \
    --n-gpu-layers 9999 \
    --port 8080 \
    --ctx-size 4096

# curl the server with an image:
curl http://localhost:8080/v1/chat/completions \
    -H "Content-Type: application/json" \
    -d '{
      "model": "gemma4-4b",
      "messages": [{
        "role": "user",
        "content": [
          {"type": "image_url", "image_url": {"url": "data:image/jpeg;base64,'$(base64 -w0 /tmp/frame.jpg)'"}},
          {"type": "text", "text": "What object is in front of the robot?"}
        ]
      }]
    }'
```

### 2.3 HuggingFace Transformers（全精度，适合做原型验证）

```python
from transformers import AutoProcessor, Gemma3ForConditionalGeneration
import torch, PIL.Image

# Gemma 4 VL models use the Gemma3 family in HF:
model_id = "google/gemma-4-4b-it"

processor = AutoProcessor.from_pretrained(model_id)
model = Gemma3ForConditionalGeneration.from_pretrained(
    model_id,
    torch_dtype=torch.bfloat16,
    device_map="cuda"
)

image = PIL.Image.open("/tmp/jetson_camera.jpg").convert("RGB")

messages = [{
    "role": "user",
    "content": [
        {"type": "image", "image": image},
        {"type": "text", "text": "Is the robot arm's gripper open or closed?"},
    ]
}]

inputs = processor.apply_chat_template(
    messages, add_generation_prompt=True, tokenize=True,
    return_dict=True, return_tensors="pt"
).to(model.device)

with torch.inference_mode():
    output = model.generate(**inputs, max_new_tokens=200, do_sample=False)

print(processor.decode(output[0][inputs["input_ids"].shape[1]:], skip_special_tokens=True))
```

---

## 3. 量化视觉栈

Gemma 4-VL 部署有两个敏感度不同的量化目标：

### 3.1 文本 decoder（与第 02 讲规则相同）

```text
Gemma 4 text decoder: Q4_K_M GGUF, AWQ INT4, or GPTQ INT4
  Sensitivity: moderate — QK-norm + GQA make it robust to INT4
  Target: 2.2 GB for 4B (vs 8.2 GB BF16)
  Accuracy loss: ≤0.3 MMLU points at Q4_K_M (see Lecture 02)
```

### 3.2 SigLIP 视觉编码器——保持 FP16

视觉编码器对量化比文本 decoder 更敏感：

```text
WHY FP16 FOR SIGLIP:
  1. The encoder produces 256 visual tokens that REPLACE the image for the decoder.
     Quantization errors in the visual tokens compound across all 256 token positions.
  2. Image patches are continuous-valued — no discrete tokenization to absorb errors.
     Unlike text (limited vocab = natural quantization), pixels vary continuously.
  3. SigLIP uses sigmoid loss training — the feature space is calibrated for FP32/FP16
     precision; INT8 introduces systematic bias that degrades spatial reasoning.
  4. The mmproj (projection layer) is tiny (~80 MB FP16 for 4B).
     Saving to INT8 saves ~40 MB — not worth the accuracy degradation.

RULE: Always keep the mmproj/SigLIP encoder in FP16.
      Only quantize the text decoder (the large component).

EXCEPTION: INT8 for SigLIP is acceptable if you measure VQA accuracy drop < 1%
           on your specific task. Use llm-compressor or quanto for dynamic INT8.
```


<details>
<summary>English original</summary>

**2.1 Ollama (recommended for fast bring-up)**

Ollama v0.4+ supports Gemma 4 vision models natively:

```bash
# Pull and run Gemma 4 4B vision model:
ollama pull gemma4:4b-instruct-vision-q4_K_M  # ~2.2 GB

# Interactive vision query:
ollama run gemma4:4b-instruct-vision-q4_K_M

# Programmatic with image:
curl http://localhost:11434/api/generate -d '{
  "model": "gemma4:4b-instruct-vision-q4_K_M",
  "prompt": "What objects are on the table?",
  "images": ["'$(base64 -w0 /tmp/camera_frame.jpg)'"],
  "stream": false
}'

# Expected throughput on Orin (INT4, SigLIP 224px):
# encode:  ~80 ms
# prefill: ~500 ms
# decode:  ~55 tok/s
# TTFT:    ~600 ms for a typical scene description prompt
```

**2.2 llama.cpp with mmproj (multimodal projection)**

llama.cpp handles Gemma 4-VL via its mmproj (multimodal projection) system:

```bash
# You need TWO files:
# 1. The main GGUF (text model weights):
#    gemma4-4b-it-q4_k_m.gguf
# 2. The vision projection GGUF (SigLIP + projection):
#    mmproj-gemma4-4b-f16.gguf  (keep FP16 — quantizing mmproj loses accuracy)

# Interactive VLM CLI:
./build/bin/llama-cli \
    --model      gemma4-4b-it-q4_k_m.gguf \
    --mmproj     mmproj-gemma4-4b-f16.gguf \
    --n-gpu-layers 9999 \
    --image      /tmp/robot_view.jpg \
    -p "Describe what the gripper is touching."

# Server mode for API access:
./build/bin/llama-server \
    --model      gemma4-4b-it-q4_k_m.gguf \
    --mmproj     mmproj-gemma4-4b-f16.gguf \
    --n-gpu-layers 9999 \
    --port 8080 \
    --ctx-size 4096

# curl the server with an image:
curl http://localhost:8080/v1/chat/completions \
    -H "Content-Type: application/json" \
    -d '{
      "model": "gemma4-4b",
      "messages": [{
        "role": "user",
        "content": [
          {"type": "image_url", "image_url": {"url": "data:image/jpeg;base64,'$(base64 -w0 /tmp/frame.jpg)'"}},
          {"type": "text", "text": "What object is in front of the robot?"}
        ]
      }]
    }'
```

**2.3 HuggingFace Transformers (full precision, good for prototyping)**

```python
from transformers import AutoProcessor, Gemma3ForConditionalGeneration
import torch, PIL.Image

# Gemma 4 VL models use the Gemma3 family in HF:
model_id = "google/gemma-4-4b-it"

processor = AutoProcessor.from_pretrained(model_id)
model = Gemma3ForConditionalGeneration.from_pretrained(
    model_id,
    torch_dtype=torch.bfloat16,
    device_map="cuda"
)

image = PIL.Image.open("/tmp/jetson_camera.jpg").convert("RGB")

messages = [{
    "role": "user",
    "content": [
        {"type": "image", "image": image},
        {"type": "text", "text": "Is the robot arm's gripper open or closed?"},
    ]
}]

inputs = processor.apply_chat_template(
    messages, add_generation_prompt=True, tokenize=True,
    return_dict=True, return_tensors="pt"
).to(model.device)

with torch.inference_mode():
    output = model.generate(**inputs, max_new_tokens=200, do_sample=False)

print(processor.decode(output[0][inputs["input_ids"].shape[1]:], skip_special_tokens=True))
```

---

**3. Quantizing the vision stack**

The Gemma 4-VL deployment has two quantization targets with different sensitivities:

**3.1 Text decoder (same rules as Lecture 02)**

```text
Gemma 4 text decoder: Q4_K_M GGUF, AWQ INT4, or GPTQ INT4
  Sensitivity: moderate — QK-norm + GQA make it robust to INT4
  Target: 2.2 GB for 4B (vs 8.2 GB BF16)
  Accuracy loss: ≤0.3 MMLU points at Q4_K_M (see Lecture 02)
```

**3.2 SigLIP vision encoder — keep at FP16**

The vision encoder is more sensitive to quantization than the text decoder:

```text
WHY FP16 FOR SIGLIP:
  1. The encoder produces 256 visual tokens that REPLACE the image for the decoder.
     Quantization errors in the visual tokens compound across all 256 token positions.
  2. Image patches are continuous-valued — no discrete tokenization to absorb errors.
     Unlike text (limited vocab = natural quantization), pixels vary continuously.
  3. SigLIP uses sigmoid loss training — the feature space is calibrated for FP32/FP16
     precision; INT8 introduces systematic bias that degrades spatial reasoning.
  4. The mmproj (projection layer) is tiny (~80 MB FP16 for 4B).
     Saving to INT8 saves ~40 MB — not worth the accuracy degradation.

RULE: Always keep the mmproj/SigLIP encoder in FP16.
      Only quantize the text decoder (the large component).

EXCEPTION: INT8 for SigLIP is acceptable if you measure VQA accuracy drop < 1%
           on your specific task. Use llm-compressor or quanto for dynamic INT8.
```

</details>

### 3.3 Orin 上 4B-VL 的内存布局

```text
Component                   FP16 Size   INT4 Size   Notes
─────────────────────────── ─────────── ─────────── ───────────────────────────
SigLIP-400M encoder         800 MB      N/A         Keep FP16
mmproj projection           80 MB       N/A         Keep FP16
Gemma 4 4B text decoder     8,200 MB    2,200 MB    Quantize to INT4
KV cache (4096 ctx, FP16)   ~200 MB     ~100 MB     Use INT8 KV (–kv-cache-type q8_0)
─────────────────────────── ─────────── ─────────── ───────────────────────────
TOTAL (text INT4 + vis FP16) 1,280 MB   3,480 MB    ← fits on Orin 64 GB
TOTAL (text BF16 + vis FP16) 9,280 MB                ← still fits, slower
```

---

## 4. Jetson 上的机器人感知流水线

### 4.1 Gemma 4-VL 的 Physical AI 用例

Gemma 4 融合了 128K 上下文、有界 KV 与多模态能力，是首个适用于以下场景的边缘 VLM：

**物体识别与空间推理：**

```text
"What objects are within reach of the gripper?" → closed-loop pick-and-place
"Is the bin empty?" → warehouse inventory
"Which cable is the red one?" → assembly robot guidance
```

**面向自主导航的场景描述：**

```text
"Describe the obstacles in front of the robot." → path planning assist
"Is the path to the dock clear?" → docking AI
"What is the surface condition of the floor?" → traction-aware navigation
```

**人机交互：**

```text
"What is the person pointing at?" → instruction following
"Is the person wearing safety equipment?" → compliance monitoring
"What task is the human performing?" → collaborative manipulation
```

### 4.2 端到端延迟预算

对于以 2 Hz 运行的机器人（每 500 ms 做一次动作决策）：

```text
BUDGET: 500 ms total
  Image capture:     10 ms   (Jetson MIPI CSI pipeline, 1080p30)
  Image resize:      5 ms    (cudaResize to 224×224)
  SigLIP encode:     80 ms   (FP16, GPU)
  Prefill:           400 ms  (256 visual tokens + 50 text tokens = 306 tokens)
                             (Gemma 4 4B INT4, ~750 tok/s prefill on Orin)
  First decode:      18 ms   (1 token at ~55 tok/s)
  TOTAL TTFT:        513 ms  ← slightly over budget

OPTIMIZATION TO FIT 500 ms:
  1. Use INT8 KV cache: saves KV memory, more GPU cache available → prefill faster
  2. Reduce prompt length: 50 → 20 text tokens → saves ~30 ms
  3. Pipeline: start prefill while previous decode is completing (temporal overlap)
  4. Result: 80 + 330 + 18 = 428 ms ← fits in 500 ms budget with headroom
```

### 4.3 闭环感知的 Python 流水线

```python
import threading, queue, time
import torch, PIL.Image
from transformers import AutoProcessor, Gemma3ForConditionalGeneration

# Load model once at startup:
model_id = "google/gemma-4-4b-it"
processor = AutoProcessor.from_pretrained(model_id)
model = Gemma3ForConditionalGeneration.from_pretrained(
    model_id, torch_dtype=torch.bfloat16, device_map="cuda"
)

frame_queue = queue.Queue(maxsize=2)
result_queue = queue.Queue()

def perception_worker():
    while True:
        frame = frame_queue.get()  # (PIL.Image, timestamp)
        if frame is None: break
        image, t_capture = frame

        t0 = time.perf_counter()
        messages = [{
            "role": "user",
            "content": [
                {"type": "image", "image": image},
                {"type": "text", "text": "What object is nearest to the gripper?"},
            ]
        }]
        inputs = processor.apply_chat_template(
            messages, add_generation_prompt=True, tokenize=True,
            return_dict=True, return_tensors="pt"
        ).to(model.device)

        with torch.inference_mode():
            output = model.generate(
                **inputs, max_new_tokens=50, do_sample=False
            )

        answer = processor.decode(
            output[0][inputs["input_ids"].shape[1]:], skip_special_tokens=True
        )
        latency_ms = (time.perf_counter() - t0) * 1000
        result_queue.put({"answer": answer, "latency_ms": latency_ms, "t_capture": t_capture})

# Start worker:
t = threading.Thread(target=perception_worker, daemon=True)
t.start()

# Main loop (simulated camera at 10 Hz, VLM at 2 Hz):
for frame_num in range(100):
    if frame_num % 5 == 0:  # every 5th frame → 2 Hz VLM rate
        img = PIL.Image.open(f"/tmp/frame_{frame_num:04d}.jpg").convert("RGB")
        if not frame_queue.full():
            frame_queue.put((img, time.perf_counter()))

    if not result_queue.empty():
        result = result_queue.get()
        print(f"Perception: '{result['answer']}' | latency: {result['latency_ms']:.0f} ms")

    time.sleep(0.1)  # 10 Hz main loop

frame_queue.put(None)  # shutdown
t.join()
```

### 4.4 Jetson 摄像头集成（GStreamer + CUDA）

```python
import cv2, PIL.Image
import numpy as np

# GStreamer pipeline for Jetson CSI camera (tested on Orin):
gst_pipeline = (
    "nvarguscamerasrc sensor-id=0 ! "
    "video/x-raw(memory:NVMM), width=1920, height=1080, framerate=30/1 ! "
    "nvvidconv flip-method=0 ! "
    "video/x-raw, width=224, height=224, format=BGRx ! "  # resize in hardware
    "videoconvert ! "
    "video/x-raw, format=BGR ! "
    "appsink"
)

cap = cv2.VideoCapture(gst_pipeline, cv2.CAP_GSTREAMER)
if not cap.isOpened():
    raise RuntimeError("Failed to open CSI camera with GStreamer pipeline.")

def get_frame_for_vlm():
    ret, frame = cap.read()
    if not ret: return None
    # Convert BGR numpy → RGB PIL:
    frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    return PIL.Image.fromarray(frame_rgb)  # already 224×224 from GStreamer

# SigLIP processes PIL images — GStreamer handles resize in hardware (free)
```

GStreamer 流水线在 ISP（图像信号处理器）/视频转换硬件中把 1080p 缩放到 224×224，从而把约 5 ms 的软件缩放从延迟预算中移除。

---

## 5. 使用 LiteRT 的多模态量化

对于需要 Google AI Edge 认证或 DLA（深度学习加速器）加速的生产部署，LiteRT 导出同时包含文本与视觉组件：

```python
import ai_edge_torch
from transformers import Gemma3ForConditionalGeneration, AutoProcessor
import torch

model_id = "google/gemma-4-4b-it"
model = Gemma3ForConditionalGeneration.from_pretrained(
    model_id, torch_dtype=torch.bfloat16
)
model.eval()

processor = AutoProcessor.from_pretrained(model_id)

# Sample inputs for tracing:
sample_image = torch.zeros(1, 3, 224, 224, dtype=torch.bfloat16)  # dummy image
sample_text = processor.tokenizer("Hello", return_tensors="pt")["input_ids"]

# Export SEPARATE models for vision and text components:
# (LiteRT handles them in two graphs, stitched at the projection layer)

# Vision encoder export:
vision_encoder = model.model.vision_tower  # SigLIP ViT-L
edge_vision = ai_edge_torch.convert(
    vision_encoder.eval(),
    sample_args=(sample_image,),
)
edge_vision.export("gemma4-4b-siglip-fp16.tflite")

# Text decoder export (with int8 quantization):
text_decoder = model.language_model
edge_text = ai_edge_torch.convert(
    text_decoder.eval(),
    sample_args=(sample_text,),
    quant_config=ai_edge_torch.quantize.QuantizationConfig(
        ai_edge_torch.quantize.IntQuantizationConfig(num_bits=8)
    )
)
edge_text.export("gemma4-4b-text-int8.tflite")
```

**Thor 上 SigLIP 的 DLA 加速：**

Jetson AGX Thor 的 DLA（深度学习加速器）能以比独立 GPU 更低的功耗运行 SigLIP ViT-L/14 编码器：

```text
SigLIP on Orin iGPU (FP16): ~80 ms, ~4W
SigLIP on Orin DLA (INT8):  ~120 ms, ~0.8W  ← 5× power reduction, 50% slower
SigLIP on Thor GPU (FP16):  ~40 ms, ~3W
SigLIP on Thor DLA (INT8):  ~60 ms, ~0.4W   ← 7.5× power reduction, 50% slower

RULE: Use DLA for SigLIP when power budget is critical (mobile robot, drone).
      Use GPU for SigLIP when latency budget is critical (manipulation robot).
```

---

## 6. 性能对比：Gemma 4-VL 与其他边缘 VLM

| 模型 | 参数量 | runtime | Orin 首 token 时延 | Orin tok/s | 4K 上下文 KV | 备注 |
|-------|-----------|---------|-----------|-----------|-------------|-------|
| Gemma 4 4B-VL INT4 | 4B | llama.cpp | ~600 ms | 55 | 200 MB | 吞吐/质量最佳 |
| Gemma 4 1B-VL INT4 | 1B | llama.cpp | ~150 ms | 180 | 50 MB | 最快，质量较低 |
| Llama 3.2 11B-VL INT4 | 11B | ollama | ~900 ms | 25 | 400 MB | 推理能力更强 |
| Qwen2.5-VL 7B INT4 | 7B | llama.cpp | ~750 ms | 38 | 300 MB | OCR 出色 |
| Phi-4 14B-VL INT4 | 14B | TRT-LLM | ~1200 ms | 18 | 550 MB | 需要 Thor 或外加 GPU |
| Gemma 4 12B-VL INT4 | 12B | llama.cpp | ~1100 ms | 22 | 450 MB | Thor 上表现最佳 |

**Gemma 4 4B-VL 是 Orin 的推荐目标**。它达到 600 ms 的首 token 时延（对 1 Hz 控制可接受）、用于详细描述的 55 tok/s decode（逐 token 生成阶段）吞吐，以及 4K 上下文下 200 MB 的 KV——为机器人软件栈的其余部分留出充足空间。

---

## 7. Capstone：端到端机器人感知系统

### Capstone 任务

构建一个感知模块，要求：
1. 接收来自 Jetson CSI 摄像头的相机帧
2. 运行 Gemma 4 4B-VL 回答“机器人路径上是否有障碍物？”
3. 返回结构化的 JSON 答案：`{"obstacle": true/false, "description": "...", "confidence": 0-1, "latency_ms": ...}`
4. 以 ≥ 1 Hz 运行（首 token 时延 ≤ 1000 ms）


<details>
<summary>English original</summary>

**4.4 Jetson camera integration (GStreamer + CUDA)**

```python
import cv2, PIL.Image
import numpy as np

# GStreamer pipeline for Jetson CSI camera (tested on Orin):
gst_pipeline = (
    "nvarguscamerasrc sensor-id=0 ! "
    "video/x-raw(memory:NVMM), width=1920, height=1080, framerate=30/1 ! "
    "nvvidconv flip-method=0 ! "
    "video/x-raw, width=224, height=224, format=BGRx ! "  # resize in hardware
    "videoconvert ! "
    "video/x-raw, format=BGR ! "
    "appsink"
)

cap = cv2.VideoCapture(gst_pipeline, cv2.CAP_GSTREAMER)
if not cap.isOpened():
    raise RuntimeError("Failed to open CSI camera with GStreamer pipeline.")

def get_frame_for_vlm():
    ret, frame = cap.read()
    if not ret: return None
    # Convert BGR numpy → RGB PIL:
    frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    return PIL.Image.fromarray(frame_rgb)  # already 224×224 from GStreamer

# SigLIP processes PIL images — GStreamer handles resize in hardware (free)
```

The GStreamer pipeline resizes from 1080p to 224×224 in the ISP/video converter hardware, eliminating the ~5 ms software resize from the latency budget.

---

**5. Multimodal quantization with LiteRT**

For production deployments that require Google AI Edge certification or DLA acceleration, LiteRT exports include both the text and vision components:

```python
import ai_edge_torch
from transformers import Gemma3ForConditionalGeneration, AutoProcessor
import torch

model_id = "google/gemma-4-4b-it"
model = Gemma3ForConditionalGeneration.from_pretrained(
    model_id, torch_dtype=torch.bfloat16
)
model.eval()

processor = AutoProcessor.from_pretrained(model_id)

# Sample inputs for tracing:
sample_image = torch.zeros(1, 3, 224, 224, dtype=torch.bfloat16)  # dummy image
sample_text = processor.tokenizer("Hello", return_tensors="pt")["input_ids"]

# Export SEPARATE models for vision and text components:
# (LiteRT handles them in two graphs, stitched at the projection layer)

# Vision encoder export:
vision_encoder = model.model.vision_tower  # SigLIP ViT-L
edge_vision = ai_edge_torch.convert(
    vision_encoder.eval(),
    sample_args=(sample_image,),
)
edge_vision.export("gemma4-4b-siglip-fp16.tflite")

# Text decoder export (with int8 quantization):
text_decoder = model.language_model
edge_text = ai_edge_torch.convert(
    text_decoder.eval(),
    sample_args=(sample_text,),
    quant_config=ai_edge_torch.quantize.QuantizationConfig(
        ai_edge_torch.quantize.IntQuantizationConfig(num_bits=8)
    )
)
edge_text.export("gemma4-4b-text-int8.tflite")
```

**DLA acceleration for SigLIP on Thor:**

Jetson AGX Thor's DLA (Deep Learning Accelerator) can run the SigLIP ViT-L/14 encoder at lower power than the discrete GPU:

```text
SigLIP on Orin iGPU (FP16): ~80 ms, ~4W
SigLIP on Orin DLA (INT8):  ~120 ms, ~0.8W  ← 5× power reduction, 50% slower
SigLIP on Thor GPU (FP16):  ~40 ms, ~3W
SigLIP on Thor DLA (INT8):  ~60 ms, ~0.4W   ← 7.5× power reduction, 50% slower

RULE: Use DLA for SigLIP when power budget is critical (mobile robot, drone).
      Use GPU for SigLIP when latency budget is critical (manipulation robot).
```

---

**6. Performance comparison: Gemma 4-VL vs other edge VLMs**

| Model | Parameters | Runtime | Orin TTFT | Orin tok/s | KV at 4K ctx | Notes |
|-------|-----------|---------|-----------|-----------|-------------|-------|
| Gemma 4 4B-VL INT4 | 4B | llama.cpp | ~600 ms | 55 | 200 MB | Best throughput/quality |
| Gemma 4 1B-VL INT4 | 1B | llama.cpp | ~150 ms | 180 | 50 MB | Fastest, lower quality |
| Llama 3.2 11B-VL INT4 | 11B | ollama | ~900 ms | 25 | 400 MB | Stronger reasoning |
| Qwen2.5-VL 7B INT4 | 7B | llama.cpp | ~750 ms | 38 | 300 MB | Excellent OCR |
| Phi-4 14B-VL INT4 | 14B | TRT-LLM | ~1200 ms | 18 | 550 MB | Needs Thor or +GPU |
| Gemma 4 12B-VL INT4 | 12B | llama.cpp | ~1100 ms | 22 | 450 MB | Best on Thor |

**Gemma 4 4B-VL is the recommended target for Orin**. It hits 600 ms TTFT (acceptable for 1 Hz control), 55 tok/s decode for verbose descriptions, and 200 MB KV at 4K context — leaving ample space for the rest of the robot software stack.

---

**7. Capstone: end-to-end robot perception system**

**Capstone task**

Build a perception module that:
1. Accepts a camera frame from a Jetson CSI camera
2. Runs Gemma 4 4B-VL to answer "Is there an obstacle in the robot's path?"
3. Returns a structured JSON answer: `{"obstacle": true/false, "description": "...", "confidence": 0-1, "latency_ms": ...}`
4. Operates at ≥ 1 Hz (TTFT ≤ 1000 ms)

</details>

### 参考实现

```python
import json, time, re
import torch, PIL.Image
from transformers import AutoProcessor, Gemma3ForConditionalGeneration

SYSTEM_PROMPT = """You are a robot perception module.
Answer ONLY with a JSON object: {"obstacle": true/false, "description": "...", "confidence": 0.0-1.0}
Keep description under 20 words. Be conservative: if unclear, obstacle=true."""

def build_perception_model(model_id="google/gemma-4-4b-it"):
    proc = AutoProcessor.from_pretrained(model_id)
    model = Gemma3ForConditionalGeneration.from_pretrained(
        model_id, torch_dtype=torch.bfloat16, device_map="cuda"
    )
    model.eval()
    return proc, model

def perceive(image: PIL.Image.Image, proc, model) -> dict:
    t0 = time.perf_counter()

    messages = [
        {"role": "system", "content": SYSTEM_PROMPT},
        {"role": "user", "content": [
            {"type": "image", "image": image},
            {"type": "text", "text": "Is there an obstacle in the robot's path?"}
        ]}
    ]

    inputs = proc.apply_chat_template(
        messages, add_generation_prompt=True, tokenize=True,
        return_dict=True, return_tensors="pt"
    ).to(model.device)

    with torch.inference_mode():
        out = model.generate(**inputs, max_new_tokens=80, do_sample=False)

    raw = proc.decode(out[0][inputs["input_ids"].shape[1]:], skip_special_tokens=True)

    # Extract JSON from model output:
    match = re.search(r'\{.*?\}', raw, re.DOTALL)
    if match:
        result = json.loads(match.group())
    else:
        result = {"obstacle": True, "description": raw[:100], "confidence": 0.5}

    result["latency_ms"] = (time.perf_counter() - t0) * 1000
    return result

# Usage:
proc, model = build_perception_model()
image = PIL.Image.open("/tmp/robot_view.jpg").convert("RGB")
result = perceive(image, proc, model)
print(result)
# → {"obstacle": false, "description": "Clear path, table and boxes visible", "confidence": 0.9, "latency_ms": 582}
```

### 综合项目验证检查清单

- [ ] 在 Orin 上端到端（capture → JSON 响应）测得的 TTFT ≤ 1000 ms
- [ ] 对含障碍物的图像，Obstacle=true 的比例 ≥ 90%（测试 20 个样本）
- [ ] 假阳性率（畅通路径上 obstacle=true）≤ 10%（测试 20 个样本）
- [ ] 连续运行 5 分钟无内存增长（检查 `tegrastats`）
- [ ] KV cache 内存保持有界（设置 `--ctx-size 4096` 加以限制）
- [ ] 流水线对每个响应都产生有效 JSON（测试 50 张不同图像）

---

## 要点总结

- **SigLIP-400M** 将图像编码为 256 个视觉 token（224×224）或 1024 个 token（448×448），并作为标准输入 token 前置到文本上下文 —— 无需特殊的 attention 掩码。
- **Orin 上 Gemma 4 4B-VL 的 TTFT** 约为 600 ms（80 ms SigLIP + 500 ms prefill（首字前的整段计算）+ 20 ms decode（逐 token 生成阶段）），可纳入 1 Hz 的机器人控制回路。
- **视觉编码器始终保持 FP16**；仅将文本解码器量化到 INT4。SigLIP 权重（FP16 下 880 MB）使部署占用增加约 1 GB，但其规模不足以抵偿 INT4 造成的准确率损失。
- **带 --mmproj 的 llama.cpp** 是获得可用 VLM 部署的最快路径；Ollama 对其做封装以便使用。HuggingFace Transformers 仅用于原型验证（BF16 下仅文本解码器即为 8.2 GB）。
- 在 Jetson Thor 上**用 DLA 跑 SigLIP** 可将编码器功耗降低 7.5×，代价是延迟增加 50% —— 适用于功耗主导的移动或无人机部署。
- Jetson 上的 **GStreamer 硬件流水线**在 ISP/video converter 中缩放帧，消除了软件缩放开销，并将 224×224 帧直接交付给 CUDA。
- **关键的延迟调节项**不是 decode 速度而是 **prefill** —— 256 个视觉 token 主导 TTFT。缩短文本 prompt 长度以节省文本侧的 prefill 时间。

---

## 课程完成检查清单

当你能够做到以下各项时，即已完成 Gemma 4 边缘部署课程：

- [ ] 解释 Gemma 4 交错 attention 的 KV-cache 计算方式，并对任意（模型、批大小、上下文）三元组算出精确占用。
- [ ] 从 Gemma 4 BF16 检查点校准并生成 Q4_K_M GGUF，MMLU 退化 ≤ 0.3。
- [ ] 用 llama.cpp 在 Jetson Orin 上部署 Gemma 4 4B（纯文本），测得 ≥ 45 tok/s。
- [ ] 以 Gemma 4 1B 作为 draft 配置投机解码，测量接受长度 τ，并验证加速比与 τ / (1 + K×c) 公式相符。
- [ ] 在 Jetson Orin 上部署 Gemma 4 4B-VL，测量端到端 TTFT，并验证其满足 ≤ 1 Hz 感知回路要求。
- [ ] 完成综合项目的感知模块，通过检查清单，并在笔记中报告 latency_ms。

---


<details>
<summary>English original</summary>

**Reference implementation**

```python
import json, time, re
import torch, PIL.Image
from transformers import AutoProcessor, Gemma3ForConditionalGeneration

SYSTEM_PROMPT = """You are a robot perception module.
Answer ONLY with a JSON object: {"obstacle": true/false, "description": "...", "confidence": 0.0-1.0}
Keep description under 20 words. Be conservative: if unclear, obstacle=true."""

def build_perception_model(model_id="google/gemma-4-4b-it"):
    proc = AutoProcessor.from_pretrained(model_id)
    model = Gemma3ForConditionalGeneration.from_pretrained(
        model_id, torch_dtype=torch.bfloat16, device_map="cuda"
    )
    model.eval()
    return proc, model

def perceive(image: PIL.Image.Image, proc, model) -> dict:
    t0 = time.perf_counter()

    messages = [
        {"role": "system", "content": SYSTEM_PROMPT},
        {"role": "user", "content": [
            {"type": "image", "image": image},
            {"type": "text", "text": "Is there an obstacle in the robot's path?"}
        ]}
    ]

    inputs = proc.apply_chat_template(
        messages, add_generation_prompt=True, tokenize=True,
        return_dict=True, return_tensors="pt"
    ).to(model.device)

    with torch.inference_mode():
        out = model.generate(**inputs, max_new_tokens=80, do_sample=False)

    raw = proc.decode(out[0][inputs["input_ids"].shape[1]:], skip_special_tokens=True)

    # Extract JSON from model output:
    match = re.search(r'\{.*?\}', raw, re.DOTALL)
    if match:
        result = json.loads(match.group())
    else:
        result = {"obstacle": True, "description": raw[:100], "confidence": 0.5}

    result["latency_ms"] = (time.perf_counter() - t0) * 1000
    return result

# Usage:
proc, model = build_perception_model()
image = PIL.Image.open("/tmp/robot_view.jpg").convert("RGB")
result = perceive(image, proc, model)
print(result)
# → {"obstacle": false, "description": "Clear path, table and boxes visible", "confidence": 0.9, "latency_ms": 582}
```

**Capstone verification checklist**

- [ ] TTFT ≤ 1000 ms measured end-to-end (capture → JSON response) on Orin
- [ ] Obstacle=true rate for images with blockers ≥ 90% (test 20 samples)
- [ ] False positive rate (obstacle=true on clear path) ≤ 10% (test 20 samples)
- [ ] Continuous operation for 5 minutes without memory growth (check `tegrastats`)
- [ ] KV cache memory stays bounded (set `--ctx-size 4096` to cap it)
- [ ] Pipeline produces valid JSON for every response (test 50 diverse images)

---

**Key takeaways**

- **SigLIP-400M** encodes images as 256 visual tokens (224×224) or 1024 tokens (448×448) that are prepended to the text context as standard input tokens — no special attention masking required.
- **TTFT for Gemma 4 4B-VL on Orin** is approximately 600 ms (80 ms SigLIP + 500 ms prefill + 20 ms decode), fitting within a 1 Hz robot control loop.
- **Always keep the vision encoder in FP16**; only quantize the text decoder to INT4. The SigLIP weights (880 MB FP16) add ~1 GB to the deployment footprint but are not large enough to justify the accuracy loss from INT4.
- **llama.cpp with --mmproj** is the fastest path to a working VLM deployment; Ollama wraps this for ease of use. Use HuggingFace Transformers for prototyping only (BF16 = 8.2 GB text decoder alone).
- **DLA for SigLIP** on Jetson Thor reduces encoder power by 7.5× at the cost of 50% higher latency — use on mobile or drone deployments where power dominates.
- **The GStreamer hardware pipeline** on Jetson resizes frames in the ISP/video converter, eliminating software resize cost and delivering 224×224 frames directly to CUDA.
- **The key latency knob** is not decode speed but **prefill** — 256 visual tokens dominate TTFT. Reduce text prompt length to save prefill time on the text side.

---

**Course completion checklist**

You have completed the Gemma 4 Edge Deployment course when you can:

- [ ] Explain the KV-cache math for Gemma 4's interleaved attention and compute the exact footprint for any (model, batch, context) triple.
- [ ] Calibrate and produce a Q4_K_M GGUF from a Gemma 4 BF16 checkpoint with ≤ 0.3 MMLU degradation.
- [ ] Deploy Gemma 4 4B (text only) on Jetson Orin with llama.cpp, measured at ≥ 45 tok/s.
- [ ] Configure speculative decoding with Gemma 4 1B as draft, measure acceptance length τ, and verify the speedup matches the τ / (1 + K×c) formula.
- [ ] Deploy Gemma 4 4B-VL on Jetson Orin, measure end-to-end TTFT, and verify it meets a ≤ 1 Hz perception loop requirement.
- [ ] Complete the capstone perception module, pass the checklist, and report latency_ms in your notes.

---

</details>

## 参考文献

- "Sigmoid Loss for Language Image Pre-Training"（Zhai 等，2023）— arXiv:2303.15343
- "Gemma 4 Technical Report"（Google DeepMind，2025 年 4 月）— 见 ai.google.dev/gemma
- "PaliGemma 2 VLM"（Beyer 等，2024）— arXiv:2412.03555（Gemma 4-VL 基于 PaliGemma 架构构建）
- llama.cpp 多模态（--mmproj）— [github.com/ggml-org/llama.cpp/tree/master/examples/llava](https://github.com/ggml-org/llama.cpp)
- Google AI Edge / LiteRT — [ai.google.dev/edge/litert](https://ai.google.dev/edge/litert)
- Jetson Thor SDK（DLA v3）— NVIDIA JetPack 7.x SDK 文档
- [Qwen 推理优化 → Lecture 05](../05-Qwen推理优化/Lecture-05.md) — 多模态推理栈，同一 runtime layer

---

## 截至 2026-06 的最新情况

Gemma 4 4B-VL（多模态变体）可在 HuggingFace 获取，见 `google/gemma-4-4b-it`。SigLIP 已集成到模型中，无需单独下载。llama.cpp 对 Gemma 4-VL 的 mmproj 支持：查看 v0.4.0 之后的 llama.cpp 发布版本。LiteRT AI Edge Torch：导出 Gemma 4 需要 `ai-edge-torch >= 0.3.0`。Ollama 视觉支持：`ollama pull gemma4:4b`（最新 tag 见 ollama.com/library）。Jetson AGX Thor：DLA v3 与大于 128 GB 的统一内存需要 JetPack 7.0+。

---

*上一篇：[← Lecture 04 — 使用 Gemma 4 的投机解码](Lecture-04.md) · 上级：[Gemma 4 边缘部署](README.md)*


<details>
<summary>English original</summary>

**References**

- "Sigmoid Loss for Language Image Pre-Training" (Zhai et al., 2023) — arXiv:2303.15343
- "Gemma 4 Technical Report" (Google DeepMind, April 2025) — check ai.google.dev/gemma
- "PaliGemma 2 VLM" (Beyer et al., 2024) — arXiv:2412.03555 (Gemma 4-VL builds on PaliGemma architecture)
- llama.cpp multimodal (--mmproj) — [github.com/ggml-org/llama.cpp/tree/master/examples/llava](https://github.com/ggml-org/llama.cpp)
- Google AI Edge / LiteRT — [ai.google.dev/edge/litert](https://ai.google.dev/edge/litert)
- Jetson Thor SDK (DLA v3) — NVIDIA JetPack 7.x SDK documentation
- [Qwen Inference Optimization → Lecture 05](../05-Qwen推理优化/Lecture-05.md) — multimodal inference stack, same runtime layer

---

**Current as of 2026-06**

Gemma 4 4B-VL available at `google/gemma-4-4b-it` (multimodal variant) on HuggingFace. SigLIP is integrated into the model; no separate download needed. llama.cpp mmproj support for Gemma 4-VL: check llama.cpp releases after v0.4.0. LiteRT AI Edge Torch: requires `ai-edge-torch >= 0.3.0` for Gemma 4 export. Ollama vision support: `ollama pull gemma4:4b` (check ollama.com/library for updated tags). Jetson AGX Thor: JetPack 7.0+ required for DLA v3 and unified memory >128 GB.

---

*Previous: [← Lecture 04 — Speculative Decoding with Gemma 4](Lecture-04.md) · Up: [Gemma 4 Edge Deployment](README.md)*

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track C - Edge AI/Gemma 4 Edge Deployment/Lecture-05.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20C%20-%20Edge%20AI/Gemma%204%20Edge%20Deployment/Lecture-05.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
