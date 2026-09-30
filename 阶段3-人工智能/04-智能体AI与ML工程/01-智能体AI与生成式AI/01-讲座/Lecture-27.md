---
title: 第 27 讲 - AI Agent 系统的确定性启动
description: 第 27 讲 - AI Agent 系统的确定性启动
---

# 第 27 讲 - AI Agent 系统的确定性启动

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [第 26 讲](Lecture-26.md) | **下一讲：** [第 28 讲](Lecture-28.md)

---

## 为什么会有这一讲

AI agent 系统不只是一次模型调用。

它通常是一层层可动的部件：

- 配置文件
- 环境变量
- 模型客户端
- 提示词
- 工具
- 记忆存储
- 向量索引
- 工作流图
- 调度器
- 后台 worker
- 认证提供方
- 可观测性
- runtime 策略

如果这些部件每次都以不同的顺序启动，系统就会难以调试。

如果某个依赖只准备了一半，agent 可能带着缺失的工具、过期的记忆、错误的提示词、失效的检索或不安全的权限运行。

这就是生产环境的 agent 系统需要**确定性启动**的原因。

确定性启动意味着：

> 给定相同的代码、配置、密钥、数据快照和环境，agent 系统每次都会启动到同一个已知良好的状态。

这并不意味着模型输出变得确定。LLM 仍可能生成不同的文本。

它意味着**模型周围的系统**以**可预测的方式**启动。

---

## 学习目标

学完这一讲，你将能够：

1. 用简单的语言解释确定性启动。
2. 指出启动顺序不明确时 agent 系统为什么会失败。
3. 设计带有明确阶段的启动序列。
4. 在对外提供推理服务之前校验配置、提示词、工具、模型客户端、记忆、索引、策略和 worker。
5. 构建就绪检查，阻止只启动了一半的 agent 接收请求。
6. 把启动、预热、恢复和正常推理服务区分开。
7. 为真实的 AI agent 应用编写启动 manifest。
8. 理解为什么确定性启动对边缘 AI 设备和常驻助手很重要。

---

## 1. 简单的心智模型

把 AI agent 系统想象成一座智能工厂。

工厂开工之前，你不会希望工人随意地按任意顺序打开机器。

你需要一份检查清单：

1. 电力稳定。
2. 安全防护已安装。
3. 机器已校准。
4. 物料已装载。
5. 操作员已分配。
6. 急停可用。
7. 质量检查通过。
8. 开始生产。

agent 系统也需要同样的纪律。

糟糕的启动长这样：

```text
server starts
  -> accepts request
  -> model client is ready
  -> tool registry is still loading
  -> vector index is stale
  -> memory migration is incomplete
  -> policy engine has old rules
  -> agent acts incorrectly
```


确定性启动长这样：

```text
load config
  -> validate schema
  -> connect dependencies
  -> register tools
  -> load prompts
  -> load policies
  -> hydrate memory
  -> verify indexes
  -> warm model paths
  -> run startup self-test
  -> mark service ready
  -> accept traffic
```


在完整的**启动契约**通过之前，系统不应为用户提供服务。

---

## 2. 确定性启动不是什么

确定性启动**不**意味着：

- 模型总是返回相同的答案
- temperature 必须始终为 `0`
- 每个请求都走同一条路径
- agent 无法在 runtime 自适应
- 系统永不失败

它意味着：

- 启动顺序是明确的
- 启动检查可重复
- 配置经过校验
- 工具以可预测的方式注册
- 提示词和策略有已知的版本
- 依赖要么就绪，要么服务拒绝流量
- 失败发生在早期，而不是在用户请求过程中悄无声息地出现

在专业系统里，启动应该是无聊的。

如果启动令人意外，生产环境只会更糟。

---

## 3. 为什么 agent 系统尤其需要它

普通 Web 服务同样需要确定性启动。

agent 系统更需要它，因为模型可以用流畅的语言把基础设施问题掩盖起来。

例如：

```text
User:
What did customer ACME order last month?

Agent problem:
CRM tool did not register at startup.

Bad agent behavior:
"ACME likely ordered standard parts based on previous demand."
```


答案听起来合理，但它是错的。

再看一个例子：

```text
User:
Summarize the safety procedure.

Agent problem:
Vector index failed to load the latest safety manual.

Bad agent behavior:
Answers from an old version of the manual.
```


在普通软件中，缺少依赖通常会引发明显的错误。

在 AI 系统中，缺少依赖可能导致**自信的错误行为**。

这就是核心风险。

---


<details>
<summary>English original</summary>

**Lecture 27 - Deterministic Startup for AI Agent Systems**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 26](Lecture-26.md) | **Next:** [Lecture 28](Lecture-28.md)

---

**Why this lecture exists**

An AI agent system is not just a model call.

It is usually a stack of moving parts:

- configuration files
- environment variables
- model clients
- prompts
- tools
- memory stores
- vector indexes
- workflow graphs
- schedulers
- background workers
- auth providers
- observability
- runtime policies

If those parts start in a different order every time, the system becomes hard to debug.

If one dependency is half-ready, the agent may run with missing tools, stale memory, wrong prompts, broken retrieval, or unsafe permissions.

That is why production agent systems need **deterministic startup**.

Deterministic startup means:

> Given the same code, config, secrets, data snapshot, and environment, the agent system boots into the same known-good state every time.

This does not mean model outputs become deterministic. LLMs may still produce different text.

It means the **system around the model** starts **predictably**.

---

**Learning objectives**

By the end of this lecture you will be able to:

1. Explain deterministic startup in simple terms.
2. Identify why agent systems fail when startup order is unclear.
3. Design a startup sequence with explicit phases.
4. Validate config, prompts, tools, model clients, memory, indexes, policies, and workers before serving traffic.
5. Build readiness checks that prevent half-started agents from accepting requests.
6. Separate startup, warmup, recovery, and normal serving.
7. Write a startup manifest for a real AI agent application.
8. Understand why deterministic startup matters for edge AI devices and always-on assistants.

---

**1. The simple mental model**

Think of an AI agent system like a smart factory.

Before the factory opens, you do not want workers randomly turning on machines in any order.

You want a checklist:

1. Power is stable.
2. Safety guards are installed.
3. Machines are calibrated.
4. Materials are loaded.
5. Operators are assigned.
6. Emergency stop works.
7. Quality checks pass.
8. Production starts.

Agent systems need the same discipline.

A bad startup looks like this:

```text
server starts
  -> accepts request
  -> model client is ready
  -> tool registry is still loading
  -> vector index is stale
  -> memory migration is incomplete
  -> policy engine has old rules
  -> agent acts incorrectly
```

A deterministic startup looks like this:

```text
load config
  -> validate schema
  -> connect dependencies
  -> register tools
  -> load prompts
  -> load policies
  -> hydrate memory
  -> verify indexes
  -> warm model paths
  -> run startup self-test
  -> mark service ready
  -> accept traffic
```

The system should not serve users until the full **startup contract** passes.

---

**2. What deterministic startup is not**

Deterministic startup does **not** mean:

- the model always returns the same answer
- temperature must always be `0`
- every request follows the same path
- the agent cannot adapt at runtime
- the system never fails

It means:

- startup order is explicit
- startup checks are repeatable
- configuration is validated
- tools are registered predictably
- prompts and policies have known versions
- dependencies are either ready or the service refuses traffic
- failures happen early instead of silently during user requests

In professional systems, startup should be boring.

If startup is surprising, production will be worse.

---

**3. Why agent systems especially need this**

Normal web services need deterministic startup too.

Agent systems need it more because the model can hide infrastructure problems behind fluent language.

Example:

```text
User:
What did customer ACME order last month?

Agent problem:
CRM tool did not register at startup.

Bad agent behavior:
"ACME likely ordered standard parts based on previous demand."
```

The answer sounds plausible, but it is wrong.

Another example:

```text
User:
Summarize the safety procedure.

Agent problem:
Vector index failed to load the latest safety manual.

Bad agent behavior:
Answers from an old version of the manual.
```

With normal software, a missing dependency often causes an obvious error.

With AI systems, missing dependencies can cause **confident wrong behavior**.

That is the core risk.

---

</details>

## 4. 启动契约

**启动契约**是一份书面承诺，规定系统在接受流量之前必须满足哪些条件。

示例：

```text
This agent service is ready only when:

- required environment variables are present
- config schema validates
- model provider is reachable
- prompt bundle version is known
- tool registry contains exactly the expected tools
- tool permissions are loaded
- vector index version matches the document snapshot
- memory store schema is migrated
- policy engine has loaded the active policy bundle
- tracing and audit logging are writable
- health and readiness checks pass
```

该契约应由代码强制执行。

不要只依赖 README 里的检查清单。

---

## 5. 启动阶段

用**阶段**替代随意的初始化。

### 阶段 0 - 进程启动

进程已存在，但此时还不应对外提供任何流量服务。

### 阶段 1 - 加载静态配置

加载：

- 配置文件
- 环境变量
- 部署 profile
- 功能开关
- 模型名称
- 提示词 bundle 版本
- 工具允许列表
- 策略 bundle 版本

规则：

> 此时还不要发起网络调用。只加载并校验本地输入。

### 阶段 2 - 校验配置

检查：

- 必填字段存在
- 路径有效
- 模型名称在允许范围内
- 工具名称为已知名称
- 数值上限合理
- 危险的功能开关未被误开启

配置无效时快速失败。

### 阶段 3 - 连接依赖

连接：

- 模型供应商
- 向量数据库
- 关系型数据库
- 缓存
- 队列
- 记忆存储
- 对象存储
- 认证供应商
- 可观测性后端

规则：

> 连接、校验并记录版本。除非应用明确支持降级模式，否则不要在依赖缺失时静默继续。

### 阶段 4 - 注册工具

构建工具注册表。

为每个工具注册：

- 名称
- 描述
- 输入 schema
- 风险等级
- 超时
- 权限规则
- 负责人
- 审计策略
- 幂等行为

不要让工具在不受控的情况下动态出现。

### 阶段 5 - 加载提示词与策略

加载：

- 系统提示词
- agent 角色提示词
- RAG 提示词模板
- 工具使用说明
- runtime 安全策略
- 拒答策略
- 人工审批规则

每一项都应有版本。

### 阶段 6 - 填充记忆与状态

加载：

- 会话状态
- 长期记忆
- 用户偏好
- 工作流检查点
- agent graph 检查点
- 任务队列

在提供推理服务之前运行迁移。

### 阶段 7 - 校验检索索引

检查：

- 索引存在
- 文档快照版本与预期版本一致
- embedding 模型版本与已存储向量一致
- top-k 检索冒烟测试通过
- 访问控制过滤器已安装

### 阶段 8 - 预热关键路径

预热：

- 模型客户端
- tokenizer 或本地模型 runtime
- embedding 模型
- 向量搜索路径
- 常用提示词模板渲染
- 常用工具 schema 校验

这能减少首个请求时的意外。

### 阶段 9 - 启动自检

运行一段简短的自检：

- 安全提示词调用
- 安全检索查询
- 只读工具调用
- 策略拦截测试
- 审计日志写入
- 就绪报告

### 阶段 10 - 标记就绪

只有到这时，`/readyz` 才应返回成功。

在此之前，`/livez` 可以为真，但 `/readyz` 应为假。

---

## 6. 存活 vs 就绪

这一区分很重要。

| 检查 | 含义 | 是否应发送流量？ |
|---|---|---|
| 存活 | 进程存活 | 不一定 |
| 就绪 | 服务已准备好处理请求 | 是 |

agent 服务可以处于存活但未就绪的状态。

示例：

```text
/livez  -> 200 OK
/readyz -> 503 Not Ready
```

启动仍在进行时，这是正确的。

糟糕的设计：

```text
/health -> 200 OK
```

即使工具、记忆或策略缺失也如此。

专业规则：

> 存活问的是“这个进程是否应被重启？”就绪问的是“这个进程是否应接收用户流量？”

---

## 7. 启动清单

启动清单是一份简单的文档，准确描述 agent 在启动时预期什么。

示例：

```yaml
service: hardware_support_agent
version: 0.4.2

startup:
  required_env:
    - MODEL_PROVIDER
    - MODEL_API_KEY
    - VECTOR_DB_URL
    - AUDIT_LOG_URL

  models:
    chat:
      name: gpt-example-prod
      required: true
    embedding:
      name: text-embedding-example
      required: true

  prompts:
    bundle: hardware_support_prompts
    version: 2026-04-23

  retrieval:
    index: hardware_docs_index
    document_snapshot: docs_2026_04_20
    embedding_model: text-embedding-example

  tools:
    - name: search_docs
      risk: low
      required: true
    - name: create_ticket
      risk: medium
      required: true
    - name: send_email
      risk: high
      required: false

  policies:
    bundle: agent_runtime_policy
    version: 8

  readiness:
    require_audit_log: true
    require_policy_engine: true
    require_retrieval_smoke_test: true
```

清单让启动过程可审查。

如果有任何变化，diff 都是可见的。

---


<details>
<summary>English original</summary>

**4. The startup contract**

A **startup contract** is a written promise about what must be true before the system accepts traffic.

Example:

```text
This agent service is ready only when:

- required environment variables are present
- config schema validates
- model provider is reachable
- prompt bundle version is known
- tool registry contains exactly the expected tools
- tool permissions are loaded
- vector index version matches the document snapshot
- memory store schema is migrated
- policy engine has loaded the active policy bundle
- tracing and audit logging are writable
- health and readiness checks pass
```

This contract should be enforced by code.

Do not rely on a README checklist alone.

---

**5. Startup phases**

Use **phases** instead of random initialization.

**Phase 0 - process starts**

The process exists, but nothing should serve traffic yet.

**Phase 1 - load static configuration**

Load:

- config files
- environment variables
- deployment profile
- feature flags
- model names
- prompt bundle version
- tool allowlist
- policy bundle version

Rule:

> No network calls yet. Just load and validate local inputs.

**Phase 2 - validate configuration**

Check:

- required fields exist
- paths are valid
- model names are allowed
- tool names are known
- numeric limits are sane
- dangerous feature flags are not enabled by accident

Fail fast if config is invalid.

**Phase 3 - connect dependencies**

Connect to:

- model provider
- vector database
- relational database
- cache
- queue
- memory store
- object storage
- auth provider
- observability backend

Rule:

> Connect, verify, and record versions. Do not silently continue with missing dependencies unless the app explicitly supports degraded mode.

**Phase 4 - register tools**

Build the tool registry.

For each tool, register:

- name
- description
- input schema
- risk level
- timeout
- permission rule
- owner
- audit policy
- idempotency behavior

Do not let tools appear dynamically without control.

**Phase 5 - load prompts and policies**

Load:

- system prompts
- agent role prompts
- RAG prompt templates
- tool-use instructions
- runtime security policies
- refusal policies
- human approval rules

Each should have a version.

**Phase 6 - hydrate memory and state**

Load:

- session state
- long-term memory
- user preferences
- workflow checkpoints
- agent graph checkpoints
- task queues

Run migrations before serving.

**Phase 7 - verify retrieval indexes**

Check:

- index exists
- document snapshot version matches expected version
- embedding model version matches the stored vectors
- top-k retrieval smoke test works
- access control filters are installed

**Phase 8 - warm critical paths**

Warm:

- model client
- tokenizer or local model runtime
- embedding model
- vector search path
- common prompt template rendering
- common tool schema validation

This reduces first-request surprises.

**Phase 9 - startup self-test**

Run a short self-test:

- safe prompt call
- safe retrieval query
- read-only tool call
- policy block test
- audit log write
- readiness report

**Phase 10 - mark ready**

Only now should `/readyz` return success.

Before this point, `/livez` may be true, but `/readyz` should be false.

---

**6. Liveness vs readiness**

This distinction matters.

| Check | Meaning | Should traffic be sent? |
|---|---|---|
| Liveness | The process is alive | Not necessarily |
| Readiness | The service is ready to handle requests | Yes |

An agent service can be alive but not ready.

Example:

```text
/livez  -> 200 OK
/readyz -> 503 Not Ready
```

This is correct while startup is still running.

Bad design:

```text
/health -> 200 OK
```

even though tools, memory, or policies are missing.

Professional rule:

> Liveness asks "should this process be restarted?" Readiness asks "should this process receive user traffic?"

---

**7. Startup manifest**

A startup manifest is a simple document that describes exactly what the agent expects at boot.

Example:

```yaml
service: hardware_support_agent
version: 0.4.2

startup:
  required_env:
    - MODEL_PROVIDER
    - MODEL_API_KEY
    - VECTOR_DB_URL
    - AUDIT_LOG_URL

  models:
    chat:
      name: gpt-example-prod
      required: true
    embedding:
      name: text-embedding-example
      required: true

  prompts:
    bundle: hardware_support_prompts
    version: 2026-04-23

  retrieval:
    index: hardware_docs_index
    document_snapshot: docs_2026_04_20
    embedding_model: text-embedding-example

  tools:
    - name: search_docs
      risk: low
      required: true
    - name: create_ticket
      risk: medium
      required: true
    - name: send_email
      risk: high
      required: false

  policies:
    bundle: agent_runtime_policy
    version: 8

  readiness:
    require_audit_log: true
    require_policy_engine: true
    require_retrieval_smoke_test: true
```

The manifest makes startup reviewable.

If something changes, the diff is visible.

---

</details>

## 8. 配置验证示例

使用 **结构化配置**，而不是散落在代码库各处、松散的环境变量访问。

```python
from pydantic import BaseModel, Field, HttpUrl


class ModelConfig(BaseModel):
    chat_model: str
    embedding_model: str
    temperature: float = Field(ge=0.0, le=2.0)
    max_tokens: int = Field(gt=0, le=8192)


class RetrievalConfig(BaseModel):
    vector_db_url: HttpUrl
    index_name: str
    document_snapshot: str
    top_k: int = Field(gt=0, le=50)


class RuntimePolicyConfig(BaseModel):
    policy_bundle: str
    policy_version: int = Field(gt=0)
    require_human_approval_for_high_risk_tools: bool = True


class AgentConfig(BaseModel):
    service_name: str
    environment: str
    models: ModelConfig
    retrieval: RetrievalConfig
    policy: RuntimePolicyConfig


def load_config(raw: dict) -> AgentConfig:
    config = AgentConfig.model_validate(raw)

    if config.environment == "prod" and config.models.temperature > 0.7:
        raise ValueError("production temperature is too high for this agent")

    return config
```

要点：

> 配置错误应当在启动阶段就失败，而不是等到第一个客户请求时才失败。

---

## 9. 工具注册表确定性

**工具注册表**必须可预测。

不良模式：

```python
tools = discover_all_tools_from_folder("tools/")
```

为什么这样做有风险：

- 文件顺序可能变化
- 可能意外加载非预期工具
- 实验性工具可能出现在生产环境
- 权限可能与工具集不匹配
- 审查困难

更好的模式：

```python
EXPECTED_TOOLS = [
    "search_docs",
    "create_ticket",
    "lookup_part_number",
    "summarize_datasheet",
]


def build_tool_registry(tool_factories: dict) -> dict:
    registry = {}

    for name in EXPECTED_TOOLS:
        if name not in tool_factories:
            raise RuntimeError(f"missing required tool: {name}")

        tool = tool_factories[name]()
        validate_tool_schema(tool)
        validate_tool_policy(tool)
        registry[name] = tool

    extra_tools = set(tool_factories) - set(EXPECTED_TOOLS)
    if extra_tools:
        raise RuntimeError(f"unexpected tools available: {sorted(extra_tools)}")

    return registry
```

专业准则：

> 生产工具应当被显式注册、带版本，并通过策略检查。

---

## 10. Prompt 确定性

Prompt 是类似代码的资产。

它们应当具备：

- 名称
- 版本
- 负责人
- 测试
- 变更日志
- 回滚路径

不良模式：

```python
SYSTEM_PROMPT = "You are helpful."
```

更好的模式：

```yaml
prompt_bundle: hardware_support_agent
version: 2026-04-23

prompts:
  system:
    file: prompts/system.md
    sha256: "..."
  tool_router:
    file: prompts/tool_router.md
    sha256: "..."
  rag_answer:
    file: prompts/rag_answer.md
    sha256: "..."
```

启动时验证：

- 文件存在
- 哈希匹配
- 必需变量齐备
- 使用测试数据能正常渲染
- prompt 版本被记录

这样出故障时更容易排查。

如果模型产出错误输出，需要知道当时生效的是哪个 prompt 版本。

---

## 11. 检索确定性

**RAG（检索增强生成）启动**必须验证检索没有被静默破坏。

检查：

- 向量数据库可达
- 索引存在
- 文档数量在预期范围内
- embedding 维度匹配
- embedding 模型版本匹配
- 访问控制过滤器存在
- 样本查询返回预期文档

启动冒烟测试示例：

```python
def verify_retrieval(index, expected_snapshot: str):
    metadata = index.get_metadata()

    if metadata["snapshot"] != expected_snapshot:
        raise RuntimeError(
            f"index snapshot mismatch: expected {expected_snapshot}, "
            f"got {metadata['snapshot']}"
        )

    results = index.search("ESP32-C6 UART pin configuration", top_k=3)
    ids = {item.document_id for item in results}

    if "esp32c6_uart_guide" not in ids:
        raise RuntimeError("retrieval smoke test failed")
```

不要依赖「向量数据库连接正常」这一点。

连接成功只能证明数据库给出了响应。

它不能证明加载的是正确的索引。

---


<details>
<summary>English original</summary>

**8. Config validation example**

Use **structured config** instead of loose environment access scattered through the codebase.

```python
from pydantic import BaseModel, Field, HttpUrl


class ModelConfig(BaseModel):
    chat_model: str
    embedding_model: str
    temperature: float = Field(ge=0.0, le=2.0)
    max_tokens: int = Field(gt=0, le=8192)


class RetrievalConfig(BaseModel):
    vector_db_url: HttpUrl
    index_name: str
    document_snapshot: str
    top_k: int = Field(gt=0, le=50)


class RuntimePolicyConfig(BaseModel):
    policy_bundle: str
    policy_version: int = Field(gt=0)
    require_human_approval_for_high_risk_tools: bool = True


class AgentConfig(BaseModel):
    service_name: str
    environment: str
    models: ModelConfig
    retrieval: RetrievalConfig
    policy: RuntimePolicyConfig


def load_config(raw: dict) -> AgentConfig:
    config = AgentConfig.model_validate(raw)

    if config.environment == "prod" and config.models.temperature > 0.7:
        raise ValueError("production temperature is too high for this agent")

    return config
```

Important point:

> Configuration errors should fail during startup, not during the first customer request.

---

**9. Tool registry determinism**

The **tool registry** must be predictable.

Bad pattern:

```python
tools = discover_all_tools_from_folder("tools/")
```

Why this is risky:

- file ordering may vary
- accidental tools may load
- experimental tools may appear in production
- permissions may not match the tool set
- review is difficult

Better pattern:

```python
EXPECTED_TOOLS = [
    "search_docs",
    "create_ticket",
    "lookup_part_number",
    "summarize_datasheet",
]


def build_tool_registry(tool_factories: dict) -> dict:
    registry = {}

    for name in EXPECTED_TOOLS:
        if name not in tool_factories:
            raise RuntimeError(f"missing required tool: {name}")

        tool = tool_factories[name]()
        validate_tool_schema(tool)
        validate_tool_policy(tool)
        registry[name] = tool

    extra_tools = set(tool_factories) - set(EXPECTED_TOOLS)
    if extra_tools:
        raise RuntimeError(f"unexpected tools available: {sorted(extra_tools)}")

    return registry
```

Professional rule:

> Production tools should be explicitly registered, versioned, and policy-checked.

---

**10. Prompt determinism**

Prompts are code-like assets.

They should have:

- names
- versions
- owners
- tests
- changelogs
- rollback path

Bad pattern:

```python
SYSTEM_PROMPT = "You are helpful."
```

Better pattern:

```yaml
prompt_bundle: hardware_support_agent
version: 2026-04-23

prompts:
  system:
    file: prompts/system.md
    sha256: "..."
  tool_router:
    file: prompts/tool_router.md
    sha256: "..."
  rag_answer:
    file: prompts/rag_answer.md
    sha256: "..."
```

At startup, verify:

- files exist
- hashes match
- required variables are present
- rendering works with test data
- prompt versions are logged

This makes incidents easier to investigate.

If a model produces bad output, you need to know which prompt version was active.

---

**11. Retrieval determinism**

**RAG startup** must verify that retrieval is not silently broken.

Check:

- vector database reachable
- index exists
- expected document count range
- embedding dimension matches
- embedding model version matches
- access-control filters exist
- sample query returns expected documents

Example startup smoke test:

```python
def verify_retrieval(index, expected_snapshot: str):
    metadata = index.get_metadata()

    if metadata["snapshot"] != expected_snapshot:
        raise RuntimeError(
            f"index snapshot mismatch: expected {expected_snapshot}, "
            f"got {metadata['snapshot']}"
        )

    results = index.search("ESP32-C6 UART pin configuration", top_k=3)
    ids = {item.document_id for item in results}

    if "esp32c6_uart_guide" not in ids:
        raise RuntimeError("retrieval smoke test failed")
```

Do not rely on "the vector DB connection works."

Connection success only proves that the database answered.

It does not prove that the right index is loaded.

---

</details>

## 12. 记忆的确定性

Agent 记忆强大，但也很危险。

启动时需确定：

- 加载哪些记忆存储
- 恢复哪些会话
- 哪些记忆已过期
- 哪些记忆被隔离
- 必须执行哪些 schema 迁移
- 支持哪个检查点版本

错误模式：

```text
load all previous memory into context automatically
```

更好的模式：

```text
load only memory that:
  - belongs to this user
  - belongs to this tenant
  - matches current schema
  - is not expired
  - is not quarantined
  - is relevant to the current task
```

记忆启动检查：

- schema 版本匹配
- 迁移已完成
- 记忆数量在预期范围内
- 隔离表可读
- 检查点回放对某个测试会话有效

专业准则：

> 记忆不只是数据。它是未来的 prompt 上下文。要把它当作可执行影响来对待。

---

## 13. 策略的确定性

**Runtime 安全策略**必须在使用工具之前加载。

错误的启动方式：

```text
agent starts
  -> tools register
  -> service accepts traffic
  -> policy engine loads later
```

这会制造一个窗口期，工具可能在无强制约束的情况下运行。

正确的启动方式：

```text
load policy bundle
  -> validate policy syntax
  -> register tools
  -> bind tool to policy
  -> run allow/block self-test
  -> mark tool layer ready
```

自检示例：

```python
def verify_policy_engine(policy_engine):
    allowed = policy_engine.evaluate(
        user_role="engineer",
        tool="search_docs",
        arguments={"query": "Jetson audio setup"},
    )
    assert allowed.decision == "allow"

    blocked = policy_engine.evaluate(
        user_role="guest",
        tool="export_customer_records",
        arguments={"format": "csv"},
    )
    assert blocked.decision == "block"
```

如果阻断测试失败，服务就不应启动。

---

## 14. 模型客户端的确定性

**模型客户端**不应在无检查的情况下惰性创建。

启动时需验证：

- provider 凭证存在
- 所选模型被允许使用
- 已配置 timeout
- 已配置重试策略
- 已配置熔断器
- 已知回退模型
- 模型响应路径对一个小测试请求可用

不要在启动时运行昂贵的 prompt。

用一个低成本的健全性检查：

```python
def verify_model_client(client):
    response = client.generate(
        messages=[
            {"role": "system", "content": "Return exactly OK."},
            {"role": "user", "content": "health check"},
        ],
        max_tokens=4,
        temperature=0,
        timeout=5,
    )

    if "OK" not in response.text:
        raise RuntimeError("model client health check failed")
```

这并不能证明模型质量。

它能证明模型路径可达且配置正确。

---

## 15. 确定性的图启动

工作流 agent 通常使用一张图：

```text
planner -> retriever -> tool_router -> executor -> reviewer -> responder
```

启动时需验证：

- 所有节点都存在
- 所有边都有效
- 不存在不可达的必需节点
- 环是有意为之的
- 在需要的地方启用了检查点
- 高风险路径上存在人工审批节点
- 图版本已记录

图 manifest 示例：

```yaml
graph: support_agent_graph
version: 12

nodes:
  - planner
  - retriever
  - tool_router
  - executor
  - reviewer
  - responder

edges:
  planner:
    - retriever
    - tool_router
  retriever:
    - responder
  tool_router:
    - executor
    - reviewer
  executor:
    - reviewer
  reviewer:
    - responder
```

若图引用了不存在的节点，启动时应予拒绝。

---

## 16. 幂等启动

启动应当**可以安全地重复运行**。

这一点很重要，因为容器、systemd 服务、边缘设备和云平台都可能重启进程。

幂等启动意味着重复启动不会重复产生状态，也不会破坏数据。

错误示例：

- 每次重启都创建重复的后台任务
- 每次重启都重发“startup complete”通知
- 不检查版本就重建索引
- 自动运行破坏性迁移
- 追加重复的 system 记忆

更好的示例：

- 仅在队列缺失时创建队列
- 带版本跟踪地运行迁移
- 注册带过期时间的 worker lease
- 按不可变版本加载 prompt bundle
- 用唯一 boot ID 写入启动事件

使用 boot ID：

```python
import uuid

BOOT_ID = str(uuid.uuid4())
```

把它附加到日志上：

```json
{
  "boot_id": "2d8b...",
  "event": "startup_phase_complete",
  "phase": "tool_registry",
  "status": "ok"
}
```

这样就能把一次进程启动产生的所有启动日志归为一组。

---


<details>
<summary>English original</summary>

**12. Memory determinism**

Agent memory is powerful but dangerous.

At startup, decide:

- which memory stores are loaded
- which sessions are resumed
- which memories are expired
- which memories are quarantined
- which schema migrations must run
- which checkpoint version is supported

Bad pattern:

```text
load all previous memory into context automatically
```

Better pattern:

```text
load only memory that:
  - belongs to this user
  - belongs to this tenant
  - matches current schema
  - is not expired
  - is not quarantined
  - is relevant to the current task
```

Memory startup checks:

- schema version matches
- migrations completed
- memory count is within expected range
- quarantine table is readable
- checkpoint replay works for one test session

Professional rule:

> Memory is not just data. It is future prompt context. Treat it like executable influence.

---

**13. Policy determinism**

**Runtime security policies** must load before tools are usable.

Bad startup:

```text
agent starts
  -> tools register
  -> service accepts traffic
  -> policy engine loads later
```

This creates a window where tools may run without enforcement.

Correct startup:

```text
load policy bundle
  -> validate policy syntax
  -> register tools
  -> bind tool to policy
  -> run allow/block self-test
  -> mark tool layer ready
```

Self-test example:

```python
def verify_policy_engine(policy_engine):
    allowed = policy_engine.evaluate(
        user_role="engineer",
        tool="search_docs",
        arguments={"query": "Jetson audio setup"},
    )
    assert allowed.decision == "allow"

    blocked = policy_engine.evaluate(
        user_role="guest",
        tool="export_customer_records",
        arguments={"format": "csv"},
    )
    assert blocked.decision == "block"
```

If the block test fails, the service should not start.

---

**14. Model client determinism**

**Model clients** should not be created lazily without checks.

At startup, verify:

- provider credentials exist
- selected model is allowed
- timeout is configured
- retry policy is configured
- circuit breaker is configured
- fallback model is known
- model response path works for a tiny test request

Do not run an expensive prompt at startup.

Use a cheap sanity check:

```python
def verify_model_client(client):
    response = client.generate(
        messages=[
            {"role": "system", "content": "Return exactly OK."},
            {"role": "user", "content": "health check"},
        ],
        max_tokens=4,
        temperature=0,
        timeout=5,
    )

    if "OK" not in response.text:
        raise RuntimeError("model client health check failed")
```

This does not prove model quality.

It proves the model path is reachable and correctly configured.

---

**15. Deterministic graph startup**

Workflow agents often use a graph:

```text
planner -> retriever -> tool_router -> executor -> reviewer -> responder
```

At startup, verify:

- all nodes exist
- all edges are valid
- no unreachable required node exists
- cycles are intentional
- checkpointing is enabled where needed
- human approval nodes exist for high-risk paths
- graph version is logged

Example graph manifest:

```yaml
graph: support_agent_graph
version: 12

nodes:
  - planner
  - retriever
  - tool_router
  - executor
  - reviewer
  - responder

edges:
  planner:
    - retriever
    - tool_router
  retriever:
    - responder
  tool_router:
    - executor
    - reviewer
  executor:
    - reviewer
  reviewer:
    - responder
```

Startup should reject a graph that references a missing node.

---

**16. Idempotent startup**

Startup should be **safe to run more than once**.

This matters because containers, systemd services, edge devices, and cloud platforms may restart processes.

Idempotent startup means repeated startup does not duplicate state or corrupt data.

Bad examples:

- create duplicate background jobs every restart
- re-send "startup complete" notifications every restart
- recreate indexes without checking version
- run destructive migrations automatically
- append duplicate system memories

Better examples:

- create queue only if missing
- run migrations with version tracking
- register worker lease with expiration
- load prompt bundle by immutable version
- write startup event with unique boot ID

Use a boot ID:

```python
import uuid

BOOT_ID = str(uuid.uuid4())
```

Attach it to logs:

```json
{
  "boot_id": "2d8b...",
  "event": "startup_phase_complete",
  "phase": "tool_registry",
  "status": "ok"
}
```

Now you can group all startup logs from one process boot.

---

</details>

## 17. 降级模式

有时服务可以以**有限能力**启动。

示例：

- chat 可用，但 RAG 不可用
- 只读工具可用，但写入工具被禁用
- 本地模型可用，但云端 fallback 不可用

只有当降级模式是显式的，这才可接受。

糟糕的降级模式：

```text
retrieval broken, but agent answers from memory without telling anyone
```

良好的降级模式：

```text
retrieval unavailable
  -> readiness reports degraded
  -> RAG features disabled
  -> agent tells user it cannot access documents
  -> alert is emitted
```

在就绪状态中体现这一点：

```json
{
  "ready": true,
  "mode": "degraded",
  "disabled_features": ["rag_search"],
  "reason": "vector index unavailable"
}
```

对于高风险系统，可能不允许降级模式。

---

## 18. 启动时间线示例

一份干净的启动日志应该讲出一个故事。

示例：

```text
00.000 boot_id=42 service=assistant start
00.018 phase=config_load ok config_version=prod-17
00.026 phase=config_validate ok
00.143 phase=dependency_connect ok vector_db=ready cache=ready audit=ready
00.181 phase=prompt_load ok prompt_bundle=assistant_prompts@2026-04-23
00.214 phase=policy_load ok policy_bundle=runtime_policy@8
00.266 phase=tool_registry ok tools=7 high_risk=2
00.402 phase=memory_migration ok schema=5
00.611 phase=retrieval_verify ok snapshot=docs_2026_04_20
00.902 phase=model_warmup ok model=prod-small latency_ms=288
01.104 phase=self_test ok
01.105 readiness=true
```

糟糕的启动日志：

```text
server started
```

它几乎没告诉你任何信息。

---

## 19. 边缘设备上的确定性启动

本路线图关注 Jetson 级和嵌入式 AI 系统。

**边缘启动**更难，因为：

- 电源可能不稳定
- 网络可能不可用
- 本地模型加载可能耗时
- 传感器可能延迟出现
- 音频设备可能以不同方式枚举
- 加速器可能需要预热
- 存储可能很慢
- 设备时钟在启动时可能是错的

对于 AI 智能音箱或本地助手，确定性启动可能包括：

```text
system boot
  -> audio device detected
  -> wake-word engine loaded
  -> local ASR model loaded
  -> TTS voice loaded
  -> tool registry loaded
  -> home devices paired
  -> memory store mounted
  -> network state detected
  -> cloud fallback optional
  -> readiness announced
```

如果麦克风阵列未就绪，助手不应假装正在聆听。

如果智能家居控制器不可用，设备控制命令应被禁用。

边缘 agent 规则：

> 本地 AI 产品必须知道启动后哪些能力实际可用。

---

## 20. 启动测试计划

像测试功能一样测试启动。

### 测试 1 - 干净启动

预期：

- 所有启动阶段通过
- 就绪状态变为 true
- 启动时间在预算内

### 测试 2 - 缺少配置

移除一个必需的环境变量。

预期：

- 启动提前失败
- 清晰的错误消息
- 不接受任何流量

### 测试 3 - 缺少工具

移除一个必需的工具实现。

预期：

- 工具注册阶段失败
- 服务保持未就绪

### 测试 4 - 损坏的向量索引

指向错误的文档快照。

预期：

- 检索验证失败
- 根据策略禁用 RAG 或启动失败

### 测试 5 - 策略引擎故障

加载一个无效的策略文件。

预期：

- 策略验证失败
- 高风险工具永远不会变为可用

### 测试 6 - 重启幂等性

启动、停止、再次启动。

预期：

- 无重复任务
- 无重复记忆
- 无重复索引
- 相同的 manifest 版本

### 测试 7 - 冷边缘启动

从关机状态重启设备。

预期：

- 检测到传感器和音频设备
- 本地模型加载
- 服务报告真实能力状态

---

## 21. 实用启动检查清单

在宣布 agent 系统生产就绪之前，回答这些问题。

### 配置

- 所有必需配置字段都经过验证了吗？
- 生产环境中不安全的默认值被拒绝了吗？
- 模型、prompt、策略和图版本都被记录了吗？

### 依赖

- 启动时验证了每个必需依赖吗？
- 降级模式是显式的吗？
- 超时和重试配置好了吗？

### 工具

- 工具注册是显式的吗？
- 意外工具被拒绝了吗？
- 高风险工具绑定了策略吗？
- 工具 schema 经过验证了吗？

### 检索与记忆

- 向量索引版本检查了吗？
- embedding 模型版本检查了吗？
- 记忆迁移在推理服务前完成了吗？
- 被隔离的记忆被排除了吗？

### Runtime 安全

- 策略引擎在工具可用之前加载了吗？
- block-policy 自检运行了吗？
- 审计日志在就绪之前可写吗？

### 运维

- `/livez` 和 `/readyz` 是分开的吗？
- 启动时间被测量了吗？
- 每个启动阶段都记录了吗？
- 有 boot ID 吗？
- 系统能安全重启吗？

---

## 22. 常见错误


<details>
<summary>English original</summary>

**17. Degraded mode**

Sometimes a service can start with **limited capability**.

Example:

- chat works, but RAG is unavailable
- read-only tools work, but write tools are disabled
- local model works, but cloud fallback is unavailable

This is acceptable only if the degraded mode is explicit.

Bad degraded mode:

```text
retrieval broken, but agent answers from memory without telling anyone
```

Good degraded mode:

```text
retrieval unavailable
  -> readiness reports degraded
  -> RAG features disabled
  -> agent tells user it cannot access documents
  -> alert is emitted
```

Represent this in readiness:

```json
{
  "ready": true,
  "mode": "degraded",
  "disabled_features": ["rag_search"],
  "reason": "vector index unavailable"
}
```

For high-risk systems, degraded mode may not be allowed.

---

**18. Startup timeline example**

A clean startup log should tell a story.

Example:

```text
00.000 boot_id=42 service=assistant start
00.018 phase=config_load ok config_version=prod-17
00.026 phase=config_validate ok
00.143 phase=dependency_connect ok vector_db=ready cache=ready audit=ready
00.181 phase=prompt_load ok prompt_bundle=assistant_prompts@2026-04-23
00.214 phase=policy_load ok policy_bundle=runtime_policy@8
00.266 phase=tool_registry ok tools=7 high_risk=2
00.402 phase=memory_migration ok schema=5
00.611 phase=retrieval_verify ok snapshot=docs_2026_04_20
00.902 phase=model_warmup ok model=prod-small latency_ms=288
01.104 phase=self_test ok
01.105 readiness=true
```

Bad startup log:

```text
server started
```

That tells you almost nothing.

---

**19. Deterministic startup on edge devices**

This roadmap cares about Jetson-class and embedded AI systems.

**Edge startup** is harder because:

- power may be unstable
- network may be unavailable
- local models may take time to load
- sensors may appear late
- audio devices may enumerate differently
- accelerators may need warmup
- storage may be slow
- device clocks may be wrong at boot

For an AI smart speaker or local assistant, deterministic startup might include:

```text
system boot
  -> audio device detected
  -> wake-word engine loaded
  -> local ASR model loaded
  -> TTS voice loaded
  -> tool registry loaded
  -> home devices paired
  -> memory store mounted
  -> network state detected
  -> cloud fallback optional
  -> readiness announced
```

If the microphone array is not ready, the assistant should not pretend it is listening.

If the smart-home controller is unavailable, device-control commands should be disabled.

Edge agent rule:

> Local AI products must know which capabilities are actually available after boot.

---

**20. Startup test plan**

Test startup like you test features.

**Test 1 - clean boot**

Expected:

- all startup phases pass
- readiness becomes true
- startup time is within budget

**Test 2 - missing config**

Remove a required environment variable.

Expected:

- startup fails early
- clear error message
- no traffic accepted

**Test 3 - missing tool**

Remove a required tool implementation.

Expected:

- tool registry phase fails
- service stays not ready

**Test 4 - broken vector index**

Point to the wrong document snapshot.

Expected:

- retrieval verification fails
- RAG disabled or startup fails depending on policy

**Test 5 - policy engine failure**

Load an invalid policy file.

Expected:

- policy validation fails
- high-risk tools never become available

**Test 6 - restart idempotency**

Start, stop, start again.

Expected:

- no duplicate jobs
- no duplicate memories
- no duplicate indexes
- same manifest version

**Test 7 - cold edge boot**

Reboot the device from power-off.

Expected:

- sensors and audio devices are detected
- local models load
- service reports real capability status

---

**21. Practical startup checklist**

Before you call an agent system production-ready, answer these questions.

**Configuration**

- Are all required config fields validated?
- Are unsafe defaults rejected in production?
- Are model, prompt, policy, and graph versions logged?

**Dependencies**

- Does startup verify every required dependency?
- Is degraded mode explicit?
- Are timeouts and retries configured?

**Tools**

- Is the tool registry explicit?
- Are unexpected tools rejected?
- Are high-risk tools bound to policies?
- Are tool schemas validated?

**Retrieval and memory**

- Is the vector index version checked?
- Is the embedding model version checked?
- Are memory migrations complete before serving?
- Are quarantined memories excluded?

**Runtime safety**

- Does the policy engine load before tools are usable?
- Does a block-policy self-test run?
- Are audit logs writable before readiness?

**Operations**

- Are `/livez` and `/readyz` separate?
- Is startup time measured?
- Is every startup phase logged?
- Is there a boot ID?
- Can the system restart safely?

---

**22. Common mistakes**

</details>

### 错误 1 - 就绪前就开始推理服务

进程启动，端口打开，在工具、内存或策略就绪之前，流量就已经开始。

修复：

> 在完整启动契约通过之前，保持 `/readyz` 为 false。

### 错误 2 - 惰性加载关键工具

第一个用户请求才发现某个工具是坏的。

修复：

> 在启动时加载并校验所需工具。

### 错误 3 - 依赖碰巧存在的文件

系统动态地发现 prompt、工具或配置，意外加载了实验性资产。

修复：

> 使用显式清单和允许列表。

### 错误 4 - 没有版本记录

系统无法判断是哪一版 prompt、策略、图、模型或索引导致了故障。

修复：

> 在启动时记录版本，并将其附加到请求 trace 上。

### 错误 5 - 把边缘启动当作普通服务器启动

当进程启动时，音频、传感器、本地模型和硬件设备可能尚未就绪。

修复：

> 增加硬件能力检查和特性级就绪。

---

## 23. 设计练习

为以下系统设计确定性启动：

> 一个本地 AI 工程助手运行在 Jetson 上。它支持语音输入、针对硬件文档的本地 RAG（检索增强生成）、项目文件夹内的代码编辑，以及通过 Zigbee 控制智能实验室设备。

创建一个启动清单，包含：

- 必需的环境变量
- 必需的本地设备
- 模型路径
- prompt 包版本
- 向量索引快照
- 工具注册表
- 策略包
- 就绪检查
- 降级模式规则

然后回答：

1. 哪些特性可以离线运行？
2. 哪些特性需要网络？
3. 哪些特性需要人工批准？
4. 哪些启动失败应该阻塞整个服务？
5. 哪些启动失败只应禁用单个特性？

---

## 关键要点

- 确定性启动意味着 agent 系统在接受流量之前，先进入已知良好的状态。
- 它不会让 LLM 输出变得确定性，而是让周边系统变得可预测。
- 启动应该基于阶段、有日志、有校验、可测试。
- 在就绪之前，应校验所需的工具、prompt、策略、内存、检索索引和模型客户端。
- `/livez` 和 `/readyz` 必须分开。
- 工具注册表应当显式且与策略绑定。
- RAG 启动必须校验正确的索引、快照和 embedding 模型。
- 内存启动必须处理 schema、过期、隔离区以及检查点兼容性。
- 边缘 AI 系统需要能力感知的启动，因为硬件和网络状态在启动时可能变化。
- 生产环境的 agent 应当尽早失败，而不是在半就绪状态下就开始推理服务。

---

## 参考文献

- [Kubernetes probes：liveness、readiness 和 startup probes](https://kubernetes.io/docs/concepts/configuration/liveness-readiness-startup-probes/)
- [Twelve-Factor App：Config](https://12factor.net/config)
- [OpenTelemetry](https://opentelemetry.io/)
- [OWASP Top 10 for Large Language Model Applications](https://owasp.org/www-project-top-10-for-large-language-model-applications/)

---

*下一篇：[Lecture 28](Lecture-28.md)


<details>
<summary>English original</summary>

**Mistake 1 - serving traffic before readiness**

The process starts, the port opens, and traffic begins before tools, memory, or policy are ready.

Fix:

> Keep `/readyz` false until the full startup contract passes.

**Mistake 2 - lazy-loading critical tools**

The first user request discovers that a tool is broken.

Fix:

> Load and validate required tools at startup.

**Mistake 3 - relying on whatever files are present**

The system discovers prompts, tools, or configs dynamically and accidentally loads experimental assets.

Fix:

> Use explicit manifests and allowlists.

**Mistake 4 - no version record**

The system cannot tell which prompt, policy, graph, model, or index version caused an incident.

Fix:

> Log versions at startup and attach them to request traces.

**Mistake 5 - treating edge boot as normal server boot**

Audio, sensors, local models, and hardware devices may not be ready when the process starts.

Fix:

> Add hardware capability checks and feature-level readiness.

---

**23. Design exercise**

Design deterministic startup for this system:

> A local AI engineering assistant runs on a Jetson. It supports voice input, local RAG over hardware docs, code editing inside a project folder, and smart-lab device control through Zigbee.

Create a startup manifest with:

- required environment variables
- required local devices
- model paths
- prompt bundle version
- vector index snapshot
- tool registry
- policy bundle
- readiness checks
- degraded mode rules

Then answer:

1. Which features can run offline?
2. Which features require network?
3. Which features require human approval?
4. Which startup failures should block the whole service?
5. Which startup failures should only disable one feature?

---

**Key takeaways**

- Deterministic startup means the agent system boots into a known-good state before accepting traffic.
- It does not make LLM outputs deterministic. It makes the surrounding system predictable.
- Startup should be phase-based, logged, validated, and testable.
- Required tools, prompts, policies, memory, retrieval indexes, and model clients should be verified before readiness.
- `/livez` and `/readyz` must be separate.
- Tool registries should be explicit and policy-bound.
- RAG startup must verify the right index, snapshot, and embedding model.
- Memory startup must handle schema, expiration, quarantine, and checkpoint compatibility.
- Edge AI systems need capability-aware startup because hardware and network state may vary at boot.
- A production agent should fail early rather than serve traffic in a half-ready state.

---

**References**

- [Kubernetes probes: liveness, readiness, and startup probes](https://kubernetes.io/docs/concepts/configuration/liveness-readiness-startup-probes/)
- [Twelve-Factor App: Config](https://12factor.net/config)
- [OpenTelemetry](https://opentelemetry.io/)
- [OWASP Top 10 for Large Language Model Applications](https://owasp.org/www-project-top-10-for-large-language-model-applications/)

---

*Next: [Lecture 28](Lecture-28.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-27.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-27.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
