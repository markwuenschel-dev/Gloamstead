---
name: gloam-start-task
description: Start one bounded Gloamstead task under Orchestrator supervision (ad-hoc). Acquires lock, handoff, routing, delegation. Use for /gloam-start-task.
metadata:
  short-description: Gloamstead ad-hoc task start
---

# gloam-start-task

Escape hatch — prefer full wave planning via `/gloam-resume` loop.

## Preconditions

- Acting as Orchestrator on **grok-cursor**.
- Lock held (`Acquire-OrchestratorLock.ps1 -Slug gloam`).

## Steps

1. Parse human goal, acceptance criteria, risk guess.
2. Optionally delegate Planner (`grok-planner` via Task + `agents/grok-planner.md`).
3. Create handoff in `handoffs/claimed/` from `HANDOFF_TEMPLATE.md`.
4. Update `task_state.json` (Orchestrator only).
5. `Select-Runtime.ps1` with correct `-Role` and `-SafetyFloor`.
6. If Coder on grok-cursor: `New-TaskWorktree.ps1 -TaskId <id>`.
7. Delegate via Task tool; require JSON per `result_contract.md`.
8. Validate, write `outbox/`, update state, log `decisions.md`.

Report: `task_id`, branch, template, handoff path. Stop for human input.

Never bypass integration verification for code promotion.