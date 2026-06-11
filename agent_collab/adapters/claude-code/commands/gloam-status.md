---
description: Quick status dump of the Gloamstead collaboration system (no mutations, safe to run anytime).
allowed-tools: Read, Grep, Glob, Bash
---

# gloam-status

Produce a concise dashboard of the current collaboration state.

## Output Format (markdown)

**Mode**: sync | async (from scheduler_state.json)

**Lock**: owner / heartbeat age / cwd (from state/orchestrator.lock or "none")

**Waves**:
- List recent waves with status, task count, candidate branch

**Queue**:
- In-flight: list task_ids + assigned template/instance
- Ready: count + examples
- Blocked: count + reasons (top 3)

**Recent Activity** (last 10 from orchestrator.log + decisions.md)

**Handoffs**:
- Claimed: count
- Done (unpromoted): count
- Blocked: count

**Inbox Raw**:
- grok-cursor/raw: unprocessed file count
- claude-code/raw: unprocessed file count
- local-script/raw: unprocessed file count

**Git**:
- Current branch
- Worktree count (git worktree list --porcelain)
- agent-collab/gloam/work status (clean/dirty/ahead)

**Recommendation**: one-sentence suggested next Orchestrator action (may be auto-continued on /gloam-resume per autonomy policy).

**Autonomy note**: Status includes run_state budget and policy level. Resume will auto-advance safe reversible work; this command never mutates.

Do not acquire the lock or mutate anything. Pure observation.
