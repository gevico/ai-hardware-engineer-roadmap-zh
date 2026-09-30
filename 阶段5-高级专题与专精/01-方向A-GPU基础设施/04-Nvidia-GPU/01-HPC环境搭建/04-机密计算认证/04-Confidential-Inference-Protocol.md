---
title: 04 — 机密推理：模型身份与端到端协议
description: 04 — 机密推理：模型身份与端到端协议
---

# 04 — 机密推理：模型身份与端到端协议

文件 [01](01-Intel-TDX-Attestation-Chain.md)–[03](03-Dual-Vendor-Claim-Binding-and-Verification.md) 证明的是**真实节点运行了真实且经过度量的软件栈**。本文件把这套机制应用到一个具体且高风险的场景：客户端调用某个「confidential」或「sealed」的 LLM API，并希望在发出任何一条 prompt 之前就确知：(1) 服务端是真正的 TEE，(2) 模型*恰好*是它声称的那些权重，(3) prompt/response 在该 TEE 之外从不存在明文。这三者是彼此独立的属性，而「sealed」或「TEE-hosted」这类营销文案往往把它们捆成一个词——尽管它们并非一回事。

## 1.「Sealed」到底意味着什么

「Sealed」意味着工作负载运行在 TEE 内部——仅此而已，不多不少。它本身既不说明网络路径，也不说明加载的是哪个模型。

```text
Normal VM                                   Sealed VM (Intel TDX Trust Domain)
┌────────────────────────┐                  ╔════════════════════════╗
│ Linux VM                │                  ║ Intel TDX Trust Domain ║
│  Model                  │                  ║  Linux                ║
│  Inference server       │                  ║  Model                ║
│  Memory                 │                  ║  Inference server     ║
└────────────────────────┘                  ║  Memory               ║
                                              ╚════════════════════════╝
Host OS / Hypervisor / Cloud operator        Host OS / Hypervisor / Cloud operator
  → can read guest memory                      → memory encrypted + integrity-protected
                                                → cannot read or modify guest memory
```

这正是 [file 01 §2](01-Intel-TDX-Attestation-Chain.md) 中的 MRTD/RTMR 隔离模型——「sealed」就是同一个 TD 边界的白话说法。它覆盖什么、不覆盖什么：

| 「sealed」覆盖的范围（TEE 隔离） | 不在范围内——需要单独的机制 |
|---|---|
| 内存对 hypervisor/宿主 OS 加密 | 网络传输加密 |
| 与同一宿主机上的其他租户隔离 | 对调用方客户端的 API 认证 |
| 云运营商无法 dump/检视 guest RAM | 对加载的是*哪个*模型的验证 |
| 由硬件强制（CPU 封装），而非策略承诺 | 绑定到远程证明的 prompt/response 端到端加密 |

CC 模式的 GPU 或 TDX 主机确实处于 sealed 状态，与客户端能够*验证*右栏中的任何一项，是彼此独立的论断。将它们混为一谈，是「confidential AI」产品宣传中最常见的错误。

## 2. 不能互相替代的两层加密

发往 sealed 推理端点的请求会跨越两个不同的受保护边界，每一层都需要独立的检查：

```text
Layer 1 — TLS (transport)                  Layer 2 — TDX (execution)
  Client ──HTTPS──▶ Server                   Server ──▶ TDX Trust Domain ──▶ Model

  Protects the request in transit            Protects the request while it's being computed on
  over the public Internet.                  Standard for any HTTPS API — proves nothing about
  Says nothing about what happens             what runs inside the box once the TLS session
  once the bytes reach the server.            terminates.
```

TLS 在服务端的边缘终止——一个挡在非机密 VM 前面的普通反向代理就能提供 TLS，完全不涉及 TEE。TDX 保护的是终止之后发生的事。只检查「这个 API 是否用了 HTTPS」的客户端，验证的只是第 1 层，对第 2 层则一无所知。第 8 节会讲到真正把二者串联起来的协议。


<details>
<summary>English original</summary>

**04 — Confidential Inference: Model Identity & the End-to-End Protocol**

Files [01](01-Intel-TDX-Attestation-Chain.md)–[03](03-Dual-Vendor-Claim-Binding-and-Verification.md) attest that **a genuine node ran a genuine, measured software stack**. This file applies that machinery to a specific, high-stakes case: a client calling a "confidential" or "sealed" LLM API and wanting to know, before sending a single prompt, that (1) the server is a real TEE, (2) the model is *exactly* the weights it claims to be, and (3) the prompt/response never exist in plaintext outside that TEE. Each of those is a separate property, and marketing copy that says "sealed" or "TEE-hosted" tends to bundle them into one word when they aren't.

**1. What "Sealed" Actually Means**

"Sealed" means the workload runs inside a TEE — nothing more, nothing less. It says nothing by itself about the network path or about which model is loaded.

```text
Normal VM                                   Sealed VM (Intel TDX Trust Domain)
┌────────────────────────┐                  ╔════════════════════════╗
│ Linux VM                │                  ║ Intel TDX Trust Domain ║
│  Model                  │                  ║  Linux                ║
│  Inference server       │                  ║  Model                ║
│  Memory                 │                  ║  Inference server     ║
└────────────────────────┘                  ║  Memory               ║
                                              ╚════════════════════════╝
Host OS / Hypervisor / Cloud operator        Host OS / Hypervisor / Cloud operator
  → can read guest memory                      → memory encrypted + integrity-protected
                                                → cannot read or modify guest memory
```

This is exactly the MRTD/RTMR isolation model from [file 01 §2](01-Intel-TDX-Attestation-Chain.md) — "sealed" is the plain-English name for the same TD boundary. What it covers and doesn't:

| In scope for "sealed" (TEE isolation) | Out of scope — separate mechanisms required |
|---|---|
| Memory encrypted against the hypervisor/host OS | Network transport encryption |
| Isolation from co-tenants on the same host | API authentication of the calling client |
| Cloud operator cannot dump/inspect guest RAM | Verification of *which* model is loaded |
| Hardware-enforced (CPU package), not a policy promise | End-to-end encryption of prompts/responses tied to the attestation |

A CC-mode GPU or TDX host being genuinely sealed and a client being able to *verify* any of the right-hand column are independent claims. Conflating them is the single most common mistake in "confidential AI" product claims.

**2. Two Encryption Layers That Don't Substitute for Each Other**

A request to a sealed inference endpoint crosses two distinct protected boundaries, and each needs its own check:

```text
Layer 1 — TLS (transport)                  Layer 2 — TDX (execution)
  Client ──HTTPS──▶ Server                   Server ──▶ TDX Trust Domain ──▶ Model

  Protects the request in transit            Protects the request while it's being computed on
  over the public Internet.                  Standard for any HTTPS API — proves nothing about
  Says nothing about what happens             what runs inside the box once the TLS session
  once the bytes reach the server.            terminates.
```

TLS terminates at the server's edge — a plain-old reverse proxy in front of a non-confidential VM can offer TLS with zero TEE involvement. TDX protects what happens after that termination. A client that only checked "does this API use HTTPS" verified layer 1 and learned nothing about layer 2. Section 8 covers the protocol that actually chains the two together.

</details>

## 3. 远程证明能证明加载的是*哪个模型*吗？

单靠它自己不能，而这正是 file 03 §4 针对 golden-measurement registry 一般性指出的同一个局限，如今被具体应用到模型权重上。

TDX 测量的是字节，而不是语义。所有加载进 TD 的内容——bootloader、kernel、initrd、文件系统、推理服务器、模型权重——都会通过与 [file 01 §2](01-Intel-TDX-Attestation-Chain.md) 相同的 extend 操作被并入 RTMR：

```text
RTMR_new = SHA-384( RTMR_old ‖ component )

  bootloader ─▶ kernel ─▶ initrd ─▶ filesystem ─▶ inference_server ─▶ weights.bin
       (each stage extends the running hash; changing any stage changes every hash after it)
```

由此得到的 quote 证明：**“这串被精确测量的 stack，包括这个精确的 weights.bin，就是实际启动的内容。”** 它并不天然知道该测量值对应于一个名为 “Qwen3-32B” 的模型——这个映射是外部的：

```text
Measurement  ABC123...  =  Qwen3-32B, FP16, published 2026-03-01     (maintained registry, not part of TDX)
```

翻转权重中的一个字节，整条链的形态就会彻底改变：

```text
weights.bin (unmodified)  ──▶  RTMR extend  ──▶  Measurement ABC123...  ──▶  known-good, attestation succeeds
weights.bin (1 byte swapped) ─▶ RTMR extend  ──▶  Measurement XYZ987...  ──▶  not in registry, attestation fails
```

因此 TDX **确实**能证明权重完整性，并能在替换发生的瞬间检测到任何替换——但仅相对于验证方 registry 判定为 “真正的 Qwen3-32B” 的那个测量值而言。“这是 Qwen” 这一断言的强度，完全取决于该 registry 最初是如何得到 ABC123 → Qwen3-32B 这个映射的，而这是第 6 节要处理的问题。

## 4. 缺口：被修改的模型被登记为合法

假设服务器运营方——不是外部攻击者，而是运营方自身——构建了一个经过修改的推理 stack，在用它做推理服务之前悄悄记录每个 prompt：

```text
Qwen3-32B (official)  ──modify──▶  Qwen3-32B + prompt exfiltration
```

如果运营方计算这个修改过的 stack 的测量值，并*把那个* hash 登记为期望值，那么远程证明每次都会成功：quote 会正确地报告 “测量到的字节与 registry 匹配”，因为 registry 本身在源头就已经被投毒了。这恰恰是 [file 03 §4](03-Dual-Vendor-Claim-Binding-and-Verification.md) 中的告诫——*“一个过期或错误的 golden value 会静默地让这一切失效”*——只不过这里的 golden value 从一开始就从未正确过，因为设定它的一方与被验证的一方是同一个实体。

要弥合这个缺口，需要一个独立于服务器运营方的信任根：

```text
Attestation (proves: these exact bytes launched)
        +
Signed model manifest (weight_hash, model_name, version)
        +
Publisher's signature over that manifest (Alibaba/Qwen team's key, not the server operator's)
        │
        ▼
Now the client knows: not just "unmodified since launch," but "this is the
model the publisher actually released" — a claim the hosting operator cannot forge alone.
```

这在结构上与 DCAP 将 TD Quote 链接到 Intel 根 CA（[file 01 §4](01-Intel-TDX-Attestation-Chain.md)）或 NRAS 将 GPU EAT 链接到 NVIDIA 签名密钥（[file 02](02-NVIDIA-CC-Attestation-Chain.md)）是同一种模式——即引入第三个独立签名者，因此要伪造该声明，除了控制模型运行所在的机器之外，还必须攻破发布方的密钥。


<details>
<summary>English original</summary>

**3. Does Attestation Prove *Which Model* Is Loaded?**

Not by itself, and this is the same limitation file 03 §4 flags for the golden-measurement registry generally, now applied specifically to model weights.

TDX measures bytes, not semantics. Everything that loads into the TD — bootloader, kernel, initrd, filesystem, inference server, model weights — gets folded into RTMR via the same extend operation from [file 01 §2](01-Intel-TDX-Attestation-Chain.md):

```text
RTMR_new = SHA-384( RTMR_old ‖ component )

  bootloader ─▶ kernel ─▶ initrd ─▶ filesystem ─▶ inference_server ─▶ weights.bin
       (each stage extends the running hash; changing any stage changes every hash after it)
```

The resulting quote proves: **"this exact measured stack, including this exact weights.bin, is what launched."** It does not inherently know that measurement corresponds to a model called "Qwen3-32B" — that mapping is external:

```text
Measurement  ABC123...  =  Qwen3-32B, FP16, published 2026-03-01     (maintained registry, not part of TDX)
```

Flip one byte of the weights and the chain changes shape entirely:

```text
weights.bin (unmodified)  ──▶  RTMR extend  ──▶  Measurement ABC123...  ──▶  known-good, attestation succeeds
weights.bin (1 byte swapped) ─▶ RTMR extend  ──▶  Measurement XYZ987...  ──▶  not in registry, attestation fails
```

So TDX **does** prove weight integrity and detects any substitution the instant it happens — but only relative to whatever measurement the verifier's registry says is "the real Qwen3-32B." The strength of "this is Qwen" is entirely a function of how that registry got its ABC123 → Qwen3-32B mapping in the first place, which is section 6's problem.

**4. The Gap: A Modified Model Registered as Legitimate**

Suppose the server operator — not an external attacker, the operator itself — builds a modified inference stack that quietly logs every prompt before serving it:

```text
Qwen3-32B (official)  ──modify──▶  Qwen3-32B + prompt exfiltration
```

If the operator computes this modified stack's measurement and registers *that* hash as the expected value, attestation succeeds every time: the quote correctly reports "the measured bytes match the registry," because the registry itself was poisoned at the source. This is precisely the caveat from [file 03 §4](03-Dual-Vendor-Claim-Binding-and-Verification.md) — *"a stale or wrong golden value defeats this silently"* — except here the golden value was never right to begin with, because the party setting it and the party being verified are the same entity.

Closing this gap needs a trust root that is independent of the server operator:

```text
Attestation (proves: these exact bytes launched)
        +
Signed model manifest (weight_hash, model_name, version)
        +
Publisher's signature over that manifest (Alibaba/Qwen team's key, not the server operator's)
        │
        ▼
Now the client knows: not just "unmodified since launch," but "this is the
model the publisher actually released" — a claim the hosting operator cannot forge alone.
```

This is structurally the same pattern as DCAP chaining a TD Quote to Intel's root CA ([file 01 §4](01-Intel-TDX-Attestation-Chain.md)) or NRAS chaining a GPU EAT to NVIDIA's signing key ([file 02](02-NVIDIA-CC-Attestation-Chain.md)) — a third independent signer, so forging the claim requires compromising the publisher's key too, not just controlling the box the model runs on.

</details>

## 5. 三个独立信任根，一个会话

把文件 01–03 中的全部内容，再加上第 4 节的发布者链，组合起来就得到三个必须全部达成一致的厂商，并锚定在同一个客户端会话上：

```text
                              client session
                    ┌───────────────┼───────────────┐
                    ▼               ▼                ▼
            Intel root CA    NVIDIA root (NRAS)   Publisher signing key
         (CPU/TD genuine,   (GPU genuine, CC-On   (weight_hash matches
          stack measured)      mode, EAT valid)     the official release)
```

每一列防的是不同的伪造：

| 属性 | 提供方 | 缺少它会出什么问题 |
|---|---|---|
| 已验证的服务器（真正的 TDX TD） | DCAP → Intel 根 CA（[文件 01 §4](01-Intel-TDX-Attestation-Chain.md)） | 攻击者跑一个普通 VM，谎称它是 TD |
| 已验证的 GPU（真正的 CC 模式硅片） | NRAS → NVIDIA 根（[文件 02](02-NVIDIA-CC-Attestation-Chain.md)） | 攻击者跑在非 CC 的 GPU 上，自行签一个 EAT |
| 加密传输 | TLS / mTLS | 对 prompt/response 的被动网络窃听 |
| 加密执行（RAM 对宿主机永不为明文） | TDX 内存加密 | 有特权的宿主机管理员 dump 客户机 RAM，读取 prompt |
| 模型身份（与官方权重完全一致） | 发布者签名的 manifest（§4） | 运营方换入一个被改过的模型，并把它自己的 hash 注册为「预期」值 |
| 会话绑定到上述全部 | 协议设计（§8），而非任何单一厂商 | 把来自*某次*请求的有效远程证明重放出去，用来授权*另一次*不同的、未被验证的请求 |

没有哪一行单独就够用——这张表是 [文件 03 §3](03-Dual-Vendor-Claim-Binding-and-Verification.md) 里「绑定 vs. 真实性」这条准则在模型推理服务上的具体实例：每一项属性既需要某个被绑定进去的东西，也需要一条验证它的独立签名链。

## 6. 机密推理协议

把三个信任根和两层加密放进一条由客户端驱动的流程——客户端在发出任何 prompt 之前就完成验证，而不是之后：

```text
Client                                                      Server (TDX TD + CC-mode GPU)
  │
  │ 1. Request attestation bundle
  │───────────────────────────────────────────────────────▶│
  │                                                          │  generates TD Quote + GPU EAT,
  │                                                          │  includes signed model manifest
  │◀───────────────────────────────────────────────────────│
  │
  │ 2. Verify TD Quote → Intel root CA           (file 01 §4)
  │ 3. Verify GPU EAT → NVIDIA NRAS/JWKS          (file 02)
  │ 4. Verify weight_hash in measurement matches
  │    the signed model manifest                  (§4 above)
  │ 5. Verify manifest signature → publisher key   (§4 above)
  │
  │    ── only proceed past this line if 2–5 all pass ──
  │
  │ 6. Generate ephemeral session key, encrypt to a
  │    public key bound inside the attested TD
  │───────────────────────────────────────────────────────▶│
  │ 7. Encrypt prompt under the session key                 │
  │───────────────────────────────────────────────────────▶│  decrypts only inside the
  │                                                          │  TDX TD; plaintext prompt
  │                                                          │  never exists outside it
  │                                                          │  runs inference
  │ 8. Encrypted response                                    │  encrypts response under
  │◀───────────────────────────────────────────────────────│  the same session key
  │
  │ 9. Client decrypts locally
```

相比「API 用了 HTTPS，而机器刚好是台 TDX 主机」，这带来了什么：

- **先验证，后信任，而不是反过来。** 步骤 2–5 为步骤 6 设卡——只要服务器有任何一项检查没过，prompt 就绝不会被加密发向它；这不同于裸 TLS 连接，后者客户端还没了解到执行环境的任何信息，就已经把数据交了出去。
- **会话密钥绑定到经过远程证明的身份**，而不只是绑定到 TLS 证书。仅有 TLS 只能证明「我在和持有这张证书的一方说话」——它不能证明那个端点是一台运行着正版模型的真正的 TD。加密所用的密钥是在经过证明的 TD *内部*生成的（而不是服务器进程可以在任何地方生成的密钥），这把两者绑在了一起。
- **模型替换和 prompt 外泄两者都被覆盖**，而不只是其中一个。§4 的发布者签名链能阻止一个被悄悄改过的模型通过检查；TDX 内存加密能阻止主机运营方读取 prompt，即便它有 root 权限。


<details>
<summary>English original</summary>

**5. Three Independent Trust Roots, One Session**

Composing everything from files 01–03 plus the publisher chain from section 4 gives three vendors that all have to agree, anchored to one client session:

```text
                              client session
                    ┌───────────────┼───────────────┐
                    ▼               ▼                ▼
            Intel root CA    NVIDIA root (NRAS)   Publisher signing key
         (CPU/TD genuine,   (GPU genuine, CC-On   (weight_hash matches
          stack measured)      mode, EAT valid)     the official release)
```

Each column defends a different forgery:

| Property | Provided by | What fails without it |
|---|---|---|
| Verified server (genuine TDX TD) | DCAP → Intel root CA ([file 01 §4](01-Intel-TDX-Attestation-Chain.md)) | Attacker runs a normal VM and lies about it being a TD |
| Verified GPU (genuine CC-mode silicon) | NRAS → NVIDIA root ([file 02](02-NVIDIA-CC-Attestation-Chain.md)) | Attacker runs on non-CC GPUs and self-signs an EAT |
| Encrypted transport | TLS / mTLS | Passive network eavesdropping of prompts/responses |
| Encrypted execution (RAM never plaintext to the host) | TDX memory encryption | A privileged host admin dumps guest RAM and reads prompts |
| Model identity (exact official weights) | Publisher-signed manifest (§4) | Operator swaps in a modified model and registers its own hash as "expected" |
| Session bound to all of the above | Protocol design (§8), not any single vendor | A valid attestation from *a* request gets replayed to authorize a *different*, unattested one |

No single row is sufficient alone — this table is the model-serving-specific instance of the "binding vs. authenticity" discipline from [file 03 §3](03-Dual-Vendor-Claim-Binding-and-Verification.md): each property needs both something bound in and an independent signature chain validating it.

**6. The Confidential Inference Protocol**

Putting all three trust roots and both encryption layers into one client-driven flow — the client verifies before it ever sends a prompt, not after:

```text
Client                                                      Server (TDX TD + CC-mode GPU)
  │
  │ 1. Request attestation bundle
  │───────────────────────────────────────────────────────▶│
  │                                                          │  generates TD Quote + GPU EAT,
  │                                                          │  includes signed model manifest
  │◀───────────────────────────────────────────────────────│
  │
  │ 2. Verify TD Quote → Intel root CA           (file 01 §4)
  │ 3. Verify GPU EAT → NVIDIA NRAS/JWKS          (file 02)
  │ 4. Verify weight_hash in measurement matches
  │    the signed model manifest                  (§4 above)
  │ 5. Verify manifest signature → publisher key   (§4 above)
  │
  │    ── only proceed past this line if 2–5 all pass ──
  │
  │ 6. Generate ephemeral session key, encrypt to a
  │    public key bound inside the attested TD
  │───────────────────────────────────────────────────────▶│
  │ 7. Encrypt prompt under the session key                 │
  │───────────────────────────────────────────────────────▶│  decrypts only inside the
  │                                                          │  TDX TD; plaintext prompt
  │                                                          │  never exists outside it
  │                                                          │  runs inference
  │ 8. Encrypted response                                    │  encrypts response under
  │◀───────────────────────────────────────────────────────│  the same session key
  │
  │ 9. Client decrypts locally
```

What this buys over "the API uses HTTPS and the box happens to be a TDX host":

- **Verification precedes trust, not the reverse.** Steps 2–5 gate step 6 — no prompt is ever encrypted toward a server that failed any check, unlike a bare TLS connection where the client has already committed data before learning anything about the execution environment.
- **The session key is bound to the attested identity**, not just to the TLS certificate. TLS alone proves "I'm talking to whoever holds this cert" — it does not prove that endpoint is a genuine TD running the genuine model. Encrypting to a key generated *inside* the attested TD (vs. a key the server process could generate anywhere) ties the two together.
- **Model substitution and prompt exfiltration are both covered**, not just one. §4's publisher-signature chain stops a silently-modified model from passing; TDX memory encryption stops the host operator from reading prompts even with root access.

</details>

## 7. 它在文件 01–03 之外新增了什么

文件 01–03 回答的是“一个真实的节点，配合真实的 GPU，是否运行了真实的已度量栈”——对于验证者在*事后*检查声明的训练证明或执行证明声明来说，这已经足够。本文件的协议更强也更窄：它适用于客户端需要在*发送任何数据之前*进行验证的情况，并且受保护的对象（用户的 prompt）恰好就是流经被证明通道的载荷。§4 中的发布者签名链是唯一真正新的信任根——它根本不来自 Intel 或 NVIDIA，而跳过它的设计会继承来自 [文件 03 §4](03-Dual-Vendor-Claim-Binding-and-Verification.md) 的“已证明，但未审计”的每一项风险，只是针对的是模型权重，而不是训练声明。


<details>
<summary>English original</summary>

**7. What This Adds Beyond Files 01–03**

Files 01–03 answer "did a genuine node, with a genuine GPU, run a genuine measured stack" — sufficient for proof-of-training or proof-of-execution claims where the verifier checks a claim *after the fact*. This file's protocol is stronger and narrower: it's for the case where a client needs to verify *before sending any data*, and where the thing being protected (a user's prompt) is exactly the payload flowing through the channel being attested. The publisher-signature chain in §4 is the one genuinely new trust root — it doesn't come from Intel or NVIDIA at all, and a design that skips it inherits every risk of "attested, but not audited" from [file 03 §4](03-Dual-Vendor-Claim-Binding-and-Verification.md), just aimed at the model weights instead of the training claim.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track A - GPU Infrastructure/Nvidia GPU/HPC Setup/Confidential-Computing-Attestation/04-Confidential-Inference-Protocol.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20A%20-%20GPU%20Infrastructure/Nvidia%20GPU/HPC%20Setup/Confidential-Computing-Attestation/04-Confidential-Inference-Protocol.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
