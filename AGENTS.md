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

**Standing order — agents own the full lifecycle.** For a change you are authorized to make, take
it all the way without waiting for a human to click buttons: **commit → push → open PR → squash-merge →
delete the branch (local + remote) → prune**. The human has pre-authorized the squash-merge-and-delete
policy (step 6). The only hard stops are: (a) `./gate.ps1` is red, (b) a branch-protection / required-check
gate would need `--admin` (step 7), or (c) a conflict you cannot cleanly resolve — in those cases **stop and
report**. Doc/config-only changes with no build impact (e.g. Markdown, `.gitignore`) do not require a UE5
build; say so in the PR body instead of skipping silently.

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

> **If `gh` is not installed** (e.g. this WSL runtime), the steps above still work via the GitHub REST API
> with the env token — the git credential helper already injects `${GH_TOKEN:-$GITHUB_TOKEN}` for HTTPS
> `git push`. Substitute:
> - Create PR → `POST /repos/{owner}/{repo}/pulls` with `{"title","head","base","body"}`
> - Merge (squash) → `PUT /repos/{owner}/{repo}/pulls/{n}/merge` with `{"merge_method":"squash"}`
> - Delete remote branch → `DELETE /repos/{owner}/{repo}/git/refs/heads/{branch}`
>
> e.g. `set -a; . ./.env; set +a; curl -sS -H "Authorization: Bearer $GH_TOKEN" -H "Accept: application/vnd.github+json" https://api.github.com/...`
> — never put the token on a logged command line or echo it.

### Stale-branch cleanup (after every merge, and periodically)

"Own the lifecycle" includes leaving no orphan branches.

- **Remote:** merging with `--delete-branch` (or the REST `DELETE …/git/refs/heads/<branch>`) removes the PR
  branch. To sweep others, list remote branches already merged into `main`
  (`git branch -r --merged origin/main`) and delete the ones you own — **never** `main` or `HEAD`.
- **Local:** `git checkout main && git pull --prune` (drops local refs to remote branches that are now gone),
  then delete merged locals:
  `git branch --merged main | grep -vE '^\*|^\s*main$' | xargs -r git branch -d`.
- **Safety:** use `git branch -d` (refuses unmerged); only `-D` (force) an unmerged branch when the human
  explicitly says so.

### Providing GitHub credentials (PAT)

Auth must never block a push / PR / merge. `gh` is the credential path for this repo (HTTPS `origin`).
A human can supply a Personal Access Token so any agent can push **without the token ever entering the repo**:

- **Current setup — `.env` at repo root:** `GH_TOKEN=…` (and/or `GITHUB_TOKEN=…`) live in `.env`, which is
  **git-ignored and must stay that way**. It is not auto-exported into the shell, so agents load it per command
  (`set -a; . ./.env; set +a; <cmd>`). The repo's git credential helper reads `${GH_TOKEN:-$GITHUB_TOKEN}`, so
  HTTPS `git push` authenticates with no token in the repo. `gh` (when installed) also reads `GH_TOKEN`
  automatically.
- **Or — gh keyring:** `gh auth login --with-token` (token on stdin), then `gh auth setup-git`.
  Stored in the OS keyring, persists across sessions, and git push over HTTPS uses it.
- **Or — environment variable in the profile:** export `GH_TOKEN` (or `GITHUB_TOKEN`) in the profile the
  agent's commands inherit (`~/.bashrc`, Windows user env). gh and the git credential helper read it
  automatically. A one-off `export` in an interactive prompt may **not** carry into an agent's separate tool
  calls — set it in the profile (or use `.env` above) so it persists.

**Hand the token over out-of-band** — your own terminal, profile, or keyring — **not by pasting it into the
chat/transcript** (anything in the conversation is retained, and so is any command line that echoes it).

Token scope — keep it minimal: classic PAT `repo` (add `workflow` only if touching Actions), or a
fine-grained PAT scoped to this repo with **Contents + Pull requests: read/write**. SSO-authorize if the org requires it.

**NEVER:** commit a token or write it into any tracked file, commit message, or PR body; pass it on a
command line that gets logged; or stash it in `.claude/`/`.grok/` expecting privacy. Rotate/revoke when no
longer needed, and revoke immediately if one is ever exposed.

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