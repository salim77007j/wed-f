#!/bin/sh
# resolvelibs.sh — turn pkg-config --libs output into full .so paths
# (sandbox has no dev symlinks). Usage: resolvelibs.sh <pkg...>
ROOTS="${LIBDIR:-/home/z/rootfs/usr/lib/x86_64-linux-gnu} /usr/lib/x86_64-linux-gnu /lib/x86_64-linux-gnu"
for tok in $(pkg-config --libs "$@"); do
    case "$tok" in
    -l*)
        n=${tok#-l}
        f=""
        for r in $ROOTS; do
            for sfx in "" .0 .1 .2; do
                if [ -e "$r/lib$n.so$sfx" ]; then f="$r/lib$n.so$sfx"; break 2; fi
            done
        done
        if [ -n "$f" ]; then echo "$f"; else echo "$tok"; fi
        ;;
    -L*) ;;
    *) echo "$tok" ;;
    esac
done
