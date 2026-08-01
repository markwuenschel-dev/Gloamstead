# Task 4 implementation report

## Scope

Implemented the Gloamstead-owned Sanctuary biome intent, immutable inventory, acceptance profile, WorldForge 0.2.0 vendor lock/sync boundary, runtime-identity serialization parity helper, typed workstation probes, request builder, operator entrypoint, hostile fixtures, and accepted-source LFS policy. No runtime provider C++, maps, Content binaries, generated art, or live vendored plugin bytes were changed.

## Contract result

- Immutable kit: `sanctuary-biome-kit-1.0.0`.
- Exact deliverables: 60 (16 hero/ritual state assets, 12 modular ruins, 3 governed surfaces, 6 foliage/natural forms, 8 decals, 4 VFX families, 4 placement rules, 4 state material instances, and 3 integration assets).
- Every deliverable carries Gloamstead semantics, purpose, class, state, exact source/object destination, dependencies, acceptance tags, owner/license, and exclusive lane.
- Intent, acceptance, and inventory are canonical-hash bound; closed Draft 2020-12 schemas reject unknown fields.
- Automatic acceptance mutation is forbidden and promotion is fixed to `automatic_on_all_green`.

## Vendor result

- Lock binds WorldForge source `61009579821f280ad3a000da9262c9f6ff5d5398`, release-manifest commit `00d6c7ea`, release-manifest SHA-256 `2daf2ad...`, package SHA-256 `da19fbd...`, descriptor/source/schema hashes, capabilities, UE 5.8, and build identity.
- Sync verifies the lock-hash-bound release manifest and ZIP independently, rejects unsafe/colliding/unexpected/forbidden entries, validates every payload hash and exact file set, stages under the exact Plugins parent, swaps only `Plugins/WorldForge`, verifies after swap, and restores the prior directory if verification fails. No cryptographic signature is claimed.
- Installed verification hashes the exact packaged source tree and rejects hand edits. Runtime identity remains an independently observed full installed/build tree per `gloamstead.worldforge.runtime-identity@1`; the package-tree lock is not substituted for runtime observation.
- The live 0.1 plugin was intentionally not replaced while the shared worktree contained concurrent runtime-lane changes. All sync tests used disposable Git repositories.

## Verification

- `pwsh -NoProfile -File scripts/Test-SanctuaryBiomeKit.ps1`: PASS, 12 tests.
- Real PowerShell JSON Schema validation: PASS for intent, acceptance, inventory, and vendor lock.
- Frozen WorldForge `BiomeKitRequest.from_dict` interoperability: PASS.
- Canonical hash mutation fuzz: 200/200 rejected.
- Verified package install/hand-edit rejection in disposable repositories: PASS.
- ZIP traversal/unexpected-file negative: PASS.
- Workstation dry probe: expected typed rejection: no qualified ComfyUI/model/workflow/custom-node/license stack, no legitimate Substance Automation Toolkit, and Houdini 21.0.729 versus Houdini Engine 21.0.753.
- A `qualified: true` label cannot bypass review or probes: canonical qualified-pin bytes must match the hash in the committed Gloamstead toolchain approval, after which the operator independently re-hashes every workflow/model/custom-node/graph/HDA/executable/plugin/license receipt, confirms tool versions and the ComfyUI Git commit/API/hardware report, and requires three production-use license receipts.
- Parameterless operator: expected typed `FAIL-UNVERIFIED-RUNTIME` until the three explicit execution/toolchain settings are configured.

## Remaining external gate

The exact WorldForge 0.2.0 snapshot currently exposes a native compile-stamp identity surface, not the verifier-authored `wfplugin-*` release identity required for generated-mode runtime trust. The Gloamstead runtime provider therefore must continue to fail closed until the integration lane supplies and verifies that generic release identity; this task does not manufacture it from declared lock values.
