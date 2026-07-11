# Naming and Voice Decision

_Locked 2026-06-18. Canonical source for the central light's name, the Gloam's definition, the Heart's
nature, and the authoring voice for its fragments. Supersedes the "working term / unresolved" flags
previously carried in `world/`, `questions/`, and `ROADMAP.md`._

## Decisions locked

| Question | Decision |
|---|---|
| What is the Heart? | A **wounded memory-engine** (see *Nature* below) |
| What is the Gloam? | **The unremembering** (see below) |
| Is the Heart tied to the Gloam? | **Left deliberately ambiguous** — an old alchemical work *or* a gentled fragment of the Gloam — to preserve ending headroom |
| Player-facing name | Everyday: **"the Heart."** Proper / lore name (revealed gradually): **"the Gloamheart."** |
| Legacy / code term | **"Veil Heart" / `VeilHeart`** stays the internal + C++ identifier — no refactor |

## The naming model (three layers)

1. **Everyday — "the Heart."** Default term in HUD, prompts, most whispers, casual reference. Warm,
   plain, immediately legible. Never needs to change.
2. **Proper — "the Gloamheart."** The Heart's true name. Surfaced *gradually* through restoration:
   late-tier fragments, the journal as clarity rises, and the ending. It ties to the world
   (Gloamstead, the Gloam) and quietly seeds the central mystery — *is the Heart made from the dark it
   opposes?* — without answering it.
3. **Legacy / code — "Veil Heart" / `VeilHeart`.** Class names (`AVeilHeart`,
   `FVeilHeartWarningFragment`, `UVeilHeartWarningCatalog`), categories, and existing system docs keep
   this. It is a code identifier, not player-facing text; renaming carries cost and no benefit. Where a
   doc uses "Veil Heart" to mean the *system*, that is correct.

## Nature of the Heart (sets every fragment's voice)

The Heart is a **wounded memory-engine**: an old work — alchemical, or a captured and gentled fragment of
the Gloam (left ambiguous) — that holds the true *remembered* shape of this place against the
unremembering. It is not a god and not a machine. It is ancient memory trying to become young again,
speaking in broken fragments because its own recall is damaged.

It is **never false.** It remembers imperfectly. Apparent contradiction is damaged memory or missing
player context — never deception. This makes the already-locked *"cryptic but never wrong"* rule a
**consequence of the Heart's nature** rather than an external constraint.

As the player restores the fixed structures the Heart remembers, its recall sharpens: it speaks more
clearly, reveals more of the past, and its glow steadies (see `01_veil_heart_character.md`).

**Why memory-engine:** it is the reading the locked **"Withered Gothic Stylization"** art direction (its
liminal-memory layer) and the
**fixed-point PCG ritual model** already demand — restoration works because the Heart re-asserts the
remembered pattern of *specific places*. See candidate #5 in `00_world_baseline_and_lore_questions.md`.

## The Gloam — the unremembering

The Gloam is a **semi-intentional decay that erases meaning, warmth, and the identity of places.** Not
cartoon evil; a force that makes places forget what they are. It is *why* paths vanish, structures lose
their purpose, corruption spreads through the neglected, and the Heart's own warnings are hard to parse —
the Gloam is eating the memory the Heart is trying to hold.

**Heart (remembering) vs Gloam (unremembering)** is the spine of both the fiction and the mechanics.

## Voice guide (authoring rule for `FVeilHeartWarningFragment::Fragment`)

- **Form:** 4–12 words, one breath. Present-tense, sensory, declarative fragments.
- **Vocabulary:** remembering / forgetting, warmth / cold, light / shape, root / ash / water / door /
  threshold. Avoid modern abstractions and game-y nouns.
- **Never lies.** Cryptic, incomplete, emotionally distorted — never false.
- **Fair Crypticism:** every fragment must be inferable from **≥ 2 support channels** (environmental clue,
  prior pattern, restored-object reaction, audio cue, enemy behaviour, readable consequence, dawn
  feedback). This is the same rule the `FVeilHeartWarningFragment` support-channel fields + validator
  enforce (Phase 3 workstream J / ROADMAP Track D).

### Clarity tiers (`FVeilHeartWarningFragment::ClarityTier`)

| Tier | When | Style | Example |
|---|---|---|---|
| 0 | Early nights, low restoration | One pure image, no referent named | "Three lights make a door." |
| 1 | Mid, some restoration | Image + a hint of place or cost | "Cold comes where no flame remembers." |
| 2 | Late, high restoration | Names the structure or the stakes | "The bell you woke will call them. Ring it, or be found." |

The Heart climbs tiers as restoration raises its recall — the player **earns** clarity. Reserve the
proper name *"the Gloamheart"* for Tier-2 fragments, the journal at high clarity, and the ending.

## Impact

- Unblocks Phase 3 workstreams **G** (UI/UMG text) and **J** (warning / dawn / journal authoring) — see
  `../Phase3_SixHourExperience.md`.
- Resolves the ROADMAP "Lore that blocks content authoring" items: Naming, What is the Heart, What is the
  Gloam, Heart's voice guide.
- Endings stay open (`../systems/05_progression_and_endings.md`): the Heart's ambiguous tie to the Gloam
  feeds benevolent / possessive / transformed axes later without locking them now.
