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
$UProject = Join-Path $ProjectRoot 'Gloamstead.uproject'
$BuildBat = 'C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat'

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
