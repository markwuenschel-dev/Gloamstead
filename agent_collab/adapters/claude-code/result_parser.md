# Result Parser for Claude Code

Claude Code (via native agents/subagents) can be instructed to emit the final result as JSON matching the required output schema (worker_summary, critic_verdict, etc.).

The Invoke-AgentRuntime or Orchestrator captures the structured portion.

Raw conversation is saved to runtime_raw/ for audit.
