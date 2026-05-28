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

$icon = Join-Path $proj 'romfs\icon.jpg'
$simFrag = Join-Path $proj 'romfs\shaders\sim.frag'
$paintFrag = Join-Path $proj 'romfs\shaders\paint.frag'
$paletteFrag = Join-Path $proj 'romfs\shaders\palette_lookup.frag'
$simCommon = Join-Path $proj 'romfs\shaders\sim_common.glsl'
$simIds = Join-Path $proj 'romfs\shaders\sim_ids.glsl'
if (-not (Test-Path $icon)) {
    Write-Error "romfs\icon.jpg missing - run: powershell -File scripts\gen_icon.ps1"
}
if (-not (Test-Path $simFrag)) {
    Write-Error "romfs\shaders\sim.frag missing - run: make (prepare_romfs)"
}
if (-not (Test-Path $paintFrag)) {
    Write-Error "romfs\shaders\paint.frag missing - run: make (prepare_romfs)"
}
if (-not (Test-Path $paletteFrag)) {
    Write-Error "romfs\shaders\palette_lookup.frag missing - run: make (prepare_romfs)"
}
if (-not (Test-Path $simCommon)) {
    Write-Error "romfs\shaders\sim_common.glsl missing - run: make (prepare_romfs)"
}
if (-not (Test-Path $simIds)) {
    Write-Error "romfs\shaders\sim_ids.glsl missing - run: make (prepare_romfs)"
}

$iconInfo = Get-Item $icon
if ($iconInfo.Length -lt 500) {
    Write-Error "romfs\icon.jpg looks corrupt ($($iconInfo.Length) bytes)."
}

# Embedded romfs (shader sources) and NACP title must be inside the NRO.
if (-not (Test-BinaryContainsAscii $bytes 'GLES 3.0 fragment Margolus pass')) {
    Write-Error @"
NRO is missing the GPU sim shader in romfs (sim.frag).
Rebuild so elf2nro gets: --romfsdir=<project>/romfs
You should see shaders/sim.frag copied during make (prepare_romfs).
"@
}

if (-not (Test-BinaryContainsAscii $bytes 'Shared Margolus CA rules')) {
    Write-Error "NRO is missing romfs shader include sim_common.glsl. Rebuild prepare_romfs."
}

if (-not (Test-BinaryContainsAscii $bytes 'NXSand')) {
    Write-Error "NRO is missing embedded NACP title (NXSand). Rebuild with --nacp=build/NXSand.nacp."
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
