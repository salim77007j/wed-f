/* omnibox.c — the smart address bar: pill shape (Chrome 153 style),
 * security indicator, live suggestions from history + bookmarks +
 * search, direct URL entry. */
#include "wed.h"

/* pill drawing: rounded rect fill + border; focus ring in accent */
static gboolean pill_draw(GtkWidget *w, cairo_t *cr, gpointer ud) {
    (void)ud;
    GtkAllocation al;
    gtk_widget_get_allocation(w, &al);
    double h = al.height, wdt = al.width, r = h / 2.0;
    gboolean focused = gtk_widget_has_focus(gtk_bin_get_child(GTK_BIN(w)));

    WedColor fill = focused ? g_theme.omnibox_fill_focused : g_theme.omnibox_fill;
    cairo_new_sub_path(cr);
    cairo_arc(cr, r, h / 2, r, M_PI / 2, 3 * M_PI / 2);
    cairo_line_to(cr, wdt - r, 0);
    cairo_arc(cr, wdt - r, h / 2, r, -M_PI / 2, M_PI / 2);
    cairo_close_path(cr);
    cairo_set_source_rgb(cr, fill.r, fill.g, fill.b);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, g_theme.omnibox_border.r, g_theme.omnibox_border.g,
                         g_theme.omnibox_border.b);
    cairo_set_line_width(cr, 1);
    cairo_stroke(cr);
    if (focused) {
        cairo_set_source_rgb(cr, g_theme.accent.r, g_theme.accent.g, g_theme.accent.b);
        cairo_set_line_width(cr, 2);
        cairo_stroke(cr);
    }
    return FALSE;
}

/* security icon drawn at left inside the pill */
typedef struct {
    GtkWidget *win;      /* the pill (GtkEventBox) */
    GtkWidget *entry;
    GtkWidget *seclbl;   /* lock / info icon */
    GtkWidget *star;
    WedBrowser *b;
} OmniCtx;
static OmniCtx omni;

static void set_star(WedBrowser *b) {
    WedTab *t = browser_active_tab(b);
    gboolean marked = t && t->url && wed_is_bookmarked(t->url);
    gtk_image_set_from_pixbuf(GTK_IMAGE(omni.star),
        icon_get(marked ? IC_STAR_FILLED : IC_STAR, 15));
}

static void on_star(GtkWidget *w, WedBrowser *b) {
    (void)w;
    WedTab *t = browser_active_tab(b);
    if (!t || !t->url || !*t->url) return;
    if (wed_is_bookmarked(t->url)) {
        wed_bookmark_remove(t->url);
    } else {
        wed_bookmark_add(t->url, t->title ? t->title : "", "root");
    }
    set_star(b);
}

/* ---------------------------------------------------------------- navigate */
static void omnibox_navigate(WedBrowser *b, const char *text) {
    if (!text || !*text) return;
    g_free(b->sugg_typed);
    b->sugg_typed = NULL;
    if (b->sugg_popover && gtk_widget_is_visible(b->sugg_popover))
        gtk_widget_hide(b->sugg_popover);

    char *url;
    if (looks_like_url(text)) {
        if (strstr(text, "://")) url = g_strdup(text);
        else url = g_strdup_printf("https://%s", text);
    } else {
        url = search_url_for(text);
    }
    browser_load_url(b, url);
    g_free(url);
}

/* ---------------------------------------------------------------- suggestions */
typedef struct {
    char *title, *url, *kind;   /* history | bookmark | search | url */
} Sugg;

static void sugg_free(Sugg *s) { g_free(s->title); g_free(s->url); g_free(s->kind); g_free(s); }

static void on_sugg_activate(GtkWidget *row, WedBrowser *b) {
    Sugg *s = g_object_get_data(G_OBJECT(row), "sugg");
    if (!s) return;
    if (!strcmp(s->kind, "search")) {
        char *u = search_url_for(s->title);
        browser_load_url(b, u);
        g_free(u);
    } else {
        browser_load_url(b, s->url);
    }
    if (b->sugg_popover) gtk_widget_hide(b->sugg_popover);
    gtk_entry_set_text(GTK_ENTRY(omni.entry), s->kind && strcmp(s->kind, "search") == 0
        ? s->title : s->url);
}

static GtkWidget *sugg_row_new(WedBrowser *b, Sugg *s) {
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_name(row, "suggrow");
    WedIcon ic = !strcmp(s->kind, "search") ? IC_SEARCH
               : !strcmp(s->kind, "bookmark") ? IC_BOOKMARK
               : !strcmp(s->kind, "history") ? IC_HISTORY : IC_TAB;
    GtkWidget *img = gtk_image_new_from_pixbuf(icon_get(ic, 14));
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *t = gtk_label_new(s->title);
    gtk_widget_set_name(t, "suggtitle");
    gtk_label_set_ellipsize(GTK_LABEL(t), PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign(GTK_LABEL(t), 0.0);
    GtkWidget *u = gtk_label_new(s->url && *s->url ? s->url : "");
    gtk_widget_set_name(u, "suggurl");
    gtk_label_set_ellipsize(GTK_LABEL(u), PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign(GTK_LABEL(u), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), t, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), u, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(row), img, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(row), vbox, TRUE, TRUE, 0);

    gtk_widget_set_events(row, GDK_BUTTON_PRESS_MASK);
    g_object_set_data_full(G_OBJECT(row), "sugg", s, (GDestroyNotify)sugg_free);
    g_signal_connect(row, "button-press-event", G_CALLBACK(on_sugg_activate), b);
    GtkWidget *ev = gtk_event_box_new();
    gtk_container_add(GTK_CONTAINER(ev), row);
    return ev;
}

static void suggestions_rebuild(WedBrowser *b, const char *text) {
    if (!b->sugg_popover) return;
    GPtrArray *rows = b->sugg_rows;
    for (guint i = 0; i < rows->len; i++)
        gtk_container_remove(GTK_CONTAINER(b->sugg_list), g_ptr_array_index(rows, i));
    g_ptr_array_set_size(rows, 0);

    if (!text || !*text) { gtk_widget_hide(b->sugg_popover); return; }

    gboolean seen_hist = FALSE, seen_bm = FALSE;
    /* bookmarks first (like Chrome) */
    char *bmj = wed_bookmarks_json("root");
    for (int i = 0; i < json_count(bmj) && rows->len < 3; i++) {
        char *u = json_array_str(bmj, i, "url");
        char *ti = json_array_str(bmj, i, "title");
        if (u && (g_strstr_len(u, -1, text) || (ti && g_strstr_len(ti, -1, text)))) {
            Sugg *s = g_new0(Sugg, 1);
            s->kind = g_strdup("bookmark");
            s->title = g_strdup(ti && *ti ? ti : u);
            s->url = g_strdup(u);
            GtkWidget *r = sugg_row_new(b, s);
            gtk_container_add(GTK_CONTAINER(b->sugg_list), r);
            g_ptr_array_add(rows, r);
            seen_bm = TRUE;
        }
        g_free(u); g_free(ti);
    }
    free(bmj);

    char *hj = wed_history_json(60, text);
    for (int i = 0; i < json_count(hj) && rows->len < 8; i++) {
        char *u = json_array_str(hj, i, "url");
        char *ti = json_array_str(hj, i, "title");
        if (u) {
            Sugg *s = g_new0(Sugg, 1);
            s->kind = g_strdup("history");
            s->title = ti && *ti ? ti : u;
            s->url = u;
            GtkWidget *r = sugg_row_new(b, s);
            gtk_container_add(GTK_CONTAINER(b->sugg_list), r);
            g_ptr_array_add(rows, r);
            seen_hist = TRUE;
        } else g_free(u);
        g_free(ti);
    }
    free(hj);
    (void)seen_hist; (void)seen_bm;

    /* search suggestion (always last, always present for non-URL text) */
    if (!looks_like_url(text)) {
        Sugg *s = g_new0(Sugg, 1);
        s->kind = g_strdup("search");
        s->title = g_strdup(text);
        s->url = g_strdup("");
        GtkWidget *r = sugg_row_new(b, s);
        gtk_container_add(GTK_CONTAINER(b->sugg_list), r);
        g_ptr_array_add(rows, r);
    }

    if (rows->len) {
        gtk_widget_show_all(b->sugg_list);
        gtk_widget_show_all(b->sugg_popover);
    } else {
        gtk_widget_hide(b->sugg_popover);
    }
}

/* ---------------------------------------------------------------- entry events */
static gboolean on_entry_key(GtkWidget *e, GdkEventKey *ev, WedBrowser *b) {
    if (ev->keyval == GDK_KEY_Return || ev->keyval == GDK_KEY_KP_Enter) {
        omnibox_navigate(b, gtk_entry_get_text(GTK_ENTRY(e)));
        return TRUE;
    }
    if (ev->keyval == GDK_KEY_Escape) {
        omnibox_update(b);
        gtk_widget_grab_focus(GTK_WIDGET(gtk_bin_get_child(GTK_BIN(b->omnibox))));
    }
    return FALSE;
}

static void on_entry_changed(GtkWidget *e, WedBrowser *b) {
    const char *text = gtk_entry_get_text(GTK_ENTRY(e));
    if (!g_strcmp0(text, b->sugg_typed)) return;
    g_free(b->sugg_typed);
    b->sugg_typed = g_strdup(text);
    suggestions_rebuild(b, text);
}

static gboolean on_entry_focus_out(GtkWidget *e, GdkEventFocus *ev, WedBrowser *b) {
    (void)e; (void)ev;
    if (b->sugg_popover && gtk_widget_is_visible(b->sugg_popover))
        g_timeout_add(150, (GSourceFunc)gtk_widget_hide, b->sugg_popover);
    omnibox_update(b);
    return FALSE;
}

static gboolean pill_button_press(GtkWidget *w, GdkEventButton *ev, gpointer ud) {
    /* click anywhere in the pill focuses the entry (Chrome behavior) */
    (void)w; (void)ud;
    if (ev->type == GDK_BUTTON_PRESS && ev->button == 1) {
        gtk_widget_grab_focus(omni.entry);
    }
    return FALSE;
}

/* ---------------------------------------------------------------- build */
GtkWidget *omnibox_new(WedBrowser *b) {
    omni.b = b;
    omni.win = gtk_event_box_new();
    gtk_widget_set_name(omni.win, "omniwrap");
    gtk_widget_set_events(omni.win, GDK_BUTTON_PRESS_MASK);
    gtk_widget_set_size_request(omni.win, 200, 32);
    gtk_widget_set_valign(omni.win, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand(omni.win, TRUE);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 0);

    /* security icon */
    GtkWidget *secbtn = gtk_button_new();
    gtk_widget_set_name(secbtn, "toolbtn");
    GtkWidget *secimg = gtk_image_new_from_pixbuf(icon_get(IC_INFO, 14));
    gtk_container_add(GTK_CONTAINER(secbtn), secimg);
    gtk_box_pack_start(GTK_BOX(hbox), secbtn, FALSE, FALSE, 4);
    omni.seclbl = secimg;

    /* entry */
    omni.entry = gtk_entry_new();
    gtk_widget_set_name(omni.entry, "omnibox");
    gtk_entry_set_placeholder_text(GTK_ENTRY(omni.entry), "Search or enter address");
    gtk_widget_set_hexpand(omni.entry, TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), omni.entry, TRUE, TRUE, 0);
    b->omnibox_entry = omni.entry;

    /* star */
    omni.star = gtk_image_new_from_pixbuf(icon_get(IC_STAR, 15));
    GtkWidget *starbtn = gtk_button_new();
    gtk_widget_set_name(starbtn, "toolbtn");
    gtk_container_add(GTK_CONTAINER(starbtn), omni.star);
    gtk_box_pack_start(GTK_BOX(hbox), starbtn, FALSE, FALSE, 4);
    b->btn_star = starbtn;

    gtk_container_add(GTK_CONTAINER(omni.win), hbox);
    g_signal_connect_after(omni.win, "draw", G_CALLBACK(pill_draw), NULL);
    g_signal_connect(omni.win, "button-press-event", G_CALLBACK(pill_button_press), NULL);
    g_signal_connect(omni.entry, "key-press-event", G_CALLBACK(on_entry_key), b);
    g_signal_connect(omni.entry, "changed", G_CALLBACK(on_entry_changed), b);
    g_signal_connect(omni.entry, "focus-out-event", G_CALLBACK(on_entry_focus_out), b);
    g_signal_connect(starbtn, "clicked", G_CALLBACK(on_star), b);

    /* suggestions popover */
    b->sugg_rows = g_ptr_array_new();
    b->sugg_popover = gtk_popover_new(omni.win);
    gtk_popover_set_position(GTK_POPOVER(b->sugg_popover), GTK_POS_BOTTOM);
    GtkWidget *sbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    b->sugg_list = sbox;
    gtk_container_set_border_width(GTK_CONTAINER(sbox), 4);
    gtk_container_add(GTK_CONTAINER(b->sugg_popover), sbox);

    b->omnibox = omni.win;
    gtk_widget_show_all(omni.win);
    gtk_widget_hide(b->sugg_popover);
    return omni.win;
}

void omnibox_update(WedBrowser *b) {
    WedTab *t = browser_active_tab(b);
    if (!t) return;
    if (gtk_widget_has_focus(omni.entry)) return;
    const char *uri = t->url ? t->url : "";
    gtk_entry_set_text(GTK_ENTRY(omni.entry),
        g_str_has_prefix(uri, "wed://start") ? "" : uri);

    /* security indicator */
    gboolean secure = g_str_has_prefix(uri, "https://");
    gtk_image_set_from_pixbuf(GTK_IMAGE(omni.seclbl),
        icon_get(secure ? IC_LOCK : IC_INFO, 13));
    set_star(b);
}

void omnibox_focus(WedBrowser *b) {
    gtk_widget_grab_focus(omni.entry);
    gtk_editable_select_region(GTK_EDITABLE(omni.entry), 0, -1);
    gtk_widget_show_all(b->sugg_popover);
    gtk_widget_hide(b->sugg_popover);
}
