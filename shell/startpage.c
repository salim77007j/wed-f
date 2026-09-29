/* startpage.c — the wed://start new-tab page, redesigned to the stakeholder
 * reference (wed44.png): greeting + weather/time widgets, centered search
 * pill with drop shadow, shortcuts grid with real brand SVG icons and an
 * add-shortcut tile, footer with tagline + settings gear. All widgets are
 * real: live clock, live weather (wttr.in, CORS-enabled), shortcuts from
 * real bookmarks/history, gear opens the settings window, "+" adds a
 * shortcut through a real dialog → bookmarks DB. */
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

static char *escape_js(const char *s) {
    if (!s) return g_strdup("");
    GString *o = g_string_new("");
    for (const char *p = s; *p; p++) {
        if (*p == '\'' || *p == '\\') g_string_append_c(o, '\\');
        g_string_append_c(o, *p);
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

/* ------------------------------------------------------------ brand icons */
/* Real brand marks as inline SVG (single-color-safe, authentic palettes). */
static const char *BRAND_SVG(const char *host) {
    if (!host) return NULL;
    if (strstr(host, "google."))
        return "<svg viewBox='0 0 48 48' width='28' height='28'>"
            "<path fill='#4285F4' d='M45.1 24.5c0-1.6-.1-2.8-.4-4.1H24v7.8h11.9c-.2 2-1.5 5-4.4 7l6.7 5.2c4-3.7 6.9-9.1 6.9-15.9z'/>"
            "<path fill='#34A853' d='M24 46c6 0 11-2 14.2-5.6l-6.7-5.2c-1.8 1.2-4.2 2.1-7.5 2.1-5.8 0-10.7-3.8-12.4-9.1l-7 5.4C7.9 41.4 15.4 46 24 46z'/>"
            "<path fill='#FBBC05' d='M11.6 28.2c-.5-1.4-.7-2.8-.7-4.2s.3-2.9.7-4.2l-7-5.4C3.3 17 2.5 20.4 2.5 24s.8 7 2.1 9.6z'/>"
            "<path fill='#EA4335' d='M24 10.2c4.1 0 6.9 1.8 8.5 3.3l6.2-6C35 4.1 30 2 24 2 15.4 2 7.9 6.6 4.6 14.4l7 5.4c1.7-5.3 6.6-9.6 12.4-9.6z'/>"
            "</svg>";
    if (strstr(host, "youtube."))
        return "<svg viewBox='0 0 48 48' width='28' height='28'>"
            "<rect x='2' y='10' width='44' height='28' rx='8' fill='#FF0000'/>"
            "<path fill='#fff' d='M20 17.5v13l11-6.5z'/></svg>";
    if (strstr(host, "gmail.") || strstr(host, "mail.google"))
        return "<svg viewBox='0 0 48 48' width='28' height='28'>"
            "<rect x='3' y='11' width='42' height='26' rx='4' fill='#fff' stroke='#DADCE0'/>"
            "<path fill='#EA4335' d='M3 15l21 14L45 15v-2a2 2 0 0 0-2-2H5a2 2 0 0 0-2 2z'/>"
            "<path fill='#C5221F' d='M3 33v2a2 2 0 0 0 2 2h38a2 2 0 0 0 2-2v-2L24 20z'/></svg>";
    if (strstr(host, "drive."))
        return "<svg viewBox='0 0 48 48' width='28' height='28'>"
            "<path fill='#00AC47' d='M17.5 3h13L44 29H31z'/>"
            "<path fill='#00832D' d='M17.5 3h6.5L44 29h-6.5z' opacity='.95'/>"
            "<path fill='#2684FC' d='M11 29h6.5L30.5 3H24z'/>"
            "<path fill='#0066DA' d='M4.5 41.5L11 29h6.5l-6.5 12.5a3 3 0 0 0 2.6 1.5h26.4a3 3 0 0 0 2.6-1.5L44 29h-6.5L24 45H13.1a3 3 0 0 1-2.6-1.5z'/>"
            "</svg>";
    if (strstr(host, "wikipedia."))
        return "<svg viewBox='0 0 48 48' width='28' height='28'>"
            "<circle cx='24' cy='24' r='21' fill='#000000'/>"
            "<text x='24' y='31' text-anchor='middle' font-family='Georgia,serif' font-weight='700' font-size='20' fill='#fff'>W</text></svg>";
    if (strstr(host, "github."))
        return "<svg viewBox='0 0 24 24' width='28' height='28'>"
            "<path fill='#181717' d='M12 .297c-6.63 0-12 5.373-12 12 0 5.303 3.438 9.8 8.205 11.385.6.113.82-.258.82-.577 0-.285-.01-1.04-.015-2.04-3.338.724-4.042-1.61-4.042-1.61C4.422 18.07 3.633 17.7 3.633 17.7c-1.087-.744.084-.729.084-.729 1.205.084 1.838 1.236 1.838 1.236 1.07 1.835 2.809 1.305 3.495.998.108-.776.417-1.305.76-1.605-2.665-.3-5.466-1.332-5.466-5.93 0-1.31.465-2.38 1.235-3.22-.135-.303-.54-1.523.105-3.176 0 0 1.005-.322 3.3 1.23.96-.267 1.98-.399 3-.405 1.02.006 2.04.138 3 .405 2.28-1.552 3.285-1.23 3.285-1.23.645 1.653.24 2.873.12 3.176.765.84 1.23 1.91 1.23 3.22 0 4.61-2.805 5.625-5.475 5.92.42.36.81 1.096.81 2.22 0 1.606-.015 2.896-.015 3.286 0 .315.21.69.825.57C20.565 22.092 24 17.592 24 12.297c0-6.627-5.373-12-12-12'/></svg>";
    if (strstr(host, "facebook."))
        return "<svg viewBox='0 0 48 48' width='28' height='28'>"
            "<circle cx='24' cy='24' r='21' fill='#1877F2'/>"
            "<path fill='#fff' d='M24 9C15.7 9 9 15.7 9 24c0 7.1 5.1 13 11.9 14.4v-10h-3.6v-4.4h3.6v-3.3c0-4 2.3-6.2 5.9-6.2 1.7 0 3.4.3 3.4.3v3.8h-1.9c-1.9 0-2.5 1.2-2.5 2.5v2.9h4.3l-.7 4.4h-3.6v10C33.9 37 39 31.1 39 24c0-8.3-6.7-15-15-15z'/></svg>";
    if (strstr(host, "x.com") || strstr(host, "twitter."))
        return "<svg viewBox='0 0 24 24' width='26' height='26'>"
            "<path fill='#000' d='M18.244 2.25h3.308l-7.227 8.26 8.502 11.24H16.17l-5.214-6.817L4.99 21.75H1.68l7.73-8.835L1.254 2.25H8.08l4.713 6.231zm-1.161 17.52h1.833L7.084 4.126H5.117z'/></svg>";
    if (strstr(host, "reddit."))
        return "<svg viewBox='0 0 48 48' width='28' height='28'>"
            "<circle cx='24' cy='24' r='21' fill='#FF4500'/>"
            "<ellipse cx='24' cy='28' rx='12' ry='9' fill='#fff'/>"
            "<circle cx='19.5' cy='27' r='2' fill='#FF4500'/><circle cx='28.5' cy='27' r='2' fill='#FF4500'/>"
            "<path d='M20 32q4 3 8 0' stroke='#FF4500' stroke-width='1.6' fill='none'/>"
            "<line x1='24' y1='19' x2='26.5' y2='9' stroke='#fff' stroke-width='2'/>"
            "<circle cx='27' cy='8' r='2.6' fill='#fff'/></svg>";
    if (strstr(host, "amazon."))
        return "<svg viewBox='0 0 48 48' width='28' height='28'>"
            "<circle cx='24' cy='24' r='21' fill='#fff' stroke='#DADCE0'/>"
            "<text x='24' y='31' text-anchor='middle' font-family='Arial' font-weight='700' font-size='21' fill='#232F3E'>a</text>"
            "<path d='M15 34q9 5 18 0' stroke='#FF9900' stroke-width='2.6' fill='none' stroke-linecap='round'/>"
            "<path d='M14.6 33.2l1.2 3.6' stroke='#FF9900' stroke-width='2.2' fill='none' stroke-linecap='round'/></svg>";
    if (strstr(host, "linkedin."))
        return "<svg viewBox='0 0 48 48' width='28' height='28'>"
            "<rect x='3' y='3' width='42' height='42' rx='8' fill='#0A66C2'/>"
            "<text x='24' y='33' text-anchor='middle' font-family='Arial' font-weight='700' font-size='20' fill='#fff'>in</text></svg>";
    if (strstr(host, "stackoverflow."))
        return "<svg viewBox='0 0 48 48' width='28' height='28'>"
            "<rect x='3' y='3' width='42' height='42' rx='8' fill='#F48024'/>"
            "<rect x='13' y='27' width='22' height='3' fill='#fff'/><rect x='13' y='32' width='22' height='3' fill='#fff'/>"
            "<path d='M16 8l3-1 8 17-3 1zM22 6l3-1 8 17-3 1zM27 4l3-1 8 17-3 1z' fill='#fff'/></svg>";
    if (strstr(host, "bing."))
        return "<svg viewBox='0 0 48 48' width='28' height='28'>"
            "<circle cx='24' cy='24' r='21' fill='#008373'/>"
            "<path fill='#fff' d='M18 12l6 2v20l-6-2z'/><path fill='#fff' opacity='.7' d='M24 14l6 4v12l-6-2z'/></svg>";
    return NULL;
}

/* Default shortcut set when the profile is fresh (still real links). */
typedef struct { const char *url, *label; } DefaultSite;
static const DefaultSite DEFAULTS[] = {
    { "https://www.google.com",  "Google" },
    { "https://www.youtube.com", "YouTube" },
    { "https://www.wikipedia.org", "Wikipedia" },
    { "https://github.com",      "GitHub" },
    { "https://news.ycombinator.com", "Hacker News" },
    { "https://www.reddit.com",  "Reddit" },
};

static void tile_html(GString *html, const char *url, const char *title) {
    char *host = host_from_uri(url);
    char *ue = escape_html(url);
    char *te = escape_html(title && *title ? title : (host ? host : url));
    const char *svg = BRAND_SVG(host);
    if (svg) {
        g_string_append_printf(html,
            "<a class='tile' href='%s' title='%s'>"
            "<div class='box'>%s</div><span>%s</span></a>",
            ue, te, svg, te);
    } else {
        char *color = letter_color(host);
        char letter = (te && *te) ? g_ascii_toupper(te[0]) : '?';
        g_string_append_printf(html,
            "<a class='tile' href='%s' title='%s'>"
            "<div class='box' style='background:%s'>%c</div>"
            "<span>%s</span></a>",
            ue, te, color, letter, te);
        g_free(color);
    }
    g_free(host); g_free(ue); g_free(te);
}

static void build_page(GString *html) {
    gboolean dark = g_theme.dark;
    const char *tx  = dark ? "#e8eaed" : "#202124";
    const char *tx2 = dark ? "#9aa0a6" : "#5f6368";
    const char *txh = dark ? "#5f6368" : "#9aa0a6";
    const char *bd  = dark ? "#3c4043" : "#dadce0";
    const char *bx  = dark ? "#2d2e31" : "#ffffff";
    const char *hv  = dark ? "#28292c" : "#f1f3f4";
    const char *ac  = dark ? "#8ab4f8" : "#1a73e8";
    const char *pgbg = dark ? "#202124" : "#ffffff";

    /* live privacy numbers are surfaced in the shield popover — the NTP stays
     * clean exactly like the reference design. */

    g_string_append(html, "<!DOCTYPE html><html><head><meta charset='utf-8'>");
    g_string_append(html, "<title>New tab</title><style>");
    g_string_append_printf(html,
        "* { box-sizing: border-box; -webkit-font-smoothing: antialiased; }"
        "html, body { height: 100%%; margin: 0; }"
        "body { font-family: system-ui, 'DejaVu Sans', sans-serif; background: %s;"
        " color: %s; display: flex; flex-direction: column; }"
        /* ---- widgets row ---- */
        ".widgets { display: flex; justify-content: space-between; align-items: center;"
        " width: min(640px, 92vw); margin: 40px auto 0; padding: 0 8px; }"
        ".greet { display: flex; align-items: center; gap: 14px; }"
        ".avatar { width: 42px; height: 42px; border-radius: 50%%; flex: none;"
        "  display: flex; align-items: center; justify-content: center;"
        "  background: %s; }"
        ".greet .hi { font-size: 17px; font-weight: 500; }"
        ".greet .sub { font-size: 13px; color: %s; margin-top: 2px; }"
        ".right { display: flex; align-items: center; gap: 0; }"
        ".wx { display: flex; align-items: center; gap: 10px; padding: 0 18px; }"
        ".wx .t { font-size: 15px; font-weight: 500; }"
        ".wx .c { font-size: 12px; color: %s; margin-top: 2px; }"
        ".wx .ico { width: 30px; height: 30px; flex: none; }"
        ".vsep { width: 1px; height: 34px; background: %s; }"
        ".clock { padding: 0 0 0 18px; text-align: right; }"
        ".clock .t { font-size: 15px; font-weight: 500; font-variant-numeric: tabular-nums; }"
        ".clock .d { font-size: 12px; color: %s; margin-top: 2px; }"
        /* ---- search ---- */
        ".center { flex: 1; display: flex; flex-direction: column;"
        " align-items: center; justify-content: center; padding-bottom: 2vh; }"
        ".head { font-size: 30px; font-weight: 400; letter-spacing: -0.3px;"
        " margin: 0 0 8px; }"
        ".sub { font-size: 14px; color: %s; margin: 0 0 28px; }"
        ".search { display: flex; align-items: center; width: min(584px, 90vw);"
        " height: 52px; border: 1px solid %s; border-radius: 26px;"
        " background: %s; padding: 0 8px 0 20px;"
        " box-shadow: 0 4px 14px rgba(0,0,0,%s); transition: box-shadow .15s; }"
        ".search:focus-within { box-shadow: 0 6px 20px rgba(26,115,232,%s);"
        " border-color: %s; }"
        ".search svg { flex: none; }"
        ".search input { flex: 1; border: none; outline: none; background: transparent;"
        " font-size: 15px; color: %s; padding: 0 12px; height: 100%%; }"
        ".search input::placeholder { color: %s; }"
        ".aibtn { width: 36px; height: 36px; border-radius: 50%%; border: none;"
        " background: transparent; cursor: pointer; flex: none;"
        " display: flex; align-items: center; justify-content: center; }"
        ".aibtn:hover { background: %s; }"
        /* ---- shortcuts ---- */
        ".dial { display: grid; grid-template-columns: repeat(auto-fit, 96px);"
        " justify-content: center; gap: 4px 8px; width: min(640px, 94vw);"
        " margin-top: 40px; }"
        ".tile { width: 96px; text-decoration: none; display: flex;"
        " flex-direction: column; align-items: center; gap: 8px;"
        " border-radius: 14px; padding: 10px 2px 12px; }"
        ".tile:hover { background: %s; }"
        ".tile .box { width: 56px; height: 56px; border-radius: 50%%;"
        " display: flex; align-items: center; justify-content: center;"
        " color: #fff; font-weight: 600; font-size: 22px; }"
        ".tile .box svg { display: block; }"
        ".tile.add .box { background: %s; color: %s; font-size: 26px;"
        " font-weight: 300; }"
        ".tile span { font-size: 12px; font-weight: 500; max-width: 88px;"
        " overflow: hidden; text-overflow: ellipsis; white-space: nowrap;"
        " color: %s; }"
        /* ---- footer ---- */
        ".footer { display: flex; align-items: center; justify-content: space-between;"
        " width: min(640px, 92vw); margin: 0 auto 22px; padding: 0 8px; }"
        ".brand { display: flex; align-items: center; gap: 10px; color: %s;"
        " font-size: 12.5px; }"
        ".brand .dot { width: 22px; height: 22px; border-radius: 50%%;"
        " background: %s; color: #fff; font-size: 11px; font-weight: 700;"
        " display: flex; align-items: center; justify-content: center; }"
        ".gearbtn { width: 34px; height: 34px; border-radius: 50%%; border: none;"
        " background: transparent; cursor: pointer; display: flex;"
        " align-items: center; justify-content: center; }"
        ".gearbtn:hover { background: %s; }"
        ".gearbtn svg { display: block; }"
        "</style></head><body>",
        pgbg, tx,
        dark ? "#3c4043" : "#f1f3f4",
        tx2, tx2, bd, tx2, tx2, bd, bx,
        dark ? "0.5" : "0.08",
        dark ? "0.35" : "0.18",
        dark ? "#5f6368" : "#dadce0",
        tx, txh, hv,
        hv, hv, tx2, tx2,
        tx, ac, hv);

    /* ---- widgets: greeting (JS picks the words), weather, clock ---- */
    g_string_append(html,
        "<div class='widgets'>"
        "<div class='greet'>"
          "<div class='avatar'><svg width='24' height='24' viewBox='0 0 24 24'>"
          "<circle cx='12' cy='8' r='4' fill='none' stroke='#5f6368' stroke-width='2'/>"
          "<path d='M4 21c1.2-4 4.4-6 8-6s6.8 2 8 6' fill='none' "
          "stroke='#5f6368' stroke-width='2' stroke-linecap='round'/></svg></div>"
          "<div><div class='hi' id='greet'>Hello</div>"
          "<div class='sub'>Hope you have a great day.</div></div>"
        "</div>"
        "<div class='right'>"
          "<div class='wx' id='wx' style='display:none'>"
            "<div class='ico' id='wxico'></div>"
            "<div><div class='t' id='wxtemp'>–</div>"
            "<div class='c' id='wxcond'></div></div>"
          "</div>"
          "<div class='vsep' id='wxsep' style='display:none'></div>"
          "<div class='clock'>"
            "<div class='t' id='clk'>--:--</div>"
            "<div class='d' id='date'></div>"
          "</div>"
        "</div>"
        "</div>");

    /* ---- center: heading, sub, search pill ---- */
    const char *ph = "Search the web or enter address";
    g_string_append_printf(html,
        "<div class='center'>"
        "<h1 class='head'>Search the web</h1>"
        "<div class='sub'>Fast, private, ad-free — powered by WED.</div>"
        "<form class='search' onsubmit=\"location.href='%s'+encodeURIComponent(this.q.value); return false;\">"
        "<svg width='19' height='19' viewBox='0 0 24 24' fill='none'>"
        "<circle cx='10.5' cy='10.5' r='6.5' stroke='%s' stroke-width='2'/>"
        "<line x1='15.5' y1='15.5' x2='21' y2='21' stroke='%s' stroke-width='2' "
        "stroke-linecap='round'/></svg>"
        "<input name='q' placeholder='%s' autofocus>"
        "<button class='aibtn' type='button' title='Ask AI about this page'"
        " onclick=\"location.href='wed://ai'\">"
        "<svg width='18' height='18' viewBox='0 0 24 24' fill='%s'>"
        "<path d='M12 2l2.1 5.4L19.5 9l-4.4 3.9L16.2 19 12 16l-4.2 3 1.1-6.1L4.5 9l5.4-1.6z'/></svg>"
        "</button></form>"
        "<div class='dial'>",
        search_engine_url(),
        dark ? "#9aa0a6" : "#5f6368",
        dark ? "#9aa0a6" : "#5f6368",
        ph, ac);

    /* ---- shortcuts: user's pinned (bookmarks/startpage) → top sites →
     *      defaults to fill the row → always an Add tile ---- */
    GString *seen = g_string_new("");
    int shown = 0, MAX_TILES = 11;

    char *pinned = wed_bookmarks_json("startpage");
    for (int i = 0; i < json_count(pinned) && shown < MAX_TILES; i++) {
        char *u = json_array_str(pinned, i, "url");
        char *t = json_array_str(pinned, i, "title");
        if (u) { tile_html(html, u, t); shown++; g_string_append_printf(seen, "%s|", u); }
        g_free(u); g_free(t);
    }
    free(pinned);

    char *top = wed_top_sites_json();
    for (int i = 0; i < json_count(top) && shown < MAX_TILES; i++) {
        char *u = json_array_str(top, i, "url");
        char *t = json_array_str(top, i, "title");
        if (u && !strstr(seen->str, u)) {
            tile_html(html, u, t); shown++;
            g_string_append_printf(seen, "%s|", u);
        }
        g_free(u); g_free(t);
    }
    free(top);

    for (guint i = 0; shown < 5 && i < sizeof DEFAULTS / sizeof DEFAULTS[0]; i++) {
        if (!strstr(seen->str, DEFAULTS[i].url)) {
            tile_html(html, DEFAULTS[i].url, DEFAULTS[i].label);
            shown++;
        }
    }
    g_string_free(seen, TRUE);

    /* add-shortcut tile (opens a real GTK dialog via wed://add-shortcut) */
    g_string_append(html,
        "<a class='tile add' href='wed://add-shortcut' title='Add shortcut'>"
        "<div class='box'>+</div><span>Add shortcut</span></a>");
    g_string_append(html, "</div></div>");

    /* ---- footer: brand tagline + settings gear ---- */
    g_string_append_printf(html,
        "<div class='footer'>"
        "<div class='brand'><div class='dot'>W</div>"
        "<span>A cleaner, faster web</span></div>"
        "<a class='gearbtn' href='wed://settings' title='Settings'>"
        "<svg width='19' height='19' viewBox='0 0 24 24' fill='%s'>"
        "<path d='M19.4 13c.05-.33.08-.66.08-1s-.03-.67-.08-1l2.1-1.65a.5.5 0 0 0 .12-.64"
        "l-2-3.46a.5.5 0 0 0-.61-.22l-2.49 1a7.3 7.3 0 0 0-1.73-1l-.37-2.65A.5.5 0 0 0"
        "13.93 2h-4a.5.5 0 0 0-.49.38l-.37 2.65c-.62.26-1.2.6-1.73 1l-2.49-1a.5.5 0 0"
        " 0-.61.22l-2 3.46a.5.5 0 0 0 .12.64L4.47 11c-.05.33-.08.66-.08 1s.03.67.08"
        " 1l-2.1 1.65a.5.5 0 0 0-.12.64l2 3.46c.13.22.39.31.61.22l2.49-1c.53.4"
        " 1.11.74 1.73 1l.37 2.65c.04.22.23.38.49.38h4c.26 0 .45-.16.49-.38l.37-2.65"
        "c.62-.26 1.2-.6 1.73-1l2.49 1c.22.09.48 0 .61-.22l2-3.46a.5.5 0 0"
        " 0-.12-.64L19.4 13zM11.93 15.5a3.5 3.5 0 1 1 0-7 3.5 3.5 0 0 1 0 7z'/></svg>"
        "</a></div>",
        dark ? "#9aa0a6" : "#5f6368");

    /* ---- live clock + greeting + weather (real fetch, graceful offline) ---- */
    g_string_append(html,
        "<script>"
        "(function(){"
        "function tick(){"
        "  var d = new Date();"
        "  var h = d.getHours(), m = d.getMinutes();"
        "  var ap = h >= 12 ? 'PM' : 'AM'; h = h % 12; if (!h) h = 12;"
        "  var el = document.getElementById('clk');"
        "  if (el) el.textContent = h + ':' + (m < 10 ? '0' : '') + m + ' ' + ap;"
        "  var dd = document.getElementById('date');"
        "  if (dd) dd.textContent = d.toLocaleDateString(undefined,"
        "    { weekday: 'short', month: 'short', day: 'numeric' });"
        "  var g = document.getElementById('greet');"
        "  if (g) g.textContent = h < 12 && ap === 'AM' ? 'Good morning'"
        "    : (h < 5 && ap === 'PM' ? 'Good afternoon' : 'Good evening');"
        "}"
        "tick(); setInterval(tick, 15000);"

        /* weather icons (SVG, no emoji-font dependency) */
        "var SVG = {"
        " sun: '<svg viewBox=\"0 0 30 30\" width=\"30\" height=\"30\"><circle cx=\"15\" cy=\"15\" r=\"6\" fill=\"#FDB813\"/><path d=\"M15 2v4M15 24v4M2 15h4M24 15h4M5.6 5.6l2.8 2.8M21.6 21.6l2.8 2.8M24.4 5.6l-2.8 2.8M8.4 21.6l-2.8 2.8\" stroke=\"#FDB813\" stroke-width=\"2\" stroke-linecap=\"round\"/></svg>',"
        " pcloud: '<svg viewBox=\"0 0 30 30\" width=\"30\" height=\"30\"><circle cx=\"19\" cy=\"10\" r=\"4.5\" fill=\"#FDB813\"/><path d=\"M8 10.5a1 1 0 1 1 0-.01\" fill=\"none\"/><path d=\"M7 23h13a4.5 4.5 0 0 0 .4-9 6 6 0 0 0-11.5 1.5A4 4 0 0 0 7 23z\" fill=\"#B8C0C8\"/></svg>',"
        " cloud: '<svg viewBox=\"0 0 30 30\" width=\"30\" height=\"30\"><path d=\"M7 23h13a4.5 4.5 0 0 0 .4-9 6 6 0 0 0-11.5 1.5A4 4 0 0 0 7 23z\" fill=\"#9AA0A6\"/><path d=\"M13 17a5 5 0 0 1 9.4-1.4A4.5 4.5 0 0 0 20.4 14 6 6 0 0 0 13 17z\" fill=\"#7C848C\"/></svg>',"
        " rain: '<svg viewBox=\"0 0 30 30\" width=\"30\" height=\"30\"><path d=\"M7 20h13a4.5 4.5 0 0 0 .4-9 6 6 0 0 0-11.5 1.5A4 4 0 0 0 7 20z\" fill=\"#9AA0A6\"/><path d=\"M10 23l-1 4M15 23l-1 4M20 23l-1 4\" stroke=\"#4285F4\" stroke-width=\"2\" stroke-linecap=\"round\"/></svg>',"
        " storm: '<svg viewBox=\"0 0 30 30\" width=\"30\" height=\"30\"><path d=\"M7 19h13a4.5 4.5 0 0 0 .4-9 6 6 0 0 0-11.5 1.5A4 4 0 0 0 7 19z\" fill=\"#9AA0A6\"/><path d=\"M16 20l-4 5h3l-1 4 5-6h-3z\" fill=\"#FDB813\"/></svg>',"
        " fog: '<svg viewBox=\"0 0 30 30\" width=\"30\" height=\"30\"><path d=\"M7 13h16M5 18h20M8 23h14\" stroke=\"#9AA0A6\" stroke-width=\"2.5\" stroke-linecap=\"round\"/></svg>'"
        "};"
        "function iconFor(c){"
        "  c = (c || '').toLowerCase();"
        "  if (c.indexOf('thunder') >= 0 || c.indexOf('storm') >= 0) return SVG.storm;"
        "  if (c.indexOf('rain') >= 0 || c.indexOf('drizzle') >= 0 || c.indexOf('shower') >= 0) return SVG.rain;"
        "  if (c.indexOf('fog') >= 0 || c.indexOf('mist') >= 0 || c.indexOf('haze') >= 0) return SVG.fog;"
        "  if (c.indexOf('overcast') >= 0 || c.indexOf('cloudy') >= 0) return SVG.cloud;"
        "  if (c.indexOf('partly') >= 0 || c.indexOf('sunny') >= 0) return SVG.pcloud;"
        "  if (c.indexOf('clear') >= 0 || c.indexOf('sunny') >= 0) return SVG.sun;"
        "  return SVG.pcloud;"
        "}"
        "function applyWx(t, c){"
        "  var w = document.getElementById('wx');"
        "  if (!w) return;"
        "  document.getElementById('wxtemp').textContent = t.replace(/\\s*\\+/, '');"
        "  document.getElementById('wxcond').textContent = c;"
        "  document.getElementById('wxico').innerHTML = iconFor(c);"
        "  w.style.display = 'flex';"
        "  var s = document.getElementById('wxsep'); if (s) s.style.display = 'block';"
        "}"
        "window.wedApplyWx = applyWx;   /* C-side weather injector entry */"
        "if (navigator.onLine) {"
        "  fetch('https://wttr.in/?format=%t|%C', { signal: "
        "    AbortSignal.timeout ? AbortSignal.timeout(4000) : undefined })"
        "  .then(function(r){ return r.text(); })"
        "  .then(function(s){"
        "    var p = s.trim().split('|');"
        "    if (p.length >= 2 && p[0]) applyWx(p[0], p[1]);"
        "  }).catch(function(){});"
        "}"
        "})();</script>");

    g_string_append(html, "</body></html>");
}

static void on_scheme_request(WebKitURISchemeRequest *req, gpointer ud) {
    (void)ud;
    GString *html = g_string_sized_new(16384);
    build_page(html);

    GInputStream *stream = g_memory_input_stream_new_from_data(html->str, html->len, NULL);
    webkit_uri_scheme_request_finish(req, stream, html->len, "text/html");
    g_object_unref(stream);
    g_string_free(html, TRUE);
    /* kick the weather fetch (cached 30 min) */
    startpage_weather_start();
}

/* ---------------- weather via curl subprocess (C-side) ------------------
 * Custom (wed://) origins can't fetch() cross-origin in WebKit, so the
 * shell fetches wttr.in itself and injects the result into live start pages.
 * Uses a curl(1) subprocess: proven TLS/DNS path, zero extra deps, robust
 * offline fallback (widget simply stays hidden). */
static char *g_wx_cache = NULL;      /* "temp|cond" */
static gint64 g_wx_fetched_at = 0;
static gboolean g_wx_inflight = FALSE;

static void wx_inject(const char *temp, const char *cond) {
    if (!temp || !*temp || !cond) return;
    char *t = escape_js(temp), *c = escape_js(cond);
    char *js = g_strdup_printf(
        "if (window.wedApplyWx) wedApplyWx('%s', '%s');", t, c);
    browser_inject_js(js);
    g_free(js); g_free(t); g_free(c);
}

static void wx_parse(const char *body) {
    if (!body || !*body) return;
    char *s = g_strdup(body);
    g_strstrip(s);
    char *bar = strchr(s, '|');
    if (bar && *s) {
        *bar = 0;
        g_free(g_wx_cache);
        g_wx_cache = g_strdup_printf("%s|%s", s, bar + 1);
        g_wx_fetched_at = g_get_real_time();
        wx_inject(s, bar + 1);
    }
    g_free(s);
}

static void wx_stdout_read(GObject *src, GAsyncResult *res, gpointer ud) {
    (void)ud;
    g_wx_inflight = FALSE;
    GError *err = NULL;
    char *out = NULL;
    g_subprocess_communicate_utf8_finish(G_SUBPROCESS(src), res, &out, NULL, &err);
    if (err) { g_error_free(err); return; }
    if (out) { wx_parse(out); g_free(out); }
}

void startpage_weather_start(void) {
    gint64 now = g_get_real_time();
    if (g_wx_cache && now - g_wx_fetched_at < 1800LL * 1000000LL) {
        char *bar = strchr(g_wx_cache, '|');
        if (bar) {
            *bar = 0;
            wx_inject(g_wx_cache, bar + 1);
            *bar = '|';
        }
        return;
    }
    if (g_wx_inflight) return;
    g_wx_inflight = TRUE;

    GError *err = NULL;
    GSubprocess *sp = g_subprocess_new(
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE,
        &err, "curl", "-s", "--max-time", "5",
        "https://wttr.in/?format=%t|%C", NULL);
    if (!sp) {
        if (err) g_error_free(err);
        g_wx_inflight = FALSE;
        return;
    }
    g_subprocess_communicate_utf8_async(sp, NULL, NULL, wx_stdout_read, NULL);
    g_object_unref(sp);
}

void startpage_register_scheme(WebKitWebContext *ctx) {
    webkit_web_context_register_uri_scheme(ctx, "wed",
        on_scheme_request, NULL, NULL);
}

/* expose escape for other modules (add-shortcut dialog etc.) */
char *wed_escape_js_str(const char *s) { return escape_js(s); }
