---
title: 使用 Unsloth 微调 Qwen3.5-4B-Base
description: 使用 Unsloth 微调 Qwen3.5-4B-Base
---

# 使用 Unsloth 微调 Qwen3.5-4B-Base

<div class="course-identity auto-course" style="--course-accent: #be123c; --course-accent-rgb: 190, 18, 60;" markdown="1">
<div class="course-identity__icon">Q4BF</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度剖析 · AI 工作负载</p>
<p class="course-identity__title">使用 Unsloth 微调 Qwen3.5-4B-Base 的专用课程标识。</p>
<p class="course-identity__meta">产物：模型或工作负载研究 · 度量：准确率、延迟、内存、吞吐</p>
</div>

</div>


**父级：** [模块 5B - LLM 应用开发](../Guide.md) / [阶段 3 - 人工智能](../../../Guide.md)

> *用 Unsloth 微调 `Qwen/Qwen3.5-4B-Base`，然后度量对 AI 硬件工程师至关重要的训练、导出与推理成本。*

**层映射：** **L1**（应用与框架）供给 **L2/L3**（编译器/runtime）与 **L5/L6**（内存、精度、加速器设计）。

**前置要求：** PyTorch 基础、tokenizer/chat-template 熟练度、LoRA/PEFT 概念，以及一块内存足以运行 16-bit LoRA 的 CUDA GPU。

**目标角色：** AI 工程师 / LLM 微调工程师 / ML 平台工程师 / AI 推理工程师

**课程产出：** 可复现的 LoRA 适配器、评估报告、导出的推理产物，以及一份解释 VRAM、吞吐与部署取舍的简短硬件说明。

---

## 课程时效性说明

模型 ID、Unsloth 支持与内存需求变化很快。截至 2026 年 5 月，Unsloth 在其 Qwen3.5 支持中记录了 `Qwen/Qwen3.5-4B-Base`，并建议该系列使用 16-bit LoRA，而非 4-bit QLoRA。在把命令复制进真实训练任务之前，请查阅当前的 Qwen 模型卡与 Unsloth Qwen3.5 指南。

---

## 为什么这对 AI 硬件很重要

微调不只是一项模型质量练习。它会改变硬件必须承载的部署产物：

- **适配器 vs 合并权重：** 未合并的 LoRA 在推理时增加额外的低秩矩阵乘；合并权重消除了这部分开销，但会产生新的模型产物。
- **精度选择：** 16-bit LoRA、训练后量化以及 GGUF/AWQ/GPTQ 导出都会改变内存流量与 kernel 选择。
- **上下文长度：** SFT 序列长度决定训练激活值内存，评估上下文长度决定 KV-cache 内存。
- **数据形态：** 短指令样例、长工具 trace 与多模态样例会给栈的不同部分带来压力。
- **推理服务目标：** 微调后的 4B 模型可以在工作站/云 GPU 上训练，然后在 Jetson 或自定义 runtime 上量化与性能剖析。

与硬件相关的问题不是“loss 下降了吗？”。而是：**你创建了什么产物、它有多大、使用什么精度、在目标硬件上运行多快？**

---

## 你将构建什么

| 产物 | 最低期望 |
|----------|---------------------|
| 数据集 | 带文档化 chat template 的 JSONL 训练/验证划分 |
| 训练运行 | 在 `Qwen/Qwen3.5-4B-Base` 上运行 Unsloth LoRA SFT |
| 适配器 | 保存的 LoRA 适配器，含配置、tokenizer 与训练元数据 |
| 评估 | 在留出 prompt 上对比 base 与适配器 |
| 导出 | 合并的 16-bit 模型或量化部署产物 |
| Benchmark | VRAM、训练 tokens/s、评估 tokens/s、适配器大小与输出质量说明 |

---

## 硬件预算

在开始优化 kernel 之前，用本课程了解内存包络。

| 目标 | 用途 | 说明 |
|--------|------------|-------|
| 12-16 GB CUDA GPU | 适中序列长度的小型 LoRA SFT | 在 Ampere 架构或更新架构上使用 BF16；BF16 不可用时使用 FP16 |
| 24 GB CUDA GPU | 更长序列长度、更大的批、更稳妥的评测 | 良好的本地开发目标 |
| 云 L40S/A100/H100 | 对 LoRA rank、序列长度与批进行扫描 | 需要干净的吞吐数据时使用 |
| Jetson Orin Nano / NX | 仅用于训练后推理性能剖析 | 不要将 Jetson 当作主要训练目标 |

让首次运行保持平淡：序列长度 2048、LoRA rank 16、小而干净的数据集，以及一个评估脚本。只有在流水线可复现之后才扩展。

---

## 1. 定义微调任务

从精确的目标开始。好的目标足够狭窄，可以直接对 base 模型进行评估：

- 硬件手册的领域问答
- CUDA/Jetson 工作流的终端命令解释
- 从日志中进行结构化 bug 分诊
- 从自然语言生成工具调用参数
- 带引用源片段的简短硬件设计辅导

避免把首次运行变成通用助手。base 模型需要训练数据同时教行为与格式，因此含糊的数据会产生含糊的失败。


<details>
<summary>English original</summary>

**Qwen3.5-4B-Base Fine-Tuning with Unsloth**

<div class="course-identity auto-course" style="--course-accent: #be123c; --course-accent-rgb: 190, 18, 60;" markdown="1">
<div class="course-identity__icon">Q4BF</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · AI Workloads</p>
<p class="course-identity__title">Specialized course identity for Qwen3.5-4B-Base Fine-Tuning with Unsloth.</p>
<p class="course-identity__meta">Artifact: model or workload study · Measure: accuracy, latency, memory, throughput</p>
</div>
</div>


**Parent:** [Module 5B - LLM Application Development](../Guide.md) / [Phase 3 - Artificial Intelligence](../../../Guide.md)

> *Fine-tune `Qwen/Qwen3.5-4B-Base` with Unsloth, then measure the training, export, and inference costs that matter to an AI hardware engineer.*

**Layer mapping:** **L1** (Application & Framework) feeding **L2/L3** (compiler/runtime) and **L5/L6** (memory, precision, accelerator design).

**Prerequisites:** PyTorch basics, tokenizer/chat-template fluency, LoRA/PEFT concepts, and one CUDA GPU with enough memory for 16-bit LoRA.

**Role targets:** AI Engineer / LLM Fine-Tuning Engineer / ML Platform Engineer / AI Inference Engineer

**Course output:** a reproducible LoRA adapter, an evaluation report, an exported inference artifact, and a short hardware note explaining VRAM, throughput, and deployment tradeoffs.

---

**Course Currency Note**

Model IDs, Unsloth support, and memory requirements change quickly. As of May 2026, Unsloth documents `Qwen/Qwen3.5-4B-Base` as part of its Qwen3.5 support and recommends 16-bit LoRA rather than 4-bit QLoRA for this family. Check the current Qwen model card and Unsloth Qwen3.5 guide before copying commands into a real training job.

---

**Why This Matters for AI Hardware**

Fine-tuning is not just a model-quality exercise. It changes the deployment artifact your hardware must serve:

- **Adapter vs merged weights:** unmerged LoRA adds extra low-rank matmuls at inference; merged weights remove that overhead but create a new model artifact.
- **Precision choices:** 16-bit LoRA, post-training quantization, and GGUF/AWQ/GPTQ exports all change memory traffic and kernel choices.
- **Context length:** SFT sequence length drives training activation memory, and evaluation context length drives KV-cache memory.
- **Data shape:** short instruction examples, long tool traces, and multimodal examples stress very different parts of the stack.
- **Serving target:** a fine-tuned 4B model can be trained on a workstation/cloud GPU, then quantized and profiled on Jetson or a custom runtime.

The hardware-relevant question is not "did loss go down?" It is: **what artifact did you create, how big is it, what precision does it use, and how fast does it run on the target hardware?**

---

**What You Will Build**

| Artifact | Minimum expectation |
|----------|---------------------|
| Dataset | JSONL train/validation split with a documented chat template |
| Training run | Unsloth LoRA SFT run on `Qwen/Qwen3.5-4B-Base` |
| Adapter | Saved LoRA adapter with config, tokenizer, and training metadata |
| Evaluation | Base-vs-adapter comparison on held-out prompts |
| Export | Merged 16-bit model or quantized deployment artifact |
| Benchmark | VRAM, train tokens/s, eval tokens/s, adapter size, and output-quality notes |

---

**Hardware Budget**

Use this course to learn the memory envelope before you start optimizing kernels.

| Target | Use it for | Notes |
|--------|------------|-------|
| 12-16 GB CUDA GPU | small LoRA SFT at modest sequence length | Use BF16 on Ampere or newer; use FP16 when BF16 is unavailable |
| 24 GB CUDA GPU | longer sequence length, larger batch, safer eval | Good local development target |
| Cloud L40S/A100/H100 | sweeps over LoRA rank, sequence length, and batch | Use when you want clean throughput data |
| Jetson Orin Nano / NX | post-training inference profiling only | Do not treat Jetson as the primary training target |

Keep the first run boring: sequence length 2048, LoRA rank 16, small clean dataset, and one evaluation script. Expand only after the pipeline is reproducible.

---

**1. Define the Fine-Tuning Job**

Start with a precise objective. Good objectives are narrow enough that the base model can be evaluated directly:

- domain Q&A over a hardware manual
- terminal command explanation for CUDA/Jetson workflows
- structured bug triage from logs
- tool-call argument generation from natural language
- short hardware-design tutoring with cited source snippets

Avoid turning the first run into a general assistant. A base model needs the training data to teach both behavior and format, so vague data creates vague failures.

</details>

### 数据集 Schema

使用 JSONL，并显式指定角色：

```json
{"system":"You are a concise AI hardware engineering assistant.","user":"Explain why Qwen decode is memory-bandwidth bound at batch 1.","assistant":"At batch 1, each generated token streams large weight matrices for GEMV while doing little arithmetic reuse..."}
```

保留一个训练器永远看不到的验证文件：

```text
data/qwen35_train.jsonl
data/qwen35_valid.jsonl
```

### 对话模板

对于监督微调，精确的序列化文本至关重要。可用时使用 tokenizer 的对话模板，而不是自行发明一个：

```python
def format_example(example, tokenizer):
    messages = [
        {"role": "system", "content": example["system"]},
        {"role": "user", "content": example["user"]},
        {"role": "assistant", "content": example["assistant"]},
    ]
    return {
        "text": tokenizer.apply_chat_template(
            messages,
            tokenize=False,
            add_generation_prompt=False,
        )
    }
```

需要警惕的失效模式：损失下降，但生成永不停止或重复角色标签。这通常意味着 EOS/对话模板不匹配，而不是硬件问题。

---

## 2. 环境搭建

使用全新的虚拟环境。Qwen3.5 支持可能需要最新的 `transformers` 和 Unsloth 包。

```bash
python -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install --upgrade unsloth unsloth_zoo
pip install --upgrade datasets trl accelerate peft bitsandbytes torchvision pillow
```

Qwen3.5 支持目前期望当前的 Unsloth 技术栈和 Transformers v5。如果包解析安装了较旧的 Transformers 构建，在调试训练代码之前，请遵循 Unsloth 当前的安装指南。

记录实际版本：

```bash
python - <<'PY'
import torch, transformers, trl, peft
print("torch", torch.__version__)
print("cuda", torch.version.cuda)
print("transformers", transformers.__version__)
print("trl", trl.__version__)
print("peft", peft.__version__)
PY
```

将此输出放入最终报告。没有包版本的微调结果难以复现。

---

## 3. 使用 Unsloth LoRA 训练

首先使用 16-bit LoRA。除非已确认你的确切 Qwen3.5 目标和 Unsloth 版本对其支持足以达到你的质量门槛，否则不要从 4-bit QLoRA 开始。

```python
import torch
from datasets import load_dataset
from trl import SFTConfig, SFTTrainer
from unsloth import FastLanguageModel

MODEL_ID = "Qwen/Qwen3.5-4B-Base"
MAX_SEQ_LENGTH = 2048

model, tokenizer = FastLanguageModel.from_pretrained(
    model_name=MODEL_ID,
    max_seq_length=MAX_SEQ_LENGTH,
    dtype=torch.bfloat16 if torch.cuda.is_bf16_supported() else torch.float16,
    load_in_4bit=False,
    load_in_16bit=True,
    full_finetuning=False,
)

model = FastLanguageModel.get_peft_model(
    model,
    r=16,
    lora_alpha=16,
    lora_dropout=0,
    bias="none",
    use_gradient_checkpointing="unsloth",
    random_state=3407,
    max_seq_length=MAX_SEQ_LENGTH,
    target_modules=[
        "q_proj", "k_proj", "v_proj", "o_proj",
        "gate_proj", "up_proj", "down_proj",
    ],
)

raw = load_dataset(
    "json",
    data_files={
        "train": "data/qwen35_train.jsonl",
        "validation": "data/qwen35_valid.jsonl",
    },
)

def to_text(example):
    messages = [
        {"role": "system", "content": example["system"]},
        {"role": "user", "content": example["user"]},
        {"role": "assistant", "content": example["assistant"]},
    ]
    return {
        "text": tokenizer.apply_chat_template(
            messages,
            tokenize=False,
            add_generation_prompt=False,
        )
    }

dataset = raw.map(to_text, remove_columns=raw["train"].column_names)

trainer = SFTTrainer(
    model=model,
    tokenizer=tokenizer,
    train_dataset=dataset["train"],
    eval_dataset=dataset["validation"],
    dataset_text_field="text",
    args=SFTConfig(
        output_dir="runs/qwen35-4b-unsloth-lora",
        max_seq_length=MAX_SEQ_LENGTH,
        per_device_train_batch_size=1,
        gradient_accumulation_steps=8,
        learning_rate=2e-4,
        warmup_ratio=0.03,
        num_train_epochs=1,
        logging_steps=10,
        eval_steps=100,
        save_steps=100,
        bf16=torch.cuda.is_bf16_supported(),
        fp16=not torch.cuda.is_bf16_supported(),
        optim="adamw_8bit",
        report_to="none",
    ),
)

trainer.train()
trainer.save_model("artifacts/qwen35-4b-unsloth-lora")
tokenizer.save_pretrained("artifacts/qwen35-4b-unsloth-lora")
```

对于更大规模的运行，一次只扫描一个变量：

| 变量 | 起始值 | 扫描 |
|----------|----------------|-------|
| LoRA rank | 16 | 8, 16, 32 |
| 序列长度 | 2048 | 1024, 2048, 4096 |
| 有效批大小 | 8 | 8, 16, 32 |
| 学习率 | 2e-4 | 1e-4, 2e-4, 5e-5 |

除非数据集划分、随机种子、序列长度和评测提示词固定，否则不要比较运行结果。

---


<details>
<summary>English original</summary>

**Dataset Schema**

Use JSONL with explicit roles:

```json
{"system":"You are a concise AI hardware engineering assistant.","user":"Explain why Qwen decode is memory-bandwidth bound at batch 1.","assistant":"At batch 1, each generated token streams large weight matrices for GEMV while doing little arithmetic reuse..."}
```

Keep a validation file that the trainer never sees:

```text
data/qwen35_train.jsonl
data/qwen35_valid.jsonl
```

**Chat Template**

For supervised fine-tuning, the exact serialized text matters. Use the tokenizer's chat template when available instead of inventing one:

```python
def format_example(example, tokenizer):
    messages = [
        {"role": "system", "content": example["system"]},
        {"role": "user", "content": example["user"]},
        {"role": "assistant", "content": example["assistant"]},
    ]
    return {
        "text": tokenizer.apply_chat_template(
            messages,
            tokenize=False,
            add_generation_prompt=False,
        )
    }
```

Failure mode to watch for: loss falls, but generation never stops or repeats role tags. That usually means an EOS/chat-template mismatch, not a hardware problem.

---

**2. Environment Setup**

Use a fresh virtual environment. Qwen3.5 support may require current `transformers` and Unsloth packages.

```bash
python -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install --upgrade unsloth unsloth_zoo
pip install --upgrade datasets trl accelerate peft bitsandbytes torchvision pillow
```

Qwen3.5 support currently expects the current Unsloth stack and Transformers v5. If package resolution installs an older Transformers build, follow Unsloth's current install guidance before debugging training code.

Record the actual versions:

```bash
python - <<'PY'
import torch, transformers, trl, peft
print("torch", torch.__version__)
print("cuda", torch.version.cuda)
print("transformers", transformers.__version__)
print("trl", trl.__version__)
print("peft", peft.__version__)
PY
```

Put this output in your final report. Fine-tuning results without package versions are hard to reproduce.

---

**3. Train with Unsloth LoRA**

Use 16-bit LoRA first. Do not start with 4-bit QLoRA unless you have confirmed that your exact Qwen3.5 target and Unsloth version support it well enough for your quality bar.

```python
import torch
from datasets import load_dataset
from trl import SFTConfig, SFTTrainer
from unsloth import FastLanguageModel

MODEL_ID = "Qwen/Qwen3.5-4B-Base"
MAX_SEQ_LENGTH = 2048

model, tokenizer = FastLanguageModel.from_pretrained(
    model_name=MODEL_ID,
    max_seq_length=MAX_SEQ_LENGTH,
    dtype=torch.bfloat16 if torch.cuda.is_bf16_supported() else torch.float16,
    load_in_4bit=False,
    load_in_16bit=True,
    full_finetuning=False,
)

model = FastLanguageModel.get_peft_model(
    model,
    r=16,
    lora_alpha=16,
    lora_dropout=0,
    bias="none",
    use_gradient_checkpointing="unsloth",
    random_state=3407,
    max_seq_length=MAX_SEQ_LENGTH,
    target_modules=[
        "q_proj", "k_proj", "v_proj", "o_proj",
        "gate_proj", "up_proj", "down_proj",
    ],
)

raw = load_dataset(
    "json",
    data_files={
        "train": "data/qwen35_train.jsonl",
        "validation": "data/qwen35_valid.jsonl",
    },
)

def to_text(example):
    messages = [
        {"role": "system", "content": example["system"]},
        {"role": "user", "content": example["user"]},
        {"role": "assistant", "content": example["assistant"]},
    ]
    return {
        "text": tokenizer.apply_chat_template(
            messages,
            tokenize=False,
            add_generation_prompt=False,
        )
    }

dataset = raw.map(to_text, remove_columns=raw["train"].column_names)

trainer = SFTTrainer(
    model=model,
    tokenizer=tokenizer,
    train_dataset=dataset["train"],
    eval_dataset=dataset["validation"],
    dataset_text_field="text",
    args=SFTConfig(
        output_dir="runs/qwen35-4b-unsloth-lora",
        max_seq_length=MAX_SEQ_LENGTH,
        per_device_train_batch_size=1,
        gradient_accumulation_steps=8,
        learning_rate=2e-4,
        warmup_ratio=0.03,
        num_train_epochs=1,
        logging_steps=10,
        eval_steps=100,
        save_steps=100,
        bf16=torch.cuda.is_bf16_supported(),
        fp16=not torch.cuda.is_bf16_supported(),
        optim="adamw_8bit",
        report_to="none",
    ),
)

trainer.train()
trainer.save_model("artifacts/qwen35-4b-unsloth-lora")
tokenizer.save_pretrained("artifacts/qwen35-4b-unsloth-lora")
```

For a larger run, sweep one variable at a time:

| Variable | Starting value | Sweep |
|----------|----------------|-------|
| LoRA rank | 16 | 8, 16, 32 |
| Sequence length | 2048 | 1024, 2048, 4096 |
| Effective batch | 8 | 8, 16, 32 |
| Learning rate | 2e-4 | 1e-4, 2e-4, 5e-5 |

Do not compare runs unless dataset split, seed, sequence length, and eval prompts are fixed.

---

</details>

## 4. 导出前先评估

用同一组留出 prompt 分别跑：

1. `Qwen/Qwen3.5-4B-Base`
2. base model + LoRA adapter
3. 合并后的模型（若做了合并）
4. 量化后的产物（若做了量化）

既要测质量，也要测系统行为：

| 指标 | 为什么重要 |
|--------|----------------|
| 验证损失 | 快速健全性检查，不能作为最终结论 |
| 严格格式通过率 | 捕获 chat/template/schema 回退 |
| 人工或 LLM 评判胜率 | 衡量任务上的提升 |
| 幻觉 / 无依据断言率 | 对硬件文档和手册很重要 |
| 训练峰值 VRAM | 决定本地硬件是否可行 |
| 训练 tokens/s | 反映 loader、检查点和 GPU 效率 |
| 推理 tok/s 和 TTFT | 决定推理服务成本 |
| 适配器大小 | 决定 OTA/更新的可行性 |

最小评估表：

| 模型产物 | 通过率 | 相对基座的胜率 | TTFT | decode tok/s | 备注 |
|----------------|-----------|------------------|------|--------------|-------|
| 基座 | | | | | |
| LoRA | | | | | |
| 合并后 | | | | | |
| 量化后 | | | | | |

---

## 5. 合并、导出与部署

先保存适配器。只有适配器通过评估之后才做合并。

```python
from unsloth import FastLanguageModel

model, tokenizer = FastLanguageModel.from_pretrained(
    model_name="artifacts/qwen35-4b-unsloth-lora",
    max_seq_length=2048,
    load_in_4bit=False,
)

model.save_pretrained_merged(
    "artifacts/qwen35-4b-merged-16bit",
    tokenizer,
    save_method="merged_16bit",
)
```

部署时，选择与 runtime 匹配的产物：

| Runtime 路径 | 产物 | 需要检查什么 |
|--------------|----------|---------------|
| Transformers / PEFT | base + LoRA adapter | 适配器加载延迟和额外的矩阵乘开销 |
| vLLM / TensorRT-LLM | 合并后的模型 | 对 Qwen3.5 的版本支持、tokenizer、RoPE、QKV bias 以及支持的架构 |
| llama.cpp / GGUF | 量化后的 GGUF | 量化后质量、prompt 模板和 tok/s |
| Jetson 边缘 demo | 小型量化产物 | RAM 压力、热管理和持续 decode 速度 |

如果在微调之后做量化，要重新评估。一次能提升 BF16 质量的微调，在激进量化之后仍可能退化。

---

## 6. 硬件笔记模板

每个学生最终都应产出一页纸的硬件笔记：

```markdown
# Qwen3.5-4B Unsloth Fine-Tuning Hardware Note

## Training setup
- GPU:
- VRAM:
- CUDA / PyTorch / Unsloth:
- Sequence length:
- LoRA rank:
- Effective batch:

## Results
- Peak train VRAM:
- Train tokens/s:
- Final train loss:
- Validation loss:
- Adapter size:
- Merged model size:

## Inference
- Runtime:
- Precision / quantization:
- Prompt length:
- TTFT:
- Decode tok/s:
- Peak memory:

## Interpretation
- What changed versus the base model?
- Did the adapter create measurable inference overhead?
- What precision is the best deployment compromise?
- What would this imply for SRAM, memory bandwidth, and kernel fusion on a custom accelerator?
```

最后四个问题是从「我微调了一个模型」通向「我理解这个工作负载在硬件上要付出什么代价」的桥梁。

---

## 常见失效模式

| 症状 | 可能原因 | 修复 |
|---------|--------------|-----|
| 训练一开始就 OOM | sequence length 或 batch 过大 | 降低 sequence length、使用梯度检查点、减小 batch |
| 损失下降但评估变差 | 数据质量差、记忆化、评估模板错误 | 检查样本、去重、修正模板 |
| 模型重复输出角色标签 | EOS/chat template 不匹配 | 校验 tokenizer 模板和 assistant 结束标记 |
| 导出的模型与适配器不一致 | merge/export 流程有 bug | 量化前对比 base+adapter 与合并后的模型 |
| 量化产物丢失任务技能 | 量化过于激进 | 尝试更高 bit 的量化，或豁免敏感张量 |
| runtime 在 Qwen 权重上崩溃 | 架构支持存在缺口 | 校验 QKV bias、RoPE 布局、tokenizer 和模型配置支持 |

---

## 结课项目

在一个小规模硬件工程指令数据集上微调 `Qwen/Qwen3.5-4B-Base`，然后通过两条推理路径部署结果：

1. Transformers 中的 base + LoRA adapter
2. 推理 runtime 中的合并后或量化后产物

交付：

- `data_card.md`，说明来源、过滤规则、划分和模板
- `train_config.yaml` 或等价的脚本参数
- 保存好的 LoRA 适配器
- 基座与微调后模型的对比评估报告
- 推理 benchmark 表
- 把结果与内存、精度和推理服务成本联系起来的硬件笔记

---


<details>
<summary>English original</summary>

**4. Evaluate Before You Export**

Run the same held-out prompts against:

1. `Qwen/Qwen3.5-4B-Base`
2. base model + LoRA adapter
3. merged model, if you merge
4. quantized artifact, if you quantize

Measure both quality and systems behavior:

| Metric | Why it matters |
|--------|----------------|
| Validation loss | Fast sanity check, not final proof |
| Exact-format pass rate | Catches chat/template/schema regressions |
| Human or LLM-judge win rate | Measures task improvement |
| Hallucination / unsupported-claim rate | Important for hardware docs and manuals |
| Peak VRAM during train | Determines feasible local hardware |
| Train tokens/s | Captures loader, checkpointing, and GPU efficiency |
| Inference tok/s and TTFT | Determines serving cost |
| Adapter size | Determines OTA/update feasibility |

Minimal eval table:

| Model artifact | Pass rate | Win rate vs base | TTFT | Decode tok/s | Notes |
|----------------|-----------|------------------|------|--------------|-------|
| Base | | | | | |
| LoRA | | | | | |
| Merged | | | | | |
| Quantized | | | | | |

---

**5. Merge, Export, and Deploy**

Save the adapter first. Merge only after the adapter passes evaluation.

```python
from unsloth import FastLanguageModel

model, tokenizer = FastLanguageModel.from_pretrained(
    model_name="artifacts/qwen35-4b-unsloth-lora",
    max_seq_length=2048,
    load_in_4bit=False,
)

model.save_pretrained_merged(
    "artifacts/qwen35-4b-merged-16bit",
    tokenizer,
    save_method="merged_16bit",
)
```

For deployment, choose the artifact that matches the runtime:

| Runtime path | Artifact | What to check |
|--------------|----------|---------------|
| Transformers / PEFT | base + LoRA adapter | adapter load latency and extra matmul overhead |
| vLLM / TensorRT-LLM | merged model | version support for Qwen3.5, tokenizer, RoPE, QKV bias, and supported architecture |
| llama.cpp / GGUF | quantized GGUF | post-quant quality, prompt template, and tok/s |
| Jetson edge demo | small quantized artifact | RAM pressure, thermals, and sustained decode speed |

If you quantize after fine-tuning, evaluate again. A fine-tune that improves BF16 quality can still regress after aggressive quantization.

---

**6. Hardware Note Template**

Every student should finish with a one-page hardware note:

```markdown
# Qwen3.5-4B Unsloth Fine-Tuning Hardware Note

## Training setup
- GPU:
- VRAM:
- CUDA / PyTorch / Unsloth:
- Sequence length:
- LoRA rank:
- Effective batch:

## Results
- Peak train VRAM:
- Train tokens/s:
- Final train loss:
- Validation loss:
- Adapter size:
- Merged model size:

## Inference
- Runtime:
- Precision / quantization:
- Prompt length:
- TTFT:
- Decode tok/s:
- Peak memory:

## Interpretation
- What changed versus the base model?
- Did the adapter create measurable inference overhead?
- What precision is the best deployment compromise?
- What would this imply for SRAM, memory bandwidth, and kernel fusion on a custom accelerator?
```

The last four questions are the bridge from "I fine-tuned a model" to "I understand what this workload costs in hardware."

---

**Common Failure Modes**

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| Training OOMs immediately | sequence length or batch too high | lower sequence length, use gradient checkpointing, reduce batch |
| Loss falls but eval is worse | bad data, memorization, wrong eval template | inspect examples, deduplicate, fix template |
| Model repeats role tags | EOS/chat-template mismatch | verify tokenizer template and assistant termination |
| Exported model differs from adapter | merge/export path bug | compare base+adapter vs merged before quantization |
| Quantized artifact loses task skill | quantization too aggressive | try higher-bit quant or exempt sensitive tensors |
| Runtime crashes on Qwen weights | architecture support gap | verify QKV bias, RoPE layout, tokenizer, and model config support |

---

**Capstone**

Fine-tune `Qwen/Qwen3.5-4B-Base` on a small hardware-engineering instruction dataset, then deploy the result through two inference paths:

1. base + LoRA adapter in Transformers
2. merged or quantized artifact in an inference runtime

Deliver:

- `data_card.md` describing source, filters, split, and template
- `train_config.yaml` or equivalent script arguments
- saved LoRA adapter
- base-vs-finetuned evaluation report
- inference benchmark table
- hardware note connecting the results to memory, precision, and serving cost

---

</details>

## 资源

| 资源 | 重要性 |
|----------|----------------|
| [Qwen/Qwen3.5-4B-Base 模型卡](https://huggingface.co/Qwen/Qwen3.5-4B-Base) | 源模型、配置、许可证与使用说明 |
| [Unsloth Qwen3.5 微调指南](https://unsloth.ai/docs/models/qwen3.5/fine-tune) | 当前 Unsloth 支持情况、推荐精度与训练示例 |
| [TRL SFTTrainer 文档](https://huggingface.co/docs/trl/sft_trainer) | 用于监督式微调的 Trainer API |
| [PEFT LoRA 文档](https://huggingface.co/docs/peft/main/en/conceptual_guides/lora) | 适配器机制与可调参数 |
| [阶段 5 - Qwen 推理优化](../../../../阶段5-高级专题与专精/03-方向C-边缘AI/05-Qwen推理优化/README.md) | 配套的推理课程，用于对导出的模型做性能剖析 |

---

## 下一步

-> 待微调产物可用于 benchmark 后，进入 [阶段 5 - Qwen 推理优化](../../../../阶段5-高级专题与专精/03-方向C-边缘AI/05-Qwen推理优化/README.md)。


<details>
<summary>English original</summary>

**Resources**

| Resource | Why it matters |
|----------|----------------|
| [Qwen/Qwen3.5-4B-Base model card](https://huggingface.co/Qwen/Qwen3.5-4B-Base) | Source model, config, license, and usage notes |
| [Unsloth Qwen3.5 fine-tuning guide](https://unsloth.ai/docs/models/qwen3.5/fine-tune) | Current Unsloth support, recommended precision, and training examples |
| [TRL SFTTrainer documentation](https://huggingface.co/docs/trl/sft_trainer) | Trainer API used for supervised fine-tuning |
| [PEFT LoRA documentation](https://huggingface.co/docs/peft/main/en/conceptual_guides/lora) | Adapter mechanics and tunable parameters |
| [Phase 5 - Qwen Inference Optimization](../../../../阶段5-高级专题与专精/03-方向C-边缘AI/05-Qwen推理优化/README.md) | Companion inference course for profiling the exported model |

---

**Next**

-> [Phase 5 - Qwen Inference Optimization](../../../../阶段5-高级专题与专精/03-方向C-边缘AI/05-Qwen推理优化/README.md) once the fine-tuned artifact is ready to benchmark.

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/5. LLM Application Development/Qwen3.5-4B Unsloth Fine-Tuning/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/5.%20LLM%20Application%20Development/Qwen3.5-4B%20Unsloth%20Fine-Tuning/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
