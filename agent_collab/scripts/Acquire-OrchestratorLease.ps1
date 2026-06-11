#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Acquire the exclusive Orchestrator lease.

    Validates that the requesting runtime supports the orchestrator role and has required capabilities.
    Only one non-expired lease may exist.

    Exits 0 on success, 2 on failure/block.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$Slug,

    [Parameter(Mandatory=$true)]
    [string]$Runtime,

    [string]$AgentId = $env:COMPUTERNAME,
    [string]$SessionId = [guid]::NewGuid().ToString()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (git rev-parse --show-toplevel).Trim()
$leasesFile = Join-Path $repoRoot "agent_collab/state/leases.json"
$runtimesFile = Join-Path $repoRoot "agent_collab/registry/runtimes.json"
$rolesFile = Join-Path $repoRoot "agent_collab/registry/roles.json"

# Load data
$runtimes = Get-Content -Raw $runtimesFile | ConvertFrom-Json
$roles = Get-Content -Raw $rolesFile | ConvertFrom-Json
$leasesData = Get-Content -Raw $leasesFile | ConvertFrom-Json

# Validate runtime
if (-not $runtimes.runtimes.PSObject.Properties[$Runtime] -or -not $runtimes.runtimes.$Runtime.enabled) {
    Write-Error "BLOCKED: Runtime '$Runtime' is not enabled."
    exit 2
}
$rt = $runtimes.runtimes.$Runtime
if (-not $rt.may_act_as_orchestrator) {
    Write-Error "BLOCKED: Runtime '$Runtime' may not act as orchestrator (missing may_act_as_orchestrator or capabilities)."
    exit 2
}

# Validate capabilities
$orchestratorReqs = $roles.roles.orchestrator.required_capabilities
$missing = $orchestratorReqs | Where-Object { $_ -notin $rt.capabilities }
if ($missing) {
    Write-Error "BLOCKED: Runtime '$Runtime' missing required Orchestrator capabilities: $($missing -join ', ')"
    exit 2
}

# Check for existing active lease
$now = Get-Date
$active = $leasesData.leases | Where-Object {
    (Get-Date $_.expires_at) -gt $now
}
if ($active) {
    Write-Error "BLOCKED: Active Orchestrator lease exists (runtime=$($active.runtime), expires=$($active.expires_at)). Release or wait for expiry."
    exit 2
}

# Create new lease (15 minute default expiry, renewable)
$leaseId = [guid]::NewGuid().ToString()
$acquired = (Get-Date).ToUniversalTime().ToString("o")
$expires = (Get-Date).AddMinutes(15).ToUniversalTime().ToString("o")

$newLease = [pscustomobject]@{
    lease_id = $leaseId
    role = "orchestrator"
    runtime = $Runtime
    agent_id = $AgentId
    session_id = $SessionId
    host_id = $env:COMPUTERNAME
    process_id = $PID
    acquired_at = $acquired
    heartbeat_at = $acquired
    expires_at = $expires
    notes = "Acquired via Acquire-OrchestratorLease.ps1"
}

$leasesData.leases = @($newLease)  # Replace any stale ones
$leasesData | ConvertTo-Json -Depth 10 | Set-Content $leasesFile -Encoding UTF8

Write-Output "LEASE_ACQUIRED: $leaseId runtime=$Runtime expires=$expires"
exit 0
