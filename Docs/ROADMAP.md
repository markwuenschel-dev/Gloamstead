# Gloamstead Roadmap

**Status as of 2026-06-17.** This is a *map and a sequence*, not a build order to execute blind.
Each item carries a **trigger** — the condition that earns the right to build it. Tests/checks get
written when the logic already exists on disk (ground what shipped) or when a failure class recurs;
features get built when they're a real MVP gap; flavor/docs when they unblock authoring.

Source of truth for humans *and* coding agents is markdown — so the automation gate is documented
here (it was previously untracked, so agents couldn't know the oracle existed).

---

## The oracle: `gate.ps1`

`gate.ps1` (repo root) is the authority for checkable work:

1. Builds `GloamsteadEditor Win64 Development` (UBT exit code is the build oracle).
2. Runs `UnrealEditor-Cmd Automation RunTests Gloamstead` and **parses the report, failing closed**
   (no report / zero tests / any non-`Success` ⇒ `GATE FAIL`).

**Current: GATE PASS — 16 tests green.** Filter is `Gloamstead`, so any `Gloamstead.*` test runs
automatically; new tests need no gate change.

| Test file | Module | Covers |
|---|---|---|
| `GloamsteadEditor/Private/Tests/SpineSmokeTest.cpp` | Editor | harness liveness (`Gloamstead.Spine.Smoke`) |
| `Gloamstead/Tests/PCGSubsystemTests.cpp` | Runtime | empty-state safe defaults, corruption clamp, spread preserves restored flags |
| `Gloamstead/Tests/PCGStateTests.cpp` | Runtime | restored-set round-trip, exact aggregate math, `ApplyRestoration` mutation, **full SaveGame round-trip** |
| `Gloamstead/Tests/NightBrainTests.cpp` | Runtime | MVP catalog contents, night-selection determinism / forced-tutorial / empty-catalog fallback, Veil Heart warning matching (catalog / ritual-name fallback / no-match / no-catalog) |

**Test seams** (let tests reach internal state without a world/PCG init): `UGloamsteadPCGSubsystem::Test_SeedPointStates` / `Test_PeekPointStates` and `UNightConsequenceManager::Test_SelectNightType`. They are unconditional inline (unused in shipping → linker emits nothing); the test `.cpp` files stay `#if WITH_DEV_AUTOMATION_TESTS`, so test code compiles out of shipping (verified: Shipping build of the `Gloamstead` target excludes them).

---

## Track A — Ground the shipped logic

Convert Phase 0–2 code that rests on manual PIE + Critic sign-off into gate-enforced trust.

| Batch | Scope | Status |
|---|---|---|
| 1 | PCG invariants (empty defaults, corruption clamp, restored-flag preservation) | **Done** |
| 2 | Restored-set round-trip + exact aggregates + `ApplyRestoration` mutation | **Done (2026-06-17)** |
| 3 | Night-selection determinism, MVP catalog contents, Veil Heart warning matching | **Done (2026-06-17)** |

**Known gaps surfaced while grounding (not fixed — residual/feature):**
- `GetNightConsequenceTypeDisplayName` and `GetRitualTypeDisplayName` only cover the 3 MVP types; all
  others (incl. MirrorPillar/BellShrine) return `"Invalid"`. The Veil Heart ritual-name fallback for
  those types is therefore latent-broken until the display names are extended.
- `GetRestoredCountByRitualType` / per-ritual snapshot counts depend on `CachedPoints` metadata, which
  the `PointStates`-only seam doesn't populate — not assertable until a `CachedPoints` seam exists.

---

## Track B — PCG init → playable (editor-bound; NOT gate-enforced)

Frozen at editor checklist step 6. **This is binary-asset, editor-GUI work — it cannot be done or
verified headlessly by the loop.** Runbook for a human:

1. Open `Content/Maps/Lvl_ThirdPerson` (or the active sanctuary map) in the Unreal Editor.
2. Add/confirm a PCG graph + `UPCGComponent` that emits ritual points with the metadata attributes the
   subsystem reads: `RitualType` (int), `bIsRestored` (bool), `LightLevel` (float), `CorruptionLevel`
   (float), and optionally `PathSegmentID`/`PathPosition` (see `GloamsteadPCGSubsystem.cpp` getters).
3. At level start (Level Blueprint `BeginPlay` or a placed actor), get the world subsystem
   `UGloamsteadPCGSubsystem` and call `InitializeFromPCGComponent(PCGComponent, WorldSeed)` **after**
   the PCG graph has generated (it reads `GetGeneratedGraphOutput`).
4. PIE: restore a ritual, advance to dusk (`Advance Gloamstead Day Phase`), confirm a non-Tutorial night
   + dusk warning fire.

**Payoff once green:** a full end-to-end functional test on real point data — the integration oracle
that retires a class of manual PIE checks. Trigger: real playability blocker, justified now.

---

## Track C — MVP features, sequenced by checkability

| # | Feature | Trigger | Status |
|---|---|---|---|
| 1 | **Save/load (full per-point state)** | real gap + invariant ready | **Done (2026-06-17)** |
| 2 | Night-type expansion (Retrieval, Possession, Mirror, Bargain, Fracture, TrueSiege) | per-type, when authored | gated |
| 3 | Ritual expansion (MirrorPillar, BellShrine) + alter/undo restoration | when slice needs more than lanterns/gardens | gated |
| 4 | Resource model (night yield, interpretation bonus, baseline) | when restoration needs a cost | gated |
| 5 | Combat pressure (move/strike/ward-cleanse/interact) | when a night type needs enforced urgency | gated |
| 6 | Journal + structured dawn rewards | when feedback is the playtest weak link | gated |

**#1 (done):** `UGloamsteadSaveGame` (`Source/Gloamstead/Save/`) captures the full `PointStates`
(light + corruption + flags) + restored set + seed; `CaptureToSaveGame`/`RestoreFromSaveGame` give a
true round-trip (vs. `ReapplyRestoredState`, which only re-flips flags), with `SaveToSlot`/`LoadFromSlot`
convenience wrappers. Invariant `Gloamstead.PCG.SaveGameFullRoundTrip` asserts capture→restore equality.
`FRitualPointState` fields are now `UPROPERTY()` for reflection-based serialization.

---

## Track D — Manufactured oracles (deferred: data model not yet present)

These would turn design judgment into gate-enforceable checks, but **their inputs don't exist on disk yet**,
so building them now would mean inventing the data model speculatively. Prerequisites first:

- **Fair Crypticism validator** (highest value): requires per-warning *support-channel* fields
  (environmental clue, prior pattern, restored-object reaction, audio cue, enemy behavior, readable
  consequence, dawn feedback). `FVeilHeartWarningFragment` currently has only `WarningId`, `Fragment`,
  `AssociatedNightType`, `SatisfiableTags`, `ClarityTier`. **Trigger:** add the support-channel fields
  (likely alongside night-type/warning authoring volume), then the check counts non-empty channels and
  fails any warning with < 2.
- **Catalog coverage validator:** needs a tag→ritual producibility map (which rituals can satisfy which
  tags) — not encoded. The cheap, groundable subset (no `Invalid` night types, `min ≤ max` ranges,
  non-empty `SatisfiableTags`) can be a structural automation test now if a wiring gap recurs.
- **Data-asset validator** (`UEditorValidatorBase`, GloamsteadEditor module): every `ERitualType` has a
  `URitualDefinition` or documented default — needs the placement component's type→definition map.
  **Trigger:** first missing-DA bug.

---

## Documentation & lore (load-bearing flavor)

**Catch-up docs needed:** a persistence/save spec (now that Track C #1 landed), a buildable combat/threat
spec (`04_combat_and_interaction_system.md` is still intent, not spec), and this testing-approach record
(done — see top).

**Lore that blocks content authoring** (warnings, dawn lines, night flavor can't be written coherently
until resolved; flagged unresolved in `Docs/questions/`):
- **What is the Heart?** (working blend: alchemical memory-seed / transformed Gloam-piece) — sets every fragment's voice.
- **What is the Gloam?** (working answer: "unremembering") — shapes night-consequence flavor.
- **Naming:** "Veil Heart" is a working term; do a naming pass before UI/journal text is written.
- **"Can the Heart be wrong?"** Locked: no — cryptic but never false. Load-bearing for Fair Crypticism (truthful Heart ⇒ every warning must be inferable).
- **Endings:** seeds only, but write down the axes (benevolent/possessive, lucid/confused, bound/resisting) so restoration choices know what they feed.
- **Heart's voice guide:** once the Heart's nature is picked, turn the 4 example fragments into an authoring guide (cadence, clarity-tier progression, fragment→night-rule mapping).

---

## Execution model (open question)

The `agent_collab` roster (orchestrator + coder + critic) was drawn *before* `gate.ps1` existed. With a
real oracle now in hand, the principled revision is to **collapse the Critic to the residual** (taste,
design, fun, lore resonance) and let the gate carry everything checkable — re-derive the roster against
the new oracle rather than carry the old math forward. Decision pending.
