/* findbar.c — in-page find bar (Chrome style, top-right overlay). */
#include "wed.h"

static GtkWidget *entry, *btn_prev, *btn_next, *btn_close, *lbl_count;

static void do_find(WedBrowser *b, const char *text, gboolean forward) {
    WedTab *t = browser_active_tab(b);
    if (!t || !t->webview || !text || !*text) return;
    WebKitFindController *fc = webkit_web_view_get_find_controller(
        WEBKIT_WEB_VIEW(t->webview));
    WebKitFindOptions opts = WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE;
    if (wed_settings_bool("find.wrap", TRUE)) opts |= WEBKIT_FIND_OPTIONS_WRAP_AROUND;
    if (!forward) opts |= WEBKIT_FIND_OPTIONS_BACKWARDS;
    webkit_find_controller_search(fc, text, opts, 150);
}

static void on_search_changed(GtkWidget *e, WedBrowser *b) {
    do_find(b, gtk_entry_get_text(GTK_ENTRY(e)), TRUE);
}

static void on_next(GtkWidget *w, WedBrowser *b) { (void)w; do_find(b, gtk_entry_get_text(GTK_ENTRY(entry)), TRUE); }
static void on_prev(GtkWidget *w, WedBrowser *b) { (void)w; do_find(b, gtk_entry_get_text(GTK_ENTRY(entry)), FALSE); }

static void on_close(GtkWidget *w, WedBrowser *b) {
    (void)w;
    findbar_show(b, FALSE);
    WedTab *t = browser_active_tab(b);
    if (t && t->webview) {
        WebKitFindController *fc = webkit_web_view_get_find_controller(
            WEBKIT_WEB_VIEW(t->webview));
        webkit_find_controller_search_finish(fc);
    }
}

static gboolean on_key(GtkWidget *e, GdkEventKey *ev, WedBrowser *b) {
    if (ev->keyval == GDK_KEY_Return) { do_find(b, gtk_entry_get_text(GTK_ENTRY(e)), TRUE); return TRUE; }
    if (ev->keyval == GDK_KEY_Escape) { on_close(e, b); return TRUE; }
    return FALSE;
}

static void on_found(WebKitFindController *fc, guint n, gpointer ud) {
    (void)fc;
    WedBrowser *b = ud;
    char *s = n ? g_strdup_printf("%d match%s", n, n == 1 ? "" : "es")
                : g_strdup("No matches");
    gtk_label_set_text(GTK_LABEL(lbl_count), s);
    g_free(s);
    (void)b;
}

GtkWidget *findbar_new(WedBrowser *b) {
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_name(bar, "findbar");
    gtk_widget_set_size_request(bar, 360, -1);
    gtk_widget_set_margin_top(bar, 6);
    gtk_widget_set_margin_end(bar, 8);

    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Find in page");
    gtk_widget_set_hexpand(entry, TRUE);
    gtk_box_pack_start(GTK_BOX(bar), entry, TRUE, TRUE, 0);

    btn_prev = wed_image_button(IC_BACK, "Previous (Enter=next)");
    btn_next = wed_image_button(IC_FORWARD, "Next");
    btn_close = wed_image_button(IC_CLOSE, "Close (Esc)");
    gtk_box_pack_start(GTK_BOX(bar), btn_prev, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), btn_next, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bar), btn_close, FALSE, FALSE, 0);

    lbl_count = gtk_label_new("");
    gtk_widget_set_name(lbl_count, "secondary");
    gtk_box_pack_start(GTK_BOX(bar), lbl_count, FALSE, FALSE, 6);

    g_signal_connect(entry, "changed", G_CALLBACK(on_search_changed), b);
    g_signal_connect(entry, "key-press-event", G_CALLBACK(on_key), b);
    g_signal_connect(btn_next, "clicked", G_CALLBACK(on_next), b);
    g_signal_connect(btn_prev, "clicked", G_CALLBACK(on_prev), b);
    g_signal_connect(btn_close, "clicked", G_CALLBACK(on_close), b);

    gtk_widget_show_all(bar);
    gtk_widget_hide(bar);
    return bar;
}

void findbar_show(WedBrowser *b, gboolean show) {
    if (show) {
        gtk_widget_show(b->findbar);
        gtk_widget_grab_focus(entry);
    } else {
        gtk_widget_hide(b->findbar);
    }
}

void findbar_search(WedBrowser *b, const char *text, gboolean forward) {
    gtk_entry_set_text(GTK_ENTRY(entry), text);
    do_find(b, text, forward);
}
