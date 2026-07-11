# Gloamstead Ritual System – Architecture Overview

## Core Philosophy

> “I understood the warning. I restored the right place.”

The restoration system is designed around **meaningful cause and effect**, not generic building or tower defense.

## System Layers

### Layer 0 – Data Contracts (Phase 0)

- `ERitualType`, `FRestorationEventPayload`, `URitualDefinition`
- `ENightConsequenceType`, `UNightConsequenceCatalog`, `FNightSanctuarySnapshot`
- `FVeilHeartWarningFragment`, `UVeilHeartWarningCatalog`

### Layer 1 – World State Authority (Phase 1)

**`UGloamsteadPCGSubsystem`**

- Owns ritual point state (`FRitualPointState`, spatial grid)
- `ApplyRestoration`, `ApplyCorruptionSpread`, sanctuary aggregates
- Broadcasts `OnStructureRestored`

### Layer 1.5 – Player Interaction (Phase 1.5 → RP-1)

**`URitualPlacementComponent`**

- Snapping, validation, definition-driven payloads
- `OnRestoredActorSpawned` for Blueprint visuals

### Layer 2 – Schedule & consequences (Phase 2)

**`UGloamsteadDayNightSubsystem`**

- Phase authority: Day / Dusk / Night / Dawn
- Invokes night prep, runtime, and dawn reflection in order

**`UNightConsequenceManager` + `UNightConsequenceRuntime`**

- Dusk: select night type from catalog + snapshot
- Night: execute type stub (spread, omen delegate, etc.)

**`AVeilHeart`**

- Warnings at dusk; tag satisfaction on restore; reflection at dawn

## Data Flow (Core Loop)

```
Player Input
    ↓
RitualPlacementComponent
    ↓
BuildRestorationPayload → ApplyRestoration (PCG)
    ↓
OnStructureRestored(Payload)
    ├→ VeilHeart (EvaluateRestorationAgainstWarnings)
    └→ NightConsequenceManager (path coverage, future)

[Later: AdvanceToNextPhase → Dusk]
    ↓
PrepareNightConsequences → OnNightPlanReady
    ↓
VeilHeart::EmitWarningForNight

[Night]
    ↓
NightConsequenceRuntime::BeginNight → ExecuteNightStub

[Dawn]
    ↓
EndNight → VeilHeart::ProcessDawnReflection
```

## Performance Architecture

| Concern | Solution | Location |
|---------|----------|----------|
| Frequent state reads | `PointStates` array | PCG Subsystem |
| Nearest neighbor | Spatial hash grid | PCG Subsystem |
| Metadata writes | `SyncPointToMetadata()` only when needed | PCG Subsystem |
| Event context | `PointIndex` on payload | Payload |

## Key Design Rules

1. **PCG subsystem** is the source of truth for ritual point state.
2. **Placement component** constructs `FRestorationEventPayload` (only place).
3. **DayNight subsystem** owns phase transitions; do not duplicate dusk/dawn hooks elsewhere without reason.
4. **Downstream systems** use `Payload.PointIndex` + subsystem getters, not location scans in hot paths.

## Current Maturity (June 2026)

| Layer | Status |
|-------|--------|
| Phase 0 – Data | Complete |
| Phase 1 – PCG | Complete |
| Phase 1.5 – Placement | Complete |
| Phase 2 – Core loop | **Complete in C++** (stubs, no spawn pipeline) |
| Phase 2 – Polish | Journal, assets, VFX — planned. Persistence — **built** (`UGloamsteadSaveGame`, full per-point state, test green) but not yet wired to the loop |

The event pipeline **Placement → PCG → Listeners → Day/Night → Night runtime → Dawn** is implemented and testable in PIE.

**Detail:** [Phase2_CoreLoop.md](Phase2_CoreLoop.md)

## Future Expansion

- Full runtime behavior branches for the `MirrorPillar`, `BellShrine` ritual types (the enum values already exist)
- Full night-type enum and entity spawning
- Wiring the existing save/load (autosave-at-dawn, load-on-start); optional full serialization of derived dynamic attributes
- Optional spatial structure upgrade (e.g. loose octree) for larger worlds