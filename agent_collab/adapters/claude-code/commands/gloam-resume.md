---
description: Resume or cold-start the Gloamstead Collaboration Orchestrator (acquire lock, reconcile, report status, wait for instruction).
allowed-tools: Read, Grep, Glob, Bash, Task
---

# gloam-resume

**Purpose**: Cold-restartable Orchestrator entrypoint for the Gloamstead multi-agent collaboration system.

## Steps (execute in order)

1. Acquire lock:
   - Run `pwsh -NoProfile -File agent_collab/scripts/Acquire-OrchestratorLock.ps1 -Slug gloam`
   - If another active session holds a fresh lock → report owner and stop.
   - If stale → ask human for explicit takeover approval before proceeding; log decision.

2. Read key context (read-only):
   - agent_collab/context/project_goal.md
   - agent_collab/context/agent_rules.md
   - agent_collab/context/scope_roots.json + command_policy.json
   - agent_collab/registry/* (for enabled_runtimes only)
   - agent_collab/state/task_state.json + scheduler_state.json
   - agent_collab/state/leases.json (if async)
   - Last ~50 lines of agent_collab/logs/orchestrator.log + recent decisions.md

3. Reconcile (critical):
   - Compare state caches vs ground truth: git worktree list + branch status, handoffs/ (claimed/done/blocked), inbox/*/raw/ contents, active leases.
   - If divergence → rebuild task_state / scheduler_state from git + handoffs + raw inbox; append reconciliation record to decisions.md with reason.

4. Report concise status:
   - Mode (sync/async)
   - Current wave + in-flight / ready / blocked / completed counts
   - Any pending raw inbox items or expired leases
   - Lock heartbeat age
   - Next recommended action (continue wave, plan new DAG, reconcile specific task, release to human)

5. **Autonomous progress (policy-gated to reduce check-ins)**:
   - Call Update-RunState Tick/Check/Record as appropriate (via Bash).
   - Gate every mutation with Assert-ActionPolicy (ActionType e.g. "edit_code", "promote_candidate_local", "plan"). Only proceed on "allow".
   - Auto-process inbox, delegate safe ready tasks (low risk, policy reversible), run Critics, promote when green.
   - Halt for human on policy "ask", high risk, blocks, new planning needed, or budget stop.
   - Sync: multiple steps in one resume invocation. Async: lease + release, reconcile later.

## Async vs Sync
- This session is the Orchestrator. In sync mode you may spawn workers and receive results turn-by-turn.
- In async mode: issue requests, create leases, persist minimal state, release lock, exit. A later invocation of this command performs reconciliation of completed work.

## Safety
- Never bypass the lock.
- Never trust raw output from a can_return_schema=false runtime without Normalize + Validate.
- Work branch must remain green; only promote after verified integration Critic pass on candidate.
- Always honor Assert-ActionPolicy and single-writer (write only via the documented paths).

After safe autonomous work or explicit gate: report status (include run_state budget, queues) and ask only if required: "Autonomous segment complete/paused per policy. Ready for next (continue wave / plan / release)?" 
