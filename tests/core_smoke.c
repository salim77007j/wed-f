/* C ABI smoke test for wed_core: load filters, match, compile content rules. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int wed_init(const char *data_dir);
void wed_free_string(char *s);
int wed_load_filters(const char *text, int is_privacy_list);
int wed_decide(const char *url);
int wed_shields_enabled_for(const char *host);
char *wed_content_rules_json(void);
char *wed_cosmetic_json(const char *host);
char *wed_history_json(int limit, const char *query);
char *wed_settings_schema(void);
char *wed_settings_get(const char *key, const char *def);
void wed_settings_set(const char *key, const char *value);
int wed_proxy_start(void);
int wed_bookmark_add(const char *url, const char *title, const char *folder);
char *wed_bookmarks_json(const char *folder);
char *wed_session_load(void);
void wed_session_save(const char *tabs, const char *win);
unsigned long gstats[4];
void wed_stats(unsigned long *a, unsigned long *t, unsigned long *p, unsigned long *b);

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(n + 1);
    fread(buf, 1, n, f);
    buf[n] = 0;
    fclose(f);
    return buf;
}

int main(void) {
    if (wed_init("/tmp/wedtest") != 0) { puts("init FAIL"); return 1; }
    puts("init OK");

    char *easy = read_file("resources/lists/easylist.txt");
    char *priv = read_file("resources/lists/easyprivacy.txt");
    int n1 = wed_load_filters(easy, 0);
    int n2 = wed_load_filters(priv, 1);
    printf("filter rules: easylist=%d easyprivacy=%d\n", n1, n2);
    free(easy); free(priv);

    const char *tests[] = {
        "https://doubleclick.net/ddm/adj/",
        "https://www.google-analytics.com/collect?v=1",
        "https://example.com/",
        "https://www.wikipedia.org/",
        "https://ad.doubleclick.net/click",
    };
    for (unsigned i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        printf("decide %s -> %d\n", tests[i], wed_decide(tests[i]));
    }

    char *rules = wed_content_rules_json();
    printf("content rules json: %zu bytes, head=%.60s\n", strlen(rules), rules);
    // minimal well-formedness check
    long depth = 0; int ok = rules[0] == '[';
    for (char *p = rules; *p; p++) {
        if (*p == '[' || *p == '{') depth++;
        else if (*p == ']' || *p == '}') depth--;
    }
    printf("balanced: %s (depth=%ld)\n", ok && depth == 0 ? "YES" : "NO", depth);
    wed_free_string(rules);

    char *cos = wed_cosmetic_json("example.com");
    printf("cosmetic example.com: %s\n", cos ? "present" : "none");
    if (cos) wed_free_string(cos);

    wed_history_json(0, "");
    wed_bookmark_add("https://example.com", "Example", "root");
    char *bm = wed_bookmarks_json("root");
    printf("bookmarks: %.80s\n", bm);
    wed_free_string(bm);

    char *schema = wed_settings_schema();
    printf("settings schema: %zu bytes\n", strlen(schema));
    wed_free_string(schema);

    wed_settings_set("theme.mode", "dark");
    char *v = wed_settings_get("theme.mode", "light");
    printf("settings roundtrip: %s\n", v);
    wed_free_string(v);

    wed_session_save("[{\"url\":\"https://example.com\"}]", "{\"x\":100,\"y\":50}");
    char *s = wed_session_load();
    printf("session: %.60s\n", s ? s : "(none)");
    if (s) wed_free_string(s);

    int port = wed_proxy_start();
    printf("proxy port: %d\n", port);

    wed_stats(&gstats[0], &gstats[1], &gstats[2], &gstats[3]);
    printf("stats: ads=%lu trackers=%lu pages=%lu\n", gstats[0], gstats[1], gstats[2]);
    puts("ALL CORE TESTS DONE");
    return 0;
}
