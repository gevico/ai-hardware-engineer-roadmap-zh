cmake_minimum_required(VERSION 3.20)
project(jetson-llm LANGUAGES C CXX CUDA)

# ── Jetson ONLY — no x86, no discrete GPU, no Windows ────────────────────
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    message(FATAL_ERROR "This project targets Jetson (aarch64) ONLY.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_ARCHITECTURES 87)   # SM 8.7 = Orin Nano/NX/AGX (Ampere)
set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)

# Orin-specific CUDA flags
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -O3 --use_fast_math --ptxas-options=-v")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=armv8.2-a+fp16 -ffast-math -Wno-format-truncation -Wno-unused-result")
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} --diag-suppress=177")

find_package(CUDAToolkit REQUIRED)
find_package(Threads REQUIRED)

# Make CUDA headers visible to .cpp files (they include cuda_runtime.h)
include_directories(${CUDAToolkit_INCLUDE_DIRS})

# ── Sources ──────────────────────────────────────────────────────────────
set(MEMORY_SRC
    src/memory/budget.cpp
    src/memory/kv_cache.cpp
    src/memory/pool.cpp
)

set(JETSON_SRC
    src/jetson/power.cpp
    src/jetson/thermal.cpp
    src/jetson/sysinfo.cpp
)

set(KERNEL_SRC
    src/kernels/gemv_q4.cu
    src/kernels/fused_norm.cu
    src/kernels/attention.cu
    src/kernels/rope.cu
    src/kernels/softmax.cu
    src/kernels/convert.cu
)

set(ENGINE_SRC
    src/engine/model.cpp
    src/engine/decode.cu
    src/engine/sample.cpp
    src/engine/tokenizer.cpp
)

set(SERVER_SRC
    src/server/http_server.cpp
)

# ── Library ──────────────────────────────────────────────────────────────
add_library(jetson_llm_core STATIC
    ${MEMORY_SRC} ${JETSON_SRC} ${KERNEL_SRC} ${ENGINE_SRC}
)
target_include_directories(jetson_llm_core PUBLIC include)
target_link_libraries(jetson_llm_core PUBLIC
    CUDA::cudart CUDA::cublas Threads::Threads
)

# ── CLI binary ───────────────────────────────────────────────────────────
add_executable(jetson-llm src/main.cpp)
target_link_libraries(jetson-llm PRIVATE jetson_llm_core)

# ── Server binary ────────────────────────────────────────────────────────
add_executable(jetson-llm-server src/main_server.cpp ${SERVER_SRC})
target_link_libraries(jetson-llm-server PRIVATE jetson_llm_core)

# ── Tests ────────────────────────────────────────────────────────────────
enable_testing()
add_executable(test_memory tests/test_memory.cpp)
target_link_libraries(test_memory PRIVATE jetson_llm_core)
add_test(NAME memory COMMAND test_memory)

add_executable(test_kernels tests/test_kernels.cu)
target_link_libraries(test_kernels PRIVATE jetson_llm_core)
add_test(NAME kernels COMMAND test_kernels)

add_executable(test_model_load tests/test_model_load.cpp)
target_link_libraries(test_model_load PRIVATE jetson_llm_core)
# Run with: ./build/test_model_load model.gguf
