/* toolbar.c — navigation cluster + shield + omnibox pill + star + downloads + ⋮. */
#include "wed.h"

static void on_back(GtkWidget *w, WedBrowser *b)   { (void)w; WedTab *t = browser_active_tab(b); if (t && t->webview) webkit_web_view_go_back(WEBKIT_WEB_VIEW(t->webview)); }
static void on_fwd(GtkWidget *w, WedBrowser *b)    { (void)w; WedTab *t = browser_active_tab(b); if (t && t->webview) webkit_web_view_go_forward(WEBKIT_WEB_VIEW(t->webview)); }
static void on_reload(GtkWidget *w, WedBrowser *b) { (void)w; WedTab *t = browser_active_tab(b); if (t && t->webview) webkit_web_view_reload(WEBKIT_WEB_VIEW(t->webview)); }
static void on_home(GtkWidget *w, WedBrowser *b)   { (void)w; browser_load_url(b, "wed://start"); }

static void on_menu_btn(GtkWidget *w, WedBrowser *b) {
    (void)w;
    GdkEventButton ev = { 0 };
    ev.type = GDK_BUTTON_PRESS;
    menu_popup_app_menu(b, &ev);
}

static void on_shield(GtkWidget *w, WedBrowser *b) {
    (void)w;
    browser_show_privacy(b);
}

static void on_downloads(GtkWidget *w, WedBrowser *b) {
    (void)w;
    browser_show_downloads(b);
}

GtkWidget *wed_image_button(WedIcon ic, const char *tooltip) {
    GtkWidget *btn = gtk_button_new();
    gtk_widget_set_name(btn, "toolbtn");
    GtkWidget *img = gtk_image_new_from_pixbuf(icon_get(ic, 17));
    gtk_container_add(GTK_CONTAINER(btn), img);
    gtk_widget_set_tooltip_text(btn, tooltip);
    return btn;
}

GtkWidget *toolbar_new(WedBrowser *b) {
    GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_name(bar, "toolbar");
    gtk_widget_set_size_request(bar, -1, 46);
    gtk_container_set_border_width(GTK_CONTAINER(bar), 5);

    b->btn_back = wed_image_button(IC_BACK, "Back (Alt+Left)");
    b->btn_fwd = wed_image_button(IC_FORWARD, "Forward (Alt+Right)");
    b->btn_reload = wed_image_button(IC_RELOAD, "Reload (F5, Ctrl+R)");
    b->btn_home = wed_image_button(IC_HOME, "Home");
    gtk_box_pack_start(GTK_BOX(bar), b->btn_back, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(bar), b->btn_fwd, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(bar), b->btn_reload, FALSE, FALSE, 1);
    gtk_box_pack_start(GTK_BOX(bar), b->btn_home, FALSE, FALSE, 1);

    b->btn_shield = wed_image_button(IC_SHIELD, "Privacy shields");
    b->shield_count_lbl = gtk_label_new("");
    gtk_widget_set_name(b->shield_count_lbl, "shieldlbl");
    gtk_box_pack_start(GTK_BOX(bar), b->btn_shield, FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(bar), b->shield_count_lbl, FALSE, FALSE, 0);

    GtkWidget *omni = omnibox_new(b);
    gtk_box_pack_start(GTK_BOX(bar), omni, TRUE, TRUE, 6);

    b->btn_downloads = wed_image_button(IC_DOWNLOAD, "Downloads (Ctrl+J)");
    b->btn_menu = wed_image_button(IC_MENU, "Customize and control WED");
    gtk_box_pack_start(GTK_BOX(bar), b->btn_downloads, FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(bar), b->btn_menu, FALSE, FALSE, 2);

    g_signal_connect(b->btn_back, "clicked", G_CALLBACK(on_back), b);
    g_signal_connect(b->btn_fwd, "clicked", G_CALLBACK(on_fwd), b);
    g_signal_connect(b->btn_reload, "clicked", G_CALLBACK(on_reload), b);
    g_signal_connect(b->btn_home, "clicked", G_CALLBACK(on_home), b);
    g_signal_connect(b->btn_shield, "clicked", G_CALLBACK(on_shield), b);
    g_signal_connect(b->btn_downloads, "clicked", G_CALLBACK(on_downloads), b);
    g_signal_connect(b->btn_menu, "clicked", G_CALLBACK(on_menu_btn), b);

    gtk_widget_show_all(bar);
    return bar;
}
