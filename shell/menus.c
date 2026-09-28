/* menus.c — ⋮ app menu, tab context menu, page context menu (with AI). */
#include "wed.h"

static void mi_new_tab(GtkWidget *m, WedBrowser *b)   { (void)m; browser_add_tab(b, NULL); }
static void mi_close_tab(GtkWidget *m, WedBrowser *b) { (void)m; browser_close_tab(b, b->active); }
static void mi_history(GtkWidget *m, WedBrowser *b)   { (void)m; browser_toggle_panel(b, 1); }
static void mi_bookmarks(GtkWidget *m, WedBrowser *b) { (void)m; browser_toggle_panel(b, 2); }
static void mi_downloads(GtkWidget *m, WedBrowser *b) { (void)m; browser_show_downloads(b); }
static void mi_find(GtkWidget *m, WedBrowser *b)      { (void)m; browser_toggle_find(b, TRUE); }
static void mi_privacy(GtkWidget *m, WedBrowser *b)   { (void)m; browser_show_privacy(b); }
static void mi_settings(GtkWidget *m, WedBrowser *b)  { (void)m; settings_window_open(b); }
static void mi_quit(GtkWidget *m, WedBrowser *b)      { (void)m; browser_quit(b); }

static void mi_reload_tab(GtkWidget *m, WedBrowser *b) {
    (void)m; WedTab *t = browser_active_tab(b);
    if (t && t->webview) webkit_web_view_reload(WEBKIT_WEB_VIEW(t->webview));
}
static void mi_zoom_in(GtkWidget *m, WedBrowser *b) {
    (void)m; WedTab *t = browser_active_tab(b);
    if (t) webview_zoom(t, t->zoom + 0.1);
}
static void mi_zoom_out(GtkWidget *m, WedBrowser *b) {
    (void)m; WedTab *t = browser_active_tab(b);
    if (t) webview_zoom(t, t->zoom - 0.1);
}
static void mi_zoom_reset(GtkWidget *m, WedBrowser *b) {
    (void)m; WedTab *t = browser_active_tab(b);
    if (t) webview_zoom(t, 1.0);
}
static void mi_print(GtkWidget *m, WedBrowser *b) {
    (void)m; WedTab *t = browser_active_tab(b);
    if (t && t->webview) {
        WebKitPrintOperation *po = webkit_print_operation_new(WEBKIT_WEB_VIEW(t->webview));
        webkit_print_operation_run_dialog(po, GTK_WINDOW(b->window));
        g_object_unref(po);
    }
}
static void mi_save_page(GtkWidget *m, WedBrowser *b) {
    (void)m; WedTab *t = browser_active_tab(b);
    if (!t || !t->webview) return;
    GtkWidget *dlg = gtk_file_chooser_dialog_new("Save page",
        GTK_WINDOW(b->window), GTK_FILE_CHOOSER_ACTION_SAVE,
        "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dlg), TRUE);
    char *sug = t->title ? g_strdup(t->title) : g_strdup("page");
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg), sug);
    g_free(sug);
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char *file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        char *uri = g_filename_to_uri(file, NULL, NULL);
        WebKitSaveMode mode = WEBKIT_SAVE_MODE_MHTML;
        webkit_web_view_save_to_file(WEBKIT_WEB_VIEW(t->webview),
            g_file_new_for_uri(uri), mode, NULL, NULL, NULL);
        g_free(uri);
        g_free(file);
    }
    gtk_widget_destroy(dlg);
}
static void mi_devtools(GtkWidget *m, WedBrowser *b) {
    (void)m; WedTab *t = browser_active_tab(b);
    if (t && t->webview) {
        WebKitWebInspector *insp = webkit_web_view_get_inspector(WEBKIT_WEB_VIEW(t->webview));
        webkit_web_inspector_show(insp);
    }
}
static void mi_pin(GtkWidget *m, WedBrowser *b) {
    (void)m;
    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(m), "tab-idx"));
    if (idx < 0 || idx >= (int)b->tabs->len) return;
    WedTab *t = g_ptr_array_index(b->tabs, idx);
    t->pinned = !t->pinned;
    tabstrip_update(b);
}
static void mi_mute(GtkWidget *m, WedBrowser *b) {
    (void)m;
    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(m), "tab-idx"));
    if (idx < 0 || idx >= (int)b->tabs->len) return;
    WedTab *t = g_ptr_array_index(b->tabs, idx);
    t->muted = !t->muted;
    if (t->webview)
        webkit_web_view_set_is_muted(WEBKIT_WEB_VIEW(t->webview), t->muted);
    tabstrip_update(b);
}
static void mi_duplicate(GtkWidget *m, WedBrowser *b) {
    (void)m;
    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(m), "tab-idx"));
    if (idx < 0 || idx >= (int)b->tabs->len) return;
    WedTab *t = g_ptr_array_index(b->tabs, idx);
    browser_add_tab(b, t->url);
}
static void mi_close_others(GtkWidget *m, WedBrowser *b) {
    (void)m;
    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(m), "tab-idx"));
    for (int i = b->tabs->len - 1; i >= 0; i--) {
        if (i != idx) browser_close_tab_silent(b, i);
    }
    browser_switch_tab(b, 0);
}
static void mi_close_right(GtkWidget *m, WedBrowser *b) {
    (void)m;
    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(m), "tab-idx"));
    for (int i = b->tabs->len - 1; i > idx; i--) {
        browser_close_tab_silent(b, i);
    }
}
static void mi_reopen_closed(GtkWidget *m, WedBrowser *b) {
    (void)m; (void)b;
    /* session-based: restore last saved session in a new tab set is too
       aggressive; offer history panel instead */
    browser_toggle_panel(b, 1);
}
static void mi_bookmark_page(GtkWidget *m, WedBrowser *b) {
    (void)m;
    WedTab *t = browser_active_tab(b);
    if (t && t->url) {
        if (wed_is_bookmarked(t->url)) wed_bookmark_remove(t->url);
        else wed_bookmark_add(t->url, t->title ? t->title : "", "root");
        omnibox_update(b);
    }
}
static void mi_clear_data(GtkWidget *m, WedBrowser *b) {
    (void)m;
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(b->window), GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
        "Clear browsing data?");
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d),
        "Deletes history, cookies, cache and site data now.");
    if (gtk_dialog_run(GTK_DIALOG(d)) == GTK_RESPONSE_OK) {
        WebKitWebsiteDataTypes types = WEBKIT_WEBSITE_DATA_ALL &
            ~WEBKIT_WEBSITE_DATA_DISK_CACHE;
        webkit_website_data_manager_clear(b->data_manager, types, 0, NULL, NULL, NULL);
        wed_history_clear();
    }
    gtk_widget_destroy(d);
}
static void mi_ai_page(GtkWidget *m, WedBrowser *b) {
    (void)m;
    ai_ask_about_page(b);
}

static GtkWidget *menu_item(const char *label, GCallback cb, WedBrowser *b) {
    GtkWidget *mi = gtk_menu_item_new_with_label(label);
    g_signal_connect(mi, "activate", cb, b);
    return mi;
}

static GtkWidget *menu_item_idx(const char *label, GCallback cb, WedBrowser *b, int idx) {
    GtkWidget *mi = gtk_menu_item_new_with_label(label);
    g_object_set_data(G_OBJECT(mi), "tab-idx", GINT_TO_POINTER(idx));
    g_signal_connect(mi, "activate", cb, b);
    return mi;
}

void menu_popup_app_menu(WedBrowser *b, GdkEventButton *ev) {
    GtkWidget *menu = gtk_menu_new();

    gtk_container_add(GTK_CONTAINER(menu), menu_item("New tab  Ctrl+T", G_CALLBACK(mi_new_tab), b));
    gtk_container_add(GTK_CONTAINER(menu), menu_item("Close tab  Ctrl+W", G_CALLBACK(mi_close_tab), b));
    gtk_container_add(GTK_CONTAINER(menu), gtk_separator_menu_item_new());
    GtkWidget *hist = menu_item("History  Ctrl+H", G_CALLBACK(mi_history), b);
    gtk_container_add(GTK_CONTAINER(menu), hist);
    gtk_container_add(GTK_CONTAINER(menu), menu_item("Bookmarks  Ctrl+B", G_CALLBACK(mi_bookmarks), b));
    gtk_container_add(GTK_CONTAINER(menu), menu_item("Downloads  Ctrl+J", G_CALLBACK(mi_downloads), b));
    gtk_container_add(GTK_CONTAINER(menu), gtk_separator_menu_item_new());
    gtk_container_add(GTK_CONTAINER(menu), menu_item("Find…  Ctrl+F", G_CALLBACK(mi_find), b));
    gtk_container_add(GTK_CONTAINER(menu), menu_item("Privacy dashboard", G_CALLBACK(mi_privacy), b));
    gtk_container_add(GTK_CONTAINER(menu), menu_item("Bookmark this page  Ctrl+D", G_CALLBACK(mi_bookmark_page), b));
    if (ai_available())
        gtk_container_add(GTK_CONTAINER(menu), menu_item("Ask AI about this page", G_CALLBACK(mi_ai_page), b));
    gtk_container_add(GTK_CONTAINER(menu), gtk_separator_menu_item_new());
    gtk_container_add(GTK_CONTAINER(menu), menu_item("Save page…", G_CALLBACK(mi_save_page), b));
    gtk_container_add(GTK_CONTAINER(menu), menu_item("Print…  Ctrl+P", G_CALLBACK(mi_print), b));
    gtk_container_add(GTK_CONTAINER(menu), menu_item("Developer tools  F12", G_CALLBACK(mi_devtools), b));
    gtk_container_add(GTK_CONTAINER(menu), menu_item("Clear browsing data…", G_CALLBACK(mi_clear_data), b));
    gtk_container_add(GTK_CONTAINER(menu), gtk_separator_menu_item_new());
    gtk_container_add(GTK_CONTAINER(menu), menu_item("Settings", G_CALLBACK(mi_settings), b));
    gtk_container_add(GTK_CONTAINER(menu), menu_item("Quit", G_CALLBACK(mi_quit), b));

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)ev);
}

void menu_popup_tab_menu(WedBrowser *b, int tab_index, GdkEventButton *ev) {
    GtkWidget *menu = gtk_menu_new();
    gtk_container_add(GTK_CONTAINER(menu),
        menu_item_idx("Reload", G_CALLBACK(mi_reload_tab), b, tab_index));
    gtk_container_add(GTK_CONTAINER(menu),
        menu_item_idx("Duplicate", G_CALLBACK(mi_duplicate), b, tab_index));
    WedTab *t = g_ptr_array_index(b->tabs, tab_index);
    if (t && t->webview) {
        gtk_container_add(GTK_CONTAINER(menu),
            menu_item_idx("Mute site", G_CALLBACK(mi_mute), b, tab_index));
    }
    gtk_container_add(GTK_CONTAINER(menu),
        menu_item_idx(t && t->pinned ? "Unpin tab" : "Pin tab",
            G_CALLBACK(mi_pin), b, tab_index));
    gtk_container_add(GTK_CONTAINER(menu), gtk_separator_menu_item_new());
    gtk_container_add(GTK_CONTAINER(menu),
        menu_item_idx("Close other tabs", G_CALLBACK(mi_close_others), b, tab_index));
    gtk_container_add(GTK_CONTAINER(menu),
        menu_item_idx("Close tabs to the right", G_CALLBACK(mi_close_right), b, tab_index));
    gtk_container_add(GTK_CONTAINER(menu), gtk_separator_menu_item_new());
    gtk_container_add(GTK_CONTAINER(menu),
        menu_item("Reopen closed tab", G_CALLBACK(mi_reopen_closed), b));

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)ev);
}

/* page context menu: WebKit's built-in menu is used; the AI-on-selection
 * item is appended via webview.c's context-menu handler. */
