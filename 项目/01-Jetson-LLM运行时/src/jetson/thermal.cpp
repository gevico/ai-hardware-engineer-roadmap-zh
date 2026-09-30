// thermal.cpp — Thermal monitoring and adaptive throttling

#include "jllm_jetson.h"
#include <cstdio>
#include <cstring>
#include <dirent.h>

namespace jllm {

static float read_thermal_zone(const char* zone_type) {
    DIR* dir = opendir("/sys/devices/virtual/thermal/");
    if (!dir) return -1.0f;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strncmp(entry->d_name, "thermal_zone", 12) != 0) continue;

        char path[256];
        snprintf(path, sizeof(path), "/sys/devices/virtual/thermal/%s/type", entry->d_name);
        FILE* f = fopen(path, "r");
        if (!f) continue;

        char type[64] = {};
        fgets(type, sizeof(type), f);
        fclose(f);

        // Strip newline
        char* nl = strchr(type, '\n');
        if (nl) *nl = '\0';

        if (strcmp(type, zone_type) == 0) {
            snprintf(path, sizeof(path), "/sys/devices/virtual/thermal/%s/temp", entry->d_name);
            f = fopen(path, "r");
            if (f) {
                int temp = 0;
                fscanf(f, "%d", &temp);
                fclose(f);
                closedir(dir);
                return temp / 1000.0f;
            }
        }
    }
    closedir(dir);
    return -1.0f;
}

ThermalState read_thermal() {
    ThermalState ts = {};
    ts.cpu_temp_c   = read_thermal_zone("CPU-therm");
    ts.gpu_temp_c   = read_thermal_zone("GPU-therm");
    ts.board_temp_c = read_thermal_zone("Tboard_tegra");

    // Fallback: read first zone if named zones not found
    if (ts.cpu_temp_c < 0) {
        FILE* f = fopen("/sys/devices/virtual/thermal/thermal_zone0/temp", "r");
        if (f) { int t; fscanf(f, "%d", &t); ts.cpu_temp_c = t / 1000.0f; fclose(f); }
    }
    if (ts.gpu_temp_c < 0) ts.gpu_temp_c = ts.cpu_temp_c;  // shared die

    // Throttling detection
    ts.throttling = (ts.gpu_temp_c > 85.0f || ts.cpu_temp_c > 85.0f);

    return ts;
}

int thermal_backoff_us(const ThermalState& ts) {
    float max_temp = ts.gpu_temp_c > ts.cpu_temp_c ? ts.gpu_temp_c : ts.cpu_temp_c;

    if (max_temp > 95.0f) return 200000;  // 200ms — emergency
    if (max_temp > 90.0f) return 100000;  // 100ms — critical
    if (max_temp > 85.0f) return  50000;  //  50ms — throttle zone
    if (max_temp > 80.0f) return  10000;  //  10ms — pre-throttle
    return 0;                              // full speed
}

}  // namespace jllm
