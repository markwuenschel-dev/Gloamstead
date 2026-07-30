# Command Policy Specification — Gloamstead shell guard

**Status:** normative. **Version:** 1.0. **Established:** 2026-07-29.

This document is the single source of truth for what the Gloamstead shell guard must decide,
and why. It is written to be *testable*: every rule carries an identifier, and every row in the
verification corpus must cite the rule identifier it exercises.

This is a specification of required behaviour, not a description of current behaviour. Where the
implementation disagrees with this document, the implementation is wrong.

> **Separation of duties.** The corpus that verifies these rules is authored independently of the
> implementation that satisfies them. An implementer may not weaken a corpus row to make a
> classifier pass, and may not sign off on its own work. See `AUTHORITY` below.

---

## 1. Authority boundary — what this guard is not

The guard is a **direct-invocation guard**. It reads one string — the command submitted to a shell
tool — and nothing else.

It **cannot**:

- read the body of a script that the command runs;
- see child processes that script spawns, or their descendants;
- observe file writes, network access, or environment mutation;
- know anything about the working directory, the repository state, or the active handoff.

Therefore `pwsh -File gate.ps1` is **allowed** even when `gate.ps1` itself launches
`UnrealEditor-Cmd.exe`. One level of script indirection defeats the guard **by construction**.
This is an accepted limit, not a defect awaiting repair.

**Containment is provided elsewhere** and must never be attributed to this guard: explicit handoff
`file_ownership` and `generated_output_ownership`, edit-scope enforcement, worktree isolation,
vendor immutability, Critic review, candidate integration verification, and human approval gates.

Any document, comment, or policy field that describes this guard as *enforcing* a rule, or as
*containing* a worker, is factually wrong and must be corrected. The guard catches a careless
direct call. That is its entire value, and it is a real but bounded value.

---

## 2. Exit-code contract

`Assert-BashPolicy.ps1` is the policy entry point. Its exit status is the contract.

| Decision | Exit | Meaning | Hook behaviour |
|---|---|---|---|
| `allow` | `0` | No proven or suspected protected invocation. | **Emit no decision.** Fall through to the operator's normal permission flow. |
| `deny`  | `2` | A protected target proven at a command position. | Emit `permissionDecision: "deny"` with reason. |
| `ask`   | `3` | Could reach a protected target; target not provable from the string. | Emit `permissionDecision: "ask"` with reason. |

**R-EXIT-1.** `allow` MUST NOT emit an explicit `allow` permission decision. An explicit allow
would override the operator's own deny rules and approval prompts, converting a guard into a
bypass. Silence is the required behaviour.

**R-EXIT-2.** `deny` MUST report, in its diagnostic: a human-readable reason, the matched token,
and the token's position in the command string. A denial the operator cannot audit is not
acceptable.

**R-EXIT-3.** `ask` MUST reach a human. It MUST NOT be silently downgraded to allow. Any code path
that computes `ask` and then discards it is a defect of the highest severity in this component —
this exact defect existed until 2026-07-27, when the hook branched only on exit 2.

**R-EXIT-4.** Exit codes other than 0, 2, 3 are undefined and MUST NOT be produced by policy
logic. An unhandled exception is an infrastructure failure and is governed by R-OPEN-1.

---

## 3. The governing principle: invocation versus data

A protected name appearing in a command string is **not** evidence of an invocation. The guard
classifies by *lexical position*, never by appearance.

**R-CORE-1.** A protected target is denied only when it occupies a **command position** — the
position a shell would use to resolve an executable.

**R-CORE-2.** A protected name that occupies any non-command position MUST be allowed. This
includes, non-exhaustively: prose, search patterns, arguments, option values, comments, commit
messages, heredoc bodies, here-string bodies, quoted strings, variable assignments' right-hand
sides, and file paths passed as data.

**R-CORE-3.** Classification MUST be deterministic. The same input string MUST produce the same
decision, reason, matched token, and position on every run, in any process, in any order. No
randomness, no clock dependence, no dependence on environment or filesystem state.

**R-CORE-4.** Classification MUST NOT crash. Any input — empty, whitespace, binary-ish, deeply
nested, megabyte-scale, malformed in any way — MUST produce a decision object, never an unhandled
exception.

### Rationale, with the defect that motivated it

The rule this replaced was a regex over the raw string. It classified by how a command *looked*.
It had a proven false positive: a bash heredoc whose **prose body** contained a quoted engine path
was **denied**, though nothing was being invoked. Appearance-based matching produces exactly this
failure, and a guard that blocks legitimate work gets disabled by the people it obstructs. False
positives on payload data are therefore treated as severe, not cosmetic.

---

## 4. Command-position rules

**R-POS-1.** The first token of the string is a command position.

**R-POS-2.** The token following each of these separators is a command position:
`;` &nbsp; `&&` &nbsp; `||` &nbsp; `|` &nbsp; `&` &nbsp; newline &nbsp; `|&`

**R-POS-3.** The token following a bash/POSIX assignment prefix is a command position:
in `FOO=bar cmd`, `cmd` is a command position and `bar` is not.

**R-POS-4.** The first token inside a command substitution — `$( … )` or backticks — is a command
position, as is any token following a separator within it. Substitutions nest; nesting MUST be
tracked to arbitrary depth (subject to R-OPEN-3).

**R-POS-5.** The token following a PowerShell call operator — `&` or `.` used as an invocation
operator — is a command position.

**R-POS-6.** A token is *not* a command position merely because it names an executable. In
`grep UnrealEditor *.log`, `grep` is the command position and `UnrealEditor` is a search term:
**allow** (R-CORE-2).

---

## 5. Data regions

Tokens inside these regions are data and MUST NOT be treated as command positions.

**R-DATA-1.** Single-quoted strings — no interpretation of contents.

**R-DATA-2.** Double-quoted strings — contents are data for command-position purposes, even though
a shell would expand variables inside them. An expansion inside double quotes at a command
position is governed by R-ASK-3.

**R-DATA-3.** Comments — from an unquoted `#` to end of line, in both bash and PowerShell forms.

**R-DATA-4.** Bash heredoc bodies, from the operator to the terminator line, for all forms:
`<<EOF`, `<<-EOF` (tab-stripping), `<<'EOF'` and `<<"EOF"` (quoted delimiters). The body is data,
**except as required by R-STDIN-1.** The heredoc *operator's* command remains subject to normal
analysis.

**R-DATA-4a.** Terminator matching MUST respect the form: an indented terminator closes the body
only for the `<<-` variant. Applying a trim unconditionally ends the body early on input a shell
would treat as data, which produces a false denial on ordinary prose — the precise defect class
this design exists to eliminate.

**R-DATA-5.** PowerShell here-string bodies: `@' … '@` and `@" … "@`. Here-string syntax is
PowerShell-specific; an `@'…'` sequence in a POSIX command line is an ordinary word and MUST NOT
cause the parser to hunt for a `'@` terminator, nor to abandon analysis of the rest of the string.

**R-DATA-5a.** PowerShell block comments `<# … #>` are comments in full, not a redirection
followed by a line comment. Their contents MUST NOT be classified as commands.

**R-DATA-6.** Escaped characters do not open or close data regions: bash `\"` and `\'`,
PowerShell backtick-quote. An escaped quote MUST NOT be treated as a region delimiter.

**R-DATA-7.** Payload arguments to interpreters that do not execute them as commands — for
example a script path passed to a text tool, or a string passed to `echo`, `printf`,
`Write-Output`, `git commit -m`, `grep`, `rg`, `sed`, `awk`, `Select-String` — are data.

---

## 6. Nested interpreters and process launchers

A nested interpreter is a command that takes another command as an argument and executes it. The
guard MUST analyse the nested command when it can be read as a literal, and escalate when it
cannot.

Interpreters in scope: `pwsh`, `powershell`, `pwsh.exe`, `powershell.exe`, `cmd`, `cmd.exe`,
`bash`, `sh`, `dash`, `zsh`, plus `Start-Process`, `Invoke-Expression` / `iex`, and
`Invoke-Command`.

**R-NEST-1 (literal → recurse).** When the nested payload is a literal string, the guard MUST
analyse it as a command string and adopt the strongest resulting decision. `bash -c "UnrealEditor-Cmd ..."`
is a **deny**: the protected target is proven at a command position one level down.
Flags in scope: `-c`, `-Command`, `-c` for `cmd` as `/c` and `/k`, and `Start-Process`'s
`-FilePath` / first positional argument.

**R-NEST-2 (script file → allow).** When the payload is a **script path** rather than an inline
command — `pwsh -File gate.ps1`, `bash ./build.sh` — the guard MUST **allow**. The script body is
outside its field of view (§1). This is the accepted blind spot and MUST be documented as such,
never silently treated as safe.

**R-NEST-3 (data → allow).** An interpreter name appearing as data, or a nested payload that
contains a protected name only in a non-command position, is **allow** per R-CORE-2.

**R-ASK-1 (unprovable launcher).** When a process launcher's target cannot be resolved to a
literal — `Start-Process $exe`, `& $cmd`, `iex $payload` — the decision is **ask**.

**R-ASK-2 (expansion at command position).** When a command position is occupied by a variable
expansion or substitution whose value is not determinable from the string — `$TOOL --render`,
`${CMD}`, `%TOOL%`, `$(Get-Thing) -x` — the decision is **ask**.

**R-ASK-3 (expansion inside a nested payload's command position).** When a nested interpreter's
payload is a literal but its own command position is an expansion —
`pwsh -Command "$tool -run"` — the decision is **ask**.

**R-ASK-4.** `ask` is never a punishment and never a denial. It is the correct answer to
"this could reach a protected target and I cannot prove what it is." Escalating is strictly
preferable to guessing in either direction.

### Interpreters that execute standard input

**R-STDIN-1.** When a heredoc or here-string body is redirected into a command that **executes
standard input as commands**, the body is NOT data — it MUST be scanned as a command string.
`bash <<EOF` / `UnrealEditor-Cmd.exe -run=Cook` / `EOF` really does invoke the editor, and
classifying that body as data is a false allow on a *proven* invocation.

The distinction is the owning command, not the heredoc syntax:

- `cat <<EOF`, `grep -f - <<EOF`, `python script.py <<EOF` → body is **data** (R-DATA-4).
- `bash <<EOF`, `sh <<EOF`, `bash -s <<EOF`, `zsh <<EOF`, `pwsh -Command -`,
  `powershell -Command -`, `python -`, `node -`, `perl`, `iex` fed from a here-string
  → body is **commands** (scan it).

**R-STDIN-2.** A body scanned under R-STDIN-1 obeys every other rule in this document, recursively.
A protected target proven at a command position inside it is a `deny`; an unprovable expansion
inside it is an `ask`.

**R-STDIN-3.** Where it cannot be determined whether the owning command executes stdin, the
decision for a body containing a protected name at an apparent command position is `ask`, never a
silent allow.

---

## 7. Protected targets

**R-PROT-1.** The protected set MUST be declared in exactly one tracked location and read from
there. It MUST NOT be duplicated across the module, the assert script, and the documentation —
three copies of a security-relevant list will drift, and the drift will favour the attacker.

**R-PROT-2.** The set covers Unreal generation, packaging, and automation entry points, and
binary-churning Git LFS subcommands. Matching MUST be case-insensitive and MUST tolerate an
optional executable suffix (`.exe`, `.bat`, `.cmd`, `.sh`), a directory prefix (absolute or
relative, quoted or unquoted, with or without spaces in the path), and forward or backslash
separators.

**R-PROT-2a — matching MUST apply Win32 path canonicalisation before comparison.** Windows strips
trailing dots and spaces from a filename when resolving it, so `UnrealEditor-Cmd.exe.` and
`"UnrealEditor-Cmd.exe "` both launch the binary. A matcher that compares the raw token misses
both. Canonicalise (trim trailing dots and whitespace, then strip a known suffix) and only then
compare.

**R-PROT-2b.** The set MUST cover the real shipped variants, not only the canonical names —
including build-configuration suffixed editors (e.g. `UnrealEditor-Win64-DebugGame`), and the other
generation and packaging entry points that carry the same authority: `UnrealBuildTool`,
`AutomationTool`, `UnrealPak`, `Build.bat`, and `Cook.bat`. A list that names the polite spelling
of a binary and omits its four shipped aliases protects nothing.

**R-PROT-3.** LFS subcommand matching (`git lfs pull|checkout|smudge`) applies when `git` is the
proven command position and `lfs` is its first argument. `git commit -m "ran git lfs pull"` is
data: **allow**.

**R-PROT-4.** Adding a target to the protected set MUST NOT require editing the lexer.

---

## 8. Malformed input and fail-open

**R-OPEN-1.** Malformed or unsupported syntax MUST fail **open** — decision `allow` — with an
auditable diagnostic naming the specific malformation. A guard that bricks every shell command
when its parser breaks is worse than the exposure it removes.

**R-OPEN-2.** Ambiguity MUST NOT be converted into denial. Only a *proven* invocation denies.

**R-OPEN-3.** Resource limits (nesting depth, input length) MUST be explicit, MUST fail open when
exceeded, and MUST say so in the diagnostic.

**R-OPEN-4.** Every fail-open MUST be distinguishable in output from an ordinary allow, so that a
reviewer can tell "nothing to see here" from "I could not read this."

**R-OPEN-5 — a fail-open MUST NOT truncate analysis.** This is the most important rule in the
section. When the parser cannot read one region, it MUST record the fallback and **continue
scanning the remainder** in a degraded mode; it MUST NOT abandon the rest of the command string.

Rationale: if any malformation stops analysis, then *every* malformation is a universal off switch.
`echo "unclosed && UnrealEditor-Cmd.exe -run=Cook` would be allowed because the unterminated quote
ended the scan before the invocation was reached. That turns the guard's own safety behaviour into
the easiest way to defeat it — a guard whose failure mode is "silently stop looking" gives less
protection than no guard at all, because it also produces a false assurance.

Degraded mode means: a `deny` may still be proven after the malformation (R-PREC-1), an `ask` may
still be raised, and the fallback diagnostic still reports what could not be read. Only the
unreadable region is skipped, not the tail of the string.

**R-OPEN-6.** Resource ceilings (R-OPEN-3) are subject to R-OPEN-5: exceeding a nesting limit
skips that subtree, not the remainder of the command.

### Precedence

Decisions are resolved in this fixed order. Earlier wins.

1. **R-PREC-1.** A protected target **proven** at a command position in a successfully parsed
   region → `deny`. This beats a malformation elsewhere in the string. A string that both proves an
   invocation and is malformed later is still a proven invocation.
2. **R-PREC-2.** A construct identified in a successfully parsed region that could reach a
   protected target but cannot be proven → `ask`. This beats a *subsequent* malformation, for the
   reason in R-ASK-4: escalation costs a prompt, silent allow costs the guarantee.
3. **R-PREC-3.** A malformation that **precedes or encloses** the region where a suspicious
   construct sits → `allow` (fail open). Once the malformation is upstream, position analysis
   downstream is not trustworthy enough to justify even an escalation, and false escalations train
   an operator to rubber-stamp.
4. **R-PREC-4.** Otherwise → `allow`.

> **Design note on R-PREC-2 / R-PREC-3.** The boundary between them is a deliberate choice, and
> it is the one rule in this document most worth challenging. The alternative — malformation
> always fails open, even downstream of a proven-suspicious construct — is simpler to implement
> and simpler to explain, at the cost of losing escalations the lexer had already legitimately
> earned. The split is specified because the information is available and discarding it is a
> silent loss. A reviewer who disagrees should say so explicitly rather than let the
> implementation settle it by accident.

---

## 9. Verification requirements

**R-VER-1.** Every *classification* rule MUST be exercised by at least one corpus row citing its
identifier. Coverage MUST be reported per rule, and an uncovered classification rule is a
verification failure, not a note.

### Coverage ownership — which suite verifies which rule

Not every rule in this document is expressible as a corpus row, and a coverage checker that demands
one for each will either fail permanently or invite someone to bolt on fake rows to silence it.
Both outcomes are worse than stating the mapping. Rules are owned as follows, and a coverage
checker MUST honour this table rather than requiring corpus rows for all of them:

| Rules | Verified by | Why not a corpus row |
|---|---|---|
| `R-CORE-*`, `R-POS-*`, `R-DATA-*`, `R-NEST-*`, `R-ASK-*`, `R-STDIN-*`, `R-PROT-2*`, `R-PROT-3`, `R-OPEN-*`, `R-PREC-*`, `R-EXIT-2`, `R-EXIT-4` | **corpus** (`Test-CommandPolicy.ps1`) | classify one string, assert one decision |
| `R-EXIT-1`, `R-EXIT-3` | **live hook** (`Test-BashPolicyHook.ps1`) | properties of what the hook *emits*, not of a classification — the 2026-07-27 defect was invisible to module tests precisely because the module was right and the hook was wrong |
| `R-CORE-3`, `R-CORE-4` (as properties over many inputs) | **fuzz** (`Test-CommandPolicyFuzz.ps1`) | determinism and no-crash are properties over a population, not a single expectation |
| `R-PROT-1`, `R-PROT-4` | **structural test** | "the list lives in one place" and "adding a target needs no lexer edit" are facts about the code's shape; verified by adding a target to the policy file at runtime and asserting classification changes with no module edit |
| `R-VER-*` | **the harness and the gate wiring** | requirements *on* the verification system; satisfied by this suite existing, running on a mandatory path, and proving clean-checkout reproducibility |

**R-VER-1a.** A coverage checker MUST report uncovered rules **per owning suite**, and MUST fail
only when a rule its own suite owns is uncovered. It MUST NOT silently drop unowned rules from the
denominator — an unattributed rule is itself a coverage failure, because it means this table has
drifted from the spec.

**R-VER-2.** The corpus MUST be authored independently of the implementation (§AUTHORITY).

**R-VER-3.** Verification MUST include a deterministic (seeded, reproducible) fuzz/property pass
over hundreds of inputs, asserting: no crashes (R-CORE-4), stable repeat classification
(R-CORE-3), no payload false positives (R-CORE-2), and no dropped `ask` (R-EXIT-3).

**R-VER-4.** Verification MUST exercise the **live hook** — real PreToolUse JSON on stdin,
asserting the emitted decision and exit status — not only the module in isolation. The 2026-07-27
defect was invisible to module tests because the module was correct and the hook was not.

**R-VER-5.** Verification MUST run on a mandatory tracked path, automatically. A suite that passes
only when someone remembers to run it does not protect against regression.

**R-VER-6.** Verification MUST leave tracked repository state byte-identical. Proven by hashing
before and after and by running the suite twice.

**R-VER-7.** A clean clone MUST contain, or deterministically recreate from tracked sources, both
the policy logic and the hook registration that invokes it. An unreproducible guard is not a
guard.

---

## AUTHORITY

- The **implementer** satisfies this specification. It does not author the corpus that judges it,
  and does not issue its own verdict.
- The **corpus author** encodes this specification as executable expectations, adversarially, and
  does not modify the implementation.
- The **critic** re-runs every declared gate from a committed-clean checkout and returns a verdict
  with evidence. A verdict without reproduced command output is not a verdict.
- Disagreement between implementation and specification is resolved in favour of the
  specification, or by amending the specification explicitly — never silently.
