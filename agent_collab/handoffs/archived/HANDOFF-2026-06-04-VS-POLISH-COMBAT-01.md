# Handoff Template

**handoff_id**: HANDOFF-2026-06-04-VS-POLISH-COMBAT-01
**task_id**: VS-POLISH-COMBAT-01
**task_type**: gameplay-loop
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
Implement simple third-person combat and cleansing mechanics (move, attack, block/ward/cleanse, interact) as pressure during nights. Tie to night types (e.g., cleanse corruption, ward possession). Per combat design doc and ProjectRules (simple, not dominant).

## Context
Depends on DATA-02. Enables night pressure in VS polish.

## Assigned Role
coder

## Allowed Runtimes / Preferred
claude-code / claude-code

## Asset Packs
None

## File Ownership
["Source/Gloamstead/", "Content/Blueprints/"]

## Generated Output Ownership
["Content/Blueprints/"]

## Content Policy
Generated content approved.

## Required Capabilities
["ue5-cpp", "generated-content-production"]

## Required Verification Profiles
["compile", "editor-generation", "map-load"]

## Acceptance Criteria
- Basic third-person combat functional in night context.
- Cleansing interacts with restored structures and consequences.
- Does not overshadow restoration/interpretation (per pillars).

## Docs Impact
false

## Human Playtest Requirement
false

## Attachments
- plan json
- systems/04_combat_and_interaction_system.md

## Dependencies
["VS-POLISH-DATA-02"]

## History
- 2026-06-04: Created by Orchestrator.
- 2026-06-20: ARCHIVED. Task never executed; no worktree was ever created (`git worktree list` shows only the main checkout; `.grok/worktrees/` does not exist). Superseded by direct Phase-3 branch development on `feat/a1-sanctuary-bootstrap` (Phase 3 — Six-Hour Experience, see docs/Phase3_SixHourExperience.md). Wave wave-vs-polish-202606 closed as superseded during /gloam-status reconcile.
