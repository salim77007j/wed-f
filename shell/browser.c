/* browser.c — window assembly, tab lifecycle, navigation, session,
 * hibernation governor, panels, find bar, global shortcuts. */
#include "wed.h"

static gint64 now_sec(void) {
    return g_get_monotonic_time() / 1000000;
}

/* ---------------------------------------------------------------- json-lite */
int json_count(const char *json) {
    if (!json) return 0;
    int n = 0, depth = 0;
    gboolean in_obj = FALSE;
    for (const char *p = json; *p; p++) {
        if (*p == '"') {
            /* skip string */
            p++;
            while (*p && *p != '"') { if (*p == '\\') p++; p++; }
        } else if (*p == '{') {
            depth++;
            if (depth == 1) { in_obj = TRUE; continue; }
        } else if (*p == '}') {
            depth--;
            if (depth == 0 && in_obj) { n++; in_obj = FALSE; }
        }
    }
    return n;
}

/* find the k-th object in a flat array; locate "key":"value" or "key":num */
static const char *json_obj_at(const char *json, int k) {
    int n = -1, depth = 0;
    const char *start = NULL;
    for (const char *p = json; *p; p++) {
        if (*p == '"') { p++; while (*p && *p != '"') { if (*p == '\\') p++; p++; } }
        else if (*p == '{') {
            depth++;
            if (depth == 1) { start = p; }
        } else if (*p == '}') {
            depth--;
            if (depth == 0 && start) {
                n++;
                if (n == k) return start;
                start = NULL;
            }
        }
    }
    return NULL;
}

char *json_array_str(const char *json, int k, const char *key) {
    const char *obj = json_obj_at(json, k);
    if (!obj) return NULL;
    char *pat = g_strdup_printf("\"%s\":", key);
    const char *f = strstr(obj, pat);
    size_t patlen = strlen(pat);
    g_free(pat);
    if (!f) return NULL;
    /* f points at the opening quote of "key": — skip the whole token */
    const char *p = f + patlen;
    while (*p == ' ' || *p == ':') p++;
    if (*p != '"') return NULL;
    p++;
    GString *s = g_string_new("");
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
            case 'n': g_string_append_c(s, '\n'); break;
            case 't': g_string_append_c(s, '\t'); break;
            case 'r': g_string_append_c(s, '\r'); break;
            case '"': g_string_append_c(s, '"'); break;
            case '\\': g_string_append_c(s, '\\'); break;
            case 'u': {
                if (p[1] && p[2] && p[3] && p[4]) {
                    char hex[5] = { p[1], p[2], p[3], p[4], 0 };
                    gunichar uc = strtol(hex, NULL, 16);
                    gunichar out[2] = { uc, 0 };
                    gchar *u = g_ucs4_to_utf8(out, 1, NULL, NULL, NULL);
                    if (u) { g_string_append(s, u); g_free(u); }
                    p += 4;
                }
                break;
            }
            default: g_string_append_c(s, *p); break;
            }
        } else {
            g_string_append_c(s, *p);
        }
        p++;
    }
    return g_string_free(s, FALSE);
}

double json_array_num(const char *json, int k, const char *key) {
    char *s = json_array_str(json, k, key);
    if (!s) {
        /* numeric value: manual scan */
        const char *obj = json_obj_at(json, k);
        if (!obj) return 0;
        char *pat = g_strdup_printf("\"%s\":", key);
        const char *f = strstr(obj, pat);
        if (!f) { g_free(pat); return 0; }
        const char *p = f + strlen(pat);
        g_free(pat);
        while (*p == ' ' || *p == ':') p++;
        if (!strncmp(p, "true", 4)) return 1;
        if (!strncmp(p, "false", 5)) return 0;
        return g_ascii_strtod(p, NULL);
    }
    double v = g_ascii_strtod(s, NULL);
    g_free(s);
    return v;
}

/* ---------------------------------------------------------------- tabs */
WedTab *browser_active_tab(WedBrowser *b) {
    if (b->tabs->len == 0 || b->active < 0 || b->active >= (int)b->tabs->len)
        return NULL;
    return g_ptr_array_index(b->tabs, b->active);
}

static void tab_free(gpointer p) {
    WedTab *t = p;
    g_free(t->url);
    g_free(t->title);
    if (t->favicon) g_object_unref(t->favicon);
    if (t->hibernate_shot) g_object_unref(t->hibernate_shot);
    g_free(t);
}

void browser_add_tab(WedBrowser *b, const char *url) {
    WedTab *t = g_new0(WedTab, 1);
    t->b = b;
    t->zoom = 1.0;
    t->last_active = now_sec();
    t->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(t->box, "content");

    webview_new(b, t, url);
    gtk_container_add(GTK_CONTAINER(t->box), t->webview);
    g_ptr_array_add(b->tabs, t);

    char *name = g_strdup_printf("tab%d", (int)b->tabs->len);
    gtk_stack_add_named(GTK_STACK(b->content_stack), t->box, name);
    g_free(name);
    gtk_widget_show_all(t->box);

    int idx = b->tabs->len - 1;
    browser_switch_tab(b, idx);
}

void browser_close_tab_silent(WedBrowser *b, int index);

static void really_close_tab(WedBrowser *b, int index) {
    if (index < 0 || index >= (int)b->tabs->len) return;
    WedTab *t = g_ptr_array_index(b->tabs, index);
    gtk_container_remove(GTK_CONTAINER(b->content_stack), t->box);
    /* GPtrArray owns tabs via free_func — removal frees the WedTab */
    g_ptr_array_remove_index(b->tabs, index);
    if (b->active >= (int)b->tabs->len) b->active = b->tabs->len - 1;
    if (b->tabs->len == 0) {
        if (b->closing_all) return;
        browser_add_tab(b, NULL);
    } else {
        browser_switch_tab(b, b->active);
    }
    tabstrip_update(b);
}

void browser_close_tab_silent(WedBrowser *b, int index) {
    really_close_tab(b, index);
}

void browser_close_tab(WedBrowser *b, int index) {
    if (index < 0 || index >= (int)b->tabs->len) return;
    if ((int)b->tabs->len == 1) {
        if (wed_settings_bool("tab.warn_on_close", TRUE) && !b->restoring_session) {
            GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(b->window),
                GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL,
                "Close the last tab?");
            gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d),
                "This is the only open tab. Closing it will close the browser.");
            if (gtk_dialog_run(GTK_DIALOG(d)) != GTK_RESPONSE_OK) {
                gtk_widget_destroy(d);
                return;
            }
            gtk_widget_destroy(d);
            browser_quit(b);
            return;
        }
        browser_quit(b);
        return;
    }
    really_close_tab(b, index);
}

void browser_switch_tab(WedBrowser *b, int index) {
    if (index < 0 || index >= (int)b->tabs->len) return;
    b->active = index;
    WedTab *t = g_ptr_array_index(b->tabs, index);
    /* take focus off the omnibox so URL display refreshes */
    if (t->webview) gtk_widget_grab_focus(t->webview);
    gtk_stack_set_visible_child(GTK_STACK(b->content_stack), t->box);
    t->last_active = now_sec();
    browser_update_nav(b);
    browser_update_omnibox(b);
    tabstrip_update(b);
    browser_update_shield_ui(b);
    {
        const char *title = t->title && *t->title ? t->title : "New tab";
        char *full = g_strdup_printf("%s — WED", title);
        gtk_window_set_title(GTK_WINDOW(b->window), full);
        g_free(full);
    }
}

void browser_load_url(WedBrowser *b, const char *url) {
    WedTab *t = browser_active_tab(b);
    if (!t) { browser_add_tab(b, url); return; }
    if (t->hibernated) browser_wake_tab(b, b->active);
    g_free(t->url);
    t->url = g_strdup(url);
    if (t->webview) {
        webkit_web_view_load_uri(WEBKIT_WEB_VIEW(t->webview), url);
    }
    browser_update_omnibox(b);
}

void browser_update_nav(WedBrowser *b) {
    WedTab *t = browser_active_tab(b);
    gboolean can_back = FALSE, can_fwd = FALSE;
    if (t && t->webview) {
        WebKitBackForwardList *l = webkit_web_view_get_back_forward_list(
            WEBKIT_WEB_VIEW(t->webview));
        can_back = webkit_back_forward_list_get_back_item(l) != NULL;
        can_fwd = webkit_back_forward_list_get_forward_item(l) != NULL;
    }
    gtk_widget_set_sensitive(b->btn_back, can_back);
    gtk_widget_set_sensitive(b->btn_fwd, can_fwd);
}

void browser_update_omnibox(WedBrowser *b) {
    omnibox_update(b);
    /* keep stack child titles in sync for a11y */
}

void browser_update_tab_ui(WedBrowser *b) {
    tabstrip_update(b);
}

/* ---------------------------------------------------------------- shield ui */
void browser_update_shield_ui(WedBrowser *b) {
    unsigned long ads = 0, tr = 0, pages = 0, bytes = 0;
    wed_stats(&ads, &tr, &pages, &bytes);
    unsigned long total = ads + tr;
    char *lbl;
    if (total >= 10000) lbl = g_strdup_printf("%luk", total / 1000);
    else if (total > 0) lbl = g_strdup_printf("%lu", total);
    else lbl = g_strdup("");
    if (b->shield_count_lbl)
        gtk_label_set_text(GTK_LABEL(b->shield_count_lbl), lbl);
    g_free(lbl);
    WedTab *t = browser_active_tab(b);
    gboolean on = TRUE;
    if (t && t->url) {
        char *host = host_from_uri(t->url);
        if (host && *host) on = wed_shields_enabled_for(host);
        g_free(host);
    }
    GdkPixbuf *pb = icon_get(on ? IC_SHIELD : IC_SHIELD_OFF, 17);
    GtkWidget *img = gtk_bin_get_child(GTK_BIN(b->btn_shield));
    if (GTK_IS_IMAGE(img)) gtk_image_set_from_pixbuf(GTK_IMAGE(img), pb);
}

/* ---------------------------------------------------------------- session */
static char *tabs_to_json(WedBrowser *b) {
    GString *s = g_string_new("[");
    for (guint i = 0; i < b->tabs->len; i++) {
        WedTab *t = g_ptr_array_index(b->tabs, i);
        if (i) g_string_append_c(s, ',');
        g_string_append_printf(s, "{\"url\":\"%s\",\"title\":\"%s\",\"active\":%s}",
            t->url ? t->url : "", t->title ? t->title : "",
            i == (guint)b->active ? "true" : "false");
    }
    g_string_append_c(s, ']');
    return g_string_free(s, FALSE);
}

void browser_save_session(WedBrowser *b) {
    char *tabs = tabs_to_json(b);
    gint w = 1280, h = 850;
    gtk_window_get_size(GTK_WINDOW(b->window), &w, &h);
    gboolean max = gtk_window_is_maximized(GTK_WINDOW(b->window));
    char *ws = g_strdup_printf("{\"w\":%d,\"h\":%d,\"max\":%s}", w, h,
                               max ? "true" : "false");
    wed_session_save(tabs, ws);
    g_free(tabs);
    g_free(ws);
}

static gboolean session_timer_cb(gpointer ud) {
    WedBrowser *b = ud;
    browser_save_session(b);
    return G_SOURCE_CONTINUE;
}

void browser_restore_session(WedBrowser *b) {
    char *tabs = wed_session_load();
    if (!tabs || !*tabs) { free(tabs); browser_add_tab(b, NULL); return; }
    if (wed_settings_bool("session.restore_prompt", TRUE)) {
        GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(b->window),
            GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
            "Restore your previous session?");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d),
            "%d tabs from your last session can be reopened.",
            json_count(tabs));
        gint r = gtk_dialog_run(GTK_DIALOG(d));
        gtk_widget_destroy(d);
        if (r != GTK_RESPONSE_YES) { free(tabs); browser_add_tab(b, NULL); return; }
    }
    b->restoring_session = TRUE;
    int n = json_count(tabs);
    int active = 0;
    for (int i = 0; i < n; i++) {
        char *u = json_array_str(tabs, i, "url");
        double act = json_array_num(tabs, i, "active");
        if (act > 0.5) active = i;
        browser_add_tab(b, u && *u ? u : NULL);
        g_free(u);
        /* let loads settle */
        while (gtk_events_pending()) gtk_main_iteration();
    }
    b->restoring_session = FALSE;
    browser_switch_tab(b, active);
    free(tabs);
}

static void on_hibernate_reload(GtkWidget *btn, gpointer ud);

/* ---------------------------------------------------------------- hibernate */
void browser_hibernate_tab(WedBrowser *b, int index) {
    if (index < 0 || index >= (int)b->tabs->len || index == b->active) return;
    WedTab *t = g_ptr_array_index(b->tabs, index);
    if (t->hibernated || !t->webview) return;
    if (t->webview && webkit_web_view_is_playing_audio(WEBKIT_WEB_VIEW(t->webview)))
        return;

    GdkPixbuf *shot = wed_snapshot(t);
    t->hibernated = TRUE;
    gtk_container_remove(GTK_CONTAINER(t->box), t->webview);
    t->webview = NULL;

    /* placeholder UI */
    t->hibernate_holder = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_name(t->hibernate_holder, "content");
    GtkWidget *lbl = gtk_label_new(NULL);
    char *msg = g_strdup_printf("<span size='large' weight='bold'>%s</span>\n"
        "<span color='gray' size='small'>Tab is hibernating to save memory — "
        "click to reload</span>",
        t->title ? t->title : (t->url ? t->url : "Tab"));
    gtk_label_set_markup(GTK_LABEL(lbl), msg);
    g_free(msg);
    gtk_container_add(GTK_CONTAINER(t->hibernate_holder), lbl);
    GtkWidget *btn = gtk_button_new_with_label("Reload tab");
    gtk_widget_set_name(btn, "suggested-action");
    gtk_widget_set_halign(btn, GTK_ALIGN_CENTER);
    g_object_set_data(G_OBJECT(btn), "wed-browser", b);
    g_object_set_data(G_OBJECT(btn), "wed-tab", GINT_TO_POINTER(index));
    g_signal_connect(btn, "clicked", G_CALLBACK(on_hibernate_reload), b);
    gtk_container_add(GTK_CONTAINER(t->hibernate_holder), btn);
    gtk_widget_set_valign(t->hibernate_holder, GTK_ALIGN_CENTER);
    gtk_widget_show_all(t->hibernate_holder);
    gtk_container_add(GTK_CONTAINER(t->box), t->hibernate_holder);

    if (shot) {
        if (t->hibernate_shot) g_object_unref(t->hibernate_shot);
        t->hibernate_shot = shot;
    }
}

void browser_wake_tab(WedBrowser *b, int index) {
    if (index < 0 || index >= (int)b->tabs->len) return;
    WedTab *t = g_ptr_array_index(b->tabs, index);
    if (!t->hibernated) return;
    if (t->hibernate_holder) {
        gtk_container_remove(GTK_CONTAINER(t->box), t->hibernate_holder);
        t->hibernate_holder = NULL;
    }
    t->hibernated = FALSE;
    webview_new(b, t, t->url ? t->url : "wed://start");
    gtk_container_add(GTK_CONTAINER(t->box), t->webview);
    gtk_widget_show_all(t->box);
    t->last_active = now_sec();
}

static void on_hibernate_reload(GtkWidget *btn, gpointer ud) {
    (void)ud;
    WedBrowser *b = g_object_get_data(G_OBJECT(btn), "wed-browser");
    int index = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "wed-tab"));
    browser_wake_tab(b, index);
}

static gboolean hibernate_timer_cb(gpointer ud) {
    WedBrowser *b = ud;
    for (guint i = 0; i < b->tabs->len; i++) {
        if ((int)i == b->active) continue;
        WedTab *t = g_ptr_array_index(b->tabs, i);
        if (t->hibernated) continue;
        int idle = (int)(now_sec() - t->last_active);
        int playing = (t->webview &&
            webkit_web_view_is_playing_audio(WEBKIT_WEB_VIEW(t->webview))) ? 1 : 0;
        if (wed_governor_should_hibernate(idle, playing)) {
            browser_hibernate_tab(b, i);
        }
    }
    browser_update_shield_ui(b);
    return G_SOURCE_CONTINUE;
}

/* ---------------------------------------------------------------- panels */
void browser_toggle_panel(WedBrowser *b, int panel) {
    if (b->panel_kind == panel) panel = 0;
    b->panel_kind = panel;
    if (panel == 0) {
        gtk_revealer_set_transition_type(GTK_REVEALER(b->panel_revealer),
            GTK_REVEALER_TRANSITION_TYPE_SLIDE_LEFT);
        gtk_revealer_set_reveal_child(GTK_REVEALER(b->panel_revealer), FALSE);
        return;
    }
    /* rebuild panel content */
    GList *ch = gtk_container_get_children(GTK_CONTAINER(b->panel_box));
    for (GList *i = ch; i; i = i->next) gtk_container_remove(GTK_CONTAINER(b->panel_box), i->data);
    g_list_free(ch);
    gtk_container_add(GTK_CONTAINER(b->panel_box), panel_new(b, panel));
    gtk_widget_show_all(b->panel_box);
    gtk_revealer_set_reveal_child(GTK_REVEALER(b->panel_revealer), TRUE);
}

void browser_toggle_find(WedBrowser *b, gboolean show) {
    findbar_show(b, show);
}

void browser_show_downloads(WedBrowser *b) {
    gtk_widget_show_all(b->downloads_popover);
}

void browser_show_privacy(WedBrowser *b) {
    privacy_refresh(b);
    gtk_widget_show_all(b->privacy_popover);
}

/* ---------------------------------------------------------------- quit */
void browser_quit(WedBrowser *b) {
    b->closing_all = TRUE;
    browser_save_session(b);
    gtk_widget_destroy(b->window);
}

static gboolean on_delete(GtkWidget *w, GdkEvent *ev, WedBrowser *b) {
    (void)w; (void)ev;
    browser_save_session(b);
    if (b->tabs->len > 1 && wed_settings_bool("tab.warn_on_close", TRUE)) {
        GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(b->window),
            GTK_DIALOG_MODAL, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
            "Close window with %d tabs?", (int)b->tabs->len);
        gtk_dialog_add_buttons(GTK_DIALOG(d),
            "_Close and exit", GTK_RESPONSE_OK,
            "_Cancel", GTK_RESPONSE_CANCEL, NULL);
        if (gtk_dialog_run(GTK_DIALOG(d)) != GTK_RESPONSE_OK) {
            gtk_widget_destroy(d);
            return TRUE;
        }
        gtk_widget_destroy(d);
    }
    b->closing_all = TRUE;
    return FALSE;   /* let default destroy proceed */
}

static void on_destroy(GtkWidget *w, WedBrowser *b) {
    (void)w; (void)b;
    gtk_main_quit();
}

/* ---------------------------------------------------------------- keys */
static gboolean on_key(GtkWidget *w, GdkEventKey *ev, WedBrowser *b) {
    (void)w;
    WedTab *t = browser_active_tab(b);
    gboolean ctrl = (ev->state & GDK_CONTROL_MASK) != 0;
    switch (ev->keyval) {
    case GDK_KEY_t:
        if (ctrl) { browser_add_tab(b, NULL); return TRUE; }
        break;
    case GDK_KEY_w:
        if (ctrl) {
            if (b->tabs->len > 1) browser_close_tab(b, b->active);
            else browser_quit(b);
            return TRUE;
        }
        break;
    case GDK_KEY_T:
        if (ctrl && (ev->state & GDK_SHIFT_MASK)) { browser_restore_session(b); return TRUE; }
        break;
    case GDK_KEY_l:
        if (ctrl) { omnibox_focus(b); return TRUE; }
        break;
    case GDK_KEY_f:
        if (ctrl) { browser_toggle_find(b, TRUE); return TRUE; }
        break;
    case GDK_KEY_j:
        if (ctrl) { browser_show_downloads(b); return TRUE; }
        break;
    case GDK_KEY_h:
        if (ctrl) { browser_toggle_panel(b, 1); return TRUE; }
        break;
    case GDK_KEY_b:
        if (ctrl) { browser_toggle_panel(b, 2); return TRUE; }
        break;
    case GDK_KEY_comma:
        if (ctrl) { settings_window_open(b); return TRUE; }
        break;
    case GDK_KEY_d:
        if (ctrl && t && t->url) {
            if (wed_is_bookmarked(t->url)) wed_bookmark_remove(t->url);
            else wed_bookmark_add(t->url, t->title ? t->title : "", "root");
            omnibox_update(b);
            return TRUE;
        }
        break;
    case GDK_KEY_r:
        if (ctrl && t && t->webview) { webkit_web_view_reload(WEBKIT_WEB_VIEW(t->webview)); return TRUE; }
        break;
    case GDK_KEY_R:
        if (ctrl && t && t->webview) { webkit_web_view_reload_bypass_cache(WEBKIT_WEB_VIEW(t->webview)); return TRUE; }
        break;
    case GDK_KEY_plus: case GDK_KEY_equal:
        if (ctrl && t) { webview_zoom(t, t->zoom + 0.1); return TRUE; }
        break;
    case GDK_KEY_minus:
        if (ctrl && t) { webview_zoom(t, t->zoom - 0.1); return TRUE; }
        break;
    case GDK_KEY_0:
        if (ctrl && t) { webview_zoom(t, 1.0); return TRUE; }
        break;
    case GDK_KEY_s:
        if (ctrl && t && t->webview) { return TRUE; }   /* handled in menus */
        break;
    case GDK_KEY_p:
        if (ctrl && t && t->webview) {
            WebKitPrintOperation *po = webkit_print_operation_new(WEBKIT_WEB_VIEW(t->webview));
            webkit_print_operation_run_dialog(po, GTK_WINDOW(b->window));
            g_object_unref(po);
            return TRUE;
        }
        break;
    case GDK_KEY_F5:
        if (t && t->webview) { webkit_web_view_reload(WEBKIT_WEB_VIEW(t->webview)); return TRUE; }
        break;
    case GDK_KEY_F12:
        if (t && t->webview) {
            WebKitWebInspector *insp = webkit_web_view_get_inspector(WEBKIT_WEB_VIEW(t->webview));
            webkit_web_inspector_show(insp);
            return TRUE;
        }
        break;
    case GDK_KEY_Left:
        if (ev->state & GDK_MOD1_MASK && t && t->webview) {
            webkit_web_view_go_back(WEBKIT_WEB_VIEW(t->webview)); return TRUE;
        }
        break;
    case GDK_KEY_Right:
        if (ev->state & GDK_MOD1_MASK && t && t->webview) {
            webkit_web_view_go_forward(WEBKIT_WEB_VIEW(t->webview)); return TRUE;
        }
        break;
    case GDK_KEY_Page_Up: case GDK_KEY_Page_Down:
        if (ctrl && b->tabs->len > 1) {
            int dir = ev->keyval == GDK_KEY_Page_Down ? 1 : -1;
            browser_switch_tab(b, (b->active + dir + (int)b->tabs->len) % (int)b->tabs->len);
            return TRUE;
        }
        break;
    case GDK_KEY_Tab: {
        if (ctrl) {
            int dir = (ev->state & GDK_SHIFT_MASK) ? -1 : 1;
            browser_switch_tab(b, (b->active + dir + (int)b->tabs->len) % (int)b->tabs->len);
            return TRUE;
        }
        break;
    }
    case GDK_KEY_F11: {
        GdkWindowState st = gdk_window_get_state(gtk_widget_get_window(b->window));
        if (st & GDK_WINDOW_STATE_FULLSCREEN)
            gtk_window_unfullscreen(GTK_WINDOW(b->window));
        else gtk_window_fullscreen(GTK_WINDOW(b->window));
        return TRUE;
    }
    default:
        break;
    }
    return FALSE;
}

/* ---------------------------------------------------------------- window */
/* global main-browser pointer for cross-module JS injection (weather etc.) */
WedBrowser *wed_main_browser = NULL;

/* run JS on every live start-page tab (used to push live data into the
 * wed://start page — weather, stats — without reloading) */
void browser_inject_js(const char *js) {
    WedBrowser *b = wed_main_browser;
    if (!b || !js) return;
    for (guint i = 0; i < b->tabs->len; i++) {
        WedTab *t = g_ptr_array_index(b->tabs, i);
        if (t->webview && t->url && g_str_has_prefix(t->url, "wed://start")) {
            webkit_web_view_evaluate_javascript(
                WEBKIT_WEB_VIEW(t->webview), js, -1, NULL, NULL, NULL, NULL, NULL);
        }
    }
}

WedBrowser *browser_new(WebKitWebContext *ctx, WebKitUserContentFilter *filter,
                        const char *startup_url, gboolean private_mode) {
    WedBrowser *b = g_new0(WedBrowser, 1);
    b->tabs = g_ptr_array_new_with_free_func(tab_free);
    b->active = -1;
    b->context = ctx;
    b->content_filter = filter;
    wed_main_browser = b;

    b->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(b->window), 1280, 850);
    gtk_window_set_title(GTK_WINDOW(b->window), "WED");
    GdkPixbuf *appico = icon_get(IC_SHIELD, 48);
    gtk_window_set_icon(GTK_WINDOW(b->window), appico);

    /* restore window geometry */
    char *ws = wed_session_window_state();
    if (ws && *ws) {
        double w = json_array_num(ws, 0, "w");
        double h = json_array_num(ws, 0, "h");
        char *mx = json_array_str(ws, 0, "max");
        if (w >= 400 && h >= 300)
            gtk_window_set_default_size(GTK_WINDOW(b->window), (int)w, (int)h);
        if (mx && !strcmp(mx, "true"))
            gtk_window_maximize(GTK_WINDOW(b->window));
        free(mx);
    }
    free(ws);
    (void)private_mode;

    b->vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(b->window), b->vbox);

    b->tabstrip = tabstrip_new(b);
    gtk_box_pack_start(GTK_BOX(b->vbox), b->tabstrip, FALSE, FALSE, 0);

    b->toolbar = toolbar_new(b);
    gtk_box_pack_start(GTK_BOX(b->vbox), b->toolbar, FALSE, FALSE, 0);

    /* Chrome-style bookmarks bar (real DB-backed, toggleable) */
    b->bookmarkbar = bookmarkbar_new(b);
    gtk_box_pack_start(GTK_BOX(b->vbox), b->bookmarkbar, FALSE, FALSE, 0);
    if (!wed_settings_bool("startpage.show_bookmarks", TRUE))
        gtk_widget_hide(b->bookmarkbar);

    /* horizontal: side panel + content */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    b->panel_revealer = gtk_revealer_new();
    gtk_revealer_set_transition_type(GTK_REVEALER(b->panel_revealer),
        GTK_REVEALER_TRANSITION_TYPE_SLIDE_RIGHT);
    gtk_revealer_set_transition_duration(GTK_REVEALER(b->panel_revealer), 180);
    gtk_widget_set_size_request(b->panel_revealer, 340, -1);
    b->panel_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_name(b->panel_box, "panel");
    gtk_container_add(GTK_CONTAINER(b->panel_revealer), b->panel_box);
    gtk_box_pack_start(GTK_BOX(hbox), b->panel_revealer, FALSE, FALSE, 0);

    /* content stack + findbar overlay */
    b->content_stack = gtk_stack_new();
    gtk_stack_set_homogeneous(GTK_STACK(b->content_stack), TRUE);
    gtk_stack_set_transition_type(GTK_STACK(b->content_stack), GTK_STACK_TRANSITION_TYPE_NONE);
    gtk_widget_set_name(b->content_stack, "content");

    b->findbar = findbar_new(b);
    b->overlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(b->overlay), b->content_stack);
    gtk_overlay_add_overlay(GTK_OVERLAY(b->overlay), b->findbar);
    gtk_widget_set_halign(b->findbar, GTK_ALIGN_END);
    gtk_widget_set_valign(b->findbar, GTK_ALIGN_START);

    gtk_box_pack_start(GTK_BOX(hbox), b->overlay, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(b->vbox), hbox, TRUE, TRUE, 0);

    /* downloads + privacy popovers (lazily built) */
    downloads_init(b);
    b->privacy_popover = privacy_popover_new(b);

    g_signal_connect(b->window, "delete-event", G_CALLBACK(on_delete), b);
    g_signal_connect(b->window, "destroy", G_CALLBACK(on_destroy), b);
    g_signal_connect(b->window, "key-press-event", G_CALLBACK(on_key), b);

    gtk_widget_show_all(b->window);
    /* the panel revealer stays mapped with reveal_child=FALSE (zero width);
       hiding it would prevent the reveal from ever mapping it */
    gtk_revealer_set_reveal_child(GTK_REVEALER(b->panel_revealer), FALSE);
    gtk_widget_hide(b->findbar);
    if (b->sugg_popover) gtk_widget_hide(b->sugg_popover);
    if (b->downloads_popover) gtk_widget_hide(b->downloads_popover);
    if (b->privacy_popover) gtk_widget_hide(b->privacy_popover);

    /* first tab */
    if (startup_url && *startup_url) browser_add_tab(b, startup_url);
    else browser_restore_session(b);

    /* timers: session autosave, hibernation governor, shield badge */
    b->session_timer = g_timeout_add_seconds(30, session_timer_cb, b);
    b->hibernate_timer = g_timeout_add_seconds(30, hibernate_timer_cb, b);
    return b;
}
