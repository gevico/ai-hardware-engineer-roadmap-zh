---
title: 安全与 OTA
description: 安全与 OTA
---

# 安全与 OTA

<div class="course-identity security-ota" markdown="1">
<div class="course-identity__icon">SEC</div>
<div markdown="1">
<p class="course-identity__eyebrow">方向 B6 · 安全与 OTA</p>
<p class="course-identity__title">通过安全启动、签名产物、更新策略和回滚行为来加固 Jetson 产品。</p>
<p class="course-identity__meta">产物：安全更新计划 · 度量：回滚时间、完整性检查、故障恢复</p>
</div>
</div>


**阶段 4 — 方向 B — Nvidia Jetson** · 模块 6 / 7

> **重点：** 为现场部署加固一台 **Jetson Orin Nano 8GB** 产品：**安全启动**（PKC/SBK 熔丝烧录）、**OP-TEE** 可信执行、**磁盘加密**、**A/B rootfs 冗余**，以及一条完整的 **OTA 更新流水线**，涵盖回滚、监控与可靠性测试。
>
> **主硬件：** 定制载板或开发套件载板上的 Jetson Orin Nano 8GB

**上一章：** [5. 应用开发](../05-应用开发/Guide.md) · **下一章：** [7. 合规与制造](../07-合规与制造/Guide.md)

---


## 1. 为什么安全与 OTA 是同一个模块

在生产级 Jetson 产品中，安全与 OTA 紧密耦合：

- **安全启动**确保只有经过授权的固件和 kernel 能在设备上运行
- **OTA 通道**必须签名，攻击者才能无法推送恶意更新
- **磁盘加密**在设备被物理窃取时保护静态数据
- **A/B 冗余**让 OTA 变得安全——更新失败时回滚到上一个已知良好的槽位
- **熔丝烧录**不可逆，必须在量产烧录前做好规划

本模块把此前分散在模块 1 各深度子指南中的材料汇集起来，组织成一条**生产决策流程**：做什么、按什么顺序做、以及为什么。

> **深度配套材料（模块 1）：** 下列子指南包含实现层细节，本模块引用它们但不再重复：
> - [Orin Nano 安全](../01-Nvidia-Jetson平台/13-Orin-Nano安全/Guide.md) — 威胁模型、攻击面、详细的 PKC/SBK 演练
> - [Orin Nano OTA 深度解析](../01-Nvidia-Jetson平台/08-Orin-Nano-OTA深入解析/Guide.md) — NVIDIA `nv_update_engine`、槽位管理内部机制
> - [Orin Nano Rootfs 与 A/B 冗余](../01-Nvidia-Jetson平台/12-Orin-Nano-Rootfs与AB冗余/Guide.md) — 分区布局、`nvbootctrl`、槽位切换

---

## 2. 已部署 Jetson 产品的威胁模型

在加固之前，先定义你要防范的是什么：

| 威胁 | 示例 | 缓解措施 |
|--------|---------|------------|
| **固件篡改** | 攻击者通过物理接触替换 bootloader | 安全启动（PKC）——硬件信任根 |
| **数据窃取** | 设备被盗，NVMe 被拆下读取 | 磁盘加密（LUKS + OP-TEE 密钥） |
| **未授权更新** | 恶意 OTA 被推送到设备 | 签名更新镜像 + TLS 通道 |
| **网络入侵** | 攻击者利用设备上的开放端口 | 防火墙、最小化服务、仅密钥 SSH |
| **权限提升** | 应用容器逃逸到宿主机 | AppArmor/SELinux、最小 rootfs、禁止 root SSH |
| **供应链** | 假冒模块或篡改过的固件镜像 | 基于熔丝的设备身份、签名出厂镜像 |

---

## 3. 安全启动——PKC 与 SBK

### 安全启动链

Jetson Orin Nano（T234）支持基于熔丝的**硬件信任根**：

```
BootROM (silicon)
  │  Verifies MB1 signature using PKC hash in fuses
  │
MB1 (QSPI)
  │  Verifies MB2 using PKC chain
  │
MB2 (QSPI)
  │  Verifies UEFI, OP-TEE, kernel
  │
UEFI → Linux kernel → userspace
```

### PKC（公钥密码学）

- 生成 RSA-2048 或 RSA-3072 密钥对
- 将**公钥哈希**烧录进 OTP 熔丝
- 所有固件镜像都用私钥签名
- BootROM 在每一级验证签名链

```bash
# Generate PKC key pair
openssl genrsa -out pkc.pem 3072

# Extract public key
openssl rsa -in pkc.pem -pubout -out pkc_pub.pem
```

### SBK（对称绑定密钥）

- 128 位 AES 密钥烧录进 OTP 熔丝
- 除签名外还对固件镜像加密
- 即使能物理接触 QSPI 也无法读取固件
- 与 PKC 结合：镜像既被加密又被签名

---

## 4. 熔丝烧录工作流

**熔丝烧录不可逆。** 熔丝一旦烧录，设备将永久要求签名/加密的固件。

### 实验室工作流（测试密钥）

1. 生成**测试** PKC 和 SBK 密钥（明确标注，与生产密钥分开存放）
2. 使用签名镜像烧录并验证设备可正常工作
3. 仅在**测试机**上烧录熔丝
4. 验证已烧录熔丝的设备能用签名镜像启动，并拒绝未签名镜像
5. 在整条启动/OTA 链验证通过之前，**不要**对量产机烧录熔丝


<details>
<summary>English original</summary>

**Security and OTA**

<div class="course-identity security-ota" markdown="1">
<div class="course-identity__icon">SEC</div>
<div markdown="1">
<p class="course-identity__eyebrow">Track B6 · Security & OTA</p>
<p class="course-identity__title">Harden Jetson products with secure boot, signed artifacts, update strategy, and rollback behavior.</p>
<p class="course-identity__meta">Artifact: secure update plan · Measure: rollback time, integrity checks, failure recovery</p>
</div>
</div>


**Phase 4 — Track B — Nvidia Jetson** · Module 6 of 7

> **Focus:** Harden a **Jetson Orin Nano 8GB** product for field deployment: **secure boot** (PKC/SBK fuse programming), **OP-TEE** trusted execution, **disk encryption**, **A/B rootfs redundancy**, and a complete **OTA update pipeline** with rollback, monitoring, and reliability testing.
>
> **Primary hardware:** Jetson Orin Nano 8GB on custom or dev-kit carrier

**Previous:** [5. Application Development](../05-应用开发/Guide.md) · **Next:** [7. Compliance and Manufacturing](../07-合规与制造/Guide.md)

---


**1. Why security and OTA are a single module**

Security and OTA are tightly coupled in production Jetson products:

- **Secure boot** ensures only authorized firmware and kernel run on the device
- **OTA channels** must be signed so an attacker cannot push malicious updates
- **Disk encryption** protects data at rest if the device is physically stolen
- **A/B redundancy** makes OTA safe — a failed update rolls back to the last known-good slot
- **Fuse programming** is irreversible and must be planned before production flashing

This module brings together material that was previously split across deep-dive sub-guides in Module 1 and organizes it as a **production decision flow**: what to do, in what order, and why.

> **Deep-dive companions (Module 1):** The sub-guides below contain implementation-level detail that this module references but does not duplicate:
> - [Orin Nano Security](../01-Nvidia-Jetson平台/13-Orin-Nano安全/Guide.md) — threat model, attack surfaces, detailed PKC/SBK walkthrough
> - [Orin Nano OTA Deep Dive](../01-Nvidia-Jetson平台/08-Orin-Nano-OTA深入解析/Guide.md) — NVIDIA `nv_update_engine`, slot management internals
> - [Orin Nano Rootfs and A/B Redundancy](../01-Nvidia-Jetson平台/12-Orin-Nano-Rootfs与AB冗余/Guide.md) — partition layout, `nvbootctrl`, slot switching

---

**2. Threat model for deployed Jetson products**

Before hardening, define what you're protecting against:

| Threat | Example | Mitigation |
|--------|---------|------------|
| **Firmware tampering** | Attacker replaces bootloader via physical access | Secure boot (PKC) — hardware root of trust |
| **Data theft** | Device stolen, NVMe removed and read | Disk encryption (LUKS + OP-TEE key) |
| **Unauthorized updates** | Malicious OTA pushed to device | Signed update images + TLS channel |
| **Network intrusion** | Attacker exploits open port on device | Firewall, minimal services, SSH key-only |
| **Privilege escalation** | Application container escapes to host | AppArmor/SELinux, minimal rootfs, no root SSH |
| **Supply chain** | Counterfeit module or tampered firmware image | Fuse-based device identity, signed factory images |

---

**3. Secure boot — PKC and SBK**

**Secure boot chain**

Jetson Orin Nano (T234) supports a **hardware root of trust** using fuses:

```
BootROM (silicon)
  │  Verifies MB1 signature using PKC hash in fuses
  │
MB1 (QSPI)
  │  Verifies MB2 using PKC chain
  │
MB2 (QSPI)
  │  Verifies UEFI, OP-TEE, kernel
  │
UEFI → Linux kernel → userspace
```

**PKC (Public Key Cryptography)**

- Generate an RSA-2048 or RSA-3072 key pair
- Burn the **public key hash** into OTP fuses
- All firmware images are signed with the private key
- BootROM verifies the signature chain at every stage

```bash
# Generate PKC key pair
openssl genrsa -out pkc.pem 3072

# Extract public key
openssl rsa -in pkc.pem -pubout -out pkc_pub.pem
```

**SBK (Symmetric Binding Key)**

- 128-bit AES key burned into OTP fuses
- Encrypts firmware images in addition to signing them
- Prevents reading firmware even with physical access to QSPI
- Combined with PKC: images are encrypted AND signed

---

**4. Fuse programming workflow**

**Fuse programming is irreversible.** Once fuses are burned, the device permanently requires signed/encrypted firmware.

**Lab workflow (test keys)**

1. Generate **test** PKC and SBK keys (clearly labeled, stored separately from production keys)
2. Flash and validate the device works with signed images
3. Burn fuses on a **test unit only**
4. Verify the fused device boots with signed images and rejects unsigned images
5. **Do not** fuse production units until the entire boot/OTA chain is validated

</details>

### 生产工作流

1. 生成 **生产** PKC 与 SBK 密钥
2. 将私钥存储在 HSM（Hardware Security Module）中，或至少存放在加密且受访问控制的保险库中
3. 将密钥签名集成到 CI/CD 构建流水线中
4. 在工厂烧录环节烧写 fuse (见 [Module 8 — Compliance and Manufacturing](../07-合规与制造/Guide.md))

### fuse 烧写命令

```bash
sudo ./odmfuse.sh -i <board_id> -k pkc.pem -S sbk.key <board_config>
```

### 验证 fuse 状态

```bash
# On target
cat /sys/devices/platform/tegra-fuse/odm_production_mode
# 1 = fused (production mode)
```

---

## 5. OP-TEE 与可信执行

OP-TEE 在 Cortex-A78AE 核上提供 **可信执行环境（TEE）**：

- **安全世界**运行 OP-TEE OS，与 **普通世界** Linux 并存
- **可信应用（TA）**在安全世界中执行
- 使用场景：密钥存储、远程证明、DRM、安全传感器处理

### Jetson OP-TEE 集成

NVIDIA 将 OP-TEE 作为 L4T BSP 的一部分提供。它在启动链中被加载（MB2 在 UEFI 之前加载 OP-TEE）。

| 组件 | 位置 |
|-----------|----------|
| OP-TEE OS 镜像 | 位于 flash 分区的 `tos-optee_t234.img` |
| 可信应用 | 内置于 OP-TEE 镜像中，或在 runtime 加载 |
| TEE 客户端库 | `libteec.so`（位于 rootfs 中） |
| TEE supplicant | `tee-supplicant` 守护进程 |

### 用 OP-TEE 存储密钥

将磁盘加密密钥存放在 OP-TEE 安全存储中，而不是以明文形式放在文件系统上：

1. OP-TEE 生成或接收 LUKS 密钥
2. 密钥存储在 OP-TEE 安全存储中（RPMB 或加密文件）
3. 启动时，OP-TEE 取出密钥并传给 kernel 以解锁 LUKS
4. 密钥绝不会出现在普通世界的内存中

---

## 6. 磁盘加密（LUKS + OP-TEE 密钥存储）

### 为何要加密

部署在现场的设备可能被物理接触。没有磁盘加密，攻击者可以：

- 拆下 NVMe SSD，在另一台机器上读取全部数据
- 提取 ML 模型、凭据、客户数据
- 通过复制文件系统克隆设备

### 在 Jetson 上配置 LUKS

```bash
# Create encrypted partition
sudo cryptsetup luksFormat /dev/nvme0n1p1

# Open (unlock) the partition
sudo cryptsetup open /dev/nvme0n1p1 crypt_root

# Format and mount
sudo mkfs.ext4 /dev/mapper/crypt_root
sudo mount /dev/mapper/crypt_root /mnt
```

### 用 OP-TEE 自动解锁

对于无头设备，手动输入口令并不现实。用 OP-TEE 在启动时自动解锁：

1. 在工厂配置阶段将 LUKS 密钥存入 OP-TEE 安全存储
2. 启动时，initrd 脚本调用 OP-TEE 客户端取出密钥
3. 密钥解锁 LUKS 分区
4. 启动后无法从普通世界访问该密钥

### 性能影响

磁盘加密会为 I/O 增加 CPU 开销：

| 工作负载 | 不加密 | 使用 LUKS（AES-256-XTS） | 开销 |
|----------|-------------------|------------------------|----------|
| 顺序读 | ~3.2 GB/s | ~2.4 GB/s | ~25% |
| 顺序写 | ~2.8 GB/s | ~2.1 GB/s | ~25% |
| 随机 4K 读 | ~180K IOPS | ~140K IOPS | ~22% |

Cortex-A78AE 具备 ARMv8 加密扩展，因此 AES 有硬件加速。对大多数边缘 AI 工作负载来说，瓶颈是 GPU 推理而非存储 I/O，这样的开销可以接受。

---

## 7. runtime 加固

### 服务最小化

```bash
# List all enabled services
systemctl list-unit-files --state=enabled

# Disable unnecessary services
sudo systemctl disable bluetooth
sudo systemctl disable avahi-daemon
sudo systemctl disable cups
sudo systemctl mask <service>  # prevent re-enabling
```

### SSH 加固

```
# /etc/ssh/sshd_config
PermitRootLogin no
PasswordAuthentication no
PubkeyAuthentication yes
AllowUsers deploy
MaxAuthTries 3
```

### 防火墙

```bash
sudo ufw default deny incoming
sudo ufw default allow outgoing
sudo ufw allow ssh
sudo ufw allow 443/tcp   # OTA update channel
sudo ufw enable
```

### AppArmor

JetPack 自带 AppArmor 支持。为应用容器和服务创建 profile，以限制文件访问、网络访问和 capabilities。

### 生产环境中的调试 UART

在生产设备上**禁用或限制**调试 UART。暴露的 UART 会提供 root shell 访问权限。可选方案：

- 从生产载板 PCB 上移除调试 UART 排针（推荐）
- 在内核命令行中禁用 UART 控制台（将 `console=` 设为 `ttyS0`，且不启用 getty）
- 用密码保护（安全性较低，仍会暴露启动日志）

---

## 8. A/B rootfs 冗余架构

A/B rootfs 支持安全更新：把新镜像写入非活动槽位，验证后再切换。


<details>
<summary>English original</summary>

**Production workflow**

1. Generate **production** PKC and SBK keys
2. Store private key in an HSM (Hardware Security Module) or at minimum an encrypted, access-controlled vault
3. Integrate key signing into the CI/CD build pipeline
4. Program fuses as part of factory flashing (see [Module 8 — Compliance and Manufacturing](../07-合规与制造/Guide.md))

**Fuse burning command**

```bash
sudo ./odmfuse.sh -i <board_id> -k pkc.pem -S sbk.key <board_config>
```

**Verifying fuse state**

```bash
# On target
cat /sys/devices/platform/tegra-fuse/odm_production_mode
# 1 = fused (production mode)
```

---

**5. OP-TEE and trusted execution**

OP-TEE provides a **Trusted Execution Environment (TEE)** on the Cortex-A78AE cores:

- **Secure world** runs OP-TEE OS alongside **normal world** Linux
- **Trusted Applications (TAs)** execute in the secure world
- Use cases: key storage, attestation, DRM, secure sensor processing

**Jetson OP-TEE integration**

NVIDIA ships OP-TEE as part of the L4T BSP. It is loaded during the boot chain (MB2 loads OP-TEE before UEFI).

| Component | Location |
|-----------|----------|
| OP-TEE OS image | `tos-optee_t234.img` in flash partition |
| Trusted Applications | Built into the OP-TEE image or loaded at runtime |
| TEE client library | `libteec.so` (in rootfs) |
| TEE supplicant | `tee-supplicant` daemon |

**Key storage with OP-TEE**

Store disk encryption keys in OP-TEE secure storage rather than in plaintext on the filesystem:

1. OP-TEE generates or receives the LUKS key
2. Key is stored in OP-TEE secure storage (RPMB or encrypted file)
3. At boot, OP-TEE retrieves the key and passes it to the kernel for LUKS unlock
4. The key never appears in normal-world memory

---

**6. Disk encryption (LUKS + OP-TEE key storage)**

**Why encrypt**

Devices deployed in the field can be physically accessed. Without disk encryption, an attacker can:

- Remove the NVMe SSD and read all data on a different machine
- Extract ML models, credentials, customer data
- Clone the device by copying the filesystem

**LUKS setup on Jetson**

```bash
# Create encrypted partition
sudo cryptsetup luksFormat /dev/nvme0n1p1

# Open (unlock) the partition
sudo cryptsetup open /dev/nvme0n1p1 crypt_root

# Format and mount
sudo mkfs.ext4 /dev/mapper/crypt_root
sudo mount /dev/mapper/crypt_root /mnt
```

**Auto-unlock with OP-TEE**

For headless devices, manual passphrase entry is not practical. Use OP-TEE to auto-unlock at boot:

1. Store the LUKS key in OP-TEE secure storage during factory provisioning
2. At boot, an initrd script calls the OP-TEE client to retrieve the key
3. The key unlocks the LUKS partition
4. The key is not accessible from normal-world after boot

**Performance impact**

Disk encryption adds CPU overhead for I/O:

| Workload | Without encryption | With LUKS (AES-256-XTS) | Overhead |
|----------|-------------------|------------------------|----------|
| Sequential read | ~3.2 GB/s | ~2.4 GB/s | ~25% |
| Sequential write | ~2.8 GB/s | ~2.1 GB/s | ~25% |
| Random 4K read | ~180K IOPS | ~140K IOPS | ~22% |

The Cortex-A78AE has ARMv8 crypto extensions, so AES is hardware-accelerated. The overhead is acceptable for most edge AI workloads where the bottleneck is GPU inference, not storage I/O.

---

**7. Runtime hardening**

**Service minimization**

```bash
# List all enabled services
systemctl list-unit-files --state=enabled

# Disable unnecessary services
sudo systemctl disable bluetooth
sudo systemctl disable avahi-daemon
sudo systemctl disable cups
sudo systemctl mask <service>  # prevent re-enabling
```

**SSH hardening**

```
# /etc/ssh/sshd_config
PermitRootLogin no
PasswordAuthentication no
PubkeyAuthentication yes
AllowUsers deploy
MaxAuthTries 3
```

**Firewall**

```bash
sudo ufw default deny incoming
sudo ufw default allow outgoing
sudo ufw allow ssh
sudo ufw allow 443/tcp   # OTA update channel
sudo ufw enable
```

**AppArmor**

JetPack ships with AppArmor support. Create profiles for your application containers and services to restrict file access, network access, and capabilities.

**Debug UART in production**

**Disable or restrict** the debug UART on production devices. An exposed UART gives root shell access. Options:

- Remove the debug UART header from the production carrier PCB (recommended)
- Disable UART console in the kernel command line (`console=` set to `ttyS0` with no getty)
- Protect with password (less secure, still exposes boot log)

---

**8. A/B rootfs redundancy architecture**

A/B rootfs allows safe updates: write the new image to the inactive slot, validate, then switch.

</details>

### 分区布局

```
QSPI NOR (boot):
  ├─ mb1_a / mb1_b     (A/B bootloader stage 1)
  ├─ mb2_a / mb2_b     (A/B bootloader stage 2)
  ├─ uefi_a / uefi_b   (A/B UEFI)
  ├─ tos_a / tos_b     (A/B OP-TEE)
  └─ bpmp_a / bpmp_b   (A/B BPMP firmware)

NVMe:
  ├─ APP_a              (rootfs slot A)
  ├─ APP_b              (rootfs slot B)
  └─ DATA               (persistent user data, not duplicated)
```

### slot 管理

```bash
# Check active slot
sudo nvbootctrl get-current-slot

# Set next boot slot
sudo nvbootctrl set-active-boot-slot <0|1>

# Mark current slot as successful (after validation)
sudo nvbootctrl set-slot-as-successful <0|1>
```

### 容量权衡

A/B 会使 rootfs 存储需求翻倍。对于 4 GB 的 rootfs：

- 单 slot：4 GB
- A/B：8 GB + 约 1 GB 开销 = 约 9 GB
- 加上持久化 data 分区：9 GB + DATA 大小

据此规划 NVMe 容量。128 GB 的 NVMe 空间充裕；32 GB 的 eMMC 可能吃紧。

> **深入阅读：** [Orin Nano Rootfs and A/B Redundancy](../01-Nvidia-Jetson平台/12-Orin-Nano-Rootfs与AB冗余/Guide.md)

---

## 9. OTA 流水线设计

### 端到端架构

```
Build server (CI/CD)
  │  Build rootfs image, sign with PKC/OTA key
  │
Staging server
  │  Host signed images, manage release channels
  │  (stable, beta, canary)
  │
  ├─ TLS ─────────────────────────────────────────┐
  │                                                │
Device agent                                       │
  │  Poll for updates or receive push notification │
  │  Download image, verify signature              │
  │  Write to inactive slot                        │
  │  Reboot into new slot                          │
  │  Run validation tests                          │
  │  Mark slot as successful (or rollback)         │
  │  Report status back to server                  │
  └────────────────────────────────────────────────┘
```

### 发布通道策略

| 通道 | 用途 | 推送范围 |
|---------|---------|---------|
| **canary** | 内部测试、每夜构建 | 1–5 台设备 |
| **beta** | 早期采用者或预发布设备群 | 设备群的 5–10% |
| **stable** | 生产环境 | 全量设备群（分阶段推送） |

---

## 10. Jetson 上的 OTA 框架（SWUpdate、Mender、RAUC）

| 框架 | 优势 | Jetson 集成 |
|-----------|-----------|-------------------|
| **SWUpdate** | 灵活、可脚本化、支持双副本与单副本模式、签名镜像、Lua handler | 良好 — 可与 `nvbootctrl` 集成完成 slot 管理 |
| **Mender** | 托管云服务、设备看板、设备群管理 | 良好 — Mender Hub 提供 Jetson 示例 |
| **RAUC** | D-Bus API、slot 状态跟踪、bundle 签名 | 良好 — RAUC bundle 可针对 Jetson A/B 配置 |
| **NVIDIA nv_update_engine** | Jetson 原生工具、分区级更新 | 内置，但设备群管理能力有限 |

### 推荐方案

使用 **SWUpdate** 或 **Mender** 作为设备侧 agent，配合 **NVIDIA 的分区工具**（`nvbootctrl`、`nv_update_engine`）完成 bootloader 与 slot 操作。由此可兼得生态标准的 OTA 功能与 Jetson 特有的启动集成。

---

## 11. bootloader 与固件 OTA

在现场更新驻留于 QSPI 的固件（MB1、MB2、UEFI、OP-TEE、BPMP、SPE）：

- 利用 QSPI 的 A/B slot 支持 — 将新固件写入非活动 slot
- NVIDIA 的 `nv_update_engine` 负责分区级写入
- **风险：** 若未妥善进行 A/B 管理，bootloader 更新失败可能使设备变砖
- **缓解措施：** 始终先更新 bootloader 再更新 rootfs；在切换 rootfs slot 前验证 bootloader 可正常启动

---

## 12. 容器层 OTA

对于运行在 Docker 容器中的应用（见模块 2 — L4T 定制）：

- 容器镜像的更新独立于 rootfs
- 使用私有容器 registry（Harbor、AWS ECR 或本地）
- 拉取新镜像，停止旧容器，启动新容器
- **回滚：** 保留上一个镜像 tag；`docker rollback` 或使用旧 tag 重启

### layer 缓存

Docker layer 缓存可减小下载体积 — 只拉取发生变化的 layer。按如下方式组织 Dockerfile：

1. 基础 L4T 镜像（体积大、很少变更）作为最底部的 layer
2. 系统依赖（偶尔变更）放在中间的 layer
3. 应用代码（频繁变更）放在顶部的 layer

---

## 13. 增量更新与带宽工程

对于使用蜂窝网络或带宽受限网络的设备：

| 技术 | 节省 | 复杂度 |
|-----------|---------|------------|
| **完整镜像** | 0%（基线） | 低 |
| **bsdiff / zstd delta** | 60–90% | 中 — 需为每个版本对生成 delta |
| **casync**（内容寻址分块） | 70–95% | 中 — chunk 存于服务端，仅下载新增 chunk |
| **容器 layer diff** | 80–95%（应用更新场景） | 低 — Docker 内置 |


<details>
<summary>English original</summary>

**Partition layout**

```
QSPI NOR (boot):
  ├─ mb1_a / mb1_b     (A/B bootloader stage 1)
  ├─ mb2_a / mb2_b     (A/B bootloader stage 2)
  ├─ uefi_a / uefi_b   (A/B UEFI)
  ├─ tos_a / tos_b     (A/B OP-TEE)
  └─ bpmp_a / bpmp_b   (A/B BPMP firmware)

NVMe:
  ├─ APP_a              (rootfs slot A)
  ├─ APP_b              (rootfs slot B)
  └─ DATA               (persistent user data, not duplicated)
```

**Slot management**

```bash
# Check active slot
sudo nvbootctrl get-current-slot

# Set next boot slot
sudo nvbootctrl set-active-boot-slot <0|1>

# Mark current slot as successful (after validation)
sudo nvbootctrl set-slot-as-successful <0|1>
```

**Sizing trade-offs**

A/B doubles the rootfs storage requirement. For a 4 GB rootfs:

- Single slot: 4 GB
- A/B: 8 GB + ~1 GB overhead = ~9 GB
- With persistent data partition: 9 GB + DATA size

Plan NVMe capacity accordingly. A 128 GB NVMe gives ample room; a 32 GB eMMC may be tight.

> **Deep dive:** [Orin Nano Rootfs and A/B Redundancy](../01-Nvidia-Jetson平台/12-Orin-Nano-Rootfs与AB冗余/Guide.md)

---

**9. OTA pipeline design**

**End-to-end architecture**

```
Build server (CI/CD)
  │  Build rootfs image, sign with PKC/OTA key
  │
Staging server
  │  Host signed images, manage release channels
  │  (stable, beta, canary)
  │
  ├─ TLS ─────────────────────────────────────────┐
  │                                                │
Device agent                                       │
  │  Poll for updates or receive push notification │
  │  Download image, verify signature              │
  │  Write to inactive slot                        │
  │  Reboot into new slot                          │
  │  Run validation tests                          │
  │  Mark slot as successful (or rollback)         │
  │  Report status back to server                  │
  └────────────────────────────────────────────────┘
```

**Release channel strategy**

| Channel | Purpose | Rollout |
|---------|---------|---------|
| **canary** | Internal testing, nightly builds | 1–5 devices |
| **beta** | Early adopter or staging fleet | 5–10% of fleet |
| **stable** | Production | 100% of fleet (phased rollout) |

---

**10. OTA frameworks on Jetson (SWUpdate, Mender, RAUC)**

| Framework | Strengths | Jetson integration |
|-----------|-----------|-------------------|
| **SWUpdate** | Flexible, scriptable, double-copy and single-copy modes, signed images, Lua handlers | Good — integrates with `nvbootctrl` for slot management |
| **Mender** | Managed cloud service, device dashboard, fleet management | Good — Mender Hub has Jetson examples |
| **RAUC** | D-Bus API, slot status tracking, bundle signing | Good — RAUC bundles can be configured for Jetson A/B |
| **NVIDIA nv_update_engine** | Native Jetson tool, partition-level updates | Built-in but limited fleet management |

**Recommended approach**

Use **SWUpdate** or **Mender** as the device-side agent, with **NVIDIA's partition tools** (`nvbootctrl`, `nv_update_engine`) for bootloader and slot operations. This gives you both ecosystem-standard OTA features and Jetson-specific boot integration.

---

**11. Bootloader and firmware OTA**

Updating QSPI-resident firmware (MB1, MB2, UEFI, OP-TEE, BPMP, SPE) in the field:

- Use A/B slot support in QSPI — write new firmware to the inactive slot
- NVIDIA's `nv_update_engine` handles partition-level writes
- **Risk:** A failed bootloader update can brick the device if not properly A/B managed
- **Mitigation:** Always update bootloader before rootfs; validate bootloader boots before switching rootfs slot

---

**12. Container-layer OTA**

For applications running in Docker containers (as set up in Module 2 — L4T Customization):

- Update container images independently of the rootfs
- Use a private container registry (Harbor, AWS ECR, or local)
- Pull new images, stop old container, start new container
- **Rollback:** Keep the previous image tag; `docker rollback` or restart with old tag

**Layer caching**

Docker layer caching reduces download size — only changed layers are pulled. Structure your Dockerfile so that:

1. Base L4T image (large, changes rarely) is the bottom layer
2. System dependencies (changes occasionally) in middle layers
3. Application code (changes frequently) in the top layer

---

**13. Delta updates and bandwidth engineering**

For devices on cellular or bandwidth-constrained networks:

| Technique | Savings | Complexity |
|-----------|---------|------------|
| **Full image** | 0% (baseline) | Low |
| **bsdiff / zstd delta** | 60–90% | Medium — requires generating deltas per version pair |
| **casync** (content-addressable chunks) | 70–95% | Medium — chunk store on server, only new chunks downloaded |
| **Container layer diff** | 80–95% (for app updates) | Low — built into Docker |

</details>

### 实用建议

- 初始设备群（<100 台设备）使用 **完整 rootfs 镜像**——更简单，基础设施更少
- 当带宽成本变得显著时，加入 **增量更新**
- 应用变更使用 **容器层更新**（最常见的更新类型）

---

## 14. 回滚与掉电安全

### OTA 期间掉电

若更新过程中发生掉电：

| 阶段 | 掉电结果 | 恢复 |
|-------|-------------------|----------|
| 下载镜像 | 下载不完整 | 下次启动时续传或重新下载 |
| 写入非活动槽位 | 槽位写入不完整 | 非活动槽位损坏，但 **活动槽位未被触碰**——设备正常启动 |
| 重启进入新槽位 | 启动中断 | 看门狗或重试计数器触发回滚到前一槽位 |
| 校验运行中 | 校验未完成 | 槽位未标记为成功——下次重启回退 |

### 看门狗定时器

配置硬件看门狗，当系统在更新后校验期间挂起时触发重启：

```bash
# Enable watchdog
echo 1 > /dev/watchdog

# Application must pet the watchdog periodically
# If it fails to pet within timeout → hardware reset → rollback
```

### 重试计数器

使用 `nvbootctrl` 中的启动计数器，或自定义持久化标志：

1. 切换到新槽位前，将重试计数器设为 3
2. 每次启动尝试将计数器递减
3. 若计数器归零而槽位仍未标记为成功 → 回退到旧槽位

---

## 15. OTA 签名与校验链

```
Build server:
  ├─ Build rootfs image
  ├─ Compute SHA-256 hash of image
  ├─ Sign hash with OTA signing key (RSA-3072 or Ed25519)
  └─ Bundle: image + signature + metadata (version, channel, timestamp)

Device:
  ├─ Download bundle
  ├─ Verify signature using embedded OTA public key
  ├─ Verify hash matches image
  ├─ Check version > current (prevent downgrade)
  └─ Apply update
```

### 密钥管理

- **OTA 签名密钥**与 **安全启动 PKC 密钥**相互独立
- 将 OTA 私钥存放于 CI/CD secrets 或 HSM
- 将 OTA 公钥嵌入 rootfs（或只读分区）
- 轮换 OTA 密钥时，先通过签名更新下发新公钥，再行切换

---

## 16. 设备群监控与遥测

| 指标 | 来源 | 用途 |
|--------|--------|---------|
| **启动次数** | 持久化计数器 | 检测重启循环 |
| **更新状态** | OTA agent | 跟踪推送成功率/失败率 |
| **槽位状态** | `nvbootctrl` | 检测滞留在旧槽位的设备 |
| **温度** | `tegrastats` | 检测现场热管理问题 |
| **磁盘使用量** | `df` | 防止设备空间耗尽 |
| **运行时长** | `/proc/uptime` | 检测意外重启 |
| **应用健康状态** | 健康检查端点 | 检测应用级故障 |

### 遥测技术栈

- **设备侧：** 轻量 agent（自研，或 Telegraf/Fluent Bit）通过 MQTT 或 HTTPS 上报指标
- **服务端：** 时序数据库（InfluxDB、Prometheus）+ 仪表盘（Grafana）
- **告警：** 为重启循环、更新失败、温度异常设置阈值

---

## 17. 可靠性测试（HALT/HASS、soak、掉电循环）

出货前，需验证设备能在真实环境中存活：

### 掉电循环耐受

```bash
#!/bin/bash
# Automated power-cycle test (requires controllable power supply or relay)
for i in $(seq 1 1000); do
    power_off
    sleep 5
    power_on
    wait_for_boot   # timeout → fail
    run_smoke_test  # basic peripheral check
    log_result $i
done
```

**目标：** 1000 次掉电循环，零启动失败。

### 温度循环

| 参数 | 消费级 | 工业级 |
|-----------|----------|------------|
| 温度范围 | 0 到 +45 C | -20 到 +70 C |
| 升温速率 | 5 C/min | 10 C/min |
| 极值保持时间 | 15 min | 30 min |
| 循环次数 | 100 | 500 |

### Soak 测试

- 连续运行生产工作负载（AI 推理 + OTA 检查 + 遥测）**7–14 天**
- 监控内存泄漏、文件描述符泄漏、磁盘空间增长、温度漂移
- 记录所有指标，比较第 1 天与第 14 天

### HALT（高加速寿命测试）

- 超出规格限值（温度、振动、电压）施压，以找出设计裕量
- 不是通过/失败型测试——它揭示设计 **何处** 失效
- 在设计周期早期对 5–10 台样机执行

---


<details>
<summary>English original</summary>

**Practical recommendation**

- Use **full rootfs images** for the initial fleet (<100 devices) — simpler, less infrastructure
- Add **delta updates** when bandwidth costs become significant
- Use **container-layer updates** for application changes (most common update type)

---

**14. Rollback and power-fail safety**

**Power-fail during OTA**

If power fails during an update:

| Phase | Power-fail result | Recovery |
|-------|-------------------|----------|
| Downloading image | Incomplete download | Resume or re-download on next boot |
| Writing to inactive slot | Partially written slot | Inactive slot is corrupt but **active slot is untouched** — device boots normally |
| Rebooting into new slot | Boot interrupted | Watchdog or retry counter triggers rollback to previous slot |
| Validation running | Validation incomplete | Slot not marked as successful — next reboot falls back |

**Watchdog timer**

Configure a hardware watchdog to trigger reboot if the system hangs during post-update validation:

```bash
# Enable watchdog
echo 1 > /dev/watchdog

# Application must pet the watchdog periodically
# If it fails to pet within timeout → hardware reset → rollback
```

**Retry counter**

Use a boot counter in `nvbootctrl` or a custom persistent flag:

1. Before switching to new slot, set retry counter = 3
2. Each boot attempt decrements the counter
3. If the counter reaches 0 without the slot being marked successful → revert to old slot

---

**15. OTA signing and verification chain**

```
Build server:
  ├─ Build rootfs image
  ├─ Compute SHA-256 hash of image
  ├─ Sign hash with OTA signing key (RSA-3072 or Ed25519)
  └─ Bundle: image + signature + metadata (version, channel, timestamp)

Device:
  ├─ Download bundle
  ├─ Verify signature using embedded OTA public key
  ├─ Verify hash matches image
  ├─ Check version > current (prevent downgrade)
  └─ Apply update
```

**Key management**

- **OTA signing key** is separate from the **secure boot PKC key**
- Store OTA private key in CI/CD secrets or HSM
- Embed OTA public key in the rootfs (or in a read-only partition)
- Rotate OTA keys by shipping the new public key in a signed update before switching

---

**16. Fleet monitoring and telemetry**

| Metric | Source | Purpose |
|--------|--------|---------|
| **Boot count** | Persistent counter | Detect reboot loops |
| **Update status** | OTA agent | Track rollout success/failure rate |
| **Slot status** | `nvbootctrl` | Detect devices stuck on old slot |
| **Temperature** | `tegrastats` | Detect thermal issues in the field |
| **Disk usage** | `df` | Prevent devices from running out of space |
| **Uptime** | `/proc/uptime` | Detect unexpected reboots |
| **Application health** | Health check endpoint | Detect application-level failures |

**Telemetry stack**

- **Device side:** Lightweight agent (custom, or Telegraf/Fluent Bit) reports metrics via MQTT or HTTPS
- **Server side:** Time-series database (InfluxDB, Prometheus) + dashboard (Grafana)
- **Alerts:** Set thresholds for reboot loops, update failures, temperature anomalies

---

**17. Reliability testing (HALT/HASS, soak, power-cycle)**

Before shipping, validate that the device survives real-world conditions:

**Power-cycle endurance**

```bash
#!/bin/bash
# Automated power-cycle test (requires controllable power supply or relay)
for i in $(seq 1 1000); do
    power_off
    sleep 5
    power_on
    wait_for_boot   # timeout → fail
    run_smoke_test  # basic peripheral check
    log_result $i
done
```

**Target:** 1000 power cycles with zero boot failures.

**Thermal cycling**

| Parameter | Consumer | Industrial |
|-----------|----------|------------|
| Temperature range | 0 to +45 C | -20 to +70 C |
| Ramp rate | 5 C/min | 10 C/min |
| Soak time at extreme | 15 min | 30 min |
| Cycles | 100 | 500 |

**Soak testing**

- Run the production workload (AI inference + OTA checks + telemetry) continuously for **7–14 days**
- Monitor for memory leaks, file descriptor leaks, disk space growth, temperature drift
- Log all metrics and compare day 1 vs day 14

**HALT (Highly Accelerated Life Test)**

- Push beyond spec limits (temperature, vibration, voltage) to find design margins
- Not a pass/fail test — it reveals **where** the design breaks
- Performed on 5–10 units early in the design cycle

---

</details>

## 18. 项目

- **安全启动实验：** 生成测试用 PKC/SBK 密钥，对完整的 JetPack 镜像签名，在启用安全启动的情况下烧录 dev kit，然后验证未签名镜像会被拒绝。
- **端到端 OTA 流水线：** 在带 A/B rootfs 的 Jetson Orin Nano 上配置 SWUpdate、一台签名服务器和一台 staging server。推送一次更新，验证在故意制造失败时的回滚。
- **反复上下电老化测试：** 搭建带继电器控制电源的测试治具，运行 1000 次上下电启动测试，记录结果，并分析任何失败。
- **设备群仪表盘：** 部署 3 台以上 Jetson 设备，搭建 MQTT 遥测 + Grafana 仪表盘，监控整个设备群的启动次数、温度和 OTA 状态。

---

## 19. 资源

| 资源 | 说明 |
|----------|-------------|
| **NVIDIA Jetson Security Guide** | 官方安全启动、熔丝烧写、加密文档 |
| [Orin Nano Security deep dive](../01-Nvidia-Jetson平台/13-Orin-Nano安全/Guide.md) | PKC/SBK 详细演练、威胁模型（Module 1 配套材料） |
| [Orin Nano OTA deep dive](../01-Nvidia-Jetson平台/08-Orin-Nano-OTA深入解析/Guide.md) | `nv_update_engine`、slot 管理内部机制（Module 1 配套材料） |
| [Orin Nano Rootfs and A/B Redundancy](../01-Nvidia-Jetson平台/12-Orin-Nano-Rootfs与AB冗余/Guide.md) | 分区布局、`nvbootctrl`（Module 1 配套材料） |
| **SWUpdate** (sbabic.github.io/swupdate) | 开源 OTA 框架 |
| **Mender** (mender.io) | 提供 Jetson 支持的托管式 OTA 平台 |
| **RAUC** (rauc.io) | 健壮的自动更新控制器 |
| **OP-TEE** (optee.org) | 开源可信执行环境 |


<details>
<summary>English original</summary>

**18. Projects**

- **Secure boot lab:** Generate test PKC/SBK keys, sign a full JetPack image, flash a dev kit with secure boot enabled, then verify that unsigned images are rejected.
- **End-to-end OTA pipeline:** Set up SWUpdate on a Jetson Orin Nano with A/B rootfs, a signing server, and a staging server. Push an update, verify rollback on intentional failure.
- **Power-cycle soak:** Build a test jig with a relay-controlled power supply, run 1000 power-cycle boot tests, log results, and analyze any failures.
- **Fleet dashboard:** Deploy 3+ Jetson devices, set up MQTT telemetry + Grafana dashboard, monitor boot count, temperature, and OTA status across the fleet.

---

**19. Resources**

| Resource | Description |
|----------|-------------|
| **NVIDIA Jetson Security Guide** | Official secure boot, fuse programming, encryption documentation |
| [Orin Nano Security deep dive](../01-Nvidia-Jetson平台/13-Orin-Nano安全/Guide.md) | Detailed PKC/SBK walkthrough, threat model (Module 1 companion) |
| [Orin Nano OTA deep dive](../01-Nvidia-Jetson平台/08-Orin-Nano-OTA深入解析/Guide.md) | `nv_update_engine`, slot management internals (Module 1 companion) |
| [Orin Nano Rootfs and A/B Redundancy](../01-Nvidia-Jetson平台/12-Orin-Nano-Rootfs与AB冗余/Guide.md) | Partition layout, `nvbootctrl` (Module 1 companion) |
| **SWUpdate** (sbabic.github.io/swupdate) | Open-source OTA framework |
| **Mender** (mender.io) | Managed OTA platform with Jetson support |
| **RAUC** (rauc.io) | Robust Auto-Update Controller |
| **OP-TEE** (optee.org) | Open-source Trusted Execution Environment |

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/6. Security and OTA/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/6.%20Security%20and%20OTA/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
