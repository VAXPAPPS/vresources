#include <gtk/gtk.h>
#include "ui.h"
#include "system_reader.h"

static gboolean on_telemetry_tick(gpointer user_data) {
    UIContext *ctx = (UIContext *)user_data;
    system_reader_update(&ctx->stats);
    update_ui(ctx);
    return G_SOURCE_CONTINUE;
}

void ui_set_telemetry_interval(UIContext *ctx, int interval_ms) {
    if (!ctx) return;
    if (ctx->telemetry_timer_id > 0) {
        g_source_remove(ctx->telemetry_timer_id);
    }
    if (interval_ms < 50) interval_ms = 50;
    ctx->telemetry_timer_id = g_timeout_add(interval_ms, (GSourceFunc)on_telemetry_tick, ctx);
}

static gboolean on_animation_tick(gpointer user_data) {
    UIContext *ctx = (UIContext *)user_data;
    ui_animate_tick(ctx);
    return G_SOURCE_CONTINUE;
}

static void on_window_destroy(GtkWidget *widget, gpointer user_data) {
    (void)widget;
    UIContext *ctx = (UIContext *)user_data;
    
    /* Safely cancel background timer ticks */
    if (ctx->telemetry_timer_id > 0) {
        g_source_remove(ctx->telemetry_timer_id);
    }
    if (ctx->animation_timer_id > 0) {
        g_source_remove(ctx->animation_timer_id);
    }
    
    /* Clean up resource telemetry handles */
    system_reader_cleanup();
    
    /* Free chart structures */
    chart_history_free(ctx->cpu_history);
    chart_history_free(ctx->mem_history);
    for (int i = 0; i < MAX_GPUS; i++) {
        chart_history_free(ctx->gpu_histories[i]);
    }
    chart_history_free(ctx->net_rx_history);
    chart_history_free(ctx->net_tx_history);
    chart_history_free(ctx->disk_r_history);
    chart_history_free(ctx->disk_w_history);
    chart_history_free(ctx->bat_history);
    
    /* Clean up process telemetry backend and context */
    process_reader_cleanup();
    if (ctx->process_view_context) {
        g_free(ctx->process_view_context);
    }
    
    g_free(ctx);
    g_print("VResources: Context cleaned up successfully. Exiting.\n");
}

static void activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    
    /* Allocate heap context for application lifetime */
    UIContext *ctx = g_new0(UIContext, 1);
    
    /* Initialize Linux procfs/sysfs parser */
    system_reader_init();
    
    /* Construct main layouts and bindings */
    GtkWidget *win = create_main_ui(app, ctx);
    
    /* Connect window destruction to cleanup */
    g_signal_connect(win, "destroy", G_CALLBACK(on_window_destroy), ctx);
    
    /* Register 1s telemetry updater loop */
    ctx->telemetry_timer_id = g_timeout_add(1000, (GSourceFunc)on_telemetry_tick, ctx);
    
    /* Register 50ms (20 FPS) fluid GUI wave animation loop */
    ctx->animation_timer_id = g_timeout_add(50, (GSourceFunc)on_animation_tick, ctx);
}

int main(int argc, char *argv[]) {
    GtkApplication *app = gtk_application_new("org.vaxp.vresources", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    
    return status;
}
