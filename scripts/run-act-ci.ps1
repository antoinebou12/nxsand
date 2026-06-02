# Run GitHub Actions locally via act (Docker required).
# Usage:
#   powershell -File scripts/run-act-ci.ps1
#   powershell -File scripts/run-act-ci.ps1 -Workflow release -Tag v0.0.1
#   powershell -File scripts/run-act-ci.ps1 -Workflow release -Job build-linux

param(
    [ValidateSet("ci", "release")]
    [string] $Workflow = "ci",
    [ValidateSet("all", "unit-tests", "switch", "desktop", "desktop-windows", "build-switch", "build-linux", "build-windows", "publish")]
    [string] $Job = "all",
    [string] $Tag = "v0.0.1",
    [switch] $Prerelease
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

$networkName = "act-network"
if (-not (docker network ls --format "{{.Name}}" | Select-String -Pattern "^$networkName$" -Quiet)) {
    Write-Host "Creating docker network $networkName ..."
    docker network create $networkName | Out-Null
}

$actArgs = @(
    "--container-architecture", "linux/amd64",
    "--network", $networkName
)

if ($Workflow -eq "release") {
    $eventFile = Join-Path $PWD ".github/act/release-event.json"
    if (-not (Test-Path $eventFile)) {
        Write-Error "Missing $eventFile"
    }
    $event = Get-Content $eventFile -Raw | ConvertFrom-Json
    $event.inputs.tag = $Tag
    $event.inputs.prerelease = if ($Prerelease) { "true" } else { "false" }
    $json = $event | ConvertTo-Json -Depth 4 -Compress
    [System.IO.File]::WriteAllText($eventFile, $json, (New-Object System.Text.UTF8Encoding $false))

    $actArgs += @(
        "workflow_dispatch",
        "-W", ".github/workflows/release.yml",
        "-e", $eventFile,
        "--input", "tag=$Tag",
        "--input", "prerelease=$(if ($Prerelease) { 'true' } else { 'false' })"
    )
    if ($Job -eq "all") {
        $actArgs += @("-j", "unit-tests", "-j", "build-switch", "-j", "build-linux")
    } elseif ($Job -ne "publish" -and $Job -ne "build-windows" -and $Job -ne "desktop-windows") {
        $actArgs += @("-j", $Job)
    } else {
        Write-Error "Job '$Job' is not runnable under act in Docker. Use GitHub Actions (windows-latest) or run scripts/package-release.py locally."
    }
} else {
    $actArgs += @(
        "push",
        "-W", ".github/workflows/native-nro.yml"
    )
    if ($Job -ne "all") {
        if ($Job -eq "build-switch" -or $Job -eq "build-linux" -or $Job -eq "publish") {
            Write-Error "Job '$Job' belongs to the release workflow. Use -Workflow release."
        }
        $actArgs += @("-j", $Job)
    }
}

Write-Host "Running: act $($actArgs -join ' ')"
if ($Workflow -eq "release") {
    Write-Host "Release act runs unit-tests + build-switch + build-linux in Docker."
    Write-Host "publish and build-windows are skipped (ACT=true / no Windows runner)."
}
& act @actArgs
exit $LASTEXITCODE
