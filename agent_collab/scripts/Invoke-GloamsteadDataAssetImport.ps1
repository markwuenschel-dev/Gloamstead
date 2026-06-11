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

$UProject = Join-Path $ProjectRoot 'Gloamstead.uproject'
if (-not (Test-Path $UProject)) { throw "Missing uproject: $UProject" }

$ManifestRel = ($Manifest -replace '\\', '/')
if ([IO.Path]::IsPathRooted($Manifest)) {
    $ManifestRel = [IO.Path]::GetRelativePath($ProjectRoot, $Manifest).Replace('\', '/')
}
$ManifestFull = Join-Path $ProjectRoot ($ManifestRel -replace '/', [IO.Path]::DirectorySeparatorChar)
if (-not (Test-Path $ManifestFull)) { throw "Missing manifest: $ManifestFull" }

function Find-UnrealEditorCmd {
    param([string]$Root)
    if ($Root) {
        $Candidate = Join-Path $Root 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
        if (Test-Path $Candidate) { return $Candidate }
    }
    $Guess = 'C:\Program Files\Epic Games\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
    if (Test-Path $Guess) { return $Guess }
    throw 'UnrealEditor-Cmd.exe not found. Set UE_ROOT to your engine install.'
}

$EditorCmd = Find-UnrealEditorCmd -Root $UeRoot
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

Build Development Editor first:
  & `"C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat`" GloamsteadEditor Win64 Development `"-Project=$UProject`"
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
