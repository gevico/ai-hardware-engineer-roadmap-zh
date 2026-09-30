---
title: 网络与连接
description: 网络与连接
---

# 网络与连接

<div class="course-identity auto-course" style="--course-accent: #4f46e5; --course-accent-rgb: 79, 70, 229;" markdown="1">
<div class="course-identity__icon">NAC</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度解析 · Jetson 方向</p>
<p class="course-identity__title">Network and Connectivity 的专用课程标识。</p>
<p class="course-identity__meta">产物：Jetson 集成 demo · 度量：延迟、内存、功耗、日志</p>
</div>
</div>


**阶段 4 — 方向 B — 模块 5.2** · 应用开发

> **重点：**在 **Jetson Orin Nano 8GB** 上配置有线与无线网络——以太网、Wi-Fi（客户端与 AP）、蓝牙、VPN 隧道，以及用于设备管理的轻量 web 服务器。

**Hub：** [5. 应用开发](../Guide.md)

---


## 1. 以太网（Linux）

千兆以太网是大多数 Jetson 载板上的主网络接口。

### 配置

```bash
# Check link status
ip link show eth0
ethtool eth0

# Static IP
sudo nmcli con mod "Wired connection 1" \
    ipv4.addresses 192.168.1.100/24 \
    ipv4.gateway 192.168.1.1 \
    ipv4.dns "8.8.8.8" \
    ipv4.method manual
sudo nmcli con up "Wired connection 1"

# DHCP (default)
sudo nmcli con mod "Wired connection 1" ipv4.method auto
```

### 性能验证

```bash
# Install iperf3
sudo apt install iperf3

# Server (on another machine)
iperf3 -s

# Client (on Jetson)
iperf3 -c <server-ip>
# Expected: ~940 Mbps for GbE
```

---

## 2. Wi-Fi 客户端（Linux）

Jetson Orin Nano 开发套件不含 Wi-Fi——可通过 USB dongle 或 M.2 Key E 模块（Intel AX200/AX210、Realtek RTL8852）添加。

如果需要更偏嵌入式的集成路径，也可以通过 SPI 配合 **ESP-Hosted-NG**，把 **ESP32-C6** 作为无线协处理器接入。参见 [ESP32-C6 ESP-Hosted over SPI on Jetson Orin Nano](ESP32-C6-ESP-Hosted-SPI-Jetson-Orin-Nano.md)。

如果还想在同一块 Jetson 上使用 **Thread / 802.15.4**，更清晰的架构是让第一块 ESP32-C6 继续负责 Wi-Fi/BLE，再增加 **第二块 ESP32-C6** 作为 **OpenThread RCP**。参见 [ESP32-C6 OpenThread RCP on Jetson Orin Nano](ESP32-C6-OpenThread-RCP-Jetson-Orin-Nano.md)。

如果当前 Jetson 镜像缺少完整 OTBR 路由所需的 kernel 支持，第二块 ESP32-C6 也可以改为用作 **Zigbee 协处理器**。该路径仍属同一个 802.15.4 家族，但从 OTBR 的 IPv6 边界路由器模型转为由主机控制的 Zigbee 协处理器模型。参见 [ESP32-C6 Zigbee NCP on Jetson Orin Nano](ESP32-C6-Zigbee-NCP-Jetson-Orin-Nano.md)。

### 用 NetworkManager 连接

```bash
# Scan for networks
nmcli dev wifi list

# Connect
sudo nmcli dev wifi connect "SSID" password "password"

# Verify
nmcli con show --active
ip addr show wlan0
```

### 用 wpa_supplicant 连接（headless）

```bash
# /etc/wpa_supplicant/wpa_supplicant.conf
cat <<EOF | sudo tee /etc/wpa_supplicant/wpa_supplicant.conf
network={
    ssid="SSID"
    psk="password"
}
EOF

sudo wpa_supplicant -B -i wlan0 -c /etc/wpa_supplicant/wpa_supplicant.conf
sudo dhclient wlan0
```

---

## 3. Wi-Fi 接入点模式（Linux）

将 Jetson 作为 Wi-Fi 热点运行，供设备直连（适用于现场配置或独立运行）。

### 使用 NetworkManager

```bash
sudo nmcli dev wifi hotspot ifname wlan0 \
    ssid "Jetson-Setup-AP" password "securepass"
```

### 使用 hostapd（控制更细）

```bash
sudo apt install hostapd dnsmasq

# /etc/hostapd/hostapd.conf
cat <<EOF | sudo tee /etc/hostapd/hostapd.conf
interface=wlan0
driver=nl80211
ssid=Jetson-Setup-AP
hw_mode=g
channel=7
wmm_enabled=0
auth_algs=1
wpa=2
wpa_passphrase=securepass
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
EOF

# /etc/dnsmasq.conf (DHCP for AP clients)
cat <<EOF | sudo tee /etc/dnsmasq.d/ap.conf
interface=wlan0
dhcp-range=192.168.4.2,192.168.4.50,255.255.255.0,24h
EOF

sudo systemctl start hostapd
sudo systemctl start dnsmasq
```

---

## 4. 蓝牙（Linux）

蓝牙可通过 USB dongle 或 M.2 组合式 Wi-Fi/BT 模块提供。

### 基本操作

```bash
# Check adapter
bluetoothctl
  power on
  agent on
  scan on
  # Wait for devices...
  pair <MAC>
  connect <MAC>
```

### 蓝牙串口（SPP / RFCOMM）

```bash
# Listen for incoming serial connections
sudo rfcomm listen /dev/rfcomm0 1

# Connect to a device
sudo rfcomm connect /dev/rfcomm0 <MAC> 1
```

### 低功耗蓝牙（BLE）

```bash
# Scan for BLE devices
sudo hcitool lescan

# Use bluetoothctl for GATT operations
bluetoothctl
  menu gatt
  list-attributes <MAC>
  select-attribute <UUID>
  read
  write <value>
```

### Python（用 bleak 处理 BLE）

```python
import asyncio
from bleak import BleakScanner, BleakClient

async def main():
    devices = await BleakScanner.discover()
    for d in devices:
        print(d)

    async with BleakClient("AA:BB:CC:DD:EE:FF") as client:
        value = await client.read_gatt_char("0000180a-0000-1000-8000-00805f9b34fb")
        print(value)

asyncio.run(main())
```

---


<details>
<summary>English original</summary>

**Network and Connectivity**

<div class="course-identity auto-course" style="--course-accent: #4f46e5; --course-accent-rgb: 79, 70, 229;" markdown="1">
<div class="course-identity__icon">NAC</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Jetson Track</p>
<p class="course-identity__title">Specialized course identity for Network and Connectivity.</p>
<p class="course-identity__meta">Artifact: Jetson integration demo · Measure: latency, memory, power, logs</p>
</div>
</div>


**Phase 4 — Track B — Module 5.2** · Application Development

> **Focus:** Configure wired and wireless networking on the **Jetson Orin Nano 8GB** — Ethernet, Wi-Fi (client and AP), Bluetooth, VPN tunnels, and lightweight web servers for device management.

**Hub:** [5. Application Development](../Guide.md)

---


**1. Ethernet (Linux)**

Gigabit Ethernet is the primary network interface on most Jetson carriers.

**Configuration**

```bash
# Check link status
ip link show eth0
ethtool eth0

# Static IP
sudo nmcli con mod "Wired connection 1" \
    ipv4.addresses 192.168.1.100/24 \
    ipv4.gateway 192.168.1.1 \
    ipv4.dns "8.8.8.8" \
    ipv4.method manual
sudo nmcli con up "Wired connection 1"

# DHCP (default)
sudo nmcli con mod "Wired connection 1" ipv4.method auto
```

**Performance validation**

```bash
# Install iperf3
sudo apt install iperf3

# Server (on another machine)
iperf3 -s

# Client (on Jetson)
iperf3 -c <server-ip>
# Expected: ~940 Mbps for GbE
```

---

**2. Wi-Fi client (Linux)**

Jetson Orin Nano dev kit does not include Wi-Fi — add it via USB dongle or M.2 Key E module (Intel AX200/AX210, Realtek RTL8852).

If you want a more embedded integration path, you can also attach an **ESP32-C6** as a wireless coprocessor over SPI with **ESP-Hosted-NG**. See [ESP32-C6 ESP-Hosted over SPI on Jetson Orin Nano](ESP32-C6-ESP-Hosted-SPI-Jetson-Orin-Nano.md).

If you also want **Thread / 802.15.4** on the same Jetson, the cleaner architecture is to keep that first ESP32-C6 for Wi-Fi/BLE and add a **second ESP32-C6** as an **OpenThread RCP**. See [ESP32-C6 OpenThread RCP on Jetson Orin Nano](ESP32-C6-OpenThread-RCP-Jetson-Orin-Nano.md).

If your current Jetson image is missing the kernel support needed for full OTBR routing, a second ESP32-C6 can also be used as a **Zigbee coprocessor** instead. That path stays in the same 802.15.4 family but shifts from OTBR's IPv6 border-router model to a host-controlled Zigbee coprocessor model. See [ESP32-C6 Zigbee NCP on Jetson Orin Nano](ESP32-C6-Zigbee-NCP-Jetson-Orin-Nano.md).

**Connect with NetworkManager**

```bash
# Scan for networks
nmcli dev wifi list

# Connect
sudo nmcli dev wifi connect "SSID" password "password"

# Verify
nmcli con show --active
ip addr show wlan0
```

**Connect with wpa_supplicant (headless)**

```bash
# /etc/wpa_supplicant/wpa_supplicant.conf
cat <<EOF | sudo tee /etc/wpa_supplicant/wpa_supplicant.conf
network={
    ssid="SSID"
    psk="password"
}
EOF

sudo wpa_supplicant -B -i wlan0 -c /etc/wpa_supplicant/wpa_supplicant.conf
sudo dhclient wlan0
```

---

**3. Wi-Fi access point mode (Linux)**

Run the Jetson as a Wi-Fi hotspot for direct device connection (useful for field configuration or standalone operation).

**Using NetworkManager**

```bash
sudo nmcli dev wifi hotspot ifname wlan0 \
    ssid "Jetson-Setup-AP" password "securepass"
```

**Using hostapd (more control)**

```bash
sudo apt install hostapd dnsmasq

# /etc/hostapd/hostapd.conf
cat <<EOF | sudo tee /etc/hostapd/hostapd.conf
interface=wlan0
driver=nl80211
ssid=Jetson-Setup-AP
hw_mode=g
channel=7
wmm_enabled=0
auth_algs=1
wpa=2
wpa_passphrase=securepass
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
EOF

# /etc/dnsmasq.conf (DHCP for AP clients)
cat <<EOF | sudo tee /etc/dnsmasq.d/ap.conf
interface=wlan0
dhcp-range=192.168.4.2,192.168.4.50,255.255.255.0,24h
EOF

sudo systemctl start hostapd
sudo systemctl start dnsmasq
```

---

**4. Bluetooth (Linux)**

Bluetooth is available via USB dongle or M.2 combo Wi-Fi/BT module.

**Basic operations**

```bash
# Check adapter
bluetoothctl
  power on
  agent on
  scan on
  # Wait for devices...
  pair <MAC>
  connect <MAC>
```

**Bluetooth serial (SPP / RFCOMM)**

```bash
# Listen for incoming serial connections
sudo rfcomm listen /dev/rfcomm0 1

# Connect to a device
sudo rfcomm connect /dev/rfcomm0 <MAC> 1
```

**Bluetooth Low Energy (BLE)**

```bash
# Scan for BLE devices
sudo hcitool lescan

# Use bluetoothctl for GATT operations
bluetoothctl
  menu gatt
  list-attributes <MAC>
  select-attribute <UUID>
  read
  write <value>
```

**Python (bleak for BLE)**

```python
import asyncio
from bleak import BleakScanner, BleakClient

async def main():
    devices = await BleakScanner.discover()
    for d in devices:
        print(d)

    async with BleakClient("AA:BB:CC:DD:EE:FF") as client:
        value = await client.read_gatt_char("0000180a-0000-1000-8000-00805f9b34fb")
        print(value)

asyncio.run(main())
```

---

</details>

## 5. VPN（WireGuard / OpenVPN）

### WireGuard（推荐 —— 更快、更简单）

```bash
sudo apt install wireguard

# Generate keys
wg genkey | tee privatekey | wg pubkey > publickey

# /etc/wireguard/wg0.conf
cat <<EOF | sudo tee /etc/wireguard/wg0.conf
[Interface]
PrivateKey = $(cat privatekey)
Address = 10.0.0.2/24

[Peer]
PublicKey = <server-public-key>
Endpoint = <server-ip>:51820
AllowedIPs = 10.0.0.0/24
PersistentKeepalive = 25
EOF

sudo wg-quick up wg0
```

### OpenVPN

```bash
sudo apt install openvpn
sudo openvpn --config client.ovpn
```

### 已部署 Jetson 设备的使用场景

- **远程访问：** 通过 VPN 隧道 SSH 登录现场设备
- **设备群管理：** 所有设备位于同一 VPN，用于 OTA 与遥测
- **安全：** 即使在不受信任的网络上也能加密通信

---

## 6. SMB / 网络文件共享

在 Jetson 与局域网内其他机器之间共享文件。

```bash
# Install Samba
sudo apt install samba

# Create a share
cat <<EOF | sudo tee -a /etc/samba/smb.conf
[jetson-data]
   path = /data
   browseable = yes
   read only = no
   guest ok = no
EOF

sudo smbpasswd -a $(whoami)
sudo systemctl restart smbd
```

从其他机器访问：`\\<jetson-ip>\jetson-data`

---

## 7. Web 服务器（Linux）

在 Jetson 上运行轻量级 Web 服务器，用于设备管理、配置 UI 或 API 端点。

### nginx（反向代理 / 静态文件）

```bash
sudo apt install nginx
sudo systemctl start nginx
# Access at http://<jetson-ip>/
```

### Python Flask（REST API）

```python
from flask import Flask, jsonify
app = Flask(__name__)

@app.route('/api/status')
def status():
    return jsonify({
        'device': 'JetsonEdge',
        'temperature': read_temperature(),
        'uptime': read_uptime()
    })

app.run(host='0.0.0.0', port=8080)
```

### 使用场景

| 使用场景 | 技术 |
|----------|-----------|
| **设备配置门户** | Flask/FastAPI + HTML 表单 |
| **传感器 REST API** | Flask/FastAPI 返回 JSON |
| **OTA 触发端点** | nginx + webhook |
| **摄像头流** | GStreamer RTSP 或基于 HTTP 的 MJPEG |

---

## 8. 项目

- **无头 Wi-Fi 配置：** 构建配网流程，Jetson 先以 Wi-Fi AP 模式启动，提供用于输入 Wi-Fi 凭据的网页，然后切换到客户端模式。
- **[在 Jetson Orin Nano 上通过 SPI 运行 ESP32-C6 ESP-Hosted](ESP32-C6-ESP-Hosted-SPI-Jetson-Orin-Nano.md)：** 在 SPI1 上启用外部 Wi-Fi 协处理器，带握手、数据就绪和复位 GPIO。
- **[在 Jetson Orin Nano 上运行 ESP32-C6 Zigbee NCP](ESP32-C6-Zigbee-NCP-Jetson-Orin-Nano.md)：** 将第二颗 ESP32-C6 用作 Zigbee 协处理器，并把 Jetson 视为上层 Zigbee 主机或网关控制器。
- **低功耗蓝牙（BLE）传感器网关：** 读取低功耗蓝牙（BLE）传感器数据（温度、湿度），并通过以太网发布到 MQTT。
- **VPN 设备群：** 在 3 台 Jetson 设备与一台云服务器之间搭建 WireGuard。验证从服务器 SSH 访问所有设备。
- **设备管理 API：** 构建 Flask REST API，暴露设备状态（温度、磁盘、运行时长、固件版本），并接受 OTA 触发命令。

---

## 9. 资源

| 资源 | 说明 |
|----------|-------------|
| **NetworkManager CLI** | `nmcli` 连接管理文档 |
| **WireGuard**（wireguard.com）| 现代 VPN 协议，内核集成 |
| **hostapd** | Wi-Fi 接入点守护进程文档 |
| **BlueZ** | 官方 Linux 蓝牙协议栈 |
| **bleak** | 跨平台 Python 低功耗蓝牙（BLE）库 |
| **nginx** | 轻量级 Web 服务器 / 反向代理 |
| **ESP-Hosted-NG** | Espressif 面向 Linux 主机的 Wi-Fi/蓝牙传输层，用于 ESP 外设通过 SPI/SDIO/UART 接入 |


<details>
<summary>English original</summary>

**5. VPN (WireGuard / OpenVPN)**

**WireGuard (recommended — faster, simpler)**

```bash
sudo apt install wireguard

# Generate keys
wg genkey | tee privatekey | wg pubkey > publickey

# /etc/wireguard/wg0.conf
cat <<EOF | sudo tee /etc/wireguard/wg0.conf
[Interface]
PrivateKey = $(cat privatekey)
Address = 10.0.0.2/24

[Peer]
PublicKey = <server-public-key>
Endpoint = <server-ip>:51820
AllowedIPs = 10.0.0.0/24
PersistentKeepalive = 25
EOF

sudo wg-quick up wg0
```

**OpenVPN**

```bash
sudo apt install openvpn
sudo openvpn --config client.ovpn
```

**Use case for deployed Jetson devices**

- **Remote access:** SSH into field devices through VPN tunnel
- **Fleet management:** All devices on same VPN for OTA and telemetry
- **Security:** Encrypted communication even on untrusted networks

---

**6. SMB / network file sharing**

Share files between Jetson and other machines on the local network.

```bash
# Install Samba
sudo apt install samba

# Create a share
cat <<EOF | sudo tee -a /etc/samba/smb.conf
[jetson-data]
   path = /data
   browseable = yes
   read only = no
   guest ok = no
EOF

sudo smbpasswd -a $(whoami)
sudo systemctl restart smbd
```

Access from other machines: `\\<jetson-ip>\jetson-data`

---

**7. Web server (Linux)**

Run a lightweight web server on the Jetson for device management, configuration UI, or API endpoints.

**nginx (reverse proxy / static files)**

```bash
sudo apt install nginx
sudo systemctl start nginx
# Access at http://<jetson-ip>/
```

**Python Flask (REST API)**

```python
from flask import Flask, jsonify
app = Flask(__name__)

@app.route('/api/status')
def status():
    return jsonify({
        'device': 'JetsonEdge',
        'temperature': read_temperature(),
        'uptime': read_uptime()
    })

app.run(host='0.0.0.0', port=8080)
```

**Use cases**

| Use case | Technology |
|----------|-----------|
| **Device config portal** | Flask/FastAPI + HTML form |
| **REST API for sensors** | Flask/FastAPI returning JSON |
| **OTA trigger endpoint** | nginx + webhook |
| **Camera stream** | GStreamer RTSP or MJPEG over HTTP |

---

**8. Projects**

- **Headless Wi-Fi setup:** Build a provisioning flow where the Jetson starts as a Wi-Fi AP, serves a web page for entering Wi-Fi credentials, then switches to client mode.
- **[ESP32-C6 ESP-Hosted over SPI on Jetson Orin Nano](ESP32-C6-ESP-Hosted-SPI-Jetson-Orin-Nano.md):** Bring up an external Wi-Fi coprocessor on SPI1 with handshake, data-ready, and reset GPIOs.
- **[ESP32-C6 Zigbee NCP on Jetson Orin Nano](ESP32-C6-Zigbee-NCP-Jetson-Orin-Nano.md):** Use a second ESP32-C6 as a Zigbee coprocessor and treat Jetson as the higher-level Zigbee host or gateway controller.
- **BLE sensor gateway:** Read BLE sensor data (temperature, humidity) and publish to MQTT over Ethernet.
- **VPN fleet:** Set up WireGuard between 3 Jetson devices and a cloud server. Verify SSH access to all devices from the server.
- **Device management API:** Build a Flask REST API that exposes device status (temperature, disk, uptime, firmware version) and accepts OTA trigger commands.

---

**9. Resources**

| Resource | Description |
|----------|-------------|
| **NetworkManager CLI** | `nmcli` documentation for connection management |
| **WireGuard** (wireguard.com) | Modern VPN protocol, kernel-integrated |
| **hostapd** | Wi-Fi access point daemon documentation |
| **BlueZ** | Official Linux Bluetooth stack |
| **bleak** | Cross-platform BLE library for Python |
| **nginx** | Lightweight web server / reverse proxy |
| **ESP-Hosted-NG** | Espressif Linux-hosted Wi-Fi/Bluetooth transport for ESP peripherals over SPI/SDIO/UART |

</details>

---

> 原文：[`Phase 4 - Track B - Nvidia Jetson/5. Application Development/2. Network and Connectivity/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%204%20-%20Track%20B%20-%20Nvidia%20Jetson/5.%20Application%20Development/2.%20Network%20and%20Connectivity/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
