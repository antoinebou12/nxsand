# Build NXSand.nro via the devkitPro msys2 shell on Windows.
# Defaults to the standard devkitPro install location; override with $env:DEVKITPRO.
$ErrorActionPreference = 'Stop'

$dkp = if ($env:DEVKITPRO) { $env:DEVKITPRO } else { 'C:\devkitPro' }
if (-not (Test-Path $dkp) -and (Test-Path 'C:\devkitPro')) {
    $dkp = 'C:\devkitPro'
}
if (-not (Test-Path $dkp)) {
    Write-Error "devkitPro not found at $dkp. Install from https://devkitpro.org/ or set `$env:DEVKITPRO to C:\devkitPro."
}
$env:DEVKITPRO = $dkp

$msys2 = Join-Path $dkp 'msys2\usr\bin\bash.exe'
if (-not (Test-Path $msys2)) {
    Write-Error "msys2 bash not found at $msys2. Reinstall devkitPro with the MSYS2 component."
}

# Translate Windows path to msys (/c/Users/.../NXEngine)
$proj = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'gen_icon.ps1')

$drive = ($proj.Substring(0,1)).ToLower()
$rest  = $proj.Substring(2).Replace('\','/')
$mproj = "/$drive$rest"

# Use --noprofile --norc to skip the user's interactive rc files (e.g. ~/.bashrc),
# which can emit stderr noise that PowerShell mis-treats as a fatal NativeCommandError.
# /etc/profile is sourced explicitly to pick up devkitPro env (DEVKITPRO, PATH, etc.).
& $msys2 --noprofile --norc -c "source /etc/profile && cd '$mproj' && make $args"
$makeExit = $LASTEXITCODE
if ($makeExit -ne 0) {
    Write-Error "Switch build failed (make exit $makeExit)."
}
$nro = Join-Path $proj 'build\NXSand.nro'
if (-not (Test-Path $nro)) {
    Write-Error "Build succeeded but missing: $nro"
}
$distSwitch = Join-Path $proj 'dist\switch'
New-Item -ItemType Directory -Force -Path $distSwitch | Out-Null
Copy-Item -Force $nro (Join-Path $distSwitch 'NXSand.nro')

Write-Host "Output: $nro"
Write-Host "Staged: dist\switch\NXSand.nro"
& (Join-Path $PSScriptRoot 'verify-nro.ps1')
