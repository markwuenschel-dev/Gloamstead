#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Given a planner DAG (tasks with depends_on), emit wave plan (levels of parallelizable work).
    Very simple topological level assignment for initial scaffold. Real version would be richer.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$DagJsonPath,

    [Parameter(Mandatory=$true)]
    [string]$OutPath
)

Set-StrictMode -Version Latest

$dag = Get-Content -Raw $DagJsonPath | ConvertFrom-Json
$tasks = $dag.dag.tasks

$waves = @()
$remaining = $tasks | ForEach-Object { $_ }
$assigned = @{}

while ($remaining.Count -gt 0) {
    $waveTasks = @()
    $newRemaining = @()

    foreach ($t in $remaining) {
        $deps = $t.depends_on
        $allDone = $true
        if ($deps) {
            foreach ($d in $deps) {
                if (-not $assigned.ContainsKey($d)) { $allDone = $false; break }
            }
        }
        if ($allDone) {
            $waveTasks += $t.task_id
            $assigned[$t.task_id] = $true
        } else {
            $newRemaining += $t
        }
    }

    if ($waveTasks.Count -eq 0 -and $remaining.Count -gt 0) {
        Write-Error "Cycle or unmet dependency detected in DAG. Remaining: $($remaining.task_id -join ', ')"
        exit 1
    }

    if ($waveTasks.Count -gt 0) {
        $waves += [ordered]@{
            wave_id = "wave-$($waves.Count + 1)"
            task_ids = $waveTasks
            status = "planned"
        }
    }
    $remaining = $newRemaining
}

$result = [ordered]@{
    waves = $waves
    total_tasks = $tasks.Count
    generated = [DateTimeOffset]::UtcNow.ToString('o')
}

$result | ConvertTo-Json -Depth 5 | Set-Content $OutPath
Write-Output "WAVE_PLAN: $($waves.Count) waves for $($tasks.Count) tasks -> $OutPath"
exit 0
