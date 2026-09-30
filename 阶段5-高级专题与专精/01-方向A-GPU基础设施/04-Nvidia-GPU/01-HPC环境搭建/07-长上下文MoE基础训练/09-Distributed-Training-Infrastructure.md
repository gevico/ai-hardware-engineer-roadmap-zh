---
title: 模块 09 — 分布式训练基础设施
description: 模块 09 — 分布式训练基础设施
---

# 模块 09 — 分布式训练基础设施

**Parent:** [Long-Context MoE Foundation Training](README.md)

**一句话目的：** 搭建长上下文 MoE 训练所依赖的多节点 H200/B200 基础设施——互连、调度器、NCCL 调优、检查点与容错——并用一次真实的多节点训练把它验证一遍。

**前置要求：** 模块 02（长上下文 attention）、05（MoE 系统）、08（mesh 设计）。HPC Setup [NCCL Deep Dive](../08-NCCL深度探索/README.md)、[8×H200 Training/Inference](../01-8x-H200训练推理/README.md) 与 [GPUDirect Storage](../05-GPUDirect存储/README.md) 模块。

**产物：** 一次能跑起来的多节点 Megatron 训练启动，一轮检查点 + 续跑，以及一次故障注入测试：杀掉一个节点并证明该次运行能从最近的检查点恢复。

---

## 为什么重要

长上下文 MoE 训练是一次长时间运行、高吞吐的作业，会触及集群的每个部分：GPU、NVLink、InfiniBand、NVMe、网络文件系统、调度器。其中任何一环失效都会浪费数小时的 GPU 时间。本模块要做的，就是阻止一个 100K-USD 的训练作业变成一次 100K-USD 的宕机。

---

## 心智模型

### 物理栈

```
Host bare-metal: BIOS, IOMMU, NUMA topology
   └─ OS + drivers: kernel, NVIDIA driver, MOFED (InfiniBand)
       └─ Runtime: CUDA, NCCL, MPI
           └─ Containers: NGC base image, your training image
               └─ Scheduler: Slurm or Kubernetes + GPU operator
                   └─ Training framework: Megatron-LM / NeMo / DeepSpeed
                       └─ Your code + config
```

每一层都可能独立失效。如果你并不拥有「your code」之下的那些层，至少要知道它们失效时该找谁。

### 互连层级

| 层级 | 硬件 | 有效带宽 | 延迟 | 使用者 |
|------|----------|--------------|---------|---------|
| GPU-GPU 节点内 | NVLink 4 / NVSwitch（Hopper） | 每对 GPU 约 900 GB/s，每个 8-GPU 节点合计约 3.6 TB/s | ~µs | TP、SP、EP、节点内 CP |
| GPU-GPU 跨节点 | InfiniBand HDR/NDR（NDR = 400 Gbps） | 每端口约 50 GB/s，每节点 4× NDR 端口时约 200 GB/s | ~5 µs | PP、DP、跨节点 CP/EP（避免） |
| GPU-存储 | 基于 IB + NVMe 的 GPUDirect Storage | 每节点持续约 30 GB/s | ms | 数据加载、检查点 I/O |
| Host-Host | 以太网 | 与训练无关 | - | 管理、日志转发 |

NVLink 与 InfiniBand 之间约 20× 的差距，决定了你的 mesh 布局（模块 08）。NVLink 与 GPUDirect Storage 之间约 30× 的差距，使检查点 I/O 成为一个前台问题。

### 调度器

对大多数 HPC（高性能计算）集群：Slurm。对大多数 Kubernetes 原生集群：KubeRay 或 Volcano 加 NVIDIA GPU operator。

训练作业需要：

- **感知 rank 的布局**：每个节点必须启动相同数量的进程（`--nproc-per-node 8` 是通用的）；rank 0 是启动器；`MASTER_ADDR` / `MASTER_PORT` 会透传。
- **NCCL 环境**：`NCCL_SOCKET_IFNAME`、`NCCL_IB_HCA`、`NCCL_P2P_LEVEL=NVL`、`NCCL_NVLS_ENABLE=1`、`NCCL_DEBUG=WARN`（仅在调试时设为 `INFO`）。
- **拓扑提示**：非默认 GPU 子集用 `CUDA_VISIBLE_DEVICES`；NVSwitch 机箱内手工调优的拓扑用 `NCCL_TOPO_FILE`。
- **韧性**：torchrun 用 `--max-restarts`；与调度器重启钩子集成的检查点-续跑。

值得常备的 Slurm 启动器模板：

```bash
#!/bin/bash
#SBATCH --nodes=8 --ntasks-per-node=1 --gres=gpu:8
#SBATCH --time=24:00:00 --signal=USR1@180

export MASTER_ADDR=$(scontrol show hostnames "$SLURM_JOB_NODELIST" | head -n 1)
export MASTER_PORT=29500
export NCCL_SOCKET_IFNAME=ib0
export NCCL_IB_HCA=mlx5_0,mlx5_1,mlx5_2,mlx5_3
export NCCL_P2P_LEVEL=NVL
export NCCL_NVLS_ENABLE=1
export NCCL_DEBUG=WARN

srun --container-image=$IMAGE --container-mounts=$PWD:/workspace \
     torchrun --nnodes=$SLURM_NNODES --nproc-per-node=8 \
              --rdzv-id=$SLURM_JOB_ID --rdzv-backend=c10d \
              --rdzv-endpoint=$MASTER_ADDR:$MASTER_PORT \
              pretrain_gpt.py \
              [training args]
```

### 真正重要的 NCCL 调优

- `NCCL_P2P_LEVEL=NVL` — 在可用之处强制仅走 NVLink 的 peer 访问。
- `NCCL_NVLS_ENABLE=1` — 在 NVSwitch 机箱（H100/H200 SXM）上启用 NVLink Sharp，以加速归约。
- `NCCL_IB_HCA` — 指定使用哪些 HCA；名字写错会回落到 TCP，性能会被毁掉。
- `NCCL_IB_GID_INDEX` — 仅在 RoCE 上相关；选对 RoCE v2 GID。
- `NCCL_BUFFSIZE=8388608` — 每通道 8 MB 缓冲；对很长的消息有用，但要小心内存。
- `NCCL_ALGO=Tree,Ring` — 让 NCCL 在 tree（受延迟约束）与 ring（受带宽约束）之间选择。

当 `NCCL_DEBUG=INFO` 时，NCCL 会在初始化时打印它选中的算法。用新布局首次运行时，抓下那份日志，确认它选的东西是合理的。


<details>
<summary>English original</summary>

**Module 09 — Distributed Training Infrastructure**

**Parent:** [Long-Context MoE Foundation Training](README.md)

**One-line purpose:** Stand up the multi-node H200/B200 infrastructure that long-context MoE training depends on — interconnects, schedulers, NCCL tuning, checkpointing, and fault tolerance — and verify it with a real multi-node training run.

**Prerequisites:** Modules 02 (long-context attention), 05 (MoE systems), 08 (mesh design). The HPC Setup [NCCL Deep Dive](../08-NCCL深度探索/README.md), [8×H200 Training/Inference](../01-8x-H200训练推理/README.md), and [GPUDirect Storage](../05-GPUDirect存储/README.md) modules.

**Artifact:** A working multi-node Megatron training launch, a checkpoint + resume cycle, and a fault-injection test that kills one node and proves the run recovers from the last checkpoint.

---

**Why it matters**

A long-context MoE training run is a long-running, high-throughput job that touches every part of the cluster: GPU, NVLink, InfiniBand, NVMe, network filesystem, scheduler. Any one of them failing wastes hours of GPU time. This module is what stops a 100K-USD training job from becoming a 100K-USD outage.

---

**Mental model**

**The physical stack**

```
Host bare-metal: BIOS, IOMMU, NUMA topology
   └─ OS + drivers: kernel, NVIDIA driver, MOFED (InfiniBand)
       └─ Runtime: CUDA, NCCL, MPI
           └─ Containers: NGC base image, your training image
               └─ Scheduler: Slurm or Kubernetes + GPU operator
                   └─ Training framework: Megatron-LM / NeMo / DeepSpeed
                       └─ Your code + config
```

Each layer can break independently. If you do not own the layers below "your code," you must at least know whom to call when they break.

**Interconnect tiers**

| Tier | Hardware | Effective BW | Latency | Used by |
|------|----------|--------------|---------|---------|
| GPU-GPU intra-node | NVLink 4 / NVSwitch (Hopper) | ~900 GB/s per GPU pair, ~3.6 TB/s aggregate per 8-GPU node | ~µs | TP, SP, EP, intra-node CP |
| GPU-GPU inter-node | InfiniBand HDR/NDR (NDR = 400 Gbps) | ~50 GB/s per port, ~200 GB/s with 4× NDR ports per node | ~5 µs | PP, DP, inter-node CP/EP (avoid) |
| GPU-Storage | GPUDirect Storage over IB + NVMe | ~30 GB/s per node sustained | ms | data loading, checkpoint I/O |
| Host-Host | Ethernet | irrelevant for training | - | management, log shipping |

The factor of ~20× between NVLink and InfiniBand is what dictates your mesh layout (Module 08). The factor of ~30× between NVLink and GPUDirect Storage is what makes checkpoint I/O a foreground concern.

**Scheduler**

For most HPC clusters: Slurm. For most Kubernetes-native clusters: KubeRay or Volcano with the NVIDIA GPU operator.

The training job needs:

- **Rank-aware placement**: every node must launch the same number of processes (`--nproc-per-node 8` is universal); rank 0 is the launcher; `MASTER_ADDR` / `MASTER_PORT` are passed through.
- **NCCL environment**: `NCCL_SOCKET_IFNAME`, `NCCL_IB_HCA`, `NCCL_P2P_LEVEL=NVL`, `NCCL_NVLS_ENABLE=1`, `NCCL_DEBUG=WARN` (set to `INFO` only when debugging).
- **Topology hints**: `CUDA_VISIBLE_DEVICES` for non-default GPU subsets; `NCCL_TOPO_FILE` for hand-tuned topology in NVSwitch boxes.
- **Resilience**: `--max-restarts` for torchrun; checkpoint-and-resume integration with the scheduler's restart hook.

The Slurm launcher template you want to keep around:

```bash
#!/bin/bash
#SBATCH --nodes=8 --ntasks-per-node=1 --gres=gpu:8
#SBATCH --time=24:00:00 --signal=USR1@180

export MASTER_ADDR=$(scontrol show hostnames "$SLURM_JOB_NODELIST" | head -n 1)
export MASTER_PORT=29500
export NCCL_SOCKET_IFNAME=ib0
export NCCL_IB_HCA=mlx5_0,mlx5_1,mlx5_2,mlx5_3
export NCCL_P2P_LEVEL=NVL
export NCCL_NVLS_ENABLE=1
export NCCL_DEBUG=WARN

srun --container-image=$IMAGE --container-mounts=$PWD:/workspace \
     torchrun --nnodes=$SLURM_NNODES --nproc-per-node=8 \
              --rdzv-id=$SLURM_JOB_ID --rdzv-backend=c10d \
              --rdzv-endpoint=$MASTER_ADDR:$MASTER_PORT \
              pretrain_gpt.py \
              [training args]
```

**NCCL tuning that actually matters**

- `NCCL_P2P_LEVEL=NVL` — force NVLink-only peer access where available.
- `NCCL_NVLS_ENABLE=1` — enables NVLink Sharp for accelerated reductions on NVSwitch boxes (H100/H200 SXM).
- `NCCL_IB_HCA` — specify which HCAs to use; misnaming this falls back to TCP, which destroys performance.
- `NCCL_IB_GID_INDEX` — only relevant on RoCE; pick the right RoCE v2 GID.
- `NCCL_BUFFSIZE=8388608` — 8 MB per-channel buffer; useful for very long messages, careful with memory.
- `NCCL_ALGO=Tree,Ring` — let NCCL choose between tree (latency-bound) and ring (bandwidth-bound).

NCCL prints its chosen algorithms on init when `NCCL_DEBUG=INFO`. The first run with a new layout, capture that log and confirm it picked something sensible.

</details>

### 检查点

长上下文 MoE 训练检查点体量巨大：一个 70B 参数模型的优化器状态 + fp32 参数约 840 GB。分片检查点（每个 DP × PP × EP × TP rank 一个分片）是唯一可行的方案。

检查点层：

- **Megatron 的分布式检查点**：每个 rank 写自己的分片；元数据文件映射整个 mesh。
- **Torch DCP**（distributed checkpoint）：较新，能处理保存与加载之间的 mesh 变化。
- **异步检查点**（Megatron `--async-save`）：后台写检查点，训练继续；防止检查点引起的单步耗时尖峰。

I/O 目标：一个 64×H200 的 cluster 应能在 60 秒内端到端完成 70B 模型的检查点。若耗时更长，瓶颈在存储层级 —— 参见 GPUDirect Storage 模块。

### 容错

在真实运行中，硬件*必然*会故障：GPU ECC 错误、交换机抖动、节点因 kernel panic 掉线。作业必须：

1. 检测故障（NCCL 挂起检测、watchdog）。
2. 干净地拆除（避免留下僵尸进程）。
3. 从最近的检查点重新拉起，可能落在不同的物理节点上。
4. 记录事故。

Torchrun 的 `--max-restarts` 负责重新拉起；`c10d` rendezvous 负责重新组队。训练脚本必须足够确定性的，使得从 step N 恢复能复现 step N+1 的行为（或者足够接近，使 loss 曲线仍然合理）。

一个实用模式：在 `USR1` 上挂一个 Slurm 信号处理函数，触发优雅检查点，然后由调度器重新提交作业。作业会自动从最新检查点继续。

### 监控

最低限度的看板：

- **单步耗时** + 方差。
- **每 rank 的 loss**（必须保持接近；rank 之间发散说明是 routing 或数值 bug）。
- **按 collective 统计的 NCCL 通信时间**。
- **每 rank 的 GPU 利用率、显存、温度、功耗**。
- **网络**：每 NIC 吞吐、重传、链路错误。
- **存储**：检查点窗口期的写吞吐。

标准栈：NVIDIA DCGM exporter → Prometheus → Grafana。深入排查时用 PyTorch profiler dump。

---

## 动手搭建

### 1. 多节点基线

使用上面的 Slurm 模板（或你的调度器等价物）。用一个小的 Megatron 配置拉起 2 节点 × 8 GPU 的训练：

```bash
sbatch train_2node.sbatch
# Inside the sbatch, pretrain_gpt.py with --num-layers 12 --hidden-size 2048
# --tensor-model-parallel-size 4 --pipeline-model-parallel-size 2
# --num-experts 8 --moe-router-topk 2 --expert-model-parallel-size 2
# --seq-length 8192 --use-flash-attn --bf16
# --train-iters 200 --save /shared/checkpoints/v1 --save-interval 100
```

确认：NCCL 选择了节点内 NVLink、节点间 IB，没有回退。单步耗时稳定。Loss 下降。

### 2. 检查点与恢复

在 step 100 之后，作业已保存检查点。杀掉它。用 `--load /shared/checkpoints/v1` 重启。确认运行恰好从 step 100 接续，且 loss 与杀进程前的轨迹一致。

### 3. 故障注入

在运行过程中，登录到某个 worker 节点，只在该节点上对训练进程执行 `kill -9`。调度器应检测到缺失的 rank，作业应拆除，重新拉起（通过 `--max-restarts` 或 Slurm requeue）应从最近的检查点继续。总停机时间：2 节点配置下理想情况小于 5 分钟。

如果重新拉起产生的 loss 曲线与杀进程前运行本应走的轨迹不同，那你在扩大规模之前就有一个确定性问题要修。

### 4. NCCL 性能剖析

用 `NCCL_DEBUG=INFO NCCL_DEBUG_FILE=/tmp/nccl.%h.%p.log` 重跑。检查日志：

- NCCL 是否选择了预期的算法（tree vs ring vs hierarchical）？
- 是否有回退到 TCP 或更慢路径的警告？
- 各 rank 的每 collective 时间是否一致？

把这个 profile 与训练日志一起留存；将来某次运行出现性能回退时，这是你第一个要查的东西。

---

## 在真实技术栈中使用

真实的研究 cluster 通常构建在以下之上：

- **NVIDIA Base Command Manager + DGX SuperPOD 参考架构** —— 教科书式的「我们卖你一套长上下文训练环境」的部署。
- **Slurm + Pyxis + Enroot**，用于裸金属 cluster 上的容器感知调度。
- **Kubernetes + KubeRay + GPU Operator**，用于云原生部署（在最大的训练作业中不太常见，因为 pod 重启延迟）。
- **CoreWeave / Lambda / Crusoe** 托管方案 —— 提供现成的 Slurm + IB + 存储栈，并交给你一个开箱即用的 launcher。

NVIDIA 的 MoE 长上下文技能页面假定使用 Slurm 风格的 launcher。无论你在哪个栈上，你的 launcher 都必须产出等价的环境变量和进程布局。

---


<details>
<summary>English original</summary>

**Checkpointing**

Long-context MoE training checkpoints are huge: a 70B-parameter model's optimizer state + parameters in fp32 is ~840 GB. Sharded checkpoints (one shard per DP × PP × EP × TP rank) are the only practical option.

The checkpoint layer:

- **Megatron's distributed checkpoint**: each rank writes its own shard; metadata file maps the mesh.
- **Torch DCP** (distributed checkpoint): newer, handles mesh changes between save and load.
- **Async checkpointing** (Megatron `--async-save`): write checkpoint in background, training continues; protects against the checkpoint-induced step-time spike.

I/O target: a 64×H200 cluster should checkpoint a 70B model in under 60 seconds end-to-end. If it takes longer, the storage tier is the bottleneck — see GPUDirect Storage module.

**Fault tolerance**

In a real run, hardware *will* fail: a GPU ECC error, a switch hiccup, a node lost to a kernel panic. The job must:

1. Detect the failure (NCCL hang detection, watchdog).
2. Tear down cleanly (avoid leaving zombie processes).
3. Relaunch from the last checkpoint, possibly on different physical nodes.
4. Log the incident.

Torchrun's `--max-restarts` handles the relaunch; `c10d` rendezvous handles re-membership. The training script must be deterministic enough that resuming from step N reproduces step N+1's behavior (or close enough that loss curves stay sensible).

A practical pattern: a Slurm signal handler on `USR1` triggers a graceful checkpoint, then the scheduler resubmits the job. The job picks up the latest checkpoint automatically.

**Monitoring**

Minimum dashboards:

- **Per-step time** + variance.
- **Per-rank loss** (must stay tight; divergence across ranks is a routing or numerics bug).
- **NCCL communication time** by collective.
- **GPU utilization, memory, temperature, power** per rank.
- **Network**: per-NIC throughput, retransmissions, link errors.
- **Storage**: write throughput during checkpoint windows.

Standard stack: NVIDIA DCGM exporter → Prometheus → Grafana. PyTorch profiler dumps for the deep-dive moments.

---

**Build it**

**1. Multi-node baseline**

Use the Slurm template above (or your scheduler's equivalent). Launch a 2-node × 8-GPU training run with a small Megatron config:

```bash
sbatch train_2node.sbatch
# Inside the sbatch, pretrain_gpt.py with --num-layers 12 --hidden-size 2048
# --tensor-model-parallel-size 4 --pipeline-model-parallel-size 2
# --num-experts 8 --moe-router-topk 2 --expert-model-parallel-size 2
# --seq-length 8192 --use-flash-attn --bf16
# --train-iters 200 --save /shared/checkpoints/v1 --save-interval 100
```

Confirm: NCCL chose NVLink intra-node, IB inter-node, no fallbacks. Per-step time stable. Loss decreasing.

**2. Checkpoint and resume**

After step 100, the job saved a checkpoint. Kill it. Restart with `--load /shared/checkpoints/v1`. Confirm the run picks up exactly at step 100 with a loss in line with the pre-kill trajectory.

**3. Fault injection**

While the run is going, log into one of the worker nodes and `kill -9` the training process on that node only. The scheduler should detect the missing rank, the job should tear down, and a relaunch (via `--max-restarts` or a Slurm requeue) should pick up from the last checkpoint. The total downtime: ideally under 5 minutes for a 2-node setup.

If the relaunch produces a different loss curve from what the pre-kill run was on track for, you have a determinism problem to fix before scaling up.

**4. NCCL profiling**

Rerun with `NCCL_DEBUG=INFO NCCL_DEBUG_FILE=/tmp/nccl.%h.%p.log`. Inspect the logs:

- Did NCCL pick the expected algorithm (tree vs ring vs hierarchical)?
- Are there warnings about fallbacks to TCP or to a slower path?
- Is the per-collective time consistent across ranks?

Persist this profile alongside your training log; it is the first thing you check when a future run regresses.

---

**Use it in the real stack**

Real research clusters tend to live on top of:

- **NVIDIA Base Command Manager + DGX SuperPOD reference architecture** — the canonical "we sell you a long-context training setup" deployment.
- **Slurm + Pyxis + Enroot** for container-aware scheduling on bare-metal clusters.
- **Kubernetes + KubeRay + GPU Operator** for cloud-native deployments (less common for largest training jobs because of pod-restart latency).
- **CoreWeave / Lambda / Crusoe** managed offerings — provide a ready-made Slurm + IB + storage stack and hand you a ready-to-use launcher.

The MoE long-context skill page from NVIDIA assumes a Slurm-style launcher. Whatever stack you're on, your launcher must produce equivalent environment variables and process placement.

---

</details>

## 实测

针对上述各次运行：

- **每步时间**的平均值与 p99。
- **NCCL 时间占每步时间的比例**（在你选择的配置下应 ≤ 30%）。
- **检查点写入时间**（目标：70B 级别 ≤ 60s）。
- **检查点恢复时间**（目标：从作业提交到第一个新 step ≤ 2 min）。
- **故障恢复总停机时间**（目标：≤ 5 min）。

将这些记录为该簇的 SLO。此后的性能回退都与此基线比较。

---

## 交付

放入 `lcm-course/`：

1. `train_2node.sbatch` —— 你的调度器启动模板。
2. `nccl_env.sh` —— 你最终选定的 NCCL 环境变量。
3. `multi_node_run.log` —— 精简日志，展示每步时间、NCCL 初始化、检查点与故障注入周期。
4. `infra_slo.md` —— 以可直接测量形式给出的簇 SLO（上述各项指标，附你实际测得的值）。

---

## 相关页面

- [Module 05 — MoE systems and infrastructure](05-MoE-Systems-Infrastructure.md)
- [Module 08 — Combining long-context and MoE](08-Combining-LongContext-and-MoE.md)
- [NCCL Deep Dive](../08-NCCL深度探索/README.md)
- [8×H200 Training/Inference](../01-8x-H200训练推理/README.md)
- [GPUDirect Storage](../05-GPUDirect存储/README.md)
- Megatron-LM 分布式检查点：<https://github.com/NVIDIA/Megatron-LM/blob/main/megatron/core/dist_checkpointing/README.md>
- NCCL 调优：<https://docs.nvidia.com/deeplearning/nccl/user-guide/docs/env.html>


<details>
<summary>English original</summary>

**Measure it**

For the runs above:

- **Per-step time** average and p99.
- **NCCL time as fraction of step time** (should be ≤ 30% at the configurations you chose).
- **Checkpoint write time** (target: ≤ 60s for 70B-class).
- **Checkpoint resume time** (target: ≤ 2 min from job submission to first new step).
- **Fault-recovery total downtime** (target: ≤ 5 min).

Record these as a SLO for the cluster. Future regressions are compared against this baseline.

---

**Ship it**

Drop into `lcm-course/`:

1. `train_2node.sbatch` — your scheduler launcher template.
2. `nccl_env.sh` — the NCCL environment variables you settled on.
3. `multi_node_run.log` — abbreviated log showing per-step time, NCCL inits, checkpoint and fault-injection cycles.
4. `infra_slo.md` — your cluster SLOs in measurement-ready form (numbers above with the values you actually measured).

---

**Related pages**

- [Module 05 — MoE systems and infrastructure](05-MoE-Systems-Infrastructure.md)
- [Module 08 — Combining long-context and MoE](08-Combining-LongContext-and-MoE.md)
- [NCCL Deep Dive](../08-NCCL深度探索/README.md)
- [8×H200 Training/Inference](../01-8x-H200训练推理/README.md)
- [GPUDirect Storage](../05-GPUDirect存储/README.md)
- Megatron-LM distributed checkpointing: <https://github.com/NVIDIA/Megatron-LM/blob/main/megatron/core/dist_checkpointing/README.md>
- NCCL tuning: <https://docs.nvidia.com/deeplearning/nccl/user-guide/docs/env.html>

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Long-Context-MoE-Foundation-Training/09-Distributed-Training-Infrastructure.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Long-Context-MoE-Foundation-Training/09-Distributed-Training-Infrastructure.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
