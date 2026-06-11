# Claude Code Adapter Onboarding (Gloamstead)

## Setup
1. Run self-test: `pwsh -NoProfile -File agent_collab/scripts/Test-AgentCollabScaffold.ps1`
2. Ensure .claude/worktrees/ is ignored (already in .gitignore).

## Starting as Orchestrator
`claude --agent gloam-orchestrator`

Or inside session: `/gloam-resume`

The first message must perform lease acquisition + full reconciliation + status report.

See agent_collab/context/restart_instructions.md for the exact phrase and steps.

Native files under .claude/ (agents, commands, hooks, settings) are the projection for this runtime. They are maintained to match the registry.
