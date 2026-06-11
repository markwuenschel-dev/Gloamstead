#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Renew the active Orchestrator lease (extend expiry).
#>
param(
    [Parameter(Mandatory=$true)]
    [string]$LeaseId,
    [int]$Minutes = 15
)

Set-StrictMode -Version Latest
$repoRoot = (git rev-parse --show-toplevel).Trim()
$leasesFile = Join-Path $repoRoot "agent_collab/state/leases.json"
$leasesData = Get-Content -Raw $leasesFile | ConvertFrom-Json

$lease = $leasesData.leases | Where-Object { $_.lease_id -eq $LeaseId }
if (-not $lease) {
    Write-Error "BLOCKED: Lease $LeaseId not found."
    exit 2
}

$now = Get-Date
if ((Get-Date $lease.expires_at) -lt $now) {
    Write-Error "BLOCKED: Lease already expired."
    exit 2
}

$lease.heartbeat_at = (Get-Date).ToUniversalTime().ToString("o")
$lease.expires_at = (Get-Date).AddMinutes($Minutes).ToUniversalTime().ToString("o")

$leasesData | ConvertTo-Json -Depth 10 | Set-Content $leasesFile -Encoding UTF8
Write-Output "LEASE_RENEWED: $LeaseId new_expires=$($lease.expires_at)"
exit 0
