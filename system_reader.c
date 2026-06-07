#define _GNU_SOURCE
#include "system_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <sys/statvfs.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <math.h>

/* Global config for demo/simulation mode */
static bool g_demo_mode = false;
static double g_last_time = 0.0;

/* Static states for delta calculations */
typedef struct {
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
} CpuTicks;

static CpuTicks prev_cpu_total;
static CpuTicks prev_cpu_cores[MAX_CPU_CORES];
static double prev_cpu_time = 0.0;

typedef struct {
    char name[32];
    unsigned long long rx_bytes;
    unsigned long long tx_bytes;
    double timestamp;
} NetState;

static NetState prev_net_states[MAX_NET_INTERFACES];
static int prev_net_count = 0;

typedef struct {
    char name[32];
    unsigned long long read_sectors;
    unsigned long long write_sectors;
    double timestamp;
} DiskState;

static DiskState prev_disk_states[MAX_STORAGE_MOUNTS];
static int prev_disk_count = 0;

/* Helper to get monotonic time in seconds */
static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

/* Enable or disable demo mode */
void system_reader_set_demo_mode(bool enable) {
    g_demo_mode = enable;
}

bool system_reader_get_demo_mode(void) {
    return g_demo_mode;
}

void system_reader_init(void) {
    g_last_time = get_time_sec();
    prev_cpu_time = get_time_sec();
    memset(&prev_cpu_total, 0, sizeof(CpuTicks));
    memset(prev_cpu_cores, 0, sizeof(prev_cpu_cores));
    memset(prev_net_states, 0, sizeof(prev_net_states));
    memset(prev_disk_states, 0, sizeof(prev_disk_states));
    prev_net_count = 0;
    prev_disk_count = 0;
}

void system_reader_cleanup(void) {
    /* Nothing to clean up */
}

/* Helper to trim leading/trailing whitespace */
static char *trim(char *str) {
    char *end;
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    end[1] = '\0';
    return str;
}

/* --- Simulation / Demo Data Generator --- */
static void generate_simulation_data(SystemStats *stats, double elapsed) {
    static double sim_time = 0.0;
    sim_time += elapsed;

    /* CPU simulation */
    strcpy(stats->cpu.model, "VAXP Silicon-X1 Octa-Core");
    stats->cpu.core_count = 8;
    stats->cpu.frequency_ghz = 3.2 + 0.8 * sin(sim_time * 0.2) + ((rand() % 10) / 50.0);
    
    double total_cpu = 0.0;
    for (int i = 0; i < 8; i++) {
        double base = 15.0 + 10.0 * sin(sim_time * 0.1 + i);
        if (i % 3 == 0) base += 40.0 * (0.5 + 0.5 * sin(sim_time * 0.5));
        base += (rand() % 15);
        if (base < 0) base = 0;
        if (base > 100) base = 100;
        stats->cpu.core_usage[i] = base;
        total_cpu += base;
    }
    stats->cpu.total_usage = total_cpu / 8.0;
    stats->cpu.temperature_c = 42.0 + 18.0 * (stats->cpu.total_usage / 100.0) + sin(sim_time * 0.05) * 2.0;

    /* Memory simulation */
    stats->memory.total_ram_gb = 16.0;
    double target_ram_used = 6.4 + 2.1 * sin(sim_time * 0.08);
    stats->memory.used_ram_gb = target_ram_used + ((rand() % 100) / 1000.0);
    stats->memory.active_ram_gb = stats->memory.used_ram_gb * 0.75;
    stats->memory.cached_ram_gb = 4.2 + 0.5 * cos(sim_time * 0.05);
    stats->memory.buffers_ram_gb = 0.8;
    stats->memory.ram_percent = (stats->memory.used_ram_gb / stats->memory.total_ram_gb) * 100.0;
    
    stats->memory.total_swap_gb = 8.0;
    stats->memory.used_swap_gb = 1.2 + 0.1 * sin(sim_time * 0.02);
    stats->memory.swap_percent = (stats->memory.used_swap_gb / stats->memory.total_swap_gb) * 100.0;

    /* GPU simulation for dual GPU demo mode */
    stats->gpu.gpu_count = 2;
    
    /* GPU 0: Intel integrated */
    GpuDevice *g0 = &stats->gpu.gpus[0];
    g0->present = true;
    strcpy(g0->brand, "Intel");
    strcpy(g0->model, "Intel Iris Xe Graphics (Integrated)");
    g0->usage_percent = 15.0 + 8.0 * sin(sim_time * 0.15) + (rand() % 5);
    g0->total_vram_gb = 4.0;
    g0->used_vram_gb = 0.8 + 0.2 * sin(sim_time * 0.05);
    g0->vram_percent = (g0->used_vram_gb / g0->total_vram_gb) * 100.0;
    g0->temperature_c = 41.0 + g0->usage_percent * 0.1;
    g0->core_clock_mhz = 450.0 + g0->usage_percent * 2.0;

    /* GPU 1: NVIDIA discrete */
    GpuDevice *g1 = &stats->gpu.gpus[1];
    g1->present = true;
    strcpy(g1->brand, "NVIDIA");
    strcpy(g1->model, "NVIDIA GeForce RTX 4060 Laptop GPU");
    
    /* Simulate periodic heavy load spikes (e.g. gaming bursts) */
    double spike = sin(sim_time * 0.06);
    if (spike > 0.4) {
        g1->usage_percent = 65.0 + 20.0 * sin(sim_time * 0.5) + (rand() % 10);
        g1->used_vram_gb = 4.2 + 0.8 * sin(sim_time * 0.2);
    } else {
        g1->usage_percent = 0.0 + (rand() % 2); /* idle */
        g1->used_vram_gb = 0.4 + 0.1 * sin(sim_time * 0.02);
    }
    g1->total_vram_gb = 8.0;
    g1->vram_percent = (g1->used_vram_gb / g1->total_vram_gb) * 100.0;
    g1->temperature_c = 38.0 + g1->usage_percent * 0.3;
    g1->core_clock_mhz = 1350.0 + g1->usage_percent * 4.0;

    /* Network simulation */
    stats->network.interface_count = 2;
    
    strcpy(stats->network.interfaces[0].name, "wl01");
    stats->network.interfaces[0].is_up = true;
    strcpy(stats->network.interfaces[0].ip_address, "192.168.1.105");
    stats->network.interfaces[0].rx_speed_kbps = 450.0 + 400.0 * sin(sim_time * 0.3) + (rand() % 100);
    stats->network.interfaces[0].tx_speed_kbps = 45.0 + 30.0 * sin(sim_time * 0.3) + (rand() % 20);
    if (stats->network.interfaces[0].rx_speed_kbps < 0) stats->network.interfaces[0].rx_speed_kbps = 5.0;
    if (stats->network.interfaces[0].tx_speed_kbps < 0) stats->network.interfaces[0].tx_speed_kbps = 1.0;
    
    strcpy(stats->network.interfaces[1].name, "eth0");
    stats->network.interfaces[1].is_up = false;
    strcpy(stats->network.interfaces[1].ip_address, "N/A");
    stats->network.interfaces[1].rx_speed_kbps = 0.0;
    stats->network.interfaces[1].tx_speed_kbps = 0.0;

    stats->network.total_rx_speed_kbps = stats->network.interfaces[0].rx_speed_kbps;
    stats->network.total_tx_speed_kbps = stats->network.interfaces[0].tx_speed_kbps;

    /* Storage simulation */
    stats->storage.mount_count = 2;
    
    strcpy(stats->storage.mounts[0].mount_point, "/");
    strcpy(stats->storage.mounts[0].device, "/dev/nvme0n1p2");
    stats->storage.mounts[0].total_gb = 512.0;
    stats->storage.mounts[0].used_gb = 184.2;
    stats->storage.mounts[0].usage_percent = (stats->storage.mounts[0].used_gb / stats->storage.mounts[0].total_gb) * 100.0;

    strcpy(stats->storage.mounts[1].mount_point, "/home");
    strcpy(stats->storage.mounts[1].device, "/dev/nvme0n1p3");
    stats->storage.mounts[1].total_gb = 1024.0;
    stats->storage.mounts[1].used_gb = 412.5;
    stats->storage.mounts[1].usage_percent = (stats->storage.mounts[1].used_gb / stats->storage.mounts[1].total_gb) * 100.0;

    stats->storage.total_storage_gb = 1536.0;
    stats->storage.used_storage_gb = 596.7;
    stats->storage.storage_percent = (stats->storage.used_storage_gb / stats->storage.total_storage_gb) * 100.0;
    
    stats->storage.read_speed_mbps = (stats->cpu.total_usage > 40.0) ? (2.1 + 8.5 * sin(sim_time * 0.4)) : 0.02;
    stats->storage.write_speed_mbps = (stats->cpu.total_usage > 55.0) ? (0.8 + 4.2 * sin(sim_time * 0.6)) : 0.01;
    if (stats->storage.read_speed_mbps < 0) stats->storage.read_speed_mbps = 0.0;
    if (stats->storage.write_speed_mbps < 0) stats->storage.write_speed_mbps = 0.0;

    /* Battery simulation */
    stats->battery.present = true;
    double charge_rate = -0.01; /* discharge slowly */
    static double battery_charge = 85.0;
    battery_charge += charge_rate * elapsed;
    if (battery_charge < 5) battery_charge = 100.0; /* Reset */
    stats->battery.charge_percent = battery_charge;
    
    strcpy(stats->battery.status, "Discharging");
    stats->battery.health_percent = 96.5;
    stats->battery.voltage_v = 11.8 - 0.6 * (100.0 - stats->battery.charge_percent) / 100.0;
    stats->battery.energy_rate_w = 8.5 + 4.2 * (stats->cpu.total_usage / 100.0);
    stats->battery.temp_c = 28.0 + 6.0 * (stats->cpu.total_usage / 100.0);
    stats->battery.time_to_empty_min = (int)((stats->battery.charge_percent / 100.0 * 60.0 * 12.0) / (stats->battery.energy_rate_w / 8.5));
    stats->battery.time_to_full_min = -1;
}

/* --- Real Linux Parsing Code --- */

static void parse_cpu_model(char *model_out, int max_len) {
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f) {
        strncpy(model_out, "Unknown CPU", max_len);
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "model name", 10) == 0) {
            char *colon = strchr(line, ':');
            if (colon) {
                char *trimmed = trim(colon + 1);
                strncpy(model_out, trimmed, max_len - 1);
                model_out[max_len - 1] = '\0';
                fclose(f);
                return;
            }
        }
    }
    fclose(f);
    strncpy(model_out, "Linux Processor", max_len);
}

static void read_cpu_stats(CpuStats *cpu) {
    parse_cpu_model(cpu->model, sizeof(cpu->model));
    cpu->core_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu->core_count > MAX_CPU_CORES) cpu->core_count = MAX_CPU_CORES;

    FILE *f = fopen("/proc/stat", "r");
    if (!f) return;

    char line[1024];
    int core_idx = -1;
    double current_time = get_time_sec();
    
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "cpu", 3) != 0) continue;
        
        char name[32];
        CpuTicks ticks;
        int parsed = sscanf(line, "%s %llu %llu %llu %llu %llu %llu %llu %llu",
                            name, &ticks.user, &ticks.nice, &ticks.system,
                            &ticks.idle, &ticks.iowait, &ticks.irq, &ticks.softirq, &ticks.steal);
        if (parsed < 9) continue;

        if (strcmp(name, "cpu") == 0) {
            /* Overall CPU */
            unsigned long long prev_total = prev_cpu_total.user + prev_cpu_total.nice + prev_cpu_total.system +
                                            prev_cpu_total.idle + prev_cpu_total.iowait + prev_cpu_total.irq +
                                            prev_cpu_total.softirq + prev_cpu_total.steal;
            unsigned long long curr_total = ticks.user + ticks.nice + ticks.system +
                                            ticks.idle + ticks.iowait + ticks.irq +
                                            ticks.softirq + ticks.steal;
            unsigned long long prev_idle = prev_cpu_total.idle + prev_cpu_total.iowait;
            unsigned long long curr_idle = ticks.idle + ticks.iowait;

            unsigned long long total_diff = curr_total - prev_total;
            unsigned long long idle_diff = curr_idle - prev_idle;

            if (total_diff > 0) {
                cpu->total_usage = (1.0 - ((double)idle_diff / total_diff)) * 100.0;
            } else {
                cpu->total_usage = 0.0;
            }
            prev_cpu_total = ticks;
        } else {
            /* Individual cores */
            core_idx++;
            if (core_idx >= cpu->core_count) continue;

            unsigned long long prev_total = prev_cpu_cores[core_idx].user + prev_cpu_cores[core_idx].nice + prev_cpu_cores[core_idx].system +
                                            prev_cpu_cores[core_idx].idle + prev_cpu_cores[core_idx].iowait + prev_cpu_cores[core_idx].irq +
                                            prev_cpu_cores[core_idx].softirq + prev_cpu_cores[core_idx].steal;
            unsigned long long curr_total = ticks.user + ticks.nice + ticks.system +
                                            ticks.idle + ticks.iowait + ticks.irq +
                                            ticks.softirq + ticks.steal;
            unsigned long long prev_idle = prev_cpu_cores[core_idx].idle + prev_cpu_cores[core_idx].iowait;
            unsigned long long curr_idle = ticks.idle + ticks.iowait;

            unsigned long long total_diff = curr_total - prev_total;
            unsigned long long idle_diff = curr_idle - prev_idle;

            if (total_diff > 0) {
                cpu->core_usage[core_idx] = (1.0 - ((double)idle_diff / total_diff)) * 100.0;
            } else {
                cpu->core_usage[core_idx] = 0.0;
            }
            prev_cpu_cores[core_idx] = ticks;
        }
    }
    fclose(f);
    prev_cpu_time = current_time;

    /* Try parsing scaling frequency of core 0 */
    f = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", "r");
    if (f) {
        unsigned long freq_khz = 0;
        if (fscanf(f, "%lu", &freq_khz) == 1) {
            cpu->frequency_ghz = freq_khz / 1000000.0;
        }
        fclose(f);
    } else {
        /* Check /proc/cpuinfo for frequency */
        f = fopen("/proc/cpuinfo", "r");
        if (f) {
            char line_freq[256];
            while (fgets(line_freq, sizeof(line_freq), f)) {
                if (strncmp(line_freq, "cpu MHz", 7) == 0) {
                    char *colon = strchr(line_freq, ':');
                    if (colon) {
                        cpu->frequency_ghz = atof(colon + 1) / 1000.0;
                        break;
                    }
                }
            }
            fclose(f);
        }
    }

    /* Read temperature */
    cpu->temperature_c = 0.0;
    /* Look in sysfs for thermal zones (covering more indices) */
    for (int tz = 0; tz < 32; tz++) {
        char type_path[128];
        char temp_path[128];
        snprintf(type_path, sizeof(type_path), "/sys/class/thermal/thermal_zone%d/type", tz);
        snprintf(temp_path, sizeof(temp_path), "/sys/class/thermal/thermal_zone%d/temp", tz);
        
        FILE *tf = fopen(type_path, "r");
        if (!tf) continue;
        char tz_type[128] = "";
        if (fgets(tz_type, sizeof(tz_type), tf)) {
            if (strstr(tz_type, "x86_pkg_temp") || strstr(tz_type, "coretemp") || strstr(tz_type, "cpu") || tz == 0) {
                FILE *temp_f = fopen(temp_path, "r");
                if (temp_f) {
                    long raw_temp = 0;
                    if (fscanf(temp_f, "%ld", &raw_temp) == 1) {
                        cpu->temperature_c = raw_temp / 1000.0;
                    }
                    fclose(temp_f);
                    fclose(tf);
                    break;
                }
            }
        }
        fclose(tf);
    }
    
    /* Fallback: scan all hwmon devices for CPU coretemp/k10temp sensors */
    if (cpu->temperature_c <= 0.0) {
        DIR *dir = opendir("/sys/class/hwmon");
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strncmp(entry->d_name, "hwmon", 5) == 0) {
                    char name_path[512];
                    snprintf(name_path, sizeof(name_path), "/sys/class/hwmon/%s/name", entry->d_name);
                    FILE *nf = fopen(name_path, "r");
                    if (nf) {
                        char sname[128] = "";
                        if (fgets(sname, sizeof(sname), nf)) {
                            if (strstr(sname, "coretemp") || strstr(sname, "k10temp") || strstr(sname, "cpu_thermal")) {
                                char temp_path[512];
                                snprintf(temp_path, sizeof(temp_path), "/sys/class/hwmon/%s/temp1_input", entry->d_name);
                                FILE *temp_f = fopen(temp_path, "r");
                                if (!temp_f) {
                                    snprintf(temp_path, sizeof(temp_path), "/sys/class/hwmon/%s/temp2_input", entry->d_name);
                                    temp_f = fopen(temp_path, "r");
                                }
                                if (temp_f) {
                                    long raw_temp = 0;
                                    if (fscanf(temp_f, "%ld", &raw_temp) == 1) {
                                        cpu->temperature_c = raw_temp / 1000.0;
                                    }
                                    fclose(temp_f);
                                    fclose(nf);
                                    break;
                                }
                            }
                        }
                        fclose(nf);
                    }
                }
            }
            closedir(dir);
        }
    }
    
    /* Absolute safe default if all hardware calls fail */
    if (cpu->temperature_c <= 0.0) {
        cpu->temperature_c = 45.0;
    }
}

static void read_memory_stats(MemoryStats *mem) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return;

    double mem_total_kb = 0;
    double mem_free_kb = 0;
    double mem_avail_kb = 0;
    double cached_kb = 0;
    double buffers_kb = 0;
    double swap_total_kb = 0;
    double swap_free_kb = 0;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "MemTotal:", 9) == 0) {
            sscanf(line + 9, "%lf", &mem_total_kb);
        } else if (strncmp(line, "MemFree:", 8) == 0) {
            sscanf(line + 8, "%lf", &mem_free_kb);
        } else if (strncmp(line, "MemAvailable:", 13) == 0) {
            sscanf(line + 13, "%lf", &mem_avail_kb);
        } else if (strncmp(line, "Cached:", 7) == 0) {
            sscanf(line + 7, "%lf", &cached_kb);
        } else if (strncmp(line, "Buffers:", 8) == 0) {
            sscanf(line + 8, "%lf", &buffers_kb);
        } else if (strncmp(line, "SwapTotal:", 10) == 0) {
            sscanf(line + 10, "%lf", &swap_total_kb);
        } else if (strncmp(line, "SwapFree:", 9) == 0) {
            sscanf(line + 9, "%lf", &swap_free_kb);
        }
    }
    fclose(f);

    mem->total_ram_gb = mem_total_kb / 1024.0 / 1024.0;
    
    if (mem_avail_kb > 0) {
        mem->used_ram_gb = (mem_total_kb - mem_avail_kb) / 1024.0 / 1024.0;
    } else {
        mem->used_ram_gb = (mem_total_kb - mem_free_kb - cached_kb - buffers_kb) / 1024.0 / 1024.0;
    }
    
    mem->cached_ram_gb = cached_kb / 1024.0 / 1024.0;
    mem->buffers_ram_gb = buffers_kb / 1024.0 / 1024.0;
    mem->active_ram_gb = mem->used_ram_gb * 0.8; /* Approximated active */
    
    if (mem->total_ram_gb > 0) {
        mem->ram_percent = (mem->used_ram_gb / mem->total_ram_gb) * 100.0;
    } else {
        mem->ram_percent = 0.0;
    }

    mem->total_swap_gb = swap_total_kb / 1024.0 / 1024.0;
    mem->used_swap_gb = (swap_total_kb - swap_free_kb) / 1024.0 / 1024.0;
    
    if (mem->total_swap_gb > 0) {
        mem->swap_percent = (mem->used_swap_gb / mem->total_swap_gb) * 100.0;
    } else {
        mem->swap_percent = 0.0;
    }
}

static void read_gpu_stats(GpuStats *gpu) {
    int gpu_idx = 0;
    
    /* Clear the GPU struct */
    memset(gpu, 0, sizeof(GpuStats));

    /* Look in DRM sysfs for up to 4 potential cards */
    for (int card = 0; card < 4 && gpu_idx < MAX_GPUS; card++) {
        char card_path[128];
        snprintf(card_path, sizeof(card_path), "/sys/class/drm/card%d", card);
        
        struct stat st;
        if (stat(card_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
            continue;
        }

        /* Check the vendor ID to determine GPU brand */
        char vendor_path[256];
        snprintf(vendor_path, sizeof(vendor_path), "%s/device/vendor", card_path);
        FILE *fv = fopen(vendor_path, "r");
        if (!fv) continue;
        char vendor_id[32] = "";
        if (fscanf(fv, "%31s", vendor_id) != 1) {
            fclose(fv);
            continue;
        }
        fclose(fv);
        
        GpuDevice *g = &gpu->gpus[gpu_idx];
        g->present = true;

        if (strcmp(vendor_id, "0x1002") == 0) {
            /* AMD Radeon GPU */
            strcpy(g->brand, "AMD");
            snprintf(g->model, sizeof(g->model), "AMD Radeon Graphics (card%d)", card);
            
            char path_busy[256], path_vram_tot[256], path_vram_usd[256];
            snprintf(path_busy, sizeof(path_busy), "%s/device/gpu_busy_percent", card_path);
            snprintf(path_vram_tot, sizeof(path_vram_tot), "%s/device/mem_info_vram_total", card_path);
            snprintf(path_vram_usd, sizeof(path_vram_usd), "%s/device/mem_info_vram_used", card_path);

            FILE *f = fopen(path_busy, "r");
            if (f) {
                unsigned int busy = 0;
                if (fscanf(f, "%u", &busy) == 1) g->usage_percent = busy;
                fclose(f);
            }

            f = fopen(path_vram_tot, "r");
            if (f) {
                unsigned long long vtot = 0;
                if (fscanf(f, "%llu", &vtot) == 1) g->total_vram_gb = vtot / (1024.0 * 1024.0 * 1024.0);
                fclose(f);
            }
            
            f = fopen(path_vram_usd, "r");
            if (f) {
                unsigned long long vusd = 0;
                if (fscanf(f, "%llu", &vusd) == 1) g->used_vram_gb = vusd / (1024.0 * 1024.0 * 1024.0);
                fclose(f);
            }
            
            if (g->total_vram_gb > 0) {
                g->vram_percent = (g->used_vram_gb / g->total_vram_gb) * 100.0;
            }

            /* Temp */
            g->temperature_c = 40.0;
            char hwmon_dir[256];
            snprintf(hwmon_dir, sizeof(hwmon_dir), "%s/device/hwmon", card_path);
            DIR *hdir = opendir(hwmon_dir);
            if (hdir) {
                struct dirent *hentry;
                while ((hentry = readdir(hdir)) != NULL) {
                    if (strncmp(hentry->d_name, "hwmon", 5) == 0) {
                        char path_temp[1024];
                        snprintf(path_temp, sizeof(path_temp), "%s/%s/temp1_input", hwmon_dir, hentry->d_name);
                        FILE *tf = fopen(path_temp, "r");
                        if (tf) {
                            long t_raw = 0;
                            if (fscanf(tf, "%ld", &t_raw) == 1) {
                                g->temperature_c = t_raw / 1000.0;
                            }
                            fclose(tf);
                            break;
                        }
                    }
                }
                closedir(hdir);
            }
            g->core_clock_mhz = 1000.0;
            gpu_idx++;
        }
        else if (strcmp(vendor_id, "0x8086") == 0) {
            /* Intel Graphics (integrated) */
            strcpy(g->brand, "Intel");
            snprintf(g->model, sizeof(g->model), "Intel Iris Xe Graphics (card%d)", card);
            g->usage_percent = 5.0; /* Minimal default */
            g->total_vram_gb = 4.0; /* Simulated shared VRAM */
            g->used_vram_gb = 0.5;
            g->vram_percent = (g->used_vram_gb / g->total_vram_gb) * 100.0;
            g->temperature_c = 38.0;
            g->core_clock_mhz = 450.0;
            gpu_idx++;
        }
        else if (strcmp(vendor_id, "0x10de") == 0) {
            /* NVIDIA Graphics */
            strcpy(g->brand, "NVIDIA");
            snprintf(g->model, sizeof(g->model), "NVIDIA GeForce GPU (card%d)", card);
            
            g->usage_percent = 0.0;
            g->total_vram_gb = 8.0;
            g->used_vram_gb = 0.0;
            g->vram_percent = 0.0;
            g->temperature_c = 42.0;
            g->core_clock_mhz = 1200.0;
            
            /* Execute nvidia-smi command to get actual metrics */
            FILE *p = popen("nvidia-smi --query-gpu=utilization.gpu,temperature.gpu,utilization.memory,memory.total,memory.used --format=csv,noheader,nounits 2>/dev/null", "r");
            if (p) {
                unsigned int use = 0, temp = 0, vuse = 0;
                double vtot = 0, vusd = 0;
                if (fscanf(p, "%u, %u, %u, %lf, %lf", &use, &temp, &vuse, &vtot, &vusd) == 5) {
                    g->usage_percent = use;
                    g->temperature_c = temp;
                    g->total_vram_gb = vtot / 1024.0; /* MB to GB */
                    g->used_vram_gb = vusd / 1024.0;
                    g->vram_percent = (g->used_vram_gb / g->total_vram_gb) * 100.0;
                }
                pclose(p);
            }
            gpu_idx++;
        }
    }

    /* Set GPU count */
    if (gpu_idx > 0) {
        gpu->gpu_count = gpu_idx;
    } else {
        /* Fallback simulation/default if no hardware GPUs detected */
        gpu->gpu_count = 1;
        GpuDevice *g = &gpu->gpus[0];
        g->present = true;
        strcpy(g->brand, "Intel");
        strcpy(g->model, "Intel HD Graphics (Fallback)");
        g->usage_percent = 5.0;
        g->total_vram_gb = 4.0;
        g->used_vram_gb = 0.6;
        g->vram_percent = (g->used_vram_gb / g->total_vram_gb) * 100.0;
        g->temperature_c = 40.0;
        g->core_clock_mhz = 600.0;
    }
}

static void read_network_stats(NetworkStats *net) {
    FILE *f = fopen("/proc/net/dev", "r");
    if (!f) return;

    char line[1024];
    int count = 0;
    double current_time = get_time_sec();
    
    /* Skip header 2 lines */
    if (fgets(line, sizeof(line), f)) {}
    if (fgets(line, sizeof(line), f)) {}

    net->total_rx_speed_kbps = 0.0;
    net->total_tx_speed_kbps = 0.0;

    while (fgets(line, sizeof(line), f) && count < MAX_NET_INTERFACES) {
        char name[32];
        unsigned long long rx_bytes = 0, tx_bytes = 0;
        unsigned long long dummy;
        
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        
        strcpy(name, trim(line));
        
        /* Check if loopback */
        if (strcmp(name, "lo") == 0) continue;

        sscanf(colon + 1, "%llu %llu %llu %llu %llu %llu %llu %llu %llu",
               &rx_bytes, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &dummy, &tx_bytes);

        NetInterface *ni = &net->interfaces[count];
        strcpy(ni->name, name);
        ni->is_up = true; /* Basic assumption */
        ni->total_rx_bytes = rx_bytes;
        ni->total_tx_bytes = tx_bytes;
        
        /* Find IP address */
        strcpy(ni->ip_address, "Disconnected");
        struct ifaddrs *ifaddr, *ifa;
        if (getifaddrs(&ifaddr) == 0) {
            for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
                if (ifa->ifa_addr && strcmp(ifa->ifa_name, name) == 0 && ifa->ifa_addr->sa_family == AF_INET) {
                    struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
                    strcpy(ni->ip_address, inet_ntoa(sa->sin_addr));
                    break;
                }
            }
            freeifaddrs(ifaddr);
        }

        /* Calculate delta speed */
        int prev_idx = -1;
        for (int i = 0; i < prev_net_count; i++) {
            if (strcmp(prev_net_states[i].name, name) == 0) {
                prev_idx = i;
                break;
            }
        }

        if (prev_idx != -1) {
            double time_diff = current_time - prev_net_states[prev_idx].timestamp;
            if (time_diff > 0.0) {
                long long rx_diff = rx_bytes - prev_net_states[prev_idx].rx_bytes;
                long long tx_diff = tx_bytes - prev_net_states[prev_idx].tx_bytes;
                if (rx_diff < 0) rx_diff = 0; /* overflow wrap */
                if (tx_diff < 0) tx_diff = 0;
                
                /* bytes/sec to kbps: bytes * 8 / 1000 */
                ni->rx_speed_kbps = ((rx_diff / time_diff) * 8.0) / 1000.0;
                ni->tx_speed_kbps = ((tx_diff / time_diff) * 8.0) / 1000.0;
            }
        } else {
            ni->rx_speed_kbps = 0.0;
            ni->tx_speed_kbps = 0.0;
        }

        net->total_rx_speed_kbps += ni->rx_speed_kbps;
        net->total_tx_speed_kbps += ni->tx_speed_kbps;

        /* Store current state as previous */
        strcpy(prev_net_states[count].name, name);
        prev_net_states[count].rx_bytes = rx_bytes;
        prev_net_states[count].tx_bytes = tx_bytes;
        prev_net_states[count].timestamp = current_time;

        count++;
    }
    fclose(f);
    prev_net_count = count;
    net->interface_count = count;
}

static void read_storage_stats(StorageStats *store) {
    store->mount_count = 0;
    store->total_storage_gb = 0.0;
    store->used_storage_gb = 0.0;

    FILE *f = fopen("/proc/mounts", "r");
    if (!f) return;

    char line[512];
    int count = 0;
    while (fgets(line, sizeof(line), f) && count < MAX_STORAGE_MOUNTS) {
        char dev[256], mnt[256], type[256], opts[256];
        int freq, passno;
        if (sscanf(line, "%255s %255s %255s %255s %d %d", dev, mnt, type, opts, &freq, &passno) < 4) continue;

        /* Filter physical drives only, e.g. starting with /dev/sd, /dev/nvme, /dev/mapper, /dev/mmcblk */
        if (strncmp(dev, "/dev/sd", 7) != 0 &&
            strncmp(dev, "/dev/nvme", 9) != 0 &&
            strncmp(dev, "/dev/mapper", 11) != 0 &&
            strncmp(dev, "/dev/md", 7) != 0) {
            continue;
        }

        /* Check duplicate mounts */
        bool duplicate = false;
        for (int i = 0; i < count; i++) {
            if (strcmp(store->mounts[i].mount_point, mnt) == 0) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;

        struct statvfs sv;
        if (statvfs(mnt, &sv) == 0) {
            StorageMount *sm = &store->mounts[count];
            strcpy(sm->mount_point, mnt);
            strcpy(sm->device, dev);
            
            sm->total_gb = (double)sv.f_blocks * sv.f_frsize / (1024.0 * 1024.0 * 1024.0);
            double free_gb = (double)sv.f_bfree * sv.f_frsize / (1024.0 * 1024.0 * 1024.0);
            sm->used_gb = sm->total_gb - free_gb;
            
            if (sm->total_gb > 0) {
                sm->usage_percent = (sm->used_gb / sm->total_gb) * 100.0;
            } else {
                sm->usage_percent = 0.0;
            }

            store->total_storage_gb += sm->total_gb;
            store->used_storage_gb += sm->used_gb;
            count++;
        }
    }
    fclose(f);
    store->mount_count = count;

    if (store->total_storage_gb > 0) {
        store->storage_percent = (store->used_storage_gb / store->total_storage_gb) * 100.0;
    } else {
        store->storage_percent = 0.0;
    }

    /* Disk IO Speed calculations from /proc/diskstats */
    f = fopen("/proc/diskstats", "r");
    store->read_speed_mbps = 0.0;
    store->write_speed_mbps = 0.0;
    
    if (f) {
        double current_time = get_time_sec();
        char ds_line[256];
        int ds_count = 0;

        while (fgets(ds_line, sizeof(ds_line), f) && ds_count < MAX_STORAGE_MOUNTS) {
            unsigned int major, minor;
            char dev_name[32];
            unsigned long long read_ios, read_merges, read_sectors, read_ticks;
            unsigned long long write_ios, write_merges, write_sectors, write_ticks;
            
            int parsed = sscanf(ds_line, "%u %u %s %llu %llu %llu %llu %llu %llu %llu %llu",
                                &major, &minor, dev_name, &read_ios, &read_merges, &read_sectors, &read_ticks,
                                &write_ios, &write_merges, &write_sectors, &write_ticks);
            if (parsed < 11) continue;

            /* Filter common primary disk devices (e.g., sda, nvme0n1, etc., skip partitions like sda1) */
            /* We can check if it ends with digit, but for NVMe it's nvme0n1. So let's match sda, sdb, sdc or nvme0n1, nvme1n1 */
            bool is_primary = false;
            if ((strncmp(dev_name, "sd", 2) == 0 && strlen(dev_name) == 3) ||
                (strncmp(dev_name, "nvme", 4) == 0 && strstr(dev_name, "n") && !strstr(dev_name, "p"))) {
                is_primary = true;
            }

            if (!is_primary) continue;

            int prev_idx = -1;
            for (int i = 0; i < prev_disk_count; i++) {
                if (strcmp(prev_disk_states[i].name, dev_name) == 0) {
                    prev_idx = i;
                    break;
                }
            }

            if (prev_idx != -1) {
                double time_diff = current_time - prev_disk_states[prev_idx].timestamp;
                if (time_diff > 0.0) {
                    long long r_sec_diff = read_sectors - prev_disk_states[prev_idx].read_sectors;
                    long long w_sec_diff = write_sectors - prev_disk_states[prev_idx].write_sectors;
                    if (r_sec_diff < 0) r_sec_diff = 0;
                    if (w_sec_diff < 0) w_sec_diff = 0;
                    
                    /* Sector is usually 512 bytes. MB/s = (sectors * 512) / (1024 * 1024) / time_diff */
                    store->read_speed_mbps += ((r_sec_diff * 512.0) / (1024.0 * 1024.0)) / time_diff;
                    store->write_speed_mbps += ((w_sec_diff * 512.0) / (1024.0 * 1024.0)) / time_diff;
                }
            }

            strcpy(prev_disk_states[ds_count].name, dev_name);
            prev_disk_states[ds_count].read_sectors = read_sectors;
            prev_disk_states[ds_count].write_sectors = write_sectors;
            prev_disk_states[ds_count].timestamp = current_time;
            ds_count++;
        }
        fclose(f);
        prev_disk_count = ds_count;
    }
}

static void read_battery_stats(BatteryStats *bat) {
    bat->present = false;
    DIR *dir = opendir("/sys/class/power_supply");
    if (!dir) return;

    struct dirent *entry;
    char bat_name[64] = "";
    while ((entry = readdir(dir)) != NULL) {
        if (strncmp(entry->d_name, "BAT", 3) == 0) {
            strcpy(bat_name, entry->d_name);
            bat->present = true;
            break;
        }
    }
    closedir(dir);

    if (!bat->present) return;

    char path[256];
    
    /* Charge Capacity */
    snprintf(path, sizeof(path), "/sys/class/power_supply/%s/capacity", bat_name);
    FILE *f = fopen(path, "r");
    if (f) {
        fscanf(f, "%lf", &bat->charge_percent);
        fclose(f);
    }

    /* Status */
    snprintf(path, sizeof(path), "/sys/class/power_supply/%s/status", bat_name);
    f = fopen(path, "r");
    if (f) {
        char status_raw[64];
        if (fgets(status_raw, sizeof(status_raw), f)) {
            strcpy(bat->status, trim(status_raw));
        }
        fclose(f);
    } else {
        strcpy(bat->status, "Unknown");
    }

    /* Voltage */
    snprintf(path, sizeof(path), "/sys/class/power_supply/%s/voltage_now", bat_name);
    f = fopen(path, "r");
    if (f) {
        double v_raw = 0;
        fscanf(f, "%lf", &v_raw);
        bat->voltage_v = v_raw / 1000000.0; /* microvolts to volts */
        fclose(f);
    }

    /* Energy/Current Rate */
    snprintf(path, sizeof(path), "/sys/class/power_supply/%s/power_now", bat_name);
    f = fopen(path, "r");
    if (f) {
        double p_raw = 0;
        fscanf(f, "%lf", &p_raw);
        bat->energy_rate_w = p_raw / 1000000.0; /* microwatts to watts */
        fclose(f);
    } else {
        /* Try current_now * voltage_now */
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/current_now", bat_name);
        FILE *f_curr = fopen(path, "r");
        if (f_curr) {
            double c_raw = 0;
            fscanf(f_curr, "%lf", &c_raw);
            fclose(f_curr);
            bat->energy_rate_w = (c_raw / 1000000.0) * bat->voltage_v; /* microamps to amps * volts */
        } else {
            bat->energy_rate_w = 0.0;
        }
    }

    /* Temp */
    snprintf(path, sizeof(path), "/sys/class/power_supply/%s/temp", bat_name);
    f = fopen(path, "r");
    if (f) {
        double t_raw = 0;
        fscanf(f, "%lf", &t_raw);
        bat->temp_c = t_raw / 10.0; /* usually tenths of degree */
        if (bat->temp_c > 100.0) bat->temp_c /= 10.0; /* sometimes millidegrees */
        fclose(f);
    } else {
        bat->temp_c = 30.0;
    }

    /* Time to empty / full */
    bat->time_to_empty_min = -1;
    bat->time_to_full_min = -1;
    
    snprintf(path, sizeof(path), "/sys/class/power_supply/%s/time_to_empty_now", bat_name);
    f = fopen(path, "r");
    if (f) {
        int sec = -1;
        if (fscanf(f, "%d", &sec) == 1 && sec > 0) {
            bat->time_to_empty_min = sec / 60;
        }
        fclose(f);
    }
    
    snprintf(path, sizeof(path), "/sys/class/power_supply/%s/time_to_full_now", bat_name);
    f = fopen(path, "r");
    if (f) {
        int sec = -1;
        if (fscanf(f, "%d", &sec) == 1 && sec > 0) {
            bat->time_to_full_min = sec / 60;
        }
        fclose(f);
    }

    /* Health estimate */
    double energy_full = 0.0, energy_full_design = 0.0;
    snprintf(path, sizeof(path), "/sys/class/power_supply/%s/energy_full", bat_name);
    f = fopen(path, "r");
    if (f) {
        fscanf(f, "%lf", &energy_full);
        fclose(f);
    }
    snprintf(path, sizeof(path), "/sys/class/power_supply/%s/energy_full_design", bat_name);
    f = fopen(path, "r");
    if (f) {
        fscanf(f, "%lf", &energy_full_design);
        fclose(f);
    }
    
    if (energy_full > 0 && energy_full_design > 0) {
        bat->health_percent = (energy_full / energy_full_design) * 100.0;
    } else {
        /* Try charge_full / charge_full_design */
        double charge_full = 0.0, charge_full_design = 0.0;
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/charge_full", bat_name);
        f = fopen(path, "r");
        if (f) {
            fscanf(f, "%lf", &charge_full);
            fclose(f);
        }
        snprintf(path, sizeof(path), "/sys/class/power_supply/%s/charge_full_design", bat_name);
        f = fopen(path, "r");
        if (f) {
            fscanf(f, "%lf", &charge_full_design);
            fclose(f);
        }
        if (charge_full > 0 && charge_full_design > 0) {
            bat->health_percent = (charge_full / charge_full_design) * 100.0;
        } else {
            bat->health_percent = 100.0; /* Fallback */
        }
    }
}

void system_reader_update(SystemStats *stats) {
    double current_time = get_time_sec();
    double elapsed = current_time - g_last_time;
    if (elapsed <= 0.0) elapsed = 1.0;
    
    g_last_time = current_time;

    if (g_demo_mode) {
        generate_simulation_data(stats, elapsed);
        return;
    }

    /* Read actual metrics from OS */
    read_cpu_stats(&stats->cpu);
    read_memory_stats(&stats->memory);
    read_gpu_stats(&stats->gpu);
    read_network_stats(&stats->network);
    read_storage_stats(&stats->storage);
    read_battery_stats(&stats->battery);
}
