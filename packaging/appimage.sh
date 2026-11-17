#!/usr/bin/env bash
# Builds ARM_IDE-<version>-x86_64.AppImage in build/appimage/ from a release build.
# The cross tools, QEMU, fontconfig's fc-list/fc-match and gdbus stay the user's own.
# Usage: packaging/appimage.sh
set -euo pipefail
cd "$(dirname "$0")/.."

linuxdeploy_tag=1-alpha-20251107-1
linuxdeploy_sha256=c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
work=build/appimage

cmake --preset release
cmake --build --preset release
version=$(sed -n 's/^CMAKE_PROJECT_VERSION:STATIC=//p' build/CMakeCache.txt)

rm -rf "$work/AppDir"
DESTDIR="$work/AppDir" cmake --install build --prefix /usr

linuxdeploy="$work/linuxdeploy"
if ! echo "$linuxdeploy_sha256  $linuxdeploy" | sha256sum --check --status 2>/dev/null; then
    curl -fsSLo "$linuxdeploy" \
        "https://github.com/linuxdeploy/linuxdeploy/releases/download/$linuxdeploy_tag/linuxdeploy-x86_64.AppImage"
    echo "$linuxdeploy_sha256  $linuxdeploy" | sha256sum --check
    chmod +x "$linuxdeploy"
fi

# Extract-and-run: CI runners have no FUSE to mount linuxdeploy's own AppImage.
cd "$work"
APPIMAGE_EXTRACT_AND_RUN=1 LINUXDEPLOY_OUTPUT_VERSION="$version" ./linuxdeploy \
    --appdir AppDir \
    --desktop-file AppDir/usr/share/applications/arm_ide.desktop \
    --icon-file AppDir/usr/share/icons/hicolor/256x256/apps/arm_ide.png \
    --output appimage
echo "Built $work/ARM_IDE-$version-x86_64.AppImage"
