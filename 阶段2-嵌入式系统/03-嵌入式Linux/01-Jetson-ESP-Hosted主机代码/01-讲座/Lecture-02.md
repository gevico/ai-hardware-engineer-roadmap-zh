---
title: 第 2 讲 — Jetson 上的构建、加载与板级策略
description: 第 2 讲 — Jetson 上的构建、加载与板级策略
---

# 第 2 讲 — Jetson 上的构建、加载与板级策略

**课程：** [Jetson ESP-Hosted Host Code guide](../Guide.md) · **阶段 2 — 嵌入式 Linux**

**上一篇：** [第 01 讲](Lecture-01.md) · **下一篇：** [第 03 讲 — SPI 传输与 IRQ](Lecture-03.md)

---

## 1. 从 shell 脚本入手，而不是 C 文件

阅读：

- `esp_hosted_ng/host/jetson_orin_nano_init.sh`

该文件给出经过验证的板级假设：

- `RESETPIN=-1`
- `HANDSHAKEPIN=471`
- `DATAREADYPIN=433`
- `SPI_BUS_NUM=0`
- `SPI_CHIP_SELECT=0`
- `SPI_MODE=2`
- `CLOCKSPEED=10`

这不是随意的配置。它是 Jetson 开发套件的**板级策略层**。

嵌入式 Linux 要点：

- 上游代码通常只假设一种板子
- 量产 bring-up（上电点亮/调通）通常需要板级专用的封装或策略层

## 相关 Linux 内核概念

本讲与以下内容最契合：

- [OS 第 5 讲 — 内核模块、启动流程与设备树](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-05.md)
- [OS 第 17 讲 — Linux 设备驱动模型与设备树](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md)

第 5 讲解释了为什么可加载模块与板级描述是彼此分离的关注点：板子说明存在什么硬件，模块则为其提供行为。正是这种分离，使得 `jetson_orin_nano_init.sh` 可以专注于 Jetson 策略，而 `esp32_spi.ko` 专注于传输与子系统集成。

第 17 讲解释了驱动绑定与 Linux 设备模型，这正是理解 `spidev` unbind 步骤的正确思维模型。该脚本并不是“在做什么取巧的事”；它是把通用驱动从真实的 SPI 设备上解除，以便目标驱动能绑定到同一个内核对象。

这里重要的嵌入式 Linux 观念是：**“加载模块”并不等于“硬件现在可用”。** 模块可以加载成功，却仍然无法绑定到正确的设备、无法申请到正确的 GPIO，或者在之后向 Wi‑Fi 或 Bluetooth 子系统注册时失败。

这也是本讲花这么多篇幅讲 shell 脚本策略的原因。在真实系统中，小型的封装脚本往往承载板级事实：哪条总线是真实的、内核期望哪种 GPIO 编号方案、驱动 reset 是否安全，以及是否必须先移除像 `spidev` 这样的通用占位驱动。

这里要留意的内核接口与概念有：

- 树外内核模块
- `insmod` 与 `module_param(...)`
- 设备归属与驱动绑定
- 设备树在真实驱动接管之前先暴露总线

默认值在 helper 中直接给出：

```bash
IF_TYPE="spi"
MODULE_NAME="esp32_spi.ko"
RESETPIN=-1
HANDSHAKEPIN=471
DATAREADYPIN=433
SPI_BUS_NUM=0
SPI_CHIP_SELECT=0
SPI_MODE=2
CLOCKSPEED=10
```


这一个小块涵盖了大部分 Jetson 专有策略：

- SPI 传输，而非 SDIO
- 总线 `0`，片选 `0`
- 传统全局 GPIO `471` 与 `433`
- 默认禁用 reset，因为板级实际行为比理论上的便利更重要

---

## 2. 先弄清 Jetson 排针映射，再相信 GPIO 编号

ESP-Hosted helper 使用像 `471` 和 `433` 这样的 Linux GPIO 编号，但它们**并不**等同于 Jetson 物理 40-pin 排针上的标注。

先使用这份 NVIDIA 载板引脚参考：

![Jetson Orin Nano 40-pin header pinout](../../../../Assets/images/jetson-orin-nano-40-pin-header.png)

来源：[NVIDIA Jetson Orin Nano carrier-board pin header diagram](https://developer.download.nvidia.com/embedded/images/jetsonOrinNano/user_guide/images/jonano_cbspec_figure_3-1_white-bg.png#only-light)

对于 SPI 上的 ESP-Hosted，最需要关心的排针引脚是：

- SPI1 MOSI：引脚 `19`
- SPI1 MISO：引脚 `21`
- SPI1 SCK：引脚 `23`
- SPI1 CS0：引脚 `24`
- SPI1 CS1：引脚 `26`
- 接地：引脚 `6`、`9`、`14`、`20`、`25`、`30`、`34`、`39`

这张图有用，因为它把两套不同的编号体系区分开来：

- **排针引脚编号**，如 `19` 和 `24`，是物理连接器位置
- **Linux GPIO 编号**，如 `471` 和 `433`，是驱动 helper 使用的、内核可见的 ID

这一区分在实际 bring-up 中很重要。接线按**排针引脚**，而调试 helper 脚本与模块参数则用 **Linux GPIO 编号**。

---

## 3. 为什么 `resetpin=-1` 是一个严肃的工程选择

经过验证的 Jetson 流程有意使用：

- `resetpin=-1`

这意味着主机驱动默认**不**驱动 ESP reset。

这在真实硬件上为何重要：

- 由 Jetson 驱动的 reset 可能干扰 ESP 启动
- 也可能干扰来自另一台主机的 USB 烧录
- 实测在模块加载后手动 reset ESP 更可靠

这是许多人忽略的嵌入式 Linux 一课：

- reset 并非“只是又一个 GPIO”
- reset 是一种**板级控制策略**

仍可以设计后续的自动化路径，但稳定的 bring-up 优先。

---


<details>
<summary>English original</summary>

**Lecture 2 — Build, load, and board policy on Jetson**

**Course:** [Jetson ESP-Hosted Host Code guide](../Guide.md) · **Phase 2 — Embedded Linux**

**Previous:** [Lecture 01](Lecture-01.md) · **Next:** [Lecture 03 — SPI transport and IRQs](Lecture-03.md)

---

**1. Start with the shell script, not the C files**

Read:

- `esp_hosted_ng/host/jetson_orin_nano_init.sh`

This file tells you the validated board assumptions:

- `RESETPIN=-1`
- `HANDSHAKEPIN=471`
- `DATAREADYPIN=433`
- `SPI_BUS_NUM=0`
- `SPI_CHIP_SELECT=0`
- `SPI_MODE=2`
- `CLOCKSPEED=10`

This is not random configuration. It is the **board policy layer** for the Jetson dev kit.

Embedded Linux takeaway:

- upstream code often assumes one board
- production bring-up usually needs a board-specific wrapper or policy layer

**Related Linux kernel concepts**

This lecture lines up best with:

- [OS Lecture 5 — Kernel Modules, Boot Process & Device Tree](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-05.md)
- [OS Lecture 17 — Linux Device Driver Model & Device Tree](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md)

Lecture 5 explains why loadable modules and board description are separate concerns: the board says what hardware exists, and the module provides behavior for it. That separation is exactly why `jetson_orin_nano_init.sh` can focus on Jetson policy while `esp32_spi.ko` focuses on transport and subsystem integration.

Lecture 17 explains driver binding and the Linux device model, which is the right mental model for the `spidev` unbind step. The script is not “doing something hacky”; it is clearing a generic driver off a real SPI device so the intended driver can bind to the same kernel object.

The important Embedded Linux idea here is that **“loading a module” is not the same as “hardware is now usable.”** A module can load successfully and still fail to bind the right device, fail to claim the right GPIOs, or fail later when it tries to register itself with Wi‑Fi or Bluetooth subsystems.

That is also why this lecture spends so much time on shell script policy. In real systems, a small wrapper script often carries the board truth: which bus is real, which GPIO numbering scheme the kernel expects, whether reset is safe to drive, and whether a generic placeholder driver like `spidev` must be removed first.

The kernel interfaces and concepts to watch here are:

- out-of-tree kernel modules
- `insmod` and `module_param(...)`
- device ownership and driver binding
- device tree exposing a bus before the real driver claims it

The defaults are stated directly in the helper:

```bash
IF_TYPE="spi"
MODULE_NAME="esp32_spi.ko"
RESETPIN=-1
HANDSHAKEPIN=471
DATAREADYPIN=433
SPI_BUS_NUM=0
SPI_CHIP_SELECT=0
SPI_MODE=2
CLOCKSPEED=10
```

That small block captures most of the Jetson-specific policy:

- SPI transport, not SDIO
- bus `0`, chip-select `0`
- legacy global GPIOs `471` and `433`
- reset disabled by default because board behavior mattered more than theoretical convenience

---

**2. Map the Jetson header before you trust the GPIO numbers**

The ESP-Hosted helper uses Linux GPIO numbers like `471` and `433`, but those are **not** the same thing as the Jetson's physical 40-pin header labels.

Use this NVIDIA carrier-board pin reference first:

![Jetson Orin Nano 40-pin header pinout](../../../../Assets/images/jetson-orin-nano-40-pin-header.png)

Source: [NVIDIA Jetson Orin Nano carrier-board pin header diagram](https://developer.download.nvidia.com/embedded/images/jetsonOrinNano/user_guide/images/jonano_cbspec_figure_3-1_white-bg.png#only-light)

For ESP-Hosted over SPI, the header pins you care about most are:

- SPI1 MOSI: pin `19`
- SPI1 MISO: pin `21`
- SPI1 SCK: pin `23`
- SPI1 CS0: pin `24`
- SPI1 CS1: pin `26`
- Ground: pins `6`, `9`, `14`, `20`, `25`, `30`, `34`, `39`

This picture is useful because it keeps two different numbering systems separate:

- **header pin numbers** like `19` and `24`, which are physical connector locations
- **Linux GPIO numbers** like `471` and `433`, which are kernel-visible IDs used by the driver helper

That distinction matters in real bring-up. You wire by **header pin**, but you debug the helper script and module parameters with **Linux GPIO numbers**.

---

**3. Why `resetpin=-1` is a serious engineering choice**

The validated Jetson flow intentionally used:

- `resetpin=-1`

That means the host driver does **not** drive ESP reset by default.

Why that mattered on real hardware:

- Jetson-driven reset could interfere with ESP boot
- it could also interfere with USB flashing from another host
- a manual ESP reset after module load proved more reliable

This is an Embedded Linux lesson many people miss:

- reset is not “just another GPIO”
- reset is a **board-level control policy**

You can still design a later automation path, but stable bring-up comes first.

---

</details>

## 4. 理解模块构建路径

阅读：

- `esp_hosted_ng/host/Makefile`

最重要的细节是：

- `target ?= sdio`
- 但 Jetson bring-up（上电点亮/调通）使用的是 `target=spi`

因此实际的模块变为：

- `esp32_spi.ko`

`Makefile` 组装了：

- 传输相关的源代码：
  - `spi/esp_spi.o`
- 公共源代码：
  - `main.o`
  - `esp_cfg80211.o`
  - `esp_bt.o`
  - `esp_cmd.o`
  - `esp_utils.o`
  - `esp_stats.o`
  - `esp_debugfs.o`
  - `esp_log.o`

这种拆分正是 **可移植的嵌入式 Linux 驱动** 中希望看到的：

- 一个传输相关的模块切片
- 一个基本与传输无关的 Linux 集成层

`Makefile` 非常直接地说明了这一点：

```make
target ?= sdio
MODULE_NAME := esp32_$(target)

ifeq ($(target), spi)
    ccflags-y += -I$(src)/spi -I$(CURDIR)/spi
    EXTRA_CFLAGS += -I$(M)/spi
    module_objects += spi/esp_spi.o
endif

module_objects += esp_bt.o main.o esp_cmd.o esp_utils.o \
                  esp_cfg80211.o esp_stats.o esp_debugfs.o esp_log.o

obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-y := $(module_objects)
```

这说明：

- `target=spi` 负责选择传输胶合部分
- Wi-Fi、蓝牙与生命周期文件是共享的
- 最终的 kernel 目标文件组装为 `esp32_spi.ko`

---

## 5. 脚本为何要解绑 `spidev`

在 Jetson 路径中，辅助脚本可以解绑：

- 把 `spi0.0` 从 `spidev` 解绑

为什么这是必要的：

- 设备树把 SPI 总线暴露给了 Linux
- 但通用的 `spidev` 驱动可能已经占有了该 SPI 设备
- 在该通用绑定被移除之前，真正的 host 模块无法干净地使用它

这是一个 **经典的嵌入式 Linux 模式**：

- 先用通用工具路径证明总线存在
- 再禁用通用占有者，让真正的子系统驱动可以认领它

这就是“总线可见”与“系统已集成”之间的区别。

确切的解绑步骤很短，且极具说明性：

```bash
if [ "$driver_name" = "spidev" ]; then
	echo "Unbinding ${spi_dev} from spidev for this boot..."
	echo "$spi_dev" | sudo tee /sys/bus/spi/drivers/spidev/unbind > /dev/null
	return
fi
```

这是一条很好的嵌入式 Linux 经验：shell 脚本并不是在“做驱动的工作”。它是在清除设备模型冲突，以便真正的子系统驱动能够复用 `spi0.0`。

---

## 6. 模块参数：无需重新编译即可适配特定板卡

阅读：

- `esp_hosted_ng/host/main.c`
- `esp_hosted_ng/host/spi/esp_spi.c`

Jetson 分支新增了以下模块参数：

- `resetpin`
- `clockspeed`
- `spi_bus_num`
- `spi_chip_select`
- `spi_handshake_gpio`
- `spi_dataready_gpio`
- `spi_mode`

为什么这很重要：

- 原始板卡假设不再需要编译进代码
- 板卡映射变为 **加载时决策**
- 调试总线与 GPIO 问题变得快得多

当嵌入式 Linux 代码脱离单板教程阶段之后，正应如此演进。

---

## 7. 已验证的加载命令的真正含义

可用的 Jetson 加载路径在逻辑上等价于：

```bash
sudo insmod ./esp32_spi.ko \
  resetpin=-1 \
  clockspeed=10 \
  spi_bus_num=0 \
  spi_chip_select=0 \
  spi_handshake_gpio=471 \
  spi_dataready_gpio=433 \
  spi_mode=2
```

这说明：

- 该驱动在模块加载时即可感知板卡
- 板卡相关选择与已编译代码分离
- Linux 子系统集成无需直接知道 Jetson 排针引脚编号

相反，驱动使用的是：

- Linux SPI 总线选择
- Linux 片选
- Linux 遗留全局 GPIO 编号

这正是 kernel 所期望的层级。

辅助脚本显式构造最终的 `insmod` 参数：

```bash
insmod_args=(
	resetpin="$RESETPIN"
	clockspeed="$CLOCKSPEED"
	raw_tp_mode="$RAW_TP_MODE"
	spi_bus_num="$SPI_BUS_NUM"
	spi_chip_select="$SPI_CHIP_SELECT"
	spi_handshake_gpio="$HANDSHAKEPIN"
	spi_dataready_gpio="$DATAREADYPIN"
	spi_mode="$SPI_MODE"
)

sudo insmod "$MODULE_NAME" "${insmod_args[@]}"
```

这就是从板卡策略到内核模块的实际交接。

---

## 实验

回答以下问题：

1. `jetson_orin_nano_init.sh` 中哪些设置明显是板卡相关的？
2. 哪些设置是协议相关的，而非板卡相关的？
3. 为什么解绑 `spidev` 是 Linux 集成问题，而不只是 shell 脚本问题？
4. 为什么 `resetpin=-1` 对 bring-up 而言是合理的默认值？

---

**Previous:** [Lecture 01](Lecture-01.md) · **Next:** [Lecture 03 — SPI 传输与 IRQ](Lecture-03.md)


<details>
<summary>English original</summary>

**4. Understand the module build path**

Read:

- `esp_hosted_ng/host/Makefile`

The most important detail is:

- `target ?= sdio`
- but Jetson bring-up uses `target=spi`

So the actual module becomes:

- `esp32_spi.ko`

The `Makefile` assembles:

- transport-specific sources:
  - `spi/esp_spi.o`
- common sources:
  - `main.o`
  - `esp_cfg80211.o`
  - `esp_bt.o`
  - `esp_cmd.o`
  - `esp_utils.o`
  - `esp_stats.o`
  - `esp_debugfs.o`
  - `esp_log.o`

That split is exactly what you want to see in a **portable Embedded Linux driver**:

- one transport-specific module slice
- one mostly transport-agnostic Linux integration layer

The `Makefile` says that very directly:

```make
target ?= sdio
MODULE_NAME := esp32_$(target)

ifeq ($(target), spi)
    ccflags-y += -I$(src)/spi -I$(CURDIR)/spi
    EXTRA_CFLAGS += -I$(M)/spi
    module_objects += spi/esp_spi.o
endif

module_objects += esp_bt.o main.o esp_cmd.o esp_utils.o \
                  esp_cfg80211.o esp_stats.o esp_debugfs.o esp_log.o

obj-m := $(MODULE_NAME).o
$(MODULE_NAME)-y := $(module_objects)
```

That tells you:

- `target=spi` chooses the transport glue
- the Wi-Fi, Bluetooth, and lifecycle files are shared
- the final kernel object is assembled as `esp32_spi.ko`

---

**5. Why the script unbinds `spidev`**

In the Jetson path, the helper script can unbind:

- `spi0.0` from `spidev`

Why this is necessary:

- the device tree exposed the SPI bus to Linux
- but the generic `spidev` driver may already own that SPI device
- the real host module cannot use it cleanly until that generic binding is removed

This is a **classic Embedded Linux pattern**:

- first you prove the bus exists with a generic tool path
- then you disable the generic owner so the real subsystem driver can claim it

That is the difference between “bus visible” and “system integrated.”

The exact unbind step is short and very revealing:

```bash
if [ "$driver_name" = "spidev" ]; then
	echo "Unbinding ${spi_dev} from spidev for this boot..."
	echo "$spi_dev" | sudo tee /sys/bus/spi/drivers/spidev/unbind > /dev/null
	return
fi
```

This is a nice Embedded Linux lesson because the shell script is not “doing driver work.” It is clearing a device-model conflict so the real subsystem driver can reuse `spi0.0`.

---

**6. Module parameters: board-specific without recompiling**

Read:

- `esp_hosted_ng/host/main.c`
- `esp_hosted_ng/host/spi/esp_spi.c`

The Jetson fork adds module parameters for:

- `resetpin`
- `clockspeed`
- `spi_bus_num`
- `spi_chip_select`
- `spi_handshake_gpio`
- `spi_dataready_gpio`
- `spi_mode`

Why this matters:

- the original board assumptions no longer have to be compiled in
- board mapping becomes a **load-time decision**
- debugging bus and GPIO problems becomes much faster

That is exactly how Embedded Linux code should evolve when it leaves a single-board tutorial phase.

---

**7. The real meaning of the validated load command**

The working Jetson load path was logically equivalent to:

```bash
sudo insmod ./esp32_spi.ko \
  resetpin=-1 \
  clockspeed=10 \
  spi_bus_num=0 \
  spi_chip_select=0 \
  spi_handshake_gpio=471 \
  spi_dataready_gpio=433 \
  spi_mode=2
```

What that tells you:

- this driver is board-aware at module load time
- the board-specific choices are separated from the compiled code
- the Linux subsystem integration does not need to know Jetson header pin numbers directly

Instead, the driver consumes:

- Linux SPI bus selection
- Linux chip select
- Linux legacy global GPIO numbers

That is exactly the level the kernel expects.

The helper constructs the final `insmod` arguments explicitly:

```bash
insmod_args=(
	resetpin="$RESETPIN"
	clockspeed="$CLOCKSPEED"
	raw_tp_mode="$RAW_TP_MODE"
	spi_bus_num="$SPI_BUS_NUM"
	spi_chip_select="$SPI_CHIP_SELECT"
	spi_handshake_gpio="$HANDSHAKEPIN"
	spi_dataready_gpio="$DATAREADYPIN"
	spi_mode="$SPI_MODE"
)

sudo insmod "$MODULE_NAME" "${insmod_args[@]}"
```

That is the practical handoff from board policy into the kernel module.

---

**Lab**

Answer these:

1. Which settings in `jetson_orin_nano_init.sh` are clearly board-specific?
2. Which settings are protocol-specific rather than board-specific?
3. Why is `spidev` unbinding a Linux integration problem, not just a shell-script problem?
4. Why is `resetpin=-1` a reasonable default for bring-up?

---

**Previous:** [Lecture 01](Lecture-01.md) · **Next:** [Lecture 03 — SPI transport and IRQs](Lecture-03.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Jetson ESP-Hosted Host Code/Lecture/Lecture-02.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Jetson%20ESP-Hosted%20Host%20Code/Lecture/Lecture-02.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
