---
title: Yocto Project — 嵌入式 Linux 发行版工程
description: Yocto Project — 嵌入式 Linux 发行版工程
---

# Yocto Project — 嵌入式 Linux 发行版工程

<div class="course-identity auto-course" style="--course-accent: #0284c7; --course-accent-rgb: 2, 132, 199;" markdown="1">
<div class="course-identity__icon">YPEL</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度解析 · 嵌入式系统</p>
<p class="course-identity__title">Yocto Project — 嵌入式 Linux 发行版工程的专属课程标识。</p>
<p class="course-identity__meta">产物：bring-up 或固件演示 · 度量：启动、延迟、功耗、可靠性</p>
</div>
</div>


面向硬件与软件工程师的结构化课程，目标是**构建、掌控并交付**一套定制嵌入式 Linux，而不只是把它装起来。首要原则是清晰：你最终应获得一个能经受版本更名和新板卡考验的**心智模型**。

**时间投入（典型）：** 模块 0–6 兼职学习需 4–10 周；包含生产主题的完整课程配合真实硬件需 3–6 个月。

**你将能够做到**

- 解释 BitBake、recipe、layer 与 image 如何端到端关联。
- 仅凭元数据复现一次构建（团队可用，而非「在我笔记本上能跑」）。
- 添加软件包、为组件打补丁，并交付更小、可测试的镜像。
- 借助日志与任务图排查失败，而非靠猜。
- 判断何时 Yocto 是合适的工具——以及何时 Buildroot 或厂商 BSP 更快。

---

## 分步讲义

每个模块是 **[Lecture/](01-讲座/README.md)** 下的独立文件，带有实验以及上一节/下一节导航。按顺序学习：[Lecture-01](01-讲座/Lecture-01.md) 到 [Lecture-16](01-讲座/Lecture-16.md)。

| # | 主题 | 讲义 |
|---|--------|---------|
| 1 | 如何使用本课程 | [Lecture-01.md](01-讲座/Lecture-01.md) |
| 2 | 模块 0 — 前置要求与主机环境搭建 | [Lecture-02.md](01-讲座/Lecture-02.md) |
| 3 | 模块 1 — Yocto 是什么（以及不是什么） | [Lecture-03.md](01-讲座/Lecture-03.md) |
| 4 | 模块 2 — 一张图看懂架构 | [Lecture-04.md](01-讲座/Lecture-04.md) |
| 5 | 模块 3 — 第一次成功构建 | [Lecture-05.md](01-讲座/Lecture-05.md) |
| 6 | 模块 4 — recipe：系统的原子单元 | [Lecture-06.md](01-讲座/Lecture-06.md) |
| 7 | 模块 5 — layer：元数据如何保持可维护 | [Lecture-07.md](01-讲座/Lecture-07.md) |
| 8 | 模块 6 — image、软件包与特性 | [Lecture-08.md](01-讲座/Lecture-08.md) |
| 9 | 模块 7 — kernel、bootloader、设备树 | [Lecture-09.md](01-讲座/Lecture-09.md) |
| 10 | 模块 8 — SDK 与应用开发流程 | [Lecture-10.md](01-讲座/Lecture-10.md) |
| 11 | 模块 9 — 像工程师一样调试构建 | [Lecture-11.md](01-讲座/Lecture-11.md) |
| 12 | 模块 10 — 许可证、合规、供应链 | [Lecture-12.md](01-讲座/Lecture-12.md) |
| 13 | 模块 11 — 性能、缓存与 CI | [Lecture-13.md](01-讲座/Lecture-13.md) |
| 14 | 结业项目 | [Lecture-14.md](01-讲座/Lecture-14.md) |
| 15 | 术语表与快速参考 | [Lecture-15.md](01-讲座/Lecture-15.md) |
| 16 | 延伸阅读（链接 + 维护者提示） | [Lecture-16.md](01-讲座/Lecture-16.md) |

---

所有分步内容、实验、术语表与外部链接都位于上述 **[Lecture/](01-讲座/README.md)** 文件中。

**专业提示：** 当文档与互联网说法不一致时，请相信**与你当前 Yocto 发行分支完全匹配的文档**——元数据语法与变量名确实会演变。


<details>
<summary>English original</summary>

**Yocto Project — Embedded Linux Distribution Engineering**

<div class="course-identity auto-course" style="--course-accent: #0284c7; --course-accent-rgb: 2, 132, 199;" markdown="1">
<div class="course-identity__icon">YPEL</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Embedded Systems</p>
<p class="course-identity__title">Specialized course identity for Yocto Project — Embedded Linux Distribution Engineering.</p>
<p class="course-identity__meta">Artifact: bring-up or firmware demo · Measure: boot, latency, power, reliability</p>
</div>
</div>


A structured course for hardware and software engineers who need to **build, own, and ship** a custom embedded Linux—not just install one. The goal is clarity first: you should finish with a **mental model** that survives release renames and new boards.

**Time investment (typical):** 4–10 weeks part-time for Modules 0–6; full course including production topics 3–6 months alongside real hardware.

**What you will be able to do**

- Explain how BitBake, recipes, layers, and images relate end-to-end.
- Reproduce a build from metadata alone (team-ready, not "works on my laptop").
- Add a package, patch a component, and ship a smaller, testable image.
- Debug failures using logs and task graphs instead of guessing.
- Know when Yocto is the right tool—and when Buildroot or a vendor BSP is faster.

---

**Step-by-step lectures**

Each module is a separate file under **[Lecture/](01-讲座/README.md)** with labs and previous/next navigation. Work in order: [Lecture-01](01-讲座/Lecture-01.md) through [Lecture-16](01-讲座/Lecture-16.md).

| # | Topic | Lecture |
|---|--------|---------|
| 1 | How to use this course | [Lecture-01.md](01-讲座/Lecture-01.md) |
| 2 | Module 0 — Prerequisites and host setup | [Lecture-02.md](01-讲座/Lecture-02.md) |
| 3 | Module 1 — What Yocto is (and is not) | [Lecture-03.md](01-讲座/Lecture-03.md) |
| 4 | Module 2 — Architecture in one picture | [Lecture-04.md](01-讲座/Lecture-04.md) |
| 5 | Module 3 — First successful build | [Lecture-05.md](01-讲座/Lecture-05.md) |
| 6 | Module 4 — Recipes: the atoms of the system | [Lecture-06.md](01-讲座/Lecture-06.md) |
| 7 | Module 5 — Layers: how metadata stays maintainable | [Lecture-07.md](01-讲座/Lecture-07.md) |
| 8 | Module 6 — Images, packages, and features | [Lecture-08.md](01-讲座/Lecture-08.md) |
| 9 | Module 7 — Kernel, bootloader, device tree | [Lecture-09.md](01-讲座/Lecture-09.md) |
| 10 | Module 8 — SDK and application workflow | [Lecture-10.md](01-讲座/Lecture-10.md) |
| 11 | Module 9 — Debugging builds like an engineer | [Lecture-11.md](01-讲座/Lecture-11.md) |
| 12 | Module 10 — Licenses, compliance, supply-chain | [Lecture-12.md](01-讲座/Lecture-12.md) |
| 13 | Module 11 — Performance, caching, and CI | [Lecture-13.md](01-讲座/Lecture-13.md) |
| 14 | Capstone projects | [Lecture-14.md](01-讲座/Lecture-14.md) |
| 15 | Glossary and quick reference | [Lecture-15.md](01-讲座/Lecture-15.md) |
| 16 | Further reading (links + maintainer tip) | [Lecture-16.md](01-讲座/Lecture-16.md) |

---

All step-by-step content, labs, glossary, and external links live in the **[Lecture/](01-讲座/README.md)** files above.

**Pro tip:** when documentation and the internet disagree, trust the **docs matching your exact Yocto release branch**—metadata syntax and variable names do evolve.

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Yocto/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Yocto/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
