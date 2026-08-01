# Lane 4 recipe — Restored First Lantern

**Status: SPECIFICATION. Nothing in this document has been executed.** No editor call was made
by this lane. The script in §5 has never been run; it is authored against source-verified
bindings, not against a live session.

Content root: `Content/Gloamstead/Restoration/FirstLantern/`
Object root: `/Game/Gloamstead/Restoration/FirstLantern/`

---

## 1. What this actor is, and how it relates to the existing ruin

The restored actor is a **crown**, not a replacement post. It materializes the lantern head,
its collar, and its light *on top of the ruin pieces that are already in the level*. It adds
no ground geometry and it references no other actor, so it needs no level-actor wiring and
cannot desync from the ruin.

### The ruin as it is actually saved right now

The ruin pieces were re-placed by `reposition.lua` to correct for min-corner pivots, and that
pass **zeroed every rotation**. Positions below are min-corner (the value stored on the actor),
with the derived centre and the top face:

| Actor label | Mesh | Size (uu) | Min-corner location | Centre (X,Y) | Top Z |
|---|---|---|---|---|---|
| `Lantern_First_Plinth` | `SM_Cylinder` | 220 x 220 x 120 | (-910, -1110, 0) | (-800, -1000) | 120 |
| `Lantern_First_BrokenPost` | `SM_Cube` | 70 x 70 x 380 | (-795, -1020, 110) | (-760, -985) | 490 |
| `Lantern_First_FallenHead` | `SM_Cube` | 150 x 150 x 140 | (-545, -1265, 0) | (-470, -1190) | 140 |
| `Lantern_First_Rubble_A` | `SM_Cube` | 120 x 90 x 70 | (-1070, -885, 0) | (-1010, -840) | 70 |

**[verified]** Source of these numbers: the session scratchpad authoring scripts
`…\scratchpad\lua\stage3.lua:5,30,34,39,43` (original centres and sizes) and
`…\scratchpad\lua\reposition.lua:28-34` (the `C - S/2` re-placement that also wrote
`rotation = { pitch = 0, yaw = 0, roll = 0 }`). All four labels are present in
`D:\Unreal Projects\Gloamstead5_8\Content\Maps\Lvl_Gloamstead.umap`.

**Discrepancy you need to know before art-directing this.** The brief describes a *leaning
snapped post*. In the saved level the post is **upright and unrotated** — `reposition.lua`
destroyed the 14-degree roll and nothing restored it. So today the pre-restoration reading is
carried almost entirely by the **fallen head lying 3.4 m away** and the rubble block, not by a
lean. That is still a legible ruin ("the post stands, its head is on the ground"), and this
recipe is designed around that reading. If you want the lean back, restore
`roll = 14` on `Lantern_First_BrokenPost`, `{pitch=8, yaw=34, roll=22}` on
`Lantern_First_FallenHead`, and `yaw = 18` on `Lantern_First_Rubble_A` — but note that a
rotated min-corner-pivot cube also moves, so the locations need recomputing. **[OPEN]**

### The before / after read

```
   BEFORE (now)                          AFTER (this actor materializes)

        |                                      ,---------.   <- Halo (additive glow shell)
        |  <- BrokenPost                       |  Head   |      z 470..660
        |     z 110..490                       `---------'   <- Head  z 490..640
        |                                          |||      <- Collar z 460..500
        |                                          |||
     ___|___                                     ___|___     <- BrokenPost (untouched)
    /       \  <- Plinth z 0..120               /       \
   /_________\                                 /_________\   <- Plinth (untouched)
  ================================== ground ==================================
                    [] <- FallenHead                   [] <- FallenHead (left as debris)
```

The `Collar` deliberately overlaps the post top (460..500 against a post top of 490) so the
seam is hidden without needing to modify the post. The `Head` sits directly on the post top.
The `Halo` is a slightly oversized additive shell around the head — at emissive zero it is
literally invisible (additive blend, black = no contribution), which is what gives the
"absent" pre-restoration reading for free.

**Open art call [OPEN]:** the `FallenHead` stays on the ground after restoration. Reading it
as a shed husk is defensible and costs nothing; hiding it would require the spawner to hold a
level-actor reference, which this actor deliberately avoids. Recommend leaving it.

### Local space

The actor's origin is authored to sit at the **plinth axis at ground level**, world
`(-800, -1000, 0)`. Every `RelativeLocation` below is in that frame. The existing post is
offset `(+40, +15)` from the plinth axis, so the crown is offset by the same amount — that
offset is baked into the component transforms, not corrected, because correcting it would
float the head off the post.

> The spawner must therefore place this actor **at the ritual point's ground location with
> zero rotation**. If the PCG point for this site is not at `(-800, -1000, 0)`, the crown will
> not land on the post. **[assumed]** — this lane cannot read the PCG point data; verify with
> `GetCurrentTargetPointInfo()` before shipping.

---

## 2. Asset inventory

### Created by the script

| Object path | Type | Parent class | Notes |
|---|---|---|---|
| `/Game/Gloamstead/Restoration/FirstLantern/BP_Restored_LanternPost` | `Blueprint` | `/Script/Engine.Actor` | The deliverable. Name follows `docs/Phase3_SixHourExperience.md:115`. |

### Human-owed — the script cannot create this

| Object path | Type | Why not scripted |
|---|---|---|
| `/Game/Gloamstead/Restoration/FirstLantern/NS_FirstLanternMotes` | `NiagaraSystem` | **[verified]** `create_asset` has no Niagara alias in this build. `Plugins/NeoStackAI/Source/NeoStackAI/Private/Lua/Bindings/LuaBinding_Niagara.cpp` is two comment lines — "has been moved to the NSAI_Niagara extension module" — and `Plugins/NeoStackAI/NeoStackAI.uplugin` declares only the `NeoStackAI` module, so that extension is not present. The alias table at `Plugins/NeoStackAI/Source/NeoStackAI/Private/Blueprint/BlueprintUtils.cpp:1991-2137` contains no Niagara entry. |

**Brief for `NS_FirstLanternMotes` (restrained, deliberately):** one emitter, burst of 14
particles at t=0 plus a 0.4/s trickle for 2 s then stop. Sprite size 6 uu. Lifetime 1.4 s.
Velocity +Z 40 uu/s with 15 uu/s cone jitter. Colour the same amber as the light, alpha
fading to 0 over life. **No loop, no ribbon, no mesh particles.** Total on-screen count must
never exceed ~20. If it reads as sparkle rather than as embers drifting up, it is too much.

The script wires `Motes.Asset` to this path **only if the asset already exists**, so running
the script before the Niagara system is authored produces no spurious `[FAIL]` line. Once the
asset exists, re-run just the one `bp:set("Motes", "Asset", …)` line, or delete and re-run.

### Referenced, not created (all verified present on disk)

| Object path | Type | Role |
|---|---|---|
| `/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube` | StaticMesh | Collar, Head |
| `/Game/LevelPrototyping/Meshes/SM_Cylinder.SM_Cylinder` | StaticMesh | Halo shell |
| `/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray.MI_PrototypeGrid_Gray` | MaterialInstanceConstant | Collar + Head surface. Chosen to contrast the ruin, which uses `MI_PrototypeGrid_TopDark` (`stage3.lua:6`) — restored geometry reads lighter than ruined geometry. |
| `/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial` | Material | Halo. **[verified]** `MSM_Unlit` + `BLEND_Additive`, exposes one `MaterialExpressionVectorParameter` named **`Color`** which drives `EmissiveColor`. The parameter name is confirmed by the engine's own render-proxy overrides: `D:\UE_5.8\Engine\Source\Runtime\Renderer\Private\SceneVisibility.cpp:5836,5840` construct `FColoredMaterialRenderProxy(GEngine->EmissiveMeshMaterial->GetRenderProxy(), Color, NAME_Color)`; the material is loaded at `Runtime/Engine/Private/UnrealEngine.cpp:3575`. |
| `/Engine/EngineSounds/1kSineTonePing.1kSineTonePing` | SoundWave | Both audio cues (see §4). |

### Components on `BP_Restored_LanternPost`

All attach to the default scene root. Prototyping meshes pivot at their **minimum corner**, so
`RelativeLocation = centre - size/2`; the table gives both so the arithmetic is auditable.

| Component | Class | Centre (local) | `RelativeLocation` | `RelativeScale3D` | Other properties |
|---|---|---|---|---|---|
| `Collar` | `StaticMeshComponent` | (40, 15, 480) | `(X=-5,Y=-30,Z=460)` | `(X=0.9,Y=0.9,Z=0.4)` | `StaticMesh` = SM_Cube, `OverrideMaterials` = MI_PrototypeGrid_Gray |
| `Head` | `StaticMeshComponent` | (40, 15, 565) | `(X=-25,Y=-50,Z=490)` | `(X=1.3,Y=1.3,Z=1.5)` | `StaticMesh` = SM_Cube, `OverrideMaterials` = MI_PrototypeGrid_Gray |
| `Halo` | `StaticMeshComponent` | (40, 15, 565) | `(X=-45,Y=-70,Z=470)` | `(X=1.7,Y=1.7,Z=1.9)` | `StaticMesh` = SM_Cylinder, `OverrideMaterials` = EmissiveMeshMaterial, `CastShadow` = false |
| `Glow` | `PointLightComponent` | (40, 15, 565) | `(X=40,Y=15,Z=565)` | — | **`Mobility` = `Movable`**, `IntensityUnits` = `Candelas`, `Intensity` = 0, `AttenuationRadius` = 300, `SourceRadius` = 12, `LightColor` = `(R=255,G=196,B=132,A=255)` |
| `Motes` | `NiagaraComponent` | (40, 15, 545) | `(X=40,Y=15,Z=545)` | — | `bAutoActivate` = true, `Asset` = NS_FirstLanternMotes (conditional) |
| `AudioOnset` | `AudioComponent` | (40, 15, 565) | `(X=40,Y=15,Z=565)` | — | `Sound` = 1kSineTonePing, `bAutoActivate` = false, `PitchMultiplier` = 0.5, `VolumeMultiplier` = 0.55 |
| `AudioSettle` | `AudioComponent` | (40, 15, 565) | `(X=40,Y=15,Z=565)` | — | `Sound` = 1kSineTonePing, `bAutoActivate` = false, `PitchMultiplier` = 0.75, `VolumeMultiplier` = 0.35 |

**Why `Movable` is mandatory, with two independent reasons — both verified:**

1. `Config/DefaultEngine.ini:17` sets `r.AllowStaticLighting=False`. There is no lightmass
   build and there never will be, so a Static or Stationary light contributes nothing.
2. `UPointLightComponent` is a `ULocalLightComponent`, and this actor **ramps its attenuation
   radius at runtime**. `ULocalLightComponent::SetAttenuationRadius` is gated on
   `AreDynamicDataChangesAllowed(false)` —
   `D:\UE_5.8\Engine\Source\Runtime\Engine\Private\Components\LocalLightComponent.cpp:20-29`,
   with the comment *"Only movable lights can change their radius at runtime"*. On a
   non-Movable light the radius track silently does nothing.

### Variables

| Name | Type | Options | Purpose |
|---|---|---|---|
| `bMaterialized` | `bool` | `blueprint_read_only` | Set true on timeline `Finished`. This is the readable proof of the stable final state; see §4. |
| `RestorationPointIndex` | `int` | `expose_on_spawn`, `edit_instance_only` | Stamped by the spawner with the `PointIndex` it was given. Makes "one actor per authoritative success" auditable by inspection rather than by trust. Defaults to `-1`. |

---

## 3. Timeline `TL_Materialize`

`length = 1.6`, `auto_play = false`, `loop = false`. Four tracks.

The curve values below are the **final property values**, not normalised alphas. There is no
lerp maths in the graph — each track output wires straight into its setter. This is what keeps
the graph small enough to script reliably, and it means this table *is* the authored curve.

| t (s) | `GlowColor` (linear RGB, vector) | `LightIntensity` (cd) | `LightRadius` (cm) | Particle state | Audio event |
|---|---|---|---|---|---|
| 0.00 | (0.00, 0.00, 0.00) | 0 | 300 | Motes burst of 14 fires (auto-activate at BeginPlay) | `AudioOnset` plays — 1 kHz ping at pitch 0.50 ≈ 500 Hz, volume 0.55 |
| 0.35 | (0.30, 0.17, 0.07) | 95 | 520 | Burst rising, trickle begins | — |
| 0.80 | (1.55, 0.88, 0.37) | **340** (peak) | 880 | Trickle at full rate | — |
| 1.15 | (0.95, 0.54, 0.22) | 250 (undershoot) | 900 | Trickle tapering | `SettleCue` event track fires -> `AudioSettle` plays, pitch 0.75 ≈ 750 Hz, volume 0.35 |
| 1.60 | (1.10, 0.62, 0.26) | **280** (final) | 900 | Trickle ends ~t=2.0, emitter goes idle | — |

Timeline `Finished` -> `Set bMaterialized = true`.

**The overshoot at 0.80 and the undershoot at 1.15 are the whole point.** A monotonic 0-to-1
ramp reads as a dimmer knob. Peak-then-settle reads as something arriving. The peak is only
1.21x final, which is restraint — it is a breath, not a flashbang.

**Interpolation.** The curve shape comes from key placement plus the engine's automatic
tangents, **not** from an `interp_mode` argument. **[verified]** `add_timeline` accepts an
`interp_mode` key syntactically but discards it on the timeline path: the parser stores only
`TPair<float, FString>` (`LuaBinding_Blueprint.cpp:2317,2345,2361,2372`) and
`AddTrackToTemplate` calls `FloatCurve.AddKey(...)` followed by `AutoSetTangents()`
(`Plugins/NeoStackAI/Source/NeoStackAI/Private/Blueprint/BlueprintUtils.cpp:1615-1617,1644,1669`)
with no `SetKeyInterpMode` anywhere on that path. `AutoSetTangents` gives smooth cubic-like
tangents, which is what we want; the script does not pass `interp_mode`, because passing an
argument that is silently dropped is how a spec starts lying about itself.

### Justifying the light values against a 3.0-lux DirectionalLight

**[verified]** `UDirectionalLightComponent::GetLightUnits()` returns
`ELightUnits::Unitless /* Lux */` —
`D:\UE_5.8\Engine\Source\Runtime\Engine\Private\Components\DirectionalLightComponent.cpp:1509`.
So the key light is **3.0 lux**, against an engine default of 10
(`DirectionalLightComponent.cpp:1030`). At pitch -26 degrees, the ground receives
`3.0 x sin(26 deg) = 1.31 lux`. That is the number the restored light has to beat.

**[verified]** `ULocalLightComponent`'s constructor sets `IntensityUnits = ELightUnits::Unitless`
(`LocalLightComponent.cpp:14`), *not* Candelas. A bare `Intensity = 280` on a fresh component
would therefore be 280 **unitless**, which is a completely different brightness. The script
sets `IntensityUnits = "Candelas"` explicitly so every number in this document is physical.
Valid enum entries are `Unitless, Candelas, Lumens, EV, Nits`
(`D:\UE_5.8\Engine\Source\Runtime\Engine\Classes\Engine\Scene.h:83-90`).

The lamp sits at local Z = 565, i.e. **5.65 m above the ground**. Inverse-square from 280 cd:

| Ground point | Distance | Cosine factor | Illuminance | vs 1.31 lux key |
|---|---|---|---|---|
| Directly below the lamp | 5.65 m | 1.00 | 8.8 lux | **6.7x** |
| 4 m out (plinth edge, rubble) | 6.92 m | 0.816 | 4.8 lux | **3.7x** |
| 9 m out (attenuation edge) | 10.60 m | 0.533 | 1.33 lux | **1.0x — parity** |

That is the shape the requirement asks for: a pool that unambiguously dominates the key
underfoot, still clearly reads at the plinth and the rubble, and lands exactly at ambient
parity where it is cut off — so the falloff has no visible hard edge. The attenuation radius
of 900 cm is chosen *because* 9 m is where parity happens, not picked round.

280 cd is also physically sane for the fiction: a 100 W incandescent bulb is roughly 100-140 cd,
a street lamp is thousands. A large courtyard lantern at 280 cd is a lantern, not a floodlight.

**Tuning band: 180-420 cd.** Below ~180 the pool stops beating the key at the plinth edge;
above ~420 the parity radius exceeds the attenuation radius and you get a visible cut-off ring.

### Exposure caveat — read this before tuning the emissive

**[verified]** `Config/DefaultEngine.ini:16` sets `r.EyeAdaptationQuality=0`, with the tracked
comment at lines 14-15: *"Lock gameplay exposure — disable auto eye-adaptation so it can't
chase the emissive readability proxies and wash out the character/ground."* Line 12-13 set
`r.DefaultFeature.AutoExposure.ExtendDefaultLuminanceRange=True`.

Consequence: **there is no eye adaptation to compensate for anything.** The emissive numbers
below are absolute against a fixed exposure whose effective EV this lane could not determine
without opening the editor. They are a defensible starting point, not a measured result.
**[assumed]**

Emissive end value `(1.10, 0.62, 0.26)` — warm amber, peak channel 1.1, i.e. mildly
overbright so it blooms without clipping. **Tuning band 0.6 - 2.5 on the red channel**, holding
the ratio `1.00 : 0.56 : 0.24`.

**Acceptance criterion, so this is checkable rather than arguable:** at the final state the
halo must still read as *amber* at its core — if the centre has clipped to white and the
channel separation is gone, it is too bright; drop the red channel until colour returns. If
the halo does not visibly bloom against the dusk sky at all, it is too dim.

---

## 4. Requirements the actor satisfies, and how

| Requirement | Mechanism |
|---|---|
| Ruined / absent pre-restoration reading | The ruin pieces are untouched and already provide it (§1). The actor's own glow starts at additive black, which contributes literally nothing — the "absent" state is not faked with opacity. |
| Short materialization transition | `TL_Materialize`, **1.6 s**. Shape from key placement + `AutoSetTangents` (§3). |
| Emissive ramp | `Halo` uses `EmissiveMeshMaterial` (additive, unlit) driven through `SetVectorParameterValueOnMaterials("Color", …)` from the `GlowColor` vector track. |
| Point light ramp | `Glow` intensity 0 -> 280 cd (peak 340), radius 300 -> 900 cm, justified in §3. |
| Restrained particles | `Motes` NiagaraComponent, auto-activating. **Asset is human-owed** — see §2. |
| Distinct restoration audio | Two `AudioComponent`s on the same engine wave at pitches 0.50 and 0.75 — a rising two-note figure at t=0 and t=1.15. See the note below. |
| Visible local illumination change on ground and architecture | The illuminance table in §3 — 6.7x key underfoot, 3.7x at the plinth edge, parity at 9 m. The radius ramp 300 -> 900 makes the pool visibly *grow*, which is what sells it as spreading rather than switching on. |
| Stable final state, no replay | See "The guard" below. |
| Exactly one final actor per authoritative success | See "Identity" below. |
| Failed spawn produces a visible structured failure | See "Failure" below — **this one is not fully solvable inside this actor**, and the gap is in C++. |

**On the audio.** The project contains **zero audio assets** — verified by searching all of
`Content/` for `*Cue*`, `S_*`, `*Sound*`, `MS_*`: no matches. So *any* audible event is
distinct here by construction. `1kSineTonePing` is engine content
(`D:\UE_5.8\Engine\Content\EngineSounds\1kSineTonePing.uasset`), pitched down to ~500 Hz and
~750 Hz so it reads as two soft bell tones rather than as a test beep. **This is a greybox
placeholder and should be labelled as such** — it is chosen because it is real, verifiable,
and available today, not because it is the right sound. Replace with an authored cue before
anyone calls this shipped.

NeoStack *can* author a `SoundCue` (`Private/Lua/Bindings/LuaBinding_SoundCue.cpp`), but doing
so means editing the cue's node graph, which is another unverified surface. Not worth it for a
placeholder.

### The guard against replay

Three layers, stated precisely:

1. **Structural.** The timeline's `Play from Start` pin is wired to exactly one thing: the
   `Event BeginPlay` exec chain. This actor is spawned at runtime, so BeginPlay fires exactly
   once in its lifetime. Nothing in this Blueprint binds to any delegate, subsystem, or
   gameplay event — `OnStructureRestored`, the day/night subsystem, and the placement
   component are all absent from its graph. There is no path by which an unrelated event can
   reach the timeline.
2. **Timeline configuration.** `loop = false`. On `Finished` the timeline holds its last key
   value, so the final state persists without any additional write.
3. **Observable.** `bMaterialized` is set true on `Finished` and is never cleared. It exists so
   the state is *checkable* — if you ever see a replay, `bMaterialized` was already true when
   the ramp restarted, and that immediately tells you something external is calling in.

Note that (1) is the real guard and (3) is the instrument. A boolean branch in front of the
timeline would be theatre: on a freshly spawned actor the flag is always false.

**Blueprint default edits do not propagate to already-placed instances.** That caveat does not
bite here, because this actor is only ever spawned at runtime and therefore always inherits the
current CDO. It *would* bite if anyone drags one into the level for testing and then edits the
Blueprint — the placed copy keeps its old values. If you greybox-test by hand-placing, delete
and re-place after every Blueprint change.

### Identity — one actor, and the payload pointing at it

> **Citation freshness.** `RitualPlacementComponent.cpp/.h` are under active edit by another
> lane — the working tree carries uncommitted changes on top of `2c3351b` that renamed the
> commit path and shifted every line number. The citations below were **re-read against the
> working tree after those edits**. If the numbers have drifted again, the symbols are stable:
> `ConfirmPlacement`, `BuildRestorationPayload`, `CommitRestorationWithEvidence`.

**[verified]** `URitualPlacementComponent::ConfirmPlacement` declares `AActor* SpawnedActor = nullptr`
(`Source/Gloamstead/Components/RitualPlacementComponent.cpp:113`), fills it via
`SpawnRestoredActor(FinalPointIndex, SpawnedActor)` (`:114`), passes that same raw pointer into
`BuildRestorationPayload(FinalPointIndex, SpawnedActor, Payload)` (`:117`), which assigns
`OutPayload.RestoredActor = SpawnedRestoredActor` at `:352`. It is a straight pass-through of
one pointer with no re-spawn, no copy, and no second allocation, so **the payload's
`RestoredActor` and the final actor are the same object by construction** — provided the
`SpawnRestoredActor` implementation returns the one actor it spawned and does not spawn twice.

The obligation this places on the `SpawnRestoredActor` Blueprint (a different asset, owned by
another lane) is therefore exactly two lines:

- spawn `BP_Restored_LanternPost` **once**, and assign it to `OutSpawnedActor`;
- write the `PointIndex` it was given into the new actor's `RestorationPointIndex` before
  returning, so the actor carries its own provenance.

`RestorationPointIndex` is `expose_on_spawn`, so it can be set on the `SpawnActor` node itself
rather than in a follow-up call — which also removes the window where a half-configured actor
exists.

**Finding you need, because it changes what "verifying identity" can mean.** Nothing reads
`FRestorationEventPayload::RestoredActor`. There is exactly one writer
(`RitualPlacementComponent.cpp:352`) and **zero readers** — all five `OnStructureRestored`
subscribers were checked (`VeilHeart.cpp:100`, `GloamsteadMeshForgeAdapterSubsystem.cpp:328`,
`GloamsteadFirstNightDirector.cpp:185`, `NightConsequenceRuntime.cpp:206`,
`NightConsequenceManager.cpp:56`) and none touch the field, and a name-table scan of all
`.uasset` files under `Content/` finds no reference to it. So "the payload's `RestoredActor`
and the final actor are the same object" is currently true *and unobservable at runtime*. You
cannot verify it by watching behaviour; verify it by reading the pass-through above, or add a
temporary log in a listener. Do not let a green playtest be mistaken for evidence on this point.

### Failure — and the C++ gap this actor cannot close

The requirement is that a failed spawn produce a visible structured failure rather than a
silent partial success. **This actor can only cover half of it, and you should know which half.**

**[verified] The silent-partial-success path is live in C++ today.** Read
`RitualPlacementComponent.cpp:113-144`: after `SpawnRestoredActor` returns, `SpawnedActor` is
**never null-checked**. `BuildRestorationPayload` does not fail on a null actor — it assigns
the null into `RestoredActor` at `:352` and returns true.
`CommitRestorationWithEvidence` (`:127`, defined at `:147`) does not inspect it either. So if
`SpawnRestoredActor` produces nothing, `ConfirmPlacement` still returns
`true`, still logs the success line at `:140-141`
(*"RitualPlacement: Restored point %d type %d (request %s)"*),
the point is marked restored, the evidence artifact is published, and the light/corruption
deltas are applied — **with no actor anywhere in the world.** That is precisely the failure
mode the requirement names, and it is not reachable from this Blueprint. Note that the new
evidence artifact will therefore record a successful restoration for a lantern that does not
visually exist.

The two paths that *do* clean up are the ones where the actor spawned fine and something
*after* it failed: `:119-122` and `:129-132`, which destroy the actor and log a warning. Both use
`LogTemp`. There is no dedicated log category and **no player-facing failure surface of any
kind** in the restoration system — the only `AddOnScreenDebugMessage` in the project is in
`GloamsteadCycleFeedbackSubsystem.cpp:150`, which is day/night feedback.

**What this actor does about it (its half):** nothing, deliberately — an actor that failed to
spawn cannot report anything. The failure has to be reported by the code that *tried* to spawn it.

**What must be done in `SpawnRestoredActor`'s Blueprint (the other lane's half), specified here
because this recipe is where the contract lives:**

1. After `SpawnActor from Class`, branch on `IsValid(SpawnedActor)`.
2. On the invalid branch, before returning: `Print String` with
   `[Gloam][FirstLantern][FAIL] restored-actor spawn returned null for point <PointIndex>`,
   duration 8 s, colour red, **and** a `PrintText`/log at Error severity so it survives outside
   PIE. Leave `OutSpawnedActor` null.
3. Do **not** try to fake success by returning some other actor.

**Recommended C++ follow-up, out of scope for this lane but the only real fix
(`RitualPlacementComponent.cpp`, immediately after the `SpawnRestoredActor` call at `:114`):**
treat a null `SpawnedActor` as a failure
path exactly like the other two — log a warning and `return false` before
`BuildRestorationPayload`. Until that lands, step 2 above is the only thing standing between a
failed spawn and a point that is permanently marked restored with nothing to show for it.
File this as a separate task; **this lane did not change any C++.**

---

## 5. The script

Authored against bindings read from `Plugins/NeoStackAI/Source/NeoStackAI/`, not from memory.
It has **not been run**. It is idempotent-hostile on purpose: it aborts if the Blueprint
already exists, because `add_timeline` appends tracks and **fails the whole call** if a track
name is already in use (`BlueprintUtils.cpp:1589,1606`), so a second run against an existing
asset would half-work. Delete the asset to re-run.

Every graph operation is checked, and the script prints a `[LANE4] WIRING` summary at the end.
**If that summary is not `0 failures`, the graph is incomplete — finish it by hand from the
node/pin tables in §3 and §4 rather than assuming it worked.** That is the same principle the
actor itself is specified under: no silent partial success.

```lua
-- ============================================================================
-- Lane 4 - BP_Restored_LanternPost  (restored first lantern, Gloamstead)
-- Creates: /Game/Gloamstead/Restoration/FirstLantern/BP_Restored_LanternPost
-- Touches nothing else. Does not build. Does not commit.
-- ============================================================================

local BP    = "/Game/Gloamstead/Restoration/FirstLantern/BP_Restored_LanternPost"
local GRAPH = "EventGraph"

local SM_CUBE  = "/Game/LevelPrototyping/Meshes/SM_Cube.SM_Cube"
local SM_CYL   = "/Game/LevelPrototyping/Meshes/SM_Cylinder.SM_Cylinder"
local MI_CLEAN = "/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray.MI_PrototypeGrid_Gray"
local M_EMISS  = "/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial"
local SW_TONE  = "/Engine/EngineSounds/1kSineTonePing.1kSineTonePing"
local NS_MOTES = "/Game/Gloamstead/Restoration/FirstLantern/NS_FirstLanternMotes"

local fails = {}
local function fail(what) fails[#fails + 1] = what end
local function chk(ok, what) if not ok then fail(what) end return ok end

-- ---------------------------------------------------------------- 0. guard
if asset_exists(BP) then
  log("[LANE4][ABORT] " .. BP .. " already exists.")
  log("[LANE4][ABORT] add_timeline cannot re-add existing tracks; delete the asset and re-run.")
  return
end

local bp = create_asset(BP, "Blueprint", { ParentClass = "/Script/Engine.Actor" })
if not bp then log("[LANE4][ABORT] create_asset failed for " .. BP); return end
log("[LANE4] created " .. BP)

-- ---------------------------------------------------------- 1. components
chk(bp:add_component("Collar", "StaticMeshComponent"), "add_component Collar")
chk(bp:set("Collar", "StaticMesh",       SM_CUBE),                "Collar.StaticMesh")
chk(bp:set("Collar", "OverrideMaterials", "(" .. MI_CLEAN .. ")"), "Collar.OverrideMaterials")
chk(bp:set("Collar", "RelativeLocation", "(X=-5,Y=-30,Z=460)"),    "Collar.RelativeLocation")
chk(bp:set("Collar", "RelativeScale3D",  "(X=0.9,Y=0.9,Z=0.4)"),   "Collar.RelativeScale3D")

chk(bp:add_component("Head", "StaticMeshComponent"), "add_component Head")
chk(bp:set("Head", "StaticMesh",        SM_CUBE),                 "Head.StaticMesh")
chk(bp:set("Head", "OverrideMaterials", "(" .. MI_CLEAN .. ")"),  "Head.OverrideMaterials")
chk(bp:set("Head", "RelativeLocation",  "(X=-25,Y=-50,Z=490)"),   "Head.RelativeLocation")
chk(bp:set("Head", "RelativeScale3D",   "(X=1.3,Y=1.3,Z=1.5)"),   "Head.RelativeScale3D")

-- Halo: additive + unlit. Color=(0,0,0) contributes nothing, so "absent" is real,
-- not a faked opacity. The vector param is literally named "Color".
chk(bp:add_component("Halo", "StaticMeshComponent"), "add_component Halo")
chk(bp:set("Halo", "StaticMesh",        SM_CYL),                  "Halo.StaticMesh")
chk(bp:set("Halo", "OverrideMaterials", "(" .. M_EMISS .. ")"),   "Halo.OverrideMaterials")
chk(bp:set("Halo", "RelativeLocation",  "(X=-45,Y=-70,Z=470)"),   "Halo.RelativeLocation")
chk(bp:set("Halo", "RelativeScale3D",   "(X=1.7,Y=1.7,Z=1.9)"),   "Halo.RelativeScale3D")
chk(bp:set("Halo", "CastShadow",        "false"),                 "Halo.CastShadow")

-- Glow: Movable is mandatory - no lightmass (r.AllowStaticLighting=False) AND
-- SetAttenuationRadius is gated on AreDynamicDataChangesAllowed(false).
chk(bp:add_component("Glow", "PointLightComponent"), "add_component Glow")
chk(bp:set("Glow", "Mobility",          "Movable"),               "Glow.Mobility")
chk(bp:set("Glow", "IntensityUnits",    "Candelas"),              "Glow.IntensityUnits")
chk(bp:set("Glow", "Intensity",         "0.0"),                   "Glow.Intensity")
chk(bp:set("Glow", "AttenuationRadius", "300.0"),                 "Glow.AttenuationRadius")
chk(bp:set("Glow", "SourceRadius",      "12.0"),                  "Glow.SourceRadius")
chk(bp:set("Glow", "LightColor",        "(R=255,G=196,B=132,A=255)"), "Glow.LightColor")
chk(bp:set("Glow", "RelativeLocation",  "(X=40,Y=15,Z=565)"),     "Glow.RelativeLocation")

chk(bp:add_component("Motes", "NiagaraComponent"), "add_component Motes")
chk(bp:set("Motes", "RelativeLocation", "(X=40,Y=15,Z=545)"),     "Motes.RelativeLocation")
chk(bp:set("Motes", "bAutoActivate",    "true"),                  "Motes.bAutoActivate")
if asset_exists(NS_MOTES) then
  chk(bp:set("Motes", "Asset", NS_MOTES), "Motes.Asset")
else
  log("[LANE4][PENDING] " .. NS_MOTES .. " does not exist yet - Motes.Asset left empty.")
  log("[LANE4][PENDING] Author it (see recipe section 2), then set Motes.Asset by hand.")
end

chk(bp:add_component("AudioOnset", "AudioComponent"), "add_component AudioOnset")
chk(bp:set("AudioOnset", "Sound",            SW_TONE),            "AudioOnset.Sound")
chk(bp:set("AudioOnset", "bAutoActivate",    "false"),            "AudioOnset.bAutoActivate")
chk(bp:set("AudioOnset", "PitchMultiplier",  "0.5"),              "AudioOnset.PitchMultiplier")
chk(bp:set("AudioOnset", "VolumeMultiplier", "0.55"),             "AudioOnset.VolumeMultiplier")
chk(bp:set("AudioOnset", "RelativeLocation", "(X=40,Y=15,Z=565)"),"AudioOnset.RelativeLocation")

chk(bp:add_component("AudioSettle", "AudioComponent"), "add_component AudioSettle")
chk(bp:set("AudioSettle", "Sound",            SW_TONE),           "AudioSettle.Sound")
chk(bp:set("AudioSettle", "bAutoActivate",    "false"),           "AudioSettle.bAutoActivate")
chk(bp:set("AudioSettle", "PitchMultiplier",  "0.75"),            "AudioSettle.PitchMultiplier")
chk(bp:set("AudioSettle", "VolumeMultiplier", "0.35"),            "AudioSettle.VolumeMultiplier")
chk(bp:set("AudioSettle", "RelativeLocation", "(X=40,Y=15,Z=565)"),"AudioSettle.RelativeLocation")

-- ----------------------------------------------------------- 2. variables
chk(bp:add_variable("bMaterialized", "bool",
      { default = false, category = "Gloamstead|Restoration", blueprint_read_only = true,
        tooltip = "True once TL_Materialize has finished. Never cleared." }),
    "add_variable bMaterialized")

chk(bp:add_variable("RestorationPointIndex", "int",
      { default = -1, category = "Gloamstead|Restoration", expose_on_spawn = true,
        edit_instance_only = true,
        tooltip = "PCG point index this lantern was restored for. Stamped by SpawnRestoredActor." }),
    "add_variable RestorationPointIndex")

-- Compile so the components/variables register in the Blueprint action database,
-- otherwise find_nodes("Get Glow") has nothing to find.
bp:compile()

-- ------------------------------------------------------------ 3. timeline
-- interp_mode is deliberately NOT passed: the timeline path discards it and calls
-- AutoSetTangents() instead. See recipe section 3.
local tl = bp:add_timeline("TL_Materialize", {
  length = 1.6, auto_play = false, loop = false,
  tracks = {
    { name = "LightIntensity", type = "float", keys = {
        { time = 0.00, value = 0   }, { time = 0.35, value = 95  },
        { time = 0.80, value = 340 }, { time = 1.15, value = 250 },
        { time = 1.60, value = 280 } } },
    { name = "LightRadius", type = "float", keys = {
        { time = 0.00, value = 300 }, { time = 0.35, value = 520 },
        { time = 0.80, value = 880 }, { time = 1.15, value = 900 },
        { time = 1.60, value = 900 } } },
    { name = "GlowColor", type = "vector", keys = {
        { time = 0.00, x = 0.00, y = 0.00, z = 0.00 },
        { time = 0.35, x = 0.30, y = 0.17, z = 0.07 },
        { time = 0.80, x = 1.55, y = 0.88, z = 0.37 },
        { time = 1.15, x = 0.95, y = 0.54, z = 0.22 },
        { time = 1.60, x = 1.10, y = 0.62, z = 0.26 } } },
    { name = "SettleCue", type = "event", keys = { 1.15 } },
  }
})
if not tl then log("[LANE4][ABORT] add_timeline failed - stopping before graph work."); return end
bp:compile()

-- --------------------------------------------------------- 4. graph tools
local function pick(query, name_frag, class_frag)
  local hits = find_nodes(query, BP, GRAPH, 30)
  if not hits then return nil end
  local best
  for _, h in ipairs(hits) do
    local nm = string.lower(tostring(h.name or ""))
    local oc = string.lower(tostring(h.owning_class or ""))
    local nm_ok = (name_frag == nil) or (string.find(nm, string.lower(name_frag), 1, true) ~= nil)
    local oc_ok = (class_frag == nil) or (string.find(oc, string.lower(class_frag), 1, true) ~= nil)
    if nm_ok and oc_ok then
      if (best == nil) or ((h.score or 0) > (best.score or 0)) then best = h end
    end
  end
  return best
end

local function spawn(label, query, name_frag, class_frag, x, y)
  local hit = pick(query, name_frag, class_frag)
  if not hit then fail("find_nodes: " .. label); return nil end
  local n = add_node(BP, GRAPH, hit, x, y)
  if not n then fail("add_node: " .. label); return nil end
  log(string.format("[LANE4] node %-22s -> %s", label, tostring(n.name)))
  return n
end

-- Resolve a real pin name from a node's pin list. Handles both the string form
-- and the table form, exact match first then substring.
local function pin(node, dir, candidates, label)
  if not node then return nil end
  local list = (dir == "in") and node.pins_in or node.pins_out
  if not list then fail("pins missing: " .. label); return nil end
  local names = {}
  for _, p in ipairs(list) do
    names[#names + 1] = (type(p) == "table") and tostring(p.name or "") or tostring(p)
  end
  for _, c in ipairs(candidates) do
    for _, nm in ipairs(names) do
      if string.lower(nm) == string.lower(c) then return nm end
    end
  end
  for _, c in ipairs(candidates) do
    for _, nm in ipairs(names) do
      if string.find(string.lower(nm), string.lower(c), 1, true) then return nm end
    end
  end
  fail("pin not found: " .. label .. " (candidates: " .. table.concat(candidates, "/")
       .. " | actual: " .. table.concat(names, ", ") .. ")")
  return nil
end

local function wire(label, a, ap, b, bpin)
  if not (a and ap and b and bpin) then fail("wire (missing endpoint): " .. label); return end
  if not connect(a, ap, b, bpin) then fail("connect: " .. label) end
end

-- --------------------------------------------------- 5. anchor + tl node
local rg = read_graph(BP, GRAPH)
if not rg then log("[LANE4][ABORT] read_graph failed."); return end

local begin_play, tlnode
for _, n in ipairs(rg.nodes) do
  if n.handle == tl.handle then tlnode = n end
  if tostring(n.name) == "Event BeginPlay" then begin_play = n end
end

if not begin_play then
  local ev = bp:override_function("ReceiveBeginPlay")
  if ev and ev.handle then
    local rg2 = read_graph(BP, GRAPH)
    for _, n in ipairs(rg2.nodes) do if n.handle == ev.handle then begin_play = n end end
  end
end
if not begin_play then log("[LANE4][ABORT] no Event BeginPlay in the graph."); return end
if not tlnode      then log("[LANE4][ABORT] timeline node not found in the graph."); return end

log("[LANE4] timeline pins IN : " .. (function()
  local t = {} for _, p in ipairs(tlnode.pins_in or {}) do
    t[#t+1] = (type(p)=="table") and tostring(p.name) or tostring(p) end
  return table.concat(t, ", ") end)())
log("[LANE4] timeline pins OUT: " .. (function()
  local t = {} for _, p in ipairs(tlnode.pins_out or {}) do
    t[#t+1] = (type(p)=="table") and tostring(p.name) or tostring(p) end
  return table.concat(t, ", ") end)())

-- ------------------------------------------------------------- 6. nodes
local g_onset  = spawn("Get AudioOnset",  "Get AudioOnset",  "AudioOnset",  nil, -400,  180)
local g_settle = spawn("Get AudioSettle", "Get AudioSettle", "AudioSettle", nil,  700,  620)
local g_glow   = spawn("Get Glow",        "Get Glow",        "Glow",        nil,  700,  180)
local g_halo   = spawn("Get Halo",        "Get Halo",        "Halo",        nil,  700,  460)

local n_play1  = spawn("Play onset",   "Play", "play", "AudioComponent",   -160,   60)
local n_play2  = spawn("Play settle",  "Play", "play", "AudioComponent",   1000,  560)
local n_int    = spawn("SetIntensity", "Set Intensity", "set intensity", "LightComponent",       1000,   60)
local n_rad    = spawn("SetAttenRadius", "Set Attenuation Radius", "attenuation radius", "LocalLightComponent", 1360,   60)
local n_vec    = spawn("SetVecParamOnMats", "Set Vector Parameter Value on Materials",
                       "vector parameter value on materials", "MeshComponent",                    1720,   60)
local n_flag   = spawn("Set bMaterialized", "Set bMaterialized", "bmaterialized", nil,           1000,  820)

-- ------------------------------------------------------- 7. pin resolution
local BP_THEN   = pin(begin_play, "out", { "then" }, "BeginPlay.then")
local TL_PLAY   = pin(tlnode, "in",  { "Play from Start", "PlayFromStart", "Play" }, "TL.PlayFromStart")
local TL_UPD    = pin(tlnode, "out", { "Update" },        "TL.Update")
local TL_FIN    = pin(tlnode, "out", { "Finished" },      "TL.Finished")
local TL_INT    = pin(tlnode, "out", { "LightIntensity" },"TL.LightIntensity")
local TL_RAD    = pin(tlnode, "out", { "LightRadius" },   "TL.LightRadius")
local TL_COL    = pin(tlnode, "out", { "GlowColor" },     "TL.GlowColor")
local TL_CUE    = pin(tlnode, "out", { "SettleCue" },     "TL.SettleCue")

local P1_IN     = pin(n_play1, "in",  { "execute" }, "Play1.execute")
local P1_OUT    = pin(n_play1, "out", { "then" },    "Play1.then")
local P1_TGT    = pin(n_play1, "in",  { "Target", "self" }, "Play1.Target")
local P2_IN     = pin(n_play2, "in",  { "execute" }, "Play2.execute")
local P2_TGT    = pin(n_play2, "in",  { "Target", "self" }, "Play2.Target")

local I_IN      = pin(n_int, "in",  { "execute" },        "SetIntensity.execute")
local I_OUT     = pin(n_int, "out", { "then" },           "SetIntensity.then")
local I_TGT     = pin(n_int, "in",  { "Target", "self" }, "SetIntensity.Target")
local I_VAL     = pin(n_int, "in",  { "New Intensity", "NewIntensity", "Intensity" }, "SetIntensity.NewIntensity")

local R_IN      = pin(n_rad, "in",  { "execute" },        "SetRadius.execute")
local R_OUT     = pin(n_rad, "out", { "then" },           "SetRadius.then")
local R_TGT     = pin(n_rad, "in",  { "Target", "self" }, "SetRadius.Target")
local R_VAL     = pin(n_rad, "in",  { "New Radius", "NewRadius", "Radius" }, "SetRadius.NewRadius")

local V_IN      = pin(n_vec, "in",  { "execute" },        "SetVec.execute")
local V_TGT     = pin(n_vec, "in",  { "Target", "self" }, "SetVec.Target")
local V_NAME    = pin(n_vec, "in",  { "Parameter Name", "ParameterName" },  "SetVec.ParameterName")
local V_VAL     = pin(n_vec, "in",  { "Parameter Value", "ParameterValue" },"SetVec.ParameterValue")

local F_IN      = pin(n_flag, "in", { "execute" }, "SetFlag.execute")
local F_VAL     = pin(n_flag, "in", { "bMaterialized" }, "SetFlag.bMaterialized")

local GO_OUT    = g_onset  and pin(g_onset,  "out", { "AudioOnset"  }, "GetAudioOnset.out")
local GS_OUT    = g_settle and pin(g_settle, "out", { "AudioSettle" }, "GetAudioSettle.out")
local GG_OUT    = g_glow   and pin(g_glow,   "out", { "Glow" },        "GetGlow.out")
local GH_OUT    = g_halo   and pin(g_halo,   "out", { "Halo" },        "GetHalo.out")

-- ------------------------------------------------------------ 8. wiring
-- exec: BeginPlay -> Play(onset) -> Timeline.PlayFromStart
wire("BeginPlay -> Play(onset)", begin_play.handle, BP_THEN, n_play1 and n_play1.handle, P1_IN)
wire("Play(onset) -> TL.Play",   n_play1 and n_play1.handle, P1_OUT, tlnode.handle, TL_PLAY)

-- exec: Timeline.Update -> SetIntensity -> SetAttenuationRadius -> SetVectorParam
wire("TL.Update -> SetIntensity", tlnode.handle, TL_UPD, n_int and n_int.handle, I_IN)
wire("SetIntensity -> SetRadius", n_int and n_int.handle, I_OUT, n_rad and n_rad.handle, R_IN)
wire("SetRadius -> SetVec",       n_rad and n_rad.handle, R_OUT, n_vec and n_vec.handle, V_IN)

-- exec: Timeline.SettleCue -> Play(settle)
wire("TL.SettleCue -> Play(settle)", tlnode.handle, TL_CUE, n_play2 and n_play2.handle, P2_IN)

-- exec: Timeline.Finished -> Set bMaterialized
wire("TL.Finished -> SetFlag", tlnode.handle, TL_FIN, n_flag and n_flag.handle, F_IN)

-- data: targets
wire("AudioOnset -> Play1.Target",  g_onset  and g_onset.handle,  GO_OUT, n_play1 and n_play1.handle, P1_TGT)
wire("AudioSettle -> Play2.Target", g_settle and g_settle.handle, GS_OUT, n_play2 and n_play2.handle, P2_TGT)
wire("Glow -> SetIntensity.Target", g_glow   and g_glow.handle,   GG_OUT, n_int  and n_int.handle,  I_TGT)
wire("Glow -> SetRadius.Target",    g_glow   and g_glow.handle,   GG_OUT, n_rad  and n_rad.handle,  R_TGT)
wire("Halo -> SetVec.Target",       g_halo   and g_halo.handle,   GH_OUT, n_vec  and n_vec.handle,  V_TGT)

-- data: track values
wire("TL.LightIntensity -> NewIntensity", tlnode.handle, TL_INT, n_int and n_int.handle, I_VAL)
wire("TL.LightRadius -> NewRadius",       tlnode.handle, TL_RAD, n_rad and n_rad.handle, R_VAL)
wire("TL.GlowColor -> ParameterValue",    tlnode.handle, TL_COL, n_vec and n_vec.handle, V_VAL)

-- literals
if n_vec and V_NAME then
  if not set_pin(n_vec.handle, V_NAME, "Color") then fail("set_pin SetVec.ParameterName") end
end
if n_flag and F_VAL then
  if not set_pin(n_flag.handle, F_VAL, true) then fail("set_pin SetFlag.bMaterialized") end
end

-- ----------------------------------------------------------- 9. persist
bp:compile()
bp:save()

log("========================================================================")
if #fails == 0 then
  log("[LANE4] WIRING: 0 failures. " .. BP .. " authored and saved.")
else
  log("[LANE4] WIRING: " .. #fails .. " FAILURES - the graph is INCOMPLETE.")
  for i, f in ipairs(fails) do log(string.format("[LANE4]   %2d. %s", i, f)) end
  log("[LANE4] Finish the missing wiring by hand from recipe sections 3 and 4.")
  log("[LANE4] Do NOT treat this actor as done until this line reads 0 failures.")
end
log("========================================================================")
```

---

## 6. Verify block

**Command:** paste §5 into NeoStack `execute_script` and run it once.

**Paths touched by the script:**
- `Content/Gloamstead/Restoration/FirstLantern/BP_Restored_LanternPost.uasset` (created)

**Paths touched by this lane in the repo:**
- `docs/gloamstead/recipes/lane4-restored-lantern.md` (this file, created)

Nothing else. No C++, no level, no config, no commit, no build.

**What you should observe when it works:**

1. The script's final line reads `[LANE4] WIRING: 0 failures.` Anything else means the graph is
   partial — read the enumerated failures, they name the exact node or pin.
2. The trace contains two `[LANE4] timeline pins` lines. **Read them.** They are the ground
   truth for the timeline pin names, which are documented nowhere in the plugin; if the
   resolver picked something odd, this is where you will see it.
3. Content Browser shows `BP_Restored_LanternPost` under `Gloamstead/Restoration/FirstLantern`.
   Open it: 7 components, 2 variables, and an EventGraph whose BeginPlay chain reaches
   `TL_Materialize`, whose `Update` reaches three setters, and whose `Finished` reaches
   `Set bMaterialized`.
4. Expect one `[LANE4][PENDING]` line about `NS_FirstLanternMotes` until that asset exists.
   That is correct behaviour, not a failure.

**To see it in the level (greybox check, before the spawner exists):** drag one instance to
world `(-800, -1000, 0)` with zero rotation. On Play the head should appear lit on top of the
existing broken post, a warm pool should spread across the plinth and reach the rubble block
~4 m away, and two soft tones should sound 1.15 s apart. Remember that a hand-placed instance
does **not** pick up later Blueprint default edits — delete and re-place after any change.

---

## 7. Assumptions this lane could not verify

Every one of these is a thing that could make the first run imperfect. None of them are guesses
about the C++ contract or the light maths, which are cited above.

1. **[assumed]** `bp:add_component`, `bp:set`, and `bp:add_timeline` behave as their plugin
   source reads. The bindings and their help text were read directly
   (`LuaBinding_Blueprint.cpp:1306-1336`, `:1621-1830`, `:2257-2440`, `:5060-5157`), but no call
   was executed this session. `bp:set` and `bp:add_timeline` have **no prior usage precedent in
   this project**; `bp:add_component` + `bp:set` do (`…\scratchpad\lua\bp_heart2.lua:8-26`).
2. **[assumed]** Timeline pin names are `Play from Start`, `Update`, `Finished`, and one pin per
   track named after the track. These are engine conventions for `UK2Node_Timeline` and are
   documented nowhere in the plugin. The script resolves them dynamically and logs what it
   found rather than hardcoding them, which is why step 2 of the Verify block matters.
3. **[assumed]** `find_nodes` surfaces component variable getters as `Get <ComponentName>` once
   the Blueprint has been compiled. The script compiles before graph work for this reason. If
   the getters do not appear, the `find_nodes:` failures in the summary will name them.
4. **[assumed]** `OverrideMaterials` accepts the parenthesised single-element array literal
   `"(/Path/Asset.Asset)"`. This is standard `ImportText` array syntax and `bp:set` routes to
   `ImportText_InContainer` (`BlueprintUtils.cpp:842`), but it was not executed.
5. **[assumed]** `IntensityUnits` accepts the bare enum entry name `"Candelas"`. If it does not,
   the trace will print the valid list verbatim (`BlueprintUtils.cpp:851-876`); retry with
   `"ELightUnits::Candelas"`.
6. **[assumed]** The Niagara plugin is enabled, so `add_component("Motes", "NiagaraComponent")`
   resolves. Indirect evidence: `Content/` contains three `NS_*` assets and
   `Gloamstead5_8.uproject:72` enables `PCGNiagaraInterop`. Not directly confirmed.
7. **[assumed]** The effective fixed exposure. `r.EyeAdaptationQuality=0` is verified; the EV it
   resolves to is not, and no PostProcessVolume in `Lvl_Gloamstead` was inspected. This affects
   only the emissive values, which is why §3 gives a tuning band and a colour-based acceptance
   criterion instead of a single number.
8. **[assumed]** The PCG ritual point for this site is at or near `(-800, -1000, 0)`. This lane
   could not read the generated PCG point data. If it is elsewhere, the crown misses the post.
9. **[assumed]** `SM_Cylinder`'s base size is 100 x 100 x 100 like `SM_Cube`. Inferred from
   `stage3.lua:30`, where a 220 x 220 x 120 plinth is authored as scale `2.2, 2.2, 1.2`.
10. **[assumed]** `1kSineTonePing` loads and is audible in PIE. The `.uasset` is confirmed on
    disk at `D:\UE_5.8\Engine\Content\EngineSounds\`; it was not played.
