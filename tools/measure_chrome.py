#!/usr/bin/env python3
"""measure_chrome.py — precise UI metrics from the Chrome screenshots.

Measures row-band boundaries (tab strip / toolbar / bookmarks / content),
extracts the exact palette, and reports key geometry numbers that will
drive our browser's design.
"""
from PIL import Image
import os, json

UX = "/home/z/ux"

def row_color(im, y):
    w, h = im.size
    px = [im.getpixel((x, y)) for x in range(0, w, 4)]
    n = len(px)
    return tuple(sum(c[i] for c in px) // n for i in range(3))

def find_bands(im, max_y=140):
    """Scan rows 0..max_y for band boundaries by mean-color change."""
    bands = []
    last = None
    for y in range(0, max_y):
        c = row_color(im, y)
        if last is None:
            last = c; start = 0; continue
        # color distance
        d = sum((a - b) ** 2 for a, b in zip(c, last)) ** 0.5
        if d > 12:
            bands.append((start, y, last))
            start = y
        last = c
    bands.append((start, max_y, last))
    return bands

def px_at(im, x, y): return im.getpixel((x, y))

report = {}

im = Image.open(f"{UX}/03_chrome_three_tabs.png")
report["window"] = im.size
report["bands_page"] = [(a, b, c) for a, b, c in find_bands(im)]
# precise palette probes (three tabs open, light theme)
report["colors"] = {
    "tabstrip_bg_topleft": px_at(im, 5, 8),
    "tabstrip_bg_mid": px_at(im, 800, 12),
    "active_tab_title_area": px_at(im, 260, 15),
    "tab_underline_blue": px_at(im, 260, 44),
    "toolbar_bg": px_at(im, 800, 60),
    "toolbar_bg_below": px_at(im, 800, 70),
    "omnibox_bg": px_at(im, 700, 62),
    "omnibox_border": px_at(im, 585, 62),
    "newtab_btn": px_at(im, 396, 25),
    "content_bg": px_at(im, 800, 300),
}

im2 = Image.open(f"{UX}/04_chrome_newtab.png")
report["bands_ntp"] = [(a, b, c) for a, b, c in find_bands(im2)]
report["colors"]["ntp_bg"] = px_at(im2, 800, 500)
report["colors"]["ntp_omnibox_area"] = px_at(im2, 800, 400)

im3 = Image.open(f"{UX}/06_chrome_settings.png")
report["bands_settings"] = [(a, b, c) for a, b, c in find_bands(im3)]
report["colors"]["settings_bg"] = px_at(im3, 800, 300)
report["colors"]["settings_nav"] = px_at(im3, 130, 300)

# toolbar icon area: find dark pixels (icons) in toolbar row band
im4 = Image.open(f"{UX}/01_chrome_page_example.png")
icons = [x for x in range(0, 1600, 2) if sum(im4.getpixel((x, 61))) < 200]
report["toolbar_icon_xs"] = (min(icons), max(icons), len(icons)) if icons else None

# find the exact toolbar height: locate the omnibox rounded rect on the left side
# omnibox starts after back/fwd/reload/home icons ~ x=140
for y in range(45, 90):
    c = px_at(im4, 200, y)
    # omnibox background is white-ish, toolbar bg slightly gray
    if c[0] > 250 and c[1] > 250 and c[2] > 250:
        report["omnibox_top_y"] = y; break
for y in range(90, 45, -1):
    c = px_at(im4, 200, y)
    if c[0] > 250 and c[1] > 250 and c[2] > 250:
        report["omnibox_bottom_y"] = y; break

# tab strip height: active tab's blue underline y + a couple px
for y in range(30, 60):
    c = px_at(im, 260, y)
    if c[2] > 200 and c[0] < 120:  # blue
        report["tab_underline_y"] = y; break

print(json.dumps(report, indent=1))
