# Corrected Wave 2 — Real Night Consequence Runtime (Plan)

**Branch:** `gloamstead/w2-night-runtime` (off `main` @ `ae2c7b3`)
**Baseline:** `gate.ps1` GREEN this session (build + 26 tests) on the identical tree; editor closed.
**Status:** planning locked; implementation in dependency order.

## Design decisions (confirmed with human 2026-07-11)

1. **Pressure model = Hybrid.** Subsystem-simulated corruption escalation is the *testable spine* (proven headlessly through `gate.ps1`); plus **one optional, minimal light-reactive pressure actor** (`ANightPressureActor`) for PIE feel. The actor is **not** required for the gate and does **not** depend on `Variant_Combat` — it is a plain `AActor` with a light-modulated "menace" scalar, no AI/navmesh/combat.
2. **First objective = Cleanse a corruption bloom before dawn.** A target point's corruption escalates through the night; the player restores/cleanses it. Resolution: **Success** (cleared/resolved before dawn), **Partial** (reduced but not cleared), **Failure** (untouched → fail-forward scar). Tutorial night = the same shape, bounded + always-winnable (teaching beat).
3. **Night spine = UObject Blueprint-extensible strategies.** `UNightStrategy` (Blueprintable, abstract) base with `UNightTutorialStrategy` / `UNightCorruptionStrategy`. The runtime owns lifecycle + context + outcome and delegates per-type behavior to a strategy instance. New night types = new strategy, no loop rewrite.

## Architectural shift

```
Night starts with CONTEXT   → FNightRuntimeContext (selected type + dusk snapshot + target)
Night applies PRESSURE      → strategy escalates corruption on the target (timer-driven)
Night tracks OBJECTIVE      → strategy observes restoration; resolves early on cleanse
Night ends INTENTIONALLY    → objective resolved (early) OR deadline; NOT a bare timer skip
Dawn receives OUTCOME       → FNightRuntimeOutcome → VeilHeart::ProcessDawnReflection(outcome)
Save/load preserves state   → PCG per-point corruption mutation already round-trips (verified)
```

## File ownership / build order

1. `Source/Gloamstead/Data/NightRuntimeTypes.h` (NEW) — `ENightObjectiveKind`, `ENightOutcomeResult`, `FNightRuntimeContext`, `FNightObjective`, `FNightRuntimeOutcome`.
2. `Source/Gloamstead/Systems/NightStrategy.h/.cpp` (NEW) — `UNightStrategy` base + `UNightTutorialStrategy` + `UNightCorruptionStrategy`.
3. `Source/Gloamstead/Systems/NightConsequenceRuntime.h/.cpp` (EXTEND) — context build, strategy instantiation, pressure timer, restoration observation, outcome capture, `OnNightShouldEnd` early-end delegate, `GetLastOutcome()`. Keep existing delegates + `ExecuteNightStub` removed/replaced.
4. `Source/Gloamstead/Systems/NightPressureActor.h/.cpp` (NEW) — optional hybrid actor; light-reactive menace; no combat dep.
5. `Source/Gloamstead/Systems/VeilHeart.h/.cpp` (EXTEND) — `ProcessDawnReflection(const FNightRuntimeOutcome&)`; keep no-arg for BP compat; record last outcome so the next cycle can see it.
6. `Source/Gloamstead/Systems/GloamsteadDayNightSubsystem.cpp` (EXTEND) — at dawn, pull `Runtime->GetLastOutcome()` and pass to the Heart.
7. `Source/Gloamstead/Systems/GloamsteadFirstNightDirector.h/.cpp` (EXTEND) — listen to `OnNightShouldEnd`; advance to dawn early (clear the duration timer). Timer remains the deadline bound.
8. `Source/Gloamstead/Tests/NightRuntimeTests.cpp` (NEW) + extend continuity coverage — strategy resolution, outcome plumbing, corruption-night mutation persists through save/load.

## Verification profile

- **Headless (gate.ps1):** all runtime/strategy/outcome/continuity logic. This is the authoritative proof for Wave 2.
- **Human PIE (optional, follow-up):** live feel of `ANightPressureActor` + full dusk→night→dawn with a resolved objective on real hardware.

## Save scope

Night **outcome/scar is session-only this wave** (documented). The *meaningful* sanctuary mutation — per-point corruption raised by the night — already persists via the existing `SaveToSlot` per-point state. No save-format expansion (stays out of the High-Risk save-format gate).

## Non-goals (this wave)

No horde/loot/full-combat; no GloamsteadForge contracts; no UI/journal/VFX/audio; no engine/plugin/EngineAssociation changes; no `.uasset`/`.umap` hand-edits; no vendor content changes.
