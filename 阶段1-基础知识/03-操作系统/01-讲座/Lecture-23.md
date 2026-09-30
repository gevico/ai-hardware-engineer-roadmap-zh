---
title: 第 23 讲：容器、cgroups v2 与 NVIDIA Container Runtime
description: 第 23 讲：容器、cgroups v2 与 NVIDIA Container Runtime
---

# 第 23 讲：容器、cgroups v2 与 NVIDIA Container Runtime

## 概述

容器是在云端与边缘环境中部署 AI 推理软件的标准打包格式。它们解决了 AI 部署中的一个根本问题：一个在某台机器上正确运行的 TensorRT 模型——依赖特定的 CUDA、cuDNN 与 TensorRT 库版本——在另一台机器上可能静默地产生不同结果，或者干脆失败。容器把所有依赖打包进一个不可变镜像，使部署可复现。

本讲需要贯穿始终的心智模型是**分层隔离栈**：Linux 命名空间控制容器能*看见*什么（进程、网络、文件系统）；cgroups 控制容器能*消耗*什么（CPU、内存、GPU）；NVIDIA Container Runtime 控制容器能访问哪些 GPU 硬件与库。这三套机制合在一起，让容器获得接近裸机的性能，同时保护宿主机免于资源耗尽与相互干扰。

AI 硬件工程师需要理解这一栈，因为容器化推理正是 TensorRT 模型大规模上生产的部署方式。理解 cgroups 是对延迟敏感的推理线程做 CPU 绑核的前提。理解 NVIDIA Container Runtime 才能解释为什么容器内 GPU 访问可用、却不必在每个镜像里安装 CUDA。而 MIG（Multi-Instance GPU）使多个独立推理工作负载能够共享一块 A100 或 H100，并带性能隔离保证。

---

## 容器 vs 虚拟机

容器**共享宿主机内核**。隔离由 Linux 命名空间（可见性）与 cgroups（资源限制）提供。**没有 hypervisor**，没有客户机内核，也没有硬件模拟。

| 属性 | 容器 | VM |
|---|---|---|
| 内核 | 与宿主机共享 | 独立的客户机内核 |
| 启动时间 | 5–50 ms | 1–10 s |
| 开销 | ~1% CPU | 5–15% CPU（hypervisor） |
| 隔离 | 命名空间 + cgroups | 完整硬件虚拟化 |
| GPU 访问 | 直接（直通） | 需要 GPU 虚拟化（vGPU） |

对推理部署而言，容器以**可复现的软件环境**提供**接近裸机的 GPU 性能**。

> **关键洞察：** 与 VM 相比，容器之所以能做到接近零开销，原因在于容器与硬件之间没有 hypervisor。容器内的一个 GPU kernel 由与原生 GPU kernel 相同的物理 GPU SM 执行——不存在翻译或模拟层。容器的 GPU 访问只不过是把宿主机的 `/dev/nvidia0` 设备节点挂载进容器的文件系统命名空间。

---

## Linux 命名空间

**七种命名空间类型**隔离系统环境的不同方面。每个命名空间都是一个**内核对象**；共享同一命名空间的进程看到该资源的同一视图。

| 命名空间 | 隔离内容 | 关键细节 |
|---|---|---|
| PID | 进程树 | 容器 init = PID 1；看不到宿主机 PID |
| Network | 网络栈 | 私有接口、iptables、routing、端口 |
| Mount | 文件系统视图 | overlayfs 建立之后的 `pivot_root` |
| UTS | 主机名、域名 | `uname -n` 返回容器名 |
| IPC | System V IPC + POSIX shm | 阻止跨容器共享内存 |
| User | UID/GID 映射 | 容器 UID 0 → 宿主机 UID 1000（rootless） |
| Time（5.6+） | CLOCK_BOOTTIME/MONOTONIC 偏移 | CRIU 检查点/恢复 |

命名空间之间的关系：

```
Host namespace view:          Container namespace view:
  PID 1: systemd                PID 1: python inference_server.py
  PID 847: containerd           PID 2: /bin/sh (entrypoint)
  PID 1023: inference_server ←──── same process, different PID
  PID 1024: /bin/sh         ←────  same process, different PID
  eth0: 192.168.1.100           eth0: 172.17.0.2 (virtual veth)
  /: host rootfs                /: container overlayfs rootfs
                                    (host rootfs not visible)
```

### 关键操作

```bash
unshare --pid --fork --mount-proc bash   # new PID namespace with fresh /proc
ip netns add myns                        # create network namespace
ip netns exec myns ip addr               # run command inside namespace
nsenter -t <PID> --net --pid bash        # enter existing process's namespaces
```

这些命令正是容器 runtime 以编程方式所做的事。`unshare` **创建新命名空间**；`nsenter` **加入已有命名空间**。当你 `docker exec` 进入一个运行中的容器时，Docker 会调用 `nsenter`，在容器已有的命名空间集合中运行你的命令。

> **关键洞察：** 命名空间本身并不提供安全边界——它们只控制可见性。PID 命名空间中的进程看不到宿主机 PID，但如果它（借由某个漏洞）逃出了自己的 mount 命名空间，就能读取宿主机文件。真正的容器安全需要把命名空间与 seccomp 过滤器、AppArmor 配置文件，以及正确的 user 命名空间配置结合起来。

---


<details>
<summary>English original</summary>

**Lecture 23: Containers, cgroups v2 & NVIDIA Container Runtime**

**Overview**

Containers are the standard packaging format for deploying AI inference software in both cloud and edge environments. They solve a fundamental problem in AI deployment: a TensorRT model that runs correctly on one machine — with specific CUDA, cuDNN, and TensorRT library versions — may silently produce different results or fail entirely on another. Containers bundle all dependencies into an immutable image, making deployment reproducible.

The mental model to carry through this lecture is the **layered isolation stack**: Linux namespaces control what a container can see (processes, network, filesystem); cgroups control what a container can consume (CPU, memory, GPU); and the NVIDIA Container Runtime controls what GPU hardware and libraries the container can access. Together these three mechanisms give a container near-bare-metal performance while protecting the host from resource exhaustion and interference.

AI hardware engineers need to understand this stack because containerized inference is how TensorRT models are deployed to production at scale. Understanding cgroups is required for CPU pinning of latency-sensitive inference threads. Understanding the NVIDIA Container Runtime explains why GPU access works inside a container without installing CUDA in every image. And MIG (Multi-Instance GPU) enables multiple independent inference workloads to share an A100 or H100 with guaranteed performance isolation.

---

**Containers vs Virtual Machines**

Containers **share the host kernel**. Isolation is provided by Linux namespaces (visibility) and cgroups (resource limits). There is **no hypervisor**, no guest kernel, and no hardware emulation.

| Property | Container | VM |
|---|---|---|
| Kernel | Shared with host | Separate guest kernel |
| Startup time | 5–50 ms | 1–10 s |
| Overhead | ~1% CPU | 5–15% CPU (hypervisor) |
| Isolation | Namespace + cgroups | Full hardware virtualization |
| GPU access | Direct (passthrough) | Requires GPU virtualization (vGPU) |

For inference deployment, containers provide **near-bare-metal GPU performance** with **reproducible software environments**.

> **Key Insight:** The reason containers achieve near-zero overhead compared to VMs is that there is no hypervisor between the container and the hardware. A GPU kernel inside a container is executed by the same physical GPU SMs that execute a native GPU kernel — there is no translation or emulation layer. The container's GPU access is just the host's `/dev/nvidia0` device node mounted into the container's filesystem namespace.

---

**Linux Namespaces**

**Seven namespace types** isolate different aspects of the system environment. Each namespace is a **kernel object**; processes that share a namespace see the same view of that resource.

| Namespace | Isolates | Key detail |
|---|---|---|
| PID | Process tree | Container init = PID 1; cannot see host PIDs |
| Network | Network stack | Private interfaces, iptables, routing, ports |
| Mount | Filesystem view | `pivot_root` after overlayfs setup |
| UTS | Hostname, domain name | `uname -n` returns container name |
| IPC | System V IPC + POSIX shm | Prevents cross-container shared memory |
| User | UID/GID mapping | Container UID 0 → host UID 1000 (rootless) |
| Time (5.6+) | CLOCK_BOOTTIME/MONOTONIC offset | CRIU checkpoint/restore |

The relationship between namespaces:

```
Host namespace view:          Container namespace view:
  PID 1: systemd                PID 1: python inference_server.py
  PID 847: containerd           PID 2: /bin/sh (entrypoint)
  PID 1023: inference_server ←──── same process, different PID
  PID 1024: /bin/sh         ←────  same process, different PID
  eth0: 192.168.1.100           eth0: 172.17.0.2 (virtual veth)
  /: host rootfs                /: container overlayfs rootfs
                                    (host rootfs not visible)
```

**Key Operations**

```bash
unshare --pid --fork --mount-proc bash   # new PID namespace with fresh /proc
ip netns add myns                        # create network namespace
ip netns exec myns ip addr               # run command inside namespace
nsenter -t <PID> --net --pid bash        # enter existing process's namespaces
```

These commands are what container runtimes do programmatically. `unshare` **creates new namespaces**; `nsenter` **joins existing ones**. When you `docker exec` into a running container, Docker calls `nsenter` to run your command in the container's existing namespace set.

> **Key Insight:** Namespaces do not provide security boundaries by themselves — they only control visibility. A process in a PID namespace cannot see host PIDs, but if it escapes its mount namespace (via a vulnerability), it can read host files. Real container security requires combining namespaces with seccomp filters, AppArmor profiles, and proper user namespace configuration.

---

</details>

## cgroups v2（统一层级）

Namespace 控制**可见性**。cgroup 控制**资源消耗**。二者共同定义完整的容器隔离模型。

cgroups v2 用位于 `/sys/fs/cgroup/` 的**单一统一层级**取代了 v1 按子系统划分的层级。控制器按 cgroup 启用，并由子 cgroup 继承。

### 控制器参考

| 控制器 | 关键文件 | 示例值 | 效果 |
|---|---|---|---|
| `cpu` | `cpu.max` | `500000 1000000` | 限制为单颗 CPU 的 50% |
| `memory` | `memory.max` | `4G` | 硬性内存上限；触发 OOM kill |
| `memory` | `memory.swap.max` | `0` | 对此 cgroup 禁用 swap |
| `cpuset` | `cpuset.cpus` | `4-7` | 限定到 4–7 号核心 |
| `cpuset` | `cpuset.cpus.exclusive` | `1` | 独占分配（Kubernetes static CPU） |
| `io` | `io.max` | `8:0 rbps=1073741824` | 限制设备 8:0 的读取为 1 GB/s |
| `pids` | `pids.max` | `256` | cgroup 中的最大进程数 |

### 创建与管理 cgroups

```bash
# Create a new cgroup for an inference process
mkdir /sys/fs/cgroup/inference

# Pin to cores 4-7 (isolated from OS and other workloads)
echo "4-7" > /sys/fs/cgroup/inference/cpuset.cpus

# Restrict to NUMA node 0 memory
echo "0"   > /sys/fs/cgroup/inference/cpuset.mems

# Hard memory limit: OOM kill if exceeded
echo "8G"  > /sys/fs/cgroup/inference/memory.max

# Disable swap: no latency spikes from swap I/O
echo "0"   > /sys/fs/cgroup/inference/memory.swap.max

# Move the current shell (and any processes it starts) into the cgroup
echo $$    > /sys/fs/cgroup/inference/cgroup.procs
```

执行这些命令后，从该 shell 启动的任何进程都会在施加 cgroup 约束的情况下运行。内核**透明地强制执行这些限制**——进程本身无需修改。

Kubernetes 用 cgroups v2 完成**全部资源限制的强制执行**。kubelet 按 Pod、按容器创建 cgroup 层级；container runtime 把资源限制写入对应的 cgroup 文件。

> **常见陷阱：** 只设置 `memory.max` 而不设置 `memory.swap.max`。如果 swap 可用且触达 memory.max，内核会把页面换到 swap，而不是触发 OOM kill。对推理容器而言，这会造成灾难性的延迟尖峰（swap I/O 比 RAM 慢若干个数量级）。务必把 `memory.max` 和 `memory.swap.max` 设为同一个值（或者把 `memory.swap.max` 设为 0，对该 cgroup 完全禁用 swap）。

---

## 容器 Runtime

cgroup 与 namespace 的机制由 container runtime 来编排。

| Runtime | 角色 | 说明 |
|---|---|---|
| `containerd` | 高层；管理镜像与快照 | Kubernetes（CRI）的默认选择 |
| `runc` | 底层 OCI runtime；创建 namespace 与 cgroup | 被 containerd 使用 |
| `crun` | runc 的 C 语言重实现 | 启动快 2–5×；内存占用更低 |
| `kata-containers` | 每容器一个轻量级 VM | 隔离更强；用于多租户 GPU |

完整的容器启动流程：

1. **containerd** 从 Kubernetes（或 Docker）接收 OCI 容器 spec。
2. **containerd** 从镜像 layer 准备 overlayfs rootfs（借助 snapshotter）。
3. **containerd** fork 出 `runc`，以 OCI spec 作为输入。
4. **runc** 调用 `clone()` 并传入 `CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWNS`，创建新的 namespace。
5. **runc** 设置 overlayfs 挂载，并调用 `pivot_root()` 切换文件系统根。
6. **runc** 把资源限制写入 cgroup 文件（memory.max、cpu.max、cpuset.cpus）。
7. **runc** 执行容器 entrypoint（例如 `python inference_server.py`）。

`containerd` → `runc` → `clone()` 并传入 `CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWNS` → 设置 overlayfs rootfs → `pivot_root` → 写入 cgroup 限制 → exec 容器 entrypoint。

> **关键洞察：** 容器的进程从第 4 步起就已运行在新的 namespace 中，但只有到第 5 步执行 `pivot_root` 之后，它才能看到自己的文件系统。cgroup 限制在第 6 步写入，早于第 7 步应用代码启动。这一顺序保证了应用运行时，所有隔离与资源限制均已就位。

---


<details>
<summary>English original</summary>

**cgroups v2 (Unified Hierarchy)**

Namespaces control **visibility**. cgroups control **resource consumption**. Together they define the complete container isolation model.

cgroups v2 replaced the per-subsystem hierarchy of v1 with a **single unified hierarchy** at `/sys/fs/cgroup/`. Controllers are enabled per-cgroup and inherited by children.

**Controller Reference**

| Controller | Key file | Example value | Effect |
|---|---|---|---|
| `cpu` | `cpu.max` | `500000 1000000` | Limit to 50% of one CPU |
| `memory` | `memory.max` | `4G` | Hard memory limit; triggers OOM kill |
| `memory` | `memory.swap.max` | `0` | Disable swap for this cgroup |
| `cpuset` | `cpuset.cpus` | `4-7` | Restrict to cores 4–7 |
| `cpuset` | `cpuset.cpus.exclusive` | `1` | Exclusive assignment (Kubernetes static CPU) |
| `io` | `io.max` | `8:0 rbps=1073741824` | Limit to 1 GB/s read on device 8:0 |
| `pids` | `pids.max` | `256` | Maximum number of processes in cgroup |

**Creating and Managing cgroups**

```bash
# Create a new cgroup for an inference process
mkdir /sys/fs/cgroup/inference

# Pin to cores 4-7 (isolated from OS and other workloads)
echo "4-7" > /sys/fs/cgroup/inference/cpuset.cpus

# Restrict to NUMA node 0 memory
echo "0"   > /sys/fs/cgroup/inference/cpuset.mems

# Hard memory limit: OOM kill if exceeded
echo "8G"  > /sys/fs/cgroup/inference/memory.max

# Disable swap: no latency spikes from swap I/O
echo "0"   > /sys/fs/cgroup/inference/memory.swap.max

# Move the current shell (and any processes it starts) into the cgroup
echo $$    > /sys/fs/cgroup/inference/cgroup.procs
```

After these commands, any process started from this shell runs with the cgroup constraints applied. The kernel **enforces the limits transparently** — the process does not need to be modified.

Kubernetes uses cgroups v2 for **all resource enforcement**. The kubelet creates a cgroup hierarchy per-Pod and per-container; the container runtime writes resource limits into the appropriate cgroup files.

> **Common Pitfall:** Setting `memory.max` without setting `memory.swap.max`. If swap is available and memory.max is hit, the kernel moves pages to swap instead of triggering an OOM kill. For an inference container, this causes catastrophic latency spikes (swap I/O is orders of magnitude slower than RAM). Always set both `memory.max` and `memory.swap.max` to the same value (or set `memory.swap.max` to 0 to disable swap entirely for the cgroup).

---

**Container Runtimes**

The cgroup and namespace mechanics are orchestrated by the container runtime.

| Runtime | Role | Notes |
|---|---|---|
| `containerd` | High-level; manages images and snapshots | Default for Kubernetes (CRI) |
| `runc` | Low-level OCI runtime; creates namespaces and cgroups | Used by containerd |
| `crun` | C reimplementation of runc | 2–5× faster startup; lower memory |
| `kata-containers` | Lightweight VM per container | Stronger isolation; used for multi-tenant GPU |

The full container launch sequence:

1. **containerd** receives the OCI container spec from Kubernetes (or Docker).
2. **containerd** prepares the overlayfs rootfs from the image layers (using the snapshotter).
3. **containerd** forks `runc` with the OCI spec as input.
4. **runc** calls `clone()` with `CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWNS` to create the new namespaces.
5. **runc** sets up the overlayfs mount and calls `pivot_root()` to switch the filesystem root.
6. **runc** writes resource limits to the cgroup files (memory.max, cpu.max, cpuset.cpus).
7. **runc** executes the container entrypoint (e.g., `python inference_server.py`).

`containerd` → `runc` → `clone()` with `CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWNS` → set up overlayfs rootfs → `pivot_root` → write cgroup limits → exec container entrypoint.

> **Key Insight:** The container's process has been running in its new namespace since step 4, but it can only see its filesystem after `pivot_root` in step 5. The cgroup limits are written in step 6 before the application code starts in step 7. This ordering guarantees that by the time the application runs, all isolation and resource limits are already in place.

---

</details>

## NVIDIA Container Runtime

从容器内部访问 GPU 需要特殊处理，因为 **CUDA 库与设备节点必须与宿主驱动版本匹配**。NVIDIA Container Runtime 优雅地解决了这个问题。

`nvidia-container-toolkit` 是一个 **OCI runtime hook**，在容器进程启动之前运行。它会：

1. **读取 `NVIDIA_VISIBLE_DEVICES` 环境变量**（或 `--gpus` 标志）：确定向该容器暴露哪些物理 GPU 或 MIG 实例。
2. **把选中的 `/dev/nvidia*` 设备节点挂载**到容器的 mount namespace 中：`/dev/nvidia0`、`/dev/nvidiactl`、`/dev/nvidia-uvm`。此时容器看到的这些设备就如同本地设备。
3. **注入 CUDA 库**（`libcuda.so.X`、`libnvrtc.so`、`libnvidia-ml.so`）：从宿主注入到容器中的固定路径（`/usr/local/lib/...`，或通过 `ldconfig` 条目）。注入的库与宿主驱动版本完全一致。
4. **配置 `/proc/driver/nvidia` 与 capability 文件**：`libcuda.so` 查询 GPU 能力时需要这些文件。

容器镜像 **无需包含 CUDA**；只打包 CUDA 应用代码。宿主驱动版本按原样暴露。这样同一个容器镜像就能运行在 **不同驱动版本** 的宿主上，只要该驱动与所需的 CUDA toolkit 版本兼容。

```bash
# Run any CUDA application without CUDA installed in the image
docker run --gpus all --rm nvidia/cuda:12.2-base nvidia-smi

# Run TensorRT inference server with specific GPU and capabilities
docker run --gpus "device=0" -e NVIDIA_DRIVER_CAPABILITIES=compute,utility \
    my-trt-inference-server
```

`NVIDIA_DRIVER_CAPABILITIES` 变量控制注入哪些库集合。`compute` 注入 CUDA 计算库；`utility` 注入 `nvidia-smi` 工具；`video` 注入 NVENC/NVDEC 视频编解码库。

> **常见陷阱：** 运行使用 NVENC 或 NVDEC 做硬件视频编解码的容器时，忘记设置 `NVIDIA_DRIVER_CAPABILITIES=video`。容器会正常启动，CUDA 计算也能工作，但任何对视频编解码 API 的调用都会以 "driver not loaded" 错误失败。视频编解码库属于独立的注入集合，不包含在默认的 `compute,utility` 能力中。

---

## Multi-Instance GPU (MIG)

MIG 在 A100、H100 和 Jetson Orin 上可用。它把一块物理 GPU 划分为最多 **7 个独立的 GPU Instances (GIs)**，每个都拥有专用的：

- 计算资源（SM 分区）
- 内存（带专用带宽的 HBM 切片）
- 片上缓存与解码器

对 CUDA 而言，每个 GI 都表现为一块 **独立的 GPU**。**不存在分时共享**；各 GI 真正并行运行。

```
A100 80GB with MIG enabled:
┌─────────────────────────────────────────────────────────┐
│                     A100 Physical GPU                    │
│  ┌────────────┐  ┌────────────┐  ┌────────────────────┐ │
│  │   GI 0     │  │   GI 1     │  │       GI 2         │ │
│  │ 3g.40gb    │  │ 1g.10gb    │  │    1g.10gb         │ │
│  │ 42 SMs     │  │ 14 SMs     │  │    14 SMs          │ │
│  │ 40 GB HBM  │  │ 10 GB HBM  │  │    10 GB HBM       │ │
│  │            │  │            │  │                    │ │
│  │ TRT Model  │  │ TRT Model  │  │    TRT Model       │ │
│  │ (large)    │  │ (small)    │  │    (small)         │ │
│  └────────────┘  └────────────┘  └────────────────────┘ │
│     Truly parallel — no time sharing between GIs         │
└─────────────────────────────────────────────────────────┘
```

```bash
nvidia-smi mig -lgip                        # list GPU instance profiles
nvidia-smi mig -cgi 3g.40gb,1g.10gb -C     # create instances
# Container gets one GI via: --gpus "MIG-GPU-<uuid>"
```

在 Jetson Orin 上，MIG 支持在相互隔离的 GPU 分区上运行多个推理模型（感知、occupancy、预测），并获得有保证的内存带宽。

> **关键洞察：** 对生产级推理服务而言，MIG 的关键特性是 **带宽隔离**，而不只是计算隔离。HBM 内存带宽按 GI 专用 —— GI 1 中噪声大的模型无法抢走 GI 0 的内存带宽。正因如此，才可能在多模型推理服务系统中为单个模型保证 SLA 延迟目标。没有 MIG 时，所有模型争用同一条 HBM 总线，一个大批次就可能让其他所有模型出现延迟尖峰。

---


<details>
<summary>English original</summary>

**NVIDIA Container Runtime**

GPU access from inside a container requires special handling because **CUDA libraries and device nodes must be version-matched** to the host driver. The NVIDIA Container Runtime solves this elegantly.

`nvidia-container-toolkit` is an **OCI runtime hook** that runs before the container process starts. It:

1. **Reads `NVIDIA_VISIBLE_DEVICES` env var** (or `--gpus` flag): determines which physical GPUs or MIG instances to expose to this container.
2. **Mounts the selected `/dev/nvidia*` device nodes** into the container's mount namespace: `/dev/nvidia0`, `/dev/nvidiactl`, `/dev/nvidia-uvm`. The container now sees these devices as if they were local.
3. **Injects CUDA libraries** (`libcuda.so.X`, `libnvrtc.so`, `libnvidia-ml.so`) from the host into the container at well-known paths (`/usr/local/lib/...` or via `ldconfig` entries). The injected libraries match the host driver version exactly.
4. **Sets up `/proc/driver/nvidia` and capability files**: these are required by `libcuda.so` to query GPU capabilities.

The container image **does not need to contain CUDA**; only the CUDA application code is packaged. Host driver version is exposed as-is. This allows a single container image to run on hosts with **different driver versions**, as long as the driver is compatible with the required CUDA toolkit version.

```bash
# Run any CUDA application without CUDA installed in the image
docker run --gpus all --rm nvidia/cuda:12.2-base nvidia-smi

# Run TensorRT inference server with specific GPU and capabilities
docker run --gpus "device=0" -e NVIDIA_DRIVER_CAPABILITIES=compute,utility \
    my-trt-inference-server
```

The `NVIDIA_DRIVER_CAPABILITIES` variable controls which library sets are injected. `compute` injects CUDA compute libraries; `utility` injects `nvidia-smi` tools; `video` injects the NVENC/NVDEC video codec libraries.

> **Common Pitfall:** Forgetting to set `NVIDIA_DRIVER_CAPABILITIES=video` when running a container that uses NVENC or NVDEC for hardware video encoding/decoding. The container will launch successfully and CUDA compute will work, but any call to the video codec API will fail with a "driver not loaded" error. The video codec libraries are a separate injection set and are not included in the default `compute,utility` capabilities.

---

**Multi-Instance GPU (MIG)**

MIG is available on A100, H100, and Jetson Orin. It partitions one physical GPU into up to **7 independent GPU Instances (GIs)**, each with dedicated:

- Compute (SM partition)
- Memory (HBM slice with dedicated bandwidth)
- On-chip caches and decoders

Each GI appears as an **independent GPU** to CUDA. There is **no time-sharing**; GIs run truly in parallel.

```
A100 80GB with MIG enabled:
┌─────────────────────────────────────────────────────────┐
│                     A100 Physical GPU                    │
│  ┌────────────┐  ┌────────────┐  ┌────────────────────┐ │
│  │   GI 0     │  │   GI 1     │  │       GI 2         │ │
│  │ 3g.40gb    │  │ 1g.10gb    │  │    1g.10gb         │ │
│  │ 42 SMs     │  │ 14 SMs     │  │    14 SMs          │ │
│  │ 40 GB HBM  │  │ 10 GB HBM  │  │    10 GB HBM       │ │
│  │            │  │            │  │                    │ │
│  │ TRT Model  │  │ TRT Model  │  │    TRT Model       │ │
│  │ (large)    │  │ (small)    │  │    (small)         │ │
│  └────────────┘  └────────────┘  └────────────────────┘ │
│     Truly parallel — no time sharing between GIs         │
└─────────────────────────────────────────────────────────┘
```

```bash
nvidia-smi mig -lgip                        # list GPU instance profiles
nvidia-smi mig -cgi 3g.40gb,1g.10gb -C     # create instances
# Container gets one GI via: --gpus "MIG-GPU-<uuid>"
```

On Jetson Orin, MIG enables running multiple inference models (perception, occupancy, prediction) on isolated GPU partitions with guaranteed memory bandwidth.

> **Key Insight:** MIG's critical property for production inference serving is **bandwidth isolation**, not just compute isolation. HBM memory bandwidth is dedicated per GI — a noisy model in GI 1 cannot steal memory bandwidth from GI 0. This is what makes it possible to guarantee SLA latency targets for individual models in a multi-model serving system. Without MIG, all models compete for the same HBM bus, and one large batch can cause latency spikes for all other models.

---

</details>

## 不使用 MIG 的 GPU 共享

当 MIG 硬件不可用时（较老的 GPU、边缘设备），CUDA MPS 提供了一种软件级的共享机制。

CUDA MPS（Multi-Process Service）允许**多个 CUDA 进程共享一块 GPU**，方式是经由单个 MPS server 进程：

- 进程通过 MPS server 提交工作；由它对提交串行化并批处理
- 相比不使用 MPS 的分时共享，降低了上下文切换开销
- 客户端之间没有内存隔离（与 MIG 相对）
- 适用场景：推理服务集群中多个轻量推理进程共享一块 GPU

```bash
nvidia-cuda-mps-control -d    # start MPS daemon
export CUDA_MPS_PIPE_DIRECTORY=/tmp/nvidia-mps
# All CUDA processes launched after this will share via MPS
```

> **常见陷阱：** 在多租户环境中使用 CUDA MPS，让来自不同用户（或不同信任级别）的容器共享同一块 GPU。MPS 不提供内存隔离——某个客户端进程的 bug 可以读取或破坏另一个客户端的 GPU 内存。MPS 适用于同一信任级别的进程（例如同一推理服务的多个 worker 线程）。若要多租户隔离，MIG 才是正确的机制。

---

## 用于 AI 推理的 Kubernetes

Kubernetes 在一组 GPU 节点构成的集群上编排容器化的推理。

### NVIDIA Device Plugin

device plugin 在每个 GPU 节点上以 DaemonSet 方式运行。它：

- 调用 `nvidia-smi` 枚举 GPU 与 MIG 实例
- 把 `nvidia.com/gpu` 作为 Extended Resource 注册到 kubelet
- 把特定的设备节点分配给调度到该节点上的 pod

### Pod 规格

```yaml
resources:
  limits:
    nvidia.com/gpu: "1"     # request 1 GPU (or 1 MIG instance)
    cpu: "4"                # 4 CPU cores
    memory: "16Gi"          # 16 GB RAM
```

### 关键组件

- **Node Feature Discovery (NFD)**：为节点打上 GPU 型号、CUDA 版本、驱动版本的标签；供调度器做 GPU 类型感知的布局
- **Triton Inference Server**：容器化；动态批处理；后端：TensorRT、ONNX Runtime、PyTorch；暴露 gRPC + HTTP 端点
- **DCGM Exporter**：把 GPU 指标（利用率、显存、温度、NVLink 带宽）导出到 Prometheus

Kubernetes 中完整的生产推理栈：

```
Kubernetes Scheduler
        ↓ schedules pod to GPU node
Node (DaemonSet: NVIDIA Device Plugin)
        ↓ allocates /dev/nvidia0 to pod
containerd + NVIDIA Container Runtime
        ↓ sets up namespaces, cgroups, injects CUDA libs
Pod: Triton Inference Server container
        ↓ loads TensorRT engine
CUDA / TensorRT → /dev/nvidia0 → A100 GPU
        ↓
Model output → gRPC response to client
```

> **关键洞察：** DCGM Exporter 把 GPU 指标送入 Prometheus，再由 Prometheus 供给 Grafana 仪表盘与自动扩缩容规则。在生产推理集群中，扩缩容决策（增加副本）由 DCGM 导出的 GPU 利用率与请求队列深度指标触发。由此形成闭环：Kubernetes 提供资源隔离（cgroups、namespace），device plugin 提供 GPU 访问，Triton 提供高效批处理，DCGM 提供可观测性——生产级推理服务四者缺一不可。

---

## 小结

| 隔离机制 | 内核特性 | 资源类型 | 主要工具 |
|---|---|---|---|
| 进程可见性 | PID namespace | 进程树 | `unshare`, `nsenter` |
| 网络隔离 | Network namespace | 网络接口、端口 | `ip netns`, `veth` |
| 文件系统隔离 | Mount namespace + overlayfs | rootfs | `pivot_root`, `containerd` |
| CPU 限制 | cgroups v2 `cpu` controller | CPU 时间 | `cpu.max`, kubelet |
| 内存限制 | cgroups v2 `memory` controller | RAM + swap | `memory.max` |
| CPU 绑核 | cgroups v2 `cpuset` controller | CPU 核 | `cpuset.cpus` |
| GPU 访问 | NVIDIA Container Toolkit | 设备节点 + 库 | `--gpus`, device plugin |
| GPU 划分 | MIG（硬件） | SM + HBM 切片 | `nvidia-smi mig` |


<details>
<summary>English original</summary>

**GPU Sharing Without MIG**

When MIG hardware is not available (older GPUs, edge devices), CUDA MPS provides a software-level sharing mechanism.

CUDA MPS (Multi-Process Service) allows **multiple CUDA processes to share a GPU** through a single MPS server process:

- Processes submit work via the MPS server; it serializes and batches submissions
- Reduces context switch overhead compared to time-sharing without MPS
- No memory isolation between clients (contrast with MIG)
- Use case: many lightweight inference processes sharing one GPU in a serving cluster

```bash
nvidia-cuda-mps-control -d    # start MPS daemon
export CUDA_MPS_PIPE_DIRECTORY=/tmp/nvidia-mps
# All CUDA processes launched after this will share via MPS
```

> **Common Pitfall:** Using CUDA MPS in a multi-tenant environment where containers from different users (or different trust levels) share the same GPU. MPS does not provide memory isolation — a bug in one client process can read or corrupt another client's GPU memory. MPS is appropriate for same-trust processes (e.g., multiple worker threads of the same inference service). For multi-tenant isolation, MIG is the correct mechanism.

---

**Kubernetes for AI Inference**

Kubernetes orchestrates containerized inference across a cluster of GPU nodes.

**NVIDIA Device Plugin**

The device plugin runs as a DaemonSet on each GPU node. It:

- Calls `nvidia-smi` to enumerate GPUs and MIG instances
- Registers `nvidia.com/gpu` as an Extended Resource with the kubelet
- Allocates specific device nodes to pods scheduled on the node

**Pod Specification**

```yaml
resources:
  limits:
    nvidia.com/gpu: "1"     # request 1 GPU (or 1 MIG instance)
    cpu: "4"                # 4 CPU cores
    memory: "16Gi"          # 16 GB RAM
```

**Key Components**

- **Node Feature Discovery (NFD)**: labels nodes with GPU model, CUDA version, driver version; used by scheduler for GPU-type-aware placement
- **Triton Inference Server**: containerized; dynamic batching; backends: TensorRT, ONNX Runtime, PyTorch; exposes gRPC + HTTP endpoints
- **DCGM Exporter**: exports GPU metrics (utilization, memory, temperature, NVLink bandwidth) to Prometheus

The complete production inference stack in Kubernetes:

```
Kubernetes Scheduler
        ↓ schedules pod to GPU node
Node (DaemonSet: NVIDIA Device Plugin)
        ↓ allocates /dev/nvidia0 to pod
containerd + NVIDIA Container Runtime
        ↓ sets up namespaces, cgroups, injects CUDA libs
Pod: Triton Inference Server container
        ↓ loads TensorRT engine
CUDA / TensorRT → /dev/nvidia0 → A100 GPU
        ↓
Model output → gRPC response to client
```

> **Key Insight:** The DCGM Exporter feeds GPU metrics into Prometheus, which feeds Grafana dashboards and autoscaler rules. In a production inference cluster, autoscaling decisions (spin up more replicas) are triggered by GPU utilization and request queue depth metrics exported by DCGM. This closes the loop: Kubernetes provides resource isolation (cgroups, namespaces), the device plugin provides GPU access, Triton provides efficient batching, and DCGM provides observability — all four are required for a production-grade inference service.

---

**Summary**

| Isolation mechanism | Kernel feature | Resource type | Primary tools |
|---|---|---|---|
| Process visibility | PID namespace | Process tree | `unshare`, `nsenter` |
| Network isolation | Network namespace | Interfaces, ports | `ip netns`, `veth` |
| Filesystem isolation | Mount namespace + overlayfs | Rootfs | `pivot_root`, `containerd` |
| CPU limits | cgroups v2 `cpu` controller | CPU time | `cpu.max`, kubelet |
| Memory limits | cgroups v2 `memory` controller | RAM + swap | `memory.max` |
| CPU pinning | cgroups v2 `cpuset` controller | CPU cores | `cpuset.cpus` |
| GPU access | NVIDIA Container Toolkit | Device nodes + libraries | `--gpus`, device plugin |
| GPU partitioning | MIG (hardware) | SM + HBM slices | `nvidia-smi mig` |

</details>

### 概念回顾

- **为什么容器能取得接近裸金属的 GPU 性能，而 VM 却需要 GPU 虚拟化？** 容器对 GPU 的访问，就是把宿主机的 `/dev/nvidia*` 设备节点挂载进容器的文件系统命名空间。容器内的 CUDA 调用与原生应用走同一个 `nvidia.ko` 内核驱动。没有 hypervisor 层，没有 VGPU 转换，没有半虚拟化。GPU 硬件直接执行 kernel。VM 则需要 vGPU 驱动，把 guest 的 GPU 命令经 hypervisor 层转换，从而带来额外开销。

- **命名空间隔离与 cgroup 资源控制有什么区别？** 命名空间控制进程能看到什么：有了 PID 命名空间，容器看不到宿主机进程；有了 network 命名空间，容器看不到宿主机网络接口。但没有任何机制阻止容器耗尽全部可用 CPU 周期或 RAM —— 命名空间管的是可见性，不是资源消耗。cgroup 补上了资源这一维：`cpu.max` 限制 CPU 时间，`memory.max` 限制 RAM 用量。命名空间 + cgroup = 完整的容器隔离。

- **为什么 NVIDIA Container Runtime 从宿主机注入库，而不是把它们打包进容器镜像？** CUDA 库必须与内核驱动版本精确匹配（`libcuda.so.X` 链接到 `nvidia.ko` 内部 ABI）。如果容器自带 CUDA 库，那么宿主机驱动每次更新都得重新构建它们。改为在 runtime 从宿主机注入后，同一个容器镜像可以在任何具备兼容驱动版本的宿主机上运行。这也大幅减小了镜像体积（CUDA 库有好几 GB）。

- **面向多模型推理，MIG 与 CUDA MPS 的关键运维差异是什么？** MIG 是硬件划分：每个 GPU Instance 拥有专属的 SM、专属的 HBM 带宽和专属的缓存。GI 之间的干扰在物理上不可能发生。MPS 是软件多路复用：MPS server 把多个进程的 CUDA 命令串行化，减少了上下文切换开销，但不提供内存隔离 —— 进程之间会互相干扰对方的 GPU 内存。MIG 提供有保证的性能隔离；MPS 只提供调度效率。

- **Kubernetes device plugin 如何与 NVIDIA Container Runtime 协同完成 GPU 分配？** device plugin 向 kubelet 通告可用的 GPU 资源（`nvidia.com/gpu`）。当请求 GPU 的 pod 被调度到该节点时，kubelet 向 device plugin 查询某个具体设备（例如 `/dev/nvidia0`）。device plugin 返回设备路径以及所需的环境变量（例如 `NVIDIA_VISIBLE_DEVICES=0`）。kubelet 把这些传给容器 runtime，后者（通过 NVIDIA Container Runtime hook）使用 `NVIDIA_VISIBLE_DEVICES` 判定要注入哪些设备节点和库。

- **`cpuset.cpus.exclusive` 提供了哪些仅靠 `cpuset.cpus` 无法提供的能力？** `cpuset.cpus` 把 cgroup 限制在特定核心上运行，但其他 cgroup 也可以在这些核心上运行。`cpuset.cpus.exclusive` 把这些核心独占性地分配给该 cgroup —— 其他 cgroup 都不能调度到这些核心上。Kubernetes Static CPU Manager 正是用它把核心完整地专用于对延迟敏感的推理容器，消除其他工作负载共用同一批核心所带来的 OS 调度抖动。

---

## AI 硬件关联

- NVIDIA Container Runtime 让 TensorRT 和 Triton 推理容器以原生性能访问 GPU 硬件；宿主驱动在容器启动时注入，无需按驱动版本重建镜像
- A100/H100/Orin 上的 MIG 把 GPU 划分给多租户推理服务；每个模型获得有保证的内存带宽份额，避免某个工作负载饿死另一个
- Kubernetes static CPU manager 中的 cgroups v2 `cpuset.cpus.exclusive` 把推理 worker 线程绑定到专属核心，消除 OS 调度器干扰并降低尾延迟
- rootless 容器（user 命名空间 UID 重映射）是边缘 AI 部署的正确安全姿态：容器用户绝不能映射到宿主机上的特权账户
- CUDA MPS 支撑高并发推理服务场景：大量轻量推理进程（例如每传感器一个模型实例）共享一块 GPU，而无需承担完整上下文切换的开销
- 整套技术栈 —— cgroups v2 资源隔离 + NVIDIA device plugin + Triton 动态批处理 —— 是 Kubernetes 中可扩展 GPU 推理的生产参考架构


<details>
<summary>English original</summary>

**Conceptual Review**

- **Why do containers achieve near-bare-metal GPU performance when VMs require GPU virtualization?** A container's GPU access is the host `/dev/nvidia*` device node mounted into the container's filesystem namespace. The CUDA calls inside the container execute through the same `nvidia.ko` kernel driver as a native application. There is no hypervisor layer, no VGPU translation, no para-virtualization. The GPU hardware executes the kernels directly. VMs require a vGPU driver that translates guest GPU commands through a hypervisor layer, adding overhead.

- **What is the difference between namespace isolation and cgroup resource control?** Namespaces control what a process can see: with a PID namespace, the container cannot see host processes; with a network namespace, it cannot see host network interfaces. But nothing stops the container from consuming all available CPU cycles or RAM — namespaces are about visibility, not resource consumption. cgroups add the resource dimension: `cpu.max` caps CPU time, `memory.max` caps RAM usage. Together, namespaces + cgroups = complete container isolation.

- **Why does the NVIDIA Container Runtime inject libraries from the host rather than packaging them in the container image?** CUDA libraries must exactly match the kernel driver version (`libcuda.so.X` links against `nvidia.ko` internal ABI). If the container bundled its own CUDA libraries, they would need to be rebuilt every time the host driver is updated. By injecting from the host at runtime, a single container image works across any host with a compatible driver version. This also dramatically reduces image size (CUDA libraries are several GB).

- **What is the key operational difference between MIG and CUDA MPS for multi-model inference?** MIG is hardware partitioning: each GPU Instance has dedicated SMs, dedicated HBM bandwidth, and dedicated caches. Interference between GIs is physically impossible. MPS is software multiplexing: the MPS server serializes CUDA commands from multiple processes, reducing context-switch overhead but providing no memory isolation — processes can interfere with each other's GPU memory. MIG provides guaranteed performance isolation; MPS provides only scheduling efficiency.

- **How does the Kubernetes device plugin coordinate GPU allocation with the NVIDIA Container Runtime?** The device plugin advertises available GPU resources (`nvidia.com/gpu`) to the kubelet. When a pod requesting a GPU is scheduled to the node, the kubelet queries the device plugin for a specific device (e.g., `/dev/nvidia0`). The device plugin returns the device path and any required environment variables (e.g., `NVIDIA_VISIBLE_DEVICES=0`). The kubelet passes these to the container runtime, which (via the NVIDIA Container Runtime hook) uses `NVIDIA_VISIBLE_DEVICES` to determine which device nodes and libraries to inject.

- **What does `cpuset.cpus.exclusive` provide that `cpuset.cpus` alone does not?** `cpuset.cpus` restricts a cgroup to running on specific cores, but other cgroups can also run on those same cores. `cpuset.cpus.exclusive` claims those cores exclusively for this cgroup — no other cgroup can be scheduled on them. This is what Kubernetes Static CPU Manager uses to fully dedicate cores to latency-sensitive inference containers, eliminating OS scheduling jitter from other workloads sharing the same cores.

---

**AI Hardware Connection**

- NVIDIA Container Runtime enables TensorRT and Triton inference containers to access GPU hardware at native performance; the host driver is injected at container start, eliminating the need to rebuild images per driver version
- MIG on A100/H100/Orin partitions the GPU for multi-tenant inference serving; each model gets a guaranteed memory bandwidth slice, preventing one workload from starving another
- cgroups v2 `cpuset.cpus.exclusive` in Kubernetes static CPU manager pins inference worker threads to dedicated cores, eliminating OS scheduler interference and reducing tail latency
- Rootless containers (user namespace UID remapping) are the correct security posture for edge AI deployments where the container user must not map to a privileged host account
- CUDA MPS enables high-concurrency serving scenarios where many lightweight inference processes (e.g., per-sensor model instances) share one GPU without the overhead of full context switches
- The full stack — cgroups v2 resource isolation + NVIDIA device plugin + Triton dynamic batching — is the production reference architecture for scalable GPU inference in Kubernetes

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-23.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-23.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
