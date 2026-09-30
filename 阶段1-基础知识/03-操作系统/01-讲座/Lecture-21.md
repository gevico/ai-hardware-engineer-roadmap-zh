---
title: 第 21 讲：文件系统：ext4、btrfs、F2FS 与 overlayfs
description: 第 21 讲：文件系统：ext4、btrfs、F2FS 与 overlayfs
---

# 第 21 讲：文件系统：ext4、btrfs、F2FS 与 overlayfs

## 概述

文件系统是应用与裸存储之间的那一层。人们很容易把文件系统当作隐形基础设施——打开文件、写数据、关闭，然后假定数据已经持久化。但文件系统的选择对 AI 硬件系统有着直接且可度量的影响：它决定了摄像头帧落盘的速度、崩溃后重启耗时、OTA 更新失败时是变砖还是安全回滚，以及 eMMC 芯片在磨损前的寿命。

贯穿本讲的心智模型是**分层存储栈**：应用与 VFS（Virtual File System）层交互，该层提供统一 API。VFS 之下是实际的文件系统实现（ext4、btrfs、F2FS）。再往下是块层，负责把 I/O 路由到物理设备。每一层所做的决策都会影响其上各层。

AI 硬件工程师需要理解文件系统，因为边缘 AI 设备（行车记录仪、机器人控制器）会持续写入闪存，而闪存会磨损。OTA 更新的可靠性取决于文件系统的快照与回滚能力。容器化推理部署用 overlayfs 提供不可变的 base image。而把 openpilot 所有进程串联起来的 IPC 机制，跑在 RAM 支撑的文件系统（tmpfs）上。

---

## 文件系统的职责与 VFS

文件系统把文件组织成目录，提供**跨掉电的持久化**，管理空闲空间，并确保**崩溃后的元数据一致性**。

### VFS（Virtual File System）

Linux VFS 是一个**抽象层**，无论底层文件系统实现如何，都呈现**统一的 API**。

分层架构如下：

```
Application
    │  open() read() write() mmap() fsync()
    ▼
┌─────────────────────────────────────────┐
│          VFS (Virtual File System)       │
│  struct file_operations dispatch table  │
│  dentry cache (name → inode)            │
│  inode cache (per-file metadata)        │
└──────┬──────────┬──────────┬────────────┘
       │          │          │
  ┌────▼───┐ ┌───▼───┐ ┌───▼────┐
  │  ext4  │ │ btrfs │ │  F2FS  │  ... (any registered filesystem)
  └────┬───┘ └───┬───┘ └───┬────┘
       └─────────┴──────────┘
                 │
    ┌────────────▼────────────┐
    │      Block Layer        │
    │  blk-mq, I/O scheduler  │
    └────────────┬────────────┘
                 │
    ┌────────────▼────────────┐
    │    Storage Device       │
    │  NVMe / eMMC / UFS      │
    └─────────────────────────┘
```

关键数据结构：

- `struct super_block`：每个已挂载文件系统的状态；指向根 inode
- `struct inode`：每个文件的元数据（uid、gid、大小、时间戳、块指针）；不含文件名
- `struct dentry`：名字到 inode 的缓存项；在内存中构成目录树
- `struct file`：每个已打开文件的状态；当前偏移量、标志；指向 inode

关键操作表：`struct file_operations` —— `open`、`read`、`write`、`mmap`、`ioctl`、`fsync`、`llseek`。每个文件系统都注册这些回调的**自己的实现**。`mmap` 对 AI 工作负载至关重要：**内存映射的数据集文件**完全避免 read() 系统调用。

> **关键洞察：** 当你在 ext4 文件系统上调用 `open("/data/frames/frame0001.raw", O_RDONLY)`，随后又在 btrfs 文件系统上调用时，得到的文件描述符相同、`read()` API 相同、从应用视角看行为相同。VFS 正是其可行之因。文件系统之间的差异在于底层发生了什么——块如何分配、崩溃如何处理、元数据操作完成得多快。

---

## ext4

ext4 是**Linux 默认文件系统**；向后兼容 ext2/ext3。

ext4 是一个成熟、经过充分测试的文件系统，优先考虑**可靠性与广泛的硬件兼容性**。它是通用 Linux 根分区——包括 Jetson rootfs——的正确选择，在这类场景中，可预测的行为比高级特性更重要。

### 关键特性

- **基于 extent 的分配**：取代间接块映射；一个 extent = (逻辑块, 物理块, 长度)；为大块连续文件减少元数据
- **64 位块地址**：支持最大 1 EB 的卷
- **延迟分配**（`delalloc`）：在回写时批量分配块；改善空间局部性；`nodelalloc` 可禁用它
- **dir_index**（htree）：大目录以哈希 B 树存储；O(log n) 查找，对比 O(n) 线性扫描

> **关键洞察：** ext4 的 extent 格式对 AI 数据集性能很重要。由单个 extent 描述的大文件（逻辑块 0 → 物理块 1000，长度 100K 块）可在一次连续 DMA 操作中读取。由数百个间接块指针描述的文件则需要许多次独立的元数据查找。对于大型训练数据集文件，采用 extent 分配的 ext4 接近顺序读性能。


<details>
<summary>English original</summary>

**Lecture 21: Filesystems: ext4, btrfs, F2FS & overlayfs**

**Overview**

A filesystem is the layer between your application and raw storage. It is easy to treat filesystems as invisible infrastructure — you open a file, write data, close it, and assume the data persists. But the choice of filesystem has direct, measurable consequences for AI hardware systems: it determines how fast camera frames land on storage, how long a reboot takes after a crash, whether a failed OTA update bricks a device or safely rolls back, and how long an eMMC chip lasts before wearing out.

The mental model to carry through this lecture is the **layered storage stack**: applications talk to the VFS (Virtual File System) layer, which provides a uniform API. Below VFS sits the actual filesystem implementation (ext4, btrfs, F2FS). Below that is the block layer, which routes I/O to the physical device. Each layer makes decisions that affect the layers above it.

AI hardware engineers need to understand filesystems because edge AI devices (dashcams, robotics controllers) write continuously to flash storage, which wears out. OTA update reliability depends on filesystem snapshot and rollback capabilities. Containerized inference deployments use overlayfs for immutable base images. And the IPC mechanism that ties together all of openpilot's processes runs on a RAM-backed filesystem (tmpfs).

---

**Filesystem Role and VFS**

A filesystem organizes files into directories, provides **persistence across power cycles**, manages free space, and ensures **metadata consistency after crashes**.

**VFS (Virtual File System)**

Linux VFS is an **abstraction layer** that presents a **uniform API** regardless of the underlying filesystem implementation.

The layered architecture looks like this:

```
Application
    │  open() read() write() mmap() fsync()
    ▼
┌─────────────────────────────────────────┐
│          VFS (Virtual File System)       │
│  struct file_operations dispatch table  │
│  dentry cache (name → inode)            │
│  inode cache (per-file metadata)        │
└──────┬──────────┬──────────┬────────────┘
       │          │          │
  ┌────▼───┐ ┌───▼───┐ ┌───▼────┐
  │  ext4  │ │ btrfs │ │  F2FS  │  ... (any registered filesystem)
  └────┬───┘ └───┬───┘ └───┬────┘
       └─────────┴──────────┘
                 │
    ┌────────────▼────────────┐
    │      Block Layer        │
    │  blk-mq, I/O scheduler  │
    └────────────┬────────────┘
                 │
    ┌────────────▼────────────┐
    │    Storage Device       │
    │  NVMe / eMMC / UFS      │
    └─────────────────────────┘
```

Key data structures:

- `struct super_block`: per-mounted-filesystem state; points to root inode
- `struct inode`: per-file metadata (uid, gid, size, timestamps, block pointers); no filename
- `struct dentry`: name-to-inode cache entry; forms the directory tree in memory
- `struct file`: per-open-file state; current offset, flags; points to inode

Key operations table: `struct file_operations` — `open`, `read`, `write`, `mmap`, `ioctl`, `fsync`, `llseek`. Every filesystem registers its **own implementation** of these callbacks. `mmap` is critical for AI workloads: **memory-mapped dataset files** avoid read() syscalls entirely.

> **Key Insight:** When you call `open("/data/frames/frame0001.raw", O_RDONLY)` on an ext4 filesystem and then again on a btrfs filesystem, you get the same file descriptor, the same `read()` API, the same behavior from the application's perspective. VFS is the reason this works. What differs between filesystems is what happens underneath — how blocks are allocated, how crashes are handled, and how fast metadata operations complete.

---

**ext4**

ext4 is the **default Linux filesystem**; backward-compatible with ext2/ext3.

ext4 is a mature, well-tested filesystem that prioritizes **reliability and broad hardware compatibility**. It is the right choice for general-purpose Linux root partitions — including Jetson rootfs — where predictable behavior matters more than advanced features.

**Key Features**

- **Extent-based allocation**: replaces indirect block maps; one extent = (logical block, physical block, length); reduces metadata for large contiguous files
- **64-bit block addresses**: supports volumes up to 1 EB
- **Delayed allocation** (`delalloc`): batch block allocation at writeback time; improves spatial locality; `nodelalloc` disables it
- **dir_index** (htree): large directories stored as hash B-tree; O(log n) lookup vs O(n) linear scan

> **Key Insight:** The ext4 extent format matters for AI dataset performance. A large file described by a single extent (logical block 0 → physical block 1000, length 100K blocks) can be read in one contiguous DMA operation. A file described by hundreds of indirect block pointers requires many separate metadata lookups. For large training dataset files, ext4 with extent allocation approaches sequential read performance.

</details>

### 日志模式

| 模式 | 记录日志的内容 | 数据安全性 | 性能 |
|---|---|---|---|
| `data=ordered`（默认） | 仅元数据；数据在提交前写入 | 良好 | 快 |
| `data=journal` | 元数据 + 数据 | 最佳 | 最慢 |
| `data=writeback` | 仅元数据；不保证数据顺序 | 最弱 | 最快 |

用 `mount -o data=ordered /dev/mmcblk0p1 /mnt` 挂载。对于 Jetson rootfs，`data=ordered` 是**标准做法**。

默认日志大小：128 MB（可通过 `tune2fs -J size=N` 调整）。日志是**环形日志**；崩溃时，未提交的事务被丢弃；已提交但未建立检查点的会被重放。

`data=ordered` 模式的崩溃恢复流程：

1. **文件数据先写入磁盘**，之后任何引用它的元数据日志条目才会被提交。这确保如果系统在日志提交之后崩溃，日志所指向的数据块已经在磁盘上。
2. **日志提交**向环形日志写入一个提交块，使元数据变更持久化。
3. **检查点**：在之后的某个时刻，已记录日志的元数据被写入其在文件系统中的最终位置。只有到这时，日志空间才能被复用。
4. **崩溃恢复时**：`e2fsck` 读取日志，重放已提交但未建立检查点的条目，丢弃未提交的条目。日志干净意味着无需 fsck。

> **常见陷阱：** 有时会为了最大写入吞吐而使用 `data=writeback`（例如用在临时数据分区上）。危险在于，崩溃之后，一个已提交的元数据条目（文件大小、块指针）可能引用尚未刷盘的数据块，导致看起来已成功写入的文件中出现垃圾数据。对于必须能在崩溃后存活的应用数据，绝不使用 `data=writeback`。

---

## btrfs

btrfs 是一个**写时复制 B-tree 文件系统**，其特性面向现代存储与系统管理而设计。ext4 优先考虑简单与兼容，而 btrfs 优先考虑**高级特性**——尤其是快照和校验和——这些对 OTA 升级策略至关重要。

### 核心特性

- **写时复制（CoW）**：写入绝不就地覆盖已有数据；新数据写入空闲空间，然后原子更新 B-tree 根指针
- **快照**：创建耗时为 O(1)；快照只是一个指向相同叶节点的新 B-tree 根；在 OTA 升级策略中用于 A/B rootfs
- **子卷**：同一 btrfs 池内相互独立的文件系统树；可分别挂载，也可单独打快照
- **校验和**：对数据和元数据使用 CRC32c（默认）、xxHash、SHA256 或 Blake2；可检测静默 bit rot
- **RAID 模式**：跨多设备的 RAID 0、1、10、5、6
- **透明压缩**：zstd（压缩率最佳）、lzo（最快）、zlib；可按文件或按子卷设置
- **send/receive**：计算两个快照之间的增量并流式传输到另一系统；支持增量 OTA 升级

> **关键洞察：** btrfs 快照创建是 O(1)，因为快照只是一个新的 B-tree 根，与原文件系统共享相同的叶节点。不复制任何数据。原文件系统与快照都引用相同的物理块。当其中任一方被修改时，CoW 会为被修改的数据创建新块——另一方快照继续不受影响地引用原块。这正是实现即时 A/B rootfs 切换的原因。

### 用 btrfs 快照实现 A/B Rootfs

```bash
# Create read-only snapshot before OTA
btrfs subvolume snapshot -r /rootfs /.snapshots/$(date +%Y%m%d)

# After update, create new snapshot from updated rootfs
btrfs subvolume snapshot /rootfs_new /.snapshots/new

# Rollback: delete new rootfs, restore from snapshot
```

使用 btrfs 快照的 A/B OTA 流程如下：

1. **更新开始前**：为当前 rootfs 创建一个只读快照。耗时几毫秒，不占用额外磁盘空间。
2. **应用更新**：把新文件、软件包或完整 rootfs 镜像写入一个新子卷。旧 rootfs 的快照不受影响。
3. **重启进入新 rootfs**：bootloader 把新子卷挂载为活动 rootfs。
4. **如果新 rootfs 启动失败**：bootloader 检测到失败（启动计数器超过阈值），把旧快照挂载为活动 rootfs。零数据搬移的完整回滚。
5. **启动成功后**：可删除旧快照以回收空间。

`btrfs scrub start /`：用已存储的校验和校验所有数据块；后台操作；对长期运行的边缘 AI 设备至关重要，因为**静默损坏**是这类设备的隐患。

> **常见陷阱：** btrfs 的 CoW 会为数据库类工作负载（对同一文件区域的大量小规模随机更新）带来额外的写放大。在 btrfs 上，SQLite 数据库和 RocksDB 的性能可能不如在 ext4 上，因为每次小写入都会复制整个 B-tree 叶节点。对 btrfs 上的数据库文件使用 `chattr +C`（对特定文件或目录禁用 CoW），或把数据库文件放到单独的 ext4 分区上。

---


<details>
<summary>English original</summary>

**Journal Modes**

| Mode | What is journaled | Data safety | Performance |
|---|---|---|---|
| `data=ordered` (default) | Metadata only; data written before commit | Good | Fast |
| `data=journal` | Metadata + data | Best | Slowest |
| `data=writeback` | Metadata only; data order not guaranteed | Weakest | Fastest |

Mount with `mount -o data=ordered /dev/mmcblk0p1 /mnt`. For Jetson rootfs, `data=ordered` is the **standard**.

Default journal size: 128 MB (tunable via `tune2fs -J size=N`). Journal is a **circular log**; on crash, uncommitted transactions are discarded; committed but not checkpointed are replayed.

The crash recovery sequence for `data=ordered` mode:

1. **File data is written to disk** before any metadata journal entry referencing it is committed. This ensures that if the system crashes after the journal commit, the data blocks it points to are already on disk.
2. **Journal commit** writes a commit block to the circular journal log, making the metadata change durable.
3. **Checkpoint**: at some later time, the journaled metadata is written to its final location in the filesystem. Only then can the journal space be reused.
4. **On crash recovery**: `e2fsck` reads the journal, replays committed but uncheckpointed entries, discards uncommitted entries. A clean journal means no fsck needed.

> **Common Pitfall:** `data=writeback` is sometimes used for maximum write throughput (e.g., on a temporary data partition). The danger is that after a crash, a committed metadata entry (file size, block pointer) may reference data blocks that were not yet flushed, resulting in garbage data in files that appeared successfully written. Never use `data=writeback` for application data that must survive a crash.

---

**btrfs**

btrfs is a **Copy-on-Write B-tree filesystem** with features designed for modern storage and system management. Where ext4 prioritizes simplicity and compatibility, btrfs prioritizes **advanced features** — particularly snapshots and checksums — that are critical for OTA update strategies.

**Core Features**

- **Copy-on-Write (CoW)**: writes never overwrite existing data in place; new data is written to free space, then the B-tree root pointer is atomically updated
- **Snapshots**: O(1) creation; a snapshot is simply a new B-tree root pointing to the same leaf nodes; used for A/B rootfs in OTA update strategies
- **Subvolumes**: independent filesystem trees within the same btrfs pool; can be mounted separately or snapshotted individually
- **Checksums**: CRC32c (default), xxHash, SHA256, or Blake2 on both data and metadata; detects silent bit rot
- **RAID modes**: RAID 0, 1, 10, 5, 6 across multiple devices
- **Transparent compression**: zstd (best ratio), lzo (fastest), zlib; per-file or per-subvolume
- **send/receive**: compute a delta between two snapshots and stream it to another system; enables incremental OTA updates

> **Key Insight:** btrfs snapshot creation is O(1) because a snapshot is just a new B-tree root that shares the same leaf nodes as the original. No data is copied. The original and the snapshot both reference the same physical blocks. When either is modified, CoW creates new blocks for the modified data — the other snapshot continues to reference the original blocks undisturbed. This is what makes instant A/B rootfs switching possible.

**A/B Rootfs with btrfs Snapshots**

```bash
# Create read-only snapshot before OTA
btrfs subvolume snapshot -r /rootfs /.snapshots/$(date +%Y%m%d)

# After update, create new snapshot from updated rootfs
btrfs subvolume snapshot /rootfs_new /.snapshots/new

# Rollback: delete new rootfs, restore from snapshot
```

The A/B OTA flow with btrfs snapshots works as follows:

1. **Before the update begins**: create a read-only snapshot of the current rootfs. This takes milliseconds and uses no additional disk space.
2. **Apply the update**: write new files, packages, or a full rootfs image to a new subvolume. The snapshot of the old rootfs is untouched.
3. **Reboot into the new rootfs**: bootloader mounts the new subvolume as the active rootfs.
4. **If the new rootfs fails to boot**: bootloader detects the failure (boot counter exceeds threshold) and mounts the old snapshot as the active rootfs. Full rollback with zero data movement.
5. **After a successful boot**: the old snapshot can be deleted to reclaim space.

`btrfs scrub start /`: verify all data blocks against stored checksums; background operation; critical for long-running edge AI devices where **silent corruption** is a concern.

> **Common Pitfall:** btrfs CoW creates extra write amplification for database-style workloads (many small, random updates to the same file regions). SQLite databases and RocksDB on btrfs can perform worse than on ext4 because each small write copies an entire B-tree leaf node. Use `chattr +C` (disable CoW for specific files or directories) for database files on btrfs, or place database files on a separate ext4 partition.

---

</details>

## F2FS (Flash-Friendly File System)

在理解 ext4 和 btrfs 之后，接下来转向专为 AI 边缘设备所用存储硬件设计的文件系统：eMMC 和 UFS 封装中的 NAND flash。

F2FS 专为 **NAND flash 存储** 设计：eMMC、UFS 和 NVMe SSD。由 Samsung 开发；合入 Linux 3.8。

### 设计原则

- **日志结构，节点/数据分离**：节点区域（inode 和索引）与数据区域分开记录；减少混合更新造成的碎片
- **自适应日志**：在普通日志（高利用率）与线程化日志（低利用率）之间切换，以平衡写放大和碎片
- **降低写放大**：flash 感知的分配避免部分块更新；将写入对齐到 flash 擦除块大小
- **优化 `fsync()` 延迟**：对数据库工作负载（Android 上的 SQLite）很重要；使用 roll-forward 恢复机制，避免每次 fsync 都进行完整检查点

> **关键洞察：** Flash 存储器存在一个根本性的不对称：可以写入小块，但必须擦除大块（通常为 256 KB–2 MB）。写入小粒度随机更新的文件系统会迫使 flash 控制器对大擦除块进行读-改-写（写放大）。F2FS 的日志结构设计按顺序收集写入，将其对齐到擦除块边界，并大幅降低这种放大——从而延长 flash 芯片的寿命。

使用场景：Android（自 Android 9 起在 eMMC/UFS 上默认）、Chromebook、带 eMMC 存储的嵌入式 AI 设备。

---

## overlayfs

overlayfs 是让 **容器、OTA overlay 和只读 rootfs** 配置得以工作的机制。理解它对于使用 Docker 容器和 Jetson OTA 策略是必需的。

overlayfs 将 **两棵目录树** 叠加为统一视图：

- **lower**：只读基础 layer（一个或多个堆叠的 layer）
- **upper**：可写 layer；接收所有修改
- **workdir**：与 `upper` 位于同一文件系统的临时目录；用于原子重命名操作

首次对来自 `lower` 的文件执行写入时，该文件会**向上复制到 `upper`**（写时复制）。后续写入直接进入 `upper`。

```
Application sees /container/merged (unified view):
  /bin/bash       ← from lower layer (image)
  /lib/libc.so    ← from lower layer (image)
  /etc/config     ← from upper layer (modified by container)
  /tmp/cache      ← from upper layer (created by container)

Physical layout:
┌────────────────────────────────────────────────────────┐
│  upper (writable, per-container)                        │
│  /etc/config   /tmp/cache                               │
├────────────────────────────────────────────────────────┤
│  lower layer 2 (image layer, read-only)                 │
│  /etc/config.orig   (shadowed by upper /etc/config)     │
├────────────────────────────────────────────────────────┤
│  lower layer 1 (base image, read-only)                  │
│  /bin/bash   /lib/libc.so                               │
└────────────────────────────────────────────────────────┘
overlayfs merges these: upper takes precedence, lower fills in the rest
```

### 挂载语法

```bash
mount -t overlay overlay \
  -o lowerdir=/image/layer2:/image/layer1,\   # colon-separated, top-to-bottom
     upperdir=/container/rw,\                 # writable layer (must be empty)
     workdir=/container/work \                # must be on same fs as upperdir
  /container/merged                           # unified view mount point
```

### 使用场景

- **Docker/OCI 容器**：镜像 layer 形成 `lower` 栈；容器的可写 layer 是 `upper`；容器将 `/merged` 视为其 rootfs
- **Jetson OTA**：新的 rootfs 镜像作为 `lower`；持久化用户数据位于 `upper`；避免为配置进行完整 rootfs 复制
- **带可写 overlay 的只读 rootfs**：从只读 squashfs 启动；将 tmpfs 作为 `upper` 挂载 overlayfs；可写 rootfs 在 RAM 中，不修改 flash

> **常见陷阱：** `upper` 和 `workdir` 目录必须位于同一文件系统（同一挂载点）。这是因为 overlayfs 在 workdir 内使用原子重命名，以保证 copy-up 操作安全。将 workdir 放在与 upper 不同的文件系统会导致挂载失败并报 `EINVAL`。常见错误是将 workdir 放在 tmpfs 上、将 upper 放在 ext4 上（或反之）。

---


<details>
<summary>English original</summary>

**F2FS (Flash-Friendly File System)**

With ext4 and btrfs understood, we turn to the filesystem designed specifically for the storage hardware used in AI edge devices: NAND flash in eMMC and UFS packages.

F2FS is designed for **NAND flash storage**: eMMC, UFS, and NVMe SSDs. Developed by Samsung; merged into Linux 3.8.

**Design Principles**

- **Log-structured with node/data separation**: node area (inodes and index) and data area are logged separately; reduces fragmentation from mixed updates
- **Adaptive logging**: switches between normal logging (high utilization) and threaded logging (low utilization) to balance write amplification and fragmentation
- **Reduced write amplification**: flash-aware allocation avoids partial block updates; aligns writes to flash erase block size
- **Optimized `fsync()` latency**: important for database workloads (SQLite on Android); uses a roll-forward recovery mechanism to avoid full checkpoint on every fsync

> **Key Insight:** Flash memory has a fundamental asymmetry: you can write small chunks but must erase large chunks (typically 256 KB–2 MB). A filesystem that writes small random updates forces the flash controller to read-modify-write large erase blocks (write amplification). F2FS's log-structured design collects writes sequentially, aligning them to erase block boundaries and dramatically reducing this amplification — extending the life of the flash chip.

Use cases: Android (since Android 9 default on eMMC/UFS), Chromebooks, embedded AI devices with eMMC storage.

---

**overlayfs**

overlayfs is the mechanism that makes **containers, OTA overlays, and read-only rootfs** configurations work. Understanding it is required for working with Docker containers and Jetson OTA strategies.

overlayfs overlays **two directory trees** into a unified view:

- **lower**: read-only base layer (one or more stacked layers)
- **upper**: writable layer; receives all modifications
- **workdir**: temporary directory on the same filesystem as `upper`; used for atomic rename operations

On first write to a file from `lower`, the file is **copied up to `upper`** (copy-on-write). Subsequent writes go directly to `upper`.

```
Application sees /container/merged (unified view):
  /bin/bash       ← from lower layer (image)
  /lib/libc.so    ← from lower layer (image)
  /etc/config     ← from upper layer (modified by container)
  /tmp/cache      ← from upper layer (created by container)

Physical layout:
┌────────────────────────────────────────────────────────┐
│  upper (writable, per-container)                        │
│  /etc/config   /tmp/cache                               │
├────────────────────────────────────────────────────────┤
│  lower layer 2 (image layer, read-only)                 │
│  /etc/config.orig   (shadowed by upper /etc/config)     │
├────────────────────────────────────────────────────────┤
│  lower layer 1 (base image, read-only)                  │
│  /bin/bash   /lib/libc.so                               │
└────────────────────────────────────────────────────────┘
overlayfs merges these: upper takes precedence, lower fills in the rest
```

**Mount Syntax**

```bash
mount -t overlay overlay \
  -o lowerdir=/image/layer2:/image/layer1,\   # colon-separated, top-to-bottom
     upperdir=/container/rw,\                 # writable layer (must be empty)
     workdir=/container/work \                # must be on same fs as upperdir
  /container/merged                           # unified view mount point
```

**Use Cases**

- **Docker/OCI containers**: image layers form the `lower` stack; the container's writable layer is `upper`; the container sees `/merged` as its rootfs
- **Jetson OTA**: new rootfs image as `lower`; persistent user data in `upper`; avoids a full rootfs copy for configuration
- **Read-only rootfs with writable overlay**: boot from read-only squashfs; mount overlayfs with tmpfs as `upper`; writable rootfs in RAM without modifying flash

> **Common Pitfall:** The `upper` and `workdir` directories must reside on the same filesystem (same mount point). This is because overlayfs uses atomic renames within workdir for safe copy-up operations. Placing workdir on a different filesystem than upper causes the mount to fail with `EINVAL`. A common mistake is placing workdir on tmpfs and upper on ext4 (or vice versa).

---

</details>

## tmpfs：内存中的文件系统

overlayfs 常将 tmpfs 用作其可写的 `upper` layer，以支持临时容器。但 tmpfs 自身还有重要的独立角色：它是 Linux 中所有共享内存 IPC 的基础。

`tmpfs` 使用匿名页（RAM + swap）作为存储；读取和写入均**无磁盘 I/O**。

```bash
mount -t tmpfs -o size=1G tmpfs /dev/shm  # explicit tmpfs mount, 1 GB limit
```

- 在 systemd 系统上，`/dev/shm` 和 `/run` 默认即为 tmpfs
- `shm_open()` / `mmap(MAP_SHARED)` 使用 tmpfs 实现 POSIX 共享内存
- openpilot 的 `cereal` msgq 在 `/dev/shm` 上通过 `shm_open()` 分配共享内存段；进程之间的所有 IPC（modeld、plannerd、controllsd）都经过这些以 RAM 为后端的段

> **关键洞察：** 当 openpilot 的 `modeld` 发布模型输出（轨迹预测、检测到的目标）而 `plannerd` 读取它时，数据从不接触磁盘。由 `shm_open("/cereal_modeld", ...)` 创建的“文件”只存在于 tmpfs 中——也就是 RAM 中。多进程架构正是以此实现与单进程设计直接写内存相同的低延迟 IPC。

---

## TRIM 与闪存寿命

每当 OS 在以闪存为后端的文件系统上删除文件，它知道那些块已空闲——但**闪存硬件并不会自动得知这一点**。TRIM 弥合了这一差距。

`fstrim` 通知 SSD **已释放的块范围**，以便控制器在空闲时回收它们。

- `discard` 挂载选项：每次 `unlink()` 时内联发出 TRIM（每次删除的延迟更高）
- `systemd-fstrim.timer`：每周一次 `fstrim` 扫描（更推荐；批量执行 TRIM）
- 没有 TRIM，FTL 不知道哪些逻辑块是空闲的；随着驱动器老化，写放大加剧
- 对带有 eMMC 的嵌入式 AI 设备（行车记录仪、边缘推理盒子）至关重要，这类设备持续写入，且无法离线维护

> **常见陷阱：** 部署基于 eMMC 的 AI 边缘设备时，`discard` 处于禁用状态，而 `systemd-fstrim.timer` 也未启用。在持续摄像头录制数周之后，FTL 的映射表中塞满了“活跃”块——OS 知道这些块空闲，但 SSD 并不知道。写放大增长，吞吐下降，最终写延迟尖峰导致录制流水线掉帧。修复很简单——启用定时器——但事后诊断根因需要把一段时间内 `iostat` 吞吐的退化与设备使用时长关联起来。

---

## 小结

| 文件系统 | 日志 | CoW | 最适用场景 | 关键限制 |
|---|---|---|---|---|
| ext4 | 是（ordered/journal/writeback） | 否 | 通用 Linux rootfs | 不支持快照 |
| btrfs | 否（CoW 取代日志） | 是 | A/B OTA、基于快照的回滚 | CPU 开销更高 |
| F2FS | 检查点 | 否 | eMMC/UFS 嵌入式设备 | 不适合 HDD |
| overlayfs | 继承自 upper layer | 首次写入时 | 容器 rootfs、OTA overlay | upper/lower 必须不同 |
| tmpfs | 无（RAM） | 否 | IPC 共享内存、/tmp | 重启/OOM 后丢失 |
| ext4 + F2FS | 是 | 否 | 混合：rootfs（ext4）+ 数据（F2FS） | 需要独立分区 |


<details>
<summary>English original</summary>

**tmpfs: Filesystem in RAM**

overlayfs often uses tmpfs as its writable `upper` layer for ephemeral containers. But tmpfs has its own important standalone role: it is the foundation of all shared memory IPC in Linux.

`tmpfs` uses anonymous pages (RAM + swap) for storage; **no disk I/O** for reads or writes.

```bash
mount -t tmpfs -o size=1G tmpfs /dev/shm  # explicit tmpfs mount, 1 GB limit
```

- `/dev/shm` and `/run` are tmpfs by default on systemd systems
- `shm_open()` / `mmap(MAP_SHARED)` use tmpfs for POSIX shared memory
- openpilot `cereal` msgq allocates shared memory segments via `shm_open()` on `/dev/shm`; all IPC between processes (modeld, plannerd, controlsd) traverses these RAM-backed segments

> **Key Insight:** When openpilot's `modeld` publishes a model output (trajectory prediction, detected objects) and `plannerd` reads it, the data never touches the disk. The "file" created by `shm_open("/cereal_modeld", ...)` exists only in tmpfs — in RAM. This is how a multi-process architecture achieves the same low-latency IPC as a single-process design with direct memory writes.

---

**TRIM and Flash Longevity**

Every time the OS deletes a file on a flash-backed filesystem, it knows those blocks are now free — but the **flash hardware does not automatically learn this**. TRIM bridges this gap.

`fstrim` notifies the SSD of **freed block ranges** so the controller can reclaim them during idle time.

- `discard` mount option: issue TRIM inline on every `unlink()` (higher latency per delete)
- `systemd-fstrim.timer`: weekly `fstrim` sweep (preferred; batches TRIMs)
- Without TRIM, the FTL does not know which logical blocks are free; write amplification increases as the drive ages
- Critical for embedded AI devices with eMMC (dashcams, edge inference boxes) that write continuously and cannot be taken offline for maintenance

> **Common Pitfall:** Deploying an eMMC-based AI edge device with `discard` disabled and `systemd-fstrim.timer` not enabled. Over weeks of continuous camera recording, the FTL fills its mapping table with "live" blocks that the OS knows are free but the SSD does not. Write amplification grows, throughput drops, and eventually write latency spikes cause frame drops in the recording pipeline. The fix is simple — enable the timer — but diagnosing the root cause after the fact requires correlating `iostat` throughput degradation over time with device age.

---

**Summary**

| Filesystem | Journaling | CoW | Best for | Key limitation |
|---|---|---|---|---|
| ext4 | Yes (ordered/journal/writeback) | No | General-purpose Linux rootfs | No snapshot support |
| btrfs | No (CoW replaces journal) | Yes | A/B OTA, snapshot-based rollback | Higher CPU overhead |
| F2FS | Checkpointing | No | eMMC/UFS embedded devices | Not ideal for HDDs |
| overlayfs | Inherits from upper layer | On first write | Container rootfs, OTA overlay | upper/lower must differ |
| tmpfs | None (RAM) | No | IPC shared memory, /tmp | Lost on reboot/OOM |
| ext4 + F2FS | Yes | No | Mixed: rootfs (ext4) + data (F2FS) | Separate partitions needed |

</details>

### 概念回顾

- **为什么 ext4 默认使用 `data=ordered` 模式而不是 `data=journal`？** 完整数据日志（`data=journal`）会把每一个数据字节写两遍：一次写入日志，一次写入其最终位置。对于以高吞吐录制相机帧的设备，这会使写入负载翻倍。`data=ordered` 提供强崩溃一致性（数据在元数据提交到日志之前已落盘），代价是只对元数据做日志——数据量小得多。

- **是什么让 btrfs 快照在时间上为 O(1)、在空间上初始为 O(0)？** btrfs 快照是一个新的 B-tree 根节点，它指向与原 subvolume 相同的叶节点。创建它只需写入一个新的根节点——无论文件系统中有多少个文件。快照时不会复制任何数据块。只有当其中一个版本（原始或快照）通过写入与另一个版本产生分歧时才会消耗空间，这会触发 CoW。

- **在 eMMC 上做连续录制工作负载时，为什么倾向于选 F2FS 而不是 ext4？** eMMC NAND flash 要求先擦除后写入，且以大的擦除块为粒度。ext4 的随机写入模式迫使 eMMC FTL 频繁执行读-改-写周期，使物理写入次数成倍增加（写放大）。F2FS 的日志结构设计将写入按顺序聚合，匹配 flash 的自然写入粒度并降低写放大，延长设备的 P/E 周期预算。

- **overlayfs 如何实现不可变容器镜像？** 基础镜像层以 `lower`（只读）挂载。容器所做的任何修改——安装包、创建日志文件——都写入 `upper`（可写）层。磁盘上的基础镜像永远不会被修改。容器停止时，`upper` 层可以被丢弃，基础镜像保持原样。多个容器可以同时共享同一批基础镜像层，不会产生冲突。

- **tmpfs 在 openpilot 的 IPC 架构中扮演什么角色？** openpilot 进程（modeld、plannerd、controlsd、camerad）通过 cereal msgq 通信，后者使用 POSIX 共享内存（`shm_open()`），底层是 `/dev/shm`——一个 tmpfs 挂载点。进程之间的消息写入 RAM，而不是磁盘。这使进程间通信具有与直接内存写入相同的延迟，且没有文件系统开销。

- **没有 TRIM 的 eMMC 上，写放大随时间推移会怎样？** Flash Translation Layer（FTL）维护逻辑块到物理块的映射。删除文件时，OS 会释放逻辑块。没有 TRIM 时，FTL 仍认为那些物理块是「live」，不先擦除就无法把它们用于新的写入。随着设备被这些僵尸块填满，FTL 在任何新写入落地之前，必须越来越多地对部分使用的擦除块执行垃圾回收（擦除 + 拷贝），在最坏情况下使有效写放大增至 10–50 倍。

---

## AI 硬件关联

- btrfs 快照在 openpilot Agnos 与 Jetson OTA 更新流水线中提供 O(1) 的 A/B rootfs 切换；启动失败时，bootloader 激活上一个快照，无需移动数据
- eMMC 上的 F2FS 显著降低进行连续相机录制的嵌入式 AI 边缘设备（行车记录仪、机器人控制器）的写放大
- overlayfs 使容器化的 TensorRT 推理部署成为可能，其中基础镜像层只读且不可变，而容器仅在上层添加 runtime 状态
- tmpfs 与 `shm_open()` 是 openpilot cereal msgq IPC 的支撑；进程之间的所有模型输出、轨迹规划与控制命令都经由 RAM 支撑的共享内存段传递
- ext4 `data=ordered` 模式是 Jetson rootfs 分区的标准配置；它在没有完整数据日志开销的前提下提供最强的崩溃一致性保证
- 对于任何长期运行的基于 eMMC 的 AI 边缘设备，`systemd-fstrim.timer` 都是必配项；缺少它，写放大会降低吞吐并缩短 flash 寿命


<details>
<summary>English original</summary>

**Conceptual Review**

- **Why does ext4 use `data=ordered` mode by default instead of `data=journal`?** Full data journaling (`data=journal`) writes every byte of data twice: once to the journal, once to its final location. For a device recording camera frames at high throughput, this doubles the write load. `data=ordered` provides strong crash consistency (data is on disk before metadata commits to the journal) at the cost of only journaling metadata — a much smaller amount of data.

- **What makes btrfs snapshots O(1) in time and initially O(0) in space?** A btrfs snapshot is a new B-tree root node that points to the same leaf nodes as the original subvolume. Creating it requires writing only one new root node — regardless of how many files are in the filesystem. No data blocks are duplicated at snapshot time. Space is only consumed when one version (original or snapshot) diverges from the other through writes, which triggers CoW.

- **Why is F2FS preferred over ext4 on eMMC for continuous recording workloads?** eMMC NAND flash requires erase-before-write at the granularity of large erase blocks. ext4's random-write patterns force the eMMC FTL to perform read-modify-write cycles frequently, multiplying the physical write count (write amplification). F2FS's log-structured design aggregates writes sequentially, matching the natural write granularity of flash and reducing write amplification, extending the device's P/E cycle budget.

- **How does overlayfs enable immutable container images?** The base image layers are mounted as `lower` (read-only). Any modification the container makes — installing a package, creating a log file — goes to the `upper` (writable) layer. The base image on disk is never modified. When the container stops, the `upper` layer can be discarded, leaving the base image exactly as it was. Multiple containers can share the same base image layers simultaneously with no conflicts.

- **What role does tmpfs play in openpilot's IPC architecture?** openpilot processes (modeld, plannerd, controlsd, camerad) communicate via cereal msgq, which uses POSIX shared memory (`shm_open()`) backed by `/dev/shm` — a tmpfs mount. Messages between processes are written to RAM, not disk. This gives inter-process communication the same latency as direct memory writes, with no filesystem overhead.

- **What happens to write amplification on an eMMC without TRIM over time?** The Flash Translation Layer (FTL) maintains a logical-to-physical block mapping. When a file is deleted, the OS frees the logical blocks. Without TRIM, the FTL still considers those physical blocks "live" and cannot use them for new writes without first erasing them. As the device fills with these zombie blocks, the FTL must increasingly perform garbage collection (erase + copy) on partially-used erase blocks before any new write can land, multiplying the effective write amplification by 10–50x in the worst case.

---

**AI Hardware Connection**

- btrfs snapshots provide O(1) A/B rootfs switching in openpilot Agnos and Jetson OTA update pipelines; on failed boot, the bootloader activates the previous snapshot without data movement
- F2FS on eMMC significantly reduces write amplification in embedded AI edge devices (dashcams, robotics controllers) that perform continuous camera recording
- overlayfs enables containerized TensorRT inference deployments where the base image layer is read-only and immutable while the container adds only runtime state in the upper layer
- tmpfs and `shm_open()` underpin openpilot cereal msgq IPC; all model outputs, trajectory plans, and control commands between processes traverse RAM-backed shared memory segments
- ext4 `data=ordered` mode is the standard for Jetson rootfs partitions; it provides the strongest crash consistency guarantee without the overhead of full data journaling
- `systemd-fstrim.timer` is a mandatory configuration item for any long-running eMMC-based AI edge device; without it, write amplification degrades throughput and reduces flash lifespan

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-21.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-21.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
