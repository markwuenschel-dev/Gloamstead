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

Visual language: spatial discontinuity, misaligned paths, memory afterimages, drifting architecture edges, fog that cuts space into uncertain layers. Use Liminal Memory Realism most strongly here.

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

## Implementation status (2026-06-01)

Wave **night-consequence NC-1** (tasks NC-001–NC-003) adds data and dusk **selection** only. No night entities spawn yet; design sections above still describe the full target.

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

**PIE / editor**: advance phases manually (e.g. test BP calling **`AdvanceToNextPhase`**). Assign **`UNightConsequenceCatalog`** on the night manager (NC-1 follow-up still applies).

### Deferred (post NC-2)

- **Spawning / runtime night mechanics** (corruption spread, omens, combat, VFX, failure states).
- **Full night-type catalog** in code (Retrieval, Silence/Possession, Mirror, Bargain, Fracture, True Siege) and designer assets per type.
- **Dawn feedback** beyond tag clear + log (journal, resources, adaptation rollout).
