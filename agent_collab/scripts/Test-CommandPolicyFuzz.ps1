#requires -Version 7
<#
.SYNOPSIS
    Deterministic fuzz / property harness for CommandPolicy.psm1 (spec R-VER-3).

.DESCRIPTION
    Generates several hundred command strings by composition and asserts the properties the
    specification states must hold for EVERY input, plus expectations for the two generator
    families where the spec determines the answer:

      family data      - a protected name composed into a data region (quote, heredoc,
                         here-string, comment, text-tool argument, request body). Every case
                         MUST be allow: R-CORE-2. A deny here is a payload false positive, the
                         defect class this design exists to eliminate; an ask here is a false
                         escalation and is reported separately.
      family malformed - systematic mutation of data-only seeds that contain NO separator and
                         never place the protected name after one: truncation at a quote, a
                         dropped heredoc terminator, a removed here-string terminator, an
                         unbalanced substitution. Because no mutation can create a command
                         position for the name, every case MUST be allow (R-OPEN-1), and a
                         mutation that provably breaks a region MUST be flagged as a fail-open
                         (R-OPEN-4).
      family property  - token soup including separators, quotes and interpreters. No fixed
                         expectation is derivable, so only the universal properties are checked.

    Universal properties, asserted for every case in every family:
      R-CORE-4   no crash: classification returns a decision object.
      R-CORE-3   determinism: classified twice, every property of the result is identical; and
                 the whole case list is classified again in reverse order with identical results.
      R-EXIT-4   the decision is exactly one of allow | ask | deny.
      R-EXIT-2   every deny carries a non-empty reason, a matched token, and a position.

.PARAMETER Seed
    Fixed default. The generator uses its own seeded linear congruential PRNG - never Get-Random,
    never the clock, never the filesystem - so the same seed produces the identical case list and
    the identical report on any machine.

.EXAMPLE
    pwsh -NoProfile -File agent_collab/scripts/Test-CommandPolicyFuzz.ps1
    pwsh -NoProfile -File agent_collab/scripts/Test-CommandPolicyFuzz.ps1 -Seed 424242
#>
[CmdletBinding()]
param(
    [int]$Seed = 20260729,

    # Number of family-property (token soup) cases to generate.
    [int]$PropertyCases = 140,

    # Print every generated case with its decision.
    [switch]$ShowAll
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$modulePath = Join-Path $PSScriptRoot 'CommandPolicy.psm1'
if (-not (Test-Path -LiteralPath $modulePath)) {
    Write-Error "CommandPolicy.psm1 not found next to this script (expected: $modulePath)"
    exit 1
}
Import-Module $modulePath -Force

# ---------------------------------------------------------------------------------------------
# Seeded PRNG. A 31-bit LCG implemented in explicit [long] arithmetic: no overflow, no reliance
# on any runtime's Random implementation, identical output on every platform and version.
# ---------------------------------------------------------------------------------------------
$script:PrngState = [long](($Seed % 2147483647) + 1)
function Next-Int {
    param([int]$Bound)   # returns 0 .. Bound-1
    $script:PrngState = (($script:PrngState * 1103515245L) + 12345L) % 2147483648L
    if ($script:PrngState -lt 0) { $script:PrngState += 2147483648L }
    return [int]([long]($script:PrngState / 65536L) % [long]$Bound)
}
function Pick { param([object[]]$Items) return $Items[(Next-Int $Items.Count)] }

# ---------------------------------------------------------------------------------------------
# Building blocks
# ---------------------------------------------------------------------------------------------
$protectedNames = @(
    'UnrealEditor',
    'UnrealEditor-Cmd.exe',
    'RunUAT.bat',
    'GenerateProjectFiles.bat',
    'BuildCookRun',
    'UnrealPak.exe',
    'UnrealBuildTool',
    'Build.bat',
    'git lfs pull',
    'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
)

# Data-region templates. {0} is where the protected name goes. Every one of these keeps the name
# in a non-command position, so R-CORE-2 requires allow.
$dataTemplates = @(
    @{ Id = 'dq-arg';        T = 'echo "{0}"' }
    @{ Id = 'sq-arg';        T = "echo '{0}'" }
    @{ Id = 'grep-pattern';  T = 'grep -F "{0}" build.log' }
    @{ Id = 'rg-pattern';    T = "rg -F '{0}' --stats agent_collab" }
    @{ Id = 'sed-script';    T = "sed -i 's|{0}|gate.ps1|g' notes.md" }
    @{ Id = 'awk-pattern';   T = "awk '/{0}/ {{print NR}}' build.log" }
    @{ Id = 'selectstring';  T = "Select-String -Pattern '{0}' -Path *.md" }
    @{ Id = 'commit-msg';    T = 'git commit -m "docs: never call {0} directly"' }
    @{ Id = 'log-grep';      T = 'git log --grep="{0}" --oneline' }
    @{ Id = 'printf-arg';    T = "printf 'tool %s\n' '{0}'" }
    @{ Id = 'write-output';  T = 'Write-Output "{0}"' }
    @{ Id = 'json-body';     T = 'curl -s -d ''{{"exe":"{0}"}}'' http://localhost:9999/x' }
    @{ Id = 'comment-line';  T = '# {0} is what gate.ps1 does internally' }
    @{ Id = 'trailing-cmt';  T = 'ls -la # then someone runs {0} by hand' }
    @{ Id = 'path-arg';      T = 'ls -la "{0}"' }
    @{ Id = 'hash-arg';      T = 'Get-FileHash "{0}" -Algorithm SHA256' }
    @{ Id = 'heredoc';       T = "cat <<EOF`nnote: {0} is never called here`nEOF" }
    @{ Id = 'heredoc-q';     T = "cat <<'EOF'`n{0}`nEOF" }
    @{ Id = 'heredoc-dash';  T = "cat <<-EOT`n`t{0}`n`tEOT" }
    @{ Id = 'herestring-l';  T = "`$doc = @'`n{0}`n'@" }
    @{ Id = 'herestring-e';  T = "`$doc = @`"`n{0}`n`"@" }
    @{ Id = 'nested-echo';   T = 'bash -c "echo {0}"' }
    @{ Id = 'nested-grep';   T = 'pwsh -Command "Select-String -Pattern ''{0}'' -Path *.md"' }
    @{ Id = 'assign-rhs';    T = 'UE_TOOL="{0}"' }
    @{ Id = 'assign-prefix'; T = 'UE_TOOL="{0}" ls -la' }
    @{ Id = 'base64ish';     T = "echo 'ZXhlPUJhc2U2NDo{0}' | base64 -d" }
    @{ Id = 'lfs-prose';     T = 'git commit -m "reverted {0} because it churns binaries"' }
    @{ Id = 'tee-note';      T = 'echo "{0}" | tee -a notes.txt' }
)

# Mutation seeds: no separator anywhere, and the protected name is always preceded by an ordinary
# word, so no truncation or region break can promote it to a command position.
$mutationSeeds = @(
    @{ Id = 'seed-dq';    T = 'echo NOTE {0} here' }
    @{ Id = 'seed-dqq';   T = 'echo "NOTE {0} here"' }
    @{ Id = 'seed-sq';    T = "echo 'NOTE {0} here'" }
    @{ Id = 'seed-grep';  T = 'grep -F "NOTE {0}" build.log' }
    @{ Id = 'seed-msg';   T = 'git commit -m "note {0} here"' }
    @{ Id = 'seed-hd';    T = "cat <<EOF`nnote {0} here`nEOF" }
    @{ Id = 'seed-hs';    T = "`$doc = @'`nnote {0} here`n'@" }
    @{ Id = 'seed-hse';   T = "`$doc = @`"`nnote {0} here`n`"@" }
    @{ Id = 'seed-sub';   T = 'echo "note $(basename {0}) here"' }
)

$soupTokens = @(
    ';', '&&', '||', '|', '&', '|&', "`n", '$(', ')', '`', '"', "'", '#', '<<EOF', 'EOF',
    '@''', '''@', '\', '{', '}', '(', ')', '2>&1', '>out.txt',
    'echo', 'ls -la', 'grep -F', 'cat', 'bash -c', 'pwsh -Command', 'cmd /c', 'sh -c',
    'Start-Process', 'iex', '&', '.', 'git commit -m', 'FOO=bar', '$TOOL', '${CMD}', '%TOOL%',
    '$env:UE_TOOL', 'gate.ps1', '-run', '-File', 'notes.md',
    'UnrealEditor', 'UnrealEditor-Cmd.exe', 'RunUAT.bat', 'git lfs pull', 'UnrealPak.exe'
)

# ---------------------------------------------------------------------------------------------
# Case generation
# ---------------------------------------------------------------------------------------------
$cases = [System.Collections.Generic.List[hashtable]]::new()

# family data ---------------------------------------------------------------------------------
foreach ($tpl in $dataTemplates) {
    foreach ($name in $protectedNames) {
        $cases.Add(@{
            Id     = "data/$($tpl.Id)/$([array]::IndexOf($protectedNames, $name))"
            Family = 'data'
            Expect = 'allow'
            Cmd    = ($tpl.T -f $name)
        })
    }
}

# family malformed ----------------------------------------------------------------------------
function Get-Mutations {
    param([string]$Text)
    $out = [System.Collections.Generic.List[hashtable]]::new()

    # truncate at every quote character
    $quoteIdx = @()
    for ($i = 0; $i -lt $Text.Length; $i++) {
        if ($Text[$i] -eq '"' -or $Text[$i] -eq "'") { $quoteIdx += $i }
    }
    # Broken = $null means "this oracle does not know", and R-OPEN-4 is NOT asserted for it.
    # It must not claim $true here. Truncating at the LAST quote reproduces the balanced original
    # verbatim (`git commit -m "note X here"`), which is well-formed, so the classifier rightly
    # analyses it fully and emits no fail-open -- that produced 20 false R-OPEN-4 violations.
    # Deciding brokenness properly would mean re-implementing the lexical rules under test: a plain
    # quote closes on one character, but a here-string delimiter is TWO (@' ... '@), so truncating
    # seed-hs/seed-hse at their final quote leaves a genuinely unterminated region while the quote
    # count looks balanced. An oracle that reimplements the implementation cannot falsify it.
    # R-OPEN-4 keeps its coverage from the mutation kinds below that are broken BY CONSTRUCTION.
    foreach ($qi in $quoteIdx) {
        $out.Add(@{ Kind = "truncate-at-quote-$qi"; Text = $Text.Substring(0, $qi + 1); Broken = $null })
    }
    # drop the final quote character
    if ($quoteIdx.Count -gt 0) {
        $last = $quoteIdx[-1]
        $out.Add(@{ Kind = 'drop-last-quote'; Text = $Text.Remove($last, 1); Broken = $true })
    }
    # drop a heredoc / here-string terminator line
    $nl = "`n"
    if ($Text -match '<<-?[''"]?\w') {
        $parts = @($Text -split $nl)
        if ($parts.Count -ge 2) {
            $out.Add(@{ Kind = 'drop-heredoc-terminator'; Text = (($parts[0..($parts.Count - 2)]) -join $nl); Broken = $true })
        }
    }
    if ($Text -match "@['`"]") {
        $out.Add(@{ Kind = 'drop-herestring-terminator'; Text = ($Text -replace "(?m)^['`"]@\s*$", ''); Broken = $true })
    }
    # unbalance a substitution
    $out.Add(@{ Kind = 'append-open-subst'; Text = ($Text + ' $('); Broken = $true })
    if ($Text -match '\$\(') {
        $out.Add(@{ Kind = 'drop-subst-close'; Text = ($Text -replace '\)', ''); Broken = $true })
    }
    # truncate at a random interior offset (still no separator to promote the name)
    if ($Text.Length -gt 6) {
        $cut = 3 + (Next-Int ($Text.Length - 4))
        $out.Add(@{ Kind = "truncate-at-$cut"; Text = $Text.Substring(0, $cut); Broken = $false })
    }
    return $out
}

# NOTE: the loop variable must NOT be named $seed. PowerShell variable names are case-insensitive,
# so $seed and the [int]$Seed parameter (see param block) are the SAME variable; binding a hashtable
# to it throws "Cannot convert ... Hashtable ... to type System.Int32" before a single case is built.
foreach ($ms in $mutationSeeds) {
    foreach ($name in @($protectedNames[0], $protectedNames[1], $protectedNames[2], $protectedNames[8])) {
        $base = ($ms.T -f $name)
        foreach ($m in (Get-Mutations -Text $base)) {
            $cases.Add(@{
                Id       = "malformed/$($ms.Id)/$($m.Kind)/$([array]::IndexOf($protectedNames, $name))"
                Family   = 'malformed'
                Expect   = 'allow'
                Cmd      = $m.Text
                # NOT [bool]: that collapsed the tri-state, turning "unknown" into "not broken".
                Broken   = $m.Broken
            })
        }
    }
}

# family property -----------------------------------------------------------------------------
for ($i = 0; $i -lt $PropertyCases; $i++) {
    $len = 3 + (Next-Int 10)
    $sb = [System.Text.StringBuilder]::new()
    for ($t = 0; $t -lt $len; $t++) {
        if ($t -gt 0) { [void]$sb.Append(' ') }
        [void]$sb.Append((Pick $soupTokens))
    }
    $cases.Add(@{
        Id     = "property/soup/$i"
        Family = 'property'
        Expect = $null
        Cmd    = $sb.ToString()
    })
}

# a handful of scale / degenerate cases, still deterministic
$cases.Add(@{ Id = 'property/empty';      Family = 'property'; Expect = $null; Cmd = '' })
$cases.Add(@{ Id = 'property/ws';         Family = 'property'; Expect = $null; Cmd = "  `t `n  " })
$cases.Add(@{ Id = 'property/long';       Family = 'property'; Expect = $null; Cmd = ('echo "' + ('UnrealEditor-Cmd.exe ' * 3000) + '"') })
$cases.Add(@{ Id = 'property/deep';       Family = 'property'; Expect = $null; Cmd = ('echo ' + ('$(' * 200) + 'echo hi' + (')' * 200)) })
$cases.Add(@{ Id = 'property/ctrl';       Family = 'property'; Expect = $null; Cmd = ("echo `"$([char]1)$([char]2)$([char]27) UnrealEditor-Cmd.exe`"") })
$cases.Add(@{ Id = 'property/nulish';     Family = 'property'; Expect = $null; Cmd = ("echo UnrealEditor`u{0000}-Cmd.exe") })

# ---------------------------------------------------------------------------------------------
# Classification
# ---------------------------------------------------------------------------------------------
function Get-Snapshot {
    param($Result)
    if ($null -eq $Result) { return $null }
    $s = [ordered]@{}
    foreach ($p in $Result.PSObject.Properties) {
        $s[$p.Name] = if ($null -eq $p.Value) { '<null>' } else { [string]$p.Value }
    }
    return $s
}
function Compare-Snapshot {
    param($A, $B)
    if ($null -eq $A -or $null -eq $B) { return @('one snapshot was null') }
    $d = [System.Collections.Generic.List[string]]::new()
    $keys = @($A.Keys) + @($B.Keys) | Select-Object -Unique
    foreach ($k in $keys) {
        $x = if ($A.Contains($k)) { $A[$k] } else { '<absent>' }
        $y = if ($B.Contains($k)) { $B[$k] } else { '<absent>' }
        if ($x -ne $y) { $d.Add("$k : '$x' vs '$y'") }
    }
    return $d
}

$positionPattern = '^(Position|Index|Offset|Column|CharIndex|TokenPosition|TokenIndex|Start|StartIndex|Location)$'

$violations = [System.Collections.Generic.List[hashtable]]::new()
$records    = [System.Collections.Generic.List[hashtable]]::new()

function Invoke-Classify {
    param([string]$Cmd)
    $r = Get-CommandClassification -Command $Cmd
    return $r
}

foreach ($case in $cases) {
    $rec = @{ Id = $case.Id; Family = $case.Family; Cmd = $case.Cmd; Decision = '(error)'; Reason = ''; Fallback = $false }
    $snapA = $null
    $snapB = $null
    try {
        $r1 = Invoke-Classify -Cmd $case.Cmd
        $snapA = Get-Snapshot $r1
        $r2 = Invoke-Classify -Cmd $case.Cmd
        $snapB = Get-Snapshot $r2

        $names = @($r1.PSObject.Properties.Name)
        $rec.Decision = [string]$r1.Decision
        if ($names -contains 'Reason')         { $rec.Reason   = [string]$r1.Reason }
        if ($names -contains 'ParserFallback') { $rec.Fallback = [bool]$r1.ParserFallback }
        $token = if ($names -contains 'MatchedToken') { $r1.MatchedToken } else { $null }
        $posProp = @($names | Where-Object { $_ -match $positionPattern }) | Select-Object -First 1
        $position = if ($posProp) { $r1.$posProp } else { $null }

        # R-EXIT-4
        if (@('allow', 'ask', 'deny') -notcontains $rec.Decision) {
            $violations.Add(@{ Rule = 'R-EXIT-4'; Id = $case.Id; Detail = "decision '$($rec.Decision)' outside allow|ask|deny"; Cmd = $case.Cmd })
        }
        # R-CORE-3 (repeat)
        $diff = Compare-Snapshot $snapA $snapB
        if (@($diff).Count -gt 0) {
            $violations.Add(@{ Rule = 'R-CORE-3'; Id = $case.Id; Detail = "repeat classification differed: $((@($diff)) -join '; ')"; Cmd = $case.Cmd })
        }
        # R-EXIT-2
        if ($rec.Decision -eq 'deny') {
            $lacks = [System.Collections.Generic.List[string]]::new()
            if ([string]::IsNullOrWhiteSpace($rec.Reason)) { $lacks.Add('reason') }
            if ($null -eq $token -or [string]::IsNullOrWhiteSpace([string]$token)) { $lacks.Add('matched token') }
            if ($null -eq $posProp) { $lacks.Add('position property') }
            elseif ($null -eq $position -or [string]::IsNullOrWhiteSpace([string]$position)) { $lacks.Add("position value ($posProp)") }
            if ($lacks.Count -gt 0) {
                $violations.Add(@{ Rule = 'R-EXIT-2'; Id = $case.Id; Detail = "deny without $($lacks -join ', ')"; Cmd = $case.Cmd })
            }
        }
        # family expectations
        if ($case.Family -eq 'data') {
            if ($rec.Decision -eq 'deny') {
                $violations.Add(@{ Rule = 'R-CORE-2'; Id = $case.Id; Detail = 'PAYLOAD FALSE POSITIVE: data-only input denied'; Cmd = $case.Cmd })
            } elseif ($rec.Decision -ne 'allow') {
                $violations.Add(@{ Rule = 'R-CORE-2'; Id = $case.Id; Detail = "false escalation: data-only input returned '$($rec.Decision)'"; Cmd = $case.Cmd })
            }
        }
        elseif ($case.Family -eq 'malformed') {
            if ($rec.Decision -eq 'deny') {
                $violations.Add(@{ Rule = 'R-OPEN-1'; Id = $case.Id; Detail = 'malformed data-only input denied instead of failing open'; Cmd = $case.Cmd })
            } elseif ($rec.Decision -ne 'allow') {
                $violations.Add(@{ Rule = 'R-OPEN-1'; Id = $case.Id; Detail = "malformed data-only input returned '$($rec.Decision)' instead of allow"; Cmd = $case.Cmd })
            }
            # -eq $true, not truthiness: $null means the oracle cannot decide brokenness (see
            # Get-Mutations) and must not assert R-OPEN-4 either way.
            if ($case.ContainsKey('Broken') -and $case.Broken -eq $true -and -not $rec.Fallback) {
                $violations.Add(@{ Rule = 'R-OPEN-4'; Id = $case.Id; Detail = 'a broken region was not reported as a fail-open'; Cmd = $case.Cmd })
            }
        }
    }
    catch {
        $violations.Add(@{ Rule = 'R-CORE-4'; Id = $case.Id; Detail = "classifier threw: $($_.Exception.Message)"; Cmd = $case.Cmd })
    }
    $records.Add($rec)
}

# ---- order independence (R-CORE-3): classify the whole list again, reversed ------------------
$reverseDecisions = @{}
$reversed = @($cases) ; [array]::Reverse($reversed)
foreach ($case in $reversed) {
    try {
        $r = Invoke-Classify -Cmd $case.Cmd
        $reverseDecisions[$case.Id] = (Get-Snapshot $r)
    }
    catch {
        $reverseDecisions[$case.Id] = $null
        $violations.Add(@{ Rule = 'R-CORE-4'; Id = $case.Id; Detail = "classifier threw on reverse pass: $($_.Exception.Message)"; Cmd = $case.Cmd })
    }
}
foreach ($rec in $records) {
    if (-not $reverseDecisions.ContainsKey($rec.Id)) { continue }
    $rev = $reverseDecisions[$rec.Id]
    if ($null -eq $rev) { continue }
    $decRev = if ($rev.Contains('Decision')) { $rev['Decision'] } else { '<absent>' }
    if ($decRev -ne $rec.Decision) {
        $violations.Add(@{ Rule = 'R-CORE-3'; Id = $rec.Id
            Detail = "order dependence: forward '$($rec.Decision)', reverse '$decRev'"; Cmd = $rec.Cmd })
    }
}

# ---------------------------------------------------------------------------------------------
# Report
# ---------------------------------------------------------------------------------------------
function Show-Line { param([string]$Text) Write-Host $Text }
function Escape-Cmd { param([string]$Text) return ($Text | ConvertTo-Json -Compress) }

$sep = ('-' * 78)
Show-Line ''
Show-Line 'COMMAND POLICY FUZZ / PROPERTY REPORT'
Show-Line $sep
Show-Line ("module        : {0}" -f $modulePath)
Show-Line ("seed          : {0}   (deterministic: seeded LCG, no clock, no Get-Random)" -f $Seed)
Show-Line ("property cases: {0}" -f $PropertyCases)
Show-Line ("total cases   : {0}   (each classified twice, plus one reversed-order pass)" -f $records.Count)
Show-Line ''

Show-Line 'CASES BY FAMILY AND DECISION'
Show-Line $sep
Show-Line ("  {0,-12} {1,6} {2,8} {3,6} {4,6} {5,10} {6,9}" -f 'family', 'cases', 'allow', 'ask', 'deny', 'fail-open', '(error)')
foreach ($fam in @('data', 'malformed', 'property')) {
    $g = @($records | Where-Object { $_.Family -eq $fam })
    Show-Line ("  {0,-12} {1,6} {2,8} {3,6} {4,6} {5,10} {6,9}" -f $fam, $g.Count,
        @($g | Where-Object { $_.Decision -eq 'allow' }).Count,
        @($g | Where-Object { $_.Decision -eq 'ask'   }).Count,
        @($g | Where-Object { $_.Decision -eq 'deny'  }).Count,
        @($g | Where-Object { $_.Fallback }).Count,
        @($g | Where-Object { $_.Decision -eq '(error)' }).Count)
}
Show-Line ''

Show-Line 'PROPERTY VIOLATIONS BY RULE'
Show-Line $sep
if ($violations.Count -eq 0) {
    Show-Line '  (none)'
} else {
    foreach ($grp in ($violations | Group-Object -Property { $_.Rule } | Sort-Object Name)) {
        Show-Line ("  {0,-10} {1}" -f $grp.Name, @($grp.Group).Count)
    }
}
Show-Line ''

if ($violations.Count -gt 0) {
    Show-Line 'VIOLATION DETAIL  (rule | id | detail | input as a JSON string)'
    Show-Line $sep
    foreach ($v in ($violations | Sort-Object -Property { $_.Rule }, { $_.Id })) {
        Show-Line ("  {0} | {1} | {2}" -f $v.Rule, $v.Id, $v.Detail)
        Show-Line ("      input: {0}" -f (Escape-Cmd $v.Cmd))
    }
    Show-Line ''
}

if ($ShowAll) {
    Show-Line 'ALL CASES'
    Show-Line $sep
    foreach ($rec in $records) {
        Show-Line ("  {0,-9} {1,-46} {2}" -f $rec.Decision, $rec.Id, (Escape-Cmd $rec.Cmd))
    }
    Show-Line ''
}

$denyData = @($records | Where-Object { $_.Family -eq 'data' -and $_.Decision -eq 'deny' }).Count
Show-Line $sep
$verdict = if ($violations.Count -eq 0) { 'PASS' } else { 'FAIL' }
Show-Line ("RESULT: {0} - seed {1}, {2} cases, {3} property violation(s), {4} payload false positive(s)" -f
    $verdict, $Seed, $records.Count, $violations.Count, $denyData)
Show-Line ''

if ($verdict -eq 'PASS') { exit 0 } else { exit 1 }
