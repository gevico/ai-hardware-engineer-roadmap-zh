---
title: Guide
description: Guide
---

## Yocto Project — 专题课程

<div class="course-identity embedded-linux" markdown="1">
<div class="course-identity__icon">LNX</div>
<div markdown="1">
<p class="course-identity__eyebrow">模块 3 · 嵌入式 Linux</p>
<p class="course-identity__title">定制 kernel、根文件系统、设备树、BSP 以及量产 Linux 镜像。</p>
<p class="course-identity__meta">产物：可启动镜像或设备树补丁 · 度量：启动时间、probe 日志、占用空间</p>
</div>
</div>


如需完整的 **vendor-neutral** Yocto 课程体系（心智模型、模块 0–11、实验、capstone、术语表），请使用：

**[Yocto / Guide.md](02-Yocto/Guide.md)**

下面的提纲仍是路线图检查清单；Yocto 文件夹才是结构化的“课程”版本。

---


<details>
<summary>English original</summary>

**Yocto Project — dedicated course**

<div class="course-identity embedded-linux" markdown="1">
<div class="course-identity__icon">LNX</div>
<div markdown="1">
<p class="course-identity__eyebrow">Module 3 · Embedded Linux</p>
<p class="course-identity__title">Customize kernels, root filesystems, device trees, BSPs, and production Linux images.</p>
<p class="course-identity__meta">Artifact: bootable image or device-tree patch · Measure: boot time, probe logs, footprint</p>
</div>
</div>


For a full **vendor-neutral** Yocto curriculum (mental model, modules 0–11, labs, capstones, glossary), use:

**[Yocto / Guide.md](02-Yocto/Guide.md)**

The outline below remains a roadmap checklist; the Yocto folder is the structured “course” version.

---

</details>

## 实战 Linux 代码阅读课程

若想围绕真实 Jetson 主机驱动做一个具体的 **嵌入式 Linux** 案例研究，请使用：

**[Jetson ESP-Hosted Host Code / Guide.md](01-Jetson-ESP-Hosted主机代码/Guide.md)**

这是一个结构化的迷你课程，与 Yocto 材料并行，包含五节详细讲座，涵盖：

- Linux 如何看待主机栈
- Jetson 构建/加载路径与板级策略
- SPI、GPIO 与 IRQ 驱动的 bring-up（上电点亮/调通）
- `cfg80211` Wi-Fi 集成到 `wlan0`
- HCI/BLE 集成到 `hci0`

当你想要一次真正的 Linux 主机驱动代码阅读练习，而不是又一份通用嵌入式 Linux 检查清单时，使用它。

---

**阶段 1：嵌入式 Linux 开发（12-24 个月）**

**1. Yocto Project（超越基础）**

* **Yocto 架构与内部机制：**
    * **BitBake 与 OpenEmbedded：**  深入研究 BitBake——Yocto 背后的构建引擎，以及 OpenEmbedded——核心构建框架。理解它们如何协同工作，以创建自定义 Linux 发行版。
    * **recipe、layer 与元数据：**  掌握 recipe（单个软件包的构建指令）、layer（recipe 的集合）与元数据（配置和依赖信息）的概念。学习如何创建和修改它们，以定制构建。
    * **Yocto 构建过程：**  全面理解 Yocto 构建过程，从获取源代码到生成最终镜像。学习如何分析构建日志、排查错误并优化构建时间。

* **Yocto 高级定制：**
    * **自定义 BSP（板级支持包）开发：**  不要只停留在使用现有 BSP。学习如何为自定义硬件平台创建并维护自己的 BSP，包括编写设备驱动、配置 bootloader 以及集成 kernel 修改。
    * **使用 `menuconfig` 进行 kernel 配置：**  掌握 `menuconfig` 接口，用于配置 Linux 内核、启用和禁用功能，并针对特定硬件和应用需求进行定制。
    * **根文件系统定制：**  探索定制根文件系统的高级技术，包括添加和移除软件包、配置系统服务（systemd）以及针对大小和性能进行优化。


<details>
<summary>English original</summary>

**Worked Linux code-reading course**

For a concrete **Embedded Linux** case study built around a real Jetson host driver, use:

**[Jetson ESP-Hosted Host Code / Guide.md](01-Jetson-ESP-Hosted主机代码/Guide.md)**

This is a structured mini-course, parallel to the Yocto material, with five detailed lectures covering:

- how Linux sees the host stack
- the Jetson build/load path and board policy
- SPI, GPIO, and IRQ-driven bring-up
- `cfg80211` Wi-Fi integration into `wlan0`
- HCI/BLE integration into `hci0`

Use it when you want a real Linux host-driver reading exercise rather than another generic Embedded Linux checklist.

---

**Phase 1: Embedded Linux Development (12-24 months)**

**1. Yocto Project (Beyond the Basics)**

* **Yocto Architecture and Internals:**
    * **BitBake and OpenEmbedded:**  Dive deep into BitBake, the build engine behind Yocto, and OpenEmbedded, the core build framework. Understand how they work together to create your custom Linux distributions.
    * **Recipes, Layers, and Metadata:**  Master the concepts of recipes (build instructions for individual packages), layers (collections of recipes), and metadata (configuration and dependency information). Learn how to create and modify them to customize your builds.
    * **Yocto Build Process:**  Gain a thorough understanding of the Yocto build process, from fetching source code to generating the final images. Learn how to analyze build logs, troubleshoot errors, and optimize build times.

* **Advanced Yocto Customization:**
    * **Custom BSP (Board Support Package) Development:**  Go beyond using existing BSPs. Learn how to create and maintain your own BSPs for custom hardware platforms, including writing device drivers, configuring bootloaders, and integrating kernel modifications.
    * **Kernel Configuration with `menuconfig`:**  Master the `menuconfig` interface for configuring the Linux kernel, enabling and disabling features, and tailoring it to your specific hardware and application requirements.
    * **Root File System Customization:**  Explore advanced techniques for customizing the root file system, including adding and removing packages, configuring system services (systemd), and optimizing for size and performance.

</details>

* **Yocto 进阶应用：**
    * **用 Yocto 构建实时系统：**  学习如何使用 Yocto 构建实时 Linux 系统，集成实时补丁（PREEMPT_RT）并为实时性能配置内核。
    * **用 Yocto 进行安全加固：**  探索 Yocto 中的安全特性，如安全启动、镜像签名和访问控制，以构建安全的嵌入式 Linux 发行版。
    * **Yocto 多平台开发：**  学习如何使用 Yocto 为不同处理器架构（如 ARM、x86）和硬件平台创建镜像，从而开发可移植、易适配的嵌入式系统。

**2. PetaLinux（精通 Xilinx 专用工具）**

* **PetaLinux 工作流与集成：**
    * **PetaLinux 工程创建与配置：**  掌握 PetaLinux 工作流，从创建新工程到配置硬件平台、内核和根文件系统。
    * **将自定义硬件集成到 PetaLinux：**  学习如何将自定义 FPGA 设计和外设与 PetaLinux 集成，包括创建设备树描述和编写设备驱动。
    * **PetaLinux 与 Yocto：**  理解 PetaLinux 与 Yocto 之间的关系，以及如何在 PetaLinux 环境中利用 Yocto 的特性和定制选项。

* **PetaLinux 高级定制：**
    * **内核与设备树定制（高级）：**  深入 PetaLinux 中的内核与设备树定制，包括使用设备树生成器（DTG）和定制设备树源文件。
    * **根文件系统定制（高级）：**  探索在 PetaLinux 中定制根文件系统的高级技术，包括添加自定义软件包、配置系统服务，以及针对体积和性能进行优化。
    * **PetaLinux 工程的调试与问题排查：**  学习如何调试和排查在 PetaLinux 构建过程中或运行嵌入式 Linux 系统时可能出现的问题。

* **PetaLinux 高级应用：**
    * **面向 Zynq UltraScale+ MPSoC 的 PetaLinux：**  掌握使用 PetaLinux 在 Zynq UltraScale+ MPSoC 上开发嵌入式 Linux 系统，充分利用其异构处理能力和高级特性。
    * **PetaLinux 在工业与汽车领域的应用：**  探索 PetaLinux 在工业和汽车系统中的应用，这些领域对实时性能、可靠性和安全性有着严苛要求。


<details>
<summary>English original</summary>

* **Yocto for Advanced Applications:**
    * **Real-Time Systems with Yocto:**  Learn how to build real-time Linux systems using Yocto, incorporating real-time patches (PREEMPT_RT) and configuring the kernel for real-time performance.
    * **Security Hardening with Yocto:**  Explore security features in Yocto, such as secure boot, image signing, and access control, to build secure embedded Linux distributions.
    * **Yocto for Multi-Platform Development:**  Learn how to use Yocto to create images for different processor architectures (e.g., ARM, x86) and hardware platforms, enabling you to develop portable and adaptable embedded systems.

**2. PetaLinux (Mastering Xilinx-Specific Tools)**

* **PetaLinux Workflow and Integration:**
    * **PetaLinux Project Creation and Configuration:**  Master the PetaLinux workflow, from creating new projects to configuring the hardware platform, kernel, and root file system.
    * **Integrating Custom Hardware with PetaLinux:**  Learn how to integrate your custom FPGA designs and peripherals with PetaLinux, including creating device tree descriptions and writing device drivers.
    * **PetaLinux and Yocto:**  Understand the relationship between PetaLinux and Yocto, and how you can leverage Yocto's features and customization options within the PetaLinux environment.

* **Advanced PetaLinux Customization:**
    * **Kernel and Device Tree Customization (Advanced):**  Dive deeper into kernel and device tree customization within PetaLinux, including using the Device Tree Generator (DTG) and customizing device tree source files.
    * **Root File System Customization (Advanced):**  Explore advanced techniques for customizing the root file system in PetaLinux, including adding custom packages, configuring system services, and optimizing for size and performance.
    * **Debugging and Troubleshooting PetaLinux Projects:**  Learn how to debug and troubleshoot issues that may arise during the PetaLinux build process or when running your embedded Linux system.

* **PetaLinux for Advanced Applications:**
    * **PetaLinux for Zynq UltraScale+ MPSoC:**  Master the use of PetaLinux for developing embedded Linux systems on the Zynq UltraScale+ MPSoC, leveraging its heterogeneous processing capabilities and advanced features.
    * **PetaLinux for Industrial and Automotive Applications:**  Explore the application of PetaLinux in industrial and automotive systems, where real-time performance, reliability, and safety are critical.

</details>

**3. 内核配置（微调 Linux 的心脏）**

* **内核配置（深入剖析）：**
    * **内核构建系统（kbuild）：**  理解 Linux 内核构建系统（kbuild）以及它如何编译和链接内核源代码。
    * **内核配置选项：**  探究种类繁多的内核配置选项，理解它们对内核大小、功能与性能的影响。
    * **内核模块：**  学习如何构建并加载内核模块，以动态扩展 Linux 内核的功能。

* **面向嵌入式系统的内核定制：**
    * **缩减内核大小：**  掌握缩减 Linux 内核大小的技术，例如移除不必要的驱动和特性，以及使用内核压缩。
    * **优化内核性能：**  探究用于优化内核性能的内核配置选项与技术，包括调整调度参数、高效管理内存以及启用实时特性。
    * **内核安全加固：**  学习如何为内核配置安全特性，例如访问控制列表（ACL）、安全启动和内核地址空间布局随机化（KASLR）。

**4. 根文件系统创建（构建基石）**

* **根文件系统要点：**
    * **文件系统层级：**  理解 Linux 文件系统层级（例如 `/`、`/bin`、`/etc`、`/dev`）以及各目录的用途。
    * **必需的文件与目录：**  了解根文件系统中的必需文件和目录，例如用于用户账户的 `/etc/passwd` 文件、用于组的 `/etc/group` 文件，以及用于挂载文件系统的 `/etc/fstab` 文件。
    * **init 系统与 systemd：**  探究 init 系统（systemd）在启动和管理系统服务与进程中的作用。

* **构建最小根文件系统：**
    * **Busybox：**  学习如何使用 Busybox（一套轻量的 Linux 必备工具集）来创建最小根文件系统。
    * **根文件系统工具：**  探究 `genimage` 和 `mkfs` 这类工具，用于创建和格式化不同的文件系统类型（例如 ext4、squashfs）。
    * **定制根文件系统：**  学习如何通过为嵌入式应用添加必要的库、工具和配置文件来定制根文件系统。

**资源：**


<details>
<summary>English original</summary>

**3. Kernel Configuration (Fine-Tuning the Heart of Linux)**

* **Kernel Configuration (Deep Dive):**
    * **Kernel Build System (kbuild):**  Understand the Linux kernel build system (kbuild) and how it compiles and links the kernel source code.
    * **Kernel Configuration Options:**  Explore the vast array of kernel configuration options, understanding their impact on kernel size, functionality, and performance.
    * **Kernel Modules:**  Learn how to build and load kernel modules to dynamically extend the functionality of the Linux kernel.

* **Kernel Customization for Embedded Systems:**
    * **Reducing Kernel Size:**  Master techniques for reducing the size of the Linux kernel, such as removing unnecessary drivers and features, and using kernel compression.
    * **Optimizing Kernel Performance:**  Explore kernel configuration options and techniques to optimize kernel performance, including tuning scheduling parameters, managing memory efficiently, and enabling real-time features.
    * **Security Hardening the Kernel:**  Learn how to configure the kernel with security features, such as access control lists (ACLs), secure boot, and kernel address space layout randomization (KASLR).

**4. Root File System Creation (Building the Foundation)**

* **Root File System Essentials:**
    * **File System Hierarchy:**  Understand the Linux file system hierarchy (e.g., `/`, `/bin`, `/etc`, `/dev`) and the purpose of different directories.
    * **Essential Files and Directories:**  Learn about essential files and directories in the root file system, such as the `/etc/passwd` file for user accounts, the `/etc/group` file for groups, and the `/etc/fstab` file for mounting file systems.
    * **Init System and Systemd:**  Explore the role of the init system (systemd) in starting and managing system services and processes.

* **Building a Minimal Root File System:**
    * **Busybox:**  Learn how to use Busybox, a lightweight set of essential Linux utilities, to create a minimal root file system.
    * **Root File System Tools:**  Explore tools like `genimage` and `mkfs` for creating and formatting different file system types (e.g., ext4, squashfs).
    * **Customizing the Root File System:**  Learn how to customize the root file system by adding necessary libraries, utilities, and configuration files for your embedded application.

**Resources:**

</details>

* **"Embedded Linux Development with Yocto Project"，Rudolf Streif 著：**  Yocto Project 的全面指南，涵盖其特性、工作流与定制选项。
* **"Linux Kernel Development"，Robert Love 著：**  详解 Linux 内核内部机制与内核开发的著作，包括内核配置与驱动开发。
* **"Building Embedded Linux Systems"，Karim Yaghmour 著：**  构建嵌入式 Linux 系统的实用指南，涵盖从 bootloader 到根文件系统的各个方面。

**项目：**

* **用 Yocto 创建自定义 Linux 发行版：**  为你的 Zynq 板构建一套定制的 Linux 发行版，包含特定的软件包集合、内核配置以及量身定制的根文件系统。
* **把现有 Linux 应用移植到你的嵌入式平台：**  将某个 Linux 应用（例如 web 服务器、媒体播放器）移植到你的嵌入式平台，使其适配特定的硬件与软件环境。
* **构建安全的嵌入式 Linux 系统：**  在嵌入式 Linux 系统中实现用户认证、访问控制、加密等安全特性，以保护系统免受未授权访问与数据泄露。

你说得对，那就更深入地探索嵌入式 Linux 开发的世界吧！接下来会探讨更专门的领域、更高级的技术与业界最佳实践，让你成为该领域真正的专家。

**阶段 2（大幅扩充）：嵌入式 Linux 开发（18-36 个月）**

**1. 构建嵌入式 Linux 系统（高级）**


<details>
<summary>English original</summary>

* **"Embedded Linux Development with Yocto Project" by Rudolf Streif:**  A comprehensive guide to the Yocto Project, covering its features, workflows, and customization options.
* **"Linux Kernel Development" by Robert Love:**  A detailed book on Linux kernel internals and development, including kernel configuration and driver development.
* **"Building Embedded Linux Systems" by Karim Yaghmour:**  A practical guide to building embedded Linux systems, covering various aspects from bootloader to root file system.

**Projects:**

* **Create a Custom Linux Distribution with Yocto:**  Build a customized Linux distribution for your Zynq board with a specific set of packages, kernel configurations, and a tailored root file system.
* **Port an Existing Linux Application to Your Embedded Platform:**  Port a Linux application (e.g., a web server, a media player) to your embedded platform, adapting it to the specific hardware and software environment.
* **Build a Secure Embedded Linux System:**  Implement security features like user authentication, access control, and encryption in your embedded Linux system to protect it from unauthorized access and data breaches.

You're right, let's dive even deeper into the world of Embedded Linux Development! We'll explore more specialized areas, advanced techniques, and industry best practices to make you a true expert in this field.

**Phase 2 (Significantly Expanded): Embedded Linux Development (18-36 months)**

**1. Building Embedded Linux Systems (Advanced)**

</details>

* **Yocto Project（精通之道）：**
    * **自定义 BSP（板级支持包）开发：**  不要局限于使用现有 BSP。学习如何为自定义硬件平台创建和维护自己的 BSP，包括编写设备驱动、配置 bootloader、集成 kernel 修改。这涉及理解硬件规格、编写底层代码，以及与硬件工程师协作。
    * **高级 Yocto 配置：**  深入 Yocto 配置文件（例如 `local.conf`、`bblayers.conf`），以微调构建过程、管理依赖，并针对特定硬件和软件需求进行优化。这包括掌握 BitBake 语法、理解变量继承，以及利用高级配置选项。
    * **Yocto 安全最佳实践：**  探讨 Yocto 构建中的安全考量，包括安全启动、代码签名和漏洞管理。学习如何将安全特性集成到嵌入式 Linux 发行版中。这涉及理解安全威胁、实施安全策略，以及使用漏洞分析和缓解工具。
    * **使用 Yocto 的多平台支持：**  学习如何使用 Yocto 为不同处理器架构（例如 ARM、x86）和硬件平台构建镜像，从而创建可移植且可适配的嵌入式系统。这涉及理解交叉编译、管理特定架构的配置，以及在多种硬件上进行测试。


<details>
<summary>English original</summary>

* **Yocto Project (Mastering the Art):**
    * **Custom BSP (Board Support Package) Development:**  Go beyond using existing BSPs. Learn how to create and maintain your own BSPs for custom hardware platforms, including writing device drivers, configuring bootloaders, and integrating kernel modifications. This involves understanding hardware specifications, writing low-level code, and collaborating with hardware engineers.
    * **Advanced Yocto Configuration:**  Dive deeper into Yocto configuration files (e.g., `local.conf`, `bblayers.conf`) to fine-tune the build process, manage dependencies, and optimize for specific hardware and software requirements. This includes mastering BitBake syntax, understanding variable inheritance, and utilizing advanced configuration options.
    * **Yocto Security Best Practices:**  Explore security considerations in Yocto builds, including secure boot, code signing, and vulnerability management. Learn how to integrate security features into your embedded Linux distributions. This involves understanding security threats, implementing security policies, and using tools for vulnerability analysis and mitigation.
    * **Multi-Platform Support with Yocto:**  Learn how to use Yocto to build images for different processor architectures (e.g., ARM, x86) and hardware platforms, enabling you to create portable and adaptable embedded systems. This involves understanding cross-compilation, managing architecture-specific configurations, and testing on diverse hardware.

</details>

* **Buildroot（进阶）：**
    * **Buildroot 定制：**  精通定制 Buildroot 配置、让 Linux 发行版精确贴合自身需求的方法。学习如何增删软件包、配置 kernel 选项以及集成自定义软件组件。这涉及理解 Buildroot 基础设施、管理软件包依赖以及编写自定义构建脚本。
    * **Buildroot 软件包管理：**  探索 Buildroot 中高级的软件包管理技术，包括创建自定义软件包、管理依赖以及解决冲突。这涉及理解软件包元数据、使用软件包管理工具以及解决构建问题。
    * **面向特定应用的 Buildroot：**  学习如何通过选择和配置相关软件包与特性，把 Buildroot 用于工业自动化、汽车、网络等特定应用领域。这涉及理解行业特定需求、集成领域专用软件以及针对性能与安全进行优化。

* **OpenEmbedded：**
    * **OpenEmbedded 基础：**  熟悉 OpenEmbedded，它是另一套强大的嵌入式 Linux 构建系统。理解其核心组件（recipe、layer、元数据）与构建流程。这涉及学习 OpenEmbedded 构建环境、理解其配置文件以及构建基础镜像。
    * **OpenEmbedded 定制：**  学习如何定制 OpenEmbedded 构建，为目标硬件创建裁剪后的 Linux 发行版。这涉及修改 recipe、添加 layer 以及配置构建选项。
    * **OpenEmbedded 与 BitBake：**  深入探究 OpenEmbedded 与 BitBake 之间的关系，理解 BitBake 如何解析 recipe 并执行构建流程。

**2.  kernel 与驱动开发（精通 kernel）**


<details>
<summary>English original</summary>

* **Buildroot (Beyond the Basics):**
    * **Buildroot Customization:**  Master the art of customizing Buildroot configurations to tailor the Linux distribution to your exact needs. Learn how to add and remove packages, configure kernel options, and integrate custom software components. This involves understanding the Buildroot infrastructure, managing package dependencies, and writing custom build scripts.
    * **Buildroot Package Management:**  Explore advanced package management techniques in Buildroot, including creating custom packages, managing dependencies, and resolving conflicts. This involves understanding package metadata, using package management tools, and resolving build issues.
    * **Buildroot for Specific Applications:**  Learn how to use Buildroot for specific application domains, such as industrial automation, automotive, and networking, by selecting and configuring relevant packages and features. This involves understanding industry-specific requirements, integrating domain-specific software, and optimizing for performance and security.

* **OpenEmbedded:**
    * **OpenEmbedded Fundamentals:**  Get familiar with OpenEmbedded, another powerful build system for embedded Linux. Understand its core components (recipes, layers, metadata) and build process. This involves learning the OpenEmbedded build environment, understanding its configuration files, and building basic images.
    * **OpenEmbedded Customization:**  Learn how to customize OpenEmbedded builds to create tailored Linux distributions for your target hardware. This involves modifying recipes, adding layers, and configuring build options.
    * **OpenEmbedded and BitBake:**  Dive deeper into the relationship between OpenEmbedded and BitBake, understanding how BitBake is used to parse recipes and execute the build process.

**2.  Kernel and Driver Development (Mastering the Kernel)**

</details>

* **kernel 内部机制（深入）：**
    * **内存管理：**  深入探究 Linux 内存管理的细节，包括虚拟内存、分页、内存分配和 slab 分配器。理解 kernel 如何管理物理内存与虚拟内存、如何处理缺页异常，以及如何向进程分配内存。
    * **进程调度：**  深入 Linux 进程调度器，理解不同的调度算法、进程优先级和实时调度。了解调度器如何决定下一个运行的进程、如何管理进程状态，以及如何处理上下文切换。
    * **文件系统：**  了解 Linux 支持的不同文件系统（如 ext4、squashfs、JFFS2）及其特性。探究如何为嵌入式应用选择合适的文件系统，需考虑读写性能、磨损均衡和功耗等因素。
    * **网络协议栈：**  理解 Linux 网络协议栈，包括 TCP/IP 协议、网络接口和 socket 编程。了解网络数据包如何被处理、网络接口如何被管理，以及如何使用 socket 开发网络应用。

* **高级驱动开发：**
    * **中断处理（进阶）：**  掌握高级中断处理技术，包括中断共享、线程化中断和中断延迟优化。理解如何处理来自多个设备的中断、如何设置中断优先级，以及如何将中断延迟降至最低以获得实时性能。
    * **DMA（进阶）：**  探究高级 DMA 技术，例如 scatter-gather DMA 和 DMA 引擎管理。了解如何使用 DMA 在设备与内存之间高效传输数据，从而降低 CPU 开销并提升系统性能。
    * **设备树绑定：**  学习如何编写设备树绑定，向 Linux kernel 描述自定义硬件。这涉及理解设备树语法、定义设备属性，以及将设备树与 kernel 构建系统集成。
    * **kernel 调试（进阶）：**  掌握高级 kernel 调试技术，包括使用 kernel 调试器（kgdb）、链路追踪工具（ftrace），以及分析 kernel 崩溃转储。学习如何定位并修复 kernel 缺陷、分析 kernel 行为，以及排查系统崩溃。

**3.  系统集成与优化（超越基础）**


<details>
<summary>English original</summary>

* **Kernel Internals (Deep Dive):**
    * **Memory Management:**  Explore the intricacies of Linux memory management, including virtual memory, paging, memory allocation, and the slab allocator. Understand how the kernel manages physical and virtual memory, handles page faults, and allocates memory to processes.
    * **Process Scheduling:**  Dive deeper into the Linux process scheduler, understanding different scheduling algorithms, process priorities, and real-time scheduling. Learn how the scheduler determines which process to run next, manages process states, and handles context switching.
    * **File Systems:**  Learn about different file systems supported by Linux (e.g., ext4, squashfs, JFFS2) and their characteristics. Explore how to choose the appropriate file system for your embedded application, considering factors like read/write performance, wear leveling, and power consumption.
    * **Networking Stack:**  Understand the Linux networking stack, including TCP/IP protocols, network interfaces, and socket programming. Learn how network packets are processed, how network interfaces are managed, and how to develop network applications using sockets.

* **Advanced Driver Development:**
    * **Interrupt Handling (Advanced):**  Master advanced interrupt handling techniques, including interrupt sharing, threaded interrupts, and interrupt latency optimization. Understand how to handle interrupts from multiple devices, prioritize interrupts, and minimize interrupt latency for real-time performance.
    * **DMA (Advanced):**  Explore advanced DMA techniques, such as scatter-gather DMA and DMA engine management. Learn how to efficiently transfer data between devices and memory using DMA, minimizing CPU overhead and improving system performance.
    * **Device Tree Bindings:**  Learn how to write device tree bindings to describe your custom hardware to the Linux kernel. This involves understanding the device tree syntax, defining device properties, and integrating your device tree with the kernel build system.
    * **Kernel Debugging (Advanced):**  Master advanced kernel debugging techniques, including using kernel debuggers (kgdb), tracing tools (ftrace), and analyzing kernel crash dumps. Learn how to identify and fix kernel bugs, analyze kernel behavior, and troubleshoot system crashes.

**3.  System Integration and Optimization (Beyond the Basics)**

</details>

* **启动流程与 U-Boot（高级）：**
    * **U-Boot 定制（高级）：**  深入 U-Boot 定制，包括添加新硬件支持、修改启动脚本、实现自定义命令。这涉及理解 U-Boot 架构、编写自定义 U-Boot 驱动，以及按具体需求配置启动流程。
    * **使用 U-Boot 实现安全启动：**  学习如何使用 U-Boot 实现安全启动，确保启动过程中只执行受信任的代码。这涉及理解安全启动概念、为安全启动配置 U-Boot，以及与安全硬件特性集成。
    * **使用 U-Boot 进行网络启动：**  探索 U-Boot 的网络启动能力，使嵌入式系统可通过网络启动。这涉及为网络启动配置 U-Boot、搭建 TFTP 服务器，以及通过网络传输 kernel 与根文件系统镜像。

* **根文件系统管理（高级）：**
    * **Initramfs：**  理解 initramfs（初始 RAM 文件系统）及其在 Linux 启动流程中的用途。学习如何创建和定制 initramfs 镜像。这涉及理解 initramfs 结构、向其中填充必要的文件与驱动，以及将其与启动流程集成。
    * **systemd（高级）：**  探索 systemd 的高级特性，如 unit 文件、服务依赖和资源管理。学习如何编写自定义 systemd unit 文件、管理服务依赖，以及为不同服务控制资源分配。
    * **根文件系统安全：**  在根文件系统中实施安全措施，如访问控制列表（ACL）、强制访问控制（MAC）和加密。这涉及理解不同的安全模型、配置访问控制策略，以及加密敏感数据。


<details>
<summary>English original</summary>

* **Boot Process and U-Boot (Advanced):**
    * **U-Boot Customization (Advanced):**  Dive deeper into U-Boot customization, including adding support for new hardware, modifying boot scripts, and implementing custom commands. This involves understanding the U-Boot architecture, writing custom U-Boot drivers, and configuring the boot process for your specific needs.
    * **Secure Boot with U-Boot:**  Learn how to implement secure boot using U-Boot, ensuring that only trusted code is executed during the boot process. This involves understanding secure boot concepts, configuring U-Boot for secure boot, and integrating with secure hardware features.
    * **Network Booting with U-Boot:**  Explore network booting capabilities in U-Boot, enabling you to boot your embedded system over the network. This involves configuring U-Boot for network booting, setting up a TFTP server, and transferring kernel and root file system images over the network.

* **Root File System Management (Advanced):**
    * **Initramfs:**  Understand the initramfs (initial RAM file system) and how it is used during the Linux boot process. Learn how to create and customize initramfs images. This involves understanding the initramfs structure, populating it with necessary files and drivers, and integrating it with the boot process.
    * **Systemd (Advanced):**  Explore advanced Systemd features, such as unit files, service dependencies, and resource management. Learn how to write custom systemd unit files, manage service dependencies, and control resource allocation for different services.
    * **Root File System Security:**  Implement security measures in the root file system, such as access control lists (ACLs), mandatory access control (MAC), and encryption. This involves understanding different security models, configuring access control policies, and encrypting sensitive data.

</details>

* **性能分析与优化（高级）：**
    * **性能剖析与链路追踪：**  使用高级剖析工具（例如 perf、ftrace）分析系统性能、识别瓶颈并优化关键代码路径。这涉及理解性能指标、采集性能数据，并对其进行分析以找出可改进之处。
    * **实时优化：**  探索针对实时性能优化嵌入式 Linux 系统的技术，包括使用实时内核（例如 PREEMPT_RT）和配置进程优先级。这涉及理解实时概念、为实时调度配置 kernel，以及为实时任务设定优先级。
    * **电源管理：**  了解嵌入式 Linux 中的电源管理技术，例如 CPU 频率调节、设备电源管理和挂起/恢复功能。这涉及理解电源管理状态、配置电源管理策略，以及针对低功耗进行优化。


<details>
<summary>English original</summary>

* **Performance Analysis and Optimization (Advanced):**
    * **Performance Profiling and Tracing:**  Use advanced profiling tools (e.g., perf, ftrace) to analyze system performance, identify bottlenecks, and optimize critical code paths. This involves understanding performance metrics, collecting performance data, and analyzing it to identify areas for improvement.
    * **Real-Time Optimization:**  Explore techniques for optimizing embedded Linux systems for real-time performance, including using real-time kernels (e.g., PREEMPT_RT) and configuring process priorities. This involves understanding real-time concepts, configuring the kernel for real-time scheduling, and prioritizing real-time tasks.
    * **Power Management:**  Learn about power management techniques in embedded Linux, such as CPU frequency scaling, device power management, and suspend/resume functionality. This involves understanding power management states, configuring power management policies, and optimizing for low power consumption.

</details>

---

> 原文：[`Phase 2 - Embedded Systems/3. Embedded Linux/Guide.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%202%20-%20Embedded%20Systems/3.%20Embedded%20Linux/Guide.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
