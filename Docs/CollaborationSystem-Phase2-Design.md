# Phase 2 Design Document: Async Mode and First Async Runtime for the Gloamstead agent_collab Collaboration System (v8.1)

**Document ID**: grok-design-doc-5ca77255  
**Date**: 2026-05-31  
**Author**: Systems Architect (Grok Build subagent)  
**Status**: Design for implementation planning  
**Target Audience**: Human Orchestrator + Claude Code sessions implementing the Gloamstead agent_collab system  
**Related Specs**: Collaboration Architect v8.1 (original), agent_collab/adapters/_EXTENSION_CONTRACT.md (authoritative for adapters)  

---

## Executive Summary

This design provides a concrete, incremental, and fully compliant plan to implement **Phase 2** of the Gloamstead `agent_collab` system. Phase 1 delivered a complete synchronous (sync) foundation: full protocol (schemas in `agent_collab/protocol/`), registries (`agent_collab/registry/`), two runtimes (`claude-code` native + `local-script` runner), supporting PowerShell scripts, lock management, scope/policy guards, normalization, wave planning, self-test (`agent_collab/scripts/Test-AgentCollabScaffold.ps1` passes cleanly), and a successful first `/gloam-resume` (2026-05-31) with clean reconciliation, empty consistent state, and `scheduler_state.mode = "sync"`.

Phase 2 executes the explicit v8.1 directive: "when a second runtime is concrete: implement exactly one async adapter against _EXTENSION_CONTRACT.md, switch `scheduler_state.mode` to `async`, exercise leases/heartbeats." It does **not** build adapters for runtimes not about to be used.

**Core deliverable**: Move from sync-only (two runtimes) to full support for exactly **one** additional async runtime + complete asynchronous Orchestrator control flow, while strictly preserving every hard invariant:
- Only the Orchestrator (session holding `agent_collab/state/orchestrator.lock`) writes durable protocol artifacts (`state/`, `handoffs/`, `outbox/`, `logs/`, `decisions.md`).
- Workers/adapters/watches write **exclusively** to `inbox/<runtime>/raw/`.
- A watcher writing normalized artifacts is a corruption vector.
- Leases/heartbeats/expiry exist **only** for async mode.
- Safety floors and routing (`registry/routing.json`) remain authoritative.
- Design for N runtimes; exactly two implemented today (claude-code, local-script); exactly one new async added in Phase 2.

**Recommended minimal first async runtime (justified below)**: `paste-watcher` (file-drop / manual-paste watcher). This is the lowest-risk concrete runtime that fully exercises the machinery (lease acquire, fire-and-forget delegation, inbox/raw drop, normalization + validation, heartbeat/expiry detection, stale result handling, reconcile on resume) without external APIs, keys, or automated execution. It is used first only for trivial non-mutating tasks.

All changes are incremental. The mode flip is the **last** controlled step, after scripts, docs, tests, adapter, **and completion of at least one real non-trivial Claude-only sync wave** (per _EXTENSION_CONTRACT.md precondition #2 — see Staged Rollout). Self-test is extended to cover async scenarios synthetically before the real runtime is added.

This document cites **exact existing file paths** from the current scaffold (verified via filesystem inspection on 2026-05-31). The system has run zero application tasks to date; a required real non-trivial sync wave (exercising Coder/Critic/integration verification/restart on actual handoffs under claude-code) must complete and be recorded in `decisions.md` *before* the paste-watcher adapter folder is created or mode is flipped. Only after full Phase 2 machinery validation (including the first trivial async exercise on throwaway work) will real Coder/Critic work on `Source/` proceed.

---

## 1. Background and Current State (Phase 1 Complete)

### Verified State (2026-05-31)
- **Self-test**: `pwsh -NoProfile -File agent_collab/scripts/Test-AgentCollabScaffold.ps1` passes cleanly (structure, files, schemas, lock acquire/release, scope guards, policy, .gitignore, projections, Claude-only profile, no unexpected adapters).
- **First /gloam-resume**: Performed successfully. Lock acquired cleanly. Full reconciliation: no `agent-collab/gloam/*` branches, no worktrees, no handoffs (only `HANDOFF_TEMPLATE.md`), `inbox/*/raw/` empty (only `.gitkeep`), `outbox/` empty, `leases.json` empty, `task_state.json` and `scheduler_state.json` match ground truth exactly (empty). Mode: sync. Recorded in `agent_collab/logs/decisions.md` and `agent_collab/logs/orchestrator.log`.
- **No application tasks**: Zero waves, zero Coders/Critics on real work. System is cold and ready.
- **Async skeletons (prepared, unexercised)**:
  - `agent_collab/state/scheduler_state.json`: `"mode": "sync"`, `in_flight/ready_queue/blocked/completed` empty.
  - `agent_collab/state/leases.json`: `{"leases": [], "notes": "Only populated and used when scheduler_state.mode == \"async\"..."}`
  - `agent_collab/protocol/lease.schema.json`: Full lease shape with `lease_id`, `task_id`, `request_id`, `status` enum (active/completed/expired/released/failed), `created`/`heartbeat`/`expires`.
  - `worker_request.schema.json` and `task_state.schema.json`: `lease_id` optional fields present.
  - `scheduler_state.schema.json`: mode enum includes "async".
  - Descriptive "if mode == async" branches and lease mentions exist in:
    - `agent_collab/context/restart_instructions.md`
    - `agent_collab/adapters/claude-code/commands/gloam-resume.md`
    - `agent_collab/adapters/claude-code/commands/gloam-status.md`
    - `agent_collab/adapters/claude-code/commands/gloam-start-task.md`
    - `agent_collab/handoffs/HANDOFF_TEMPLATE.md` (lease_id note in Runtime Notes)
    - `agent_collab/adapters/_EXTENSION_CONTRACT.md` (Section 6: Async requirements)
  - **No implementation**: No `New-Lease.ps1`, no reaper, no async reconcile logic, no mode-flip script, no async branches exercised in any script. `Normalize-RuntimeResult.ps1` is a conservative skeleton.
- **Runtimes (exactly two)**: `claude-code` (launch_mode: "native", full caps including edit/scope/tests), `local-script` (launch_mode: "runner", deliberately limited caps; fails edit_code and final Critic safety floors).
- **Inbox boundary respected**: Only two `inbox/<runtime>/raw/` dirs exist (with `.gitkeep`).
- **Single-writer demonstrated**: All mutations during resume/reconcile/lock tests performed only while holding lock via scripts; no watchers or adapters have written normalized artifacts.
- **Projections**: `.claude/` (agents + commands + settings.json) present as projection of `agent_collab/adapters/claude-code/` (source of truth). `.claude/worktrees/` exists and is empty (gitignore'd).
- **Other exact files** (complete relevant set): See `agent_collab/adapters/_EXTENSION_CONTRACT.md`, all under `agent_collab/registry/`, `protocol/`, `context/` (including `scope_roots.json`, `command_policy.json`, `agent_rules.md`, `project_goal.md`, `environment.md`), `scripts/` (Acquire-OrchestratorLock.ps1, Release-..., Assert-EditScope.ps1, Assert-BashPolicy.ps1, Build-WavePlan.ps1, Normalize-RuntimeResult.ps1, Validate-JsonSchema.ps1, Test-...), `state/orchestrator.lock.example`, `handoffs/HANDOFF_TEMPLATE.md`, `logs/decisions.md` (entries for scaffold + cold resume), `logs/orchestrator.log`.

The system is in a perfect "green field" state for safely introducing async machinery.

---

## 2. Phase 2 Objectives

1. Add support for **exactly one** async runtime adapter, implemented strictly per `agent_collab/adapters/_EXTENSION_CONTRACT.md` **and only after the Claude-only sync path has been exercised against at least one real, non-trivial task** (contract precondition #2, enforced as hard gate in rollout).
2. Implement the full async Orchestrator resume-and-reconcile loop (lease lifecycle, expiry detection, stale result handling, inbox/raw collection for out-of-band results).
3. Provide lease management as reusable, testable PowerShell scripts (with built-in support for isolated/temp execution from day one).
4. Adapt scheduler/wave execution for async (fire-and-forget delegation + later collection on resume; support for hybrid sync/async within a wave where safe).
5. Create safe, validated mechanism to flip `scheduler_state.mode` (and keep it sync until ready).
6. Extend self-test (`Test-AgentCollabScaffold.ps1`) with synthetic async scenarios that pass before any real async runtime or task.
7. Deliver staged rollout: **at least one real non-trivial Claude-only sync wave (Coder/Critic + integration verification + restart on actual artifacts) completed and recorded before adapter creation or mode flip**; machinery then validated on trivial async tasks **before** any use for production Coder/Critic work on `Source/`.
8. Update all relevant documentation (restart_instructions.md, the three gloam-*.md commands, onboarding, etc.) with concrete procedures.
9. Preserve 100% backward compatibility for pure sync (Claude-only) operation until the deliberate flip.

**Non-goals for Phase 2**:
- Adding >1 async adapter.
- Scaffolding speculative adapters (forbidden by contract).
- Using the async runtime for any `edit_code`, worktree, or final integration Critic work.
- Changing core game implementation (Unreal `Source/`, `Content/`, etc.).
- Automated heartbeat push from workers (manual refresh + expiry windows only for the starter runtime).
- New schemas (existing ones are sufficient).

---

## 3. Guiding Principles and Hard Rules (Must Be Followed)

Directly from v8.1 context and `_EXTENSION_CONTRACT.md` (read and respected in full):

- "Phase 2 (when a second runtime is concrete): implement **exactly one** async adapter against _EXTENSION_CONTRACT.md, switch `scheduler_state.mode` to `async`, exercise leases/heartbeats. **Do not build adapters for runtimes you are not about to use**."
- "Design for N runtimes. Implement two now: claude-code and local-script." (already done; Phase 2 adds the third total, first async).
- "The protocol is the source of truth. .claude/ is one projection of it."
- "**Only the Orchestrator writes durable state.**"
- "Two Orchestrator control-flow modes: Synchronous (default for Claude-only) vs Asynchronous (when any runtime returns out-of-band)."
- "Declare the active mode in scheduler_state.json (`mode: sync | async`)."
- "Leases and heartbeats exist for async mode only."
- "**A watcher that writes a "normalized" protocol artifact is a second writer and a corruption vector.**"
- From contract Section 6 (Async): Support `lease_id` in worker_request; participate in heartbeat/expiry **(or launcher updates leases on completion)** — but **overridden by Single-Writer Guarantee (Section 69-70)**: "only the Orchestrator ... may write the durable protocol artifacts. Any adapter or watcher that violates this is a correctness and audit bug."
- Safety floors in `routing.json` and agents.json templates are non-negotiable.
- "No Direct Worker Spawning": Workers return BLOCKED + needs/request; only Orchestrator creates handoffs/instances.
- Extension only when **all** three preconditions: concrete runtime exists + will be used for real work; Claude-only sync path exercised on >=1 real non-trivial task; ready to switch mode + implement lease/heartbeat/reconcile.
- "When you add a runtime, bump the `schema_version` in affected registry files and note the change in `logs/decisions.md`."

**Design implication**: The `paste-watcher` launcher/result drop process will **never** write leases.json, task_state, handoffs, etc. It writes **only** the raw result file. Orchestrator (via new scripts, lock held) performs all updates on reconcile. The parenthetical "launcher updates leases" is interpreted as optional signaling via raw result only; single-writer is absolute.

---

## 4. Recommended First Async Runtime: paste-watcher

### 4.1 Name and Identity
- **Runtime ID**: `paste-watcher`
- **Display name**: "Human Paste / File-Drop Watcher (out-of-band manual result delivery)"
- **launch_mode**: "out_of_band"
- **Why this exact choice** (low-risk starter, per prompt guidance):
  - Zero external dependencies, no API keys, no network, no local LLM runner, no background job management complexity.
  - Purely exercises the **protocol boundary** that matters for Phase 2: Orchestrator creates lease + request, "fires" (writes request details for pickup), releases lock; later resume detects raw drop in `inbox/paste-watcher/raw/`, normalizes (if needed), validates, reconciles lease/result into state/handoffs/scheduler, handles expiry/stale.
  - Human acts as the "executor" (runs the work in a separate terminal/Claude session, formats per `result_contract.md`, drops file). This is the simplest possible concrete runtime that satisfies "a second runtime is concrete" and "will be used for real work."
  - Far lower risk than "local long-running command watcher" (no polling, no partial writes from jobs, no shell escaping issues) or any LLM API adapter.
  - Immediately usable for the **mandatory first trivial async exercise** (see Rollout section) without touching `Source/` or game logic.
  - "file-drop / manual-paste watcher" exactly matches the prompt's recommended example.
  - Once machinery is proven, higher-capability async runtimes (local LLM, API-based) can be added later following the exact same contract and patterns (N-runtime design).

**Rejected alternatives (for Phase 2 only) + explicit comparison** (addresses review feedback on meaningful exploration):

**Comparison of bootstrap options for "first concrete async runtime"** (while strictly honoring "exactly one async adapter" and "do not build for runtimes not about to be used"):

| Option | Pros | Cons / Risks | Why Rejected for Phase 2 |
|--------|------|--------------|--------------------------|
| Promote *existing* `local-script` (already concrete, in registries/agents.json/self-test/lock tests, launch_mode "runner") as first out_of_band (zero new adapter dir; just update its launcher/runner_config + 1 new template + lease wiring) | Satisfies "second runtime is concrete" literally with *zero* new files/folders; leverages fully tested existing code; minimal surface area. | Current design/docs intentionally bound it as *synchronous runner* for non-editing work; making it cleanly fire-and-forget + lease_id + out_of_band would require non-trivial changes to its semantics, result delivery, and safety story; risks diluting its "fails edit/floor" intent and "exactly one new adapter" discipline. | Rejected: Would modify an *existing* adapter's contract/behavior (against the spirit of adding "exactly one async adapter" and the contract's "create adapter directory" step for new runtimes). Paste-watcher keeps the two existing runtimes pristine. |
| New dedicated `paste-watcher` (file-drop/manual-paste) | Purpose-built pure out_of_band with zero side effects on current runtimes; lowest operational risk (human-mediated, no jobs/polling); matches prompt's explicit low-risk starter example; exercises full machinery immediately; can be added atomically with registry "first". | Introduces one new (small) adapter folder (per contract, only when ready). | **Chosen**: Best balance for bootstrap. Zero impact on claude-code/local-script. Fully satisfies "implement exactly one" + contract preconditions once the real sync wave gate is passed. Demonstrates the N-runtime extension pattern cleanly for future (real LLM) adapters. |
| Automated local long-running (pwsh Start-Job + watcher) or early LLM API adapter | More "realistic" async execution. | High complexity for first exercise; keys or job management risks; violates "before any external LLM API" and "low-risk starter" guidance. | Rejected per explicit v8.1/prompt constraints. |

The paste-watcher choice is deliberate, low-risk, and fully compliant. Promoting local-script was seriously weighed but would have required more invasive changes than adding a minimal dedicated adapter. See also strengthened Key Decision on this topic.

### 4.2 Capability Declaration (Must Be Accurate)
Will be added to `agent_collab/registry/runtimes.json` (in the PR that introduces the adapter; initially as disabled per contract if early doc PR):

```json
"paste-watcher": {
  "display_name": "Human Paste / File-Drop Watcher (out-of-band manual result delivery)",
  "launch_mode": "out_of_band",
  "can_edit_files": false,
  "can_run_tests": false,
  "can_use_worktree": false,
  "can_enforce_scope": false,
  "can_return_schema": false,
  "can_parallelize": true,
  "notes": "Phase 2 bootstrap async runtime. Human executor performs work externally (research, simple commands, test log capture) and drops exactly one raw result file into inbox/paste-watcher/raw/<request_id>-<timestamp>.raw.json (or .txt with fenced JSON). Strictly limited to repo_read / web_research roles. MUST NOT be routed to edit_code or final_integration_critic (fails safety floor). Single-writer: drops ONLY raw results; Orchestrator reconciles all state."
}
```

Corresponding template in `agent_collab/registry/agents.json` (enabled only when ready):

```json
{
  "id": "paste-researcher",
  "runtime": "paste-watcher",
  "role": "Researcher",
  "enabled": true,
  "capabilities": ["repo_read", "web_research"],
  "max_parallel": 2,
  "writes_state": false,
  "notes": "Phase 2 async exerciser only. Human pastes result after external execution. NEVER use for Coder, Critic, Documentor, Architect, Planner, or any edit/scope/test-verification task. Routing must respect this."
}
```

**Routing impact**: `registry/routing.json` preference_rank and safety_floor already sufficient (paste-watcher will naturally fail edit_code and final Critic floors). Orchestrator prompts/docs will explicitly avoid it for high-trust roles.

### 4.3 Adapter Directory (Created in One Specific PR Only)
Per `_EXTENSION_CONTRACT.md` "What a Complete Adapter Must Provide" and "When to Add":

`agent_collab/adapters/paste-watcher/` (exact minimum):

- `runner_config.json` — invocation notes, result_delivery rule (ONLY to inbox/paste-watcher/raw/), timeout guidance (human-managed), no auth.
- `launcher.md` — **exact** human steps: how Orchestrator signals the request (print full worker_request JSON + lease_id + exact target filename for drop), how/where to drop the raw result, verification checklist (request_id/lease_id match in content and filename, timestamps reasonable).
- `onboarding.md` — one-time: mkdir for inbox, first drop example, warnings about single-writer and id matching, how to request heartbeat refresh.
- `result_contract.md` — **exact** acceptable raw shapes (pure JSON worker_summary + lease_id + raw_output_path; or markdown with last fenced ```json block). Must contain lease_id for matching. Human responsibility for formatting; Orchestrator will Normalize + Validate before use.

**Optional but recommended for minimal**: A tiny helper script? No — keep adapter declarative. Any normalization dispatch goes in updated `Normalize-RuntimeResult.ps1`.

**Inbox requirement**: On adapter addition, ensure `agent_collab/inbox/paste-watcher/raw/` exists with `.gitkeep` (will be added to `Test-AgentCollabScaffold.ps1` requiredDirs).

**Critical rule enforcement in docs**: Every file in this adapter will contain (in prominent section): "Per Single-Writer Guarantee in _EXTENSION_CONTRACT.md: This adapter and its human process write **ONLY** raw result files to `inbox/paste-watcher/raw/`. Never create, edit, or normalize any file under `handoffs/`, `state/`, `outbox/`, `logs/`, or `decisions.md`."

---

## 5. Lease Management (Detailed Lifecycle and Scripts)

Leases are the mechanism for tracking out-of-band work across Orchestrator restarts. They live only in async mode.

### Lifecycle States (from lease.schema.json)
`active` → `completed` (on successful reconcile of matching raw result) | `expired` (reaper) | `released` (early cancel) | `failed` (result processing error).

### Key Timestamps
- `created`: Delegation time.
- `heartbeat`: Last known "alive" signal (initially = created; refreshed by Orchestrator on human confirmation of ongoing external work; updated on result).
- `expires`: Absolute deadline (created + timeout).

### New Scripts (agent_collab/scripts/)
All scripts:
- Require lock held (or take -RequireLock and call Acquire internally where safe).
- Are idempotent where possible.
- Append structured entries to `orchestrator.log` and `decisions.md`.
- Operate on copies or atomically (read-modify-write with validation).

1. **New-Lease.ps1** (new)
   - Params: `-TaskId`, `-RequestId`, `-Role`, `-Runtime`, `-TemplateId`, `-InstanceId`, `-TimeoutMinutes` (default 60; Researcher 45, others configurable).
   - Generates `lease_id`.
   - Creates entry, appends to `leases.json` (validates no duplicate active lease for task_id).
   - Returns JSON of the lease.
   - Used at delegation time for any out_of_band runtime.

2. **Refresh-LeaseHeartbeat.ps1** (new)
   - Params: `-LeaseId` (or `-TaskId`).
   - Updates `heartbeat` = now for active lease.
   - Human-triggered via Orchestrator during long-running external work (e.g., "still working on paste request for TASK-xxx").

3. **Reap-ExpiredLeases.ps1** (new)
   - No params (or optional -DryRun).
   - Scans all leases with status=active.
   - If `now > expires` OR `(now - heartbeat) > (expires - created)` (generous window), set status=expired.
   - For each: update corresponding `task_state.json` entry (status=blocked, `blocked_reason`="Lease expired: $lease_id", `reopen_reason` if applicable).
   - Log + decisions.md entry with full diff.
   - Returns count reaped. Safe to run on every resume in async mode (or always, no-op if sync/empty).

4. **Reconcile-AsyncResults.ps1** (new, the heart of async collection)
   - Scans `inbox/*/raw/` (focus on runtimes with launch_mode out_of_band or all if mode=async).
   - For each raw file:
     - Parse `request_id` and `lease_id` (from filename convention `<request_id>-*.raw.*` + content).
     - Lookup lease in `leases.json`.
     - **Stale handling** (critical): If lease missing, status != active, or result timestamp indicates too late → move raw to `inbox/<runtime>/stale/` (or `handoffs/archived/stale-<lease>-<req>.raw`), append "STALE_RESULT_IGNORED" to decisions.md + log. **Never** apply to state/handoff.
     - Active + valid window: 
       - If runtime `can_return_schema=false` (paste-watcher always): call `Normalize-RuntimeResult.ps1 -Runtime paste-watcher -RawPath ... -OutPath temp-normalized.json`.
       - `Validate-JsonSchema.ps1` against `worker_summary.schema.json` (or critic_verdict if applicable).
       - On success: 
         - Update lease: status=completed, heartbeat=now.
         - Locate handoff (by task_id or request_id in claimed/ or done/): update status=done, fill output fields, `runtime_notes` with lease + reason.
         - Update `task_state.json` active entry + append history.
         - Remove from `scheduler_state.in_flight` if present; advance wave status if all tasks resolved.
         - Record in logs/decisions.
       - On validation fail: treat as BLOCKED result, update with verdict=BLOCKED, require human review.
   - Idempotent: re-running on already-processed results is no-op (check lease status or a small processed manifest if needed; keep simple via lease state).
   - Atomicity: Write updated JSONs to .tmp then Move-Item (PowerShell atomic on same volume).

5. **Set-SchedulerMode.ps1** (new, see flip section).

**Updates to existing scripts**:
- `Normalize-RuntimeResult.ps1`: Extend parameter handling and mapping logic for `-Runtime paste-watcher` (and future). Improve skeleton: attempt to parse raw as JSON; if contains fenced block, extract; populate lease_id/request_id from raw if present; keep conservative defaults (verdict=BLOCKED on uncertainty) but produce usable worker_summary shape. Add runtime-specific notes.
- `Test-AgentCollabScaffold.ps1`: Major extension (see dedicated section).
- `Acquire-OrchestratorLock.ps1` and `Release-OrchestratorLock.ps1`: **Minimal targeted updates** (PR4 or dedicated) for async safety: when `scheduler_state.mode == "async"` or `leases.json` contains active leases, the stale takeover path requires explicit human confirmation (via new `-ForceTakeover` param or interactive prompt in non-interactive contexts). On confirmation, append a detailed entry to `decisions.md` including snapshot of active leases + mode. The scripts will read the mode/leases files (cheap) for this check. This directly addresses takeover risk with in-flight async work (see Risk Analysis 10.1 and Section 8). No other behavioral changes to lock heartbeat or basic fresh/stale logic.

`Build-WavePlan.ps1` requires no changes (waves remain DAG-driven; async affects only dispatch timing).

---

## 6. Changes to Orchestrator Resume-and-Reconcile Loop

The "Orchestrator" is the human + Claude Code session following the command docs and using tools (Read, Bash/pwsh, Write, Task) to invoke scripts. No monolithic orchestrator.ps1 exists or will be added.

### Updates Required
- **agent_collab/context/restart_instructions.md** (primary source of truth for restart):
  - Expand Step 2 (Read): Always read leases.json (cheap if empty).
  - Expand Step 3 (Reconcile):
    - "If scheduler_state.mode == 'async' OR leases.json.leases non-empty: (a) Run `pwsh .../Reap-ExpiredLeases.ps1`. (b) Run `pwsh .../Reconcile-AsyncResults.ps1`. (c) Cross-check resulting in_flight vs active leases."
    - Stale result handling procedure.
    - "Rebuild caches from git + handoffs + inbox/raw + leases if any divergence."
  - Step 4 (Report): Add "Active/expired leases: N (details)", "Raw drops processed this resume: X (Y stale)".
  - New subsection: "Lease Lifecycle During Resume".
  - Cold restart safety: Emphasize lease-based recovery for out-of-band work.

- **agent_collab/adapters/claude-code/commands/gloam-resume.md**:
  - Mirror the above in its numbered steps.
  - Expand "Async vs Sync" section with concrete paste-watcher example (fire request + lease, release lock, later resume reconciles drop).
  - Final human prompt updated: include options like "refresh heartbeat for LEASE-xxx", "force reconcile of specific raw file", "reap leases".

- **agent_collab/adapters/claude-code/commands/gloam-status.md**:
  - Add **Active Leases** section after Queue: count by status, list top 3 with age vs expires, task_id, role/runtime.
  - Expand **Inbox Raw** to enumerate per-runtime (including paste-watcher once added): unprocessed + stale counts.
  - Recommendation logic considers pending async results or expired leases.

- **agent_collab/adapters/claude-code/commands/gloam-start-task.md** (and any ad-hoc delegation):
  - Update step 9: "Delegate ... In async mode or for out_of_band runtimes: create lease via New-Lease.ps1, write worker_request JSON to conventional pickup location (inbox/paste-watcher/requests/ for this runtime or stdout for human copy), persist all state, **do not wait**."

Similar light updates to `launcher.md` files and `onboarding.md` (claude-code and local-script) to note async availability post-Phase 2.

All updates keep sync-only paths identical and fully functional.

---

## 7. Scheduler / Wave Execution for Async (Fire-and-Forget + Collection)

- **Dispatch (in a resume session, after planning)**:
  - For each task in current wave (respecting max_parallel and in_flight count):
    - Run routing (registries + safety floor).
    - If chosen runtime launch_mode == "out_of_band" (paste-watcher) or system is in async mode for this delegation:
      - If this is the first async delegation: Orchestrator confirms with human, then calls `Set-SchedulerMode.ps1 -Mode async`.
      - Call `New-Lease.ps1` (short timeout for first exercises).
      - Write `worker_request` (full JSON including `lease_id`) to `inbox/paste-watcher/requests/<request_id>.request.json` (Orchestrator write to inbox/ is allowed and expected; it is delivery, not durable protocol state mutation of handoffs/state).
      - Create/update handoff in `handoffs/claimed/` with lease_id in runtime_notes + reason.
      - Update `task_state.json` (status: assigned → in_progress, lease_id, runtime, template, instance).
      - Update `scheduler_state.json` (add task_id to in_flight; wave status → "running").
      - Persist everything.
      - Log decision.
      - **Continue to next task or end turn** (no synchronous wait).
  - For claude-code (native): Can still use in-session subagent spawn (sync within the Claude session) even after global mode=async, because it does not "return out-of-band." Hybrid waves supported.
  - Release lock when handing back to human or between waves.

- **Collection (on subsequent resume)**:
  - Reap + Reconcile-AsyncResults (as above) populates results into handoffs/done, task_state, scheduler (move from in_flight to completed or blocked).
  - Wave status transitions (planned → running → candidate_testing once all tasks resolved or partial failure policy).
  - If a wave has async tasks, its promotion to candidate waits for full collection (or explicit human "proceed with partial + re-plan").

- **in_flight / ready_queue / blocked handling**: Unchanged in structure; async simply means some in_flight entries have associated active leases and will be resolved on future resumes rather than same-session.

This matches "fire-and-forget + later collection."

---

## 8. How to Safely Flip the Mode Flag

**Never** edit `agent_collab/state/scheduler_state.json` by hand or via generic JSON tools.

**Only mechanism**: New script `agent_collab/scripts/Set-SchedulerMode.ps1 -Mode <sync|async> [-Reason "..."] [-Force]`

**Preconditions enforced by the script (before any write)**:
- Orchestrator lock is held by current process (calls Acquire if needed or fails).
- Current `scheduler_state.json` parses and is valid per schema.
- Target != current (or warn).
- If setting `async`:
  - `registry/runtimes.json` has >=1 runtime in enabled_runtimes with `launch_mode != "native"`.
  - For every such async runtime: `adapters/<id>/` dir exists with at minimum `launcher.md` and `runner_config.json` (or adapter.json).
  - `inbox/<id>/raw/` directory exists (with or without .gitkeep).
  - No active leases with status that would be invalid post-flip.
- If setting `sync`: No active leases (or --Force + human confirmation recorded).
- Self-test (or at least the async simulation subset) has passed in this session (script can invoke it).
- Append full rationale + before/after + timestamp + actor to `decisions.md` and `orchestrator.log`.

**When to flip (orchestrator decision, recorded)**:
- After all PRs up to the mode script are merged.
- After self-test (extended) passes.
- After adapter + registries updated.
- **Only after the mandatory real non-trivial Claude-only sync wave has been executed and recorded in decisions.md** (see Staged Rollout §11 "Hard Gate", fulfilling _EXTENSION_CONTRACT.md precondition #2; this is the binding requirement, not merely "preferably").
- Additional trivial exercises for comfort are encouraged but secondary to the contract gate.
- The flip itself can be the trigger for the very first trivial async exercise in the same session (post-Hard Gate).

Post-flip, `gloam-resume` and status commands surface async prominently. Revert is possible via script but discouraged until Phase 2 stabilized (one-way recommended in practice).

---

## 9. Self-Test Extensions for Async Scenarios

**agent_collab/scripts/Test-AgentCollabScaffold.ps1** will gain a new major section (e.g., #13 or parameterized `-TestAsync`):

- **Synthetic lease lifecycle** (uses temp directories + temp copies of state/*.json to avoid mutating real state):
  - Create temp paste-watcher inbox/raw + requests.
  - Call New-Lease.ps1 (via pwsh invocation on temp leases.json), assert creation, unique id, timestamps, status=active, file updated.
  - Call Refresh-LeaseHeartbeat, assert heartbeat advanced.
  - Call Reap-ExpiredLeases (after forcing old heartbeat/expires on a copy), assert status=expired + task_state mutation (on temp task_state) + log entry.
- **Full reconcile simulation**:
  - Setup: temp lease (active), temp task_state entry with lease, temp handoff in claimed, temp raw drop file in temp inbox/paste-watcher/raw/ (valid JSON worker_summary + matching lease_id/request_id + verdict=DONE + summary).
  - Invoke Reconcile-AsyncResults.ps1 against the temp structures.
  - Assert: lease status=completed, handoff moved/updated to done with output filled, task_state updated + history appended, scheduler in_flight adjusted (if simulated), raw file moved or marked processed, no errors, Validate called.
  - Idempotency: re-run reconcile → no duplicate changes.
- **Stale handling**:
  - Drop raw for a lease that is already completed/expired or non-existent → assert raw moved to stale/, decisions.md has STALE entry, **no** changes to task/handoff/state.
- **Mode flip script tests**:
  - Negative: Attempt Set-SchedulerMode async with no async runtime enabled or missing adapter dir → fails with clear error (before any write).
  - Positive (synthetic): With mock registry + fake adapter dir + inbox, succeeds, updates temp scheduler_state, logs decision.
- **Structure checks**: Add the full inbox/paste-watcher/{raw,requests,stale,processed} subdirs + .gitkeeps (per the "Inbox Layout for Async Runtimes" subsection in Section 12 and the atomic PR5) to requiredDirs (post real non-trivial sync wave Hard Gate + PR5; use flag/detect presence for backward-compat in earlier PRs).
- **Adapter count**: Update expectedAdapters logic or make it dynamic ("at least claude-code + local-script; paste-watcher allowed post-Phase2"). See PR5 and "Inbox Layout" for the authoritative 4-subdir convention.
- **Cleanup**: All temp artifacts removed even on failure.
- Existing 12 tests remain unchanged and must still pass in pure-sync config.

These extensions run in every self-test invocation post their PR. They validate the new machinery **without** requiring the real paste-watcher adapter (simulations only). Real adapter addition + first exercise happens later in rollout.

---

## 10. Risk Analysis (Focused on Single-Writer Invariant and Cross-Runtime Safety)

### 10.1 Single-Writer Invariant (Highest Severity)
**Threat**: Any path that allows a second writer (human following bad launcher instructions, buggy reconcile script, future adapter) to mutate handoffs/state/logs without the orchestrator.lock or outside the defined scripts.

**Phase 2 specific vectors**:
- paste-watcher human process writes directly to handoffs/claimed/ or leases.json (e.g., "I normalized it for you").
- Reconcile-AsyncResults.ps1 or New-Lease.ps1 writes partial state then crashes (inconsistent caches).
- Orchestrator writes request JSONs in a way that looks like normalized output.
- Multiple concurrent Orchestrator sessions (lock prevents, but stale lock takeover + async work in flight is risky).

**Mitigations (mandatory before mode flip)**:
- **Contract + Adapter Docs**: _EXTENSION_CONTRACT.md + every file in `adapters/paste-watcher/` (launcher.md, onboarding.md, result_contract.md) contains repeated, prominent single-writer warnings + exact "only write raw to inbox/.../raw/" rule. Human must read before first use.
- **Script-only mutations**: All durable writes go through the 4-5 new scripts. No direct JSON editing in prompts. Scripts always acquire/verify lock context.
- **Atomic + validated writes**: Read full file → validate schema where applicable → modify → write to .tmp → Move-Item (or equivalent). On error path: best-effort cleanup + loud log.
- **Extended self-test**: The simulation tests explicitly assert post-reconcile that only expected files under state/handoffs/ were mutated, and raw drops never caused direct writes.
- **Audit on every mutation**: Every script appends before/after summary (or git-diff style) + lease_id/task_id to both logs/orchestrator.log and decisions.md.
- **Stale lock policy (enhanced in Acquire/Release per Section 5)**: Requires explicit human confirmation for takeover when async leases may be in flight or mode=async; recorded with lease snapshot. (See "Updates to existing scripts" in Section 5 and the new "Lease Semantics and Policy" + "Contract Compliance Matrix" in Section 12 for details.)
- **Inbox/raw as the only channel**: Enforced in result_contract.md (filename + content must reference lease; mismatch → stale treatment).
- **Human double-check in first exercises**: Orchestrator always reports "about to process drop X for lease Y — contents look correct?" before reconcile applies.

**Residual risk**: Low after mitigations. Human error is the main remaining vector; mitigated by staged rollout on trivial tasks + review of first drops.

### 10.2 Cross-Runtime Safety and Routing
**Threat**: paste-watcher (or its template) is accidentally or maliciously routed to a role it cannot fulfill (Coder → no edits performed; final Critic → no test execution, fake verdict).

**Mitigations**:
- Accurate, narrow caps in runtimes.json + agents.json (only repo_read/web_research; notes explicitly forbid other uses).
- Safety floor logic in `registry/routing.json` + Orchestrator prompt text (in updated gloam-*.md and agent defs) will skip or warn on unsafe selection.
- First exercise deliberately forces paste-watcher on a pure Researcher task.
- No `can_enforce_scope` / `can_edit_files` / `can_run_tests` → routing.json selection_algorithm + safety_floor will exclude it (orchestrator will implement the pure function described).
- "Capability Truthfulness" section of contract is called out in paste-watcher onboarding.

### 10.3 Other Notable Risks
- **Lease expiry during legitimate long external run**: Generous defaults + Refresh script + status reporting of "age vs expires" + human training. First exercise uses 10-15 min timeout deliberately short to test expiry path too.
- **Request/lease ID mismatch or malformed drop**: Stale handling + validation failures → BLOCKED verdict + human review. Never silent corruption.
- **Mode flip partial state**: Preconditions + script + recorded decision prevent this.
- **Discovery late (no prior app tasks)**: Directly addressed by "staged rollout with trivial async first" and "exercise sync core before flip."
- **Performance/scale**: Irrelevant for Phase 2 (small number of tasks, manual paste).

All risks are acceptable for the bootstrap goal and lower than introducing a full LLM async runtime first.

---

## 11. Staged Rollout Plan (Realistic, Incremental, Safe)

**Prerequisite (current)**: All Phase 1 artifacts present, self-test green, one successful cold resume, zero app tasks run.

**Hard Gate (per _EXTENSION_CONTRACT.md precondition #2 — authoritative)**: Before *any* creation of `agent_collab/adapters/paste-watcher/` (PR5) or mode flip, the Claude-only (sync) path **must** have been exercised against at least one *real, non-trivial task*. This means a bounded but genuine wave involving Coder (or Critic) on actual handoffs/artifacts, integration verification, restart/reconcile, and promotion behavior under claude-code (e.g., a self-contained docs-only enhancement, test-harness utility, or low-risk isolated component change that exercises the full end-to-end sync flow including failure modes). The wave, its handoffs, Critic verdict, promotion (or explicit BLOCKED handling), and restart must be recorded in `agent_collab/logs/decisions.md` with rationale. "Trivial read-only Planner/Researcher" does **not** satisfy this. This gate is non-negotiable for contract compliance; the design does not relax it.

1. **Required Pre-Phase-2 Real Non-Trivial Sync Wave (mandatory gate before any PR5 work)**: After PRs 1–4 land (or in parallel review), human + Orchestrator execute one real non-trivial sync wave as described above (claude-code only, Coder/Critic involved, full verification). Full self-test + lock/restart exercised on the resulting state. Record complete trace (task_ids, handoff paths, Critic verdict, promotion commit, restart diffs) in `decisions.md`. Only after this recorded exercise may the team proceed to PR5 (adapter + registry). This fulfills contract precondition #2 before introducing the async adapter or leases.

2. **PRs 1–4 land** (scripts with testability hooks, Normalize update, docs updates for async procedures, self-test extensions). System remains 100% sync-compatible. Run full self-test (new async sims pass; old tests unchanged). Use this window to complete the required real sync wave (Step 1).

3. **PR 5 lands** (only after Step 1 gate passed + recorded): Registry entries (initially documenting the intended runtime as disabled per contract) + creation of exactly the `agent_collab/adapters/paste-watcher/` folder + 4 files + full inbox/paste-watcher/{raw,requests,stale,processed}/.gitkeep per the new Inbox Layout subsection. Self-test updated to require the new dirs. This is the atomic "second runtime is concrete" step.

4. **PR 6 lands**: Registries fully enabled (paste-watcher + paste-researcher template). Bump schema_version. decisions.md entry. Full self-test must pass.

5. **PR 7 lands**: Set-SchedulerMode.ps1 hardening + any final doc tweaks. Self-test includes flip negative/positive sims.

6. **Mode flip + first trivial async exercise (in one Orchestrator session, post all merges and after real sync wave gate)**:
   - Acquire lock.
   - Run full self-test (green).
   - Call Set-SchedulerMode async (passes all preconditions, including confirmation of the prior real sync wave in decisions.md).
   - Human gives directive for the exact trivial Researcher task via paste-watcher (example: "Researcher: externally count *.md under docs/ and under agent_collab/, return as worker_summary verdict=DONE").
   - Orchestrator: creates task/handoff/lease/request (writes to inbox/paste-watcher/requests/ if used), updates state, releases lock. Reports exact drop filename expected.
   - Human (separate shell/ Claude): performs the trivial work (find/ls), formats per result_contract.md, **drops only the raw file**.
   - New Orchestrator session: /gloam-resume → auto-reap (none), Reconcile-AsyncResults detects drop, normalizes, validates, completes lease, updates handoff/task/scheduler, logs everything.
   - Human inspects: `git status` / `git diff agent_collab/state/ agent_collab/handoffs/`, leases.json (has completed entry), no unexpected files, status report clean.
   - Run self-test again.
   - Record full trace in decisions.md.

7. **Post-exercise stabilization**: 1–3 more trivial async Researcher/research-adjacent tasks (e.g., "summarize environment.md via human read + paste"). Exercise Refresh, a deliberate near-expiry case, and a stale drop case. Only after these does the system proceed to broader real application tasks (initially still preferring claude-code sync for any Coder/Critic work on Source/).

8. **Ongoing**: Use paste-watcher for suitable low-stakes parallel research or command-capture tasks within waves. Never for safety-floor roles. Monitor for any single-writer near-misses.

This rollout satisfies the contract's precondition #2 on real non-trivial sync maturity *before* the async adapter or mode change, then de-risks the new async machinery on throwaway work before it touches production game logic.

---

## 12. Implementation Notes, Open Items, and Future

- **Atomicity in pwsh**: Use `Move-Item` for JSON updates; consider `System.IO.File` replace patterns if needed for Windows. All new scripts share a small internal `Atomic-JsonUpdate` helper pattern (documented in the Implementation Specification Addendum).
- **Request pickup for paste-watcher**: Primary = Orchestrator prints the full JSON + instructions (including exact target filename) in the resume output (human copies). Secondary (optional convenience) = Orchestrator may write `inbox/paste-watcher/requests/<request_id>.request.json`. `requests/` files are transient delivery aids (cleaned on successful reconcile); they are *not* durable protocol state.
- **Timeout policy**: Start conservative (Researcher 30-45 min default). See "Lease Semantics and Policy" subsection below for exact refresh/expires math and per-role defaults. Configurable later via `context/lease_policy.json` if needed.
- **No new protocol artifacts**: As above for requests/. All durable state mutations remain exclusively via the new scripts while the orchestrator.lock is held.
- **Open for later phases**: Real automated async runtimes (following identical contract), dynamic heartbeat pings from workers, richer lease policy file, CI integration of self-test, automated cleanup of old processed/ inboxes.
- **Versioning**: Registry schema_version bumps on paste-watcher addition + any script-induced state shape changes (documented in decisions.md with before/after).

### Lease Semantics and Policy (precise rules — addresses Issue 5)

**lease_id generation** (exact, in New-Lease.ps1 and documented for humans):
```powershell
$lease_id = "lease-${TaskId}-$([guid]::NewGuid().ToString('N').Substring(0,8).ToLower())"
```
Uniqueness checked within active leases for the task_id at creation time.

**Raw result filename convention** (communicated in dispatch output and used by Reconcile):
`${request_id}-$(Get-Date -Format "yyyyMMddTHHmmssZ").raw.json` (preferred) or .txt (with last fenced ```json block extractable). Example: `req-abc123-20260531T203015Z.raw.json`.

**.request.json shape** (when written to requests/): Full `worker_request` JSON (per schema) + `lease_id` + `expires` + human instructions block. Optional for convenience; primary is always the printed directive.

**Exact refresh / expires math** (Refresh-LeaseHeartbeat.ps1 and Reap):
- On New-Lease: `created = now`, `heartbeat = now`, `expires = now + $TimeoutMinutes`.
- On Refresh: `heartbeat = now`; `expires = now + $GraceMinutes` (default: original timeout or 30m policy minimum; sliding window, not fixed from creation).
- Reaper condition (Reap-ExpiredLeases.ps1, run on every async resume):
  ```powershell
  if ($lease.status -eq 'active' -and 
      ($now -gt $lease.expires -or 
       ($now - $heartbeat).TotalMinutes -gt ($lease.expires - $lease.created).TotalMinutes * 1.5)) {  # generous 1.5x heartbeat window
      # mark expired, update task_state + decisions.md
  }
  ```
- "released" status: Supported via new `Release-Lease.ps1` (or Reconcile param `-ReleaseForTask`); used for early human cancel of a paste request. Sets status=released, updates task to blocked with reopen_reason.
- "failed": Set by Reconcile on processing/validation error after a result drop (distinct from expired).

**Per-role defaults** (hardcoded in New-Lease or simple table; override via param):
- Researcher (paste-researcher): 45 min initial, 30 min grace on refresh.
- Other (future): 90–120 min.

**decisions.md entry template** (standardized, used by all new scripts):
```
## YYYY-MM-DDTHH:MM:SSZ — <ScriptName> for lease-xxx / task-YYY
- Before: (key fields or hash)
- Action: ...
- After: ...
- Diff (relevant files): (or "see git commit <sha>")
- Rationale: ...
```

All new scripts support `-DryRun` (log only, no writes) and path overrides for testability.

### Inbox Layout for Async Runtimes (addresses Issue 6)

Per-runtime under `agent_collab/inbox/<runtime-id>/` (created on first use or in adapter onboarding + Reconcile-AsyncResults.ps1 safe mkdir):

- `raw/` — Incoming drops only (workers/human). .gitkeep required. Commit policy: **commit all raw drops as immutable audit artifacts** (referenced in worker_summary.raw_output_path, decisions.md, handoffs). Never .gitignored.
- `requests/` (paste-watcher and future out_of_band) — Transient delivery aids written by Orchestrator (worker_request + instructions). Cleaned by Reconcile on successful processing. Commit for audit during active use; safe to prune processed after 30 days via explicit step.
- `stale/` — Late/mismatched results moved here by Reconcile (never applied). Commit for audit.
- `processed/` — Successfully reconciled raw results moved here (with original name or timestamped). Commit for audit.

**Creation**: Reconcile-AsyncResults and adapter onboarding do `New-Item -ItemType Directory -Force` + touch .gitkeep for each subdir if missing.

**Commit policy**: All under inbox/<id>/ (raw + requests + stale + processed) are committed (they are protocol evidence and support restart/reconciliation). Use explicit archive/cleanup script in future if bloat occurs. Never move raw files to handoffs/archived/ (raw stays under inbox for boundary purity).

**Test-AgentCollabScaffold.ps1**: After PR5, requiredDirs includes the four subdirs + .gitkeep for paste-watcher (and future async runtimes dynamically or via list).

**Move semantics in Reconcile** (exact):
- Valid active result → raw/... -> processed/<original-filename-or-timestamped>
- Stale/mismatch → raw/... -> stale/<original>
- requests/ cleaned (deleted or moved to processed/requests-archive/) on success.

This layout is documented in paste-watcher launcher/onboarding/result_contract.md and added to _EXTENSION_CONTRACT.md guidance in a future minor update if needed.

### Implementation Specification Addendum (addresses Issue 9 + testability)

**Handoff mutation in Reconcile**:
- Handoff files are always named `<handoff_id>.md` (UUID or slug-timestamp) and live in `handoffs/claimed/`, `done/`, etc.
- Orchestrator records the *full relative path* (e.g. "handoffs/claimed/h-uuid123.md") in:
  - `task_state.active.<task_id>.handoff_file`
  - The corresponding `worker_request.handoff_file`
  - Runtime notes in the handoff itself.
- Reconcile uses the recorded `handoff_file` path *directly* for locate + in-place update (status, output_artifacts, verdict, runtime_notes including lease) or move to `done/`. No content search by task_id.

**Normalize-RuntimeResult extension for paste-watcher (sample)**:
Input raw (JSON): `{"request_id":"req-123", "lease_id":"lease-TASK-007-abc12345", "verdict":"DONE", "summary":"...", ...}`
Expected normalized: full worker_summary with `raw_output_path`, `lease_id` populated from input, other fields mapped or defaulted conservatively.

Input raw (fenced .txt): Last ```json { ... } ``` block extracted + same mapping.

**New scripts common requirements** (from PR1):
- All accept `-Root` (defaults to git toplevel), per-file path params, `-DryRun`, `-Verbose`.
- Exit codes: 0=success, 1=validation/stale (non-fatal for some), 2=lock not held when required, 3=IO/schema error, 4=precondition fail.
- Example test invocation: `pwsh -File New-Lease.ps1 -TaskId "TASK-007" -LeasesPath "C:\temp\leases.json" -DryRun`.

**Atomic helper pattern** (pseudocode, to be implemented in each or shared .psm1 if added later):
```powershell
function Update-JsonFile {
    param($Path, $Updater)  # $Updater is scriptblock that receives parsed obj and returns modified
    $orig = Get-Content -Raw $Path | ConvertFrom-Json
    $new = & $Updater $orig
    $tmp = "$Path.tmp"
    $new | ConvertTo-Json -Depth 10 | Set-Content $tmp
    Move-Item $tmp $Path -Force
}
```

Full examples and more pseudocode (Generate-LeaseId, Parse-RawForLease, etc.) live in the script files themselves (header comments) once implemented.

### Single-Writer Warning Boilerplate (Exact Copy-Paste Text — addresses Issue 12)

**For all three new paste-watcher .md files (launcher.md, onboarding.md, result_contract.md) — prominent early section**:
```
**SINGLE-WRITER INVARIANT (NON-NEGOTIABLE — v8.1 + _EXTENSION_CONTRACT.md)**:
This adapter and *any* human or process using it for paste-watcher MUST write *ONLY* raw result files to `agent_collab/inbox/paste-watcher/raw/<exact-request-filename>`.
NEVER create, edit, move, normalize, or touch any file under:
- handoffs/ (claimed/done/blocked/archived)
- state/ (leases.json, task_state.json, scheduler_state.json, orchestrator.lock)
- outbox/
- logs/ (except via explicit Orchestrator script append)
- decisions.md
- Any other protocol artifact.

Violations are correctness bugs, audit failures, and corruption vectors. Only the session holding `orchestrator.lock` (the Orchestrator) may write durable state. The raw inbox/ boundary exists precisely to enforce this. Report any accidental writes immediately.
```

**Shorter version for updates to HANDOFF_TEMPLATE.md, gloam-*.md, restart_instructions.md, and existing launcher.md files** (add in relevant sections):
```
Reminder (single-writer): Workers and paste-watcher drops write *only* to inbox/<runtime>/raw/. All state/handoff mutations are Orchestrator-only while lock is held.
```

**Codified "human double-check" step** (add as explicit numbered sub-step in restart_instructions.md Step 3 and gloam-resume.md Reconcile section):
"3c. For every candidate raw drop from an async runtime: 
   - Parse lease/request from filename + content.
   - If active lease: Display to human: 'About to process drop <fullpath> for lease <lease_id> (task <task_id>, role <role>, raw verdict <verdict>, summary excerpt: <first 200 chars>). This came from external human execution per the request? Contents appear correct and match expected request? (yes / no / abort / treat as stale)'
   - Only proceed with Normalize/Validate/apply on explicit recorded 'yes'. Log the confirmation in decisions.md."

**Exact self-test mutation assertions** (in the async simulation section of Test-AgentCollabScaffold.ps1):
```powershell
# After Reconcile invoke on $temp tree
$afterFiles = Get-ChildItem $tempRoot -Recurse -File | Select -Expand FullName
$changed = Compare-Object $beforeSnapshot $afterFiles -PassThru
$allowed = @("state/", "handoffs/", "logs/", "decisions.md")  # plus inbox/ moves out of raw/
$violations = $changed | Where { $_.SideIndicator -eq "=>" -and -not (allowed patterns match) }
if ($violations) { Fail "Unexpected mutations outside allowed: $violations" }
# Also assert raw files only moved (not content-edited in place) and no new files in handoffs outside the one updated handoff.
```

### Contract Compliance Matrix (addresses review recommendation)

| v8.1 / _EXTENSION_CONTRACT.md Rule | Design Element Enforcing It | Notes / Any Deviation |
|------------------------------------|-----------------------------|-----------------------|
| Only Orchestrator writes durable state | All mutations via new scripts (New-Lease etc.) while lock held; single-writer boilerplate in every adapter file + docs; self-test mutation assertions on temp trees; Reconcile never writes raw to handoffs/state. | None. Strengthened with Acquire updates for async takeover. |
| Registry entries "done first" before adapter dir | PR5 explicitly does registry (disabled) + adapter as atomic step; Set-SchedulerMode + self-test preconditions check registry presence; contract text quoted. | None (Issue 2 addressed by reordering). |
| Exactly one async adapter; no scaffolding speculative | PR5 creates only paste-watcher; "exactly one" language throughout; no other adapter dirs planned or mentioned. | None. |
| Leases/heartbeats/expiry *only* for async mode; mode declared in scheduler_state | Leases.json notes + Reap/Reconcile early-exit or no-op on sync; Set-Mode + preconditions; status reports always surface mode + leases. | None. |
| Watcher writes *only* to inbox/<id>/raw/; never normalized artifacts | result_contract.md + launcher.md strict rules + boilerplate; Reconcile treats anything else as stale; no Normalize in adapter. | None. |
| Precondition #2: real non-trivial sync task before adapter/async | Hard gate in Section 11 Step 1 + rollout; required before PR5; recorded in decisions.md; updated objectives/Executive/Key Decisions. | None (Issue 1 addressed by elevation to mandatory). |
| Design for N; implement two (claude + local-script) now | paste-watcher is the third total / first async; comparison table in 4.1 weighs promoting local-script but rejects for clean separation. | None. |
| No worker spawning workers; safety floors in routing | Unchanged registries + routing logic; paste-watcher caps deliberately narrow (no edit/test floors); explicit notes in agents.json. | None. |

Any future deviation would require explicit sign-off and matrix update.

(End of new subsections.)

---

## Key Decisions

- **Decision: "paste-watcher" is the sole first async runtime exercised in Phase 2.**  
  **Rationale**: Exactly matches the v8.1 "exactly one" + "low-risk starter such as a file-drop / manual-paste watcher" guidance. Zero external surface area, fully exercises every required async mechanism (leases, heartbeats/expiry, fire-and-forget, reconcile on resume, stale handling, normalization boundary) while obeying the single-writer rule and _EXTENSION_CONTRACT.md preconditions to the letter. Allows the mandatory trivial async exercise immediately after flip, before any real Coder work. Higher-value async runtimes (LLM-based) come after the protocol machinery is proven.

- **Decision: Lease lifecycle, expiry detection, and async result collection are implemented exclusively as new dedicated PowerShell scripts (`New-Lease.ps1`, `Reap-ExpiredLeases.ps1`, `Reconcile-AsyncResults.ps1`, etc.) invoked by the Orchestrator while holding the lock.**  
  **Rationale**: Directly enforces "Only the Orchestrator writes durable state" and prevents any watcher/adapter from becoming a second writer. Scripts are unit-testable in isolation (via extended self-test simulations) and match the existing pattern (Acquire/Release, Normalize, Build-Wave, Validate are all separate helpers). No daemons, no in-adapter state mutation.

- **Decision: The `scheduler_state.mode` flip occurs only via the new `Set-SchedulerMode.ps1` script with strict precondition validation; never manual edit.**  
  **Rationale**: Eliminates partial-flip or "async mode but no adapter/inbox" failure modes. Preconditions tie the flip directly to the presence of the concrete adapter + enabled registry entry + inbox dirs (per contract). Flip is a deliberate, auditable Orchestrator action recorded in decisions.md. Keeps the system safely in sync until the exact moment the team is ready.

- **Decision: The paste-watcher adapter folder is created in its own late PR (after core async scripts + docs + self-test extensions are merged and green), and only when the team is prepared to execute the first trivial async exercise in the same or next session.**  
  **Rationale**: Strictly follows `_EXTENSION_CONTRACT.md` ("Do not create adapter folders for speculative runtimes" and the three preconditions, including "ready to switch mode and implement lease/heartbeat/reconcile logic"). Early PRs deliver reusable, backward-compatible async infrastructure (scripts + tests + docs) that do not depend on the specific runtime. The adapter PR is the point of no return for Phase 2.

- **Decision: First real application tasks (any work touching Source/ or affecting game logic) will continue to use only the claude-code runtime (even after mode=async); paste-watcher is initially restricted to Researcher / read-only report tasks.**  
  **Rationale**: The paste-watcher deliberately declares (and provides) zero edit/scope/test capabilities. Using it for Coder/Critic would violate safety floors and the "exercise machinery before using it for real Coder/Critic work" rollout requirement. This also gives multiple successful trivial async cycles to shake out any reconcile/expiry/stale bugs on non-critical work. Per contract precondition #2, a real non-trivial sync wave exercising Coder/Critic + verification + restart must precede the adapter/PR5.

- **Decision: The paste-watcher adapter is the chosen bootstrap (see expanded alternatives analysis in 4.1); promoting the existing local-script runtime as the "first out_of_band" was explicitly evaluated and rejected for Phase 2.**  
  **Rationale**: While local-script is already fully concrete/implemented/in-registries/self-tested, its current "runner" launch_mode and deliberately narrow caps (no can_return_schema, intentionally fails edit floors) are designed for bounded *synchronous* invocation within the Orchestrator session. Making it cleanly out-of-band would require non-trivial changes to its launcher/runner_config semantics and risk diluting its safety intent. A dedicated purpose-built paste-watcher provides a minimal, zero-side-effect pure async exerciser that leaves the two existing runtimes untouched. This preserves the "exactly one async adapter" discipline and "do not build for runtimes not about to be used" rule while still honoring "design for N". See Section 4.1 comparison table.

- **Decision: Existing schemas (`lease.schema.json`, `worker_request.schema.json`, `scheduler_state.schema.json`, etc.) and `Normalize-RuntimeResult.ps1` skeleton are sufficient; no schema_version bumps or new protocol files required for Phase 2 core.**  
  **Rationale**: Phase 1 already prepared the async fields (lease_id, mode enum, empty leases.json). Changes are in behavior (scripts + docs + reconcile logic), not data shape. Minimizes diff and validation churn. Any minor normalization improvements stay within the existing generic script.

- **Decision: Hybrid sync/async waves and delegations are supported after the flip (claude-code native subagents can still be spawned in-session for sync feel; paste-watcher is always out-of-band).**  
  **Rationale**: Matches the v8.1 definition ("Asynchronous (when **any** runtime returns out-of-band)"). Allows gradual adoption and keeps the powerful claude-code path fully usable. Orchestrator (following updated docs) decides per-delegation based on routing + runtime launch_mode.

---

## PR Plan

**Revised for review feedback (Issues 2, 7, and specificity)**: The 7 PRs remain small and ordered for shippability, but "independently mergeable" is qualified due to cumulative touches on shared files (`Test-AgentCollabScaffold.ps1` touched in PR1/2/4/5/6; `decisions.md` appended in every PR; gloam-*.md + restart_instructions.md across PR3/7). Early PRs still deliver standalone value (new scripts are usable in sim mode even if Phase 2 paused). 

**Mandatory gate after *every* PR (including this plan's PRs)**: 
- Re-run `pwsh -NoProfile -File agent_collab/scripts/Test-AgentCollabScaffold.ps1` in full.
- Confirm zero regression in pure sync paths (all  original 12 tests + any new sims that are skipped in sync mode must pass).
- Append "PR N green on <date>; sync paths verified; no unexpected adapter dirs" to `agent_collab/logs/decisions.md`.
- For PRs touching Test or docs: reviewer focus on the shared-file diff (use stacked branches or clear "focus: Test-AgentCollabScaffold.ps1 cumulative" labels).

**PR ordering and explicit dependencies** (more precise than initial draft):
- PR1 (scripts + initial Test hooks) → PR2 → PR4 (full sims + Set-Mode skeleton) must precede heavy doc references in PR3.
- PR5 (registry "disabled" entry per contract "done first" + atomic adapter creation + inbox layout subdirs + .gitkeeps) is the "concrete runtime" step; requires PR1–4 + the required real non-trivial sync wave gate (Issue 1).
- PR3 docs can be split or combined; PR7 is final polish only.

**PR 1: Add core lease management scripts (New, Refresh, Reap, initial Reconcile) + testability hooks + initial Test extensions**  
**Files affected**:
- `agent_collab/scripts/New-Lease.ps1` (new; *must* include -Root/-LeasesPath/-TaskStatePath/-HandoffsRoot/-InboxRoot/-DryRun params from day 1, defaulting to live repo paths; shared Atomic-Update-Json helper or pattern)
- `agent_collab/scripts/Refresh-LeaseHeartbeat.ps1` (new; same override params)
- `agent_collab/scripts/Reap-ExpiredLeases.ps1` (new; same + precise expiry math per Lease Semantics subsection)
- `agent_collab/scripts/Reconcile-AsyncResults.ps1` (new, initial with -DryRun + overrides; supports temp trees for sims)
- `agent_collab/scripts/Test-AgentCollabScaffold.ps1` (extend with initial synthetic lease/reconcile tests using temp trees + override params; add note on backward-compat for future adapter checks)
- `agent_collab/logs/decisions.md` (append Phase 2 PR1 entry)
**Dependencies**: None (additive; scripts usable with overrides for testing even if later PRs delayed; current sync behavior untouched).  
**Description + gates**: Implements lease lifecycle + built-in testability (addresses Issue 3). Self-test extensions run cleanly in sync-only mode using temp state. Post-merge gate (above) mandatory. Prepares reusable infrastructure. (See Section 5 for exact param requirements and Implementation Specification Addendum for formats.)

**PR 2: Enhance Normalize-RuntimeResult.ps1 for async runtimes and lease awareness**  
**Files affected**:
- `agent_collab/scripts/Normalize-RuntimeResult.ps1` (update mapping, add paste-watcher / lease_id / fenced-JSON extraction logic)
- `agent_collab/scripts/Test-AgentCollabScaffold.ps1` (add tests for the enhanced normalizer with sample raw drops)
- `agent_collab/logs/decisions.md` (entry)
**Dependencies**: PR 1 (tests benefit from lease concepts, though normalizer is independent).  
**Description**: Makes the existing skeleton robust enough for real raw drops from out-of-band runtimes while remaining conservative (BLOCKED defaults on uncertainty). Adds no new files. Fully backward compatible for local-script.

**PR 3: Update all restart, command, and onboarding documentation with async procedures**  
**Files affected**:
- `agent_collab/context/restart_instructions.md` (detailed async reconcile steps, lease lifecycle subsection)
- `agent_collab/adapters/claude-code/commands/gloam-resume.md` (async sections + examples)
- `agent_collab/adapters/claude-code/commands/gloam-status.md` (leases + per-runtime inbox in dashboard)
- `agent_collab/adapters/claude-code/commands/gloam-start-task.md` (async delegation path)
- `agent_collab/adapters/claude-code/onboarding.md` (update "Adding a Second Runtime" + Phase 2 note)
- `agent_collab/adapters/claude-code/launcher.md` and `local-script/launcher.md` (minor async awareness notes)
- `agent_collab/handoffs/HANDOFF_TEMPLATE.md` (minor clarification)
- `agent_collab/logs/decisions.md` (entry)
**Dependencies**: None (documentation only; does not change behavior).  
**Description**: Fills in the descriptive "if async" branches with concrete, script-name-specific steps. Sync paths remain identical. Prepares humans for the later flip and exercise. Independently reviewable.

**PR 4: Complete self-test async simulation coverage + Set-SchedulerMode.ps1 skeleton + Acquire/Release async enhancements**  
**Files affected**:
- `agent_collab/scripts/Test-AgentCollabScaffold.ps1` (full async simulation battery: lease full lifecycle, reconcile happy/stale/expiry paths, mode flip negative cases, idempotency)
- `agent_collab/scripts/Set-SchedulerMode.ps1` (new; implements all preconditions, validation, atomic update, logging — but flip still blocked by registry checks until later PRs)
- `agent_collab/scripts/Acquire-OrchestratorLock.ps1` (update; minimal async-aware stale takeover: -ForceTakeover / interactive confirm + decisions.md lease snapshot when mode=async or active leases; cheap mode/leases read)
- `agent_collab/scripts/Release-OrchestratorLock.ps1` (update; symmetry for async takeover logging)
- `agent_collab/logs/decisions.md` (entry)
**Dependencies**: PR 1 + PR 2 (for reconcile/normalize in sims); PR 3 helpful for context but not required.  
**Description**: Self-test now exercises 100% of the new machinery synthetically (no real async runtime or mode flip yet). Set-SchedulerMode script exists and is tested for error paths. Acquire/Release receive the minimal targeted async safety updates detailed in Section 5. System remains sync-only. This PR makes the async infrastructure "ready to exercise."

**PR 5: Registry entries (documenting intended runtime, initially disabled per contract "done first") + atomic creation of exactly one paste-watcher adapter + inbox layout subdirs (the "second runtime is concrete" step)**  
**Files affected**:
- `agent_collab/registry/runtimes.json` (add paste-watcher entry with accurate caps, initially "enabled" via comment or disabled flag per contract guidance; bump schema_version)
- `agent_collab/registry/agents.json` (add paste-researcher template, initially disabled; bump)
- `agent_collab/adapters/paste-watcher/runner_config.json` (new)
- `agent_collab/adapters/paste-watcher/launcher.md` (new, with exact single-writer boilerplate + drop instructions + filename formats)
- `agent_collab/adapters/paste-watcher/onboarding.md` (new)
- `agent_collab/adapters/paste-watcher/result_contract.md` (new, precise raw shapes + lease matching + boilerplate)
- `agent_collab/inbox/paste-watcher/{raw,requests,stale,processed}/.gitkeep` (new; full layout per Inbox Layout subsection)
- `agent_collab/scripts/Test-AgentCollabScaffold.ps1` (add the new inbox subdirs + adapter to required structure checks; make adapter check allow the known Phase-2 runtime)
- `agent_collab/logs/decisions.md` (detailed entry noting contract compliance, "registry first", and confirmation of prior real non-trivial sync wave gate)
**Dependencies**: PR1–PR4 + **completion + recording of the required real non-trivial Claude-only sync wave** (per Issue 1 / contract precondition #2 / hard gate in Section 11). Self-test ready for new dirs.  
**Description**: Satisfies contract "Registry Entries (done first)" literally by co-locating or preceding the adapter creation in this atomic PR. Creates exactly one async adapter (no more). No mode flip. Human review of full adapter + registry entry is the gate. "When a second runtime is concrete" step, now correctly sequenced. (See Section 4.3 and Inbox Layout.)

**PR 6: Enable paste-watcher in registries + schema bumps + decisions log**  
**Files affected**:
- `agent_collab/registry/runtimes.json` (add paste-watcher entry with accurate caps; bump schema_version)
- `agent_collab/registry/agents.json` (add paste-researcher template; bump schema_version)
- `agent_collab/registry/routing.json` (optional note on paste-watcher limitations, no functional change)
- `agent_collab/logs/decisions.md` (full rationale + date + "Phase 2 runtime enabled")
- `agent_collab/scripts/Test-AgentCollabScaffold.ps1` (final adapter count / enabled runtime expectations if needed)
**Dependencies**: PR 5 (adapter must exist before enabling).  
**Description**: Makes the runtime and template selectable by routing. Still sync mode. Self-test passes. Now "ready to switch" per contract.

**PR 7: Final integration, mode-flip script hardening, and rollout documentation**  
**Files affected**:
- `agent_collab/scripts/Set-SchedulerMode.ps1` (harden preconditions to require the now-enabled paste-watcher adapter + inbox + registry entry)
- `agent_collab/context/restart_instructions.md`, gloam-*.md files (final polish / examples using the real runtime name)
- `agent_collab/adapters/paste-watcher/*` (any last clarifications from review)
- `agent_collab/logs/decisions.md` (pre-flip entry)
- (No behavior change to sync path)
**Dependencies**: PR 6.  
**Description**: Makes the flip script fully functional now that the adapter + registries are present. Any final doc tweaks from PR reviews. Prepares the exact commands for the flip + first trivial async exercise. Independently mergeable as long as prior PRs are in (or can be sequenced).

**Post-PR-7 (not a PR — execution in the repo)**: In a dedicated Orchestrator session (lock held, self-test green): run `Set-SchedulerMode.ps1 -Mode async`, perform the first trivial async paste-watcher Researcher exercise, verify, record in decisions.md, then proceed to real application tasks (preferring claude-code for safety-floor roles).

These 7 PRs are small, focused, and ordered so that the system is always in a shippable state. Early PRs deliver value even if Phase 2 is paused. The critical "create adapter" and "flip mode" steps are isolated and late.

---

**End of Design Document**

*This document was produced per the assigned task. All recommendations strictly respect the v8.1 spec, the _EXTENSION_CONTRACT.md (cited verbatim where authoritative), the current scaffold state (exact paths), and the "implement exactly one" + "low-risk starter" constraints. No multiple async adapters are planned or recommended. The first async exercise is deliberately trivial and non-state-affecting.*

---

## Appendix: Exact File Paths Referenced (for implementers)

**Core existing (Phase 1)**: `agent_collab/adapters/_EXTENSION_CONTRACT.md`, `agent_collab/registry/{runtimes,agents,routing,capabilities}.json`, `agent_collab/state/{scheduler_state,leases,task_state,orchestrator.lock,*.example}.json`, `agent_collab/protocol/{lease,scheduler_state,task_state,worker_request,worker_summary,handoff,critic_verdict,planner_output}.schema.json`, `agent_collab/scripts/{Test-AgentCollabScaffold,Normalize-RuntimeResult,Build-WavePlan,Acquire-OrchestratorLock,Release-OrchestratorLock,Validate-JsonSchema,Assert-EditScope,Assert-BashPolicy}.ps1`, `agent_collab/context/{restart_instructions,project_goal,agent_rules,environment,scope_roots,command_policy}.md` + `.json`, `agent_collab/adapters/claude-code/{launcher,onboarding,settings}.md/.json` + `commands/{gloam-resume,gloam-status,gloam-start-task}.md` + `agents/gloam-*.md`, `agent_collab/adapters/local-script/{launcher,runner_config}.md/.json`, `agent_collab/handoffs/HANDOFF_TEMPLATE.md`, `agent_collab/logs/{orchestrator.log,decisions.md}`, `agent_collab/inbox/*/raw/.gitkeep`, `.claude/` (projection), `.gitignore`.

**New in Phase 2 (via the PRs above)**: The four lease/reconcile scripts + Set-SchedulerMode.ps1 under `scripts/`, the entire `adapters/paste-watcher/` tree, inbox/paste-watcher/raw/.gitkeep, targeted updates only to the listed docs and Test script.

*Implementers must re-run the full self-test after every change and before any mode flip or real task.*
