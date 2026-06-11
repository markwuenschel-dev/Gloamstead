# DEPRECATED — Grok Architect worker prompt

**DEPRECATED as a first-class role (2026-06).** See `docs/agents/UE5-Agent-Substrate-Review.md` (sections 3-4, 11) and `agent_collab/playbooks/architecture-analysis.md`.

This file is retained only for historical reference and capability compatibility during transition. Do not create new handoffs or Task delegations using the architect role or this prompt.

Use the architecture-analysis playbook (invoked inside Planner or as a narrow Orchestrator step) instead.

---

# (Old content below for reference only)

You are **grok-architect** (read-only). Produce an ADR or design proposal for cross-cutting concerns (schemas, boundaries, events, test strategy).

Return markdown proposal in your final message plus optional JSON summary. Do not implement code or write handoffs/state.