/* privacydash.c — the privacy dashboard popover: global counters,
 * current-site shield switch, top blocked hosts, quick links. */
#include "wed.h"

static GtkWidget *lbl_ads, *lbl_trackers, *lbl_pages, *lbl_site;
static GtkWidget *sw_shields;
static char *cur_host = NULL;

static void refresh_labels(WedBrowser *b) {
    unsigned long ads = 0, tr = 0, pages = 0, bytes = 0;
    wed_stats(&ads, &tr, &pages, &bytes);
    char *a = g_strdup_printf("%lu", ads);
    char *t = g_strdup_printf("%lu", tr);
    char *p = g_strdup_printf("%lu", pages);
    gtk_label_set_text(GTK_LABEL(lbl_ads), a);
    gtk_label_set_text(GTK_LABEL(lbl_trackers), t);
    gtk_label_set_text(GTK_LABEL(lbl_pages), p);
    g_free(a); g_free(t); g_free(p);
    (void)b;
}

static void on_shield_toggle(GtkSwitch *sw, gboolean state, WedBrowser *b) {
    WedTab *t = browser_active_tab(b);
    if (t && t->url && cur_host && *cur_host) {
        wed_set_shields(cur_host, state);
        webview_apply_privacy(t);
    }
    browser_update_shield_ui(b);
}

static void on_open_site_settings(GtkWidget *w, WedBrowser *b) {
    (void)w;
    settings_window_open(b);
}

GtkWidget *privacy_popover_new(WedBrowser *b) {
    GtkWidget *pop = gtk_popover_new(b->btn_shield);
    b->privacy_popover = pop;
    gtk_popover_set_position(GTK_POPOVER(pop), GTK_POS_BOTTOM);
    gtk_widget_set_size_request(pop, 380, -1);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);

    /* header */
    GtkWidget *hdr = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(hdr),
        "<span size='large' weight='bold'>Privacy dashboard</span>");
    gtk_widget_set_halign(hdr, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), hdr, FALSE, FALSE, 0);

    /* current site + shield switch */
    GtkWidget *site_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    lbl_site = gtk_label_new("");
    gtk_widget_set_halign(lbl_site, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(site_row), lbl_site, TRUE, TRUE, 0);
    sw_shields = gtk_switch_new();
    gtk_box_pack_start(GTK_BOX(site_row), sw_shields, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), site_row, FALSE, FALSE, 0);
    g_signal_connect(sw_shields, "state-set", G_CALLBACK(on_shield_toggle), b);

    /* stats grid (Chrome Material: thin large numerals + secondary labels) */
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 24);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 2);
    gtk_widget_set_margin_top(grid, 6);
    const char *labels[] = { "Ads blocked", "Trackers blocked", "Pages loaded" };
    GtkWidget *stats[] = { NULL, NULL, NULL };
    for (int i = 0; i < 3; i++) {
        GtkWidget *l = gtk_label_new("0");
        gtk_widget_set_name(l, "statbig");
        gtk_widget_set_halign(l, GTK_ALIGN_START);
        GtkWidget *d = gtk_label_new(labels[i]);
        gtk_widget_set_name(d, "statlbl");
        gtk_widget_set_halign(d, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), l, i, 0, 1, 1);
        gtk_grid_attach(GTK_GRID(grid), d, i, 1, 1, 1);
        stats[i] = l;
    }
    lbl_ads = stats[0];
    lbl_trackers = stats[1];
    lbl_pages = stats[2];
    gtk_box_pack_start(GTK_BOX(vbox), grid, FALSE, FALSE, 8);

    /* top blocked hosts (Chrome row style) */
    GtkWidget *top_hdr = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(top_hdr),
        "<span size='medium' weight='bold'>Most blocked</span>");
    gtk_widget_set_halign(top_hdr, GTK_ALIGN_START);
    gtk_widget_set_margin_top(top_hdr, 8);
    gtk_box_pack_start(GTK_BOX(vbox), top_hdr, FALSE, FALSE, 0);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll, -1, 140);
    GtkWidget *list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_container_add(GTK_CONTAINER(scroll), list);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);
    g_object_set_data(G_OBJECT(vbox), "wed-blocklist", list);

    GtkWidget *btn = gtk_button_new_with_label("Site permissions & exceptions…");
    g_signal_connect(btn, "clicked", G_CALLBACK(on_open_site_settings), b);
    gtk_box_pack_start(GTK_BOX(vbox), btn, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(pop), vbox);
    gtk_widget_show_all(vbox);
    gtk_widget_hide(pop);
    return pop;
}

void privacy_refresh(WedBrowser *b) {
    refresh_labels(b);
    WedTab *t = browser_active_tab(b);
    g_free(cur_host);
    cur_host = NULL;
    gboolean on = TRUE;
    if (t && t->url) {
        cur_host = host_from_uri(t->url);
        if (cur_host && *cur_host) {
            on = wed_shields_enabled_for(cur_host);
            char *s = g_strdup_printf("<b>%s</b>\n<span size='small' color='%s'>%s</span>",
                cur_host,
                g_theme.dark ? "gray" : "#5f6368",
                on ? "Shields are protecting you on this site"
                   : "Shields are OFF on this site");
            gtk_label_set_markup(GTK_LABEL(lbl_site), s);
            g_free(s);
        }
    }
    gtk_switch_set_active(GTK_SWITCH(sw_shields), on);

    /* blocked list */
    GtkWidget *list = g_object_get_data(G_OBJECT(gtk_bin_get_child(GTK_BIN(b->privacy_popover))),
        "wed-blocklist");
    if (list) {
        GList *ch = gtk_container_get_children(GTK_CONTAINER(list));
        for (GList *i = ch; i; i = i->next)
            gtk_container_remove(GTK_CONTAINER(list), i->data);
        g_list_free(ch);
        char *json = wed_site_stats_json();
        int n = json_count(json);
        for (int i = 0; i < n && i < 10; i++) {
            char *h = json_array_str(json, i, "host");
            double ads = json_array_num(json, i, "ads");
            double tr = json_array_num(json, i, "trackers");
            if (h) {
                GtkWidget *r = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
                gtk_widget_set_name(r, "panelrow");
                GtkWidget *hl = gtk_label_new(h);
                gtk_widget_set_name(hl, "panelrowtitle");
                gtk_label_set_ellipsize(GTK_LABEL(hl), PANGO_ELLIPSIZE_END);
                gtk_label_set_xalign(GTK_LABEL(hl), 0.0);
                gtk_box_pack_start(GTK_BOX(r), hl, TRUE, TRUE, 0);
                char *cnt = g_strdup_printf("%d", (int)(ads + tr));
                GtkWidget *cl = gtk_label_new(cnt);
                gtk_widget_set_name(cl, "secondary");
                gtk_box_pack_end(GTK_BOX(r), cl, FALSE, FALSE, 8);
                gtk_container_add(GTK_CONTAINER(list), r);
                g_free(cnt);
                free(h);
            }
        }
        free(json);
        gtk_widget_show_all(list);
    }
    refresh_labels(b);
}
