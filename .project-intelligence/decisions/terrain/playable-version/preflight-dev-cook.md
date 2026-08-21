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

## The concrete authorization request

> Do you authorize one Win64 Development cook of commit `fa87a66b50cef2798043c60ef458d1ebf0a483ec`,
> to `Saved/Packages/20260820-fa87a66-Win64Dev/`, with 243,222,454,272 bytes free measured
> 2026-08-20 (to be re-measured immediately before running), logs retained at that same output
> root, no distribution, and package-smoke evidence captured under the T6 acceptance checklist?
