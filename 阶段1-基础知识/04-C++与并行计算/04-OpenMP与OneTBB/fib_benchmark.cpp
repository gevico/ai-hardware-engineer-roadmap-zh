// fib_benchmark.cpp — serial / OpenMP tasks / oneTBB parallel_invoke
//
// Compile:
//   g++ -O2 -std=c++17 -fopenmp fib_benchmark.cpp -ltbb -o fib_benchmark
//
// Run:
//   ./fib_benchmark
//
// Dependencies (Ubuntu/Debian):
//   sudo apt install g++ libomp-dev libtbb-dev

#include <chrono>
#include <cstdio>
#include <omp.h>
#include "oneapi/tbb/parallel_invoke.h"

using Clock = std::chrono::high_resolution_clock;
using Ms    = std::chrono::duration<double, std::milli>;

// Below this depth, run sequentially.
// Avoids task-creation overhead on trivial work.
// CUTOFF=25 is optimal for fib(50) on most machines:
//   too low → too many tiny tasks, scheduler overhead dominates
//   too high → not enough tasks to keep threads busy
#ifndef CUTOFF_VAL
static constexpr int CUTOFF = 25;
#else
static constexpr int CUTOFF = CUTOFF_VAL;
#endif

// ── 1. Serial ─────────────────────────────────────────────────────────────────
long long fib_serial(int n) {
    if (n < 2) return n;
    return fib_serial(n - 1) + fib_serial(n - 2);
}

// ── 2. OpenMP tasks ───────────────────────────────────────────────────────────
// Each recursive call below CUTOFF falls back to fib_serial to avoid spawning
// thousands of tiny tasks with more overhead than compute.
long long fib_omp(int n) {
    if (n < CUTOFF) return fib_serial(n);
    long long x, y;
    #pragma omp task shared(x) firstprivate(n)
    x = fib_omp(n - 1);
    #pragma omp task shared(y) firstprivate(n)
    y = fib_omp(n - 2);
    #pragma omp taskwait
    return x + y;
}

// ── 3. oneTBB parallel_invoke ─────────────────────────────────────────────────
// parallel_invoke forks exactly two callables and joins when both finish.
// Work-stealing handles load imbalance between branches automatically.
long long fib_tbb(int n) {
    if (n < CUTOFF) return fib_serial(n);
    long long x, y;
    tbb::parallel_invoke(
        [&]{ x = fib_tbb(n - 1); },
        [&]{ y = fib_tbb(n - 2); }
    );
    return x + y;
}

// ── Timing helper ─────────────────────────────────────────────────────────────
template<typename Fn>
double time_ms(Fn&& fn) {
    auto t0 = Clock::now();
    fn();
    return Ms(Clock::now() - t0).count();
}

int main() {
    const int N = 50;   // fib(50) = 12,586,269,025 — fits in long long
                        // serial: ~30–60 s; parallel: ~5–10 s on 8 cores
    long long r_serial, r_omp, r_tbb;

    // Warm up: first call pays library init and branch-predictor training cost
    fib_serial(30);

    double t_serial = time_ms([&]{ r_serial = fib_serial(N); });

    double t_omp = time_ms([&]{
        #pragma omp parallel       // create thread team
        #pragma omp single         // one thread starts task creation; rest execute tasks
        r_omp = fib_omp(N);
    });

    double t_tbb = time_ms([&]{ r_tbb = fib_tbb(N); });

    const int threads = omp_get_max_threads();
    std::printf("fib(%d) = %lld   [threads available: %d, cutoff: %d]\n\n",
                N, r_serial, threads, CUTOFF);
    std::printf("%-12s %8.1f ms  (baseline)\n",              "serial",  t_serial);
    std::printf("%-12s %8.1f ms  (%.2fx speedup)\n", "openmp", t_omp, t_serial / t_omp);
    std::printf("%-12s %8.1f ms  (%.2fx speedup)\n", "onetbb", t_tbb, t_serial / t_tbb);

    if (r_serial != r_omp || r_serial != r_tbb) {
        std::fprintf(stderr, "ERROR: results differ! serial=%lld omp=%lld tbb=%lld\n",
                     r_serial, r_omp, r_tbb);
        return 1;
    }
    return 0;
}
