# Gloamstead — Agent Instructions

Follow `docs/agents/ProjectRules.md` for game architecture and UE5 conventions.

**Agent Collaboration Substrate**: See `docs/agents/UE5-Agent-Substrate-Review.md` (the authoritative diagnosis and minimal roster for this UE5 project). The living protocol source of truth is `agent_collab/`. 

Minimal active roles: orchestrator, planner, coder, critic. Architect / researcher / documentor have been converted to playbooks (agent_collab/playbooks/) + checklists per the review. Use `workflow_activation.json` to decide when the full set is justified. Small tasks must not pull unnecessary roles.

After clone or adapter changes: `pwsh -NoProfile -File agent_collab/scripts/Project-GrokAdapter.ps1`
Start orchestrator: **`/gloam-resume`**
Status only: **`/gloam-status`**

## Git / PR / merge workflow (ALL agents)

Applies to every agent and runtime (Claude Code, Grok, Codex, etc.). This is the GitHub
integration flow to `main`; it complements — does not replace — the agent_collab promotion rules below.

1. **Branch first — never commit directly to `main`.** One branch per logical change
   (`feat/…`, `test/…`, `chore/…`, `docs/…`).
2. **Green before PR.** `./gate.ps1` must pass (build green + all automation tests green) before opening a
   PR. Do not open a PR on red. New tests live under `Source/Gloamstead/Tests/` and run via the
   `Gloamstead` filter automatically.
3. **Stage only intended paths.** Never commit local tooling dirs (`.claude/`, `.grok/`) or build
   intermediates. Use explicit `git add <paths>`, not `git add -A`.
4. **Push auth (HTTPS remote).** `origin` is HTTPS; configure `gh` as the git credential helper once
   (`gh auth setup-git`). After pushing, **verify the ref actually landed**
   (`git ls-remote origin <branch>`) — push stdout can falsely look successful.
5. **Open the PR** with `gh pr create --base main` and a body describing what changed + how it was verified.
6. **Merge policy (human standing order):** squash-merge and delete the branch:
   `gh pr merge <n> --squash --delete-branch`, then `git checkout main && git pull --prune` and delete the
   local branch (`git branch -d <branch>`).
7. **Never bypass protections.** Do **not** pass `--admin` to override a branch-protection / required-check
   gate. If a gate blocks the merge, **stop and report to the human**; use `--admin` only when the human
   explicitly authorizes it for that specific merge.
8. **Attribution.** End commit messages with a `Co-Authored-By:` trailer naming the acting agent/model; end
   PR bodies with the agent's "generated with" line.
9. **Line endings / LFS.** Repo blobs are LF; non-Windows clients must set `core.autocrlf=input` and have
   git-lfs installed, or `Content/*` and sources will show phantom churn.

## Multi-agent collaboration (agent_collab)

This repo uses the **agent_collab** protocol (v8.1). Source of truth: `agent_collab/` (not `.grok/` or `.claude/`).

### Grok in Cursor (this session)

1. After clone or adapter changes: `pwsh -NoProfile -File agent_collab/scripts/Project-GrokAdapter.ps1`
2. Start orchestrator: **`/gloam-resume`**
3. Status only: **`/gloam-status`**

You may act as Orchestrator on runtime **grok-cursor**. Acquire the lock before state writes. Delegate workers via the Task tool using prompts in `agent_collab/adapters/grok-cursor/agents/`.

### Claude Code

`claude --agent gloam-orchestrator` or `/gloam-resume` with `.claude/` projection (`Project-ClaudeAdapter.ps1`).

### Shared rules

- Only Orchestrator writes `agent_collab/state/`, `handoffs/`, `outbox/`, `logs/decisions.md`.
- Workers write **only** to `inbox/<runtime>/raw/` when not returning schema inline.
- Promote to `agent-collab/gloam/work` only after integration Critic APPROVED.

Details: `agent_collab/context/agent_rules.md`, `agent_collab/adapters/grok-cursor/onboarding.md`.