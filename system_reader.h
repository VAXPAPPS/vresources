#ifndef SYSTEM_READER_H
#define SYSTEM_READER_H

#include <stdbool.h>

#define MAX_CPU_CORES 128
#define MAX_NET_INTERFACES 16
#define MAX_STORAGE_MOUNTS 16

typedef struct {
    char model[128];
    double total_usage;
    int core_count;
    double core_usage[MAX_CPU_CORES];
    double frequency_ghz;
    double temperature_c;
} CpuStats;

typedef struct {
    double total_ram_gb;
    double used_ram_gb;
    double active_ram_gb;
    double cached_ram_gb;
    double buffers_ram_gb;
    double total_swap_gb;
    double used_swap_gb;
    double ram_percent;
    double swap_percent;
} MemoryStats;

#define MAX_GPUS 2

typedef struct {
    bool present;
    char model[128];
    char brand[32];             /* "Intel", "AMD", "NVIDIA", "Unknown" */
    double usage_percent;
    double total_vram_gb;
    double used_vram_gb;
    double vram_percent;
    double temperature_c;
    double core_clock_mhz;
} GpuDevice;

typedef struct {
    int gpu_count;
    GpuDevice gpus[MAX_GPUS];
} GpuStats;

typedef struct {
    char name[32];
    bool is_up;
    char ip_address[48];
    double rx_speed_kbps;  /* Kilobits per second */
    double tx_speed_kbps;
    unsigned long long total_rx_bytes;
    unsigned long long total_tx_bytes;
} NetInterface;

typedef struct {
    int interface_count;
    NetInterface interfaces[MAX_NET_INTERFACES];
    double total_rx_speed_kbps;
    double total_tx_speed_kbps;
} NetworkStats;

typedef struct {
    char mount_point[128];
    char device[128];
    double total_gb;
    double used_gb;
    double usage_percent;
} StorageMount;

typedef struct {
    int mount_count;
    StorageMount mounts[MAX_STORAGE_MOUNTS];
    double total_storage_gb;
    double used_storage_gb;
    double storage_percent;
    double read_speed_mbps;  /* Megabytes per second */
    double write_speed_mbps;
} StorageStats;

typedef struct {
    bool present;
    double charge_percent;
    char status[32];          /* "Charging", "Discharging", "Full", "Not charging", "Unknown" */
    double health_percent;
    double voltage_v;
    double energy_rate_w;     /* Power consumption rate */
    double temp_c;
    int time_to_empty_min;    /* -1 if unknown/charging */
    int time_to_full_min;     /* -1 if unknown/discharging */
} BatteryStats;

typedef struct {
    CpuStats cpu;
    MemoryStats memory;
    GpuStats gpu;
    NetworkStats network;
    StorageStats storage;
    BatteryStats battery;
} SystemStats;

void system_reader_init(void);
void system_reader_update(SystemStats *stats);
void system_reader_cleanup(void);

#endif /* SYSTEM_READER_H */
