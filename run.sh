#!/usr/bin/env bash
# Installs what is missing (Debian/Ubuntu), builds the IDE and starts it.
# Usage: ./run.sh [file.s]
set -euo pipefail
cd "$(dirname "$0")"

packages=(clang cmake ninja-build pkg-config git
          binutils-aarch64-linux-gnu qemu-user
          libglfw3-dev libgl-dev libfreetype-dev libvterm-dev)

missing=()
for p in "${packages[@]}"; do
    dpkg -s "$p" &>/dev/null || missing+=("$p")
done

if (( ${#missing[@]} )); then
    echo "Installing: ${missing[*]}"
    sudo apt-get update
    sudo apt-get install -y "${missing[@]}"
fi

if [[ ! -f build/build.ninja ]]; then
    cmake --preset release
fi
cmake --build --preset release

exec ./build/arm_ide "$@"
