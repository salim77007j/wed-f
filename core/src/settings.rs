//! Settings schema — the single source of truth for every setting the UI
//! exposes. The native settings window is generated from this JSON, so
//! adding a setting means adding one entry here (and reading it where it
//! applies). `default` values mirror the privacy-first product stance.

pub struct SettingDef {
    pub key: &'static str,
    pub label: &'static str,
    pub kind: &'static str, // bool | string | number | choice
    pub options: &'static [(&'static str, &'static str)],
    pub default: &'static str,
    pub section: &'static str,
    pub hint: &'static str,
}

const SECTIONS: &[&str] = &["Privacy", "Appearance", "Behavior", "Performance", "AI Assist", "Network"];

pub const SETTINGS: &[SettingDef] = &[
    // ---- Privacy ----
    SettingDef { key: "block.ads", label: "Block ads", kind: "bool", options: &[],
        default: "true", section: "Privacy", hint: "Network-level ad blocking (EasyList)" },
    SettingDef { key: "block.trackers", label: "Block trackers", kind: "bool", options: &[],
        default: "true", section: "Privacy", hint: "Cross-site trackers (EasyPrivacy)" },
    SettingDef { key: "block.cosmetic", label: "Hide ad placeholders", kind: "bool", options: &[],
        default: "true", section: "Privacy", hint: "Cosmetic filtering of leftover ad frames" },
    SettingDef { key: "fingerprint.protection", label: "Fingerprint protection", kind: "bool", options: &[],
        default: "true", section: "Privacy", hint: "Canvas / audio / WebGL / font randomization" },
    SettingDef { key: "cookies.thirdparty", label: "Third-party cookies", kind: "choice",
        options: &[("block", "Block"), ("allow", "Allow")], default: "block", section: "Privacy",
        hint: "Blocks cross-site cookies via WebKit ITP" },
    SettingDef { key: "privacy.itp", label: "Intelligent tracking prevention", kind: "bool", options: &[],
        default: "true", section: "Privacy", hint: "WebKit ITP: tracker classification + storage partitioning" },
    SettingDef { key: "privacy.doh", label: "DNS-over-HTTPS when available", kind: "bool", options: &[],
        default: "false", section: "Privacy", hint: "Resolves via HTTPS RR when the system resolver offers it" },
    SettingDef { key: "privacy.send_dnt", label: "Send Do-Not-Track header", kind: "bool", options: &[],
        default: "false", section: "Privacy", hint: "Adds the DNT signal to requests" },
    SettingDef { key: "privacy.send_gpc", label: "Send Global Privacy Control", kind: "bool", options: &[],
        default: "true", section: "Privacy", hint: "Legally-binding opt-out signal (GPC)" },

    // ---- Appearance ----
    SettingDef { key: "theme.mode", label: "Theme", kind: "choice",
        options: &[("light", "Light"), ("dark", "Dark"), ("system", "Follow system")],
        default: "light", section: "Appearance", hint: "" },
    SettingDef { key: "theme.accent", label: "Accent color", kind: "choice",
        options: &[("blue", "Blue"), ("teal", "Teal"), ("violet", "Violet"), ("rose", "Rose")],
        default: "blue", section: "Appearance", hint: "" },
    SettingDef { key: "toolbar.compact", label: "Compact toolbar", kind: "bool", options: &[],
        default: "false", section: "Appearance", hint: "40px toolbar instead of 46px" },
    SettingDef { key: "startpage.show_bookmarks", label: "Show bookmarks bar on start page", kind: "bool",
        options: &[], default: "true", section: "Appearance", hint: "" },
    SettingDef { key: "startpage.background", label: "Start page wallpaper", kind: "choice",
        options: &[("plain", "Plain"), ("gradient", "Gradient")], default: "gradient",
        section: "Appearance", hint: "" },

    // ---- Behavior ----
    SettingDef { key: "startpage.search", label: "Search engine", kind: "choice",
        options: &[("duckduckgo", "DuckDuckGo"), ("google", "Google"), ("bing", "Bing"),
                   ("wikipedia", "Wikipedia")],
        default: "duckduckgo", section: "Behavior", hint: "" },
    SettingDef { key: "downloads.ask_location", label: "Ask where to save downloads", kind: "bool",
        options: &[], default: "false", section: "Behavior", hint: "" },
    SettingDef { key: "downloads.dir", label: "Download folder", kind: "string", options: &[],
        default: "", section: "Behavior", hint: "Empty = system Downloads folder" },
    SettingDef { key: "session.restore_prompt", label: "Ask before restoring session", kind: "bool",
        options: &[], default: "true", section: "Behavior", hint: "Crash/quit recovery" },
    SettingDef { key: "tab.warn_on_close", label: "Warn when closing multiple tabs", kind: "bool",
        options: &[], default: "true", section: "Behavior", hint: "" },
    SettingDef { key: "find.wrap", label: "Find bar wraps around", kind: "bool", options: &[],
        default: "true", section: "Behavior", hint: "" },

    // ---- Performance ----
    SettingDef { key: "hibernate.enabled", label: "Memory Saver (tab hibernation)", kind: "bool",
        options: &[], default: "true", section: "Performance",
        hint: "Unloads inactive tabs and restores on click" },
    SettingDef { key: "hibernate.idle.minutes", label: "Hibernate after (minutes)", kind: "number",
        options: &[], default: "10", section: "Performance", hint: "" },
    SettingDef { key: "cache.disk_mb", label: "Disk cache size (MB)", kind: "number",
        options: &[], default: "128", section: "Performance", hint: "" },
    SettingDef { key: "gpu.software_fallback", label: "Force software rendering", kind: "bool",
        options: &[], default: "false", section: "Performance",
        hint: "For GPU-less machines (CPU rendering via WebKit software path)" },

    // ---- AI Assist ----
    SettingDef { key: "ai.enabled", label: "Enable AI page assistant", kind: "bool", options: &[],
        default: "false", section: "AI Assist",
        hint: "Optional Node sidecar; off by default (privacy first)" },
    SettingDef { key: "ai.model", label: "AI model", kind: "choice",
        options: &[("glm-4-flash", "GLM-4 Flash (fast)"), ("glm-4.5", "GLM-4.5 (deep)")],
        default: "glm-4-flash", section: "AI Assist", hint: "" },

    // ---- Network ----
    SettingDef { key: "net.https_first", label: "HTTPS-first upgrades", kind: "bool", options: &[],
        default: "true", section: "Network", hint: "Retry http:// pages over https://" },
    SettingDef { key: "net.proxy", label: "Use system proxy", kind: "bool", options: &[],
        default: "false", section: "Network",
        hint: "WED routes through its own filtering loopback proxy otherwise" },
];

pub fn schema_json() -> String {
    let mut out = String::from("[");
    let mut first = true;
    for s in SETTINGS {
        if !first {
            out.push(',');
        }
        first = false;
        let opts: Vec<String> = s.options.iter()
            .map(|(v, l)| format!("{{\"value\":{},\"label\":{}}}", crate::db::jq(v), crate::db::jq(l)))
            .collect();
        out.push_str(&format!(
            "{{\"key\":{},\"label\":{},\"kind\":{},\"options\":[{}],\"default\":{},\"section\":{},\"hint\":{}}}",
            crate::db::jq(s.key), crate::db::jq(s.label), crate::db::jq(s.kind),
            options_join(&opts), crate::db::jq(s.default),
            crate::db::jq(s.section), crate::db::jq(s.hint)));
    }
    out.push(']');
    out
}

fn options_join(opts: &[String]) -> String {
    opts.join(",")
}

/// Sections in display order.
pub fn sections() -> &'static [&'static str] {
    SECTIONS
}
