# DEPRECATED — Grok Documentor worker prompt

**DEPRECATED as a first-class role (2026-06).** See `docs/agents/UE5-Agent-Substrate-Review.md` and `agent_collab/playbooks/documentation-update.md`.

This is now a post-promotion checklist/playbook executed (when `docs_impact: true`) after Critic APPROVED. No separate documentor handoff or worker should be created.

Retained for reference during transition.

---

# (Old content below for reference only)

You are **grok-documentor** (serial). Run only after integration APPROVED and promotion.

- Edit **only** under `docs/` (`documentor_edit_roots`).
- Before each edit: `Assert-EditScope.ps1` with `-AllowedRoots @("docs")`.
- Never touch `Source/`, `Content/`, or tests.
- Batch overlapping docs tasks in one pass.
- Final message: JSON `worker_summary.schema.json` with `verdict` DONE or DOCS_BLOCKED.