# Phase 1.5 – RitualPlacementComponent

## Overview

`URitualPlacementComponent` is the player-facing system responsible for discovering, previewing, and committing ritual restorations. It bridges player input with the PCG Subsystem.

**Architecture:**
- **C++ Base** (`URitualPlacementComponent`): Core logic, queries, payload construction, and Subsystem communication.
- **Blueprint Child** (`BP_RitualPlacementComponent`): Input binding, preview actor spawning/management, and visual feedback.

## Core Responsibilities

1. Enter/exit placement mode
2. Query the Subsystem for the nearest valid unrestored point
3. Manage preview actor (spawn, transform, valid/invalid state)
4. Validate placement
5. Build and submit `FRestorationEventPayload` (including `PointIndex`)
6. Spawn the final restored actor

## Key Design Decisions

### Snapping & Validation
- Uses `FindNearestUnrestoredPointIndex()` from the Subsystem
- Validates using `IsPointRestored()` (fast parallel state)
- Supports type preference (defaults to `LanternPost`)
- PathPoint redirection with one-time educational message

### Preview Management
- Previews are spawned as **independent world actors** (not attached to player)
- Transform comes from `GetCurrentTargetPointInfo()` (includes `TerrainNormal` alignment + camera bias on steep slopes)
- Visual state driven by `ValidState` material parameter (0 = invalid, 1 = valid)

### Payload Construction
The component is the **only** place that should build `FRestorationEventPayload`.

Critical line:
```cpp
Payload.PointIndex = FinalPointIndex;
```

This enables all downstream systems to use the fast getters on the Subsystem.

## Blueprint Integration Points

### Events from C++ (Implement in Blueprint)
- `OnPreviewTargetChanged(PointIndex, Type, bIsValid)`
- `OnPathPointRedirected(Message)`
- `OnPlacementConfirmed(PointIndex)`
- `OnPlacementModeExited()`

### Functions to Call from Blueprint
- `EnterPlacementMode()`
- `ExitPlacementMode()`
- `ConfirmPlacement()`
- `GetCurrentTargetPointInfo()` (recommended for transform)

### Recommended Custom Functions (Blueprint)
- `Update Preview Actor`
- `Update Preview Transform`
- `Update Preview Visual State`
- `Destroy Current Preview`

## Input Recommendations (Phase 1)

- Dedicated action: **IA_ActivateRitualLens**
- Confirm: **IA_ConfirmRitualPlacement**
- Cancel: **IA_CancelRitualPlacement**
- Default mode = `LanternPost`

## Material Parameter Convention (Preview Actors)

| Parameter                    | Type   | Valid Default       | Invalid Default    |
|-----------------------------|--------|---------------------|--------------------|
| `ValidState`                | Scalar | 1.0                 | 0.0                |
| `EmissiveIntensity`         | Scalar | 2.5                 | 0.6                |
| `BaseColorTint`             | Vector | (0.15, 0.85, 1.0)   | (1.0, 0.15, 0.15)  |
| `NiagaraSpawnRateMultiplier`| Scalar | 1.0                 | 0.15               |

## Testing Focus Areas

- Snapping accuracy (especially on steep terrain)
- PathPoint redirection behavior (first time only)
- `PointIndex` is correctly populated in the payload
- Preview actors are properly destroyed on exit/cancel
- No orphan preview actors left in the level

## Relationship to Other Systems

- **PCG Subsystem**: Primary consumer of queries and sole caller of `ApplyRestoration`
- **Restored Actors**: Spawned by this component (not by the Subsystem)

## Phase 2 additions (RP-1)

- **`RitualDefinitions`** map (`ERitualType` → `URitualDefinition`) drives `LightDelta`, `CorruptionCleared`, and default warning tags in the payload.
- **`OnRestoredActorSpawned`** Blueprint event after successful placement (implement meshes/VFX in BP).
- Payload includes **`TimeOfDayAtRestoration`** and **`NightCountAtRestoration`** from `UGloamsteadDayNightSubsystem` when present.

See [Phase2_CoreLoop.md](Phase2_CoreLoop.md) and [systems/02_restoration_system.md](systems/02_restoration_system.md).