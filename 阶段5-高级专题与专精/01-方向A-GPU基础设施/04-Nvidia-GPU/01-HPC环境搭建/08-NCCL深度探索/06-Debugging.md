---
title: 06 — 生产环境中的 NCCL 故障调试
description: 06 — 生产环境中的 NCCL 故障调试
---

# 06 — 生产环境中的 NCCL 故障调试

## 1. NCCL 故障的三大类别

```
Category 1: HANGS (most common in production)
  All processes freeze. No error, no timeout (unless NCCL_TIMEOUT set).
  Root cause: one or more processes cannot reach a collective.

Category 2: ERRORS (second most common)
  Process crashes with ncclInternalError, ncclSystemError, etc.
  Root cause: network issues, GPU errors, software bugs.

Category 3: SILENT CORRUPTION (hardest to detect)
  Training runs but produces wrong results.
  Root cause: ECC errors, NVLink errors, numerical overflow.
```

---

## 2. 首轮响应检查清单

NCCL 挂起或崩溃时，立即执行此检查清单：

```bash
# 1. Check GPU health on all nodes
nvidia-smi -q | grep -E "ECC|Uncorrected|Temperature|Power"
nvidia-smi --query-gpu=index,ecc.errors.uncorrected.volatile.total --format=csv
# Non-zero ECC errors = hardware problem

# 2. Check NVLink errors (for NVLink systems)
nvidia-smi nvlink --errorcounters -i 0   # repeat for GPU 0-7
# Any counter > 0 is a problem

# 3. Check dmesg for PCIe or GPU hardware errors
dmesg | grep -E "NVRM|nvidia|PCIe|AER" | tail -50
# Look for: "PCIe Bus Error", "IOMMU", "XID" (nvidia error codes)

# 4. Check NCCL debug output (restart with NCCL_DEBUG=INFO to capture)
export NCCL_DEBUG=INFO
export NCCL_DEBUG_FILE=/tmp/nccl_%h_%p.log
# Rerun job, collect all log files from all nodes

# 5. Check if all ranks are alive
ps aux | grep python   # are all 8 processes still running?
# Kill orphan processes before retry:
pkill -f "torchrun\|python.*train"
```

---

## 3. 挂起诊断

### 模式 1：缺少一个 Rank

症状：作业在 `dist.init_process_group()` 处立即挂起。

```
Node 0: Rank 0, 1, 2, 3, 4, 5, 6, 7 all call ncclCommInitRank()
         Rank 7 waits for all 8 ranks to connect
Node 1: Only Rank 8, 9, 10, 11, 12, 13, 14 start (Rank 15 never launches)
         → All 15 other ranks hang indefinitely waiting for Rank 15
```

诊断：
```bash
# Check all processes are running
for node in node0 node1; do
    ssh $node "ps aux | grep python | grep -v grep | wc -l"
done
# Should show 8 on each node

# Check master port is reachable
nc -zv $MASTER_ADDR $MASTER_PORT
# Should connect; if not, firewall issue
```

修复：确保所有进程都已启动，修复防火墙规则，检查 Slurm 作业分配。

### 模式 2：训练过程中挂起（非初始化阶段）

症状：训练运行 N 步后卡在某个特定 layer/操作上。

```bash
# Attach gdb to find where each rank is stuck
gdb -p $(pgrep -f "python train.py" | head -1)
(gdb) thread apply all bt   # show stack trace for all threads

# Or use Python's faulthandler
import faulthandler
faulthandler.enable()   # dumps stack trace on SIGSEGV or after timeout

# Or signal all processes to print stack trace
kill -USR1 $(pgrep -f "python train.py")

# Check which NCCL operation is pending
export NCCL_DEBUG=INFO
# Look for last NCCL op before hang:
# "ncclAllReduce" → gradient sync hang
# "ncclAllGather" → FSDP parameter gather hang
# "ncclBroadcast" → model weight broadcast hang
```

常见原因：**控制流不均衡** —— 某个 rank 走了不同的代码路径，跳过了某次集合通信：

```python
# WRONG — can cause hang if condition differs across ranks
if loss.item() > threshold:        # loss.item() may differ between ranks!
    dist.all_reduce(some_tensor)   # only some ranks execute this

# CORRECT — synchronize the condition first
is_above = torch.tensor(loss.item() > threshold, device="cuda")
dist.all_reduce(is_above, op=dist.ReduceOp.SUM)
if is_above.item() > 0:            # now all ranks agree
    dist.all_reduce(some_tensor)
```

### 模式 3：网络挂起（多节点）

```bash
# Test basic network connectivity between nodes
ssh node1 ping -c 4 node0

# Test IB connectivity
ibping -S -d mlx5_0 -P 1   # start server on node1
ibping -d mlx5_0 -P 1 node1 # ping from node0
# Should show < 1 µs round-trip

# Test bandwidth
ib_write_bw -d mlx5_0 node1   # run on node0, point to node1
# Should show ~23 GB/s for HDR 200 Gb/s

# Check for dropped IB packets
perfquery -x 0 -d mlx5_0 -P 1  # shows error counters
# RcvSwRelayErrors, SymbolErrorCounter non-zero = physical link problem
```

---

## 4. 常见 NCCL 错误与修复

### `ncclSystemError` — 系统/网络问题

```
NCCL error: ncclSystemError (system/network error)
```

```bash
# Likely causes:
# 1. Out of shared memory
ls -la /dev/shm/
# NCCL uses /dev/shm for intra-node; if full, communication fails
# Fix:
rm /dev/shm/nccl-*   # clean stale NCCL shared memory files

# 2. InfiniBand device error
ibstat | grep State
# Should show "Active" for all ports

# 3. CUDA out of memory during NCCL buffer allocation
# NCCL needs ~300 MB per GPU for communication buffers
# Check: nvidia-smi shows GPU memory before launching
```


<details>
<summary>English original</summary>

**06 — Debugging NCCL Failures in Production**

**1. The Three Categories of NCCL Failures**

```
Category 1: HANGS (most common in production)
  All processes freeze. No error, no timeout (unless NCCL_TIMEOUT set).
  Root cause: one or more processes cannot reach a collective.

Category 2: ERRORS (second most common)
  Process crashes with ncclInternalError, ncclSystemError, etc.
  Root cause: network issues, GPU errors, software bugs.

Category 3: SILENT CORRUPTION (hardest to detect)
  Training runs but produces wrong results.
  Root cause: ECC errors, NVLink errors, numerical overflow.
```

---

**2. First Response Checklist**

When NCCL hangs or crashes, run this checklist immediately:

```bash
# 1. Check GPU health on all nodes
nvidia-smi -q | grep -E "ECC|Uncorrected|Temperature|Power"
nvidia-smi --query-gpu=index,ecc.errors.uncorrected.volatile.total --format=csv
# Non-zero ECC errors = hardware problem

# 2. Check NVLink errors (for NVLink systems)
nvidia-smi nvlink --errorcounters -i 0   # repeat for GPU 0-7
# Any counter > 0 is a problem

# 3. Check dmesg for PCIe or GPU hardware errors
dmesg | grep -E "NVRM|nvidia|PCIe|AER" | tail -50
# Look for: "PCIe Bus Error", "IOMMU", "XID" (nvidia error codes)

# 4. Check NCCL debug output (restart with NCCL_DEBUG=INFO to capture)
export NCCL_DEBUG=INFO
export NCCL_DEBUG_FILE=/tmp/nccl_%h_%p.log
# Rerun job, collect all log files from all nodes

# 5. Check if all ranks are alive
ps aux | grep python   # are all 8 processes still running?
# Kill orphan processes before retry:
pkill -f "torchrun\|python.*train"
```

---

**3. Diagnosing Hangs**

**Pattern 1: One Rank is Missing**

Symptom: Job hangs immediately at `dist.init_process_group()`.

```
Node 0: Rank 0, 1, 2, 3, 4, 5, 6, 7 all call ncclCommInitRank()
         Rank 7 waits for all 8 ranks to connect
Node 1: Only Rank 8, 9, 10, 11, 12, 13, 14 start (Rank 15 never launches)
         → All 15 other ranks hang indefinitely waiting for Rank 15
```

Diagnosis:
```bash
# Check all processes are running
for node in node0 node1; do
    ssh $node "ps aux | grep python | grep -v grep | wc -l"
done
# Should show 8 on each node

# Check master port is reachable
nc -zv $MASTER_ADDR $MASTER_PORT
# Should connect; if not, firewall issue
```

Fix: ensure all processes launch, fix firewall rules, check Slurm job allocation.

**Pattern 2: Hang During Training (Not Init)**

Symptom: Training runs for N steps then freezes at a specific layer/operation.

```bash
# Attach gdb to find where each rank is stuck
gdb -p $(pgrep -f "python train.py" | head -1)
(gdb) thread apply all bt   # show stack trace for all threads

# Or use Python's faulthandler
import faulthandler
faulthandler.enable()   # dumps stack trace on SIGSEGV or after timeout

# Or signal all processes to print stack trace
kill -USR1 $(pgrep -f "python train.py")

# Check which NCCL operation is pending
export NCCL_DEBUG=INFO
# Look for last NCCL op before hang:
# "ncclAllReduce" → gradient sync hang
# "ncclAllGather" → FSDP parameter gather hang
# "ncclBroadcast" → model weight broadcast hang
```

Common cause: **unbalanced control flow** — one rank takes a different code path and skips a collective:

```python
# WRONG — can cause hang if condition differs across ranks
if loss.item() > threshold:        # loss.item() may differ between ranks!
    dist.all_reduce(some_tensor)   # only some ranks execute this

# CORRECT — synchronize the condition first
is_above = torch.tensor(loss.item() > threshold, device="cuda")
dist.all_reduce(is_above, op=dist.ReduceOp.SUM)
if is_above.item() > 0:            # now all ranks agree
    dist.all_reduce(some_tensor)
```

**Pattern 3: Network Hang (Multi-Node)**

```bash
# Test basic network connectivity between nodes
ssh node1 ping -c 4 node0

# Test IB connectivity
ibping -S -d mlx5_0 -P 1   # start server on node1
ibping -d mlx5_0 -P 1 node1 # ping from node0
# Should show < 1 µs round-trip

# Test bandwidth
ib_write_bw -d mlx5_0 node1   # run on node0, point to node1
# Should show ~23 GB/s for HDR 200 Gb/s

# Check for dropped IB packets
perfquery -x 0 -d mlx5_0 -P 1  # shows error counters
# RcvSwRelayErrors, SymbolErrorCounter non-zero = physical link problem
```

---

**4. Common NCCL Errors and Fixes**

**`ncclSystemError` — System/Network Issue**

```
NCCL error: ncclSystemError (system/network error)
```

```bash
# Likely causes:
# 1. Out of shared memory
ls -la /dev/shm/
# NCCL uses /dev/shm for intra-node; if full, communication fails
# Fix:
rm /dev/shm/nccl-*   # clean stale NCCL shared memory files

# 2. InfiniBand device error
ibstat | grep State
# Should show "Active" for all ports

# 3. CUDA out of memory during NCCL buffer allocation
# NCCL needs ~300 MB per GPU for communication buffers
# Check: nvidia-smi shows GPU memory before launching
```

</details>

### `ncclInvalidArgument` — 用法错误

```
NCCL error: ncclInvalidArgument (invalid argument)
```

常见原因：tensor 不连续、数据类型错误、尺寸不匹配。

```python
# Ensure tensors are contiguous before NCCL
tensor = tensor.contiguous()
dist.all_reduce(tensor)

# Ensure all ranks have the same tensor size
assert tensor.numel() == expected_size, f"Rank {rank}: got {tensor.numel()}, expected {expected_size}"

# Ensure data types match
tensor = tensor.to(torch.float32)  # NCCL doesn't support BF16 for all ops on older versions
```

### `ncclUnhandledCudaError` — GPU 错误

```
NCCL error: ncclUnhandledCudaError (unhandled CUDA error)
```

```bash
# Check CUDA error
python -c "import torch; torch.cuda.check_error(torch.cuda.current_device())"

# Run with CUDA error checking enabled
CUDA_LAUNCH_BLOCKING=1 torchrun ...   # makes CUDA errors synchronous and easier to trace

# Check for GPU hardware errors
nvidia-smi --query-gpu=index,ecc.errors.uncorrected.volatile.total --format=csv
# Non-zero = bad GPU; replace hardware
```

### 超时（长时间挂起）

```python
# PyTorch DDP has a built-in timeout
import datetime
dist.init_process_group(
    backend="nccl",
    timeout=datetime.timedelta(seconds=300),   # 5-minute timeout
)
# After 300s of a hung collective: ncclInternalError with timeout message
```

```bash
# NCCL-level timeout
export NCCL_TIMEOUT=300   # 300 seconds
```

---

## 5. XID 代码 — Nvidia GPU 错误 ID

当 `dmesg` 显示 `nvidia: Xid...` 时，这些是 GPU 硬件错误：

```bash
dmesg | grep "Xid"
# Examples:
# Xid 43: GPU-NVLink error     → NVLink hardware problem
# Xid 48: DBE (uncorrected)    → Double-bit ECC error → GPU memory failing
# Xid 79: GPU hang detected    → GPU kernel timeout
# Xid 94: Container violated   → GPU process isolation issue
# Xid 119: GSP RPC timeout     → Driver-firmware communication failure
```

| XID | 含义 | 处理措施 |
|---|---|---|
| 43 | NVLink 错误 | 检查 `nvidia-smi nvlink --errorcounters`，检查线缆 |
| 48 | 未纠正的 ECC | GPU 显存故障，更换 GPU |
| 79 | GPU 挂起 | 驱动或 kernel bug，更新驱动 |
| 94 | 容器错误 | 检查 CUDA/Docker 版本兼容性 |
| 119 | GSP 超时 | 驱动问题，重新加载驱动或重启 |

```bash
# Enable ECC to catch memory errors before they become Xid 48
sudo nvidia-smi -e 1   # enable ECC (requires reboot)
```

---

## 6. 用 NVTX 和 Nsight 调试 NCCL

```python
# Add NVTX markers around NCCL calls for timeline visualization
import torch.cuda.nvtx as nvtx

class DebugDDP(torch.nn.parallel.DistributedDataParallel):
    def _run_ddp_forward(self, *args, **kwargs):
        nvtx.range_push("DDP_forward")
        result = super()._run_ddp_forward(*args, **kwargs)
        nvtx.range_pop()
        return result

# Profile with Nsight Systems:
nsys profile \
    --trace=cuda,nvtx,nccl \
    --output=nccl_debug \
    torchrun --nproc_per_node=8 train.py
# Open nccl_debug.nsys-rep — NCCL ops appear as colored blocks
# Look for: long gaps between compute and NCCL = communication bottleneck
```

---

## 7. 容错：从 NCCL 故障中恢复

### PyTorch Elastic 训练（自动恢复）

```bash
# torchrun with elastic training: automatically restarts on node failure
torchrun \
    --nnodes=4:8 \                     # accept 4-8 nodes (elastic range)
    --nproc_per_node=8 \
    --max_restarts=3 \                 # retry up to 3 times
    --rdzv_backend=etcd \             # use etcd for rendezvous (fault-tolerant)
    --rdzv_endpoint=etcd-server:2379 \
    train.py
```

```python
# Training code must use checkpointing for elastic recovery
import torch.distributed.elastic.multiprocessing as mp

def train(state):
    # Load from checkpoint if restarting
    if state.step > 0:
        model.load_state_dict(torch.load(f"checkpoint_{state.step}.pt"))

    for step in range(state.step, total_steps):
        loss = compute_loss(model, batch)
        loss.backward()
        optimizer.step()

        # Save checkpoint every N steps
        if step % 100 == 0:
            torch.save(model.state_dict(), f"checkpoint_{step}.pt")
            state.step = step
```

### 手动检查点重启

```bash
#!/bin/bash
# retry_training.sh — simple checkpoint-restart loop

MAX_RETRIES=5
RETRY=0
while [ $RETRY -lt $MAX_RETRIES ]; do
    torchrun --nnodes=4 --nproc_per_node=8 train.py \
        --resume-from-checkpoint latest

    EXIT_CODE=$?
    if [ $EXIT_CODE -eq 0 ]; then
        echo "Training complete"
        break
    else
        RETRY=$((RETRY+1))
        echo "Training failed (exit $EXIT_CODE), retry $RETRY/$MAX_RETRIES"
        sleep 30  # wait before retry
    fi
done
```

---


<details>
<summary>English original</summary>

**`ncclInvalidArgument` — Wrong Usage**

```
NCCL error: ncclInvalidArgument (invalid argument)
```

Common cause: tensor not contiguous, wrong data type, mismatched sizes.

```python
# Ensure tensors are contiguous before NCCL
tensor = tensor.contiguous()
dist.all_reduce(tensor)

# Ensure all ranks have the same tensor size
assert tensor.numel() == expected_size, f"Rank {rank}: got {tensor.numel()}, expected {expected_size}"

# Ensure data types match
tensor = tensor.to(torch.float32)  # NCCL doesn't support BF16 for all ops on older versions
```

**`ncclUnhandledCudaError` — GPU Error**

```
NCCL error: ncclUnhandledCudaError (unhandled CUDA error)
```

```bash
# Check CUDA error
python -c "import torch; torch.cuda.check_error(torch.cuda.current_device())"

# Run with CUDA error checking enabled
CUDA_LAUNCH_BLOCKING=1 torchrun ...   # makes CUDA errors synchronous and easier to trace

# Check for GPU hardware errors
nvidia-smi --query-gpu=index,ecc.errors.uncorrected.volatile.total --format=csv
# Non-zero = bad GPU; replace hardware
```

**Timeout (Long-Running Hangs)**

```python
# PyTorch DDP has a built-in timeout
import datetime
dist.init_process_group(
    backend="nccl",
    timeout=datetime.timedelta(seconds=300),   # 5-minute timeout
)
# After 300s of a hung collective: ncclInternalError with timeout message
```

```bash
# NCCL-level timeout
export NCCL_TIMEOUT=300   # 300 seconds
```

---

**5. XID Codes — Nvidia GPU Error IDs**

When `dmesg` shows `nvidia: Xid...`, these are GPU hardware errors:

```bash
dmesg | grep "Xid"
# Examples:
# Xid 43: GPU-NVLink error     → NVLink hardware problem
# Xid 48: DBE (uncorrected)    → Double-bit ECC error → GPU memory failing
# Xid 79: GPU hang detected    → GPU kernel timeout
# Xid 94: Container violated   → GPU process isolation issue
# Xid 119: GSP RPC timeout     → Driver-firmware communication failure
```

| XID | Meaning | Action |
|---|---|---|
| 43 | NVLink error | Check `nvidia-smi nvlink --errorcounters`, check cables |
| 48 | Uncorrected ECC | GPU memory failing, replace GPU |
| 79 | GPU hang | Driver or kernel bug, update driver |
| 94 | Container error | Check CUDA/Docker version compatibility |
| 119 | GSP timeout | Driver issue, reload driver or reboot |

```bash
# Enable ECC to catch memory errors before they become Xid 48
sudo nvidia-smi -e 1   # enable ECC (requires reboot)
```

---

**6. Debugging NCCL with NVTX and Nsight**

```python
# Add NVTX markers around NCCL calls for timeline visualization
import torch.cuda.nvtx as nvtx

class DebugDDP(torch.nn.parallel.DistributedDataParallel):
    def _run_ddp_forward(self, *args, **kwargs):
        nvtx.range_push("DDP_forward")
        result = super()._run_ddp_forward(*args, **kwargs)
        nvtx.range_pop()
        return result

# Profile with Nsight Systems:
nsys profile \
    --trace=cuda,nvtx,nccl \
    --output=nccl_debug \
    torchrun --nproc_per_node=8 train.py
# Open nccl_debug.nsys-rep — NCCL ops appear as colored blocks
# Look for: long gaps between compute and NCCL = communication bottleneck
```

---

**7. Fault Tolerance: Recovering from NCCL Failures**

**PyTorch Elastic Training (Automatic Recovery)**

```bash
# torchrun with elastic training: automatically restarts on node failure
torchrun \
    --nnodes=4:8 \                     # accept 4-8 nodes (elastic range)
    --nproc_per_node=8 \
    --max_restarts=3 \                 # retry up to 3 times
    --rdzv_backend=etcd \             # use etcd for rendezvous (fault-tolerant)
    --rdzv_endpoint=etcd-server:2379 \
    train.py
```

```python
# Training code must use checkpointing for elastic recovery
import torch.distributed.elastic.multiprocessing as mp

def train(state):
    # Load from checkpoint if restarting
    if state.step > 0:
        model.load_state_dict(torch.load(f"checkpoint_{state.step}.pt"))

    for step in range(state.step, total_steps):
        loss = compute_loss(model, batch)
        loss.backward()
        optimizer.step()

        # Save checkpoint every N steps
        if step % 100 == 0:
            torch.save(model.state_dict(), f"checkpoint_{step}.pt")
            state.step = step
```

**Manual Checkpoint-Restart**

```bash
#!/bin/bash
# retry_training.sh — simple checkpoint-restart loop

MAX_RETRIES=5
RETRY=0
while [ $RETRY -lt $MAX_RETRIES ]; do
    torchrun --nnodes=4 --nproc_per_node=8 train.py \
        --resume-from-checkpoint latest

    EXIT_CODE=$?
    if [ $EXIT_CODE -eq 0 ]; then
        echo "Training complete"
        break
    else
        RETRY=$((RETRY+1))
        echo "Training failed (exit $EXIT_CODE), retry $RETRY/$MAX_RETRIES"
        sleep 30  # wait before retry
    fi
done
```

---

</details>

## 8. 调试检查清单小结

```
NCCL Hang:
[ ] Are all N processes running? (ps aux count matches world_size)
[ ] Is master port reachable? (nc -zv MASTER_ADDR MASTER_PORT)
[ ] Is there a conditional collective? (all ranks must call every collective)
[ ] Check NCCL_DEBUG=INFO for last operation before hang
[ ] Check IB link status: ibstat
[ ] Set NCCL_TIMEOUT=300 to force timeout + error message

NCCL Error:
[ ] Check nvidia-smi for ECC errors
[ ] Check dmesg for Xid codes
[ ] Check /dev/shm/ for stale NCCL files
[ ] Run with CUDA_LAUNCH_BLOCKING=1 for synchronous error reporting
[ ] Check tensor is contiguous and correct dtype

Performance Issue:
[ ] Run nccl-tests to verify expected bus bandwidth
[ ] Check NCCL_DEBUG=INFO for algorithm selected (Ring/Tree)
[ ] Profile with Nsight Systems to find communication gaps
[ ] Verify P2P_LEVEL=NVL for NVLink systems
[ ] Check bucket_cap_mb in DDP, reduce_bucket_size in DeepSpeed
```

---

## 参考资料

- [PyTorch NCCL 错误处理](https://pytorch.org/docs/stable/distributed.html#torch.distributed.is_available)
- [NCCL 已知问题](https://docs.nvidia.com/deeplearning/nccl/release-notes/rel_2-20-5.html)
- [Nvidia XID 错误参考](https://docs.nvidia.com/deploy/xid-errors/index.html)
- [PyTorch Elastic 训练](https://pytorch.org/docs/stable/elastic/run.html)
- [NCCL GitHub Issues](https://github.com/NVIDIA/nccl/issues) — 提新 issue 前先搜索


<details>
<summary>English original</summary>

**8. Debugging Checklist Summary**

```
NCCL Hang:
[ ] Are all N processes running? (ps aux count matches world_size)
[ ] Is master port reachable? (nc -zv MASTER_ADDR MASTER_PORT)
[ ] Is there a conditional collective? (all ranks must call every collective)
[ ] Check NCCL_DEBUG=INFO for last operation before hang
[ ] Check IB link status: ibstat
[ ] Set NCCL_TIMEOUT=300 to force timeout + error message

NCCL Error:
[ ] Check nvidia-smi for ECC errors
[ ] Check dmesg for Xid codes
[ ] Check /dev/shm/ for stale NCCL files
[ ] Run with CUDA_LAUNCH_BLOCKING=1 for synchronous error reporting
[ ] Check tensor is contiguous and correct dtype

Performance Issue:
[ ] Run nccl-tests to verify expected bus bandwidth
[ ] Check NCCL_DEBUG=INFO for algorithm selected (Ring/Tree)
[ ] Profile with Nsight Systems to find communication gaps
[ ] Verify P2P_LEVEL=NVL for NVLink systems
[ ] Check bucket_cap_mb in DDP, reduce_bucket_size in DeepSpeed
```

---

**References**

- [PyTorch NCCL Error Handling](https://pytorch.org/docs/stable/distributed.html#torch.distributed.is_available)
- [NCCL Known Issues](https://docs.nvidia.com/deeplearning/nccl/release-notes/rel_2-20-5.html)
- [Nvidia XID Error Reference](https://docs.nvidia.com/deploy/xid-errors/index.html)
- [PyTorch Elastic Training](https://pytorch.org/docs/stable/elastic/run.html)
- [NCCL GitHub Issues](https://github.com/NVIDIA/nccl/issues) — search before opening new issues

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/NCCL-Deep-Dive/06-Debugging.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/NCCL-Deep-Dive/06-Debugging.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
