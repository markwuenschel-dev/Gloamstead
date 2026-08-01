#requires -Version 7
<#
.SYNOPSIS
    Prove that a clean checkout reproduces the shell guard and its hook registration from tracked
    sources alone, and that the mandatory suites pass there.

.DESCRIPTION
    Answers R-VER-7 with evidence rather than argument. The adapter projections (.claude/, .grok/)
    are generated and gitignored, so "is the guard reproducible?" cannot be answered by looking at
    the working tree — the working tree has locally-projected files that a fresh clone would not.

    This script creates a DETACHED GIT WORKTREE at a chosen commit. A worktree contains exactly the
    tracked files at that commit and nothing else, which is precisely the clean-clone condition,
    without paying for a full clone of the repository's binary content. It then:

      1. asserts the projection targets are ABSENT (proving the hole is real, not assumed);
      2. asserts every executable link in the guard chain IS present from tracked sources;
      3. runs Project-ClaudeAdapter.ps1 inside the worktree;
      4. asserts .claude/settings.json now exists and is byte-identical to its tracked source, and
         carries the PreToolUse Bash registration naming pre-bash-policy.ps1;
      5. runs the full shell-guard suite inside the clean worktree — the "all mandatory tests pass
         from a committed-clean checkout" proof;
      6. removes the worktree.

    Uncommitted working-tree changes are invisible here BY DESIGN. If this passes on your working
    tree but you have not committed, it proves nothing about what a colleague would get.

.NOTES
    Run on demand (before a PR touching the guard), NOT from gate.ps1: creating and tearing down a
    worktree is far heavier than the text-only suites the gate runs on every invocation.

    Writes only inside the OS temp directory plus git's own .git/worktrees bookkeeping. Touches no
    tracked file in the primary work tree.

.EXAMPLE
    pwsh -NoProfile -File agent_collab/scripts/Test-CleanCloneReproducibility.ps1
.EXAMPLE
    pwsh -NoProfile -File agent_collab/scripts/Test-CleanCloneReproducibility.ps1 -Commit HEAD~1
#>
[CmdletBinding()]
param(
    # Commit-ish to check out. HEAD proves the current committed state.
    [string]$Commit = 'HEAD',

    # Skip the suite run; only prove reproducibility of the hook path.
    [switch]$SkipSuites,

    # Keep the worktree for inspection instead of removing it.
    [switch]$KeepWorktree
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$failures = [System.Collections.Generic.List[string]]::new()
function Fail([string]$m) { $failures.Add($m); Write-Host "FAIL: $m" -ForegroundColor Red }
function Pass([string]$m) { Write-Host "PASS: $m" -ForegroundColor Green }
function Note([string]$m) { Write-Host "      $m" -ForegroundColor DarkGray }

$repoRoot = (git rev-parse --show-toplevel).Trim()
if (-not $repoRoot) { throw 'Not inside a git work tree.' }
Set-Location $repoRoot

$sha = (git rev-parse --short $Commit).Trim()
$wt  = Join-Path ([System.IO.Path]::GetTempPath()) ("gloam-cleanclone-" + [guid]::NewGuid().ToString('N').Substring(0, 12))

Write-Host ''
Write-Host 'CLEAN-CHECKOUT REPRODUCIBILITY PROOF' -ForegroundColor Cyan
Write-Host ('-' * 78)
Note "repo     : $repoRoot"
Note "commit   : $Commit ($sha)"
Note "worktree : $wt"
Write-Host ''

# Warn loudly if the proof is about to be less meaningful than it looks.
$dirty = @(git status --porcelain --untracked-files=normal | Where-Object { $_ })
if ($dirty.Count -gt 0) {
    Note "NOTE: working tree has $($dirty.Count) uncommitted change(s). They are NOT part of this"
    Note "      proof. Commit them first if you intend to prove the current state of your work."
}

$created = $false
try {
    git worktree add --detach $wt $Commit 2>&1 | ForEach-Object { Note $_ }
    if ($LASTEXITCODE -ne 0) { throw "git worktree add failed ($LASTEXITCODE)" }
    $created = $true
    Write-Host ''

    # --- 1. The projection targets must be absent in a clean checkout -------------------------
    # If these exist, they are tracked, and the VCS policy decision has been violated.
    $mustBeAbsent = @('.claude/settings.json', '.claude/hooks', '.claude/agents', '.grok/rules')
    $unexpected = @($mustBeAbsent | Where-Object { Test-Path -LiteralPath (Join-Path $wt $_) })
    if ($unexpected.Count -eq 0) {
        Pass 'Clean checkout contains no adapter projection (generated output is not committed)'
    } else {
        Fail "Projection artifacts are committed but policy says generated: $($unexpected -join ', ')"
    }

    # --- 2. Every executable link must be present from tracked sources ------------------------
    $mustExist = @(
        'agent_collab/adapters/claude-code/settings.json',
        'agent_collab/adapters/claude-code/hooks/pre-bash-policy.ps1',
        'agent_collab/scripts/Assert-BashPolicy.ps1',
        'agent_collab/scripts/CommandPolicy.psm1',
        'agent_collab/scripts/Project-ClaudeAdapter.ps1',
        'agent_collab/scripts/Test-ShellGuard.ps1',
        'agent_collab/scripts/Test-CommandPolicy.ps1',
        # Test-ShellGuard.ps1 fails closed on a declared-but-absent suite, so every suite it
        # registers has to be listed here too. These four were missing: a forgotten `git add` on any
        # of them slipped past this check and only surfaced later as a downstream suite failure,
        # which is exactly the confusing shape this proof exists to prevent.
        'agent_collab/scripts/Test-BashPolicyHook.ps1',
        'agent_collab/scripts/Test-CommandPolicyFuzz.ps1',
        'agent_collab/scripts/Test-PolicyStructure.ps1',
        'agent_collab/tests/rule-coverage-map.json',
        'agent_collab/tests/command-policy-smoke.jsonl',
        'agent_collab/context/command_policy.json',
        'agent_collab/context/command-policy-spec.md'
    )
    $missing = @($mustExist | Where-Object { -not (Test-Path -LiteralPath (Join-Path $wt $_)) })
    if ($missing.Count -eq 0) {
        Pass "All $($mustExist.Count) guard-chain files present from tracked sources"
    } else {
        Fail "Absent from a clean checkout: $($missing -join ', ')"
    }

    # --- 3. Project, then 4. verify the registration reproduced byte-for-byte -----------------
    Push-Location $wt
    try {
        & pwsh -NoProfile -File 'agent_collab/scripts/Project-ClaudeAdapter.ps1' 2>&1 |
            Select-Object -Last 3 | ForEach-Object { Note $_ }
        $projExit = $LASTEXITCODE
        if ($projExit -ne 0) { Fail "Project-ClaudeAdapter.ps1 exited $projExit in a clean checkout" }
        else { Pass 'Project-ClaudeAdapter.ps1 ran clean in a fresh checkout' }

        $src  = Join-Path $wt 'agent_collab/adapters/claude-code/settings.json'
        $proj = Join-Path $wt '.claude/settings.json'
        if (-not (Test-Path -LiteralPath $proj)) {
            Fail 'Projection did not produce .claude/settings.json — the hook would be unregistered'
        }
        else {
            $hs = (Get-FileHash -LiteralPath $src  -Algorithm SHA256).Hash
            $hp = (Get-FileHash -LiteralPath $proj -Algorithm SHA256).Hash
            if ($hs -eq $hp) { Pass "Projected settings.json byte-identical to tracked source (SHA256 $($hs.Substring(0,12))…)" }
            else { Fail 'Projected settings.json differs from its tracked source — projection is not deterministic' }

            $raw = Get-Content -Raw $proj
            $okMatcher = $raw -match '"PreToolUse"' -and $raw -match '"matcher"\s*:\s*"Bash"'
            $okHook    = $raw -match 'pre-bash-policy\.ps1'
            if ($okMatcher -and $okHook) { Pass 'Reproduced registration binds PreToolUse Bash -> pre-bash-policy.ps1' }
            else { Fail "Reproduced registration incomplete (matcher=$okMatcher hook=$okHook)" }
        }

        # --- 5. Mandatory suites, in the clean checkout ---------------------------------------
        if ($SkipSuites) {
            Note 'Suite run skipped (-SkipSuites).'
        }
        else {
            Write-Host ''
            Note 'Running shell-guard suite inside the clean checkout…'
            $out = & pwsh -NoProfile -File 'agent_collab/scripts/Test-ShellGuard.ps1' 2>&1
            $suiteExit = $LASTEXITCODE
            if ($suiteExit -eq 0) {
                Pass 'Shell-guard suite GREEN from a committed-clean checkout'
                $out | Select-Object -Last 4 | ForEach-Object { Note $_ }
            }
            else {
                Fail "Shell-guard suite failed in a clean checkout (exit $suiteExit)"
                $out | ForEach-Object { Write-Host "      $_" -ForegroundColor DarkYellow }
            }
        }
    }
    finally { Pop-Location }
}
finally {
    if ($created -and -not $KeepWorktree) {
        git worktree remove --force $wt 2>&1 | Out-Null
        git worktree prune 2>&1 | Out-Null
        if (Test-Path -LiteralPath $wt) { Remove-Item -LiteralPath $wt -Recurse -Force -ErrorAction SilentlyContinue }
        Note 'Worktree removed.'
    }
    elseif ($created) { Note "Worktree kept at $wt" }
}

Write-Host ''
Write-Host ('-' * 78)
if ($failures.Count -eq 0) {
    Write-Host "CLEAN-CLONE PROOF PASS (commit $sha)" -ForegroundColor Green
    exit 0
}
Write-Host "CLEAN-CLONE PROOF FAIL: $($failures.Count) failure(s) (commit $sha)" -ForegroundColor Red
$failures | ForEach-Object { Write-Host "  - $_" -ForegroundColor Red }
exit 1
