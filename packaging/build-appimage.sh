#!/bin/bash
#
# Build AppImage for r64u
#
# Prerequisites:
#   - linuxdeploy: https://github.com/linuxdeploy/linuxdeploy
#   - linuxdeploy-plugin-qt: https://github.com/linuxdeploy/linuxdeploy-plugin-qt
#
# Usage:
#   ./packaging/build-appimage.sh [build_dir]
#
# Environment variables:
#   LINUXDEPLOY - Path to linuxdeploy AppImage (default: downloaded automatically)
#   QT_DIR - Qt installation directory (default: auto-detected)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${1:-$PROJECT_DIR/build}"
APPDIR="$BUILD_DIR/AppDir"

# Version from CMakeLists.txt
VERSION=$(grep "project(r64u" "$PROJECT_DIR/CMakeLists.txt" | grep -oP 'VERSION \K[0-9.]+')
ARCH=$(uname -m)

echo "Building r64u AppImage v$VERSION for $ARCH"
echo "Build directory: $BUILD_DIR"
echo "AppDir: $APPDIR"

# Check for linuxdeploy
if [ -z "$LINUXDEPLOY" ]; then
    LINUXDEPLOY="$BUILD_DIR/linuxdeploy-x86_64.AppImage"
    if [ ! -f "$LINUXDEPLOY" ]; then
        echo "Downloading linuxdeploy..."
        wget -q -O "$LINUXDEPLOY" \
            "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
        chmod +x "$LINUXDEPLOY"
    fi
fi

# Check for Qt plugin
LINUXDEPLOY_QT="$BUILD_DIR/linuxdeploy-plugin-qt-x86_64.AppImage"
if [ ! -f "$LINUXDEPLOY_QT" ]; then
    echo "Downloading linuxdeploy-plugin-qt..."
    wget -q -O "$LINUXDEPLOY_QT" \
        "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
    chmod +x "$LINUXDEPLOY_QT"
fi

# Clean previous AppDir
rm -rf "$APPDIR"

# Install to AppDir
echo "Installing to AppDir..."
cmake --build "$BUILD_DIR" --target install DESTDIR="$APPDIR"

# Copy desktop file and icon
mkdir -p "$APPDIR/usr/share/applications"
mkdir -p "$APPDIR/usr/share/icons/hicolor/256x256/apps"

cp "$PROJECT_DIR/packaging/r64u.desktop" "$APPDIR/usr/share/applications/"

# Copy icon if exists
if [ -f "$PROJECT_DIR/resources/icons/app.png" ]; then
    cp "$PROJECT_DIR/resources/icons/app.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/r64u.png"
fi

# Set Qt environment
if [ -z "$QMAKE" ]; then
    QMAKE=$(which qmake6 2>/dev/null || which qmake 2>/dev/null || echo "")
fi

if [ -n "$QMAKE" ]; then
    export QMAKE
fi

# Build AppImage
echo "Building AppImage with linuxdeploy..."
export OUTPUT="r64u-$VERSION-$ARCH.AppImage"
export LDAI_UPDATE_INFORMATION="gh-releases-zsync|svetzal|r64u|latest|r64u-*-$ARCH.AppImage.zsync"

"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --desktop-file "$APPDIR/usr/share/applications/r64u.desktop" \
    --plugin qt \
    --output appimage

echo ""
echo "AppImage built successfully: $OUTPUT"
echo ""
echo "To test: ./$OUTPUT"
