# WED Browser

**A fast, private, native web browser for Windows and Linux.**

WED is a real, production-shaped browser — not a wrapper demo. It embeds the
Chromium engine (through Qt WebEngine) for full modern-site compatibility, adds
a **privacy core written in Rust**, and drives a **100% native Qt Widgets UI**
(no Electron, no HTML/JS chrome, no web-tech shell).

```
┌───────────────────────────────────────────────┐
│  Native UI  (C++ / Qt 6 Widgets + QSS theming)│  tabs, address bar, menus,
│  every control bound to real behavior         │  panels, dialogs, settings
├───────────────────────────────────────────────┤
│  Chromium (Qt WebEngine 6.7)                  │  rendering, JS, networking
├───────────────────────────────────────────────┤
│  Rust core — wed_core (static C ABI)          │  ad/tracker network filter
│  thread-safe, memory-safe, zero-copy FFI      │  engine + cosmetic engine
└───────────────────────────────────────────────┘
```

## Highlights

**Privacy & security**
- Network-level ad + tracker blocking via the Rust filter engine
  (EasyList + EasyPrivacy shipped; allowlists and per-site shield exceptions)
- Cosmetic filtering — domain-specific hidden-element selectors injected per page
- Fingerprint protection: canvas / audio / WebGL / screen / hardware / plugin
  spoofing with per-site-session deterministic randomization
- Third-party cookie control, DNT + GPC headers, HTTPS-first upgrades
- WebRTC IP leak prevention, per-site permission brokering, certificate
  interstitials, private windows with isolated off-the-record profiles
- Crash-safe session persistence and one-click crash recovery

**Performance**
- Inactive-tab hibernation (freeze renderer work after N minutes)
- Lazy filter engine: decisions in microseconds on Chromium IO threads
- Session writes debounced and atomic (QSaveFile), SQLite on a single connection

**Full browser feature set** — multi-tab management (pin/mute/detach/drag/reopen),
smart address bar (history + bookmarks + search suggestions), bookmarks manager
with import/export, history panel, download manager (pause/resume/cancel),
deep settings (6 pages, live-applied), privacy dashboard, start page with speed
dial, in-page find, per-site zoom, full screen, print-to-PDF, save page,
developer tools, extensive keyboard shortcuts, dark/light themes.

## Build

Requirements: **Qt 6.7+ (Widgets, WebEngineWidgets, Sql, Network)**, **CMake ≥ 3.21**,
**Ninja**, a C++17 compiler, and **Rust (cargo)**.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_PREFIX_PATH=/path/to/Qt/6.7.3/gcc_64/lib/cmake
cmake --build build --parallel
./build/wed-browser
```

Windows: use the `Visual Studio 17 2022` generator (x64) with MSVC 2022, or see
the CI workflow which produces a fully self-contained zip via `windeployqt`.

## Self-test / verification

The repo contains a headless self-test harness that drives the *real* UI
(under xvfb on Linux), verifies behavior and captures screenshots:

```bash
xvfb-run -a ./build/wed-selftest shots/
```

CI runs this on every push and publishes the screenshots as build artifacts.

## License

Application code: MIT (see `LICENSE`). Ships EasyList and EasyPrivacy filter
lists (their own licenses, see `src/resources/lists/`); uses Qt 6 (LGPL/GPL)
and Chromium (BSD-style and others) via Qt WebEngine.
