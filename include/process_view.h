#ifndef PROCESS_VIEW_H
#define PROCESS_VIEW_H

#include <gtk/gtk.h>
#include "process_reader.h"

GtkWidget *create_process_view(GtkWidget *parent, gpointer ui_context);
void update_process_view(gpointer ui_context);

#endif /* PROCESS_VIEW_H */
