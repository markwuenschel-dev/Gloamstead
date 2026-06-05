# Runtime Adapter Extension Contract

**Status**: This is the authoritative specification for adding any runtime beyond the initial `claude-code` and `local-script`. Do not create adapter folders for speculative runtimes.

## When to Add a New Adapter
Only when **all** of the following are true:
1. A concrete second runtime exists and will be used for real work in this repo (e.g. you have API keys, a local model runner, or a human-paste watcher process that will actually be invoked).
2. The Claude-only (sync) path has been exercised against at least one real, non-trivial task and the failure modes / restart / integration verification behavior are understood.
3. You are ready to switch (or add) `scheduler_state.mode` to `async` and implement lease/heartbeat/reconcile logic.

Until then, document the intended runtime in `runtimes.json` (disabled) and leave the adapter as a future extension.

## What a Complete Adapter Must Provide

### 1. Registry Entries (done first)
- Add accurate capability flags to `registry/runtimes.json` (can_edit_files, can_run_tests, can_use_worktree, can_enforce_scope, can_return_schema, can_parallelize, launch_mode).
- Add one or more templates to `registry/agents.json` with the new runtime, role(s), max_parallel, writes_state:false.

### 2. Adapter Directory
Create `agent_collab/adapters/<runtime-id>/` (e.g. `openai`, `grok-build`, `gemini-cli`, `chatgpt-manual`).

Minimum contents:
- `adapter.json` or `runner_config.json` — declares invocation mechanism, result delivery path (must be ONLY `inbox/<runtime-id>/raw/`), timeout, auth notes (never commit secrets).
- `launcher.md` — exact steps a human or script uses to launch a worker request for this runtime and how/where the raw result appears.
- `onboarding.md` — one-time setup, credential handling, scope enforcement story for this runtime.
- `result_contract.md` — the exact shape of raw output this runtime will produce (so Normalize-RuntimeResult.ps1 can be written or extended).
- (Optional) `Normalize-<Runtime>.ps1` or equivalent helper if complex mapping is needed.

### 3. Result Delivery Discipline (non-negotiable)
- The adapter / launcher / watcher / human process for this runtime **writes ONLY to `agent_collab/inbox/<runtime-id>/raw/**`.
- It must **never** write handoffs/, outbox/, state/, logs/, or any normalized protocol artifact.
- If the runtime supports structured output (`can_return_schema: true`), the Orchestrator may read it directly from the worker response channel; no raw round-trip is required.
- If `can_return_schema: false`, a raw file lands in inbox/.../raw/ and `Normalize-RuntimeResult.ps1` (or runtime-specific normalizer) + `Validate-JsonSchema.ps1` must succeed before the Orchestrator may treat it as a `worker_summary` or `critic_verdict`.

### 4. Capability Truthfulness
The flags declared in runtimes.json must be accurate:
- If it cannot reliably prevent out-of-scope edits → `can_enforce_scope: false`.
- If it cannot run the project's test suite in CI-like conditions → `can_run_tests: false`.
- If workers can hallucinate file paths or the platform has no worktree equivalent → `can_use_worktree: false`.
- Overstating capabilities will cause routing to select unsafe runtimes for Coder or final Critic roles → integration failures or corruption.

### 5. Safety Floor Compliance
A new runtime may only be chosen for:
- `edit_code` tasks if it declares (and actually provides) can_edit_files + can_use_worktree + can_enforce_scope.
- Final integration Critic if it declares can_run_tests + repo_read (and the platform can actually execute the suite).

### 6. Async Mode Requirements (if launch_mode implies out-of-band)
- Must support `lease_id` in worker_request.
- Must participate in heartbeat/expiry protocol (or the launcher must update leases.json on completion).
- The Orchestrator's resume-and-reconcile loop must be able to detect late results and either accept them (if lease still valid) or archive them as stale.

### 7. No Direct Worker Spawning
Even if the new runtime supports sub-delegation or tool-calling that spawns other agents, the worker contract forbids it. Workers return BLOCKED + needs/request; only the Orchestrator creates new handoffs and instances.

### 8. Projection / Generation (if applicable)
If the runtime has its own "agent" or "prompt" registry (like .claude/ for claude-code), the adapter folder is the source of truth. A documented projection step (script or manual) populates the runtime-specific location. Never edit the projected files directly.

## Example Future Adapter (sketch only — do not implement until concrete need)

```
adapters/gemini-cli/
  adapter.json
  launcher.md                 # how to invoke `gemini ...` with the worker_request context
  onboarding.md
  result_contract.md          # "it emits a markdown response; the last fenced JSON block is the candidate worker_summary"
  Normalize-Gemini.ps1        # extracts the JSON, maps to schema, writes to a temp normalized file
```

## Single-Writer Guarantee
Adding a runtime never relaxes the rule: only the Orchestrator (the main session/process that holds the orchestrator.lock) may write the durable protocol artifacts. Any adapter or watcher that violates this is a correctness and audit bug.

## Versioning
When you add a runtime, bump the `schema_version` in affected registry files and note the change in `logs/decisions.md` with date and rationale.

This contract exists so the protocol remains the stable source of truth while adapters are minimal, late-bound projections.
