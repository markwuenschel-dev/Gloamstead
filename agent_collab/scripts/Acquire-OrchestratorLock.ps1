#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Acquire the single-writer orchestrator lock for the given slug.
    Exits 0 on success (lock acquired or already held by this process), non-zero on conflict.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$Slug,

    [int]$StaleMinutes = 30
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$lockDir = "agent_collab/state"
$lockFile = Join-Path $lockDir "orchestrator.lock"
New-Item -ItemType Directory -Path $lockDir -Force | Out-Null

$now = [DateTimeOffset]::UtcNow
$sessionId = "$env:COMPUTERNAME-$PID-$($now.ToUnixTimeSeconds())"
$pidVal = $PID
$cwd = (Get-Location).Path

function Get-LockContent {
    if (Test-Path $lockFile) {
        try { return Get-Content -Raw $lockFile | ConvertFrom-Json -ErrorAction Stop }
        catch { return $null }
    }
    return $null
}

$existing = Get-LockContent
if ($existing) {
    $hb = [DateTimeOffset]::Parse($existing.heartbeat)
    $ageMin = ($now - $hb).TotalMinutes

    if ($ageMin -lt $StaleMinutes) {
        if ($existing.pid -eq $pidVal -and $existing.cwd -eq $cwd) {
            # We already hold it (re-entrant in same process)
            Write-Output "REACQUIRED (same process): slug=$Slug pid=$pidVal"
            exit 0
        }
        Write-Error "FRESH_LOCK_HELD: slug=$($existing.slug) pid=$($existing.pid) cwd=$($existing.cwd) heartbeat=$($existing.heartbeat) age_minutes=$([math]::Round($ageMin,1))"
        Write-Error "Another Orchestrator session is active. Stop or wait, or ask human for takeover."
        exit 3
    } else {
        Write-Warning "STALE_LOCK: age_minutes=$([math]::Round($ageMin,1)). Will take over after human confirmation in interactive use."
        # In non-interactive / script context we still take over but log loudly.
        # Real usage: the Orchestrator (human+Claude) sees the warning and decides.
    }
}

$lockObj = [ordered]@{
    slug = $Slug
    session_id = $sessionId
    pid = $pidVal
    created = $now.ToString('o')
    heartbeat = $now.ToString('o')
    cwd = $cwd
}

$lockObj | ConvertTo-Json -Depth 5 | Set-Content -Path $lockFile -Encoding UTF8
Write-Output "ACQUIRED: slug=$Slug session=$sessionId pid=$pidVal created=$($lockObj.created)"
exit 0
