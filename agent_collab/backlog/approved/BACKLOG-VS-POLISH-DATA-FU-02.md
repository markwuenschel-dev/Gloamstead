# Backlog Item

**id**: BACKLOG-VS-POLISH-DATA-FU-02
**title**: Extend GetNightConsequenceTypeDisplayName and PopulateMVPNightConsequenceRules for all documented night types + richer MVP examples
**goal**: Update the display name helper (and any UI-facing uses) to return proper strings for Retrieval, SilencePossession, Mirror, Bargain, Fracture, TrueSiege (currently fall to "Invalid"). Extend or add a secondary Populate function (or enrich the existing MVP one with comments) that includes sample rules for the additional night types using the full FNightConsequenceRule fields as specified in the DATA-01 authoring guide (weights, bands, favored rituals, OmenClueTag). Update the pseudo "Example Asset" in the guide if the populate changes.
**task_type**: ue5-cpp
**slice_id**: vertical-slice
**why_this_matters**: The authoring guide written in VS-POLISH-DATA-01 lists the *complete* ENightConsequenceType values and shows example catalog rules that include Retrieval etc. for Phase 2+. Current MVP populate only seeds Tutorial/Corruption/Omen, and display name is incomplete. This makes logs, UI (future), and designer iteration using the catalog DA produce "Invalid" for advanced nights and prevents easy testing of the full documented contract without hand-authoring every rule in-editor. Gap surfaced while writing the "full night-type catalog" section and verifying against NightConsequenceTypes.cpp.
**dependencies**: ["VS-POLISH-DATA-01"]
**asset_packs**: []
**expected_file_ownership**: ["Source/Gloamstead/Data/NightConsequenceTypes.h", "Source/Gloamstead/Data/NightConsequenceTypes.cpp", "docs/systems/03_night_consequence_system.md"]
**expected_generated_output_ownership**: []
**risk_level**: low
**required_capabilities**: ["ue5-cpp", "documentation"]
**required_verification_profiles**: ["compile", "text-only"]
**allowed_runtimes**: ["claude-code", "grok"]
**preferred_runtime**: "grok"
**acceptance_criteria**:
- GetNightConsequenceTypeDisplayName returns sensible names (e.g. "Retrieval", "Silence/Possession", "Mirror", "Bargain", "Fracture", "True Siege") for all non-Invalid values; unit-testable or log-verifiable.
- PopulateMVPNightConsequenceRules (or a new PopulateFullNightConsequenceRulesForTesting) seeds at least one rule for each new night type using realistic bands/favored/OmenClue from the guide examples. Existing MVP 3 rules remain identical.
- Calling the populate + iterating Rules produces entries for advanced types without crashing; GetDisplayName on them succeeds.
- The "Example Asset (pseudo for DA_NightConsequenceCatalog)" section in the authoring guide remains accurate or is lightly updated with a note referencing this backlog item.
- No behavior change for Tutorial/Corruption/Omen paths.
**docs_impact**: true
**requires_unreal_editor**: false
**requires_human_playtest**: false
**approval_status**: approved
**created**: 2026-06-05
**updated**: 2026-06-05
**notes**: Low-risk hygiene item to keep the "complete specs" in the DATA-01 guide executable in the current C++ base. Can be done by documentor (grok) or coder. Complements DATA-02 (which will create real DA assets using these rules) and NIGHT-01 (which will implement behaviors for the types). Add to scheduler/plan when approved if it unblocks testing of catalog rules for new nights.

Promoted by Orchestrator 2026-06-05: copied from proposed/ to approved/ (per human), integrated into wave plan as new tasks VS-POLISH-DATA-FU-02 etc., added to scheduler/task_state, roadmap details updated, docs status notes refreshed. See plan JSON for new task definitions with full ACs/ownership.

Copy to approved/ after human review. Promote to wave via Orchestrator (add to plan JSON + handoff + state) when ready.