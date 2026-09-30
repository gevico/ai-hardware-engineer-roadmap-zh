---
title: Jetson 硬件抽象层
description: Jetson 硬件抽象层
---

# Jetson 硬件抽象层

所有硬件查询都直接读取 sysfs/procfs —— 不依赖 NVIDIA SDK。

## 电源管理

### 来源：`src/jetson/power.cpp`

### 电源模式

| 模式 | 瓦特 | GPU 最高 MHz | 推荐用途 |
|------|-------|-------------|-----------------|
| MAXN (0) | 25W | 1300 | 最高性能，需要主动散热 |
| 15W (1) | 15W | 900 | 均衡，小型散热器 |
| 10W (2) | 10W | 600 | 低功耗，可无风扇 |
| 7W (3) | 7W | 400 | 最低，电池供电运行 |

### 读取电源状态

```cpp
PowerState ps = read_power_state();
// ps.mode:            POWER_MAXN / POWER_15W / POWER_10W / POWER_7W
// ps.watts:           25 / 15 / 10 / 7
// ps.gpu_freq_mhz:    current GPU frequency
// ps.emc_freq_mhz:    memory controller frequency
// ps.cpu_freq_mhz:    max CPU frequency
// ps.cpu_online:      number of online CPU cores
```

### sysfs 路径

| 项目 | 路径 |
|------|------|
| GPU 当前频率 | `/sys/devices/17000000.ga10b/devfreq/17000000.ga10b/cur_freq` |
| GPU 最高频率 | `/sys/devices/17000000.ga10b/devfreq/17000000.ga10b/max_freq` |
| EMC（内存）频率 | `/sys/kernel/debug/bpmp/debug/clk/emc/rate` |
| CPU 频率 | `/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq` |
| CPU 在线 | `/sys/devices/system/cpu/cpuN/online` |
| 电源模式 | `nvpmodel -q` (popen) |

### 设置电源模式

```cpp
set_power_mode(POWER_MAXN);  // calls: nvpmodel -m 0
lock_clocks();                // calls: jetson_clocks
```

## 热管理

### 来源：`src/jetson/thermal.cpp`

### 读取温度

```cpp
ThermalState ts = read_thermal();
// ts.cpu_temp_c:   CPU temperature (°C)
// ts.gpu_temp_c:   GPU temperature (°C)
// ts.board_temp_c: Board temperature (°C)
// ts.throttling:   true if any zone > 85°C
```

从 `/sys/devices/virtual/thermal/thermal_zone*/temp` 读取，并匹配 zone 类型名（"CPU-therm"、"GPU-therm"、"Tboard_tegra"）。

### 自适应退避

```cpp
int delay_us = thermal_backoff_us(ts);
```

| 温度 | 退避 | 影响 |
|------------|---------|--------|
| < 80°C | 0 | 全速 |
| 80–85°C | 10 ms | 预降频 —— 轻微降速 |
| 85–90°C | 50 ms | 降频区 —— 明显降速 |
| 90–95°C | 100 ms | 临界 —— 显著降速 |
| > 95°C | 200 ms | 紧急 —— 接近关机 |

在 decode（逐 token 生成阶段）循环中每 10 个 token 调用一次（不是每个 token —— sysfs 读取很慢，每次约 100μs）。

## 系统信息

### 来源：`src/jetson/sysinfo.cpp`

### 一次性探测

```cpp
JetsonInfo info = probe_jetson();
print_jetson_info(info);
```

输出：
```
╔══════════════════════════════════════╗
║   Jetson LLM Runtime v0.1            ║
╠══════════════════════════════════════╣
║ L4T:    36.4       CUDA: 12.6       ║
║ SMs:    16          Cores: 1024      ║
║ RAM:    7633  MB    CMA: 768  MB    ║
║ NVMe:   42000 MB free               ║
╚══════════════════════════════════════╝
```

读取：
- `/etc/nv_tegra_release` → L4T 版本
- `cudaRuntimeGetVersion()` → CUDA 版本
- `cudaGetDeviceProperties()` → SM 数量、计算能力
- `/proc/meminfo` → RAM、CMA
- `df` 命令 → NVMe 可用空间

### 实时统计

```cpp
LiveStats s = read_live_stats();
print_live_stats(s);
```

输出（单行，用回车符原地刷新）：
```
[RAM 3200/7633 MB | GPU 75% @ 1300 MHz | 52.3°C | 25.4 tok/s]
```

读取：
- `/proc/meminfo` → RAM 已用/总量
- `/sys/devices/17000000.ga10b/devfreq/17000000.ga10b/load` → GPU 利用率 %
- `/sys/devices/17000000.ga10b/devfreq/17000000.ga10b/cur_freq` → GPU MHz
- thermal zone → GPU 温度
- `tokens_per_sec` 由引擎设置


<details>
<summary>English original</summary>

**Jetson Hardware Abstraction Layer**

All hardware queries read directly from sysfs/procfs — no NVIDIA SDK dependency.

**Power Management**

**Source: `src/jetson/power.cpp`**

**Power Modes**

| Mode | Watts | GPU Max MHz | Recommended use |
|------|-------|-------------|-----------------|
| MAXN (0) | 25W | 1300 | Maximum performance, active cooling required |
| 15W (1) | 15W | 900 | Balanced, small heatsink |
| 10W (2) | 10W | 600 | Low power, fanless possible |
| 7W (3) | 7W | 400 | Minimum, battery operation |

**Reading Power State**

```cpp
PowerState ps = read_power_state();
// ps.mode:            POWER_MAXN / POWER_15W / POWER_10W / POWER_7W
// ps.watts:           25 / 15 / 10 / 7
// ps.gpu_freq_mhz:    current GPU frequency
// ps.emc_freq_mhz:    memory controller frequency
// ps.cpu_freq_mhz:    max CPU frequency
// ps.cpu_online:      number of online CPU cores
```

**sysfs Paths**

| What | Path |
|------|------|
| GPU current frequency | `/sys/devices/17000000.ga10b/devfreq/17000000.ga10b/cur_freq` |
| GPU max frequency | `/sys/devices/17000000.ga10b/devfreq/17000000.ga10b/max_freq` |
| EMC (memory) frequency | `/sys/kernel/debug/bpmp/debug/clk/emc/rate` |
| CPU frequency | `/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq` |
| CPU online | `/sys/devices/system/cpu/cpuN/online` |
| Power mode | `nvpmodel -q` (popen) |

**Setting Power Mode**

```cpp
set_power_mode(POWER_MAXN);  // calls: nvpmodel -m 0
lock_clocks();                // calls: jetson_clocks
```

**Thermal Management**

**Source: `src/jetson/thermal.cpp`**

**Reading Temperature**

```cpp
ThermalState ts = read_thermal();
// ts.cpu_temp_c:   CPU temperature (°C)
// ts.gpu_temp_c:   GPU temperature (°C)
// ts.board_temp_c: Board temperature (°C)
// ts.throttling:   true if any zone > 85°C
```

Reads from `/sys/devices/virtual/thermal/thermal_zone*/temp` and matches zone type names ("CPU-therm", "GPU-therm", "Tboard_tegra").

**Adaptive Backoff**

```cpp
int delay_us = thermal_backoff_us(ts);
```

| Temperature | Backoff | Effect |
|------------|---------|--------|
| < 80°C | 0 | Full speed |
| 80–85°C | 10 ms | Pre-throttle — slight slowdown |
| 85–90°C | 50 ms | Throttle zone — noticeable slowdown |
| 90–95°C | 100 ms | Critical — significant slowdown |
| > 95°C | 200 ms | Emergency — near shutdown |

Called every 10 tokens in the decode loop (not every token — sysfs reads are slow, ~100μs each).

**System Info**

**Source: `src/jetson/sysinfo.cpp`**

**One-Time Probe**

```cpp
JetsonInfo info = probe_jetson();
print_jetson_info(info);
```

Output:
```
╔══════════════════════════════════════╗
║   Jetson LLM Runtime v0.1            ║
╠══════════════════════════════════════╣
║ L4T:    36.4       CUDA: 12.6       ║
║ SMs:    16          Cores: 1024      ║
║ RAM:    7633  MB    CMA: 768  MB    ║
║ NVMe:   42000 MB free               ║
╚══════════════════════════════════════╝
```

Reads:
- `/etc/nv_tegra_release` → L4T version
- `cudaRuntimeGetVersion()` → CUDA version
- `cudaGetDeviceProperties()` → SM count, compute capability
- `/proc/meminfo` → RAM, CMA
- `df` command → NVMe free space

**Live Stats**

```cpp
LiveStats s = read_live_stats();
print_live_stats(s);
```

Output (single-line, carriage return for in-place update):
```
[RAM 3200/7633 MB | GPU 75% @ 1300 MHz | 52.3°C | 25.4 tok/s]
```

Reads:
- `/proc/meminfo` → RAM used/total
- `/sys/devices/17000000.ga10b/devfreq/17000000.ga10b/load` → GPU utilization %
- `/sys/devices/17000000.ga10b/devfreq/17000000.ga10b/cur_freq` → GPU MHz
- Thermal zones → GPU temperature
- `tokens_per_sec` set by engine

</details>

---

> 原文：[`Projects/jetson-llm-runtime/docs/jetson-hal.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Projects/jetson-llm-runtime/docs/jetson-hal.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
