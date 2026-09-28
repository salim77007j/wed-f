/* settingsui.c — settings window generated from the core's settings
 * schema (single source of truth): sidebar sections + live-applied rows. */
#include "wed.h"

static GtkWidget *settings_win;
static GtkWidget *stack;
static GPtrArray *widgets;   /* GtkWidget* bound to keys */

typedef struct {
    char *key;
    GtkWidget *w;
    int kind;   /* 0 bool, 1 choice, 2 string, 3 number, 4 folder
                   (number/string share entry) */
} Bind;

static void bind_free(Bind *b) { g_free(b->key); g_free(b); }

static void apply_setting(WedBrowser *b, const char *key, const char *val) {
    wed_settings_save(key, val);
    /* live application */
    if (!strcmp(key, "theme.mode") || !strcmp(key, "theme.accent")) {
        theme_init();
        theme_apply();
        /* icons are theme-dependent: recreate window visuals */
        browser_update_shield_ui(b);
        omnibox_update(b);
    }
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

static void on_sidebar_btn(GtkWidget *w, gpointer st) {
    gtk_stack_set_visible_child_name(GTK_STACK(st),
        (const char*)g_object_get_data(G_OBJECT(w), "target"));
}

static void on_closed(GtkWidget *w, gpointer ud) {
    (void)w; (void)ud;
    settings_win = NULL;
}

void settings_window_open(WedBrowser *b) {
    if (settings_win) {
        gtk_window_present(GTK_WINDOW(settings_win));
        return;
    }
    widgets = g_ptr_array_new_with_free_func((GDestroyNotify)bind_free);
    settings_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(settings_win), "Settings");
    gtk_window_set_default_size(GTK_WINDOW(settings_win), 860, 640);
    gtk_window_set_transient_for(GTK_WINDOW(settings_win), GTK_WINDOW(b->window));
    g_object_set_data(G_OBJECT(settings_win), "wed-browser", b);
    g_signal_connect(settings_win, "destroy", G_CALLBACK(on_closed), NULL);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    /* sidebar */
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(sidebar, "panel");
    gtk_widget_set_size_request(sidebar, 230, -1);
    GtkWidget *logo = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(logo), "  <span size='large' weight='bold'>Settings</span>");
    gtk_widget_set_halign(logo, GTK_ALIGN_START);
    gtk_widget_set_margin_top(logo, 12);
    gtk_widget_set_margin_bottom(logo, 12);
    gtk_box_pack_start(GTK_BOX(sidebar), logo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), sidebar, FALSE, FALSE, 0);

    /* content stack */
    stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    gtk_box_pack_start(GTK_BOX(hbox), stack, TRUE, TRUE, 0);

    char *schema = wed_settings_schema();
    int n = json_count(schema);

    const char *sections[] = { "Privacy", "Appearance", "Behavior",
                               "Performance", "AI Assist", "Network", "About" };
    const char *icons[] = { "Privacy", "Appearance", "Behavior",
                            "Performance", "AI Assist", "Network", "About" };
    (void)icons;

    GPtrArray *sec_boxes = g_ptr_array_new();
    for (int s = 0; s < 7; s++) {
        GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
            GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(scroll), page);
        gtk_stack_add_named(GTK_STACK(stack), scroll, sections[s]);

        GtkWidget *btn = gtk_button_new_with_label(sections[s]);
        gtk_widget_set_halign(btn, GTK_ALIGN_FILL);
        g_object_set_data(G_OBJECT(btn), "target", (gpointer)sections[s]);
        gtk_box_pack_start(GTK_BOX(sidebar), btn, FALSE, FALSE, 1);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_sidebar_btn), stack);
        g_ptr_array_add(sec_boxes, page);
    }

    /* build rows into sections */
    for (int i = 0; i < n; i++) {
        char *key = json_array_str(schema, i, "key");
        char *label = json_array_str(schema, i, "label");
        char *kind = json_array_str(schema, i, "kind");
        char *def = json_array_str(schema, i, "default");
        char *hint = json_array_str(schema, i, "hint");
        if (!key || !label || !kind) { g_free(key); g_free(label); g_free(kind); g_free(def); g_free(hint); continue; }

        /* find section */
        int sec = 5;
        char *section = json_array_str(schema, i, "section");
        for (int s = 0; s < 6; s++)
            if (section && !strcmp(section, sections[s])) { sec = s; break; }
        free(section);

        GtkWidget *page = g_ptr_array_index(sec_boxes, sec);
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_name(row, "card");
        GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_container_set_border_width(GTK_CONTAINER(row), 4);
        gtk_widget_set_margin_start(row, 16);
        gtk_widget_set_margin_end(row, 16);
        gtk_widget_set_margin_top(row, 4);
        gtk_widget_set_margin_bottom(row, 4);

        GtkWidget *lbl = gtk_label_new(label);
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(top), lbl, FALSE, FALSE, 0);
        GtkWidget *widget = NULL;
        int bkind = 0;

        if (!strcmp(kind, "bool")) {
            widget = gtk_switch_new();
            char *cur = wed_settings_str(key, def ? def : "true");
            gtk_switch_set_active(GTK_SWITCH(widget), !strcmp(cur, "true"));
            free(cur);
            bkind = 0;
        } else if (!strcmp(kind, "choice")) {
            widget = gtk_combo_box_text_new();
            /* count options by parsing schema string manually is hard here;
               use known keys */
            if (!strcmp(key, "theme.mode")) {
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "Light");
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "Dark");
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "Follow system");
            } else if (!strcmp(key, "theme.accent")) {
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "Blue");
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "Teal");
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "Violet");
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "Rose");
            } else if (!strcmp(key, "startpage.search")) {
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "DuckDuckGo");
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "Google");
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "Bing");
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "Wikipedia");
            } else if (!strcmp(key, "ai.model")) {
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "glm-4-flash");
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "glm-4.5");
            } else if (!strcmp(key, "cookies.thirdparty")) {
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "block");
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "allow");
            } else if (!strcmp(key, "startpage.background")) {
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "plain");
                gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(widget), "gradient");
            }
            char *cur = wed_settings_str(key, def ? def : "");
            /* map value → index */
            GtkTreeModel *m = gtk_combo_box_get_model(GTK_COMBO_BOX(widget));
            GtkTreeIter it;
            int idx = 0;
            if (gtk_tree_model_get_iter_first(m, &it)) {
                do {
                    gchar *txt = NULL;
                    gtk_tree_model_get(m, &it, 0, &txt, -1);
                    if (txt && !g_ascii_strcasecmp(txt, cur)) { g_free(txt); break; }
                    g_free(txt);
                    idx++;
                } while (gtk_tree_model_iter_next(m, &it));
            }
            if (idx >= gtk_tree_model_iter_n_children(m, NULL)) idx = 0;
            gtk_combo_box_set_active(GTK_COMBO_BOX(widget), idx);
            free(cur);
            bkind = 1;
        } else if (!strcmp(kind, "string")) {
            widget = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
            GtkWidget *e = gtk_entry_new();
            char *cur = wed_settings_str(key, def ? def : "");
            gtk_entry_set_text(GTK_ENTRY(e), cur);
            free(cur);
            gtk_box_pack_start(GTK_BOX(widget), e, TRUE, TRUE, 0);
            if (!strcmp(key, "downloads.dir")) {
                GtkWidget *fb = gtk_button_new_with_label("Browse…");
                g_object_set_data(G_OBJECT(fb), "entry", e);
                gtk_box_pack_start(GTK_BOX(widget), fb, FALSE, FALSE, 0);
                g_object_set_data(G_OBJECT(fb), "entry", e);
                Bind *fbnd = g_new0(Bind, 1);
                fbnd->key = g_strdup(key);
                fbnd->w = widget;
                g_object_set_data_full(G_OBJECT(fb), "bind", fbnd, (GDestroyNotify)bind_free);
                g_signal_connect(fb, "clicked", G_CALLBACK(on_folder), fbnd);
            }
            bkind = 2;
        } else if (!strcmp(kind, "number")) {
            widget = gtk_entry_new();
            char *cur = wed_settings_str(key, def ? def : "");
            gtk_entry_set_text(GTK_ENTRY(widget), cur);
            free(cur);
            gtk_entry_set_width_chars(GTK_ENTRY(widget), 8);
            bkind = 3;
        }
        gtk_box_pack_end(GTK_BOX(top), widget, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(row), top, TRUE, TRUE, 0);
        if (hint && *hint) {
            GtkWidget *h = gtk_label_new(hint);
            gtk_widget_set_name(h, "secondary");
            gtk_widget_set_halign(h, GTK_ALIGN_START);
            gtk_label_set_line_wrap(GTK_LABEL(h), TRUE);
            gtk_label_set_xalign(GTK_LABEL(h), 0.0);
            gtk_box_pack_start(GTK_BOX(row), h, TRUE, TRUE, 0);
        }
        gtk_box_pack_start(GTK_BOX(page), row, FALSE, FALSE, 0);

        /* bind */
        Bind *bind = g_new0(Bind, 1);
        bind->key = g_strdup(key);
        bind->w = widget;
        bind->kind = bkind;
        if (bkind == 0) g_signal_connect(widget, "state-set", G_CALLBACK(on_switch), bind);
        else if (bkind == 1) g_signal_connect(widget, "changed", G_CALLBACK(on_combo), bind);
        else if (bkind == 2) {
            GList *ch = gtk_container_get_children(GTK_CONTAINER(widget));
            if (ch) {
                g_signal_connect(ch->data, "activate",
                    G_CALLBACK(on_entry_apply), bind);
                g_list_free(ch);
            }
        } else if (bkind == 3) {
            g_signal_connect(widget, "activate", G_CALLBACK(on_entry_apply), bind);
        }
        g_ptr_array_add(widgets, bind);

        g_free(key); g_free(label); g_free(kind); g_free(def); g_free(hint);
    }

    /* About page */
    GtkWidget *about = g_ptr_array_index(sec_boxes, 6);
    GtkWidget *al = gtk_label_new(NULL);
    char *ver = g_strdup_printf("WebKit %d.%d.%d",
        WEBKIT_MAJOR_VERSION, WEBKIT_MINOR_VERSION, WEBKIT_MICRO_VERSION);
    char *mk = g_strdup_printf(
        "<span size='xx-large' weight='bold'>WED</span>\n"
        "<span color='gray'>version 2.0 · %s</span>\n\n"
        "Engine-agnostic browser core in Rust.\n"
        "Native GTK3 shell · no web-tech UI.\n\n"
        "<span size='small'>Filter lists: EasyList + EasyPrivacy. "
        "Rendering: WebKitGTK (system component).</span>", ver);
    gtk_label_set_markup(GTK_LABEL(al), mk);
    g_free(mk);
    g_free(ver);
    gtk_label_set_justify(GTK_LABEL(al), GTK_JUSTIFY_CENTER);
    gtk_widget_set_margin_top(al, 60);
    gtk_container_add(GTK_CONTAINER(about), al);

    gtk_container_add(GTK_CONTAINER(settings_win), hbox);
    gtk_widget_show_all(settings_win);
    free(schema);
}
