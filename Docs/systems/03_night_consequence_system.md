# Night Consequence System

_Nights as varied supernatural tests rather than horde waves._

## Purpose

Night is the consequence phase. It tests what the player restored, neglected, misunderstood, or disturbed.

Night is not the default horde-defense phase.

## Night Types

### Tutorial Night

A structured first night that teaches the grammar of the game:

- The player accidentally awakens the Heart.
- Heart gives a cryptic but fair warning.
- Environment contains clear clues.
- Player restores one relevant fixed-location structure.
- Dusk gives time to prepare around that interpretation.
- Night consequence is manageable.
- Dawn proves the warning had meaning.

The first night should teach action -> preparation -> consequence -> dawn feedback. It does not need to fully explain deeper adaptation.

### Corruption Night

Darkness infects structures or ground. The player must cleanse, isolate, or use the right restored structures to slow spread.

### Omen Night

A warning points toward a specific rule. Correct interpretation provides a major advantage. Misinterpretation creates a painful but fair surprise.

### Retrieval Night

The darkness tries to reclaim a specific restored object or recovered relic.

### Silence/Possession Night

No obvious enemies at first. The threat begins inside the sanctuary: a structure, object, or golem-like helper turns against the player.

### Mirror Night

The darkness copies, reflects, or distorts something the player created.

### Bargain Night

The darkness offers a tempting advantage with a long-term cost.

### Fracture Night

The rules of space, paths, or sanctuary boundaries temporarily break.

### True Siege Night

A rare direct assault. This is allowed, but should be special rather than the default loop.

## Design Rule

For each night, define:

- warning fragment
- environmental clues
- player preparation options
- consequence mechanic
- simple combat/cleansing role
- failure result
- dawn feedback
- what the player learns

## Avoid

- endless waves as the main content
- DPS checks
- enemy spam
- tower defense lane logic
- random punishment with no interpretive path


## Night Visual Languages

Each night type should have a distinct atmospheric and material behavior. This helps players learn the rules of the world instead of reading every night as generic darkness.

### Corruption Night

Visual language: crawling stains, blackened veins, ground-darkening, rotting bloom, dimming restoration edges. The player should read spread direction and affected structures clearly.

### Omen Night

Visual language: subtle foreshadowing, unusual light alignment, repeated silhouettes, distant motion, environmental objects reacting before danger arrives. The threat should feel interpretable in hindsight.

### Retrieval Night

Visual language: tendrils, grasping shadows, cold wind toward the targeted object, light being pulled away from the sanctuary. The target should be unmistakable once the night begins.

### Silence / Possession Night

Visual language: lack of motion, deadened audio, too-still restored objects, inverted glow, internal cracks, delayed reactions. The threat begins inside the sanctuary, so the visuals should feel intimate and wrong.

### Mirror Night

Visual language: doubled silhouettes, broken reflections, surfaces showing impossible angles, false safe zones, copied restoration motifs. Readability is essential; confusion should be eerie, not unfair.

### Bargain Night

Visual language: beautiful but suspicious warmth, over-saturated glow, false sanctuary comfort, seductive clarity, gold that feels slightly diseased. The temptation should look useful before it looks dangerous.

### Fracture Night

Visual language: spatial discontinuity, misaligned paths, memory afterimages, drifting architecture edges, fog that cuts space into uncertain layers. Use the Liminal Memory layer of the stylization most strongly here.

### True Siege Night

Visual language: direct pressure, harsher silhouettes, aggressive motion, damaged light, physical threat. This should feel exceptional, not the default aesthetic of night.

## Night Readability Rule

Atmosphere may obscure comfort, but it must not obscure necessary interpretation.

Every night should preserve:

- clear player navigation
- readable threat silhouettes
- visible restoration states
- distinguishable corruption effects
- legible consequence feedback

If the player fails, the visual sequence should help them understand why.


## Adaptation Rollout

Night adaptation should be introduced gradually.

### Tutorial Level

The player sees that a daytime action and dusk preparation affect the night. This is the minimum lesson.

### Early Game

The player sees that wrong restoration, neglected clues, or ignored structures can alter the next night. This teaches that the world responds to misunderstanding, not just success.

### Mid Game

The player begins to recognize patterns: the darkness responds to what was restored, altered, undone, overused, neglected, or misunderstood.

## Night Resource Reward

Successful resistance against the night should grant a baseline resource. This protects progression from becoming too brittle if the player misses a discovery or puzzle path.

Resource rules:

- surviving the night grants a minimum reward
- stronger understanding can improve the reward
- failure or corruption can taint, reduce, or complicate the reward
- the baseline reward should help the player recover, not replace interpretation as the main path forward

## Simple Failure State

The simplest current failure state is:

- the Heart becomes corrupted or destroyed
- sanctuary light fails
- the player dies or is consumed by the dark

Because the game is now third-person, player death can be direct and embodied. Heart failure should still feel central: the player is not only losing health, but losing the last stable light in the place.

## Implementation status (August 2026 — authored cycles 1–4)

The runtime now executes Tutorial, Corruption, Omen, Retrieval, and the authored Silence Possession
slice. Possession is a bounded light-ward consequence on the exact restored authored target; it does
not introduce a generic wave or horde loop. Mirror, Bargain, Fracture, and True Siege remain planned
extensions, and the full presentation/human-playtest bar is still outstanding.

### NC-001 — Data types (`Source/Gloamstead/Data/NightConsequenceTypes.*`)

- **`ENightConsequenceType`**: `Tutorial`, `Corruption`, `Omen` (plus hidden `Invalid`). Matches the three MVP night types in this doc; other night types remain design-only.
- **`FNightSanctuarySnapshot`**: aggregates used for rule scoring (`AverageLightLevel`, `AverageCorruptionLevel`, `RestoredPointCount`, per-ritual restored counts).
- **`FNightConsequenceRule`**: designer row with `NightType`, `Weight`, light/corruption min–max bands, and `FavoredRitualTypes`.
- **`UNightConsequenceCatalog`**: `UPrimaryDataAsset` with `Rules`, `FallbackNightType`, and `bForceTutorialOnFirstNight`.
- **`GetNightConsequenceTypeDisplayName`**: logging/UI helper for the enum.

### NC-002 — Sanctuary snapshot (`UGloamsteadPCGSubsystem`)

BlueprintPure read-only getters (safe `0` when uninitialized):

- `GetSanctuaryAverageLightLevel` / `GetSanctuaryAverageCorruptionLevel`
- `GetRestoredPointCount` / `GetRestoredCountByRitualType`
- **`BuildSanctuarySnapshot`**: fills `FNightSanctuarySnapshot` from the getters above.

`ApplyRestoration` behavior is unchanged.

### NC-003 — Night selection (`UNightConsequenceManager`)

- Subscribes to **`OnStructureRestored`** (path-segment light coverage tracked for future use).
- **`PrepareNightConsequences`**: `BuildSanctuarySnapshot` → `SelectNightTypeFromCatalog` → stores `LastSelectedNightType`, increments `NightsPrepared`, logs, broadcasts **`OnNightPlanReady`**.
- **`ScoreRule`**: filters by snapshot light/corruption bands; adds weight from favored ritual restore counts.
- First night: **`bForceTutorialOnFirstNight`** (manager or catalog) forces **`Tutorial`** when `NightsPrepared == 0`.
- Missing/empty catalog: hard fallback **`Corruption`**; no matching rules: catalog **`FallbackNightType`**.

Assign a **`UNightConsequenceCatalog`** asset on the manager (or defaults) before calling prep from gameplay.

### NC-2 — Day/night loop (wave **night-consequence NC-2**, tasks NC-004–NC-006)

- **`UGloamsteadDayNightSubsystem`** (`Source/Gloamstead/Systems/GloamsteadDayNightSubsystem.*`): phases `Day → Dusk → Night → Dawn`. BlueprintCallable **`AdvanceToNextPhase`** / **`SetPhase`**. **`GetNormalizedTimeOfDay`** (0 dawn, 1 dusk) and **`GetNightCount`** (increments when leaving Dawn).
- **Dusk**: calls **`UNightConsequenceManager::PrepareNightConsequences`**.
- **Dawn**: finds first **`AVeilHeart`** in the world and calls **`ProcessDawnReflection`**.
- **`URitualPlacementComponent`**: restoration payload fills **`TimeOfDayAtRestoration`** / **`NightCountAtRestoration`** from DayNight when the subsystem exists.
- **`AVeilHeart`**: tracks satisfied warning tags from restorations; dawn reflection logs the count, then clears tags for the next cycle. **`GetSatisfiedWarningTagCount`** for Blueprint.

**PIE / editor**: advance phases via Level Blueprint **`Advance Gloamstead Day Phase`** (`UGloamsteadBlueprintLibrary`) or **`AdvanceToNextPhase`** on the day/night subsystem. **`UNightConsequenceManager`** auto-loads **`DA_NightConsequenceCatalog`** from `/Game/Data/` when present (verified 2026-06-11 on `Lvl_ThirdPerson` — [specs/data/VERIFICATION-2026-06-11.md](../../specs/data/VERIFICATION-2026-06-11.md)). Without PCG, sanctuary snapshot stays zero and **Tutorial** wins catalog scoring every dusk; non-Tutorial nights and catalog dusk warnings require PCG init + optional Tutorial warning row in `DA_VeilHeartWarningCatalog`.

### NC-3 — Night execution bridge (wave **night-consequence NC-3**, tasks NC-007–NC-009)

- **`ApplyCorruptionSpread`** on `UGloamsteadPCGSubsystem`: bounded random point corruption bump (does not touch restoration flags).
- **`UNightConsequenceRuntime`**: caches plan from **`OnNightPlanReady`**; **`BeginNight`** / **`EndNight`** with **`OnNightStarted`** / **`OnNightEnded`**.
- **DayNight**: entering **Night** calls **`BeginNight`** + type stub; entering **Dawn** calls **`EndNight`** before Veil Heart reflection.
- **Stubs**: Corruption applies spread (log avg before/after); Tutorial/Omen log-only until wave-nc-4.

### NC-4 — Catalog defaults and type stubs (wave **night-consequence NC-4**, NC-010–NC-012)

- **`PopulateMVPNightConsequenceRules`**: built-in Tutorial / Corruption / Omen rules when no catalog DA is assigned on the manager.
- **`FNightConsequenceRule::OmenClueTag`**: catalog-driven omen hint; **`OnOmenClueReady`** on **`UNightConsequenceRuntime`** at omen night start.
- **Tutorial night**: **`ApplyCorruptionSpread(0.06, 4)`** (half of corruption night) for a gentler first teaching beat.
- Editor can still assign a **`UNightConsequenceCatalog`** asset to override defaults.

### Deferred (post NC-4)

- **Spawning / combat / VFX** per night visual language.
- **Full night-type catalog** in code (Retrieval, Silence/Possession, Mirror, Bargain, Fracture, True Siege) and designer assets per type.
- **Dawn feedback** beyond tag clear + log (journal, resources, adaptation rollout).

## Data Asset Authoring Guide for Designers (Complete Specs)

This section provides the full, designer-facing specification for the data assets used in the Night Consequence and Ritual systems. These are implemented as UPrimaryDataAsset subclasses in C++ (see Source/Gloamstead/Data/*.h) and can be authored/assigned in the Unreal Editor without code changes.

All fields are exposed for tuning. Use Data Assets (not hard-coded values) for production to allow iteration without recompiles.

### UNightConsequenceCatalog (for Night Selection at Dusk)

**Purpose**: Defines the rules for choosing which night type occurs based on the current sanctuary state (light/corruption levels from restorations).

**Location**: Content/Data/ (e.g. DA_NightConsequenceCatalog)

**Key Fields** (from UNightConsequenceCatalog and FNightConsequenceRule):

- **Rules** (TArray<FNightConsequenceRule>): List of possible night rules. Each rule:
  - NightType (ENightConsequenceType): Tutorial, Corruption, Omen, Retrieval, Silence/Possession, Mirror, Bargain, Fracture, TrueSiege (extend enum as needed).
  - Weight (float): Base probability weight for this rule (higher = more likely).
  - MinAverageLight / MaxAverageLight (float 0-1): Light level band this rule can trigger in (from PCG snapshot).
  - MinAverageCorruption / MaxAverageCorruption (float 0-1): Corruption band.
  - FavoredRitualTypes (TArray<ERitualType>): If player has restored any of these, boost this rule's score (e.g. LanternPost favors Corruption).
  - OmenClueTag (FName): For Omen nights, the tag broadcast via OnOmenClueReady (e.g. "GardenRot"). Player must interpret to gain advantage.
- **FallbackNightType** (ENightConsequenceType): Default if no rules match (usually Corruption).
- **bForceTutorialOnFirstNight** (bool): If true, first night (NightsPrepared == 0) is always Tutorial, regardless of rules.

**Tuning Guidelines**:
- Start with low weights for complex nights (e.g. TrueSiege weight 1-2).
- Use light/corruption bands to make nights respond to player actions (high restoration = different nights).
- Favored rituals create "I restored the right thing" payoff.
- For new nights (e.g. Mirror): add rule with appropriate bands and OmenClue if omen-like.
- Test in PIE: assign catalog to UNightConsequenceManager, call AdvanceToNextPhase at dusk, check logs for selected type.

**Example Asset (pseudo for DA_NightConsequenceCatalog)**:
- Rules: [Tutorial (weight 10, any light/corruption), Corruption (weight 5, low light high corruption, favored LanternPost), Omen (weight 4, med light low corruption, favored GardenBed, OmenClue "GardenRot"), ... add Retrieval etc. for Phase 2+]
- Fallback: Corruption
- bForceTutorialOnFirstNight: true

See also: PopulateMVPNightConsequenceRules() in code for MVP defaults when no asset assigned.

See also: Content/Data/DA_NightConsequenceCatalog (generated via specs/data/vs-polish-starter.json per VS-POLISH-FACTORY-DATA-01).

### UVeilHeartWarningCatalog (for Dusk Warnings and Dawn Reflection)

**Purpose**: Catalog of cryptic warnings the Veil Heart emits at dusk. Player restorations "satisfy" tags to clear them at dawn. Ties restoration to night choice.

**Location**: Content/Data/ (e.g. DA_VeilHeartWarningCatalog)

**Key Fields** (from FVeilHeartWarningFragment in UVeilHeartWarningCatalog):

- **Warnings** (TArray<FVeilHeartWarningFragment>):
  - WarningId (FName): Unique ID for the warning (e.g. "PathBlocked").
  - Fragment (FText): The cryptic text shown to player (poetic, not literal).
  - AssociatedNightType (ENightConsequenceType): Which night this warning foreshadows (e.g. Omen for this fragment).
  - SatisfiableTags (TArray<FName>): Tags from restoration payload that satisfy this (e.g. "LightPath", ritual name fallback).
  - ClarityTier (int): Higher = more advanced warning, requires more restorations to understand/satisfy.
- No other top-level fields; the catalog is just the list.

**Tuning Guidelines**:
- Fragments should be mysterious but fair (player can match via environment or prior nights).
- Link to night types: a warning for "Corruption" night might have tags like "GardenBed" or "LightSource".
- For new rituals (MirrorPillar, BellShrine): add entries with their SatisfiableWarningTags (e.g. "Reflection", "Resonance").
- ClarityTier: 0 for tutorial, 1-3 for mid/late.
- In code: AVeilHeart uses EvaluateRestorationAgainstWarnings() with payload's WarningTagSatisfied.
- Editor: assign to AVeilHeart in level. Test by restoring and advancing to dusk/dawn.

**Example Asset**:
- Warnings:
  - {WarningId="PathBlocked", Fragment="The way is veiled until the light returns.", AssociatedNightType=Omen, SatisfiableTags=["LightPath", "LanternPost"], ClarityTier=0}
  - {WarningId="GardenRot", Fragment="What grows in darkness must be tended before the bell tolls.", AssociatedNightType=Corruption, SatisfiableTags=["GardenBed"], ClarityTier=1}
  - Add for MirrorPillar: Fragment about "reflections that reveal the hidden", tags ["MirrorPillar", "Reflection"].

See VeilHeart.cpp/h for usage (EmitWarningForNight at dusk, ProcessDawnReflection at dawn).

See also: Content/Data/DA_VeilHeartWarningCatalog (generated via specs/data/vs-polish-starter.json per VS-POLISH-FACTORY-DATA-01).

### URitualDefinition (Base for Ritual Data Assets)

**Purpose**: Per-ritual tuning data. Designers create concrete Data Assets (one per ERitualType) and assign to URitualPlacementComponent.

**Location**: Content/Data/ (e.g. DA_Ritual_LanternPost, DA_Ritual_MirrorPillar, DA_Ritual_BellShrine)

**Key Fields** (from URitualDefinition base):

- RitualType (ERitualType): Matches the enum (LanternPost, GardenBed, PathPoint, MirrorPillar, BellShrine).
- DefaultLightContribution (float): Light added on restoration (e.g. 0.35 for lantern).
- DefaultCorruptionClearance (float): Corruption removed (e.g. 0.2).
- RestorationRadius (float): Area affected (e.g. 800 units).
- SatisfiableWarningTags (TArray<FName>): Tags this ritual can satisfy for Veil Heart warnings (links to catalog).

**Tuning Guidelines**:
- Balance light vs corruption based on night rules (e.g. lanterns good vs corruption).
- New types:
  - MirrorPillar: High light, low corruption clear, tags for "reflection" warnings. Radius for revealing hidden.
  - BellShrine: Medium values, tags for "resonance" or omen. Affects night selection (favors certain types).
- Assign map in URitualPlacementComponent (TMap<ERitualType, URitualDefinition*>).
- In payload: RitualType drives the rest.
- Editor: Create assets inheriting URitualDefinition, set values, assign in placement component or PCG.
- Test: Place, restore, check PCG snapshot and warnings.

**Example Assets (design specs)**:
- DA_Ritual_LanternPost: RitualType=LanternPost, Light=0.4, CorruptionClear=0.15, Radius=600, Tags=["LightPath"]
- DA_Ritual_GardenBed: GardenBed, Light=0.25, Corruption=0.3, Radius=500, Tags=["Garden", "Growth"]
- DA_Ritual_MirrorPillar (new): MirrorPillar, Light=0.3, Corruption=0.1, Radius=400, Tags=["Reflection", "Hidden"]
- DA_Ritual_BellShrine (new): BellShrine, Light=0.2, Corruption=0.25, Radius=700, Tags=["Resonance", "Call"]

See RitualPlacementComponent for GetRitualDefinitionForType and payload construction. Update enum in RitualTypes.h when adding types.

See also: Content/Data/DA_Ritual_* (generated via specs/data/vs-polish-starter.json per VS-POLISH-FACTORY-DATA-01).

### General Authoring Workflow
1. Update enum in Source/Gloamstead/Data/RitualTypes.h (and NightConsequenceTypes.h for new nights) if adding types.
2. Create concrete UPrimaryDataAsset in Content/Data/ inheriting the base/catalog.
3. Fill fields per specs above, matching C++ structs.
4. Assign in relevant actors (AVeilHeart for warnings, UNightConsequenceManager for nights, URitualPlacementComponent for rituals).
5. Use in PIE: advance phases, observe logs/UI, tune values.
6. For factory/adapters: manifests can reference these assets for generated content.

This ensures the "I restored the right place" payoff through data-driven rules. Keep fragments cryptic but linked to tags/environment.

See also: ArchitectureOverview.md for data flow, Phase2_CoreLoop.md for current stubs, production/01_asset_and_tech_rules.md for UE5 asset strategy.

**Implementation Status Note**: As of VS-POLISH-DATA-01 + promotion (2026-06-05), enums have basic + new types (MirrorPillar, BellShrine; full Retrieval+); catalogs have MVP + full fields documented for Phase 2+ designer use (see "Data Asset Authoring Guide for Designers (Complete Specs)" above for NightConsequenceCatalog, VeilHeartWarningCatalog, RitualDefinition with examples and exact tuning specs). 

The 3 backlog follow-ups (now in approved/ and promoted to wave plan as VS-POLISH-DATA-FU-01/02/03) address gaps for contract fidelity:
- FU-01 (VS-POLISH-DATA-FU-01, in plan, dep for NIGHT-01): snapshot + scoring support for new rituals in FavoredRitualTypes (implemented: added MirrorPillarRestored/BellShrineRestored to FNightSanctuarySnapshot, PCGSubsystem::BuildSanctuarySnapshot, and NightConsequenceManager::ScoreRule cases).
- FU-02: display names + richer PopulateMVPNightConsequenceRules for all documented night types.
- FU-03 (dep for DATA-02): concrete starter Data Assets in Content/Data/ exactly matching the guide's "Example Assets (design specs)" (with values, fragments, etc.). Human editor step required per policy.

See agent_collab/outbox/planner/plan-vs-polish-202606.json (now 12 tasks) and backlog/approved/ for details. Roadmap overarching updated. Priority list maintained for execution.

Concrete assets and full tuning to be iterated in editor via DATA-02/FU-03 (and C++ FUs). The guide + plan tasks are now the contract; code/docs must align. Update this note as FUs and DATA-02 complete.

This completes the data asset documentation for the vertical slice polish.
