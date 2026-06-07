#include "ui.h"
#include "process_view.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double g_wave_phase = 0.0;

/* Helper to create a card box */
static GtkWidget *create_card(const char *title, GtkWidget *content, const char *theme_class) {
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(card, "card");
    gtk_widget_set_margin_start(card, 8);
    gtk_widget_set_margin_end(card, 8);
    gtk_widget_set_margin_top(card, 8);
    gtk_widget_set_margin_bottom(card, 8);
    
    if (title) {
        GtkWidget *lbl_title = gtk_label_new(title);
        gtk_widget_add_css_class(lbl_title, "card-title");
        if (theme_class) {
            gtk_widget_add_css_class(lbl_title, theme_class);
        }
        gtk_widget_set_halign(lbl_title, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(card), lbl_title);
    }
    
    if (content) {
        gtk_box_append(GTK_BOX(card), content);
    }
    
    return card;
}

/* Custom Drawing Functions for multi-line details charts */
static void draw_network_chart_cb(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    UIContext *ctx = (UIContext *)user_data;
    if (!ctx) return;
    
    /* Draw Rx (Download) first - Pink */
    draw_history_chart(area, cr, width, height, ctx->net_rx_history);
    /* Draw Tx (Upload) second - Cyan */
    draw_history_chart(area, cr, width, height, ctx->net_tx_history);
}

static void draw_storage_chart_cb(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    UIContext *ctx = (UIContext *)user_data;
    if (!ctx) return;
    
    /* Draw Read - Yellow */
    draw_history_chart(area, cr, width, height, ctx->disk_r_history);
    /* Draw Write - Orange */
    draw_history_chart(area, cr, width, height, ctx->disk_w_history);
}

/* Custom animated liquid battery drawing function */
static void draw_liquid_battery_cb(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    UIContext *ctx = (UIContext *)user_data;
    if (!ctx) return;
    
    (void)area;
    
    double center_x = width / 2.0;
    double center_y = height / 2.0;
    
    double bat_w = 90.0;
    double bat_h = 150.0;
    double rx = center_x - bat_w / 2.0;
    double ry = center_y - bat_h / 2.0;
    double corner_radius = 14.0;
    
    /* Draw Battery Outer Body Outline */
    cairo_set_source_rgba(cr, 255, 255, 255, 0.15);
    cairo_set_line_width(cr, 6.0);
    
    /* Rounded rect formula in Cairo */
    cairo_new_sub_path(cr);
    cairo_arc(cr, rx + bat_w - corner_radius, ry + corner_radius, corner_radius, -M_PI_2, 0);
    cairo_arc(cr, rx + bat_w - corner_radius, ry + bat_h - corner_radius, corner_radius, 0, M_PI_2);
    cairo_arc(cr, rx + corner_radius, ry + bat_h - corner_radius, corner_radius, M_PI_2, M_PI);
    cairo_arc(cr, rx + corner_radius, ry + corner_radius, corner_radius, M_PI, 3.0 * M_PI_2);
    cairo_close_path(cr);
    cairo_stroke(cr);
    
    /* Draw Battery positive terminal tip (top) */
    double tip_w = 30.0;
    double tip_h = 10.0;
    double tx = center_x - tip_w / 2.0;
    double ty = ry - tip_h;
    
    cairo_set_source_rgba(cr, 255, 255, 255, 0.15);
    cairo_new_sub_path(cr);
    cairo_arc(cr, tx + tip_w - 4.0, ty + tip_h - 4.0, 4.0, -M_PI_2, 0);
    cairo_line_to(cr, tx + tip_w, ty + tip_h);
    cairo_line_to(cr, tx, ty + tip_h);
    cairo_arc(cr, tx + 4.0, ty + tip_h - 4.0, 4.0, M_PI, 3.0 * M_PI_2);
    cairo_close_path(cr);
    cairo_fill(cr);
    
    /* Fill contents with liquid wave based on charge percent */
    double pct = ctx->stats.battery.charge_percent / 100.0;
    if (pct < 0.0) pct = 0.0;
    if (pct > 1.0) pct = 1.0;
    
    if (pct > 0.0) {
        double fill_h = (bat_h - 12.0) * pct;
        double fill_y_target = ry + bat_h - 6.0 - fill_h;
        
        cairo_save(cr);
        
        /* Create clipping path matching the inner battery body */
        cairo_new_sub_path(cr);
        double inner_r = corner_radius - 3.0;
        if (inner_r < 0) inner_r = 0;
        cairo_arc(cr, rx + bat_w - 6.0 - inner_r, ry + 6.0 + inner_r, inner_r, -M_PI_2, 0);
        cairo_arc(cr, rx + bat_w - 6.0 - inner_r, ry + bat_h - 6.0 - inner_r, inner_r, 0, M_PI_2);
        cairo_arc(cr, rx + 6.0 + inner_r, ry + bat_h - 6.0 - inner_r, inner_r, M_PI_2, M_PI);
        cairo_arc(cr, rx + 6.0 + inner_r, ry + 6.0 + inner_r, inner_r, M_PI, 3.0 * M_PI_2);
        cairo_close_path(cr);
        cairo_clip(cr);
        
        /* Draw the liquid wave */
        cairo_new_path(cr);
        cairo_move_to(cr, rx + 6.0, ry + bat_h - 6.0);
        cairo_line_to(cr, rx + bat_w - 6.0, ry + bat_h - 6.0);
        cairo_line_to(cr, rx + bat_w - 6.0, fill_y_target);
        
        /* Draw sine wave top surface */
        double wave_amp = 5.0;  /* 5px wave amplitude */
        double wave_freq = 0.08; /* wavelength scale */
        for (double x = rx + bat_w - 6.0; x >= rx + 6.0; x -= 2.0) {
            double y = fill_y_target + wave_amp * sin((x - rx) * wave_freq + g_wave_phase);
            cairo_line_to(cr, x, y);
        }
        
        cairo_close_path(cr);
        
        /* Choose color based on charge status & level */
        float r = 0.15, g = 0.68, b = 0.38; /* Green */
        if (strcmp(ctx->stats.battery.status, "Charging") == 0) {
            r = 0.95; g = 0.79; b = 0.15; /* Yellow charging */
        } else if (pct <= 0.20) {
            r = 0.95; g = 0.25; b = 0.15; /* Red low battery */
        }
        
        /* Fill with vertical gradient for liquid appearance */
        cairo_pattern_t *liq_grad = cairo_pattern_create_linear(0, fill_y_target - 10.0, 0, ry + bat_h);
        cairo_pattern_add_color_stop_rgba(liq_grad, 0.0, r * 1.2, g * 1.2, b * 1.2, 0.85);
        cairo_pattern_add_color_stop_rgba(liq_grad, 0.4, r, g, b, 0.75);
        cairo_pattern_add_color_stop_rgba(liq_grad, 1.0, r * 0.5, g * 0.5, b * 0.5, 0.9);
        cairo_set_source(cr, liq_grad);
        cairo_fill(cr);
        cairo_pattern_destroy(liq_grad);
        
        cairo_restore(cr);
    }
    
    /* Draw charging lightning bolt overlay if charging */
    if (strcmp(ctx->stats.battery.status, "Charging") == 0) {
        cairo_set_source_rgba(cr, 255, 255, 255, 0.9);
        cairo_new_path(cr);
        cairo_move_to(cr, center_x + 5.0, center_y - 25.0);
        cairo_line_to(cr, center_x - 15.0, center_y + 5.0);
        cairo_line_to(cr, center_x - 2.0, center_y + 5.0);
        cairo_line_to(cr, center_x - 5.0, center_y + 25.0);
        cairo_line_to(cr, center_x + 15.0, center_y - 5.0);
        cairo_line_to(cr, center_x + 2.0, center_y - 5.0);
        cairo_close_path(cr);
        cairo_fill(cr);
    }
    
    /* Draw percentage text in center */
    char pct_str[16];
    snprintf(pct_str, sizeof(pct_str), "%.0f%%", ctx->stats.battery.charge_percent);
    cairo_select_font_face(cr, "Outfit", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 18.0);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, pct_str, &ext);
    
    /* Draw backing shadow for text readability */
    cairo_set_source_rgba(cr, 0, 0, 0, 0.4);
    cairo_move_to(cr, center_x - ext.width/2.0 - ext.x_bearing + 1.0, center_y - ext.height/2.0 - ext.y_bearing + 1.0);
    cairo_show_text(cr, pct_str);
    
    cairo_set_source_rgba(cr, 255, 255, 255, 0.95);
    cairo_move_to(cr, center_x - ext.width/2.0 - ext.x_bearing, center_y - ext.height/2.0 - ext.y_bearing);
    cairo_show_text(cr, pct_str);
}

/* Sidebar Tab toggle callback */
static void on_tab_toggled(GtkToggleButton *button, gpointer user_data) {
    GtkStack *stack = GTK_STACK(user_data);
    if (gtk_toggle_button_get_active(button)) {
        const char *page_name = g_object_get_data(G_OBJECT(button), "page-name");
        gtk_stack_set_visible_child_name(stack, page_name);
    }
}

/* Demo Switch Callback */
static void on_demo_switch_changed(GtkSwitch *widget, gboolean state, gpointer user_data) {
    (void)widget;
    (void)user_data;
    extern void system_reader_set_demo_mode(bool enable);
    system_reader_set_demo_mode(state);
}

/* UI Creation */
GtkWidget *create_main_ui(GtkApplication *app, UIContext *ctx) {
    /* Set up styling */
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(css_provider, "style.css");
    gtk_style_context_add_provider_for_display(gdk_display_get_default(),
                                               GTK_STYLE_PROVIDER(css_provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css_provider);

    /* Allocate historical charts */
    ctx->cpu_history = chart_history_new(120, 0.0, 100.0, false, 0.0, 0.95, 0.99); /* Cyan */
    ctx->mem_history = chart_history_new(120, 0.0, 100.0, false, 0.61, 0.32, 0.88); /* Purple */
    for (int i = 0; i < MAX_GPUS; i++) {
        if (i == 0) {
            ctx->gpu_histories[i] = chart_history_new(120, 0.0, 100.0, false, 0.44, 1.0, 0.0); /* Green iGPU */
        } else {
            ctx->gpu_histories[i] = chart_history_new(120, 0.0, 100.0, false, 0.0, 0.95, 0.99); /* Cyan dGPU */
        }
    }
    ctx->net_rx_history = chart_history_new(120, 0.0, 1000.0, true, 1.0, 0.0, 0.48);  /* Pink (Download) */
    ctx->net_tx_history = chart_history_new(120, 0.0, 1000.0, true, 0.0, 0.95, 0.99);  /* Cyan (Upload) */
    ctx->disk_r_history = chart_history_new(120, 0.0, 10.0, true, 0.95, 0.79, 0.15);  /* Yellow (Read) */
    ctx->disk_w_history = chart_history_new(120, 0.0, 10.0, true, 0.95, 0.45, 0.1);    /* Orange (Write) */
    ctx->bat_history = chart_history_new(120, 0.0, 100.0, false, 0.15, 0.68, 0.38);  /* Green */

    /* Setup default gauge data */
    strcpy(ctx->cpu_gauge.label, "CPU");
    strcpy(ctx->cpu_gauge.unit, "%");
    ctx->cpu_gauge.color_r = 0.0; ctx->cpu_gauge.color_g = 0.95; ctx->cpu_gauge.color_b = 0.99;
    ctx->cpu_gauge.value = 0.0;

    strcpy(ctx->mem_gauge.label, "RAM");
    strcpy(ctx->mem_gauge.unit, "%");
    ctx->mem_gauge.color_r = 0.61; ctx->mem_gauge.color_g = 0.32; ctx->mem_gauge.color_b = 0.88;
    ctx->mem_gauge.value = 0.0;

    for (int i = 0; i < MAX_GPUS; i++) {
        snprintf(ctx->gpu_gauges[i].label, sizeof(ctx->gpu_gauges[i].label), "GPU %d", i);
        strcpy(ctx->gpu_gauges[i].unit, "%");
        if (i == 0) {
            ctx->gpu_gauges[i].color_r = 0.44; ctx->gpu_gauges[i].color_g = 1.0; ctx->gpu_gauges[i].color_b = 0.0;
        } else {
            ctx->gpu_gauges[i].color_r = 0.0; ctx->gpu_gauges[i].color_g = 0.95; ctx->gpu_gauges[i].color_b = 0.99;
        }
        ctx->gpu_gauges[i].value = 0.0;
    }

    strcpy(ctx->bat_gauge.label, "BAT");
    strcpy(ctx->bat_gauge.unit, "%");
    ctx->bat_gauge.color_r = 0.15; ctx->bat_gauge.color_g = 0.68; ctx->bat_gauge.color_b = 0.38;
    ctx->bat_gauge.value = 0.0;

    /* Read initial telemetry to configure core count and specs */
    system_reader_update(&ctx->stats);

    /* Construct Main Window */
    GtkWidget *win = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(win), "VResources Monitor");
    gtk_window_set_default_size(GTK_WINDOW(win), 1020, 680);
    gtk_widget_add_css_class(win, "main-window");

    /* Main box layout */
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_window_set_child(GTK_WINDOW(win), main_box);

    /* --- SIDEBAR ASSEMBLY --- */
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(sidebar, "sidebar");
    gtk_widget_set_size_request(sidebar, 200, -1);
    gtk_box_append(GTK_BOX(main_box), sidebar);

    /* Sidebar Logo/Header */
    GtkWidget *logo_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_bottom(logo_box, 24);
    gtk_widget_set_margin_top(logo_box, 10);
    gtk_box_append(GTK_BOX(sidebar), logo_box);

    GtkWidget *lbl_logo_main = gtk_label_new("VResources");
    GtkWidget *lbl_logo_sub = gtk_label_new("VAXP OS Suite");
    gtk_widget_set_halign(lbl_logo_main, GTK_ALIGN_START);
    gtk_widget_set_halign(lbl_logo_sub, GTK_ALIGN_START);
    
    /* Apply special sizes via inline style contexts */
    gtk_label_set_markup(GTK_LABEL(lbl_logo_main), "<span font='18' weight='bold' color='#ffffff'>VResources</span>");
    gtk_label_set_markup(GTK_LABEL(lbl_logo_sub), "<span font='9' weight='semibold' color='#8c8c8c'>ORGANIZATION VAXP</span>");
    
    gtk_box_append(GTK_BOX(logo_box), lbl_logo_main);
    gtk_box_append(GTK_BOX(logo_box), lbl_logo_sub);

    /* Stack panel setup */
    ctx->stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(ctx->stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_stack_set_transition_duration(GTK_STACK(ctx->stack), 250);
    gtk_box_append(GTK_BOX(main_box), ctx->stack);
    gtk_widget_set_hexpand(ctx->stack, TRUE);
    gtk_widget_set_vexpand(ctx->stack, TRUE);

    /* Helper function to create navigation tabs */
    GtkWidget *first_btn = NULL;
    void add_nav_tab(const char *label, const char *page_name) {
        GtkWidget *btn = gtk_toggle_button_new_with_label(label);
        gtk_widget_add_css_class(btn, "nav-btn");
        g_object_set_data(G_OBJECT(btn), "page-name", (gpointer)page_name);
        
        if (!first_btn) {
            first_btn = btn;
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn), TRUE);
        } else {
            gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(btn), GTK_TOGGLE_BUTTON(first_btn));
        }
        
        g_signal_connect(btn, "toggled", G_CALLBACK(on_tab_toggled), ctx->stack);
        gtk_box_append(GTK_BOX(sidebar), btn);
    }

    add_nav_tab("Dashboard", "dashboard");
    add_nav_tab("CPU", "cpu");
    add_nav_tab("Memory", "memory");
    add_nav_tab("GPU", "gpu");
    add_nav_tab("Network", "network");
    add_nav_tab("Storage", "storage");
    add_nav_tab("Battery", "battery");
    add_nav_tab("App Resources", "processes");

    /* Bottom spacing in sidebar */
    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(sidebar), spacer);
    gtk_widget_set_vexpand(spacer, TRUE);

    /* Demo Mode Switch Container */
    GtkWidget *demo_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_bottom(demo_box, 10);
    gtk_widget_set_halign(demo_box, GTK_ALIGN_CENTER);
    
    GtkWidget *lbl_demo = gtk_label_new("Demo Mode");
    gtk_label_set_markup(GTK_LABEL(lbl_demo), "<span font='9' color='#8c8c8c'>Simulation</span>");
    ctx->switch_demo = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(ctx->switch_demo), FALSE);
    g_signal_connect(ctx->switch_demo, "state-set", G_CALLBACK(on_demo_switch_changed), NULL);
    
    gtk_box_append(GTK_BOX(demo_box), lbl_demo);
    gtk_box_append(GTK_BOX(demo_box), ctx->switch_demo);
    gtk_box_append(GTK_BOX(sidebar), demo_box);

    /* --- PAGE 1: DASHBOARD OVERVIEW --- */
    GtkWidget *page_dashboard = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(page_dashboard), 8);
    gtk_grid_set_column_spacing(GTK_GRID(page_dashboard), 8);
    gtk_widget_set_margin_start(page_dashboard, 12);
    gtk_widget_set_margin_end(page_dashboard, 12);
    gtk_widget_set_margin_top(page_dashboard, 12);
    gtk_widget_set_margin_bottom(page_dashboard, 12);

    /* Card creator for dashboard */
    GtkWidget *make_ov_card(const char *title, const char *theme_class, 
                            GtkWidget **val_out, GtkWidget **sub_out, 
                            GtkWidget **da_out, gpointer gauge_ptr) {
        GtkWidget *cbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_set_size_request(cbox, 240, 110);

        GtkWidget *left_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_set_hexpand(left_vbox, TRUE);
        gtk_box_append(GTK_BOX(cbox), left_vbox);

        *val_out = gtk_label_new("0%");
        gtk_widget_add_css_class(*val_out, "card-value");
        gtk_widget_set_halign(*val_out, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(left_vbox), *val_out);

        *sub_out = gtk_label_new("Loading...");
        gtk_widget_add_css_class(*sub_out, "info-list-label");
        gtk_widget_set_halign(*sub_out, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(left_vbox), *sub_out);

        *da_out = gtk_drawing_area_new();
        gtk_widget_set_size_request(*da_out, 76, 76);
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(*da_out), draw_radial_gauge, gauge_ptr, NULL);
        gtk_box_append(GTK_BOX(cbox), *da_out);

        return create_card(title, cbox, theme_class);
    }

    GtkWidget *card_ov_cpu = make_ov_card("CPU UTILIZATION", "cpu-theme", &ctx->lbl_ov_cpu_val, &ctx->lbl_ov_cpu_sub, &ctx->da_ov_cpu, &ctx->cpu_gauge);
    GtkWidget *card_ov_mem = make_ov_card("MEMORY ALLOCATION", "memory-theme", &ctx->lbl_ov_mem_val, &ctx->lbl_ov_mem_sub, &ctx->da_ov_mem, &ctx->mem_gauge);
    GtkWidget *card_ov_gpu;
    if (ctx->stats.gpu.gpu_count == 1) {
        card_ov_gpu = make_ov_card("GPU TELEMETRY", "gpu-theme", 
                                   &ctx->lbl_ov_gpu_val[0], &ctx->lbl_ov_gpu_sub[0], 
                                   &ctx->da_ov_gpu[0], &ctx->gpu_gauges[0]);
    } else {
        GtkWidget *gpu_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
        for (int i = 0; i < 2; i++) {
            GtkWidget *sub_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
            gtk_widget_set_hexpand(sub_box, TRUE);
            
            char gpu_lbl_text[32];
            snprintf(gpu_lbl_text, sizeof(gpu_lbl_text), "GPU %d (%s)", i, ctx->stats.gpu.gpus[i].brand);
            GtkWidget *lbl_title = gtk_label_new(NULL);
            gtk_label_set_markup(GTK_LABEL(lbl_title), g_strdup_printf("<span font='8' weight='bold' color='#8c8c8c'>%s</span>", gpu_lbl_text));
            gtk_widget_set_halign(lbl_title, GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(sub_box), lbl_title);
            
            GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            GtkWidget *vtext_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
            gtk_widget_set_hexpand(vtext_box, TRUE);
            
            ctx->lbl_ov_gpu_val[i] = gtk_label_new("0%");
            gtk_widget_add_css_class(ctx->lbl_ov_gpu_val[i], "card-value");
            gtk_label_set_markup(GTK_LABEL(ctx->lbl_ov_gpu_val[i]), "<span font='16' weight='bold'>0%</span>");
            gtk_widget_set_halign(ctx->lbl_ov_gpu_val[i], GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(vtext_box), ctx->lbl_ov_gpu_val[i]);
            
            ctx->lbl_ov_gpu_sub[i] = gtk_label_new("VRAM: --");
            gtk_widget_add_css_class(ctx->lbl_ov_gpu_sub[i], "info-list-label");
            gtk_widget_set_halign(ctx->lbl_ov_gpu_sub[i], GTK_ALIGN_START);
            gtk_box_append(GTK_BOX(vtext_box), ctx->lbl_ov_gpu_sub[i]);
            
            gtk_box_append(GTK_BOX(row_box), vtext_box);
            
            ctx->da_ov_gpu[i] = gtk_drawing_area_new();
            gtk_widget_set_size_request(ctx->da_ov_gpu[i], 56, 56);
            gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(ctx->da_ov_gpu[i]), draw_radial_gauge, &ctx->gpu_gauges[i], NULL);
            gtk_box_append(GTK_BOX(row_box), ctx->da_ov_gpu[i]);
            
            gtk_box_append(GTK_BOX(sub_box), row_box);
            gtk_box_append(GTK_BOX(gpu_hbox), sub_box);
        }
        card_ov_gpu = create_card("GPU TELEMETRY", gpu_hbox, "gpu-theme");
    }
    
    /* Storage card (No gauge, custom bar) */
    GtkWidget *store_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    ctx->lbl_ov_store_val = gtk_label_new("0%");
    gtk_widget_add_css_class(ctx->lbl_ov_store_val, "card-value");
    gtk_widget_set_halign(ctx->lbl_ov_store_val, GTK_ALIGN_START);
    ctx->lbl_ov_store_sub = gtk_label_new("0 GB / 0 GB");
    gtk_widget_add_css_class(ctx->lbl_ov_store_sub, "info-list-label");
    gtk_widget_set_halign(ctx->lbl_ov_store_sub, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(store_vbox), ctx->lbl_ov_store_val);
    gtk_box_append(GTK_BOX(store_vbox), ctx->lbl_ov_store_sub);
    GtkWidget *card_ov_store = create_card("STORAGE CAPACITY", store_vbox, "storage-theme");
    gtk_widget_set_size_request(card_ov_store, 240, 110);

    /* Network card (No gauge, Tx/Rx display) */
    GtkWidget *net_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    ctx->lbl_ov_net_val = gtk_label_new("0 Kbps");
    gtk_widget_add_css_class(ctx->lbl_ov_net_val, "card-value");
    gtk_widget_set_halign(ctx->lbl_ov_net_val, GTK_ALIGN_START);
    ctx->lbl_ov_net_sub = gtk_label_new("Rx: 0 | Tx: 0");
    gtk_widget_add_css_class(ctx->lbl_ov_net_sub, "info-list-label");
    gtk_widget_set_halign(ctx->lbl_ov_net_sub, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(net_vbox), ctx->lbl_ov_net_val);
    gtk_box_append(GTK_BOX(net_vbox), ctx->lbl_ov_net_sub);
    GtkWidget *card_ov_net = create_card("NETWORK THROUGHPUT", net_vbox, "network-theme");
    gtk_widget_set_size_request(card_ov_net, 240, 110);

    GtkWidget *card_ov_bat = make_ov_card("BATTERY STATE", "battery-theme", &ctx->lbl_ov_bat_val, &ctx->lbl_ov_bat_sub, &ctx->da_ov_bat, &ctx->bat_gauge);

    /* Attach cards to grid */
    gtk_grid_attach(GTK_GRID(page_dashboard), card_ov_cpu,   0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(page_dashboard), card_ov_mem,   1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(page_dashboard), card_ov_gpu,   2, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(page_dashboard), card_ov_net,   0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(page_dashboard), card_ov_store, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(page_dashboard), card_ov_bat,   2, 1, 1, 1);

    gtk_stack_add_named(GTK_STACK(ctx->stack), page_dashboard, "dashboard");

    /* --- PAGE 2: CPU DETAILS --- */
    GtkWidget *page_cpu = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(page_cpu, 12);
    gtk_widget_set_margin_end(page_cpu, 12);
    gtk_widget_set_margin_top(page_cpu, 12);
    gtk_widget_set_margin_bottom(page_cpu, 12);

    /* Left col: Large history graph */
    ctx->da_cpu_chart = gtk_drawing_area_new();
    gtk_widget_set_size_request(ctx->da_cpu_chart, 460, 320);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(ctx->da_cpu_chart), draw_history_chart, ctx->cpu_history, NULL);
    GtkWidget *card_cpu_graph = create_card("CPU UTILIZATION HISTORY (60s)", ctx->da_cpu_chart, "cpu-theme");
    gtk_box_append(GTK_BOX(page_cpu), card_cpu_graph);
    gtk_widget_set_hexpand(card_cpu_graph, TRUE);

    /* Right col: Stats & Cores list */
    GtkWidget *cpu_right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(cpu_right_box, 320, -1);
    gtk_box_append(GTK_BOX(page_cpu), cpu_right_box);

    /* CPU Specs card */
    GtkWidget *cpu_specs_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    ctx->lbl_cpu_model = gtk_label_new("Loading model...");
    gtk_widget_set_halign(ctx->lbl_cpu_model, GTK_ALIGN_START);
    ctx->lbl_cpu_speed = gtk_label_new("Speed: -- GHz");
    gtk_widget_set_halign(ctx->lbl_cpu_speed, GTK_ALIGN_START);
    ctx->lbl_cpu_temp = gtk_label_new("Core Temp: -- °C");
    gtk_widget_set_halign(ctx->lbl_cpu_temp, GTK_ALIGN_START);
    
    gtk_box_append(GTK_BOX(cpu_specs_box), ctx->lbl_cpu_model);
    gtk_box_append(GTK_BOX(cpu_specs_box), ctx->lbl_cpu_speed);
    gtk_box_append(GTK_BOX(cpu_specs_box), ctx->lbl_cpu_temp);
    gtk_box_append(GTK_BOX(cpu_right_box), create_card("SPECIFICATIONS", cpu_specs_box, NULL));

    /* Cores utilization card */
    GtkWidget *cores_scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(cores_scroll, -1, 220);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cores_scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    
    ctx->grid_cpu_cores = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(ctx->grid_cpu_cores), 6);
    gtk_grid_set_column_spacing(GTK_GRID(ctx->grid_cpu_cores), 12);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(cores_scroll), ctx->grid_cpu_cores);

    /* Setup initial core widgets based on detected core count */
    for (int i = 0; i < ctx->stats.cpu.core_count && i < MAX_CPU_CORES; i++) {
        char core_lbl[16];
        snprintf(core_lbl, sizeof(core_lbl), "Core %d", i);
        ctx->core_labels[i] = gtk_label_new(core_lbl);
        gtk_widget_add_css_class(ctx->core_labels[i], "info-list-label");
        gtk_widget_set_halign(ctx->core_labels[i], GTK_ALIGN_START);
        
        ctx->core_progress[i] = gtk_progress_bar_new();
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ctx->core_progress[i]), 0.0);
        gtk_widget_set_size_request(ctx->core_progress[i], 160, 6);
        gtk_widget_set_valign(ctx->core_progress[i], GTK_ALIGN_CENTER);
        
        gtk_grid_attach(GTK_GRID(ctx->grid_cpu_cores), ctx->core_labels[i], 0, i, 1, 1);
        gtk_grid_attach(GTK_GRID(ctx->grid_cpu_cores), ctx->core_progress[i], 1, i, 1, 1);
    }
    gtk_box_append(GTK_BOX(cpu_right_box), create_card("PER-CORE LOAD", cores_scroll, NULL));
    gtk_stack_add_named(GTK_STACK(ctx->stack), page_cpu, "cpu");

    /* --- PAGE 3: MEMORY DETAILS --- */
    GtkWidget *page_mem = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(page_mem, 12);
    gtk_widget_set_margin_end(page_mem, 12);
    gtk_widget_set_margin_top(page_mem, 12);
    gtk_widget_set_margin_bottom(page_mem, 12);

    /* Graph left */
    ctx->da_mem_chart = gtk_drawing_area_new();
    gtk_widget_set_size_request(ctx->da_mem_chart, 460, 320);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(ctx->da_mem_chart), draw_history_chart, ctx->mem_history, NULL);
    GtkWidget *card_mem_graph = create_card("MEMORY ALLOCATION HISTORY (60s)", ctx->da_mem_chart, "memory-theme");
    gtk_box_append(GTK_BOX(page_mem), card_mem_graph);
    gtk_widget_set_hexpand(card_mem_graph, TRUE);

    /* Details right */
    GtkWidget *mem_right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(mem_right_box, 320, -1);
    gtk_box_append(GTK_BOX(page_mem), mem_right_box);

    /* Radial gauge widget */
    ctx->da_mem_gauge = gtk_drawing_area_new();
    gtk_widget_set_size_request(ctx->da_mem_gauge, 150, 150);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(ctx->da_mem_gauge), draw_radial_gauge, &ctx->mem_gauge, NULL);
    gtk_box_append(GTK_BOX(mem_right_box), create_card("RAM UTILIZATION", ctx->da_mem_gauge, NULL));

    /* Data list */
    GtkWidget *mem_list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    void make_info_row(GtkWidget *parent, const char *label, GtkWidget **val_out) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_widget_add_css_class(row, "info-list-row");
        
        GtkWidget *lbl = gtk_label_new(label);
        gtk_widget_add_css_class(lbl, "info-list-label");
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(row), lbl);
        
        GtkWidget *spacer_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_append(GTK_BOX(row), spacer_row);
        gtk_widget_set_hexpand(spacer_row, TRUE);
        
        *val_out = gtk_label_new("--");
        gtk_widget_add_css_class(*val_out, "info-list-value");
        gtk_widget_set_halign(*val_out, GTK_ALIGN_END);
        gtk_box_append(GTK_BOX(row), *val_out);
        
        gtk_box_append(GTK_BOX(parent), row);
    }
    make_info_row(mem_list_box, "Total Physical RAM", &ctx->lbl_mem_avail);
    make_info_row(mem_list_box, "Used Memory", &ctx->lbl_mem_used);
    make_info_row(mem_list_box, "Active Memory", &ctx->lbl_mem_active);
    make_info_row(mem_list_box, "Cached & Buffers", &ctx->lbl_mem_cached);
    make_info_row(mem_list_box, "Uptime Swap File", &ctx->lbl_mem_swap);
    
    gtk_box_append(GTK_BOX(mem_right_box), create_card("DETAILS", mem_list_box, NULL));
    gtk_stack_add_named(GTK_STACK(ctx->stack), page_mem, "memory");

    /* --- PAGE 4: GPU DETAILS --- */
    GtkWidget *page_gpu = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(page_gpu, 12);
    gtk_widget_set_margin_end(page_gpu, 12);
    gtk_widget_set_margin_top(page_gpu, 12);
    gtk_widget_set_margin_bottom(page_gpu, 12);

    ctx->selected_gpu_idx = 0;

    /* Top Row: Sub-navigation selector for multiple GPUs */
    if (ctx->stats.gpu.gpu_count == 2) {
        ctx->box_gpu_selector = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_bottom(ctx->box_gpu_selector, 4);
        gtk_box_append(GTK_BOX(page_gpu), ctx->box_gpu_selector);

        void on_gpu_selector_toggled(GtkToggleButton *button, gpointer user_data) {
            UIContext *c = (UIContext *)user_data;
            if (gtk_toggle_button_get_active(button)) {
                int gpu_idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "gpu-idx"));
                c->selected_gpu_idx = gpu_idx;
                
                gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(c->da_gpu_chart), draw_history_chart, c->gpu_histories[gpu_idx], NULL);
                gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(c->da_gpu_gauge), draw_radial_gauge, &c->gpu_gauges[gpu_idx], NULL);
                
                gtk_widget_queue_draw(c->da_gpu_chart);
                gtk_widget_queue_draw(c->da_gpu_gauge);
                update_ui(c);
            }
        }

        for (int i = 0; i < 2; i++) {
            char tab_label[64];
            snprintf(tab_label, sizeof(tab_label), "GPU %d: %s", i, ctx->stats.gpu.gpus[i].brand);
            ctx->gpu_selector_btn[i] = gtk_toggle_button_new_with_label(tab_label);
            gtk_widget_add_css_class(ctx->gpu_selector_btn[i], "nav-btn");
            g_object_set_data(G_OBJECT(ctx->gpu_selector_btn[i]), "gpu-idx", GINT_TO_POINTER(i));
            
            if (i == 0) {
                gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ctx->gpu_selector_btn[i]), TRUE);
            } else {
                gtk_toggle_button_set_group(GTK_TOGGLE_BUTTON(ctx->gpu_selector_btn[i]), GTK_TOGGLE_BUTTON(ctx->gpu_selector_btn[0]));
            }
            
            g_signal_connect(ctx->gpu_selector_btn[i], "toggled", G_CALLBACK(on_gpu_selector_toggled), ctx);
            gtk_box_append(GTK_BOX(ctx->box_gpu_selector), ctx->gpu_selector_btn[i]);
        }
    }

    GtkWidget *gpu_content_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(page_gpu), gpu_content_hbox);

    /* Graph left */
    ctx->da_gpu_chart = gtk_drawing_area_new();
    gtk_widget_set_size_request(ctx->da_gpu_chart, 460, 320);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(ctx->da_gpu_chart), draw_history_chart, ctx->gpu_histories[0], NULL);
    GtkWidget *card_gpu_graph = create_card("GPU METRICS HISTORY (60s)", ctx->da_gpu_chart, "gpu-theme");
    gtk_box_append(GTK_BOX(gpu_content_hbox), card_gpu_graph);
    gtk_widget_set_hexpand(card_gpu_graph, TRUE);

    /* Details right */
    GtkWidget *gpu_right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(gpu_right_box, 320, -1);
    gtk_box_append(GTK_BOX(gpu_content_hbox), gpu_right_box);

    /* Gauge */
    ctx->da_gpu_gauge = gtk_drawing_area_new();
    gtk_widget_set_size_request(ctx->da_gpu_gauge, 150, 150);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(ctx->da_gpu_gauge), draw_radial_gauge, &ctx->gpu_gauges[0], NULL);
    gtk_box_append(GTK_BOX(gpu_right_box), create_card("GPU ENGINE LOAD", ctx->da_gpu_gauge, NULL));

    /* Info */
    GtkWidget *gpu_list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    make_info_row(gpu_list_box, "GPU Core Architecture", &ctx->lbl_gpu_model);
    make_info_row(gpu_list_box, "Core clock frequency", &ctx->lbl_gpu_clock);
    make_info_row(gpu_list_box, "GPU Active Core Load", &ctx->lbl_gpu_load);
    make_info_row(gpu_list_box, "Dedicated VRAM Alloc", &ctx->lbl_gpu_vram);
    make_info_row(gpu_list_box, "GPU Junction Temp", &ctx->lbl_gpu_temp);
    gtk_box_append(GTK_BOX(gpu_right_box), create_card("HARDWARE METRICS", gpu_list_box, NULL));
    gtk_stack_add_named(GTK_STACK(ctx->stack), page_gpu, "gpu");

    /* --- PAGE 5: NETWORK DETAILS --- */
    GtkWidget *page_net = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(page_net, 12);
    gtk_widget_set_margin_end(page_net, 12);
    gtk_widget_set_margin_top(page_net, 12);
    gtk_widget_set_margin_bottom(page_net, 12);

    /* Graph left */
    ctx->da_net_chart = gtk_drawing_area_new();
    gtk_widget_set_size_request(ctx->da_net_chart, 460, 320);
    /* Custom drawing function to show BOTH Rx (Pink) and Tx (Cyan) */
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(ctx->da_net_chart), draw_network_chart_cb, ctx, NULL);
    GtkWidget *card_net_graph = create_card("NETWORK TRANSFER RATES (60s) [Rx: Pink | Tx: Cyan]", ctx->da_net_chart, "network-theme");
    gtk_box_append(GTK_BOX(page_net), card_net_graph);
    gtk_widget_set_hexpand(card_net_graph, TRUE);

    /* Details right */
    GtkWidget *net_right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(net_right_box, 320, -1);
    gtk_box_append(GTK_BOX(page_net), net_right_box);

    /* Overview stats */
    GtkWidget *net_sum_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    make_info_row(net_sum_box, "Current Download (Rx)", &ctx->lbl_net_current_rx);
    make_info_row(net_sum_box, "Current Upload (Tx)", &ctx->lbl_net_current_tx);
    make_info_row(net_sum_box, "Total Received Data", &ctx->lbl_net_total_rx);
    make_info_row(net_sum_box, "Total Transmitted Data", &ctx->lbl_net_total_tx);
    gtk_box_append(GTK_BOX(net_right_box), create_card("INTEGRATED SPEEDS", net_sum_box, NULL));

    /* Interface list */
    ctx->box_net_interfaces = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *net_scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(net_scroll, -1, 160);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(net_scroll), ctx->box_net_interfaces);
    gtk_box_append(GTK_BOX(net_right_box), create_card("ADAPTER DETAILS", net_scroll, NULL));
    
    gtk_stack_add_named(GTK_STACK(ctx->stack), page_net, "network");

    /* --- PAGE 6: STORAGE DETAILS --- */
    GtkWidget *page_store = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(page_store, 12);
    gtk_widget_set_margin_end(page_store, 12);
    gtk_widget_set_margin_top(page_store, 12);
    gtk_widget_set_margin_bottom(page_store, 12);

    /* Graph left */
    ctx->da_store_chart = gtk_drawing_area_new();
    gtk_widget_set_size_request(ctx->da_store_chart, 460, 320);
    /* Custom draw showing Read (Yellow) and Write (Orange) */
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(ctx->da_store_chart), draw_storage_chart_cb, ctx, NULL);
    GtkWidget *card_store_graph = create_card("DISK I/O throughput (60s) [Read: Yellow | Write: Orange]", ctx->da_store_chart, "storage-theme");
    gtk_box_append(GTK_BOX(page_store), card_store_graph);
    gtk_widget_set_hexpand(card_store_graph, TRUE);

    /* Details right */
    GtkWidget *store_right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(store_right_box, 320, -1);
    gtk_box_append(GTK_BOX(page_store), store_right_box);

    /* Rates card */
    GtkWidget *store_rates_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    make_info_row(store_rates_box, "Read Throughput", &ctx->lbl_store_io_read);
    make_info_row(store_rates_box, "Write Throughput", &ctx->lbl_store_io_write);
    gtk_box_append(GTK_BOX(store_right_box), create_card("ACTIVE HARDWARE SPEEDS", store_rates_box, NULL));

    /* Mount list */
    ctx->box_store_mounts = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *store_scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(store_scroll, -1, 200);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(store_scroll), ctx->box_store_mounts);
    gtk_box_append(GTK_BOX(store_right_box), create_card("MOUNTED DEV PARTITIONS", store_scroll, NULL));
    
    gtk_stack_add_named(GTK_STACK(ctx->stack), page_store, "storage");

    /* --- PAGE 7: BATTERY DETAILS --- */
    GtkWidget *page_bat = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(page_bat, 12);
    gtk_widget_set_margin_end(page_bat, 12);
    gtk_widget_set_margin_top(page_bat, 12);
    gtk_widget_set_margin_bottom(page_bat, 12);

    /* Gauge left (Liquid wave indicator) */
    ctx->da_bat_gauge = gtk_drawing_area_new();
    gtk_widget_set_size_request(ctx->da_bat_gauge, 240, 240);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(ctx->da_bat_gauge), draw_liquid_battery_cb, ctx, NULL);
    GtkWidget *card_bat_liquid = create_card("LIVE LIQUID DYNAMICS", ctx->da_bat_gauge, "battery-theme");
    gtk_box_append(GTK_BOX(page_bat), card_bat_liquid);
    gtk_widget_set_hexpand(card_bat_liquid, TRUE);

    /* Stats middle/right */
    GtkWidget *bat_mid_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(bat_mid_box, 320, -1);
    gtk_box_append(GTK_BOX(page_bat), bat_mid_box);

    GtkWidget *bat_stats_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    make_info_row(bat_stats_box, "Battery Health", &ctx->lbl_bat_health);
    make_info_row(bat_stats_box, "Status", &ctx->lbl_bat_status);
    make_info_row(bat_stats_box, "Terminal Voltage", &ctx->lbl_bat_voltage);
    make_info_row(bat_stats_box, "Discharge/Charge Rate", &ctx->lbl_bat_rate);
    make_info_row(bat_stats_box, "Battery Temp", &ctx->lbl_bat_temp);
    make_info_row(bat_stats_box, "Estimated Time Remaining", &ctx->lbl_bat_remaining);
    gtk_box_append(GTK_BOX(bat_mid_box), create_card("HEALTH & TELEMETRY", bat_stats_box, NULL));

    /* History graph */
    ctx->da_bat_chart = gtk_drawing_area_new();
    gtk_widget_set_size_request(ctx->da_bat_chart, 200, 120);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(ctx->da_bat_chart), draw_history_chart, ctx->bat_history, NULL);
    gtk_box_append(GTK_BOX(bat_mid_box), create_card("CHARGE RETENTION (60s)", ctx->da_bat_chart, NULL));

    gtk_stack_add_named(GTK_STACK(ctx->stack), page_bat, "battery");

    /* --- PAGE 8: PROCESS RESOURCES --- */
    GtkWidget *page_processes = create_process_view(win, ctx);
    gtk_stack_add_named(GTK_STACK(ctx->stack), page_processes, "processes");

    /* Show window */
    gtk_window_present(GTK_WINDOW(win));
    
    return win;
}

/* Update Function for UI widgets */
void update_ui(UIContext *ctx) {

    /* Step 2: Feed history charts and radial gauges */
    chart_history_add_point(ctx->cpu_history, ctx->stats.cpu.total_usage);
    chart_history_add_point(ctx->mem_history, ctx->stats.memory.ram_percent);
    for (int i = 0; i < ctx->stats.gpu.gpu_count; i++) {
        chart_history_add_point(ctx->gpu_histories[i], ctx->stats.gpu.gpus[i].usage_percent);
    }
    chart_history_add_point(ctx->net_rx_history, ctx->stats.network.total_rx_speed_kbps);
    chart_history_add_point(ctx->net_tx_history, ctx->stats.network.total_tx_speed_kbps);
    chart_history_add_point(ctx->disk_r_history, ctx->stats.storage.read_speed_mbps);
    chart_history_add_point(ctx->disk_w_history, ctx->stats.storage.write_speed_mbps);
    
    if (ctx->stats.battery.present) {
        chart_history_add_point(ctx->bat_history, ctx->stats.battery.charge_percent);
    } else {
        chart_history_add_point(ctx->bat_history, 0.0);
    }

    ctx->cpu_gauge.value = ctx->stats.cpu.total_usage;
    ctx->mem_gauge.value = ctx->stats.memory.ram_percent;
    for (int i = 0; i < ctx->stats.gpu.gpu_count; i++) {
        ctx->gpu_gauges[i].value = ctx->stats.gpu.gpus[i].usage_percent;
    }
    ctx->bat_gauge.value = ctx->stats.battery.present ? ctx->stats.battery.charge_percent : 0.0;

    /* Step 3: Trigger redraws on detail charts */
    if (ctx->da_cpu_chart) gtk_widget_queue_draw(ctx->da_cpu_chart);
    if (ctx->da_mem_chart) gtk_widget_queue_draw(ctx->da_mem_chart);
    if (ctx->da_gpu_chart) gtk_widget_queue_draw(ctx->da_gpu_chart);
    if (ctx->da_net_chart) gtk_widget_queue_draw(ctx->da_net_chart);
    if (ctx->da_store_chart) gtk_widget_queue_draw(ctx->da_store_chart);
    if (ctx->da_bat_chart) gtk_widget_queue_draw(ctx->da_bat_chart);

    /* Trigger redraws on overview gauges */
    if (ctx->da_ov_cpu) gtk_widget_queue_draw(ctx->da_ov_cpu);
    if (ctx->da_ov_mem) gtk_widget_queue_draw(ctx->da_ov_mem);
    for (int i = 0; i < ctx->stats.gpu.gpu_count; i++) {
        if (ctx->da_ov_gpu[i]) gtk_widget_queue_draw(ctx->da_ov_gpu[i]);
    }
    if (ctx->da_ov_bat) gtk_widget_queue_draw(ctx->da_ov_bat);

    if (ctx->da_mem_gauge) gtk_widget_queue_draw(ctx->da_mem_gauge);
    for (int i = 0; i < ctx->stats.gpu.gpu_count; i++) {
        if (ctx->da_gpu_gauge) gtk_widget_queue_draw(ctx->da_gpu_gauge);
    }

    /* Step 4: Update Dashboard Texts */
    char buf[128];
    
    /* CPU dashboard */
    snprintf(buf, sizeof(buf), "%.0f%%", ctx->stats.cpu.total_usage);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_ov_cpu_val), buf);
    snprintf(buf, sizeof(buf), "%.2f GHz | %.1f°C", ctx->stats.cpu.frequency_ghz, ctx->stats.cpu.temperature_c);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_ov_cpu_sub), buf);

    /* Memory dashboard */
    snprintf(buf, sizeof(buf), "%.1f GB", ctx->stats.memory.used_ram_gb);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_ov_mem_val), buf);
    snprintf(buf, sizeof(buf), "Active: %.1f GB | Cached: %.1f GB", ctx->stats.memory.active_ram_gb, ctx->stats.memory.cached_ram_gb);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_ov_mem_sub), buf);

    /* GPU dashboard */
    for (int i = 0; i < ctx->stats.gpu.gpu_count; i++) {
        if (ctx->lbl_ov_gpu_val[i]) {
            snprintf(buf, sizeof(buf), "%.0f%%", ctx->stats.gpu.gpus[i].usage_percent);
            if (ctx->stats.gpu.gpu_count == 2) {
                char mark_buf[256];
                snprintf(mark_buf, sizeof(mark_buf), "<span font='16' weight='bold'>%s</span>", buf);
                gtk_label_set_markup(GTK_LABEL(ctx->lbl_ov_gpu_val[i]), mark_buf);
            } else {
                gtk_label_set_text(GTK_LABEL(ctx->lbl_ov_gpu_val[i]), buf);
            }
        }
        if (ctx->lbl_ov_gpu_sub[i]) {
            snprintf(buf, sizeof(buf), "VRAM: %.1fG/%.0fG", ctx->stats.gpu.gpus[i].used_vram_gb, ctx->stats.gpu.gpus[i].total_vram_gb);
            gtk_label_set_text(GTK_LABEL(ctx->lbl_ov_gpu_sub[i]), buf);
        }
    }

    /* Network dashboard */
    if (ctx->stats.network.total_rx_speed_kbps > 1000.0) {
        snprintf(buf, sizeof(buf), "%.1f Mbps", ctx->stats.network.total_rx_speed_kbps / 1000.0);
    } else {
        snprintf(buf, sizeof(buf), "%.0f Kbps", ctx->stats.network.total_rx_speed_kbps);
    }
    gtk_label_set_text(GTK_LABEL(ctx->lbl_ov_net_val), buf);
    snprintf(buf, sizeof(buf), "Rx: %.0f Kbps | Tx: %.0f Kbps", ctx->stats.network.total_rx_speed_kbps, ctx->stats.network.total_tx_speed_kbps);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_ov_net_sub), buf);

    /* Storage dashboard */
    snprintf(buf, sizeof(buf), "%.0f%%", ctx->stats.storage.storage_percent);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_ov_store_val), buf);
    snprintf(buf, sizeof(buf), "Used: %.0f GB | Total: %.0f GB", ctx->stats.storage.used_storage_gb, ctx->stats.storage.total_storage_gb);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_ov_store_sub), buf);

    /* Battery dashboard */
    if (ctx->stats.battery.present) {
        snprintf(buf, sizeof(buf), "%.0f%%", ctx->stats.battery.charge_percent);
        gtk_label_set_text(GTK_LABEL(ctx->lbl_ov_bat_val), buf);
        snprintf(buf, sizeof(buf), "Status: %s", ctx->stats.battery.status);
    } else {
        gtk_label_set_text(GTK_LABEL(ctx->lbl_ov_bat_val), "N/A");
        gtk_label_set_text(GTK_LABEL(ctx->lbl_ov_bat_sub), "No Battery Found");
    }

    /* Step 5: Update Detail Views */
    /* CPU details */
    gtk_label_set_text(GTK_LABEL(ctx->lbl_cpu_model), ctx->stats.cpu.model);
    snprintf(buf, sizeof(buf), "Active Clock Frequency: %.3f GHz", ctx->stats.cpu.frequency_ghz);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_cpu_speed), buf);
    snprintf(buf, sizeof(buf), "Hardware Temperature: %.1f °C", ctx->stats.cpu.temperature_c);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_cpu_temp), buf);

    for (int i = 0; i < ctx->stats.cpu.core_count && i < MAX_CPU_CORES; i++) {
        if (ctx->core_progress[i]) {
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(ctx->core_progress[i]), ctx->stats.cpu.core_usage[i] / 100.0);
            char core_txt[32];
            snprintf(core_txt, sizeof(core_txt), "Core %d: %.0f%%", i, ctx->stats.cpu.core_usage[i]);
            gtk_label_set_text(GTK_LABEL(ctx->core_labels[i]), core_txt);
        }
    }

    /* Memory details */
    snprintf(buf, sizeof(buf), "%.3f GB", ctx->stats.memory.total_ram_gb);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_mem_avail), buf);
    snprintf(buf, sizeof(buf), "%.3f GB (%.1f%%)", ctx->stats.memory.used_ram_gb, ctx->stats.memory.ram_percent);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_mem_used), buf);
    snprintf(buf, sizeof(buf), "%.3f GB", ctx->stats.memory.active_ram_gb);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_mem_active), buf);
    snprintf(buf, sizeof(buf), "%.3f GB", ctx->stats.memory.cached_ram_gb + ctx->stats.memory.buffers_ram_gb);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_mem_cached), buf);
    snprintf(buf, sizeof(buf), "%.3f GB (%.1f%%)", ctx->stats.memory.used_swap_gb, ctx->stats.memory.swap_percent);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_mem_swap), buf);

    /* GPU details */
    int active_gpu = ctx->selected_gpu_idx;
    gtk_label_set_text(GTK_LABEL(ctx->lbl_gpu_model), ctx->stats.gpu.gpus[active_gpu].model);
    snprintf(buf, sizeof(buf), "%.1f MHz", ctx->stats.gpu.gpus[active_gpu].core_clock_mhz);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_gpu_clock), buf);
    snprintf(buf, sizeof(buf), "%.1f%%", ctx->stats.gpu.gpus[active_gpu].usage_percent);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_gpu_load), buf);
    snprintf(buf, sizeof(buf), "%.2f GB / %.2f GB (%.1f%%)", ctx->stats.gpu.gpus[active_gpu].used_vram_gb, ctx->stats.gpu.gpus[active_gpu].total_vram_gb, ctx->stats.gpu.gpus[active_gpu].vram_percent);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_gpu_vram), buf);
    snprintf(buf, sizeof(buf), "%.1f °C", ctx->stats.gpu.gpus[active_gpu].temperature_c);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_gpu_temp), buf);

    /* Network details */
    snprintf(buf, sizeof(buf), "%.2f MB", ctx->stats.network.interfaces[0].total_rx_bytes / 1024.0 / 1024.0);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_net_total_rx), buf);
    snprintf(buf, sizeof(buf), "%.2f MB", ctx->stats.network.interfaces[0].total_tx_bytes / 1024.0 / 1024.0);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_net_total_tx), buf);
    snprintf(buf, sizeof(buf), "%.2f Kbps", ctx->stats.network.total_rx_speed_kbps);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_net_current_rx), buf);
    snprintf(buf, sizeof(buf), "%.2f Kbps", ctx->stats.network.total_tx_speed_kbps);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_net_current_tx), buf);

    /* Build interface list inside details view */
    /* Clean first */
    GtkWidget *child = gtk_widget_get_first_child(ctx->box_net_interfaces);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(ctx->box_net_interfaces), child);
        child = next;
    }
    
    for (int i = 0; i < ctx->stats.network.interface_count; i++) {
        NetInterface *ni = &ctx->stats.network.interfaces[i];
        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
        gtk_widget_add_css_class(hbox, "info-list-row");
        
        GtkWidget *lbl_name = gtk_label_new(ni->name);
        gtk_widget_add_css_class(lbl_name, "info-list-label");
        gtk_widget_set_halign(lbl_name, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(hbox), lbl_name);
        
        GtkWidget *lbl_ip = gtk_label_new(ni->ip_address);
        gtk_widget_add_css_class(lbl_ip, "info-list-label");
        gtk_widget_set_halign(lbl_ip, GTK_ALIGN_CENTER);
        gtk_widget_set_hexpand(lbl_ip, TRUE);
        gtk_box_append(GTK_BOX(hbox), lbl_ip);

        char rx_tx_txt[64];
        snprintf(rx_tx_txt, sizeof(rx_tx_txt), "Rx: %.0f | Tx: %.0f Kbps", ni->rx_speed_kbps, ni->tx_speed_kbps);
        GtkWidget *lbl_speed = gtk_label_new(rx_tx_txt);
        gtk_widget_add_css_class(lbl_speed, "info-list-value");
        gtk_widget_set_halign(lbl_speed, GTK_ALIGN_END);
        gtk_box_append(GTK_BOX(hbox), lbl_speed);

        gtk_box_append(GTK_BOX(ctx->box_net_interfaces), hbox);
    }

    /* Storage details */
    snprintf(buf, sizeof(buf), "%.2f MB/s", ctx->stats.storage.read_speed_mbps);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_store_io_read), buf);
    snprintf(buf, sizeof(buf), "%.2f MB/s", ctx->stats.storage.write_speed_mbps);
    gtk_label_set_text(GTK_LABEL(ctx->lbl_store_io_write), buf);

    /* Build storage mounts list inside details view */
    child = gtk_widget_get_first_child(ctx->box_store_mounts);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(ctx->box_store_mounts), child);
        child = next;
    }
    
    for (int i = 0; i < ctx->stats.storage.mount_count; i++) {
        StorageMount *sm = &ctx->stats.storage.mounts[i];
        
        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_widget_add_css_class(vbox, "info-list-row");
        
        GtkWidget *row1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        GtkWidget *lbl_mnt = gtk_label_new(sm->mount_point);
        gtk_widget_add_css_class(lbl_mnt, "info-list-value");
        gtk_widget_set_halign(lbl_mnt, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(row1), lbl_mnt);
        
        GtkWidget *spacer_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
        gtk_box_append(GTK_BOX(row1), spacer_row);
        gtk_widget_set_hexpand(spacer_row, TRUE);
        
        char cap_txt[64];
        snprintf(cap_txt, sizeof(cap_txt), "%.1f / %.1f GB (%.0f%%)", sm->used_gb, sm->total_gb, sm->usage_percent);
        GtkWidget *lbl_cap = gtk_label_new(cap_txt);
        gtk_widget_add_css_class(lbl_cap, "info-list-label");
        gtk_widget_set_halign(lbl_cap, GTK_ALIGN_END);
        gtk_box_append(GTK_BOX(row1), lbl_cap);
        
        gtk_box_append(GTK_BOX(vbox), row1);
        
        /* Add a utilization progress bar */
        GtkWidget *pbar = gtk_progress_bar_new();
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pbar), sm->usage_percent / 100.0);
        gtk_widget_set_size_request(pbar, -1, 4);
        gtk_box_append(GTK_BOX(vbox), pbar);
        
        gtk_box_append(GTK_BOX(ctx->box_store_mounts), vbox);
    }

    /* Battery details */
    if (ctx->stats.battery.present) {
        gtk_label_set_text(GTK_LABEL(ctx->lbl_bat_status), ctx->stats.battery.status);
        
        snprintf(buf, sizeof(buf), "%.1f%%", ctx->stats.battery.health_percent);
        gtk_label_set_text(GTK_LABEL(ctx->lbl_bat_health), buf);
        
        snprintf(buf, sizeof(buf), "%.2f V", ctx->stats.battery.voltage_v);
        gtk_label_set_text(GTK_LABEL(ctx->lbl_bat_voltage), buf);
        
        snprintf(buf, sizeof(buf), "%.2f W", ctx->stats.battery.energy_rate_w);
        gtk_label_set_text(GTK_LABEL(ctx->lbl_bat_rate), buf);
        
        snprintf(buf, sizeof(buf), "%.1f °C", ctx->stats.battery.temp_c);
        gtk_label_set_text(GTK_LABEL(ctx->lbl_bat_temp), buf);
        
        if (strcmp(ctx->stats.battery.status, "Charging") == 0) {
            if (ctx->stats.battery.time_to_full_min > 0) {
                snprintf(buf, sizeof(buf), "%d min to full charge", ctx->stats.battery.time_to_full_min);
            } else {
                strcpy(buf, "Charging (Calculating time...)");
            }
        } else if (strcmp(ctx->stats.battery.status, "Full") == 0) {
            strcpy(buf, "Battery Full");
        } else {
            if (ctx->stats.battery.time_to_empty_min > 0) {
                snprintf(buf, sizeof(buf), "%d hours %d min remaining", 
                         ctx->stats.battery.time_to_empty_min / 60,
                         ctx->stats.battery.time_to_empty_min % 60);
            } else {
                strcpy(buf, "Discharging (Calculating time...)");
            }
        }
        gtk_label_set_text(GTK_LABEL(ctx->lbl_bat_remaining), buf);
        
        snprintf(buf, sizeof(buf), "Status: %s (%.0f%%)", ctx->stats.battery.status, ctx->stats.battery.charge_percent);
        gtk_label_set_text(GTK_LABEL(ctx->lbl_ov_bat_sub), buf);
    } else {
        gtk_label_set_text(GTK_LABEL(ctx->lbl_bat_status), "No Battery Found");
        gtk_label_set_text(GTK_LABEL(ctx->lbl_bat_health), "N/A");
        gtk_label_set_text(GTK_LABEL(ctx->lbl_bat_voltage), "-- V");
        gtk_label_set_text(GTK_LABEL(ctx->lbl_bat_rate), "-- W");
        gtk_label_set_text(GTK_LABEL(ctx->lbl_bat_temp), "-- °C");
        gtk_label_set_text(GTK_LABEL(ctx->lbl_bat_remaining), "Running on AC Power Source");
    }

    /* Update App Resources telemetry grid */
    update_process_view(ctx);
}

void ui_animate_tick(UIContext *ctx) {
    g_wave_phase += 0.08; /* smooth liquid wave animation */
    if (g_wave_phase > 2.0 * M_PI) {
        g_wave_phase -= 2.0 * M_PI;
    }
    if (ctx->da_bat_gauge) {
        gtk_widget_queue_draw(ctx->da_bat_gauge);
    }
}
