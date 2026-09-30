---
title: 06 — 解耦存储：NVMe-oF + OpenFlex + RapidFlex
description: 06 — 解耦存储：NVMe-oF + OpenFlex + RapidFlex
---

# 06 — 解耦存储：NVMe-oF + OpenFlex + RapidFlex

## 1. 为什么 AI 需要解耦存储

传统 HPC 存储模型：**存储位于计算节点本地**。

```
Compute Node A:    GPU + 8 × NVMe local drives
Compute Node B:    GPU + 8 × NVMe local drives
Compute Node C:    GPU + 8 × NVMe local drives

Problems:
  - Dataset must be replicated to every node (wastes capacity)
  - When a node fails, its local data is unavailable
  - Storage is idle when the node is running a different job
  - Adding more compute means buying more storage too (coupled scaling)
```

解耦存储模型：

```
Compute Node A:    GPU + NIC (no local NVMe)
Compute Node B:    GPU + NIC (no local NVMe)
Compute Node C:    GPU + NIC (no local NVMe)

Storage Server:    OpenFlex Data24 × N (NVMe pool, shared)
Network:           RoCEv2, 200 Gb/s (NVIDIA SN3700)

Benefits:
  + Dataset stored once, accessed by any compute node
  + Storage fails independently from compute (no coupled failure)
  + Add more storage without adding compute (and vice versa)
  + Storage utilization: 90%+ (shared pool vs 40% with local)
```

---

## 2. NVMe-oF 协议栈

NVMe over Fabrics（NVMe-oF）把 NVMe 协议扩展到网络 fabric 之上：

```
Standard NVMe (local):          NVMe-oF (network):

Application                     Application
    │                               │
    │ read(fd, buf, size)           │ read(fd, buf, size)   ← IDENTICAL API
    ▼                               ▼
VFS / Block Layer               VFS / Block Layer
    │                               │
NVMe Driver                     NVMe-oF Host Driver
    │                               │
PCIe Bus                        RDMA transport (RoCEv2 / iWARP / InfiniBand)
    │                               │
NVMe SSD                        Network Switch
                                    │
                                NVMe-oF Target (OpenFlex)
                                    │
                                NVMe SSDs
```

GDS 驱动看到 NVMe-oF 命名空间与本地 NVMe 完全相同——同一套 GDS 代码对两者都适用。

---

## 3. RapidFlex 适配器：让远程看起来像本地

WD RapidFlex AIC（Add-In Card，附加卡）安装在**计算服务器**中，负责处理 NVMe-oF 协议：

```
Inside the compute server:

PCIe Bus
├── GPU (x16)
├── NIC / ConnectX-7 (x16)
└── RapidFlex AIC (x8)
    │  Presents remote storage as local NVMe namespaces
    │  /dev/nvme0n1  →  actually OpenFlex drive 0 (over RDMA)
    │  /dev/nvme1n1  →  actually OpenFlex drive 1 (over RDMA)
    └── GDS sees these as standard NVMe devices
```

从 GDS 的视角看：**远程 NVMe-oF 存储与本地 NVMe 完全一致**。

这意味着：
- 使用解耦存储时应用代码零改动
- 同一个 `cuFileRead()` 调用对本地与远程 NVMe 均适用
- 驱动透明地处理 NVMe-oF 传输

---

## 4. OpenFlex Data24 3200：参考存储系统

```
WD OpenFlex Data24 3200 Series specifications:

Internal:
  24 × U.2 NVMe SSD slots (PCIe Gen3)
  Internal bandwidth: limited by PCIe Gen3 backplane

Frontend (network-facing):
  6 × 100 Gb/s Ethernet ports (via RapidFlex AIC frontend cards)
  Each port: 12.5 GB/s
  Total aggregate: 75 GB/s  (6 × 12.5 GB/s)
  Protocol: NVMe-oF over RoCEv2

Capacity:
  Scales with NVMe drive selection (e.g., 24 × 15.36 TB = 368 TB raw)
  After redundancy (RAID-like protection): ~300 TB usable
```

### 75 GB/s 的上限

OpenFlex 的 **6 × 100 Gb/s** 前端是关键约束：

```
4 compute nodes × 4 GPUs each = 16 GPUs total
OpenFlex: 75 GB/s total

Per-GPU bandwidth from OpenFlex: 75 / 16 = 4.7 GB/s
  ← sufficient for most training workloads (dataset read is not usually the bottleneck)

For GPU-intensive workloads where I/O IS the bottleneck:
  → Add more OpenFlex units (linear scaling: 2× units = 2× bandwidth)
  → 3 × OpenFlex = 225 GB/s = 14 GB/s per GPU (approaches NIC limits)
```

---

## 5. 网络配置：无损 RoCEv2

RoCEv2（RDMA over Converged Ethernet v2）要求网络**无损**——任何丢包都会触发 RDMA 重传，从而严重降低性能。

### NVIDIA SN3700 交换机配置

```bash
# On the Spectrum-2 switch, configure lossless settings:

# 1. Enable Priority Flow Control (PFC) for RDMA priority
# PFC pauses specific traffic classes instead of dropping packets

# 2. Configure ECN (Explicit Congestion Notification)
# Early warning to senders before buffers overflow

# 3. Set DSCP markings for RDMA traffic
# Traffic class 3 (DSCP 26) for RDMA on most deployments

# Using NVOS CLI on SN3700:
interface ethernet 1/1 traffic-class 3 pfc
interface ethernet 1/1 congestion-control ecn minimum-absolute 150 maximum-absolute 1500
```


<details>
<summary>English original</summary>

**06 — Disaggregated Storage: NVMe-oF + OpenFlex + RapidFlex**

**1. Why Disaggregated Storage for AI**

Traditional HPC storage model: **storage is local to the compute node**.

```
Compute Node A:    GPU + 8 × NVMe local drives
Compute Node B:    GPU + 8 × NVMe local drives
Compute Node C:    GPU + 8 × NVMe local drives

Problems:
  - Dataset must be replicated to every node (wastes capacity)
  - When a node fails, its local data is unavailable
  - Storage is idle when the node is running a different job
  - Adding more compute means buying more storage too (coupled scaling)
```

Disaggregated storage model:

```
Compute Node A:    GPU + NIC (no local NVMe)
Compute Node B:    GPU + NIC (no local NVMe)
Compute Node C:    GPU + NIC (no local NVMe)

Storage Server:    OpenFlex Data24 × N (NVMe pool, shared)
Network:           RoCEv2, 200 Gb/s (NVIDIA SN3700)

Benefits:
  + Dataset stored once, accessed by any compute node
  + Storage fails independently from compute (no coupled failure)
  + Add more storage without adding compute (and vice versa)
  + Storage utilization: 90%+ (shared pool vs 40% with local)
```

---

**2. NVMe-oF Protocol Stack**

NVMe over Fabrics (NVMe-oF) extends the NVMe protocol over a network fabric:

```
Standard NVMe (local):          NVMe-oF (network):

Application                     Application
    │                               │
    │ read(fd, buf, size)           │ read(fd, buf, size)   ← IDENTICAL API
    ▼                               ▼
VFS / Block Layer               VFS / Block Layer
    │                               │
NVMe Driver                     NVMe-oF Host Driver
    │                               │
PCIe Bus                        RDMA transport (RoCEv2 / iWARP / InfiniBand)
    │                               │
NVMe SSD                        Network Switch
                                    │
                                NVMe-oF Target (OpenFlex)
                                    │
                                NVMe SSDs
```

The GDS driver sees NVMe-oF namespaces as identical to local NVMe — same GDS code works for both.

---

**3. RapidFlex Adapters: Making Remote Look Local**

The WD RapidFlex AIC (Add-In Card) is installed in the **compute server** and handles the NVMe-oF protocol:

```
Inside the compute server:

PCIe Bus
├── GPU (x16)
├── NIC / ConnectX-7 (x16)
└── RapidFlex AIC (x8)
    │  Presents remote storage as local NVMe namespaces
    │  /dev/nvme0n1  →  actually OpenFlex drive 0 (over RDMA)
    │  /dev/nvme1n1  →  actually OpenFlex drive 1 (over RDMA)
    └── GDS sees these as standard NVMe devices
```

From GDS's perspective: **remote NVMe-oF storage looks exactly like local NVMe**.

This means:
- Zero application code changes to use disaggregated storage
- Same `cuFileRead()` call works for local and remote NVMe
- Driver handles the NVMe-oF transport transparently

---

**4. OpenFlex Data24 3200: Reference Storage System**

```
WD OpenFlex Data24 3200 Series specifications:

Internal:
  24 × U.2 NVMe SSD slots (PCIe Gen3)
  Internal bandwidth: limited by PCIe Gen3 backplane

Frontend (network-facing):
  6 × 100 Gb/s Ethernet ports (via RapidFlex AIC frontend cards)
  Each port: 12.5 GB/s
  Total aggregate: 75 GB/s  (6 × 12.5 GB/s)
  Protocol: NVMe-oF over RoCEv2

Capacity:
  Scales with NVMe drive selection (e.g., 24 × 15.36 TB = 368 TB raw)
  After redundancy (RAID-like protection): ~300 TB usable
```

**The 75 GB/s Ceiling**

The OpenFlex's **6 × 100 Gb/s** frontend is the key constraint:

```
4 compute nodes × 4 GPUs each = 16 GPUs total
OpenFlex: 75 GB/s total

Per-GPU bandwidth from OpenFlex: 75 / 16 = 4.7 GB/s
  ← sufficient for most training workloads (dataset read is not usually the bottleneck)

For GPU-intensive workloads where I/O IS the bottleneck:
  → Add more OpenFlex units (linear scaling: 2× units = 2× bandwidth)
  → 3 × OpenFlex = 225 GB/s = 14 GB/s per GPU (approaches NIC limits)
```

---

**5. Network Configuration: Lossless RoCEv2**

RoCEv2 (RDMA over Converged Ethernet v2) requires a **lossless** network — any dropped packet causes RDMA retransmission which degrades performance severely.

**NVIDIA SN3700 Switch Configuration**

```bash
# On the Spectrum-2 switch, configure lossless settings:

# 1. Enable Priority Flow Control (PFC) for RDMA priority
# PFC pauses specific traffic classes instead of dropping packets

# 2. Configure ECN (Explicit Congestion Notification)
# Early warning to senders before buffers overflow

# 3. Set DSCP markings for RDMA traffic
# Traffic class 3 (DSCP 26) for RDMA on most deployments

# Using NVOS CLI on SN3700:
interface ethernet 1/1 traffic-class 3 pfc
interface ethernet 1/1 congestion-control ecn minimum-absolute 150 maximum-absolute 1500
```

</details>

### 服务器端 RoCEv2 配置

```bash
# On each compute server (ConnectX-7):

# Step 1: Set trust mode to DSCP (match switch DSCP markings)
mlnx_qos -i ens1f0 --trust=dscp

# Step 2: Enable PFC for priority 3 (RDMA)
mlnx_qos -i ens1f0 -p 0,0,0,1,0,0,0,0   # enable PFC on priority 3

# Step 3: Set DSCP→priority mapping
mlnx_qos -i ens1f0 --dscp2prio=26,3       # DSCP 26 → priority 3

# Step 4: Verify RoCEv2 mode
cat /sys/class/infiniband/mlx5_0/ports/1/gid_attrs/types/3
# Expected: RoCE v2

# Step 5: Set RoCEv2 ECN (on the NIC)
cma_roce_mode -d mlx5_0 -p 1 -m 2        # mode 2 = RoCEv2

# Test RDMA connectivity
ib_write_bw -d mlx5_0 --report_gbits storage-server-ip &   # server
ib_write_bw -d mlx5_0 --report_gbits storage-server-ip     # client
# Expected: ~190 Gb/s (95% of 200 Gb/s ConnectX-7)
```

---

## 6. 通过 nvme-cli 连接 OpenFlex

```bash
# Step 1: Discover NVMe-oF targets on the OpenFlex
nvme discover \
    --transport rdma \
    --traddr 192.168.100.10 \       # OpenFlex frontend IP
    --trsvcid 4420                  # standard NVMe-oF RDMA port

# Expected output:
# Discovery Log Number of Records 8, Generation counter 1
# =====Discovery Log Entry 0======
# trtype: rdma
# adrfam: ipv4
# subtype: nvme subsystem
# treq: not specified
# portid: 0
# trsvcid: 4420
# subnqn: nqn.2018-01.org.nvmexpress:wd:data24:ns0
# traddr: 192.168.100.10

# Step 2: Connect to all discovered subsystems
nvme connect-all \
    --transport rdma \
    --traddr 192.168.100.10 \
    --trsvcid 4420

# Step 3: Verify connected namespaces
nvme list
# /dev/nvme4n1   WD OpenFlex Data24  NVMeoF   500GB
# /dev/nvme5n1   WD OpenFlex Data24  NVMeoF   500GB
# ...

# Step 4: Verify GDS sees the NVMe-oF devices
/usr/local/cuda/gds/tools/gdscheck -p
# Should show NVMe-oF devices in the GDS-compatible device list

# Step 5: Mount and prepare filesystem
mkfs.ext4 /dev/nvme4n1
mkdir -p /mnt/openflex0
mount -o noatime /dev/nvme4n1 /mnt/openflex0

# Step 6: Test GDS bandwidth over NVMe-oF
/usr/local/cuda/gds/tools/gds_bandwidth \
    --file=/mnt/openflex0/test.bin \
    --size=16384M \                    # 16 GB test file
    --gpu_id=0 \
    --pattern=sequential
# Expected: 20–25 GB/s (ConnectX-7 200 Gb/s NIC limited)
```

---

## 7. 线性横向扩展

解耦存储的关键优势：**性能与容量线性扩展**。

```
1 × OpenFlex Data24:
  Frontend: 75 GB/s
  Capacity: ~300 TB usable
  Serves: up to 12 GPUs at 6 GB/s each

2 × OpenFlex Data24:
  Frontend: 150 GB/s (2 × 75)
  Capacity: ~600 TB usable
  Serves: up to 24 GPUs at 6 GB/s each

N × OpenFlex Data24:
  Frontend: N × 75 GB/s
  Capacity: N × 300 TB
  No central bottleneck — each unit has independent NIC ports

Compare with SAN/NAS:
  Traditional NAS: central controller = single point of failure + bottleneck
  OpenFlex: no central controller, each unit is independent
```

### 存储与计算配比规划

```python
def plan_gds_storage(
    num_gpus: int,
    per_gpu_io_bw_gbps: float,   # GB/s needed per GPU during training
    dataset_tb: float,            # total dataset size in TB
    replication_factor: float = 1.5  # some redundancy
):
    total_io_bw = num_gpus * per_gpu_io_bw_gbps
    openflex_per_unit_bw = 75   # GB/s
    openflex_per_unit_cap = 300  # TB usable

    units_for_bw = total_io_bw / openflex_per_unit_bw
    units_for_cap = (dataset_tb * replication_factor) / openflex_per_unit_cap
    units_needed = max(units_for_bw, units_for_cap)

    print(f"GPUs: {num_gpus}")
    print(f"Required I/O bandwidth: {total_io_bw:.0f} GB/s")
    print(f"Units for bandwidth: {units_for_bw:.1f}")
    print(f"Units for capacity ({dataset_tb} TB): {units_for_cap:.1f}")
    print(f"OpenFlex units needed: {int(units_needed) + 1}")

# Example: 32 GPUs, each needs 10 GB/s, 500 TB dataset
plan_gds_storage(num_gpus=32, per_gpu_io_bw_gbps=10, dataset_tb=500)
# GPUs: 32
# Required I/O bandwidth: 320 GB/s → 5 units for bandwidth
# Units for capacity (500 TB): 2.5 → 3 units for capacity
# OpenFlex units needed: 5 (bandwidth-limited)
```

---

## 8. End-to-End GDS + OpenFlex 架构

```
Complete reference architecture (WD technical brief):

┌─────────────────────────────────────────────────────────────────┐
│                      Compute Cluster                            │
│                                                                 │
│  Node 0                           Node 1                        │
│  ┌──────────────────────┐         ┌──────────────────────┐      │
│  │ 4 × A100 GPU         │         │ 4 × A100 GPU         │      │
│  │ 6 × ConnectX-7       │         │ 6 × ConnectX-7       │      │
│  │   (3 per NUMA node)  │         │   (3 per NUMA node)  │      │
│  │ Local NVMe (8-bay)   │         │ Local NVMe (8-bay)   │      │
│  │   → hot tier cache   │         │   → hot tier cache   │      │
│  └──────────┬───────────┘         └──────────┬───────────┘      │
└─────────────┼───────────────────────────────┼──────────────────┘
              │  200 Gb/s RoCEv2               │
              │  (24 × DAC cables)             │
              ▼                                ▼
┌─────────────────────────────────────────────────────────────────┐
│           NVIDIA SN3700 Spectrum-2 Switch                       │
│           32 × 200 Gb/s, lossless RoCEv2                        │
└─────────────────────────────────────────────────────────────────┘
              │
              ▼
┌─────────────────────────────────────────────────────────────────┐
│           WD OpenFlex Data24 3200                               │
│                                                                 │
│  ┌─────────────────────────────────────────────┐               │
│  │ RapidFlex AIC (6 × 100 Gb/s Ethernet)       │               │
│  │ → Exposes NVMe namespaces via NVMe-oF/RoCEv2 │               │
│  └─────────────────────────────────────────────┘               │
│                                                                 │
│  24 × U.2 NVMe SSD (PCIe Gen3 internal)                         │
│  Max throughput: 75 GB/s total                                  │
└─────────────────────────────────────────────────────────────────┘

Data flow (GDS active):
  Training data on OpenFlex → RoCEv2 → ConnectX-7 → GPU HBM
  No CPU involvement at any stage
  GPU sees OpenFlex drives as local /dev/nvmeXnY via NVMe-oF
```

---

## 参考文献

- [WD OpenFlex Data24 3200 Datasheet](https://www.westerndigital.com/products/data-center-platforms/openflex-data24)
- [WD RapidFlex AIC Technical Brief](https://www.westerndigital.com/solutions/data-center/openflex)
- [NVMe-oF Specification (NVM Express)](https://nvmexpress.org/developers/nvme-of-specification/)
- [RoCEv2 Deployment Guide](https://community.mellanox.com/s/article/recommended-network-configuration-examples-for-roce-deployment)
- [NVIDIA GDS + NVMe-oF Design Guide](https://docs.nvidia.com/gpudirect-storage/design-guide/index.html#nvme-over-fabrics)
- [RAPIDS kvikIO (Python GDS)](https://github.com/rapidsai/kvikio)

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/GPUDirect-Storage/06-Disaggregated-Storage.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/GPUDirect-Storage/06-Disaggregated-Storage.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
