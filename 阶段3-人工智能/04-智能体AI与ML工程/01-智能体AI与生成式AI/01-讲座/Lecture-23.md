---
title: 第 23 讲 — 评估与可观测性
description: 第 23 讲 — 评估与可观测性
---

# 第 23 讲 — 评估与可观测性

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 22](Lecture-22.md) | **Next:** [Lecture 24](Lecture-24.md)

---

## 学习目标

学完本讲后，你将能够：

1. 使用 LLM-as-judge 模式为 LLM 输出打正确性与质量分。
2. 在 RAG 系统上计算 RAGAS 指标（faithfulness、answer relevancy、context precision、context recall）。
3. 用 LangSmith 对 LLM 调用做 trace，并理解该记录哪些内容。
4. 构建一个成本追踪装饰器，按 session 累计 token 开销。
5. 记录每次 LLM 调用的完整 input/output/token/延迟明细。
6. 从生产流量中构建评测数据集，并运行 A/B prompt 测试。
7. 把编码或 agent 模型放到公开的 **benchmark 阶梯** 上（HumanEval → SWE-bench → Terminal-Bench → MCPMark），批判性地解读分数（污染、harness、pass@1），并选出能跨过任务所在档位的最便宜模型。

---

## 1. LLM-as-Judge

**“LLM-as-judge”** 模式用一个能力较强的 LLM 去评估另一次 LLM 调用的输出。它 **比人工标注更便宜、更快**，且在评分标准设计良好时与人类判断的相关性很好。

```python
# pip install openai

import os
import json
from openai import OpenAI

client = OpenAI(api_key=os.environ.get("OPENAI_API_KEY", "sk-fake"))

JUDGE_PROMPT = """You are an expert evaluator. Score the answer on the following rubric.
Return a JSON object with keys: score (0-5), reasoning (one sentence).

Rubric:
  5 = Completely correct, well-cited, nothing to add.
  4 = Correct but missing minor details.
  3 = Partially correct with one factual error.
  2 = Mostly wrong or misleading.
  1 = Completely wrong.
  0 = Refused to answer or empty.

Question: {question}
Reference Answer: {reference}
Model Answer: {answer}
"""

def judge_answer(question: str, reference: str, answer: str) -> dict:
    prompt = JUDGE_PROMPT.format(
        question=question, reference=reference, answer=answer
    )
    response = client.chat.completions.create(
        model=os.environ.get("OPENAI_JUDGE_MODEL", "your-judge-model-id"),
        messages=[{"role": "user", "content": prompt}],
        response_format={"type": "json_object"},
        temperature=0,
    )
    return json.loads(response.choices[0].message.content)


# Example evaluation
test_cases = [
    {
        "question": "What is the memory bandwidth of the H100 SXM5?",
        "reference": "3.35 TB/s using HBM3 memory.",
        "answer": "The H100 SXM5 provides approximately 3.35 terabytes per second of memory bandwidth via HBM3.",
    },
    {
        "question": "What is the memory bandwidth of the H100 SXM5?",
        "reference": "3.35 TB/s using HBM3 memory.",
        "answer": "The H100 SXM5 has 2 TB/s memory bandwidth.",  # wrong
    },
]

for tc in test_cases:
    result = judge_answer(tc["question"], tc["reference"], tc["answer"])
    print(f"Score: {result['score']}/5 — {result['reasoning']}")
    print(f"Answer was: '{tc['answer'][:60]}...'")
    print()
```

### 1.1 批量评估

```python
from concurrent.futures import ThreadPoolExecutor, as_completed

def batch_evaluate(test_cases: list[dict], max_workers: int = 4) -> list[dict]:
    """Evaluate multiple test cases in parallel."""
    results = []
    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        futures = {
            executor.submit(judge_answer, tc["question"], tc["reference"], tc["answer"]): tc
            for tc in test_cases
        }
        for future in as_completed(futures):
            tc = futures[future]
            result = future.result()
            results.append({**tc, **result})

    results.sort(key=lambda x: x["score"], reverse=True)
    avg = sum(r["score"] for r in results) / len(results)
    print(f"\nMean score: {avg:.2f}/5.0 over {len(results)} cases")
    return results
```

---


<details>
<summary>English original</summary>

**Lecture 23 — Evaluation & Observability**

**Course:** [AI Agent Development 2026](../Guide.md) | **Previous:** [Lecture 22](Lecture-22.md) | **Next:** [Lecture 24](Lecture-24.md)

---

**Learning Objectives**

By the end of this lecture you will be able to:

1. Score LLM outputs for correctness and quality using an LLM-as-judge pattern.
2. Compute RAGAS metrics (faithfulness, answer relevancy, context precision, context recall) on a RAG system.
3. Trace LLM calls with LangSmith and understand what to log.
4. Build a cost-tracking decorator that accumulates token spend per session.
5. Log every LLM call with full input/output/token/latency details.
6. Construct an evaluation dataset from production traffic and run A/B prompt tests.
7. Place a coding or agent model on the public **benchmark ladder** (HumanEval → SWE-bench → Terminal-Bench → MCPMark), read a score critically (contamination, harness, pass@1), and choose the cheapest model that clears your task's rung.

---

**1. LLM-as-Judge**

The **"LLM-as-judge"** pattern uses a capable LLM to evaluate the output of another LLM call. It is **cheaper and faster than human annotation** while correlating well with human judgment for well-designed rubrics.

```python
# pip install openai

import os
import json
from openai import OpenAI

client = OpenAI(api_key=os.environ.get("OPENAI_API_KEY", "sk-fake"))

JUDGE_PROMPT = """You are an expert evaluator. Score the answer on the following rubric.
Return a JSON object with keys: score (0-5), reasoning (one sentence).

Rubric:
  5 = Completely correct, well-cited, nothing to add.
  4 = Correct but missing minor details.
  3 = Partially correct with one factual error.
  2 = Mostly wrong or misleading.
  1 = Completely wrong.
  0 = Refused to answer or empty.

Question: {question}
Reference Answer: {reference}
Model Answer: {answer}
"""

def judge_answer(question: str, reference: str, answer: str) -> dict:
    prompt = JUDGE_PROMPT.format(
        question=question, reference=reference, answer=answer
    )
    response = client.chat.completions.create(
        model=os.environ.get("OPENAI_JUDGE_MODEL", "your-judge-model-id"),
        messages=[{"role": "user", "content": prompt}],
        response_format={"type": "json_object"},
        temperature=0,
    )
    return json.loads(response.choices[0].message.content)


# Example evaluation
test_cases = [
    {
        "question": "What is the memory bandwidth of the H100 SXM5?",
        "reference": "3.35 TB/s using HBM3 memory.",
        "answer": "The H100 SXM5 provides approximately 3.35 terabytes per second of memory bandwidth via HBM3.",
    },
    {
        "question": "What is the memory bandwidth of the H100 SXM5?",
        "reference": "3.35 TB/s using HBM3 memory.",
        "answer": "The H100 SXM5 has 2 TB/s memory bandwidth.",  # wrong
    },
]

for tc in test_cases:
    result = judge_answer(tc["question"], tc["reference"], tc["answer"])
    print(f"Score: {result['score']}/5 — {result['reasoning']}")
    print(f"Answer was: '{tc['answer'][:60]}...'")
    print()
```

**1.1 Batch Evaluation**

```python
from concurrent.futures import ThreadPoolExecutor, as_completed

def batch_evaluate(test_cases: list[dict], max_workers: int = 4) -> list[dict]:
    """Evaluate multiple test cases in parallel."""
    results = []
    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        futures = {
            executor.submit(judge_answer, tc["question"], tc["reference"], tc["answer"]): tc
            for tc in test_cases
        }
        for future in as_completed(futures):
            tc = futures[future]
            result = future.result()
            results.append({**tc, **result})

    results.sort(key=lambda x: x["score"], reverse=True)
    avg = sum(r["score"] for r in results) / len(results)
    print(f"\nMean score: {avg:.2f}/5.0 over {len(results)} cases")
    return results
```

---

</details>

## 2. RAGAS 指标

**RAGAS**（Retrieval Augmented Generation Assessment，检索增强生成评估）为 RAG 系统定义了**四个互补指标**。前两个指标的计算无需 ground-truth 答案。

```python
# pip install ragas langchain-openai datasets

import os
from datasets import Dataset
from ragas import evaluate
from ragas.metrics import (
    faithfulness,
    answer_relevancy,
    context_precision,
    context_recall,
)
from langchain_openai import ChatOpenAI, OpenAIEmbeddings

# Prepare evaluation data
# Each row: question, answer (from RAG), contexts (list of retrieved chunks), ground_truth
eval_data = {
    "question": [
        "What is the H100 memory bandwidth?",
        "What does HBM3 stand for?",
        "How does NVLink 4.0 compare to NVLink 3.0?",
    ],
    "answer": [
        "The H100 SXM5 delivers 3.35 TB/s of memory bandwidth using HBM3.",
        "HBM3 stands for High Bandwidth Memory generation 3.",
        "NVLink 4.0 provides 900 GB/s bidirectional bandwidth, double NVLink 3.0's 600 GB/s.",
    ],
    "contexts": [
        ["H100 SXM5 uses HBM3 providing 3.35 TB/s bandwidth.", "The H100 PCIe offers 2 TB/s."],
        ["HBM3 is the third generation of High Bandwidth Memory DRAM standard."],
        ["NVLink 4.0 delivers 900 GB/s total bidirectional bandwidth.", "NVLink 3.0 provided 600 GB/s."],
    ],
    "ground_truth": [
        "3.35 TB/s",
        "High Bandwidth Memory generation 3",
        "NVLink 4.0 doubles NVLink 3.0 bandwidth to 900 GB/s",
    ],
}

dataset = Dataset.from_dict(eval_data)

# Configure LLM and embeddings for RAGAS internal evaluation
llm = ChatOpenAI(model=os.environ.get("OPENAI_EVAL_MODEL", "your-eval-model-id"), temperature=0)
embeddings = OpenAIEmbeddings(model=os.environ.get("OPENAI_EMBEDDING_MODEL", "your-embedding-model-id"))

results = evaluate(
    dataset=dataset,
    metrics=[faithfulness, answer_relevancy, context_precision, context_recall],
    llm=llm,
    embeddings=embeddings,
)

print("\nRAGAS Evaluation Results:")
print(results.to_pandas().to_string(index=False))
```

### 2.1 理解每个指标

| 指标 | 度量内容 | 取值范围 | 公式 |
|---|---|---|---|
| **忠实度** | 答案是否在事实上以检索到的上下文为依据？ | 0–1 | 上下文中成立的断言数 / 断言总数 |
| **答案相关性** | 答案是否切中问题？ | 0–1 | 重新生成问题的 cosine 相似度 |
| **上下文精度** | 检索到的 chunk 是否相关（精度）？ | 0–1 | 相关 chunk 数 / 检索总数 |
| **上下文召回率** | 上下文是否覆盖所有 ground-truth 事实？ | 0–1 | 上下文中的事实数 / 事实总数 |

```python
# Manual faithfulness calculation (illustrative)
import re

def compute_faithfulness_manual(answer: str, context: str, llm) -> float:
    """Decompose answer into claims and check each against context."""

    # Step 1: extract claims from the answer
    claims_prompt = f"List every factual claim in this answer as a JSON array of strings:\n{answer}"
    claims_response = llm.invoke(claims_prompt).content
    try:
        claims = json.loads(claims_response)
    except Exception:
        claims = [answer]  # fallback

    if not claims:
        return 1.0

    # Step 2: verify each claim against context
    supported = 0
    for claim in claims:
        verify_prompt = (
            f"Context: {context}\n\n"
            f"Claim: {claim}\n\n"
            "Is this claim supported by the context? Reply only YES or NO."
        )
        verdict = llm.invoke(verify_prompt).content.strip().upper()
        if verdict.startswith("YES"):
            supported += 1

    return supported / len(claims)
```

---

## 3. 用 LangSmith 做链路追踪

**LangSmith** 记录每次 LangChain 调用，包含完整的**输入/输出、时序与 token 计数**。取不到 API key 时，可对追踪接口做 mock。

```python
# With a real LangSmith account:
# export LANGCHAIN_TRACING_V2=true
# export LANGCHAIN_API_KEY=ls__...
# export LANGCHAIN_PROJECT=my-rag-project
# All subsequent LangChain calls are automatically traced.

import os

def setup_tracing(project_name: str = "rag-evaluation"):
    """Configure LangSmith tracing if credentials are available."""
    api_key = os.environ.get("LANGCHAIN_API_KEY")
    if api_key:
        os.environ["LANGCHAIN_TRACING_V2"] = "true"
        os.environ["LANGCHAIN_PROJECT"] = project_name
        print(f"LangSmith tracing enabled → project: {project_name}")
    else:
        print("LANGCHAIN_API_KEY not set — tracing disabled (running locally)")

setup_tracing()
```


<details>
<summary>English original</summary>

**2. RAGAS Metrics**

**RAGAS** (Retrieval Augmented Generation Assessment) defines **four complementary metrics** for RAG systems. All are computed without needing ground-truth answers for the first two.

```python
# pip install ragas langchain-openai datasets

import os
from datasets import Dataset
from ragas import evaluate
from ragas.metrics import (
    faithfulness,
    answer_relevancy,
    context_precision,
    context_recall,
)
from langchain_openai import ChatOpenAI, OpenAIEmbeddings

# Prepare evaluation data
# Each row: question, answer (from RAG), contexts (list of retrieved chunks), ground_truth
eval_data = {
    "question": [
        "What is the H100 memory bandwidth?",
        "What does HBM3 stand for?",
        "How does NVLink 4.0 compare to NVLink 3.0?",
    ],
    "answer": [
        "The H100 SXM5 delivers 3.35 TB/s of memory bandwidth using HBM3.",
        "HBM3 stands for High Bandwidth Memory generation 3.",
        "NVLink 4.0 provides 900 GB/s bidirectional bandwidth, double NVLink 3.0's 600 GB/s.",
    ],
    "contexts": [
        ["H100 SXM5 uses HBM3 providing 3.35 TB/s bandwidth.", "The H100 PCIe offers 2 TB/s."],
        ["HBM3 is the third generation of High Bandwidth Memory DRAM standard."],
        ["NVLink 4.0 delivers 900 GB/s total bidirectional bandwidth.", "NVLink 3.0 provided 600 GB/s."],
    ],
    "ground_truth": [
        "3.35 TB/s",
        "High Bandwidth Memory generation 3",
        "NVLink 4.0 doubles NVLink 3.0 bandwidth to 900 GB/s",
    ],
}

dataset = Dataset.from_dict(eval_data)

# Configure LLM and embeddings for RAGAS internal evaluation
llm = ChatOpenAI(model=os.environ.get("OPENAI_EVAL_MODEL", "your-eval-model-id"), temperature=0)
embeddings = OpenAIEmbeddings(model=os.environ.get("OPENAI_EMBEDDING_MODEL", "your-embedding-model-id"))

results = evaluate(
    dataset=dataset,
    metrics=[faithfulness, answer_relevancy, context_precision, context_recall],
    llm=llm,
    embeddings=embeddings,
)

print("\nRAGAS Evaluation Results:")
print(results.to_pandas().to_string(index=False))
```

**2.1 Understanding Each Metric**

| Metric | Measures | Range | Formula |
|---|---|---|---|
| **Faithfulness** | Is the answer factually grounded in the retrieved context? | 0–1 | claims in context / total claims |
| **Answer Relevancy** | Does the answer address the question? | 0–1 | cosine sim of regenerated questions |
| **Context Precision** | Are retrieved chunks relevant (precision)? | 0–1 | relevant chunks / total retrieved |
| **Context Recall** | Are all ground-truth facts covered by context? | 0–1 | facts in context / total facts |

```python
# Manual faithfulness calculation (illustrative)
import re

def compute_faithfulness_manual(answer: str, context: str, llm) -> float:
    """Decompose answer into claims and check each against context."""

    # Step 1: extract claims from the answer
    claims_prompt = f"List every factual claim in this answer as a JSON array of strings:\n{answer}"
    claims_response = llm.invoke(claims_prompt).content
    try:
        claims = json.loads(claims_response)
    except Exception:
        claims = [answer]  # fallback

    if not claims:
        return 1.0

    # Step 2: verify each claim against context
    supported = 0
    for claim in claims:
        verify_prompt = (
            f"Context: {context}\n\n"
            f"Claim: {claim}\n\n"
            "Is this claim supported by the context? Reply only YES or NO."
        )
        verdict = llm.invoke(verify_prompt).content.strip().upper()
        if verdict.startswith("YES"):
            supported += 1

    return supported / len(claims)
```

---

**3. Tracing with LangSmith**

**LangSmith** records every LangChain invocation with full **input/output, timing, and token counts**. When no API key is available, we can mock the tracing interface.

```python
# With a real LangSmith account:
# export LANGCHAIN_TRACING_V2=true
# export LANGCHAIN_API_KEY=ls__...
# export LANGCHAIN_PROJECT=my-rag-project
# All subsequent LangChain calls are automatically traced.

import os

def setup_tracing(project_name: str = "rag-evaluation"):
    """Configure LangSmith tracing if credentials are available."""
    api_key = os.environ.get("LANGCHAIN_API_KEY")
    if api_key:
        os.environ["LANGCHAIN_TRACING_V2"] = "true"
        os.environ["LANGCHAIN_PROJECT"] = project_name
        print(f"LangSmith tracing enabled → project: {project_name}")
    else:
        print("LANGCHAIN_API_KEY not set — tracing disabled (running locally)")

setup_tracing()
```

</details>

### 3.1 手动运行日志记录（Mock）

当 LangSmith 不可用时，将运行记录写入**本地 JSONL 文件**：

```python
import json
import time
import uuid
from pathlib import Path
from functools import wraps
from typing import Any, Callable

TRACE_FILE = Path("./traces.jsonl")

def trace_llm_call(run_name: str):
    """Decorator that logs LLM calls to a local JSONL trace file."""
    def decorator(fn: Callable) -> Callable:
        @wraps(fn)
        def wrapper(*args, **kwargs) -> Any:
            run_id = str(uuid.uuid4())[:8]
            start = time.perf_counter()
            error = None
            result = None
            try:
                result = fn(*args, **kwargs)
                return result
            except Exception as e:
                error = str(e)
                raise
            finally:
                elapsed = time.perf_counter() - start
                record = {
                    "run_id": run_id,
                    "name": run_name,
                    "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                    "latency_s": round(elapsed, 4),
                    "inputs": {"args": str(args)[:200], "kwargs": str(kwargs)[:200]},
                    "output": str(result)[:500] if result is not None else None,
                    "error": error,
                }
                with TRACE_FILE.open("a") as f:
                    f.write(json.dumps(record) + "\n")
        return wrapper
    return decorator


@trace_llm_call("summarize")
def summarize(text: str) -> str:
    # Mocked — replace with real LLM call
    return f"Summary of: {text[:50]}..."

summarize("The H100 GPU achieves 3.35 TB/s memory bandwidth using HBM3...")
print(f"Trace written to {TRACE_FILE}")
```

---

## 4. 成本追踪

```python
import os
import time
import uuid
from dataclasses import dataclass, field
from openai import OpenAI

# Example pricing per million tokens.
# Do not use this table for billing decisions. Keep real prices in deployment
# config and update them from the current provider pricing page.
PRICING = {
    "fast-model": {"input": 0.15, "output": 0.60},
    "balanced-agent-model": {"input": 3.00, "output": 15.00},
    "reasoning-model": {"input": 15.00, "output": 75.00},
    "embedding-model": {"input": 0.02, "output": 0.0},
}

@dataclass
class CostTracker:
    session_id: str = field(default_factory=lambda: str(uuid.uuid4())[:8])
    total_input_tokens: int = 0
    total_output_tokens: int = 0
    total_cost_usd: float = 0.0
    calls: list[dict] = field(default_factory=list)

    def record(self, model: str, input_tokens: int, output_tokens: int, latency_s: float):
        pricing = PRICING.get(model, {"input": 0.0, "output": 0.0})
        cost = (input_tokens * pricing["input"] + output_tokens * pricing["output"]) / 1_000_000
        self.total_input_tokens += input_tokens
        self.total_output_tokens += output_tokens
        self.total_cost_usd += cost
        self.calls.append({
            "model": model,
            "input_tokens": input_tokens,
            "output_tokens": output_tokens,
            "cost_usd": round(cost, 6),
            "latency_s": round(latency_s, 4),
        })

    def summary(self) -> dict:
        return {
            "session_id": self.session_id,
            "total_calls": len(self.calls),
            "total_input_tokens": self.total_input_tokens,
            "total_output_tokens": self.total_output_tokens,
            "total_cost_usd": round(self.total_cost_usd, 6),
        }


# Global tracker for the session
tracker = CostTracker()

def tracked_completion(messages: list[dict], model: str | None = None) -> str:
    """OpenAI chat completion with automatic cost tracking."""
    client = OpenAI(api_key=os.environ.get("OPENAI_API_KEY", "sk-fake"))
    model = model or os.environ.get("OPENAI_MODEL", "your-model-id")
    start = time.perf_counter()
    response = client.chat.completions.create(
        model=model, messages=messages, temperature=0
    )
    elapsed = time.perf_counter() - start
    usage = response.usage
    tracker.record(model, usage.prompt_tokens, usage.completion_tokens, elapsed)
    return response.choices[0].message.content


# Demo
# answer = tracked_completion([{"role": "user", "content": "What is HBM3?"}])
# print(tracker.summary())
```

---

## 5. 结构化 LLM 调用日志记录器

```python
import json
import logging
import time
import uuid
from pathlib import Path
from openai import OpenAI

# Configure structured logging
logging.basicConfig(level=logging.INFO, format="%(message)s")
logger = logging.getLogger("llm_logger")
log_file = Path("llm_calls.jsonl")

class LLMLogger:
    """Logs every LLM call to both console and a JSONL file."""

    def __init__(self, model: str | None = None, log_path: Path = log_file):
        self.client = OpenAI(api_key=os.environ.get("OPENAI_API_KEY", "sk-fake"))
        self.model = model or os.environ.get("OPENAI_MODEL", "your-model-id")
        self.log_path = log_path

    def _write(self, record: dict):
        self.log_path.open("a").write(json.dumps(record) + "\n")
        # Brief console log
        logger.info(
            f"[LLM] call_id={record['call_id']} "
            f"model={record['model']} "
            f"tokens={record['usage']['total_tokens']} "
            f"latency={record['latency_s']:.3f}s "
            f"cost=${record['cost_usd']:.5f}"
        )

    def complete(self, messages: list[dict], **kwargs) -> str:
        call_id = uuid.uuid4().hex[:8]
        start = time.perf_counter()
        response = self.client.chat.completions.create(
            model=self.model, messages=messages, temperature=0, **kwargs
        )
        latency = time.perf_counter() - start
        usage = response.usage
        pricing = PRICING.get(self.model, {"input": 0.0, "output": 0.0})
        cost = (
            usage.prompt_tokens * pricing["input"]
            + usage.completion_tokens * pricing["output"]
        ) / 1_000_000

        record = {
            "call_id": call_id,
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "model": self.model,
            "messages": [{"role": m["role"], "content": m["content"][:300]} for m in messages],
            "response": response.choices[0].message.content[:500],
            "usage": {
                "prompt_tokens": usage.prompt_tokens,
                "completion_tokens": usage.completion_tokens,
                "total_tokens": usage.total_tokens,
            },
            "latency_s": round(latency, 4),
            "cost_usd": round(cost, 6),
        }
        self._write(record)
        return response.choices[0].message.content


# Usage
# llm = LLMLogger()
# answer = llm.complete([{"role": "user", "content": "Explain HBM3 in one sentence."}])
```

---

## 6. 从生产流量构建评估数据集

```python
import json
import random
from pathlib import Path
from datetime import datetime

TRACE_FILE = Path("./traces.jsonl")
EVAL_DATASET_FILE = Path("./eval_dataset.json")


def sample_production_traces(
    trace_file: Path,
    sample_size: int = 50,
    min_response_length: int = 50,
) -> list[dict]:
    """Sample high-quality traces to build a labeled eval dataset."""
    traces = []
    if not trace_file.exists():
        print(f"Trace file not found: {trace_file}")
        return []

    with trace_file.open() as f:
        for line in f:
            try:
                record = json.loads(line)
                if (
                    record.get("output")
                    and len(record["output"]) >= min_response_length
                    and not record.get("error")
                ):
                    traces.append(record)
            except json.JSONDecodeError:
                continue

    sample = random.sample(traces, min(sample_size, len(traces)))
    print(f"Sampled {len(sample)} traces from {len(traces)} total")
    return sample


def build_eval_dataset(traces: list[dict]) -> list[dict]:
    """
    Convert production traces into evaluation examples.
    In production: send these to human annotators for labeling.
    Here: auto-generate placeholder labels.
    """
    dataset = []
    for trace in traces:
        dataset.append({
            "id": trace.get("run_id", "unknown"),
            "question": _extract_question(trace),
            "answer": trace.get("output", ""),
            "label": None,       # to be filled by human annotator
            "score": None,       # to be filled by judge
            "sampled_at": datetime.utcnow().isoformat(),
        })
    EVAL_DATASET_FILE.write_text(json.dumps(dataset, indent=2))
    print(f"Saved {len(dataset)} eval examples → {EVAL_DATASET_FILE}")
    return dataset


def _extract_question(trace: dict) -> str:
    """Pull the user question from a trace record."""
    inputs = trace.get("inputs", {})
    if isinstance(inputs, dict):
        for key in ("question", "query", "input", "user_message"):
            if key in inputs:
                return inputs[key]
    return str(inputs)[:200]
```

---

## 7. A/B 测试 Prompt

```python
import hashlib
import random
from typing import Callable

PROMPT_A = """Answer the following question concisely using only facts from the context.
Context: {context}
Question: {question}"""

PROMPT_B = """You are a precise technical assistant. Using ONLY the provided context,
answer the question. If unsure, say "I don't know."
Context: {context}
Question: {question}"""


def ab_router(user_id: str, variants: list[str], weights: list[float] | None = None) -> str:
    """
    Deterministically assign a user to a prompt variant using their ID hash.
    Same user always gets the same variant (sticky assignment).
    """
    if weights:
        # Weighted random assignment (not deterministic, for gradual rollout)
        return random.choices(variants, weights=weights)[0]

    # Deterministic: hash user_id to choose variant
    hash_int = int(hashlib.md5(user_id.encode()).hexdigest(), 16)
    return variants[hash_int % len(variants)]


class PromptABTest:
    def __init__(self, variant_a: str, variant_b: str):
        self.variants = {"A": variant_a, "B": variant_b}
        self.results: dict[str, list[float]] = {"A": [], "B": []}

    def run(self, user_id: str, context: str, question: str, judge_fn: Callable) -> dict:
        variant_key = ab_router(user_id, ["A", "B"])
        prompt = self.variants[variant_key].format(context=context, question=question)

        # Generate answer (mock here)
        answer = f"[Variant {variant_key}] Answer about: {question[:40]}..."

        # Score the answer
        score = judge_fn(question=question, reference="ground truth placeholder", answer=answer)
        self.results[variant_key].append(score["score"])

        return {"variant": variant_key, "answer": answer, "score": score["score"]}

    def report(self) -> dict:
        report = {}
        for key, scores in self.results.items():
            if scores:
                report[key] = {
                    "n": len(scores),
                    "mean_score": round(sum(scores) / len(scores), 2),
                    "min": min(scores),
                    "max": max(scores),
                }
        winner = max(report, key=lambda k: report[k]["mean_score"]) if report else None
        return {"variants": report, "winner": winner}


# Demo
# ab = PromptABTest(PROMPT_A, PROMPT_B)
# for i in range(20):
#     ab.run(f"user_{i}", context="H100 has 3.35 TB/s bandwidth.", question="H100 bandwidth?", judge_fn=judge_answer)
# print(ab.report())
```

---

## 8. 标准化能力 Benchmark —— 编码与 Agent 阶梯

第 1–7 节回答的是*「**我**的 agent 好不好？」*——对一个你已构建好的系统做运行层面评估。本节回答的是构建*之前*的那个问题：*「底下的模型能力到底够不够——以及这个领域是怎么衡量的？」*当你选择驱动编码 agent 的模型，或判断前沿是否已推进到值得重新测试时，你读的是**公开能力 benchmark**。它们构成一道阶梯，从*写一个函数*一直到*借助工具像工程师一样操作一个代码仓库*。

> **Harness（agent 运行时框架）提醒（Lecture 02）：** benchmark 打分的对象是 **model + harness**，而非单独的模型。同一个模型在裸 prompt 下与在强 agent scaffold（规划、重试、测试执行、文件工具）下，得分差别很大。读数字时永远要连着 harness 一起读——leaderboard 上的跃升往往来自更好的循环，而不是更好的模型。


<details>
<summary>English original</summary>

**7. A/B Testing Prompts**

```python
import hashlib
import random
from typing import Callable

PROMPT_A = """Answer the following question concisely using only facts from the context.
Context: {context}
Question: {question}"""

PROMPT_B = """You are a precise technical assistant. Using ONLY the provided context,
answer the question. If unsure, say "I don't know."
Context: {context}
Question: {question}"""


def ab_router(user_id: str, variants: list[str], weights: list[float] | None = None) -> str:
    """
    Deterministically assign a user to a prompt variant using their ID hash.
    Same user always gets the same variant (sticky assignment).
    """
    if weights:
        # Weighted random assignment (not deterministic, for gradual rollout)
        return random.choices(variants, weights=weights)[0]

    # Deterministic: hash user_id to choose variant
    hash_int = int(hashlib.md5(user_id.encode()).hexdigest(), 16)
    return variants[hash_int % len(variants)]


class PromptABTest:
    def __init__(self, variant_a: str, variant_b: str):
        self.variants = {"A": variant_a, "B": variant_b}
        self.results: dict[str, list[float]] = {"A": [], "B": []}

    def run(self, user_id: str, context: str, question: str, judge_fn: Callable) -> dict:
        variant_key = ab_router(user_id, ["A", "B"])
        prompt = self.variants[variant_key].format(context=context, question=question)

        # Generate answer (mock here)
        answer = f"[Variant {variant_key}] Answer about: {question[:40]}..."

        # Score the answer
        score = judge_fn(question=question, reference="ground truth placeholder", answer=answer)
        self.results[variant_key].append(score["score"])

        return {"variant": variant_key, "answer": answer, "score": score["score"]}

    def report(self) -> dict:
        report = {}
        for key, scores in self.results.items():
            if scores:
                report[key] = {
                    "n": len(scores),
                    "mean_score": round(sum(scores) / len(scores), 2),
                    "min": min(scores),
                    "max": max(scores),
                }
        winner = max(report, key=lambda k: report[k]["mean_score"]) if report else None
        return {"variants": report, "winner": winner}


# Demo
# ab = PromptABTest(PROMPT_A, PROMPT_B)
# for i in range(20):
#     ab.run(f"user_{i}", context="H100 has 3.35 TB/s bandwidth.", question="H100 bandwidth?", judge_fn=judge_answer)
# print(ab.report())
```

---

**8. Standardized Capability Benchmarks — The Coding & Agent Ladder**

Sections 1–7 answer *"is **my** agent good?"* — operational evaluation of a system you already built. This section answers the question that comes *before* you build: *"is the model underneath even capable enough — and how does the field measure that?"* When you choose the model that powers a coding agent, or decide whether the frontier moved enough to re-test, you read **public capability benchmarks**. They form a ladder, from *write one function* up to *operate a repository like an engineer through tools*.

> **Harness reminder (Lecture 02):** a benchmark scores a **model + harness**, not a model alone. The same model scores very differently under a bare prompt versus a strong agent scaffold (planning, retries, test execution, file tools). Always read the harness next to the number — a leaderboard jump is often a better loop, not a better model.

</details>

### 8.1 阶梯

这是主干 —— 从孤立代码一路爬升到协议级工具使用。难度列与“数字含义”列按 **2026 年年中**校准；请把分数当作移动的锚点，而非常量。

| Benchmark | 测试内容 | 难度 | 规模 | 分数的实际含义（2026） |
|---|---|---|---|---|
| **[HumanEval](https://github.com/openai/human-eval)** | 写一个函数 | 简单 | 164 | **已饱和（~99%）** —— 是下限，不是区分度。改用 **HumanEval+**（EvalPlus）；其附加测试会暴露对原始用例过拟合的解。 |
| **[MBPP](https://github.com/google-research/google-research/tree/master/mbpp)** | 小型编码任务 | 简单–中等 | ~970 | 同样接近饱和。**MBPP+** 是更诚实的变体。只适合筛查弱模型/本地模型。 |
| **[LiveCodeBench](https://livecodebench.github.io/)** | 竞赛编程 | 中等–困难 | 滚动更新 | **无污染**：题目带时间戳，因此模型只在其训练截止时间*之后*发布的题目上计分。这是面对新模型时第一个可信的阶梯。 |
| **[SWE-bench](https://www.swebench.com/)** | 修复真实 GitHub bug | 困难 | 2,294 | 真实世界的战场。应引用 **SWE-bench Verified**（500 个经人工验证的任务）；前沿 harness（agent 运行时框架）在 2026 年达到约 **85–90%+**。agent 必须跨仓库定位 bug、编辑、并通过隐藏测试。 |
| **[Terminal-Bench](https://www.tbench.ai/)** | 像工程师一样使用终端 | 极难 | — | 在真实容器中完成端到端 shell 任务（构建、调试、配置）。v2 前沿约 **80%+**。考察规划 + 工具循环 + 恢复，而不只是代码合成。 |
| **[NL2Repo-Bench](https://arxiv.org/abs/2512.12730)** | 理解 / 生成整个仓库 | 极难 | 104 | 依据约 18.8k token 的规格说明构建完整可安装的库。**基本未解决** —— 最好的 agent 仍停在 **< 40%** 测试通过率。衡量长时程、多模块一致性。 |
| **[MCPMark](https://arxiv.org/abs/2509.24002)** | 通过 MCP 使用工具 | 面向 agent | 127 | 压力测试真实的 **MCP** 工具使用（跨 Notion / GitHub / Postgres / Filesystem / Playwright 的 CRUD）。每个任务平均 **16.2 turns / 17.4 次工具调用**；奖励失败恢复与长时间多工具执行。 |

把这条阶梯读作一条能力梯度：

```text
  isolated code  →  contamination-safe code  →  repo bug-fix  →  repo synthesis  →  terminal agency  →  protocol tool use
   HumanEval/MBPP     LiveCodeBench               SWE-bench         NL2Repo-Bench      Terminal-Bench       MCPMark
   (saturated)        (trustworthy)              (the fight)       (unsolved)         (very hard)          (agent-native)
```

2026 年的前沿模型**会打满底层阶梯**（函数级代码），但在**仓库合成与长时程自主性上仍低于约 50%**。这个差距 —— 介于“写对一个函数”与“通过工具在数十轮内操作一个仓库”之间 —— 正是 agent 工程（也就是整门课程）的价值所在。

### 8.2 其他值得了解的 benchmark

上面七个是主干；下面这些是会被引用的重要邻近项，按同样的阶梯分组：

```text
function-level   HumanEval+, MBPP+, BigCodeBench        realistic library/API calls; mostly saturated
competitive      LiveCodeBench Pro, CodeContests,       harder, expert-curated, contamination-aware
                 Aider Polyglot                          multi-language whole-file edits
repo bug-fix     SWE-bench Verified / Pro / Multimodal, human-validated, harder, multilingual,
                 Multi-SWE-bench, SWE-Lancer             and $-denominated (freelance value)
repo synthesis   NL2Repo-Bench, Commit0                  build whole libraries from a spec (long-horizon)
computer / OS    OSWorld, WebArena                       desktop and browser agency
tool & protocol  MCP-Bench, MCP-Atlas, τ-bench           multi-tool / MCP / domain workflows
                 (tau-bench)
ML engineering   MLE-bench, RE-Bench                     Kaggle-style and research-engineering agents
general agent    GAIA                                    multi-step assistant tasks with tools
```


<details>
<summary>English original</summary>

**8.1 The ladder**

This is the spine — the climb from isolated code to protocol-level tool use. Difficulty and the "what the number means" column are calibrated to **mid-2026**; treat the scores as moving anchors, not constants.

| Benchmark | Tests | Difficulty | Scale | What the score really means (2026) |
|---|---|---|---|---|
| **[HumanEval](https://github.com/openai/human-eval)** | Write a function | Easy | 164 | **Saturated (~99%)** — a floor, not a differentiator. Use **HumanEval+** (EvalPlus); its extra tests expose solutions that overfit the original cases. |
| **[MBPP](https://github.com/google-research/google-research/tree/master/mbpp)** | Small coding tasks | Easy–Medium | ~970 | Also near-saturated. **MBPP+** is the honest variant. Good only for screening weak/local models. |
| **[LiveCodeBench](https://livecodebench.github.io/)** | Competitive programming | Medium–Hard | rolling | **Contamination-free**: problems are time-stamped, so a model is scored only on problems published *after* its training cutoff. The first rung you can trust on a fresh model. |
| **[SWE-bench](https://www.swebench.com/)** | Fix real GitHub bugs | Hard | 2,294 | The real-world battleground. Cite **SWE-bench Verified** (500 human-validated tasks); frontier harnesses reach ~**85–90%+** in 2026. The agent must localize a bug across a repo, edit, and pass hidden tests. |
| **[Terminal-Bench](https://www.tbench.ai/)** | Use the terminal like an engineer | Very Hard | — | End-to-end shell tasks (build, debug, configure) in a real container. v2 frontier ~**80%+**. Tests planning + tool loops + recovery, not just code synthesis. |
| **[NL2Repo-Bench](https://arxiv.org/abs/2512.12730)** | Understand / generate an entire repository | Very Hard | 104 | Build a full installable library from an ~18.8k-token spec. **Largely unsolved** — best agents stay **< 40%** test-pass. Measures long-horizon, multi-module coherence. |
| **[MCPMark](https://arxiv.org/abs/2509.24002)** | Tool use through MCP | Agent-focused | 127 | Stress-tests realistic **MCP** tool use (CRUD across Notion / GitHub / Postgres / Filesystem / Playwright). Averages **16.2 turns / 17.4 tool calls** per task; rewards failure recovery and long multi-tool execution. |

Read the ladder as a capability gradient:

```text
  isolated code  →  contamination-safe code  →  repo bug-fix  →  repo synthesis  →  terminal agency  →  protocol tool use
   HumanEval/MBPP     LiveCodeBench               SWE-bench         NL2Repo-Bench      Terminal-Bench       MCPMark
   (saturated)        (trustworthy)              (the fight)       (unsolved)         (very hard)          (agent-native)
```

A 2026 frontier model **saturates the bottom rungs** (function-level code) and is **still under ~50% on repo synthesis and long-horizon agency**. That gap — between "writes a correct function" and "operates a repo through tools over dozens of turns" — is exactly where agent engineering (this whole course) earns its keep.

**8.2 Other benchmarks worth knowing**

The seven above are the spine; these are the important neighbors you will see cited, grouped by the same rungs:

```text
function-level   HumanEval+, MBPP+, BigCodeBench        realistic library/API calls; mostly saturated
competitive      LiveCodeBench Pro, CodeContests,       harder, expert-curated, contamination-aware
                 Aider Polyglot                          multi-language whole-file edits
repo bug-fix     SWE-bench Verified / Pro / Multimodal, human-validated, harder, multilingual,
                 Multi-SWE-bench, SWE-Lancer             and $-denominated (freelance value)
repo synthesis   NL2Repo-Bench, Commit0                  build whole libraries from a spec (long-horizon)
computer / OS    OSWorld, WebArena                       desktop and browser agency
tool & protocol  MCP-Bench, MCP-Atlas, τ-bench           multi-tool / MCP / domain workflows
                 (tau-bench)
ML engineering   MLE-bench, RE-Bench                     Kaggle-style and research-engineering agents
general agent    GAIA                                    multi-step assistant tasks with tools
```

</details>

### 8.3 如何读一个 benchmark 数字而不自欺

- **数据污染与饱和。** HumanEval 和 MBPP 多年前就泄漏进了训练语料；在那上面拿 99% 毫无意义。优先选**带时间戳**的集合（LiveCodeBench），或**人工标注、留出**的集合（SWE-bench Verified）。
- **harness（agent 运行时框架）的权重可能超过模型。** SWE-bench 和 Terminal-Bench 的数字是 *scaffold* 的结果。比较模型时要把 harness 固定住，否则你 benchmark 的是循环，不是智能。
- **看 pass@1，不看 pass@k。** pass@1（一次尝试）才是诚实的部署数字；pass@k 把方差藏在重试后面，只是好看。
- **测试时算力不是免费的。** 顶尖分数往往要花很多轮、几千个 token，有时还要并行 rollout。能力是真的，但它带着价签（§4）。
- **一个数字 ≠ 你的任务。** 在 SWE-bench（Python 修 bug）上登顶的模型，在你的 Rust、终端或 MCP 工作负载上可能平庸。让你实际要干的活对上相应的 rung。

### 8.4 哪个决策读哪个 benchmark

| 你的决策 | 读哪个 rung |
|---|---|
| "这个小的/本地的模型够不够好，能给函数做自动补全？" | HumanEval+ / MBPP+ / BigCodeBench |
| "它在没法记住的新题上还撑得住吗？" | LiveCodeBench |
| "它能作为 agent 修我真实仓库里的 bug 吗？" | SWE-bench Verified（难的长尾加 + Pro） |
| "它能搭起一个新服务，或者驱动 shell 吗？" | NL2Repo-Bench, Terminal-Bench |
| "它能在很多轮里可靠地驱动我的 MCP 工具吗？" | MCPMark, τ-bench |

> **硬件 / 推理视角。** 排行榜是被前沿模型用**每个任务很多轮、几千个 token** 打下来的——与便宜正好相反。对一个*已部署的* coding agent，你是在拿能力换 **tokens/$ 和延迟**（阶段 5 —— [MLSys Deep Dives](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/README.md)）。划算的一步是：部署**能跨过你任务所在那个 rung 的最便宜的模型**——一个在 BigCodeBench + LiveCodeBench 上过关的强开源模型，在自动补全上的 TOK/$ 能打赢前沿模型——把前沿能力留给真正需要它的 SWE-bench 级工作。梯子标出那条线；§4 里的成本追踪器给它定价。

> **内在指标 vs. 外在指标。** 这一节是模型度量的**外在**那一半——任务分数。**内在**那一半——困惑度、KL 散度，以及量化或蒸馏模型时用到的 logprob 级打分——在阶段 5 → [Logprobs, Perplexity & KL Divergence](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/04-Logprobs-困惑度与KL散度/README.md)。内在指标便宜且连续（任何文本上都能跑，几秒内抓到回归）；benchmark 分数昂贵且贴近真实任务。成熟的团队两者都用——用内在指标快速失败，用梯子确认能力。

**来源：** [SWE-bench](https://www.swebench.com/) · [LiveCodeBench](https://livecodebench.github.io/) · [Terminal-Bench](https://www.tbench.ai/) · [NL2Repo-Bench (arXiv 2512.12730)](https://arxiv.org/abs/2512.12730) · [MCPMark (arXiv 2509.24002, ICLR 2026)](https://arxiv.org/abs/2509.24002) · [EvalPlus / HumanEval+ & MBPP+](https://github.com/evalplus/evalplus)。这些分数是 2026 年中的锚点，每月都在变——引用前重新核对排行榜。

---

## 关键要点

- **LLM-as-judge** 把评估快速扩展到几千个样例。用分数为明确整数的评分标准（0-5），并要求 JSON 输出，以便可靠解析。
- **RAGAS** 给你四个正交的 RAG 指标。Faithfulness 抓幻觉；context recall 诊断检索缺口。
- **LangSmith** 链路追踪只需设好环境变量，零代码。即使不上生产，也要在 staging 里始终做追踪。
- **成本追踪**应按会话累加、按调用记录。尽早看到成本，预算才不会出意外。
- **结构化调用日志**（JSONL）是你的飞行记录仪——调试失败、构建评测数据集都离不开它。
- **A/B 测试**配 sticky 用户分桶，实验才可复现。宣布胜出之前，一定要跑到统计上有意义的样本量。
- **公开能力 benchmark**（coding/agent 的梯子——HumanEval → SWE-bench → Terminal-Bench → MCPMark）告诉你*底下那个模型*够不够好，但要批判地读：HumanEval/MBPP 已饱和，SWE-bench/Terminal-Bench 的分数取决于 harness，pass@1 才是诚实的数字，而头条分数通常来自昂贵的多轮运行。部署能跨过你任务那个 rung 的最便宜的模型。

---

## 练习


<details>
<summary>English original</summary>

**8.3 How to read a benchmark number without fooling yourself**

- **Contamination & saturation.** HumanEval and MBPP leaked into training corpora years ago; 99% there means nothing. Prefer **time-stamped** sets (LiveCodeBench) or **human-curated, held-out** ones (SWE-bench Verified).
- **Harness can outweigh the model.** SWE-bench and Terminal-Bench numbers are a *scaffold* result. Hold the harness fixed when you compare models, or you are benchmarking loops, not intelligence.
- **pass@1, not pass@k.** pass@1 (one attempt) is the honest deployment number; pass@k flatters by hiding variance behind retries.
- **Test-time compute is not free.** Top scores often spend many turns, thousands of tokens, and sometimes parallel rollouts. The capability is real, but it has a price tag (§4).
- **One number ≠ your task.** A model that tops SWE-bench (Python bug-fixing) can be mediocre on your Rust, terminal, or MCP workload. Match the rung to the job you actually have.

**8.4 Which benchmark for which decision**

| Your decision | Read this rung |
|---|---|
| "Is this small/local model good enough to autocomplete functions?" | HumanEval+ / MBPP+ / BigCodeBench |
| "Will it hold up on fresh problems it can't have memorized?" | LiveCodeBench |
| "Can it fix bugs in my actual repo, as an agent?" | SWE-bench Verified (+ Pro for the hard tail) |
| "Can it scaffold a new service or drive the shell?" | NL2Repo-Bench, Terminal-Bench |
| "Can it drive my MCP tools reliably over many turns?" | MCPMark, τ-bench |

> **Hardware / inference lens.** Leaderboards are won by frontier models running **many turns and thousands of tokens per task** — the opposite of cheap. For a *deployed* coding agent you are trading capability against **tokens/$ and latency** (Phase 5 — [MLSys Deep Dives](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/05-MLSys深度探索/README.md)). The move that pays: deploy the **cheapest model that clears the rung your task lives on** — a strong open model passing BigCodeBench + LiveCodeBench can beat a frontier model on TOK/$ for autocomplete — and reserve frontier capability for the SWE-bench-class work that genuinely needs it. The ladder locates that line; the cost tracker in §4 prices it.

> **Intrinsic vs. extrinsic.** This section is the **extrinsic** half of model measurement — task scores. The **intrinsic** half — perplexity, KL divergence, and logprob-level grading used when you quantize or distill a model — lives in Phase 5 → [Logprobs, Perplexity & KL Divergence](../../../../阶段5-高级专题与专精/07-方向G-ML系统工程/04-Logprobs-困惑度与KL散度/README.md). Intrinsic metrics are cheap and continuous (run on any text, catch regressions in seconds); benchmark scores are expensive and task-real. Mature teams use both — intrinsic to fail fast, the ladder to confirm capability.

**Sources:** [SWE-bench](https://www.swebench.com/) · [LiveCodeBench](https://livecodebench.github.io/) · [Terminal-Bench](https://www.tbench.ai/) · [NL2Repo-Bench (arXiv 2512.12730)](https://arxiv.org/abs/2512.12730) · [MCPMark (arXiv 2509.24002, ICLR 2026)](https://arxiv.org/abs/2509.24002) · [EvalPlus / HumanEval+ & MBPP+](https://github.com/evalplus/evalplus). Scores are mid-2026 anchors and move monthly — re-check the leaderboards before quoting.

---

**Key Takeaways**

- **LLM-as-judge** scales evaluation to thousands of examples quickly. Use a rubric with clear integer scores (0-5) and require JSON output for reliable parsing.
- **RAGAS** gives you four orthogonal RAG metrics. Faithfulness catches hallucinations; context recall diagnoses retrieval gaps.
- **LangSmith** tracing is zero-code with environment variables set. Always trace in staging even if not in production.
- **Cost tracking** should accumulate per-session and log per-call. Early visibility on costs prevents budget surprises.
- **Structured call logging** (JSONL) is your flight recorder — essential for debugging failures and building eval datasets.
- **A/B testing** with sticky user assignment ensures reproducible experiments. Always run for a statistically meaningful number of samples before declaring a winner.
- **Public capability benchmarks** (the coding/agent ladder — HumanEval → SWE-bench → Terminal-Bench → MCPMark) tell you whether the *model underneath* is good enough, but read them critically: HumanEval/MBPP are saturated, SWE-bench/Terminal-Bench scores depend on the harness, pass@1 is the honest number, and the headline usually comes from expensive many-turn runs. Deploy the cheapest model that clears your task's rung.

---

**Exercises**

</details>

### 练习 1 — 多准则评判器

将 LLM-as-judge 扩展到按三项独立准则评测：（1）事实准确率，（2）完整性，（3）简洁度。每个准则应返回 0–5 的分数，并附一句理由。构建一个 `MultiCriteriaJudge` 类，对三个分数取平均并返回综合判定。用来自你熟悉领域的至少 5 个问答对测试它。

### 练习 2 — 在你自己的数据上运行 RAGAS

自选任意技术主题，构建一个 5–10 篇文档的小型语料库。编写 5 个带已知标准答案的测试问题。构建一个简单的 RAG（检索增强生成）系统（来自 Lecture 11/10），对全部 5 个问题运行它，并用 RAGAS 评测。在表格中报告全部四项指标的分数。对任何低于 0.7 的指标，指出一项能改进该 RAG 系统的具体改动。

### 练习 3 — 成本看板

构建一个 `CostDashboard` 类，从 `LLMLogger` 生成的 JSONL 日志文件中读取数据，并渲染出汇总报告。报告应包含：总支出、按模型划分的支出、每次调用的平均成本、最贵的 5 次调用（含其输入），以及按小时分组的调用次数时间序列。报告应可打印为格式化的文本表格。

### 练习 4 — 在阶梯上定位你的模型

挑选一个你能调用的模型（开源或托管）。从公开排行榜上找到它最新的 **LiveCodeBench** 和 **SWE-bench Verified** 分数，并记下每个结果所用的 *harness*（agent 运行时框架）。然后写一段话：这个模型能轻松越过阶梯（§8.1）的哪一级，会在哪一级失败；对于一个代码补全产品，你会部署它还是更便宜的模型？用分数 **以及** tokens/$ 的论据来论证。

---

*下一篇：[Lecture 24](Lecture-24.md)*


<details>
<summary>English original</summary>

**Exercise 1 — Multi-Criteria Judge**

Extend the LLM-as-judge to evaluate on three separate criteria: (1) factual accuracy, (2) completeness, and (3) conciseness. Each criterion should return a score of 0–5 with a one-sentence justification. Build a `MultiCriteriaJudge` class that averages the three scores and returns a combined verdict. Test it on at least 5 question-answer pairs from a domain you know well.

**Exercise 2 — RAGAS on Your Own Data**

Create a small corpus of 5–10 documents on any technical topic you choose. Write 5 test questions with known ground-truth answers. Build a simple RAG system (from Lecture 11/10), run it on all 5 questions, and evaluate using RAGAS. Report all four metric scores in a table. For any metric below 0.7, identify one concrete change to the RAG system that would improve it.

**Exercise 3 — Cost Dashboard**

Build a `CostDashboard` class that reads from the JSONL log file produced by `LLMLogger` and renders a summary report. The report should include: total spend, spend by model, average cost per call, top 5 most expensive calls (with their inputs), and a call count time series grouped by hour. The report should be printable as a formatted text table.

**Exercise 4 — Locate Your Model on the Ladder**

Pick one model you can call (open or hosted). Find its most recent **LiveCodeBench** and **SWE-bench Verified** scores from the public leaderboards, noting the *harness* each result used. Then write one paragraph: which rung of the ladder (§8.1) does this model comfortably clear, which rung does it fail, and for a code-autocomplete product would you deploy it or a cheaper model? Justify with the score **and** a tokens/$ argument.

---

*Next: [Lecture 24](Lecture-24.md)*

</details>

---

> 原文：[`Phase 3 - Artificial Intelligence/Track B - Agentic AI and ML Engineering/3. Agentic AI and GenAI/Lectures/Lecture-23.md`](https://github.com/ai-hpc/ai-hardware-engineer-roadmap/blob/main/Phase%203%20-%20Artificial%20Intelligence/Track%20B%20-%20Agentic%20AI%20and%20ML%20Engineering/3.%20Agentic%20AI%20and%20GenAI/Lectures/Lecture-23.md)（上游仓库 ai-hpc/ai-hardware-engineer-roadmap，MIT License）。本页为中文翻译，术语保留英文原词；与原文不一致时以原文为准。
