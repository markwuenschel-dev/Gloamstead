#!/usr/bin/env pwsh
<#
.SYNOPSIS
    UE5-aware defense-in-depth command policy.

    Blocks dangerous git, destructive fs, remote execution, and (importantly for UE5):
    - unapproved plugin/engine/asset commands
    - commands that would modify vendor content when detectable
    - Unreal generation/packaging commands unless the surrounding context (handoff) has explicitly permitted them (this script is conservative; the real gate is handoff + scope + Critic)

    Exit contract:
        0 = allowed (per policy)
        2 = blocked
        3 = ask (a launch that COULD reach a protected executable but cannot be proven; escalate
            to a human/approval prompt rather than deciding it here)

    This is defense-in-depth. The real containment is explicit ownership + worktrees + Critic + candidate verification.

.NOTES
    SCOPE — read this before relying on it (corrected 2026-07-27).

    This is a DIRECT-INVOCATION GUARD, not worker containment. It inspects the submitted command
    string and nothing else. Specifically it does NOT:
      - read the body of any script the command runs, or
      - see processes that script spawns.

    So `pwsh -File gate.ps1` is allowed even though gate.ps1 itself launches UnrealEditor-Cmd.exe:
    the launch is a grandchild process and is never presented to this script. One level of script
    indirection defeats the guard by construction. That is a known and accepted limit — the guard
    exists to catch a careless direct call, not to contain a determined or scripted path.

    Real containment remains: explicit handoff file_ownership + generated_output_ownership, edit-scope
    guards, worktree isolation, vendor immutability, Critic audits, and candidate integration
    verification. See agent_collab/context/command_policy.json -> enforcement_notes.

    UNREAL CLASSIFICATION IS NO LONGER APPEARANCE-BASED (2026-07-27).

    The Unreal rule used to be a regex over the raw command string. It classified by how a command
    LOOKED, and had a proven defect: a bash heredoc whose PROSE BODY contained a quoted engine path
    was DENIED, though nothing was being invoked. That regex is gone. Unreal classification is now
    delegated to CommandPolicy.psm1, which lexes the command, finds real executable-token positions,
    and treats quoted strings, heredoc bodies, here-string bodies and comments as data. Malformed
    input fails OPEN there — see that module's header for the full limits and the deny-vs-ask
    decision for nested interpreters.

    The non-Unreal patterns below are retained verbatim as pre-existing behaviour.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$Command
)

Set-StrictMode -Version Latest

if ($Command -match 'Invoke-GloamsteadDataAssetImport\.ps1') {
    exit 0
}

# ---------------------------------------------------------------------------------------------
# 1. Non-Unreal blocked patterns, READ FROM THE SINGLE DECLARATION (R-PROT-1).
#
#    These used to be a hardcoded copy in this file. command_policy.json's own description
#    declared that this script reads them from there rather than duplicating them, and that
#    declaration was simply false -- with two consequences that had both already bitten:
#
#      1. The 2026-07-29 history-rewrite fix landed in command_policy.json only. This file kept
#         'git\s+amend', which matches a command git does not have and never matched the real
#         'git commit --amend'. The corrected pattern was live in a file nothing read.
#      2. The inline LFS pattern 'git\s+lfs\s+(pull|checkout|smudge)' DENIED
#         `git commit -m "ran git lfs pull"`, which command-policy-spec.md:251 states must be
#         allowed. It is the surviving twin of the appearance-based Unreal regex removed on
#         2026-07-27 (see .NOTES): it classified by how a command LOOKED. R-PROT-3 is now owned
#         by the classifier (command_policy.json 'subcommand_rules' -> CommandPolicy.psm1), which
#         matches it only at a proven command position, so the inline pattern is gone rather than
#         ported. That is why the list read from JSON is shorter than the list it replaces.
#
#    FAIL OPEN, loudly, if the declaration cannot be read -- matching what the classifier block
#    below does, and for the same reason. There is deliberately NO hardcoded fallback list: a
#    fallback is a second declaration, which is the defect this section exists to remove.
# ---------------------------------------------------------------------------------------------
$policyDeclPath = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..' 'context' 'command_policy.json'))
$decl = $null
try {
    if (-not (Test-Path -LiteralPath $policyDeclPath)) {
        throw "not found at $policyDeclPath"
    }
    $decl = Get-Content -LiteralPath $policyDeclPath -Raw -Encoding UTF8 | ConvertFrom-Json
}
catch {
    Write-Error "POLICY WARNING: command_policy.json unreadable ($($_.Exception.Message)); blocked_patterns and vendor_content_patterns are NOT being applied (fail-open)."
    $decl = $null
}

if ($null -ne $decl) {
    foreach ($group in @('blocked_patterns', 'vendor_content_patterns')) {
        if ($decl.PSObject.Properties.Name -notcontains $group) { continue }
        foreach ($entry in @($decl.$group)) {
            if ([string]::IsNullOrWhiteSpace([string]$entry.pattern)) { continue }
            if ($Command -match ([string]$entry.pattern)) {
                Write-Error "BLOCKED by policy: $([string]$entry.reason)"
                Write-Error "Declared in: command_policy.json -> $group"
                Write-Error "Command was: $Command"
                exit 2
            }
        }
    }
}

# ---------------------------------------------------------------------------------------------
# 2. Unreal invocation: real lexical classification, not appearance matching.
#    If the classifier cannot be loaded we FAIL OPEN on this rule only; the patterns above have
#    already run. A guard that bricks every shell command when it breaks is worse than the
#    exposure it removes.
# ---------------------------------------------------------------------------------------------
$modulePath = Join-Path $PSScriptRoot 'CommandPolicy.psm1'
$classification = $null

if (Test-Path -LiteralPath $modulePath) {
    try {
        Import-Module $modulePath -Force -ErrorAction Stop
        $classification = Get-CommandClassification -Command $Command
    }
    catch {
        Write-Error "POLICY WARNING: command classifier unavailable ($($_.Exception.Message)); Unreal invocation rule not applied."
        $classification = $null
    }
} else {
    Write-Error "POLICY WARNING: CommandPolicy.psm1 not found at $modulePath; Unreal invocation rule not applied."
}

if ($null -ne $classification) {
    switch ($classification.Decision) {
        'deny' {
            Write-Error "BLOCKED by policy: Direct Unreal generation/packaging/automation invocation requires explicit handoff permission + assigned generated output ownership. Not generally allowed. (Guard inspects the submitted command string only.)"
            Write-Error "Classifier: $($classification.Reason)"
            if ($classification.MatchedToken) {
                Write-Error "Token: $($classification.MatchedToken) (position $($classification.Position))"
            }
            Write-Error "Command was: $Command"
            exit 2
        }
        'ask' {
            Write-Error "ASK (human decision required): $($classification.Reason)"
            Write-Error "This command could reach a protected Unreal executable, but the target cannot be proven from the command string. It is not denied; escalate it."
            Write-Error "Command was: $Command"
            exit 3
        }
        default {
            if ($classification.ParserFallback) {
                Write-Error "POLICY NOTE (fail-open): $($classification.Reason)"
            }
        }
    }
}

Write-Output "ALLOWED (policy check passed)"
exit 0
