# Task 3 runtime review round 7 report

## Finding closed

`Test_SetObservedRuntimeIdentity` installs a synthetic `FFixedRuntimeIdentitySource` for automation,
but the generated provider previously retained that source across `Configure` and `Deactivate`.
Reusing the same provider object could therefore validate a production catalog using test-only identity
bytes instead of failing closed with `GAC037`.

## Implementation

- `Configure` now cancels the prior load generation and installs a fresh production fail-closed runtime
  identity source before accepting new settings.
- `Deactivate` now cancels the prior load generation and independently installs a fresh production
  fail-closed runtime identity source before clearing the loaded catalog and state.
- The fixed identity implementation and its setter remain guarded by `WITH_DEV_AUTOMATION_TESTS`.
- Hostile automation covers injected-success to `Configure`, revalidation after `Configure`, injected
  identity to `Deactivate`, a stale completion from before `Deactivate`, and
  `Deactivate`-then-`Configure`. Every current load after a lifecycle boundary fails with `GAC037`.

## Red/green evidence

- Red, after compiling only the new hostile assertions against the prior implementation:
  `Gloamstead.GeneratedAssets.ProviderNeverFallsBack` failed four assertions showing that the injected
  identity survived `Configure`, revalidation, and `Deactivate`/reconfiguration.
- Green, after the lifecycle reset implementation:
  `Gloamstead.GeneratedAssets.ProviderNeverFallsBack` succeeded.

## Final verification

- UE 5.8 editor build: `GloamsteadEditor Win64 Development` succeeded.
- `Automation RunTests Gloamstead.GeneratedAssets`: 7/7 succeeded.
- `Automation RunTests Gloamstead.MeshForge`: 6/6 succeeded.
- `Automation RunTests Gloamstead`: 91/91 succeeded.
- Automation used `-DDC-ForceMemoryCache` because the restricted execution environment cannot write the
  machine-wide installed Derived Data Cache; test semantics are unchanged.
