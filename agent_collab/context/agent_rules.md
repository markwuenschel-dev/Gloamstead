# Agent Collaboration Rules for Gloamstead

These rules apply to all participants in the agent_collab system (Orchestrator, Planner, Coder, Critic, etc.). They are in addition to the per-runtime agent definitions.

## Core Identity (always preserved)
Gloamstead is a third-person dark fantasy sanctuary-restoration game. The player interprets cryptic warnings, performs ritualistic restorations of meaningful structures, and faces narrative/mechanical consequences at night. "I understood the warning. I restored the right place."

## Implementation Bias
- Use C++ for core systems, components, interfaces, data structs, subsystems, save logic.
- Use Blueprints for designer-facing iteration, effects, tuning, UI hookup, prototypes.
- Keep mechanics data-driven (Data Assets, Data Tables, Curves).
- Prefer small vertical slices validating the restoration-warning-consequence loop.
- Avoid broad frameworks, backward-compat shims, or unrelated systems before the core loop proves fun.
- When modifying Unreal C++: update .h and .cpp together; follow UE naming; practical UPROPERTY metadata; Blueprint-readable where useful.

## Naming & Scope Discipline
- Do not invent canonical names for mechanics, enemies, factions, places, or resources unless explicitly approved in docs/.
- Use generic safe terms until terms are locked: LightSource, ProtectedObject, RestorationSite, NightConsequence, Corruption, WarningFragment, RestorationPiece, Darkness.
- Allowed current terms: Gloamstead, Veil Heart.
- Before coding, identify the specific system doc being implemented (e.g. Phase2 docs or ADR).
- After coding, summarize changed files + any editor setup steps required.

## Collaboration Hard Rules (this system)
- Only the Orchestrator writes durable state (agent_collab/state/**, handoffs/**, outbox/**, logs/**, decisions.md).
- Workers never spawn workers. Return BLOCKED + needs/request if help required.
- Coder edits ONLY within its file_ownership inside coder_edit_roots; never touches docs/.
- Documentor edits ONLY documentor_edit_roots after integration verification; never touches source/tests.
- No push, PR, rebase, amend, hard reset, filter-branch, or history rewrite on any branch.
- Work branch (agent-collab/gloam/work) must always be green after promotion.
- Every candidate wave is verified by integration Critic on a test-capable trusted runtime before any promotion.
- Scope guards (Assert-EditScope) and command policy (Assert-BashPolicy) are enforced; violations are BLOCKED.
- All routing decisions and state transitions are logged with reason in orchestrator.log and decisions.md.

## Existing Agent Guidance
See docs/agents/ProjectRules.md and .cursor/rules/gloamstead.mdc for project-specific constraints. These take precedence for game content decisions.

## Runtimes (Grok + Claude)
- **grok-cursor**: Grok in Cursor — `/gloam-resume`, Task subagents, `.grok/worktrees/`, projection via `Project-GrokAdapter.ps1`.
- **claude-code**: Claude Code — `claude --agent gloam-orchestrator`, native worktrees, projection via `Project-ClaudeAdapter.ps1`.
- **local-script**: bounded runner only; raw output to `inbox/local-script/raw/`.

Only one Orchestrator session should hold `orchestrator.lock` at a time across IDEs.

## Restart & Audit
On every Orchestrator start: acquire lock, reconcile ground truth (git + handoffs + inbox/raw + leases), rebuild state caches if they diverge, log to decisions.md, report status, wait for human instruction.
