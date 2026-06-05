#!/usr/bin/env pwsh
# SubagentStart / SubagentStop audit hook (append one line to orchestrator.log).
Set-StrictMode -Version Latest

$raw = [Console]::In.ReadToEnd()
$event = 'unknown'
$agent = 'unknown'
$task = ''

if ($raw) {
    try {
        $input = $raw | ConvertFrom-Json
        $event = $input.hook_event_name
        $agent = if ($input.agent_type) { $input.agent_type } else { 'main' }
        if ($input.PSObject.Properties['task_id']) { $task = $input.task_id }
    } catch {}
}

$projectDir = $env:CLAUDE_PROJECT_DIR
if (-not $projectDir) { $projectDir = (git rev-parse --show-toplevel 2>$null).Trim() }
if (-not $projectDir) { exit 0 }

$log = Join-Path $projectDir 'agent_collab/logs/orchestrator.log'
$ts = [DateTimeOffset]::UtcNow.ToString('o')
Add-Content -Path $log -Value "[$ts] $event agent=$agent task=$task"
exit 0