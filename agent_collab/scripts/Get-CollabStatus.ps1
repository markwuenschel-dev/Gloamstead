#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Quick status reporter for the Gloamstead agent_collab system (Hard Gate task).
.DESCRIPTION
    Safe read-only script. Reports mode, lock, queues, handoffs, inbox, decisions, git state.
#>
[CmdletBinding()]
param()

$repo = (git rev-parse --show-toplevel 2>$null).Trim()
if (-not $repo) { $repo = (Get-Location).Path }
$root = Join-Path $repo "agent_collab"

function J($p) { if (Test-Path $p) { try { Get-Content -Raw $p | ConvertFrom-Json } catch { $null } } else { $null } }

Write-Host "=== Gloamstead Collab Status ===" -ForegroundColor Cyan
Write-Host "Time: $(Get-Date -Format o)"

$s = J "$root/state/scheduler_state.json"
if ($s) {
    Write-Host "Mode: $($s.mode)  Parallel: $($s.max_parallel)"
    Write-Host "Queues: InFlight=$($s.in_flight.Count) Ready=$($s.ready_queue.Count) Blocked=$($s.blocked.Count) Done=$($s.completed.Count)"
}

$l = J "$root/state/orchestrator.lock"
if ($l) {
    $age = ([DateTimeOffset]::UtcNow - [DateTimeOffset]::Parse($l.heartbeat)).TotalMinutes
    Write-Host "Lock: HELD pid=$($l.pid) age=$([math]::Round($age,1))m"
} else { Write-Host "Lock: NONE" }

$h = @{}
foreach ($d in "claimed","done","blocked","archived") {
    $h[$d] = (Get-ChildItem "$root/handoffs/$d" -File -EA 0 | ? Name -ne '.gitkeep').Count
}
Write-Host "Handoffs: claimed=$($h.claimed) done=$($h.done) blocked=$($h.blocked) archived=$($h.archived)"

$i = @{}
foreach ($r in "grok-cursor","claude-code","local-script") {
    $i[$r] = (Get-ChildItem "$root/inbox/$r/raw" -File -EA 0 | ? Name -ne '.gitkeep').Count
}
Write-Host "InboxRaw: claude=$($i['claude-code']) local=$($i['local-script'])"

Write-Host "`nRecent Decisions (tail):"
Get-Content "$root/logs/decisions.md" -Tail 6 -EA 0 | ? {$_ -and -not $_.StartsWith('#')} | % { "  $_" }

Write-Host "`nCollab Branches:"
git branch --list '*gloam*' 2>$null | % { "  $_" }

Write-Host "Worktrees: $((git worktree list 2>$null).Count)"

$rstate = J "$root/state/run_state.json"
if ($rstate) {
    Write-Host "RunState: $($rstate.run_id) ticks=$($rstate.ticks) tasks=$($rstate.tasks_completed) status=$($rstate.status)"
}

$apol = J "$root/../context/autonomy_policy.json"  # context relative
if (-not $apol) { $apol = J "$root/context/autonomy_policy.json" }
if ($apol) {
    Write-Host "Autonomy: level=$($apol.autonomy_level) max_tasks=$($apol.cumulative.max_tasks_per_run)"
}

Write-Host "=== End ===" -ForegroundColor Cyan
