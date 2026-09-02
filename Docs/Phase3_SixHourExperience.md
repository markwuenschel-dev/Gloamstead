# Phase 3 — The Six-Hour Experience

_Plan of record for turning the shipped C++ loop into a ~6-hour playable first experience._

**Status: active implementation (2026-09-02).** All six cycles now have authored runtime contracts,
each with its own place, ritual form, evidence set and second reading; the threat roster (Gatherer,
Borrowed, Bargainer, Echo) is implemented and light-vulnerable. Human playtest approval remains
planned. This is a *map and a sequence*, like `ROADMAP.md` — each item carries a trigger and an
acceptance check.

**2026-09-02 — the visibility pass.** A headless boot of `Lvl_Gloamstead` established that the loop
runs end to end on real PCG data, and that four things the player could not *see* were the actual
remaining blockers, none of which a logic test could fail:

- Cycles III–VI restored nothing. `URitualPlacementComponent`'s switch covered LanternPost and
  GardenBed and logged "no restored-actor contract" for the other four forms, so four of six cycles
  consumed their point, advanced the arc and changed nothing on screen.
  `AGloamsteadRestoredStructure` now builds all four from the shipped sanctuary kit.
- Night threats had no skeletal mesh. The runtime spawns the raw C++ class and this project puts
  mesh/AnimBP on a Blueprint child; none exists for that class, so every Gatherer, Borrowed,
  Bargainer and Echo walked, drained light and died invisibly.
- There was no HUD at all. `AGloamsteadHUD` draws cycle, phase, sanctuary light, corruption, the
  night countdown, what is abroad in the dark, and the Heart's standing sentence — on the canvas from
  C++, because a Widget Blueprint can only be authored in the editor.
- **Cycle V was unreachable.** `DA_RitualSiteCatalog` placed the bell shrine at a 2000-unit floor
  from the Heart, in a sanctuary whose nine ritual points span 603–1266 units. The site bound
  nothing, its evidence and reading choices were never placed, and one sixth of the arc could not be
  played — with the entire suite green. Floors for Cycles IV/V/VI are now 1000 apiece.
- `UNightConsequenceRuntime::DisruptNearestThreat` — the Strike verb — was fully implemented with
  zero callers. Now bound to Left Mouse.

Two design decisions are **locked** for this phase (they change the build, not just the polish):

1. **Combat model — reuse `Variant_Combat`.** Night threats are built on the repo's existing
   `ACombatEnemy` / `ACombatAIController` / Combat StateTree / EQS, but **constrained**: 1–3
   light-vulnerable enemies acting as *pressure while you cleanse or activate*, never an action game.
   The non-goals in `production/06_scope_cuts_and_non_goals.md` (no combos, no weapon trees, no horde)
   are the guardrails.
   *As implemented:* `AGloamsteadNightThreat` derives from `ACombatEnemy` — so a threat is a real
   damageable, strikeable thing and Strike costs nothing new to build — but it does **not** possess the
   Combat AI controller. These threats want restored structures rather than the player, and their whole
   behaviour is a handful of rules in `StepBehaviour`, kept in C++ so the light relationship (the part
   that ties combat to the restoration fantasy) is testable headless and lives in one readable place.
2. **Phase cadence — player-driven rest at the Heart.** Day→Dusk and Dusk→Night advance when the player
   chooses to *rest / commune* at the Heart (agency + a natural autosave point). Night→Dawn resolves
   on the night's win-condition or a fallback timer. No wall-clock day timer.

Legend for effort/owner tags: **G** = gate-checkable C++ (`gate.ps1` is the oracle) · **N** =
NeoStack/Lua-scriptable editor work (Blueprints, widgets, PCG, playtests) · **H** = human taste/art pass ·
size **S/M/L**.

---

## 1. Where we are (grounded gap analysis)

The loop's *brain* is code-complete and gate-tested (177 green tests as of 2026-09-02). The gap is
the content + integration layer.

| Layer | State |
|---|---|
| Day/night phase authority (`UGloamsteadDayNightSubsystem`) | Code complete |
| Restoration contract + placement (`URitualPlacementComponent`, `FRestorationEventPayload`) | Code complete |
| PCG backbone + spatial queries + save/load (`UGloamsteadPCGSubsystem`, `UGloamsteadSaveGame`) | Code complete |
| Night selection brain (`UNightConsequenceManager`) + Veil Heart warnings (`AVeilHeart`) | Code complete |
| Night **execution** (`UNightConsequenceRuntime`) | Per-type strategies + a spawned, visible, light-vulnerable threat roster |
| Data assets (catalogs + 6× `DA_Ritual_*`) | All six ritual forms authored, incl. `DA_Ritual_PathPoint` |
| **PCG → subsystem init** (`InitializeFromPCGComponent`) | Called by `AGloamsteadSanctuaryBootstrap`; verified live — 9 points, 5/5 sites bound |
| Level / world | `Lvl_Gloamstead` — sanctuary-kit greybox, `PCG_RitualPoints`, Heart, first-lantern anchor, foliage, sky |
| Gloamstead Blueprints (player, Heart, restored actors, threats, game mode) | Player/Heart/lantern authored; the other five restored forms and the threats are code-owned so they need no Blueprint |
| UI / UMG (HUD, whisper, journal, dawn summary, menus) | Canvas HUD + prompt + caption widgets shipped; journal, dawn-summary screen and menus still absent |
| Day/night **visuals** | `AGloamsteadSkyPresenter` blends sun/sky/fog/exposure per phase; corruption stains via `UGloamsteadCorruptionVisualizer` |
| VFX / audio / first ending | Ending seed present; VFX one Niagara system; **audio: zero assets exist** |

**That blocking fact is retired.** `AGloamsteadSanctuaryBootstrap` calls `InitializeFromPCGComponent`
from `BeginPlay`, and a headless boot of `Lvl_Gloamstead` on 2026-09-02 logged the whole chain:
warning catalog loaded and contract-satisfied for every plan, 5 ritual definitions mapped, the first
lantern re-seated onto its authored anchor, **5 of 5** authored sites bound, 15 evidence sources and
15 reading choices placed, 9 ritual points initialised at seed 42. (`ROADMAP.md` Track B, "frozen at
editor checklist step 6", is likewise no longer frozen.)

**Accelerator:** the NeoStackAI plugin + the `neostack-blueprint` / `neostack-widget` /
`neostack-umg-design` / `neostack-game-testing` skills + the `neostack-pcg-editing` recipe make most
"editor-only" work scriptable via Lua rather than human-gated.

---

## 2. The experience shape

Six day→night cycles, ~45–60 min each, escalating along the existing `ENightConsequenceType` /
`ERitualType` values. Act 1 teaches, Act 2 complicates + introduces light combat pressure, Act 3 is
mastery + climax + one ending.

The spine is spatial: each cycle opens one more part of the same small island, and the sixth opens
none — it turns everything already opened into the puzzle. Night 1 is the explicit tutorial; the five
cycles after it are the five actual interpretation tests.

| Cycle | Place | Night type (enum) | Ritual | Warning | Teaches |
|---|---|---|---|---|---|
| 1 | Corridor + Heart plaza | `Tutorial` | LanternPost | "The path has forgotten its light…" | what I restore matters |
| 2 | The garden, off the plaza | `Corruption` | GardenBed | "Wake the roots. Wet earth shelters; bare ash feeds the Gloam." | the warning holds more than the minimum |
| 3 | The broken road | `Retrieval` | PathPoint | "Give the lantern a road. Loops guard; dead ends invite hands." | geometry between restorations matters |
| 4 | The mirror overlook | `SilencePossession` | MirrorPillar | "Raise the mirror. Face stolen light; never show it the Heart." | how a restoration is *configured* matters |
| 5 | The bell shrine | `Bargain` | BellShrine | "Wake the bell. One answer frees; three answers invite company." | timing and restraint matter |
| 6 | The whole sanctuary | `Fracture` → `TrueSiege` | AnchorStone | "Bind three lights apart. A closed ring holds; a crown breaks." | read the whole sanctuary as the answer |

Cycle 3 recontextualises ground the player has already walked: on Day 1 the approach road was merely
the route to the Heart; now its broken path network is a system to repair, and it is the first night
where the thing in danger is something the player made. Cycle 4 climbs, so plaza, lantern, garden and
road are all visible at once — which is what makes "point the mirror at the right one" a readable
question. Cycle 6 deliberately reveals no new spoke of the map.

### The second reading

Every warning from Cycle II onward is one sentence in two halves: an imperative naming the minimum
restoration, then a contrastive pair naming a sharper reading and a plausible-but-wrong overreading.
The minimum gets the player through the night. The sharper reading earns an advantage. The overread
makes the night worse.

These are **not** secret collectibles. They are second-order interpretations of the same warning the
Heart already spoke aloud, backed by the same evidence, and they are authored data
(`FExperienceCycleSecondReading` on the plan) rather than bespoke quest logic:

| Cycle | Sharper reading (Insight) | Plausible overread (Overreach) |
|---|---|---|
| 2 | reopen the sluice and water the bed → `Boon.GardenAura` | empty the ash brazier over it → `Scar.AshFed` |
| 3 | close the lit path into a loop → `Boon.PathLoop` | run the road out to the abandoned gate → `Scar.DeadEnd` |
| 4 | face the mirror at the stolen light → `Boon.TetherExposed` | face it inward, at the Heart → `Scar.HeartRevealed` |
| 5 | ring once, on the answering beat → `Boon.Resonance` | ring three times to be certain → `Scar.CompanyCalled` |
| 6 | bind the far three into a closed ring → `Boon.RingHeld` | crown the Heart with all three → `Scar.CrownBroken` |

The authoring rule is enforced, not conventional: a plan either offers **no** readings or offers
exactly one Insight, exactly one Overreach, and at least one defensible Plain middle, each graded
reading carrying a unique durable tag. Without the middle, the sharper read is a coin flip between
reward and scar — which is the "random-feeling punishment" Pillar 7 exists to prevent.
`FExperienceCyclePlan::HasCoherentSecondReadings` refuses anything else, at import and at runtime.

A reading is a **configuration of a restoration the player already earned**, never a substitute for
earning it: `AVeilHeart::RecordSecondReadingInternal` refuses to record one until the plan's
interpretation receipt exists, and the verdict it mints is re-derived from the plan rather than
trusted, so a forged or stale grade cannot survive.

### The enemy roster

Four archetypes, each teaching a different relationship to restoration, and the climax recombines
three the player already knows rather than introducing a fifth.

| Archetype | Enters | What it wants | How it is answered |
|---|---|---|---|
| **The Gatherer** | Cycle 3 | walks to a restored structure and takes the light out of it; ignores the player unless obstructed | connected light slows it and enough stops it; maintain the route and force it back into the light |
| **The Borrowed** | Cycle 4 | the Gloam wearing the shape of something the sanctuary knew | Strike interrupts; a correctly faced mirror exposes the tether; **Cleanse** resolves it, and only once exposed |
| **The Bargainer** | Cycle 5 | stands at the edge of the light offering shortcuts — false prompts, false safe ground | the restored bell's resonance dismisses it |
| **The Echo** | as company | repeats what it just saw, late and in the wrong place | costs attention, not light; it drains nothing and deals nothing |

The 1–3 ceiling is enforced in code, not review: `BuildNightThreatRoster` clamps to
`FNightThreatRoster::MaxSimultaneousThreats` and drops from the *end* of the list, so an over-full
roster loses the extra manifestation rather than the enemy that teaches the lesson. Cycles 1–2 field
no threat at all. An Insight reading does **not** delete the night's threat — that would make the
sharper read a skip button; it removes the extra one and lowers the light level at which the rest are
held off, so the sanctuary the player configured does the work.

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
| **2 — Real nights** | ✅ threat roster + Corruption/Retrieval/Possession + J (catalog authoring, `DA_Ritual_PathPoint`) — remaining: C (PCG path light + Mirror/Bell branches) | Cycles 2–4 playable; threats tied to night rules |
| **3 — Full arc + climax** | ✅ Bargain + Fracture/TrueSiege strategies, BellShrine + AnchorStone rituals, second readings — remaining: ending decision seed, Journal, menus, authored geometry for the later sites | All 6 cycles + one ending, ~6 hr |
| **4 — Fair crypticism + polish** | J support-channels + validator, audio, VFX, per-night atmosphere, accessibility, naming pass, perf | Gate green + clean playtest + captions in |

---

## 6. Open decisions / risks

- **Resource model** (ROADMAP Track C #4): a night-yield/interpretation-bonus resource is *deferred* — add
  only if restoration needs a cost to feel meaningful in playtest. Not in the 6-hour critical path.
- **Endings scope:** one complete ending + one decision seed for MVP (per
  `systems/05_progression_and_endings.md`); do not let branching pull scope from the loop.
- **Naming:** ✅ **resolved 2026-06-18** — everyday "the Heart", proper name **"the Gloamheart"**, Gloam = the unremembering, Heart = wounded memory-engine; full voice guide in [`world/02_naming_and_voice_decision.md`](world/02_naming_and_voice_decision.md). G + J are unblocked.
- **Combat drift:** the locked decision reuses `Variant_Combat`; guard against it growing into an action
  game — the 1–3 enemy, light-vulnerable, "pressure not kill-count" constraint is now enforced by
  `Gloamstead.NightThreat.NoNightEverExceedsThreeSimultaneousThreats`, which is exhaustive over every
  night type × reading grade × warning-heeded combination rather than over the cases the arc happens to
  author. Review still owns whether a *new* archetype earns its place; the count no longer needs it.
- **Authored geometry for Cycles 3–6: RESOLVED 2026-09-02.** This entry read as a missing-greybox
  problem and was not one. Both landmarks the catalog names — the Heart and `BP_FirstLanternAnchor` —
  are placed in `Lvl_Gloamstead`, and a headless boot binds **5 of 5** authored sites, placing 15
  evidence sources and 15 reading choices. What actually failed was one authored distance: the bell
  shrine's 2000-unit floor named a place past the far edge of a sanctuary ~1500 units across. The
  failure was fail-closed and correct, but its diagnostic said only "widen BindRadius" — advice that
  costs a build to act on — so it now lists every unclaimed point nearest-to-farthest with its
  distance, and states that the binder takes the *nearest* point in band. Two latent instances of the
  same bug were retired at the same time: Cycle IV's 1200 floor was binding its point at 1201, on one
  unit of margin.
- **Audio is still entirely absent.** There are zero sound assets in `Content/` of any kind — no
  cues, no waves, no MetaSounds. Workstream H cannot start from inside the repo; it needs source
  material first.
- **The night threats wear the stock mannequin.** That is the body that makes the night legible
  today, and it is thematically defensible for the Gloam wearing a shape the sanctuary knew, but a
  bespoke silhouette per archetype is real art work and is still owed. Archetypes are currently told
  apart by a per-archetype coloured glow.
