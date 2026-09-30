---
title: FSP（Firmware Support Package）与 SPE 固件
description: FSP（Firmware Support Package）与 SPE 固件
---

# FSP（Firmware Support Package）与 SPE 固件

<div class="course-identity fsp" markdown="1">
<div class="course-identity__icon">FSP</div>
<div markdown="1">
<p class="course-identity__eyebrow">方向 B4 · Firmware Support Package</p>
<p class="course-identity__title">理解底层固件、启动配置、电源行为与板级适配。</p>
<p class="course-identity__meta">产物：固件适配笔记 · 度量：启动阶段、电源轨、配置差异</p>
</div>
</div>


**阶段 4 — 方向 B — Nvidia Jetson** · 模块 4 / 7

> **重点：** 定制运行在 Jetson **Sensor Processing Engine (SPE)** 上的固件——即 **always-on (AON)** 域中的 **Cortex-R5**——使用基于 **FreeRTOS** 的 NVIDIA **Firmware Support Package (FSP)**。这是 **低层 I/O**、**唤醒**场景以及不应放在主 Linux **CCPLEX** 上的**时间关键**任务的路径。
>
> **范围：** 下文的**分步 demo 章节**面向 **Jetson Orin Nano (T234)**，使用 **r35.6** SPE 指南中的路径（**p3767** MB1 BCT、**p3768** kernel DTS，即 NVIDIA 所命名的那些）。其他 Jetson 型号使用不同文件——**AGX Xavier** / **AGX Orin** 变体参见同一份 SPE 指南。

**上一节：** [3. L4T 定制](../03-L4T定制/Guide.md) · **下一节：** [5. 应用开发](../05-应用开发/Guide.md) · **配套内容：** [3. L4T 定制](../03-L4T定制/Guide.md)（flash 布局、`Linux_for_Tegra`、BCT/pinmux）

---

## 为什么它与 L4T 相邻

SPE 固件是一个**独立的二进制**（`spe_t194.bin` / `spe_t234.bin`），与板上其余部分一样通过同一套 **Jetson Linux** 工具链烧写。改动 SPE 行为几乎总会触及你在 L4T 工作中已经实践过的**量产事项**：**pinmux**、**GPIO 初始化**、**防火墙（SCR）**、**设备树**（包括用于时钟的 **BPMP** DT），以及**仅分区**烧写。把 SPE 镜像视作与 kernel 和 DTB 并列的**带版本的产物**。

---

## 官方文档（对齐你的 JetPack / L4T 版本线）

以下内容与 **r35.6** 文档集附带的 **Jetson SPE Developer Guide** 保持一致。其他版本请打开 [Jetson documentation](https://docs.nvidia.com/jetson/) 下对应的归档。

| 主题 | 链接 |
|--------|------|
| **SPE 指南（欢迎页、BSP 布局、功能矩阵）** | [Jetson SPE Developer Guide — r35.6](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/index.html) |
| **FSP 架构（OSA / CPL / 驱动）** | [FSP (Firmware Support Package)](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_fsp.html) |
| **构建、产物、烧写** | [Compiling and Flashing](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/rt-compiling.html) |
| **AODMIC demo（DMIC5、唤醒）** | [AODMIC Application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_aodmic_app.html) |
| **处理器间通道** | [IVC](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_ivc.html) |
| **GTE（带时间戳的 GPIO 事件）** | [GTE Application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_gte_app.html) |
| **从 SPE 访问 AON GPIO** | [GPIO Application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_gpio.html) |
| **从 SPE 访问 AON I2C** | [I2C application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_i2c_app.html) |
| **从 SPE 访问 AON SPI** | [SPI application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_spi_app.html) |
| **Timer 驱动 demo** | [Timer application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_timer_app.html) |

---


<details>
<summary>English original</summary>

**FSP (Firmware Support Package) and SPE firmware**

<div class="course-identity fsp" markdown="1">
<div class="course-identity__icon">FSP</div>
<div markdown="1">
<p class="course-identity__eyebrow">Track B4 · Firmware Support Package</p>
<p class="course-identity__title">Understand low-level firmware, boot configuration, power behavior, and board-specific adaptation.</p>
<p class="course-identity__meta">Artifact: firmware adaptation note · Measure: boot stages, power rails, config deltas</p>
</div>
</div>


**Phase 4 — Track B — Nvidia Jetson** · Module 4 of 7

> **Focus:** Customize firmware that runs on the Jetson **Sensor Processing Engine (SPE)**—the **Cortex-R5** in the **always-on (AON)** domain—using NVIDIA’s **Firmware Support Package (FSP)** on **FreeRTOS**. This is the path for **low-level I/O**, **wake** scenarios, and **time-critical** tasks that should not live on the main Linux **CCPLEX**.
>
> **Scope:** The **step-by-step demo sections** below target **Jetson Orin Nano (T234)** using **r35.6** SPE guide paths (**p3767** MB1 BCT, **p3768** kernel DTS where NVIDIA names them). Other Jetson models use different files—see the same SPE guide for **AGX Xavier** / **AGX Orin** variants.

**Previous:** [3. L4T Customization](../03-L4T定制/Guide.md) · **Next:** [5. Application Development](../05-应用开发/Guide.md) · **Companion:** [3. L4T customization](../03-L4T定制/Guide.md) (flash layout, `Linux_for_Tegra`, BCT/pinmux)

---

**Why this sits next to L4T**

SPE firmware is a **separate binary** (`spe_t194.bin` / `spe_t234.bin`) flashed via the same **Jetson Linux** toolkit as the rest of the board. Changing SPE behavior almost always touches **production concerns** you already practice in L4T work: **pinmux**, **GPIO init**, **firewall (SCR)**, **device tree** (including **BPMP** DT for clocks), and **partition-only** flashes. Treat SPE images as **versioned artifacts** alongside kernel and DTB.

---

**Official documentation (pin to your JetPack / L4T line)**

The material below is aligned with the **Jetson SPE Developer Guide** packaged with the **r35.6** documentation set. For other releases, open the matching archive under [Jetson documentation](https://docs.nvidia.com/jetson/).

| Topic | Link |
|--------|------|
| **SPE guide (welcome, BSP layout, feature matrix)** | [Jetson SPE Developer Guide — r35.6](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/index.html) |
| **FSP architecture (OSA / CPL / drivers)** | [FSP (Firmware Support Package)](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_fsp.html) |
| **Build, artifacts, flash** | [Compiling and Flashing](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/rt-compiling.html) |
| **AODMIC demo (DMIC5, wake)** | [AODMIC Application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_aodmic_app.html) |
| **Inter-processor channels** | [IVC](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_ivc.html) |
| **GTE (timestamped GPIO events)** | [GTE Application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_gte_app.html) |
| **AON GPIO from SPE** | [GPIO Application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_gpio.html) |
| **AON I2C from SPE** | [I2C application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_i2c_app.html) |
| **AON SPI from SPE** | [SPI application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_spi_app.html) |
| **Timer driver demo** | [Timer application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_timer_app.html) |

---

</details>

## SPE / BSP layout（心智模型）

来自 SPE BSP 包：

- **`fsp/source`** — 公共 **驱动**、**OSA**（OS 抽象）、**CPL**（CPU 抽象）和 **`soc/<soc>/...`** port/ID 数据。
- **`rt-aux-cpu-demo-fsp`** — Demo 应用（`app/`）、构建系统（`Makefile`、`soc/t19x` / `soc/t23x` **target_specific.mk**）、平台代码和 **FreeRTOS** 集成。
- **`FreeRTOSV10.4.3/FreeRTOS/Source`** — FreeRTOS 源码（该 BSP drop 中随附的版本）。

NVIDIA 的欢迎页面列出了**哪些 demo 在哪些平台上受支持**（例如该矩阵中 **Orin Nano** 上的 **IVC**、**GTE**、**GPIO**、**I2C**、**SPI**、**Timer**；**AODMIC** 在该处**未**为 Orin Nano 列出）。始终对照你的 **SoC** 和 **指南修订版**进行确认。

**Orin Nano 构建：** 在 `soc/t23x/target_specific.mk` 中，为面向 Nano 模块的 demo 设置 **`ENABLE_SPE_FOR_ORIN_NANO := 1`**（如下面的 **GTE**、**GPIO** 和 **SPI** recipe 所示）。对 **`ENABLE_I2C_APP`**、**`ENABLE_SPI_APP`**、**`ENABLE_TIMER_APP`** 等每应用标志，使用同一文件。

---

## FSP 架构（简述）

### OSA（操作系统抽象）

**为什么需要 OSA？**  
OSA 提供了一层，位于固件代码（驱动/应用）与底层 RTOS 之间——NVIDIA 的 FSP 中为 **FreeRTOS v10**。这个**抽象层**意味着大多数驱动和中间件代码是针对 *可移植 API* 编写的，而不是硬编码到 FreeRTOS 调用。如果需要迁移到不同的 RTOS（或更新 FreeRTOS 版本），只需更改 OSA 实现——而不是每个驱动或应用。这提高了**可移植性**，简化了维护，并能更快适应新平台或需求。

实际示例：所有典型 RTOS 原语（例如信号量、互斥量、事件组、队列和任务）都由 OSA API 封装。`fsp/source/include/osa/freertosv10/osa/` 中相关的头文件包括：

| 功能      | OSA API 头文件             |
|--------------------|---------------------------|
| 信号量          | `osa-semaphore.h`         |
| 互斥量              | `osa-mutex.h`             |
| 事件组        | `osa-event-group.h`       |
| 队列              | `osa-queue.h`             |
| 任务 / 调度 | `osa-task.h`              |
| 软件定时器     | `osa-timer.h`             |

总结：**OSA 隔离了 RTOS 细节**，让你在需求或平台变化时，能以最小代价复用并演进嵌入式代码库。

### CPL（CPU / 平台抽象）：为什么需要 CPL？

**为什么需要 CPL？**  
CPL（CPU/平台层）抽象了不同 CPU 或平台所需的底层、硬件特定操作——例如缓存管理、寄存器操作、内存屏障、中断控制和芯片识别。

这种抽象确保固件和驱动代码保持可移植和可维护。无需在代码中到处硬编码 ARM Cortex-R5 寄存器访问或缓存刷新例程，而是依赖标准化的 CPL 函数调用或宏。如果 NVIDIA 或你的项目迁移到不同的处理器核心，只需为新硬件更新 CPL——你的应用、中间件甚至驱动都保持不变。

没有 CPL，代码会散落硬件细节，使得在例如 Cortex-R5 和 Cortex-A 平台之间移植成为耗时且易错的过程。有了 CPL，可以干净地替换或增强处理器和平台原语，而保持栈的其余部分不变。

**CPL 提供的头文件示例（路径如 NVIDIA 的 FSP 中所示）：**

| 关注点                  | 头文件                                                      |
|--------------------------|-------------------------------------------------------------|
| 缓存操作         | `fsp/source/include/cpu/arm/common/cpu/cache.h`             |
| 寄存器访问 (Cortex-R5) | `fsp/source/include/cpu/arm/armv7/cortex-r5/reg-access/reg-access.h` |
| 屏障 (内存/顺序)  | `fsp/source/include/cpu/arm/common/cpu/barriers.h`          |
| VIC (中断)         | `fsp/source/include/cpu/arm/common/cpu/arm-vic.h`           |
| 芯片 ID / SKU            | `fsp/source/include/chipid/chip-id.h`                       |

### 驱动

外设驱动位于 OSA/CPL 之上。NVIDIA 指出用于 GPIO 的头文件如 **`fsp/source/include/gpio/tegra-gpio.h`**。**SoC 特定**部分使用 `fsp/source/soc/<soc>/port/aon` 和 `fsp/source/soc/<soc>/ids/aon` 提供 **实例 ID**、**基地址**和 **IRQ** 数据。

---

## 工具链、构建和烧录

### 工具链

NVIDIA 期望使用外部 **GNU Arm Embedded** 工具链（文档引用 **`gcc-arm-none-eabi-7-2018-q2-update`**）。NVIDIA 不重新分发它；请从 Arm 的存档中为你的主机 OS 下载。在 **Windows** 上，使用 **WSL2**（Ubuntu），采用与 Jetson 烧录相同的流程。


<details>
<summary>English original</summary>

**SPE / BSP layout (mental model)**

From the SPE BSP package:

- **`fsp/source`** — Common **drivers**, **OSA** (OS abstraction), **CPL** (CPU abstraction), and **`soc/<soc>/...`** port/ID data.
- **`rt-aux-cpu-demo-fsp`** — Demo apps (`app/`), build system (`Makefile`, `soc/t19x` / `soc/t23x` **target_specific.mk**), platform code, and **FreeRTOS** integration.
- **`FreeRTOSV10.4.3/FreeRTOS/Source`** — FreeRTOS sources (version as shipped in that BSP drop).

NVIDIA’s welcome page lists **which demos are supported on which platforms** (e.g. **IVC**, **GTE**, **GPIO**, **I2C**, **SPI**, **Timer** on **Orin Nano** in that matrix; **AODMIC** is **not** listed for Orin Nano there). Always confirm against your **SoC** and **guide revision**.

**Orin Nano builds:** In `soc/t23x/target_specific.mk`, set **`ENABLE_SPE_FOR_ORIN_NANO := 1`** for demos that target the Nano module (as in the **GTE**, **GPIO**, and **SPI** recipes below). Use the same file for per-app flags such as **`ENABLE_I2C_APP`**, **`ENABLE_SPI_APP`**, **`ENABLE_TIMER_APP`**.

---

**FSP architecture (short)**

**OSA (Operating System Abstraction)**

**Why OSA?**  
OSA provides a layer that sits between firmware code (drivers/apps) and the underlying RTOS—**FreeRTOS v10** in NVIDIA’s FSP. This **abstraction layer** means most driver and middleware code is written against a *portable API*, not hardwired to FreeRTOS calls. If you need to move to a different RTOS (or update FreeRTOS version), only the OSA implementation needs change—not every driver or app. This improves **portability**, simplifies maintenance, and enables faster adaptation to new platforms or requirements.

Practical example: All the typical RTOS primitives (such as semaphores, mutexes, event groups, queues, and tasks) are wrapped by OSA APIs. The relevant headers in `fsp/source/include/osa/freertosv10/osa/` include:

| Functionality      | OSA API Header             |
|--------------------|---------------------------|
| Semaphore          | `osa-semaphore.h`         |
| Mutex              | `osa-mutex.h`             |
| Event group        | `osa-event-group.h`       |
| Queue              | `osa-queue.h`             |
| Tasks / scheduling | `osa-task.h`              |
| Software timer     | `osa-timer.h`             |

In summary: **OSA isolates RTOS specifics**, letting you reuse and evolve your embedded codebase with minimal pain as requirements or platforms change.

**CPL (CPU / platform abstraction): Why CPL?**

**Why CPL?**  
The CPL (CPU/Platform Layer) abstracts low-level, hardware-specific operations—such as cache management, register manipulation, memory barriers, interrupt control, and chip identification—that are required across different CPUs or platforms.

This abstraction ensures that firmware and driver code remains portable and maintainable. Instead of hardcoding ARM Cortex-R5 register access or cache flush routines throughout your code, you rely on standardized CPL function calls or macros. If NVIDIA or your project moves to a different processor core, only CPL needs to be updated for the new hardware—your application, middleware, and even drivers remain unchanged.

Without CPL, code would be littered with hardware specifics, making porting between, e.g., Cortex-R5 and Cortex-A platforms a time-consuming, error-prone process. With CPL, you can cleanly swap out or enhance processor and platform primitives, keeping the rest of the stack untouched.

**Examples of CPL-provided headers (paths as in NVIDIA’s FSP):**

| Concern                  | Header                                                      |
|--------------------------|-------------------------------------------------------------|
| Cache operations         | `fsp/source/include/cpu/arm/common/cpu/cache.h`             |
| Register access (Cortex-R5) | `fsp/source/include/cpu/arm/armv7/cortex-r5/reg-access/reg-access.h` |
| Barriers (memory/order)  | `fsp/source/include/cpu/arm/common/cpu/barriers.h`          |
| VIC (interrupts)         | `fsp/source/include/cpu/arm/common/cpu/arm-vic.h`           |
| Chip ID / SKU            | `fsp/source/include/chipid/chip-id.h`                       |

**Drivers**

Peripheral drivers sit above OSA/CPL. NVIDIA points to headers such as **`fsp/source/include/gpio/tegra-gpio.h`** for GPIO. **SoC-specific** pieces use `fsp/source/soc/<soc>/port/aon` and `fsp/source/soc/<soc>/ids/aon` for **instance IDs**, **base addresses**, and **IRQ** data.

---

**Toolchain, build, and flash**

**Toolchain**

NVIDIA expects an external **GNU Arm Embedded** toolchain (documentation references **`gcc-arm-none-eabi-7-2018-q2-update`**). NVIDIA does not redistribute it; download from Arm’s archive for your host OS. On **Windows**, use **WSL2** (Ubuntu) for the same flow as Jetson flashing.

</details>

### 环境与构建

```bash
export SPE_FREERTOS_BSP=<root containing rt-aux-cpu-demo-fsp, fsp, FreeRTOS>
export CROSS_COMPILE=<toolchain>/bin/arm-none-eabi-

cd "${SPE_FREERTOS_BSP}/rt-aux-cpu-demo-fsp"
make -j"$(nproc)" bin_t19x    # T194-class
make -j"$(nproc)" bin_t23x    # T234-class
```

其他有用的 target（见[编译与烧写](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/rt-compiling.html)）：

- **`make docs`** — Doxygen 输出位于 `out/docs/index.html`
- **`make` / `make all`** — 所有 SOC 二进制文件 + 文档
- **`make clean`**、**`make clean_t19x`**、**`make clean_t23x`**、**`make clean_docs`**

**产物：** `out/<soc>/spe.bin`

### 安装到 `Linux_for_Tegra` 并烧写

1. **备份** `Linux_for_Tegra/bootloader/` 中的原始文件：
   - **`spe_t194.bin`**（T194）
   - **`spe_t234.bin`**（T234）
2. 复制构建结果：
   - T194 → `Linux_for_Tegra/bootloader/spe_t194.bin`
   - T234 → `Linux_for_Tegra/bootloader/spe_t234.bin`
3. 迭代时**仅**烧写 SPE 分区：

```bash
# T194 (partition name spe-fw)
sudo ./flash.sh -k spe-fw <board-name> mmcblk0p1

# T234 (partition name A_spe-fw)
sudo ./flash.sh -k A_spe-fw <board-name> mmcblk0p1
```

沿用 Jetson Linux Developer Guide 中针对你的 carrier/module 的同一套 **`<board-name>`** 约定。

---

## Orin Nano — IVC echo 通道（CCPLEX ↔ AON）

**IVC** 使用由 mailbox 支撑的 **memory channel**。原厂 SPE 发行版记录了一个 **echo** 通道：Linux 发送字符串，SPE 将其回显。

NVIDIA 的 r35.6 **IVC** 页面把 **设备树** recipe 标注在 **AGX Orin** 下；**Orin Nano** 属于同一条 **T234** 产品线，因此同步源码后使用相同的 **`tegra234-aon.dtsi`** 模式。

1. 运行 **`source_sync.sh`**（按 Jetson Linux 文档），使 kernel DTS 在 `Linux_for_Tegra/sources/` 下可用。
2. 编辑 **`Linux_for_Tegra/sources/hardware/nvidia/soc/t23x/kernel-dts/tegra234-soc/tegra234-aon.dtsi`**。要么添加一个 **`aon_echo`** 节点，要么在它已存在但被禁用时设置 **`status = "okay"`**，并使 mailbox 与指南匹配（AGX Orin 示例为 **`mboxes = <&aon 1>;`**）：

```dts
aon_echo {
    compatible = "nvidia,tegra186-aon-ivc-echo";
    mboxes = <&aon 1>;
    status = "okay";
};
```

3. **编译**设备树，把 **DTB** 安装到 **`Linux_for_Tegra/kernel/dtb/`** 或 **`Linux_for_Tegra/dtb/`**（按你的 Jetson Linux 布局使用其一），并按照 Jetson Linux Developer Guide 的 **Building the kernel / device tree** 流程**重新烧写**。
4. 构建包含 **IVC echo** 任务的 **SPE** 固件，然后复制 **`out/t23x/spe.bin`** → **`${L4T}/bootloader/spe_t234.bin`** 并烧写（仅更新 SPE 时用 **`-k A_spe-fw`**）。

**固件源码（参考）：**

- `rt-aux-cpu-demo-fsp/app/ivc-echo-task.c`
- `rt-aux-cpu-demo-fsp/platform/ivc-channel-ids.c`

**从 Linux 测试**（echo 通道启用后）：

```bash
sudo su -c 'echo tegra > /sys/devices/platform/aon_echo/data_channel'
cat /sys/devices/platform/aon_echo/data_channel
# expect: tegra
```

完整细节：[IVC](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_ivc.html)。

---

## Orin Nano — GTE 应用（`app/gte-app.c`）

**Generic Timestamping Engine (GTE)** 是 NVIDIA Jetson SoC 中的专用硬件模块，用于监测各种系统信号，并记录特定事件发生的精确时间。GTE 的主要特性包括：

- **高精度时间戳：** GTE 利用一个专用的 32-bit 硬件计数器，为事件提供准确、低延迟的时间戳。这对需要确定性的时序与日志的实时应用至关重要。
- **通过 slice 进行事件监测：** GTE 被划分为多个 *slice*；在 Orin Nano 上，AON/SPE CPU 可访问三个 *32-bit* slice。每个 slice 均可配置为监视特定的信号组（例如 GPIO 事件、CAN 信号边沿等）。哪些外部信号连接到哪些 GTE slice/bit 的精确映射记录在 SoC 的 pinmux 电子表格中（查找 “GTE” 列，它通常隐藏在 slice 2 中）。
- **FIFO 事件缓冲：** 当被监测的事件或信号跳变发生时，GTE 记录事件的 ID、时间戳以及发生所在的 slice，并将其写入内存映射的 FIFO 缓冲区。SPE 固件可以读取出这些带时间戳的事件并实时处理。
- **灵活的信号复用：** GTE 可通过设备树和硬件寄存器配置，以连接多种来源：GPIO、CAN 或其他内部/外部信号。
- **软件接口：** 为便于应用开发，NVIDIA 在 `gte-tegra-hw.h` 中提供了硬件抽象和信号映射。

典型的 FSP demo（`app/gte-app.c`）将 GTE 与 GPIO 事件配合使用：通过连接或翻转 GPIO，GTE 捕获该变化、记录时间戳，并将这些数据传递给正在运行的 SPE 固件。


<details>
<summary>English original</summary>

**Environment and build**

```bash
export SPE_FREERTOS_BSP=<root containing rt-aux-cpu-demo-fsp, fsp, FreeRTOS>
export CROSS_COMPILE=<toolchain>/bin/arm-none-eabi-

cd "${SPE_FREERTOS_BSP}/rt-aux-cpu-demo-fsp"
make -j"$(nproc)" bin_t19x    # T194-class
make -j"$(nproc)" bin_t23x    # T234-class
```

Other useful targets (see [Compiling and Flashing](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/rt-compiling.html)):

- **`make docs`** — Doxygen output under `out/docs/index.html`
- **`make` / `make all`** — All SOC binaries + docs
- **`make clean`**, **`make clean_t19x`**, **`make clean_t23x`**, **`make clean_docs`**

**Artifact:** `out/<soc>/spe.bin`

**Install into `Linux_for_Tegra` and flash**

1. **Back up** the stock files in `Linux_for_Tegra/bootloader/`:
   - **`spe_t194.bin`** (T194)
   - **`spe_t234.bin`** (T234)
2. Copy your build:
   - T194 → `Linux_for_Tegra/bootloader/spe_t194.bin`
   - T234 → `Linux_for_Tegra/bootloader/spe_t234.bin`
3. Flash **only** the SPE partition when iterating:

```bash
# T194 (partition name spe-fw)
sudo ./flash.sh -k spe-fw <board-name> mmcblk0p1

# T234 (partition name A_spe-fw)
sudo ./flash.sh -k A_spe-fw <board-name> mmcblk0p1
```

Use the same **`<board-name>`** conventions as in the Jetson Linux Developer Guide for your carrier/module.

---

**Orin Nano — IVC echo channel (CCPLEX ↔ AON)**

**IVC** uses a mailbox-backed **memory channel**. The stock SPE distribution documents an **echo** channel: Linux sends a string, SPE echoes it back.

NVIDIA’s r35.6 **IVC** page labels the **device tree** recipe under **AGX Orin**; **Orin Nano** is the same **T234** line, so you use the same **`tegra234-aon.dtsi`** pattern after syncing sources.

1. Run **`source_sync.sh`** (per Jetson Linux documentation) so kernel DTS is available under `Linux_for_Tegra/sources/`.
2. Edit **`Linux_for_Tegra/sources/hardware/nvidia/soc/t23x/kernel-dts/tegra234-soc/tegra234-aon.dtsi`**. Either add an **`aon_echo`** node or, if it already exists disabled, set **`status = "okay"`** and match mailboxes to the guide (**`mboxes = <&aon 1>;`** for the AGX Orin example):

```dts
aon_echo {
    compatible = "nvidia,tegra186-aon-ivc-echo";
    mboxes = <&aon 1>;
    status = "okay";
};
```

3. **Compile** the device tree, install the **DTB** into **`Linux_for_Tegra/kernel/dtb/`** or **`Linux_for_Tegra/dtb/`** (whichever your Jetson Linux layout uses), and **reflash** using the **Building the kernel / device tree** flow from the Jetson Linux Developer Guide.
4. Build the **SPE** firmware that includes the **IVC echo** task, then copy **`out/t23x/spe.bin`** → **`${L4T}/bootloader/spe_t234.bin`** and flash (**`-k A_spe-fw`** when you are only updating SPE).

**Firmware sources (reference):**

- `rt-aux-cpu-demo-fsp/app/ivc-echo-task.c`
- `rt-aux-cpu-demo-fsp/platform/ivc-channel-ids.c`

**Test from Linux** (after the echo channel is enabled):

```bash
sudo su -c 'echo tegra > /sys/devices/platform/aon_echo/data_channel'
cat /sys/devices/platform/aon_echo/data_channel
# expect: tegra
```

Full detail: [IVC](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_ivc.html).

---

**Orin Nano — GTE application (`app/gte-app.c`)**

The **Generic Timestamping Engine (GTE)** is a specialized hardware module in NVIDIA Jetson SoCs designed to monitor various system signals and record the exact time when specific events occur. Key features of GTE include:

- **High-Precision Timestamping:** GTE leverages a dedicated 32-bit hardware counter to provide accurate, low-latency timestamping of events. This is essential for real-time applications requiring deterministic timing and logging.
- **Event Monitoring via Slices:** The GTE is partitioned into *slices*; on Orin Nano, there are three *32-bit* slices accessible to the AON/SPE CPU. Each slice can be configured to watch specific groups of signals (for instance, GPIO events, CAN signal edges, etc.). The exact mapping of which external signals connect to which GTE slices/bits is captured in the SoC’s pinmux spreadsheet (look for the “GTE” column, often hidden in slice 2).
- **FIFO Event Buffering:** When a monitored event or signal transition occurs, the GTE records the event’s ID, the timestamp, and the slice where it happened and writes this into a memory-mapped FIFO buffer. The SPE firmware can read out these timestamped events and process them in real time.
- **Flexible Signal Multiplexing:** GTE can be configured via the device tree and hardware registers to connect to a variety of sources: GPIOs, CAN, or other internal/external signals.
- **Software Interface:** To facilitate application development, NVIDIA provides hardware abstraction and signal mapping in `gte-tegra-hw.h`.

The typical FSP demo (`app/gte-app.c`) pairs GTE with GPIO events: by connecting or toggling a GPIO, the GTE picks up the change, logs the timestamp, and passes this data to the running SPE firmware.

</details>

### 在 Demo 中 GTE 与 GPIO 如何协同工作

- 某条特定的 GPIO 线连接到 GTE 输入，具体由载板的 pinmux 定义（参见表格——通常 “GTE” 列默认隐藏）。
- 当 GPIO 状态变化（如上升沿或下降沿）时，GTE 硬件将其捕获为一个事件，打上时间戳，并排入该 slice 的 FIFO。
- 运行在 SPE 上的 GTE 应用（`gte-app.c`）读取这些 FIFO 事件，解析出 slice、event ID 和时间戳，并可对该数据执行打印/记录/响应。

### GTE 应用的软件配置（Orin Nano）

1. 在 SPE 固件构建系统中启用 GTE demo 应用。在 **`soc/t23x/target_specific.mk`** 中，设置：
    - `ENABLE_GTE_APP := 1`
    - `ENABLE_SPE_FOR_ORIN_NANO := 1`
   然后重新构建 FSP 固件（`bin_t23x`），并将生成的 `spe.bin` 复制到部署位置：
   - `${L4T}/bootloader/spe_t234.bin`

2. 在设备树中禁用默认的 Linux GTE 驱动，使 Linux 内核不会独占 GTE 硬件模块（改由 SPE 控制）。在以下文件中：
   - `Linux_for_Tegra/sources/hardware/nvidia/platform/t23x/p3768/kernel-dts/cvb/tegra234-p3768-0000-a0.dtsi`
   添加或修改：
   ```dts
   gte@c1e0000 {
       status = "disabled";
   };
   ```

3. 重新构建内核设备树，并将更新后的 DTB 复制到 `Linux_for_Tegra/kernel/dtb/`。接着，调整系统的硬件防火墙，允许 SPE CPU 读写 GTE 寄存器。对于 Orin Nano，修改此条目：
   - `${L4T}/bootloader/tegra234-firewall-config-base.dtsi`
   如何确定 `reg@1359` 就是 GTE GPIO Security Control Register？

   `reg@1359` 节点对应 GTE GPIO Security Control Register 的硬件地址偏移，即 `GTE_GPIO_SCR_TESCR_0`。该信息来自 Orin Nano Technical Reference Manual（TRM），具体来自 GTE（Generic Timestamp Engine）的内存映射与寄存器文档。在防火墙配置设备树（`tegra234-firewall-config-base.dtsi`）中，诸如 `reg@1359` 的条目使用这些偏移来控制 GTE 模块各寄存器的访问权限。

   总结：通过将寄存器地址（0x1359）与 TRM 中的 “GTE_GPIO_SCR_TESCR_0” 寄存器名对应起来，即可建立这一关联。Nvidia 的文档与示例设备树 overlay 也引用了这一映射。
   ```dts
   reg@1359 { /* GTE_GPIO_SCR_TESCR_0 */
       exclusion-info = <3>;
       value = <0x38001232>;
   };
   ```
   此步骤赋予 SPE 独占访问相关 GTE 寄存器的权限。

4. 执行**整机烧录**（而非仅烧录 SPE 分区），使新的设备树与防火墙设置应用到设备上。**首次**修改这些内容时必须这样做；后续仅更新 SPE 时可只烧录固件。

### 预期结果：日志与用法

- 当 GTE 检测到符合条件的输入信号变化，且 FIFO 缓冲超过其配置阈值（`GTE_FIFO_OCCUPANCY`）时，GTE 向 SPE 发出中断。
- SPE 的 GTE 中断服务程序（ISR）运行，读出队列中的条目并对其进行解码。
- 输出日志通常包含显示应用动作（用于 GPIO 配对）的行、记录的事件详情（“Slice Id”、“Event Id”、“Timestamp in nanosec”），以及 ISR 执行日志，例如 `gpio_app_task - Setting GPIO_APP_OUT to 1` 或 `can_gpio_irq_handler` 翻转输出 GPIO。
- 要测试该配置，可以像 GPIO 应用中那样将两个 GPIO 引脚相连：翻转输入应立即生成一个带时间戳的事件，由 SPE 记录到日志。

**延伸阅读与示例：** NVIDIA 的 [GTE Application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_gte_app.html) 页面提供了更深入的架构细节、事件格式，以及如何将信号、pinmux、GTE 配置与 SPE 固件应用配合起来用于高级时间戳用例的完整演示。


---

## Orin Nano — GPIO 应用（`app/gpio-app.c`）

Demo：从 SPE 驱动一个 **AON GPIO**，并在另一个上接收**中断**。**MB1** pinmux 与 **GPIO int map** 必须与 Orin Nano 载板匹配（r35.6 指南中的 **p3767** BCT）。

### 硬件（Orin Nano 开发套件）

在 **J12** 上，将 **Pin 5**（**PDD1**，输出）连到 **Pin 3**（**PDD2**，输入）。


<details>
<summary>English original</summary>

**How GTE and GPIO Work Together in the Demo**

- A specific GPIO line is connected to a GTE input, as defined in your carrier board’s pinmux (refer to the spreadsheet—often the “GTE” column is hidden by default).
- When the GPIO state changes (such as a rising or falling edge), the GTE hardware captures this as an event, tags it with a timestamp, and queues it in the slice’s FIFO.
- The GTE application (`gte-app.c`) running on the SPE reads these FIFO events, parses out the slice, event ID, and timestamp, and can print/log/react to this data.

**Software Setup for GTE Application (Orin Nano)**

1. Enable the GTE demo application in the SPE firmware build system. In **`soc/t23x/target_specific.mk`**, set:
    - `ENABLE_GTE_APP := 1`
    - `ENABLE_SPE_FOR_ORIN_NANO := 1`
   Then rebuild the FSP firmware (`bin_t23x`), and copy the resulting `spe.bin` to your deployment location:
   - `${L4T}/bootloader/spe_t234.bin`

2. Disable the default Linux GTE driver in the device tree so that the Linux kernel does not claim exclusive access to the GTE hardware block (the SPE will control it instead). In the following file:
   - `Linux_for_Tegra/sources/hardware/nvidia/platform/t23x/p3768/kernel-dts/cvb/tegra234-p3768-0000-a0.dtsi`
   add or modify:
   ```dts
   gte@c1e0000 {
       status = "disabled";
   };
   ```

3. Rebuild the kernel device tree and copy the updated DTBs to `Linux_for_Tegra/kernel/dtb/`. Next, adjust the system’s hardware firewall to allow the SPE CPU to read and write GTE registers. For Orin Nano, patch this entry in:
   - `${L4T}/bootloader/tegra234-firewall-config-base.dtsi`
   How do we know that `reg@1359` is the GTE GPIO Security Control Register?

   The `reg@1359` node corresponds to the hardware address offset for the GTE GPIO Security Control Register, known as `GTE_GPIO_SCR_TESCR_0`. This information comes from the Orin Nano Technical Reference Manual (TRM), specifically from the memory map and register documentation for the GTE (Generic Timestamp Engine). In the firewall configuration device tree (`tegra234-firewall-config-base.dtsi`), entries like `reg@1359` use these offsets to control access permissions for the GTE module's registers.

   In summary: The association is made by matching the register address (0x1359) to the "GTE_GPIO_SCR_TESCR_0" register name in the TRM. Nvidia's documentation and sample device tree overlays also reference this mapping.
   ```dts
   reg@1359 { /* GTE_GPIO_SCR_TESCR_0 */
       exclusion-info = <3>;
       value = <0x38001232>;
   };
   ```
   This step gives the SPE permission to access the relevant GTE registers exclusively.

4. Perform a **full device flash** (not just the SPE partition) so that the new device tree and firewall settings are applied to the device. This is required the **first** time you modify these aspects; for later SPE-only updates you can flash just the firmware.

**What to Expect: Logs and Usage**

- When the GTE detects a qualifying input signal change and the FIFO buffer exceeds its configured threshold (`GTE_FIFO_OCCUPANCY`), the GTE issues an interrupt to the SPE.
- The SPE’s GTE interrupt service routine (ISR) runs, reads out queued entries, and decodes them.
- Output logs typically contain lines showing the application’s action (for GPIO pairing), recorded event details (“Slice Id”, “Event Id”, “Timestamp in nanosec”), and ISR execution logs such as `gpio_app_task - Setting GPIO_APP_OUT to 1` or `can_gpio_irq_handler` toggling the output GPIO.
- To test the setup, you can wire two GPIO pins as in the GPIO app: toggling an input should immediately generate a timestamped event logged by the SPE.

**Further reading and examples:** NVIDIA’s [GTE Application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_gte_app.html) page provides deeper architectural details, event format, and a walk-through of how to pair signal, pinmux, GTE configuration, and SPE firmware application together for advanced timestamping use cases.


---

**Orin Nano — GPIO application (`app/gpio-app.c`)**

Demo: drive one **AON GPIO** from SPE and receive an **interrupt** on another. **MB1** pinmux and **GPIO int map** must match the Orin Nano carrier (**p3767** BCT in the r35.6 guide).

**Hardware (Orin Nano dev kit)**

On **J12**, tie **Pin 5** (**PDD1**, output) to **Pin 3** (**PDD2**, input).

</details>

### 软件步骤

1. **`soc/t23x/target_specific.mk`**：**`ENABLE_GPIO_APP := 1`**、**`ENABLE_SPE_FOR_ORIN_NANO := 1`**。重新构建，复制 **`spe.bin`** → **`${L4T}/bootloader/spe_t234.bin`**。
2. **GPIO 中断映射** — **`${L4T}/bootloader/t186ref/BCT/tegra234-mb1-bct-gpioint-p3767-0000.dts`**：在 **`port@DD`** 下，将 **DD2** 路由到中断线 **2**：

```dts
port@DD {
    pin-0-int-line = <4>; // GPIO DD0 to INT0
    pin-1-int-line = <4>; // GPIO DD1 to INT0
    pin-2-int-line = <2>; // GPIO DD2 to INT2
};
```

3. **AON GPIO 归属（MB1 GPIO DTSI）** — **`${L4T}/bootloader/tegra234-mb1-bct-gpio-p3767-dp-a03.dtsi`**：将 **DD2** 添加为输入、**DD1** 添加为 output-low（NVIDIA 的示例）：

```dts
gpio-input = <
    TEGRA234_AON_GPIO(EE, 2)
    TEGRA234_AON_GPIO(EE, 4)
    TEGRA234_AON_GPIO(DD, 2)
>;
gpio-output-low = <
    TEGRA234_AON_GPIO(DD, 1)
    TEGRA234_AON_GPIO(CC, 0)
>;
```

4. **引脚复用** — **`${L4T}/bootloader/t186ref/BCT/tegra234-mb1-bct-pinmux-p3767-dp-a03.dtsi`**：将 **gen8_i2c_scl_pdd1** / **gen8_i2c_sda_pdd2** 从 **I2C8** 改作 **`rsvd1`**，并按 NVIDIA 指定的 pull / tristate / input 使能（使这些引脚在 demo 中表现为 GPIO）。确切属性值见 [GPIO Application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_gpio.html) 页面。
5. **全量烧录**，使 **MB1 BCT** 与 **GPIO** 设置生效。

### 预期串口输出

```text
gpio_app_task - Setting GPIO_APP_OUT to 1 - IRQ should trigger
can_gpio_irq_handler - gpio irq triggered - setting GPIO_APP_OUT to 0
```

完整细节：[GPIO Application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_gpio.html)。

---

## Orin Nano — I2C 应用（`app/i2c-app.c`）

NVIDIA 将 **Jetson AGX Orin** 与 **Orin Nano** 归入同一个 **软件** recipe：把 **AON I2C8** 的 **Linux** 所有权交还给 **SPE**，打开 **防火墙**，然后运行 demo 固件。

AON 有多个 **I2C** 实例（**2**、**8**、**10**）；哪些可用取决于平台。本 demo 使用 **I2C bus 8**，通过该总线读取 **BMI160**（6 轴）的 **WHO_AM_I** 风格 ID。

### 硬件

将 **BMI160** 模块（或等效器件）接到 **40-pin header J30**（NVIDIA 的映射）：

| J30 引脚 | 信号 |
|---------|--------|
| 1 | 3.3V |
| 3 | SDA |
| 5 | SCL |
| 34 | SAO（按模块接；在 demo 中设置 **7-bit address 0x68**） |
| 39 | GND |

参考板（NVIDIA 指南中引用的示例）：[BMI160 breakout](https://hackspark.fr/en/electronics/1341-6dof-bosch-6-axis-acceleration-gyro-gravity-sensor-gy-bmi160.html)。

### 软件步骤

1. 运行 **`source_sync.sh`**，使 kernel DTS 位于 **`Linux_for_Tegra/sources/`** 下。
2. 在 **`Linux_for_Tegra/sources/hardware/nvidia/soc/t23x/kernel-dts/tegra234-soc/tegra234-soc-cvm.dtsi`** 中禁用 **I2C8** 控制器，使 kernel 不去占用它：

```dts
i2c@c250000 {
    status = "disabled";
};
```

3. **编译** DTB，按你的 Jetson Linux layout 复制到 **`Linux_for_Tegra/kernel/dtb/`** 或 **`Linux_for_Tegra/dtb/`**。
4. 在 **`${L4T}/bootloader/tegra234-firewall-config-base.dtsi`** 中，允许 SPE 访问 **I2C8** 的时钟/复位（**`CLK_RST_CONTROLLER_AON_SCR_I2C8_0`**）：

```dts
reg@2130 { /* CLK_RST_CONTROLLER_AON_SCR_I2C8_0 */
    exclusion-info = <3>;
    value = <0x30001610>;
};
```

5. **`soc/t23x/target_specific.mk`**：**`ENABLE_I2C_APP := 1`**（若构建 Nano SPE 镜像，还有 **`ENABLE_SPE_FOR_ORIN_NANO := 1`**）。重新构建 **`bin_t23x`**，复制 **`spe.bin`** → **`${L4T}/bootloader/spe_t234.bin`**。
6. **全量烧录**，使 **kernel DTB + 防火墙 + SPE** 保持一致。

### 成功输出

```text
I2C test successful
```

完整细节：[I2C application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_i2c_app.html)。

---

## Orin Nano — SPI 应用（`app/spi-app.c`）

**SPI2** 位于 **AON** 域。该 demo 是一个 **loopback**：必须短接 **MISO** 与 **MOSI**；固件发送固定字节并检查回读。

> **BCT 冲突：** SPI demo 的 **MB1 GPIO** 改动会从 **`tegra234-mb1-bct-gpio-p3767-dp-a03.dtsi`** 的输出列表中**移除**若干 **`TEGRA234_AON_GPIO(CC, …)`** 条目。如果你合并了依赖这些行的 **GPIO** 或 **GTE** recipe，请在烧录前整合出一个一致的 BCT。

### 硬件（Orin Nano dev kit）

在 **J2** 上短接 **MISO** ↔ **MOSI**，然后连接：

| J2 引脚 | SPI2 信号 |
|--------|-------------|
| 126 | CLK |
| 127 | MISO |
| 128 | MOSI |
| 130 | CS0 |


<details>
<summary>English original</summary>

**Software steps**

1. **`soc/t23x/target_specific.mk`**: **`ENABLE_GPIO_APP := 1`**, **`ENABLE_SPE_FOR_ORIN_NANO := 1`**. Rebuild, copy **`spe.bin`** → **`${L4T}/bootloader/spe_t234.bin`**.
2. **GPIO interrupt map** — **`${L4T}/bootloader/t186ref/BCT/tegra234-mb1-bct-gpioint-p3767-0000.dts`**: under **`port@DD`**, route **DD2** to interrupt line **2**:

```dts
port@DD {
    pin-0-int-line = <4>; // GPIO DD0 to INT0
    pin-1-int-line = <4>; // GPIO DD1 to INT0
    pin-2-int-line = <2>; // GPIO DD2 to INT2
};
```

3. **AON GPIO ownership (MB1 GPIO DTSI)** — **`${L4T}/bootloader/tegra234-mb1-bct-gpio-p3767-dp-a03.dtsi`**: add **DD2** as input and **DD1** as output-low (example from NVIDIA):

```dts
gpio-input = <
    TEGRA234_AON_GPIO(EE, 2)
    TEGRA234_AON_GPIO(EE, 4)
    TEGRA234_AON_GPIO(DD, 2)
>;
gpio-output-low = <
    TEGRA234_AON_GPIO(DD, 1)
    TEGRA234_AON_GPIO(CC, 0)
>;
```

4. **Pinmux** — **`${L4T}/bootloader/t186ref/BCT/tegra234-mb1-bct-pinmux-p3767-dp-a03.dtsi`**: repurpose **gen8_i2c_scl_pdd1** / **gen8_i2c_sda_pdd2** from **I2C8** to **`rsvd1`** with the pull / tristate / input enables NVIDIA specifies (so the pins behave as GPIO for the demo). See the [GPIO Application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_gpio.html) page for the exact property values.
5. **Full flash** so **MB1 BCT** and **GPIO** settings take effect.

**Expected serial output**

```text
gpio_app_task - Setting GPIO_APP_OUT to 1 - IRQ should trigger
can_gpio_irq_handler - gpio irq triggered - setting GPIO_APP_OUT to 0
```

Full detail: [GPIO Application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_gpio.html).

---

**Orin Nano — I2C application (`app/i2c-app.c`)**

NVIDIA groups **Jetson AGX Orin** and **Orin Nano** under one **software** recipe: give **Linux** ownership of **AON I2C8** back to **SPE**, open the **firewall**, then run the demo firmware.

AON has multiple **I2C** instances (**2**, **8**, **10**); which ones are available depends on the platform. This demo uses **I2C bus 8** and reads a **BMI160** (6-axis) **WHO_AM_I**-style ID over the bus.

**Hardware**

Wire a **BMI160** module (or equivalent) to **40-pin header J30** (NVIDIA’s map):

| J30 pin | Signal |
|---------|--------|
| 1 | 3.3V |
| 3 | SDA |
| 5 | SCL |
| 34 | SAO (tie per module; sets **7-bit address 0x68** in the demo) |
| 39 | GND |

Reference board (example cited in NVIDIA’s guide): [BMI160 breakout](https://hackspark.fr/en/electronics/1341-6dof-bosch-6-axis-acceleration-gyro-gravity-sensor-gy-bmi160.html).

**Software steps**

1. Run **`source_sync.sh`** so kernel DTS is under **`Linux_for_Tegra/sources/`**.
2. In **`Linux_for_Tegra/sources/hardware/nvidia/soc/t23x/kernel-dts/tegra234-soc/tegra234-soc-cvm.dtsi`**, disable the **I2C8** controller so the kernel does not claim it:

```dts
i2c@c250000 {
    status = "disabled";
};
```

3. **Compile** DTBs, copy into **`Linux_for_Tegra/kernel/dtb/`** or **`Linux_for_Tegra/dtb/`**, per your Jetson Linux layout.
4. In **`${L4T}/bootloader/tegra234-firewall-config-base.dtsi`**, allow SPE to access **I2C8** clocks/resets (**`CLK_RST_CONTROLLER_AON_SCR_I2C8_0`**):

```dts
reg@2130 { /* CLK_RST_CONTROLLER_AON_SCR_I2C8_0 */
    exclusion-info = <3>;
    value = <0x30001610>;
};
```

5. **`soc/t23x/target_specific.mk`**: **`ENABLE_I2C_APP := 1`** (and **`ENABLE_SPE_FOR_ORIN_NANO := 1`** if you build the Nano SPE image). Rebuild **`bin_t23x`**, copy **`spe.bin`** → **`${L4T}/bootloader/spe_t234.bin`**.
6. **Full flash** so **kernel DTB + firewall + SPE** stay consistent.

**Success output**

```text
I2C test successful
```

Full detail: [I2C application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_i2c_app.html).

---

**Orin Nano — SPI application (`app/spi-app.c`)**

**SPI2** lives in the **AON** domain. The demo is a **loopback**: **MISO** and **MOSI** must be shorted; firmware sends fixed bytes and checks the readback.

> **BCT conflict:** The SPI demo’s **MB1 GPIO** edits **remove** several **`TEGRA234_AON_GPIO(CC, …)`** entries from output lists in **`tegra234-mb1-bct-gpio-p3767-dp-a03.dtsi`**. If you merged **GPIO** or **GTE** recipes that rely on those lines, reconcile one coherent BCT before flashing.

**Hardware (Orin Nano dev kit)**

On **J2**, short **MISO** ↔ **MOSI**, then connect:

| J2 pin | SPI2 signal |
|--------|-------------|
| 126 | CLK |
| 127 | MISO |
| 128 | MOSI |
| 130 | CS0 |

</details>

### 软件步骤

1. **引脚复用** — **`${L4T}/bootloader/t186ref/BCT/tegra234-mb1-bct-pinmux-p3767-dp-a03.dtsi`**：将 **`spi2_sck_pcc0`**、**`spi2_miso_pcc1`**、**`spi2_mosi_pcc2`**、**`spi2_cs0_pcc3`** 设为 **`nvidia,function = "spi2"`**，并按 [SPI 应用 — Orin Nano](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_spi_app.html) 配置 pull/tristate/input 使能（例如 **MISO** 为采样保持 **tristate/input enabled**）。
2. **MB1 GPIO** — **`${L4T}/bootloader/tegra234-mb1-bct-gpio-p3767-dp-a03.dtsi`**：按 NVIDIA 的 diff，从 **`gpio-output-low`** / **`gpio-output-high`** 中删除 **CC0–CC3**，以免 **SPI2** 焊球被当作 GPIO 驱动。
3. **防火墙** — **`${L4T}/bootloader/tegra234-mb2-bct-scr-p3767-0000.dts`**：为 **`CLK_RST_CONTROLLER_AON_SCR_SPI2_0`** 添加 **`reg@2135`**：

```dts
reg@2135 { /* CLK_RST_CONTROLLER_AON_SCR_SPI2_0 */
    exclusion-info = <3>;
    value = <0x30001410>;
};
```

4. **`soc/t23x/target_specific.mk`**：**`ENABLE_SPI_APP := 1`**、**`ENABLE_SPE_FOR_ORIN_NANO := 1`**。重新构建，安装 **`spe_t234.bin`**，**全量刷写**。

### 成功输出

```text
SPI test successful
```

完整细节：[SPI 应用](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_spi_app.html)。

---

## Orin Nano — 定时器应用（`app/timer-app.c`）

使用 **timer2** 的 **periodic** 模式（NVIDIA 的示例：**5 s** 周期，**`TIMER2_PTV`**）。该 demo 在每次 IRQ 时打印，并在 **`STOP_TIMER`** 次计数后停止。

1. **`soc/t23x/target_specific.mk`**：**`ENABLE_TIMER_APP := 1`**（除非有意组合多个 demo，否则将其他 **`ENABLE_*_APP`** 标志设为 **0**）。对于 **Orin Nano** 模块镜像，如果你的 BSP 在 Nano 上为 **`bin_t23x`** 需要它，则保留 **`ENABLE_SPE_FOR_ORIN_NANO := 1`**。
2. 重新构建 **`bin_t23x`**，复制 **`spe.bin`** → **`spe_t234.bin`**，烧录（如果未修改 kernel 或 BCT，**`-k A_spe-fw`** 就足够）。

### 预期串口输出

```text
Timer2 irq triggered
```

（每个周期重复，直到达到停止计数。）

完整细节：[定时器应用](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_timer_app.html)。

---

## 示例：AODMIC 应用（r35.6 矩阵中面向 AGX）

SPE 欢迎页面中的 r35.6 **支持特性**表为 **AGX Xavier** 和 **AGX Orin** 列出了 **AODMIC**，但没有为 **Orin Nano** 列出。如果你使用这些载板，或将 **DMIC5** 暴露给 AON 的 **定制** T234 板，以下内容仍然有用。

**AODMIC** demo 展示了从 SPE 捕获 **DMIC5**、**GPCDMA**、可选的基于音量阈值的 **system wake**，以及与 **suspend** 和 **BPMP** 时钟的交互。NVIDIA 文档说明：

- **采样率** — `TEGRA_AODMIC_RATE_8KHZ`、`_16KHZ`（`app/aodmic-app.c` 中的默认值）、`_44KHZ`、`_48KHZ`。
- **通道数** — `TEGRA_AODMIC_CHANNEL_STEREO`（默认）、`MONO_LEFT`、`MONO_RIGHT`。
- **`num_periods`** — 通常为 **2**（双缓冲）；最大值在 `fsp/source/include/aodmic/tegra-aodmic.h` 中定义为 `AODMIC_MAX_NUM_PERIODS`（如需可扩展）。
- **过采样** — AODMIC 使用 **64×** 过采样；有效 PDM 时钟为 **64 × sample_rate**——保持在麦克风允许的位时钟范围内。

**硬件：** 平台 **AODMIC** 引脚上的 PDM 麦克风（NVIDIA 给出了 **AGX Xavier** 和 **AGX Orin** 的 **40-pin** 映射——DAT/CLK 引脚和电源/GND）。

**软件集成**（高层次——具体修改随版本和载板而异）：

- **引脚复用** — Xavier：MB1 CFG 修改（例如 `tegra19x-mb1-pinmux-...cfg`）。Orin：MB1 **pinmux DTSI**（例如 `tegra234-mb1-bct-pinmux-...dtsi`），用于将 **CAN1**/GPIO 焊球布线到 **dmic5**。
- **GPIO** — 在引脚变为 DMIC 的位置，移除 MB1 GPIO DTSI 中冲突的 **AON GPIO** 声明。
- **防火墙** — Orin 示例：**SCR** override DTS 允许 SPE 访问 **AODMIC clock**（`CLK_RST_CONTROLLER_AON_SCR_DMIC5_0`）。
- **启用应用** — 在 `soc/t19x/target_specific.mk` 或 `soc/t23x/target_specific.mk` 中设置 **`ENABLE_AODMIC_APP := 1`**，重新构建，将 `spe.bin` 复制到正确的 **`spe_t194.bin` / `spe_t234.bin`**，然后烧录（MB1/SCR 变更时全量刷写；这些稳定后 **仅 SPE**）。

**Suspend 注意事项：** resume 之后，**BPMP** 可能会门控 **dmic5** 时钟并破坏捕获。指南中的缓解措施：runtime 强制开启 `debugfs` 时钟，或修补 **BPMP DTB** `dmic5` 的 **lateinit** 时钟元组（第三个参数 = **sample_rate × 64**），然后烧录 **`bpmp-fw-dtb`** 或完整镜像。

**唤醒路径：** 对于 **voice wake** demo，**wake83** 必须同时出现在 BPMP UART 日志的 **wake mask** 和 **Tier2** 布线中（NVIDIA 给出了示例 mask 片段）。

---


<details>
<summary>English original</summary>

**Software steps**

1. **Pinmux** — **`${L4T}/bootloader/t186ref/BCT/tegra234-mb1-bct-pinmux-p3767-dp-a03.dtsi`**: set **`spi2_sck_pcc0`**, **`spi2_miso_pcc1`**, **`spi2_mosi_pcc2`**, **`spi2_cs0_pcc3`** to **`nvidia,function = "spi2"`** with pull/tristate/input enables per [SPI application — Orin Nano](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_spi_app.html) (e.g. **MISO** keeps **tristate/input enabled** for sampling).
2. **MB1 GPIO** — **`${L4T}/bootloader/tegra234-mb1-bct-gpio-p3767-dp-a03.dtsi`**: drop **CC0–CC3** from **`gpio-output-low`** / **`gpio-output-high`** as in NVIDIA’s diff so **SPI2** balls are not driven as GPIO.
3. **Firewall** — **`${L4T}/bootloader/tegra234-mb2-bct-scr-p3767-0000.dts`**: add **`reg@2135`** for **`CLK_RST_CONTROLLER_AON_SCR_SPI2_0`**:

```dts
reg@2135 { /* CLK_RST_CONTROLLER_AON_SCR_SPI2_0 */
    exclusion-info = <3>;
    value = <0x30001410>;
};
```

4. **`soc/t23x/target_specific.mk`**: **`ENABLE_SPI_APP := 1`**, **`ENABLE_SPE_FOR_ORIN_NANO := 1`**. Rebuild, install **`spe_t234.bin`**, **full flash**.

**Success output**

```text
SPI test successful
```

Full detail: [SPI application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_spi_app.html).

---

**Orin Nano — Timer application (`app/timer-app.c`)**

Uses **timer2** in **periodic** mode (NVIDIA’s example: **5 s** period, **`TIMER2_PTV`**). The demo prints on each IRQ and stops after **`STOP_TIMER`** counts.

1. **`soc/t23x/target_specific.mk`**: **`ENABLE_TIMER_APP := 1`** (set other **`ENABLE_*_APP`** flags to **0** unless you intentionally combine demos). For **Orin Nano** module images, keep **`ENABLE_SPE_FOR_ORIN_NANO := 1`** if your BSP requires it for **`bin_t23x`** on Nano.
2. Rebuild **`bin_t23x`**, copy **`spe.bin`** → **`spe_t234.bin`**, flash (**`-k A_spe-fw`** is enough if you did not change kernel or BCT).

**Expected serial output**

```text
Timer2 irq triggered
```

(repeats each period until the stop count is reached.)

Full detail: [Timer application](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/md__home_jenkins_workspace_Utilities_rt_aux_cpu_demo_fsp_docs_work_rt_aux_cpu_demo_fsp_doc_timer_app.html).

---

**Example: AODMIC application (AGX-oriented in r35.6 matrix)**

The r35.6 **supported features** table in the SPE welcome page lists **AODMIC** for **AGX Xavier** and **AGX Orin**, not **Orin Nano**. The following is still useful if you work on those carriers or a **custom** T234 board that exposes **DMIC5** to AON.

The **AODMIC** demo shows **DMIC5** capture from SPE, **GPCDMA**, optional **system wake** from volume threshold, and the interaction with **suspend** and **BPMP** clocks. NVIDIA documents:

- **Sample rates** — `TEGRA_AODMIC_RATE_8KHZ`, `_16KHZ` (default in `app/aodmic-app.c`), `_44KHZ`, `_48KHZ`.
- **Channels** — `TEGRA_AODMIC_CHANNEL_STEREO` (default), `MONO_LEFT`, `MONO_RIGHT`.
- **`num_periods`** — typically **2** (double-buffered); max defined in `fsp/source/include/aodmic/tegra-aodmic.h` as `AODMIC_MAX_NUM_PERIODS` (extend if needed).
- **Oversampling** — AODMIC uses **64×** oversampling; effective PDM clock is **64 × sample_rate**—stay within your microphone’s allowed bit clock range.

**Hardware:** PDM microphone on the platform’s **AODMIC** pins (NVIDIA gives **40-pin** mappings for **AGX Xavier** and **AGX Orin**—DAT/CLK pins and power/GND).

**Software integration** (high level—exact edits change by release and carrier):

- **Pinmux** — Xavier: MB1 CFG edits (e.g. `tegra19x-mb1-pinmux-...cfg`). Orin: MB1 **pinmux DTSI** (e.g. `tegra234-mb1-bct-pinmux-...dtsi`) to route **CAN1**/GPIO balls to **dmic5**.
- **GPIO** — Remove conflicting **AON GPIO** claims in the MB1 GPIO DTSI where pins become DMIC.
- **Firewall** — Orin example: **SCR** override DTS allows SPE to touch **AODMIC clock** (`CLK_RST_CONTROLLER_AON_SCR_DMIC5_0`).
- **Enable the app** — In `soc/t19x/target_specific.mk` or `soc/t23x/target_specific.mk`, set **`ENABLE_AODMIC_APP := 1`**, rebuild, copy `spe.bin` to the correct **`spe_t194.bin` / `spe_t234.bin`**, then flash (full flash when MB1/SCR change; **SPE-only** once those are stable).

**Suspend caveat:** After resume, **BPMP** may gate **dmic5** clock and break capture. Mitigations in the guide: runtime `debugfs` clock force-on, or patch **BPMP DTB** `dmic5` **lateinit** clock tuple (third argument = **sample_rate × 64**), then flash **`bpmp-fw-dtb`** or full image.

**Wake path:** For **voice wake** demos, **wake83** must appear in BPMP UART logs in both **wake mask** and **Tier2** routing (NVIDIA gives example mask snippets).

---

</details>

## 实践检查清单

- [ ] 将 **SPE BSP** 与 **L4T** 的主线版本匹配到出货所用的 **JetPack**（避免混用未记录的组合）。
- [ ] 把**原始** `spe_t194.bin` / `spe_t234.bin` 以及每个**自定义**构建存入 **Git** 或产物存储，并带 **board + L4T** 元数据。
- [ ] 修改 **pinmux / SCR / BPMP DT** 时，规划一次**全量刷写**，之后固件迭代用 **`-k spe-fw`** / **`-k A_spe-fw`**。
- [ ] 在生产环境依赖 demo 之前，先阅读 **SoC TRM** 中关于 **SPE/AON** 外设实例与**防火墙**规则的部分。

---

*主要参考：[Jetson Sensor Processing Engine (SPE) Developer Guide — r35.6](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/index.html) (NVIDIA)。请将命令与文件名对齐到与你的 Jetson Linux 版本相符的归档。*


<details>
<summary>English original</summary>

**Practice checklist**

- [ ] Match **SPE BSP** and **L4T** major lines to your shipping **JetPack** (avoid mixing undocumented combinations).
- [ ] Store **original** `spe_t194.bin` / `spe_t234.bin` and every **custom** build in **Git** or artifact storage with **board + L4T** metadata.
- [ ] When changing **pinmux / SCR / BPMP DT**, plan a **full flash** once, then use **`-k spe-fw`** / **`-k A_spe-fw`** for firmware iteration.
- [ ] Read the **SoC TRM** for **SPE/AON** peripheral instances and **firewall** rules before relying on demos in production.

---

*Primary reference: [Jetson Sensor Processing Engine (SPE) Developer Guide — r35.6](https://docs.nvidia.com/jetson/archives/r35.6.0/spe/index.html) (NVIDIA). Align commands and file names with the archive that matches your Jetson Linux release.*

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/4. FSP (Firmware Support Package) Customization/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/4.%20FSP%20%28Firmware%20Support%20Package%29%20Customization/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
