# Task 3 runtime review round 8 report

## Finding closed

The catalog previously accepted `/Script` as a broad terminal root and accepted arbitrary exact
`/Script/*` package strings. The runtime identity covered UE, Gloamstead, and WorldForge, but did not
carry a complete enabled-plugin/module inventory. A dependency on an enabled third-party module such
as `/Script/NeoStackAI` could therefore be classified as terminal without binding that plugin's bytes.

## Implementation

- Removed `/Script` from terminal roots and rejected script packages in the legacy exact platform list.
- Added exact terminal authority records keyed by canonical `/Script/<Module>` package name and owner
  class: engine, Gloamstead project, WorldForge plugin, or externally pinned plugin.
- Extended `gloamstead.worldforge.runtime-identity@1` with a complete enabled-plugin inventory digest.
  Plugin records are sorted by name; their module packages are sorted by name; every record binds
  version, descriptor SHA-256, full installed-tree SHA-256 (binaries, content, config, and source),
  build identity, and zero or more script packages. Content-only plugins are therefore still bound.
- Derived engine authority identities from the exact UE build axes, Gloamstead identities from the
  exact commit and engine build, and WorldForge/external-plugin identities from their canonical plugin
  records. The canonical runtime identity includes the independently observed inventory digest and
  sorted derived authority records.
- Required catalog and observed terminal authority sets to be exactly equal before the provider becomes
  ready. The provider retains only the verified observed package set and clears it at every failure,
  reconfiguration, deactivation, and revalidation boundary.
- Kept production fail-closed behavior: the currently unavailable production observer still reports
  `GAC037`. Task 4 must implement the filesystem/build observer against these documented serializer
  fields; catalog declarations alone cannot populate the trusted set.
- Added `GAC038` for terminal authority/inventory mismatch. Hostile coverage rejects the broad root,
  legacy script allowlists, absent NeoStackAI inventory records, owner drift, module edits without a
  matching inventory digest, content-only plugin tree drift, and post-validation catalog mutation.
  Positive coverage proves exact engine, Gloamstead, WorldForge, and fully pinned external-plugin sets.

## Red/green evidence

- Red compile: tests referenced the new inventory/authority contract before its implementation.
- First implementation compile failed in the pointer-array sort predicate; corrected to Unreal's
  dereferenced `TArray::Sort` predicate contract.
- First focused run: 6/7 tests passed. The closure fixture failed with `GAC034,GAC020`, proving that
  strict `/Script/<Module>` pseudo-packages also needed an explicit direct-edge grammar instead of
  `FPackageName::IsValidLongPackageName`.
- After that edge fix, the closure test and the complete focused suite passed.

## Final verification

- UE 5.8 `GloamsteadEditor Win64 Development` build: succeeded.
- `Automation RunTests Gloamstead.GeneratedAssets`: 7/7 succeeded; one pre-existing
  duplicate-console-registration warning, zero failures.
- `Automation RunTests Gloamstead`: 91/91 succeeded; zero failures and zero not-run tests.
- Automation used `-DDC-ForceMemoryCache` because the restricted environment cannot use the normal
  machine-wide installed Derived Data Cache.
