# Playbooks (Skills, Rubrics, Checklists)

These are **not agent roles**. They are reusable procedures, rubrics, and checklists extracted from what were previously over-privileged first-class roles (architect, researcher, documentor).

See `docs/agents/UE5-Agent-Substrate-Review.md` (sections 2-4, 11) and `agent_collab/context/workflow_activation.json`.

## Current Playbooks

- `architecture-analysis.md` — Cross-cutting design rubric. Invoke inside Planner or as Orchestrator step for complex data/contract/PCG boundary work.
- `external-research.md` — Read-only external investigation (UE docs, packs, plugins). Parallelizable information gathering only. Never authorizes acquisition or installs.
- `documentation-update.md` — Post-promotion docs/ edit checklist. Run only after Critic APPROVED (and playtest if required). Scope strictly limited to docs/.

## Usage

- Prefer embedding the relevant playbook steps inside an existing Planner handoff or as a narrow Orchestrator action after promotion.
- Do not create new handoffs or leases whose primary purpose is one of these playbooks.
- Update the playbooks when project conventions (ProjectRules, scope_roots, verification profiles) change.

These mechanisms reduce agent sprawl while preserving the knowledge and process that the old roles captured.
