---
title: 第 5 讲：内核模块、启动流程与设备树
description: 第 5 讲：内核模块、启动流程与设备树
---

# 第 5 讲：内核模块、启动流程与设备树

## 概述

在任何 AI 推理发生之前，硬件必须初始化，kernel 必须加载，并且每个设备——摄像头传感器、NPU、GPIO 扩展器——都必须被发现并驱动。本讲要解决的核心挑战是：一个无法预先知道所有可能硬件的 kernel，如何仍能正确地驱动它们？心智模型有两部分：**启动顺序**作为信任链（每一阶段验证并移交给下一阶段），以及**设备树**作为硬件描述契约（一个数据文件，告诉 kernel 存在哪些硬件，而无需将其硬编码到 kernel 源码中）。对 AI 硬件工程师而言，每次 bring up 新的摄像头传感器、集成 FPGA 加速器，或调试推理加速器节点在启动时为何从未获得 `probe()` 调用时，这一点都很重要。

---

## ARM SoC 上的 Linux 启动流程

现代嵌入式 AI 板卡（Jetson Orin、Zynq UltraScale+、i.MX8）遵循该流程。每个阶段都是一个 **独立的程序**，它运行、初始化一层硬件，然后启动下一阶段。

| 阶段 | 负责方 | 加载的产物 | 备注 |
|---|---|---|---|
| 1. 上电 | SoC ROM（BootROM） | 来自掩膜 ROM 的 BootROM 代码 | 检查熔丝，从 flash 加载已签名的下一阶段 |
| 2. 主 bootloader | U-Boot SPL 或 UEFI stub | SPL / TF-A BL2 | 初始化 DRAM、最基本的时钟 |
| 3. 固件 | TF-A BL31 / PSCI | Secure monitor（EL3） | 设置 ATF，可选加载 OP-TEE |
| 4. 第二级 bootloader | U-Boot proper 或 UEFI | kernel + DTB + initramfs | 读取存储，设置启动参数 |
| 5. kernel 解压缩 | kernel `head.S` | 解压 `Image.gz` | 验证 DTB，设置早期分页 |
| 6. kernel 初始化 | `start_kernel()` | 初始化所有子系统 | 内存、调度器、驱动、VFS |
| 7. PID 1 | systemd（或 `init`） | 挂载 rootfs，启动服务 | udev 创建 `/dev` 节点 |

在 x86 上：UEFI（或 legacy BIOS）取代 U-Boot。在 Jetson 上：NVIDIA 的 MB1（Miniboot）和 MB2 对应 SPL 和 U-Boot；CBoot 是 NVIDIA 在 JetPack 5 中的 U-Boot 替代品。

```
ARM SoC Boot Chain — Jetson Example
                    Power applied
                         │
                         ▼
               ┌─────────────────┐
               │  BootROM (EL3)  │  ← burned into SoC silicon
               │  checks OTP     │
               │  verifies MB1   │
               └────────┬────────┘
                         │ loads signed MB1 from eMMC
                         ▼
               ┌─────────────────┐
               │  MB1 (EL3)      │  ← NVIDIA Miniboot
               │  init DRAM      │
               │  clock setup    │
               └────────┬────────┘
                         │ loads MB2 + TF-A BL31
                         ▼
               ┌─────────────────┐
               │  TF-A BL31(EL3) │  ← ARM Trusted Firmware
               │  PSCI handler   │  stays resident in EL3
               │  switches to EL1│
               └────────┬────────┘
                         │ loads CBoot / UEFI
                         ▼
               ┌─────────────────┐
               │  CBoot / UEFI   │  ← like U-Boot for Jetson
               │  (EL2 / EL1)    │
               │  loads kernel   │
               │  + DTB +initrd  │
               └────────┬────────┘
                         │ jumps to kernel entry (EL1)
                         ▼
               ┌─────────────────┐
               │  Linux Kernel   │  ← start_kernel()
               │  (EL1)          │
               │  subsystem init │
               │  device probing │
               └────────┬────────┘
                         │ executes /sbin/init
                         ▼
               ┌─────────────────┐
               │  systemd (EL0)  │  ← PID 1
               │  mounts rootfs  │
               │  starts services│
               └─────────────────┘
```

> **关键洞察：** 启动链不仅仅是一系列程序——它是一条 **信任链**。每个阶段在执行下一阶段之前都会验证其数字签名。如果链中任何一环断裂（密钥错误、二进制被篡改），启动就会停止。对于量产 AV 平台，这条链是安全基础：它保证运行中的 kernel 和推理二进制文件自制造商对其签名以来未被篡改。

---


<details>
<summary>English original</summary>

**Lecture 5: Kernel Modules, Boot Process & Device Tree**

**Overview**

Before any AI inference can happen, the hardware must be initialized, the kernel must be loaded, and every device — camera sensor, NPU, GPIO expander — must be discovered and driven. The core challenge this lecture addresses is: how does a kernel that cannot know about all possible hardware in advance still manage to drive it correctly? The mental model has two parts: the **boot sequence** as a chain of trust (each stage verifies and hands off to the next), and the **Device Tree** as a hardware description contract (a data file that tells the kernel what hardware exists without hardcoding it into kernel source). For an AI hardware engineer, this matters every time you bring up a new camera sensor, integrate an FPGA accelerator, or debug why an inference accelerator node never gets a `probe()` call at boot.

---

**Linux Boot Sequence on ARM SoC**

Modern embedded AI boards (Jetson Orin, Zynq UltraScale+, i.MX8) follow this sequence. Each stage is a **separate program** that runs, initializes a layer of hardware, and then launches the next stage.

| Stage | Responsible | Artifact loaded | Notes |
|---|---|---|---|
| 1. Power-on | SoC ROM (BootROM) | BootROM code from mask ROM | Checks fuses, loads signed next stage from flash |
| 2. Primary bootloader | U-Boot SPL or UEFI stub | SPL / TF-A BL2 | Initializes DRAM, minimal clocks |
| 3. Firmware | TF-A BL31 / PSCI | Secure monitor (EL3) | Sets up ATF, optionally loads OP-TEE |
| 4. Second bootloader | U-Boot proper or UEFI | Kernel + DTB + initramfs | Reads storage, sets boot args |
| 5. Kernel decompression | Kernel `head.S` | Decompresses `Image.gz` | Verifies DTB, sets up early paging |
| 6. Kernel init | `start_kernel()` | Initializes all subsystems | Memory, scheduler, drivers, VFS |
| 7. PID 1 | systemd (or `init`) | Mounts rootfs, starts services | udev creates `/dev` nodes |

On x86: UEFI (or legacy BIOS) replaces U-Boot. On Jetson: NVIDIA's MB1 (Miniboot) and MB2 correspond to SPL and U-Boot; CBoot is NVIDIA's U-Boot replacement in JetPack 5.

```
ARM SoC Boot Chain — Jetson Example
                    Power applied
                         │
                         ▼
               ┌─────────────────┐
               │  BootROM (EL3)  │  ← burned into SoC silicon
               │  checks OTP     │
               │  verifies MB1   │
               └────────┬────────┘
                         │ loads signed MB1 from eMMC
                         ▼
               ┌─────────────────┐
               │  MB1 (EL3)      │  ← NVIDIA Miniboot
               │  init DRAM      │
               │  clock setup    │
               └────────┬────────┘
                         │ loads MB2 + TF-A BL31
                         ▼
               ┌─────────────────┐
               │  TF-A BL31(EL3) │  ← ARM Trusted Firmware
               │  PSCI handler   │  stays resident in EL3
               │  switches to EL1│
               └────────┬────────┘
                         │ loads CBoot / UEFI
                         ▼
               ┌─────────────────┐
               │  CBoot / UEFI   │  ← like U-Boot for Jetson
               │  (EL2 / EL1)    │
               │  loads kernel   │
               │  + DTB +initrd  │
               └────────┬────────┘
                         │ jumps to kernel entry (EL1)
                         ▼
               ┌─────────────────┐
               │  Linux Kernel   │  ← start_kernel()
               │  (EL1)          │
               │  subsystem init │
               │  device probing │
               └────────┬────────┘
                         │ executes /sbin/init
                         ▼
               ┌─────────────────┐
               │  systemd (EL0)  │  ← PID 1
               │  mounts rootfs  │
               │  starts services│
               └─────────────────┘
```

> **Key Insight:** The boot chain is not just a sequence of programs — it is a **chain of trust**. Each stage verifies the digital signature of the next before executing it. If any link in the chain is broken (wrong key, tampered binary), the boot halts. For production AV platforms, this chain is the security foundation: it guarantees that the running kernel and inference binaries have not been tampered with since the manufacturer signed them.

---

</details>

## 安全启动

从 ROM 到运行中 kernel 的**信任链**：每一级在执行下一级之前都**验证其签名**。

- BootROM 用烧录进熔丝的密钥验证 SPL/BL2 签名
- BL2 验证 BL31 与 U-Boot/UEFI
- U-Boot 验证 kernel 镜像签名
- kernel 验证 rootfs（dm-verity）或模块签名

**Jetson 安全启动**：BCT（Boot Configuration Table）与 BL31 默认使用 NVIDIA 的密钥签名；生产部署则使用客户自己的 RSA-2048/4096 或 ECDSA 密钥对，烧录进 OTP。`tegraflash` 负责签名与烧写。

对量产的 AV 平台来说，**安全启动是强制要求**——可防止在 bootloader 阶段被换成恶意 kernel 镜像或被篡改的推理二进制。

> **常见陷阱：** 开发期间，图方便很容易把安全启动关掉。危险在于产品出货前忘了重新打开。关闭安全启动的开发镜像会接受任何未签名的 kernel——包括攻击者通过物理接触 eMMC 装入的那一个。始终在开启安全启动的状态下用开发者密钥开发，发布构建时再切换到生产密钥。

---

## UEFI vs U-Boot

| | UEFI | U-Boot |
|---|---|---|
| 主要领域 | x86 服务器、工作站、现代 ARM 服务器 | 嵌入式：Zynq、Jetson、Raspberry Pi、i.MX |
| 标准 | UEFI 规范（Tianocore/EDK2） | 开源；板级专属配置 |
| 启动协议 | kernel 中的 EFI stub；DTB 来自 EFI 配置 | 通过寄存器传递 kernel + DTB 地址 |
| 脚本 / 配置 | EFI 变量、NVRAM | U-Boot 环境变量、`boot.cmd` |
| 网络启动 | 经 UEFI 网络栈的 PXE | TFTP + NFS；嵌入式开发中很常见 |

两者都把 kernel 镜像、DTB 以及可选的 initramfs 加载进内存，然后跳转到 kernel 入口点。

---

## initramfs

- 压缩的 cpio 归档（gzip、lz4、zstd），内嵌在 kernel 中或单独加载
- 包含早期用户空间：busybox、udev、cryptsetup、`fsck`、自定义 `init` 脚本
- kernel 把它作为初始 rootfs 挂载到 tmpfs；运行 `/init`；挂载真正的 rootfs；`switch_root` 切换到永久根
- 在 Jetson 上，NVIDIA 用 initramfs 做早期 TNSPEC（板卡识别）以及基于 extlinux 的启动选择

```bash
mkdir /tmp/initrd && cd /tmp/initrd
zcat /boot/initrd.img | cpio -idm    # decompress and extract the initramfs cpio archive
ls -la                               # busybox, lib, udev, etc.
```

上面的 initramfs 解包展开整个早期用户空间，揭示在真正的根文件系统挂载之前 kernel 会用到哪些工具和脚本。

> **关键洞察：** initramfs 之所以存在，是因为缺少尚未加载的驱动时，kernel 并不总能挂载真正的根文件系统。例如，如果 root 位于 LUKS 加密的 NVMe 盘上，kernel 就需要从 initramfs 取得 `cryptsetup` 来解锁它，之后才能挂载 root。在 Jetson 上，initramfs 负责板卡识别（TNSPEC），使同一个 kernel 镜像能在载板不同的多种 Jetson 型号上启动。

理解了 kernel 如何被加载之后，接下来看它如何发现需要驱动的硬件——设备树。

---

## 设备树


<details>
<summary>English original</summary>

**Secure Boot**

**Trust chain** from ROM to running kernel: each stage **verifies the signature** of the next before executing it.

- BootROM verifies SPL/BL2 signature with a key burned into fuses
- BL2 verifies BL31 and U-Boot/UEFI
- U-Boot verifies kernel image signature
- Kernel verifies rootfs (dm-verity) or module signatures

**Jetson Secure Boot**: BCT (Boot Configuration Table) and BL31 are signed with NVIDIA's key by default; production deployment uses a customer RSA-2048/4096 or ECDSA key pair fused into OTP. `tegraflash` handles signing and flashing.

**Secure boot is mandatory** for production AV platforms — prevents substitution of a malicious kernel image or modified inference binary at the bootloader stage.

> **Common Pitfall:** During development, it is tempting to disable secure boot for convenience. The danger is forgetting to re-enable it before shipping a product. A development image with secure boot disabled will accept any unsigned kernel — including one an attacker installs via physical access to the eMMC. Always develop with secure boot enabled using developer keys, and switch to production keys for release builds.

---

**UEFI vs U-Boot**

| | UEFI | U-Boot |
|---|---|---|
| Primary domain | x86 servers, workstations, modern ARM servers | Embedded: Zynq, Jetson, Raspberry Pi, i.MX |
| Standard | UEFI specification (Tianocore/EDK2) | Open-source; board-specific configuration |
| Boot protocol | EFI stub in kernel; DTB from EFI config | Passes kernel + DTB address in registers |
| Script / config | EFI variables, NVRAM | U-Boot environment variables, `boot.cmd` |
| Network boot | PXE via UEFI network stack | TFTP + NFS; common for embedded development |

Both load the kernel image, a DTB, and optionally an initramfs into memory, then jump to the kernel entry point.

---

**initramfs**

- Compressed cpio archive (gzip, lz4, zstd) embedded in the kernel or loaded separately
- Contains early userspace: busybox, udev, cryptsetup, `fsck`, custom `init` script
- Kernel mounts it as the initial rootfs in a tmpfs; runs `/init`; mounts the real rootfs; `switch_root` to the permanent root
- On Jetson, NVIDIA uses initramfs for early TNSPEC (board identification) and extlinux-based boot selection

```bash
mkdir /tmp/initrd && cd /tmp/initrd
zcat /boot/initrd.img | cpio -idm    # decompress and extract the initramfs cpio archive
ls -la                               # busybox, lib, udev, etc.
```

The initramfs extraction above unpacks the entire early userspace, revealing what tools and scripts the kernel uses before the real root filesystem is mounted.

> **Key Insight:** The initramfs exists because the kernel cannot always mount the real root filesystem without drivers that haven't been loaded yet. For example, if root is on a LUKS-encrypted NVMe drive, the kernel needs `cryptsetup` from initramfs to unlock it before it can mount root. On Jetson, initramfs handles board identification (TNSPEC) so the same kernel image can boot on multiple Jetson variants with different carrier boards.

Now that we understand how the kernel gets loaded, let's look at how it discovers what hardware it needs to drive — the Device Tree.

---

**Device Tree**

</details>

### 用途

**硬件描述**，用于没有自描述总线的 SoC。PCIe 是自描述的（设备会自动上报 vendor/device ID）；I2C、SPI、UART、AXI 和 MMIO 外设则不是——kernel 必须被告知它们存在。

设备树用 bootloader 在 runtime 传给 kernel 的**数据文件**，取代了 kernel 源码中针对每块板子的 `#ifdef` hack。U-Boot 在跳转到 kernel 入口点之前，把 **DTB 地址**放入一个寄存器。

不妨把设备树看成一张接线图：它告诉 kernel “有一颗 Sony IMX477 摄像头传感器接在 I2C 总线 0 的地址 0x1a 上，由 GPIO 42 复位，其 CSI 输出连接到 CSI 端口 0”。

```
Device Tree → Driver Binding Flow
┌──────────────────────────────────────────────────────────┐
│  Device Tree (DTB file, loaded by bootloader)            │
│                                                          │
│  i2c0 {                                                  │
│    camera0: imx477@1a {                                  │
│      compatible = "sony,imx477";  ← binding key         │
│      reg = <0x1a>;                                       │
│    };                                                    │
│  };                                                      │
└───────────────────────┬──────────────────────────────────┘
                        │ kernel parses DTB at boot
                        ▼
┌──────────────────────────────────────────────────────────┐
│  Kernel OF (Open Firmware) matching                      │
│  → scans all registered drivers                         │
│  → finds imx477 driver with of_match_table:             │
│    { .compatible = "sony,imx477" }                      │
└───────────────────────┬──────────────────────────────────┘
                        │ match found
                        ▼
┌──────────────────────────────────────────────────────────┐
│  Driver probe() called                                   │
│  → reads reg property (I2C addr 0x1a)                   │
│  → requests GPIO 42 for reset                           │
│  → registers V4L2 subdevice                             │
│  → camera is now accessible at /dev/videoN              │
└──────────────────────────────────────────────────────────┘
```

### 节点结构

```dts
/* Example: IMX477 camera sensor on I2C bus */
&i2c0 {
    camera0: imx477@1a {
        compatible = "sony,imx477";      /* driver match string */
        reg = <0x1a>;                    /* I2C address 0x1a */
        clocks = <&clk IMX477_CLK>;      /* clock provider reference */
        clock-names = "xclk";
        reset-gpios = <&gpio 42 GPIO_ACTIVE_LOW>; /* GPIO 42, active-low reset */
        port {
            cam0_ep: endpoint {
                remote-endpoint = <&csi0_ep>;  /* connects to CSI port 0 */
                data-lanes = <1 2>;            /* uses MIPI D-PHY lanes 1 and 2 */
            };
        };
    };
};
```

| 属性 | 含义 |
|---|---|
| `compatible` | 字符串列表；kernel 用它来匹配驱动中的 `of_match_table` |
| `reg` | MMIO 基地址与大小，或总线地址（I2C、SPI） |
| `interrupts` | IRQ 说明符：GIC SPI 编号、触发类型 |
| `clocks` | 时钟提供者的 handle 与时钟 ID |
| `dma-names` | 分配给该设备的具名 DMA 通道 |
| `status` | `"okay"` 用于启用；`"disabled"` 用于抑制驱动的 probe |

### 编译与检查

```bash
dtc -I dts -O dtb -o my_board.dtb my_board.dts    # compile DTS → DTB binary
dtc -I dtb -O dts -o decoded.dts my_board.dtb      # decompile DTB back to readable DTS
ls /sys/firmware/devicetree/base/                   # live DT from running kernel
cat /sys/firmware/devicetree/base/model             # board model string
```


<details>
<summary>English original</summary>

**Purpose**

**Hardware description** for SoCs without self-describing buses. PCIe is self-describing (devices report vendor/device IDs); I2C, SPI, UART, AXI, and MMIO peripherals are not — the kernel must be told they exist.

The Device Tree replaces per-board `#ifdef` hacks in kernel source with a **data file** the bootloader passes to the kernel at runtime. U-Boot places the **DTB address** in a register before jumping to the kernel entry point.

Think of the Device Tree as a wiring diagram: it tells the kernel "there is a Sony IMX477 camera sensor connected to I2C bus 0 at address 0x1a, reset by GPIO 42, and its CSI output connects to CSI port 0."

```
Device Tree → Driver Binding Flow
┌──────────────────────────────────────────────────────────┐
│  Device Tree (DTB file, loaded by bootloader)            │
│                                                          │
│  i2c0 {                                                  │
│    camera0: imx477@1a {                                  │
│      compatible = "sony,imx477";  ← binding key         │
│      reg = <0x1a>;                                       │
│    };                                                    │
│  };                                                      │
└───────────────────────┬──────────────────────────────────┘
                        │ kernel parses DTB at boot
                        ▼
┌──────────────────────────────────────────────────────────┐
│  Kernel OF (Open Firmware) matching                      │
│  → scans all registered drivers                         │
│  → finds imx477 driver with of_match_table:             │
│    { .compatible = "sony,imx477" }                      │
└───────────────────────┬──────────────────────────────────┘
                        │ match found
                        ▼
┌──────────────────────────────────────────────────────────┐
│  Driver probe() called                                   │
│  → reads reg property (I2C addr 0x1a)                   │
│  → requests GPIO 42 for reset                           │
│  → registers V4L2 subdevice                             │
│  → camera is now accessible at /dev/videoN              │
└──────────────────────────────────────────────────────────┘
```

**Node Structure**

```dts
/* Example: IMX477 camera sensor on I2C bus */
&i2c0 {
    camera0: imx477@1a {
        compatible = "sony,imx477";      /* driver match string */
        reg = <0x1a>;                    /* I2C address 0x1a */
        clocks = <&clk IMX477_CLK>;      /* clock provider reference */
        clock-names = "xclk";
        reset-gpios = <&gpio 42 GPIO_ACTIVE_LOW>; /* GPIO 42, active-low reset */
        port {
            cam0_ep: endpoint {
                remote-endpoint = <&csi0_ep>;  /* connects to CSI port 0 */
                data-lanes = <1 2>;            /* uses MIPI D-PHY lanes 1 and 2 */
            };
        };
    };
};
```

| Property | Meaning |
|---|---|
| `compatible` | String list; kernel matches against `of_match_table` in driver |
| `reg` | MMIO base address and size, or bus address (I2C, SPI) |
| `interrupts` | IRQ specifier: GIC SPI number, trigger type |
| `clocks` | Clock provider handle and clock ID |
| `dma-names` | Named DMA channels assigned to the device |
| `status` | `"okay"` to enable; `"disabled"` to suppress driver probe |

**Compilation and Inspection**

```bash
dtc -I dts -O dtb -o my_board.dtb my_board.dts    # compile DTS → DTB binary
dtc -I dtb -O dts -o decoded.dts my_board.dtb      # decompile DTB back to readable DTS
ls /sys/firmware/devicetree/base/                   # live DT from running kernel
cat /sys/firmware/devicetree/base/model             # board model string
```

</details>

### 设备树 Overlay（DTBO）

**Overlay** 在 runtime 对基础 DT 打补丁，无需重新构建 kernel 或基础 DTB。这是为出厂即固定基础 DTB 的平台新增 sensor 或外设支持的机制。

Overlay 的应用流程：

1. **编写 `.dts` overlay 文件**，在基础树中新增或修改节点。
2. **用 `dtc` 编译为 `.dtbo`**。
3. **在 runtime 应用**（Raspberry Pi），或在 bootloader 中配置（Jetson extlinux.conf）。
4. **kernel 将 overlay 合并**到内存中的实时设备树。
5. 对任何新使能的节点**触发驱动 probe()**。

- **Jetson pin mux overlay**：为载板扩展引脚选择 UART、SPI 或 I2C 功能
- **Raspberry Pi HAT overlay**：使能 I2S 音频、SPI ADC、camera sensor 节点
- **Zynq PL overlay**：加载 partial bitstream，并为连接 FPGA 的 AXI 外设添加 DT 节点

```bash
dtoverlay imx477                        # apply overlay (Raspberry Pi)
cat /boot/extlinux/extlinux.conf        # FDT_OVERLAYS= line on Jetson
ls /sys/firmware/devicetree/base/       # verify overlay nodes appeared
```

> **关键洞察：** 设备树 overlay 是把载板外设加入基础 Jetson 或 Raspberry Pi 镜像的正确机制，无需修改厂商提供的基础 DTB。一块在非标准 I2C 总线上接了 IMX477 camera 的自定义载板，只需一个 20 行的 overlay 文件即可支持，而不必做完整的 DTB 修改——后者会在每次 L4T 更新时失效。

> **常见陷阱：** 如果某个驱动的 `probe()` 始终不运行，首先要检查该 DT 节点是否有 `status = "okay"`。若该属性缺失或被设为任何其他值，节点默认为 `"disabled"`。这是在平台之间移植 overlay 时的常见错误——必须把 `status` 属性显式设为 `"okay"`，kernel 才会尝试为该节点绑定驱动。

---

## 内核模块

### 模块入口、出口与设备匹配

```c
static const struct of_device_id my_of_match[] = {
    { .compatible = "vendor,my-accel" },  /* match DT node with this compatible string */
    { }                                    /* sentinel; marks end of match table */
};
MODULE_DEVICE_TABLE(of, my_of_match);    /* writes alias → modules.alias → udev auto-load */

static struct platform_driver my_driver = {
    .probe  = my_probe,    /* called when a matching DT node is found */
    .remove = my_remove,   /* called when the device is removed or module unloaded */
    .driver = {
        .name           = "my-accel",
        .of_match_table = my_of_match,  /* binds this driver to DT nodes */
    },
};
module_platform_driver(my_driver);       /* wraps module_init / module_exit */
MODULE_LICENSE("GPL");
```

当带有 `compatible = "vendor,my-accel"` 的 DT 节点出现时（启动时或通过 overlay），udev 读取 `modules.alias`，调用 `modprobe`，随后运行驱动的 `probe()` 函数。

### modprobe vs insmod

| 命令 | 行为 |
|---|---|
| `insmod my.ko` | 从文件路径加载；不解析依赖 |
| `modprobe mymodule` | 从 `/lib/modules/$(uname -r)/modules.dep` 解析并加载依赖 |
| `modprobe -r mymodule` | 卸载模块及任何未使用的依赖 |
| `depmod -a` | 安装新模块后重建 `modules.dep` |
| `lsmod` | 列出已加载模块、使用计数、依赖方 |
| `modinfo nvme` | 显示参数、license、固件版本字段 |

> **关键洞察：** `modprobe` 几乎总是正确的命令。`insmod` 加载单个 `.ko` 文件，不检查依赖。如果你的 FPGA PCIe 驱动依赖 `dma-buf` 子系统模块，`insmod` 会因晦涩的符号解析错误而失败，而 `modprobe` 会先自动加载 `dma-buf`。在脚本和 systemd unit 中始终使用 `modprobe`；只有在开发期间测试单个模块时才用 `insmod`。

### 模块签名

- `CONFIG_MODULE_SIG_FORCE` **拒绝未签名的模块**；量产的 Jetson 和车载 ECU kernel 强制执行此策略
- 构建时签名：`scripts/sign-file sha256 signing_key.pem signing_cert.pem my_driver.ko`
- 自定义 FPGA PCIe 驱动在安全启动平台上部署前必须先签名


<details>
<summary>English original</summary>

**Device Tree Overlays (DTBO)**

**Overlays** patch the base DT at runtime without rebuilding the kernel or base DTB. This is the mechanism for adding support for a new sensor or peripheral to a platform that ships with a fixed base DTB.

The overlay sequence:

1. **Write a `.dts` overlay file** that adds or modifies nodes in the base tree.
2. **Compile to `.dtbo`** using `dtc`.
3. **Apply at runtime** (Raspberry Pi) or configure in the bootloader (Jetson extlinux.conf).
4. **Kernel merges the overlay** into the live device tree in memory.
5. **Driver probe() fires** for any newly enabled node.

- **Jetson pin mux overlays**: select UART vs SPI vs I2C function for carrier board expansion pins
- **Raspberry Pi HAT overlays**: enable I2S audio, SPI ADC, camera sensor nodes
- **Zynq PL overlays**: load partial bitstream and add DT nodes for FPGA-connected AXI peripherals

```bash
dtoverlay imx477                        # apply overlay (Raspberry Pi)
cat /boot/extlinux/extlinux.conf        # FDT_OVERLAYS= line on Jetson
ls /sys/firmware/devicetree/base/       # verify overlay nodes appeared
```

> **Key Insight:** Device Tree overlays are the correct mechanism for adding carrier board peripherals to a base Jetson or Raspberry Pi image without modifying the vendor-provided base DTB. A custom carrier board with an IMX477 camera on a non-standard I2C bus can be supported with a 20-line overlay file, rather than requiring a full DTB modification that would break on every L4T update.

> **Common Pitfall:** If a driver's `probe()` never runs, the first thing to check is whether the DT node has `status = "okay"`. Nodes default to `"disabled"` if the property is absent or set to any other value. This is a frequent mistake when porting overlays between platforms — the `status` property must be explicitly set to `"okay"` for the kernel to attempt to bind a driver to the node.

---

**Kernel Modules**

**Module Entry, Exit, and Device Matching**

```c
static const struct of_device_id my_of_match[] = {
    { .compatible = "vendor,my-accel" },  /* match DT node with this compatible string */
    { }                                    /* sentinel; marks end of match table */
};
MODULE_DEVICE_TABLE(of, my_of_match);    /* writes alias → modules.alias → udev auto-load */

static struct platform_driver my_driver = {
    .probe  = my_probe,    /* called when a matching DT node is found */
    .remove = my_remove,   /* called when the device is removed or module unloaded */
    .driver = {
        .name           = "my-accel",
        .of_match_table = my_of_match,  /* binds this driver to DT nodes */
    },
};
module_platform_driver(my_driver);       /* wraps module_init / module_exit */
MODULE_LICENSE("GPL");
```

When a DT node with `compatible = "vendor,my-accel"` appears (at boot or via overlay), udev reads `modules.alias`, calls `modprobe`, and the driver's `probe()` function runs.

**modprobe vs insmod**

| Command | Behavior |
|---|---|
| `insmod my.ko` | Load from file path; no dependency resolution |
| `modprobe mymodule` | Resolve and load dependencies from `/lib/modules/$(uname -r)/modules.dep` |
| `modprobe -r mymodule` | Unload module and any unused dependencies |
| `depmod -a` | Rebuild `modules.dep` after installing new modules |
| `lsmod` | List loaded modules, usage count, dependents |
| `modinfo nvme` | Show parameters, license, firmware version fields |

> **Key Insight:** `modprobe` is almost always the right command to use. `insmod` loads a single `.ko` file without checking dependencies. If your FPGA PCIe driver depends on the `dma-buf` subsystem module, `insmod` will fail with a cryptic symbol resolution error, while `modprobe` automatically loads `dma-buf` first. Always use `modprobe` in scripts and systemd units; use `insmod` only when you are testing a single module during development.

**Module Signing**

- `CONFIG_MODULE_SIG_FORCE` **rejects unsigned modules**; production Jetson and automotive ECU kernels enforce this
- Sign during build: `scripts/sign-file sha256 signing_key.pem signing_cert.pem my_driver.ko`
- Custom FPGA PCIe driver must be signed before deployment on a secure-boot platform

</details>

### DKMS

**Dynamic Kernel Module Support** 在内核更新时自动重新构建树外模块。

```bash
dkms add -m my-fpga-driver -v 1.0       # register the module source with DKMS
dkms build -m my-fpga-driver -v 1.0    # compile against current kernel
dkms install -m my-fpga-driver -v 1.0  # install the compiled .ko
```

在开发主机上，DKMS 是 NVIDIA 专有 GPU 驱动的标准做法，也是自定义 FPGA PCIe 驱动的标准做法——这些驱动必须在 kernel 点版本升级后仍能存活，无需手动重构建。

> **常见陷阱：** 如果没有为新 kernel 版本安装 kernel headers，DKMS 重构建会静默失败。在一次升级 kernel 的 `apt upgrade` 之后，重启前先运行 `apt install linux-headers-$(uname -r)`，否则 DKMS 的安装后钩子会重建失败。在 CUDA/TensorRT 至关重要的系统上，把这步写进升级流程。

---

## Kernel Command Line

由 U-Boot（`bootargs` 环境变量）或 UEFI 传入。**可在 `/proc/cmdline` 处读取**。

| Parameter | Effect |
|---|---|
| `console=ttyS0,115200n8` | 启动时将内核日志输出到串口 |
| `root=/dev/mmcblk0p1` | 根文件系统设备 |
| `rdinit=/sbin/init` | initramfs 中的第一个用户空间进程 |
| `isolcpus=4-7` | 从通用调度器中排除 core 4–7 |
| `nohz_full=4-7` | 消除隔离 core 上的调度器 tick 中断 |
| `rcu_nocbs=4-7` | 将 RCU 回调移出隔离 core |
| `systemd.unit=inference.target` | 引导到自定义 systemd target |
| `nvidia-l4t-bootloader.secure-boot=1` | Jetson：启用校验启动链 |

```bash
cat /proc/cmdline                         # inspect active boot parameters
cat /sys/devices/system/cpu/isolated      # verify isolcpus took effect
```

CPU 隔离三件套——`isolcpus`、`nohz_full`、`rcu_nocbs`——作为一个整体协同工作：

1. **`isolcpus=4-7`**：把 core 4–7 从通用调度器池中移除。除非显式绑定，普通进程不会被调度到这些 core 上。
2. **`nohz_full=4-7`**：停止这些 core 上的 250 Hz 调度器 tick 中断。没有它，即使在被隔离的 core 上 tick 也会每秒触发 250 次，造成抖动。
3. **`rcu_nocbs=4-7`**：将 RCU（Read-Copy-Update）宽限期回调移出隔离 core。没有它，隔离 core 上会偶发 RCU 工作。

三者合在一起，让推理线程获得基本无中断的 CPU 时间。

> **关键洞见：** 仅靠 `isolcpus` 不足以实现实时隔离。没有 `nohz_full`，调度器 tick 会在隔离 core 上以 250 Hz 触发，引入 4 ms 的周期性抖动。没有 `rcu_nocbs`，RCU 回调会偶发触发——通常只有几微秒，但间隔不可预测。三者结合才能消除内核对隔离 core 执行的非自愿侵入。

> **常见陷阱：** 在 Jetson 上修改内核命令行需要改 `extlinux.conf`（针对 CBoot/extlinux）或 UEFI 启动变量（针对 JetPack 6 UEFI）。编辑 `/proc/cmdline` 没有效果——它是只读的。修改 `extlinux.conf` 后，重启并用 `cat /proc/cmdline` 验证参数确实生效。

---

## 小结

| Boot stage | Responsible | Artifact loaded | Jetson equivalent |
|---|---|---|---|
| BootROM | SoC 掩膜 ROM | 签名的第一阶段 loader | Jetson BootROM → MB1 |
| SPL / BL2 | U-Boot SPL / TF-A | DRAM 初始化；TF-A BL31 | MB1 → MB2 |
| Bootloader | U-Boot / CBoot | kernel + DTB + initramfs | CBoot（JetPack 5）、UEFI（JetPack 6） |
| Kernel entry | `head.S` → `start_kernel()` | 子系统初始化 | L4T kernel image |
| Early userspace | initramfs `/init` | 挂载真正的 rootfs | NVIDIA TNSPEC + `switch_root` |
| PID 1 | systemd | 所有服务、udev | Jetson systemd 推理 target |


<details>
<summary>English original</summary>

**DKMS**

**Dynamic Kernel Module Support** rebuilds out-of-tree modules automatically when the kernel is updated.

```bash
dkms add -m my-fpga-driver -v 1.0       # register the module source with DKMS
dkms build -m my-fpga-driver -v 1.0    # compile against current kernel
dkms install -m my-fpga-driver -v 1.0  # install the compiled .ko
```

DKMS is standard for the NVIDIA proprietary GPU driver on development hosts and for custom FPGA PCIe drivers that must survive kernel point-release updates without manual rebuilds.

> **Common Pitfall:** DKMS rebuilds fail silently if the kernel headers are not installed for the new kernel version. After a `apt upgrade` that upgrades the kernel, run `apt install linux-headers-$(uname -r)` before rebooting, or the DKMS post-install hook will fail to rebuild. On systems where CUDA/TensorRT is critical, include this in the upgrade procedure.

---

**Kernel Command Line**

Passed by U-Boot (`bootargs` env variable) or UEFI. **Readable at `/proc/cmdline`**.

| Parameter | Effect |
|---|---|
| `console=ttyS0,115200n8` | Kernel log to serial at boot |
| `root=/dev/mmcblk0p1` | Root filesystem device |
| `rdinit=/sbin/init` | First userspace process in initramfs |
| `isolcpus=4-7` | Exclude cores 4–7 from the general scheduler |
| `nohz_full=4-7` | Eliminate scheduler tick interrupts on isolated cores |
| `rcu_nocbs=4-7` | Move RCU callbacks off isolated cores |
| `systemd.unit=inference.target` | Boot to custom systemd target |
| `nvidia-l4t-bootloader.secure-boot=1` | Jetson: enable verified boot chain |

```bash
cat /proc/cmdline                         # inspect active boot parameters
cat /sys/devices/system/cpu/isolated      # verify isolcpus took effect
```

The CPU isolation triple — `isolcpus`, `nohz_full`, `rcu_nocbs` — works together as a unit:

1. **`isolcpus=4-7`**: removes cores 4–7 from the general scheduler pool. No normal process will be scheduled there unless explicitly pinned.
2. **`nohz_full=4-7`**: stops the 250 Hz scheduler tick interrupt on those cores. Without this, the tick fires 250 times per second even on isolated cores, causing jitter.
3. **`rcu_nocbs=4-7`**: moves RCU (Read-Copy-Update) grace-period callbacks off isolated cores. Without this, occasional RCU work fires on the isolated core.

All three together give inference threads essentially interrupt-free CPU time.

> **Key Insight:** `isolcpus` alone is not sufficient for real-time isolation. Without `nohz_full`, the scheduler tick fires 250 Hz on the isolated core, adding 4 ms periodic jitter. Without `rcu_nocbs`, RCU callbacks fire occasionally — typically a few microseconds, but at unpredictable intervals. The combination of all three eliminates the kernel's involuntary intrusions into the isolated core's execution.

> **Common Pitfall:** Changes to the kernel command line on Jetson require modifying `extlinux.conf` (for CBoot/extlinux) or the UEFI boot variables (for JetPack 6 UEFI). Editing `/proc/cmdline` has no effect — it is read-only. After changing `extlinux.conf`, verify with `cat /proc/cmdline` after reboot that the parameters actually took effect.

---

**Summary**

| Boot stage | Responsible | Artifact loaded | Jetson equivalent |
|---|---|---|---|
| BootROM | SoC mask ROM | Signed first-stage loader | Jetson BootROM → MB1 |
| SPL / BL2 | U-Boot SPL / TF-A | DRAM init; TF-A BL31 | MB1 → MB2 |
| Bootloader | U-Boot / CBoot | Kernel + DTB + initramfs | CBoot (JetPack 5), UEFI (JetPack 6) |
| Kernel entry | `head.S` → `start_kernel()` | Subsystem init | L4T kernel image |
| Early userspace | initramfs `/init` | Mount real rootfs | NVIDIA TNSPEC + `switch_root` |
| PID 1 | systemd | All services, udev | Jetson systemd inference target |

</details>

### 概念回顾

- **为什么启动序列有这么多阶段，而不是从 ROM 直接跳到 kernel？** 每个阶段都要初始化下一阶段所依赖的硬件（DRAM、时钟、secure monitor）。ROM 代码极小，无法初始化 DRAM；U-Boot SPL 初始化 DRAM，然后加载完整的 bootloader；完整 bootloader 拥有足够的内存，可以从存储中读取 kernel 和 DTB。每个阶段只做启用下一阶段所需的最小工作。
- **什么是设备树，为什么需要它？** 设备树是一个数据文件，描述板卡上存在哪些硬件——内存地址、总线连接、IRQ 号、时钟源。在设备树出现之前，这些信息以 `#ifdef BOARD_X` 的形式硬编码在 kernel 源码中。现在单个 kernel 二进制就能支持成千上万种不同的板卡，因为硬件描述是外部数据，而非编译进代码的内容。
- **设备树节点中的 `compatible` 属性是什么？** 它是一个字符串（或字符串列表），kernel 用它把设备节点匹配到驱动。kernel 会把 DT 节点的 `compatible` 值与每个已注册驱动的 `of_match_table` 逐一比对。一旦匹配成功，驱动的 `probe()` 函数就会被调用，并传入指向该设备节点的指针。
- **DKMS 做什么，什么时候必须用它？** 当运行中的 kernel 更新时，DKMS 会重建树外内核模块。任何不属于上游 kernel 源码的驱动都需要它——NVIDIA 专有 GPU 驱动、定制 FPGA PCIe 驱动、厂商特定的传感器驱动。没有 DKMS，一次更新 kernel 的 `apt upgrade` 就会让这些驱动失效，直到手动重建为止。
- **kernel 命令行中的 `isolcpus` 有什么作用？** 它把指定的 CPU 核心从通用调度器池中移除。普通进程不会被调度到这些核心上。绑定到隔离核心上的推理线程运行时不会受到其他进程的干扰。这是多核嵌入式 AI 平台上硬实时隔离的基础。
- **为什么在挂载真正的根文件系统之前需要 initramfs？** 真正的根文件系统可能被加密（LUKS）、位于 RAID 阵列上，或者所在设备的驱动并未编译进 kernel。initramfs 提供了一个最小环境，内含搭建真正根文件系统所需的工具（cryptsetup、mdadm、fsck），之后由 `switch_root` 把执行权移交给永久根文件系统。

---

## AI 硬件关联

- 用于 Zynq/MPSoC 的 U-Boot 会在 kernel 启动前加载 FPGA bitstream（BOOT.BIN）——当 kernel 的设备树描述 AXI 推理加速器节点时，PL（Programmable Logic）已经配置就绪，从而从启动路径中消除了固件加载延迟。
- 设备树 `compatible` 字符串是 Jetson 摄像头传感器驱动与硬件之间的绑定契约；在 DTBO 中把 `sony,imx477` 改为 `sony,imx219` 会选择不同的 V4L2 subdevice 配置，进而影响 `camerad` 采集的每一帧。
- Jetson AGX Orin 上针对 IMX477 的 DTBO overlay 允许在 runtime 配置 CSI lane 和 pin mux，无需重新烧写基础 DTB——这对载板 bring-up（上电点亮/调通）和多传感器机械臂载荷至关重要。
- 安全启动链验证（BootROM → MB1 → CBoot → kernel）是量产 AV 部署的强制要求；未经校验的 kernel 镜像会破坏 ISO 26262 ASIL 合规的安全论证，并使平台暴露于持久性 rootkit 攻击之下。
- DKMS 在开发主机上跨 kernel point release 更新管理 NVIDIA 专有 GPU 驱动——没有它，每次 kernel 更新都会打断 CUDA 初始化和 TensorRT engine 构建。
- Jetson kernel 命令行中的 `isolcpus=4-7 nohz_full=4-7 rcu_nocbs=4-7` 会在任何用户态进程启动之前，把 big 簇的 Cortex-A78AE 核心专留给 `modeld`、`camerad` 和 `controlsd`，构成实时推理 CPU 隔离的基础。


<details>
<summary>English original</summary>

**Conceptual Review**

- **Why does the boot sequence have so many stages instead of jumping directly from ROM to kernel?** Each stage initializes hardware (DRAM, clocks, secure monitor) that the next stage depends on. ROM code is tiny and cannot initialize DRAM; U-Boot SPL initializes DRAM and then loads the full bootloader; the full bootloader has enough memory to read the kernel and DTB from storage. Each stage does the minimum needed to enable the next.
- **What is the Device Tree and why was it needed?** The Device Tree is a data file describing what hardware is present on a board — memory addresses, bus connections, IRQ numbers, clock sources. Before Device Trees, this information was hardcoded as `#ifdef BOARD_X` in kernel source. A single kernel binary can now support thousands of different boards because the hardware description is external data, not compiled-in code.
- **What is the `compatible` property in a Device Tree node?** It is a string (or list of strings) that the kernel uses to match a device node to a driver. The kernel compares the DT node's `compatible` value against every registered driver's `of_match_table`. When a match is found, the driver's `probe()` function is called with a pointer to the device node.
- **What does DKMS do and when is it necessary?** DKMS rebuilds out-of-tree kernel modules when the running kernel is updated. It is necessary for any driver that is not part of the upstream kernel source — NVIDIA's proprietary GPU driver, custom FPGA PCIe drivers, vendor-specific sensor drivers. Without DKMS, a `apt upgrade` that updates the kernel breaks these drivers until manually rebuilt.
- **What is the effect of `isolcpus` in the kernel command line?** It removes specified CPU cores from the general scheduler pool. Normal processes will not be scheduled on these cores. Inference threads pinned to the isolated cores run without interference from other processes. This is the foundation of hard real-time isolation on multi-core embedded AI platforms.
- **Why is initramfs needed before mounting the real root filesystem?** The real root filesystem may be encrypted (LUKS), on a RAID array, or on a device whose driver is not built into the kernel. initramfs provides a minimal environment with the tools needed to set up the real root (cryptsetup, mdadm, fsck) before `switch_root` transfers execution to the permanent root.

---

**AI Hardware Connection**

- U-Boot for Zynq/MPSoC loads the FPGA bitstream (BOOT.BIN) before the kernel starts — the PL (Programmable Logic) is configured and ready when the kernel's Device Tree describes the AXI inference accelerator nodes, eliminating a firmware-load delay from the boot path.
- Device Tree `compatible` strings are the binding contract between Jetson camera sensor drivers and hardware; changing `sony,imx477` to `sony,imx219` in the DTBO selects a different V4L2 subdevice configuration, affecting every frame captured by `camerad`.
- DTBO overlays for IMX477 on Jetson AGX Orin allow runtime CSI lane and pin mux configuration without reflashing the base DTB — essential for carrier board bring-up and multi-sensor robot arm payloads.
- Secure boot chain verification (BootROM → MB1 → CBoot → kernel) is mandatory for production AV deployment; an unverified kernel image breaks the safety argument for ISO 26262 ASIL compliance and opens the platform to persistent rootkit attacks.
- DKMS manages the NVIDIA proprietary GPU driver on development hosts across kernel point-release updates — without it, every kernel update would break CUDA initialization and TensorRT engine builds.
- `isolcpus=4-7 nohz_full=4-7 rcu_nocbs=4-7` in the Jetson kernel command line reserves the big-cluster Cortex-A78AE cores exclusively for `modeld`, `camerad`, and `controlsd` before any userspace process starts, forming the foundation of real-time inference CPU isolation.

</details>

### openpilot（本仓库）中的真实示例

第 5 讲的主题（启动、设备树、内核模块、内核 cmdline）位于**平台**中（comma 设备上是 Agnos，Jetson 上是 L4T），而不在 openpilot 应用源码里。在 comma 设备上，该平台就是 **AGNOS** —— 为在道路上运行的 openpilot 而构建的 **fork 并定制修改的 Linux**；启动链、设备树和内核模块的**开发改动**位于 [agnos-kernel-sdm845](https://github.com/commaai/agnos-kernel-sdm845) 和 [agnos-builder](https://github.com/commaai/agnos-builder)（见阶段 5 — [AGNOS Guide — Development Changes](../../../阶段5-高级专题与专精/05-方向E-自动驾驶/02-openpilot参考栈/01-agnos/Guide.md)）。本路线图中的 openpilot 仓库与之的关联如下：

| 第 5 讲概念 | 与 openpilot 的关联方式 |
|--------------------|-----------------------------|
| **内核命令行**（`isolcpus`、`nohz_full`、`rcu_nocbs`） | 在平台的启动配置中设置（例如 Jetson 上的 `extlinux.conf`）。这些参数预留出核心，使得 openpilot 运行时只有预期的进程会使用它们。 |
| **CPU 隔离 → 用户态绑核** | 内核一旦隔离了核心（通过 cmdline），openpilot 就把线程绑定到这些核心上。**`openpilot/common/util.cc`**：`set_core_affinity(std::vector<int> cores)` 使用 `sched_setaffinity(tid, ...)`，使 `modeld`/`camerad`/`controlsd` 只在被隔离的核心上运行 —— 即本讲所述隔离机制的**用户态那一半**。 |
| **设备树 / DTBO** | 摄像头传感器节点（例如 IMX477）和 CSI 在平台的 DTB/overlay 中描述。openpilot 的 `camerad` 与那些 DT 节点所创建的 V4L2 设备通信；openpilot 应用仓库中不含任何 DTB 源码。 |
| **启动链 / 安全启动** | 在 comma 硬件上，Agnos 实现验证启动链；在 Jetson 上由 L4T/CBoot 实现。openpilot 假定内核和 rootfs 已正确启动。 |
| **内核模块** | 摄像头、CAN 和 GPU 驱动由平台加载（从 OS 镜像经 udev/modprobe）。openpilot 不随附内核模块。 |

关于 Jetson 风格技术栈上 DTB、`extlinux.conf` 和 `isolcpus` 的具体示例，见阶段 4 方向 B（Jetson/Orin）指南（例如 Orin-Nano-Real-Time-Inference、Orin-Nano-Security、Orin-Nano-Yocto-BSP-Production）。对应的用户态调度与亲和性代码，见**第 6 讲**和 **`openpilot/common/util.cc`**（`set_realtime_priority`、`set_core_affinity`）。


<details>
<summary>English original</summary>

**Real example in openpilot (this repo)**

Lecture-05 topics (boot, Device Tree, kernel modules, kernel cmdline) live in the **platform** (Agnos on comma devices, L4T on Jetson), not in the openpilot application source. On comma devices, that platform is **AGNOS** — **forked and custom-modified Linux** built for openpilot on the road; the **development changes** for boot chain, device tree, and kernel modules are in [agnos-kernel-sdm845](https://github.com/commaai/agnos-kernel-sdm845) and [agnos-builder](https://github.com/commaai/agnos-builder) (see Phase 5 — [AGNOS Guide — Development Changes](../../../阶段5-高级专题与专精/05-方向E-自动驾驶/02-openpilot参考栈/01-agnos/Guide.md)). The openpilot repo in this roadmap still ties in as follows:

| Lecture-05 concept | How it connects to openpilot |
|--------------------|-----------------------------|
| **Kernel command line** (`isolcpus`, `nohz_full`, `rcu_nocbs`) | Set in the platform’s boot config (e.g. `extlinux.conf` on Jetson). Those parameters reserve cores so that when openpilot runs, only the intended processes use them. |
| **CPU isolation → userspace pinning** | Once the kernel has isolated cores (cmdline), openpilot pins threads to those cores. **`openpilot/common/util.cc`**: `set_core_affinity(std::vector<int> cores)` uses `sched_setaffinity(tid, ...)` so `modeld`/`camerad`/`controlsd` run only on the isolated cores — the **userspace half** of the isolation described in this lecture. |
| **Device Tree / DTBO** | Camera sensor nodes (e.g. IMX477) and CSI are described in the platform DTB/overlays. openpilot’s `camerad` talks to the V4L2 devices that those DT nodes bring up; no DTB sources are in the openpilot app repo. |
| **Boot chain / secure boot** | On comma hardware, Agnos implements the verified boot chain; on Jetson, L4T/CBoot do. openpilot assumes a correctly booted kernel and rootfs. |
| **Kernel modules** | Camera, CAN, and GPU drivers are loaded by the platform (udev/modprobe from the OS image). openpilot does not ship kernel modules. |

For concrete examples of DTB, `extlinux.conf`, and `isolcpus` on a Jetson-style stack, see the Phase 4 Track B (Jetson/Orin) guides (e.g. Orin-Nano-Real-Time-Inference, Orin-Nano-Security, Orin-Nano-Yocto-BSP-Production). For the matching userspace scheduling and affinity code, see **Lecture-06** and **`openpilot/common/util.cc`** (`set_realtime_priority`, `set_core_affinity`).

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-05.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-05.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
