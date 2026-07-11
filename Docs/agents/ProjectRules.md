# Gloamstead Agent Project Rules

_Always-on constraints for Cursor/LLM agents implementing the current version._

**Collaboration context**: This file covers game/UE5 architecture rules. For the current agent operating model (minimal roster, playbooks, workflow activation, human gates), see the sibling `UE5-Agent-Substrate-Review.md` and `../agent_collab/context/agent_rules.md`.

## Current Game Identity

Gloamstead is a third-person dark fantasy sanctuary-restoration game.

The player protects and interprets the Heart, a mysterious growing source of light in a bleak, half-dead world. The main gameplay is not tower defense, horde survival, village management, colony simulation, RTS command, or survival crafting.

## Design North Star

> I understood the warning. I restored the right place. I built or altered the right thing. I survived because I learned the world.

Agents should preserve this fantasy in every system decision.

## Core Rules

- Third-person perspective is core.
- Combat is simple and secondary.
- Restoration and interpretation are primary.
- Nights are consequences, not standardized horde waves.
- Building is ritualistic restoration, not Lego-like base construction.
- No tower-defense lanes as the main loop.
- No village management or colony simulation in the core scope.
- No survival-crafting resource grind in the core scope.
- No complex hack-and-slash combat.
- No RTS-lite command layer.
- No multiplayer/co-op for the current baseline.

## Implementation Bias

- Use C++ for core systems, data models, save logic, and interactions.
- Use Blueprints for designer-facing iteration, effects, tuning, UI hookup, and prototypes.
- Keep mechanics data-driven through Data Assets, Curves, and Data Tables where appropriate.
- Prefer small vertical slices that test the restoration-warning-night consequence loop.
- Avoid building broad frameworks before the core loop proves fun.

## Naming Rules

- Do not invent canonical names for mechanics, enemies, factions, places, or resources unless explicitly asked.
- Use generic names in code until terms are approved.
- Allowed current terms: `Gloamstead`; **"the Heart"** (everyday player-facing name), **"the Gloamheart"** (proper/lore name, revealed gradually). `Veil Heart` / `VeilHeart` is the **code/system identifier only** — never player-facing prose. See `../world/02_naming_and_voice_decision.md` (locked 2026-06-18).
- Generic safe terms: `LightSource`, `ProtectedObject`, `RestorationSite`, `NightConsequence`, `Corruption`, `WarningFragment`, `RestorationPiece`, `Darkness`.

## First Prototype Target

The first prototype should include:

1. Third-person controller.
2. One small ruined area.
3. One protected Heart object (`VeilHeart` in code).
4. One cryptic warning.
5. One restoration interaction.
6. One night consequence.
7. One simple combat/cleansing interaction.
8. One dawn payoff showing that the warning had meaning.

Do not build inventory, villagers, economy, tower defense, horde waves, open-world scope, or deep combat before this loop works.
