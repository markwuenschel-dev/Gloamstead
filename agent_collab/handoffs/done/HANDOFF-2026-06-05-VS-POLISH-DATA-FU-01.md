# Handoff Template

**handoff_id**: HANDOFF-2026-06-05-VS-POLISH-DATA-FU-01
**task_id**: VS-POLISH-DATA-FU-01
**task_type**: ue5-cpp
**slice_id**: vertical-slice
**role**: coder
**from**: orchestrator
**to**: gloam-coder
**selected_runtime**: claude-code
**allowed_runtimes**: ["claude-code", "grok"]
**preferred_runtime**: claude-code
**created**: 2026-06-05T08:30:00Z
**status**: done

## Goal
Extend FNightSanctuarySnapshot and night scoring to support MirrorPillar / BellShrine favored rituals. Update the sanctuary snapshot struct, PCG snapshot builder (BuildSanctuarySnapshot), NightConsequenceManager::ScoreRule (add cases for the two new ERitualType values), and related restoration counting logic so that FNightConsequenceRule::FavoredRitualTypes can meaningfully boost scores for MirrorPillar and BellShrine. (Promoted from approved backlog item BACKLOG-VS-POLISH-DATA-FU-01.md.)

## Context
Part of wave-vs-polish-202606 post-DATA-01 promotion. Discovered gap in DATA-01 review: new enums + FavoredRitualTypes in authoring guide (systems/03) not supported in MVP snapshot/scoring (only 3 Phase-1 rituals). Must land to keep documented contract honest before/parallel DATA-02 and for NIGHT-01. See plan JSON task entry, priority list, and backlog/approved/ for full ACs/why/ownership. Complements DATA-01 complete specs.

## Assigned Role
coder

## Allowed Runtimes / Preferred
claude-code, grok / claude-code

## Asset Packs
None

## File Ownership
["Source/Gloamstead/Data/NightConsequenceTypes.h", "Source/Gloamstead/Systems/NightConsequenceManager.h", "Source/Gloamstead/Systems/NightConsequenceManager.cpp", "Source/Gloamstead/PCG/GloamsteadPCGSubsystem.h", "Source/Gloamstead/PCG/GloamsteadPCGSubsystem.cpp", "Source/Gloamstead/Components/RitualPlacementComponent.*"]

## Generated Output Ownership
[]

## Content Policy
See agent_collab/context/content_policy.json: text-editable project-owned (Source/). No direct binary. Update docs_impact items in 03_night... as part of work.

## Required Capabilities
["ue5-cpp"]

## Required Verification Profiles
["compile", "editor-generation"]

## Acceptance Criteria
- FNightSanctuarySnapshot has two new int32 fields: MirrorPillarRestored, BellShrineRestored (default 0). RestoredPointCount / existing fields unchanged.
- GloamsteadPCGSubsystem::BuildSanctuarySnapshot() (and any OnStructureRestored listeners) populates the new counts based on restoration payload RitualType (using the same pattern as the 3 existing).
- NightConsequenceManager::ScoreRule switch statement has explicit cases for ERitualType::MirrorPillar and ERitualType::BellShrine (e.g. Score += count * 0.25f); no silent default for these. Existing Lantern/Garden/Path cases untouched.
- Catalog rules that list the new types in FavoredRitualTypes now correctly influence SelectNightTypeFromCatalog when conditions met (verifiable via log or test BP).
- No compile breaks; PIE with a sample catalog rule favoring a new ritual type shows expected selection bias after restorations of that type.
- A one-line note added to the authoring guide Implementation Status or a cross-ref in 03_night... (or this backlog serves as the note).

## Docs Impact
true

## Human Playtest Requirement
false

## Attachments
- agent_collab/outbox/planner/plan-vs-polish-202606.json
- agent_collab/backlog/approved/BACKLOG-VS-POLISH-DATA-FU-01.md
- docs/systems/03_night_consequence_system.md (authoring guide)
- priority-list-wave-vs-polish-202606.md

## Dependencies
["VS-POLISH-DATA-01"]

## History
- 2026-06-05: Created by Orchestrator upon promotion of approved backlog item (copied from proposed/ per human directive). Added as missing plan task VS-POLISH-DATA-FU-01 (with full ACs/ownership from backlog). Integrated into wave DAG, scheduler, task_state. Roadmap and key docs (systems/03, Phase2, production/01) updated with status/details. Priority list created.
- Follow DATA-01 completion and enum+guide work. Prep for human editor verification on compile/map steps.
- Executed by grok (as coder in .grok/worktrees/VS-POLISH-DATA-FU-01 on task branch). Changes: snapshot struct + PCG builder + ScoreRule cases + doc note in worktree and main. Committed locally (4f7f13c). Task marked done in state. Handoff moved to done/. Worker summary to be produced. Verification: text + code review (editor-gen pending human).