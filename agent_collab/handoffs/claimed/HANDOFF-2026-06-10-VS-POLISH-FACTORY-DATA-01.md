# Handoff

**handoff_id**: HANDOFF-2026-06-10-VS-POLISH-FACTORY-DATA-01
**task_id**: VS-POLISH-FACTORY-DATA-01
**task_type**: editor-automation
**slice_id**: vertical-slice
**role**: coder
**from**: orchestrator
**to**: gloam-coder
**selected_runtime**: grok-cursor
**allowed_runtimes**: ["grok-cursor", "claude-code"]
**preferred_runtime**: grok-cursor
**created**: 2026-06-10T12:00:00Z
**status**: done

## Goal
Build JSON-manifest Data Asset import factory: GloamsteadEditor module + GloamsteadImportDataAssets commandlet, starter manifest matching guide examples, whitelisted import script, protocol updates.

## Context
Implements Editor Asset Automation plan. Unblocks VS-POLISH-DATA-FU-03 / DATA-02. Human runs compile + import per specs/data/HUMAN_RUN_IMPORT.md.

## Assigned Role
coder

## Allowed Runtimes / Preferred
grok-cursor / grok-cursor

## Asset Packs
None

## File Ownership
["Source/GloamsteadEditor/", "specs/data/", "Gloamstead.uproject", "Source/GloamsteadEditor.Target.cs", "Source/Gloamstead/Data/RitualDefinition.h", "agent_collab/scripts/Invoke-GloamsteadDataAssetImport.ps1", "agent_collab/context/scope_roots.json", "agent_collab/context/verification_profiles.json", "agent_collab/context/command_policy.json", "agent_collab/scripts/Assert-BashPolicy.ps1", "docs/systems/03_night_consequence_system.md", "agent_collab/context/environment.md"]

## Generated Output Ownership
["Content/Data/"]

## Content Policy
Generated binary outputs ONLY via Invoke-GloamsteadDataAssetImport.ps1 + GloamsteadImportDataAssets commandlet on assigned paths. No direct .uasset editing.

## Required Capabilities
["ue5-editor-automation", "generated-content-production"]

## Required Verification Profiles
["text-only", "compile", "editor-generation", "map-load"]

## Acceptance Criteria
- GloamsteadEditor module compiles; commandlet reads specs/data/vs-polish-starter.json
- Import script whitelisted in command_policy + Assert-BashPolicy
- editor-generation profile available in verification_profiles
- Manifest values match guide Example Assets sections
- WIRING.md + HUMAN_RUN_IMPORT.md document human steps
- C++ fallbacks unchanged when no asset assigned

## Docs Impact
true

## Human Playtest Requirement
false

## Attachments
- specs/data/vs-polish-starter.json
- specs/data/WIRING.md
- specs/data/HUMAN_RUN_IMPORT.md

## Dependencies
["VS-POLISH-DATA-01"]

## History
- 2026-06-10: Implemented factory (commandlet, manifest, wrapper, protocol). Human compile+import pending.
- 2026-06-11: Human verification complete — import exit 0, six `Content/Data/DA_*`, `Lvl_ThirdPerson` wiring + PIE day/night smoke. Evidence: `specs/data/VERIFICATION-2026-06-11.md`.
