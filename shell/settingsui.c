/* settingsui.c — Chrome-style settings (per wed45 reference): 256px sidebar
 * with icons + pill-selected active item, "Settings" title + in-page search
 * field, Material rows (icon + title + description + control) separated by
 * hairline dividers. Schema-driven from the Rust core (single source of
 * truth), live-applied. Search filters rows across all sections. */
#include "wed.h"

static GtkWidget *settings_win;
static GtkWidget *stack;
static GPtrArray *widgets;   /* GtkWidget* bound to keys */

typedef struct {
    char *key;
    GtkWidget *w;
    int kind;   /* 0 bool, 1 choice, 2 string, 3 number */
} Bind;

typedef struct {
    char *key, *label, *hint;
    GtkWidget *row;        /* the row container (for search filtering) */
    int section;
} RowMeta;

static GPtrArray *row_metas;  /* RowMeta* for search */

static void bind_free(Bind *b) { g_free(b->key); g_free(b); }
static void rowmeta_free(RowMeta *r) {
    g_free(r->key); g_free(r->label); g_free(r->hint); g_free(r);
}

static void apply_setting(WedBrowser *b, const char *key, const char *val) {
    wed_settings_save(key, val);
    /* live application */
    if (!strcmp(key, "theme.mode") || !strcmp(key, "theme.accent")) {
        theme_init();
        theme_apply();
        icons_init();                    /* icons are theme-colored */
        browser_update_shield_ui(b);
        omnibox_update(b);
    }
    if (!strncmp(key, "hibernate.", 10) || !strncmp(key, "gpu.", 4))
        browser_save_session(b);         /* governor re-reads settings */
}

static void on_switch(GtkSwitch *sw, gboolean state, gpointer ud) {
    Bind *bind = ud;
    WedBrowser *b = g_object_get_data(G_OBJECT(settings_win), "wed-browser");
    apply_setting(b, bind->key, state ? "true" : "false");
}

static void on_combo(GtkComboBox *cb, gpointer ud) {
    Bind *bind = ud;
    WedBrowser *b = g_object_get_data(G_OBJECT(settings_win), "wed-browser");
    char *txt = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(cb));
    if (txt) {
        apply_setting(b, bind->key, txt);
        g_free(txt);
    }
}

static void on_entry_apply(GtkWidget *e, gpointer ud) {
    Bind *bind = ud;
    WedBrowser *b = g_object_get_data(G_OBJECT(settings_win), "wed-browser");
    apply_setting(b, bind->key, gtk_entry_get_text(GTK_ENTRY(e)));
}

static void on_folder(GtkWidget *btn, gpointer ud) {
    Bind *bind = ud;
    WedBrowser *b = g_object_get_data(G_OBJECT(settings_win), "wed-browser");
    GtkWidget *dlg = gtk_file_chooser_dialog_new("Choose folder",
        GTK_WINDOW(settings_win), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "_Cancel", GTK_RESPONSE_CANCEL, "_Select", GTK_RESPONSE_ACCEPT, NULL);
    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        char *path = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        GtkWidget *entry = g_object_get_data(G_OBJECT(btn), "entry");
        gtk_entry_set_text(GTK_ENTRY(entry), path);
        apply_setting(b, bind->key, path);
        g_free(path);
    }
    gtk_widget_destroy(dlg);
}

/* Chrome-style sidebar item: icon + label, pill-selected state */
static GtkWidget *side_item_new(const char *label, WedIcon ic) {
    GtkWidget *btn = gtk_button_new();
    gtk_widget_set_name(btn, "sideitem");
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(hbox, GTK_ALIGN_START);
    GtkWidget *img = gtk_image_new_from_pixbuf(icon_get(ic, 17));
    GtkWidget *lbl = gtk_label_new(label);
    gtk_widget_set_name(lbl, "sidelbl");
    gtk_box_pack_start(GTK_BOX(hbox), img, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), lbl, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(btn), hbox);
    return btn;
}

static void on_sidebar_btn(GtkWidget *w, gpointer st) {
    /* reset all, then pill-highlight active (Chrome #E8F0FE) */
    GList *kids = gtk_container_get_children(GTK_CONTAINER(gtk_widget_get_parent(w)));
    for (GList *k = kids; k; k = k->next) {
        GtkWidget *c = k->data;
        if (GTK_IS_BUTTON(c)) gtk_widget_set_name(c, "sideitem");
    }
    g_list_free(kids);
    gtk_widget_set_name(w, "sideitem-on");
    gtk_stack_set_visible_child_name(GTK_STACK(st),
        (const char *)g_object_get_data(G_OBJECT(w), "target"));
}

static void on_search_changed(GtkWidget *e, gpointer ud) {
    (void)ud;
    const char *q = gtk_entry_get_text(GTK_ENTRY(e));
    gboolean empty = !q || !*q;
    for (guint i = 0; row_metas && i < row_metas->len; i++) {
        RowMeta *m = g_ptr_array_index(row_metas, i);
        gboolean show = empty
            || (m->key   && strcasestr(m->key, q))
            || (m->label && strcasestr(m->label, q))
            || (m->hint  && strcasestr(m->hint, q));
        gtk_widget_set_visible(m->row, show);
    }
}

static void on_closed(GtkWidget *w, gpointer ud) {
    (void)w; (void)ud;
    settings_win = NULL;
    if (row_metas) g_ptr_array_unref(row_metas);
    row_metas = NULL;
}

/* per-key icons: Chrome's settings maps every row to a meaningful glyph */
static WedIcon icon_for_key(const char *key) {
    if (!strcmp(key, "block.ads"))            return IC_SHIELD;
    if (!strcmp(key, "block.trackers"))       return IC_EYE;
    if (!strcmp(key, "block.cosmetic"))       return IC_SHIELD_OFF;
    if (!strcmp(key, "fingerprint.protection")) return IC_FINGERPRINT;
    if (!strcmp(key, "cookies.thirdparty"))   return IC_COOKIE;
    if (!strcmp(key, "privacy.itp"))          return IC_SHIELD;
    if (!strcmp(key, "privacy.doh"))          return IC_GLOBE;
    if (!strcmp(key, "privacy.send_dnt"))     return IC_PERSON;
    if (!strcmp(key, "privacy.send_gpc"))     return IC_PERSON;
    if (!strcmp(key, "theme.mode"))           return IC_MOON;
    if (!strcmp(key, "theme.accent"))         return IC_PALETTE;
    if (!strcmp(key, "toolbar.compact"))      return IC_WINDOW;
    if (!strcmp(key, "startpage.show_bookmarks")) return IC_BOOKMARK;
    if (!strcmp(key, "startpage.background")) return IC_PALETTE;
    if (!strcmp(key, "startpage.search"))     return IC_SEARCH;
    if (!strcmp(key, "downloads.ask_location")) return IC_DOWNLOAD;
    if (!strcmp(key, "downloads.dir"))        return IC_FOLDER;
    if (!strcmp(key, "session.restore_prompt")) return IC_RESET;
    if (!strcmp(key, "tab.warn_on_close"))    return IC_TAB;
    if (!strcmp(key, "find.wrap"))            return IC_SEARCH;
    if (!strcmp(key, "hibernate.enabled"))    return IC_GAUGE;
    if (!strcmp(key, "hibernate.idle.minutes")) return IC_HISTORY;   /* clock */
    if (!strcmp(key, "cache.disk_mb"))        return IC_COMPUTER;
    if (!strcmp(key, "gpu.software_fallback")) return IC_COMPUTER;
    if (!strcmp(key, "ai.enabled"))           return IC_AI;
    if (!strcmp(key, "ai.model"))             return IC_AI;
    if (!strcmp(key, "net.https_first"))      return IC_LOCK;
    if (!strcmp(key, "net.proxy"))            return IC_GLOBE;
    return IC_GEAR;
}

/* Chrome section display names + icons (mapped from core schema sections) */
static const char *SECTIONS[] = {
    "You and WED", "Privacy and security", "Appearance", "Behavior",
    "Performance", "AI Assist", "Network", "About"
};
static const WedIcon SEC_ICONS[] = {
    IC_PERSON, IC_SHIELD, IC_PALETTE, IC_GEAR,
    IC_GAUGE, IC_AI, IC_GLOBE, IC_INFO
};
/* core schema section name → our sidebar slot */
static const char *SEC_MAP[] = {
    NULL, "Privacy", "Appearance", "Behavior",
    "Performance", "AI Assist", "Network", NULL
};

void settings_window_open(WedBrowser *b) {
    if (settings_win) {
        gtk_window_present(GTK_WINDOW(settings_win));
        return;
    }
    widgets = g_ptr_array_new_with_free_func((GDestroyNotify)bind_free);
    row_metas = g_ptr_array_new_with_free_func((GDestroyNotify)rowmeta_free);

    settings_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(settings_win), "Settings");
    gtk_window_set_default_size(GTK_WINDOW(settings_win), 980, 680);
    gtk_window_set_transient_for(GTK_WINDOW(settings_win), GTK_WINDOW(b->window));
    g_object_set_data(G_OBJECT(settings_win), "wed-browser", b);
    g_signal_connect(settings_win, "destroy", G_CALLBACK(on_closed), NULL);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    /* ---------------- sidebar (Chrome: 256px, pill selection) ------------- */
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_name(sidebar, "sidebar");
    gtk_widget_set_size_request(sidebar, 232, -1);
    gtk_container_set_border_width(GTK_CONTAINER(sidebar), 10);

    /* content stack */
    stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);

    GtkWidget *side_btns[8];
    GtkWidget *pages[8];
    for (int s = 0; s < 8; s++) {
        /* page: scrollable column with per-section content */
        GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        pages[s] = page;                       /* keep direct reference —
                                                   scrolled windows wrap children
                                                   in a viewport, so re-fetching
                                                   via gtk_bin_get_child would
                                                   return the viewport instead */
        GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
            GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_container_set_border_width(GTK_CONTAINER(page), 0);
        gtk_container_add(GTK_CONTAINER(scroll), page);
        gtk_stack_add_named(GTK_STACK(stack), scroll, SECTIONS[s]);

        GtkWidget *btn = side_item_new(SECTIONS[s], SEC_ICONS[s]);
        g_object_set_data(G_OBJECT(btn), "target", (gpointer)SECTIONS[s]);
        gtk_widget_set_halign(btn, GTK_ALIGN_FILL);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_sidebar_btn), stack);
        gtk_box_pack_start(GTK_BOX(sidebar), btn, FALSE, FALSE, 0);
        side_btns[s] = btn;
        g_object_set_data(G_OBJECT(page), "wed-slot", GINT_TO_POINTER(s));
    }
    gtk_box_pack_start(GTK_BOX(hbox), sidebar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), stack, TRUE, TRUE, 0);

    char *schema = wed_settings_schema();
    int n = json_count(schema);

    /* every page gets: title row, search field, then section rows */
    for (int s = 0; s < 8; s++) {
        GtkWidget *head = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
        gtk_widget_set_margin_top(head, 28);
        gtk_widget_set_margin_bottom(head, 14);
        gtk_widget_set_margin_start(head, 48);
        gtk_widget_set_margin_end(head, 48);

        if (s == 0) {
            /* "You and WED" profile card */
            GtkWidget *card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
            gtk_widget_set_name(card, "youcard");
            gtk_widget_set_margin_start(card, 0);
            GtkWidget *av = gtk_image_new_from_pixbuf(icon_get(IC_PERSON, 40));
            gtk_widget_set_size_request(av, 44, 44);
            GtkWidget *vv = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
            GtkWidget *nm = gtk_label_new("WED local profile");
            gtk_widget_set_name(nm, "rowtitle");
            gtk_widget_set_halign(nm, GTK_ALIGN_START);
            GtkWidget *em = gtk_label_new("Data stays on this device — no account, no sync");
            gtk_widget_set_name(em, "rowhint");
            gtk_widget_set_halign(em, GTK_ALIGN_START);
            gtk_box_pack_start(GTK_BOX(vv), nm, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(vv), em, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(card), av, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(card), vv, TRUE, TRUE, 0);
            gtk_box_pack_start(GTK_BOX(head), card, FALSE, FALSE, 8);
        } else if (s == 7) {
            /* About */
            char *ver = g_strdup_printf("WebKit %d.%d.%d",
                WEBKIT_MAJOR_VERSION, WEBKIT_MINOR_VERSION, WEBKIT_MICRO_VERSION);
            GtkWidget *about = gtk_label_new(NULL);
            char *mk = g_strdup_printf(
                "<span size='xx-large' weight='bold'>WED</span>\n"
                "<span color='gray'>version 2.0 · engine %s</span>\n\n"
                "Engine-agnostic browser core in Rust.\n"
                "Native GTK3 shell · no web-tech UI.\n"
                "Filter lists: EasyList + EasyPrivacy.",
                ver);
            gtk_label_set_markup(GTK_LABEL(about), mk);
            g_free(mk); g_free(ver);
            gtk_label_set_justify(GTK_LABEL(about), GTK_JUSTIFY_CENTER);
            gtk_widget_set_margin_top(about, 80);
            gtk_box_pack_start(GTK_BOX(head), about, TRUE, TRUE, 0);
        }

        if (s != 7) {
            GtkWidget *title = gtk_label_new(s == 0 ? NULL : SECTIONS[s]);
            if (s == 0) gtk_label_set_markup(GTK_LABEL(title), "<span size='large' weight='bold'>You and WED</span>");
            gtk_widget_set_name(title, "pagetitle");
            gtk_widget_set_halign(title, GTK_ALIGN_START);
            gtk_box_pack_start(GTK_BOX(head), title, FALSE, FALSE, 0);

            GtkWidget *searchhbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_widget_set_name(searchhbox, "searchpill");
            GtkWidget *simg = gtk_image_new_from_pixbuf(icon_get(IC_SEARCH, 15));
            gtk_widget_set_margin_start(simg, 12);
            gtk_widget_set_valign(simg, GTK_ALIGN_CENTER);
            GtkWidget *se = gtk_entry_new();
            gtk_entry_set_placeholder_text(GTK_ENTRY(se), "Search settings");
            gtk_widget_set_hexpand(se, TRUE);
            g_signal_connect(se, "changed", G_CALLBACK(on_search_changed), NULL);
            gtk_box_pack_start(GTK_BOX(searchhbox), simg, FALSE, FALSE, 0);
            gtk_box_pack_start(GTK_BOX(searchhbox), se, TRUE, TRUE, 0);
            gtk_widget_set_size_request(searchhbox, 360, -1);
            gtk_widget_set_halign(searchhbox, GTK_ALIGN_START);
            gtk_box_pack_start(GTK_BOX(head), searchhbox, FALSE, FALSE, 6);
        }
        gtk_box_pack_start(GTK_BOX(pages[s]), head, FALSE, FALSE, 0);
    }

    /* ---------------- build rows from schema ---------------- */
    GtkWidget *sec_boxes[8];
    for (int s = 0; s < 8; s++) {
        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_widget_set_margin_start(vbox, 48);
        gtk_widget_set_margin_end(vbox, 48);
        gtk_widget_set_margin_bottom(vbox, 32);
        gtk_box_pack_start(GTK_BOX(pages[s]), vbox, FALSE, FALSE, 0);
        sec_boxes[s] = vbox;
    }

    for (int i = 0; i < n; i++) {
        char *key = json_array_str(schema, i, "key");
        char *label = json_array_str(schema, i, "label");
        char *kind = json_array_str(schema, i, "kind");
        char *def = json_array_str(schema, i, "default");
        char *hint = json_array_str(schema, i, "hint");
        char *section = json_array_str(schema, i, "section");
        if (!key || !label || !kind) {
            g_free(key); g_free(label); g_free(kind); g_free(def);
            g_free(hint); g_free(section); continue;
        }

        int sec = 3;
        for (int s = 1; s < 7; s++)
            if (section && !strcmp(section, SEC_MAP[s])) { sec = s; break; }
        g_free(section);

        GtkWidget *vbox = sec_boxes[sec];

        /* hairline divider between rows (Chrome: indented under icon) */
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 18);
        gtk_widget_set_name(row, "settingrow");
        gtk_widget_set_size_request(row, -1, 56);

        GtkWidget *icon = gtk_image_new_from_pixbuf(icon_get(icon_for_key(key), 19));
        gtk_widget_set_valign(icon, GTK_ALIGN_CENTER);
        gtk_widget_set_margin_start(icon, 4);
        gtk_box_pack_start(GTK_BOX(row), icon, FALSE, FALSE, 0);

        GtkWidget *txt = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        GtkWidget *lbl = gtk_label_new(label);
        gtk_widget_set_name(lbl, "rowtitle");
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(txt), lbl, FALSE, FALSE, 0);
        if (hint && *hint) {
            GtkWidget *h = gtk_label_new(hint);
            gtk_widget_set_name(h, "rowhint");
            gtk_widget_set_halign(h, GTK_ALIGN_START);
            gtk_label_set_line_wrap(GTK_LABEL(h), TRUE);
            gtk_label_set_xalign(GTK_LABEL(h), 0.0);
            gtk_label_set_max_width_chars(GTK_LABEL(h), 46);
            gtk_box_pack_start(GTK_BOX(txt), h, FALSE, FALSE, 0);
        }
        gtk_box_pack_start(GTK_BOX(row), txt, TRUE, TRUE, 0);

        GtkWidget *widget = NULL;
        int bkind = 0;

        if (!strcmp(kind, "bool")) {
            widget = gtk_switch_new();
            gtk_widget_set_valign(widget, GTK_ALIGN_CENTER);
            gtk_widget_set_margin_end(widget, 8);
            char *cur = wed_settings_str(key, def ? def : "true");
            gtk_switch_set_active(GTK_SWITCH(widget), !strcmp(cur, "true"));
            g_free(cur);
            bkind = 0;
        } else if (!strcmp(kind, "choice")) {
            widget = gtk_combo_box_text_new();
            static const struct { const char *key; const char *opts[5]; } CHOICES[] = {
                { "theme.mode",          { "light", "dark", NULL } },
                { "theme.accent",        { "blue", "teal", "violet", "rose", NULL } },
                { "startpage.search",    { "duckduckgo", "google", "bing", "wikipedia", NULL } },
                { "startpage.background",{ "gradient", "plain", NULL } },
                { "ai.model",            { "glm-4-flash", "glm-4.5", NULL } },
                { "cookies.thirdparty",  { "block", "allow", NULL } },
            };
            const char **opts = NULL;
            for (guint c = 0; c < sizeof CHOICES / sizeof CHOICES[0]; c++)
                if (!strcmp(CHOICES[c].key, key)) { opts = CHOICES[c].opts; break; }
            if (!opts) { opts = (const char*[]){ def ? def : "", NULL }; }
            for (int o = 0; opts[o]; o++)
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), opts[o]);
            char *cur = wed_settings_str(key, def ? def : "");
            int idx = 0;
            for (int o = 0; opts[o]; o++) {
                if (opts[o] && !g_ascii_strcasecmp(opts[o], cur)) { idx = o; break; }
            }
            gtk_combo_box_set_active(GTK_COMBO_BOX(widget), idx);
            g_free(cur);
            gtk_widget_set_valign(widget, GTK_ALIGN_CENTER);
            gtk_widget_set_margin_end(widget, 8);
            bkind = 1;
        } else if (!strcmp(kind, "string")) {
            widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
            GtkWidget *e = gtk_entry_new();
            char *cur = wed_settings_str(key, def ? def : "");
            gtk_entry_set_text(GTK_ENTRY(e), cur);
            gtk_entry_set_placeholder_text(GTK_ENTRY(e),
                !strcmp(key, "downloads.dir") ? "System Downloads folder" : "");
            g_free(cur);
            gtk_widget_set_valign(e, GTK_ALIGN_CENTER);
            gtk_box_pack_start(GTK_BOX(widget), e, TRUE, TRUE, 0);
            if (!strcmp(key, "downloads.dir")) {
                GtkWidget *fb = gtk_button_new_with_label("Browse…");
                g_object_set_data(G_OBJECT(fb), "entry", e);
                gtk_box_pack_start(GTK_BOX(widget), fb, FALSE, FALSE, 0);
                Bind *fbnd = g_new0(Bind, 1);
                fbnd->key = g_strdup(key);
                fbnd->w = widget;
                g_object_set_data_full(G_OBJECT(fb), "bind", fbnd, (GDestroyNotify)bind_free);
                g_signal_connect(fb, "clicked", G_CALLBACK(on_folder), fbnd);
            }
            gtk_widget_set_valign(widget, GTK_ALIGN_CENTER);
            gtk_widget_set_margin_end(widget, 8);
            bkind = 2;
        } else if (!strcmp(kind, "number")) {
            widget = gtk_entry_new();
            char *cur = wed_settings_str(key, def ? def : "");
            gtk_entry_set_text(GTK_ENTRY(widget), cur);
            g_free(cur);
            gtk_entry_set_width_chars(GTK_ENTRY(widget), 8);
            gtk_widget_set_valign(widget, GTK_ALIGN_CENTER);
            gtk_widget_set_margin_end(widget, 8);
            bkind = 3;
        }
        if (widget) gtk_box_pack_end(GTK_BOX(row), widget, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 0);

        /* divider after each row except the last-looking one */
        GtkWidget *div = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
        gtk_widget_set_name(div, "rowdivider");
        gtk_box_pack_start(GTK_BOX(vbox), div, FALSE, FALSE, 0);

        /* register for search filtering */
        RowMeta *meta = g_new0(RowMeta, 1);
        meta->key = g_strdup(key);
        meta->label = g_strdup(label);
        meta->hint = g_strdup(hint ? hint : "");
        meta->row = row;
        meta->section = sec;
        g_ptr_array_add(row_metas, meta);

        /* bind */
        Bind *bind = g_new0(Bind, 1);
        bind->key = g_strdup(key);
        bind->w = widget;
        bind->kind = bkind;
        if (bkind == 0 && widget) g_signal_connect(widget, "state-set", G_CALLBACK(on_switch), bind);
        else if (bkind == 1 && widget) g_signal_connect(widget, "changed", G_CALLBACK(on_combo), bind);
        else if (bkind == 2 && widget) {
            GList *ch = gtk_container_get_children(GTK_CONTAINER(widget));
            if (ch) {
                g_signal_connect(ch->data, "activate", G_CALLBACK(on_entry_apply), bind);
                g_list_free(ch);
            }
        } else if (bkind == 3 && widget) {
            g_signal_connect(widget, "activate", G_CALLBACK(on_entry_apply), bind);
        }
        g_ptr_array_add(widgets, bind);

        g_free(key); g_free(label); g_free(kind); g_free(def); g_free(hint);
    }
    free(schema);

    gtk_container_add(GTK_CONTAINER(settings_win), hbox);
    gtk_widget_show_all(settings_win);
    /* window managers normally stack transients above their parent; under
     * bare X (kiosks, Xvfb test rigs) nothing does it for us, so raise
     * explicitly and pull focus. */
    gtk_window_present(GTK_WINDOW(settings_win));
    GdkWindow *gw = gtk_widget_get_window(settings_win);
    if (gw) gdk_window_raise(gw);
    gtk_widget_grab_focus(settings_win);

    /* start on Privacy (Chrome starts on its landing section) */
    gtk_widget_set_name(side_btns[1], "sideitem-on");
    gtk_stack_set_visible_child_name(GTK_STACK(stack), SECTIONS[1]);
}
