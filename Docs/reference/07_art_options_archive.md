# Stylized Realism Options Archive

_Original option set and final usage decision._

_Historical archive — superseded by the 2026-06-21 stylization pivot. Retained for record; current art direction lives in docs/game/00_current_design_baseline.md and reference/05._

_Updated 2026-07-11 — realigned from "Withered Gothic Realism" to "Withered Gothic Stylization" per docs/game/00_current_design_baseline.md._

**Stylized Realism Art Directions for Gloamstead**

Here are four refined, self-contained art directions pulled firmly from the stylized-realism bucket. Each is built to feel grounded and tactile (realistic scale, believable physics, material response) while using deliberate artistic choices to heighten atmosphere, readability, and emotional contrast between the dying world and the fragile warmth of restoration.

### 1. **Withered Romantic Stylization** (Recommended Primary Direction)
- **Core Feeling**: Late-19th-century romantic landscape painting (think Caspar David Friedrich or John Atkinson Grimshaw) meets modern high-fidelity fantasy.
- **Key Traits**:
  - Realistic human/anatomy scale and weight, but forms are slightly elongated and weathered for melancholy elegance.
  - Rich, painterly textures: cracked stone with moss glowing softly like velvet, rusted iron with pearlescent patina, wood grain that feels carved by centuries of wind.
  - Color language: Desaturated cool blues, ashen greys, and deep umbers for the world. Restoration introduces warm amber, muted gold, and faint bioluminescent teal accents that feel like they’re literally bleeding life back into the scene.
  - Lighting: Lumen + volumetric fog with strong rim lighting on restored objects. Lanterns and the Heart cast soft, almost oil-paint glows that caress surfaces rather than harsh PBR specular.
- **UE5.8 Technical Backbone**: Heavy use of Parallax Occlusion Mapping + Virtual Textures on ruins, custom Distance Field materials for glowing restoration edges, and Niagara GPU sprites for floating motes that react to player proximity.

### 2. **Bleak Mineral Stylization**
- **Core Feeling**: The world is made of stone, crystal, and petrified matter that is slowly remembering it was once alive.
- **Key Traits**:
  - Everything has a mineral quality — even organic matter looks half-fossilized. Trees are gnarled obsidian-veined trunks; grass is brittle quartz blades.
  - Restoration reverses this: stone softens into living marble, roots pulse with inner sap-light, mirrors have liquid mercury surfaces that ripple.
  - Tight value range in the world (mostly mid-to-dark tones) so any warm light from the Heart or lanterns reads as luminous and precious.
  - Subtle stylization: slight geometric faceting on corrupted surfaces (like low-poly facets under high-detail normal maps) to suggest the world is fracturing along unnatural rules.
- **UE5.8 Wins**: Nanite + Chaos for destructible/cracking geometry during night events, Substrate materials for complex layered stone/organic blending, and Lumen’s Global Illumination to make restored warm zones feel like they’re pushing back real darkness.

### 3. **Faded Tapestry Stylization**
- **Core Feeling**: The world looks like an ancient, threadbare tapestry that the player is slowly re-weaving with light and life.
- **Key Traits**:
  - Textures have a woven/micro-pattern quality even on stone and metal (tiny embroidered-like details visible up close, softening at distance).
  - Color is desaturated but with intentional “dye” accents — faded crimson on old banners, tarnished gold thread on shrines, ink-blue shadows.
  - Restoration literally re-dyes the world: new growth has richer saturation and subtle embroidered edge highlights.
  - Atmospheric perspective pushed artistically — distant ruins look like they’re dissolving into stitched mist.
- **UE5.8 Technical Backbone**: Custom Material Layers with world-position UV panning for “re-weaving” effects on restored objects, high-quality Virtual Shadow Maps for crisp lantern light shafts cutting through volumetric “thread” fog, and PCG for procedural tapestry-like moss/ivy patterns.

### 4. **Liminal Memory Stylization**
- **Core Feeling**: The environment is half-remembered — photorealistic in moments of focus, but softly stylized and dream-like at the edges.
- **Key Traits**:
  - Sharp detail on interactable/restorable objects, softer and more impressionistic on distant or corrupted geometry (custom depth-of-field + stylized normal perturbation).
  - Light behaves with memory: restored lanterns leave faint after-glow trails on surfaces for several in-game minutes.
  - The Heart has subtle refractive caustics and inner “veins” that shift like living stained glass.
  - Silhouettes are slightly exaggerated on threats and ruins to enhance readability during night phases without breaking the stylized register.
- **UE5.8 Wins**: Temporal Super Resolution + custom Post Process Materials for the “memory” softening effect, Niagara + Chaos Cloth for flowing tattered banners/roots that react to night winds, and strong use of Runtime Virtual Textures for dynamic corruption/reclamation painting across the sanctuary ground.

**Common Technical Guardrails Across All Directions** (important for scope):
- Target 60 fps on mid-high hardware with Nanite + Lumen + Virtual Shadow Maps.
- All restoration assets share a unified “Before / During / After” material state system (scalar parameter + texture blend driven by gameplay).
- Heavy emphasis on readable silhouettes and strong value contrast so players can read threats and opportunities even in heavy fog.
- Particle and VFX language stays grounded — embers, motes, and corruption tendrils should feel like real matter behaving unnaturally rather than pure magic effects.

Any of these directions will give us that beautiful tension between harsh, believable decay and the precious, almost sacred quality of restoration without sliding into full photorealism (too expensive/sterile) or heavy stylization (too cartoony for the tone).

Which of these resonates most with the current vision, or would you like me to expand one into a full moodboard-style reference list + material breakdown?

## Final Usage Decision

- **Withered Romantic Stylization:** primary emotional foundation.
- **Painterly Gothic Stylization:** primary rendering philosophy.
- **Liminal Memory Stylization:** world-behavior and atmosphere layer.
- **Ritualistic Naturalism:** restorable object and sanctuary structure language.
- **Luminous Ruin Fantasy:** restrained restoration and the Heart accent.
- **Ashen Mythic Stylization:** palette/tone discipline, not the main name.
- **Bleak Mineral Stylization:** corruption, fracture, and petrification accent only.
- **Faded Tapestry Stylization:** avoid as primary; borrow only subtle re-weaving transition ideas if useful.
