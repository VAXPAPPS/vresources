#include "charts.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

HistoryChart *chart_history_new(int max_points, float min_val, float max_val, bool auto_scale, float r, float g, float b) {
    HistoryChart *chart = malloc(sizeof(HistoryChart));
    if (!chart) return NULL;
    
    memset(chart->data, 0, sizeof(chart->data));
    chart->count = 0;
    chart->max_points = max_points > MAX_CHART_POINTS ? MAX_CHART_POINTS : max_points;
    chart->min_val = min_val;
    chart->max_val = max_val;
    chart->auto_scale = auto_scale;
    chart->color_r = r;
    chart->color_g = g;
    chart->color_b = b;
    
    return chart;
}

void chart_history_add_point(HistoryChart *chart, float value) {
    if (!chart) return;
    
    if (chart->count < chart->max_points) {
        chart->data[chart->count] = value;
        chart->count++;
    } else {
        for (int i = 0; i < chart->max_points - 1; i++) {
            chart->data[i] = chart->data[i + 1];
        }
        chart->data[chart->max_points - 1] = value;
    }
}

void chart_history_free(HistoryChart *chart) {
    if (chart) {
        free(chart);
    }
}

void draw_history_chart(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    HistoryChart *chart = (HistoryChart *)user_data;
    if (!chart || chart->count == 0) return;

    (void)area; /* unused */

    float min_val = chart->min_val;
    float max_val = chart->max_val;

    if (chart->auto_scale) {
        min_val = chart->data[0];
        max_val = chart->data[0];
        for (int i = 1; i < chart->count; i++) {
            if (chart->data[i] < min_val) min_val = chart->data[i];
            if (chart->data[i] > max_val) max_val = chart->data[i];
        }
        /* Add 10% padding */
        float range = max_val - min_val;
        if (range < 1.0) range = 1.0;
        max_val += range * 0.1;
        min_val -= range * 0.1;
        if (min_val < 0.0) min_val = 0.0;
    }

    float val_range = max_val - min_val;
    if (val_range <= 0.0) val_range = 1.0;

    double padding_x = 5.0;
    double padding_y = 10.0;
    double plot_w = width - 2.0 * padding_x;
    double plot_h = height - 2.0 * padding_y;

    /* Draw Grid Lines */
    cairo_set_source_rgba(cr, 255, 255, 255, 0.04);
    cairo_set_line_width(cr, 1.0);
    double dashes[] = {4.0, 4.0};
    cairo_set_dash(cr, dashes, 2, 0.0);

    for (int g = 0; g <= 4; g++) {
        double y = padding_y + plot_h * (1.0 - (double)g / 4.0);
        cairo_move_to(cr, padding_x, y);
        cairo_line_to(cr, width - padding_x, y);
        cairo_stroke(cr);
    }
    cairo_set_dash(cr, NULL, 0, 0.0); /* clear dashes */

    /* If not enough points, just clear and return */
    if (chart->count < 2) return;

    /* Helper lambda equivalent: calculate coordinates */
    double get_x(int i) {
        return padding_x + plot_w * ((double)i / (chart->max_points - 1));
    }
    double get_y(int i) {
        float val = chart->data[i];
        if (val < min_val) val = min_val;
        if (val > max_val) val = max_val;
        return padding_y + plot_h * (1.0 - (val - min_val) / val_range);
    }

    /* Create the area path for gradient fill */
    cairo_new_path(cr);
    cairo_move_to(cr, get_x(0), padding_y + plot_h); /* start bottom-left */
    for (int i = 0; i < chart->count; i++) {
        cairo_line_to(cr, get_x(i), get_y(i));
    }
    cairo_line_to(cr, get_x(chart->count - 1), padding_y + plot_h); /* end bottom-right */
    cairo_close_path(cr);

    /* Fill with vertical gradient */
    cairo_pattern_t *pat = cairo_pattern_create_linear(0, padding_y, 0, padding_y + plot_h);
    cairo_pattern_add_color_stop_rgba(pat, 0.0, chart->color_r, chart->color_g, chart->color_b, 0.25);
    cairo_pattern_add_color_stop_rgba(pat, 1.0, chart->color_r, chart->color_g, chart->color_b, 0.0);
    cairo_set_source(cr, pat);
    cairo_fill(cr);
    cairo_pattern_destroy(pat);

    /* Draw the glow line (thick, translucent) */
    cairo_new_path(cr);
    cairo_move_to(cr, get_x(0), get_y(0));
    for (int i = 1; i < chart->count; i++) {
        cairo_line_to(cr, get_x(i), get_y(i));
    }
    cairo_set_source_rgba(cr, chart->color_r, chart->color_g, chart->color_b, 0.25);
    cairo_set_line_width(cr, 4.0);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_stroke(cr);

    /* Draw the sharp foreground line */
    cairo_new_path(cr);
    cairo_move_to(cr, get_x(0), get_y(0));
    for (int i = 1; i < chart->count; i++) {
        cairo_line_to(cr, get_x(i), get_y(i));
    }
    cairo_set_source_rgba(cr, chart->color_r, chart->color_g, chart->color_b, 0.95);
    cairo_set_line_width(cr, 2.0);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_stroke(cr);
}

void draw_radial_gauge(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer user_data) {
    RadialGauge *gauge = (RadialGauge *)user_data;
    if (!gauge) return;

    (void)area;

    double center_x = width / 2.0;
    double center_y = height / 2.0;
    double radius = (width < height ? width : height) / 2.0 - 12.0;
    if (radius < 20.0) radius = 20.0;

    /* Background circle track */
    cairo_set_source_rgba(cr, 255, 255, 255, 0.05);
    cairo_set_line_width(cr, 8.0);
    cairo_arc(cr, center_x, center_y, radius, 0, 2.0 * M_PI);
    cairo_stroke(cr);

    /* Active arc */
    double val_clamped = gauge->value;
    if (val_clamped < 0.0) val_clamped = 0.0;
    if (val_clamped > 100.0) val_clamped = 100.0;

    double angle_start = -M_PI_2;
    double angle_end = angle_start + (val_clamped / 100.0) * 2.0 * M_PI;

    if (val_clamped > 0.0) {
        /* Glow arc */
        cairo_set_source_rgba(cr, gauge->color_r, gauge->color_g, gauge->color_b, 0.25);
        cairo_set_line_width(cr, 12.0);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_arc(cr, center_x, center_y, radius, angle_start, angle_end);
        cairo_stroke(cr);

        /* Sharp arc */
        cairo_set_source_rgba(cr, gauge->color_r, gauge->color_g, gauge->color_b, 0.95);
        cairo_set_line_width(cr, 8.0);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_arc(cr, center_x, center_y, radius, angle_start, angle_end);
        cairo_stroke(cr);
    }

    /* Render text value */
    char val_str[32];
    if (strcmp(gauge->unit, "%") == 0) {
        snprintf(val_str, sizeof(val_str), "%.0f%%", val_clamped);
    } else {
        snprintf(val_str, sizeof(val_str), "%.1f %s", val_clamped, gauge->unit);
    }

    cairo_select_font_face(cr, "Outfit", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, radius * 0.45);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, val_str, &ext);
    
    /* Center horizontal and align vertical */
    double tx = center_x - ext.width / 2.0 - ext.x_bearing;
    double ty = center_y - ext.height / 2.0 - ext.y_bearing;
    
    /* Slightly adjust for label space */
    ty -= radius * 0.08;

    cairo_set_source_rgba(cr, 255, 255, 255, 0.95);
    cairo_move_to(cr, tx, ty);
    cairo_show_text(cr, val_str);

    /* Render label */
    cairo_select_font_face(cr, "Outfit", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, radius * 0.22);
    cairo_text_extents_t ext_lbl;
    cairo_text_extents(cr, gauge->label, &ext_lbl);
    
    double lx = center_x - ext_lbl.width / 2.0 - ext_lbl.x_bearing;
    double ly = center_y + radius * 0.35;
    
    cairo_set_source_rgba(cr, 255, 255, 255, 0.45);
    cairo_move_to(cr, lx, ly);
    cairo_show_text(cr, gauge->label);
}
