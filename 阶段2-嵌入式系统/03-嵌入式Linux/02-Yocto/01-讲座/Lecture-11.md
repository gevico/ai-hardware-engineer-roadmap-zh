---
title: 第 11 讲 — 模块 9：像工程师一样调试构建
description: 第 11 讲 — 模块 9：像工程师一样调试构建
---

# 第 11 讲 — 模块 9：像工程师一样调试构建

**课程：** [Yocto guide](../Guide.md) | **阶段 2 — 嵌入式 Linux、Yocto**

**上一讲：** [第 10 讲](Lecture-10.md) | **下一讲：** [第 12 讲 — 模块 10](Lecture-12.md)

---

## 1. 从日志顶部开始读失败

BitBake 报告失败时：

1. 确定 **recipe** 和 **task**（`do_compile`、`do_configure`、…）。
2. 打开错误信息中打印的 **task log** 文件路径。
3. 滚动到 **第一处**编译器或配置错误，而不是最后一行级联报错。

## 2. 高信噪比命令（概念层面）

- bitbake -g TARGET（把 TARGET 换成你的镜像或 recipe），用于生成依赖图产物。
- `devtool`（在你的工作流中可用时）— 在受控工作区中修改源码。

## 3. 常见失败类别

| 症状 | 通常意味着 |
|---------|-------------|
| Fetch 失败 | 网络、代理、缺少 `SRC_URI` 校验和、URL 失效 |
| 补丁被拒 | 版本漂移；分支 pin 有误 |
| CMake/autotools 缺少依赖 | `DEPENDS` 不完整；`PACKAGECONFIG` 有误 |
| Rootfs 体积暴涨 | 镜像中误入 `-dev` 包 |

## 4. 实验 9 — 故意把它搞坏

在一份 *一次性* 的 layer 副本中移除一个 **依赖**，重新构建，并练习 **把日志回溯到根因**。再把改动还原。

**完成条件：**你能讲清楚 *为什么* 该失败属于 fetch、compile 还是 packaging。

---

**上一讲：** [第 10 讲](Lecture-10.md) | **下一讲：** [第 12 讲 — 模块 10](Lecture-12.md)


<details>
<summary>English original</summary>

**Lecture 11 — Module 9: Debugging builds like an engineer**

**Course:** [Yocto guide](../Guide.md) | **Phase 2 — Embedded Linux, Yocto**

**Previous:** [Lecture 10](Lecture-10.md) | **Next:** [Lecture 12 — Module 10](Lecture-12.md)

---

**1. Read failures from the top of the log**

When BitBake reports failure:

1. Identify the **recipe** and **task** (`do_compile`, `do_configure`, …).
2. Open the **task log** file path printed in the error.
3. Scroll to the **first** compiler or configuration error, not the last cascading line.

**2. High-signal commands (conceptual)**

- bitbake -g TARGET (replace TARGET with your image or recipe) for dependency graph artifacts.
- `devtool` (when available in your workflow) — modify sources in a controlled workspace.

**3. Common failure classes**

| Symptom | Often means |
|---------|-------------|
| Fetch failures | Network, proxy, missing `SRC_URI` checksum, retired URLs |
| Patch rejects | Version drift; branch pin wrong |
| CMake/autotools missing deps | `DEPENDS` incomplete; wrong `PACKAGECONFIG` |
| Rootfs size explosions | accidental `-dev` packages in image |

**4. Lab 9 — Break it on purpose**

Remove a **dependency** in a *throwaway* layer copy, rebuild, and practice **tracing the log back to root cause**. Revert the change.

**Done when:** you can articulate *why* the failure belongs to fetch vs compile vs packaging.

---

**Previous:** [Lecture 10](Lecture-10.md) | **Next:** [Lecture 12 — Module 10](Lecture-12.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Yocto/Lecture/Lecture-11.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Yocto/Lecture/Lecture-11.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
