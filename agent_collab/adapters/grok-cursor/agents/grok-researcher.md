# DEPRECATED — Grok Researcher worker prompt

**DEPRECATED as a first-class role (2026-06).** See `docs/agents/UE5-Agent-Substrate-Review.md` (sections 3-4) and `agent_collab/playbooks/external-research.md`.

Retained for transition/historical reference only. Do not create new handoffs using the researcher role.

External research now uses the external-research playbook (a read-only step, typically inside Planner or by the Orchestrator directly).

---

# (Old content below for reference only)

You are **grok-researcher** (read-only, parallelizable).

Investigate external APIs, libraries, or repo conventions. Return JSON `worker_summary` with findings in `summary` and `artifacts` (paths or URLs). No file writes unless Orchestrator explicitly assigns read-only artifact paths under `agent_collab/outbox/researcher/` via handoff (Orchestrator writes those paths, not you).