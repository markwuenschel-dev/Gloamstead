# Corrected Wave 3 — GloamsteadForge Contracts + Hostile Runtime Validation (Plan)

**Branch:** `gloamstead/w3-gloamsteadforge-contracts` (off `main` @ `fdbf5a5`, which contains merged Wave 2)
**Baseline:** Wave 2 merged (PR #18). `main` == the exact tree gated at `7ccba5b` → gate GREEN (build + 34 tests) by construction.
**Identity rule:** Gloamstead owns *meaning*; GloamsteadForge owns *evidence discipline*. This is a validation wave — NO gameplay expansion, NO new night types, NO content/binary edits.

## Architecture

```
Real Wave 2 runtime  --(C++ automation emitter)-->  runtime report JSON  --(schema)--> structurally valid
                                                          |
                                                          +--(PS validators, fail-closed)--> semantically valid
                                                          +--(report integrity)-----------> fresh, non-stale, non-partial
                                                          +--(hostile negatives/fuzz)-----> fake success REJECTED
```

Evidence is emitted **only from a real runtime run** (a C++ automation test that drives the actual Wave 2
strategies/PCG on seeded state, then serializes the observed values). Hand-authored fixtures (good + bad)
exist only to exercise the validators; they are clearly marked as fixtures, never as live evidence.

## Decisions (Lane 1)

- **Evidence produced by tests:** the runtime reports (PCG init, restoration, night loop, sanctuary state,
  save/load) are emitted by a C++ automation test (`GloamsteadForgeEvidenceTests`) that runs the real loop
  headlessly and writes conformant JSON to `procedural/reports/gloamsteadforge/`. The test also asserts the
  reports carry real (non-default) values, so `gate.ps1` proves emission integrity.
- **Evidence requiring human PIE:** the live-feel check of `ANightPressureActor` + a real-hardware cascade
  with objective resolution (Lane 7). Headless-only for this wave unless a human gate is exercised; if not
  exercised, the report says so and claims NO human playtest.
- **Validators:** PowerShell 7 (`Test-Json -Schema` for structure + custom cross-field logic for
  fail-closed semantics + integrity checks). Not wired into `gate.ps1` (which is build+UE-tests); run as
  their own acceptance commands.
- **Stale-evidence detection:** report integrity compares `git_commit` in each report to repo HEAD and
  rejects mismatches; also rejects reports whose `generated_at_utc` is absent/implausible or whose scenario
  matrix claims full pass while any scenario is missing/failed.
- **Report success fails closed:** any missing required field, unknown enum, or success-without-substantiation
  yields a GF code and a non-zero validator exit.

## Deliverables / file ownership

- `specs/gloamsteadforge/contracts/*.schema.json` (6 schemas) + `failure_codes.md` (GF001–GF080).
- `Source/Gloamstead/Tests/GloamsteadForgeEvidenceTests.cpp` (NEW) — real-runtime emitter + assertions.
- `Source/Gloamstead/Systems/GloamsteadForgeEvidence.h/.cpp` (NEW) — report structs + JSON writer.
- `scripts/Test-GloamsteadForgeContracts.ps1`, `Validate-GloamsteadForgeRuntime.ps1`,
  `Test-GloamsteadForgeReportIntegrity.ps1`, `Test-GloamsteadForgeNegatives.ps1`, `Test-GloamsteadForgeFuzz.ps1`.
- `specs/gloamsteadforge/fixtures/good/*.json` + `bad/*.json`.
- `specs/gloamsteadforge/scenario_matrix.json` + `Docs/gloamsteadforge/evidence_index.md`.

## Scenario matrix (bounded)

Tutorial success · Corruption success · Corruption partial · Corruption failure · Quiet fallback ·
Save/load continuity · (negatives) invalid/fake report · stale evidence. No new gameplay types.

## Non-goals

New night types, Variant_Combat, content/`.uasset`/`.umap`, cook/package, plugins, EngineAssociation,
WorldForge, scenario generation, six-hour scope.
