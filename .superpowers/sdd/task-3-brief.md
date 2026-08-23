# Task 3: Gloamstead generated-asset catalog and runtime provider

Work only in `D:\Unreal Projects\.worktrees\gloamstead-production-asset-forge` on `codex/gloamstead-production-asset-forge`, based at `362076f3395a24844703000b5d685b8ebd79948f`.

## Mission

Implement Gloamstead-owned immutable generated-asset selection behind the existing MeshForge provider seam. This lane owns runtime consumer semantics only; it must not generate assets, mutate gameplay/save truth, or add Gloamstead defaults to WorldForge.

## Owned paths

- new `Source/Gloamstead/Data/GloamsteadGeneratedAssetCatalog.{h,cpp}`
- new `Source/Gloamstead/Settings/GloamsteadGeneratedAssetSettings.{h,cpp}`
- `Source/Gloamstead/Systems/GloamsteadMeshForgeProvider.{h,cpp}`
- `Source/Gloamstead/Systems/GloamsteadMeshForgeAdapterSubsystem.{h,cpp}`
- `Source/Gloamstead/Data/GloamsteadMeshForgeTypes.{h,cpp}` only where needed for honest generated provenance/failures
- new `Source/Gloamstead/Tests/GeneratedAssetCatalogTests.cpp`, and narrowly relevant existing MeshForge tests
- `Source/Gloamstead/Gloamstead.Build.cs` only for needed runtime module dependencies
- `Config/DefaultGame.ini` only for an explicit development-safe provider selection default; never hardcode a production asset path

Do not touch plugin vendor files, JSON intent/acceptance files, maps, Content binaries, PCG graphs, WorldForge, save/gameplay authority, or generated reports.

## Contract

Add a `UPrimaryDataAsset` catalog owned by Gloamstead with:

- immutable `BundleId` (active kit/version identity) and `ReceiptSha256`
- immutable Unreal root `/Game/Gloamstead/Generated/Biomes/Sanctuary/<version>`
- entries keyed uniquely by caller-owned semantic role plus restoration state
- soft object reference, expected class, object SHA-256/receipt binding, dependencies, ownership and license identifiers
- placement/material/mesh/VFX capable generic entries; the MeshForge provider may consume only static meshes/materials it understands

Provide pure fail-closed validation with stable `GACxxx` failures for empty/invalid IDs, duplicate keys, unknown state, path outside version root, entry path not matching root/version, missing receipt hash, missing ownership/license/class/hash, unsupported provider asset class, stale bundle/receipt mismatch, and soft-reference load failure. No path repair, inferred state, first-match, or fallback within the generated provider.

Add `UGloamsteadGeneratedAssetSettings` (`UDeveloperSettings`) selecting:

- provider mode: generated catalog or explicit engine-primitive development fallback
- soft catalog asset path
- expected active bundle id and expected receipt hash

The production/generated mode must fail loudly if settings/catalog/entry/reference/class/receipt do not match. The primitive provider remains available only when explicitly configured as development fallback; never silently activate it after a generated-provider failure.

Implement `UGloamsteadGeneratedAssetMeshForgeProvider` behind `UGloamsteadMeshForgeProvider`. It must:

- load the configured catalog through `TSoftObjectPtr`/streamable infrastructure; expose an async preload seam and deterministic ready/failed state
- resolve an exact role/state entry (no best match), load through the catalog soft reference, validate expected class, and spawn an honest generated-owned proxy whose `GeneratedAssetPath`, bundle id, receipt hash, provider type, ownership, and failures are reportable
- project gameplay inputs one way: restoration/corruption/day phase/wetness/warning select state/material parameters only; never write back to gameplay systems
- reject before spawning on missing/stale/wrong-class/receipt mismatch

Update the adapter so provider creation follows settings. Generated mode waits for catalog readiness before building. Replace `FindHeart` first-actor guessing with `UGloamsteadSurveySubjectRegistry::ResolveSubject("sanctuary.heart")`; ambiguity/unresolved means no Heart proxy and a reported failure, never `Found[0]`. Tests may preserve an explicit test seam.

Expand instance/report contracts so generated-owned reports are valid only when every generated instance is version-root/receipt/bundle closed. Preserve all existing primitive-provider invariants and tests.

## Tests and proof

TDD with automation tests covering:

- valid catalog and exact lookup
- duplicate key/path escape/wrong root/stale receipt/missing license/class/hash rejected
- explicit primitive fallback remains valid
- generated mode never falls back on missing catalog/entry/load failure
- role/state exactness
- generated instance provenance validation
- registry-based Heart resolution rejects ambiguity instead of choosing first actor
- gameplay state remains unchanged

Run the repository build/test command available for C++ source changes, or if UE 5.8 is unavailable, run every source-level/contract gate possible and report the runtime build as blocked rather than claiming it. Use `apply_patch`, stage only owned paths, commit with `Co-Authored-By: Codex <codex@openai.com>`, and write `.superpowers/sdd/task-3-report.md`.
