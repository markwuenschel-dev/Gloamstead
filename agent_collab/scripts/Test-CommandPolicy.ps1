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
        rule     (string)  one or more spec rule IDs, comma separated, e.g. "R-POS-2,R-DATA-1"
        fallback (bool)    OPTIONAL. When present, asserts the fail-open flag on the result
                           equals this value (R-OPEN-4: a fail-open must be distinguishable
                           from an ordinary allow).

    Runs Get-CommandClassification over every row and prints:
      - counts by outcome and by category
      - the expected -> actual matrix
      - per-rule coverage against the rule IDs parsed out of the specification, naming any
        spec rule with no corpus row and no declared external coverage
      - contract violations: non-deterministic repeat classification (R-CORE-3), a deny with
        no reason / matched token / position (R-EXIT-2), a decision outside allow|ask|deny
        (R-EXIT-4), a classifier exception (R-CORE-4), a wrong fail-open flag (R-OPEN-4)
      - every mismatch as:  id | rule | expected | actual | cmd

    Exit code: 0 only if there are zero mismatches, zero unreadable rows, zero contract
    violations and zero uncovered spec rules; otherwise 1.

    This runner is authored independently of the implementation it judges (spec AUTHORITY).
    A failing row is a finding for the implementer, never a reason to change the expectation.

.EXAMPLE
    pwsh -NoProfile -File agent_collab/scripts/Test-CommandPolicy.ps1 `
         -CorpusPath agent_collab/tests/command-policy-smoke.jsonl
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$CorpusPath,

    # Normative specification. Rule IDs are parsed from here, so coverage is computed against
    # the spec rather than against a hand-maintained list.
    [string]$SpecPath,

    # Declares the rules that no corpus row can exercise (hook boundary, implementation shape,
    # verification process) and names the artefact that does cover each one.
    [string]$CoverageMapPath,

    # Print one line per row, not just mismatches.
    [switch]$Verbose_Rows
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..' '..'))
if (-not $SpecPath) {
    $SpecPath = Join-Path $repoRoot 'agent_collab' 'context' 'command-policy-spec.md'
}
if (-not $CoverageMapPath) {
    $CoverageMapPath = Join-Path $repoRoot 'agent_collab' 'tests' 'rule-coverage-map.json'
}

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
    $missing = @(@('id', 'cmd', 'expect', 'rule') | Where-Object { $names -notcontains $_ })
    if ($missing.Count -gt 0) {
        $badLines.Add("line $lineNo : missing field(s) $($missing -join ', ')")
        continue
    }
    # Rule ids may carry a lowercase sub-rule suffix (R-DATA-4a, R-PROT-2a). Normalise the
    # alphabetic part to upper case and keep the suffix lower case, matching the spec's spelling.
    $ruleList = @(([string]$obj.rule) -split ',' | ForEach-Object {
        $t = $_.Trim()
        if ($t -match '^(?<core>[Rr]-[A-Za-z]+-\d+)(?<suffix>[A-Za-z]?)$') {
            $Matches['core'].ToUpperInvariant() + $Matches['suffix'].ToLowerInvariant()
        } else { $t }
    } | Where-Object { $_ })
    $badRule  = @($ruleList | Where-Object { $_ -notmatch '^R-[A-Z]+-\d+[a-z]?$' })
    if ($ruleList.Count -eq 0 -or $badRule.Count -gt 0) {
        $badLines.Add("line $lineNo : malformed rule field '$($obj.rule)'")
        continue
    }
    $expectFallback = $null
    if ($names -contains 'fallback') { $expectFallback = [bool]$obj.fallback }
    $rows.Add(@{
        Id             = [string]$obj.id
        Cmd            = [string]$obj.cmd
        Expect         = ([string]$obj.expect).Trim().ToLowerInvariant()
        Category       = if ($names -contains 'category' -and $obj.category) { [string]$obj.category } else { '(uncategorised)' }
        Why            = if ($names -contains 'why' -and $obj.why) { [string]$obj.why } else { '' }
        Rules          = $ruleList
        ExpectFallback = $expectFallback
    })
}

# Duplicate ids make the work queue ambiguous; treat as an unreadable-corpus condition.
$dupIds = @($rows | Group-Object -Property { $_.Id } | Where-Object { $_.Count -gt 1 } | ForEach-Object { $_.Name })
foreach ($d in $dupIds) { $badLines.Add("duplicate row id: $d") }

# ---- classification -------------------------------------------------------------------------
# Property names on the result object are discovered, not assumed: this runner must be able to
# judge R-EXIT-2 (a deny reports reason, matched token and position) without reading the module.
$positionPropertyPattern = '^(Position|Index|Offset|Column|CharIndex|TokenPosition|TokenIndex|Start|StartIndex|Location)$'

$results    = [System.Collections.Generic.List[hashtable]]::new()
$errored    = [System.Collections.Generic.List[string]]::new()
$violations = [System.Collections.Generic.List[hashtable]]::new()
$positionPropertiesSeen = [System.Collections.Generic.HashSet[string]]::new()

function Get-ResultSnapshot {
    param($Result)
    if ($null -eq $Result) { return $null }
    $snap = [ordered]@{}
    foreach ($p in $Result.PSObject.Properties) {
        $v = $p.Value
        $snap[$p.Name] = if ($null -eq $v) { '<null>' } else { [string]$v }
    }
    return $snap
}

foreach ($row in $rows) {
    $actual   = '(error)'
    $reason   = ''
    $fallback = $false
    $token    = $null
    $position = $null
    $positionProperty = $null
    $snap1    = $null
    $snap2    = $null
    $threw    = $false

    try {
        $r = Get-CommandClassification -Command $row.Cmd
        $actual = [string]$r.Decision
        $names  = @($r.PSObject.Properties.Name)
        if ($names -contains 'Reason')         { $reason   = [string]$r.Reason }
        if ($names -contains 'ParserFallback') { $fallback = [bool]$r.ParserFallback }
        if ($names -contains 'MatchedToken')   { $token    = $r.MatchedToken }
        $posProp = @($names | Where-Object { $_ -match $positionPropertyPattern }) | Select-Object -First 1
        if ($posProp) {
            $positionProperty = $posProp
            $position = $r.$posProp
            [void]$positionPropertiesSeen.Add($posProp)
        }
        $snap1 = Get-ResultSnapshot -Result $r

        # R-CORE-3: same input, same decision/reason/token/position on a second call.
        $r2 = Get-CommandClassification -Command $row.Cmd
        $snap2 = Get-ResultSnapshot -Result $r2
    }
    catch {
        $threw = $true
        $errored.Add("$($row.Id) : classifier threw - $($_.Exception.Message)")
        $reason = "classifier threw: $($_.Exception.Message)"
        $violations.Add(@{ Id = $row.Id; Rule = 'R-CORE-4'; Detail = "classifier threw: $($_.Exception.Message)" })
    }

    if (-not $threw) {
        # R-EXIT-4: decision domain.
        if (@('allow', 'ask', 'deny') -notcontains $actual) {
            $violations.Add(@{ Id = $row.Id; Rule = 'R-EXIT-4'; Detail = "decision '$actual' is outside allow|ask|deny" })
        }

        # R-CORE-3: determinism across repeat classification.
        if ($null -ne $snap1 -and $null -ne $snap2) {
            $diffs = [System.Collections.Generic.List[string]]::new()
            $keys  = @($snap1.Keys) + @($snap2.Keys) | Select-Object -Unique
            foreach ($k in $keys) {
                $a = if ($snap1.Contains($k)) { $snap1[$k] } else { '<absent>' }
                $b = if ($snap2.Contains($k)) { $snap2[$k] } else { '<absent>' }
                if ($a -ne $b) { $diffs.Add("$k : '$a' -> '$b'") }
            }
            if ($diffs.Count -gt 0) {
                $violations.Add(@{ Id = $row.Id; Rule = 'R-CORE-3'; Detail = "repeat classification differed: $($diffs -join '; ')" })
            }
        }

        # R-EXIT-2: a deny must be auditable.
        if ($actual -eq 'deny') {
            $lacks = [System.Collections.Generic.List[string]]::new()
            if ([string]::IsNullOrWhiteSpace($reason))                { $lacks.Add('reason') }
            if ($null -eq $token -or [string]::IsNullOrWhiteSpace([string]$token)) { $lacks.Add('matched token') }
            if ($null -eq $positionProperty)                          { $lacks.Add('position property') }
            elseif ($null -eq $position -or [string]::IsNullOrWhiteSpace([string]$position)) { $lacks.Add("position value ($positionProperty)") }
            if ($lacks.Count -gt 0) {
                $violations.Add(@{ Id = $row.Id; Rule = 'R-EXIT-2'; Detail = "deny without $($lacks -join ', ')" })
            }
        }

        # R-OPEN-4: a fail-open must be distinguishable from an ordinary allow.
        if ($null -ne $row.ExpectFallback -and $fallback -ne $row.ExpectFallback) {
            $violations.Add(@{ Id = $row.Id; Rule = 'R-OPEN-4'
                Detail = "expected fail-open flag $($row.ExpectFallback), observed $fallback" })
        }
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
        Rules    = $row.Rules
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
Show-Line ("spec       : {0}" -f $SpecPath)
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
Show-Line ("  contract violations                    : {0}" -f $violations.Count)
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
Show-Line ("  {0,-38} {1,6} {2,6} {3,6} {4,6} {5,6} {6,6}" -f 'category', 'rows', 'pass', 'fail', 'allow', 'ask', 'deny')
$cats = $results | Group-Object -Property { $_.Category } | Sort-Object Name
foreach ($cat in $cats) {
    $g = @($cat.Group)
    Show-Line ("  {0,-38} {1,6} {2,6} {3,6} {4,6} {5,6} {6,6}" -f
        $cat.Name,
        $g.Count,
        @($g | Where-Object { $_.Pass }).Count,
        @($g | Where-Object { -not $_.Pass }).Count,
        @($g | Where-Object { $_.Actual -eq 'allow' }).Count,
        @($g | Where-Object { $_.Actual -eq 'ask'   }).Count,
        @($g | Where-Object { $_.Actual -eq 'deny'  }).Count)
}
Show-Line ''

# ---- per-rule coverage against the spec ----------------------------------------------------------
$specRules      = @()
$specMissingMsg = $null
if (Test-Path -LiteralPath $SpecPath) {
    $specText  = Get-Content -LiteralPath $SpecPath -Raw -Encoding UTF8
    # Rule ids in the spec are R-<AREA>-<n> with an optional lowercase sub-rule suffix
    # (R-DATA-4a, R-PROT-2a, R-PROT-2b). Both forms are real rules and both must be covered.
    $specRules = @([regex]::Matches($specText, 'R-[A-Z]+-\d+[a-z]?') |
        ForEach-Object { $_.Value } | Sort-Object -Unique)
} else {
    $specMissingMsg = "spec not found: $SpecPath"
}

$externalMap = @{}
$suiteFiles  = @{}
$mapLoadErr  = $null
if (Test-Path -LiteralPath $CoverageMapPath) {
    try {
        $mapObj = Get-Content -LiteralPath $CoverageMapPath -Raw -Encoding UTF8 | ConvertFrom-Json
        # 'suites' maps a suite id to the file that owns it. A rule may only name an id declared
        # here, and that file must exist. v1 of this map allowed free-text artefacts, and four rules
        # named the bare word 'critic'; because the old check only existence-tested strings
        # containing a path separator, 'critic' counted as covered unconditionally. R-PROT-1 was one
        # of those four and was FALSE at the time it read green. There is deliberately no longer any
        # spelling that satisfies the checker without a real file behind it.
        if ($mapObj.PSObject.Properties.Name -contains 'suites' -and $null -ne $mapObj.suites) {
            foreach ($s in $mapObj.suites.PSObject.Properties) {
                $suiteFiles[$s.Name] = [string]$s.Value
            }
        }
        if ($mapObj.PSObject.Properties.Name -contains 'external' -and $null -ne $mapObj.external) {
            foreach ($p in $mapObj.external.PSObject.Properties) {
                $externalMap[$p.Name.ToUpperInvariant()] = $p.Value
            }
        }
    }
    catch {
        $mapLoadErr = "coverage map unreadable: $($_.Exception.Message)"
    }
} else {
    $mapLoadErr = "coverage map not found: $CoverageMapPath"
}

$ruleRows = @{}
foreach ($r in $results) {
    foreach ($rule in $r.Rules) {
        if (-not $ruleRows.ContainsKey($rule)) { $ruleRows[$rule] = [System.Collections.Generic.List[hashtable]]::new() }
        $ruleRows[$rule].Add($r)
    }
}

# R-VER-1a: report uncovered rules PER OWNING SUITE, fail only on rules this suite owns, and never
# drop an unattributed rule from the denominator. Every spec rule is therefore resolved to exactly
# one owner below, and the failure classes are kept distinct:
#   corpus-uncovered  - the corpus owns it (or nothing does) and no row cites it   -> FAILURE here
#   unresolved-suite  - the map names a suite id absent from the 'suites' table    -> FAILURE here
#   missing-artefact  - the owning suite is declared but its file is absent        -> FAILURE here
#   delegated         - another existing suite owns it                            -> reported only
# A delegated rule is NOT a failure of this runner; Test-ShellGuard.ps1 runs those suites and they
# answer for their own rules. Silently counting them as covered here would be the v1 defect again.
$attribution     = [System.Collections.Generic.List[hashtable]]::new()
$unknownRules    = @($ruleRows.Keys | Where-Object { $specRules -notcontains $_ } | Sort-Object)

foreach ($rule in $specRules) {
    if ($ruleRows.ContainsKey($rule)) {
        $g = @($ruleRows[$rule])
        $attribution.Add(@{
            Rule = $rule; Owner = 'corpus'; Status = 'corpus'; Detail = ''
            Rows = $g.Count
            Pass = @($g | Where-Object { $_.Pass }).Count
            Fail = @($g | Where-Object { -not $_.Pass }).Count
            Deny = @($g | Where-Object { $_.Actual -eq 'deny' }).Count
        })
        continue
    }
    if ($externalMap.ContainsKey($rule)) {
        $entry     = $externalMap[$rule]
        $suiteId   = if ($entry.PSObject.Properties.Name -contains 'suite') { [string]$entry.suite } else { '' }
        $caseNote  = if ($entry.PSObject.Properties.Name -contains 'case')  { [string]$entry.case }  else { '' }
        if ([string]::IsNullOrWhiteSpace($suiteId)) {
            $attribution.Add(@{ Rule = $rule; Owner = '(unattributed)'; Status = 'unresolved-suite'
                Detail = 'map entry declares no suite'; Rows = 0; Pass = 0; Fail = 0; Deny = 0 })
            continue
        }
        if (-not $suiteFiles.ContainsKey($suiteId)) {
            $attribution.Add(@{ Rule = $rule; Owner = $suiteId; Status = 'unresolved-suite'
                Detail = "suite id '$suiteId' is not declared in the map's 'suites' table"; Rows = 0; Pass = 0; Fail = 0; Deny = 0 })
            continue
        }
        $rel  = $suiteFiles[$suiteId]
        $full = Join-Path $repoRoot $rel
        if (-not (Test-Path -LiteralPath $full)) {
            $attribution.Add(@{ Rule = $rule; Owner = $suiteId; Status = 'missing-artefact'
                Detail = "declared owner file absent: $rel"; Rows = 0; Pass = 0; Fail = 0; Deny = 0 })
            continue
        }
        $attribution.Add(@{ Rule = $rule; Owner = $suiteId; Status = 'delegated'
            Detail = $caseNote; Rows = 0; Pass = 0; Fail = 0; Deny = 0 })
        continue
    }
    $attribution.Add(@{ Rule = $rule; Owner = '(unattributed)'; Status = 'corpus-uncovered'
        Detail = 'no corpus row cites it and the coverage map does not attribute it'; Rows = 0; Pass = 0; Fail = 0; Deny = 0 })
}

$coverageFailures = [System.Collections.Generic.List[string]]::new()
foreach ($a in $attribution) {
    switch ($a.Status) {
        'corpus-uncovered' { $coverageFailures.Add("$($a.Rule) - $($a.Detail)") }
        'unresolved-suite' { $coverageFailures.Add("$($a.Rule) - $($a.Detail)") }
        'missing-artefact' { $coverageFailures.Add("$($a.Rule) - $($a.Detail)") }
    }
}

Show-Line 'RULE COVERAGE (computed against the rule IDs parsed from the spec)'
Show-Line $sep
if ($specMissingMsg) { Show-Line "  !! $specMissingMsg" }
if ($mapLoadErr)     { Show-Line "  !! $mapLoadErr" }
Show-Line ("  spec rules found : {0}" -f $specRules.Count)
Show-Line ("  {0,-12} {1,6} {2,6} {3,6} {4,6}  {5}" -f 'rule', 'rows', 'pass', 'fail', 'deny', 'coverage')
foreach ($a in $attribution) {
    $label = switch ($a.Status) {
        'corpus'           { 'corpus' }
        'delegated'        { "delegated -> $($a.Owner)" }
        'missing-artefact' { "MISSING ARTEFACT: $($a.Detail)" }
        'unresolved-suite' { "UNRESOLVED SUITE: $($a.Detail)" }
        default            { 'UNCOVERED' }
    }
    Show-Line ("  {0,-12} {1,6} {2,6} {3,6} {4,6}  {5}" -f $a.Rule, $a.Rows, $a.Pass, $a.Fail, $a.Deny, $label)
}
Show-Line ''

# R-VER-1a: the per-owning-suite view. Every spec rule appears under exactly one owner, so the
# denominator is visibly complete and no rule can go missing without showing up here.
Show-Line 'RULE COVERAGE BY OWNING SUITE (R-VER-1a)'
Show-Line $sep
$ownerOrder = @('corpus') + @($suiteFiles.Keys | Where-Object { $_ -ne 'corpus' } | Sort-Object) + @('(unattributed)')
$seenOwners = @{}
foreach ($owner in $ownerOrder) {
    if ($seenOwners.ContainsKey($owner)) { continue }
    $seenOwners[$owner] = $true
    $mine = @($attribution | Where-Object { $_.Owner -eq $owner })
    if ($mine.Count -eq 0) { continue }
    $bad  = @($mine | Where-Object { $_.Status -ne 'corpus' -and $_.Status -ne 'delegated' })
    $file = if ($suiteFiles.ContainsKey($owner)) { $suiteFiles[$owner] } else { '-' }
    Show-Line ("  {0,-14} {1,3} rule(s), {2,3} unverified   {3}" -f $owner, $mine.Count, $bad.Count, $file)
    foreach ($b in $bad) { Show-Line "      !! $($b.Rule) - $($b.Detail)" }
}
Show-Line ''
Show-Line ("  rules with corpus rows        : {0}" -f @($attribution | Where-Object { $_.Status -eq 'corpus' }).Count)
Show-Line ("  rules delegated to a suite    : {0}" -f @($attribution | Where-Object { $_.Status -eq 'delegated' }).Count)
Show-Line ("  rules attributed (total)      : {0} of {1}" -f @($attribution | Where-Object { $_.Status -eq 'corpus' -or $_.Status -eq 'delegated' }).Count, $specRules.Count)
Show-Line ("  coverage FAILURES             : {0}" -f $coverageFailures.Count)
if ($coverageFailures.Count -gt 0) {
    foreach ($u in $coverageFailures) { Show-Line "    !! $u" }
}
if ($unknownRules.Count -gt 0) {
    Show-Line '  !! rule ids cited by the corpus but ABSENT from the spec:'
    foreach ($k in $unknownRules) { Show-Line "    - $k" }
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

# ---- result-object shape (informational, NOT a violation) -----------------------------------------
# R-EXIT-2 requires a deny to report the offending token's position, so which property carries it is
# worth printing. It used to be printed inside the CONTRACT VIOLATIONS block below, where a
# successful discovery read as a violation -- confusing on an otherwise clean run.
Show-Line 'RESULT OBJECT SHAPE (informational)'
Show-Line $sep
if ($positionPropertiesSeen.Count -gt 0) {
    Show-Line ("  position property discovered on the result object: {0}" -f (($positionPropertiesSeen | Sort-Object) -join ', '))
} else {
    Show-Line '  position property discovered on the result object: (none)'
}
Show-Line ''

# ---- contract violations --------------------------------------------------------------------------
Show-Line 'CONTRACT VIOLATIONS  (rule | id | detail)'
Show-Line $sep
if ($violations.Count -eq 0) {
    Show-Line '  (none)'
} else {
    foreach ($v in ($violations | Sort-Object -Property { $_.Rule }, { $_.Id })) {
        Show-Line ("  {0} | {1} | {2}" -f $v.Rule, $v.Id, $v.Detail)
    }
}
Show-Line ''

# ---- per-row detail (opt-in) ----------------------------------------------------------------------
if ($Verbose_Rows) {
    Show-Line 'ALL ROWS'
    Show-Line $sep
    foreach ($r in $results) {
        $flag = if ($r.Pass) { 'ok  ' } else { 'FAIL' }
        Show-Line ("  {0} {1} | {2} | {3} -> {4} | {5}" -f $flag, $r.Id, ($r.Rules -join ','), $r.Expect, $r.Actual, ($r.Cmd -replace "`n", '\n'))
    }
    Show-Line ''
}

# ---- mismatches -------------------------------------------------------------------------------------
Show-Line 'MISMATCHES  (id | rule | expected | actual | cmd)'
Show-Line $sep
if ($mismatch.Count -eq 0) {
    Show-Line '  (none)'
} else {
    foreach ($m in $mismatch) {
        Show-Line ("  {0} | {1} | {2} | {3} | {4}" -f $m.Id, ($m.Rules -join ','), $m.Expect, $m.Actual, ($m.Cmd -replace "`n", '\n'))
        if ($m.Why)    { Show-Line ("      why-expected : {0}" -f $m.Why) }
        if ($m.Reason) { Show-Line ("      classifier   : {0}" -f $m.Reason) }
    }
}
Show-Line ''
Show-Line $sep
# $coverageFailures counts only rules THIS suite answers for (corpus-owned, unattributed, or a
# broken map declaration) - R-VER-1a. Rules delegated to an existing sibling suite are reported
# above and are that suite's verdict to give, not this one's. A map that cannot be read or that
# names a suite id it never declares still fails here: fail closed on a broken declaration.
$ok = ($mismatch.Count -eq 0 -and $badLines.Count -eq 0 -and $errored.Count -eq 0 -and
       $violations.Count -eq 0 -and $coverageFailures.Count -eq 0 -and $unknownRules.Count -eq 0 -and
       $null -eq $mapLoadErr)
$verdict = if ($ok) { 'PASS' } else { 'FAIL' }
Show-Line ("RESULT: {0} - {1}/{2} rows matched expectation, {3} mismatch(es), {4} fallback(s), {5} unreadable line(s), {6} contract violation(s), {7} coverage failure(s)" -f
    $verdict, @($results | Where-Object { $_.Pass }).Count, $results.Count,
    $mismatch.Count, $fallbacks.Count, $badLines.Count, $violations.Count, $coverageFailures.Count)
Show-Line ''

if ($ok) { exit 0 } else { exit 1 }
