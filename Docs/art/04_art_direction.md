# Art Direction

_Production-facing visual direction for the current version of Gloamstead._

## Final Direction Name

> **Withered Gothic Realism**

Expanded description:

> A painterly, romantic dark-fantasy realism with liminal memory atmospherics and ritualized restoration.

This is the recommended synthesis of the strongest stylized-realism options. It should replace reference-collage phrasing such as “Skyrim meets Dark Souls meets Ghibli.”

## Core Intent

Gloamstead should feel grounded and tactile, but not purely photoreal. The world should appear believable enough to inhabit, yet stylized enough that symbols, warnings, restoration states, and supernatural consequences can be read clearly.

The art direction should communicate:

- a dying world that is cold, abandoned, and half-forgotten
- a sanctuary that becomes warm, coherent, and emotionally meaningful through restoration
- a Veil Heart that feels like a wounded miracle rather than a generic base core
- a night phase where the world responds through distinct supernatural consequence languages
- mystery that feels poetic but fair

## Art Direction Stack

### Base Mood: Withered Romantic Realism

This is the emotional foundation.

The world should feel mournful, sacred, weathered, and lonely. It should carry the emotional logic of romantic landscape painting: fragile light, sublime decay, spiritual atmosphere, and small warmth against vast coldness.

Use this for:

- overall mood
- terrain and ruin atmosphere
- melancholic compositions
- sanctuary contrast
- long-view environment shots

### Rendering Philosophy: Painterly Gothic Realism

This is the visual execution layer.

Use grounded PBR materials and modern lighting, but subordinate realism to art direction. Surfaces should be believable, yet composed with painterly restraint. Texture density, value contrast, and silhouette readability matter more than maximum photogrammetry detail.

Use this for:

- architecture
- ruins
- foliage
- terrain materials
- lighting composition
- fog breakup
- asset cohesion

### World Behavior: Liminal Memory Realism

This is what makes Gloamstead feel distinct.

Unrestored and corrupted spaces should feel partly remembered: soft at the edges, slightly uncertain, visually unstable, and less trustworthy. Restored spaces should regain perceptual coherence.

Use this for:

- fog behavior
- distant geometry softness
- corruption distortion
- afterimages
- mirror/fracture nights
- Veil Heart perception effects
- dawn feedback

Core rule:

> Restoration restores perceptual coherence.

### Object and Structure Language: Ritualistic Naturalism

Restored structures are not generic buildings. They are symbolic, place-specific instruments of sanctuary logic.

Use this for:

- lanterns
- gardens
- mirrors
- bells
- roots
- boundaries
- shrines
- ritual markings
- object placement

Objects should feel like they had a cultural, spiritual, or ecological purpose before the player arrived.

### Restoration Accent: Luminous Ruin Fantasy

Use luminous beauty sparingly.

The Veil Heart, restored structures, dawn recoveries, and rare sacred reveals can use warm amber, muted gold, faint teal, motes, soft bloom, and living glow. These accents should feel precious because they are rare.

Do not let the whole game become pretty magical fantasy.

### Constraint Layer: Ashen Mythic Discipline

The baseline world should stay cold, sparse, and reverent. Ashen grays, cool blues, deep umbers, wet stone, dead wood, and old iron should dominate unrecovered spaces.

Warmth must be earned through restoration.

### Corruption Accent: Bleak Mineral Elements

Bleak Mineral Realism should be used as an accent, not the full identity.

Use mineral, fossilized, crystalline, or faceted material language for:

- corrupted zones
- fracture events
- petrified roots
- late-stage night escalation
- places where life has been unnaturally arrested

Avoid making the whole world feel like cursed geology.

## Color Language

### Baseline World

- ashen gray
- cold blue
- blue-black
- deep umber
- dead moss green
- wet stone neutral
- rusted iron brown
- old bone beige

### Restoration

- warm amber
- muted gold
- candlelight orange
- faint bioluminescent teal
- soft moss green
- low dawn peach

### Corruption

- black-violet
- dead crimson
- bruised blue
- oil-slick green
- cold silver-white
- mineral obsidian

### Rule

Color should be functional. The player should gradually learn what colors imply about safety, corruption, memory, restoration, temptation, and fracture.

## Lighting Rules

- Use realistic lighting as a foundation, but do not let physical correctness override mood or readability.
- Restored light should caress surfaces rather than blast them.
- Lanterns should define safe or remembered space.
- The Veil Heart should be the warmest and most emotionally important light source in the sanctuary.
- Night should have distinct atmospheric identities depending on consequence type.
- Dawn should be visible feedback, not just a skybox change.

## Texture Rules

- Avoid high-frequency noise everywhere.
- Background decay should be quieter than meaningful structures.
- Restorable objects should have readable seams, ritual marks, cracks, roots, or material boundaries.
- Painterly breakup can soften realism without becoming cartoony.
- Materials should change state through warmth, clarity, moisture, saturation, edge definition, and local growth.

## Silhouette Rules

- Restorable structures must be identifiable from a distance.
- Threats need readable shapes during fog and low light.
- Corrupted versions of objects should preserve enough silhouette to be recognized.
- Ritual structures should have iconic outlines: lantern posts, bell frames, root gates, mirror arches, shrine stones, garden circles.

## Restoration-State Rules

### Before

The object or place is cold, dead, unclear, broken, or half-forgotten.

### During

The object begins to remember its purpose. Light appears in cracks, seams, roots, glass, carvings, or suspended motes.

### After

The object feels active, coherent, warm, and slightly vulnerable. The nearby world becomes more readable.

## VFX Rules

VFX should feel like real matter behaving unnaturally.

Good:

- embers
- motes
- dust
- mist
- pollen
- ash
- root-veins
- liquid reflection shifts
- shadow tendrils with physical drag

Avoid:

- generic spell circles
- noisy particle storms
- overpowered bloom
- excessive magical UI-like effects
- VFX that obscure clues

## Production Guardrails

- Prioritize readable silhouettes and value contrast over raw detail density.
- Keep shared material systems for Before / During / After restoration states.
- Make visual progression systemic where possible: warmth, coherence, fog behavior, edge clarity, and resonance can be parameterized.
- Use procedural tools for ruin variation, moss, ivy, fog pockets, and corruption spread, but keep symbolic structures authored.
- Optimize toward a small-to-medium scope; one memorable sanctuary is more important than broad open-world sprawl.

## Do / Don't

### Do

- make restored places feel sacred and fragile
- use painterly composition over noisy realism
- let fog and light teach the player
- make the Veil Heart visually alive
- make night consequences visually distinct
- make restoration improve clarity, not just beauty

### Don't

- default to gray ruins plus fog
- make every surface equally detailed
- overuse glow until it loses meaning
- make the world cozy by default
- hide clues behind darkness or post-processing
- turn symbolic structures into generic utility buildings
- chase pure photorealism at the expense of identity
