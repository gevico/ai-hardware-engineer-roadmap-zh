// jllm.h — Jetson LLM Runtime: master header
// Target: Orin Nano Super 8GB (SM 8.7, 102 GB/s LPDDR5, 67 TOPS GPU + 10 TOPS DLA)

#pragma once

#include "jllm_memory.h"
#include "jllm_jetson.h"
#include "jllm_engine.h"
#include "jllm_kernels.h"

#define JLLM_VERSION_MAJOR 0
#define JLLM_VERSION_MINOR 1
#define JLLM_VERSION_PATCH 0

// SM 8.7 constants (Orin Nano/NX/AGX)
// JLLM_WARP_SIZE and JLLM_SHARED_MEM_SM defined in jllm_kernels.h
// Detected at runtime — these are defaults for Orin Nano Super (16 SMs)
// Orin Nano 8GB (non-Super) has 8 SMs / 512 cores / 16 TCs
static constexpr int JLLM_SM_COUNT       = 16;   // override: cudaGetDeviceProperties
static constexpr int JLLM_CUDA_CORES     = 1024;  // SM_COUNT × 64
static constexpr int JLLM_TENSOR_CORES   = 32;    // SM_COUNT × 2
static constexpr int JLLM_MAX_THREADS_SM = 1536;
static constexpr int JLLM_MAX_WARPS_SM   = 48;
static constexpr int JLLM_REG_FILE_SM    = 256 * 1024;
static constexpr int JLLM_L2_CACHE       = 512 * 1024;

static constexpr float JLLM_DRAM_BW_GBS  = 102.0f;
static constexpr float JLLM_GPU_TOPS_INT8 = 67.0f;
static constexpr float JLLM_DLA_TOPS_INT8 = 10.0f;
static constexpr float JLLM_RIDGE_POINT   = JLLM_GPU_TOPS_INT8 / JLLM_DRAM_BW_GBS;
