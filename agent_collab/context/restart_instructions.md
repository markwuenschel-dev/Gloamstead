# Restart and Reconciliation Instructions (Runtime-Neutral Orchestrator)

## Short Restart Phrase (use on every cold/warm start, any compatible runtime)
"Acquire the exclusive Orchestrator lease (verifying this runtime supports the role and has required capabilities); read project_goal.md, agent_rules.md, scope_roots.json, command_policy.json, autonomy_policy.json, content_policy.json, unreal_project.json, verification_profiles.json, all registry/*.json, task_state.json + scheduler_state.json + leases.json + logs/ (orchestrator.log, decisions.md, audit.jsonl); reconcile against handoffs/*, outbox/* (incl playtest/ and runtime_raw/), git branches + worktrees + candidate branches + actual generated binary files + vendor immutability (ground truth); report current status; continue safe approved work or wait when no work is approved or a human playtest is pending."

Git, handoffs, outbox (incl playtest/ and runtime_raw/), leases, candidate branches, and actual on-disk generated files are ground truth. State JSON files are caches that may be rebuilt.

## Full Restart Instructions (runtime-neutral process)

1. Verify this runtime is enabled in registry/runtimes.json, supports the "orchestrator" role, and declares all required Orchestrator capabilities.
2. Acquire the exclusive Orchestrator lease:
   - Use Acquire-OrchestratorLease.ps1 (or equivalent for the runtime).
   - Provide runtime id, agent/session identifiers, etc.
   - If another non-expired lease exists for a compatible runtime, stop and report.
   - If stale/expired, acquire after reconciliation.
3. Read (read-only, in order):
   - All context/ files listed in the short phrase.
   - Registries (runtimes, roles, agents, capabilities, adapter_matrix, models).
   - Protocol schemas as needed.
   - State files (task_state, scheduler_state, leases).
   - Logs (orchestrator.log tail, decisions.md, audit.jsonl).
4. Reconcile against ground truth:
   - Git branches (agent-collab/<slug>/*), worktree list, HEADs, uncommitted changes in any worktree.
   - Handoffs (claimed/done/blocked/archived) vs task_state.
   - Outbox (all role dirs + integration + playtest + runtime_raw) for unconsumed results.
   - Actual on-disk generated files vs assigned generated_output_ownership in state/handoffs.
   - Vendor content immutability (no changes under vendor_content_roots).
   - Pending human playtest requirements.
   - Orphan task/candidate branches or worktrees not reflected in state.
   - If divergence: rebuild state caches from ground truth; append reconciliation record to decisions.md and audit.jsonl.
5. Report current status (include multi-runtime and UE5 specifics):
   - Current lease holder (runtime + identifiers)
   - Active waves, queues, blocked items
   - Any pending human playtests
   - Status of generated content vs ownership
   - Vendor immutability status
   - Availability of required verification profiles and runtimes
   - Budget remaining
6. Continue or wait:
   - Only on goals from human or approved backlog.
   - Respect all human gates and unavailable profiles.
   - For generated binary work: only with explicit handoff permission + ownership + verification path.
   - Halt for high risk, ambiguous ownership, no compatible runtime available, etc.

## Runtime-Specific Launch Notes
See the launcher.md and onboarding.md inside each runtime's adapter under agent_collab/adapters/<runtime-id>/ for how to invoke that runtime as Orchestrator (e.g. claude --agent <slug>-orchestrator or equivalent for grok).

The lease (not the runtime name) grants Orchestrator authority. Any compatible runtime may acquire it after release/expiry and continue.

Re-run full reconciliation on every activation before delegation or state mutation.
