# Environment and Tooling (Gloamstead UE5 — Multi-Runtime agent_collab)

## Runtime Environment
- OS: Windows (primary)
- Shell: PowerShell 7+ (pwsh)
- Git + LFS (Content/** and .u* binaries)
- Unreal Engine 5.7

## Discovered Runtimes (from repo + environment)
- claude-code: .claude/ projection present, adapter at agent_collab/adapters/claude-code/. Supports full roles including Orchestrator via native subagents + hooks.
- grok: .grok/ projection present, adapter at agent_collab/adapters/grok/. Supports full roles including Orchestrator in this Grok Build / Cursor environment.

## Project Facts (see unreal_project.json, scope_roots.json)
- Mixed C++/Blueprint, heavy PCG.
- **Data Asset factory (2026-06-10):** `Source/GloamsteadEditor/` + `GloamsteadImportDataAssets` commandlet; manifests in `specs/data/`; human wrapper `agent_collab/scripts/Invoke-GloamsteadDataAssetImport.ps1`. Generated output: `Content/Data/`.
- LFS active.
- Vendor content: Content/ThirdPerson, Content/Characters (read-only).

## Build / Verification
- Manual per README: open .uproject, generate VS, build Development Editor for **Gloamstead** + **GloamsteadEditor**.
- **editor-generation:** run `Invoke-GloamsteadDataAssetImport.ps1` after compile (see `specs/data/HUMAN_RUN_IMPORT.md`).
- **compile / map-load:** verified 2026-06-11 on `Lvl_ThirdPerson` — see `specs/data/VERIFICATION-2026-06-11.md`.
- See verification_profiles.json for availability status.

## Collaboration
- Multi-runtime, lease-based Orchestrator.
- Strict UE5 content model: no direct binary edits; use manifest + commandlet for Content/Data/ assets.
