---
title: jetson-llm 文档
description: jetson-llm 文档
---

# jetson-llm 文档

| 文档 | 描述 |
|----------|-------------|
| [architecture.md](architecture.md) | 系统架构、数据流、设计决策 |
| [memory.md](memory.md) | 内存优先设计：budget、KV cache、scratch pool、OOM 防护 |
| [kernels.md](kernels.md) | 全部 6 个 CUDA kernel：各自的功能、Orin 调优、性能说明 |
| [gguf.md](gguf.md) | GGUF 格式解析：config、张量、tokenizer、权重映射 |
| [engine.md](engine.md) | Transformer 前向传播、decode 循环、CUDA graphs、采样 |
| [jetson-hal.md](jetson-hal.md) | 功耗模式、热管理、sysfs 接口、实时统计 |
| [server.md](server.md) | HTTP API：端点、请求/响应格式、部署 |
| [build.md](build.md) | 构建系统、依赖、交叉编译说明 |
| [testing.md](testing.md) | 测试计划、测试说明、预期结果、调试 |
| [performance.md](performance.md) | benchmark、性能剖析、优化目标 |


<details>
<summary>English original</summary>

**jetson-llm Documentation**

| Document | Description |
|----------|-------------|
| [architecture.md](architecture.md) | System architecture, data flow, design decisions |
| [memory.md](memory.md) | Memory-first design: budget, KV cache, scratch pool, OOM guard |
| [kernels.md](kernels.md) | All 6 CUDA kernels: what they do, Orin tuning, performance notes |
| [gguf.md](gguf.md) | GGUF format parsing: config, tensors, tokenizer, weight mapping |
| [engine.md](engine.md) | Transformer forward pass, decode loop, CUDA graphs, sampling |
| [jetson-hal.md](jetson-hal.md) | Power modes, thermal management, sysfs interface, live stats |
| [server.md](server.md) | HTTP API: endpoints, request/response format, deployment |
| [build.md](build.md) | Build system, dependencies, cross-compilation notes |
| [testing.md](testing.md) | Test plan, test descriptions, expected results, debugging |
| [performance.md](performance.md) | Benchmarking, profiling, optimization targets |

</details>

---

> 原文：[`Projects/jetson-llm-runtime/docs/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/docs/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
