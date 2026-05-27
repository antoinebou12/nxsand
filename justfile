# NXEngine — Windows + WSL helpers (`cargo install just` / https://github.com/casey/just)
# Run from anywhere; paths resolve from this file's directory.

set windows-shell := ["powershell.exe", "-NoProfile", "-Command"]

repo := justfile_directory()

# Default: show recipes
default:
    @just --list

# Build desktop (WSL) then Switch .nro (devkitPro MSYS2 / PowerShell)
[group('build')]
all: desktop nro

# SDL2 + GLES desktop binary via WSL (Ubuntu); see scripts/build-desktop-wsl.sh
[group('build')]
desktop:
    $w = (wsl wslpath -a "{{ repo }}").Trim(); if (-not $w) { throw 'wslpath failed — is WSL installed?' }; wsl -e bash -lc "cd `"$w`" && bash scripts/build-desktop-wsl.sh"

# Switch homebrew .nro (requires devkitPro + scripts/build-native.ps1)
[group('build')]
nro:
    Set-Location "{{ repo }}"; ./scripts/build-native.ps1

# CPU unit tests inside WSL (same Makefile target as CI)
[group('test')]
test:
    $w = (wsl wslpath -a "{{ repo }}").Trim(); if (-not $w) { throw 'wslpath failed' }; wsl -e bash -lc "cd `"$w`" && make test"

# Remove build/, dist/, and stray artifacts (uses Makefile clean in WSL)
[group('maintain')]
clean:
    $w = (wsl wslpath -a "{{ repo }}").Trim(); if (-not $w) { throw 'wslpath failed' }; wsl -e bash -lc "cd `"$w`" && make clean"
