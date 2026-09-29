/* icons.c — all toolbar/tab icons drawn with cairo (no asset files).
 * Drawn once per size into GdkPixbufs and cached. */
#include "wed.h"

typedef void (*DrawFn)(cairo_t *cr, double s, WedColor c, gboolean dark);

static void stroke_setup(cairo_t *cr, WedColor c, double w) {
    cairo_set_source_rgb(cr, c.r, c.g, c.b);
    cairo_set_line_width(cr, w);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
}

static void draw_back(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.11);
    cairo_move_to(cr, s * 0.62, s * 0.22);
    cairo_line_to(cr, s * 0.32, s * 0.5);
    cairo_line_to(cr, s * 0.62, s * 0.78);
    cairo_stroke(cr);
}
static void draw_fwd(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.11);
    cairo_move_to(cr, s * 0.38, s * 0.22);
    cairo_line_to(cr, s * 0.68, s * 0.5);
    cairo_line_to(cr, s * 0.38, s * 0.78);
    cairo_stroke(cr);
}
static void draw_reload(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.1);
    cairo_arc(cr, s * 0.5, s * 0.5, s * 0.26, -0.5, 3.6);
    cairo_stroke(cr);
    /* arrowhead */
    cairo_move_to(cr, s * 0.5 + s * 0.26 * cos(-0.5), s * 0.5 + s * 0.26 * sin(-0.5));
    cairo_rel_line_to(cr, -s * 0.14, -s * 0.02);
    cairo_rel_line_to(cr, s * 0.06, s * 0.13);
    cairo_close_path(cr);
    cairo_fill(cr);
}
static void draw_home(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.1);
    cairo_move_to(cr, s * 0.16, s * 0.52);
    cairo_line_to(cr, s * 0.5, s * 0.2);
    cairo_line_to(cr, s * 0.84, s * 0.52);
    cairo_stroke(cr);
    cairo_rectangle(cr, s * 0.3, s * 0.52, s * 0.4, s * 0.28);
    cairo_stroke(cr);
    cairo_rectangle(cr, s * 0.44, s * 0.64, s * 0.12, s * 0.16);
    cairo_fill(cr);
}
static void draw_shield(cairo_t *cr, double s, WedColor c, gboolean dk) {
    stroke_setup(cr, c, s * 0.09);
    cairo_move_to(cr, s * 0.5, s * 0.14);
    cairo_line_to(cr, s * 0.8, s * 0.26);
    cairo_line_to(cr, s * 0.8, s * 0.5);
    cairo_curve_to(cr, s * 0.8, s * 0.7, s * 0.66, s * 0.84, s * 0.5, s * 0.88);
    cairo_curve_to(cr, s * 0.34, s * 0.84, s * 0.2, s * 0.7, s * 0.2, s * 0.5);
    cairo_line_to(cr, s * 0.2, s * 0.26);
    cairo_close_path(cr);
    cairo_stroke(cr);
    /* checkmark */
    cairo_move_to(cr, s * 0.38, s * 0.5);
    cairo_line_to(cr, s * 0.47, s * 0.6);
    cairo_line_to(cr, s * 0.64, s * 0.38);
    cairo_stroke(cr);
}
static void draw_shield_off(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; draw_shield(cr, s, c, dk);
    stroke_setup(cr, c, s * 0.1);
    cairo_move_to(cr, s * 0.22, s * 0.8);
    cairo_line_to(cr, s * 0.78, s * 0.2);
    cairo_stroke(cr);
}
static void draw_star(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    double cx = s * 0.5, cy = s * 0.5, R = s * 0.34, r = s * 0.14;
    cairo_move_to(cr, cx, cy - R);
    for (int i = 1; i < 10; i++) {
        double a = -M_PI / 2 + i * M_PI / 5;
        double rad = (i % 2 == 0) ? R : r;
        cairo_line_to(cr, cx + rad * cos(a), cy + rad * sin(a));
    }
    cairo_close_path(cr);
    cairo_stroke(cr);
}
static void draw_star_filled(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk;
    cairo_set_source_rgb(cr, c.r, c.g, c.b);
    double cx = s * 0.5, cy = s * 0.5, R = s * 0.34, r = s * 0.14;
    cairo_move_to(cr, cx, cy - R);
    for (int i = 1; i < 10; i++) {
        double a = -M_PI / 2 + i * M_PI / 5;
        double rad = (i % 2 == 0) ? R : r;
        cairo_line_to(cr, cx + rad * cos(a), cy + rad * sin(a));
    }
    cairo_close_path(cr);
    cairo_fill(cr);
}
static void draw_download(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.1);
    cairo_move_to(cr, s * 0.5, s * 0.18);
    cairo_line_to(cr, s * 0.5, s * 0.66);
    cairo_move_to(cr, s * 0.32, s * 0.5);
    cairo_line_to(cr, s * 0.5, s * 0.68);
    cairo_line_to(cr, s * 0.68, s * 0.5);
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.24, s * 0.82);
    cairo_line_to(cr, s * 0.76, s * 0.82);
    cairo_stroke(cr);
}
static void draw_menu(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; cairo_set_source_rgb(cr, c.r, c.g, c.b);
    for (int i = 0; i < 3; i++) {
        cairo_arc(cr, s * 0.5, s * (0.3 + 0.2 * i), s * 0.055, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}
static void draw_close(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.12);
    cairo_move_to(cr, s * 0.3, s * 0.3);
    cairo_line_to(cr, s * 0.7, s * 0.7);
    cairo_move_to(cr, s * 0.7, s * 0.3);
    cairo_line_to(cr, s * 0.3, s * 0.7);
    cairo_stroke(cr);
}
static void draw_search(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.1);
    cairo_arc(cr, s * 0.44, s * 0.44, s * 0.22, 0, 2 * M_PI);
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.6, s * 0.6);
    cairo_line_to(cr, s * 0.76, s * 0.76);
    cairo_stroke(cr);
}
static void draw_gear(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.08);
    cairo_arc(cr, s * 0.5, s * 0.5, s * 0.2, 0, 2 * M_PI);
    cairo_stroke(cr);
    for (int i = 0; i < 8; i++) {
        double a = i * M_PI / 4;
        cairo_move_to(cr, s * 0.5 + s * 0.28 * cos(a), s * 0.5 + s * 0.28 * sin(a));
        cairo_rel_line_to(cr, s * 0.1 * cos(a), s * 0.1 * sin(a));
    }
    cairo_stroke(cr);
}
static void draw_clock(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_arc(cr, s * 0.5, s * 0.5, s * 0.32, 0, 2 * M_PI);
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.5, s * 0.32);
    cairo_line_to(cr, s * 0.5, s * 0.52);
    cairo_line_to(cr, s * 0.64, s * 0.6);
    cairo_stroke(cr);
}
static void draw_bookmark(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.1);
    cairo_move_to(cr, s * 0.28, s * 0.16);
    cairo_line_to(cr, s * 0.72, s * 0.16);
    cairo_line_to(cr, s * 0.72, s * 0.84);
    cairo_line_to(cr, s * 0.5, s * 0.66);
    cairo_line_to(cr, s * 0.28, s * 0.84);
    cairo_close_path(cr);
    cairo_stroke(cr);
}
static void draw_plus(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.12);
    cairo_move_to(cr, s * 0.5, s * 0.24);
    cairo_line_to(cr, s * 0.5, s * 0.76);
    cairo_move_to(cr, s * 0.24, s * 0.5);
    cairo_line_to(cr, s * 0.76, s * 0.5);
    cairo_stroke(cr);
}
static void draw_volume(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.1);
    cairo_move_to(cr, s * 0.24, s * 0.4);
    cairo_line_to(cr, s * 0.38, s * 0.4);
    cairo_line_to(cr, s * 0.54, s * 0.24);
    cairo_line_to(cr, s * 0.54, s * 0.76);
    cairo_line_to(cr, s * 0.38, s * 0.6);
    cairo_line_to(cr, s * 0.24, s * 0.6);
    cairo_close_path(cr);
    cairo_fill(cr);
    cairo_arc(cr, s * 0.66, s * 0.5, s * 0.12, -0.9, 0.9);
    cairo_stroke(cr);
    cairo_arc(cr, s * 0.66, s * 0.5, s * 0.22, -0.8, 0.8);
    cairo_stroke(cr);
}
static void draw_volume_muted(cairo_t *cr, double s, WedColor c, gboolean dk) {
    draw_volume(cr, s, c, dk);
    stroke_setup(cr, g_theme.danger, s * 0.11);
    cairo_move_to(cr, s * 0.62, s * 0.3);
    cairo_line_to(cr, s * 0.82, s * 0.7);
    cairo_stroke(cr);
}
static void draw_info(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_arc(cr, s * 0.5, s * 0.5, s * 0.33, 0, 2 * M_PI);
    cairo_stroke(cr);
    cairo_arc(cr, s * 0.5, s * 0.32, s * 0.05, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_move_to(cr, s * 0.5, s * 0.44);
    cairo_line_to(cr, s * 0.5, s * 0.7);
    cairo_stroke(cr);
}
static void draw_lock(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_rectangle(cr, s * 0.3, s * 0.44, s * 0.4, s * 0.36);
    cairo_stroke(cr);
    cairo_arc(cr, s * 0.5, s * 0.4, s * 0.16, M_PI, 2 * M_PI);
    cairo_stroke(cr);
}
static void draw_trash(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_move_to(cr, s * 0.26, s * 0.3);
    cairo_line_to(cr, s * 0.74, s * 0.3);
    cairo_stroke(cr);
    cairo_rectangle(cr, s * 0.34, s * 0.3, s * 0.32, s * 0.5);
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.42, s * 0.22);
    cairo_line_to(cr, s * 0.58, s * 0.22);
    cairo_stroke(cr);
}
static void draw_ai(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.1);
    /* sparkle: 4-point star */
    cairo_move_to(cr, s * 0.5, s * 0.18);
    cairo_line_to(cr, s * 0.58, s * 0.42);
    cairo_line_to(cr, s * 0.82, s * 0.5);
    cairo_line_to(cr, s * 0.58, s * 0.58);
    cairo_line_to(cr, s * 0.5, s * 0.82);
    cairo_line_to(cr, s * 0.42, s * 0.58);
    cairo_line_to(cr, s * 0.18, s * 0.5);
    cairo_line_to(cr, s * 0.42, s * 0.42);
    cairo_close_path(cr);
    cairo_stroke(cr);
    cairo_arc(cr, s * 0.74, s * 0.26, s * 0.06, 0, 2 * M_PI);
    cairo_fill(cr);
}
static void draw_tab(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.1);
    cairo_move_to(cr, s * 0.2, s * 0.62);
    cairo_curve_to(cr, s * 0.2, s * 0.42, s * 0.32, s * 0.34, s * 0.36, s * 0.24);
    cairo_line_to(cr, s * 0.64, s * 0.24);
    cairo_curve_to(cr, s * 0.68, s * 0.34, s * 0.8, s * 0.42, s * 0.8, s * 0.62);
    cairo_close_path(cr);
    cairo_stroke(cr);
}
static void draw_external(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.1);
    cairo_move_to(cr, s * 0.3, s * 0.7);
    cairo_line_to(cr, s * 0.7, s * 0.3);
    cairo_move_to(cr, s * 0.44, s * 0.3);
    cairo_line_to(cr, s * 0.7, s * 0.3);
    cairo_line_to(cr, s * 0.7, s * 0.56);
    cairo_stroke(cr);
    cairo_rectangle(cr, s * 0.2, s * 0.5, s * 0.34, s * 0.3);
    cairo_stroke(cr);
}

/* ------------------------- Material icon set ------------------------- */
static void draw_person(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; cairo_set_source_rgb(cr, c.r, c.g, c.b);
    cairo_arc(cr, s * 0.5, s * 0.33, s * 0.17, 0, 2 * M_PI);   /* head */
    cairo_fill(cr);
    cairo_new_sub_path(cr);                                     /* shoulders */
    cairo_arc(cr, s * 0.5, s * 0.82, s * 0.30, M_PI, 2 * M_PI);
    cairo_close_path(cr);
    cairo_fill(cr);
}
static void draw_key(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_arc(cr, s * 0.36, s * 0.36, s * 0.16, 0, 2 * M_PI);   /* bow */
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.47, s * 0.47);
    cairo_line_to(cr, s * 0.80, s * 0.80);
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.68, s * 0.68);
    cairo_line_to(cr, s * 0.68, s * 0.78);
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.60, s * 0.60);
    cairo_line_to(cr, s * 0.60, s * 0.70);
    cairo_stroke(cr);
}
static void draw_palette(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    /* teardrop palette shape */
    cairo_move_to(cr, s * 0.5, s * 0.16);
    cairo_curve_to(cr, s * 0.78, s * 0.16, s * 0.84, s * 0.42, s * 0.80, s * 0.60);
    cairo_curve_to(cr, s * 0.76, s * 0.80, s * 0.62, s * 0.84, s * 0.56, s * 0.80);
    cairo_line_to(cr, s * 0.48, s * 0.72);
    cairo_line_to(cr, s * 0.38, s * 0.78);
    cairo_curve_to(cr, s * 0.28, s * 0.82, s * 0.18, s * 0.74, s * 0.20, s * 0.58);
    cairo_curve_to(cr, s * 0.22, s * 0.34, s * 0.32, s * 0.16, s * 0.5, s * 0.16);
    cairo_close_path(cr);
    cairo_stroke(cr);
    cairo_arc(cr, s * 0.42, s * 0.38, s * 0.035, 0, 2 * M_PI);  /* paint dots */
    cairo_fill(cr);
    cairo_arc(cr, s * 0.58, s * 0.36, s * 0.035, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_arc(cr, s * 0.64, s * 0.52, s * 0.035, 0, 2 * M_PI);
    cairo_fill(cr);
}
static void draw_globe(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_arc(cr, s * 0.5, s * 0.5, s * 0.32, 0, 2 * M_PI);
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.5, s * 0.18); cairo_line_to(cr, s * 0.5, s * 0.82);
    cairo_stroke(cr);
    cairo_save(cr);
    cairo_translate(cr, s * 0.5, s * 0.5);
    cairo_scale(cr, 1.0, 0.42);
    cairo_arc(cr, 0, 0, s * 0.32, 0, 2 * M_PI);
    cairo_restore(cr);
    cairo_stroke(cr);
    cairo_save(cr);
    cairo_translate(cr, s * 0.5, s * 0.5);
    cairo_scale(cr, 0.42, 1.0);
    cairo_arc(cr, 0, 0, s * 0.32, 0, 2 * M_PI);
    cairo_restore(cr);
    cairo_stroke(cr);
}
static void draw_laptop(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_rectangle(cr, s * 0.24, s * 0.26, s * 0.52, s * 0.34);  /* screen */
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.14, s * 0.72);                        /* base */
    cairo_line_to(cr, s * 0.86, s * 0.72);
    cairo_stroke(cr);
}
static void draw_gauge(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_arc(cr, s * 0.5, s * 0.55, s * 0.32, M_PI, 2 * M_PI);   /* dial */
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.5, s * 0.55);                          /* needle */
    cairo_line_to(cr, s * 0.66, s * 0.34);
    cairo_stroke(cr);
    cairo_arc(cr, s * 0.5, s * 0.55, s * 0.04, 0, 2 * M_PI);
    cairo_fill(cr);
}
static void draw_language(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_move_to(cr, s * 0.14, s * 0.30);                         /* A stroke */
    cairo_line_to(cr, s * 0.30, s * 0.30);
    cairo_move_to(cr, s * 0.22, s * 0.30);
    cairo_line_to(cr, s * 0.22, s * 0.72);
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.17, s * 0.62);
    cairo_line_to(cr, s * 0.27, s * 0.62);
    cairo_stroke(cr);
    /* 文-style right glyph */
    cairo_move_to(cr, s * 0.42, s * 0.32);
    cairo_line_to(cr, s * 0.84, s * 0.32);
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.63, s * 0.32);
    cairo_line_to(cr, s * 0.63, s * 0.72);
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.50, s * 0.46);
    cairo_line_to(cr, s * 0.76, s * 0.46);
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.50, s * 0.58);
    cairo_line_to(cr, s * 0.76, s * 0.58);
    cairo_stroke(cr);
}
static void draw_access(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.10);
    cairo_arc(cr, s * 0.5, s * 0.18, s * 0.06, 0, 2 * M_PI);      /* head */
    cairo_fill(cr);
    cairo_move_to(cr, s * 0.5, s * 0.28);
    cairo_line_to(cr, s * 0.5, s * 0.62);                          /* body */
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.28, s * 0.38);                         /* arms */
    cairo_line_to(cr, s * 0.72, s * 0.38);
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.5, s * 0.62);                          /* legs */
    cairo_line_to(cr, s * 0.32, s * 0.82);
    cairo_move_to(cr, s * 0.5, s * 0.62);
    cairo_line_to(cr, s * 0.68, s * 0.82);
    cairo_stroke(cr);
}
static void draw_computer(cairo_t *cr, double s, WedColor c, gboolean dk) {
    draw_laptop(cr, s, c, dk);
}
static void draw_reset(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_arc(cr, s * 0.5, s * 0.52, s * 0.26, 0.6, 4.8);
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.5 + s * 0.26 * cos(4.8), s * 0.52 + s * 0.26 * sin(4.8));
    cairo_rel_line_to(cr, -s * 0.13, 0.0);
    cairo_rel_line_to(cr, s * 0.05, s * 0.12);
    cairo_close_path(cr);
    cairo_fill(cr);
}
static void draw_chevron(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.10);
    cairo_move_to(cr, s * 0.42, s * 0.26);
    cairo_line_to(cr, s * 0.62, s * 0.5);
    cairo_line_to(cr, s * 0.42, s * 0.74);
    cairo_stroke(cr);
}
static void draw_mic(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_new_sub_path(cr);
    cairo_arc(cr, s * 0.5, s * 0.34, s * 0.11, -M_PI, 0);          /* capsule */
    cairo_line_to(cr, s * 0.61, s * 0.48);
    cairo_arc(cr, s * 0.5, s * 0.48, s * 0.11, 0, M_PI);
    cairo_close_path(cr);
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.36, s * 0.50);                         /* arc + stem */
    cairo_arc(cr, s * 0.5, s * 0.50, s * 0.14, M_PI, 2 * M_PI);
    cairo_line_to(cr, s * 0.5, s * 0.72);
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.40, s * 0.78);
    cairo_line_to(cr, s * 0.60, s * 0.78);
    cairo_stroke(cr);
}
static void draw_camera(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_new_sub_path(cr);                                        /* lens */
    cairo_move_to(cr, s * 0.20, s * 0.34);
    cairo_line_to(cr, s * 0.68, s * 0.34);
    cairo_curve_to(cr, s * 0.76, s * 0.34, s * 0.80, s * 0.40, s * 0.80, s * 0.48);
    cairo_line_to(cr, s * 0.80, s * 0.60);
    cairo_curve_to(cr, s * 0.80, s * 0.70, s * 0.74, s * 0.76, s * 0.64, s * 0.76);
    cairo_line_to(cr, s * 0.36, s * 0.76);
    cairo_curve_to(cr, s * 0.26, s * 0.76, s * 0.20, s * 0.70, s * 0.20, s * 0.60);
    cairo_line_to(cr, s * 0.20, s * 0.40);
    cairo_close_path(cr);
    cairo_stroke(cr);
    cairo_arc(cr, s * 0.5, s * 0.55, s * 0.13, 0, 2 * M_PI);
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.68, s * 0.28);
    cairo_line_to(cr, s * 0.80, s * 0.16);
    cairo_stroke(cr);
}
static void draw_apps(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; cairo_set_source_rgb(cr, c.r, c.g, c.b);
    double cx[3] = { 0.28, 0.5, 0.72 }, cy[3] = { 0.28, 0.5, 0.72 };
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++) {
            if (i == 1 && j == 1) continue;                        /* Chrome-style: center empty */
            cairo_arc(cr, s * cx[j], s * cy[i], s * 0.07, 0, 2 * M_PI);
            cairo_fill(cr);
        }
}
static void draw_sun(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; cairo_set_source_rgb(cr, c.r, c.g, c.b);
    cairo_arc(cr, s * 0.5, s * 0.5, s * 0.17, 0, 2 * M_PI);
    cairo_fill(cr);
    stroke_setup(cr, c, s * 0.07);
    for (int i = 0; i < 8; i++) {
        double a = i * M_PI / 4;
        cairo_move_to(cr, s * 0.5 + s * 0.26 * cos(a), s * 0.5 + s * 0.26 * sin(a));
        cairo_line_to(cr, s * 0.5 + s * 0.36 * cos(a), s * 0.5 + s * 0.36 * sin(a));
    }
    cairo_stroke(cr);
}
static void draw_cloudsun(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk;
    cairo_set_source_rgb(cr, c.r, c.g, c.b);
    cairo_arc(cr, s * 0.62, s * 0.32, s * 0.12, 0, 2 * M_PI);      /* sun */
    cairo_fill(cr);
    stroke_setup(cr, c, s * 0.07);
    for (int i = 0; i < 4; i++) {
        double a = -M_PI / 2 + i * M_PI / 6 - M_PI / 6;
        cairo_move_to(cr, s * 0.62 + s * 0.20 * cos(a), s * 0.32 + s * 0.20 * sin(a));
        cairo_line_to(cr, s * 0.62 + s * 0.28 * cos(a), s * 0.32 + s * 0.28 * sin(a));
    }
    cairo_stroke(cr);
    cairo_new_sub_path(cr);                                        /* cloud */
    cairo_move_to(cr, s * 0.22, s * 0.68);
    cairo_arc(cr, s * 0.34, s * 0.58, s * 0.11, M_PI * 0.5, M_PI * 1.5);
    cairo_arc(cr, s * 0.50, s * 0.50, s * 0.13, M_PI, M_PI * 1.75);
    cairo_arc(cr, s * 0.62, s * 0.62, s * 0.11, M_PI * 1.3, M_PI * 1.98);
    cairo_close_path(cr);
    cairo_fill(cr);
}
static void draw_moon(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; cairo_set_source_rgb(cr, c.r, c.g, c.b);
    cairo_new_sub_path(cr);
    cairo_arc(cr, s * 0.52, s * 0.5, s * 0.30, M_PI * 0.25, M_PI * 1.25);
    cairo_curve_to(cr, s * 0.68, s * 0.86, s * 0.28, s * 0.86, s * 0.22, s * 0.5);
    cairo_close_path(cr);
    cairo_fill(cr);
}
static void draw_cookie(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; cairo_set_source_rgb(cr, c.r, c.g, c.b);
    cairo_arc(cr, s * 0.5, s * 0.5, s * 0.32, 0.3, 2 * M_PI - 0.3);/* bitten cookie */
    cairo_line_to(cr, s * 0.72, s * 0.42);
    cairo_close_path(cr);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 1, 1, 1);                             /* chips */
    double px[4] = { 0.40, 0.55, 0.46, 0.60 }, py[4] = { 0.40, 0.46, 0.58, 0.60 };
    for (int i = 0; i < 4; i++) {
        cairo_arc(cr, s * px[i], s * py[i], s * 0.035, 0, 2 * M_PI);
        cairo_fill(cr);
    }
}
static void draw_fingerprint(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.07);
    for (int i = 2; i <= 4; i++) {
        cairo_save(cr);
        cairo_translate(cr, s * 0.5, s * 0.52);
        cairo_scale(cr, 1.0, 1.12);
        cairo_arc(cr, 0, 0, s * 0.08 * i, -0.5, 3.6);
        cairo_restore(cr);
        cairo_stroke(cr);
    }
}
static void draw_eye(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_save(cr);
    cairo_translate(cr, s * 0.5, s * 0.5);
    cairo_scale(cr, 1.0, 0.6);
    cairo_arc(cr, 0, 0, s * 0.30, 0, 2 * M_PI);
    cairo_restore(cr);
    cairo_stroke(cr);
    cairo_arc(cr, s * 0.5, s * 0.5, s * 0.09, 0, 2 * M_PI);
    cairo_fill(cr);
}
static void draw_folder(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_new_sub_path(cr);
    cairo_move_to(cr, s * 0.14, s * 0.70);
    cairo_line_to(cr, s * 0.14, s * 0.34);
    cairo_line_to(cr, s * 0.36, s * 0.34);
    cairo_line_to(cr, s * 0.44, s * 0.26);
    cairo_line_to(cr, s * 0.72, s * 0.26);
    cairo_curve_to(cr, s * 0.82, s * 0.26, s * 0.86, s * 0.32, s * 0.86, s * 0.40);
    cairo_line_to(cr, s * 0.86, s * 0.70);
    cairo_close_path(cr);
    cairo_stroke(cr);
}
static void draw_pin(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_move_to(cr, s * 0.5, s * 0.14);                          /* pin head */
    cairo_curve_to(cr, s * 0.30, s * 0.32, s * 0.36, s * 0.50, s * 0.42, s * 0.58);
    cairo_line_to(cr, s * 0.58, s * 0.58);
    cairo_curve_to(cr, s * 0.64, s * 0.50, s * 0.70, s * 0.32, s * 0.5, s * 0.14);
    cairo_close_path(cr);
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.50, s * 0.58);                         /* needle */
    cairo_line_to(cr, s * 0.50, s * 0.86);
    cairo_stroke(cr);
}
static void draw_copy(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_rectangle(cr, s * 0.32, s * 0.32, s * 0.42, s * 0.42);
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.60, s * 0.24);
    cairo_line_to(cr, s * 0.28, s * 0.24);
    cairo_curve_to(cr, s * 0.22, s * 0.24, s * 0.20, s * 0.28, s * 0.20, s * 0.34);
    cairo_line_to(cr, s * 0.20, s * 0.62);
    cairo_stroke(cr);
}
static void draw_share(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; cairo_set_source_rgb(cr, c.r, c.g, c.b);
    for (int i = 0; i < 3; i++) {
        cairo_arc(cr, s * 0.5 + s * 0.24 * cos(-M_PI / 2 + i * 2 * M_PI / 3),
                       s * 0.5 + s * 0.24 * sin(-M_PI / 2 + i * 2 * M_PI / 3),
                  s * 0.09, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    stroke_setup(cr, c, s * 0.08);
    cairo_move_to(cr, s * 0.38, s * 0.34);
    cairo_line_to(cr, s * 0.62, s * 0.62);
    cairo_move_to(cr, s * 0.62, s * 0.34);
    cairo_line_to(cr, s * 0.38, s * 0.62);
    cairo_stroke(cr);
}
static void draw_windowicon(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_rectangle(cr, s * 0.18, s * 0.24, s * 0.64, s * 0.52);
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.18, s * 0.38);
    cairo_line_to(cr, s * 0.82, s * 0.38);
    cairo_stroke(cr);
}
static void draw_zoomin(cairo_t *cr, double s, WedColor c, gboolean dk) {
    draw_search(cr, s, c, dk);
    stroke_setup(cr, c, s * 0.10);
    cairo_move_to(cr, s * 0.38, s * 0.44);
    cairo_line_to(cr, s * 0.50, s * 0.44);
    cairo_move_to(cr, s * 0.44, s * 0.38);
    cairo_line_to(cr, s * 0.44, s * 0.50);
    cairo_stroke(cr);
}
static void draw_zoomout(cairo_t *cr, double s, WedColor c, gboolean dk) {
    draw_search(cr, s, c, dk);
    stroke_setup(cr, c, s * 0.10);
    cairo_move_to(cr, s * 0.38, s * 0.44);
    cairo_line_to(cr, s * 0.50, s * 0.44);
    cairo_stroke(cr);
}
static void draw_print(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_rectangle(cr, s * 0.26, s * 0.14, s * 0.48, s * 0.20);   /* paper top */
    cairo_stroke(cr);
    cairo_rectangle(cr, s * 0.18, s * 0.34, s * 0.64, s * 0.30);   /* body */
    cairo_stroke(cr);
    cairo_rectangle(cr, s * 0.28, s * 0.56, s * 0.44, s * 0.30);   /* paper out */
    cairo_stroke(cr);
}
static void draw_cast(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_move_to(cr, s * 0.20, s * 0.24);
    cairo_curve_to(cr, s * 0.70, s * 0.24, s * 0.80, s * 0.34, s * 0.80, s * 0.44);
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.20, s * 0.40);
    cairo_arc(cr, s * 0.34, s * 0.46, s * 0.14, M_PI, 2 * M_PI);
    cairo_stroke(cr);
    cairo_move_to(cr, s * 0.20, s * 0.58);
    cairo_arc(cr, s * 0.34, s * 0.64, s * 0.26, M_PI, 2 * M_PI);
    cairo_stroke(cr);
    cairo_arc(cr, s * 0.22, s * 0.78, s * 0.03, 0, 2 * M_PI);
    cairo_fill(cr);
}
static void draw_fullscr(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.10);
    cairo_move_to(cr, s * 0.16, s * 0.36); cairo_line_to(cr, s * 0.16, s * 0.16);
    cairo_line_to(cr, s * 0.40, s * 0.16);
    cairo_move_to(cr, s * 0.84, s * 0.36); cairo_line_to(cr, s * 0.84, s * 0.16);
    cairo_line_to(cr, s * 0.60, s * 0.16);
    cairo_move_to(cr, s * 0.16, s * 0.64); cairo_line_to(cr, s * 0.16, s * 0.84);
    cairo_line_to(cr, s * 0.40, s * 0.84);
    cairo_move_to(cr, s * 0.84, s * 0.64); cairo_line_to(cr, s * 0.84, s * 0.84);
    cairo_line_to(cr, s * 0.60, s * 0.84);
    cairo_stroke(cr);
}
static void draw_notif(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_new_sub_path(cr);
    cairo_move_to(cr, s * 0.24, s * 0.60);
    cairo_line_to(cr, s * 0.24, s * 0.42);
    cairo_arc(cr, s * 0.5, s * 0.42, s * 0.26, M_PI, 2 * M_PI);
    cairo_line_to(cr, s * 0.76, s * 0.60);
    cairo_line_to(cr, s * 0.76, s * 0.66);
    cairo_line_to(cr, s * 0.24, s * 0.66);
    cairo_close_path(cr);
    cairo_stroke(cr);
    cairo_arc(cr, s * 0.5, s * 0.70, s * 0.05, 0, 2 * M_PI);
    cairo_fill(cr);
}
static void draw_ext(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_move_to(cr, s * 0.5, s * 0.14);                          /* puzzle bump */
    cairo_curve_to(cr, s * 0.42, s * 0.06, s * 0.30, s * 0.14, s * 0.36, s * 0.26);
    cairo_line_to(cr, s * 0.22, s * 0.26);
    cairo_line_to(cr, s * 0.22, s * 0.78);
    cairo_line_to(cr, s * 0.44, s * 0.78);
    cairo_curve_to(cr, s * 0.42, s * 0.88, s * 0.58, s * 0.88, s * 0.56, s * 0.78);
    cairo_line_to(cr, s * 0.78, s * 0.78);
    cairo_line_to(cr, s * 0.78, s * 0.26);
    cairo_line_to(cr, s * 0.64, s * 0.26);
    cairo_curve_to(cr, s * 0.70, s * 0.14, s * 0.58, s * 0.06, s * 0.5, s * 0.14);
    cairo_stroke(cr);
}
static void draw_incognito(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_new_sub_path(cr);                                        /* hat brim */
    cairo_move_to(cr, s * 0.12, s * 0.48);
    cairo_line_to(cr, s * 0.88, s * 0.48);
    cairo_stroke(cr);
    cairo_new_sub_path(cr);                                        /* hat crown */
    cairo_move_to(cr, s * 0.30, s * 0.48);
    cairo_line_to(cr, s * 0.38, s * 0.22);
    cairo_line_to(cr, s * 0.62, s * 0.22);
    cairo_line_to(cr, s * 0.70, s * 0.48);
    cairo_stroke(cr);
    cairo_arc(cr, s * 0.34, s * 0.66, s * 0.10, 0, 2 * M_PI);      /* glasses */
    cairo_stroke(cr);
    cairo_arc(cr, s * 0.66, s * 0.66, s * 0.10, 0, 2 * M_PI);
    cairo_stroke(cr);
}
static void draw_help(cairo_t *cr, double s, WedColor c, gboolean dk) {
    (void)dk; stroke_setup(cr, c, s * 0.09);
    cairo_arc(cr, s * 0.5, s * 0.5, s * 0.32, 0, 2 * M_PI);
    cairo_stroke(cr);
    cairo_new_sub_path(cr);                                        /* ? shape */
    cairo_move_to(cr, s * 0.38, s * 0.36);
    cairo_curve_to(cr, s * 0.42, s * 0.28, s * 0.58, s * 0.28, s * 0.58, s * 0.38);
    cairo_curve_to(cr, s * 0.58, s * 0.46, s * 0.5, s * 0.48, s * 0.5, s * 0.56);
    cairo_stroke(cr);
    cairo_arc(cr, s * 0.5, s * 0.66, s * 0.035, 0, 2 * M_PI);
    cairo_fill(cr);
}

static DrawFn DRAWERS[IC_COUNT] = {
    draw_back, draw_fwd, draw_reload, draw_home, draw_shield, draw_shield_off,
    draw_star, draw_star_filled, draw_download, draw_menu, draw_close, draw_search,
    draw_gear, draw_clock, draw_bookmark, draw_plus, draw_volume, draw_volume_muted,
    draw_info, draw_lock, draw_trash, draw_ai, draw_tab, draw_external,
    draw_person, draw_key, draw_palette, draw_globe, draw_laptop, draw_gauge,
    draw_language, draw_access, draw_computer, draw_reset, draw_chevron,
    draw_mic, draw_camera, draw_apps, draw_sun, draw_cloudsun, draw_moon, draw_cookie,
    draw_fingerprint, draw_eye, draw_folder, draw_pin, draw_copy, draw_share,
    draw_windowicon, draw_zoomin, draw_zoomout, draw_print, draw_cast, draw_fullscr,
    draw_notif, draw_ext, draw_incognito, draw_help,
};

static GdkPixbuf *cache[IC_COUNT][5];   /* size buckets: 12,16,20,24,32 */

void icons_init(void) {
    memset(cache, 0, sizeof cache);
}

static int bucket(int size) {
    if (size <= 12) return 0;
    if (size <= 16) return 1;
    if (size <= 20) return 2;
    if (size <= 24) return 3;
    return 4;
}

GdkPixbuf *icon_get(WedIcon ic, int size) {
    int b = bucket(size);
    if (cache[ic][b]) return g_object_ref(cache[ic][b]);
    int px = size;
    cairo_surface_t *sf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, px, px);
    cairo_t *cr = cairo_create(sf);
    cairo_scale(cr, (double)px / 24.0, (double)px / 24.0);
    /* all icons designed on a 24-unit grid */
    WedColor c = (ic == IC_STAR_FILLED) ? g_theme.accent
                : (ic == IC_SHIELD || ic == IC_AI) ? g_theme.accent_dark
                : g_theme.text_primary;
    DRAWERS[ic](cr, 24.0, c, g_theme.dark);
    cairo_destroy(cr);
    GdkPixbuf *pb = gdk_pixbuf_get_from_surface(sf, 0, 0, px, px);
    cairo_surface_destroy(sf);
    cache[ic][b] = pb;
    return g_object_ref(pb);
}

GdkPixbuf *icon_get_scaled(WedIcon ic, int w, int h) {
    GdkPixbuf *src = icon_get(ic, w > h ? w : h);
    GdkPixbuf *dst = gdk_pixbuf_scale_simple(src, w, h, GDK_INTERP_BILINEAR);
    g_object_unref(src);
    return dst;
}

cairo_surface_t *icon_surface(WedIcon ic, int size) {
    GdkPixbuf *pb = icon_get(ic, size);
    cairo_surface_t *sf = gdk_cairo_surface_create_from_pixbuf(pb, 1, NULL);
    g_object_unref(pb);
    return sf;
}
