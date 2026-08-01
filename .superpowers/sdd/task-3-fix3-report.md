# Task 3 external dependency closure fix report

## Result

The generated-asset catalog now models and verifies a complete direct package dependency graph rather than treating external roots as opaque.

- Every generated catalog entry declares `DirectPackageDependencies`, the complete exact package-level direct edge set returned by the cooked Asset Registry. The existing generated-object `Dependencies` mappings remain mandatory for every generated direct edge so class, load, and object-provenance checks are preserved.
- Every non-terminal external dependency is a `FGloamsteadGeneratedExternalPackageRecord` with an exact package name, a package-local provenance object, package hash, receipt/bundle binding, and its own complete direct edges. External records are queried and traversed even when unattached from the selected runtime object, preventing hidden unverified subgraphs.
- External package identity is read independently from the installer-authored `WorldForge.PackageSha256`, `WorldForge.ReceiptSha256`, and `WorldForge.BundleId` Asset Registry tags. Missing evidence fails as `GAC023`; mismatched external evidence fails as `GAC035`.
- Terminal policy is limited to exact `/Engine` or `/Script` packages and optional exact safe roots `/Engine` and `/Script`. Arbitrary `/Game/Shared` content can never be terminal. Terminal identity is bound through `TargetBuildIdentitySha256`, a canonical target UE build, Gloamstead base commit, and vendored plugin lock hash that must match `ExpectedTargetBuildIdentitySha256` in project settings (`GAC036`).
- The runtime compares the full declared and observed direct sets for generated and external nodes, then recursively traverses every non-terminal node. It rejects observed edge additions (`GAC030`), declared-but-unused edges (`GAC031`), cycles (`GAC032`), undeclared external transitives (`GAC033`), query failures (`GAC027`), prior generated-version reentry (`GAC028`), and unmapped current-version reentry (`GAC029`).
- Package and object identity comparisons are case-folded consistently while stored package names remain canonical Unreal long package names.

## Task 4 authoring contract

Future Sanctuary intent/catalog authoring must populate all of the following from the frozen WorldForge receipt and install receipt; no field may be inferred or repaired at runtime:

1. `DirectPackageDependencies` for every generated entry, including generated, shared/plugin, and platform packages.
2. One recursively complete `ExternalPackageRecords` record for every non-terminal package outside the immutable generated version root. Each record requires `WorldForge.PackageSha256`, receipt, and bundle evidence on its declared provenance object.
3. Only exact platform packages or an explicitly reviewed `/Engine` or `/Script` terminal root. `/Game`, `/Game/Shared`, plugin roots, and generated roots are never valid terminal policies.
4. `TargetBuildIdentitySha256` and the matching project-settings expectation, derived from the exact Gloamstead target/base commit, UE 5.8 build identity, and vendored WorldForge plugin lock.
5. A generated-object `Dependencies` mapping for every generated direct package edge, preserving per-object class, load, ownership/license, and provenance verification.

The WorldForge installer will need to stamp `WorldForge.PackageSha256` on each external record's provenance object when the generic import lane is implemented. Until that evidence exists, Gloamstead intentionally fails closed.

## Verification

- UE 5.8 editor build: `Build.bat GloamsteadEditor Win64 Development ... -NoUBTMakefiles` succeeded.
- Focused `Gloamstead.GeneratedAssets`: 6/6 successful.
- Full `Gloamstead`: 86/86 successful with worktree-safe Git provenance configuration.
- The closure regression test covers full generated/shared/platform success, external package-hash tampering, external direct-edge drift, undeclared transitives, prior/current generated-version reentry, terminal substitution drift, external query failure, and external cycles.
- `git diff --check`: clean.

No WorldForge, Task 4 intent/vendor files, binary assets, maps, PCG graphs, or active catalog pointers were changed.
