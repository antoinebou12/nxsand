# Build NXSand desktop binary on Windows (SDL2 + OpenGL ES 3.0).
# Switch (.nro) remains the primary verified target; use devkitPro for that.
param(
    [string]$MsysBash = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
& (Join-Path $PSScriptRoot 'fetch-font.ps1')

function Find-MsysBash {
    $candidates = @(
        $env:MSYSTEM_BASH,
        "C:\msys64\usr\bin\bash.exe",
        "C:\devkitPro\msys2\usr\bin\bash.exe"
    ) | Where-Object { $_ -and (Test-Path $_) }
    if ($candidates.Count -gt 0) { return $candidates[0] }
    return $null
}

if (-not $MsysBash) { $MsysBash = Find-MsysBash }

if ($MsysBash) {
    $msysScript = Join-Path $PSScriptRoot 'build-desktop-msys.sh'
    Write-Host "Building desktop via MSYS2 bash: $MsysBash"
    if (Test-Path $msysScript) {
        & $MsysBash $msysScript
    } else {
        & $MsysBash -lc "cd '$($root -replace '\\','/')' && make desktop"
    }
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $exe = Join-Path $root 'build\NXSand.exe'
    if (Test-Path $exe) { Write-Host "OK: $exe" } else { Write-Host "OK: $root\build\NXSand" }
    exit 0
}

Write-Host @"
Desktop build on Windows needs SDL2 + GLESv2 (OpenGL ES 3.0).

Recommended (MSYS2 / devkitPro shell):
  pacman -S mingw-w64-x86_64-SDL2 mingw-w64-x86_64-mesa
  make desktop

Alternative - ANGLE via vcpkg:
  vcpkg install sdl2 angle
  set PKG_CONFIG_PATH=<vcpkg>\installed\x64-windows\lib\pkgconfig
  make desktop

ANGLE loads GLES on top of D3D11; Mesa provides libGLESv2 on Linux.
The Switch build (make in devkitPro) does not need ANGLE.

Re-run this script after installing MSYS2, or pass -MsysBash path\to\bash.exe
"@
exit 1
