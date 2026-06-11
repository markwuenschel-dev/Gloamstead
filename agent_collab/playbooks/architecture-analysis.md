# Architecture Analysis Playbook (replaces former "architect" role)

**Classification**: skill/playbook + rubric. Not a first-class agent role.

**When to use**:
- First tasks of a wave that introduce new data contracts, event boundaries, subsystem interfaces, save schema, or cross-cutting PCG integration.
- Any task the Planner or Orchestrator marks as "architecture_notes_required".

**Input**: Current handoff goal + relevant design docs (systems/, game/, production/) + existing source headers for the affected modules + project_goal.md + agent_rules.md.

**Output shape** (attach to planner_output or as separate ADR-style note in handoff context):
- Problem / forces (UE5 + Gloamstead specific: C++/BP split, PCG determinism, LFS cost of mistakes, ritual restoration north star).
- Proposed boundaries (files/modules that will own the contract).
- Data model sketch (USTRUCTs, Data Assets, enums, tables) with ownership.
- Risks and verification implications (which verification profiles this will require).
- Integration points with existing systems (NightConsequence, Restoration, PCGSubsystem, etc.).
- Open questions for human.

**Rules**:
- This is read-only analysis. Do not propose code edits.
- Prefer narrow ownership. In a mixed C++/BP project, core logic and persistent data should bias toward C++ (per ProjectRules).
- Output must be usable by Planner to produce accurate file_ownership and generated_output_ownership.
- Do not invent canonical names unless already approved in docs/.

**Do not spawn a separate agent for this**. Invoke as a step inside Planner (preferred) or as an Orchestrator skill before delegating coders on complex waves.

Reference: docs/agents/UE5-Agent-Substrate-Review.md (sections 2-4), docs/agents/ProjectRules.md.
