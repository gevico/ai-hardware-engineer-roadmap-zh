---
title: 机密计算与硬件远程证明 — 深度解析
description: 机密计算与硬件远程证明 — 深度解析
---

# 机密计算与硬件远程证明 — 深度解析

本 HPC（高性能计算）方向的大部分内容都假设你掌控整个技术栈：你的集群、你的 NVLink fabric、你的 Slurm 队列。但只要算力是**租来的、共享的，或者运行在你并不物理掌控的节点上** —— GPU 云容量、多租户 HPC 集群，或由匿名运营者贡献节点的去中心化算力网络 —— 这个假设立刻失效。在那种环境里，一个新问题会先于任何性能问题冒出来：**你如何知道某个远程节点确实运行了你以为的那份工作负载、跑在真实硬件上、且未被篡改？**

这正是本篇深度解析要覆盖的内容：面向 CPU（Intel TDX）与 GPU（NVIDIA Confidential Computing）的**以硬件为根的远程证明**，以及 —— 因为单一厂商的校验就是一个单点伪造点 —— 如何**把同一条声明一次性绑定到两个硬件根上**，让验证者不必单独信任任何一家厂商的芯片。

## 为什么这属于 HPC 基础设施

本节其他每一篇深度解析（8x H200、Blackwell B200、GPUDirect Storage、NCCL）优化的都是你信任的集群。本篇回答的则是另一个正交问题，它会在节点脱离你的物理管控的那一刻出现：

```text
Trusted cluster (rest of HPC Setup):        Untrusted/rented/decentralized node (this deep dive):
  "how fast can this run?"                    "did this actually run, on real hardware,
                                                unmodified, and can I prove it without
                                                re-executing the whole job?"
```

具体而言，只要训练或推理任务跑在你没法直接 SSH 进去查看的地方，这件事就很重要：**GPU 云租赁**（你不信任运营方的主机）、**多租户本地集群**（同租户不应读到你的权重或数据），以及**去中心化/训练证明（proof-of-training）算力网络**（匿名节点提交训练声明，必须在不重跑训练的前提下完成验证）。

## 参考硬件

| 组件 | 所需条件 | 备注 |
|---|---|---|
| **CPU** | 第 4/5 代 Intel Xeon Scalable（Sapphire Rapids、Emerald Rapids），以及支持 Intel TDX 的 Xeon 6（Sierra Forest、Granite Rapids） | 在 BIOS 中启用 TDX Module v1.5.x（SPR/EMR）或 v2.0.x（Granite Rapids） |
| **GPU** | NVIDIA H100（Hopper）或 Blackwell（B200/HGX），CC-On 模式 | 与 [8x H200 训练与推理](../01-8x-H200训练推理/README.md) 和 [Blackwell B200 Qwen 推理](../02-Blackwell-B200-Qwen推理/README.md) 中覆盖的是同一批芯片 —— 本篇深度解析在其之上叠加安全层 |
| **远程证明工具链** | Intel DCAP（`dcap-qvl` 或等效方案）或 Intel Tiber Trust Services；NVIDIA NRAS + nvTrust | 完整的开源 Intel TDX 主机/客户机参考栈见 [canonical/tdx](https://github.com/canonical/tdx) |

## 主题索引

| # | 主题 | 描述 |
|---|---|---|
| 01 | [Intel TDX 远程证明链](01-Intel-TDX-Attestation-Chain.md) | 以 TDX Module 作为测量信任根（Root of Trust for Measurement），MRTD/RTMR/REPORTDATA，TD Report → Quoting Enclave → TD Quote，对照 Intel 根 CA 做 DCAP 验证 |
| 02 | [NVIDIA CC 远程证明链](02-NVIDIA-CC-Attestation-Chain.md) | CC-On 模式下的 GPU EAT token，`eat_nonce` 绑定，通过 JWKS 做 NRAS 签名验证，对称密钥自证明陷阱 |
| 03 | [双厂商声明绑定与验证审查](03-Dual-Vendor-Claim-Binding-and-Verification.md) | 围绕同一条 claim hash 组合两条证明链，把“绑定 vs 真实性”作为可推广的代码审查纪律，威胁模型，验证检查清单 |
| 04 | [机密推理：模型身份与端到端协议](04-Confidential-Inference-Protocol.md) | “sealed”意味着什么、不意味着什么，证明加载的是*哪一个模型*（而不只是哪一段字节），发布方签名的缺口，以及一套完整的“客户端先验证再发送”的机密推理协议 |

## 快速导航

- **第一次在主机/客户机上搭建 TDX？** → [01 — Intel TDX 远程证明链](01-Intel-TDX-Attestation-Chain.md)
- **要验证 GPU 的 CC 模式远程证明 token？** → [02 — NVIDIA CC 远程证明链](02-NVIDIA-CC-Attestation-Chain.md)
- **在设计或审查训练证明 / 远程算力验证器？** → [03 — 双厂商声明绑定与验证审查](03-Dual-Vendor-Claim-Binding-and-Verification.md)
- **在构建或评测“机密 AI”/ 密封模型推理 API？** → [04 — 机密推理：模型身份与端到端协议](04-Confidential-Inference-Protocol.md)


<details>
<summary>English original</summary>

**Confidential Computing & Hardware Attestation — Deep Dive**

Most of this HPC track assumes you control the whole stack: your cluster, your NVLink fabric, your Slurm queue. That assumption breaks the moment compute is **rented, shared, or run on a node you don't physically control** — GPU-cloud capacity, a multi-tenant HPC cluster, or a decentralized compute network where anonymous operators contribute nodes. In that world a new question shows up before any performance question does: **how do you know a remote node actually ran the workload you think it ran, on real hardware, without tampering?**

That's what this deep dive covers: **hardware-rooted remote attestation** for CPU (Intel TDX) and GPU (NVIDIA Confidential Computing), and — because a single-vendor check is a single point of forgery — how to **bind one claim into both hardware roots at once** so a verifier doesn't have to trust either vendor's silicon alone.

**Why This Belongs in HPC Infrastructure**

Every other deep dive in this section (8x H200, Blackwell B200, GPUDirect Storage, NCCL) optimizes a cluster you trust. This one answers the orthogonal question that shows up as soon as a node leaves your physical custody:

```text
Trusted cluster (rest of HPC Setup):        Untrusted/rented/decentralized node (this deep dive):
  "how fast can this run?"                    "did this actually run, on real hardware,
                                                unmodified, and can I prove it without
                                                re-executing the whole job?"
```

Concretely, this matters whenever a training or inference job runs somewhere you can't just SSH in and check: **GPU-cloud rental** (you don't trust the operator's host), **multi-tenant on-prem clusters** (co-tenants shouldn't read your weights or data), and **decentralized/proof-of-training compute networks** (anonymous nodes submit training claims that must be verified without redoing the training).

**Reference Hardware**

| Component | What's needed | Notes |
|---|---|---|
| **CPU** | 4th/5th-gen Intel Xeon Scalable (Sapphire Rapids, Emerald Rapids), Xeon 6 (Sierra Forest, Granite Rapids) with Intel TDX | TDX Module v1.5.x (SPR/EMR) or v2.0.x (Granite Rapids) enabled in BIOS |
| **GPU** | NVIDIA H100 (Hopper) or Blackwell (B200/HGX), CC-On mode | Same silicon covered in [8x H200 Training & Inference](../01-8x-H200训练推理/README.md) and [Blackwell B200 Qwen Inference](../02-Blackwell-B200-Qwen推理/README.md) — this deep dive adds the security layer on top |
| **Attestation tooling** | Intel DCAP (`dcap-qvl` or equivalent) or Intel Tiber Trust Services; NVIDIA NRAS + nvTrust | See [canonical/tdx](https://github.com/canonical/tdx) for a full open-source Intel TDX host/guest reference stack |

**Topic Index**

| # | Topic | Description |
|---|---|---|
| 01 | [Intel TDX Attestation Chain](01-Intel-TDX-Attestation-Chain.md) | TDX Module as Root of Trust for Measurement, MRTD/RTMR/REPORTDATA, TD Report → Quoting Enclave → TD Quote, DCAP verification against Intel's root CA |
| 02 | [NVIDIA CC Attestation Chain](02-NVIDIA-CC-Attestation-Chain.md) | CC-On mode GPU EAT tokens, `eat_nonce` binding, NRAS signature verification via JWKS, the symmetric-key self-attestation trap |
| 03 | [Dual-Vendor Claim Binding & Verification Review](03-Dual-Vendor-Claim-Binding-and-Verification.md) | Composing both chains around one claim hash, binding-vs-authenticity as a generalizable code-review discipline, threat model, verification checklist |
| 04 | [Confidential Inference: Model Identity & the End-to-End Protocol](04-Confidential-Inference-Protocol.md) | What "sealed" does and doesn't mean, proving *which model* (not just which bytes) is loaded, the publisher-signature gap, and a full client-verifies-before-sending confidential inference protocol |

**Quick Navigation**

- **Setting up TDX on a host/guest for the first time?** → [01 — Intel TDX Attestation Chain](01-Intel-TDX-Attestation-Chain.md)
- **Verifying a GPU's CC-mode attestation token?** → [02 — NVIDIA CC Attestation Chain](02-NVIDIA-CC-Attestation-Chain.md)
- **Designing or reviewing a proof-of-training / remote-compute verifier?** → [03 — Dual-Vendor Claim Binding & Verification Review](03-Dual-Vendor-Claim-Binding-and-Verification.md)
- **Building or evaluating a "confidential AI" / sealed-model inference API?** → [04 — Confidential Inference: Model Identity & the End-to-End Protocol](04-Confidential-Inference-Protocol.md)

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Confidential-Computing-Attestation/README.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Confidential-Computing-Attestation/README.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
