#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Release the orchestrator lock if it is held by the current process.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$Slug
)

Set-StrictMode -Version Latest

$lockFile = "agent_collab/state/orchestrator.lock"

if (-not (Test-Path $lockFile)) {
    Write-Output "NO_LOCK_FILE"
    exit 0
}

try {
    $lock = Get-Content -Raw $lockFile | ConvertFrom-Json -ErrorAction Stop
    if ($lock.slug -ne $Slug) {
        Write-Warning "Lock slug mismatch (file=$($lock.slug), requested=$Slug). Not removing."
        exit 1
    }

    $currentPid = $PID
    $holderPid = $lock.pid

    if ($holderPid -ne $currentPid) {
        # Check if the recorded holder process is still alive (cross-platform best effort)
        $holderAlive = $false
        try {
            $proc = Get-Process -Id $holderPid -ErrorAction Stop
            $holderAlive = $true
        } catch {}

        if ($holderAlive) {
            Write-Warning "Lock held by different live pid ($holderPid vs current $currentPid). Not removing."
            exit 2
        } else {
            Write-Warning "Lock holder pid $holderPid no longer running. Releasing stale lock for slug=$Slug."
        }
    }

    Remove-Item $lockFile -Force
    Write-Output "RELEASED: slug=$Slug (was held by pid $holderPid)"
    exit 0
} catch {
    Write-Error "Failed to release lock: $_"
    exit 1
}
