/* wed_core.h — C ABI for the WED browser product core (Rust).
 * Single source of truth for the shell ↔ core boundary.
 * All returned strings are heap-allocated; free with wed_free_string(). */
#ifndef WED_CORE_H
#define WED_CORE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- lifecycle ---- */
int  wed_init(const char *data_dir);
void wed_free_string(char *s);

/* ---- filtering ---- */
int  wed_load_filters(const char *text, int is_privacy_list);
int  wed_decide(const char *url);
int  wed_shields_enabled_for(const char *host);
void wed_set_shields(const char *host, int enabled);
char *wed_cosmetic_json(const char *host);       /* {"generic":[],"specific":[]} */
char *wed_content_rules_json(void);              /* WebKit content-blocker rules */

/* ---- local filtering proxy ---- */
int  wed_proxy_start(void);
int  wed_proxy_port(void);
void wed_proxy_set_enabled(int enabled);

/* ---- statistics ---- */
void wed_stats(unsigned long *blocked_ads, unsigned long *blocked_trackers,
               unsigned long *pages, unsigned long *saved_bytes);
void wed_record_page_load(void);
char *wed_site_stats_json(void);

/* ---- history ---- */
void wed_history_add(const char *url, const char *title);
char *wed_history_json(int limit, const char *query);
void wed_history_clear(void);
void wed_history_remove(const char *url);
char *wed_top_sites_json(void);

/* ---- bookmarks ---- */
int  wed_bookmark_add(const char *url, const char *title, const char *folder);
void wed_bookmark_remove(const char *url);
char *wed_bookmarks_json(const char *folder);
int  wed_is_bookmarked(const char *url);

/* ---- session ---- */
void wed_session_save(const char *tabs_json, const char *window_state);
char *wed_session_load(void);
char *wed_session_window_state(void);

/* ---- settings ---- */
char *wed_settings_get(const char *key, const char *default_value);
void wed_settings_set(const char *key, const char *value);
char *wed_settings_schema(void);

/* ---- permissions & site exceptions ---- */
void wed_permission_set(const char *host, const char *kind, const char *value);
char *wed_permission_get(const char *host, const char *kind);
char *wed_site_settings_json(void);
void wed_site_reset(const char *host);

/* ---- downloads ---- */
void wed_download_record(const char *url, const char *path,
                         unsigned long size, int ok);
char *wed_downloads_json(void);

/* ---- efficiency governor ---- */
int  wed_governor_should_hibernate(int idle_seconds, int is_playing_media);

#ifdef __cplusplus
}
#endif
#endif /* WED_CORE_H */
