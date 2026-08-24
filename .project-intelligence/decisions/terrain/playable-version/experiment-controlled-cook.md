# Experiment record — controlled-resource Win64 Development cook

Written 2026-08-21, after the third cook attempt failed at the shader-compilation stage. This record
**requests** one bounded rerun; it authorizes nothing on its own. `agent_collab/context/human_approval_gates.md`
"Always Human" names "Running cook, package, BuildCookRun, or release packaging", and the previous
authorization is recorded as CONSUMED in `preflight-dev-cook.md`.

## What this experiment is for

The failed cook of `86d1b7e` produced two candidate explanations. The human ruled that the
material/compiler hypothesis (H2) is unproven while acute memory pressure (H1) is evidenced, and
directed that no foliage content be edited or excluded. This experiment is the discriminating test:
**run the same cook with compilation concurrency and memory deliberately bounded below available
commit headroom, changing nothing else.**

- A **pass** supports "resource pressure was necessary" for the failure. It does not prove pressure
  was the direct cause of each `ShaderCompileWorker` crash, and it does not close H2.
- **The same two materials failing while recorded headroom stays adequate** makes H2 actionable and
  moves the investigation to the material/compiler layer.

## Why H1 is the evidenced hypothesis — mechanism, verified in engine source

Not inferred from the exit code; traced end to end.

1. `Saved/Packages/20260821-86d1b7e-Win64Dev/Logs/cook.log:423` —
   `LogShaderCompilers: Display: Using 29 local workers for shader compilation`.
2. That 29 is exactly what the engine computes for a cook on this machine:
   `D:\UE_5.8\Engine\Source\Runtime\Engine\Private\ShaderCompiler\ShaderCompiler.cpp:1257` —
   `NumShaderCompilingThreads = NumVirtualCores - NumUnusedShaderCompilingThreads`; and `:1238`
   skips the percentage-based reservation entirely `if (!IsRunningCommandlet() && ...)`, so during a
   cook commandlet only `NumUnusedShaderCompilingThreads=3`
   (`D:\UE_5.8\Engine\Config\BaseEngine.ini:2181`) applies. 32 logical cores − 3 = **29**.
3. Nothing caps what those 29 workers may consume:
   `ShaderCompilerThreadRunnable.cpp:35-43` registers `r.ShaderCompiler.MemoryLimit` with default
   **0**, documented as "effectively disabling the limitation", and `ECVF_ReadOnly` (settable only
   at startup). `BaseEditor.ini:418` sets `MaxConcurrentShaderJobs=65536`.
4. Async asset compilation was likewise uncapped: `Editor.AsyncAssetCompilationMaxMemoryUsage`
   defaults to 0 (`AssetCompilingManager.cpp:67-73`), which is *provable from our own log* — the
   warning we received is the non-hard-limit branch (`AssetCompilingManager.cpp:462-469`); the
   hard-limit branch at `:450-458` prints different text.
5. The `MemoryLimit = 423.070 MiB` in that warning is not a configured budget. It is
   `GetMemoryAvailableForAssetCompilation()` (`AssetCompilingManager.cpp:159-208`):
   `SystemAvailableMemory = FMath::Min3(AvailablePhysical, AvailableVirtual, INT64_MAX)` (`:176`),
   returning `Max(0, SystemAvailableMemory - 64MiB)` when no hard limit is set (`:181-184`).
6. On Windows that `AvailableVirtual` is **system commit headroom**, not free RAM:
   `D:\UE_5.8\Engine\Source\Runtime\Core\Private\Windows\WindowsPlatformMemory.cpp:340` —
   `MemoryStats.AvailableVirtual = FMath::Min(MemoryStats.AvailableVirtual, MemoryStatusEx.ullAvailPageFile)`,
   with the comment at `:337-339` stating the system-wide commit limit is the binding constraint.

**Therefore** `MemoryLimit = 423 MiB` means commit headroom was about 487 MiB at that instant —
against 10.5 GB measured at launch. The cooker's own governor agreed: `cook.log:471`
`MemoryMinFreeVirtual 2048MiB`, breached twice — `cook.log:628` (1468 MiB) and `cook.log:649`
(1833 MiB), the latter immediately before the five `ShaderCompileWorker failed` entries at
`cook.log:673-699`. `cook.log:469` shows `MemoryMaxUsedVirtual 0MiB` — no ceiling on cooker virtual
usage.

Corroborating: `Saved/Crashes/` contains nothing newer than 2026-08-07 17:22, so the dead workers
filed no crash reports — consistent with external termination rather than a handled compiler fault,
though not proof of it. The empty `Crash inside the platform compiler:` bodies remain the reason each
individual worker death cannot be attributed with certainty; that is precisely why this test is
needed.

## Environment baseline (measured 2026-08-21T23:29-23:35Z)

| Property | Value |
|---|---|
| Physical RAM | 61.64 GB total |
| Commit limit | **89.83 GB — was 82.58 GB before the failed cook** |
| Pagefile | `C:\pagefile.sys`, **system-managed**, 28.18 GB allocated, 5.4 GB peak |
| CPU | 32 logical / 16 physical |
| WSL | 4 distros Running: NVIDIA-Workbench, Ubuntu, Ubuntu-22.04, docker-desktop; `vmmemWSL` 4.00 GB |
| Free disk `D:` | 228,260,864,000 bytes (212.58 GiB) |

**The commit limit is not a constant.** It moved 82.58 to 89.83 GB across the failed cook because the
pagefile is system-managed and Windows grew it. Left alone, a rerun could pass because Windows
expanded the pagefile rather than because bounded concurrency worked — an accidental pass that proves
nothing repeatable. See the open item below.

## Proposed configuration for the rerun

All values are set **on the command line only**. No file in the repository is modified, no engine
config is edited, and no system setting is changed. Verified mechanisms:
`-ini:<Ini>:[Section]:Key=Value` args on the RunUAT line are harvested at
`D:\UE_5.8\Engine\Source\Programs\AutomationTool\AutomationUtils\ProjectParams.cs:1372-1382` and
re-emitted onto the cook commandlet (`CookCommand.Automation.cs:110-115`); ini-override-from-
commandline is compiled in for non-Shipping (`ConfigCacheIni.h:57`).

| Control | Setting | Failed run | Proposed | Mechanism |
|---|---|---|---|---|
| Shader worker count | `[DevOptions.Shaders] NumUnusedShaderCompilingThreads` | 3 → **29 workers** | 26 → **6 workers** | `-ini:Engine:[DevOptions.Shaders]:NumUnusedShaderCompilingThreads=26` |
| Shader worker memory | `r.ShaderCompiler.MemoryLimit` (MiB) | 0 = unlimited | **3072** | `-dpcvars=r.ShaderCompiler.MemoryLimit=3072` via `-AdditionalCookerOptions` (ECVF_ReadOnly — startup only) |
| Shader job batch | `[DevOptions.Shaders] MaxShaderJobBatchSize` | 10 | 10 (unchanged) | — |
| Cook shader job ceiling | `[CookSettings] MaxConcurrentShaderJobs` | 65536 | **256** | `-ini:Editor:[CookSettings]:MaxConcurrentShaderJobs=256` |
| Asset compile concurrency | `Editor.AsyncAssetCompilationMaxConcurrency` | −1 = unlimited | **2** | `-asyncassetcompilationmaxconcurrency=2` (`AsyncCompilationHelpers.cpp:305-309`) |
| Asset compile memory | `Editor.AsyncAssetCompilationMaxMemoryUsage` (GB) | 0 = no hard limit | **3** | `-dpcvars=Editor.AsyncAssetCompilationMaxMemoryUsage=3` |
| Cook process count | `[CookSettings] CookProcessCount` | 1 | **1 (unchanged, explicit)** | stays single-process |
| Cooker GC floors | `MemoryMinFreePhysical` / `MemoryMinFreeVirtual` | 2048 / 2048 | **unchanged** | left at default deliberately |

**Budget arithmetic.** Bounded compile work totals about 6 GB (3 GB shader workers + 3 GB asset
compilation). The cooker process itself reached `VirtualMemory=5563MiB` in the failed run
(`cook.log:705`). 6 + 5.5 = about 11.5 GB against 12.83 GB free commit measured above — deliberately
below headroom, but with a thin margin, which is why peak commit must be sampled throughout (below).

**Why single-process.** `D:\UE_5.8\Engine\Source\Editor\UnrealEd\Private\Cooker\CookDirector.cpp:1435-1449`
zeroes `MemoryMinFreeVirtual` and `MemoryMinFreePhysical` when cooking multiprocess. Raising
`CookProcessCount` would discard the cooker's memory floors — the opposite of a bounded experiment.

**Why GC floors are left alone.** Changing worker bounds *and* GC behaviour in one run would make a
pass uninterpretable. Bounded concurrency is the single independent variable.

## Instrumentation required

The failed run's memory collapse was only visible in retrospect, sampled by chance. The rerun must
sample continuously, or a pass proves nothing about margin:

- Sample free commit, free physical and `vmmemWSL` working set every 10s into
  `Logs/memory-samples.csv` in the output root, for the whole run.
- Record the **minimum** free commit observed, alongside the peak. That minimum is the number that
  decides whether the run was genuinely bounded.
- Record the commit limit at start and end, to detect mid-run pagefile growth.
- Copy `%APPDATA%\Unreal Engine\AutomationTool\Logs\D+UE_5.8\Log.txt` into the output root after the
  run, as it is overwritten by the next UAT invocation.
- Retain `cook.log`, full UAT stdout, and `disk-before-launch.txt` as before.

## Fixed conditions

- **WSL: left running, all four distros, unchanged.** Matching the failed run keeps the experiment
  single-variable, and the human directed that WSL not be stopped solely for a cook run. Its state
  is recorded, not altered.
- **No concurrent workload**: no other cook, build, editor, gate run, or memory-heavy job. Verified
  immediately before launch and recorded.
- **Output root**: `Saved/Packages/20260822-86d1b7e-Win64Dev-bounded/` — new, unique, never reused.
  The run aborts if it already exists.
- **No automatic retry.** Single run, whatever the outcome.
- **No distribution** of any produced package.
- **Timeout: 120 minutes, enforced by an external watchdog in the runner script.** Verified that the
  engine offers no such control: the cook is a blocking call with no timeout parameter
  (`CookCommand.Automation.cs:347` → `CommandletUtils.cs:60` → `:273-282`), and `-RunTimeoutSeconds`
  applies only to the Run stage (`RunProjectCommand.Automation.cs:330, 389-393`). On timeout the
  watchdog terminates the process tree, records the fact, and retains all logs — a timeout is
  recorded as a failure, never as a partial pass.

## Interpretation rules, fixed in advance

| Outcome | Reading | Next step |
|---|---|---|
| Package produced, no SCW failures, **min free commit stayed comfortably above zero** | Supports "resource pressure was necessary". Does not prove sufficiency; does not close H2. | Proceed to T6 package-smoke as a separate step. |
| Cook fails, **same two materials**, min free commit stayed adequate | H2 becomes actionable — the defect is not capacity. | Move to material/compiler investigation. Still no content exclusion. |
| Cook fails, min free commit again near zero | Bounds were still too loose. Not discriminating. | Lower bounds further, or raise commit limit, then re-request. |
| Package produced but T6 smoke fails | Recorded as a **package-smoke failure**, never a release pass. | Separate investigation. |
| Watchdog timeout | Failure. | Record; re-scope before any further request. |

A package produced by excluding or editing foliage content would be rejected regardless of outcome:
it changes shipped content, can omit runtime dependencies, and would mask a machine-capacity defect.

## Open item requiring a human decision

**[OPEN] Pagefile policy.** The commit limit moved 82.58 to 89.83 GB across the failed cook because
the pagefile is system-managed. Two options:

- **(a) Leave system-managed, record it.** No system change, nothing to undo. Cost: the commit limit
  is an uncontrolled variable, so a pass is weaker evidence — Windows may simply have grown the
  pagefile again. The sampled minimum-free-commit figure partly compensates by showing the true
  margin.
- **(b) Pin the pagefile to a fixed size before the run.** Makes commit headroom a controlled
  constant and the result genuinely repeatable. Cost: a system-level change requiring elevation,
  outside anything currently authorized, and it must be recorded and reverted afterwards.

Lean: **(a)** for this run, because the point of the experiment is to test bounded concurrency and
(a) changes nothing about the machine, keeping the run single-variable. Continuous sampling makes an
accidental pass detectable: if minimum free commit never approached the wall *and* the commit limit
did not grow mid-run, the pass is real. The counter-argument is real though — if the commit limit
does grow again mid-run, the result is contaminated and we will have spent a run to learn that,
where (b) would have prevented it outright.

## AMENDMENT 2026-08-24: the request below names a stale commit

**Do not answer the request as originally written.** It targets
`86d1b7e27f40a6d06f8233d382439e10746b9910`, which is now nine commits behind and does not contain the
fixes that make the game start at all. Cooking it would produce an artifact in which Cycle 1 can restore
the first lantern but can never begin its first night - the exact defect the record below predates.

**Current revision:** `049f28d306a6e588a56c5cd6cb078bbd555952e4` on branch
`fix/cycle1-tutorial-warning-content-gap` (unpushed; agents cannot publish - see AGENTS.md
"Git / PR / merge workflow"). `gate.ps1` GATE PASS on that revision: build green, 145/145 tests green,
GloamsteadForge evidence validated.

**What changed since `86d1b7e`, all verified against a green gate:**

| Commit | Change |
|---|---|
| `45f1c8c` | Retrieval double-reclaim test bug fixed; trustworthy baseline restored |
| `6ff4f8b` | `TutorialLostPath`/`Tutorial` and the Cycle 4 row authored in the manifest and re-imported; the C++ runtime fallbacks deleted and replaced with fail-closed contract validation |
| `d044316`, `26f51ca`, `f9498ca` | `SemanticSubject` given a shipping writer (`UGloamsteadRitualSiteComponent` + `ApplyAuthoredSiteContracts`), with coverage proving the night runtime resolves a subject written by the shipping path rather than the automation seam |
| `c2a1030` | `DA_ExperienceCycleCatalog` authored as shipped content; runtime names its source; drift guard added |
| `402f9cf` | `AGENTS.md` reconciled with the enforced command policy |
| `049f28d` | Post-final-cycle soft-lock replaced with a legible ending state |

**Content changed since the last cook attempt**, so the cook's inputs are not the same:
`Content/Data/DA_VeilHeartWarningCatalog.uasset` (6005 -> 7935 bytes), a new
`Content/Data/DA_ExperienceCycleCatalog.uasset`, and four `DA_*` assets re-saved from UE 5.7 to 5.8
serialization (+4 bytes each, no data change - verified by diffing the old LFS blobs).

**Unchanged and still true:** the failure being investigated is five `ShaderCompileWorker` crashes on two
vendored foliage materials, H1 (memory pressure) versus H2 (material-specific compiler crash) still
undecided, the bounded-concurrency parameters below still the discriminating experiment, and the
`[OPEN]` pagefile question above still requiring an answer. Free space on `D:` at the time of this
amendment: 192 GB of 839 GB.

**The amended ask** is the request below with `86d1b7e27f40a6d06f8233d382439e10746b9910` replaced by
`049f28d306a6e588a56c5cd6cb078bbd555952e4` and the output root by
`Saved/Packages/20260824-049f28d-Win64Dev-bounded/`. Every other parameter stands. This amendment
authorizes nothing on its own; the Always-Human cook gate is unchanged and no cook has been run.

## The concrete authorization request

> Do you authorize **one** bounded Win64 Development cook of commit
> `86d1b7e27f40a6d06f8233d382439e10746b9910`, output to
> `Saved/Packages/20260822-86d1b7e-Win64Dev-bounded/`, with 6 shader workers,
> `r.ShaderCompiler.MemoryLimit=3072` MiB, `MaxConcurrentShaderJobs=256`, asset-compile concurrency 2
> and a 3 GB asset-compile budget, single-process, all four WSL distros left running, every limit
> passed on the command line with no repository or system change, continuous memory sampling into the
> output root, a 120-minute external watchdog, no automatic retry and no distribution — and with the
> pagefile policy resolved as (a) or (b) above?

Fresh disk and commit measurements will be taken and recorded immediately before launch, as with the
previous run. This request covers exactly one run; any retry is a fresh ask.
