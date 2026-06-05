#requires -Version 7.0
<#
.SYNOPSIS
  Project current Orchestrator state into a single read-only status.json for the dashboard.
.DESCRIPTION
  Reads task_state.json, scheduler_state.json, leases.json, run_state.json, and the tail of
  orchestrator.log, and writes a flattened state/status.json that a static dashboard polls.

  This is a PROJECTION ONLY. It reads source state and writes status.json -- it never writes back
  to task_state/scheduler_state/leases/run_state. status.json carries no authority; nothing reads
  it to make decisions. That is what keeps the single-writer invariant intact: the dashboard is an
  observer, not a second writer.

  Project-agnostic: hardcodes no slug (reads context/project.json if present) and tolerates any
  source file being absent.
.PARAMETER OutFile
  Where to write the projection. Default ../state/status.json relative to this script. Point it at
  a served folder (e.g. a GitHub Pages directory) to publish online.
.PARAMETER LogTail
  Number of trailing orchestrator.log lines to include. Default 25.
.OUTPUTS
  Writes OutFile and echoes its path. Exit 0 success, 2 error.
#>
[CmdletBinding()]
param(
  [string] $OutFile,
  [int] $LogTail = 25
)

$ErrorActionPreference = 'Stop'

function Fail([string]$msg) { [Console]::Error.WriteLine("ERROR: $msg"); exit 2 }

try {
  $scriptDir = Split-Path -Parent $PSCommandPath
  $stateDir  = (Resolve-Path (Join-Path $scriptDir '..\state')).Path
  $ctxDir    = (Resolve-Path (Join-Path $scriptDir '..\context')).Path
  $logPath   = Join-Path $scriptDir '..\logs\orchestrator.log'
  if (-not $OutFile) { $OutFile = Join-Path $stateDir 'status.json' }

  function Read-Json([string]$path) {
    if (-not (Test-Path -LiteralPath $path)) { return $null }
    try { return (Get-Content -LiteralPath $path -Raw | ConvertFrom-Json) } catch { return $null }
  }

  $task  = Read-Json (Join-Path $stateDir 'task_state.json')
  $sched = Read-Json (Join-Path $stateDir 'scheduler_state.json')
  $lease = Read-Json (Join-Path $stateDir 'leases.json')
  $run   = Read-Json (Join-Path $stateDir 'run_state.json')
  $proj  = Read-Json (Join-Path $ctxDir   'project.json')

  $slug = if ($proj -and $proj.slug) { $proj.slug } else { $null }

  # --- Active tasks (active is a map keyed by task_id) ---
  $activeTasks = @()
  if ($task -and $task.active) {
    foreach ($p in $task.active.PSObject.Properties) {
      $t = $p.Value
      $activeTasks += [pscustomobject]@{
        task_id     = $p.Name
        status      = $t.status
        phase       = $t.phase
        assigned_to = $t.assigned_to
        runtime     = $t.runtime
        lane        = $t.lane
        risk_level  = $t.risk_level
        cycle       = $t.cycle
        wave_id     = $t.wave_id
      }
    }
  }
  $counts = [pscustomobject]@{
    active  = $activeTasks.Count
    blocked = @($activeTasks | Where-Object { $_.status -in @('blocked','docs_blocked','integration_conflict') }).Count
    done    = if ($task -and $task.history) { @($task.history).Count } else { 0 }
  }

  # --- Waves ---
  $waves = @()
  if ($sched -and $sched.waves) {
    foreach ($w in @($sched.waves)) {
      $waves += [pscustomobject]@{
        wave_id  = $w.wave_id
        status   = $w.status
        parallel = $w.parallel
        task_ids = @($w.task_ids)
      }
    }
  }

  # --- Leases (summary) ---
  $leaseList = @()
  if ($lease -and $lease.leases) {
    foreach ($l in @($lease.leases)) {
      $leaseList += [pscustomobject]@{
        lease_id = $l.lease_id; task_id = $l.task_id; runtime = $l.runtime
        status = $l.status; expires = $l.expires
      }
    }
  }

  # --- Run budget ---
  $runOut = $null
  if ($run) {
    $runOut = [pscustomobject]@{
      run_id               = $run.run_id
      status               = $run.status
      ticks                = $run.ticks
      tasks_completed      = $run.tasks_completed
      tokens_used_estimate = $run.tokens_used_estimate
      candidate_failures   = $run.candidate_failures
      runtime_failures     = $run.runtime_failures
      started              = $run.started
      last_tick            = $run.last_tick
      stop_reason          = $run.stop_reason
    }
  }

  # --- Log tail ---
  $logLines = @()
  if (Test-Path -LiteralPath $logPath) {
    $logLines = @(Get-Content -LiteralPath $logPath -Tail $LogTail)
  }

  $status = [ordered]@{
    generated_at = (Get-Date).ToUniversalTime().ToString('o')
    slug         = $slug
    mode         = if ($sched) { $sched.mode } else { $null }
    run          = $runOut
    counts       = $counts
    active_tasks = @($activeTasks)
    waves        = @($waves)
    leases       = @($leaseList)
    log_tail     = @($logLines)
  }

  $tmp = "$OutFile.tmp"
  ($status | ConvertTo-Json -Depth 8) | Set-Content -LiteralPath $tmp -Encoding utf8
  Move-Item -LiteralPath $tmp -Destination $OutFile -Force

  Write-Output "PROJECTED: $OutFile"
  exit 0
}
catch {
  Fail $_.Exception.Message
}
