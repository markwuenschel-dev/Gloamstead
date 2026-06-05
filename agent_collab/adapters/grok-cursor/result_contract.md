# grok-cursor Result Contract

## Primary path (`can_return_schema: true`)

Workers return a **final message** containing valid JSON matching the role schema:

| Role | Schema |
|------|--------|
| Coder, Documentor, Researcher | `protocol/worker_summary.schema.json` |
| Critic | `protocol/critic_verdict.schema.json` |
| Planner | `protocol/planner_output.schema.json` |
| Architect | ADR/proposal markdown + optional JSON artifact path in summary |

The Orchestrator parses JSON directly from the Task subagent return value. **No inbox round-trip required.**

## Fallback path (out-of-band / truncated sessions)

If a worker cannot finish in-session, write **only** raw text to:

```
agent_collab/inbox/grok-cursor/raw/<request_id>.json
```

Shape (informal, normalized later):

```json
{
  "request_id": "...",
  "task_id": "...",
  "role": "Coder",
  "verdict": "DONE|BLOCKED|...",
  "summary": "...",
  "body": "full subagent transcript or last message"
}
```

Then: `Normalize-RuntimeResult.ps1` → `Validate-JsonSchema.ps1` → Orchestrator state transition.

## Forbidden

Workers and subagents must **never** write: `state/`, `handoffs/`, `outbox/`, `logs/decisions.md`, or any normalized protocol file.