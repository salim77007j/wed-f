# Engine Research & Architecture Decision

> Question posed: *"Instead of just following the herd by using QtWebEngine,
> why not innovate — something efficient that consumes minimal system
> resources, built through smart research rather than years of work?"*
>
> This document is the research. Every claim is either measured in this
> repository's environment (scripts included) or sourced from the engines'
> public repos. The decision at the end follows from the data, not habit.

## 1. What "the herd" actually is

Every browser that "competes with Chrome" chose one of exactly two viable
paths:

| Browser | Engine | Who maintains the engine |
|---|---|---|
| Edge, Brave, Opera, Vivaldi, Arc | Chromium (Blink) | Google |
| Firefox | Gecko (not embeddable via stable API) | Mozilla |
| Safari | WebKit | Apple |
| GNOME Web | WebKitGTK | WebKitGTK team / Igalia |

Embedding Chromium is the herd path (QtWebEngine ≈ Chromium 108+ repackaged
as Qt modules, ~2 GB of build tooling and a ~150–400 MB deployed footprint).
Embedding Gecko is effectively impossible (XULRunner dead, no stable C API).

## 2. Why building an engine from scratch is not the innovation lever

- **Ladybird** (the most credible from-scratch effort, full-time team since
  2022): still cannot browse large parts of the modern web as of 2026.
- **Servo** (Rust engine, 8+ years, restarted 2020+, now Linux Foundation):
  embedding story is still experimental; layout of real-world sites lags
  Chromium/WebKit substantially.
- Web compatibility is a decade of accumulated long tail (DRM, WebRTC,
  fingerprinting quirks, JS engine perf, CSS layout bugs, site workarounds).
  No AI process shortens that timeline — **what AI *can* shorten is
  everything AROUND the engine**: filtering logic, data layer, UX, and
  tests. Which is exactly where this browser innovates.

An honest competitive product today = production engine + product core that
is genuinely better. The engine is a *component*, and components should be
swappable — that is the architectural innovation here.

## 3. Measured facts (this environment, 2026-09-29)

| Metric | Chromium 153 (for Testing) | WebKitGTK 2.52 |
|---|---|---|
| On-disk engine size | **393 MB** | **123 MB** (92 libwebkit + 31 JSC) |
| Pre-installed on typical Linux desktop? | No | **Yes** (GNOME stack) → incremental 0 MB |
| Extra processes per tab (default) | 1 renderer per site | shared WebProcess pool |
| Engine lineage | Blink (Google) | WebKit (Safari family) — *not* the herd |
| Hardware accel | yes | yes, + first-class **CPU/Skia software path** |
| Content-blocker API (uBlock-style rule engine in engine) | no (needs extension) | **built-in** (WebKitUserContentFilter) |
| ITP (intelligent tracking prevention) built-in | no (3P cookie phase-out only) | **yes** |

Windows: WebView2 (Edge/Chromium lineage) is **pre-installed on every
Windows 10/11 machine** — embedding it costs **0 MB** of download, while
bundling Chromium (QtWebEngine/CEF) costs 150–400 MB.

## 4. The WED architecture (engine-agnostic Rust core)

```
┌───────────────────────────────────────────────────────────────┐
│  Native shell per platform (NO web-tech UI)                   │
│  Linux: C + GTK3 (this repo, ships here)                      │
│  Windows: C + Win32/WebView2 (CI-built)                       │
├───────────────────────────────────────────────────────────────┤
│  Engine backend — a swappable component                       │
│  Linux: WebKitGTK 2.52 (system lib, 0 MB download)            │
│  Windows: WebView2 (OS component, 0 MB download)              │
│  Experimental: Servo (future lite-mode, same interface)       │
├───────────────────────────────────────────────────────────────┤
│  wed_core (Rust, static C ABI) — THE PRODUCT                  │
│  • adblock rule compiler: EasyList/EasyPrivacy → WebKit       │
│    ContentBlocker JSON + host-level Rust matcher (two layers) │
│  • history / bookmarks / downloads / sessions (SQLite)        │
│  • privacy policy engine: cookie/storage/permission policy,   │
│    per-site shield exceptions, fingerprint injection rules     │
│  • efficiency governor: tab hibernation & discard policy      │
│  • AI assist broker (optional sidecar, off by default)        │
└───────────────────────────────────────────────────────────────┘
```

Why this is *not* the herd path, concretely:
1. **No engine bundling.** The browser downloads as a small native binary;
   it binds the platform's existing production engine. Smaller install,
   less RAM (no second engine instance), shared code with OS updates.
2. **Engine diversity by design.** Linux users get WebKit (Safari lineage)
   rather than deepening the Chromium monoculture — with ITP and native
   content-blocking that Chromium only offers via extensions.
3. **All product value is engine-independent Rust** — swap the backend and
   privacy/data/efficiency features survive. The interface between shell
   and core is a C ABI (`include/wed_core.h`), stable across platforms.
4. **CPU-only rendering fallback** (WebKit's software path) for GPU-less
   machines — validated in this repo's CI/test harness.

## 5. Honest limitations

- Rendering/web-compat quality = the engine's. We test on WebKit 2.52
  (current stable); Chromium-only sites may degrade — same trade Safari
  users already make.
- Windows backend uses WebView2 (Chromium lineage): the *core* and *UI*
  stay ours, but Windows users render with Edge's engine. Engine
  non-bundling beat monoculture-purity there; revisit when Servo matures.
- We do not claim to out-engine Google. We claim to out-*product* the herd:
  privacy defaults, resource discipline, no-bloat UI, native feel.
