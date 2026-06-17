# Handoff Template

**handoff_id**: HANDOFF-2026-06-05-VS-POLISH-DATA-FU-03
**task_id**: VS-POLISH-DATA-FU-03
**task_type**: ue5-config
**slice_id**: vertical-slice
**role**: coder
**from**: orchestrator
**to**: gloam-coder
**selected_runtime**: claude-code
**allowed_runtimes**: ["claude-code"]
**preferred_runtime**: claude-code
**created**: 2026-06-05T08:30:00Z
**status**: done

## Goal
Provide concrete starter Data Asset seeds / tuning values matching the exact examples in the DATA-01 authoring guide. Create the exact DA_NightConsequenceCatalog, DA_VeilHeartWarningCatalog, and 4 DA_Ritual_* with the precise values/fragments from the guide's "Example Assets (design specs)" and "Example Asset (pseudo...)" sections. (Promoted from approved backlog item BACKLOG-VS-POLISH-DATA-FU-03.md; primary deliverable feeding DATA-02.)

## Context
Critical for making the complete designer contract from VS-POLISH-DATA-01 (systems/03 authoring guide) real in Content/Data/. DATA-02 now depends directly on this. First point where actual assets are required (human editor creation + verification per content_policy, empty generated roots, unavailable autonomous profiles). See "When will you need actual assets?" analysis, priority list (Phase B), plan task, backlog, and guide for exact numbers (e.g. DA_Ritual_LanternPost Light=0.4 etc.) + fragments. Prep scope_roots update before execution.

## Assigned Role
coder

## Allowed Runtimes / Preferred
claude-code / claude-code

## Asset Packs
None

## File Ownership
["Content/Data/", "docs/systems/03_night_consequence_system.md", "Source/Gloamstead/Data/"]

## Generated Output Ownership
["Content/Data/"]

## Content Policy
See agent_collab/context/content_policy.json: generated binary outputs ONLY via explicit handoff + editor automation in approved coder_generated_output_roots (currently empty in scope_roots -- Orchestrator note: update scope first). No direct binary. Human performs editor steps + records evidence (editor-gen + map-load). Update guide note on creation.

## Required Capabilities
["ue5-config", "documentation", "generated-content-production"]

## Required Verification Profiles
["editor-generation", "map-load", "text-only"]

## Acceptance Criteria
- At minimum the 4 ritual DAs + one VeilHeartWarningCatalog + one NightConsequenceCatalog exist in Content/Data/ (or as importable seeds) with *exactly* the values and fragments listed in the guide's example sections (or a documented delta).
- A test level or the main VeilHeart/Night manager has the catalogs assigned (or a BP note on how to assign).
- In PIE, AdvanceToNextPhase uses rules from the asset (not just MVP populate); warnings from the catalog appear; ritual tuning values are read via GetRitualDefinitionForType.
- Editor load succeeds; no property mismatches with the C++ USTRUCTs/UCLASSes.
- The authoring guide's "Example Asset" blocks are updated with a "See also: Content/Data/DA_* (created per VS-POLISH-DATA-FU-03)" note.
- Verification (editor-generation + map-load) recorded.

## Docs Impact
true

## Human Playtest Requirement
false

## Attachments
- agent_collab/outbox/planner/plan-vs-polish-202606.json (DATA-02 depends on this)
- agent_collab/backlog/approved/BACKLOG-VS-POLISH-DATA-FU-03.md
- docs/systems/03_night_consequence_system.md (full guide + examples)
- priority-list-wave-vs-polish-202606.md
- agent_collab/context/scope_roots.json (prep generated roots)

## Dependencies
["VS-POLISH-DATA-01"]

## History
- 2026-06-05: Created by Orchestrator upon promotion of approved backlog to claimed/. Added as VS-POLISH-DATA-FU-03 to plan (DATA-02 dep), state, scheduler. Roadmap/docs/priority list updated. This is the asset creation gate -- coordinate human editor session. Existing DATA-02 handoff context updated in parallel if needed.
- 2026-06-11: Assets created via import factory; editor wiring + PIE smoke on `Lvl_ThirdPerson`. Catalog load from `DA_NightConsequenceCatalog` confirmed. Dusk warning + non-Tutorial nights deferred until PCG init. Evidence: `specs/data/VERIFICATION-2026-06-11.md`.
