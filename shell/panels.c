/* panels.c — side panels: History (searchable list, delete rows, clear all)
 * and Bookmarks (list, open, remove). Data via wed_core JSON. */
#include "wed.h"

static GtkWidget *search_entry;
static int panel_kind_static;

typedef struct {
    char *url, *title;
} Row;

static void row_free(Row *r) { g_free(r->url); g_free(r->title); g_free(r); }

static void on_open_row(GtkWidget *ev, GdkEventButton *e, gpointer ud) {
    if (e->type == GDK_BUTTON_PRESS && e->button == 1) {
        Row *r = ud;
        WedBrowser *b = g_object_get_data(G_OBJECT(ev), "wed-browser");
        browser_load_url(b, r->url);
    }
}

static void on_remove_row(GtkWidget *btn, gpointer ud) {
    Row *r = ud;
    WedBrowser *b = g_object_get_data(G_OBJECT(btn), "wed-browser");
    if (panel_kind_static == 1) wed_history_remove(r->url);
    else wed_bookmark_remove(r->url);
    panel_refresh(b);
    omnibox_update(b);
}

static GtkWidget *row_widget(WedBrowser *b, Row *r) {
    GtkWidget *ev = gtk_event_box_new();
    gtk_widget_set_events(ev, GDK_BUTTON_PRESS_MASK);
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_name(hbox, "panelrow");

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *t = gtk_label_new(r->title && *r->title ? r->title : r->url);
    gtk_label_set_ellipsize(GTK_LABEL(t), PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign(GTK_LABEL(t), 0.0);
    gtk_widget_set_name(t, "paneltitle");
    GtkWidget *u = gtk_label_new(r->url);
    gtk_label_set_ellipsize(GTK_LABEL(u), PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign(GTK_LABEL(u), 0.0);
    gtk_widget_set_name(u, "panelurl");
    gtk_box_pack_start(GTK_BOX(vbox), t, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), u, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

    GtkWidget *del = wed_image_button(IC_TRASH, "Remove");
    g_object_set_data(G_OBJECT(del), "wed-browser", b);
    g_signal_connect(del, "clicked", G_CALLBACK(on_remove_row), r);
    gtk_box_pack_start(GTK_BOX(hbox), del, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(ev), hbox);
    g_object_set_data(G_OBJECT(ev), "wed-browser", b);
    g_signal_connect(ev, "button-press-event", G_CALLBACK(on_open_row), r);
    return ev;
}

static void on_search(GtkWidget *e, gpointer ud) {
    (void)e;
    WedBrowser *b = ud;
    panel_refresh(b);
}

static void on_clear_history(GtkWidget *btn, gpointer ud) {
    (void)btn;
    WedBrowser *b = ud;
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(b->window), GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, "Clear all browsing history?");
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK) {
        wed_history_clear();
        panel_refresh(b);
    }
    gtk_widget_destroy(d);
}

GtkWidget *panel_new(WedBrowser *b, int kind) {
    panel_kind_static = kind;
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(hdr), 8);
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title),
        kind == 1 ? "<span size='large' weight='bold'>History</span>"
                  : "<span size='large' weight='bold'>Bookmarks</span>");
    gtk_box_pack_start(GTK_BOX(hdr), title, FALSE, FALSE, 0);

    if (kind == 1) {
        GtkWidget *clr = wed_image_button(IC_TRASH, "Clear browsing history");
        g_signal_connect(clr, "clicked", G_CALLBACK(on_clear_history), b);
        gtk_box_pack_end(GTK_BOX(hdr), clr, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(box), hdr, FALSE, FALSE, 0);

    if (kind == 1) {
        search_entry = gtk_entry_new();
        gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "Search history");
        gtk_widget_set_margin_start(search_entry, 8);
        gtk_widget_set_margin_end(search_entry, 8);
        g_signal_connect(search_entry, "changed", G_CALLBACK(on_search), b);
        gtk_box_pack_start(GTK_BOX(box), search_entry, FALSE, FALSE, 4);
    } else {
        search_entry = NULL;
    }

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    GtkWidget *list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(scroll), list);
    gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);

    g_object_set_data(G_OBJECT(box), "wed-list", list);
    g_object_set_data(G_OBJECT(box), "wed-browser", b);

    /* populate immediately */
    const char *query = (kind == 1 && search_entry)
        ? gtk_entry_get_text(GTK_ENTRY(search_entry)) : "";
    char *json = kind == 1 ? wed_history_json(300, query)
                           : wed_bookmarks_json("root");
    int n = json_count(json);
    for (int i = 0; i < n; i++) {
        char *u = json_array_str(json, i, "url");
        char *t = json_array_str(json, i, "title");
        if (u) {
            Row *r = g_new0(Row, 1);
            r->url = u;
            r->title = t ? t : g_strdup("");
            gtk_container_add(GTK_CONTAINER(list), row_widget(b, r));
        } else g_free(t);
    }
    free(json);

    gtk_widget_show_all(box);
    return box;
}

void panel_refresh(WedBrowser *b) {
    if (b->panel_kind == 0) return;
    int kind = b->panel_kind;
    browser_toggle_panel(b, 0);      /* hide */
    b->panel_kind = kind;
    browser_toggle_panel(b, kind);   /* rebuild + show */
}
