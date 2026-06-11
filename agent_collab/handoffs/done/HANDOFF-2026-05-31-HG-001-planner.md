# Handoff

**handoff_id**: HANDOFF-2026-05-31-HG-001-planner
**task_id**: HG-001
**role**: Planner
**from**: orchestrator
**to**: claude-planner
**created**: 2026-05-31T17:20:00Z
**status**: claimed

## Input Artifacts
- agent_collab/context/project_goal.md
- agent_collab/context/agent_rules.md
- agent_collab/context/scope_roots.json
- agent_collab/registry/agents.json (claude-code templates only)
- agent_collab/registry/capabilities.json

## Objective (from Human Orchestrator)
Break down the following goal into a dependency DAG for the Hard Gate task:

**Goal**: Implement a new PowerShell script `agent_collab/scripts/Get-CollabStatus.ps1` that provides a concise, human- and machine-readable snapshot of the current state of the agent_collab system. It should be safe to run at any time (read-only) and useful for the Orchestrator during resume/status checks.

Key requirements from Orchestrator:
- Must respect all scope guards (only edit under scripts/ via Assert-EditScope).
- Output should include: current mode, lock status/age, wave/queue summary, recent handoffs count, inbox raw counts, last 5 lines of decisions.md, current git branch + any agent-collab/gloam branches, worktree count.
- Should be a single .ps1 file with good help/comments.
- Must be executable via `pwsh -NoProfile -File ...`
- After implementation, it will be reviewed by Critic and (if approved) promoted.

This is the **Hard Gate** task for Phase 2 per the design. Keep scope narrow and low-risk.

## Runtime Notes
- chosen_runtime: claude-code
- template_id: claude-planner
- reason: Read-only DAG planning task. claude-code is the only enabled runtime that satisfies dag_planning + file_ownership_prediction capabilities.

## Next Action (Orchestrator)
Await planner_output.json matching planner_output.schema.json. Then create Coder handoff(s).
