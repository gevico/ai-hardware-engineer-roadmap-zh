---
title: 第 08 讲 - 生产环境 MCP：网关、注册表、评测与结课项目
description: 第 08 讲 - 生产环境 MCP：网关、注册表、评测与结课项目
---

# 第 08 讲 - 生产环境 MCP：网关、注册表、评测与结课项目

**合集：** [MCP for AI Agents](README.md) | **上一讲：** [← 第 07 讲](Lecture-07.md) | **下一讲：** [课程索引](README.md)

---

到此为止讲的一直都是*一个*服务器：把它写出来（第 05 讲）、用 OAuth 2.1 远程提供服务（第 06 讲）、对它做威胁建模（第 07 讲）。生产环境是另一回事，因为在生产环境中你不会只跑一个服务器——你跑二十个。一个真实的 agent 平台会连到 filesystem 服务器、GitHub 服务器、Postgres 服务器、三个内部 SaaS 封装、一个 Playwright 服务器，以及产品团队上周刚搭起来的随便什么东西。一旦超过一小把，两个问题就会压过其他一切：**如何把它们全都放到一个可信的统一入口之后**，以及**如何避免它们合起来的工具面把模型淹没**。

本讲就是那个世界的运维手册。我们会讲**组合与代理**——把服务器串成链，并把许多服务器聚合到一个端点之后——以及**MCP 网关**，它集中处理认证、策略、限流和可观测性，让每个服务器不必各自重新造一遍。我们会讲**工具上下文预算**：这个不起眼却决定成败的事实——把 200 个工具倒进模型上下文会劣化其工具选择、并抬高每次请求的 token 成本——以及修复它的命名空间化/过滤/精选。我们会讲**官方注册表**，即服务器发布和被发现的场所；决定你是否真的安装某个服务器的**信任信号**；以及你必须在每次 MCP 调用周围接上的**可观测性**。最后是 **MCPMark**——告诉你某个 agent 加服务器的组合是否达到生产可用而非仅 demo 可用的评测——以及把整门课收束成一个可交付产物的**结课项目**。

与本课程每一讲一样，事实都锚定 **2025-11-25** 稳定版规范，最后一节会详述 **2026-07-28 release candidate** 改了什么——因为无状态核心、MCP Apps 和 Tasks 扩展重塑的正是本讲所讲的部署故事。

---

## 学习目标

学完本讲，你应当能够：

- 解释组合、代理以及 **MCP 网关**的角色，并画出把 N 个服务器置于一个集中处理认证、策略、限流和可观测性的网关之后的结构。
- 诊断**工具上下文预算**问题，并施加正确的缓解手段——命名空间化、按 agent 划分工具子集、网关侧过滤/精选，或动态发现。
- 向**官方 MCP 注册表**发布，并从中发现；读懂那些能证明安装第三方服务器合理的**信任信号**（回扣第 07 讲）。
- 指明**每次 MCP 调用要记录什么日志**，并把它与 [`../Lectures/Lecture-23.md`](../01-讲座/Lecture-23.md) 的评估/可观测性纪律联系起来。
- 解读 **MCPMark** 结果，并用它判断某个 agent 加服务器的组合是否达到生产可用。
- 描述 **2026-07-28 RC** 解锁了什么——无状态核心、MCP Apps、Tasks、更严格的 OAuth/OIDC——并对照具体验收标准完成课程**结课项目**。

---


<details>
<summary>English original</summary>

**Lecture 08 - Production MCP: Gateways, Registry, Eval & Capstone**

**Collection:** [MCP for AI Agents](README.md) | **Previous:** [← Lecture 07](Lecture-07.md) | **Next:** [Course index](README.md)

---

Everything to this point has been about *one* server: write it (Lecture 05), serve it remotely with OAuth 2.1 (Lecture 06), and threat-model it (Lecture 07). Production is a different animal, because in production you do not run one server — you run twenty. A real agent platform reaches a filesystem server, a GitHub server, a Postgres server, three internal SaaS wrappers, a Playwright server, and whatever a product team stood up last week. The moment you have more than a handful, two problems dominate everything else: **how do you put them all behind one trustworthy front door**, and **how do you keep their combined tool surface from drowning the model**.

This lecture is the operations manual for that world. We cover **composition and proxying** — chaining servers and aggregating many behind one endpoint — and the **MCP gateway** that centralizes auth, policy, rate-limiting, and observability so each server does not reinvent them. We cover the **tool-context budget**: the unglamorous, decisive fact that dumping 200 tools into a model's context degrades its tool selection and inflates every request's token cost, and the namespacing/filtering/curation that fixes it. We cover the **official registry** as the place servers are published and discovered, the **trust signals** that decide whether you actually install one, and the **observability** you must wire around every MCP call. We close with **MCPMark** — the evaluation that tells you whether an agent-plus-server combination is production-ready rather than demo-ready — and a **capstone** that ties the whole course into one shippable artifact.

Like every lecture in this course, the facts pin to the **2025-11-25** stable spec, and the final section details what the **2026-07-28 release candidate** changes — because the stateless core, MCP Apps, and the Tasks extension reshape exactly the deployment story this lecture is about.

---

**Learning objectives**

By the end of this lecture you should be able to:

- Explain composition, proxying, and the role of an **MCP gateway**, and draw N servers behind one gateway that centralizes auth, policy, rate-limiting, and observability.
- Diagnose the **tool-context budget** problem and apply the right mitigation — namespacing, per-agent tool subsets, gateway-side filtering/curation, or dynamic discovery.
- Publish to and discover from the **official MCP registry**, and read the **trust signals** that justify installing a third-party server (tying back to Lecture 07).
- Specify what to **log per MCP call** and connect it to the evaluation/observability discipline of [`../Lectures/Lecture-23.md`](../01-讲座/Lecture-23.md).
- Interpret an **MCPMark** result and use it to decide whether an agent+server combination is production-ready.
- Describe what the **2026-07-28 RC** unlocks — stateless core, MCP Apps, Tasks, tighter OAuth/OIDC — and execute the course **capstone** against concrete acceptance criteria.

---

</details>

## 1. 扩展到单台服务器之外：组合、代理与网关

服务器有两种截然不同的组合方式，把二者混为一谈会导致实实在在的设计错误。

**组合（chaining）** 是指一个服务器本身就是另一个服务器的 *client*。你的 “deploy” 服务器调用 “GitHub” 服务器来开 PR，再调用 “Slack” 服务器来宣布这件事。host 只看到一个服务器；其背后则是一小张它无从知晓的 MCP 连接图。组合让你能从低层能力构建出高层能力，而无需让 host 了解每一个叶子。

**代理 / 聚合** 是指一个端点*前置*多个服务器，并把它们的 tools、resources 和 prompts 重新暴露出来，如同这些本就是它自己的一样。host 只打开一条连接，就能看到代理背后所有内容的并集。这正是能扩展 agent 平台的模式：host 不再需要管理二十个 Client 和二十套 auth 配置，而只管理一个。

**MCP 网关** 是一个把生产级关注点都加装上去的聚合代理。它是一群服务器的唯一前门，并把四件你不希望重复实现二十遍的事情集中起来：

- **Auth** — 在一处终止 OAuth 2.1（第 06 讲）、校验 audience-bound token，并把调用方映射到其被允许访问的服务器与工具。后端服务器可以信任由网关签发的短期内部凭证，而不必各自处理最终用户的 OAuth。
- **策略** — 允许 / 拒绝某个给定 agent 或租户可以调用哪些工具，强制参数约束，并在一处卡点上应用数据外发规则。
- **限流** — 按租户、按工具、按 token 的配额，使一个失控的 agent 无法耗尽下游 API 或你的账单。
- **可观测性** — 每次调用都流经网关，因此它是记录工具名、延迟、错误和 token 成本（第 4 节）的天然位置。

```text
                          ┌───────────────────────────────────────────┐
        OAuth 2.1 token   │                MCP GATEWAY                  │
   AGENT ───────────────► │  ┌─────────┬─────────┬───────────┬──────┐  │
   (one client,           │  │  AUTH   │ POLICY  │ RATE-LIMIT │ OBS  │  │
    one connection)       │  │ (L06)   │ allow/  │ per-tenant │ log  │  │
                          │  │ verify  │ deny    │ /per-tool  │ every│  │
                          │  │ audience│ tools   │ quotas     │ call │  │
                          │  └─────────┴─────────┴───────────┴──────┘  │
                          │     curated, namespaced tool surface        │
                          └───┬──────────┬──────────┬──────────┬───────┘
                              │ internal │ creds     │          │
                              ▼          ▼           ▼          ▼
                        ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐
                        │ github  │ │ postgres│ │ files   │ │ slack   │
                        │ server  │ │ server  │ │ server  │ │ server  │
                        └─────────┘ └─────────┘ └─────────┘ └─────────┘
```

网关*并没有*消解第 02 讲中的 1:1 Client↔Server 规则 —— 它只是把这套规则挪了个位置。host 仍然是针对一个端点（网关）运行一个 Client；网关则为每个后端服务器运行一个 Client。该不变量在每一跳上都成立；网关不过是在一个进程里同时充当 host 与 server。

---


<details>
<summary>English original</summary>

**1. Scaling beyond one server: composition, proxying & the gateway**

There are two distinct ways servers combine, and conflating them causes real design mistakes.

**Composition (chaining)** is when one server is itself the *client* of another. Your "deploy" server calls a "GitHub" server to open a PR, then a "Slack" server to announce it. The host sees one server; behind it sits a small graph of MCP connections it never learns about. Composition is how you build higher-level capabilities out of lower-level ones without teaching the host about every leaf.

**Proxying / aggregation** is when one endpoint *fronts* many servers and re-exposes their tools, resources, and prompts as if they were its own. The host opens a single connection and sees the union of everything behind the proxy. This is the pattern that scales an agent platform: instead of the host managing twenty Clients with twenty auth configs, it manages one.

An **MCP gateway** is an aggregating proxy with the production concerns bolted on. It is the single front door for a fleet of servers, and it centralizes the four things you do not want re-implemented twenty times:

- **Auth** — one place to terminate OAuth 2.1 (Lecture 06), validate the audience-bound token, and map the caller to the servers and tools they are allowed to reach. Backend servers can trust a short-lived internal credential the gateway mints rather than handling end-user OAuth each.
- **Policy** — allow/deny which tools a given agent or tenant may call, enforce argument constraints, and apply data-egress rules at one choke point.
- **Rate-limiting** — per-tenant, per-tool, per-token quotas, so one runaway agent cannot exhaust a downstream API or your bill.
- **Observability** — every call flows through the gateway, so it is the natural place to log tool name, latency, errors, and token cost (Section 4).

```text
                          ┌───────────────────────────────────────────┐
        OAuth 2.1 token   │                MCP GATEWAY                  │
   AGENT ───────────────► │  ┌─────────┬─────────┬───────────┬──────┐  │
   (one client,           │  │  AUTH   │ POLICY  │ RATE-LIMIT │ OBS  │  │
    one connection)       │  │ (L06)   │ allow/  │ per-tenant │ log  │  │
                          │  │ verify  │ deny    │ /per-tool  │ every│  │
                          │  │ audience│ tools   │ quotas     │ call │  │
                          │  └─────────┴─────────┴───────────┴──────┘  │
                          │     curated, namespaced tool surface        │
                          └───┬──────────┬──────────┬──────────┬───────┘
                              │ internal │ creds     │          │
                              ▼          ▼           ▼          ▼
                        ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐
                        │ github  │ │ postgres│ │ files   │ │ slack   │
                        │ server  │ │ server  │ │ server  │ │ server  │
                        └─────────┘ └─────────┘ └─────────┘ └─────────┘
```

The gateway does *not* dissolve the 1:1 Client↔Server rule from Lecture 02 — it relocates it. The host still runs one Client to one endpoint (the gateway); the gateway runs one Client per backend server. The invariant holds at every hop; the gateway is simply a host-and-server in one process.

---

</details>

## 2. 工具上下文预算

这就是那种没人会提醒你、直到它咬你一口才浮现的失效模式：服务器聚合到一定规模，agent 反而变得*更差*，而不是更好。服务器暴露的每个工具，都会在每一轮列出工具的请求里，把它的名称、描述和完整 input schema 送进模型的上下文。二十台服务器、平均每台十个工具，就是 200 个工具定义 —— 轻松达到数万 token —— 在用户开口之前就已经摆在模型面前。

这笔开销要付两次。**token 成本**是显而易见的税：只要请求携带这些定义就会被计费，而且它们挤占了 agent 真正需要的工作上下文。**选择准确率**上的税更重、也更隐蔽：模型要在 200 个近乎重复的工具中做选择（三个不同的 `search` 工具、两个 `create_issue` 变体），要么选错，要么选中被遮蔽的那个，要么反复摇摆。过了某个点之后，工具更多并不等于能力更强 —— 那只是一个 prompt 更大、分类更差的模型。

这里需要的纪律是：把对外暴露的工具面当作**一笔需要刻意花掉的预算**，而不是一堆慢慢堆积的东西。花这笔预算的地方就是 gateway，因为 gateway 是策展者，它决定每个 agent 实际看到 fleet 的哪一部分。

| 策略 | 作用 | 何时采用 |
| --- | --- | --- |
| **命名空间隔离** | 按服务器给工具加前缀（`github.create_issue`、`postgres.query`），使命名冲突不可能发生、来源一目了然 | 始终 —— 最便宜的修法；在 gateway 上默认就做 |
| **按 agent 划分工具子集** | 只暴露某个 agent/角色需要的工具；分诊 agent 不给写工具 | 当一个 fleet 服务多种 agent 角色时 |
| **工具过滤 / 策展** | gateway 对工具做 allowlist/denylist，丢弃重复项，精简冗长的描述 | 当 backend 服务器暴露的工具超出 agent 该调用的范围时 |
| **动态发现** | 从一个小核心集开始；只在任务需要时按需拉取更多工具（search-a-tool，或 "load server X" 元工具） | 当*目录*很大，但任一单个任务只用少数工具时 |

经验法则：agent 应当只看到**能让它完成任务的最小工具集**，负责落实这一点的是 gateway，而不是模型。如果你发现自己开始琢磨“模型怎么从这一堆里做选择”，那说明你已经暴露过度了 —— 在上游把工具面收窄。（这是“结构化工具胜过 computer use”在生产规模上的回响：更少、更锋利、边界清晰的工具胜过最大化的工具面，在协议层面如此，在单 agent 层面同样如此。）

---


<details>
<summary>English original</summary>

**2. The tool-context budget**

Here is the failure mode nobody warns you about until it bites: aggregate enough servers and the agent gets *worse*, not better. Every tool a server exposes contributes its name, description, and full input schema to the model's context on every turn that lists tools. Twenty servers averaging ten tools each is 200 tool definitions — easily tens of thousands of tokens — sitting in front of the model before the user has said anything.

This costs you twice. **Token cost** is the obvious tax: those definitions are billed on every request that carries them, and they crowd out the working context the agent actually needs. The **selection-accuracy** tax is worse and less visible: a model choosing among 200 near-duplicate tools (three different `search` tools, two `create_issue` variants) picks wrong, picks the shadowed one, or thrashes. More tools is not more capable past a point — it is a worse classifier with a bigger prompt.

The discipline is to treat the exposed tool surface as a **budget you spend deliberately**, not a pile you accumulate. The gateway is where you spend it, because the gateway is the curator that decides which slice of the fleet each agent actually sees.

| Strategy | What it does | When to reach for it |
| --- | --- | --- |
| **Namespacing** | Prefix tools by server (`github.create_issue`, `postgres.query`) so collisions are impossible and provenance is legible | Always — the cheapest fix; do it by default at the gateway |
| **Per-agent tool subsets** | Expose only the tools a given agent/role needs; a triage agent does not get write tools | When one fleet serves many agent roles |
| **Tool filtering / curation** | Gateway allowlists/denylists tools, drops duplicates, trims verbose descriptions | When backend servers expose more than the agent should ever call |
| **Dynamic discovery** | Start with a small core set; fetch more tools on demand (search-a-tool, or a "load server X" meta-tool) only when the task needs them | When the *catalog* is large but any one task uses few tools |

The rule of thumb: an agent should see the **smallest tool set that lets it finish its tasks**, and the gateway is responsible for enforcing that, not the model. If you find yourself reasoning about "how does the model pick among all these," you have already over-exposed — narrow the surface upstream. (This is the production-scale echo of "structured tools beat computer use": fewer, sharper, well-scoped tools beat a maximal surface, at the protocol level just as at the single-agent level.)

---

</details>

## 3. registry：发布、发现与信任

到 2026 年，**官方 MCP registry** 已收录 **2,000 多个社区服务器**，它也是服务器被*发布*与*发现*的权威场所——MCP 世界的 npm/PyPI。发布是指提交一个服务器及其元数据：名称、命名空间、描述、它所使用的 transport、它暴露的 tools/resources/prompts，以及它的源码。发现则是指检索该目录，而不是在 Slack 里互相传 GitHub 链接。

可发现性不等于信任。registry 中的一条记录只说明某个服务器*存在*，并不说明把 agent 指向它是安全的——而第 07 讲整篇讲的就是这一区别为何重要。服务器的工具描述会被直接注入模型的上下文，因此恶意描述就是一条 prompt injection 向量（tool poisoning），而被攻陷的依赖则是一次供应链入侵，背后还挂着你 agent 的凭证。registry 是一个发现面，攻击者可以像任何人一样轻松地往上面发布。

因此，在安装第三方服务器之前，先看它的**信任信号**：

| 信号 | 它告诉你什么 | 危险信号 |
| --- | --- | --- |
| **发布者 / 命名空间归属** | 声称拥有 `github.*` 的组织是否真的拥有它（已验证的命名空间） | 未验证的命名空间冒充已知厂商 |
| **源码与构建溯源** | 可审计的开源；可复现、已签名的构建 | 闭源二进制，无源码，无签名 |
| **版本固定** | 可以固定到确切版本，而不是浮动在 `latest` 上 | 只有会移动的 tag；行为可能在你脚下改变 |
| **签名 / 完整性** | 产物已签名，可对照发布者验证 | 无签名；你无法证明你运行的就是他们发布的 |
| **维护与采用度** | 近期有 commit、issue 响应及时、有真实的安装基数 | 已被弃置，或刚发布且描述好得过分 |

在操作上：**固定具体版本、验证其签名，并在首次运行前审计工具描述与权限**——这正是第 07 讲所规定的供应链卫生做法，只不过应用在 registry 边界上。对待一个未固定版本、未签名、命名空间未验证的服务器，就要像对待陌生人给的 `curl | sudo bash` 一样，因为从 agent 的角度看，安装它就等同于这件事。对于内部 agent 依赖的任何东西，与其去访问你无法控制的第三方主机，不如在你自己的网关后面跑一份自己的副本。

---

## 4. 可观测性：记录每一次 MCP 调用

没有度量就无法运维、评估或调试，而驱动 MCP 工具的 agent 是一个分布式系统，其最值得注意的故障恰恰发生在调用*之间*。不可妥协的基线是**记录每一次 MCP 调用**——而网关（第 1 节）是天然的落点，因为每一次调用本来就会流经它。

对每一次工具调用，至少捕获：

| 字段 | 为何重要 |
| --- | --- |
| **工具名**（带命名空间） | 跑的是哪项能力；可以看出选择模式，以及从来没被调用过的死工具 |
| **参数大小**（以及脱敏/哈希后的形态——绝不能是原始密钥） | 发现超大或畸形输入；在不泄露数据的前提下关联成本 |
| **延迟** | 慢工具通常就是拖垮 agent 墙钟耗时的那一个；找到你的尾部 |
| **结果 / 错误** | 成功还是失败、错误类别，以及 agent 是恢复了还是放弃了 |
| **Token 成本** | 该调用的定义 + 结果消耗的 token；把工具面（第 2 节）和账单挂上钩 |
| **会话 / trace ID** | 把一个 agent 任务跨其 15–20 次调用缝合进同一条 trace |

```python
import time, logging

log = logging.getLogger("mcp.audit")

async def traced_call(gateway, session_id, tool, args):
    t0 = time.perf_counter()
    err = None
    try:
        return await gateway.call_tool(tool, args)
    except Exception as e:
        err = type(e).__name__
        raise
    finally:
        log.info("mcp_call", extra={
            "session_id": session_id,
            "tool": tool,                 # namespaced, e.g. "github.create_issue"
            "args_bytes": len(repr(args)), # size, not contents
            "latency_ms": round((time.perf_counter() - t0) * 1000, 1),
            "error": err,                  # None on success
            # token_cost filled from the model-call accounting around this turn
        })
```

这份逐调用日志，正是 [`../Lectures/Lecture-23.md`](../01-讲座/Lecture-23.md) 中所展开的评估与可观测性方法论的原材料：trace 成为你据以评估的数据集，延迟与 token 成本分布成为你的 SLO，而错误/恢复字段正是 MCPMark 这类 benchmark 所评的东西。可观测性不是评估的附加项——它是评估的基底。

---


<details>
<summary>English original</summary>

**3. The registry: publishing, discovery & trust**

By 2026 the **official MCP registry** lists **more than 2,000 community servers**, and it is the canonical place servers are *published* and *discovered* — the npm/PyPI of the MCP world. Publishing means submitting a server with its metadata: name, namespace, description, the transport it speaks, the tools/resources/prompts it exposes, and its source. Discovery means searching that catalog rather than trading GitHub links in Slack.

Discoverability is not trust. A registry entry tells you a server *exists*, not that it is safe to point an agent at — and Lecture 07 is the whole reason that distinction matters. A server's tool descriptions are injected straight into your model's context, so a malicious description is a prompt-injection vector (tool poisoning), and a compromised dependency is a supply-chain breach with your agent's credentials behind it. The registry is a discovery surface that an attacker can publish to as easily as anyone else.

So before you install a third-party server, read its **trust signals**:

| Signal | What it tells you | Red flag |
| --- | --- | --- |
| **Publisher / namespace ownership** | Whether the org claiming `github.*` actually owns it (verified namespace) | Unverified namespace impersonating a known vendor |
| **Source & build provenance** | Open source you can audit; reproducible, signed builds | Closed binary, no source, no signature |
| **Version pinning** | You can pin an exact version rather than floating `latest` | Only a moving tag; behavior can change under you |
| **Signature / integrity** | The artifact is signed and verifiable against the publisher | No signature; you cannot prove what you ran is what they shipped |
| **Maintenance & adoption** | Recent commits, responsive issues, real install base | Abandoned, or freshly published with a too-good description |

Operationally: **pin a specific version, verify its signature, and audit the tool descriptions and permissions before first run** — exactly the supply-chain hygiene Lecture 07 prescribes, applied at the registry boundary. Treat an unpinned, unsigned, unverified-namespace server the way you would treat `curl | sudo bash` from a stranger, because in agent terms that is what installing it is. For anything an internal agent depends on, prefer running your own copy behind your gateway over reaching a third-party host you do not control.

---

**4. Observability: log every MCP call**

You cannot operate, evaluate, or debug what you do not measure, and an agent driving MCP tools is a distributed system whose most interesting failures happen *between* the calls. The non-negotiable baseline is to **log every MCP call** — and the gateway (Section 1) is the natural place to do it, because every call already flows through it.

For each tool invocation, capture at minimum:

| Field | Why it matters |
| --- | --- |
| **Tool name** (namespaced) | Which capability ran; lets you see selection patterns and dead tools nothing ever calls |
| **Arguments size** (and a redacted/hashed shape — never raw secrets) | Spot oversized or malformed inputs; correlate cost without leaking data |
| **Latency** | The slow tool is usually the one wrecking the agent's wall-clock; find your tail |
| **Outcome / error** | Success vs failure, error class, and whether the agent recovered or gave up |
| **Token cost** | Tokens the call's definition + result spent; ties tool surface (Section 2) to the bill |
| **Session / trace ID** | Stitch a single agent task across its 15–20 calls into one trace |

```python
import time, logging

log = logging.getLogger("mcp.audit")

async def traced_call(gateway, session_id, tool, args):
    t0 = time.perf_counter()
    err = None
    try:
        return await gateway.call_tool(tool, args)
    except Exception as e:
        err = type(e).__name__
        raise
    finally:
        log.info("mcp_call", extra={
            "session_id": session_id,
            "tool": tool,                 # namespaced, e.g. "github.create_issue"
            "args_bytes": len(repr(args)), # size, not contents
            "latency_ms": round((time.perf_counter() - t0) * 1000, 1),
            "error": err,                  # None on success
            # token_cost filled from the model-call accounting around this turn
        })
```

That per-call log is the raw material for the evaluation and observability discipline developed in [`../Lectures/Lecture-23.md`](../01-讲座/Lecture-23.md): traces become the dataset you evaluate over, latency and token-cost distributions become your SLOs, and the error/recovery field is exactly what a benchmark like MCPMark scores. Observability is not an add-on to evaluation — it is its substrate.

---

</details>

## 5. 用 MCPMark 评估

一个能通过 MCP Inspector、能回答 happy-path 问题的服务器只是一个 *demo*。生产环境会问一个更难的问题：当 agent 驱动这个服务器去完成一个真实的多步任务——而任务中途出了岔子，这总是会发生——它还能恢复并跑完吗？这正是 **MCPMark** 要衡量的。

MCPMark 是由专家整理的 benchmark，包含 **127 个任务**，覆盖 **Notion、GitHub、Postgres、Filesystem 和 Playwright**——都是真实系统，不是玩具桩。它的标志性特征是深度：任务平均 **16.2 轮**、**17.4 次工具调用**，并且被刻意构造成 agent 必须**串联操作、读取中间状态、从失败中恢复**才能成功。任务不是“调用一个工具”，而是“在一个真实工作空间里导航到一个目标”，评分奖励的是达成目标，包括走错路之后达成。

正是这种设计让它能预测生产行为。平均 16 轮意味着 MCPMark 压测的正是第 2 节和第 4 节所讨论的东西——agent 在深入长上下文之后是否仍能正确选择，工具延迟是否会累积成不可用的墙钟时间，以及一次失败的调用是死路还是可恢复的一步。单次调用的工具调用 benchmark 看不到其中任何一点。

把它当作 **go/no-go 门禁**，而不是虚荣指标。具体来说：

- 针对你具体的 **agent + 服务器（或网关）组合**跑对应的 MCPMark 切片——分数是这一对的属性，而不是服务器单独的属性。
- 读 **failure-recovery** 行为，而不只是通过率：在第 3 轮失败且再也恢复不了的任务，说明你的工具或工具描述有问题；游荡 30 轮的任务，说明你的工具上下文预算有问题。
- 把这次运行当作 **回归覆盖**：当你改动工具面、换模型或升级后端服务器时重跑它，并以此为部署门禁。

MCPMark 位于 [`../Lectures/Lecture-23.md` §8](../01-讲座/Lecture-23.md) 所编目的更宏大的 benchmark 阶梯之中——从单工具正确性一路递进到长时程、多系统的智能体化任务。这个阶梯是你为所要主张的结论选择*正确*评估的方式；MCPMark 是回答“这个 agent 加服务器的组合是否已准备好承担真实的多步工作？”的那一级。

---

## 6. 2026 路线图：2026-07-28 release candidate

**2026-07-28 release candidate** 是 MCP 发布以来最大的一次修订，它改变了整讲所描述的生产故事。其中四点很重要，每一点都解锁了具体的东西。

| RC 特性 | 它是什么 | 它为生产解锁了什么 |
| --- | --- | --- |
| **无状态核心** | 一种核心协议模式，不携带任何必需的按会话服务器状态 | 在普通 HTTP / serverless 上扩展——任何实例都能处理任何请求，无需粘性路由，无需共享会话存储；网关集群横向扩展变得轻而易举 |
| **MCP Apps** | 服务器渲染的**交互式 UI**，而不只是文本/数据结果 | 工具可以返回一个真实 UI 界面（表单、图表、确认控件），由 host 渲染——比 JSON blob 更丰富，人在回路交互内联其中 |
| **Tasks 扩展** | 一等公民的**长时间运行 / 异步**工作 | 工具可以发起比单个请求活得更久的工作——长时间的迁移、爬取、构建——agent 轮询/等待其完成，而不是一直占着连接 |
| **更严格的 OAuth/OIDC** | 与标准 OAuth 2.1 / OIDC 更紧密的对齐 | 与企业身份提供商集成更干净；第 06 讲的认证方案在网关处变得更标准、更少定制 |

其中两项解决了本课程早前提出的矛盾。**无状态核心**是对第 02 讲指出的扩展摩擦的解答——按会话有状态的服务器需要亲和性或共享存储，而无状态模式彻底移除了这一要求，这正是让网关前置的集群扩展成本低廉的原因。**Tasks** 扩展弥合了“工具调用在几秒内返回”与“真实工作要花几分钟”之间的鸿沟——没有它，长任务只能靠轮询工具和外部状态来伪装异步。

与规范 RC 同步，官方 **`mcp` SDK 正在迈向 v2**（整个 2026 年处于 beta），因此第 05 讲的 FastMCP 接口会有变化。在 RC 获批之前，把这一切都当作**即将到来**：今天基于 **2025-11-25** 构建，并把你的网关和服务器设计成以后采用无状态核心和 Tasks 只是一次配置变更，而不是重写。

---


<details>
<summary>English original</summary>

**5. Evaluation with MCPMark**

A server that passes MCP Inspector and answers a happy-path question is a *demo*. Production asks a harder question: when an agent drives this server across a real, multi-step task — and something goes wrong mid-task, as it always does — does it recover and finish? That is what **MCPMark** measures.

MCPMark is an expert-curated benchmark of **127 tasks** spanning **Notion, GitHub, Postgres, Filesystem, and Playwright** — real systems, not toy stubs. Its defining characteristic is depth: the tasks average **16.2 turns** and **17.4 tool calls** each, and they are deliberately constructed so that an agent must **chain operations, read intermediate state, and recover from failure** to succeed. A task is not "call one tool"; it is "navigate a real workspace to a goal," and the scoring rewards getting there, including after a wrong turn.

That design is exactly why it predicts production behavior. The 16-turn average means MCPMark stresses the very things Sections 2 and 4 are about — whether the agent can still select correctly deep into a long context, whether tool latency compounds into an unusable wall-clock, and whether a failed call is a dead end or a recoverable step. A single-shot tool-calling benchmark cannot see any of that.

Use it as a **go/no-go gate**, not a vanity number. Concretely:

- Run the relevant MCPMark slice against your specific **agent + server (or gateway) combination** — the score is a property of the pair, not the server alone.
- Read the **failure-recovery** behavior, not just pass rate: tasks that fail on turn 3 and never recover indict your tools or descriptions; tasks that wander for 30 turns indict your tool-context budget.
- Treat the run as **regression coverage**: re-run it when you change the tool surface, swap a model, or upgrade a backend server, and gate the deploy on it.

MCPMark sits inside the broader benchmark ladder catalogued in [`../Lectures/Lecture-23.md` §8](../01-讲座/Lecture-23.md) — the progression from single-tool correctness up to long-horizon, multi-system agentic tasks. The ladder is how you choose the *right* evaluation for the claim you are making; MCPMark is the rung that answers "is this agent-plus-server combination ready to carry real, multi-step work?"

---

**6. The 2026 roadmap: the 2026-07-28 release candidate**

The **2026-07-28 release candidate** is the largest revision since MCP launched, and it changes the production story this whole lecture describes. Four pieces matter, each unlocking something concrete.

| RC feature | What it is | What it unlocks for production |
| --- | --- | --- |
| **Stateless core** | A core protocol mode that carries no required per-session server state | Scale on ordinary HTTP / serverless — any instance handles any request, no sticky routing, no shared session store; the gateway fleet becomes trivially horizontal |
| **MCP Apps** | Server-rendered **interactive UIs**, not just text/data results | A tool can return a real UI surface (a form, a chart, a confirmation widget) the host renders — richer than a JSON blob, with the human-in-the-loop interaction inline |
| **Tasks extension** | First-class **long-running / async** work | A tool can kick off work that outlives one request — a long migration, a crawl, a build — and the agent polls/awaits completion instead of holding a connection open |
| **Tighter OAuth/OIDC** | Closer alignment with standard OAuth 2.1 / OIDC | Cleaner integration with enterprise identity providers; the Lecture 06 auth story gets more standard and less bespoke at the gateway |

Two of these resolve tensions raised earlier in the course. The **stateless core** is the answer to the scaling friction Lecture 02 flagged — a per-session-stateful server needs affinity or a shared store, and the stateless mode removes the requirement entirely, which is what makes a gateway-fronted fleet cheap to scale. The **Tasks** extension closes the gap between "a tool call returns in seconds" and "real work takes minutes" — without it, long jobs force you to fake async with polling tools and external state.

Alongside the spec RC, the official **`mcp` SDK is heading to v2** (in beta through 2026), so the FastMCP surfaces from Lecture 05 will shift. Treat all of this as **forthcoming until the RC is ratified**: build against **2025-11-25** today, and design your gateway and servers so that adopting the stateless core and Tasks later is a configuration change, not a rewrite.

---

</details>

## 7. Capstone：交付生产级 MCP 服务器

这就是整门课程浓缩成的一个产物。你将把一个服务器从空白文件做到另一个团队可以信任、注册并依赖的程度——此前每一讲都会恰好出现一次。选一个真实领域（内部 API 的封装、精选数据库、文档存储）；评分看的是工程，不是新意。

**Step 1 — 构建服务器（第 05 讲）。** 用官方 SDK / FastMCP 实现至少 **三个 tool、两个 resource 和一个 prompt**。tool 携带完整、准确的 **JSON-Schema** 输入与结构化输出；resource 以 URI 寻址；prompt 是可复用的模板。在 **MCP Inspector** 中把它跑绿。

**Step 2 — 用 OAuth 2.1 远程提供（第 06 讲）。** 把它放在符合规范的 **OAuth 2.1** 之后，通过 **Streamable HTTP**（而非已废弃的 SSE 传输）暴露：资源服务器元数据（RFC 9728）、**带 scope、绑定 audience 的 token**，以及认证服务器/资源服务器的拆分。没有有效且 audience 正确的 token 的请求一律拒绝。

**Step 3 — 做威胁建模并加固（第 07 讲）。** 写一份一页的威胁模型，列出相关攻击——**tool poisoning、经由 tool 结果的 prompt injection、confused deputy、token passthrough、supply-chain**——以及阻止每种攻击的具体控制措施。确认你**没有**把 token 透传给下游服务，且 tool 描述是干净的。

**Step 4 — 发布到 registry（第 3 节）。** 提交服务器，附带正确的元数据、**已验证的 namespace**、**固定版本**和**已签名**的产物。记录使用方在安装前应检查的信任信号。

**Step 5 — 把它放在网关之后（第 1–2 节）。** 在它前面放一个**网关**，网关后面还有至少一个其他服务器，由网关集中认证、施加**策略**（允许/拒绝 tool）、执行**限流**，并**记录每一次调用**（第 4 节）。给 tool 加 **namespace**，只向 agent 暴露**精选子集**——证明你没有超出 tool 上下文预算。

**Step 6 — 对它做评估（第 5 节）。** 运行一次 **MCPMark 风格的评估**，让 agent 驱动你的服务器（或用最接近的 MCPMark 切片加上你自己的一些领域任务）。报告**通过率、平均轮次/tool 调用次数、失败恢复行为、延迟和 token 成本**，并给出带理由的 **go/no-go** 结论。

**验收标准**——以下全部成立时才算完成：

| # | 标准 | 证据 |
| --- | --- | --- |
| 1 | 服务器暴露 ≥3 个 tool、≥2 个 resource、≥1 个 prompt，且 schema 全部正确 | MCP Inspector 显示为绿；schema 通过校验 |
| 2 | 只能经由 OAuth 2.1 之后的 Streamable HTTP 访问 | 未认证 / audience 错误的请求 → 被拒；带 scope 的 token → 成功 |
| 3 | 威胁模型把每种列出的攻击映射到一项控制措施 | 一页文档；无 token 透传；tool 描述已审计 |
| 4 | 已发布到 registry，namespace 已验证，版本固定且已签名 | registry 条目；使用方可以固定到确切的已签名版本 |
| 5 | 运行在网关之后，且网关后还有 ≥1 个其他服务器 | 集中认证 + 策略 + 限流；加 namespace、经精选的 tool 面 |
| 6 | 每次 MCP 调用都有日志 | trace 显示每次调用的 tool、参数大小、延迟、错误、token 成本 |
| 7 | 报告 MCPMark 风格的评估并给出结论 | 通过率 + 轮次/tool 调用 + 恢复 + 成本；明确的 go/no-go |

如果你能完成全部七项，你就做出了这门课程存在的意义所在的东西：不是一个 agent 调用一次就完的 tool，而是一个**别人可以大规模安全依赖**的 MCP 服务器。

---

## 时效说明

本讲内容截至 **2026 年 6 月**，对齐最新稳定版 MCP 规范 **2025-11-25**——即今天构建生产级服务器与网关所应对齐的修订版。规模数字（官方 registry 收录 **2,000+ 个服务器**，`mcp` 包月下载量约 97M）以及 **MCPMark** 规范（跨 Notion/GitHub/Postgres/Filesystem/Playwright 的 **127 个任务**，平均 **16.2 轮 / 17.4 次 tool 调用**，对失败恢复给予奖励）反映的是 2026 年初的报告；在评审中引用任何数字之前，请重新拉取 MCPMark 的任务列表与评分。**2026-07-28 候选版本**是发布以来最大的一次修订——**无状态核心**（可在普通 HTTP/serverless 上扩展，无需会话亲和）、**MCP Apps**（服务器渲染的交互式 UI）、**Tasks** 扩展（长时运行/异步工作），以及更紧密的 **OAuth/OIDC** 对齐——并且官方 **`mcp` SDK 正走向 v2**（2026 年全年为 beta）；在 RC 获批之前，这些都应视为尚未定稿，并且设计时要让采用它们只是改配置，而不是重写。传输层现状：**stdio** 与 **Streamable HTTP** 是当前的；**HTTP+SSE** 自 **2025-03-26** 起已废弃。网关产品、registry 工具链和 SDK 接口每个版本都在变——把这里的任何代码当作快照，并对照你安装的版本核实；上面关于协议和 benchmark 的事实才是稳定的基础。


<details>
<summary>English original</summary>

**7. Capstone: ship a production-grade MCP server**

This is the course in one artifact. You will take a server from a blank file to something another team can trust, register, and depend on — every prior lecture shows up exactly once. Pick a real domain (a wrapper over an internal API, a curated database, a document store); the grading is on engineering, not novelty.

**Step 1 — Build the server (Lecture 05).** Implement at least **three tools, two resources, and one prompt** with the official SDK / FastMCP. Tools carry complete, accurate **JSON-Schema** inputs and structured output; resources are URI-addressed; the prompt is a reusable template. Demonstrate it green in **MCP Inspector**.

**Step 2 — Serve it remotely with OAuth 2.1 (Lecture 06).** Expose it over **Streamable HTTP** (not the deprecated SSE transport) behind spec-compliant **OAuth 2.1**: resource-server metadata (RFC 9728), **scoped, audience-bound tokens**, and the auth-server/resource-server split. A request without a valid, correctly-audienced token is rejected.

**Step 3 — Threat-model and harden it (Lecture 07).** Write a one-page threat model naming the relevant attacks — **tool poisoning, prompt injection via tool results, confused deputy, token passthrough, supply-chain** — and the specific control that stops each. Confirm you do **not** pass tokens through to downstream services and that tool descriptions are clean.

**Step 4 — Publish to the registry (Section 3).** Submit the server with correct metadata, a **verified namespace**, a **pinned version**, and a **signed** artifact. Document the trust signals a consumer should check before installing it.

**Step 5 — Put it behind a gateway (Sections 1–2).** Front it with at least one other server behind a **gateway** that centralizes auth, applies a **policy** (allow/deny tools), enforces a **rate limit**, and **logs every call** (Section 4). **Namespace** the tools and expose only a **curated subset** to the agent — demonstrate you stayed inside the tool-context budget.

**Step 6 — Evaluate it (Section 5).** Run an **MCPMark-style evaluation** of an agent driving your server (or the closest MCPMark slice plus a few domain tasks of your own). Report **pass rate, average turns/tool calls, failure-recovery behavior, latency, and token cost**, and state a **go/no-go** verdict with the reasoning.

**Acceptance criteria** — you are done when all of these hold:

| # | Criterion | Evidence |
| --- | --- | --- |
| 1 | Server exposes ≥3 tools, ≥2 resources, ≥1 prompt, all schema-correct | MCP Inspector shows them green; schemas validate |
| 2 | Reachable only over Streamable HTTP behind OAuth 2.1 | Unauthenticated / wrong-audience request → rejected; scoped token → succeeds |
| 3 | Threat model maps each named attack to a control | One-page doc; no token passthrough; tool descriptions audited |
| 4 | Published to the registry, verified namespace, pinned + signed | Registry entry; a consumer can pin the exact signed version |
| 5 | Runs behind a gateway with ≥1 other server | Centralized auth + policy + rate limit; namespaced, curated tool surface |
| 6 | Every MCP call is logged | Trace shows tool, args size, latency, error, token cost per call |
| 7 | MCPMark-style evaluation reported with a verdict | Pass rate + turns/tool-calls + recovery + cost; explicit go/no-go |

If you can do all seven, you have built the thing this course exists to teach: not a tool an agent can call once, but an MCP server **other people can safely depend on at scale**.

---

**Current as of**

This lecture is current as of **June 2026**, pinned to the latest stable MCP specification, **2025-11-25** — the revision to build production servers and gateways against today. The scale figures (the official registry passing **2,000+ servers**, the `mcp` package at ~97M monthly downloads) and the **MCPMark** specification (**127 tasks** across Notion/GitHub/Postgres/Filesystem/Playwright, averaging **16.2 turns / 17.4 tool calls**, rewarding failure recovery) reflect early-2026 reporting; re-pull MCPMark's task list and scoring before you cite a number in a review. The **2026-07-28 release candidate** is the largest revision since launch — a **stateless core** (scale on ordinary HTTP/serverless without session affinity), **MCP Apps** (server-rendered interactive UIs), a **Tasks** extension (long-running/async work), and tighter **OAuth/OIDC** alignment — and the official **`mcp` SDK is heading to v2** (beta through 2026); treat all of these as forthcoming until the RC is ratified, and design so adopting them is configuration, not rewrite. Transport status: **stdio** and **Streamable HTTP** are current; **HTTP+SSE** has been deprecated since **2025-03-26**. Gateway products, registry tooling, and SDK surfaces move release-to-release — treat any code here as a snapshot and verify against your installed versions; the protocol and benchmark facts above are the stable ground.

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/MCP for AI Agents/Lecture-08.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/MCP%20for%20AI%20Agents/Lecture-08.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
