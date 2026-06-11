# Handoff Template

**handoff_id**: HANDOFF-2026-06-04-VS-POLISH-NIGHT-01
**task_id**: VS-POLISH-NIGHT-01
**task_type**: gameplay-loop
**slice_id**: vertical-slice
**role**: coder
**from**: orchestrator
**to**: gloam-coder
**selected_runtime**: claude-code
**allowed_runtimes**: ["claude-code"]
**preferred_runtime**: claude-code
**created**: 2026-06-04T19:30:00Z
**status**: claimed

## Goal
Expand night types and spawn/mechanics beyond stubs: implement at least Retrieval, Silence/Possession, and basic entity spawning tied to consequence rules. Add failure hooks and resource rewards. Per night system design and deferred items.

## Context
Depends on DATA-02 and COMBAT. Key for full VS polish, making nights rule-driven tests.

## Assigned Role
coder

## Allowed Runtimes / Preferred
claude-code / claude-code

## Asset Packs
None

## File Ownership
["Source/Gloamstead/Systems/", "Source/Gloamstead/"]

## Generated Output Ownership
["Content/Blueprints/"]

## Content Policy
Generated via automation.

## Required Capabilities
["ue5-cpp", "ue5-config", "generated-content-production"]

## Required Verification Profiles
["compile", "editor-generation", "map-load"]

## Acceptance Criteria
- 3+ night types functional with distinct behaviors and visuals.
- Spawns or effects tied to restoration state and warnings.
- Dawn feedback reflects night outcome.

## Docs Impact
false

## Human Playtest Requirement
false

## Attachments
- plan json
- systems/03_night_consequence_system.md

## Dependencies
["VS-POLISH-DATA-02", "VS-POLISH-COMBAT-01"]

## History
- 2026-06-04: Created by Orchestrator.
