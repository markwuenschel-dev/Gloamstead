# Backlog Item

**id**: BACKLOG-VS-POLISH-DATA-FU-01
**title**: Extend FNightSanctuarySnapshot and night scoring to support MirrorPillar / BellShrine favored rituals
**goal**: Update the sanctuary snapshot struct, PCG snapshot builder (BuildSanctuarySnapshot), NightConsequenceManager::ScoreRule (add cases for the two new ERitualType values), and related restoration counting logic so that FNightConsequenceRule::FavoredRitualTypes can meaningfully boost scores for MirrorPillar and BellShrine. Add minimal coverage in any type maps or switch defaults.
**task_type**: ue5-cpp
**slice_id**: vertical-slice
**why_this_matters**: The complete data asset authoring guide produced by VS-POLISH-DATA-01 explicitly documents FavoredRitualTypes (including examples for new ritual types) as the mechanism for the core "I restored the right thing" payoff in night selection. Without snapshot + scoring support, catalog rules favoring the new types (added to the enum in DATA-01) will never trigger boosts. This was a direct gap discovered while authoring the full specs and reviewing current MVP scoring (only 3 Phase-1 types handled). Must land before or alongside full catalog usage in DATA-02 / NIGHT-01 to keep the documented contract honest.
**dependencies**: ["VS-POLISH-DATA-01"]
**asset_packs**: []
**expected_file_ownership**: ["Source/Gloamstead/Data/NightConsequenceTypes.h", "Source/Gloamstead/Systems/NightConsequenceManager.h", "Source/Gloamstead/Systems/NightConsequenceManager.cpp", "Source/Gloamstead/PCG/GloamsteadPCGSubsystem.h", "Source/Gloamstead/PCG/GloamsteadPCGSubsystem.cpp", "Source/Gloamstead/Components/RitualPlacementComponent.*"]
**expected_generated_output_ownership**: []
**risk_level**: low
**required_capabilities**: ["ue5-cpp"]
**required_verification_profiles**: ["compile", "editor-generation"]
**allowed_runtimes**: ["claude-code", "grok"]
**preferred_runtime**: "claude-code"
**acceptance_criteria**:
- FNightSanctuarySnapshot has two new int32 fields: MirrorPillarRestored, BellShrineRestored (default 0). RestoredPointCount / existing fields unchanged.
- GloamsteadPCGSubsystem::BuildSanctuarySnapshot() (and any OnStructureRestored listeners) populates the new counts based on restoration payload RitualType (using the same pattern as the 3 existing).
- NightConsequenceManager::ScoreRule switch statement has explicit cases for ERitualType::MirrorPillar and ERitualType::BellShrine (e.g. Score += count * 0.25f); no silent default for these. Existing Lantern/Garden/Path cases untouched.
- Catalog rules that list the new types in FavoredRitualTypes now correctly influence SelectNightTypeFromCatalog when conditions met (verifiable via log or test BP).
- No compile breaks; PIE with a sample catalog rule favoring a new ritual type shows expected selection bias after restorations of that type.
- A one-line note added to the authoring guide Implementation Status or a cross-ref in 03_night... (or this backlog serves as the note).
**docs_impact**: true
**requires_unreal_editor**: true
**requires_human_playtest**: false
**approval_status**: approved
**created**: 2026-06-05
**updated**: 2026-06-05
**notes**: Discovered during Orchestrator review of DATA-01 deliverables + live Source vs. the new "Data Asset Authoring Guide for Designers (Complete Specs)" section. Directly enables the full night consequence rules documented for Phase 2+ (Retrieval etc. can favor new rituals). Should be scheduled before or in parallel with VS-POLISH-DATA-02 asset population and VS-POLISH-NIGHT-01. 

Promoted by Orchestrator 2026-06-05: copied from proposed/ to approved/ (per human), integrated into wave plan as new tasks VS-POLISH-DATA-FU-01 etc., added to scheduler/task_state, roadmap details updated, docs status notes refreshed. See plan JSON for new task definitions with full ACs/ownership.

Copy to approved/ after human review. Promote to wave via Orchestrator (add to plan JSON + handoff + state) when ready.