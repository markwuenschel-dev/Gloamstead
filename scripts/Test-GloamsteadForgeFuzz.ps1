#requires -Version 7
# Fuzz: mutate a known-good report N ways; every mutation MUST be rejected by the semantic validator.
# If any mutated (fake) report is accepted, the validator has a hole -> fail closed.
[CmdletBinding()]
param(
    [int]$Cases = 300,
    [string]$Base,
    [switch]$Strict
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'GloamsteadForge.Common.ps1')
$RepoRoot = Split-Path $PSScriptRoot -Parent
if (-not $Base) { $Base = Join-Path $RepoRoot 'specs/gloamsteadforge/fixtures/good/corruption_success.json' }

$baseJson = Get-Content -Raw -LiteralPath $Base
# Sanity: the base must itself be valid, else the fuzz proves nothing.
$baseObj = $baseJson | ConvertFrom-Json
$baseCodes = @(Get-GFCodes -R $baseObj)
if ($baseCodes.Count -ne 0) {
    Write-Host "FUZZ: base fixture is not valid ($($baseCodes -join ',')) — aborting" -ForegroundColor Red
    exit 1
}

# Each mutation corrupts exactly one invariant of a Corruption/Success/objective-bearing report.
$mutations = @(
    { param($c) $c.pcg_init.point_count = 0 },
    { param($c) $c.restoration.point_index = -1 },
    { param($c) $c.restoration.applied = $false },
    { param($c) $c.night_loop.objective_resolved = $false },
    { param($c) $c.night_loop.target_point_index = -1 },
    { param($c) $c.night_loop.ended_intentionally = $false },
    { param($c) $c.night_loop.result_tag = '' },
    { param($c) $c.night_loop.night_type = 'Xyzzy' },
    { param($c) $c.dawn_reflection.consumed_outcome = $false },
    { param($c) $c.dawn_reflection.outcome_result = 'Failure' },
    { param($c) $c.sanctuary_state.mutated = $false },
    { param($c) $c.sanctuary_state.target_corruption_after = $c.sanctuary_state.target_corruption_before }
)

$leaks = 0
for ($i = 0; $i -lt $Cases; $i++) {
    $clone = $baseJson | ConvertFrom-Json
    $m = $mutations[(Get-Random -Minimum 0 -Maximum $mutations.Count)]
    & $m $clone
    $codes = @(Get-GFCodes -R $clone)
    if ($codes.Count -eq 0) { $leaks++; Write-Host "  LEAK: mutation $i accepted a fake report" -ForegroundColor Red }
}

Write-Host "FUZZ: $($Cases - $leaks)/$Cases mutated reports correctly rejected"
if ($leaks -gt 0) { exit 1 }
exit 0
