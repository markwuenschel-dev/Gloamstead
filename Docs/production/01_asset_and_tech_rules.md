# Asset and Tech Rules

_Practical production rules for UE5 and premade assets._

## UE5 Direction

Use UE5 for atmosphere, lighting, third-person control, environmental detail, VFX, and rapid iteration.

Recommended features:

- Lumen for dynamic lighting if performance allows
- Niagara for Heart particles, corruption, fog, cleansing, and night effects
- Nanite for ruins/rocks where appropriate
- Data Assets for restoration definitions and night rules
- Curves/Data Tables for tuning warnings, clarity, corruption, and consequence intensity

## Premade Asset Strategy

Premade assets are encouraged for:

- ruins
- environments
- materials
- VFX primitives
- third-person controller base
- simple enemy animations
- sound ambience

Premade systems should not dictate the game's identity.

Avoid importing large survival/crafting/base-defense frameworks unless used only as references or isolated parts.

## Art Asset Selection

Prefer assets that support:

- realistic dark fantasy
- ruined environments
- bleak natural spaces
- readable silhouettes
- subtle magical restoration
- cyan/teal/warm light contrast

Avoid assets that push the game toward:

- cartoon cozy village sim
- bright MMO fantasy
- gothic horror gore
- sci-fi
- tower defense toy style
- survival-crafting clutter

## Agent Implementation Rule

Before implementing any feature, identify which core doc justifies it. If no doc justifies it, do not add it.
