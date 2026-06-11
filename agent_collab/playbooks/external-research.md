# External Research Playbook (replaces former "researcher" role)

**Classification**: skill/playbook. Not a first-class agent role.

**When to use**:
- Need to investigate Unreal Engine 5 docs, PCG features, StateTree, Niagara, Modeling tools, Fab/Marketplace pack compatibility, third-party plugin behavior, or engine version differences.
- Clarify a convention or limitation before Planner finalizes ownership or verification profiles.

**Input**: Specific question(s) + allowed sources (Unreal docs, Fab, project Content/ for existing usage patterns, engine plugin .uplugin manifests if present).

**Execution**:
- Use only repository-read + external tools/MCP (web_search, open_page, etc.). Never mutate.
- Parallelize aggressively when multiple independent questions exist.
- Capture source URLs + dates + exact quotes or version notes.
- Explicitly note UE5 5.7 + Gloamstead constraints (Git LFS, no project Plugins/ dir, heavy PCG, mixed C++/BP).

**Output** (structured, for Planner or Orchestrator consumption):
- Question
- Findings (with citations)
- Implications for file_ownership / generated_output_ownership / verification profiles
- Risks (license, binary size, determinism, hot-reload)
- Recommendation (use / avoid / needs human license acceptance)

**Rules**:
- This is information only. Research never authorizes asset acquisition, plugin install, or license acceptance (those are always human gates per autonomy_policy.json and content_policy.json).
- Do not propose code or content changes.
- Prefer citing official Unreal docs or existing project usage over speculation.

**Invocation**: Planner should perform or attach required research as part of DAG construction. Orchestrator may run this playbook directly for ad-hoc questions. No dedicated handoff or lease for pure research.

Reference: docs/agents/UE5-Agent-Substrate-Review.md, agent_collab/context/content_policy.json (vendor content read-only), unreal_project.json (enabled PCG plugins).
