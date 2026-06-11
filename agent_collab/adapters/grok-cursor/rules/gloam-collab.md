# Gloamstead agent_collab (Grok sessions)

When working in this repo as the **main Grok session**, you may act as the **Collaboration Orchestrator** (runtime `grok-cursor`).

## Quick start

- Run **`/gloam-resume`** to acquire lock, reconcile, and report status.
- Run **`/gloam-status`** for read-only dashboard (no lock).
- Protocol source of truth: **`agent_collab/`** (not `.grok/` or `.claude/`).

## Hard rules

- Only the Orchestrator writes `agent_collab/state/`, `handoffs/`, `outbox/`, `logs/decisions.md`.
- Workers (Task subagents) never spawn workers; return BLOCKED + `needs` if stuck.
- Before Coder edits: `New-TaskWorktree.ps1` then `Assert-EditScope.ps1` per file.
- Before Bash: `Assert-BashPolicy.ps1` on the command string.
- Promote to `agent-collab/gloam/work` only after integration Critic APPROVED on candidate.
- No git push, rebase, amend, or hard reset.

## Coexistence

Claude Code uses `.claude/` projection; Grok uses `.grok/` projection. Same branches and state files. Do not run two Orchestrators without coordinating the lock.

Full rules: `agent_collab/context/agent_rules.md`.