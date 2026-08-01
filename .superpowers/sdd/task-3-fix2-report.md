# Task 3 dependency-closure fix report

## Result

The generated catalog provider now verifies the cooked Asset Registry dependency graph before any generated actor spawn.

- UE 5.8 `IAssetRegistry::GetDependencies` supplies each package's on-disk `Package` dependencies with an empty dependency query, deliberately covering both hard and soft edges.
- Catalog `Dependencies` are generated-only direct edges. For every catalog package, the declared package set must exactly equal the observed direct generated package set.
- The provider traverses the complete generated graph transitively, rejects cycles, query failures, version-root escapes, missing or ambiguous package-to-entry mappings, omitted observed edges, and declared-but-unused edges.
- Every catalog entry in the closed graph is checked against independent `WorldForge.ObjectSha256`, `WorldForge.ReceiptSha256`, and `WorldForge.BundleId` Asset Registry tags before spawn. Missing and tampered transitive evidence remains fail-closed as `GAC023`/`GAC024`.
- `AllowedExternalDependencyRoots` is an explicit Gloamstead-owned generic policy for engine, script, plugin, or shared caller packages. Boundary matching is exact; broad roots that could authorize generated content, malformed roots, duplicates, and unlisted external dependencies are rejected.
- A `WITH_DEV_AUTOMATION_TESTS` graph seam exercises the same production closure validator with deterministic hard/soft graphs.

New failure codes are `GAC027` through `GAC034` for query failure, generated-root escape, ambiguous mapping, observed omission, unused declaration, cycle, external-policy violation, and invalid policy.

## Connected impact

- The catalog UPrimaryDataAsset owns the explicit external dependency policy alongside the immutable version root and direct generated dependency declarations.
- The existing spawn path invokes closure verification after exact role/state and target-location resolution but before object loading or `SpawnActor`.
- Existing direct dependency class/load checks remain in place after graph and provenance closure, so the stronger graph proof does not weaken runtime type validation.
- No WorldForge, Task 4, binary content, settings defaults, save truth, PCG semantics, or primitive fallback files changed.

## Verification

- UE 5.8 editor build: `Build.bat GloamsteadEditor Win64 Development ... -NoUBTMakefiles` succeeded.
- Focused `Gloamstead.GeneratedAssets`: 6/6 successful.
- Full `Gloamstead`: 86/86 successful.
- `git diff --check`: clean.

The focused suite covers a valid transitive graph with explicit `/Engine` and `/Script` policy, omitted actual direct edges, stale and missing transitive provenance, declared-but-unused edges, generated cycles, version-root escape, unapproved external packages, ambiguous same-package entries, and invalid broad external policy.
