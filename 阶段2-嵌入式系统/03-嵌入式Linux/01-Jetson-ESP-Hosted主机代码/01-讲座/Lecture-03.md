---
title: 第 3 讲 — SPI transport、GPIO 与 IRQ 驱动的 bring-up
description: 第 3 讲 — SPI transport、GPIO 与 IRQ 驱动的 bring-up
---

# 第 3 讲 — SPI transport、GPIO 与 IRQ 驱动的 bring-up

**课程：** [Jetson ESP-Hosted Host Code 指南](../Guide.md) · **阶段 2 — 嵌入式 Linux**

**上一讲：** [第 02 讲](Lecture-02.md) · **下一讲：** [第 04 讲 — Wi-Fi 如何变成 `wlan0`](Lecture-04.md)

---

## 1. 要研究的 transport 文件

阅读：

- `esp_hosted_ng/host/spi/esp_spi.c`

这个文件是主机把：

- SPI 总线选择
- chip select
- handshake GPIO
- data-ready GPIO

变成 ESP 可用的 transport endpoint 的地方。

这使它成为学习 **transport glue** 在真实嵌入式 Linux 驱动中长什么样的最佳文件。

## 相关的 Linux 内核概念

本讲直接建立在以下内容之上：

- [OS 第 3 讲 — 中断、异常与下半部](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-03.md)
- [OS 第 17 讲 — Linux 设备驱动模型与设备树](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md)
- [OS 第 18 讲 — 字符驱动、中断驱动 I/O 与 V4L2](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-18.md)

OS 课程的第 3 讲给出了重要的中断模型：硬件触发 IRQ，内核快速确认它，真正的工作被安全地延后。这正是阅读 `gpio_to_irq(...)`、`request_irq(...)` 以及 `esp_spi.c` 中由 workqueue 驱动的 SPI 路径的正确框架。

第 17 讲解释了 Linux 如何把驱动与已描述的设备匹配起来，第 18 讲解释了设备活起来之后的事件驱动 I/O。二者共同解释了为什么这份代码先复用内核设备模型中的 `spi0.0`，然后把 handshake 和 data-ready GPIO 边沿变成中断驱动的 transport，而不是盲目轮询。

最重要的细节是：**IRQ 不是“仅仅一个 GPIO 变化通知”。** 在 Linux 中，IRQ 是内核对驱动做出的承诺：当硬件边沿到来时，驱动会通过中断路径被调度，并能以**低延迟**推进协议。

这就是为什么验证聚焦于 `/proc/interrupts` 的计数，而不只是引脚名。一个命名正确却从不产生 IRQ 的 GPIO 在运行上是无用的，而一条计数不断上升的线则证明：电气信号、GPIO 映射、IRQ 转换以及内核 handler 路径全都对齐了。

这里要追踪的内核接口有：

- `module_param(...)`
- SPI 设备查找与复用
- `gpio_to_irq(...)`
- `request_irq(...)`
- IRQ 驱动的工作调度

---

## 2. 从模块参数开始

在 `esp_spi.c` 顶部附近，找这些：

- `spi_bus_num`
- `spi_chip_select`
- `spi_handshake_gpio`
- `spi_dataready_gpio`
- `spi_mode`

它们以 `module_param(...)` 的形式暴露出来。

这说明：

- transport 接线现在可在加载时配置
- 驱动无需重新编译即可跨板卡复用

这是对硬编码板卡假设的直接改进。

你可以在 `spi/esp_spi.c` 顶部直接看到策略到 transport 的这座桥：

```c
static ushort spi_bus_num = DEFAULT_SPI_BUS_NUM;
static ushort spi_chip_select = DEFAULT_SPI_CHIP_SELECT;
static int spi_handshake_gpio = DEFAULT_HANDSHAKE_PIN;
static int spi_dataready_gpio = DEFAULT_SPI_DATA_READY_PIN;
static uint spi_mode = DEFAULT_SPI_MODE;

module_param(spi_bus_num, ushort, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
module_param(spi_chip_select, ushort, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
module_param(spi_handshake_gpio, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
module_param(spi_dataready_gpio, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
module_param(spi_mode, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
```

这里正是“Jetson 引脚选择”不再只是 shell 脚本文字、而成为驱动所消费的 transport 状态的地方。

---



---


<details>
<summary>English original</summary>

**Lecture 3 — SPI transport, GPIOs, and IRQ-driven bring-up**

**Course:** [Jetson ESP-Hosted Host Code guide](../Guide.md) · **Phase 2 — Embedded Linux**

**Previous:** [Lecture 02](Lecture-02.md) · **Next:** [Lecture 04 — How Wi-Fi becomes `wlan0`](Lecture-04.md)

---

**1. The transport file to study**

Read:

- `esp_hosted_ng/host/spi/esp_spi.c`

This file is where the host turns:

- SPI bus selection
- chip select
- handshake GPIO
- data-ready GPIO

into a working transport endpoint for the ESP.

That makes it the best file for learning how **transport glue** looks in a real Embedded Linux driver.

**Related Linux kernel concepts**

This lecture sits directly on top of:

- [OS Lecture 3 — Interrupts, Exceptions & Bottom Halves](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-03.md)
- [OS Lecture 17 — Linux Device Driver Model & Device Tree](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-17.md)
- [OS Lecture 18 — Character Drivers, Interrupt-Driven I/O & V4L2](../../../../阶段1-基础知识/03-操作系统/01-讲座/Lecture-18.md)

Lecture 3 from the OS course gives the important interrupt model: hardware raises an IRQ, the kernel acknowledges it quickly, and real work is deferred safely. That is the right frame for reading `gpio_to_irq(...)`, `request_irq(...)`, and the workqueue-driven SPI path in `esp_spi.c`.

Lecture 17 explains how Linux matches a driver to an already-described device, and Lecture 18 explains event-driven I/O once the device is alive. Together they explain why this code first reuses `spi0.0` from the kernel device model and then turns handshake and data-ready GPIO edges into an interrupt-driven transport instead of polling blindly.

The detail that matters most is that an **IRQ is not “just a GPIO change notification.”** In Linux, an IRQ is the kernel’s promise that when the hardware edge arrives, the driver will get scheduled through the interrupt path and can move the protocol forward with **low latency**.

That is why the validation focused on `/proc/interrupts` counts and not only on pin names. A correctly named GPIO that never produces an IRQ is operationally useless, while a line with rising counters proves that the electrical signal, the GPIO mapping, the IRQ translation, and the kernel handler path are all aligned.

The kernel interfaces to track here are:

- `module_param(...)`
- SPI device lookup and reuse
- `gpio_to_irq(...)`
- `request_irq(...)`
- IRQ-driven work scheduling

---

**2. Start at the module parameters**

Near the top of `esp_spi.c`, look for these:

- `spi_bus_num`
- `spi_chip_select`
- `spi_handshake_gpio`
- `spi_dataready_gpio`
- `spi_mode`

These are exposed as `module_param(...)`.

That tells you:

- transport wiring is now configurable at load time
- the driver can be reused across boards without recompilation

This is a direct improvement over hardcoded board assumptions.

You can see that policy-to-transport bridge right at the top of `spi/esp_spi.c`:

```c
static ushort spi_bus_num = DEFAULT_SPI_BUS_NUM;
static ushort spi_chip_select = DEFAULT_SPI_CHIP_SELECT;
static int spi_handshake_gpio = DEFAULT_HANDSHAKE_PIN;
static int spi_dataready_gpio = DEFAULT_SPI_DATA_READY_PIN;
static uint spi_mode = DEFAULT_SPI_MODE;

module_param(spi_bus_num, ushort, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
module_param(spi_chip_select, ushort, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
module_param(spi_handshake_gpio, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
module_param(spi_dataready_gpio, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
module_param(spi_mode, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
```

This is where “Jetson pin choices” stop being shell-script text and become transport state consumed by the driver.

---

</details>

## 3. 找到 `spi_dev_init(...)`

`spi_dev_init(...)` 是传输 bring-up（上电点亮/调通）的核心。

从概念上讲，它负责：

- 选择 Linux SPI 设备
- 配置 SPI 模式与时钟
- 建立 transport 上下文
- 申请并配置握手/数据就绪 GPIO
- 把这些 GPIO 映射为 IRQ

正是这一点，让“Linux 板级描述”变成**“活跃的 transport 端点”**。

这是嵌入式 Linux 上一个重要的思维转变：

- 设备树与 sysfs 证明的是**存在性**
- transport init 证明的是**可用性**

`spi_dev_init(...)` 里的关键部分值得逐字读一遍：

```c
esp_info("Config - SPI GPIOs: Handshake[%d] Dataready[%d]\n",
	spi_context.handshake_gpio, spi_context.dataready_gpio);

esp_info("Config - SPI clock[%dMHz] bus[%d] cs[%d] mode[%d]\n",
	spi_context.spi_clk_mhz, esp_board.bus_num,
	esp_board.chip_select, esp_board.mode);

status = gpio_request(spi_context.handshake_gpio, "SPI_HANDSHAKE_PIN");
...
spi_context.handshake_irq = gpio_to_irq(spi_context.handshake_gpio);
status = request_irq(spi_context.handshake_irq, spi_interrupt_handler,
		IRQF_SHARED | IRQF_TRIGGER_RISING,
		"ESP_SPI", spi_context.esp_spi_dev);
...
status = gpio_request(spi_context.dataready_gpio, "SPI_DATA_READY_PIN");
...
spi_context.dataready_irq = gpio_to_irq(spi_context.dataready_gpio);
status = request_irq(spi_context.dataready_irq, spi_data_ready_interrupt_handler,
		IRQF_SHARED | IRQF_TRIGGER_RISING,
		"ESP_SPI_DATA_READY", spi_context.esp_spi_dev);
```

这段代码把 transport bring-up 的整个过程集中在一处：

- 记录配置
- 申请 GPIO
- 把它们转换为 IRQ
- 绑定 IRQ handler，让 transport 真正活起来

---

## 4. 为什么 IRQ 比 GPIO 名字更重要

经过验证的 Jetson 路径使用了：

- `spi_handshake_gpio=471`
- `spi_dataready_gpio=433`

但光有这些编号并不算成功。

更强的成功信号是：

- host 驱动申请了它们
- 它们成为了 IRQ 源
- ESP 启动时中断计数发生了变化

调试 **基于 GPIO 的 bring-up** 就该这样做：

- 名字 -> 申请 -> IRQ 映射 -> 实际的边沿活动

如果你停留在“编号看起来对”这一步，那你**其实什么都没调试**。

---

## 5. 复用 `spi0.0` 是 Linux 设备模型的决策

Jetson 分支在可能的情况下会复用已有的 SPI 设备，例如：

- `spi0.0`

这一点很重要，因为 Linux 可能已经有了：

- 设备树节点
- 总线编号
- 片选映射

这比总是强迫驱动去新建一个 SPI 设备对象要好。

嵌入式 Linux 的教训：

- 当 kernel 的设备模型已经正确描述了硬件时，就尊重它

复用行为是显式的：

```c
existing_dev = spi_find_device(esp_board.bus_num, esp_board.chip_select);
if (existing_dev) {
	...
	spi_context.esp_spi_dev = existing_dev;
	esp_info("Reusing existing SPI device spi%u.%u\n",
		esp_board.bus_num, esp_board.chip_select);
} else {
	master = spi_busnum_to_master(esp_board.bus_num);
	...
	spi_context.esp_spi_dev = spi_new_device(master, &esp_board);
}
```

shell 脚本会 unbind `spidev`，但 transport 代码的写法是与已有的 SPI 设备路径协作的。

---

## 6. Runtime 时钟钳制是一项真实的 bring-up 策略

经过验证的 Jetson 工作改变了 runtime 时钟行为。

host 可以从 ESP 启动路径收到请求，要求切换到：

- `26 MHz`

但经过验证的 Jetson 流程有意将 host 限制在：

- `10 MHz`

为什么这很重要：

- 在 bring-up 期间，**transport 稳定性**胜过标称峰值速度
- 驱动现在把 `clockspeed=` 同时用作：
  - 初始速度
  - runtime 上限

这是一个非常真实的嵌入式 Linux 工程模式：

- 让代码反映经过验证的板级限制
- 之后只在有测量数据时才放宽

实际的钳制逻辑很简洁：

```c
static void adjust_spi_clock(u8 spi_clk_mhz)
{
	u8 target_spi_clk = spi_clk_mhz;

	if (spi_context.spi_clk_cap_mhz && target_spi_clk > spi_context.spi_clk_cap_mhz) {
		esp_info("ESP requested SPI CLK %u MHz, clamping to host limit %u MHz\n",
			 target_spi_clk, spi_context.spi_clk_cap_mhz);
		target_spi_clk = spi_context.spi_clk_cap_mhz;
	}

	if (target_spi_clk != spi_context.spi_clk_mhz) {
		esp_info("ESP Reconfigure SPI CLK to %u MHz\n", target_spi_clk);
		spi_context.spi_clk_mhz = target_spi_clk;
		spi_context.esp_spi_dev->max_speed_hz = target_spi_clk * NUMBER_1M;
	}
}
```

---

## 7. 硬件上成功是什么样子

关键的 transport 层日志行是：

- `Received ESP boot-up event`
- `Chipset=ESP32-C6 ID=0d detected over SPI`
- `ESP requested SPI CLK 26 MHz, clamping to host limit 10 MHz`

这证明了：

- SPI 数据传输是活的
- 协议交互是活的
- transport 策略正在被强制执行

这比下面这些强得多的成功判据：

- 模块已插入
- 没有 kernel oops
- `/dev/spidev0.0` 存在

---


<details>
<summary>English original</summary>

**3. Find `spi_dev_init(...)`**

`spi_dev_init(...)` is the heart of transport bring-up.

Conceptually, it is responsible for:

- selecting the Linux SPI device
- configuring SPI mode and clock
- setting up the transport context
- requesting and configuring the handshake/data-ready GPIOs
- mapping those GPIOs into IRQs

This is the point where a “Linux board description” becomes an **“active transport endpoint.”**

That is an important Embedded Linux mental shift:

- device tree and sysfs prove **presence**
- transport init proves **usability**

The critical part of `spi_dev_init(...)` is worth reading literally:

```c
esp_info("Config - SPI GPIOs: Handshake[%d] Dataready[%d]\n",
	spi_context.handshake_gpio, spi_context.dataready_gpio);

esp_info("Config - SPI clock[%dMHz] bus[%d] cs[%d] mode[%d]\n",
	spi_context.spi_clk_mhz, esp_board.bus_num,
	esp_board.chip_select, esp_board.mode);

status = gpio_request(spi_context.handshake_gpio, "SPI_HANDSHAKE_PIN");
...
spi_context.handshake_irq = gpio_to_irq(spi_context.handshake_gpio);
status = request_irq(spi_context.handshake_irq, spi_interrupt_handler,
		IRQF_SHARED | IRQF_TRIGGER_RISING,
		"ESP_SPI", spi_context.esp_spi_dev);
...
status = gpio_request(spi_context.dataready_gpio, "SPI_DATA_READY_PIN");
...
spi_context.dataready_irq = gpio_to_irq(spi_context.dataready_gpio);
status = request_irq(spi_context.dataready_irq, spi_data_ready_interrupt_handler,
		IRQF_SHARED | IRQF_TRIGGER_RISING,
		"ESP_SPI_DATA_READY", spi_context.esp_spi_dev);
```

That snippet is the transport bring-up story in one place:

- log the configuration
- request the GPIOs
- convert them to IRQs
- bind the IRQ handlers that make the transport live

---

**4. Why the IRQs matter more than the GPIO names**

The validated Jetson path used:

- `spi_handshake_gpio=471`
- `spi_dataready_gpio=433`

But those numbers alone are not success.

The stronger success signal is:

- the host driver requested them
- they became IRQ sources
- interrupt counters moved when the ESP booted

That is how you should debug **GPIO-based bring-up**:

- name -> request -> IRQ mapping -> actual edge activity

If you stop at “the number looks right,” you have **not really debugged anything**.

---

**5. Reusing `spi0.0` is a Linux device-model decision**

The Jetson fork reuses an existing SPI device such as:

- `spi0.0`

when possible.

That matters because Linux may already have:

- a device tree node
- a bus number
- a chip-select mapping

This is better than always forcing the driver to invent a new SPI device object.

Embedded Linux lesson:

- respect the kernel’s device model when it already describes the hardware correctly

The reuse behavior is explicit:

```c
existing_dev = spi_find_device(esp_board.bus_num, esp_board.chip_select);
if (existing_dev) {
	...
	spi_context.esp_spi_dev = existing_dev;
	esp_info("Reusing existing SPI device spi%u.%u\n",
		esp_board.bus_num, esp_board.chip_select);
} else {
	master = spi_busnum_to_master(esp_board.bus_num);
	...
	spi_context.esp_spi_dev = spi_new_device(master, &esp_board);
}
```

The shell script unbinds `spidev`, but the transport code is written to cooperate with the existing SPI device path.

---

**6. Runtime clock clamping is a real bring-up policy**

The validated Jetson work changed the runtime clock behavior.

The host can receive a request from the ESP boot-up path to move to:

- `26 MHz`

But the validated Jetson flow intentionally keeps the host capped at:

- `10 MHz`

Why this is important:

- **transport stability** beat nominal peak speed during bring-up
- the driver now uses `clockspeed=` as both:
  - initial speed
  - runtime ceiling

That is a very real Embedded Linux engineering pattern:

- make the code reflect the validated board limit
- then widen later only with measurement

The actual clamp logic is concise:

```c
static void adjust_spi_clock(u8 spi_clk_mhz)
{
	u8 target_spi_clk = spi_clk_mhz;

	if (spi_context.spi_clk_cap_mhz && target_spi_clk > spi_context.spi_clk_cap_mhz) {
		esp_info("ESP requested SPI CLK %u MHz, clamping to host limit %u MHz\n",
			 target_spi_clk, spi_context.spi_clk_cap_mhz);
		target_spi_clk = spi_context.spi_clk_cap_mhz;
	}

	if (target_spi_clk != spi_context.spi_clk_mhz) {
		esp_info("ESP Reconfigure SPI CLK to %u MHz\n", target_spi_clk);
		spi_context.spi_clk_mhz = target_spi_clk;
		spi_context.esp_spi_dev->max_speed_hz = target_spi_clk * NUMBER_1M;
	}
}
```

---

**7. What success looked like on hardware**

The key transport-level log lines were:

- `Received ESP boot-up event`
- `Chipset=ESP32-C6 ID=0d detected over SPI`
- `ESP requested SPI CLK 26 MHz, clamping to host limit 10 MHz`

That proves:

- SPI data transfer is alive
- the protocol exchange is alive
- the transport policy is being enforced

This is a much stronger success criterion than:

- module inserted
- no kernel oops
- `/dev/spidev0.0` exists

---

</details>

## 实验

完成以下任务：

1. 找出 `spi_context.spi_bus_num` 和 `spi_context.spi_chip_select` 在哪里被填充。
2. 找出握手与 data-ready 的 GPIO 值在哪里进入 transport 上下文。
3. 找出处理 runtime SPI 时钟变化的函数。
4. 写下在 bring-up（上电点亮/调通）期间你最想要的三条最有价值的 transport 层日志行。

---

**上一讲：** [第 02 讲](Lecture-02.md) · **下一讲：** [第 04 讲 — Wi-Fi 如何变成 `wlan0`](Lecture-04.md)


<details>
<summary>English original</summary>

**Lab**

Do these:

1. Find where `spi_context.spi_bus_num` and `spi_context.spi_chip_select` get filled.
2. Find where the handshake and data-ready GPIO values enter the transport context.
3. Find the function that handles runtime SPI clock changes.
4. Write down the three strongest transport-level log lines you would want during bring-up.

---

**Previous:** [Lecture 02](Lecture-02.md) · **Next:** [Lecture 04 — How Wi-Fi becomes `wlan0`](Lecture-04.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Jetson ESP-Hosted Host Code/Lecture/Lecture-03.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Jetson%20ESP-Hosted%20Host%20Code/Lecture/Lecture-03.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
