#ifndef PROCESS_ACTIONS_H
#define PROCESS_ACTIONS_H

#include <gtk/gtk.h>

void process_action_properties(GtkWindow *parent, int pid, const char *name, double cpu_percent, double ram_mb, double real_mem_mb, double gpu_percent, double cache_mb);
void process_action_memory_map(GtkWindow *parent, int pid, const char *name);
void process_action_open_files(GtkWindow *parent, int pid, const char *name);
void process_action_change_priority(GtkWindow *parent, int pid, const char *name);
void process_action_set_affinity(GtkWindow *parent, int pid, const char *name);
void process_action_send_signal(GtkWindow *parent, int pid, const char *name, int signal_num);

#endif /* PROCESS_ACTIONS_H */
