#define _GNU_SOURCE
#include "process_actions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sched.h>
#include <signal.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <ctype.h>
#include <errno.h>
#include <gtk/gtk.h>

/* Helper to display standard error dialogs */
static void show_error_dialog(GtkWindow *parent, const char *title, const char *message) {
    GtkWidget *dialog = gtk_message_dialog_new(parent,
                                               GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                               GTK_MESSAGE_ERROR,
                                               GTK_BUTTONS_OK,
                                               "%s", message);
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
    gtk_window_present(GTK_WINDOW(dialog));
}

/* Helper to parse status properties from procfs */
static void get_process_properties(int pid, int *ppid, char *state, int *threads, char *owner, char *cmdline) {
    *ppid = 0;
    strcpy(state, "Unknown");
    *threads = 1;
    strcpy(owner, "Unknown");
    strcpy(cmdline, "N/A");

    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE *f = fopen(path, "r");
    if (f) {
        char line[512];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "PPid:", 5) == 0) {
                *ppid = atoi(line + 5);
            } else if (strncmp(line, "State:", 6) == 0) {
                char *s = line + 6;
                while (*s && isspace((unsigned char)*s)) s++;
                strncpy(state, s, 63);
                state[63] = '\0';
                char *nl = strchr(state, '\n');
                if (nl) *nl = '\0';
            } else if (strncmp(line, "Threads:", 8) == 0) {
                *threads = atoi(line + 8);
            } else if (strncmp(line, "Uid:", 4) == 0) {
                int uid = 0;
                sscanf(line + 4, "%d", &uid);
                struct passwd *pw = getpwuid(uid);
                if (pw) {
                    strncpy(owner, pw->pw_name, 63);
                    owner[63] = '\0';
                } else {
                    snprintf(owner, 64, "%d", uid);
                }
            }
        }
        fclose(f);
    }

    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    f = fopen(path, "r");
    if (f) {
        char buf[1024];
        size_t n = fread(buf, 1, sizeof(buf) - 1, f);
        if (n > 0) {
            for (size_t i = 0; i < n - 1; i++) {
                if (buf[i] == '\0') {
                    buf[i] = ' ';
                }
            }
            buf[n] = '\0';
            strncpy(cmdline, buf, 1023);
            cmdline[1023] = '\0';
        }
        fclose(f);
    }
}

/* --- ACTION 1: PROPERTIES DIALOG --- */
void process_action_properties(GtkWindow *parent, int pid, const char *name) {
    int ppid = 0;
    char state[64] = "";
    int threads = 1;
    char owner[64] = "";
    char cmdline[1024] = "";
    
    get_process_properties(pid, &ppid, state, &threads, owner, cmdline);
    
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Process Properties");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 480, 320);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 16);
    gtk_window_set_child(GTK_WINDOW(dialog), vbox);
    
    char title_buf[256];
    snprintf(title_buf, sizeof(title_buf), "%s (PID %d)", name, pid);
    GtkWidget *lbl_title = gtk_label_new(title_buf);
    gtk_widget_add_css_class(lbl_title, "card-title");
    gtk_box_append(GTK_BOX(vbox), lbl_title);
    
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
    gtk_box_append(GTK_BOX(vbox), grid);
    
    int row = 0;
    
    #define ADD_GRID_ROW(label, value) { \
        GtkWidget *lbl_l = gtk_label_new(label); \
        gtk_widget_add_css_class(lbl_l, "info-list-label"); \
        gtk_widget_set_halign(lbl_l, GTK_ALIGN_START); \
        gtk_grid_attach(GTK_GRID(grid), lbl_l, 0, row, 1, 1); \
        GtkWidget *lbl_v = gtk_label_new(value); \
        gtk_widget_add_css_class(lbl_v, "info-list-value"); \
        gtk_widget_set_halign(lbl_v, GTK_ALIGN_START); \
        gtk_grid_attach(GTK_GRID(grid), lbl_v, 1, row, 1, 1); \
        row++; \
    }
    
    char buf[128];
    snprintf(buf, sizeof(buf), "%d", ppid);
    ADD_GRID_ROW("Parent PID:", buf);
    ADD_GRID_ROW("State:", state);
    snprintf(buf, sizeof(buf), "%d", threads);
    ADD_GRID_ROW("Active Threads:", buf);
    ADD_GRID_ROW("User Owner:", owner);
    
    GtkWidget *lbl_cmd_lbl = gtk_label_new("Command Line:");
    gtk_widget_add_css_class(lbl_cmd_lbl, "info-list-label");
    gtk_widget_set_halign(lbl_cmd_lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(vbox), lbl_cmd_lbl);
    
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_append(GTK_BOX(vbox), scroll);
    
    GtkWidget *lbl_cmd = gtk_label_new(cmdline);
    gtk_widget_add_css_class(lbl_cmd, "info-list-value");
    gtk_label_set_wrap(GTK_LABEL(lbl_cmd), TRUE);
    gtk_label_set_selectable(GTK_LABEL(lbl_cmd), TRUE);
    gtk_widget_set_halign(lbl_cmd, GTK_ALIGN_START);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), lbl_cmd);
    
    GtkWidget *btn_close = gtk_button_new_with_label("Close");
    gtk_widget_add_css_class(btn_close, "nav-btn");
    g_signal_connect_swapped(btn_close, "clicked", G_CALLBACK(gtk_window_destroy), dialog);
    gtk_box_append(GTK_BOX(vbox), btn_close);
    
    gtk_window_present(GTK_WINDOW(dialog));
}

/* --- ACTION 2: MEMORY MAP DIALOG --- */
void process_action_memory_map(GtkWindow *parent, int pid, const char *name) {
    GtkWidget *dialog = gtk_window_new();
    char title_buf[256];
    snprintf(title_buf, sizeof(title_buf), "Memory Map - %s (PID %d)", name, pid);
    gtk_window_set_title(GTK_WINDOW(dialog), title_buf);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 640, 400);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 16);
    gtk_window_set_child(GTK_WINDOW(dialog), vbox);
    
    GtkWidget *lbl_title = gtk_label_new("Virtual Memory Mappings");
    gtk_widget_add_css_class(lbl_title, "card-title");
    gtk_box_append(GTK_BOX(vbox), lbl_title);
    
    GtkListStore *store = gtk_list_store_new(4, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    FILE *f = fopen(path, "r");
    if (f) {
        char line[1024];
        GtkTreeIter iter;
        while (fgets(line, sizeof(line), f)) {
            char addr[128] = "";
            char perm[32] = "";
            char offset[64] = "";
            char map_path[512] = "";
            
            int n_parsed = sscanf(line, "%127s %31s %63s %*s %*s %511[^\n]", addr, perm, offset, map_path);
            if (n_parsed >= 3) {
                if (n_parsed == 3 || strlen(map_path) == 0) {
                    strcpy(map_path, "[anonymous]");
                }
                gtk_list_store_append(store, &iter);
                gtk_list_store_set(store, &iter, 0, addr, 1, perm, 2, offset, 3, map_path, -1);
            }
        }
        fclose(f);
    }
    
    GtkWidget *tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);
    
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), TRUE);
    gtk_widget_add_css_class(tree_view, "card");
    
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Address Range", renderer, "text", 0, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Perm", renderer, "text", 1, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Offset", renderer, "text", 2, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Mapping / File Path", renderer, "text", 3, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), tree_view);
    gtk_box_append(GTK_BOX(vbox), scroll);
    
    GtkWidget *btn_close = gtk_button_new_with_label("Close");
    gtk_widget_add_css_class(btn_close, "nav-btn");
    g_signal_connect_swapped(btn_close, "clicked", G_CALLBACK(gtk_window_destroy), dialog);
    gtk_box_append(GTK_BOX(vbox), btn_close);
    
    gtk_window_present(GTK_WINDOW(dialog));
}

/* --- ACTION 3: OPEN FILES DIALOG --- */
void process_action_open_files(GtkWindow *parent, int pid, const char *name) {
    GtkWidget *dialog = gtk_window_new();
    char title_buf[256];
    snprintf(title_buf, sizeof(title_buf), "Open Files - %s (PID %d)", name, pid);
    gtk_window_set_title(GTK_WINDOW(dialog), title_buf);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 640, 400);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 16);
    gtk_window_set_child(GTK_WINDOW(dialog), vbox);
    
    GtkWidget *lbl_title = gtk_label_new("Open File Descriptors (FDs)");
    gtk_widget_add_css_class(lbl_title, "card-title");
    gtk_box_append(GTK_BOX(vbox), lbl_title);
    
    GtkListStore *store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
    
    char fd_dir_path[256];
    snprintf(fd_dir_path, sizeof(fd_dir_path), "/proc/%d/fd", pid);
    DIR *dir = opendir(fd_dir_path);
    if (dir) {
        struct dirent *entry;
        GtkTreeIter iter;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                continue;
            }
            
            char link_path[512];
            snprintf(link_path, sizeof(link_path), "%s/%s", fd_dir_path, entry->d_name);
            
            char target[512];
            ssize_t len = readlink(link_path, target, sizeof(target) - 1);
            if (len != -1) {
                target[len] = '\0';
                gtk_list_store_append(store, &iter);
                gtk_list_store_set(store, &iter, 0, entry->d_name, 1, target, -1);
            }
        }
        closedir(dir);
    }
    
    GtkWidget *tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    g_object_unref(store);
    
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(tree_view), TRUE);
    gtk_widget_add_css_class(tree_view, "card");
    
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("FD", renderer, "text", 0, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Open File Path / Target", renderer, "text", 1, NULL);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);
    
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), tree_view);
    gtk_box_append(GTK_BOX(vbox), scroll);
    
    GtkWidget *btn_close = gtk_button_new_with_label("Close");
    gtk_widget_add_css_class(btn_close, "nav-btn");
    g_signal_connect_swapped(btn_close, "clicked", G_CALLBACK(gtk_window_destroy), dialog);
    gtk_box_append(GTK_BOX(vbox), btn_close);
    
    gtk_window_present(GTK_WINDOW(dialog));
}

/* --- ACTION 4: CHANGE PRIORITY --- */
typedef struct {
    int pid;
    GtkWidget *scale;
    GtkWidget *window;
    GtkWindow *parent_win;
} NiceDialogData;

static void on_nice_apply_clicked(GtkWidget *btn, gpointer user_data) {
    (void)btn;
    NiceDialogData *data = (NiceDialogData *)user_data;
    double val = gtk_range_get_value(GTK_RANGE(data->scale));
    int new_nice = (int)val;
    
    if (setpriority(PRIO_PROCESS, data->pid, new_nice) < 0) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Failed to change process priority: %s.\n\nNote: Lowering nice values (increasing priority) usually requires root privileges.", strerror(errno));
        show_error_dialog(data->parent_win, "Error", err_msg);
    }
    
    gtk_window_destroy(GTK_WINDOW(data->window));
    g_free(data);
}

static void on_nice_cancel_clicked(GtkWidget *btn, gpointer user_data) {
    (void)btn;
    NiceDialogData *data = (NiceDialogData *)user_data;
    gtk_window_destroy(GTK_WINDOW(data->window));
    g_free(data);
}

void process_action_change_priority(GtkWindow *parent, int pid, const char *name) {
    errno = 0;
    int current_nice = getpriority(PRIO_PROCESS, pid);
    if (current_nice == -1 && errno != 0) {
        current_nice = 0;
    }
    
    NiceDialogData *data = g_new0(NiceDialogData, 1);
    data->pid = pid;
    data->parent_win = parent;
    
    GtkWidget *dialog = gtk_window_new();
    data->window = dialog;
    char title_buf[256];
    snprintf(title_buf, sizeof(title_buf), "Change Priority - %s (PID %d)", name, pid);
    gtk_window_set_title(GTK_WINDOW(dialog), title_buf);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 380, 200);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 16);
    gtk_window_set_child(GTK_WINDOW(dialog), vbox);
    
    GtkWidget *lbl_info = gtk_label_new("Set Process Niceness (Priority Level):\n-20 (Highest Priority) to 19 (Lowest Priority)");
    gtk_label_set_justify(GTK_LABEL(lbl_info), GTK_JUSTIFY_CENTER);
    gtk_box_append(GTK_BOX(vbox), lbl_info);
    
    data->scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, -20.0, 19.0, 1.0);
    gtk_range_set_value(GTK_RANGE(data->scale), (double)current_nice);
    gtk_scale_set_draw_value(GTK_SCALE(data->scale), TRUE);
    gtk_scale_set_value_pos(GTK_SCALE(data->scale), GTK_POS_TOP);
    gtk_box_append(GTK_BOX(vbox), data->scale);
    
    GtkWidget *bbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(bbox, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(vbox), bbox);
    
    GtkWidget *btn_apply = gtk_button_new_with_label("Apply");
    gtk_widget_add_css_class(btn_apply, "nav-btn");
    g_signal_connect(btn_apply, "clicked", G_CALLBACK(on_nice_apply_clicked), data);
    gtk_box_append(GTK_BOX(bbox), btn_apply);
    
    GtkWidget *btn_cancel = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(btn_cancel, "nav-btn");
    g_signal_connect(btn_cancel, "clicked", G_CALLBACK(on_nice_cancel_clicked), data);
    gtk_box_append(GTK_BOX(bbox), btn_cancel);
    
    gtk_window_present(GTK_WINDOW(dialog));
}

/* --- ACTION 5: SET AFFINITY --- */
typedef struct {
    int pid;
    int num_cores;
    GtkWidget **checks;
    GtkWidget *window;
    GtkWindow *parent_win;
} AffinityDialogData;

static void on_affinity_apply_clicked(GtkWidget *btn, gpointer user_data) {
    (void)btn;
    AffinityDialogData *data = (AffinityDialogData *)user_data;
    
    cpu_set_t mask;
    CPU_ZERO(&mask);
    
    for (int i = 0; i < data->num_cores; i++) {
        if (gtk_check_button_get_active(GTK_CHECK_BUTTON(data->checks[i]))) {
            CPU_SET(i, &mask);
        }
    }
    
    if (sched_setaffinity(data->pid, sizeof(mask), &mask) < 0) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Failed to apply CPU affinity: %s", strerror(errno));
        show_error_dialog(data->parent_win, "Error", err_msg);
    }
    
    gtk_window_destroy(GTK_WINDOW(data->window));
    g_free(data->checks);
    g_free(data);
}

static void on_affinity_cancel_clicked(GtkWidget *btn, gpointer user_data) {
    (void)btn;
    AffinityDialogData *data = (AffinityDialogData *)user_data;
    gtk_window_destroy(GTK_WINDOW(data->window));
    g_free(data->checks);
    g_free(data);
}

void process_action_set_affinity(GtkWindow *parent, int pid, const char *name) {
    int num_cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (num_cores <= 0) num_cores = 1;
    if (num_cores > 128) num_cores = 128;
    
    cpu_set_t mask;
    CPU_ZERO(&mask);
    if (sched_getaffinity(pid, sizeof(mask), &mask) < 0) {
        for (int i = 0; i < num_cores; i++) {
            CPU_SET(i, &mask);
        }
    }
    
    AffinityDialogData *data = g_new0(AffinityDialogData, 1);
    data->pid = pid;
    data->num_cores = num_cores;
    data->checks = g_new0(GtkWidget *, num_cores);
    data->parent_win = parent;
    
    GtkWidget *dialog = gtk_window_new();
    data->window = dialog;
    char title_buf[256];
    snprintf(title_buf, sizeof(title_buf), "CPU Affinity - %s (PID %d)", name, pid);
    gtk_window_set_title(GTK_WINDOW(dialog), title_buf);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 320, 240);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 16);
    gtk_window_set_child(GTK_WINDOW(dialog), vbox);
    
    GtkWidget *lbl_info = gtk_label_new("Select CPU Cores this process is allowed to run on:");
    gtk_label_set_wrap(GTK_LABEL(lbl_info), TRUE);
    gtk_box_append(GTK_BOX(vbox), lbl_info);
    
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_append(GTK_BOX(vbox), scroll);
    
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), grid);
    
    for (int i = 0; i < num_cores; i++) {
        char core_label[64];
        snprintf(core_label, sizeof(core_label), "CPU Core %d", i);
        data->checks[i] = gtk_check_button_new_with_label(core_label);
        gtk_check_button_set_active(GTK_CHECK_BUTTON(data->checks[i]), CPU_ISSET(i, &mask));
        
        int col = i % 2;
        int row = i / 2;
        gtk_grid_attach(GTK_GRID(grid), data->checks[i], col, row, 1, 1);
    }
    
    GtkWidget *bbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(bbox, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(vbox), bbox);
    
    GtkWidget *btn_apply = gtk_button_new_with_label("Apply");
    gtk_widget_add_css_class(btn_apply, "nav-btn");
    g_signal_connect(btn_apply, "clicked", G_CALLBACK(on_affinity_apply_clicked), data);
    gtk_box_append(GTK_BOX(bbox), btn_apply);
    
    GtkWidget *btn_cancel = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(btn_cancel, "nav-btn");
    g_signal_connect(btn_cancel, "clicked", G_CALLBACK(on_affinity_cancel_clicked), data);
    gtk_box_append(GTK_BOX(bbox), btn_cancel);
    
    gtk_window_present(GTK_WINDOW(dialog));
}

/* --- ACTION 6: SEND SIGNALS & CONFIRMATION --- */
typedef struct {
    int pid;
    int sig;
    GtkWidget *window;
    GtkWindow *parent_win;
} SignalConfirmData;

static void on_signal_confirm_yes(GtkWidget *btn, gpointer user_data) {
    (void)btn;
    SignalConfirmData *data = (SignalConfirmData *)user_data;
    
    if (kill(data->pid, data->sig) < 0) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "Failed to send signal to process: %s", strerror(errno));
        show_error_dialog(data->parent_win, "Error", err_msg);
    }
    
    gtk_window_destroy(GTK_WINDOW(data->window));
    g_free(data);
}

static void on_signal_confirm_no(GtkWidget *btn, gpointer user_data) {
    (void)btn;
    SignalConfirmData *data = (SignalConfirmData *)user_data;
    gtk_window_destroy(GTK_WINDOW(data->window));
    g_free(data);
}

void process_action_send_signal(GtkWindow *parent, int pid, const char *name, int signal_num) {
    if (signal_num == SIGSTOP || signal_num == SIGCONT) {
        if (kill(pid, signal_num) < 0) {
            char err_msg[256];
            snprintf(err_msg, sizeof(err_msg), "Failed to send signal: %s", strerror(errno));
            show_error_dialog(parent, "Error", err_msg);
        }
        return;
    }
    
    SignalConfirmData *data = g_new0(SignalConfirmData, 1);
    data->pid = pid;
    data->sig = signal_num;
    data->parent_win = parent;
    
    GtkWidget *dialog = gtk_window_new();
    data->window = dialog;
    
    const char *action_str = (signal_num == SIGKILL) ? "FORCED KILL" : "TERMINATE";
    char title_buf[256];
    snprintf(title_buf, sizeof(title_buf), "Confirm - %s", action_str);
    gtk_window_set_title(GTK_WINDOW(dialog), title_buf);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 340, 160);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 16);
    gtk_window_set_child(GTK_WINDOW(dialog), vbox);
    
    char prompt[512];
    if (signal_num == SIGKILL) {
        snprintf(prompt, sizeof(prompt), "Are you absolutely sure you want to FORCE KILL process '%s' (PID %d)?\n\nThis will terminate it immediately and may cause data loss.", name, pid);
    } else {
        snprintf(prompt, sizeof(prompt), "Are you sure you want to TERMINATE process '%s' (PID %d)?\n\nThis will request the application to close gracefully.", name, pid);
    }
    
    GtkWidget *lbl_prompt = gtk_label_new(prompt);
    gtk_label_set_wrap(GTK_LABEL(lbl_prompt), TRUE);
    gtk_label_set_justify(GTK_LABEL(lbl_prompt), GTK_JUSTIFY_CENTER);
    gtk_box_append(GTK_BOX(vbox), lbl_prompt);
    
    GtkWidget *bbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(bbox, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(vbox), bbox);
    
    GtkWidget *btn_yes = gtk_button_new_with_label((signal_num == SIGKILL) ? "Yes, Force Kill" : "Yes, Terminate");
    gtk_widget_add_css_class(btn_yes, "nav-btn");
    g_signal_connect(btn_yes, "clicked", G_CALLBACK(on_signal_confirm_yes), data);
    gtk_box_append(GTK_BOX(bbox), btn_yes);
    
    GtkWidget *btn_no = gtk_button_new_with_label("Cancel");
    gtk_widget_add_css_class(btn_no, "nav-btn");
    g_signal_connect(btn_no, "clicked", G_CALLBACK(on_signal_confirm_no), data);
    gtk_box_append(GTK_BOX(bbox), btn_no);
    
    gtk_window_present(GTK_WINDOW(dialog));
}
