#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Remove a task worktree under .grok/worktrees/<TaskId>.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$TaskId,

    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (git rev-parse --show-toplevel).Trim()
$worktreePath = Join-Path $repoRoot ".grok/worktrees/$TaskId"

if (-not (Test-Path $worktreePath)) {
    Write-Output "NO_WORKTREE: $worktreePath"
    exit 0
}

$args = @('worktree', 'remove', $worktreePath)
if ($Force) { $args += '--force' }

Set-Location $repoRoot
& git @args
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Output "REMOVED: $worktreePath"
exit 0