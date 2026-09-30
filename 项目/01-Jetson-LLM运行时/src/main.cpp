// main.cpp — Jetson LLM Runtime CLI
// Usage: jetson-llm -m model.gguf [-c context] [-n max_tokens] [-p prompt]

#include "jllm.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <signal.h>
#include <unistd.h>

static jllm::Engine* g_engine = nullptr;

void signal_handler(int sig) {
    if (g_engine) g_engine->stop();
    fprintf(stderr, "\n[interrupted]\n");
}

struct Args {
    std::string model_path;
    std::string prompt;
    int         max_tokens  = 256;
    int         context     = 0;      // 0 = auto from memory budget
    float       temperature = 0.7f;
    int         top_k       = 40;
    float       top_p       = 0.9f;
    bool        interactive = false;
    bool        verbose     = false;
};

Args parse_args(int argc, char** argv) {
    Args args;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-m") == 0 && i+1 < argc) args.model_path = argv[++i];
        else if (strcmp(argv[i], "-p") == 0 && i+1 < argc) args.prompt = argv[++i];
        else if (strcmp(argv[i], "-n") == 0 && i+1 < argc) args.max_tokens = atoi(argv[++i]);
        else if (strcmp(argv[i], "-c") == 0 && i+1 < argc) args.context = atoi(argv[++i]);
        else if (strcmp(argv[i], "-t") == 0 && i+1 < argc) args.temperature = atof(argv[++i]);
        else if (strcmp(argv[i], "-i") == 0) args.interactive = true;
        else if (strcmp(argv[i], "-v") == 0) args.verbose = true;
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            fprintf(stderr,
                "jetson-llm — Memory-first LLM runtime for Jetson Orin\n\n"
                "Usage: jetson-llm -m model.gguf [options]\n\n"
                "Options:\n"
                "  -m PATH    GGUF model file (required)\n"
                "  -p TEXT    Prompt text\n"
                "  -n INT     Max tokens to generate (default: 256)\n"
                "  -c INT     Context length (0 = auto from memory budget)\n"
                "  -t FLOAT   Temperature (default: 0.7)\n"
                "  -i         Interactive mode (chat loop)\n"
                "  -v         Verbose (show memory/thermal stats)\n"
                "  -h         This help\n\n"
                "Jetson-specific:\n"
                "  Auto-detects power mode, thermal state, and available memory.\n"
                "  Context length is auto-calculated to fit memory budget.\n"
                "  OOM guard prevents crashes — stops generation gracefully.\n"
            );
            exit(0);
        }
    }
    return args;
}

int main(int argc, char** argv) {
    Args args = parse_args(argc, argv);

    if (args.model_path.empty()) {
        fprintf(stderr, "Error: model path required. Use -m model.gguf\n");
        return 1;
    }

    // ── System probe ──────────────────────────────────────────
    auto info = jllm::probe_jetson();
    jllm::print_jetson_info(info);

    auto power = jllm::read_power_state();
    fprintf(stderr, "Power: %dW mode, GPU @ %d MHz, %d CPUs online\n",
            power.watts, power.gpu_freq_mhz, power.cpu_online);

    auto budget = jllm::probe_system_memory();
    budget.print();

    // ── Check if model fits ───────────────────────────────────
    // Quick check: does the GGUF file size fit in available memory?
    FILE* f = fopen(args.model_path.c_str(), "rb");
    if (!f) {
        fprintf(stderr, "Error: cannot open %s\n", args.model_path.c_str());
        return 1;
    }
    fseek(f, 0, SEEK_END);
    int64_t file_size_mb = ftell(f) / (1024 * 1024);
    fclose(f);

    fprintf(stderr, "Model file: %ld MB\n", file_size_mb);
    if (!budget.can_allocate(file_size_mb + 200)) {  // +200 MB for KV + scratch
        fprintf(stderr, "ERROR: Model (%ld MB) + overhead won't fit in %ld MB free.\n",
                file_size_mb, budget.free_mb());
        fprintf(stderr, "Try: disable GUI (sudo systemctl set-default multi-user.target)\n");
        fprintf(stderr, "     or use a smaller model / more aggressive quantization.\n");
        return 1;
    }

    // ── Load model ────────────────────────────────────────────
    jllm::Engine engine;
    g_engine = &engine;
    signal(SIGINT, signal_handler);

    jllm::GenParams params;
    params.max_tokens = args.max_tokens;
    params.temperature = args.temperature;
    params.top_k = args.top_k;
    params.top_p = args.top_p;
    params.context_limit = args.context;

    fprintf(stderr, "Loading model...\n");
    if (!engine.load(args.model_path, params)) {
        fprintf(stderr, "Failed to load model.\n");
        return 1;
    }

    auto cfg = engine.config();
    fprintf(stderr, "Model: %s (%d layers, %d heads, %d KV heads, %d dim)\n",
            cfg.name.c_str(), cfg.n_layers, cfg.n_heads, cfg.n_kv_heads, cfg.hidden_dim);

    // ── Generate ──────────────────────────────────────────────
    if (args.interactive) {
        // Interactive chat loop
        fprintf(stderr, "\nEntering interactive mode. Type 'quit' to exit.\n\n");
        char line[4096];
        while (true) {
            fprintf(stderr, "> ");
            if (!fgets(line, sizeof(line), stdin)) break;

            // Strip newline
            char* nl = strchr(line, '\n');
            if (nl) *nl = '\0';
            if (strcmp(line, "quit") == 0 || strcmp(line, "exit") == 0) break;
            if (strlen(line) == 0) continue;

            auto stats = engine.generate(line, params, [](const char* text, bool eos) {
                fputs(text, stdout);
                fflush(stdout);
            });

            fprintf(stderr, "\n[%d tokens, %.1f tok/s, peak %ld MB, %.1f°C]\n\n",
                    stats.completion_tokens, stats.decode_tok_per_sec,
                    stats.peak_memory_mb, stats.peak_thermal_c);
        }
    } else if (!args.prompt.empty()) {
        // Single prompt mode
        auto stats = engine.generate(args.prompt, params, [](const char* text, bool eos) {
            fputs(text, stdout);
            fflush(stdout);
        });

        fprintf(stderr, "\n\n--- Stats ---\n");
        fprintf(stderr, "Prompt:  %d tokens, %.1f tok/s (%.0f ms)\n",
                stats.prompt_tokens, stats.prompt_tok_per_sec, stats.prompt_ms);
        fprintf(stderr, "Decode:  %d tokens, %.1f tok/s (%.0f ms)\n",
                stats.completion_tokens, stats.decode_tok_per_sec, stats.decode_ms);
        fprintf(stderr, "Memory:  peak %ld MB\n", stats.peak_memory_mb);
        fprintf(stderr, "Thermal: peak %.1f°C\n", stats.peak_thermal_c);
        if (stats.oom_stops > 0)
            fprintf(stderr, "WARNING: OOM guard stopped generation %d time(s)\n", stats.oom_stops);
    } else {
        fprintf(stderr, "No prompt provided. Use -p 'your prompt' or -i for interactive.\n");
    }

    engine.unload();
    return 0;
}
