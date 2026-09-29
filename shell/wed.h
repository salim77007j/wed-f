/* wed.h — internal shared declarations for the WED GTK3 shell. */
#ifndef WED_SHELL_H
#define WED_SHELL_H

#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <string.h>
#include <math.h>
#include "../include/wed_core.h"

/* ---------------------------------------------------------------- palette */
typedef struct {
    double r, g, b;      /* 0..1 */
} WedColor;

typedef struct {
    WedColor tabstrip_bg, toolbar_bg, content_bg;
    WedColor tab_active, tab_hover, tab_inactive_text, tab_text;
    WedColor omnibox_fill, omnibox_fill_focused, omnibox_border, omnibox_text;
    WedColor accent, accent_dark, danger, success;
    WedColor text_primary, text_secondary, text_hint, divider, hover;
    WedColor select_fill, select_text;
    WedColor menu_bg, menu_border, menu_hover, menu_text;
    WedColor panel_bg, card_bg, field_bg, entry_border;
    gboolean dark;
} WedTheme;

extern WedTheme g_theme;
void theme_init(void);                    /* reads settings, builds g_theme */
const char *theme_css(void);              /* full GTK CSS provider source */
void theme_apply(void);

/* ---------------------------------------------------------------- icons */
typedef enum {
    IC_BACK, IC_FORWARD, IC_RELOAD, IC_HOME, IC_SHIELD, IC_SHIELD_OFF,
    IC_STAR, IC_STAR_FILLED, IC_DOWNLOAD, IC_MENU, IC_CLOSE, IC_SEARCH,
    IC_GEAR, IC_HISTORY, IC_BOOKMARK, IC_PLUS, IC_VOLUME, IC_VOLUME_MUTED,
    IC_INFO, IC_LOCK, IC_TRASH, IC_AI, IC_TAB, IC_EXTERNAL,
    /* Material set for settings/sidebar/startpage */
    IC_PERSON, IC_KEY, IC_PALETTE, IC_GLOBE, IC_LAPTOP, IC_GAUGE,
    IC_LANGUAGE, IC_ACCESS, IC_COMPUTER, IC_RESET, IC_CHEVRON,
    IC_MIC, IC_CAMERA, IC_APPS, IC_SUN, IC_CLOUDSUN, IC_MOON, IC_COOKIE,
    IC_FINGERPRINT, IC_EYE, IC_FOLDER, IC_PIN, IC_COPY, IC_SHARE,
    IC_WINDOW, IC_ZOOMIN, IC_ZOOMOUT, IC_PRINT, IC_CAST, IC_FULLSCR,
    IC_NOTIF, IC_EXT, IC_INCognito, IC_HELP, IC_COUNT
} WedIcon;

void    icons_init(void);
GdkPixbuf *icon_get(WedIcon ic, int size);          /* returns owned ref */
GdkPixbuf *icon_get_scaled(WedIcon ic, int w, int h);
cairo_surface_t *icon_surface(WedIcon ic, int size);

/* ---------------------------------------------------------------- app state */
typedef struct _WedTab WedTab;
typedef struct _WedBrowser WedBrowser;

typedef void (*PanelToggleFn)(WedBrowser *b, int panel);

struct _WedBrowser {
    GtkWidget *window;
    GtkWidget *vbox;
    GtkWidget *tabstrip;
    GtkWidget *toolbar;
    GtkWidget *bookmarkbar;
    GtkWidget *content_stack;
    GtkWidget *overlay;
    GtkWidget *findbar;
    GtkWidget *panel_revealer;      /* side panel: history/bookmarks */
    GtkWidget *panel_box;
    int         panel_kind;         /* 0 none, 1 history, 2 bookmarks */

    GPtrArray  *tabs;               /* WedTab* */
    int         active;             /* index */
    int         closing_all;

    WebKitUserContentFilter *content_filter;
    WebKitWebsiteDataManager *data_manager;
    WebKitWebContext *context;
    gint64      last_user_activity; /* for session autosave */
    guint       session_timer;
    guint       hibernate_timer;
    guint       stats_timer;

    /* toolbar widgets */
    GtkWidget *btn_back, *btn_fwd, *btn_reload, *btn_home, *btn_shield;
    GtkWidget *omnibox, *omnibox_entry, *btn_star;
    GtkWidget *btn_downloads, *btn_menu;
    GtkWidget *shield_count_lbl;

    /* omnibox suggestion popup */
    GtkWidget *sugg_popover, *sugg_list;
    GPtrArray *sugg_rows;
    char      *sugg_typed;

    GtkWidget *downloads_popover, *downloads_list;
    GtkWidget *privacy_popover;

    /* per-tab creation */
    gboolean   restoring_session;
};

struct _WedTab {
    WedBrowser *b;
    GtkWidget *box;                 /* container handed to stack */
    GtkWidget *webview;             /* NULL when hibernating */
    GtkWidget *hibernate_holder;    /* placeholder when hibernating */
    GdkPixbuf *hibernate_shot;      /* last snapshot */
    char *url;                      /* current/restore URL */
    char *title;
    GdkPixbuf *favicon;
    gboolean pinned, muted, hibernated, loading;
    double zoom;
    gint64 last_active;             /* monotonic seconds */
    char *find_text;
    WebKitFindController *fc;
    GtkWidget *spinner_tab;         /* loading indicator in tab */
};

/* ---------------------------------------------------------------- modules */
/* browser.c */
WedBrowser *browser_new(WebKitWebContext *ctx, WebKitUserContentFilter *filter,
                        const char *startup_url, gboolean private_mode);
void browser_add_tab(WedBrowser *b, const char *url);
void browser_close_tab(WedBrowser *b, int index);
void browser_switch_tab(WedBrowser *b, int index);
void browser_load_url(WedBrowser *b, const char *url);
void browser_update_nav(WedBrowser *b);
void browser_update_omnibox(WedBrowser *b);
void browser_update_tab_ui(WedBrowser *b);
void browser_save_session(WedBrowser *b);
void browser_restore_session(WedBrowser *b);
WedTab *browser_active_tab(WedBrowser *b);
void browser_toggle_find(WedBrowser *b, gboolean show);
void browser_toggle_panel(WedBrowser *b, int panel);
void browser_show_downloads(WedBrowser *b);
void browser_show_privacy(WedBrowser *b);
void browser_update_shield_ui(WedBrowser *b);
void browser_hibernate_tab(WedBrowser *b, int index);
void browser_wake_tab(WedBrowser *b, int index);
void browser_quit(WedBrowser *b);
void browser_close_tab_silent(WedBrowser *b, int index);

/* tabstrip.c */
GtkWidget *tabstrip_new(WedBrowser *b);
void tabstrip_update(WedBrowser *b);

/* toolbar.c */
GtkWidget *toolbar_new(WedBrowser *b);

/* omnibox.c */
GtkWidget *omnibox_new(WedBrowser *b);      /* returns pill container */
void omnibox_update(WedBrowser *b);
void omnibox_focus(WedBrowser *b);

/* findbar.c */
GtkWidget *findbar_new(WedBrowser *b);
void findbar_show(WedBrowser *b, gboolean show);
void findbar_search(WedBrowser *b, const char *text, gboolean forward);

/* panels.c */
GtkWidget *panel_new(WedBrowser *b, int kind);   /* 1 history 2 bookmarks */
void panel_refresh(WedBrowser *b);

/* bookmarkbar.c */
GtkWidget *bookmarkbar_new(WedBrowser *b);
void bookmarkbar_refresh(WedBrowser *b);

/* downloads.c */
void downloads_init(WedBrowser *b);
GtkWidget *downloads_popover_new(WedBrowser *b);
void downloads_add(WedBrowser *b, WebKitDownload *dl);
void downloads_update_badge(WedBrowser *b);

/* privacydash.c */
GtkWidget *privacy_popover_new(WedBrowser *b);
void privacy_refresh(WedBrowser *b);

/* settingsui.c */
void settings_window_open(WedBrowser *b);

/* startpage.c */
void startpage_register_scheme(WebKitWebContext *ctx);
void startpage_weather_start(void);

/* browser.c — cross-module JS injection into live start-page tabs */
extern WedBrowser *wed_main_browser;
void browser_inject_js(const char *js);

/* webview.c */
GtkWidget *webview_new(WedBrowser *b, WedTab *t, const char *url);
void webview_apply_privacy(WedTab *t);
void webview_zoom(WedTab *t, double z);

/* menus.c */
void menu_popup_app_menu(WedBrowser *b, GdkEventButton *ev);
void menu_popup_tab_menu(WedBrowser *b, int tab_index, GdkEventButton *ev);

/* ai.c */
void ai_ask_about_page(WedBrowser *b);
void ai_ask_about_selection(WedBrowser *b, const char *text);
void ai_selection_action(GtkAction *action, gpointer ud);
gboolean ai_available(void);

/* helpers */
char *wed_settings_str(const char *key, const char *def);
gboolean wed_settings_bool(const char *key, gboolean def);
void wed_settings_save(const char *key, const char *val);
const char *search_engine_url(void);
char *search_url_for(const char *text);
gboolean looks_like_url(const char *text);
char *host_from_uri(const char *uri);
GdkPixbuf *wed_snapshot(WedTab *t);
GtkWidget *wed_image_button(WedIcon ic, const char *tooltip);

/* json-lite helpers (no glib json dep needed for simple arrays) */
typedef struct { const char *text; int len; } JStr;
int json_count(const char *json);
/* extract nth string field "key" of the k-th object in a flat JSON array */
char *json_array_str(const char *json, int k, const char *key);
double json_array_num(const char *json, int k, const char *key);

#endif
