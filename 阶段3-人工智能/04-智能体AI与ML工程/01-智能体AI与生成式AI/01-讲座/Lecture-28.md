---
title: 第 28 讲 - Agent 系统的 runtime 策略：Node、Bun、Rust 与边缘打包
description: 第 28 讲 - Agent 系统的 runtime 策略：Node、Bun、Rust 与边缘打包
---

# 第 28 讲 - Agent 系统的 runtime 策略：Node、Bun、Rust 与边缘打包

**课程：** [AI Agent Development 2026](../Guide.md) | **上一讲：** [第 27 讲](Lecture-27.md) | **下一讲：** [第 29 讲](Lecture-29.md)

---

OpenClaw 目前主要仍处在 **Node/TypeScript 世界**。

这是一个务实的选择：

- TypeScript 生产力高。
- Node 拥有生态。
- npm 包几乎覆盖了所有集成面。
- agent 产品迭代快。

但 runtime 层正在变得 **具有战略意义**。

Bun 公开的 Zig 到 Rust 移植工作是一个有用的信号。它 **并不** 意味着 Bun 已经变成 Rust runtime。我们用作证据的那个具体 commit，只是一份初始的 Phase-A 移植指南和辅助脚本。信号更窄，也更有用：

```text
fast runtimes are not enough
runtime ecosystems, maintainability, tooling, and packaging now matter as product strategy
```


这对 agent 系统很重要，因为 agent runtime **不是普通的 web 服务器**。

它是一个 **长时运行、使用工具、流式、子进程密集、受策略门控** 的执行平台。

---

## 学习目标

学完本讲，你应当能够：

1. 解释 Bun 是什么，以及它的 runtime 策略为何重要。
2. 区分已确认的 Bun 移植工作与被过度炒作的“Bun 已经是 Rust”说法。
3. 比较 Node、Bun 和 Rust 作为 agent runtime 层的差异。
4. 解释为什么 agent 工作负载对 runtime 造成的压力不同于标准 web 工作负载。
5. 指出 OpenClaw 风格系统中哪些部分应留在 TypeScript，哪些部分可能属于 Rust。
6. 设计一个不破坏兼容性的 runtime 迁移实验。
7. 定义针对启动、内存、事件循环行为、子进程编排与打包的度量方式。
8. 把 runtime 选择与边缘/端侧 AI 部署联系起来。

---

## 1. Bun 是什么

Bun 是一个 **替代性的 JavaScript runtime** 与工具链。

它试图把若干部分折叠进同一个发行版：

```text
JavaScript runtime
package manager
test runner
transpiler
bundler-style tooling
```


而 Node 传统上依赖一整套外围生态：

```text
node
npm / yarn / pnpm
tsx / ts-node
jest / vitest
esbuild / swc / webpack
```


Bun 的产品主张是：

```text
one fast integrated toolchain
```


重要的实现细节：

- Node 使用 V8。
- Bun 使用 JavaScriptCore。
- Bun 历史上大量使用 Zig。

runtime **不只是“JavaScript 执行的地方”**。

它塑造：

- 启动延迟
- 包安装速度
- 子进程行为
- 原生依赖处理
- 测试循环速度
- 单二进制打包选项
- 边缘设备部署的易用性

---

## 2. Zig 到 Rust 的信号究竟说明了什么

所引用的 Bun commit 标题为：

```text
docs: add Phase-A porting guide
```


它新增了一份 `docs/PORTING.md` 指南和一个辅助脚本。

该指南描述了一个分阶段的 Zig 到 Rust 翻译流程：

```text
Phase A: draft .rs next to .zig, logic faithful, does not need to compile
Phase B: make it compile crate-by-crate
```


它还给出了严格的约束：

- 在草稿阶段保持 Zig 结构
- 不要自创 crate 布局
- 避免常见的 Rust 异步/runtime crate，如 Tokio、Rayon、Hyper、async-trait 和 futures
- 在 Bun 自有的 runtime 路径中避免使用 `std::fs`、`std::net` 和 `std::process`
- 保留 Bun 对事件循环与系统调用的所有权
- 为 `unsafe` 块标注等价的 Zig 不变式

已确认的结论：

```text
Bun maintainers are documenting how to port parts of Bun from Zig to Rust.
```


未经确认的过度延伸：

```text
"Bun has completed a Rust rewrite."
```


不要基于这种 **过度延伸** 做架构决策。

把它当作 **战略信号**，而不是迁移的触发条件。

---

## 3. 为什么 Zig 说得通

Zig 对 **runtime 基础设施** 有吸引力，因为它提供：

- 底层控制
- 直截了当的 C 互操作
- 显式内存管理
- 简单的交叉编译方案
- 极少的 runtime 假设
- 面向性能的易用性

对于 Bun 这样的 runtime，这些是实打实的优势。

取舍在于 **生态**。

Zig 的生态与人才池 **比 Rust 更小**。

对一个快速演进的 runtime 而言，这可能和语言设计本身一样重要。

---


<details>
<summary>English original</summary>

**Lecture 28 - Runtime Strategy for Agent Systems: Node, Bun, Rust, and Edge Packaging**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 27](Lecture-27.md) | **Next:** [Lecture 29](Lecture-29.md)

---

OpenClaw currently lives mostly in the **Node/TypeScript world**.

That is a pragmatic choice:

- TypeScript is productive.
- Node has the ecosystem.
- npm packages cover almost every integration surface.
- agent products move quickly.

But the runtime layer is becoming **strategic**.

Bun's public Zig-to-Rust porting work is a useful signal. It does **not** mean Bun has already become a Rust runtime. The specific commit we are using as evidence is an initial Phase-A porting guide and helper script. The signal is narrower and more useful:

```text
fast runtimes are not enough
runtime ecosystems, maintainability, tooling, and packaging now matter as product strategy
```

That matters for agent systems because an agent runtime is **not a normal web server**.

It is a **long-running, tool-using, streaming, subprocess-heavy, policy-gated** execution platform.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Explain what Bun is and why its runtime strategy matters.
2. Distinguish confirmed Bun porting work from overhyped "Bun is now Rust" claims.
3. Compare Node, Bun, and Rust as agent-runtime layers.
4. Explain why agent workloads stress runtimes differently from standard web workloads.
5. Identify which parts of an OpenClaw-style system should stay in TypeScript and which parts may belong in Rust.
6. Design a runtime migration experiment without breaking compatibility.
7. Define measurements for startup, memory, event-loop behavior, subprocess orchestration, and packaging.
8. Connect runtime choices to edge/on-device AI deployment.

---

**1. What Bun is**

Bun is an **alternative JavaScript runtime** and tooling stack.

It tries to collapse several pieces into one distribution:

```text
JavaScript runtime
package manager
test runner
transpiler
bundler-style tooling
```

Where Node traditionally relies on a surrounding ecosystem:

```text
node
npm / yarn / pnpm
tsx / ts-node
jest / vitest
esbuild / swc / webpack
```

Bun's product argument is:

```text
one fast integrated toolchain
```

Important implementation detail:

- Node uses V8.
- Bun uses JavaScriptCore.
- Bun has historically used a large amount of Zig.

The runtime is **not just "where JavaScript executes."**

It shapes:

- startup latency
- package install speed
- subprocess behavior
- native dependency handling
- test-loop speed
- single-binary packaging options
- edge-device deployment ergonomics

---

**2. What the Zig-to-Rust signal actually says**

The referenced Bun commit is titled:

```text
docs: add Phase-A porting guide
```

It adds a `docs/PORTING.md` guide and a helper script.

The guide describes a staged Zig-to-Rust translation process:

```text
Phase A: draft .rs next to .zig, logic faithful, does not need to compile
Phase B: make it compile crate-by-crate
```

It also gives strong constraints:

- keep the Zig structure during the draft phase
- do not invent crate layouts
- avoid common Rust async/runtime crates such as Tokio, Rayon, Hyper, async-trait, and futures
- avoid `std::fs`, `std::net`, and `std::process` for Bun-owned runtime paths
- preserve Bun's event loop and syscall ownership
- annotate `unsafe` blocks with the equivalent Zig invariant

The confirmed takeaway:

```text
Bun maintainers are documenting how to port parts of Bun from Zig to Rust.
```

The unconfirmed overreach:

```text
"Bun has completed a Rust rewrite."
```

Do not build architecture decisions on the **overreach**.

Use this as a **strategic signal**, not a migration trigger.

---

**3. Why Zig made sense**

Zig is attractive for **runtime infrastructure** because it gives:

- low-level control
- straightforward C interop
- explicit memory management
- simple cross-compilation story
- small runtime assumptions
- performance-oriented ergonomics

For a runtime like Bun, those are real advantages.

The tradeoff is **ecosystem**.

Zig's ecosystem and hiring pool are **smaller than Rust's**.

For a fast-moving runtime, that can matter as much as language design.

---

</details>

## 4. 为什么 Rust 可能长期胜出

Rust 对 **平台基础设施** 有吸引力，因为它提供：

- 无需垃圾回收的内存安全
- 强并发保证
- 成熟的 crates 生态
- 强大的工具链
- 庞大的贡献者群体
- 良好的供应链/安全工具
- 在生产系统中的可信度

取舍是 **复杂度**。

Rust 有：

- 更陡峭的学习曲线
- borrow checker 摩擦
- 编译期成本
- 更多前期的类型与生命周期设计

但对长期平台工作而言，Rust 往往胜出，因为 **可维护性与贡献者规模** 更重要。

务实的解读：

```text
Zig can be excellent for a focused systems codebase.
Rust can be better for a broad, long-lived runtime ecosystem.
```

这 **不是意识形态**。

这是 **平台经济学**。

---

## 5. 为什么这对 agent 系统很重要

agent 工作负载不同于 **普通后端工作负载**。

它们具有：

- 长时间运行的会话
- 流式模型输出
- 大量子进程
- 延迟不可预测的工具调用
- 文件监听
- terminal/PTY 处理
- 浏览器自动化
- 本地 socket
- WebSocket 控制平面
- prompt/上下文组装
- 内存与产物写入
- 审批与策略检查
- 崩溃恢复

这使得 runtime 成为 **product surface**。

如果 runtime 处理不当：

- 子进程
- 信号
- 文件描述符
- 背压
- 定时器
- 内存增长
- native 依赖

那么即便模型很强，agent 产品也会显得 **不可靠**。

模型负责 **推理**。

runtime 负责 **在与操作系统的接触中存活**。

---

## 6. 今天的 OpenClaw：Node 24 作为稳定目标

对于今天 OpenClaw 风格的开发，稳定基线是：

```text
Node 24
TypeScript
pnpm
native helpers where necessary
```

这是 **正确的默认选择**，因为：

- 兼容性比新颖性更重要
- 插件生态假定 Node
- 许多 SDK 优先提供 Node 版本
- 调试工具成熟
- 长时间运行的服务行为已被充分理解
- 贡献者上手更容易

对 OpenClaw 而言，runtime 决策不是：

```text
Should everything move to Bun tomorrow?
```

更好的问题是：

```text
Which runtime properties do agent systems need long-term?
```

---

## 7. 关键的 runtime 属性

对 agent 系统，应度量：

| Property | Why it matters |
|---|---|
| 冷启动 | CLI agent、hooks、短生命周期工具、serverless 任务 |
| 热内存 | 长时间运行的 Gateway、会话、事件日志 |
| 子进程处理 | shell 工具、构建工具、node 主机、审批 |
| WebSocket 行为 | Gateway RPC、流式、节点、dashboard |
| 文件监听 | code agent、本地工作区、重载循环 |
| native 依赖情况 | 浏览器自动化、语音、ML 辅助组件 |
| 跨平台打包 | macOS、Linux、Windows、Jetson |
| 调试工具 | 生产事故分析 |
| 生态兼容性 | SDK、插件、认证库、包管理器 |
| 安全面 | 沙箱、供应链、权限边界 |

仅靠启动速度 **还不够**。

runtime 可以很快，但仍然是 **糟糕的控制平面底座**。

---

## 8. Bun 能在哪些地方帮到 agent

Bun 在 agent 系统中可能有用，用于：

### 快速的 CLI 启动

短生命周期命令受益于更快的启动：

```text
agent helper
hook runner
test probe
small local tool
```

### 集成的测试与运行工具

更少的活动部件可以简化开发者循环：

```bash
bun install
bun test
bun run
```

### 打包方向

agent 产品往往希望 **本地安装体积小**。

单二进制或紧密打包的工具链对以下场景有价值：

- 本地优先助手
- 桌面应用
- CI agent
- 边缘设备
- 教室/实验室环境

### TypeScript 优先的开发者体验

如果 Bun 能在不需要额外转译层的情况下运行项目的大部分内容，**本地迭代就会更简单**。

---

## 9. Bun 的风险所在

不要假定 Bun 是每个 Node 工作负载的 **直接替代品**。

风险领域：

- Node API 兼容性边界情况
- native 模块行为
- agent 循环下长时间运行进程的稳定性
- 调试与性能剖析的成熟度
- 子进程与 PTY 边界情况
- WebSocket/控制平面压力
- 包管理器差异
- CI 与生产环境的一致性

对个人原型而言，这些可能 **可以接受**。

对 agent 网关而言，它们 **必须被度量**。

---


<details>
<summary>English original</summary>

**4. Why Rust might win long-term**

Rust is attractive for **platform infrastructure** because it offers:

- memory safety without garbage collection
- strong concurrency guarantees
- mature crates ecosystem
- strong tooling
- large contributor pool
- good supply-chain/security tooling
- credibility in production systems

The tradeoff is **complexity**.

Rust has:

- a steeper learning curve
- borrow-checker friction
- compile-time cost
- more up-front type and lifetime design

But for long-term platform work, Rust often wins because **maintainability and contributor scale** matter.

The practical interpretation:

```text
Zig can be excellent for a focused systems codebase.
Rust can be better for a broad, long-lived runtime ecosystem.
```

This is **not ideology**.

It is **platform economics**.

---

**5. Why this matters for agent systems**

Agent workloads differ from **normal backend workloads**.

They have:

- long-running sessions
- streaming model output
- many subprocesses
- tool calls with unpredictable latency
- file watching
- terminal/PTY handling
- browser automation
- local sockets
- WebSocket control planes
- prompt/context assembly
- memory and artifact writes
- approval and policy checks
- crash recovery

That makes the runtime a **product surface**.

If the runtime mishandles:

- child processes
- signals
- file descriptors
- backpressure
- timers
- memory growth
- native dependencies

then the agent product feels **unreliable** even if the model is strong.

The model **reasons**.

The runtime **survives contact with the operating system**.

---

**6. OpenClaw today: Node 24 as stable target**

For OpenClaw-style development today, the stable baseline is:

```text
Node 24
TypeScript
pnpm
native helpers where necessary
```

This is the **correct default** because:

- compatibility matters more than novelty
- plugin ecosystems assume Node
- many SDKs ship Node-first
- debugging tools are mature
- long-running service behavior is well understood
- contributor onboarding is easier

For OpenClaw, the runtime decision is not:

```text
Should everything move to Bun tomorrow?
```

The better question:

```text
Which runtime properties do agent systems need long-term?
```

---

**7. Runtime properties that matter**

For agent systems, measure:

| Property | Why it matters |
|---|---|
| Cold start | CLI agents, hooks, short-lived tools, serverless tasks |
| Warm memory | long-running Gateway, sessions, event logs |
| Subprocess handling | shell tools, build tools, node hosts, approvals |
| WebSocket behavior | Gateway RPC, streaming, nodes, dashboards |
| File watching | code agents, local workspaces, reload loops |
| Native dependency story | browser automation, speech, ML helpers |
| Cross-platform packaging | macOS, Linux, Windows, Jetson |
| Debug tooling | production incident analysis |
| Ecosystem compatibility | SDKs, plugins, auth libraries, package managers |
| Security surface | sandboxing, supply chain, permission boundaries |

Startup speed alone is **not enough**.

A runtime can be fast and still be a **poor control-plane substrate**.

---

**8. Where Bun could help agents**

Bun may be useful in agent systems for:

**Fast CLI startup**

Short-lived commands benefit from faster startup:

```text
agent helper
hook runner
test probe
small local tool
```

**Integrated test and run tooling**

Fewer moving parts can simplify developer loops:

```bash
bun install
bun test
bun run
```

**Packaging direction**

Agent products often want a **small local install footprint**.

Single-binary or tightly bundled toolchains are valuable for:

- local-first assistants
- desktop apps
- CI agents
- edge devices
- classroom/lab setups

**TypeScript-first developer experience**

If Bun can run enough of a project without extra transpilation layers, **local iteration gets simpler**.

---

**9. Where Bun is risky**

Do not assume Bun is a **drop-in replacement** for every Node workload.

Risk areas:

- Node API compatibility edge cases
- native module behavior
- long-running process stability under agent loops
- debugging and profiling maturity
- subprocess and PTY edge cases
- WebSocket/control-plane stress
- package manager differences
- CI parity with production

For a personal prototype, these may be **acceptable**.

For an agent gateway, they **must be measured**.

---

</details>

## 10. Rust 的适用位置

Rust **不需要取代 TypeScript** 也能发挥作用。

更好的拆分：

```text
TypeScript:
  product logic
  Gateway RPC schemas
  plugins
  user-facing SDKs
  fast iteration

Rust:
  local daemon
  PTY/session supervisor
  file watching core
  sandbox executor
  artifact store
  vector/indexing service
  audio pipeline helper
  packaging-critical native service
```

这与 local-first agent 工作区中已经可见的模式一致：

```text
TypeScript gives product velocity.
Rust gives OS-facing reliability.
```

不要**为了重写而重写**。

只迁移那些 Rust 的特性**能收回成本**的部分。

---

## 11. 三层 runtime 策略

一个贴近 OpenClaw 的务实策略：

### 阶段 1：Node 稳定性

```text
Node 24
pnpm
strict tests
known deployment path
```

用于：

- 网关
- App SDK
- 插件
- 文档
- 标准开发者工作流

### 阶段 2：Bun 实验

```text
run selected CLI/helper paths under Bun
measure startup, memory, compatibility, test behavior
keep production on Node until evidence says otherwise
```

用于：

- 短生命周期 helper
- 测试
- 本地工具
- 打包实验

### 阶段 3：Rust 卸载

```text
move OS-facing or performance-critical services into Rust
keep TypeScript as the orchestration/product layer
```

用于：

- node host
- PTY 监管
- 沙箱化 exec
- 音频/视频 helper
- 本地向量存储
- 硬件/设备服务

---

## 12. 如何为 OpenClaw 式的工作评测 Bun

不要问：

```text
Does Bun feel fast?
```

要问：

```text
Which OpenClaw paths pass under Bun?
Which fail?
Why?
Is the failure fixable or structural?
```

实验矩阵：

| 测试 | 度量什么 |
|---|---|
| CLI 启动 | 到 `--help` 的时间、配置读取、简单状态 |
| 网关启动 | 启动时间、内存、插件加载失败 |
| App SDK 冒烟 | 连接、agents 列表、运行、流、等待、取消 |
| WebSocket 压力 | 事件顺序、重连、背压 |
| 工具执行 | 子进程环境、stdout/stderr、信号 |
| 文件监视 | 编辑风暴下的重载行为 |
| 文档/测试运行器 | 与 Node CI 的精度一致性 |
| 原生依赖 | 浏览器自动化、语音、canvas、SQLite |

结果格式：

```text
Path:
Node result:
Bun result:
Failure class:
Workaround:
Decision:
```

这样能让 runtime 决策有据可依。

---

## 13. 边缘与 Jetson 视角

对边缘 AI 系统而言，runtime 打包很关键，因为设备存在：

- 有限的 RAM
- 慢速存储
- 热管理约束
- 脆弱的安装流程
- 长时间无人值守运行
- CPU/GPU/原生依赖混杂

在 Jetson 级设备上，Node 通常可以接受。

但 agent 系统可能还需要：

- 原生音频采集 helper
- CUDA/TensorRT 服务
- Rust 或 C++ 设备守护进程
- 看门狗
- 进程监管器
- 确定性的启动

务实架构：

```text
TypeScript Gateway
  -> Rust/C++ hardware services
  -> Python/ML inference services where needed
  -> explicit RPC boundaries
```

不要把所有逻辑塞进一种语言。

使用清晰的进程边界与带类型的契约。

---

## 14. agent runtime 的平台视角

更大的转变不是：

```text
Bun vs Node
```

而是：

```text
JavaScript runtime
  -> agent execution platform
```

agent 平台需要：

- 快速启动
- 流式
- 子进程控制
- 工具策略
- 会话持久性
- 产物存储
- 本地设备访问
- 可预测的打包
- 安全可审计性
- 生态兼容性

这就是 Bun 的 runtime 策略重要的原因。

它表明 runtime 构建者优化的是集成化的平台体验，而不只是 JavaScript 执行。

---

## 15. 建议立场

对 OpenClaw 式的工作：

```text
Stay on Node 24 for production stability.
Experiment with Bun in narrow paths.
Use Rust for OS-facing, performance-sensitive, or packaging-critical services.
Measure before moving anything broad.
```

决策表：

| 问题 | 默认答案 |
|---|---|
| 网关现在就该迁到 Bun 吗？ | 不，没有兼容性证据就不迁。 |
| CLI/helper 路径该在 Bun 下测试吗？ | 该。 |
| 原生守护进程该用 Rust 写吗？ | 常常该，前提是它们承担面向 OS 的可靠性。 |
| 该放弃 TypeScript 吗？ | 不该。它对产品迭代和 SDK 依然强势。 |
| 该跟踪 runtime 策略吗？ | 该。它影响打包、边缘部署和贡献者体验。 |

---


<details>
<summary>English original</summary>

**10. Where Rust belongs**

Rust does **not need to replace TypeScript** to be useful.

Better split:

```text
TypeScript:
  product logic
  Gateway RPC schemas
  plugins
  user-facing SDKs
  fast iteration

Rust:
  local daemon
  PTY/session supervisor
  file watching core
  sandbox executor
  artifact store
  vector/indexing service
  audio pipeline helper
  packaging-critical native service
```

This matches patterns already visible in local-first agent workspaces:

```text
TypeScript gives product velocity.
Rust gives OS-facing reliability.
```

Do not **rewrite just to rewrite**.

Move the parts where Rust's properties **pay for themselves**.

---

**11. Three-layer runtime strategy**

A realistic OpenClaw-adjacent strategy:

**Phase 1: Node stability**

```text
Node 24
pnpm
strict tests
known deployment path
```

Use this for:

- Gateway
- App SDK
- plugins
- docs
- standard developer workflows

**Phase 2: Bun experiments**

```text
run selected CLI/helper paths under Bun
measure startup, memory, compatibility, test behavior
keep production on Node until evidence says otherwise
```

Use this for:

- short-lived helpers
- tests
- local tooling
- packaging experiments

**Phase 3: Rust offload**

```text
move OS-facing or performance-critical services into Rust
keep TypeScript as the orchestration/product layer
```

Use this for:

- node host
- PTY supervision
- sandboxed exec
- audio/video helpers
- local vector store
- hardware/device services

---

**12. How to evaluate Bun for OpenClaw-style work**

Do not ask:

```text
Does Bun feel fast?
```

Ask:

```text
Which OpenClaw paths pass under Bun?
Which fail?
Why?
Is the failure fixable or structural?
```

Experiment matrix:

| Test | What to measure |
|---|---|
| CLI startup | time to `--help`, config read, simple status |
| Gateway boot | startup time, memory, plugin load failures |
| App SDK smoke | connect, agents list, run, stream, wait, cancel |
| WebSocket stress | event ordering, reconnects, backpressure |
| tool execution | subprocess env, stdout/stderr, signals |
| file watcher | reload behavior under edit storms |
| docs/test runner | parity with Node CI |
| native deps | browser automation, speech, canvas, SQLite |

Result format:

```text
Path:
Node result:
Bun result:
Failure class:
Workaround:
Decision:
```

This keeps runtime decisions evidence-backed.

---

**13. Edge and Jetson angle**

For edge AI systems, runtime packaging matters because devices have:

- limited RAM
- slow storage
- thermal constraints
- fragile setup processes
- long unattended operation
- mixed CPU/GPU/native dependencies

Node is often acceptable on Jetson-class devices.

But an agent system may also need:

- native audio capture helpers
- CUDA/TensorRT services
- Rust or C++ device daemons
- watchdogs
- process supervisors
- deterministic startup

The practical architecture:

```text
TypeScript Gateway
  -> Rust/C++ hardware services
  -> Python/ML inference services where needed
  -> explicit RPC boundaries
```

Do not force all logic into one language.

Use clear process boundaries and typed contracts.

---

**14. The agent-runtime platform view**

The larger shift is not:

```text
Bun vs Node
```

It is:

```text
JavaScript runtime
  -> agent execution platform
```

Agent platforms need:

- fast startup
- streaming
- subprocess control
- tool policy
- session durability
- artifact storage
- local device access
- predictable packaging
- security reviewability
- ecosystem compatibility

This is why Bun's runtime strategy matters.

It shows that runtime builders are optimizing for integrated platform experience, not just JavaScript execution.

---

**15. Recommended stance**

For OpenClaw-style work:

```text
Stay on Node 24 for production stability.
Experiment with Bun in narrow paths.
Use Rust for OS-facing, performance-sensitive, or packaging-critical services.
Measure before moving anything broad.
```

Decision table:

| Question | Default answer |
|---|---|
| Should Gateway move to Bun now? | No, not without compatibility evidence. |
| Should CLI/helper paths be tested under Bun? | Yes. |
| Should native daemons be written in Rust? | Often, if they own OS-facing reliability. |
| Should TypeScript be abandoned? | No. It remains strong for product iteration and SDKs. |
| Should runtime strategy be tracked? | Yes. It affects packaging, edge deployment, and contributor experience. |

---

</details>

## 小实验：runtime 评估 harness（agent 运行时框架）

为一个 OpenClaw 风格的项目创建一张小型的 runtime 兼容性矩阵。

先在 Node 下测试，再尽可能在 Bun 下测试：

```bash
node --version
bun --version

time node ./scripts/smoke.js
time bun ./scripts/smoke.js
```

测量：

- 启动时间
- 峰值内存
- 包安装时间
- 子进程行为
- WebSocket 行为
- 测试精度一致性
- 原生依赖失败

写一页结果：

```text
Runtime:
Version:
What passed:
What failed:
Why it failed:
Would we ship this:
Next experiment:
```

目的不是证明 Bun 更好或更差。

目的是让 runtime 决策可度量。

---

## 关键要点

- Bun 的 Zig 转 Rust 工作目前只是初步移植计划，并非已完成的改写。
- 战略信号是：runtime 的可维护性与生态规模很重要，而不只是原始速度。
- agent 工作负载通过流式传输、子进程、会话、工具、文件监听和原生辅助程序给 runtime 施压。
- 目前 Node 24 仍是 OpenClaw 的稳定基线。
- Bun 值得在窄范围的 CLI/辅助程序/工具链路径中测试。
- Rust 非常适合面向 OS 的服务、沙箱、PTY/会话监督以及设备本地辅助程序。
- 胜出的架构很可能是混合式的：TypeScript 用于产品迭代速度，Rust/C++ 用于硬性的 runtime 边界，两者之间用清晰的 RPC 契约衔接。

---

## 参考文献

- Bun commit `46d3bc2`，"docs: add Phase-A porting guide"：[https://github.com/oven-sh/bun/commit/46d3bc29f270fa881dd5730ef1549e88407701a5](https://github.com/oven-sh/bun/commit/46d3bc29f270fa881dd5730ef1549e88407701a5)
- 该 commit 上的 Bun Phase-A 移植指南：[https://raw.githubusercontent.com/oven-sh/bun/46d3bc29f270fa881dd5730ef1549e88407701a5/docs/PORTING.md](https://raw.githubusercontent.com/oven-sh/bun/46d3bc29f270fa881dd5730ef1549e88407701a5/docs/PORTING.md)
- Bun 仓库：[https://github.com/oven-sh/bun](https://github.com/oven-sh/bun)
- Node.js 发布版本：[https://nodejs.org/en/about/previous-releases](https://nodejs.org/en/about/previous-releases)
- Lecture 02 - Agent Harness：[Lecture-02.md](Lecture-02.md)
- Lecture 41 - Pi：[Lecture-41.md](Lecture-41.md)
- Lecture 29 - Agentic SDLC：[Lecture-29.md](Lecture-29.md)

---

*下一讲：[Lecture 29](Lecture-29.md)*


<details>
<summary>English original</summary>

**Mini-lab: runtime evaluation harness**

Create a small runtime compatibility matrix for one OpenClaw-style project.

Test under Node first, then Bun where possible:

```bash
node --version
bun --version

time node ./scripts/smoke.js
time bun ./scripts/smoke.js
```

Measure:

- startup time
- peak memory
- package install time
- subprocess behavior
- WebSocket behavior
- test parity
- native dependency failures

Write a one-page result:

```text
Runtime:
Version:
What passed:
What failed:
Why it failed:
Would we ship this:
Next experiment:
```

The goal is not to prove Bun is better or worse.

The goal is to make runtime decisions measurable.

---

**Key takeaways**

- Bun's Zig-to-Rust work is currently an initial porting plan, not a completed rewrite.
- The strategic signal is that runtime maintainability and ecosystem scale matter, not only raw speed.
- Agent workloads stress runtimes through streaming, subprocesses, sessions, tools, file watching, and native helpers.
- Node 24 remains the stable OpenClaw baseline today.
- Bun is worth testing in narrow CLI/helper/tooling paths.
- Rust is a strong fit for OS-facing services, sandboxing, PTY/session supervision, and device-local helpers.
- The winning architecture is likely hybrid: TypeScript for product velocity, Rust/C++ for hard runtime boundaries, and clear RPC contracts between them.

---

**References**

- Bun commit `46d3bc2`, "docs: add Phase-A porting guide": [https://github.com/oven-sh/bun/commit/46d3bc29f270fa881dd5730ef1549e88407701a5](https://github.com/oven-sh/bun/commit/46d3bc29f270fa881dd5730ef1549e88407701a5)
- Bun Phase-A porting guide at that commit: [https://raw.githubusercontent.com/oven-sh/bun/46d3bc29f270fa881dd5730ef1549e88407701a5/docs/PORTING.md](https://raw.githubusercontent.com/oven-sh/bun/46d3bc29f270fa881dd5730ef1549e88407701a5/docs/PORTING.md)
- Bun repository: [https://github.com/oven-sh/bun](https://github.com/oven-sh/bun)
- Node.js releases: [https://nodejs.org/en/about/previous-releases](https://nodejs.org/en/about/previous-releases)
- Lecture 02 - Agent Harness: [Lecture-02.md](Lecture-02.md)
- Lecture 41 - Pi: [Lecture-41.md](Lecture-41.md)
- Lecture 29 - Agentic SDLC: [Lecture-29.md](Lecture-29.md)

---

*Next: [Lecture 29](Lecture-29.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-28.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-28.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
