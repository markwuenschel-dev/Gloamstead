#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Whitelisted entry point for editor-generation: import Data Assets from JSON manifest.
#>
[CmdletBinding()]
param(
    [string]$Manifest = "specs/data/vs-polish-starter.json",
    [string]$ProjectRoot = "",
    [string]$UeRoot = $env:UE_ROOT
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $ProjectRoot) {
    $ProjectRoot = (git -C $PSScriptRoot rev-parse --show-toplevel 2>$null).Trim()
    if (-not $ProjectRoot) {
        $ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
    }
}

# Resolve the single .uproject in the repo root (drift-proof: no hardcoded name).
$UProject = (Get-ChildItem -Path $ProjectRoot -Filter '*.uproject' -File -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
if (-not $UProject -or -not (Test-Path $UProject)) { throw "No .uproject found in $ProjectRoot" }

$ManifestRel = ($Manifest -replace '\\', '/')
if ([IO.Path]::IsPathRooted($Manifest)) {
    $ManifestRel = [IO.Path]::GetRelativePath($ProjectRoot, $Manifest).Replace('\', '/')
}
$ManifestFull = Join-Path $ProjectRoot ($ManifestRel -replace '/', [IO.Path]::DirectorySeparatorChar)
if (-not (Test-Path $ManifestFull)) { throw "Missing manifest: $ManifestFull" }

# Resolve the engine for this .uproject's EngineAssociation, portable across install
# locations: UE_ROOT arg > GLOAMSTEAD_UE_ENGINE env > registry InstalledDirectory >
# D:\UE_<ver> / Program Files. (This machine's 5.8 is at D:\UE_5.8, not Program Files.)
function Find-UnrealEditorCmd {
    param([string]$Root, [string]$UProjectPath)
    if ($Root) {
        $Candidate = Join-Path $Root 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
        if (Test-Path $Candidate) { return $Candidate }
    }
    $assoc = (Get-Content $UProjectPath -Raw | ConvertFrom-Json).EngineAssociation
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
    $candidates += @($env:GLOAMSTEAD_UE_ENGINE, "D:\UE_$assoc", "C:\Program Files\Epic Games\UE_$assoc")
    foreach ($c in $candidates) {
        if ($c) {
            $Candidate = Join-Path $c 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
            if (Test-Path $Candidate) { return $Candidate }
        }
    }
    throw "UnrealEditor-Cmd.exe not found for EngineAssociation '$assoc'. Set UE_ROOT or GLOAMSTEAD_UE_ENGINE."
}

$EditorCmd  = Find-UnrealEditorCmd -Root $UeRoot -UProjectPath $UProject
$EngineRoot = (Resolve-Path (Join-Path (Split-Path $EditorCmd) '../../..')).Path
$LogDir = Join-Path $ProjectRoot 'Saved/Logs'
New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
$LogName = 'GloamsteadImportDataAssets.log'
$LogFile = Join-Path $LogDir $LogName
$CmdLog = Join-Path $LogDir 'GloamsteadImportDataAssets.cmd.log'
$DefaultLog = Join-Path $LogDir 'Gloamstead.log'
$EditorModuleDll = Join-Path $ProjectRoot 'Binaries/Win64/UnrealEditor-GloamsteadEditor.dll'

if (-not (Test-Path $EditorModuleDll)) {
    throw @"
GloamsteadEditor is not compiled (missing $EditorModuleDll).

Build Development Editor first (or run gate.ps1):
  & `"$EngineRoot\Engine\Build\BatchFiles\Build.bat`" GloamsteadEditor Win64 Development `"-Project=$UProject`"
"@
}

Write-Host "Project:  $UProject"
Write-Host "Manifest: $ManifestRel"
Write-Host "Running:  $EditorCmd -run=GloamsteadImportDataAssets -Manifest=$ManifestRel -log=$LogName"

Push-Location $ProjectRoot
try {
    $Output = & $EditorCmd $UProject '-run=GloamsteadImportDataAssets' "-Manifest=$ManifestRel" '-unattended' '-nopause' '-NullRHI' "-log=$LogName" 2>&1
    $Exit = $LASTEXITCODE
}
finally {
    Pop-Location
}

$Output | Out-File -FilePath $CmdLog -Encoding utf8
$Output | ForEach-Object { Write-Host $_ }

if ($Exit -ne 0) {
    Write-Error "Import failed (exit $Exit). Check:`n  $LogFile`n  $DefaultLog`n  $CmdLog"
    foreach ($Candidate in @($LogFile, $DefaultLog, $CmdLog)) {
        if (Test-Path $Candidate) {
            Write-Host "--- tail of $Candidate ---"
            Get-Content $Candidate -Tail 50
            break
        }
    }
    exit $Exit
}

Write-Host "Import succeeded."
Get-ChildItem (Join-Path $ProjectRoot 'Content/Data') -Filter 'DA_*' -ErrorAction SilentlyContinue | ForEach-Object { Write-Host "  $($_.Name)" }
exit 0
