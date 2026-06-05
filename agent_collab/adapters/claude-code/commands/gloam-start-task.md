---
description: Start a single well-scoped task under Orchestrator supervision (used for ad-hoc or recovery). Acquires lock, creates handoff + task state entry, routes to appropriate worker.
allowed-tools: Read, Write, Grep, Glob, Bash, Task, Edit
---

# gloam-start-task

**Warning**: This is an escape hatch. Prefer full wave planning via the Orchestrator loop.

## Preconditions
- You are acting as (or under) the Orchestrator.
- Lock is held (run Acquire if needed).

## Steps
1. Read the task description provided by the human (goal, rough acceptance criteria, risk guess).
2. Optionally invoke Planner (claude-planner) or Architect first if cross-cutting.
3. Create a minimal handoff in handoffs/claimed/ using HANDOFF_TEMPLATE.md as base.
4. Create/update task_state.json entry (status: planned -> assigned).
5. Determine required_capabilities from goal.
6. Run routing logic (respect safety floor) to choose runtime + template.
7. Create task branch if needed: git checkout -b agent-collab/gloam/task/<task_id> (or worktree for Coder).
8. Write worker_request.
9. Delegate (in sync mode: spawn subagent using the template; in async: write request and lease).
10. On result: normalize/validate, persist via single-writer path only, update state, log decision.
11. Release lock only on clean handoff to human or completion of this bounded task.

## Output
Report the created task_id, branch, assigned template, and handoff path. Then stop and return control.

Never use this to bypass wave scheduling or integration verification for code changes.
