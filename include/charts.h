#ifndef CHARTS_H
#define CHARTS_H

#include <gtk/gtk.h>
#include <cairo.h>

#define MAX_CHART_POINTS 120

typedef struct {
    float data[MAX_CHART_POINTS];
    int count;
    int max_points;
    float min_val;
    float max_val;
    bool auto_scale;
    float color_r;
    float color_g;
    float color_b;
} HistoryChart;

typedef struct {
    float value;  /* 0.0 to 100.0 */
    float color_r;
    float color_g;
    float color_b;
    char label[16];
    char unit[8];
} RadialGauge;

/* Initialize and manage history charts */
HistoryChart *chart_history_new(int max_points, float min_val, float max_val, bool auto_scale, float r, float g, float b);
void chart_history_add_point(HistoryChart *chart, float value);
void chart_history_free(HistoryChart *chart);

/* Cairo drawing callback functions for GtkDrawingArea */
void draw_history_chart(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data);
void draw_radial_gauge(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data);

#endif /* CHARTS_H */
