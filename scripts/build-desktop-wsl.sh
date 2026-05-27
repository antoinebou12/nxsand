#!/usr/bin/env bash
# Desktop build inside WSL (Ubuntu/Debian). From repo root:
#   bash scripts/build-desktop-wsl.sh
set -euo pipefail
cd "$(dirname "$0")/.."

need_pkg() {
  pkg-config --exists "$1" 2>/dev/null
}

if ! need_pkg sdl2 || ! need_pkg glesv2 || ! need_pkg freetype2; then
  echo "Installing desktop build deps (needs sudo once)..."
  sudo apt-get update -y
  sudo DEBIAN_FRONTEND=noninteractive apt-get install -y \
    build-essential pkg-config \
    libsdl2-dev libgles2-mesa-dev libegl1-mesa-dev libfreetype6-dev
fi

mkdir -p build
make desktop
echo "OK: $(pwd)/build/NXSand"
