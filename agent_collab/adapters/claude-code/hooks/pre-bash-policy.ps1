#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Claude Code PreToolUse(Bash) hook: deliver the submitted command to Assert-BashPolicy.ps1.

    Previously this discarded its input and wrote {"permission":"allow"} unconditionally, which was
    both the wrong output shape and an unconditional pass. The policy script it names was therefore
    never consulted by anything except its own scaffold self-test, while the repository documented it
    as enforced. Repaired 2026-07-27 (decision A').

.NOTES
    SCOPE: direct-invocation guard, not worker containment. The hook receives tool_input.command --
    one command string -- and nothing else. Script bodies and spawned child processes are not visible
    to it, so one level of script indirection defeats the guard by construction. Accepted limit.

    ALLOW IS SILENT BY DESIGN. On pass this emits no decision and exits 0, letting the normal
    permission flow proceed. It deliberately does NOT return permissionDecision "allow", because an
    explicit allow would override the operator's own deny rules and approval prompts -- a guard must
    not become a bypass.

    FAIL-OPEN on infrastructure error. If stdin is unparseable or the policy script cannot be run, the
    command is permitted and a diagnostic goes to stderr. A defense-in-depth guard that bricks every
    shell command when it breaks is worse than the exposure it removes; deny is reserved for an
    explicit policy match.
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Approve {
    # Emit no decision: fall through to the normal permission flow.
    exit 0
}

function Decide([string]$Decision, [string]$Reason) {
    $payload = @{
        hookSpecificOutput = @{
            hookEventName            = 'PreToolUse'
            permissionDecision       = $Decision
            permissionDecisionReason = $Reason
        }
    }
    [Console]::Out.WriteLine(($payload | ConvertTo-Json -Depth 5 -Compress))
    exit 0
}

function Deny([string]$Reason) { Decide 'deny' $Reason }

# Exit 3 from the policy script means "could reach a protected executable, cannot be proven".
# It must reach a human, not be silently approved. Until 2026-07-27 this hook only branched on
# exit 2, so the entire ask class was computed, logged, and then dropped on the floor.
function Ask([string]$Reason) { Decide 'ask' $Reason }

try {
    $raw = [Console]::In.ReadToEnd()
    if ([string]::IsNullOrWhiteSpace($raw)) { Approve }

    $hookEvent = $raw | ConvertFrom-Json
    $command = $null
    if ($hookEvent.PSObject.Properties.Name -contains 'tool_input' -and $null -ne $hookEvent.tool_input) {
        if ($hookEvent.tool_input.PSObject.Properties.Name -contains 'command') {
            $command = [string]$hookEvent.tool_input.command
        }
    }
    if ([string]::IsNullOrWhiteSpace($command)) { Approve }

    $policy = Join-Path $PSScriptRoot '..' '..' '..' 'scripts' 'Assert-BashPolicy.ps1'
    $policy = [System.IO.Path]::GetFullPath($policy)
    if (-not (Test-Path -LiteralPath $policy)) {
        [Console]::Error.WriteLine("pre-bash-policy: policy script not found at $policy; allowing.")
        Approve
    }

    $output = & pwsh -NoProfile -File $policy -Command $command 2>&1
    $policyExit = $LASTEXITCODE

    # Keep the policy's own stated reason, minus PowerShell's stream decoration.
    $reason = (($output | Out-String) -split "`r?`n" |
        ForEach-Object { $_ -replace '^\s*Write-Error:\s*', '' } |
        Where-Object   { $_ -match '\S' } |
        Select-Object  -First 1) -join ' '

    switch ($policyExit) {
        2 {
            if ([string]::IsNullOrWhiteSpace($reason)) {
                $reason = 'Blocked by Gloamstead command policy (direct-invocation guard).'
            }
            Deny $reason
        }
        3 {
            if ([string]::IsNullOrWhiteSpace($reason)) {
                $reason = 'This command could reach a protected Unreal executable, but the target cannot be proven from the command string.'
            }
            Ask $reason
        }
    }

    Approve
}
catch {
    [Console]::Error.WriteLine("pre-bash-policy: guard error, allowing. $($_.Exception.Message)")
    exit 0
}
