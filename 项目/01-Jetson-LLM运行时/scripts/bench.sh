#!/bin/bash
# bench.sh — Benchmark jetson-llm against llama.cpp baseline
# Usage: ./scripts/bench.sh model.gguf

set -e

MODEL=${1:-"model.gguf"}
if [ ! -f "$MODEL" ]; then
    echo "Usage: $0 <model.gguf>"
    exit 1
fi

echo "═══════════════════════════════════════════════════"
echo "  jetson-llm benchmark"
echo "  Model: $MODEL"
echo "  Date:  $(date)"
echo "  Git:   $(git rev-parse --short HEAD 2>/dev/null || echo 'n/a')"
echo "═══════════════════════════════════════════════════"

# System state
echo ""
echo "▸ System state:"
echo "  Power: $(sudo nvpmodel -q 2>/dev/null | head -1 || echo 'unknown')"
echo "  GPU:   $(cat /sys/devices/17000000.ga10b/devfreq/17000000.ga10b/cur_freq 2>/dev/null | awk '{print $1/1000000 " MHz"}' || echo 'unknown')"
echo "  RAM:   $(free -m | awk '/Mem:/ {print $7}') MB free"
echo "  Temp:  $(cat /sys/devices/virtual/thermal/thermal_zone0/temp 2>/dev/null | awk '{print $1/1000 "°C"}' || echo 'unknown')"

# File size
SIZE=$(ls -lh "$MODEL" | awk '{print $5}')
echo "  Model: $SIZE"

echo ""
echo "▸ Running jetson-llm benchmark..."
echo "  Prompt: 'Explain quantum computing in simple terms.'"
echo ""

# Warmup run
./build/jetson-llm -m "$MODEL" -p "Hello" -n 1 > /dev/null 2>&1 || true

# Benchmark: short prompt, 128 token generation
echo "--- Short generation (128 tokens) ---"
time ./build/jetson-llm -m "$MODEL" \
    -p "Explain quantum computing in simple terms." \
    -n 128 2>&1 | grep -E '\[engine\]|tok/s|peak|oom'

echo ""

# Benchmark: longer generation (256 tokens)
echo "--- Long generation (256 tokens) ---"
time ./build/jetson-llm -m "$MODEL" \
    -p "Write a detailed technical tutorial about GPU memory optimization." \
    -n 256 2>&1 | grep -E '\[engine\]|tok/s|peak|oom'

echo ""

# Memory profile during inference
echo "--- Memory profile ---"
./build/jetson-llm -m "$MODEL" -p "Test" -n 64 &
PID=$!
sleep 1

for i in 1 2 3 4 5; do
    if kill -0 $PID 2>/dev/null; then
        MEM=$(cat /proc/$PID/status 2>/dev/null | grep VmRSS | awk '{print $2}')
        FREE=$(free -m | awk '/Mem:/ {print $7}')
        TEMP=$(cat /sys/devices/virtual/thermal/thermal_zone0/temp 2>/dev/null | awk '{print $1/1000}')
        echo "  t=${i}s: RSS=${MEM} KB, Free=${FREE} MB, Temp=${TEMP}°C"
    fi
    sleep 1
done
wait $PID 2>/dev/null

echo ""
echo "═══════════════════════════════════════════════════"
echo "  Benchmark complete"
echo "═══════════════════════════════════════════════════"
