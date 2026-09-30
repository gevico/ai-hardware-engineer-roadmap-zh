---
title: AI 智能体开发 2026 —— 讲座索引
description: AI 智能体开发 2026 —— 讲座索引
---

# AI 智能体开发 2026 —— 讲座索引

一个动手实践的讲座系列，从现代 agent harness（agent 运行时框架）起步，经由基础知识与 agent 的核心组成部分，直到一个真实的 harness 案例研究（OpenClaw）和一个 capstone 构建（**genie-claw**）。

> 这是全部讲座的 **扁平编号索引**。按模块分组的推荐 **理论优先阅读顺序**，见 **[课程 Guide](../Guide.md)**。

> **★ 配套深度课程：** [**MCP for AI Agents**](../02-AI智能体MCP/README.md) —— 一门 8 讲课程，讲解 **Model Context Protocol**，即 agent 用来访问工具和数据的开放标准。它直接建立在 [Tool Use & Function Calling](Lecture-08.md) 之上：架构与六个原语、构建并保护真实服务器、用于远程部署的 OAuth 2.1，以及生产技术栈 —— 内容更新至 2025-11-25 规范与 2026 发布候选版。

## 2026 年如何阅读本课程

模型名称、上下文窗口、SDK 功能和 token 价格变化很快。把厂商特定的示例视为实现快照，而非长期推荐。

本课程中耐久有效的概念有：

- **Model API：** 直接的文本、结构化输出、工具调用和流式接口。
- **Agent runtime：** 管理轮次、工具、会话、handoff、护栏和 trace 的循环。
- **工具协议：** 外部系统暴露的 MCP 风格 tools、resources 和 prompts。
- **工作流控制：** 图、检查点、重试、人工审核和确定性启动。
- **产品控制平面：** 网关、渠道、会话、布线、身份和审计日志。
- **Runtime 安全：** 最小权限、策略门控、遥测、事件响应和证据。

讲座按推荐阅读顺序编号为 01–42（第 42 讲是关于机密与可验证 agent 的高级安全 capstone）。它们分为六个模块 —— *从这开始*（现代 agent、harness，以及如何构建一个）、*基础知识*、*核心构建块*、*生产与 runtime*、*OpenClaw* 示例，以及 *genie-claw* 实践 capstone。完整模块划分见 **[课程 Guide](../Guide.md)**。


<details>
<summary>English original</summary>

**AI Agent Development 2026 — Lecture Index**

A hands-on lecture series building from the modern agent harness, through fundamentals and the core parts of an agent, to a real harness case study (OpenClaw) and a capstone build (**genie-claw**).

> This is the **flat, numbered index** of all lectures. For the recommended **theory-first reading order** grouped into modules, see the **[course Guide](../Guide.md)**.

> **★ Companion deep-dive:** [**MCP for AI Agents**](../02-AI智能体MCP/README.md) — an 8-lecture course on the **Model Context Protocol**, the open standard agents use to reach tools and data. It builds directly on [Tool Use & Function Calling](Lecture-08.md): architecture and the six primitives, building and securing real servers, OAuth 2.1 for remote deployment, and the production stack — current to the 2025-11-25 spec and the 2026 release candidate.

**How to Read This Course in 2026**

Model names, context windows, SDK features, and token prices change quickly. Treat vendor-specific examples as implementation snapshots, not permanent recommendations.

The durable concepts in this course are:

- **Model API:** the direct text, structured output, tool-call, and streaming interface.
- **Agent runtime:** the loop that manages turns, tools, sessions, handoffs, guardrails, and traces.
- **Tool protocol:** MCP-style tools, resources, and prompts exposed by external systems.
- **Workflow control:** graphs, checkpoints, retries, human review, and deterministic startup.
- **Product control plane:** gateways, channels, sessions, routing, identities, and audit logs.
- **Runtime security:** least privilege, policy gates, telemetry, incident response, and evidence.

The lectures are numbered 01–42 in the recommended reading order (Lecture 42 is an advanced security capstone on confidential & verifiable agents). They group into six modules — *start here* (the modern agent, the harness, and how to build one), *fundamentals*, *core building blocks*, *production & runtime*, the *OpenClaw* example, and the *genie-claw* practice capstone. See the **[course Guide](../Guide.md)** for the full module breakdown.

</details>

## Lecture Index

<div class="lecture-map" markdown>

| # | 标题 | 主题 |
|---|-------|--------|
| [Lecture 01](Lecture-01.md) | 2026 年的现代 AI 智能体：什么变了 | agent 定义、2023→2026 的转变、harness AI（agent 运行时框架）、参考系统、课程地图 |
| [Lecture 02](Lecture-02.md) | 什么是 AI 智能体 harness？模型外围的 runtime | harness 与模型之别、六项核心职责、Claude Code / Cursor / Codex 对比、硬件影响 |
| [Lecture 03](Lecture-03.md) | 构建 agent I：基础（模型、工具、指令） | 什么是 agent、何时该构建 agent，以及三大组件——模型、工具、指令 |
| [Lecture 04](Lecture-04.md) | 构建 agent II：编排与护栏 | 单 agent 与多 agent、manager 与去中心化模式、护栏类型、人在回路 |
| [Lecture 05](Lecture-05.md) | 面向 agent 的 LLM 基础 | Transformer、tokenization、推理机制、上下文窗口 |
| [Lecture 06](Lecture-06.md) | 从零实现 LLM——面向 agent 与 GPU 工程师的模型机制 | Tokenizer、Transformer 块、训练循环、推理、prefill（首字前的整段计算）、decode（逐 token 生成阶段）、GPU kernel 直觉 |
| [Lecture 07](Lecture-07.md) | 提示工程与结构化输出 | 系统提示、few-shot、JSON 模式、函数调用 |
| [Lecture 08](Lecture-08.md) | 工具使用与函数调用 | 工具 schema、并行调用、错误处理、安全 |
| [Lecture 09](Lecture-09.md) | 结构化工具胜过 Computer Use——面向 agent 的接口层级 | Reflex benchmark、结构化 API 与视觉的对比、工具 schema、验证、安全、OpenClaw 工具设计 |
| [Lecture 10](Lecture-10.md) | 记忆系统 | 短期、长期、情景、语义记忆 |
| [Lecture 11](Lecture-11.md) | RAG（检索增强生成）——数据摄入与 embedding | 分块、embedding 模型、向量库、索引 |
| [Lecture 12](Lecture-12.md) | RAG——检索与重排 | 混合搜索、MMR、cross-encoder 重排、评估 |
| [Lecture 13](Lecture-13.md) | Qdrant、pgvector 与 embedding 模型选型 | 向量库、HNSW、IVFFlat、稠密/稀疏/混合检索、Granite 替代方案、embedding 评测、迁移 |
| [Lecture 14](Lecture-14.md) | 高效的本地 RAG 栈——Qwen3.5-4B INT4 与 Granite Embeddings | Jetson RAG、Granite 97M、Qdrant、分块、重排、INT4、llama.cpp、vLLM、TensorRT-LLM、KV cache |
| [Lecture 15](Lecture-15.md) | agent 架构模式 | ReAct、CoT、Reflexion、plan-and-execute |
| [Lecture 16](Lecture-16.md) | LangGraph——有状态工作流 | 节点、边、状态、检查点、人在回路 |
| [Lecture 17](Lecture-17.md) | agent SDK 与 runtime API | SDK、provider 适配器、MCP、handoff、流式、runtime 策略 |
| [Lecture 18](Lecture-18.md) | OpenAI Agents SDK——原生沙箱与持久化 agent harness | 沙箱 agent、manifest、shell/apply_patch、MCP、技能、AGENTS.md、状态恢复、harness/compute 分离 |
| [Lecture 19](Lecture-19.md) | 多 agent 系统 | CrewAI、AutoGen、supervisor 模式、协调 |
| [Lecture 20](Lecture-20.md) | Nemotron 3 Nano Omni——多模态感知子 agent | 统一的视频/音频/图像/文本推理、混合 MoE（混合专家模型）、EVS、吞吐、OpenClaw 子 agent 架构 |
| [Lecture 21](Lecture-21.md) | agent 技能——可靠编码 agent 的工作流纪律 | 技能工作流、反合理化、验证证据、渐进式披露、范围纪律 |
| [Lecture 22](Lecture-22.md) | agent 技能评测——SKILL.md 文件的 benchmark | 带技能与基线的评测对比、LLM judge 断言、产物、CI 门禁、OpenClaw 技能回归测试 |
| [Lecture 23](Lecture-23.md) | 评估与可观测性 | LLM-as-judge、RAGAS、链路追踪、成本追踪 |
| [Lecture 24](Lecture-24.md) | runtime 纪律与 AI runtime 安全 | runtime 控制、工具策略、遥测、可审计性、agent 风险 |
| [Lecture 25](Lecture-25.md) | AI 智能体安全工程师——实践者路线图 | 8 阶段课程体系、prompt 注入的信任边界、沙箱分级、红队实践、审计日志纪律、硬件信任根 |
| [Lecture 26](Lecture-26.md) | 以会话为事实来源：事件溯源的 agent 状态 | 会话与上下文窗口的对比、事件 schema、`wake(sessionId)`、流式崩溃恢复、工具幂等性 |
| [Lecture 27](Lecture-27.md) | AI 智能体系统的确定性启动 | 启动契约、就绪门禁、工具注册表、prompt 版本、记忆水合 |
| [Lecture 28](Lecture-28.md) | agent 系统的 runtime 策略——Node、Bun、Rust 与边缘打包 | Bun 从 Zig 转向 Rust 的信号、Node 基线、Rust offload、runtime 测量、边缘打包 |
| [Lecture 29](Lecture-29.md) | 智能体化 SDLC——快速探索，安全交付 | 廉价的代码、把实现当作探索、测试即契约、演进的规格、双模式 agent |
| [Lecture 30](Lecture-30.md) | 生产部署 | 流式、缓存、模型路由、安全、扩缩容 |
| [Lecture 31](Lecture-31.md) | OpenClaw 案例研究——网关架构 | 控制平面、通道、客户端、节点、agent loop |
| [Lecture 32](Lecture-32.md) | OpenClaw 案例研究——路由与会话 | 通道路由、会话键、DM 隔离、回复确定性 |
| [Lecture 33](Lecture-33.md) | OpenClaw 案例研究——多 agent 隔离 | 工作区、状态、会话、记忆边界 |
| [Lecture 34](Lecture-34.md) | OpenClaw 案例研究——运维与安全 | 配对、监督、沙箱、工具策略、远程访问 |
| [Lecture 35](Lecture-35.md) | OpenClaw 案例研究——agent loop | 接入、队列、锁、流式、工具、hook、持久化 |
| [Lecture 36](Lecture-36.md) | OpenClaw 案例研究——Cron 与定时 agent 运行 | Cron 表达式、隔离任务、投递、重试、日志、校验 |
| [Lecture 37](Lecture-37.md) | OpenClaw 案例研究——系统提示词架构 | 提示词归属、bootstrap 上下文、技能、提示词模式、provider 覆盖层 |
| [Lecture 38](Lecture-38.md) | OpenClaw 案例研究——App SDK 自用与类型化网关 RPC | App SDK、happy path、事件归一化、未来的 RPC 接口面 |
| [Lecture 39](Lecture-39.md) | OpenClaw 案例研究——网关 RPC 协议 | WebSocket 帧、握手、角色、作用域、配对、特性、节点传输 |
| [Lecture 40](Lecture-40.md) | OpenClaw 威胁模型——面向 agent 安全的 MITRE ATLAS | 威胁矩阵、攻击链、信任边界、prompt 注入、技能供应链、工具执行控制 |
| [Lecture 41](Lecture-41.md) | Pi——极简编码 agent 与 OpenClaw 之下的底座 | 微型内核（4 个工具）、不用 MCP 的理由、会话日志中的自定义消息、热重载、树状会话、TUI 与 LLM 工具接口面的对比 |
| [Lecture 42](Lecture-42.md) | 机密且可验证的 AI 智能体（NVIDIA CC、zk-STARK、PearlChain） | TEE / 机密 GPU（H100/Blackwell）、NRAS 远程证明、zk-STARK / ZKML、TEE+ZK 混合、区块链审计、企业威胁模型 |


<details>
<summary>English original</summary>

| # | Title | Topics |
|---|-------|--------|
| [Lecture 01](Lecture-01.md) | The Modern AI Agent in 2026: What Changed | Agent definition, 2023→2026 shifts, harness AI, reference systems, course map |
| [Lecture 02](Lecture-02.md) | What Is an AI Agent Harness? The Runtime Around the Model | Harness vs model, six core responsibilities, Claude Code / Cursor / Codex compared, hardware impact |
| [Lecture 03](Lecture-03.md) | Building Agents I: Foundations (Model, Tools, Instructions) | What an agent is, when to build one, and the three components — model, tools, instructions |
| [Lecture 04](Lecture-04.md) | Building Agents II: Orchestration & Guardrails | Single vs multi-agent, manager & decentralized patterns, guardrail types, human-in-the-loop |
| [Lecture 05](Lecture-05.md) | LLM Fundamentals for Agents | Transformers, tokenization, inference mechanics, context windows |
| [Lecture 06](Lecture-06.md) | LLM From Scratch - Model Mechanics for Agent and GPU Engineers | Tokenizers, transformer blocks, training loop, inference, prefill/decode, GPU kernel intuition |
| [Lecture 07](Lecture-07.md) | Prompt Engineering & Structured Output | System prompts, few-shot, JSON mode, function calling |
| [Lecture 08](Lecture-08.md) | Tool Use & Function Calling | Tool schemas, parallel calls, error handling, safety |
| [Lecture 09](Lecture-09.md) | Structured Tools Beat Computer Use - Interface Hierarchy for Agents | Reflex benchmark, structured API vs vision, tool schemas, verification, security, OpenClaw tool design |
| [Lecture 10](Lecture-10.md) | Memory Systems | Short-term, long-term, episodic, semantic memory |
| [Lecture 11](Lecture-11.md) | RAG — Ingestion & Embeddings | Chunking, embedding models, vector stores, indexing |
| [Lecture 12](Lecture-12.md) | RAG — Retrieval & Reranking | Hybrid search, MMR, cross-encoder reranking, evaluation |
| [Lecture 13](Lecture-13.md) | Qdrant, pgvector, and Embedding Model Selection | vector stores, HNSW, IVFFlat, dense/sparse/hybrid retrieval, Granite alternatives, embedding evals, migration |
| [Lecture 14](Lecture-14.md) | Efficient Local RAG Stack - Qwen3.5-4B INT4 and Granite Embeddings | Jetson RAG, Granite 97M, Qdrant, chunking, reranking, INT4, llama.cpp, vLLM, TensorRT-LLM, KV cache |
| [Lecture 15](Lecture-15.md) | Agent Architecture Patterns | ReAct, CoT, Reflexion, plan-and-execute |
| [Lecture 16](Lecture-16.md) | LangGraph — Stateful Workflows | Nodes, edges, state, checkpointing, human-in-the-loop |
| [Lecture 17](Lecture-17.md) | Agent SDKs and Runtime APIs | SDKs, provider adapters, MCP, handoffs, streaming, runtime policy |
| [Lecture 18](Lecture-18.md) | OpenAI Agents SDK - Native Sandbox and Durable Agent Harness | sandbox agents, manifests, shell/apply_patch, MCP, skills, AGENTS.md, state recovery, harness/compute separation |
| [Lecture 19](Lecture-19.md) | Multi-Agent Systems | CrewAI, AutoGen, supervisor patterns, coordination |
| [Lecture 20](Lecture-20.md) | Nemotron 3 Nano Omni - Multimodal Perception Sub-Agents | Unified video/audio/image/text reasoning, hybrid MoE, EVS, throughput, OpenClaw sub-agent architecture |
| [Lecture 21](Lecture-21.md) | Agent Skills - Workflow Discipline for Reliable Coding Agents | Skill workflows, anti-rationalization, verification evidence, progressive disclosure, scope discipline |
| [Lecture 22](Lecture-22.md) | Agent Skills Eval - Benchmarking SKILL.md Files | with-skill vs baseline evals, LLM judge assertions, artifacts, CI gates, OpenClaw skill regression testing |
| [Lecture 23](Lecture-23.md) | Evaluation & Observability | LLM-as-judge, RAGAS, tracing, cost tracking |
| [Lecture 24](Lecture-24.md) | Runtime Discipline & AI Runtime Security | Runtime controls, tool policy, telemetry, auditability, agent risk |
| [Lecture 25](Lecture-25.md) | AI Agent Security Engineer - A Practitioner's Roadmap | 8-phase curriculum, prompt-injection trust boundaries, sandboxing tiers, red-team practice, audit log discipline, hardware-rooted trust |
| [Lecture 26](Lecture-26.md) | Session as Source of Truth: Event-Sourced Agent State | Session vs context window, event schema, `wake(sessionId)`, streaming-crash recovery, tool idempotency |
| [Lecture 27](Lecture-27.md) | Deterministic Startup for AI Agent Systems | Startup contracts, readiness gates, tool registries, prompt versions, memory hydration |
| [Lecture 28](Lecture-28.md) | Runtime Strategy for Agent Systems - Node, Bun, Rust, and Edge Packaging | Bun Zig-to-Rust signal, Node baseline, Rust offload, runtime measurements, edge packaging |
| [Lecture 29](Lecture-29.md) | Agentic SDLC - Explore Fast, Ship Safely | Cheap code, implementation as exploration, tests as contracts, evolving specs, dual-mode agents |
| [Lecture 30](Lecture-30.md) | Production Deployment | Streaming, caching, model routing, safety, scaling |
| [Lecture 31](Lecture-31.md) | OpenClaw Case Study - Gateway Architecture | Control plane, channels, clients, nodes, agent loop |
| [Lecture 32](Lecture-32.md) | OpenClaw Case Study - Routing and Sessions | Channel routing, session keys, DM isolation, reply determinism |
| [Lecture 33](Lecture-33.md) | OpenClaw Case Study - Multi-Agent Isolation | Workspaces, state, sessions, memory boundaries |
| [Lecture 34](Lecture-34.md) | OpenClaw Case Study - Operations and Security | Pairing, supervision, sandbox, tool policy, remote access |
| [Lecture 35](Lecture-35.md) | OpenClaw Case Study - The Agent Loop | Intake, queues, locks, streaming, tools, hooks, persistence |
| [Lecture 36](Lecture-36.md) | OpenClaw Case Study - Cron and Scheduled Agent Runs | Cron expressions, isolated jobs, delivery, retries, logs, validation |
| [Lecture 37](Lecture-37.md) | OpenClaw Case Study - System Prompt Architecture | Prompt ownership, bootstrap context, skills, prompt modes, provider overlays |
| [Lecture 38](Lecture-38.md) | OpenClaw Case Study - App SDK Dogfooding and Typed Gateway RPCs | App SDK, happy path, event normalization, future RPC surfaces |
| [Lecture 39](Lecture-39.md) | OpenClaw Case Study - Gateway RPC Protocol | WebSocket frames, handshake, roles, scopes, pairing, features, node transport |
| [Lecture 40](Lecture-40.md) | OpenClaw Threat Model - MITRE ATLAS for Agent Security | threat matrix, attack chains, trust boundaries, prompt injection, skill supply chain, tool execution controls |
| [Lecture 41](Lecture-41.md) | Pi - A Minimal Coding Agent and the Substrate Beneath OpenClaw | Tiny core (4 tools), no-MCP rationale, custom messages in session log, hot reload, tree-structured sessions, TUI vs LLM-tool surfaces |
| [Lecture 42](Lecture-42.md) | Confidential & Verifiable AI Agents (NVIDIA CC, zk-STARK, PearlChain) | TEE / confidential GPUs (H100/Blackwell), NRAS attestation, zk-STARK / ZKML, hybrid TEE+ZK, blockchain audit, enterprise threat model |

</details>

</div>

## 实验索引

<div class="lecture-map" markdown>

| # | 标题 | 构建 |
|---|-------|-------|
| [实验 01](Lab-01-Research-Agent.md) | 研究 agent 与工具使用 | Web 搜索 + 代码执行 + 引用 |
| [实验 02](Lab-02-Multi-Agent-Pipeline.md) | 多 agent 代码审查 | 规划器 → 编码器 → 审查器 → 总结器 |
| [实验 03](Lab-03-Production-RAG.md) | 生产级 RAG 系统 | 摄入流水线 + 混合搜索 + RAGAS 评测 |
| [实验 04](Lab-04-TokenJuice-Output-Compaction.md) | TokenJuice 输出压缩 | 确定性的终端输出归约，原始绕过，产物恢复，项目归约器 |
| [实验 05](Lab-05-OpenMeow-App-SDK-Dogfood.md) | 在 macOS 上 dogfood OpenMeow App SDK | 使用 OpenCoven 的 OpenMeow 适配器测试 OpenClaw App SDK，fixtures，UI 归约器，实时 Gateway 冒烟测试，以及可选的 Coven 会话 |
| [实验 06](Lab-06-Genie-Claw-Capstone.md) | **顶点项目：构建 genie-claw** | 你自己的最小 agent harness（agent 运行时框架）—— 运行循环、工具、持久会话、护栏、人类在环，连接到本地 LLM runtime |

</div>

## 前置要求

- Python 3.10+
- PyTorch 基础（阶段 3 核心 — 神经网络）
- 你运行的任何 provider 示例的 API 密钥

```bash
pip install anthropic openai pydantic fastapi uvicorn \
            langchain langgraph langchain-anthropic langchain-openai \
            chromadb sentence-transformers ragas opentelemetry-api
```

只安装你正在运行的讲座所需的包。对于生产工作，在 `requirements.txt` 或 `pyproject.toml` 中固定版本，并在升级 SDK 之前查看 provider 迁移说明。

代码片段使用占位符模型 ID，例如 `your-agent-model-id`、`your-fast-model-id` 和 `your-embedding-model-id`。在运行示例之前，将它们替换为你 provider 的当前模型 ID。

---

## 外部参考

<div class="lecture-map" markdown>

| 资源 | 涵盖内容 |
|----------|----------------|
| [The OpenClaw Book](https://openclawconsultant.com/openclaw-book/) | 从业者 OpenClaw 指南：架构、设置、技能、提示、规划、优化、子 agent、安全 |
| [LangChain Documentation](https://python.langchain.com/docs/) | agent 和 RAG 框架 |
| [LangGraph Documentation](https://langchain-ai.github.io/langgraph/) | 持久、有状态的 agent 工作流，人类在环，记忆和链路追踪 |
| [OpenAI Agents SDK](https://openai.github.io/openai-agents-python/) | agent 循环、工具、移交、护栏、会话、链路追踪和 MCP 集成 |
| [OpenAI API Agents Guide](https://platform.openai.com/docs/guides/agents) | 代码优先的 agent 应用、工具、编排和可观测性 |
| [Model Context Protocol Specification](https://modelcontextprotocol.io/) | 用于工具、资源、提示、主机、客户端、服务器和安全的标准协议 |
| [Claude Code Overview](https://docs.claude.com/en/docs/claude-code/overview) | 智能体化编码工作流，MCP，多 agent 使用，以及 CI 模式 |
| [Claude Code Plugins](https://docs.claude.com/en/docs/claude-code/plugins) | 技能、agent、钩子、MCP 服务器、插件结构和分发 |
| [Claude Code Repository](https://github.com/anthropics/claude-code) | 公共实现表面、示例、插件和项目布局 |
| [Anthropic Cookbook](https://github.com/anthropics/anthropic-cookbook) | 实用的 Claude API 示例 |
| OpenClaw Repository | 本地优先的助手架构、频道、网关模型和安全默认值 |
| OpenClaw Gateway Architecture | 长生命周期网关、WS 协议、节点、配对和远程访问模型 |
| OpenClaw Features | 多 agent 路由、媒体、频道、工具、应用和 provider 支持 |
| GitHub Agentic Workflows | GitHub 对智能体化 CI/CD、权限和安全输出的官方框架 |
| [OWASP Top 10 for LLM Applications](https://genai.owasp.org/llm-top-10/) | 提示注入、不安全的输出处理、工具风险、过度代理、LLM 应用安全 |
| [NIST AI RMF Generative AI Profile](https://www.nist.gov/itl/ai-risk-management-framework) | 生成式 AI 系统的治理和风险管理框架 |
| [LlamaIndex Documentation](https://docs.llamaindex.ai/) | RAG 最佳实践 |
| [Build a Large Language Model (From Scratch) — Raschka](https://github.com/rasbt/LLMs-from-scratch) | LLM 内部原理 |

</div>


<details>
<summary>English original</summary>

**Lab Index**

<div class="lecture-map" markdown>

| # | Title | Build |
|---|-------|-------|
| [Lab 01](Lab-01-Research-Agent.md) | Research Agent with Tool Use | Web search + code execution + citations |
| [Lab 02](Lab-02-Multi-Agent-Pipeline.md) | Multi-Agent Code Review | Planner → Coder → Reviewer → Summarizer |
| [Lab 03](Lab-03-Production-RAG.md) | Production RAG System | Ingestion pipeline + hybrid search + RAGAS eval |
| [Lab 04](Lab-04-TokenJuice-Output-Compaction.md) | TokenJuice Output Compaction | Deterministic terminal-output reduction, raw bypasses, artifact recovery, project reducers |
| [Lab 05](Lab-05-OpenMeow-App-SDK-Dogfood.md) | OpenMeow App SDK Dogfood on macOS | Test the OpenClaw App SDK with OpenCoven's OpenMeow adapter, fixtures, UI reducers, live Gateway smoke tests, and optional Coven sessions |
| [Lab 06](Lab-06-Genie-Claw-Capstone.md) | **Capstone: Build genie-claw** | Your own minimal agent harness — run loop, tools, durable sessions, guardrails, human-in-the-loop, wired to a local LLM runtime |

</div>

**Prerequisites**

- Python 3.10+
- PyTorch basics (Phase 3 Core — Neural Networks)
- API keys for whichever provider examples you run

```bash
pip install anthropic openai pydantic fastapi uvicorn \
            langchain langgraph langchain-anthropic langchain-openai \
            chromadb sentence-transformers ragas opentelemetry-api
```

Install only the packages needed for the lecture you are running. For production work, pin versions in `requirements.txt` or `pyproject.toml` and review provider migration notes before upgrading SDKs.

Code snippets use placeholder model IDs such as `your-agent-model-id`, `your-fast-model-id`, and `your-embedding-model-id`. Replace them with current model IDs from your provider before running the examples.

---

**External References**

<div class="lecture-map" markdown>

| Resource | What it covers |
|----------|----------------|
| [The OpenClaw Book](https://openclawconsultant.com/openclaw-book/) | Practitioner OpenClaw guide: architecture, setup, skills, prompting, planning, optimization, sub-agents, security |
| [LangChain Documentation](https://python.langchain.com/docs/) | Agent and RAG framework |
| [LangGraph Documentation](https://langchain-ai.github.io/langgraph/) | Durable, stateful agent workflows, human-in-the-loop, memory, and tracing |
| [OpenAI Agents SDK](https://openai.github.io/openai-agents-python/) | Agent loops, tools, handoffs, guardrails, sessions, tracing, and MCP integration |
| [OpenAI API Agents Guide](https://platform.openai.com/docs/guides/agents) | Code-first agent apps, tools, orchestration, and observability |
| [Model Context Protocol Specification](https://modelcontextprotocol.io/) | Standard protocol for tools, resources, prompts, hosts, clients, servers, and safety |
| [Claude Code Overview](https://docs.claude.com/en/docs/claude-code/overview) | Agentic coding workflows, MCP, multi-agent use, and CI patterns |
| [Claude Code Plugins](https://docs.claude.com/en/docs/claude-code/plugins) | Skills, agents, hooks, MCP servers, plugin structure, and distribution |
| [Claude Code Repository](https://github.com/anthropics/claude-code) | Public implementation surface, examples, plugins, and project layout |
| [Anthropic Cookbook](https://github.com/anthropics/anthropic-cookbook) | Practical Claude API examples |
| OpenClaw Repository | Local-first assistant architecture, channels, gateway model, and security defaults |
| OpenClaw Gateway Architecture | Long-lived gateway, WS protocol, nodes, pairing, and remote access model |
| OpenClaw Features | Multi-agent routing, media, channels, tools, apps, and provider support |
| GitHub Agentic Workflows | Official GitHub framing for agentic CI/CD, permissions, and safe outputs |
| [OWASP Top 10 for LLM Applications](https://genai.owasp.org/llm-top-10/) | Prompt injection, insecure output handling, tool risk, excessive agency, LLM app security |
| [NIST AI RMF Generative AI Profile](https://www.nist.gov/itl/ai-risk-management-framework) | Governance and risk-management framing for generative AI systems |
| [LlamaIndex Documentation](https://docs.llamaindex.ai/) | RAG best practices |
| [Build a Large Language Model (From Scratch) — Raschka](https://github.com/rasbt/LLMs-from-scratch) | LLM internals |

</div>

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
