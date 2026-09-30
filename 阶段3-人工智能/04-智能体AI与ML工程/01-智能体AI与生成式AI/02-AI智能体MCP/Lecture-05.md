---
title: 第 05 讲 - 构建 MCP 服务器（FastMCP）
description: 第 05 讲 - 构建 MCP 服务器（FastMCP）
---

# 第 05 讲 - 构建 MCP 服务器（FastMCP）

**合集：** [MCP for AI Agents](README.md) | **上一篇：** [← 第 04 讲](Lecture-04.md) | **下一篇：** [第 06 讲](Lecture-06.md)

---

前四讲都在读协议：宿主/客户端/服务器的划分、生命周期，以及六种原语 —— 由服务器→客户端的 Tools、Resources、Prompts，以及反向运行的采样、Roots、Elicitation。现在，你要停止阅读规范，开始输出规范。到本讲结束时，你将拥有一个服务器，真实的宿主 —— Claude Desktop、Claude Code 或 MCP Inspector —— 可以连接它、枚举它、调用它。

对你手腕的好消息是：你不需要手写 JSON-RPC 信封或 JSON Schema。**FastMCP** API —— 随官方 `mcp` Python SDK *一同发布* 的高层封装 —— 能把一个加了装饰器的 Python 函数变成完整的 tool：它读取你的类型注解来构建 `inputSchema`，读取你的 docstring 来构建 description。第 02–04 讲中的协议机制仍然在下面；FastMCP 只是替你生成它，好让你把注意力花在真正要暴露的能力上。

在任何代码之前，有一条长期有效的警告：**SDK 的接口面是一个移动靶。** 官方包正走向 v2（2026 年进入 beta），独立的 FastMCP 2.x 则每个版本都在演进。装饰器名称、`Context` 方法集合以及 lifespan 签名在不同版本间都发生过变化。把这里的每个代码片段都当作 *截止当前* 的版本，并对照你实际 `pip install` 的版本进行验证 —— `mcp version`，然后，如果某个调用无法解析，就去读模块自身的 docstring。

---

## 学习目标

到本讲结束时，你应该能够：

- 在官方 `mcp` SDK（FastMCP）、独立的 FastMCP 2.x 和 TypeScript SDK 之间做出选择，并安装正确的那一个。
- 编写一个完整的 FastMCP 服务器来暴露 `@mcp.tool()`，并解释 Python 类型注解如何变成宿主看到的 `inputSchema`。
- 用 `@mcp.resource(...)` 暴露带模板的 **resources**，用 `@mcp.prompt()` 暴露可复用的 **prompts**。
- 使用注入的 **`Context`** 对象做日志、进度、resource 读取、采样和 elicitation，并用 **lifespan** 保存共享状态。
- 返回一个 Pydantic 模型，让 SDK 输出 `outputSchema` 和结构化内容。
- 用 **MCP Inspector** 测试服务器，并通过一个 JSON 配置块把它接入宿主。

---

## 1. SDK 全景与安装

2026 年有三个 SDK 值得关注，而且因为它们的名字容易混淆，有必要精确区分谁是谁。

- **官方 `mcp` Python SDK。** 与规范同步维护。它同时提供一个低层 `Server` 类 *和* 一个称为 **FastMCP** 的高层易用封装 —— 以 `from mcp.server.fastmcp import FastMCP` 导入。这是最正统、紧跟规范的选择，也是本讲大部分内容所用的。
- **独立的 FastMCP 2.x**（`jlowin/Prefect`、`pip install fastmcp`）。最初的 FastMCP 项目，如今是一个带额外附件的超集 —— 认证辅助工具、服务器组合、测试工具、部署粘合层。它驱动着大约 **70% 的社区服务器**。核心装饰器 API 与官方版本几乎相同，所以这里学到的大部分内容都能迁移；FastMCP 2.x 只是在边缘上多加了一些东西。
- **官方 TypeScript SDK**（`@modelcontextprotocol/sdk`）。面向 Node/浏览器世界的同一套协议 —— 在 §7 中简要涉及。

一句话说怎么选：**对于任何你希望最大程度贴合规范、依赖又少的东西，从官方 `mcp` SDK 的 FastMCP 开始；当你需要它那套额外的部署/认证/组合机制时，再转向独立的 FastMCP 2.x。** 两者足够接近，在它们之间迁移通常不过是改一个 import，再加几处签名微调。

用 CLI extra 安装官方 SDK（它会带来 `mcp dev`、`mcp install` 等）：

```bash
pip install "mcp[cli]"
```


或者，用 2026 年的惯用方式，通过 [`uv`](https://docs.astral.sh/uv/) —— 快、可复现，也正是 §7 中的宿主配置将要调用的东西：

```bash
uv init my-mcp-server
cd my-mcp-server
uv add "mcp[cli]"
```


如果你想要的是独立项目，那就是 `pip install fastmcp`（或 `uv add fastmcp`）和 `from fastmcp import FastMCP`。其余几乎不变。

---


<details>
<summary>English original</summary>

**Lecture 05 - Building an MCP Server (FastMCP)**

**Collection:** [MCP for AI Agents](README.md) | **Previous:** [← Lecture 04](Lecture-04.md) | **Next:** [Lecture 06](Lecture-06.md)

---

The previous four lectures were spent reading the protocol: the host/client/server split, the lifecycle, and the six primitives — Tools, Resources, Prompts going server→client, and Sampling, Roots, Elicitation running backwards. Now you stop reading the spec and start emitting it. By the end of this lecture you will have a server that a real host — Claude Desktop, Claude Code, or the MCP Inspector — can connect to, enumerate, and call.

The good news for your wrists: you are not going to hand-write JSON-RPC envelopes or JSON Schema. The **FastMCP** API — the high-level layer that ships *inside* the official `mcp` Python SDK — turns a decorated Python function into a fully-formed tool, reading your type hints to build the `inputSchema` and your docstring to build the description. The protocol machinery from Lectures 02–04 is still down there; FastMCP just generates it for you so you can spend your attention on the actual capability you are exposing.

One standing warning before any code: **the SDK surface is a moving target.** The official package is heading toward a v2 (beta in 2026), and the standalone FastMCP 2.x evolves release-to-release. Decorator names, the `Context` method set, and the lifespan signature have all shifted across versions. Treat every snippet here as *current-as-of* and verify against the version you actually `pip install`ed — `mcp version`, then read the module's own docstrings if a call doesn't resolve.

---

**Learning objectives**

By the end of this lecture you should be able to:

- Choose between the official `mcp` SDK (FastMCP), the standalone FastMCP 2.x, and the TypeScript SDK, and install the right one.
- Write a complete FastMCP server exposing a `@mcp.tool()`, and explain how Python type hints become the `inputSchema` the host sees.
- Expose templated **resources** with `@mcp.resource(...)` and reusable **prompts** with `@mcp.prompt()`.
- Use the injected **`Context`** object for logging, progress, resource reads, sampling, and elicitation, and hold shared state in a **lifespan**.
- Return a Pydantic model so the SDK emits an `outputSchema` and structured content.
- Test the server with the **MCP Inspector** and wire it into a host via a JSON config block.

---

**1. The SDK landscape & install**

Three SDKs matter in 2026, and it is worth being precise about which is which because their names collide.

- **The official `mcp` Python SDK.** Maintained alongside the spec. It ships a low-level `Server` class *and* a high-level ergonomic layer called **FastMCP** — imported as `from mcp.server.fastmcp import FastMCP`. This is the canonical, spec-tracking choice, and it is what most of this lecture uses.
- **The standalone FastMCP 2.x** (`jlowin/Prefect`, `pip install fastmcp`). The original FastMCP project, now a superset with extra batteries — auth helpers, server composition, testing utilities, deployment glue. It powers roughly **70% of community servers**. The core decorator API is nearly identical to the official one, so most of what you learn here transfers; FastMCP 2.x just adds more around the edges.
- **The official TypeScript SDK** (`@modelcontextprotocol/sdk`). The same protocol for the Node/browser world — covered briefly in §7.

How to choose, in one sentence: **start on the official `mcp` SDK's FastMCP for anything you want maximally spec-aligned and dependency-light, and reach for standalone FastMCP 2.x when you need its extra deployment/auth/composition machinery.** They are close enough that moving between them is rarely more than an import change plus a few signature tweaks.

Install the official SDK with the CLI extra (which gives you `mcp dev`, `mcp install`, and friends):

```bash
pip install "mcp[cli]"
```

Or, the idiomatic 2026 way, with [`uv`](https://docs.astral.sh/uv/) — fast, reproducible, and what the host config in §7 will invoke:

```bash
uv init my-mcp-server
cd my-mcp-server
uv add "mcp[cli]"
```

If you instead want the standalone project, it is `pip install fastmcp` (or `uv add fastmcp`) and `from fastmcp import FastMCP`. The rest changes very little.

---

</details>

## 2. 一个最小服务器

下面是一个完整、可运行的服务器。它只做一件事——暴露单个 tool——但它是一个*真正的* MCP server：host 可以通过 stdio 连接它、列出其 tools 并调用它们。

```python
# server.py
from mcp.server.fastmcp import FastMCP

# The server's name is what shows up in the host's UI.
mcp = FastMCP("Demo")


@mcp.tool()
def add(a: int, b: int) -> int:
    """Add two integers and return their sum."""
    return a + b


if __name__ == "__main__":
    # Run over stdio — the host launches this process and talks to it
    # over stdin/stdout. This is the default, local transport.
    mcp.run()
```

这就是整个服务器。用 `python server.py` 直接运行它（它会在 stdio 上等待 client），或者——开发期间更有用的是——让 Inspector 指向它（§6）。

值得放慢细看的部分是 **`add` 如何变成一个协议合法的 tool**。你写的是一个普通 Python 函数；FastMCP 完成了协议层面的工作：

- **函数名 `add`** 成为该 tool 的 `name`。
- **docstring** `"Add two integers and return their sum."` 成为该 tool 的 `description`——host 的模型读取这段文本来决定*是否以及如何*调用它。这不是注释；它是承重的 prompt 素材，所以要为模型而写。
- **类型提示** `a: int, b: int` 被内省并编译成 JSON Schema `inputSchema`。host 大致收到：

```json
{
  "name": "add",
  "description": "Add two integers and return their sum.",
  "inputSchema": {
    "type": "object",
    "properties": {
      "a": { "type": "integer" },
      "b": { "type": "integer" }
    },
    "required": ["a", "b"]
  }
}
```

这与你在 Lecture 03 中手写的 `inputSchema` 是同一份——FastMCP 只是从签名推导出了它。更丰富的提示会产生更丰富的 schema：`str` 变成 `"type": "string"`，带默认值的 parameter（`unit: str = "celsius"`）变成可选并从 `required` 中消失，`Literal["celsius", "fahrenheit"]` 变成 `enum`，而 Pydantic model parameter 会展开为嵌套的 object schema。教训是：**模型看到的 schema 质量，正是你所写的类型标注质量。** 含糊的提示（无类型的 `a`，或为结构化参数写的 `dict`）会产生含糊的 schema 和更差的 tool 选择。

---

## 3. FastMCP 中的 resources 与 prompts

Tools 是模型控制的操作。Lecture 03 中的另外两个 server→client 原语——**resources**（应用控制的数据）和 **prompts**（用户控制的模板）——有各自的 decorator。

**resource** 是可寻址的数据，由 URI 标识。FastMCP 允许你对 URI 做模板化：URI 字符串中的路径参数映射到函数参数，因此一个函数就能服务一整族 resources。

```python
from mcp.server.fastmcp import FastMCP

mcp = FastMCP("Demo")

# Static, well-known config values keyed by name.
CONFIG = {"theme": "dark", "max_retries": "3", "region": "us-east-1"}


@mcp.resource("config://{key}")
def read_config(key: str) -> str:
    """Return the value of a single configuration key."""
    return CONFIG.get(key, f"(no such key: {key})")
```

host 读取 `config://region` 时调用 `read_config("region")` 并取回 `"us-east-1"`。URI scheme 中的 `{key}` 段成为 `key` parameter——与 tools 相同的 hint-to-schema 机制，只是作用于 URI 模板。resources 用于应用想要*拉入上下文*的数据（文件、记录、配置），而非模型*执行*的操作；Lecture 03 中的那个 controller 区分正是让干净服务器保持干净的关键。

**prompt** 是用户可调用的、可复用的参数化消息模板——可以想成 host 呈现的斜杠命令。一个 FastMCP prompt 返回应当注入的消息：

```python
@mcp.prompt()
def review_code(language: str, code: str) -> str:
    """A prompt that asks for a focused code review."""
    return (
        f"Please review the following {language} code for bugs, security "
        f"issues, and style. Be specific and cite line numbers.\n\n"
        f"```{language}\n{code}\n```"
    )
```

返回普通字符串会生成单条 user 消息。当需要多轮结构或非 user 角色时，改为返回 message object 列表：

```python
from mcp.server.fastmcp.prompts import base


@mcp.prompt()
def debug_session(error: str) -> list[base.Message]:
    """Seed a debugging conversation with the error already in context."""
    return [
        base.UserMessage("I hit this error and need help:"),
        base.UserMessage(error),
        base.AssistantMessage("Let's work through it. What were you running?"),
    ]
```

message 辅助工具的确切 import 路径（`base.UserMessage` 等）是会在各 SDK 版本之间漂移的东西之一——如果解析不了，返回由普通 `{"role": ..., "content": ...}` dict 组成的列表是版本健壮的回退方案。

---


<details>
<summary>English original</summary>

**2. A minimal server**

Here is a complete, runnable server. It does one thing — exposes a single tool — but it is a *real* MCP server: a host can connect to it over stdio, list its tools, and call them.

```python
# server.py
from mcp.server.fastmcp import FastMCP

# The server's name is what shows up in the host's UI.
mcp = FastMCP("Demo")


@mcp.tool()
def add(a: int, b: int) -> int:
    """Add two integers and return their sum."""
    return a + b


if __name__ == "__main__":
    # Run over stdio — the host launches this process and talks to it
    # over stdin/stdout. This is the default, local transport.
    mcp.run()
```

That is the whole server. Run it directly with `python server.py` (it will sit waiting for a client on stdio), or — far more usefully during development — point the Inspector at it (§6).

The part worth slowing down on is **how `add` became a protocol-legal tool**. You wrote a plain Python function; FastMCP did the protocol work:

- **The function name `add`** becomes the tool's `name`.
- **The docstring** `"Add two integers and return their sum."` becomes the tool's `description` — the text the host's model reads to decide *whether and how* to call it. This is not a comment; it is load-bearing prompt material, so write it for the model.
- **The type hints** `a: int, b: int` are introspected and compiled into a JSON Schema `inputSchema`. The host receives, roughly:

```json
{
  "name": "add",
  "description": "Add two integers and return their sum.",
  "inputSchema": {
    "type": "object",
    "properties": {
      "a": { "type": "integer" },
      "b": { "type": "integer" }
    },
    "required": ["a", "b"]
  }
}
```

This is the same `inputSchema` you would have hand-authored in Lecture 03 — FastMCP just derived it from the signature. Richer hints produce richer schemas: a `str` becomes `"type": "string"`, a parameter with a default (`unit: str = "celsius"`) becomes optional and drops out of `required`, a `Literal["celsius", "fahrenheit"]` becomes an `enum`, and a Pydantic model parameter expands into a nested object schema. The lesson: **the schema quality the model sees is exactly the type-annotation quality you write.** Vague hints (`a` with no type, or `dict` for a structured argument) produce a vague schema and worse tool selection.

---

**3. Resources & prompts in FastMCP**

Tools are model-controlled actions. The other two server→client primitives from Lecture 03 — **resources** (app-controlled data) and **prompts** (user-controlled templates) — get their own decorators.

A **resource** is addressable data, identified by a URI. FastMCP lets you template the URI: path parameters in the URI string map to function arguments, so one function serves a whole family of resources.

```python
from mcp.server.fastmcp import FastMCP

mcp = FastMCP("Demo")

# Static, well-known config values keyed by name.
CONFIG = {"theme": "dark", "max_retries": "3", "region": "us-east-1"}


@mcp.resource("config://{key}")
def read_config(key: str) -> str:
    """Return the value of a single configuration key."""
    return CONFIG.get(key, f"(no such key: {key})")
```

A host reading `config://region` invokes `read_config("region")` and gets back `"us-east-1"`. The `{key}` segment in the URI scheme becomes the `key` parameter — the same hint-to-schema mechanism as tools, applied to a URI template. Use resources for data the application wants to *pull into context* (files, records, config) rather than actions the model *performs*; that controller distinction from Lecture 03 is what keeps a clean server clean.

A **prompt** is a reusable, parameterized message template the user can invoke — think of the slash-commands a host surfaces. A FastMCP prompt returns the messages that should be injected:

```python
@mcp.prompt()
def review_code(language: str, code: str) -> str:
    """A prompt that asks for a focused code review."""
    return (
        f"Please review the following {language} code for bugs, security "
        f"issues, and style. Be specific and cite line numbers.\n\n"
        f"```{language}\n{code}\n```"
    )
```

Returning a plain string produces a single user message. When you need multi-turn structure or non-user roles, return a list of message objects instead:

```python
from mcp.server.fastmcp.prompts import base


@mcp.prompt()
def debug_session(error: str) -> list[base.Message]:
    """Seed a debugging conversation with the error already in context."""
    return [
        base.UserMessage("I hit this error and need help:"),
        base.UserMessage(error),
        base.AssistantMessage("Let's work through it. What were you running?"),
    ]
```

The exact import path for the message helpers (`base.UserMessage`, etc.) is one of the things that drifts between SDK versions — if it doesn't resolve, returning a list of plain `{"role": ..., "content": ...}` dicts is the version-robust fallback.

---

</details>

## 4. Context 对象与 lifespan

到目前为止的一切都是无状态、单向的。真实服务器还需要两样东西：一是让工具在运行期间能*反向对话*宿主的机制，二是能在服务器整个生命周期内持有昂贵共享状态（数据库连接池、HTTP 客户端）的机制。前者由 FastMCP 的 `Context` 提供，后者由 **lifespan** 提供。

### Context 对象

把某个参数的类型标注为 `Context`，FastMCP 就会注入它 —— 它**不会**出现在工具的 `inputSchema` 中，因此模型永远看不到它；它纯粹是你操作会话的句柄。借助它，工具可以完成你在 Lecture 04 中见过的*反向原语*那些操作：

- 向宿主**记录日志**：`ctx.info(...)`、`ctx.debug(...)`、`ctx.warning(...)`、`ctx.error(...)`。
- 为长耗时操作**上报进度**：`await ctx.report_progress(current, total)`。
- 无需客户端往返即可**读取资源**：`await ctx.read_resource(uri)`。
- **采样** —— 借用宿主的 LLM 做一次子补全：`await ctx.session.create_message(...)`（某些 SDK 版本提供便捷的 `ctx.sample(...)`）。
- **Elicit** —— 在任务中途暂停并向用户索取结构化输入：`await ctx.elicit(...)`。

```python
from mcp.server.fastmcp import Context, FastMCP

mcp = FastMCP("Demo")


@mcp.tool()
async def summarize_files(uris: list[str], ctx: Context) -> str:
    """Read several resources and return a combined, LLM-written summary."""
    await ctx.info(f"Summarizing {len(uris)} resource(s)")

    chunks: list[str] = []
    for i, uri in enumerate(uris):
        await ctx.report_progress(i, len(uris))      # progress bar in the host UI
        content, _mime = await ctx.read_resource(uri)  # server-side resource read
        chunks.append(content)

    corpus = "\n\n---\n\n".join(chunks)

    # Sampling: ask the HOST's model to do the summary. The server has no
    # API key of its own — it borrows the host's LLM (see Lecture 04).
    result = await ctx.session.create_message(
        messages=[
            {
                "role": "user",
                "content": {
                    "type": "text",
                    "text": f"Summarize these documents in 5 bullet points:\n\n{corpus}",
                },
            }
        ],
        max_tokens=400,
    )
    return result.content.text
```

有两点需要内化。第一，`ctx` 是被注入的，而非由模型传入 —— 宿主从不提供它，也从不在此 schema 中看到它。第二，**正是采样让服务器无需持有任何凭据就能“使用 LLM”**：它并不直接调用 Anthropic（或任何一方）；它*向宿主*请求补全，由宿主用自己的密钥和策略运行自己的模型。这正是反向通信的全部意义所在，也正是它让服务器保持廉价、可移植，并且不必操心凭据管理。

### Lifespan —— 共享的启动/关闭状态

你不会想每次工具调用都开一个数据库连接。**lifespan** 是一个异步上下文管理器，你把它交给 `FastMCP`；它在启动时 yield 出的东西会在服务器整个生命周期内持有，并在关闭时销毁，工具通过 context 访问它。

```python
from collections.abc import AsyncIterator
from contextlib import asynccontextmanager
from dataclasses import dataclass

from mcp.server.fastmcp import Context, FastMCP


@dataclass
class AppState:
    """Shared resources, created once at startup."""
    db: "Database"  # e.g. an asyncpg pool, an httpx.AsyncClient, etc.


@asynccontextmanager
async def lifespan(server: FastMCP) -> AsyncIterator[AppState]:
    db = await Database.connect("postgres://localhost/app")  # open once
    try:
        yield AppState(db=db)        # everything yielded is the shared context
    finally:
        await db.close()             # closed once, on shutdown


mcp = FastMCP("Demo", lifespan=lifespan)


@mcp.tool()
async def get_user(user_id: int, ctx: Context) -> str:
    """Look up a user by id using the shared DB pool."""
    state: AppState = ctx.request_context.lifespan_context
    row = await state.db.fetch_one("SELECT name FROM users WHERE id = $1", user_id)
    return row["name"] if row else "(not found)"
```

要记住的形态：**在 `yield` 之前打开，在 `finally` 中关闭，通过 `ctx.request_context.lifespan_context` 拿到 yield 出的对象。** 启动失败也应当在这里大声暴露 —— 连不上数据库的服务器应该在 lifespan 中失败，而不是在每次工具调用时静默降级。（指向 lifespan context 的确切属性路径也是一个对版本敏感的地方；如果 `ctx.request_context.lifespan_context` 无法解析，就查看你所安装版本中的 `Context` docstring。）

---


<details>
<summary>English original</summary>

**4. The Context object & lifespan**

Everything so far has been stateless and one-directional. Real servers need two more things: a way for a tool to *talk back* to the host while it runs, and a way to hold expensive shared state (a database pool, an HTTP client) across the server's life. FastMCP gives you `Context` for the first and **lifespan** for the second.

**The Context object**

Declare a parameter type-hinted as `Context` and FastMCP injects it — it does **not** appear in the tool's `inputSchema`, so the model never sees it; it is purely your handle to the session. Through it, a tool can do the things you met as the *reverse primitives* in Lecture 04:

- **Log** to the host: `ctx.info(...)`, `ctx.debug(...)`, `ctx.warning(...)`, `ctx.error(...)`.
- **Report progress** for long operations: `await ctx.report_progress(current, total)`.
- **Read a resource** without the client round-tripping: `await ctx.read_resource(uri)`.
- **Sample** — borrow the host's LLM for a sub-completion: `await ctx.session.create_message(...)` (some SDK versions expose a convenience `ctx.sample(...)`).
- **Elicit** — pause and ask the user for structured input mid-task: `await ctx.elicit(...)`.

```python
from mcp.server.fastmcp import Context, FastMCP

mcp = FastMCP("Demo")


@mcp.tool()
async def summarize_files(uris: list[str], ctx: Context) -> str:
    """Read several resources and return a combined, LLM-written summary."""
    await ctx.info(f"Summarizing {len(uris)} resource(s)")

    chunks: list[str] = []
    for i, uri in enumerate(uris):
        await ctx.report_progress(i, len(uris))      # progress bar in the host UI
        content, _mime = await ctx.read_resource(uri)  # server-side resource read
        chunks.append(content)

    corpus = "\n\n---\n\n".join(chunks)

    # Sampling: ask the HOST's model to do the summary. The server has no
    # API key of its own — it borrows the host's LLM (see Lecture 04).
    result = await ctx.session.create_message(
        messages=[
            {
                "role": "user",
                "content": {
                    "type": "text",
                    "text": f"Summarize these documents in 5 bullet points:\n\n{corpus}",
                },
            }
        ],
        max_tokens=400,
    )
    return result.content.text
```

Two things to internalize. First, `ctx` is injected, not passed by the model — the host never supplies it and never sees it in the schema. Second, **sampling is what makes a server able to "use an LLM" without holding any credentials**: it does not call Anthropic (or anyone) directly; it requests a completion *from the host*, which runs its own model under its own keys and policy. That is the whole point of the reverse direction, and it is what keeps servers cheap, portable, and out of the credential-management business.

**Lifespan — shared startup/shutdown state**

You do not want to open a database connection per tool call. The **lifespan** is an async context manager you hand to `FastMCP`; whatever it yields on startup is held for the server's life and torn down on shutdown, and tools reach it through the context.

```python
from collections.abc import AsyncIterator
from contextlib import asynccontextmanager
from dataclasses import dataclass

from mcp.server.fastmcp import Context, FastMCP


@dataclass
class AppState:
    """Shared resources, created once at startup."""
    db: "Database"  # e.g. an asyncpg pool, an httpx.AsyncClient, etc.


@asynccontextmanager
async def lifespan(server: FastMCP) -> AsyncIterator[AppState]:
    db = await Database.connect("postgres://localhost/app")  # open once
    try:
        yield AppState(db=db)        # everything yielded is the shared context
    finally:
        await db.close()             # closed once, on shutdown


mcp = FastMCP("Demo", lifespan=lifespan)


@mcp.tool()
async def get_user(user_id: int, ctx: Context) -> str:
    """Look up a user by id using the shared DB pool."""
    state: AppState = ctx.request_context.lifespan_context
    row = await state.db.fetch_one("SELECT name FROM users WHERE id = $1", user_id)
    return row["name"] if row else "(not found)"
```

The shape to remember: **open before `yield`, close in the `finally`, reach the yielded object via `ctx.request_context.lifespan_context`.** This is also where startup failures should surface loudly — a server that can't reach its database should fail in the lifespan, not silently degrade on every tool call. (The exact attribute path to the lifespan context is another version-sensitive spot; if `ctx.request_context.lifespan_context` doesn't resolve, check the `Context` docstring in your installed version.)

---

</details>

## 5. 结构化输出

默认情况下，工具返回文本。但 host 越来越希望得到**结构化、带类型**的结果，以便可靠解析——而模型提前知道结果的形状也大有好处。返回一个 Pydantic 模型（或带类型的 `dict`/`TypedDict`），SDK 就会做两件事：在工具定义旁发出一份 `outputSchema`，并返回*结构化内容*，host 可以把它当数据消费，而无需重新解析字符串。

```python
from pydantic import BaseModel, Field
from mcp.server.fastmcp import FastMCP

mcp = FastMCP("Weather")


class Weather(BaseModel):
    """A structured weather report."""
    location: str
    temperature_c: float = Field(description="Temperature in degrees Celsius")
    conditions: str
    humidity_pct: int


@mcp.tool()
def get_weather(city: str) -> Weather:
    """Get the current weather for a city as a structured report."""
    # (a real implementation would call a weather API here)
    return Weather(
        location=city,
        temperature_c=18.5,
        conditions="partly cloudy",
        humidity_pct=64,
    )
```

因为返回注解是 `Weather`，工具定义现在会带上一个由该模型推导出的 `outputSchema`——正是 §2 中 `inputSchema` 推导的镜像——而 host 收到的结果是带具名字段、经过校验的对象，而不是必须去刮取的 blob。字段级的 `Field(description=...)` 注解也会流入 schema，因此你可以像为每个参数写文档那样为每个字段写文档。凡是下游代码（或另一个工具）会消费的内容，都优先用这种方式；纯字符串返回只留给真正自由形式的文本。

---

## 6. 用 MCP Inspector 测试

在把 server 接入 host 之前，先单独测试它。**MCP Inspector** 是官方的交互式测试/调试 UI——一个 web 应用，它会启动你的 server、与它讲协议，并让你点遍它暴露的一切。没有 host，没有模型，没有配置文件：只有你和 server。

用 `uv` 针对你的 server 运行它（无需全局安装）：

```bash
npx @modelcontextprotocol/inspector uv run server.py
```

如果你是用普通的 `pip` 安装的，就把命令换掉：`npx @modelcontextprotocol/inspector python server.py`。官方 SDK 还附带一个快捷方式——`mcp dev server.py`——它会启动同一个 Inspector 并接上你的 server。

它打开后，按这份检查清单逐项过一遍——它与第 02–04 讲中的生命周期和原语一一对应：

- **能力。** 连接时，确认 server 报告的能力与你预期一致（tools / resources / prompts）。这就是第 02 讲里 `initialize` 握手过程的可视化。
- **`tools/list`。** 打开 Tools 标签页，核对每个工具的名称、描述，以及——关键的——它生成的 `inputSchema`。缺少或写错的类型提示，就会在这里表现为一个糟糕的 schema。
- **调用工具。** 填好参数并调用它。检查结果*以及*结构化输出 / `outputSchema`（如果你返回了模型，见 §5）。留意日志面板里有没有 `ctx.info(...)` 消息和进度更新。
- **读取资源。** 打开 Resources 标签页，解析一个模板化 URI（例如 `config://region`），并确认其内容和 MIME type。
- **运行 prompt。** 带参数调用一个 prompt，检查它产出的消息。

如果它在 Inspector 里是绿的，那它在 host 里也会表现正常。如果它在这里*不*绿，再跑到 Claude Desktop 里调试——那里失败会被吞掉，只以“工具不工作”的形式冒出来——就痛苦得多。**先 Inspector，后 host**，才是能替你省下数小时的工作流。

---


<details>
<summary>English original</summary>

**5. Structured output**

By default a tool returns text. But hosts increasingly want **structured, typed** results they can parse reliably — and the model benefits from knowing the shape in advance. Return a Pydantic model (or a typed `dict`/`TypedDict`) and the SDK does two things: it emits an `outputSchema` alongside the tool definition, and it returns *structured content* the host can consume as data rather than re-parsing a string.

```python
from pydantic import BaseModel, Field
from mcp.server.fastmcp import FastMCP

mcp = FastMCP("Weather")


class Weather(BaseModel):
    """A structured weather report."""
    location: str
    temperature_c: float = Field(description="Temperature in degrees Celsius")
    conditions: str
    humidity_pct: int


@mcp.tool()
def get_weather(city: str) -> Weather:
    """Get the current weather for a city as a structured report."""
    # (a real implementation would call a weather API here)
    return Weather(
        location=city,
        temperature_c=18.5,
        conditions="partly cloudy",
        humidity_pct=64,
    )
```

Because the return annotation is `Weather`, the tool definition now carries an `outputSchema` derived from the model — the mirror image of the `inputSchema` derivation in §2 — and the host receives the result as a validated object with named fields, not a blob it has to scrape. Field-level `Field(description=...)` annotations flow into the schema too, so you can document each field the way you documented each parameter. Prefer this for anything downstream code (or another tool) will consume; reserve plain-string returns for genuinely free-form text.

---

**6. Testing with MCP Inspector**

Before you wire a server into a host, test it in isolation. The **MCP Inspector** is the official interactive test/debug UI — a web app that launches your server, speaks the protocol to it, and lets you click through everything it exposes. No host, no model, no config file: just you and the server.

Run it against your server with `uv` (no global install needed):

```bash
npx @modelcontextprotocol/inspector uv run server.py
```

If you installed with plain `pip`, swap the command: `npx @modelcontextprotocol/inspector python server.py`. The official SDK also ships a shortcut — `mcp dev server.py` — which launches the same Inspector wired to your server.

Once it opens, work through this checklist — it maps one-to-one onto the lifecycle and primitives from Lectures 02–04:

- **Capabilities.** On connect, confirm the server reports the capabilities you expect (tools / resources / prompts). This is the `initialize` handshake from Lecture 02, made visible.
- **`tools/list`.** Open the Tools tab and verify each tool's name, description, and — critically — its generated `inputSchema`. This is where a missing or wrong type hint shows up as a bad schema.
- **Call a tool.** Fill in arguments and invoke it. Check the result *and* the structured output / `outputSchema` if you returned a model (§5). Watch the log pane for any `ctx.info(...)` messages and progress updates.
- **Read a resource.** Open the Resources tab, resolve a templated URI (e.g. `config://region`), and confirm the content and MIME type.
- **Run a prompt.** Invoke a prompt with arguments and inspect the messages it produces.

If it is green in the Inspector, it will behave in a host. If it is *not* green here, debugging it inside Claude Desktop — where failures are swallowed and surface only as "the tool didn't work" — is far more painful. **Inspector first, host second** is the workflow that saves you hours.

---

</details>

## 7. 接入 host 与打包

Host 通过一小段 JSON 配置块发现你的 server，该配置块告诉它 *该运行什么命令*。对于 stdio server，这就是命令及其参数，外加 server 所需的任何环境。下面是一份 Claude Desktop / Claude Code 配置（Desktop 上为 `claude_desktop_config.json`；Claude Code 对应的 MCP 配置）：

```json
{
  "mcpServers": {
    "demo": {
      "command": "uv",
      "args": ["--directory", "/abs/path/to/my-mcp-server", "run", "server.py"],
      "env": {
        "WEATHER_API_KEY": "sk-..."
      }
    }
  }
}
```

Host 启动 `uv --directory ... run server.py`，通过该进程的 stdio 讲 MCP，并在其 UI 中呈现 server 的 tools、resources 和 prompts。`env` 块是 secrets 送达 *本地* server 的方式 —— 注意这是 stdio/本地这一套；**远程 server 的认证方式完全不同，走 OAuth 2.1，而那正是 Lecture 06 的全部主题。** 不要把 secret 放在 env var 里发往运行在别人机器上的 server。

官方 SDK 提供一条一行命令，在开发期间为你写出这个块：

```bash
mcp install server.py
```

若要向他人**分发**，2026 年的惯例是：把 server 发布为包，让用户用 `uvx your-server`（零安装执行，相当于 `npx`）或 `pip install your-server` 运行它；附上一个带 console-script 入口点的 `pyproject.toml`，使命令保持稳定；以及 —— 为了公开可发现性 —— 在 **MCP registry** 中列出它。打包约定与 registry 是另一个独立主题，见 **Lecture 08**。

### TypeScript 等价物

相同的协议，相同的心智模型，不同的语言。TS SDK 的高层 server 注册一个 tool，用一个 [Zod](https://zod.dev/) schema 替代 Python 的类型提示：

```typescript
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { z } from "zod";

const server = new McpServer({ name: "Demo", version: "1.0.0" });

server.registerTool(
  "add",
  {
    description: "Add two integers and return their sum.",
    inputSchema: { a: z.number().int(), b: z.number().int() },
  },
  async ({ a, b }) => ({ content: [{ type: "text", text: String(a + b) }] }),
);

await server.connect(new StdioServerTransport());
```

与 Python `add` 相同的三种成分：一个 name，一段给模型的 description，以及一个 schema（这里用 Zod，那边从类型提示推断）。host 配置结构上完全一致 —— 只需把 `command`/`args` 指向 `node build/server.js` 而不是 `uv run server.py`。

---

你现在有了完整的本地构建闭环：编写一个带 tools、resources 和 prompts 的 FastMCP server；通过 `Context` 回连 host，用于 logging、progress、sampling 和 elicitation；在 lifespan 中持有共享状态；返回 Pydantic 模型以实现结构化输出；在 Inspector 中测到全绿；并把它接入 host。到目前为止的一切都通过 stdio 运行在 `localhost` 上。**Lecture 06** 把同一个 server 带到互联网上 —— Streamable HTTP、会话，以及本地 env var 无法提供的 OAuth 2.1 授权。

---

## 截至

2026 年 6 月，锚定 **2025-11-25** MCP 规范修订版。官方 `mcp` Python SDK 正朝着 **v2（2026 年 beta）** 前进，它和独立的 FastMCP 2.x 都逐版本演进 —— decorator 名称、`Context` 方法集（`ctx.sample` 对 `ctx.session.create_message`）、lifespan 签名，以及 prompt-message 的 import 路径都随版本变动过。把每个代码片段都当作快照，在依赖某个精确调用之前对照你安装的版本核实（`mcp version`，然后是模块 docstrings）。


<details>
<summary>English original</summary>

**7. Wiring into a host & packaging**

A host discovers your server through a small JSON config block that tells it *what command to run*. For a stdio server, that is the command and its arguments, plus any environment the server needs. Here is a Claude Desktop / Claude Code config (`claude_desktop_config.json` on Desktop; the equivalent MCP config for Claude Code):

```json
{
  "mcpServers": {
    "demo": {
      "command": "uv",
      "args": ["--directory", "/abs/path/to/my-mcp-server", "run", "server.py"],
      "env": {
        "WEATHER_API_KEY": "sk-..."
      }
    }
  }
}
```

The host launches `uv --directory ... run server.py`, speaks MCP over the process's stdio, and surfaces the server's tools, resources, and prompts in its UI. The `env` block is how secrets reach a *local* server — note this is the stdio/local story; **remote servers authenticate completely differently, over OAuth 2.1, and that is the entire subject of Lecture 06.** Do not try to ship a secret in an env var to a server running on someone else's machine.

The official SDK gives you a one-liner that writes this block for you during development:

```bash
mcp install server.py
```

For **distribution** to other people, the 2026 norms are: publish the server as a package and let users run it with `uvx your-server` (zero-install execution, the analog of `npx`) or `pip install your-server`; ship a `pyproject.toml` with a console-script entry point so the command is stable; and — for public discoverability — list it in the **MCP registry**. Packaging conventions and the registry are their own topic, covered in **Lecture 08**.

**The TypeScript equivalent**

The same protocol, the same mental model, different language. The TS SDK's high-level server registers a tool with a [Zod](https://zod.dev/) schema standing in for Python's type hints:

```typescript
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import { z } from "zod";

const server = new McpServer({ name: "Demo", version: "1.0.0" });

server.registerTool(
  "add",
  {
    description: "Add two integers and return their sum.",
    inputSchema: { a: z.number().int(), b: z.number().int() },
  },
  async ({ a, b }) => ({ content: [{ type: "text", text: String(a + b) }] }),
);

await server.connect(new StdioServerTransport());
```

Same three ingredients as the Python `add`: a name, a description for the model, and a schema (Zod here, inferred from type hints there). The host config is identical in shape — just point `command`/`args` at `node build/server.js` instead of `uv run server.py`.

---

You now have the full local build loop: write a FastMCP server with tools, resources, and prompts; reach back to the host through `Context` for logging, progress, sampling, and elicitation; hold shared state in a lifespan; return Pydantic models for structured output; test it green in the Inspector; and wire it into a host. Everything to this point has run on `localhost` over stdio. **Lecture 06** takes the same server to the internet — Streamable HTTP, sessions, and the OAuth 2.1 authorization that local env vars cannot provide.

---

**Current as of**

June 2026, pinned to the **2025-11-25** MCP specification revision. The official `mcp` Python SDK is heading toward a **v2 (beta in 2026)**, and both it and the standalone FastMCP 2.x evolve release-to-release — decorator names, the `Context` method set (`ctx.sample` vs `ctx.session.create_message`), the lifespan signature, and prompt-message import paths have all shifted across versions. Treat every snippet as a snapshot and verify against your installed version (`mcp version`, then the module docstrings) before relying on an exact call.

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/MCP for AI Agents/Lecture-05.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/MCP%20for%20AI%20Agents/Lecture-05.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
