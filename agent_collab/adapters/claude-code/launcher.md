# Claude Code Adapter Launcher (Gloamstead)

## Primary Entry (Orchestrator)
In a Claude Code session (claude or claude-code CLI):

```
claude --agent gloam-orchestrator
# or inside an active session, use the slash command:
/gloam-resume
```

The main session acts as Orchestrator. It uses subagents (via Task tool or native subagent spawning) for the worker templates defined in agents/.

## Subagent Invocation (from Orchestrator prompts)
When the Orchestrator needs a worker:

- Use the registered subagent names: gloam-planner, gloam-coder, gloam-critic, gloam-documentor, gloam-researcher, gloam-architect.
- Pass the worker_request (as prompt context or structured input).
- Instruct the subagent to emit its final result as JSON matching the appropriate output schema (worker_summary, critic_verdict, planner_output, etc.).
- Because can_return_schema=true for claude-code, the Orchestrator can often parse the structured output directly without raw inbox round-trip.

## Worktree Usage for Coders
- Orchestrator creates a git worktree for each Coder instance (under .claude/worktrees/ or standard git worktree dir).
- baseRef: "head" (from settings) ensures the worktree sees the latest local commits on the work branch.
- Coder works exclusively inside its worktree on its task branch.
- After result, Orchestrator (or cleanup) can remove the worktree.

## Scope Enforcement
The agent definitions for gloam-coder etc. instruct the model to call Assert-EditScope.ps1 before writes. The .claude/settings.json permissions.deny and hooks provide additional signals.

## Projection
This adapter folder is the source of truth. Run the projection step (or manually copy/sync) to populate .claude/agents/, .claude/commands/, .claude/settings.json before starting a session that relies on them.

See onboarding.md for first-time setup.
