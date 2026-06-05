# Claude Code Adapter Onboarding (Gloamstead)

## One-time Setup

1. Ensure PowerShell 7 (pwsh) is available (required for all guard + test scripts).
2. From repo root:
   - `pwsh -NoProfile -File agent_collab/scripts/Test-AgentCollabScaffold.ps1`
     - Must pass cleanly before first real task.
3. Project the adapter into `.claude/`:
   - `pwsh -NoProfile -File agent_collab/scripts/Project-ClaudeAdapter.ps1`
4. Add `.claude/worktrees/` to .gitignore (already handled by scaffold).
5. (Optional but recommended) Review and customize:
   - agent_collab/context/scope_roots.json (add any project-specific forbidden or allowed roots)
   - agent_collab/context/command_policy.json

## Starting the Orchestrator
- Preferred: `claude --agent gloam-orchestrator` (if your Claude Code supports agent profiles)
- Or: open Claude Code in the repo and immediately run `/gloam-resume`
- The first message should perform lock acquisition + full reconciliation and report status.

## First Real Task (after scaffold self-test passes)
- Human provides a small, self-contained objective (e.g. "Add a new ritual type definition following Phase 0 patterns").
- Orchestrator (with Planner help) produces a 1-3 task DAG.
- Routes first task to appropriate Coder (claude-coder on claude-code runtime).
- Coder runs in worktree, scope-guarded.
- On completion, Orchestrator builds candidate, runs integration Critic (claude-critic), promotes only on APPROVED.
- Documentor runs serially only for docs_impact tasks after promotion.

## Adding a Second Runtime (later)
- Do NOT create adapters/<new-runtime>/ yet.
- Follow agents/_EXTENSION_CONTRACT.md exactly when a concrete runtime (and need) exists.
- Only then: add to runtimes.json (with accurate capability flags), add template(s) to agents.json, implement the adapter, switch mode to async if required, exercise leases.

## Common Pitfalls to Avoid
- Treating .claude/ as source of truth (it is a projection).
- Allowing a watcher or adapter to write normalized handoffs/state (single-writer violation).
- Routing a Coder to a runtime without worktree + scope enforcement.
- Promoting a candidate without a trusted integration Critic pass.
- Parallelizing high-risk or overlapping-ownership tasks.

This system is deliberately conservative. Its value appears on the second or third real wave when restart, parallel safety, and audit become necessary.
