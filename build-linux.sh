#!/bin/bash
# Build snipQ for Linux and create a self-contained bundle
set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
DIST_DIR="$SCRIPT_DIR/dist/snipQ-linux"

echo "=== snipQ Linux Build ==="

# Check dependencies
check_dep() { command -v "$1" &>/dev/null || { echo "ERROR: $1 not found. Install it first."; exit 1; }; }
check_dep cmake
check_dep ninja || true

# Check Qt6
if ! pkg-config --exists Qt6Core 2>/dev/null && \
   ! cmake --find-package -DNAME=Qt6 -DCOMPILER_ID=GNU -DLANGUAGE=CXX -DMODE=EXIST &>/dev/null 2>&1; then
    echo ""
    echo "Qt6 not found. Install it:"
    echo "  Ubuntu/Debian: sudo apt install qt6-base-dev qt6-tools-dev"
    echo "  Arch/CachyOS:  sudo pacman -S qt6-base qt6-tools"
    echo "  Fedora:        sudo dnf install qt6-qtbase-devel"
    echo ""
fi

# Build
echo "[1/4] Configuring..."
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
    ${1:+-DCMAKE_PREFIX_PATH="$1"}
echo "[2/4] Building..."
cmake --build "$BUILD_DIR" --parallel "$(nproc)"
strip "$BUILD_DIR/snipQ"

# Bundle
echo "[3/4] Creating bundle..."
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/bin" "$DIST_DIR/lib" \
         "$DIST_DIR/plugins/platforms" "$DIST_DIR/plugins/sqldrivers" \
         "$DIST_DIR/plugins/imageformats" "$DIST_DIR/plugins/xcbglintegrations"

cp "$BUILD_DIR/snipQ" "$DIST_DIR/bin/"

# Detect Qt installation
QT_LIB_DIR=$(pkg-config --variable=libdir Qt6Core 2>/dev/null || \
             find /usr/lib -name "libQt6Core.so.6" 2>/dev/null | head -1 | xargs dirname)
QT_PLUGIN_DIR=$(pkg-config --variable=plugindir Qt6Core 2>/dev/null || \
                find /usr -path "*/qt6/plugins" -type d 2>/dev/null | head -1)

if [ -n "$QT_LIB_DIR" ]; then
    for lib in libQt6Widgets libQt6Sql libQt6Network libQt6Gui libQt6Core \
               libQt6DBus libQt6XcbQpa libQt6OpenGL libQt6OpenGLWidgets; do
        src=$(find "$QT_LIB_DIR" -name "${lib}.so.6" 2>/dev/null | head -1)
        [ -n "$src" ] && cp "$src" "$DIST_DIR/lib/"
    done
fi

if [ -n "$QT_PLUGIN_DIR" ]; then
    [ -f "$QT_PLUGIN_DIR/platforms/libqxcb.so"      ] && cp "$QT_PLUGIN_DIR/platforms/libqxcb.so"      "$DIST_DIR/plugins/platforms/"
    [ -f "$QT_PLUGIN_DIR/platforms/libqoffscreen.so" ] && cp "$QT_PLUGIN_DIR/platforms/libqoffscreen.so" "$DIST_DIR/plugins/platforms/"
    [ -f "$QT_PLUGIN_DIR/sqldrivers/libqsqlite.so"  ] && cp "$QT_PLUGIN_DIR/sqldrivers/libqsqlite.so"  "$DIST_DIR/plugins/sqldrivers/"
    find "$QT_PLUGIN_DIR/imageformats" -name "*.so" 2>/dev/null | while read f; do cp "$f" "$DIST_DIR/plugins/imageformats/"; done
    find "$QT_PLUGIN_DIR/xcbglintegrations" -name "*.so" 2>/dev/null | while read f; do cp "$f" "$DIST_DIR/plugins/xcbglintegrations/"; done
fi

# Collect system libs via ldd
ldd "$DIST_DIR/bin/snipQ" 2>/dev/null | awk '{print $3}' | grep "^/" | sort -u | while read dep; do
    base=$(basename "$dep")
    case "$base" in linux-vdso*|ld-linux*|libQt6*) continue ;; esac
    [ ! -f "$DIST_DIR/lib/$base" ] && [ -f "$dep" ] && cp "$dep" "$DIST_DIR/lib/"
done

# Patch rpaths
if command -v patchelf &>/dev/null; then
    patchelf --set-rpath '$ORIGIN/../lib' "$DIST_DIR/bin/snipQ"
    for lib in "$DIST_DIR/lib"/libQt6*.so.6; do
        patchelf --set-rpath '$ORIGIN' "$lib" 2>/dev/null || true
    done
fi

# qt.conf
cat > "$DIST_DIR/bin/qt.conf" << 'QTCONF'
[Paths]
Prefix = ..
Plugins = plugins
Libraries = lib
QTCONF

# Launcher
cat > "$DIST_DIR/snipQ.sh" << 'LAUNCHER'
#!/bin/sh
DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$DIR/lib:$LD_LIBRARY_PATH"
export QT_PLUGIN_PATH="$DIR/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$DIR/plugins/platforms"
exec "$DIR/bin/snipQ" "$@"
LAUNCHER
chmod +x "$DIST_DIR/snipQ.sh"
cp "$SCRIPT_DIR/README.md" "$DIST_DIR/" 2>/dev/null || true

# Zip
echo "[4/4] Packaging..."
cd "$SCRIPT_DIR/dist"
tar czf "../snipQ-linux-x86_64.tar.gz" "snipQ-linux/"
echo ""
echo "✓ Linux bundle: $SCRIPT_DIR/snipQ-linux-x86_64.tar.gz"
echo "  Run: ./dist/snipQ-linux/snipQ.sh"
