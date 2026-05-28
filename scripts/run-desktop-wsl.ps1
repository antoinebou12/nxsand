# Launch NXSand desktop via WSLg (fixes invisible-window COPY MODE when possible).
# Usage: powershell -File scripts/run-desktop-wsl.ps1
# Optional: -ShutdownFirst  runs wsl --shutdown before start (slower, often clears WSLg)
param([switch]$ShutdownFirst)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

if ($ShutdownFirst) {
    Write-Host 'Restarting WSL (wsl --shutdown)...'
    wsl --shutdown
    Start-Sleep -Seconds 2
}

$fix = @'
if ! mountpoint -q /mnt/shared_memory 2>/dev/null; then
  sudo mkdir -p /mnt/shared_memory 2>/dev/null || true
  sudo mount -t tmpfs tmpfs /mnt/shared_memory 2>/dev/null || true
fi
'@

$run = "cd '$($root -replace '\\','/')' && ./build/NXSand"
$cmd = "$fix`nif [ ! -x ./build/NXSand ]; then echo 'Missing build/NXSand. Run: bash scripts/build-desktop-wsl.sh'; exit 1; fi`n$run"

# Detached GUI: do not tie the window to this terminal (helps Cursor/WT copy-mode quirks).
Start-Process wsl.exe -ArgumentList @(
    '-e', 'bash', '-lc', $cmd
) -WorkingDirectory $root

Write-Host 'Started NXSand in a separate WSL window.'
Write-Host 'If the window is still blank, close it and re-run with -ShutdownFirst, or run: wsl --shutdown'
