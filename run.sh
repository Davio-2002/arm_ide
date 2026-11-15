#!/usr/bin/env bash
# Installs what is missing (apt, dnf, pacman or zypper), builds the IDE and starts it.
# Usage: ./run.sh [file.s]
set -euo pipefail
cd "$(dirname "$0")"

if command -v apt-get &>/dev/null; then
    packages=(clang cmake ninja-build pkg-config git
              binutils-aarch64-linux-gnu gcc-aarch64-linux-gnu libc6-dev-arm64-cross
              qemu-user
              libglfw3-dev libgl-dev libfreetype-dev libvterm-dev)
    is_installed() { dpkg -s "$1" &>/dev/null; }
    install() { sudo apt-get update && sudo apt-get install -y "$@"; }
elif command -v dnf &>/dev/null; then
    packages=(clang gcc-c++ cmake ninja-build pkgconf-pkg-config git
              binutils-aarch64-linux-gnu qemu-user
              glfw-devel mesa-libGL-devel freetype-devel libvterm-devel)
    is_installed() { rpm -q --whatprovides "$1" &>/dev/null; }
    install() { sudo dnf install -y "$@"; }
    no_cross_libc=1
elif command -v pacman &>/dev/null; then
    packages=(clang cmake ninja pkgconf git
              aarch64-linux-gnu-binutils aarch64-linux-gnu-gcc qemu-user
              glfw libglvnd freetype2 libvterm)
    is_installed() { pacman -Qi "$1" &>/dev/null; }
    install() { sudo pacman -S --needed --noconfirm "$@"; }
elif command -v zypper &>/dev/null; then
    packages=(clang gcc-c++ cmake ninja pkgconf-pkg-config git
              cross-aarch64-binutils qemu-linux-user
              glfw-devel Mesa-libGL-devel freetype2-devel libvterm-devel)
    is_installed() { rpm -q --whatprovides "$1" &>/dev/null; }
    install() { sudo zypper install -y "$@"; }
    no_cross_libc=1
else
    echo "Unknown package manager. Install these yourself, then run this script again:"
    echo "  clang (C++23), cmake 3.24+, ninja, pkg-config, git,"
    echo "  AArch64 binutils (aarch64-*-as, ld, objdump, nm), for C an AArch64 GCC cross compiler"
    echo "  with its C library, QEMU user mode (qemu-aarch64),"
    echo "  GLFW 3, OpenGL, FreeType and libvterm development files."
    packages=()
fi

missing=()
for p in "${packages[@]}"; do
    is_installed "$p" || missing+=("$p")
done

if (( ${#missing[@]} )); then
    echo "Installing: ${missing[*]}"
    install "${missing[@]}"
fi

# Their AArch64 cross GCC has no C library, so it can't link C programs.
if [[ -n ${no_cross_libc-} ]]; then
    echo "Note: this distribution has no AArch64 cross C library: assembly works, C programs won't build."
fi

if [[ ! -f build/build.ninja ]]; then
    cmake --preset release
fi
cmake --build --preset release

exec ./build/arm_ide "$@"
