# Priority List for wave-vs-polish-202606 (updated 2026-06-05)

As Orchestrator, after promoting the 3 approved backlog FUs into the plan (now 12 tasks total), here is the recommended execution priority order. This respects the DAG in plan-vs-polish-202606.json, maximizes parallel work (max_parallel=4), prioritizes unblocking the data contract + "I restored the right thing" core loop, accounts for human-gated editor steps (editor-generation/map-load unavailable autonomously per verification_profiles), and sequences asset creation after specs/C++ support.

Assets (real .uasset in Content/Data/ and later Content/) are first required at FU-03 / DATA-02 time (see user query "When will you need actual assets?"). Before that, C++/text work can proceed using MVP fallbacks.

## Priority Order (recommended sequence / parallel groups)

**Phase A: Data Contract Hygiene (unblock everything; low risk, some can be grok/doc heavy)**
1. **VS-POLISH-DATA-FU-02** (preferred: grok; allowed grok/claude; verification: compile + text-only; no UE editor required)
   - Extend display names + Populate for full 10 night types + update guide examples.
   - Why first: Makes the complete specs from DATA-01 actually executable in code/logs without "Invalid". Low dependency on editor.
   - Can start immediately (depends only on done DATA-01).

2. **VS-POLISH-DATA-FU-01** (preferred: claude-code; verification: compile + editor-generation)
   - Snapshot fields + PCG builder + ScoreRule cases for MirrorPillar/BellShrine.
   - Why high: Enables FavoredRitualTypes for new rituals in catalog (core fantasy). Dep for NIGHT-01. Requires some editor for verification but C++ primary.

**Phase B: Asset Creation Gate (first need for actual binaries; human editor required)**
3. **VS-POLISH-DATA-FU-03** (preferred: claude-code; verification: editor-generation + map-load + text-only; requires_unreal_editor: true)
   - Create exact starter DA_NightConsequenceCatalog, DA_VeilHeartWarningCatalog, 4x DA_Ritual_* matching the precise values/fragments in the authoring guide (systems/03 "Example Assets (design specs)").
   - Why: Bridges the text contract to real Content/Data/. DATA-02 now depends on this. Human must perform editor creation + assign + evidence (per content_policy + empty coder_generated_output_roots currently -- prep scope update if not done).
   - **This is when actual assets are needed** (see separate analysis). Prepare scope_roots.json update + detailed checklist from guide before claiming handoff.

4. **VS-POLISH-DATA-02** (preferred: claude-code; now depends on DATA-01 + FU-03)
   - Extend/create additional assets or Data Tables if needed; ensure C++ fallbacks; integrate.
   - Builds directly on FU-03 examples. Human editor step.

**Phase C: Parallel Implementation (after assets or in parallel where possible; many editor-gated)**
- **VS-POLISH-JOURNAL-01** (depends on DATA-01; ue5-cpp; can start early with MVP)
- **VS-POLISH-UI-01** (depends on DATA-02; gameplay + UI hooks for warnings/omen/journal)
- **VS-POLISH-VISUALS-01** (depends on DATA-02; asset_pack "curated-props-vfx"; meshes/VFX for restorations; human asset acquisition + editor)
- **VS-POLISH-COMBAT-01** (depends on DATA-02; simple pressure mechanics)

**Phase D: Integration & Polish**
- **VS-POLISH-PERSIST-01** (depends on JOURNAL-01; save/load for state/Heart)
- **VS-POLISH-NIGHT-01** (depends on DATA-02 + COMBAT-01 + FU-01; expand night types + spawns using the now-supported catalog rules)

**Final Gate**
- **VS-POLISH-PLAYTEST-01** (human; depends on UI/VISUALS/PERSIST/NIGHT/COMBAT; record in outbox/playtest/ with 7+ ratings + notes; creates further backlog if needed)

## Notes on Parallelism & Gating
- Max 4 in flight. FUs + JOURNAL can overlap early (text/C++).
- Editor steps (FU-01/03, DATA-02, UI, VISUALS, NIGHT, COMBAT, PERSIST) require human in loop for verification (profiles unavailable autonomously; see verification_profiles.json and environment.md). No UnrealEditor-Cmd wrappers yet.
- Update scope_roots.json (coder_generated_output_roots to include "Content/Data/" and relevant Content/ subdirs) + Assert-EditScope if needed before heavy asset work.
- Handoffs: Existing DATA-02 handoff in claimed/; create new claimed/ handoffs for FUs when promoted/assigned (use Invoke or runtime claim).
- Backlog: The 3 are now in approved/ with status updated; remove from proposed (done).
- Roadmap: Updated with details on FUs and data polish.
- Docs: systems/03, Phase2_CoreLoop.md, production/01_asset_and_tech_rules.md refreshed with status, cross-refs, and "as we go" notes. Continue this for future completions (e.g. append to Implementation Status when FU-01 lands).
- Priority can shift based on human availability for editor sessions or specific runtime (grok for FU-02).

## Execution Recommendations (as Orchestrator)
- Start with FU-02 (grok friendly, unblocks contract immediately).
- Prep scope + detailed asset creation spec/checklist (extracted from guide) before FU-03/DATA-02.
- After FUs + DATA-02, batch the parallel C tasks.
- Before PLAYTEST, ensure all verifs (including human playtest evidence) pass per profiles; Critic review on candidate.
- Re-run Reconcile-CollaborationState.ps1 after state changes.
- Log all in decisions.md (this list is also persisted here).

This list + plan JSON + backlog/approved/ + updated roadmap/docs keep everything consistent and actionable. Next: human confirm or direct which to claim handoff for / start worker on. 

(Generated as Orchestrator action on user directive to create missing plan tasks, update roadmap/docs, provide priority.)