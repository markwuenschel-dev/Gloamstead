#requires -Version 7
<#
.SYNOPSIS
    Deterministic command classifier: decides whether a submitted shell/PowerShell command
    string INVOKES a protected Unreal executable, or merely MENTIONS one as data.

.DESCRIPTION
    Get-CommandClassification -Command '<string>' returns exactly one of:

      deny  - an unambiguous direct invocation of a protected executable at a real command
              position (including one literal syntactic layer down inside a nested interpreter).
      ask   - a recognisable nested-interpreter or process-launch pattern COULD invoke one, but
              the final executable cannot be proven (variable/env expansion at a command
              position, Start-Process with an unprovable target, pwsh -Command "& $var",
              cmd /c "%VAR%", -EncodedCommand that will not decode).
      allow - the protected name appears only as DATA (prose, search argument, commit message,
              heredoc body, here-string content, comment, string literal), or nothing protected
              appears at all.

    Protected basenames (case-insensitive, any path prefix, optional .exe/.bat/.cmd/.sh):
        UnrealEditor, UnrealEditor-Cmd, RunUAT, BuildCookRun, GenerateProjectFiles

    This replaces the previous appearance-matching regex, which classified by how a command
    LOOKED rather than by what it would RUN. That regex had a proven defect: a bash heredoc
    whose prose body contained a quoted engine path was DENIED, though nothing was invoked.

.NOTES
    ===== LIMITS - READ BEFORE RELYING ON THIS =====

    This examines the SUBMITTED COMMAND SYNTAX ONLY. Specifically it does NOT:
      * observe or classify processes spawned by the command (children or grandchildren);
      * read the body of any script the command runs, so it cannot make a wrapper script safe
        (`pwsh -File gate.ps1` is ALLOW even though gate.ps1 itself launches UnrealEditor-Cmd);
      * constitute worker containment, sandboxing, or a security boundary.

    It is a MISTAKE-PREVENTION GUARD against a careless direct call. One level of script
    indirection defeats it by construction. Real containment remains: handoff file_ownership,
    edit-scope guards, worktree isolation, vendor immutability, Critic audits.

    ===== FAIL-OPEN POLICY =====

    Malformed or unsupported syntax (unterminated quote / heredoc / here-string, unbalanced
    substitution, recursion depth exceeded, internal parser error) FAILS OPEN: Decision =
    'allow', ParserFallback = $true, and Reason carries a diagnostic. Ambiguity is NEVER
    converted into a denial.

    One deliberate refinement: if a deny was already PROVEN in the successfully parsed prefix
    BEFORE the malformed region was reached, that deny is preserved (ParserFallback stays $true
    and the Reason names both facts). A proven literal invocation is not ambiguity. An 'ask'
    found before a malformed region is NOT preserved - it degrades to the fail-open allow.

    ===== DECISION: LITERAL PROTECTED EXE INSIDE A NESTED INTERPRETER => deny =====

    `pwsh -Command "& 'D:\UE\...\UnrealEditor-Cmd.exe'"`, `cmd /c "RunUAT.bat ..."`,
    `bash -c 'UnrealEditor ...'` and `Start-Process -FilePath '...\UnrealEditor.exe'` are
    classified DENY, not ASK.

    Why: the payload is a literal string with no expansion in it, so the executable at the
    nested command position is PROVEN, not guessed - which is exactly the definition of deny.
    The whole point of this rewrite is to classify by what will run rather than by how the
    text looks, and one quoting layer does not change what will run. Treating it as 'ask'
    would also make the guard trivially defeatable: wrapping any denied command in
    `bash -c '...'` would downgrade every deny to a prompt, and a guard that a single quote
    pair disarms is not a guard. 'ask' stays reserved for genuinely unprovable targets.

    Counter-argument, recorded honestly: a nested payload is one step further from the user's
    intent than a bare command line, so a false positive there is more surprising. That risk is
    accepted because the payload must be wholly literal for deny to fire; any expansion
    anywhere in the resolved payload routes to ask instead.

    ===== KNOWN LIMITS OF THE LEXER =====
      * Heredoc bodies are treated as DATA unconditionally, per contract. `bash <<EOF` with an
        invocation in the body is therefore ALLOW even though it would really run.
      * Positions reported for findings inside a nested interpreter payload are approximate:
        they are anchored at the interpreter token, because the payload is reconstructed from
        parsed argument tokens.
      * Backtick handling is heuristic: bash command substitution outside quotes, escape
        character inside double quotes and inside tokens.
#>

Set-StrictMode -Version Latest

# ---------------------------------------------------------------------------------------------
# Tables
# ---------------------------------------------------------------------------------------------

$script:ProtectedBasenames = @(
    'unrealeditor'
    'unrealeditor-cmd'
    'runuat'
    'buildcookrun'
    'generateprojectfiles'
)

# Executables whose command ARGUMENT is itself a command and must be inspected.
$script:Interpreters = @(
    'pwsh', 'powershell', 'cmd', 'bash', 'sh', 'zsh', 'dash',
    'start-process', 'invoke-expression', 'iex', 'eval'
)

# Tokens that precede a command without being one (bash keywords / launcher prefixes).
$script:PassThroughPrefixes = @(
    'sudo', 'env', 'nohup', 'time', 'command', 'exec', 'do', 'then', 'else', 'elif'
)

$script:ExtensionRx = '\.(exe|bat|cmd|sh)$'

# A token that contains any of these cannot be resolved to a literal executable.
$script:ExpansionRx = '(\$\()|(\$\{)|(\$env:)|(\$[A-Za-z_][A-Za-z0-9_]*)|(%[A-Za-z_][A-Za-z0-9_]*%)'

$script:TokenBreakChars = " `t`r`n;|&()<>{}"

$script:MaxDepth = 8

$script:Rank = @{ 'allow' = 0; 'ask' = 1; 'deny' = 2 }

# ---------------------------------------------------------------------------------------------
# State helpers
# ---------------------------------------------------------------------------------------------

function New-PolicyState {
    return @{
        Decision       = 'allow'
        Reason         = 'No protected executable at any command position.'
        MatchedToken   = $null
        Position       = $null
        Fallback       = $false
        FallbackReason = ''
    }
}

function Add-PolicyFinding {
    param(
        [hashtable]$State,
        [string]$Decision,
        [string]$Reason,
        [string]$Token,
        [int]$Position
    )
    if ($script:Rank[$Decision] -gt $script:Rank[$State.Decision]) {
        $State.Decision     = $Decision
        $State.Reason       = $Reason
        $State.MatchedToken = $Token
        $State.Position     = $Position
    }
}

function Set-PolicyFallback {
    param([hashtable]$State, [string]$Reason)
    if (-not $State.Fallback) {
        $State.Fallback       = $true
        $State.FallbackReason = $Reason
    }
}

# ---------------------------------------------------------------------------------------------
# Small lexical helpers
# ---------------------------------------------------------------------------------------------

function Get-PolicyBasename {
    param([string]$Token)
    if ([string]::IsNullOrEmpty($Token)) { return '' }
    $t = $Token
    # A trailing separator means the token names a directory, not an executable.
    $slash = [Math]::Max($t.LastIndexOf('/'), $t.LastIndexOf('\'))
    if ($slash -ge 0) { $t = $t.Substring($slash + 1) }
    return $t
}

function Test-PolicyProtected {
    param([string]$Basename)
    if ([string]::IsNullOrWhiteSpace($Basename)) { return $false }
    $b = $Basename.ToLowerInvariant()
    $b = [regex]::Replace($b, $script:ExtensionRx, '')
    return ($script:ProtectedBasenames -contains $b)
}

function Test-PolicyExpansion {
    param([string]$Text)
    if ([string]::IsNullOrEmpty($Text)) { return $false }
    return ($Text -match $script:ExpansionRx)
}

function Get-PolicyInterpreterName {
    param([string]$Basename)
    if ([string]::IsNullOrWhiteSpace($Basename)) { return $null }
    $b = $Basename.ToLowerInvariant()
    $b = [regex]::Replace($b, '\.(exe|bat|cmd)$', '')
    if ($script:Interpreters -contains $b) { return $b }
    return $null
}

function Find-PolicyClosingParen {
    # $OpenIndex must point at the '('. Returns index of the matching ')' or -1.
    param([string]$Text, [int]$OpenIndex)
    $depth = 0
    $i = $OpenIndex
    $n = $Text.Length
    while ($i -lt $n) {
        $c = $Text[$i]
        if ($c -eq "'") {
            $j = $Text.IndexOf("'", $i + 1)
            if ($j -lt 0) { return -1 }
            $i = $j + 1
            continue
        }
        if ($c -eq '"') {
            $j = $Text.IndexOf('"', $i + 1)
            if ($j -lt 0) { return -1 }
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
        # Walk back over spaces/tabs; the terminator must begin a line.
        $q = $p - 1
        while ($q -ge 0 -and ($Text[$q] -eq ' ' -or $Text[$q] -eq [char]9)) { $q-- }
        if ($q -lt 0 -or $Text[$q] -eq [char]10 -or $Text[$q] -eq [char]13) { return $p }
        $search = $p + 1
    }
}

# ---------------------------------------------------------------------------------------------
# Token reader
# ---------------------------------------------------------------------------------------------

function Read-PolicyToken {
    <#
      Reads one shell word starting at $Start. Quote bodies are appended as literal content
      (they are data, never command positions), $( ) inside a token or inside double quotes is
      recursed into as a nested command context, and escapes are consumed.

      Returns @{ Text; End; Malformed; MalformedReason }.
    #>
    param(
        [string]$Text,
        [int]$Start,
        [int]$Base,
        [int]$Depth,
        [hashtable]$State
    )

    $sb = [System.Text.StringBuilder]::new()
    $i  = $Start
    $n  = $Text.Length

    while ($i -lt $n) {
        $c = $Text[$i]

        if ($script:TokenBreakChars.IndexOf($c) -ge 0) { break }

        # ---- single-quoted segment: pure data -------------------------------------------
        if ($c -eq "'") {
            $j = $Text.IndexOf("'", $i + 1)
            if ($j -lt 0) {
                return @{ Text = $sb.ToString(); End = $n; Malformed = $true
                          MalformedReason = 'unterminated single quote' }
            }
            [void]$sb.Append($Text.Substring($i + 1, $j - $i - 1))
            $i = $j + 1
            continue
        }

        # ---- double-quoted segment: data, except $( ) substitution ------------------------
        if ($c -eq '"') {
            $i++
            $closed = $false
            while ($i -lt $n) {
                $ch = $Text[$i]

                if ($ch -eq '"') { $closed = $true; $i++; break }

                # \" is an escape only when the quote clearly does not close the string.
                # Windows paths end in a backslash often enough ("D:\UE_5.8\") that treating
                # every \" as an escape would break normal commands.
                if ($ch -eq '\' -and ($i + 1) -lt $n -and $Text[$i + 1] -eq '"') {
                    $after = if (($i + 2) -lt $n) { $Text[$i + 2] } else { [char]0 }
                    $looksTerminal = ($after -eq [char]0) -or ($script:TokenBreakChars.IndexOf($after) -ge 0)
                    $moreQuotes    = ($Text.IndexOf('"', $i + 2) -ge 0)
                    # Only let the quote close when the string cannot plausibly continue:
                    # a path like "D:\UE_5.8\" ends here, but  "he said \" and more"  does not.
                    $closesHere = $looksTerminal -and (-not $moreQuotes)
                    if (-not $closesHere) {
                        [void]$sb.Append('"'); $i += 2; continue
                    }
                    # Treat the backslash as a path separator and let the quote close.
                    [void]$sb.Append('\'); $i++; continue
                }

                if ($ch -eq '`' -and ($i + 1) -lt $n) {
                    # PowerShell escape inside a double-quoted string.
                    [void]$sb.Append($Text[$i + 1]); $i += 2; continue
                }

                if ($ch -eq '$' -and ($i + 1) -lt $n -and $Text[$i + 1] -eq '(') {
                    $close = Find-PolicyClosingParen -Text $Text -OpenIndex ($i + 1)
                    if ($close -lt 0) {
                        return @{ Text = $sb.ToString(); End = $n; Malformed = $true
                                  MalformedReason = 'unbalanced $( ) inside double quotes' }
                    }
                    $inner = $Text.Substring($i + 2, $close - $i - 2)
                    $null = Invoke-PolicyScan -Text $inner -Base ($Base + $i + 2) -Depth ($Depth + 1) -State $State
                    [void]$sb.Append('$()')
                    $i = $close + 1
                    continue
                }

                [void]$sb.Append($ch)
                $i++
            }
            if (-not $closed) {
                return @{ Text = $sb.ToString(); End = $n; Malformed = $true
                          MalformedReason = 'unterminated double quote' }
            }
            continue
        }

        # ---- ${name} expansion -------------------------------------------------------------
        if ($c -eq '$' -and ($i + 1) -lt $n -and $Text[$i + 1] -eq '{') {
            $j = $Text.IndexOf('}', $i + 2)
            if ($j -lt 0) {
                return @{ Text = $sb.ToString(); End = $n; Malformed = $true
                          MalformedReason = 'unterminated ${ } expansion' }
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
                          MalformedReason = 'unbalanced $( ) substitution' }
            }
            $inner = $Text.Substring($i + 2, $close - $i - 2)
            $null = Invoke-PolicyScan -Text $inner -Base ($Base + $i + 2) -Depth ($Depth + 1) -State $State
            [void]$sb.Append('$()')
            $i = $close + 1
            continue
        }

        # ---- escapes -------------------------------------------------------------------------
        if ($c -eq '\') {
            if (($i + 1) -lt $n -and (" `t;|&()<>`"'" ).IndexOf($Text[$i + 1]) -ge 0) {
                [void]$sb.Append($Text[$i + 1]); $i += 2; continue
            }
            # Otherwise a literal path separator.
            [void]$sb.Append($c); $i++; continue
        }

        if ($c -eq '`') {
            if (($i + 1) -lt $n -and ($Text[$i + 1] -eq [char]10 -or $Text[$i + 1] -eq [char]13)) {
                $i += 2; continue       # line continuation
            }
            if (($i + 1) -lt $n) { [void]$sb.Append($Text[$i + 1]); $i += 2; continue }
            $i++; continue
        }

        [void]$sb.Append($c)
        $i++
    }

    return @{ Text = $sb.ToString(); End = $i; Malformed = $false; MalformedReason = '' }
}

# ---------------------------------------------------------------------------------------------
# Nested interpreter payload resolution
# ---------------------------------------------------------------------------------------------

function Resolve-PolicyInterpreter {
    <#
      Given an interpreter name and its parsed argument tokens, work out which argument (if any)
      is a COMMAND payload and re-scan it. Arguments that are not payloads (script paths passed
      to -File, -ArgumentList values, search patterns) are deliberately NOT scanned, because
      scanning them would deny `pwsh -File build.ps1 -Name UnrealEditor` for mentioning a name.
    #>
    param(
        [hashtable]$State,
        [string]$Name,
        [string[]]$TokenArgs,
        [int]$Position,
        [int]$Depth
    )

    $payloads = @()
    $askReason = $null

    switch ($Name) {

        { $_ -in @('pwsh', 'powershell') } {
            for ($k = 0; $k -lt $TokenArgs.Count; $k++) {
                $a = $TokenArgs[$k]
                if ($a -match '^[-/](c|com|comm|comma|comman|command)$') {
                    if (($k + 1) -lt $TokenArgs.Count) {
                        # -Command consumes the remainder.
                        $payloads += (($TokenArgs[($k + 1)..($TokenArgs.Count - 1)]) -join ' ')
                    }
                    break
                }
                if ($a -match '^[-/](e|ec|enc|encodedcommand|encoded)$') {
                    if (($k + 1) -lt $TokenArgs.Count) {
                        $decoded = $null
                        try {
                            $bytes   = [Convert]::FromBase64String($TokenArgs[$k + 1])
                            $decoded = [Text.Encoding]::Unicode.GetString($bytes)
                        } catch { $decoded = $null }
                        if ($null -ne $decoded -and $decoded.Trim().Length -gt 0) {
                            $payloads += $decoded
                        } else {
                            $askReason = "'$Name -EncodedCommand' payload could not be decoded; the executable it would launch cannot be proven."
                        }
                    } else {
                        $askReason = "'$Name -EncodedCommand' with no payload; target cannot be proven."
                    }
                    break
                }
            }
            break
        }

        'cmd' {
            for ($k = 0; $k -lt $TokenArgs.Count; $k++) {
                if ($TokenArgs[$k] -match '^[-/](c|k)$') {
                    if (($k + 1) -lt $TokenArgs.Count) {
                        $payloads += (($TokenArgs[($k + 1)..($TokenArgs.Count - 1)]) -join ' ')
                    }
                    break
                }
            }
            break
        }

        { $_ -in @('bash', 'sh', 'zsh', 'dash') } {
            # -c, and combined short flags that end in c (-lc, -ec, -xc ...)
            for ($k = 0; $k -lt $TokenArgs.Count; $k++) {
                if ($TokenArgs[$k] -cmatch '^-[a-zA-Z]*c$') {
                    if (($k + 1) -lt $TokenArgs.Count) { $payloads += $TokenArgs[$k + 1] }
                    break
                }
            }
            break
        }

        { $_ -in @('invoke-expression', 'iex', 'eval') } {
            foreach ($a in $TokenArgs) {
                if ($a -match '^-') { continue }
                $payloads += $a
            }
            if ($payloads.Count -eq 0 -and $TokenArgs.Count -gt 0) {
                $askReason = "'$Name' payload could not be identified; target cannot be proven."
            }
            break
        }

        'start-process' {
            $target = $null
            for ($k = 0; $k -lt $TokenArgs.Count; $k++) {
                if ($TokenArgs[$k] -match '^-(FilePath|Path|fp|f)$') {
                    if (($k + 1) -lt $TokenArgs.Count) { $target = $TokenArgs[$k + 1] }
                    break
                }
            }
            if ($null -eq $target) {
                for ($k = 0; $k -lt $TokenArgs.Count; $k++) {
                    $a = $TokenArgs[$k]
                    if ($a -match '^-') { continue }
                    if ($k -gt 0 -and $TokenArgs[$k - 1] -match '^-') { continue }
                    $target = $a
                    break
                }
            }
            if ($null -eq $target -or $target.Trim().Length -eq 0) {
                $askReason = 'Start-Process without a provable target executable; what it launches cannot be determined from the command string.'
            } else {
                $payloads += $target
            }
            break
        }

        default { break }
    }

    if ($null -ne $askReason) {
        Add-PolicyFinding -State $State -Decision 'ask' -Reason $askReason `
                          -Token $Name -Position $Position
    }

    foreach ($p in $payloads) {
        if ([string]::IsNullOrWhiteSpace($p)) { continue }
        $before = $State.Decision
        $null = Invoke-PolicyScan -Text $p -Base $Position -Depth ($Depth + 1) -State $State
        if ($State.Decision -ne $before -and $State.Decision -ne 'allow') {
            $State.Reason = "$($State.Reason) (inside '$Name' command payload)"
        }
    }
}

# ---------------------------------------------------------------------------------------------
# Scanner
# ---------------------------------------------------------------------------------------------

function Invoke-PolicyScan {
    <#
      Walks $Text, tracking whether the cursor is at a COMMAND POSITION, and classifies the
      first token of every command it finds. Quote bodies, heredoc bodies, here-string bodies
      and comments are data and never yield command positions.
    #>
    param(
        [string]$Text,
        [int]$Base,
        [int]$Depth,
        [hashtable]$State
    )

    if ($Depth -gt $script:MaxDepth) {
        Set-PolicyFallback -State $State -Reason "nesting deeper than $($script:MaxDepth) levels; not analysed"
        return
    }
    if ([string]::IsNullOrEmpty($Text)) { return }

    $i = 0
    $n = $Text.Length
    $atCmdPos = $true
    $pendingHeredocs = [System.Collections.Generic.List[hashtable]]::new()
    $ctx = @{ Interp = $null }

    # Local flush of any interpreter whose argument list just ended.
    function local:Flush {
        param([hashtable]$Context, [hashtable]$St, [int]$D)
        if ($null -ne $Context.Interp) {
            $it = $Context.Interp
            $Context.Interp = $null
            Resolve-PolicyInterpreter -State $St -Name $it.Name `
                -TokenArgs ([string[]]$it.Args) -Position $it.Position -Depth $D
        }
    }

    while ($i -lt $n) {
        $c = $Text[$i]

        # ---- newline: heredoc bodies, then a fresh command position ------------------------
        if ($c -eq [char]13) { $i++; continue }
        if ($c -eq [char]10) {
            $i++
            if ($pendingHeredocs.Count -gt 0) {
                foreach ($hd in $pendingHeredocs) {
                    $found = $false
                    while ($i -lt $n) {
                        $lineEnd = $Text.IndexOf([char]10, $i)
                        if ($lineEnd -lt 0) { $line = $Text.Substring($i);                 $next = $n }
                        else                { $line = $Text.Substring($i, $lineEnd - $i);  $next = $lineEnd + 1 }
                        $cmp = $line.TrimEnd([char]13)
                        if ($hd.Strip) { $cmp = $cmp.TrimStart([char]9, ' ') }
                        $i = $next
                        if ($cmp.Trim() -eq $hd.Term) { $found = $true; break }
                    }
                    if (-not $found) {
                        Set-PolicyFallback -State $State -Reason "heredoc terminator '$($hd.Term)' never appears; body treated as data and command not analysed further"
                        Flush -Context $ctx -St $State -D $Depth
                        return
                    }
                }
                $pendingHeredocs.Clear()
            }
            Flush -Context $ctx -St $State -D $Depth
            $atCmdPos = $true
            continue
        }

        # ---- whitespace -------------------------------------------------------------------
        if ($c -eq ' ' -or $c -eq [char]9) { $i++; continue }

        # ---- comment (only at the start of a word) ------------------------------------------
        if ($c -eq '#') {
            $lineEnd = $Text.IndexOf([char]10, $i)
            if ($lineEnd -lt 0) { $i = $n } else { $i = $lineEnd }
            continue
        }

        # ---- separators ---------------------------------------------------------------------
        if ($c -eq ';') { Flush -Context $ctx -St $State -D $Depth; $i++; $atCmdPos = $true; continue }

        if ($c -eq '&') {
            # 2>&1 style redirection - not a separator.
            if (($i + 1) -lt $n -and [char]::IsDigit($Text[$i + 1])) { $i += 2; continue }
            if (($i + 1) -lt $n -and $Text[$i + 1] -eq '&') { $i += 2 } else { $i++ }
            Flush -Context $ctx -St $State -D $Depth
            $atCmdPos = $true
            continue
        }

        if ($c -eq '|') {
            if (($i + 1) -lt $n -and $Text[$i + 1] -eq '|') { $i += 2 } else { $i++ }
            Flush -Context $ctx -St $State -D $Depth
            $atCmdPos = $true
            continue
        }

        # '(' / '{' open a command position only when they start a word. Mid-word they are
        # syntax belonging to the previous token (xargs -I{}, PS method calls, glob braces),
        # and treating them as openers would misread the following word as a command.
        if ($c -eq '(' -or $c -eq '{') {
            $prev = if ($i -gt 0) { $Text[$i - 1] } else { [char]0 }
            $opensWord = ($i -eq 0) -or ($script:TokenBreakChars.IndexOf($prev) -ge 0)
            $i++
            if ($opensWord) { Flush -Context $ctx -St $State -D $Depth; $atCmdPos = $true }
            continue
        }
        if ($c -eq ')' -or $c -eq '}') { Flush -Context $ctx -St $State -D $Depth; $i++; $atCmdPos = $false; continue }

        # ---- @( ) array subexpression --------------------------------------------------------
        if ($c -eq '@' -and ($i + 1) -lt $n -and $Text[$i + 1] -eq '(') {
            $i += 2; $atCmdPos = $true; continue
        }

        # ---- PowerShell here-strings @' '@ and @" "@ : data -----------------------------------
        if ($c -eq '@' -and ($i + 1) -lt $n -and ($Text[$i + 1] -eq "'" -or $Text[$i + 1] -eq '"')) {
            $q   = $Text[$i + 1]
            $end = Find-PolicyHereStringEnd -Text $Text -From ($i + 2) -Quote $q
            if ($end -lt 0) {
                Set-PolicyFallback -State $State -Reason "unterminated PowerShell here-string (@$q ... $q@)"
                Flush -Context $ctx -St $State -D $Depth
                return
            }
            $i = $end + 2
            $atCmdPos = $false
            continue
        }

        # ---- heredoc operator: << , <<- , <<'X' , <<"X"  (but not <<< here-string) -------------
        if ($c -eq '<' -and ($i + 1) -lt $n -and $Text[$i + 1] -eq '<' -and
            -not (($i + 2) -lt $n -and $Text[$i + 2] -eq '<')) {
            $j     = $i + 2
            $strip = $false
            if ($j -lt $n -and $Text[$j] -eq '-') { $strip = $true; $j++ }
            while ($j -lt $n -and ($Text[$j] -eq ' ' -or $Text[$j] -eq [char]9)) { $j++ }
            $term = ''
            if ($j -lt $n -and ($Text[$j] -eq "'" -or $Text[$j] -eq '"')) {
                $q = [string]$Text[$j]
                $k = $Text.IndexOf($q, $j + 1)
                if ($k -lt 0) {
                    Set-PolicyFallback -State $State -Reason 'unterminated quoted heredoc delimiter'
                    Flush -Context $ctx -St $State -D $Depth
                    return
                }
                $term = $Text.Substring($j + 1, $k - $j - 1)
                $j = $k + 1
            } else {
                $s = $j
                while ($j -lt $n -and ([string]$Text[$j]) -match '[A-Za-z0-9_\-\.]') { $j++ }
                $term = $Text.Substring($s, $j - $s)
            }
            if ([string]::IsNullOrWhiteSpace($term)) {
                Set-PolicyFallback -State $State -Reason 'heredoc operator with no delimiter word'
                Flush -Context $ctx -St $State -D $Depth
                return
            }
            $pendingHeredocs.Add(@{ Term = $term.Trim(); Strip = $strip })
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

        # ---- backtick command substitution -----------------------------------------------------
        if ($c -eq '`') {
            if (($i + 1) -lt $n -and ($Text[$i + 1] -eq [char]10 -or $Text[$i + 1] -eq [char]13)) {
                $i += 2; continue        # line continuation
            }
            $close = $Text.IndexOf('`', $i + 1)
            if ($close -ge 0) {
                $inner = $Text.Substring($i + 1, $close - $i - 1)
                $null = Invoke-PolicyScan -Text $inner -Base ($Base + $i + 1) -Depth ($Depth + 1) -State $State
                $i = $close + 1
                $atCmdPos = $false
                continue
            }
            $i += 2      # lone backtick: treat as an escape
            continue
        }

        # ---- a word ------------------------------------------------------------------------------
        $tokStart = $i
        $tok = Read-PolicyToken -Text $Text -Start $i -Base $Base -Depth $Depth -State $State
        if ($tok.Malformed) {
            Set-PolicyFallback -State $State -Reason "$($tok.MalformedReason); command not fully analysed"
            Flush -Context $ctx -St $State -D $Depth
            return
        }
        if ($tok.End -le $i) { $i++; continue }      # defensive: never spin
        $i = $tok.End
        $word = [string]$tok.Text

        if ([string]::IsNullOrEmpty($word)) { $atCmdPos = $false; continue }

        if (-not $atCmdPos) {
            if ($null -ne $ctx.Interp) { [void]$ctx.Interp.Args.Add($word) }
            continue
        }

        # ===== this token is at a real command position =========================================

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

        $basename = Get-PolicyBasename -Token $word
        $absPos   = $Base + $tokStart

        if (Test-PolicyProtected -Basename $basename) {
            Add-PolicyFinding -State $State -Decision 'deny' `
                -Reason "Direct invocation of protected Unreal executable '$basename' at a command position." `
                -Token $word -Position $absPos
            $atCmdPos = $false
            continue
        }

        if (Test-PolicyExpansion -Text $basename) {
            Add-PolicyFinding -State $State -Decision 'ask' `
                -Reason "Command position resolves through a variable/environment expansion ('$word'); the executable it names cannot be proven from the command string." `
                -Token $word -Position $absPos
            $atCmdPos = $false
            continue
        }

        $lowerBase = $basename.ToLowerInvariant()
        if ($script:PassThroughPrefixes -contains $lowerBase) {
            continue        # stays at a command position
        }

        $interp = Get-PolicyInterpreterName -Basename $basename
        if ($null -ne $interp) {
            Flush -Context $ctx -St $State -D $Depth
            $ctx.Interp = @{
                Name     = $interp
                Args     = [System.Collections.Generic.List[string]]::new()
                Position = $absPos
            }
            $atCmdPos = $false
            continue
        }

        $atCmdPos = $false
    }

    if ($pendingHeredocs.Count -gt 0) {
        Set-PolicyFallback -State $State -Reason "heredoc terminator '$($pendingHeredocs[0].Term)' never appears; body treated as data"
    }
    Flush -Context $ctx -St $State -D $Depth
}

# ---------------------------------------------------------------------------------------------
# Public entry point
# ---------------------------------------------------------------------------------------------

function Get-CommandClassification {
    <#
    .SYNOPSIS
        Classify one command string as allow / ask / deny with respect to protected Unreal
        executables.

    .OUTPUTS
        PSCustomObject with:
          Decision       - 'allow' | 'ask' | 'deny'
          Reason         - human-readable explanation (always populated)
          MatchedToken   - the offending token, or $null
          Position       - 0-based index of that token in the input, or $null
          ParserFallback - $true when malformed/unsupported syntax forced a fail-open result
    #>
    [CmdletBinding()]
    [OutputType([pscustomobject])]
    param(
        [Parameter(Mandatory = $true, Position = 0)]
        [AllowEmptyString()]
        [AllowNull()]
        [string]$Command
    )

    $state = New-PolicyState

    if ([string]::IsNullOrWhiteSpace($Command)) {
        return [pscustomobject]@{
            Decision       = 'allow'
            Reason         = 'Empty command; nothing to classify.'
            MatchedToken   = $null
            Position       = $null
            ParserFallback = $false
        }
    }

    try {
        $null = Invoke-PolicyScan -Text $Command -Base 0 -Depth 0 -State $state
    }
    catch {
        Set-PolicyFallback -State $state -Reason "internal parser error: $($_.Exception.Message)"
    }

    if ($state.Fallback) {
        if ($state.Decision -eq 'deny') {
            # A deny proven in the parsed prefix survives: it is not ambiguity.
            return [pscustomobject]@{
                Decision       = 'deny'
                Reason         = "$($state.Reason) (parser also reported: $($state.FallbackReason))"
                MatchedToken   = $state.MatchedToken
                Position       = $state.Position
                ParserFallback = $true
            }
        }
        return [pscustomobject]@{
            Decision       = 'allow'
            Reason         = "Fail-open: $($state.FallbackReason). Ambiguity is not converted into a denial."
            MatchedToken   = $null
            Position       = $null
            ParserFallback = $true
        }
    }

    return [pscustomobject]@{
        Decision       = $state.Decision
        Reason         = $state.Reason
        MatchedToken   = $state.MatchedToken
        Position       = $state.Position
        ParserFallback = $false
    }
}

Export-ModuleMember -Function Get-CommandClassification
