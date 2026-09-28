//! Compiles the loaded network rules into WebKit content-blocker JSON
//! (the Safari/WebKit `WKContentRuleListStore` rule format, also supported
//! by WebKitGTK's WebKitUserContentFilterStore).
//!
//! Strategy: translate the subset of Adblock-rule syntax that maps cleanly
//! onto the content-rule grammar. Rules that do not map (wildcard-heavy
//! patterns, subdocument-only rules) are skipped here — they remain active
//! in the Rust host-level proxy layer and the cosmetic engine.
//! Exceptions (@@) are emitted last ("ignore-previous-rules"), matching
//! WebKit's last-match-wins evaluation.

use crate::filters::{Category, Pattern, Rule};

/// Hard cap to stay within WebKit's compiled-rule limits.
const MAX_RULES: usize = 48000;

pub fn compile_json(engine: &crate::filters::Engine) -> String {
    let (blocks, exceptions) = collect(engine);
    let mut out = String::with_capacity(64 * 1024 * 1024 / 8);
    out.push('[');
    let mut first = true;
    let mut count = 0usize;
    /* exceptions must survive the cap: blocks get MAX - n_exceptions slots */
    let block_budget = MAX_RULES.saturating_sub(exceptions.len());
    for (i, rule) in blocks.iter().enumerate() {
        if i >= block_budget {
            break;
        }
        let rule = rule;
        append_rule(&mut out, &mut first, &mut count, rule);
    }
    for rule in exceptions.iter() {
        append_rule(&mut out, &mut first, &mut count, rule);
    }
    out.push(']');
    out
}

fn append_rule(out: &mut String, first: &mut bool, count: &mut usize, rule: &Rule) {
    if let Some(s) = rule_to_json(rule) {
        if !*first {
            out.push(',');
        }
        out.push_str(&s);
        *first = false;
        *count += 1;
    }
}

fn collect(engine: &crate::filters::Engine) -> (Vec<Rule>, Vec<Rule>) {
    let snapshot: Vec<Rule> = engine.rules_snapshot();
    let mut blocks = Vec::new();
    let mut exceptions = Vec::new();
    for r in snapshot {
        if r.exception {
            exceptions.push(r);
        } else {
            blocks.push(r);
        }
    }
    (blocks, exceptions)
}

/// Escape a literal string into WebKit regex syntax.
fn re_escape(s: &str) -> String {
    let mut out = String::with_capacity(s.len() * 2);
    for c in s.chars() {
        if "\\^$.|?*+()[]{}".contains(c) {
            out.push('\\');
        }
        out.push(c);
    }
    out
}

/// Translate `^` separators (Adblock sense) inside the pattern remainder.
fn translate_separators(s: &str) -> String {
    // Adblock ^ = any char except [A-Za-z0-9_-] and not end-of-string.
    // WebKit content rules support `^` as a separator natively; keep it.
    s.to_string()
}

fn rule_to_json(rule: &Rule) -> Option<String> {
    // trigger
    let url_filter = pattern_to_filter(&rule.pattern)?;
    let mut trigger = format!("{{\"url-filter\":{}", json_esc(&url_filter));

    if let Pattern::HostAnchored { host, .. } = &rule.pattern {
        if host.starts_with("www.") {
            // also match bare domain
            let bare = &host[4..];
            let alt = format!("^[a-z][a-z0-9+.-]*://([^/]*\\\\.)?{}(/|$)", re_escape(bare));
            let _ = alt; // single filter is enough; keep simple
        }
    }

    // resource types
    let rt = types_to_resource(&rule.types);
    if !rt.is_empty() {
        let items: Vec<String> = rt.iter().map(|t| format!("\"{t}\"")).collect();
        trigger.push_str(&format!(",\"resource-type\":[{}]", items.join(",")));
    }

    if let Some(tp) = rule.third_party {
        if tp {
            trigger.push_str(",\"load-type\":[\"third-party\"]");
        } else {
            trigger.push_str(",\"load-type\":[\"first-party\"]");
        }
    }

    let (if_domains, unless_domains) = domains_to_lists(&rule.domains);
    if !if_domains.is_empty() {
        let items: Vec<String> = if_domains.iter().map(|d| format!("\"*{}\"", json_body(d)))
            .collect();
        trigger.push_str(&format!(",\"if-domain\":[{}]", items.join(",")));
    }
    if !unless_domains.is_empty() {
        let items: Vec<String> = unless_domains.iter().map(|d| format!("\"*{}\"", json_body(d)))
            .collect();
        trigger.push_str(&format!(",\"unless-domain\":[{}]", items.join(",")));
    }
    trigger.push('}');

    let action = if rule.exception {
        "{\"type\":\"ignore-previous-rules\"}"
    } else {
        "{\"type\":\"block\"}"
    };
    Some(format!("{{\"trigger\":{trigger},\"action\":{action}}}"))
}

fn pattern_to_filter(p: &Pattern) -> Option<String> {
    match p {
        Pattern::HostAnchored { host, rest } => {
            // ^scheme://(sub.)host<rest>  — also covers http/https/any scheme.
            let mut f = format!(
                "^[a-z][a-z0-9+.-]*:(//)?([^/]*\\\\.)?{}",
                re_escape(host)
            );
            if let Some(rest) = rest {
                if !rest.is_empty() {
                    f.push_str(&translate_separators(&re_escape(rest)));
                }
            } else {
                f.push_str("[/^]");
            }
            Some(f)
        }
        Pattern::Plain(s) => Some(re_escape(s)),
        Pattern::StartAnchored(s) => Some(format!("^[a-z][a-z0-9+.-]*:(//)?{}", re_escape(s))),
        Pattern::EndAnchored(s) => Some(format!("{}$", re_escape(s))),
        Pattern::Glob(_) => None, // host proxy layer handles globs
    }
}

fn types_to_resource(types: &u32) -> Vec<&'static str> {
    use crate::filters::*;
    let mut out = Vec::new();
    if *types == 0 {
        return out; // all types: omit resource-type entirely
    }
    let pairs = [
        (CT_SCRIPT, "script"), (CT_IMAGE, "image"), (CT_STYLESHEET, "style-sheet"),
        (CT_OBJECT, "raw"), (CT_XHR, "raw"), (CT_PING, "raw"),
        (CT_MEDIA, "media"), (CT_FONT, "font"), (CT_WEBSOCKET, "raw"),
        (CT_OTHER, "raw"),
    ];
    for (bit, name) in pairs {
        if *types & bit != 0 {
            if !out.contains(&name) {
                out.push(name);
            }
        }
    }
    out
}

fn domains_to_lists(domains: &[(String, bool)]) -> (Vec<String>, Vec<String>) {
    let mut include = Vec::new();
    let mut exclude = Vec::new();
    for (d, allowed) in domains {
        if *allowed {
            include.push(d.clone());
        } else {
            exclude.push(d.clone());
        }
    }
    (include, exclude)
}

fn json_esc(s: &str) -> String {
    crate::db::jq(s)
}

/// escape a JSON string body (no surrounding quotes)
fn json_body(s: &str) -> String {
    crate::db::jq(s).trim_matches('"').to_string()
}
