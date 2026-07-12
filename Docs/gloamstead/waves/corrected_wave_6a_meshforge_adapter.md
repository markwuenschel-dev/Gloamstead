# Corrected Wave 6A — Gloamstead MeshForge Adapter

**Status:** source adapter implemented; `gate.ps1` green (build + 49 automation tests + GloamsteadForge evidence). **PIE readability: PENDING — editor not open** (the editor crashed mid-session; the automated live-world test is the current proof of visibility).

**Branch:** `gloamstead/wave-6a-meshforge-adapter`

---

## 1. What this wave is (and is not)

The Gloamstead loop already works, but the player cannot *see* it: the Veil Heart is logic-only, ritual points are PCG data, and night/phase feedback is numeric. Wave 6A adds a **source-first visibility adapter** that observes the real gameplay systems read-only and renders a visible proxy for each, through a **replaceable provider seam**.

- The first (and only) provider in this wave is `engine_primitive_runtime_proxy` — engine `BasicShapes` meshes spawned at runtime, tinted with dynamic material instances. It is **code-owned**; it authors nothing and touches no binary content.
- The seam is designed so a future `generated_owned_meshforge_asset` provider can replace the visuals **without changing the source-binding logic** (see §6).

**This wave is not:** final art, an environment pass, a HUD, a MeshForge asset-generation pipeline, or any generated-owned assets. No `.uasset`/`.umap`/binary content was created or edited.

---

## 2. Architecture & file map

Three layers, all in the `Gloamstead` runtime module:

| Layer | Responsibility | Files |
|---|---|---|
| 1. Contracts / data model | Proxy specs, source bindings, provider descriptors, proxy instances, visibility report; token + fail-closed validators; JSON report writer | `Source/Gloamstead/Data/GloamsteadMeshForgeTypes.{h,cpp}` |
| 2. Provider seam + engine-primitive provider | Abstract `UGloamsteadMeshForgeProvider`; `UGloamsteadEnginePrimitiveMeshForgeProvider`; `AGloamsteadMeshForgeProxyActor` (the visible body) | `Source/Gloamstead/Systems/GloamsteadMeshForgeProvider.{h,cpp}` |
| 3. Runtime binding + spawner | `UGloamsteadMeshForgeAdapterSubsystem` (`UWorldSubsystem`): discovers sources, builds bindings, drives the provider, reacts to source delegates, emits the report | `Source/Gloamstead/Systems/GloamsteadMeshForgeAdapterSubsystem.{h,cpp}` |
| Tests | Contract/overclaim, report validation, live-world spawn + no-mutation proof | `Source/Gloamstead/Tests/MeshForgeAdapterTests.cpp` |

Supporting change: `UGloamsteadPCGSubsystem` gained a read-only `GetRitualPointCount()` and a `Test_SeedPoints(...)` test seam (+17 lines). No authority moved.

---

## 3. Provider honesty contract

Every proxy and the aggregate report carry explicit, validated provenance. For this wave the values are fixed:

```
provider_type       = engine_primitive_runtime_proxy
ownership_class     = code_owned_runtime_proxy
runtime_only        = true
generated_asset_path = null
binary_content_touched = false
```

The validators in `GloamsteadMeshForgeTypes.cpp` **fail closed** on any overclaim — e.g. an engine-primitive provider that names an ownership class of `generated_owned` (`GMF014`), a runtime-only proxy that carries a generated asset path (`GMF015`), or a report missing Heart/ritual coverage (`GMF010`/`GMF011`). These are proven by `Gloamstead.MeshForge.ContractsRejectOverclaim` and `...ReportValidationFailsClosed`.

---

## 4. Source bindings (read-only)

The adapter **only observes**. Gameplay authority stays where it is: the PCG subsystem owns ritual-point state, the placement component builds payloads, the day/night subsystem owns phase, the Veil Heart owns warnings/reflection, the night runtime owns nights.

| Proxy | Source system | How bound (read-only) |
|---|---|---|
| Heart | `AVeilHeart` | `GetAllActorsOfClass` → `[0]`; `GetActorLocation()` |
| InteractionRadius | `AVeilHeart` | flat disc at the Heart |
| RitualPoint / LanternRestore | `UGloamsteadPCGSubsystem` | iterate `0..GetRitualPointCount()`; `GetPointByIndex`, `IsPointRestored`, `GetCorruptionLevel`, ritual type via `GetIntAttribute(Point,"RitualType")` |
| NightFeedback | `UGloamsteadDayNightSubsystem` | re-tints on `OnPhaseChanged(Old,New)` |
| (restoration reaction) | `UGloamsteadPCGSubsystem::OnStructureRestored` | restored point turns green in `HandleStructureRestored` |

Colour language: Heart = warm gold; restorable lantern = amber; restored = green; corrupted (≥0.5) = purple; interaction radius = cyan; night feedback = phase-coloured. Corruption is made visible **per point** (a ritual point turns purple) rather than as a separate proxy type.

The adapter reacts to source **delegates** (`OnPhaseChanged`, `OnStructureRestored`) — it never calls a mutator (`ApplyRestoration`, `AdvanceToNextPhase`, `SetPhase`, `BeginNight`). The live-world test asserts PCG point state is byte-for-byte unchanged after a full build (`adapter did not mutate gameplay state`).

---

## 5. Reports (regenerated evidence — not committed)

`procedural/reports/` is **git-ignored** (repo convention; the GloamsteadForge reports work the same way). The three MeshForge reports are **regenerated by `gate.ps1`** — the live-world automation test builds a real 6-proxy sanctuary and calls `EmitReport`:

```
procedural/reports/gloamstead_meshforge/visibility_proxy_report.json   (proxy_count 6, failure_codes [])
procedural/reports/gloamstead_meshforge/provider_report.json
procedural/reports/gloamstead_meshforge/source_binding_report.json
```

> **Wave-6A hardening:** the subsystem is created for *every* game world, including unrelated automation worlds with no sanctuary. Those previously overwrote the shared report with an empty, coverage-failing artifact (`proxy_count 0`, `GMF010/GMF011`). The adapter now **only emits when it actually produced proxies** (`OnWorldBeginPlay` / `RetryRitualProxies` guard on `Proxies.Num() > 0`), so the persisted report deterministically reflects a real Heart + ritual build.

Regenerate at any time with `pwsh -NoProfile -File gate.ps1`.

---

## 6. Future generated-owned provider seam

To add real generated assets later, implement a second `UGloamsteadMeshForgeProvider` subclass. **No source-binding code changes** — the adapter only calls `GetDescriptor()` and `CreateProxy(Spec, Binding, World)`.

A `generated_owned` provider must declare, and the validators will require:

```
provider_type        = generated_owned_meshforge_asset   (EGMFProviderType::GeneratedOwnedMeshForgeAsset)
ownership_class      = generated_owned                    (EGMFOwnershipClass::GeneratedOwned)
supports_generated_assets = true
runtime_only         = false          // per instance
generated_asset_path = /Game/...      // per instance, non-empty and real
```

Validator rules already enforced for that path: a `generated_owned` instance with **no** asset path → `GMF015`; a `generated_owned` instance that is also `runtime_only` → `GMF015`. So a future provider cannot claim generated ownership without a real asset behind it.

**Human / editor gate required for the future provider:** creating `/Game/...` assets is binary content and must go through the editor/NeoStack/human review — it is explicitly **out of scope** for any source-only wave. Wave 6A is `code_owned_runtime_proxy` **only**.

---

## 7. PIE readability checklist — **PENDING (editor closed)**

The automated live-world test proves proxies spawn visibly and bind to real sources without mutating state. Human PIE readability is **not yet performed** because the editor is closed. When the editor is open, verify:

- [ ] Heart is visible (gold pillar) and you can walk to it
- [ ] At least one restoration target is visible (amber beacon) and you can walk to it
- [ ] Restore by hand (Restore input → placement → confirm); the point turns green
- [ ] Rest by hand at the Heart (Interact) advances the resting phases
- [ ] Complete Day → Dusk → Night → Dawn **without the T debug key**
- [ ] Night/phase feedback changes visibly (the night-feedback sphere re-tints)

---

## 8. Honest caveats

- This wave builds the **MeshForge Adapter substrate**. The first provider is `engine_primitive_runtime_proxy`.
- **No `generated_owned` assets** are created; **no binary content** is edited. Not final art, not a MeshForge asset-generation pipeline.
- The provider seam is designed for a future generated-owned provider (§6).
- **PIE readability is PENDING** — it is only "passed" when actually tested in-editor.
- **The "T" key is a debug shortcut** (`IA_DebugAdvance` → `AdvanceGloamsteadDayPhase` → `AdvanceToNextPhase`) that jumps the whole Day→Dusk→Night→Dawn cycle in one press. Wave 6A makes the *intended* loop visible so T is unnecessary; it does not remove the debug key.
- **The day/night cycle has no self-advancing clock.** During the scripted first night the `AGloamsteadFirstNightDirector` auto-advances on lantern restoration + timers (≈4s dusk→night, ≈8s night→dawn); the recurring loop advances via a Heart "rest" interaction. Making the loop *auto-run* is gameplay authority and is deliberately **outside** the adapter's scope (would trip `GMF017`).
- `ritual_type` provenance is read from PCG point metadata; it reads `Invalid` for synthetic test points that carry no `RitualType` attribute (honest), and resolves to the real type in a PCG-initialised world.
