# Phase 3 — The Six-Hour Experience

_Plan of record for turning the shipped C++ loop into a ~6-hour playable first experience._

**Status: planned (2026-06-18).** This is a *map and a sequence*, like `ROADMAP.md` — each item carries a
trigger and an acceptance check. It assumes the Phase 0–2 systems on `main` and does not re-spec them.

Two design decisions are **locked** for this phase (they change the build, not just the polish):

1. **Combat model — reuse `Variant_Combat`.** Night threats are built on the repo's existing
   `ACombatEnemy` / `ACombatAIController` / Combat StateTree / EQS, but **constrained**: 1–3
   light-vulnerable enemies acting as *pressure while you cleanse or activate*, never an action game.
   The non-goals in `production/06_scope_cuts_and_non_goals.md` (no combos, no weapon trees, no horde)
   are the guardrails.
2. **Phase cadence — player-driven rest at the Heart.** Day→Dusk and Dusk→Night advance when the player
   chooses to *rest / commune* at the Heart (agency + a natural autosave point). Night→Dawn resolves
   on the night's win-condition or a fallback timer. No wall-clock day timer.

Legend for effort/owner tags: **G** = gate-checkable C++ (`gate.ps1` is the oracle) · **N** =
NeoStack/Lua-scriptable editor work (Blueprints, widgets, PCG, playtests) · **H** = human taste/art pass ·
size **S/M/L**.

---

## 1. Where we are (grounded gap analysis)

The loop's *brain* is code-complete and gate-tested (16 green tests). The gap is the content +
integration layer.

| Layer | State |
|---|---|
| Day/night phase authority (`UGloamsteadDayNightSubsystem`) | Code complete |
| Restoration contract + placement (`URitualPlacementComponent`, `FRestorationEventPayload`) | Code complete |
| PCG backbone + spatial queries + save/load (`UGloamsteadPCGSubsystem`, `UGloamsteadSaveGame`) | Code complete |
| Night selection brain (`UNightConsequenceManager`) + Veil Heart warnings (`AVeilHeart`) | Code complete |
| Night **execution** (`UNightConsequenceRuntime`) | **Stub** — logs + corruption spread; no threats |
| Data assets (catalogs + 4× `DA_Ritual_*`) | Exist but MVP-thin; **`DA_Ritual_PathPoint` missing** |
| **PCG → subsystem init** (`InitializeFromPCGComponent`) | **Never called by anything** → loop is starved |
| Level / world | Stock `Lvl_ThirdPerson` template |
| Gloamstead Blueprints (player, Heart, restored actors, threats, game mode) | None |
| UI / UMG (HUD, whisper, journal, dawn summary, menus) | None |
| Day/night **visuals** (DaySequence plugin enabled) | Unused in code |
| VFX / audio / first ending | None |

**The single blocking fact:** nothing calls `InitializeFromPCGComponent`, so the loop does not run in PIE.
This is the critical path; everything visible depends on it. (See `ROADMAP.md` Track B — "frozen at editor
checklist step 6.")

**Accelerator:** the NeoStackAI plugin + the `neostack-blueprint` / `neostack-widget` /
`neostack-umg-design` / `neostack-game-testing` skills + the `neostack-pcg-editing` recipe make most
"editor-only" work scriptable via Lua rather than human-gated.

---

## 2. The experience shape

Six day→night cycles, ~45–60 min each, escalating along the existing `ENightConsequenceType` /
`ERitualType` values. Act 1 teaches, Act 2 complicates + introduces light combat pressure, Act 3 is
mastery + climax + one ending.

| Cycle | Night type (enum) | Ritual unlocked | Teaches |
|---|---|---|---|
| 1 | `Tutorial` | LanternPost | action → prepare → consequence → dawn (cause & effect) |
| 2 | `Corruption` | GardenBed | neglect spreads; restore to hold ground |
| 3 | `Omen` → `Retrieval` | PathPoint (light propagation) | warnings point to *places*; a restored thing is targeted |
| 4 | `SilencePossession` | MirrorPillar | a restored thing turns; disrupt → purify; **combat pressure enters** |
| 5 | `Mirror` → `Bargain` | BellShrine | interpretation under temptation; call / repel |
| 6 | `Fracture` → `TrueSiege` (climax) | — | mastery test + **ending decision seed** at the final dawn |

This matches the "discoverable adaptation" curve in `game/02_gameplay_loop.md` (Night 1 = clear cause/effect;
Nights 2–3 = neglect/wrong restoration changes pressure; later = darkness responds to patterns).

---

## 3. Workstreams

### A. Make the loop run (critical path) — G + N
- **A1. `AGloamsteadSanctuaryBootstrap`** (new C++ actor placed in the level): holds a `UPCGComponent*`
  reference, waits for the `GenerateOnLoad` graph to finish, then calls
  `InitializeFromPCGComponent(comp, WorldSeed)`. Retires Track B and unlocks the end-to-end integration
  test the ROADMAP wants. **S, G.**
- **A2. `UGloamsteadDayNightDirector`** (component or world subsystem): owns the rest-driven cadence,
  calls `AdvanceToNextPhase`, and fans out per phase — Dusk → `UNightConsequenceManager::PrepareNightConsequences`
  + Veil Heart warning emit; Night → `UNightConsequenceRuntime::BeginNight`; Dawn →
  `AVeilHeart::ProcessDawnReflection` + autosave. Today nothing orchestrates these together. **M, G.**
- **A3. Fix latent display-name bug:** `GetNightConsequenceTypeDisplayName` / `GetRitualTypeDisplayName`
  only cover the 3 MVP types; all others (incl. Mirror/Bell) return `"Invalid"`, which breaks the Veil
  Heart ritual-name warning fallback. Extend to the full enums. **S, G.** (Flagged in ROADMAP Track A gaps.)

**Acceptance:** PIE proves restore → rest → dusk warning → night → dawn reflection on *real* PCG point
data; the headless integration test Track B has been waiting for goes green in `gate.ps1`.

### B. Player, input, interaction verbs — N + G
- `BP_GloamsteadCharacter` (from the stock third-person pawn) hosting `URitualPlacementComponent` with
  input bindings; `BP_GloamsteadPlayerController`; concrete `BP_GloamsteadGameMode` (subclass the abstract
  `AGloamsteadGameMode`, set default pawn/controller/HUD).
- Verbs from `systems/04_combat_and_interaction_system.md`: **Restore/Interact** (E, drives
  `EnterPlacementMode`/`ConfirmPlacement`), **Examine/Focus** (inspect mode), **Activate**,
  **Cleanse/Ward** (RMB), light **Strike** (LMB, used sparingly at night).
- **`UGloamInteractionComponent` + `IGloamInteractable`** interface (new C++; mirror the
  `Variant_SideScrolling` interactable pattern) for examine/activate on world objects. **M, G.**

### C. World, level & PCG — N + H
- New **`Lvl_Gloamstead`** map: one contained sanctuary island — central Heart plaza, ruined radial
  paths, lantern posts, garden beds, a mirror-pillar overlook, a bell shrine. Greybox with Modeling Tools
  first (`ModelingToolsEditorMode` is enabled), art pass in Phase 4.
- **Expand `PCG_RitualPoints`** from 9 fixed points to the full sanctuary: scatter across the real play
  space, add **MirrorPillar(4)** and **BellShrine(5)** branches, author `PathSegmentID` / `PathPosition`
  so the subsystem's path-light propagation has real data, region-partition for density. Follow the
  `neostack-pcg-editing` memo: `invoke()` / `AddNodeOfType`, the Output node's pin is `"Out"`, set
  `AttributeTypes.Type` explicitly per attribute, keep `bIsComponentPartitioned=false` so
  `GetGeneratedGraphOutput` works. **M–L, N.**

### D. Restored-actor & preview Blueprints + restoration "language" — N + H
- `BP_Restored_LanternPost` / `_GardenBed` / `_PathLight` / `_MirrorPillar` / `_BellShrine`, spawned via
  the placement component's `SpawnRestoredActor` BlueprintImplementableEvent, each with a ruined→restored
  transition. Light/garden contribute to the sanctuary light average the night brain reads.
- `BP_RitualPreview` ghost/reticle (valid/invalid feedback, hooked to `OnPreviewTargetChanged`).
- `BP_VeilHeart` (subclass `AVeilHeart`): the emotional center — glow keyed to clarity / satisfied warning
  tags, implements `OnWarningEmitted`, hosts the **rest** interaction that drives cadence (A2).

### E. Day/night atmosphere — N + H
- Wire `DaySequence` to phases: a `DaySequenceActor` whose time is set from
  `UGloamsteadDayNightSubsystem::OnPhaseChanged` (use `GetNormalizedTimeOfDay`). Per-phase
  post-process/fog/lighting per `game/02_gameplay_loop.md`: day cold & legible, dusk interpretive tension,
  **night authored per night-type** (crawling corruption, swallowed paths, mirrored silhouettes, dead
  silence, fractured space, rare siege), dawn warm release. **M, N+H.**

### F. Night runtime — turn the stub into real consequence — G + N (biggest code gap)
- Replace `UNightConsequenceRuntime::ExecuteNightStub` with a **`UNightThreatDirector`** that dispatches
  per `ENightConsequenceType`.
- **Reuse `Variant_Combat`** (`ACombatEnemy`, `ACombatAIController`, Combat StateTree,
  `EnvQueryContext_Player/Danger`, `ACombatEnemySpawner`) as the threat foundation. Add a thin
  Gloamstead threat subclass that is **light-vulnerable** (weak/cleansable inside restored lantern radius —
  ties threats to the restoration fantasy).
- Per-type behaviour: `Corruption` (spread + cleansable nodes), `Retrieval` (enemy targets a restored
  object), `SilencePossession` (a restored actor turns hostile → disrupt then purify), `Mirror`
  (threat only revealed via mirror pillar), `Bargain` (temptation/choice), `Fracture`, `TrueSiege`
  (rare direct assault climax).
- **Night win/lose:** define success (timer survived / objective cleansed) and a *fail-forward* loss
  (Heart corruption maxes → setback + scar, not a hard game-over). **L, G+N.**

### G. UI / UMG — N + H (`neostack-umg-design`)
- `WBP_HUD` — phase, night count, sanctuary light/corruption meters, interaction prompt.
- `WBP_VeilHeartWhisper` — the cryptic fragment, atmospheric typography. **With captions (see K).**
- `WBP_Journal` — logged warnings + clarity tiers, restored structures, omen clues; the interpretation aid.
- `WBP_DawnSummary` — the payoff screen (understood / misread / changed / earned), satisfying the
  `game/00_core_loop.md` rule that every dawn answers ≥1 question.
- `WBP_MainMenu`, `WBP_PauseMenu`, `WBP_Settings`.

### H. Audio — H
- Heart whisper voice (the warnings), day/dusk/night/dawn ambient beds, restoration stinger, dawn release,
  a distinct sound signature per night type.

### I. Progression, save, ending — G + N
- Wire the **already-built** persistence into the loop: `SaveToSlot` autosave at each dawn (and on rest),
  `LoadFromSlot` on level start. It is fully implemented (`Gloamstead.PCG.SaveGameFullRoundTrip` green) but
  currently unused by gameplay.
- One complete ending + one decision seed at the Night-6 dawn, reading variables already tracked: Heart
  clarity (`GetSatisfiedWarningTagCount`), sanctuary light/corruption averages, restored counts.

### J. Content authoring + Fair Crypticism — H + G
- Author all 9 night rules into `DA_NightConsequenceCatalog`; a warning per night type into
  `DA_VeilHeartWarningCatalog`; add the missing **`DA_Ritual_PathPoint`**.
- **Fair Crypticism (ROADMAP Track D):** extend `FVeilHeartWarningFragment` with support-channel fields
  (environmental clue, prior pattern, restored-object reaction, audio cue, enemy behaviour, readable
  consequence, dawn feedback), then a `UEditorValidatorBase` validator (GloamsteadEditor module) that
  fails any warning with < 2 non-empty channels. This makes the "cryptic but never false" promise
  enforceable. **M, G.**

### K. Not on the original list, required for a complete experience
- **Captions/subtitles for the Heart's whispers** — non-negotiable; the whole game is interpreting what it
  *says*. Accessibility-critical and load-bearing.
- **No clue encoded in colour alone** (colourblind fairness) + a **brightness/gamma calibration** step
  (the game is deliberately dark).
- **Main menu / pause / settings / new-game vs continue**, input remap.
- **Death/failure recovery** loop — interpretation games must fail forward, not dead-end.
- **Onboarding first 5 minutes** + contextual control prompts.
- **Naming pass** — ✅ done ([`world/02_naming_and_voice_decision.md`](world/02_naming_and_voice_decision.md)): everyday "the Heart", proper "the Gloamheart", Gloam = the unremembering, + voice guide. ("Veil Heart"/`VeilHeart` stays the code term.)
- **Performance budget** — Lumen + PCG + Niagara at night. Remember full builds need
  `-MaxParallelActions=6` or UBA kill-loops on RAM (see build memory).
- **`gate.ps1` stays the oracle** for every C++ workstream; editor/content work gets a `neostack-game-testing`
  playtest pass instead.

---

## 4. New C++ classes (summary)

| Class | Module | Purpose |
|---|---|---|
| `AGloamsteadSanctuaryBootstrap` | Gloamstead | Drives `InitializeFromPCGComponent` after PCG generates (A1) |
| `UGloamsteadDayNightDirector` | Gloamstead | Rest-driven cadence + per-phase orchestration (A2) |
| `UGloamInteractionComponent` + `IGloamInteractable` | Gloamstead | Examine/activate verbs (B) |
| `UNightThreatDirector` | Gloamstead | Per-night-type threat dispatch over reused Combat tech (F) |
| Gloamstead threat subclass of `ACombatEnemy` | Gloamstead | Light-vulnerable night threat (F) |
| Display-name table extension | Gloamstead | Cover full `ERitualType` / `ENightConsequenceType` (A3) |
| `FVeilHeartWarningFragment` support-channel fields + validator | Gloamstead / GloamsteadEditor | Fair Crypticism (J) |

---

## 5. Implementation order

| Phase | Scope | Acceptance |
|---|---|---|
| **0 — Make it run** | A1 bootstrap + A2 director + A3 display-name fix | End-to-end loop runs on real PCG data in PIE; Track B integration test green in `gate.ps1` |
| **1 — One playable cycle** | B (player+verbs) + D (lantern restored + preview + `BP_VeilHeart`) + minimal G (HUD + whisper + dawn summary) + E (basic day/night) + I (autosave at dawn) | Cycle 1 (Tutorial night) fully playable to dawn |
| **2 — Real nights** | F (threat director on Combat tech) + Corruption/Retrieval/Possession + C (PCG: path light + Mirror/Bell branches) + J (catalog authoring + `DA_Ritual_PathPoint`) | Cycles 2–4 playable; threats tied to night rules |
| **3 — Full arc + climax** | Mirror/Bargain/Fracture/TrueSiege + BellShrine + ending decision seed + Journal + menus | All 6 cycles + one ending, ~6 hr |
| **4 — Fair crypticism + polish** | J support-channels + validator, audio, VFX, per-night atmosphere, accessibility, naming pass, perf | Gate green + clean playtest + captions in |

---

## 6. Open decisions / risks

- **Resource model** (ROADMAP Track C #4): a night-yield/interpretation-bonus resource is *deferred* — add
  only if restoration needs a cost to feel meaningful in playtest. Not in the 6-hour critical path.
- **Endings scope:** one complete ending + one decision seed for MVP (per
  `systems/05_progression_and_endings.md`); do not let branching pull scope from the loop.
- **Naming:** ✅ **resolved 2026-06-18** — everyday "the Heart", proper name **"the Gloamheart"**, Gloam = the unremembering, Heart = wounded memory-engine; full voice guide in [`world/02_naming_and_voice_decision.md`](world/02_naming_and_voice_decision.md). G + J are unblocked.
- **Combat drift:** the locked decision reuses `Variant_Combat`; guard against it growing into an action
  game — enforce the 1–3 enemy, light-vulnerable, "pressure not kill-count" constraint in review.
