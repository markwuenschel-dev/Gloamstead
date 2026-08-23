# Task 4: Gloamstead sanctuary intent, acceptance, vendor lock, and operator command

Work only in `D:\Unreal Projects\.worktrees\gloamstead-production-asset-forge` on `codex/gloamstead-production-asset-forge`.

Start only after WorldForge Tasks 1 and 2 are committed. Bind to their actual schemas and release manifest; do not invent a parallel wire format.

## Owned paths

- new `specs/worldforge_asset_forge/**`
- new `scripts/Forge-SanctuaryBiome.ps1`
- new `scripts/Test-SanctuaryBiomeKit.ps1`
- new `scripts/Sync-WorldForgePlugin.ps1`
- new narrowly scoped Python/PowerShell helpers under `scripts/worldforge_asset_forge/**`
- `.gitattributes`
- new `SourceArt/WorldForge/Accepted/Sanctuary/.gitkeep` and README only (no generated art)
- `Plugins/WorldForge/**` only through the exact verified package-sync command, never by hand
- `Config/DefaultEngine.ini` / `.uproject` only if the verified plugin snapshot requires explicit generic module enablement

Do not edit Task 3 C++ files, maps, Content binaries, PCG graphs, gameplay/save systems, or WorldForge repo files.

## Committed Gloamstead authority

Create immutable version `sanctuary-biome-kit-1.0.0` under a versioned spec root with strict schemas, canonical hashes, positive and hostile fixtures, and checked-in documents for:

- art intent: Withered Gothic Stylization, caller-owned subjects, semantic roles, art purposes, states, exact source destinations and exact `/Game/Gloamstead/Generated/Biomes/Sanctuary/1_0_0/...` object paths
- acceptance profile: locked against automatic weakening, palette/value/silhouette/readability constraints, PBR/channel rules, texture/mesh/LOD/collision/Nanite limits, PCG density, draw/memory budgets, golden scene and packaged-runtime gates
- exact dependency-closed inventory:
  - one Veil Heart hero family with before/in-progress/restored/corrupted states
  - lantern, mirror-pillar, bell-shrine families with the same four states
  - twelve distinct modular ruin pieces
  - ashen soil, cracked sanctuary stone, withered loam/moss surfaces
  - six foliage/root/natural-form families
  - eight decal motifs spanning cracks, soot, ritual marks, moss, corruption
  - Heart ambience, restoration motes, corruption wisps, ground fog VFX
  - courtyard, perimeter ruins, ritual approaches, corruption pockets placement/PCG rules
  - exact material instances, placement Data Assets, biome manifest, generated catalog, runtime bindings
- promotion policy exactly `automatic_on_all_green`
- output allowlists and LFS policy

Each inventory member must name semantic role, art purpose, asset class, restoration state when applicable, source path, final object path, dependency IDs, acceptance tags, ownership/license expectations, and asset-family lane. No unspecified wildcards, path repair, inferred destinations, `TODO`, placeholder, or WorldForge-authored Gloamstead meaning.

## Vendor lock/sync

Check in the lock at exact path `specs/worldforge_asset_forge/worldforge-plugin.lock.json`, bound to the verified WorldForge `0.2.0` release manifest: upstream commit, descriptor/source/package hashes, UE 5.8, schema hashes, capabilities, package build identity. The sync command:

- accepts explicit package + manifest inputs
- independently verifies package and manifest before extraction
- rejects dirty Gloamstead worktree, stale/wrong version/engine, hash/schema/capability drift, zip traversal/case collisions, forbidden roots, and unexpected files
- stages in a temp directory, compares exact file set/hashes, then replaces only `Plugins/WorldForge` via recoverable exact-path operations
- verifies the installed copy against the lock and refuses hand edits
- never copies Binaries/Intermediate/Saved/reports/host behavior

The lock and runtime settings must use the exact canonical runtime-identity contract `gloamstead.worldforge.runtime-identity@1`: canonical UTF-8/LF serialization over the actual `FEngineVersion` version/build/changelist/compatible changelist, actual installed `WorldForge.uplugin` bytes/hash/version/engine constraint, actual current Gloamstead commit when Git metadata exists, exact lock-file bytes/hash, and the plugin-reported verifier-authored release/package build identity. Declared lock package/build fields are not themselves runtime observations. Generated mode fails closed if the installed 0.2 plugin does not expose the matching generic release identity; explicit primitive development fallback remains independent.

Use test temp repos; never mutate the live plugin in a negative test.

## Operator command

`pwsh -NoProfile -File scripts/Forge-SanctuaryBiome.ps1` is the ordinary entrypoint. It must:

- require a clean Gloamstead worktree and exact origin/main-derived base
- verify vendor lock, intent, acceptance, inventory closure, LFS policy, expected active pointer and output allowlists
- probe required UE 5.8, ComfyUI, Substance, Houdini/HDA, model/workflow/custom-node/license pins and fail closed with typed `FAIL-*` codes if absent/drifted
- build the exact canonical WorldForge `BiomeKitRequest` using the frozen schemas and compute its request hash/idempotency key
- require an explicit configured WorldForge execution checkout/command; do not infer or silently use a dirty sibling checkout
- invoke only `asset_set.reconcile@1`, persist the hash-bound request/result/evidence under the versioned spec root, and return nonzero unless result is `NoChange` or `Promoted`
- never directly edit `.uasset`, maps, pointer, or source outputs

The workstation has authoritative UE 5.8 at `D:\UE_5.8`, but ComfyUI and a legitimate licensed Substance Automation Toolkit are absent, and Houdini `21.0.729` does not match the discovered Houdini Engine plugin target `21.0.753`. The committed workstation probe must record the UE identity and deterministically return typed prerequisite/drift failures (`FAIL-UNVERIFIED-RUNTIME`, `FAIL-AI-PIN-DRIFT`, etc.) for the unavailable or mismatched capabilities, never a fake green.

Extend LFS rules for accepted `.sbs`, `.sbsar`, `.hda/.hdalc/.hdanc`, images, meshes, VFX source, Unreal binaries, and large model-independent generated sources while keeping JSON/manifests/receipts text-diffable.

TDD; run strict positive/negative/fuzz/operator dry-probe tests. Use apply_patch. Stage only owned paths, commit with `Co-Authored-By: Codex <codex@openai.com>`, and write `.superpowers/sdd/task-4-report.md`.
