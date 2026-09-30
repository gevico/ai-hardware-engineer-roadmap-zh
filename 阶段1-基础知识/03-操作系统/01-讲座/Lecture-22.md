---
title: 第 22 讲：嵌入式存储：eMMC、UFS、NVMe 与 OTA 分区
description: 第 22 讲：嵌入式存储：eMMC、UFS、NVMe 与 OTA 分区
---

# 第 22 讲：嵌入式存储：eMMC、UFS、NVMe 与 OTA 分区

## 概述

嵌入式 AI 设备——行车记录仪、自动驾驶计算机、边缘推理盒子——必须存储操作系统、神经网络权重以及连续不断的传感器录制数据。为每台设备选择的存储技术决定了写入吞吐、随机 I/O 延迟、物理耐受性，以及设备在 flash 磨损前能撑多久。与可升级存储的笔记本或服务器不同，嵌入式存储焊接在板上，必须承受多年连续运行。

本讲贯穿始终的心智模型是 **存储是寿命有限的系统组件**：flash 存储在经历规定次数的 Program/Erase 周期后就会磨损。每一项设计决策——文件系统选择、分区布局、OTA 策略、TRIM 配置——都会延长或缩短这一寿命。OTA 分区布局决定了一次失败的软件更新会让设备变砖还是能优雅恢复；在已部署的自动驾驶车队中，这就是软件召回与无缝更新之间的差别。

AI 硬件工程师必须理解嵌入式存储，因为错误的分区布局会让一次 OTA 更新变得不可逆。行车记录仪 eMMC 上错误的文件系统选择会导致 flash 提前死亡。新平台上错误的存储类型（eMMC 还是 NVMe）会限制摄像头录制吞吐。这些都不是理论上的担忧——它们是真实部署的 AI 边缘硬件中反复出现的失效模式。

---

## 嵌入式 AI 设备的存储层级

AI 边缘硬件中出现三种存储技术，依据 **成本、封装形式和吞吐** 需求来选择：

| 技术 | 接口 | 顺序读 | 随机 IOPS | 封装形式 |
|---|---|---|---|---|
| eMMC 5.1 | 并行（8-bit HS400） | ~400 MB/s | ~15K | 焊接 BGA |
| UFS 3.1 | 串行 MIPI M-PHY | ~2100 MB/s | ~70K | 焊接 / 插槽 |
| UFS 4.0 | 串行 MIPI M-PHY | ~4200 MB/s | ~130K | 焊接 / 插槽 |
| NVMe Gen4 x4 | PCIe | ~7000 MB/s | ~1M | M.2 / BGA |

选择遵循一条 **性价比曲线**：eMMC 最便宜也最慢；NVMe 最快也最贵（且需要占用 SoC 的 PCIe 通道）。UFS 占据 **中间地带**，是高端移动 AI 平台的标准配置。

```
Cost / Integration                     Performance
  ◀────────────────────────────────────────────────▶
  eMMC 5.1          UFS 3.1/4.0           NVMe Gen4
  (8-bit parallel)  (MIPI serial)         (PCIe x4)
  ~400 MB/s         ~2100-4200 MB/s       ~7000 MB/s
  ~15K IOPS         ~70K-130K IOPS        ~1M IOPS
  Jetson Nano       comma 3X              Jetson Orin
  Low-cost edge     Snapdragon 845        M.2 slot
```

---

## eMMC（embedded MultiMediaCard）

eMMC 实现 **JEDEC JESD84 标准**。flash 控制器、磨损均衡、ECC 和坏块管理全部 **集成在封装内部**。从主机视角看，eMMC 表现为一个容量固定的简单块设备。

### 内部分区结构

每个 eMMC 器件都会暴露若干固定分区，它们独立于用户数据区，且无法由 OS 重新分区：

```
┌─────────────────────────────────────────────────────┐
│              eMMC Physical Package                   │
│                                                     │
│  ┌──────────┐  ┌──────────┐  ┌────────┐            │
│  │  BOOT0   │  │  BOOT1   │  │  RPMB  │            │
│  │  (4 MB)  │  │  (4 MB)  │  │ (4 MB) │            │
│  │ U-Boot   │  │ Backup   │  │ Secure │            │
│  │   SPL    │  │ bootldr  │  │  keys  │            │
│  └──────────┘  └──────────┘  └────────┘            │
│                                                     │
│  ┌─────────────────────────────────────────────┐   │
│  │           User Data Area (UDA)               │   │
│  │  GPT partitioned by the OS                  │   │
│  │  /dev/mmcblk0 → mmcblk0p1, p2, p3, ...      │   │
│  └─────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

- **BOOT0 / BOOT1**：两个独立的 boot 区，各最大 4 MB；烧录后写保护；存放 bootloader（U-Boot SPL、UEFI）
- **RPMB（Replay Protected Memory Block）**：4 MB 认证存储；使用生产时写入的器件唯一密钥，通过 HMAC-SHA256 保护读写
- **User Data Area（UDA）**：主要存储区域；由 OS 按 GPT 分区


<details>
<summary>English original</summary>

**Lecture 22: Embedded Storage: eMMC, UFS, NVMe & OTA Partitioning**

**Overview**

Embedded AI devices — dashcams, autonomous driving computers, edge inference boxes — must store operating systems, neural network weights, and continuous sensor recordings. The storage technology chosen for each device determines write throughput, random I/O latency, physical resilience, and how long the device lasts before the flash wears out. Unlike a laptop or server where storage can be upgraded, embedded storage is soldered to the board and must last for years of continuous operation.

The mental model to carry through this lecture is **storage as a system component with a finite lifespan**: flash memory wears out after a defined number of Program/Erase cycles. Every design decision — filesystem choice, partition layout, OTA strategy, TRIM configuration — either extends or shortens that lifespan. OTA partition layout determines whether a failed software update bricks a device or recovers gracefully, which in a deployed fleet of autonomous vehicles is the difference between a software recall and a seamless update.

AI hardware engineers need to understand embedded storage because the wrong partition layout makes an OTA update irreversible. The wrong filesystem choice on a dashcam eMMC leads to premature flash death. The wrong storage type (eMMC vs NVMe) on a new platform limits camera recording throughput. These are not theoretical concerns — they are recurring failure modes in real deployed AI edge hardware.

---

**Storage Hierarchy for Embedded AI Devices**

Three storage technologies appear in AI edge hardware, chosen by **cost, form factor, and throughput** requirements:

| Technology | Interface | Seq Read | Random IOPS | Form Factor |
|---|---|---|---|---|
| eMMC 5.1 | Parallel (8-bit HS400) | ~400 MB/s | ~15K | Soldered BGA |
| UFS 3.1 | Serial MIPI M-PHY | ~2100 MB/s | ~70K | Soldered / socket |
| UFS 4.0 | Serial MIPI M-PHY | ~4200 MB/s | ~130K | Soldered / socket |
| NVMe Gen4 x4 | PCIe | ~7000 MB/s | ~1M | M.2 / BGA |

The choice follows a **cost-performance curve**: eMMC is cheapest and slowest; NVMe is fastest and most expensive (and requires PCIe lanes from the SoC). UFS occupies the **middle ground** and is the standard for high-end mobile AI platforms.

```
Cost / Integration                     Performance
  ◀────────────────────────────────────────────────▶
  eMMC 5.1          UFS 3.1/4.0           NVMe Gen4
  (8-bit parallel)  (MIPI serial)         (PCIe x4)
  ~400 MB/s         ~2100-4200 MB/s       ~7000 MB/s
  ~15K IOPS         ~70K-130K IOPS        ~1M IOPS
  Jetson Nano       comma 3X              Jetson Orin
  Low-cost edge     Snapdragon 845        M.2 slot
```

---

**eMMC (embedded MultiMediaCard)**

eMMC implements the **JEDEC JESD84 standard**. The flash controller, wear leveling, ECC, and bad block management are all **integrated inside the package**. From the host's perspective, eMMC presents a simple block device with a fixed capacity.

**Internal Partition Structure**

Every eMMC device exposes fixed partitions that are separate from the user data area and cannot be repartitioned by the OS:

```
┌─────────────────────────────────────────────────────┐
│              eMMC Physical Package                   │
│                                                     │
│  ┌──────────┐  ┌──────────┐  ┌────────┐            │
│  │  BOOT0   │  │  BOOT1   │  │  RPMB  │            │
│  │  (4 MB)  │  │  (4 MB)  │  │ (4 MB) │            │
│  │ U-Boot   │  │ Backup   │  │ Secure │            │
│  │   SPL    │  │ bootldr  │  │  keys  │            │
│  └──────────┘  └──────────┘  └────────┘            │
│                                                     │
│  ┌─────────────────────────────────────────────┐   │
│  │           User Data Area (UDA)               │   │
│  │  GPT partitioned by the OS                  │   │
│  │  /dev/mmcblk0 → mmcblk0p1, p2, p3, ...      │   │
│  └─────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

- **BOOT0 / BOOT1**: two independent boot areas, each up to 4 MB; write-protected after provisioning; hold bootloader (U-Boot SPL, UEFI)
- **RPMB (Replay Protected Memory Block)**: 4 MB authenticated storage; read/write protected by HMAC-SHA256 using a device-unique key provisioned at manufacturing
- **User Data Area (UDA)**: the main storage region; GPT-partitioned by the OS

</details>

### RPMB 安全属性

RPMB 防止**对固件的回滚攻击**。安全世界（OP-TEE、ARM TrustZone）在每次写入时递增写计数器；eMMC 控制器拒绝任何不携带**有效 HMAC** 且计数值 ≥ 当前值的写入。在 Jetson 安全启动中用于存储：

- 固件版本计数器（防回滚）
- 用于全盘加密的加密密钥材料

> **关键洞察：** RPMB 是主机无法伪造的硬件强制写计数器。获取主机 OS root 权限的攻击者无法递减 RPMB 中的固件版本计数器，因为每次写入都必须包含用仅存在于 TrustZone 安全世界中的密钥计算的有效 HMAC。这使得固件降级攻击（降级到存在已知漏洞的版本）即使在整个 OS 被攻陷的情况下也不可能。

### eMMC 可靠性

- 消费级：每个单元 3K–10K 次 P/E 周期（MLC）；1K–3K（TLC）
- 企业级（JEDEC JESD84-B51）：额定 16 PBW（拍字节写入量）
- 预留空间：保留 10–28% 的原始容量用于磨损均衡和坏块替换

> **常见陷阱：** 在高写入应用（连续摄像头录制）中使用消费级 eMMC，如果没有 F2FS 或 TRIM，可能在几个月而非几年内耗尽 P/E 周期。64 GB TLC eMMC 在 1K P/E 周期下的总字节写入预算约为 64 TB。如果写放大为 1x，持续以 50 MB/s 写入的行车记录仪理论上可在约 15 天内耗尽——而在没有 F2FS 时绝不会是 1x。借助 F2FS 和正确的 TRIM 配置，有效写放大降低，显著延长设备寿命。

---

## UFS（通用闪存存储）

UFS 使用**串行、全双工 MIPI M-PHY** 物理层，并采用基于 SCSI 的命令集（UFS Transport Protocol）。相对 eMMC 的关键优势：

- **命令队列**：最多 32 条命令在途（相比 eMMC 单命令）；对随机 I/O 延迟至关重要
- **全双工**：同时读写；提升混合工作负载吞吐
- **更低的 CPU 开销**：命令处理卸载到 UFS 设备控制器

UFS 使用与 eMMC 相同的逻辑分区结构（BOOT、RPMB、UDA）。它是 **Qualcomm Snapdragon 平台的标准配置**。comma 3X（openpilot 主要硬件）使用带 UFS 存储的 Snapdragon 845 进行摄像头录制和 OS 运行。

> **关键洞察：** 对自动驾驶平台而言，eMMC 与 UFS 之间最重要的实际差异是命令队列。eMMC 一次处理一条命令。当摄像头驱动发出第 N 帧的写入时，任何其他 I/O（OS、modeld 读取模型权重、日志记录）都必须等待。UFS 可同时处理 32 条命令，意味着摄像头写入、模型读取和日志写入全部并行进行，没有队头阻塞。

随机 IOPS 的提升（UFS 3.1 约 70K，而 eMMC 5.1 约 15K）直接反映了这一点：随机 I/O 吞吐随设备可处理的未完成命令数量扩展。

---

## 嵌入式平台上的 NVMe

对于需要**最大存储吞吐**的平台——大规模数据集记录、高频传感器记录或训练数据采集——**通过 PCIe 的 NVMe** 是正确选择。

Jetson Orin 提供连接到 PCIe Gen4 x4（约 7 GB/s）的 M.2 Key M 插槽：

- 支持标准 NVMe 2280 和 2230 外形规格的 SSD
- 在需要大规模数据集记录、回放或高频传感器记录时使用
- 使用 `blk-mq` 配合每 CPU NVMe 队列；将 `io_uring` 与 `O_DIRECT` 结合以获得最大吞吐
- 用 `nvme list` 枚举设备；用 `nvme smart-log /dev/nvme0` 查看健康状态和磨损指示

借助第 19 讲的 io_uring 技术和第 20 讲的 PCIe 架构，从 CUDA 推理到 NVMe 记录的完整路径是：CUDA 输出 → GPU 显存 → DMA-BUF → GPUDirect Storage → NVMe PCIe DMA → SSD——数据路径中无需 CPU 参与。

---

## OTA 分区布局

理解了存储技术后，下一个问题是如何组织分区以实现安全的空中升级。分区布局决定更新失败时会发生什么。

空中升级策略决定失败时的**恢复行为和停机时间**。


<details>
<summary>English original</summary>

**RPMB Security Properties**

RPMB prevents **rollback attacks on firmware**. The secure world (OP-TEE, ARM TrustZone) increments a write counter on each write; the eMMC controller rejects any write that does not carry a **valid HMAC** and a counter value ≥ current. Used in Jetson secure boot to store:

- Firmware version counter (anti-rollback)
- Encryption key material for full-disk encryption

> **Key Insight:** RPMB is a hardware-enforced write counter that the host cannot fake. An attacker who gains root access on the host OS cannot decrement the firmware version counter in RPMB because every write must include a valid HMAC computed with a key that exists only inside the TrustZone secure world. This makes firmware downgrade attacks (to a version with known exploits) impossible even with full OS compromise.

**eMMC Reliability**

- Consumer grade: 3K–10K P/E cycles per cell (MLC); 1K–3K (TLC)
- Enterprise grade (JEDEC JESD84-B51): 16 PBW (petabytes written) rated
- Over-provisioning: 10–28% of raw capacity reserved for wear leveling and bad block replacement

> **Common Pitfall:** Consumer-grade eMMC used in a high-write application (continuous camera recording) without F2FS or TRIM can exhaust P/E cycles in months rather than years. A 64 GB TLC eMMC at 1K P/E cycles has a total byte write budget of roughly 64 TB. A dashcam writing 50 MB/s continuously could theoretically exhaust this in ~15 days if write amplification is 1x — which it never is without F2FS. With F2FS and proper TRIM configuration, the effective write amplification drops, extending device life significantly.

---

**UFS (Universal Flash Storage)**

UFS uses a **serial, full-duplex MIPI M-PHY** physical layer with a SCSI-based command set (UFS Transport Protocol). Key advantages over eMMC:

- **Command queuing**: up to 32 commands in flight (vs eMMC single-command); critical for random I/O latency
- **Full duplex**: simultaneous read and write; improves mixed workload throughput
- **Lower CPU overhead**: command processing offloaded to UFS device controller

UFS uses the same logical partition structure as eMMC (BOOT, RPMB, UDA). It is the **standard on Qualcomm Snapdragon platforms**. The comma 3X (openpilot primary hardware) uses a Snapdragon 845 with UFS storage for camera recording and OS.

> **Key Insight:** The most important practical difference between eMMC and UFS for an autonomous driving platform is command queuing. eMMC processes one command at a time. While the camera driver issues a write for frame N, any other I/O (OS, modeld reading model weights, logging) must wait. UFS can process 32 commands simultaneously, meaning camera writes, model reads, and log writes all proceed in parallel with no head-of-line blocking.

The improvement in random IOPS (~70K for UFS 3.1 vs ~15K for eMMC 5.1) directly reflects this: random I/O throughput scales with the number of outstanding commands the device can handle.

---

**NVMe on Embedded Platforms**

For platforms where **maximum storage throughput** is required — large-scale dataset logging, high-frequency sensor recording, or training data capture — **NVMe via PCIe** is the correct choice.

Jetson Orin exposes an M.2 Key M slot connected to PCIe Gen4 x4 (~7 GB/s):

- Supports standard NVMe 2280 and 2230 form-factor SSDs
- Used when large-scale dataset logging, replay, or high-frequency sensor recording is needed
- `blk-mq` with per-CPU NVMe queues; combine with `io_uring` + `O_DIRECT` for maximum throughput
- `nvme list` to enumerate devices; `nvme smart-log /dev/nvme0` for health and wear indicators

With the io_uring techniques from Lecture 19 and the PCIe architecture from Lecture 20, the full path from CUDA inference to NVMe logging is: CUDA output → GPU memory → DMA-BUF → GPUDirect Storage → NVMe PCIe DMA → SSD — with no CPU involvement in the data path.

---

**OTA Partition Layouts**

With the storage technologies understood, the next question is how to structure partitions for safe over-the-air updates. The partition layout determines what happens when an update fails.

Over-the-air update strategy determines **recovery behavior and downtime** on failure.

</details>

### A/B 无缝更新

两套完整的系统分区集（**slot A 和 slot B**）。**非活动 slot 接收更新**，而活动 slot 正常运行。

```
/dev/mmcblk0p1  boot_a      (active)
/dev/mmcblk0p2  boot_b      (inactive → receives new image)
/dev/mmcblk0p3  system_a    (active rootfs)
/dev/mmcblk0p4  system_b    (inactive → receives new rootfs)
/dev/mmcblk0p5  userdata    (persistent, not updated)
```

A/B 更新与回滚序列：

```
Normal operation:          Slot A active, Slot B empty/old
      ↓
OTA download begins:       New image written to Slot B
      ↓                    (device continues running on Slot A)
Download complete:         Bootloader marks Slot B as "try next boot"
      ↓
Reboot:                    Bootloader activates Slot B, boots count = 1
      ↓
Boot success check:        If userspace confirms success → mark B permanent
Boot failure check:        If boot count exceeds threshold (3) → revert to A
      ↓
Result:
  Success path: Slot B is now active, Slot A becomes the new inactive slot
  Failure path: Slot A is active again, Slot B contains the failed image
```

重启时：bootloader 将 slot B 标记为活动，尝试引导。若引导计数超过阈值（通常为 3）且没有成功引导标记，bootloader 回退到 slot A。用于：Android A/B、openpilot Agnos、Jetson UEFI capsule update。

> **关键洞察：** A/B OTA 的关键特性是，在更新下载与写入过程中，运行中的系统从不被中断。新镜像写入非活动 slot，而设备继续在活动 slot 上正常运行。这消除了设备处于不可引导状态时的停机窗口。对于自动驾驶计算机，这意味着 OTA 更新可在车辆停放时下发并准备好，并在下次重启时以秒级完成最终的原子引导 slot 切换。

### Recovery 分区更新

单个活动分区 + 一个小型 recovery 镜像。更新流程：下载新镜像 → 重启进入 recovery → 烧写系统分区 → 重启进入新系统。**没有备份镜像就无法回滚**。用于较老的 Android 设备和简单的嵌入式系统。

> **常见陷阱：** Recovery 分区更新没有自动回滚。如果新镜像引导失败，设备就会卡死。对于量产自动驾驶部署，这是不可接受的 —— 一次失败的更新绝不能导致现场设备永久变砖。A/B 分区是任何可 OTA 更新的 AI 边缘设备的最低要求。

### OSTree：类 Git 的原子更新

OSTree 维护文件系统树的**内容寻址对象存储**，类似于 git。已部署的更新从对象存储硬链接而来；**通过 symlink 更新实现原子切换**。用于 Automotive Grade Linux (AGL) 和汽车 ECU Linux 平台。兼容 ext4（硬链接）和 btrfs。

OSTree 模型在概念上很优雅：每个已部署的 OS 版本就是一个 git commit hash。回滚到先前版本只需更新一个 symlink。差分更新（仅变更文件）尽可能紧凑，因为未变更的文件已经在对象存储中。

---

## 分区工具与 kernel 接口

```bash
parted /dev/mmcblk0 print       # display current GPT partition table
sgdisk -p /dev/mmcblk0          # GPT partition table details
partprobe /dev/mmcblk0          # re-read partition table without reboot
cat /proc/partitions             # kernel's view of all block devices
cat /sys/block/mmcblk0/mmcblk0p1/size  # partition size in 512-byte sectors
ls /dev/mmcblk0*                 # list all partition device nodes
```

- `parted` / `sgdisk`：创建和修改 GPT 分区表
- `partprobe /dev/mmcblk0`：无需重启重新读取分区表
- `/proc/partitions`：kernel 对所有块设备和分区的视图
- `/sys/block/mmcblk0/mmcblk0p1/size`：以 512 字节扇区计的分区大小
- `/dev/mmcblk0p1`、`/dev/nvme0n1p1`：用于访问分区的设备节点

---


<details>
<summary>English original</summary>

**A/B Seamless Update**

Two full system partition sets (**slot A and slot B**). The **inactive slot receives the update** while the active slot runs normally.

```
/dev/mmcblk0p1  boot_a      (active)
/dev/mmcblk0p2  boot_b      (inactive → receives new image)
/dev/mmcblk0p3  system_a    (active rootfs)
/dev/mmcblk0p4  system_b    (inactive → receives new rootfs)
/dev/mmcblk0p5  userdata    (persistent, not updated)
```

The A/B update and rollback sequence:

```
Normal operation:          Slot A active, Slot B empty/old
      ↓
OTA download begins:       New image written to Slot B
      ↓                    (device continues running on Slot A)
Download complete:         Bootloader marks Slot B as "try next boot"
      ↓
Reboot:                    Bootloader activates Slot B, boots count = 1
      ↓
Boot success check:        If userspace confirms success → mark B permanent
Boot failure check:        If boot count exceeds threshold (3) → revert to A
      ↓
Result:
  Success path: Slot B is now active, Slot A becomes the new inactive slot
  Failure path: Slot A is active again, Slot B contains the failed image
```

On reboot: bootloader marks slot B as active, tries to boot. If boot count exceeds threshold (typically 3) without a successful boot marker, bootloader reverts to slot A. Used in: Android A/B, openpilot Agnos, Jetson UEFI capsule update.

> **Key Insight:** The critical property of A/B OTA is that the running system is never interrupted during the update download and write process. The new image is written to the inactive slot while the device continues operating normally on the active slot. This eliminates the downtime window during which the device would be in an unbootable state. For an autonomous driving computer, this means an OTA update can be delivered and prepared while the vehicle is parked, completing the final atomic boot-slot-switch in seconds on the next restart.

**Recovery Partition Update**

Single active partition + a small recovery image. Update process: download new image → reboot into recovery → flash system partition → reboot into new system. **No rollback without a backup image**. Used in older Android devices and simple embedded systems.

> **Common Pitfall:** Recovery-partition updates have no automatic rollback. If the new image fails to boot, the device is stuck. For production autonomous driving deployments, this is unacceptable — a failed update must never result in a permanently bricked device in the field. A/B partitioning is the minimum requirement for any OTA-updatable AI edge device.

**OSTree: Git-Like Atomic Updates**

OSTree maintains a **content-addressed object store** of filesystem trees, analogous to git. Deployed updates are hard-linked from the object store; **atomic switchover via a symlink update**. Used in Automotive Grade Linux (AGL) and automotive ECU Linux platforms. Compatible with ext4 (hard links) and btrfs.

The OSTree model is conceptually elegant: each deployed OS version is a git commit hash. Rolling back to a previous version is as simple as updating a symlink. Differential updates (only changed files) are as compact as possible because unchanged files are already in the object store.

---

**Partition Tools and Kernel Interfaces**

```bash
parted /dev/mmcblk0 print       # display current GPT partition table
sgdisk -p /dev/mmcblk0          # GPT partition table details
partprobe /dev/mmcblk0          # re-read partition table without reboot
cat /proc/partitions             # kernel's view of all block devices
cat /sys/block/mmcblk0/mmcblk0p1/size  # partition size in 512-byte sectors
ls /dev/mmcblk0*                 # list all partition device nodes
```

- `parted` / `sgdisk`: create and modify GPT partition tables
- `partprobe /dev/mmcblk0`: re-read partition table without reboot
- `/proc/partitions`: kernel's view of all block devices and partitions
- `/sys/block/mmcblk0/mmcblk0p1/size`: partition size in 512-byte sectors
- `/dev/mmcblk0p1`, `/dev/nvme0n1p1`: device nodes for partition access

---

</details>

## 磨损均衡与写放大

Flash 存储会随 **Program/Erase (P/E) 次数**而退化。Flash Translation Layer（FTL）负责管理其寿命：

- **动态磨损均衡**：优先写入磨损最少的 block；对频繁更新的数据有效
- **静态磨损均衡**：定期把冷数据从磨损 block 迁移到新 block；防止冷热失衡
- **写放大系数（WAF）**：物理写入 / 逻辑写入；WAF 为 1.0 是理想值；在缺少文件系统配合时，对消费级 SSD 的随机小写入可产生 10–50 的 WAF
- 降低 WAF 的手段：顺序写入、大 block 尺寸、F2FS 或 btrfs，以及正确的 TRIM 配置

写放大系数的级联：

```
Application writes 4 KB (one page)
       ↓ filesystem (F2FS: log-structured, ~1x WAF)
       ↓ filesystem (ext4 random: ~3-5x WAF)
       ↓ FTL garbage collection (without TRIM: +5-10x)
Physical NAND writes: 4 KB × WAF_fs × WAF_ftl
  Best case (F2FS + TRIM): ~4 KB physical
  Worst case (ext4 random, no TRIM): ~160-200 KB physical
```

> **关键洞察：** WAF 是乘性的：文件系统的 WAF 与 FTL 的 WAF 相乘。文件系统层 WAF 为 3，叠加 FTL 层 WAF 为 10，意味着物理写入是逻辑写入的 30 倍。在标称 1K P/E 次数的 TLC eMMC 上，这会使器件的有效寿命缩短 30 倍。6 个月的 flash 寿命与 15 年的 flash 寿命之差，可能完全取决于文件系统与 TRIM 配置。

---

## 存储诊断

```bash
# Real-time I/O statistics: util%, await (ms), r/s, w/s, throughput
iostat -x 1

# Block-level I/O tracing for a specific device
blktrace -d /dev/mmcblk0 -o trace    # capture block I/O events

# Parse and display timing breakdown from trace
blkparse trace.blktrace.0            # shows per-request latency breakdown

# NVMe health and wear indicators
nvme smart-log /dev/nvme0            # temperature, available spare, wear indicator

# eMMC health
cat /sys/class/mmc_host/mmc0/mmc0:0001/life_time  # eMMC wear level (0x01-0x0A)
```

`iostat` 输出中的 `await` 是**平均 I/O 服务时间**，包含排队等待时间。在摄像头录制负载下 `await` 持续上升，表明**存储已饱和**。`blktrace` 可定位是哪个进程造成延迟尖峰。

> **常见陷阱：** 只用 `iostat -x` 来诊断存储饱和。iostat 中的 `%util` 字段显示的是队列利用率，而非设备饱和度 —— 若队列深度较低，现代 NVMe SSD 可能在队列利用率 100% 时仍有余量。应始终分别检查 `await`（平均服务时间）与 `r_await`/`w_await`。针对 eMMC，开发期间应每月检查 life_time sysfs 属性，以确认写入模式处于预期磨损预算之内。

---

## 小结

| 存储类型 | 接口 | 顺序读 | 随机 IOPS | AI 硬件中的典型用途 |
|---|---|---|---|---|
| eMMC 5.1 | 8-bit 并行 HS400 | ~400 MB/s | ~15K | Jetson Nano、低成本边缘设备 |
| UFS 3.1 | MIPI M-PHY 串行 | ~2100 MB/s | ~70K | comma 3X（Snapdragon）、中端 SoC |
| UFS 4.0 | MIPI M-PHY 串行 | ~4200 MB/s | ~130K | 旗舰移动 AI SoC |
| NVMe Gen4 x4 | PCIe Gen4 | ~7000 MB/s | ~1M | Jetson Orin（M.2 插槽）、数据集记录 |
| NVMe Gen3 x4 | PCIe Gen3 | ~3500 MB/s | ~500K | 工作站 AI 开发、云端训练节点 |


<details>
<summary>English original</summary>

**Wear Leveling and Write Amplification**

Flash storage degrades with **Program/Erase (P/E) cycles**. The Flash Translation Layer (FTL) manages longevity:

- **Dynamic wear leveling**: preferentially writes to least-worn blocks; effective for frequently updated data
- **Static wear leveling**: periodically migrates cold data from worn blocks to fresh ones; prevents hot/cold imbalance
- **Write Amplification Factor (WAF)**: physical writes / logical writes; WAF of 1.0 is ideal; random small writes to consumer SSDs can produce WAF of 10–50 without filesystem cooperation
- Minimize WAF with: sequential writes, large block sizes, F2FS or btrfs, and proper TRIM configuration

The Write Amplification Factor cascade:

```
Application writes 4 KB (one page)
       ↓ filesystem (F2FS: log-structured, ~1x WAF)
       ↓ filesystem (ext4 random: ~3-5x WAF)
       ↓ FTL garbage collection (without TRIM: +5-10x)
Physical NAND writes: 4 KB × WAF_fs × WAF_ftl
  Best case (F2FS + TRIM): ~4 KB physical
  Worst case (ext4 random, no TRIM): ~160-200 KB physical
```

> **Key Insight:** WAF is multiplicative: filesystem WAF multiplies with FTL WAF. A WAF of 3 at the filesystem layer combined with a WAF of 10 at the FTL layer means 30x more physical writes than logical writes. On a TLC eMMC rated for 1K P/E cycles, this reduces effective device lifetime by 30x. The difference between a 6-month flash lifespan and a 15-year flash lifespan can come down entirely to filesystem and TRIM configuration.

---

**Storage Diagnostics**

```bash
# Real-time I/O statistics: util%, await (ms), r/s, w/s, throughput
iostat -x 1

# Block-level I/O tracing for a specific device
blktrace -d /dev/mmcblk0 -o trace    # capture block I/O events

# Parse and display timing breakdown from trace
blkparse trace.blktrace.0            # shows per-request latency breakdown

# NVMe health and wear indicators
nvme smart-log /dev/nvme0            # temperature, available spare, wear indicator

# eMMC health
cat /sys/class/mmc_host/mmc0/mmc0:0001/life_time  # eMMC wear level (0x01-0x0A)
```

`await` in `iostat` output is the **average I/O service time** including queue wait. A rising `await` under camera recording load indicates the **storage is saturated**. `blktrace` identifies which process is responsible for latency spikes.

> **Common Pitfall:** Diagnosing storage saturation only with `iostat -x`. The `%util` field in iostat shows queue utilization, not device saturation — a modern NVMe SSD can be at 100% queue utilization while still having headroom if the queue depth is low. Always check `await` (average service time) and `r_await`/`w_await` separately. For eMMC specifically, check the life_time sysfs attribute monthly during development to validate that your write patterns are within the expected wear budget.

---

**Summary**

| Storage type | Interface | Sequential read | Random IOPS | Typical use in AI hardware |
|---|---|---|---|---|
| eMMC 5.1 | 8-bit parallel HS400 | ~400 MB/s | ~15K | Jetson Nano, low-cost edge devices |
| UFS 3.1 | MIPI M-PHY serial | ~2100 MB/s | ~70K | comma 3X (Snapdragon), mid-range SoC |
| UFS 4.0 | MIPI M-PHY serial | ~4200 MB/s | ~130K | Flagship mobile AI SoC |
| NVMe Gen4 x4 | PCIe Gen4 | ~7000 MB/s | ~1M | Jetson Orin (M.2 slot), dataset logging |
| NVMe Gen3 x4 | PCIe Gen3 | ~3500 MB/s | ~500K | Workstation AI dev, cloud training node |

</details>

### 概念回顾

- **即使攻击者在宿主 OS 上拥有 root 权限，RPMB 为何仍能阻止固件回滚攻击？** RPMB 写认证要求使用一个只存在于 TrustZone 安全世界（OP-TEE）中的密钥来计算 HMAC。正常世界 Linux OS 上的 root 权限并不能获得该密钥的访问权。eMMC 硬件控制器自身会校验 HMAC 和单调递增的写计数器——无论请求者是谁，任何未通过该检查的写操作都会被拒绝。

- **对自动驾驶平台而言，UFS 命令队列相比 eMMC 有什么实际优势？** eMMC 一次只接受一条命令（深度为 1 的队列）。在一个同时录制摄像头帧（顺序写）、运行神经网络推理（随机读取模型权重）并记录传感器数据（混合 I/O）的系统上，这些操作只能彼此串行。而 UFS 允许 32 条命令同时在途，使这三者可以并行推进，降低每个操作的平均延迟。

- **A/B OTA 回滚的触发条件是什么，由谁控制？** bootloader 会为正在尝试的非活动 slot 维护一个启动尝试计数器。如果用户态软件（更新守护进程、openpilot 的 updateinstallerd）在启动后的超时时间内没有写入“启动成功”标记，该计数器就会继续递增。当计数器超过阈值（通常为 3）时，bootloader 将该非活动 slot 标记为失败，并回退到此前活动的 slot。必须由用户态给出显式的成功确认——沉默即视为失败。

- **写放大因子（WAF）如何与 eMMC 寿命相互作用，最小化它的最佳方式是什么？** 消耗的物理 P/E 周期 = 逻辑写入量 × WAF_filesystem × WAF_FTL。最小化 WAF 需要：(1) 采用日志结构文件系统（F2FS），使其产生的顺序写与闪存擦除块边界对齐；(2) 启用 TRIM（通过 `systemd-fstrim.timer`），让 FTL 知道哪些块是空闲的，从而避免垃圾回收开销；(3) 在分区布局中预留充足的 over-provisioning。

- **OSTree 与 git 有什么共同点，为什么这对 OTA 更新很有用？** OSTree 以内容寻址对象的形式存储文件系统树（类似 git 的 blob 和 tree）。每个 OS 版本都是一个指向其根 tree 的 commit。差分更新只下载本地存储中尚不存在的对象——与 `git fetch` 完全一致。回滚就是更新符号链接，使其指向前一个 commit 的 tree。版本之间的 delta 可以任意小，因此即使在带宽有限的情况下，也能实际做到网络高效的增量更新。

- **`blktrace` 能揭示哪些 `iostat` 无法揭示的信息？** `iostat` 报告的是设备层面的汇总统计：总吞吐、平均延迟、队列深度。`blktrace` 则捕获每一个单独的 I/O 请求，并记录块层流水线各阶段的时间戳：plug（批量）、unplug（派发）、issue（发送到设备）、complete（DMA 完成）。据此可以定位是哪个具体进程产生了延迟最高的请求、延迟在流水线的哪一环被加上，以及问题究竟出在队列争用还是设备本身慢。

---

## AI 硬件关联

- A/B OTA 分区布局是 openpilot Agnos 更新策略的基础；新固件写入非活动 slot 期间，活动 slot 继续运行，启动失败时自动回滚
- RPMB 认证存储是 Jetson 安全启动防止固件降级攻击的手段；TrustZone 安全世界在每次固件更新成功后递增防回滚计数器
- Snapdragon 845（comma 3X）上的 UFS 提供了在运行推理的同时维持三路摄像头同步录制所需的随机 IOPS
- Jetson Orin 上的 NVMe 支持为持续学习流水线进行大规模端侧数据集记录；PCIe P2P 让记录的数据可以直接在 GPU 内存中预处理
- 对长期运行的边缘 AI 行车记录仪设备而言，eMMC 上的 F2FS 是正确的文件系统选择；相比 ext4，它最小化写放大并延长 eMMC 寿命
- 当摄像头录制流水线丢帧时，`iostat -x 1` 是首选诊断工具；`await` 升至摄像头帧周期以上即表明存在存储瓶颈


<details>
<summary>English original</summary>

**Conceptual Review**

- **Why does RPMB prevent firmware rollback attacks even when the attacker has root access on the host OS?** RPMB write authentication requires an HMAC computed with a key that lives exclusively in the TrustZone secure world (OP-TEE). Root access on the normal world Linux OS does not grant access to this key. The eMMC hardware controller itself validates the HMAC and monotonically increasing write counter — it rejects any write that does not pass this check, regardless of who is requesting it.

- **What practical advantage does UFS command queuing provide over eMMC for an autonomous driving platform?** eMMC accepts one command at a time (depth-1 queue). On a system simultaneously recording camera frames (sequential writes), running neural net inference (random reads of model weights), and logging sensor data (mixed I/O), these operations serialize behind each other. UFS with 32 commands in flight allows all three to proceed in parallel, reducing the average latency of each operation.

- **What is the A/B OTA rollback trigger, and who controls it?** The bootloader maintains a boot attempt counter for the inactive slot being tried. If the userspace software (update daemon, openpilot's updateinstallerd) does not write a "boot successful" marker within a timeout after boot, the counter continues to increment. When the counter exceeds the threshold (typically 3), the bootloader marks the inactive slot as failed and reverts to the previously active slot. The explicit success confirmation from userspace is required — silence is treated as failure.

- **How does Write Amplification Factor (WAF) interact with eMMC lifespan, and what is the best way to minimize it?** Physical P/E cycles consumed = logical writes × WAF_filesystem × WAF_FTL. Minimizing WAF requires: (1) a log-structured filesystem (F2FS) that produces sequential writes matching flash erase block boundaries; (2) TRIM enabled (via `systemd-fstrim.timer`) so the FTL knows which blocks are free and can avoid garbage collection overhead; (3) adequate over-provisioning reserved in the partition layout.

- **What does OSTree have in common with git, and why is this useful for OTA updates?** OSTree stores filesystem trees as content-addressed objects (like git blobs and trees). Each OS version is a commit pointing to its root tree. A differential update downloads only the objects not already present in the local store — exactly like `git fetch`. Rollback is a symlink update to point to the previous commit's tree. The delta between versions can be arbitrarily small, making network-efficient incremental updates practical even over limited bandwidth.

- **What does `blktrace` reveal that `iostat` cannot?** `iostat` reports aggregate device statistics: total throughput, average latency, queue depth. `blktrace` captures every individual I/O request with timestamps at each stage of the block layer pipeline: plug (batched), unplug (dispatched), issue (sent to device), complete (DMA done). This allows you to identify which specific process is generating the highest-latency requests, where in the pipeline latency is being added, and whether the issue is queue contention vs. actual device slowness.

---

**AI Hardware Connection**

- A/B OTA partition layout is the foundation of openpilot Agnos update strategy; the active slot continues running while the new firmware is written to the inactive slot, with automatic rollback on failed boot
- RPMB authenticated storage is how Jetson secure boot prevents firmware downgrade attacks; the TrustZone secure world increments the anti-rollback counter on each successful firmware update
- UFS on the Snapdragon 845 (comma 3X) provides the random IOPS needed to sustain simultaneous recording from three cameras while running inference
- NVMe on Jetson Orin enables large-scale on-device dataset logging for continuous learning pipelines; PCIe P2P allows the logged data to be pre-processed directly in GPU memory
- F2FS on eMMC is the correct filesystem choice for long-running edge AI dashcam devices; it minimizes write amplification and extends eMMC lifespan compared to ext4
- `iostat -x 1` is the first diagnostic tool to run when a camera recording pipeline drops frames; `await` rising above the camera frame period indicates a storage bottleneck

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-22.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-22.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
