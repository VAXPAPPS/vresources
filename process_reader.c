#define _GNU_SOURCE
#include "process_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <ctype.h>
#include <math.h>

#define CACHE_SIZE 2048

typedef struct {
    int pid;
    unsigned long long prev_ticks;
    double last_time;
    bool active;
} PidCache;

static PidCache g_pid_cache[CACHE_SIZE];
static int g_pid_cache_count = 0;
static double g_last_clean_time = 0.0;

/* Helper to get monotonic time in seconds */
static double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void process_reader_init(void) {
    memset(g_pid_cache, 0, sizeof(g_pid_cache));
    g_pid_cache_count = 0;
    g_last_clean_time = get_time_sec();
}

void process_reader_cleanup(void) {
    /* Nothing to clean up */
}

/* Helper comparison function to sort processes by CPU usage descending */
static int compare_processes(const void *a, const void *b) {
    const ProcessInfo *pa = (const ProcessInfo *)a;
    const ProcessInfo *pb = (const ProcessInfo *)b;
    
    if (pa->cpu_percent > pb->cpu_percent) return -1;
    if (pa->cpu_percent < pb->cpu_percent) return 1;
    if (pa->ram_mb > pb->ram_mb) return -1;
    if (pa->ram_mb < pb->ram_mb) return 1;
    return 0;
}

/* Helper to clean stale cache entries once in a while */
static void clean_stale_cache(double current_time) {
    int active_idx = 0;
    for (int i = 0; i < g_pid_cache_count; i++) {
        /* Keep if active or updated in the last 10 seconds */
        if (g_pid_cache[i].active || (current_time - g_pid_cache[i].last_time < 10.0)) {
            if (active_idx != i) {
                g_pid_cache[active_idx] = g_pid_cache[i];
            }
            g_pid_cache[active_idx].active = false; /* reset flag for next cycle */
            active_idx++;
        }
    }
    g_pid_cache_count = active_idx;
}

/* --- Simulation Process Data Generator --- */
static void generate_simulated_processes(ProcessList *list) {
    static double sim_time = 0.0;
    sim_time += 1.0;

    typedef struct {
        int pid;
        char name[128];
        double base_cpu;
        double base_ram;
        double base_gpu;
        double base_cache;
        double speed_factor;
    } SimProcessDef;

    static SimProcessDef defs[] = {
        { 1,    "systemd-init",     0.05,  12.4,  0.0,   4.2,   0.02 },
        { 240,  "xorg-server",      2.4,   85.2,  12.0,  32.5,  0.1  },
        { 650,  "vaxp-core-daemon", 0.8,   45.1,  0.0,   12.8,  0.05 },
        { 880,  "aether-winman",    3.5,   142.6, 25.0,  54.2,  0.2  },
        { 1022, "dbus-daemon",      0.1,   8.2,   0.0,   2.1,   0.01 },
        { 1205, "vsettings-app",    0.2,   34.5,  0.0,   8.4,   0.08 },
        { 1420, "vbrowser-engine",  12.5,  412.8, 42.0,  125.6, 0.3  },
        { 1425, "vbrowser-tab1",    2.1,   198.4, 5.0,   45.2,  0.15 },
        { 1430, "vbrowser-tab2",    8.4,   256.1, 18.0,  62.1,  0.25 },
        { 1802, "pulseaudio",       1.2,   24.8,  0.0,   6.5,   0.12 },
        { 2210, "terminal-daemon",  0.4,   18.6,  0.0,   5.1,   0.05 },
        { 2409, "vresources",       1.5,   38.4,  2.0,   11.2,  0.1  },
        { 2812, "bash-shell",       0.0,   6.4,   0.0,   1.8,   0.02 },
        { 3105, "git-lfs-sync",     0.0,   28.2,  0.0,   8.0,   0.04 }
    };

    int num_defs = sizeof(defs) / sizeof(defs[0]);
    list->process_count = 0;

    for (int i = 0; i < num_defs && list->process_count < MAX_PROCESSES; i++) {
        ProcessInfo *p = &list->processes[list->process_count];
        p->pid = defs[i].pid;
        strcpy(p->name, defs[i].name);

        /* Fluctuating telemetry using sine waves and randomness */
        double cpu = defs[i].base_cpu + (defs[i].base_cpu * 0.8 * sin(sim_time * defs[i].speed_factor)) + ((rand() % 10) / 10.0);
        if (cpu < 0.0) cpu = 0.0;
        p->cpu_percent = cpu;

        p->ram_mb = defs[i].base_ram + (defs[i].base_ram * 0.08 * sin(sim_time * 0.05 + i)) + ((rand() % 20) / 10.0);
        p->cache_mb = defs[i].base_cache + (defs[i].base_cache * 0.04 * cos(sim_time * 0.04 + i));

        if (defs[i].base_gpu > 0.0) {
            double gpu = defs[i].base_gpu + (defs[i].base_gpu * 0.5 * sin(sim_time * defs[i].speed_factor)) + (rand() % 5);
            if (gpu < 0.0) gpu = 0.0;
            p->gpu_percent = gpu;
        } else {
            p->gpu_percent = 0.0;
        }

        list->process_count++;
    }

    /* Sort the simulated list */
    qsort(list->processes, list->process_count, sizeof(ProcessInfo), compare_processes);
}

/* --- Real Linux Telemetry Collection --- */
void process_reader_update(ProcessList *list, bool demo_mode) {
    if (demo_mode) {
        generate_simulated_processes(list);
        return;
    }

    DIR *dir = opendir("/proc");
    if (!dir) {
        list->process_count = 0;
        return;
    }

    struct dirent *entry;
    int count = 0;
    double current_time = get_time_sec();
    long clk_tck = sysconf(_SC_CLK_TCK);
    long page_size = sysconf(_SC_PAGESIZE);

    /* Read NVIDIA process details if available */
    typedef struct {
        int pid;
        float vram_used;
    } NvProc;
    NvProc nv_procs[32];
    int nv_proc_count = 0;
    
    FILE *np = popen("nvidia-smi --query-compute-apps=pid,used_memory --format=csv,noheader,nounits 2>/dev/null", "r");
    if (np) {
        int pid;
        float vram;
        while (fscanf(np, "%d, %f", &pid, &vram) == 2 && nv_proc_count < 32) {
            nv_procs[nv_proc_count].pid = pid;
            nv_procs[nv_proc_count].vram_used = vram;
            nv_proc_count++;
        }
        pclose(np);
    }

    while ((entry = readdir(dir)) != NULL && count < MAX_PROCESSES) {
        /* Check if folder name is all digits (representing a PID) */
        char *p_name = entry->d_name;
        bool is_pid = true;
        while (*p_name) {
            if (!isdigit(*p_name)) {
                is_pid = false;
                break;
            }
            p_name++;
        }

        if (!is_pid) continue;

        int pid = atoi(entry->d_name);
        if (pid <= 0) continue;

        char path[256];
        
        /* 1. Read process name and CPU ticks from /proc/PID/stat */
        snprintf(path, sizeof(path), "/proc/%d/stat", pid);
        FILE *f_stat = fopen(path, "r");
        if (!f_stat) continue;

        char comm[256] = "";
        char state;
        unsigned long long utime = 0, stime = 0;
        
        /* Format: PID (Name) State PPID ... utime [14] stime [15] */
        /* Since process names can contain spaces, we parse till closing parenthesis */
        char line_stat[1024];
        if (fgets(line_stat, sizeof(line_stat), f_stat)) {
            char *open_p = strchr(line_stat, '(');
            char *close_p = strrchr(line_stat, ')');
            if (open_p && close_p && close_p > open_p) {
                int name_len = close_p - (open_p + 1);
                if (name_len > 255) name_len = 255;
                strncpy(comm, open_p + 1, name_len);
                comm[name_len] = '\0';
                
                /* Parse stats after the ')' */
                sscanf(close_p + 2, "%c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu",
                       &state, &utime, &stime);
            }
        }
        fclose(f_stat);

        if (strlen(comm) == 0) continue;

        /* Total process ticks */
        unsigned long long total_proc_ticks = utime + stime;

        /* 2. Read memory details from /proc/PID/statm */
        snprintf(path, sizeof(path), "/proc/%d/statm", pid);
        FILE *f_statm = fopen(path, "r");
        if (!f_statm) continue;
        
        long size = 0, resident = 0, shared = 0;
        if (fscanf(f_statm, "%ld %ld %ld", &size, &resident, &shared) < 3) {
            fclose(f_statm);
            continue;
        }
        fclose(f_statm);

        if (size == 0) continue; /* Skip kernel threads and idle hardware workers with no user-space footprints */

        /* RAM = resident set size (RSS) in MB */
        double ram_mb = (resident * page_size) / (1024.0 * 1024.0);
        /* Cache = shared memory and libraries in MB */
        double cache_mb = (shared * page_size) / (1024.0 * 1024.0);

        /* 3. Retrieve or calculate CPU percentage */
        double cpu_percent = 0.0;
        int cache_idx = -1;
        
        /* Find in local cache */
        for (int i = 0; i < g_pid_cache_count; i++) {
            if (g_pid_cache[i].pid == pid) {
                cache_idx = i;
                break;
            }
        }

        if (cache_idx != -1) {
            double dt = current_time - g_pid_cache[cache_idx].last_time;
            if (dt > 0.1) {
                long long ticks_diff = total_proc_ticks - g_pid_cache[cache_idx].prev_ticks;
                if (ticks_diff < 0) ticks_diff = 0;
                
                cpu_percent = (ticks_diff / (double)clk_tck) / dt * 100.0;
            }
            g_pid_cache[cache_idx].prev_ticks = total_proc_ticks;
            g_pid_cache[cache_idx].last_time = current_time;
            g_pid_cache[cache_idx].active = true;
        } else {
            /* Add new process to cache */
            if (g_pid_cache_count < CACHE_SIZE) {
                g_pid_cache[g_pid_cache_count].pid = pid;
                g_pid_cache[g_pid_cache_count].prev_ticks = total_proc_ticks;
                g_pid_cache[g_pid_cache_count].last_time = current_time;
                g_pid_cache[g_pid_cache_count].active = true;
                g_pid_cache_count++;
            }
            cpu_percent = 0.0;
        }

        /* Populate results */
        ProcessInfo *p = &list->processes[count];
        p->pid = pid;
        
        /* Read full command path as fallback if name is too brief */
        snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
        FILE *f_cmd = fopen(path, "r");
        if (f_cmd) {
            char cmdline[256];
            if (fgets(cmdline, sizeof(cmdline), f_cmd)) {
                /* extract executable name from path */
                char *slash = strrchr(cmdline, '/');
                if (slash) {
                    strncpy(p->name, slash + 1, sizeof(p->name) - 1);
                } else {
                    strncpy(p->name, cmdline, sizeof(p->name) - 1);
                }
                p->name[sizeof(p->name) - 1] = '\0';
            } else {
                strcpy(p->name, comm);
            }
            fclose(f_cmd);
        } else {
            strcpy(p->name, comm);
        }
        
        /* Sanitize names with strange characters */
        if (strlen(p->name) == 0) {
            strcpy(p->name, comm);
        }

        p->cpu_percent = cpu_percent;
        p->ram_mb = ram_mb;
        p->cache_mb = cache_mb;

        /* 4. GPU mapping */
        double gpu_percent = 0.0;
        for (int i = 0; i < nv_proc_count; i++) {
            if (nv_procs[i].pid == pid) {
                /* Set simulated GPU load proportional to VRAM usage and CPU activity */
                gpu_percent = (nv_procs[i].vram_used > 500.0) ? (15.0 + cpu_percent * 0.5) : 1.0;
                break;
            }
        }
        /* Fallback for graphic processes (Xorg/Wayland compositor) if NVIDIA is absent */
        if (gpu_percent == 0.0 && nv_proc_count == 0) {
            char lower_name[256] = "";
            int j = 0;
            for (j = 0; p->name[j] && j < 255; j++) {
                lower_name[j] = tolower((unsigned char)p->name[j]);
            }
            lower_name[j] = '\0';

            if (strstr(lower_name, "xorg") || 
                strstr(lower_name, "wayland") || 
                strstr(lower_name, "gnome-shell") || 
                strstr(lower_name, "mutter") || 
                strstr(lower_name, "kwin") ||
                strstr(lower_name, "chrome") || 
                strstr(lower_name, "firefox") || 
                strstr(lower_name, "vresources") ||
                strstr(lower_name, "vbrowser")) {
                
                /* Estimate GPU load based on CPU activity */
                gpu_percent = 0.5 + cpu_percent * 0.3;
                if (gpu_percent > 100.0) gpu_percent = 100.0;
            }
        }
        p->gpu_percent = gpu_percent;

        count++;
    }
    closedir(dir);
    list->process_count = count;

    /* Clean old cache entries every 15 seconds */
    if (current_time - g_last_clean_time > 15.0) {
        clean_stale_cache(current_time);
        g_last_clean_time = current_time;
    }

    /* Sort by CPU usage */
    qsort(list->processes, list->process_count, sizeof(ProcessInfo), compare_processes);
}
