//! WED Browser product core — C ABI surface for native shells (GTK/Win32).
//!
//! Design: the engine (WebKitGTK / WebView2) is a swappable component; all
//! product value lives here: ad/tracker filtering (two layers: engine
//! content-blocker rules + host-level connect filtering through the local
//! Rust proxy), history/bookmarks/downloads/sessions (SQLite), per-site
//! privacy policy, and the tab efficiency governor.
//!
//! All strings returned to C are heap-allocated; free with `wed_free_string`.

#![allow(clippy::missing_safety_doc)]

pub mod contentblocker;
pub mod cosmetic;
pub mod db;
pub mod filters;
pub mod proxy;
pub mod settings;

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int, c_ulong};
use std::sync::atomic::{AtomicU64, AtomicUsize, Ordering};
use std::sync::{Mutex, OnceLock};

use db::Database;
use filters::{Category, Decision, Engine};
use cosmetic::CosmeticEngine;

pub const WED_ALLOW: c_int = 0;
pub const WED_BLOCK_AD: c_int = 1;
pub const WED_BLOCK_TRACKER: c_int = 2;

pub struct Core {
    pub engine: Mutex<Engine>,
    pub cosmetic: Mutex<CosmeticEngine>,
    pub db: Mutex<Database>,
    pub proxy_port: AtomicUsize,
    pub blocked_ads: AtomicU64,
    pub blocked_trackers: AtomicU64,
    pub saved_bytes: AtomicU64,
    pub pages_loaded: AtomicU64,
}

static CORE: OnceLock<Core> = OnceLock::new();

impl Core {
    fn global() -> &'static Core {
        CORE.get_or_init(|| Core {
            engine: Mutex::new(Engine::new()),
            cosmetic: Mutex::new(CosmeticEngine::new()),
            db: Mutex::new(Database::open_default()),
            proxy_port: AtomicUsize::new(0),
            blocked_ads: AtomicU64::new(0),
            blocked_trackers: AtomicU64::new(0),
            saved_bytes: AtomicU64::new(0),
            pages_loaded: AtomicU64::new(0),
        })
    }
}

/// Bump a persistent global counter (DB-backed, write-through).
/// Chrome-parity: lifetime totals survive restarts.
fn gstat_bump(key: &str, inc: u64) {
    Core::global().db.lock().unwrap().gstat_bump(key, inc);
}

fn gstat_get(key: &str) -> u64 {
    Core::global().db.lock().unwrap().gstat_get(key)
}

/// Initialise the core with a data directory (created if missing).
/// Returns 0 on success. Idempotent (subsequent calls re-open the DB path).
#[no_mangle]
pub extern "C" fn wed_init(data_dir: *const c_char) -> c_int {
    let dir = unsafe {
        if data_dir.is_null() {
            String::new()
        } else {
            CStr::from_ptr(data_dir).to_string_lossy().into_owned()
        }
    };
    let core = Core::global();
    let res = core.db.lock().unwrap().open(&dir);
    match res {
        Ok(()) => 0,
        Err(e) => {
            eprintln!("wed_init: db open failed: {e}");
            -1
        }
    }
}

#[no_mangle]
pub extern "C" fn wed_free_string(s: *mut c_char) {
    if !s.is_null() {
        unsafe { drop(CString::from_raw(s)) };
    }
}

/// Parse filter lists (uBlock/EasyList syntax). `text` is NUL-terminated
/// list content. Returns the number of accepted network rules.
#[no_mangle]
pub extern "C" fn wed_load_filters(text: *const c_char, is_privacy_list: c_int) -> c_int {
    let text = unsafe { CStr::from_ptr(text).to_string_lossy() };
    let cat = if is_privacy_list != 0 { Category::Tracker } else { Category::Ad };
    let core = Core::global();
    let mut engine = core.engine.lock().unwrap();
    let n = engine.add_list(&text, cat);
    drop(engine);
    let mut cosmetic = core.cosmetic.lock().unwrap();
    let c = cosmetic.add_list(&text);
    n as c_int + c as c_int
}

/// Host-level match decision for a request URL (used by the proxy layer).
/// Returns WED_ALLOW / WED_BLOCK_AD / WED_BLOCK_TRACKER.
#[no_mangle]
pub extern "C" fn wed_decide(url: *const c_char) -> c_int {
    let url = unsafe { CStr::from_ptr(url).to_string_lossy() };
    let core = Core::global();
    let mut engine = core.engine.lock().unwrap();
    match engine.check(&url, "", 0) {
        Decision::Allow => WED_ALLOW,
        Decision::Block(Category::Ad | Category::Custom) => WED_BLOCK_AD,
        Decision::Block(Category::Tracker) => WED_BLOCK_TRACKER,
    }
}

/// Whether ad/tracker blocking is enabled for `host` (per-site exceptions).
#[no_mangle]
pub extern "C" fn wed_shields_enabled_for(host: *const c_char) -> c_int {
    let host = unsafe { CStr::from_ptr(host).to_string_lossy() };
    let core = Core::global();
    core.db.lock().unwrap().shields_enabled(&host) as c_int
}

#[no_mangle]
pub extern "C" fn wed_set_shields(host: *const c_char, enabled: c_int) {
    let host = unsafe { CStr::from_ptr(host).to_string_lossy() };
    let core = Core::global();
    core.db.lock().unwrap().set_shields(&host, enabled != 0);
}

/// Cosmetic selectors for `host` as JSON {"generic":[..],"specific":[..]}
/// or NULL. The shell injects these as a page stylesheet.
#[no_mangle]
pub extern "C" fn wed_cosmetic_json(host: *const c_char) -> *mut c_char {
    let host = unsafe { CStr::from_ptr(host).to_string_lossy() };
    let core = Core::global();
    let cosmetic = core.cosmetic.lock().unwrap();
    let json = cosmetic.stylesheet_json(&host);
    if json == "{\"generic\":[],\"specific\":[]}" {
        return std::ptr::null_mut();
    }
    match CString::new(json) {
        Ok(c) => c.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

/// Start the local filtering proxy on an OS-chosen port.
/// Returns the port, or 0 on failure. HTTPS passes through untouched
/// (CONNECT tunneling, no MITM) — blocking is host-level only.
#[no_mangle]
pub extern "C" fn wed_proxy_start() -> c_int {
    let core = Core::global();
    match proxy::start() {
        Ok(port) => {
            core.proxy_port.store(port as usize, Ordering::SeqCst);
            port as c_int
        }
        Err(e) => {
            eprintln!("wed_proxy_start: {e}");
            0
        }
    }
}

#[no_mangle]
pub extern "C" fn wed_proxy_port() -> c_int {
    Core::global().proxy_port.load(Ordering::SeqCst) as c_int
}

#[no_mangle]
pub extern "C" fn wed_proxy_set_enabled(enabled: c_int) {
    proxy::set_enabled(enabled != 0);
}

// ---------------------------------------------------------------- statistics

#[no_mangle]
pub extern "C" fn wed_stats(blocked_ads: *mut c_ulong, blocked_trackers: *mut c_ulong,
                            pages: *mut c_ulong, saved_bytes: *mut c_ulong) {
    /* lifetime totals: DB value (previous sessions) + this session's atomics */
    unsafe {
        if !blocked_ads.is_null() {
            *blocked_ads = gstat_get("ads") + Core::global().blocked_ads.load(Ordering::Relaxed);
        }
        if !blocked_trackers.is_null() {
            *blocked_trackers = gstat_get("trackers") + Core::global().blocked_trackers.load(Ordering::Relaxed);
        }
        if !pages.is_null() {
            *pages = gstat_get("pages") + Core::global().pages_loaded.load(Ordering::Relaxed);
        }
        if !saved_bytes.is_null() {
            *saved_bytes = gstat_get("bytes") + Core::global().saved_bytes.load(Ordering::Relaxed);
        }
    }
}

#[no_mangle]
pub extern "C" fn wed_record_page_load() {
    Core::global().pages_loaded.fetch_add(1, Ordering::Relaxed);
    gstat_bump("pages", 1);
}

/// Per-site blocked counts for the dashboard: JSON array of
/// {"host":..,"ads":..,"trackers":..,"last":..}, newest first, up to 200.
#[no_mangle]
pub extern "C" fn wed_site_stats_json() -> *mut c_char {
    let json = Core::global().db.lock().unwrap().site_stats_json();
    match CString::new(json) {
        Ok(c) => c.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

// ---------------------------------------------------------------- content rules

/// Compile loaded rules into WebKit content-blocker JSON.
/// Caller frees with wed_free_string.
#[no_mangle]
pub extern "C" fn wed_content_rules_json() -> *mut c_char {
    let core = Core::global();
    let engine = core.engine.lock().unwrap();
    let json = contentblocker::compile_json(&engine);
    match CString::new(json) {
        Ok(c) => c.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

// ---------------------------------------------------------------- history

#[no_mangle]
pub extern "C" fn wed_history_add(url: *const c_char, title: *const c_char) {
    let url = unsafe { CStr::from_ptr(url).to_string_lossy() };
    let title = unsafe { CStr::from_ptr(title).to_string_lossy() };
    Core::global().db.lock().unwrap().history_add(&url, &title);
}

/// JSON: [{"url","title","ts","visits"}...] latest-first, up to `limit`.
#[no_mangle]
pub extern "C" fn wed_history_json(limit: c_int, query: *const c_char) -> *mut c_char {
    let query = unsafe {
        if query.is_null() { String::new() } else { CStr::from_ptr(query).to_string_lossy().into_owned() }
    };
    let json = Core::global().db.lock().unwrap().history_json(limit.max(1), &query);
    CString::new(json).map(|c| c.into_raw()).unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn wed_history_clear() {
    Core::global().db.lock().unwrap().history_clear();
}

#[no_mangle]
pub extern "C" fn wed_history_remove(url: *const c_char) {
    let url = unsafe { CStr::from_ptr(url).to_string_lossy() };
    Core::global().db.lock().unwrap().history_remove(&url);
}

// ---------------------------------------------------------------- bookmarks

#[no_mangle]
pub extern "C" fn wed_bookmark_add(url: *const c_char, title: *const c_char, folder: *const c_char) -> c_int {
    let url = unsafe { CStr::from_ptr(url).to_string_lossy() };
    let title = unsafe { CStr::from_ptr(title).to_string_lossy() };
    let folder = unsafe {
        if folder.is_null() { "root".into() } else { CStr::from_ptr(folder).to_string_lossy().into_owned() }
    };
    Core::global().db.lock().unwrap().bookmark_add(&url, &title, &folder) as c_int
}

#[no_mangle]
pub extern "C" fn wed_bookmark_remove(url: *const c_char) {
    let url = unsafe { CStr::from_ptr(url).to_string_lossy() };
    Core::global().db.lock().unwrap().bookmark_remove(&url);
}

#[no_mangle]
pub extern "C" fn wed_bookmarks_json(folder: *const c_char) -> *mut c_char {
    let folder = unsafe {
        if folder.is_null() { "root".into() } else { CStr::from_ptr(folder).to_string_lossy().into_owned() }
    };
    let json = Core::global().db.lock().unwrap().bookmarks_json(&folder);
    CString::new(json).map(|c| c.into_raw()).unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn wed_is_bookmarked(url: *const c_char) -> c_int {
    let url = unsafe { CStr::from_ptr(url).to_string_lossy() };
    Core::global().db.lock().unwrap().is_bookmarked(&url) as c_int
}

// ---------------------------------------------------------------- sessions

/// Save the current session: `tabs` is JSON [{"url","title","active"}...].
#[no_mangle]
pub extern "C" fn wed_session_save(tabs_json: *const c_char, window_state: *const c_char) {
    let tabs = unsafe { CStr::from_ptr(tabs_json).to_string_lossy() };
    let ws = unsafe {
        if window_state.is_null() { String::new() } else { CStr::from_ptr(window_state).to_string_lossy().into_owned() }
    };
    Core::global().db.lock().unwrap().session_save(&tabs, &ws);
}

/// Returns the saved tabs JSON or NULL if none.
#[no_mangle]
pub extern "C" fn wed_session_load() -> *mut c_char {
    let s = Core::global().db.lock().unwrap().session_load();
    match s {
        Some(s) => CString::new(s).map(|c| c.into_raw()).unwrap_or(std::ptr::null_mut()),
        None => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn wed_session_window_state() -> *mut c_char {
    let s = Core::global().db.lock().unwrap().session_window_state();
    match s {
        Some(s) => CString::new(s).map(|c| c.into_raw()).unwrap_or(std::ptr::null_mut()),
        None => std::ptr::null_mut(),
    }
}

// ---------------------------------------------------------------- settings

#[no_mangle]
pub extern "C" fn wed_settings_get(key: *const c_char, default: *const c_char) -> *mut c_char {
    let key = unsafe { CStr::from_ptr(key).to_string_lossy() };
    let def = unsafe {
        if default.is_null() { String::new() } else { CStr::from_ptr(default).to_string_lossy().into_owned() }
    };
    let v = Core::global().db.lock().unwrap().settings_get(&key, &def);
    CString::new(v).map(|c| c.into_raw()).unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn wed_settings_set(key: *const c_char, value: *const c_char) {
    let key = unsafe { CStr::from_ptr(key).to_string_lossy() };
    let value = unsafe { CStr::from_ptr(value).to_string_lossy() };
    Core::global().db.lock().unwrap().settings_set(&key, &value);
}

/// Settings schema (JSON array of {key,label,type,options,default,section})
/// describing every setting the UI offers. Single source of truth.
#[no_mangle]
pub extern "C" fn wed_settings_schema() -> *mut c_char {
    let s = settings::schema_json();
    CString::new(s).map(|c| c.into_raw()).unwrap_or(std::ptr::null_mut())
}

// ---------------------------------------------------------------- permissions

/// Record a per-site permission decision. `kind` is one of
/// "geolocation" | "notifications" | "camera" | "microphone" | "clipboard" |
/// "popup". `value` is "allow" | "deny" | "ask".
#[no_mangle]
pub extern "C" fn wed_permission_set(host: *const c_char, kind: *const c_char, value: *const c_char) {
    let host = unsafe { CStr::from_ptr(host).to_string_lossy() };
    let kind = unsafe { CStr::from_ptr(kind).to_string_lossy() };
    let value = unsafe { CStr::from_ptr(value).to_string_lossy() };
    Core::global().db.lock().unwrap().permission_set(&host, &kind, &value);
}

/// Returns "allow" | "deny" | "ask" (default "ask").
#[no_mangle]
pub extern "C" fn wed_permission_get(host: *const c_char, kind: *const c_char) -> *mut c_char {
    let host = unsafe { CStr::from_ptr(host).to_string_lossy() };
    let kind = unsafe { CStr::from_ptr(kind).to_string_lossy() };
    let v = Core::global().db.lock().unwrap().permission_get(&host, &kind);
    CString::new(v).map(|c| c.into_raw()).unwrap_or(std::ptr::null_mut())
}

/// All per-site permissions + shield exceptions as JSON (for the settings
/// "site settings" list).
#[no_mangle]
pub extern "C" fn wed_site_settings_json() -> *mut c_char {
    let s = Core::global().db.lock().unwrap().site_settings_json();
    CString::new(s).map(|c| c.into_raw()).unwrap_or(std::ptr::null_mut())
}

#[no_mangle]
pub extern "C" fn wed_site_reset(host: *const c_char) {
    let host = unsafe { CStr::from_ptr(host).to_string_lossy() };
    Core::global().db.lock().unwrap().site_reset(&host);
}

// ---------------------------------------------------------------- downloads

#[no_mangle]
pub extern "C" fn wed_download_record(url: *const c_char, path: *const c_char, size: c_ulong, ok: c_int) {
    let url = unsafe { CStr::from_ptr(url).to_string_lossy() };
    let path = unsafe { CStr::from_ptr(path).to_string_lossy() };
    Core::global().db.lock().unwrap().download_record(&url, &path, size as i64, ok != 0);
}

#[no_mangle]
pub extern "C" fn wed_downloads_json() -> *mut c_char {
    let s = Core::global().db.lock().unwrap().downloads_json();
    CString::new(s).map(|c| c.into_raw()).unwrap_or(std::ptr::null_mut())
}

// ---------------------------------------------------------------- top sites

/// Speed-dial entries: top 12 sites by visit frequency (JSON).
#[no_mangle]
pub extern "C" fn wed_top_sites_json() -> *mut c_char {
    let s = Core::global().db.lock().unwrap().top_sites_json();
    CString::new(s).map(|c| c.into_raw()).unwrap_or(std::ptr::null_mut())
}

// ---------------------------------------------------------------- governor

/// Efficiency governor: should a background tab be hibernated?
/// `idle_seconds` = seconds since last user interaction with the tab;
/// returns 1 if the tab should be unloaded (hibernated), else 0.
#[no_mangle]
pub extern "C" fn wed_governor_should_hibernate(idle_seconds: c_int, is_playing_media: c_int) -> c_int {
    let enabled = {
        let core = Core::global();
        let db = core.db.lock().unwrap();
        db.settings_get("hibernate.enabled", "true") == "true"
    };
    if !enabled || is_playing_media != 0 {
        return 0;
    }
    let threshold: i64 = Core::global()
        .db.lock().unwrap()
        .settings_get("hibernate.idle.minutes", "10")
        .parse().unwrap_or(10);
    (idle_seconds as i64 >= threshold * 60) as c_int
}
