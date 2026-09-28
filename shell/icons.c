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

static DrawFn DRAWERS[IC_COUNT] = {
    draw_back, draw_fwd, draw_reload, draw_home, draw_shield, draw_shield_off,
    draw_star, draw_star_filled, draw_download, draw_menu, draw_close, draw_search,
    draw_gear, draw_clock, draw_bookmark, draw_plus, draw_volume, draw_volume_muted,
    draw_info, draw_lock, draw_trash, draw_ai, draw_tab, draw_external,
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
