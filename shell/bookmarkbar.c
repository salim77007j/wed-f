/* bookmarkbar.c — Chrome-style bookmarks bar under the toolbar: favicon +
 * title chips from the real bookmarks DB, "Other bookmarks" folder at the
 * end, middle-click opens in new tab, right-click menu (open/remove). */
#include "wed.h"

static GtkWidget *bm_bar;
static WedBrowser *bm_b;

static void on_open(GtkWidget *w, gpointer ud) {
    (void)w;
    char *url = ud;
    browser_load_url(bm_b, url);
}

static void on_open_new(GtkWidget *w, gpointer ud) {
    (void)w;
    char *url = ud;
    browser_add_tab(bm_b, url);
}

static void on_remove(GtkWidget *w, gpointer ud) {
    (void)w;
    char *url = ud;
    wed_bookmark_remove(url);
    bookmarkbar_refresh(bm_b);
}

static void on_other_bookmarks(GtkWidget *w, gpointer ud) {
    (void)w;
    WedBrowser *b = ud;
    browser_toggle_panel(b, 2);   /* bookmarks panel */
}

static gboolean on_bm_button(GtkWidget *w, GdkEventButton *ev, gpointer ud) {
    char *url = ud;
    if (ev->type == GDK_BUTTON_PRESS && ev->button == 3) {
        GtkWidget *menu = gtk_menu_new();
        GtkWidget *i1 = gtk_menu_item_new_with_label("Open");
        GtkWidget *i2 = gtk_menu_item_new_with_label("Open in new tab");
        GtkWidget *sep = gtk_separator_menu_item_new();
        GtkWidget *i3 = gtk_menu_item_new_with_label("Remove");
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), i1);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), i2);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), sep);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), i3);
        g_signal_connect(i1, "activate", G_CALLBACK(on_open), url);
        g_signal_connect(i2, "activate", G_CALLBACK(on_open_new), url);
        g_signal_connect(i3, "activate", G_CALLBACK(on_remove), url);
        gtk_widget_show_all(menu);
        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)ev);
        return TRUE;
    }
    if (ev->type == GDK_BUTTON_PRESS && ev->button == 2) {
        on_open_new(NULL, url);
        return TRUE;
    }
    return FALSE;   /* let the plain click handler run */
}

static GtkWidget *bm_item_new(WedBrowser *b, const char *title, const char *url) {
    char *url_owned = g_strdup(url);
    GtkWidget *btn = gtk_button_new();
    gtk_widget_set_name(btn, "bmitem");
    /* owns url for the button lifetime — safe to use as signal user data */
    g_object_set_data_full(G_OBJECT(btn), "url", url_owned, g_free);
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_valign(hbox, GTK_ALIGN_CENTER);

    GtkWidget *icon = gtk_image_new();
    gtk_widget_set_size_request(icon, 16, 16);
    char *host = host_from_uri(url);
    /* colored letter glyph when no favicon (fast, always available) */
    GtkWidget *fav = gtk_image_new_from_pixbuf(NULL);
    (void)fav;
    GdkPixbuf *pb = NULL;
    if (b && b->tabs && b->tabs->len) {
        for (guint i = 0; i < b->tabs->len; i++) {
            WedTab *t = g_ptr_array_index(b->tabs, i);
            if (t->favicon && t->url && !strcmp(t->url, url)) { pb = t->favicon; break; }
        }
    }
    if (pb) gtk_image_set_from_pixbuf(GTK_IMAGE(icon), pb);
    else {
        char letter = (title && *title) ? g_ascii_toupper(title[0]) : '?';
        char *txt = g_strdup_printf("<span size='8200' weight='700'>%c</span>", letter);
        GtkWidget *l = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(l), txt);
        g_free(txt);
        gtk_widget_set_size_request(l, 16, 16);
        icon = l;
    }
    (void)host;
    GtkWidget *lbl = gtk_label_new(title && *title ? title : url);
    gtk_widget_set_name(lbl, "bmlbl");
    gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(lbl), 18);

    gtk_box_pack_start(GTK_BOX(hbox), icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), lbl, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(btn), hbox);
    gtk_widget_set_tooltip_text(btn, url);
    g_signal_connect(btn, "clicked", G_CALLBACK(on_open), (gpointer)url_owned);
    g_signal_connect(btn, "button-press-event", G_CALLBACK(on_bm_button), (gpointer)url_owned);
    return btn;
}

GtkWidget *bookmarkbar_new(WedBrowser *b) {
    bm_b = b;
    bm_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    gtk_widget_set_name(bm_bar, "bookmarkbar");
    gtk_widget_set_size_request(bm_bar, -1, 34);
    gtk_container_set_border_width(GTK_CONTAINER(bm_bar), 2);

    bookmarkbar_refresh(b);
    return bm_bar;
}

void bookmarkbar_refresh(WedBrowser *b) {
    if (!bm_bar) return;
    GList *kids = gtk_container_get_children(GTK_CONTAINER(bm_bar));
    for (GList *k = kids; k; k = k->next)
        gtk_container_remove(GTK_CONTAINER(bm_bar), k->data);
    g_list_free(kids);

    char *json = wed_bookmarks_json("root");
    int n = json_count(json);
    int shown = 0;
    for (int i = 0; i < n && shown < 14; i++) {
        char *u = json_array_str(json, i, "url");
        char *t = json_array_str(json, i, "title");
        if (u) {
            GtkWidget *item = bm_item_new(b, t && *t ? t : u, u);
            gtk_box_pack_start(GTK_BOX(bm_bar), item, FALSE, FALSE, 1);
            shown++;
        }
        g_free(u); g_free(t);
    }
    free(json);

    /* "Other bookmarks" folder chip at the end (Chrome pattern) */
    GtkWidget *other = gtk_button_new();
    gtk_widget_set_name(other, "bmitem");
    GtkWidget *ohbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_valign(ohbox, GTK_ALIGN_CENTER);
    GtkWidget *oicon = gtk_image_new_from_pixbuf(icon_get(IC_FOLDER, 14));
    GtkWidget *olbl = gtk_label_new("Other bookmarks");
    gtk_widget_set_name(olbl, "bmlbl");
    gtk_box_pack_start(GTK_BOX(ohbox), oicon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ohbox), olbl, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(other), ohbox);
    gtk_box_pack_end(GTK_BOX(bm_bar), other, FALSE, FALSE, 2);
    g_signal_connect(other, "clicked", G_CALLBACK(on_other_bookmarks), b);

    gtk_widget_show_all(bm_bar);
}
