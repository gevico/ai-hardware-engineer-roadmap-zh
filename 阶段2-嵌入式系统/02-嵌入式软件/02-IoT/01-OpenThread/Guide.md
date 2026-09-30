---
title: OpenThread
description: OpenThread
---

# OpenThread

<div class="course-identity auto-course" style="--course-accent: #16a34a; --course-accent-rgb: 22, 163, 74;" markdown="1">
<div class="course-identity__icon">OPE</div>
<div markdown="1">
<p class="course-identity__eyebrow">深度剖析 · 嵌入式系统</p>
<p class="course-identity__title">OpenThread 的专用课程标识。</p>
<p class="course-identity__meta">产物：bring-up（上电点亮/调通）或固件 demo · 度量：启动、延迟、功耗、可靠性</p>
</div>
</div>


*上承 [**IoT Networking and Device Connectivity**](../Guide.md)，并把阶段 2 的嵌入式软件路径从本地外设总线延伸到低功耗 IP 组网。用本指南理解 Thread 是什么、OpenThread 如何实现它，以及同一套协议栈如何既能直接跑在 MCU（微控制器）上，也能经由 Linux 主机加射频协处理器运行。*

---

## 1. 为什么 OpenThread 重要

OpenThread 是 Google 对 **Thread** 协议的开源实现。它面向基于 IPv6 的低功耗 mesh 组网，构建在 **IEEE 802.15.4** 之上，因而很适合电池供电的传感器、智能家居端点、工业节点和边界路由器设计。  
官方来源：[OpenThread overview](https://openthread.io/)，[ESP-IDF Thread guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_openthread.html)

对路线图学习者而言，OpenThread 很有价值，因为它恰好位于 **MCU 固件**与**联网系统**的边界上。不能把它当作「又一个驱动」或「又一个协议」，因为它迫使你同时考虑射频时序、低功耗行为、安全的入网配置、IPv6 组网和 Linux 主机集成。

---

## 2. Thread 究竟是什么

Thread 是一种**基于 IP 的 mesh 协议**。在射频层它使用 IEEE 802.15.4，但在网络层它仍然是 IPv6 体系，这正是 Thread 节点能比许多更老的嵌入式现场总线更自然地融入常规 IP 工具链的原因。

这一区别很重要。低功耗蓝牙（BLE）擅长短距离外设连接，Wi-Fi 在你需要带宽时很出色，但 Thread 是为**低功耗、多节点、始终可用的组网**而构建的，网络能在路由器加入或消失时自我修复。

OpenThread 的官方平台文档称，该协议栈实现了完整的 Thread 网络各层，包括 **IPv6、6LoWPAN、带 MAC 安全的 IEEE 802.15.4、Mesh Link Establishment 和 Mesh Routing**。在实践中，这意味着协议栈既处理面向射频的机制，也处理更高层的路由行为——把众多小设备变成一个稳定的网络。  
官方来源：[OpenThread features](https://openthread.io/)

![Thread 商用网络拓扑](https://docs.silabs.com/thread-fundamentals/0.1/images/commercial-network-topology.png)

*官方参考拓扑图，展示 Thread 的主要角色，以及边界路由器如何把 mesh 接入更大的 IP 网络。来源：[Silicon Labs Thread Fundamentals](https://docs.silabs.com/thread-fundamentals/latest/)。*

---

## 3. 协议栈：从射频帧到 IP 包

把 OpenThread 当作分层协议栈来读，最容易理解：

| 层 | 作用 |
|---|---|
| IEEE 802.15.4 PHY/MAC | 定义低功耗射频链路、帧格式、信道使用、确认机制和 MAC 层安全 |
| 6LoWPAN | 压缩 IPv6 报头，使 IPv6 包能在 802.15.4 极小的帧预算下高效传输 |
| IPv6 | 为每个节点提供真正的 IP 身份，而非专有的、仅供应用使用的地址模型 |
| UDP / ICMPv6 / 更高层服务 | 在网络层之上支持消息传递、发现和管理 |
| Thread 控制面 | 处理网络组建、分区恢复、角色、入网配置和 mesh 路由 |

关键的工程理念是：Thread 不是「一个小型定制的传感器协议」。它是一个受限的 IP 网络。这就是它与边界路由器和 Linux 主机契合得如此自然的原因，也是 `ot-ctl` 和 OTBR 这类主机工具无需另造一整套独立网络体系就能管理它的原因。

6LoWPAN 在此很关键，因为原始 IPv6 包对于 802.15.4 的小帧预算来说太大、太啰嗦。Thread 通过按需压缩和分片流量来保持 IP 原生，而不是放弃 IPv6。

### 6LoWPAN：为什么 IPv6 能塞进微型射频

6LoWPAN 代表 **IPv6 over Low-Power Wireless Personal Area Networks**。它是 IETF 定义的适配层，让受限设备能在 IEEE 802.15.4 这类小型低功耗链路上承载 IPv6 流量，而不需要一整套完全独立的非 IP 协议栈。

这是 Thread 尽管跑在微型射频上、却能把自己呈现为一个真正 IP 网络的核心原因。普通 IPv6 包假定的链路比 802.15.4 提供的要大得多、能力强得多，因此 6LoWPAN 位于 MAC 层与 IPv6 层之间，对流量重新塑形以适配该介质。


<details>
<summary>English original</summary>

**OpenThread**

<div class="course-identity auto-course" style="--course-accent: #16a34a; --course-accent-rgb: 22, 163, 74;" markdown="1">
<div class="course-identity__icon">OPE</div>
<div markdown="1">
<p class="course-identity__eyebrow">Deep Dive · Embedded Systems</p>
<p class="course-identity__title">Specialized course identity for OpenThread.</p>
<p class="course-identity__meta">Artifact: bring-up or firmware demo · Measure: boot, latency, power, reliability</p>
</div>
</div>


*Follows [**IoT Networking and Device Connectivity**](../Guide.md) and extends the Phase 2 embedded-software path from local peripheral buses to low-power IP networking. Use this guide to understand what Thread is, how OpenThread implements it, and how the same stack can run either directly on an MCU or through a Linux host plus radio co-processor.*

---

**1. Why OpenThread Matters**

OpenThread is Google's open-source implementation of the **Thread** protocol. It is designed for low-power, IPv6-based mesh networking on top of **IEEE 802.15.4**, which makes it a good fit for battery-powered sensors, smart-home endpoints, industrial nodes, and border-router designs.  
Official sources: [OpenThread overview](https://openthread.io/), [ESP-IDF Thread guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_openthread.html)

For roadmap learners, OpenThread is valuable because it sits exactly at the boundary between **MCU firmware** and **networked systems**. You cannot treat it as "just another driver" or "just another protocol" because it forces you to think about radio timing, low-power behavior, secure commissioning, IPv6 networking, and Linux-host integration at the same time.

---

**2. What Thread Actually Is**

Thread is an **IP-based mesh protocol**. At the radio layer it uses IEEE 802.15.4, but at the network layer it is still an IPv6 system, which is why a Thread node can fit into normal IP-based tooling much more naturally than many older embedded fieldbuses.

That distinction is important. BLE is excellent for short-range peripheral links, and Wi-Fi is excellent when you need bandwidth, but Thread is built for **low-power, many-node, always-available networking** where the network can repair itself as routers join or disappear.

OpenThread's official platform documentation describes the stack as implementing the full Thread networking layers, including **IPv6, 6LoWPAN, IEEE 802.15.4 with MAC security, Mesh Link Establishment, and Mesh Routing**. In practice, that means the stack handles both the radio-facing mechanics and the higher routing behavior that turns many small devices into one stable network.  
Official source: [OpenThread features](https://openthread.io/)

![Thread commercial network topology](https://docs.silabs.com/thread-fundamentals/0.1/images/commercial-network-topology.png)

*Official reference topology image showing the main Thread roles and how a border router connects the mesh to the wider IP network. Source: [Silicon Labs Thread Fundamentals](https://docs.silabs.com/thread-fundamentals/latest/).*

---

**3. Protocol Stack: From Radio Frames to IP Packets**

OpenThread is easiest to understand if you read it as a layered stack:

| Layer | What it does |
|---|---|
| IEEE 802.15.4 PHY/MAC | Defines the low-power radio link, frame format, channel use, acknowledgements, and MAC-layer security |
| 6LoWPAN | Compresses IPv6 headers so IPv6 packets fit efficiently over a tiny 802.15.4 frame budget |
| IPv6 | Gives each node a real IP identity instead of a proprietary application-only address model |
| UDP / ICMPv6 / higher services | Supports messaging, discovery, and management above the network layer |
| Thread control plane | Handles network formation, partition recovery, roles, commissioning, and mesh routing |

The important engineering idea is that Thread is not a "small custom sensor protocol." It is a constrained IP network. That is why it fits so naturally with border routers and Linux hosts, and why host tools such as `ot-ctl` and OTBR can manage it without inventing an entirely separate networking world.

6LoWPAN matters here because raw IPv6 packets are too large and verbose for a small 802.15.4 frame budget. Thread stays IP-native by compressing and fragmenting traffic where needed instead of abandoning IPv6.

**6LoWPAN: why IPv6 can fit on tiny radios**

6LoWPAN stands for **IPv6 over Low-Power Wireless Personal Area Networks**. It is an IETF-defined adaptation layer that lets constrained devices carry IPv6 traffic over small, low-power links such as IEEE 802.15.4 instead of requiring a completely separate non-IP protocol stack.

This is the core reason Thread can present itself as a real IP network even though it runs on a tiny radio. A normal IPv6 packet assumes a much larger and more capable link than 802.15.4 provides, so 6LoWPAN sits between the MAC layer and the IPv6 layer and reshapes traffic to fit the medium.

</details>

#### 报头压缩

相对于 802.15.4 链路上可用的帧大小，一个普通的 IPv6 报头是很大的。6LoWPAN 通过压缩那些可预测、可共享、可从上下文推导或在网络中重复出现的字段来降低这项开销。在一个构建良好的低功耗 mesh 中，这些字段里有许多并不需要每次都完整发送。

这意味着射频花在承载协议开销上的空口时间更少，花在承载有用应用载荷上的空口时间更多。对于电池供电的设备，这不仅仅是带宽优化；它直接影响能耗、延迟，以及射频需要保持唤醒的频率。

#### 分片

IPv6 要求最小 MTU 为 1280 字节，但 IEEE 802.15.4 帧要小得多。6LoWPAN 通过把较大的报文分片成更小的无线帧，并在接收端重新组装，来处理这一不匹配。

这也是 Thread 即使在资源受限的设备上仍像是一个真正的 IP 网络的又一个原因。应用仍然可以按 IPv6 通信的思路来思考，而适配层则处理把这些报文塞进一条小得多的链路这类难看的细节。

#### 适配层的角色

从嵌入式系统的角度看，6LoWPAN 是“微型无线世界”与“IPv6 世界”之间的翻译层。它不是 IPv6 的替代品，也不是一个独立的应用协议；它是让 IPv6 在低功耗个域网中变得实用的机制。

在 Thread 协议栈中，正是这一点让你能够保留 IPv6 寻址、UDP、服务发现和边界路由等标准 IP 概念，而不必假装射频的能力与 Ethernet 或 Wi-Fi 相当。这就是为什么 Thread 能够与 Linux 主机、OTBR 以及基于 IP 的工具干净地集成，而不必到处都要求一套专有的网关协议。

#### 功耗与 mesh 的影响

6LoWPAN 是为那些可能经常休眠、短暂唤醒、却仍需要参与路由网络的设备而设计的。它与低功耗 mesh 的行为配合良好，因为它降低了传输开销，使 IP 通信的射频代价保持在可控范围内。

这就是它如此频繁地出现在智能家居、工业和传感器网络部署中的原因。IEEE 802.15.4、6LoWPAN、IPv6 与 Thread 路由的组合，给你一个低功耗、自愈、并且仍能通过主流网络概念来理解的网络。

### 6LoWPAN / Thread 网络中的 UDP 与 ICMPv6

一旦 6LoWPAN 让 IPv6 变得实用，下一个问题就是哪些传输与控制协议运行在它之上。实践中，**UDP** 和 **ICMPv6** 是最先需要理解的两个最重要的协议。

OpenThread 的概览明确将该协议栈描述为支持 **IPv6、UDP、CoAP 和 ICMPv6**，这强烈暗示了真实应用应当如何使用这个网络。UDP 是应用流量的常用传输协议，而 ICMPv6 负责诊断等核心网络控制任务以及部分 IPv6 控制行为。  
官方来源：[OpenThread overview](https://openthread.io/)

#### 为什么 UDP 被如此频繁地使用

在低功耗 Thread 系统中，UDP 是常见的传输选择，因为它轻量且无连接。电池供电的传感器并不想要更重的传输协议带来的复杂度和持续的状态开销，除非它确实需要，因此 CoAP 这类应用协议通常构建在 UDP 而非 TCP 之上。

RFC 6282 在这里很重要，因为它定义了 **LOWPAN_NHC**，即压缩 IPv6 报头之后使用的下一报头压缩格式。该 RFC 专门定义了 **UDP** 的压缩，这也是 UDP 在 6LoWPAN 网络中如此自然契合的原因之一。  
官方来源：[RFC 6282](https://datatracker.ietf.org/doc/html/rfc6282)

UDP 压缩在两个实际方面很重要：

* 报头可以被压缩，突破常规的 8 字节 UDP 形式
* 某个特定端口范围（`0xf0b0` 到 `0xf0bf`，十进制 `61616` 到 `61631`）可以被非常激进地压缩

这并不只是一个协议上的趣闻。在很小的低功耗帧预算下，反复省下几个字节可以实实在在地减少空口时间和功耗。

有一点细微之处值得明确说明：在 IPv6 中，UDP 校验和通常是强制的。RFC 6282 仅在受限条件下允许省略校验和，且需要上层授权和额外的完整性检查。因此正确的心智模型不是“6LoWPAN 去掉了可靠性检查”，而是“当系统其余部分仍能保证完整性时，6LoWPAN 允许经过谨慎控制的报头大小取舍”。


<details>
<summary>English original</summary>

**Header compression**

A plain IPv6 header is large relative to the frame size available on 802.15.4 links. 6LoWPAN reduces this cost by compressing fields that are predictable, shared, derivable from context, or repeated across the network. In a well-formed low-power mesh, many of those fields do not need to be sent in full every time.

That means the radio spends less airtime carrying protocol overhead and more airtime carrying useful application payload. For battery-powered devices, this is not just a bandwidth optimization; it directly affects energy use, latency, and how often the radio has to stay awake.

**Fragmentation**

IPv6 requires a minimum MTU of 1280 bytes, but IEEE 802.15.4 frames are far smaller. 6LoWPAN handles this mismatch by fragmenting larger packets into smaller radio frames and reassembling them on the receiving side.

This is another reason Thread feels like a real IP network even on constrained devices. The application can still think in terms of IPv6 communication, while the adaptation layer handles the ugly details of fitting those packets onto a much smaller link.

**Adaptation layer role**

From an embedded-systems point of view, 6LoWPAN is the translation layer between "tiny radio world" and "IPv6 world." It is not a replacement for IPv6 and not a separate application protocol; it is the mechanism that makes IPv6 practical on low-power personal area networks.

In the Thread stack, this is what lets you keep standard IP ideas such as IPv6 addressing, UDP, service discovery, and border routing without pretending that the radio is as capable as Ethernet or Wi-Fi. That is why Thread can integrate cleanly with Linux hosts, OTBR, and IP-based tools instead of requiring a proprietary gateway protocol everywhere.

**Power and mesh implications**

6LoWPAN is designed for devices that may sleep often, wake briefly, and still need to participate in a routed network. It works well with low-power mesh behavior because it reduces transmission overhead and keeps the radio cost of IP communication manageable.

This is why it shows up so often in smart-home, industrial, and sensor-network deployments. The combination of IEEE 802.15.4, 6LoWPAN, IPv6, and Thread routing gives you a network that is low-power, self-healing, and still understandable through mainstream networking concepts.

**UDP and ICMPv6 in a 6LoWPAN / Thread network**

Once IPv6 is made practical by 6LoWPAN, the next question is what transport and control protocols ride on top of it. In practice, **UDP** and **ICMPv6** are the two most important ones to understand first.

OpenThread's overview explicitly presents the stack as supporting **IPv6, UDP, CoAP, and ICMPv6**, which is a strong hint about how real applications are expected to use the network. UDP is the usual transport for application traffic, while ICMPv6 handles core network-control tasks such as diagnostics and parts of IPv6 control behavior.  
Official source: [OpenThread overview](https://openthread.io/)

**Why UDP is used so often**

UDP is the common transport choice in low-power Thread systems because it is lightweight and connectionless. A battery-powered sensor does not want the complexity and ongoing state cost of a heavier transport unless it truly needs it, so application protocols such as CoAP are commonly built on UDP instead of TCP.

RFC 6282 matters here because it defines **LOWPAN_NHC**, the next-header compression format used after the compressed IPv6 header. That RFC specifically defines compression for **UDP**, which is one reason UDP fits so naturally in 6LoWPAN networks.  
Official source: [RFC 6282](https://datatracker.ietf.org/doc/html/rfc6282)

UDP compression matters in two practical ways:

* the header can be compressed beyond the normal 8-byte UDP form
* a specific port range (`0xf0b0` to `0xf0bf`, decimal `61616` to `61631`) can be compressed very aggressively

That is not just a protocol curiosity. On a tiny low-power frame budget, saving a few bytes repeatedly can materially reduce airtime and power use.

One nuance is worth stating clearly: with IPv6, the UDP checksum is normally mandatory. RFC 6282 allows checksum elision only under restricted conditions, with upper-layer authorization and an additional integrity check. So the correct mental model is not "6LoWPAN removes reliability checks," but rather "6LoWPAN allows carefully controlled header-size tradeoffs when the rest of the system can still guarantee integrity."

</details>

#### 为什么 ICMPv6 仍然重要

ICMPv6 不是可有可无的背景噪声。它是 IPv6 网络保持可管理性的一部分。在低功耗 Thread 系统中，它仍被用于诊断和核心控制行为，包括 echo request（`ping`）和 IPv6 控制交互之类的事情。

但标准 IPv6 Neighbor Discovery 严重依赖组播，这对休眠型低功耗无线网络很不合适。RFC 6775 之所以存在，正是因为未经修改的 IPv6 Neighbor Discovery 由于大量使用组播和非传递性无线链路，在 6LoWPAN 中效率低下，有时甚至不可行。  
官方来源：[RFC 6775](https://www.rfc-editor.org/info/rfc6775)

因此在 6LoWPAN 系统中，ICMPv6 依然存在，但周边的发现行为针对受限网络做了适配。这就是为什么低功耗 IP 组网不只是“在小无线电上跑 IPv6”——控制行为也必须为休眠、易丢包、电池供电的节点重新塑形。

#### 为什么这两个协议合在一起才重要

正是 UDP 和 ICMPv6 一起，让 Thread 设备表现得像一等的 IP 节点，而不是自定义网关背后的专有端点。UDP 承载应用流量，而 ICMPv6 和优化后的 IPv6 控制行为让网络保持可诊断、可运行。

这一组合是 Thread 能与 Linux 主机和边界路由器顺畅集成的最大原因之一。你并不是发明一种特殊的应用协议再把它通过网关隧道化；你仍然在 IPv6 的世界里工作，只不过是一个经过适配的世界。

### Thread 控制平面：mesh 如何思考

数据平面搬运报文。**控制平面**决定 mesh 如何形成、哪些节点相互信任、哪些路径是好的，以及全网范围的信息如何分发。

在 Thread 中，核心的控制平面机制是 **Mesh Link Establishment (MLE)**。OpenThread 的 Thread primer 指出，MLE 用于配置链路，并向 Thread 设备传播有关网络的信息。  
官方来源：[OpenThread Thread Primer: Network Discovery and Formation](https://openthread.io/guides/thread-primer/network-discovery)

#### MLE

MLE 这一协议帮助设备从“我在无线电上听到了些什么”走到“我已安全地接入这个 mesh，并了解自己的 parent、leader 和路由上下文”。OpenThread primer 列出了 MLE 的这些职责：

* 发现邻近设备
* 判定链路质量
* 与邻居建立链路
* 协商链路参数，例如设备类型、帧计数器和超时

同一份 primer 还说明，MLE 会传播：

* leader 数据
* 网络数据
* 路由传播信息

正因如此，MLE 最好被理解为 Thread 控制平面的粘合剂。它不只关乎一条链路建立起来；它还关乎 mesh 如何共享足够的状态，使整个网络保持一致。

#### 距离矢量风格的路由传播

OpenThread 的 Thread primer 明确指出，Thread 中的路由传播与 **RIP** 这一距离矢量路由协议的工作方式类似。这提供了一个有用的心智模型：路由器交换与路由相关的信息，基于分布式的代价信息选择路径，而不是依赖单一的中心控制器。  
官方来源：[OpenThread Thread Primer: Network Discovery and Formation](https://openthread.io/guides/thread-primer/network-discovery)

这一点很重要，因为它解释了 Thread 为何能够自愈。如果某个路由器消失，控制平面的其余部分仍能收敛到新的 parent 关系和路由选择，而无需运维人员手工重建网络。


<details>
<summary>English original</summary>

**Why ICMPv6 still matters**

ICMPv6 is not optional background noise. It is part of how an IPv6 network remains manageable. In low-power Thread systems, it is still used for diagnostics and core control behavior, including things like echo requests (`ping`) and IPv6 control interactions.

But standard IPv6 Neighbor Discovery relies heavily on multicast, and that is a bad fit for sleepy low-power wireless networks. RFC 6775 exists exactly because unmodified IPv6 Neighbor Discovery is inefficient and sometimes impractical in a 6LoWPAN due to heavy multicast use and non-transitive wireless links.  
Official source: [RFC 6775](https://www.rfc-editor.org/info/rfc6775)

So in 6LoWPAN systems, ICMPv6 is still present, but the surrounding discovery behavior is adapted for constrained networks. This is why low-power IP networking is more than just "run IPv6 on a small radio" — the control behavior also has to be reshaped for sleepy, lossy, battery-powered nodes.

**Why these two protocols matter together**

UDP and ICMPv6 together are what make Thread devices feel like first-class IP nodes instead of proprietary endpoints behind a custom gateway. UDP carries the application traffic, while ICMPv6 and the optimized IPv6 control behavior keep the network diagnosable and operational.

That combination is one of the biggest reasons Thread integrates cleanly with Linux hosts and border routers. You are not inventing a special application protocol and then tunneling it through a gateway; you are still operating in an IPv6 world, just an adapted one.

**Thread control plane: how the mesh thinks**

The data plane moves packets. The **control plane** decides how the mesh forms, which nodes trust each other, which paths are good, and how network-wide information is distributed.

In Thread, the core control-plane mechanism is **Mesh Link Establishment (MLE)**. OpenThread's Thread primer states that MLE is used to configure links and disseminate information about the network to Thread devices.  
Official source: [OpenThread Thread Primer: Network Discovery and Formation](https://openthread.io/guides/thread-primer/network-discovery)

**MLE**

MLE is the protocol that helps a device move from "I hear something on the radio" to "I am securely attached to this mesh and understand my parent, leader, and route context." The OpenThread primer lists these MLE responsibilities:

* discover neighboring devices
* determine link quality
* establish links to neighbors
* negotiate link parameters such as device type, frame counters, and timeout

The same primer also explains that MLE disseminates:

* leader data
* network data
* route propagation information

This is why MLE is best thought of as the glue of the Thread control plane. It is not only about one link coming up; it is also how the mesh shares enough state for the whole network to stay coherent.

**Distance-vector style route propagation**

OpenThread's Thread primer explicitly says that route propagation in Thread works similarly to **RIP**, a distance-vector routing protocol. That gives you a useful mental model: routers exchange route-related information and choose paths based on distributed cost knowledge rather than relying on one central controller.  
Official source: [OpenThread Thread Primer: Network Discovery and Formation](https://openthread.io/guides/thread-primer/network-discovery)

This matters because it explains why Thread can be self-healing. If one router disappears, the rest of the control plane can still converge on new parent relationships and route choices without an operator manually rebuilding the network.

</details>

#### Leader 选举与故障切换

每个 Thread 分区恰好有一个 **Leader**。Leader 仍然是路由器，但它额外承担管理路由器集合、协调 Router ID 分配与分区状态等信息的职责。  
官方来源：[OpenThread Thread Primer: Node Roles and Types](https://openthread.io/guides/thread-primer/node-roles-and-types)

该角色是动态的，并非永久硬连线到某一台设备上。网络首次组建时，第一个路由器可以自选为 Leader。若该 Leader 消失，另一个路由器可以自动接管，这也是 Thread 在常规 mesh 运行中不需要一个常设受管中心控制器的原因之一。  
官方来源：[OpenThread Thread Primer: Network Discovery and Formation](https://openthread.io/guides/thread-primer/network-discovery), [OpenThread API codelab](https://openthread.io/codelabs/openthread-apis)

工程实践中的经验是：Leader 丢失应当是可恢复的网络事件，而不是灾难性中断。仍然需要稳定的射频与供电，但控制平面的设计使得单个节点消失不会摧毁整个 mesh。

OpenThread 的 CLI 与 Commissioner 状态模型也明确表明，Leader 身份与 commissioning 是受管理的角色，而非一次性固定属性。Commissioner 在成为活动状态之前可以进入 **petitioning** 状态，而若另一个路由器需要接管分区管理，Leader 也可以被替换。  
官方来源：[OpenThread CLI reference](https://openthread.io/reference/cli/commands), [OpenThread Commissioner API](https://openthread.io/reference/group/api-commissioner)

#### REED 提升与路由器数量管理

Thread 不希望每个 Full Thread Device 始终都做路由器，因为那样会浪费功耗并增加不必要的控制流量。OpenThread 的角色文档指出，Thread 会尽量把路由器数量维持在一个健康的运行区间内，而不是简单地将其最大化。  
官方来源：[OpenThread Thread Primer: Node Roles and Types](https://openthread.io/guides/thread-primer/node-roles-and-types)

这正是 **Router Eligible End Device (REED)** 角色的意义所在。REED 可以先以终端设备身份接入，随后在网络需要更多路由容量时将自己提升为路由器。OpenThread 的入门文档指出，当路由器数量低于首选阈值时，REED 可以自动自我升级；当条件变化时，一个没有子节点的路由器也可以降级回终端设备角色。  
官方来源：[OpenThread Thread Primer: Node Roles and Types](https://openthread.io/guides/thread-primer/node-roles-and-types), [OpenThread Thread Primer: Router Selection](https://openthread.io/guides/thread-primer/router-selection)

这就是 Thread 既稳定又经济的原因。它可以在需要时增加路由容量，但不会强迫每个有能力的节点永远停留在开销最大的角色上。

#### MLE 与 commissioning 不是一回事

OpenThread 入门文档中一个细微但重要的点是：**只有在设备通过 Thread commissioning 获取到 Thread 网络凭证之后，MLE 才会继续推进**。换言之，安全入网先发生，之后设备才完整参与 mesh 控制机制。

这种分离是良好的系统设计。Commissioning 回答的是“这台设备是否被允许加入？”，而 MLE 回答的是“既然已被允许加入，它如何接入并获知 mesh？”。把这两个概念混在一起会让 Thread 更难推理。

---

## 4. 设备角色及其为何重要

Thread 网络不是扁平结构。节点会根据自身是转发流量、积极休眠，还是协调 mesh 的某些部分，而承担不同的角色。

### Leader

Leader 管理关键的全网状态，例如分区信息、配置数据以及路由器集合管理。它不是一个永久的首领节点；若它消失，另一个路由器可以接管，因此网络无需人工干预即可承受节点丢失。  
官方来源：[OpenThread Thread Primer: Node Roles and Types](https://openthread.io/guides/thread-primer/node-roles-and-types), [OpenThread API codelab](https://openthread.io/codelabs/openthread-apis)

### Router

路由器为其他设备转发报文并维持 mesh 连通。它们是 Thread 网络的基础设施，因此通常采用市电供电，或至少比休眠型端点受到的功耗约束更宽松。


<details>
<summary>English original</summary>

**Leader selection and failover**

Every Thread partition has exactly one **Leader**. The Leader is still a router, but it has the extra responsibility of managing the router set and coordinating information such as Router ID assignments and partition state.  
Official source: [OpenThread Thread Primer: Node Roles and Types](https://openthread.io/guides/thread-primer/node-roles-and-types)

This role is dynamic, not permanently hard-wired to one device. When a network is first formed, the first router can elect itself Leader. If that Leader disappears, another router can take over automatically, which is one of the reasons Thread does not need a permanently managed central controller for normal mesh operation.  
Official sources: [OpenThread Thread Primer: Network Discovery and Formation](https://openthread.io/guides/thread-primer/network-discovery), [OpenThread API codelab](https://openthread.io/codelabs/openthread-apis)

The practical engineering lesson is that Leader loss is supposed to be a recoverable network event, not a catastrophic outage. You still need stable radios and power, but the control plane is designed so one node disappearing does not destroy the entire mesh.

OpenThread's CLI and Commissioner state model also make it clear that leadership and commissioning are managed roles, not one-time fixed properties. A Commissioner can enter a **petitioning** state before becoming active, and a Leader can be replaced if another router needs to take over partition management.  
Official sources: [OpenThread CLI reference](https://openthread.io/reference/cli/commands), [OpenThread Commissioner API](https://openthread.io/reference/group/api-commissioner)

**REED promotion and router-count management**

Thread does not want every Full Thread Device to become a router all the time, because that would waste power and increase unnecessary control traffic. OpenThread's role documentation says Thread tries to keep the number of routers in a healthy operating band rather than simply maximizing it.  
Official source: [OpenThread Thread Primer: Node Roles and Types](https://openthread.io/guides/thread-primer/node-roles-and-types)

This is where the **Router Eligible End Device (REED)** role matters. A REED can attach as an end device first and then promote itself to a router if the network needs more routing capacity. OpenThread's primer notes that when the router count is below the preferred threshold, a REED can automatically upgrade itself; when conditions change, a router without children can also downgrade back toward an end-device role.  
Official sources: [OpenThread Thread Primer: Node Roles and Types](https://openthread.io/guides/thread-primer/node-roles-and-types), [OpenThread Thread Primer: Router Selection](https://openthread.io/guides/thread-primer/router-selection)

That is why Thread is both stable and economical. It can add routing capacity when needed, but it does not force every capable node to stay in the most expensive role forever.

**MLE and commissioning are not the same thing**

One subtle but important point from the OpenThread primer is that **MLE only proceeds once a device has obtained Thread network credentials through Thread commissioning**. In other words, secure admission to the network happens first, and only then does the device participate fully in the mesh-control machinery.

This separation is good system design. Commissioning answers "is this device allowed in?" while MLE answers "now that it is allowed in, how does it attach and learn the mesh?" Mixing those two ideas together makes Thread harder to reason about.

---

**4. Device Roles and Why They Matter**

A Thread network is not flat. Nodes take on different roles depending on whether they route traffic, sleep aggressively, or coordinate parts of the mesh.

**Leader**

The Leader manages key network-wide state such as partition information, configuration data, and router-set management. It is not a permanent boss node; if it disappears, another router can take over, so the network can survive node loss without manual intervention.  
Official sources: [OpenThread Thread Primer: Node Roles and Types](https://openthread.io/guides/thread-primer/node-roles-and-types), [OpenThread API codelab](https://openthread.io/codelabs/openthread-apis)

**Router**

Routers forward packets for other devices and keep the mesh connected. They are the infrastructure of the Thread network, so they are normally mains-powered or at least less aggressively power-constrained than sleepy endpoints.

</details>

### 符合路由器条件的终端设备（REED）

REED 本身尚未参与路由，但当网络需要更多路由容量时，它可以升级为路由器。这让网络具备自适应性，而不必在上线前为每个节点手动指定角色。

OpenThread 的角色文档把这个机制存在的原因说得很清楚：Thread 会尽量把路由器数量维持在有用的区间内，当路由容量不足时，REED 可以自行提升为路由器。反过来，不再拥有子节点的路由器之后可能降级，这有助于让控制面保持精简，而不是用常开的路由器把 mesh 塞得过满。  
官方来源：[OpenThread Thread Primer: Node Roles and Types](https://openthread.io/guides/thread-primer/node-roles-and-types), [OpenThread Thread Primer: Router Selection](https://openthread.io/guides/thread-primer/router-selection)

### 终端设备 / 休眠终端设备

终端设备通过父路由器接入，而不自行转发 mesh 流量。休眠终端设备更进一步，大部分时间都处于休眠状态，Thread 正是借此在支持长续航的同时不牺牲网络可达性。

关键在于，**休眠终端设备（Sleepy End Device，SED）**不参与路由，也不会让射频持续开启。它依赖**父路由器**作为自己常开的代表。SED 休眠期间由父节点缓存数据，SED 周期性唤醒，轮询是否有待处理的流量。  
官方来源：[OpenThread Thread Primer: Node Roles and Types](https://openthread.io/guides/thread-primer/node-roles-and-types)

这是 Thread 中最重要的节能思路之一。由于父节点保持活跃，网络始终可达，而电池供电的子节点只需周期性地付出射频开销。如果设计目标是低平均电流，那么理解这套父节点缓存与轮询模型是必需的。

OpenThread 的高级特性指南把这一机制称为**间接传输**：父节点一直持有数据，直到休眠子节点来索取。同一份指南还指出，休眠终端设备会周期性唤醒并轮询其父节点，OpenThread 使用 frame-pending 行为来指示是否存在排队的数据。  
官方来源：[OpenThread advanced features](https://openthread.io/guides/porting/implement-advanced-features)

这套轮询模型是低功耗问题的实际解法。SED 不需要一直开着接收而白白耗电，网络也不会失去可达性，因为父路由器充当入向流量的常开代理。

### 边界路由器与 Commissioner

边界路由器把 Thread mesh 连接到其他 IP 网络，例如 Ethernet、Wi-Fi 或基于 USB 的 Linux 接口。Commissioner 负责新设备的入网与授权，因此 Thread 的部署配置是一个网络管理问题，而不只是射频问题。

实际上，Commissioner 是安全准入路径的一部分。它并不是简单地“发现”某台设备就放行，而是参与显式的 commissioning 流程，对 Joiner 进行授权并传递正确的凭证。

---

## 5. OpenThread 软件架构

OpenThread 被如此广泛使用的一个原因，是它支持多种部署模型，而不是强迫每个产品采用同一种软件划分方式。OpenThread 的平台文档明确列出了 **SoC** 与**协处理器**两种设计。  
官方来源：[OpenThread platforms](https://openthread.io/platforms)

### SoC 设计

在 System-on-Chip 设计中，射频、OpenThread 协议栈与应用全部运行在同一颗 MCU 或无线 SoC 上。这是终端设备最常见的做法，因为成本低、功耗低且直接：固件在本地调用 OpenThread API，同一颗芯片驱动射频。

当设备本身就是产品时，例如电池传感器、灯开关或智能插座，用的就是这个模型。在 ESP-IDF 中，`openthread/ot_cli` 示例是理解这条路径的一个很好的思维模型。

### NCP 设计

在 Network Co-Processor 设计中，主机应用通过 **Spinel** 协议与协处理器通信。OpenThread 可以部分运行在主机侧，面向网络的设备则表现得像一个专用的网络组件，而不是主应用处理器。

当已经存在一颗更强的主机 CPU，而你又希望把网络功能与应用逻辑分离时，这种划分就很有用。它也让主机侧开发更容易，因为应用的大改动不必每次都重新构建射频固件。


<details>
<summary>English original</summary>

**Router-Eligible End Device (REED)**

A REED is not routing yet, but it can become a router if the network needs more routing capacity. This makes the network adaptive instead of requiring every node to be manually assigned a role up front.

OpenThread's role documentation is very clear on why this exists: Thread tries to keep router count in a useful band, and a REED can promote itself when routing capacity is low. Conversely, a router that no longer has children may later downgrade, which helps keep the control plane lean instead of over-populating the mesh with always-on routers.  
Official sources: [OpenThread Thread Primer: Node Roles and Types](https://openthread.io/guides/thread-primer/node-roles-and-types), [OpenThread Thread Primer: Router Selection](https://openthread.io/guides/thread-primer/router-selection)

**End Device / Sleepy End Device**

End devices attach through a parent router instead of forwarding mesh traffic themselves. Sleepy end devices go further and spend most of their time asleep, which is how Thread supports long battery life without giving up network reachability.

The key point is that a **Sleepy End Device (SED)** does not participate in routing and does not keep its radio on continuously. Instead, it depends on a **parent router** to act as its always-on representative. The parent buffers data while the SED sleeps, and the SED wakes periodically to poll for pending traffic.  
Official source: [OpenThread Thread Primer: Node Roles and Types](https://openthread.io/guides/thread-primer/node-roles-and-types)

This is one of the most important energy-saving ideas in Thread. The network stays reachable because the parent remains active, while the battery-powered child only pays the radio cost periodically. If you are designing for low average current, understanding this parent-buffering and poll model is essential.

OpenThread's advanced-feature guide explains this as **indirect transmission**: the parent holds data until the sleepy child asks for it. That same guide also notes that the sleepy end device periodically wakes to poll its parent, and OpenThread uses frame-pending behavior to indicate whether queued data exists.  
Official source: [OpenThread advanced features](https://openthread.io/guides/porting/implement-advanced-features)

That polling model is the practical answer to the low-power problem. The SED does not burn power listening all the time, and the network does not lose reachability because the parent router serves as the always-on proxy for inbound traffic.

**Border Router and Commissioner**

A Border Router connects the Thread mesh to other IP networks such as Ethernet, Wi-Fi, or USB-backed Linux interfaces. A Commissioner is responsible for onboarding and authorizing new devices, which is why Thread setup is a network-management problem, not just a radio problem.

In practice, the Commissioner is part of the secure admission path. It does not simply "notice" a device and allow it in; it is involved in the explicit commissioning process that authorizes a Joiner and transfers the right credentials.

---

**5. OpenThread Software Architectures**

One reason OpenThread is so widely used is that it supports multiple deployment models instead of forcing every product into the same software split. The OpenThread platform documentation explicitly calls out both **SoC** and **co-processor** designs.  
Official source: [OpenThread platforms](https://openthread.io/platforms)

**SoC Design**

In a System-on-Chip design, the radio, OpenThread stack, and application all run on the same MCU or wireless SoC. This is the most common approach for end devices because it is low-cost, low-power, and direct: your firmware calls the OpenThread APIs locally and the same chip drives the radio.

This is the model you use when the device itself is the product, such as a battery sensor, light switch, or smart plug. In ESP-IDF, the `openthread/ot_cli` example is a good mental model for this path.

**NCP Design**

In a Network Co-Processor design, the host application talks to a co-processor using the **Spinel** protocol. OpenThread can run partly on the host side, with the network-facing device behaving like a dedicated networking component rather than the main application processor.

This split is useful when a stronger host CPU exists already and you want the network function separated from the application logic. It also makes host-side development easier because large application changes do not require rebuilding the radio firmware every time.

</details>

### RCP 设计

在 Radio Co-Processor 设计中，主机处理器运行核心 OpenThread 协议栈，而所连接的设备处理最少的射频相关工作。OpenThread 官方的协处理器文档将此描述为：主机保留协议逻辑，而设备充当 Thread 无线控制器，通常通过 **SPI** 或 **UART** 连接，并使用 **Spinel**。  
官方来源：[Co-Processor Designs](https://openthread.io/platforms/co-processor), [OT Daemon](https://openthread.io/platforms/co-processor/ot-daemon)

这是对 Linux 网关和 Jetson 级系统最重要的模型。它让 Linux 主机运行 OTBR 或 `ot-daemon`，而小型 MCU（微控制器）或射频 SoC 提供 802.15.4 链路。

---

## 6. OpenThread 内部机制：协议栈为何可移植

OpenThread 有意设计为使网络核心可跨许多芯片和操作系统移植。OpenThread 平台指南将其描述为带有 **窄平台抽象层（PAL）** 的可移植 C/C++，因此同一个核心可以运行在裸机系统、FreeRTOS、Zephyr、Linux 和 macOS 上。  
官方来源：[OpenThread platforms](https://openthread.io/platforms)

PAL 是 OpenThread 核心与底层硬件移植层之间的契约。OpenThread 移植指南列出的主要 PAL 领域包括：

* alarm/timer 服务
* 总线接口，如 UART 或 SPI
* IEEE 802.15.4 射频接口
* 熵 / 随机源
* 设置存储
* 日志记录
* 系统特定的初始化  
官方来源：[Platform Abstraction Layer APIs](https://openthread.io/guides/porting/implement-platform-abstraction-layer-apis)

这很重要，因为它告诉你“移植 OpenThread”真正意味着什么。你不是在从头重写 mesh 协议栈。你是在正确实现面向硬件的接口，以便可移植的上层能够信任你的射频、定时器、存储和传输行为。

---

## 7. ESP32-C6 上的 OpenThread

ESP-IDF 通过其 `openthread` 组件提供 Thread 支持，并记录了若干官方示例：

* `openthread/ot_cli`
* `openthread/ot_rcp`
* `openthread/ot_br`
* `openthread/ot_trel`
* sleepy-device 示例  
官方来源：[ESP-IDF Thread guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_openthread.html)

这是路线图的关键实践点：**ESP32-C6 既可以用作完整的 Thread 节点，也可以用作连接到 Linux 主机的 RCP**。这使它格外适合学习，因为同一颗芯片可以让你同时了解 MCU 侧的 Thread 固件和主机加射频架构。

ESP-IDF 还通过如下函数暴露 OpenThread 生命周期：

* `esp_openthread_init(...)`
* `esp_openthread_auto_start(...)`
* `esp_openthread_launch_mainloop(...)`

这些 API 清晰地展示了嵌入式控制流。首先初始化平台和协议栈，然后可选地用 dataset 启动，接着进入主循环，处理射频事件、定时器和协议工作。

---

## 8. CLI、Spinel 和主机控制

OpenThread CLI 不只是一个演示 shell。它是了解协议栈处于什么状态、节点如何组建网络以及它当前扮演什么角色的最快方式之一。

对于 SoC 设计，CLI 通常直接在设备上运行。对于主机加 RCP 设计，主机通过 **Spinel** 与射频通信；OpenThread 将 Spinel 描述为 RCP 和 NCP 设计都使用的标准主机控制器协议。  
官方来源：[Co-Processor Designs](https://openthread.io/platforms/co-processor)

当主机侧是 POSIX 或 Linux 时，**OpenThread Daemon（`ot-daemon`）** 是围绕此模型的轻量级服务封装。OpenThread 文档显示它作为服务运行，暴露一个 UNIX socket，并可通过 `ot-ctl` 控制。这是理解 Linux 主机与串行 Thread 射频通信的最清晰方式，而无需立即引入完整的 Border Router 协议栈。  
官方来源：[OT Daemon](https://openthread.io/platforms/co-processor/ot-daemon)

### `dataset` 命令究竟是什么

在 OpenThread CLI 中，**dataset** 是定义 Thread 网络的一组参数。官方 dataset 参考说明，Thread 网络配置通过 **Active** 和 **Pending Operational Dataset** 对象管理。  
官方来源：[Display and Manage Datasets with OT CLI](https://openthread.io/reference/cli/concepts/dataset)

Active Operational Dataset 包含网络中实际正在使用的参数，包括以下项目：

* 信道
* 扩展 PAN ID
* mesh-local 前缀
* 网络名称
* PAN ID
* PSKc
* 安全策略

这意味着 dataset 命令不是随意的辅助命令。它们是检查和构建定义 Thread mesh 的网络身份与凭据的方式。


<details>
<summary>English original</summary>

**RCP Design**

In a Radio Co-Processor design, the host processor runs the core OpenThread stack while the attached device handles the minimal radio-facing work. OpenThread's official co-processor documentation describes this as the host keeping the protocol logic while the device acts as the Thread radio controller, usually connected over **SPI** or **UART** using **Spinel**.  
Official sources: [Co-Processor Designs](https://openthread.io/platforms/co-processor), [OT Daemon](https://openthread.io/platforms/co-processor/ot-daemon)

This is the model that matters most for Linux gateways and Jetson-class systems. It lets a Linux host run OTBR or `ot-daemon`, while a small MCU or radio SoC provides the 802.15.4 link.

---

**6. OpenThread Internals: Why the Stack Is Portable**

OpenThread is intentionally written so the networking core is portable across many chips and operating systems. The OpenThread platform guide describes it as portable C/C++ with a **narrow Platform Abstraction Layer (PAL)**, which is why the same core can run on bare-metal systems, FreeRTOS, Zephyr, Linux, and macOS.  
Official source: [OpenThread platforms](https://openthread.io/platforms)

The PAL is the contract between the OpenThread core and the underlying hardware port. The OpenThread porting guide lists the major PAL areas as:

* alarm/timer services
* bus interfaces such as UART or SPI
* IEEE 802.15.4 radio interface
* entropy / random source
* settings storage
* logging
* system-specific initialization  
Official source: [Platform Abstraction Layer APIs](https://openthread.io/guides/porting/implement-platform-abstraction-layer-apis)

This matters because it tells you what "porting OpenThread" really means. You are not rewriting the mesh stack from scratch. You are implementing the hardware-facing interfaces correctly so the portable upper layers can trust your radio, timer, storage, and transport behavior.

---

**7. OpenThread on ESP32-C6**

ESP-IDF exposes Thread support through its `openthread` component and documents several official examples:

* `openthread/ot_cli`
* `openthread/ot_rcp`
* `openthread/ot_br`
* `openthread/ot_trel`
* sleepy-device examples  
Official source: [ESP-IDF Thread guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_openthread.html)

This is the key practical point for the roadmap: **ESP32-C6 can be used either as a full Thread node or as an RCP attached to a Linux host**. That makes it unusually good for study, because the same silicon can teach you both MCU-side Thread firmware and host-plus-radio architectures.

ESP-IDF also exposes the OpenThread lifecycle through functions such as:

* `esp_openthread_init(...)`
* `esp_openthread_auto_start(...)`
* `esp_openthread_launch_mainloop(...)`

These APIs show the embedded control flow clearly. First you initialize the platform and stack, then optionally start with a dataset, then enter the main loop that services radio events, timers, and protocol work.

---

**8. CLI, Spinel, and Host Control**

The OpenThread CLI is not just a demo shell. It is one of the fastest ways to understand what state the stack is in, how a node forms a network, and what role it currently holds.

For SoC designs, the CLI usually runs directly on the device. For host-plus-RCP designs, the host talks to the radio through **Spinel**, which OpenThread describes as the standard host-controller protocol used by both RCP and NCP designs.  
Official source: [Co-Processor Designs](https://openthread.io/platforms/co-processor)

When the host side is POSIX or Linux, **OpenThread Daemon (`ot-daemon`)** is the lightweight service wrapper around this model. The OpenThread docs show that it runs as a service, exposes a UNIX socket, and can be controlled with `ot-ctl`. This is the cleanest way to understand a Linux host talking to a serial Thread radio without bringing in the full Border Router stack immediately.  
Official source: [OT Daemon](https://openthread.io/platforms/co-processor/ot-daemon)

**What `dataset` commands actually are**

In OpenThread CLI, a **dataset** is the bundle of parameters that defines a Thread network. The official dataset reference says Thread network configuration is managed using **Active** and **Pending Operational Dataset** objects.  
Official source: [Display and Manage Datasets with OT CLI](https://openthread.io/reference/cli/concepts/dataset)

The Active Operational Dataset contains the parameters that are actually in use across the network, including items such as:

* channel
* extended PAN ID
* mesh-local prefix
* network name
* PAN ID
* PSKc
* security policy

That means dataset commands are not random helper commands. They are how you inspect or construct the network identity and credentials that define a Thread mesh.

</details>

#### `ot_cli` 中通用的 dataset 流程

在实验室中创建一个全新的独立 Thread 网络时，常见顺序是：

```bash
dataset init new
dataset
dataset commit active
ifconfig up
thread start
state
```

OpenThread 的 on-mesh commissioning 指南展示的正是这一模式：`dataset init new`，检查生成的值，`dataset commit active`，然后拉起接口并启动 Thread。  
官方来源：[On-Mesh Commissioning](https://openthread.io/guides/build/commissioning)

这些命令的含义：

* `dataset init new`：用新生成的网络参数创建一个全新的工作 dataset
* `dataset`：在 CLI 缓冲区中显示当前 dataset 的内容
* `dataset commit active`：把该 dataset 保存为 Active Operational Dataset
* `ifconfig up`：拉起 IPv6 接口
* `thread start`：真正开始在 mesh 中参与协议
* `state`：显示该节点是否已成为 `leader`、`router`、`child` 等

重要的心智模型是：dataset 是**网络定义**，而 `thread start` 则是实际加入该网络的行为。

#### 为什么这在生产环境中很重要

dataset 参考文档中包含一条重要警告：通过 CLI 直接编辑 Active 或 Pending Operational Dataset，主要面向新网络中的第一台设备或测试用途。在生产系统中，dataset 的变更应通过正规的 commissioning 与管理机制来完成，而不是随意的本地编辑。  
官方来源：[Display and Manage Datasets with OT CLI](https://openthread.io/reference/cli/concepts/dataset)

对学习者来说这是一个有用的区分。CLI 的 dataset 命令非常适合理解协议、组建实验网络，但真实产品不应把每一台现场设备都当作不受限制的网络管理员 shell。


<details>
<summary>English original</summary>

**Common dataset flow in `ot_cli`**

When you create a brand-new standalone Thread network in the lab, the common sequence is:

```bash
dataset init new
dataset
dataset commit active
ifconfig up
thread start
state
```

OpenThread's on-mesh commissioning guide shows exactly this pattern: `dataset init new`, inspect the generated values, `dataset commit active`, then bring the interface up and start Thread.  
Official source: [On-Mesh Commissioning](https://openthread.io/guides/build/commissioning)

What these commands mean:

* `dataset init new`: create a fresh working dataset with new generated network parameters
* `dataset`: display the current dataset contents in the CLI buffer
* `dataset commit active`: save that dataset as the Active Operational Dataset
* `ifconfig up`: bring up the IPv6 interface
* `thread start`: actually start protocol participation in the mesh
* `state`: show whether the node became `leader`, `router`, `child`, and so on

The important mental model is that the dataset is the **network definition**, while `thread start` is the act of actually participating in that network.

**Why this matters in production**

The dataset reference includes an important warning: directly editing Active or Pending Operational Datasets through CLI is mainly for the first device in a new network or for testing. In production systems, dataset changes should be managed through the proper commissioning and management mechanisms rather than arbitrary local edits.  
Official source: [Display and Manage Datasets with OT CLI](https://openthread.io/reference/cli/concepts/dataset)

That is a useful distinction for learners. CLI dataset commands are excellent for understanding the protocol and forming a lab network, but real products should not treat every field device like an unrestricted network-admin shell.

</details>

### 读取真实 `ot-ctl` 与 OTBR 状态快照

从独立的 `ot_cli` 节点转到 Linux 主机加 RCP 的设计之后，最有用的技能就是学会读取由以下内容组成的混合快照：

* `ot-ctl` 输出
* Thread IPv6 地址
* OTBR 日志行

例如，一次真实的 Jetson 加 ESP32-C6 RCP 会话可能显示：

```text
extaddr: fac5eb4acbada19e
rloc16: a400
ipaddr:
  fd3f:d825:5faf:9782:0:ff:fe00:a400
  fd3f:d825:5faf:9782:8b89:772a:39cd:737a
  fe80::f8c5:eb4a:cbad:a19e
state: detached
```

这类输出已经能告诉你很多信息：

* `extaddr` 是射频的 64 位 IEEE 802.15.4 身份标识。
* `rloc16` 是 Thread 网络内部使用的紧凑 16 位 mesh 定位符。
* 第一个 `fd...ff:fe00:a400` 地址是 **RLOC IPv6 地址**，由 mesh-local 前缀加上路由器定位符构成。
* 第二个 `fd...8b89:...` 地址是 **Mesh-Local EID**，即节点在 mesh 内部更稳定的 IPv6 身份。
* `fe80::...` 地址是普通的 IPv6 链路本地地址。

微妙之处在于，看到合法的 Thread 地址**并不**自动意味着节点已完全挂载。一个节点可以拥有 dataset、mesh-local 地址以及 Linux 接口状态，却仍报告 `detached`。

这就是状态机重要的原因：

* `disabled` 表示 Thread 协议栈存在但未启动。
* `detached` 表示协议栈已启动，正在尝试挂载或组建分区，但尚未进入 `leader`、`router` 或 `child` 这样的已挂载角色。
* `leader` 表示节点已挂载，并且当前正在管理自己的分区。

现在看对应的 OTBR 日志风格：

```text
Mle-----------: Send Link Request (ff02::2)
MeshForwarder-: Sent IPv6 UDP msg ... dst:[ff02::2]:19788
Settings------: Read NetworkInfo {rloc:0xa400, extaddr:..., role:leader, ...}
BorderAgent---: Registering service OpenThread BorderRouter #A19E _meshcop._udp
```

这些行直接对应前面章节中的 OpenThread 概念：

* `Mle-----------` 是 **Mesh Link Establishment** 控制平面在尝试发现或挂载到路由器。
* `MeshForwarder-` 表明射频路径确实在发送 Thread 控制流量。
* `Settings------` 表示 OpenThread 正在从非易失性存储读写持久化的网络状态。
* `BorderAgent--- ... _meshcop._udp` 表示面向 commissioning 的 Border Agent 服务正在注册。

有一行日志经常让人困惑：

```text
Settings------: Read NetworkInfo { ... role:leader, ... }
```

那一行描述的是**持久化状态**，而不是保证的当前实时状态。节点可以读回说明它之前曾是 leader 的旧信息，但此刻仍报告 `detached`，因为实时挂载流程尚未完成。

另一类值得理解的日志行是：

```text
P-Daemon------: Session socket is ready
P-Daemon------: Daemon read: Connection reset by peer
```

在正常的实验室工作流中，这通常只意味着 `ot-ctl` 这样的客户端连接到了 UNIX 控制套接字，然后退出了。它并不自动是射频问题的证据。

工程上的教训很简单：健康的 OpenThread 系统不能凭一行日志来诊断。你要读：

* CLI 状态，如 `disabled`、`detached` 或 `leader`
* 地址状态，如 `extaddr`、`rloc16` 和 `ipaddr`
* 控制平面日志行，如 `Mle-----------`
* 持久化日志行，如 `Settings------`

综合起来，这些信息能告诉你正在调试的是：

* 射频或传输链路失效
* dataset / 启动问题
* 已启动但未挂载的 attach 问题
* 还是已完全组建的分区

同样的读取方法在 Jetson RCP 路径中尤为重要，因为在那里 Linux、OTBR、Spinel 和射频协处理器各自贡献了可见系统状态的一部分。


<details>
<summary>English original</summary>

**Reading a real `ot-ctl` and OTBR state snapshot**

Once you move from a standalone `ot_cli` node to a Linux host plus RCP design, the most useful skill is learning how to read a mixed snapshot made from:

* `ot-ctl` output
* Thread IPv6 addresses
* OTBR log lines

For example, a real Jetson plus ESP32-C6 RCP session may show:

```text
extaddr: fac5eb4acbada19e
rloc16: a400
ipaddr:
  fd3f:d825:5faf:9782:0:ff:fe00:a400
  fd3f:d825:5faf:9782:8b89:772a:39cd:737a
  fe80::f8c5:eb4a:cbad:a19e
state: detached
```

This kind of output already tells you a lot:

* `extaddr` is the 64-bit IEEE 802.15.4 identity of the radio.
* `rloc16` is the compact 16-bit mesh locator used inside the Thread network.
* the first `fd...ff:fe00:a400` address is the **RLOC IPv6 address**, built from the mesh-local prefix plus the router locator.
* the second `fd...8b89:...` address is the **Mesh-Local EID**, the node's more stable IPv6 identity inside the mesh.
* the `fe80::...` address is the normal IPv6 link-local address.

The subtle point is that seeing valid Thread addresses does **not** automatically mean the node is fully attached. A node can have a dataset, mesh-local addresses, and Linux interface state while still reporting `detached`.

That is why the state machine matters:

* `disabled` means the Thread stack is present but not started.
* `detached` means the stack has been started and is trying to attach or form a partition, but it is not yet in an attached role such as `leader`, `router`, or `child`.
* `leader` means the node is attached and currently managing its own partition.

Now look at the matching OTBR log style:

```text
Mle-----------: Send Link Request (ff02::2)
MeshForwarder-: Sent IPv6 UDP msg ... dst:[ff02::2]:19788
Settings------: Read NetworkInfo {rloc:0xa400, extaddr:..., role:leader, ...}
BorderAgent---: Registering service OpenThread BorderRouter #A19E _meshcop._udp
```

These lines map directly back to the OpenThread concepts from earlier sections:

* `Mle-----------` is the **Mesh Link Establishment** control plane trying to discover or attach to routers.
* `MeshForwarder-` shows that the radio path is actually transmitting Thread control traffic.
* `Settings------` means OpenThread is reading or writing persisted network state from non-volatile storage.
* `BorderAgent--- ... _meshcop._udp` means the commissioning-facing Border Agent service is being registered.

One log line often confuses people:

```text
Settings------: Read NetworkInfo { ... role:leader, ... }
```

That line describes **persisted state**, not guaranteed current live state. A node can read back old information saying it was previously a leader and still report `detached` right now because the live attach procedure has not completed.

Another family of lines worth understanding is:

```text
P-Daemon------: Session socket is ready
P-Daemon------: Daemon read: Connection reset by peer
```

In a normal lab workflow, that often just means a client such as `ot-ctl` connected to the UNIX control socket and then exited. It is not automatically evidence of a radio problem.

The engineering lesson is simple: a healthy OpenThread system is not diagnosed from one line. You read:

* CLI state such as `disabled`, `detached`, or `leader`
* address state such as `extaddr`, `rloc16`, and `ipaddr`
* control-plane log lines such as `Mle-----------`
* persistence lines such as `Settings------`

Together, these tell you whether you are debugging:

* a dead radio or transport link
* a dataset / startup problem
* a live-but-detached attach problem
* or a fully formed partition

This same reading method becomes especially important in the Jetson RCP path, where Linux, OTBR, Spinel, and the radio coprocessor each contribute part of the visible system state.

</details>

### 当前已验证的 Jetson RCP 案例：Thread 成功，完整 OTBR 未成功

从 Jetson 加 ESP32-C6 RCP 这条路径中得到一个非常有用的实战教训：**Thread 成功与完整 OTBR 成功并不是同一个里程碑**。

在本路线图的已验证 Jetson 实验室运行中，最终出现的序列是：

```text
Role detached -> leader
Allocate router id 41
Partition ID 0x1a1d09c0
Route table ... me - leader
```


这几行极其重要。它们意味着：

* 主机通过 Spinel 成功与 RCP 通信
* MLE attach 逻辑完成
* 节点组建了自己的 partition
* 节点成为单节点 Thread 网络的 **leader**

从 OpenThread 协议的角度看，这是真正的成功。控制面工作正常，leader 选举工作正常，dataset 可用，节点从 `detached` 过渡到 attached 角色。

但紧接着，Linux 主机侧尝试启用更高级的 Border Router 行为，却遇到了：

```text
InitMulticastRouterSock() ... Protocol not available
```


该失败不是 Thread 状态失败，而是 **Linux 内核能力失败**。在本例中，Jetson 内核没有启用 multicast-routing 支持：

```text
# CONFIG_IP_MROUTE is not set
# CONFIG_IPV6_MROUTE is not set
```


这一区别对学习者非常重要：

* OpenThread 本身工作正常。
* ESP32-C6 RCP 路径工作正常。
* leader 形成与 partition 创建均工作正常。
* 完整的 OTBR border-routing 功能被主机内核配置阻塞。

这正是 host 加 RCP 调试必须分层解读的原因。一个系统可以：

* 作为 Thread 节点是健康的
* 作为 RCP 主机链路是健康的
* 但作为完整 Border Router 产品仍然被阻塞

实际上，这意味着即便当前内核镜像上无法使用完整的 OTBR border-routing 功能，Linux 主机仍然可以很好地配合 **`ot-daemon`** 使用，或用于协议学习。网络栈与 border-router 产品栈有重叠，但并不相同。

---

## 9. 安全性与可靠性

Thread 是为真实部署设计的，而非一次性的实验室链路。OpenThread 的公开描述强调可靠、安全、低功耗的设备到设备通信，且该栈包含 IEEE 802.15.4 MAC 安全、安全 commissioning 和 border-router 支持。  
官方来源：[OpenThread overview](https://openthread.io/)

需要理解的重点是，OpenThread 的安全是**分层的**，而不是单个勾选项。不同的保护应用在系统中的不同位置：

* **链路层保护：** IEEE 802.15.4 帧由 MAC 安全保护。
* **Commissioning 保护：** 新设备在加入 mesh 之前必须获得授权。
* **应用 / 端到端保护：** 应用流量可以使用更高层的安全协议，如基于 DTLS 的 Secure CoAP。

### 链路层安全：AES-CCM

OpenThread 的移植指南明确指出，OpenThread Security 使用 **AES-CCM** 密码学对 IEEE 802.15.4 或 MLE 消息进行加解密并校验其完整性。实际上，这意味着数据包不仅对随意查看不可见，还经过认证，因此被篡改的数据包可以被检测出来，而不是被静默信任。  
官方来源：[OpenThread advanced porting features](https://openthread.io/guides/porting/implement-advanced-features)

这对嵌入式开发者很重要，因为这里的密码学不是抽象的“云安全”。它取决于本地平台移植层在 entropy、nonce 和密钥处理上是否正确。如果 entropy 源很弱，或 PAL 实现草率，即便高层 Thread 代码正确，安全模型也会被削弱。

### 安全 commissioning：谁可以加入 mesh

Thread 不会仅仅因为设备在射频范围内就让任意设备加入。OpenThread 的 Border Agent API 文档指出，commissioner 候选者使用 **PSKc** 与 Border Agent 建立 **安全 DTLS 会话**，之后已连接的 commissioner 才能申请成为完整 commissioner。  
官方来源：[OpenThread Border Agent API](https://openthread.io/reference/group/api-border-agent)

在设备侧，OpenThread Commissioner API 和 OTBR 工具使用 **PSKd** 或 Joiner 凭据来授权特定的入网设备。实战教训很简单：加入网络是一个受控的准入过程，而不是广播式的“与附近任意设备配对”流程。这也是 Thread 适合真实智能家居与工业产品、而非仅限爱好者 mesh 链路的主要原因之一。  
官方来源：[Commissioner API](https://openthread.io/reference/group/api-commissioner), [OTBR PSKc tools](https://openthread.io/guides/border-router/tools)


<details>
<summary>English original</summary>

**Current validated Jetson RCP case: Thread succeeded, full OTBR did not**

One very useful real-world lesson from the Jetson plus ESP32-C6 RCP path is that **Thread success and full OTBR success are not the same milestone**.

In the validated Jetson lab run for this roadmap, the sequence eventually became:

```text
Role detached -> leader
Allocate router id 41
Partition ID 0x1a1d09c0
Route table ... me - leader
```

Those lines are extremely important. They mean:

* the host talked successfully to the RCP over Spinel
* MLE attach logic completed
* the node formed its own partition
* the node became **leader** of a one-node Thread network

From the OpenThread protocol point of view, that is a real success. The control plane worked, leader election worked, the dataset was usable, and the node transitioned out of `detached` into an attached role.

But immediately after that, the Linux-host side tried to bring up more advanced Border Router behavior and hit:

```text
InitMulticastRouterSock() ... Protocol not available
```

That failure is not a Thread-state failure. It is a **Linux kernel capability failure**. In this case, the Jetson kernel did not have multicast-routing support enabled:

```text
# CONFIG_IP_MROUTE is not set
# CONFIG_IPV6_MROUTE is not set
```

This distinction matters a lot for learners:

* OpenThread itself was functioning.
* The ESP32-C6 RCP path was functioning.
* Leader formation and partition creation were functioning.
* Full OTBR border-routing features were blocked by host-kernel configuration.

That is exactly why host-plus-RCP debugging must be read in layers. A system can be:

* healthy as a Thread node
* healthy as an RCP host link
* but still blocked as a full Border Router product

Practically, this means a Linux host may still be perfectly usable with **`ot-daemon`** or for protocol learning even when full OTBR border-routing features are unavailable on the current kernel image. The network stack and the border-router product stack overlap, but they are not identical.

---

**9. Security and Reliability**

Thread is designed for real deployments, not one-off lab links. OpenThread's public description emphasizes reliable, secure, low-power device-to-device communication, and the stack includes IEEE 802.15.4 MAC security, secure commissioning, and border-router support.  
Official source: [OpenThread overview](https://openthread.io/)

The important thing to understand is that OpenThread security is **layered**, not a single checkbox. Different protections apply at different points in the system:

* **Link-layer protection:** IEEE 802.15.4 frames are protected with MAC security.
* **Commissioning protection:** a new device must be authorized before it joins the mesh.
* **Application / end-to-end protection:** application traffic can use higher secure protocols such as DTLS-backed Secure CoAP.

**Link-layer security: AES-CCM**

OpenThread's porting guide explicitly states that OpenThread Security uses **AES-CCM** cryptography to encrypt and decrypt IEEE 802.15.4 or MLE messages and validate their integrity. In practice, that means packets are not just hidden from casual inspection; they are also authenticated so modified packets can be detected instead of silently trusted.  
Official source: [OpenThread advanced porting features](https://openthread.io/guides/porting/implement-advanced-features)

This matters for embedded developers because the crypto is not abstract "cloud security." It depends on the local platform port doing the right thing with entropy, nonces, and key handling. If your entropy source is weak or your PAL implementation is careless, the security model weakens even if the high-level Thread code is correct.

**Secure commissioning: who is allowed onto the mesh**

Thread does not let random devices join just because they are in radio range. OpenThread's Border Agent API documentation states that commissioner candidates establish **secure DTLS sessions** with the Border Agent using **PSKc**, and only then can a connected commissioner petition to become a full commissioner.  
Official source: [OpenThread Border Agent API](https://openthread.io/reference/group/api-border-agent)

On the device side, OpenThread Commissioner APIs and OTBR tools use **PSKd** or Joiner credentials to authorize specific joining devices. The practical lesson is simple: joining the network is a controlled admission process, not a broadcast "pair with anything nearby" flow. That is one of the main reasons Thread is suitable for real smart-home and industrial products instead of hobby-only mesh links.  
Official sources: [Commissioner API](https://openthread.io/reference/group/api-commissioner), [OTBR PSKc tools](https://openthread.io/guides/border-router/tools)

</details>

#### commissioning 流程在做什么

在系统层面，commissioning 回答了一个非常具体的问题：一个**尚未成为网络可信成员**的设备，如何在不把凭据以明文形式通过空中暴露的前提下，获得它所需的凭据？

整体流程是：

* Commissioner 被授权在该网络上行事
* Joiner 通过 Joiner 凭据来标识，例如 **PSKd**
* 使用 DTLS 保护准入会话
* 安全交换成功后，Joiner 收到所需的运行数据，随后即可进行正常的 Thread 附着

这种分离很重要。Joiner 并不是先被视为完整的 mesh 参与者、之后再加以保护。安全本身就是准入路径的一部分，这比常见的 IoT 反模式——先让设备临时接入、事后再试图把它锁死——要强得多。

commissioning 结束后，基于 MLE 的正常附着与控制面行为才能开始。正因如此，OpenThread 入门文档明确指出，只有在 Thread commissioning 提供网络凭据之后，MLE 才会继续进行。  
官方来源：[OpenThread Thread Primer: Network Discovery and Formation](https://openthread.io/guides/thread-primer/network-discovery)

有一个微妙之处值得说清楚，因为许多总结都弄错了：OpenThread 的 commissioning 指南说的是 **Commissioner 对 Joiner 进行认证**，但 Commissioner 本身并不“拥有”Thread 网络密钥，也不会把它作为应用密钥手工分发出去。它的职责是授权设备进入网络的安全 onboarding 流程。  
官方来源：[OpenThread on-mesh commissioning](https://openthread.io/guides/build/commissioning)

在外部 commissioning 流程中，Border Router、Border Agent、Commissioner、Joiner Router 和 Joiner 各自扮演不同角色。实用的心智模型是：mesh 之外的设备同样可以被安全地 commissioning，因为网络在受 DTLS 保护的交换周围提供了中继与授权机制，而不是要求尚未认证的 Joiner 先表现得像一个普通 mesh 节点。

#### PSKd 与 PSKc：人们通常混淆的区别

Thread onboarding 中会出现两个非常相似的名字，很容易混淆：

* **PSKd**：用于认证想要加入的设备的 **Joiner 凭据**
* **PSKc**：commissioning 建立阶段由 **Commissioner / 网络侧**使用的凭据

OpenThread 的外部 commissioning 指南指出，Joining Device Credential 也可称为 Joiner Password 或 **PSKd**，并且它可以与设备的 **EUI-64** 组合，生成唯一的 QR code。  
官方来源：[Prepare the Thread Network and Joiner Device](https://openthread.io/guides/border-router/external-commissioning/prepare)

这就是为什么两家公司不需要在制造阶段全局预共享密钥。制造 Thread 终端设备的公司 A 在出厂时随设备携带其自己的 Joiner 凭据与身份信息。制造边界路由器或生态应用的公司 B 只需实现标准的 Commissioner 流程，使安装人员在 onboarding 时能扫描或输入设备的凭据。

在真实的跨厂商部署中，面向用户的流程通常是：

1. 公司 A 为设备印上 QR code 或口令标签。
2. 安装人员在一个支持 Commissioner 的应用中扫描该码。
3. Commissioner 被授权接入 Thread 网络。
4. 使用其 **PSKd** 对 Joiner 进行认证。
5. Joiner 收到 Thread 网络凭据，然后正常附着。

这就是关键的互操作性要点：跨公司 onboarding 之所以可行，是因为**角色与凭据语义是标准化的**，而不是因为每个厂商都共享一个全局 PSKd 数据库。

### mesh 之上的端到端安全

Thread 内置的 mesh 保护并不能免除应用层安全的需要。OpenThread 的 Secure CoAP 文档表明，**Secure CoAP 使用 DTLS** 在对等端之间建立安全的端到端连接。  
官方来源：[Secure CoAP CLI concepts](https://openthread.io/reference/cli/concepts/coaps)

这一区分很重要。链路层安全保护无线跳与 mesh 传输，而 Secure CoAP 之类的协议保护应用会话本身。换言之，OpenThread 提供了安全的网络，但扎实的产品设计仍需在其之上叠加安全的应用协议。


<details>
<summary>English original</summary>

**What the commissioning flow is doing**

At a system level, commissioning answers a very specific question: how can a device that is **not yet a trusted member of the network** receive the credentials it needs without those credentials being exposed in plaintext over the air?

The broad flow is:

* a Commissioner is authorized to act on the network
* a Joiner is identified by Joiner credentials such as **PSKd**
* DTLS is used to protect the admission conversation
* after the secure exchange succeeds, the Joiner receives the operational data it needs and can then proceed to normal Thread attachment

This separation is important. The Joiner is not considered a full mesh participant first and secured later. Security is part of the admission path itself, which is much stronger than the common IoT anti-pattern of letting devices connect provisionally and trying to lock them down afterward.

Once commissioning finishes, normal MLE-based attach and control-plane behavior can begin. That is why the OpenThread primer explicitly notes that MLE only proceeds after Thread commissioning has provided network credentials.  
Official source: [OpenThread Thread Primer: Network Discovery and Formation](https://openthread.io/guides/thread-primer/network-discovery)

One subtle point is worth stating clearly because many summaries get it wrong: the OpenThread commissioning guide says the **Commissioner authenticates the Joiner**, but the Commissioner does **not** itself "own" or manually hand out the Thread network key as an application secret. Its job is to authorize admission into the network's secure onboarding flow.  
Official source: [OpenThread on-mesh commissioning](https://openthread.io/guides/build/commissioning)

In external commissioning flows, the Border Router, Border Agent, Commissioner, Joiner Router, and Joiner all play different roles. The practical mental model is that a device outside the mesh can still be securely commissioned because the network provides relay and authorization machinery around the DTLS-protected exchange, rather than expecting the unauthenticated Joiner to behave like a normal mesh node first.

**PSKd vs PSKc: the distinction people usually confuse**

Two very similar names show up in Thread onboarding and they are easy to mix up:

* **PSKd**: the **Joiner credential** used to authenticate the device that wants to join
* **PSKc**: the credential used by the **Commissioner / network side** during commissioning setup

OpenThread's external-commissioning guide states that the Joining Device Credential may also be called the Joiner Password or **PSKd**, and that it can be combined with the device's **EUI-64** to generate a unique QR code.  
Official source: [Prepare the Thread Network and Joiner Device](https://openthread.io/guides/border-router/external-commissioning/prepare)

This is why two companies do not need to pre-share secrets globally during manufacturing. Company A, which makes the Thread end device, ships the device with its own Joiner credential and identity information. Company B, which makes the border router or ecosystem app, only needs to implement the standard Commissioner flow so that the installer can scan or enter the device's credential at onboarding time.

In a real cross-vendor deployment, the user-facing flow is usually:

1. Company A prints a QR code or passphrase label for the device.
2. The installer scans that code in a Commissioner-capable app.
3. The Commissioner is authorized onto the Thread network.
4. The Joiner is authenticated using its **PSKd**.
5. The Joiner receives Thread network credentials and then attaches normally.

This is the key interoperability point: cross-company onboarding works because the **roles and credential semantics are standardized**, not because every vendor shares one global PSKd database.

**End-to-end security above the mesh**

Thread's built-in mesh protections do not remove the need for application-level security. OpenThread's Secure CoAP documentation shows that **Secure CoAP uses DTLS** to establish secure end-to-end connections between peers.  
Official source: [Secure CoAP CLI concepts](https://openthread.io/reference/cli/concepts/coaps)

That distinction is important. Link-layer security protects the radio hop and the mesh transport, while protocols such as Secure CoAP protect the application conversation itself. In other words, OpenThread gives you secure networking, but strong product design still requires secure application protocols on top.

</details>

### 关键材料、熵与存储

从嵌入式软件的角度看，有意思的地方在于：安全性不是「稍后在云里加上去」的。设备身份、网络准入、凭据存储和持久化设置从一开始就是固件设计的一部分。正因如此，OpenThread PAL 把 **熵** 和 **非易失性存储** 列为一等的平台需求。  
官方来源：[OpenThread PAL guide](https://openthread.io/guides/porting/implement-platform-abstraction-layer-apis)

PAL 指南明确指出，熵 API 用于维护网络的安全资产，包括 AES-CCM nonce 和其他随机值。这就是为什么许多量产平台把 OpenThread 与硬件随机数发生器、安全存储块或密码学加速器搭配使用：并非因为协议栈要求厂商锁定，而是因为这些特性提升了移植的安全质量。

### 可靠性与自愈行为

可靠性来自 mesh 行为本身。Thread 网络能够承受节点丢失、把合格设备提升为路由角色，并通过父节点保持 sleepy 端点挂接，这与简单的点对点无线链路非常不同。

这并不会让网络变得刀枪不入。真实部署仍然要考虑拒绝服务状况、糟糕的射频环境、复位行为，以及制造和更新过程中的凭据处理。但 OpenThread 的起点比 ad hoc 明文无线协议强得多，因为安全、入网调试和网络修复已经是架构的一部分。

---

## 10. 为什么 OpenThread 属于嵌入式软件

OpenThread 属于**嵌入式软件**，而不只是属于 Linux 或网络部分，因为该协议与固件关注点紧密耦合：

* 无线驱动的正确性
* 定时器精度
* ISR 与任务调度行为
* 持久化设置
* 电源状态与 sleepy 设备时序
* 用于 CLI 或 Spinel 的串行总线

如果你的 UART 路径丢字节，你的 RCP 主机设计就会失败。如果你的定时器或 alarm 实现有误，mesh 行为就会变得不稳定。如果你的功耗模型有误，你的 sleepy 节点要么耗电过多，要么掉出网络。

这正是那种把「我会写驱动」变成「我能做出联网嵌入式产品」的协议。

---

## 11. 与路线图其余部分的联系

| 路线图领域 | OpenThread 如何连接 |
|---|---|
| ARM MCU + CMSIS / HAL | 你需要在协议栈之下有可用的无线、定时器、UART/SPI、熵和设置驱动 |
| FreeRTOS | Thread 节点通常运行在基于 RTOS 的固件中，因此任务结构、事件循环、队列和功耗感知调度都很重要 |
| UART / SPI | 这些总线不仅用于传感器，还用于 CLI 和 Spinel 主机控制器链路 |
| 嵌入式 Linux | OTBR 和 `ot-daemon` 是 RCP 架构在 Linux 主机上的示例 |
| Jetson 部署 | Jetson 可以充当更强的 Host，而 ESP32-C6 充当 Thread 无线 |

因此，OpenThread 是纯 MCU 工作与 MCU 加 Linux 混合系统之间的天然桥梁。如果你把它理解透彻，就能更好地准备网关设计、边界路由器、Matter 风格协议栈和多处理器嵌入式产品。

---

## 12. 建议项目

### 项目 1：单板 CLI 节点

把 `ot_cli` 烧录到 ESP32-C6 上，并使用 CLI 组建一个单节点 Thread 网络。练习 `ifconfig up`、`thread start`、`state` 和 dataset 命令，直到你能自如地直接读取网络状态。

### 项目 2：双节点 mesh

启动两块支持 Thread 的板子，验证其中一块成为 Leader，另一块以 child 或 router 身份加入。在一台节点复位或断电重启时，观察路由角色和挂接行为如何变化。

### 项目 3：把 ESP32-C6 作为 Linux 主机的 RCP

烧录 `ot_rcp` 并通过 UART 或 SPI 将其连接到 Linux 主机。使用 `ot-daemon` 或 OTBR，确认主机创建了 `wpan0`、能够查询 `ot-ctl state`，并且能够启动一个 Thread dataset。

### 项目 4：带外部骨干网的边界路由器

使用 OTBR 把 Thread mesh 桥接到以太网、Wi-Fi 或由 USB 支撑的 Linux 网桥。正是在这里，OpenThread 不再只是一个无线实验，而成为真正的基础设施组件。

---

## References

### Official

* [OpenThread overview](https://openthread.io/)
* [OpenThread platforms](https://openthread.io/platforms)
* [OpenThread co-processor designs](https://openthread.io/platforms/co-processor)
* [OpenThread daemon](https://openthread.io/platforms/co-processor/ot-daemon)
* [OpenThread platform abstraction layer guide](https://openthread.io/guides/porting/implement-platform-abstraction-layer-apis)
* [ESP-IDF Thread / OpenThread guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_openthread.html)


<details>
<summary>English original</summary>

**Key material, entropy, and storage**

From an embedded-software perspective, the interesting part is that security is not "added later in the cloud." Device identity, network admission, credential storage, and persistent settings are part of the firmware design from the beginning. That is why the OpenThread PAL includes **entropy** and **non-volatile storage** as first-class platform requirements.  
Official source: [OpenThread PAL guide](https://openthread.io/guides/porting/implement-platform-abstraction-layer-apis)

The PAL guide explicitly calls out the entropy API as maintaining security assets for the network, including AES-CCM nonces and other random values. This is why many production platforms pair OpenThread with hardware random generators, secure storage blocks, or cryptographic accelerators: not because the stack requires vendor lock-in, but because those features improve the security quality of the port.

**Reliability and self-healing behavior**

Reliability comes from the mesh behavior itself. A Thread network can survive node loss, promote eligible devices into routing roles, and keep sleepy endpoints attached through parents, which is very different from a simple point-to-point radio link.

That does not make the network invulnerable. A real deployment still has to think about denial-of-service conditions, bad radio environments, reset behavior, and credential handling during manufacturing and updates. But OpenThread starts from a much stronger foundation than ad hoc plaintext radio protocols because security, commissioning, and network repair are already part of the architecture.

---

**10. Why OpenThread Belongs in Embedded Software**

OpenThread belongs in **Embedded Software**, not only in Linux or networking sections, because the protocol is tightly coupled to firmware concerns:

* radio driver correctness
* timer precision
* ISR and task scheduling behavior
* persistent settings
* power states and sleepy-device timing
* serial buses used for CLI or Spinel

If your UART path drops bytes, your RCP host design fails. If your timer or alarm implementation is wrong, the mesh behavior becomes unstable. If your power model is wrong, your sleepy node either burns too much current or falls off the network.

This is exactly the type of protocol that turns "I can write drivers" into "I can build a networked embedded product."

---

**11. Connection to the Rest of the Roadmap**

| Roadmap area | How OpenThread connects |
|---|---|
| ARM MCU + CMSIS / HAL | You need working radio, timer, UART/SPI, entropy, and settings drivers underneath the stack |
| FreeRTOS | Thread nodes often run in RTOS-based firmware, so task structure, event loops, queues, and power-aware scheduling matter |
| UART / SPI | These buses are used not just for sensors, but also for CLI and Spinel host-controller links |
| Embedded Linux | OTBR and `ot-daemon` are Linux-host examples of the RCP architecture |
| Jetson deployment | A Jetson can act as the stronger host while an ESP32-C6 acts as the Thread radio |

OpenThread is therefore a natural bridge between pure MCU work and mixed MCU-plus-Linux systems. If you understand it well, you are better prepared for gateway design, border routers, Matter-style stacks, and multi-processor embedded products.

---

**12. Suggested Projects**

**Project 1: Single-board CLI node**

Flash `ot_cli` onto an ESP32-C6 and use the CLI to form a one-node Thread network. Practice `ifconfig up`, `thread start`, `state`, and dataset commands until you are comfortable reading the network state directly.

**Project 2: Two-node mesh**

Bring up two Thread-capable boards and verify that one becomes Leader while the other joins as a child or router. Watch how the routing roles and attach behavior change as you reset or power-cycle one node.

**Project 3: ESP32-C6 as RCP for a Linux host**

Flash `ot_rcp` and connect it to a Linux host over UART or SPI. Use `ot-daemon` or OTBR and confirm that the host creates `wpan0`, can query `ot-ctl state`, and can start a Thread dataset.

**Project 4: Border router with external backbone**

Use OTBR to bridge the Thread mesh to Ethernet, Wi-Fi, or a USB-backed Linux bridge. This is where OpenThread stops being just a radio experiment and becomes a real infrastructure component.

---

**References**

**Official**

* [OpenThread overview](https://openthread.io/)
* [OpenThread platforms](https://openthread.io/platforms)
* [OpenThread co-processor designs](https://openthread.io/platforms/co-processor)
* [OpenThread daemon](https://openthread.io/platforms/co-processor/ot-daemon)
* [OpenThread platform abstraction layer guide](https://openthread.io/guides/porting/implement-platform-abstraction-layer-apis)
* [ESP-IDF Thread / OpenThread guide](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/network/esp_openthread.html)

</details>

### 路线图后续内容

* [ARM MCU、FreeRTOS 与通信协议](../../Guide.md)
* [Jetson ESP-Hosted 主机代码](../../../03-嵌入式Linux/01-Jetson-ESP-Hosted主机代码/Guide.md)
* [ESP32-C6 OpenThread RCP 运行于 Jetson Orin Nano](../../../../阶段4-路线B-Nvidia-Jetson/05-应用开发/02-网络与连接/ESP32-C6-OpenThread-RCP-Jetson-Orin-Nano.md)


<details>
<summary>English original</summary>

**Roadmap follow-ons**

* [ARM MCU, FreeRTOS, and Communication Protocols](../../Guide.md)
* [Jetson ESP-Hosted Host Code](../../../03-嵌入式Linux/01-Jetson-ESP-Hosted主机代码/Guide.md)
* [ESP32-C6 OpenThread RCP on Jetson Orin Nano](../../../../阶段4-路线B-Nvidia-Jetson/05-应用开发/02-网络与连接/ESP32-C6-OpenThread-RCP-Jetson-Orin-Nano.md)

</details>

---

> 原文：[`Phase 2 - Embedded Systems/2. Embedded Software/IoT/OpenThread/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/2.%20Embedded%20Software/IoT/OpenThread/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
