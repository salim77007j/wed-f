//! Cosmetic filtering — `domain##selector` and generic `##selector` rules,
//! `#@#` exceptions. Returns per-site CSS as JSON for the C++ side to inject.

use std::collections::HashMap;

pub struct CosmeticEngine {
    /// host -> specific selectors (suffix matching on host parts)
    specific: HashMap<String, Vec<String>>,
    /// host -> exception selectors
    exceptions: HashMap<String, Vec<String>>,
    /// generic selectors applied everywhere
    generic: Vec<String>,
    /// generic exceptions (rules like `example.com#@#.ad` that kill generics)
    generic_exceptions: Vec<(String, String)>, // (host, selector)
}

impl CosmeticEngine {
    pub fn new() -> CosmeticEngine {
        CosmeticEngine {
            specific: HashMap::new(),
            exceptions: HashMap::new(),
            generic: Vec::new(),
            generic_exceptions: Vec::new(),
        }
    }

    pub fn clear(&mut self) {
        self.specific.clear();
        self.exceptions.clear();
        self.generic.clear();
        self.generic_exceptions.clear();
    }

    pub fn selector_count(&self) -> usize {
        self.generic.len()
            + self.specific.values().map(|v| v.len()).sum::<usize>()
    }

    pub fn add_list(&mut self, content: &str) -> usize {
        let mut added = 0;
        for line in content.lines() {
            let line = line.trim();
            if line.is_empty() || line.starts_with('!') || line.starts_with('[') {
                continue;
            }
            // skip procedural cosmetic rules (#?#)
            if line.contains("#?#") {
                continue;
            }
            let marker = if let Some(p) = line.find("#@#") {
                Some((p, 3usize, true))
            } else if let Some(p) = line.find("##") {
                Some((p, 2usize, false))
            } else {
                None
            };
            let Some((pos, marker_len, is_exception)) = marker else { continue };
            let domains_part = &line[..pos];
            let sel = &line[pos + marker_len..];
            let sel = sel.trim().to_string();
            if sel.is_empty() || sel.len() > 512 {
                continue;
            }
            if domains_part.is_empty() {
                if is_exception {
                    continue; // bare #@# meaningless
                }
                self.generic.push(sel);
                added += 1;
            } else {
                for d in domains_part.split(',') {
                    let d = d.trim().to_ascii_lowercase();
                    if d.is_empty() {
                        continue;
                    }
                    if let Some(host) = d.strip_prefix('~') {
                        // ~domain##sel: exception applies on that host
                        let host = host.to_string();
                        self.exceptions
                            .entry(host.clone())
                            .or_default()
                            .push(sel.clone());
                        self.generic_exceptions.push((host, sel.clone()));
                    } else if is_exception {
                        self.exceptions
                            .entry(d.clone())
                            .or_default()
                            .push(sel.clone());
                        self.generic_exceptions.push((d, sel.clone()));
                    } else {
                        self.specific.entry(d).or_default().push(sel.clone());
                        added += 1;
                    }
                }
            }
        }
        added
    }

    /// Build the style source for a page host.
    /// JSON: {"generic": [..], "specific": [..]}
    pub fn stylesheet_json(&self, host: &str) -> String {
        let host = host.to_ascii_lowercase();
        let mut spec: Vec<String> = Vec::new();
        let mut exempt: std::collections::HashSet<String> = std::collections::HashSet::new();

        // walk host suffixes: example.com, sub.example.com matches example.com rules
        let mut parts: Vec<&str> = host.split('.').collect();
        while parts.len() >= 2 {
            let candidate = parts.join(".");
            if let Some(v) = self.specific.get(&candidate) {
                spec.extend(v.iter().cloned());
            }
            if let Some(v) = self.exceptions.get(&candidate) {
                exempt.extend(v.iter().cloned());
            }
            parts.remove(0);
        }

        let mut out = String::with_capacity(1024);
        out.push_str("{\"generic\":[");
        for (i, s) in self.generic.iter().enumerate() {
            if i > 0 {
                out.push(',');
            }
            push_json_str(&mut out, s);
        }
        out.push_str("],\"specific\":[");
        for (i, s) in spec.iter().enumerate() {
            if i > 0 {
                out.push(',');
            }
            push_json_str(&mut out, s);
        }
        out.push_str("]}");
        out
    }

    /// Selectors count for a host (for popup display)
    pub fn count_for_host(&self, host: &str) -> (usize, usize) {
        let host = host.to_ascii_lowercase();
        let mut spec = 0;
        let mut parts: Vec<&str> = host.split('.').collect();
        while parts.len() >= 2 {
            let candidate = parts.join(".");
            if let Some(v) = self.specific.get(&candidate) {
                spec += v.len();
            }
            parts.remove(0);
        }
        (self.generic.len(), spec)
    }
}

fn push_json_str(out: &mut String, s: &str) {
    out.push('"');
    for c in s.chars() {
        match c {
            '"' => out.push_str("\\\""),
            '\\' => out.push_str("\\\\"),
            '\n' => out.push_str("\\n"),
            '\r' => {}
            c if (c as u32) < 0x20 => {}
            c => out.push(c),
        }
    }
    out.push('"');
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parse_and_query() {
        let mut c = CosmeticEngine::new();
        c.add_list("##.ad-banner\nexample.com##.sponsored\nexample.com#@#.ad-banner");
        let json = c.stylesheet_json("www.example.com");
        assert!(json.contains(".sponsored"), "{}", json);
        assert!(json.contains(".ad-banner"), "{}", json);
    }
}
