# Agent Collaboration Rules for Gloamstead (Multi-Runtime UE5 Vertical Slice Factory)

These rules apply to all participants regardless of which runtime is performing which logical role. They are in addition to per-runtime adapter prompts and registry definitions.

## Foundational Rule: Role Independence from Runtime
Logical role != runtime.

Any enabled runtime that declares support and has the required capabilities may perform any role (including Orchestrator) after successfully acquiring the exclusive Orchestrator lease.

A runtime acting as a worker must never implicitly gain Orchestrator authority. It must separately acquire the lease before performing Orchestrator actions.

## UE5 Content-Generation Rule (mandatory)
Agents must not manually place assets or directly edit, patch, or synthesize Unreal binary files (.uasset, .umap, .ubulk, .uexp, etc.) through file-editing tools.

Binary content may only be produced when **all** of the following are true:
- The active handoff explicitly permits generated binary outputs.
- The exact output paths are assigned in generated_output_ownership.
- The paths are inside approved coder_generated_output_roots.
- An approved Unreal automation command (Editor or command-line) produced the changes on the isolated task worktree.
- The task branch owns those paths.
- The Critic audits the resulting changed binary files.
- Required map-load and candidate integration verification passes.

## Vendor and Third-Party Content
- All Marketplace, Fab, sample, and vendor content (e.g. Content/ThirdPerson, Content/Characters) is **read-only**.
- Never modify vendor packs in place.
- Integration must occur through project-owned adapters, manifests, derived assets, or generated content **outside** vendor roots.
- Asset acquisition, license acceptance, plugin installation, and engine upgrades **always require human approval** (autonomy_policy "ask" or human gate).

## Role Boundaries (strict, runtime-independent)
- **Only the invocation currently holding the active Orchestrator lease** may delegate, update durable state (state/, handoffs/, outbox/, logs/decisions.md, audit.jsonl), manage leases, create worktrees/candidates, promote branches, or record human playtest results.
- Workers (any role other than the active Orchestrator) must never delegate, never modify durable collaboration state, never write durable outbox records, and must return structured results for validation by the active Orchestrator.

**Minimal roster (per UE5-Agent-Substrate-Review.md)**: orchestrator, planner, coder, critic. Architect/researcher/documentor are deprecated as first-class roles. Use the playbooks under agent_collab/playbooks/ (architecture-analysis, external-research, documentation-update) as steps inside Planner or post-promotion Orchestrator actions. See workflow_activation.json for when Planner is required vs optional for small tasks.

- **Coder**: edits only inside assigned file_ownership within coder_edit_roots. Produces generated binaries only inside assigned generated_output_ownership via approved automation. Never edits binaries directly. Never touches docs or vendor content.
- **Critic**: read-only. Must return APPROVED / REJECTED / BLOCKED with evidence. Must reject on scope violations, vendor changes, missing/failed required verification, unrecorded human playtest when required, etc. Independence from the producing coder is mandatory.
- **Planner**: must produce DAGs that avoid overlapping file_ownership or generated_output_ownership for parallel tasks and must separate text-edit, generation, validation, packaging, and human-playtest concerns. May incorporate architecture-analysis and external-research playbook outputs as structured fields or attachments. Planner is not required for every small single-file owned text task (see workflow_activation.json).
- (Deprecated roles converted to playbooks: see agent_collab/playbooks/ and the review document. Their old prompts remain for transition reference only.)

## Git & Worktree Model
- All Coder work happens in isolated git worktrees under the task branch (agent-collab/<slug>/task/<task_id>).
- Task branches are **never** merged directly into the work branch (agent-collab/<slug>/work).
- Always merge approved task branches into a candidate/<wave_id> first.
- Run all required verification profiles on the candidate.
- Promote to work branch **only** when candidate is green + Critic APPROVED.
- Never push, PR, rebase, amend, hard-reset, rewrite history, or modify remotes.
- Never copy or track transient Unreal dirs (Intermediate/, Saved/, DerivedDataCache/, Binaries/).

## Verification and Human Gates
- Use only verification profiles from verification_profiles.json.
- Unavailable profiles block promotion for tasks that require them.
- When requires_human_playtest: true, the active Orchestrator must stop and obtain/record a human result in outbox/playtest/ before marking complete.
- All high-privilege actions (generated binary, packaging, etc.) require explicit handoff + ownership + audit.

## Autonomy & Human Gates
Follow autonomy_policy.json exactly. The active Orchestrator (lease holder) must stop for any high-risk, ambiguous ownership, human-gated action, unavailable required runtime/command, repeated failures, or pending human playtest.

## Restart & Reconciliation (runtime-neutral)
On every activation as Orchestrator:
- Verify the runtime supports Orchestrator role + has required capabilities.
- Acquire the exclusive Orchestrator lease.
- Read all context, registry, state, leases, and logs.
- Reconcile against Git + worktrees + handoffs + outbox + candidate branches + actual generated files + runtime_raw + playtest records.
- State JSONs are caches; rebuild from ground truth as needed.
- Report status.
- Continue only safe approved work or wait.

Git, handoffs, outbox (incl playtest/), leases, branches, worktrees, candidates, and actual generated files are ground truth.

## Hard Rules (non-negotiable)
- No direct binary Unreal asset editing.
- No vendor content modification.
- No generated output outside approved roots + explicit handoff.
- No task branch promotion to work without candidate + verification + Critic APPROVED.
- No human-playtest-required task marked complete without recorded result.
- Workers never write durable state or delegate.
- Coder never edits docs or vendor content.
- Documentor never edits anything but docs/.
- Only the lease holder performs Orchestrator actions.
- No push/PR/rebase/amend/hard-reset/rewrite.
- No invention of commands or runtime availability.

Violations must result in BLOCKED by Critic and be logged.

These rules enable safe scaling of the UE5 vertical-slice factory across multiple runtimes.
