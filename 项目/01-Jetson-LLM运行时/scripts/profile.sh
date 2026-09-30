#!/bin/bash
# profile.sh — Nsight Systems profiling for kernel optimization
# Usage: ./scripts/profile.sh model.gguf

MODEL=${1:-"model.gguf"}
OUTPUT="profile_$(date +%Y%m%d_%H%M%S)"

echo "Profiling jetson-llm with Nsight Systems..."
echo "Output: ${OUTPUT}.nsys-rep"

nsys profile \
    --trace=cuda,nvtx,osrt \
    --cuda-memory-usage=true \
    --output="$OUTPUT" \
    ./build/jetson-llm -m "$MODEL" -p "Hello, how are you?" -n 64

echo ""
echo "▸ Top kernels by time:"
nsys stats "${OUTPUT}.nsys-rep" 2>/dev/null | head -20

echo ""
echo "▸ View full trace:"
echo "  nsys-ui ${OUTPUT}.nsys-rep"
echo "  (or copy to workstation with GUI)"
