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

# -Recurse (added 2026-07-29): this was a flat, non-recursive copy, so a subdirectory added under
# adapters/claude-code/{agents,commands,hooks} would be silently skipped. Since .gitignore now
# treats these projection targets as generated-and-ignored, a file the projection cannot reproduce
# is a file that cannot be recovered. Mirrors Project-GrokAdapter.ps1's Copy-Tree.
foreach ($m in $dirMaps) {
    if (-not (Test-Path $m.From)) { throw "Missing source dir: $($m.From)" }
    if (-not $WhatIf) { New-Item -ItemType Directory -Path $m.To -Force | Out-Null }

    $fromRoot = (Resolve-Path $m.From).Path
    Get-ChildItem $m.From -Recurse -File | ForEach-Object {
        $rel  = $_.FullName.Substring($fromRoot.Length + 1)
        $dest = Join-Path $m.To $rel
        if ($WhatIf) { Write-Output "WOULD COPY $($_.FullName) -> $dest" }
        else {
            $destDir = Split-Path $dest -Parent
            if ($destDir) { New-Item -ItemType Directory -Path $destDir -Force | Out-Null }
            Copy-Item $_.FullName $dest -Force
            Write-Output "COPIED $rel -> $dest"
        }
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