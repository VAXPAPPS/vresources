#include "process_view.h"
#include "ui.h"
#include <string.h>

enum {
    COL_NAME = 0,
    COL_PID,
    COL_CPU,
    COL_RAM,
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

static void format_gpu_cell(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
    (void)col; (void)data;
    double val;
    gtk_tree_model_get(model, iter, COL_GPU, &val, -1);
    char buf[32];
    if (val > 0.0) {
        snprintf(buf, sizeof(buf), "%.1f %%", val);
    } else {
        strcpy(buf, "--");
    }
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

GtkWidget *create_process_view(GtkWidget *parent, gpointer ui_context) {
    (void)parent;
    UIContext *ctx = (UIContext *)ui_context;

    /* Allocate the private state structure */
    ProcessViewState *state = g_new0(ProcessViewState, 1);
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
    column = gtk_tree_view_column_new_with_attributes("Application / Service", renderer, "text", COL_NAME, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_min_width(column, 240);
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
    gtk_tree_view_column_set_title(column, "CPU (%)");
    gtk_tree_view_column_set_cell_data_func(column, renderer, format_cpu_cell, NULL, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, COL_CPU);
    gtk_tree_view_append_column(GTK_TREE_VIEW(state->tree_view), column);

    /* 4. RAM Column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "RAM Usage (MB)");
    gtk_tree_view_column_set_cell_data_func(column, renderer, format_ram_cell, NULL, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, COL_RAM);
    gtk_tree_view_append_column(GTK_TREE_VIEW(state->tree_view), column);

    /* 5. GPU Column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(column, "GPU (%)");
    gtk_tree_view_column_set_cell_data_func(column, renderer, format_gpu_cell, NULL, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_column_set_sort_column_id(column, COL_GPU);
    gtk_tree_view_append_column(GTK_TREE_VIEW(state->tree_view), column);

    /* 6. Cache Column */
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new();
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
                           COL_GPU, p->gpu_percent,
                           COL_CACHE, p->cache_mb,
                           -1);
    }
}
