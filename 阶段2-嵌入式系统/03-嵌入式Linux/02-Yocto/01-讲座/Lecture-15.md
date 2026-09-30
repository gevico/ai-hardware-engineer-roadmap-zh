---
title: 讲座 15 — 术语表与快速参考
description: 讲座 15 — 术语表与快速参考
---

# 讲座 15 — 术语表与快速参考

**课程：** [Yocto 指南](../Guide.md) | **阶段 2 — 嵌入式 Linux、Yocto**

**上一讲：** [讲座 14](Lecture-14.md) | **下一讲：** [讲座 16 — 延伸阅读](Lecture-16.md)

---

## 术语表

| 术语 | 通俗解释 |
|------|----------------|
| BitBake | 任务执行器：读取元数据，并行执行编译/打包/镜像步骤 |
| Recipe | 单个软件件的构建指令 |
| Layer | 元数据 + 配置的版本化集合 |
| Image | 面向某一产品配置的根文件系统加 kernel/启动产物 |
| MACHINE | 硬件/BSP 选择 |
| DISTRO | 跨镜像的策略选择（在已配置的情况下） |
| Sysroot | 代表目标上实际存在的头文件/库，用于链接 |
| BSP | 板级支持包元数据：machine 配置、kernel、启动固件的粘合部分 |

## 需要慢慢记住的命令（不必第一天全记住）

- source oe-init-build-env
- bitbake 加你的镜像或 recipe 目标
- bitbake-layers show-layers

---

**上一讲：** [讲座 14](Lecture-14.md) | **下一讲：** [讲座 16 — 延伸阅读](Lecture-16.md)


<details>
<summary>English original</summary>

**Lecture 15 — Glossary and quick reference**

**Course:** [Yocto guide](../Guide.md) | **Phase 2 — Embedded Linux, Yocto**

**Previous:** [Lecture 14](Lecture-14.md) | **Next:** [Lecture 16 — Further reading](Lecture-16.md)

---

**Glossary**

| Term | Plain English |
|------|----------------|
| BitBake | Task executor: reads metadata, runs compile/package/image steps with parallelism |
| Recipe | Build instructions for one piece of software |
| Layer | Versioned bundle of metadata + configuration |
| Image | Root filesystem plus kernel/boot artifacts for a product configuration |
| MACHINE | Hardware/BSP selection |
| DISTRO | Policy selection across images (where configured) |
| Sysroot | Headers/libs representing what exists on target for linking |
| BSP | Board Support Package metadata: machine configs, kernels, boot firmware glue |

**Commands to memorize slowly (not all on day one)**

- source oe-init-build-env
- bitbake with your image or recipe target
- bitbake-layers show-layers

---

**Previous:** [Lecture 14](Lecture-14.md) | **Next:** [Lecture 16 — Further reading](Lecture-16.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Yocto/Lecture/Lecture-15.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Yocto/Lecture/Lecture-15.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
