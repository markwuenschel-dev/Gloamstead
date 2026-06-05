# Restart and Reconciliation Instructions (Orchestrator)

## On Every Orchestrator Invocation (including cold restart)

1. **Acquire the lock** (or fail with clear guidance):
   - Run `agent_collab/scripts/Acquire-OrchestratorLock.ps1 -Slug gloam`
   - If fresh lock held by another session → stop, report owner, do not proceed.
   - If stale lock → ask human explicitly before takeover (record in decisions.md).

2. **Read ground truth sources** (in order):
   - agent_collab/context/project_goal.md
   - agent_collab/context/agent_rules.md
   - agent_collab/context/environment.md
   - agent_collab/context/scope_roots.json
   - agent_collab/context/command_policy.json
   - Registries for **enabled runtimes only**:
     - agent_collab/registry/runtimes.json (filter enabled_runtimes)
     - agent_collab/registry/agents.json (only templates for enabled runtimes)
     - agent_collab/registry/capabilities.json
     - agent_collab/registry/routing.json
   - Protocol schemas (lazily, only those needed for current enabled runtimes)
   - agent_collab/state/task_state.json
   - agent_collab/state/scheduler_state.json
   - agent_collab/state/leases.json (if mode == "async")
   - Last 50 lines of agent_collab/logs/orchestrator.log
   - agent_collab/logs/decisions.md (recent entries)

3. **Reconcile against ground truth** (state files are caches):
   - Git: current branch, worktrees (git worktree list), status of task/candidate/work branches, uncommitted changes in any worktree.
   - Handoffs: claimed/, done/, blocked/, archived/ — cross-check task_ids, verdicts, branches.
   - inbox/<runtime>/raw/ — any unprocessed outputs from watchers/runners/human pastes.
   - Active leases (if async): validate heartbeats/expiry against wall time.
   - If task_state.json or scheduler_state.json disagree with ground truth → rebuild caches from git + handoffs + inbox + leases; append reconciliation diff + rationale to decisions.md.

4.5 **Autonomy bootstrap** (new in fill-out for reduced check-ins):
   - `Update-RunState.ps1 -Action Init -Force` (or Tick if continuing run).
   - Read autonomy_policy.json; use Assert-ActionPolicy for every state-changing step.
   - Rebuild status projection for observability.

4. **Report status** (concise) + autonomous continuation where safe:
   - Active mode, run_id + budget spend (from run_state), waves/queues from scheduler, lock, inbox/handoff counts.
   - Recommendation: (auto-continue safe work | plan next wave | reconcile X | release). Only pause for human if policy gate or no safe work remains.

5. **Autonomous progress (policy-gated) + human handoff only when required**:
   - Run Update-RunState Tick + Check (exit 10 on budget hit → stop).
   - Before mutations/delegations run Assert-ActionPolicy.ps1. If "allow" (reversible low/med risk) → proceed autonomously (no per-step check-in).
   - Auto-reconcile any inbox/raw, auto-delegate next safe ready_queue items (up to parallelism), auto-run Critic on candidates, auto-promote when green (if policy allows).
   - Stop and surface for human ONLY on: policy "ask"/"deny", high risk, new wave planning, budget exceed, BLOCKED tasks, or explicit "pause" directive.
   - Sync mode: stay in session for multiple autonomous steps within the resume.
   - Async: issue + lease, release lock, later resume reconciles.
   - Always log policy decisions + actions to decisions.md.

## Cold Restart Safety
- The system is designed so that no in-flight worker holds exclusive state.
- Leases (async) + heartbeats provide timeout-based recovery.
- Git branches + handoff files are the durable source of work product.
- Never assume a synchronous wait for any runtime that declared launch_mode other than "native" in the current session.

## After Reconciliation
- Refresh lock heartbeat if long operation follows.
- On clean exit or handoff to human: release the lock via Release-OrchestratorLock.ps1.
