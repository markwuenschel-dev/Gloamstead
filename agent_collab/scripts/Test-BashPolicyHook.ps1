#requires -Version 7
<#
.SYNOPSIS
    Live end-to-end tests for the Claude Code PreToolUse(Bash) hook (spec R-VER-4).

.DESCRIPTION
    Drives the REAL hook the way the harness does: builds PreToolUse JSON, writes it to the
    hook's standard input, and asserts on the parsed stdout and the process exit code. The
    2026-07-27 defect - the whole `ask` class computed and then dropped - was invisible to module
    tests because the module was correct and the hook was not (spec R-VER-4, R-EXIT-3).

    Assertions, per the exit-code contract in section 2 of the spec:
      R-EXIT-1  allow emits NO decision at all. An explicit "allow" would override the operator's
                own deny rules, so its absence is asserted positively: empty stdout, and the
                string "allow" must not appear.
      R-EXIT-2  a deny carries a non-empty reason.
      R-EXIT-3  an ask reaches the human as permissionDecision "ask".
      R-OPEN-1  malformed, empty and non-JSON stdin fail OPEN: no decision, exit 0.
      R-VER-6   driving the hook leaves durable repository state byte-identical.

    stdout is parsed as JSON; substring matching is used only where the assertion is literally
    about the absence of a substring.

    This script writes nothing into agent_collab/. It creates no temp files: the payload is
    written directly to the child process's stdin.

.EXAMPLE
    pwsh -NoProfile -File agent_collab/scripts/Test-BashPolicyHook.ps1
#>
[CmdletBinding()]
param(
    [string]$HookPath,
    [switch]$ShowRaw
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..' '..'))
if (-not $HookPath) {
    $HookPath = Join-Path $repoRoot 'agent_collab' 'adapters' 'claude-code' 'hooks' 'pre-bash-policy.ps1'
}
if (-not (Test-Path -LiteralPath $HookPath)) {
    Write-Error "hook not found: $HookPath"
    exit 1
}
$policyPath = Join-Path $repoRoot 'agent_collab' 'scripts' 'Assert-BashPolicy.ps1'

# ---------------------------------------------------------------------------------------------
# durable-state fingerprint (R-VER-6 / no-side-effect requirement)
# ---------------------------------------------------------------------------------------------
$durableRoots = @('logs', 'state', 'handoffs', 'outbox') | ForEach-Object { Join-Path $repoRoot 'agent_collab' $_ }

function Get-DurableFingerprint {
    $map = [ordered]@{}
    foreach ($root in $durableRoots) {
        if (-not (Test-Path -LiteralPath $root)) { continue }
        $files = Get-ChildItem -LiteralPath $root -Recurse -File -ErrorAction SilentlyContinue | Sort-Object FullName
        foreach ($f in $files) {
            $rel = $f.FullName.Substring($repoRoot.Length).TrimStart('\', '/')
            $hash = try { (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash } catch { 'unreadable' }
            $map[$rel] = "$hash|$($f.Length)|$($f.LastWriteTimeUtc.ToString('o'))"
        }
    }
    return $map
}

function Compare-Fingerprint {
    param($Before, $After)
    $changes = [System.Collections.Generic.List[string]]::new()
    foreach ($k in $Before.Keys) {
        if (-not $After.Contains($k)) { $changes.Add("removed: $k") }
        elseif ($After[$k] -ne $Before[$k]) { $changes.Add("modified: $k") }
    }
    foreach ($k in $After.Keys) {
        if (-not $Before.Contains($k)) { $changes.Add("added: $k") }
    }
    # Return the plain array and let every call site re-collect it with @(...). A ',' wrapper here
    # was worse than the unrolling it tried to prevent: the wrapper is itself unrolled on output, so
    # @(Compare-Fingerprint ...) yielded a ONE-element array whose single element was the whole
    # string[]. With zero changes that element is an empty string[], and `($_ -split ': ',2)[-1]`
    # over it indexes an empty result -> StrictMode "Index was outside the bounds of the array",
    # which fired on the healthy quiet-window path and aborted this suite before any assertion ran.
    # With N changes it silently collapsed N filenames to one.
    return [string[]]$changes.ToArray()
}

# ---------------------------------------------------------------------------------------------
# hook driver
# ---------------------------------------------------------------------------------------------
function Invoke-Hook {
    param(
        [AllowEmptyString()][string]$StdinText,
        [switch]$CloseWithoutWriting
    )
    $psi = [System.Diagnostics.ProcessStartInfo]::new()
    $psi.FileName  = (Get-Command pwsh).Source
    $psi.Arguments = "-NoProfile -File `"$HookPath`""
    $psi.RedirectStandardInput  = $true
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError  = $true
    $psi.UseShellExecute = $false
    $psi.WorkingDirectory = $repoRoot

    $proc = [System.Diagnostics.Process]::Start($psi)
    $outTask = $proc.StandardOutput.ReadToEndAsync()
    $errTask = $proc.StandardError.ReadToEndAsync()
    if (-not $CloseWithoutWriting) { $proc.StandardInput.Write($StdinText) }
    $proc.StandardInput.Close()
    if (-not $proc.WaitForExit(120000)) {
        try { $proc.Kill($true) } catch { }
        return @{ ExitCode = -1; StdOut = ''; StdErr = 'hook timed out after 120s' }
    }
    return @{
        ExitCode = $proc.ExitCode
        StdOut   = $outTask.GetAwaiter().GetResult()
        StdErr   = $errTask.GetAwaiter().GetResult()
    }
}

function New-PreToolUseJson {
    param([string]$Command, [string]$ToolName = 'Bash')
    return (@{ tool_name = $ToolName; tool_input = @{ command = $Command } } | ConvertTo-Json -Depth 5 -Compress)
}

function Get-HookDecision {
    param([string]$StdOut)
    # Returns @{ HasJson; Decision; Reason; EventName; ParseError }
    $res = @{ HasJson = $false; Decision = $null; Reason = $null; EventName = $null; ParseError = $null }
    if ([string]::IsNullOrWhiteSpace($StdOut)) { return $res }
    try {
        $obj = $StdOut | ConvertFrom-Json -ErrorAction Stop
    }
    catch {
        $res.ParseError = $_.Exception.Message
        return $res
    }
    $res.HasJson = $true
    if ($obj.PSObject.Properties.Name -contains 'hookSpecificOutput' -and $null -ne $obj.hookSpecificOutput) {
        $h = $obj.hookSpecificOutput
        $hn = @($h.PSObject.Properties.Name)
        if ($hn -contains 'permissionDecision')       { $res.Decision  = [string]$h.permissionDecision }
        if ($hn -contains 'permissionDecisionReason') { $res.Reason    = [string]$h.permissionDecisionReason }
        if ($hn -contains 'hookEventName')            { $res.EventName = [string]$h.hookEventName }
    }
    return $res
}

# ---------------------------------------------------------------------------------------------
# cases
# ---------------------------------------------------------------------------------------------
$cases = @(
    @{ Id = 'hook-deny-proven';        Rule = 'R-EXIT-2';  Expect = 'deny'
       Stdin = (New-PreToolUseJson 'UnrealEditor-Cmd.exe -run=Foo')
       Note  = 'a proven direct invocation must emit permissionDecision deny with a non-empty reason' }

    @{ Id = 'hook-deny-nested';        Rule = 'R-NEST-1';  Expect = 'deny'
       Stdin = (New-PreToolUseJson 'bash -c "UnrealEditor-Cmd.exe -run"')
       Note  = 'literal nested payload proves the target one level down' }

    @{ Id = 'hook-deny-lfs';           Rule = 'R-PROT-3';  Expect = 'deny'
       Stdin = (New-PreToolUseJson 'git lfs pull')
       Note  = 'git lfs pull is a protected binary-churning subcommand' }

    @{ Id = 'hook-deny-after-malformation'; Rule = 'R-OPEN-5'; Expect = 'deny'
       Stdin = (New-PreToolUseJson 'echo "unclosed && UnrealEditor-Cmd.exe -run=Cook')
       Note  = 'spec line 277: a fail-open must not truncate analysis, so this must still deny' }

    @{ Id = 'hook-ask-reaches-human';  Rule = 'R-EXIT-3';  Expect = 'ask'
       Stdin = (New-PreToolUseJson '$editor -run')
       Note  = 'THE 2026-07-27 REGRESSION: ask must reach the human, never be silently downgraded' }

    @{ Id = 'hook-ask-launcher';       Rule = 'R-ASK-1';   Expect = 'ask'
       Stdin = (New-PreToolUseJson 'Start-Process $exe')
       Note  = 'unprovable launcher target escalates' }

    @{ Id = 'hook-allow-silent';       Rule = 'R-EXIT-1';  Expect = 'none'
       Stdin = (New-PreToolUseJson 'git status --porcelain')
       Note  = 'allow MUST emit no decision at all - an explicit allow would override operator deny rules' }

    @{ Id = 'hook-allow-heredoc-prose'; Rule = 'R-DATA-4'; Expect = 'none'
       Stdin = (New-PreToolUseJson "cat <<EOF`nWe never call `"D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe`" from a worker.`nEOF")
       Note  = 'the historical false positive: a prose heredoc body must pass silently' }

    @{ Id = 'hook-allow-build-oracle'; Rule = 'R-NEST-2';  Expect = 'none'
       Stdin = (New-PreToolUseJson 'pwsh -File gate.ps1')
       Note  = 'the build oracle must never be blocked' }

    @{ Id = 'hook-empty-stdin';        Rule = 'R-OPEN-1';  Expect = 'none'
       Stdin = ''; CloseWithoutWriting = $true
       Note  = 'no stdin at all: fail open, no decision, exit 0' }

    @{ Id = 'hook-whitespace-stdin';   Rule = 'R-OPEN-1';  Expect = 'none'
       Stdin = "   `n  "
       Note  = 'whitespace-only stdin: fail open' }

    @{ Id = 'hook-non-json-stdin';     Rule = 'R-OPEN-1';  Expect = 'none'
       Stdin = 'this is not JSON at all { , '
       Note  = 'unparseable stdin: fail open, exit 0' }

    @{ Id = 'hook-json-no-command';    Rule = 'R-OPEN-1';  Expect = 'none'
       Stdin = '{"tool_name":"Bash","tool_input":{"description":"no command field"}}'
       Note  = 'tool_input without a command field: fail open' }

    @{ Id = 'hook-json-null-command';  Rule = 'R-OPEN-1';  Expect = 'none'
       Stdin = '{"tool_name":"Bash","tool_input":{"command":null}}'
       Note  = 'null command: fail open' }

    @{ Id = 'hook-json-no-tool-input'; Rule = 'R-OPEN-1';  Expect = 'none'
       Stdin = '{"tool_name":"Bash"}'
       Note  = 'no tool_input at all: fail open' }

    @{ Id = 'hook-non-bash-tool';      Rule = 'R-CORE-4';  Expect = 'none'
       Stdin = '{"tool_name":"Read","tool_input":{"file_path":"D:/x.txt"}}'
       Note  = 'a non-Bash payload must not crash the hook' }

    @{ Id = 'hook-json-array';         Rule = 'R-CORE-4';  Expect = 'none'
       Stdin = '[1,2,3]'
       Note  = 'JSON of the wrong shape must not crash the hook' }
)

$results = [System.Collections.Generic.List[hashtable]]::new()

$fpBefore = Get-DurableFingerprint
$sw = [System.Diagnostics.Stopwatch]::StartNew()

foreach ($case in $cases) {
    $closeOnly = ($case.ContainsKey('CloseWithoutWriting') -and $case.CloseWithoutWriting)
    $run = if ($closeOnly) { Invoke-Hook -StdinText '' -CloseWithoutWriting } else { Invoke-Hook -StdinText $case.Stdin }
    $parsed = Get-HookDecision -StdOut $run.StdOut

    $failures = [System.Collections.Generic.List[string]]::new()

    # the hook must never fail the tool call
    if ($run.ExitCode -ne 0) { $failures.Add("exit code $($run.ExitCode), expected 0") }

    switch ($case.Expect) {
        'none' {
            if (-not [string]::IsNullOrWhiteSpace($run.StdOut)) {
                $failures.Add("expected NO decision on stdout, got: $($run.StdOut.Trim())")
            }
            # R-EXIT-1 asserted positively: the word allow must not be emitted as a decision
            if ($run.StdOut -match '"permissionDecision"\s*:\s*"allow"') {
                $failures.Add('emitted an explicit permissionDecision "allow", which would override operator deny rules (R-EXIT-1)')
            }
        }
        default {
            if (-not $parsed.HasJson) {
                $failures.Add("expected a JSON decision object, stdout was: '$($run.StdOut.Trim())'" +
                    $(if ($parsed.ParseError) { " (parse error: $($parsed.ParseError))" } else { '' }))
            } else {
                if ($parsed.Decision -ne $case.Expect) {
                    $failures.Add("permissionDecision '$($parsed.Decision)', expected '$($case.Expect)'")
                }
                if ([string]::IsNullOrWhiteSpace($parsed.Reason)) {
                    $failures.Add('permissionDecisionReason is empty (R-EXIT-2 requires an auditable reason)')
                }
                if ($parsed.EventName -ne 'PreToolUse') {
                    $failures.Add("hookEventName '$($parsed.EventName)', expected 'PreToolUse'")
                }
            }
        }
    }

    $results.Add(@{
        Id       = $case.Id
        Rule     = $case.Rule
        Expect   = $case.Expect
        Actual   = if ($parsed.HasJson) { [string]$parsed.Decision } elseif ([string]::IsNullOrWhiteSpace($run.StdOut)) { 'none' } else { 'unparseable-output' }
        ExitCode = $run.ExitCode
        Reason   = $parsed.Reason
        StdOut   = $run.StdOut
        StdErr   = $run.StdErr
        Note     = $case.Note
        Pass     = ($failures.Count -eq 0)
        Failures = $failures
    })
}

$fpAfter  = Get-DurableFingerprint
$sw.Stop()
$hookChanges = @(Compare-Fingerprint -Before $fpBefore -After $fpAfter)

# Ambient control: this repository has concurrently running agents that write their own logs.
# Measure an idle window of the same duration and subtract anything that churns without any hook
# activity, so a durable write is only attributed to the guard when the guard actually caused it.
$fpIdle0 = Get-DurableFingerprint
Start-Sleep -Milliseconds ([Math]::Max(1500, [int]$sw.Elapsed.TotalMilliseconds))
$fpIdle1 = Get-DurableFingerprint
$ambientChanges = @(Compare-Fingerprint -Before $fpIdle0 -After $fpIdle1)
$ambientFiles   = @($ambientChanges | ForEach-Object { ($_ -split ': ', 2)[-1] })
$stateChanges   = @($hookChanges | Where-Object { $ambientFiles -notcontains (($_ -split ': ', 2)[-1]) })

# Attribute any durable write: call the policy script directly and fingerprint again.
$fpBeforePolicy = Get-DurableFingerprint
$policyRan = $false
$policyExit = $null
if (Test-Path -LiteralPath $policyPath) {
    $policyRan = $true
    & pwsh -NoProfile -File $policyPath -Command 'UnrealEditor-Cmd.exe -run=Foo' *> $null
    $policyExit = $LASTEXITCODE
}
$fpAfterPolicy = Get-DurableFingerprint
$policyStateChanges = @(Compare-Fingerprint -Before $fpBeforePolicy -After $fpAfterPolicy)

$results.Add(@{
    Id       = 'durable-state-untouched'
    Rule     = 'R-VER-6'
    Expect   = 'no change'
    Actual   = if ($stateChanges.Count -eq 0) { 'no change' } else { "$($stateChanges.Count) change(s)" }
    ExitCode = 0
    Reason   = ($stateChanges -join '; ')
    StdOut   = ''
    StdErr   = ''
    Note     = 'driving the hook must leave agent_collab logs/state/handoffs/outbox byte-identical'
    Pass     = ($stateChanges.Count -eq 0)
    Failures = $stateChanges
})

# ---------------------------------------------------------------------------------------------
# report
# ---------------------------------------------------------------------------------------------
function Show-Line { param([string]$Text) Write-Host $Text }
$sep = ('-' * 78)

Show-Line ''
Show-Line 'LIVE HOOK REPORT (real PreToolUse JSON on stdin)'
Show-Line $sep
Show-Line ("hook   : {0}" -f $HookPath)
Show-Line ("policy : {0}" -f $policyPath)
Show-Line ("cases  : {0}" -f $results.Count)
Show-Line ''

Show-Line ("  {0,-30} {1,-10} {2,-10} {3,-19} {4,4}  {5}" -f 'case', 'rule', 'expected', 'actual', 'exit', 'result')
Show-Line $sep
foreach ($r in $results) {
    Show-Line ("  {0,-30} {1,-10} {2,-10} {3,-19} {4,4}  {5}" -f
        $r.Id, $r.Rule, $r.Expect, $r.Actual, $r.ExitCode, $(if ($r.Pass) { 'PASS' } else { 'FAIL' }))
}
Show-Line ''

$failed = @($results | Where-Object { -not $_.Pass })
if ($failed.Count -gt 0) {
    Show-Line 'FAILURE DETAIL'
    Show-Line $sep
    foreach ($r in $failed) {
        Show-Line ("  {0} ({1})" -f $r.Id, $r.Rule)
        Show-Line ("      why-it-matters : {0}" -f $r.Note)
        foreach ($f in $r.Failures) { Show-Line ("      !! {0}" -f $f) }
        if ($r.StdErr -and $r.StdErr.Trim()) { Show-Line ("      stderr : {0}" -f ($r.StdErr.Trim() -replace "`r?`n", ' | ')) }
    }
    Show-Line ''
}

if ($ShowRaw) {
    Show-Line 'RAW STDOUT PER CASE'
    Show-Line $sep
    foreach ($r in $results) {
        Show-Line ("  {0}: {1}" -f $r.Id, $(if ($r.StdOut) { $r.StdOut.Trim() } else { '(empty)' }))
    }
    Show-Line ''
}

Show-Line 'DURABLE-WRITE OBSERVATION (reported, not repaired)'
Show-Line $sep
if ($stateChanges.Count -eq 0) {
    Show-Line '  no durable change attributable to the hook under agent_collab/{logs,state,handoffs,outbox}'
} else {
    Show-Line '  driving the hook CHANGED durable state:'
    foreach ($c in $stateChanges) { Show-Line "    - $c" }
}
if ($hookChanges.Count -gt 0) {
    Show-Line ("  raw changes observed across the hook pass ({0}):" -f $hookChanges.Count)
    foreach ($c in $hookChanges) { Show-Line "    - $c" }
}
if ($ambientChanges.Count -gt 0) {
    Show-Line ("  of those, files that also churn during an idle control window of the same length ({0}) - ambient writers, not the guard:" -f $ambientChanges.Count)
    foreach ($c in $ambientChanges) { Show-Line "    - $c" }
}
if ($policyRan) {
    if ($policyStateChanges.Count -eq 0) {
        Show-Line ("  invoking Assert-BashPolicy.ps1 directly (exit {0}) changed nothing" -f $policyExit)
    } else {
        Show-Line ("  invoking Assert-BashPolicy.ps1 directly (exit {0}) CHANGED durable state:" -f $policyExit)
        foreach ($c in $policyStateChanges) { Show-Line "    - $c" }
    }
} else {
    Show-Line '  Assert-BashPolicy.ps1 not found; attribution pass skipped'
}
Show-Line ''

Show-Line $sep
$verdict = if ($failed.Count -eq 0) { 'PASS' } else { 'FAIL' }
Show-Line ("RESULT: {0} - {1}/{2} hook cases passed" -f $verdict, @($results | Where-Object { $_.Pass }).Count, $results.Count)
Show-Line ''

if ($verdict -eq 'PASS') { exit 0 } else { exit 1 }
