# Homebrew icon for hbmenu: 256x256 JPEG (devkitPro / elf2nro --icon).
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$defaultSrc = Join-Path $root 'assets\nxsand-icon.png'
$fallbackSrc = Join-Path $root 'data\icon-source.png'
$src = if (Test-Path $defaultSrc) { $defaultSrc } elseif (Test-Path $fallbackSrc) { $fallbackSrc } else { $defaultSrc }
$out = Join-Path $root 'romfs\icon.jpg'
$dataOut = Join-Path $root 'data\icon-source.png'
New-Item -ItemType Directory -Force -Path (Split-Path $out) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path $dataOut) | Out-Null

if (-not (Test-Path $src)) {
    Write-Error "Missing icon source. Add assets\nxsand-icon.png or data\icon-source.png (256x256 PNG preferred)."
}

Add-Type -AssemblyName System.Drawing

function New-LetterboxBitmap([System.Drawing.Image]$loaded) {
    $size = 256
    $padR = 0xE8; $padG = 0xC8; $padB = 0x90
    $bmp = New-Object System.Drawing.Bitmap $size, $size
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.Clear([System.Drawing.Color]::FromArgb(255, $padR, $padG, $padB))
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality

    $scale = [Math]::Min($size / $loaded.Width, $size / $loaded.Height)
    $drawW = [int][Math]::Round($loaded.Width * $scale)
    $drawH = [int][Math]::Round($loaded.Height * $scale)
    $drawX = [int](($size - $drawW) / 2)
    $drawY = [int](($size - $drawH) / 2)
    if ($loaded.Width -ne $loaded.Height -and ($drawW -eq $size -and $drawH -eq $size)) {
        Write-Error "Icon source would require non-uniform stretch; use square art or wider letterbox canvas."
    }
    $g.DrawImage($loaded, $drawX, $drawY, $drawW, $drawH)
    $g.Dispose()
    return $bmp
}

$loaded = [System.Drawing.Image]::FromFile((Resolve-Path $src))
try {
    if ($loaded.Width -ne 256 -or $loaded.Height -ne 256) {
        Write-Warning "Letterboxing $($loaded.Width)x$($loaded.Height) -> 256x256 (no stretch)."
    }
    $bmp = New-LetterboxBitmap $loaded

    $enc = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
    $ep = New-Object System.Drawing.Imaging.EncoderParameters 1
    $ep.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter ([System.Drawing.Imaging.Encoder]::Quality, 94L)
    $bmp.Save($out, $enc, $ep)
    $bmp.Save($dataOut, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    $ep.Dispose()
}
finally {
    $loaded.Dispose()
}

Write-Host "Wrote $out and $dataOut (256x256 letterboxed from $src)"
$cropPy = Join-Path $root 'scripts\crop_icon.py'
if (Test-Path $cropPy) {
    & python $cropPy $out | Write-Host
}
