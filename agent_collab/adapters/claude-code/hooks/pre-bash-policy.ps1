#!/usr/bin/env pwsh
# PreToolUse hook: run Assert-BashPolicy.ps1 on Bash tool commands (exit 2 blocks).
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$raw = [Console]::In.ReadToEnd()
if (-not $raw) { exit 0 }

try { $input = $raw | ConvertFrom-Json } catch { exit 0 }
if ($input.tool_name -ne 'Bash') { exit 0 }

$command = $input.tool_input.command
if (-not $command) { exit 0 }

$projectDir = $env:CLAUDE_PROJECT_DIR
if (-not $projectDir) { $projectDir = (git rev-parse --show-toplevel 2>$null).Trim() }
if (-not $projectDir) { exit 0 }

$policy = Join-Path $projectDir 'agent_collab/scripts/Assert-BashPolicy.ps1'
& pwsh -NoProfile -File $policy -Command $command 2>&1 | ForEach-Object { Write-Error $_ }
exit $LASTEXITCODE