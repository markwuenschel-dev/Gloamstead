# Human Approval Gates — Gloamstead UE5 (agent_collab)

**Principle**: The human approves *authority and risk acceptance*, not low-level mechanics.

See also: autonomy_policy.json ("ask" actions), content_policy.json (human_gates list), verification_profiles.json (availability + requires_human), workflow_activation.json, and the full diagnosis in docs/agents/UE5-Agent-Substrate-Review.md section 10.

## Always Human (blocked for autonomous action; "ask" or explicit gate)

- Acquiring, purchasing, or downloading Marketplace/Fab asset packs.
- Accepting any asset, plugin, or content license.
- Installing or enabling new plugins (project or engine).
- Changing EngineAssociation in .uproject or upgrading the engine.
- Any destructive delete of project-owned Content/ (outside approved automation on an owned task branch with generated_output_ownership).
- Any modification under vendor_content_roots (Content/ThirdPerson, Content/Characters and subpaths).
- Running cook, package, BuildCookRun, or release packaging.
- Recording a human playtest result (human performs the playtest and provides the structured data; Orchestrator only persists it to outbox/playtest/).
- Any task that declares a required verification profile whose availability is "unavailable" (compile, editor-generation, map-load, automation-test, package-smoke, full candidate-integration UE steps) until that profile is explicitly whitelisted for the handoff with evidence capture instructions.

## High-Risk (Orchestrator must surface explicit human confirmation before proceeding)

- Data schema or contract changes that affect persistent state, ritual catalogs, night consequence rules, restoration progression, or save format.
- Large multi-file vertical slice waves that produce generated binary content (maps, PCG outputs, Data Assets under Content/Data/).
- Any wave where requires_human_playtest = true (playtest must be performed and recorded before promotion).
- Tasks that would touch or regenerate Content/ outside the narrow coder_generated_output_roots currently defined in scope_roots.json.
- First use of any newly discovered editor automation, commandlet, or Python script for content generation (must be added to verification_profiles + handoff-whitelisted before autonomous use).

## Low-Risk (may proceed autonomously when other controls pass)

- Pure text edits inside assigned file_ownership under coder_edit_roots (Source/, Config/) with text-only verification profile available and no docs_impact or playtest flag.
- Planner, architecture-analysis playbook, or external-research playbook invocations (read-only).
- Post-promotion documentation-update playbook execution (after Critic APPROVED).
- Reconciliation, lease management, and status projection updates (Orchestrator lease holder only).
- Small bug-fix or refactor tasks that stay within clear single-file or tightly-related owned paths and pass text-only + Critic.

## Evidence and Recording

When a human gate is exercised:
- The exact action, risk level, and human directive/confirmation must be appended to logs/decisions.md.
- For playtest: structured result (ratings + free text per the human-playtest profile) must land in outbox/playtest/ before the task/wave can be marked complete.
- For verification profiles: the human-captured log excerpts, success statements, or screenshots/notes must be referenced from the handoff or critic verdict.

## Orchestrator Duty

The lease-holding Orchestrator is responsible for stopping at these gates, presenting the exact authority question to the human ("Do you authorize this schema change for the NightConsequence catalog given the save implications?"), and only proceeding on clear affirmative. The human is not asked to "run the build" or perform the mechanical steps unless the action itself is the gate (e.g. playtest performance or manual compile evidence).

Never hide the gate behind "the agent will handle it." The substrate's job is to make the boundary visible and auditable.
