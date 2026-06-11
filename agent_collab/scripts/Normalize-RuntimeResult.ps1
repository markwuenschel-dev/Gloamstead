#!/usr/bin/env pwsh
<#
.SYNOPSIS
    For runtimes with can_return_schema=false (e.g. local-script), map raw inbox output
    to a minimal worker_summary-shaped object and write it to -OutPath.
    The Orchestrator still validates the result against the real schema after this step.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$Runtime,

    [Parameter(Mandatory=$true)]
    [string]$RawPath,

    [Parameter(Mandatory=$true)]
    [string]$OutPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not (Test-Path $RawPath)) {
    Write-Error "Raw input not found: $RawPath"
    exit 2
}

New-Item -ItemType Directory -Path (Split-Path $OutPath) -Force | Out-Null

$rawContent = Get-Content -Raw $RawPath
$now = [DateTimeOffset]::UtcNow.ToString('o')

# Very conservative default mapping. Real implementations would parse command output, exit codes, etc.
$normalized = [ordered]@{
    request_id = "unknown-from-raw-$([Guid]::NewGuid().ToString().Substring(0,8))"
    task_id = "unknown"
    role = "unknown"
    runtime = $Runtime
    instance_id = "raw-$Runtime-$($now -replace '[:\.]','-')"
    verdict = "BLOCKED"   # Conservative default; Orchestrator / human must review
    summary = "Raw output from $Runtime normalized by Normalize-RuntimeResult.ps1. Manual review and schema validation required before use."
    changed_files = @()
    commands_run = @()
    criteria_results = @()
    artifacts = @($RawPath)
    branch = $null
    worktree_path = $null
    base_commit = $null
    head_commit = $null
    risks = @("Raw output from can_return_schema=false runtime; not authoritative until validated and approved by Orchestrator")
    needs = $null
    blocker = "Normalization only; full validation + human/Orchestrator review still required"
    raw_output_path = $RawPath
}

$normalized | ConvertTo-Json -Depth 6 | Set-Content -Path $OutPath -Encoding UTF8
Write-Output "NORMALIZED: $RawPath -> $OutPath (verdict default BLOCKED; review required)"
exit 0
