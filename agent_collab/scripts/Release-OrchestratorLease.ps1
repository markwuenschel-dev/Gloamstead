#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Release the active Orchestrator lease.
#>
param(
    [Parameter(Mandatory=$true)]
    [string]$LeaseId
)

Set-StrictMode -Version Latest
$repoRoot = (git rev-parse --show-toplevel).Trim()
$leasesFile = Join-Path $repoRoot "agent_collab/state/leases.json"
$leasesData = Get-Content -Raw $leasesFile | ConvertFrom-Json

$leasesData.leases = $leasesData.leases | Where-Object { $_.lease_id -ne $LeaseId }
$leasesData | ConvertTo-Json -Depth 10 | Set-Content $leasesFile -Encoding UTF8

Write-Output "LEASE_RELEASED: $LeaseId"
exit 0
