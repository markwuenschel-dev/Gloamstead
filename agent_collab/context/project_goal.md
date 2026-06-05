# Gloamstead Project Goal (for Collaboration System)

**Vertical slice**: Prove the core loop "Warning → Understanding → Restoration → Consequence → Reflection" in a focused third-person dark fantasy experience centered on the Veil Heart.

## Current Baseline (Phase 1.5 Complete)
- Ritual data contracts (ERitualType, UritualDefinition Data Assets, FRestorationEventPayload)
- Optimized hybrid PCG subsystem with spatial hash grid for ritual point queries/mutations
- Player placement component (C++ base + BP) with snapping, preview, validation, payload construction
- Multiple variant maps exercising the placement + PCG restoration flow

## Near-term Objectives (Phase 2 Priority)
- Night Consequence System (data-driven consequence types, selection/spawning modulated by LightLevel/CorruptionLevel and restoration state)
- Veil Heart dawn reflection and contextual payoff based on satisfied warning tags
- Journal/memory system tied to restorations
- Restored actor visual/behavior integration (LanternPost, GardenBed, etc.)
- Expanded ritual archetypes (MirrorPillar, BellShrine prototypes)
- Persistence of dynamic ritual state

## Constraints (non-negotiable for this collaboration)
- Third-person perspective is core.
- Restoration and interpretation are primary; combat is simple and secondary.
- No tower defense, horde survival, village/colony management, survival crafting grind, or large open-world scope in the vertical slice.
- Prefer C++ for core systems + data models; Blueprints for designer iteration, effects, UI.
- Strong data-driven design via Data Assets, Curves, Data Tables.
- Small vertical slices that validate the warning-restoration-night loop before broadening.

## Success Criteria for Collaboration
All agent work must preserve the core fantasy, respect the vertical slice boundary, produce auditable changes on task/candidate branches, pass integration verification on a trusted runtime before promotion to the work branch, and keep docs in sync only after code is green.

See docs/ for detailed phase notes and ArchitectureOverview.md.
