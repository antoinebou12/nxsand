# Validate NXSand.nro before copying to the Switch SD card.
$ErrorActionPreference = 'Stop'

$proj = Split-Path -Parent $PSScriptRoot
$nro = Join-Path $proj 'build\NXSand.nro'
$nacp = Join-Path $proj 'build\NXSand.nacp'

if (-not (Test-Path $nro)) {
    Write-Error "Missing $nro - run scripts\build-native.ps1 or make in devkitPro shell."
}

$bytes = [System.IO.File]::ReadAllBytes($nro)
$size = $bytes.Length
if ($size -lt 500000) {
    Write-Error "NRO too small ($size bytes) - build may have failed."
}

$magic = [System.Text.Encoding]::ASCII.GetString($bytes, 0x10, 4)
if ($magic -ne 'NRO0') {
    Write-Error "Invalid NRO header (expected NRO0 at 0x10, got '$magic')."
}

function Test-BinaryContainsAscii {
    param(
        [byte[]]$Haystack,
        [string]$Needle
    )
    $pattern = [System.Text.Encoding]::ASCII.GetBytes($Needle)
    if ($pattern.Length -eq 0 -or $Haystack.Length -lt $pattern.Length) {
        return $false
    }
    for ($i = 0; $i -le $Haystack.Length - $pattern.Length; $i++) {
        $ok = $true
        for ($j = 0; $j -lt $pattern.Length; $j++) {
            if ($Haystack[$i + $j] -ne $pattern[$j]) {
                $ok = $false
                break
            }
        }
        if ($ok) { return $true }
    }
    return $false
}

$requiredRomfsShaders = @(
    'sim.frag',
    'sim.comp',
    'sim_rules_body.glsl',
    'sim_common.glsl',
    'sim_ids.glsl',
    'paint.frag',
    'palette_lookup.frag',
    'upscale.frag',
    'fullscreen.vert',
    'bloom_bright.frag',
    'bloom_blur.frag',
    'bloom_composite.frag',
    'ui_quad.vert',
    'ui_quad.frag'
)
foreach ($name in $requiredRomfsShaders) {
    $path = Join-Path $proj "romfs\shaders\$name"
    if (-not (Test-Path $path)) {
        Write-Error "romfs\shaders\$name missing - run: make (prepare_romfs)"
    }
}

$icon = Join-Path $proj 'romfs\icon.jpg'
if (-not (Test-Path $icon)) {
    Write-Error "romfs\icon.jpg missing - run: powershell -File scripts\gen_icon.ps1"
}
$iconInfo = Get-Item $icon
if ($iconInfo.Length -lt 500) {
    Write-Error "romfs\icon.jpg looks corrupt ($($iconInfo.Length) bytes)."
}

# Embedded romfs (shader sources) and NACP title must be inside the NRO.
if (-not (Test-BinaryContainsAscii $bytes 'fragment Margolus pass')) {
    Write-Error @"
NRO is missing the GPU sim shader in romfs (sim.frag).
Rebuild so elf2nro gets: --romfsdir=<project>/romfs
You should see shaders/sim.frag copied during make (prepare_romfs).
"@
}

if (-not (Test-BinaryContainsAscii $bytes 'Shared Margolus CA rules')) {
    Write-Error "NRO is missing romfs shader include sim_common.glsl. Rebuild prepare_romfs."
}

if (-not (Test-BinaryContainsAscii $bytes 'GLES 3.1+ compute Margolus pass')) {
    Write-Error "NRO is missing romfs shader sim.comp. Rebuild prepare_romfs."
}

if (-not (Test-BinaryContainsAscii $bytes 'uniform int        uPaletteMode')) {
    Write-Error "NRO is missing romfs shader palette_lookup.frag. Rebuild prepare_romfs."
}

if (-not (Test-BinaryContainsAscii $bytes 'NXSand')) {
    Write-Error "NRO is missing embedded NACP title (NXSand). Rebuild with --nacp=build/NXSand.nacp."
}

if (-not (Test-BinaryContainsAscii $bytes 'switch-sim-log-v11')) {
    Write-Error @"
NRO is missing the current Switch diagnostic marker: switch-sim-log-v11.
This usually means build\NXSand.nro is stale. Rebuild with make, then verify launch.log shows:
  build: switch-sim-log-v11 ...
"@
}
Write-Host "    Sim compile diagnostics: found 'switch-sim-log-v11' in binary"

if (-not (Test-BinaryContainsAscii $bytes 'sim shader path:')) {
    Write-Error "NRO is missing Switch sim shader path diagnostics. Rebuild from current source."
}
if (-not (Test-BinaryContainsAscii $bytes 'boot sim deferred')) {
    Write-Error "NRO is missing Switch boot sim defer diagnostics. Rebuild from current source."
}

if (-not (Test-BinaryContainsAscii $bytes 'switch: saved sim backend=')) {
    Write-Error "NRO is missing Switch saved-backend boot marker. Rebuild from current source."
}

if (Test-BinaryContainsAscii $bytes 'preferring compute sim') {
    Write-Error "NRO contains stale 'preferring compute sim' string. Rebuild from current source."
}
if (Test-BinaryContainsAscii $bytes 'fragment fallback skipped on Switch') {
    Write-Error "NRO contains stale 'fragment fallback skipped' string. Rebuild from current source."
}

if (-not (Test-BinaryContainsAscii $bytes 'ensureSimPipeline')) {
    Write-Error "NRO is missing ensureSimPipeline diagnostics. Rebuild with the current source."
}

# JPEG SOI 0xFF 0xD8 (icon embedded via --icon)
$hasJpeg = $false
for ($i = 0; $i -lt $bytes.Length - 1; $i++) {
    if ($bytes[$i] -eq 0xFF -and $bytes[$i + 1] -eq 0xD8) {
        $hasJpeg = $true
        break
    }
}
if (-not $hasJpeg) {
    Write-Warning "JPEG icon marker not found in NRO; hbmenu may show a blank tile. Rebuild with --icon=romfs/icon.jpg."
}

if (-not (Test-Path $nacp)) {
    Write-Warning "build\NXSand.nacp missing (should be generated before elf2nro)."
}

Write-Host "OK: $nro"
Write-Host "    Size: $([math]::Round($size / 1MB, 2)) MB"
Write-Host "    Title/icon/shaders: present in binary"
Write-Host ""
Write-Host "Copy to microSD:"
Write-Host "    <SD>:/switch/NXSand.nro"
Write-Host ""
Write-Host "Saves only (not the .nro):"
Write-Host "    <SD>:/switch/nxsand/"
