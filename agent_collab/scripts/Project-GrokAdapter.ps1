#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Project agent_collab/adapters/grok-cursor/ into .grok/skills and .grok/rules.
#>
[CmdletBinding()]
param([switch]$WhatIf)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (git rev-parse --show-toplevel).Trim()
Set-Location $repoRoot

$src = 'agent_collab/adapters/grok-cursor'

function Copy-Tree {
    param([string]$From, [string]$To)
    if (-not (Test-Path $From)) { throw "Missing: $From" }
    if ($WhatIf) {
        Get-ChildItem $From -Recurse -File | ForEach-Object {
            $rel = $_.FullName.Substring((Resolve-Path $From).Path.Length + 1)
            Write-Output "WOULD COPY $rel -> $To/$rel"
        }
        return
    }
    New-Item -ItemType Directory -Path $To -Force | Out-Null
    Get-ChildItem $From -Recurse -File | ForEach-Object {
        $rel = $_.FullName.Substring((Resolve-Path $From).Path.Length + 1)
        $dest = Join-Path $To $rel
        $destDir = Split-Path $dest -Parent
        New-Item -ItemType Directory -Path $destDir -Force | Out-Null
        Copy-Item $_.FullName $dest -Force
        Write-Output "COPIED $rel"
    }
}

function Copy-File {
    param([string]$From, [string]$To)
    if (-not (Test-Path $From)) { throw "Missing: $From" }
    if ($WhatIf) { Write-Output "WOULD COPY $From -> $To"; return }
    $destDir = Split-Path $To -Parent
    if ($destDir) { New-Item -ItemType Directory -Path $destDir -Force | Out-Null }
    Copy-Item $From $To -Force
    Write-Output "COPIED $From -> $To"
}

Copy-Tree -From "$src/skills" -To '.grok/skills'
Copy-Tree -From "$src/rules" -To '.grok/rules'
Copy-Tree -From "$src/agents" -To '.grok/agents'

# runner_config.json is a single tracked file, not a tree. It was present in .grok/ but no
# projection step produced it, so a clean clone could not reproduce the directory it lives in
# (added 2026-07-29). Every file under .grok/ must be regenerable from tracked sources or the
# ignore rules in .gitignore would make it unrecoverable.
Copy-File -From "$src/runner_config.json" -To '.grok/runner_config.json'

Write-Output 'GROK_PROJECTION_COMPLETE'
exit 0