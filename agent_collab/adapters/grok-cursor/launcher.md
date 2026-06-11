# Grok-in-Cursor Adapter Launcher

## Orchestrator (you — main Grok session)

1. Project adapter (once per session or after adapter edits — this now also projects agents/ prompts):
   ```powershell
   pwsh -NoProfile -File agent_collab/scripts/Project-GrokAdapter.ps1
   ```
2. Invoke skill **`/gloam-resume`** (or say "resume Gloamstead orchestrator").
3. Hold the lock; reconcile; report status; wait for human directive.

**Runtime template:** `grok-orchestrator` (this session). You are the single writer of durable state.

## Delegate a worker (sync mode)

1. Run routing:
   ```powershell
   pwsh -NoProfile -File agent_collab/scripts/Select-Runtime.ps1 -RequiredCapabilities edit_code -Role Coder -SafetyFloor edit_code
   ```
2. For **Coder** on `grok-cursor`, create worktree first:
   ```powershell
   pwsh -NoProfile -File agent_collab/scripts/New-TaskWorktree.ps1 -TaskId <TASK_ID>
   ```
3. Write `worker_request` + handoff under `agent_collab/handoffs/claimed/` (Orchestrator only).
4. Spawn subagent via **Task** tool:
   - Read prompt body from `agent_collab/adapters/grok-cursor/agents/grok-<role>.md`
   - Pass handoff path, `file_ownership`, `allowed_roots`, acceptance criteria
   - Require final JSON per `result_contract.md`
5. Parse result → validate schema → write `outbox/<role>/` + update `task_state.json` + log `decisions.md`.

### Task tool mapping (minimal roster)

| Role | Suggested `subagent_type` |
|------|---------------------------|
| Coder | `generalPurpose` |
| Planner | `generalPurpose` |
| Critic (integration) | `code-reviewer` |

Only the four active roles (orchestrator, planner, coder, critic) per `docs/agents/UE5-Agent-Substrate-Review.md`. Architecture, research, and documentation concerns are handled via playbooks/ instead of separate roles.

## Coexistence with Claude Code

- **Claude Code:** `claude --agent gloam-orchestrator` + `.claude/` projection
- **Grok (Cursor):** `/gloam-resume` + `.grok/` projection
- Protocol, state, and branches are **shared**. Only one Orchestrator session should hold the lock at a time.

## Raw inbox (fallback only)

```powershell
pwsh -NoProfile -File agent_collab/scripts/Write-InboxRaw.ps1 -Runtime grok-cursor -RequestId <id> -ContentPath <file>
```