# Linked-worktree Git evidence reliability report

## Result

Gloamstead evidence emission now resolves and authenticates Git identity without invoking `git` at runtime for both ordinary repositories and linked worktrees.

- `.git` directories and strict `gitdir: <absolute-or-relative>` indirection files are supported.
- Linked administrative directories must be exactly `<common .git>/worktrees/<entry>`, their `commondir` must resolve to that common `.git`, and their `gitdir` back-pointer must resolve to the requesting project's `.git` entry.
- `HEAD` may be detached or a safe `refs/heads/...` symbolic ref. Worktree-local loose refs, common loose refs, and exact packed-ref entries are supported in precedence order.
- Commit values must be complete 40- or 64-character hexadecimal object IDs. All returned object IDs are normalized to lowercase.
- Metadata files are size-bounded and single-line. Missing, malformed, multi-line, escaped, cyclic, non-SHA, and suffix-spoofed metadata fails closed and clears both outputs.
- Report and run-manifest serialization resolve commit and branch as one identity snapshot so a concurrent metadata transition cannot mix two observations.

The only public addition is `ReadGitIdentityForProjectRoot`, an explicit-root seam used by focused hostile filesystem tests. All resolver and parser helpers remain private to `GloamsteadForgeEvidence.cpp`.

## Test-first evidence

- Red: the new focused test translation unit compiled and the UE link failed on the intentionally missing `ReadGitIdentityForProjectRoot` definition.
- First implementation run: hostile cases passed; valid cases exposed that the test scratch root was still UE-relative. The fixture was corrected to use an absolute root.
- Final UE 5.8 editor build succeeded.
- `Gloamstead.ForgeEvidence.GitIdentity`: 3/3 green:
  - `CurrentCheckout` authenticates this real linked worktree.
  - `ValidLayouts` covers ordinary, detached, absolute and relative linked-worktree pointers, CRLF/LF, loose worktree/common refs, and packed refs.
  - `HostileLayouts` covers malformed pointers, unauthenticated roots/back-pointers, commondir cycles, non-SHA values, ref traversal, packed-ref suffix spoofing, and missing HEAD.
- `git diff --check` passed.

## Repository gate evidence

Two full `gate.ps1 -Engine D:\UE_5.8` runs reached all evidence stages that previously failed on linked-worktree Git resolution:

- shell guard: green;
- UE 5.8 build: green;
- full `Gloamstead` automation: 90/90 green;
- GloamsteadForge contracts: green;
- GloamsteadForge runtime: green;
- nonce-bound integrity: green.

The gate then reported the pre-existing negatives wrapper as exit 1. Running `scripts/Test-GloamsteadForgeNegatives.ps1` directly immediately afterward rejected 30/30 hostile fixtures and returned exit 0; the same direct command also returned 0 with the gate nonce set and output piped to `Out-Null`. No negatives/gate script was in this task's ownership boundary, so that separate wrapper defect was not changed here.

No catalog, generated-provider, settings, config, Task 4, map, asset, or generated-report files are included in this change.
