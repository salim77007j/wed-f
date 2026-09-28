#!/bin/bash
# ui_tests.sh — drive WED with xdotool and screenshot EVERY feature.
# Usage: run-x11.sh 1280x850x24 -- ui_tests.sh <shotdir>
OUT="${1:-/home/z/wedui}"
mkdir -p "$OUT"
rm -f "$OUT"/*.png
cd /home/z/my-project/repo

sh() { xwd -root -out "$OUT/$1.xwd"; python3 /home/z/my-project/scripts/xwd2png.py "$OUT/$1.xwd" "$OUT/$1.png" >/dev/null && echo "shot $1"; }
key() { xdotool key "$1"; sleep "$2"; }

export WED_FORCE_SOFTWARE_RENDERING=1
${WED_BIN:-./wed-browser} "https://example.com" > /home/z/wed_test.log 2>&1 &
BPID=$!
sleep 12
sh 01_page_load

# ---- omnibox navigation like a user ----
xdotool key ctrl+l; sleep 0.5
xdotool type --delay 30 "www.bbc.com/news"; sleep 1
xdotool key Return; sleep 10
sh 02_navigate_bbc

# ---- new tab: start page ----
key ctrl+t 3
sleep 4
sh 03_startpage

# ---- search from omnibox (non-URL) ----
xdotool key ctrl+l; sleep 0.5
xdotool type --delay 30 "browser"; sleep 0.5
xdotool key Return; sleep 10
sh 04_search

# ---- third tab + tab strip ----
key ctrl+t 2
xdotool key ctrl+l; sleep 0.4
xdotool type --delay 30 "https://example.com"; sleep 0.3
xdotool key Return; sleep 8
sh 05_three_tabs

# ---- back/forward ----
xdotool key alt+Left; sleep 3
sh 06_back

# ---- find bar ----
xdotool key ctrl+f; sleep 0.6
xdotool type --delay 25 "example"; sleep 2
sh 07_findbar
xdotool key Escape; sleep 0.4

# ---- bookmark page (Ctrl+D) ----
key ctrl+d 1
sh 08_bookmarked

# ---- history panel ----
key ctrl+h 2
sh 09_history_panel
key ctrl+h 1

# ---- bookmarks panel ----
key ctrl+b 2
sh 10_bookmarks_panel
key ctrl+b 1

# ---- app menu (⋮) at ~ (1170, 63) ----
xdotool mousemove 1258 65; xdotool click 1; sleep 1.2
sh 11_app_menu
xdotool key Escape; sleep 0.4

# ---- privacy dashboard (shield button ~ (300,63)) ----
xdotool mousemove 170 65; xdotool click 1; sleep 1.2
sh 12_privacy_dashboard
xdotool key Escape; sleep 0.3

# ---- downloads popover (button ~ (1080,63)) ----
xdotool mousemove 1220 65; xdotool click 1; sleep 1
sh 13_downloads
xdotool key Escape; sleep 0.3

# ---- settings (menu + keyboard navigation: Settings is the 13th item) ----
xdotool mousemove 1258 65; xdotool click 1; sleep 1.2
for i in $(seq 1 13); do xdotool key Down; sleep 0.15; done
xdotool key Return; sleep 1.5
sh 14_settings
xdotool key Escape; sleep 0.5

# ---- tab context menu (right click on 1st tab) ----
xdotool mousemove 100 20; xdotool click 3; sleep 1
sh 15_tab_context_menu
xdotool key Escape; sleep 0.3

# ---- zoom ----
xdotool key ctrl+plus; xdotool key ctrl+plus; sleep 1
sh 16_zoomed_in
xdotool key ctrl+0; sleep 0.5

# ---- session restore: hard kill, restart ----
kill -9 $BPID 2>/dev/null; sleep 1
${WED_BIN:-./wed-browser} > /home/z/wed_test2.log 2>&1 &
BPID2=$!
sleep 14
sh 17_session_restore_prompt
# answer YES to restore dialog (default focus on Yes)
xdotool key Return; sleep 12
sh 18_session_restored

# ---- ad/tracker block test: local page with known ad+tracker URLs ----
kill -9 $BPID2 2>/dev/null; sleep 1
mkdir -p /tmp/wedtest && cat > /tmp/wedtest/adtest.html <<'HTML'
<!DOCTYPE html><html><head><title>Ad Block Test</title></head>
<body style="background:#fff">
<h1>Ad Block Test Page</h1>
<p>This page references known ad and tracker endpoints.</p>
<img src="https://www.google-analytics.com/collect?v=1&t=pageview" alt="tracker">
<img src="https://ad.doubleclick.net/ddm/adj/test" alt="ad">
<img src="https://example.com/ok.png" alt="ok">
<div id="banner" style="width:728px;height:90px;background:#eee">AD SLOT</div>
</body></html>
HTML
${WED_BIN:-./wed-browser} "file:///tmp/wedtest/adtest.html" > /home/z/wed_test3.log 2>&1 &
BPID3=$!
sleep 18
sh 19_adblock_test_page

# privacy dashboard after ad test
xdotool mousemove 170 65; xdotool click 1; sleep 1.5
sh 20_adblock_privacy_stats
xdotool key Escape; sleep 0.3

# ---- devtools ----
xdotool key F12; sleep 3
sh 21_devtools
kill -9 $BPID3 2>/dev/null
echo "UI TEST SESSION COMPLETE: $(ls $OUT/*.png | wc -l) screenshots"
