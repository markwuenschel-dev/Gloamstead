# Phase 2 – Core Loop (Implementation)

**Status:** Core C++ loop complete on `main` (June 2026). Polish wave (visuals, data assets, UI, persistence, night/combat pressure) active under agent_collab. Human editor verification required for generated content. Combat spawns, full journal, and deeper persistence are part of the current polish efforts.

---

## Goal

Prove the full cycle in code:

**Warning → Restoration → Dusk prep → Night consequence → Dawn reflection**

---

## Phase timeline (collab waves)

| Wave | Tasks | Deliverable |
|------|-------|-------------|
| NC-1 | NC-001–003 | Night types, sanctuary snapshot, catalog scoring |
| NC-2 | NC-004–006 | `UGloamsteadDayNightSubsystem`, payload time/night count, VeilHeart dawn clear |
| NC-3 | NC-007–009 | `UNightConsequenceRuntime`, `ApplyCorruptionSpread`, night phase hooks |
| NC-4 | NC-010–012 | Built-in MVP catalog, `OnOmenClueReady`, tutorial half-spread |
| VH-1 | VH-001–003 | Warning catalog, dusk emit, catalog-based tag satisfaction |
| RP-1 | RP-001–002 | `URitualDefinition` payloads, `OnRestoredActorSpawned` |

---

## Day / night authority

**`UGloamsteadDayNightSubsystem`** (`Source/Gloamstead/Systems/GloamsteadDayNightSubsystem.*`)

| Phase | Behavior |
|-------|----------|
| **Dusk** | `UNightConsequenceManager::PrepareNightConsequences()` then `AVeilHeart::EmitWarningForNight` |
| **Night** | `UNightConsequenceRuntime::BeginNight()` + type stub |
| **Dawn** | `EndNight()` then first `AVeilHeart::ProcessDawnReflection()` |

BlueprintCallable: `AdvanceToNextPhase`, `SetPhase`, `GetNormalizedTimeOfDay`, `GetNightCount`.

---

## Night selection

**`UNightConsequenceManager`**

- Listens to `OnStructureRestored` (path light coverage tracked for future use).
- `PrepareNightConsequences`: `BuildSanctuarySnapshot` → score rules → `OnNightPlanReady`.
- If no catalog asset is assigned, **`PopulateMVPNightConsequenceRules`** fills a runtime default catalog (Tutorial / Corruption / Omen). See VS-POLISH-DATA-FU-02 for extension to all documented night types per the complete authoring guide in systems/03_night_consequence_system.md.

---

## Night execution

**`UNightConsequenceRuntime`**

| `ENightConsequenceType` | Runtime behavior |
|-------------------------|------------------|
| **Corruption** | `ApplyCorruptionSpread(0.12f, 8)` on PCG points |
| **Tutorial** | `ApplyCorruptionSpread(0.06f, 4)` |
| **Omen** | Broadcast **`OnOmenClueReady`** (`FName` from catalog rule, default `GardenRot`) |

Delegates: `OnNightStarted`, `OnNightEnded`, `OnOmenClueReady`.

---

## PCG mutations

**`UGloamsteadPCGSubsystem::ApplyCorruptionSpread`**

- Increases `CorruptionLevel` on random points (capped count, clamped 0–1).
- Does **not** change restoration flags or `ApplyRestoration` behavior.

Sanctuary getters feed night scoring: `BuildSanctuarySnapshot`, averages, per-ritual restore counts.

---

## Veil Heart

**`AVeilHeart`**

- **`UVeilHeartWarningCatalog`**: rows with `AssociatedNightType`, `SatisfiableTags`, `Fragment` text. Full designer contract + examples in the Data Asset Authoring Guide (systems/03); concrete assets via VS-POLISH-DATA-FU-03 + DATA-02.
- **`EvaluateRestorationAgainstWarnings`**: matches `Payload.WarningTagSatisfied` (or ritual name fallback).
- **`EmitWarningForNight`**: picks catalog row, calls BP **`OnWarningEmitted`**.
- **`ProcessDawnReflection`**: logs satisfied tags, clears set for next cycle.

---

## Ritual placement

**`URitualPlacementComponent`**

- `TMap<ERitualType, URitualDefinition*>` for light/corruption/tag defaults. Extended enum (MirrorPillar, BellShrine) and full URitualDefinition specs + example tuning values in authoring guide (systems/03). Snapshot/scoring support for new types in VS-POLISH-DATA-FU-01. Concrete DAs via FU-03.
- Point metadata `RecommendedForWarning` overrides when set.
- **`OnRestoredActorSpawned`** after successful `ConfirmPlacement` (BP implements visuals).

---

## Editor checklist

| Step | Status (2026-06-11) |
|------|---------------------|
| 1. Build **Gloamstead** + **GloamsteadEditor** | Done |
| 2. Data assets in `Content/Data/` (import factory) | Done — [specs/data/VERIFICATION-2026-06-11.md](../specs/data/VERIFICATION-2026-06-11.md) |
| 3. **AVeilHeart** placed; catalogs assigned / auto-loaded | Done on `Lvl_ThirdPerson` |
| 4. **URitualPlacementComponent** + ritual DAs on player BP | Done |
| 5. Level BP test key → **`Advance Gloamstead Day Phase`** | Done |
| 6. PCG initialized (`InitializeFromPCGComponent`) | **Next** |
| 7. Restore ritual; non-Tutorial night + dusk warning in PIE | Blocked on step 6 |

Wiring: [specs/data/WIRING.md](../specs/data/WIRING.md).

---

## Deferred (Phase 2+)

- Night entities, VFX, combat per night visual language.
- Journal subsystem and structured dawn rewards.
- SaveGame for point state.
- Extended `ENightConsequenceType` enum and designer tooling.

See [systems/03_night_consequence_system.md](systems/03_night_consequence_system.md) for design detail and per-wave implementation notes.