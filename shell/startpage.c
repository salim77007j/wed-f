/* startpage.c — the wed://start new-tab page: speed dial from real history,
 * search box, privacy stats strip. Served via a custom URI scheme so it is
 * a real in-engine page (like chrome://newtab). */
#include "wed.h"

static char *escape_html(const char *s) {
    if (!s) return g_strdup("");
    GString *o = g_string_new("");
    for (const char *p = s; *p; p++) {
        switch (*p) {
        case '<': g_string_append(o, "&lt;"); break;
        case '>': g_string_append(o, "&gt;"); break;
        case '&': g_string_append(o, "&amp;"); break;
        case '"': g_string_append(o, "&quot;"); break;
        case '\'': g_string_append(o, "&#39;"); break;
        default: g_string_append_c(o, *p); break;
        }
    }
    return g_string_free(o, FALSE);
}

static char *letter_color(const char *host) {
    guint h = g_str_hash(host ? host : "");
    static const char *pals[] = {
        "#1A73E8", "#0B8043", "#B06000", "#C5221F", "#8430CE", "#00838F",
        "#D81B60", "#5F6368", "#3F51B5", "#EF6C00",
    };
    return g_strdup(pals[h % 10]);
}

static void build_page(GString *html) {
    char *top = wed_top_sites_json();
    int n = json_count(top);

    gboolean grad = !strcmp(wed_settings_str("startpage.background", "gradient"), "gradient");
    gboolean dark = g_theme.dark;

    g_string_append(html, "<!DOCTYPE html><html><head><meta charset='utf-8'>");
    g_string_append(html, "<title>New tab</title><style>");
    g_string_append_printf(html,
        "* { box-sizing: border-box; font-family: system-ui, 'DejaVu Sans', sans-serif; }"
        "body { margin:0; height:100vh; display:flex; flex-direction:column; align-items:center;"
        " background: %s;", dark ? "#202124" : "#ffffff");
    if (grad) {
        g_string_append(html,
            dark ? "background: linear-gradient(160deg, #202124 0%%, #2b2f3a 60%%, #243042 100%%);"
                 : "background: linear-gradient(160deg, #ffffff 0%%, #eef2fa 55%%, #dfe7f8 100%%);");
    }
    g_string_append_printf(html,
        "color: %s; }"
        ".logo { margin-top: 9vh; font-size: 56px; font-weight: 800; letter-spacing: 6px; }"
        ".logo b { color: %s; }"
        ".stats { display:flex; gap:28px; margin:18px 0 8px; }"
        ".stat { text-align:center; }"
        ".stat b { display:block; font-size:22px; color: %s; }"
        ".stat span { font-size:11px; color: %s; }"
        ".search { display:flex; width:min(560px, 90vw); margin-top:26px; border:1px solid %s;"
        " border-radius:24px; padding:12px 18px; background: %s; }"
        ".search input { flex:1; border:none; outline:none; background:transparent;"
        " font-size:15px; color: inherit; }"
        ".dial { display:grid; grid-template-columns:repeat(6, 96px); gap:14px;"
        " margin-top:44px; }"
        ".tile { width:96px; text-decoration:none; display:flex; flex-direction:column;"
        " align-items:center; gap:8px; border-radius:12px; padding:12px 4px; }"
        ".tile:hover { background: %s; }"
        ".tile .box { width:48px; height:48px; border-radius:50%%; display:flex;"
        " align-items:center; justify-content:center; color:#fff; font-weight:700;"
        " font-size:20px; }"
        ".tile span { font-size:11px; max-width:90px; overflow:hidden;"
        " text-overflow:ellipsis; white-space:nowrap; color: %s; }"
        ".empty { margin-top:40px; color: %s; font-size:13px; }"
        "@media (max-width: 700px) { .dial { grid-template-columns:repeat(3, 96px); } }"
        "</style></head><body>",
        dark ? "#e8eaed" : "#202124",
        dark ? "#8ab4f8" : "#1A73E8",
        dark ? "#8ab4f8" : "#1A73E8",
        dark ? "#9aa0a6" : "#5f6368",
        dark ? "#5f6368" : "#dadce0",
        dark ? "#3c4043" : "#ffffff",
        dark ? "#2a2d31" : "#f1f3f4",
        dark ? "#e8eaed" : "#202124",
        dark ? "#9aa0a6" : "#5f6368");

    g_string_append(html, "<div class='logo'>WE<b>D</b></div>");

    /* live privacy stats */
    unsigned long ads = 0, tr = 0, pages = 0, bytes = 0;
    wed_stats(&ads, &tr, &pages, &bytes);
    g_string_append_printf(html,
        "<div class='stats'>"
        "<div class='stat'><b>%lu</b><span>ads blocked</span></div>"
        "<div class='stat'><b>%lu</b><span>trackers blocked</span></div>"
        "<div class='stat'><b>%lu</b><span>pages loaded</span></div>"
        "</div>", ads, tr, pages);

    /* search box → navigate via engine */
    g_string_append_printf(html,
        "<form class='search' onsubmit=\"location.href='%s'+encodeURIComponent(this.q.value); return false;\">"
        "<input name='q' placeholder='Search the web' autofocus></form>",
        search_engine_url());

    /* speed dial from real history */
    if (n > 0) {
        g_string_append(html, "<div class='dial'>");
        for (int i = 0; i < n && i < 12; i++) {
            char *u = json_array_str(top, i, "url");
            char *t = json_array_str(top, i, "title");
            if (!u) continue;
            char *host = host_from_uri(u);
            char *color = letter_color(host);
            char *ue = escape_html(u);
            char *te = escape_html(t && *t ? t : (host ? host : u));
            char letter = (te && *te) ? g_ascii_toupper(te[0]) : '?';
            g_string_append_printf(html,
                "<a class='tile' href='%s' title='%s'>"
                "<div class='box' style='background:%s'>%c</div>"
                "<span>%s</span></a>",
                ue, te, color, letter, te);
            g_free(u); g_free(t); g_free(host); g_free(color); g_free(ue); g_free(te);
        }
        g_string_append(html, "</div>");
    } else {
        g_string_append(html,
            "<div class='empty'>Sites you visit often will appear here.</div>");
    }
    free(top);

    g_string_append(html, "</body></html>");
}

static void on_scheme_request(WebKitURISchemeRequest *req, gpointer ud) {
    (void)ud;
    GString *html = g_string_sized_new(8192);
    build_page(html);

    GInputStream *stream = g_memory_input_stream_new_from_data(html->str, html->len, NULL);
    webkit_uri_scheme_request_finish(req, stream, html->len, "text/html");
    g_object_unref(stream);
    g_string_free(html, TRUE);
}

void startpage_register_scheme(WebKitWebContext *ctx) {
    webkit_web_context_register_uri_scheme(ctx, "wed",
        on_scheme_request, NULL, NULL);
}
