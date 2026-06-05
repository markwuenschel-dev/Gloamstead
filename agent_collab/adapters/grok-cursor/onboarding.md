# Grok-in-Cursor Adapter Onboarding

## One-time setup

1. PowerShell 7 (`pwsh`) on PATH.
2. Self-test (all three runtimes):
   ```powershell
   pwsh -NoProfile -File agent_collab/scripts/Test-AgentCollabScaffold.ps1
   ```
3. Project Grok adapter:
   ```powershell
   pwsh -NoProfile -File agent_collab/scripts/Project-GrokAdapter.ps1
   ```
4. Confirm `.grok/worktrees/` is in `.gitignore`.

## Starting as Orchestrator

- Open this repo in **Cursor with Grok**.
- Run **`/gloam-resume`** or ask: "Resume Gloamstead orchestrator".
- Grok discovers skills from `<repo>/.grok/skills/` (projected from this adapter).

## Worker sandboxes

Grok has no native worktree UI like Claude Code. Coders use **git worktrees** created by the Orchestrator:

```powershell
pwsh -NoProfile -File agent_collab/scripts/New-TaskWorktree.ps1 -TaskId TASK-0001
```

Work happens under `.grok/worktrees/<task_id>/` on branch `agent-collab/gloam/task/<task_id>`.

Before each edit, workers run:

```powershell
pwsh -NoProfile -File agent_collab/scripts/Assert-EditScope.ps1 -TargetFile <path> -AllowedRoots @("Source")
```

## Routing preference

With `grok-cursor` enabled, `registry/routing.json` prefers **grok-cursor** over claude-code when both satisfy the safety floor. Use Claude explicitly by logging a routing override in `decisions.md` when needed.

## Do not

- Edit `.grok/skills/` directly (projection target — edit `adapters/grok-cursor/skills/` then re-run `Project-GrokAdapter.ps1`).
- Let subagents write `handoffs/`, `state/`, or `outbox/`.
- Skip integration Critic before promoting to `agent-collab/gloam/work`.