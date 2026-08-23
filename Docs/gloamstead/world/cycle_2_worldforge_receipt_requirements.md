# Cycle II WorldForge receipt requirements

This document defines the evidence required before a WorldForge run may be
considered a reproducible Cycle II world-production result. It does not
authorize generation, materialization, map editing, or a gameplay-state write
back. Gloamstead remains the owner of `Cycle2_Garden`, `GardenRot`, the exact
GardenBed target contract, persistence, warning interpretation, night outcome,
and dawn understanding.

## Input and provenance

Every receipt must identify all of the following fields:

- `gloamstead_source_revision`: immutable Gloamstead commit SHA used to read
  the intent.
- `gloamstead_source_clean`: explicit clean/dirty result for the source paths
  consumed by the run; a dirty source is not a clean-source receipt.
- `worldforge_source_revision`: immutable WorldForge commit SHA used by the
  generator.
- `worldforge_source_clean`: explicit clean/dirty result for the WorldForge
  source paths consumed by the run.
- `world_spec_path`: `specs/world/cycle-2-corruption-neglect.world.json`.
- `world_spec_sha256`: SHA-256 of the exact bytes consumed by the generator.
- `schema_path` and `schema_sha256`: the matching semantic schema and hash.
- `poi_id` and `poi_anchor_id`: exactly `Cycle2_Garden` and
  `Cycle2_Garden.Anchor`.
- `poi_coordinate_space` and `poi_anchor_translation`: exactly
  `sanctuary_bootstrap_local` and `[480.0, 160.0, 0.0]`.
- `poi_box_half_extents`: exactly `[240.0, 280.0, 160.0]`, with the receipt
  recording that the box is contained by the source-owned
  `[800.0, 800.0, 400.0]` sanctuary-bootstrap extent.
- `generation_input_seed` and `generation_input_version`: exactly `42` and
  `gloamstead-cycle2-corruption-neglect.v1` from the consumed world spec.
- `generator_revision`: the WorldForge generator/tool revision, including any
  materializer and survey command revisions.
- `generator_parameters`: all non-default generation parameters in addition
  to the required semantic `generationInput` values above.

The receipt must reject a mismatch between the named spec hash and the actual
input, a dirty source where `*_source_clean` claims clean, or a generator
revision that cannot be resolved to a source revision.

## Permitted subjects and output boundary

This slice has exactly one semantic subject and exactly one bounded POI:
`Cycle2_Garden`, with warning `GardenRot`, ritual type `GardenBed`, restoration
tag `GardenBed`, anchor `Cycle2_Garden.Anchor`, and map
`/Game/Maps/Lvl_Gloamstead`. Its POI is the
`sanctuary_bootstrap_local` box centered at `[480.0, 160.0, 0.0]` with
half-extents `[240.0, 280.0, 160.0]`. A receipt must reject any second,
decoy, or otherwise unlisted anchor or subject instead of selecting one.

All generated or materialized assets must be located below exactly:

```
/Game/Generated/WorldForge/Cycle2/
```

The receipt must list every output package and fail if any package escapes that
root. It must also list stable subject and survey identifiers, rejecting an
ambiguous or duplicate identifier rather than choosing an arbitrary subject.

## Required generic state scenarios

The run must evaluate the Gloamstead-projected WorldForge state address:

```
Scope     Region
ContextId Cycle2_Garden
Key       restoration_level
```

It must produce two separately labeled surveys:

| Scenario | `restoration_level` | Meaning |
| --- | ---: | --- |
| untouched | `0.0` | Exact Cycle II target has not been restored. |
| restored | `1.0` | Exact Cycle II target is restored in Gloamstead authoritative PCG state. |

WorldForge is a consumer of those values. A receipt may never claim that a
WorldForge placement, material, or survey chose the target, changed PCG state,
created an interpretation receipt, modified a save, or selected a night
outcome.

## Dual-state survey matrix

For *each* state scenario, the receipt must include a distinct survey record
for every reactive category below, tied to `Cycle2_Garden` and the state
address above:

| Reactive category | Required survey evidence in both `0.0` and `1.0` |
| --- | --- |
| foliage | stable placement/instance identifiers and state-dependent result |
| ruins | stable placement/instance identifiers and state-dependent result |
| paths | stable placement/instance identifiers and state-dependent result |
| lighting_materials | material/lighting binding identifiers and state-dependent result |

That is eight category-state survey records minimum: four at `0.0` and the
same four at `1.0`. The records must identify their input subject, map, anchor,
output package, state value, and survey ID. A successful one-state survey, a
single aggregate screenshot, or a generic count without subject provenance is
not sufficient.

## Evidence surfaces and review boundary

The spec binds the existing GardenRot support IDs to the already-authorized
surface kinds `environmental`, `object_reaction`, and `audio`, and records all
three in the `dawn_report` surface. WorldForge must preserve those identifiers
as subjects for survey/provenance only; it must not write prose, invent a clue,
or decide whether the player understood it.

Task 7 deliberately ends before WorldForge generation, NeoStack/Unreal
materialization, `Lvl_Gloamstead` editing, map-load validation, and human
playtest. Those require a subsequent approved materialization receipt plus
gameplay validation and human evidence.
