#include "process_view.h"
#include "ui.h"
#include "process_actions.h"
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>

enum {
    COL_NAME = 0,
    COL_ICON_NAME,
    COL_PID,
    COL_CPU,
    COL_RAM,
    COL_REAL_MEM,
    COL_GPU,
    COL_CACHE,
    NUM_COLS
};

static const char *get_process_icon_name(const char *proc_name) {
    if (!proc_name) return NULL;

    /* Lowercase lookup to match easily */
    char lower[256];
    int i = 0;
    for (i = 0; proc_name[i] && i < 255; i++) {
        lower[i] = tolower((unsigned char)proc_name[i]);
    }
    lower[i] = '\0';

    /* Custom mappings for common applications/services */
    if (strstr(lower, "bash") || strstr(lower, "sh") || strstr(lower, "zsh") || strstr(lower, "terminal")) {
        return "utilities-terminal";
    }
    if (strstr(lower, "firefox")) {
        return "firefox";
    }
    if (strstr(lower, "chrome") || strstr(lower, "chromium")) {
        return "google-chrome";
    }
    if (strstr(lower, "vbrowser")) {
        return "web-browser";
    }
    if (strstr(lower, "vresources") || strstr(lower, "system-monitor") || strstr(lower, "top")) {
        return "utilities-system-monitor";
    }
    if (strstr(lower, "pulseaudio") || strstr(lower, "pipewire")) {
        return "audio-card";
    }
    if (strstr(lower, "xorg") || strstr(lower, "wayland") || strstr(lower, "gnome-shell") || strstr(lower, "mutter")) {
        return "video-display";
    }
    if (strstr(lower, "settings") || strstr(lower, "control-center")) {
        return "preferences-system";
    }
    if (strstr(lower, "dbus") || strstr(lower, "systemd")) {
        return "system-run";
    }
    if (strstr(lower, "git")) {
        return "git";
    }
    if (strstr(lower, "python")) {
        return "python";
    }

    /* Check if the icon theme has an icon matching the exact process name */
    GdkDisplay *display = gdk_display_get_default();
    if (display) {
        GtkIconTheme *theme = gtk_icon_theme_get_for_display(display);
        if (theme && gtk_icon_theme_has_icon(theme, lower)) {
            static char matched_icon[256];
            strncpy(matched_icon, lower, sizeof(matched_icon) - 1);
            matched_icon[sizeof(matched_icon) - 1] = '\0';
            return matched_icon;
        }
        if (theme && gtk_icon_theme_has_icon(theme, proc_name)) {
            static char matched_icon_orig[256];
            strncpy(matched_icon_orig, proc_name, sizeof(matched_icon_orig) - 1);
            matched_icon_orig[sizeof(matched_icon_orig) - 1] = '\0';
            return matched_icon_orig;
        }
    }

    return NULL;
}

typedef struct {
    GtkListStore *store;
    GtkWidget *tree_view;
    GtkWidget *search_entry;
    GtkTreeModel *filter_model;
    ProcessList list;
    GtkWindow *parent_window;
    GtkWidget *switch_all;
} ProcessViewState;

/* Case-insensitive search filter function */
static gboolean process_search_filter_func(GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
    GtkSearchEntry *entry = GTK_SEARCH_ENTRY(data);
    const char *search_text = gtk_editable_get_text(GTK_EDITABLE(entry));
    
    if (!search_text || strlen(search_text) == 0) {
        return TRUE; /* show all if search is empty */
    }

    char *proc_name = NULL;
    gtk_tree_model_get(model, iter, COL_NAME, &proc_name, -1);
    
    if (!proc_name) return FALSE;

    /* Perform case-insensitive search substring check */
    gboolean visible = FALSE;
    char *proc_lower = g_utf8_strdown(proc_name, -1);
    char *search_lower = g_utf8_strdown(search_text, -1);

    if (strstr(proc_lower, search_lower) != NULL) {
        visible = TRUE;
    }

    g_free(proc_lower);
    g_free(search_lower);
    g_free(proc_name);

    return visible;
}

/* Callbacks to format double numbers in columns with proper units */
static void format_cpu_cell(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
    (void)col; (void)data;
    double val;
    gtk_tree_model_get(model, iter, COL_CPU, &val, -1);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f %%", val);
    g_object_set(renderer, "text", buf, NULL);
}

static void format_ram_cell(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
    (void)col; (void)data;
    double val;
    gtk_tree_model_get(model, iter, COL_RAM, &val, -1);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f MB", val);
    g_object_set(renderer, "text", buf, NULL);
}

static void format_real_mem_cell(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
    (void)col; (void)data;
    double val;
    gtk_tree_model_get(model, iter, COL_REAL_MEM, &val, -1);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f MB", val);
    g_object_set(renderer, "text", buf, NULL);
}

static void format_gpu_cell(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
    (void)col; (void)data;
    double val;
    gtk_tree_model_get(model, iter, COL_GPU, &val, -1);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f %%", val);
    g_object_set(renderer, "text", buf, NULL);
}

static void format_cache_cell(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
    (void)col; (void)data;
    double val;
    gtk_tree_model_get(model, iter, COL_CACHE, &val, -1);
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f MB", val);
    g_object_set(renderer, "text", buf, NULL);
}

/* Callback triggered when user types in the search box */
static void on_search_changed(GtkSearchEntry *entry, gpointer user_data) {
    GtkTreeModelFilter *filter = GTK_TREE_MODEL_FILTER(user_data);
    (void)entry;
    gtk_tree_model_filter_refilter(filter);
}

/* Callback structures and helpers for the context menu */
typedef struct {
    int pid;
    char name[256];
    double cpu_percent;
    double ram_mb;
    double real_mem_mb;
    double gpu_percent;
    double cache_mb;
    GtkWidget *popover;
    GtkWindow *parent_win;
} ContextMenuCallbackData;

static void on_popover_destroy(GtkWidget *popover, gpointer user_data) {
    (void)popover;
    ContextMenuCallbackData *data = (ContextMenuCallbackData *)user_data;
    g_free(data);
}

static void on_menu_action_clicked(GtkWidget *btn, gpointer user_data) {
    ContextMenuCallbackData *data = (ContextMenuCallbackData *)user_data;
    
    /* Safely fetch and copy action name before popping down popover (which destroys button data) */
    const char *action_val = g_object_get_data(G_OBJECT(btn), "action");
    char action[64] = "";
    if (action_val) {
        strncpy(action, action_val, sizeof(action) - 1);
    }
    
    /* Capture all required data into local variables to prevent Use-After-Free when popover is destroyed */
    int target_pid = data->pid;
    char target_name[256];
    strncpy(target_name, data->name, sizeof(target_name) - 1);
    target_name[sizeof(target_name) - 1] = '\0';
    
    GtkWindow *parent_win = data->parent_win;
    double cpu_percent = data->cpu_percent;
    double ram_mb = data->ram_mb;
    double real_mem_mb = data->real_mem_mb;
    double gpu_percent = data->gpu_percent;
    double cache_mb = data->cache_mb;
    
    /* Pop down the popover, which triggers closed -> unparent -> destroy -> on_popover_destroy (g_free(data)) */
    gtk_popover_popdown(GTK_POPOVER(data->popover));
    
    if (strcmp(action, "properties") == 0) {
        process_action_properties(parent_win, target_pid, target_name, cpu_percent, ram_mb, real_mem_mb, gpu_percent, cache_mb);
    } else if (strcmp(action, "maps") == 0) {
        process_action_memory_map(parent_win, target_pid, target_name);
    } else if (strcmp(action, "fd") == 0) {
        process_action_open_files(parent_win, target_pid, target_name);
    } else if (strcmp(action, "priority") == 0) {
        process_action_change_priority(parent_win, target_pid, target_name);
    } else if (strcmp(action, "affinity") == 0) {
        process_action_set_affinity(parent_win, target_pid, target_name);
    } else if (strcmp(action, "stop") == 0) {
        process_action_send_signal(parent_win, target_pid, target_name, SIGSTOP);
    } else if (strcmp(action, "continue") == 0) {
        process_action_send_signal(parent_win, target_pid, target_name, SIGCONT);
    } else if (strcmp(action, "terminate") == 0) {
        process_action_send_signal(parent_win, target_pid, target_name, SIGTERM);
    } else if (strcmp(action, "kill") == 0) {
        process_action_send_signal(parent_win, target_pid, target_name, SIGKILL);
    }
}

static void show_process_context_menu(ProcessViewState *state, int pid, const char *name, double cpu, double ram, double real_mem, double gpu, double cache, double x, double y) {
    GtkWidget *popover = gtk_popover_new();
    gtk_widget_set_parent(popover, state->tree_view);
    
    /* Remove bubble arrow tail to make it a clean, official rectangular floating context menu */
    gtk_popover_set_has_arrow(GTK_POPOVER(popover), FALSE);
    gtk_widget_add_css_class(popover, "context-menu-popover");
    
    /* Destroy and free callback data when the popover is dismissed */
    g_signal_connect(popover, "closed", G_CALLBACK(gtk_widget_unparent), NULL);
    
    ContextMenuCallbackData *data = g_new0(ContextMenuCallbackData, 1);
    data->pid = pid;
    strncpy(data->name, name, sizeof(data->name) - 1);
    data->cpu_percent = cpu;
    data->ram_mb = ram;
    data->real_mem_mb = real_mem;
    data->gpu_percent = gpu;
    data->cache_mb = cache;
    data->popover = popover;
    data->parent_win = state->parent_window;
    g_signal_connect(popover, "destroy", G_CALLBACK(on_popover_destroy), data);
    
    GdkRectangle rect;
    rect.x = (int)x;
    rect.y = (int)y;
    rect.width = 1;
    rect.height = 1;
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    gtk_widget_set_margin_start(vbox, 4);
    gtk_widget_set_margin_end(vbox, 4);
    gtk_widget_set_margin_top(vbox, 4);
    gtk_widget_set_margin_bottom(vbox, 4);
    gtk_popover_set_child(GTK_POPOVER(popover), vbox);
    
    /* Compact header displaying name and PID */
    char header[256];
    snprintf(header, sizeof(header), "%s (PID %d)", name, pid);
    GtkWidget *lbl_header = gtk_label_new(header);
    gtk_widget_add_css_class(lbl_header, "info-list-label");
    gtk_widget_set_halign(lbl_header, GTK_ALIGN_START);
    gtk_widget_set_margin_start(lbl_header, 8);
    gtk_widget_set_margin_bottom(lbl_header, 4);
    gtk_box_append(GTK_BOX(vbox), lbl_header);
    
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top(sep, 2);
    gtk_widget_set_margin_bottom(sep, 4);
    gtk_box_append(GTK_BOX(vbox), sep);
    
    #define ADD_MENU_BUTTON(lbl, act, theme) { \
        GtkWidget *btn = gtk_button_new_with_label(lbl); \
        gtk_widget_add_css_class(btn, "context-menu-btn"); \
        if (theme) gtk_widget_add_css_class(btn, theme); \
        GtkWidget *child = gtk_button_get_child(GTK_BUTTON(btn)); \
        if (GTK_IS_LABEL(child)) { \
            gtk_widget_set_halign(child, GTK_ALIGN_START); \
        } \
        g_object_set_data(G_OBJECT(btn), "action", (gpointer)act); \
        g_signal_connect(btn, "clicked", G_CALLBACK(on_menu_action_clicked), data); \
        gtk_box_append(GTK_BOX(vbox), btn); \
    }
    
    ADD_MENU_BUTTON("Properties", "properties", NULL);
    ADD_MENU_BUTTON("Memory Map", "maps", NULL);
    ADD_MENU_BUTTON("Open Files", "fd", NULL);
    ADD_MENU_BUTTON("Change Priority", "priority", NULL);
    ADD_MENU_BUTTON("Set Affinity", "affinity", NULL);
    
    sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top(sep, 4);
    gtk_widget_set_margin_bottom(sep, 4);
    gtk_box_append(GTK_BOX(vbox), sep);
    
    ADD_MENU_BUTTON("Stop (Pause)", "stop", "warn-theme");
    ADD_MENU_BUTTON("Continue (Resume)", "continue", "accent-theme");
    ADD_MENU_BUTTON("Terminate", "terminate", "warn-theme");
    ADD_MENU_BUTTON("Kill Process (Force)", "kill", "destruct-theme");
    
    #undef ADD_MENU_BUTTON
    
    gtk_popover_popup(GTK_POPOVER(popover));
}

static void on_treeview_right_click(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
    (void)n_press;
    ProcessViewState *state = (ProcessViewState *)user_data;
    GtkTreeView *treeview = GTK_TREE_VIEW(state->tree_view);
    
    GtkTreePath *path = NULL;
    GtkTreeViewColumn *column = NULL;
    int cell_x, cell_y;
    
    if (gtk_tree_view_get_path_at_pos(treeview, (int)x, (int)y, &path, &column, &cell_x, &cell_y)) {
        GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
        gtk_tree_selection_select_path(selection, path);
        
        GtkTreeModel *model;
        GtkTreeIter iter;
        if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
            int pid;
            char *name = NULL;
            double cpu = 0.0, ram = 0.0, real_mem = 0.0, gpu = 0.0, cache = 0.0;
            gtk_tree_model_get(model, &iter, 
                               COL_PID, &pid, 
                               COL_NAME, &name, 
                               COL_CPU, &cpu,
                               COL_RAM, &ram,
                               COL_REAL_MEM, &real_mem,
                               COL_GPU, &gpu,
                               COL_CACHE, &cache,
                               -1);
            
            char name_buf[256];
            if (name) {
                strncpy(name_buf, name, sizeof(name_buf) - 1);
                name_buf[sizeof(name_buf) - 1] = '\0';
                g_free(name);
            } else {
                strcpy(name_buf, "Unknown");
            }
            
            show_process_context_menu(state, pid, name_buf, cpu, ram, real_mem, gpu, cache, x, y);
        }
        gtk_tree_path_free(path);
    }
}

static void on_switch_all_notify(GObject *gobject, GParamSpec *pspec, gpointer user_data) {
    (void)gobject; (void)pspec;
    update_process_view(user_data);
}

GtkWidget *create_process_view(GtkWidget *parent, gpointer ui_context) {
    UIContext *ctx = (UIContext *)ui_context;

    /* Allocate the private state structure */
    ProcessViewState *state = g_new0(ProcessViewState, 1);
    state->parent_window = GTK_WINDOW(parent);
    ctx->process_view_context = state;

    /* Initialize process reader delta cache */
    process_reader_init();

    /* Main Container VBox */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 12);
    gtk_widget_set_margin_end(vbox, 12);
    gtk_widget_set_margin_top(vbox, 12);
    gtk_widget_set_margin_bottom(vbox, 12);

    /* Search Bar Box Setup */
    GtkWidget *search_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(vbox), search_box);

    GtkWidget *lbl_search = gtk_label_new("Search Processes:");
    gtk_widget_add_css_class(lbl_search, "info-list-label");
    gtk_box_append(GTK_BOX(search_box), lbl_search);

    state->search_entry = gtk_search_entry_new();
    gtk_widget_set_hexpand(state->search_entry, TRUE);
    gtk_box_append(GTK_BOX(search_box), state->search_entry);

    GtkWidget *lbl_all = gtk_label_new("All Users:");
    gtk_widget_add_css_class(lbl_all, "info-list-label");
    gtk_widget_set_margin_start(lbl_all, 12);
    gtk_box_append(GTK_BOX(search_box), lbl_all);

    state->switch_all = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(state->switch_all), FALSE);
    g_signal_connect(state->switch_all, "notify::active", G_CALLBACK(on_switch_all_notify), ui_context);
    gtk_box_append(GTK_BOX(search_box), state->switch_all);

    /* Create the base GtkListStore */
    state->store = gtk_list_store_new(NUM_COLS, 
                                      G_TYPE_STRING,   /* Name */
                                      G_TYPE_STRING,   /* Icon Name */
                                      G_TYPE_INT,      /* PID */
                                      G_TYPE_DOUBLE,   /* CPU */
                                      G_TYPE_DOUBLE,   /* RAM */
                                      G_TYPE_DOUBLE,   /* Real Memory */
                                      G_TYPE_DOUBLE,   /* GPU */
                                      G_TYPE_DOUBLE);  /* Cache */

    /* Wrap in Filter Model */
    state->filter_model = gtk_tree_model_filter_new(GTK_TREE_MODEL(state->store), NULL);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(state->filter_model), 
                                           process_search_filter_func, 
                                           state->search_entry, NULL);

    /* Connect search entry trigger to filter model updates */
    g_signal_connect(state->search_entry, "search-changed", G_CALLBACK(on_search_changed), state->filter_model);

    /* Wrap in Sort Model */
    GtkTreeModel *sort_model = gtk_tree_model_sort_new_with_model(state->filter_model);

    /* Create TreeView wrapped in Scroll Window */
    state->tree_view = gtk_tree_view_new_with_model(sort_model);
    g_object_unref(sort_model);
    g_object_unref(state->filter_model);
    g_object_unref(state->store);

    /* Grid columns visual setup */
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;

    /* 1. Name Column */
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "Application / Service");
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_min_width(column, 200);
    gtk_tree_view_column_set_sort_column_id(column, COL_NAME);

    /* Icon Renderer */
    GtkCellRenderer *icon_renderer = gtk_cell_renderer_pixbuf_new();
    g_object_set(icon_renderer, "xpad", 4, NULL);
    gtk_tree_view_column_pack_start(column, icon_renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, icon_renderer, "icon-name", COL_ICON_NAME);

    /* Text Renderer */
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer,
                 "ellipsize", 3, /* PANGO_ELLIPSIZE_END is 3. Using number to avoid any header dependency issues */
                 "width-chars", 20,
                 "max-width-chars", 20,
                 NULL);
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_add_attribute(column, renderer, "text", COL_NAME);

    gtk_tree_view_append_column(GTK_TREE_VIEW(state->tree_view), column);

    /* 2. PID Column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("PID", renderer, "text", COL_PID, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, COL_PID);
    gtk_tree_view_append_column(GTK_TREE_VIEW(state->tree_view), column);

    /* 3. CPU Column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_title(column, "CPU (%)");
    gtk_tree_view_column_set_cell_data_func(column, renderer, format_cpu_cell, NULL, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, COL_CPU);
    gtk_tree_view_append_column(GTK_TREE_VIEW(state->tree_view), column);

    /* 4. RAM Column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_title(column, "RAM Usage (MB)");
    gtk_tree_view_column_set_cell_data_func(column, renderer, format_ram_cell, NULL, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, COL_RAM);
    gtk_tree_view_append_column(GTK_TREE_VIEW(state->tree_view), column);

    /* 4b. Real Memory Column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_title(column, "Real Memory");
    gtk_tree_view_column_set_cell_data_func(column, renderer, format_real_mem_cell, NULL, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, COL_REAL_MEM);
    gtk_tree_view_append_column(GTK_TREE_VIEW(state->tree_view), column);

    /* 5. GPU Column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_title(column, "GPU (%)");
    gtk_tree_view_column_set_cell_data_func(column, renderer, format_gpu_cell, NULL, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, COL_GPU);
    gtk_tree_view_append_column(GTK_TREE_VIEW(state->tree_view), column);

    /* 6. Cache Column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_title(column, "Memory Cache");
    gtk_tree_view_column_set_cell_data_func(column, renderer, format_cache_cell, NULL, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, COL_CACHE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(state->tree_view), column);

    /* TreeView Styling */
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(state->tree_view), TRUE);
    gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(state->tree_view), GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
    gtk_widget_add_css_class(state->tree_view, "card");

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), state->tree_view);
    gtk_box_append(GTK_BOX(vbox), scroll);

    /* Right click gesture setup for context menu */
    GtkGesture *gesture = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture), GDK_BUTTON_SECONDARY);
    g_signal_connect(gesture, "pressed", G_CALLBACK(on_treeview_right_click), state);
    gtk_widget_add_controller(state->tree_view, GTK_EVENT_CONTROLLER(gesture));

    return vbox;
}

void update_process_view(gpointer ui_context) {
    UIContext *ctx = (UIContext *)ui_context;
    if (!ctx || !ctx->process_view_context) return;

    ProcessViewState *state = (ProcessViewState *)ctx->process_view_context;
    
    /* Determine if Demo/Simulation Mode is active from UI switch */
    bool demo_mode = gtk_switch_get_active(GTK_SWITCH(ctx->switch_demo));

    /* Poll fresh process metrics */
    process_reader_update(&state->list, demo_mode);

    bool show_all = gtk_switch_get_active(GTK_SWITCH(state->switch_all));
    uid_t my_uid = getuid();

    /* 1. Remove rows for processes that are no longer active or don't match the user filter */
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(state->store), &iter);
    while (valid) {
        int row_pid;
        gtk_tree_model_get(GTK_TREE_MODEL(state->store), &iter, COL_PID, &row_pid, -1);
        
        gboolean still_active = FALSE;
        for (int i = 0; i < state->list.process_count; i++) {
            ProcessInfo *p = &state->list.processes[i];
            if (!show_all && p->uid != (int)my_uid) {
                continue;
            }
            if (p->pid == row_pid) {
                still_active = TRUE;
                break;
            }
        }
        
        if (!still_active) {
            valid = gtk_list_store_remove(state->store, &iter);
        } else {
            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(state->store), &iter);
        }
    }

    /* 2. Update existing rows or append new ones */
    for (int i = 0; i < state->list.process_count; i++) {
        ProcessInfo *p = &state->list.processes[i];
        
        if (!show_all && p->uid != (int)my_uid) {
            continue;
        }

        gboolean found = FALSE;
        if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(state->store), &iter)) {
            do {
                int row_pid;
                gtk_tree_model_get(GTK_TREE_MODEL(state->store), &iter, COL_PID, &row_pid, -1);
                if (row_pid == p->pid) {
                    found = TRUE;
                    break;
                }
            } while (gtk_tree_model_iter_next(GTK_TREE_MODEL(state->store), &iter));
        }

        if (found) {
            gtk_list_store_set(state->store, &iter,
                               COL_NAME, p->name,
                               COL_ICON_NAME, get_process_icon_name(p->name),
                               COL_CPU, p->cpu_percent,
                               COL_RAM, p->ram_mb,
                               COL_REAL_MEM, p->real_mem_mb,
                               COL_GPU, p->gpu_percent,
                               COL_CACHE, p->cache_mb,
                               -1);
        } else {
            GtkTreeIter new_iter;
            gtk_list_store_append(state->store, &new_iter);
            gtk_list_store_set(state->store, &new_iter,
                               COL_NAME, p->name,
                               COL_ICON_NAME, get_process_icon_name(p->name),
                               COL_PID, p->pid,
                               COL_CPU, p->cpu_percent,
                               COL_RAM, p->ram_mb,
                               COL_REAL_MEM, p->real_mem_mb,
                               COL_GPU, p->gpu_percent,
                               COL_CACHE, p->cache_mb,
                               -1);
        }
    }
}
