# Task 3 independent-review fix report

## Result

Resolved all five findings from the independent review of the generated catalog/provider slice.

1. Generated objects and every declared dependency are now bound to independent, exact-path, on-disk/cooked Asset Registry evidence before spawn. The generic importer contract is `WorldForge.ObjectSha256`, `WorldForge.ReceiptSha256`, and `WorldForge.BundleId`; missing evidence fails `GAC023` and mismatched evidence fails `GAC024`. Runtime code does not hash packaged `.uasset` files and test overrides compile only with `WITH_DEV_AUTOMATION_TESTS`.
2. Generated provider attempts fail explicitly on an unresolved world/location (`GAC025`) and on actor/visible-mesh spawn failure (`GAC026`). Generated report coverage counts only instances for which both `bSpawned` and `bVisibleProxyCreated` are true; `GMF024` rejects missing or overclaimed visibility coverage.
3. Ritual specs project `Wetness` (clamped to `[0,1]`) and `RecommendedForWarning` directly from authoritative PCG point metadata. The live-world test proves the projected values and proves both PCG gameplay state and the source metadata remain unchanged.
4. Configure/preload now uses monotonically increasing load generations plus the captured requested path. Reconfiguration cancels/releases the outstanding handle before state mutation, and stale completions cannot install a catalog or invoke the superseded completion delegate.
5. Catalog validation and exact lookup use reflected enum validity, rejecting values outside the declared enum such as `static_cast<EGloamsteadGeneratedAssetState>(255)` as `GAC006`.

The explicit primitive development fallback in `Config/DefaultGame.ini` is unchanged.

## Connected-impact changes

- Added the runtime `AssetRegistry` module dependency required to read cooked/on-disk provenance tags.
- Extended the existing PCG test-seeding seam to author the same `Wetness` and `RecommendedForWarning` metadata fields consumed by production code.
- Kept report aggregation, per-instance validation, adapter coverage counters, provider runtime behavior, and regression tests aligned with the stronger visibility contract.
- Communicated the three generic provenance tag names to the WorldForge importer lane; WorldForge must author them during import/save/reload/cook without introducing Gloamstead semantic defaults.

## Verification

- UE 5.8 editor build:
  - `Build.bat GloamsteadEditor Win64 Development ...`
  - `BUILD_EXIT=0`; 6/6 actions completed; `Result: Succeeded`.
- `Gloamstead.GeneratedAssets`:
  - 5/5 successful, 0 failed.
  - One success-with-warning from the already-vendored WorldForge plugin registering `WorldForge.SetState` twice; no test error.
- `Gloamstead.MeshForge`:
  - 5/5 successful, 0 failed.
  - Existing live-world warnings only; no test error.
- Full `Gloamstead` automation filter:
  - 85/85 successful, 0 failed, 0 not run.
- Repository gate:
  - shell guard green twice with deterministic output and unchanged durable state;
  - UE build green;
  - 85/85 tests green;
  - final status remains failed at the pre-existing GloamsteadForge contracts validator (`0/13`) because `GloamsteadForgeEvidence::ReadGitCommit/ReadGitBranch` treats `<worktree>/.git` as a directory. In a linked worktree `.git` is an indirection file, so generated reports contain empty `git_commit` and `git_branch`. This slice does not modify that evidence subsystem.

## Files

- `Source/Gloamstead/Data/GloamsteadGeneratedAssetCatalog.{h,cpp}`
- `Source/Gloamstead/Data/GloamsteadMeshForgeTypes.cpp`
- `Source/Gloamstead/Systems/GloamsteadMeshForgeProvider.{h,cpp}`
- `Source/Gloamstead/Systems/GloamsteadMeshForgeAdapterSubsystem.cpp`
- `Source/Gloamstead/PCG/GloamsteadPCGSubsystem.{h,cpp}` (test seeding only)
- `Source/Gloamstead/Tests/GeneratedAssetCatalogTests.cpp`
- `Source/Gloamstead/Tests/MeshForgeAdapterTests.cpp`
- `Source/Gloamstead/Gloamstead.Build.cs`
- `.superpowers/sdd/task-3-fix-report.md`
