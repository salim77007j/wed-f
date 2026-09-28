# UX Research: Studying Chrome 153 Before Designing WED

> Method: Chrome for Testing 153.0.8010.12 (Chromium, headed) was launched in
> the development environment at 1600×900 under Xvfb. A real interaction
> session was scripted (xdotool): opening tabs, typing URLs, opening the ⋮
> menu, tab context menu, find bar, and all chrome:// pages. Screenshots are
> in `docs/research/`, pixel measurements taken programmatically
> (`scripts/measure_chrome.py`), and layout described by a vision model
> reviewing the actual captures.

## 1. Measured layout (1600×900, light theme, default profile)

| Region | Measured geometry | Color (sampled) |
|---|---|---|
| Tab strip | y = 0–40 (40 px) | `#D3E3FD` (211,227,253) tinted |
| Inactive tab | fills strip | same tint, no separator lines |
| Active tab | white bottom half merging into toolbar | `#FFFFFF`, rounded top corners ~8–10 px |
| Toolbar | y = 40–86 (46 px) | `#EDF2FA` |
| Omnibox pill | y ≈ 45–81 (~36 px tall), full width minus icon clusters | unfocused fill `#EDF3FA`, focused white + blue ring |
| Content | y = 86+ | page-dependent |

Accent blue sampled at `#2D6ED6` (mixed toward `#1A73E8`).
Toolbar icon clusters: left navigation starts at x≈128 (back/forward/reload),
right cluster ends at x≈1536 (star, profile, ⋮ menu).

## 2. Toolbar anatomy (left → right)

1. Back, Forward, Reload (icon buttons, ~28 px targets)
2. Omnibox pill containing: site-info icon (🔒/ⓘ) + URL/search text +
   right-side icons (bookmark star lives OUTSIDE the pill in 153)
3. Right cluster: extensions, profile pill ("Sign in"), ⋮ menu

## 3. Menu structures (captured, item order verified)

**⋮ app menu** (05_chrome_menu_open.png): New tab, New window, New profile —
— History, Downloads, Bookmarks, Tabs — Passwords and autofill, Delete
browsing data, Zoom controls row (−, %, +, fullscreen), Print, Cast, Find,
Translate, Save and share, More tools → (Task manager, Performance,
Developer tools), Help, Settings, Exit.

**Tab context menu** (13): New tab to the right · New split view · Add tab
to new group · Move tab to new window · — · Reload · Duplicate · Pin ·
Mute site · — · Add tab to reading list · Send to your device · — ·
Show tabs vertically · — · Close · Close other tabs · Close tabs to the
right.

## 4. Internal page patterns

**Settings** (06): fixed top header (title + centered pill search), left
sidebar with icon+label nav (You and Google · Autofill and passwords ·
Privacy and security · Performance · AI in Chrome · Appearance · Search
engine · Default browser · On startup · Languages · Downloads ·
Accessibility · System · Reset settings · Extensions), main column of white
cards (8 px radius, shadow, no border), rows with chevron `>` navigation,
dividers between rows, blue primary buttons, in-page Ctrl+F find bar (07).

**History** (09): title + "Search history" pill, left mini-sidebar
(Chromium history · Tabs from other devices · Delete browsing data),
segmented tabs "By date / By group", stacked cards; each row = checkbox +
time + favicon + title + URL + three-dot overflow. Date group headers
("Today - Monday, September 28, 2026"). Dismissible promo cards.

**Downloads** (10) / **Bookmarks** (11): same header+sidebar+cards pattern.
**NTP** (04): centered logo, large rounded search pill, shortcut tiles grid.

## 5. Design decisions for WED (from this data)

1. **Adopt**: 40 px tab strip + 46 px toolbar, omnibox as rounded pill
   (radius = height/2) with unfocused tint / focused white+accent ring,
   white content cards with 8 px radius + subtle shadow (no hard borders),
   divider-separated rows, chevron sub-navigation in settings.
2. **Simplify**: no profile pill in the toolbar (avatar in menu instead);
   bookmark star inside the omnibox right slot (pre-2023 Chrome position —
   still familiar, one less toolbar item); no split view/tab groups v1.
3. **Keep parity on essentials**: back/forward/reload/home, new-tab `+`,
   per-tab close on hover, pinned tabs (icon-only, 32 px), tab mute,
   Ctrl+T/W/L/F, Ctrl+Click, middle-click close, session restore prompt.
4. **Differentiate (privacy-first)**: shield icon with block counter lives
   LEFT of the omnibox (visible at all times, like Brave) rather than
   buried; privacy dashboard instead of "You and Google" settings hero.
5. **Our engine = WebKit, so our settings get privacy sections WebKit
   gives us for free** (ITP, storage access, notification permissions),
   plus the Rust-core network filter toggle with live counters.

## 6. Evidence

`docs/research/01…13_*.png` — real interaction captures (not mocks).
Measurement script: `scripts/measure_chrome.py` (repo copy in
`tools/measure_chrome.py`).
