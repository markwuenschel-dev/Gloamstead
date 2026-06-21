# Handoff Template

**handoff_id**: HANDOFF-2026-06-04-VS-POLISH-VISUALS-01
**task_id**: VS-POLISH-VISUALS-01
**task_type**: generated-content
**slice_id**: vertical-slice
**role**: coder
**from**: orchestrator
**to**: gloam-coder
**selected_runtime**: claude-code
**allowed_runtimes**: ["claude-code"]
**preferred_runtime**: claude-code
**created**: 2026-06-04T19:30:00Z
**status**: archived

## Goal
Create or adapt restored actor meshes, materials, VFX for at least 3 ritual types (lantern, garden, etc.) using OnRestoredActorSpawned. Use asset packs or project content via approved factory/adapters where possible. Per Phase 3 and art direction.

## Context
Depends on DATA-02. Visual polish for restorations in VS.

## Assigned Role
coder

## Allowed Runtimes / Preferred
claude-code / claude-code

## Asset Packs
["curated-props-vfx"]

## File Ownership
["Content/Blueprints/", "Content/Meshes/", "Content/Materials/", "Content/VFX/"]

## Generated Output Ownership
["Content/"]

## Content Policy
Generated binary outputs only via handoff + automation in assigned roots. Use adapters for asset packs, do not modify vendor in place.

## Required Capabilities
["generated-content-production", "asset-pack-analysis"]

## Required Verification Profiles
["editor-generation", "map-load"]

## Acceptance Criteria
- Visuals spawn on restoration for assigned rituals.
- Match Withered Gothic Realism and ritualistic naturalism.
- No vendor content modified in place.

## Docs Impact
false

## Human Playtest Requirement
false

## Attachments
- plan json
- art/04_art_direction.md

## Dependencies
["VS-POLISH-DATA-02"]

## History
- 2026-06-04: Created by Orchestrator.
- 2026-06-20: ARCHIVED. Task never executed; no worktree was ever created (`git worktree list` shows only the main checkout; `.grok/worktrees/` does not exist). Superseded by direct Phase-3 branch development on `feat/a1-sanctuary-bootstrap` (Phase 3 — Six-Hour Experience, see docs/Phase3_SixHourExperience.md). Wave wave-vs-polish-202606 closed as superseded during /gloam-status reconcile.
