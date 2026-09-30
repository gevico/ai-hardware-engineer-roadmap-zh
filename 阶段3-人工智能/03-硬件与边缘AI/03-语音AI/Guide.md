---
title: 模块 6A — Voice AI
description: 模块 6A — Voice AI
---

# 模块 6A — Voice AI

<div class="course-identity auto-course" style="--course-accent: #0891b2; --course-accent-rgb: 8, 145, 178;" markdown="1">
<div class="course-identity__icon">M6VA</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入剖析 · AI 工作负载</p>
<p class="course-identity__title">模块 6A — Voice AI 的专属课程标识。</p>
<p class="course-identity__meta">产物：模型或工作负载研究 · 度量：准确率、延迟、内存、吞吐</p>
</div>
</div>


**Parent:** [阶段 3 — 人工智能](../../Guide.md) · 方向 A

> *语音转文本、文本转语音与实时语音流水线 —— 驱动边缘 AI 硬件的音频工作负载。*

**前置要求：** 模块 1（神经网络 —— Transformer、attention）、模块 2（框架 —— PyTorch）。

**岗位目标：** Voice AI 工程师 · 语音/音频 ML 工程师 · 边缘音频工程师 · 对话式 AI 工程师

---

## 为什么 Voice AI 对硬件工程师重要

语音是延迟最敏感的 AI 工作负载之一。用户在对话中能察觉到 200ms+ 的延迟。这给硬件提出了硬性要求：

- **STT（Speech-to-Text）：** 实时转写需要流式推理，每个 chunk 的延迟 <100ms
- **TTS（Text-to-Speech）：** 自然语音合成需要自回归生成（类似 LLM）或快速扩散模型
- **端侧语音：** 隐私敏感的应用（医疗、军用、汽车）需要无云推理 —— 你的边缘芯片必须撑得住
- **VAD（语音活动检测）：** 常开、超低功耗 —— 跑在 MCU 或专用 DSP 上（L4/L5 硬件）

| 语音任务 | 计算模式 | 硬件含义 |
|-----------|----------------|---------------------|
| VAD | 微型 CNN，常开 | L4：MCU/DSP，功耗预算 < 1mW |
| STT（流式） | encoder-decoder，分块 | L1/L3：流式推理，低延迟 |
| TTS（神经网络） | 自回归或扩散 | L1/L3：带宽受限的生成，类似 LLM decode（逐 token 生成阶段） |
| 关键词检测 | 小型 CNN/RNN，常开 | L4：TinyML，专用音频 NPU |
| 说话人验证 | embedding 模型，单样本 | L1：推理 + 向量相似度 |
| 噪声抑制 | U-Net 或 RNNoise，实时 | L4/L6：DSP 或 FPGA，严格延迟预算 |

---

## 1. 语音转文本（STT / ASR）

### 现代 STT 如何工作

```
Audio Input → Feature Extraction → Encoder → Decoder → Text Output
             (mel spectrogram)    (transformer)  (CTC/attention)
```

### 关键模型与架构

| 模型 | 架构 | 优势 | 使用场景 |
|-------|-------------|-----------|----------|
| **Whisper**（OpenAI） | encoder-decoder Transformer | 多语言、鲁棒、开源 | 通用 STT |
| **Wav2Vec 2.0**（Meta） | 自监督 encoder + CTC | 在无标注音频上预训练 | 低资源语言 |
| **Conformer**（Google） | 卷积 + Transformer | benchmark 上准确率最佳 | 生产级 ASR |
| **DeepSpeech**（Mozilla） | RNN + CTC | 简单、轻量 | 遗留系统 / 嵌入式 |
| **Whisper.cpp** | GGML 量化 Whisper | 无需 GPU，可在 CPU/边缘运行 | 端侧 STT |
| **Faster-Whisper** | CTranslate2 后端 | 比原版 Whisper 快 4 倍 | 生产推理服务 |

### 音频特征提取

```python
import torchaudio

# Load audio
waveform, sample_rate = torchaudio.load("speech.wav")

# Convert to mel spectrogram (the "image" that the model sees)
mel_transform = torchaudio.transforms.MelSpectrogram(
    sample_rate=16000,
    n_fft=400,        # FFT window size
    hop_length=160,   # 10ms hop (160 samples at 16kHz)
    n_mels=80         # 80 mel frequency bins
)
mel = mel_transform(waveform)
# Shape: [1, 80, T] — 80 frequency bins x T time frames
# This is the input to the encoder
```

**为什么这对硬件重要：**
- Mel 频谱图计算是一次 FFT + filterbank —— 可在 DSP 或 FPGA 上加速（L6）
- encoder 处理 mel 频谱图 —— 这是计算量大的部分（矩阵乘）
- 流式 STT 按 chunk 处理（例如每次 1 秒） —— 需要谨慎的状态管理

### 流式 STT 与批处理 STT

| 模式 | 延迟 | 准确率 | 使用场景 |
|------|---------|----------|----------|
| **批处理** | 一次性处理整个音频文件 | 最高 | 转写、字幕 |
| **流式** | 实时处理 chunk | 略低 | 实时对话、语音助手 |

流式需要：
- 分块输入（例如带重叠的 1 秒窗口）
- chunk 之间缓存 encoder 状态
- 输出部分假设并做修正

### 项目

1. 在示例音频文件上**运行 Whisper**。测量推理时间与 WER（词错误率）。
2. **Jetson 上的 Whisper** —— 用 TensorRT 部署 Whisper。测量延迟并与 CPU 对比。
3. **流式 STT** —— 实现分块 Whisper 推理。测量首个词输出时间。
4. **边缘上的 Whisper.cpp** —— 在 Raspberry Pi 或 Jetson 上运行量化 Whisper。对比 INT8 与 FP16 的 benchmark。

---

## 2. 文本转语音（TTS）


<details>
<summary>English original</summary>

**Module 6A — Voice AI**

<div class="course-identity auto-course" style="--course-accent: #0891b2; --course-accent-rgb: 8, 145, 178;" markdown="1">
<div class="course-identity__icon">M6VA</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · AI Workloads</p>
<p class="course-identity__title">Specialized course identity for Module 6A — Voice AI.</p>
<p class="course-identity__meta">Artifact: model or workload study · Measure: accuracy, latency, memory, throughput</p>
</div>
</div>


**Parent:** [Phase 3 — Artificial Intelligence](../../Guide.md) · Track A

> *Speech-to-text, text-to-speech, and real-time voice pipelines — the audio workloads that drive edge AI hardware.*

**Prerequisites:** Module 1 (Neural Networks — transformers, attention), Module 2 (Frameworks — PyTorch).

**Role targets:** Voice AI Engineer · Speech/Audio ML Engineer · Edge Audio Engineer · Conversational AI Engineer

---

**Why Voice AI Matters for Hardware Engineers**

Voice is one of the most latency-sensitive AI workloads. Users notice 200ms+ delay in conversation. This creates hard requirements for hardware:

- **STT (Speech-to-Text):** Real-time transcription needs streaming inference with <100ms latency per chunk
- **TTS (Text-to-Speech):** Natural voice synthesis requires autoregressive generation (like LLMs) or fast diffusion models
- **On-device voice:** Privacy-critical applications (medical, military, automotive) need inference without cloud — your edge chip must handle it
- **VAD (Voice Activity Detection):** Always-on, ultra-low-power — runs on MCU or dedicated DSP (L4/L5 hardware)

| Voice task | Compute pattern | Hardware implication |
|-----------|----------------|---------------------|
| VAD | Tiny CNN, always-on | L4: MCU/DSP, < 1mW power budget |
| STT (streaming) | Encoder-decoder, chunked | L1/L3: streaming inference, low latency |
| TTS (neural) | Autoregressive or diffusion | L1/L3: memory-bound generation, like LLM decode |
| Keyword spotting | Small CNN/RNN, always-on | L4: TinyML, dedicated audio NPU |
| Speaker verification | Embedding model, one-shot | L1: inference + vector similarity |
| Noise suppression | U-Net or RNNoise, real-time | L4/L6: DSP or FPGA, strict latency budget |

---

**1. Speech-to-Text (STT / ASR)**

**How Modern STT Works**

```
Audio Input → Feature Extraction → Encoder → Decoder → Text Output
             (mel spectrogram)    (transformer)  (CTC/attention)
```

**Key Models and Architectures**

| Model | Architecture | Strengths | Use case |
|-------|-------------|-----------|----------|
| **Whisper** (OpenAI) | Encoder-decoder transformer | Multi-language, robust, open-source | General-purpose STT |
| **Wav2Vec 2.0** (Meta) | Self-supervised encoder + CTC | Pre-trained on unlabeled audio | Low-resource languages |
| **Conformer** (Google) | Convolution + transformer | Best accuracy on benchmarks | Production ASR |
| **DeepSpeech** (Mozilla) | RNN + CTC | Simple, lightweight | Legacy / embedded |
| **Whisper.cpp** | GGML quantized Whisper | Runs on CPU/edge without GPU | On-device STT |
| **Faster-Whisper** | CTranslate2 backend | 4x faster than original Whisper | Production serving |

**Audio Feature Extraction**

```python
import torchaudio

# Load audio
waveform, sample_rate = torchaudio.load("speech.wav")

# Convert to mel spectrogram (the "image" that the model sees)
mel_transform = torchaudio.transforms.MelSpectrogram(
    sample_rate=16000,
    n_fft=400,        # FFT window size
    hop_length=160,   # 10ms hop (160 samples at 16kHz)
    n_mels=80         # 80 mel frequency bins
)
mel = mel_transform(waveform)
# Shape: [1, 80, T] — 80 frequency bins x T time frames
# This is the input to the encoder
```

**Why this matters for hardware:**
- Mel spectrogram computation is an FFT + filterbank — can be accelerated on DSP or FPGA (L6)
- The encoder processes the mel spectrogram — this is the compute-heavy part (matrix multiply)
- Streaming STT processes chunks (e.g., 1 second at a time) — requires careful state management

**Streaming vs Batch STT**

| Mode | Latency | Accuracy | Use case |
|------|---------|----------|----------|
| **Batch** | Process entire audio file at once | Highest | Transcription, subtitles |
| **Streaming** | Process chunks in real-time | Slightly lower | Live conversation, voice assistant |

Streaming requires:
- Chunked input (e.g., 1-second windows with overlap)
- Encoder state caching between chunks
- Partial hypothesis output with correction

**Projects**

1. **Run Whisper** on a sample audio file. Measure inference time and WER (word error rate).
2. **Whisper on Jetson** — deploy Whisper with TensorRT. Measure latency and compare with CPU.
3. **Streaming STT** — implement chunked Whisper inference. Measure time-to-first-word.
4. **Whisper.cpp on edge** — run quantized Whisper on Raspberry Pi or Jetson. Benchmark INT8 vs FP16.

---

**2. Text-to-Speech (TTS)**

</details>

### 现代 TTS 如何工作

```
Text Input → Text Analysis → Acoustic Model → Vocoder → Audio Output
             (phonemes,       (mel spectrogram   (waveform
              prosody)         generation)         synthesis)
```

### 关键模型与架构

| 模型 | 类型 | 质量 | 速度 | 用例 |
|-------|------|---------|-------|----------|
| **VITS** | 端到端（文本 → 音频） | 高 | 快 | 生产 TTS |
| **Bark**（Suno） | GPT 风格自回归 | 非常高，富有表现力 | 慢 | 创意、多语言 |
| **Tortoise TTS** | 自回归 + 扩散 | 最高质量 | 非常慢 | 声音克隆 |
| **Piper** | 基于 VITS，已优化 | 良好 | 非常快 | 端侧、嵌入式 |
| **Coqui TTS** | 多种架构 | 高 | 中 | 开源工具包 |
| **F5-TTS** | 流匹配 | 高，零样本 | 快 | 声音克隆、多语言 |
| **XTTS**（Coqui） | GPT + VITS | 高，声音克隆 | 中 | 多说话人、多语言 |

### TTS 流水线组件

**文本分析：**
- 文本归一化（数字、缩写、日期 → 词）
- 字素到音素（G2P）转换
- 韵律预测（时长、音高、能量）

**声学模型（mel 生成）：**
- 从音素序列生成 mel 频谱图
- 自回归（Tacotron 2）或非自回归（FastSpeech 2、VITS）
- 非自回归更快 — 更适合边缘部署

**声码器（波形合成）：**
- 将 mel 频谱图 → 音频波形
- HiFi-GAN：快、高质量、轻量
- WaveGlow / WaveNet：质量更高、慢得多

```python
# Piper TTS — fast on-device TTS
import piper

voice = piper.PiperVoice.load("en_US-lessac-medium.onnx")
audio = voice.synthesize("Hello, I am running on edge hardware.")
# Runs on CPU, ~10x real-time on Raspberry Pi 4
```

### 项目

1. 在 CPU 上运行 Piper TTS。测量实时因子（RTF）。目标：RTF < 0.1（比实时快 10 倍）。
2. **Jetson 上的 VITS** — 用 ONNX Runtime 或 TensorRT 部署 VITS。测量每句延迟。
3. **声音克隆** — 用 XTTS 或 F5-TTS 从 10 秒样本克隆声音。
4. **HiFi-GAN 声码器** — 独立运行，在 GPU 与 CPU 上 benchmark。理解 mel → 波形瓶颈。

---

## 3. 语音活动检测（VAD）与关键词识别

### VAD — 有人在说话吗？

始终开启，超低功耗。持续运行以唤醒完整的 STT 流水线。

| 模型 | 尺寸 | 延迟 | 功耗 | 平台 |
|-------|------|---------|-------|----------|
| **Silero VAD** | 1.5 MB | 每帧 <1ms | MCU 上约 10mW | CPU、边缘 |
| **WebRTC VAD** | <100 KB | <0.1ms | 约 1mW | 任意 CPU |
| **自定义 CNN VAD** | 50–500 KB | <1ms | <5mW | MCU、DSP |

```python
# Silero VAD — production-quality, lightweight
import torch
model, utils = torch.hub.load('snakers4/silero-vad', 'silero_vad')
(get_speech_timestamps, _, read_audio, _, _) = utils

wav = read_audio('speech.wav', sampling_rate=16000)
timestamps = get_speech_timestamps(wav, model, sampling_rate=16000)
# Returns: [{'start': 1000, 'end': 15000}, ...] — speech segments
```

### 关键词识别 —“Hey [Device]”

始终开启的唤醒词检测。对电池供电设备，必须运行在 < 1mW。

- **模型：** 小型 CNN（DS-CNN）、RNN 或基于 attention
- **训练：** 通常针对特定唤醒词定制训练
- **部署：** Cortex-M 上的 TFLite Micro、专用音频 DSP
- **与 L4/L5 的联系：** 这是你会为其设计定制硅的工作负载（always-on NPU）

### 噪声抑制 / 增强

STT 前的实时音频清理。

| 模型 | 方法 | 延迟 | 质量 |
|-------|----------|---------|---------|
| **RNNoise** | 基于 GRU，手工特征 | <5ms | 良好 |
| **DTLN** | 双信号 Transformer | ~10ms | 高 |
| **DeepFilterNet** | 基于 attention 的滤波器组 | ~10ms | 非常高 |
| **NSNet2** | 稠密 + GRU | <5ms | 良好 |

### 项目

1. **Silero VAD** — 在连续音频流上运行。测量检测延迟与误报率。
2. **MCU 上的关键词识别器** — 为自定义唤醒词训练一个小型 CNN。用 TFLite Micro 部署到 Cortex-M。
3. **FPGA 上的 RNNoise** — 用 HLS 实现基于 GRU 的噪声抑制（与阶段 4A 的联系）。

---

## 4. 端到端语音流水线


<details>
<summary>English original</summary>

**How Modern TTS Works**

```
Text Input → Text Analysis → Acoustic Model → Vocoder → Audio Output
             (phonemes,       (mel spectrogram   (waveform
              prosody)         generation)         synthesis)
```

**Key Models and Architectures**

| Model | Type | Quality | Speed | Use case |
|-------|------|---------|-------|----------|
| **VITS** | End-to-end (text → audio) | High | Fast | Production TTS |
| **Bark** (Suno) | GPT-style autoregressive | Very high, expressive | Slow | Creative, multi-language |
| **Tortoise TTS** | Autoregressive + diffusion | Highest quality | Very slow | Voice cloning |
| **Piper** | VITS-based, optimized | Good | Very fast | On-device, embedded |
| **Coqui TTS** | Multiple architectures | High | Medium | Open-source toolkit |
| **F5-TTS** | Flow matching | High, zero-shot | Fast | Voice cloning, multilingual |
| **XTTS** (Coqui) | GPT + VITS | High, voice cloning | Medium | Multi-speaker, multi-language |

**TTS Pipeline Components**

**Text analysis:**
- Text normalization (numbers, abbreviations, dates → words)
- Grapheme-to-phoneme (G2P) conversion
- Prosody prediction (duration, pitch, energy)

**Acoustic model (mel generation):**
- Generates mel spectrogram from phoneme sequence
- Autoregressive (Tacotron 2) or non-autoregressive (FastSpeech 2, VITS)
- Non-autoregressive is faster — better for edge deployment

**Vocoder (waveform synthesis):**
- Converts mel spectrogram → audio waveform
- HiFi-GAN: fast, high-quality, lightweight
- WaveGlow / WaveNet: higher quality, much slower

```python
# Piper TTS — fast on-device TTS
import piper

voice = piper.PiperVoice.load("en_US-lessac-medium.onnx")
audio = voice.synthesize("Hello, I am running on edge hardware.")
# Runs on CPU, ~10x real-time on Raspberry Pi 4
```

**Projects**

1. **Run Piper TTS** on CPU. Measure real-time factor (RTF). Target: RTF < 0.1 (10x faster than real-time).
2. **VITS on Jetson** — deploy VITS with ONNX Runtime or TensorRT. Measure latency per sentence.
3. **Voice cloning** — use XTTS or F5-TTS to clone a voice from a 10-second sample.
4. **HiFi-GAN vocoder** — run standalone, benchmark on GPU vs CPU. Understand the mel → waveform bottleneck.

---

**3. Voice Activity Detection (VAD) & Keyword Spotting**

**VAD — Is Someone Speaking?**

Always-on, ultra-low-power. Runs continuously to wake up the full STT pipeline.

| Model | Size | Latency | Power | Platform |
|-------|------|---------|-------|----------|
| **Silero VAD** | 1.5 MB | <1ms per frame | ~10mW on MCU | CPU, edge |
| **WebRTC VAD** | <100 KB | <0.1ms | ~1mW | Any CPU |
| **Custom CNN VAD** | 50–500 KB | <1ms | <5mW | MCU, DSP |

```python
# Silero VAD — production-quality, lightweight
import torch
model, utils = torch.hub.load('snakers4/silero-vad', 'silero_vad')
(get_speech_timestamps, _, read_audio, _, _) = utils

wav = read_audio('speech.wav', sampling_rate=16000)
timestamps = get_speech_timestamps(wav, model, sampling_rate=16000)
# Returns: [{'start': 1000, 'end': 15000}, ...] — speech segments
```

**Keyword Spotting — "Hey [Device]"**

Always-on wake word detection. Must run at < 1mW for battery-powered devices.

- **Models:** Small CNNs (DS-CNN), RNNs, or attention-based
- **Training:** Typically custom-trained on the specific wake word
- **Deployment:** TFLite Micro on Cortex-M, dedicated audio DSP
- **Connection to L4/L5:** This is a workload you'd design custom silicon for (always-on NPU)

**Noise Suppression / Enhancement**

Real-time audio cleanup before STT.

| Model | Approach | Latency | Quality |
|-------|----------|---------|---------|
| **RNNoise** | GRU-based, handcrafted features | <5ms | Good |
| **DTLN** | Dual-signal transformer | ~10ms | High |
| **DeepFilterNet** | Attention-based filterbank | ~10ms | Very high |
| **NSNet2** | Dense + GRU | <5ms | Good |

**Projects**

1. **Silero VAD** — run on a continuous audio stream. Measure detection latency and false positive rate.
2. **Keyword spotter on MCU** — train a small CNN for a custom wake word. Deploy with TFLite Micro on Cortex-M.
3. **RNNoise on FPGA** — implement the GRU-based noise suppression in HLS (connection to Phase 4A).

---

**4. End-to-End Voice Pipeline**

</details>

### 边缘语音助手架构

```
┌─────────────────────────────────────────────────────────┐
│                   Edge Device (Jetson / MCU+NPU)         │
│                                                          │
│  ┌─────────┐   ┌──────────┐   ┌──────────┐            │
│  │  VAD    │──▶│   STT    │──▶│  NLU /   │            │
│  │(always  │   │(Whisper  │   │  LLM     │            │
│  │  on)    │   │ or       │   │(on-device│            │
│  └─────────┘   │Conformer)│   │ or cloud)│            │
│                └──────────┘   └────┬─────┘            │
│                                    │                    │
│                                    ▼                    │
│                              ┌──────────┐              │
│  ┌─────────┐                │   TTS    │              │
│  │ Speaker │◀───────────────│  (VITS/  │              │
│  │         │                │  Piper)  │              │
│  └─────────┘                └──────────┘              │
└─────────────────────────────────────────────────────────┘
```

**自然对话的延迟预算：**

| 阶段 | 目标 | 模型 |
|-------|--------|-------|
| VAD 检测 | < 50ms | Silero VAD |
| STT（流式） | 到首词 < 300ms | Whisper / Conformer |
| NLU / 大语言模型响应 | < 500ms | 端侧小型大语言模型或云端 |
| TTS 合成 | 到首段音频 < 200ms | VITS / Piper |
| **总往返** | **< 1 秒** | |

### 项目

1. **Jetson 上的完整流水线** — VAD → Whisper STT → 简单 NLU → Piper TTS。测量端到端延迟。
2. **面向延迟优化** — 把 STT 量化为 INT8，使用流式分块推理，预热 TTS。目标往返 < 800ms。
3. **对比云端与边缘** — 同一条流水线分别跑在 Jetson 与云端 API 上。测量延迟、准确率与隐私权衡。

---

## 5. 硬件设计语境下的语音 AI

| 语音工作负载 | 计算模式 | 硬件工程师为何关注 |
|---------------|----------------|-----------------------------|
| Mel 频谱图 | FFT + filterbank | 可在 DSP/FPGA 中加速 — 固定功能与通用计算的权衡 |
| 编码器（Conformer） | attention + conv | 与视觉同为矩阵乘密集型计算 — 适用脉动阵列 |
| 自回归 decode（逐 token 生成阶段） | 逐 token 串行生成 | 与大语言模型 decode 一样带宽受限 — HBM 带宽至关重要 |
| 声码器（HiFi-GAN） | 转置卷积、上采样 | 计算模式独特 — 标准矩阵乘加速器难以适配 |
| VAD / 关键词 | 小型 CNN/RNN | always-on NPU 设计的目标（< 1mW） — 阶段 5F AI 芯片设计 |
| 噪声抑制 | GRU / filterbank | 严格延迟下的实时流式 — FPGA 或专用 DSP |

---

## 资源

| 资源 | 覆盖内容 |
|----------|---------------|
| [Whisper](https://github.com/openai/whisper) | 开源 STT 模型 |
| [Faster-Whisper](https://github.com/SYSTRAN/faster-whisper) | 基于 CTranslate2 的快速 Whisper |
| [Whisper.cpp](https://github.com/ggerganov/whisper.cpp) | 面向边缘部署的 C/C++ 移植 |
| [Piper](https://github.com/rhasspy/piper) | 快速的端侧 TTS |
| [Coqui TTS](https://github.com/coqui-ai/TTS) | 开源 TTS 工具包 |
| [Silero VAD](https://github.com/snakers4/silero-vad) | 生产级 VAD |
| [RNNoise](https://github.com/xiph/rnnoise) | 实时噪声抑制 |
| [ESPnet](https://github.com/espnet/espnet) | 端到端语音处理工具包 |
| [SpeechBrain](https://github.com/speechbrain/speechbrain) | PyTorch 语音工具包 |

---

## 下一节

→ [**模块 5A — 边缘 AI 与模型优化**](../04-边缘AI与模型优化/Guide.md) — 在边缘硬件上量化并部署这些语音模型。


<details>
<summary>English original</summary>

**Architecture for Edge Voice Assistant**

```
┌─────────────────────────────────────────────────────────┐
│                   Edge Device (Jetson / MCU+NPU)         │
│                                                          │
│  ┌─────────┐   ┌──────────┐   ┌──────────┐            │
│  │  VAD    │──▶│   STT    │──▶│  NLU /   │            │
│  │(always  │   │(Whisper  │   │  LLM     │            │
│  │  on)    │   │ or       │   │(on-device│            │
│  └─────────┘   │Conformer)│   │ or cloud)│            │
│                └──────────┘   └────┬─────┘            │
│                                    │                    │
│                                    ▼                    │
│                              ┌──────────┐              │
│  ┌─────────┐                │   TTS    │              │
│  │ Speaker │◀───────────────│  (VITS/  │              │
│  │         │                │  Piper)  │              │
│  └─────────┘                └──────────┘              │
└─────────────────────────────────────────────────────────┘
```

**Latency budget for natural conversation:**

| Stage | Target | Model |
|-------|--------|-------|
| VAD detection | < 50ms | Silero VAD |
| STT (streaming) | < 300ms to first word | Whisper / Conformer |
| NLU / LLM response | < 500ms | On-device small LLM or cloud |
| TTS synthesis | < 200ms to first audio | VITS / Piper |
| **Total round-trip** | **< 1 second** | |

**Projects**

1. **Full pipeline on Jetson** — VAD → Whisper STT → simple NLU → Piper TTS. Measure end-to-end latency.
2. **Optimize for latency** — quantize STT to INT8, use streaming chunked inference, pre-warm TTS. Target < 800ms round-trip.
3. **Compare cloud vs edge** — same pipeline on Jetson vs cloud API. Measure latency, accuracy, and privacy trade-off.

---

**5. Voice AI for Hardware Design Context**

| Voice workload | Compute pattern | Why hardware engineers care |
|---------------|----------------|-----------------------------|
| Mel spectrogram | FFT + filterbank | Can be accelerated in DSP/FPGA — fixed-function vs general compute trade-off |
| Encoder (Conformer) | Attention + conv | Same matmul-heavy compute as vision — systolic arrays apply |
| Autoregressive decode | Sequential token generation | Memory-bound like LLM decode — HBM bandwidth matters |
| Vocoder (HiFi-GAN) | Transposed conv, upsampling | Unique compute pattern — not well-served by standard matmul accelerators |
| VAD / keyword | Tiny CNN/RNN | Target for always-on NPU design (< 1mW) — Phase 5F AI Chip Design |
| Noise suppression | GRU / filterbank | Real-time streaming with strict latency — FPGA or dedicated DSP |

---

**Resources**

| Resource | What it covers |
|----------|---------------|
| [Whisper](https://github.com/openai/whisper) | Open-source STT model |
| [Faster-Whisper](https://github.com/SYSTRAN/faster-whisper) | CTranslate2-based fast Whisper |
| [Whisper.cpp](https://github.com/ggerganov/whisper.cpp) | C/C++ port for edge deployment |
| [Piper](https://github.com/rhasspy/piper) | Fast on-device TTS |
| [Coqui TTS](https://github.com/coqui-ai/TTS) | Open-source TTS toolkit |
| [Silero VAD](https://github.com/snakers4/silero-vad) | Production-quality VAD |
| [RNNoise](https://github.com/xiph/rnnoise) | Real-time noise suppression |
| [ESPnet](https://github.com/espnet/espnet) | End-to-end speech processing toolkit |
| [SpeechBrain](https://github.com/speechbrain/speechbrain) | PyTorch speech toolkit |

---

**Next**

→ [**Module 5A — Edge AI & Model Optimization**](../04-边缘AI与模型优化/Guide.md) — quantize and deploy these voice models on edge hardware.

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track A - Hardware and Edge AI/5. Voice AI/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20A%20-%20Hardware%20and%20Edge%20AI/5.%20Voice%20AI/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
