#requires -Version 7
<#
.SYNOPSIS
    Aggregate verification for the Gloamstead shell-policy guard. One entry point, fail-closed.

.DESCRIPTION
    Runs every suite that verifies the command-policy guard, and proves two properties the
    individual suites cannot prove about themselves:

      1. DETERMINISM at suite level (R-CORE-3). Every suite runs twice; both passes must agree.
      2. NO DURABLE STATE MUTATION (R-VER-6). Tracked files under agent_collab/state/ are
         SHA256-fingerprinted before the first pass and re-checked after each pass. Any drift
         fails the run. This follows the pattern established by commit c9d4455 for
         Test-AgentCollabScaffold.ps1 — the fingerprint is the only part that actually guarantees
         byte-identity, because it catches writers nobody anticipated, including ones added later.

    Wired into gate.ps1 (the repo's mandatory verification path, per AGENTS.md) BEFORE the UE build,
    so a classifier regression is caught in seconds rather than after a ten-minute link.

.NOTES
    NO -Strict ESCAPE HATCH, deliberately. Some GloamsteadForge validators accept a -Strict switch
    that downgrades failure to exit 0. A security guard's own test suite must not have that
    affordance: Test-GloamsteadForgeNegatives.ps1 sets the precedent ("negatives always fail
    closed"). A missing suite file is a FAILURE, not a skip — an absent security test is the one
    thing that most resembles a passing one.

    This script writes nothing outside the OS temp directory.

.EXAMPLE
    pwsh -NoProfile -File agent_collab/scripts/Test-ShellGuard.ps1
#>
[CmdletBinding()]
param(
    # Repo root. Defaults to two levels up from this script (agent_collab/scripts -> repo root).
    [string]$RepoRoot,

    # Corpus for the classification runner.
    [string]$CorpusPath,

    # Normative spec, used by the runner for per-rule coverage reporting.
    [string]$SpecPath,

    # How many times to run the whole set. 2 proves suite-level determinism; 1 is for quick local
    # iteration only and weakens the guarantee.
    [ValidateRange(1, 5)]
    [int]$Passes = 2,

    # Print full suite output even when a suite passes.
    [switch]$ShowOutput
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $RepoRoot) { $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..' '..')).Path }
if (-not $CorpusPath) { $CorpusPath = Join-Path $RepoRoot 'agent_collab/tests/command-policy-smoke.jsonl' }
if (-not $SpecPath) { $SpecPath = Join-Path $RepoRoot 'agent_collab/context/command-policy-spec.md' }

$scriptsDir = Join-Path $RepoRoot 'agent_collab/scripts'
$stateRoot  = Join-Path $RepoRoot 'agent_collab/state'
$pwshExe    = Join-Path $PSHOME 'pwsh.exe'
if (-not (Test-Path -LiteralPath $pwshExe)) { $pwshExe = 'pwsh' }

$failures = [System.Collections.Generic.List[string]]::new()
function Fail([string]$m) { $failures.Add($m); Write-Host "FAIL: $m" -ForegroundColor Red }
function Pass([string]$m) { Write-Host "PASS: $m" -ForegroundColor Green }
function Note([string]$m) { Write-Host "      $m" -ForegroundColor DarkGray }

# --- Durable-state fingerprint (R-VER-6) -----------------------------------------------------
# Recursive and keyed on relative path: agent_collab/state/ is flat today, but a non-recursive
# scan would silently miss a subdirectory added later, which is exactly when this guard matters.
function Get-StateFingerprints {
    $map = @{}
    if (-not (Test-Path -LiteralPath $stateRoot)) { return $map }
    $root = (Resolve-Path $stateRoot).Path
    Get-ChildItem -LiteralPath $stateRoot -Recurse -File | Sort-Object FullName | ForEach-Object {
        # orchestrator.lock is gitignored volatile runtime state (.gitignore:40) and is expected
        # to change; every other file under state/ is tracked and must not move.
        if ($_.Name -eq 'orchestrator.lock') { return }
        $rel = $_.FullName.Substring($root.Length + 1)
        $map[$rel] = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }
    return $map
}

function Compare-Fingerprints {
    param([hashtable]$Before, [hashtable]$After, [string]$Label)
    $drift = [System.Collections.Generic.List[string]]::new()
    foreach ($k in $Before.Keys) {
        if (-not $After.ContainsKey($k)) { $drift.Add("deleted: $k") }
        elseif ($After[$k] -ne $Before[$k]) { $drift.Add("modified: $k") }
    }
    foreach ($k in $After.Keys) {
        if (-not $Before.ContainsKey($k)) { $drift.Add("created: $k") }
    }
    if ($drift.Count -gt 0) {
        Fail "$Label mutated durable state: $($drift -join ', ')"
        return $false
    }
    return $true
}

# --- Suite registry ---------------------------------------------------------------------------
# Adding a suite here is the whole registration step; gate.ps1 calls this script, not the suites.
$suites = @(
    @{
        Name = 'classification-corpus'
        File = 'Test-CommandPolicy.ps1'
        Args = @('-CorpusPath', $CorpusPath, '-SpecPath', $SpecPath)
        Why  = 'Allow/Ask/Deny semantics + per-rule spec coverage'
    },
    @{
        Name = 'fuzz-properties'
        File = 'Test-CommandPolicyFuzz.ps1'
        Args = @()
        Why  = 'No crashes, deterministic, no payload false positives'
    },
    @{
        Name = 'live-hook'
        File = 'Test-BashPolicyHook.ps1'
        Args = @()
        Why  = 'Real PreToolUse stdin -> emitted permissionDecision'
    },
    @{
        Name = 'policy-structure'
        File = 'Test-PolicyStructure.ps1'
        Args = @()
        Why  = 'R-PROT-1 single declaration, R-PROT-4 no lexer edit, R-VER-5 mandatory-path wiring'
    }
)

Write-Host ''
Write-Host 'SHELL GUARD VERIFICATION' -ForegroundColor Cyan
Write-Host ('-' * 78)
Note "repo    : $RepoRoot"
Note "corpus  : $CorpusPath"
Note "spec    : $SpecPath"
Note "passes  : $Passes"
Write-Host ''

# A declared suite that does not exist is a failure. Fail closed.
foreach ($s in $suites) {
    $p = Join-Path $scriptsDir $s.File
    if (-not (Test-Path -LiteralPath $p)) {
        Fail "declared suite missing: agent_collab/scripts/$($s.File) — a security suite that is absent must not read as green"
    }
}

$stateBefore = Get-StateFingerprints
Note "state fingerprint: $($stateBefore.Count) tracked file(s) under agent_collab/state/"
Write-Host ''

$verdictsByPass = @{}

if ($failures.Count -eq 0) {
    for ($pass = 1; $pass -le $Passes; $pass++) {
        Write-Host "--- pass $pass of $Passes ---" -ForegroundColor Cyan
        $passVerdicts = @{}

        foreach ($s in $suites) {
            $p = Join-Path $scriptsDir $s.File
            $output = & $pwshExe -NoProfile -File $p @($s.Args) 2>&1
            $code = $LASTEXITCODE
            $passVerdicts[$s.Name] = $code

            if ($code -eq 0) {
                Pass "$($s.Name) — $($s.Why)"
                if ($ShowOutput) { $output | ForEach-Object { Note $_ } }
            }
            else {
                Fail "$($s.Name) exited $code"
                # Print the suite's own report on failure: it names the failing rows, which is the
                # information a reviewer actually needs.
                $output | ForEach-Object { Write-Host "      $_" -ForegroundColor DarkYellow }
            }
        }

        $stateAfter = Get-StateFingerprints
        [void](Compare-Fingerprints -Before $stateBefore -After $stateAfter -Label "pass $pass")
        $verdictsByPass[$pass] = $passVerdicts
        Write-Host ''
    }

    # Suite-level determinism: identical verdicts across passes.
    if ($Passes -gt 1) {
        $stable = $true
        foreach ($s in $suites) {
            $seen = @($verdictsByPass.Keys | Sort-Object | ForEach-Object { $verdictsByPass[$_][$s.Name] } | Select-Object -Unique)
            if ($seen.Count -ne 1) {
                Fail "$($s.Name) is NON-DETERMINISTIC across passes (exit codes: $($seen -join ', '))"
                $stable = $false
            }
        }
        if ($stable) { Pass "all suites returned identical verdicts across $Passes passes (determinism)" }
    }
}

Write-Host ('-' * 78)
if ($failures.Count -eq 0) {
    Write-Host "SHELL GUARD PASS: $($suites.Count) suite(s) green x$Passes, durable state unchanged" -ForegroundColor Green
    exit 0
}
Write-Host "SHELL GUARD FAIL: $($failures.Count) failure(s)" -ForegroundColor Red
$failures | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
exit 1
