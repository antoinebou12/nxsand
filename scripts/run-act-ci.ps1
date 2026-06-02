# Run GitHub Actions workflow locally via act (Docker required).
# Usage: powershell -File scripts/run-act-ci.ps1 [-Job unit-tests|switch|desktop]

param(
    [ValidateSet("all", "unit-tests", "switch", "desktop")]
    [string] $Job = "all"
)

$ErrorActionPreference = "Stop"
Set-Location (Split-Path $PSScriptRoot -Parent)

if (-not (Get-Command act -ErrorAction SilentlyContinue)) {
    Write-Error "act not found. Install: winget install nektos.act"
}

docker info 2>$null | Out-Null
if ($LASTEXITCODE -ne 0) {
    Write-Error "Docker is not running. Start Docker Desktop, then retry."
}

$actArgs = @(
    "push",
    "-W", ".github/workflows/native-nro.yml",
    "--container-architecture", "linux/amd64"
)

if ($Job -ne "all") {
    $actArgs += @("-j", $Job)
}

Write-Host "Running: act $($actArgs -join ' ')"
& act @actArgs
exit $LASTEXITCODE
