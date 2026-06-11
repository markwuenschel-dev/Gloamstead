#requires -Version 7.0
<#
.SYNOPSIS
  Maintain the persisted, cumulative autonomy budget across woken Orchestrator ticks.
.DESCRIPTION
  The Orchestrator is woken (not looping): each tick is a fresh session that must learn how much
  budget the current autonomous run has already spent. This script owns agent_collab/state/run_state.json
  and is the only place that run is opened, advanced, checked, and closed.

  It is project-agnostic: it hardcodes no slug and resolves paths relative to itself
  (../state, ../context). Cumulative budgets are read from context/autonomy_policy.json
  ("cumulative" block); a missing budget means "no limit".

  This mutates run_state.json, which is Orchestrator-owned state -- consistent with single-writer.
  It never writes to outbox/, inbox/, or handoffs/.

.PARAMETER Action
  Init     - start a new run (refuses if one is already running, unless -Force).
  Tick     - record a wake (ticks++, last_tick = now).
  Record   - add to cumulative counters (use the increment params).
  Check    - evaluate cumulative spend vs budget; emit a decision; exit 10 if a budget is hit.
  Stop     - mark the run stopped with -Reason.
  Complete - mark the run completed (no approved work remains).

.PARAMETER TasksCompleted   Increment for Record.
.PARAMETER TokensUsed       Increment for Record (estimate).
.PARAMETER CandidateFailures Increment for Record.
.PARAMETER RuntimeFailures  Increment for Record.
.PARAMETER Reason           Reason text for Stop.
.PARAMETER Force            For Init: overwrite a run that is still 'running'.

.OUTPUTS
  Init/Tick/Record/Stop/Complete -> the current run_state JSON on stdout, exit 0.
  Check -> a decision object on stdout; exit 0 = continue, exit 10 = stop (budget hit),
           exit 3 = no active run. exit 2 = error on any action.
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory)]
  [ValidateSet('Init','Tick','Record','Check','Stop','Complete')]
  [string] $Action,

  [int] $TasksCompleted   = 0,
  [int] $TokensUsed       = 0,
  [int] $CandidateFailures = 0,
  [int] $RuntimeFailures  = 0,
  [string] $Reason,
  [switch] $Force
)

$ErrorActionPreference = 'Stop'

function Fail([string]$msg) { [Console]::Error.WriteLine("ERROR: $msg"); exit 2 }

try {
  $scriptDir  = Split-Path -Parent $PSCommandPath
  $stateDir   = (Resolve-Path (Join-Path $scriptDir '..\state')).Path
  $statePath  = Join-Path $stateDir 'run_state.json'
  $policyPath = Join-Path $scriptDir '..\context\autonomy_policy.json'
  $nowUtc     = (Get-Date).ToUniversalTime()
  $nowIso     = $nowUtc.ToString('o')

  function Read-State {
    if (-not (Test-Path -LiteralPath $statePath)) { return $null }
    return (Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json)
  }

  function Write-State($obj) {
    # Atomic-ish: write to a temp file then move into place.
    $tmp = "$statePath.tmp"
    ($obj | ConvertTo-Json -Depth 8) | Set-Content -LiteralPath $tmp -Encoding utf8
    Move-Item -LiteralPath $tmp -Destination $statePath -Force
  }

  function New-RunState {
    [ordered]@{
      run_id               = "run-{0}-{1}" -f $nowUtc.ToString('yyyyMMddHHmmss'), ([guid]::NewGuid().ToString('N').Substring(0,6))
      status               = 'running'
      started              = $nowIso
      last_tick            = $nowIso
      updated              = $nowIso
      ticks                = 0
      tasks_completed      = 0
      tokens_used_estimate = 0
      candidate_failures   = 0
      runtime_failures     = 0
      stop_reason          = $null
    }
  }

  switch ($Action) {

    'Init' {
      $existing = Read-State
      if ($existing -and $existing.status -eq 'running' -and -not $Force) {
        Fail "a run is already 'running' (run_id $($existing.run_id)). Use -Force to start a new run."
      }
      $state = New-RunState
      Write-State $state
      $state | ConvertTo-Json -Depth 8
      exit 0
    }

    'Tick' {
      $state = Read-State
      if (-not $state) { [Console]::Error.WriteLine("NO_RUN: run_state.json absent; call Init first."); exit 3 }
      $state.ticks     = [int]$state.ticks + 1
      $state.last_tick = $nowIso
      $state.updated   = $nowIso
      Write-State $state
      $state | ConvertTo-Json -Depth 8
      exit 0
    }

    'Record' {
      $state = Read-State
      if (-not $state) { [Console]::Error.WriteLine("NO_RUN: run_state.json absent; call Init first."); exit 3 }
      $state.tasks_completed      = [int]$state.tasks_completed      + $TasksCompleted
      $state.tokens_used_estimate = [int]$state.tokens_used_estimate + $TokensUsed
      $state.candidate_failures   = [int]$state.candidate_failures   + $CandidateFailures
      $state.runtime_failures     = [int]$state.runtime_failures     + $RuntimeFailures
      $state.updated              = $nowIso
      Write-State $state
      $state | ConvertTo-Json -Depth 8
      exit 0
    }

    'Check' {
      $state = Read-State
      if (-not $state) { [Console]::Error.WriteLine("NO_RUN: run_state.json absent; call Init first."); exit 3 }

      # Budgets (missing -> no limit).
      $cum = $null
      if (Test-Path -LiteralPath $policyPath) {
        $policy = Get-Content -LiteralPath $policyPath -Raw | ConvertFrom-Json
        $cum = $policy.cumulative
      }
      function Limit($name) {
        if ($cum -and ($cum.PSObject.Properties.Name -contains $name) -and $null -ne $cum.$name) { return [double]$cum.$name }
        return [double]::PositiveInfinity
      }

      $started = [datetime]::Parse($state.started).ToUniversalTime()
      $elapsedMin = [math]::Round(($nowUtc - $started).TotalMinutes, 2)

      $exceeded = @()
      if ([int]$state.tasks_completed      -ge (Limit 'max_tasks_per_run'))     { $exceeded += 'max_tasks_per_run' }
      if ([int]$state.tokens_used_estimate -ge (Limit 'max_tokens'))            { $exceeded += 'max_tokens' }
      if ($elapsedMin                      -ge (Limit 'max_wall_minutes'))      { $exceeded += 'max_wall_minutes' }
      if ([int]$state.candidate_failures   -ge (Limit 'max_candidate_failures')) { $exceeded += 'max_candidate_failures' }
      if ([int]$state.runtime_failures     -ge (Limit 'max_runtime_failures'))  { $exceeded += 'max_runtime_failures' }
      if ($state.status -in @('stopped','blocked','completed'))                 { $exceeded += "status:$($state.status)" }

      $decision = if ($exceeded.Count -gt 0) { 'stop' } else { 'continue' }

      [pscustomobject]@{
        run_id               = $state.run_id
        decision             = $decision
        exceeded             = @($exceeded)
        ticks                = [int]$state.ticks
        tasks_completed      = [int]$state.tasks_completed
        tokens_used_estimate = [int]$state.tokens_used_estimate
        candidate_failures   = [int]$state.candidate_failures
        runtime_failures     = [int]$state.runtime_failures
        elapsed_minutes      = $elapsedMin
        status               = $state.status
      } | ConvertTo-Json -Depth 6

      if ($decision -eq 'stop') { exit 10 } else { exit 0 }
    }

    'Stop' {
      $state = Read-State
      if (-not $state) { [Console]::Error.WriteLine("NO_RUN: run_state.json absent."); exit 3 }
      $state.status      = 'stopped'
      $state.stop_reason = if ($Reason) { $Reason } else { 'unspecified' }
      $state.updated     = $nowIso
      Write-State $state
      $state | ConvertTo-Json -Depth 8
      exit 0
    }

    'Complete' {
      $state = Read-State
      if (-not $state) { [Console]::Error.WriteLine("NO_RUN: run_state.json absent."); exit 3 }
      $state.status      = 'completed'
      $state.stop_reason = if ($Reason) { $Reason } else { 'no approved work remaining' }
      $state.updated     = $nowIso
      Write-State $state
      $state | ConvertTo-Json -Depth 8
      exit 0
    }
  }
}
catch {
  Fail $_.Exception.Message
}
