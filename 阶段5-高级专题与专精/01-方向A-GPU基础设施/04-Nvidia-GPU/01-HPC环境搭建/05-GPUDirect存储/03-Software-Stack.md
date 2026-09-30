---
title: 03 — 软件栈与安装
description: 03 — 软件栈与安装
---

# 03 — 软件栈与安装

## 1. GDS 软件架构

```
┌────────────────────────────────────────────────────────┐
│  Application (PyTorch DataLoader, custom training loop) │
├────────────────────────────────────────────────────────┤
│  libcufile (GDS user-space library)                    │
│  cuFile API: cuFileRead(), cuFileWrite(), cuFileBufReg()│
├────────────────────────────────────────────────────────┤
│  cuFile Daemon (cufiledaemon)                          │
│  Manages file registration, policy, async coordination │
├────────────────────────────────────────────────────────┤
│  nvidia-fs (kernel module)                             │
│  Programs DMA engines, maps GPU BAR, handles RDMA      │
├───────────────────────┬────────────────────────────────┤
│  NVIDIA Driver        │  Mellanox OFED (RDMA for NVMe-oF)│
├───────────────────────┴────────────────────────────────┤
│  Linux Kernel (5.4+)                                   │
└────────────────────────────────────────────────────────┘
```

---

## 2. 安装

### 步骤 1：前置要求

```bash
# Verify kernel version (5.4+ required, 5.14+ recommended per WD brief)
uname -r
# Expected: 5.14.0-70.70.1.el9_0.x86_64 (RHEL 9) or similar

# Verify GPU driver is installed
nvidia-smi
# Expected: Driver 535.86.10 or later

# Verify CUDA is installed
nvcc --version
# Expected: CUDA 12.2.1 or later

# Install kernel headers (needed to build nvidia-fs module)
# RHEL/CentOS:
sudo dnf install kernel-devel kernel-headers

# Ubuntu:
sudo apt install linux-headers-$(uname -r)
```

### 步骤 2：安装 Mellanox OFED（用于 NVMe-oF / RDMA）

```bash
# Download MLNX_OFED from NVIDIA networking portal
# Match version to reference: 5.8-3.0.7.0

wget https://linux.mellanox.com/public/repo/mlnx_ofed/5.8-3.0.7.0/rhel9.0/x86_64/MLNX_OFED_LINUX-5.8-3.0.7.0-rhel9.0-x86_64.tgz
tar xzf MLNX_OFED_LINUX-5.8-3.0.7.0-rhel9.0-x86_64.tgz
cd MLNX_OFED_LINUX-5.8-3.0.7.0-rhel9.0-x86_64

# Install with GDS-specific flags
sudo ./mlnxofedinstall \
    --with-nvmf-host-target \   # NVMe-oF host support
    --with-nfsrdma \            # RDMA for NFS (optional)
    --without-fw-update \       # skip firmware update (do separately)
    --add-kernel-support        # build kernel modules

sudo dracut -f   # update initramfs
sudo reboot
```

### 步骤 3：安装 GDS 软件包

```bash
# GDS is bundled with CUDA Toolkit extras and standalone package

# Method A: CUDA Toolkit installer (includes GDS)
# When running CUDA installer, select "GPUDirect Storage" component

# Method B: Standalone GDS install (RHEL/Rocky)
sudo dnf install cuda-cudart-12-2          # CUDA runtime
sudo dnf install libcufile-12-2            # GDS library
sudo dnf install libcufile-devel-12-2      # GDS headers
sudo dnf install nvidia-gds-12-2          # GDS metapackage (includes nvidia-fs)

# Verify nvidia-fs module is installed
sudo modprobe nvidia_fs
lsmod | grep nvidia_fs
# nvidia_fs   1234567  0
```

### 步骤 4：配置文件系统

GDS 需要特定的文件系统配置：

```bash
# Mount NVMe with GDS-compatible options
# ext4 — no special mount options needed
# xfs  — use: dax=never (avoid DAX mode conflicts)

# Check current mounts
mount | grep nvme

# Example /etc/fstab entry for NVMe with GDS
/dev/nvme0n1p1  /mnt/nvme0  ext4  defaults,noatime  0 2

# Re-mount without atime for performance
sudo mount -o remount,noatime /mnt/nvme0

# For NVMe-oF targets, connect via nvme-cli
sudo nvme connect \
    --transport rdma \
    --traddr 192.168.100.10 \      # OpenFlex IP
    --trsvcid 4420 \               # NVMe-oF port
    --nqn nqn.2020-08.org.nvmexpress:subsys1

# Verify connected NVMe-oF namespaces
nvme list
# /dev/nvme4n1   WD OpenFlex ...
```

### 步骤 5：启动 cuFile Daemon

```bash
# Start daemon (required for GDS operations)
sudo systemctl enable cufile.service
sudo systemctl start cufile.service
sudo systemctl status cufile.service

# Manual start (for testing)
sudo /usr/local/cuda/bin/cufile_daemon &

# Verify daemon is running
ps aux | grep cufile
```

---

## 3. GDS 配置文件

cuFile 守护进程通过 `/etc/cufile.json` 配置：

```json
{
  "logging": {
    "level": "WARN"
  },
  "profile": {
    "nvtx": false,
    "cufile_stats": 0
  },
  "execution": {
    "max_direct_io_size_kb": 16384,
    "max_device_cache_size_kb": 131072,
    "max_io_queue_depth": 128,
    "max_batch_io_timeout_msecs": 5,
    "num_threads_per_block": 32,
    "num_io_threads": 4
  },
  "properties": {
    "max_direct_io_size_kb": 16384,
    "use_poll_mode": false,
    "poll_mode_max_size_kb": 4,
    "max_batch_io_size_kb": 65536,
    "allow_compat_mode": true,      ← fallback to CPU path if GDS unavailable
    "gds_rdma_write_support": true
  },
  "fs": {
    "generic": {
      "posix_unaligned_writes": false,
      "posix_writes": false,
      "allow_sb_offload": false
    }
  }
}
```

关键参数：

| 参数 | 推荐值 | 效果 |
|---|---|---|
| `max_direct_io_size_kb` | 16384 (16 MB) | 单次 GDS 传输最大大小 |
| `max_device_cache_size_kb` | 131072 (128 MB) | GPU 侧 cuFile 缓存 |
| `max_io_queue_depth` | 128 | 并发 I/O 请求数 |
| `allow_compat_mode` | true | 静默回退到 CPU 路径 |
| `use_poll_mode` | false | 中断 vs 轮询（轮询：延迟更低，CPU 开销更大） |

---

## 4. 验证 GDS 是否激活

### gdscheck —— 必备验证工具

```bash
# Run GDS compatibility check
/usr/local/cuda/gds/tools/gdscheck -p

# Expected output for working GDS:
GDS release version: 2.17.3
nvidia_fs version:   2.17.3
cuFile version:      2.17.3
Platform:            Linux
GDS-Supported:       Yes  ← must be Yes

  ============
  ENVIRONMENT:
  ============
  DriverVersion          = 535.86.10
  IsSupported            = 1  ← 1 = supported
  IsRDMASupported        = 1  ← 1 = RDMA path available
  IsDirectStorageEnabled = 1  ← 1 = GDS kernel mode active
  IsCompatModeEnabled    = 0  ← 0 = NOT falling back to CPU path

# CRITICAL: IsDirectStorageEnabled must be 1
# If 0: check nvidia-fs module, check file system support, check driver version
```

### 用 gds_bandwidth 工具测试

```bash
# Built-in GDS bandwidth benchmark
/usr/local/cuda/gds/tools/gds_bandwidth

# Test specific paths
/usr/local/cuda/gds/tools/gds_bandwidth \
    --file=/mnt/nvme0/test_gds.bin \   # local NVMe path
    --size=1GB \
    --gpu_id=0 \
    --pattern=sequential

# Expected output:
# GDS Read:  6.8 GB/s   (local NVMe, PCIe Gen4 x4)
# GDS Write: 3.2 GB/s
# Compat Read:  2.1 GB/s  (CPU path comparison)
# → GDS is 3.2× faster than CPU path for reads
```

### Python 验证

```python
import cupy as cp
import cufile  # pip install cufile

# Verify GDS is active on GPU 0
dev = cp.cuda.Device(0)
with dev:
    print(f"GDS enabled: {cufile.is_gds_available()}")
    # Expected: GDS enabled: True
```

---

## 5. 排查常见安装问题

### 问题：`lsmod | grep nvidia_fs` 未显示任何内容

```bash
# nvidia-fs not loaded — try manual load
sudo modprobe nvidia_fs

# If modprobe fails: module not built for your kernel
sudo /usr/src/nvidia-*/nvidia-fs/build nvidia-fs

# Or reinstall GDS package
sudo dnf reinstall nvidia-gds-12-2
sudo modprobe nvidia_fs
```

### 问题：`gdscheck` 显示 `IsDirectStorageEnabled = 0`

```bash
# Most common cause: file system is not GDS-compatible
# Check if your NVMe is mounted as ext4 or xfs (NOT tmpfs, NFS, ZFS)
mount | grep nvme
# Expected: /dev/nvme0n1 on /mnt/data type ext4 ...

# ZFS is NOT supported by GDS kernel mode
# Use ext4 or xfs on the NVMe drives used for training data

# Also check: nvidia-fs and CUDA driver version mismatch
cat /proc/driver/nvidia-fs/params  # shows nvidia-fs version
nvidia-smi | grep "Driver Version"  # must match GDS expected driver version
```

### 问题：性能与 CPU 路径相当（GDS 实际未运行）

```bash
# allow_compat_mode=true in cufile.json causes silent fallback
# Disable compat mode to force GDS (will error if GDS unavailable):
# In /etc/cufile.json:
# "allow_compat_mode": false

# Monitor which path is being used:
export CUFILE_LOGFILE=/tmp/cufile.log
export CUFILE_LOG_LEVEL=INFO
# Re-run your workload, check log:
grep "GDS_COMPAT\|DIRECT_RDMA\|CUFILE_MODE" /tmp/cufile.log
# DIRECT_RDMA = GDS active ✓
# GDS_COMPAT = CPU fallback active ✗
```

### 问题：OFED 版本与 GDS 不匹配

```bash
# Check OFED version
ofed_info | head -1
# Must match GDS requirements (5.8-3.0.7.0 for GDS 2.17.3)

# Full compatibility matrix:
# https://docs.nvidia.com/gpudirect-storage/release-notes/index.html
```

---

## 6. 软件版本兼容性矩阵（参考）

| GDS 版本 | CUDA | 驱动 | OFED | Kernel |
|---|---|---|---|---|
| 2.17.3 | 12.2 | 535.86.10 | 5.8-3.0.7.0 | ≥ 5.14 |
| 1.9.1 | 12.0 | 525.x | 5.8-x | ≥ 5.4 |
| 1.7.x | 11.8 | 520.x | 5.6-x | ≥ 5.4 |

升级任何组件之前，务必查阅 [GDS Release Notes](https://docs.nvidia.com/gpudirect-storage/release-notes/index.html)。

---


<details>
<summary>English original</summary>

**3. GDS Configuration File**

The cuFile daemon is configured via `/etc/cufile.json`:

```json
{
  "logging": {
    "level": "WARN"
  },
  "profile": {
    "nvtx": false,
    "cufile_stats": 0
  },
  "execution": {
    "max_direct_io_size_kb": 16384,
    "max_device_cache_size_kb": 131072,
    "max_io_queue_depth": 128,
    "max_batch_io_timeout_msecs": 5,
    "num_threads_per_block": 32,
    "num_io_threads": 4
  },
  "properties": {
    "max_direct_io_size_kb": 16384,
    "use_poll_mode": false,
    "poll_mode_max_size_kb": 4,
    "max_batch_io_size_kb": 65536,
    "allow_compat_mode": true,      ← fallback to CPU path if GDS unavailable
    "gds_rdma_write_support": true
  },
  "fs": {
    "generic": {
      "posix_unaligned_writes": false,
      "posix_writes": false,
      "allow_sb_offload": false
    }
  }
}
```

Key parameters:

| Parameter | Recommended Value | Effect |
|---|---|---|
| `max_direct_io_size_kb` | 16384 (16 MB) | Max single GDS transfer size |
| `max_device_cache_size_kb` | 131072 (128 MB) | GPU-side cuFile cache |
| `max_io_queue_depth` | 128 | Concurrent I/O requests |
| `allow_compat_mode` | true | Silent fallback to CPU path |
| `use_poll_mode` | false | Interrupt vs polling (polling: lower latency, more CPU) |

---

**4. Verifying GDS is Active**

**gdscheck — The Essential Verification Tool**

```bash
# Run GDS compatibility check
/usr/local/cuda/gds/tools/gdscheck -p

# Expected output for working GDS:
GDS release version: 2.17.3
nvidia_fs version:   2.17.3
cuFile version:      2.17.3
Platform:            Linux
GDS-Supported:       Yes  ← must be Yes

  ============
  ENVIRONMENT:
  ============
  DriverVersion          = 535.86.10
  IsSupported            = 1  ← 1 = supported
  IsRDMASupported        = 1  ← 1 = RDMA path available
  IsDirectStorageEnabled = 1  ← 1 = GDS kernel mode active
  IsCompatModeEnabled    = 0  ← 0 = NOT falling back to CPU path

# CRITICAL: IsDirectStorageEnabled must be 1
# If 0: check nvidia-fs module, check file system support, check driver version
```

**Test with gds_bandwidth Tool**

```bash
# Built-in GDS bandwidth benchmark
/usr/local/cuda/gds/tools/gds_bandwidth

# Test specific paths
/usr/local/cuda/gds/tools/gds_bandwidth \
    --file=/mnt/nvme0/test_gds.bin \   # local NVMe path
    --size=1GB \
    --gpu_id=0 \
    --pattern=sequential

# Expected output:
# GDS Read:  6.8 GB/s   (local NVMe, PCIe Gen4 x4)
# GDS Write: 3.2 GB/s
# Compat Read:  2.1 GB/s  (CPU path comparison)
# → GDS is 3.2× faster than CPU path for reads
```

**Python Verification**

```python
import cupy as cp
import cufile  # pip install cufile

# Verify GDS is active on GPU 0
dev = cp.cuda.Device(0)
with dev:
    print(f"GDS enabled: {cufile.is_gds_available()}")
    # Expected: GDS enabled: True
```

---

**5. Troubleshooting Common Installation Issues**

**Issue: `lsmod | grep nvidia_fs` shows nothing**

```bash
# nvidia-fs not loaded — try manual load
sudo modprobe nvidia_fs

# If modprobe fails: module not built for your kernel
sudo /usr/src/nvidia-*/nvidia-fs/build nvidia-fs

# Or reinstall GDS package
sudo dnf reinstall nvidia-gds-12-2
sudo modprobe nvidia_fs
```

**Issue: `gdscheck` shows `IsDirectStorageEnabled = 0`**

```bash
# Most common cause: file system is not GDS-compatible
# Check if your NVMe is mounted as ext4 or xfs (NOT tmpfs, NFS, ZFS)
mount | grep nvme
# Expected: /dev/nvme0n1 on /mnt/data type ext4 ...

# ZFS is NOT supported by GDS kernel mode
# Use ext4 or xfs on the NVMe drives used for training data

# Also check: nvidia-fs and CUDA driver version mismatch
cat /proc/driver/nvidia-fs/params  # shows nvidia-fs version
nvidia-smi | grep "Driver Version"  # must match GDS expected driver version
```

**Issue: Performance matches CPU path (GDS not actually running)**

```bash
# allow_compat_mode=true in cufile.json causes silent fallback
# Disable compat mode to force GDS (will error if GDS unavailable):
# In /etc/cufile.json:
# "allow_compat_mode": false

# Monitor which path is being used:
export CUFILE_LOGFILE=/tmp/cufile.log
export CUFILE_LOG_LEVEL=INFO
# Re-run your workload, check log:
grep "GDS_COMPAT\|DIRECT_RDMA\|CUFILE_MODE" /tmp/cufile.log
# DIRECT_RDMA = GDS active ✓
# GDS_COMPAT = CPU fallback active ✗
```

**Issue: OFED version mismatch with GDS**

```bash
# Check OFED version
ofed_info | head -1
# Must match GDS requirements (5.8-3.0.7.0 for GDS 2.17.3)

# Full compatibility matrix:
# https://docs.nvidia.com/gpudirect-storage/release-notes/index.html
```

---

**6. Software Version Compatibility Matrix (Reference)**

| GDS Version | CUDA | Driver | OFED | Kernel |
|---|---|---|---|---|
| 2.17.3 | 12.2 | 535.86.10 | 5.8-3.0.7.0 | ≥ 5.14 |
| 1.9.1 | 12.0 | 525.x | 5.8-x | ≥ 5.4 |
| 1.7.x | 11.8 | 520.x | 5.6-x | ≥ 5.4 |

Always consult the [GDS Release Notes](https://docs.nvidia.com/gpudirect-storage/release-notes/index.html) before upgrading any component.

---

</details>

## 参考文献

- [GDS 安装指南](https://docs.nvidia.com/gpudirect-storage/overview-guide/index.html#installing-gpudirect-storage)
- [cuFile 守护进程配置](https://docs.nvidia.com/gpudirect-storage/configuration-guide/index.html)
- [MLNX_OFED 安装](https://docs.nvidia.com/networking/display/MLNXOFEDv583070/Introduction)
- [gdscheck 文档](https://docs.nvidia.com/gpudirect-storage/troubleshooting-guide/index.html#gdscheck)


<details>
<summary>English original</summary>

**References**

- [GDS Installation Guide](https://docs.nvidia.com/gpudirect-storage/overview-guide/index.html#installing-gpudirect-storage)
- [cuFile Daemon Configuration](https://docs.nvidia.com/gpudirect-storage/configuration-guide/index.html)
- [MLNX_OFED Installation](https://docs.nvidia.com/networking/display/MLNXOFEDv583070/Introduction)
- [gdscheck Documentation](https://docs.nvidia.com/gpudirect-storage/troubleshooting-guide/index.html#gdscheck)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/GPUDirect-Storage/03-Software-Stack.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/GPUDirect-Storage/03-Software-Stack.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
