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

## Current Status (September 2026)

The **six-cycle authored arc** runs end to end in C++: placement -> restoration -> dusk warning ->
player-brought night -> night runtime with a light-vulnerable threat roster -> dawn reflection ->
ending signal. `Lvl_Gloamstead` is the shipped map (sanctuary-kit greybox, `PCG_RitualPoints`, the
Heart, the first-lantern anchor, foliage, sky).

**Verified 2026-09-02 by a headless boot of the real map:** the sanctuary bootstraps on real PCG data
(9 points, seed 42), the warning catalog satisfies the authored contract for every plan, **5 of 5**
authored ritual sites bind, and 15 evidence sources + 15 reading choices are placed. The automation
gate is at **186 green tests**.

That same pass closed four defects that were invisible to a logic-only suite: Cycles III-VI restored
nothing visible, night threats had no body, there was no HUD, and Cycle V's authored site sat past the
edge of the map so one sixth of the game could not be played. See
[Docs/Phase3_SixHourExperience.md](Docs/Phase3_SixHourExperience.md).

**2026-09-03 — the payoff pass.** Three things the loop computed and never showed the player are now
on screen, and the automation gate is at **186 green tests**:

- **Every dawn answers its night.** `AVeilHeart::OnDawnReflectionDelegate` broadcasts a complete
  `FNightRuntimeOutcome` - objective resolved, warning heeded, second-reading grade, scar or boon
  carried forward, corruption delta - and its only consumer was a `UE_LOG`. Six nights could be
  played through without once being told whether any of them had been understood.
- **The arc ends on a screen.** `IsExperienceComplete()` had a single production reader, one line of
  small text, so finishing all six nights left the readout counting toward a seventh. The ending now
  reads the whole arc back - each night with its verdict and reading, the held/lingering/scarred
  tally, and a closing line derived from the ledger rather than from having reached it.
- **The warnings are readable.** `FVeilHeartWarningSupportChannel::EvidenceText` is documented in the
  type as player-facing journal text, is authored for every shipped warning, and was presented
  nowhere: the only production reader of `SupportChannels` was the predicate deciding whether an
  encounter counted. The journal now shows found clues in their authored words, names the medium of
  the ones still missing without spoiling what they say, and states the distinct-clue gate the cycle
  is actually enforcing.

Night length now follows what a night fields, rather than one ceiling for all six: Cycles I and II
spawn no threat at all and were spending the same budget as Cycle VI's three-threat siege.

**The loop has now been driven end to end unattended.** Every phase transition in this game is
player-gated by design, so a headless boot supplies no input and sits in Day forever - which is
indistinguishable from a hang if nothing can play it. `AGloamsteadCharacter` now carries a
`GloamAutoPlay` harness (`-GloamAutoPlay [-GloamAutoPlayBeat=1.0]`, read from `BeginPlay`, because
`-ExecCmds` runs before a pawn exists for a Character exec to route to). Its first traversal walked
five of six cycles in ~9 min: Tutorial *held*, Corruption **scarred** (bloom worsened 0.40),
Retrieval / Possession / Bargain *held*, corruption climbing 0.60 -> 0.97. The Tutorial night ended
at 61 s against its 70 s ceiling - on its objective, not on the clock. The harness cannot walk to a
ritual point, so it proves the cadence and payoff plumbing, not restoration and interpretation.

**Known gaps:** night threats wear the stock mannequin under a forged shroud, pending bespoke
silhouettes; the audio bed is synthesised in C++ because the project ships no sound assets, so real
music and a recorded whisper for the Heart are still owed; `Content/Gloamstead` ships no textures of
its own, so every sanctuary material samples the two desert terrain sets in `Content/Textures/`; and
the seven forged gloam meshes under `Content/Gloamstead/World/` are loaded by path from C++ but are
still untracked in git.

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
- After clone/adapter changes, run **both** projections — `.claude/` and `.grok/` are generated and
  gitignored, so a fresh clone has neither:
  ```
  pwsh -NoProfile -File agent_collab/scripts/Project-ClaudeAdapter.ps1
  pwsh -NoProfile -File agent_collab/scripts/Project-GrokAdapter.ps1
  ```
  The Claude one is load-bearing for safety, not convenience: `.claude/settings.json` is the only
  file that registers the `PreToolUse` Bash hook, so until it is projected every Bash command runs
  without the shell-policy guard. Restart the session afterwards.
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