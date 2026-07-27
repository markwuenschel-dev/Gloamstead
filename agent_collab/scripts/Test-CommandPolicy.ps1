#requires -Version 7
<#
.SYNOPSIS
    Corpus runner for CommandPolicy.psm1.

.DESCRIPTION
    Reads a JSONL corpus - one JSON object per line, with fields:

        id       (string)  stable row identifier
        cmd      (string)  the command string to classify (use \n for newlines)
        expect   (string)  allow | ask | deny
        category (string)  free-form grouping, e.g. data-mention, direct-invocation
        why      (string)  rationale, printed only for mismatches

    Runs Get-CommandClassification over every row and prints a report with counts by category
    and by outcome, then every mismatch as:  id | expected | actual | cmd

    Exit code: 0 only if there are zero mismatches and zero unreadable rows; otherwise 1.

.EXAMPLE
    pwsh -NoProfile -File agent_collab/scripts/Test-CommandPolicy.ps1 `
         -CorpusPath agent_collab/tests/command-policy-smoke.jsonl
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$CorpusPath,

    # Print one line per row, not just mismatches.
    [switch]$Verbose_Rows
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$modulePath = Join-Path $PSScriptRoot 'CommandPolicy.psm1'
if (-not (Test-Path -LiteralPath $modulePath)) {
    Write-Error "CommandPolicy.psm1 not found next to this script (expected: $modulePath)"
    exit 1
}
Import-Module $modulePath -Force

if (-not (Test-Path -LiteralPath $CorpusPath)) {
    Write-Error "Corpus not found: $CorpusPath"
    exit 1
}

$lines = Get-Content -LiteralPath $CorpusPath -Encoding UTF8

$rows        = [System.Collections.Generic.List[hashtable]]::new()
$badLines    = [System.Collections.Generic.List[string]]::new()
$lineNo      = 0

foreach ($line in $lines) {
    $lineNo++
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    if ($line.TrimStart().StartsWith('//') -or $line.TrimStart().StartsWith('#')) { continue }
    try {
        $obj = $line | ConvertFrom-Json -ErrorAction Stop
    }
    catch {
        $badLines.Add("line $lineNo : unparseable JSON - $($_.Exception.Message)")
        continue
    }
    $names = $obj.PSObject.Properties.Name
    $missing = @(@('id', 'cmd', 'expect') | Where-Object { $names -notcontains $_ })
    if ($missing.Count -gt 0) {
        $badLines.Add("line $lineNo : missing field(s) $($missing -join ', ')")
        continue
    }
    $rows.Add(@{
        Id       = [string]$obj.id
        Cmd      = [string]$obj.cmd
        Expect   = ([string]$obj.expect).Trim().ToLowerInvariant()
        Category = if ($names -contains 'category' -and $obj.category) { [string]$obj.category } else { '(uncategorised)' }
        Why      = if ($names -contains 'why' -and $obj.why) { [string]$obj.why } else { '' }
    })
}

$results   = [System.Collections.Generic.List[hashtable]]::new()
$errored   = [System.Collections.Generic.List[string]]::new()

foreach ($row in $rows) {
    $actual   = '(error)'
    $reason   = ''
    $fallback = $false
    $token    = $null
    try {
        $r = Get-CommandClassification -Command $row.Cmd
        $actual   = [string]$r.Decision
        $reason   = [string]$r.Reason
        $fallback = [bool]$r.ParserFallback
        $token    = $r.MatchedToken
    }
    catch {
        $errored.Add("$($row.Id) : classifier threw - $($_.Exception.Message)")
        $reason = "classifier threw: $($_.Exception.Message)"
    }
    $results.Add(@{
        Id       = $row.Id
        Cmd      = $row.Cmd
        Expect   = $row.Expect
        Actual   = $actual
        Category = $row.Category
        Why      = $row.Why
        Reason   = $reason
        Fallback = $fallback
        Token    = $token
        Pass     = ($actual -eq $row.Expect)
    })
}

function Show-Line { param([string]$Text) Write-Host $Text }

$sep = ('-' * 78)

Show-Line ''
Show-Line "COMMAND POLICY CORPUS REPORT"
Show-Line $sep
Show-Line ("corpus     : {0}" -f (Resolve-Path -LiteralPath $CorpusPath).Path)
Show-Line ("module     : {0}" -f $modulePath)
Show-Line ("rows read  : {0}   (unreadable lines: {1})" -f $results.Count, $badLines.Count)
Show-Line ''

# ---- outcome counts -------------------------------------------------------------------------
$denied    = @($results | Where-Object { $_.Actual -eq 'deny'  })
$allowed   = @($results | Where-Object { $_.Actual -eq 'allow' })
$asked     = @($results | Where-Object { $_.Actual -eq 'ask'   })
$fallbacks = @($results | Where-Object { $_.Fallback })
$mismatch  = @($results | Where-Object { -not $_.Pass })

Show-Line 'OUTCOME COUNTS'
Show-Line $sep
Show-Line ("  denied  (direct invocations blocked)   : {0}" -f $denied.Count)
Show-Line ("  allowed (data-only mentions permitted) : {0}" -f $allowed.Count)
Show-Line ("  asked   (ambiguous launches escalated) : {0}" -f $asked.Count)
Show-Line ("  parser fallbacks (fail-open)           : {0}" -f $fallbacks.Count)
Show-Line ("  unexpected outcomes (mismatches)       : {0}" -f $mismatch.Count)
Show-Line ("  classifier exceptions                  : {0}" -f $errored.Count)
Show-Line ''

# ---- expected-vs-actual matrix ---------------------------------------------------------------
Show-Line 'EXPECTED -> ACTUAL MATRIX'
Show-Line $sep
Show-Line ("  {0,-10} {1,8} {2,8} {3,8}" -f 'expected', 'allow', 'ask', 'deny')
foreach ($e in @('allow', 'ask', 'deny')) {
    $bucket = @($results | Where-Object { $_.Expect -eq $e })
    Show-Line ("  {0,-10} {1,8} {2,8} {3,8}" -f $e,
        @($bucket | Where-Object { $_.Actual -eq 'allow' }).Count,
        @($bucket | Where-Object { $_.Actual -eq 'ask'   }).Count,
        @($bucket | Where-Object { $_.Actual -eq 'deny'  }).Count)
}
$otherExpect = @($results | Where-Object { @('allow','ask','deny') -notcontains $_.Expect })
if ($otherExpect.Count -gt 0) {
    Show-Line ("  {0,-10} {1} row(s) declare an expectation outside allow/ask/deny" -f '(invalid)', $otherExpect.Count)
}
Show-Line ''

# ---- category counts ---------------------------------------------------------------------------
Show-Line 'CATEGORY COUNTS'
Show-Line $sep
Show-Line ("  {0,-34} {1,6} {2,6} {3,6} {4,6} {5,6} {6,6}" -f 'category', 'rows', 'pass', 'fail', 'allow', 'ask', 'deny')
$cats = $results | Group-Object -Property { $_.Category } | Sort-Object Name
foreach ($cat in $cats) {
    $g = @($cat.Group)
    Show-Line ("  {0,-34} {1,6} {2,6} {3,6} {4,6} {5,6} {6,6}" -f
        $cat.Name,
        $g.Count,
        @($g | Where-Object { $_.Pass }).Count,
        @($g | Where-Object { -not $_.Pass }).Count,
        @($g | Where-Object { $_.Actual -eq 'allow' }).Count,
        @($g | Where-Object { $_.Actual -eq 'ask'   }).Count,
        @($g | Where-Object { $_.Actual -eq 'deny'  }).Count)
}
Show-Line ''

# ---- parser fallbacks ----------------------------------------------------------------------------
if ($fallbacks.Count -gt 0) {
    Show-Line 'PARSER FALLBACKS (fail-open, malformed or unsupported syntax)'
    Show-Line $sep
    foreach ($f in $fallbacks) {
        Show-Line ("  {0} | {1} | {2}" -f $f.Id, $f.Actual, $f.Reason)
    }
    Show-Line ''
}

# ---- unreadable rows -----------------------------------------------------------------------------
if ($badLines.Count -gt 0) {
    Show-Line 'UNREADABLE CORPUS LINES'
    Show-Line $sep
    foreach ($b in $badLines) { Show-Line "  $b" }
    Show-Line ''
}

if ($errored.Count -gt 0) {
    Show-Line 'CLASSIFIER EXCEPTIONS (contract violation: it must never throw)'
    Show-Line $sep
    foreach ($e in $errored) { Show-Line "  $e" }
    Show-Line ''
}

# ---- per-row detail (opt-in) ----------------------------------------------------------------------
if ($Verbose_Rows) {
    Show-Line 'ALL ROWS'
    Show-Line $sep
    foreach ($r in $results) {
        $flag = if ($r.Pass) { 'ok  ' } else { 'FAIL' }
        Show-Line ("  {0} {1} | {2} -> {3} | {4}" -f $flag, $r.Id, $r.Expect, $r.Actual, ($r.Cmd -replace "`n", '\n'))
    }
    Show-Line ''
}

# ---- mismatches -------------------------------------------------------------------------------------
Show-Line 'MISMATCHES  (id | expected | actual | cmd)'
Show-Line $sep
if ($mismatch.Count -eq 0) {
    Show-Line '  (none)'
} else {
    foreach ($m in $mismatch) {
        Show-Line ("  {0} | {1} | {2} | {3}" -f $m.Id, $m.Expect, $m.Actual, ($m.Cmd -replace "`n", '\n'))
        if ($m.Why)    { Show-Line ("      why-expected : {0}" -f $m.Why) }
        if ($m.Reason) { Show-Line ("      classifier   : {0}" -f $m.Reason) }
    }
}
Show-Line ''
Show-Line $sep
$verdict = if ($mismatch.Count -eq 0 -and $badLines.Count -eq 0 -and $errored.Count -eq 0) { 'PASS' } else { 'FAIL' }
Show-Line ("RESULT: {0} - {1}/{2} rows matched expectation, {3} mismatch(es), {4} fallback(s), {5} unreadable line(s)" -f
    $verdict, @($results | Where-Object { $_.Pass }).Count, $results.Count,
    $mismatch.Count, $fallbacks.Count, $badLines.Count)
Show-Line ''

if ($verdict -eq 'PASS') { exit 0 } else { exit 1 }
