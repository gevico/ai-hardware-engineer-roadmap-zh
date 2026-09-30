---
title: 01 — Intel TDX 远程证明链
description: 01 — Intel TDX 远程证明链
---

# 01 — Intel TDX 远程证明链

## 1. TDX 为多租户 HPC 节点带来了什么

共享主机上的普通 VM 完全信任 hypervisor —— 主机管理员，或任何攻陷了主机的人，都能读取 guest 内存。**Intel Trust Domain Extensions (TDX)** 去掉了这一假设：它创建 **Trust Domain (TD)**，一个硬件隔离的 VM，其内存经过加密与完整性保护，因而 hypervisor、host OS 和云运营商都无法读取或修改。在租用或多租户的 HPC 节点上，这就是“运营商承诺不看”与“运营商的 root 权限被密码学阻止查看”之间的差别。

不过，仅靠 TDX 只能得到机密性。另一半 —— **向远端证明实际运行的确实是一个真实、未被修改的 TD** —— 是**远程证明**，这正是本文件的主题。

## 2. TDX Module：驻留 CPU 的度量可信根

TD 启动时，硬件隔离的 **Intel TDX Module** 充当**度量可信根（RTM）** —— 这一不可变、驻留在 CPU 封装内的组件会计算并存储所有启动内容的密码学哈希。它维护：

| 寄存器 | 类型 | 度量的内容 |
|---|---|---|
| **MRTD** | 静态，SHA-384 | TD 的初始构建：虚拟固件、初始化参数、初始内存布局。对刚启动的域的一次性“身份指纹”。 |
| **RTMR[0..3]** | 动态，SHA-384，4 个寄存器 | 启动期间扩展的 runtime 度量，与 TPM PCR 完全一致：`new_hash = SHA-384(old_hash ‖ payload)`。跟踪 OS kernel、文件系统与应用负载的加载过程 —— 后续阶段叠加在先前阶段之上，因此篡改任一阶段都会改变其后每一个哈希。 |
| **REPORTDATA** | 自由格式，64 字节 | 不是度量 —— 一个暂存字段，guest 可在生成报告时填入*任意*内容。这是应用用来把自身数据（nonce、公钥、声明哈希）绑定进硬件签名报告的挂钩。 |

## 3. 从 TDCALL 到 TD Quote

TD 内的租户工作负载用一条硬件指令请求报告，而该报告在离开机器之前要经过两次变换：

```text
[ Tenant Workload ]                [ Intel TDX Module ]                  [ Quoting Enclave (SGX) ]
        │                                   │                                      │
        │──TDCALL[TDG.MR.REPORT]──────────▶│                                      │
        │                                   │  captures MRTD + RTMR[0..3]         │
        │                                   │  + REPORTDATA (caller-supplied)     │
        │                                   │  authenticates locally with an      │
        │                                   │  HMAC only the CPU package can read │
        │                                   │──local HMAC'd TD Report────────────▶│
        │                                   │                       EVERIFYREPORT2 checks the HMAC
        │                                   │                       replaces it with an asymmetric
        │                                   │                       signature from an Attestation Key
        │                                   │                                      │
        │◀──────────────────────────────────────────── TD Quote ───────────────────┘
```

这里有两点需要牢记：

- **TD Report 的 HMAC 永远不会离开 CPU 封装** —— 它只对本地 Quoting Enclave 的校验有用。远程验证方根本无法验证原始 TD Report。
- **TD Quote 是重新签名、可导出的对象。** Quoting Enclave（同一主机上的 SGX enclave）通过 `EVERIFYREPORT2` 验证本地 HMAC，然后用硬件派生的 **Attestation Key** 以 ECDSA 重新签名该报告。正是这个非对称签名能被远端实际验证 —— 也正是不可信主机转发给验证方的对象。


<details>
<summary>English original</summary>

**01 — Intel TDX Attestation Chain**

**1. What TDX Adds to a Multi-Tenant HPC Node**

A regular VM on a shared host trusts the hypervisor completely — the host admin, or anyone who compromises the host, can read guest memory. **Intel Trust Domain Extensions (TDX)** removes that assumption: it creates a **Trust Domain (TD)**, a hardware-isolated VM whose memory is encrypted and integrity-protected such that the hypervisor, host OS, and cloud operator cannot read or modify it. On a rented or multi-tenant HPC node, this is the difference between "the operator promises not to look" and "the operator's root access is cryptographically prevented from looking."

TDX alone only gets you confidentiality, though. The other half — **proving to a remote party that a genuine, unmodified TD is what actually ran** — is **remote attestation**, and that's the subject of this file.

**2. The TDX Module: CPU-Resident Root of Trust for Measurement**

When a TD launches, the hardware-isolated **Intel TDX Module** acts as the **Root of Trust for Measurement (RTM)** — the immutable, CPU-package-resident component that computes and stores cryptographic hashes of everything that boots. It maintains:

| Register | Type | What it measures |
|---|---|---|
| **MRTD** | Static, SHA-384 | The TD's initial build: virtual firmware, initialization parameters, initial memory layout. A one-shot "identity fingerprint" of the freshly launched domain. |
| **RTMR[0..3]** | Dynamic, SHA-384, 4 registers | Runtime measurements extended during boot, exactly like TPM PCRs: `new_hash = SHA-384(old_hash ‖ payload)`. Tracks OS kernel, filesystem, and application payloads as they load — later stages layer on top of earlier ones, so tampering with any stage changes every subsequent hash. |
| **REPORTDATA** | Free-form, 64 bytes | Not a measurement — a scratch field the guest can fill with *anything* at report-generation time. This is the hook applications use to bind their own data (a nonce, a public key, a claim hash) into a hardware-signed report. |

**3. From TDCALL to TD Quote**

A tenant workload inside the TD requests a report with a single hardware instruction, and the report is transformed twice before it can leave the machine:

```text
[ Tenant Workload ]                [ Intel TDX Module ]                  [ Quoting Enclave (SGX) ]
        │                                   │                                      │
        │──TDCALL[TDG.MR.REPORT]──────────▶│                                      │
        │                                   │  captures MRTD + RTMR[0..3]         │
        │                                   │  + REPORTDATA (caller-supplied)     │
        │                                   │  authenticates locally with an      │
        │                                   │  HMAC only the CPU package can read │
        │                                   │──local HMAC'd TD Report────────────▶│
        │                                   │                       EVERIFYREPORT2 checks the HMAC
        │                                   │                       replaces it with an asymmetric
        │                                   │                       signature from an Attestation Key
        │                                   │                                      │
        │◀──────────────────────────────────────────── TD Quote ───────────────────┘
```

Two things to internalize here:

- **The TD Report's HMAC never leaves the CPU package** — it's only useful for the local Quoting Enclave to check. A remote verifier can't validate a raw TD Report at all.
- **The TD Quote is the re-signed, exportable object.** The Quoting Enclave (an SGX enclave on the same host) validates the local HMAC via `EVERIFYREPORT2`, then re-signs the report with a hardware-derived **Attestation Key** using ECDSA. This asymmetric signature is what a remote party can actually verify — and it's what the untrusted host routes out to a verifier.

</details>

## 4. DCAP 验证：把 Quote 逐级链接回 Intel 根 CA

仅凭 TD Quote 的签名还不够——验证方还需要确认签名密钥本身是合法的。**DCAP（Data Center Attestation Primitives）** 提供了这条信任链：

```text
TD Quote
   ├─ ECDSA-P256 signature over the quote body   ──verified against──▶  Attestation Key
   ├─ Attestation Key certified by                ──────────────────▶  PCK (Platform Certification Key) certificate
   ├─ PCK certificate chains to                     ────────────────▶  Intel PCK Platform CA → Intel Root CA
   ├─ Quoting Enclave identity                       ──────────────▶  MRSIGNER / ISVPRODID match expected values
   └─ Live TCB status from Intel PCS collateral       ────────────▶  "UpToDate" / "OutOfDate" / specific advisory IDs
```

```python
def verify_tdx_quote(quote_bytes):
    result = dcap_qvl.verify(quote_bytes)   # ECDSA sig, PCK chain -> Intel root CA, QE identity
    return {
        "signature_valid": result.signature_valid,
        "chain_valid":     result.chain_valid_to_intel_root,
        "tcb_status":      result.tcb_status,        # e.g. "UpToDate"
        "advisory_ids":    result.advisory_ids,       # e.g. []
    }
```

**报告 TCB 状态，不要把它压缩成布尔值。** `UpToDate`、`OutOfDate` 与某个具体的 advisory ID 属于不同的风险等级——对于你的威胁模型而言，一个存在不可利用 advisory 的 `OutOfDate` 平台或许可以接受，而 `Revoked` 平台则绝不应该接受。把这一切压平为 `passed: true/false` 的验证方，会在当事人毫不知情的情况下，把这项策略决策从远程证明的消费方手中夺走。

这与 [canonical/tdx](https://github.com/canonical/tdx) 开源参考栈通过 `trustauthority-cli evidence --tdx` / `trustauthority-cli token` 所驱动的信任链相同，只是改由 Intel Tiber Trust Services 作为外部验证方来路由，而不是使用本地 DCAP 库——同一条信任链，不同的验证方端点。如果想在真实的 Sapphire Rapids / Emerald Rapids / Xeon 6 硬件上复现其中任何一部分，该项目也是搭建可用 TDX host + guest + 远程证明环境的最快路径：host 配置、TD 镜像创建、启动，以及本地（DCAP）与云端代理（Intel Tiber Trust Services）两种远程证明流程，都已端到端脚本化。

## 5. 通过 REPORTDATA 绑定应用数据

REPORTDATA 的作用是把“这块硬件是真的”变成“这块硬件确实产生了*这一特定声明*”。工作负载希望被证明的任何 64 字节值，都会在请求 report 之前零填充进该字段：

```text
application_hash (32 bytes) ──zero-padded──▶ REPORTDATA (64 bytes) ──▶ TDCALL[TDG.MR.REPORT] ──▶ TD Report
```

例如，一个 HPC（高性能计算）作业想要证明*究竟是哪个 recipe 和输出*跑在租来的节点上，它会先对自己的声明（作业规格、环境度量、输出哈希）计算摘要，然后在请求 quote 之前直接填入 REPORTDATA。验证方随后会做两项独立的检查——而不是一项：

```python
def check_tdx_binding(claim_hash, tdx_report):
    return tdx_report.reportdata[:32] == claim_hash    # binding only — see file 03 for why this is not enough alone
```

如果缺少 §4 中的 DCAP 签名/证书链检查，绑定就只是一种装饰性检查——一个手工构造的 JSON blob，只要在名为 `reportdata` 的字段里放上正确的 32 字节，就能在完全没有 TDX Module 参与的情况下通过 `check_tdx_binding`。[File 03](03-Dual-Vendor-Claim-Binding-and-Verification.md) 深入讨论了这一失效模式及其推广；请把这里的 §4 与 §5 当作配套的一对，绝不可只用其一。

## 6. 支持的硬件速查

| 处理器 | 代号 | TDX Module 版本 |
|---|---|---|
| 第 4 代 Intel Xeon 可扩展处理器 | Sapphire Rapids | 1.5.x |
| 第 5 代 Intel Xeon 可扩展处理器 | Emerald Rapids | 1.5.x |
| Intel Xeon 6（E-Cores） | Sierra Forest | 1.5.x |
| Intel Xeon 6（P-Cores） | Granite Rapids | 2.0.x |

用 `sudo dmesg | grep -i tdx` 查看当前加载的 TDX Module 版本——同时报告 `virt/tdx: module initialized` 以及 `major_version`/`minor_version`/`build_date` 的那一行，既能确认 TDX 已初始化，也能说明正在运行的是哪个 TDX Module 构建。


<details>
<summary>English original</summary>

**4. DCAP Verification: Chaining the Quote Back to Intel's Root CA**

A TD Quote's signature alone isn't enough — a verifier also needs to know the signing key itself is legitimate. **DCAP (Data Center Attestation Primitives)** provides that chain:

```text
TD Quote
   ├─ ECDSA-P256 signature over the quote body   ──verified against──▶  Attestation Key
   ├─ Attestation Key certified by                ──────────────────▶  PCK (Platform Certification Key) certificate
   ├─ PCK certificate chains to                     ────────────────▶  Intel PCK Platform CA → Intel Root CA
   ├─ Quoting Enclave identity                       ──────────────▶  MRSIGNER / ISVPRODID match expected values
   └─ Live TCB status from Intel PCS collateral       ────────────▶  "UpToDate" / "OutOfDate" / specific advisory IDs
```

```python
def verify_tdx_quote(quote_bytes):
    result = dcap_qvl.verify(quote_bytes)   # ECDSA sig, PCK chain -> Intel root CA, QE identity
    return {
        "signature_valid": result.signature_valid,
        "chain_valid":     result.chain_valid_to_intel_root,
        "tcb_status":      result.tcb_status,        # e.g. "UpToDate"
        "advisory_ids":    result.advisory_ids,       # e.g. []
    }
```

**Report the TCB status, don't collapse it to a boolean.** `UpToDate`, `OutOfDate`, and a specific advisory ID are different risk levels — an `OutOfDate` platform with a non-exploitable advisory for your threat model may be an acceptable accept, while a `Revoked` platform never should be. A verifier that flattens this into `passed: true/false` takes that policy decision away from whoever consumes the attestation, silently.

This is the same chain the [canonical/tdx](https://github.com/canonical/tdx) open-source reference stack drives via `trustauthority-cli evidence --tdx` / `trustauthority-cli token`, just routed through Intel Tiber Trust Services as an external verifier instead of a local DCAP library — same trust chain, different verifier endpoint. That project is also the fastest path to a working TDX host + guest + attestation setup if you want to reproduce any of this on real Sapphire Rapids / Emerald Rapids / Xeon 6 hardware: host setup, TD image creation, boot, and both local (DCAP) and cloud-brokered (Intel Tiber Trust Services) attestation flows are scripted end to end.

**5. Binding Application Data via REPORTDATA**

REPORTDATA is what turns "this hardware is genuine" into "this hardware genuinely produced *this specific claim*." Any 64-byte value the workload wants attested to gets zero-padded into the field before requesting the report:

```text
application_hash (32 bytes) ──zero-padded──▶ REPORTDATA (64 bytes) ──▶ TDCALL[TDG.MR.REPORT] ──▶ TD Report
```

For example, an HPC job that wants to prove *which exact recipe and output* ran on a rented node computes a digest of its own claim (job spec, environment measurement, output hash) and drops it straight into REPORTDATA before requesting the quote. A verifier then does two independent checks — not one:

```python
def check_tdx_binding(claim_hash, tdx_report):
    return tdx_report.reportdata[:32] == claim_hash    # binding only — see file 03 for why this is not enough alone
```

Binding without the DCAP signature/chain check from §4 is a decorative check — a hand-crafted JSON blob with the right 32 bytes in a field named `reportdata` passes `check_tdx_binding` with no TDX Module ever involved. [File 03](03-Dual-Vendor-Claim-Binding-and-Verification.md) covers this failure mode and its generalization in depth; treat §4 and §5 here as a matched pair, never one without the other.

**6. Supported Hardware Quick Reference**

| Processor | Code Name | TDX Module Version |
|---|---|---|
| 4th Gen Intel Xeon Scalable | Sapphire Rapids | 1.5.x |
| 5th Gen Intel Xeon Scalable | Emerald Rapids | 1.5.x |
| Intel Xeon 6 (E-Cores) | Sierra Forest | 1.5.x |
| Intel Xeon 6 (P-Cores) | Granite Rapids | 2.0.x |

Check the currently loaded module version with `sudo dmesg | grep -i tdx` — the line reporting `virt/tdx: module initialized` alongside `major_version`/`minor_version`/`build_date` confirms both that TDX initialized and which module build is running.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Confidential-Computing-Attestation/01-Intel-TDX-Attestation-Chain.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Confidential-Computing-Attestation/01-Intel-TDX-Attestation-Chain.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
