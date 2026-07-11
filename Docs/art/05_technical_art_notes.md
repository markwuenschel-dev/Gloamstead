# Technical Art Notes

_Implementation-facing notes for supporting the Withered Gothic Stylization art direction._

_Updated 2026-07-11 — art direction realigned from "Withered Gothic Realism" to "Withered Gothic Stylization" per docs/game/00_current_design_baseline.md._

## Core Technical Goal

Technical art should support clarity, restoration state, atmosphere, and emotional contrast. Expensive rendering features are only valuable if they reinforce interpretation and consequence.

## Shared Restoration Material System

Major restoration assets should use a shared Before / During / After state model.

Suggested parameters:

- `RestorationAmount` from 0.0 to 1.0
- `WarmthAmount`
- `CoherenceAmount`
- `CorruptionAmount`
- `GlowIntensity`
- `MoteSpawnRate`
- `EdgeClarity`
- `RitualLineActivation`
- `WetnessOrLifeReturn`

Suggested material transitions:

- dead roughness to warmer roughness variation
- cold albedo to warmer local accents
- inactive cracks to glowing seams
- dry moss to subtle living moss
- obscuring grime to clearer shape boundaries
- corrupted veins to cleansed root/light lines

## Atmosphere System

Fog should not be a single global mood layer. It should support gameplay state.

Suggested fog states:

- baseline decay fog
- sanctuary-safe fog thinning
- warning-reactive fog
- corruption-crawling fog
- mirror/reflection fog
- fracture-layer fog
- dawn-release fog

Fog should be allowed to reveal as often as it hides.

## Light System

Important light types:

### Veil Heart Light

- warm but wounded
- soft, living, slightly irregular
- strongest emotional anchor
- clarity and pulse stability improve over progression

### Lantern Light

- path-memory light
- creates readable local safety
- may leave faint after-glow on surfaces
- can attract things that follow light

### Corruption Light

- not always dark; sometimes false warmth
- should be distinguishable from true restoration warmth
- may use sickly gold, bruised violet, cold silver, or oil-green accents

### Dawn Light

- feedback light
- reveals what changed
- reduces visual uncertainty after successful interpretation

## Liminal Memory Effects

Use sparingly and systemically.

Possible effects:

- subtle normal perturbation on corrupted distant geometry
- low-amplitude edge shimmer in fracture states
- temporary afterimages from restored lanterns
- soft-focus distance treatment in unrestored zones
- increased edge clarity in restored zones
- reflection delay or distortion for mirror events

Avoid making the game visually blurry by default. Liminal effects should be readable and state-driven.

## Niagara / VFX Direction

Particle effects should feel material and atmospheric.

Good sources:

- ash
- dust
- embers
- pollen
- dew motes
- splinters
- fog wisps
- rot spores
- tiny glass-like Heart motes

Behavior should be tied to game state:

- motes gather near restorable seams
- ash pulls toward corrupted objects
- pollen returns around restored gardens
- fog avoids active lantern memory paths
- Heart motes become erratic when damaged

## Procedural Content Notes

Procedural tools can support the world, but symbolic objects should remain authored.

Good procedural uses:

- moss and ivy distribution
- stone wear variation
- fog pockets
- ruin scatter
- ground grime
- corruption creep masks
- small root networks

Authored or semi-authored uses:

- lantern paths
- shrine layouts
- mirror sites
- bell frames
- Heart chamber
- major sanctuary boundaries
- tutorial clue arrangements

## Performance Guardrails

- Do not spend the entire visual budget on dense surface detail.
- Preserve clear value separation around interactable structures.
- Use scalable VFX for motes, fog, and corruption tendrils.
- Avoid full-screen post-processing that hides gameplay information.
- Prioritize a stable 60 fps target on mid-high hardware where possible.
- Ensure heavy night atmospherics still support readability.

## Debug Views Worth Building

- restoration state overlay
- corruption spread overlay
- clue visibility overlay
- interactable silhouette readability pass
- fog density by gameplay state
- Heart influence radius
- sanctuary warmth/coherence map
- night consequence visual state overlay

## Technical Art Test

A technical effect belongs if it makes one of these clearer:

- what changed
- what is threatened
- what was restored
- what the Heart is warning about
- where the player should look
- what rule the night is testing
- why dawn feels like feedback

If it only makes the scene look more expensive, it is secondary.
