# Task 3 runtime review round 6 report

## Scope

- Replaced the circular catalog/settings-only target-build check with a versioned canonical runtime-identity contract and a narrow observation protocol.
- Bound successful validation to an independently supplied identity containing engine version/build/changelists, Gloamstead commit, installed WorldForge descriptor/tree evidence, vendor-lock hash, and the lock-declared package/build identities.
- Kept the production observer fail-closed (`GAC037`) until the verified 0.2.0 plugin and committed lock exist; the explicit engine-primitive development fallback remains independent.
- Added adapter configuration fingerprinting for catalog path, active bundle pointer, receipt, and runtime identity. Any same-mode drift replaces the provider, cancels preloads, and invalidates stale callbacks.
- Revalidated runtime identity on every generated-provider build/rebuild and cancelled pending preload work during adapter shutdown.

## Shared contract

- Lock path: `specs/worldforge_asset_forge/worldforge-plugin.lock.json`
- Contract: `gloamstead.worldforge.runtime-identity@1`
- Serialization: UTF-8, LF-only, exact ordered `key=value` lines with a final LF, as asserted by `RuntimeIdentityContractIsCanonicalAndIndependent`.
- Hash: SHA-256. The portable implementation is checked against the independent Python vector `354fd50d48b60f0af25644a5acc016cb81aa933be8a6245f1404c72d1105a355`.
- WorldForge release build identity format: `wfplugin-<64 lowercase hex>`.

## Verification

- UE 5.8 editor build: `GloamsteadEditor Win64 Development` — succeeded.
- `Automation RunTests Gloamstead.GeneratedAssets` — 7/7 succeeded.
- `Automation RunTests Gloamstead.MeshForge` — 6/6 succeeded.
- `Automation RunTests Gloamstead` — 91/91 succeeded.
- `git diff --check` — clean.

The first focused run caught that `FPlatformMisc::GetSHA256Signature` asserts on this UE Windows build (`No SHA256 Platform implementation`). It was replaced before the green runs; no platform helper or expected-value-derived hash remains in the runtime contract.
