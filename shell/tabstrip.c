/* tabstrip.c — Chrome-style tab strip: rounded-top active tabs on a tinted
 * strip, close-on-hover X, favicon + ellipsized title, pinned tabs,
 * middle-click close, context menu, spinner while loading, mute indicator. */
#include "wed.h"

#define TAB_MAX_W 240
#define TAB_MIN_W 56
#define TAB_PIN_W 44
#define STRIP_H   40
#define RADIUS    10

typedef struct {
    WedBrowser *b;
    int index;
    GtkWidget *evbox;          /* event box: input region */
    GtkWidget *inner;          /* drawing area */
    GtkWidget *lbl;            /* title (label for a11y + text) */
    GtkWidget *spinner;
    GtkWidget *icon;           /* favicon or muted icon */
    GtkWidget *closebtn;
    gboolean hover, close_hover;
} TabUI;

static GPtrArray *tab_uis = NULL;   /* parallel to b->tabs */
static GtkWidget *strip;         /* the horizontal box */
static GtkWidget *plus_btn;
static WedBrowser *strip_b;

static int tab_width(WedTab *t) {
    return t->pinned ? TAB_PIN_W : TAB_MAX_W;
}

/* Chrome-style: tabs share the strip but cap at TAB_MAX_W. */
static int compute_tab_width(WedBrowser *b) {
    GtkAllocation al;
    gtk_widget_get_allocation(strip, &al);
    int avail = al.width > 40 ? al.width - 60 : 940;
    int n = (int)b->tabs->len;
    if (n <= 0) return TAB_MAX_W;
    int w = avail / n;
    return CLAMP(w, TAB_MIN_W, TAB_MAX_W);
}

static gboolean tab_draw(GtkWidget *w, cairo_t *cr, TabUI *ui) {
    WedBrowser *b = ui->b;
    int idx = ui->index;
    if (idx < 0 || idx >= (int)b->tabs->len) return FALSE;
    WedTab *t = g_ptr_array_index(b->tabs, idx);
    gboolean active = b->active == idx;
    GtkAllocation al;
    gtk_widget_get_allocation(w, &al);
    int width = al.width, height = al.height;

    if (!gtk_cairo_should_draw_window(cr, gtk_widget_get_window(w))) {
        /* GTK3 double buffering: still paint the content */
    }

    WedColor bg;
    if (active) {
        bg = g_theme.tab_active;
    } else if (ui->hover) {
        bg = g_theme.dark ? (WedColor){0.16,0.17,0.20} : (WedColor){0.88,0.91,0.97};
        bg = g_theme.tabstrip_bg; /* tint base; hover adds overlay below */
    } else {
        bg = g_theme.tabstrip_bg;
    }

    /* rounded-top tab shape */
    double r = RADIUS;
    cairo_new_path(cr);
    cairo_move_to(cr, 0, height);
    cairo_line_to(cr, 0, r);
    if (active) {
        cairo_curve_to(cr, 0, r * 0.4, r * 0.4, 0, r, 0);
        cairo_line_to(cr, width - r, 0);
        cairo_curve_to(cr, width - r * 0.4, 0, width, r * 0.4, width, r);
    } else {
        cairo_line_to(cr, 0, 0);
        cairo_line_to(cr, width, 0);
        cairo_line_to(cr, width, r);
    }
    cairo_line_to(cr, width, height);
    cairo_close_path(cr);
    cairo_set_source_rgb(cr, bg.r, bg.g, bg.b);
    cairo_fill(cr);

    if (active) {
        /* accent underline */
        cairo_set_source_rgb(cr, g_theme.accent.r, g_theme.accent.g, g_theme.accent.b);
        cairo_rectangle(cr, 4, height - 3, width - 8, 3);
        cairo_fill(cr);
    } else if (ui->hover) {
        cairo_set_source_rgba(cr, 1, 1, 1, g_theme.dark ? 0.06 : 0.35);
        cairo_paint(cr);
    }

    /* separator between inactive tabs */
    if (!active && idx > 0) {
        cairo_set_source_rgba(cr, g_theme.tab_inactive_text.r,
                              g_theme.tab_inactive_text.g,
                              g_theme.tab_inactive_text.b, 0.25);
        cairo_rectangle(cr, 0, 8, 1, height - 16);
        cairo_fill(cr);
    }
    return FALSE;
}

static void tab_close_click(GtkWidget *btn, TabUI *ui) {
    (void)btn;
    browser_close_tab(ui->b, ui->index);
}

static gboolean tab_button_press(GtkWidget *w, GdkEventButton *ev, TabUI *ui) {
    WedBrowser *b = ui->b;
    if (ev->type == GDK_BUTTON_PRESS) {
        if (ev->button == 2) {           /* middle click → close */
            browser_close_tab(b, ui->index);
            return TRUE;
        }
        if (ev->button == 3) {           /* right click → menu */
            menu_popup_tab_menu(b, ui->index, ev);
            return TRUE;
        }
        if (ev->button == 1) {
            browser_switch_tab(b, ui->index);
            return FALSE;
        }
    }
    (void)w;
    return FALSE;
}

static gboolean tab_motion(GtkWidget *w, GdkEventMotion *ev, TabUI *ui) {
    gboolean was = ui->hover;
    GtkAllocation al;
    gtk_widget_get_allocation(w, &al);
    ui->hover = ev->x >= 0 && ev->x < al.width && ev->y >= 0 && ev->y < al.height;
    if (was != ui->hover) gtk_widget_queue_draw(w);
    return FALSE;
}

static gboolean tab_leave(GtkWidget *w, GdkEventCrossing *ev, TabUI *ui) {
    (void)ev;
    ui->hover = FALSE;
    gtk_widget_queue_draw(w);
    return FALSE;
}

static TabUI *tab_ui_new(WedBrowser *b, int index) {
    TabUI *ui = g_new0(TabUI, 1);
    ui->b = b;
    ui->index = index;

    ui->evbox = gtk_event_box_new();
    gtk_widget_set_events(ui->evbox, GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK
        | GDK_POINTER_MOTION_MASK | GDK_LEAVE_NOTIFY_MASK | GDK_SCROLL_MASK);

    ui->inner = gtk_drawing_area_new();

    /* content overlay: icon + title + close */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_margin_start(hbox, 8);
    gtk_widget_set_margin_end(hbox, 2);
    gtk_widget_set_valign(hbox, GTK_ALIGN_CENTER);

    ui->icon = gtk_image_new();
    gtk_widget_set_size_request(ui->icon, 16, 16);
    gtk_box_pack_start(GTK_BOX(hbox), ui->icon, FALSE, FALSE, 0);

    ui->spinner = gtk_spinner_new();
    gtk_widget_set_size_request(ui->spinner, 14, 14);
    gtk_box_pack_start(GTK_BOX(hbox), ui->spinner, FALSE, FALSE, 0);

    ui->lbl = gtk_label_new("New tab");
    gtk_label_set_ellipsize(GTK_LABEL(ui->lbl), PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign(GTK_LABEL(ui->lbl), 0.0);
    gtk_widget_set_name(ui->lbl, "tablbl");
    gtk_box_pack_start(GTK_BOX(hbox), ui->lbl, TRUE, TRUE, 0);

    ui->closebtn = gtk_button_new();
    gtk_widget_set_name(ui->closebtn, "toolbtn");
    GtkWidget *cimg = gtk_image_new_from_pixbuf(icon_get(IC_CLOSE, 13));
    gtk_container_add(GTK_CONTAINER(ui->closebtn), cimg);
    gtk_box_pack_start(GTK_BOX(hbox), ui->closebtn, FALSE, FALSE, 0);
    g_signal_connect(ui->closebtn, "clicked", G_CALLBACK(tab_close_click), ui);

    /* overlay inner drawing + hbox */
    GtkWidget *overlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(overlay), ui->inner);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), hbox);
    gtk_widget_set_halign(hbox, GTK_ALIGN_FILL);
    gtk_container_add(GTK_CONTAINER(ui->evbox), overlay);
    gtk_widget_set_size_request(ui->inner, TAB_MIN_W, STRIP_H);

    g_signal_connect(ui->evbox, "button-press-event", G_CALLBACK(tab_button_press), ui);
    g_signal_connect(ui->evbox, "motion-notify-event", G_CALLBACK(tab_motion), ui);
    g_signal_connect(ui->evbox, "leave-notify-event", G_CALLBACK(tab_leave), ui);
    g_signal_connect(ui->inner, "draw", G_CALLBACK(tab_draw), ui);

    gtk_widget_set_size_request(ui->evbox, TAB_MIN_W, STRIP_H);
    gtk_widget_show_all(ui->evbox);
    return ui;
}

static void on_plus(GtkWidget *b) { (void)b; browser_add_tab(strip_b, NULL); }

GtkWidget *tabstrip_new(WedBrowser *b) {
    strip_b = b;
    tab_uis = g_ptr_array_new();
    strip = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_name(strip, "tabstrip");
    gtk_widget_set_size_request(strip, -1, STRIP_H);
    gtk_widget_set_valign(strip, GTK_ALIGN_START);

    GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(strip), spacer, FALSE, FALSE, 8);

    plus_btn = gtk_button_new();
    gtk_widget_set_name(plus_btn, "toolbtn");
    GtkWidget *pimg = gtk_image_new_from_pixbuf(icon_get(IC_PLUS, 15));
    gtk_container_add(GTK_CONTAINER(plus_btn), pimg);
    gtk_widget_set_tooltip_text(plus_btn, "New tab (Ctrl+T)");
    gtk_box_pack_end(GTK_BOX(strip), plus_btn, FALSE, FALSE, 8);
    g_signal_connect(plus_btn, "clicked", G_CALLBACK(on_plus), NULL);

    gtk_widget_show_all(strip);
    return strip;
}

void tabstrip_update(WedBrowser *b) {
    if (!tab_uis) return;
    /* sync UI array length to tabs */
    while (tab_uis->len < b->tabs->len) {
        TabUI *ui = tab_ui_new(b, tab_uis->len);
        g_ptr_array_add(tab_uis, ui);
        gtk_box_pack_start(GTK_BOX(strip), ui->evbox, FALSE, FALSE, 0);
    }
    while (tab_uis->len > b->tabs->len) {
        TabUI *ui = g_ptr_array_index(tab_uis, tab_uis->len - 1);
        gtk_container_remove(GTK_CONTAINER(strip), ui->evbox);
        g_ptr_array_remove_index(tab_uis, tab_uis->len - 1);
        g_free(ui);
    }
    /* update contents + indices */
    for (guint i = 0; i < tab_uis->len; i++) {
        TabUI *ui = g_ptr_array_index(tab_uis, i);
        ui->index = i;
        WedTab *t = g_ptr_array_index(b->tabs, i);
        gboolean active = b->active == (int)i;

        const char *title = t->title && *t->title ? t->title : "New tab";
        if (t->muted && !t->pinned) {
            char *m = g_strdup_printf("%s  (muted)", title);
            gtk_label_set_text(GTK_LABEL(ui->lbl), m);
            g_free(m);
        } else {
            gtk_label_set_text(GTK_LABEL(ui->lbl), title);
        }

        /* icon: favicon / spinner / muted */
        if (t->loading) {
            gtk_spinner_start(GTK_SPINNER(ui->spinner));
            gtk_widget_set_visible(ui->spinner, TRUE);
            gtk_widget_set_visible(ui->icon, FALSE);
        } else {
            gtk_spinner_stop(GTK_SPINNER(ui->spinner));
            gtk_widget_set_visible(ui->spinner, FALSE);
            if (t->muted) {
                gtk_image_set_from_pixbuf(GTK_IMAGE(ui->icon), icon_get(IC_VOLUME_MUTED, 14));
                gtk_widget_set_visible(ui->icon, TRUE);
            } else if (t->favicon) {
                gtk_image_set_from_pixbuf(GTK_IMAGE(ui->icon), t->favicon);
                gtk_widget_set_visible(ui->icon, TRUE);
            } else {
                gtk_widget_set_visible(ui->icon, FALSE);
            }
        }

        gtk_widget_set_visible(ui->lbl, !t->pinned);
        gtk_widget_set_visible(ui->closebtn, !t->pinned || TRUE);
        gtk_widget_set_visible(ui->closebtn, TRUE);

        int w = t->pinned ? TAB_PIN_W : compute_tab_width(b);
        gtk_widget_set_size_request(ui->evbox, w, STRIP_H);
        gtk_widget_set_hexpand(ui->evbox, FALSE);

        /* label color for active/inactive */
        gtk_widget_set_name(ui->lbl, active ? "tablbl" : "tablbl-dim");
        gtk_widget_queue_draw(ui->evbox);
    }
    gtk_widget_queue_draw(strip);
}
