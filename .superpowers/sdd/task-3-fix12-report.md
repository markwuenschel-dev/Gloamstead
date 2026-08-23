# Task 3 fix round 12: generation-aware catalog completion

## Scope

Closed the same-provider reentrancy gap in the generated-catalog provider and MeshForge
adapter. Task 4 Python, specifications, intent, vendor, operator, and asset-source work was
not changed.

## Implementation

- Added a typed catalog-load terminal (`Accepted`, `Rejected`, `Cancelled`, or `Stale`)
  carrying the provider acceptance epoch, load generation, provider state, catalog UObject
  generation, and accepted catalog-contract digest. The existing `FSimpleDelegate` API is a
  compatibility wrapper and remains silent for stale/cancelled generations.
- Made one provider-owned typed completion the exclusive owner of an async request. Platform
  cancellation completes it once; a late platform callback cannot complete it again. The
  acceptance frame moves completion ownership before validation so nested Configure,
  Deactivate, or catalog-B acceptance cannot steal or duplicate catalog-A completion.
- Added cancellation-boundary ownership checks to Configure, Deactivate, preload, and
  mutation invalidation. A nested lifecycle call owns the provider after callback reentry;
  the stale outer frame does not overwrite it.
- Added an adapter request serial in addition to provider UObject generation. The adapter
  rebuilds only for an `Accepted` result whose complete epoch/load/catalog/digest tuple is
  still the exact current `Ready` provider generation. Rejected results report failure;
  cancelled/stale results only retire the pending world.
- Retained strong guards across production `TryLoad`, dependency resolution,
  `ExpectedClass.LoadSynchronous`, and actor-spawn boundaries.

## Hostile coverage

- The adapter drives the real preload registration and `AcceptCatalogLoad` path, then
  reenters the same provider during catalog-A observation with Configure, Deactivate, or
  nested catalog-B acceptance. Every case proves catalog A returns one non-accepted terminal
  and never reenters `BuildFor`; nested B remains `Ready`.
- Typed accepted and cancelled requests are exact-once. A late completion after cancellation
  is inert, and only an exact accepted tuple is current.
- A nine-case production-boundary matrix covers all three lifecycle actions at real
  `FSoftObjectPath::TryLoad`, dependency `TryLoad`, and
  `ExpectedClass.LoadSynchronous` boundaries. Each hostile callback forces garbage
  collection and proves the boundary object remains alive, the stale operation reports
  `GAC039` without spawning, and catalog B remains usable and unpoisoned.

## Verification state

- Red proof: UE 5.8 build failed on the new generation-aware adapter test APIs before their
  implementation existed.
- Last fully verified source point (before the later real-load/GC matrix was added):
  `GloamsteadEditor Win64 Development` built successfully, and
  `Gloamstead.MeshForge.StaleAcceptedCatalogNeverReentersBuild` passed 1/1.
- Post-GC-matrix bytes: `git diff --check` is clean and the implementation received a static
  ownership/diff audit. A fresh UE build and the focused, GeneratedAssets, and full
  Gloamstead automation runs remain pending because the approval service exhausted its usage
  allowance and UnrealBuildTool could not rotate its AppData trace inside the filesystem
  sandbox. These bytes are not claimed UE-green.
