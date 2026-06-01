#!/usr/bin/env bash
# One-shot desktop build in MSYS2 MinGW64 (SDL2 + ANGLE GLES + FreeType).
set -euo pipefail
cd "$(dirname "$0")/.."

export MSYSTEM=MINGW64
export PATH="/mingw64/bin:/usr/local/bin:/usr/bin:/bin"
export PKG_CONFIG_PATH="/mingw64/lib/pkgconfig:/mingw64/share/pkgconfig"

if ! pkg-config --exists sdl2 freetype2 angleproject; then
  echo "Missing packages. In MSYS2 MinGW64 terminal run:"
  echo "  pacman -S mingw-w64-x86_64-SDL2 mingw-w64-x86_64-freetype mingw-w64-x86_64-angleproject mingw-w64-x86_64-gcc mingw-w64-x86_64-make"
  exit 1
fi

FLAGS=$(pkg-config --cflags sdl2 freetype2 angleproject)
LIBS=$(pkg-config --libs sdl2 freetype2 angleproject)
# Console subsystem so init failures print to stderr when launched from a terminal.
LIBS=${LIBS//-mwindows/}
mkdir -p build
make desktop \
  DESKTOP_CXXFLAGS="-std=c++17 -O3 -Wall -Wno-missing-field-initializers -Isource -Ithird_party -Ithird_party/glad/include -DNX_DESKTOP=1 ${FLAGS}" \
  DESKTOP_LIBS="${LIBS}"
for dll in libEGL.dll libGLESv2.dll; do
  if [[ -f "/mingw64/bin/${dll}" ]]; then
    cp -f "/mingw64/bin/${dll}" build/
  fi
done
echo "OK: $(pwd)/build/NXSand.exe"
