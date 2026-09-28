//! WED Browser privacy core — C ABI surface for the Qt/C++ application.
//! All string memory returned to C++ must be freed with `wed_free_string`.
//! The engine is thread-safe (interceptor callbacks arrive on Chromium IO threads).

#![allow(clippy::missing_safety_doc)]

mod cosmetic;
mod filters;

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Mutex;

use cosmetic::CosmeticEngine;
use filters::{Category, Decision, Engine};

pub const WED_ALLOW: c_int = 0;
pub const WED_BLOCK_AD: c_int = 1;
pub const WED_BLOCK_TRACKER: c_int = 2;
pub const WED_BLOCK_CUSTOM: c_int = 3;

struct Inner {
    engine: Engine,
    cosmetic: CosmeticEngine,
}

pub struct Core {
    inner: Mutex<Inner>,
    ads: AtomicU64,
    trackers: AtomicU64,
    cosmetic_hits: AtomicU64,
}

impl Core {
    fn new() -> Core {
        Core {
            inner: Mutex::new(Inner {
                engine: Engine::new(),
                cosmetic: CosmeticEngine::new(),
            }),
            ads: AtomicU64::new(0),
            trackers: AtomicU64::new(0),
            cosmetic_hits: AtomicU64::new(0),
        }
    }
}

#[no_mangle]
pub extern "C" fn wed_core_create() -> *mut Core {
    Box::into_raw(Box::new(Core::new()))
}

#[no_mangle]
pub extern "C" fn wed_core_destroy(core: *mut Core) {
    if core.is_null() {
        return;
    }
    unsafe { drop(Box::from_raw(core)) };
}

/// Load a filter list from a file path. `category`: 0=ad, 1=tracker, 2=custom.
/// Returns number of network rules loaded (cosmetic parsed too). -1 on I/O error.
#[no_mangle]
pub extern "C" fn wed_core_load_list(core: *mut Core, path: *const c_char, category: c_int) -> c_int {
    let core = unsafe { &*core };
    let path = unsafe { CStr::from_ptr(path) }.to_string_lossy().to_string();
    let content = match std::fs::read_to_string(&path) {
        Ok(c) => c,
        Err(_) => return -1,
    };
    let cat = match category {
        0 => Category::Ad,
        1 => Category::Tracker,
        _ => Category::Custom,
    };
    let mut inner = core.inner.lock().unwrap();
    inner.cosmetic.add_list(&content);
    inner.engine.add_list(&content, cat) as c_int
}

/// Replace all allowlist entries with the given hosts (newline separated).
#[no_mangle]
pub extern "C" fn wed_core_set_allowlist(
    core: *mut Core,
    hosts: *const c_char,
) {
    let core = unsafe { &*core };
    let hosts = unsafe { CStr::from_ptr(hosts) }.to_string_lossy().to_string();
    let mut inner = core.inner.lock().unwrap();
    inner.engine.clear_allowlist();
    for h in hosts.lines() {
        let h = h.trim().to_ascii_lowercase();
        if !h.is_empty() {
            inner.engine.set_allowlist(h, true);
        }
    }
}

#[no_mangle]
pub extern "C" fn wed_core_allow_site(core: *mut Core, host: *const c_char, allowed: c_int) {
    let core = unsafe { &*core };
    let host = unsafe { CStr::from_ptr(host) }
        .to_string_lossy()
        .trim()
        .to_ascii_lowercase();
    let mut inner = core.inner.lock().unwrap();
    inner.engine.set_allowlist(host, allowed != 0);
}

#[no_mangle]
pub extern "C" fn wed_core_is_allowlisted(core: *mut Core, host: *const c_char) -> c_int {
    let core = unsafe { &*core };
    let host = unsafe { CStr::from_ptr(host) }.to_string_lossy().to_string();
    let inner = core.inner.lock().unwrap();
    inner.engine.is_allowlisted(&host) as c_int
}

/// Main request check. `resource_type` uses CT_* bit values from the C++ side.
/// Returns WED_* decision. Thread-safe, updates internal counters.
#[no_mangle]
pub extern "C" fn wed_core_check(
    core: *mut Core,
    url: *const c_char,
    first_party: *const c_char,
    resource_type: c_int,
) -> c_int {
    let core = unsafe { &*core };
    let url = unsafe { CStr::from_ptr(url) }.to_string_lossy().to_string();
    let first_party = unsafe { CStr::from_ptr(first_party) }.to_string_lossy().to_string();
    let mut inner = core.inner.lock().unwrap();
    match inner.engine.check(&url, &first_party, resource_type as u32) {
        Decision::Allow => WED_ALLOW,
        Decision::Block(c) => match c {
            Category::Ad => {
                core.ads.fetch_add(1, Ordering::Relaxed);
                WED_BLOCK_AD
            }
            Category::Tracker => {
                core.trackers.fetch_add(1, Ordering::Relaxed);
                WED_BLOCK_TRACKER
            }
            Category::Custom => {
                core.trackers.fetch_add(1, Ordering::Relaxed);
                WED_BLOCK_CUSTOM
            }
        },
    }
}

/// Cosmetic stylesheet for a host, JSON: {"generic":[...],"specific":[...]}.
/// Caller must `wed_free_string` the result. Returns null on failure.
#[no_mangle]
pub extern "C" fn wed_core_cosmetic(core: *mut Core, host: *const c_char) -> *mut c_char {
    let core = unsafe { &*core };
    let host = unsafe { CStr::from_ptr(host) }.to_string_lossy().to_string();
    let inner = core.inner.lock().unwrap();
    let json = inner.cosmetic.stylesheet_json(&host);
    match CString::new(json) {
        Ok(c) => c.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

/// Number of cosmetic selectors available for host (generic, specific).
#[no_mangle]
pub extern "C" fn wed_core_cosmetic_count(
    core: *mut Core,
    host: *const c_char,
    generic_out: *mut i64,
    specific_out: *mut i64,
) {
    let core = unsafe { &*core };
    let host = unsafe { CStr::from_ptr(host) }.to_string_lossy().to_string();
    let inner = core.inner.lock().unwrap();
    let (g, s) = inner.cosmetic.count_for_host(&host);
    unsafe {
        if !generic_out.is_null() {
            *generic_out = g as i64;
        }
        if !specific_out.is_null() {
            *specific_out = s as i64;
        }
    }
}

#[no_mangle]
pub extern "C" fn wed_core_free_string(s: *mut c_char) {
    if s.is_null() {
        return;
    }
    unsafe { drop(CString::from_raw(s)) };
}

/// Lifetime totals since process start. Pointers may be null.
#[no_mangle]
pub extern "C" fn wed_core_stats(
    core: *mut Core,
    ads: *mut u64,
    trackers: *mut u64,
) {
    let core = unsafe { &*core };
    unsafe {
        if !ads.is_null() {
            *ads = core.ads.load(Ordering::Relaxed);
        }
        if !trackers.is_null() {
            *trackers = core.trackers.load(Ordering::Relaxed);
        }
    }
}

#[no_mangle]
pub extern "C" fn wed_core_rule_count(core: *mut Core) -> i64 {
    let core = unsafe { &*core };
    let inner = core.inner.lock().unwrap();
    inner.engine.rule_count() as i64
}

/// Exposed for unit tests via cargo test
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn full_pipeline() {
        let mut core = Core::new();
        core.inner.get_mut().unwrap().engine.add_list(
            "||doubleclick.net^\n||google-analytics.com^$script",
            Category::Tracker,
        );
        let d = core.inner.get_mut().unwrap().engine.check(
            "https://stats.google-analytics.com/collect?x=1",
            "https://example.com/",
            filters::CT_SCRIPT,
        );
        assert!(matches!(d, Decision::Block(_)));
    }
}
