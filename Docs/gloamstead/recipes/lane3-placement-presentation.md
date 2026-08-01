# Lane 3 — Placement Presentation Layer

**Assets specified:** `BP_RitualPlacementComponent`, `BP_RitualPreview`
**Content root:** `Content/Gloamstead/Placement/` → package root `/Game/Gloamstead/Placement/`
**Native parent:** `URitualPlacementComponent` (`/Script/Gloamstead.RitualPlacementComponent`)
**Status:** SPECIFICATION. The script in §5 has **not been executed** — Lane 3 holds no editor access.

---

## 0 · Verification ledger

Everything load-bearing below was read this session. Anything not read is labelled `[assumed]` inline.

> **Citations are against the working tree, not `HEAD`.** `RitualPlacementComponent.h` and `.cpp` carry **uncommitted modifications** made by another lane *during* this session (the tree was clean when Lane 3 started). An earlier read of both files went stale mid-task; every line number below was re-verified against the current working tree **after** that change. If that lane edits again, re-verify before trusting these numbers. The changed surface is F27.

| # | Fact | Evidence |
|---|---|---|
| F1 | The Blueprint-facing surface is six `BlueprintImplementableEvent`s and **ten** `BlueprintPure` getters — five placement getters plus the five `Ritual|Evidence` getters added by the concurrent edit (F27). | `Source/Gloamstead/Components/RitualPlacementComponent.h:37-50` (placement), `:63-81` (evidence), `:97-113` (events) |
| F2 | `OnPreviewTargetChanged` fires **only when the resolved point index changes**, never on a validity change alone. | `RitualPlacementComponent.cpp:277` (`if (ResolvedIndex != CurrentTargetPointIndex)`), event at `:290` |
| F3 | Validity depends on **live player distance**, so it changes continuously while the index is constant. | `RitualPlacementComponent.cpp:328` (`Distance <= Radius * 1.25f`) |
| F4 | The target **rotation** depends on the player's *control rotation* on slopes steeper than `dot < 0.6`, so it changes as the player looks around with the index constant. | `RitualPlacementComponent.cpp:383-401`, branch at `:388` |
| F5 | `GetCurrentTargetPointInfo()` applies `VerticalOffset` along the terrain normal and the slope-aligned rotation. Raw PCG point transforms do **not** match it. | `RitualPlacementComponent.cpp:403-421`, offset at `:419` |
| F6 | `EnterPlacementMode()` sets `bIsInPlacementMode = true` **before** calling `UpdateTargetPoint()`, and resets the index to `-1` first — so entering emits at most one `OnPreviewTargetChanged`, and **zero** if no point is in range. There is no `OnPlacementModeEntered` event. | `RitualPlacementComponent.cpp:71` (flag), `:73` (index reset), `:79` (`UpdateTargetPoint`) |
| F7 | A **successful** confirm calls `ExitPlacementMode()`, which fires `OnPlacementModeExited`. Success and cancel therefore share one teardown path. | `RitualPlacementComponent.cpp:138-142`; `ExitPlacementMode` at `:82-89`, event at `:89` |
| F8 | Every **failure** path of `ConfirmPlacement()` returns `false` **without firing any event and without exiting placement mode**. | `RitualPlacementComponent.cpp:94, 95, 107, 122, 132` |
| F9 | `ExitPlacementMode()` early-returns when not in placement mode, so `OnPlacementModeExited` fires at most once per session. | `RitualPlacementComponent.cpp:84` |
| F10 | `SpawnRestoredActor` runs **before** payload construction and **before** the restoration commit; C++ destroys the returned actor if either fails. | `RitualPlacementComponent.cpp:114` (call), failure blocks ending `:122` and `:132` |
| F11 | `BuildRestorationPayload` accepts a **null** `RestoredActor` and still returns `true` — returning null from `SpawnRestoredActor` does **not** abort restoration. | `RitualPlacementComponent.cpp:352` (assignment), `:380` (`return true`) |
| F12 | Duplicate-confirm is prevented natively at three layers: the mode check, the `IsPointRestored` pre-check, and `ApplyRestoration` itself refusing an already-restored point. | `RitualPlacementComponent.cpp:94`, `:107`, and the comment at `:156-159` citing `GloamsteadPCGSubsystem.cpp:296-300` |
| F13 | `UActorComponent::TickComponent` dispatches `ReceiveTick` **only for Blueprint-compiled classes** — which `BP_RitualPlacementComponent` is. | `D:/UE_5.8/Engine/Source/Runtime/Engine/Private/Components/ActorComponent.cpp:1882-1888` |
| F14 | `URitualPlacementComponent::TickComponent` calls `Super::TickComponent` **unconditionally at `cpp:47`**, before its `!bIsInPlacementMode` early-return at `cpp:49`. Blueprint `Event Tick` therefore fires every frame, in and out of placement mode. | `RitualPlacementComponent.cpp:45-49` |
| F15 | `ReceiveEndPlay(EEndPlayReason)` exists on `UActorComponent` and fires for BP-compiled components unless the object is already being garbage-collected. | `ActorComponent.h:958-959`; `ActorComponent.cpp:1680-1684` |
| F16 | `M_Preview_Valid` and `M_Preview_Invalid` exist at `Content/Input/` → `/Game/Input/M_Preview_Valid`, `/Game/Input/M_Preview_Invalid`. They are the only assets in `Content/` whose name contains "Preview" — **no preview actor Blueprint exists yet**. | `find Content -iname "*preview*"`, this session |
| F17 | `Content/Gloamstead/Placement/` does **not** exist. `Content/Gloamstead/` contains only `Blueprints/BP_VeilHeart.uasset`. Both target assets are new. | `ls -R Content/Gloamstead`, this session |
| F18 | `SM_Cube`, `SM_Cylinder`, `SM_ChamferCube`, `SM_Plane`, `SM_Ramp`, `SM_QuarterCylinder*` exist under `Content/LevelPrototyping/Meshes/`. | `ls Content/LevelPrototyping/Meshes`, this session |
| F19 | `ERitualType` is a `uint8` `UENUM(BlueprintType)`: `Invalid=0, LanternPost=1, GardenBed=2, PathPoint=3, MirrorPillar=4, BellShrine=5`. `FRitualPointInfo` fields: `PointIndex, bIsValid, Location, RitualType, Rotation`. | `Source/Gloamstead/Data/RitualTypes.h:12-20, 94-110` |
| F20 | `UGloamsteadPCGSubsystem::GetPointByIndex` is `BlueprintCallable`, but it returns the **raw** point — it does not apply F5's offset/alignment. Blueprint must not use it to place the restored actor. | `Source/Gloamstead/PCG/GloamsteadPCGSubsystem.h:52-53` |
| F21 | Both asset names are **pre-existing design commitments**, not new coinages: `BP_RitualPlacementComponent` is named as the Blueprint child, and `BP_RitualPreview` as the ghost/reticle hooked to `OnPreviewTargetChanged`. | `docs/Phase1.5_PlacementComponent.md:9`; `docs/Phase3_SixHourExperience.md:118` |
| F22 | Prior design already fixes two rules this spec must honour: previews are **independent world actors, not attached to the player**, and the transform comes from `GetCurrentTargetPointInfo()`. Both match §3–§4 below. | `docs/Phase1.5_PlacementComponent.md:29-30` |
| F23 | **Conflict.** Prior design specifies a *single* preview material driven by parameters — `ValidState` (1.0/0.0), `EmissiveIntensity` (2.5/0.6), `BaseColorTint`, `NiagaraSpawnRateMultiplier` — not the two-material swap this lane was briefed to use. See §4.3. | `docs/Phase1.5_PlacementComponent.md:31, 70-77` |
| F24 | `execute_script` has a hard **60-second tool ceiling**, and the first PIE launch after a fresh build can stall the editor's MCP bridge on shader compilation. The §5 script must be chunked accordingly. | `agent_collab/logs/decisions.md:591` |
| F25 | Creating `/Game/...` binary assets is explicitly human/editor-gated and out of scope for a source-only lane — which is why Lane 3 delivers a script, not assets. | `docs/gloamstead/waves/corrected_wave_6a_meshforge_adapter.md:101` |
| F26 | `M_Preview_Valid` / `M_Preview_Invalid` are referenced by **no** doc and **no** source file in the repo, and they sit in `Content/Input/` beside `BP_BuildPlacementComponent` — i.e. they belong to the generic build/placement system, not the ritual system. Treat their parameter surface as unknown until opened. | repo-wide grep, this session |
| F27 | **Concurrent edit (uncommitted).** Another lane added a `Ritual|Evidence` surface: five new `BlueprintPure` getters (`GetLastEvidenceRequestId`, `GetLastEvidenceReportPath`, `GetLastEvidenceFailureCodes`, `WasLastEvidencePublished`, `GetLastEvidencePointIndex`), and `ConfirmPlacement` now calls `CommitRestorationWithEvidence` instead of `ApplyRestoration` directly. **The six BIEs and the five placement getters are unchanged in name, signature and order** — only their line numbers moved. The confirm-path control flow this spec depends on (F7–F12) is preserved. | `RitualPlacementComponent.h:52-94`; `.cpp:92-170` |
| F28 | The new getters are declared as "read-only diagnostics **for a HUD**", explicitly so a HUD never reaches into `UGloamsteadSurveySubjectRegistry`. They are therefore a presentation surface and belong in this lane's contract — see §2.4. | `RitualPlacementComponent.h:52-61` |
| F29 | The five evidence fields are written **as a block** on every restored confirmation and are never touched by a refused one, so a HUD may render them as one snapshot without cross-confirmation tearing. | `RitualPlacementComponent.cpp:178-182`, and the header contract at `.h:54-56` |
| F30 | Evidence publication **cannot roll back gameplay**: `CommitRestorationWithEvidence` returns whether the *restoration* succeeded, never whether the report was published. A failed publish is loud (Error log + the getters) and nothing else. | `RitualPlacementComponent.cpp:165-170`; contract at `.h:125-132` |

### Consequences that drive the whole design

- **F2 + F3 + F4 ⇒ event-driven presentation is insufficient.** Validity and rotation both drift while the index is unchanged, and no event fires. Material state and preview transform **must be polled on `Event Tick`**. F13/F14 confirm `Event Tick` is available and unconditional.
- **F8 ⇒ the Blueprint cannot observe a failed confirm through any of the six events.** The failure hook must live in the wrapper that *calls* `ConfirmPlacement` and reads its `bool` return. This is why §2 introduces `TryConfirmPlacement`.
- **F7 ⇒ one teardown site covers both cancel and success**, because C++ routes success through `ExitPlacementMode`.

---

## 1 · Ownership boundary

| Blueprint MAY own | Blueprint MUST NOT own |
|---|---|
| Preview actor spawn / destroy / re-use | Point selection authority (`UpdateTargetPoint`, `ResolveTargetForPlacement`) |
| Preview transform updates | Restoration state (`ApplyRestoration`, `IsPointRestored`) |
| Valid / invalid material state | Payload construction (`BuildRestorationPayload`) |
| Audio (target-acquired, denied, confirmed) | Confirmation success (`ConfirmPlacement`'s return value is authority) |
| Restrained VFX (a pulse, a fade — no gameplay-readable signal) | Duplicate-confirm prevention (F12) |
| Presentation callbacks after C++ succeeds or fails | Subject resolution (which point, which ritual type) |
| Reading the `Ritual|Evidence` getters in a **debug HUD** (§2.4) | Evidence publication, retry, or its success/failure semantics (F30) — and neither of these two assets reads those getters at all |

**Enforcement rule:** the Blueprint never writes a variable that any other system reads. Its only outputs are actor spawns it also destroys, material parameters, sounds, and the one `AActor*` it hands back through `SpawnRestoredActor` — which C++ owns from the moment it is returned (F10).

---

## 2 · Contract table — native surface → Blueprint behaviour

`Self` below is the `BP_RitualPlacementComponent` instance. `PreviewActor` is its single `BP_RitualPreview` object reference variable.

### 2.1 · BlueprintImplementableEvents (overrides)

| Native event (`.h` line) | Signature | What the Blueprint does | Must NOT do |
|---|---|---|---|
| `OnPreviewTargetChanged` (`.h:97-98`) | `(int32 PointIndex, ERitualType Type, bool bIsValid)` | **Branch on `PointIndex >= 0`.**<br>True → `EnsurePreview` (spawn iff `IsValid(PreviewActor)` is false) → `PreviewActor.SetPreviewType(Type)` → `RefreshPreviewTransform` → `ApplyValidState(bIsValid)` → play *target-acquired* cue **only if** the previous `CachedIndex` was `-1` (avoids a cue on every step between points).<br>False → `DestroyPreview`. | Re-derive the target, cache `PointIndex` as gameplay state, or spawn a second preview. The `IsValid` guard in `EnsurePreview` is the sole reason "at most one preview" holds. |
| `OnPathPointRedirected` (`.h:100-101`) | `(const FString& Message)` | Route `Message` verbatim to the HUD / on-screen tip channel; optional soft UI sound. Fires **once per placement session** natively (`cpp:309-313`), so no debounce is needed in Blueprint. | Rewrite, translate, or suppress the message — the string is authored in C++ (`cpp:311`). Change the target. |
| `OnPlacementConfirmed` (`.h:103-104`) | `(int32 PointIndex)` | Success presentation only: confirm sound, one-shot restrained VFX at `PreviewActor`'s transform (read it **before** teardown; `OnPlacementModeExited` fires later in the same call — F7). Set a local `bConfirmedThisSession = true` so `OnPlacementModeExited` can choose a *satisfied* teardown rather than a *cancelled* one. | Destroy the preview here. Teardown belongs to `OnPlacementModeExited` alone, so cancel and success converge on one code path. Mark anything restored. |
| `OnRestoredActorSpawned` (`.h:106-107`) | `(AActor* SpawnedActor, int32 PointIndex, ERitualType RitualType)` | Post-success dressing on the **durable** actor: attach a niagara one-shot, start its ambient audio, tag it for the day/night pass. Guard `IsValid(SpawnedActor)` — it is legitimately null when `SpawnRestoredActor` returned null (F11). | Assume this ran — it only fires after `ApplyRestoration` succeeded (`cpp:139`). Treat `SpawnedActor` as the preview. |
| `OnPlacementModeExited` (`.h:109-110`) | `()` | **The single teardown site.** `DestroyPreview` → stop placement-loop audio → clear `bWasInPlacementMode`, `CachedIndex = -1`, `bLastAppliedValid` unset, `bConfirmedThisSession = false`. Fires for cancel *and* success (F7); fires at most once per session (F9). | Branch on why it exited to decide whether to destroy. Destroy is unconditional; only the *audio* differs, keyed off `bConfirmedThisSession`. |
| `SpawnRestoredActor` (`.h:112-113`) | `(int32 PointIndex, AActor*& OutSpawnedActor)` | 1. Read `Info = GetCurrentTargetPointInfo()`.<br>2. Guard `Info.PointIndex == PointIndex` — on mismatch, log an error and return **null** (see the note below).<br>3. `SpawnActorFromClass(RestoredClassFor(Info.RitualType), MakeTransform(Info.Location, Info.Rotation))` with collision handling `AlwaysSpawn`.<br>4. Return it in `OutSpawnedActor`. Nothing else. | Register the actor anywhere, cache it, fire "restored" audio/VFX, or destroy the preview. C++ destroys this actor on any downstream failure (F10) — every side effect here would survive the rollback. Use `GetPointByIndex` for the transform (F20): it omits the vertical offset and slope alignment the preview used, so the restored actor would not land where the preview stood. |

**Mismatch note (`SpawnRestoredActor` step 2).** `FinalPointIndex` is `ResolveTargetForPlacement(CurrentTargetPointIndex)` (`cpp:106`), and `CurrentTargetPointIndex` is itself already a resolved index (`cpp:277-281`). `ResolveTargetForPlacement` returns non-`PathPoint` indices unchanged (`cpp:304`) and redirects `PathPoint`s to a `LanternPost` (`cpp:307, 315`) — so a resolved index is never a `PathPoint` and re-resolving is a no-op. `[inferred]` from that chain: the guard is unreachable in normal play. It is kept because the alternative failure — silently spawning a lantern at the wrong point — is unrecoverable, whereas returning null produces a point that restores with no actor (F11) and a log line naming both indices.

### 2.2 · BlueprintPure getters

| Native getter (`.h` line) | Where the Blueprint calls it | Why |
|---|---|---|
| `IsInPlacementMode()` (`.h:37-38`) | `Event Tick`, every frame | The **only** session-edge detector available — there is no enter event (F6). Rising edge ⇒ session start; the falling edge is already covered by `OnPlacementModeExited`, so Tick only acts on the rise. |
| `IsCurrentPlacementValid()` (`.h:40-41`) | `Event Tick`, only while `IsInPlacementMode()` | F3: validity flips with player distance and no event announces it. Compared against `bLastAppliedValid` so the material is swapped on change only, never every frame. |
| `GetCurrentTargetPointInfo()` (`.h:46-47`) | `SpawnRestoredActor`; `RefreshPreviewTransform` | The single source of the offset + slope-aligned transform (F5). Both the preview and the restored actor read it, which is what makes them coincide. |
| `GetCurrentTargetTransform(Location, Rotation)` (`.h:49-50`) | `RefreshPreviewTransform` (Tick path) | Convenience wrapper over the same data with a `bool` "is there a target" return (`cpp:424-431`) — a cheaper branch than testing `Info.PointIndex != -1`. F4: must run on Tick, not only on target change. |
| `GetCurrentTargetRitualType()` (`.h:43-44`) | Not called | `OnPreviewTargetChanged` already delivers `Type` as a parameter, and the getter rebuilds the whole `FRitualPointInfo` to return one field (`cpp:434-437`). Listed here so the omission is deliberate, not an oversight. |

### 2.3 · Engine events the Blueprint also overrides

| Event | Role |
|---|---|
| `Event Tick` (`ReceiveTick`, F13/F14) | Session-edge detection + validity poll + transform refresh. Fires unconditionally, including outside placement mode — the handler must early-out on `IsInPlacementMode() == false` after the falling-edge bookkeeping. |
| `Event End Play` (`ReceiveEndPlay`, F15) | `DestroyPreview`. Belt-and-braces for `Destroyed` / `RemovedFromWorld`. |

### 2.4 · The `Ritual|Evidence` getters (added mid-session by another lane — F27/F28)

Five `BlueprintPure` getters at `RitualPlacementComponent.h:63-81`, declared explicitly as "read-only diagnostics **for a HUD**" (`.h:52-61`). They are a **presentation** surface, so they fall inside this lane's boundary — but **not** inside these two assets.

| Getter | Returns | Presentation use |
|---|---|---|
| `GetLastEvidenceRequestId()` | `FString` | Correlation id to show beside a confirmation in a debug HUD. Empty until one restored confirmation has happened. |
| `GetLastEvidenceReportPath()` | `FString` | Absolute artifact path. Empty when the emission never reached disk. |
| `GetLastEvidenceFailureCodes()` | `TArray<FString>` | GSS codes; empty on a clean publish. |
| `WasLastEvidencePublished()` | `bool` | The only reliable "published vs never ran" discriminator — an empty path alone is ambiguous. |
| `GetLastEvidencePointIndex()` | `int32` | Correlation key back to the ritual point. |

**Rules for this lane:**

- **Read as one snapshot, never field-by-field across frames.** All five are written as a block on every restored confirmation and are untouched by a refused one (F29), so a single-frame read is internally consistent. Caching individual fields at different times would reintroduce the tearing the block write exists to prevent.
- **Never branch gameplay on them.** A failed publish does not roll back the restoration (F30); treating `WasLastEvidencePublished() == false` as "the placement failed" would invert the contract stated at `.h:125-132`.
- **They do not belong in `BP_RitualPlacementComponent` or `BP_RitualPreview`.** Neither asset should read them. A preview is a picture of a *prospective* target; these describe a *completed* one. Wiring them into the preview would couple the placement loop to the evidence pipeline for no presentational gain. They belong to a debug HUD widget — out of scope for Lane 3, flagged here so the surface is not silently missed.

---

## 3 · Lifecycle state machine

Three states. `PreviewActor` is either `None` or a valid `BP_RitualPreview`.

| # | State | Trigger | Action (node sequence) | Resulting state |
|---|---|---|---|---|
| 1 | `Idle` | `Event Tick` sees `IsInPlacementMode()` **false→true** | `DestroyPreview` (defensive, no-op when `PreviewActor` is `None`) → `bWasInPlacementMode = true` → `CachedIndex = -1` → `bConfirmedThisSession = false` → start placement-loop audio. **No spawn.** | `Armed` |
| 2 | `Armed` | `OnPreviewTargetChanged(idx >= 0, Type, bValid)` | `EnsurePreview`: `IsValid(PreviewActor)` → False branch → `SpawnActorFromClass(PreviewClass, GetCurrentTargetTransform)` → assign `PreviewActor`. Then `PreviewActor.SetPreviewType(Type)` → `RefreshPreviewTransform` → `ApplyValidState(bValid)` → `CachedIndex = idx` → target-acquired cue if the old `CachedIndex` was `-1`. | `Previewing` |
| 3 | `Armed` | `OnPreviewTargetChanged(idx == -1, …)` | `DestroyPreview` → `CachedIndex = -1`. (No target in range — F6 allows entering with none.) | `Armed` |
| 4 | `Previewing` | `OnPreviewTargetChanged(idx >= 0)` — **target change** | Same sequence as row 2. `EnsurePreview` short-circuits on the valid existing actor; the **same** preview is moved and re-typed. See §3.1. | `Previewing` |
| 5 | `Previewing` | `OnPreviewTargetChanged(idx == -1)` — **target lost / destroyed / restored by another system** | `DestroyPreview` → `CachedIndex = -1`. F2: the index moving to `-1` is the only signal that the target is gone, and it is guaranteed to fire because `-1 != CachedIndex`. | `Armed` |
| 6 | `Previewing` | `Event Tick`, `IsInPlacementMode()` true | `IsCurrentPlacementValid()` → compare to `bLastAppliedValid` → on change only, `ApplyValidState`. Then `GetCurrentTargetTransform` → if true, `SetActorLocationAndRotation` on `PreviewActor`. Required by F3 and F4. | `Previewing` |
| 7 | `Previewing` | `Event Tick`, `PreviewActor` failed `IsValid` (destroyed externally — streaming, cheat, level script) | Skip the transform/material writes this frame. Do **not** re-spawn from Tick — re-spawn is `EnsurePreview`'s job and will happen on the next `OnPreviewTargetChanged`. Keeps the spawn site singular. | `Previewing` (degraded) |
| 8 | `Previewing` | `TryConfirmPlacement` → parent `ConfirmPlacement()` returns **false** | **Preserve the preview.** `ApplyValidState(false)` → `PreviewActor.PlayRejectPulse()` → denied sound. `PreviewActor` untouched. See §3.2. | `Previewing` |
| 9 | `Previewing` | `TryConfirmPlacement` → `ConfirmPlacement()` returns **true** | C++ has already fired, in order: `SpawnRestoredActor` → `OnPlacementConfirmed` → `OnRestoredActorSpawned` → `ExitPlacementMode` → `OnPlacementModeExited` (`cpp:114, 138, 139, 142`). Blueprint work happens in those handlers; the `True` branch of `TryConfirmPlacement` runs *after* teardown and does nothing but return. | `Idle` |
| 10 | `Previewing` / `Armed` | `OnPlacementModeExited` (cancel **or** success — F7) | `DestroyActor(PreviewActor)` guarded by `IsValid` → `PreviewActor = None` → stop placement-loop audio → play satisfied *or* cancelled cue per `bConfirmedThisSession` → `bWasInPlacementMode = false`, `CachedIndex = -1`, `bConfirmedThisSession = false`. | `Idle` |
| 11 | `Idle` | Cancel called again (`ExitPlacementMode` while not in mode) | Native early-return at `cpp:84`; `OnPlacementModeExited` does **not** fire a second time (F9). Even if a Blueprint path calls `DestroyPreview` directly, the `IsValid` guard makes it a no-op. **Idempotent at both layers.** | `Idle` |
| 12 | any | `Event End Play` (F15) | `DestroyPreview`. Reason code is ignored — teardown is unconditional. | `Idle` |
| 13 | any | PIE stop / `Quit` | `ReceiveEndPlay` normally fires with `EndPlayInEditor`/`Quit` and destroys the preview. If the component is already unreachable it is skipped (`ActorComponent.cpp:1680-1681`) — harmless, because the preview is a transient world actor and dies with the world. **Invariant holds: no preview actor survives PIE.** Requires that the preview is spawned into the component's own world and is never marked persistent. | `Idle` |

### 3.1 · Decision — target change **updates** the same preview (does not replace it)

**Chosen: update in place.**

- `OnPreviewTargetChanged` can fire as often as every `QueryUpdateInterval = 0.15 s`, or immediately on `QueryMovementThreshold = 75 cm` of player movement (`cpp:55-60`, defaults `cpp:24-27`). A walking player crossing a lantern field re-triggers it several times per second.
- Destroy-and-respawn on that cadence discards the preview's dynamic material instance and any audio component every time, producing an audible/visual stutter and per-frame allocation churn.
- Replacement also opens a window in which two previews exist (old not yet destroyed, new already spawned), which is exactly the invariant §3 row 2 is built to guarantee.
- Type changes — the only reason to want a fresh actor — are handled inside the same actor by `SetPreviewType`, which swaps the static mesh on the existing `PreviewMesh` component.

**Counter-argument, for the record:** a replace-always rule is stateless and therefore trivially correct — no `SetPreviewType` path to get wrong, no stale material parameter carried between ritual types. If `SetPreviewType` grows past a mesh swap (per-type particle rigs, per-type audio loops), revisit. The update-in-place rule is the right default only while the per-type delta is a mesh.

### 3.2 · Decision — failed confirmation **preserves** the preview

**Chosen: preserve.** One rule, no exceptions, for all five failure returns.

- **F8 is the whole argument.** `ConfirmPlacement` returns `false` at `cpp:94, 95, 107, 122, 132` and **never** calls `ExitPlacementMode` on the way out. The player is still in placement mode, still targeting the same point, with `CurrentTargetPointIndex` unchanged.
- **F2 makes destroy unrecoverable.** `OnPreviewTargetChanged` fires only on an index *change*. If the preview were destroyed on failure and the player stood still, nothing would ever re-create it — the player would be stuck in a live placement mode staring at nothing, with the confirm key still armed. There is no repaint event to recover from.
- Preserve therefore keeps the visual and the native state in agreement, which is the only correctness criterion available: the preview is a picture of `CurrentTargetPointIndex`, and that index survived the failure.

**Counter-argument, for the record:** destroying gives unmissable "that didn't work" feedback. Rejected because §3 row 8's reject pulse plus the forced invalid material deliver the same feedback without desynchronising from native state — and because the recovery problem above has no clean fix.

### 3.3 · Invariant checklist

| Required invariant | Held by |
|---|---|
| Enter creates at most one preview | Row 1 spawns nothing; row 2's `EnsurePreview` is the sole spawn site and is `IsValid`-guarded |
| Target change updates or deterministically replaces | Row 4 — updates in place, §3.1 |
| Cancel destroys it | Row 10 via `OnPlacementModeExited` |
| Failed confirmation follows one documented rule | Row 8 — preserve, §3.2 |
| Successful confirmation destroys it | Row 9 → row 10; C++ routes success through `ExitPlacementMode` (F7) |
| Target destruction invalidates it | Rows 5 and 7 |
| EndPlay / PIE leave no preview actor | Rows 12 and 13 |
| Repeated cancel is safe | Row 11 — native early-return (F9) plus the `IsValid` guard |

---

## 4 · Asset structure

### 4.1 · `/Game/Gloamstead/Placement/BP_RitualPreview` (parent `Actor`)

| Element | Type | Default | Note |
|---|---|---|---|
| `PreviewMesh` | `StaticMeshComponent` | `/Game/LevelPrototyping/Meshes/SM_Cylinder` (F18) | Root-attached. Collision **off**, `CastShadow` **off** — a preview must never occlude, block, or be traced against. |
| `MatValid` | `MaterialInterface` | `/Game/Input/M_Preview_Valid` (F16) | |
| `MatInvalid` | `MaterialInterface` | `/Game/Input/M_Preview_Invalid` (F16) | |
| `MeshLanternPost` / `MeshGardenBed` / `MeshPathPoint` / `MeshDefault` | `StaticMesh` | prototyping meshes | `SetPreviewType` selects between them on `ERitualType` (F19). |
| `CurrentType` | `ERitualType` | `Invalid` | Last applied type; skips a redundant mesh swap. |
| `bIsValidState` / `bStateInitialized` | `bool` | `false` | `bStateInitialized` forces the first `SetPreviewValid` through even when the incoming value equals the `false` default. |
| `SetPreviewValid(bool)` | custom event | — | Swap `PreviewMesh` material 0 between `MatValid` / `MatInvalid`. Early-out when `bStateInitialized && bIsValidState == In`. |
| `SetPreviewType(ERitualType)` | custom event | — | Early-out when equal to `CurrentType`; otherwise `SetStaticMesh` from the four mesh vars. |
| `PlayRejectPulse()` | custom event | — | Restrained failure tell for §3.2 row 8 — a short scalar-parameter pulse on the dynamic material, no gameplay-readable signal. |

**Pivot correction (mandatory).** The `LevelPrototyping` meshes pivot at their **minimum corner**, not their centre. A piece of size `S` whose centre must sit at `C` is placed at `C − S/2`. The preview should be *centred horizontally* on the target point and *standing on* it, so on `PreviewMesh`:

```
RelativeScale3D = (X=0.35, Y=0.35, Z=1.6)
RelativeLocation = (X = −(BaseSizeX · 0.35)/2, Y = −(BaseSizeY · 0.35)/2, Z = 0)
```

With `BaseSizeX = BaseSizeY = 100` `[assumed]` — the `SM_Cylinder` bounds were **not** read this session — that is `(X=-17.5, Y=-17.5, Z=0)`. **Verify the bounds in the Static Mesh editor and recompute if they differ.** `Z = 0` is deliberate: `GetCurrentTargetPointInfo` already lifts `Location` by `VerticalOffset` along the terrain normal (F5), so the preview's base belongs at the returned location.

### 4.2 · `/Game/Gloamstead/Placement/BP_RitualPlacementComponent` (parent `URitualPlacementComponent`)

| Element | Type | Note |
|---|---|---|
| `PreviewActor` | `BP_RitualPreview` object ref | Transient. The only handle to the live preview. |
| `PreviewClass` | `class` ref, default `BP_RitualPreview` | Kept as a variable rather than hard-referenced so a per-region override is possible without editing the graph. |
| `RestoredClassLanternPost` / `…GardenBed` / `…PathPoint` / `…Default` | `class` refs | Consumed only by `SpawnRestoredActor`. |
| `bWasInPlacementMode` | `bool` | Session-edge latch for row 1 — the substitute for the missing enter event (F6). |
| `bLastAppliedValid` / `bValidLatched` | `bool` | Change-detection for row 6 so the material swaps on transition, not per frame. |
| `CachedIndex` | `int` | Presentation only — drives "first acquisition" cue gating. **Never** read as gameplay state; §1. |
| `bConfirmedThisSession` | `bool` | Set in `OnPlacementConfirmed`; read in `OnPlacementModeExited` to pick the teardown cue. |
| `EnsurePreview` | custom event | Sole spawn site. |
| `DestroyPreview` | custom event | Sole destroy site. Idempotent. |
| `RefreshPreviewTransform` | custom event | `GetCurrentTargetTransform` → `SetActorLocationAndRotation`. |
| `ApplyValidState(bool)` | custom event | Forwards to `PreviewActor.SetPreviewValid`. |
| `TryConfirmPlacement` | custom event | **The failure hook (F8).** Calls parent `ConfirmPlacement`, branches the `bool`; `False` → `HandleConfirmRejected`. Player input must call *this*, never `ConfirmPlacement` directly. |
| `HandleConfirmRejected` | custom event | Row 8. |
| `HandleSessionStart` | custom event | Row 1. |

**Wiring note.** Whatever calls into placement — pawn, controller, ability — must call `EnterPlacementMode` / `ExitPlacementMode` on the parent as usual, but route confirm through **`TryConfirmPlacement`**. Calling `ConfirmPlacement` directly discards the `bool` and silently loses every failure tell.

**Instance-propagation warning.** Blueprint **default** edits do not propagate to already-placed instances. Any pawn already placed in a level that carries the *native* `URitualPlacementComponent` will not become `BP_RitualPlacementComponent` by editing the Blueprint — the component must be replaced on that instance (or the pawn re-placed). The same applies to later default changes to `PreviewClass` and the `RestoredClass*` variables: placed instances keep the values they were serialised with.

### 4.3 · Unresolved conflict with the Phase 1.5 material convention (F23)

`docs/Phase1.5_PlacementComponent.md:31, 70-77` already specifies a **different** valid/invalid mechanism from the one this lane was briefed to use:

| | Phase 1.5 convention (`Phase1.5:70-77`) | This spec (briefed) |
|---|---|---|
| Mechanism | **One** material, parameter-driven | **Two** materials, swapped |
| Valid / invalid signal | `ValidState` scalar `1.0` / `0.0` | `MatValid` / `MatInvalid` assignment |
| Also specified | `EmissiveIntensity` `2.5`/`0.6`, `BaseColorTint` `(0.15,0.85,1.0)`/`(1.0,0.15,0.15)`, `NiagaraSpawnRateMultiplier` `1.0`/`0.15` | none |

**This spec follows the brief — two materials — and does not silently adopt the older convention.** Reasons, and the cost:

- `M_Preview_Valid` and `M_Preview_Invalid` **exist on disk** (F16); a single parameterised ritual preview material does not. The two-material path is buildable today.
- Those two materials are referenced by **no doc and no source in the repo** and live beside `BP_BuildPlacementComponent` in `Content/Input/` (F26). Their parameter surface is unread — whether either exposes `ValidState` or `EmissiveIntensity` is **unknown**, so the Phase 1.5 convention cannot be assumed to be implementable on them.
- **Cost of the choice:** the two-material swap gives a binary tell and no continuous channel. `PlayRejectPulse` (§3.2 row 8) therefore needs a scalar parameter that neither material is known to expose. If it doesn't exist, degrade the reject pulse to a brief actor-scale or visibility flicker driven from the Blueprint, and record that as the reason.

**Action for whoever runs the script:** open both materials, list their scalar/vector parameters, and either (a) confirm two-material + a usable pulse parameter and mark F23 closed in favour of this spec, or (b) if a parameterised single material is preferred, replace `SetPreviewValid`'s body with a `SetScalarParameterValue("ValidState", …)` on a dynamic material instance — the rest of §2 and §3 is unaffected, because the valid/invalid decision point is isolated behind that one custom event. **That isolation is deliberate**; it is the reason this conflict costs one graph body rather than a redesign.

### 4.4 · Naming map to the Phase 1.5 recommended functions

`docs/Phase1.5_PlacementComponent.md:57-61` recommends four Blueprint helpers. This spec keeps its own names (verb-first, no spaces, unambiguous about the idempotency guarantee) and maps them:

| Phase 1.5 name | This spec | Why renamed |
|---|---|---|
| `Update Preview Actor` | `EnsurePreview` | "Update" hides that this is the **sole spawn site**, and that it no-ops on a valid existing actor. §3 row 2's invariant depends on that being obvious at the call site. |
| `Update Preview Transform` | `RefreshPreviewTransform` | Same role; renamed only for consistency. |
| `Update Preview Visual State` | `ApplyValidState` | Names the one input it actually takes, and is the single seam §4.3 relies on. |
| `Destroy Current Preview` | `DestroyPreview` | Same role; "Current" is redundant — there is only ever one (§3). |

---

## 5 · NeoStack authoring script

Run this in `execute_script`. It is **idempotent-by-refusal**: it aborts before mutating anything if a target path it is about to create already exists (F17 says neither does today). Every step is `pcall`-wrapped and reported, so one unsupported call cannot abandon the run half-done; the tail prints a `read_graph`-based verification pass so you can see ground truth rather than trust the log.

**60-second ceiling (F24).** `execute_script` is capped at 60 s, and the editor's MCP bridge can stall entirely on post-build shader compilation. Run this with `PHASE = 0` (everything) first; if it times out, re-run with `PHASE = 1`, then `2`, then `3` — each phase is self-contained and re-acquires its assets with `open_asset`. Do not run it during a shader-compilation storm.

**This script has not been executed.** Steps marked `soft` in the report are the ones whose exact API shape could not be verified from a file — read the report before assuming a green run.

```lua
--------------------------------------------------------------------------------
-- Gloamstead · Lane 3 · placement presentation authoring
-- Creates /Game/Gloamstead/Placement/BP_RitualPreview  (parent: Actor)
--     and /Game/Gloamstead/Placement/BP_RitualPlacementComponent
--         (parent: /Script/Gloamstead.RitualPlacementComponent)
--------------------------------------------------------------------------------

local PREVIEW = "/Game/Gloamstead/Placement/BP_RitualPreview"
local COMPBP  = "/Game/Gloamstead/Placement/BP_RitualPlacementComponent"
local PARENT  = "/Script/Gloamstead.RitualPlacementComponent"

local M_VALID   = "/Game/Input/M_Preview_Valid.M_Preview_Valid"
local M_INVALID = "/Game/Input/M_Preview_Invalid.M_Preview_Invalid"
local SM_CYL    = "/Game/LevelPrototyping/Meshes/SM_Cylinder.SM_Cylinder"
local SM_CUBE   = "/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube"
local SM_CHAMF  = "/Game/LevelPrototyping/Meshes/SM_ChamferCube.SM_ChamferCube"
local SM_PLANE  = "/Game/LevelPrototyping/Meshes/SM_Plane.SM_Plane"

-- Prototyping meshes pivot at the MINIMUM CORNER. Centre a 100u base scaled by
-- 0.35 => offset -17.5 in X and Y. Z stays 0: GetCurrentTargetPointInfo already
-- lifted the location by VerticalOffset along the terrain normal.
local PREVIEW_SCALE = "(X=0.350000,Y=0.350000,Z=1.600000)"
local PREVIEW_OFFS  = "(X=-17.500000,Y=-17.500000,Z=0.000000)"

-- execute_script is capped at 60s (agent_collab/logs/decisions.md:591). Run with
-- PHASE = 0 first; if it times out, run 1, then 2, then 3 as separate calls.
--   0 = everything   1 = BP_RitualPreview only
--   2 = BP_RitualPlacementComponent only   3 = verification pass only
local PHASE = 0
local function want(p) return PHASE == 0 or PHASE == p end

--------------------------------------------------------------------------------
-- reporting harness
--------------------------------------------------------------------------------
local report, hard_fail = {}, false

local function step(kind, label, fn)
  local ok, res = pcall(fn)
  if ok and res == false then ok, res = false, "returned false/nil" end
  report[#report + 1] = string.format("[%s] %-52s %s",
    kind, label, ok and "OK" or ("FAIL: " .. tostring(res)))
  if not ok and kind == "hard" then hard_fail = true end
  return ok, res
end

local function dump_report()
  log("================ Lane 3 authoring report ================")
  for _, line in ipairs(report) do log(line) end
  log("=========================================================")
  log("hard = verified API shape · soft = unverified, inspect in editor")
end

--------------------------------------------------------------------------------
-- 0 · pre-flight: refuse to overwrite anything this run intends to CREATE.
--     Phase-aware, so a phased re-run is not blocked by its own earlier phase.
--------------------------------------------------------------------------------
local blocked = (want(1) and asset_exists(PREVIEW)) or (want(2) and asset_exists(COMPBP))
if blocked then
  log("ABORT: an asset this phase would create already exists. Nothing was modified.")
  log("  PHASE  = " .. tostring(PHASE))
  log("  " .. PREVIEW .. " exists = " .. tostring(asset_exists(PREVIEW)))
  log("  " .. COMPBP  .. " exists = " .. tostring(asset_exists(COMPBP)))
  log("Delete deliberately (delete_asset) before re-running, or advance PHASE.")
  return
end

--------------------------------------------------------------------------------
-- helpers
--------------------------------------------------------------------------------

-- Add a variable, falling back through candidate type strings.
local function add_var(bp, name, types)
  for _, t in ipairs(types) do
    local ok, r = pcall(function() return bp:add_variable(name, t) end)
    if ok and r ~= false and r ~= nil then return t end
  end
  return nil
end

-- Find the entry/event node of a graph: an exec producer with no exec input.
local function find_entry(path, graph)
  local rg = read_graph(path, graph)
  if not rg or not rg.nodes then return nil end
  local best
  for _, n in ipairs(rg.nodes) do
    local has_exec_in, has_exec_out = false, false
    for _, p in ipairs(n.pins_in  or {}) do if p.type == "exec" then has_exec_in  = true end end
    for _, p in ipairs(n.pins_out or {}) do if p.type == "exec" then has_exec_out = true end end
    if has_exec_out and not has_exec_in then best = best or n end
  end
  return best, rg
end

-- Resolve the graph name an override_function call produced.
local function graph_name_of(res, fallback)
  if type(res) == "table" then
    return res.graph_name or res.name or res.graph or fallback
  end
  if type(res) == "string" then return res end
  return fallback
end

-- Override a BlueprintImplementableEvent and wire its exec to a self-call.
local function wire_override(bp, path, fn_name, call_name, x, y)
  local _, res = pcall(function() return bp:override_function(fn_name) end)
  local gname   = graph_name_of(res, fn_name)
  local entry   = find_entry(path, gname)
  if not entry then error("no entry node found in graph '" .. tostring(gname) .. "'") end
  if not call_name then return true end
  local hits = find_nodes(call_name, path, gname, 5)
  if not hits or #hits == 0 then error("no node found for '" .. call_name .. "'") end
  hits[1].from_handle = entry.handle
  hits[1].from_pin    = "then"
  add_node(path, gname, hits[1], x or 420, y or 0)
  return true
end

local prev, comp   -- chunk scope: the verification phase re-acquires these

-- Override map, at chunk scope so PHASE 3 can verify it without PHASE 2 running.
-- Native surface: RitualPlacementComponent.h:97-113 (working tree, see spec F27)
-- Engine surface: ReceiveTick / ReceiveEndPlay (ActorComponent.h:1371, :959)
local overrides = {
  {"OnPreviewTargetChanged", "EnsurePreview",         420, 0},
  {"OnPathPointRedirected",  nil,                     420, 0},  -- HUD sink, by hand
  {"OnPlacementConfirmed",   nil,                     420, 0},  -- cue only, by hand
  {"OnRestoredActorSpawned", nil,                     420, 0},  -- dressing, by hand
  {"OnPlacementModeExited",  "DestroyPreview",        420, 0},
  {"SpawnRestoredActor",     nil,                     420, 0},  -- see spec section 2.1
  {"ReceiveTick",            "HandleSessionStart",    420, 0},
  {"ReceiveEndPlay",         "DestroyPreview",        420, 0},
}

--------------------------------------------------------------------------------
-- 1 · BP_RitualPreview   [PHASE 1]
--------------------------------------------------------------------------------
if want(1) then

step("hard", "create BP_RitualPreview", function()
  prev = create_asset(PREVIEW, "Blueprint", {ParentClass = "Actor"})
  return prev ~= nil
end)
if hard_fail then dump_report(); return end

step("hard", "preview: add PreviewMesh (StaticMeshComponent)", function()
  return prev:add_component("PreviewMesh", "StaticMeshComponent")
end)

step("soft", "preview: PreviewMesh.StaticMesh = SM_Cylinder", function()
  return prev:set("PreviewMesh", "StaticMesh", SM_CYL)
end)
step("soft", "preview: PreviewMesh.RelativeScale3D", function()
  return prev:set("PreviewMesh", "RelativeScale3D", PREVIEW_SCALE)
end)
step("soft", "preview: PreviewMesh.RelativeLocation (min-corner fix)", function()
  return prev:set("PreviewMesh", "RelativeLocation", PREVIEW_OFFS)
end)
step("soft", "preview: PreviewMesh.CastShadow = false", function()
  return prev:set("PreviewMesh", "CastShadow", "false")
end)
step("soft", "preview: PreviewMesh collision off", function()
  return prev:set("PreviewMesh", "CollisionEnabled", "NoCollision")
end)
step("soft", "preview: PreviewMesh.CollisionProfileName = NoCollision", function()
  return prev:set("PreviewMesh", "CollisionProfileName", "NoCollision")
end)

local prev_vars = {
  {"MatValid",        {"MaterialInterface", "material_interface", "object:MaterialInterface"}},
  {"MatInvalid",      {"MaterialInterface", "material_interface", "object:MaterialInterface"}},
  {"MeshLanternPost", {"StaticMesh", "static_mesh", "object:StaticMesh"}},
  {"MeshGardenBed",   {"StaticMesh", "static_mesh", "object:StaticMesh"}},
  {"MeshPathPoint",   {"StaticMesh", "static_mesh", "object:StaticMesh"}},
  {"MeshDefault",     {"StaticMesh", "static_mesh", "object:StaticMesh"}},
  {"DynMat",          {"MaterialInstanceDynamic", "object:MaterialInstanceDynamic"}},
  {"CurrentType",     {"ERitualType", "byte", "int"}},
  {"bIsValidState",     {"bool"}},
  {"bStateInitialized", {"bool"}},
}
for _, v in ipairs(prev_vars) do
  step("soft", "preview var: " .. v[1], function()
    local used = add_var(prev, v[1], v[2])
    if not used then return false end
    log("   -> " .. v[1] .. " declared as '" .. used .. "'")
    return true
  end)
end

step("soft", "preview default: MatValid",   function() return prev:set("self", "MatValid",   M_VALID)   end)
step("soft", "preview default: MatInvalid", function() return prev:set("self", "MatInvalid", M_INVALID) end)
step("soft", "preview default: MeshLanternPost", function() return prev:set("self", "MeshLanternPost", SM_CYL)   end)
step("soft", "preview default: MeshGardenBed",   function() return prev:set("self", "MeshGardenBed",   SM_CUBE)  end)
step("soft", "preview default: MeshPathPoint",   function() return prev:set("self", "MeshPathPoint",   SM_PLANE) end)
step("soft", "preview default: MeshDefault",     function() return prev:set("self", "MeshDefault",     SM_CHAMF) end)

for _, ev in ipairs({"SetPreviewValid", "SetPreviewType", "PlayRejectPulse"}) do
  step("hard", "preview event: " .. ev, function() return prev:add_custom_event(ev) end)
end

step("hard", "preview: compile", function() return prev:compile() end)
step("hard", "preview: save",    function() return prev:save()    end)

end -- PHASE 1

--------------------------------------------------------------------------------
-- 2 · BP_RitualPlacementComponent   [PHASE 2]
--------------------------------------------------------------------------------
if want(2) then

step("hard", "create BP_RitualPlacementComponent", function()
  comp = create_asset(COMPBP, "Blueprint", {ParentClass = PARENT})
  return comp ~= nil
end)
if not comp then dump_report(); return end

local comp_vars = {
  {"PreviewActor",             {"BP_RitualPreview", "actor"}},
  {"PreviewClass",             {"class:Actor", "class", "object:Class"}},
  {"RestoredClassLanternPost", {"class:Actor", "class", "object:Class"}},
  {"RestoredClassGardenBed",   {"class:Actor", "class", "object:Class"}},
  {"RestoredClassPathPoint",   {"class:Actor", "class", "object:Class"}},
  {"RestoredClassDefault",     {"class:Actor", "class", "object:Class"}},
  {"bWasInPlacementMode",   {"bool"}},
  {"bLastAppliedValid",     {"bool"}},
  {"bValidLatched",         {"bool"}},
  {"bConfirmedThisSession", {"bool"}},
  {"CachedIndex",           {"int"}},
}
for _, v in ipairs(comp_vars) do
  step("soft", "component var: " .. v[1], function()
    local used = add_var(comp, v[1], v[2])
    if not used then return false end
    log("   -> " .. v[1] .. " declared as '" .. used .. "'")
    return true
  end)
end

step("soft", "component default: PreviewClass = BP_RitualPreview", function()
  return comp:set("self", "PreviewClass", PREVIEW .. "_C")
end)

-- Helper custom events. These are the bodies the overrides call into, which is
-- what keeps every override graph to a single node.
local helpers = {
  "EnsurePreview", "DestroyPreview", "RefreshPreviewTransform",
  "ApplyValidState", "HandleSessionStart", "HandleConfirmRejected",
  "TryConfirmPlacement",
}
for _, ev in ipairs(helpers) do
  step("hard", "component event: " .. ev, function() return comp:add_custom_event(ev) end)
end

step("hard", "component: compile (publish custom events)", function() return comp:compile() end)

--------------------------------------------------------------------------------
-- 3 · Override graphs. Each override's exec is wired to one helper call.
--     (The `overrides` table is declared at chunk scope, above.)
--------------------------------------------------------------------------------
for _, o in ipairs(overrides) do
  step("soft", "override: " .. o[1] .. (o[2] and (" -> " .. o[2]) or " (stub)"), function()
    return wire_override(comp, COMPBP, o[1], o[2], o[3], o[4])
  end)
end

step("hard", "component: compile", function() return comp:compile() end)
step("hard", "component: save",    function() return comp:save()    end)

end -- PHASE 2

--------------------------------------------------------------------------------
-- 4 · Verification pass — read the assets back rather than trusting the log
--     [PHASE 3]. Re-acquires both assets so it can run as a standalone call.
--------------------------------------------------------------------------------
if want(3) then

if not prev then pcall(function() prev = open_asset(PREVIEW) end) end
if not comp then pcall(function() comp = open_asset(COMPBP)  end) end

log("---------------- verification ----------------")
pcall(function()
  log("BP_RitualPreview info:");            prev:info()
  log("BP_RitualPreview properties:");      prev:list_properties()
end)
pcall(function()
  log("BP_RitualPlacementComponent info:");       comp:info()
  log("BP_RitualPlacementComponent properties:"); comp:list_properties()
  log("BP_RitualPlacementComponent events:");     comp:list_events()
end)

for _, o in ipairs(overrides) do
  pcall(function()
    local rg = read_graph(COMPBP, o[1])
    if rg and rg.nodes then
      log(string.format("graph %-24s nodes=%d", o[1], #rg.nodes))
      for _, n in ipairs(rg.nodes) do log("    - " .. tostring(n.name)) end
    else
      log(string.format("graph %-24s NOT READABLE under that name", o[1]))
    end
  end)
end

end -- PHASE 3

dump_report()
log("Both assets are authored but NOT gameplay-complete: the node bodies of the")
log("helper events and the four stub overrides are the manual pass in section 6.")
```

---

## 6 · Manual pass after the script

The script produces the asset skeletons, every variable, every custom event, and every override graph with its exec routed to the right helper. It does **not** author the interiors of the helper events or the four stub overrides. Those are the graphs to build by hand (or in a follow-up script once the report in §5 confirms which node names resolve):

| Graph | Body to author |
|---|---|
| `EnsurePreview` | `IsValid(PreviewActor)` → False → `SpawnActorFromClass(PreviewClass, GetCurrentTargetTransform, AlwaysSpawn)` → `Set PreviewActor`. |
| `DestroyPreview` | `IsValid(PreviewActor)` → True → `DestroyActor` → `Set PreviewActor = None`. The `IsValid` guard is what makes row 11 idempotent. |
| `RefreshPreviewTransform` | `GetCurrentTargetTransform` → Branch → True → `PreviewActor.SetActorLocationAndRotation`. |
| `ApplyValidState` | `PreviewActor.SetPreviewValid(In)` → `Set bLastAppliedValid`, `Set bValidLatched = true`. |
| `HandleSessionStart` | Row 1, then the row 6 poll: `IsInPlacementMode` rising-edge check against `bWasInPlacementMode`; then `IsCurrentPlacementValid` vs `bLastAppliedValid`; then `RefreshPreviewTransform`. |
| `TryConfirmPlacement` | Parent `ConfirmPlacement` → Branch → False → `HandleConfirmRejected`. |
| `HandleConfirmRejected` | `ApplyValidState(false)` → `PreviewActor.PlayRejectPulse` → denied sound. |
| `OnPreviewTargetChanged` | After the wired `EnsurePreview` call: `PointIndex >= 0` branch, `SetPreviewType`, `RefreshPreviewTransform`, `ApplyValidState`, `Set CachedIndex`. |
| `SpawnRestoredActor` | §2.1 — index guard, `GetCurrentTargetPointInfo`, `SpawnActorFromClass`, set `OutSpawnedActor`. |
| `OnPathPointRedirected` / `OnPlacementConfirmed` / `OnRestoredActorSpawned` | §2.1 — HUD sink, confirm cue, durable-actor dressing. |
| `BP_RitualPreview.SetPreviewValid` / `SetPreviewType` / `PlayRejectPulse` | §4.1. |

**Verify block for the finished layer** (run once the manual pass is done — Lane 3 has not run any of it):

1. Open `/Game/Gloamstead/Placement/BP_RitualPlacementComponent`; confirm the parent class reads `Ritual Placement Component` and that `list_events` shows all eight overrides.
2. Add it to the player pawn **on the placed instance**, not only in the Blueprint defaults (§4.2 propagation warning).
3. PIE. Walk into a lantern field: exactly one preview appears, follows the target, and its material flips as you cross the `RestorationRadius * 1.25` boundary **while standing on the same target** — that specifically exercises F3, which no event reports.
4. Look around while standing on a slope steeper than `dot < 0.6`: the preview rotation tracks the camera (F4). That exercises the Tick transform refresh.
5. Confirm on a valid point: preview disappears, restored actor stands exactly where the preview stood (both read `GetCurrentTargetPointInfo`, F5).
6. Confirm on an invalid point: preview **stays**, flashes invalid, denied sound (§3.2).
7. Cancel twice in a row: no error, no orphan.
8. Stop PIE, then check the World Outliner in the editor world — no `BP_RitualPreview` actor remains (row 13).

---

## 7 · Assumptions not verified from a file

| # | Assumption | Risk if wrong |
|---|---|---|
| A1 | `SM_Cylinder`'s base is 100×100 units, making the min-corner correction `-17.5` at scale `0.35`. The mesh bounds were **not** read. | Preview is offset horizontally from the point. Fix: read the bounds, recompute `−(BaseSize · Scale)/2`. |
| A2 | The `add_variable` type DSL accepts `MaterialInterface`, `StaticMesh`, `MaterialInstanceDynamic`, `ERitualType`, `class:Actor`, and `BP_RitualPreview` as class-name types. The skill documents `<class-name>` generically but names none of these. | The script's `add_var` fallback chain covers it and logs which spelling won; a variable that exhausts all candidates is reported `FAIL` and must be added by hand. |
| A3 | `bp:set(target, property, value)` accepts `"self"` as the target for member-variable defaults. Only component targets are documented. | Those steps are `soft`; defaults get set by hand in the Details panel. |
| A4 | `bp:set` accepts asset paths as plain strings for object properties, and `NoCollision` for `CollisionEnabled`. | `soft` steps; set in the Details panel instead. |
| A5 | `bp:override_function("ReceiveTick")` / `("ReceiveEndPlay")` accept the internal names rather than the display names `"Tick"` / `"End Play"`. Both exist as `BlueprintImplementableEvent` (F15, F13) but the override entry point's expected string is unverified. | Those two `soft` steps fail loudly; retry with `"Tick"` / `"End Play"`. |
| A6 | Overridden BIE graphs are readable via `read_graph(path, <event name>)`. Graph naming for overrides is not documented in the skill. | The verification pass prints `NOT READABLE under that name` and you enumerate with `comp:info()`. |
| A7 | `add_custom_event` publishes a callable node discoverable by `find_nodes` after a compile — hence the intermediate compile before the override pass. | The override wiring steps report `no node found for '<helper>'` and get connected by hand. |
| A8 | Ritual-type → preview-mesh and ritual-type → restored-class assignments are placeholders. No art direction for lanterns, garden beds, or path markers was read this session. | Cosmetic only. |
| A9 | The §5 script is **structurally** checked but **not syntax-checked**: no Lua interpreter is available on this machine. A token-balance pass over the extracted block confirmed `function`/`then`/`do` vs `end` nets to zero and all brackets balance, and that every phase guard opens and closes. That is not a parser. | A syntax error would abort the run before any mutation, which the pre-flight guard makes safe to retry. |
| A10 | `open_asset(path)` (used by PHASE 3 to re-acquire both assets standalone) returns the same enriched BP object as `create_asset`. The skill states this for the general case but PHASE 3 was not run. | PHASE 3's calls are all `pcall`-wrapped; a nil return degrades to a skipped verification line, never a mutation. |
| A11 | `M_Preview_Valid` / `M_Preview_Invalid` expose whatever `SetPreviewValid` and `PlayRejectPulse` need. **Neither material was opened.** §4.3 is the live conflict this rests on. | Reject pulse may have no parameter to drive; degrade to a scale/visibility flicker and record why. |
