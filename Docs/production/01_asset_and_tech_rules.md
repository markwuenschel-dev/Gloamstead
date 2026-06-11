# Asset and Tech Rules

_Practical production rules for UE5 and premade assets._

## UE5 Direction

Use UE5 for atmosphere, lighting, third-person control, environmental detail, VFX, and rapid iteration.

Recommended features:

- Lumen for dynamic lighting if performance allows
- Niagara for Heart particles, corruption, fog, cleansing, and night effects
- Nanite for ruins/rocks where appropriate
- Data Assets for restoration definitions and night rules
- Curves/Data Tables for tuning warnings, clarity, corruption, and consequence intensity

## Premade Asset Strategy

Premade assets are encouraged for:

- ruins
- environments
- materials
- VFX primitives
- third-person controller base
- simple enemy animations
- sound ambience

Premade systems should not dictate the game's identity.

Avoid importing large survival/crafting/base-defense frameworks unless used only as references or isolated parts.

## Art Asset Selection

Prefer assets that support:

- realistic dark fantasy
- ruined environments
- bleak natural spaces
- readable silhouettes
- subtle magical restoration
- cyan/teal/warm light contrast

Avoid assets that push the game toward:

- cartoon cozy village sim
- bright MMO fantasy
- gothic horror gore
- sci-fi
- tower defense toy style
- survival-crafting clutter

## Agent Implementation Rule

Before implementing any feature, identify which core doc justifies it. If no doc justifies it, do not add it.

## Data Asset Guidelines (for Catalogs and Ritual Definitions)

See docs/systems/03_night_consequence_system.md "Data Asset Authoring Guide for Designers" for complete specs on:

- UNightConsequenceCatalog and FNightConsequenceRule (for night selection).
- UVeilHeartWarningCatalog and FVeilHeartWarningFragment (for warnings/reflection).
- URitualDefinition and concrete assets (for per-ritual tuning, including new MirrorPillar/BellShrine).

Use UPrimaryDataAsset + Data Tables/Curves for all tunable values. This keeps core loop data-driven and designer-iterable.

Update enums in Source/Gloamstead/Data/*.h when adding types (e.g. new rituals or nights).

Assign assets per [specs/data/WIRING.md](../../specs/data/WIRING.md). PIE smoke: Level Blueprint **Advance Gloamstead Day Phase** (see [specs/data/VERIFICATION-2026-06-11.md](../../specs/data/VERIFICATION-2026-06-11.md)).

**Starter assets (2026-06-11):** Six `Content/Data/DA_*` from import factory (`specs/data/vs-polish-starter.json`). Verified on `Lvl_ThirdPerson`. **Next:** PCG level init before restoration / non-Tutorial night smoke.

**Generation policy:** binary `Content/Data/` outputs only via `agent_collab/scripts/Invoke-GloamsteadDataAssetImport.ps1` + `GloamsteadImportDataAssets` commandlet (see `agent_collab/context/content_policy.json`).
