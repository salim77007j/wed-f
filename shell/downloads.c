/* downloads.c — download manager: rows with progress, pause/resume/cancel,
 * open file/folder, badge on toolbar button, history in core DB. */
#include "wed.h"

typedef struct {
    WebKitDownload *dl;
    GtkWidget *row, *name_lbl, *state_lbl, *prog, *pause, *cancel, *open;
    char *dest;
    guint64 total;
} DlRow;

static GPtrArray *dl_rows = NULL;
static WedBrowser *dlb = NULL;

static void on_received(WebKitDownload *dl, guint64 received, DlRow *r) {
    if (!r->total)
        r->total = webkit_uri_response_get_content_length(
            webkit_download_get_response(dl));
    double p = webkit_download_get_estimated_progress(dl);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(r->prog), p);
    char *s = g_strdup_printf("%.1f MB of %.1f MB",
        received / 1048576.0, r->total / 1048576.0);
    gtk_label_set_text(GTK_LABEL(r->state_lbl), s);
    g_free(s);
    downloads_update_badge(dlb);
}

static void on_finished(WebKitDownload *dl, DlRow *r) {
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(r->prog), 1.0);
    gtk_label_set_text(GTK_LABEL(r->state_lbl), "Completed");
    const char *dest = webkit_download_get_destination(dl);
    char *path = dest ? g_filename_from_uri(dest, NULL, NULL) : NULL;
    guint64 sz = webkit_download_get_received_data_length(dl);
    wed_download_record(webkit_uri_request_get_uri(webkit_download_get_request(dl)),
                        path ? path : "", (unsigned long)sz, 1);
    g_free(path);
    downloads_update_badge(dlb);
}

static void on_failed(WebKitDownload *dl, GError *err, DlRow *r) {
    gtk_label_set_text(GTK_LABEL(r->state_lbl),
        err ? err->message : "Failed");
    (void)dl;
}

static void on_cancel(GtkWidget *w, DlRow *r) {
    (void)w;
    webkit_download_cancel(r->dl);
    gtk_label_set_text(GTK_LABEL(r->state_lbl), "Cancelled");
    gtk_widget_set_sensitive(r->pause, FALSE);
    gtk_widget_set_sensitive(r->cancel, FALSE);
}

static void on_pause(GtkWidget *w, DlRow *r) {
    (void)w;
    if (webkit_download_get_estimated_progress(r->dl) >= 0) {
        /* WebKitGTK resume: cancel + re-request is the only path; mark state */
        gtk_label_set_text(GTK_LABEL(r->state_lbl), "Paused (restart to resume)");
        webkit_download_cancel(r->dl);
    }
}

static void on_open_file(GtkWidget *w, DlRow *r) {
    (void)w;
    if (r->dest) {
        char *uri = g_filename_to_uri(r->dest, NULL, NULL);
        if (uri) {
            g_app_info_launch_default_for_uri(uri, NULL, NULL);
            g_free(uri);
        }
    }
}

static void on_open_folder(GtkWidget *w, DlRow *r) {
    (void)w;
    if (r->dest) {
        char *dir = g_path_get_dirname(r->dest);
        char *uri = g_filename_to_uri(dir, NULL, NULL);
        if (uri) {
            g_app_info_launch_default_for_uri(uri, NULL, NULL);
            g_free(uri);
        }
        g_free(dir);
    }
}

static void on_decide_destination(WebKitDownload *dl, const char *suggested, DlRow *r) {
    (void)dl;
    const char *dir = g_get_user_special_dir(G_USER_DIRECTORY_DOWNLOAD);
    if (!dir) dir = g_get_home_dir();
    char *ask = wed_settings_str("downloads.ask_location", "false");
    char *file = NULL;
    if (strcmp(ask, "true") == 0) {
        GtkWidget *dlg = gtk_file_chooser_dialog_new("Save file",
            GTK_WINDOW(dlb->window), GTK_FILE_CHOOSER_ACTION_SAVE,
            "_Cancel", GTK_RESPONSE_CANCEL, "_Save", GTK_RESPONSE_ACCEPT, NULL);
        gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dlg), TRUE);
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dlg), dir);
        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dlg),
            suggested ? g_filename_display_basename(suggested) : "download");
        if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
            file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dlg));
        }
        gtk_widget_destroy(dlg);
    }
    free(ask);
    if (!file) {
        file = g_build_filename(dir,
            suggested ? g_filename_display_basename(suggested) : "download", NULL);
        /* uniquify */
        if (g_file_test(file, G_FILE_TEST_EXISTS)) {
            int i = 1;
            char *base = g_strdup(file);
            while (1) {
                g_free(file);
                file = g_strdup_printf("%s (%d)", base, i++);
                if (!g_file_test(file, G_FILE_TEST_EXISTS)) break;
            }
            g_free(base);
        }
    }
    char *uri = g_filename_to_uri(file, NULL, NULL);
    webkit_download_set_destination(dl, uri);
    g_free(uri);
    g_free(r->dest);
    r->dest = file;
    gtk_label_set_text(GTK_LABEL(r->name_lbl), g_path_get_basename(file));
}

void downloads_add(WedBrowser *b, WebKitDownload *dl) {
    if (!dl_rows) dl_rows = g_ptr_array_new();
    dlb = b;

    DlRow *r = g_new0(DlRow, 1);
    r->dl = dl;

    r->row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_name(r->row, "dlrow");

    GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    r->name_lbl = gtk_label_new(webkit_uri_request_get_uri(
        webkit_download_get_request(dl)));
    gtk_label_set_ellipsize(GTK_LABEL(r->name_lbl), PANGO_ELLIPSIZE_MIDDLE);
    gtk_label_set_xalign(GTK_LABEL(r->name_lbl), 0.0);
    gtk_box_pack_start(GTK_BOX(top), r->name_lbl, TRUE, TRUE, 0);

    r->open = wed_image_button(IC_EXTERNAL, "Open file");
    gtk_box_pack_start(GTK_BOX(top), r->open, FALSE, FALSE, 0);
    r->pause = wed_image_button(IC_VOLUME, "Pause");
    gtk_box_pack_start(GTK_BOX(top), r->pause, FALSE, FALSE, 0);
    r->cancel = wed_image_button(IC_CLOSE, "Cancel");
    gtk_box_pack_start(GTK_BOX(top), r->cancel, FALSE, FALSE, 0);

    r->prog = gtk_progress_bar_new();
    r->state_lbl = gtk_label_new("Starting…");
    gtk_label_set_xalign(GTK_LABEL(r->state_lbl), 0.0);
    gtk_widget_set_name(r->state_lbl, "secondary");

    gtk_box_pack_start(GTK_BOX(r->row), top, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(r->row), r->prog, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(r->row), r->state_lbl, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(b->downloads_list), r->row);
    gtk_widget_show_all(r->row);
    gtk_widget_show_all(b->downloads_popover);
    g_ptr_array_add(dl_rows, r);

    g_object_set_data(G_OBJECT(r->open), "row", r);
    g_object_set_data(G_OBJECT(r->cancel), "row", r);
    g_object_set_data(G_OBJECT(r->pause), "row", r);
    g_signal_connect(r->open, "clicked", G_CALLBACK(on_open_file), r);
    g_signal_connect(r->cancel, "clicked", G_CALLBACK(on_cancel), r);
    g_signal_connect(r->pause, "clicked", G_CALLBACK(on_pause), r);

    g_signal_connect(dl, "decide-destination", G_CALLBACK(on_decide_destination), r);
    g_signal_connect(dl, "received-data", G_CALLBACK(on_received), r);
    g_signal_connect(dl, "finished", G_CALLBACK(on_finished), r);
    g_signal_connect(dl, "failed", G_CALLBACK(on_failed), r);
}

static void on_context_download(WebKitWebContext *ctx, WebKitDownload *dl, gpointer ud) {
    (void)ctx;
    WedBrowser *b = ud;
    downloads_add(b, dl);
}

void downloads_init(WedBrowser *b) {
    dlb = b;
    if (!dl_rows) dl_rows = g_ptr_array_new();
    b->downloads_popover = downloads_popover_new(b);
    g_signal_connect(b->context, "download-started", G_CALLBACK(on_context_download), b);
}

GtkWidget *downloads_popover_new(WedBrowser *b) {
    GtkWidget *pop = gtk_popover_new(b->btn_downloads ? NULL : NULL);
    /* attach to the downloads button once toolbar exists */
    b->downloads_popover = pop;
    gtk_popover_set_position(GTK_POPOVER(pop), GTK_POS_BOTTOM);
    gtk_widget_set_size_request(pop, 420, 360);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *hdr = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(hdr), "<span size='large' weight='bold'>Downloads</span>");
    gtk_widget_set_margin_start(hdr, 10);
    gtk_widget_set_margin_top(hdr, 8);
    gtk_widget_set_halign(hdr, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(box), hdr, FALSE, FALSE, 0);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    GtkWidget *list = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(scroll), list);
    gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 4);
    b->downloads_list = list;

    gtk_container_add(GTK_CONTAINER(pop), box);
    gtk_widget_show_all(box);
    gtk_widget_hide(pop);
    return pop;
}

void downloads_update_badge(WedBrowser *b) {
    (void)b;
    /* simple: highlight via tooltip count */
    if (!dlb || !dlb->btn_downloads) return;
    char *tip = g_strdup_printf("Downloads (Ctrl+J) — %d this session",
        dl_rows ? (int)dl_rows->len : 0);
    gtk_widget_set_tooltip_text(dlb->btn_downloads, tip);
    g_free(tip);
}
