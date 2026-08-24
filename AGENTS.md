# Gloamstead — Agent Instructions

Follow `docs/agents/ProjectRules.md` for game architecture and UE5 conventions.

**Agent Collaboration Substrate**: See `docs/agents/UE5-Agent-Substrate-Review.md` (the authoritative diagnosis and minimal roster for this UE5 project). The living protocol source of truth is `agent_collab/`. 

Minimal active roles: orchestrator, planner, coder, critic. Architect / researcher / documentor have been converted to playbooks (agent_collab/playbooks/) + checklists per the review. Use `workflow_activation.json` to decide when the full set is justified. Small tasks must not pull unnecessary roles.

**After clone or adapter changes, run BOTH projections.** The adapter directories (`.claude/`,
`.grok/`) are generated and gitignored, so a fresh clone has neither until you project them. For
Claude Code this is not cosmetic: `.claude/settings.json` is the only file that registers the
`PreToolUse` Bash hook, so until it exists **every Bash command runs unguarded**.

```
pwsh -NoProfile -File agent_collab/scripts/Project-ClaudeAdapter.ps1
pwsh -NoProfile -File agent_collab/scripts/Project-GrokAdapter.ps1
```

Restart the Claude Code session afterwards so settings are re-read. `Test-AgentCollabScaffold.ps1`
verifies the tracked source carries the registration and that no projected copy has drifted.

Start orchestrator: **`/gloam-resume`**
Status only: **`/gloam-status`**

## Command permission standing order (ALL agents)

For this repo, agents are pre-authorized to run the ordinary shell commands needed to inspect,
build, test, ship, and clean up work without asking for extra human permission first. This includes
PowerShell, `pwsh`, bash, `cmd.exe`, git survey/branch/stage/commit/push/fetch commands, UE build and
automation commands, `gate.ps1`, project agent scripts under `agent_collab/scripts/`, and token-safe
GitHub REST calls that load credentials from the git-ignored `.env`.

Do not pause for confirmation just because a command is PowerShell/bash/ps1 or may write normal build
outputs under this workspace. If the runtime itself requires an approval prompt, request that approval
directly instead of asking in chat first.

This does **not** authorize unsafe shortcuts: never expose tokens, never bypass branch protection, never
use `--admin`, never destructively delete/reset user work, never close the user's editor/GUI session, and
never delete unmerged local or remote branches unless the human explicitly asks for that specific action.

## Git / PR / merge workflow (ALL agents)

Applies to every agent and runtime (Claude Code, Grok, Codex, etc.). This is the GitHub
integration flow to `main`; it complements — does not replace — the agent_collab promotion rules below.

> **ENFORCED REALITY (2026-08-24): remote git operations are BLOCKED for every agent.**
> `agent_collab/context/command_policy.json` blocks the pattern
> `git\s+(push|pull|fetch\s+origin|remote|pr)\b` with the reason *"No remote mutations or PRs from any
> agent. Orchestrator coordinates branches locally only."* The pre-bash policy hook enforces this at the
> tool-call layer — it refuses even a read-only `gh auth status`. **`command_policy.json` and the hook are
> authoritative; this document is not.** The standing order below described a lifecycle no agent could
> actually execute, which cost a full ship attempt to discover.
>
> **What an agent can actually do:** commit locally on a task branch, and run `./gate.ps1`. That is the
> end of an agent's landing authority. Push, PR, merge, branch deletion and prune are the human's to run.
> An agent that believes it "landed" a change has not — verify with `git log`, never with an assumption.
>
> To change this, change `command_policy.json` (and its spec + smoke corpus) deliberately. Do not
> "fix" the contradiction by editing this paragraph back.

**Standing order — agents own the local lifecycle.** For a change you are authorized to make, take it as
far as an agent may: **commit → `./gate.ps1` green → report the branch and SHA for the human to push**.
Historically this order read *commit → push → open PR → merge → delete the branch → prune*; the human's
pre-authorization of the merge-and-delete policy still stands in principle, but the enforced policy above
means an agent cannot perform those steps. The hard stops are: (a) `./gate.ps1` is red, (b) a
branch-protection / required-check gate would need `--admin` (step 7), (c) a conflict you cannot cleanly
resolve, or (d) any step requiring a remote operation — in those cases **stop and
report**. Doc/config-only changes with no build impact (e.g. Markdown, `.gitignore`) do not require a UE5
build; say so in the PR body instead of skipping silently.

**The doc/config exemption does NOT cover the collaboration substrate's own guards.** Changes
touching any of these always require a gate run, regardless of file extension, because `gate.ps1` is
what verifies them and they are exactly the changes that look like "just config":
`agent_collab/scripts/**`, `agent_collab/tests/**`, `agent_collab/context/command_policy.json`,
`agent_collab/context/command-policy-spec.md`, and `agent_collab/adapters/*/hooks/**`. A `.psm1`
lexer, a `.jsonl` corpus and a `.json` policy declaration are code with a test suite attached —
`gate.ps1` runs that suite first and it takes seconds. If a full UE build is genuinely impossible,
run `pwsh -NoProfile -File agent_collab/scripts/Test-ShellGuard.ps1` and state in the PR body that
only the shell-guard segment was verified, naming what was not.

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
6. **Merge policy (human standing order):** merge with a real **merge commit** — **not** squash, **not**
   rebase — and delete the branch: `gh pr merge <n> --merge --delete-branch`, then
   `git checkout main && git pull --prune` and delete the local branch (`git branch -d <branch>`).
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
> - Merge (merge commit) → `PUT /repos/{owner}/{repo}/pulls/{n}/merge` with `{"merge_method":"merge"}`
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
- **Safety:** `git branch -d` (refuses unmerged) is the default and works cleanly under the merge-commit
  policy — once merged, the branch is a true ancestor of `main`, so git recognizes it (no squash-style `-D`
  workaround needed). Only `-D` (force) an unmerged branch when the human explicitly says so.

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

`claude --agent gloam-orchestrator` or `/gloam-resume`.

**Required first step after any clone:** `pwsh -NoProfile -File agent_collab/scripts/Project-ClaudeAdapter.ps1`,
then restart the session. This writes `.claude/settings.json`, the only place the `PreToolUse` Bash
hook is registered — without it `agent_collab/adapters/claude-code/hooks/pre-bash-policy.ps1` is
never invoked and the shell-policy guard is silently inactive. Source of truth is
`agent_collab/adapters/claude-code/`; never edit `.claude/` directly, it is overwritten.

### Shared rules

- Only Orchestrator writes `agent_collab/state/`, `handoffs/`, `outbox/`, `logs/decisions.md`.
- Workers write **only** to `inbox/<runtime>/raw/` when not returning schema inline.
- Promote to `agent-collab/gloam/work` only after integration Critic APPROVED.

Details: `agent_collab/context/agent_rules.md`, `agent_collab/adapters/grok-cursor/onboarding.md`.
