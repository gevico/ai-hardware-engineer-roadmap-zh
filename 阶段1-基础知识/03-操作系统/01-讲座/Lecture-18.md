---
title: 第 18 讲：字符设备驱动、中断驱动 I/O 与 V4L2
description: 第 18 讲：字符设备驱动、中断驱动 I/O 与 V4L2
---

# 第 18 讲：字符设备驱动、中断驱动 I/O 与 V4L2

## 概述

上一讲说明了内核如何发现硬件并把驱动绑定到设备。本讲讨论驱动在完成绑定后实际做什么：向用户空间暴露编程接口、实时响应硬件事件，以及为 sensor 到推理的流水线接入 V4L2 摄像头子系统。核心挑战在于设计这样一个接口：高效、在用户/内核边界上安全，并且能响应硬件事件，同时不把 CPU 浪费在忙等上。心智模型是一条事件驱动的流水线：硬件产生数据，中断处理程序唤醒等待中的用户空间读者，数据经由零拷贝缓冲区路径从 sensor 流向推理引擎。对 AI 硬件工程师而言，这些模式直接出现在每一个自定义加速器驱动、每一个摄像头流水线实现和每一条低延迟推理数据路径中。

---

## 字符设备驱动

**字符设备**在 `/dev/` 中暴露**类文件接口**。用户空间像操作普通文件一样对它执行 open、read、write 和 ioctl。底层上，每个操作都会分派到驱动定义的函数。这是**自定义 AI 加速器的标准接口**，也是 FPGA 控制面以及任何无法归入更专门子系统的硬件的标准接口。

```
Character Device: Kernel ↔ Userspace Interface

Userspace               Kernel (driver code)
─────────               ───────────────────
open("/dev/mydev0")  →  mydev_open()
read(fd, buf, n)     →  mydev_read()    ← blocks until data ready
write(fd, buf, n)    →  mydev_write()   ← sends data to device
ioctl(fd, CMD, arg)  →  mydev_ioctl()   ← device-specific control
mmap(fd, ...)        →  mydev_mmap()    ← map DMA buffer to user VA
poll(fd, ...)        →  mydev_poll()    ← report readiness for epoll
close(fd)            →  mydev_release() ← cleanup
```

### 注册

建立字符设备遵循固定步骤：

1. **分配设备号** —— 内核动态（首选）或静态地分配主/次设备号。
2. **初始化 cdev 结构** —— 把 file_operations 表关联到 cdev。
3. **把 cdev 加入内核** —— 使其生效；此后 `open()` 调用就可能到达。
4. **创建设备类** —— udev 用它自动创建 `/dev/` 节点。
5. **创建设备** —— 创建实际的 `/dev/mydev0` 节点。

```c
alloc_chrdev_region(&devno, 0, 1, "mydev");   // dynamic major/minor
cdev_init(&mydev->cdev, &mydev_fops);
cdev_add(&mydev->cdev, devno, 1);
cls = class_create(THIS_MODULE, "mydev");
device_create(cls, NULL, devno, NULL, "mydev0");   // creates /dev/mydev0
```

### struct file_operations

| 操作 | 签名 | 用途 |
|-----------|-----------|---------|
| `open` | `(inode, file)` | 分配每文件状态；检查权限 |
| `release` | `(inode, file)` | 释放每文件状态；刷新硬件 |
| `read` | `(file, buf, len, off)` | 把数据拷贝给用户；不可用时阻塞 |
| `write` | `(file, buf, len, off)` | 从用户拷贝数据；提交给设备 |
| `ioctl` / `unlocked_ioctl` | `(file, cmd, arg)` | 设备特定命令 |
| `mmap` | `(file, vma)` | 把设备/内核内存映射到用户虚拟地址 |
| `poll` | `(file, poll_table)` | 为 select/poll/epoll 报告就绪状态 |

---

## ioctl 接口

**ioctl**（I/O 控制）接口是设备特定命令的标准机制，这些命令不符合 read/write 模型。可以把它看作通过文件描述符发起的一次带类型的函数调用。

```c
#define MYDEV_IOC_MAGIC  'M'
#define MYDEV_RESET      _IO(MYDEV_IOC_MAGIC,  0)       // no arg
#define MYDEV_GET_STATUS _IOR(MYDEV_IOC_MAGIC, 1, u32)  // read from device
#define MYDEV_SET_CONFIG _IOW(MYDEV_IOC_MAGIC, 2, struct mydev_cfg) // write

static long mydev_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    struct mydev_cfg cfg;
    if (copy_from_user(&cfg, (void __user *)arg, sizeof(cfg)))
        return -EFAULT;   // never dereference user pointer directly
    // ...
}
```

`_IO`、`_IOR`、`_IOW` 宏把 **magic number、命令号、方向和参数大小**编码进一个 32 位值。内核利用这一编码自动**校验参数大小**。

- 绝不要在内核中直接解引用 `(void *)arg`；始终使用 `copy_from_user()` / `copy_to_user()`
- 出错时 `copy_*_user` 返回未拷贝的字节数；要检查返回值

> **常见陷阱：** 直接使用 `(struct foo *)arg`（不经过 `copy_from_user` 就转换用户指针）是一个严重的安全漏洞。该用户指针可能指向无效内存、未映射内存，或在检查与使用之间发生变化的内存（TOCTOU 竞争）。始终用 `copy_from_user()` 拷贝到内核侧栈变量。

---


<details>
<summary>English original</summary>

**Lecture 18: Character Drivers, Interrupt-Driven I/O & V4L2**

**Overview**

The previous lecture established how the kernel discovers hardware and binds drivers to devices. This lecture covers what a driver actually does once it is bound: exposing a programming interface to userspace, responding to hardware events in real time, and integrating with the V4L2 camera subsystem for sensor-to-inference pipelines. The core challenge is designing an interface that is efficient, safe across the user/kernel boundary, and responsive to hardware events without wasting CPU on busy-waiting. The mental model is an event-driven pipeline: hardware generates data, the interrupt handler wakes waiting userspace readers, and data flows through a zero-copy buffer path from sensor to inference engine. For an AI hardware engineer, these patterns appear directly in every custom accelerator driver, every camera pipeline implementation, and every low-latency inference data path.

---

**Character Device Driver**

A **character device** exposes a **file-like interface** in `/dev/`. Userspace opens, reads, writes, and issues ioctls on it just like a regular file. Under the hood, each operation dispatches to a driver-defined function. This is the **standard interface for custom AI accelerators**, FPGA control planes, and any hardware that does not fit a more specialized subsystem.

```
Character Device: Kernel ↔ Userspace Interface

Userspace               Kernel (driver code)
─────────               ───────────────────
open("/dev/mydev0")  →  mydev_open()
read(fd, buf, n)     →  mydev_read()    ← blocks until data ready
write(fd, buf, n)    →  mydev_write()   ← sends data to device
ioctl(fd, CMD, arg)  →  mydev_ioctl()   ← device-specific control
mmap(fd, ...)        →  mydev_mmap()    ← map DMA buffer to user VA
poll(fd, ...)        →  mydev_poll()    ← report readiness for epoll
close(fd)            →  mydev_release() ← cleanup
```

**Registration**

Setting up a character device follows a fixed sequence:

1. **Allocate device numbers** — the kernel assigns major/minor numbers dynamically (preferred) or statically.
2. **Initialize the cdev structure** — link the file_operations table to the cdev.
3. **Add cdev to the kernel** — make it live; `open()` calls can arrive after this point.
4. **Create a device class** — used by udev to create the `/dev/` node automatically.
5. **Create the device** — creates the actual `/dev/mydev0` node.

```c
alloc_chrdev_region(&devno, 0, 1, "mydev");   // dynamic major/minor
cdev_init(&mydev->cdev, &mydev_fops);
cdev_add(&mydev->cdev, devno, 1);
cls = class_create(THIS_MODULE, "mydev");
device_create(cls, NULL, devno, NULL, "mydev0");   // creates /dev/mydev0
```

**struct file_operations**

| Operation | Signature | Purpose |
|-----------|-----------|---------|
| `open` | `(inode, file)` | Allocate per-file state; check permissions |
| `release` | `(inode, file)` | Free per-file state; flush hardware |
| `read` | `(file, buf, len, off)` | Copy data to user; block if unavailable |
| `write` | `(file, buf, len, off)` | Copy data from user; submit to device |
| `ioctl` / `unlocked_ioctl` | `(file, cmd, arg)` | Device-specific commands |
| `mmap` | `(file, vma)` | Map device/kernel memory into user VA |
| `poll` | `(file, poll_table)` | Report readiness for select/poll/epoll |

---

**ioctl Interface**

The **ioctl** (I/O control) interface is the standard mechanism for device-specific commands that do not fit the read/write model. Think of it as a typed function call through a file descriptor.

```c
#define MYDEV_IOC_MAGIC  'M'
#define MYDEV_RESET      _IO(MYDEV_IOC_MAGIC,  0)       // no arg
#define MYDEV_GET_STATUS _IOR(MYDEV_IOC_MAGIC, 1, u32)  // read from device
#define MYDEV_SET_CONFIG _IOW(MYDEV_IOC_MAGIC, 2, struct mydev_cfg) // write

static long mydev_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
    struct mydev_cfg cfg;
    if (copy_from_user(&cfg, (void __user *)arg, sizeof(cfg)))
        return -EFAULT;   // never dereference user pointer directly
    // ...
}
```

The `_IO`, `_IOR`, `_IOW` macros encode the **magic number, command number, direction, and argument size** into a single 32-bit value. The kernel uses this encoding to **validate the argument size** automatically.

- Never dereference `(void *)arg` directly in kernel; always use `copy_from_user()` / `copy_to_user()`
- `copy_*_user` returns bytes not copied on error; check return value

> **Common Pitfall:** Using `(struct foo *)arg` directly (casting the user pointer without `copy_from_user`) is a critical security vulnerability. The user pointer may point to invalid memory, unmapped memory, or memory that changes between the check and the use (TOCTOU race). Always copy to a kernel-side stack variable using `copy_from_user()`.

---

</details>

## 驱动中的 mmap

**mmap** 文件操作把内核或设备内存直接映射到用户空间虚拟地址空间。这使得对 DMA 输出缓冲区的**零拷贝访问**成为可能——用户空间以内存速度读取推理结果，每次访问无需一次系统调用。

```c
static int mydev_mmap(struct file *f, struct vm_area_struct *vma)
{
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot); // for MMIO: disable caching
    return remap_pfn_range(vma, vma->vm_start,
                           phys_addr >> PAGE_SHIFT,          // physical frame number
                           vma->vm_end - vma->vm_start,      // mapping size
                           vma->vm_page_prot);
}
```

`pgprot_noncached()` 将被映射的页标记为 **uncached**——这对 MMIO 区域和 DMA 输出缓冲区至关重要，CPU 必须始终从 RAM 读取而不是从自己的缓存读取。

- `remap_pfn_range()`：映射连续的物理区间；用于 DMA 缓冲区和 MMIO
- `vm_insert_page()`：映射单个页；用于 vmalloc 支撑的缓冲区
- `vm_ops->fault`：访问时按需映射页；用于大块稀疏缓冲区

> **关键洞察：** `mmap` 消除了 `read()` 每帧一次系统调用的开销。无需为每个推理结果调用 `read()`，用户空间只需映射一次 DMA 输出缓冲区，即可直接读取结果。对于 60fps 摄像头流水线，这避免了每个缓冲区每秒 60 次系统调用——孤立看微不足道，但在多摄像头系统中跨多个缓冲区累加后就相当可观。

---

## 中断驱动的 I/O

轮询硬件以检查新数据是否就绪会**浪费 CPU 周期**并增加延迟。**中断驱动的 I/O** 让硬件在数据就绪时精确通知 CPU，使 CPU 在此期间可以做有用的工作。

```
Interrupt-Driven Data Flow:

Hardware (camera ISP / AI accelerator)
    │
    │  DMA transfer complete
    ▼
IRQ line asserted
    │
    ▼
CPU's interrupt controller (GIC/APIC)
    │
    ▼
ISR (top half): runs with interrupts disabled
    ├── read STATUS_REG to acknowledge IRQ
    ├── set dev->data_ready = true
    └── wake_up_interruptible(&dev->wait_q)
                │
                ▼
        blocked read() thread wakes up
            │
            ▼
        copies data to userspace, returns to application
```

```c
devm_request_irq(dev, irq, mydev_isr, IRQF_SHARED, "mydev", priv);

static irqreturn_t mydev_isr(int irq, void *data)
{
    struct mydev *dev = data;
    u32 status = readl(dev->base + STATUS_REG);      /* read and clear interrupt status */
    if (!(status & MY_IRQ_BIT)) return IRQ_NONE;     /* not our interrupt (shared IRQ line) */

    dev->data_ready = true;
    wake_up_interruptible(&dev->wait_q);   // unblock waiting readers
    return IRQ_HANDLED;
}
```

如果状态寄存器显示中断来自**共享同一条 IRQ 线的其他设备**，ISR 返回 `IRQ_NONE`。对于 `IRQF_SHARED` 中断线这是必需的——内核会调用所有已注册的处理程序，直到其中一个认领该中断。

### 上半部 / 下半部拆分

ISR 在执行所在的 CPU 上以**中断关闭**状态运行。耗时工作必须**推迟**到允许睡眠和阻塞的上下文：

| 机制 | 上下文 | 用途 |
|-----------|---------|-----|
| ISR（上半部） | 中断；不可睡眠 | 确认 IRQ；唤醒队列 |
| `tasklet_schedule()` | Softirq；不可睡眠 | 短的推迟工作 |
| `queue_work()` | 内核线程；可睡眠 | I/O、内存分配、耗时工作 |

> **常见陷阱：** 在 ISR（上半部）中执行 `kmalloc(GFP_KERNEL)` 或任何会睡眠的操作会导致内核 BUG：“BUG: scheduling while atomic”。ISR 代码应始终限制为读寄存器、设置标志和唤醒队列。任何可能睡眠的工作都通过 `queue_work()` 交给 workqueue。

---

## 等待队列

**等待队列**是连接 ISR 通知与阻塞式用户空间操作的同步原语。ISR 调用 `wake_up_interruptible()` 来唤醒在 `wait_event_interruptible()` 中等待的线程。

```c
wait_queue_head_t wq;
init_waitqueue_head(&wq);

// Producer (ISR): wake waiters
wake_up_interruptible(&wq);

// Consumer (read fop): sleep until condition
wait_event_interruptible(wq, dev->data_ready);
```

`wait_event_interruptible()` 宏会**检查条件**，为真则立即返回，为假则让线程睡眠。当 ISR 调用 `wake_up_interruptible()` 时，所有睡眠中的线程被唤醒并**重新检查条件**。

- 若收到信号，`wait_event_interruptible()` 返回 `-ERESTARTSYS`；调用者应向用户空间返回 `-EINTR`
- `wait_event_interruptible_timeout()`：为轮询回退添加截止时间

> **关键洞察：** `wait_event_interruptible()` 不只是一种睡眠——它在睡眠前原子地检查条件，以避免这样的竞态：ISR 在检查之后、睡眠之前触发并设置 `data_ready`。该宏的实现使用等待队列自旋锁来保证这种原子性。

---


<details>
<summary>English original</summary>

**mmap in Drivers**

The **mmap** file operation maps kernel or device memory directly into userspace virtual address space. This enables **zero-copy access** to DMA output buffers — userspace reads inference results at memory speed without a syscall per access.

```c
static int mydev_mmap(struct file *f, struct vm_area_struct *vma)
{
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot); // for MMIO: disable caching
    return remap_pfn_range(vma, vma->vm_start,
                           phys_addr >> PAGE_SHIFT,          // physical frame number
                           vma->vm_end - vma->vm_start,      // mapping size
                           vma->vm_page_prot);
}
```

`pgprot_noncached()` marks the mapped pages as **uncached** — essential for MMIO regions and DMA output buffers where the CPU must always read from RAM rather than its cache.

- `remap_pfn_range()`: maps contiguous physical range; used for DMA buffers and MMIO
- `vm_insert_page()`: maps individual pages; used for vmalloc-backed buffers
- `vm_ops->fault`: demand-map pages on access; used for large sparse buffers

> **Key Insight:** `mmap` eliminates the syscall-per-frame overhead of `read()`. Instead of calling `read()` for each inference result, userspace maps the DMA output buffer once and reads results directly. For a 60fps camera pipeline, this avoids 60 syscalls per second per buffer — negligible in isolation, but meaningful when multiplied across many buffers in a multi-camera system.

---

**Interrupt-Driven I/O**

Polling the hardware to check if new data is ready **wastes CPU cycles** and increases latency. **Interrupt-driven I/O** lets the hardware signal the CPU precisely when data is ready, allowing the CPU to do useful work in the meantime.

```
Interrupt-Driven Data Flow:

Hardware (camera ISP / AI accelerator)
    │
    │  DMA transfer complete
    ▼
IRQ line asserted
    │
    ▼
CPU's interrupt controller (GIC/APIC)
    │
    ▼
ISR (top half): runs with interrupts disabled
    ├── read STATUS_REG to acknowledge IRQ
    ├── set dev->data_ready = true
    └── wake_up_interruptible(&dev->wait_q)
                │
                ▼
        blocked read() thread wakes up
            │
            ▼
        copies data to userspace, returns to application
```

```c
devm_request_irq(dev, irq, mydev_isr, IRQF_SHARED, "mydev", priv);

static irqreturn_t mydev_isr(int irq, void *data)
{
    struct mydev *dev = data;
    u32 status = readl(dev->base + STATUS_REG);      /* read and clear interrupt status */
    if (!(status & MY_IRQ_BIT)) return IRQ_NONE;     /* not our interrupt (shared IRQ line) */

    dev->data_ready = true;
    wake_up_interruptible(&dev->wait_q);   // unblock waiting readers
    return IRQ_HANDLED;
}
```

The ISR returns `IRQ_NONE` if the status register shows the interrupt came from a **different device sharing the same IRQ line**. This is required for `IRQF_SHARED` interrupt lines — the kernel will call all registered handlers until one claims it.

**Top Half / Bottom Half Split**

ISRs run with **interrupts disabled** on the executing CPU. Long work must be **deferred** to a context where sleeping and blocking are permitted:

| Mechanism | Context | Use |
|-----------|---------|-----|
| ISR (top half) | Interrupt; no sleep | Acknowledge IRQ; wake queue |
| `tasklet_schedule()` | Softirq; no sleep | Short deferred work |
| `queue_work()` | Kernel thread; can sleep | I/O, memory alloc, long work |

> **Common Pitfall:** Performing `kmalloc(GFP_KERNEL)` or any sleeping operation in the ISR (top half) causes a kernel BUG: "BUG: scheduling while atomic." Always limit ISR code to register reads, flag sets, and queue wakeups. Delegate any work that may sleep to a workqueue via `queue_work()`.

---

**Wait Queues**

**Wait queues** are the synchronization primitive that connects ISR notifications to blocking userspace operations. The ISR calls `wake_up_interruptible()` to unblock threads waiting in `wait_event_interruptible()`.

```c
wait_queue_head_t wq;
init_waitqueue_head(&wq);

// Producer (ISR): wake waiters
wake_up_interruptible(&wq);

// Consumer (read fop): sleep until condition
wait_event_interruptible(wq, dev->data_ready);
```

The `wait_event_interruptible()` macro **checks the condition** and returns immediately if true, or puts the thread to sleep if false. When the ISR calls `wake_up_interruptible()`, all sleeping threads are woken and **re-check the condition**.

- `wait_event_interruptible()` returns `-ERESTARTSYS` if signal received; caller should return `-EINTR` to userspace
- `wait_event_interruptible_timeout()`: add deadline for polling fallback

> **Key Insight:** `wait_event_interruptible()` is not just a sleep — it atomically checks the condition before sleeping to avoid the race where the ISR fires and sets `data_ready` after the check but before the sleep. The macro's implementation ensures this atomicity using the wait queue spinlock.

---

</details>

## 驱动中的 poll() / select() / epoll

对多路复用多个设备（多摄像头、多路推理输出）的应用，按设备逐个阻塞 `read()` 并不现实。`poll` fop 把驱动接入 Linux 的 I/O 多路复用基础设施。

```c
static __poll_t mydev_poll(struct file *f, poll_table *wait)
{
    poll_wait(f, &dev->wait_q, wait);   // register wait queue with poll infrastructure
    if (dev->data_ready)
        return EPOLLIN | EPOLLRDNORM;   // signal: data available for reading
    return 0;                           // signal: not yet ready
}
```

`poll_wait()` 把驱动的等待队列注册到内核的 polling 基础设施上，**并不真正睡眠**。用户态调用 `epoll_wait()` 后，当在该等待队列上调用 `wake_up_interruptible()` 时，内核会唤醒 epoll 线程。

用户态 `epoll_wait()` 在 `EPOLLIN` 被置位时返回；从而支持事件驱动的数据流水线，无需忙轮询。

底层中断与轮询基础设施建立之后，V4L2 子系统在这些原语之上构建了标准化的摄像头采集框架。

---

## V4L2 (Video4Linux2)

**V4L2** 是面向**摄像头与视频采集设备**的内核子系统。它为所有摄像头硬件统一了帧采集、格式协商与缓冲区管理的 API。头文件：`<linux/videodev2.h>`。

```
V4L2 Pipeline Architecture

Camera Sensor (IMX477)
    │  I2C config, MIPI CSI-2 data
    ▼
NVCSI (MIPI CSI-2 receiver)
    │  raw pixel data
    ▼
ISP (Image Signal Processor)
    │  debayer, denoise, exposure, color correction
    ▼
V4L2 capture node (/dev/video0)
    │  DMABUF fd
    ▼
CUDA importer (GPU)
    │  inference kernel
    ▼
Output (display / network / storage)
```

### 采集流程

V4L2 采集流程遵循固定协议。每一步都对应一个特定的 ioctl：

1. **`VIDIOC_QUERYCAP`** — 查询驱动能力；确认其支持 streaming 与所需格式。
2. **`VIDIOC_S_FMT`** — 设置像素格式（例如 `V4L2_PIX_FMT_NV12`）、分辨率与 stride。
3. **`VIDIOC_REQBUFS`** — 分配 N 个内核侧 DMA 缓冲区；选择内存类型（MMAP 或 DMABUF）。
4. **`VIDIOC_QBUF`** — 将每个缓冲区入队；把所有权交给驱动以进行 DMA 填充。
5. **`VIDIOC_STREAMON`** — 启动硬件流水线；ISP（图像信号处理器）开始采集帧。
6. **`poll()` / `select()`** — 等待（不忙循环）某个缓冲区被填满。
7. **`VIDIOC_DQBUF`** — 出队一个已填充的缓冲区；驱动把所有权交还用户态。
8. **处理帧** — 执行推理、显示、编码或存储。
9. **`VIDIOC_QBUF`** — 重新入队该缓冲区，供下一帧使用。
10. **`VIDIOC_STREAMOFF`** — 停止硬件；所有已入队的缓冲区归还用户态。

```
VIDIOC_QUERYCAP     → verify capabilities
VIDIOC_S_FMT        → set pixel format, resolution
VIDIOC_REQBUFS      → allocate N buffers (MMAP or DMABUF)
VIDIOC_QBUF         → enqueue buffer (give to driver)
VIDIOC_STREAMON     → start streaming
poll() / select()   → wait for filled buffer
VIDIOC_DQBUF        → dequeue filled buffer; process frame
VIDIOC_QBUF         → re-enqueue for next frame
VIDIOC_STREAMOFF    → stop streaming
```

> **常见陷阱：** 在 `VIDIOC_DQBUF` 之后没有及时用 `VIDIOC_QBUF` 重新入队缓冲区。若用户态处理帧过慢导致队列耗尽，驱动便无缓冲区可填，只能丢帧。始终保证队列中至少有 2–3 个缓冲区；若处理较慢，采用生产者-消费者线程模型。

### 缓冲区内存类型

| 类型 | 说明 | 到 GPU 零拷贝？ |
|------|-------------|------------------|
| `V4L2_MEMORY_MMAP` | 内核分配；用户 mmap() | 否（需 CPU 拷贝） |
| `V4L2_MEMORY_USERPTR` | 用户分配；驱动 pin 住 | 有条件 |
| `V4L2_MEMORY_DMABUF` | 导入外部 DMA-BUF fd | 是（GPU 导入同一 buf） |

`V4L2_MEMORY_DMABUF` 是 **camera→GPU 零拷贝流水线**的关键。GPU 创建 DMA-BUF 缓冲区，将其导出为 fd，V4L2 驱动通过 DMA 直接填充它。GPU 随后从同一批物理页读取推理输入——数据全程不被拷贝。


<details>
<summary>English original</summary>

**poll() / select() / epoll in Drivers**

For applications that multiplex multiple devices (multiple cameras, multiple inference outputs), blocking `read()` per device is impractical. The `poll` fop integrates the driver with Linux's I/O multiplexing infrastructure.

```c
static __poll_t mydev_poll(struct file *f, poll_table *wait)
{
    poll_wait(f, &dev->wait_q, wait);   // register wait queue with poll infrastructure
    if (dev->data_ready)
        return EPOLLIN | EPOLLRDNORM;   // signal: data available for reading
    return 0;                           // signal: not yet ready
}
```

`poll_wait()` registers the driver's wait queue with the kernel's polling infrastructure **without actually sleeping**. When `epoll_wait()` is called in userspace, the kernel will wake the epoll thread when `wake_up_interruptible()` is called on this wait queue.

Userspace `epoll_wait()` returns when `EPOLLIN` is set; enables event-driven data pipelines without busy-polling.

With the low-level interrupt and polling infrastructure established, the V4L2 subsystem builds a standardized camera capture framework on top of these primitives.

---

**V4L2 (Video4Linux2)**

**V4L2** is the kernel subsystem for **cameras and video capture devices**. It standardizes the API for frame capture, format negotiation, and buffer management across all camera hardware. Header: `<linux/videodev2.h>`.

```
V4L2 Pipeline Architecture

Camera Sensor (IMX477)
    │  I2C config, MIPI CSI-2 data
    ▼
NVCSI (MIPI CSI-2 receiver)
    │  raw pixel data
    ▼
ISP (Image Signal Processor)
    │  debayer, denoise, exposure, color correction
    ▼
V4L2 capture node (/dev/video0)
    │  DMABUF fd
    ▼
CUDA importer (GPU)
    │  inference kernel
    ▼
Output (display / network / storage)
```

**Capture Sequence**

The V4L2 capture sequence follows a fixed protocol. Each step maps to a specific ioctl:

1. **`VIDIOC_QUERYCAP`** — query driver capabilities; verify it supports streaming and the required formats.
2. **`VIDIOC_S_FMT`** — set pixel format (e.g., `V4L2_PIX_FMT_NV12`), resolution, and stride.
3. **`VIDIOC_REQBUFS`** — allocate N kernel-side DMA buffers; choose memory type (MMAP or DMABUF).
4. **`VIDIOC_QBUF`** — enqueue each buffer; hand ownership to the driver for DMA fill.
5. **`VIDIOC_STREAMON`** — start the hardware pipeline; ISP begins capturing frames.
6. **`poll()` / `select()`** — wait (without busy-loop) for a buffer to be filled.
7. **`VIDIOC_DQBUF`** — dequeue a filled buffer; driver returns ownership to userspace.
8. **Process the frame** — run inference, display, encode, or store.
9. **`VIDIOC_QBUF`** — re-enqueue the buffer for the next frame.
10. **`VIDIOC_STREAMOFF`** — stop the hardware; all queued buffers returned to userspace.

```
VIDIOC_QUERYCAP     → verify capabilities
VIDIOC_S_FMT        → set pixel format, resolution
VIDIOC_REQBUFS      → allocate N buffers (MMAP or DMABUF)
VIDIOC_QBUF         → enqueue buffer (give to driver)
VIDIOC_STREAMON     → start streaming
poll() / select()   → wait for filled buffer
VIDIOC_DQBUF        → dequeue filled buffer; process frame
VIDIOC_QBUF         → re-enqueue for next frame
VIDIOC_STREAMOFF    → stop streaming
```

> **Common Pitfall:** Not re-enqueueing the buffer with `VIDIOC_QBUF` promptly after `VIDIOC_DQBUF`. If userspace processes the frame slowly and the queue runs dry, the driver has no buffer to fill and drops frames. Maintain a minimum of 2–3 buffers in the queue at all times, using a producer-consumer thread model if processing is slow.

**Buffer Memory Types**

| Type | Description | Zero-copy to GPU? |
|------|-------------|------------------|
| `V4L2_MEMORY_MMAP` | Kernel allocates; user mmap()s | No (CPU copy needed) |
| `V4L2_MEMORY_USERPTR` | User allocates; driver pins | Conditional |
| `V4L2_MEMORY_DMABUF` | Import external DMA-BUF fd | Yes (GPU imports same buf) |

`V4L2_MEMORY_DMABUF` is the key to **zero-copy camera→GPU pipelines**. The GPU creates a DMA-BUF buffer, exports it as an fd, and the V4L2 driver fills it directly via DMA. The GPU then reads inference input from the same physical pages — no data is ever copied.

</details>

### 媒体控制器

对于包含多个互连阶段（sensor → ISP → CSI → capture node）的复杂流水线，V4L2 的 **media controller** 将流水线表示为一张由实体和链路构成的有向图。

- `/dev/media0`：将整条流水线表示为实体图
- 实体：sensor → ISP → CSI bridge → V4L2 capture node
- `MEDIA_IOC_SETUP_LINK`：启用/禁用实体之间的数据通路
- `media-ctl` 工具：从 shell 配置流水线；查看实体属性

```
Media Controller Entity Graph (Jetson IMX477 example)

[IMX477 sensor]──MIPI──>[NVCSI]──pixel──>[ISP]──NV12──>[/dev/video0]
      │                                              │
      │ I2C (exposure, gain,                        │ DMABUF fd
      │ focus control)                              ▼
      │                                     CUDA inference kernel
      │
  /dev/v4l2-subdev0    /dev/media0 graph:  use media-ctl to enable links
```

### openpilot camerad 流水线

```
IMX sensors → NVCSI → ISP (NvCamSrc) → V4L2/ISP API
    --> frame buffer (DMA-BUF or nvmap)
    --> VisionIPC shared memory
    --> modeld (neural network inference)
```

这是 **openpilot 中的生产流水线**。帧从物理 sensor 出发，经 ISP，进入由 DMA-BUF 支撑的缓冲区，再通过 VisionIPC 共享给推理模型 —— **全程无 CPU 拷贝**。

> **关键要点：** V4L2 `VIDIOC_QBUF`/`VIDIOC_DQBUF` 缓冲区交换是摄像头流水线的同步协议。在 QBUF 与 DQBUF 之间，缓冲区归摄像头驱动所有；其余时间归用户空间所有。违反这一所有权归属（在驱动填充缓冲区时读取它）会导致撕裂帧和非确定性的损坏。

---

## 小结

| 驱动类型 | 关键 fops | 内核子系统 | 示例用途 |
|-------------|----------|-----------------|------------|
| 字符设备 | open/read/write/ioctl/mmap/poll | `cdev` | FPGA 控制、自定义加速器 |
| V4L2 采集 | VIDIOC_* ioctl | `videobuf2` | 摄像头 sensor、USB UVC |
| V4L2 + DMA-BUF | REQBUFS(DMABUF) + DQ/QBUF | `videobuf2` + `dma-buf` | Jetson ISP → GPU 流水线 |
| Platform + IRQ | ISR + waitqueue + poll | `platform_driver` | AI 加速器中断 |

### 概念回顾

- **为什么必须使用 `copy_from_user()`，而不能直接对 ioctl `arg` 指针做强制类型转换？** `arg` 是用户空间指针。在内核中直接解引用它是危险的：它可能未被映射、可能指向内核内存（提权），也可能在检查与使用之间发生变化（TOCTOU 竞态）。`copy_from_user()` 会校验该指针并将数据安全拷贝到内核空间。

- **中断处理中的上半部/下半部划分是什么？** ISR（上半部）在中断关闭的状态下运行，必须尽可能精简：确认 IRQ、记录状态、唤醒等待队列。更长的工作（内存分配、I/O、睡眠）会被推迟到 workqueue（下半部），它运行在内核线程上下文中，允许睡眠。

- **`wait_event_interruptible()` 如何避免漏唤醒竞态？** 该宏在等待队列锁内检查条件。如果 ISR 在条件检查与睡眠操作之间触发并设置 `data_ready`，等待队列机制会确保该线程要么看到条件为真而不进入睡眠，要么被 `wake_up_interruptible()` 调用立即唤醒。

- **`V4L2_MEMORY_MMAP` 与 `V4L2_MEMORY_DMABUF` 有什么区别？** 使用 MMAP 时，内核分配缓冲区并由用户空间映射 —— 但 GPU 无法在不发生拷贝的情况下直接导入它。使用 DMABUF 时，由外部分配器（例如 GPU 驱动）创建缓冲区并导出 fd；V4L2 驱动导入该缓冲区并通过 DMA 填充它。GPU 读取的是同一批物理页 —— 无需拷贝。

- **什么是 media controller，为什么摄像头流水线配置需要它？** media controller 是一张由流水线实体（sensor、ISP、CSI receiver、capture node）及其之间的链路构成的图。复杂的 ISP 具有多条输出通路和多个处理阶段。media controller API（`MEDIA_IOC_SETUP_LINK`）允许用户空间在不修改内核代码的情况下启用/禁用特定通路。

- **为什么在 mmap fop 中使用 `remap_pfn_range()`，而不是拷贝到用户空间？** `remap_pfn_range()` 在用户进程的页表中安装 PTE，直接指向 DMA 缓冲区的物理页。此后 CPU 可通过普通的指针解引用以内存速度读取这些页 —— 无系统调用、无拷贝、每次访问都不涉及内核。推理结果缓冲区正是以这种方式在全内存带宽下被读取。

---


<details>
<summary>English original</summary>

**Media Controller**

For complex pipelines with multiple linked stages (sensor → ISP → CSI → capture node), V4L2's **media controller** represents the pipeline as a directed graph of entities and links.

- `/dev/media0`: represents the full pipeline as an entity graph
- Entities: sensor → ISP → CSI bridge → V4L2 capture node
- `MEDIA_IOC_SETUP_LINK`: enable/disable data paths between entities
- `media-ctl` tool: configure pipeline from shell; inspect entity properties

```
Media Controller Entity Graph (Jetson IMX477 example)

[IMX477 sensor]──MIPI──>[NVCSI]──pixel──>[ISP]──NV12──>[/dev/video0]
      │                                              │
      │ I2C (exposure, gain,                        │ DMABUF fd
      │ focus control)                              ▼
      │                                     CUDA inference kernel
      │
  /dev/v4l2-subdev0    /dev/media0 graph:  use media-ctl to enable links
```

**openpilot camerad Pipeline**

```
IMX sensors → NVCSI → ISP (NvCamSrc) → V4L2/ISP API
    --> frame buffer (DMA-BUF or nvmap)
    --> VisionIPC shared memory
    --> modeld (neural network inference)
```

This is the **production pipeline in openpilot**. Frames flow from the physical sensor through the ISP, into DMA-BUF backed buffers, shared via VisionIPC to the inference model — with **no CPU copy at any stage**.

> **Key Insight:** The V4L2 `VIDIOC_QBUF`/`VIDIOC_DQBUF` buffer exchange is the synchronization protocol of the camera pipeline. The camera driver owns the buffer between QBUF and DQBUF; userspace owns it otherwise. Violating this ownership (reading a buffer while the driver is filling it) causes torn frames and non-deterministic corruption.

---

**Summary**

| Driver type | Key fops | Kernel subsystem | Example use |
|-------------|----------|-----------------|------------|
| Char device | open/read/write/ioctl/mmap/poll | `cdev` | FPGA control, custom accelerator |
| V4L2 capture | VIDIOC_* ioctls | `videobuf2` | Camera sensor, USB UVC |
| V4L2 + DMA-BUF | REQBUFS(DMABUF) + DQ/QBUF | `videobuf2` + `dma-buf` | Jetson ISP → GPU pipeline |
| Platform + IRQ | ISR + waitqueue + poll | `platform_driver` | AI accelerator interrupt |

**Conceptual Review**

- **Why must `copy_from_user()` be used instead of directly casting the ioctl `arg` pointer?** The `arg` is a user-space pointer. Dereferencing it directly in the kernel is dangerous: it may be unmapped, may point to kernel memory (privilege escalation), or may change between check and use (TOCTOU race). `copy_from_user()` validates the pointer and safely copies the data to kernel space.

- **What is the top half / bottom half split in interrupt handling?** The ISR (top half) runs with interrupts disabled and must be minimal: acknowledge the IRQ, record state, wake a wait queue. Longer work (memory allocation, I/O, sleeping) is deferred to a workqueue (bottom half) that runs in kernel thread context where sleeping is allowed.

- **How does `wait_event_interruptible()` avoid the missed-wakeup race?** The macro checks the condition inside the wait queue lock. If the ISR fires and sets `data_ready` between the condition check and the sleep operation, the wait queue machinery ensures the thread either sees the condition as true and does not sleep, or is woken immediately by the `wake_up_interruptible()` call.

- **What is the difference between `V4L2_MEMORY_MMAP` and `V4L2_MEMORY_DMABUF`?** With MMAP, the kernel allocates the buffer and userspace maps it — but it cannot be directly imported by a GPU without a copy. With DMABUF, an external allocator (e.g., the GPU driver) creates the buffer and exports an fd; the V4L2 driver imports it and fills it via DMA. The GPU reads the same physical pages — no copy needed.

- **What is the media controller and why is it needed for camera pipeline configuration?** The media controller is a graph of pipeline entities (sensor, ISP, CSI receiver, capture node) and links between them. Complex ISPs have multiple output paths and processing stages. The media controller API (`MEDIA_IOC_SETUP_LINK`) allows userspace to enable/disable specific paths without modifying kernel code.

- **Why is `remap_pfn_range()` used in the mmap fop instead of copying to userspace?** `remap_pfn_range()` installs PTEs in the user process's page table pointing directly to the physical pages of the DMA buffer. The CPU can then read these pages at memory speed through normal pointer dereference — no syscall, no copy, no kernel involvement per access. This is how inference result buffers can be read at full memory bandwidth.

---

</details>

## AI 硬件连接

- V4L2 配合 `V4L2_MEMORY_DMABUF`，可在 Jetson 上实现向 CUDA 零拷贝投递摄像头帧；缓冲区由 CUDA 导入，无需任何 CPU memcpy。
- openpilot camerad 使用 V4L2 / ISP（图像信号处理器）API 从 IMX 传感器采集帧，并通过 VisionIPC 把 DMA-BUF fd 传给 modeld 做推理。
- 自定义 ioctl 接口是 FPGA 控制平面的标准模式：配置 kernel 参数、触发 DMA、查询硬件状态。
- 驱动中的 `mmap` + `remap_pfn_range`，让用户态推理引擎能以内存速度读取 DMA 输出缓冲区，且每帧无 syscall 开销。
- 由传感器 ISR 触发 `wake_up_interruptible` 的等待队列，是通知推理线程 data-ready 事件、且 CPU 无忙等开销的正确模式。
- Jetson 摄像头 bring-up（上电点亮/调通）需要配置 media controller 流水线：传感器格式、ISP 处理各级与输出节点，都必须在开始流传输之前显式链接。


<details>
<summary>English original</summary>

**AI Hardware Connection**

- V4L2 with `V4L2_MEMORY_DMABUF` enables zero-copy camera frame delivery to CUDA on Jetson; buffer is imported by CUDA without any CPU memcpy.
- openpilot camerad uses the V4L2 / ISP API to capture frames from IMX sensors and passes DMA-BUF fds through VisionIPC to modeld for inference.
- Custom ioctl interface is the standard pattern for FPGA control plane: configure kernel parameters, trigger DMA, query hardware status.
- `mmap` in driver + `remap_pfn_range` enables userspace inference engines to read DMA output buffers at memory speed without syscall overhead per frame.
- Wait queues with `wake_up_interruptible` from the sensor ISR is the correct pattern for notifying inference threads of data-ready events with zero busy-wait CPU overhead.
- Media controller pipeline configuration is required for Jetson camera bring-up: sensor format, ISP processing stages, and output node must all be explicitly linked before streaming begins.

</details>

---

> 原文：[`Phase 1 - Foundational Knowledge/3. Operating Systems/Lectures/Lecture-18.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%201%20-%20Foundational%20Knowledge/3.%20Operating%20Systems/Lectures/Lecture-18.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
