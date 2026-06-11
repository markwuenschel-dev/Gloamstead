# Backlog Item

**id**: BACKLOG-VS-POLISH-DATA-FU-03
**title**: Provide concrete starter Data Asset seeds / tuning values matching the exact examples in the DATA-01 authoring guide
**goal**: Create (or document in machine-readable form for DATA-02) the exact example assets called out in "Example Assets (design specs)" and "Example Asset (pseudo...)" sections of the new authoring guide: e.g. DA_NightConsequenceCatalog with a starter set of 5-7 rules covering MVP + at least 2 advanced nights; DA_VeilHeartWarningCatalog with the 2-3 example fragments; and the 4 DA_Ritual_* (LanternPost, GardenBed, MirrorPillar, BellShrine) with the precise Light/Corruption/Radius/Tags values listed. Can be .uasset via editor, or .json DataTable seeds + import script note, or detailed table in docs/ for the coder to replicate exactly. Assign defaults to a test VeilHeart / NightManager / PlacementComponent.
**task_type**: ue5-config
**slice_id**: vertical-slice
**why_this_matters**: VS-POLISH-DATA-01 delivered the *complete designer contract* with specific numeric examples and fragment text so that DATA-02 (and later tuning) has no ambiguity. Without actually materializing (or providing copy-paste ready seeds for) those exact examples, DATA-02 risks drift, and designers cannot immediately iterate the "full" catalog in-editor as the guide promises. This item bridges the text spec to the first real Content/Data/ assets and ensures the guide is not just aspirational.
**dependencies**: ["VS-POLISH-DATA-01"]
**asset_packs**: []
**expected_file_ownership**: ["Content/Data/", "docs/systems/03_night_consequence_system.md", "Source/Gloamstead/Data/"]
**expected_generated_output_ownership**: ["Content/Data/"]
**risk_level**: low
**required_capabilities**: ["ue5-config", "documentation"]
**required_verification_profiles**: ["editor-generation", "map-load", "text-only"]
**allowed_runtimes**: ["claude-code"]
**preferred_runtime**: "claude-code"
**acceptance_criteria**:
- At minimum the 4 ritual DAs + one VeilHeartWarningCatalog + one NightConsequenceCatalog exist in Content/Data/ (or as importable seeds) with *exactly* the values and fragments listed in the guide's example sections (or a documented delta).
- A test level or the main VeilHeart/Night manager has the catalogs assigned (or a BP note on how to assign).
- In PIE, AdvanceToNextPhase uses rules from the asset (not just MVP populate); warnings from the catalog appear; ritual tuning values are read via GetRitualDefinitionForType.
- Editor load succeeds; no property mismatches with the C++ USTRUCTs/UCLASSes.
- The authoring guide's "Example Asset" blocks are updated with a "See also: Content/Data/DA_* (created per BACKLOG-VS-POLISH-DATA-FU-03)" note.
- Verification (editor-generation + map-load) recorded.
**docs_impact**: true
**requires_unreal_editor**: true
**requires_human_playtest**: false
**approval_status**: approved
**created**: 2026-06-05
**updated**: 2026-06-05
**notes**: This can be rolled into VS-POLISH-DATA-02 (the existing ue5-config task) as its primary deliverable, or treated as a follow-up refinement if DATA-02 is kept minimal. Either way, creating the backlog item ensures the specific examples from the DATA-01 guide are tracked and not lost. Preferred for the claude-code gloam-coder. Human will need to perform/approve the actual editor asset creation + verification per content policy (no direct binary by agents).

Promoted by Orchestrator 2026-06-05: copied from proposed/ to approved/ (per human), integrated into wave plan as new tasks VS-POLISH-DATA-FU-03 etc., added to scheduler/task_state, roadmap details updated, docs status notes refreshed. See plan JSON for new task definitions with full ACs/ownership.

Copy to approved/ after human review. Promote to wave via Orchestrator (add to plan JSON + handoff + state) when ready.