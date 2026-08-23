# Task 3 fix round 10: reentrant catalog acceptance hardening

## Scope

Closed the five independent Round 9 review findings in the Gloamstead generated-catalog
runtime only. Task 4 intent, vendor, operator, and asset-source files were not changed.

## Implementation

- Quarantine every mutated catalog UObject generation across `Configure`. A cached rejected
  object cannot be accepted again; a distinct object generation loaded from the declared path
  is required. The provider does not attempt a global package unload or garbage collection.
- Sandwich initial structural validation, runtime observation, active-binding validation, and
  terminal-authority derivation between canonical catalog hashes before entering `Ready`.
- Recheck the accepted contract after runtime observation, dependency qualification, every
  `TryLoad`/dependency load, `LoadSynchronous`, provenance read, and `SpawnActor` boundary.
  Entry data crossing reentrant boundaries is copied by value. A spawned actor is destroyed
  before return if the contract changed during spawn.
- Execute the completion delegate exactly once for every current-generation catalog-load
  terminal path via scope-exit ownership. Superseded generations remain intentionally silent.
- Sort every set-like catalog canonical record by its encoded UTF-8 bytes, not UTF-16 `TCHAR`
  ordering. The hostile vector is pinned to an independently produced Python serializer digest.

## Hostile coverage

- Same cached mutated UObject rejected after `Configure`; a distinct generation succeeds.
- Structurally valid mutation during initial observation and runtime revalidation rejected.
- Mutation during asset resolution rejected before spawn.
- Mutation during actor spawn rejected and the exact spawned actor destroyed.
- Success, null-catalog, path-mismatch, mutation-rejection completions fire exactly once;
  stale-generation completion remains ignored.
- U+E000 plus ASCII versus U+10000 demonstrates UTF-8 order differs from UTF-16 order and
  matches independent digest `1124eca43183e5c7b96a947c4edc1ea70685a9d9b0960bcd99070ce3840f6702`.

## Verification

- Red proof: the first UE 5.8 build failed on the new observer/load/spawn test seams before
  their implementation existed.
- `Build.bat GloamsteadEditor Win64 Development ... -MaxParallelActions=6`: success.
- Focused `ProviderRejectsPostAcceptanceCatalogMutation`: 1/1 success.
- `Automation RunTests Gloamstead.GeneratedAssets`: 10/10 success.
- `Automation RunTests Gloamstead`: 94/94 success.
- `git diff --check`: clean for the owned diff.
