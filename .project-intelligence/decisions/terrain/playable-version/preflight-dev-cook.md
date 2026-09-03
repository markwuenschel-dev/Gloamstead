# Preflight record — first Win64 Development cook

Written 2026-08-20. Governs the request for the single authorization named at the bottom. Nothing
here authorizes execution on its own — `agent_collab/context/human_approval_gates.md`, "Always
Human", names "Running cook, package, BuildCookRun, or release packaging" explicitly.

## Fields

| Field | Value |
|---|---|
| **Command owner** | The acting agent (this session), executing only after the explicit human authorization requested below. Per `human_approval_gates.md`'s "Orchestrator Duty," the human approves the gate; the human is not expected to type the build command themselves. |
| **Commit** | `fa87a66b50cef2798043c60ef458d1ebf0a483ec` — current `HEAD` of `feat/authored-sanctuary-environment`, tip of the branch as checked out right now. |
| **Target** | `GloamsteadEditor`/`Gloamstead` — `Win64`, **Development** configuration. Diagnostic run only; not the Shipping RC. |
| **Output root** | `Saved/Packages/20260820-fa87a66-Win64Dev/` — new, dated, sha-stamped, immutable per-run directory. Already covered by the blanket `Saved/` gitignore rule (`.gitignore:8`); nothing new to ignore. A second attempt gets a new dated root — this one is never reused or overwritten. |
| **Free-disk measurement** | 243,222,454,272 bytes (≈226.5 GiB / ≈243.2 GB) free on `D:`, measured 2026-08-20 immediately before writing this record. Content alone is ~24.65 GB (Megascans) + the rest of the 2.9 GB-ish tracked/untracked project content — comfortable margin, but this number must be re-measured immediately before the cook actually runs, not trusted from this record. |
| **Expected output retention** | Kept under its dated output root until package-smoke/acceptance evidence (T6, Q10 criteria) is captured and logged against it. May be deleted after that evidence is recorded. Logs retained at least as long as the output itself, under the same root. |
| **Acceptance evidence** | T6's acceptance checklist, scoped to Q10's criteria: package launches, reaches the authored level, completes the lantern restoration once, shows the terminal completion state, exits cleanly, no crash/`ensure`/fatal, no debug/editor artifact visible. This run is Development config, so full Q10 (clean machine, no Unreal installed) applies at the later Shipping-RC stage — this cook's own bar is narrower: does it cook and launch at all, and does the loop complete once, locally. |

## Explicitly not covered by this record

Provenance (Q6) is tracked separately in T4 and is **not** a precondition for this cook — a
Development build never leaves this machine. It remains a hard precondition for Q7's ZIP handoff.
See the map's "Open" section for both live gates.

## Attempt log

**2026-08-21, authorized ("cook") and run.** Failed in 3.13s, before compilation — `RunUAT.bat
BuildCookRun`, exit 6. UBT's own toolchain check: *"Visual Studio is installed, but is out of date or
missing a valid C++ toolchain (minimum version 14.38.33130, preferred version 14.50.35717)... Visual
Studio x64 must be installed in order to build this target."* Full output:
`Saved/Packages/20260821-ed4885d-Win64Dev/Logs/cook.log`.

Verified independently (not just trusting the error text): only Visual Studio Build Tools 2019
(16.11.37530.7) is present on this machine — no VS2022 install found under either Program Files
root. UE 5.8 requires VS2022 17.8+/MSVC 14.50. This is a real, verified environment gap.

**Not resolved**: whether `gate.ps1`/`Build.bat` currently succeeds on this same machine. The VS
version check lives in UBT, the same path both invocations hit, which is hard to reconcile with
`ROADMAP.md`'s prior green-gate claims — but that wasn't re-verified live this session, so it's
flagged as open, not asserted either way.

**Blocked on**: a human decision to install/update the VS2022 C++ toolchain (system-level change,
not something to run unattended), or an alternative explanation for the discrepancy above. No retry
without one of those.

**2026-08-21 05:53, second attempt — also failed, and absent from this log until 2026-08-21 23:xx.**
Discovered on 2026-08-21 while preflighting the third attempt, by listing `Saved/Packages/` rather
than trusting this record. Evidence: `Saved/Packages/20260821-86d1b7e-Win64Dev-retry1/Logs/cook.log`
(41 lines, retained). Same `BuildCookRun` shape as the 05:25 attempt but at commit `86d1b7e`
(committed 05:25:34, so this ran 28 minutes after it) and archiving to a `-retry1` root. Failed in
3.29s, `ExitCode=6`, `Result: Failed (OtherCompilationError)`.

Its cause was **not** the toolchain: that log shows the VS toolchain already resolved
(`D:\VisualStudio\VC\Tools\MSVC\14.51.36231`, Version 14.51.36256, warn-only "newer than latest
preferred version"). It died on the NeoStackAI plugin duplication instead — the same two
`Referenced directory ... does not exist` warnings for `ThirdParty\Lua\include` and
`ThirdParty\sol2\include`, then `Plugin 'NeoStackAI' (referenced via Gloamstead5_8.uproject) does not
contain the 'NeoStackAI' module, but lists it in '...NeoStack7811ed331f04V5\NeoStackAI.uplugin'`.

This is the same fault later found blocking `gate.ps1`, and it is now closed by the descriptor/
directory rename recorded below. The correction matters for two reasons: the attempt history is two
failures rather than one, and the second failure is the one whose cause the rename actually fixes.
Its `-retry1` root holds only a `Logs/` directory (4 KB, no partial package) and is retained as
diagnostic evidence, never reused.

**2026-08-21 (later, same day), `gate.ps1` run — GATE PASS.** Not a cook; `gate.ps1` is explicitly
outside the Always-Human cook/package gate. Run against `86d1b7e` (branch tip, tree clean apart from
an untracked `scripts/worldforge_caller/__pycache__/`). Result, read from the gate's own stdout and
from UBT's own log rather than a console summary:

- Shell-guard step: 4 suites green x2 passes, determinism check green, durable `agent_collab/state/`
  unchanged.
- Build: `Result: Succeeded`, 118.62s total, 24/24 actions; `UnrealEditor-Gloamstead.dll`,
  `UnrealEditor-WorldForgeCore.dll`, `UnrealEditor-WorldForgeEd.dll` and
  `UnrealEditor-GloamsteadEditor.dll` all linked.
- Tests + evidence: 83 test(s) green; GloamsteadForge contracts / runtime / integrity / negatives /
  fuzz all ok, nonce `22e66cd3-6626-46c0-9b6f-6d0c7734716c`.

**This resolves the "Not resolved" item above**: `gate.ps1` does now succeed on this machine, and
`ROADMAP.md`'s prior green-gate claims are no longer in tension with the failed cook. Two distinct
environment faults explain the discrepancy, both now closed:

1. *VS2022 toolchain gap* (the cook's failure at 05:25) — closed independently of this project.
   Visual Studio Community 2026 (v18.9.12112.369, MSVC 14.51.36231) is installed at
   `D:\VisualStudio`. UBT now only warns that 14.51.36256 is newer than its preferred 14.50.35717;
   it no longer errors.
2. *NeoStackAI plugin duplication* (blocked `gate.ps1` itself, failing in 1.87s before any
   compilation) — the project-local `Plugins/NeoStackAI/` copy (VersionName 3.1.25) had
   re-installed itself alongside the engine-level Fab install at
   `D:\UE_5.8\Engine\Plugins\Marketplace\NeoStack7811ed331f04V5\` (VersionName 2.0.45), and UBT threw
   `BuildException ... does not contain the 'NeoStackAI' module`. The project-local copy ships
   `Source/NeoStackAI/NeoStackAI.Build.cs`, which requires `Source/ThirdParty/Lua/include`,
   `.../Lua/src` and `.../sol2/include` (lines 24-30) — none of which that copy contains. Applied the
   remedy already recorded in `.gitignore:99-102` (committed in `4d6e703`): renamed both the
   directory and the descriptor aside, to `Plugins/NeoStackAI.disabled/` and
   `NeoStackAI.uplugin.disabled`. Renaming the directory alone is *not* sufficient — UE discovers
   plugins by scanning for `.uplugin` files, not by directory name. Both renames are untracked and
   gitignored; reverse them to restore. UBT confirms the effect at `UBT-Log.txt:38` ("Removed source
   file ... NeoStackAI.Build.cs"). Side effect: in-editor NeoStack tooling falls back to engine
   v2.0.45 until reversed.

**Retained evidence** (under the blanket `Saved/` ignore, `.gitignore:8`; not committed):
`Saved/GateRuns/20260821-86d1b7e/UBT-Log.txt`, `UBT-Log.json` and `gate-stdout.log`. UBT overwrites
its live `Log.txt` on the next invocation, so these copies are the durable record of this run.

**Observed, not blocking**: Unreal Build Accelerator repeatedly killed and retried compile processes
under memory pressure ("Low on memory (86.5gb/88.7gb). Kill threshold is 84.2gb"). Those figures are
*commit* charge, not physical RAM: this machine has 61.64 GB physical (14.29 GB free) against an
82.58 GB commit limit with a 20.93 GB pagefile, spread across 818 processes — there is no single hog
(largest resident process is `vmmemWSL` at 3.51 GB). Every killed action was retried and succeeded
and the build completed green. A cook is a heavier, longer job than this gate, so the same pressure
could slow or destabilise it. Mitigation if it bites: close WSL and browser sessions, or raise the
pagefile.

**2026-08-21 23:18, third attempt — authorized, run once, FAILED at the cook stage.** Authorized
explicitly by the human for exactly one run, no auto-retry, no config changes, WSL left running.
Commit `86d1b7e27f40a6d06f8233d382439e10746b9910`, output root
`Saved/Packages/20260821-86d1b7e-Win64Dev/` (new; verified absent before launch, never reused).
Free disk re-measured immediately before launch, as required: **232,258,584,576 bytes** (216.31 GiB)
at `2026-08-21T23:18:27.194Z`, recorded in `Logs/disk-before-launch.txt`. Commit headroom at launch
was 10.5 GB of an 82.58 GB limit. Launched 23:18:28Z, exited 23:27:21Z.

**Result: `ExitCode=25 (Error_UnknownCookFailure)`**, `Cook failed.`, underlying
`Took 383.88s to run UnrealEditor-Cmd.exe, ExitCode=1`. AutomationTool ran 8m53s; the cook
commandlet itself ran 5m53s with `PeakPhysMemoryMB=6323`, `PeakVirtMemoryMB=7125`.

**How far it got — much further than any prior attempt.** Both earlier cooks died at ~3.2s during
target configuration. This one:
- Compiled the `Gloamstead` **game** target successfully (`Result: Succeeded`, BUILD COMMAND
  COMPLETED) — a target no prior cook and no `gate.ps1` run has ever built, since the gate builds
  `GloamsteadEditor` only.
- Cooked **all** content: high-water `Cooked packages 1249 Packages Remain 0 Total 1249`.
- Then failed the cook on shader compilation: `Failure - 15 error(s), 1 warning(s)`.

**No package was produced.** The output root contains only `Logs/` (229 KB) — no partial staged or
`.pak` output exists, so there is nothing partial to preserve or to be tempted into reusing. Stage,
pak and archive were never reached.

**The actual failure**, from the cook log rather than the exit code: five distinct
`ShaderCompileWorker failed` jobs, each followed by `Crash inside the platform compiler:` with an
**empty body** — no compiler diagnostic text. The five jobs cluster on just two materials:
- `MA_Impostor_SimpleOffset_MS_a3a0174e04fddedf` — `FMicropolyRasterizeCS` permutations 70, 79 and
  99 (`/Engine/Private/Nanite/NaniteRasterizer.usf`), plus
  `TBasePassVSFNoLightMapPolicy` permutation 0 (`/Engine/Private/BasePassVertexShader.usf`).
- `MA_Foliage_968c064216dc82da` — `FNaniteVertexFactory`/`FLumenCardCS` permutation 0
  (`/Engine/Private/Lumen/LumenCardComputeShader.usf`).
All at `Low` quality, `SM6`/`PCD3D_SM6`. Surrounding `LogMaterial` lines show the cook compiling
`/Game/BlackAlder/Materials/SimpleWind/*` and `/Game/EuropeanHornbeam/Materials/SimpleWind/*` at the
same moment — the vendored, gitignored foliage content.

**Two live hypotheses; neither is asserted as the cause yet.**
1. *Memory exhaustion.* `cook.log:647` records `LogCook: Garbage collection triggers ignored: Out of
   memory condition has been detected...`, and 14 `LogAsyncCompilation: BEWARE: AssetCompile memory
   estimate is greater than available, but we're running it anyway!` lines report requirements far
   over budget (e.g. `RequiredMemory = 5439.228 MiB, MemoryLimit = 423.070 MiB`). An SCW killed by
   the OS would produce exactly the observed empty crash body. Consistent with the pre-launch
   commit-pressure warning and with UBA's kill/retry behavior during `gate.ps1`.
2. *Material- or permutation-specific compiler crash.* The failures are not scattered: across 1249
   cooked packages they land on exactly two materials, and `MA_Impostor_SimpleOffset_MS_*` fails
   across three different shader types. Random OOM kills would be expected to scatter more widely.
   This pattern would reproduce regardless of available memory.

Both are plausible and they are not mutually exclusive — memory pressure could be tipping over one
unusually heavy pair of Megascans/foliage material compiles. Distinguishing them requires a
deliberate experiment (e.g. a memory-relieved re-run, or compiling those two materials in isolation),
which is **not** authorized under this record and needs a fresh decision.

**Retained evidence**, all under `Saved/Packages/20260821-86d1b7e-Win64Dev/Logs/` (blanket `Saved/`
ignore, `.gitignore:8`; not committed): `cook.log` (the cook commandlet's own log, 229 KB),
`cook-driver.out` (full UAT stdout, 230 KB), `AutomationTool-Log.txt` (UAT's own log with the
exception trace, 255 KB, copied out of `%APPDATA%` where it is overwritten by the next UAT run), and
`disk-before-launch.txt`.

**Post-run state**: no cook processes remain; free disk on `D:` 228,260,864,000 bytes (212.58 GiB) at
`2026-08-21T23:29:14Z` — about 3.7 GiB consumed by cooked intermediates, none of it in the output
root. **No retry was performed**, per the authorization. This authorization is now consumed; any
further cook requires a fresh explicit human go-ahead.

## The concrete authorization request

> **STATUS 2026-08-21: CONSUMED.** The request below was granted and executed once at
> 23:18:28Z; it failed at the cook stage (see the third attempt in the log above). It
> authorizes nothing further. A new cook — including any re-run of this same commit —
> requires a fresh explicit human authorization under the Always-Human gate.

Superseding the `fa87a66` request above, which was consumed by the failed 2026-08-21 05:25 attempt
and cannot be reused. This is a fresh, single-run request:

> Do you authorize **one** Win64 Development cook of commit
> `86d1b7e27f40a6d06f8233d382439e10746b9910` (branch `feat/authored-sanctuary-environment`), output
> to `Saved/Packages/20260821-86d1b7e-Win64Dev/`, with **232,258,945,024 bytes** (~216.31 GiB) free
> on `D:` measured 2026-08-21T23:09:10Z, logs retained at that same output root, **no
> distribution**, and package-smoke evidence captured under the T6 acceptance checklist scoped to
> Q10's criteria?

Standing conditions, unchanged from the record above: it covers exactly one run; a retry is a fresh
ask; the output root is new, dated and sha-stamped, never reused or overwritten; and Q6/T4
provenance is *not* a precondition for this Development cook, while remaining a hard precondition
for the Q7 ZIP handoff, tracked separately.

## Successor

The failure above is being investigated under a controlled-resource experiment recorded in
`experiment-controlled-cook.md` (written 2026-08-21). That record contains the next authorization
request; this one is consumed and authorizes nothing further.
