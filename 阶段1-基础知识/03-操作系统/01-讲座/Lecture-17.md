---
title: 第 17 讲：Linux 设备驱动模型与设备树
description: 第 17 讲：Linux 设备驱动模型与设备树
---

# 第 17 讲：Linux 设备驱动模型与设备树

## 概述

连接到 Linux 系统的每一块硬件——无论是 USB 摄像头、PCIe GPU，还是 SoC 上的定制 AI 加速器——都需要一个 kernel 驱动来管理。挑战在于硬件极其多样：不同的总线、不同的配置机制和不同的资源布局。Linux **驱动模型**提供统一框架，使一次编写的驱动无论硬件如何被发现都能工作。心智模型是一个配对服务：设备声明自己的身份，驱动声明自己支持什么，kernel 总线核心将它们配对。对于嵌入式 SoC 硬件（Jetson、Zynq、定制 FPGA 板卡），**设备树**是在启动时向 kernel 描述硬件身份的机制。对于 AI 硬件工程师，理解驱动模型和设备树对于启动定制传感器、配置摄像头流水线以及为 AI 加速器外设编写驱动至关重要。

---

## Linux 驱动模型：核心抽象

Linux 设备模型为所有总线提供 **统一框架**。在 `include/linux/device.h` 中定义的三个主要结构表示硬件和软件实体：

```
Linux Driver Model: Match → Probe → Manage

Bus Core (e.g., platform_bus)
    │
    ├── Device Registry (linked list of registered devices)
    │       device_A: compatible="vendor,mydevice-v2"
    │       device_B: compatible="arm,pl011"
    │       device_C: VID=0x8086, PID=0x1592
    │
    ├── Driver Registry (linked list of registered drivers)
    │       driver_X: of_match_table="vendor,mydevice-v2"
    │       driver_Y: of_match_table="arm,pl011"
    │
    └── Match Loop
            for each (device, driver) pair:
                if bus_match(device, driver):
                    driver->probe(device)   ← sets up resources
                    (later) driver->remove(device) ← releases resources
```

| 对象 | 结构 | 作用 |
|--------|-----------|------|
| 总线 | `struct bus_type` | 枚举与匹配逻辑（PCIe、USB、I2C、SPI、platform） |
| 设备 | `struct device` | 物理或虚拟硬件的一个实例 |
| 驱动 | `struct device_driver` | 管理特定设备类型的代码 |

总线核心将每个驱动的 `id_table`（PCI/USB）或 `of_match_table`（设备树 `compatible`）与每个已注册设备进行比较。**匹配时**，总线调用 `driver->probe(device)`。移除或驱动解绑时，调用 `driver->remove(device)`。

> **关键洞察：** probe/remove 生命周期意味着驱动永远不会硬编码其设备的资源地址。总线框架将 `platform_device` 传递给 `probe()`，驱动从该对象查询其资源（MMIO、IRQ、时钟）。这使得同一个驱动二进制文件可用于多个硬件变体，而这些变体仅在设备树描述上不同。

---


<details>
<summary>English original</summary>

**Lecture 17: Linux Device Driver Model & Device Tree**

**Overview**

Every piece of hardware connected to a Linux system — whether a USB camera, a PCIe GPU, or a custom AI accelerator on an SoC — needs a kernel driver to manage it. The challenge is that hardware is enormously diverse: different buses, different configuration mechanisms, and different resource layouts. The Linux **driver model** provides a unified framework so that a driver written once works regardless of how the hardware was discovered. The mental model is a matchmaking service: devices announce their identity, drivers announce what they support, and the kernel bus core pairs them. For embedded SoC hardware (Jetson, Zynq, custom FPGA boards), the **Device Tree** is the mechanism by which hardware identity is described to the kernel at boot. For an AI hardware engineer, understanding the driver model and Device Tree is essential for bringing up custom sensors, configuring camera pipelines, and writing drivers for AI accelerator peripherals.

---

**Linux Driver Model: Core Abstractions**

The Linux device model provides a **unified framework** across all buses. Three primary structures, defined in `include/linux/device.h`, represent the hardware and software entities:

```
Linux Driver Model: Match → Probe → Manage

Bus Core (e.g., platform_bus)
    │
    ├── Device Registry (linked list of registered devices)
    │       device_A: compatible="vendor,mydevice-v2"
    │       device_B: compatible="arm,pl011"
    │       device_C: VID=0x8086, PID=0x1592
    │
    ├── Driver Registry (linked list of registered drivers)
    │       driver_X: of_match_table="vendor,mydevice-v2"
    │       driver_Y: of_match_table="arm,pl011"
    │
    └── Match Loop
            for each (device, driver) pair:
                if bus_match(device, driver):
                    driver->probe(device)   ← sets up resources
                    (later) driver->remove(device) ← releases resources
```

| Object | Structure | Role |
|--------|-----------|------|
| Bus | `struct bus_type` | Enumeration and matching logic (PCIe, USB, I2C, SPI, platform) |
| Device | `struct device` | One instance of physical or virtual hardware |
| Driver | `struct device_driver` | Code that manages a specific device type |

The bus core compares each driver's `id_table` (PCI/USB) or `of_match_table` (Device Tree `compatible`) against each registered device. **On a match**, the bus calls `driver->probe(device)`. On removal or driver unbind, it calls `driver->remove(device)`.

> **Key Insight:** The probe/remove lifecycle means a driver never hardcodes its device's resource addresses. The bus framework passes the `platform_device` to `probe()`, and the driver queries its resources (MMIO, IRQ, clocks) from that object. This makes the same driver binary usable across multiple hardware variants that differ only in their Device Tree descriptions.

---

</details>

## 平台设备

PCIe 与 USB 设备是**自描述**的：它们携带 vendor ID、device ID 和 capability 寄存器，总线可自动读取。SoC 外设**没有这种机制**——无法在地址 0xFE200000 处动态发现一个 UART 控制器。

SoC 外设（UART、I2C 控制器、摄像头 CSI、AI 加速器、FPGA AXI slave）不是自描述的；总线无法枚举它们。它们借助设备树或 ACPI 中的描述注册为**平台设备**。

```
Platform Driver Registration

Device Tree (.dts)          Kernel Driver (C code)
┌──────────────────┐        ┌─────────────────────────────┐
│ mydev@40000000   │        │ static const struct         │
│ compatible =     │        │   of_device_id mydev_ids[] =│
│  "vendor,mydev-v2"│─match─│ { .compatible =             │
│ reg = <0x40000000 │       │   "vendor,mydev-v2" },      │
│       0x10000>   │        │ { .compatible =             │
│ interrupts = ... │        │   "vendor,mydev-v1" }, {}   │
│ status = "okay"  │        │ };                          │
└──────────────────┘        │                             │
                            │ .probe  = mydev_probe,      │
                            │ .remove = mydev_remove,     │
                            └─────────────────────────────┘
```

```c
static const struct of_device_id mydev_of_match[] = {
    { .compatible = "vendor,mydevice-v2" },
    { .compatible = "vendor,mydevice-v1" },   /* older silicon */
    { }   /* sentinel — marks end of the table */
};
MODULE_DEVICE_TABLE(of, mydev_of_match);

static struct platform_driver mydev_driver = {
    .probe  = mydev_probe,
    .remove = mydev_remove,
    .driver = {
        .name           = "mydevice",
        .of_match_table = mydev_of_match,
    },
};
module_platform_driver(mydev_driver);
```

`module_platform_driver()` 展开为调用 `platform_driver_register()` 和 `platform_driver_unregister()` 的 `module_init` / `module_exit` wrapper。

`MODULE_DEVICE_TABLE(of, mydev_of_match)` 把匹配表嵌入模块二进制中。当发现匹配的设备树节点时，`udevd` 利用它**自动加载模块**——无需手动 `modprobe`。

---

## 设备树（DTS）

**设备树**是面向嵌入式 SoC 的**硬件描述语言**。源格式（`.dts`）由 `dtc` 编译为二进制 blob（`.dtb`）。bootloader（U-Boot、EDK II）在启动时把 DTB 物理地址传给 kernel（放在 CPU 寄存器中：ARM64 上是 `x0`，ARM32 上是 `r2`）。kernel 解析它以发现所有平台设备。

```
DTS → DTB → Kernel boot flow:

camera_isp.dts
    │
    │ dtc -I dts -O dtb
    ▼
camera_isp.dtb   (binary blob, ~10–500 KB)
    │
    │ bootloader loads DTB, passes PA in x0 register
    ▼
Linux kernel
    │
    │ of_platform_populate() scans DTB
    │ creates platform_device for each node with status="okay"
    ▼
platform_device "camera_isp@fe100000"
    │
    │ platform_bus match loop
    │ finds driver with compatible="vendor,cam-isp-v3"
    ▼
mydriver.probe(pdev) called
    │
    │ driver maps MMIO, registers IRQ, enables clocks
    ▼
Device ready for use
```

### 节点结构

```dts
soc {
    camera_isp@fe100000 {
        compatible = "vendor,cam-isp-v3", "vendor,cam-isp";  /* primary + fallback */
        reg = <0 0xfe100000 0 0x10000>;      /* 64KB MMIO at PA 0xfe100000 */
        interrupts = <GIC_SPI 25 IRQ_TYPE_LEVEL_HIGH>;
        clocks = <&cru SCLK_ISP_CLK>, <&cru ACLK_ISP_CLK>;
        clock-names = "isp", "aclk";
        resets = <&cru SRST_ISP>;
        reset-names = "isp";
        dmas = <&dmac1 5>;
        dma-names = "dma";
        power-domains = <&power RK3588_PD_ISP>;
        status = "okay";
    };
};
```

`compatible` 属性按**从最具体到最不具体**的顺序列出 ID。驱动的 `of_match_table` 会与所有条目逐一比对；**首个匹配者胜出**。这使一个驱动可以支持多个硬件版本。


<details>
<summary>English original</summary>

**Platform Devices**

PCIe and USB devices are **self-describing**: they carry vendor IDs, device IDs, and capability registers that the bus can read automatically. SoC peripherals have **no such mechanism** — there is no way to dynamically discover a UART controller at address 0xFE200000.

SoC peripherals (UART, I2C controller, camera CSI, AI accelerator, FPGA AXI slave) are not self-describing; the bus cannot enumerate them. They are registered as **platform devices** using descriptions from Device Tree or ACPI.

```
Platform Driver Registration

Device Tree (.dts)          Kernel Driver (C code)
┌──────────────────┐        ┌─────────────────────────────┐
│ mydev@40000000   │        │ static const struct         │
│ compatible =     │        │   of_device_id mydev_ids[] =│
│  "vendor,mydev-v2"│─match─│ { .compatible =             │
│ reg = <0x40000000 │       │   "vendor,mydev-v2" },      │
│       0x10000>   │        │ { .compatible =             │
│ interrupts = ... │        │   "vendor,mydev-v1" }, {}   │
│ status = "okay"  │        │ };                          │
└──────────────────┘        │                             │
                            │ .probe  = mydev_probe,      │
                            │ .remove = mydev_remove,     │
                            └─────────────────────────────┘
```

```c
static const struct of_device_id mydev_of_match[] = {
    { .compatible = "vendor,mydevice-v2" },
    { .compatible = "vendor,mydevice-v1" },   /* older silicon */
    { }   /* sentinel — marks end of the table */
};
MODULE_DEVICE_TABLE(of, mydev_of_match);

static struct platform_driver mydev_driver = {
    .probe  = mydev_probe,
    .remove = mydev_remove,
    .driver = {
        .name           = "mydevice",
        .of_match_table = mydev_of_match,
    },
};
module_platform_driver(mydev_driver);
```

`module_platform_driver()` expands to `module_init` / `module_exit` wrappers that call `platform_driver_register()` and `platform_driver_unregister()`.

`MODULE_DEVICE_TABLE(of, mydev_of_match)` embeds the match table in the module binary. `udevd` uses this to **automatically load the module** when a matching Device Tree node is discovered — without requiring manual `modprobe`.

---

**Device Tree (DTS)**

**Device Tree** is a **hardware description language** for embedded SoCs. The source format (`.dts`) is compiled by `dtc` into a binary blob (`.dtb`). The bootloader (U-Boot, EDK II) passes the DTB physical address to the kernel at boot (in a CPU register: `x0` on ARM64, `r2` on ARM32). The kernel parses it to discover all platform devices.

```
DTS → DTB → Kernel boot flow:

camera_isp.dts
    │
    │ dtc -I dts -O dtb
    ▼
camera_isp.dtb   (binary blob, ~10–500 KB)
    │
    │ bootloader loads DTB, passes PA in x0 register
    ▼
Linux kernel
    │
    │ of_platform_populate() scans DTB
    │ creates platform_device for each node with status="okay"
    ▼
platform_device "camera_isp@fe100000"
    │
    │ platform_bus match loop
    │ finds driver with compatible="vendor,cam-isp-v3"
    ▼
mydriver.probe(pdev) called
    │
    │ driver maps MMIO, registers IRQ, enables clocks
    ▼
Device ready for use
```

**Node Structure**

```dts
soc {
    camera_isp@fe100000 {
        compatible = "vendor,cam-isp-v3", "vendor,cam-isp";  /* primary + fallback */
        reg = <0 0xfe100000 0 0x10000>;      /* 64KB MMIO at PA 0xfe100000 */
        interrupts = <GIC_SPI 25 IRQ_TYPE_LEVEL_HIGH>;
        clocks = <&cru SCLK_ISP_CLK>, <&cru ACLK_ISP_CLK>;
        clock-names = "isp", "aclk";
        resets = <&cru SRST_ISP>;
        reset-names = "isp";
        dmas = <&dmac1 5>;
        dma-names = "dma";
        power-domains = <&power RK3588_PD_ISP>;
        status = "okay";
    };
};
```

The `compatible` property lists IDs from **most-specific to least-specific**. The driver's `of_match_table` is checked against all entries; the **first match wins**. This allows one driver to support multiple hardware revisions.

</details>

### 关键 DTS 属性

| 属性 | 解析方 | Kernel API |
|----------|-----------|-----------|
| `compatible` | 总线匹配逻辑 | `of_match_table` 查找 |
| `reg` | 资源子系统 | `platform_get_resource()`, `devm_ioremap_resource()` |
| `interrupts` | IRQ 子系统 | `platform_get_irq()`, `devm_request_irq()` |
| `clocks` / `clock-names` | 时钟框架 | `devm_clk_get(dev, "isp")`, `clk_prepare_enable()` |
| `resets` / `reset-names` | 复位控制器 | `devm_reset_control_get(dev, "isp")` |
| `dmas` / `dma-names` | DMA 引擎 | `dma_request_chan(dev, "dma")` |
| `power-domains` | genpd | `pm_runtime_get()` 自动开启 |
| `status` | 启动扫描 | `"okay"` 使能；`"disabled"` 跳过 probe |

> **关键洞察：** `status = "okay"` / `"disabled"` 属性正是 DTS 在不重新编译 kernel 的情况下控制哪些硬件模块处于激活状态的方式。要禁用一个外设，就在 DTS overlay 中设置 `status = "disabled"` —— kernel 会完全跳过它的 probe。Jetson 载板定制就是这么实现的。

---

## 设备树 Overlay（DTBO）

基础 DTB 描述的是 SoC 本身。对于支持多种硬件配置的板卡 —— 不同的 camera sensor、可选外设、扩展模块 —— 为每种变体重新构建完整 DTB 并不现实。**Overlay** 解决了这个问题：它们是在基础 DTB 之上应用的增量补丁。

Overlay 是对基础 DTB 的**增量添加**，在 runtime 应用（通过 `/sys/kernel/config/device-tree/overlays/`）或由 bootloader 应用。它们添加、修改或删除节点，**无需重新编译完整 DTB**。

```bash
# Compile overlay source to binary
dtc -I dts -O dtb -o camera_imx477.dtbo camera_imx477.dts

# Jetson: reference in extlinux.conf
FDT_OVERLAYS /boot/camera_imx477.dtbo

# Runtime apply (if ConfigFS enabled)
mkdir /sys/kernel/config/device-tree/overlays/camera
cp camera_imx477.dtbo /sys/kernel/config/device-tree/overlays/camera/dtbo
echo 1 > /sys/kernel/config/device-tree/overlays/camera/status
```

用于 Jetson Xavier/Orin 的 camera 载板（sensor 模块）配置，以及 Raspberry Pi 的 HAT 描述符 overlay。

---


<details>
<summary>English original</summary>

**Key DTS Properties**

| Property | Parsed by | Kernel API |
|----------|-----------|-----------|
| `compatible` | Bus match logic | `of_match_table` lookup |
| `reg` | Resource subsystem | `platform_get_resource()`, `devm_ioremap_resource()` |
| `interrupts` | IRQ subsystem | `platform_get_irq()`, `devm_request_irq()` |
| `clocks` / `clock-names` | Clock framework | `devm_clk_get(dev, "isp")`, `clk_prepare_enable()` |
| `resets` / `reset-names` | Reset controller | `devm_reset_control_get(dev, "isp")` |
| `dmas` / `dma-names` | DMA engine | `dma_request_chan(dev, "dma")` |
| `power-domains` | genpd | Automatic on `pm_runtime_get()` |
| `status` | Boot scan | `"okay"` enables; `"disabled"` skips probe |

> **Key Insight:** The `status = "okay"` / `"disabled"` property is how the DTS controls which hardware blocks are active without recompiling the kernel. To disable a peripheral, set `status = "disabled"` in a DTS overlay — the kernel skips its probe entirely. This is how Jetson carrier board customization works.

---

**Device Tree Overlays (DTBO)**

The base DTB describes the SoC itself. For boards that support multiple hardware configurations — different camera sensors, optional peripherals, add-on modules — rebuilding the full DTB for each variant is impractical. **Overlays** solve this: they are incremental patches applied on top of the base DTB.

Overlays are **incremental additions** to the base DTB, applied at runtime (via `/sys/kernel/config/device-tree/overlays/`) or by the bootloader. They add, modify, or delete nodes **without recompiling the full DTB**.

```bash
# Compile overlay source to binary
dtc -I dts -O dtb -o camera_imx477.dtbo camera_imx477.dts

# Jetson: reference in extlinux.conf
FDT_OVERLAYS /boot/camera_imx477.dtbo

# Runtime apply (if ConfigFS enabled)
mkdir /sys/kernel/config/device-tree/overlays/camera
cp camera_imx477.dtbo /sys/kernel/config/device-tree/overlays/camera/dtbo
echo 1 > /sys/kernel/config/device-tree/overlays/camera/status
```

Used in Jetson Xavier/Orin for camera carrier board (sensor module) configuration and in Raspberry Pi for HAT descriptor overlays.

---

</details>

## probe() 函数：资源设置模式

`probe()` 函数是驱动的**初始化入口点**。其职责是申请所有硬件资源、初始化设备，并将其注册到上层子系统（V4L2、IIO 等）。关键模式是对每一次分配都使用 `devm_*`（**受管资源**）函数：

```c
static int mydev_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct mydev_priv *priv;
    struct resource *res;
    int irq, ret;

    /* Allocate driver private state — freed automatically on probe failure or driver unbind */
    priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    /* Map MMIO registers from DTS 'reg' property — unmapped automatically on cleanup */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    priv->base = devm_ioremap_resource(dev, res);
    if (IS_ERR(priv->base))
        return PTR_ERR(priv->base);   /* devm unwinds previous allocations */

    /* Register interrupt handler from DTS 'interrupts' property */
    irq = platform_get_irq(pdev, 0);
    ret = devm_request_irq(dev, irq, mydev_isr,
                           IRQF_SHARED, "mydevice", priv);
    if (ret)
        return ret;

    /* Acquire clock and reset from DTS 'clocks' and 'resets' properties */
    priv->clk = devm_clk_get(dev, "core");
    priv->rst = devm_reset_control_get(dev, "rst");

    platform_set_drvdata(pdev, priv);   /* store priv pointer for later retrieval */
    return 0;
}
```

若任一 `devm_*` 调用失败且函数返回负错误码，此前获取的所有受管资源会**按逆序自动释放**。无需 `goto err_cleanup` 标签。

成功 probe 的分步流程：

1. **分配私有状态**（`devm_kzalloc`）——驱动专属的上下文结构，在设备生命周期内持续存在。
2. **映射 MMIO**（`devm_ioremap_resource`）——使硬件寄存器可通过 `readl`/`writel` 访问。
3. **注册 IRQ**（`devm_request_irq`）——将硬件中断线连接到驱动的 ISR 函数。
4. **获取时钟**（`devm_clk_get`）——拿到驱动外设的时钟句柄。
5. **获取复位**（`devm_reset_control_get`）——取得硬件复位线的控制权以便初始化。
6. **保存私有数据**（`platform_set_drvdata`）——使私有结构可在其他回调（ISR、fops、sysfs）中取回。
7. **返回 0**——向总线表明成功；设备此时已激活。

> **常见陷阱：** 在 `probe()` 中调用 `clk_prepare_enable()` 却忘了在 `remove()` 中调用 `clk_disable_unprepare()`，会导致驱动卸载后时钟仍在运行。在嵌入式系统上，这会浪费功耗，并可能阻止 SoC 进入低功耗睡眠状态。改用 `devm_clk_get_enabled()`（kernel 5.19+），它会在设备 detach 时自动关闭时钟。

---

## devm_* 受管资源

`devres` 框架正是让 `devm_*` 模式得以成立的基础。每次 `devm_*` 调用都会向设备的资源列表**注册一个清理动作**。当设备与驱动解绑，或 `probe()` 返回错误时，框架会**按逆序**遍历资源列表，逐个调用清理函数。

`devres` 框架会在设备与驱动解绑时，或 `probe()` 返回错误时自动释放资源：

| 函数 | detach 时释放的资源 |
|----------|-----------------------------|
| `devm_kzalloc()` | `kfree()` |
| `devm_ioremap_resource()` | `iounmap()` + `release_mem_region()` |
| `devm_request_irq()` | `free_irq()` |
| `devm_clk_get()` | `clk_put()` |
| `devm_reset_control_get()` | `reset_control_put()` |
| `devm_regulator_get()` | `regulator_put()` |
| `devm_gpiod_get()` | `gpiod_put()` |

自定义受管资源：`devm_add_action(dev, fn, data)` 可注册任意清理函数。

> **关键洞察：** `devres` 框架把驱动错误处理问题从“在每一条可能的失败路径上都正确地撤销一切”转变为“只需返回错误码”。框架会自动处理回退。这消除了一整类曾困扰 pre-devres 驱动的资源泄漏 bug，在迭代式 FPGA bring-up（上电点亮/调通）中尤为明显——那里的 `modprobe`/`rmmod` 循环会发生几十次。

---


<details>
<summary>English original</summary>

**probe() Function: Resource Setup Pattern**

The `probe()` function is the driver's **initialization entry point**. Its job is to claim all hardware resources, initialize the device, and register it with any higher-level subsystems (V4L2, IIO, etc.). The key pattern is using `devm_*` (**managed resource**) functions for every allocation:

```c
static int mydev_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    struct mydev_priv *priv;
    struct resource *res;
    int irq, ret;

    /* Allocate driver private state — freed automatically on probe failure or driver unbind */
    priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;

    /* Map MMIO registers from DTS 'reg' property — unmapped automatically on cleanup */
    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    priv->base = devm_ioremap_resource(dev, res);
    if (IS_ERR(priv->base))
        return PTR_ERR(priv->base);   /* devm unwinds previous allocations */

    /* Register interrupt handler from DTS 'interrupts' property */
    irq = platform_get_irq(pdev, 0);
    ret = devm_request_irq(dev, irq, mydev_isr,
                           IRQF_SHARED, "mydevice", priv);
    if (ret)
        return ret;

    /* Acquire clock and reset from DTS 'clocks' and 'resets' properties */
    priv->clk = devm_clk_get(dev, "core");
    priv->rst = devm_reset_control_get(dev, "rst");

    platform_set_drvdata(pdev, priv);   /* store priv pointer for later retrieval */
    return 0;
}
```

If any `devm_*` call fails and the function returns a negative error code, all previously acquired managed resources are **automatically released in reverse order**. No `goto err_cleanup` labels needed.

The step-by-step sequence of a successful probe:

1. **Allocate private state** (`devm_kzalloc`) — driver-specific context structure that persists for the device lifetime.
2. **Map MMIO** (`devm_ioremap_resource`) — makes hardware registers accessible via `readl`/`writel`.
3. **Register IRQ** (`devm_request_irq`) — connects the hardware interrupt line to the driver's ISR function.
4. **Acquire clocks** (`devm_clk_get`) — gets a handle to the clock that drives the peripheral.
5. **Acquire reset** (`devm_reset_control_get`) — gets control of the hardware reset line for initialization.
6. **Store private data** (`platform_set_drvdata`) — makes the private structure retrievable in other callbacks (ISR, fops, sysfs).
7. **Return 0** — signals success to the bus; device is now active.

> **Common Pitfall:** Calling `clk_prepare_enable()` in `probe()` but forgetting to call `clk_disable_unprepare()` in `remove()` leaves the clock running after the driver unloads. On embedded systems, this wastes power and may prevent the SoC from entering low-power sleep states. Use `devm_clk_get_enabled()` (kernel 5.19+) which disables the clock automatically on device detach.

---

**devm_* Managed Resources**

The `devres` framework is what makes the `devm_*` pattern work. Each `devm_*` call **registers a cleanup action** with the device's resource list. When the device is unbound from its driver or when `probe()` returns an error, the framework walks the resource list **in reverse order** and calls each cleanup function.

`devres` framework automatically releases resources when the device is unbound from its driver or when `probe()` returns an error:

| Function | Resource released on detach |
|----------|-----------------------------|
| `devm_kzalloc()` | `kfree()` |
| `devm_ioremap_resource()` | `iounmap()` + `release_mem_region()` |
| `devm_request_irq()` | `free_irq()` |
| `devm_clk_get()` | `clk_put()` |
| `devm_reset_control_get()` | `reset_control_put()` |
| `devm_regulator_get()` | `regulator_put()` |
| `devm_gpiod_get()` | `gpiod_put()` |

Custom managed resources: `devm_add_action(dev, fn, data)` registers an arbitrary cleanup function.

> **Key Insight:** The `devres` framework transforms the driver error handling problem from "undo everything correctly on every possible failure path" to "just return the error code." The framework handles the unwinding automatically. This eliminates an entire class of resource leak bugs that plagued pre-devres drivers, especially on iterative FPGA bring-up where `modprobe`/`rmmod` cycles happen dozens of times.

---

</details>

## sysfs 属性

驱动一旦完成 probe，就能通过 **sysfs** 虚拟文件系统向**用户空间暴露硬件状态**。sysfs 属性以文件形式出现在 `/sys/bus/platform/devices/` 下，可用标准文件操作读或写。

```c
static ssize_t utilization_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
    struct mydev_priv *priv = dev_get_drvdata(dev);
    return sysfs_emit(buf, "%u\n", readl(priv->base + REG_UTIL));
}
static DEVICE_ATTR_RO(utilization);

static struct attribute *mydev_attrs[] = {
    &dev_attr_utilization.attr,
    NULL,
};
ATTRIBUTE_GROUPS(mydev);
```

sysfs 属性出现在 `/sys/bus/platform/devices/mydevice.0/utilization` 下。用户空间用 `cat` 读取，用 `echo` 写入。属性必须可重入；用 mutex 或 spinlock 保护共享状态。

sysfs 属性属于 **ABI** —— 一旦导出到用户空间，删除或改名会**破坏用户空间的使用方**。只导出稳定、有意义的值。可能随内核版本变化的内核调试状态请用 `debugfs`（见下）。

sysfs 讲完了，下一部分看内核如何在设备出现或消失时通知用户空间。

---

## udev：用户空间设备管理

设备**添加/移除**时，内核发出 **uevent** netlink 消息。`udevd` 接收这些消息，匹配 `/etc/udev/rules.d/` 规则并执行动作：

- 创建 `/dev/` 设备节点，赋予正确的属主与权限
- 通过 `request_firmware()` 机制加载固件
- 对带有匹配 `MODULE_DEVICE_TABLE` 条目的模块运行 `modprobe`
- 执行自定义脚本完成设备初始化

```
SUBSYSTEM=="video4linux", ATTRS{name}=="IMX477*", SYMLINK+="camera0", MODE="0666"
```

只要检测到 IMX477 传感器，该规则就创建一个稳定符号链接 `/dev/camera0`，指向实际的 `/dev/video0` 节点。这样在存在多个摄像头时，应用代码就不受设备号变化的影响。

---

## debugfs

驱动通过 **debugfs** 暴露内部状态，用于开发诊断：

```c
debugfs_create_u32("frame_count", 0444, priv->dbgfs_dir, &priv->frame_count);
debugfs_create_file("regs", 0444, priv->dbgfs_dir, priv, &mydev_regs_fops);
```

可通过 `/sys/kernel/debug/mydevice/` 访问。它不保证 ABI 稳定，可能随内核版本变化。`media-ctl` 和 `v4l2-ctl` 使用 media controller 与 V4L2 ioctl 接口（这些接口 ABI 稳定）来配置流水线拓扑。

debugfs 纯粹用于开发和调试。与 sysfs 不同，它不提供任何 ABI 稳定性保证。生产监控应从 sysfs 读取；开发用的寄存器转储和内部计数器应放在 debugfs 中。

---

## 小结

| Bus type | Self-describing? | DT needed? | Match mechanism | Example |
|----------|-----------------|-----------|----------------|---------|
| PCIe | Yes (config space) | No | `pci_device_id` vendor:device | NVIDIA GPU, Intel E810 NIC |
| USB | Yes (descriptor) | No | `usb_device_id` vid:pid | UVC camera, USB GNSS |
| Platform | No | Yes (DTS/ACPI) | `of_match_table` compatible | UART, FPGA AXI peripheral |
| I2C | Partial (address) | Yes | `of_match_table` + I2C address | IMX477, ICM-42688 IMU |
| SPI | Partial (CS) | Yes | `of_match_table` + SPI chip select | ADC, display controller |


<details>
<summary>English original</summary>

**sysfs Attributes**

Once a driver is probed, it can expose **hardware state to userspace** through the **sysfs** virtual filesystem. sysfs attributes appear as files under `/sys/bus/platform/devices/` and can be read or written with standard file operations.

```c
static ssize_t utilization_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
    struct mydev_priv *priv = dev_get_drvdata(dev);
    return sysfs_emit(buf, "%u\n", readl(priv->base + REG_UTIL));
}
static DEVICE_ATTR_RO(utilization);

static struct attribute *mydev_attrs[] = {
    &dev_attr_utilization.attr,
    NULL,
};
ATTRIBUTE_GROUPS(mydev);
```

sysfs attributes appear at `/sys/bus/platform/devices/mydevice.0/utilization`. Userspace reads with `cat`; write with `echo`. Must be re-entrant; protect shared state with a mutex or spinlock.

sysfs attributes are **ABI** — once exported to userspace, removing or renaming them **breaks userspace consumers**. Export only stable, meaningful values. Use `debugfs` (below) for internal debug state that may change between kernel versions.

With sysfs covered, the next piece is how the kernel notifies userspace when devices appear or disappear.

---

**udev: Userspace Device Management**

The kernel emits **uevent** netlink messages on **device add/remove**. `udevd` receives them, matches `/etc/udev/rules.d/` rules, and acts:

- Creates `/dev/` device nodes with correct owner and permissions
- Loads firmware via `request_firmware()` mechanism
- Runs `modprobe` for modules with matching `MODULE_DEVICE_TABLE` entries
- Executes custom scripts for device initialization

```
SUBSYSTEM=="video4linux", ATTRS{name}=="IMX477*", SYMLINK+="camera0", MODE="0666"
```

This rule creates a stable symlink `/dev/camera0` pointing to the actual `/dev/video0` node whenever an IMX477 sensor is detected. This insulates application code from device number changes when multiple cameras are present.

---

**debugfs**

Drivers expose internal state for development diagnostics via **debugfs**:

```c
debugfs_create_u32("frame_count", 0444, priv->dbgfs_dir, &priv->frame_count);
debugfs_create_file("regs", 0444, priv->dbgfs_dir, priv, &mydev_regs_fops);
```

Accessible at `/sys/kernel/debug/mydevice/`. Not ABI-stable; may change between kernel versions. `media-ctl` and `v4l2-ctl` use the media controller and V4L2 ioctl interfaces (which do have stable ABI) for pipeline topology configuration.

debugfs is meant purely for development and debugging. Unlike sysfs, it carries no ABI stability guarantees. Production monitoring should read from sysfs; development register dumps and internal counters belong in debugfs.

---

**Summary**

| Bus type | Self-describing? | DT needed? | Match mechanism | Example |
|----------|-----------------|-----------|----------------|---------|
| PCIe | Yes (config space) | No | `pci_device_id` vendor:device | NVIDIA GPU, Intel E810 NIC |
| USB | Yes (descriptor) | No | `usb_device_id` vid:pid | UVC camera, USB GNSS |
| Platform | No | Yes (DTS/ACPI) | `of_match_table` compatible | UART, FPGA AXI peripheral |
| I2C | Partial (address) | Yes | `of_match_table` + I2C address | IMX477, ICM-42688 IMU |
| SPI | Partial (CS) | Yes | `of_match_table` + SPI chip select | ADC, display controller |

</details>

### 概念回顾

- **为什么平台设备需要设备树，而 PCIe 设备不需要？** PCIe 设备在片上配置空间中自带自识别信息（vendor ID、device ID），由总线控制器自动读取。SoC 外设硬连线在固定地址上，不具备自识别能力。设备树提供了硬件自身无法提供的描述。

- **当驱动模块被加载、且存在匹配的 DTS 节点时会发生什么？** 模块的 `module_init` 调用 `platform_driver_register()`。平台总线立即检查是否有已注册的设备匹配该驱动的 `of_match_table`。若有，则立刻调用 `probe()`。反之，若设备先注册，则总线在匹配的驱动注册时调用 `probe()`。

- **`compatible` 属性包含多个字符串的用途是什么？** 它构成一个从最具体到最不具体的优先级列表。kernel 按顺序尝试每个字符串。新驱动可匹配 `"vendor,mydevice-v2"`，而较旧的兜底驱动匹配 `"vendor,mydevice"`。这使硬件改版能使用专用驱动，同时保持向后兼容。

- **为什么 `devm_*` 优于手动资源管理？** 手动资源管理要求在 `probe()` 中为每一条可能的错误路径编写正确的清理代码。新增一个资源就要更新每一个错误标签。`devm_*` 将清理自动化：无论失败发生在 `probe()` 的哪个位置，只要设备脱离，框架就按相反顺序回卷所有受管资源。

- **什么是设备树 overlay，何时使用？** overlay 是一份编译后的补丁（`.dtbo`），在启动时或 runtime 向基础 DTB 添加、修改或删除节点。它用于模块化的硬件配置，例如 Jetson 上的摄像头载板，同一块基础板可以安装不同的 sensor 模块。

- **sysfs 与 debugfs 在驱动状态方面有何区别？** sysfs 属性具有 ABI 稳定性：一旦导出，移除它们会破坏用户态工具和监控面板。它们用于稳定、有意义的取值，如设备状态和硬件计数器。debugfs 没有稳定性保证——它用于开发者诊断、寄存器 dump 和临时调试数据。

---

## AI 硬件关联

- 带有正确 `compatible` 字符串的 DTS 节点把 Jetson 的 IMX477 和 AR0234 摄像头 sensor 连接到 NVIDIA 的 V4L2 sensor 驱动；`reg` 指定 I2C 地址，`clocks` 配置 NVCSI 时钟树
- Jetson 上的 NVDLA 是平台设备，其 DTS 条目包含 MMIO 基址、IRQ 线、时钟域和 DMA 通道；kernel 驱动通过 `devm_ioremap_resource` 和 `dma_request_chan` 映射所有资源
- 用于传感器融合或预处理的定制 Xilinx Zynq AXI 外设在 DTS 中描述为平台设备；`reg` 指定 AXI slave 基址和大小；驱动用 `ioread32` / `iowrite32` 访问寄存器
- DTBO overlay 让 Jetson 开发套件无需重新烧录基础系统镜像即可热插拔摄像头模块配置，加速 sensor bring-up 迭代
- sysfs `DEVICE_ATTR_RO` 属性把 AI 加速器利用率计数器和错误寄存器暴露给 Prometheus node exporter 与健康监控守护进程，无需特权 ioctl
- `devm_*` 受管资源可防止 FPGA 驱动 `probe()` 错误路径中的资源泄漏，这些路径在迭代式硬件 bring-up 周期中每次模块重载时都会被执行


<details>
<summary>English original</summary>

**Conceptual Review**

- **Why do platform devices need Device Tree when PCIe devices do not?** PCIe devices carry self-identification (vendor ID, device ID) in on-chip configuration space that the bus controller reads automatically. SoC peripherals are hardwired at fixed addresses with no self-identification capability. Device Tree provides the description that the hardware itself cannot.

- **What happens when a driver module is loaded and a matching DTS node exists?** The module's `module_init` calls `platform_driver_register()`. The platform bus immediately checks if any already-registered devices match the driver's `of_match_table`. If so, it calls `probe()` right away. Conversely, if the device is registered first, the bus calls `probe()` when the matching driver registers.

- **What is the purpose of the `compatible` property having multiple strings?** It creates a priority list from most-specific to least-specific. The kernel tries each string in order. A new driver can match on `"vendor,mydevice-v2"` while an older, fallback driver matches on `"vendor,mydevice"`. This allows hardware revisions to use specialized drivers while maintaining backward compatibility.

- **Why is `devm_*` preferred over manual resource management?** Manual resource management requires a correct cleanup for every possible error path in `probe()`. Adding one new resource requires updating every error label. `devm_*` automates the cleanup: the framework unwinds all managed resources in reverse order whenever the device detaches, regardless of where in `probe()` the failure occurred.

- **What is a Device Tree overlay and when is it used?** An overlay is a compiled patch (`.dtbo`) that adds, modifies, or removes nodes from the base DTB at boot time or runtime. It is used for modular hardware configurations like camera carrier boards on Jetson, where the same base board can have different sensor modules installed.

- **What is the difference between sysfs and debugfs for driver state?** sysfs attributes are ABI-stable: once exported, removing them breaks userspace tools and monitoring dashboards. They are for stable, meaningful values like device state and hardware counters. debugfs has no stability guarantee — it is for developer diagnostics, register dumps, and transient debugging data.

---

**AI Hardware Connection**

- DTS nodes with correct `compatible` strings link Jetson IMX477 and AR0234 camera sensors to NVIDIA's V4L2 sensor drivers; `reg` specifies the I2C address and `clocks` provisions the NVCSI clock tree
- NVDLA on Jetson is a platform device with DTS entries for MMIO base, IRQ lines, clock domains, and DMA channels; the kernel driver maps all resources via `devm_ioremap_resource` and `dma_request_chan`
- Custom Xilinx Zynq AXI peripherals for sensor fusion or pre-processing are described as platform devices in the DTS; `reg` specifies AXI slave base address and size; the driver accesses registers with `ioread32` / `iowrite32`
- DTBO overlays enable hot-swappable camera module configuration on Jetson development kits without reflashing the base system image, accelerating sensor bring-up iteration
- sysfs `DEVICE_ATTR_RO` attributes expose AI accelerator utilization counters and error registers to Prometheus node exporters and health monitoring daemons without requiring privileged ioctls
- `devm_*` managed resources prevent resource leaks in FPGA driver `probe()` error paths, which are exercised at every module reload during iterative hardware bring-up cycles

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-17.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-17.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
