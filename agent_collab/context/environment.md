# Environment and Tooling

## Runtime Environment
- OS: Windows (primary dev) / WSL2 Ubuntu (agent execution possible)
- Shell: PowerShell 7 (pwsh) for orchestration scripts; bash available in WSL
- Git: required, used for branch/worktree management and toplevel resolution in scope guards

## Primary Engine
- Unreal Engine 5.7+
- Project: Gloamstead.uproject
- Language mix: C++ (core systems, PCG, components, save, subsystems) + Blueprints (designer iteration, UI, effects, placement previews)
- Build: Visual Studio 2022 (Windows) or appropriate toolchain

## Key Directories (repo-relative)
- Source/ — C++ module source (Gloamstead module)
- Content/ — Unreal assets (Blueprints, maps, data, materials, etc.)
- docs/ — All human-readable design, architecture, phase, and production documentation (markdown + docx)
- Config/ — Engine/project INI settings
- (no top-level Tests/ directory discovered; automation tests would live under Source/ or a dedicated test module if added)

## Collaboration Tooling
- PowerShell 7.4+ for guard scripts, schema validation, lock management, wave planning, normalization
- Git worktrees for isolated Coder execution (baseRef: "head" to carry unpushed work-branch commits)
- Claude Code (claude-code runtime) for the primary Orchestrator and subagent workers
- local-script runtime for executing repository scripts/commands as bounded workers (e.g. build, test invocation, simple transforms)

## Branching (managed by Orchestrator only)
- work: agent-collab/gloam/work (always green after verified promotion)
- task: agent-collab/gloam/task/<task_id>
- candidate: agent-collab/gloam/candidate/<wave_id>

## Safety & Audit
All edits by non-Orchestrator roles are guarded by scope + command policy scripts. Integration verification is mandatory before any work-branch promotion. Full audit trail in agent_collab/logs/ and git history on task/candidate branches.
