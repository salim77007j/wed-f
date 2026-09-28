/* ai.c — optional AI assistant. Off by default (privacy-first).
 * Runs a Node sidecar (ai/assistant.js) that calls the GLM model;
 * the browser passes the prompt on stdin and reads the reply. */
#include "wed.h"

static gboolean ai_checked = FALSE;
static gboolean ai_ok = FALSE;

static const char *SIDEKAR = "ai/assistant.js";

gboolean ai_available(void) {
    if (!ai_checked) {
        ai_checked = TRUE;
        ai_ok = wed_settings_bool("ai.enabled", FALSE)
                && g_find_program_in_path("node") != NULL
                && g_file_test(SIDEKAR, G_FILE_TEST_EXISTS);
    }
    return ai_ok;
}

/* Extract visible page text (async JS with a bounded nested loop). */
typedef struct { char *text; GMainLoop *loop; } EvalCtx;

static void on_eval_done(GObject *obj, GAsyncResult *res, gpointer ud) {
    EvalCtx *ctx = ud;
    GError *err = NULL;
    JSCValue *v = webkit_web_view_evaluate_javascript_finish(
        WEBKIT_WEB_VIEW(obj), res, &err);
    if (err) {
        g_error_free(err);
    } else if (v && jsc_value_is_string(v)) {
        ctx->text = jsc_value_to_string(v);
    } else if (v && jsc_value_is_undefined(v)) {
        ctx->text = NULL;
    }
    g_main_loop_quit(ctx->loop);
}

static char *page_text(WedTab *t) {
    if (!t || !t->webview) return g_strdup("");
    EvalCtx ctx = { NULL, g_main_loop_new(NULL, FALSE) };
    const char *script =
        "document.body ? (document.body.innerText || '').slice(0, 12000) : ''";
    webkit_web_view_evaluate_javascript(WEBKIT_WEB_VIEW(t->webview),
        script, -1, NULL, NULL, NULL, on_eval_done, &ctx);
    gulong id = g_timeout_add(1500, (GSourceFunc)g_main_loop_quit, ctx.loop);
    g_main_loop_run(ctx.loop);
    g_source_remove(id);
    g_main_loop_unref(ctx.loop);
    if (!ctx.text) ctx.text = g_strdup("");
    return ctx.text;
}

static char *ai_run_prompt(const char *prompt, const char *page_ctx) {
    if (!ai_available()) return NULL;

    char *model = wed_settings_str("ai.model", "glm-4-flash");
    char *input = g_strdup_printf("%s\n\n---PAGE CONTEXT---\n%.6000s\n",
                                  prompt, page_ctx ? page_ctx : "(none)");

    char *tmp = g_strdup("/tmp/wed-ai-prompt.txt");
    g_file_set_contents(tmp, input, -1, NULL);
    g_free(input);

    char *full = g_strdup_printf("node %s %s < %s", SIDEKAR, model, tmp);
    GError *err = NULL;
    char *std_out = NULL, *std_err = NULL;
    gint status = 0;
    g_spawn_command_line_sync(full, &std_out, &std_err, &status, &err);
    g_free(full);
    g_free(model);
    g_free(tmp);

    char *result = NULL;
    if (err) {
        g_error_free(err);
    } else if (status == 0 && std_out && *std_out) {
        /* sidecar may print logging first; keep from first '{' or all */
        result = g_strdup(g_strstrip(std_out));
    }
    g_free(std_out);
    g_free(std_err);
    return result;
}

void ai_ask_about_page(WedBrowser *b) {
    if (!ai_available()) {
        GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(b->window), GTK_DIALOG_MODAL,
            GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "AI assistant is disabled");
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d),
            "Enable it in Settings → AI Assist. It runs a local Node sidecar "
            "that calls the GLM model; page content is only sent when you "
            "explicitly ask for a summary.");
        gtk_dialog_run(GTK_DIALOG(d));
        gtk_widget_destroy(d);
        return;
    }
    WedTab *t = browser_active_tab(b);
    const char *url = t && t->url ? t->url : "";
    const char *title = t && t->title ? t->title : "";
    char *body = page_text(t);
    char *prompt = g_strdup_printf(
        "Summarize this web page in 3-5 short bullet points.\n"
        "URL: %s\nTitle: %s\n", url, title);
    char *reply = ai_run_prompt(prompt, body);
    g_free(prompt);
    g_free(body);

    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(b->window), GTK_DIALOG_MODAL,
        GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "AI — page summary");
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d),
        reply && *reply ? reply
        : "(no response — check that node is installed)");
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
    g_free(reply);
}

void ai_selection_action(GtkAction *action, gpointer ud) {
    (void)action; (void)ud;
    /* the selected text is available via the PRIMARY selection */
    GtkClipboard *cb = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
    gchar *text = gtk_clipboard_wait_for_text(cb);
    if (!text || !*text) {
        cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
        text = gtk_clipboard_wait_for_text(cb);
    }
    if (text && *text)
        ai_ask_about_selection(NULL, text);
    else
        ai_ask_about_selection(NULL, "Explain the main idea of the current page.");
    g_free(text);
}

void ai_ask_about_selection(WedBrowser *b, const char *text) {
    char *prompt = g_strdup_printf(
        "Explain or summarize the following selected text from a web page:\n\n%.4000s",
        text ? text : "");
    char *reply = ai_run_prompt(prompt, NULL);
    g_free(prompt);

    GtkWidget *win = b ? b->window : NULL;
    GtkWidget *d = gtk_message_dialog_new(GTK_WINDOW(win),
        GTK_WINDOW(win) ? GTK_DIALOG_MODAL : 0, GTK_MESSAGE_INFO, GTK_BUTTONS_OK,
        "AI — about your selection");
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(d),
        reply && *reply ? reply : "(no response)");
    gtk_dialog_run(GTK_DIALOG(d));
    gtk_widget_destroy(d);
    g_free(reply);
}
