# Download Noto Sans Regular (OFL) into romfs for hbmenu/game UI text.
# License: https://openfontlicense.org/ — see third_party/fonts/OFL-NotoSans.txt
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $root 'romfs\fonts'
$out = Join-Path $outDir 'NotoSans-Regular.ttf'
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

if (Test-Path $out) {
    $len = (Get-Item $out).Length
    if ($len -gt 100000) {
        Write-Host "Font already present: $out ($len bytes)"
        exit 0
    }
}

$url = 'https://github.com/googlefonts/noto-fonts/raw/main/hinted/ttf/NotoSans/NotoSans-Regular.ttf'
Write-Host "Downloading Noto Sans Regular from $url"
Invoke-WebRequest -Uri $url -OutFile $out -UseBasicParsing

if (-not (Test-Path $out) -or (Get-Item $out).Length -lt 100000) {
    Write-Error "Download failed or file too small: $out"
}
Write-Host "Wrote $out ($((Get-Item $out).Length) bytes)"
