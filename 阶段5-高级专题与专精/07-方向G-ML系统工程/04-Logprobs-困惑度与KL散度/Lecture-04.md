---
title: Lecture 04 - KL 散度：两个分布之间的差距
description: Lecture 04 - KL 散度：两个分布之间的差距
---

# Lecture 04 - KL 散度：两个分布之间的差距

**Collection:** [Logprobs, Perplexity & KL Divergence](README.md) | **Previous:** [← Lecture 03](Lecture-03.md) | **Next:** [Lecture 05](Lecture-05.md)

---

两讲之前，主恒等式的左侧已经到手——交叉熵 `H(p,q)`，即训练所用的损失，以及熵 `H(p)`，即它永远无法突破的下界。本讲取得第三项、也是最后一项：**KL 散度** `D_KL(p ‖ q)`，即恒等式中那段不怪别人、只怪你自己的部分。

`H(p,q) = H(p) + D_KL(p ‖ q)`。把它当作一份账目来读。熵是现实向你开出的账单——数据自身的不确定性，没有任何模型能够退还。KL 散度则是你因为用错分布而额外附加的那笔附加费。它**非负，仅当模型完全正确时才为零，并且在原理上完全可以避免。** 你测量过的每一个交叉熵都是熵加上这笔附加费，而只凭损失数值你无法把两者区分开——这正是为什么损失下降可能意味着「数据变简单了」，而不是「我的模型变好了」。

以上是抽象层面的读法。而操作层面的读法，才使 KL 成为本阶段承重的指标：它是**通用的「与我信任的模型之间的距离」**。量化问的是「INT4 相对 FP16 漂移了多远？」蒸馏问的是「学生距离老师有多远？」RLHF 问的是「调优后的策略偏离基座有多远？」投机解码问的是「draft 距离 target 有多近？」四者都是同一个数——两个 next-token 分布之间的 `D_KL`——而本讲余下的部分会把它构建得足够严谨，使你能够计算它、证明它的性质，并知道任何给定技术真正在优化的是它的哪一面（forward 还是 reverse）。

---

## 学习目标

学完本讲，你应当能够：

1. 定义 `D_KL(p ‖ q)` 并陈述其操作意义：编码来自 `p` 的样本时、使用一套为 `q` 构建的编码所付出的**期望额外 nats**。
2. 证明三个关键性质——`D_KL ≥ 0`（Gibbs/Jensen）、`= 0` 当且仅当 `p = q`、以及**不对称性**——并解释为什么 KL 是一种*散度*，而不是距离。
3. 推导主恒等式 `D_KL(p ‖ q) = H(p,q) − H(p)`，并得出**MLE 就是 forward-KL 最小化**的结论，从而把 Lecture 02–03 串联起来。
4. 区分 **forward** KL（mass-covering、zero-avoiding）与 **reverse** KL（mode-seeking、zero-forcing），并把每种常见技术对应到它所最小化的那一种。
5. 当应用确实需要对称性时，转向**对称**的近亲——Jensen–Shannon、total variation——并了解 JSD 的界与度量性质。
6. 在 PyTorch 中计算两个 LM logit 向量之间的**逐 token KL**，而不落入 `F.kl_div` 的参数顺序陷阱，并把平均语料 KLD 读作单一忠实度数值（完整论述见 [Lecture 05](Lecture-05.md)）。

---


<details>
<summary>English original</summary>

**Lecture 04 - KL Divergence: The Gap Between Two Distributions**

**Collection:** [Logprobs, Perplexity & KL Divergence](README.md) | **Previous:** [← Lecture 03](Lecture-03.md) | **Next:** [Lecture 05](Lecture-05.md)

---

Two lectures ago we earned the left side of the master identity — cross-entropy `H(p,q)`, the loss you train on, and entropy `H(p)`, the floor it can never beat. This lecture earns the third and final term: the **KL divergence** `D_KL(p ‖ q)`, the part of the identity that is nobody's fault but yours.

`H(p,q) = H(p) + D_KL(p ‖ q)`. Read it as an accounting statement. Entropy is the bill reality sends you — the data's own uncertainty, which no model can refund. KL divergence is the surcharge you add on top by using the wrong distribution. It is **non-negative, zero only when your model is exactly right, and entirely avoidable in principle.** Every cross-entropy you have ever measured was entropy plus this surcharge, and you cannot tell the two apart from the loss number alone — which is why a falling loss can mean "the data got easier" rather than "my model got better."

That is the abstract reading. The operational one is what makes KL the load-bearing metric of this phase: it is the **universal "distance from the model I trust."** Quantization asks "how far did INT4 drift from FP16?" Distillation asks "how far is the student from the teacher?" RLHF asks "how far has the tuned policy strayed from the base?" Speculative decoding asks "how close is the draft to the target?" All four are one number — `D_KL` between two next-token distributions — and the rest of this lecture builds it carefully enough that you can compute it, prove its properties, and know which of its two faces (forward or reverse) any given technique is actually optimizing.

---

**Learning objectives**

By the end of this lecture, you should be able to:

1. Define `D_KL(p ‖ q)` and state its operational meaning: the **expected extra nats** you pay coding samples from `p` with a code built for `q`.
2. Prove the three properties that matter — `D_KL ≥ 0` (Gibbs/Jensen), `= 0` iff `p = q`, and **asymmetry** — and explain why KL is a *divergence*, not a distance.
3. Derive the master identity `D_KL(p ‖ q) = H(p,q) − H(p)` and conclude that **MLE is forward-KL minimization**, tying Lectures 02–03 together.
4. Distinguish **forward** (mass-covering, zero-avoiding) from **reverse** (mode-seeking, zero-forcing) KL, and map each common technique to the one it minimizes.
5. Reach for a **symmetric** cousin — Jensen–Shannon, total variation — when the application genuinely needs symmetry, and know JSD's bound and metric properties.
6. Compute **per-token KL** between two LM logit vectors in PyTorch without falling into the `F.kl_div` argument-order trap, and read mean corpus KLD as a single faithfulness number (full treatment in [Lecture 05](Lecture-05.md)).

---

</details>

## 1. 定义与含义

对于同一支撑集上的两个分布 `p` 和 `q`，**`q` 相对于 `p` 的 Kullback–Leibler 散度**为

```text
  D_KL(p ‖ q) = Σ_x p(x) log( p(x) / q(x) ) = E_p[ log p(x) − log q(x) ]
```


立刻可以读出三点：

- **期望是在 `p` 下取的。** 每一项都以*现实*（可信分布）产生 `x` 的频率来加权，而不是按模型 `q` 所相信的来加权。这正是 §2 中不对称性的全部来源——把执笔的分布换一个，得到的就是另一个数。
- **它是 log 概率之差的平均。** `log p − log q` 是逐事件的惊讶度差：在每个结果处，`q` 比真值 `p` 多给出多少 nats 的惊讶？在 `p` 下对这个差取平均，便得到该散度。
- **单位随 log 走。** 自然对数给出 nats；`log2` 给出 bits。默认用 nats，只有在与以 bits-per-token 计的困惑度比较时才写 `bits = log2`。

其操作意义是一句编码陈述，也是要记住的那句。Shannon 信源编码定理指出，对信源 `p` 最省码长的码平均每个符号 `x` 约用 `−log p(x)` nats，平均为 `H(p)`。假设你改为按分布 `q` 来构建编码——于是你在符号 `x` 上花 `−log q(x)` nats——但符号仍不断来自真实信源 `p`。你的平均代价现在是 `H(p,q) = −Σ p log q`，即交叉熵。相对最优值 `H(p)` 的**超出量**为

```text
  H(p,q) − H(p) = E_p[ −log q ] − E_p[ −log p ] = E_p[ log p − log q ] = D_KL(p ‖ q).
```


所以 `D_KL(p ‖ q)` 就是**用 `q`-最优编码去编码 `p` 数据时，每个符号浪费的额外 nats 的期望数。** 等价地——这也是要记住的说法——它是*当现实是 `p` 时却相信 `q` 所付出的惊讶度代价。* 当 `q = p` 时代价为零：你构建了正确的编码。当 `q` 给某个 `p` 经常产生的事件分配低概率时，那一项 `p(x) log(p(x)/q(x))` 就会爆炸——每当被你欠建模的事件发生，你都要在惊讶度上付出高昂代价。我们刚顺带证明了这条主恒等式；§3 会从求和式出发再慢慢证一遍，因为它值得看两次。

---

## 2. 性质与证明

四条性质界定了你可以怎样使用 KL。前两条你必须能证明；后两条则是不让你误用它的东西。

### (a) 非负性：`D_KL(p ‖ q) ≥ 0`

这就是 **Gibbs 不等式**，它由 Jensen 不等式三行推出。Jensen 指出，对*凹*函数 `φ`（而 `log` 是凹的），`E[φ(Y)] ≤ φ(E[Y])`。把它应用到在 `p` 上取期望的 `Y = q(x)/p(x)`：

```text
  −D_KL(p ‖ q) = E_p[ log( q(x) / p(x) ) ]              (flip the sign inside the log)

              ≤  log( E_p[ q(x) / p(x) ] )               (Jensen: log is concave)

               = log( Σ_x p(x) · q(x)/p(x) )             (write out the expectation)

               = log( Σ_x q(x) )                          (the p(x) cancels)

               = log( 1 ) = 0.                            (q is a distribution)

  Therefore  −D_KL(p ‖ q) ≤ 0,  i.e.  D_KL(p ‖ q) ≥ 0.   ∎
```


相消 `Σ_x p(x)·q(x)/p(x) = Σ_x q(x) = 1` 就是全部诀窍：期望中按 `p` 加权，正是把比值收缩回 `q` 的总质量所需的东西。（严格说，求和只在 `p(x) > 0` 的 `x` 上进行；若那里 `q(x) = 0`，比值发散，`D_KL = +∞`——这对 LM 是真实可能的情形，§6 会讨论。）

这一条不等式是本课程的脊梁。与 §3 的恒等式结合，它说明 `H(p,q) ≥ H(p)`：**交叉熵永远不可能优于熵。** 无论模型多好，都无法把损失压到数据自身的不确定性下限之下，因为这个差距*就是*一个 KL，而 KL 非负。

### (b) 相等时为零：`D_KL(p ‖ q) = 0 ⟺ p = q`（几乎处处）

Jensen 不等式取*等号*，恰当内部的随机变量在 `p`-几乎处处为常数。这里该变量是 `q(x)/p(x)`，故取等要求凡 `p(x) > 0` 处 `q(x)/p(x) = c`（常数）。求和得 `Σ q(x) = c·Σ p(x)`，即 `1 = c·1`，于是在 `p` 的支撑集上 `c = 1` 且 `q(x) = p(x)`。KL 为零恰在两分布重合之时——要拿到完美的零分，没有别的途径。


<details>
<summary>English original</summary>

**1. Definition and meaning**

For two distributions `p` and `q` over the same support, the **Kullback–Leibler divergence of `q` from `p`** is

```text
  D_KL(p ‖ q) = Σ_x p(x) log( p(x) / q(x) ) = E_p[ log p(x) − log q(x) ]
```

Three things to read off immediately:

- **The expectation is under `p`.** You weight every term by how often *reality* (the trusted distribution) produces `x`, not by what the model `q` believes. This is the entire source of the asymmetry in §2 — swap which distribution holds the pen and you get a different number.
- **It is a difference of log-probabilities, averaged.** `log p − log q` is the per-event surprise gap: at each outcome, how many more nats of surprise does `q` assign than the truth `p` does? Average that gap under `p` and you have the divergence.
- **Units follow the log.** Natural log gives nats; `log2` gives bits. We default to nats and write `bits = log2` only when comparing to a perplexity in bits-per-token.

The operational meaning is a coding statement, and it is the one to memorize. Shannon's source-coding theorem says the cheapest code for a source `p` uses about `−log p(x)` nats per symbol `x`, for an average of `H(p)`. Suppose you instead build your code assuming the distribution is `q` — so you spend `−log q(x)` nats on symbol `x` — but the symbols keep arriving from the real source `p`. Your average cost is now `H(p,q) = −Σ p log q`, the cross-entropy. The **excess** over the optimal `H(p)` is

```text
  H(p,q) − H(p) = E_p[ −log q ] − E_p[ −log p ] = E_p[ log p − log q ] = D_KL(p ‖ q).
```

So `D_KL(p ‖ q)` is the **expected number of extra nats per symbol you waste by coding `p`-data with a `q`-optimal code.** Equivalently — and this is the phrase to keep — it is *the surprise penalty for believing `q` when reality is `p`.* When `q = p` the penalty is zero: you built the right code. When `q` puts low probability on something `p` produces often, that term `p(x) log(p(x)/q(x))` blows up — you are paying dearly, in surprise, every time the event you under-modeled occurs. We just proved the master identity in passing; §3 does it again slowly, from the sums, because it is worth seeing twice.

---

**2. Properties, with proofs**

Four properties define how you are allowed to use KL. The first two you must be able to prove; the second two are what stop you from misusing it.

**(a) Non-negativity: `D_KL(p ‖ q) ≥ 0`**

This is **Gibbs' inequality**, and it falls out of Jensen's inequality in three lines. Jensen says that for a *concave* function `φ` (and `log` is concave), `E[φ(Y)] ≤ φ(E[Y])`. Apply it to `Y = q(x)/p(x)` under the expectation taken over `p`:

```text
  −D_KL(p ‖ q) = E_p[ log( q(x) / p(x) ) ]              (flip the sign inside the log)

              ≤  log( E_p[ q(x) / p(x) ] )               (Jensen: log is concave)

               = log( Σ_x p(x) · q(x)/p(x) )             (write out the expectation)

               = log( Σ_x q(x) )                          (the p(x) cancels)

               = log( 1 ) = 0.                            (q is a distribution)

  Therefore  −D_KL(p ‖ q) ≤ 0,  i.e.  D_KL(p ‖ q) ≥ 0.   ∎
```

The cancellation `Σ_x p(x)·q(x)/p(x) = Σ_x q(x) = 1` is the whole trick: the `p`-weighting in the expectation is exactly what is needed to collapse the ratio back to `q`'s total mass. (Strictly, the sum runs over `x` where `p(x) > 0`; if `q(x) = 0` there, the ratio diverges and `D_KL = +∞` — a real possibility for LMs, addressed in §6.)

This single inequality is the backbone of the course. Combined with the identity of §3 it says `H(p,q) ≥ H(p)`: **cross-entropy can never beat entropy.** No model, however good, drives the loss below the data's own uncertainty floor, because the gap *is* a KL and KL is non-negative.

**(b) Zero iff equal: `D_KL(p ‖ q) = 0 ⟺ p = q` (almost everywhere)**

Jensen's inequality is an *equality* exactly when the random variable inside is constant `p`-almost-everywhere. Here that variable is `q(x)/p(x)`, so equality requires `q(x)/p(x) = c` (constant) wherever `p(x) > 0`. Summing, `Σ q(x) = c·Σ p(x)`, i.e. `1 = c·1`, so `c = 1` and `q(x) = p(x)` on the support of `p`. KL is zero precisely when the two distributions coincide — there is no other way to score a perfect zero.

</details>

### (c) 不对称性：一般情况下 `D_KL(p ‖ q) ≠ D_KL(q ‖ p)`

KL **不对称**，而且这个差异不是舍入误差 —— 两种顺序回答的是不同的问题，差距可达若干个数量级。一个在两事件支撑集上的最小数值例证：

```text
  p = (0.9, 0.1),  q = (0.5, 0.5)        (natural log)

  D_KL(p ‖ q) = 0.9·log(0.9/0.5) + 0.1·log(0.1/0.5)
              = 0.9·(0.5878)   + 0.1·(−1.6094)
              = 0.5290 − 0.1609 = 0.368 nats

  D_KL(q ‖ p) = 0.5·log(0.5/0.9) + 0.5·log(0.5/0.1)
              = 0.5·(−0.5878)  + 0.5·(1.6094)
              = −0.2939 + 0.8047 = 0.511 nats

  0.368 ≠ 0.511.                          ∎
```

数值之所以不同，是因为平均所用的权重不同：`D_KL(p ‖ q)` 用 `p` 对 surprise gap 加权，`D_KL(q ‖ p)` 用 `q` 加权。§4 把这一不对称性变成全讲中影响最大的一项设计区分。

### (d) 不是度量 —— 所以称之为 *散度*

度量需要满足对称性和三角不等式。KL **两者都不满足**：由 (c) 可知它不满足对称性，而且一般不存在常数 `c` 使 `D_KL(p ‖ r) ≤ c·(D_KL(p ‖ q) + D_KL(q ‖ r))` 成立。此外，当两个分布在任何直观意义上都明显“接近”时（一个零落在错误的位置），它也可以是 `+∞`。所以它是一个**散度**：一种非负的、满足不可分辨者同一性的不相似性度量，而 *不是* 距离。用词要精确 —— 说“两者之间的 KL 散度”，绝不要说“KL 距离” —— 因为这种不对称性承担着关键作用，而 §5 正是为确实需要真正对称距离的情形而存在。

---

## 3. 主恒等式，证明

这个恒等式在 §1 和 Lecture 02 中已非正式地出现过。现在逐项从定义出发给出它：

```text
  D_KL(p ‖ q) = Σ_x p(x) log( p(x) / q(x) )

              = Σ_x p(x) [ log p(x) − log q(x) ]              (log of a quotient)

              = Σ_x p(x) log p(x)  −  Σ_x p(x) log q(x)       (split the sum)

              = −H(p)              −  ( −H(p,q) )             (defs of H(p), H(p,q))

              = H(p,q) − H(p).                                 ∎

  Rearranged:   H(p,q) = H(p) + D_KL(p ‖ q).
```

现在看那个支撑起整门课程的推论。在有监督 LM 训练中，`p` 是**数据分布** —— 对于下一 token 预测，它实际上是在观测到的下一 token 上的 one-hot delta，由数据集固定。`q` 则是**模型**。训练损失是在语料上平均后的交叉熵 `H(p_data, q_model)`（Lecture 02）。用该恒等式将其拆分：

```text
  loss = H(p_data, q_model) = H(p_data) + D_KL(p_data ‖ q_model)
                              └── fixed ──┘   └─── the only part you can move ───┘
```

`H(p_data)` 不依赖于参数 —— 它只是数据本身的性质。**因此最小化交叉熵损失就*恰好*是最小化前向 KL `D_KL(p_data ‖ q_model)`。** 最大似然估计 —— 几乎所有预训练 LM 背后的目标 —— *就是*前向 KL 最小化；那个加性项 `H(p_data)` 只是无法施加影响、因而可以忽略的偏移量。这就是为什么三个数字中的两个并不真正是各自独立的目标：把困惑度压下去（Lecture 03），从数据到模型的前向 KL 就会随之机械地缩小。所上报的度量与所最小化的散度，透过这个恒等式看，是同一回事。

---


<details>
<summary>English original</summary>

**(c) Asymmetry: `D_KL(p ‖ q) ≠ D_KL(q ‖ p)` in general**

KL is **not symmetric**, and the difference is not a rounding error — the two orderings answer different questions and can differ by orders of magnitude. A minimal numerical witness over a two-event support:

```text
  p = (0.9, 0.1),  q = (0.5, 0.5)        (natural log)

  D_KL(p ‖ q) = 0.9·log(0.9/0.5) + 0.1·log(0.1/0.5)
              = 0.9·(0.5878)   + 0.1·(−1.6094)
              = 0.5290 − 0.1609 = 0.368 nats

  D_KL(q ‖ p) = 0.5·log(0.5/0.9) + 0.5·log(0.5/0.1)
              = 0.5·(−0.5878)  + 0.5·(1.6094)
              = −0.2939 + 0.8047 = 0.511 nats

  0.368 ≠ 0.511.                          ∎
```

The numbers differ because the averaging weights differ: `D_KL(p ‖ q)` weights the surprise gap by `p`, `D_KL(q ‖ p)` weights it by `q`. §4 turns this asymmetry into the single most consequential design distinction in the lecture.

**(d) Not a metric — so call it a *divergence***

A metric needs symmetry and the triangle inequality. KL has **neither**: it fails symmetry by (c), and there is no constant `c` for which `D_KL(p ‖ r) ≤ c·(D_KL(p ‖ q) + D_KL(q ‖ r))` holds in general. It can also be `+∞` while the distributions are clearly "close" in any visual sense (one zero in the wrong place). So it is a **divergence**: a non-negative, identity-of-indiscernibles measure of dissimilarity that is *not* a distance. Speak precisely — "the KL divergence between," never "the KL distance" — because the asymmetry is load-bearing, and §5 exists exactly for the cases where you do need a true symmetric distance.

---

**3. The master identity, proven**

We met the identity informally in §1 and in Lecture 02. Here it is from the definitions, term by term:

```text
  D_KL(p ‖ q) = Σ_x p(x) log( p(x) / q(x) )

              = Σ_x p(x) [ log p(x) − log q(x) ]              (log of a quotient)

              = Σ_x p(x) log p(x)  −  Σ_x p(x) log q(x)       (split the sum)

              = −H(p)              −  ( −H(p,q) )             (defs of H(p), H(p,q))

              = H(p,q) − H(p).                                 ∎

  Rearranged:   H(p,q) = H(p) + D_KL(p ‖ q).
```

Now the consequence that justifies the whole course. In supervised LM training, `p` is the **data distribution** — for next-token prediction it is effectively a one-hot delta on the observed next token, fixed by the dataset. `q` is your **model**. The training loss is the cross-entropy `H(p_data, q_model)` averaged over the corpus (Lecture 02). Split it with the identity:

```text
  loss = H(p_data, q_model) = H(p_data) + D_KL(p_data ‖ q_model)
                              └── fixed ──┘   └─── the only part you can move ───┘
```

`H(p_data)` does not depend on your parameters — it is a property of the data alone. **So minimizing cross-entropy loss is *exactly* minimizing the forward KL `D_KL(p_data ‖ q_model)`.** Maximum-likelihood estimation, the objective behind essentially every pretrained LM, *is* forward-KL minimization; the additive `H(p_data)` is just an offset you cannot influence and therefore ignore. This is why two of our three numbers are not really separate goals: drive the perplexity down (Lecture 03), and you are mechanically shrinking the forward KL from the data to your model. The metric you report and the divergence you minimize are the same act, viewed through the identity.

---

</details>

## 4. Forward KL 与 reverse KL

由于 KL 是非对称的（§2c），`D_KL(p ‖ q)` 与 `D_KL(q ‖ p)` 是*目标不同、解也不同的两个目标*，选错哪一个会在不知不觉中决定你的模型是铺开还是坍塌。把 `p` 固定为可信的目标，让 `q` 成为被优化的那一方。

**Forward KL —— `D_KL(p ‖ q)` —— 是 mass-covering / zero-avoiding 的。** 期望在 `p` 下取，因此每个满足 `p(x) > 0` 的 `x` 都贡献一项 `p(x) log(p(x)/q(x))`。若 `q(x) → 0` 而 `p(x) > 0`，则该项 `→ +∞`。因此优化器**极其害怕在 `p` 有质量的地方放零质量**——它宁可把 `q` 摊薄以覆盖 `p` 的整个支撑集，也不愿留下空洞。在 `p(x) = 0` 处，forward KL 对 `q(x)` 无所谓（该项为 `0·log0 = 0`），所以 `q` 可以随意把质量泄漏到空区域。净效果：`q` 变得**宽泛、平均化、覆盖所有 mode。**

**Reverse KL —— `D_KL(q ‖ p)` —— 是 mode-seeking / zero-forcing 的。** 此时期望在 `q` 下取，被惩罚的项是 `q(x) log(q(x)/p(x))`。凡是 `p(x)` 小、`q(x)` 不小的地方，比值就大，就要付出代价。最便宜的逃脱方式是**在那里设置 `q(x) = 0`**——`0·log0 = 0` 不花任何代价。于是 reverse KL 会激进地把 `q` 在 `p` 的高密度区域之外强制为零，`q` 则坍塌到**单个 mode** 上，完全无视 `p` 的其余部分。

经典的直觉例子是用单个高斯 `q` 去近似一个**双峰 `p`**（两个分离的峰）：

```text
   p:   /\        /\          two modes, a valley between

   forward  D_KL(p ‖ q):   q straddles BOTH bumps — one wide, flat
                           Gaussian centered in the valley, covering
                           all of p's mass (and the empty middle too).
                           "Cover everything p does."

   reverse  D_KL(q ‖ p):   q snaps onto ONE bump — a narrow Gaussian
                           sitting on a single mode, ignoring the other.
                           "Be confidently right somewhere p is."
```

两者都谈不上“正确”——它们编码的是不同的风险偏好。forward KL 绝不放弃真实分布访问过的任何区域（适合不能给真实数据分配零概率的生成模型）；reverse KL 产生尖锐、果断、有时过度自信的行为（当你希望策略敢于下注时适用）。某个技术用的是哪一种，是一个值得记住的设计事实：

| Technique | Minimizes | Personality | Why |
|---|---|---|---|
| **MLE / 预训练** | forward `D_KL(p_data ‖ q)` | mass-covering | §3——它*就是*交叉熵损失；不能把真实 token 归零 |
| **知识蒸馏** | forward `D_KL(p_teacher ‖ q_student)` | mass-covering | student 匹配 teacher 的完整 soft 分布（Lecture 05） |
| **Label smoothing / forward-KL 微调** | forward | mass-covering | 与 MLE 同族，只是目标被软化 |
| **变分推断（ELBO）** | reverse `D_KL(q ‖ p_post)` | mode-seeking | 可计算：期望在你可控的 `q` 下取 |
| **RLHF / 策略优化（PPO、GRPO）** | reverse `D_KL(q_policy ‖ p_ref)` | mode-seeking | 用 KL 把*采样*策略正则向参考策略（Lecture 05） |

一个有用的记忆法：当你不能*漏掉*目标分布所做的任何事情（覆盖）时，最小化 **forward** KL；当你不能*凭空造出*目标分布没有的东西（集中）时，最小化 **reverse** KL。预训练与蒸馏是覆盖问题；对齐与变分近似是集中问题。

---


<details>
<summary>English original</summary>

**4. Forward vs reverse KL**

Because KL is asymmetric (§2c), `D_KL(p ‖ q)` and `D_KL(q ‖ p)` are *different objectives with different solutions*, and choosing the wrong one quietly determines whether your model spreads out or collapses. Fix `p` as the trusted target and let `q` be the thing you optimize.

**Forward KL — `D_KL(p ‖ q)` — is mass-covering / zero-avoiding.** The expectation is under `p`, so every `x` with `p(x) > 0` contributes a term `p(x) log(p(x)/q(x))`. If `q(x) → 0` where `p(x) > 0`, that term `→ +∞`. The optimizer is therefore **terrified of putting zero mass anywhere `p` has mass** — it would rather smear `q` thin to cover all of `p`'s support than leave a hole. Where `p(x) = 0`, forward KL is indifferent to `q(x)` (the term is `0·log0 = 0`), so `q` is free to leak mass into empty regions. Net effect: `q` becomes **broad, averaging, mode-covering.**

**Reverse KL — `D_KL(q ‖ p)` — is mode-seeking / zero-forcing.** Now the expectation is under `q`, and the penalized term is `q(x) log(q(x)/p(x))`. Wherever `p(x)` is small but `q(x)` is not, the ratio is large and you pay. The cheapest escape is to **set `q(x) = 0` there** — `0·log0 = 0` costs nothing. So reverse KL aggressively forces `q` to zero outside `p`'s high-density regions, and `q` collapses onto **one mode**, ignoring the rest of `p` entirely.

The classic intuition is a **bimodal `p`** (two separated bumps) approximated by a single Gaussian `q`:

```text
   p:   /\        /\          two modes, a valley between

   forward  D_KL(p ‖ q):   q straddles BOTH bumps — one wide, flat
                           Gaussian centered in the valley, covering
                           all of p's mass (and the empty middle too).
                           "Cover everything p does."

   reverse  D_KL(q ‖ p):   q snaps onto ONE bump — a narrow Gaussian
                           sitting on a single mode, ignoring the other.
                           "Be confidently right somewhere p is."
```

Neither is "correct" — they encode different risk preferences. Forward KL never abandons a region the truth visits (good for a generative model that must not assign zero probability to real data); reverse KL produces sharp, decisive, sometimes overconfident behavior (good when you want the policy to commit). Which one a technique uses is a design fact worth memorizing:

| Technique | Minimizes | Personality | Why |
|---|---|---|---|
| **MLE / pretraining** | forward `D_KL(p_data ‖ q)` | mass-covering | §3 — it *is* the cross-entropy loss; cannot zero out real tokens |
| **Knowledge distillation** | forward `D_KL(p_teacher ‖ q_student)` | mass-covering | student matches the teacher's full soft distribution (Lecture 05) |
| **Label smoothing / forward-KL fine-tune** | forward | mass-covering | same family as MLE against a softened target |
| **Variational inference (ELBO)** | reverse `D_KL(q ‖ p_post)` | mode-seeking | tractable: expectation under the `q` you control |
| **RLHF / policy optimization (PPO, GRPO)** | reverse `D_KL(q_policy ‖ p_ref)` | mode-seeking | KL-regularize the *sampling* policy toward the reference (Lecture 05) |

A useful mnemonic: you minimize **forward** KL when you must not *miss* anything the target does (coverage), and **reverse** KL when you must not *invent* anything the target doesn't (concentration). Pretraining and distillation are coverage problems; alignment and variational approximation are concentration problems.

---

</details>

## 5. 对称的近亲

有时应用确实想要一个单一的对称数值——“这两个分布有多不同？”——而不存在优先参考。KL 拒绝回答该问题（它总是偏向第一个参数）。有三种标准工具能够回答。

**Jensen–Shannon 散度（JSD）** 通过让两者都经由它们的混合分布 `m = ½(p + q)` 来对称化 KL：

```text
  m = ½(p + q)

  JSD(p, q) = ½ D_KL(p ‖ m) + ½ D_KL(q ‖ m)
```

它的性质恰好是 KL 所缺乏的。它在构造上就是**对称的**：交换 `p` 和 `q` 后 `m` 不变，同时两项互换。它**始终有限**——因为 `m(x) ≥ ½ p(x)` 和 `m(x) ≥ ½ q(x)`，即使 `p` 和 `q` 的支撑集不相交（这正是使原始 KL 变为 `+∞` 的失效模式），其中的任一项比值也不会发散。它**有界**：以 nats 计为 `0 ≤ JSD(p, q) ≤ log 2`（以 bit 计为 `≤ 1`），恰好当 `p` 和 `q` 的支撑集不相交时取到 `log 2`。而关键在于，**`√JSD` 是真正的度量**——它满足对称性和三角不等式——因此当你需要用真实距离对分布进行*聚类*、*嵌入*或*阈值化*时，`√JSD` 是有原则的选择，而 KL 则不是。

**全变差距离（Total variation distance，TV）** 是另一件主力工具，也是最直观的：

```text
  TV(p, q) = ½ Σ_x | p(x) − q(x) |
```

TV 是真正的度量，取值于 `[0, 1]`，并且可以直接读作“任一分布赋予任何事件的最大概率差”。它通过 **Pinsker 不等式** `TV(p, q) ≤ √( ½ D_KL(p ‖ q) )` 与 KL 关联，该不等式让一个 KL 界能够证明一个 TV 界——当你真正关心的是最坏情况下的概率差时，这很有用。

何时用哪个：只要有优先参考，并且你想要编码/惊异值的解释，就用**普通 KL**——量化（FP16 是参考）、蒸馏（教师是参考）、RLHF（基础策略是参考）。这几乎涵盖了本阶段的全部内容，因此本课程其余部分用的是 KL，而非 JSD。只有当两个分布真正对等时（比较两个独立训练的模型、在无“ground truth”的情况下对两个检查点做 A/B，或需要为仪表盘提供一个有界、对称、永不为无穷的分数），才使用 **JSD 或 TV**。

---


<details>
<summary>English original</summary>

**5. Symmetric cousins**

Sometimes the application genuinely wants a single symmetric number — "how different are these two distributions?" with no privileged reference. KL refuses to answer that question (it always privileges the first argument). Three standard tools do.

**Jensen–Shannon divergence (JSD)** symmetrizes KL by routing both through their mixture `m = ½(p + q)`:

```text
  m = ½(p + q)

  JSD(p, q) = ½ D_KL(p ‖ m) + ½ D_KL(q ‖ m)
```

Its properties are exactly the ones KL lacks. It is **symmetric** by construction: swapping `p` and `q` leaves `m` unchanged and swaps the two terms. It is **always finite** — because `m(x) ≥ ½ p(x)` and `m(x) ≥ ½ q(x)`, neither ratio inside can diverge even if `p` and `q` have disjoint support (the failure mode that sends raw KL to `+∞`). It is **bounded**: `0 ≤ JSD(p, q) ≤ log 2` in nats (`≤ 1` in bits), reaching `log 2` exactly when `p` and `q` have disjoint support. And critically, **`√JSD` is a true metric** — it satisfies symmetry and the triangle inequality — so when you need to *cluster*, *embed*, or *threshold* distributions with a real distance, `√JSD` is the principled choice where KL is not.

**Total variation distance (TV)** is the other workhorse, and the most intuitive:

```text
  TV(p, q) = ½ Σ_x | p(x) − q(x) |
```

TV is a genuine metric, lives in `[0, 1]`, and reads directly as "the largest gap in probability either distribution assigns to any event." It is linked to KL by **Pinsker's inequality**, `TV(p, q) ≤ √( ½ D_KL(p ‖ q) )`, which lets a KL bound certify a TV bound — useful when a worst-case probability gap is what you actually care about.

When to reach for which: use **plain KL** whenever there is a privileged reference and you want the coding/surprise interpretation — quantization (FP16 is the reference), distillation (the teacher is the reference), RLHF (the base policy is the reference). That covers almost everything in this phase, which is why the rest of the course is KL, not JSD. Reach for **JSD or TV** only when the two distributions are genuinely peers (comparing two independently trained models, A/B-ing two checkpoints with no "ground truth," or needing a bounded, symmetric, never-infinite score for a dashboard).

---

</details>

## 6. 两个 LM next-token 分布之间的 KL

这正是本课程要计算的具体对象。在单个位置上，语言模型会输出词表上的一个 logit 向量；softmax 将其转为 next-token 分布。给定两个模型——一个 **参考** `p`（比如 FP16）和一个 **候选** `q`（比如 INT4）——在观察 *同一个* context 时，逐 token KL 就是它们两个词表分布之间的 KL：

```text
  per-token  D_KL(p ‖ q) = Σ_{v ∈ vocab} p(v) log( p(v) / q(v) )
```

其中 `p = softmax(reference_logits)` 与 `q = softmax(candidate_logits)`。在语料库的每个 token 位置上取平均，就得到 **mean KLD**——一个标量，以每 token 多少 nats 表示：在真实文本上，候选模型的 next-token 信念偏离参考模型有多远。这正是 llama.cpp 用来评估量化保真度的那个忠实度数值；第 05 讲对它做了完整处理（连同 top-token 一致性和概率 RMS）。公式明确揭示了一个微妙之处：如果候选模型给一个参考模型认为合理的 token 分配了 *零* 概率，那么该项为 `+∞`。真实的 softmax 输出从不会精确为零，但在低位宽量化或激进的 top-k 截断之后，某些 token 可以逼近零到足以让逐 token KL 飙升——这是有信息量的，不是 bug：它是在大声标出一个被量化抛弃的 token。

在 PyTorch 中，这个函数是 `F.kl_div`，它有一个臭名昭著的调用约定，是 KL 数值算错的最常见单一来源：


<details>
<summary>English original</summary>

**6. KL between two LM next-token distributions**

Now the concrete object this course is built to compute. At a single position, a language model emits a logit vector over the vocabulary; softmax turns it into a next-token distribution. Given two models — a **reference** `p` (say FP16) and a **candidate** `q` (say INT4) — looking at the *same* context, the per-token KL is the KL between their two vocabulary distributions:

```text
  per-token  D_KL(p ‖ q) = Σ_{v ∈ vocab} p(v) log( p(v) / q(v) )
```

with `p = softmax(reference_logits)` and `q = softmax(candidate_logits)`. Average this over every token position in a corpus and you get **mean KLD** — a single scalar that says, in nats per token, how far the candidate's next-token beliefs drift from the reference's across real text. This is precisely the faithfulness number llama.cpp reports to grade quantization; Lecture 05 gives it the full treatment (alongside top-token agreement and probability RMS). One subtlety the formula makes explicit: if the candidate assigns *zero* to a token the reference finds plausible, that term is `+∞`. Real softmax output is never exactly zero, but after low-bit quantization or aggressive top-k truncation a token can get close enough that the per-token KL spikes — which is informative, not a bug: it is the metric loudly flagging a token the quant abandoned.

In PyTorch, the function is `F.kl_div`, and it has a notorious calling convention that is the single most common source of wrong KL numbers:

</details>

```python
import torch
import torch.nn.functional as F

# ref_logits, cand_logits: shape [T, V] — one logit row per token position.
# Convention here: p = reference (trusted), q = candidate, computing D_KL(p ‖ q).

def per_token_kl(ref_logits: torch.Tensor, cand_logits: torch.Tensor) -> torch.Tensor:
    """Returns a length-T tensor of D_KL(p ‖ q) in NATS, one value per position."""
    log_p = F.log_softmax(ref_logits,  dim=-1)   # log p  (reference)
    log_q = F.log_softmax(cand_logits, dim=-1)   # log q  (candidate)
    p     = log_p.exp()                          # p
    # D_KL(p ‖ q) = Σ p (log p − log q), summed over vocab, per token:
    return (p * (log_p - log_q)).sum(dim=-1)     # [T]

# --- The same thing via F.kl_div, demonstrating the two gotchas ---
# GOTCHA 1 (argument order): F.kl_div(input, target) computes
#     Σ target * (log target − input),
# i.e. it expects INPUT = log q (the model/candidate, already log'd) and
#      TARGET = p (the reference). The order is (log-q, p) — *backwards* from
#      how we write D_KL(p ‖ q). Passing (log_p, q) silently computes the
#      REVERSE KL and you will never see an error, only a wrong number.
# GOTCHA 2 (log_target): by default target is taken as a PROBABILITY. If your
#      target is already in log-space, you MUST pass log_target=True, or kl_div
#      will exp() it a second time and return garbage.

def per_token_kl_via_F(ref_logits, cand_logits):
    log_p = F.log_softmax(ref_logits,  dim=-1)   # log p  (reference / TARGET)
    log_q = F.log_softmax(cand_logits, dim=-1)   # log q  (candidate / INPUT)
    # input = log_q, target = log_p, reduction='none', log_target=True:
    kl = F.kl_div(log_q, log_p, reduction="none", log_target=True)  # [T, V]
    return kl.sum(dim=-1)                          # [T]  == per_token_kl(...)

# mean corpus KLD — the single faithfulness number (Lecture 05):
#   mean_kld = torch.cat([per_token_kl(r, c) for r, c in batches]).mean()
```

这些坑的要点是：`F.kl_div` 的第一个参数是 **`q` 的对数**，第二个参数是 **`p`**，与 `D_KL(p ‖ q)` 的阅读顺序正好相反；当你的目标值本身就是 log-prob 时，`log_target=True` 是必需的。拿不准时，就显式地按 `(p * (log_p - log_q)).sum(-1)` 来计算——算术完全一样，读起来与数学式完全对应，而且不会被悄悄对调。


<details>
<summary>English original</summary>

```python
import torch
import torch.nn.functional as F

# ref_logits, cand_logits: shape [T, V] — one logit row per token position.
# Convention here: p = reference (trusted), q = candidate, computing D_KL(p ‖ q).

def per_token_kl(ref_logits: torch.Tensor, cand_logits: torch.Tensor) -> torch.Tensor:
    """Returns a length-T tensor of D_KL(p ‖ q) in NATS, one value per position."""
    log_p = F.log_softmax(ref_logits,  dim=-1)   # log p  (reference)
    log_q = F.log_softmax(cand_logits, dim=-1)   # log q  (candidate)
    p     = log_p.exp()                          # p
    # D_KL(p ‖ q) = Σ p (log p − log q), summed over vocab, per token:
    return (p * (log_p - log_q)).sum(dim=-1)     # [T]

# --- The same thing via F.kl_div, demonstrating the two gotchas ---
# GOTCHA 1 (argument order): F.kl_div(input, target) computes
#     Σ target * (log target − input),
# i.e. it expects INPUT = log q (the model/candidate, already log'd) and
#      TARGET = p (the reference). The order is (log-q, p) — *backwards* from
#      how we write D_KL(p ‖ q). Passing (log_p, q) silently computes the
#      REVERSE KL and you will never see an error, only a wrong number.
# GOTCHA 2 (log_target): by default target is taken as a PROBABILITY. If your
#      target is already in log-space, you MUST pass log_target=True, or kl_div
#      will exp() it a second time and return garbage.

def per_token_kl_via_F(ref_logits, cand_logits):
    log_p = F.log_softmax(ref_logits,  dim=-1)   # log p  (reference / TARGET)
    log_q = F.log_softmax(cand_logits, dim=-1)   # log q  (candidate / INPUT)
    # input = log_q, target = log_p, reduction='none', log_target=True:
    kl = F.kl_div(log_q, log_p, reduction="none", log_target=True)  # [T, V]
    return kl.sum(dim=-1)                          # [T]  == per_token_kl(...)

# mean corpus KLD — the single faithfulness number (Lecture 05):
#   mean_kld = torch.cat([per_token_kl(r, c) for r, c in batches]).mean()
```

The takeaway from the gotchas: `F.kl_div`'s first argument is the **log of `q`** and its second is **`p`**, the opposite of the `D_KL(p ‖ q)` reading order, and `log_target=True` is mandatory when your target is already a log-prob. When in doubt, compute it explicitly as `(p * (log_p - log_q)).sum(-1)` — it is the same arithmetic, reads exactly like the math, and cannot be silently reversed.

</details>

> **硬件视角：** 计算 KL 严格比计算困惑度更重，而且代价在*内存*，不在 flops。一次普通的 PPL 运行只需要候选模型在每个位置上*那一个*实际生成的 token 的 log-prob —— 一个标量。KL 则需要每个位置上**来自*两个*模型的整个词表分布**：两个完整的 `[T, V]` logit 张量，约为 PPL 一遍的 **2× logit 内存/存储**（而 `V` 是 32K–256K，所以全语料的 logit dump 体量很大）。标准做法是计算并**缓存一次 FP16 参考 logits** —— 为整个 eval 语料把 `log_softmax(ref_logits)` 写到磁盘 —— 然后**把每个候选 quant 对着这份缓存参考做流式处理**，于是昂贵的全精度前向传播只付一次，之后每一次 quant 比较都只是一次廉价的前向传播加一次向量运算。`--kl-divergence` 评测器正是这样构建的，才能保持可处理。

> **2026 更新：** KL 散度 —— 而非单独的困惑度 —— 现在是给量化打分的首选*内在*指标。llama.cpp 通过 `--kl-divergence` 直接暴露它，而该领域已收敛于：平均 KLD（加上 top-token 一致性）与感知质量的相关性优于 PPL 差值，后者相比之下相当“粗糙”（Lecture 05）。同一个术语在对齐中反复出现：**PPO、DPO 和 GRPO 都带有朝向参考策略的显式 KL 惩罚项**。RLHF 的目标不断翻新 —— 每月流行的算法都在变 —— 但**KL 项是其中每一个的不变量**，这正是本讲在课程中处于当前位置的全部原因。Lecture 05 把这两个观察都放到真实的评测器和真实的 RLHF 损失上检验。

---


<details>
<summary>English original</summary>

> **Hardware lens:** computing KL is strictly heavier than computing perplexity, and the cost is *memory*, not flops. A plain PPL run only needs the candidate's log-prob at the *one* realized token per position — a scalar. KL needs the **entire vocabulary distribution from *both* models** at every position: two full `[T, V]` logit tensors, roughly **2× the logit memory/storage** of a PPL pass (and `V` is 32K–256K, so a full-corpus logit dump is large). The standard pattern is to compute and **cache the FP16 reference logits once** — write `log_softmax(ref_logits)` to disk for the whole eval corpus — then **stream each candidate quant against that cached reference**, so the expensive full-precision forward pass is paid a single time and every subsequent quant comparison is one cheap forward pass plus a vector op. This is exactly how `--kl-divergence` graders are built to stay tractable.

> **2026 update:** KL divergence — not perplexity alone — is now the preferred *intrinsic* metric for grading quantization. llama.cpp exposes it directly via `--kl-divergence`, and the field has converged on mean KLD (plus top-token agreement) correlating better with perceived quality than a PPL delta, which is "rough" by comparison (Lecture 05). The same term reappears across alignment: **PPO, DPO, and GRPO all carry an explicit KL penalty** toward a reference policy. RLHF objectives churn — the algorithm of the month changes — but the **KL term is the invariant across every one of them**, which is the whole reason this lecture sits where it does in the course. Lecture 05 turns both observations loose on real graders and real RLHF losses.

---

</details>

## 截至

2026 年 6 月。数学部分已经定型（Kullback–Leibler 1951；Gibbs 不等式；Jensen）。有时效性的是工具链：llama.cpp 的 `--kl-divergence` 量化评分器，以及当前共识——平均 KLD 加 top-token 一致率比单看 PPL 更能反映质量，再加 RLHF 版图（PPO/DPO/GRPO），其 KL 惩罚项是稳定的核心。引用具体细节前，请重新核实评分器的 flag 和当下主流的策略优化方法；等式本身及其证明不会过期。


<details>
<summary>English original</summary>

**Current as of**

June 2026. The mathematics is settled (Kullback–Leibler 1951; Gibbs' inequality; Jensen). What is dated is the tooling: llama.cpp's `--kl-divergence` quant grader and the current consensus that mean KLD + top-token agreement track quality better than PPL alone, plus the RLHF landscape (PPO/DPO/GRPO) whose KL penalty is the stable core. Re-verify grader flags and the reigning policy-optimization method before quoting specifics; the identity and its proofs do not expire.

</details>

---

> 原文：[`Phase 5 - Advanced Topics and Specialization/Track G - ML Systems Engineering/Logprobs, Perplexity and KL Divergence/Lecture-04.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%205%20-%20Advanced%20Topics%20and%20Specialization/Track%20G%20-%20ML%20Systems%20Engineering/Logprobs%2C%20Perplexity%20and%20KL%20Divergence/Lecture-04.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
