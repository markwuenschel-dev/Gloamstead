# Gloamstead — Agent Collaboration Goal (Multi-Runtime UE5 Vertical Slice Factory)

The purpose of this collaboration system is to enable a **runtime-agnostic, agentic Unreal Engine 5 vertical-slice factory**.

Future work should be able to assemble playable vertical slices from:

- Curated Marketplace or Fab asset packs (read-only vendor inputs)
- Project-owned gameplay systems (C++ and Blueprint)
- PCG biome and world-building systems
- Text-based slice specifications (SliceSpec)
- Asset-pack manifests and compatibility adapters
- Project-owned Unreal Editor automation and commandlets
- Automated validation, map-load, automation tests, and package-smoke checks
- Human playtest feedback loops
- Cooking, packaging, and playtest workflows

## Core Principle (non-negotiable)
Agents must not manually place assets or directly author/patch Unreal binary content (.uasset, .umap, etc.).

Agents create and modify deterministic source: text specifications, manifests, adapters, C++/Blueprint source, config, editor automation, validation logic, build/packaging scripts, and documentation.

Unreal binary assets are treated strictly as controlled generated outputs, produced only by approved automation under explicit ownership and verification.

## Current Baseline (for context)
- Mixed C++/Blueprint UE 5.7 project with existing vertical slice core loop (restoration-warning-night consequence-reflection) and strong PCG investment.
- Git LFS for all binary content.
- No project-level automation or packaging scripts discovered yet.

## Collaboration Mandate
The scaffold must be:
- Runtime-agnostic (role != runtime)
- Lease-based for exclusive Orchestrator authority (any compatible runtime may hold the lease)
- Strict about scope, ownership (file + generated output), vendor immutability, and verification
- Cold-restartable by any enabled compatible runtime
- Auditable with full reconciliation on every Orchestrator activation

All future factory tasks must respect these boundaries so the system can safely scale without corrupting the project or violating third-party terms.
