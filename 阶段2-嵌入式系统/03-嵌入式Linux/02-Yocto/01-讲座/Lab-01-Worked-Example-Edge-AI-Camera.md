---
title: Lab 1 — 完整示例：用 Yocto 术语描述的产品需求
description: Lab 1 — 完整示例：用 Yocto 术语描述的产品需求
---

# Lab 1 — 完整示例：用 Yocto 术语描述的产品需求

**配套材料：** [Lecture 3 — Module 1](Lecture-03.md) · **课程：** [Yocto guide](../Guide.md)

这是 Lab 1 的**完整参考答案**：一个可信的嵌入式产品，用 **Yocto 词汇**（MACHINE、layer、镜像内容、OTA、分区）来描述。请把硬件和 layer 名称换成你实际的 BSP 与 Yocto 发行版。

**注意：** 确切的 `MACHINE` 字符串、layer 名称和 recipe 名称取决于你的 **vendor BSP** 与 **Yocto 分支**。务必对照 BSP README 和你所 pin 的发行版进行确认。想在本路线图上深入实践 Jetson + Yocto 生产环境，参见**阶段 4 方向 B**（*Orin-Nano-Yocto-BSP-Production*）。

---

## 1. 目标硬件

**产品概念：** 边缘 AI 摄像头节点（视频接入 + 端侧推理）。

| 项目 | 选择 |
|------|--------|
| SoC | NVIDIA Jetson Orin Nano（示例） |
| 架构 | AArch64 |
| Yocto | Vendor BSP（例如 **meta-tegra** / 面向 NVIDIA 的 layer） |
| 加速 | GPU、张量核心（在获得许可并受支持的前提下，通过 vendor 栈使用 CUDA / TensorRT） |

**Yocto 映射**

- 把 **`MACHINE`** 设为你 BSP 定义的值（示例占位值：`jetson-orin-nano-devkit` — **请以 BSP 文档为准**）。
- 把 **BSP layer**（及其文档中列出的依赖）加入 **`bblayers.conf`**。
- kernel、固件以及许多用户态 GPU 组件，都由该 layer 或配套 layer 中的 **vendor recipe 提供**。

---

## 2. 连接性

**必需 / 期望的接口**

- 以太网（主回传）
- Wi-Fi 与低功耗蓝牙（BLE）（可选产品目标）
- USB host（外设）
- MIPI CSI-2（摄像头）
- UART（串口控制台）

**Yocto 映射**

- **kernel + 设备树：** 为你的载板和模块启用或带上正确的 driver 和 DT 节点。
- **用户态软件包（示例）：** Wi-Fi 用 `wpa-supplicant` 或 `iwd`，蓝牙用 `bluez5`，网络用 `iproute2`；确切的 recipe 名称因 layer 而异。
- **调试：** UART 上的 getty 通常属于镜像 / 发行版策略，不是出货之后还能“apt install”的东西。

---

## 3. 存储布局

**假设：** 量产用 eMMC（或 NVMe）；bring-up（上电点亮/调通）阶段可以用 SD。

**意图**

- 启动产物（bootloader、kernel，以及平台要求的 DTB）
- 带 **A/B** 槽位以支持 OTA 的 rootfs
- 可写的 **data** 分区（日志、模型、配置）

**Yocto 映射**

- **`wic`**（`.wks`）或 BSP 专有的镜像类型决定分区表。
- 你的 **image recipe** 与 **OTA layer** 必须在以下方面达成一致：哪个分区是活动分区、rootfs 被拷贝到哪里、bootloader 如何在 A 与 B 之间选择。

仅作示意的**概念**（不是任何具体 BSP 可直接套用的 `.wks`）：

```
# Pseudocode — replace with BSP-correct sources and sizes
part /boot   --source bootimg-partition --size 256M
part /       --source rootfs --label rootfs_a
part /       --source rootfs --label rootfs_b
part /data   --size 1024M --fstype ext4
```

---

## 4. 更新策略

**选择：** 采用 **A/B** rootfs 并支持回滚的**基于镜像的 OTA**。

**典型技术栈**

- **RAUC**（`meta-rauc` + 集成工作），或
- **Mender**（`meta-mender` + 集成工作）

**Yocto 映射**

- 加入 OTA **meta-layer**，并遵循其 **MACHINE** / **distro** 集成指南。
- bootloader（通常是 U-Boot 或平台专用固件）必须支持**切换**启动路径，以及你定义的失败策略。
- 如果你的威胁模型有此要求，CI 应产出**签名**或经其他方式保护的 bundle（此处不展开，但要提前规划）。

---

## 5. 必备用户态组件

**核心**

- **Init：** `systemd`（如果这是你的发行版策略）
- **远程访问：** `openssh`
- **日志：** journald（配合 `systemd`）

**应用 / AI 栈（取决于产品）**

- CUDA / TensorRT（通常是 **vendor recipe**；受许可与再分发规则约束）
- **Python 3**
- **OpenCV**（recipe 名可能是 `opencv`、`opencv-python`，或拆分成多个软件包 — 请查看你的 layer）
- **GStreamer** 以及用于摄像头流水线的插件

**可选**

- **容器：** OCI runtime / `docker` — 通常较重；许多团队会放到专门的 meta-layer 或 vendor bundle 里处理。要把它当作**显式**的镜像特性，而不是事后补上的东西。

**实时**

- **中等**延迟：通常 **PREEMPT** kernel 就够了。
- **硬实时：** 是另一项独立工作（PREEMPT_RT、CPU 隔离、测量）。不要以为默认 BSP kernel 就能“白送”这个能力。

**示例 `local.conf` 片段（示意）**

使用 **`IMAGE_INSTALL:append`**（近期 Yocto 的语法），并且**只**使用**你**已配置 layer 中确实存在的软件包：

```
IMAGE_INSTALL:append = " \
    openssh \
    python3 \
    python3-pip \
"
```

在你用 `bitbake-layers` / `oe-pkgdata-util` 或 layer 文档确认了 recipe 名称**之后**，再加入 `opencv`、GStreamer 软件包、OTA 客户端等。

**Kernel（概念）**

- 若要启用可抢占 kernel 行为，使用 **config fragment** 或 BSP 支持的机制；示例符号：`CONFIG_PREEMPT=y`（具体集成方式取决于 kernel recipe）。

---


<details>
<summary>English original</summary>

**Lab 1 — Worked example: product requirements in Yocto terms**

**Companion:** [Lecture 3 — Module 1](Lecture-03.md) · **Course:** [Yocto guide](../Guide.md)

This is a **complete reference answer** for Lab 1: one plausible embedded product described in **Yocto vocabulary** (MACHINE, layers, image contents, OTA, partitions). Adapt the hardware and layer names to your real BSP and Yocto release.

**Note:** Exact `MACHINE` strings, layer names, and recipe names depend on your **vendor BSP** and **Yocto branch**. Always confirm against the BSP README and the release you pinned. For deep Jetson + Yocto production practice on this roadmap, see **Phase 4 Track B** (*Orin-Nano-Yocto-BSP-Production*).

---

**1. Target hardware**

**Product concept:** Edge AI camera node (video ingest + on-device inference).

| Item | Choice |
|------|--------|
| SoC | NVIDIA Jetson Orin Nano (example) |
| Architecture | AArch64 |
| Yocto | Vendor BSP (e.g. **meta-tegra** / NVIDIA-oriented layers) |
| Acceleration | GPU, Tensor cores (CUDA / TensorRT via vendor stacks where licensed and supported) |

**Yocto mapping**

- Set **`MACHINE`** to the value defined by your BSP (example placeholder: `jetson-orin-nano-devkit` — **verify in BSP docs**).
- Add the **BSP layer** (and its documented dependencies) to **`bblayers.conf`**.
- Kernel, firmware, and many userspace GPU pieces are **owned by vendor recipes** in that layer or companion layers.

---

**2. Connectivity**

**Required / desired interfaces**

- Ethernet (primary backhaul)
- Wi-Fi and BLE (optional product goal)
- USB host (peripherals)
- MIPI CSI-2 (camera)
- UART (serial console)

**Yocto mapping**

- **Kernel + device tree:** enable or carry the right drivers and DT nodes for your carrier and modules.
- **Userspace packages (examples):** `wpa-supplicant` or `iwd` for Wi-Fi, `bluez5` for Bluetooth, `iproute2` for networking; exact recipe names vary by layer.
- **Debug:** getty on UART is typically image / distro policy, not something you “apt install” after shipping.

---

**3. Storage layout**

**Assumption:** eMMC (or NVMe) for production; SD possible for bring-up.

**Intent**

- Boot artifacts (bootloader, kernel, DTB as required by platform)
- Root filesystem with **A/B** slots for OTA
- Writable **data** partition (logs, models, config)

**Yocto mapping**

- **`wic`** (`.wks`) or BSP-specific image types define partition tables.
- Your **image recipe** + **OTA layer** must agree on which partition is active, where the rootfs is copied, and how the bootloader selects A vs B.

Illustrative **concept only** (not a drop-in `.wks` for any specific BSP):

```
# Pseudocode — replace with BSP-correct sources and sizes
part /boot   --source bootimg-partition --size 256M
part /       --source rootfs --label rootfs_a
part /       --source rootfs --label rootfs_b
part /data   --size 1024M --fstype ext4
```

---

**4. Update strategy**

**Choice:** **Image-based OTA** with **A/B** rootfs and rollback.

**Typical stacks**

- **RAUC** (`meta-rauc` + integration work), or
- **Mender** (`meta-mender` + integration work)

**Yocto mapping**

- Add the OTA **meta-layer** and follow its **MACHINE** / **distro** integration guide.
- Bootloader (often U-Boot or platform-specific firmware) must support **switching** boot paths and failure policies you define.
- CI should produce **signed** or otherwise protected bundles if your threat model requires it (out of scope here, but plan for it).

---

**5. Must-have userspace**

**Core**

- **Init:** `systemd` (if that is your distro policy)
- **Remote access:** `openssh`
- **Logs:** journald (with `systemd`)

**Application / AI stack (product-dependent)**

- CUDA / TensorRT (usually **vendor recipes**; licensing and redistribution rules apply)
- **Python 3**
- **OpenCV** (recipe name may be `opencv`, `opencv-python`, or split packages — check your layers)
- **GStreamer** and plugins for camera pipelines

**Optional**

- **Containers:** OCI runtime / `docker` — often heavy; many teams defer to a dedicated meta-layer or vendor bundle. Treat as **explicit** image feature, not an afterthought.

**Real time**

- **Moderate** latency: often **PREEMPT** kernel is enough.
- **Hard real time:** separate exercise (PREEMPT_RT, CPU isolation, measurement). Do not assume it “for free” from a default BSP kernel.

**Example `local.conf` fragment (illustrative)**

Use **`IMAGE_INSTALL:append`** (syntax for recent Yocto) and **only** packages that exist in **your** configured layers:

```
IMAGE_INSTALL:append = " \
    openssh \
    python3 \
    python3-pip \
"
```

Add `opencv`, GStreamer packages, OTA client, etc., **after** you confirm recipe names with `bitbake-layers` / `oe-pkgdata-util` or layer documentation.

**Kernel (concept)**

- For preemptible kernel behavior, use **config fragments** or BSP-supported mechanisms; example symbol: `CONFIG_PREEMPT=y` (exact integration depends on kernel recipe).

---

</details>

## 6. 必须有 vs 最好有

**必须有（生产关键）**

- 你的板子的启动链 + kernel + DTB
- 摄像头通路（CSI 驱动 + 你要出货的用户态采集栈）
- 最小网络通路（许多产品至少要有一条 Ethernet）
- SSH（或其他受控管理通路），如果运维需要
- 你实际出货的推理 runtime（CUDA/TensorRT 或更轻量的栈）
- 如果你承诺了镜像 OTA，就要有 **OTA 客户端 + A/B**
- 应用的 **systemd** unit（或你自己的 init 策略）

**最好有**

- Wi-Fi / BLE，如果第一天并不需要
- Docker，如果编排可以推迟
- 调试工具（`strace`、`htop`、`tcpdump`）——通常是 **`debug`** 镜像变体，而非生产镜像
- GUI / Wayland——在无头边缘摄像头上通常省略

---

## 心态（本实验在训练什么）

- Yocto 从**声明式元数据**构建**镜像**；问题在于**烤进去了什么**，而不是你之后在黄金设备上装了什么。
- **硬件对齐**经由 **MACHINE**、**BSP layer**、**kernel / DT** 和**镜像 recipe** 传递。
- **可复现性**意味着 layer 版本固定、**DISTRO** 策略已知，以及能从相同输入重新构建的 CI。

---

## 附加：面向仓库的文件草图

**`conf/local.conf`**（片段）

```
MACHINE ?= "YOUR_BSP_MACHINE_NAME"
DISTRO ?= "poky"
# Parallelism, download dir, sstate — set per team policy
```

**`conf/bblayers.conf`**（示意性 layer 栈）

```
# Bottom-up: core + hardware + OTA + product
# Exact paths and layer names must match your checkout
BBPATH = "${TOPDIR}"
BBFILES ?= ""
```

你将添加诸如 `meta`、`meta-poky`、`meta-oe`、**BSP meta**、**meta-rauc** 或 **meta-mender** 之类的条目，然后为你自己的镜像和 bbappend 添加 **`meta-yourproduct`**。

**自定义镜像**（位于你的产品 layer 中），概念上：

```
inherit core-image
IMAGE_INSTALL:append = "your-runtime-packages"
```

当整个栈接通后，把镜像命名为 `yourproduct-image.bb` 和 `bitbake yourproduct-image`。

---

## 如何实现（工作流）

上面的章节是**做什么**。本节是**怎么做**：从空 checkout 到可烧录产物的一个现实操作顺序。把它当作**蓝图**——你需要调整分支名、layer 路径、`MACHINE` 和 recipe 名，以匹配**你自己的** BSP 和 Yocto 发行版。

**现实检查**

- **Layer 兼容性：**Poky、`meta-openembedded`、`meta-tegra`（OE4T）和 `meta-rauc` 各自面向特定的 Yocto 分支。使用 BSP **文档中记载的组合**（例如 OE4T manifest 或 README），而不是随意混搭 `git checkout` tag。
- **Jetson + RAUC + A/B：**Tegra 上的启动流程与分区是 **BSP 特定的**。除了“加上 meta-rauc”之外，还要预期额外的集成工作（bootloader、签名、slot 布局）。
- **CUDA / TensorRT / Docker：**通常需要厂商或社区 layer、接受许可，而且有时**不能**与最小 RAUC 目标用同一个镜像。在锁定镜像之前，用 `bitbake -e` / `bitbake <recipe>` 验证每个包。

**流水线：**主机环境搭建 → clone Poky → `oe-init-build-env` → 添加 layer → `local.conf` → 产品 layer → 镜像 recipe →（可选）应用 recipe → kernel/DT 调整 → WIC/OTA 接线 → `bitbake` → deploy → 厂商烧录工具。

### 1. 主机环境搭建并获取 Poky

**安装构建依赖**（Ubuntu 最常见；其他发行版上包名不同）。对照你所用的 Yocto 发行版核对 **Yocto System Requirements** 页面。

```bash
sudo apt update
sudo apt install -y gawk wget git diffstat unzip texinfo gcc build-essential \
  chrpath socat cpio python3 python3-pip python3-pexpect xz-utils debianutils \
  iputils-ping python3-git python3-jinja2 libegl1-mesa libsdl1.2-dev pylint xterm
```

在匹配你 BSP 的**具名分支或 tag** 上 **clone Poky**（仅为示例分支名）：

```bash
git clone git://git.yoctoproject.org/poky
cd poky
git checkout kirkstone   # example only — use BSP-required revision
```

**启动构建 shell**（创建或使用 `build/`）：

```bash
source oe-init-build-env build
```

从这里开始，像 `../meta-tegra` 这样的路径假定是 `poky/` 旁边的同级目录，而不是在 `build/` 内部。

### 2. 添加 layer（示例栈）

clone **你的 BSP 所记载的依赖**（版本必须匹配）。示意性布局：

```text
work/
  poky/
  meta-openembedded/
  meta-tegra/          # OE4T — check required branch next to Poky
  meta-rauc/
```

在构建目录内部注册 layer：

```bash
bitbake-layers add-layer ../meta-tegra
bitbake-layers add-layer ../meta-openembedded/meta-oe
bitbake-layers add-layer ../meta-openembedded/meta-python
bitbake-layers add-layer ../meta-rauc
```

运行 `bitbake-layers show-layers`，并在继续之前修复 **BBFILE_COLLECTIONS** / 依赖错误。


<details>
<summary>English original</summary>

**6. Must have vs nice to have**

**Must have (production-critical)**

- Boot chain + kernel + DTB for your board
- Camera path (CSI driver + userspace capture stack you will ship)
- Minimum network path (at least Ethernet for many products)
- SSH (or another controlled admin path) if operators need it
- Inference runtime you actually ship (CUDA/TensorRT or lighter stack)
- **OTA client + A/B** if you committed to image OTA
- **systemd** units (or your init policy) for the application

**Nice to have**

- Wi-Fi / BLE if not required on day one
- Docker if you can defer orchestration
- Debug tools (`strace`, `htop`, `tcpdump`) — often a **`debug`** image variant, not the production image
- GUI / Wayland — usually omitted on headless edge cameras

---

**Mindset (what this lab is training)**

- Yocto builds **images** from **declarative metadata**; the question is **what is baked in**, not what you install later on a golden device.
- **Hardware alignment** flows through **MACHINE**, **BSP layers**, **kernel / DT**, and **image recipes**.
- **Reproducibility** means pinned layers, known **DISTRO** policy, and CI that rebuilds from the same inputs.

---

**Bonus: sketch of repo-facing files**

**`conf/local.conf`** (fragment)

```
MACHINE ?= "YOUR_BSP_MACHINE_NAME"
DISTRO ?= "poky"
# Parallelism, download dir, sstate — set per team policy
```

**`conf/bblayers.conf`** (illustrative layer stack)

```
# Bottom-up: core + hardware + OTA + product
# Exact paths and layer names must match your checkout
BBPATH = "${TOPDIR}"
BBFILES ?= ""
```

You will add entries such as `meta`, `meta-poky`, `meta-oe`, **BSP meta**, **meta-rauc** or **meta-mender**, then **`meta-yourproduct`** for your image and bbappends.

**Custom image** (in your product layer), conceptually:

```
inherit core-image
IMAGE_INSTALL:append = "your-runtime-packages"
```

Name the image `yourproduct-image.bb` and `bitbake yourproduct-image` when the stack is wired.

---

**How to implement (workflow)**

The sections above are the **what**. This section is the **how**: a realistic order of operations from empty checkout to flashable artifact. Treat it as a **blueprint**—you will adjust branch names, layer paths, `MACHINE`, and recipe names to match **your** BSP and Yocto release.

**Reality checks**

- **Layer compatibility:** Poky, `meta-openembedded`, `meta-tegra` (OE4T), and `meta-rauc` each target specific Yocto branches. Use the **combination documented** by the BSP (e.g. OE4T manifest or README), not random `git checkout` tags mixed ad hoc.
- **Jetson + RAUC + A/B:** Boot flow and partitioning on Tegra are **BSP-specific**. Expect extra integration (bootloader, signing, slot layout) beyond “add meta-rauc”.
- **CUDA / TensorRT / Docker:** Often require vendor or community layers, license acceptance, and sometimes **not** the same image as a minimal RAUC target. Prove each package with `bitbake -e` / `bitbake <recipe>` before locking the image.

**Pipeline:** host setup → clone Poky → `oe-init-build-env` → add layers → `local.conf` → product layer → image recipe → (optional) app recipe → kernel/DT tweaks → WIC/OTA wiring → `bitbake` → deploy → vendor flash tools.

**1. Host setup and get Poky**

**Install build dependencies** (Ubuntu is common; names differ on other distros). Cross-check the **Yocto System Requirements** page for your release.

```bash
sudo apt update
sudo apt install -y gawk wget git diffstat unzip texinfo gcc build-essential \
  chrpath socat cpio python3 python3-pip python3-pexpect xz-utils debianutils \
  iputils-ping python3-git python3-jinja2 libegl1-mesa libsdl1.2-dev pylint xterm
```

**Clone Poky** at a **named branch or tag** that matches your BSP (example branch name only):

```bash
git clone git://git.yoctoproject.org/poky
cd poky
git checkout kirkstone   # example only — use BSP-required revision
```

**Start a build shell** (creates or uses `build/`):

```bash
source oe-init-build-env build
```

From here, paths like `../meta-tegra` assume sibling directories next to `poky/`, not inside `build/`.

**2. Add layers (example stack)**

Clone **dependencies your BSP documents** (versions must match). Illustrative layout:

```text
work/
  poky/
  meta-openembedded/
  meta-tegra/          # OE4T — check required branch next to Poky
  meta-rauc/
```

Register layers from inside the build directory:

```bash
bitbake-layers add-layer ../meta-tegra
bitbake-layers add-layer ../meta-openembedded/meta-oe
bitbake-layers add-layer ../meta-openembedded/meta-python
bitbake-layers add-layer ../meta-rauc
```

Run `bitbake-layers show-layers` and fix **BBFILE_COLLECTIONS** / dependency errors before continuing.

</details>

### 3. 配置 `conf/local.conf`

编辑 `build/conf/local.conf`。用你的 BSP 中的值**替换** `MACHINE` 和 distro features。

```bash
# Example only — confirm MACHINE with meta-tegra / board docs
MACHINE = "jetson-orin-nano-devkit"

DISTRO_FEATURES:append = " systemd"
VIRTUAL-RUNTIME_init_manager = "systemd"

PACKAGE_CLASSES ?= "package_rpm"

# Optional: reclaim disk during build (tradeoff: harder debug of failed workdirs)
INHERIT += "rm_work"
```

**开发镜像**（可选）：增加调试能力；不要盲目投入生产。

```bash
EXTRA_IMAGE_FEATURES += "debug-tweaks ssh-server-openssh"
```

### 4. 创建产品 layer

```bash
bitbake-layers create-layer ../meta-myproduct
bitbake-layers add-layer ../meta-myproduct
```

典型布局：

```text
meta-myproduct/
├── conf/layer.conf
├── recipes-core/images/
├── recipes-app/
├── wic/                    # if you own custom .wks
└── recipes-kernel/linux/   # bbappends or fragments, if needed
```

### 5. 自定义 image recipe

文件：`meta-myproduct/recipes-core/images/my-edge-image.bb`

```text
SUMMARY = "Edge AI camera root filesystem (example)"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f833b3da"

inherit core-image

# Use :append (Kirkstone+). Only list packages that exist in your configured layers.
IMAGE_INSTALL:append = " \
    python3 \
    openssh-sshd \
"

# Add opencv, gstreamer, docker, rauc, etc. only after bitbake resolves them.
# Example placeholders (names vary by layer):
# IMAGE_INSTALL:append = " opencv gstreamer1.0-plugins-base rauc"
```

构建：

```bash
bitbake my-edge-image
```

### 6. 用 systemd 交付应用（模式）

**Recipe** `meta-myproduct/recipes-app/myapp/myapp_1.0.bb`。把 `app.py` 和 `myapp.service` 放到与 `.bb` 文件相同的目录下（或放在 `files/` 子目录下，遵循你的 layer 的 FILESPATH 约定）。

```text
SUMMARY = "Example AI app service"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f833b3da"

SRC_URI = "file://app.py \
           file://myapp.service \
          "

S = "${WORKDIR}"

RDEPENDS:${PN} += "python3"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/app.py ${D}${bindir}/myapp.py

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/myapp.service ${D}${systemd_system_unitdir}
}

inherit systemd

SYSTEMD_SERVICE:${PN} = "myapp.service"
```

**`files/myapp.service`**

```ini
[Unit]
Description=Example edge AI app

[Service]
ExecStart=/usr/bin/python3 /usr/bin/myapp.py
Restart=always

[Install]
WantedBy=multi-user.target
```

在 image recipe 中把 `myapp` 加入 `IMAGE_INSTALL:append`。

### 7. Kernel 与硬件

- **Menuconfig（受支持时）：** `bitbake virtual/kernel -c menuconfig` —— 有些 BSP 偏好**仅**使用 fragments；遵循厂商文档。
- **Fragments / defconfig：** 优先使用 `*.cfg` fragments 和 `bbappend` 到 `linux-yocto` 或 BSP kernel recipe，而不是手工编辑你无法 rebase 的代码树。
- **设备树：** 在你的 layer 中放置 `.dts`/`.dtsi`；通过 `SRC_URI` + patch 或 BSP 扩展机制应用。

### 8. 存储布局（WIC）

在你的 layer 下放置一个 `.wks`（例如 `meta-myproduct/wic/my-layout.wks`）。**不要**假定下面的分区 stanza 在未经 BSP 对齐的情况下能在 Tegra 上工作。

```bash
part /boot --source bootimg-partition --fstype=vfat --label boot --size 256M
part / --source rootfs --fstype=ext4 --label rootfs_a
part / --source rootfs --fstype=ext4 --label rootfs_b
part /data --fstype=ext4 --size 1024M
```

把 **image** 指向它。在你的 layer 中把 `.wks` 放到 `wic/` 下；然后在 image recipe 中（名称因版本而异）：

```text
WKS_FILE = "my-layout.wks"
IMAGE_FSTYPES:append = " wic"
```

如果 BitBake 找不到该文件，检查 `WKS_SEARCH_PATH` / layer `BBFILE_COLLECTIONS` 以及你所用的 Yocto 版本的文档。

A/B + RAUC 通常需要匹配 **bootloader env**、**RAUC system.conf** 和 **partition UUIDs**——遵循针对你的 SoC 的 `meta-rauc` 集成。

### 9. OTA（RAUC 概要）

- 按 `meta-rauc` 文档为你的分支添加 RAUC **distro** / **image** features。
- 把 RAUC 客户端安装到 image 中（可用时使用 `rauc` package）。
- 在 CI 中定义 **slots**、**keys** 和 **bundle** recipes；对于严肃的部署，签名是必须的。

Mender 通过 `meta-mender` 走一条平行的路线，并带有其自己的分区假设。

### 10. 构建与产物

```bash
bitbake my-edge-image
```

检查 deploy（路径因 `MACHINE` 和 `TMPDIR` 而异）：

```text
tmp/deploy/images/<MACHINE>/
  *.wic
  *.ext4 / tar / other fstypes
  kernel / dtb artifacts (BSP-dependent)
```

### 11. 烧录到硬件

使用该平台的**厂商工具**（对于 Jetson，使用 NVIDIA 的烧录工作流 / BSP 文档——而不是通用的一行命令）。OE4T 和 L4T 记录了镜像落到何处以及如何映射到 `flash.sh` 或等价物。


<details>
<summary>English original</summary>

**3. Configure `conf/local.conf`**

Edit `build/conf/local.conf`. **Replace** `MACHINE` and distro features with values from your BSP.

```bash
# Example only — confirm MACHINE with meta-tegra / board docs
MACHINE = "jetson-orin-nano-devkit"

DISTRO_FEATURES:append = " systemd"
VIRTUAL-RUNTIME_init_manager = "systemd"

PACKAGE_CLASSES ?= "package_rpm"

# Optional: reclaim disk during build (tradeoff: harder debug of failed workdirs)
INHERIT += "rm_work"
```

**Development image** (optional): adds debug affordances; do not ship blindly to production.

```bash
EXTRA_IMAGE_FEATURES += "debug-tweaks ssh-server-openssh"
```

**4. Create the product layer**

```bash
bitbake-layers create-layer ../meta-myproduct
bitbake-layers add-layer ../meta-myproduct
```

Typical layout:

```text
meta-myproduct/
├── conf/layer.conf
├── recipes-core/images/
├── recipes-app/
├── wic/                    # if you own custom .wks
└── recipes-kernel/linux/   # bbappends or fragments, if needed
```

**5. Custom image recipe**

File: `meta-myproduct/recipes-core/images/my-edge-image.bb`

```text
SUMMARY = "Edge AI camera root filesystem (example)"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f833b3da"

inherit core-image

# Use :append (Kirkstone+). Only list packages that exist in your configured layers.
IMAGE_INSTALL:append = " \
    python3 \
    openssh-sshd \
"

# Add opencv, gstreamer, docker, rauc, etc. only after bitbake resolves them.
# Example placeholders (names vary by layer):
# IMAGE_INSTALL:append = " opencv gstreamer1.0-plugins-base rauc"
```

Build:

```bash
bitbake my-edge-image
```

**6. Ship an application with systemd (pattern)**

**Recipe** `meta-myproduct/recipes-app/myapp/myapp_1.0.bb`. Put `app.py` and `myapp.service` in the same directory as the `.bb` file (or under a `files/` subdirectory, following your layer’s FILESPATH convention).

```text
SUMMARY = "Example AI app service"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f833b3da"

SRC_URI = "file://app.py \
           file://myapp.service \
          "

S = "${WORKDIR}"

RDEPENDS:${PN} += "python3"

do_install() {
    install -d ${D}${bindir}
    install -m 0755 ${WORKDIR}/app.py ${D}${bindir}/myapp.py

    install -d ${D}${systemd_system_unitdir}
    install -m 0644 ${WORKDIR}/myapp.service ${D}${systemd_system_unitdir}
}

inherit systemd

SYSTEMD_SERVICE:${PN} = "myapp.service"
```

**`files/myapp.service`**

```ini
[Unit]
Description=Example edge AI app

[Service]
ExecStart=/usr/bin/python3 /usr/bin/myapp.py
Restart=always

[Install]
WantedBy=multi-user.target
```

Add `myapp` to `IMAGE_INSTALL:append` in the image recipe.

**7. Kernel and hardware**

- **Menuconfig (when supported):** `bitbake virtual/kernel -c menuconfig` — some BSPs prefer **only** fragments; follow vendor docs.
- **Fragments / defconfig:** prefer `*.cfg` fragments and `bbappend` to `linux-yocto` or the BSP kernel recipe instead of hand-editing trees you cannot rebase.
- **Device tree:** `.dts`/`.dtsi` in your layer; apply via `SRC_URI` + patch or via BSP extension mechanisms.

**8. Storage layout (WIC)**

Place a `.wks` under your layer (e.g. `meta-myproduct/wic/my-layout.wks`). **Do not** assume the partition stanza below works on Tegra without BSP alignment.

```bash
part /boot --source bootimg-partition --fstype=vfat --label boot --size 256M
part / --source rootfs --fstype=ext4 --label rootfs_a
part / --source rootfs --fstype=ext4 --label rootfs_b
part /data --fstype=ext4 --size 1024M
```

Point the **image** at it. Put the `.wks` under `wic/` in your layer; then in the image recipe (names vary by release):

```text
WKS_FILE = "my-layout.wks"
IMAGE_FSTYPES:append = " wic"
```

If BitBake cannot find the file, check `WKS_SEARCH_PATH` / layer `BBFILE_COLLECTIONS` and the docs for your Yocto version.

A/B + RAUC usually requires matching **bootloader env**, **RAUC system.conf**, and **partition UUIDs**—follow `meta-rauc` integration for your SoC.

**9. OTA (RAUC outline)**

- Add RAUC **distro** / **image** features per `meta-rauc` documentation for your branch.
- Install the RAUC client into the image (`rauc` package when available).
- Define **slots**, **keys**, and **bundle** recipes in CI; signing is mandatory for serious deployments.

Mender follows a parallel path via `meta-mender` with its own partition assumptions.

**10. Build and artifacts**

```bash
bitbake my-edge-image
```

Inspect deploy (path varies by `MACHINE` and `TMPDIR`):

```text
tmp/deploy/images/<MACHINE>/
  *.wic
  *.ext4 / tar / other fstypes
  kernel / dtb artifacts (BSP-dependent)
```

**11. Flash to hardware**

Use **vendor tools** for the platform (for Jetson, NVIDIA’s flashing workflow / BSP docs—not a generic one-liner). OE4T and L4T document where images land and how they map to `flash.sh` or equivalent.

</details>

### 传统发行版 vs Yocto（工程视角）

| 传统 Linux | Yocto |
|-------------------|--------|
| 启动后再安装软件包 | 构建时决定镜像内容 |
| 手工的雪花式配置 | 自动化、可评审的元数据 |
| 难以复现 | 借助固定 layer 与缓存策略实现可复现 |

### 本路线图中可深入的方向

- **构建失败与日志：** [Lecture 11 — Debugging builds](Lecture-11.md)
- **性能 / sstate / CI：** [Lecture 13 — Performance, caching, and CI](Lecture-13.md)
- **bbappend 规范：** [Lecture 7 — Layers](Lecture-07.md)
- **Jetson Yocto 生产：** 阶段 4 方向 B — *Orin-Nano-Yocto-BSP-Production*

---

## 本路线图的后续步骤

1. 继续 [Lecture 4 — Architecture](Lecture-04.md)（layer 与配置）。
2. 进入 Jetson 专用 BSP 的深入阶段时，把 **阶段 4 方向 B** 的 Yocto 生产材料与本文档配合使用。

---

**上一讲：** [Lecture 3](Lecture-03.md) | **下一讲：** [Lecture 4](Lecture-04.md)


<details>
<summary>English original</summary>

**Traditional distro vs Yocto (engineering view)**

| Traditional Linux | Yocto |
|-------------------|--------|
| Install packages after boot | Decide image contents at build time |
| Manual snowflake setup | Automated, reviewable metadata |
| Hard to reproduce | Reproducible with pinned layers and cache policy |

**Where to go deeper on this roadmap**

- **Failed builds and logs:** [Lecture 11 — Debugging builds](Lecture-11.md)
- **Performance / sstate / CI:** [Lecture 13 — Performance, caching, and CI](Lecture-13.md)
- **bbappend discipline:** [Lecture 7 — Layers](Lecture-07.md)
- **Jetson Yocto production:** Phase 4 Track B — *Orin-Nano-Yocto-BSP-Production*

---

**Next steps on this roadmap**

1. Continue [Lecture 4 — Architecture](Lecture-04.md) (layers and configs).
2. When you move to Jetson-specific BSP depth, use **Phase 4 Track B** Yocto production material alongside this spec.

---

**Previous:** [Lecture 3](Lecture-03.md) | **Next:** [Lecture 4](Lecture-04.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Yocto/Lecture/Lab-01-Worked-Example-Edge-AI-Camera.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Yocto/Lecture/Lab-01-Worked-Example-Edge-AI-Camera.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
