# GloamsteadForge Evidence Index (Corrected Wave 3)

GloamsteadForge wraps the **proven Wave 2 night runtime** in evidence discipline: contracts, a real-runtime
evidence emitter, and hostile fail-closed validators. Gloamstead owns meaning; GloamsteadForge owns evidence.

## Contracts (`specs/gloamsteadforge/contracts/`)

| File | Purpose |
|------|---------|
| `GloamsteadForgeRuntimeReport.schema.json` | Composite per-scenario runtime report (self-contained). |
| `PCGInitProof.schema.json` / `RestorationProof.schema.json` / `NightLoopProof.schema.json` / `SanctuaryStateProof.schema.json` | Sub-proof object schemas. |
| `GloamsteadForgeScenario.schema.json` | Scenario-matrix entry. |
| `failure_codes.md` | GF001–GF080 failure-code map (validators emit these; zero codes = valid). |

## Runtime evidence (emitted, not committed)

`Source/Gloamstead/Tests/GloamsteadForgeEvidenceTests.cpp` (via `Source/Gloamstead/Systems/GloamsteadForgeEvidence.*`)
runs the **real** Wave 2 strategies/PCG per scenario and writes conformant JSON to
`procedural/reports/gloamsteadforge/` (git-ignored) plus a `_run_manifest.json`. Each report stamps the repo
`git_commit` and a **per-run nonce**. **Regenerate:** run `gate.ps1` (the emitter is an automation test).

### Provenance (unforgeable per-run nonce) + gate wiring

`gate.ps1` generates a fresh random `GLOAMSTEAD_FORGE_NONCE` each run, the emitter stamps it on every report
and the run manifest, and — in the **same** `gate.ps1` invocation, right after emission — the PS validators
run fail-closed with `-ExpectedNonce`. The integrity validator rejects any report whose `run_nonce` doesn't
match the run, or that isn't in the manifest set (**GF070**/**GF068**). A hand-authored report cannot know
the fresh nonce, so a fabricated "success" dropped into the reports dir is rejected even when every semantic
field is internally consistent and it carries the correct `git_commit`. The hostile PS layer is therefore
part of the automated gate, not a separate manual tier.

## Scenario matrix (`specs/gloamsteadforge/scenario_matrix.json`)

`tutorial_success` · `corruption_success` · `corruption_partial` · `corruption_failure` · `quiet_fallback`
· `saveload_continuity`. Each links to its emitted report.

## Validators (`scripts/`)

| Script | Checks | Acceptance |
|--------|--------|------------|
| `Test-GloamsteadForgeContracts.ps1 -Strict` | JSON-Schema structure | 6/6 live + 3/3 good fixtures pass |
| `Validate-GloamsteadForgeRuntime.ps1 -Strict` | Fail-closed semantics (success substantiated) | 6/6 live pass |
| `Test-GloamsteadForgeReportIntegrity.ps1 -Strict` | `git_commit`==HEAD, timestamp, matrix consistency | all live pass, matrix consistent |
| `Test-GloamsteadForgeNegatives.ps1` | Each known-bad fixture rejected by its expected code | 16/16 rejected |
| `Test-GloamsteadForgeFuzz.ps1 -Cases 300 -Strict` | Mutated good report always rejected | 300/300 rejected |

Shared rule engine: `scripts/GloamsteadForge.Common.ps1` (`Get-GFCodes`, `Get-GFIntegrityCodes`, `Get-GFScenarioMap`).

### Security model (authority)

A report's own `quiet` and `objective_kind` are attacker-controlled and are **not** trusted as switches
that disable substantiation. Authority for whether a scenario is quiet / objective-bearing comes from
`scenario_matrix.json`, bound by `scenario_id`. Absent a matrix binding, validation defaults to **strict**
(non-quiet), so a lone report cannot self-certify a benign night to skip checks. A matrix-objective-bearing
scenario whose report self-declares `objective_kind: None` is rejected (**GF043**); a report whose `quiet`
contradicts the matrix is rejected (**GF072**). Intrinsic invariants also apply: a night with no objective
applies no pressure and therefore cannot mutate the sanctuary (GF043), and a started night requires PCG
init (GF011). This model was hardened after a hostile review demonstrated a fabricated `Success` passing an
earlier report-gated version.

## Fixtures (`specs/gloamsteadforge/fixtures/`)

`good/` (3) — valid reference reports the validators must ACCEPT. `bad/` (17) — one contract violation each,
mapped to their expected GF code in the negatives suite; validators must REJECT them. Some bad fixtures claim
a real matrix `scenario_id` to test the matrix-authority binding (GF043 / GF072) and a cleanse objective on a
non-Corruption night (GF043).

## How to reproduce (acceptance order)

```powershell
pwsh -File ./gate.ps1                                                   # build + UE tests (emits live reports)
pwsh -File ./scripts/Test-GloamsteadForgeContracts.ps1 -Strict
pwsh -File ./scripts/Validate-GloamsteadForgeRuntime.ps1 -Strict
pwsh -File ./scripts/Test-GloamsteadForgeReportIntegrity.ps1 -Strict
pwsh -File ./scripts/Test-GloamsteadForgeNegatives.ps1
pwsh -File ./scripts/Test-GloamsteadForgeFuzz.ps1 -Cases 300 -Strict
```

## Live PIE / human gate

Not exercised this wave. **Headless validation complete. Live PIE feel-check remains pending. No human
playtest claimed.** Reports carry `human_playtest: false`; a report asserting a human playtest without a
recorded human gate is rejected (GF078).
