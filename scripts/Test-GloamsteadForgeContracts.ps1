#requires -Version 7
# Schema (structural) validation of GloamsteadForge runtime reports against the RuntimeReport JSON Schema.
[CmdletBinding()]
param(
    [string]$Path,
    [switch]$Strict
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$RepoRoot = Split-Path $PSScriptRoot -Parent
if (-not $Path) { $Path = Join-Path $RepoRoot 'procedural/reports/gloamsteadforge' }
$SchemaPath = Join-Path $RepoRoot 'specs/gloamsteadforge/contracts/GloamsteadForgeRuntimeReport.schema.json'
$schema = Get-Content -Raw -LiteralPath $SchemaPath

$files = @()
if (Test-Path -PathType Container $Path) { $files = @(Get-ChildItem -LiteralPath $Path -Filter *.json -File | Where-Object { $_.Name -ne '_run_manifest.json' }) }
elseif (Test-Path -PathType Leaf $Path) { $files = @(Get-Item -LiteralPath $Path) }

if ($files.Count -eq 0) {
    Write-Host "CONTRACTS: no reports found under $Path" -ForegroundColor Yellow
    if ($Strict) { exit 1 } else { exit 0 }
}

$fail = 0
foreach ($f in $files) {
    $json = Get-Content -Raw -LiteralPath $f.FullName
    $ok = $false; $ev = $null
    try { $ok = Test-Json -Json $json -Schema $schema -ErrorVariable ev -ErrorAction SilentlyContinue } catch { $ok = $false }
    if ($ok) { Write-Host "  PASS schema: $($f.Name)" -ForegroundColor Green }
    else {
        $fail++
        Write-Host "  FAIL schema: $($f.Name)" -ForegroundColor Red
        foreach ($e in @($ev)) { if ($e) { Write-Host "      $e" -ForegroundColor DarkRed } }
    }
}
Write-Host "CONTRACTS: $($files.Count - $fail)/$($files.Count) structurally valid"
if ($fail -gt 0 -and $Strict) { exit 1 }
exit 0
