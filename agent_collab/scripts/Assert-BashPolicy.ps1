#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Defense-in-depth command policy checker. Scans a proposed command string against denylists.
    Not a security boundary (alias evasion is possible). Exits 0 allowed, 2 blocked.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$Command
)

Set-StrictMode -Version Latest

$blockedPatterns = @(
    @{ Pattern = 'git\s+(push|pull|fetch\s+origin|remote|pr)\b'; Reason = 'Remote mutation or PR forbidden' },
    @{ Pattern = 'git\s+reset\s+--hard'; Reason = 'Destructive reset forbidden' },
    @{ Pattern = 'git\s+rebase'; Reason = 'History rewrite forbidden' },
    @{ Pattern = 'git\s+amend'; Reason = 'History rewrite forbidden' },
    @{ Pattern = 'git\s+filter-(branch|repo)'; Reason = 'History rewrite forbidden' },
    @{ Pattern = 'git\s+clean\s+-fd'; Reason = 'Destructive cleanup forbidden' },
    @{ Pattern = '(^|;\s*|\|\s*)rm\s+-rf\s+'; Reason = 'Recursive rm -rf forbidden' },
    @{ Pattern = '(^|;\s*|\|\s*)Remove-Item\s+.*-Recurse'; Reason = 'PowerShell recursive delete forbidden' },
    @{ Pattern = '(curl|wget|iex|Invoke-WebRequest|Invoke-Expression)\s+.*\|\s*(bash|sh|pwsh|powershell|iex)'; Reason = 'Pipe-to-shell or remote code execution vector' },
    @{ Pattern = '\bsudo\b'; Reason = 'Privilege escalation outside controlled env' }
)

foreach ($entry in $blockedPatterns) {
    if ($Command -match $entry.Pattern) {
        Write-Error "BLOCKED by policy: $($entry.Reason) (matched: $($entry.Pattern))"
        Write-Error "Command was: $Command"
        exit 2
    }
}

Write-Output "ALLOWED (policy check passed)"
exit 0
