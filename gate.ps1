#requires -Version 7
[CmdletBinding()]
param(
    # Engine root (contains Engine\Build\BatchFiles\Build.bat). Overrides auto-detection.
    [string]$Engine,
    # .uproject path. Defaults to the project next to this script.
    [string]$Proj
)
$ErrorActionPreference = 'Stop'

function Fail($m) { Write-Host "GATE FAIL: $m" -ForegroundColor Red; exit 1 }

# Portable paths: resolve the project relative to this script (repo root) instead of a
# hardcoded absolute. A moved/renamed checkout no longer silently breaks the gate.
$RepoRoot = $PSScriptRoot
if (-not $Proj) { $Proj = Join-Path $RepoRoot 'Gloamstead5_8.uproject' }
if (-not (Test-Path $Proj)) { Fail "project not found: $Proj (pass -Proj or run from the repo root)" }

$Target = 'GloamsteadEditor'
$Filter = 'Gloamstead'
$Report = Join-Path $env:TEMP 'GloamsteadGate'

# Resolve the engine for this .uproject's EngineAssociation, portable across install
# locations: -Engine arg > GLOAMSTEAD_UE_ENGINE env > registry (launcher/custom builds
# register InstalledDirectory per version) > common install roots. This machine's 5.8
# lives at D:\UE_5.8, not the default C:\Program Files\Epic Games\UE_5.8.
if (-not $Engine) { $Engine = $env:GLOAMSTEAD_UE_ENGINE }
if (-not $Engine) {
    $assoc = (Get-Content $Proj -Raw | ConvertFrom-Json).EngineAssociation
    $candidates = @()
    foreach ($hive in @(
        "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$assoc",
        "HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\$assoc"
    )) {
        try {
            $d = (Get-ItemProperty -Path $hive -ErrorAction Stop).InstalledDirectory
            if ($d) { $candidates += $d }
        } catch { }
    }
    $candidates += @("D:\UE_$assoc", "C:\Program Files\Epic Games\UE_$assoc")
    $Engine = $candidates |
        Where-Object { $_ -and (Test-Path (Join-Path $_ 'Engine\Build\BatchFiles\Build.bat')) } |
        Select-Object -First 1
}
if (-not $Engine -or -not (Test-Path (Join-Path $Engine 'Engine\Build\BatchFiles\Build.bat'))) {
    Fail "engine not found for this .uproject. Pass -Engine <root> or set `$env:GLOAMSTEAD_UE_ENGINE (tried registry + D:\UE_<ver> + Program Files)."
}
Write-Host "GATE: engine=$Engine" -ForegroundColor Cyan
Write-Host "GATE: proj=$Proj"     -ForegroundColor Cyan

# 1. Build - exit code IS the oracle here.
#    -MaxParallelActions=6 keeps UBA from kill-looping on RAM with the heavy NeoStackAI TUs.
#    Requires the editor/game to be closed: it links UnrealEditor-Gloamstead.dll, which a running
#    editor holds open (LNK1104 otherwise).
& "$Engine\Engine\Build\BatchFiles\Build.bat" $Target Win64 Development -Project="$Proj" -WaitMutex -MaxParallelActions=6
if ($LASTEXITCODE -ne 0) { Fail "build returned $LASTEXITCODE" }

# 2. Tests - editor-cmd exit code is unreliable; parse the report, fail closed.
if (Test-Path $Report) { Remove-Item $Report -Recurse -Force }
& "$Engine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $Proj -ExecCmds="Automation RunTests $Filter; Quit" -unattended -nullrhi -nosplash -nopause -ReportExportPath="$Report" | Out-Null

$index = Join-Path $Report 'index.json'
if (-not (Test-Path $index)) { Fail "no report written - runner never produced results" }

$r     = Get-Content $index -Raw | ConvertFrom-Json
$tests = @($r.tests)
if ($tests.Count -eq 0) { Fail "report has zero tests - filter matched nothing" }

$bad = $tests | Where-Object { $_.state -ne 'Success' }
if ($bad) {
    $bad | ForEach-Object { Write-Host "  FAILED: $($_.fullTestPath) [$($_.state)]" -ForegroundColor Red }
    Fail "$($bad.Count)/$($tests.Count) test(s) not green"
}

Write-Host "GATE PASS: build green, $($tests.Count) test(s) green" -ForegroundColor Green
exit 0
