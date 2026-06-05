#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Refresh heartbeat on an orchestrator lock held by the current process (or matching slug).
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$Slug
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$lockFile = 'agent_collab/state/orchestrator.lock'
if (-not (Test-Path $lockFile)) {
    Write-Error 'NO_LOCK: acquire lock first'
    exit 2
}

$lock = Get-Content -Raw $lockFile | ConvertFrom-Json
if ($lock.slug -ne $Slug) {
    Write-Error "SLUG_MISMATCH: file=$($lock.slug) requested=$Slug"
    exit 2
}

if ($lock.pid -ne $PID) {
    Write-Error "PID_MISMATCH: holder=$($lock.pid) current=$PID"
    exit 2
}

$lock.heartbeat = [DateTimeOffset]::UtcNow.ToString('o')
$lock | ConvertTo-Json -Depth 5 | Set-Content -Path $lockFile -Encoding UTF8
Write-Output "HEARTBEAT_REFRESHED: $($lock.heartbeat)"
exit 0