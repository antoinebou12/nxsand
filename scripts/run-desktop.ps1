# Run NXSand desktop (repo root cwd; ANGLE DLLs from build/ or MSYS2).
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $root "build\NXSand.exe"
if (-not (Test-Path $exe)) {
    Write-Error "Missing $exe. Run: powershell -File scripts/build-desktop.ps1"
}
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
Set-Location $root
Start-Process -FilePath $exe -WorkingDirectory $root
