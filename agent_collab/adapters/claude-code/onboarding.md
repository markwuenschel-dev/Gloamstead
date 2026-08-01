# Claude Code Adapter Onboarding (Gloamstead)

## Setup
1. **Project this adapter** — required after every clone, and after any edit under
   `agent_collab/adapters/claude-code/`:
   `pwsh -NoProfile -File agent_collab/scripts/Project-ClaudeAdapter.ps1`
   then restart the Claude Code session so settings are re-read.

   This is a safety step, not a convenience one. `.claude/settings.json` is the only file that
   registers the `PreToolUse` Bash hook. Until it exists, `hooks/pre-bash-policy.ps1` is never
   invoked and **every Bash command runs with no shell-policy guard at all** — silently, with no
   error to tell you.
2. Run self-test: `pwsh -NoProfile -File agent_collab/scripts/Test-AgentCollabScaffold.ps1`
   (checks the tracked registration exists and that no projected file has drifted from source).
3. Ensure .claude/worktrees/ is ignored (already in .gitignore).

## Starting as Orchestrator
`claude --agent gloam-orchestrator`

Or inside session: `/gloam-resume`

The first message must perform lease acquisition + full reconciliation + status report.

See agent_collab/context/restart_instructions.md for the exact phrase and steps.

Native files under .claude/ (agents, commands, hooks, settings) are the **generated projection** for
this runtime, reproduced byte-for-byte from `agent_collab/adapters/claude-code/` by
`Project-ClaudeAdapter.ps1`. They are gitignored (VCS policy, 2026-07-29): the source of truth is
the adapter directory, and a clean clone recreates the projection rather than carrying it.

Consequences worth knowing:
- **Never edit `.claude/` directly.** Projection overwrites it, and because it is gitignored your
  edit is not recoverable from git. Edit the adapter source, then re-project.
- `.claude/hooks/*.ps1` are copies that **nothing loads.** The registration in
  `settings.json` points at `${CLAUDE_PROJECT_DIR}/agent_collab/adapters/claude-code/hooks/...`,
  i.e. the tracked source. Editing the copy under `.claude/hooks/` changes nothing at runtime; the
  self-test compares the two and fails on drift so this cannot mislead silently.
