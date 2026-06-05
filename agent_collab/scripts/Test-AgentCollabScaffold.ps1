#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Self-test for the Gloamstead agent_collab v8.1 scaffold.
    Must pass cleanly before any application task is started.

    Run: pwsh -NoProfile -File agent_collab/scripts/Test-AgentCollabScaffold.ps1
#>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (git rev-parse --show-toplevel).Trim()
Set-Location $repoRoot

$failures = 0
function Fail([string]$msg) {
    Write-Error "FAIL: $msg"
    $script:failures++
}
function Pass([string]$msg) { Write-Output "PASS: $msg" }

Write-Output "=== Gloamstead Collaboration Scaffold Self-Test (slug=gloam) ==="

# 1. Directory structure
$requiredDirs = @(
    "agent_collab/context",
    "agent_collab/registry",
    "agent_collab/protocol",
    "agent_collab/adapters/claude-code/agents",
    "agent_collab/adapters/claude-code/commands",
    "agent_collab/adapters/local-script",
    "agent_collab/adapters/grok-cursor",
    "agent_collab/inbox/grok-cursor/raw",
    "agent_collab/inbox/claude-code/raw",
    "agent_collab/inbox/local-script/raw",
    "agent_collab/handoffs/claimed",
    "agent_collab/handoffs/done",
    "agent_collab/handoffs/archived",
    "agent_collab/handoffs/blocked",
    "agent_collab/outbox/architect",
    "agent_collab/outbox/planner",
    "agent_collab/outbox/researcher",
    "agent_collab/outbox/coder",
    "agent_collab/outbox/critic",
    "agent_collab/outbox/documentor",
    "agent_collab/outbox/integration",
    "agent_collab/state",
    "agent_collab/logs",
    "agent_collab/scripts",
    ".claude/agents",
    ".claude/commands",
    ".claude/worktrees",
    ".grok/skills"
)
foreach ($d in $requiredDirs) {
    if (-not (Test-Path $d -PathType Container)) { Fail "Missing required dir: $d" } else { Pass "Dir exists: $d" }
}

# 2. Key files exist
$requiredFiles = @(
    "agent_collab/context/project_goal.md",
    "agent_collab/context/agent_rules.md",
    "agent_collab/context/environment.md",
    "agent_collab/context/scope_roots.json",
    "agent_collab/context/command_policy.json",
    "agent_collab/context/restart_instructions.md",
    "agent_collab/registry/runtimes.json",
    "agent_collab/registry/agents.json",
    "agent_collab/registry/capabilities.json",
    "agent_collab/registry/routing.json",
    "agent_collab/protocol/handoff.schema.json",
    "agent_collab/protocol/planner_output.schema.json",
    "agent_collab/protocol/worker_request.schema.json",
    "agent_collab/protocol/worker_summary.schema.json",
    "agent_collab/protocol/critic_verdict.schema.json",
    "agent_collab/protocol/task_state.schema.json",
    "agent_collab/protocol/scheduler_state.schema.json",
    "agent_collab/protocol/lease.schema.json",
    "agent_collab/adapters/claude-code/settings.json",
    "agent_collab/adapters/claude-code/launcher.md",
    "agent_collab/adapters/claude-code/onboarding.md",
    "agent_collab/adapters/local-script/runner_config.json",
    "agent_collab/adapters/local-script/launcher.md",
    "agent_collab/adapters/_EXTENSION_CONTRACT.md",
    "agent_collab/handoffs/HANDOFF_TEMPLATE.md",
    "agent_collab/state/task_state.json",
    "agent_collab/state/scheduler_state.json",
    "agent_collab/state/leases.json",
    "agent_collab/state/orchestrator.lock.example",
    "agent_collab/logs/orchestrator.log",
    "agent_collab/logs/decisions.md",
    "agent_collab/scripts/Acquire-OrchestratorLock.ps1",
    "agent_collab/scripts/Release-OrchestratorLock.ps1",
    "agent_collab/scripts/Assert-EditScope.ps1",
    "agent_collab/scripts/Assert-BashPolicy.ps1",
    "agent_collab/scripts/Validate-JsonSchema.ps1",
    "agent_collab/scripts/Build-WavePlan.ps1",
    "agent_collab/scripts/Normalize-RuntimeResult.ps1",
    "agent_collab/scripts/Test-AgentCollabScaffold.ps1",
    "agent_collab/scripts/Project-ClaudeAdapter.ps1",
    "agent_collab/scripts/Refresh-OrchestratorLock.ps1",
    "agent_collab/scripts/Select-Runtime.ps1",
    "agent_collab/adapters/claude-code/agents/gloam-orchestrator.md",
    "agent_collab/adapters/claude-code/hooks/pre-edit-scope.ps1",
    "agent_collab/adapters/claude-code/hooks/pre-bash-policy.ps1",
    "agent_collab/adapters/grok-cursor/runner_config.json",
    "agent_collab/adapters/grok-cursor/launcher.md",
    "agent_collab/adapters/grok-cursor/onboarding.md",
    "agent_collab/adapters/grok-cursor/result_contract.md",
    "agent_collab/adapters/grok-cursor/skills/gloam-resume/SKILL.md",
    "agent_collab/adapters/grok-cursor/agents/grok-orchestrator.md",
    "agent_collab/context/autonomy_policy.json",
    "agent_collab/context/project.json",
    "agent_collab/state/run_state.json",
    "agent_collab/state/status.json",
    "agent_collab/scripts/Project-GrokAdapter.ps1",
    "agent_collab/scripts/New-TaskWorktree.ps1",
    "agent_collab/scripts/Write-InboxRaw.ps1",
    "AGENTS.md"
)
foreach ($f in $requiredFiles) {
    if (-not (Test-Path $f -PathType Leaf)) { Fail "Missing required file: $f" } else { Pass "File exists: $f" }
}

# 3. Only expected adapters are scaffolded (no speculative runtimes)
$adapterDirs = Get-ChildItem "agent_collab/adapters" -Directory | Where-Object { $_.Name -notlike '.*' } | Select-Object -Expand Name
$expectedAdapters = @('claude-code', 'local-script', 'grok-cursor')
$unexpected = $adapterDirs | Where-Object { $_ -notin $expectedAdapters -and $_ -ne '_EXTENSION_CONTRACT.md' }
if ($unexpected) { Fail "Unexpected adapter directories present: $($unexpected -join ', ')" } else { Pass "Expected adapters present: $($expectedAdapters -join ', ')" }

# 4. Registries and state parse as JSON + pass their schemas
$schemas = @{
    "agent_collab/state/task_state.json" = "agent_collab/protocol/task_state.schema.json"
    "agent_collab/state/scheduler_state.json" = "agent_collab/protocol/scheduler_state.schema.json"
    "agent_collab/registry/runtimes.json" = $null
    "agent_collab/registry/agents.json" = $null
}

foreach ($file in $schemas.Keys) {
    $schema = $schemas[$file]
    if ($schema) {
        $result = & pwsh -NoProfile -File "agent_collab/scripts/Validate-JsonSchema.ps1" -Path $file -SchemaFile $schema 2>&1
        if ($LASTEXITCODE -ne 0) { Fail "Schema validation failed for $file : $result" } else { Pass "Schema valid: $file" }
    } else {
        try {
            Get-Content -Raw $file | ConvertFrom-Json | Out-Null
            Pass "Parses as JSON: $file"
        } catch { Fail "JSON parse failed for $file : $_" }
    }
}

# 5. Claude-only profile derivable (all enabled claude-code templates)
$agents = Get-Content -Raw "agent_collab/registry/agents.json" | ConvertFrom-Json
$claudeOnly = $agents.templates | Where-Object { $_.enabled -and $_.runtime -eq 'claude-code' }
$grokOnly = $agents.templates | Where-Object { $_.enabled -and $_.runtime -eq 'grok-cursor' }
if ($claudeOnly.Count -lt 1) { Fail "No enabled claude-code templates found" } else { Pass "Claude profile derivable: $($claudeOnly.id -join ', ')" }
if ($grokOnly.Count -lt 1) { Fail "No enabled grok-cursor templates found" } else { Pass "Grok profile derivable: $($grokOnly.id -join ', ')" }

# 6. worktree.baseRef == "head" in settings
$settings = Get-Content -Raw "agent_collab/adapters/claude-code/settings.json" | ConvertFrom-Json
if ($settings.worktree.baseRef -ne "head") { Fail "worktree.baseRef must be 'head', got '$($settings.worktree.baseRef)'" } else { Pass "worktree.baseRef=head present in claude-code settings" }

# 7. Scope guard: blocks out-of-scope, allows in-scope
# Create a temp file inside Source/ and one outside
$testSrc = "Source/Gloamstead/TempScaffoldTest.cpp"
$testBad = "agent_collab/TempScaffoldTestBad.md"  # under collab but not in coder_edit_roots for coder role test

New-Item -ItemType File -Path $testSrc -Force | Out-Null
"// temp test file for scope guard" | Set-Content $testSrc

# Allowed (coder_edit_roots = ["Source"])
$scopeResult = & pwsh -NoProfile -File "agent_collab/scripts/Assert-EditScope.ps1" -TargetFile $testSrc -AllowedRoots @("Source") 2>&1
if ($LASTEXITCODE -eq 0) { Pass "Scope guard allows valid Source/ edit" } else { Fail "Scope guard should allow Source/ edit: $scopeResult" }

# Blocked (trying to edit under agent_collab from a coder perspective)
$badResult = & pwsh -NoProfile -File "agent_collab/scripts/Assert-EditScope.ps1" -TargetFile $testBad -AllowedRoots @("Source") 2>&1
if ($LASTEXITCODE -eq 2) { Pass "Scope guard blocks out-of-scope edit (exit 2)" } else { Fail "Scope guard should block with exit 2 for collab edit (got $LASTEXITCODE): $badResult" }

Remove-Item $testSrc -Force -ErrorAction SilentlyContinue
Remove-Item $testBad -Force -ErrorAction SilentlyContinue

# 8. Bash policy blocks dangerous command
$policyResult = & pwsh -NoProfile -File "agent_collab/scripts/Assert-BashPolicy.ps1" -Command "git push origin main" 2>&1
if ($LASTEXITCODE -eq 2) { Pass "Bash policy blocks dangerous git push" } else { Fail "Bash policy should block git push (exit 2)" }

# 9. Lock acquire + release
$lockTest = & pwsh -NoProfile -File "agent_collab/scripts/Acquire-OrchestratorLock.ps1" -Slug "gloam-test" 2>&1
if ($LASTEXITCODE -eq 0) { Pass "Lock acquire succeeded" } else { Fail "Lock acquire failed: $lockTest" }

$releaseTest = & pwsh -NoProfile -File "agent_collab/scripts/Release-OrchestratorLock.ps1" -Slug "gloam-test" 2>&1
if ($LASTEXITCODE -eq 0) { Pass "Lock release succeeded" } else { Fail "Lock release failed: $releaseTest" }

# 10. .gitignore excludes .claude/worktrees/ + .grok/worktrees/ ; agent_collab/ core is NOT blanket-ignored (volatile subs ok)
$gi = Get-Content ".gitignore" -Raw
if ($gi -match '\.claude/worktrees/') { Pass ".gitignore excludes .claude/worktrees/" } else { Fail ".gitignore missing .claude/worktrees/ exclusion" }
if ($gi -match '\.grok/worktrees/') { Pass ".gitignore excludes .grok/worktrees/" } else { Fail ".gitignore missing .grok/worktrees/ exclusion" }
# Accept if no top-level blanket "agent_collab/" or "agent_collab/*" (but sub volatile like inbox/*/raw/ are permitted)
if ($gi -match '(^|\n)agent_collab/\s*$' -or $gi -match '(^|\n)agent_collab/\*') {
  Fail ".gitignore has blanket agent_collab/ ignore (core protocol must be committed)"
} else { Pass "agent_collab/ core not blanket ignored (volatile subs permitted, correct)" }

# 11. No generic agent filenames (sanity)
$generic = Get-ChildItem "agent_collab/adapters/claude-code/agents" -Filter "*.md" | Where-Object { $_.Name -in @('coder.md','planner.md','critic.md','orchestrator.md') }
if ($generic) { Fail "Generic agent filenames found (use gloam- prefixed): $($generic.Name -join ', ')" } else { Pass "No generic agent filenames (all project-prefixed)" }

# 12. .claude/ projection exists (basic files copied or present)
$projChecks = @(
    ".claude/settings.json",
    ".claude/agents/gloam-coder.md",
    ".claude/agents/gloam-orchestrator.md",
    ".claude/commands/gloam-resume.md",
    ".claude/hooks/pre-edit-scope.ps1"
)
foreach ($p in $projChecks) {
    if (Test-Path $p) { Pass "Projection present: $p" } else { Fail "Missing .claude projection: $p (run agent_collab/scripts/Project-ClaudeAdapter.ps1)" }
}

# 13. Projection script runs cleanly
$projRun = & pwsh -NoProfile -File "agent_collab/scripts/Project-ClaudeAdapter.ps1" 2>&1
if ($LASTEXITCODE -eq 0) { Pass "Project-ClaudeAdapter.ps1 executed successfully" } else { Fail "Project-ClaudeAdapter.ps1 failed: $projRun" }

# 14. Select-Runtime routing (Coder + edit_code safety floor)
$routeResult = & pwsh -NoProfile -File "agent_collab/scripts/Select-Runtime.ps1" -RequiredCapabilities edit_code -Role Coder -SafetyFloor edit_code 2>&1
if ($LASTEXITCODE -eq 0) {
    $route = $routeResult | ConvertFrom-Json
    if ($route.chosen_runtime -eq 'grok-cursor') { Pass "Select-Runtime chose grok-cursor for Coder/edit_code (preference rank)" }
    else { Fail "Select-Runtime expected grok-cursor for Coder, got: $($route.chosen_runtime)" }
} else { Fail "Select-Runtime failed for Coder: $routeResult" }

# 15. run_tests Researcher routes to local-script (bounded runner)
$routeResearch = & pwsh -NoProfile -File "agent_collab/scripts/Select-Runtime.ps1" -RequiredCapabilities run_tests -Role Researcher 2>&1
if ($LASTEXITCODE -eq 0) {
    $rr = $routeResearch | ConvertFrom-Json
    if ($rr.chosen_runtime -eq 'local-script') { Pass "Select-Runtime chose local-script for run_tests Researcher" }
    else { Fail "Expected local-script for run_tests Researcher, got $($rr.chosen_runtime)" }
} else { Fail "Select-Runtime failed for Researcher run_tests: $routeResearch" }

# 16. Grok projection
$grokProj = & pwsh -NoProfile -File "agent_collab/scripts/Project-GrokAdapter.ps1" 2>&1
if ($LASTEXITCODE -eq 0) { Pass "Project-GrokAdapter.ps1 executed successfully" } else { Fail "Project-GrokAdapter.ps1 failed: $grokProj" }
if (Test-Path ".grok/skills/gloam-resume/SKILL.md") { Pass "Grok skill projected: gloam-resume" } else { Fail "Missing .grok/skills/gloam-resume/SKILL.md" }

Write-Output ""
if ($failures -eq 0) {
    Write-Output "=== ALL TESTS PASSED ==="
    exit 0
} else {
    Write-Error "=== $failures TEST(S) FAILED ==="
    exit 1
}
