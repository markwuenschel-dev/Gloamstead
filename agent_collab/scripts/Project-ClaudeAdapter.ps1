#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Project agent_collab/adapters/claude-code/ into .claude/ (source of truth -> runtime projection).
#>
[CmdletBinding()]
param(
    [switch]$WhatIf
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (git rev-parse --show-toplevel).Trim()
Set-Location $repoRoot

$src = 'agent_collab/adapters/claude-code'
$dirMaps = @(
    @{ From = "$src/agents"; To = '.claude/agents' },
    @{ From = "$src/commands"; To = '.claude/commands' },
    @{ From = "$src/hooks"; To = '.claude/hooks' }
)

foreach ($m in $dirMaps) {
    if (-not (Test-Path $m.From)) { throw "Missing source dir: $($m.From)" }
    if (-not $WhatIf) { New-Item -ItemType Directory -Path $m.To -Force | Out-Null }

    Get-ChildItem $m.From -File | ForEach-Object {
        $dest = Join-Path $m.To $_.Name
        if ($WhatIf) { Write-Output "WOULD COPY $($_.FullName) -> $dest" }
        else { Copy-Item $_.FullName $dest -Force; Write-Output "COPIED $($_.Name) -> $dest" }
    }
}

$settingsSrc = "$src/settings.json"
$settingsDest = '.claude/settings.json'
if (-not (Test-Path $settingsSrc)) { throw "Missing source file: $settingsSrc" }
if ($WhatIf) { Write-Output "WOULD COPY $settingsSrc -> $settingsDest" }
else {
    New-Item -ItemType Directory -Path (Split-Path $settingsDest -Parent) -Force | Out-Null
    Copy-Item $settingsSrc $settingsDest -Force
    Write-Output "COPIED $settingsSrc -> $settingsDest"
}

Write-Output "PROJECTION_COMPLETE"
exit 0