#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Structural verification of the command policy: R-PROT-1, R-PROT-4, R-VER-5.

    command-policy-spec.md:328-334 assigns these rules to a "structural test", because they are
    facts about the SHAPE of the implementation rather than about any input string. No corpus row
    can express "the protected list lives in exactly one place" or "adding a target needs no lexer
    edit". Until this file existed, rule-coverage-map.json delegated them to the bare string
    "critic", and Test-CommandPolicy.ps1 only existence-checks an artefact when its name contains a
    path separator -- so "critic" counted as covered unconditionally, forever, with nothing behind
    it. R-PROT-1 was one of those rules, and it was FALSE at the time it was being reported green:
    command_policy.json's own description claimed Assert-BashPolicy.ps1 read blocked_patterns from
    it, while that script carried a hardcoded copy whose history-rewrite pattern ('git\s+amend')
    matched a command git does not have.

.NOTES
    PREFERS BEHAVIOUR OVER GREP. R-PROT-1 is checked by removing the declaration and asserting the
    classifier STOPS denying. A module holding a second copy of the list would keep denying, and no
    amount of pattern-matching over source text proves its absence as directly. The static check
    that remains is scoped to code lines only (comment lines are stripped), because the module and
    the assert script legitimately NAME protected binaries in prose.

    WRITES NOTHING DURABLE. Temp policy files go to the system temp directory and are removed in a
    finally block; no file under agent_collab/ is written (R-VER-6).

.PARAMETER RepoRoot
    Repository root. Defaults to the git toplevel above this script.

.PARAMETER ShowDetail
    Print the reason string returned by each classification probe.
#>
[CmdletBinding()]
param(
    [string]$RepoRoot,
    [switch]$ShowDetail
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
    $RepoRoot = (git -C $PSScriptRoot rev-parse --show-toplevel 2>$null)
    if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
        $RepoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..' '..'))
    }
    $RepoRoot = $RepoRoot.Trim()
}

$scriptsDir  = Join-Path $RepoRoot 'agent_collab/scripts'
$modulePath  = Join-Path $scriptsDir 'CommandPolicy.psm1'
$assertPath  = Join-Path $scriptsDir 'Assert-BashPolicy.ps1'
$policyPath  = Join-Path $RepoRoot 'agent_collab/context/command_policy.json'
$gatePath    = Join-Path $RepoRoot 'gate.ps1'
$scaffoldPath = Join-Path $scriptsDir 'Test-AgentCollabScaffold.ps1'

$results = [System.Collections.Generic.List[hashtable]]::new()
function Add-Result([string]$Id, [string]$Rule, [bool]$Pass, [string]$Detail) {
    $results.Add(@{ Id = $Id; Rule = $Rule; Pass = $Pass; Detail = $Detail })
}

function Get-CodeLines([string]$Path) {
    # Comment lines are not a second declaration. Naming a binary in prose is allowed and
    # necessary; only executable text can constitute a duplicate list.
    return @(Get-Content -LiteralPath $Path -Encoding UTF8 |
        Where-Object { $_ -notmatch '^\s*#' })
}

foreach ($p in @($modulePath, $assertPath, $policyPath)) {
    if (-not (Test-Path -LiteralPath $p)) {
        Write-Error "structural prerequisite missing: $p" -ErrorAction Continue
        exit 1
    }
}

$moduleHashBefore = (Get-FileHash -LiteralPath $modulePath -Algorithm SHA256).Hash
$declRaw = Get-Content -LiteralPath $policyPath -Raw -Encoding UTF8
$decl    = $declRaw | ConvertFrom-Json

Import-Module $modulePath -Force -ErrorAction Stop

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("gloam-polstruct-" + [System.Guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path $tempRoot -Force

try {
    # =========================================================================================
    # R-PROT-4 - adding a target MUST NOT require editing the lexer.
    # =========================================================================================
    # A name that is deliberately absent from the shipped protected set. If this ever ships as a
    # real target the baseline probe below fails loudly rather than silently inverting the test.
    $novelStem = 'gloamstead-structural-probe-tool'
    $novelCmd  = "$novelStem.exe -run=Probe"

    $baseline = Get-CommandClassification -Command $novelCmd
    Add-Result 'novel-target-not-yet-protected' 'R-PROT-4' ($baseline.Decision -eq 'allow') `
        "baseline for an unlisted target must be allow; got '$($baseline.Decision)'"

    $mutated = $declRaw | ConvertFrom-Json
    $mutated.classifier.protected_basenames = @(@($mutated.classifier.protected_basenames) + $novelStem)
    $mutatedPath = Join-Path $tempRoot 'command_policy.mutated.json'
    ($mutated | ConvertTo-Json -Depth 40) | Set-Content -LiteralPath $mutatedPath -Encoding UTF8

    $afterAdd = Get-CommandClassification -Command $novelCmd -PolicyPath $mutatedPath
    Add-Result 'added-target-now-denies' 'R-PROT-4' ($afterAdd.Decision -eq 'deny') `
        "after adding '$novelStem' to protected_basenames the same command must deny; got '$($afterAdd.Decision)'"
    Add-Result 'added-target-policy-loaded' 'R-PROT-4' ([bool]$afterAdd.PolicyLoaded) `
        'the mutated declaration must actually have loaded (else the probe proves nothing)'
    Add-Result 'added-target-source-is-mutated' 'R-PROT-4' `
        ($afterAdd.PolicySource -and ([System.IO.Path]::GetFullPath([string]$afterAdd.PolicySource)) -eq ([System.IO.Path]::GetFullPath($mutatedPath))) `
        "the classification must have consulted the mutated file; PolicySource='$($afterAdd.PolicySource)'"

    $moduleHashAfter = (Get-FileHash -LiteralPath $modulePath -Algorithm SHA256).Hash
    Add-Result 'lexer-untouched' 'R-PROT-4' ($moduleHashBefore -eq $moduleHashAfter) `
        'CommandPolicy.psm1 must be byte-identical: adding a target required no lexer edit'

    # =========================================================================================
    # R-PROT-1 - the protected set is declared in exactly ONE tracked location, and READ there.
    # =========================================================================================
    # Behavioural half: with the declaration absent the classifier must STOP denying. A module
    # carrying a hardcoded second copy would keep denying and fail here.
    $absentPath = Join-Path $tempRoot 'no-such-policy.json'
    $realTarget = 'UnrealEditor-Cmd.exe -run=Probe'

    $withDecl = Get-CommandClassification -Command $realTarget
    Add-Result 'real-target-denies-with-declaration' 'R-PROT-1' ($withDecl.Decision -eq 'deny') `
        "sanity: with the real declaration '$realTarget' must deny; got '$($withDecl.Decision)'"

    $withoutDecl = Get-CommandClassification -Command $realTarget -PolicyPath $absentPath
    Add-Result 'no-second-copy-in-module' 'R-PROT-1' `
        (($withoutDecl.Decision -eq 'allow') -and (-not $withoutDecl.PolicyLoaded)) `
        "with the declaration removed the module must fail open, proving it holds no second copy; got '$($withoutDecl.Decision)' PolicyLoaded=$($withoutDecl.PolicyLoaded)"

    # Static half, code lines only: no declared pattern may be duplicated as a literal in the
    # assert script. This is the exact regression that shipped -- the corrected history-rewrite
    # pattern lived in JSON while the script kept its own stale copy.
    $assertCode = (Get-CodeLines $assertPath) -join "`n"
    $dupPatterns = [System.Collections.Generic.List[string]]::new()
    foreach ($group in @('blocked_patterns', 'vendor_content_patterns')) {
        if ($decl.PSObject.Properties.Name -notcontains $group) { continue }
        foreach ($entry in @($decl.$group)) {
            $pat = [string]$entry.pattern
            if ([string]::IsNullOrWhiteSpace($pat)) { continue }
            if ($assertCode.Contains($pat)) { $dupPatterns.Add("$group :: $pat") }
        }
    }
    Add-Result 'assert-script-holds-no-pattern-copy' 'R-PROT-1' ($dupPatterns.Count -eq 0) `
        $(if ($dupPatterns.Count -eq 0) { 'no declared pattern appears as a literal in Assert-BashPolicy.ps1 code' }
          else { "duplicated declarations: $($dupPatterns -join ' | ')" })

    Add-Result 'assert-script-reads-declaration' 'R-PROT-1' ($assertCode -match 'command_policy\.json') `
        'Assert-BashPolicy.ps1 must reference command_policy.json in code, not merely in a comment'

    # The specific drift sentinel. 'git amend' is not a git command; its presence means someone
    # reintroduced a hand-maintained copy of the history-rewrite rule.
    Add-Result 'no-stale-amend-pattern' 'R-PROT-1' (-not ($assertCode -match 'git\\s\+amend')) `
        "the non-existent 'git amend' pattern must not reappear in code (use git commit --amend)"

    # =========================================================================================
    # R-VER-5 - verification MUST run on a mandatory tracked path, automatically.
    # =========================================================================================
    foreach ($wiring in @(
        @{ Id = 'gate-invokes-shell-guard';     Path = $gatePath;     Rel = 'gate.ps1' },
        @{ Id = 'scaffold-invokes-shell-guard'; Path = $scaffoldPath; Rel = 'Test-AgentCollabScaffold.ps1' }
    )) {
        if (-not (Test-Path -LiteralPath $wiring.Path)) {
            Add-Result $wiring.Id 'R-VER-5' $false "$($wiring.Rel) not found at $($wiring.Path)"
            continue
        }
        $code = (Get-CodeLines $wiring.Path) -join "`n"
        $hit  = $code -match 'Test-ShellGuard\.ps1'
        Add-Result $wiring.Id 'R-VER-5' $hit `
            "$($wiring.Rel) must invoke Test-ShellGuard.ps1 in code so the suite cannot be skipped by forgetting"
    }
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# ---------------------------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------------------------
$sep = ('-' * 78)
Write-Host ''
Write-Host 'POLICY STRUCTURE (R-PROT-1, R-PROT-4, R-VER-5)'
Write-Host $sep
Write-Host ('  {0,-38} {1,-10} {2}' -f 'check', 'rule', 'result')
foreach ($r in $results) {
    Write-Host ('  {0,-38} {1,-10} {2}' -f $r.Id, $r.Rule, $(if ($r.Pass) { 'PASS' } else { 'FAIL' }))
    if ((-not $r.Pass) -or $ShowDetail) { Write-Host "      $($r.Detail)" }
}

$failed = @($results | Where-Object { -not $_.Pass })
$byRule = @($results | Select-Object -ExpandProperty Rule -Unique | Sort-Object)
Write-Host ''
Write-Host ("  rules exercised : {0}" -f ($byRule -join ', '))
Write-Host ("  checks          : {0} ({1} failed)" -f $results.Count, $failed.Count)
Write-Host $sep

if ($failed.Count -gt 0) {
    Write-Host ("RESULT: FAIL - {0} structural check(s) failed" -f $failed.Count)
    exit 1
}
Write-Host ("RESULT: PASS - {0}/{0} structural checks passed" -f $results.Count)
exit 0
