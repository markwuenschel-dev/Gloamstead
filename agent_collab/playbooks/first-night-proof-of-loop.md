# First Night Proof-of-Loop Playbook

## Purpose

Build and verify the current Gloamstead vertical slice as a deterministic first-night proof-of-loop.

The slice must prove:

> The player understood the warning, restored the lantern, survived because the lantern held the path, and saw that dawn confirmed the lesson.

## Hard Scope

This is a first-night tutorial proof, not the 2-6 hour MVP.

Build only:

- deterministic `Tutorial` night
- Heart warning/caption for a lost path
- one readable lantern restoration target
- PCG-backed ritual/path points initialized in PIE
- player lantern restoration through `URitualPlacementComponent`
- thin first-night director
- dusk readability cue
- scripted path encroachment stopped/weakened by lantern influence
- dawn confirmation from the Heart
- minimal UI: whisper/caption, interaction prompt, dawn confirmation

Do not build:

- adaptive night behavior
- combat encounters
- enemies
- resources
- journal
- save/load wiring
- multiple rituals
- multi-night progression
- failure/death/retry flow
- broad architecture frameworks

## Architecture

Add a small C++ actor:

`AGloamsteadFirstNightDirector`

The director owns only first-night beat sequencing and presentation triggers.

It may:

- listen to `UGloamsteadPCGSubsystem::OnStructureRestored`
- listen to day/night phase changes
- listen to `UNightConsequenceRuntime::OnNightStarted`
- call `AdvanceToNextPhase`
- trigger Blueprint hooks for warning, target cue, dusk cue, encroachment, and dawn payoff

It must not:

- replace `UGloamsteadDayNightSubsystem`
- replace `UNightConsequenceManager`
- mutate PCG point state directly
- own generic night selection
- implement combat, resources, save/load, or long-term progression

Core truth stays in C++. Presentation stays in Blueprint/UMG/VFX.

## Required Content/Wiring

- Add or verify a `Tutorial` warning row in `DA_VeilHeartWarningCatalog`.
- The warning must use `AssociatedNightType = Tutorial`.
- Satisfiable tags must include `LightPath` and/or `LanternPost`.
- Ensure `DA_Ritual_LanternPost` satisfies `LightPath`.
- Ensure PCG points initialize through `AGloamsteadSanctuaryBootstrap`.
- Ensure the level has a real lantern/path setup the player can read in PIE.

## First Night Sequence

1. Day begins.
2. Heart emits or presents the warning.
3. Lantern/path target becomes readable.
4. Player restores the lantern.
5. Director advances to Dusk.
6. Dusk shows a short preparation/readability cue.
7. Director advances to Night.
8. Scripted encroachment moves along the path.
9. Encroachment weakens/stops at lantern influence.
10. Director advances to Dawn.
11. Dawn shows world payoff and Heart confirmation.

No failure branch is required yet. If the lantern is not restored, night simply does not begin.

## Verification

Use two proof layers.

### Automated wiring test

Verify:

- director binds to required systems
- lantern restoration is detected
- dusk is locked until lantern restoration
- first night remains `Tutorial`
- night completion advances to Dawn
- director Blueprint events fire or test counters increment

### PIE playtest checklist

A human/player must be able to confirm without reading logs:

- warning is visible or captioned
- lantern target is readable
- restoration works
- dusk cue is understandable
- night encroachment visibly tests the path
- lantern visibly resists the encroachment
- dawn confirms the warning/restoration relationship

## Definition of Done

The slice is done only when PIE proves the player-facing loop, not merely when logs or unit tests pass.