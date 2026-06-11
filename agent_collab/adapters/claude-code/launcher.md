# Claude Code Launcher (Gloamstead)

Primary entry for Orchestrator:
```
claude --agent gloam-orchestrator
# or inside session:
/gloam-resume
```

Workers are invoked via the Task tool using the corresponding gloam-<role> subagent.

Worktrees: .claude/worktrees/<task_id>

See adapters/claude-code/role_prompts/ for the prompts used in the native projection.
