# Gloamstead Ritual System – Architecture Overview

## Core Philosophy

> “I understood the warning. I restored the right place.”

The restoration system is designed around **meaningful cause and effect**, not generic building or tower defense.

## System Layers

### Layer 0 – Data Contracts (Phase 0)
- `ERitualType`
- `FRestorationEventPayload` (single source of truth for a restoration event)
- `URitualDefinition` Data Assets

### Layer 1 – World State Authority (Phase 1)
**`UGloamsteadPCGSubsystem`**

- Owns all ritual point data at runtime
- Hybrid performance model (fast parallel state + PCG metadata on demand)
- Spatial hash acceleration for placement queries
- Broadcasts `OnStructureRestored`

### Layer 1.5 – Player Interaction (Phase 1.5)
**`URitualPlacementComponent`**

- Discovers and snaps to ritual points
- Manages preview actors and validation
- Constructs and submits restoration payloads (including `PointIndex`)

### Layer 2 – Narrative & Consequence Systems (Phase 1 → Phase 2)
- **Veil Heart**: Interprets restorations against previous warnings → Dawn reflection payoff
- **Night Consequence Manager**: Uses restoration data to shape the coming night

## Data Flow (Core Loop)

```
Player Input
    ↓
RitualPlacementComponent (queries + validation)
    ↓
BuildRestorationEventPayload (includes PointIndex)
    ↓
UGloamsteadPCGSubsystem::ApplyRestoration()
    ├── Update fast parallel state (PointStates)
    ├── Track RestoredPointIndices
    └── Broadcast OnStructureRestored(Payload)
            ↓
    ┌───────┴───────┐
    │               │
Veil Heart     Night Consequence Manager
(Meaning)      (Consequence)
    │               │
    └───────┬───────┘
            ↓
Dawn Reflection + Next Night Behavior
```

## Performance Architecture

| Concern                    | Solution                              | Location |
|---------------------------|---------------------------------------|----------|
| Frequent state reads      | Parallel `FRitualPointState` array    | Subsystem |
| Nearest neighbor queries  | Spatial Hash Grid                     | Subsystem |
| Expensive metadata writes | Explicit `SyncPointToMetadata()` only | Subsystem |
| Payload as event context  | `PointIndex` for fast follow-up reads | Payload |

## Key Design Rules

1. **The Subsystem is the only source of truth** for current ritual point state.
2. **The Placement Component is the only place** that constructs `FRestorationEventPayload`.
3. **Downstream systems** should prefer `Payload.PointIndex` + fast getters over location-based lookups.
4. **Metadata is not free** — treat writes as a deliberate, infrequent operation.

## Future Expansion (Phase 2+)

- Additional Ritual Types (`MirrorPillar`, `BellShrine`)
- More sophisticated consequence selection based on `RitualType` + current state
- Full save/load of dynamic attributes
- Potential move from Spatial Hash → Loose Octree for larger world support
- GPU-driven PCG updates (long-term)

## Current Maturity (as of this document)

- Phase 0: Complete
- Phase 1: Complete (optimized)
- Phase 1.5: Complete
- Phase 2: Not yet started

The restoration event pipeline (`Placement → Subsystem → Payload → Listeners`) is now solid and performant.