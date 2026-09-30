---
title: L4T 定制（生产环境）
description: L4T 定制（生产环境）
---

# L4T 定制（生产环境）

<div class="course-identity l4t" markdown="1">
<div class="course-identity__icon">L4T</div>
<div markdown="1">
<p class="course-identity__eyebrow">方向 B3 · L4T 定制</p>
<p class="course-identity__title">定制 Jetson Linux、BSP 资产、启动产物、设备树、烧录与部署镜像。</p>
<p class="course-identity__meta">产物：定制 L4T 镜像流程 · 度量：启动、烧录时间、日志、可复现性</p>
</div>
</div>


**阶段 4 — 方向 B — Nvidia Jetson** · 模块 3/7

> **重点：** 掌握 **Linux for Tegra (L4T)** 与 **JetPack** 这一**生产**级技术栈：可复现的镜像、最小化的根文件系统、kernel 与设备树集成、可靠的启动与更新，以及加固——让你交付产品，而不是与平台缠斗。

**上一节：** [2. 定制载板设计](../02-定制载板设计与启动/Guide.md) · **下一节：** [4. FSP 定制](../04-FSP固件支持包定制/Guide.md) · **Runtime 配套：** [5.1 外设访问](../05-应用开发/01-外设访问/Guide.md)

**范围边界：** 本模块负责 **BSP**、**kernel**、**设备树**、**烧录**与**生产镜像**相关工作。若要在运行中的目标板上从 Linux 用户态访问已配置好的外设，参见 [5.1 外设访问](../05-应用开发/01-外设访问/Guide.md)。

---

## 真实产品为何选 L4T + JetPack

对多数小团队和独立开发者而言，**L4T 配 JetPack 是务实的默认选择**：NVIDIA 支持的驱动、CUDA 与多媒体栈、熟悉的 Debian/Ubuntu 工具链，以及快速迭代能力。这里的生产工作意味着**冻结基线**、**自动化烧录与 rootfs 构建**，以及**把镜像当作产物**——而不是在每台设备上临时 `apt`。

---

## 生产实践（需要掌握什么）

| 领域 | 生产目标 |
|------|-----------------|
| **最小化根文件系统** | 从 **L4T sample rootfs** 起步，删掉 GUI 和不需要的包——镜像更小、活动部件更少、攻击面更小。记录每一处包差异。 |
| **Kernel 与设备树** | 用受支持的 **`flash.sh`** 工作流把**自定义设备树**、**overlay**、**驱动**和**补丁**固化进去。把 kernel 改动放在**小而带版本控制的 Git 仓库**中，让烧录可复现。 |
| **Systemd** | 让应用跑在 **systemd unit** 下，配好重启策略、依赖关系和日志——在嵌入式硬件上实现可靠的 bring-up（上电点亮/调通）与恢复。 |
| **容器** | 用 **Docker**（JetPack 上支持良好）跑产品栈，以获得**隔离**、**可复现的 runtime**，以及更轻松地推送到**设备集群**。 |
| **安全加固** | 禁用未使用的服务，优先用 **SSH 密钥**而非密码，并在威胁模型有要求之处叠加**防火墙**（`ufw` 或等价方案）与**强制访问控制**（例如 **AppArmor**）。 |
| **启动与更新** | 让**分区布局**、**A/B**（若采用）和 **OTA** 流程与镜像的构建和签名方式对齐——参见下文平台深入解析。 |

---

## 实操工作流（高层）

1. **冻结 JetPack / L4T 基线**——板卡 SKU、载板、确切的 JetPack 版本。把 **apt** 和 NVIDIA 仓库配置固定在脚本或文档中。
2. **列出产品差异**：包、kernel 配置、DTB 或 overlay 改动、**systemd** unit、需要 **mask/disable** 的服务。
3. 沿 sample rootfs 路径**构建最小化、可复现的 rootfs**：除非产品需要，否则剥掉桌面栈；用自己的烧录或镜像 recipe 做快照。
4. 先**优先选择受支持的集成路径**——`flash.sh`、`extlinux.conf`、用于 DT overlay 的 **`nvpmodel`**、**`jetson-io`**——再考虑维护一棵沉重的 fork 版 kernel 树。
5. **自动化验证**：烧录后跑冒烟测试（GPU、摄像头、网络），若以镜像方式发布更新，还要做 **OTA** 试运行。

---


<details>
<summary>English original</summary>

**L4T customization (production)**

<div class="course-identity l4t" markdown="1">
<div class="course-identity__icon">L4T</div>
<div markdown="1">
<p class="course-identity__eyebrow">Track B3 · L4T Customization</p>
<p class="course-identity__title">Customize Jetson Linux, BSP assets, boot artifacts, device trees, flashing, and deployment images.</p>
<p class="course-identity__meta">Artifact: custom L4T image flow · Measure: boot, flash time, logs, reproducibility</p>
</div>
</div>


**Phase 4 — Track B — Nvidia Jetson** · Module 3 of 7

> **Focus:** Master **Linux for Tegra (L4T)** with **JetPack** as a **production** stack: reproducible images, a minimal root filesystem, kernel and device-tree integration, reliable boot and updates, and hardening—so you ship products instead of fighting the platform.

**Previous:** [2. Custom Carrier Board Design](../02-定制载板设计与启动/Guide.md) · **Next:** [4. FSP Customization](../04-FSP固件支持包定制/Guide.md) · **Runtime companion:** [5.1 Peripheral Access](../05-应用开发/01-外设访问/Guide.md)

**Scope boundary:** this module owns **BSP**, **kernel**, **device tree**, **flash**, and **production image** work. For Linux userspace access to already-configured peripherals on a running target, use [5.1 Peripheral Access](../05-应用开发/01-外设访问/Guide.md).

---

**Why L4T + JetPack for real products**

For most small teams and solo developers, **L4T with JetPack is the pragmatic default**: NVIDIA-supported drivers, CUDA and multimedia stacks, familiar Debian/Ubuntu tooling, and fast iteration. Production work here means **freezing baselines**, **automating flashes and rootfs**, and **treating the image as an artifact**—not ad-hoc `apt` on every device.

---

**Production practices (what to master)**

| Area | Production goal |
|------|-----------------|
| **Minimal root filesystem** | Start from the **L4T sample rootfs**, then remove GUI and packages you do not need—smaller images, fewer moving parts, smaller attack surface. Document every package delta. |
| **Kernel & device tree** | Use the supported **`flash.sh`** workflow to bake in **custom device trees**, **overlays**, **drivers**, and **patches**. Keep kernel changes in a **small, versioned Git repo** so flashes are repeatable. |
| **Systemd** | Run your application under **systemd units** with restart policies, dependencies, and logging—reliable bring-up and recovery on embedded hardware. |
| **Containers** | Run the product stack in **Docker** (well supported on JetPack) for **isolation**, **reproducible runtime**, and easier rollout across a **fleet** of devices. |
| **Security hardening** | Disable unused services, prefer **SSH keys** over passwords, and layer **firewall** (`ufw` or equivalent) and **mandatory access control** (for example **AppArmor**) where your threat model requires it. |
| **Boot & updates** | Align **partition layout**, **A/B** if you use it, and **OTA** flows with how you build and sign images—see Platform deep dives below. |

---

**Practical workflow (high level)**

1. **Freeze a JetPack / L4T baseline**—board SKU, carrier, exact JetPack version. Pin **apt** and NVIDIA repo configuration in scripts or docs.
2. **List product deltas**: packages, kernel config, DTB or overlay changes, **systemd** units, services to **mask/disable**.
3. **Build a minimal, reproducible rootfs** from the sample rootfs path: strip desktop stacks unless the product needs them; snapshot with your own flash or image recipe.
4. **Prefer supported integration paths** first—`flash.sh`, `extlinux.conf`, **`nvpmodel`**, **`jetson-io`** for DT overlays—before maintaining a heavy forked kernel tree.
5. **Automate validation**: smoke tests after flash (GPU, camera, network), plus **OTA** dry-runs if you ship image-based updates.

---

</details>

## NVIDIA 官方文档

请使用与你交付版本同一 **JetPack / L4T 主版本**线的 **Jetson Linux Developer Guide**；下面的存档副本仅用于说明相关主题——如果你的版本不同，请在 [Jetson documentation](https://docs.nvidia.com/jetson/) 下打开对应的 release。

| 主题 | 对生产的意义 |
|-------|------------------------------|
| [**Jetson Module Adaptation and Bring-Up**](https://docs.nvidia.com/jetson/archives/r35.3.1/DeveloperGuide/text/HR/JetsonModuleAdaptationAndBringUp.html) | 从 **developer kit** 迁移到 **custom carrier**：板卡命名、rootfs 配置、MB1/MB2（pinmux、EEPROM）、**设备树**移植、PCIe/USB、烧录构建**镜像**，以及硬件/软件 bring-up（上电点亮/调通）**检查清单**。 |
| [**Kernel customization**](https://docs.nvidia.com/jetson/archives/r34.1/DeveloperGuide/index.html#page/Tegra%20Linux%20Driver%20Package%20Development%20Guide/kernel_custom.html) | 用 **Git** 同步 kernel 源码、构建并安装 kernel、**DTB** 以及必要的签名/加密、外部模块，以及可选的**实时** kernel 包工作流。 |

---

## BCT 参考（本模块内）

- [T23x-Deployment.md](T23x-Deployment.md) — **T23x BCT**（DU-10990-001），**面向 Orin Nano**：**P3767/P3768** 文件名与 flash 变量的速查表，外加逐章的 DTS 与 legacy CFG 对照（pinmux、prod、PMIC、storage、UPHY、SDRAM、GPIO intmap、SCR）。较长的 PDF 摘录仅作示意。
- [Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md](Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md) — **Jetson Orin NX / Nano** 模块适配与 bring-up（与 `T23x-Deployment.md` 搭配用于 BCT）：板卡命名、rootfs、MB1 pinmux/GPIO、DT 移植、PCIe、USB、UPHY、`l4t_initrd_flash.sh`、env 覆盖。
- [ODMDATA-and-GPIO-Jetson-Linux.md](ODMDATA-and-GPIO-Jetson-Linux.md) — **ODM data / `ODMDATA`**、**UPHY** 关系，以及 **pinmux vs `devmem` vs sysfs vs `libgpiod`**（面向 JetPack 6 的心智模型）。
- [Orin-Nano-8GB-Custom-Board-L4T-Engineering-Flow.md](Orin-Nano-8GB-Custom-Board-L4T-Engineering-Flow.md) — **端到端工程流程图**：主机准备、BSP + 源码、DTB、kernel、rootfs、flash、bring-up 测试、版本管理（附带 **JP6 / custom carrier** 注意事项）。
- [FSP / SPE customization](../04-FSP固件支持包定制/Guide.md) — **Sensor Processing Engine (AON Cortex-R5)** 固件：**FreeRTOS** 上的 NVIDIA **FSP**，构建/烧录 **`spe_t194.bin` / `spe_t234.bin`**，pinmux/SCR/BPMP 交互（BCT 与 `flash.sh` 工作流的配套内容）。

---

## EchoPilot AI（完整示例 / 厂商参考）

**EchoMAV** 为 **EchoPilot AI**（其载板上的 Orin NX / Orin Nano）发布了一条具体的 **L4T bring-up** 路径：Jetson Linux BSP、sample rootfs、`apply_binaries.sh`、创建默认用户、**设备树 overlay**，以及向 NVMe 的 **initrd flash**。可将其与 NVIDIA 的模块适配指南并列为“custom carrier + headless Orin”的**参考实现**。

| 资源 | 用途 |
|----------|-------------------|
| [**echopilot_ai_bsp** (GitHub)](https://github.com/EchoMAV/echopilot_ai_bsp) | 上游 BSP 脚本与分支（例如 Rev1B 用 `board_revision_1b`）。在构建主机上 clone 该仓库；按他们文档所述，对 `Linux_for_Tegra` 树运行 `install_l4t_orin.sh`。 |
| **EchoPilot AI documentation**（EchoMAV MkDocs，例如 *Building L4T (Orin NX and Orin Nano)*） | 分步的主机环境搭建（他们面向 **Ubuntu 22.04**）、给定 **Jetson Linux** 版本的下载名称（其指南中的示例：**36.4.3**）、面向 **external NVMe** 的 `l4t_initrd_flash.sh` 调用方式，以及运维注意事项（USB autosuspend、首次构建镜像后的 `--flash-only`）。 |
| **本地快照** — `echopilot_ai_bsp-board_revision_1b` | 本仓库中固定引用的 **board_revision_1b** 树副本：`Linux_for_Tegra/kernel/dtb/` 下的 overlay（例如关闭显示、启用串口）、BCT 相关文件、补丁，以及 `install_l4t_orin.sh`。调试 DT 或 flash layout 时，可将其与自己的 `Linux_for_Tegra` 做 diff。 |

**EchoPilot 的 Orin 指南中列出的硬件 / 软件差异**（对任何第三方载板都适用的模式）：载板可能**不**采用与 NVIDIA dev kit 相同的 **board ID EEPROM** 方案；**headless** 产品通常需要在 DT 中**禁用显示通路**，以便启动完成；产品特定的 **UART** 布线通过 **overlay** 启用。务必让 BSP 的 **branch / revision** 与单板丝印上的 **board revision** 保持一致。

---


<details>
<summary>English original</summary>

**Official NVIDIA documentation**

Use the **Jetson Linux Developer Guide** for the same **major JetPack / L4T** line you ship; archived copies below illustrate the topics—open the matching release under [Jetson documentation](https://docs.nvidia.com/jetson/) if your version differs.

| Topic | Why it matters for production |
|-------|------------------------------|
| [**Jetson Module Adaptation and Bring-Up**](https://docs.nvidia.com/jetson/archives/r35.3.1/DeveloperGuide/text/HR/JetsonModuleAdaptationAndBringUp.html) | Moving from a **developer kit** to a **custom carrier**: board naming, rootfs configuration, MB1/MB2 (pinmux, EEPROM), **device tree** porting, PCIe/USB, **flashing** the build image, and hardware/software bring-up **checklists**. |
| [**Kernel customization**](https://docs.nvidia.com/jetson/archives/r34.1/DeveloperGuide/index.html#page/Tegra%20Linux%20Driver%20Package%20Development%20Guide/kernel_custom.html) | Syncing kernel sources with **Git**, building and installing the kernel, **DTB** and signing/encryption where required, external modules, and optional **real-time** kernel package workflow. |

---

**BCT reference (in this module)**

- [T23x-Deployment.md](T23x-Deployment.md) — **T23x BCT** (DU-10990-001), **Orin Nano–oriented**: cheat sheet for **P3767/P3768** filenames and flash vars, plus chapter-by-chapter DTS vs legacy CFG (pinmux, prod, PMIC, storage, UPHY, SDRAM, GPIO intmap, SCR). Long PDF excerpts remain illustrative.
- [Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md](Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md) — **Jetson Orin NX / Nano** module adaptation and bring-up (paired with `T23x-Deployment.md` for BCT): board naming, rootfs, MB1 pinmux/GPIO, DT porting, PCIe, USB, UPHY, `l4t_initrd_flash.sh`, env overrides.
- [ODMDATA-and-GPIO-Jetson-Linux.md](ODMDATA-and-GPIO-Jetson-Linux.md) — **ODM data / `ODMDATA`**, **UPHY** relationship, and **pinmux vs `devmem` vs sysfs vs `libgpiod`** (JetPack 6–oriented mental model).
- [Orin-Nano-8GB-Custom-Board-L4T-Engineering-Flow.md](Orin-Nano-8GB-Custom-Board-L4T-Engineering-Flow.md) — **End-to-end engineering flowchart**: host prep, BSP + sources, DTB, kernel, rootfs, flash, bring-up tests, versioning (with **JP6 / custom carrier** caveats).
- [FSP / SPE customization](../04-FSP固件支持包定制/Guide.md) — **Sensor Processing Engine (AON Cortex-R5)** firmware: NVIDIA **FSP** on **FreeRTOS**, build/flash **`spe_t194.bin` / `spe_t234.bin`**, pinmux/SCR/BPMP interaction (companion to BCT and `flash.sh` workflows).

---

**EchoPilot AI (worked example / vendor reference)**

**EchoMAV** publishes a concrete **L4T bring-up** path for **EchoPilot AI** (Orin NX / Orin Nano on their carrier): Jetson Linux BSP, sample rootfs, `apply_binaries.sh`, default user creation, **device-tree overlays**, and **initrd flash** to NVMe. Treat it as a **reference implementation** for “custom carrier + headless Orin” alongside NVIDIA’s module adaptation guide.

| Resource | What to use it for |
|----------|-------------------|
| [**echopilot_ai_bsp** (GitHub)](https://github.com/EchoMAV/echopilot_ai_bsp) | Upstream BSP scripts and branches (e.g. `board_revision_1b` for Rev1B). Clone this on your build host; run `install_l4t_orin.sh` against your `Linux_for_Tegra` tree as in their docs. |
| **EchoPilot AI documentation** (EchoMAV MkDocs, e.g. *Building L4T (Orin NX and Orin Nano)*) | Step-by-step host setup (they target **Ubuntu 22.04**), download names for a given **Jetson Linux** drop (example in their guide: **36.4.3**), `l4t_initrd_flash.sh` invocation for **external NVMe**, and operational notes (USB autosuspend, `--flash-only` after first image build). |
| **Local snapshot** — `echopilot_ai_bsp-board_revision_1b` | Copy of the **board_revision_1b** tree pinned in this repo: overlays under `Linux_for_Tegra/kernel/dtb/` (e.g. disable display, enable serial), BCT-related files, patches, and `install_l4t_orin.sh`. Diff this against your own `Linux_for_Tegra` when debugging DT or flash layout. |

**Hardware / software deltas** called out in EchoPilot’s Orin guide (useful pattern for any third-party carrier): carrier may **not** use the same **board ID EEPROM** scheme as the NVIDIA dev kit; **headless** products often need **display paths disabled** in DT so boot completes; product-specific **UART** routing is enabled via **overlays**. Always match **branch / revision** of the BSP to the silkscreen **board revision** on the unit.

---

</details>

## 深入剖析（平台模块 1）

将以下平台指南用作 L4T 相关生产工作的**实现**细节：

- [Rootfs 与 A/B 冗余](../01-Nvidia-Jetson平台/12-Orin-Nano-Rootfs与AB冗余/Guide.md)
- [OTA 深入剖析](../01-Nvidia-Jetson平台/08-Orin-Nano-OTA深入解析/Guide.md)
- [Kernel 内部机制](../01-Nvidia-Jetson平台/06-Orin-Nano内核内部机制/Guide.md)
- [RT Linux](../01-Nvidia-Jetson平台/10-Orin-Nano-RT-Linux深入解析/Guide.md)（当延迟至关重要时）
- [安全加固](../01-Nvidia-Jetson平台/13-Orin-Nano安全/Guide.md)

---

## 与边缘 AI 的关系（模块 5）

**L4T 定制**定义**操作系统上运行什么**（kernel、驱动、rootfs、CUDA 安装路径、服务）。**边缘 AI 优化**以该基础为前提，聚焦于**模型**（TensorRT、量化、DeepStream）。在把推理当作主要瓶颈之前，先完成上述平台深入剖析，或与之并行推进。


<details>
<summary>English original</summary>

**Deep dives (Platform module 1)**

Use these Platform guides as **implementation** detail for L4T-facing production work:

- [Rootfs and A/B redundancy](../01-Nvidia-Jetson平台/12-Orin-Nano-Rootfs与AB冗余/Guide.md)
- [OTA deep dive](../01-Nvidia-Jetson平台/08-Orin-Nano-OTA深入解析/Guide.md)
- [Kernel internals](../01-Nvidia-Jetson平台/06-Orin-Nano内核内部机制/Guide.md)
- [RT Linux](../01-Nvidia-Jetson平台/10-Orin-Nano-RT-Linux深入解析/Guide.md) (when latency matters)
- [Security hardening](../01-Nvidia-Jetson平台/13-Orin-Nano安全/Guide.md)

---

**Relationship to Edge AI (module 5)**

**L4T customization** defines **what runs on the OS** (kernel, drivers, rootfs, CUDA install path, services). **Edge AI Optimization** assumes that foundation and focuses on **models** (TensorRT, quantization, DeepStream). Finish or parallel the platform deep dives above before treating inference as the main bottleneck.

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/3. L4T Customization/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/3.%20L4T%20Customization/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
