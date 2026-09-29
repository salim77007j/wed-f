#!/bin/bash
# Bundle a runnable Linux package: binary + required Qt/WebEngine runtime.
# Usage: package-linux.sh <path-to-wed-binary> [path-to-selftest]
set -e
QTDIR="${QT6_DIR:-$(ls -d /home/z/qt/6.*/*/ 2>/dev/null | head -1)}"
if [ -z "$QTDIR" ] || [ ! -d "$QTDIR" ]; then
  # derive from qmake if available
  QMAKE=$(command -v qmake6 || command -v qmake)
  if [ -n "$QMAKE" ]; then
    QTDIR=$(dirname "$(dirname "$(readlink -f "$QMAKE")")")
  fi
fi
if [ ! -d "$QTDIR/lib" ]; then
  echo "ERROR: Qt install dir not found (set QT6_DIR)"; exit 1
fi
echo "Qt dir: $QTDIR"

BIN="$1"; SELFTEST="$2"
[ -x "$BIN" ] || { echo "ERROR: binary $BIN missing"; exit 1; }

PKG=package
rm -rf "$PKG"; mkdir -p "$PKG/bin" "$PKG/lib" "$PKG/libexec" "$PKG/plugins" \
  "$PKG/resources" "$PKG/translations"

cp -L "$BIN" "$PKG/bin/"
[ -n "$SELFTEST" ] && [ -x "$SELFTEST" ] && cp -L "$SELFTEST" "$PKG/bin/"

# --- Qt libraries the binary needs (follow first level) ---
mapfile -t libs < <(ldd "$BIN" | awk '/=> \// {print $3}' | grep -E "Qt6|\.so" | sort -u)
for l in "${libs[@]}"; do
  case "$l" in
    *Qt6WebEngineCore*|*Qt6WebEngineWidgets*|*Qt6WebEngineQuick*|*Qt6Qml*|*Qt6Quick*|*Qt6QuickWidgets*) ;;
    *) cp -L "$l" "$PKG/lib/" 2>/dev/null || true ;;
  esac
done

# WebEngine ships as a group: core + widgets + process + resources
cp -L "$QTDIR"/lib/libQt6WebEngineCore.so* "$PKG/lib/" 2>/dev/null || true
cp -L "$QTDIR"/lib/libQt6WebEngineWidgets.so* "$PKG/lib/" 2>/dev/null || true
cp -L "$QTDIR"/lib/libQt6WebEngineQuick.so* "$PKG/lib/" 2>/dev/null || true
cp -L "$QTDIR"/lib/libQt6Quick.so* "$PKG/lib/" 2>/dev/null || true
cp -L "$QTDIR"/lib/libQt6QuickWidgets.so* "$PKG/lib/" 2>/dev/null || true
cp -L "$QTDIR"/lib/libQt6Qml.so* "$PKG/lib/" 2>/dev/null || true
cp -L "$QTDIR"/lib/libQt6QmlModels.so* "$PKG/lib/" 2>/dev/null || true
cp -L "$QTDIR"/lib/libQt6QmlWorkerScript.so* "$PKG/lib/" 2>/dev/null || true
cp -L "$QTDIR"/lib/libQt6OpenGL.so* "$PKG/lib/" 2>/dev/null || true
cp -L "$QTDIR"/lib/libQt6Svg.so* "$PKG/lib/" 2>/dev/null || true
cp -L "$QTDIR"/lib/libQt6Network.so* "$PKG/lib/" 2>/dev/null || true
cp -L "$QTDIR"/lib/libQt6PrintSupport.so* "$PKG/lib/" 2>/dev/null || true
cp -L "$QTDIR"/lib/libQt6Sql.so* "$PKG/lib/" 2>/dev/null || true
cp -L "$QTDIR"/lib/libQt6Positioning.so* "$PKG/lib/" 2>/dev/null || true

# Chromium process + resources + locales
[ -f "$QTDIR/libexec/QtWebEngineProcess" ] && cp -L "$QTDIR/libexec/QtWebEngineProcess" "$PKG/libexec/"
cp -r "$QTDIR/resources/." "$PKG/resources/"
mkdir -p "$PKG/translations"
cp -r "$QTDIR/translations/qtwebengine_locales" "$PKG/translations/" 2>/dev/null || true

# plugins
for p in platforms imageformats iconengines tls networkinformation; do
  if [ -d "$QTDIR/plugins/$p" ]; then
    mkdir -p "$PKG/plugins/$p"
    cp -L "$QTDIR/plugins/$p/"*.so "$PKG/plugins/$p/" 2>/dev/null || true
  fi
done

# launcher
cat > "$PKG/wed" <<'EOF'
#!/bin/bash
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$HERE/plugins"
export QTWEBENGINEPROCESS_PATH="$HERE/libexec/QtWebEngineProcess"
export QTWEBENGINE_RESOURCES_PATH="$HERE/resources"
export QTWEBENGINE_LOCALES_PATH="$HERE/translations/qtwebengine_locales"
exec "$HERE/bin/wed-browser" "$@"
EOF
chmod +x "$PKG/wed"

tar czf wed-linux-x64.tar.gz "$PKG"
echo "packaged:"
du -sh "$PKG" wed-linux-x64.tar.gz
