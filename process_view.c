#include "process_view.h"
#include "ui.h"
#include "process_actions.h"
#include <string.h>

enum {
    COL_NAME = 0,
    COL_PID,
    COL_CPU,
    COL_RAM,
    COL_REAL_MEM,
    COL_GPU,
    COL_CACHE,
    NUM_COLS
};

typedef struct {
    GtkListStore *store;
    GtkWidget *tree_view;
    GtkWidget *search_entry;
    GtkTreeModel *filter_model;
    ProcessList list;
    GtkWindow *parent_window;
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
    
    /* Pop down the popover, which triggers closed -> unparent -> destroy callbacks */
    gtk_popover_popdown(GTK_POPOVER(data->popover));
    
    if (strcmp(action, "properties") == 0) {
        process_action_properties(data->parent_win, data->pid, data->name);
    } else if (strcmp(action, "maps") == 0) {
        process_action_memory_map(data->parent_win, data->pid, data->name);
    } else if (strcmp(action, "fd") == 0) {
        process_action_open_files(data->parent_win, data->pid, data->name);
    } else if (strcmp(action, "priority") == 0) {
        process_action_change_priority(data->parent_win, data->pid, data->name);
    } else if (strcmp(action, "affinity") == 0) {
        process_action_set_affinity(data->parent_win, data->pid, data->name);
    } else if (strcmp(action, "stop") == 0) {
        process_action_send_signal(data->parent_win, data->pid, data->name, 19); /* SIGSTOP is 19 on Linux */
    } else if (strcmp(action, "continue") == 0) {
        process_action_send_signal(data->parent_win, data->pid, data->name, 18); /* SIGCONT is 18 on Linux */
    } else if (strcmp(action, "terminate") == 0) {
        process_action_send_signal(data->parent_win, data->pid, data->name, 15); /* SIGTERM is 15 on Linux */
    } else if (strcmp(action, "kill") == 0) {
        process_action_send_signal(data->parent_win, data->pid, data->name, 9);  /* SIGKILL is 9 on Linux */
    }
}

static void show_process_context_menu(ProcessViewState *state, int pid, const char *name, double x, double y) {
    GtkWidget *popover = gtk_popover_new();
    gtk_widget_set_parent(popover, state->tree_view);
    
    /* Destroy and free callback data when the popover is dismissed */
    g_signal_connect(popover, "closed", G_CALLBACK(gtk_widget_unparent), NULL);
    
    ContextMenuCallbackData *data = g_new0(ContextMenuCallbackData, 1);
    data->pid = pid;
    strncpy(data->name, name, sizeof(data->name) - 1);
    data->popover = popover;
    data->parent_win = state->parent_window;
    g_signal_connect(popover, "destroy", G_CALLBACK(on_popover_destroy), data);
    
    GdkRectangle rect;
    rect.x = (int)x;
    rect.y = (int)y;
    rect.width = 1;
    rect.height = 1;
    gtk_popover_set_pointing_to(GTK_POPOVER(popover), &rect);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_margin_start(vbox, 6);
    gtk_widget_set_margin_end(vbox, 6);
    gtk_widget_set_margin_top(vbox, 6);
    gtk_widget_set_margin_bottom(vbox, 6);
    gtk_popover_set_child(GTK_POPOVER(popover), vbox);
    
    char header[256];
    snprintf(header, sizeof(header), "%s (PID %d)", name, pid);
    GtkWidget *lbl_header = gtk_label_new(header);
    gtk_widget_add_css_class(lbl_header, "card-title");
    gtk_widget_set_margin_bottom(lbl_header, 4);
    gtk_box_append(GTK_BOX(vbox), lbl_header);
    
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_append(GTK_BOX(vbox), sep);
    
    #define ADD_MENU_BUTTON(lbl, act, theme) { \
        GtkWidget *btn = gtk_button_new_with_label(lbl); \
        gtk_widget_add_css_class(btn, "nav-btn"); \
        if (theme) gtk_widget_add_css_class(btn, theme); \
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
    gtk_box_append(GTK_BOX(vbox), sep);
    
    ADD_MENU_BUTTON("Stop (Pause)", "stop", "battery-theme");
    ADD_MENU_BUTTON("Continue (Resume)", "continue", "cpu-theme");
    ADD_MENU_BUTTON("Terminate", "terminate", "storage-theme");
    ADD_MENU_BUTTON("Kill Process (Force)", "kill", "network-theme");
    
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
            gtk_tree_model_get(model, &iter, COL_PID, &pid, COL_NAME, &name, -1);
            
            char name_buf[256];
            if (name) {
                strncpy(name_buf, name, sizeof(name_buf) - 1);
                name_buf[sizeof(name_buf) - 1] = '\0';
                g_free(name);
            } else {
                strcpy(name_buf, "Unknown");
            }
            
            show_process_context_menu(state, pid, name_buf, x, y);
        }
        gtk_tree_path_free(path);
    }
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

    /* Create the base GtkListStore */
    state->store = gtk_list_store_new(NUM_COLS, 
                                      G_TYPE_STRING,   /* Name */
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
    renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer,
                 "ellipsize", 3, /* PANGO_ELLIPSIZE_END is 3. Using number to avoid any header dependency issues */
                 "width-chars", 20,
                 "max-width-chars", 20,
                 NULL);
    column = gtk_tree_view_column_new_with_attributes("Application / Service", renderer, "text", COL_NAME, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_min_width(column, 160);
    gtk_tree_view_column_set_sort_column_id(column, COL_NAME);
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

    /* Clean old items in model */
    gtk_list_store_clear(state->store);

    /* Populate with updated list */
    GtkTreeIter iter;
    for (int i = 0; i < state->list.process_count; i++) {
        ProcessInfo *p = &state->list.processes[i];
        gtk_list_store_append(state->store, &iter);
        gtk_list_store_set(state->store, &iter,
                           COL_NAME, p->name,
                           COL_PID, p->pid,
                           COL_CPU, p->cpu_percent,
                           COL_RAM, p->ram_mb,
                           COL_REAL_MEM, p->real_mem_mb,
                           COL_GPU, p->gpu_percent,
                           COL_CACHE, p->cache_mb,
                           -1);
    }
}
