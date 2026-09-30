#!/bin/bash
# setup_jetson.sh — First-time setup for jetson-llm runtime
# Run on Jetson Orin Nano Super

set -e

echo "═══════════════════════════════════════════"
echo "  jetson-llm first-time setup"
echo "═══════════════════════════════════════════"

# Check we're on Jetson
if [ ! -f /etc/nv_tegra_release ]; then
    echo "ERROR: This script must run on NVIDIA Jetson."
    exit 1
fi

echo ""
echo "▸ System info:"
cat /etc/nv_tegra_release
echo "  RAM: $(free -m | awk '/Mem:/ {print $2}') MB total, $(free -m | awk '/Mem:/ {print $7}') MB available"
echo ""

# 1. Set max performance
echo "▸ Setting MAXN power mode (25W)..."
sudo nvpmodel -m 0 2>/dev/null || echo "  (nvpmodel not available)"
sudo jetson_clocks 2>/dev/null || echo "  (jetson_clocks not available)"

# 2. Disable GUI if running (frees ~500 MB)
if systemctl is-active --quiet gdm3 || systemctl is-active --quiet lightdm; then
    echo "▸ GUI detected. Disable to free ~500 MB RAM? (y/n)"
    read -r ans
    if [ "$ans" = "y" ]; then
        sudo systemctl set-default multi-user.target
        echo "  GUI will be disabled after reboot."
        echo "  Re-enable with: sudo systemctl set-default graphical.target"
    fi
fi

# 3. Reduce CMA for LLM workload (no camera needed)
echo "▸ Checking CMA allocation..."
CMA=$(grep CmaTotal /proc/meminfo | awk '{print $2}')
CMA_MB=$((CMA / 1024))
echo "  Current CMA: ${CMA_MB} MB"
if [ "$CMA_MB" -gt 512 ]; then
    echo "  Consider reducing CMA to 256 MB for LLM workloads:"
    echo "  Edit /boot/extlinux/extlinux.conf, add: cma=256M"
fi

# 4. Install build dependencies
echo ""
echo "▸ Installing build dependencies..."
sudo apt-get update -qq
sudo apt-get install -y -qq cmake build-essential cuda-toolkit-* 2>/dev/null || true

# 5. Build
echo ""
echo "▸ Building jetson-llm..."
cd "$(dirname "$0")/.."
cmake -B build \
    -DGGML_CUDA=ON \
    -DCMAKE_CUDA_ARCHITECTURES="87" \
    -DCMAKE_BUILD_TYPE=Release \
    2>&1 | tail -5
cmake --build build -j$(nproc) 2>&1 | tail -10

if [ -f build/jetson-llm ]; then
    echo ""
    echo "✓ Build successful!"
    echo ""
    echo "▸ Next: download a model and run:"
    echo ""
    echo "  # Download model (on internet-connected machine, then scp)"
    echo "  wget https://huggingface.co/bartowski/Llama-3.2-3B-Instruct-GGUF/resolve/main/Llama-3.2-3B-Instruct-Q4_K_M.gguf"
    echo ""
    echo "  # Run interactive chat"
    echo "  ./build/jetson-llm -m Llama-3.2-3B-Instruct-Q4_K_M.gguf -i"
    echo ""
    echo "  # Run API server"
    echo "  ./build/jetson-llm-server -m Llama-3.2-3B-Instruct-Q4_K_M.gguf -p 8080"
else
    echo "✗ Build failed. Check errors above."
    exit 1
fi
