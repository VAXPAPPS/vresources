#ifndef UI_H
#define UI_H

#include <gtk/gtk.h>
#include "system_reader.h"
#include "charts.h"

typedef struct {
    SystemStats stats;
    
    /* Chart data structures */
    HistoryChart *cpu_history;
    HistoryChart *mem_history;
    HistoryChart *gpu_histories[MAX_GPUS];
    HistoryChart *net_rx_history;
    HistoryChart *net_tx_history;
    HistoryChart *disk_r_history;
    HistoryChart *disk_w_history;
    HistoryChart *bat_history;
    
    /* Radial Gauges */
    RadialGauge cpu_gauge;
    RadialGauge mem_gauge;
    RadialGauge gpu_gauges[MAX_GPUS];
    RadialGauge bat_gauge;
    
    /* GTK widgets that need live updating */
    GtkWidget *stack;
    
    /* Overview widgets */
    GtkWidget *lbl_ov_cpu_val;
    GtkWidget *lbl_ov_cpu_sub;
    GtkWidget *lbl_ov_mem_val;
    GtkWidget *lbl_ov_mem_sub;
    GtkWidget *lbl_ov_gpu_val[MAX_GPUS];
    GtkWidget *lbl_ov_gpu_sub[MAX_GPUS];
    GtkWidget *lbl_ov_net_val;
    GtkWidget *lbl_ov_net_sub;
    GtkWidget *lbl_ov_store_val;
    GtkWidget *lbl_ov_store_sub;
    GtkWidget *lbl_ov_bat_val;
    GtkWidget *lbl_ov_bat_sub;
    
    GtkWidget *da_ov_cpu;
    GtkWidget *da_ov_mem;
    GtkWidget *da_ov_gpu[MAX_GPUS];
    GtkWidget *da_ov_bat;

    /* CPU Details widgets */
    GtkWidget *lbl_cpu_model;
    GtkWidget *lbl_cpu_speed;
    GtkWidget *lbl_cpu_temp;
    GtkWidget *lbl_cpu_cores_info;
    GtkWidget *grid_cpu_cores;
    GtkWidget *core_progress[MAX_CPU_CORES];
    GtkWidget *core_labels[MAX_CPU_CORES];
    GtkWidget *da_cpu_chart;
    
    /* Memory Details widgets */
    GtkWidget *lbl_mem_used;
    GtkWidget *lbl_mem_avail;
    GtkWidget *lbl_mem_active;
    GtkWidget *lbl_mem_cached;
    GtkWidget *lbl_mem_swap;
    GtkWidget *da_mem_chart;
    GtkWidget *da_mem_gauge;
    
    /* GPU Details widgets */
    GtkWidget *box_gpu_selector;
    GtkWidget *gpu_selector_btn[MAX_GPUS];
    int selected_gpu_idx;
    GtkWidget *lbl_gpu_model;
    GtkWidget *lbl_gpu_load;
    GtkWidget *lbl_gpu_vram;
    GtkWidget *lbl_gpu_temp;
    GtkWidget *lbl_gpu_clock;
    GtkWidget *da_gpu_chart;
    GtkWidget *da_gpu_gauge;
    
    /* Network Details widgets */
    GtkWidget *lbl_net_total_rx;
    GtkWidget *lbl_net_total_tx;
    GtkWidget *lbl_net_current_rx;
    GtkWidget *lbl_net_current_tx;
    GtkWidget *box_net_interfaces;
    GtkWidget *da_net_chart;
    
    /* Storage Details widgets */
    GtkWidget *lbl_store_io_read;
    GtkWidget *lbl_store_io_write;
    GtkWidget *box_store_mounts;
    GtkWidget *da_store_chart;
    
    /* Battery Details widgets */
    GtkWidget *lbl_bat_status;
    GtkWidget *lbl_bat_health;
    GtkWidget *lbl_bat_voltage;
    GtkWidget *lbl_bat_rate;
    GtkWidget *lbl_bat_temp;
    GtkWidget *lbl_bat_remaining;
    GtkWidget *da_bat_chart;
    GtkWidget *da_bat_gauge;
    
    /* Demo Mode Switch */
    GtkWidget *switch_demo;
    
    /* Timer IDs for clean cleanup */
    unsigned int telemetry_timer_id;
    unsigned int animation_timer_id;
    
    /* Process view context pointer */
    gpointer process_view_context;
    
    /* Header bar and controls box */
    GtkWidget *headerbar;
    GtkWidget *process_controls_box;
    
    /* Main window pointer */
    GtkWidget *main_window;
    
} UIContext;

GtkWidget *create_main_ui(GtkApplication *app, UIContext *ctx);
void update_ui(UIContext *ctx);
void ui_animate_tick(UIContext *ctx);
void ui_set_telemetry_interval(UIContext *ctx, int interval_ms);

#endif /* UI_H */
