#!/usr/bin/env pwsh
# PreToolUse hook: enforce role edit roots via Assert-EditScope.ps1 (exit 2 blocks the tool call).
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$raw = [Console]::In.ReadToEnd()
if (-not $raw) { exit 0 }

try { $input = $raw | ConvertFrom-Json } catch { exit 0 }

if ($input.tool_name -notin @('Edit', 'Write')) { exit 0 }

$filePath = $input.tool_input.file_path
if (-not $filePath) { exit 0 }

$projectDir = $env:CLAUDE_PROJECT_DIR
if (-not $projectDir) {
    $projectDir = (git rev-parse --show-toplevel 2>$null).Trim()
}
if (-not $projectDir) { exit 0 }

$scopeFile = Join-Path $projectDir 'agent_collab/context/scope_roots.json'
$scope = Get-Content -Raw $scopeFile | ConvertFrom-Json

$agentType = $input.agent_type
$allowed = @()

switch -Wildcard ($agentType) {
    'gloam-coder' { $allowed = @($scope.coder_edit_roots) }
    'gloam-documentor' { $allowed = @($scope.documentor_edit_roots) }
    'gloam-orchestrator' { $allowed = @($scope.orchestrator_edit_roots) }
    'gloam-planner' { $allowed = @() }
    'gloam-researcher' { $allowed = @() }
    'gloam-critic' { $allowed = @() }
    'gloam-architect' { $allowed = @() }
    default {
        if ($agentType) { $allowed = @() }
        else { $allowed = @($scope.orchestrator_edit_roots) }
    }
}

$forbidden = @($scope.forbidden_roots)
$guard = Join-Path $projectDir 'agent_collab/scripts/Assert-EditScope.ps1'

& pwsh -NoProfile -File $guard -TargetFile $filePath -AllowedRoots $allowed -ForbiddenRoots $forbidden 2>&1 | ForEach-Object { Write-Error $_ }
exit $LASTEXITCODE