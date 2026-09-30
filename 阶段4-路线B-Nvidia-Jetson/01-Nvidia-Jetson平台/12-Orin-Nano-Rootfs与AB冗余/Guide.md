---
title: Orin Nano 8GB — 根文件系统与 A/B 冗余
description: Orin Nano 8GB — 根文件系统与 A/B 冗余
---

# Orin Nano 8GB — 根文件系统与 A/B 冗余

<div class="course-identity auto-course" style="--course-accent: #16a34a; --course-accent-rgb: 22, 163, 74;" markdown="1">
<div class="course-identity__icon">ON8R</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入解析 · Jetson 赛道</p>
<p class="course-identity__title">Orin Nano 8GB 的专项课程标识 — 根文件系统与 A/B 冗余。</p>
<p class="course-identity__meta">产物：Jetson 集成 demo · 测量：延迟、内存、功耗、日志</p>
</div>
</div>


> **范围：** 达到量产级理解：Jetson rootfs 架构、三种 rootfs 版本、A/B 槽位冗余、OTA 更新策略、启动校验、分区布局，以及 rootfs 如何与内存和启动链架构衔接。
>
> **前置要求：** 熟悉 [Orin Nano 启动链](../Guide.md) 和 [内存架构](../07-Orin-Nano内存架构/Guide.md)。

---


## 1. L4T 根文件系统概览

在 Jetson 上，根文件系统（rootfs）就是整棵 Linux 文件系统树：

```
/
├── bin        ← core binaries
├── lib        ← shared libraries (including NVIDIA drivers)
├── etc        ← system configuration
├── usr        ← user programs, CUDA toolkit, TensorRT
├── opt        ← NVIDIA-specific tools
├── home       ← user data
└── var        ← logs, runtime data
```

Jetson Linux（L4T — Linux for Tegra）是：

* 基于 Ubuntu（JetPack 5.x 为 20.04 focal，JetPack 6.x 为 22.04 jammy）
* 用 NVIDIA BSP（板级支持包）定制
* 包含 CUDA、cuDNN、TensorRT、多媒体栈，以及 Jetson 专用驱动
* kernel 由 NVIDIA 打补丁（并非 mainline）

rootfs 就是写入 APP 分区的内容（Dev Kit 上是 NVMe，量产模块上是 eMMC）。

---

## 2. Rootfs 生成与 BSP 结构

### BSP 目录结构

解压 L4T BSP 后，目录结构为：

```
Linux_for_Tegra/
├── bootloader/          ← MB1, MB2, UEFI, firmware blobs
├── kernel/              ← Image, DTB, modules
├── rootfs/              ← Ubuntu root filesystem (initially empty)
├── tools/
│   └── samplefs/
│       └── nv_build_samplefs.sh   ← rootfs generator
├── nv_tegra/            ← NVIDIA drivers, configs
├── flash.sh             ← flashing script
└── apply_binaries.sh    ← applies NVIDIA binaries to rootfs
```

### 构建 rootfs

```bash
# Step 1: Generate base Ubuntu rootfs
cd Linux_for_Tegra/tools/samplefs/
sudo ./nv_build_samplefs.sh --abi aarch64 --distro ubuntu --version focal

# Step 2: Copy to rootfs directory
sudo cp <generated>.tbz2 ../../rootfs/
cd ../../rootfs/
sudo tar xpf <generated>.tbz2

# Step 3: Apply NVIDIA binaries (drivers, CUDA, etc.)
cd ..
sudo ./apply_binaries.sh

# Step 4: Flash
sudo ./flash.sh jetson-orin-nano-devkit internal
```

`apply_binaries.sh` 会安装：

* `nvgpu` 内核模块
* CUDA runtime 库
* 多媒体库（nvbufsurface、nvargus 等）
* Jetson 专用 systemd 服务
* 设备树 overlay

---

## 3. 三种 rootfs 版本

NVIDIA 提供三种 rootfs 配置。选对哪一种，是量产阶段的关键决策。

### Desktop

* 完整 Ubuntu GUI（GNOME/GDM3）
* 首次启动时的 OEM 设置向导
* 包含：桌面应用、浏览器、文本编辑器、文件管理器
* 体积：约 4–6GB
* 适用场景：开发板、原型验证、demo

### Minimal

* 无 GUI —— 仅支持 SSH / UART 访问
* 核心系统工具
* 占用更小（约 1.5–2GB）
* 适用场景：机器人、工业视觉、量产边缘系统

### Basic

* 占用最小（约 800MB–1.2GB）
* 包含 Docker/容器 runtime 依赖
* 为基于容器的部署而设计（所有应用都跑在容器里）
* 适用场景：云原生边缘 AI、机群化管理的设备

### 量产该用哪一种

| Scenario                    | Recommended    | Why                                      |
|-----------------------------|----------------|------------------------------------------|
| Development / prototyping   | Desktop        | Easy setup, GUI debugging tools          |
| Single-purpose AI device    | Minimal        | Small, predictable, low attack surface   |
| Fleet of managed devices    | Basic + Docker | Containerized updates, reproducible      |
| Autonomous robot/vehicle    | Minimal        | Tight control, custom services only      |

量产 AI 边缘系统几乎总是使用 **minimal 或 basic**。Desktop rootfs 浪费存储和内存，还会扩大攻击面。

---

## 4. 面向量产的 rootfs 定制

### 移除不必要的软件包

```bash
# After generating rootfs, chroot into it
sudo mount --bind /dev rootfs/dev
sudo mount --bind /proc rootfs/proc
sudo mount --bind /sys rootfs/sys
sudo chroot rootfs

# Remove packages
apt remove --purge snapd thunderbird libreoffice-*
apt autoremove

# Exit chroot
exit
sudo umount rootfs/{dev,proc,sys}
```


<details>
<summary>English original</summary>

**Orin Nano 8GB — Root File System & A/B Redundancy**

<div class="course-identity auto-course" style="--course-accent: #16a34a; --course-accent-rgb: 22, 163, 74;" markdown="1">
<div class="course-identity__icon">ON8R</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Jetson Track</p>
<p class="course-identity__title">Specialized course identity for Orin Nano 8GB — Root File System & A/B Redundancy.</p>
<p class="course-identity__meta">Artifact: Jetson integration demo · Measure: latency, memory, power, logs</p>
</div>
</div>


> **Scope:** Production-level understanding of Jetson rootfs architecture, the three rootfs flavors, A/B slot redundancy, OTA update strategy, boot validation, partition layout, and how rootfs connects to the memory and boot chain architecture.
>
> **Prerequisites:** Familiarity with the [Orin Nano boot chain](../Guide.md) and [memory architecture](../07-Orin-Nano内存架构/Guide.md).

---


**1. L4T Root File System Overview**

On Jetson, the root file system (rootfs) is the entire Linux filesystem tree:

```
/
├── bin        ← core binaries
├── lib        ← shared libraries (including NVIDIA drivers)
├── etc        ← system configuration
├── usr        ← user programs, CUDA toolkit, TensorRT
├── opt        ← NVIDIA-specific tools
├── home       ← user data
└── var        ← logs, runtime data
```

Jetson Linux (L4T — Linux for Tegra) is:

* Based on Ubuntu (20.04 focal for JetPack 5.x, 22.04 jammy for JetPack 6.x)
* Customized with NVIDIA BSP (Board Support Package)
* Includes CUDA, cuDNN, TensorRT, multimedia stack, and Jetson-specific drivers
* Kernel is NVIDIA-patched (not mainline)

The rootfs is what gets written to the APP partition (NVMe on Dev Kit, eMMC on production modules).

---

**2. Rootfs Generation and BSP Structure**

**BSP Directory Layout**

After extracting the L4T BSP, the directory structure is:

```
Linux_for_Tegra/
├── bootloader/          ← MB1, MB2, UEFI, firmware blobs
├── kernel/              ← Image, DTB, modules
├── rootfs/              ← Ubuntu root filesystem (initially empty)
├── tools/
│   └── samplefs/
│       └── nv_build_samplefs.sh   ← rootfs generator
├── nv_tegra/            ← NVIDIA drivers, configs
├── flash.sh             ← flashing script
└── apply_binaries.sh    ← applies NVIDIA binaries to rootfs
```

**Building a Rootfs**

```bash
# Step 1: Generate base Ubuntu rootfs
cd Linux_for_Tegra/tools/samplefs/
sudo ./nv_build_samplefs.sh --abi aarch64 --distro ubuntu --version focal

# Step 2: Copy to rootfs directory
sudo cp <generated>.tbz2 ../../rootfs/
cd ../../rootfs/
sudo tar xpf <generated>.tbz2

# Step 3: Apply NVIDIA binaries (drivers, CUDA, etc.)
cd ..
sudo ./apply_binaries.sh

# Step 4: Flash
sudo ./flash.sh jetson-orin-nano-devkit internal
```

`apply_binaries.sh` installs:

* `nvgpu` kernel module
* CUDA runtime libraries
* Multimedia libraries (nvbufsurface, nvargus, etc.)
* Jetson-specific systemd services
* Device tree overlays

---

**3. The Three Rootfs Flavors**

NVIDIA provides three rootfs configurations. Choosing the right one is a critical production decision.

**Desktop**

* Full Ubuntu GUI (GNOME/GDM3)
* OEM setup wizard on first boot
* Includes: desktop apps, browser, text editor, file manager
* Size: ~4–6GB
* Use case: development boards, prototyping, demos

**Minimal**

* No GUI — SSH / UART access only
* Core system utilities
* Smaller footprint (~1.5–2GB)
* Use case: robotics, industrial vision, production edge systems

**Basic**

* Smallest footprint (~800MB–1.2GB)
* Includes Docker/container runtime dependencies
* Designed for container-based deployment (all apps run in containers)
* Use case: cloud-native edge AI, fleet-managed devices

**Which to Use in Production**

| Scenario                    | Recommended    | Why                                      |
|-----------------------------|----------------|------------------------------------------|
| Development / prototyping   | Desktop        | Easy setup, GUI debugging tools          |
| Single-purpose AI device    | Minimal        | Small, predictable, low attack surface   |
| Fleet of managed devices    | Basic + Docker | Containerized updates, reproducible      |
| Autonomous robot/vehicle    | Minimal        | Tight control, custom services only      |

Production AI edge systems almost always use **minimal or basic**. Desktop rootfs wastes storage, memory, and increases attack surface.

---

**4. Rootfs Customization for Production**

**Removing Unnecessary Packages**

```bash
# After generating rootfs, chroot into it
sudo mount --bind /dev rootfs/dev
sudo mount --bind /proc rootfs/proc
sudo mount --bind /sys rootfs/sys
sudo chroot rootfs

# Remove packages
apt remove --purge snapd thunderbird libreoffice-*
apt autoremove

# Exit chroot
exit
sudo umount rootfs/{dev,proc,sys}
```

</details>

### 添加生产用软件包

边缘 AI 系统的常见补充项：

```bash
# Inside chroot
apt install -y \
    openssh-server \
    python3-pip \
    docker.io \
    chrony \           # time sync (critical for sensor fusion)
    watchdog \         # hardware watchdog
    logrotate          # prevent log storage exhaustion
```

### 只读 rootfs

为获得最高可靠性，将 rootfs 设为只读，并用 overlay 承载可写数据：

```
/dev/nvme0n1p1 (APP)  → mount read-only at /
tmpfs                 → overlay for /tmp, /var/run
/dev/nvme0n1p2 (data) → mount read-write at /data
```

优点：

* 意外断电后不会出现文件系统损坏
* 防止日志不断累积撑满存储
* 强制将系统数据与应用程序数据清晰分离

---

## 5. Orin Nano 上的分区布局

### 默认布局（无 A/B）

```
QSPI NOR Flash:
  mb1, mb2, uefi, firmware blobs, BCT, boot config

NVMe / eMMC:
  APP          → rootfs (ext4)
  [optional]   → data partition
```

### 启用 A/B 后的布局

```
QSPI NOR Flash:
  mb1       mb1_b
  mb2       mb2_b
  uefi      uefi_b
  BCT       BCT_b
  (each firmware blob duplicated)

NVMe / eMMC:
  APP       → rootfs A (ext4)
  APP_b     → rootfs B (ext4)
  kernel    → kernel A
  kernel_b  → kernel B
  kernel-dtb   → DTB A
  kernel-dtb_b → DTB B
```

每个组件都被复制 — bootloader、kernel、DTB 与 rootfs。这确保了任一组件损坏时都有完整的回退。

### 查看当前分区布局

```bash
# On a running Jetson
lsblk
sudo fdisk -l /dev/nvme0n1

# Or check flash layout XML before flashing
cat Linux_for_Tegra/bootloader/generic/cfg/flash_t234_qspi.xml
```

---

## 6. A/B rootfs 冗余

### 它为何存在

设想在现场部署数百台 AI 边缘设备 — 交通摄像头、工业检测系统、自主机器人。如果一次 OTA 更新弄坏了系统，且没有冗余：

* 设备变砖
* 必须到现场重新烧录
* 停机、上门服务、影响客户

有了 A/B 冗余：

* 更新写入 **非活动槽位**
* 系统重启进入已更新的槽位
* 若启动成功，该槽位被标记为正常
* 若启动失败，**自动回滚**到上一个槽位

这与汽车 ECU（ISO 26262）、Android 设备以及工业 PLC 采用的方法相同。

### 槽位如何配对

每个槽位都是一套完整的可启动系统：

```
Slot A:                           Slot B:
  Bootloader A (MB1, MB2, UEFI)    Bootloader B
  Kernel A                          Kernel B
  DTB A                             DTB B
  Rootfs A (APP)                    Rootfs B (APP_b)
```

配对至关重要 — 不能把 Slot A 的 bootloader 与 Slot B 的 rootfs 混用。这会导致驱动不匹配、模块加载失败，甚至系统无法启动。

### 启用 A/B

在烧录前于烧录配置中设置：

```bash
# In flash environment
export ROOTFS_AB=1
sudo ./flash.sh jetson-orin-nano-devkit internal
```

烧录后无法再启用 — 分区表必须在最初创建时就包含双槽位。

---

## 7. 启用 A/B 后的启动流程

### 正常启动（Slot A 活动）

```
Power On
 ↓
BootROM (reads fuses, selects boot media)
 ↓
MB1 (Slot A — DRAM training, power rails)
 ↓
MB2 (Slot A — loads UEFI A)
 ↓
UEFI (Slot A — checks slot state, loads kernel A)
 ↓
Kernel A (from kernel partition A)
 ↓
Mounts rootfs A (APP partition)
 ↓
systemd → validation services → mark slot A successful
```

### 启动失败恢复

如果 Slot A 启动失败（kernel panic、挂起、校验失败）：

```
Boot attempt 1 → Slot A → FAIL
Boot attempt 2 → Slot A → FAIL
Boot attempt 3 → Slot A → FAIL (retry count exhausted)
 ↓
cpu-bootloader marks Slot A invalid
 ↓
Switch to Slot B
 ↓
MB1 B → MB2 B → UEFI B → Kernel B → Rootfs B
 ↓
System boots from Slot B (rollback complete)
```

### 两个槽位都失败

如果 Slot A 与 Slot B 都启动失败：

```
Both slots exhausted
 ↓
Recovery kernel (if configured)
 ↓
Minimal recovery environment
 ↓
Requires manual intervention (reflash via USB)
```

这正是生产系统必须具备健壮校验的原因 — 目标是捕获 Slot A 的故障并回滚到已知可用的 Slot B，而不是把两个槽位都耗尽。

---

## 8. 槽位状态与 nvbootctrl

### 槽位属性

每个槽位有四个属性：

| Attribute     | Meaning                                           |
|---------------|---------------------------------------------------|
| **Active**    | UEFI 下次重启将启动的槽位                         |
| **Current**   | 当前正在运行的槽位                                |
| **Bootable**  | 该槽位包含有效的 OS 镜像                          |
| **Retry count** | 标记为失败前剩余的启动尝试次数                  |


<details>
<summary>English original</summary>

**Adding Production Packages**

Common additions for edge AI systems:

```bash
# Inside chroot
apt install -y \
    openssh-server \
    python3-pip \
    docker.io \
    chrony \           # time sync (critical for sensor fusion)
    watchdog \         # hardware watchdog
    logrotate          # prevent log storage exhaustion
```

**Read-Only Rootfs**

For maximum reliability, make rootfs read-only with an overlay for writable data:

```
/dev/nvme0n1p1 (APP)  → mount read-only at /
tmpfs                 → overlay for /tmp, /var/run
/dev/nvme0n1p2 (data) → mount read-write at /data
```

Benefits:

* Survives unexpected power loss without filesystem corruption
* Prevents log accumulation from filling storage
* Forces clean separation of system vs. application data

---

**5. Partition Layout on Orin Nano**

**Default Layout (No A/B)**

```
QSPI NOR Flash:
  mb1, mb2, uefi, firmware blobs, BCT, boot config

NVMe / eMMC:
  APP          → rootfs (ext4)
  [optional]   → data partition
```

**Layout With A/B Enabled**

```
QSPI NOR Flash:
  mb1       mb1_b
  mb2       mb2_b
  uefi      uefi_b
  BCT       BCT_b
  (each firmware blob duplicated)

NVMe / eMMC:
  APP       → rootfs A (ext4)
  APP_b     → rootfs B (ext4)
  kernel    → kernel A
  kernel_b  → kernel B
  kernel-dtb   → DTB A
  kernel-dtb_b → DTB B
```

Every component is duplicated — bootloader, kernel, DTB, and rootfs. This ensures a complete fallback if any single component is corrupted.

**Viewing Current Partition Layout**

```bash
# On a running Jetson
lsblk
sudo fdisk -l /dev/nvme0n1

# Or check flash layout XML before flashing
cat Linux_for_Tegra/bootloader/generic/cfg/flash_t234_qspi.xml
```

---

**6. A/B Rootfs Redundancy**

**Why It Exists**

Imagine deploying hundreds of AI edge devices in the field — traffic cameras, industrial inspection systems, autonomous robots. If an OTA update breaks the system and there is no redundancy:

* Device is bricked
* Physical access required to reflash
* Downtime, truck rolls, customer impact

With A/B redundancy:

* Update is written to the **inactive slot**
* System reboots into the updated slot
* If boot succeeds, the slot is marked good
* If boot fails, **automatic rollback** to the previous slot

This is the same approach used in automotive ECUs (ISO 26262), Android devices, and industrial PLCs.

**How Slots Are Paired**

Each slot is a complete bootable system:

```
Slot A:                           Slot B:
  Bootloader A (MB1, MB2, UEFI)    Bootloader B
  Kernel A                          Kernel B
  DTB A                             DTB B
  Rootfs A (APP)                    Rootfs B (APP_b)
```

The pairing is critical — you cannot mix Slot A bootloader with Slot B rootfs. This would cause driver mismatches, module load failures, and potentially a non-booting system.

**Enabling A/B**

Set in the flash configuration before flashing:

```bash
# In flash environment
export ROOTFS_AB=1
sudo ./flash.sh jetson-orin-nano-devkit internal
```

This cannot be enabled after flashing — the partition table must be created with dual slots from the start.

---

**7. Boot Flow With A/B Enabled**

**Normal Boot (Slot A Active)**

```
Power On
 ↓
BootROM (reads fuses, selects boot media)
 ↓
MB1 (Slot A — DRAM training, power rails)
 ↓
MB2 (Slot A — loads UEFI A)
 ↓
UEFI (Slot A — checks slot state, loads kernel A)
 ↓
Kernel A (from kernel partition A)
 ↓
Mounts rootfs A (APP partition)
 ↓
systemd → validation services → mark slot A successful
```

**Boot Failure Recovery**

If Slot A fails to boot (kernel panic, hang, validation failure):

```
Boot attempt 1 → Slot A → FAIL
Boot attempt 2 → Slot A → FAIL
Boot attempt 3 → Slot A → FAIL (retry count exhausted)
 ↓
cpu-bootloader marks Slot A invalid
 ↓
Switch to Slot B
 ↓
MB1 B → MB2 B → UEFI B → Kernel B → Rootfs B
 ↓
System boots from Slot B (rollback complete)
```

**If Both Slots Fail**

If both Slot A and Slot B fail to boot:

```
Both slots exhausted
 ↓
Recovery kernel (if configured)
 ↓
Minimal recovery environment
 ↓
Requires manual intervention (reflash via USB)
```

This is why production systems must have robust validation — you want to catch failures in Slot A and roll back to a known-good Slot B, not exhaust both slots.

---

**8. Slot States and nvbootctrl**

**Slot Attributes**

Each slot has four attributes:

| Attribute     | Meaning                                           |
|---------------|---------------------------------------------------|
| **Active**    | The slot UEFI will boot on next reboot            |
| **Current**   | The slot currently running                        |
| **Bootable**  | The slot contains a valid OS image                |
| **Retry count** | Remaining boot attempts before marking failed  |

</details>

### nvbootctrl 命令

```bash
# Dump current slot information
sudo nvbootctrl -t rootfs dump-slots-info

# Example output:
# Current slot: A
# Slot A:
#   Priority: 15
#   Suffix: _a
#   Retry count: 7
#   Boot successful: 1
# Slot B:
#   Priority: 14
#   Suffix: _b
#   Retry count: 7
#   Boot successful: 1

# Check which slot is currently active
sudo nvbootctrl -t rootfs get-current-slot

# Set Slot B as the next boot target
sudo nvbootctrl -t rootfs set-active-boot-slot 1

# Mark current slot as successfully booted
sudo nvbootctrl -t rootfs mark-boot-successful
```

`-t rootfs` 标志针对的是 rootfs 分区表。不带该标志时，`nvbootctrl` 作用于 bootloader 分区表。

---

## 9. 启动校验服务

启动后有两个关键的 systemd 服务运行，用于校验 slot。理解它们可以避免一个非常常见的生产错误。

### nv-l4tbootloader-config.service

* 在启动流程早期运行
* 调用 `nvbootctrl verify` 校验 bootloader 完整性
* 若校验通过，将该 slot 标记为启动成功

### l4t-rootfs-validation-config.service

* 自定义校验钩子 —— 在此添加自己的健康检查
* 默认：最小检查（文件系统已挂载、基础服务在运行）
* 生产：添加应用相关的检查（推理引擎启动、摄像头打开、网络连通）

### 常见的生产错误

若在校验服务完成前重启：

1. 系统启动进入 Slot A
2. 校验服务启动，但尚未完成
3. 重启（手动或 watchdog）
4. 重试次数递减
5. 重启 3 次后，系统将 Slot A 标记为失败
6. 切换到 Slot B（非预期的回滚）

预防措施：

* 确保任何重启之前校验服务已完成
* 将重试次数设为足以覆盖启动时间
* 在调用 `mark-boot-successful` 之前添加显式健康检查

### 自定义校验示例

创建生产校验脚本：

```bash
#!/bin/bash
# /opt/nvidia/validation/validate.sh

# Check CUDA is functional
nvidia-smi > /dev/null 2>&1 || exit 1

# Check camera opens
v4l2-ctl --device=/dev/video0 --all > /dev/null 2>&1 || exit 1

# Check inference engine
python3 -c "import tensorrt" > /dev/null 2>&1 || exit 1

# Check network connectivity
ping -c 1 -W 5 8.8.8.8 > /dev/null 2>&1 || exit 1

# All checks passed — mark boot successful
nvbootctrl -t rootfs mark-boot-successful
```

将其接入 `l4t-rootfs-validation-config.service`，这样只有当应用栈确认正常工作时，该 slot 才会被标记为可用。

---

## 10. 启用 A/B 时的分区大小

启用 A/B 时：

```
ROOTFS_AB=1
```

rootfs 分区被一分为二：

```
ROOTFSSIZE / 2
```

示例：

| ROOTFSSIZE | APP (Slot A) | APP_b (Slot B) |
|------------|--------------|----------------|
| 28 GiB     | 14 GiB       | 14 GiB         |
| 56 GiB     | 28 GiB       | 28 GiB         |
| 128 GiB    | 64 GiB       | 64 GiB         |

### 存储规划错误

如果忘了 A/B 会将 rootfs 减半：

* 运行过程中 rootfs 被写满
* Docker 镜像拉取失败
* OTA 更新失败（没有空间存放新镜像）
* 日志文件耗尽剩余空间
* 系统变得不稳定

### 生产存储预算（14 GiB Slot 示例）

| 组件                          | 大小      |
|-------------------------------|-----------|
| 最小 rootfs（L4T + NVIDIA）   | ~2 GB     |
| CUDA + TensorRT 库            | ~2 GB     |
| 应用代码                      | ~500 MB   |
| TensorRT 引擎                 | ~500 MB   |
| Docker 镜像（如使用）         | ~2–4 GB   |
| 日志 + 临时文件（含轮转）     | ~1 GB     |
| **空闲空间缓冲**              | **~4–6 GB** |

使用**日志轮转**、**将 /tmp 挂为 tmpfs**以及**最小 rootfs** 来最大化可用空间。

---

## 11. 基于 UUID 的分区挂载

### 为什么 A/B 下必须使用 UUID

启用 A/B 时，设备名不可靠：

```
/dev/nvme0n1p1   ← Could be Slot A or Slot B depending on layout
```

内核命令行和 fstab 必须使用 **UUID** 来标识正确的 rootfs：

```
root=UUID=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
```

### UUID 存储在哪里

Jetson 将分区 UUID 存储在 bootloader 目录中：

```
Linux_for_Tegra/bootloader/l4t-rootfs-uuid.txt      ← Slot A UUID
Linux_for_Tegra/bootloader/l4t-rootfs-uuid.txt_b     ← Slot B UUID
```

UEFI 在启动时读取这些内容，以便将正确的 `root=UUID=` 参数传给内核。

### 查看当前 UUID

```bash
# On a running system
blkid
lsblk -f

# Find the root partition UUID
findmnt / -o UUID
```

### UUID 错误会怎样

* 内核挂载 rootfs 失败
* 启动落入 initramfs 应急 shell
* 若重试次数耗尽，系统切换到另一个 slot
* 若两个 UUID 都错误，系统无法启动

---

## 12. A/B 下的 OTA 更新策略


<details>
<summary>English original</summary>

**nvbootctrl Commands**

```bash
# Dump current slot information
sudo nvbootctrl -t rootfs dump-slots-info

# Example output:
# Current slot: A
# Slot A:
#   Priority: 15
#   Suffix: _a
#   Retry count: 7
#   Boot successful: 1
# Slot B:
#   Priority: 14
#   Suffix: _b
#   Retry count: 7
#   Boot successful: 1

# Check which slot is currently active
sudo nvbootctrl -t rootfs get-current-slot

# Set Slot B as the next boot target
sudo nvbootctrl -t rootfs set-active-boot-slot 1

# Mark current slot as successfully booted
sudo nvbootctrl -t rootfs mark-boot-successful
```

The `-t rootfs` flag targets the rootfs partition table. Without it, `nvbootctrl` operates on the bootloader partition table.

---

**9. Boot Validation Services**

Two critical systemd services run after boot to validate the slot. Understanding these prevents a very common production mistake.

**nv-l4tbootloader-config.service**

* Runs early in the boot process
* Calls `nvbootctrl verify` to validate bootloader integrity
* Marks the slot as successfully booted if verification passes

**l4t-rootfs-validation-config.service**

* Custom validation hook — you add your own health checks here
* Default: minimal checks (filesystem mounted, basic services running)
* Production: add application-specific checks (inference engine starts, camera opens, network connects)

**The Common Production Mistake**

If you reboot before validation services complete:

1. System boots into Slot A
2. Validation service starts but hasn't finished
3. You reboot (manual or watchdog)
4. Retry count decrements
5. After 3 reboots, system marks Slot A as failed
6. Switches to Slot B (unintended rollback)

Prevention:

* Ensure validation services complete before any reboot
* Set retry count high enough for your boot time
* Add explicit health checks before calling `mark-boot-successful`

**Custom Validation Example**

Create a production validation script:

```bash
#!/bin/bash
# /opt/nvidia/validation/validate.sh

# Check CUDA is functional
nvidia-smi > /dev/null 2>&1 || exit 1

# Check camera opens
v4l2-ctl --device=/dev/video0 --all > /dev/null 2>&1 || exit 1

# Check inference engine
python3 -c "import tensorrt" > /dev/null 2>&1 || exit 1

# Check network connectivity
ping -c 1 -W 5 8.8.8.8 > /dev/null 2>&1 || exit 1

# All checks passed — mark boot successful
nvbootctrl -t rootfs mark-boot-successful
```

Wire this into `l4t-rootfs-validation-config.service` so the slot is only marked good when your application stack is confirmed working.

---

**10. Partition Size With A/B Enabled**

When A/B is enabled:

```
ROOTFS_AB=1
```

The rootfs partition is split in half:

```
ROOTFSSIZE / 2
```

Example:

| ROOTFSSIZE | APP (Slot A) | APP_b (Slot B) |
|------------|--------------|----------------|
| 28 GiB     | 14 GiB       | 14 GiB         |
| 56 GiB     | 28 GiB       | 28 GiB         |
| 128 GiB    | 64 GiB       | 64 GiB         |

**Storage Planning Mistakes**

If you forget that A/B halves your rootfs:

* Rootfs fills up during operation
* Docker images fail to pull
* OTA updates fail (no space for new image)
* Log files exhaust remaining space
* System becomes unstable

**Production Storage Budget (14 GiB Slot Example)**

| Component                     | Size      |
|-------------------------------|-----------|
| Minimal rootfs (L4T + NVIDIA) | ~2 GB     |
| CUDA + TensorRT libraries     | ~2 GB     |
| Application code              | ~500 MB   |
| TensorRT engines              | ~500 MB   |
| Docker images (if used)       | ~2–4 GB   |
| Logs + temp (with rotation)   | ~1 GB     |
| **Free space buffer**         | **~4–6 GB** |

Use **log rotation**, **tmpfs for /tmp**, and **minimal rootfs** to maximize available space.

---

**11. UUID-Based Partition Mounting**

**Why UUID Is Mandatory With A/B**

When A/B is enabled, device names are unreliable:

```
/dev/nvme0n1p1   ← Could be Slot A or Slot B depending on layout
```

The kernel command line and fstab must use **UUID** to identify the correct rootfs:

```
root=UUID=xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
```

**Where UUIDs Are Stored**

Jetson stores partition UUIDs in the bootloader directory:

```
Linux_for_Tegra/bootloader/l4t-rootfs-uuid.txt      ← Slot A UUID
Linux_for_Tegra/bootloader/l4t-rootfs-uuid.txt_b     ← Slot B UUID
```

UEFI reads these during boot to pass the correct `root=UUID=` parameter to the kernel.

**Checking Current UUID**

```bash
# On a running system
blkid
lsblk -f

# Find the root partition UUID
findmnt / -o UUID
```

**What Happens If UUID Is Wrong**

* Kernel fails to mount rootfs
* Boot drops to initramfs emergency shell
* If retry count exhausts, system switches to other slot
* If both UUIDs wrong, system is unbootable

---

**12. OTA Update Strategy With A/B**

</details>

### 正确的 OTA 流程

```
1. Device running on Slot A (current, verified)
2. Download new system image
3. Write new image to Slot B (inactive)
4. Update Slot B bootloader, kernel, DTB
5. Set Slot B as active: nvbootctrl -t rootfs set-active-boot-slot 1
6. Reboot
7. System boots into Slot B
8. Validation services run health checks
9. Mark Slot B successful: nvbootctrl -t rootfs mark-boot-successful
10. Slot B is now the running, verified slot
```

如果启动到 Slot B 失败：

```
Retry count exhausts → automatic rollback to Slot A
Device continues running on previous known-good image
```

### 不要做什么

**`apt upgrade` 不支持 A/B。**

Debian 软件包升级会原地修改正在运行的 rootfs。这会：

* 只更新活动 slot
* 使非活动 slot 保持陈旧
* 破坏 A/B 不变式（两个 slot 应都是完整、独立的镜像）
* 可能让系统处于不一致状态

使用**基于镜像的 OTA** —— 将完整的 rootfs 镜像写入非活动 slot。

### OTA 方法

| 方法                      | 优点                          | 缺点                         |
|---------------------------|-------------------------------|------------------------------|
| NVIDIA OTA tools          | 官方、经过测试                | NVIDIA 生态锁定              |
| 自定义基于镜像            | 完全可控                      | 工程投入更大                 |
| Mender / SWUpdate / RAUC  | 开源、fleet 管理              | 与 L4T 集成需投入            |
| 基于容器的更新            | 快、无需改动 rootfs           | 仍需要基础 OS 更新           |

### 基于容器的 OTA（fleet 场景推荐）

对于 fleet 管理的设备，混合方案效果很好：

1. **基础 rootfs**：Minimal 或 Basic，很少更新（每季度）
2. **应用**：运行在 Docker 容器中，频繁更新
3. **A/B**：在罕见的 OS 更新期间保护基础 rootfs
4. **容器镜像仓库**：按计划拉取新的推理容器

这样将 A/B 分区切换降到最少，同时保持应用更新快速且安全。

---

## 13. Flash XML 布局文件

分区布局定义在 XML 文件中，`flash.sh` 在刷写时读取这些文件。

### 关键布局文件

```
Linux_for_Tegra/bootloader/generic/cfg/
├── flash_t234_qspi.xml        ← QSPI NOR (bootloader partitions)
└── flash_t234_nvme.xml        ← NVMe (rootfs, kernel, DTB)
```

### 分区条目示例（简化）

```xml
<partition name="APP" type="data">
    <allocation_policy> sequential </allocation_policy>
    <filesystem_type> basic </filesystem_type>
    <size> 30064771072 </size>    <!-- 28 GiB in bytes -->
    <file_system_attribute> 0 </file_system_attribute>
    <allocation_attribute> 0x8 </allocation_attribute>
    <filename> system.img </filename>
</partition>
```

使用 `ROOTFS_AB=1` 时，布局文件被修改以包含：

```xml
<partition name="APP" ...>
    <size> 15032385536 </size>    <!-- 14 GiB -->
    <filename> system.img </filename>
</partition>
<partition name="APP_b" ...>
    <size> 15032385536 </size>    <!-- 14 GiB -->
    <filename> system.img </filename>
</partition>
```

### 自定义分区布局

要添加持久化数据分区：

```xml
<partition name="UDA" type="data">
    <allocation_policy> sequential </allocation_policy>
    <filesystem_type> basic </filesystem_type>
    <size> 10737418240 </size>    <!-- 10 GiB -->
    <filename> uda.img </filename>
</partition>
```

这会创建一个在 A/B slot 切换和 OTA 更新后仍然存在的数据分区 —— 用它存放模型、配置和应用数据。

### 应用自定义布局

```bash
# Flash with custom layout
sudo ROOTFS_AB=1 ./flash.sh -c my_custom_layout.xml jetson-orin-nano-devkit internal
```

---

## 14. Rootfs 与内存架构的关联

Rootfs 与 A/B slot 会以重要方式与内存架构相互作用。

### 每个 Slot 有自己的 kernel 和 DTB

启用 A/B 后，每个 slot 加载：

* 自己的 kernel（来自 kernel 或 kernel_b 分区的 `Image`）
* 自己的设备树（`kernel-dtb` 或 `kernel-dtb_b`）
* 自己的内核模块（来自 rootfs `/lib/modules/`）

如果这些在 slot 之间不匹配，会出现：

| 不匹配                          | 症状                                 |
|---------------------------------|--------------------------------------|
| Kernel 版本 != 模块版本         | `modprobe: FATAL: Module not found`  |
| DTB != kernel                    | 内存 carveout 错误、启动崩溃         |
| 旧 NVIDIA 驱动配新 kernel        | GPU 驱动加载失败、无 CUDA           |
| CMA 配置不匹配                  | 摄像头缓冲区分配失败                  |

始终将 kernel + DTB + rootfs + 模块作为一个完整的原子单元，对每个 slot 一起更新。


<details>
<summary>English original</summary>

**Correct OTA Flow**

```
1. Device running on Slot A (current, verified)
2. Download new system image
3. Write new image to Slot B (inactive)
4. Update Slot B bootloader, kernel, DTB
5. Set Slot B as active: nvbootctrl -t rootfs set-active-boot-slot 1
6. Reboot
7. System boots into Slot B
8. Validation services run health checks
9. Mark Slot B successful: nvbootctrl -t rootfs mark-boot-successful
10. Slot B is now the running, verified slot
```

If boot into Slot B fails:

```
Retry count exhausts → automatic rollback to Slot A
Device continues running on previous known-good image
```

**What NOT to Do**

**`apt upgrade` is NOT supported with A/B.**

Debian package upgrades modify the running rootfs in place. This:

* Only updates the active slot
* Leaves the inactive slot stale
* Breaks the A/B invariant (both slots should be complete, independent images)
* Can leave the system in an inconsistent state

Use **image-based OTA** — write a complete rootfs image to the inactive slot.

**OTA Methods**

| Method                    | Pros                          | Cons                         |
|---------------------------|-------------------------------|------------------------------|
| NVIDIA OTA tools          | Official, tested              | NVIDIA ecosystem lock-in     |
| Custom image-based        | Full control                  | More engineering effort      |
| Mender / SWUpdate / RAUC  | Open-source, fleet management | Integration effort with L4T  |
| Container-based updates   | Fast, no rootfs change needed | Base OS updates still needed |

**Container-Based OTA (Recommended for Fleet)**

For fleet-managed devices, a hybrid approach works well:

1. **Base rootfs**: Minimal or Basic, rarely updated (quarterly)
2. **Application**: Runs in Docker containers, updated frequently
3. **A/B**: Protects the base rootfs during rare OS updates
4. **Container registry**: Pulls new inference containers on schedule

This minimizes A/B partition switches while keeping application updates fast and safe.

---

**13. Flash XML Layout Files**

The partition layout is defined in XML files that `flash.sh` reads during flashing.

**Key Layout Files**

```
Linux_for_Tegra/bootloader/generic/cfg/
├── flash_t234_qspi.xml        ← QSPI NOR (bootloader partitions)
└── flash_t234_nvme.xml        ← NVMe (rootfs, kernel, DTB)
```

**Example Partition Entry (Simplified)**

```xml
<partition name="APP" type="data">
    <allocation_policy> sequential </allocation_policy>
    <filesystem_type> basic </filesystem_type>
    <size> 30064771072 </size>    <!-- 28 GiB in bytes -->
    <file_system_attribute> 0 </file_system_attribute>
    <allocation_attribute> 0x8 </allocation_attribute>
    <filename> system.img </filename>
</partition>
```

With `ROOTFS_AB=1`, the layout file is modified to include:

```xml
<partition name="APP" ...>
    <size> 15032385536 </size>    <!-- 14 GiB -->
    <filename> system.img </filename>
</partition>
<partition name="APP_b" ...>
    <size> 15032385536 </size>    <!-- 14 GiB -->
    <filename> system.img </filename>
</partition>
```

**Customizing Partition Layout**

To add a persistent data partition:

```xml
<partition name="UDA" type="data">
    <allocation_policy> sequential </allocation_policy>
    <filesystem_type> basic </filesystem_type>
    <size> 10737418240 </size>    <!-- 10 GiB -->
    <filename> uda.img </filename>
</partition>
```

This creates a data partition that survives A/B slot switches and OTA updates — use it for models, configuration, and application data.

**Applying Custom Layout**

```bash
# Flash with custom layout
sudo ROOTFS_AB=1 ./flash.sh -c my_custom_layout.xml jetson-orin-nano-devkit internal
```

---

**14. Rootfs and Memory Architecture Connection**

Rootfs and A/B slots interact with memory architecture in important ways.

**Each Slot Has Its Own Kernel and DTB**

With A/B enabled, each slot loads:

* Its own kernel (`Image` from kernel or kernel_b partition)
* Its own device tree (`kernel-dtb` or `kernel-dtb_b`)
* Its own kernel modules (from rootfs `/lib/modules/`)

If these are mismatched between slots, you get:

| Mismatch                        | Symptom                              |
|---------------------------------|--------------------------------------|
| Kernel version != module version | `modprobe: FATAL: Module not found`  |
| DTB != kernel                    | Wrong memory carveouts, boot crash   |
| Old NVIDIA drivers with new kernel | GPU driver load failure, no CUDA   |
| Mismatched CMA config           | Camera buffer allocation failures    |

Always update kernel + DTB + rootfs + modules as a complete atomic unit per slot.

</details>

### 内存预留区依赖 DTB

内存预留区（BPMP、SPE、RCE、OP-TEE）在 DTB 中定义。如果 Slot A 与 Slot B 的 DTB 不同，且预留区配置也不同：

* 切换 slot 会改变内存映射
* 预留区错误时，固件处理器可能失效
* CMA 大小可能不同，影响相机 buffer 分配

量产系统两个 slot 应使用完全相同的 DTB，除非是有意迁移到新的内存配置。

### 内核模块与驱动栈

rootfs 在 `/lib/modules/<version>/` 下包含 NVIDIA 内核模块：

```
nvidia.ko
nvgpu.ko
nvhost-*.ko
tegra-*.ko
```

这些模块必须与运行中的内核完全匹配。带 JetPack 5.1 模块的 rootfs 无法配合 JetPack 6.0 的内核工作。

---

## 15. 为 AI 边缘设备设计安全的现场更新

### 更新架构

```
Cloud / Update Server
        ↓ (image + manifest + signature)
Device Agent (runs on Jetson)
        ↓ (verify signature)
Write to inactive slot
        ↓
Set inactive slot as active
        ↓
Reboot
        ↓
Validation services
        ↓ (pass)                    ↓ (fail)
Mark successful              Automatic rollback
        ↓
Report success to server     Report failure to server
```

### 设计原则

1. **原子更新** —— 整个 slot（bootloader + kernel + DTB + rootfs）一起更新
2. **签名验证** —— 每个镜像都必须有密码学签名；设备写入前先校验
3. **带宽效率** —— 尽可能使用增量更新（二进制 diff）；大版本使用完整镜像
4. **回滚自动化** —— 更新失败绝不要求人工介入
5. **健康检查需因应用而异** —— 默认校验不够；需加入相机、CUDA、推理与网络检查
6. **数据分区在更新中保留** —— 模型、配置与日志存放在独立分区（UDA）
7. **分批发布** —— 先更新 1% 的设备，观察后再扩大范围

### 更新失效模式与缓解措施

| Failure Mode                   | Mitigation                                    |
|--------------------------------|-----------------------------------------------|
| 写入期间掉电        | 写入的是非活动 slot；活动 slot 保持完好   |
| 下载内容损坏             | 写入前做校验和验证           |
| 新镜像无法启动         | A/B 回滚（自动）                      |
| 新镜像能启动但应用失败  | 自定义校验拒绝该 slot                 |
| 两个 slot 均损坏           | 恢复内核 + USB 重新刷写能力       |
| 存储写满                  | 下载前预检可用空间           |

---

## 16. 调试 A/B 系统中的启动循环

### 现象

* 设备反复重启
* 在 Slot A 与 Slot B 之间来回切换
* 最终停止启动（两个 slot 的重试次数都耗尽）

### 步骤 1 —— 抓取串口控制台

将 UART（115200 baud）接到 Jetson 调试排针。串口输出会显示：

```
[UEFI] Slot A: retry_count=2, boot_successful=0
[UEFI] Booting Slot A...
[kernel] ... panic ...
[UEFI] Slot A: retry_count=1
```

串口控制台不可或缺 —— 没有它就是在盲调。

### 步骤 2 —— 检查 slot 状态

如果能进入 shell（恢复模式或短暂的启动窗口）：

```bash
sudo nvbootctrl -t rootfs dump-slots-info
```

关注以下信息：

* `retry_count: 0` —— slot 的重试次数已耗尽
* `boot_successful: 0` —— slot 从未通过校验

### 步骤 3 —— 常见原因

| Cause                                | Diagnosis                                  |
|--------------------------------------|--------------------------------------------|
| Kernel panic                         | 串口日志显示 panic trace               |
| GPU 驱动加载失败             | `dmesg | grep nvgpu` 显示错误          |
| rootfs 损坏（需要 fsck）       | initramfs 落入紧急 shell         |
| 校验服务将 slot 标记为失败 | 服务日志：`journalctl -u nv-l4t*`      |
| 校验完成前看门狗重启    | 增大看门狗超时或延迟启动   |
| DTB 不匹配                         | 预留区错误，设备未探测成功        |

### 步骤 4 —— 恢复

如果两个 slot 都已耗尽：

1. 进入恢复模式（上电时按住 recovery 按键）
2. 用 USB-C 连接到主机
3. 使用 `flash.sh` 重新刷写：

```bash
sudo ./flash.sh jetson-orin-nano-devkit internal
```

### 预防

* 批量部署前，务必先在预发布环境中测试 OTA 镜像
* 重试次数设为合理值（3–7，而不是 1）
* 确保任何看门狗触发的重启之前，校验服务都已完成
* 量产硬件上保持串口控制台可访问（调试排针或测试点）

---

## 17. 量产加固检查清单

在现场部署 Jetson Orin Nano 之前：


<details>
<summary>English original</summary>

**Memory Carveouts Depend on DTB**

Memory carveouts (BPMP, SPE, RCE, OP-TEE) are defined in the DTB. If Slot A and Slot B have different DTBs with different carveout configurations:

* Switching slots changes the memory map
* Firmware processors may malfunction if carveouts are wrong
* CMA size may differ, affecting camera buffer allocation

Production systems should use identical DTBs across both slots unless intentionally migrating to a new memory configuration.

**Kernel Modules and Driver Stack**

The rootfs contains NVIDIA kernel modules under `/lib/modules/<version>/`:

```
nvidia.ko
nvgpu.ko
nvhost-*.ko
tegra-*.ko
```

These must match the running kernel exactly. A rootfs with modules from JetPack 5.1 will not work with a kernel from JetPack 6.0.

---

**15. Designing Safe Field Updates for AI Edge Devices**

**Update Architecture**

```
Cloud / Update Server
        ↓ (image + manifest + signature)
Device Agent (runs on Jetson)
        ↓ (verify signature)
Write to inactive slot
        ↓
Set inactive slot as active
        ↓
Reboot
        ↓
Validation services
        ↓ (pass)                    ↓ (fail)
Mark successful              Automatic rollback
        ↓
Report success to server     Report failure to server
```

**Design Principles**

1. **Atomic updates** — entire slot (bootloader + kernel + DTB + rootfs) is updated together
2. **Signature verification** — every image must be cryptographically signed; device verifies before writing
3. **Bandwidth efficiency** — use delta updates (binary diff) when possible; full images for major versions
4. **Rollback is automatic** — never require manual intervention for failed updates
5. **Health checks are application-specific** — default validation is insufficient; add camera, CUDA, inference, and network checks
6. **Data partition survives updates** — models, configuration, and logs live on a separate partition (UDA)
7. **Staged rollout** — update 1% of fleet first, monitor, then expand

**Update Failure Modes and Mitigations**

| Failure Mode                   | Mitigation                                    |
|--------------------------------|-----------------------------------------------|
| Power loss during write        | Inactive slot is written; active slot intact   |
| Corrupted download             | Checksum verification before writing           |
| New image doesn't boot         | A/B rollback (automatic)                      |
| New image boots but app fails  | Custom validation rejects slot                 |
| Both slots corrupted           | Recovery kernel + USB reflash capability       |
| Storage full                   | Pre-check free space before download           |

---

**16. Debugging Bootloops in A/B Systems**

**Symptoms**

* Device reboots repeatedly
* Alternates between Slot A and Slot B
* Eventually stops booting (both slots exhausted)

**Step 1 — Capture Serial Console**

Connect UART (115200 baud) to the Jetson debug header. Serial output shows:

```
[UEFI] Slot A: retry_count=2, boot_successful=0
[UEFI] Booting Slot A...
[kernel] ... panic ...
[UEFI] Slot A: retry_count=1
```

Serial console is essential — without it, you're debugging blind.

**Step 2 — Check Slot State**

If you can get to a shell (recovery or brief boot window):

```bash
sudo nvbootctrl -t rootfs dump-slots-info
```

Look for:

* `retry_count: 0` — slot has exhausted retries
* `boot_successful: 0` — slot was never validated

**Step 3 — Common Causes**

| Cause                                | Diagnosis                                  |
|--------------------------------------|--------------------------------------------|
| Kernel panic                         | Serial log shows panic trace               |
| GPU driver fails to load             | `dmesg | grep nvgpu` shows errors          |
| Rootfs corrupted (fsck needed)       | initramfs drops to emergency shell         |
| Validation service marks slot failed | Service logs: `journalctl -u nv-l4t*`      |
| Watchdog reboot before validation    | Increase watchdog timeout or defer start   |
| DTB mismatch                         | Wrong carveouts, devices not probed        |

**Step 4 — Recovery**

If both slots are exhausted:

1. Enter recovery mode (hold recovery button during power-on)
2. Connect USB-C to host machine
3. Reflash using `flash.sh`:

```bash
sudo ./flash.sh jetson-orin-nano-devkit internal
```

**Prevention**

* Always test OTA images in a staging environment before fleet deployment
* Set retry count to a reasonable value (3–7, not 1)
* Ensure validation services complete before any watchdog-triggered reboot
* Keep serial console accessible on production hardware (debug header or test points)

---

**17. Production Hardening Checklist**

Before deploying Jetson Orin Nano in the field:

</details>

### Storage

- [ ] A/B enabled with correct ROOTFSSIZE
- [ ] Separate UDA data partition for persistent storage
- [ ] Log rotation configured (prevent storage exhaustion)
- [ ] Read-only rootfs with overlay (if applicable)

### Boot and Recovery

- [ ] Custom validation service with application-specific health checks
- [ ] Retry count set appropriately (3–7)
- [ ] Recovery kernel configured
- [ ] Serial console accessible for debugging

### OTA

- [ ] Image-based OTA (not apt upgrade)
- [ ] Cryptographic signature verification on images
- [ ] Staged rollout process defined
- [ ] Rollback tested and verified

### Security

- [ ] Secure boot enabled (fuses burned)
- [ ] SSH keys deployed (password auth disabled)
- [ ] Unnecessary services removed
- [ ] Firewall configured

### Memory

- [ ] CMA sized for workload (see [Memory Architecture Guide](../07-Orin-Nano内存架构/Guide.md))
- [ ] Memory budget calculated (cameras + model + runtime + OS)
- [ ] OOM behavior tested under load

### Monitoring

- [ ] tegrastats or equivalent running
- [ ] CMA/buddyinfo logged periodically
- [ ] Thermal monitoring with alerts
- [ ] Remote health reporting to fleet management

---

## 18. References

* [NVIDIA Jetson Linux Developer Guide — Bootloader](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/SD/Bootloader/JetsonModuleBootProcess.html) — boot flow and A/B documentation
* [NVIDIA Jetson Linux Developer Guide — Root File System](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/SD/RootFileSystem.html) — rootfs generation and customization
* [NVIDIA Jetson Linux Developer Guide — OTA](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/SD/SoftwarePackagesAndTheUpdateMechanism.html) — OTA update mechanisms
* [Mender for Jetson](https://docs.mender.io/devices/nvidia-jetson) — open-source OTA for Jetson
* [SWUpdate](https://sbabic.github.io/swupdate/) — software update framework for embedded Linux
* Main guide: [Nvidia Jetson Platform Guide](../Guide.md)
* Memory deep dive: [Orin Nano Memory Architecture](../07-Orin-Nano内存架构/Guide.md)


<details>
<summary>English original</summary>

**Storage**

- [ ] A/B enabled with correct ROOTFSSIZE
- [ ] Separate UDA data partition for persistent storage
- [ ] Log rotation configured (prevent storage exhaustion)
- [ ] Read-only rootfs with overlay (if applicable)

**Boot and Recovery**

- [ ] Custom validation service with application-specific health checks
- [ ] Retry count set appropriately (3–7)
- [ ] Recovery kernel configured
- [ ] Serial console accessible for debugging

**OTA**

- [ ] Image-based OTA (not apt upgrade)
- [ ] Cryptographic signature verification on images
- [ ] Staged rollout process defined
- [ ] Rollback tested and verified

**Security**

- [ ] Secure boot enabled (fuses burned)
- [ ] SSH keys deployed (password auth disabled)
- [ ] Unnecessary services removed
- [ ] Firewall configured

**Memory**

- [ ] CMA sized for workload (see [Memory Architecture Guide](../07-Orin-Nano内存架构/Guide.md))
- [ ] Memory budget calculated (cameras + model + runtime + OS)
- [ ] OOM behavior tested under load

**Monitoring**

- [ ] tegrastats or equivalent running
- [ ] CMA/buddyinfo logged periodically
- [ ] Thermal monitoring with alerts
- [ ] Remote health reporting to fleet management

---

**18. References**

* [NVIDIA Jetson Linux Developer Guide — Bootloader](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/SD/Bootloader/JetsonModuleBootProcess.html) — boot flow and A/B documentation
* [NVIDIA Jetson Linux Developer Guide — Root File System](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/SD/RootFileSystem.html) — rootfs generation and customization
* [NVIDIA Jetson Linux Developer Guide — OTA](https://docs.nvidia.com/jetson/archives/r36.4/DeveloperGuide/SD/SoftwarePackagesAndTheUpdateMechanism.html) — OTA update mechanisms
* [Mender for Jetson](https://docs.mender.io/devices/nvidia-jetson) — open-source OTA for Jetson
* [SWUpdate](https://sbabic.github.io/swupdate/) — software update framework for embedded Linux
* Main guide: [Nvidia Jetson Platform Guide](../Guide.md)
* Memory deep dive: [Orin Nano Memory Architecture](../07-Orin-Nano内存架构/Guide.md)

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/1. Nvidia Jetson Platform/Orin-Nano-Rootfs-and-AB-Redundancy/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/1.%20Nvidia%20Jetson%20Platform/Orin-Nano-Rootfs-and-AB-Redundancy/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
