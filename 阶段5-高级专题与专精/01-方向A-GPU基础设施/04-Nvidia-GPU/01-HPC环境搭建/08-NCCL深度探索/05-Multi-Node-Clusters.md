---
title: 05 — 多节点 GPU 簇中的 NCCL
description: 05 — 多节点 GPU 簇中的 NCCL
---

# 05 — 多节点 GPU 簇中的 NCCL

## 1. 两级通信问题

单节点 8x H200 以 NVLink 速率（900 GB/s）通信。增加第二个节点会在两者之间引入一条慢得多的链路。

```
Node 0 (8x H200):                    Node 1 (8x H200):
  GPU0 ──┐                              GPU8 ──┐
  GPU1 ──┤                              GPU9 ──┤
  GPU2 ──┤  NVLink 900 GB/s             GPU10──┤  NVLink 900 GB/s
  GPU3 ──┤  (intra-node)                GPU11──┤  (intra-node)
  GPU4 ──┤                              GPU12──┤
  GPU5 ──┤                              GPU13──┤
  GPU6 ──┤                              GPU14──┤
  GPU7 ──┘                              GPU15──┘
          │                                    │
          └────────── InfiniBand HDR ──────────┘
                      200 Gb/s = 25 GB/s
                      (inter-node)

Speed ratio: NVLink / IB = 900 / 25 = 36×

→ Inter-node bandwidth is the critical bottleneck for multi-node training
```

---

## 2. 分层 AllReduce

当 NVLink 与网络混用时，NCCL 自动采用分层 AllReduce：

```
Step 1: Intra-node Reduce-Scatter (NVLink, fast)
  Each node reduces its 8 GPUs' gradients to per-GPU shards

  Node0: [G0, G1, G2, G3, G4, G5, G6, G7] → Node0 GPU0 holds G0_reduced
                                            → Node0 GPU1 holds G1_reduced
                                            → ...

Step 2: Inter-node AllReduce (InfiniBand, slow but minimized)
  Only N/world_size fraction of data crosses IB

  Node0 GPU0 ↔ Node1 GPU0: exchange G0_reduced shards
  Node0 GPU1 ↔ Node1 GPU1: exchange G1_reduced shards (in parallel)
  ...

Step 3: Intra-node AllGather (NVLink, fast)
  Reconstruct full reduced gradient on all GPUs within each node

Total data sent over IB = S × N_nodes / (N_nodes × N_gpus) × 2 = S × 2 / N_gpus
For 2 nodes × 8 GPUs: IB carries only 2/8 = 25% of gradient data
```

正是这种分层结构，使 NCCL 即使在互连混用的情况下也能达到接近最优的带宽。

---

## 3. InfiniBand 配置

### 硬件要求

| Component | Specification | Notes |
|---|---|---|
| HCA | Mellanox ConnectX-7, HDR 200 Gb/s | 建议每 GPU 一个 |
| Switch | Mellanox Quantum-2 (NDR 400 Gb/s) or Quantum (HDR 200 Gb/s) | fat-tree 拓扑 |
| Cables | QSFP112 (NDR) or QSFP56 (HDR) | 长度影响信号完整性 |
| Topology | Fat-tree, Dragonfly, or Torus | fat-tree 最常用 |

### 软件安装

```bash
# Install MLNX_OFED (Mellanox OpenFabrics Enterprise Distribution)
wget https://www.mellanox.com/downloads/ofed/MLNX_OFED-23.10-3.2.2.0/...
./mlnxofedinstall --all --without-fw-update

# Verify installation
ibstat          # shows HCA status and port info
ibv_devinfo     # InfiniBand device details
ib_write_bw     # bandwidth test

# Check IB link speed
ibstat | grep -A2 "Port 1"
# State: Active
# Physical state: LinkUp
# Rate: 200 Gb/s  ← HDR
```

### GPUDirect RDMA — GPU 内存直接走 InfiniBand

不使用 GPUDirect RDMA 时：
```
GPU HBM → CPU RAM (PCIe) → NIC buffer (PCIe) → IB cable → NIC → CPU RAM → GPU HBM
         ← PCIe copy →  ← CPU involvement →  ← PCIe copy →
Latency: 2 PCIe transfers + CPU processing = ~10 µs extra
```

使用 GPUDirect RDMA 时：
```
GPU HBM → IB cable → GPU HBM
          ↑ direct DMA ↑
No CPU involved, no PCIe double-copy
Latency reduction: ~5 µs, bandwidth: full IB speed to GPU memory
```

```bash
# Enable GPUDirect RDMA
export NCCL_NET_GDR_LEVEL=5   # enable for all GPU-IB distances
export NCCL_IB_HCA=mlx5_0     # specify HCA connected closest to GPU (check with nvidia-smi topo)

# Verify GPUDirect is working
nv_peer_mem status   # should show "Module is loaded"
# or
ibv_devinfo -d mlx5_0 | grep dm_size  # non-zero = GPUDirect works

# Test with NCCL tests across nodes
mpirun -np 16 \
    -H node0:8,node1:8 \
    -x NCCL_NET_GDR_LEVEL=5 \
    -x NCCL_IB_HCA=mlx5_0 \
    -x NCCL_DEBUG=INFO \
    ./build/all_reduce_perf -b 1G -e 8G -f 2 -g 1
```

---

## 4. SHARP — 网内计算

SHARP（Scalable Hierarchical Aggregation and Reduction Protocol）把 AllReduce 计算**搬进 InfiniBand 交换机内部**，从而降低 GPU 与网络负载。

```
Without SHARP:
  Node0 GPU → IB switch → Node1 GPU → AllReduce in GPU → back over IB
  Data travels full round-trip

With SHARP:
  Node0 GPU → IB switch → (AllReduce computed inside switch) → each GPU gets result
  Data travels half the distance, reduction done in-switch
```

### SHARP 性能收益

```
AllReduce of 1 GB across 16 nodes (128 GPUs):

Without SHARP: 1 GB × 2(N-1)/N × 1/IB_BW = 1 GB × 1.98 / 25 GB/s = 79 ms
With SHARP:    ~40 ms (50% reduction, especially for small-medium message sizes)
```


<details>
<summary>English original</summary>

**05 — NCCL in Multi-Node GPU Clusters**

**1. The Two-Level Communication Problem**

A single node with 8x H200 communicates at NVLink speeds (900 GB/s). Adding a second node introduces a fundamentally slower link between them.

```
Node 0 (8x H200):                    Node 1 (8x H200):
  GPU0 ──┐                              GPU8 ──┐
  GPU1 ──┤                              GPU9 ──┤
  GPU2 ──┤  NVLink 900 GB/s             GPU10──┤  NVLink 900 GB/s
  GPU3 ──┤  (intra-node)                GPU11──┤  (intra-node)
  GPU4 ──┤                              GPU12──┤
  GPU5 ──┤                              GPU13──┤
  GPU6 ──┤                              GPU14──┤
  GPU7 ──┘                              GPU15──┘
          │                                    │
          └────────── InfiniBand HDR ──────────┘
                      200 Gb/s = 25 GB/s
                      (inter-node)

Speed ratio: NVLink / IB = 900 / 25 = 36×

→ Inter-node bandwidth is the critical bottleneck for multi-node training
```

---

**2. Hierarchical AllReduce**

NCCL automatically uses hierarchical AllReduce when mixing NVLink and network:

```
Step 1: Intra-node Reduce-Scatter (NVLink, fast)
  Each node reduces its 8 GPUs' gradients to per-GPU shards

  Node0: [G0, G1, G2, G3, G4, G5, G6, G7] → Node0 GPU0 holds G0_reduced
                                            → Node0 GPU1 holds G1_reduced
                                            → ...

Step 2: Inter-node AllReduce (InfiniBand, slow but minimized)
  Only N/world_size fraction of data crosses IB

  Node0 GPU0 ↔ Node1 GPU0: exchange G0_reduced shards
  Node0 GPU1 ↔ Node1 GPU1: exchange G1_reduced shards (in parallel)
  ...

Step 3: Intra-node AllGather (NVLink, fast)
  Reconstruct full reduced gradient on all GPUs within each node

Total data sent over IB = S × N_nodes / (N_nodes × N_gpus) × 2 = S × 2 / N_gpus
For 2 nodes × 8 GPUs: IB carries only 2/8 = 25% of gradient data
```

This hierarchy is why NCCL achieves nearly optimal bandwidth even with mixed interconnects.

---

**3. InfiniBand Setup**

**Hardware Requirements**

| Component | Specification | Notes |
|---|---|---|
| HCA | Mellanox ConnectX-7, HDR 200 Gb/s | One per GPU recommended |
| Switch | Mellanox Quantum-2 (NDR 400 Gb/s) or Quantum (HDR 200 Gb/s) | Fat-tree topology |
| Cables | QSFP112 (NDR) or QSFP56 (HDR) | Length matters for signal integrity |
| Topology | Fat-tree, Dragonfly, or Torus | Fat-tree most common |

**Software Installation**

```bash
# Install MLNX_OFED (Mellanox OpenFabrics Enterprise Distribution)
wget https://www.mellanox.com/downloads/ofed/MLNX_OFED-23.10-3.2.2.0/...
./mlnxofedinstall --all --without-fw-update

# Verify installation
ibstat          # shows HCA status and port info
ibv_devinfo     # InfiniBand device details
ib_write_bw     # bandwidth test

# Check IB link speed
ibstat | grep -A2 "Port 1"
# State: Active
# Physical state: LinkUp
# Rate: 200 Gb/s  ← HDR
```

**GPUDirect RDMA — GPU Memory Directly Over InfiniBand**

Without GPUDirect RDMA:
```
GPU HBM → CPU RAM (PCIe) → NIC buffer (PCIe) → IB cable → NIC → CPU RAM → GPU HBM
         ← PCIe copy →  ← CPU involvement →  ← PCIe copy →
Latency: 2 PCIe transfers + CPU processing = ~10 µs extra
```

With GPUDirect RDMA:
```
GPU HBM → IB cable → GPU HBM
          ↑ direct DMA ↑
No CPU involved, no PCIe double-copy
Latency reduction: ~5 µs, bandwidth: full IB speed to GPU memory
```

```bash
# Enable GPUDirect RDMA
export NCCL_NET_GDR_LEVEL=5   # enable for all GPU-IB distances
export NCCL_IB_HCA=mlx5_0     # specify HCA connected closest to GPU (check with nvidia-smi topo)

# Verify GPUDirect is working
nv_peer_mem status   # should show "Module is loaded"
# or
ibv_devinfo -d mlx5_0 | grep dm_size  # non-zero = GPUDirect works

# Test with NCCL tests across nodes
mpirun -np 16 \
    -H node0:8,node1:8 \
    -x NCCL_NET_GDR_LEVEL=5 \
    -x NCCL_IB_HCA=mlx5_0 \
    -x NCCL_DEBUG=INFO \
    ./build/all_reduce_perf -b 1G -e 8G -f 2 -g 1
```

---

**4. SHARP — In-Network Computing**

SHARP (Scalable Hierarchical Aggregation and Reduction Protocol) moves the AllReduce computation **into the InfiniBand switch itself**, reducing GPU and network load.

```
Without SHARP:
  Node0 GPU → IB switch → Node1 GPU → AllReduce in GPU → back over IB
  Data travels full round-trip

With SHARP:
  Node0 GPU → IB switch → (AllReduce computed inside switch) → each GPU gets result
  Data travels half the distance, reduction done in-switch
```

**SHARP Performance Gains**

```
AllReduce of 1 GB across 16 nodes (128 GPUs):

Without SHARP: 1 GB × 2(N-1)/N × 1/IB_BW = 1 GB × 1.98 / 25 GB/s = 79 ms
With SHARP:    ~40 ms (50% reduction, especially for small-medium message sizes)
```

</details>

### SHARP 配置

```bash
# SHARP requires: MLNX_OFED with SHARP support + Mellanox Quantum switch

# Enable SHARP in NCCL
export NCCL_ALGO=CollNetDirect   # use SHARP-based CollNet algorithm
# or
export NCCL_ALGO=CollNetChain

# Verify SHARP is active
export NCCL_DEBUG=INFO
# Look for: "NCCL INFO Using SHARP" in output

# SHARP works best for: 1 KB - 128 MB messages, small-medium clusters
# For very large messages (>1 GB): Ring AllReduce still better due to switch memory limits
```

---

## 5. 多节点启动模式

### torchrun（弹性分布式训练）

```bash
# Node 0 (master):
torchrun \
    --nnodes=4 \                          # 4 nodes total
    --nproc_per_node=8 \                  # 8 GPUs per node
    --node_rank=0 \                       # this node's rank
    --master_addr=192.168.1.100 \         # node 0 IP
    --master_port=29500 \
    train.py

# Nodes 1, 2, 3 (identical, change --node_rank):
torchrun \
    --nnodes=4 \
    --nproc_per_node=8 \
    --node_rank=1 \                       # 1 for node 1, 2 for node 2, etc.
    --master_addr=192.168.1.100 \
    --master_port=29500 \
    train.py
```

### MPI 启动（用于 nccl-tests 与 HPC 工作流）

```bash
# Using OpenMPI
mpirun \
    -np 32 \                             # 32 total processes (4 nodes × 8 GPUs)
    --host node0:8,node1:8,node2:8,node3:8 \
    -x NCCL_DEBUG=INFO \
    -x NCCL_IB_HCA=mlx5_0 \
    -x NCCL_NET_GDR_LEVEL=5 \
    -bind-to none \                      # don't pin to CPU cores (GPU-heavy workload)
    ./build/all_reduce_perf -b 1G -e 8G -f 2 -g 1
```

### Slurm + torchrun（HPC 簇）

```bash
#!/bin/bash
# sbatch script: train_job.sh
#SBATCH --nodes=4
#SBATCH --ntasks-per-node=8
#SBATCH --gres=gpu:8
#SBATCH --time=48:00:00
#SBATCH --partition=gpu-cluster

# Export NCCL settings
export NCCL_IB_HCA=mlx5_0,mlx5_1
export NCCL_NET_GDR_LEVEL=5
export NCCL_IB_TC=106
export NCCL_DEBUG=WARN
export NCCL_TIMEOUT=600

# Get master address from Slurm node list
export MASTER_ADDR=$(scontrol show hostname "$SLURM_NODELIST" | head -1)
export MASTER_PORT=29500

srun torchrun \
    --nnodes=$SLURM_NNODES \
    --nproc_per_node=$SLURM_NTASKS_PER_NODE \
    --master_addr=$MASTER_ADDR \
    --master_port=$MASTER_PORT \
    --node_rank=$SLURM_NODEID \
    train.py

# Submit: sbatch train_job.sh
```

---

## 6. 多节点通信优化

### 最小化节点间流量

```
Principle: keep as much communication as possible on NVLink (intra-node)

Strategy for 3D Parallelism across nodes:
  TP (tensor parallel): keep WITHIN a node (NVLink, frequent small all-reduces)
  PP (pipeline parallel): split ACROSS nodes (IB, infrequent large activation tensors)
  DP (data parallel): split ACROSS nodes (IB, one all-reduce per step, large)

Example: 2 nodes × 8 GPUs = 16 GPUs total
  TP=8 (within each node), PP=2 (one stage per node), DP=1
  → Only pipeline activations cross IB (much smaller than gradient all-reduce)
```

```python
# Megatron-LM 3D parallel setup for 2 nodes
# Node 0: GPU 0-7 = stage 0 of pipeline, fully tensor-parallel
# Node 1: GPU 8-15 = stage 1 of pipeline, fully tensor-parallel

# torchrun launch:
torchrun \
    --nnodes=2 \
    --nproc_per_node=8 \
    pretrain_gpt.py \
    --tensor-model-parallel-size 8 \     # TP=8, stays within node
    --pipeline-model-parallel-size 2 \  # PP=2, crosses nodes
    --data-parallel-size 1              # DP=1
```

### 面向低速节点间链路的梯度压缩

```python
# PowerSGD: low-rank gradient approximation
from torch.distributed.algorithms.ddp_comm_hooks import powerSGD_hook as powerSGD

state = powerSGD.PowerSGDState(
    process_group=dist.get_world_group(),
    matrix_approximation_rank=16,   # lower = more compression, less quality
    start_powerSGD_iter=100,         # start compression after 100 iters (warmup)
    warm_start=True,
)
model.register_comm_hook(state, powerSGD.powerSGD_hook)

# Compression ratio: 8192×8192 matrix (536 MB) → rank-16 approx:
# U: 8192×16 + V: 8192×16 = 4 MB → 134× compression
# Typical loss in quality: < 0.5% on perplexity
```

### 用梯度累积摊薄通信开销

```python
# Accumulate gradients over multiple microbatches before AllReduce
# This reduces communication frequency at the cost of more memory

GRAD_ACCUM_STEPS = 8   # accumulate 8 microbatches before AllReduce

optimizer.zero_grad()
for step, batch in enumerate(dataloader):
    # Only sync every GRAD_ACCUM_STEPS
    with model.no_sync() if (step % GRAD_ACCUM_STEPS != GRAD_ACCUM_STEPS - 1) else nullcontext():
        loss = model(**batch).loss / GRAD_ACCUM_STEPS
        loss.backward()

    if (step + 1) % GRAD_ACCUM_STEPS == 0:
        # AllReduce fires here — effective batch size is 8× larger
        optimizer.step()
        optimizer.zero_grad()
```

---

## 7. 多节点 NCCL benchmark

```bash
# 2-node AllReduce benchmark (16 GPUs)
mpirun -np 16 -H node0:8,node1:8 \
    -x NCCL_NET_GDR_LEVEL=5 \
    -x NCCL_IB_HCA=mlx5_0 \
    ./build/all_reduce_perf -b 8M -e 4G -f 2 -g 1

# Expected results (H200 × 16, IB HDR):
# size     algbw    busbw
# 8 MB     12 GB/s  22 GB/s   ← small: latency bound
# 256 MB   22 GB/s  41 GB/s   ← medium
# 4 GB     24 GB/s  45 GB/s   ← large: approaches IB peak (~25 GB/s unidirectional)

# Test latency for small ops (important for inference)
./build/all_reduce_perf -b 8 -e 4096 -f 2 -g 1
# Target: < 10 µs for 8-byte messages (control messages in inference serving)
```

---

## 参考文献

- [NCCL Hierarchical AllReduce](https://docs.nvidia.com/deeplearning/nccl/user-guide/docs/usage/p2p.html)
- [SHARP User Manual](https://docs.mellanox.com/display/sharpmanual)
- [GPUDirect RDMA Documentation](https://docs.nvidia.com/cuda/gpudirect-rdma/)
- [PyTorch Elastic Training](https://pytorch.org/docs/stable/elastic/run.html)
- [Slurm GPU Scheduling](https://slurm.schedmd.com/gres.html)
- [InfiniBand Trade Association](https://www.infinibandta.org/)


<details>
<summary>English original</summary>

**7. Benchmarking Multi-Node NCCL**

```bash
# 2-node AllReduce benchmark (16 GPUs)
mpirun -np 16 -H node0:8,node1:8 \
    -x NCCL_NET_GDR_LEVEL=5 \
    -x NCCL_IB_HCA=mlx5_0 \
    ./build/all_reduce_perf -b 8M -e 4G -f 2 -g 1

# Expected results (H200 × 16, IB HDR):
# size     algbw    busbw
# 8 MB     12 GB/s  22 GB/s   ← small: latency bound
# 256 MB   22 GB/s  41 GB/s   ← medium
# 4 GB     24 GB/s  45 GB/s   ← large: approaches IB peak (~25 GB/s unidirectional)

# Test latency for small ops (important for inference)
./build/all_reduce_perf -b 8 -e 4096 -f 2 -g 1
# Target: < 10 µs for 8-byte messages (control messages in inference serving)
```

---

**References**

- [NCCL Hierarchical AllReduce](https://docs.nvidia.com/deeplearning/nccl/user-guide/docs/usage/p2p.html)
- [SHARP User Manual](https://docs.mellanox.com/display/sharpmanual)
- [GPUDirect RDMA Documentation](https://docs.nvidia.com/cuda/gpudirect-rdma/)
- [PyTorch Elastic Training](https://pytorch.org/docs/stable/elastic/run.html)
- [Slurm GPU Scheduling](https://slurm.schedmd.com/gres.html)
- [InfiniBand Trade Association](https://www.infinibandta.org/)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/NCCL-Deep-Dive/05-Multi-Node-Clusters.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/NCCL-Deep-Dive/05-Multi-Node-Clusters.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
