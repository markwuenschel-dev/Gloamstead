# Human Run: Data Asset Import (editor-generation profile)

## Prerequisites

- Unreal Engine 5.8 installed (this machine: `D:\UE_5.8`)
- Engine auto-detected from the `.uproject` EngineAssociation (registry / `D:\UE_<ver>` / Program Files). Override with `UE_ROOT` or `GLOAMSTEAD_UE_ENGINE` if needed.

## Steps

### 1. Compile (Development Editor)

1. Open `Gloamstead5_8.uproject` in Unreal Editor (or generate VS project files and build).
2. Build **Development Editor** for targets **Gloamstead** and **GloamsteadEditor**.

Close the editor before command-line builds if Live Coding blocks compile. Helper: `agent_collab/scripts/Build-GloamsteadEditor.ps1`, or the full build+test gate `gate.ps1` at the repo root.

### 2. Import assets from manifest

From repo root in PowerShell:

```powershell
pwsh -NoProfile -File agent_collab/scripts/Invoke-GloamsteadDataAssetImport.ps1 `
  -Manifest specs/data/vs-polish-starter.json
```

Expected output: list of `DA_*` under `Content/Data/`.

Logs: `Saved/Logs/GloamsteadImportDataAssets.log`, `Saved/Logs/GloamsteadImportDataAssets.cmd.log`

### 3. Map-load evidence (map-load profile)

1. Open `Lvl_ThirdPerson` (or test map).
2. Confirm no load errors for new Data Assets.
3. Wire assets per [WIRING.md](WIRING.md).

### 4. Record evidence for Critic / Orchestrator

Capture for handoff (see **[VERIFICATION-2026-06-11.md](VERIFICATION-2026-06-11.md)** for a completed example):

- Import script exit code 0
- Log excerpt: `GloamsteadImportDataAssets: success (7 assets)` (6 original + `DA_Ritual_PathPoint`, added 2026-07-10 / W1c)
- List of files in `Content/Data/DA_*.uasset`
- Map open success (no critical load errors)
- PIE: `NightConsequenceManager: Loaded night catalog from /Game/Data/DA_NightConsequenceCatalog`
- PIE: `DayNight: phase` transitions via test key

## Verified (2026-06-11)

Factory import + `Lvl_ThirdPerson` wiring + PIE day/night smoke **passed**. Dusk warning text and non-Tutorial nights **deferred** until PCG init — documented in [VERIFICATION-2026-06-11.md](VERIFICATION-2026-06-11.md).
