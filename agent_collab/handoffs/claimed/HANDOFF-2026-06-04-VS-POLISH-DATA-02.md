# Handoff Template

**handoff_id**: HANDOFF-2026-06-04-VS-POLISH-DATA-02
**task_id**: VS-POLISH-DATA-02
**task_type**: ue5-config
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
Create or extend Content/Data/ assets for catalogs and definitions (UDataAsset subclasses or Data Tables). Implement defaults for MVP nights and at least 2 new ritual types. Use editor tools where possible. Builds directly on the exact starter examples/seeds from the promoted VS-POLISH-DATA-FU-03 (see its handoff + backlog + authoring guide "Example Assets").

## Context
Depends on DATA-01 docs + VS-POLISH-DATA-FU-03 (exact assets per guide). Starter assets exist in `Content/Data/` via import factory (verified 2026-06-11 — `specs/data/VERIFICATION-2026-06-11.md`). This handoff covers extending catalog usage / additional assets beyond the starter manifest.

## Assigned Role
coder

## Allowed Runtimes / Preferred
claude-code / claude-code

## Asset Packs
None

## File Ownership
["Content/Data/", "Source/Gloamstead/Data/"]

## Generated Output Ownership
["Content/Data/"]

## Content Policy
See agent_collab/context/content_policy.json: text-editable and generated binary outputs only in approved roots via explicit handoff + automation. No vendor content changes. Direct binary editing forbidden.

## Required Capabilities
["ue5-config", "generated-content-production"]

## Required Verification Profiles
["editor-generation", "map-load"]

## Acceptance Criteria
- Assets load and are assignable in editor.
- C++ defaults still work if no asset assigned.
- Editor-generation verification passes (no load errors).

## Docs Impact
false

## Human Playtest Requirement
false

## Attachments
- agent_collab/outbox/planner/plan-vs-polish-202606.json
- agent_collab/handoffs/claimed/HANDOFF-2026-06-04-VS-POLISH-DATA-01.md (dependency)
- agent_collab/handoffs/claimed/HANDOFF-2026-06-05-VS-POLISH-DATA-FU-03.md (direct dep for starter assets + guide examples)
- agent_collab/backlog/approved/BACKLOG-VS-POLISH-DATA-FU-03.md
- priority-list-wave-vs-polish-202606.md
- docs/systems/03_night_consequence_system.md (authoring guide with exact specs)

## Dependencies
["VS-POLISH-DATA-01"]

## History
- 2026-06-04: Created by Orchestrator (grok) upon approval of wave-vs-polish-202606. Handoff created in claimed/ for worker.
- 2026-06-05: Updated by Orchestrator on backlog promotion: goal/context/attachments refreshed to note direct dep on new VS-POLISH-DATA-FU-03 (and its handoff + exact guide examples). Wave plan now includes all 3 FUs as promoted tasks; scheduler/task_state updated; roadmap/docs/priority list created. DATA-02 now sequenced after FU-03 for asset creation gate.
