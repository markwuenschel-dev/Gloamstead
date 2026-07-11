# Corrected Wave 5A — Human PIE Feel-Check Checklist

Wave 5A wired the first player-driven, legible playable cycle **in C++** and proved it headlessly
(`gate.ps1` green, including a live-world automation test). This checklist is the **human** feel-check that
Wave 5A intentionally did **not** claim — run it in the editor to confirm the loop *feels* right on real
hardware. No live-PIE evidence is claimed by the wave until a human completes this.

## Setup

- Map: `/Game/ThirdPerson/Lvl_ThirdPerson` (project default; the Gloamstead sanctuary — has the placed
  `VeilHeart`, `BP_FirstNightDirector`, PCG ritual-point volume, and `GloamsteadSanctuaryBootstrap`).
- Pawn: `BP_ThirdPersonCharacter` (has `RitualPlacement` + `Interaction` + the Heart-rest verb via Interact).
- Press **Play In Editor**.

## Controls

| Key | Verb | Effect |
|-----|------|--------|
| WASD / mouse | Move / Look | — |
| Space | Jump | — |
| **R** | Restore | 1st press arms placement (preview a nearby unrestored ritual point); 2nd press *while a valid target is previewed* confirms → restores the point |
| **E** | Interact | **While arming placement → cancels** it; otherwise acts on the focused interactable — at the **Veil Heart, rests** (advances the cycle) |
| **Q** | Examine | Examine the focused object — at the Heart, logs its memory of the last night |
| **T** | Debug advance | (dev only) forces a phase advance; remove before a player build |

## The cycle to feel

1. **Day.** You spawn in the sanctuary. The FirstNightDirector presents the intro (Heart warning / lantern
   target via its Blueprint hooks). *Note:* the on-screen cycle captions appear on phase **changes**, so the
   opening Day has no caption yet — that's expected.
2. **Restore the lantern.** Walk to a lantern ritual point, press **R** to arm, move within range until the
   preview reads valid, press **R** again to confirm. The point restores (light accrues; the director's dusk
   gate unlocks). Try pressing **E** mid-arm once to confirm **cancel** works.
   - ⚠️ **Known level caveat:** in the current stripped `Lvl_ThirdPerson`, the PCG lantern point can spawn
     out of range / without a visible marker, so **R** may no-op from the spawn spot. If so, walk toward the
     PCG volume, or (editor-gated, separate authorization) add a visible lantern marker. The **R** binding
     itself is correct — this is a level-content/positioning gap, not a code gap.
3. **First night (scripted):** restoring the lantern auto-advances **Day→Dusk** (the director's tutorial
   gate). You should see the caption **"Dusk gathers. Heed the Heart's warning."** and the Heart's warning
   Blueprint hook fire. Then the director's timers carry **Dusk→Night** ("The night stirs: <type>. Act
   before dawn.") and **Night→Dawn**, ending on the coloured **dawn outcome** caption
   (green Success / yellow Partial / red Failure).
4. **Rest at the Heart (the recurring driver):** at **Dawn**, walk to the Veil Heart and press **E** — the
   prompt reads *"Greet the dawn"* and you wake into a new **Day**. On subsequent days (director dormant),
   press **E** at the Heart (*"Rest at the Heart"*) to bring the next night yourself. This is the
   player-driven advance that carries the loop past night one.
5. **Objective feel:** during Night, act (restore/cleanse per the night type) to resolve the objective early
   — resolving it should cut the night short to Dawn (the `OnNightShouldEnd` early-dawn path), and the dawn
   caption should reflect Success. Ignoring it should read Partial/Failure.
6. **Continuity:** after a Dawn, a `GloamsteadSanctuary` save exists. Stop PIE, Play again — the sanctuary's
   restored/corruption state should reload (load-on-start via the Bootstrap actor).

## What to record (for a real playtest claim)

- Exact map, pawn, and the night type that ran.
- Which captions appeared (phase, night-start, dawn outcome) and whether the outcome colour matched.
- Whether **R** restore, **E** rest, **E** cancel-placement, and **Q** examine each did what's described.
- The resolved outcome (Success / Partial / Failure) and whether dawn *felt* like a payoff.
- Any legibility gaps (couldn't find the target, unclear what to do, no feedback), for the next wave.

## Notes / expected-open

- Captions are a **debug on-screen surface** (`UGloamsteadCycleFeedbackSubsystem`), not a final HUD; a real
  widget (e.g. binding `WBP_FirstNightCaption`) is deliberately deferred (editor/UMG-gated).
- `ANightPressureActor` is logical-only in C++ (fires `OnMenaceChanged`); any visible menace needs a
  Blueprint child — deferred.
- The debug **T** advance and `IA_DebugAdvance` are dev aids; remove before a player build.
