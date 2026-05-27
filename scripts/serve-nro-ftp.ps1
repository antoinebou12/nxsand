# Build NXSand.nro, stage under dist/switch/, and serve via FTP for Switch homebrew clients.
# Requires: devkitPro MSYS2, Python 3 with pyftpdlib (`pip install pyftpdlib`).
$ErrorActionPreference = 'Stop'

$proj = Split-Path -Parent $PSScriptRoot
$nro = Join-Path $proj 'build\NXSand.nro'
$distSwitch = Join-Path $proj 'dist\switch'

& (Join-Path $PSScriptRoot 'build-native.ps1')

if (-not (Test-Path $nro)) {
    Write-Error "Build failed or missing: $nro"
}

New-Item -ItemType Directory -Force -Path $distSwitch | Out-Null
Copy-Item -Force $nro (Join-Path $distSwitch 'NXSand.nro')

$py = Get-Command python -ErrorAction SilentlyContinue
if (-not $py) {
    $py = Get-Command py -ErrorAction SilentlyContinue
}
if (-not $py) {
    Write-Error "Python not found. Install Python 3, then: pip install pyftpdlib"
}

& $py.Source -c "import pyftpdlib" 2>$null
if ($LASTEXITCODE -ne 0) {
    Write-Error "pyftpdlib not installed. Run: pip install pyftpdlib"
}

$port = if ($env:NXSAND_FTP_PORT) { $env:NXSAND_FTP_PORT } elseif ($env:NXENGINE_FTP_PORT) { $env:NXENGINE_FTP_PORT } else { 5000 }
$distRoot = Join-Path $proj 'dist'

$ip = '<your-pc-lan-ip>'
try {
    $ip = (Get-NetIPAddress -AddressFamily IPv4 -ErrorAction Stop |
        Where-Object { $_.IPAddress -notlike '127.*' -and $_.PrefixOrigin -ne 'WellKnown' } |
        Select-Object -First 1).IPAddress
} catch {
    # Fallback if Get-NetIPAddress unavailable
}
if (-not $ip) { $ip = '<your-pc-lan-ip>' }

Write-Host ""
Write-Host "Staged: dist\switch\NXSand.nro"
Write-Host "FTP root: $distRoot (port $port)"
Write-Host "URL:      ftp://${ip}:${port}/switch/NXSand.nro"
Write-Host ""
Write-Host "On Switch (FTP client, same LAN):"
Write-Host "  1. Connect to ftp://${ip}:${port}/"
Write-Host "  2. Upload switch/NXSand.nro -> sdmc:/switch/NXSand.nro"
Write-Host "     (saves go in sdmc:/switch/nxsand/ - do NOT put the .nro there)"
Write-Host "Press Ctrl+C to stop the server."
Write-Host ""

Set-Location $distRoot
& $py.Source -m pyftpdlib -p $port -w .
