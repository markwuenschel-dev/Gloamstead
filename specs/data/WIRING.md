# Wiring Data Assets in the Editor (VS-POLISH-FACTORY-DATA-01)

Generated assets live under `Content/Data/` (see [vs-polish-starter.json](vs-polish-starter.json)).

**Verification record (2026-06-11):** [VERIFICATION-2026-06-11.md](VERIFICATION-2026-06-11.md)

## 1. Veil Heart — Warning Catalog

1. Open your test map (e.g. `Content/ThirdPerson/Lvl_ThirdPerson`).
2. **Content Browser** → `C++ Classes` → `Gloamstead` → `Systems` → **left-click and drag** `VeilHeart` into the viewport.
3. **Left-click** the placed actor in the **Outliner**.
4. In **Details** → category **Veil Heart** → **Warning Catalog** → pick `Content/Data/DA_VeilHeartWarningCatalog`.

> If **Veil Heart** category is missing: close editor, rebuild **GloamsteadEditor**, reopen, delete old `VeilHeart` actor, place a fresh one.

**Auto-fallback:** If Warning Catalog is empty, `BeginPlay` loads `/Game/Data/DA_VeilHeartWarningCatalog`.

## 2. Night catalog (subsystem — not in Outliner)

`UNightConsequenceManager` is a **world subsystem** — you cannot select it in the level.

**Auto-fallback:** At runtime it loads `/Game/Data/DA_NightConsequenceCatalog` when present; otherwise built-in MVP rules.

## 3. Ritual Placement — ritual definitions

1. Open **`Content/ThirdPerson/Blueprints/BP_ThirdPersonCharacter`**.
2. **Components** panel → **Add** → **Ritual Placement Component** (not the graph node “Add Ritual Placement Component”).
3. Select the component → **Details** → **Ritual Definitions**:

| Key | Value |
|-----|-------|
| `LanternPost` | `DA_Ritual_LanternPost` |
| `GardenBed` | `DA_Ritual_GardenBed` |
| `MirrorPillar` | `DA_Ritual_MirrorPillar` |
| `BellShrine` | `DA_Ritual_BellShrine` |

4. **Compile** and **Save**.

## PIE smoke (Level Blueprint test key)

**Lvl_ThirdPerson** → **Blueprints** → **Open Level Blueprint**:

1. **Right-click** → **Input** → **Keyboard Events** → **T** (Pressed).
2. **Right-click** → search **`Advance Gloamstead Day Phase`** (category **Gloamstead | DayNight**).
3. Wire **T Pressed** → **Advance Gloamstead Day Phase**.
4. **Compile** + **Save** the Level Blueprint and the level.

**Play:** click the **center 3D view** (not the Blueprint graph), press **T** repeatedly.

Expected log lines: `DayNight: phase`, `NightConsequenceManager: Loaded night catalog`, `Prepared night type`.

> Rebuild required if node missing: `agent_collab/scripts/Build-GloamsteadEditor.ps1` (close editor first).

## Verify

- **With assets assigned:** PIE at dusk uses catalog rules from `DA_NightConsequenceCatalog`; ritual restore uses `DA_Ritual_*` when PCG is initialized.
- **Without assets:** C++ MVP fallbacks (`PopulateMVPNightConsequenceRules`, code defaults).

## Known PIE behavior (verified 2026-06-11)

- Without PCG, every dusk selects **Tutorial** (zero sanctuary snapshot); pressing **T** many times does not change night type until PCG/restoration exists.
- **Tutorial** nights log `No catalog warning for night Tutorial` — no matching row in `DA_VeilHeartWarningCatalog` (optional manifest fix).

See [VERIFICATION-2026-06-11.md](VERIFICATION-2026-06-11.md) for full evidence and next steps.
