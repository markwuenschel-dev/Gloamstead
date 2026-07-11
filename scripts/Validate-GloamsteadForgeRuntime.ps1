#requires -Version 7
# Fail-closed SEMANTIC validation of GloamsteadForge runtime reports (success must be substantiated).
[CmdletBinding()]
param(
    [string]$Path,
    [switch]$Strict
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'GloamsteadForge.Common.ps1')
$RepoRoot = Split-Path $PSScriptRoot -Parent
if (-not $Path) { $Path = Join-Path $RepoRoot 'procedural/reports/gloamsteadforge' }

$files = @()
if (Test-Path -PathType Container $Path) { $files = @(Get-ChildItem -LiteralPath $Path -Filter *.json -File) }
elseif (Test-Path -PathType Leaf $Path) { $files = @(Get-Item -LiteralPath $Path) }

if ($files.Count -eq 0) {
    Write-Host "RUNTIME: no reports found under $Path" -ForegroundColor Yellow
    if ($Strict) { exit 1 } else { exit 0 }
}

$fail = 0
foreach ($f in $files) {
    $r = Get-GFReport -Path $f.FullName
    $codes = @(Get-GFCodes -R $r)
    if ($codes.Count -eq 0) { Write-Host "  PASS runtime: $($f.Name)" -ForegroundColor Green }
    else {
        $fail++
        Write-Host "  FAIL runtime: $($f.Name) -> $($codes -join ', ')" -ForegroundColor Red
    }
}
Write-Host "RUNTIME: $($files.Count - $fail)/$($files.Count) semantically valid"
if ($fail -gt 0 -and $Strict) { exit 1 }
exit 0
