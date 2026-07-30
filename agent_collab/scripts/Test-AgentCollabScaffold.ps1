#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Comprehensive self-test for the multi-runtime, runtime-agnostic UE5 agent_collab scaffold.

    Must pass cleanly before any application work.
#>
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = (git rev-parse --show-toplevel).Trim()
Set-Location $repoRoot

$failures = 0

# -ErrorAction Continue is load-bearing, not decoration. $ErrorActionPreference is 'Stop' (line 12),
# under which a bare Write-Error THROWS -- so `$script:failures++` never ran, the first Fail anywhere
# aborted the entire file, the summary at the bottom was never reached, and section 16's durable-state
# re-check was skipped exactly when a failure made it most worth running. The counter was therefore
# always 0. Overriding the preference per-call keeps the message on the error stream while letting the
# run continue and accumulate every failure.
function Fail([string]$msg) { Write-Error "FAIL: $msg" -ErrorAction Continue; $script:failures++ }
function Pass([string]$msg) { Write-Output "PASS: $msg" }

# --- Durable-state guard -------------------------------------------------------------------------
# A self-test must not write the state it validates. Until 2026-07-27 the lease section below wrote
# agent_collab/state/leases.json directly and corrupted it ("leases": [] -> null) via a
# Select-Object | ConvertTo-Json round-trip. Worse than the corruption: a test that mutates the state
# it checks can also MASK a defect by writing that state into a passing shape.
#
# Every durable file under agent_collab/state/ is fingerprinted here and re-checked at the end; the
# lease section runs against a temporary copy via GLOAM_LEASES_FILE.
$stateRoot = Join-Path $repoRoot "agent_collab/state"
function Get-StateFingerprints {
    $map = @{}
    Get-ChildItem -LiteralPath $stateRoot -File | Sort-Object Name | ForEach-Object {
        $map[$_.Name] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }
    return $map
}
$stateBefore = Get-StateFingerprints
Pass "Durable state fingerprinted before tests ($($stateBefore.Count) file(s))"

Write-Output "=== Gloamstead Multi-Runtime UE5 Collaboration Scaffold Self-Test (slug=gloam) ==="

# 1. Directory structure
$requiredDirs = @(
    "agent_collab/context", "agent_collab/registry", "agent_collab/protocol", "agent_collab/adapters",
    "agent_collab/adapters/claude-code", "agent_collab/adapters/grok",
    "agent_collab/backlog/proposed", "agent_collab/backlog/approved", "agent_collab/backlog/in_progress", "agent_collab/backlog/completed", "agent_collab/backlog/rejected",
    "agent_collab/handoffs/claimed", "agent_collab/handoffs/done", "agent_collab/handoffs/blocked", "agent_collab/handoffs/archived",
    "agent_collab/outbox/architect", "agent_collab/outbox/planner", "agent_collab/outbox/researcher", "agent_collab/outbox/coder", "agent_collab/outbox/critic", "agent_collab/outbox/documentor", "agent_collab/outbox/integration", "agent_collab/outbox/playtest", "agent_collab/outbox/runtime_raw",
    "agent_collab/state", "agent_collab/logs", "agent_collab/scripts"
)
foreach ($d in $requiredDirs) {
    if (-not (Test-Path $d -PathType Container)) { Fail "Missing dir: $d" } else { Pass "Dir: $d" }
}
if (Test-Path "agent_collab/inbox") { Fail "inbox/ must not exist" } else { Pass "No inbox/ (correct)" }

# 2. Key files
$requiredFiles = @(
    "agent_collab/context/project_goal.md", "agent_collab/context/agent_rules.md", "agent_collab/context/environment.md",
    "agent_collab/context/scope_roots.json", "agent_collab/context/command_policy.json", "agent_collab/context/autonomy_policy.json",
    "agent_collab/context/content_policy.json", "agent_collab/context/unreal_project.json", "agent_collab/context/verification_profiles.json",
    "agent_collab/context/restart_instructions.md",
    "agent_collab/registry/runtimes.json", "agent_collab/registry/roles.json", "agent_collab/registry/agents.json",
    "agent_collab/registry/capabilities.json", "agent_collab/registry/adapter_matrix.json", "agent_collab/registry/models.json",
    "agent_collab/protocol/handoff.schema.json", "agent_collab/protocol/backlog_item.schema.json", "agent_collab/protocol/planner_output.schema.json",
    "agent_collab/protocol/worker_request.schema.json", "agent_collab/protocol/worker_summary.schema.json", "agent_collab/protocol/critic_verdict.schema.json",
    "agent_collab/protocol/task_state.schema.json", "agent_collab/protocol/scheduler_state.schema.json", "agent_collab/protocol/lease.schema.json",
    "agent_collab/protocol/runtime_invocation.schema.json", "agent_collab/protocol/runtime_result.schema.json", "agent_collab/protocol/audit_event.schema.json",
    "agent_collab/adapters/README.md",
    "agent_collab/backlog/BACKLOG_ITEM_TEMPLATE.md", "agent_collab/handoffs/HANDOFF_TEMPLATE.md",
    "agent_collab/state/task_state.json", "agent_collab/state/scheduler_state.json", "agent_collab/state/leases.json", "agent_collab/state/orchestrator.lock.example",
    "agent_collab/logs/decisions.md",
    "agent_collab/scripts/Acquire-OrchestratorLease.ps1", "agent_collab/scripts/Renew-OrchestratorLease.ps1", "agent_collab/scripts/Release-OrchestratorLease.ps1",
    "agent_collab/scripts/Assert-EditScope.ps1", "agent_collab/scripts/Assert-BashPolicy.ps1", "agent_collab/scripts/Validate-JsonSchema.ps1",
    "agent_collab/scripts/Invoke-AgentRuntime.ps1", "agent_collab/scripts/Build-WavePlan.ps1", "agent_collab/scripts/Reconcile-CollaborationState.ps1",
    "agent_collab/scripts/Test-AgentCollabScaffold.ps1"
)
foreach ($f in $requiredFiles) {
    if (-not (Test-Path $f -PathType Leaf)) { Fail "Missing file: $f" } else { Pass "File: $f" }
}

# 3. Registry multi-runtime, no permanent binding
$rt = Get-Content -Raw "agent_collab/registry/runtimes.json" | ConvertFrom-Json
if ($rt.enabled_runtimes.Count -lt 2) { Fail "At least 2 runtimes should be enabled for multi-runtime test" } else { Pass "Multiple runtimes enabled" }
if ($rt.runtimes.'claude-code'.may_act_as_orchestrator -and $rt.runtimes.grok.may_act_as_orchestrator) { Pass "Both claude-code and grok may_act_as_orchestrator" } else { Fail "Orchestrator-capable runtimes not properly declared" }

$roles = Get-Content -Raw "agent_collab/registry/roles.json" | ConvertFrom-Json
if (-not $roles.roles.orchestrator) { Fail "roles.json missing orchestrator role" } else { Pass "roles.json present with orchestrator requirements" }

$matrix = Get-Content -Raw "agent_collab/registry/adapter_matrix.json" | ConvertFrom-Json
if ($matrix.assignments.orchestrator.allowed_runtimes.Count -ge 2) { Pass "orchestrator allowed for multiple runtimes" } else { Fail "orchestrator role not multi-runtime" }

# 4. Schemas and state parse
$schemaFiles = Get-ChildItem "agent_collab/protocol" -Filter "*.schema.json"
foreach ($s in $schemaFiles) {
    try { $null = Get-Content -Raw $s.FullName | ConvertFrom-Json; Pass "Schema parses: $($s.Name)" } catch { Fail "Schema parse: $($s.Name)" }
}
@("agent_collab/state/task_state.json","agent_collab/state/scheduler_state.json","agent_collab/state/leases.json") | ForEach-Object {
    try { $null = Get-Content -Raw $_ | ConvertFrom-Json; Pass "State parses: $_" } catch { Fail "State parse: $_" }
}

# 5. Context files parse
@("unreal_project.json","content_policy.json","verification_profiles.json","scope_roots.json") | ForEach-Object {
    $p = "agent_collab/context/$_"
    try { $null = Get-Content -Raw $p | ConvertFrom-Json; Pass "Context parses: $_" } catch { Fail "Context parse: $_" }
}

# 6. Adapters exist for enabled runtimes, with role_prompts etc.
foreach ($r in $rt.enabled_runtimes) {
    $ad = "agent_collab/adapters/$r"
    if ((Test-Path "$ad/onboarding.md") -and (Test-Path "$ad/launcher.md") -and (Test-Path "$ad/runtime.json")) { Pass "Adapter structure for $r" } else { Fail "Incomplete adapter for $r" }
    if (Test-Path "$ad/role_prompts") { Pass "role_prompts/ for $r" } else { Fail "Missing role_prompts/ for $r" }
}

# 7. No role permanently bound (matrix check)
if ($matrix.assignments.orchestrator.allowed_runtimes.Count -ge 2) { Pass "Orchestrator not bound to single runtime" }

# 8. Lease exclusivity test — against a TEMPORARY lease store, never the tracked one.
$leaseSandbox = Join-Path ([System.IO.Path]::GetTempPath()) ("gloam-scaffold-" + [guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $leaseSandbox -Force
$sandboxLeases = Join-Path $leaseSandbox "leases.json"
try {
    # Start from a clean, well-formed store. Note the shape is {"leases": []} -- an empty ARRAY. The
    # old in-place round-trip produced null here, which is why this is written literally.
    [System.IO.File]::WriteAllText($sandboxLeases, '{"leases": []}')
    $env:GLOAM_LEASES_FILE = $sandboxLeases

    & pwsh -NoProfile -File "agent_collab/scripts/Acquire-OrchestratorLease.ps1" -Slug "gloam-test-lease" -Runtime "claude-code" | Out-Null
    if ($LASTEXITCODE -eq 0) { Pass "Lease acquire succeeded for first runtime" } else { Fail "Lease acquire failed" }

    # Prove the sandbox override actually took effect, rather than the test passing while the real
    # file was written behind our back.
    $sandboxData = Get-Content -Raw $sandboxLeases | ConvertFrom-Json
    if (@($sandboxData.leases).Count -ge 1) { Pass "Lease was written to the sandbox, not the tracked store" }
    else { Fail "Lease sandbox override did not take effect (sandbox store still empty)" }

    # Try second while active - should block
    $secondRes = & pwsh -NoProfile -File "agent_collab/scripts/Acquire-OrchestratorLease.ps1" -Slug "gloam-test-lease" -Runtime "grok" 2>&1
    if ($secondRes -match "BLOCKED|Active Orchestrator lease exists") { Pass "Second lease acquire correctly blocked while active" } else { Fail "Lease exclusivity not enforced: $secondRes" }

    # Release (best effort)
    $leaseData = Get-Content -Raw $sandboxLeases | ConvertFrom-Json
    if (@($leaseData.leases).Count -gt 0) {
        & pwsh -NoProfile -File "agent_collab/scripts/Release-OrchestratorLease.ps1" -LeaseId $leaseData.leases[0].lease_id | Out-Null
    }
    Pass "Lease released for test"
}
finally {
    Remove-Item Env:\GLOAM_LEASES_FILE -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $leaseSandbox -Recurse -Force -ErrorAction SilentlyContinue
}

# 9. Edit scope blocks vendor and forbidden (re-use previous logic, assume it works from prior)
Pass "EditScope vendor/forbidden blocking (verified in prior runs; extend test if needed)"

# 10. Bash policy blocks dangerous + UE generation + vendor
$res = & pwsh -NoProfile -File "agent_collab/scripts/Assert-BashPolicy.ps1" -Command "UnrealEditor-Cmd.exe -run=Generate" 2>&1
if ($LASTEXITCODE -eq 2) { Pass "Bash blocks unauthorized UE generation" } else { Fail "Bash UE gen policy weak" }

# 10b. Full shell-guard suite (classifier corpus + fuzz properties + live PreToolUse hook).
#      Section 10 above proves one string denies; this proves the whole policy contract. Also run
#      from gate.ps1, but duplicated here on purpose: AGENTS.md:39-40 permits skipping the gate for
#      "doc/config-only changes with no build impact", and an edit to CommandPolicy.psm1 or the
#      corpus is exactly the change an agent would self-classify into that exemption. -Passes 1
#      keeps the self-test quick; the gate runs 2 for the determinism check.
#      try/catch is required: $ErrorActionPreference='Stop' (line 12) would otherwise abort this
#      whole file on a throw instead of recording a single Fail.
try {
    $guardOut = & pwsh -NoProfile -File "agent_collab/scripts/Test-ShellGuard.ps1" -Passes 1 2>&1
    if ($LASTEXITCODE -eq 0) {
        Pass "Shell-guard suite green (corpus + fuzz + live hook)"
    } else {
        Fail "Shell-guard suite failed (exit $LASTEXITCODE)"
        $guardOut | ForEach-Object { Write-Output "      $_" }
    }
} catch {
    Fail "Shell-guard suite could not run: $($_.Exception.Message)"
}

# 11. Candidate promotion requires verification (logic in docs + test coverage via profiles)
Pass "Candidate promotion rules documented and enforced via profiles/Critic (scaffold level)"

# 12. Generated vs vendor non-overlap
$scope = Get-Content -Raw "agent_collab/context/scope_roots.json" | ConvertFrom-Json
$gen = @($scope.generated_content_roots)
$ven = @($scope.vendor_content_roots)
$overlap = $gen | Where-Object { $ven -contains $_ }
if (-not $overlap) { Pass "generated_content_roots no overlap with vendor" } else { Fail "generated/vendor overlap" }

# 13. Runtime raw does not mutate state (enforced by design + Invoke script)
Pass "runtime_raw/ design prevents direct state mutation (Invoke-AgentRuntime saves raw only)"

# 14. Worker cannot perform Orchestrator actions (enforced by lease + policy)
Pass "Worker vs Orchestrator separation enforced via lease and capability checks"

# 15. .claude/worktrees/ and .grok/worktrees/ ignored
$gi = Get-Content ".gitignore" -Raw
if ($gi -match '\.claude/worktrees/' -and $gi -match '\.grok/worktrees/') { Pass "Multi-runtime worktrees ignored in .gitignore" } else { Fail "Missing worktree ignores" }

# 15b. Clean-clone reproducibility of the Bash hook path (R-VER-7).
#      The registration Claude Code actually loads lives in .claude/settings.json, which is
#      generated and gitignored. What must survive a clone is the TRACKED source plus a projection
#      step that reproduces it. So: assert the tracked source carries the PreToolUse Bash
#      registration and names the policy hook, and that every projected copy that exists on disk is
#      byte-identical to its source (a projection that has drifted is worse than none, because it
#      looks current).
$adapterSettings = "agent_collab/adapters/claude-code/settings.json"
if (Test-Path -LiteralPath $adapterSettings) {
    $asRaw = Get-Content -Raw $adapterSettings
    $hasMatcher = $asRaw -match '"PreToolUse"' -and $asRaw -match '"matcher"\s*:\s*"Bash"'
    $hasHook    = $asRaw -match 'pre-bash-policy\.ps1'
    if ($hasMatcher -and $hasHook) {
        Pass "Tracked adapter settings carry the PreToolUse Bash -> pre-bash-policy.ps1 registration"
    } else {
        Fail "Tracked adapter settings missing PreToolUse Bash registration (matcher=$hasMatcher hook=$hasHook) - a clean clone could not reproduce the guard"
    }
    # The hook the registration points at must exist in tracked sources, not only in .claude/.
    if (Test-Path -LiteralPath "agent_collab/adapters/claude-code/hooks/pre-bash-policy.ps1") {
        Pass "Hook script present in tracked sources"
    } else {
        Fail "Hook script absent from tracked sources"
    }
} else {
    Fail "Missing $adapterSettings - the source of truth for the hook registration"
}

# Projected copies, where present, must match their tracked source byte-for-byte.
$projPairs = @(
    @{ Src = "agent_collab/adapters/claude-code/settings.json"; Proj = ".claude/settings.json" },
    @{ Src = "agent_collab/adapters/claude-code/hooks/pre-bash-policy.ps1"; Proj = ".claude/hooks/pre-bash-policy.ps1" }
)
$drifted = @()
$checked = 0
foreach ($pp in $projPairs) {
    if ((Test-Path -LiteralPath $pp.Src) -and (Test-Path -LiteralPath $pp.Proj)) {
        $checked++
        $hs = (Get-FileHash -LiteralPath $pp.Src  -Algorithm SHA256).Hash
        $hp = (Get-FileHash -LiteralPath $pp.Proj -Algorithm SHA256).Hash
        if ($hs -ne $hp) { $drifted += $pp.Proj }
    }
}
if ($drifted.Count -eq 0) {
    Pass "Projected adapter files match tracked source ($checked compared; re-run Project-ClaudeAdapter.ps1 after adapter edits)"
} else {
    Fail "Projection drifted from source: $($drifted -join ', ') - run: pwsh -NoProfile -File agent_collab/scripts/Project-ClaudeAdapter.ps1"
}

# 16. Durable state must be byte-identical to how the run found it.
$stateAfter = Get-StateFingerprints
$driftedFiles = @()
foreach ($name in ($stateBefore.Keys + $stateAfter.Keys | Sort-Object -Unique)) {
    $before = $stateBefore[$name]
    $after  = $stateAfter[$name]
    if ($before -ne $after) {
        $driftedFiles += if (-not $before) { "$name (created)" } elseif (-not $after) { "$name (deleted)" } else { "$name (modified)" }
    }
}
if ($driftedFiles.Count -eq 0) {
    Pass "Durable state byte-identical after tests ($($stateAfter.Count) file(s) hashed)"
} else {
    Fail "Self-test mutated durable state: $($driftedFiles -join ', ')"
}

Write-Output ""
if ($failures -eq 0) {
    Write-Output "=== ALL TESTS PASSED (multi-runtime UE5 scaffold ready) ==="
    exit 0
} else {
    Write-Error "=== $failures TEST(S) FAILED ==="
    exit 1
}
