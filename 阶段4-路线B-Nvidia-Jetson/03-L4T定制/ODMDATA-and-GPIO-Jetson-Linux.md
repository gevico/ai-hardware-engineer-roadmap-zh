---
title: Jetson Linux 上的 ODMDATA、pinmux 与 GPIO
description: Jetson Linux 上的 ODMDATA、pinmux 与 GPIO
---

# Jetson Linux 上的 ODMDATA、pinmux 与 GPIO

**阶段 4 — 方向 B — Nvidia Jetson** · L4T 定制（配套笔记）

本笔记把 **ODM data**、**UPHY / PCIe** 配置，以及人们说“Jetson 上的 GPIO”时所指的**三个不同层次**串在一起：**MB1 BCT pinmux**、**PADCTL 寄存器 poke（`devmem`）**，以及 **userspace GPIO**（`libgpiod`）。它是对 [Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md](Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md)（载板 bring-up）与 [T23x-Deployment.md](T23x-Deployment.md)（BCT / MB1 DTS 细节）的补充。

务必对照 [Jetson Linux Developer Guide](https://docs.nvidia.com/jetson/) 中**你所用 JetPack 版本线**与模块（Orin NX/Nano vs Xavier 等）确认**寄存器含义**、**flash 变量**与 **sysfs 路径**。

---

## 什么是 ODMDATA？

在 **Jetson Linux**（人们仍常称之为 **L4T** 的 BSP）中，**ODM data** 是一个**启动期配置**值，平台用它（连同其他 BCT / 固件输入）在 Linux 完全起来之前选择 **SoC 级的复用与选项**。

实践中，工程师所说的 **`ODMDATA`** 指你在 **flash 配置**侧设置的**字符串或数值**，以便 **MB1 / BPMP / bootloader** 选对 **UPHY** 预设（USB 还是 PCIe 还是 GBE 的 lane map），以及芯片文档中记录的相关**特性**位。

它**不**等同于“runtime 时随便写个 `ioctl`”——它**主要在你构建/flash 时决定**（或者在你把已知的 `.conf` + BCT 集合交给工厂时决定）。

### ODMDATA（连同 BCT / DT）通常用来指派的项

| 领域 | 思路（高层） |
|------|-------------------|
| **UPHY lane map** | 哪些高速 lane 是 **USB3**、**PCIe**、**Ethernet (GBE)** 等。 |
| **PCIe 角色** | 某些预设有关于在特定设计中控制器是用作 **root port** 还是**端点**（确切机制**因 SoC 与版本而异**——请使用针对你模块的 NVIDIA PCIe 端点主题）。 |
| **其他启动开关** | 平台相关的位（watchdog、电源或 strap 相关选项）在**官方**文档中以**位域**表给出——不要仅凭论坛帖子猜测。 |

对于 **Orin NX / Nano (T234)**，适配指南把许多 UPHY 选择表达为 **board `.conf`** 中的一个 **`ODMDATA="…"`** 字符串（常在 source `p3767.conf.common` **之后追加**）。参见模块适配笔记中的 [UPHY lane 配置](Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md)。

### 如何**设置** ODMDATA（量产式做法）

1. 在烧写设备的**主机**上于 **`Linux_for_Tegra/`** 中操作。
2. 编辑或扩展你**板级专用**的 `*.conf`（NVIDIA 惯用做法：把变量设在 `source ...conf.common` **之后**，从而**覆盖**默认值）。
3. 把 **`ODMDATA=`** 设为指南中针对你载板的**预设**（Orin 上的 HSIO / GBE 字符串记录在适配 / 烧写主题中）。
4. 用与你的存储布局相配的受支持脚本**烧写**（`flash.sh`、`l4t_initrd_flash.sh` 等——参见开发者指南中的 *Flashing support*）。

较早的帖子有时会提到通过 **`flash.sh`** 标志为**较老**模块传入一个十六进制字（例如 Xavier 时代的 **PCIe 端点**示例）。把这些当作**线索**：**受支持**的 CLI 与变量名随 **BSP 版本**变化——请使用 **`flash.sh --help`** 以及针对**你所用**软件包的指南。

### 启动后如何**查看**与 ODM 相关的数据

Linux 在 **`/sys/firmware/devicetree/`** 下暴露 **device-tree blob**。NVIDIA 与社区的帖子常指向如下路径：

`/sys/firmware/devicetree/base/chosen/plugin-manager/odm-data/`

在运行中的系统上可以**发现**这些节点（路径随版本与启动链不同）：

```bash
find /sys/firmware/devicetree -iname '*odm*' 2>/dev/null | head
```

属性往往是**二进制**的；对目录里的文件用 **`hexdump -C`** 或 **`od`** 读取。

用它来回答：“**启动的镜像认为 ODM/plugin-manager 视图是什么？**”它不能替代你在 `Linux_for_Tegra/` 中对 **`.conf`** 与 **DT** 做**版本控制**。

---

## 三个层次：pinmux vs `devmem` vs GPIO userspace

在 Jetson 上“把一个引脚做成 GPIO”时，通常要碰**不止一个层次**：

| 层次 | 控制什么 | 是否持久？ |
|-------|-------------------|-----------|
| **1. MB1 BCT pinmux（DTS）** | 某个 ball 是 **SFIO**（I2C、UART……）还是 **GPIO**、上下拉等 | **是**（重新构建并重烧 BCT/相关镜像之后） |
| **2. `devmem` / PADCTL** | 对引脚做**复用**与 **pad 配置**的**硬件寄存器** | 若你只从 Linux 写 RAM 映射的寄存器则**否**——**复位即丢失**，除非固件保存过（通常没有） |
| **3. `libgpiod` / gpiochip** | 引脚在 pinmux 中已是 **GPIO** 后，**GPIO line** 的方向与电平 | runtime 下为 **N**/`A`；重启后 pinmux 仍然优先 |

**JetPack 6** 的方向：NVIDIA 弃用旧的 **GPIO sysfs**（`/sys/class/gpio`）；**产品**代码请在 **`/dev/gpiochip*`** 之上使用 **`libgpiod`**。

---


<details>
<summary>English original</summary>

**ODMDATA, pinmux, and GPIO on Jetson Linux**

**Phase 4 — Track B — Nvidia Jetson** · L4T customization (companion note)

This note ties together **ODM data**, **UPHY / PCIe** configuration, and the **three different layers** people mean when they say “GPIO on Jetson”: **MB1 BCT pinmux**, **PADCTL register pokes (`devmem`)**, and **userspace GPIO** (`libgpiod`). It complements [Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md](Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md) (carrier bring-up) and [T23x-Deployment.md](T23x-Deployment.md) (BCT / MB1 DTS detail).

Always confirm **register meanings**, **flash variables**, and **sysfs paths** against the [Jetson Linux Developer Guide](https://docs.nvidia.com/jetson/) for **your JetPack line** and module (Orin NX/Nano vs Xavier, etc.).

---

**What is ODMDATA?**

In **Jetson Linux** (the BSP people still often call **L4T**), **ODM data** is a **boot-time configuration** value the platform uses (with other BCT / firmware inputs) to select **SoC-level muxing and options** before Linux is fully up.

In practice, engineers talk about **`ODMDATA`** as the **string or value** you set from the **flash configuration** side so that **MB1 / BPMP / bootloader** pick the right **UPHY** preset (USB vs PCIe vs GBE lane maps), and related **feature** bits documented for your chip.

It is **not** the same thing as “a random `ioctl` at runtime”—it is **primarily decided when you build/flash** (or when you ship a known `.conf` + BCT set to the factory).

**Typical things ODMDATA (plus BCT / DT) helps steer**

| Area | Idea (high level) |
|------|-------------------|
| **UPHY lane maps** | Which high-speed lanes are **USB3**, **PCIe**, **Ethernet (GBE)**, etc. |
| **PCIe roles** | Some presets interact with whether a controller is used as **root port** vs **endpoint** in a given design (exact mechanism is **SoC- and release-specific**—use NVIDIA’s PCIe endpoint topic for your module). |
| **Other boot knobs** | Platform-specific bits (watchdog, power, or strap-related options) appear in **official** docs as **bitfield** tables—do not guess from forum posts alone. |

For **Orin NX / Nano (T234)**, the adaptation guide expresses many UPHY choices as an **`ODMDATA="…"`** string in the **board `.conf`** (often **appended after** sourcing `p3767.conf.common`). See [UPHY lane configuration](Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md) in the module adaptation note.

**How to **set** ODMDATA (production-style)**

1. Work in **`Linux_for_Tegra/`** on the **host** that flashes the device.
2. Edit or extend your **board-specific** `*.conf` (NVIDIA pattern: set variables **after** `source ...conf.common` so you **override** defaults).
3. Set **`ODMDATA=`** to the **preset** from the guide for your carrier (HSIO / GBE strings on Orin are documented in the adaptation / flashing topics).
4. **Flash** with the supported script for your storage layout (`flash.sh`, `l4t_initrd_flash.sh`, etc.—see *Flashing support* in the developer guide).

Older posts sometimes mention passing a hex word via **`flash.sh`** flags for **older** modules (e.g. Xavier-era **PCIe endpoint** examples). Treat those as **hints**: the **supported** CLI and variable names change by **BSP version**—use **`flash.sh --help`** and the guide for **your** package.

**How to **inspect** ODM-related data after boot**

Linux exposes **device-tree blobs** under **`/sys/firmware/devicetree/`**. NVIDIA and community posts often point to paths such as:

`/sys/firmware/devicetree/base/chosen/plugin-manager/odm-data/`

On a running system you can **discover** nodes (paths differ by release and boot chain):

```bash
find /sys/firmware/devicetree -iname '*odm*' 2>/dev/null | head
```

Properties are often **binary**; read with **`hexdump -C`** or **`od`** on the files inside those directories.

Use this to answer: “**What did the booted image think the ODM/plugin-manager view is?**” It does not replace **source control** of your **`.conf`** and **DT** in `Linux_for_Tegra/`.

---

**Three layers: pinmux vs `devmem` vs GPIO userspace**

When you “make a pin GPIO” on Jetson, you usually touch **more than one layer**:

| Layer | What it controls | Persists? |
|-------|-------------------|-----------|
| **1. MB1 BCT pinmux (DTS)** | Whether a ball is **SFIO** (I2C, UART, …) or **GPIO**, pulls, etc. | **Yes** (after rebuild + reflash of BCT/related images) |
| **2. `devmem` / PADCTL** | **Hardware registers** that **mux** and **pad-configure** the pin | **No** if you only poke RAM-mapped registers from Linux—**lost on reset** unless firmware saved it (usually it did not) |
| **3. `libgpiod` / gpiochip** | **GPIO line** direction and level **once** the pin is already a **GPIO** in the pinmux | **N**/`A` at runtime; pinmux still wins on reboot |

**JetPack 6** direction: NVIDIA deprecates the old **GPIO sysfs** (`/sys/class/gpio`); use **`libgpiod`** on top of **`/dev/gpiochip*`** for **product** code.

---

</details>

## 1. `devmem`（直接 MMIO）

**它是什么：** 一个小工具（通常为 **`busybox devmem`**），在 kernel 替你映射的**物理**（或总线可见）**地址**上**读写一个 32-bit 字**。在 Jetson bring-up（上电点亮/调通）文档中，它用于依据 **TRM** 验证 **pinmux** 计算时 **peek/poke PADCTL** 及类似模块。

**何时使用**

- **Bring-up / 调试**：确认某引脚的 **PADCTL** 寄存器 **base + offset** 与 **Orin TRM** 及你的电子表格一致。
- **短期实验**：在把改动固化到 **BCT `.dtsi`** **之前**，先看某个 **mux** 改动能否让**示波器**或**逻辑分析仪**的 trace 看起来正确。

**风险**

- 你在**绕过** kernel 的正常驱动模型。
- 若与已使能的驱动相争，错误的地址或取值可能**挂死** SoC、使电源域出现**毛刺**，或**损坏**硬件。
- 改动通常是**易失的**——除非在 **BCT** 中复现同样意图并重新刷机，否则只能当作**实验环境专用**。

**典型用法**（摘自 NVIDIA 的适配文档）：

```bash
sudo apt-get install -y busybox
sudo busybox devmem 0x02430080      # read
sudo busybox devmem 0x02430080 w 0x…  # write (syntax per busybox build)
```

**TRM：** [Jetson Orin SoC TRM](https://developer.nvidia.com/orin-series-soc-technical-reference-manual) — **Pinmux** / **PADCTL** 章节。

---

## 2. GPIO **sysfs**（`/sys/class/gpio`）— 遗留

**它过去是什么：** **`/sys/class/gpio/`** 下的一套**虚拟文件系统** API：按 **Linux GPIO number** **export** 一条 line，再通过写入小文本文件设置 **direction** 和 **value**。

**在 JetPack 6 上的状态**

- NVIDIA 的方向与上游 Linux 一致：对于新工作，**sysfs GPIO 已废弃**。
- 旧脚本和教程仍展示 **`echo 396 > /sys/class/gpio/export`** 风格的流程——它们可能**失效**，或与能感知 **libgpiod** 的驱动**发生竞争**。

**何时仍会出现**

- **遗留**的 bring-up 脚本、CI，或尚未移植的第三方库。

**量产建议：** 规划使用 **`libgpiod`**（或 kernel 驱动），而不是 **sysfs**。

---

## 3. `libgpiod` — 当前标准

**它是什么：** 面向 **GPIO character device** API 的一套 **C 库**和 **CLI 工具**：**`/dev/gpiochipN`**（`GPIO_CDEV`）。kernel 会暴露 **chips** 和 **lines**，并在 DT 提供的情况下给出稳定的 **line names**。

**常用 CLI 工具**

| 工具 | 用途 |
|------|---------|
| **`gpiodetect`** | 列出 **`gpiochip`** 设备 |
| **`gpioinfo`** | Lines、names、**used** / **unused**、**direction** |
| **`gpioget`** | 读取一条 line |
| **`gpioset`** | 驱动一条 line（测试中常配合 **`--mode=wait`**） |

**为什么优先选它**

- 相比 sysfs 的 **export** 玩法，它是**原子**的且**抗竞争**。
- 可使用来自**设备树**的 **line labels**（若有提供）。
- 与 **mainline** Linux 的方向一致。

**前置要求：** 该引脚必须已在 **MB1 BCT** 中**复用为 GPIO**（或采用你能完全理解的安全 runtime 等效做法）。**`gpioset`** **不**能取代 **pinmux spreadsheet → `.dtsi`**。

**文档：** [libgpiod](https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git/about/) (kernel.org).

---

## 对比（速览）

| 机制 | 你触碰的东西 | 风险 | 典型用途 |
|-----------|-----------|------|-------------|
| **`devmem`** | **SoC 寄存器**（如 PADCTL） | **高** | 与 TRM 对齐的**调试**、验证某个地址、**临时**的 mux 实验 |
| **sysfs GPIO** | **遗留**的 sysfs 文件 | 能用时**低** | 新的 JetPack 6 设计应**避免** |
| **`libgpiod`** | **`/dev/gpiochip*`** lines | **低**（常规 API） | **pinmux** 正确后用于**应用**、**服务**、**测试** |

---

## 端到端心智模型（JetPack 6，Orin 级）

1. **产品意图** — 在 **pinmux spreadsheet** 中为每个 ball 选定**功能**（UART、I2C、GPIO、CSI、PCIe、USB……）；为 **MB1 BCT** 生成 **`.dtsi`**；通过 **`ODMDATA`** 及相关 **DT** 对齐 **UPHY** 选择（参见模块适配与 T23x BCT 文档）。
2. **刷机** — 板级 **`*.conf`** 选择 **BCT** 片段、**DTB** 和 **`ODMDATA`** 预设。
3. **Bring-up 调试** — 仅在需要**寄存器级**确认时使用 **`devmem`**；在**工程日志**中记录**地址**与 **bit** 的含义。
4. **Runtime GPIO** — 在 **userspace** 中用 **`libgpiod`** 做 **line** 控制；对**出货**产品，**pinmux** 要放在 **BCT** 里，而不是放在 **shell** `devmem` 脚本里。

---

## 另见

- [Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md](Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md) — **UPHY**、**`ODMDATA`**、**PCIe**、**USB**、**flash**
- [T23x-Deployment.md](T23x-Deployment.md) — **MB1 BCT** DTS 片段（**ODM data for T234** 见 BCT 部署指南）
- [Jetson Linux Developer Guide](https://docs.nvidia.com/jetson/) — 你所使用 release 对应的 **Flashing support**、**PCIe endpoint**、**GPIO** / **Jetson-IO** 主题


<details>
<summary>English original</summary>

**1. `devmem` (direct MMIO)**

**What it is:** A tiny utility (commonly **`busybox devmem`**) that **reads or writes a 32-bit word** at a **physical** (or bus-visible) **address** the kernel maps for you. On Jetson bring-up docs, it is used to **peek/poke PADCTL** and similar blocks while validating **pinmux** math from the **TRM**.

**When to use it**

- **Bring-up / debug**: confirm **base + offset** for a pin’s **PADCTL** register matches the **Orin TRM** and your spreadsheet.
- **Short experiments**: see if a **mux** change makes a **scope** or **logic analyzer** trace look right **before** you freeze the change in **BCT `.dtsi`**.

**Risks**

- You are **bypassing** the kernel’s normal driver model.
- Wrong addresses or values can **hang** the SoC, **glitch** power domains, or **damage** hardware if you fight enabled drivers.
- Changes are usually **volatile**—treat as **lab only** unless you mirror the same intent in **BCT** and reflash.

**Typical pattern** (from NVIDIA’s adaptation text):

```bash
sudo apt-get install -y busybox
sudo busybox devmem 0x02430080      # read
sudo busybox devmem 0x02430080 w 0x…  # write (syntax per busybox build)
```

**TRM:** [Jetson Orin SoC TRM](https://developer.nvidia.com/orin-series-soc-technical-reference-manual) — **Pinmux** / **PADCTL** chapters.

---

**2. GPIO **sysfs** (`/sys/class/gpio`) — legacy**

**What it was:** A **virtual filesystem** API under **`/sys/class/gpio/`**: **export** a line by **Linux GPIO number**, then set **direction** and **value** by writing to small text files.

**Status on JetPack 6**

- NVIDIA’s direction matches upstream Linux: **sysfs GPIO is deprecated** for new work.
- Old scripts and tutorials still show **`echo 396 > /sys/class/gpio/export`** style flows—they may **break** or **race** with **libgpiod**-aware drivers.

**When it still shows up**

- **Legacy** bring-up scripts, CI, or third-party libraries not yet ported.

**Production guidance:** plan **`libgpiod`** (or a kernel driver) instead of **sysfs**.

---

**3. `libgpiod` — current standard**

**What it is:** A **C library** and **CLI tools** for the **GPIO character device** API: **`/dev/gpiochipN`** (`GPIO_CDEV`). The kernel exposes **chips** and **lines** with stable **line names** where the DT provides them.

**Common CLI tools**

| Tool | Purpose |
|------|---------|
| **`gpiodetect`** | List **`gpiochip`** devices |
| **`gpioinfo`** | Lines, names, **used** / **unused**, **direction** |
| **`gpioget`** | Read a line |
| **`gpioset`** | Drive a line (often with **`--mode=wait`** for tests) |

**Why prefer it**

- **Atomic** and **race-resistant** compared to sysfs **export** games.
- Works with **line labels** from **device tree** (when present).
- Aligns with **mainline** Linux direction.

**Prerequisite:** the pin must already be **muxed as GPIO** in **MB1 BCT** (or a safe runtime equivalent you fully understand). **`gpioset`** does **not** replace **pinmux spreadsheet → `.dtsi`**.

**Docs:** [libgpiod](https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git/about/) (kernel.org).

---

**Comparison (quick)**

| Mechanism | You touch | Risk | Typical use |
|-----------|-----------|------|-------------|
| **`devmem`** | **SoC registers** (e.g. PADCTL) | **High** | TRM-aligned **debug**, prove an address, **temporary** mux experiments |
| **sysfs GPIO** | **Legacy** sysfs files | **Low** if it works | **Avoid** for new JetPack 6 designs |
| **`libgpiod`** | **`/dev/gpiochip*`** lines | **Low** (normal API) | **Apps**, **services**, **tests** once **pinmux** is correct |

---

**End-to-end mental model (JetPack 6, Orin-class)**

1. **Product intent** — Pick **functions** per ball (UART, I2C, GPIO, CSI, PCIe, USB, …) in the **pinmux spreadsheet**; generate **`.dtsi`** for **MB1 BCT**; align **UPHY** choice via **`ODMDATA`** and related **DT** (see module adaptation + T23x BCT doc).
2. **Flash** — Board **`*.conf`** selects **BCT** fragments, **DTB**, and **`ODMDATA`** preset.
3. **Bring-up debug** — Use **`devmem`** only if you need **register-level** confirmation; document the **address** and **bit** meaning in your **engineering log**.
4. **Runtime GPIO** — Use **`libgpiod`** for **line** control in **userspace**; keep **pinmux** in **BCT**, not in **shell** `devmem` scripts, for **shipping** products.

---

**See also**

- [Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md](Jetson-Module-Adaptation-Bring-Up-Orin-NX-Nano.md) — **UPHY**, **`ODMDATA`**, **PCIe**, **USB**, **flash**
- [T23x-Deployment.md](T23x-Deployment.md) — **MB1 BCT** DTS fragments (**ODM data for T234** appears in the BCT deployment guide)
- [Jetson Linux Developer Guide](https://docs.nvidia.com/jetson/) — **Flashing support**, **PCIe endpoint**, **GPIO** / **Jetson-IO** topics for your release

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/3. L4T Customization/ODMDATA-and-GPIO-Jetson-Linux.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/3.%20L4T%20Customization/ODMDATA-and-GPIO-Jetson-Linux.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
