---
name: gloam-status
description: Read-only Gloamstead collaboration status dashboard. No lock, no mutations. Use for /gloam-status or "gloam status".
metadata:
  short-description: Gloamstead collab status (read-only)
---

# gloam-status

**Read-only.** Do not acquire the lock or mutate state.

Run:

```powershell
pwsh -NoProfile -File agent_collab/scripts/Get-CollabStatus.ps1
```

Also report:

- **Mode** from `scheduler_state.json`
- **Inbox raw** counts for `grok-cursor`, `claude-code`, `local-script`
- **Waves** (recent status + candidate branches)
- **Recommendation** (one sentence)

Optional: `git branch --list '*gloam*'`, `git worktree list`.

**Autonomy**: Status is read-only. /gloam-resume will use policy to auto-advance safe work (Assert-ActionPolicy + run_state budgets). No check-in needed for low-risk reversible steps.