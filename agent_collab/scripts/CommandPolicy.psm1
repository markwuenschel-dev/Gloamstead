#requires -Version 7
<#
.SYNOPSIS
    Deterministic command classifier: decides whether a submitted shell/PowerShell command
    string INVOKES a protected Unreal executable, or merely MENTIONS one as data.

.DESCRIPTION
    Get-CommandClassification -Command '<string>' [-PolicyPath '<path>'] returns exactly one of:

      deny  - an unambiguous direct invocation of a protected executable at a real command
              position (including literal syntactic layers down inside nested interpreters, and
              inside a heredoc body fed to a command that executes standard input).
      ask   - a recognisable nested-interpreter or process-launch pattern COULD invoke one, but
              the final executable cannot be proven (variable/env expansion at a command
              position, Start-Process with an unprovable target, pwsh -Command "& $var",
              cmd /c "%VAR%", -EncodedCommand that will not decode, a heredoc body whose owning
              command may or may not execute standard input).
      allow - the protected name appears only as DATA (prose, search argument, commit message,
              heredoc body fed to a text tool, here-string content, comment, string literal), or
              nothing protected appears at all.

    THE PROTECTED SET AND EVERY LEXER TABLE LIVE IN agent_collab/context/command_policy.json
    (key: "classifier"). This module hardcodes none of them (R-PROT-1, R-PROT-4). Adding a target
    is a JSON edit. -PolicyPath overrides the location, which is the seam the corpus uses.

    This replaces the previous appearance-matching regex, which classified by how a command
    LOOKED rather than by what it would RUN. That regex had a proven defect: a bash heredoc
    whose prose body contained a quoted engine path was DENIED, though nothing was invoked.

.NOTES
    ===== LIMITS - READ BEFORE RELYING ON THIS =====

    This examines the SUBMITTED COMMAND SYNTAX ONLY. Specifically it does NOT:
      * observe or classify processes spawned by the command (children or grandchildren);
      * read the body of any script the command runs, so it cannot make a wrapper script safe
        (`pwsh -File gate.ps1` and `bash ./build.sh` are ALLOW even though the script itself
        launches UnrealEditor-Cmd - R-NEST-2, an accepted and deliberate blind spot);
      * constitute worker containment, sandboxing, or a security boundary.

    It is a MISTAKE-PREVENTION GUARD against a careless direct call. One level of script
    indirection defeats it by construction. Real containment remains: handoff file_ownership,
    edit-scope guards, worktree isolation, vendor immutability, Critic audits.

    ===== FAIL-OPEN POLICY (R-OPEN-1 .. R-OPEN-6) =====

    Malformed or unsupported syntax FAILS OPEN: ParserFallback = $true and Reason carries a
    diagnostic naming the specific malformation.

    A FAIL-OPEN NEVER TRUNCATES ANALYSIS (R-OPEN-5, rewritten 2026-07-29). Previously every
    malformation did `Set-PolicyFallback; return`, abandoning the rest of the string - which made
    every malformation a universal off switch (`bash -c 'x' <<9 ; UnrealEditor-Cmd.exe` was
    allowed). Now a malformation records the fallback, skips only the region it cannot read, and
    continues scanning the remainder in degraded mode. Each malformation declares its own extent:

      * unterminated single/double quote, unbalanced $( ), unterminated ${ }, unterminated
        PowerShell block comment:
        extent is the OPENER ONLY. Scanning resumes immediately after it, so a separator and an
        invocation later in the string are still found.
      * unterminated PowerShell here-string, heredoc whose terminator never appears, heredoc
        operator with no delimiter word, unterminated quoted heredoc delimiter: extent runs to
        END OF INPUT, because a line-oriented body of unknown length cannot be bounded. Nothing
        after it is trusted, and nothing after it is claimed.
      * nesting deeper than classifier.max_depth: extent is that SUBTREE only (R-OPEN-6); the
        remainder of the enclosing command is still scanned.

    Precedence (R-PREC-1..4): a deny proven anywhere in a readable region survives any
    malformation. An ask raised BEFORE the first malformation survives it (R-PREC-2); an ask
    raised only AFTER a malformation degrades to the fail-open allow (R-PREC-3).

    ===== DECISION: LITERAL PROTECTED EXE INSIDE A NESTED INTERPRETER => deny =====

    `pwsh -Command "& 'D:\UE\...\UnrealEditor-Cmd.exe'"`, `cmd /c "RunUAT.bat ..."`,
    `bash -c 'UnrealEditor ...'` and `Start-Process -FilePath '...\UnrealEditor.exe'` are
    classified DENY, not ASK.

    Why: the payload is a literal string with no expansion in it, so the executable at the
    nested command position is PROVEN, not guessed - which is exactly the definition of deny.
    Treating it as 'ask' would also make the guard trivially defeatable: wrapping any denied
    command in `bash -c '...'` would downgrade every deny to a prompt.

    Counter-argument, recorded honestly: a nested payload is one step further from the user's
    intent than a bare command line, so a false positive there is more surprising. That risk is
    accepted because the payload must be wholly literal for deny to fire; any expansion
    anywhere in the resolved payload routes to ask instead.

    ===== STANDARD INPUT IS NOT AUTOMATICALLY DATA (R-STDIN-1..3) =====

    `bash <<EOF` / `UnrealEditor-Cmd.exe -run=Cook` / `EOF` really executes the editor, so a
    heredoc body is scanned as COMMANDS when its owning command executes standard input, and
    treated as data when the owner is a text tool. The distinction is the owner, not the syntax:
    `cat <<EOF` is data, `bash <<EOF` is commands, `bash -c 'x' <<EOF` is data again (the -c
    payload is what runs). When the owner cannot be resolved, a protected name at an apparent
    command position in the body yields `ask`, never a silent allow.

    ===== KNOWN LIMITS OF THE LEXER =====
      * R-NEST-2 by design: a script path payload (`pwsh -File x.ps1`, `bash ./x.sh`,
        `make target`, `msbuild x.sln`) is ALLOW. The script body is out of field of view.
      * A pass-through prefix whose value-taking flag is not declared in
        classifier.pass_through_prefixes[].value_flags loses the command position at that flag's
        value (`sudo -u root UnrealEditor-Cmd.exe` is not caught; `sudo -u UnrealEditor` is not
        a false deny either, because -u is declared and its value is skipped).
      * Positions reported for findings inside a decoded -EncodedCommand payload are anchored at
        the interpreter token; positions inside an ordinary literal payload are exact, because
        the payload is taken as a raw substring of the input rather than rebuilt from tokens.
      * Backtick handling is heuristic: a backtick pair whose closing backtick is followed by a
        token break is read as bash command substitution; otherwise the backtick is read as a
        PowerShell escape character.
      * `~` is deliberately NOT treated as an expansion. `~/bin/tool` leaves the basename
        provable, and escalating every home-relative command would train an operator to
        rubber-stamp asks (see the R-PREC-3 rationale in the spec).
      * Foreign-language eval payloads (`python -c`, `node -e`, `perl -e`, `ruby -e`) are not
        parsed as those languages. They are scanned as a command string, and if a protected name
        is merely MENTIONED in them the result is `ask` (classifier.foreign_eval_mention_ask).
        This is an over-escalation by construction, chosen over a silent allow.
#>

Set-StrictMode -Version Latest

$script:PolicyCache = @{}
$script:Rank = @{ 'allow' = 0; 'ask' = 1; 'deny' = 2 }

# ---------------------------------------------------------------------------------------------
# Policy loading - agent_collab/context/command_policy.json is the single source of truth.
# ---------------------------------------------------------------------------------------------

function Get-PolicyDefaultPath {
    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..' 'context' 'command_policy.json'))
}

function New-PolicySet {
    param([object[]]$Items)
    $set = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::OrdinalIgnoreCase)
    if ($null -ne $Items) {
        foreach ($it in $Items) {
            if ($null -eq $it) { continue }
            $s = [string]$it
            if ($s.Length -gt 0) { [void]$set.Add($s) }
        }
    }
    return $set
}

function Get-PolicyJsonList {
    param([object]$Object, [string]$Name)
    if ($null -eq $Object) { return @() }
    if (($Object.PSObject.Properties.Name -contains $Name) -and $null -ne $Object.$Name) {
        return @($Object.$Name)
    }
    return @()
}

function Test-PolicySetMember {
    param([hashtable]$Sets, [string]$Key, [string]$Value)
    if ($null -eq $Sets -or -not $Sets.ContainsKey($Key)) { return $false }
    return $Sets[$Key].Contains($Value)
}

function New-PolicyModel {
    param([string]$PolicyPath)

    $model = @{
        Loaded  = $false
        Error   = ''
        Source  = $PolicyPath
    }

    if (-not (Test-Path -LiteralPath $PolicyPath -PathType Leaf)) {
        $model.Error = "command policy declaration not found at '$PolicyPath'"
        return $model
    }

    $raw = $null
    try { $raw = Get-Content -LiteralPath $PolicyPath -Raw -Encoding UTF8 }
    catch { $model.Error = "command policy '$PolicyPath' could not be read: $($_.Exception.Message)"; return $model }

    $json = $null
    try { $json = $raw | ConvertFrom-Json -ErrorAction Stop }
    catch { $model.Error = "command policy '$PolicyPath' is not valid JSON: $($_.Exception.Message)"; return $model }

    if ($null -eq $json -or
        ($json.PSObject.Properties.Name -notcontains 'classifier') -or $null -eq $json.classifier) {
        $model.Error = "command policy '$PolicyPath' has no 'classifier' object"
        return $model
    }
    $c = $json.classifier

    $names = @('protected_basenames', 'executable_suffix_pattern', 'expansion_pattern',
               'interpreter_families', 'pass_through_prefixes')
    foreach ($nm in $names) {
        if ($c.PSObject.Properties.Name -notcontains $nm -or $null -eq $c.$nm) {
            $model.Error = "command policy '$PolicyPath' classifier is missing required key '$nm'"
            return $model
        }
    }

    $protected = @(Get-PolicyJsonList $c 'protected_basenames')
    if ($protected.Count -eq 0) {
        $model.Error = "command policy '$PolicyPath' declares an empty protected_basenames list"
        return $model
    }

    try {
        $model.MaxDepth = 24
        if ($c.PSObject.Properties.Name -contains 'max_depth' -and $c.max_depth) { $model.MaxDepth = [int]$c.max_depth }
        $model.MaxInputLength = 200000
        if ($c.PSObject.Properties.Name -contains 'max_input_length' -and $c.max_input_length) {
            $model.MaxInputLength = [int]$c.max_input_length
        }

        $model.ExtensionRx = [regex]::new([string]$c.executable_suffix_pattern,
            [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
        $model.ExpansionRx = [regex]::new([string]$c.expansion_pattern)

        $model.Protected         = New-PolicySet $protected
        $model.ProtectedFiles    = New-PolicySet (Get-PolicyJsonList $c 'protected_filenames')
        $model.ProtectedPatterns = @(foreach ($p in (Get-PolicyJsonList $c 'protected_basename_patterns')) {
            [regex]::new([string]$p, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
        })

        # command -> @{ subcommand -> HashSet[verbs] }, plus command -> HashSet[global value flags]
        $model.SubCommands       = @{}
        $model.SubCommandGlobals = @{}
        foreach ($sc in (Get-PolicyJsonList $c 'protected_subcommands')) {
            $cmdName = ([string]$sc.command).ToLowerInvariant()
            if (-not $model.SubCommands.ContainsKey($cmdName)) { $model.SubCommands[$cmdName] = @{} }
            $model.SubCommands[$cmdName][([string]$sc.subcommand).ToLowerInvariant()] =
                New-PolicySet (Get-PolicyJsonList $sc 'verbs')
            $model.SubCommandGlobals[$cmdName] = New-PolicySet (Get-PolicyJsonList $sc 'global_value_flags')
        }

        # interpreter basename -> family
        $model.Families = @{}
        $fam = $c.interpreter_families
        foreach ($prop in $fam.PSObject.Properties) {
            foreach ($nm2 in @($prop.Value)) {
                $model.Families[([string]$nm2).ToLowerInvariant()] = $prop.Name
            }
        }

        $model.ForeignEvalMentionAsk = $true
        if ($c.PSObject.Properties.Name -contains 'foreign_eval_mention_ask') {
            $model.ForeignEvalMentionAsk = [bool]$c.foreign_eval_mention_ask
        }

        # pass-through name -> @{ ValueFlags; SkipPositionals }
        $model.PassThrough = @{}
        foreach ($pt in (Get-PolicyJsonList $c 'pass_through_prefixes')) {
            $ptName = ([string]$pt.name).ToLowerInvariant()
            $skip = 0
            if ($pt.PSObject.Properties.Name -contains 'skip_positionals' -and $pt.skip_positionals) {
                $skip = [int]$pt.skip_positionals
            }
            $model.PassThrough[$ptName] = @{
                ValueFlags      = New-PolicySet (Get-PolicyJsonList $pt 'value_flags')
                SkipPositionals = $skip
            }
        }

        $model.CallOperators   = New-PolicySet (Get-PolicyJsonList $c 'call_operators')
        $model.ControlKeywords = New-PolicySet (Get-PolicyJsonList $c 'control_keywords')

        $model.StdinData = New-PolicySet (Get-PolicyJsonList $c 'stdin_data_commands')
        $model.StdinExec = @{}
        if ($c.PSObject.Properties.Name -contains 'stdin_executors' -and $null -ne $c.stdin_executors) {
            foreach ($prop in $c.stdin_executors.PSObject.Properties) {
                if ($prop.Name -eq 'comment') { continue }
                $model.StdinExec[$prop.Name] = New-PolicySet @($prop.Value)
            }
        }

        $model.LauncherTargetParams = New-PolicySet (Get-PolicyJsonList $c 'start_process_target_params')
        $model.LauncherSwitches     = New-PolicySet (Get-PolicyJsonList $c 'start_process_switch_params')
        $model.LauncherValueParams  = New-PolicySet (Get-PolicyJsonList $c 'process_launcher_value_params')
    }
    catch {
        $model.Error = "command policy '$PolicyPath' could not be compiled: $($_.Exception.Message)"
        return $model
    }

    $model.Loaded = $true
    return $model
}

function Import-CommandPolicy {
    param([string]$PolicyPath)

    if ([string]::IsNullOrWhiteSpace($PolicyPath)) { $PolicyPath = Get-PolicyDefaultPath }
    $resolved = $PolicyPath
    try { $resolved = [System.IO.Path]::GetFullPath($PolicyPath) } catch { }

    $stamp = 'absent'
    try {
        $fi = [System.IO.FileInfo]::new($resolved)
        if ($fi.Exists) { $stamp = "$($fi.LastWriteTimeUtc.Ticks):$($fi.Length)" }
    } catch { }

    $key = "$resolved|$stamp"
    if ($script:PolicyCache.ContainsKey($key)) { return $script:PolicyCache[$key] }

    $model = New-PolicyModel -PolicyPath $resolved
    if (-not $model.Loaded) {
        # Loud, once per distinct policy state per process. Fail open: never brick the shell.
        [Console]::Error.WriteLine("POLICY WARNING: $($model.Error). The Unreal invocation rule is NOT being applied (fail-open). Fix agent_collab/context/command_policy.json.")
    }
    $script:PolicyCache[$key] = $model
    return $model
}

# ---------------------------------------------------------------------------------------------
# State helpers
# ---------------------------------------------------------------------------------------------

function New-PolicyState {
    return @{
        Decision          = 'allow'
        Reason            = 'No protected executable at any command position.'
        MatchedToken      = $null
        Position          = $null
        Fallback          = $false
        FallbackReason    = ''
        Fallbacks         = [System.Collections.Generic.List[string]]::new()
        Events            = 0
        MalformedSeen     = $false
        AskBeforeMalform  = $false
    }
}

function Add-PolicyFinding {
    param(
        [hashtable]$State,
        [string]$Decision,
        [string]$Reason,
        [string]$Token,
        [int]$Position,
        [bool]$SuppressAsk = $false
    )
    if ($Decision -eq 'ask' -and $SuppressAsk) { return }
    if ($script:Rank[$Decision] -gt $script:Rank[$State.Decision]) {
        $State.Decision     = $Decision
        $State.Reason       = $Reason
        $State.MatchedToken = $Token
        $State.Position     = $Position
    }
}

function Set-PolicyFallback {
    <#
      Kind matters for precedence:
        'malformed' - syntax the lexer could not read. Position analysis DOWNSTREAM of it is not
                      trustworthy enough to justify an escalation (R-PREC-3), so an ask raised
                      only after it degrades to the fail-open allow. An ask raised before it is
                      kept (R-PREC-2), and a proven deny is kept wherever it is (R-PREC-1).
        'ceiling'   - a declared resource limit (R-OPEN-3). It skips a subtree, it does not make
                      the rest of the string unreadable, so it never suppresses a later ask
                      (R-OPEN-6).
    #>
    param([hashtable]$State, [string]$Reason, [ValidateSet('malformed', 'ceiling')][string]$Kind = 'malformed')
    # Events counts every fallback, deduplicated Fallbacks does not. A caller that recursed into a
    # region and saw Events increase knows that region was unreadable, which is how R-PREC-3
    # ("the malformation encloses the construct") is implemented without truncating anything.
    $State.Events = $State.Events + 1
    if ($Kind -eq 'malformed' -and -not $State.MalformedSeen) {
        $State.MalformedSeen = $true
        if ($State.Decision -eq 'ask') { $State.AskBeforeMalform = $true }
    }
    if (-not $State.Fallback) {
        $State.Fallback       = $true
        $State.FallbackReason = $Reason
    }
    if ($State.Fallbacks.Count -lt 8 -and -not $State.Fallbacks.Contains($Reason)) {
        $State.Fallbacks.Add($Reason)
    }
}

# ---------------------------------------------------------------------------------------------
# Small lexical helpers
# ---------------------------------------------------------------------------------------------

$script:TokenBreakChars = " `t`r`n;|&()<>{}"

# Inside POSIX double quotes a backslash escapes only these; every other backslash is a literal
# path separator, which is what keeps "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" intact.
$script:PosixDqEscapes = [string]::Join('', @('$', [char]96, '\', '"', [char]10))

function Get-PolicyBasename {
    param([string]$Token)
    if ([string]::IsNullOrEmpty($Token)) { return '' }
    $t = $Token
    $slash = [Math]::Max($t.LastIndexOf('/'), $t.LastIndexOf('\'))
    if ($slash -ge 0) { $t = $t.Substring($slash + 1) }
    return $t
}

function Get-PolicyCanonicalName {
    # R-PROT-2a: Win32 strips trailing dots and whitespace when resolving a filename, so
    # `UnrealEditor-Cmd.exe.` and `'UnrealEditor-Cmd.exe '` both launch the binary. Canonicalise
    # before comparing anything.
    param([string]$Basename)
    if ([string]::IsNullOrEmpty($Basename)) { return '' }
    $b = $Basename.TrimEnd(@(' ', [char]9, [char]13, [char]10, '.'))
    return $b.ToLowerInvariant()
}

function Test-PolicyProtected {
    param([hashtable]$Policy, [string]$Basename)
    if ([string]::IsNullOrWhiteSpace($Basename)) { return $false }
    $canon = Get-PolicyCanonicalName -Basename $Basename
    if ($canon.Length -eq 0) { return $false }
    if ($Policy.ProtectedFiles.Contains($canon)) { return $true }
    $stem = $Policy.ExtensionRx.Replace($canon, '')
    if ($Policy.Protected.Contains($stem)) { return $true }
    foreach ($rx in $Policy.ProtectedPatterns) { if ($rx.IsMatch($stem)) { return $true } }
    return $false
}

function Test-PolicyExpansion {
    param([hashtable]$Policy, [string]$Text)
    if ([string]::IsNullOrEmpty($Text)) { return $false }
    return $Policy.ExpansionRx.IsMatch($Text)
}

function Get-PolicyStrippedName {
    # Interpreter / pass-through lookup key: basename, canonicalised, one exe suffix removed.
    param([hashtable]$Policy, [string]$Basename)
    if ([string]::IsNullOrWhiteSpace($Basename)) { return '' }
    $canon = Get-PolicyCanonicalName -Basename $Basename
    return [regex]::Replace($canon, '\.(exe|bat|cmd)$', '')
}

function Get-PolicyInterpreterFamily {
    param([hashtable]$Policy, [string]$Name)
    if ([string]::IsNullOrWhiteSpace($Name)) { return $null }
    if ($Policy.Families.ContainsKey($Name)) { return $Policy.Families[$Name] }
    return $null
}

function Find-PolicyClosingParen {
    <#
      $OpenIndex must point at the '('. Returns index of the matching ')' or -1.

      Quote-aware but DELIBERATELY TOLERANT (fixed 2026-07-29): an unterminated quote inside the
      substitution no longer aborts the search, and '#' is not treated as a comment here. The old
      version returned -1 for `$(true # don't)` because of the apostrophe, which - combined with
      the truncating fail-open - allowed everything after it. Erring toward finding the paren
      keeps analysis going; the contents are still scanned by the normal rules.
    #>
    param([string]$Text, [int]$OpenIndex)
    $depth = 0
    $i = $OpenIndex
    $n = $Text.Length
    while ($i -lt $n) {
        $c = $Text[$i]
        if ($c -eq '`' -and ($i + 1) -lt $n) { $i += 2; continue }
        if ($c -eq '\' -and ($i + 1) -lt $n -and ("`"'".IndexOf($Text[$i + 1]) -ge 0)) { $i += 2; continue }
        if ($c -eq "'" -or $c -eq '"') {
            $j = $Text.IndexOf($c, $i + 1)
            if ($j -lt 0) { $i++; continue }   # tolerate: treat the lone quote as an ordinary char
            $i = $j + 1
            continue
        }
        if ($c -eq '(') { $depth++ }
        elseif ($c -eq ')') {
            $depth--
            if ($depth -le 0) { return $i }
        }
        $i++
    }
    return -1
}

function Find-PolicyHereStringEnd {
    # Locate the terminator ('@ or "@) of a PowerShell here-string, at the start of a line
    # (leading whitespace tolerated). Returns the index of the quote char, or -1.
    param([string]$Text, [int]$From, [char]$Quote)
    $needle = [string]$Quote + '@'
    $search = $From
    while ($true) {
        $p = $Text.IndexOf($needle, $search)
        if ($p -lt 0) { return -1 }
        $q = $p - 1
        while ($q -ge 0 -and ($Text[$q] -eq ' ' -or $Text[$q] -eq [char]9)) { $q-- }
        if ($q -lt 0 -or $Text[$q] -eq [char]10 -or $Text[$q] -eq [char]13) { return $p }
        $search = $p + 1
    }
}

function Test-PolicyHereStringOpener {
    <#
      R-DATA-5: `@'` / `@"` opens a PowerShell here-string ONLY when nothing but whitespace
      follows it on that line - that is PowerShell's own rule. In a POSIX command line `@'x'` is
      an ordinary word, and hunting for a `'@` terminator there used to abandon the whole rest of
      the string (`grep @'x' f && UnrealEditor-Cmd.exe` was allowed).
    #>
    param([string]$Text, [int]$QuoteIndex)
    $j = $QuoteIndex + 1
    $n = $Text.Length
    while ($j -lt $n) {
        $ch = $Text[$j]
        if ($ch -eq [char]10) { return $true }
        if ($ch -eq ' ' -or $ch -eq [char]9 -or $ch -eq [char]13) { $j++; continue }
        return $false
    }
    return $true   # end of input: nothing but whitespace followed
}

# ---------------------------------------------------------------------------------------------
# Token reader
# ---------------------------------------------------------------------------------------------

function Read-PolicyToken {
    <#
      Reads one shell word starting at $Start. Quote bodies are appended as literal content
      (they are data, never command positions), $( ) inside a token or inside double quotes is
      recursed into as a nested command context, and escapes are consumed.

      Returns @{ Text; End; Malformed; MalformedReason; ResumeAt; Degraded }.

      ResumeAt (added 2026-07-29 for R-OPEN-5) is where the SCANNER must continue after a
      malformation: just past the opener it could not close, never end-of-input.

      Degraded is $true when a substitution inside this token could not be fully analysed (an
      unbalanced opener, or a nesting ceiling). The token's value is then unknowable for reasons
      the lexer has already reported, so it must NOT also be escalated as an unprovable
      expansion - that is R-PREC-3, a malformation enclosing the suspicious construct.
    #>
    param(
        [string]$Text,
        [int]$Start,
        [int]$Base,
        [int]$Depth,
        [hashtable]$State,
        [hashtable]$Policy,
        [string]$Dialect,
        [bool]$SuppressAsk
    )

    $sb = [System.Text.StringBuilder]::new()
    $i  = $Start
    $n  = $Text.Length
    $degraded = $false

    while ($i -lt $n) {
        $c = $Text[$i]

        if ($script:TokenBreakChars.IndexOf($c) -ge 0) { break }

        # ---- single-quoted segment: pure data -------------------------------------------
        if ($c -eq "'") {
            $open = $i
            $j = $i
            $closed = -1
            while ($true) {
                $j = $Text.IndexOf("'", $j + 1)
                if ($j -lt 0) { break }
                # PowerShell escapes a quote inside a literal string by doubling it.
                if (($j + 1) -lt $n -and $Text[$j + 1] -eq "'") { $j++; continue }
                $closed = $j
                break
            }
            if ($closed -lt 0) {
                return @{ Text = $sb.ToString(); End = $n; Malformed = $true
                          MalformedReason = 'unterminated single quote'; ResumeAt = ($open + 1); Degraded = $true }
            }
            [void]$sb.Append($Text.Substring($open + 1, $closed - $open - 1))
            $i = $closed + 1
            continue
        }

        # ---- double-quoted segment: data, except $( ) substitution ------------------------
        if ($c -eq '"') {
            $open = $i
            $i++
            $closed = $false
            while ($i -lt $n) {
                $ch = $Text[$i]

                if ($ch -eq '"') { $closed = $true; $i++; break }

                # R-DATA-6: an escaped quote is never a region delimiter. In POSIX dialects a
                # backslash inside double quotes escapes only $ ` " \ and newline; every other
                # backslash is a literal path separator, so "D:\UE_5.8\Engine\..." is intact.
                # cmd.exe has no backslash escape at all, so in the cmd dialect \" closes.
                if ($ch -eq '\' -and $Dialect -ne 'cmd' -and ($i + 1) -lt $n -and
                    $script:PosixDqEscapes.IndexOf($Text[$i + 1]) -ge 0) {
                    [void]$sb.Append($Text[$i + 1]); $i += 2; continue
                }

                if ($ch -eq '`' -and $Dialect -ne 'cmd' -and ($i + 1) -lt $n) {
                    # PowerShell escape inside a double-quoted string.
                    [void]$sb.Append($Text[$i + 1]); $i += 2; continue
                }

                if ($ch -eq '$' -and ($i + 1) -lt $n -and $Text[$i + 1] -eq '(') {
                    $close = Find-PolicyClosingParen -Text $Text -OpenIndex ($i + 1)
                    if ($close -lt 0) {
                        return @{ Text = $sb.ToString(); End = $n; Malformed = $true
                                  MalformedReason = 'unbalanced $( ) inside double quotes'
                                  ResumeAt = ($i + 2); Degraded = $true }
                    }
                    $inner = $Text.Substring($i + 2, $close - $i - 2)
                    $ev0 = $State.Events
                    Invoke-PolicyScan -Text $inner -Base ($Base + $i + 2) -Depth ($Depth + 1) `
                        -State $State -Policy $Policy -Dialect $Dialect -SuppressAsk $SuppressAsk
                    if ($State.Events -ne $ev0) { $degraded = $true }
                    [void]$sb.Append('$()')
                    $i = $close + 1
                    continue
                }

                [void]$sb.Append($ch)
                $i++
            }
            if (-not $closed) {
                return @{ Text = $sb.ToString(); End = $n; Malformed = $true
                          MalformedReason = 'unterminated double quote'; ResumeAt = ($open + 1); Degraded = $true }
            }
            continue
        }

        # ---- ${name} expansion -------------------------------------------------------------
        if ($c -eq '$' -and ($i + 1) -lt $n -and $Text[$i + 1] -eq '{') {
            $j = $Text.IndexOf('}', $i + 2)
            if ($j -lt 0) {
                return @{ Text = $sb.ToString(); End = $n; Malformed = $true
                          MalformedReason = 'unterminated ${ } expansion'; ResumeAt = ($i + 2); Degraded = $true }
            }
            [void]$sb.Append($Text.Substring($i, $j - $i + 1))
            $i = $j + 1
            continue
        }

        # ---- $( ) substitution inside an unquoted token --------------------------------------
        if ($c -eq '$' -and ($i + 1) -lt $n -and $Text[$i + 1] -eq '(') {
            $close = Find-PolicyClosingParen -Text $Text -OpenIndex ($i + 1)
            if ($close -lt 0) {
                return @{ Text = $sb.ToString(); End = $n; Malformed = $true
                          MalformedReason = 'unbalanced $( ) substitution'; ResumeAt = ($i + 2); Degraded = $true }
            }
            $inner = $Text.Substring($i + 2, $close - $i - 2)
            $ev0 = $State.Events
            Invoke-PolicyScan -Text $inner -Base ($Base + $i + 2) -Depth ($Depth + 1) `
                -State $State -Policy $Policy -Dialect $Dialect -SuppressAsk $SuppressAsk
            if ($State.Events -ne $ev0) { $degraded = $true }
            [void]$sb.Append('$()')
            $i = $close + 1
            continue
        }

        # ---- escapes -------------------------------------------------------------------------
        if ($c -eq '\' -and $Dialect -ne 'cmd') {
            if (($i + 1) -lt $n -and (" `t;|&()<>`"'" ).IndexOf($Text[$i + 1]) -ge 0) {
                [void]$sb.Append($Text[$i + 1]); $i += 2; continue
            }
            [void]$sb.Append($c); $i++; continue      # literal path separator
        }

        if ($c -eq '`' -and $Dialect -ne 'cmd') {
            if (($i + 1) -lt $n -and ($Text[$i + 1] -eq [char]10 -or $Text[$i + 1] -eq [char]13)) {
                $i += 2; continue       # line continuation
            }
            # G3: a backtick pair is bash command substitution only when the closing backtick ends
            # a word; otherwise the backtick is PowerShell's escape character. The old code did
            # neither reliably - a lone backtick advanced two characters and ate the first letter
            # of the following token, so `` `UnrealEditor-Cmd.exe `` lexed as 'nrealEditor-Cmd.exe'.
            $close = $Text.IndexOf('`', $i + 1)
            if ($close -lt 0) {
                # R-OPEN-1: an unbalanced backtick is a malformation. Report it, then read the
                # backtick as an escape and keep going (R-OPEN-5: never truncate).
                Set-PolicyFallback -State $State -Reason 'unbalanced backtick substitution'
                $degraded = $true
                if (($i + 1) -lt $n) { [void]$sb.Append($Text[$i + 1]); $i += 2 } else { $i++ }
                continue
            }
            $after = if (($close + 1) -lt $n) { $Text[$close + 1] } else { [char]0 }
            if (($after -eq [char]0) -or ($script:TokenBreakChars.IndexOf($after) -ge 0)) {
                $inner = $Text.Substring($i + 1, $close - $i - 1)
                $ev0 = $State.Events
                Invoke-PolicyScan -Text $inner -Base ($Base + $i + 1) -Depth ($Depth + 1) `
                    -State $State -Policy $Policy -Dialect $Dialect -SuppressAsk $SuppressAsk
                if ($State.Events -ne $ev0) { $degraded = $true }
                [void]$sb.Append('$()')
                $i = $close + 1
                continue
            }
            [void]$sb.Append($Text[$i + 1]); $i += 2; continue    # PowerShell escape
        }

        [void]$sb.Append($c)
        $i++
    }

    return @{ Text = $sb.ToString(); End = $i; Malformed = $false; MalformedReason = ''; ResumeAt = $i; Degraded = $degraded }
}

# ---------------------------------------------------------------------------------------------
# Payload helpers
# ---------------------------------------------------------------------------------------------

function Get-PolicyArgSpan {
    # Raw substring covering ArgList[$From] .. ArgList[last], with its absolute start.
    # ('Args' cannot be a parameter name: $Args is an automatic variable.)
    param([string]$Text, [int]$Base, [object[]]$ArgList, [int]$From, [switch]$SingleOnly)
    if ($null -eq $ArgList -or $From -ge $ArgList.Count -or $From -lt 0) { return $null }
    $start = [int]$ArgList[$From].Start
    $end   = if ($SingleOnly) { [int]$ArgList[$From].End } else { [int]$ArgList[$ArgList.Count - 1].End }
    if ($end -le $start) { return $null }
    return @{ Raw = $Text.Substring($start, $end - $start); Base = ($Base + $start) }
}

function Expand-PolicyPayload {
    <#
      Strip ONE fully-enclosing quote layer from a raw payload span so that
      `-Command "UnrealEditor-Cmd.exe -run"` scans as a command line rather than as one word.
      Only strips when the opening quote's match is the final character, so
      `-Command "a" "b"` is left alone.
    #>
    param([hashtable]$Payload)
    if ($null -eq $Payload) { return $null }
    $s = $Payload.Raw
    $b = $Payload.Base
    while ($s.Length -ge 2) {
        $q = $s[0]
        if ($q -ne "'" -and $q -ne '"') { break }
        $close = -1
        $j = 0
        while ($true) {
            $j = $s.IndexOf($q, $j + 1)
            if ($j -lt 0) { break }
            if ($q -eq "'" -and ($j + 1) -lt $s.Length -and $s[$j + 1] -eq "'") { $j++; continue }
            $close = $j; break
        }
        if ($close -ne ($s.Length - 1)) { break }
        $s = $s.Substring(1, $s.Length - 2)
        $b = $b + 1
    }
    return @{ Raw = $s; Base = $b }
}

function Get-PolicyFlagName {
    param([string]$Arg)
    if ([string]::IsNullOrEmpty($Arg)) { return $null }
    if ($Arg[0] -ne '-') { return $null }
    return $Arg.ToLowerInvariant()
}

# ---------------------------------------------------------------------------------------------
# Nested interpreter payload resolution
# ---------------------------------------------------------------------------------------------

function Resolve-PolicyInterpreter {
    <#
      Given a resolved command record whose name is a known interpreter, work out which argument
      (if any) is a COMMAND payload and re-scan it. Arguments that are not payloads (script paths
      passed to -File, -ArgumentList values, search patterns) are deliberately NOT scanned,
      because scanning them would deny `pwsh -File build.ps1 -Name UnrealEditor` for mentioning
      a name (R-NEST-2, R-DATA-7).
    #>
    param(
        [hashtable]$State,
        [hashtable]$Policy,
        [hashtable]$Cmd,
        [string]$Text,
        [int]$Base,
        [int]$Depth,
        [bool]$SuppressAsk
    )

    $family = $Cmd.Family
    $name   = $Cmd.Name
    $pos    = $Cmd.Position
    $argv   = @($Cmd.Args)
    $texts  = @(foreach ($a in $argv) { [string]$a.Text })

    $payloads  = @()          # each @{ Raw; Base; Dialect }
    $askReason = $null

    switch ($family) {

        'powershell' {
            for ($k = 0; $k -lt $texts.Count; $k++) {
                $a = $texts[$k]
                if ($a -match '^[-/](c|co|com|comm|comma|comman|command)$') {
                    $span = Get-PolicyArgSpan -Text $Text -Base $Base -ArgList $argv -From ($k + 1)
                    $p = Expand-PolicyPayload $span
                    if ($null -ne $p -and $p.Raw.Trim() -ne '-') {
                        $payloads += @{ Raw = $p.Raw; Base = $p.Base; Dialect = 'auto' }
                    }
                    break
                }
                if ($a -match '^[-/](f|fi|fil|file)$') {
                    # R-NEST-2 allows a script-path payload because the body is out of view - but
                    # when the path IS a protected entry point, the target is proven at a real
                    # command position (R-CORE-1) and nothing is hidden by indirection.
                    if (($k + 1) -lt $texts.Count) {
                        $fileBase = Get-PolicyBasename -Token $texts[$k + 1]
                        if (Test-PolicyProtected -Policy $Policy -Basename $fileBase) {
                            Add-PolicyFinding -State $State -Decision 'deny' `
                                -Reason "Direct invocation of protected Unreal executable '$fileBase' as the script '$name -File' is told to run." `
                                -Token $texts[$k + 1] -Position ([int]$argv[$k + 1].Start + $Base) -SuppressAsk $SuppressAsk
                        }
                    }
                    break
                }
                if ($a -match '^[-/](e|ec|enc|encoded|encodedc|encodedcommand)$') {
                    if (($k + 1) -lt $texts.Count) {
                        $decoded = $null
                        try {
                            $bytes   = [Convert]::FromBase64String($texts[$k + 1])
                            $decoded = [Text.Encoding]::Unicode.GetString($bytes)
                        } catch { $decoded = $null }
                        if ($null -ne $decoded -and $decoded.Trim().Length -gt 0) {
                            $payloads += @{ Raw = $decoded; Base = $pos; Dialect = 'auto' }
                        } else {
                            $askReason = "'$name -EncodedCommand' payload could not be decoded; the executable it would launch cannot be proven."
                        }
                    } else {
                        $askReason = "'$name -EncodedCommand' with no payload; target cannot be proven."
                    }
                    break
                }
            }
            break
        }

        'cmd' {
            # cmd's flags need not be space-separated from the payload: `cmd /c"x"`, `cmd /s/c "x"`.
            for ($k = 0; $k -lt $texts.Count; $k++) {
                $raw = [string]$argv[$k].Raw
                $flagRx = '^[-/](?<pre>(?:[sSqQdDaAuU]|[vVeE]:(?:on|off|ON|OFF)|[tT]:\w+|[-/])*)[cCkK](?<rest>.*)$'
                $m = [regex]::Match($raw, $flagRx)
                if (-not $m.Success) {
                    # The flag may have been quoted (`cmd "/c" "x"`), in which case the lexed text
                    # carries it and the raw span does not.
                    $m = [regex]::Match($texts[$k], $flagRx)
                    if (-not $m.Success -or $m.Groups['rest'].Value.Length -gt 0) { continue }
                    $p = Expand-PolicyPayload (Get-PolicyArgSpan -Text $Text -Base $Base -ArgList $argv -From ($k + 1))
                    if ($null -ne $p) { $payloads += @{ Raw = $p.Raw; Base = $p.Base; Dialect = 'cmd' } }
                    break
                }
                $rest = $m.Groups['rest'].Value
                if ($rest.Length -gt 0) {
                    $offset = $raw.Length - $rest.Length
                    $start  = [int]$argv[$k].Start + $offset
                    $end    = [int]$argv[$argv.Count - 1].End
                    $span   = @{ Raw = $Text.Substring($start, $end - $start); Base = ($Base + $start) }
                } else {
                    $span = Get-PolicyArgSpan -Text $Text -Base $Base -ArgList $argv -From ($k + 1)
                }
                $p = Expand-PolicyPayload $span
                if ($null -ne $p) { $payloads += @{ Raw = $p.Raw; Base = $p.Base; Dialect = 'cmd' } }
                break
            }
            break
        }

        'posix_shell' {
            for ($k = 0; $k -lt $texts.Count; $k++) {
                if ($texts[$k] -cmatch '^-[a-zA-Z]*c$') {
                    $span = Get-PolicyArgSpan -Text $Text -Base $Base -ArgList $argv -From ($k + 1) -SingleOnly
                    $p = Expand-PolicyPayload $span
                    if ($null -ne $p) { $payloads += @{ Raw = $p.Raw; Base = $p.Base; Dialect = 'auto' } }
                    break
                }
            }
            break
        }

        'eval' {
            $found = $false
            for ($k = 0; $k -lt $texts.Count; $k++) {
                if ($texts[$k].StartsWith('-')) { continue }
                $span = Get-PolicyArgSpan -Text $Text -Base $Base -ArgList $argv -From $k -SingleOnly
                $p = Expand-PolicyPayload $span
                if ($null -ne $p) { $payloads += @{ Raw = $p.Raw; Base = $p.Base; Dialect = 'auto' }; $found = $true }
            }
            if (-not $found -and $texts.Count -gt 0) {
                $askReason = "'$name' payload could not be identified; target cannot be proven."
            }
            break
        }

        'process_launcher' {
            $targetIdx = -1
            for ($k = 0; $k -lt $texts.Count; $k++) {
                $flag = Get-PolicyFlagName $texts[$k]
                if ($null -ne $flag -and $Policy.LauncherTargetParams.Contains($flag)) {
                    if (($k + 1) -lt $texts.Count) { $targetIdx = $k + 1 }
                    break
                }
            }
            if ($targetIdx -lt 0) {
                # G16: the old rule skipped any argument whose predecessor began with '-', which
                # made `Start-Process -Wait 'x\UnrealEditor-Cmd.exe'` unprovable. Declared
                # switch parameters take no value, so the token after one IS the positional.
                for ($k = 0; $k -lt $texts.Count; $k++) {
                    $a = $texts[$k]
                    if ($a.StartsWith('-')) { continue }
                    if ($k -gt 0) {
                        $prev = Get-PolicyFlagName $texts[$k - 1]
                        if ($null -ne $prev -and -not $Policy.LauncherSwitches.Contains($prev)) { continue }
                    }
                    $targetIdx = $k
                    break
                }
            }
            if ($targetIdx -lt 0) {
                $askReason = "'$name' without a provable target executable; what it launches cannot be determined from the command string."
            } else {
                $span = Get-PolicyArgSpan -Text $Text -Base $Base -ArgList $argv -From $targetIdx -SingleOnly
                $p = Expand-PolicyPayload $span
                if ($null -eq $p -or $p.Raw.Trim().Length -eq 0) {
                    $askReason = "'$name' without a provable target executable; what it launches cannot be determined from the command string."
                } else {
                    $payloads += @{ Raw = $p.Raw; Base = $p.Base; Dialect = 'auto' }
                }
            }
            break
        }

        'job' {
            # -ScriptBlock { ... } bodies are already scanned by the outer lexer, because '{'
            # opens a command position. What is left is an unprovable script block or a file.
            $hasFile = $false
            $unprovable = $null
            for ($k = 0; $k -lt $texts.Count; $k++) {
                $flag = Get-PolicyFlagName $texts[$k]
                if ($null -ne $flag -and ($flag -eq '-filepath' -or $flag -eq '-file')) { $hasFile = $true; break }
            }
            if (-not $hasFile) {
                for ($k = 0; $k -lt $texts.Count; $k++) {
                    $a = $texts[$k]
                    if ($a.StartsWith('-')) { continue }
                    if ($k -gt 0) {
                        $prev = Get-PolicyFlagName $texts[$k - 1]
                        if ($null -ne $prev -and -not $Policy.LauncherSwitches.Contains($prev)) { continue }
                    }
                    if (Test-PolicyExpansion -Policy $Policy -Text $a) { $unprovable = $a }
                    break
                }
                for ($k = 0; $k -lt $texts.Count; $k++) {
                    $flag = Get-PolicyFlagName $texts[$k]
                    if ($null -ne $flag -and $flag -eq '-scriptblock' -and ($k + 1) -lt $texts.Count) {
                        if (Test-PolicyExpansion -Policy $Policy -Text $texts[$k + 1]) { $unprovable = $texts[$k + 1] }
                    }
                }
            }
            if ($null -ne $unprovable) {
                $askReason = "'$name' is given a script block that is not literal ('$unprovable'); the commands it would run cannot be proven from the command string."
            }
            break
        }

        'foreign_eval' {
            for ($k = 0; $k -lt $texts.Count; $k++) {
                if ($texts[$k] -match '^-{1,2}(c|e|eval|command|exec)$') {
                    $span = Get-PolicyArgSpan -Text $Text -Base $Base -ArgList $argv -From ($k + 1) -SingleOnly
                    $p = Expand-PolicyPayload $span
                    if ($null -ne $p) {
                        $payloads += @{ Raw = $p.Raw; Base = $p.Base; Dialect = 'auto' }
                        # The payload is source code in another language, not a shell line, so a
                        # lexical scan can miss a real invocation (os.system, child_process). A
                        # bare MENTION therefore escalates rather than passing silently (G18).
                        if ($Policy.ForeignEvalMentionAsk -and (Test-PolicyMentionsProtected -Policy $Policy -Text $p.Raw)) {
                            $askReason = "'$name' is given inline source code that mentions a protected Unreal executable; what it executes cannot be proven by lexical analysis of another language."
                        }
                    }
                    break
                }
            }
            break
        }

        'find' {
            for ($k = 0; $k -lt $texts.Count; $k++) {
                if ($texts[$k] -match '^-(exec|execdir|ok|okdir)$') {
                    $stop = $texts.Count
                    for ($m2 = $k + 1; $m2 -lt $texts.Count; $m2++) {
                        if ($texts[$m2] -eq ';' -or $texts[$m2] -eq '+') { $stop = $m2; break }
                    }
                    if (($k + 1) -lt $stop) {
                        $start = [int]$argv[$k + 1].Start
                        $end   = [int]$argv[$stop - 1].End
                        $p = Expand-PolicyPayload @{ Raw = $Text.Substring($start, $end - $start); Base = ($Base + $start) }
                        if ($null -ne $p) { $payloads += @{ Raw = $p.Raw; Base = $p.Base; Dialect = 'auto' } }
                    }
                    break
                }
            }
            break
        }

        'schtasks' {
            for ($k = 0; $k -lt $texts.Count; $k++) {
                if ($texts[$k] -match '^[-/](tr|TR)$') {
                    $span = Get-PolicyArgSpan -Text $Text -Base $Base -ArgList $argv -From ($k + 1) -SingleOnly
                    $p = Expand-PolicyPayload $span
                    if ($null -ne $p) { $payloads += @{ Raw = $p.Raw; Base = $p.Base; Dialect = 'cmd' } }
                    break
                }
            }
            break
        }

        default { break }
    }

    if ($null -ne $askReason) {
        Add-PolicyFinding -State $State -Decision 'ask' -Reason $askReason `
                          -Token $name -Position $pos -SuppressAsk $SuppressAsk
    }

    foreach ($p in $payloads) {
        if ([string]::IsNullOrWhiteSpace($p.Raw)) { continue }
        $before = $State.Decision
        Invoke-PolicyScan -Text $p.Raw -Base $p.Base -Depth ($Depth + 1) -State $State `
            -Policy $Policy -Dialect $p.Dialect -SuppressAsk $SuppressAsk
        if ($State.Decision -ne $before -and $State.Decision -ne 'allow') {
            $State.Reason = "$($State.Reason) (inside '$name' command payload)"
        }
    }
}

function Test-PolicyMentionsProtected {
    # Sub-lexical mention test. Used ONLY to escalate to ask inside a foreign-language eval
    # payload, never to deny. Appearance matching is not admissible evidence of invocation.
    param([hashtable]$Policy, [string]$Text)
    if ([string]::IsNullOrWhiteSpace($Text)) { return $false }
    foreach ($m in [regex]::Matches($Text, '[A-Za-z0-9_.\-]+')) {
        if (Test-PolicyProtected -Policy $Policy -Basename (Get-PolicyBasename -Token $m.Value)) { return $true }
    }
    return $false
}

# ---------------------------------------------------------------------------------------------
# Standard input: data, or commands? (R-STDIN-1..3)
# ---------------------------------------------------------------------------------------------

function Get-PolicyStdinMode {
    <#
      Returns 'commands', 'data' or 'unknown' for a heredoc / here-string body whose owning
      command is $Owner (may be $null). The distinction is the OWNER, not the syntax.
    #>
    param([hashtable]$Policy, [hashtable]$Owner)

    if ($null -eq $Owner) { return 'data' }                 # nothing owns it, nothing executes it
    $name = [string]$Owner.Name
    if ([string]::IsNullOrWhiteSpace($name)) { return 'data' }

    if (Test-PolicyExpansion -Policy $Policy -Text ([string]$Owner.Word)) { return 'unknown' }
    if ($Policy.StdinData.Contains($name)) { return 'data' }

    $texts = @(foreach ($a in @($Owner.Args)) { [string]$a.Text })
    $hasDash       = ($texts -contains '-')
    $hasPositional = $false
    $hasInline     = $false
    foreach ($t in $texts) {
        if ($t -eq '-') { continue }
        if ($t.StartsWith('-') -or $t.StartsWith('/')) {
            if ($t -cmatch '^-[a-zA-Z]*c$' -or $t -match '^-{1,2}(c|e|eval|command|exec)$') { $hasInline = $true }
            continue
        }
        $hasPositional = $true
    }

    if (Test-PolicySetMember -Sets $Policy.StdinExec -Key 'always' -Value $name) { return 'commands' }

    if (Test-PolicySetMember -Sets $Policy.StdinExec -Key 'posix_shell' -Value $name) {
        if ($hasInline) { return 'data' }        # `bash -c 'x' <<EOF` - the -c payload runs
        if ($hasPositional) { return 'data' }    # `bash script.sh <<EOF` - the script runs
        return 'commands'
    }
    if (Test-PolicySetMember -Sets $Policy.StdinExec -Key 'powershell' -Value $name) {
        if ($hasDash) { return 'commands' }
        if ($hasInline -or $hasPositional) { return 'data' }
        return 'commands'
    }
    if (Test-PolicySetMember -Sets $Policy.StdinExec -Key 'foreign' -Value $name) {
        if ($hasDash) { return 'commands' }
        if ($hasInline -or $hasPositional) { return 'data' }
        return 'commands'
    }

    return 'unknown'
}

function Invoke-PolicyStdinBody {
    <#
      Scan a heredoc / here-string body according to the owning command's stdin mode.
      'data'     - nothing to do (R-DATA-4 / R-DATA-5).
      'commands' - scan at full strength (R-STDIN-1, R-STDIN-2).
      'unknown'  - scan into a throwaway state; a finding there escalates to ask (R-STDIN-3).
    #>
    param(
        [hashtable]$State, [hashtable]$Policy, [hashtable]$Owner,
        [string]$Body, [int]$Base, [int]$Depth, [bool]$SuppressAsk,
        [switch]$UnknownIsData
    )
    if ([string]::IsNullOrWhiteSpace($Body)) { return }
    $mode = Get-PolicyStdinMode -Policy $Policy -Owner $Owner
    if ($mode -eq 'data') { return }
    if ($mode -eq 'unknown' -and $UnknownIsData) { return }

    if ($mode -eq 'commands') {
        $before = $State.Decision
        Invoke-PolicyScan -Text $Body -Base $Base -Depth ($Depth + 1) -State $State `
            -Policy $Policy -Dialect 'auto' -SuppressAsk $SuppressAsk
        if ($State.Decision -ne $before -and $State.Decision -ne 'allow') {
            $State.Reason = "$($State.Reason) (inside a here-document body fed to '$($Owner.Name)', which executes standard input as commands)"
        }
        return
    }

    # unknown owner: probe, then escalate at most to ask.
    $probe = New-PolicyState
    Invoke-PolicyScan -Text $Body -Base $Base -Depth ($Depth + 1) -State $probe `
        -Policy $Policy -Dialect 'auto' -SuppressAsk $false
    if ($probe.Decision -ne 'allow') {
        $ownerName = if ($null -eq $Owner) { 'an unknown command' } else { "'$($Owner.Word)'" }
        Add-PolicyFinding -State $State -Decision 'ask' `
            -Reason "A here-document body redirected into $ownerName contains '$($probe.MatchedToken)' at an apparent command position, and whether that command executes standard input cannot be determined from the command string." `
            -Token $probe.MatchedToken -Position ([int]$probe.Position) -SuppressAsk $SuppressAsk
    }
}

# ---------------------------------------------------------------------------------------------
# Scanner
# ---------------------------------------------------------------------------------------------

function Invoke-PolicyFlush {
    param([hashtable]$Ctx, [hashtable]$State, [hashtable]$Policy, [string]$Text,
          [int]$Base, [int]$Depth, [bool]$SuppressAsk)
    if ($null -eq $Ctx.Cmd) { return }
    $cmd = $Ctx.Cmd
    $Ctx.Cmd = $null
    # R-PROT-3: 'git lfs pull' is only protected when git holds the command position and 'lfs' is
    # its first argument, which is knowable only once the argument list has ended.
    Test-PolicySubcommand -State $State -Policy $Policy -Cmd $cmd -SuppressAsk $SuppressAsk
    if ($null -ne $cmd.Family) {
        Resolve-PolicyInterpreter -State $State -Policy $Policy -Cmd $cmd -Text $Text `
            -Base $Base -Depth $Depth -SuppressAsk $SuppressAsk
    }
}

function Invoke-PolicyScan {
    <#
      Walks $Text, tracking whether the cursor is at a COMMAND POSITION, and classifies the
      first token of every command it finds. Quote bodies, comments, here-string bodies and
      heredoc bodies owned by a text tool are data and never yield command positions.

      NO PATH IN THIS FUNCTION RETURNS ON A MALFORMATION (R-OPEN-5). Each records a fallback and
      resumes at a declared resume point; only "extent runs to end of input" malformations move
      the cursor to $n, and they say so.
    #>
    param(
        [string]$Text,
        [int]$Base,
        [int]$Depth,
        [hashtable]$State,
        [hashtable]$Policy,
        [string]$Dialect = 'auto',
        [bool]$SuppressAsk = $false
    )

    if ($Depth -gt $Policy.MaxDepth) {
        # R-OPEN-6: this skips the SUBTREE. The caller keeps scanning its own remainder.
        Set-PolicyFallback -State $State -Kind 'ceiling' `
            -Reason "nesting deeper than $($Policy.MaxDepth) levels; that substitution subtree was not analysed (the rest of the command still was)"
        return
    }
    if ([string]::IsNullOrEmpty($Text)) { return }

    $i = 0
    $n = $Text.Length
    $atCmdPos        = $true
    $passThrough     = $null      # active pass-through spec, or $null
    $ptPositional    = 0          # positionals still to skip for the active pass-through
    $pendingHeredocs = [System.Collections.Generic.List[hashtable]]::new()
    $ctx             = @{ Cmd = $null }
    $groupStack      = [System.Collections.Generic.List[hashtable]]::new()
    $cmdStack        = [System.Collections.Generic.List[hashtable]]::new()
    $suppressDepth   = 0
    $lastWord        = ''

    while ($i -lt $n) {
        $c = $Text[$i]
        $suppress = $SuppressAsk -or ($suppressDepth -gt 0)

        # ---- newline: heredoc bodies, then a fresh command position ------------------------
        if ($c -eq [char]13) { $i++; continue }
        if ($c -eq [char]10) {
            $i++
            if ($pendingHeredocs.Count -gt 0) {
                $aborted = $false
                foreach ($hd in $pendingHeredocs) {
                    $bodyStart = $i
                    $found = $false
                    $bodyEnd = $i
                    while ($i -lt $n) {
                        $lineEnd = $Text.IndexOf([char]10, $i)
                        if ($lineEnd -lt 0) { $line = $Text.Substring($i);                $next = $n }
                        else                { $line = $Text.Substring($i, $lineEnd - $i); $next = $lineEnd + 1 }
                        $cmp = $line.TrimEnd([char]13)
                        # R-DATA-4a: leading whitespace is stripped ONLY for <<- , and POSIX <<-
                        # strips TABS only. The old code trimmed unconditionally, so an indented
                        # terminator inside ordinary prose ended the body early and the rest of
                        # the prose was scanned as commands - a false-denial engine.
                        if ($hd.Strip) { $cmp = $cmp.TrimStart([char]9) }
                        $bodyEnd = $i
                        $i = $next
                        if ($cmp -eq $hd.Term) { $found = $true; break }
                    }
                    if ($found) {
                        if ($bodyEnd -gt $bodyStart) {
                            Invoke-PolicyStdinBody -State $State -Policy $Policy -Owner $hd.Owner `
                                -Body $Text.Substring($bodyStart, $bodyEnd - $bodyStart) `
                                -Base ($Base + $bodyStart) -Depth $Depth -SuppressAsk $suppress
                        }
                    } else {
                        # Extent: end of input. A line-oriented body with no terminator cannot be
                        # bounded, so nothing after it is analysed - and nothing after it is
                        # claimed either (the diagnostic says so).
                        Set-PolicyFallback -State $State -Reason "heredoc terminator '$($hd.Term)' never appears; the body has no end, so it is treated as data and the remainder of the input was not analysed"
                        $aborted = $true
                        break
                    }
                }
                $pendingHeredocs.Clear()
                if ($aborted) {
                    Invoke-PolicyFlush -Ctx $ctx -State $State -Policy $Policy -Text $Text `
                        -Base $Base -Depth $Depth -SuppressAsk $suppress
                    $i = $n
                    continue
                }
            }
            Invoke-PolicyFlush -Ctx $ctx -State $State -Policy $Policy -Text $Text `
                -Base $Base -Depth $Depth -SuppressAsk $suppress
            $atCmdPos = $true
            $passThrough = $null
            continue
        }

        # ---- whitespace -------------------------------------------------------------------
        if ($c -eq ' ' -or $c -eq [char]9) { $i++; continue }

        # ---- PowerShell block comment <# ... #> (R-DATA-5a) --------------------------------
        # Must precede the redirection and heredoc branches, which used to split this into
        # '<' + '#' and read a documentation block as a redirection plus a line comment.
        if ($c -eq '<' -and ($i + 1) -lt $n -and $Text[$i + 1] -eq '#' -and $Dialect -ne 'cmd') {
            $close = $Text.IndexOf('#>', $i + 2)
            if ($close -lt 0) {
                Set-PolicyFallback -State $State -Reason 'unterminated PowerShell block comment (<# ... #>); the remainder of the input was not analysed'
                $i = $n
            } else {
                $i = $close + 2
            }
            continue
        }

        # ---- comment (bash '#' / PowerShell '#'; cmd.exe has no comment character) ---------
        if ($c -eq '#' -and $Dialect -ne 'cmd') {
            $lineEnd = $Text.IndexOf([char]10, $i)
            if ($lineEnd -lt 0) { $i = $n } else { $i = $lineEnd }
            continue
        }

        # ---- separators ---------------------------------------------------------------------
        if ($c -eq ';') {
            Invoke-PolicyFlush -Ctx $ctx -State $State -Policy $Policy -Text $Text -Base $Base -Depth $Depth -SuppressAsk $suppress
            $i++; $atCmdPos = $true; $passThrough = $null; continue
        }

        if ($c -eq '&') {
            if (($i + 1) -lt $n -and [char]::IsDigit($Text[$i + 1])) { $i += 2; continue }   # 2>&1
            if (($i + 1) -lt $n -and $Text[$i + 1] -eq '&') { $i += 2 } else { $i++ }
            Invoke-PolicyFlush -Ctx $ctx -State $State -Policy $Policy -Text $Text -Base $Base -Depth $Depth -SuppressAsk $suppress
            $atCmdPos = $true; $passThrough = $null
            continue
        }

        if ($c -eq '|') {
            if (($i + 1) -lt $n -and $Text[$i + 1] -eq '|') { $i += 2 }
            elseif (($i + 1) -lt $n -and $Text[$i + 1] -eq '&') { $i += 2 }                 # |&
            else { $i++ }
            Invoke-PolicyFlush -Ctx $ctx -State $State -Policy $Policy -Text $Text -Base $Base -Depth $Depth -SuppressAsk $suppress
            $atCmdPos = $true; $passThrough = $null
            continue
        }

        # '(' / '{' open a command position only when they start a word. Mid-word they are
        # syntax belonging to the previous token (xargs -I{}, PS method calls, glob braces).
        if ($c -eq '(' -or $c -eq '{') {
            $prev = if ($i -gt 0) { $Text[$i - 1] } else { [char]0 }
            $opensWord = ($i -eq 0) -or ($script:TokenBreakChars.IndexOf($prev) -ge 0)
            # A parenthesis right after a control keyword is a CONDITION, not a command context:
            # `if ($true) { ... }`, `foreach ($f in $list)`. Scanning it as a command context is
            # correct for bash `if (cmd)`, so the position is kept - but the expansion->ask rule
            # is suppressed inside, or every PowerShell `if ($x)` would raise a false escalation.
            $isCond = ($c -eq '(') -and $Policy.ControlKeywords.Contains($lastWord)
            $i++
            if ($opensWord) {
                # A parenthesised or braced group is nested INSIDE the command being built, so the
                # enclosing command is saved and restored rather than flushed: `iex (...)` and
                # `Invoke-Expression (@"..."@)` both need to know that iex owns what is inside.
                if ($c -eq '(' -and $null -ne $ctx.Cmd -and $null -ne $ctx.Cmd.Family -and
                    @('eval', 'process_launcher', 'job') -contains $ctx.Cmd.Family) {
                    Add-PolicyFinding -State $State -Decision 'ask' `
                        -Reason "'$($ctx.Cmd.Word)' is given a payload produced by a parenthesised expression evaluated at run time; the command it would execute cannot be proven from the command string." `
                        -Token $ctx.Cmd.Word -Position ([int]$ctx.Cmd.Position) -SuppressAsk $suppress
                }
                $cmdStack.Add(@{ Cmd = $ctx.Cmd })
                $ctx.Cmd = $null
                $atCmdPos = $true
                $passThrough = $null
                # A script block is an expression context: `Where-Object { $_ -match 'x' }` must
                # not escalate on $_ . Deny still fires inside it; only the ask is suppressed.
                $groupStack.Add(@{ Char = $c; Cond = $isCond; Suppress = ($isCond -or ($c -eq '{')) })
                if ($isCond -or ($c -eq '{')) { $suppressDepth++ }
            }
            $lastWord = ''
            continue
        }
        if ($c -eq ')' -or $c -eq '}') {
            Invoke-PolicyFlush -Ctx $ctx -State $State -Policy $Policy -Text $Text -Base $Base -Depth $Depth -SuppressAsk $suppress
            $i++
            $wasCond = $false
            $want = if ($c -eq ')') { '(' } else { '{' }
            if ($groupStack.Count -gt 0) {
                $top = $groupStack[$groupStack.Count - 1]
                if ([string]$top.Char -eq $want) {
                    $groupStack.RemoveAt($groupStack.Count - 1)
                    if ($top.Cond) { $wasCond = $true }
                    if ($top.Suppress -and $suppressDepth -gt 0) { $suppressDepth-- }
                    if ($cmdStack.Count -gt 0) {
                        $ctx.Cmd = $cmdStack[$cmdStack.Count - 1].Cmd
                        $cmdStack.RemoveAt($cmdStack.Count - 1)
                    }
                }
            }
            # G20: after a control keyword's condition, and after a closing brace, a statement
            # follows - that is a command position. After an ordinary ')' it is not.
            $atCmdPos = $wasCond -or ($c -eq '}')
            $passThrough = $null
            $lastWord = ''
            continue
        }

        # ---- @( ) array subexpression --------------------------------------------------------
        if ($c -eq '@' -and ($i + 1) -lt $n -and $Text[$i + 1] -eq '(') {
            $i += 2; $atCmdPos = $true
            $groupStack.Add(@{ Char = '('; Cond = $false; Suppress = $false })
            $cmdStack.Add(@{ Cmd = $ctx.Cmd }); $ctx.Cmd = $null
            continue
        }

        # ---- PowerShell here-strings @' '@ and @" "@ : data (R-DATA-5) -------------------------
        if ($c -eq '@' -and ($i + 1) -lt $n -and ($Text[$i + 1] -eq "'" -or $Text[$i + 1] -eq '"') -and
            $Dialect -ne 'cmd' -and (Test-PolicyHereStringOpener -Text $Text -QuoteIndex ($i + 1))) {
            $q   = $Text[$i + 1]
            $end = Find-PolicyHereStringEnd -Text $Text -From ($i + 2) -Quote $q
            if ($end -lt 0) {
                Set-PolicyFallback -State $State -Reason "unterminated PowerShell here-string (@$q ... $q@); the body has no end, so the remainder of the input was not analysed"
                Invoke-PolicyFlush -Ctx $ctx -State $State -Policy $Policy -Text $Text -Base $Base -Depth $Depth -SuppressAsk $suppress
                $i = $n
                continue
            }
            # A here-string piped into a stdin-executing command really does run (R-STDIN-1).
            $bodyStart = $i + 2
            $peek = $end + 2
            while ($peek -lt $n -and ($Text[$peek] -eq ' ' -or $Text[$peek] -eq [char]9 -or
                                      $Text[$peek] -eq [char]13 -or $Text[$peek] -eq [char]10)) { $peek++ }
            $hsOwner = $null
            if ($peek -lt $n -and $Text[$peek] -eq '|') {
                $rest = $Text.Substring($peek + 1)
                $m = [regex]::Match($rest, '^\s*([A-Za-z0-9_.\\/:\-]+)')
                if ($m.Success) {
                    $hsOwner = @{
                        Name = (Get-PolicyStrippedName -Policy $Policy -Basename (Get-PolicyBasename -Token $m.Groups[1].Value))
                        Word = $m.Groups[1].Value
                        Args = @()
                    }
                }
            }
            if ($null -eq $hsOwner) {
                # An argument here-string belongs to the command it is an argument of, which may be
                # an enclosing one: Invoke-Expression (@"..."@).
                $hsOwner = $ctx.Cmd
                if ($null -eq $hsOwner) {
                    for ($z = $cmdStack.Count - 1; $z -ge 0; $z--) {
                        if ($null -ne $cmdStack[$z].Cmd) { $hsOwner = $cmdStack[$z].Cmd; break }
                    }
                }
            }
            if ($null -ne $hsOwner) {
                # UnknownIsData: a here-string is a PowerShell expression value, not a redirection,
                # so an unrecognised owner means "an argument", not "possibly stdin". R-STDIN-3
                # governs redirection, which is what <<EOF is.
                Invoke-PolicyStdinBody -State $State -Policy $Policy -Owner $hsOwner `
                    -Body $Text.Substring($bodyStart, $end - $bodyStart) `
                    -Base ($Base + $bodyStart) -Depth $Depth -SuppressAsk $suppress -UnknownIsData
            }
            $i = $end + 2
            $atCmdPos = $false
            $lastWord = ''
            continue
        }

        # ---- heredoc operator: << , <<- , <<'X' , <<"X"  (but not <<< here-string) -------------
        if ($c -eq '<' -and ($i + 1) -lt $n -and $Text[$i + 1] -eq '<' -and
            -not (($i + 2) -lt $n -and $Text[$i + 2] -eq '<') -and $Dialect -ne 'cmd') {
            $j     = $i + 2
            $strip = $false
            if ($j -lt $n -and $Text[$j] -eq '-') { $strip = $true; $j++ }
            while ($j -lt $n -and ($Text[$j] -eq ' ' -or $Text[$j] -eq [char]9)) { $j++ }
            $term = ''
            $bad  = $null
            if ($j -lt $n -and ($Text[$j] -eq "'" -or $Text[$j] -eq '"')) {
                $q = [string]$Text[$j]
                $k = $Text.IndexOf($q, $j + 1)
                if ($k -lt 0) { $bad = 'unterminated quoted heredoc delimiter' }
                else {
                    $term = $Text.Substring($j + 1, $k - $j - 1)
                    $j = $k + 1
                }
            } else {
                $s = $j
                while ($j -lt $n -and ([string]$Text[$j]) -match '[A-Za-z0-9_\-\.]') { $j++ }
                $term = $Text.Substring($s, $j - $s)
            }
            if ($null -eq $bad -and [string]::IsNullOrWhiteSpace($term)) {
                $bad = 'heredoc operator with no delimiter word'
            }
            if ($null -ne $bad) {
                # Extent: end of input. Without a delimiter the body's extent is unknowable, so
                # every following line could be data. Nothing after it is analysed or claimed.
                Set-PolicyFallback -State $State -Reason "$bad; the body's extent is unknowable, so the remainder of the input was not analysed"
                Invoke-PolicyFlush -Ctx $ctx -State $State -Policy $Policy -Text $Text -Base $Base -Depth $Depth -SuppressAsk $suppress
                $i = $n
                continue
            }
            $pendingHeredocs.Add(@{ Term = $term.Trim(); Strip = $strip; Owner = $ctx.Cmd })
            $i = $j
            $atCmdPos = $false
            continue
        }

        # ---- redirections --------------------------------------------------------------------
        if ($c -eq '<' -or $c -eq '>') {
            $i++
            if ($i -lt $n -and ($Text[$i] -eq '>' -or $Text[$i] -eq '<')) { $i++ }
            $atCmdPos = $false
            continue
        }

        # ---- backtick line continuation ------------------------------------------------------
        # Substitution vs PowerShell escape is decided inside Read-PolicyToken, so that a backtick
        # pair yields a $() placeholder at the command position (R-ASK-2) exactly as $( ) does,
        # instead of being consumed by the scanner and losing that fact.
        if ($c -eq '`' -and $Dialect -ne 'cmd' -and ($i + 1) -lt $n -and
            ($Text[$i + 1] -eq [char]10 -or $Text[$i + 1] -eq [char]13)) {
            $i += 2; continue
        }

        # ---- a word ------------------------------------------------------------------------------
        $tokStart = $i
        $tok = Read-PolicyToken -Text $Text -Start $i -Base $Base -Depth $Depth -State $State `
                                -Policy $Policy -Dialect $Dialect -SuppressAsk $suppress
        $word = [string]$tok.Text

        if ($tok.Malformed) {
            Set-PolicyFallback -State $State -Reason "$($tok.MalformedReason); that region was skipped and scanning continued after it"
            # A readable prefix at a command position is still evidence.
            if ($atCmdPos -and $word.Length -gt 0) {
                if (Test-PolicyProtected -Policy $Policy -Basename (Get-PolicyBasename -Token $word)) {
                    Add-PolicyFinding -State $State -Decision 'deny' `
                        -Reason "Direct invocation of protected Unreal executable '$(Get-PolicyBasename -Token $word)' at a command position." `
                        -Token $word -Position ($Base + $tokStart) -SuppressAsk $suppress
                }
                $atCmdPos = $false
            }
            $resume = [int]$tok.ResumeAt
            $i = if ($resume -gt $i) { $resume } else { $i + 1 }
            continue
        }

        if ($tok.End -le $i) { $i++; continue }      # defensive: never spin
        $i = $tok.End

        if ([string]::IsNullOrEmpty($word)) { $atCmdPos = $false; continue }
        $lastWord = $word.ToLowerInvariant()

        if (-not $atCmdPos) {
            if ($null -ne $ctx.Cmd) {
                [void]$ctx.Cmd.Args.Add(@{
                    Text  = $word
                    Raw   = $Text.Substring($tokStart, $i - $tokStart)
                    Start = $tokStart
                    End   = $i
                })
            }
            continue
        }

        # ===== this token is at a real command position =========================================

        # A pass-through prefix (`sudo`, `env`, `if`, `xargs`, ...) keeps the command position,
        # and so must its own flags and their declared values. G7: a flag right after the prefix
        # used to end the command position, so `env -i UnrealEditor-Cmd.exe` was allowed.
        if ($null -ne $passThrough) {
            if ($word.StartsWith('-') -or $word.StartsWith('/')) {
                if ($passThrough.ValueFlags.Contains($word.ToLowerInvariant())) { $ptPositional = 1 }
                continue
            }
            if ($ptPositional -gt 0) { $ptPositional--; continue }
        }

        # bash environment prefix: FOO=bar cmd ...  -> the command follows
        if ($word -match '^[A-Za-z_][A-Za-z0-9_]*=') { continue }

        # PowerShell assignment: $x = ... / $env:X = ...  -> not an invocation
        if ($word -match '^\$[^=]*=' -and $word -notmatch '^\$[^=]*==') { $atCmdPos = $false; continue }
        if ($word.StartsWith('$')) {
            $peek = $i
            while ($peek -lt $n -and ($Text[$peek] -eq ' ' -or $Text[$peek] -eq [char]9)) { $peek++ }
            if ($peek -lt $n -and $Text[$peek] -eq '=' -and
                -not (($peek + 1) -lt $n -and $Text[$peek + 1] -eq '=')) {
                $i = $peek + 1
                $atCmdPos = $false
                continue
            }
        }

        # PowerShell call operators '&' and '.' - the NEXT token is the command (R-POS-5). '&'
        # is consumed as a separator above; '.' arrives here as a word.
        if ($Policy.CallOperators.Contains($word) -and $word.Length -le 1) {
            $passThrough = @{ ValueFlags = (New-PolicySet @()); SkipPositionals = 0 }
            $ptPositional = 0
            continue
        }

        $basename = Get-PolicyBasename -Token $word
        $absPos   = $Base + $tokStart

        if (Test-PolicyProtected -Policy $Policy -Basename $basename) {
            Add-PolicyFinding -State $State -Decision 'deny' `
                -Reason "Direct invocation of protected Unreal executable '$basename' at a command position." `
                -Token $word -Position $absPos -SuppressAsk $suppress
            $atCmdPos = $false
            $passThrough = $null
            continue
        }

        if ((-not $tok.Degraded) -and (Test-PolicyExpansion -Policy $Policy -Text $basename)) {
            Add-PolicyFinding -State $State -Decision 'ask' `
                -Reason "Command position resolves through a variable/environment expansion ('$word'); the executable it names cannot be proven from the command string." `
                -Token $word -Position $absPos -SuppressAsk $suppress
            $atCmdPos = $false
            $passThrough = $null
            continue
        }

        # G9: an expansion confined to the DIRECTORY portion leaves the basename provable, so it
        # is not a deny - but it is not nothing either: the token may resolve to a wrapper that
        # is not the file this path appears to name.
        if ((-not $tok.Degraded) -and $word.Length -gt $basename.Length -and
            (Test-PolicyExpansion -Policy $Policy -Text $word)) {
            Add-PolicyFinding -State $State -Decision 'ask' `
                -Reason "Command position resolves through an expansion in its directory portion ('$word'); the file it will execute cannot be proven from the command string." `
                -Token $word -Position $absPos -SuppressAsk $suppress
            $atCmdPos = $false
            $passThrough = $null
            continue
        }

        $stripped = Get-PolicyStrippedName -Policy $Policy -Basename $basename

        if ($Policy.PassThrough.ContainsKey($stripped)) {
            $passThrough  = $Policy.PassThrough[$stripped]
            $ptPositional = [int]$passThrough.SkipPositionals
            continue        # stays at a command position
        }

        $family = Get-PolicyInterpreterFamily -Policy $Policy -Name $stripped
        Invoke-PolicyFlush -Ctx $ctx -State $State -Policy $Policy -Text $Text -Base $Base -Depth $Depth -SuppressAsk $suppress
        $ctx.Cmd = @{
            Name     = $stripped
            Word     = $word
            Family   = $family
            Args     = [System.Collections.Generic.List[hashtable]]::new()
            Position = $absPos
        }
        $atCmdPos = $false
        $passThrough = $null
        continue
    }

    # A subcommand rule (git lfs pull) is evaluated once the command's arguments are known.
    if ($pendingHeredocs.Count -gt 0) {
        Set-PolicyFallback -State $State -Reason "heredoc terminator '$($pendingHeredocs[0].Term)' never appears; the body never begins, so the operator's own line was analysed and nothing else was"
    }
    Invoke-PolicyFlush -Ctx $ctx -State $State -Policy $Policy -Text $Text `
        -Base $Base -Depth $Depth -SuppressAsk ($SuppressAsk -or ($suppressDepth -gt 0))
}

# ---------------------------------------------------------------------------------------------
# Subcommand rule (R-PROT-3): git lfs pull|checkout|smudge
# ---------------------------------------------------------------------------------------------

function Test-PolicySubcommand {
    param([hashtable]$State, [hashtable]$Policy, [hashtable]$Cmd, [bool]$SuppressAsk)
    if ($null -eq $Cmd) { return }
    if (-not $Policy.SubCommands.ContainsKey($Cmd.Name)) { return }
    $table = $Policy.SubCommands[$Cmd.Name]
    $argv = @($Cmd.Args)
    # R-PROT-3 says the subcommand is the command's FIRST argument. Taken literally that misses
    # `git -C submodule lfs pull`, which really is a git lfs pull, so the command's own global
    # options are skipped first - value-taking ones together with their value, declared in
    # classifier.protected_subcommands[].global_value_flags so the lexer stays data-driven.
    $globals = $Policy.SubCommandGlobals
    $k = 0
    while ($k -lt $argv.Count) {
        $t = [string]$argv[$k].Text
        if (-not $t.StartsWith('-')) { break }
        if ($globals.ContainsKey($Cmd.Name) -and $globals[$Cmd.Name].Contains($t)) { $k += 2 } else { $k += 1 }
    }
    if ($k -ge $argv.Count) { return }
    $sub = ([string]$argv[$k].Text).ToLowerInvariant()
    if (-not $table.ContainsKey($sub)) { return }
    if (($k + 1) -ge $argv.Count) { return }
    $verb = ([string]$argv[$k + 1].Text).ToLowerInvariant()
    if (-not $table[$sub].Contains($verb)) { return }
    Add-PolicyFinding -State $State -Decision 'deny' `
        -Reason "Protected subcommand '$($Cmd.Name) $sub $verb' at a command position (binary-churning Git LFS operation)." `
        -Token "$($Cmd.Name) $sub $verb" -Position ([int]$Cmd.Position) -SuppressAsk $SuppressAsk
}

# ---------------------------------------------------------------------------------------------
# Public entry point
# ---------------------------------------------------------------------------------------------

function Get-CommandClassification {
    <#
    .SYNOPSIS
        Classify one command string as allow / ask / deny with respect to protected Unreal
        executables.

    .PARAMETER Command
        The submitted command string.

    .PARAMETER PolicyPath
        Path to the policy declaration. Defaults to agent_collab/context/command_policy.json
        beside this module. This is the seam the corpus uses to test policy-load failure.

    .OUTPUTS
        PSCustomObject with:
          Decision       - 'allow' | 'ask' | 'deny'
          Reason         - human-readable explanation (always populated)
          MatchedToken   - the offending token, or $null
          Position       - 0-based index of that token in the input, or $null
          ParserFallback - $true when malformed/unsupported syntax forced a fail-open result
          PolicyLoaded   - $true when the policy declaration was read and compiled
          PolicySource   - the policy path that was consulted
          PolicyError    - '' when loaded, else why it could not be
          Fallbacks      - every distinct malformation diagnostic recorded (may be empty)
    #>
    [CmdletBinding()]
    [OutputType([pscustomobject])]
    param(
        [Parameter(Mandatory = $true, Position = 0)]
        [AllowEmptyString()]
        [AllowNull()]
        [string]$Command,

        [Parameter(Position = 1)]
        [AllowEmptyString()]
        [AllowNull()]
        [string]$PolicyPath
    )

    $policy = Import-CommandPolicy -PolicyPath $PolicyPath

    if (-not $policy.Loaded) {
        # Fail open, loudly, and detectably: PolicyLoaded is $false on the returned object so a
        # test can assert the declaration actually loaded rather than assuming it did.
        return [pscustomobject]@{
            Decision       = 'allow'
            Reason         = "Fail-open: $($policy.Error). The Unreal invocation rule was not applied."
            MatchedToken   = $null
            Position       = $null
            ParserFallback = $true
            PolicyLoaded   = $false
            PolicySource   = $policy.Source
            PolicyError    = $policy.Error
            Fallbacks      = @($policy.Error)
        }
    }

    if ([string]::IsNullOrWhiteSpace($Command)) {
        return [pscustomobject]@{
            Decision       = 'allow'
            Reason         = 'Empty command; nothing to classify.'
            MatchedToken   = $null
            Position       = $null
            ParserFallback = $false
            PolicyLoaded   = $true
            PolicySource   = $policy.Source
            PolicyError    = ''
            Fallbacks      = @()
        }
    }

    $state = New-PolicyState
    $text  = $Command

    try {
        if ($text.Length -gt $policy.MaxInputLength) {
            Set-PolicyFallback -State $state -Kind 'ceiling' -Reason "input longer than $($policy.MaxInputLength) characters; only the first $($policy.MaxInputLength) were analysed"
            $text = $text.Substring(0, $policy.MaxInputLength)
        }
        Invoke-PolicyScan -Text $text -Base 0 -Depth 0 -State $state -Policy $policy `
            -Dialect 'auto' -SuppressAsk $false
    }
    catch {
        Set-PolicyFallback -State $state -Reason "internal parser error: $($_.Exception.Message)"
    }

    $decision = $state.Decision
    $reason   = $state.Reason
    $token    = $state.MatchedToken
    $position = $state.Position

    if ($state.Fallback) {
        if ($decision -eq 'deny') {
            # R-PREC-1. A proven literal invocation is not ambiguity, wherever the malformation is.
            $reason = "$reason (parser also reported: $($state.FallbackReason))"
        }
        elseif ($decision -eq 'ask' -and ((-not $state.MalformedSeen) -or $state.AskBeforeMalform)) {
            # R-PREC-2: the escalation was earned in a readable region before the malformation, or
            # the only fallback was a declared resource ceiling, which does not make the rest of
            # the string unreadable (R-OPEN-6).
            $reason = "$reason (parser also reported: $($state.FallbackReason))"
        }
        else {
            $decision = 'allow'
            $reason   = "Fail-open: $($state.FallbackReason). Ambiguity is not converted into a denial."
            $token    = $null
            $position = $null
        }
    }

    return [pscustomobject]@{
        Decision       = $decision
        Reason         = $reason
        MatchedToken   = $token
        Position       = $position
        ParserFallback = [bool]$state.Fallback
        PolicyLoaded   = $true
        PolicySource   = $policy.Source
        PolicyError    = ''
        Fallbacks      = @($state.Fallbacks)
    }
}

Export-ModuleMember -Function Get-CommandClassification
