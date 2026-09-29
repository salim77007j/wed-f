#!/bin/bash
# bench.sh — competitive benchmark: WED (WebKitGTK) vs Chromium.
# Same Xvfb, same window size, same network, software rendering for both.
R="${1:-/home/z/bench_results.txt}"
cd /home/z/my-project/repo
source /home/z/my-project/scripts/wed-env.sh
export WED_FORCE_SOFTWARE_RENDERING=1
CHROME=/home/z/.cache/ms-playwright/chromium-1243/chrome-linux64/chrome
CHROME_DIR=/tmp/bench-chrome

tree_pids() {  # all pids in the tree rooted at $1
    local root=$1
    local kids
    echo $root
    kids=$(ps -eo pid,ppid --no-headers | awk -v r=$root '$2==r{print $1}')
    for k in $kids; do tree_pids $k; done
}

tree_rss_kb() {
    local total=0 p rss
    for p in $(tree_pids $1); do
        rss=$(ps -o rss= -p $p 2>/dev/null | tr -d ' ')
        total=$((total + ${rss:-0}))
    done
    echo $total
}

tree_cpu_jiffies() {
    local total=0 p v
    for p in $(tree_pids $1); do
        v=$(awk '{print $14+$15}' /proc/$p/stat 2>/dev/null)
        total=$((total + ${v:-0}))
    done
    echo $total
}

kill_tree() {
    kill -9 $1 2>/dev/null
    sleep 0.5
    pkill -9 -f "WebKit(Web|Network|GPU)Process" 2>/dev/null
    pkill -9 -f "chrome_crashpad" 2>/dev/null
    sleep 1
}

wait_window_title() {  # $1=pid; echoes "TIME TITLE"
    local pid=$1 t0=$(date +%s.%N) title=""
    for i in $(seq 1 400); do
        local wid=$(xdotool search --onlyvisible --pid $pid 2>/dev/null | head -1)
        if [ -n "$wid" ]; then
            title=$(xdotool getwindowname $wid 2>/dev/null)
            # a loaded page shows its own title
            if [ -n "$title" ] && ! echo "$title" | grep -qE "^(WED|New tab|Chromium|Google Chrome|.* — WED)$"; then
                break
            fi
            if echo "$title" | grep -qE " — WED$| - (Google Chrome|Chromium)$"; then
                if ! echo "$title" | grep -qE "^(New tab|WED|Chromium|Google Chrome)"; then break; fi
            fi
        fi
        sleep 0.05
    done
    local t1=$(date +%s.%N)
    echo "$(echo "$t1 $t0" | awk '{printf "%.2f", $1-$2}')|$title"
}

echo "=== 1. STARTUP TO WINDOW ===" | tee "$R"
for br in wed chrome; do
    for run in 1 2 3; do
        rm -rf $CHROME_DIR
        if [ $br = wed ]; then
            ./wed-browser > /dev/null 2>&1 & pid=$!
        else
            $CHROME --user-data-dir=$CHROME_DIR --no-first-run --no-default-browser-check \
                --window-size=1280,850 --window-position=0,0 --disable-gpu \
                "about:blank" > /dev/null 2>&1 & pid=$!
        fi
        t0=$(date +%s.%N)
        for i in $(seq 1 400); do
            [ -n "$(xdotool search --onlyvisible --pid $pid 2>/dev/null)" ] && break
            sleep 0.05
        done
        t1=$(date +%s.%N)
        echo "startup $br run$run: $(echo "$t1 $t0" | awk '{printf "%.2f", $1-$2}')s" | tee -a "$R"
        kill_tree $pid
    done
done

echo "=== 2. COLD PAGE LOAD (example.com) ===" | tee -a "$R"
for br in wed chrome; do
    rm -rf $CHROME_DIR
    if [ $br = wed ]; then
        ./wed-browser "https://example.com" > /home/z/bench_$br.log 2>&1 & pid=$!
    else
        $CHROME --user-data-dir=$CHROME_DIR --no-first-run --no-default-browser-check \
            --window-size=1280,850 --window-position=0,0 --disable-gpu \
            --disable-features=Translate,MediaRouter,OptimizationHints \
            "https://example.com" > /home/z/bench_$br.log 2>&1 & pid=$!
    fi
    res=$(wait_window_title $pid)
    echo "coldload $br example.com: ${res%%|*}s (title: ${res##*|})" | tee -a "$R"
    sleep 4
    echo "rss1tab $br: $(tree_rss_kb $pid) KB" | tee -a "$R"
    c0=$(tree_cpu_jiffies $pid); sleep 8; c1=$(tree_cpu_jiffies $pid)
    echo "cpuidle8s $br: $((c1 - c0)) jiffies" | tee -a "$R"
    xwd -root -out /tmp/bench_${br}_1tab.xwd 2>/dev/null
    kill_tree $pid
done

echo "=== 3. FIVE TABS MEMORY ===" | tee -a "$R"
open5() {  # $1 = browser
    local br=$1
    xdotool key ctrl+t; sleep 1.2
    xdotool key ctrl+l; sleep 0.4
    xdotool type --delay 12 "https://example.com"; xdotool key Return; sleep 5
    xdotool key ctrl+t; sleep 1.2
    xdotool key ctrl+l; sleep 0.4
    xdotool type --delay 12 "https://www.wikipedia.org"; xdotool key Return; sleep 9
    xdotool key ctrl+t; sleep 1.2
    xdotool key ctrl+l; sleep 0.4
    xdotool type --delay 12 "https://example.com"; xdotool key Return; sleep 5
    xdotool key ctrl+t; sleep 1.2
    xdotool key ctrl+l; sleep 0.4
    xdotool type --delay 12 "https://www.bbc.com/news"; xdotool key Return; sleep 14
}
for br in wed chrome; do
    rm -rf $CHROME_DIR
    if [ $br = wed ]; then
        ./wed-browser "https://www.bbc.com/news" > /home/z/b5_$br.log 2>&1 & pid=$!
    else
        $CHROME --user-data-dir=$CHROME_DIR --no-first-run --no-default-browser-check \
            --window-size=1280,850 --window-position=0,0 --disable-gpu \
            --disable-features=Translate,MediaRouter,OptimizationHints \
            "https://www.bbc.com/news" > /home/z/b5_$br.log 2>&1 & pid=$!
    fi
    sleep 16
    open5 $br
    echo "rss5tabs $br: $(tree_rss_kb $pid) KB" | tee -a "$R"
    c0=$(tree_cpu_jiffies $pid); sleep 8; c1=$(tree_cpu_jiffies $pid)
    echo "cpuidle8s-5tabs $br: $((c1 - c0)) jiffies" | tee -a "$R"
    xwd -root -out /tmp/bench_${br}_5tabs.xwd 2>/dev/null
    kill_tree $pid
done

echo "=== 4. DISK FOOTPRINT ===" | tee -a "$R"
echo "wed binary: $(stat -c%s wed-browser) bytes + system WebKitGTK (already installed)" | tee -a "$R"
echo "chromium-1243 dir: $(du -sb /home/z/.cache/ms-playwright/chromium-1243 | cut -f1) bytes (self-contained)" | tee -a "$R"
echo "BENCH DONE" | tee -a "$R"
