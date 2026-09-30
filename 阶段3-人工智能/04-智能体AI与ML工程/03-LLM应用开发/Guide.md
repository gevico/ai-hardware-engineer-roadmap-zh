---
title: Module 5B — LLM Application Development
description: Module 5B — LLM Application Development
---

# Module 5B — LLM Application Development

<div class="course-identity auto-course" style="--course-accent: #dc2626; --course-accent-rgb: 220, 38, 38;" markdown="1">
<div class="course-identity__icon">M5LA</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度剖析 · AI 工作负载</p>
<p class="course-identity__title">Module 5B — LLM Application Development 的专用课程标识。</p>
<p class="course-identity__meta">产物：模型或工作负载研究 · 度量：准确率、延迟、内存、吞吐</p>
</div>
</div>


**父级：** [阶段 3 — 人工智能](../../Guide.md) · 方向 B

> *交付 GenAI 产品 — 从 prompt engineering 到生产部署。*

**前置要求：** Module 3B（智能体化 AI）、Module 4B（ML Engineering）。

**目标角色：** AI Engineer · GenAI Engineer · LLM Application Developer · Full-Stack AI Engineer

---

## 这对 AI 硬件为何重要

2025–2026 年，LLM 应用是 **GPU 推理算力的最大消耗方**：
- ChatGPT 每周服务 2 亿+ 用户 → 庞大的 GPU 集群
- 企业级 RAG 部署 → GPU 加速的向量检索 + LLM 推理
- 代码助手 → 长上下文 attention、流式生成
- 理解这些模式有助于硬件工程师设计满足真实需求的芯片

---

## 1. Prompt Engineering（进阶）

* **系统提示词：** 人设、约束、输出格式规范
* **Few-shot learning：** 示例选择、动态 few-shot、chain-of-thought
* **结构化输出：** JSON 模式、function calling、工具使用 schema
* **Prompt 优化：** 迭代改进、A/B 测试 prompt、自动化评估
* **长上下文策略：** 上下文窗口管理、分块、摘要链

---

## 2. 微调 LLM

* **何时选用微调、prompt engineering 还是 RAG**
* **LoRA / QLoRA：** 参数高效微调、适配器合并
* **全量微调：** 当需要最高质量且拥有充足数据/算力时
* **数据准备：** 指令格式化、chat 模板、质量过滤
* **评估：** 困惑度、任务专属指标、人工评测、LLM-as-judge
* **配套课程：** [Qwen3.5-4B-Base Fine-Tuning with Unsloth](01-Qwen3-5-4B-Unsloth微调/Guide.md) — 16-bit LoRA SFT、适配器导出、评估和硬件成本报告

**项目：**
1. 在领域专属 Q&A 数据集上用 QLoRA 微调 Llama-3-8B。与基座模型对比评估。
2. 合并 LoRA 适配器并导出为 ONNX 以用于部署。
3. 用 Unsloth LoRA 微调 `Qwen/Qwen3.5-4B-Base`，然后 benchmark 基座模型、适配器、合并版与量化后的产物。

---

## 3. 生产级 RAG 架构

* **高级检索：** 混合检索（稠密 + BM25）、重排序（cross-encoder）、查询扩展
* **分块优化：** 递归切分、语义分块、父子检索
* **多模态 RAG：** 图像 + 文本、文档版面理解
* **评估框架：** RAGAS、上下文精确率/召回率、忠实度评分
* **扩展：** 分布式向量存储、缓存、embedding 批处理

**项目：**
1. 构建一套带混合检索 + 重排序的生产级 RAG 系统。用 RAGAS 评估。
2. 加入引用追踪 — 每一条生成的论断都链接到源分块。

---

## 4. 生产部署

* **API 设计：** 流式响应、结构化输出、错误处理
* **扩展模式：** 负载均衡、自动扩缩容、GPU 规格匹配
* **成本优化：** 缓存（语义缓存、精确缓存）、模型路由（小模型 → 大模型）、prompt 压缩
* **可观测性：** token 用量追踪、延迟监控、质量评分
* **安全：** 内容过滤、PII 检测、输出校验、限流

**项目：**
1. 部署一套带流式、缓存与成本追踪的 RAG 应用。度量 tokens/$ 效率。
2. 实现语义缓存 — 缓存相似查询，将 GPU 推理调用减少 30%+。

---

## 5. 安全、审核与 Prompt 防护

如果交付 LLM 产品，这就是产品的一部分，而非可选的附加项。

你需要针对以下内容的控制：
- 有害或非法请求
- jailbreak 与 prompt 注入尝试
- 不安全的检索上下文与恶意文档
- prompt 或响应中的 PII 泄露
- 应用专属的禁止话题，例如受监管的建议、政治说服，或政策要求时的内部专属主题

### 5.1 纵深防御架构

```text
User input
  -> normalize and sanitize text
  -> blocklist / denied-topic checks
  -> moderation + jailbreak / prompt-injection detection
  -> retrieval or tool access with scoped permissions
  -> model with hardened system prompt
  -> output moderation + schema validation + PII redaction
  -> logs, review queue, metrics, threshold tuning
```

系统提示词有帮助，但不能替代输入过滤、工具约束和输出校验。


<details>
<summary>English original</summary>

**Module 5B — LLM Application Development**

<div class="course-identity auto-course" style="--course-accent: #dc2626; --course-accent-rgb: 220, 38, 38;" markdown="1">
<div class="course-identity__icon">M5LA</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · AI Workloads</p>
<p class="course-identity__title">Specialized course identity for Module 5B — LLM Application Development.</p>
<p class="course-identity__meta">Artifact: model or workload study · Measure: accuracy, latency, memory, throughput</p>
</div>
</div>


**Parent:** [Phase 3 — Artificial Intelligence](../../Guide.md) · Track B

> *Ship GenAI products — from prompt engineering to production deployment.*

**Prerequisites:** Module 3B (Agentic AI), Module 4B (ML Engineering).

**Role targets:** AI Engineer · GenAI Engineer · LLM Application Developer · Full-Stack AI Engineer

---

**Why This Matters for AI Hardware**

LLM applications are the **largest consumer of GPU inference capacity** in 2025–2026:
- ChatGPT serves 200M+ weekly users → massive GPU fleet
- Enterprise RAG deployments → GPU-accelerated vector search + LLM inference
- Code assistants → long-context attention, streaming generation
- Understanding these patterns helps hardware engineers design chips that serve real demand

---

**1. Prompt Engineering (Advanced)**

* **System prompts:** persona, constraints, output format specification
* **Few-shot learning:** example selection, dynamic few-shot, chain-of-thought
* **Structured output:** JSON mode, function calling, tool use schemas
* **Prompt optimization:** iterative refinement, A/B testing prompts, automated evaluation
* **Long-context strategies:** context window management, chunking, summarization chains

---

**2. Fine-Tuning LLMs**

* **When to fine-tune vs prompt engineering vs RAG**
* **LoRA / QLoRA:** parameter-efficient fine-tuning, adapter merging
* **Full fine-tuning:** when you need maximum quality and have enough data/compute
* **Data preparation:** instruction formatting, chat templates, quality filtering
* **Evaluation:** perplexity, task-specific metrics, human eval, LLM-as-judge
* **Dedicated course:** [Qwen3.5-4B-Base Fine-Tuning with Unsloth](01-Qwen3-5-4B-Unsloth微调/Guide.md) — 16-bit LoRA SFT, adapter export, evaluation, and hardware-cost reporting

**Projects:**
1. Fine-tune Llama-3-8B with QLoRA on a domain-specific Q&A dataset. Evaluate vs base model.
2. Merge LoRA adapters and export to ONNX for deployment.
3. Fine-tune `Qwen/Qwen3.5-4B-Base` with Unsloth LoRA, then benchmark base, adapter, merged, and quantized artifacts.

---

**3. Production RAG Architecture**

* **Advanced retrieval:** hybrid search (dense + BM25), re-ranking (cross-encoder), query expansion
* **Chunking optimization:** recursive splitting, semantic chunking, parent-child retrieval
* **Multi-modal RAG:** images + text, document layout understanding
* **Evaluation framework:** RAGAS, context precision/recall, faithfulness scoring
* **Scaling:** distributed vector stores, caching, embedding batch processing

**Projects:**
1. Build a production RAG system with hybrid retrieval + re-ranking. Evaluate with RAGAS.
2. Add citation tracking — every generated claim linked to source chunks.

---

**4. Production Deployment**

* **API design:** streaming responses, structured output, error handling
* **Scaling patterns:** load balancing, auto-scaling, GPU right-sizing
* **Cost optimization:** caching (semantic cache, exact cache), model routing (small → large), prompt compression
* **Observability:** token usage tracking, latency monitoring, quality scoring
* **Safety:** content filtering, PII detection, output validation, rate limiting

**Projects:**
1. Deploy a RAG application with streaming, caching, and cost tracking. Measure tokens/$ efficiency.
2. Implement semantic caching — cache similar queries to reduce GPU inference calls by 30%+.

---

**5. Safety, Moderation, and Prompt Security**

If you ship an LLM product, this is part of the product, not an optional add-on.

You need controls for:
- harmful or illegal requests
- jailbreaks and prompt-injection attempts
- unsafe retrieval context and hostile documents
- PII leakage in prompts or responses
- application-specific denied topics such as regulated advice, political persuasion, or internal-only subjects when policy requires it

**5.1 Defense-in-Depth Architecture**

```text
User input
  -> normalize and sanitize text
  -> blocklist / denied-topic checks
  -> moderation + jailbreak / prompt-injection detection
  -> retrieval or tool access with scoped permissions
  -> model with hardened system prompt
  -> output moderation + schema validation + PII redaction
  -> logs, review queue, metrics, threshold tuning
```

System prompts help, but they do not replace input filtering, tool constraints, and output validation.

</details>

### 5.2 模型前控制

* **输入归一化：** 在策略检查前剥离或归一化隐藏 Unicode、同形字技巧和畸形编码。
* **关键词与模式过滤：** 用快速的第一遍检查捕获明显的有害词、拒绝话题、密钥或策略敏感短语。
* **基于分类器的审核：** 运行文本分类器或托管审核 API，对毒性、仇恨、自残、色情、暴力或违法内容打分。
* **提示词注入检测：** 对越狱尝试分类，如角色扮演覆盖、「忽略之前的指令」或恶意文档载荷。
* **文档筛查：** 对 RAG，把上传文件、网页和检索到的分块视为不可信输入。在它们进入提示词之前先扫描。

### 5.3 模型内控制

* **系统提示词加固：** 明确角色、范围、拒绝行为和工具使用限制。
* **受限工具使用：** 白名单化工具、校验参数、限定凭证范围，使模型无法通过工具提权。
* **结构化输出：** 对触发下游系统的动作强制 JSON 或 schema 约束输出。
* **生成限制：** 限制输出长度、工具调用次数和递归深度，以减少滥用与失控成本。

### 5.4 模型后控制

* **输出审核：** 在返回给用户之前重新扫描补全内容。
* **PII 检测与脱敏：** 从输出和日志中清除邮箱、电话号码、账号 ID、密钥或受监管标识符。
* **接地与验证：** 对 RAG，在把事实性陈述当作可信内容呈现之前，要求引用或证据核查。
* **安全回退行为：** 将被拦截的输出替换为拒绝或升级路径，而不是暴露原始的不安全文本。

### 5.5 运维与评估

* **记录被拦截和边缘的提示词：** 需要样本来做策略调优和事件复盘。
* **度量误报和漏报：** 过严的策略会破坏合法工作流。
* **持续红队测试：** 攻击者会不断适应。测试越狱、编码提示词、多语言攻击和间接提示词注入。
* **版本化策略：** 阈值、黑名单、正则规则和系统提示词应像代码一样版本化。

### 5.6 最小 Python 示例

```python
import re

BLOCKED_TOPICS = ["bomb", "kill", "credit card dump"]
JAILBREAK_PATTERNS = [
    r"ignore previous instructions",
    r"you are dan",
    r"pretend to be",
]

def sanitize_unicode(text: str) -> str:
    return "".join(c for c in text if not (0xE0000 <= ord(c) <= 0xE007F))

def keyword_block(text: str) -> bool:
    lowered = text.lower()
    return not any(term in lowered for term in BLOCKED_TOPICS)

def detect_jailbreak(text: str) -> bool:
    return any(re.search(pattern, text, re.IGNORECASE) for pattern in JAILBREAK_PATTERNS)

def secure_prompt_pipeline(user_input: str) -> str:
    clean = sanitize_unicode(user_input)

    if not keyword_block(clean):
        return "Blocked: denied topic detected."

    if detect_jailbreak(clean):
        return "Blocked: prompt attack detected."

    # Then call your moderation service, model, output moderation,
    # and PII redaction steps.
    return "Safe to continue."
```

这个例子的重点在流水线形态，而不在具体的分类器选择。在生产环境，用真实的审核服务、提示词攻击检测器和 PII 工具替换这些占位符。

### 5.7 应当了解的工具

| Layer | Tool / Service | What it is useful for |
|------|-----------------|-----------------------|
| Input / output moderation | [OpenAI Moderation](https://platform.openai.com/docs/guides/moderation) | Managed text and image moderation for harmful content classification |
| Prompt-attack detection | [Azure AI Content Safety Prompt Shields](https://learn.microsoft.com/en-us/azure/ai-services/content-safety/quickstart-jailbreak) | Detects user prompt attacks and document attacks before generation |
| AI firewall / policy gateway | [Google Cloud Model Armor](https://docs.cloud.google.com/model-armor/overview) | Screens prompts and responses for prompt injection, harmful content, sensitive data, and malicious URLs |
| Managed guardrails | [Amazon Bedrock Guardrails](https://docs.aws.amazon.com/bedrock/latest/userguide/guardrails.html) | Content filters, denied topics, word filters, sensitive information filters, and prompt-attack detection |
| PII redaction | [Microsoft Presidio](https://microsoft.github.io/presidio/) | Detect and anonymize sensitive data in text and images |
| Programmable guardrails | [NVIDIA NeMo Guardrails](https://docs.nvidia.com/nemo-guardrails/index.html) | Open-source guardrail flows for input, output, retrieval, and security checks |
| Open-source prompt / safety classifiers | [Meta Llama Guard and Prompt Guard](https://huggingface.co/meta-llama) | Self-hosted safety and prompt-attack classifiers when you need local or customizable controls |


<details>
<summary>English original</summary>

**5.2 Pre-Model Controls**

* **Input normalization:** strip or normalize hidden Unicode, homoglyph tricks, and malformed encodings before policy checks.
* **Keyword and pattern filters:** use fast first-pass checks for obvious harmful terms, denied topics, secrets, or policy-sensitive phrases.
* **Classifier-based moderation:** run a text classifier or managed moderation API to score toxicity, hate, self-harm, sexual, violence, or illicit content.
* **Prompt-injection detection:** classify jailbreak attempts such as role-play overrides, "ignore previous instructions", or hostile document payloads.
* **Document screening:** for RAG, treat uploaded files, webpages, and retrieved chunks as untrusted input. Scan them before they enter the prompt.

**5.3 In-Model Controls**

* **System prompt hardening:** define role, scope, refusal behavior, and tool-use limits clearly.
* **Constrained tool use:** whitelist tools, validate arguments, and scope credentials so the model cannot escalate through tools.
* **Structured outputs:** force JSON or schema-constrained outputs for actions that trigger downstream systems.
* **Generation limits:** cap output length, tool calls, and recursion depth to reduce abuse and runaway cost.

**5.4 Post-Model Controls**

* **Output moderation:** rescan completions before returning them to the user.
* **PII detection and redaction:** scrub emails, phone numbers, account IDs, secrets, or regulated identifiers from outputs and logs.
* **Grounding and validation:** for RAG, require citations or evidence checks before presenting factual claims as trusted.
* **Safe fallback behavior:** replace blocked outputs with a refusal or escalation path instead of exposing raw unsafe text.

**5.5 Operations and Evaluation**

* **Log blocked and borderline prompts:** you need examples for policy tuning and incident review.
* **Measure false positives and false negatives:** strict policies can break legitimate workflows.
* **Red-team continuously:** adversaries adapt. Test jailbreaks, encoded prompts, multilingual attacks, and indirect prompt injection.
* **Version policies:** thresholds, blocklists, regex rules, and system prompts should be versioned like code.

**5.6 Minimal Python Sketch**

```python
import re

BLOCKED_TOPICS = ["bomb", "kill", "credit card dump"]
JAILBREAK_PATTERNS = [
    r"ignore previous instructions",
    r"you are dan",
    r"pretend to be",
]

def sanitize_unicode(text: str) -> str:
    return "".join(c for c in text if not (0xE0000 <= ord(c) <= 0xE007F))

def keyword_block(text: str) -> bool:
    lowered = text.lower()
    return not any(term in lowered for term in BLOCKED_TOPICS)

def detect_jailbreak(text: str) -> bool:
    return any(re.search(pattern, text, re.IGNORECASE) for pattern in JAILBREAK_PATTERNS)

def secure_prompt_pipeline(user_input: str) -> str:
    clean = sanitize_unicode(user_input)

    if not keyword_block(clean):
        return "Blocked: denied topic detected."

    if detect_jailbreak(clean):
        return "Blocked: prompt attack detected."

    # Then call your moderation service, model, output moderation,
    # and PII redaction steps.
    return "Safe to continue."
```

The point of the example is the pipeline shape, not the exact classifier choice. In production, replace the placeholders with real moderation services, prompt-attack detectors, and PII tooling.

**5.7 Tools You Should Know**

| Layer | Tool / Service | What it is useful for |
|------|-----------------|-----------------------|
| Input / output moderation | [OpenAI Moderation](https://platform.openai.com/docs/guides/moderation) | Managed text and image moderation for harmful content classification |
| Prompt-attack detection | [Azure AI Content Safety Prompt Shields](https://learn.microsoft.com/en-us/azure/ai-services/content-safety/quickstart-jailbreak) | Detects user prompt attacks and document attacks before generation |
| AI firewall / policy gateway | [Google Cloud Model Armor](https://docs.cloud.google.com/model-armor/overview) | Screens prompts and responses for prompt injection, harmful content, sensitive data, and malicious URLs |
| Managed guardrails | [Amazon Bedrock Guardrails](https://docs.aws.amazon.com/bedrock/latest/userguide/guardrails.html) | Content filters, denied topics, word filters, sensitive information filters, and prompt-attack detection |
| PII redaction | [Microsoft Presidio](https://microsoft.github.io/presidio/) | Detect and anonymize sensitive data in text and images |
| Programmable guardrails | [NVIDIA NeMo Guardrails](https://docs.nvidia.com/nemo-guardrails/index.html) | Open-source guardrail flows for input, output, retrieval, and security checks |
| Open-source prompt / safety classifiers | [Meta Llama Guard and Prompt Guard](https://huggingface.co/meta-llama) | Self-hosted safety and prompt-attack classifiers when you need local or customizable controls |

</details>

### 5.8 项目

1. 构建安全聊天或 RAG（检索增强生成）网关：Unicode 规范化 -> 审核 -> 越狱检测 -> 模型调用 -> 输出审核 -> PII 脱敏。
2. 在 prompt 语料上评估越狱防御栈。测量拦截率、误报与新增延迟。
3. 在同一个工作负载上比较托管式与自托管护栏：OpenAI 或 Azure 或 Bedrock 对比 NeMo Guardrails 或 Llama Guard。
4. 为 RAG 上传文档与检索到的 chunk 添加文档筛查，然后测试间接 prompt 注入用例。

---

## 与硬件的关联

| 应用模式 | 硬件影响 |
|--------------------|---------------------|
| 长上下文 attention（128K tokens） | HBM 带宽、KV-cache 内存 |
| 流式 token 生成 | 低延迟 kernel 调度 |
| 批推理服务 | in-flight 批处理、GPU 利用率 |
| 向量搜索（RAG 检索） | GPU 上的 cuVS / FAISS |
| 多模型路由 | 多 GPU 调度、MIG 分区 |
| 审核边车与 guard 模型 | 额外延迟、内存占用与部署拓扑选择 |
| Jetson 或边缘 NPU 上的端侧 prompt 安全 | 小型分类器选择、量化与 CPU/GPU 划分 |

---

## 资源

| 资源 | 涵盖内容 |
|----------|---------------|
| [Maxime Labonne's LLM Course](https://github.com/mlabonne/llm-course) | 免费的 LLM 路线图与 Colab notebook，覆盖 LLM 基础、LLM Scientist 路径与 LLM Engineer 路径：微调、量化、RAG、评估与部署 |
| [使用 Unsloth 微调 Qwen3.5-4B-Base](01-Qwen3-5-4B-Unsloth微调/Guide.md) | 在 `Qwen/Qwen3.5-4B-Base` 上进行 LoRA SFT 的路线图课程，含导出、评估与硬件测量 |
| [Anthropic API 文档](https://docs.anthropic.com/) | Claude API、工具使用、流 |
| [OpenAI Cookbook](https://cookbook.openai.com/) | GPT API 模式与最佳实践 |
| [LlamaIndex](https://docs.llamaindex.ai/) | RAG 框架 |
| [RAGAS](https://docs.ragas.io/) | RAG 评估框架 |
| [Azure AI Content Safety](https://learn.microsoft.com/en-us/azure/ai-services/content-safety/overview) | 托管式内容审核与可配置安全控制 |
| [Google Cloud Model Armor](https://docs.cloud.google.com/model-armor/overview) | prompt / 响应筛查、敏感数据保护、恶意 URL 检测 |
| [Amazon Bedrock Guardrails](https://docs.aws.amazon.com/bedrock/latest/userguide/guardrails.html) | 托管式内容、主题与 PII 防护 |
| [Microsoft Presidio](https://microsoft.github.io/presidio/) | PII 检测与匿名化 |
| [NVIDIA NeMo Guardrails](https://docs.nvidia.com/nemo-guardrails/index.html) | 可编程 LLM 护栏 |
| *Building LLM Applications*（多种） | 端到端 LLM 应用开发 |


<details>
<summary>English original</summary>

**5.8 Projects**

1. Build a secure chat or RAG gateway: Unicode normalization -> moderation -> jailbreak detection -> model call -> output moderation -> PII redaction.
2. Evaluate a jailbreak defense stack on a prompt corpus. Measure block rate, false positives, and added latency.
3. Compare managed vs self-hosted guardrails on one workload: OpenAI or Azure or Bedrock vs NeMo Guardrails or Llama Guard.
4. Add document screening for RAG uploads and retrieved chunks, then test indirect prompt injection cases.

---

**Connection to Hardware**

| Application pattern | Hardware implication |
|--------------------|---------------------|
| Long-context attention (128K tokens) | HBM bandwidth, KV-cache memory |
| Streaming token generation | Low-latency kernel scheduling |
| Batch inference serving | In-flight batching, GPU utilization |
| Vector search (RAG retrieval) | cuVS / FAISS on GPU |
| Multi-model routing | Multi-GPU scheduling, MIG partitioning |
| Moderation sidecars and guard models | Extra latency, memory footprint, and deployment topology choices |
| On-device prompt security on Jetson or edge NPUs | Small classifier selection, quantization, and CPU/GPU partitioning |

---

**Resources**

| Resource | What it covers |
|----------|---------------|
| [Maxime Labonne's LLM Course](https://github.com/mlabonne/llm-course) | Free LLM roadmap and Colab notebooks covering LLM fundamentals, the LLM Scientist path, and the LLM Engineer path: fine-tuning, quantization, RAG, evaluation, and deployment |
| [Qwen3.5-4B-Base Fine-Tuning with Unsloth](01-Qwen3-5-4B-Unsloth微调/Guide.md) | Roadmap course for LoRA SFT on `Qwen/Qwen3.5-4B-Base`, export, evaluation, and hardware measurements |
| [Anthropic API Documentation](https://docs.anthropic.com/) | Claude API, tool use, streaming |
| [OpenAI Cookbook](https://cookbook.openai.com/) | GPT API patterns and best practices |
| [LlamaIndex](https://docs.llamaindex.ai/) | RAG framework |
| [RAGAS](https://docs.ragas.io/) | RAG evaluation framework |
| [Azure AI Content Safety](https://learn.microsoft.com/en-us/azure/ai-services/content-safety/overview) | Managed content moderation and configurable safety controls |
| [Google Cloud Model Armor](https://docs.cloud.google.com/model-armor/overview) | Prompt / response screening, sensitive data protection, malicious URL detection |
| [Amazon Bedrock Guardrails](https://docs.aws.amazon.com/bedrock/latest/userguide/guardrails.html) | Managed content, topic, and PII safeguards |
| [Microsoft Presidio](https://microsoft.github.io/presidio/) | PII detection and anonymization |
| [NVIDIA NeMo Guardrails](https://docs.nvidia.com/nemo-guardrails/index.html) | Programmable LLM guardrails |
| *Building LLM Applications* (various) | End-to-end LLM app development |

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/5. LLM Application Development/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/5.%20LLM%20Application%20Development/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
