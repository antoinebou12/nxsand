# Homebrew icon for hbmenu: 256x256 JPEG (devkitPro / elf2nro --icon).
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$src = Join-Path $root 'data\icon-source.png'
$out = Join-Path $root 'romfs\icon.jpg'
New-Item -ItemType Directory -Force -Path (Split-Path $out) | Out-Null

if (-not (Test-Path $src)) {
    Write-Error "Missing $src - add a 256x256 PNG, then run this script again."
}

Add-Type -AssemblyName System.Drawing
$loaded = [System.Drawing.Image]::FromFile((Resolve-Path $src))
try {
    if ($loaded.Width -ne 256 -or $loaded.Height -ne 256) {
        Write-Warning "icon-source.png is $($loaded.Width)x$($loaded.Height); scaling to 256x256."
    }
    $bmp = New-Object System.Drawing.Bitmap 256, 256
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $g.DrawImage($loaded, 0, 0, 256, 256)
    $g.Dispose()

    $enc = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
    $ep = New-Object System.Drawing.Imaging.EncoderParameters 1
    $ep.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality, 94L)
    $bmp.Save($out, $enc, $ep)
    $bmp.Dispose()
    $ep.Dispose()
}
finally {
    $loaded.Dispose()
}

Write-Host "Wrote $out (256x256 JPEG from data\icon-source.png)"
