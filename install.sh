#!/usr/bin/env bash
# Installs the latest ARM IDE AppImage for the current user, with the AArch64 tools it runs.
# Running it again updates the IDE. Building from a clone of the repository is run.sh's job.
# Usage: curl -fsSL https://raw.githubusercontent.com/Davio-2002/arm_ide/main/install.sh | bash
set -euo pipefail

repository=Davio-2002/arm_ide
program="$HOME/.local/bin/arm_ide"
applications="$HOME/.local/share/applications"
icons="$HOME/.local/share/icons/hicolor/256x256/apps"

if [[ $(uname -m) != x86_64 ]]; then
    echo "The AppImage is built for x86_64 only; build from source instead (see the README)."
    exit 1
fi

# The AppImage is built on Ubuntu 24.04 and needs its glibc or newer.
glibc=$(getconf GNU_LIBC_VERSION | cut -d' ' -f2)
if [[ $(printf '%s\n' 2.39 "$glibc" | sort -V | head -1) != 2.39 ]]; then
    echo "glibc $glibc is older than 2.39; build from source instead (see the README)."
    exit 1
fi

# The tools the IDE runs: assembler, linker, C cross compiler and library, QEMU.
if command -v apt-get &>/dev/null; then
    packages=(binutils-aarch64-linux-gnu gcc-aarch64-linux-gnu libc6-dev-arm64-cross qemu-user)
    is_installed() { dpkg -s "$1" &>/dev/null; }
    install() { sudo apt-get update && sudo apt-get install -y "$@"; }
elif command -v dnf &>/dev/null; then
    packages=(binutils-aarch64-linux-gnu qemu-user)
    is_installed() { rpm -q --whatprovides "$1" &>/dev/null; }
    install() { sudo dnf install -y "$@"; }
    no_cross_libc=1
elif command -v pacman &>/dev/null; then
    packages=(aarch64-linux-gnu-binutils aarch64-linux-gnu-gcc qemu-user)
    is_installed() { pacman -Qi "$1" &>/dev/null; }
    install() { sudo pacman -S --needed --noconfirm "$@"; }
elif command -v zypper &>/dev/null; then
    packages=(cross-aarch64-binutils qemu-linux-user)
    is_installed() { rpm -q --whatprovides "$1" &>/dev/null; }
    install() { sudo zypper install -y "$@"; }
    no_cross_libc=1
else
    echo "Unknown package manager: install AArch64 binutils, an AArch64 GCC cross compiler with"
    echo "its C library and QEMU user mode (qemu-aarch64) yourself."
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

url=$(curl -fsSL "https://api.github.com/repos/$repository/releases/latest" |
      grep -o '"browser_download_url": *"[^"]*x86_64\.AppImage"' | cut -d'"' -f4 || true)
if [[ -z $url ]]; then
    echo "No AppImage found in the latest release of $repository."
    exit 1
fi

echo "Downloading ${url##*/}"
mkdir -p "${program%/*}" "$applications" "$icons"
curl -fL --progress-bar -o "$program.new" "$url"
chmod +x "$program.new"
mv "$program.new" "$program"

# The menu entry and icon come from inside the AppImage.
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
( cd "$work" && "$program" --appimage-extract 'usr/share/*' >/dev/null )
cp "$work/squashfs-root/usr/share/icons/hicolor/256x256/apps/arm_ide.png" "$icons/"
sed "s|^Exec=.*|Exec=$program %f|" "$work/squashfs-root/usr/share/applications/arm_ide.desktop" \
    > "$applications/arm_ide.desktop"

echo "Installed $program and added ARM IDE to the applications menu."
if [[ -n ${no_cross_libc-} ]]; then
    echo "Note: this distribution has no AArch64 cross C library: assembly works, C programs won't build."
fi
if ! command -v fusermount3 &>/dev/null && ! command -v fusermount &>/dev/null; then
    echo "Note: no FUSE found; start it with: arm_ide --appimage-extract-and-run"
fi
if [[ :$PATH: != *":${program%/*}:"* ]]; then
    echo "Note: ${program%/*} is not in PATH; start it from the menu or as $program"
fi
