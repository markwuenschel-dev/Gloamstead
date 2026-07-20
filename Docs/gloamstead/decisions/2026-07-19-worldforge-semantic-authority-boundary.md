# Decision: WorldForge ⇄ Gloamstead Semantic-Authority Boundary (v2.6 acceptance)

**Date:** 2026-07-19
**Status:** Ratified (human-directed)
**Scope:** WorldForge plugin (`Plugins/WorldForge/`) SceneSurvey/capture stream, "v2.6".
**Not this stream:** GloamsteadForge evidence discipline (`docs/gloamsteadforge/`) is a *separate* stream and is explicitly out of scope here.

---

## The contract (non-overlapping ownership)

The boundary is **not** "either Gloamstead or WorldForge." It is a two-sided contract with
non-overlapping ownership:

- **Gloamstead owns semantic intent.** It determines the actual project context, level, named
  area, anchor, subject, objective, and requested evidence — the *what should be documented*.
- **WorldForge owns generic execution capability.** It receives those resolved inputs, performs
  measurement or capture, and returns structured evidence, provenance, and failure codes — the
  *how to measure/capture*.

```
        SEMANTIC AUTHORITY                    │        EXECUTION CAPABILITY
        (Gloamstead owns)                     │        (WorldForge owns)
 ─────────────────────────────────────────────┼──────────────────────────────────────
  • real project context                       │   • validate the explicit request
  • real map / level                    ── request ──▶   • boot / automate Unreal
  • named area + anchor                        │   • survey · sample · capture
  • subject + objective                        │   • verify RHI / SceneCapture output
  • requested views / evidence                 │   • return evidence + provenance
                                        ◀── evidence ──   • return failure codes
 ─────────────────────────────────────────────┴──────────────────────────────────────
   ▲ authority must NOT cross this line ▲
```

Production flow:

```
Gloamstead agent
  resolves the meaningful target
  chooses the real map, anchor, purpose, and views
  calls a WorldForge operation
        ↓
WorldForge
  validates the explicit request
  boots/automates Unreal as necessary
  surveys, samples, captures, and verifies
  returns evidence tied to the requested subject
```

## The violation: semantic authority inversion

The architectural fault was **semantic authority inversion** — WorldForge assuming responsibility
for deciding *what* should be documented rather than only *how*. Symptoms:

- selecting `/Game/ThirdPerson/Lvl_ThirdPerson`,
- choosing `PlayerStart` as the anchor,
- fabricating camera offsets.

These are authority crossing the line, not mere misconfiguration.

This matters because existing Gloamstead documentation still describes `Lvl_ThirdPerson` as the
stock / transitional map used for earlier editor wiring, and identifies the real Gloamstead world
as unfinished. That historical implementation state **must not** be promoted into a WorldForge
production default.

## Evidence classification (locked)

| Verdict | Item |
|---|---|
| ✅ capability proof | headless RHI capture works |
| ✅ capability proof | SceneCapture2D → PNG works through the tested UE 5.8 API path |
| ❌ not survey evidence | the ThirdPerson screenshots |
| ❌ not a current blocker | exposure |
| ⛔ invalid production behavior | fallback Gloamstead map names, inferred semantic anchors, or invented camera coordinates **inside WorldForge** |
| 🎯 required production behavior | a **Gloamstead-originated** invocation whose request and returned evidence identify the **same** meaningful subject |

## v2.6 acceptance boundary

v2.6 does **not** pass because WorldForge can produce a visible PNG. It passes **only** when:

> the Gloamstead agent resolves a real subject, invokes the generic WorldForge capability, and
> receives valid evidence proving that exact subject was surveyed.

The earlier command-line / CLI shape remains valuable for low-level testing of argument parsing,
capture execution, failure handling, and deterministic evidence output. It is **not** the
production control plane. **The Gloamstead agent is the canonical operator; WorldForge is the
callable engine beneath it.**

---

## Verified anchors (as of this record)

- `Plugins/WorldForge/Source/WorldForgeCore/Public/SceneSurvey.h:1-18` — the C++ survey half is
  already correctly scoped: read-only, game-agnostic, lives in `WorldForgeCore` (the only module
  that ships into an external target like Gloamstead), and explicitly states camera capture is
  **not** in this module (done far-side, needs an RHI). This side of the boundary is clean.
- No `.py` files are committed in this repo — the "far-side in-editor python script" referenced by
  `SceneSurvey.h` is external tooling, so the map/anchor/camera-selection logic is **not** in
  committed Python. Its true location is pending recon (see follow-up).

## Follow-up (recon, read-only, report before editing)

Locate where the inversion currently lives (the actual `Lvl_ThirdPerson` / `PlayerStart` / camera-
offset defaults and the map-open driver), versus where the Gloamstead-originated invocation should
be authored. Report `file:line` evidence before any change.
