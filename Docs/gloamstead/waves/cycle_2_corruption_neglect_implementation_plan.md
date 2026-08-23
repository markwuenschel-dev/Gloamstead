# Cycle II — Direct Implementation Plan

**Authorization:** User elected direct implementation and approved a versioned save/resume migration on 2026-08-22.
**Base:** `86d1b7e27f40a6d06f8233d382439e10746b9910`
**Contract:** `Docs/gloamstead/waves/cycle_2_corruption_neglect_execution_contract.md`

## Global constraints

- Preserve the restoration-first, third-person dark-fantasy loop. No tower defense, horde survival, crafting grind, RTS, or deep combat.
- The player receives the Cycle II warning through the Heart during Day, has at least two readable supports before rest, and Dusk reuses—not replaces—the exact armed plan.
- `UGloamsteadDayNightSubsystem` is the only phase/cadence authority. `UNightConsequenceManager` and `UNightConsequenceRuntime` remain generic executors. Tutorial ownership ends after Cycle I's dawn.
- Every save migration is explicit and loss-aware. A legacy save may enter a reconciliation state but may not silently select or complete a later cycle.
- Gloamstead retains semantic/gameplay authority. WorldForge receives typed input and can only mirror state or produce generic generated outputs with provenance.
- Each task uses a public behavior seam, goes red before green, receives task review, and changes only its declared paths.

## Task 1 — Versioned experience persistence contract

**Behavior:** A v1 sanctuary save migrates into an explicit, safe Cycle II reconciliation state; a current save round-trips cycle slot, plan identity, outcome, scars, first-rest state, and saved phase without reinterpretation.

**Public seams:** `UGloamsteadSaveGame::MigrateToCurrentVersion()` and `FExperienceCyclePersistentState` accessors.

**Declared paths:**

- `Source/Gloamstead/Data/ExperienceCycleTypes.h`
- `Source/Gloamstead/Data/ExperienceCycleTypes.cpp`
- `Source/Gloamstead/Save/GloamsteadSaveGame.h`
- `Source/Gloamstead/Save/GloamsteadSaveGame.cpp`
- `Source/Gloamstead/PCG/GloamsteadPCGSubsystem.cpp`
- `Source/Gloamstead/Tests/ExperienceCyclePersistenceTests.cpp`

**Implementation:** Add a `FExperienceCyclePersistentState` data contract with completed slot, armed and last plan IDs, last outcome/result tag, scars, first-rest state, saved phase ordinal, and an explicit legacy-reconciliation flag. Introduce save version 2 and a pure migration method. Version 1 inputs preserve PCG payload untouched, clear unprovable authored progression, set reconciliation required, and advance to version 2. Version 2 inputs remain semantically identical. The live PCG save/load boundary must retain the current version during capture and migrate before any restore consumer reads the save; unsupported versions must fail explicitly rather than being treated as current.

**Acceptance:**

1. `Gloamstead.Experience.Persistence.LegacyV1MigratesSafely` proves no plan/outcome/scar is invented by migration and reconciliation is required.
2. `Gloamstead.Experience.Persistence.CurrentV2RoundTrips` proves every Cycle II progression field survives migration unchanged.
3. A production-adjacent PCG capture/load test proves a current v2 payload is not downgraded or cleared, while existing PCG save tests continue to prove their point-state payload remains unchanged.

**Verification:** Build and run the new `Gloamstead.Experience.Persistence` AutomationTests; run the full `Gloamstead` filter before task review.

## Task 2 — Authored upcoming-plan module

**Behavior:** The upcoming slot resolves to an immutable authored plan. Slot 1 is Tutorial; slot 2 is exact `Corruption` / `GardenRot` / `Cycle2_Garden`, regardless of catalog weight or sanctuary snapshot.

**Public seams:** `UGloamsteadExperienceCycleSubsystem::EnsureUpcomingPlan()`, `GetActivePlan()`, `RestorePersistentState()`, and `CapturePersistentState()`.

**Declared paths:**

- `Source/Gloamstead/Data/ExperienceCycleTypes.h`
- `Source/Gloamstead/Data/ExperienceCycleTypes.cpp`
- `Source/Gloamstead/Systems/GloamsteadExperienceCycleSubsystem.h`
- `Source/Gloamstead/Systems/GloamsteadExperienceCycleSubsystem.cpp`
- `Source/Gloamstead/Tests/ExperienceCycleTests.cpp`

**Implementation:** Add a two-row authored `UExperienceCycleCatalog` fallback with stable plan IDs and no narrative-selection fallback for slots one and two. The subsystem owns the active immutable plan and resolved persistent state. It fails closed when an authored required slot or its identifiers are invalid; generic catalog scoring starts only after authored rows end.

**Acceptance:** exact slot selection does not vary with catalog scores; a missing/mismatched row produces an invalid-plan result rather than a different night; restoring a persisted plan ID resolves the same plan or fails visibly.

**Verification:** Build and run `Gloamstead.Experience.Plan` tests, then the full `Gloamstead` filter.

## Task 3 — Day ownership, early warning, and full save wiring

**Behavior:** Day arms/reconstructs the plan and the Heart can expose its exact warning before rest. Dusk validates/reuses it, runtime receives its exact type, dawn records the outcome and one complete save object persists PCG plus day/cycle state.

**Public seams:** `UGloamsteadDayNightSubsystem::PrepareUpcomingCycle()`, `GetUpcomingPlan()`, `SaveProgressionToSlot()`, `LoadProgressionFromSlot()`, `AVeilHeart::EmitWarningById()`, and `UNightConsequenceManager::PrepareNightConsequencesForPlan()`.

**Declared paths:**

- `Source/Gloamstead/Systems/GloamsteadDayNightSubsystem.h`
- `Source/Gloamstead/Systems/GloamsteadDayNightSubsystem.cpp`
- `Source/Gloamstead/Data/ExperienceCycleTypes.cpp`
- `Source/Gloamstead/Systems/GloamsteadExperienceCycleSubsystem.h`
- `Source/Gloamstead/Systems/GloamsteadExperienceCycleSubsystem.cpp`
- `Source/Gloamstead/Systems/NightConsequenceManager.h`
- `Source/Gloamstead/Systems/NightConsequenceManager.cpp`
- `Source/Gloamstead/Systems/VeilHeart.h`
- `Source/Gloamstead/Systems/VeilHeart.cpp`
- `Source/Gloamstead/Systems/GloamsteadFirstNightDirector.cpp`
- `Source/Gloamstead/PCG/GloamsteadPCGSubsystem.h`
- `Source/Gloamstead/PCG/GloamsteadPCGSubsystem.cpp`
- `Source/Gloamstead/PCG/GloamsteadSanctuaryBootstrap.cpp`
- `Source/Gloamstead/Tests/ExperienceCycleTests.cpp`
- `Source/Gloamstead/Tests/PlayableCycleTests.cpp`

**Implementation:** Replace the PCG-only dawn save with a single bounded save orchestration path that invokes PCG capture and experience/day capture. On load, restore PCG first, then cycle/day state, then arm/reconcile the upcoming plan before the Heart permits progression. Exact warning presentation may retry until its one canonical Heart has both a valid catalog and an explicitly registered player-facing presenter; ambiguous Heart ownership keeps rest closed. A valid payload is never rejected solely for that timing. The lantern tutorial gate is ephemeral and may only be opened by its restoration event, never inferred from a saved armed plan. In-progress Dusk/Night state is not saveable until runtime-resume state exists; any existing such payload enters explicit Day reconciliation without replaying its already-persisted pressure. The sanctuary bootstrap delegates its existing load-on-start route to that full progression loader after it initializes the PCG baseline; it may not retain a competing PCG-only restore. A legacy reconciliation never assumes a later-cycle warning and resets phase authority coherently. Dusk must reject a missing/mismatched armed plan rather than call generic selection.

**Acceptance:** the player can inspect the exact second-day warning before rest; rest does not alter the plan ID; Dusk/Night/Dawn use that same ID; v2 save/load resumes Cycle II without replaying Cycle I or changing the plan; legacy save remains safe and explicit.

**Verification:** targeted `Gloamstead.Experience.*` and `Gloamstead.PlayableCycle.*` AutomationTests, then full `Gloamstead` filter.

## Task 4 — Retire tutorial control and centralize cadence

**Behavior:** Once Cycle I dawn completes, `AGloamsteadFirstNightDirector` clears timers and unbinds. DayNight owns Dusk-to-Night, Night-to-Dawn, and one early-objective completion transition. SkyPresenter is the sole global phase presentation writer. A safe later-cycle Day restore detaches any director that began before the save was loaded *before* generic warning presentation is retried. Any live night runtime is aborted before PCG restore, without resolving or recording its old outcome. A synchronous objective completion during `BeginNight` queues behind the already-entering Night transition; it cannot re-enter phase application or arm pressure after Dawn.

**Public seams:** `AGloamsteadFirstNightDirector::IsTutorialDetached()`, DayNight phase/cadence events, and existing runtime early-end delegate.

**Declared paths:**

- `Source/Gloamstead/Systems/GloamsteadFirstNightDirector.h`
- `Source/Gloamstead/Systems/GloamsteadFirstNightDirector.cpp`
- `Source/Gloamstead/Systems/GloamsteadDayNightSubsystem.h`
- `Source/Gloamstead/Systems/GloamsteadDayNightSubsystem.cpp`
- `Source/Gloamstead/Systems/NightConsequenceRuntime.h`
- `Source/Gloamstead/Systems/NightConsequenceRuntime.cpp`
- `Source/Gloamstead/Presentation/GloamsteadSkyPresenter.h`
- `Source/Gloamstead/Presentation/GloamsteadSkyPresenter.cpp`
- `Source/Gloamstead/Tests/FirstNightDirectorTests.cpp`
- `Source/Gloamstead/Tests/FirstNightIntegrationTests.cpp`
- `Source/Gloamstead/Tests/PlayableCycleTests.cpp`

**Acceptance:** a second Dusk/Night/Dawn cannot schedule a tutorial timer, write lantern-only reflection copy, or modify globals through the tutorial actor; early objective completion yields exactly one Dawn with ordered `Dusk -> Night -> Dawn` presentation and no timer armed after the early Dawn. Loading a later-cycle Day while a first-night director began active transfers presenter ownership safely before an exact warning can become eligible. Loading while a night runtime is active leaves no pressure timer/actor/strategy capable of mutating the restored Day state, and records no stale outcome.

**Verification:** relevant director/cycle tests and full `Gloamstead` filter.

## Task 5 — Fair crypticism, exact interpretation, and garden target

**Behavior:** `GardenRot` has three unique, distinct-media support channels. The player must encounter at least two through authored evidence sources, restore the stable `Cycle2_Garden` subject with canonical `GardenBed` semantics, and receive an exact interpretation receipt; Corruption targets that full subject contract rather than a global-most-corrupted fallback. The presented warning, evidence, and receipt are versioned save state: they restore atomically or reset safely, never leak across a rollback. Runtime data assets are regenerated from the authored manifest before this slice is called live.

**Public seams:** `AVeilHeart::RecordSupportEncounter()`, `EvaluateRestorationAgainstActivePlan()`, `ResolveSemanticSubjectToPoint()`, and exact warning validation.

**Declared paths:**

- `Source/Gloamstead/Data/ExperienceCycleTypes.h`
- `Source/Gloamstead/Data/ExperienceCycleTypes.cpp`
- `Source/Gloamstead/Data/VeilHeartWarningTypes.h`
- `Source/Gloamstead/Data/RitualTypes.h`
- `Source/Gloamstead/Systems/VeilHeart.h`
- `Source/Gloamstead/Systems/VeilHeart.cpp`
- `Source/Gloamstead/Systems/GloamsteadExperienceCycleSubsystem.cpp`
- `Source/Gloamstead/Systems/GloamsteadDayNightSubsystem.h`
- `Source/Gloamstead/Systems/GloamsteadDayNightSubsystem.cpp`
- `Source/Gloamstead/Systems/NightConsequenceRuntime.h`
- `Source/Gloamstead/Systems/NightConsequenceRuntime.cpp`
- `Source/Gloamstead/Data/NightRuntimeTypes.h`
- `Source/Gloamstead/Systems/NightStrategy.cpp`
- `Source/Gloamstead/PCG/GloamsteadPCGSubsystem.h`
- `Source/Gloamstead/PCG/GloamsteadPCGSubsystem.cpp`
- `Source/Gloamstead/Save/GloamsteadSaveGame.h`
- `Source/Gloamstead/Save/GloamsteadSaveGame.cpp`
- `Source/Gloamstead/Actors/GloamsteadEvidenceSource.h`
- `Source/Gloamstead/Actors/GloamsteadEvidenceSource.cpp`
- `Source/Gloamstead/Components/RitualPlacementComponent.cpp`
- `Source/GloamsteadEditor/Commandlets/GloamsteadImportDataAssetsCommandlet.cpp`
- `Source/GloamsteadEditor/Validation/VeilHeartWarningCatalogValidator.h`
- `Source/GloamsteadEditor/Validation/VeilHeartWarningCatalogValidator.cpp`
- `Source/GloamsteadEditor/GloamsteadEditor.Build.cs`
- `specs/data/data_asset_manifest.schema.json`
- `specs/data/vs-polish-starter.json`
- `Source/Gloamstead/Tests/FairCrypticismTests.cpp`
- `Source/Gloamstead/Tests/ExperienceCycleTests.cpp`
- `Source/Gloamstead/Tests/ExperienceCyclePersistenceTests.cpp`
- `Source/Gloamstead/Tests/PCGSubsystemTests.cpp`
- `Source/Gloamstead/Tests/PlayableCycleTests.cpp`
- `Content/Data/DA_VeilHeartWarningCatalog.uasset`
- `Content/Data/DA_Ritual_GardenBed.uasset`

**Implementation:** Consult Context7 for the current Unreal data-validation API before editor code. The importer and validator reject sparse/duplicate/mismatched support data and require the canonical Environmental/ObjectReaction/Audio GardenRot media. Runtime validates evidence/restoration against authoritative world objects and PCG point state, requires the full active-plan target contract, and fails visibly if map metadata cannot resolve it. Make evaluator write APIs non-Blueprint-forgeable. Migrate v2 saves to v3 by clearing unprovable interpretation state, then capture/restore the Heart state atomically with the cycle. Run the sanctioned importer to materialize changed controlled assets; do not hand-edit binary assets. `AGloamsteadEvidenceSource` is the player-world reporting endpoint, while Task 7/8 must materialize its authored instances and `Cycle2_Garden` metadata through WorldForge rather than one-off map edits.

**Acceptance:** one-channel/duplicate/unknown/wrong-medium support fixtures fail; the exact warning ID cannot be substituted by clarity; two distinct supports plus the correct garden restoration are required for an interpreted result; forged payloads cannot mint a receipt; v2 and rollback reloads neither invent nor leak interpretation state; runtime-loaded controlled assets match the authored contract; absent or mismatched garden target does not silently select another corruption bloom. The Cycle II route is not called end-to-end playable until Task 7/8 materializes authored evidence sources and `Cycle2_Garden` metadata into `Lvl_Gloamstead` with WorldForge provenance.

**Verification:** C++ and editor AutomationTests, import negative fixtures, then full `Gloamstead` filter.

## Task 6 — Readable Cycle II presentation and failure recovery

**Behavior:** A generic non-debug presenter gives caption, journal, and dawn-summary data for the exact plan/outcome. It is accessible without colour alone, persists a warning after its cue, and makes failure recovery legible.

**Public seams:** `UGloamsteadCyclePresentationSubsystem` warning/reflection delegates and a plan/outcome presentation model.

**Declared paths:**

- `Source/Gloamstead/Systems/GloamsteadCyclePresentationSubsystem.h`
- `Source/Gloamstead/Systems/GloamsteadCyclePresentationSubsystem.cpp`
- `Source/Gloamstead/Systems/GloamsteadCycleFeedbackSubsystem.h`
- `Source/Gloamstead/Systems/GloamsteadCycleFeedbackSubsystem.cpp`
- `Docs/gloamstead/ui/cycle_2_feedback_spec.md`
- `Source/Gloamstead/Tests/PlayableCycleTests.cpp`

**Acceptance:** Cycle II no longer depends on `AddOnScreenDebugMessage` or the tutorial actor for warning/dawn delivery. Automation proves once-only generic dispatch; human UI review and approved UMG materialization remain required.

## Task 7 — Gloamstead semantic world spec and WorldForge state projection

**Behavior:** Gloamstead declares one garden world subject and projects authoritative restoration state to WorldForge's generic mirror without reverse authority.

**Public seams:** `UGloamsteadWorldStateProjectionSubsystem::RebuildFromAuthoritativeState()` and typed semantic JSON validation.

**Declared paths:**

- `Source/Gloamstead/Systems/GloamsteadWorldStateProjectionSubsystem.h`
- `Source/Gloamstead/Systems/GloamsteadWorldStateProjectionSubsystem.cpp`
- `Source/Gloamstead/PCG/GloamsteadPCGSubsystem.h`
- `Source/Gloamstead/PCG/GloamsteadPCGSubsystem.cpp`
- `Source/Gloamstead/Tests/WorldStateProjectionTests.cpp`
- `specs/world/gloamstead_world_spec.schema.json`
- `specs/world/cycle-2-corruption-neglect.world.json`
- `Docs/gloamstead/world/cycle_2_worldforge_receipt_requirements.md`

**Acceptance:** neglected/restored garden yields deterministic `restoration_level` values; a mirror reset reconstructs from Gloamstead state; schema rejects ambiguous subjects, missing reactive categories, and output-path escape.

## Task 8 — WorldForge factory, controlled materialization, and proof

**Behavior:** A separate clean WorldForge worktree consumes the Gloamstead spec, validates, generates, materializes controlled assets, and surveys both state scenarios. Gloamstead then integrates via approved Unreal/NeoStack materialization and a map-load check.

**Declared paths:** only separately authorized WorldForge branch paths plus `/Game/Generated/WorldForge/Cycle2/`, `Content/Maps/Lvl_Gloamstead.umap`, generated receipt paths, playtest protocol, and evidence paths named by the contract.

**Acceptance:** clean-source provenance, dual-state surveys for foliage/ruins/path/lighting-material, map-load evidence, system gate, and three cold-start playtests demonstrating causal explanation. No cook/package retry is part of this task.

## Task 9 — Full carrier review, verification, and delivery

**Behavior:** The exact committed branch has a truthful SYSTEM_CORRECT receipt plus an EXPERIENCE_WORKS receipt. It is reviewed before independent verification and publication follows the repository policy.

**Acceptance:** `./gate.ps1` passes on the final exact commit; independent verifier verdict is PASS; materialization/map-load and playtest evidence are explicitly reported rather than implied.
