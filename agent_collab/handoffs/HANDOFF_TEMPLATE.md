# Handoff Template (copy and fill for each delegation)

**handoff_id**: <uuid or slug-timestamp>
**task_id**: <e.g. TASK-0001>
**role**: Orchestrator | Architect | Planner | Researcher | Coder | Critic | Documentor
**from**: orchestrator (or previous role)
**to**: <template_id or instance_id or "human">
**created**: <ISO8601>
**status**: claimed | done | blocked | archived

## Input Artifacts
- (list of paths or handoff_ids this worker should read)

## Output Artifacts (filled on completion)
- (list of paths worker produced or referenced)

## Runtime Notes (Orchestrator fills on routing + after result)
- chosen_runtime: 
- template_id:
- instance_id:
- reason: (full routing decision + safety floor check)
- lease_id (async only):

## Worker Result Summary (normalized)
- verdict: DONE | APPROVED | REJECTED | BLOCKED | DOCS_BLOCKED
- summary: (concise)
- changed_files: []
- branch:
- worktree_path:
- base_commit:
- head_commit:
- risks: []
- needs: (if BLOCKED)
- blocker:

## Next Action (Orchestrator)
- (what the Orchestrator decided after receiving this result)

---

**Invariant**: This file (and all files under handoffs/) is written ONLY by the Orchestrator. Workers and adapters write raw results exclusively to inbox/<runtime>/raw/.
