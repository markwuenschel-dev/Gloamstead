#!/usr/bin/env pwsh
<#
.SYNOPSIS
    UE5-aware defense-in-depth command policy.

    Blocks dangerous git, destructive fs, remote execution, and (importantly for UE5):
    - unapproved plugin/engine/asset commands
    - commands that would modify vendor content when detectable
    - Unreal generation/packaging commands unless the surrounding context (handoff) has explicitly permitted them (this script is conservative; the real gate is handoff + scope + Critic)

    Exits 0 = allowed (per policy), 2 = blocked.
    This is defense-in-depth. The real containment is explicit ownership + worktrees + Critic + candidate verification.
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

$blockedPatterns = @(
    @{ Pattern = 'git\s+(push|pull|fetch\s+origin|remote|pr)\b'; Reason = 'Remote mutation or PR forbidden (Orchestrator only)' },
    @{ Pattern = 'git\s+reset\s+--hard|git\s+rebase|git\s+amend|git\s+filter-(branch|repo)'; Reason = 'History rewrite forbidden' },
    @{ Pattern = '(^|;\s*|\|\s*)rm\s+-rf\s+|Remove-Item\s+.*-Recurse'; Reason = 'Recursive destructive delete forbidden' },
    @{ Pattern = '(curl|wget|iex|Invoke-WebRequest|Invoke-Expression)\s+.*\|\s*(bash|sh|pwsh|powershell)'; Reason = 'Pipe-to-shell or remote code execution vector' },
    @{ Pattern = '\bsudo\b'; Reason = 'Privilege escalation' },
    # UE5 human gates
    @{ Pattern = 'git\s+lfs\s+(pull|checkout|smudge)'; Reason = 'LFS operations on binaries should be controlled; avoid in workers' },
    @{ Pattern = 'UnrealEditor|RunUAT|BuildCookRun|GenerateProjectFiles|UnrealEditor-Cmd'; Reason = 'Unreal generation/packaging/automation commands require explicit handoff permission + assigned generated output ownership. Not generally allowed.' }
)

foreach ($entry in $blockedPatterns) {
    if ($Command -match $entry.Pattern) {
        Write-Error "BLOCKED by policy: $($entry.Reason)"
        Write-Error "Command was: $Command"
        exit 2
    }
}

# Additional heuristic: block obvious vendor modification attempts
if ($Command -match 'Content/(ThirdPerson|Characters|Mannequins|Marketplace|Fab|Sample)') {
    Write-Error "BLOCKED by policy: Command appears to target vendor/sample content (read-only per content_policy.json)"
    Write-Error "Command was: $Command"
    exit 2
}

Write-Output "ALLOWED (policy check passed)"
exit 0
