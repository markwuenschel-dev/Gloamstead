# Local-Script Runtime Launcher (Gloamstead)

## Purpose
Execute simple, bounded, non-editing work (repo queries, test invocations, log collection) via direct shell invocation. Used when a task's required_capabilities can be satisfied by local execution without a full LLM agent.

## Invocation (from Orchestrator)
The Orchestrator (or a thin wrapper) runs the configured command in the runner_config, captures all output, and drops a single raw artifact into:

`agent_collab/inbox/local-script/raw/<request_id>-<timestamp>.raw.json` (or .txt)

Example raw payload shape (illustrative):
```json
{
  "request_id": "...",
  "command": "pwsh -File agent_collab/scripts/Validate-JsonSchema.ps1 ...",
  "stdout": "...",
  "stderr": "...",
  "exit_code": 0,
  "started": "...",
  "finished": "..."
}
```

## Normalization Requirement
Because `can_return_schema: false`, the Orchestrator MUST:
1. Locate the raw file(s) for the request.
2. Invoke `Normalize-RuntimeResult.ps1 -Runtime local-script -RawPath <file> -OutPath <normalized>`
3. Validate the normalized file against worker_summary.schema.json (or appropriate schema).
4. Only then may the result drive state transitions or handoff updates.

## Limitations (Safety)
- No file edits (enforced by policy + scope if attempted).
- No worktree management.
- Cannot be selected for any role requiring edit_code or final integration Critic.
- Launch mode "runner" — synchronous within the Orchestrator session for sync mode.

## When to Prefer Over claude-code
- Pure mechanical work ("run the full test suite and return the summary log").
- Deterministic transforms or data extraction where an LLM is unnecessary overhead.

See _EXTENSION_CONTRACT.md for the contract any new runtime adapter must satisfy.
