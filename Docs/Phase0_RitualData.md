# Phase 0 – Ritual Data Foundation

## Overview

Phase 0 establishes the core data contracts for the restoration system in Gloamstead. These types form the foundation for the PCG Subsystem, Placement Component, Veil Heart, and Night Consequence systems.

## Core Types

### ERitualType (UENUM)

**Location:** `Source/Gloamstead/Data/RitualTypes.h`

```cpp
UENUM(BlueprintType)
enum class ERitualType : uint8
{
    Invalid         = 0,
    LanternPost     = 1,
    GardenBed       = 2,
    PathPoint       = 3,
    // MirrorPillar and BellShrine deferred to Phase 2
};
```

**Phase 1 Scope:** Only `LanternPost`, `GardenBed`, and `PathPoint` are active.

**Design Rule:**  
`PathPoint` types are **not directly restorable** in Phase 1. The `RitualPlacementComponent` redirects the player to the nearest `LanternPost` when a PathPoint is targeted.

### FRestorationEventPayload (USTRUCT)

**Location:** `Source/Gloamstead/Data/RitualTypes.h`

This is the primary communication contract for a successful restoration.

**Key Fields (as of final Phase 0):**

| Field                        | Type     | Purpose |
|-----------------------------|----------|--------|
| `RitualType`                | ERitualType | What was restored |
| `WorldLocation`             | FVector  | Where it happened |
| `PathSegmentID`             | int32    | For light propagation & path memory |
| `PathPosition`              | float    | 0–1 position along segment |
| `RestoredActor`             | TWeakObjectPtr<AActor> | Reference to the final placed actor |
| `LightDelta`                | float    | Light contributed |
| `CorruptionCleared`         | float    | Corruption reduced |
| `WarningTagSatisfied`       | FName    | Links to Veil Heart warnings |
| `TimeOfDayAtRestoration`    | float    | 0 = dawn, 1 = dusk |
| `PointIndex`                | int32    | **Critical** – Direct index into PCG Subsystem for fast state access |

**Important:** `PointIndex` was added in late Phase 0/early Phase 1 to enable high-performance access via the optimized Subsystem getters (`GetLightLevel`, `GetCorruptionLevel`, etc.).

### URitualDefinition (UPrimaryDataAsset)

**Location:** `Source/Gloamstead/Data/RitualDefinition.h`

Lightweight Data Asset base class for per-ritual tuning.

**Recommended Assets:**
- `DA_Ritual_LanternPost`
- `DA_Ritual_GardenBed`

This pattern keeps tuning data-driven and avoids hardcoding values in C++ or Blueprints.

## Helper Functions

Located in `RitualTypes.cpp`:

- `GetRitualTypeDisplayName()`
- `IsDirectlyRestorable()`
- `GetDefaultLightContribution()`
- `GetDefaultCorruptionClearance()`

## Usage Guidelines

- **PCG Graph** writes `RitualType` as `int32` attribute.
- **Subsystem** is the single source of truth for runtime state.
- **Placement Component** is the only place that should construct `FRestorationEventPayload`.
- **Veil Heart** consumes `WarningTagSatisfied` and `PointIndex`.
- **Night Consequence Manager** uses `PathSegmentID`, `TimeOfDayAtRestoration`, and fast state getters via `PointIndex`.

## Design Principles

1. The payload carries **context** (what happened and why it matters).
2. The Subsystem carries **current state** (via fast parallel arrays).
3. `PointIndex` is the bridge between the two.