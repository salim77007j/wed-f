/* main.c — WED browser entry point: core init, filter loading, content
 * filter compilation, proxy, single-instance guard, window creation. */
#include "wed.h"


static WebKitUserContentFilter *g_filter = NULL;
static GPtrArray *g_ucms = NULL;   /* live user content managers */

static void on_filter_saved(GObject *obj, GAsyncResult *res, gpointer ud) {
    (void)ud;
    WebKitUserContentFilterStore *store = (WebKitUserContentFilterStore *)obj;
    GError *err = NULL;
    WebKitUserContentFilter *f = webkit_user_content_filter_store_save_finish(store, res, &err);
    if (err) {
        g_warning("content filter compile failed: %s", err->message);
        g_error_free(err);
        return;
    }
    g_print("WED: engine content filter ready\n");
    if (g_ucms) {
        for (guint i = 0; i < g_ucms->len; i++)
            webkit_user_content_manager_add_filter(g_ptr_array_index(g_ucms, i), f);
    }
    g_filter = f;
}

void wed_filter_register_ucm(WebKitUserContentManager *ucm) {
    if (!g_ucms) g_ucms = g_ptr_array_new();
    g_ptr_array_add(g_ucms, ucm);
    if (g_filter)
        webkit_user_content_manager_add_filter(ucm, g_filter);
}

static WebKitUserContentFilter *loop_data_filter = NULL;

static char *read_file(const char *path) {
    gchar *data = NULL;
    gsize len = 0;
    if (g_file_get_contents(path, &data, &len, NULL))
        return data;
    return NULL;
}

static gboolean on_delete_at_exit(GtkWidget *w, GdkEvent *e, gpointer ud) {
    (void)w; (void)e; (void)ud;
    gtk_main_quit();
    return FALSE;
}

int main(int argc, char **argv) {
    gtk_init(&argc, &argv);

    /* data dir */
    const char *data_home = g_get_user_data_dir();
    char *data_dir = g_build_filename(data_home, "wed", NULL);
    g_mkdir_with_parents(data_dir, 0700);

    if (wed_init(data_dir) != 0) {
        g_warning("core init failed; running with defaults");
    }

    theme_init();
    icons_init();
    theme_apply();

    /* load filter lists (EasyList + EasyPrivacy) */
    char *easy = read_file("resources/lists/easylist.txt");
    char *priv = read_file("resources/lists/easyprivacy.txt");
    if (easy) { int n = wed_load_filters(easy, 0); g_print("WED: easylist rules: %d\n", n); free(easy); }
    if (priv) { int n = wed_load_filters(priv, 1); g_print("WED: easyprivacy rules: %d\n", n); free(priv); }

    /* start the filtering proxy and point WebKit at it */
    int proxy_port = 0;
    if (wed_settings_bool("block.ads", TRUE) || wed_settings_bool("block.trackers", TRUE)) {
        proxy_port = wed_proxy_start();
        g_print("WED: filtering proxy on 127.0.0.1:%d\n", proxy_port);
    }

    /* website data manager + proxy settings + ITP + cookies */
    char *base = g_build_filename(data_dir, "webkit", NULL);
    g_mkdir_with_parents(base, 0700);
    char *cache_dir = g_build_filename(base, "cache", NULL);
    char *data_dir_wk = g_build_filename(base, "data", NULL);
    g_mkdir_with_parents(cache_dir, 0700);
    g_mkdir_with_parents(data_dir_wk, 0700);

    WebKitWebsiteDataManager *dm = webkit_website_data_manager_new(
        "base-data-directory", data_dir_wk,
        "base-cache-directory", cache_dir, NULL);
    WebKitWebContext *ctx = webkit_web_context_new_with_website_data_manager(dm);

    /* ITP */
    if (wed_settings_bool("privacy.itp", TRUE))
        webkit_website_data_manager_set_itp_enabled(dm, TRUE);

    /* cookies: block third-party by default */
    WebKitCookieManager *cookies = webkit_website_data_manager_get_cookie_manager(dm);
    char *cookie_policy = wed_settings_str("cookies.thirdparty", "block");
    webkit_cookie_manager_set_accept_policy(cookies,
        !strcmp(cookie_policy, "allow") ? WEBKIT_COOKIE_POLICY_ACCEPT_ALWAYS
                                        : WEBKIT_COOKIE_POLICY_ACCEPT_NO_THIRD_PARTY);
    free(cookie_policy);

    /* TLS: fail on errors (strong TLS posture) */
    webkit_website_data_manager_set_tls_errors_policy(dm, WEBKIT_TLS_ERRORS_POLICY_FAIL);

    /* proxy routing through the Rust filtering core */
    if (proxy_port > 0 && !wed_settings_bool("net.proxy", FALSE)) {
        char *proxy = g_strdup_printf("http://127.0.0.1:%d", proxy_port);
        WebKitNetworkProxySettings *ps = webkit_network_proxy_settings_new(proxy, NULL);
        webkit_website_data_manager_set_network_proxy_settings(dm,
            WEBKIT_NETWORK_PROXY_MODE_CUSTOM, ps);
        webkit_network_proxy_settings_free(ps);
        g_free(proxy);
    } else {
        webkit_website_data_manager_set_network_proxy_settings(dm,
            WEBKIT_NETWORK_PROXY_MODE_DEFAULT, NULL);
    }


    /* content filter (compiled from the Rust core rules) — async; applied
       to all live views when ready so startup is instant */
    {
        char *rules = wed_content_rules_json();
        if (rules && *rules) {
            char *filter_dir = g_build_filename(data_dir, "filters", NULL);
            g_mkdir_with_parents(filter_dir, 0700);
            WebKitUserContentFilterStore *store = webkit_user_content_filter_store_new(filter_dir);
            gsize rules_len = strlen(rules);
            GBytes *src_bytes = g_bytes_new(rules, rules_len);
            webkit_user_content_filter_store_save(store, "wed-easylist", src_bytes, NULL,
                on_filter_saved, NULL);
            g_bytes_unref(src_bytes);
            g_object_unref(store);
            g_free(filter_dir);
        }
        free(rules);
    }

    /* start-page scheme on the context our views actually use */
    startpage_register_scheme(ctx);

    /* create the window (views use ctx + compiled filter) */
    WedBrowser *b = browser_new(ctx, NULL, argc > 1 ? argv[1] : NULL, FALSE);
    b->data_manager = dm;

    gtk_main();

    /* save + cleanup */
    browser_save_session(b);
    if (g_filter) webkit_user_content_filter_unref(g_filter);
    g_free(base);
    g_free(data_dir);
    return 0;
}
