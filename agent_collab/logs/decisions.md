# Gloamstead Collaboration Decisions Log

All routing decisions, reconciliations, lock takeovers, schema validation failures, and scope/command policy violations are recorded here with timestamp, actor (human or orchestrator session), and rationale. This is the human-auditable companion to orchestrator.log.

## 2026-05-09 — Scaffold Creation
- Initialized full agent_collab/ structure per Collaboration Architect v8.1 spec.
- Slug chosen: "gloam" (short, distinctive prefix of Gloamstead).
- Discovered roots: source=["Source"], test=[], docs=["docs"].
- Enabled runtimes: claude-code (primary trusted), local-script (bounded runner only).
- max_parallel=4, work_branch="agent-collab/gloam/work".
- Claude-only profile is exactly the claude-code templates in agents.json.
- Self-test (Test-AgentCollabScaffold.ps1) will be run and must pass before any application task.
- Single-writer invariant, inbox/raw boundary, and sync mode default documented and enforced in design.

No application work has begun. This is pure infrastructure.

## 2026-05-31 — Cold Resume (/gloam-resume)
- Lock acquired cleanly (new session).
- Full reconciliation performed.
- Ground truth: no agent-collab/gloam branches, no worktrees, no handoffs (only template), inbox/*/raw empty, outbox empty, leases empty.
- State caches (task_state + scheduler_state) match ground truth exactly (empty active/queues/waves).
- Mode: sync (as declared).
- .gitignore healthy for worktrees/.
- Untracked: entire agent_collab/ + .claude/ (post-scaffold; will be committed when first wave produces artifacts or per human direction).
- No stale data, no divergences, no expired leases.
- Self-test previously passed. System is cold-restart safe and ready.
- No application work has started.

Next: Awaiting human directive.

## 2026-05-31 — Phase 2 Planning Complete (via design skill)
- Full design document produced and reviewed to 0 open issues.
- Design ID: 5ca77255
- Primary artifact: docs/CollaborationSystem-Phase2-Design.md (copied from /tmp/grok-design-doc-5ca77255.md)
- Key outcome: Detailed 7-PR incremental plan for exactly one async runtime ("paste-watcher" as lowest-risk first exerciser) + full lease/heartbeat/reconcile machinery, while strictly enforcing the Hard Gate (one real non-trivial Claude-only sync wave exercising Coder/Critic + integration verification + restart *before* any adapter creation or mode flip, per _EXTENSION_CONTRACT.md precondition #2).
- All 17 issues across rounds addressed; final reviewer confirmation: 0 open issues.
- Design is implementation-ready and fully compliant with v8.1 hard rules + single-writer invariant.
- Next required step (per design): Execute the mandatory Hard Gate real non-trivial sync task before authoring any Phase 2 PRs.

Recorded by Orchestrator during /gloam-resume session.

## 2026-05-31 — Phase 2 Implementation Started: Hard Gate Execution
- Per approved Phase 2 design (docs/CollaborationSystem-Phase2-Design.md, design ID 5ca77255).
- **Hard Gate (per _EXTENSION_CONTRACT.md precondition #2)**: Executing one real non-trivial Claude-only (sync) wave before any async adapter or mode change.
- **Chosen Hard Gate Task** (low-risk, exercises full flow):
  Goal: Add `agent_collab/scripts/Get-CollabStatus.ps1` — a new PowerShell script that outputs current collaboration status (mode, lock, queues, recent decisions, git collab branches, inbox/handoff counts). Useful ongoing infrastructure. Involves Coder (new file under scripts/), scope guard enforcement, Critic review (correctness + basic execution), handoff lifecycle, candidate branch, integration verification, potential promotion, and restart/reconcile.
- This task has `docs_impact: false` (pure script), `risk_level: low`, `parallelizable: false`.
- Will use only `claude-code` runtime (satisfies all safety floors).
- Full audit trail will be recorded. No Phase 2 async changes (no paste-watcher, no mode flip) will occur until this gate completes successfully and is recorded.
- Next: Issue Planner handoff for task breakdown.

## 2026-05-31 — Hard Gate COMPLETE (Phase 2 Prerequisite Met)
- Full end-to-end sync wave executed successfully:
  - Planner produced DAG (1 task, low risk, accurate file ownership).
  - Coder implemented Get-CollabStatus.ps1 in isolated worktree on task branch.
  - Scope guard respected.
  - Candidate branch built by merging task branch.
  - Integration Critic ran the script on candidate → PASS (clean output, no mutations, correct sections).
  - Promoted to protected work branch (agent-collab/gloam/work) because green.
- New useful script committed: agent_collab/scripts/Get-CollabStatus.ps1
- This satisfies _EXTENSION_CONTRACT.md precondition #2 and the Phase 2 design Hard Gate.
- **No Phase 2 async changes have been made** (no paste-watcher adapter, mode remains "sync").
- System is now eligible to proceed with Phase 2 infrastructure PRs if desired.

Recorded by Orchestrator after successful restart/reconcile simulation.

## 2026-06-01 — v8.1 Scaffold Completion
- Added `gloam-orchestrator` agent profile and `claude --agent gloam-orchestrator` entrypoint.
- Implemented PreToolUse Edit/Write scope hooks (`pre-edit-scope.ps1`) and Bash policy hook (`pre-bash-policy.ps1`) in adapter settings + per-agent frontmatter for Coder/Documentor.
- Added `Project-ClaudeAdapter.ps1`, `Refresh-OrchestratorLock.ps1`, `Select-Runtime.ps1`.
- Reconciled `task_state.json`: HG-001 moved to history (promoted), active cleared.
- Self-test extended for projection, routing, and orchestrator agent.

## 2026-06-01 — grok-cursor Runtime Adapter
- Added concrete **grok-cursor** runtime for Grok CLI in Cursor (per _EXTENSION_CONTRACT.md).
- Enabled in `runtimes.json` with full sync capabilities; worktrees via `New-TaskWorktree.ps1` under `.grok/worktrees/`.
- Templates: grok-orchestrator, grok-planner, grok-coder, grok-critic, grok-documentor, grok-researcher, grok-architect.
- Adapter: `adapters/grok-cursor/` (skills, rules, agent prompts, launcher, result contract).
- Projection: `Project-GrokAdapter.ps1` → `.grok/skills/`, `.grok/rules/`.
- Routing preference: grok-cursor > claude-code > local-script when both satisfy safety floor.
- Root `AGENTS.md` points Grok sessions at `/gloam-resume`.
- Coexistence: shared protocol/state/branches; single orchestrator.lock across IDEs.

## 2026-06-01 — Wave plan-wave-nc-1 (subagent delegation)
- Orchestrator (Grok) acquired lock; spawned read-only workers: grok-planner, grok-researcher, grok-architect via Task tool.
- Planner output: NC-001 (data), NC-002 (PCG aggregates), NC-003 (manager selection); persisted to outbox/planner/plan-wave-nc-1.json.
- Researcher + Architect outputs persisted; wave-nc-1 status=planned.
- Coders/Critic NOT spawned pending human approval (application code, worktrees, integration gate).

## 2026-06-01 — wave-nc-1 APPROVED and PROMOTED (Grok orchestrator)
- Human approved wave-nc-1; lock takeover from prior session logged.
- Worktrees: New-TaskWorktree.ps1 -Slug gloam -WorktreeRoot .grok/worktrees for NC-001/002/003.
- Spawned grok-coder Task subagents: NC-001, NC-002 (parallel), NC-003 (after merges).
- Candidate branch agent-collab/gloam/candidate/wave-nc-1 built; integration Critic APPROVED (orchestrator static review + outbox/critic/wave-nc-1-verdict.json).
- grok-documentor updated docs/systems/03_night_consequence_system.md on candidate.
- Promoted to agent-collab/gloam/work (fast-forward). Wave status=promoted.

## 2026-06-02 — agent_collab Full Fill-Out + High Autonomy Enablement
- Ran Project-*Adapter.ps1 for both runtimes.
- Fixed .gitignore: removed blanket agent_collab/ and AGENTS.md ignores; now only volatile (raw inbox, live lock, orchestrator.log, projections). Core protocol, schemas, scripts, context (incl new autonomy_policy.json, project.json), registry, handoffs, outbox, state caches, grok/claude adapters now tracked and portable post-clone. Updated test regex to match.
- Added missing grok-orchestrator.md agent prompt (grok-cursor/agents/) for symmetry and self-reference in autonomy flows.
- Added context/autonomy_policy.json (level 2, budgets, reversible/ask/never lists, project overrides for vertical slice).
- Added context/project.json (slug etc).
- Rebuilt scheduler_state.json to exact schema (waves[] from all historical, empty queues, sync mode, max_parallel=4, work_branch set); backed up old.
- Initialized run_state.json via Update-RunState Init (new run for this autonomy session); projected status.json.
- Cleaned stale claimed handoffs (NC-001/002/003 from wave-nc-1) → done/, completed their md with results/next from outbox summaries.
- Enhanced autonomy to eliminate constant check-ins:
  - Updated restart_instructions.md (source of truth): policy bootstrap, Assert-ActionPolicy gates, auto-advance safe reversible, only halt on ask/deny/high/block/budget/new-plan.
  - Updated gloam-resume/SKILL.md (grok), gloam-status, claude resume/status/start-task, claude gloam-orchestrator.md to drive the loop: Tick+Check+Record, policy gate before every mutation, auto delegate ready low-risk, surface only when required.
  - Enhanced Get-CollabStatus.ps1 to emit run_state + autonomy level.
  - Extended Test-AgentCollabScaffold.ps1 requiredFiles + relaxed gitignore assertion + now requires new autonomy artifacts.
- All changes exercised: Test-AgentCollabScaffold.ps1 now **ALL TESTS PASSED** (including new requireds, schema, routing, projections, guards, lock, gitignore rules).
- System now supports true autonomy: Orchestrator (on /gloam-resume) acquires, reconciles, then auto-executes sequences of safe reversible actions (edits in worktree, local commits, low-risk research/plan/critic) using deterministic policy instead of per-step human ok; checkpoints only at natural gates.
- No new registry changes needed (grok-orchestrator template already present). grok preference remains for low-risk.
- Next: On resume, expect significantly fewer inputs; system drives waves to completion autonomously within policy.

Recorded by Orchestrator (grok-cursor) during agent_collab completion task.
