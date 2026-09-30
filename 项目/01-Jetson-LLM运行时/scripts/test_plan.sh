#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════
# jetson-llm Test Plan — Run on Jetson Orin Nano Super
#
# Usage:
#   ./scripts/test_plan.sh [model.gguf]
#
# If no model specified, downloads TinyLlama 1.1B Q4_K_M (~669 MB)
#
# This script runs ALL tests in order, logs results, and produces
# a pass/fail report. Stop at the first failure to debug.
# ═══════════════════════════════════════════════════════════════════════

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

PASS_COUNT=0
FAIL_COUNT=0
SKIP_COUNT=0
LOG_FILE="test_results_$(date +%Y%m%d_%H%M%S).log"

cd "$(dirname "$0")/.."

pass() { echo -e "${GREEN}PASS${NC}: $1"; PASS_COUNT=$((PASS_COUNT+1)); echo "PASS: $1" >> "$LOG_FILE"; }
fail() { echo -e "${RED}FAIL${NC}: $1"; FAIL_COUNT=$((FAIL_COUNT+1)); echo "FAIL: $1" >> "$LOG_FILE"; }
skip() { echo -e "${YELLOW}SKIP${NC}: $1"; SKIP_COUNT=$((SKIP_COUNT+1)); echo "SKIP: $1" >> "$LOG_FILE"; }
info() { echo -e "${YELLOW}INFO${NC}: $1"; echo "INFO: $1" >> "$LOG_FILE"; }

echo "═══════════════════════════════════════════════════════════════"
echo "  jetson-llm Test Plan"
echo "  Date: $(date)"
echo "  Log:  $LOG_FILE"
echo "═══════════════════════════════════════════════════════════════"
echo ""

# ═══════════════════════════════════════════════════════════════════════
# PHASE 0: System Validation
# ═══════════════════════════════════════════════════════════════════════
echo "══ PHASE 0: System Validation ═════════════════════════════════"

# T0.1: Are we on Jetson?
echo -n "T0.1 Running on Jetson: "
if [ -f /etc/nv_tegra_release ]; then
    pass "$(cat /etc/nv_tegra_release | head -1)"
else
    fail "Not a Jetson device"
    echo "This test plan must run on Jetson hardware. Exiting."
    exit 1
fi

# T0.2: CUDA available?
echo -n "T0.2 CUDA toolkit: "
if command -v nvcc &>/dev/null; then
    CUDA_VER=$(nvcc --version | grep release | awk '{print $5}' | tr -d ',')
    pass "CUDA $CUDA_VER"
else
    fail "nvcc not found — install cuda-toolkit"
    exit 1
fi

# T0.3: GPU accessible?
echo -n "T0.3 GPU device: "
GPU_NAME=$(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null || echo "")
if [ -n "$GPU_NAME" ]; then
    pass "$GPU_NAME"
else
    # Try tegra-specific check
    if [ -d /sys/devices/17000000.ga10b ]; then
        pass "Orin GPU (via sysfs)"
    else
        fail "No GPU found"
        exit 1
    fi
fi

# T0.4: RAM check
echo -n "T0.4 RAM available: "
RAM_FREE=$(free -m | awk '/Mem:/ {print $7}')
RAM_TOTAL=$(free -m | awk '/Mem:/ {print $2}')
if [ "$RAM_FREE" -gt 3000 ]; then
    pass "${RAM_FREE} MB free / ${RAM_TOTAL} MB total"
else
    fail "Only ${RAM_FREE} MB free — need >3000 MB. Disable GUI: sudo systemctl set-default multi-user.target"
fi

# T0.5: Power mode
echo -n "T0.5 Power mode: "
POWER=$(sudo nvpmodel -q 2>/dev/null | grep "Power Mode" | head -1 || echo "unknown")
info "$POWER"

# T0.6: Temperature
echo -n "T0.6 Thermal baseline: "
TEMP=$(cat /sys/devices/virtual/thermal/thermal_zone0/temp 2>/dev/null | awk '{printf "%.1f", $1/1000}')
if [ -n "$TEMP" ]; then
    pass "${TEMP}°C"
else
    skip "Cannot read thermal zone"
fi

echo ""

# ═══════════════════════════════════════════════════════════════════════
# PHASE 1: Build
# ═══════════════════════════════════════════════════════════════════════
echo "══ PHASE 1: Build ═════════════════════════════════════════════"

# T1.1: CMake configure
echo -n "T1.1 CMake configure: "
cmake -B build \
    -DGGML_CUDA=ON \
    -DCMAKE_CUDA_ARCHITECTURES="87" \
    -DCMAKE_BUILD_TYPE=Release \
    >> "$LOG_FILE" 2>&1
if [ $? -eq 0 ]; then
    pass "configured"
else
    fail "cmake configure failed — see $LOG_FILE"
    exit 1
fi

# T1.2: Build
echo -n "T1.2 Build: "
cmake --build build -j$(nproc) >> "$LOG_FILE" 2>&1
if [ $? -eq 0 ]; then
    pass "built"
else
    fail "build failed — see $LOG_FILE"
    exit 1
fi

# T1.3: Binaries exist
echo -n "T1.3 Binaries: "
ALL_BINS=true
for bin in jetson-llm jetson-llm-server test_memory test_kernels test_model_load; do
    if [ ! -f "build/$bin" ]; then
        fail "build/$bin not found"
        ALL_BINS=false
    fi
done
if $ALL_BINS; then
    pass "all 5 binaries built"
fi

echo ""

# ═══════════════════════════════════════════════════════════════════════
# PHASE 2: Unit Tests (no model needed)
# ═══════════════════════════════════════════════════════════════════════
echo "══ PHASE 2: Unit Tests (no model) ════════════════════════════"

# T2.1: Memory subsystem
echo -n "T2.1 Memory tests: "
OUTPUT=$(./build/test_memory 2>&1)
if echo "$OUTPUT" | grep -q "All memory tests passed"; then
    pass "3/3 tests passed"
else
    fail "memory tests failed"
    echo "$OUTPUT" >> "$LOG_FILE"
fi

# T2.2: CUDA kernel tests
echo -n "T2.2 Kernel tests: "
OUTPUT=$(./build/test_kernels 2>&1)
KERNEL_PASS=$(echo "$OUTPUT" | grep -c "PASS")
if [ "$KERNEL_PASS" -eq 5 ]; then
    pass "5/5 kernels correct"
else
    fail "only $KERNEL_PASS/5 kernel tests passed"
    echo "$OUTPUT" >> "$LOG_FILE"
fi

# T2.3: Memory budget reads real values
echo -n "T2.3 Budget reads /proc/meminfo: "
BUDGET_TOTAL=$(./build/test_memory 2>&1 | grep "Total DRAM" | grep -oP '\d+')
if [ -n "$BUDGET_TOTAL" ] && [ "$BUDGET_TOTAL" -gt 4000 ]; then
    pass "detected ${BUDGET_TOTAL} MB"
else
    fail "budget total=$BUDGET_TOTAL (expected >4000)"
fi

echo ""

# ═══════════════════════════════════════════════════════════════════════
# PHASE 3: Model Acquisition
# ═══════════════════════════════════════════════════════════════════════
echo "══ PHASE 3: Model Acquisition ════════════════════════════════"

MODEL="${1:-}"
MODEL_DIR="models"
mkdir -p "$MODEL_DIR"

if [ -z "$MODEL" ]; then
    MODEL="$MODEL_DIR/tinyllama-1.1b-q4_k_m.gguf"
    if [ ! -f "$MODEL" ]; then
        echo -n "T3.1 Download TinyLlama 1.1B: "
        wget -q --show-progress -O "$MODEL" \
            "https://huggingface.co/TheBloke/TinyLlama-1.1B-Chat-v1.0-GGUF/resolve/main/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf" \
            2>&1 | tail -1
        if [ -f "$MODEL" ] && [ -s "$MODEL" ]; then
            pass "downloaded $(du -h "$MODEL" | awk '{print $1}')"
        else
            fail "download failed"
            echo "Manual download: wget URL -O $MODEL"
            echo "Or copy from another machine: scp model.gguf user@jetson:$PWD/$MODEL"
            exit 1
        fi
    else
        pass "T3.1 Model already present: $(du -h "$MODEL" | awk '{print $1}')"
    fi
else
    if [ -f "$MODEL" ]; then
        pass "T3.1 Using provided model: $(du -h "$MODEL" | awk '{print $1}')"
    else
        fail "T3.1 Model not found: $MODEL"
        exit 1
    fi
fi

# T3.2: GGUF magic check
echo -n "T3.2 GGUF magic: "
MAGIC=$(xxd -l 4 -p "$MODEL")
if [ "$MAGIC" = "47475546" ]; then
    pass "valid GGUF"
else
    fail "not a valid GGUF file (magic=$MAGIC)"
    exit 1
fi

# T3.3: File size sanity
echo -n "T3.3 File size: "
SIZE_MB=$(du -m "$MODEL" | awk '{print $1}')
if [ "$SIZE_MB" -gt 100 ] && [ "$SIZE_MB" -lt 8000 ]; then
    pass "${SIZE_MB} MB (within 100 MB – 8 GB range)"
else
    fail "${SIZE_MB} MB (unexpected size)"
fi

echo ""

# ═══════════════════════════════════════════════════════════════════════
# PHASE 4: Model Loading Tests
# ═══════════════════════════════════════════════════════════════════════
echo "══ PHASE 4: Model Loading ═════════════════════════════════════"

# T4.1: Full model load test suite
echo "T4.1 Running test_model_load..."
OUTPUT=$(./build/test_model_load "$MODEL" 2>&1)
echo "$OUTPUT" >> "$LOG_FILE"

# T4.2: Config parsed
echo -n "T4.2 Config parsed: "
N_LAYERS=$(echo "$OUTPUT" | grep "n_layers" | grep -oP '\d+' | head -1)
VOCAB=$(echo "$OUTPUT" | grep "vocab_size" | grep -oP '\d+' | head -1)
if [ -n "$N_LAYERS" ] && [ "$N_LAYERS" -gt 0 ] && [ -n "$VOCAB" ] && [ "$VOCAB" -gt 0 ]; then
    pass "layers=$N_LAYERS, vocab=$VOCAB"
else
    fail "config parsing failed (layers=$N_LAYERS, vocab=$VOCAB)"
fi

# T4.3: Tokenizer loaded
echo -n "T4.3 Tokenizer: "
if echo "$OUTPUT" | grep -q "Loaded: yes"; then
    TOK_SIZE=$(echo "$OUTPUT" | grep "Vocab size" | grep -oP '\d+' | head -1)
    pass "vocab=$TOK_SIZE"
else
    fail "tokenizer failed to load"
fi

# T4.4: Weights mapped
echo -n "T4.4 Weight mapping: "
MAPPED=$(echo "$OUTPUT" | grep "Layers with QKV" | grep -oP '\d+ / \d+')
if echo "$OUTPUT" | grep "tok_embd" | grep -qv "(nil)"; then
    pass "tok_embd mapped, layers: $MAPPED"
else
    fail "weight mapping failed"
fi

# T4.5: Memory budget after load
echo -n "T4.5 Memory after load: "
FREE_AFTER=$(free -m | awk '/Mem:/ {print $7}')
if [ "$FREE_AFTER" -gt 1000 ]; then
    pass "${FREE_AFTER} MB free after model load"
else
    fail "only ${FREE_AFTER} MB free — model too large or memory leak"
fi

# T4.6: All 8 test_model_load tests pass
echo -n "T4.6 All load tests: "
TEST_PASS=$(echo "$OUTPUT" | grep -c "^PASS")
if echo "$OUTPUT" | grep -q "All model loading tests passed"; then
    pass "8/8 passed"
else
    fail "$TEST_PASS/8 passed"
fi

echo ""

# ═══════════════════════════════════════════════════════════════════════
# PHASE 5: Inference Tests
# ═══════════════════════════════════════════════════════════════════════
echo "══ PHASE 5: Inference ═════════════════════════════════════════"

# T5.1: Short generation — does it produce output?
echo -n "T5.1 Short generation (32 tokens): "
OUTPUT=$(timeout 60 ./build/jetson-llm -m "$MODEL" -p "What is 2+2?" -n 32 2>&1)
EXIT_CODE=$?
echo "$OUTPUT" >> "$LOG_FILE"

if [ $EXIT_CODE -eq 0 ] && [ ${#OUTPUT} -gt 20 ]; then
    # Check for garbage (all same character, or non-ASCII)
    UNIQUE_CHARS=$(echo "$OUTPUT" | head -1 | fold -w1 | sort -u | wc -l)
    if [ "$UNIQUE_CHARS" -gt 5 ]; then
        pass "generated ${#OUTPUT} chars, $UNIQUE_CHARS unique chars"
    else
        fail "output looks like garbage (only $UNIQUE_CHARS unique chars)"
    fi
else
    fail "generation failed (exit=$EXIT_CODE, output_len=${#OUTPUT})"
fi

# T5.2: Check tok/s from stderr
echo -n "T5.2 Performance: "
TOK_S=$(echo "$OUTPUT" | grep -oP '[\d.]+ tok/s' | head -1)
if [ -n "$TOK_S" ]; then
    pass "$TOK_S"
else
    skip "could not parse tok/s from output"
fi

# T5.3: No OOM warnings
echo -n "T5.3 No OOM warnings: "
if echo "$OUTPUT" | grep -qi "oom\|out of memory"; then
    fail "OOM detected during generation"
else
    pass "no OOM"
fi

# T5.4: No CUDA errors
echo -n "T5.4 No CUDA errors: "
if echo "$OUTPUT" | grep -qi "cuda error\|CUDA error"; then
    fail "CUDA error during generation"
else
    pass "no CUDA errors"
fi

# T5.5: Longer generation (128 tokens)
echo -n "T5.5 Long generation (128 tokens): "
OUTPUT128=$(timeout 120 ./build/jetson-llm -m "$MODEL" \
    -p "Explain how GPU memory works in simple terms." -n 128 2>&1)
if [ $? -eq 0 ]; then
    DECODE_TOK=$(echo "$OUTPUT128" | grep "Decode:" | grep -oP '\d+ tokens')
    pass "completed: $DECODE_TOK"
else
    fail "128-token generation failed or timed out"
fi

# T5.6: Memory stability — no growth during generation
echo -n "T5.6 Memory stability: "
FREE_BEFORE=$(free -m | awk '/Mem:/ {print $7}')
./build/jetson-llm -m "$MODEL" -p "Test" -n 64 >/dev/null 2>&1
FREE_AFTER=$(free -m | awk '/Mem:/ {print $7}')
DIFF=$((FREE_BEFORE - FREE_AFTER))
if [ "$DIFF" -lt 100 ] && [ "$DIFF" -gt -100 ]; then
    pass "stable (delta: ${DIFF} MB)"
else
    fail "memory changed by ${DIFF} MB during inference"
fi

echo ""

# ═══════════════════════════════════════════════════════════════════════
# PHASE 6: Server Tests
# ═══════════════════════════════════════════════════════════════════════
echo "══ PHASE 6: Server ════════════════════════════════════════════"

# Start server in background
./build/jetson-llm-server -m "$MODEL" -p 9999 &>/dev/null &
SERVER_PID=$!
sleep 3

# T6.1: Server started
echo -n "T6.1 Server started: "
if kill -0 $SERVER_PID 2>/dev/null; then
    pass "PID $SERVER_PID on port 9999"
else
    fail "server failed to start"
    SKIP_COUNT=$((SKIP_COUNT+3))
    echo ""
    echo "══ PHASE 7: skipped (server not running)"
    echo ""
fi

if kill -0 $SERVER_PID 2>/dev/null; then
    # T6.2: Health endpoint
    echo -n "T6.2 GET /health: "
    HEALTH=$(curl -s --max-time 5 http://localhost:9999/health)
    if echo "$HEALTH" | grep -q '"status":"ok"'; then
        pass "healthy"
    else
        fail "health check failed: $HEALTH"
    fi

    # T6.3: Models endpoint
    echo -n "T6.3 GET /v1/models: "
    MODELS=$(curl -s --max-time 5 http://localhost:9999/v1/models)
    if echo "$MODELS" | grep -q '"object":"model"'; then
        pass "model listed"
    else
        fail "models endpoint failed"
    fi

    # T6.4: Chat completion
    echo -n "T6.4 POST /v1/chat/completions: "
    CHAT=$(curl -s --max-time 30 http://localhost:9999/v1/chat/completions \
        -H "Content-Type: application/json" \
        -d '{"messages":[{"role":"user","content":"Say hello"}],"max_tokens":16}')
    if echo "$CHAT" | grep -q '"finish_reason"'; then
        pass "got completion"
    else
        fail "chat completion failed: $CHAT"
    fi

    # Stop server
    kill $SERVER_PID 2>/dev/null
    wait $SERVER_PID 2>/dev/null
fi

echo ""

# ═══════════════════════════════════════════════════════════════════════
# PHASE 7: Thermal and Power Tests
# ═══════════════════════════════════════════════════════════════════════
echo "══ PHASE 7: Thermal & Power ═══════════════════════════════════"

# T7.1: Temperature after tests
echo -n "T7.1 Post-test temperature: "
TEMP_AFTER=$(cat /sys/devices/virtual/thermal/thermal_zone0/temp 2>/dev/null | awk '{printf "%.1f", $1/1000}')
if [ -n "$TEMP_AFTER" ]; then
    TEMP_INT=${TEMP_AFTER%.*}
    if [ "$TEMP_INT" -lt 80 ]; then
        pass "${TEMP_AFTER}°C (below 80°C threshold)"
    else
        fail "${TEMP_AFTER}°C (above 80°C — needs better cooling)"
    fi
else
    skip "cannot read temperature"
fi

# T7.2: No thermal throttling during tests
echo -n "T7.2 Throttle check: "
if dmesg 2>/dev/null | tail -50 | grep -qi "thermal\|throttl"; then
    fail "thermal throttling detected in dmesg"
else
    pass "no throttling detected"
fi

echo ""

# ═══════════════════════════════════════════════════════════════════════
# RESULTS
# ═══════════════════════════════════════════════════════════════════════
echo "═══════════════════════════════════════════════════════════════"
echo ""
echo -e "  ${GREEN}PASSED${NC}: $PASS_COUNT"
echo -e "  ${RED}FAILED${NC}: $FAIL_COUNT"
echo -e "  ${YELLOW}SKIPPED${NC}: $SKIP_COUNT"
echo ""

TOTAL=$((PASS_COUNT + FAIL_COUNT))
if [ $FAIL_COUNT -eq 0 ]; then
    echo -e "  ${GREEN}═══ ALL $TOTAL TESTS PASSED ═══${NC}"
    echo ""
    echo "  jetson-llm is ready for production testing."
    echo "  Next: ./scripts/bench.sh $MODEL"
else
    echo -e "  ${RED}═══ $FAIL_COUNT FAILURE(S) — FIX BEFORE DEPLOYING ═══${NC}"
    echo ""
    echo "  See $LOG_FILE for details."
fi

echo ""
echo "  Full log: $LOG_FILE"
echo "═══════════════════════════════════════════════════════════════"

exit $FAIL_COUNT
