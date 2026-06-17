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

The **vertical-slice core loop** (placement → restoration → dusk warning → night selection from catalog → night runtime → dawn reflection) is implemented in C++ on `main`.

Development is driven by the **agent_collab** substrate (see `docs/agents/UE5-Agent-Substrate-Review.md`). The `wave-vs-polish-202606` polish wave is active. **Data asset factory verified 2026-06-11:** JSON manifest → `GloamsteadImportDataAssets` commandlet → six `Content/Data/DA_*` assets, wired on `Lvl_ThirdPerson`, PIE day/night smoke passed ([specs/data/VERIFICATION-2026-06-11.md](specs/data/VERIFICATION-2026-06-11.md)). Next gate: PCG init for restoration and varied night selection. A headless automation gate (`gate.ps1`) now grounds shipped logic — **16 tests green** (PCG state/persistence, night selection, Veil Heart warnings); see [Roadmap](docs/ROADMAP.md).

| Phase | Focus | Status |
|-------|-------|--------|
| **Phase 0** | Ritual data contracts | Complete |
| **Phase 1** | PCG subsystem + spatial grid | Complete |
| **Phase 1.5** | Ritual placement component | Complete |
| **Phase 2** | Day/night, night consequences, Veil Heart warnings + polish | **Core loop in C++**; polish wave active (data assets, UI, visuals, persistence, night/combat pressure) |

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

### Data assets + editor verification (2026-06-11)

| Item | Status |
|------|--------|
| Import factory (`specs/data/` + `Invoke-GloamsteadDataAssetImport.ps1`) | **Verified** |
| `Content/Data/` starter catalogs + 4 ritual DAs | **Present** |
| `Lvl_ThirdPerson` wiring (Veil Heart, ritual DAs, T test key) | **Verified** |
| PIE catalog load + day/night cycle | **Verified** |
| PCG init + ritual restore smoke | **Next** |

Docs: [specs/data/WIRING.md](specs/data/WIRING.md), [specs/data/HUMAN_RUN_IMPORT.md](specs/data/HUMAN_RUN_IMPORT.md), [specs/data/VERIFICATION-2026-06-11.md](specs/data/VERIFICATION-2026-06-11.md).

### Still manual / editor

- **PCG graph** in level + `InitializeFromPCGComponent` (blocks restoration and non-Tutorial nights).
- Restored-actor meshes/VFX, UI, persistence (polish wave handoffs).
- **Compile** after pulling C++; use **Advance Gloamstead Day Phase** in Level Blueprint for PIE smoke (`UGloamsteadBlueprintLibrary`).

## Project Structure

```
Gloamstead/
├── Source/Gloamstead/
│   ├── Data/           # RitualTypes, RitualDefinition, NightConsequenceTypes, VeilHeartWarningTypes
│   ├── PCG/            # UGloamsteadPCGSubsystem
│   ├── Components/     # RitualPlacementComponent
│   ├── Systems/        # DayNight, NightConsequence*, VeilHeart
│   └── Variant_* /     # Prototype-specific gameplay (Combat, Platforming, SideScrolling)
├── Content/Data/       # Generated DA_* (import factory); verified 2026-06-11
├── specs/data/         # Manifests, WIRING, verification records
├── Content/            # PCG graphs, Blueprints, maps
├── docs/               # Design + implementation documentation (source of truth)
└── agent_collab/       # Multi-runtime collaboration substrate (protocol, playbooks, state)
```

## Documentation

Start here: **[docs/README.md](docs/README.md)**

**Agent collaboration (current operating model)**: [docs/agents/UE5-Agent-Substrate-Review.md](docs/agents/UE5-Agent-Substrate-Review.md) (minimal roster diagnosis) + `agent_collab/` (protocol, playbooks, policies).

| Doc | Description |
|-----|-------------|
| [Architecture Overview](docs/ArchitectureOverview.md) | Layers, data flow, maturity |
| [Roadmap](docs/ROADMAP.md) | Tracks, triggers, and the `gate.ps1` automation oracle |
| [Phase 2 – Core Loop](docs/Phase2_CoreLoop.md) | Day/night + night + Heart (implementation) |
| [Phase 0 – Ritual Data](docs/Phase0_RitualData.md) | Payloads and ritual definitions |
| [Phase 1 – PCG](docs/Phase1_PCGSubsystem.md) | Subsystem and performance model |
| [Phase 1.5 – Placement](docs/Phase1.5_PlacementComponent.md) | Player placement flow |
| [Veil Heart (design)](docs/systems/01_veil_heart_system.md) | Design + implementation status |
| [Restoration (design)](docs/systems/02_restoration_system.md) | Design + implementation status |
| [Night Consequences (design)](docs/systems/03_night_consequence_system.md) | Design + implementation status (includes full Data Asset Authoring Guide) |
| [Agent Project Rules](docs/agents/ProjectRules.md) | UE5 / architecture conventions for agents |
| [UE5 Agent Substrate Review](docs/agents/UE5-Agent-Substrate-Review.md) | Current minimal agent roster, policies, and operating model |

## Development & Collaboration

Development uses the **agent_collab** substrate (v8.1, UE5-tuned). See the full review in `docs/agents/UE5-Agent-Substrate-Review.md` (minimal roster: orchestrator, planner, coder, critic + playbooks for former architect/researcher/documentor roles).

### For humans (or starting as Orchestrator)
- After clone/adapter changes: `pwsh -NoProfile -File agent_collab/scripts/Project-GrokAdapter.ps1`
- Start orchestrator (this Grok session): **`/gloam-resume`**
- Status only: **`/gloam-status`**
- Full rules: `agent_collab/context/agent_rules.md`, `workflow_activation.json`, `human_approval_gates.md`, and the playbooks under `agent_collab/playbooks/`.

### Prerequisites

- Unreal Engine 5.7+
- Visual Studio 2022 (C++)
- PowerShell 7+ (for agent_collab scripts)

### Open and build

1. Clone and open `Gloamstead.uproject`.
2. Generate Visual Studio project files if prompted.
3. Build **Development Editor** for the `Gloamstead` module.
4. For data-driven features: create/assign catalogs in `Content/Data/` (see current wave handoffs).

### PIE smoke test

**Done (2026-06-11):** `Lvl_ThirdPerson` Level Blueprint **T** → `Advance Gloamstead Day Phase`; log shows catalog load + phase transitions. See [specs/data/VERIFICATION-2026-06-11.md](specs/data/VERIFICATION-2026-06-11.md).

**Next:**

1. Initialize PCG ritual points (`InitializeFromPCGComponent`).
2. Restore at least one ritual point; confirm `DA_Ritual_*` tuning in log.
3. Advance phases until non-Tutorial night + catalog dusk warning (needs PCG snapshot).

## Roadmap

Current focus (`wave-vs-polish-202606`): **PCG level hookup** (after data-asset factory verification), then UI, visuals, persistence, night/combat pressure. Data asset `editor-generation` + `map-load` gate **closed** 2026-06-11 per [specs/data/VERIFICATION-2026-06-11.md](specs/data/VERIFICATION-2026-06-11.md).

### Next (Phase 2 polish)

- Concrete `Content/Data/` catalogs + ritual definitions (human editor + verification).
- Blueprint UI/audio hooks for warnings and clues.
- Journal / structured dawn payoff.
- Night resource/failure mechanics.

### Phase 3 – Vertical slice polish

- Restored actor meshes/VFX per ritual type.
- Save/load persistence.
- Expanded night types + spawns (beyond stubs).
- Additional ritual types (`MirrorPillar`, `BellShrine`).

### Longer term

- Multiple areas / larger scope.
- Deeper Heart progression and adaptation.
- Full variety of night consequences.

## License

Personal project. All rights reserved.

---

*Built with Unreal Engine 5*

---

**Note on development**: This project uses a minimal, UE5-aware agent collaboration system. See `docs/agents/UE5-Agent-Substrate-Review.md` and `agent_collab/` for the current model (4 core roles + playbooks, strong human gates for editor work and verification). The root docs and this README are kept in sync with active waves.