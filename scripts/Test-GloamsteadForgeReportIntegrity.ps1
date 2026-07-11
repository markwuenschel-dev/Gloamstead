#requires -Version 7
# Report integrity: provenance + freshness of LIVE emitted reports + scenario-matrix consistency.
# The trust anchor is the per-gate-run nonce: gate.ps1 generates it, the emitter stamps every report and
# the run manifest, and this validator rejects any report whose nonce does not match the run (or that is not
# in the run manifest). A hand-authored report cannot know the fresh nonce, so a fabricated "success"
# dropped into the reports dir is rejected even if every semantic field is internally consistent.
[CmdletBinding()]
param(
    [string]$ReportsDir,
    [string]$Matrix,
    [string]$ExpectedCommit,
    [string]$ExpectedNonce,   # supplied by gate.ps1 for the current run; enforced when present
    [switch]$Strict
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'GloamsteadForge.Common.ps1')
$RepoRoot = Split-Path $PSScriptRoot -Parent
if (-not $ReportsDir) { $ReportsDir = Join-Path $RepoRoot 'procedural/reports/gloamsteadforge' }
if (-not $Matrix)     { $Matrix     = Join-Path $RepoRoot 'specs/gloamsteadforge/scenario_matrix.json' }
if (-not $ExpectedCommit) {
    $ExpectedCommit = (git -C $RepoRoot rev-parse HEAD 2>$null)
    if ($ExpectedCommit) { $ExpectedCommit = $ExpectedCommit.Trim() }
}

$fail = 0
$ManifestPath = Join-Path $ReportsDir '_run_manifest.json'

# --- Run manifest is mandatory: it is the provenance anchor ---
if (-not (Test-Path -PathType Leaf $ManifestPath)) {
    Write-Host "INTEGRITY: no run manifest at $ManifestPath (run gate.ps1 to emit) -> GF070" -ForegroundColor Red
    if ($Strict) { exit 1 } else { exit 0 }
}
$manifest = Get-Content -Raw -LiteralPath $ManifestPath | ConvertFrom-Json
$runNonce = [string]$manifest.run_nonce
if ([string]::IsNullOrWhiteSpace($runNonce)) { $fail++; Write-Host "  FAIL: manifest has no run_nonce (GF070)" -ForegroundColor Red }
if ($ExpectedNonce -and $runNonce -ne $ExpectedNonce) {
    $fail++; Write-Host "  FAIL: manifest nonce != gate run nonce (GF070) — stale/forged manifest" -ForegroundColor Red
}
$manifestScenarios = @($manifest.scenarios)

# --- Every live report must belong to the run (nonce match) and be in the manifest set ---
$files = @(Get-ChildItem -LiteralPath $ReportsDir -Filter *.json -File | Where-Object { $_.Name -ne '_run_manifest.json' })
if ($files.Count -eq 0) {
    Write-Host "INTEGRITY: no live reports under $ReportsDir" -ForegroundColor Yellow
    if ($Strict) { exit 1 } else { exit 0 }
}

function Get-Prop($obj, [string]$name) {
    if ($obj.PSObject.Properties.Name -contains $name) { return $obj.$name }
    return $null
}

$presentIds = @()
foreach ($f in $files) {
    $codes = @()
    $sid = ''
    try {
        $r = Get-GFReport -Path $f.FullName
        $sid = [string](Get-Prop $r 'scenario_id')
        $codes = @(Get-GFIntegrityCodes -R $r -ExpectedCommit $ExpectedCommit)

        $reportNonce = [string](Get-Prop $r 'run_nonce')
        if ([string]::IsNullOrWhiteSpace($reportNonce) -or $reportNonce -ne $runNonce) {
            $codes += 'GF070'  # report not stamped by this run (forged / detached / hand-authored)
        }
        if ($sid -notin $manifestScenarios) {
            $codes += 'GF068'  # report present but not declared by the run manifest (unexpected/extra)
        }
    } catch {
        $codes = @('GF002')  # malformed report -> fail closed, keep scanning
    }
    if ($sid) { $presentIds += $sid }

    if ($codes.Count -eq 0) { Write-Host "  PASS integrity: $($f.Name)" -ForegroundColor Green }
    else { $fail++; Write-Host "  FAIL integrity: $($f.Name) -> $($codes -join ', ')" -ForegroundColor Red }
}

# --- Manifest completeness: every declared scenario has a present report ---
foreach ($sid in $manifestScenarios) {
    if ($sid -notin $presentIds) { $fail++; Write-Host "  FAIL: manifest scenario '$sid' has no report (GF068)" -ForegroundColor Red }
}

# --- Matrix consistency: every matrix scenario emitted, code-free (matrix-bound), expected outcome ---
if (Test-Path -PathType Leaf $Matrix) {
    $m = Get-Content -Raw -LiteralPath $Matrix | ConvertFrom-Json
    $seen = @{}
    foreach ($s in $m.scenarios) {
        if ($seen.ContainsKey($s.scenario_id)) { $fail++; Write-Host "  FAIL matrix: duplicate scenario_id '$($s.scenario_id)' (GF071)" -ForegroundColor Red }
        $seen[$s.scenario_id] = $true
        $rp = Join-Path $RepoRoot $s.report
        if (-not (Test-Path -PathType Leaf $rp)) {
            $fail++; Write-Host "  FAIL matrix: '$($s.scenario_id)' report missing $($s.report) (GF068)" -ForegroundColor Red
            continue
        }
        $rr = Get-GFReport -Path $rp
        $sem = @(Get-GFCodes -R $rr -Scenario $s)
        if ($sem.Count -gt 0) { $fail++; Write-Host "  FAIL matrix: '$($s.scenario_id)' has failure codes $($sem -join ',') (GF069)" -ForegroundColor Red }
        if ($rr.night_loop.outcome_result -ne $s.expected_outcome) {
            $fail++; Write-Host "  FAIL matrix: '$($s.scenario_id)' outcome $($rr.night_loop.outcome_result) != expected $($s.expected_outcome)" -ForegroundColor Red
        }
    }
    Write-Host "INTEGRITY: matrix has $($m.scenarios.Count) scenario(s); manifest nonce present"
}

if ($fail -gt 0 -and $Strict) { exit 1 }
exit 0
