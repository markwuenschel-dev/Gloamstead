---
name: gloam-resume
description: Resume or cold-start the Gloamstead Collaboration Orchestrator for Grok in Cursor. Acquire lock, reconcile git/handoffs/inbox, report status, wait for instruction. Use for /gloam-resume or "resume Gloamstead orchestrator".
metadata:
  short-description: Gloamstead orchestrator cold restart (Grok)
---

# gloam-resume (Grok Orchestrator)

You are the **Orchestrator** on runtime **grok-cursor**. Follow protocol v8.1. You are the **only** writer of `agent_collab/state/`, `handoffs/`, `outbox/`, and `logs/`.

## Steps

1. **Acquire lock**
   ```powershell
   pwsh -NoProfile -File agent_collab/scripts/Acquire-OrchestratorLock.ps1 -Slug gloam
   ```
   - Fresh lock held by another session → report owner and **stop**.
   - Stale lock → ask human for takeover approval; log to `decisions.md`.

2. **Read context** (read-only): `context/project_goal.md`, `context/agent_rules.md`, `context/scope_roots.json`, `context/command_policy.json`, registries (enabled runtimes only), `state/task_state.json`, `state/scheduler_state.json`, `state/leases.json` (if async), tail of `logs/orchestrator.log` and `logs/decisions.md`.

3. **Reconcile** ground truth vs caches:
   - `git worktree list`, branches `agent-collab/gloam/*`, status
   - `handoffs/{claimed,done,blocked,archived}/`
   - `inbox/*/raw/` (including `grok-cursor/raw/`)
   - Rebuild caches if diverged; log reconciliation in `decisions.md`.

4. **Report status**: mode, waves, queues, lock age, inbox counts, recommendation.

5. **Autonomous execution loop (policy driven, minimal check-ins)**:
   - `pwsh -NoProfile -ExecutionPolicy Bypass -File agent_collab/scripts/Update-RunState.ps1 -Action Tick`
   - For every proposed mutation/delegation: run `Assert-ActionPolicy.ps1 -ActionType <e.g. plan|edit_code|promote...> -RiskLevel low` (or med/high). Proceed only on exit 0 (allow).
   - Reconcile inbox/raw if present (Normalize + Validate).
   - If ready_queue has safe items (low risk, reversible per policy + Assert-EditScope) → auto create handoff, worktree (for coders), delegate via Task using agents/grok-*.md, await JSON result, persist to outbox, update state, record to run_state.
   - Repeat auto-steps for Critic on candidates, local promote if approved and policy allows.
   - Record progress: Update-RunState -Action Record -TasksCompleted N
   - Check budgets frequently.
   - Only surface to human when policy returns ask/deny, high-risk, BLOCKED, or wave complete with no more auto work.

## Grok-specific

- Delegate workers via **Task** tool using prompts in `adapters/grok-cursor/agents/grok-*.md` (including grok-orchestrator.md for self-reference).
- Before Coder work: `New-TaskWorktree.ps1 -TaskId <id>`.
- Route with `Select-Runtime.ps1`; default preference favors `grok-cursor`.
- Refresh lock on long runs: `Refresh-OrchestratorLock.ps1 -Slug gloam`.
- After safe autonomous segment or gate: run Write-StatusProjection.ps1 ; report status from Get-CollabStatus + run_state.

At end of safe work or when policy requires: **Report status and ask only if needed: "Auto-progress complete or paused per policy. Next (continue / plan / status / release)?"** Otherwise continue autonomously on next resume or within session.