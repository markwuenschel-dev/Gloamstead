#requires -Version 7
# Report integrity: freshness/provenance of LIVE emitted reports + scenario-matrix consistency.
# Live reports must carry the current repo HEAD (stale evidence fails closed).
[CmdletBinding()]
param(
    [string]$ReportsDir,
    [string]$Matrix,
    [string]$ExpectedCommit,
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

$files = @()
if (Test-Path -PathType Container $ReportsDir) { $files = @(Get-ChildItem -LiteralPath $ReportsDir -Filter *.json -File) }
if ($files.Count -eq 0) {
    Write-Host "INTEGRITY: no live reports under $ReportsDir (run gate.ps1 to emit them)" -ForegroundColor Yellow
    if ($Strict) { exit 1 } else { exit 0 }
}

foreach ($f in $files) {
    $r = Get-GFReport -Path $f.FullName
    $codes = @(Get-GFIntegrityCodes -R $r -ExpectedCommit $ExpectedCommit)
    if ($codes.Count -eq 0) { Write-Host "  PASS integrity: $($f.Name)" -ForegroundColor Green }
    else { $fail++; Write-Host "  FAIL integrity: $($f.Name) -> $($codes -join ', ')" -ForegroundColor Red }
}

# Matrix consistency: every declared scenario must have a fresh, code-free report.
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
        if ($sem.Count -gt 0) { $fail++; Write-Host "  FAIL matrix: '$($s.scenario_id)' report has failure codes $($sem -join ',') (GF069)" -ForegroundColor Red }
        if ($rr.night_loop.outcome_result -ne $s.expected_outcome) {
            $fail++; Write-Host "  FAIL matrix: '$($s.scenario_id)' outcome $($rr.night_loop.outcome_result) != expected $($s.expected_outcome)" -ForegroundColor Red
        }
    }
    Write-Host "INTEGRITY: matrix has $($m.scenarios.Count) scenario(s)"
}

if ($fail -gt 0 -and $Strict) { exit 1 }
exit 0
