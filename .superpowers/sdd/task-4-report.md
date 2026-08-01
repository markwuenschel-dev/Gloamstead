# Task 4 implementation report

## Scope

Implemented the Gloamstead-owned Sanctuary biome intent, dependency-closed immutable inventory, acceptance profile, WorldForge 0.2.0 vendor lock/sync boundary, runtime-identity serialization parity helper, typed workstation probes, request builder, strict operator verification, hostile fixtures, and accepted-source/commit LFS policy. No Round 11 runtime-provider C++, maps, Content binaries, generated art, or live vendored plugin bytes were changed.

The initial Task 4 contract landed in shared-index commit `b845f28` together with runtime-lane files. That is a combined commit rather than an isolated Task 4 commit; this report does not represent it as independently scoped and history was not rewritten.

## Contract result

- Immutable kit: `sanctuary-biome-kit-1.0.0`.
- Exact deliverables: 107. The original semantic families remain, with explicit base-color/normal/ORM Texture2D products for every surface, complete foliage-card textures/materials, decal source textures, EXR VFX plates plus deterministic flipbook assemblies, and four required parent materials.
- Every deliverable carries Gloamstead semantics, purpose, class, state, exact source/object destination, dependencies, acceptance tags, owner/license, and exclusive lane. Every ruin additionally carries a unique caller-authored functional role, centimetre dimensions, snap interfaces, silhouette requirement, and explicit placement-rule consumers.
- Intent, acceptance, and inventory are canonical-hash bound; closed Draft 2020-12 schemas reject unknown fields.
- Automatic acceptance mutation is forbidden and promotion is fixed to `automatic_on_all_green`.

## Vendor result

- Lock binds WorldForge source `61009579821f280ad3a000da9262c9f6ff5d5398`, release-manifest commit `00d6c7ea`, release-manifest SHA-256 `2daf2ad...`, package SHA-256 `da19fbd...`, descriptor/source/schema hashes, capabilities, UE 5.8, and build identity.
- Sync verifies the lock-hash-bound release manifest and ZIP independently, rejects unsafe/colliding/unexpected/forbidden entries case-insensitively, validates every payload hash and exact file set, and installs both the plugin and the host convention `Config/WorldForge/VerifiedReleaseManifest.json`. Every replace/delete transition is append-journaled below the worktree Git directory. Failure restores the exact prior state; failed restoration returns typed `FAIL-ROLLBACK` / `RecoveryRequired` with the preserved backup and journal paths. No cryptographic signature is claimed.
- Installed verification hashes the exact packaged source tree and rejects hand edits. Runtime identity remains an independently observed full installed/build tree per `gloamstead.worldforge.runtime-identity@1`; the package-tree lock is not substituted for runtime observation.
- The live 0.1 plugin was intentionally not replaced while the shared worktree contained concurrent runtime-lane changes. All sync tests used disposable Git repositories.

## Verification

- `pwsh -NoProfile -File scripts/Test-SanctuaryBiomeKit.ps1`: PASS, 18 tests.
- Real PowerShell JSON Schema validation: PASS for intent, acceptance, inventory, and vendor lock.
- Frozen WorldForge `BiomeKitRequest.from_dict` interoperability: PASS.
- Canonical hash mutation fuzz: 200/200 rejected.
- Verified package install/hand-edit rejection in disposable repositories: PASS.
- ZIP traversal/unexpected-file negative: PASS.
- Mixed/lowercase forbidden ZIP roots and injected swap/delete failures: PASS.
- A forged `{state: Promoted}` object is rejected. Successful reconcile output must pass WorldForge's exact frozen `BiomeKitResult` parser, Gloamstead request/kit/target/toolchain/artifact/family/pointer bindings, a freshly observed target snapshot, canonical Git LFS pointer/object/receipt closure, a UE 5.8 reload of the catalog with exact class/bundle/receipt/version/soft-reference closure, WorldForge's exact `ResultRef` and `TargetSnapshot` parsers, and a separate `AssetSetForge.verify` invocation before evidence is persisted.
- Workstation dry probe: expected typed rejection: no qualified ComfyUI/model/workflow/custom-node/license stack, no legitimate Substance Automation Toolkit, and Houdini 21.0.729 versus Houdini Engine 21.0.753.
- A `qualified: true` label cannot bypass review or probes: `build_request` and the operator share one validator. Canonical qualified-pin bytes must match the hash in the committed Gloamstead toolchain approval, after which it independently re-hashes every workflow/model/custom-node/graph/HDA/executable/plugin/license receipt, confirms tool versions and the clean ComfyUI Git commit, binds live `/system_stats` and `/object_info` response hashes plus hardware evidence, and requires three production-use license receipts.
- Parameterless operator: expected typed `FAIL-UNVERIFIED-RUNTIME` until the three explicit execution/toolchain settings are configured.

## Remaining external gate

The exact WorldForge 0.2.0 snapshot currently exposes a native compile-stamp identity surface, not the verifier-authored `wfplugin-*` release identity required for generated-mode runtime trust. The Gloamstead runtime provider therefore must continue to fail closed until the integration lane supplies and verifies that generic release identity; this task does not manufacture it from declared lock values.
