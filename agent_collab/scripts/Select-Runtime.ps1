#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Deterministic runtime selection per registry/routing.json safety floor + preference rank.
    Outputs JSON: chosen_runtime, template_id, reason. Exits 2 if no runtime satisfies requirements.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string[]]$RequiredCapabilities,

    [Parameter(Mandatory=$true)]
    [ValidateSet('Coder', 'Critic', 'Planner', 'Researcher', 'Documentor', 'Architect', 'Orchestrator')]
    [string]$Role,

    [string]$SafetyFloor = 'none'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (git rev-parse --show-toplevel).Trim()
$runtimes = Get-Content -Raw (Join-Path $repoRoot 'agent_collab/registry/runtimes.json') | ConvertFrom-Json
$agents = Get-Content -Raw (Join-Path $repoRoot 'agent_collab/registry/agents.json') | ConvertFrom-Json
$routing = Get-Content -Raw (Join-Path $repoRoot 'agent_collab/registry/routing.json') | ConvertFrom-Json

function Test-RuntimeFlags {
    param($RuntimeId, [string[]]$Flags)
    $rt = $runtimes.runtimes.$RuntimeId
    if (-not $rt) { return $false }
    foreach ($f in $Flags) {
        if (-not $rt.$f) { return $false }
    }
    return $true
}

$floorFlags = @()
switch ($SafetyFloor) {
    'edit_code' { $floorFlags = @('can_edit_files', 'can_use_worktree', 'can_enforce_scope') }
    'final_integration_critic' { $floorFlags = @('can_run_tests') }
    default { }
}

$candidates = @()
foreach ($rtId in $runtimes.enabled_runtimes) {
    $rt = $runtimes.runtimes.$rtId
    if (-not $rt) { continue }

    if ($floorFlags.Count -gt 0 -and -not (Test-RuntimeFlags $rtId $floorFlags)) { continue }

    $templates = $agents.templates | Where-Object {
        $_.enabled -and $_.runtime -eq $rtId -and $_.role -eq $Role
    }
    foreach ($t in $templates) {
        $missing = @()
        foreach ($cap in $RequiredCapabilities) {
            if ($t.capabilities -notcontains $cap) { $missing += $cap }
        }
        if ($missing.Count -eq 0) {
            $rank = [array]::IndexOf($routing.preference_rank, $rtId)
            if ($rank -lt 0) { $rank = 999 }
            $candidates += [pscustomobject]@{
                runtime = $rtId
                template_id = $t.id
                max_parallel = $t.max_parallel
                rank = $rank
            }
        }
    }
}

if ($candidates.Count -eq 0) {
    Write-Error "NO_RUNTIME: role=$Role caps=$($RequiredCapabilities -join ',') floor=$SafetyFloor"
    exit 2
}

$chosen = $candidates | Sort-Object rank, max_parallel, template_id | Select-Object -First 1
$reason = "preference_rank=$($routing.preference_rank -join '>'), safety_floor=$SafetyFloor, required=$($RequiredCapabilities -join ',')"

$result = [ordered]@{
    chosen_runtime = $chosen.runtime
    template_id = $chosen.template_id
    reason = $reason
}

$result | ConvertTo-Json -Compress | Write-Output
exit 0