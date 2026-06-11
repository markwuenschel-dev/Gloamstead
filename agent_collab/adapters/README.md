# Agent Colllab Adapters

This directory contains runtime-specific adapters and projections.

**Source of truth**: `agent_collab/registry/` (runtimes.json, roles.json, adapter_matrix.json, etc.).

Runtime-specific files here (and native projections like .claude/, .grok/) are **adapters and projections only**. They must be derivable from the registry and must never become the source of truth.

Each runtime subdirectory contains:
- runtime.json : declaration aligned with registry
- onboarding.md / launcher.md : how to start the runtime in the Orchestrator role for this project
- role_prompts/ : role-specific prompts (orchestrator.md etc.) for the runtime's native format
- result_parser.md : guidance on extracting structured output from the runtime

When adding a new runtime:
1. Add entry to registry/runtimes.json (enabled, capabilities, may_act_as_orchestrator, etc.)
2. Update registry/adapter_matrix.json
3. Create agent_collab/adapters/<new-runtime>/ with the above files
4. Create native projection (e.g. .newruntime/) only if the runtime uses one; keep it minimal and aligned
5. Update .gitignore for the runtime's worktree directory
6. Extend Invoke-AgentRuntime.ps1 and Test-AgentCollabScaffold.ps1 as needed

Never hard-bind a logical role to a single runtime.
