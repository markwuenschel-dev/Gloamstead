---
description: Commit, push, open a detailed PR, merge it (merge commit), and delete merged branches (local + remote) — the full ship flow for Gloamstead
argument-hint: "[optional: PR title/notes; 'squash'/'rebase' to override merge method]"
allowed-tools: Bash, Read, Grep, Glob, Edit
---

Run the full ship flow for the current working tree of the **Gloamstead** repo
(`origin = https://github.com/markwuenschel-dev/Gloamstead.git`, owner `markwuenschel-dev`, repo
`Gloamstead`, base branch `main`).

This command **encodes the canonical workflow in `AGENTS.md`** (§"Git / PR / merge workflow (ALL agents)",
"Stale-branch cleanup", "Providing GitHub credentials"). If anything here is ambiguous, `AGENTS.md` wins —
read it.

Default merge method is **merge commit** (human standing order — *not* squash, *not* rebase). If
`$ARGUMENTS` contains `squash` or `rebase`, use that instead. Any other text in `$ARGUMENTS` is a hint for
the PR title/description.

**Auth & platform.** Native Windows; `origin` is HTTPS. The credential path is `gh` (run
`gh auth setup-git` once). The repo's git credential helper already injects `${GH_TOKEN:-$GITHUB_TOKEN}`
for HTTPS `git push`, so a plain push authenticates with **no token in the repo**. The token lives in
`.env` (git-ignored) and is **not** auto-exported — load it per-command with `set -a; . ./.env; set +a`
when you need it for REST. **Never** echo the token or write it anywhere outside `.env`.

## Steps

1. **Survey.** `git status`, `git branch -vv`, and read the diff so the commits and PR body are accurate.
   For *real* remote state use `git ls-remote origin <branch>` or the REST API — local tracking refs can be
   stale.

2. **Branch.** If on `main`, create a `feat/…` / `fix/…` / `docs/…` / `test/…` / `chore/…` branch first —
   **never commit directly to `main`**. If already on a feature branch, stay on it. One branch per logical
   change.

3. **Stage only intended paths.** Use explicit `git add <paths>` — **never `git add -A`**. Do not stage
   Unreal build output (`Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/`, `Build/`, `.vs/`,
   `*.VC.db`), local tooling/adapter dirs (`.claude/`, `.grok/`, `.neostack/`, `.agents/`), or secrets
   (`.env`). These are already in `.gitignore`; if something new should be ignored, **add it to
   `.gitignore`** rather than committing it.

4. **Commit** in logical, conventional-commit-style groups (`feat:`, `fix:`, `docs:`, `test:`, `chore:`),
   one concern per commit, with a body explaining the *why*. End each message with:
   `Co-Authored-By: Claude Opus 4.8 <noreply@anthropic.com>`

5. **Verify — green before PR.** Run `pwsh -File ./gate.ps1` from the repo root. It builds the
   `GloamsteadEditor` target and runs the `Gloamstead` automation tests, **failing closed** (no report /
   zero tests / any non-`Success` ⇒ `GATE FAIL`). **Do not open a PR or merge on red.** New tests live under
   `Source/Gloamstead/Tests/` and run automatically via the `Gloamstead` filter.
   - **Exception (AGENTS.md):** doc/config-only changes with no build impact (Markdown, `.gitignore`) do
     **not** require a UE5 build — skip the gate and **say so explicitly in the PR body**.
   - Heads-up: full gate builds are RAM-heavy on this machine (NeoStackAI TUs). If UBA starts kill-looping
     on memory, cap parallelism with `-MaxParallelActions=6` (see the build-memory note).

6. **Push** the branch over HTTPS, then **verify the ref actually landed** (push stdout can look falsely
   successful):
   ```bash
   git push -u origin HEAD:<branch>
   git ls-remote origin <branch>     # confirm the SHA matches local HEAD
   ```
   If the credential helper isn't picking up the token, `set -a; . ./.env; set +a` first so
   `${GH_TOKEN:-$GITHUB_TOKEN}` is available to it. **No manual base64 `http.extraheader` is needed** for
   this repo.

7. **Open a detailed PR** against `main`. Prefer `gh`:
   ```bash
   gh pr create --base main --head <branch> --title "…" --body "…"
   ```
   Body sections: **Summary**, **What's included**, **Testing** (the `gate.ps1` result — or
   `doc/config-only: no UE build required per AGENTS.md`), **Notes**. End the body with the
   "🤖 Generated with [Claude Code]" line.
   - If `gh` is unavailable: `POST /repos/markwuenschel-dev/Gloamstead/pulls` with
     `{"title","head","base","body"}`, header `Authorization: Bearer $GH_TOKEN`. Build the JSON via a
     heredoc/temp file (not inline string interpolation) to avoid escaping bugs.

8. **Merge** with a real **merge commit** (unless `$ARGUMENTS` overrode the method):
   ```bash
   gh pr merge <n> --merge --delete-branch
   ```
   REST fallback: `PUT /repos/markwuenschel-dev/Gloamstead/pulls/<n>/merge` with
   `{"merge_method":"merge"}` (or `squash` / `rebase`).
   - **Never pass `--admin`.** If a branch-protection / required-check gate blocks the merge, **stop and
     report to the human** — override only with explicit per-merge authorization.

9. **Clean up — own the lifecycle, leave no orphan branches.**
   - Remote PR branch: removed by `--delete-branch` (or REST
     `DELETE /repos/markwuenschel-dev/Gloamstead/git/refs/heads/<branch>`).
   - Local `main`: `git checkout main && git pull --prune`.
   - Local feature branch: `git branch -d <branch>` (use `-d`, not `-D` — under the merge-commit policy the
     branch is a clean ancestor, so `-d` works).
   - Sweep stragglers: remote `git branch -r --merged origin/main` (delete only ones you own — **never**
     `main` or `HEAD`); local `git branch --merged main | grep -vE '^\*|^\s*main$' | xargs -r git branch -d`.

10. **Report** the PR number/URL, the merge SHA, what was deleted (local + remote), and the gate result (or
    the doc-only exemption).

**Never** echo the token or write it anywhere outside `.env`. **Confirm with the human before merging** if
`gate.ps1` is red, the diff looks unexpected, or a protection gate would require `--admin`. Canonical
reference: `AGENTS.md` → "Git / PR / merge workflow (ALL agents)".
