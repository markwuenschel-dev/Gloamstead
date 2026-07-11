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
`procedural/reports/gloamsteadforge/` (git-ignored). Each report stamps the repo `git_commit` at emit time,
so stale evidence is detectable. **Regenerate:** run `gate.ps1` (the emitter is an automation test).

## Scenario matrix (`specs/gloamsteadforge/scenario_matrix.json`)

`tutorial_success` · `corruption_success` · `corruption_partial` · `corruption_failure` · `quiet_fallback`
· `saveload_continuity`. Each links to its emitted report.

## Validators (`scripts/`)

| Script | Checks | Acceptance |
|--------|--------|------------|
| `Test-GloamsteadForgeContracts.ps1 -Strict` | JSON-Schema structure | 6/6 live + 3/3 good fixtures pass |
| `Validate-GloamsteadForgeRuntime.ps1 -Strict` | Fail-closed semantics (success substantiated) | 6/6 live pass |
| `Test-GloamsteadForgeReportIntegrity.ps1 -Strict` | `git_commit`==HEAD, timestamp, matrix consistency | all live pass, matrix consistent |
| `Test-GloamsteadForgeNegatives.ps1` | Each known-bad fixture rejected by its expected code | 14/14 rejected |
| `Test-GloamsteadForgeFuzz.ps1 -Cases 300 -Strict` | Mutated good report always rejected | 300/300 rejected |

Shared rule engine: `scripts/GloamsteadForge.Common.ps1` (`Get-GFCodes`, `Get-GFIntegrityCodes`).

## Fixtures (`specs/gloamsteadforge/fixtures/`)

`good/` (3) — valid reference reports the validators must ACCEPT. `bad/` (14) — one contract violation each,
mapped to their expected GF code in the negatives suite; validators must REJECT them.

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
