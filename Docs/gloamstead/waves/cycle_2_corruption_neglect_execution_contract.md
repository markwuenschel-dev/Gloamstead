# Cycle II — Corruption / Neglect Execution Contract

**Status:** authorized for direct implementation — no gameplay or generated-content mutation has started
**Working branch:** `feat/cycle-2-corruption-neglect`
**Baseline:** `86d1b7e27f40a6d06f8233d382439e10746b9910` (`feat/authored-sanctuary-environment`)
**Carrier:** the first recurring night after the proven lantern tutorial

## Authorization record

On 2026-08-22, the user elected direct implementation rather than an additional full design-review lane and explicitly approved the versioned save/resume migration. The accepted direct-implementation risk is that this contract, its evidence pass, and hostile tests are the design gate; implementation must stop if they reveal a new consequential ambiguity.

The execution review corrected one timing conflict in the earlier draft: a player cannot investigate a warning before restoration if it is selected and emitted only after resting into Dusk. The upcoming plan and its warning are therefore armed during the preceding Day and exposed through the Heart before the rest action; Dusk validates and reuses that exact plan rather than selecting a surprise one.

## 1. Outcome

Ship one complete second cycle in `Lvl_Gloamstead`:

> The Heart warns that unattended growth will spread. The player can study a visibly neglected garden restoration site, connect the warning to its reactions and the first night's learned pattern, restore it deliberately, choose to rest, endure a corruption consequence whose pressure and environment reflect that decision, and receive a dawn explanation of what held or spread.

The cycle is complete only when a playtester can explain, without a designer prompt, the relationship between the warning, the garden action, the night behaviour, and the dawn result. `gate.ps1` green is necessary but cannot substitute for that result.

## 2. Scope and non-goals

### In scope

- Cycle I remains an explicit tutorial; Cycle II is explicitly `Corruption`, not a probabilistic catalog outcome.
- A recurring-cycle authority separates authored progression from phase authority, generic consequence scoring, tutorial scripting, and presentation.
- The Corruption warning has at least two distinct, executable support channels and fails validation when it does not.
- One meaningful garden restoration site makes its neglect, restoration, night pressure, and dawn outcome readable through place changes.
- The first real Gloamstead-to-WorldForge vertical world-production receipt is defined and then satisfied through an isolated WorldForge task worktree.
- Generated environment state changes at least foliage, ruined dressing, path legibility, and lighting/material coherence; it never becomes gameplay authority.
- System proof and a repeatable human playtest script are both added.

### Explicitly out of scope for this carrier

- Cycles III–VI, possession combat, mirror/bargain ambiguity, fracture/siege, ending content, open-world expansion, crafting, horde behaviour, or deeper combat.
- A generic six-night framework with hard-coded narrative branches. The sequence must be data-driven but contain only Cycle I and Cycle II definitions now.
- Hand-editing `.umap` or `.uasset` files, mutating the dirty external `D:\Unreal Projects\WorldForge` checkout, or treating the existing WorldForge plugin/proxy as generated-world proof.
- Save-format migration without the explicit decision in section 16.

## 3. Baseline evidence and defect statement

The current runtime can select and execute `Corruption`, but it does not yet deliver a second authored night:

1. `UGloamsteadDayNightSubsystem` owns phase change and asks `UNightConsequenceManager` to prepare a type at dusk.
2. `UNightConsequenceManager` has only first-night forcing plus catalog scoring; `NightsPrepared` is session-only and no authored sequence selects Cycle II.
3. `AGloamsteadFirstNightDirector` binds phase, runtime, and Heart delegates. It continues to handle later Dusk/Night/Dawn events after `CompleteDawn`, so its lantern cues, lantern reflection copy, timing, and direct global presentation writes can replay for Corruption.
4. `AGloamsteadSkyPresenter` is concurrently subscribed to phase change. The map therefore has two writers for global sky/fog/post-process presentation.
5. `FVeilHeartWarningFragment` contains text, night type, satisfiable tags, and clarity only. It has no evidence-channel contract and no validator.
6. The embedded WorldForge plugin exposes generic state-aware placement/material provenance primitives, but no Gloamstead authored spec has yet produced, materialized, and surveyed a meaningful place.

The fix is not a second timer, another debug message, or a catalog-weight tweak. It is a narrow ownership correction plus one data-backed cycle with auditable evidence.

## 4. Player contract

| Beat | Player-visible promise | Required evidence |
| --- | --- | --- |
| Warning | A cryptic warning points toward tending what is spreading. | Captioned Heart line, journal entry, and a readable support-channel list behind the warning. |
| Investigation | The garden visibly looks unlike trustworthy/restored ground. | Diseased/overgrown environmental dressing, a local reaction at the site, and an interaction/readable clue. |
| Interpretation | The player can infer that this place—not any arbitrary restoration—matters before resting. | Matching subject tag in journal/clue, restoration preview/object reaction, and a warning support receipt. |
| Restoration | Restoring the garden changes it immediately and changes the surrounding place. | Garden state, nearby foliage/ruin/path state, and WorldForge state projection all update from Gloamstead authority. |
| Preparation | The player deliberately asks the Heart to bring night. | Rest interaction remains player-controlled at Day/Dawn and shows the selected Cycle II warning before night begins. |
| Consequence/survival | Neglect produces a visible corruption pressure; correct action reduces or resolves it. | Existing Corruption runtime objective/outcome, pressure presentation, and no generic wave semantics. |
| Dawn understanding | Dawn explains what the player changed and what still spread. | Specific dawn summary/journal result, not lantern-only fallback copy. |

## 5. Ownership model

```text
Gloamstead semantic intent + gameplay state
    -> typed Cycle II world specification
    -> WorldForge generic generation + strict validation
    -> approved NeoStack/Unreal materialization into /Game/Generated/WorldForge/Cycle2/
    -> Lvl_Gloamstead references generated outputs
    -> WorldForge survey receipt
    -> Gloamstead gameplay-state and warning validation
    -> human Cycle II playtest approval
```

### Gloamstead owns

- `Cycle2_Garden` identity, its gameplay anchor, what the warning means, its evidence, restoration tags, outcome criteria, and dawn language.
- Authoritative player/restoration/PCG/save state. A WorldForge state mirror is always rebuilt from this authority; it cannot decide a night or award an outcome.
- The typed world specification and acceptance expectations for this place.

### WorldForge owns

- Generic schema checking, terrain/biome/POI and placement generation, procedural materials, state-aware environment rules, materialization, survey, and generation provenance.
- Generic use of `restoration_level` as a consumer-provided state key. It must not infer why the garden matters or create new Gloamstead lore.

### Runtime authority after this carrier

| Concern | Sole owner | Prohibited owners |
| --- | --- | --- |
| Day/Dusk/Night/Dawn transitions and cadence timer lifetime | `UGloamsteadDayNightSubsystem` | `AGloamsteadFirstNightDirector`, UMG, WorldForge |
| Cycle slot and authored night plan | new `UGloamsteadExperienceCycleSubsystem` | catalog weights, tutorial actor, WorldForge |
| Generic execution of selected night | `UNightConsequenceManager` + `UNightConsequenceRuntime` | presentation actors |
| Tutorial-only gate/cues | `AGloamsteadFirstNightDirector` until its first dawn completes | later cycle phase events |
| Global sky/fog/post-process phase presentation | `AGloamsteadSkyPresenter` | `AGloamsteadFirstNightDirector` |
| Player-facing cycle captions, journal, dawn summary | new `UGloamsteadCyclePresentationSubsystem` and approved UMG assets | `UGloamsteadCycleFeedbackSubsystem` debug messages |
| Environment visual projection | new `UGloamsteadWorldStateProjectionSubsystem` writing WorldForge's generic state mirror | WorldForge mutating Gloamstead gameplay state |

## 6. Data contracts

### 6.1 Authored sequence

Add `Source/Gloamstead/Data/ExperienceCycleTypes.h` and `UExperienceCycleCatalog` with exactly two initially authored `FExperienceCycleDefinition` entries:

| Slot | Night type | Warning ID | Meaningful subject | Required tag | Outcome subject |
| --- | --- | --- | --- | --- | --- |
| 1 | `Tutorial` | existing tutorial warning | first lantern | existing lantern tag | first lantern |
| 2 | `Corruption` | `GardenRot` | `Cycle2_Garden` | `GardenBed` | garden corruption state |

Each definition carries an explicit `WarningId`, `NightType`, stable subject ID, required restoration tags, visual-state key (`restoration_level`), and an outcome-summary key. It may not carry prose invented by WorldForge or select a result itself.

`UGloamsteadExperienceCycleSubsystem` resolves a definition for the upcoming slot at the preceding Day (or reconstructs it on load) and retains it through Dawn. The Heart exposes that exact warning when the player examines it before rest. `UNightConsequenceManager` evaluates the current sanctuary snapshot as normal but must use a valid authored definition as an exact override for Cycle I/II; catalog selection is a logged fallback only after the authored catalog ends. Dusk validates and reuses the already-armed plan; it may not choose a different plan. The subsystem exposes the entire last plan, not merely an enum.

### 6.2 Fair crypticism

Extend `Source/Gloamstead/Data/VeilHeartWarningTypes.h` with:

- `EWarningSupportChannel`: `EnvironmentalClue`, `RepeatedPattern`, `RestoredObjectReaction`, `AudioCue`, `EnemyBehavior`, `VisibleConsequence`, `DawnFeedback`.
- `FWarningSupportChannel`: channel kind, stable `EvidenceId`, and player-readable `FText` description.
- `FVeilHeartWarningFragment::SupportChannels`.

`GardenRot` must declare at least the following three independent channels:

1. `EnvironmentalClue` / `cycle2_garden_neglect`: the untrusted/overgrown garden state is visibly distinct from restored ground.
2. `RestoredObjectReaction` / `cycle2_garden_restored`: restoring the garden produces a local visual and interaction-state change before the player rests.
3. `DawnFeedback` / `cycle2_garden_dawn_outcome`: dawn identifies whether tending held corruption or neglect let it spread.

At least two are the invariant; three lets one channel fail gracefully without making the warning arbitrary. Static metadata alone is insufficient: the runtime must record presentation/availability of the named channels for the active plan, and the human test must encounter two before resting.

`AVeilHeart` gains `EmitWarningById(FName)` and fails closed to a warning-free safe state if the selected plan's ID is absent, duplicated, mismatched to the selected night, or invalid. It must not silently substitute a higher-clarity warning for a different authored plan.

### 6.3 World specification

Add these owned, text-reviewed inputs before any generated asset is accepted:

- `specs/world/gloamstead_world_spec.schema.json`
- `specs/world/cycle-2-corruption-neglect.world.json`

The Cycle II spec must identify `Lvl_Gloamstead`, the `Cycle2_Garden` POI/anchor and bounds, input seed/version, allowed output root, the four required reactive environment categories (foliage, ruins, path, lighting/material), the generic `restoration_level` key and scope, evidence IDs from section 6.2, and expected survey subjects. It must not encode gameplay decision logic, faction lore, or an arbitrary generated target.

WorldForge's generated manifest must name the source spec hash, generator revision, source commit/dirty status, output root, all generated asset paths, and survey subject IDs. Dirty-source provenance is recordable but cannot be promoted as a release receipt.

## 7. Work packages and dependency order

### C2-01 — Make recurring-cycle selection explicit

**Files:**

- Add `Source/Gloamstead/Data/ExperienceCycleTypes.h` and `Source/Gloamstead/Data/ExperienceCycleTypes.cpp` only if a `.cpp` is needed for non-inline helpers.
- Add `Source/Gloamstead/Systems/GloamsteadExperienceCycleSubsystem.h/.cpp`.
- Extend `Source/Gloamstead/Systems/NightConsequenceManager.h/.cpp`.
- Extend `Source/Gloamstead/Systems/GloamsteadDayNightSubsystem.h/.cpp`.
- Add `Source/Gloamstead/Tests/ExperienceCycleTests.cpp`.

**Implementation:** resolve one immutable plan during the preceding Day, expose its exact warning before the player rests, retain its stable ID through Dusk/Night/Dawn, and pass the plan's selected type to the existing runtime. Dusk validates/reuses the armed plan and never reseats it. Keep `UGloamsteadDayNightSubsystem` as the only phase mutator. Its cadence timers must be cleared when a phase changes or the world tears down; early objective completion advances exactly once.

**System acceptance:** tests prove slot 1 is Tutorial, slot 2 is exactly Corruption regardless of catalog weights/snapshot, the warning is available before rest, a missing authored Cycle II entry fails visibly rather than falling back, and exactly one plan is used for warning/runtime/dawn.

### C2-02 — Retire tutorial ownership after its first dawn

**Files:**

- Extend `Source/Gloamstead/Systems/GloamsteadFirstNightDirector.h/.cpp`.
- Extend `Source/Gloamstead/Presentation/GloamsteadSkyPresenter.h/.cpp` only for explicit sole-owner wiring; do not duplicate phase effects.
- Extend `Source/Gloamstead/Tests/FirstNightDirectorTests.cpp` and `Source/Gloamstead/Tests/FirstNightIntegrationTests.cpp`.

**Implementation:** immediately after Cycle I's completed dawn presentation, clear tutorial timers, unbind all phase/runtime/Heart delegates, and permanently stop tutorial caption/reflection/presentation work. Remove its direct global sky/fog/post-process writes or confine them to first-night-local effects that cannot overlap SkyPresenter. SkyPresenter owns phase-wide globals for all cycles.

**System acceptance:** force a second Dusk/Night/Dawn after `CompleteDawn`; assert no tutorial timer, lantern influence cue, lantern-only reflection, or global-presentation write occurs. Assert SkyPresenter alone receives the phase event.

### C2-03 — Make the warning fair by construction

**Files:**

- Extend `Source/Gloamstead/Data/VeilHeartWarningTypes.h`.
- Extend `Source/Gloamstead/Systems/VeilHeart.h/.cpp`.
- Extend `Source/GloamsteadEditor/Commandlets/GloamsteadImportDataAssetsCommandlet.cpp`.
- Add `Source/GloamsteadEditor/Validation/VeilHeartWarningCatalogValidator.h/.cpp`.
- Extend `Source/GloamsteadEditor/GloamsteadEditor.Build.cs` with only the verified UE data-validation dependencies.
- Extend `specs/data/data_asset_manifest.schema.json` and `specs/data/vs-polish-starter.json`.
- Add `Source/Gloamstead/Tests/FairCrypticismTests.cpp` and an editor automation test beside the validator.

**Implementation:** before coding the validator, use Context7 to resolve the current Unreal `UEditorValidatorBase` registration, result, and module-dependency API for this engine version. The importer must parse every support field and reject malformed arrays. The editor validator rejects empty IDs/descriptions, duplicate channel kinds/evidence IDs, invalid night types, no `SatisfiableTags`, and fewer than two distinct support channels. The `GardenRot` row has the three channels in section 6.2.

**System acceptance:** unit/import/editor tests cover valid three-channel input, one-channel rejection, duplicate-channel rejection, unknown evidence ID rejection against the authored Cycle II spec, and exact-warning-ID emission. The asset validation command must return non-zero for an invalid fixture.

### C2-04 — Replace debug-only feedback with a Cycle II readable surface

**Files:**

- Add `Source/Gloamstead/Systems/GloamsteadCyclePresentationSubsystem.h/.cpp`.
- Extend `Source/Gloamstead/Systems/GloamsteadCycleFeedbackSubsystem.h/.cpp` to mark debug-only output unavailable in non-development presentation paths; do not route final experience through screen-debug messages.
- Add the approved source-controlled UMG design specification under `docs/gloamstead/ui/cycle_2_feedback_spec.md`.
- Generated under `/Game/Generated/Gloamstead/UI/Cycle2/` only through the approved Unreal/NeoStack materialization handoff: warning caption, journal entry surface, and dawn summary surface.

**Implementation:** bind generic Heart warning and dawn events after the tutorial actor unbinds. The presentation subsystem receives the current plan and displays its exact warning, evidence-ready journal entry, and a dawn result tied to the garden's actual outcome. Captions are on by default, localisable FText, readable without colour alone, and remain accessible from the journal after the cue ends.

**System acceptance:** automation proves the generic presenter receives Cycle II's warning/reflection once each and never calls debug presentation. Human review verifies legibility at the intended camera distance and that the UI calls the object "the Heart," never a code-only name.

### C2-05 — Project authoritative restoration state into generic WorldForge state

**Files:**

- Add `Source/Gloamstead/Systems/GloamsteadWorldStateProjectionSubsystem.h/.cpp`.
- Extend `Source/Gloamstead/PCG/GloamsteadPCGSubsystem.h/.cpp` only with a read-only snapshot/query seam if required.
- Add `Source/Gloamstead/Tests/WorldStateProjectionTests.cpp`.

**Implementation:** on relevant `FRestorationEventPayload` and on reconstruction/load, calculate the garden area's `restoration_level` from Gloamstead PCG/restoration state and write it to `UWorldStateSubsystem`. Only this projection writes the generic mirror. A WorldForge state reset is recoverable by reconstructing from Gloamstead state; WorldForge never writes back a result or changes a night plan.

**System acceptance:** an untouched garden, a restored garden, and a reload/reconstruction produce deterministic values. Tests prove the projection cannot alter Gloamstead's snapshot, selected plan, or night outcome.

### C2-06 — Author and validate the semantic WorldForge input

**Files:**

- Add `specs/world/gloamstead_world_spec.schema.json`.
- Add `specs/world/cycle-2-corruption-neglect.world.json`.
- Add `docs/gloamstead/world/cycle_2_worldforge_receipt_requirements.md`.

**Implementation:** write a reviewable spec for the one garden place. Schema validation rejects output roots outside `/Game/Generated/WorldForge/Cycle2/`, missing map/anchor/evidence references, missing state categories, and ambiguous or duplicate survey IDs. The receipt requirements document exact provenance fields, state scenarios (`0.0` neglected and `1.0` restored), expected paths, and minimum survey evidence.

**System acceptance:** schema fixtures prove a malformed semantic spec fails before WorldForge is called. The source spec binds every `GardenRot` evidence ID to either an environmental subject, object reaction, or dawn report.

### C2-07 — Execute WorldForge factory work in its own clean worktree

**Repository boundary:** this is a separate delivery item in a new clean `D:\Unreal Projects\WorldForge` worktree. Do not mutate the current dirty WorldForge checkout and do not copy generic generator logic into Gloamstead.

**Work:** extend/reuse WorldForge's consumer-profile/bridge/materialization/survey tools only where generic capability is missing. It must consume the Gloamstead typed spec as caller input, generate placement/material/environment records with provenance, validate before mutation, materialize only under the allowed output root, and survey the actual outputs. Generated placement rules use generic `restoration_level` to change foliage, ruined dressing, and path coherence; material/environment outputs react to the same key for lighting/material coherence.

**System acceptance:** WorldForge produces a clean-source manifest and survey receipt that names every expected subject, source spec hash, generator revision, output asset, and state scenario. The two surveys differ in all four required categories. A missing or unproven category is a red receipt, not a manual substitute.

### C2-08 — Controlled Unreal materialization and map integration

**Files/artifacts:** generated content only under `/Game/Generated/WorldForge/Cycle2/`, map references in `Content/Maps/Lvl_Gloamstead.umap`, plus the tracked manifest/survey receipts named in C2-07.

**Implementation:** submit an explicit binary-materialization handoff containing exact generated asset paths, source spec and generator hashes, the approved automation invocation, expected map references, and rollback target. Run only the approved Unreal/NeoStack automation. Load `Lvl_Gloamstead` immediately after materialization; any load warning, ownership mismatch, stale provenance, or unexpected asset path fails the handoff. Never hand-edit binary assets.

**System acceptance:** map-load proof shows the intended garden subjects, no substitute proxy, and no unrelated map churn. Survey after materialization matches the pre-mutation manifest. A cleanup/rebuild repeats from the same spec into the same controlled root without accumulating unnamed actors.

### C2-09 — End-to-end system and experience proof

**Files:**

- Extend `Source/Gloamstead/Tests/PlayableCycleTests.cpp`.
- Extend `Source/Gloamstead/Tests/NightRuntimeTests.cpp`.
- Add `docs/gloamstead/playtests/cycle_2_corruption_neglect_playtest.md`.
- Add generated-but-tracked evidence receipt path in `docs/gloamstead/evidence/cycle_2/` only after proof is obtained.

**System acceptance:** a deterministic test drives tutorial completion, the second Day warning before rest, investigation receipt, garden restoration or neglect branch, Corruption runtime, exactly one dawn, correct outcome, generic dawn feedback, and a deterministic WorldForge state projection. `./gate.ps1` must pass on the final branch; all new hostile tests must run under the existing `Gloamstead` filter.

**Experience acceptance:** use the playtest protocol with at least three fresh participants or three independent cold-start sessions. Before dawn, ask: "What do you think the warning means and what would you do?" After dawn, ask: "Why did the night behave that way?" Pass only when each participant encounters two supports and can state a causal explanation without a facilitator supplying it. Record answers, path taken, missed evidence, death/failure recovery, and UI/caption issues. A participant who succeeds by guessing is not a pass.

## 8. Test matrix

| Proof | Automated? | Failure that must be caught |
| --- | --- | --- |
| Exact authored Cycle II selection | Yes | catalog weight or snapshot changes select a different night |
| Tutorial detaches | Yes | later phase event triggers lantern timers/copy/global presentation |
| Phase/early-dawn single transition | Yes | duplicate Dawn or orphan cadence timer |
| Fair-warning structure/import/validator | Yes | one vague clue, duplicate or malformed support, wrong warning ID |
| Support availability receipt | Yes | data declares a channel that runtime never exposes |
| State projection directionality | Yes | WorldForge mirror alters gameplay authority |
| World spec/schema | Yes | semantic ambiguity, path escape, missing reactive category |
| WorldForge provenance + dual-state survey | Yes, external tool receipt | generated assets without reproducible source/subjects |
| Map load after materialization | Human/approved UE automation | invalid binary/map reference or stale generated asset |
| Warning-to-dawn comprehension | Human | player cannot explain causal relationship |
| Failure recovery/accessibility | Human | unreadable caption/feedback or soft lock after neglect/death |

## 9. Required verification order

1. Run `pwsh -NoProfile -File agent_collab/scripts/Test-ShellGuard.ps1` after any collaboration-guard change. This carrier should not modify guards.
2. Run targeted automation during each C2 package, then `./gate.ps1` after C++/data integration. Do not call the carrier green from partial targeted tests.
3. Validate semantic JSON before invoking WorldForge.
4. In the isolated WorldForge worktree: validate → generate/pre-mutation preview → materialize → survey. Preserve emitted manifest and survey receipts.
5. Run the approved Unreal/NeoStack materialization handoff and load `Lvl_Gloamstead`; inspect generated-output ownership/path/provenance.
6. Run the Cycle II playtest protocol. Archive system proof and experience evidence separately.
7. Before PR/merge, run the repository-declared full gate again on the final exact commit. No cook/package retry is included here; the prior cook authorization is consumed and requires a fresh human authorization.

## 10. Failure and recovery rules

- **Missing plan, warning, or evidence:** do not start a generic night. Keep the rest action unavailable with a player-readable reason and log the invalid plan ID.
- **Runtime objective failure:** fail forward to the defined corruption scar/outcome; never trap the player in Night or silently reset the garden.
- **WorldForge invalid/pre-mutation failure:** preserve source spec and receipt, leave map/generated output untouched, correct generic tooling or Gloamstead spec in its owning worktree.
- **Materialization/map-load failure:** revert only the controlled generated output via the approved automation rollback; do not delete broad Content folders or hand-edit binaries.
- **WorldForge state reset:** reconstruct the mirror from Gloamstead PCG/restoration state, then re-survey; never treat mirror reset as gameplay progress loss.
- **Playtest comprehension failure:** record the missed channel and repair the weakest support/feedback before adding Cycle III. Do not compensate by merely making the warning literal.

## 11. Observability and evidence

Every active plan must log/record: plan ID, cycle slot, selected type, warning ID, subject ID, support IDs offered, restoration event ID, projection value, runtime outcome, and dawn summary key. These are diagnostic receipts, not player-facing debug prose.

The final evidence folder has distinct records for:

- SYSTEM_CORRECT: gate commit, targeted test names/results, semantic validation, WorldForge manifest, dual-state surveys, map-load evidence.
- EXPERIENCE_WORKS: playtest script, anonymized causal-answer table, footage/screenshots where consented, comprehension result, known failures, and follow-up decision.

## 12. Integration boundaries

- This branch begins atop `feat/authored-sanctuary-environment`, which is ahead of `main`. Do not open a Cycle II PR to `main` that accidentally contains the authored-environment dependency without first reconciling that dependency. Either land/rebase the prerequisite intentionally or use a review base that makes the dependency explicit.
- Generated content is not a substitute for tracked source spec/manifest/survey evidence.
- Any WorldForge code changes receive their own branch/PR/merge lifecycle in the WorldForge repository. Gloamstead receives only its reviewed inputs and controlled generated outputs.
- The existing `scripts/worldforge_caller/gloamstead_consumer.py` remains a narrow first-night wayfinding caller until a reviewed replacement consumes the typed Cycle II spec. Its hard-coded commit identity is not acceptable provenance for this carrier.

## 13. Risks and stop conditions

| Risk | Mitigation / stop condition |
| --- | --- |
| Cycle II becomes a tutorial replay | Detachment test and exact warning/outcome assertions must be green before map work. |
| Fairness becomes metadata theatre | Require runtime support receipts and cold-start causal explanations. |
| WorldForge invents semantic content | Reject any generated spec/output with new place/lore/goal identifiers absent from the Gloamstead spec. |
| Generated output creates opaque binary churn | Use controlled root, manifest, survey, map load, and approved automation only. |
| WorldForge mirror becomes authoritative | Directionality tests and rebuild-on-reset rule. |
| Save gap misrepresents a persistent arc | Resolve section 16 before claiming quit/resume readiness. |
| Presentation regressions remain hidden by tests | Human map-load and caption/accessibility review are mandatory. |
| Scope expands into combat/horde/survival systems | Reject any task whose primary verb is not investigation/restoration/interpretation. |

## 14. Definition of done

Cycle II is done only when all are true:

1. A fresh run reaches Cycle II after Cycle I without the tutorial actor controlling it.
2. The second Day presents `GardenRot` by exact ID before rest; the second Dusk reuses that same `Corruption` plan.
3. A player can encounter at least two support channels before choosing to rest.
4. Restoring or neglecting the named garden makes a visible, mechanically meaningful difference to Corruption and dawn.
5. WorldForge-generated foliage, ruins, paths, and lighting/material state demonstrably respond to Gloamstead-projected `restoration_level`, with clean provenance and post-materialization survey.
6. `Lvl_Gloamstead` loads with only approved generated outputs/references.
7. `./gate.ps1` and all hostile tests pass on the exact final commit.
8. The cold-start playtest protocol passes; SYSTEM_CORRECT and EXPERIENCE_WORKS receipts both exist.
9. No claim is made that Cycle III or persistence-resume is delivered by this carrier.

## 15. Follow-on sequence after acceptance

Only after Cycle II passes its playtest, begin Cycle III (Omen/Retrieval) using the same plan/evidence/WorldForge receipt shape. Cycle IV introduces possession plus light combat pressure only when Cycle III demonstrates that warnings and place evidence remain legible. Cycles V–VI must not be implemented as a bulk content dump.

## 16. Required human decision before implementation proceeds past a continuous-session slice

The existing save path persists PCG state at dawn, but the authored cycle slot/outcome history is not established as durable progression. The project workflow marks save-schema migrations as an explicit decision. Choose one before a claim of resumable Cycle II progression:

- **Approve a versioned save migration now:** add cycle slot, active/last plan IDs, outcome/scar state, and migration tests. This makes quit/resume part of Cycle II's definition of done.
- **Defer save migration deliberately:** Cycle II is tested as a continuous session only; the UI labels resume/progression as unavailable, and persistence becomes the next fenced carrier before Cycle III.

The recommended choice is the first option. It preserves the user's stated persistence requirement and prevents an apparently complete second night from becoming a restart-only demo. It is intentionally not assumed by this document.
