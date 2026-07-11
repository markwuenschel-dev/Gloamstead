# GloamsteadForge Failure Codes (GF001–GF080)

Validators emit these codes when a runtime report fails a contract. A report is **valid** only when the
validator produces **zero** codes. Success claims must be *substantiated*: a report claiming success while
any substantiating field is missing/false is a failure, not a pass (**fail closed**).

Band allocation:

| Band | Area |
|------|------|
| GF001–GF009 | Structure / schema |
| GF010–GF019 | PCG init |
| GF020–GF029 | Restoration |
| GF030–GF044 | Night loop / objective / outcome |
| GF045–GF054 | Sanctuary state mutation |
| GF055–GF059 | Dawn reflection |
| GF060–GF064 | Save/load continuity |
| GF065–GF072 | Report integrity (stale / partial / matrix) |
| GF073–GF080 | Scope / enum / cross-cutting |

## Structure / schema

- **GF001** — Report is not valid JSON.
- **GF002** — Report fails its JSON Schema (missing required field or wrong type).
- **GF003** — Missing/blank `schema` version tag.
- **GF004** — Missing/blank `scenario_id`.
- **GF005** — Missing/blank `generated_at_utc`.

## PCG init

- **GF010** — Missing `pcg_init` object.
- **GF011** — `pcg_init.initialized` is false while the scenario requires init.
- **GF012** — `pcg_init.initialized` is true but `point_count` <= 0 (init claimed with no points).
- **GF013** — `point_count` negative.

## Restoration

- **GF020** — Missing `restoration` object.
- **GF021** — `restoration.applied` true but `point_index` < 0 (invalid PointIndex).
- **GF022** — `restoration.attempted` false but `restoration.applied` true (applied without an attempt).
- **GF023** — Success outcome for an objective-bearing night with `restoration.applied` false
  (success claimed without a cleanse action).

## Night loop / objective / outcome

- **GF030** — Missing `night_loop` object.
- **GF031** — `night_loop.started` false but an `outcome_result` other than `None` is present.
- **GF032** — Unknown `night_type` (not in the ENightConsequenceType display set).
- **GF033** — Unknown `objective_kind`.
- **GF034** — Unknown `outcome_result` (not None/Success/Partial/Failure).
- **GF035** — `outcome_result` is `Success` but `objective_resolved` is false (for an objective-bearing night).
- **GF036** — `outcome_result` is `Partial` or `Failure` but `objective_resolved` is true (contradiction).
- **GF037** — Objective-bearing night with `target_point_index` < 0 (no target selected).
- **GF038** — `ended_intentionally` false (night ended only by hard timeout / no intentional condition modeled).
- **GF039** — `result_tag` missing for a non-None outcome.
- **GF040** — Corruption `Success` but `result_tag` != `CorruptionCleansed`.
- **GF041** — Corruption `Partial` but `result_tag` != `CorruptionLingers`.
- **GF042** — Corruption `Failure` but `result_tag` != `CorruptionScar`.
- **GF043** — A night type declared "objective-bearing" resolves with `objective_kind` == `None`.
- **GF044** — `Partial`/`Failure` claimed unreachable: the scenario matrix declares a type but never
  produces a non-Success outcome across the matrix (fake "always wins").

## Sanctuary state mutation

- **GF045** — Missing `sanctuary_state` object.
- **GF046** — `mutated` false for a night that applied pressure (state did not change).
- **GF047** — `Success` (cleanse) but `target_corruption_after` >= `target_corruption_before`
  (claimed cleanse with no reduction).
- **GF048** — `Partial` but `target_corruption_after` >= `target_corruption_before` (no reduction).
- **GF049** — `Failure` but `target_corruption_after` < `target_corruption_before` (bloom improved on a "failure").
- **GF050** — Corruption values out of range [0,1].

## Dawn reflection

- **GF055** — Missing `dawn_reflection` object.
- **GF056** — `dawn_reflection.consumed_outcome` false (dawn did not receive the night outcome).
- **GF057** — `dawn_reflection.outcome_result` != `night_loop.outcome_result` (dawn saw a different outcome).

## Save/load continuity

- **GF060** — Continuity scenario but `save_load.checked` false.
- **GF061** — `save_load.checked` true but `roundtrip_ok` false (state did not survive save/load).
- **GF062** — `save_load` claims roundtrip_ok but reports no pre/post state to compare.

## Report integrity (stale / partial / matrix)

- **GF065** — `git_commit` does not match repo HEAD (stale evidence).
- **GF066** — `git_branch` missing/blank.
- **GF067** — `generated_at_utc` unparseable/implausible (future, or epoch-zero).
- **GF068** — Scenario matrix marks overall pass while a listed scenario has no report.
- **GF069** — Scenario matrix marks overall pass while a listed report contains failure codes.
- **GF070** — Report references telemetry/artifact paths that do not exist.
- **GF071** — Duplicate `scenario_id` across the matrix.
- **GF072** — Report's self-declared `quiet` (or benign `outcome_result: None`) contradicts the scenario matrix's declared `quiet` for that `scenario_id`. A report may not self-certify a quiet/benign night to skip substantiation — authority is the matrix.

## Scope / cross-cutting

- **GF073** — Report claims an unauthorized binary/`.uasset`/`.umap` mutation.
- **GF074** — Report claims vendor-content modification.
- **GF075** — `engine` tag missing/mismatched.
- **GF076** — Report `schema` version unknown to the validator.
- **GF077** — Success claimed for an unknown/unsupported night type (should be benign fallback).
- **GF078** — Report asserts a human playtest without a corresponding human-gate record.
- **GF079** — Required field present but null where a value is contractually required.
- **GF080** — Any other fail-closed rejection (validator default-deny catch-all).
