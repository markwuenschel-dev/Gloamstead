# Gloamstead — Agent Instructions

Follow `docs/agents/ProjectRules.md` for game architecture and UE5 conventions.

**Agent Collaboration Substrate**: See `docs/agents/UE5-Agent-Substrate-Review.md` (the authoritative diagnosis and minimal roster for this UE5 project). The living protocol source of truth is `agent_collab/`. 

Minimal active roles: orchestrator, planner, coder, critic. Architect / researcher / documentor have been converted to playbooks (agent_collab/playbooks/) + checklists per the review. Use `workflow_activation.json` to decide when the full set is justified. Small tasks must not pull unnecessary roles.

After clone or adapter changes: `pwsh -NoProfile -File agent_collab/scripts/Project-GrokAdapter.ps1`
Start orchestrator: **`/gloam-resume`**
Status only: **`/gloam-status`**

## Multi-agent collaboration (agent_collab)

This repo uses the **agent_collab** protocol (v8.1). Source of truth: `agent_collab/` (not `.grok/` or `.claude/`).

### Grok in Cursor (this session)

1. After clone or adapter changes: `pwsh -NoProfile -File agent_collab/scripts/Project-GrokAdapter.ps1`
2. Start orchestrator: **`/gloam-resume`**
3. Status only: **`/gloam-status`**

You may act as Orchestrator on runtime **grok-cursor**. Acquire the lock before state writes. Delegate workers via the Task tool using prompts in `agent_collab/adapters/grok-cursor/agents/`.

### Claude Code

`claude --agent gloam-orchestrator` or `/gloam-resume` with `.claude/` projection (`Project-ClaudeAdapter.ps1`).

### Shared rules

- Only Orchestrator writes `agent_collab/state/`, `handoffs/`, `outbox/`, `logs/decisions.md`.
- Workers write **only** to `inbox/<runtime>/raw/` when not returning schema inline.
- Promote to `agent-collab/gloam/work` only after integration Critic APPROVED.

Details: `agent_collab/context/agent_rules.md`, `agent_collab/adapters/grok-cursor/onboarding.md`.