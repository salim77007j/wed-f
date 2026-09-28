//! URL filter engine — Adblock Plus syntax subset with token-indexed matching.
//! Supports: ||host^ anchors, |start / end| anchors, * wildcard, ^ separator,
//! $options (domain=, third-party, content types, ~negations), @@exceptions.

use std::collections::{HashMap, HashSet};

// Content type bitmask (mirrors what C++ sends from QWebEngineUrlRequestInfo)
pub const CT_SCRIPT: u32 = 1;
pub const CT_IMAGE: u32 = 2;
pub const CT_STYLESHEET: u32 = 4;
pub const CT_OBJECT: u32 = 8;
pub const CT_XHR: u32 = 16;
pub const CT_SUBDOCUMENT: u32 = 32;
pub const CT_PING: u32 = 64;
pub const CT_MEDIA: u32 = 128;
pub const CT_FONT: u32 = 256;
pub const CT_OTHER: u32 = 512;
pub const CT_WEBSOCKET: u32 = 1024;
pub const CT_MAINFRAME: u32 = 2048;

#[derive(Clone, Debug)]
pub enum Pattern {
    /// Plain substring (fast path: token indexed)
    Plain(String),
    /// `||host^rest` — anchored to scheme + host boundary
    HostAnchored { host: String, rest: Option<String> },
    /// `|url` anchored at start
    StartAnchored(String),
    /// `url|` anchored at end
    EndAnchored(String),
    /// Contains wildcard(s) — regex-free glob matcher
    Glob(Vec<GlobPart>),
}

#[derive(Clone, Debug, PartialEq)]
pub enum GlobPart {
    Literal(String),
    Wildcard,
}

#[derive(Clone, Debug)]
pub struct Rule {
    pub pattern: Pattern,
    pub exception: bool,
    pub types: u32,          // 0 = any type
    pub third_party: Option<bool>, // Some(true) = third-party only, Some(false) = first-party only
    pub domains: Vec<(String, bool)>, // (domain, allowed)
    pub category: Category,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Category {
    Ad,
    Tracker,
    Custom,
}

impl Rule {
    fn matches_types(&self, ct: u32) -> bool {
        if self.types == 0 {
            return true;
        }
        self.types & ct != 0
    }
}

pub struct Engine {
    rules: Vec<Rule>,
    /// token (>=3 alnum chars) -> rule indices
    index: HashMap<String, Vec<u32>>,
    /// rules whose pattern yields no usable token (wildcards, short patterns)
    unindexed: Vec<u32>,
    /// exception rules are always checked fully (they are few relative to blocks)
    exceptions: Vec<u32>,
    /// site allowlist (host suffix matches) — user-controlled
    allowlist: HashSet<String>,
    /// registrable-domain heuristic table
    multi_tlds: HashSet<&'static str>,
    pub stats_ads: u64,
    pub stats_trackers: u64,
}

pub enum Decision {
    Allow,
    Block(Category),
}

impl Engine {
    pub fn new() -> Engine {
        let mut multi = HashSet::new();
        for t in [
            "co.uk", "org.uk", "ac.uk", "gov.uk", "co.jp", "or.jp", "ne.jp", "co.kr", "co.za",
            "com.au", "net.au", "org.au", "com.br", "com.cn", "com.tw", "com.hk", "com.sg",
            "com.my", "com.ar", "com.mx", "co.in", "co.id", "com.tr", "com.vn", "com.ph",
            "com.ng", "co.ke", "com.eg", "co.il", "com.pk", "com.bd", "co.nz",
        ] {
            multi.insert(t);
        }
        Engine {
            rules: Vec::new(),
            index: HashMap::new(),
            unindexed: Vec::new(),
            exceptions: Vec::new(),
            allowlist: HashSet::new(),
            multi_tlds: multi,
            stats_ads: 0,
            stats_trackers: 0,
        }
    }

    pub fn clear(&mut self) {
        self.rules.clear();
        self.index.clear();
        self.unindexed.clear();
        self.exceptions.clear();
    }

    pub fn rule_count(&self) -> usize {
        self.rules.len()
    }

    /// Full copy of parsed rules (for the content-blocker compiler).
    pub fn rules_snapshot(&self) -> Vec<Rule> {
        self.rules.clone()
    }

    pub fn add_list(&mut self, content: &str, category: Category) -> usize {
        let start = self.rules.len();
        for line in content.lines() {
            let line = line.trim();
            if line.is_empty() || line.starts_with('!') || line.starts_with('[') {
                continue;
            }
            // skip cosmetic rules here (handled by cosmetic module)
            if line.contains("##") || line.contains("#@#") || line.contains("#?#") {
                continue;
            }
            if let Some(rule) = parse_rule(line, category) {
                self.push_rule(rule);
            }
        }
        self.rules.len() - start
    }

    fn push_rule(&mut self, rule: Rule) {
        let idx = self.rules.len() as u32;
        if rule.exception {
            self.exceptions.push(idx);
        } else {
            let tokens = pattern_tokens(&rule.pattern);
            if tokens.is_empty() {
                self.unindexed.push(idx);
            } else {
                // index on the rarest-looking token: we simply index all tokens
                for t in tokens {
                    self.index.entry(t).or_default().push(idx);
                }
            }
        }
        self.rules.push(rule);
    }

    pub fn clear_allowlist(&mut self) {
        self.allowlist.clear();
    }

    pub fn set_allowlist(&mut self, host: String, allowed: bool) {
        if allowed {
            self.allowlist.insert(host);
        } else {
            self.allowlist.remove(&host);
        }
    }

    pub fn is_allowlisted(&self, host: &str) -> bool {
        // exact or parent-domain match
        let mut h = host;
        loop {
            if self.allowlist.contains(h) {
                return true;
            }
            match h.find('.') {
                Some(p) => h = &h[p + 1..],
                None => return false,
            }
        }
    }

    /// `url` — request URL, `first_party` — page URL, `resource_type` — CT_ bit,
    /// `page_host` passed separately to avoid re-parsing.
    pub fn check(&mut self, url: &str, first_party: &str, resource_type: u32) -> Decision {
        let req_host = host_of(url);
        let page_host = host_of(first_party);
        let req_reg = registrable(req_host, &self.multi_tlds);
        let page_reg = registrable(page_host, &self.multi_tlds);
        // Unknown/empty first-party (proxy layer, top-level navigation) is
        // treated as third-party: privacy-first default.
        let third_party = !req_reg.is_empty() && (page_reg.is_empty() || req_reg != page_reg);

        // site allowlist: block nothing on allowlisted pages
        if self.is_allowlisted(page_host) {
            return Decision::Allow;
        }

        // collect candidates from token index
        let mut candidates: Vec<u32> = Vec::new();
        for t in url_tokens(url) {
            if let Some(v) = self.index.get(&t) {
                candidates.extend_from_slice(v);
            }
        }
        candidates.extend_from_slice(&self.unindexed);
        // dedup + sort for deterministic order
        candidates.sort_unstable();
        candidates.dedup();

        let mut blocked: Option<Category> = None;
        for &i in &candidates {
            let r = &self.rules[i as usize];
            if !r.matches_types(resource_type) {
                continue;
            }
            match r.third_party {
                Some(true) if !third_party => continue,
                Some(false) if third_party => continue,
                _ => {}
            }
            if !r.domains.is_empty()
                && !domains_match(&r.domains, page_host, &page_reg)
            {
                continue;
            }
            if pattern_matches(&r.pattern, url, req_host) {
                if r.exception {
                    return Decision::Allow;
                }
                blocked = Some(r.category);
                break;
            }
        }
        // exceptions pass (slower path, only when blocked)
        if blocked.is_some() {
            for &i in &self.exceptions {
                let r = &self.rules[i as usize];
                if !r.domains.is_empty() && !domains_match(&r.domains, page_host, &page_reg) {
                    continue;
                }
                match r.third_party {
                    Some(true) if !third_party => continue,
                    _ => {}
                }
                if pattern_matches(&r.pattern, url, req_host) {
                    return Decision::Allow;
                }
            }
        }
        match blocked {
            Some(c) => {
                match c {
                    Category::Ad => self.stats_ads += 1,
                    Category::Tracker => self.stats_trackers += 1,
                    Category::Custom => {}
                }
                Decision::Block(c)
            }
            None => Decision::Allow,
        }
    }
}

fn domains_match(domains: &[(String, bool)], page_host: &str, page_reg: &str) -> bool {
    let mut any_pos = false;
    for (d, positive) in domains {
        let d = d.trim_start_matches('~');
        let hit = page_host == d
            || page_host.ends_with(&format!(".{}", d))
            || page_reg == d;
        if hit {
            if *positive {
                any_pos = true;
            } else {
                return false; // negated domain excludes this rule
            }
        }
    }
    any_pos
}

pub fn host_of(url: &str) -> &str {
    let after_scheme = match url.find("://") {
        Some(p) => &url[p + 3..],
        None => url,
    };
    let end = after_scheme
        .find(['/', '?', '#', '\\'])
        .unwrap_or(after_scheme.len());
    let hostport = &after_scheme[..end];
    // strip userinfo
    let hostport = match hostport.rfind('@') {
        Some(p) => &hostport[p + 1..],
        None => hostport,
    };
    // strip port
    let h = hostport;
    if let Some(colon) = h.rfind(':') {
        if !h[colon + 1..].is_empty() && h[colon + 1..].chars().all(|c| c.is_ascii_digit()) {
            return &h[..colon];
        }
    }
    h
}

pub fn registrable(host: &str, multi: &HashSet<&'static str>) -> String {
    if host.is_empty() {
        return String::new();
    }
    let labels: Vec<&str> = host.split('.').collect();
    if labels.len() >= 3 {
        let last_two = format!("{}.{}", labels[labels.len() - 2], labels[labels.len() - 1]);
        if multi.contains(last_two.as_str()) {
            // registrable = last three labels
            let start = labels.len() - 3;
            return labels[start..].join(".");
        }
    }
    if labels.len() >= 2 {
        labels[labels.len() - 2..].join(".")
    } else {
        host.to_string()
    }
}

fn is_separator(c: u8) -> bool {
    !c.is_ascii_alphanumeric() && c != b'.' && c != b'-' && c != b'%'
}

fn pattern_tokens(p: &Pattern) -> Vec<String> {
    let s = match p {
        Pattern::Plain(s) => s.clone(),
        Pattern::HostAnchored { host, rest } => {
            let mut t = host.clone();
            if let Some(r) = rest {
                t.push_str(r);
            }
            t
        }
        Pattern::StartAnchored(s) => s.clone(),
        Pattern::EndAnchored(s) => s.clone(),
        Pattern::Glob(_) => return Vec::new(),
    };
    url_tokens(&s)
}

fn url_tokens(s: &str) -> Vec<String> {
    let b = s.as_bytes();
    let mut out = Vec::new();
    let mut i = 0;
    while i < b.len() {
        if b[i].is_ascii_alphanumeric() {
            let start = i;
            while i < b.len() && b[i].is_ascii_alphanumeric() {
                i += 1;
            }
            if i - start >= 3 {
                out.push(s[start..i].to_ascii_lowercase());
            }
        } else {
            i += 1;
        }
    }
    out
}

fn pattern_matches(p: &Pattern, url: &str, req_host: &str) -> bool {
    match p {
        Pattern::Plain(s) => {
            // case-insensitive contains
            url.to_ascii_lowercase().contains(&s.to_ascii_lowercase())
        }
        Pattern::HostAnchored { host, rest } => {
            // host must equal the request host (or a parent domain); ^ / rest follows
            let h = host; // parsed lowercased
            let u = url.to_ascii_lowercase();
            let auth_start = match u.find("://") {
                Some(p) => p + 3,
                None => 0,
            };
            let auth_len = u[auth_start..]
                .find(['/', '?', '#'])
                .unwrap_or(u.len() - auth_start);
            let authority = &u[auth_start..auth_start + auth_len];
            let user_end = authority.rfind('@').map(|p| p + 1).unwrap_or(0);
            let mut host_seg = &authority[user_end..];
            if let Some(p) = host_seg.rfind(':') {
                if !host_seg[p + 1..].is_empty()
                    && host_seg[p + 1..].bytes().all(|b| b.is_ascii_digit())
                {
                    host_seg = &host_seg[..p];
                }
            }
            let host_ok = host_seg == h || host_seg.ends_with(&format!(".{}", h));
            if !host_ok {
                return false;
            }
            let after = &u[auth_start + user_end + host_seg.len()..];
            match rest {
                None => true,
                Some(r) => {
                    if r.is_empty() {
                        return true;
                    }
                    match r.strip_prefix('^') {
                        Some(tail) => {
                            if after.is_empty() {
                                true
                            } else if is_separator(after.as_bytes()[0]) {
                                if tail.is_empty() {
                                    true
                                } else {
                                    after[1..].starts_with(tail)
                                }
                            } else {
                                false
                            }
                        }
                        None => after.starts_with(r),
                    }
                }
            }
        }
        Pattern::StartAnchored(s) => {
            let u = url.to_ascii_lowercase();
            u.starts_with(&s.to_ascii_lowercase())
        }
        Pattern::EndAnchored(s) => {
            let u = url.to_ascii_lowercase();
            u.ends_with(&s.to_ascii_lowercase())
        }
        Pattern::Glob(parts) => glob_match(parts, url),
    }
}

fn glob_match(parts: &[GlobPart], url: &str) -> bool {
    let u = url.as_bytes();
    let mut i = 0usize;
    let mut lit_iter = parts.iter().peekable();
    while let Some(part) = lit_iter.next() {
        match part {
            GlobPart::Literal(l) => {
                let lb = l.as_bytes();
                // find lb starting at i (wildcard semantics: literal after wildcard can appear anywhere >= i)
                if i > u.len() {
                    return false;
                }
                let hay = &u[i..];
                let found = find_sub(hay, lb);
                match found {
                    Some(p) => i = i + p + lb.len(),
                    None => return false,
                }
            }
            GlobPart::Wildcard => {
                // if wildcard is last, everything matches
                if lit_iter.peek().is_none() {
                    return true;
                }
                // else: next literal search starts from current i (already handled)
            }
        }
    }
    i == u.len() || parts.last() == Some(&GlobPart::Wildcard)
}

fn find_sub(hay: &[u8], needle: &[u8]) -> Option<usize> {
    if needle.is_empty() {
        return Some(0);
    }
    if needle.len() > hay.len() {
        return None;
    }
    for i in 0..=hay.len() - needle.len() {
        if &hay[i..i + needle.len()] == needle {
            return Some(i);
        }
    }
    None
}

pub fn parse_rule(line: &str, category: Category) -> Option<Rule> {
    let mut rule = line.trim();
    let exception = rule.starts_with("@@");
    if exception {
        rule = &rule[2..];
    }

    // split options
    let mut pattern = rule;
    let mut types = 0u32;
    let mut third_party: Option<bool> = None;
    let mut domains: Vec<(String, bool)> = Vec::new();

    // find unescaped $
    if let Some(dollar) = find_unescaped_dollar(rule) {
        pattern = &rule[..dollar];
        let opts = &rule[dollar + 1..];
        for opt in opts.split(',') {
            let opt = opt.trim();
            if opt.is_empty() {
                continue;
            }
            match opt {
                "script" => types |= CT_SCRIPT,
                "image" => types |= CT_IMAGE,
                "stylesheet" => types |= CT_STYLESHEET,
                "object" | "object-subrequest" => types |= CT_OBJECT,
                "xhr" | "xmlhttprequest" => types |= CT_XHR,
                "subdocument" => types |= CT_SUBDOCUMENT,
                "ping" | "beacon" => types |= CT_PING,
                "media" => types |= CT_MEDIA,
                "font" => types |= CT_FONT,
                "websocket" => types |= CT_WEBSOCKET,
                "other" => types |= CT_OTHER,
                "document" => types |= CT_MAINFRAME,
                "third-party" => third_party = Some(true),
                "1p" | "first-party" => third_party = Some(false),
                "3p" => third_party = Some(true),
                _ => {
                    if let Some(v) = opt.strip_prefix("domain=") {
                        for d in v.split('|') {
                            let d = d.trim();
                            if d.is_empty() {
                                continue;
                            }
                            if let Some(neg) = d.strip_prefix('~') {
                                domains.push((neg.to_ascii_lowercase(), false));
                            } else {
                                domains.push((d.to_ascii_lowercase(), true));
                            }
                        }
                    }
                    // unknown option → reject rule conservatively
                    else if !opt.starts_with('~')
                        && !matches!(
                            opt,
                            "collapse"
                                | "donotcollapse"
                                | "genericblock"
                                | "generichide"
                                | "important"
                                | "badfilter"
                                | "match-case"
                                | "cname"
                        )
                    {
                        return None;
                    }
                }
            }
        }
    }

    if pattern.is_empty() {
        return None;
    }

    let parsed = if let Some(rest) = pattern.strip_prefix("||") {
        // split host at first ^ or /
        let (host, rest_part) = match rest.find(['^', '/']) {
            Some(p) => (&rest[..p], Some(rest[p..].to_string())),
            None => (rest, None),
        };
        let host = host.trim().to_ascii_lowercase();
        if host.is_empty() {
            return None;
        }
        Pattern::HostAnchored {
            host,
            rest: rest_part,
        }
    } else if let Some(rest) = pattern.strip_prefix('|') {
        Pattern::StartAnchored(rest.to_ascii_lowercase())
    } else if pattern.ends_with('|') && !pattern.ends_with("||") {
        Pattern::EndAnchored(pattern[..pattern.len() - 1].to_ascii_lowercase())
    } else if pattern.contains('*') {
        let mut parts: Vec<GlobPart> = Vec::new();
        let mut cur = String::new();
        for c in pattern.chars() {
            if c == '*' {
                if !cur.is_empty() {
                    parts.push(GlobPart::Literal(cur.to_ascii_lowercase()));
                    cur.clear();
                }
                if parts.last() != Some(&GlobPart::Wildcard) {
                    parts.push(GlobPart::Wildcard);
                }
            } else {
                cur.push(c);
            }
        }
        if !cur.is_empty() {
            parts.push(GlobPart::Literal(cur.to_ascii_lowercase()));
        }
        Pattern::Glob(parts)
    } else {
        Pattern::Plain(pattern.to_ascii_lowercase())
    };

    Some(Rule {
        pattern: parsed,
        exception,
        types,
        third_party,
        domains,
        category,
    })
}

fn find_unescaped_dollar(s: &str) -> Option<usize> {
    let b = s.as_bytes();
    for (i, &c) in b.iter().enumerate() {
        if c == b'$' && (i == 0 || b[i - 1] != b'\\') {
            // heuristic: $ followed by option-looking content
            return Some(i);
        }
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_host_anchored() {
        let mut e = Engine::new();
        e.add_list("||doubleclick.net^\n||googlesyndication.com^", Category::Ad);
        match e.check(
            "https://ad.doubleclick.net/ddm/adj/x",
            "https://example.com/page.html",
            CT_SCRIPT,
        ) {
            Decision::Block(_) => {}
            Decision::Allow => panic!("should block"),
        }
        match e.check(
            "https://example.com/doubleclick.netish",
            "https://example.com/page.html",
            CT_IMAGE,
        ) {
            Decision::Allow => {}
            _ => panic!("should allow"),
        }
    }

    #[test]
    fn test_exception() {
        let mut e = Engine::new();
        e.add_list("||ads.example.com^\n@@||ads.example.com/whitelist", Category::Ad);
        match e.check(
            "https://ads.example.com/whitelist",
            "https://example.com/",
            CT_IMAGE,
        ) {
            Decision::Allow => {}
            _ => panic!("should be excepted"),
        }
    }

    #[test]
    fn test_third_party_option() {
        let mut e = Engine::new();
        e.add_list("||tracker.io^$third-party,script", Category::Tracker);
        match e.check(
            "https://tracker.io/t.js",
            "https://tracker.io/page",
            CT_SCRIPT,
        ) {
            Decision::Allow => {}
            _ => panic!("first-party must be allowed"),
        }
    }
}
