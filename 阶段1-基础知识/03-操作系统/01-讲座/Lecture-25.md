---
title: 第 25 讲：Capstone 项目 —— 用 Yocto 构建自定义 Linux 镜像
description: 第 25 讲：Capstone 项目 —— 用 Yocto 构建自定义 Linux 镜像
---

# 第 25 讲：Capstone 项目 —— 用 Yocto 构建自定义 Linux 镜像

## 概述

前 24 讲覆盖了 Linux 如何工作：进程、scheduling、内存管理、驱动、文件系统、容器与 real-time 调优。本讲作为 Capstone 项目要问的是：运行在你的 AI 硬件目标上的那个 Linux 镜像，究竟如何*构建*出来？答案是 Yocto —— **嵌入式 Linux 的业界标准构建系统**。本课程覆盖的每个平台（Jetson、openpilot Agnos、定制 FPGA 板卡、车规 ECU）都使用 Yocto 或基于 Yocto 的工具链，来产出**可复现、最小化、生产级**的 Linux 镜像。

这里要建立的心智模型是：Yocto 是一座生产 Linux 发行版的**工厂**。你描述自己想要什么（machine 硬件、软件包、文件系统布局），Yocto 就负责组装、交叉编译并打包成一个可烧写的镜像。与先装 Ubuntu 再删包不同，Yocto **从零开始** —— 只有你显式包含的内容才会出现在目标上。这**缩小了攻击面**、减小了 OTA 升级体积，并保证可复现性：同样的输入永远产出逐比特一致的输出。

本讲由两个层层递进的项目组成：
- **Project 1**：在 QEMU（x86-64）上构建一个最小的通用镜像 —— 无需硬件
- **Project 2**：使用 meta-tegra 为 **NVIDIA Jetson Orin Nano 8GB** 构建完整的 AI-ready 定制镜像

---

## Yocto 核心概念

动手构建之前，先弄清这套词汇。Yocto 的初始学习曲线很陡，但只要这五个术语打通，后面就会变平缓。

```
Yocto Conceptual Map

  poky/                          ← Reference distribution (Yocto Project)
  ├── meta/                      ← Core recipes (Linux, glibc, busybox, systemd)
  ├── meta-poky/                 ← Poky distro configuration
  ├── meta-yocto-bsp/            ← Reference BSP machines (qemux86, qemuarm64)
  │
  meta-tegra/                    ← NVIDIA Tegra BSP layer (external)
  meta-openembedded/             ← Additional packages (Python, networking tools)
  meta-my-ai-image/             ← YOUR custom layer
  │
  build/
  ├── conf/
  │   ├── local.conf             ← Your machine/distro/build settings
  │   └── bblayers.conf          ← Which layers are active
  └── tmp/                       ← Build artifacts, rootfs, kernel
```

| 术语 | 含义 |
|------|---------|
| **Layer** | 按用途（BSP、distro、应用）分组的 recipe 目录。像 Git 分支一样层层堆叠 —— 上层覆盖下层。 |
| **Recipe**（`.bb`） | 单个软件包的构建脚本：源码 URL、补丁、编译标志、安装路径。类似于 Debian 的 `.spec` 文件。 |
| **Machine** | 硬件目标定义：CPU 架构、kernel 配置、bootloader、设备树。例如 `qemux86-64`、`jetson-orin-nano-devkit`。 |
| **Distro** | 全局策略：libc（glibc/musl）、init 系统（systemd/SysV）、包格式（rpm/deb/ipk）、调试标志。 |
| **Image** | 把软件包汇集进 rootfs 与磁盘镜像的 recipe。`core-image-minimal` ≈ 20 MB；`core-image-full-cmdline` ≈ 80 MB。 |
| **BitBake** | 构建引擎。解析全部 recipe、求解依赖关系，并并行执行 fetch/compile/install/package 任务。 |

> **关键洞察：** Yocto 不下载预编译二进制 —— 它针对你的确切目标从源码交叉编译一切。这意味着你能掌控每一个编译器标志、每一个启用的特性、每一个 kernel 配置项。代价是首次构建要花 2–6 小时（它在构建一套完整的交叉工具链外加数百个软件包）；增量重建则只需几分钟。

---

## Project 1：QEMU 上的最小 x86 镜像

### 目标

为 `qemux86-64` 构建一个可启动的最小 Linux 镜像，并在 QEMU 中运行它。**不需要任何物理硬件**。这是在接触真实硬件之前，于安全环境中演练**完整 Yocto 工作流**的方式。

### 步骤 1：安装前置要求

```bash
# Ubuntu 22.04 / 24.04 host (recommended)
sudo apt-get update
sudo apt-get install -y \
    gawk wget git diffstat unzip texinfo gcc build-essential \
    chrpath socat cpio python3 python3-pip python3-pexpect \
    xz-utils debianutils iputils-ping python3-git python3-jinja2 \
    libegl1-mesa libsdl1.2-dev python3-subunit mesa-common-dev \
    zstd liblz4-tool file locales libacl1
# Ensure UTF-8 locale (BitBake requires it)
sudo locale-gen en_US.UTF-8
```

构建大约需要 **50–100 GB 可用磁盘空间**和 8 GB 内存（建议 16 GB）。BitBake 默认会跨所有可用 CPU 核并行执行。


<details>
<summary>English original</summary>

**Lecture 25: Capstone Project — Custom Linux Images with Yocto**

**Overview**

The previous 24 lectures covered how Linux works: processes, scheduling, memory management, drivers, filesystems, containers, and real-time tuning. This capstone lecture asks: how do you *build* the Linux image that runs on your AI hardware target? The answer is Yocto — the **industry-standard build system for embedded Linux**. Every platform covered in this course (Jetson, openpilot Agnos, custom FPGA boards, automotive ECUs) uses either Yocto or a Yocto-derived toolchain to produce **reproducible, minimal, production-grade** Linux images.

The mental model to carry here is that Yocto is a **factory** for Linux distributions. You describe what you want (machine hardware, packages, filesystem layout), and Yocto assembles, cross-compiles, and packages it into a flashable image. Unlike installing Ubuntu and removing packages, Yocto **starts from nothing** — only what you explicitly include ends up on the target. This **minimizes attack surface**, reduces OTA update size, and guarantees reproducibility: the same inputs always produce bit-identical outputs.

This lecture is structured as two projects that build on each other:
- **Project 1**: Build a minimal general-purpose image on QEMU (x86-64) — no hardware required
- **Project 2**: Build a full AI-ready custom image for the **NVIDIA Jetson Orin Nano 8GB** using meta-tegra

---

**Yocto Core Concepts**

Before building, understand the vocabulary. Yocto has a steep initial learning curve that flattens once these five terms click.

```
Yocto Conceptual Map

  poky/                          ← Reference distribution (Yocto Project)
  ├── meta/                      ← Core recipes (Linux, glibc, busybox, systemd)
  ├── meta-poky/                 ← Poky distro configuration
  ├── meta-yocto-bsp/            ← Reference BSP machines (qemux86, qemuarm64)
  │
  meta-tegra/                    ← NVIDIA Tegra BSP layer (external)
  meta-openembedded/             ← Additional packages (Python, networking tools)
  meta-my-ai-image/             ← YOUR custom layer
  │
  build/
  ├── conf/
  │   ├── local.conf             ← Your machine/distro/build settings
  │   └── bblayers.conf          ← Which layers are active
  └── tmp/                       ← Build artifacts, rootfs, kernel
```

| Term | Meaning |
|------|---------|
| **Layer** | A directory of recipes grouped by purpose (BSP, distro, application). Stacked like Git branches — higher layers override lower ones. |
| **Recipe** (`.bb`) | A build script for one package: source URL, patches, compile flags, install paths. Analogous to a Debian `.spec` file. |
| **Machine** | Hardware target definition: CPU arch, kernel config, bootloader, Device Tree. Examples: `qemux86-64`, `jetson-orin-nano-devkit`. |
| **Distro** | Global policy: libc (glibc/musl), init system (systemd/SysV), package format (rpm/deb/ipk), debug flags. |
| **Image** | A recipe that collects packages into a rootfs and disk image. `core-image-minimal` ≈ 20 MB; `core-image-full-cmdline` ≈ 80 MB. |
| **BitBake** | The build engine. Parses all recipes, resolves dependencies, runs fetch/compile/install/package tasks in parallel. |

> **Key Insight:** Yocto does not download pre-built binaries — it cross-compiles everything from source for your exact target. This means you control every compiler flag, every enabled feature, and every kernel config option. The trade-off is that the first build takes 2–6 hours (it is building a complete cross-toolchain plus hundreds of packages); incremental rebuilds take minutes.

---

**Project 1: Minimal x86 Image on QEMU**

**Goal**

Build a bootable minimal Linux image for `qemux86-64` and run it in QEMU. **No physical hardware needed**. This teaches the **full Yocto workflow** in a safe environment before touching real hardware.

**Step 1: Install Prerequisites**

```bash
# Ubuntu 22.04 / 24.04 host (recommended)
sudo apt-get update
sudo apt-get install -y \
    gawk wget git diffstat unzip texinfo gcc build-essential \
    chrpath socat cpio python3 python3-pip python3-pexpect \
    xz-utils debianutils iputils-ping python3-git python3-jinja2 \
    libegl1-mesa libsdl1.2-dev python3-subunit mesa-common-dev \
    zstd liblz4-tool file locales libacl1
# Ensure UTF-8 locale (BitBake requires it)
sudo locale-gen en_US.UTF-8
```

The build requires approximately **50–100 GB of free disk space** and 8 GB of RAM (16 GB recommended). BitBake parallelizes across all available CPU cores by default.

</details>

### Step 2：克隆 Poky（Yocto 参考发行版）

```bash
git clone git://git.yoctoproject.org/poky --branch scarthgap --depth 1
# scarthgap = Yocto 5.0 LTS, released April 2024, kernel 6.6 LTS
# Alternative: kirkstone (Yocto 4.0 LTS, kernel 5.15 LTS)
cd poky
```

> **关键洞察：** 始终锁定到某个具名的 LTS 版本（`scarthgap`、`kirkstone`）。`master` 分支会持续收到破坏性变更。生产系统在整个产品生命周期内都停留在同一个 LTS 版本上——就像 Linux 内核 LTS 分支一样。

### Step 3：初始化构建环境

```bash
source oe-init-build-env build-qemu
# This script:
# 1. Creates the build/ directory
# 2. Creates build/conf/local.conf (your settings file)
# 3. Creates build/conf/bblayers.conf (active layers list)
# 4. Sets PATH and environment variables for BitBake
# 5. Changes your working directory to build/
```

source 之后，你就进入了 `build-qemu/`。所有 `bitbake` 命令都从这里运行。

### Step 4：配置 local.conf

打开 `conf/local.conf`，检查/设置以下关键变量：

```bash
# The hardware target
MACHINE = "qemux86-64"

# Number of parallel compile threads (set to number of CPU cores)
BB_NUMBER_THREADS = "8"
PARALLEL_MAKE = "-j 8"

# Download cache — reused across builds (set to a persistent directory)
DL_DIR = "/opt/yocto/downloads"

# Shared state cache — speeds up rebuilds dramatically
SSTATE_DIR = "/opt/yocto/sstate-cache"

# Image features: add ssh server for remote access
EXTRA_IMAGE_FEATURES += "ssh-server-openssh"

# Disk image format for QEMU
IMAGE_FSTYPES = "ext4 wic.qcow2"
```

> **常见陷阱：** 如果你把 `DL_DIR` 和 `SSTATE_DIR` 设在另一块磁盘上，要确保该磁盘支持 POSIX 扩展属性（`xattr`）。有些文件系统（FAT32、exFAT、某些网络共享）不支持——BitBake 会因文件锁问题报出晦涩难懂的错误。

### Step 5：构建最小镜像

```bash
bitbake core-image-minimal
```

这一条命令会让 BitBake：
1. 解析所有活跃 layer 中的所有 recipe 文件
2. 解析完整的依赖图（kernel → glibc → busybox → init → image）
3. 拉取源码归档（Linux 内核、BusyBox、systemd 等）
4. 构建交叉编译工具链（`qemux86-64` 的 sysroot）
5. 交叉编译每个软件包
6. 组装 root 文件系统
7. 创建磁盘镜像（`*.ext4`、`*.wic.qcow2`）

**首次构建耗时：** 视硬件而定，1–3 小时。观察进度：

```bash
# In a separate terminal, monitor build status
bitbake -u taskexp core-image-minimal   # graphical task explorer
# Or watch the log:
tail -f tmp/log/cooker/qemux86-64/console-latest.log
```

### Step 6：在 QEMU 中运行

```bash
# Boot the image in QEMU (no external QEMU installation needed — Yocto builds its own)
runqemu qemux86-64 core-image-minimal nographic

# You should see kernel boot messages, then a login prompt
# Default login: root (no password)
```

在 QEMU VM 内验证系统：

```bash
uname -r              # should show 6.6.x kernel
cat /proc/cpuinfo     # QEMU virtual CPU
free -m               # memory
df -h                 # filesystem (tiny — this is a minimal image)
ps aux                # running processes (very few — truly minimal)
```

### Step 7：理解构建产物

```
build-qemu/
├── conf/
│   ├── local.conf                    ← your settings
│   └── bblayers.conf                 ← active layers
└── tmp/
    ├── deploy/images/qemux86-64/
    │   ├── core-image-minimal-qemux86-64.ext4    ← root filesystem
    │   ├── core-image-minimal-qemux86-64.wic     ← complete disk image
    │   ├── bzImage                               ← compressed kernel
    │   └── modules-qemux86-64.tgz                ← kernel modules
    ├── work/x86_64-linux/            ← host tools (cross-compiler)
    ├── work/qemux86_64-poky-linux/   ← target packages
    │   └── linux-yocto-6.6.*/        ← kernel build directory
    └── sysroots/qemux86-64/          ← target sysroot for SDK
```

`work/` 目录包含每个软件包解包、打补丁、编译并安装后的文件。**调试软件包构建失败时，就看这里。**

### Step 8：定制——添加软件包

要把 Python 3 加入镜像，在 `local.conf` 中添加：

```bash
IMAGE_INSTALL:append = " python3 python3-pip"
```

然后重新构建（增量——只添加新软件包）：

```bash
bitbake core-image-minimal
```

> **关键洞察：** Yocto 的共享状态（sstate）缓存会记录每个任务的输出哈希。添加一个软件包不会重新构建内核——它会拉取缓存的内核产物，只构建新软件包。增量构建很快（几秒到几分钟），这正是那 2 小时初次构建值得投入的原因。

---

## Project 2：NVIDIA Jetson Orin Nano 8GB 定制 AI 镜像


<details>
<summary>English original</summary>

**Step 2: Clone Poky (Yocto Reference Distribution)**

```bash
git clone git://git.yoctoproject.org/poky --branch scarthgap --depth 1
# scarthgap = Yocto 5.0 LTS, released April 2024, kernel 6.6 LTS
# Alternative: kirkstone (Yocto 4.0 LTS, kernel 5.15 LTS)
cd poky
```

> **Key Insight:** Always pin to a named LTS release (`scarthgap`, `kirkstone`). The `master` branch receives breaking changes continuously. Production systems stay on the same LTS release for the entire product lifetime — just like Linux kernel LTS branches.

**Step 3: Initialize the Build Environment**

```bash
source oe-init-build-env build-qemu
# This script:
# 1. Creates the build/ directory
# 2. Creates build/conf/local.conf (your settings file)
# 3. Creates build/conf/bblayers.conf (active layers list)
# 4. Sets PATH and environment variables for BitBake
# 5. Changes your working directory to build/
```

After sourcing, you are inside `build-qemu/`. All `bitbake` commands run from here.

**Step 4: Configure local.conf**

Open `conf/local.conf` and review/set these key variables:

```bash
# The hardware target
MACHINE = "qemux86-64"

# Number of parallel compile threads (set to number of CPU cores)
BB_NUMBER_THREADS = "8"
PARALLEL_MAKE = "-j 8"

# Download cache — reused across builds (set to a persistent directory)
DL_DIR = "/opt/yocto/downloads"

# Shared state cache — speeds up rebuilds dramatically
SSTATE_DIR = "/opt/yocto/sstate-cache"

# Image features: add ssh server for remote access
EXTRA_IMAGE_FEATURES += "ssh-server-openssh"

# Disk image format for QEMU
IMAGE_FSTYPES = "ext4 wic.qcow2"
```

> **Common Pitfall:** If you set `DL_DIR` and `SSTATE_DIR` on a separate disk, ensure it supports POSIX extended attributes (`xattr`). Some filesystems (FAT32, exFAT, some network shares) do not — BitBake will fail with cryptic errors about file locking.

**Step 5: Build the Minimal Image**

```bash
bitbake core-image-minimal
```

This single command triggers BitBake to:
1. Parse all recipe files in all active layers
2. Resolve the full dependency graph (kernel → glibc → busybox → init → image)
3. Fetch source archives (Linux kernel, BusyBox, systemd, etc.)
4. Build the cross-compilation toolchain (sysroot for `qemux86-64`)
5. Cross-compile every package
6. Assemble the root filesystem
7. Create the disk image (`*.ext4`, `*.wic.qcow2`)

**First build time:** 1–3 hours depending on hardware. Watch the progress:

```bash
# In a separate terminal, monitor build status
bitbake -u taskexp core-image-minimal   # graphical task explorer
# Or watch the log:
tail -f tmp/log/cooker/qemux86-64/console-latest.log
```

**Step 6: Run in QEMU**

```bash
# Boot the image in QEMU (no external QEMU installation needed — Yocto builds its own)
runqemu qemux86-64 core-image-minimal nographic

# You should see kernel boot messages, then a login prompt
# Default login: root (no password)
```

Inside the QEMU VM, verify the system:

```bash
uname -r              # should show 6.6.x kernel
cat /proc/cpuinfo     # QEMU virtual CPU
free -m               # memory
df -h                 # filesystem (tiny — this is a minimal image)
ps aux                # running processes (very few — truly minimal)
```

**Step 7: Understand the Build Artifacts**

```
build-qemu/
├── conf/
│   ├── local.conf                    ← your settings
│   └── bblayers.conf                 ← active layers
└── tmp/
    ├── deploy/images/qemux86-64/
    │   ├── core-image-minimal-qemux86-64.ext4    ← root filesystem
    │   ├── core-image-minimal-qemux86-64.wic     ← complete disk image
    │   ├── bzImage                               ← compressed kernel
    │   └── modules-qemux86-64.tgz                ← kernel modules
    ├── work/x86_64-linux/            ← host tools (cross-compiler)
    ├── work/qemux86_64-poky-linux/   ← target packages
    │   └── linux-yocto-6.6.*/        ← kernel build directory
    └── sysroots/qemux86-64/          ← target sysroot for SDK
```

The `work/` directory contains the unpacked, patched, compiled, and installed files for every package. **When debugging a package build failure, this is where you look.**

**Step 8: Customize — Add a Package**

To add Python 3 to the image, add to `local.conf`:

```bash
IMAGE_INSTALL:append = " python3 python3-pip"
```

Then rebuild (incremental — only adds the new packages):

```bash
bitbake core-image-minimal
```

> **Key Insight:** Yocto's shared state (sstate) cache records every task's output hash. Adding a package does not rebuild the kernel — it pulls the cached kernel artifact and only builds the new packages. Incremental builds are fast (seconds to minutes), which is why the 2-hour initial build is worth the investment.

---

**Project 2: NVIDIA Jetson Orin Nano 8GB Custom AI Image**

</details>

### 目标

为 Jetson Orin Nano 8GB 开发者套件构建一个生产级 AI 镜像，包含：
- L4T 36.x kernel（6.1 LTS，含 NVIDIA Tegra 补丁）
- NVIDIA GPU 驱动与 CUDA 用户态
- TensorRT 与 cuDNN
- 支持 CUDA 的 OpenCV
- systemd、SSH、Python 3

该镜像可用**可复现、可定制、精简**的 Yocto 方案替代默认的 NVIDIA JetPack 安装。已在生产环境用于边缘 AI 设备。

### 架构：meta-tegra

```
Layer Stack for Jetson Orin Nano 8GB:

meta-tegra/              ← NVIDIA Tegra BSP: kernel, U-Boot, flash tools
                            Machine: jetson-orin-nano-devkit
meta-openembedded/       ← Extra packages: OpenCV, Python libs, networking
meta-tegra-community/    ← Optional: TensorRT, CUDA recipes from community
meta-my-jetson-image/    ← YOUR layer: custom recipes, image definition
poky/meta/               ← Core (gcc, glibc, busybox, systemd)
```

**meta-tegra** 由 Open Embedded for Tegra（OE4T）社区维护。它**跟踪 NVIDIA 的 L4T 发布**，提供 kernel、U-Boot、设备树和板级驱动。

### 步骤 1：准备工作区

```bash
mkdir jetson-orin-build && cd jetson-orin-build

# Clone all required layers
git clone git://git.yoctoproject.org/poky             --branch scarthgap
git clone https://github.com/OE4T/meta-tegra          --branch scarthgap
git clone https://github.com/openembedded/meta-openembedded --branch scarthgap

# Confirm meta-tegra supports Orin Nano
ls meta-tegra/conf/machine/
# Should list: jetson-orin-nano-devkit.conf, jetson-agx-orin-devkit.conf, etc.
```

> **关键提示：** meta-tegra 的分支名对应 Yocto 发布名（`scarthgap`、`kirkstone`），而非 L4T 版本。L4T 版本在 meta-tegra 的 recipe 中设置。meta-tegra 的 Scarthgap 分支对应 L4T 36.x（JetPack 6.x）。

### 步骤 2：初始化并配置构建

```bash
source poky/oe-init-build-env build-jetson

# Edit bblayers.conf to include all layers:
cat > conf/bblayers.conf << 'EOF'
POKY_BBLAYERS_CONF_VERSION = "2"
BBPATH = "${TOPDIR}"
BBFILES ?= ""

BBLAYERS ?= " \
  ${TOPDIR}/../poky/meta \
  ${TOPDIR}/../poky/meta-poky \
  ${TOPDIR}/../poky/meta-yocto-bsp \
  ${TOPDIR}/../meta-tegra \
  ${TOPDIR}/../meta-openembedded/meta-oe \
  ${TOPDIR}/../meta-openembedded/meta-python \
  ${TOPDIR}/../meta-openembedded/meta-networking \
  "
EOF
```


<details>
<summary>English original</summary>

**Goal**

Build a production-quality AI image for the Jetson Orin Nano 8GB developer kit that includes:
- L4T 36.x kernel (6.1 LTS with NVIDIA Tegra patches)
- NVIDIA GPU drivers and CUDA userspace
- TensorRT and cuDNN
- OpenCV with CUDA support
- systemd, SSH, Python 3

This image can replace the default NVIDIA JetPack install with a **reproducible, customizable, minimal** Yocto-based alternative. Used in production for edge AI appliances.

**Architecture: meta-tegra**

```
Layer Stack for Jetson Orin Nano 8GB:

meta-tegra/              ← NVIDIA Tegra BSP: kernel, U-Boot, flash tools
                            Machine: jetson-orin-nano-devkit
meta-openembedded/       ← Extra packages: OpenCV, Python libs, networking
meta-tegra-community/    ← Optional: TensorRT, CUDA recipes from community
meta-my-jetson-image/    ← YOUR layer: custom recipes, image definition
poky/meta/               ← Core (gcc, glibc, busybox, systemd)
```

**meta-tegra** is maintained by the Open Embedded for Tegra (OE4T) community. It **tracks NVIDIA's L4T releases** and provides the kernel, U-Boot, Device Tree, and board-specific drivers.

**Step 1: Prepare the Workspace**

```bash
mkdir jetson-orin-build && cd jetson-orin-build

# Clone all required layers
git clone git://git.yoctoproject.org/poky             --branch scarthgap
git clone https://github.com/OE4T/meta-tegra          --branch scarthgap
git clone https://github.com/openembedded/meta-openembedded --branch scarthgap

# Confirm meta-tegra supports Orin Nano
ls meta-tegra/conf/machine/
# Should list: jetson-orin-nano-devkit.conf, jetson-agx-orin-devkit.conf, etc.
```

> **Key Insight:** meta-tegra branch names track Yocto release names (`scarthgap`, `kirkstone`), not L4T versions. The L4T version is set inside meta-tegra's recipes. Scarthgap branch of meta-tegra corresponds to L4T 36.x (JetPack 6.x).

**Step 2: Initialize and Configure the Build**

```bash
source poky/oe-init-build-env build-jetson

# Edit bblayers.conf to include all layers:
cat > conf/bblayers.conf << 'EOF'
POKY_BBLAYERS_CONF_VERSION = "2"
BBPATH = "${TOPDIR}"
BBFILES ?= ""

BBLAYERS ?= " \
  ${TOPDIR}/../poky/meta \
  ${TOPDIR}/../poky/meta-poky \
  ${TOPDIR}/../poky/meta-yocto-bsp \
  ${TOPDIR}/../meta-tegra \
  ${TOPDIR}/../meta-openembedded/meta-oe \
  ${TOPDIR}/../meta-openembedded/meta-python \
  ${TOPDIR}/../meta-openembedded/meta-networking \
  "
EOF
```

</details>

### Step 3：为 Jetson Orin Nano 8GB 配置 local.conf

```bash
cat > conf/local.conf << 'EOF'
# ── Machine ───────────────────────────────────────────────────────────────────
MACHINE = "jetson-orin-nano-devkit"
# jetson-orin-nano-devkit covers both Orin Nano 4GB and 8GB developer kits.
# The Orin Nano 8GB module uses a different SOM; the MACHINE is the same
# because the devkit carrier board is identical. The SOM variant is selected
# at flash time via the appropriate Device Tree Blob (DTB).

# ── Distribution ─────────────────────────────────────────────────────────────
DISTRO = "poky"
# Use systemd instead of SysV init (required for most modern AI stacks)
DISTRO_FEATURES:append = " systemd"
VIRTUAL-RUNTIME_init_manager = "systemd"
DISTRO_FEATURES_BACKFILL_CONSIDERED += "sysvinit"

# ── NVIDIA GPU / CUDA ─────────────────────────────────────────────────────────
# Include NVIDIA binary drivers (requires accepting NVIDIA's EULA)
LICENSE_FLAGS_ACCEPTED = "commercial_nvidia-l4t-core nvidia-eula"
# Enable CUDA support in packages that support it (OpenCV, etc.)
CUDA_NVCC_EXTRA_FLAGS = "--gpu-architecture=sm_87"
# Jetson Orin Nano has Ampere GPU, SM 8.7

# ── Packages to include ────────────────────────────────────────────────────────
# Core AI stack
IMAGE_INSTALL:append = " \
    cuda-toolkit \
    tensorrt \
    libcudnn \
    opencv \
    python3 \
    python3-numpy \
    python3-pip \
"
# System utilities
IMAGE_INSTALL:append = " \
    openssh \
    git \
    htop \
    i2c-tools \
    can-utils \
    iproute2 \
"
# Real-time tuning tools (from previous OS lectures)
IMAGE_INSTALL:append = " \
    rt-tests \
    trace-cmd \
    perf \
"

# ── Image Features ────────────────────────────────────────────────────────────
EXTRA_IMAGE_FEATURES += "ssh-server-openssh debug-tweaks"
# debug-tweaks: enables root login without password (remove for production)

# ── Build parallelism ─────────────────────────────────────────────────────────
BB_NUMBER_THREADS = "12"
PARALLEL_MAKE = "-j 12"

# ── Shared caches ─────────────────────────────────────────────────────────────
DL_DIR = "/opt/yocto/downloads"
SSTATE_DIR = "/opt/yocto/sstate-cache"

# ── Output image format ───────────────────────────────────────────────────────
# Tegra images use tegraflash format — includes CBoot, DTB, rootfs partition
IMAGE_FSTYPES = "tegraflash"
EOF
```

> **常见陷阱：** `LICENSE_FLAGS_ACCEPTED` 变量必须与所示完全一致地包含 `nvidia-eula`。如果遗漏它，BitBake 会静默跳过所有 NVIDIA 二进制 recipe，构建出的镜像不含 CUDA —— 你不会看到任何报错，只是 runtime 时缺少某个包。如果 CUDA 包似乎缺失，务必运行 `bitbake -g core-image-my-ai` 并检查 `task-depends.dot`。

### Step 4：创建自定义镜像 recipe

创建自己的 layer 和镜像 recipe，让自定义内容保持**整洁且受版本控制**：

```bash
mkdir -p ../meta-my-jetson-image/recipes-core/images
cat > ../meta-my-jetson-image/recipes-core/images/jetson-orin-ai.bb << 'EOF'
# Custom AI image for Jetson Orin Nano 8GB
SUMMARY = "AI inference image for Jetson Orin Nano 8GB"
LICENSE = "MIT"

# Inherit from the standard Tegra image class
require recipes-core/images/tegra-image.inc

# Add the base Tegra runtime (kernel modules, L4T libs)
IMAGE_INSTALL += "tegra-libraries-core"

# ── AI inference stack ────────────────────────────────────────────────────────
IMAGE_INSTALL += " \
    cuda-toolkit \
    tensorrt \
    libcudnn \
    opencv \
    python3 \
    python3-numpy \
    python3-onnxruntime \
"

# ── Real-time and diagnostic tools ───────────────────────────────────────────
IMAGE_INSTALL += " \
    rt-tests \
    trace-cmd \
    bpftrace \
    can-utils \
    i2c-tools \
"

# ── System configuration ──────────────────────────────────────────────────────
IMAGE_INSTALL += " \
    openssh \
    systemd \
    util-linux \
    procps \
    htop \
"

# Enable root filesystem expansion on first boot (fills the eMMC partition)
IMAGE_FEATURES += "read-only-rootfs-delayed-postinsts"
EOF
```

把新 layer 添加到 `bblayers.conf`：

```bash
# Add to the BBLAYERS list in conf/bblayers.conf:
#   ${TOPDIR}/../meta-my-jetson-image \
```

### Step 5：构建镜像

```bash
bitbake jetson-orin-ai
```

这次构建比 Project 1 更大 —— 它包含 CUDA 和 TensorRT。在现代工作站上的预期耗时：
- 首次构建：4–8 小时（下载约 15 GB 源码与二进制 blob）
- 配置变更后的增量重建：10–30 分钟
- 新增一个包后的增量重建：2–5 分钟

监控进度：
```bash
# Real-time task log
tail -f tmp/log/cooker/jetson-orin-nano-devkit/console-latest.log

# Show currently running tasks
bitbake -u taskexp jetson-orin-ai
```


<details>
<summary>English original</summary>

**Step 3: Configure local.conf for Jetson Orin Nano 8GB**

```bash
cat > conf/local.conf << 'EOF'
# ── Machine ───────────────────────────────────────────────────────────────────
MACHINE = "jetson-orin-nano-devkit"
# jetson-orin-nano-devkit covers both Orin Nano 4GB and 8GB developer kits.
# The Orin Nano 8GB module uses a different SOM; the MACHINE is the same
# because the devkit carrier board is identical. The SOM variant is selected
# at flash time via the appropriate Device Tree Blob (DTB).

# ── Distribution ─────────────────────────────────────────────────────────────
DISTRO = "poky"
# Use systemd instead of SysV init (required for most modern AI stacks)
DISTRO_FEATURES:append = " systemd"
VIRTUAL-RUNTIME_init_manager = "systemd"
DISTRO_FEATURES_BACKFILL_CONSIDERED += "sysvinit"

# ── NVIDIA GPU / CUDA ─────────────────────────────────────────────────────────
# Include NVIDIA binary drivers (requires accepting NVIDIA's EULA)
LICENSE_FLAGS_ACCEPTED = "commercial_nvidia-l4t-core nvidia-eula"
# Enable CUDA support in packages that support it (OpenCV, etc.)
CUDA_NVCC_EXTRA_FLAGS = "--gpu-architecture=sm_87"
# Jetson Orin Nano has Ampere GPU, SM 8.7

# ── Packages to include ────────────────────────────────────────────────────────
# Core AI stack
IMAGE_INSTALL:append = " \
    cuda-toolkit \
    tensorrt \
    libcudnn \
    opencv \
    python3 \
    python3-numpy \
    python3-pip \
"
# System utilities
IMAGE_INSTALL:append = " \
    openssh \
    git \
    htop \
    i2c-tools \
    can-utils \
    iproute2 \
"
# Real-time tuning tools (from previous OS lectures)
IMAGE_INSTALL:append = " \
    rt-tests \
    trace-cmd \
    perf \
"

# ── Image Features ────────────────────────────────────────────────────────────
EXTRA_IMAGE_FEATURES += "ssh-server-openssh debug-tweaks"
# debug-tweaks: enables root login without password (remove for production)

# ── Build parallelism ─────────────────────────────────────────────────────────
BB_NUMBER_THREADS = "12"
PARALLEL_MAKE = "-j 12"

# ── Shared caches ─────────────────────────────────────────────────────────────
DL_DIR = "/opt/yocto/downloads"
SSTATE_DIR = "/opt/yocto/sstate-cache"

# ── Output image format ───────────────────────────────────────────────────────
# Tegra images use tegraflash format — includes CBoot, DTB, rootfs partition
IMAGE_FSTYPES = "tegraflash"
EOF
```

> **Common Pitfall:** The `LICENSE_FLAGS_ACCEPTED` variable must include `nvidia-eula` exactly as shown. If you forget it, BitBake will silently skip all NVIDIA binary recipes and build without CUDA — you will not see an error, just a missing package at runtime. Always run `bitbake -g core-image-my-ai` and check `task-depends.dot` if CUDA packages seem absent.

**Step 4: Create a Custom Image Recipe**

Create your own layer and image recipe to keep customizations **clean and version-controlled**:

```bash
mkdir -p ../meta-my-jetson-image/recipes-core/images
cat > ../meta-my-jetson-image/recipes-core/images/jetson-orin-ai.bb << 'EOF'
# Custom AI image for Jetson Orin Nano 8GB
SUMMARY = "AI inference image for Jetson Orin Nano 8GB"
LICENSE = "MIT"

# Inherit from the standard Tegra image class
require recipes-core/images/tegra-image.inc

# Add the base Tegra runtime (kernel modules, L4T libs)
IMAGE_INSTALL += "tegra-libraries-core"

# ── AI inference stack ────────────────────────────────────────────────────────
IMAGE_INSTALL += " \
    cuda-toolkit \
    tensorrt \
    libcudnn \
    opencv \
    python3 \
    python3-numpy \
    python3-onnxruntime \
"

# ── Real-time and diagnostic tools ───────────────────────────────────────────
IMAGE_INSTALL += " \
    rt-tests \
    trace-cmd \
    bpftrace \
    can-utils \
    i2c-tools \
"

# ── System configuration ──────────────────────────────────────────────────────
IMAGE_INSTALL += " \
    openssh \
    systemd \
    util-linux \
    procps \
    htop \
"

# Enable root filesystem expansion on first boot (fills the eMMC partition)
IMAGE_FEATURES += "read-only-rootfs-delayed-postinsts"
EOF
```

Add the new layer to `bblayers.conf`:

```bash
# Add to the BBLAYERS list in conf/bblayers.conf:
#   ${TOPDIR}/../meta-my-jetson-image \
```

**Step 5: Build the Image**

```bash
bitbake jetson-orin-ai
```

This build is larger than Project 1 — it includes CUDA and TensorRT. Expected time on a modern workstation:
- First build: 4–8 hours (downloads ~15 GB of sources and binary blobs)
- Incremental rebuild after config change: 10–30 minutes
- Incremental rebuild after adding one package: 2–5 minutes

Monitor progress:
```bash
# Real-time task log
tail -f tmp/log/cooker/jetson-orin-nano-devkit/console-latest.log

# Show currently running tasks
bitbake -u taskexp jetson-orin-ai
```

</details>

### 步骤 6：检查输出

```bash
ls tmp/deploy/images/jetson-orin-nano-devkit/
# Key files:
# jetson-orin-ai-jetson-orin-nano-devkit.tegraflash.tar.gz  ← flash bundle
# Image                                                      ← compressed kernel
# tegra234-p3768-0000+p3767-0005-nv.dtb                    ← Orin Nano 8GB DTB
# bootloader/                                               ← CBoot, MB1, TOS
```

`tegraflash.tar.gz` bundle 包含 **`tegraflash.py` 烧录板卡所需的一切**。DTB `p3767-0005` 是 Orin Nano 8GB 模块的标识符。

```
tegraflash bundle contents:
├── flash.xml              ← partition layout (eMMC map)
├── Image                  ← kernel
├── tegra234-*.dtb         ← Device Tree for your specific module
├── boot.img               ← kernel + initramfs
├── system.img             ← rootfs (ext4, ~2–4 GB)
├── bootloader/
│   ├── mb1_t234_prod.bin  ← MB1 bootloader (NVIDIA signed)
│   ├── cboot.bin          ← CBoot (U-Boot replacement)
│   └── tos-a.img          ← Trusted OS (TrustZone)
└── tegraflash.py          ← flash script
```

### 步骤 7：烧录到 Jetson Orin Nano 8GB

**硬件准备：**

```
1. Connect Jetson Orin Nano devkit to host PC via USB-C (J15 port — Recovery USB)
2. Insert jumper on J14 (Force Recovery header) pins 1-2
3. Power on the board
4. Confirm the board appears on host: lsusb | grep NVIDIA
   → "ID 0955:7323 NVIDIA Corp. APX" indicates recovery mode
5. Remove the recovery jumper
```

**烧录：**

```bash
# Extract the tegraflash bundle
tar xf tmp/deploy/images/jetson-orin-nano-devkit/jetson-orin-ai-jetson-orin-nano-devkit.tegraflash.tar.gz
cd jetson-orin-ai-*tegraflash/

# Flash all partitions
sudo ./tegraflash.py --flash all
# This takes 5–15 minutes
# Progress: writes bootloader, DTB, kernel, and rootfs to eMMC
```

> **常见陷阱：**如果 USB 线插错端口，烧录会以 "device not found" 失败。Orin Nano 开发套件有两个 USB-C 端口：J14（显示/数据）和 J15（恢复/调试）。恢复模式仅在 J15 上有效。如果线材质量勉强（高阻），APX 设备会短暂出现然后消失 —— 请使用短且已知良好的 USB 3.0 线。

### 步骤 8：首次启动与验证

烧录完成后，取下恢复跳线（若仍插着），重新上下电，并通过串口控制台或 SSH 连接：

```bash
# Serial console (115200 baud) on J14 micro-USB
screen /dev/ttyUSB0 115200

# Or wait for DHCP and SSH in
ssh root@<jetson-ip>
```

验证关键 AI 组件：

```bash
# Verify NVIDIA GPU driver loaded
nvidia-smi
# Should show: "Orin (nvgpu)", CUDA Version, memory

# Verify CUDA installation
nvcc --version
python3 -c "import ctypes; ctypes.cdll.LoadLibrary('libcuda.so'); print('CUDA OK')"

# Verify TensorRT
python3 -c "import tensorrt as trt; print('TRT', trt.__version__)"

# Check kernel version (should be L4T 6.1.x)
uname -r

# Check thermal zones (from Lecture 1 — thermal monitoring)
cat /sys/class/thermal/thermal_zone*/temp
# CPU, GPU, SoC, CV zones in millidegrees Celsius

# RT scheduling tools available?
cyclictest --help
chrt -p 1
```

### 步骤 9：定制模式

基础镜像跑通后，以下是 **AI 部署的常见定制项**：

```bash
# ── Disable unnecessary services (reduce boot time and attack surface) ────────
# In a bbappend or local.conf:
SYSTEMD_AUTO_ENABLE:pn-avahi-daemon = "disable"
SYSTEMD_AUTO_ENABLE:pn-bluetooth = "disable"

# ── Pre-install a TensorRT model at build time ────────────────────────────────
# In your image recipe:
IMAGE_INSTALL += "my-model-package"
# Where my-model-package.bb installs the .engine file to /opt/models/

# ── Apply RT tuning at boot (from Lecture 7) ─────────────────────────────────
# Add a systemd service that runs the RT tuning script:
# isolcpus=4-11 nohz_full=4-11 rcu_nocbs=4-11
# in /boot/extlinux/extlinux.conf APPEND line (modify via bbappend)

# ── Set root password for production (remove debug-tweaks) ───────────────────
# Remove "debug-tweaks" from IMAGE_FEATURES
# Add to local.conf:
INHERIT += "extrausers"
EXTRA_USERS_PARAMS = "usermod -p '\$6\$...' root;"

# ── OTA A/B update support (from Lecture 22) ─────────────────────────────────
# meta-tegra supports OTA updates via NVIDIA's Over-the-Air (OTA) framework
# Enable with:
TEGRA_REDUNDANT_BOOT = "1"
```

---


<details>
<summary>English original</summary>

**Step 6: Inspect the Output**

```bash
ls tmp/deploy/images/jetson-orin-nano-devkit/
# Key files:
# jetson-orin-ai-jetson-orin-nano-devkit.tegraflash.tar.gz  ← flash bundle
# Image                                                      ← compressed kernel
# tegra234-p3768-0000+p3767-0005-nv.dtb                    ← Orin Nano 8GB DTB
# bootloader/                                               ← CBoot, MB1, TOS
```

The `tegraflash.tar.gz` bundle contains **everything `tegraflash.py` needs to flash the board**. The DTB `p3767-0005` is the Orin Nano 8GB module identifier.

```
tegraflash bundle contents:
├── flash.xml              ← partition layout (eMMC map)
├── Image                  ← kernel
├── tegra234-*.dtb         ← Device Tree for your specific module
├── boot.img               ← kernel + initramfs
├── system.img             ← rootfs (ext4, ~2–4 GB)
├── bootloader/
│   ├── mb1_t234_prod.bin  ← MB1 bootloader (NVIDIA signed)
│   ├── cboot.bin          ← CBoot (U-Boot replacement)
│   └── tos-a.img          ← Trusted OS (TrustZone)
└── tegraflash.py          ← flash script
```

**Step 7: Flash to Jetson Orin Nano 8GB**

**Hardware setup:**

```
1. Connect Jetson Orin Nano devkit to host PC via USB-C (J15 port — Recovery USB)
2. Insert jumper on J14 (Force Recovery header) pins 1-2
3. Power on the board
4. Confirm the board appears on host: lsusb | grep NVIDIA
   → "ID 0955:7323 NVIDIA Corp. APX" indicates recovery mode
5. Remove the recovery jumper
```

**Flash:**

```bash
# Extract the tegraflash bundle
tar xf tmp/deploy/images/jetson-orin-nano-devkit/jetson-orin-ai-jetson-orin-nano-devkit.tegraflash.tar.gz
cd jetson-orin-ai-*tegraflash/

# Flash all partitions
sudo ./tegraflash.py --flash all
# This takes 5–15 minutes
# Progress: writes bootloader, DTB, kernel, and rootfs to eMMC
```

> **Common Pitfall:** Flashing fails with "device not found" if the USB cable is plugged into the wrong port. The Orin Nano devkit has two USB-C ports: J14 (display/data) and J15 (recovery/debug). Recovery mode only works on J15. If your cable is marginal (high-resistance), the APX device appears briefly then disappears — use a short, known-good USB 3.0 cable.

**Step 8: First Boot and Validation**

After flashing, remove the recovery jumper (if still in), power cycle, and connect via serial console or SSH:

```bash
# Serial console (115200 baud) on J14 micro-USB
screen /dev/ttyUSB0 115200

# Or wait for DHCP and SSH in
ssh root@<jetson-ip>
```

Validate the key AI components:

```bash
# Verify NVIDIA GPU driver loaded
nvidia-smi
# Should show: "Orin (nvgpu)", CUDA Version, memory

# Verify CUDA installation
nvcc --version
python3 -c "import ctypes; ctypes.cdll.LoadLibrary('libcuda.so'); print('CUDA OK')"

# Verify TensorRT
python3 -c "import tensorrt as trt; print('TRT', trt.__version__)"

# Check kernel version (should be L4T 6.1.x)
uname -r

# Check thermal zones (from Lecture 1 — thermal monitoring)
cat /sys/class/thermal/thermal_zone*/temp
# CPU, GPU, SoC, CV zones in millidegrees Celsius

# RT scheduling tools available?
cyclictest --help
chrt -p 1
```

**Step 9: Customization Patterns**

Once the base image works, these are the **common customizations for AI deployment**:

```bash
# ── Disable unnecessary services (reduce boot time and attack surface) ────────
# In a bbappend or local.conf:
SYSTEMD_AUTO_ENABLE:pn-avahi-daemon = "disable"
SYSTEMD_AUTO_ENABLE:pn-bluetooth = "disable"

# ── Pre-install a TensorRT model at build time ────────────────────────────────
# In your image recipe:
IMAGE_INSTALL += "my-model-package"
# Where my-model-package.bb installs the .engine file to /opt/models/

# ── Apply RT tuning at boot (from Lecture 7) ─────────────────────────────────
# Add a systemd service that runs the RT tuning script:
# isolcpus=4-11 nohz_full=4-11 rcu_nocbs=4-11
# in /boot/extlinux/extlinux.conf APPEND line (modify via bbappend)

# ── Set root password for production (remove debug-tweaks) ───────────────────
# Remove "debug-tweaks" from IMAGE_FEATURES
# Add to local.conf:
INHERIT += "extrausers"
EXTRA_USERS_PARAMS = "usermod -p '\$6\$...' root;"

# ── OTA A/B update support (from Lecture 22) ─────────────────────────────────
# meta-tegra supports OTA updates via NVIDIA's Over-the-Air (OTA) framework
# Enable with:
TEGRA_REDUNDANT_BOOT = "1"
```

---

</details>

## 与 OS 课程衔接

| OS 课程 | Yocto 中的体现 |
|-----------|---------------------|
| 第 1 讲：Linux 内核架构 | Yocto 从源码构建 kernel；`KCONFIG_MODE` 控制编译进哪些驱动 |
| 第 5 讲：启动流程与设备树 | `meta-tegra` 提供 DTB 文件；kernel recipe 中的 `SRC_URI` 给 DTS 打补丁 |
| 第 7 讲：PREEMPT_RT | 加入 `PREFERRED_PROVIDER_virtual/kernel = "linux-tegra"` 并给 `CONFIG_PREEMPT_RT=y` 打补丁 |
| 第 17 讲：Linux 驱动模型 | `meta-tegra/recipes-kernel/` 中的 recipe 加入树外模块（`nvgpu.ko`、`nvcsi.ko`） |
| 第 21 讲：文件系统 | `IMAGE_FSTYPES` 控制 ext4/btrfs/F2FS；`EXTRA_IMAGECMD:ext4` 设置块大小 |
| 第 22 讲：OTA 分区 | `tegraflash.xml` 定义 A/B 分区布局；`TEGRA_REDUNDANT_BOOT=1` 启用它 |
| 第 23 讲：容器 | 把 `docker` 加入 `IMAGE_INSTALL`；NVIDIA Container Runtime 是单独的 meta-tegra recipe |
| 第 24 讲：L4T、面向 AI 的 Yocto | 本项目本身就是 AI 边缘设备的生产工作流 |

---

## 小结

| 项目 | 目标 | 镜像 | 关键工具 | 烧录方式 |
|---------|--------|-------|----------|--------------|
| 项目 1 | `qemux86-64`（虚拟） | `core-image-minimal` | `runqemu` | 无需烧录 |
| 项目 2 | Jetson Orin Nano 8GB | `jetson-orin-ai` | `tegraflash.py` | USB-C Recovery Mode |

### 概念回顾

- **为什么用 Yocto，而不是烧录现成的 JetPack 镜像？** 现成的 JetPack 是固定的、基于 Ubuntu 的发行版，既难以定制，也无法逐 bit 复现。Yocto 从源码构建最小镜像，只包含你显式纳入的内容，且由 sstate 缓存保证构建可复现。对生产设备而言，这能缩小攻击面、控制二进制组成，并支持自动化 OTA 更新。
- **`source oe-init-build-env` 究竟做了什么？** 它设置 `BBPATH`、`PATH` 和 `BUILDDIR` 环境变量，按模板创建 `build/conf/local.conf` 和 `bblayers.conf`，并 cd 进入构建目录。BitBake 依赖这些环境变量来定位 recipe 和配置。
- **为什么 Jetson 镜像用 `tegraflash` 格式，而不是裸 ext4？** Tegra 平台的分区布局很复杂：MB1（一级 bootloader）、CBoot、设备树、A/B kernel、A/B rootfs、RPMB（安全存储）各有独立分区。`tegraflash.py` 知道如何借助 `flash.xml` 分区映射把每个分区写到正确的 eMMC 偏移处。裸 ext4 只包含 rootfs，什么都启动不了。
- **`meta-tegra` 和 L4T 是什么关系？** `meta-tegra` 是打包 L4T 的 Yocto layer。它从 NVIDIA 的 GitHub 拉取 L4T kernel 源码，应用 Tegra 专用补丁，并用目标板所需的配置构建。该 layer 在其 recipe 文件中固定到特定的 L4T 版本。
- **如何把自定义 Python 应用加入 Yocto 镜像？** 写一个 recipe（`my-app.bb`），把 `SRC_URI` 设为应用的源码位置，定义 `do_install` 将文件复制到 `${D}/opt/my-app/`，并把 `my-app` 加入 `IMAGE_INSTALL`。BitBake 会自动处理交叉编译、依赖和打包。
- **为什么首次构建要 4–8 小时，而增量构建只需几分钟？** BitBake 会对每个任务的输入（源码、recipe 内容、环境变量）做哈希。若哈希不变，就直接从 sstate 缓存恢复输出，不再重跑该任务。往 `IMAGE_INSTALL` 里加一个包，只会触发该包的 fetch/compile/install 任务，然后重新组装 rootfs —— kernel、glibc 以及其他所有包都瞬间从缓存中取出。

---


<details>
<summary>English original</summary>

**Connecting to the OS Curriculum**

| OS Lecture | Yocto Manifestation |
|-----------|---------------------|
| Lecture 1: Linux kernel architecture | Yocto builds the kernel from source; `KCONFIG_MODE` controls which drivers compile in |
| Lecture 5: Boot process & Device Tree | `meta-tegra` provides DTB files; `SRC_URI` in kernel recipe patches the DTS |
| Lecture 7: PREEMPT_RT | Add `PREFERRED_PROVIDER_virtual/kernel = "linux-tegra"` and patch `CONFIG_PREEMPT_RT=y` |
| Lecture 17: Linux driver model | Recipes in `meta-tegra/recipes-kernel/` add out-of-tree modules (`nvgpu.ko`, `nvcsi.ko`) |
| Lecture 21: Filesystems | `IMAGE_FSTYPES` controls ext4/btrfs/F2FS; `EXTRA_IMAGECMD:ext4` sets block size |
| Lecture 22: OTA partitioning | `tegraflash.xml` defines A/B partition layout; `TEGRA_REDUNDANT_BOOT=1` enables it |
| Lecture 23: Containers | Add `docker` to `IMAGE_INSTALL`; NVIDIA Container Runtime is a separate meta-tegra recipe |
| Lecture 24: L4T, Yocto for AI | This project IS the production workflow for AI edge devices |

---

**Summary**

| Project | Target | Image | Key Tool | Flash Method |
|---------|--------|-------|----------|--------------|
| Project 1 | `qemux86-64` (virtual) | `core-image-minimal` | `runqemu` | No flashing needed |
| Project 2 | Jetson Orin Nano 8GB | `jetson-orin-ai` | `tegraflash.py` | USB-C Recovery Mode |

**Conceptual Review**

- **Why use Yocto instead of flashing the stock JetPack image?** Stock JetPack is a fixed Ubuntu-based distribution you cannot easily customize or reproduce bit-for-bit. Yocto builds a minimal image from source that contains only what you explicitly include, with reproducible builds guaranteed by the sstate cache. For production devices, this reduces attack surface, controls binary composition, and enables automated OTA updates.
- **What does `source oe-init-build-env` actually do?** It sets `BBPATH`, `PATH`, and `BUILDDIR` environment variables, creates `build/conf/local.conf` and `bblayers.conf` from templates, and cd's into the build directory. BitBake relies on these environment variables to locate recipes and configuration.
- **Why does the Jetson image use `tegraflash` format instead of a raw ext4?** The Tegra platform has a complex partition layout: separate partitions for MB1 (first-stage bootloader), CBoot, Device Tree, A/B kernel, A/B rootfs, and RPMB (secure storage). `tegraflash.py` knows how to write each partition to the correct eMMC offset using the `flash.xml` partition map. A raw ext4 would only contain the rootfs and boot on nothing.
- **What is the relationship between `meta-tegra` and L4T?** `meta-tegra` is the Yocto layer that packages L4T. It pulls the L4T kernel source from NVIDIA's GitHub, applies Tegra-specific patches, and builds it with the configuration needed for the target board. The layer pins to specific L4T versions in its recipe files.
- **How do you add a custom Python application to the Yocto image?** Write a recipe (`my-app.bb`) that sets `SRC_URI` to your application's source, defines `do_install` to copy files to `${D}/opt/my-app/`, and adds `my-app` to `IMAGE_INSTALL`. BitBake handles cross-compilation, dependencies, and packaging automatically.
- **Why does the first build take 4–8 hours but incremental builds take minutes?** BitBake hashes the inputs of every task (source code, recipe contents, environment variables). If the hash is unchanged, it restores the output from the sstate cache without rerunning the task. Adding one package to `IMAGE_INSTALL` only triggers that package's fetch/compile/install tasks, then reassembles the rootfs — the kernel, glibc, and all other packages are served from cache instantly.

---

</details>

## AI 硬件连接

- Yocto 是这条路线图中每款量产 AI 边缘设备的标准构建系统：openpilot Agnos、Jetson 商用部署、汽车 ECU 的 Linux 分区，以及定制的 FPGA SoC 板卡，都使用 Yocto 或 Yocto 派生的构建系统（meta-tegra、OpenWRT）。
- 第 7 讲中的 `PREEMPT_RT` kernel config 通过创建一个 `linux-tegra_%.bbappend` 文件加入 Yocto Jetson 构建，该文件在 kernel fragment 中设置 `CONFIG_PREEMPT_RT=y` —— 你研究过的同一个 kernel，现在可以在构建时由你自行配置。
- Jetson 上的 OTA A/B 更新（第 22 讲）由 Yocto 中的 `TEGRA_REDUNDANT_BOOT = "1"` 启用，并由 NVIDIA 的 `nv-update-engine` 管理 —— 从一开始就把它构建进镜像，远比部署之后再改造便宜。
- Yocto 镜像中的 TensorRT 和 CUDA 包（`cuda-toolkit`、`tensorrt`）与阶段 4 方向 B（Jetson）中研究的用户态库相同，但现在由你控制设备上装的是哪个版本 —— 对于确保可能相隔数月制造的整批设备的模型兼容性至关重要。
- 可复现性对安全至关重要：ISO 26262 ASIL 认证要求能够重建出经过认证的那一份确切二进制。Yocto 的 sstate 缓存和 `SSTATE_MIRRORS` 基础设施提供了这一保证 —— 认证过的构建可以在多年之后从相同的 recipe 输入复现出来。
- 自定义 layer（`meta-my-jetson-image`）是量产的工程产物：当你加入一支 AI 硬件团队时，你将在定义公司产品镜像的 meta layer 中工作，添加传感器、优化服务，并在固件更新过程中管理操作系统。


<details>
<summary>English original</summary>

**AI Hardware Connection**

- Yocto is the standard build system for every production AI edge device in this roadmap: openpilot Agnos, Jetson commercial deployments, automotive ECU Linux partitions, and custom FPGA SoC boards all use Yocto or Yocto-derived build systems (meta-tegra, OpenWRT).
- The `PREEMPT_RT` kernel config from Lecture 7 is added to a Yocto Jetson build by creating a `linux-tegra_%.bbappend` file that sets `CONFIG_PREEMPT_RT=y` in a kernel fragment — the same kernel you studied is now yours to configure at build time.
- OTA A/B updates (Lecture 22) on Jetson are enabled by `TEGRA_REDUNDANT_BOOT = "1"` in Yocto and managed by NVIDIA's `nv-update-engine` — building this into the image from the start is far cheaper than retrofitting it after deployment.
- The TensorRT and CUDA packages in the Yocto image (`cuda-toolkit`, `tensorrt`) are the same userspace libraries studied in Phase 4 Track B (Jetson), but now you control which version is on the device — critical for ensuring model compatibility across a fleet of devices that may have been manufactured months apart.
- Reproducibility matters for safety: ISO 26262 ASIL certification requires the ability to rebuild the exact binary that was certified. Yocto's sstate cache and `SSTATE_MIRRORS` infrastructure provide this guarantee — a certified build can be reproduced years later from the same recipe inputs.
- Custom layers (`meta-my-jetson-image`) are the production engineering artifact: when you join an AI hardware team, you will be working in a meta layer that defines the company's product image, adding sensors, optimizing services, and managing the OS across firmware updates.

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-25.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-25.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
