/* theme.c — palette from UX research + full GTK CSS. */
#include "wed.h"

WedTheme g_theme;

static double C(int r, int g, int b) {
    (void)r; (void)g; (void)b; return 0;
}

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
        /* Chrome 153 light, sampled in docs/research (1600x900) */
        g_theme.tabstrip_bg   = col(211, 227, 253);   /* #D3E3FD */
        g_theme.toolbar_bg    = col(237, 242, 250);   /* #EDF2FA */
        g_theme.content_bg    = col(255, 255, 255);
        g_theme.tab_active    = col(255, 255, 255);
        g_theme.tab_text      = col(32, 33, 36);
        g_theme.tab_inactive_text = col(60, 64, 67);
        g_theme.omnibox_fill  = col(240, 243, 249);   /* unfocused pill */
        g_theme.omnibox_fill_focused = col(255, 255, 255);
        g_theme.omnibox_border = col(218, 220, 224);
        g_theme.omnibox_text = col(32, 33, 36);
        g_theme.text_primary = col(32, 33, 36);
        g_theme.text_secondary = col(95, 99, 104);
        g_theme.divider = col(228, 231, 235);
        g_theme.hover = col(232, 240, 254);
        g_theme.menu_bg = col(255, 255, 255);
        g_theme.menu_border = col(219, 223, 225);
        g_theme.menu_hover = col(241, 243, 244);
        g_theme.menu_text = col(32, 33, 36);
        g_theme.panel_bg = col(245, 248, 252);
        g_theme.card_bg = col(255, 255, 255);
        g_theme.entry_border = col(218, 220, 224);
        g_theme.danger = col(217, 48, 37);
    } else {
        g_theme.tabstrip_bg   = col(35, 37, 42);
        g_theme.toolbar_bg    = col(41, 43, 49);
        g_theme.content_bg    = col(32, 33, 36);
        g_theme.tab_active    = col(53, 55, 61);
        g_theme.tab_text      = col(238, 238, 238);
        g_theme.tab_inactive_text = col(170, 173, 177);
        g_theme.omnibox_fill  = col(59, 61, 67);
        g_theme.omnibox_fill_focused = col(66, 68, 74);
        g_theme.omnibox_border = col(75, 78, 84);
        g_theme.omnibox_text = col(238, 238, 238);
        g_theme.text_primary = col(238, 238, 238);
        g_theme.text_secondary = col(154, 160, 166);
        g_theme.divider = col(60, 62, 68);
        g_theme.hover = col(48, 50, 56);
        g_theme.menu_bg = col(45, 47, 52);
        g_theme.menu_border = col(60, 62, 68);
        g_theme.menu_hover = col(56, 58, 64);
        g_theme.menu_text = col(238, 238, 238);
        g_theme.panel_bg = col(38, 40, 45);
        g_theme.card_bg = col(45, 47, 52);
        g_theme.entry_border = col(75, 78, 84);
        g_theme.danger = col(242, 106, 90);
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
        g_theme.accent_dark = dark ? col(174, 203, 250) : col(20, 90, 185);
    }
}

static void css_rgba(GString *s, const char *name, WedColor c) {
    g_string_append_printf(s, "@define-color %s rgb(%d,%d,%d);\n", name,
                           (int)(c.r * 255), (int)(c.g * 255), (int)(c.b * 255));
}

const char *theme_css(void) {
    static GString *s = NULL;
    if (s) return s->str;
    s = g_string_sized_new(6000);
    css_rgba(s, "strip_bg", g_theme.tabstrip_bg);
    css_rgba(s, "toolbar_bg", g_theme.toolbar_bg);
    css_rgba(s, "content_bg", g_theme.content_bg);
    css_rgba(s, "tab_active", g_theme.tab_active);
    css_rgba(s, "tab_text", g_theme.tab_text);
    css_rgba(s, "tab_text_inactive", g_theme.tab_inactive_text);
    css_rgba(s, "omni_fill", g_theme.omnibox_fill);
    css_rgba(s, "omni_fill_focus", g_theme.omnibox_fill_focused);
    css_rgba(s, "omni_border", g_theme.omnibox_border);
    css_rgba(s, "text_primary", g_theme.text_primary);
    css_rgba(s, "text_secondary", g_theme.text_secondary);
    css_rgba(s, "accent", g_theme.accent);
    css_rgba(s, "accent_dark", g_theme.accent_dark);
    css_rgba(s, "divider", g_theme.divider);
    css_rgba(s, "hover", g_theme.hover);
    css_rgba(s, "menu_bg", g_theme.menu_bg);
    css_rgba(s, "menu_border", g_theme.menu_border);
    css_rgba(s, "menu_hover", g_theme.menu_hover);
    css_rgba(s, "menu_text", g_theme.menu_text);
    css_rgba(s, "panel_bg", g_theme.panel_bg);
    css_rgba(s, "card_bg", g_theme.card_bg);
    css_rgba(s, "entry_border", g_theme.entry_border);
    css_rgba(s, "danger", g_theme.danger);

    g_string_append(s,
        "* { outline-color: alpha(@accent, 0.4); }\n"
        "window { background-color: @content_bg; }\n"
        "#tabstrip { background-color: @strip_bg; }\n"
        "#toolbar { background-color: @toolbar_bg; border-bottom: 1px solid @divider; }\n"
        "#content { background-color: @content_bg; }\n"
        "button.flat, #toolbtn { background: none; border: none; border-radius: 16px;\n"
        "    padding: 3px; min-width: 24px; min-height: 24px; }\n"
        "#toolbtn:hover { background-color: alpha(@text_primary, 0.09); }\n"
        "#toolbtn:active { background-color: alpha(@text_primary, 0.14); }\n"
        "#toolbtn:disabled image { opacity: 0.35; }\n"
        "#omniwrap { border-radius: 18px; background-color: @omni_fill; padding: 0 4px; }\n"
        "#omniwrap:focus-within { background-color: @omni_fill_focus; }\n"
        "entry#omnibox { background: none; border: none; box-shadow: none;\n"
        "    padding: 4px 8px 4px 30px; color: @text_primary; caret-color: @accent;\n"
        "    font-size: 10pt; }\n"
        "#shieldlbl { font-size: 9pt; color: @text_secondary; }\n"
        "#tablbl { font-size: 9pt; color: @tab_text; }\n"
        "menu, .menu { background-color: @menu_bg; border: 1px solid @menu_border;\n"
        "    border-radius: 8px; padding: 6px 0; }\n"
        "menuitem { color: @menu_text; padding: 5px 14px 5px 10px; border-radius: 6px;\n"
        "    margin: 0 6px; }\n"
        "menuitem:hover { background-color: @menu_hover; }\n"
        "separator { background-color: @divider; min-height: 1px; min-width: 1px; }\n"
        "popover { background-color: @menu_bg; border: 1px solid @menu_border;\n"
        "    border-radius: 10px; }\n"
        "#panel { background-color: @panel_bg; border-right: 1px solid @divider; }\n"
        "#panelrow { padding: 6px 8px; border-bottom: 1px solid @divider; }\n"
        "#paneltitle { font-size: 10pt; font-weight: bold; color: @text_primary; }\n"
        "#panelurl { font-size: 8pt; color: @text_secondary; }\n"
        "#card { background-color: @card_bg; border-radius: 8px; padding: 10px; }\n"
        "#findbar { background-color: @toolbar_bg; border: 1px solid @menu_border;\n"
        "    border-radius: 8px; padding: 4px; }\n"
        "switch { border-radius: 14px; }\n"
        "checkbutton, radiobutton { color: @text_primary; padding: 2px; }\n"
        "check, radio { background-color: @card_bg; border: 1px solid @entry_border;\n"
        "    border-radius: 3px; min-width: 14px; min-height: 14px; }\n"
        "check:checked, radio:checked { background-color: @accent; color: #fff; }\n"
        "dialog .dialog-action-area button { min-width: 64px; }\n"
        "dialog { background-color: @card_bg; }\n"
        "dialog headerbar { background-color: @toolbar_bg; }\n"
        "scrolledwindow trough { background-color: @omni_fill; }\n"
        "scrollbar { background: transparent; border: none; }\n"
        "scrollbar slider { background-color: alpha(@text_secondary, 0.5);\n"
        "    border-radius: 4px; min-width: 6px; min-height: 24px; }\n"
        "scrollbar slider:hover { background-color: alpha(@text_secondary, 0.8); }\n"
        "combobox { background-color: @omni_fill; border-radius: 6px; }\n"
        "entry, textview text { background-color: @omni_fill; border: 1px solid @omni_border;\n"
        "    border-radius: 6px; padding: 4px 6px; color: @text_primary; }\n"
        "entry:focus { border-color: @accent; }\n"
        "button:not(.flat) { background-color: @omni_fill; border: 1px solid @omni_border;\n"
        "    border-radius: 6px; padding: 5px 10px; color: @text_primary; }\n"
        "button:not(.flat):hover { background-color: @hover; }\n"
        "button.suggested-action { background-color: @accent; color: #ffffff;\n"
        "    border: none; font-weight: bold; }\n"
        "button.destructive-action { background-color: @danger; color: #fff; border: none; }\n"
        "label { color: @text_primary; }\n"
        "#secondary { color: @text_secondary; }\n"
        "#statbig { font-size: 22pt; font-weight: bold; color: @accent; }\n"
        "#statlbl { color: @text_secondary; font-size: 9pt; }\n"
        "#suggrow { padding: 6px 10px; border-radius: 6px; }\n"
        "#suggrow:hover { background-color: @menu_hover; }\n"
        "#suggtitle { color: @text_primary; font-size: 9.5pt; }\n"
        "#suggurl { color: @text_secondary; font-size: 8.5pt; }\n"
        "#dlrow { padding: 8px; border-bottom: 1px solid @divider; }\n"
        "progressbar trough { background-color: @omni_fill; border-radius: 3px; min-height: 4px; }\n"
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
