# Documentation Update Playbook + Checklist (replaces former "documentor" role)

**Classification**: skill/playbook + post-promotion checklist. Not a first-class agent role.

**Trigger**:
- A handoff declares `docs_impact: true`.
- Or a wave produces changes that affect design docs, systems/, production/, or open questions.

**Timing**: Only after Critic has returned APPROVED on the candidate and (if required) human playtest is recorded. Never before integration is green.

**Scope**:
- Edits only inside `docs/` (documentor_edit_roots per scope_roots.json).
- Never touch Source/, Config/, Content/, or agent_collab/ state.

**Checklist** (must be explicitly walked; attach results to the handoff or outbox/documentor entry):
1. Identify the design documents that describe the changed behavior (systems/*.md, game/*.md, production/*.md, questions/*.md, art/*.md).
2. Update the "current baseline" or "implementation" sections to match what was actually landed (not the plan).
3. If new data assets, enums, or PCG nodes were introduced, add or update the relevant Phase0/Phase1/ritual or system doc with the concrete types and file locations.
4. Note any open questions that are now answered or newly raised.
5. Cross-link from the changed code (header comments or README) back to the doc if the convention exists.
6. Verify no invented canonical names (per ProjectRules.md).
7. Confirm the change still aligns with the north star quote and core pillars.

**Output**:
- List of edited files under docs/.
- Checklist status (all items addressed or explicit "N/A because..." for each).
- Any new open questions surfaced.

**Invocation**:
- Prefer running this as an Orchestrator skill or narrow post-promotion step after the real work is promoted.
- For large waves, the Planner may include a documentation task that uses this playbook (still executed after Critic APPROVED).
- The old "gloam-documentor" template may remain for transition but should not be used for new work.

**Rules**:
- Documentation is never on the critical path for code promotion.
- Human may always perform the update directly; the playbook exists to make agent assistance consistent and scoped.

Reference: docs/agents/UE5-Agent-Substrate-Review.md (sections 3-4), scope_roots.json (documentor_edit_roots), agent_rules.md (Documentor boundaries), docs/agents/ProjectRules.md (naming, implementation bias).
