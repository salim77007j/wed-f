/* theme.c — Chrome-accurate palette measured from the stakeholder reference
 * designs (wed44 NTP + wed45 Settings) + full Material-style GTK CSS.
 * Light: strip #F1F3F4, toolbar #FFFFFF, active tab #FFFFFF, accent #1A73E8.
 * Dark:  strip #202124, toolbar #292A2D, active tab #35363B, accent #8AB4F8. */
#include "wed.h"

WedTheme g_theme;

static WedColor col(int r, int g, int b) {
    WedColor c = { r / 255.0, g / 255.0, b / 255.0 };
    return c;
}

void theme_init(void) {
    const char *mode = wed_settings_str("theme.mode", "light");
    const char *accent = wed_settings_str("theme.accent", "blue");
    gboolean dark = strcmp(mode, "dark") == 0;
    memset(&g_theme, 0, sizeof g_theme);
    g_theme.dark = dark;

    if (!dark) {
        /* measured from wed44/wed45 references */
        g_theme.tabstrip_bg   = col(241, 243, 244);   /* #F1F3F4 */
        g_theme.toolbar_bg    = col(255, 255, 255);   /* toolbar white */
        g_theme.content_bg    = col(255, 255, 255);
        g_theme.tab_active    = col(255, 255, 255);
        g_theme.tab_hover     = col(232, 234, 237);   /* #E8EAED */
        g_theme.tab_text      = col(32, 33, 36);      /* #202124 */
        g_theme.tab_inactive_text = col(95, 99, 104); /* #5F6368 */
        g_theme.omnibox_fill  = col(255, 255, 255);
        g_theme.omnibox_fill_focused = col(255, 255, 255);
        g_theme.omnibox_border = col(218, 220, 224);  /* #DADCE0 */
        g_theme.omnibox_text = col(32, 33, 36);
        g_theme.text_primary = col(32, 33, 36);
        g_theme.text_secondary = col(95, 99, 104);    /* #5F6368 */
        g_theme.text_hint = col(154, 160, 166);       /* #9AA0A6 */
        g_theme.divider = col(218, 220, 224);         /* #DADCE0 */
        g_theme.hover = col(241, 243, 244);           /* #F1F3F4 */
        g_theme.select_fill = col(232, 240, 254);     /* #E8F0FE */
        g_theme.select_text = col(25, 103, 210);      /* #1967D2 */
        g_theme.menu_bg = col(255, 255, 255);
        g_theme.menu_border = col(218, 220, 224);
        g_theme.menu_hover = col(241, 243, 244);
        g_theme.menu_text = col(32, 33, 36);
        g_theme.panel_bg = col(248, 249, 250);        /* #F8F9FA */
        g_theme.card_bg = col(255, 255, 255);
        g_theme.field_bg = col(241, 243, 244);        /* #F1F3F4 */
        g_theme.entry_border = col(218, 220, 224);
        g_theme.danger = col(217, 48, 37);            /* #D93025 */
        g_theme.success = col(11, 128, 67);           /* #0B8043 */
    } else {
        g_theme.tabstrip_bg   = col(32, 34, 37);      /* #202225 */
        g_theme.toolbar_bg    = col(41, 42, 45);      /* #292A2D */
        g_theme.content_bg    = col(32, 33, 36);
        g_theme.tab_active    = col(53, 54, 59);      /* #35363B */
        g_theme.tab_hover     = col(66, 67, 73);
        g_theme.tab_text      = col(232, 234, 237);
        g_theme.tab_inactive_text = col(154, 160, 166);
        g_theme.omnibox_fill  = col(60, 64, 67);      /* #3C4043 */
        g_theme.omnibox_fill_focused = col(32, 33, 36);
        g_theme.omnibox_border = col(95, 99, 104);
        g_theme.omnibox_text = col(232, 234, 237);
        g_theme.text_primary = col(232, 234, 237);
        g_theme.text_secondary = col(154, 160, 166);
        g_theme.text_hint = col(128, 134, 139);
        g_theme.divider = col(60, 64, 67);
        g_theme.hover = col(66, 67, 73);
        g_theme.select_fill = col(48, 63, 90);
        g_theme.select_text = col(138, 180, 248);     /* #8AB4F8 */
        g_theme.menu_bg = col(45, 47, 50);
        g_theme.menu_border = col(60, 64, 67);
        g_theme.menu_hover = col(56, 58, 64);
        g_theme.menu_text = col(232, 234, 237);
        g_theme.panel_bg = col(38, 39, 42);
        g_theme.card_bg = col(45, 47, 50);
        g_theme.field_bg = col(60, 64, 67);
        g_theme.entry_border = col(95, 99, 104);
        g_theme.danger = col(242, 106, 90);
        g_theme.success = col(129, 201, 149);
    }

    if (strcmp(accent, "teal") == 0) {
        g_theme.accent = dark ? col(52, 168, 163) : col(0, 128, 124);
        g_theme.accent_dark = dark ? col(72, 200, 190) : col(0, 100, 96);
    } else if (strcmp(accent, "violet") == 0) {
        g_theme.accent = dark ? col(160, 120, 240) : col(124, 77, 200);
        g_theme.accent_dark = dark ? col(190, 160, 250) : col(100, 60, 175);
    } else if (strcmp(accent, "rose") == 0) {
        g_theme.accent = dark ? col(240, 110, 130) : col(200, 60, 90);
        g_theme.accent_dark = dark ? col(250, 150, 165) : col(170, 40, 70);
    } else {
        g_theme.accent = dark ? col(138, 180, 250) : col(26, 115, 232);  /* #1A73E8 */
        g_theme.accent_dark = dark ? col(174, 203, 250) : col(25, 103, 210);
    }
}

static void css_rgba(GString *s, const char *name, WedColor c) {
    g_string_append_printf(s, "@define-color %s rgb(%d,%d,%d);\n", name,
                           (int)(c.r * 255), (int)(c.g * 255), (int)(c.b * 255));
}

const char *theme_css(void) {
    static GString *s = NULL;
    if (s) return s->str;
    s = g_string_sized_new(9000);
    css_rgba(s, "strip_bg", g_theme.tabstrip_bg);
    css_rgba(s, "toolbar_bg", g_theme.toolbar_bg);
    css_rgba(s, "content_bg", g_theme.content_bg);
    css_rgba(s, "tab_active", g_theme.tab_active);
    css_rgba(s, "tab_hover", g_theme.tab_hover);
    css_rgba(s, "tab_text", g_theme.tab_text);
    css_rgba(s, "tab_text_inactive", g_theme.tab_inactive_text);
    css_rgba(s, "omni_fill", g_theme.omnibox_fill);
    css_rgba(s, "omni_fill_focus", g_theme.omnibox_fill_focused);
    css_rgba(s, "omni_border", g_theme.omnibox_border);
    css_rgba(s, "text_primary", g_theme.text_primary);
    css_rgba(s, "text_secondary", g_theme.text_secondary);
    css_rgba(s, "text_hint", g_theme.text_hint);
    css_rgba(s, "accent", g_theme.accent);
    css_rgba(s, "accent_dark", g_theme.accent_dark);
    css_rgba(s, "divider", g_theme.divider);
    css_rgba(s, "hover", g_theme.hover);
    css_rgba(s, "select_fill", g_theme.select_fill);
    css_rgba(s, "select_text", g_theme.select_text);
    css_rgba(s, "menu_bg", g_theme.menu_bg);
    css_rgba(s, "menu_border", g_theme.menu_border);
    css_rgba(s, "menu_hover", g_theme.menu_hover);
    css_rgba(s, "menu_text", g_theme.menu_text);
    css_rgba(s, "panel_bg", g_theme.panel_bg);
    css_rgba(s, "card_bg", g_theme.card_bg);
    css_rgba(s, "field_bg", g_theme.field_bg);
    css_rgba(s, "entry_border", g_theme.entry_border);
    css_rgba(s, "danger", g_theme.danger);
    css_rgba(s, "success", g_theme.success);

    g_string_append(s,
        /* ---------- base ---------- */
        "* { outline-color: alpha(@accent, 0.4); outline-radius: 4px; }\n"
        "window { background-color: @content_bg; }\n"

        /* ---------- tab strip / toolbar / bookmarks bar ---------- */
        "#tabstrip { background-color: @strip_bg; }\n"
        "#toolbar { background-color: @toolbar_bg; }\n"
        "#bookmarkbar { background-color: @toolbar_bg; border-bottom: 1px solid @divider; }\n"
        "#bmitem { border-radius: 14px; padding: 3px 8px; color: @text_primary; }\n"
        "#bmitem:hover { background-color: @hover; }\n"
        "#bmlbl { font-size: 9pt; color: @text_primary; }\n"

        /* ---------- buttons ---------- */
        "button.flat, #toolbtn { background: none; border: none; border-radius: 16px;\n"
        "    padding: 4px; min-width: 24px; min-height: 24px; }\n"
        "#toolbtn:hover { background-color: alpha(@text_primary, 0.08); }\n"
        "#toolbtn:active { background-color: alpha(@text_primary, 0.14); }\n"
        "#toolbtn:disabled image { opacity: 0.35; }\n"

        /* ---------- omnibox pill ---------- */
        "entry#omnibox { background: none; border: none; box-shadow: none;\n"
        "    padding: 5px 6px; color: @text_primary; caret-color: @accent;\n"
        "    font-size: 10pt; }\n"
        "entry#omnibox placeholder { color: @text_hint; }\n"
        "#shieldlbl { font-size: 9pt; color: @text_secondary; }\n"
        "#tablbl { font-size: 9pt; color: @tab_text; }\n"
        "#tablbl-dim { font-size: 9pt; color: @tab_text_inactive; }\n"

        /* ---------- menus (Material surface) ---------- */
        "menu, .menu { background-color: @menu_bg; border: 1px solid @menu_border;\n"
        "    border-radius: 8px; padding: 6px 0; }\n"
        "menuitem { color: @menu_text; padding: 6px 14px 6px 10px; border-radius: 6px;\n"
        "    margin: 0 6px; }\n"
        "menuitem:hover { background-color: @menu_hover; }\n"
        "separator { background-color: @divider; min-height: 1px; min-width: 1px; }\n"
        "popover { background-color: @menu_bg; border: 1px solid @menu_border;\n"
        "    border-radius: 10px; }\n"

        /* ---------- settings: Chrome-style sidebar + rows ---------- */
        "#settingswin { background-color: @card_bg; }\n"
        "#sidebar { background-color: @card_bg; border-right: 1px solid @divider; }\n"
        "#sideitem { border-radius: 18px; padding: 8px 14px; margin: 1px 12px;\n"
        "    background: none; border: none; }\n"
        "#sideitem:hover { background-color: @hover; }\n"
        "#sideitem-on { border-radius: 18px; padding: 8px 14px; margin: 1px 12px;\n"
        "    background-color: @select_fill; }\n"
        "#sideitem-on label, #sideitem-on image { color: @select_text; }\n"
        "#sidelbl { font-size: 9.5pt; color: @text_primary; }\n"
        "#pagetitle { font-size: 15pt; font-weight: 400; color: @text_primary; }\n"
        "#sectionhdr { font-size: 10.5pt; font-weight: 500; color: @text_primary; }\n"
        "#settingrow { padding: 12px 16px; }\n"
        "#settingrow:hover { background-color: @hover; }\n"
        "#rowtitle { font-size: 9.5pt; font-weight: 500; color: @text_primary; }\n"
        "#rowhint { font-size: 8.5pt; color: @text_secondary; }\n"
        "#rowdivider { background-color: @divider; min-height: 1px;\n"
        "    margin-left: 56px; margin-right: 16px; }\n"
        "#searchpill entry { background-color: @field_bg; border: none;\n"
        "    border-radius: 18px; padding: 8px 12px 8px 36px; }\n"

        /* ---------- panels: history/bookmarks (Material rows) ---------- */
        "#panel { background-color: @card_bg; border-right: 1px solid @divider; }\n"
        "#panelhdr { padding: 10px 14px; }\n"
        "#paneltitle { font-size: 12pt; font-weight: 500; color: @text_primary; }\n"
        "#panelrow { padding: 8px 14px; }\n"
        "#panelrow:hover { background-color: @hover; }\n"
        "#panelrowtitle { font-size: 9pt; color: @text_primary; }\n"
        "#panelurl { font-size: 8pt; color: @text_secondary; }\n"
        "#card { background-color: @card_bg; border-radius: 8px; padding: 12px; }\n"

        /* ---------- find bar ---------- */
        "#findbar { background-color: @toolbar_bg; border: 1px solid @menu_border;\n"
        "    border-radius: 8px; padding: 4px; }\n"

        /* ---------- switches (Material 3 style) ---------- */
        "switch { border-radius: 14px; min-width: 34px; min-height: 14px;\n"
        "    background-color: alpha(@text_secondary, 0.4); }\n"
        "switch slider { min-width: 18px; min-height: 18px; margin: 0; border-radius: 12px;\n"
        "    background-color: #ffffff; border: 1px solid alpha(@text_primary,0.3); }\n"
        "switch:checked { background-color: @accent; }\n"
        "switch:checked slider { background-color: @card_bg; border-color: @accent; }\n"

        "checkbutton, radiobutton { color: @text_primary; padding: 2px; }\n"
        "check, radio { background-color: @card_bg; border: 1px solid @entry_border;\n"
        "    border-radius: 3px; min-width: 14px; min-height: 14px; }\n"
        "check:checked, radio:checked { background-color: @accent; color: #fff; }\n"
        "dialog .dialog-action-area button { min-width: 64px; }\n"
        "dialog { background-color: @card_bg; }\n"
        "dialog headerbar { background-color: @toolbar_bg; }\n"
        "scrolledwindow trough { background-color: @field_bg; }\n"
        "scrollbar { background: transparent; border: none; }\n"
        "scrollbar slider { background-color: alpha(@text_secondary, 0.5);\n"
        "    border-radius: 4px; min-width: 6px; min-height: 24px; }\n"
        "scrollbar slider:hover { background-color: alpha(@text_secondary, 0.8); }\n"

        /* ---------- combobox + entries (rounded, Chrome-like) ---------- */
        "combobox { border-radius: 6px; }\n"
        "combobox entry, combobox button { padding: 4px 10px; }\n"
        "entry, textview text { background-color: @field_bg; border: 1px solid @entry_border;\n"
        "    border-radius: 6px; padding: 5px 8px; color: @text_primary; }\n"
        "entry:focus { border-color: @accent; }\n"
        "button:not(.flat) { background-color: @card_bg; border: 1px solid @entry_border;\n"
        "    border-radius: 16px; padding: 6px 16px; color: @text_primary; }\n"
        "button:not(.flat):hover { background-color: @hover; }\n"
        "button.suggested-action { background-color: @accent; color: #ffffff;\n"
        "    border: none; font-weight: bold; border-radius: 16px; }\n"
        "button.destructive-action { background-color: @danger; color: #fff;\n"
        "    border: none; border-radius: 16px; }\n"

        "label { color: @text_primary; }\n"
        "#secondary { color: @text_secondary; }\n"
        "#hint { color: @text_hint; }\n"
        "#statbig { font-size: 20pt; font-weight: 300; color: @text_primary; }\n"
        "#statlbl { color: @text_secondary; font-size: 9pt; }\n"

        /* ---------- omnibox suggestions ---------- */
        "#suggrow { padding: 7px 12px; border-radius: 6px; }\n"
        "#suggrow:hover { background-color: @hover; }\n"
        "#suggtitle { color: @text_primary; font-size: 9.5pt; }\n"
        "#suggurl { color: @text_secondary; font-size: 8.5pt; }\n"

        /* ---------- downloads ---------- */
        "#dlrow { padding: 10px 14px; }\n"
        "#dlrow:hover { background-color: @hover; }\n"
        "progressbar trough { background-color: @field_bg; border-radius: 3px; min-height: 4px; }\n"
        "progressbar progress { background-color: @accent; border-radius: 3px; min-height: 4px; }\n"
        "notebook header { background-color: @toolbar_bg; }\n");
    return s->str;
}

void theme_apply(void) {
    GtkCssProvider *p = gtk_css_provider_new();
    gtk_css_provider_load_from_data(p, theme_css(), -1, NULL);
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(p), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(p);
}
