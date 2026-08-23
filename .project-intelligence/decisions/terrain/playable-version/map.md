# Decision terrain — getting Gloamstead to a user-playable version

Started 2026-08-20. Chart, not a resume — no prior map existed for this destination.

## Destination

A private Windows alpha: a packaged standalone build (no installer, no store page) that
launches without Unreal installed, handed to one named external tester as a ZIP. Scope is the
first-lantern restoration slice only — explicitly not the six-hour experience
(`Docs/Phase3_SixHourExperience.md` remains planned, not built).

## Resolved decisions

| ID | Decision | Settled answer |
|----|----------|-----------------|
| Q1 | Playable-artifact definition | Standalone package, Dev build first (diagnostic) → Shipping RC (handoff). No installer/store this round. |
| Q2 | Slice scope | First-lantern restoration only, labeled explicitly as a short alpha. Needs an excluded-feature list so scope can't silently expand. |
| Q3 | Completion state | Single-shot "Restored." + Quit to desktop, gated on the authoritative restoration-success event, not UI/actor inspection. No Restart tied in. |
| Q4 | Front-door menu | None before the first packaged acceptance run. Boot straight to the level; Quit lives on the completion screen. |
| Q5 | In-game restart | Excluded from v1. The same-map-reload/Mass crash is unverified against current code either way — that cuts toward exclusion, not enablement, until repro'd. |
| Q6 | Asset provenance / licensing | Provenance manifest required before any handoff outside this machine. Verified 2026-08-20 (re-confirmed by independent measurement): `Content/EuropeanHornbeam` + `Content/BlackAlder` + `Content/CommonHazel` + `Content/MSPresets` = **24,652,360,448 bytes exactly (24.65 GB decimal / 22.96 GiB)**, matching `.gitignore:88`'s "~24 GB, re-importable from Fab" note. Real inventory, not doc-tone risk. Does **not** block the Q8 local Development cook — only blocks Q7's external ZIP handoff. |
| Q7 | Distribution ring | Private ZIP to one named tester. No itch.io/Steam this round. Release record: recipient, artifact hash, package version, provenance-manifest version. |
| Q8 | Packaging authorization | **Gated by repo policy, not discretionary.** `agent_collab/context/human_approval_gates.md`, "Always Human": *"Running cook, package, BuildCookRun, or release packaging."* Preflight record required first; first Development cook needs explicit human go-ahead per that gate every time. |
| Q9 | Controls bar | Fixed keyboard/mouse only, documented in the handoff README. No rebinding/gamepad work for a one-tester alpha. |
| Q10 | Acceptance definition | Clean machine/account with no Unreal installed, exact archived artifact (hash-verified), full loop to completion, clean exit, no crash/debug leakage, minimum repeat count logged. Passing this authorizes only the private test ring — not public release. |

## Corrections made while integrating (2026-08-20)

- **Q8**: my original chart framed the first packaging attempt as merely "the next action, not asked as
  a choice." That was incomplete — it's an explicit repo-policy human-approval gate
  (`human_approval_gates.md`), not just unbuilt infrastructure. No autonomous path exists past it.
- **Q6**: my original citations (`00_differentiation.md:108`, `01_asset_and_tech_rules.md:51`) were
  art-direction guidance against a "Megascans gray soup" look, not asset-inventory evidence — fair
  correction. The underlying provenance concern still holds; it now rests on a verified ~24GB
  inventory instead of doc tone.

## Open — two live gates, not one

Corrected 2026-08-20: Q8 is not the map's sole frontier. Q6 is a separate, independent release gate.
They block different things and neither substitutes for the other.

- **Q8 — packaging authority.** Blocks any cook execution, local or otherwise. Preflight record now
  exists: `preflight-dev-cook.md` (owner, commit `fa87a66`, target, output root, measured free disk,
  retention, acceptance evidence all named). The concrete authorization question is in that file,
  asked in chat this turn — still awaiting the human's explicit yes.
- **Q6 — external-distribution provenance.** Blocks Q7's ZIP handoff only, not the Q8 local
  Development cook. T4 (provenance manifest) runs as an independent parallel track, not a
  precondition for requesting Q8's authorization.

## Tickets (work, not further taste questions)

- [ ] T1 — Excluded-feature list for the v1 slice (Q2)
- [ ] T2 — Identify the actual authoritative restoration-success signal in code before any UMG
      completion-screen work starts (Q3)
- [ ] T3 — Same-map-reload/Mass crash: repro + lifecycle-owner design. Gates Restart indefinitely
      until closed (Q5)
- [ ] T4 — Full asset/plugin/font/sound provenance manifest, starting with the three foliage folders
      above (Q6) — independent parallel track, does not block Q8
- [x] T5 — Release-preflight record written: `preflight-dev-cook.md` (Q8). Authorization itself is
      still open — record existing is not the same as approval granted.
- [ ] T6 — Acceptance checklist artifact matching Q10's exact criteria

## Fog / assumed, not re-asked this round

- "Release owner" / "command owner" referenced throughout Q6–Q8's answers is assumed to be the human
  (project owner) as approver, with the acting agent as executor once authorized — per
  `human_approval_gates.md`'s "Orchestrator Duty," which states the human is not expected to run the
  build command themselves. Not re-confirmed as a separate question given this is a single-tester
  private alpha.
- Preflight's free-disk figure is a point-in-time reading (2026-08-20) and is explicitly marked in
  `preflight-dev-cook.md` as needing re-measurement immediately before the cook actually runs.
