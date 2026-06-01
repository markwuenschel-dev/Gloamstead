# Gloamstead

**A third-person dark fantasy sanctuary-restoration game.**

> *"I understood the warning. I restored the right place."*

![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7-blue?logo=unrealengine)
![Status](https://img.shields.io/badge/Status-Vertical%20Slice-orange)
![Phase](https://img.shields.io/badge/Phase-2%20Core%20Loop-green)
![License](https://img.shields.io/badge/License-Personal-lightgrey)

---

## Overview

Gloamstead is a focused third-person experience centered on the **Veil Heart** — a mysterious, growing source of light in a bleak, half-dead world.

The player does not build bases, manage villages, or survive hordes. Instead, they **interpret cryptic warnings**, perform **ritualistic restorations** of meaningful structures, and face the **consequences** of their choices at night. Every restoration carries both mechanical weight and narrative meaning.

## Core Fantasy

**Warning → Understanding → Restoration → Consequence → Reflection**

Success is measured by whether the player correctly interprets the world and restores the right places.

## Current Status (June 2026)

The **vertical-slice core loop** is implemented in C++ on `main`: placement → restoration → dusk warning → night selection → night runtime → dawn reflection.

| Phase | Focus | Status |
|-------|-------|--------|
| **Phase 0** | Ritual data contracts | Complete |
| **Phase 1** | PCG subsystem + spatial grid | Complete |
| **Phase 1.5** | Ritual placement component | Complete |
| **Phase 2** | Day/night, night consequences, Veil Heart warnings | **Core loop complete** (stubs; no combat spawns yet) |

### Implemented systems (C++)

| System | Role |
|--------|------|
| `UGloamsteadPCGSubsystem` | Ritual point state, sanctuary snapshot, `ApplyCorruptionSpread` |
| `URitualPlacementComponent` | Placement, `URitualDefinition`-driven payloads, `OnRestoredActorSpawned` |
| `UGloamsteadDayNightSubsystem` | `Day → Dusk → Night → Dawn`, orchestrates prep and reflection |
| `UNightConsequenceManager` | Catalog scoring, `PrepareNightConsequences`, built-in MVP catalog fallback |
| `UNightConsequenceRuntime` | `BeginNight` / `EndNight`, type stubs, `OnOmenClueReady` |
| `AVeilHeart` | Warning catalog, tag satisfaction, dusk `EmitWarningForNight`, dawn reflection |

MVP night types in code: **Tutorial**, **Corruption**, **Omen**.

### Still manual / editor

- Assign optional `UNightConsequenceCatalog` / `UVeilHeartWarningCatalog` / `URitualDefinition` data assets for tuning (C++ defaults work in PIE without them).
- **Compile** after pulling; call `AdvanceToNextPhase` in a test Blueprint to drive the cycle.
- Restored-actor meshes/VFX via Blueprint (`OnRestoredActorSpawned`, `OnWarningEmitted`).

## Project Structure

```
Gloamstead/
├── Source/Gloamstead/
│   ├── Data/           # RitualTypes, RitualDefinition, NightConsequenceTypes, VeilHeartWarningTypes
│   ├── PCG/            # UGloamsteadPCGSubsystem
│   ├── Components/     # URitualPlacementComponent
│   └── Systems/        # DayNight, NightConsequence*, AVeilHeart
├── Content/            # PCG graphs, Blueprints, Data Assets (designer)
└── Docs/               # Design + implementation documentation
```

## Documentation

Start here: **[Docs/README.md](Docs/README.md)**

| Doc | Description |
|-----|-------------|
| [Architecture Overview](Docs/ArchitectureOverview.md) | Layers, data flow, maturity |
| [Phase 2 – Core Loop](Docs/Phase2_CoreLoop.md) | Day/night + night + Heart (implementation) |
| [Phase 0 – Ritual Data](Docs/Phase0_RitualData.md) | Payloads and ritual definitions |
| [Phase 1 – PCG](Docs/Phase1_PCGSubsystem.md) | Subsystem and performance model |
| [Phase 1.5 – Placement](Docs/Phase1.5_PlacementComponent.md) | Player placement flow |
| [Veil Heart (design)](Docs/systems/01_veil_heart_system.md) | Design + implementation status |
| [Restoration (design)](Docs/systems/02_restoration_system.md) | Design + implementation status |
| [Night Consequences (design)](Docs/systems/03_night_consequence_system.md) | Design + implementation status |
| [Project Rules (agents)](Docs/agents/ProjectRules.md) | UE5 / architecture conventions |

## Development

### Prerequisites

- Unreal Engine 5.7+
- Visual Studio 2022 (C++)

### Open and build

1. Clone and open `Gloamstead.uproject`.
2. Generate Visual Studio project files if prompted.
3. Build **Development Editor** for the `Gloamstead` module.
4. Place an **AVeilHeart** in the test map; use a BP or console hook to call `UGloamsteadDayNightSubsystem::AdvanceToNextPhase`.

### PIE smoke test (minimal)

1. Initialize PCG ritual points (existing map setup).
2. Restore at least one ritual point (placement component).
3. Advance phases: **Day → Dusk → Night → Dawn**.
4. Confirm logs: night type selected, optional corruption spread, Veil Heart warning/dawn lines.

## Roadmap

### Next (Phase 2 polish)

- Designer data assets for catalog, warnings, and ritual definitions.
- Blueprint UI/audio for `OnWarningEmitted` and `OnOmenClueReady`.
- Journal / structured dawn payoff (`wave-vh-2` planned).
- Night resource reward and failure hooks.

### Phase 3 – Vertical slice polish

- Restored actor meshes and VFX per ritual type.
- Persistence (save/load of point state).
- Expanded night types and spawn/mechanics (beyond stubs).
- `MirrorPillar` / `BellShrine` ritual types.

### Longer term

- Multiple places / larger scope, deeper Heart progression, full consequence variety.

## License

Personal project. All rights reserved.

---

*Built with Unreal Engine 5*