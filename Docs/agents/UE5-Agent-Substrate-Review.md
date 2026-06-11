# UE5 Agent Collaboration Substrate Review — Gloamstead

**Date**: 2026-06 (analysis performed on current agent_collab v8.1 state)  
**Scope**: Current roster, architecture, policies, scripts, registries, handoffs, verification, and UE5-specific constraints in this repository.  
**Mandate**: Diagnose as a draft. Reduce sprawl. Clarify boundaries. Recommend minimal reliable operating model. Prefer non-agent mechanisms (skills, playbooks, rubrics, policies, action adapters, gates, dashboards) over new roles. Be specific to Unreal Engine 5 realities.

This review follows the exact structure and distinctions required. It is deliberately critical and architecture-focused rather than collaborative cheerleading.

---

## 1. Executive diagnosis

The current `agent_collab` substrate is one of the more disciplined agentic systems observed for game development. It correctly separates logical roles from runtimes, enforces a single-writer Orchestrator via exclusive lease, uses isolated worktrees for mutation, and bakes in a hard "no direct Unreal binary editing" rule with vendor immutability and explicit generated_output_ownership. The protocol (schemas, handoffs, outbox, reconciliation from ground truth) and defense-in-depth guards (Assert-EditScope.ps1, Assert-BashPolicy.ps1, scope_roots.json, command_policy.json, content_policy.json) are substantive, not theater.

However, the system still carries draft-era role sprawl. Seven logical roles exist when four deliver the necessary authority boundaries for UE5 work. Architect, Researcher, and Documentor are narrow read-only or post-facto activities whose value does not justify the handoff, lease, routing, and invocation overhead of first-class roles. They are better expressed as skills, rubrics, checklists, or steps inside Planner/Orchestrator.

UE5 realities amplify the problem:
- Most verification profiles (compile, editor-generation, map-load, automation-test, package-smoke, candidate-integration UE portions) are marked "unavailable" because no safe command-line entry points or project-owned automation have been discovered. Human execution + evidence capture is required for anything that touches build or content generation.
- Iteration loops are long (editor launch, PIE, C++ compile, cook). A "small bug fix" in a single .cpp/.h pair should never require the full roster or async machinery.
- Generated content (especially under Content/Data/ for rituals/catalogs, PCG outputs) must be produced by automation on a task worktree, not text synthesis. The substrate already understands this; the roster does not yet minimize around it.
- Git LFS + large binary Content/ makes every scope violation expensive to recover from.

The substrate is over-indexed on "more distinct agents = more capability" and under-indexed on "make the common small-text + human-gated-UE-verification path as lightweight as possible." The good news is that the policy, adapter, and reconciliation machinery is already strong enough to support a smaller roster without losing safety.

**Core finding**: The current design is 70% of the way to a reliable UE5 operating substrate. The remaining 30% is ruthless pruning of roles into lower-privilege mechanisms + explicit UE5 workflow activation matrices + living human-approval boundary documentation.

---

## 2. Capability classification table

| Capability (current or recommended) | Classification | Rationale (UE5-specific) |
|-------------------------------------|----------------|--------------------------|
| orchestrator (lease, single writer of state/handoffs/outbox/logs, delegation, candidate promotion, playtest recording) | core agent role | Non-negotiable authority boundary. Only holder may mutate durable collab substrate. |
| planner (DAG + file_ownership_prediction + generated_output_ownership_prediction + verification profiles + acceptance_criteria) | core agent role | High value separation. Predicting narrow ownership in a mixed C++/Blueprint/PCG project prevents parallel task collisions that are painful under LFS. |
| coder (worktree-isolated text edits under strict file_ownership + approved generated binary production via UE automation) | core agent role (domain-specialized for UE5) | The only role that mutates project source under control. UE5 caps (ue5-cpp, ue5-config, ue5-pcg-integration, ue5-editor-automation, generated-content-production) are correctly attached here. |
| critic (read-only scope_audit + integration_verification on candidate; returns APPROVED/REJECTED/BLOCKED with evidence) | core agent role | Independent review gate is essential when compile/PIE feedback is slow and expensive. Must remain separate from the worker that produced the candidate. |
| architect (cross-cutting design/ADR proposals) | skill/playbook + rubric (recommend converting) | Cross-cutting concerns (data contracts, event boundaries, save schema, PCG subsystem interfaces) are real, but do not require a persistent handoff role. Better as a structured "architecture analysis" rubric that Planner must follow on first tasks of a wave, or an Orchestrator-invoked step that emits an ADR attachment. |
| researcher (external investigation: docs, packs, conventions, engine features) | skill/playbook (recommend converting) | Pure read-only, highly parallelizable. No authority, no mutation. Can be a reusable research playbook + MCP/tool usage (web_search, open_page, x_*, etc.) invoked by Planner or Orchestrator. Dedicated role adds ceremony for little gain. |
| documentor (serial docs/ edits only after verified integration + docs_impact) | skill + checklist + post-promotion action (recommend converting) | Edits only documentor_edit_roots. Low risk. Serial nature makes it a natural checklist item or Orchestrator-run post-promotion step rather than a full agent role with its own routing/lease surface. |
| human-playtest (capture structured feedback) | human approval gate + dashboard control | Correctly not a role. Human performs; Orchestrator only records to outbox/playtest/. Verification profile already treats it as mandatory gate when flagged. |
| text-only verification profile | policy + verification profile | Safe autonomous path for pure source/config changes. |
| compile / editor-generation / map-load / automation-test / package-smoke / candidate-integration (UE portions) | human approval gate + action adapter (future) + verification profile | Currently unavailable for autonomy. Must remain explicit human gates + evidence until real commandlet/automation wrappers exist and are whitelisted per-handoff. |
| git worktree management, lock/lease acquire/release, Assert-EditScope, Assert-BashPolicy, Assert-ActionPolicy, Reconcile-*, Build-WavePlan, Normalize-RuntimeResult | action adapter (policy-checked) | Excellent current state. These are controlled side-effect bridges, not agent reasoning. |
| scope_roots.json, content_policy.json, command_policy.json, autonomy_policy.json, verification_profiles.json | policy gate | UE5-specific strength (vendor roots, forbidden transients, no direct binary, LFS awareness, "ask" for plugins/engine/assets). |
| registry/* (roles, agents, capabilities, runtimes, adapter_matrix) + Select-Runtime.ps1 | policy + routing control | Prevents "every agent gets every MCP/tool". |
| /gloam-resume, /gloam-status, /gloam-start-task (skills) | skill/playbook | Correct use of non-agent mechanism for common Orchestrator entry points. |
| leases.json + Reap/Reconcile async machinery (when exercised) | dashboard control + policy gate (async) | Good for out-of-band; must not relax single-writer. |

---

## 3. Recommended minimal roster

**Final minimal logical roster (4 roles)**:

1. **orchestrator** — (unchanged, core). Lease holder. Single writer of durable protocol state. Delegates, reconciles, promotes candidates, records human gates. Any compatible runtime may hold it.
2. **planner** — (core). Produces DAG with narrow file_ownership, generated_output_ownership, required_verification_profiles, risk_level, acceptance_criteria. May incorporate architecture notes and research findings as attachments or structured fields. Read-only.
3. **coder** — (core, UE5-specialized). Isolated worktree execution. Edits only assigned file_ownership inside coder_edit_roots (Source/, Config/). Produces generated binaries only inside explicitly assigned generated_output_ownership via handoff-approved UE automation on the worktree. Never direct binary writes.
4. **critic** — (core). Independent read-only auditor. Task-branch audit + authoritative candidate integration verification. Returns exactly one of APPROVED / REJECTED / BLOCKED with evidence. Enforces scope, vendor immutability, verification profile execution, and ownership. Trusted runtime only for final integration Critic.

**Roles to remove from first-class status** (convert, see section 4):
- architect
- researcher
- documentor

No new roles (playtester, build-runner, integration-orchestrator, etc.). Add no "ue5-specific" agent roles. UE5 distinctions live in capabilities, verification profiles, scope_roots, and workflow activation matrices.

---

## 4. Merge/split/remove decisions

| Current Role | Decision | Justification (UE5) |
|--------------|----------|---------------------|
| architect | Convert to skill/playbook + "Architecture Analysis Rubric" (attach to Planner output or first tasks of a wave) | Cross-cutting design (data contracts for NightConsequenceCatalog, PCG subsystem boundaries, ritual save schema, restoration event model) is valuable but does not create an independent authority boundary or timing requirement that justifies a separate handoff. Planner already does ownership and dependency reasoning; folding architecture notes in reduces one serial hop. |
| researcher | Convert to skill/playbook ("External Research Playbook") invocable by Planner/Orchestrator | Pure information gathering. No mutation, no ownership assignment, highly parallel. Using a dedicated role for "read Unreal docs or Fab pack notes" creates unnecessary handoff surface in a system where most real work is already throttled by human verification gates. |
| documentor | Convert to post-promotion checklist + narrow skill/action ("Documentation Update Playbook") | Only touches docs/. Executed serially after Critic APPROVED + promotion when docs_impact=true. This is a classic "after green, do the docs" step. Better expressed as acceptance criteria item + Orchestrator skill than a routed agent role. Eliminates one more template in agents.json and one more projection maintenance burden. |
| coder | Keep (specialize further in capabilities if needed) | The mutation boundary is real and high-risk under UE5 (C++ hot-reload semantics, Blueprint class defaults, PCG graph determinism, LFS binary bloat on mistakes). |
| critic | Keep | Independence of review is the primary control against self-approval and scope creep on expensive-to-verify UE changes. |
| planner | Keep | Ownership prediction is the key defense against parallel collision in a project with intertwined C++, BP, PCG, and Data Assets. |
| orchestrator | Keep (strengthen documentation of authority) | The lease + single-writer rule is the cornerstone. |

**Splits considered and rejected**:
- Splitting "coder" into "cpp-coder" + "blueprint-coder" or "pcg-coder": No. The handoff already declares exact file_ownership and required capabilities. Further role fragmentation increases routing complexity without improving any authority boundary.
- Creating a "verification-runner" role: No. Verification is a profile + human gate + (future) action adapter. Making it a role would let it claim authority it does not need.

---

## 5. Permission matrix

Columns = roles (minimal roster + the three being converted for completeness during transition). Rows = actions.

| Action | orchestrator | planner | coder | critic | architect (convert) | researcher (convert) | documentor (convert) | Notes (UE5) |
|--------|--------------|---------|-------|--------|---------------------|----------------------|----------------------|-------------|
| read files (repo, .uproject, Config, Source text, Content text inspection) | yes | yes | yes | yes | yes | yes | yes | All read-only roles need this. |
| edit text files | only agent_collab/ (via scripts) | no | only inside assigned file_ownership under coder_edit_roots (Source/, Config/) | no | no | no | only inside documentor_edit_roots (docs/) | Assert-EditScope.ps1 + scope_roots.json enforce. |
| produce generated binary outputs (.uasset, maps, PCG derived, Data/ catalogs) | no (delegates) | no | only inside assigned generated_output_ownership via approved UE automation on worktree | no | no | no | no | content_policy + handoff + Critic audit. Never direct. |
| run shell / pwsh commands | limited (orchestrator scripts + Assert-ActionPolicy) | no | limited (inside worktree; Assert-BashPolicy before every) | no | no | no | no | Blocked patterns include git push/rebase/reset, rm -rf, pipe-to-shell, UnrealEditor*/RunUAT/BuildCookRun unless explicitly permitted per handoff. |
| install dependencies / plugins / engine upgrades / asset acquisition / license acceptance | "ask" (autonomy_policy) + human gate | no | no | no | no | no | no | Always human. |
| create branches / worktrees | yes (via New-TaskWorktree.ps1 for coders; candidate branches) | no | no (uses pre-created worktree) | no | no | no | no | Orchestrator only. |
| commit changes | limited (local on task branches inside worktrees; candidate merges) | no | yes (on assigned task branch inside worktree only) | no | no | no | no | No direct main; always via candidate + Critic + promote. |
| push branches | never | never | never | never | never | never | never | Hard policy + blocked pattern. |
| open draft PRs | never | never | never | never | never | never | never | Explicit non-goal. |
| approve results (lifecycle decisions) | yes (only lease holder) | no | no | no (recommends via verdict) | no | no | no | Workers produce; Orchestrator decides. |
| request human approval / gate | yes (via autonomy_policy "ask", pending playtest, unavailable profiles) | no | no | no (surfaces in verdict) | no | no | no | Human approves authority, not "press build". |
| access secrets | no (none in repo) | no | no | no | no | no | no | Policy forbids invention of auth. |
| access network (from agent) | limited (researcher-style via tools/MCP only) | limited | no (inside worktree; policy blocks curl pipe etc.) | no | limited | yes (intended) | no | Researcher/playbook use is read-only external. |
| trigger CI | never (no CI integration yet) | no | no | no | no | no | no | Future: would be Orchestrator action adapter only. |
| mutate durable control-plane state (state/, handoffs/, outbox/, leases/, logs/decisions.md) | yes (only while holding lease) | no | no | no | no | no | no | Single-writer invariant. Inbox/raw is the only worker write channel. |
| deploy / publish / package for release | "ask" + human gate + explicit handoff | no | no | no | no | no | no | package-smoke profile + human. |

**Key UE5 observation**: The matrix is already quite restrictive on the right side. The main work is making the left side (orchestrator) correctly throttle itself via Assert-ActionPolicy + verification availability + explicit playtest gates rather than over-delegating.

---

## 6. Workflow activation matrix (UE5 task classes)

Small task → minimal roster. Large or high-uncertainty → Planner first. Anything touching generated content or maps → human verification heavy.

| Workflow Class | Required Roles (minimal) | Verification Profiles (typical) | Human Gates Typical | Notes |
|----------------|---------------------------|---------------------------------|---------------------|-------|
| small bug fix (single .h/.cpp or config tweak, no generated) | orchestrator + coder + critic (text-only) | text-only | low (if text-only available) | Can be near-autonomous if ownership clear and no docs_impact. |
| feature implementation (new system or slice) | orchestrator + planner + coder + critic | text-only + compile + map-load (if maps) + editor-generation (if generated) | high (most UE profiles unavailable) | Planner required for ownership in intertwined C++/PCG/Data work. |
| refactor (cross-file, ownership change risk) | orchestrator + planner + coder + critic | text-only + compile | medium-high | Planner's ownership prediction is the main control. |
| dependency change (plugin, engine, asset pack) | orchestrator (human first) | text-only (after human install) | very high (always "ask") | Asset acquisition, plugin install, EngineAssociation change = human gate per autonomy_policy and content_policy. |
| migration / schema change (data contracts, save format, ritual catalog schema) | orchestrator + planner + coder + critic | text-only + compile + map-load (if PCG affected) + human-playtest (often) | high | Data-driven nature of restoration/ritual/night systems makes this high-risk. Requires human review of contract implications. |
| test failure repair | orchestrator + coder + critic | relevant test profile + compile | medium (once automation exists) | Currently mostly human because no discovered automation tests. |
| documentation-only change | orchestrator + (documentor skill or direct after green) | text-only | low | After Critic on the code change that triggered docs_impact. Prefer checklist over role. |
| security-sensitive change | orchestrator + planner + coder + critic | all applicable + manual audit | high (always ask for auth-related) | Policy blocks most vectors already. |
| large multi-file feature (vertical slice wave) | orchestrator + planner (wave DAG) + multiple coders (parallel) + critic (candidate) + (playtest) | per-task + candidate-integration + human-playtest (if flagged) | high | The "full" path. Planner + candidate promotion + Critic APPROVED required before any work branch promotion. |
| release / PR preparation | orchestrator + critic + human | package-smoke + playtest + manual | very high | Never autonomous. Human owns the authority decision. |

**Rule**: A small task must not pull the full roster or async lease machinery. Planner is optional for trivial single-file owned changes where ownership is obvious from the request.

---

## 7. Handoff/lifecycle model

Current model is largely correct and should be preserved:

1. Request created (human directive or approved backlog item).
2. Orchestrator (lease holder) claims or receives lease, creates handoff (from template), assigns task_id, role, file_ownership, generated_output_ownership, required_capabilities, required_verification_profiles, acceptance_criteria, requires_human_playtest, docs_impact, etc.
3. (If async/out_of_band) Orchestrator creates lease, writes delivery artifact to inbox/<runtime>/..., updates task_state + scheduler.
4. Worker (in worktree for coder, or external) performs only allowed actions, produces artifact (worker_summary or critic_verdict), returns via direct structured output or inbox/raw drop.
5. Orchestrator (lease) normalizes (if needed), validates schema, reconciles into handoff (claimed → done/blocked), outbox/, task_state, scheduler_state. Never trusts worker on lifecycle.
6. Verification profiles are executed (human + evidence capture for most UE5 ones today) and results attached.
7. For integration work: candidate branch is built from approved task branches; full candidate-integration profile + integration Critic (trusted runtime) runs on the candidate.
8. Critic returns APPROVED / REJECTED / BLOCKED with evidence (including vendor audit, scope violations, missing/failed profiles, pending playtest).
9. Only Orchestrator may promote candidate to work branch (agent-collab/<slug>/work) — and only on APPROVED + all required verification green + (if flagged) recorded human playtest.
10. If docs_impact: documentation checklist / documentor-skill step runs post-promotion.
11. Dashboard / status projection (state/status.json + Get-CollabStatus) is updated as side effect of the above.
12. Lease released or renewed as appropriate.

**Invariants (non-negotiable)**:
- Workers never mark their own work complete or mutate durable lifecycle state.
- Only the current lease-holding Orchestrator invocation makes lifecycle decisions, and only through validated script + schema paths.
- Ground truth = git (branches, worktrees, actual generated files on disk) + handoffs/* + outbox/* (incl playtest/) + inbox/raw/ + leases + vendor immutability. State JSONs are caches that reconciliation can rebuild.

This model already prevents most "workers approving themselves" and "dashboard divergence" failures.

---

## 8. Tool/MCP/action-adapter risk model

**Runtime vs Action boundary (current state is good; document it explicitly)**:

- Runtimes (claude-code, grok-cursor, future) = reasoning, context inspection, drafting changes, proposing ownership/verification, emitting structured summaries or verdicts.
- Action adapters (the PowerShell scripts under agent_collab/scripts/, future UE commandlet wrappers) = all controlled side effects: git worktree ops, branch creation, file edits (gated), lock/lease, reconciliation, state writes, candidate promotion, inbox/raw collection, Assert-* policy checks.

Current controls:
- Assert-EditScope.ps1 (UE5-aware: coder_edit_roots, documentor_edit_roots, always blocks vendor + forbidden + readonly + traversal).
- Assert-BashPolicy.ps1 (blocks git push/rebase/reset, rm -rf, pipe-to-shell, sudo, UnrealEditor*/RunUAT/BuildCookRun, obvious vendor paths).
- Assert-ActionPolicy.ps1 (autonomy_policy "allow/ask/deny" per risk).
- content_policy.json + scope_roots.json (no direct binary, generated only via automation + explicit handoff ownership).
- command_policy.json classification (most UE generation/packaging is "requires human approval" until whitelisted).
- capability validation in registry + adapter_matrix + Select-Runtime before any delegation.
- Worktree isolation for all coder mutation.
- Single-writer: only lease holder writes durable protocol; workers drop raw only.

**UE5-specific risks and mitigations**:
- Risk: A runtime hallucinates a "safe" editor commandlet or Python script path that mutates Content/ outside ownership. Mitigation: Assert-EditScope (for text) + handoff generated_output_ownership + Critic binary audit + map-load verification on candidate. No command is "safe" without explicit per-handoff permission.
- Risk: LFS smudge/checkout or large binary churn from mistaken asset work. Mitigation: blocked in Assert-BashPolicy for workers; Orchestrator controls LFS ops.
- Risk: Future discovered automation (e.g. a project-owned commandlet under Source/ or a Blutility) gets over-permissioned. Mitigation: must be added to verification_profiles.json with explicit "available" + per-handoff whitelisting; Critic must see the profile result.
- Risk: Network exfil or secret access via MCP/tools from a coder session. Mitigation: capability matrix + researcher-style tools should be routed only to read-only roles/playbooks; coders inside worktrees should have network blocked at policy level where possible.

No runtime should ever be given raw shell or direct file write outside the gated adapters.

---

## 9. Failure modes and controls

| Failure Mode | Likelihood in Current System | Primary Control(s) | UE5 Amplification / Specific Recommendation |
|--------------|------------------------------|--------------------|---------------------------------------------|
| Agent sprawl (too many roles, every task pulls full roster) | Medium (already 7; usage shows planner/coder/critic most active) | Minimal roster (4); workflow activation matrix (section 6); Planner optional for small tasks | UE5 long loops make sprawl expensive. Enforce "small task = orchestrator + coder + text-only critic" in /gloam-start-task and Orchestrator prompts. |
| Unclear authority (who may decide what) | Low (lease + single-writer is clear) | Exclusive Orchestrator lease; "only lease holder writes state/handoffs/outbox" rule in agent_rules.md + every adapter | Add explicit "Authority Model" subsection to agent_rules.md and restart_instructions.md. |
| Circular handoffs / planner depends on architect who depends on planner | Medium | Convert architect/researcher to steps inside Planner or Orchestrator skills | Remove the roles; update planner prompt to include "perform or attach architecture analysis and research findings as first-class fields in planner_output". |
| Workers approving themselves | Low (Critic is separate; Orchestrator promotes) | Critic independence + required candidate integration Critic before any promote | Strengthen: final integration Critic must use a trusted runtime that can actually run the (human-captured) verification evidence. |
| Agents bypassing policy through tools/MCP | Medium | Assert-EditScope + Assert-BashPolicy on every relevant call; capability validation at routing time; no "general shell" tool for workers | UE5: any future editor automation tool must be wrapped as an action adapter that reads the current handoff's generated_output_ownership before executing. |
| Every agent getting every MCP/tool | Low (registry + capabilities + adapter_matrix + Select-Runtime exist) | Keep and enforce strictly. Add "safety_floor" checks in routing for edit vs read-only | Already good; document the matrix in a living registry note. |
| Excessive human approval prompts | High (because UE5 verification is mostly unavailable) | autonomy_policy levels + verification_profiles availability flag + "ask" classification | Do not paper over reality. The correct UX is "this task requires human compile + map load evidence; here is the exact command and evidence format to capture". |
| Stale leases / in-flight async work lost on restart | Medium (async not yet heavily exercised) | Reap-ExpiredLeases + Reconcile-AsyncResults on every async resume; human double-check on raw drops | When enabling async, first exercises must be trivial read-only (researcher-style) before any coder work. |
| Out-of-scope edits (especially Content/ or vendor) | Medium (LFS makes recovery painful) | Assert-EditScope (always blocks vendor/readonly/forbidden) + handoff file_ownership + Critic scope_audit + generated binary list audit | UE5-specific: add explicit "binary file list diff" step to Critic prompt for any wave that declares generated_output_ownership. |
| Hidden shell access or command invention | Low | Blocked patterns + "no invention of commands" rule in command_policy + Unreal* classification as human-gated | Add "UnrealEditor-Cmd with arbitrary -run=..." to blocked until per-handoff whitelist exists. |
| Unclear artifact ownership (who owns a generated Data Asset or PCG output) | Medium (PCG + Data/ is the growth area) | Planner must predict generated_output_ownership narrowly; handoff must assign it; Critic audits actual produced paths | Update planner_output.schema.json if needed to make generated_output_ownership_prediction a required field with stricter validation. |
| Dashboard state diverging from event truth | Low (reconciliation is explicit) | Every resume does full reconcile from git + handoffs + outbox + actual generated files + leases as ground truth | Add "vendor immutability check" and "pending human playtest count" to the mandatory status report in restart_instructions.md. |

---

## 10. Final implementation sequence

**Recommended minimal roster (living)**: orchestrator, planner, coder, critic.

**Roles to merge/convert** (do not delete prompts overnight; deprecate):
- architect → Architecture Analysis Rubric (new playbook under agent_collab/playbooks/ or docs/agents/) + optional architecture_notes field in planner_output.
- researcher → External Research Playbook (skill + example MCP usage patterns).
- documentor → Documentation Update Playbook + checklist (invoked by Orchestrator after promotion when docs_impact).

**Required dashboard controls** (add/enhance):
- /gloam-status and Get-CollabStatus.ps1 must surface: active leases with age, pending human verification profiles per wave, pending playtest requirements, vendor audit status (clean/dirty), last reconciliation timestamp, unavailable profiles that are blocking promotion.
- state/status.json projection should include a "human_gates" section.

**Required human approval gates** (codify):
- Create or expand `agent_collab/context/human_approval_gates.md` (or section in autonomy_policy).
- Anything requiring an unavailable verification profile is auto-"ask".
- Plugin/engine/asset/license changes remain "ask" + human.
- Human playtest performance is always human (Orchestrator only records structured result).
- Promotion of any wave that produced generated binary content or touched maps requires explicit human confirmation of evidence unless the specific automation has been whitelisted.

**Recommended implementation order** (small, safe, incremental; no new agent roles):

1. Persist this review as `docs/agents/UE5-Agent-Substrate-Review.md` (this file) and add a short reference in AGENTS.md and agent_collab/context/agent_rules.md.
2. Add `agent_collab/context/workflow_activation.json` (or .md) containing the matrix from section 6, plus a "default_activation" rule: "small tasks default to orchestrator+coder+text-only-critic; Planner only when task_count > 1 or ownership_prediction_risk = high or docs_impact or requires_human_playtest".
3. Create `agent_collab/playbooks/` (or skills/) with:
   - `architecture-analysis.md` (rubric + output shape)
   - `external-research.md` (playbook + allowed MCPs)
   - `documentation-update.md` (checklist + when to run)
4. Update registry/roles.json and registry/agents.json: mark architect/researcher/documentor as "deprecated" or "capability_only" (keep templates for transition if needed, but set enabled=false for new role instances). Bump schema_version and record in decisions.md.
5. Update the three role prompts (orchestrator, planner, coder, critic) to reference the new playbooks and the "use skill before spawning extra role" principle. Update planner prompt to absorb architecture + research output requirements.
6. Enhance restart_instructions.md and the grok-cursor/claude-code resume/status commands with the UE5 workflow activation rules and explicit human gate checklist.
7. Add a "UE5 Action Adapters" section to command_policy.json or a new lightweight `ue5_action_adapters.md` documenting the current state ("no safe commandlets discovered; all compile/map/package are human + evidence until further notice") and the process to whitelist one.
8. Strengthen Critic prompt (all adapters) to always emit generated_binary_files list and vendor_changes_detected for any handoff that declared generated_output_ownership.
9. (Optional, later) When real project-owned editor automation or commandlets exist, add a corresponding available verification profile + action adapter wrapper + test it on a trivial generated-content task before routing real work through it.
10. Re-run full self-test + one real small task using the reduced activation path after each change.

**Non-goals**:
- Do not add a "playtest agent".
- Do not create a "ue5-build" or "package" agent role.
- Do not relax the no-push/no-PR rule.
- Do not invent command-line build/cook wrappers that do not exist in the repo.

**Success criteria for this cleanup**:
- A small pure-text bug fix can be described, planned (optional), coded, verified (text-only), criticized, and promoted with at most 3 distinct role invocations (orchestrator + coder + critic) and no new handoff for architecture/research/docs.
- Any task requiring human verification or playtest surfaces the exact gate and evidence format immediately in status.
- Registry no longer lists architect/researcher/documentor as enabled first-class roles for new work.
- The review document and workflow matrix are referenced from the main agent rules and onboarding.

This substrate, once pruned to the four roles + explicit UE5 playbooks/policies/gates, will be a reliable, auditable, minimal operating model for autonomous coding agents on an Unreal Engine 5 project. The goal is not an impressive roster. The goal is a system that does not lie about its autonomy level while safely accelerating vertical slice production under real UE5 constraints.

---

## Implementation Notes (started 2026-06)

The following artifacts were created/updated as the first concrete steps of the "Final implementation sequence" in this document:

- `docs/agents/UE5-Agent-Substrate-Review.md` (this file)
- `agent_collab/context/workflow_activation.json` (section 6 matrix + default rules)
- `agent_collab/context/human_approval_gates.md` (section 10 + authority vs mechanics)
- `agent_collab/playbooks/{architecture-analysis.md, external-research.md, documentation-update.md}` (converted roles as skills/playbooks + checklists)
- `agent_collab/registry/roles.json` and `agents.json` (architect/researcher/documentor marked deprecated + enabled:false for new work; notes point to playbooks and review)
- `agent_collab/context/agent_rules.md` (minimal roster + playbook preference stated in Role Boundaries)
- `AGENTS.md` and `.grok/rules/gloam-collab.md` (pointers to the review and reduced roster)
- `agent_collab/adapters/grok-cursor/agents/grok-critic.md` (strengthened generated binary + vendor audit requirement for content waves)

No new agent roles were introduced. No prompts or scripts were deleted (transition period). Further steps (full prompt updates across all adapters, status dashboard enhancements, stricter Planner schema for ownership fields) remain for subsequent work.

*All changes obey the "smallest reliable roster" and "prefer skill/policy/adapter/checklist/gate over new agent role" mandates.*

---

**End of Review**

*Follow-up work should update the living artifacts (this doc, workflow_activation, playbooks, registry, prompts, and restart instructions) rather than proliferating new roles.*
