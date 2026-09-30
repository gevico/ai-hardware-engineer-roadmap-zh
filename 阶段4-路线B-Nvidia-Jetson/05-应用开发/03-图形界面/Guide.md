---
title: GUI
description: GUI
---

# GUI

<div class="course-identity auto-course" style="--course-accent: #16a34a; --course-accent-rgb: 22, 163, 74;" markdown="1">
<div class="course-identity__icon">GUI</div>
<div markdown="1">
<p class="course-identity__eyebrow">深入解析 · Jetson Track</p>
<p class="course-identity__title">GUI 的专门课程标识。</p>
<p class="course-identity__meta">产物：Jetson 集成演示 · 度量：延迟、内存、功耗、日志</p>
</div>
</div>


**阶段 4 — 方向 B — 模块 5.3** · 应用开发

> **重点：** 为 **Jetson Orin Nano 8GB** 产品构建图形用户界面 —— 从轻量级嵌入式 UI（LVGL、framebuffer）到 Qt 桌面应用，再到基于 Web 的仪表盘。涵盖显示设置、触摸输入与 GPU 加速渲染。

**Hub：** [5. 应用开发](../Guide.md)

---


## 1. Jetson 上的显示设置

### 输出接口

| 接口 | 用途 | 说明 |
|-----------|----------|-------|
| **HDMI** | 开发、桌面显示 | Orin Nano 上最高 4K@60 |
| **DisplayPort** | 高分辨率显示器 | 部分载板上经由 USB-C 的 Alt-mode |
| **eDP** | 嵌入式 LCD 面板 | 需要载板支持 |
| **LVDS**（经由桥接） | 工业面板 | 需要 HDMI/DP-to-LVDS 桥接 IC |
| **DSI** | 小型 MIPI 显示屏 | Orin Nano 上支持有限 |

### 分辨率与时序

```bash
# List connected displays and modes
xrandr

# Set resolution
xrandr --output HDMI-0 --mode 1920x1080 --rate 60

# For headless (virtual framebuffer)
export DISPLAY=:0
Xvfb :0 -screen 0 1920x1080x24 &
```

---

## 2. Framebuffer（Linux）

直接访问 framebuffer，用于无需显示服务器的简单图形。

```bash
# Check framebuffer device
ls /dev/fb*
cat /sys/class/graphics/fb0/virtual_size

# Write solid color to screen (red)
dd if=/dev/zero bs=4 count=$((1920*1080)) | \
    tr '\0' '\xff\x00\x00\xff' > /dev/fb0

# Display an image (using fbi)
sudo apt install fbi
sudo fbi -T 1 -d /dev/fb0 image.png
```

### 何时使用 framebuffer

- 启动期间的**开机画面**（在 X/Wayland 启动之前）
- 小屏幕上的**极简状态显示**（无需窗口管理器）
- 单全屏应用的 **Kiosk 模式**

---

## 3. X11 与 Wayland

JetPack 默认随附 X11（Xorg）。Wayland（经由 Weston）可用，但在 Jetson 上测试较少。

```bash
# Check current display server
echo $XDG_SESSION_TYPE

# Start X11 manually (headless or remote)
startx

# For Weston (Wayland)
sudo apt install weston
weston-launch
```

### GPU 加速

NVIDIA 的 L4T 驱动为 X11 与 Wayland 提供 GPU 加速的 OpenGL ES 与 EGL。验证：

```bash
glxinfo | grep "OpenGL renderer"
# Should show "NVIDIA Tegra" or similar
```

---

## 4. Qt 入门

Qt 是带 GPU 加速的嵌入式 Linux GUI 最成熟的框架。

### 在 Jetson 上安装 Qt

```bash
# Qt 5 (from JetPack/Ubuntu repos)
sudo apt install qt5-default qtcreator

# Or Qt 6 (build from source or use Conan/aqt)
pip install aqtinstall
aqt install-qt linux desktop 6.6.0
```

### 最小 Qt 应用

```cpp
// main.cpp
#include <QApplication>
#include <QLabel>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QLabel label("Device Status: Running");
    label.setFont(QFont("Arial", 24));
    label.show();
    return app.exec();
}
```

```bash
# Build
qmake -project
qmake
make

# Run with EGLFS (no window manager, direct GPU)
./app -platform eglfs
```

### 用于嵌入式的 Qt 平台插件

| 插件 | 用途 |
|--------|----------|
| **eglfs** | 全屏、无窗口管理器、直接 GPU 渲染（kiosk 推荐） |
| **linuxfb** | Framebuffer，无 GPU 加速 |
| **wayland** | Wayland compositor |
| **xcb** | X11 窗口系统 |

---

## 5. 面向 Jetson 的 Qt 交叉编译

为加快构建周期，在 x86 主机上交叉编译面向 ARM64 的 Qt 应用。

### 工具链搭建

```bash
# Install cross-compiler
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# Sysroot: copy target libraries from Jetson
rsync -avz jetson:/usr/lib/aarch64-linux-gnu/ sysroot/usr/lib/
rsync -avz jetson:/usr/include/ sysroot/usr/include/
```

### CMake 工具链文件

```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_SYSROOT /path/to/sysroot)
```

---

## 6. LVGL（轻量级图形库）

LVGL 非常适合小屏幕与资源受限的显示屏 —— 无需窗口管理器即可在 framebuffer 上运行。

### 在 Jetson 上安装

```bash
git clone https://github.com/lvgl/lv_port_linux_frame_buffer.git
cd lv_port_linux_frame_buffer
mkdir build && cd build
cmake ..
make -j$(nproc)
./main   # renders to /dev/fb0
```


<details>
<summary>English original</summary>

**GUI**

<div class="course-identity auto-course" style="--course-accent: #16a34a; --course-accent-rgb: 22, 163, 74;" markdown="1">
<div class="course-identity__icon">GUI</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Jetson Track</p>
<p class="course-identity__title">Specialized course identity for GUI.</p>
<p class="course-identity__meta">Artifact: Jetson integration demo · Measure: latency, memory, power, logs</p>
</div>
</div>


**Phase 4 — Track B — Module 5.3** · Application Development

> **Focus:** Build graphical user interfaces for **Jetson Orin Nano 8GB** products — from lightweight embedded UIs (LVGL, framebuffer) through Qt desktop applications to web-based dashboards. Covers display setup, touch input, and GPU-accelerated rendering.

**Hub:** [5. Application Development](../Guide.md)

---


**1. Display setup on Jetson**

**Output interfaces**

| Interface | Use case | Notes |
|-----------|----------|-------|
| **HDMI** | Development, desktop display | Up to 4K@60 on Orin Nano |
| **DisplayPort** | High-res monitors | Alt-mode via USB-C on some carriers |
| **eDP** | Embedded LCD panels | Requires carrier board support |
| **LVDS** (via bridge) | Industrial panels | Needs HDMI/DP-to-LVDS bridge IC |
| **DSI** | Small MIPI displays | Limited support on Orin Nano |

**Resolution and timing**

```bash
# List connected displays and modes
xrandr

# Set resolution
xrandr --output HDMI-0 --mode 1920x1080 --rate 60

# For headless (virtual framebuffer)
export DISPLAY=:0
Xvfb :0 -screen 0 1920x1080x24 &
```

---

**2. Framebuffer (Linux)**

Direct framebuffer access for simple graphics without a display server.

```bash
# Check framebuffer device
ls /dev/fb*
cat /sys/class/graphics/fb0/virtual_size

# Write solid color to screen (red)
dd if=/dev/zero bs=4 count=$((1920*1080)) | \
    tr '\0' '\xff\x00\x00\xff' > /dev/fb0

# Display an image (using fbi)
sudo apt install fbi
sudo fbi -T 1 -d /dev/fb0 image.png
```

**When to use framebuffer**

- **Splash screen** during boot (before X/Wayland starts)
- **Minimal status display** on small screens (no window manager needed)
- **Kiosk mode** with a single full-screen application

---

**3. X11 and Wayland**

JetPack ships with X11 (Xorg) by default. Wayland (via Weston) is available but less tested on Jetson.

```bash
# Check current display server
echo $XDG_SESSION_TYPE

# Start X11 manually (headless or remote)
startx

# For Weston (Wayland)
sudo apt install weston
weston-launch
```

**GPU acceleration**

NVIDIA's L4T drivers provide GPU-accelerated OpenGL ES and EGL for both X11 and Wayland. Verify:

```bash
glxinfo | grep "OpenGL renderer"
# Should show "NVIDIA Tegra" or similar
```

---

**4. Getting started with Qt**

Qt is the most mature framework for embedded Linux GUIs with GPU acceleration.

**Install Qt on Jetson**

```bash
# Qt 5 (from JetPack/Ubuntu repos)
sudo apt install qt5-default qtcreator

# Or Qt 6 (build from source or use Conan/aqt)
pip install aqtinstall
aqt install-qt linux desktop 6.6.0
```

**Minimal Qt application**

```cpp
// main.cpp
#include <QApplication>
#include <QLabel>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QLabel label("Device Status: Running");
    label.setFont(QFont("Arial", 24));
    label.show();
    return app.exec();
}
```

```bash
# Build
qmake -project
qmake
make

# Run with EGLFS (no window manager, direct GPU)
./app -platform eglfs
```

**Qt platform plugins for embedded**

| Plugin | Use case |
|--------|----------|
| **eglfs** | Full-screen, no window manager, direct GPU rendering (recommended for kiosk) |
| **linuxfb** | Framebuffer, no GPU acceleration |
| **wayland** | Wayland compositor |
| **xcb** | X11 window system |

---

**5. Qt cross-compilation for Jetson**

For faster build cycles, cross-compile Qt apps on an x86 host targeting ARM64.

**Toolchain setup**

```bash
# Install cross-compiler
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# Sysroot: copy target libraries from Jetson
rsync -avz jetson:/usr/lib/aarch64-linux-gnu/ sysroot/usr/lib/
rsync -avz jetson:/usr/include/ sysroot/usr/include/
```

**CMake toolchain file**

```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_SYSROOT /path/to/sysroot)
```

---

**6. LVGL (Lightweight Graphics Library)**

LVGL is ideal for small screens and resource-constrained displays — runs on framebuffer without a window manager.

**Setup on Jetson**

```bash
git clone https://github.com/lvgl/lv_port_linux_frame_buffer.git
cd lv_port_linux_frame_buffer
mkdir build && cd build
cmake ..
make -j$(nproc)
./main   # renders to /dev/fb0
```

</details>

### 何时使用 LVGL、何时使用 Qt

| 评判标准 | LVGL | Qt |
|----------|------|-----|
| **屏幕尺寸** | 小（< 7"） | 任意 |
| **复杂度** | 简单仪表盘、状态界面 | 复杂多页面应用 |
| **内存** | UI 占用 < 1 MB RAM | 50+ MB |
| **GPU** | 不要求（CPU 渲染） | 受益于 GPU |
| **触摸** | 内置驱动支持 | 内置 |
| **样式** | 类 CSS 主题 | QSS、QML |

---

## 7. 基于 Web 的 UI

从 Jetson 提供 Web 界面——任何带浏览器的设备均可访问。

### 架构

```
Jetson (backend: Flask/FastAPI + frontend: React/Vue/plain HTML)
  │
  └─ Browser on phone/laptop → http://jetson-edge.local:8080 (example mDNS hostname)
```

### 对嵌入式产品的优势

- 设备本身**无需显示硬件**
- **跨平台**——任何浏览器均可使用
- **易于更新**——通过 OTA 下发新的 HTML/JS，无需重新刷机
- 框架：Flask + HTMX（简单）、FastAPI + React（完整 SPA）

---

## 8. 触摸屏设置与校准

### 电容触摸（I2C）

多数现代嵌入式显示屏使用经 I2C 连接的电容触摸。kernel 驱动呈现为输入设备：

```bash
# List input devices
cat /proc/bus/input/devices

# Test touch events
sudo apt install evtest
sudo evtest /dev/input/eventN
```

### 校准（如需）

```bash
# Install xinput_calibrator (X11)
sudo apt install xinput-calibrator
xinput_calibrator

# For tslib (non-X11)
sudo apt install tslib
export TSLIB_TSDEVICE=/dev/input/eventN
ts_calibrate
ts_test
```

### 触摸控制器的设备树

在载板的设备树中添加触摸控制器的 I2C 设备：

```dts
&i2c1 {
    touch@38 {
        compatible = "edt,edt-ft5406";
        reg = <0x38>;
        interrupt-parent = <&gpio>;
        interrupts = <IRQ_PIN IRQ_TYPE_EDGE_FALLING>;
    };
};
```

---

## 9. 项目

- **状态看板：** 构建一个 Qt EGLFS 应用，在 7" HDMI 显示屏上显示实时 GPU 温度、推理 FPS 和网络状态。
- **LVGL 仪表盘：** 在 framebuffer 上使用 LVGL，把传感器读数（I2C 温度 + CAN 数据）显示到小型 SPI/I2C 显示屏上。
- **Web 配置门户：** 为 Jetson 边缘设备构建 Flask + HTMX Web 界面，显示设备状态并支持从手机浏览器配置 Wi-Fi。

---

## 10. 资源

| 资源 | 描述 |
|----------|-------------|
| **Qt 文档**（doc.qt.io） | Qt 框架官方文档，EGLFS 平台指南 |
| **LVGL**（lvgl.io） | 轻量图形库，Linux framebuffer 移植 |
| **Weston/Wayland** | 嵌入式 Linux 的 Wayland compositor |
| **xinput_calibrator** | X11 的触摸屏校准工具 |
| **tslib** | 非 X11 环境的触摸屏库 |


<details>
<summary>English original</summary>

**When to use LVGL vs Qt**

| Criteria | LVGL | Qt |
|----------|------|-----|
| **Screen size** | Small (< 7") | Any |
| **Complexity** | Simple dashboards, status screens | Complex multi-page apps |
| **Memory** | < 1 MB RAM for UI | 50+ MB |
| **GPU** | Not required (CPU rendering) | Benefits from GPU |
| **Touch** | Built-in driver support | Built-in |
| **Styling** | CSS-like themes | QSS, QML |

---

**7. Web-based UI**

Serve a web interface from the Jetson — accessible from any device with a browser.

**Architecture**

```
Jetson (backend: Flask/FastAPI + frontend: React/Vue/plain HTML)
  │
  └─ Browser on phone/laptop → http://jetson-edge.local:8080 (example mDNS hostname)
```

**Advantages for embedded products**

- **No display hardware needed** on the device itself
- **Cross-platform** — works from any browser
- **Easy to update** — ship new HTML/JS via OTA without reflashing
- Frameworks: Flask + HTMX (simple), FastAPI + React (full SPA)

---

**8. Touch screen setup and calibration**

**Capacitive touch (I2C)**

Most modern embedded displays use capacitive touch connected via I2C. The kernel driver appears as an input device:

```bash
# List input devices
cat /proc/bus/input/devices

# Test touch events
sudo apt install evtest
sudo evtest /dev/input/eventN
```

**Calibration (if needed)**

```bash
# Install xinput_calibrator (X11)
sudo apt install xinput-calibrator
xinput_calibrator

# For tslib (non-X11)
sudo apt install tslib
export TSLIB_TSDEVICE=/dev/input/eventN
ts_calibrate
ts_test
```

**Device tree for touch controller**

Add the touch controller I2C device in your carrier's device tree:

```dts
&i2c1 {
    touch@38 {
        compatible = "edt,edt-ft5406";
        reg = <0x38>;
        interrupt-parent = <&gpio>;
        interrupts = <IRQ_PIN IRQ_TYPE_EDGE_FALLING>;
    };
};
```

---

**9. Projects**

- **Status kiosk:** Build a Qt EGLFS application that shows real-time GPU temperature, inference FPS, and network status on a 7" HDMI display.
- **LVGL dashboard:** Display sensor readings (I2C temperature + CAN data) on a small SPI/I2C display using LVGL on framebuffer.
- **Web config portal:** Build a Flask + HTMX web interface for your Jetson edge device that shows device status and allows Wi-Fi configuration from a phone browser.

---

**10. Resources**

| Resource | Description |
|----------|-------------|
| **Qt Documentation** (doc.qt.io) | Official Qt framework docs, EGLFS platform guide |
| **LVGL** (lvgl.io) | Lightweight graphics library, Linux framebuffer port |
| **Weston/Wayland** | Wayland compositor for embedded Linux |
| **xinput_calibrator** | Touch screen calibration tool for X11 |
| **tslib** | Touch screen library for non-X11 environments |

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/5. Application Development/3. GUI/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/5.%20Application%20Development/3.%20GUI/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
