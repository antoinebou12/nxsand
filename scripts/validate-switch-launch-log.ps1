# Validate the latest NXSand launch block in sdmc launch.log (or a local copy).
# Usage:
#   powershell -File scripts\validate-switch-launch-log.ps1
#   powershell -File scripts\validate-switch-launch-log.ps1 -Path D:\nxsand_launch.log
param(
    [string]$Path = ""
)

$ErrorActionPreference = 'Stop'

if (-not $Path) {
    $candidates = @(
        (Join-Path $PSScriptRoot '..\nxsand_save\launch.log'),
        (Join-Path $env:USERPROFILE 'Desktop\launch.log')
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { $Path = (Resolve-Path $c).Path; break }
    }
}

if (-not $Path -or -not (Test-Path $Path)) {
    Write-Error "No launch.log found. Copy sdmc:/switch/nxsand/launch.log locally or pass -Path."
}

$text = Get-Content -Path $Path -Raw
$blocks = [regex]::Split($text, '(?=---- NXSand launch ----)')
$latest = ($blocks | Where-Object { $_.Trim().Length -gt 0 } | Select-Object -Last 1)
if (-not $latest) {
    Write-Error "launch.log has no launch blocks."
}

function Test-BlockContains([string]$needle) {
    return $latest.Contains($needle)
}

$checks = [ordered]@{
    'switch-sim-log-v11' = Test-BlockContains 'switch-sim-log-v11'
    'saved_backend' = Test-BlockContains 'switch: saved sim backend='
    'sim_shader_path' = ($latest -match 'sim shader path: shaders/sim\.(frag|comp)')
    'romfs_rules_body' = Test-BlockContains 'sim_rules_body.glsl'
    'boot_sim_deferred' = Test-BlockContains 'boot sim deferred'
    'sim_compile_ok' = Test-BlockContains 'sim compile ok'
    'link_ok_sim_shader' = ($latest -match 'link ok: sim\.(frag|comp)')
    'sim_step_backend' = ($latest -match 'sim step ok backend=(Fragment|Compute)')
}

$fail = 0
Write-Host "Validating latest launch block in: $Path"
Write-Host ""
foreach ($kv in $checks.GetEnumerator()) {
    $ok = $kv.Value
    $mark = if ($ok) { 'OK' } else { 'MISSING' }
    if (-not $ok) { $fail++ }
    Write-Host ("  [{0}] {1}" -f $mark, $kv.Key)
}

if ($latest -match 'sim_switch') {
    Write-Host ""
    Write-Host "  [WARN] log still references sim_switch - deploy a current NRO"
}

Write-Host ""
if ($fail -eq 0) {
    Write-Host "All checks passed."
    exit 0
}

if (-not $checks['link_ok_sim_shader']) {
    Write-Host "Hint: first New Sandbox - wait for link ok: sim.frag or sim.comp (heartbeats every ~15s; do not power off)."
}
if (-not $checks['sim_step_backend']) {
    Write-Host "Hint: after sim compile ok, paint sand once to log sim step ok backend=Fragment or Compute."
}
exit 1
