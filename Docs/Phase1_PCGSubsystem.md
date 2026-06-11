# Phase 1 – UGloamsteadPCGSubsystem (Optimized)

## Overview

The `UGloamsteadPCGSubsystem` is the **authoritative runtime owner** of all ritual point state. It is the central hub that:

- Owns the mutable ritual point data
- Provides fast queries for the Placement Component
- Applies restoration mutations
- Broadcasts `OnStructureRestored` to interested systems (Veil Heart, Night Consequence Manager, VFX, etc.)

## Architecture: Hybrid State Model

### Problem
Directly writing to PCG metadata on every restoration (`SetMetadataEntry` + `SetPoints()`) is expensive and does not scale.

### Solution: Hybrid Model

| Layer                    | Purpose                              | Performance |
|--------------------------|--------------------------------------|-----------|
| `TArray<FRitualPointState> PointStates` | Fast mutable state (`bIsRestored`, `LightLevel`, `CorruptionLevel`) | Extremely fast |
| `TArray<FPCGPoint> CachedPoints` | Static + spatial data + fallback | Moderate |
| PCG Metadata (on demand) | Only synced when explicitly needed | Expensive (avoid in hot path) |

**Rule:**  
`ApplyRestoration()` and frequent queries use the parallel `PointStates` array. Metadata is only written via `SyncPointToMetadata()` for debug, save/load, or VFX binding.

## Key Features

### 1. Fast Queries

```cpp
bool IsPointRestored(int32 PointIndex) const;
float GetLightLevel(int32 PointIndex) const;
float GetCorruptionLevel(int32 PointIndex) const;
int32 FindNearestUnrestoredPointIndex(...);
```

These all read from `PointStates` — no metadata cost.

### 2. Spatial Hash Grid

A uniform grid (`TMap<FIntVector, FRitualSpatialCell>`) accelerates `FindNearestUnrestoredPointIndex`.

- Default `CellSize = 400.0f` (tunable)
- Dramatically faster than linear search once point count grows beyond ~300–400

**Debug Visualization:** Use `DrawDebugSpatialGrid()` to inspect cell occupancy and tune `CellSize`.

### 3. Initialization

```cpp
void InitializeFromPCGComponent(UPCGComponent* PCGComponent, int32 WorldSeed);
```

- Duplicates the PCG Point Data (original graph asset is never mutated)
- Builds both `PointStates` and the Spatial Grid

### 4. Restoration Flow (Strict Order)

1. Update parallel state (`PointStates`)
2. Track in `RestoredPointIndices`
3. Broadcast `OnStructureRestored(Payload)`

Metadata is **not** updated here.

## Persistence Strategy (Vertical Slice)

- Store only `TSet<int32> RestoredPointIndices` + World Seed
- On load: Regenerate PCG data → call `ReapplyRestoredState()`

Full serialization of `LightLevel`/`CorruptionLevel` can be added later.

## Debug Tools

| Function                        | Purpose |
|--------------------------------|---------|
| `DrawDebugRitualPoints()`      | Visualizes all points + restoration state |
| `DrawDebugSpatialGrid()`       | Shows grid cells colored by occupancy (critical for tuning `CellSize`) |
| `SetDrawSpatialGridDebug()`    | Toggle from Blueprint / Manager |

## Public API Summary

**Primary Consumers:**
- `URitualPlacementComponent` (queries + `ApplyRestoration`)
- `AVeilHeart`
- `UNightConsequenceManager`

**Recommended Access Pattern:**
```cpp
if (Payload.PointIndex != -1)
{
    float light = Subsystem->GetLightLevel(Payload.PointIndex);
}
```

## Performance Notes

- `ApplyRestoration` is now extremely cheap (array write + delegate).
- Spatial queries are O(1) average in well-tuned grids.
- Metadata writes are the only expensive operation — keep them explicit and infrequent.

## Phase 2 additions (night loop)

- **Sanctuary aggregates:** `GetSanctuaryAverageLightLevel`, `GetSanctuaryAverageCorruptionLevel`, `GetRestoredCountByRitualType`, `BuildSanctuarySnapshot` — used by `UNightConsequenceManager` at dusk.
- **`ApplyCorruptionSpread(Delta, MaxPoints)`:** night-only corruption bump on random points; does not alter `bIsRestored`.

See [Phase2_CoreLoop.md](Phase2_CoreLoop.md).

**Editor gate (2026-06-11):** Data assets and day/night PIE smoke verified without PCG ([specs/data/VERIFICATION-2026-06-11.md](../specs/data/VERIFICATION-2026-06-11.md)). **Next:** place PCG graph in `Lvl_ThirdPerson` and call `InitializeFromPCGComponent` at level start so placement, corruption spread, and catalog night scoring use real point data.