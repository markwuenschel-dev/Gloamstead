# Task 3 fix round 9: immutable accepted catalog contract

## Scope

Closed the post-`Ready` mutable-catalog bypass without touching Task 4 intent, vendor,
operator, or asset-source work.

## Implementation

- Added `gloamstead.generated-asset-catalog-contract@1`, a canonical UTF-8 contract
  representation and SHA-256 covering every current catalog, terminal-policy,
  authority, external-record, entry, object-path, class-path, provenance, dependency,
  ownership, license, receipt, bundle, version, and build-identity field.
- Canonical values are byte-length-prefixed. Set-like arrays and nested records are
  sorted by canonical bytes. Runtime UObject addresses and load state are excluded.
- The generated provider records the digest only after structural, active-binding,
  runtime-identity, and script-authority validation succeeds.
- Preload, async completion, runtime revalidation, `CanSpawn`, dependency closure,
  selection, and spawn all re-run structural validation and compare the exact digest.
- A mismatch emits `GAC039`, cancels/increments outstanding async work, clears the
  catalog and verified script authorities, and latches the provider failed. Neither
  revalidation, direct test reload, stale completion, nor `Deactivate` can clear the
  latch. Only explicit `Configure` followed by a fresh async load can recover.

## Hostile coverage

The focused automation mutates each accepted contract family after `Ready`: terminal
root, exact engine terminal, script authority, entry object path/hash/dependencies,
external record, bundle, receipt, target build identity, and license during a pending
reload. It also proves order-independent hashing, stale callback rejection, latch
persistence, and explicit Configure-plus-fresh-load recovery.

## Verification

- Red proof: UE 5.8 build failed at the new regression seam because
  `GACCatalogContractSha256` did not exist.
- `Build.bat GloamsteadEditor Win64 Development ... -MaxParallelActions=6`: success.
- `Automation RunTests Gloamstead.GeneratedAssets.ProviderRejectsPostAcceptanceCatalogMutation`:
  1/1 success.
- `Automation RunTests Gloamstead.GeneratedAssets`: 8/8 success.
- `Automation RunTests Gloamstead`: 92/92 success.
- `git diff --check`: clean for the owned runtime diff.

