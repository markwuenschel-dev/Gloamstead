#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Build GloamsteadEditor (close Unreal Editor first — Live Coding blocks this).
#>
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ProjectRoot = (git -C $PSScriptRoot rev-parse --show-toplevel 2>$null).Trim()
if (-not $ProjectRoot) {
    $ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
}
# Drift-proof: resolve the single .uproject in the root and the engine for its
# EngineAssociation (UE_ROOT/GLOAMSTEAD_UE_ENGINE env > registry > D:\UE_<ver> > Program Files).
$UProject = (Get-ChildItem -Path $ProjectRoot -Filter '*.uproject' -File -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
if (-not $UProject) { throw "No .uproject found in $ProjectRoot" }
$assoc = (Get-Content $UProject -Raw | ConvertFrom-Json).EngineAssociation
$engineCandidates = @()
foreach ($hive in @(
    "HKLM:\SOFTWARE\EpicGames\Unreal Engine\$assoc",
    "HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\$assoc"
)) {
    try { $d = (Get-ItemProperty -Path $hive -ErrorAction Stop).InstalledDirectory; if ($d) { $engineCandidates += $d } } catch { }
}
$engineCandidates += @($env:UE_ROOT, $env:GLOAMSTEAD_UE_ENGINE, "D:\UE_$assoc", "C:\Program Files\Epic Games\UE_$assoc")
$engineRoot = $engineCandidates | Where-Object { $_ -and (Test-Path (Join-Path $_ 'Engine\Build\BatchFiles\Build.bat')) } | Select-Object -First 1
if (-not $engineRoot) { throw "Engine not found for EngineAssociation '$assoc'. Set UE_ROOT or GLOAMSTEAD_UE_ENGINE." }
$BuildBat = Join-Path $engineRoot 'Engine\Build\BatchFiles\Build.bat'

$ueProcs = Get-Process -Name 'UnrealEditor','UnrealEditor-Cmd' -ErrorAction SilentlyContinue
if ($ueProcs) {
    Write-Warning @"
Unreal Editor is still running (Live Coding will block the build).
Close the editor completely, then run this script again.
"@
    $ueProcs | Format-Table Id, ProcessName -AutoSize
    exit 1
}

& $BuildBat GloamsteadEditor Win64 Development "-Project=$UProject"
exit $LASTEXITCODE
