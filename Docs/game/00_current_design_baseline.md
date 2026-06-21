# Current Design Baseline

_The canonical current version of Gloamstead._

## Short Pitch

> Nurture the last warm light in a dying world. Listen to what it whispers - before the dark learns how to silence it forever.

## Game Identity

Gloamstead is a third-person dark fantasy sanctuary-restoration game. The player protects a mysterious growing source of light, currently called the Veil Heart, in a bleak and half-dead world. The game is about learning the Heart's strange language, restoring meaningful structures, and surviving nights that respond to what the player understood, neglected, disturbed, or misread.

## What The Game Is

- Third-person.
- Dark fantasy, bleak but not hopeless.
- Restoration-focused.
- Interpretation-driven.
- Simple-combat pressure.
- Small-to-medium scope, at least initially.
- Centered on one meaningful place rather than a giant survival sandbox.
- Built around a clear action -> preparation -> consequence -> dawn feedback loop.

## What The Game Is Not

- Not tower defense.
- Not horde survival.
- Not RTS-lite.
- Not colony simulation.
- Not village management.
- Not survival crafting.
- Not a resource grind.
- Not a hack-and-slash action game.
- Not generic base defense.

## Core Experience

The player explores a ruined place, receives cryptic warnings from the Heart (proper name the Gloamheart), restores or alters fixed ancient structures, prepares at dusk, and survives nights that test whether the player understood the warning and the world rule behind it.

The most satisfying outcome is not "I killed everything." It is:

> I understood the warning. I restored the right place. I built the right thing. I survived because I learned the world.

## Art Direction Baseline

**Updated 2026-06-21 — stylized pivot.** The production-facing art direction is now **stylized**, not realism (driver: cohesive, affordable assets for a small/solo team). The emotional *tone* is unchanged — bleak, melancholic, fragile hope; Pillars 1, 5, 7 stand. (The deeper art corpus — `docs/art/`, `docs/reference/` — still reads "Withered Gothic Realism" and needs a follow-up rewrite pass.)

> **Withered Gothic Stylization** — hand-painted / cartoon dark-fantasy with a bleak, melancholic tone, liminal-memory atmospherics, and ritualized restoration. Realism is subordinated to silhouette and readability. Reference register: *Hollow Knight*, *Darkest Dungeon*, *Don't Starve*, *Inscryption* — stylized **and** grim, never cozy or toy.

This direction keeps:

- **Withered Romantic mood** as the emotional foundation: mournful, sacred, weathered.
- **Painterly stylization** as the rendering philosophy: hand-crafted forms, strong value contrast, readable silhouettes; texture restraint over photogrammetry detail.
- **Liminal memory** as the world-behavior layer: corrupted spaces feel half-remembered, unstable, and soft at the edges; restored spaces regain clarity and perceptual coherence.
- **Ritualistic object language**: lanterns, gardens, mirrors, bells, roots, shrines, and boundaries should feel symbolic, ancient, and place-specific rather than like generic construction pieces.
- **Luminous restraint** as the accent: warmth, teal-gold glow, motes, and miraculous beauty appear mainly through the Veil Heart, restoration, dawn feedback, and rare sacred reveals.

The art direction should make restoration feel like reality becoming trustworthy again, not merely like objects becoming cleaner or brighter.

## Current Genre Label

The best current label is:

> Third-person dark fantasy sanctuary-restoration game.

Alternative supporting tags:

- atmospheric adventure
- restoration puzzle
- light action
- supernatural consequence survival
- dark fantasy mystery

Avoid leading with "base defense" or "survival builder," because those imply the wrong game.


## Current Scope Baseline

The first playable experience should target approximately **2-6 hours**.

The current MVP shape is:

- one island or contained place
- semi-open, but scaled down
- fixed restoration locations
- a small set of meaningful night consequences
- enough dawn feedback to prove the loop
- possible ending direction seeds, but not necessarily full multiple-ending implementation

## Naming (locked)

Naming is locked (2026-06-18, see [../world/02_naming_and_voice_decision.md](../world/02_naming_and_voice_decision.md)): everyday term **"the Heart"**, proper / lore name **"the Gloamheart"** (revealed gradually). "Veil Heart" / `VeilHeart` remains only as the code/system identifier.

## Implementation Snapshot (June 2026)

The **first playable loop** exists in C++ on `main`:

- Restore ritual points via `URitualPlacementComponent` (optional `URitualDefinition` data).
- Advance **`UGloamsteadDayNightSubsystem`** through Dusk / Night / Dawn.
- Dusk: night type selection + Veil Heart warning emit (`UVeilHeartWarningCatalog`).
- Night: corruption or tutorial spread / omen clue delegate (`UNightConsequenceRuntime`).
- Dawn: Veil Heart reflection and cleared warning tags.

Not yet in code: combat encounters, journal UI, save persistence, full art pass.

See [../Phase2_CoreLoop.md](../Phase2_CoreLoop.md) and [../systems/](../systems/) implementation sections.
