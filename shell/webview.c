/* webview.c — WebKitWebView creation + all page-level wiring:
 * privacy (content filter, cosmetic CSS, fingerprint script), history,
 * titles/favicons, permissions, script dialogs, popups, TLS errors,
 * crash recovery, inspector, print. */
#include "wed.h"

/* ---------------------------------------------------------------- helpers */
char *wed_settings_str(const char *key, const char *def) {
    return wed_settings_get(key, def);
}
gboolean wed_settings_bool(const char *key, gboolean def) {
    char *v = wed_settings_get(key, def ? "true" : "false");
    gboolean r = v && strcmp(v, "true") == 0;
    free(v);
    return r;
}
void wed_settings_save(const char *key, const char *val) {
    wed_settings_set(key, val);
}

const char *search_engine_url(void) {
    char *e = wed_settings_str("startpage.search", "duckduckgo");
    const char *u;
    if (strcmp(e, "google") == 0) u = "https://www.google.com/search?q=";
    else if (strcmp(e, "bing") == 0) u = "https://www.bing.com/search?q=";
    else if (strcmp(e, "wikipedia") == 0) u = "https://en.wikipedia.org/w/index.php?search=";
    else u = "https://duckduckgo.com/?q=";
    free(e);
    return u;
}

char *search_url_for(const char *text) {
    GString *s = g_string_new(search_engine_url());
    char *enc = g_uri_escape_string(text, NULL, FALSE);
    g_string_append(s, enc);
    g_free(enc);
    return g_string_free(s, FALSE);
}

gboolean looks_like_url(const char *text) {
    if (!text || !*text) return FALSE;
    if (g_str_has_prefix(text, "http://") || g_str_has_prefix(text, "https://")
        || g_str_has_prefix(text, "file://") || g_str_has_prefix(text, "wed://")
        || g_str_has_prefix(text, "about:"))
        return TRUE;
    if (strchr(text, ' ')) return FALSE;
    if (strchr(text, '@')) return FALSE;
    if (strstr(text, "://")) return TRUE;
    char *dot = strchr(text, '.');
    if (!dot) return g_str_has_prefix(text, "localhost");
    for (const char *p = text; *p; p++) {
        if (!g_ascii_isalnum(*p) && !strchr(".-/:?&=%_~#+", *p)) return FALSE;
    }
    return TRUE;
}

char *host_from_uri(const char *uri) {
    if (!uri) return g_strdup("");
    const char *scheme_end = strstr(uri, "://");
    if (!scheme_end) return g_strdup("");
    const char *host = scheme_end + 3;
    const char *slash = strchr(host, '/');
    int len = slash ? (int)(slash - host) : (int)strlen(host);
    if (len > 0 && host[0] == '/') { host++; len--; }
    const char *colon = len ? memchr(host, ':', len) : NULL;
    if (colon) len = (int)(colon - host);
    char *h = g_strndup(host, len);
    for (char *p = h; *p; p++) *p = g_ascii_tolower(*p);
    return h;
}

/* Synchronous page snapshot (used for hibernation placeholders). */
typedef struct { GdkPixbuf *pb; GMainLoop *loop; } SnapCtx;

static void on_snapshot_done(GObject *obj, GAsyncResult *res, gpointer ud) {
    SnapCtx *ctx = ud;
    GError *err = NULL;
    cairo_surface_t *sf = webkit_web_view_get_snapshot_finish(
        WEBKIT_WEB_VIEW(obj), res, &err);
    if (err) { g_error_free(err); ctx->pb = NULL; }
    else if (sf) {
        int w = cairo_image_surface_get_width(sf);
        int h = cairo_image_surface_get_height(sf);
        ctx->pb = (w > 0 && h > 0)
            ? gdk_pixbuf_get_from_surface(sf, 0, 0, w, h) : NULL;
        cairo_surface_destroy(sf);
    }
    g_main_loop_quit(ctx->loop);
}

GdkPixbuf *wed_snapshot(WedTab *t) {
    if (!t || !t->webview) return NULL;
    SnapCtx ctx = { NULL, g_main_loop_new(NULL, FALSE) };
    webkit_web_view_get_snapshot(WEBKIT_WEB_VIEW(t->webview),
        WEBKIT_SNAPSHOT_REGION_VISIBLE, WEBKIT_SNAPSHOT_OPTIONS_NONE, NULL,
        on_snapshot_done, &ctx);
    /* bound the nested loop to 2s */
    gulong id = g_timeout_add(2000, (GSourceFunc)g_main_loop_quit, ctx.loop);
    g_main_loop_run(ctx.loop);
    g_source_remove(id);
    g_main_loop_unref(ctx.loop);
    return ctx.pb;
}

/* ---------------------------------------------------------------- load events */
static void on_load_changed(WebKitWebView *wv, WebKitLoadEvent ev, WedTab *t) {
    WedBrowser *b = t->b;
    switch (ev) {
    case WEBKIT_LOAD_STARTED:
        t->loading = TRUE;
        tabstrip_update(b);
        break;
    case WEBKIT_LOAD_COMMITTED: {
        t->loading = TRUE;
        const char *uri = webkit_web_view_get_uri(wv);
        g_free(t->url); t->url = g_strdup(uri);
        webview_apply_privacy(t);
        browser_update_omnibox(b);
        browser_update_nav(b);
        tabstrip_update(b);
        break;
    }
    case WEBKIT_LOAD_FINISHED: {
        t->loading = FALSE;
        const char *uri = webkit_web_view_get_uri(wv);
        const char *title = webkit_web_view_get_title(wv);
        if (uri && *uri && !g_str_has_prefix(uri, "wed://")
            && !g_str_has_prefix(uri, "about:")) {
            wed_history_add(uri, title ? title : "");
            wed_record_page_load();
        }
        g_free(t->title);
        t->title = g_strdup(title && *title ? title : (uri && *uri ? uri : "New tab"));
        tabstrip_update(b);
        browser_update_omnibox(b);
        break;
    }
    default:
        break;
    }
}

static void on_title_changed(WebKitWebView *wv, GParamSpec *ps, WedTab *t) {
    (void)ps;
    const char *title = webkit_web_view_get_title(wv);
    g_free(t->title);
    t->title = g_strdup(title && *title ? title : (t->url ? t->url : "New tab"));
    tabstrip_update(t->b);
}

static void on_favicon_changed(WebKitWebView *wv, GParamSpec *ps, WedTab *t) {
    (void)ps;
    cairo_surface_t *sf = webkit_web_view_get_favicon(wv);
    if (!sf) return;
    int w = cairo_image_surface_get_width(sf);
    int h = cairo_image_surface_get_height(sf);
    if (w > 0 && h > 0) {
        GdkPixbuf *pb = gdk_pixbuf_get_from_surface(sf, 0, 0, w, h);
        if (pb) {
            int sw = 16, sh = 16;
            if (w > h) sh = MAX(1, h * 16 / w); else sw = MAX(1, w * 16 / h);
            GdkPixbuf *sc = gdk_pixbuf_scale_simple(pb, sw, sh, GDK_INTERP_BILINEAR);
            g_object_unref(pb);
            if (t->favicon) g_object_unref(t->favicon);
            t->favicon = sc;
            tabstrip_update(t->b);
        }
    }
}

static gboolean on_decide_policy(WebKitWebView *wv, WebKitPolicyDecision *d,
                                 WebKitPolicyDecisionType type, WedTab *t) {
    if (type == WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION) {
        WebKitNavigationAction *act =
            webkit_navigation_policy_decision_get_navigation_action(
                WEBKIT_NAVIGATION_POLICY_DECISION(d));
        if (webkit_navigation_action_get_navigation_type(act)
                == WEBKIT_NAVIGATION_TYPE_LINK_CLICKED) {
            unsigned mods = webkit_navigation_action_get_modifiers(act);
            if (mods & GDK_CONTROL_MASK) {
                WebKitURIRequest *req = webkit_navigation_action_get_request(act);
                browser_add_tab(t->b, webkit_uri_request_get_uri(req));
                webkit_policy_decision_ignore(d);
                return TRUE;
            }
        }
    } else if (type == WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) {
        char *host = host_from_uri(t->url ? t->url : "");
        char *perm = (host && *host) ? wed_permission_get(host, "popup") : g_strdup("ask");
        gboolean allow = strcmp(perm, "allow") == 0;
        free(perm); g_free(host);
        if (allow) webkit_policy_decision_use(d);
        else webkit_policy_decision_ignore(d);
        return TRUE;
    }
    (void)wv;
    return FALSE;
}

static GtkWidget *on_create_web_view(WebKitWebView *wv, WebKitNavigationAction *act, WedTab *t) {
    (void)wv;
    char *host = host_from_uri(t->url ? t->url : "");
    char *perm = (host && *host) ? wed_permission_get(host, "popup") : g_strdup("ask");
    gboolean allow = strcmp(perm, "allow") == 0;
    free(perm); g_free(host);
    if (!allow) return NULL;
    WebKitURIRequest *req = webkit_navigation_action_get_request(act);
    browser_add_tab(t->b, webkit_uri_request_get_uri(req));
    /* return the freshly created tab's webview so WebKit completes the
     * navigation there instead of spawning an orphan window */
    WedTab *nt = t->b->tabs->len ? g_ptr_array_index(t->b->tabs, t->b->tabs->len - 1) : NULL;
    return nt ? nt->webview : NULL;
}

static gboolean on_script_dialog(WebKitWebView *wv, WebKitScriptDialog *dlg, WedTab *t) {
    (void)wv;
    GtkWindow *win = GTK_WINDOW(t->b->window);
    const char *msg = webkit_script_dialog_get_message(dlg);
    GtkWidget *dialog = NULL;
    switch (webkit_script_dialog_get_dialog_type(dlg)) {
    case WEBKIT_SCRIPT_DIALOG_ALERT:
        dialog = gtk_message_dialog_new(win, GTK_DIALOG_MODAL,
                GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", msg);
        gtk_dialog_run(GTK_DIALOG(dialog));
        break;
    case WEBKIT_SCRIPT_DIALOG_CONFIRM:
    case WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM: {
        dialog = gtk_message_dialog_new(win, GTK_DIALOG_MODAL,
                GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, "%s", msg);
        gboolean ok = gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK;
        webkit_script_dialog_confirm_set_confirmed(dlg, ok);
        break;
    }
    case WEBKIT_SCRIPT_DIALOG_PROMPT: {
        dialog = gtk_message_dialog_new(win, GTK_DIALOG_MODAL,
                GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, "%s", msg);
        GtkWidget *entry = gtk_entry_new();
        const char *def = webkit_script_dialog_prompt_get_default_text(dlg);
        gtk_entry_set_text(GTK_ENTRY(entry), def ? def : "");
        gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                           entry, FALSE, FALSE, 6);
        gtk_widget_show(entry);
        if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
            webkit_script_dialog_prompt_set_text(dlg, gtk_entry_get_text(GTK_ENTRY(entry)));
        break;
    }
    default:
        return FALSE;
    }
    gtk_widget_destroy(dialog);
    return TRUE;
}

/* ---------------------------------------------------------------- permissions */
static void on_permission_request(WebKitWebView *wv, WebKitPermissionRequest *req, WedTab *t) {
    (void)wv;
    const char *kind = "notifications";
    if (WEBKIT_IS_GEOLOCATION_PERMISSION_REQUEST(req)) kind = "geolocation";
    else if (WEBKIT_IS_USER_MEDIA_PERMISSION_REQUEST(req)) kind = "media";

    char *host = host_from_uri(t->url ? t->url : "");
    char *saved = (host && *host) ? wed_permission_get(host, kind) : g_strdup("ask");

    if (strcmp(saved, "allow") == 0) { webkit_permission_request_allow(req); free(saved); g_free(host); return; }
    if (strcmp(saved, "deny") == 0)  { webkit_permission_request_deny(req);  free(saved); g_free(host); return; }

    const char *desc = "show notifications";
    if (!strcmp(kind, "geolocation")) desc = "know your location";
    else if (!strcmp(kind, "media")) desc = "use your camera/microphone";

    char *secondary = g_strdup_printf("%s wants to %s.",
                                      *host ? host : "This site", desc);
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(t->b->window), GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, "Site permission request");
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", secondary);
    gtk_dialog_add_buttons(GTK_DIALOG(dialog),
        "_Always allow", GTK_RESPONSE_YES,
        "_Always deny", GTK_RESPONSE_NO,
        "_Deny once", GTK_RESPONSE_REJECT, NULL);
    gint resp = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    if (resp == GTK_RESPONSE_YES) {
        webkit_permission_request_allow(req);
        if (host && *host) wed_permission_set(host, kind, "allow");
    } else if (resp == GTK_RESPONSE_NO) {
        webkit_permission_request_deny(req);
        if (host && *host) wed_permission_set(host, kind, "deny");
    } else {
        webkit_permission_request_deny(req);
    }
    free(saved); g_free(host); g_free(secondary);
}

/* ---------------------------------------------------------------- crash */
static void on_web_process_terminated(WebKitWebView *wv,
        WebKitWebProcessTerminationReason reason, WedTab *t) {
    (void)wv;
    const char *why = reason == WEBKIT_WEB_PROCESS_CRASHED ? "crashed"
                    : reason == WEBKIT_WEB_PROCESS_EXCEEDED_MEMORY_LIMIT
                      ? "exceeded its memory limit" : "was terminated";
    char *msg = g_strdup_printf(
        "<big><b>Page renderer %s</b></big>\n\n"
        "The renderer process for this page died. Your tabs, history and "
        "data are safe. Reload to try again.", why);
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(t->b->window),
        GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_NONE, "Renderer problem");
    gtk_message_dialog_set_markup(GTK_MESSAGE_DIALOG(dialog), msg);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "_Reload", GTK_RESPONSE_OK);
    gtk_dialog_add_button(GTK_DIALOG(dialog), "_Close", GTK_RESPONSE_CANCEL);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK && t->url && t->webview)
        webkit_web_view_reload_bypass_cache(WEBKIT_WEB_VIEW(t->webview));
    gtk_widget_destroy(dialog);
    g_free(msg);
}

/* page context menu: keep WebKit's stock items, add AI on selection */
static gboolean on_context_menu(WebKitWebView *wv, WebKitContextMenu *menu,
                                GdkEvent *e, WebKitHitTestResult *ht, gpointer ud) {
    (void)wv; (void)e; (void)ud;
    if (webkit_hit_test_result_context_is_selection(ht)) {
        gtk_widget_grab_focus(GTK_WIDGET(wv));
        WebKitContextMenuItem *sep = webkit_context_menu_item_new_separator();
        webkit_context_menu_insert(menu, sep, -1);
        GtkAction *act = gtk_action_new("wed-ai-sel", "Ask AI about selection",
                                        "Explain or summarize the selected text with the AI assistant", NULL);
        g_signal_connect(act, "activate", G_CALLBACK(ai_selection_action), NULL);
        WebKitContextMenuItem *item = webkit_context_menu_item_new(act);
        webkit_context_menu_insert(menu, item, -1);
        g_object_unref(act);
    }
    return FALSE;
}

/* ---------------------------------------------------------------- create */
GtkWidget *webview_new(WedBrowser *b, WedTab *t, const char *url) {
    WebKitSettings *st = webkit_settings_new();
    webkit_settings_set_enable_javascript(st, TRUE);
    webkit_settings_set_javascript_can_open_windows_automatically(st, FALSE);
    webkit_settings_set_enable_developer_extras(st, TRUE);
    webkit_settings_set_enable_media_stream(st, TRUE);
    webkit_settings_set_enable_mediasource(st, TRUE);
    webkit_settings_set_enable_webrtc(st, TRUE);
    webkit_settings_set_enable_encrypted_media(st, TRUE);
    webkit_settings_set_enable_site_specific_quirks(st, FALSE);
    webkit_settings_set_allow_file_access_from_file_urls(st, FALSE);
    webkit_settings_set_user_agent(st,
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/620.1.15 (KHTML, like Gecko) "
        "Version/17.0 Safari/620.1.15 WED/2.0");
    if (wed_settings_bool("gpu.software_fallback", FALSE)
            || g_getenv("WED_FORCE_SOFTWARE_RENDERING")) {
        webkit_settings_set_hardware_acceleration_policy(st,
            WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER);
    }

    WebKitUserContentManager *ucm = webkit_user_content_manager_new();
    extern void wed_filter_register_ucm(WebKitUserContentManager *ucm);
    wed_filter_register_ucm(ucm);

    GtkWidget *wv = b->context
        ? GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW,
            "settings", st, "user-content-manager", ucm,
            "web-context", b->context, NULL))
        : GTK_WIDGET(g_object_new(WEBKIT_TYPE_WEB_VIEW,
            "settings", st, "user-content-manager", ucm, NULL));
    g_object_unref(st);
    t->webview = wv;

    g_signal_connect(wv, "load-changed", G_CALLBACK(on_load_changed), t);
    g_signal_connect(wv, "notify::title", G_CALLBACK(on_title_changed), t);
    g_signal_connect(wv, "notify::favicon", G_CALLBACK(on_favicon_changed), t);
    g_signal_connect(wv, "decide-policy", G_CALLBACK(on_decide_policy), t);
    g_signal_connect(wv, "script-dialog", G_CALLBACK(on_script_dialog), t);
    g_signal_connect(wv, "permission-request", G_CALLBACK(on_permission_request), t);
    g_signal_connect(wv, "web-process-terminated", G_CALLBACK(on_web_process_terminated), t);
    g_signal_connect(wv, "create", G_CALLBACK(on_create_web_view), t);
    g_signal_connect(wv, "context-menu", G_CALLBACK(on_context_menu), t);

    webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(wv), t->zoom);

    const char *target = (url && *url) ? url : "wed://start";
    webkit_web_view_load_uri(WEBKIT_WEB_VIEW(wv), target);

    gtk_widget_set_vexpand(wv, TRUE);
    gtk_widget_set_hexpand(wv, TRUE);
    gtk_widget_show(wv);
    return wv;
}

/* Cosmetic CSS injection for the tab's current host (called on commit). */
void webview_apply_privacy(WedTab *t) {
    if (!t || !t->webview) return;
    WebKitUserContentManager *ucm = webkit_web_view_get_user_content_manager(
        WEBKIT_WEB_VIEW(t->webview));

    char *host = host_from_uri(t->url ? t->url : "");
    if (!host || !*host) { g_free(host); return; }

    gboolean shields = wed_shields_enabled_for(host) &&
                       wed_settings_bool("block.ads", TRUE);

    /* --- cosmetic stylesheet (replace previous) --- */
    WebKitUserStyleSheet *prev = g_object_get_data(G_OBJECT(ucm), "wed-cos");
    if (prev) {
        webkit_user_content_manager_remove_style_sheet(ucm, prev);
        g_object_set_data(G_OBJECT(ucm), "wed-cos", NULL);
    }
    if (shields) {
        char *cssjson = wed_cosmetic_json(host);
        if (cssjson) {
            GString *css = g_string_new("");
            for (int i = 0; i < 2; i++) {
                const char *needle = i == 0 ? "\"generic\":[" : "\"specific\":[";
                char *arr = strstr(cssjson, needle);
                if (!arr) continue;
                arr = strchr(arr, '[') + 1;
                char *end = strchr(arr, ']');
                if (!end) continue;
                char *sel = arr;
                while (sel < end) {
                    char *q1 = memchr(sel, '"', end - sel);
                    if (!q1) break;
                    char *q2 = memchr(q1 + 1, '"', end - q1 - 1);
                    if (!q2) break;
                    if (q2 > q1 + 1) {
                        g_string_append_len(css, q1 + 1, q2 - q1 - 1);
                        g_string_append(css, ", ");
                    }
                    sel = q2 + 1;
                }
            }
            free(cssjson);
            if (css->len >= 3) {
                g_string_truncate(css, css->len - 2);
                g_string_prepend(css, " { display: none !important; }\n");
                WebKitUserStyleSheet *style = webkit_user_style_sheet_new(
                    g_string_free(css, FALSE),
                    WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
                    WEBKIT_USER_STYLE_LEVEL_USER,
                    NULL, NULL);
                webkit_user_content_manager_add_style_sheet(ucm, style);
                g_object_set_data_full(G_OBJECT(ucm), "wed-cos", style,
                    (GDestroyNotify)webkit_user_style_sheet_unref);
            } else {
                g_string_free(css, TRUE);
            }
        }
    }

    /* --- fingerprint script (installed once per ucm) --- */
    if (wed_settings_bool("fingerprint.protection", TRUE)
            && !g_object_get_data(G_OBJECT(ucm), "wed-fp")) {
        static char *fp_js = NULL;
        if (!fp_js) {
            if (g_file_test("resources/js/fingerprint.js", G_FILE_TEST_EXISTS)) {
                gsize len = 0;
                g_file_get_contents("resources/js/fingerprint.js", &fp_js, &len, NULL);
            }
        }
        if (fp_js) {
            WebKitUserScript *script = webkit_user_script_new(fp_js,
                WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
                WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
                NULL, NULL);
            webkit_user_content_manager_add_script(ucm, script);
            g_object_set_data(G_OBJECT(ucm), "wed-fp", GINT_TO_POINTER(1));
        }
    }
    g_free(host);
}

void webview_zoom(WedTab *t, double z) {
    if (!t || !t->webview) return;
    t->zoom = CLAMP(z, 0.25, 5.0);
    webkit_web_view_set_zoom_level(WEBKIT_WEB_VIEW(t->webview), t->zoom);
}
