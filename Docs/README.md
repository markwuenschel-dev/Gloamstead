# Gloamstead documentation

Design and implementation docs for the Gloamstead vertical slice. Markdown in this tree is the source of truth for humans and coding agents.

**Last updated:** 2026-06-18 (naming + Heart/Gloam nature + voice guide locked — see [world/02_naming_and_voice_decision.md](world/02_naming_and_voice_decision.md))

**Agent collaboration operating model**: See [agents/UE5-Agent-Substrate-Review.md](agents/UE5-Agent-Substrate-Review.md) (diagnosis + minimal roster) + `../agent_collab/` (living protocol, playbooks, policies, state). Use `agent_collab/context/workflow_activation.json` and playbooks/ for architecture/research/docs concerns instead of deprecated roles.

---

## Start here

| If you want… | Read |
|--------------|------|
| What the game is | [game/00_current_design_baseline.md](game/00_current_design_baseline.md) |
| How code is layered | [ArchitectureOverview.md](ArchitectureOverview.md) |
| What is implemented today | [Phase2_CoreLoop.md](Phase2_CoreLoop.md) |
| Agent / UE conventions + current operating model | [agents/ProjectRules.md](agents/ProjectRules.md) and [agents/UE5-Agent-Substrate-Review.md](agents/UE5-Agent-Substrate-Review.md) |
| Collaboration process (orchestrator, playbooks, policies) | `../agent_collab/` (start with context/agent_rules.md + context/workflow_activation.json) |

---

## Implementation (phases)

| Phase | Document | Status |
|-------|----------|--------|
| 0 | [Phase0_RitualData.md](Phase0_RitualData.md) | Complete |
| 1 | [Phase1_PCGSubsystem.md](Phase1_PCGSubsystem.md) | Complete |
| 1.5 | [Phase1.5_PlacementComponent.md](Phase1.5_PlacementComponent.md) | Complete |
| 2 | [Phase2_CoreLoop.md](Phase2_CoreLoop.md) | **Core loop in C++**; data assets verified in editor |
| 3 | [Phase3_SixHourExperience.md](Phase3_SixHourExperience.md) | **Planned** — sequenced build-out of the ~6-hour playable experience (wiring → 6 cycles → ending) |

---

## System design (+ implementation notes)

Each file includes an **Implementation status** section where the vertical slice has landed in code.

**Current active wave**: `wave-vs-polish-202606`. **Data asset factory + map wiring verified 2026-06-11** ([specs/data/VERIFICATION-2026-06-11.md](../specs/data/VERIFICATION-2026-06-11.md)): six `Content/Data/DA_*`, PIE catalog load + day/night cycle on `Lvl_ThirdPerson`. **Next editor gate:** PCG init for restoration smoke. Agent substrate: 4 core roles + playbooks (`../agent_collab/`).

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
| Naming, voice & Gloam (locked) | [world/02_naming_and_voice_decision.md](world/02_naming_and_voice_decision.md) |

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
├── Systems/        DayNightSubsystem, NightConsequence*, VeilHeart
├── Save/           Persistence
└── Variant_* /     Prototype-specific layers (Combat, Platforming, SideScrolling) with their own Characters, AI, Gameplay, UI
```

Core game logic lives under the main module + shared Systems/. Prototypes explore different movement/combat feels while feeding the vertical slice.

---

## Conventions

- **Design** sections describe the full game target.
- **Implementation status** sections describe what exists in the repo today (may be stubbed).
- Prefer updating system docs when promoting a new wave, then reflect summary changes in [Phase2_CoreLoop.md](Phase2_CoreLoop.md) and the root [README.md](../README.md).
- **Agent collaboration**: Follow the minimal roster and playbooks (see UE5 substrate review). Small tasks use orchestrator + coder + critic (text-only where possible). Consult `../agent_collab/context/workflow_activation.json` before involving Planner. Use playbooks/ for cross-cutting design, research, or post-promotion docs instead of old dedicated roles.
- Human verification (editor-generation, map-load, playtest) is recorded as evidence for Critic approval. Example: [specs/data/VERIFICATION-2026-06-11.md](../specs/data/VERIFICATION-2026-06-11.md).