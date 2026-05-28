# Veil Heart System

_Central protected object, cryptic guide, progression anchor, and emotional core._

## Purpose

The Veil Heart is the working-name center of the game. It protects, guides, reacts, suffers, and grows. The final name is unresolved.

It should not be implemented as a generic base core with dialogue attached. Its mechanics and presentation should reinforce that it is alive or semi-alive.

## Core Responsibilities

- stores current restoration state
- emits cryptic warnings
- reacts to restored structures
- tracks sanctuary health/state
- provides dawn feedback
- changes visually over time
- anchors failure conditions if applicable
- supports progression by understanding, not just numeric level

## Warning System

The Heart gives warning fragments before or during key moments.

Each warning should connect to a night rule, environmental clue, or structure behavior.

Data model idea:

- Warning ID
- Text fragment
- Associated night consequence type
- Required/recommended clue objects
- Environmental reactions
- Dawn feedback line
- Unlock conditions
- Clarity tier

## Clarity Progression

Early warnings are obscure. Later warnings become clearer because:

- the Heart heals
- the player restores relevant structures
- the player has seen prior consequences
- certain interpretation structures are restored

## Failure Handling

Current simplest failure state: the Heart becomes corrupted or destroyed, the sanctuary light fails, and the player dies or is consumed.

If the Heart can be destroyed or silenced, failure should be dramatic and legible.

Possible fail states:

- Heart goes dark
- world returns to silence
- sanctuary structures collapse or extinguish
- player gets a tragic final scene

Do not finalize loss rules until prototype testing.


## Art-System Hooks

The Veil Heart should drive presentation state as well as game state.

Recommended tracked visual parameters:

- Heart clarity tier
- Heart damage state
- sanctuary warmth level
- sanctuary coherence level
- active warning color/accent
- restored structure resonance
- corruption proximity
- dawn feedback intensity

## Warning Presentation

Each warning should have at least one associated visual echo. Examples:

- lantern warnings cause dead lamp posts or fog paths to subtly react
- garden warnings cause soil, roots, pollen, or rot to shift before night
- mirror warnings cause reflections to lag, fracture, or show impossible angles
- bell warnings cause dust, hanging metal, or distant chimes to answer
- boundary warnings cause threshold markings, roots, or stones to tense

The Heart should never be a detached hint dispenser. Its fragments should ripple into the world through light, sound, fog, material response, or object behavior.

## Clarity Progression as Visual Progression

Clarity progression should be visible. As the Heart heals and the player learns, the world should communicate more cleanly:

- warnings receive stronger environmental echoes
- restored objects react more consistently
- dawn feedback becomes more legible
- sanctuary light becomes steadier
- corrupted effects become easier to distinguish from normal decay


## Reliability Constraint

For the current design, the Heart should be hard to understand rather than wrong.

System implication:

- warning fragments can be incomplete
- clarity tiers can improve wording and environmental echoes
- player history can affect what the Heart is able to express
- dawn feedback can clarify misunderstood warnings

Avoid building systems around the Heart deliberately lying or making factual mistakes until the core loop is proven.

## Growth Direction Placeholder

The Heart may eventually have growth direction states based on player decisions. Possible axes:

- benevolent / protective
- hungry / possessive
- lucid / confused
- bound to the Gloam / resisting the Gloam
- sacrificed / awakened / restrained / transformed

These are lore and ending seeds, not MVP requirements.
