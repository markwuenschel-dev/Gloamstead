# Gloamstead documentation

Design and implementation docs for the Gloamstead vertical slice. Markdown in this tree is the source of truth for humans and coding agents.

**Last updated:** 2026-06-01

---

## Start here

| If you want… | Read |
|--------------|------|
| What the game is | [game/00_current_design_baseline.md](game/00_current_design_baseline.md) |
| How code is layered | [ArchitectureOverview.md](ArchitectureOverview.md) |
| What is implemented today | [Phase2_CoreLoop.md](Phase2_CoreLoop.md) |
| Agent / UE conventions | [agents/ProjectRules.md](agents/ProjectRules.md) |

---

## Implementation (phases)

| Phase | Document | Status |
|-------|----------|--------|
| 0 | [Phase0_RitualData.md](Phase0_RitualData.md) | Complete |
| 1 | [Phase1_PCGSubsystem.md](Phase1_PCGSubsystem.md) | Complete |
| 1.5 | [Phase1.5_PlacementComponent.md](Phase1.5_PlacementComponent.md) | Complete |
| 2 | [Phase2_CoreLoop.md](Phase2_CoreLoop.md) | **Core loop in C++** |

---

## System design (+ implementation notes)

Each file includes an **Implementation status** section where the vertical slice has landed in code.

| System | Document |
|--------|----------|
| Veil Heart | [systems/01_veil_heart_system.md](systems/01_veil_heart_system.md) |
| Restoration | [systems/02_restoration_system.md](systems/02_restoration_system.md) |
| Night consequences | [systems/03_night_consequence_system.md](systems/03_night_consequence_system.md) |
| Combat & interaction | [systems/04_combat_and_interaction_system.md](systems/04_combat_and_interaction_system.md) |
| Progression & endings | [systems/05_progression_and_endings.md](systems/05_progression_and_endings.md) |
| Scope & non-goals | [systems/06_scope_cuts_and_non_goals.md](systems/06_scope_cuts_and_non_goals.md) |

---

## Game & world

| Topic | Document |
|-------|----------|
| Core loop | [game/00_core_loop.md](game/00_core_loop.md) |
| Pillars | [game/01_core_pillars.md](game/01_core_pillars.md) |
| Gameplay loop | [game/02_gameplay_loop.md](game/02_gameplay_loop.md) |
| Player experience | [game/03_player_experience.md](game/03_player_experience.md) |
| Premise & lore | [world/00_premise_and_lore_boundaries.md](world/00_premise_and_lore_boundaries.md) |
| Veil Heart character | [world/01_veil_heart_character.md](world/01_veil_heart_character.md) |

---

## Art, production, reference

- [art/](art/) — Visual tone, direction, technical art
- [production/](production/) — Version strategy, asset rules, scope
- [reference/](reference/) — Differentiation, visual reference
- [questions/](questions/) — Open design questions

---

## Source code map

```
Source/Gloamstead/
├── Data/           NightConsequenceTypes, VeilHeartWarningTypes, RitualTypes, RitualDefinition
├── PCG/            GloamsteadPCGSubsystem
├── Components/     RitualPlacementComponent
└── Systems/        GloamsteadDayNightSubsystem, NightConsequenceManager,
                    NightConsequenceRuntime, VeilHeart
```

---

## Conventions

- **Design** sections describe the full game target.
- **Implementation status** sections describe what exists in the repo today (may be stubbed).
- Prefer updating system docs when promoting a new wave, then reflect summary changes in [Phase2_CoreLoop.md](Phase2_CoreLoop.md) and the root [README.md](../README.md).