# Gloamstead

**A third-person dark fantasy sanctuary-restoration game.**

> *"I understood the warning. I restored the right place."*

![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7-blue?logo=unrealengine)
![Status](https://img.shields.io/badge/Status-Vertical%20Slice-orange)
![Phase](https://img.shields.io/badge/Phase-1.5%20Complete-green)
![License](https://img.shields.io/badge/License-Personal-lightgrey)

---

## Overview

Gloamstead is a focused third-person experience centered on the **Veil Heart** — a mysterious, growing source of light in a bleak, half-dead world. 

The player does not build bases, manage villages, or survive hordes. Instead, they **interpret cryptic warnings**, perform **ritualistic restorations** of meaningful structures, and face the **consequences** of their choices at night. Every restoration carries both mechanical weight and narrative meaning.

## Core Fantasy

The game is built around a single powerful loop:

**Warning → Understanding → Restoration → Consequence → Reflection**

Success is measured not by power or expansion, but by whether the player correctly interprets the world and restores the right places.

## Current Status

The foundational restoration loop has been implemented across three major phases:

| Phase | Focus | Status |
|-------|-------|--------|
| **Phase 0** | Ritual Data Contracts | ✅ Complete |
| **Phase 1** | Optimized PCG Subsystem | ✅ Complete |
| **Phase 1.5** | Player Placement System | ✅ Complete |

### Completed Systems

- **Ritual Data Layer** — `ERitualType`, `FRestorationEventPayload` (with `PointIndex`), and `URitualDefinition` Data Assets
- **PCG Subsystem** — High-performance hybrid state model + spatial hash grid for ritual point queries and mutations
- **Ritual Placement Component** — C++ base + Blueprint child with snapping, preview management, validation, and proper payload construction

## Project Structure

```
Gloamstead/
├── Source/Gloamstead/
│   ├── Data/                  # RitualTypes, RitualDefinition
│   ├── PCG/                   # UGloamsteadPCGSubsystem (optimized)
│   ├── Components/            # RitualPlacementComponent
│   └── Systems/               # VeilHeart, NightConsequenceManager (in progress)
├── Content/
│   ├── PCG/Graphs/            # Ritual Infrastructure graphs
│   ├── Blueprints/PCG/        # Placement component + previews
│   ├── Blueprints/Restoration/
│   └── Data/Rituals/          # Ritual Definition Data Assets
└── Docs/                      # Phase-specific implementation documentation
```

## Key Technical Features

- **Hybrid PCG State** — Fast parallel arrays for runtime mutations with on-demand metadata sync
- **Spatial Hash Acceleration** — Efficient nearest-neighbor queries for placement
- **Event-Driven Architecture** — Clean separation between restoration events and their narrative/mechanical consequences
- **Designer-Friendly** — Strong C++/Blueprint split with exposed parameters and debug visualization tools

## Documentation

Detailed implementation notes are available in the `Docs/` folder:

- [Architecture Overview](Docs/ArchitectureOverview.md)
- [Phase 0 – Ritual Data](Docs/Phase0_RitualData.md)
- [Phase 1 – PCG Subsystem](Docs/Phase1_PCGSubsystem.md)
- [Phase 1.5 – Placement Component](Docs/Phase1.5_PlacementComponent.md)

## Development

This project follows a strict vertical slice philosophy. The current focus is on proving the **Warning → Restoration → Consequence** loop before expanding scope.

### Prerequisites

- Unreal Engine 5.7+
- Visual Studio 2022 (for C++ development)

### Running the Project

1. Open `Gloamstead.uproject`
2. Generate project files if necessary
3. Build the project in your desired configuration

## Roadmap

### Phase 2 – Consequence & Reflection (Next Priority)

- **Night Consequence System**
  - Data structures for consequence types
  - Selection and spawning logic based on restoration state
  - Path-based threat modulation using `LightLevel` and `CorruptionLevel`
- **Veil Heart Dawn Reflection**
  - System for delivering contextual payoff based on satisfied warning tags
  - Journal / memory system tied to restorations
- **Restored Actor Integration**
  - Final art and VFX for `LanternPost` and `GardenBed`
  - Reactive behavior based on current `LightLevel`
- **Expanded Ritual Types**
  - `MirrorPillar` and `BellShrine` prototyping

### Phase 3 – Vertical Slice Polish

- Full persistence of dynamic ritual state (LightLevel, CorruptionLevel, Restored flags)
- Improved spatial grid tuning tools and auto-balancing
- Sound design and ambient systems tied to restoration state
- Basic UI for warnings and restoration feedback
- Performance profiling and optimization pass

### Longer Term Vision

- Support for multiple islands / larger world scope
- Additional ritual archetypes with unique mechanical and narrative roles
- Deeper integration between the Veil Heart’s personality and restoration choices
- Potential move toward a more sophisticated spatial data structure (e.g. Loose Octree)
- Expanded consequence variety (environmental, psychological, and entity-based)

## License

This is a personal project. All rights reserved.

---

*Built with Unreal Engine 5*