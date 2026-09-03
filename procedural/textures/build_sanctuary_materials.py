"""
Build the sanctuary's master material and put the seven MI_Sanctuary_* instances onto it, headless.

Run with:
    UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript
        -script="procedural/textures/build_sanctuary_materials.py" -unattended -nullrhi -nosplash -nopause

The third and last stage of the chain: forge_sanctuary_textures.py authors the PNGs,
import_sanctuary_textures.py turns them into .uassets, and this script builds the shader that reads
them and repoints the existing instances at it.

WHY A NEW MASTER RATHER THAN AN EDIT TO THE OLD ONE
    M_Gloamstead_WorldForge_Tiled is a procedural terrain shader whose scalar parameters
    (base_hue / saturation / value / strata_angle / sand_overlay) describe a desert rock surface
    generated in the shader. Nothing in it belongs to a sanctuary. Rewriting it in place would
    silently change every other asset that happens to reference it; a new master leaves the old one
    exactly where it is and moves only the seven instances that were wrongly on it.

WHY THE PARAMETER NAMES ARE PRESERVED
    All five texture parameter names (BaseColorTexture / NormalTexture / RoughnessTexture /
    AOTexture / HeightTexture) and all eight legacy scalar names are declared on the new master with
    the spellings the old one used. A material instance stores its overrides by NAME: reparent it to
    a master that spells a parameter differently and the override is not remapped, it is dropped.
    The seven instances carry hand-authored scalar values, so every one of those names is kept alive
    even where the new shader has no use for it yet -- see LEGACY_SCALARS below.

WHAT ACTUALLY DRIVES THE LOOK NOW
    The textures. BaseColor / Normal / Roughness / AO are texture parameters, so each instance
    selects its own material set, and TintColor and TileScale are the two knobs on top.

NOTHING HERE RENAMES, MOVES OR DELETES AN EXISTING MI_Sanctuary_* ASSET. C++ loads them by path
(Source/Gloamstead/.../GloamsteadRestoredStructure.cpp and its siblings), so their paths are a
contract, not an implementation detail.
"""
import unreal

OLD_MASTER = "/Game/Gloamstead/Materials/M_Gloamstead_WorldForge_Tiled"
NEW_MASTER_PATH = "/Game/Gloamstead/Materials"
NEW_MASTER_NAME = "M_Gloam_Sanctuary"
NEW_MASTER = "%s/%s" % (NEW_MASTER_PATH, NEW_MASTER_NAME)

TEXTURE_ROOT = "/Game/Gloamstead/Kit/Textures"
INSTANCE_ROOT = "/Game/Gloamstead/Kit/Materials"

# instance suffix -> generated material set
BINDINGS = [
    ("Stone", "Stone"),
    ("Stone_Dark", "Stone_Dark"),
    ("Paving", "Paving"),
    ("Soil", "Soil"),
    ("Iron", "Iron"),
    ("Lichen", "Lichen"),
    ("Weathered", "Weathered"),
]

# texture parameter name -> generated channel suffix
TEXTURE_PARAMS = [
    ("BaseColorTexture", "BC"),
    ("NormalTexture", "N"),
    ("RoughnessTexture", "R"),
    ("AOTexture", "AO"),
    ("HeightTexture", "H"),
]

# Scalar parameters inherited from the retired procedural master.
#
# crack_depth and erosion_strength are wired into the new shader because their authored values
# already order the seven surfaces correctly for what they now drive: how pronounced the relief is,
# and how rough the surface is. The other six describe a procedural HSV rock that no longer exists.
# They are declared and held live -- see the zero-weight sink in build_master() -- rather than
# dropped, because dropping them would throw away hand-authored values with no way to get them back.
LEGACY_SCALARS_WIRED = ["crack_depth", "erosion_strength"]
LEGACY_SCALARS_PRESERVED = ["base_hue", "saturation", "value", "crack_density",
                            "strata_angle", "sand_overlay"]

# Neutral by design. The palette work happens in the textures; these exist so a level artist can
# push a single instance without regenerating anything. TileScale multiplies the mesh UVs, and 1.0
# means "whatever the mesh already says", which is the only safe default without auditing the UV
# layout of every kit mesh.
DEFAULT_TILE_SCALE = 1.0
DEFAULT_TINT = unreal.LinearColor(1.0, 1.0, 1.0, 1.0)

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary


def param_report(material, label):
    """Print the full parameter surface of a material. This is the contract that must not drift."""
    report = {}
    for kind, fn in (("scalar", "get_scalar_parameter_names"),
                     ("vector", "get_vector_parameter_names"),
                     ("texture", "get_texture_parameter_names"),
                     ("switch", "get_static_switch_parameter_names")):
        names = sorted(str(n) for n in getattr(MEL, fn)(material))
        report[kind] = names
        print("MATFORGE: %s %-8s (%d): %s" % (label, kind, len(names), ", ".join(names) or "-"))
    return report


def expr(material, cls, x, y):
    node = MEL.create_material_expression(material, cls, x, y)
    if node is None:
        raise RuntimeError("create_material_expression failed for %s" % cls.__name__)
    return node


def wire(src, dst, label, src_outs=("RGB", ""), dst_ins=("A",)):
    """Connect two expressions, trying the plausible pin spellings and failing loudly if none take.

    connect_material_expressions resolves pins by name and returns False rather than raising when
    the name is wrong, so an unchecked call produces a material that compiles to black with no
    error anywhere. Every connection in this file goes through here.
    """
    for so in src_outs:
        for di in dst_ins:
            if MEL.connect_material_expressions(src, so, dst, di):
                return
    raise RuntimeError("could not wire %s: no pin match for outputs %r into inputs %r"
                       % (label, list(src_outs), list(dst_ins)))


def wire_prop(src, prop, label, src_outs=("RGB", "")):
    for so in src_outs:
        if MEL.connect_material_property(src, so, prop):
            return
    raise RuntimeError("could not wire %s to %s: no output pin matched %r"
                       % (label, prop, list(src_outs)))


def scalar(material, name, default, x, y):
    node = expr(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", default)
    return node


def constant(material, value, x, y):
    node = expr(material, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property("r", value)
    return node


def sampler(material, name, texture, sampler_type, x, y):
    node = expr(material, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("texture", texture)
    node.set_editor_property("sampler_type", sampler_type)
    return node


def load_texture(set_name, suffix):
    path = "%s/T_Sanctuary_%s_%s" % (TEXTURE_ROOT, set_name, suffix)
    tex = EAL.load_asset(path)
    if tex is None:
        raise RuntimeError("missing texture %s (run import_sanctuary_textures.py first)" % path)
    return tex


def build_master():
    if EAL.does_asset_exist(NEW_MASTER):
        # Rerunnable: the master is rebuilt from scratch each time so the graph is a pure function of
        # this script. The instances are repointed at the fresh master later in the same run, so the
        # dangling reference never outlives the process.
        print("MATFORGE: replacing existing %s" % NEW_MASTER)
        EAL.delete_asset(NEW_MASTER)

    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NEW_MASTER_NAME, NEW_MASTER_PATH, unreal.Material, unreal.MaterialFactoryNew())
    if material is None:
        raise RuntimeError("could not create %s" % NEW_MASTER)

    st = unreal.MaterialSamplerType

    # --- UVs -------------------------------------------------------------------------------------
    tex_coord = expr(material, unreal.MaterialExpressionTextureCoordinate, -1400, 0)
    tile_scale = scalar(material, "TileScale", DEFAULT_TILE_SCALE, -1400, 150)
    uv = expr(material, unreal.MaterialExpressionMultiply, -1150, 40)
    wire(tex_coord, uv, "TexCoord->UV.A", src_outs=("",), dst_ins=("A",))
    wire(tile_scale, uv, "TileScale->UV.B", src_outs=("",), dst_ins=("B",))

    # --- Samplers --------------------------------------------------------------------------------
    defaults = {suffix: load_texture("Stone", suffix) for _n, suffix in TEXTURE_PARAMS}
    types = {"BC": st.SAMPLERTYPE_COLOR, "N": st.SAMPLERTYPE_NORMAL, "R": st.SAMPLERTYPE_MASKS,
             "AO": st.SAMPLERTYPE_MASKS, "H": st.SAMPLERTYPE_MASKS}
    samplers = {}
    for i, (pname, suffix) in enumerate(TEXTURE_PARAMS):
        node = sampler(material, pname, defaults[suffix], types[suffix], -900, -400 + i * 260)
        wire(uv, node, "UV->%s" % pname, src_outs=("",), dst_ins=("Coordinates", "UVs"))
        samplers[suffix] = node

    # --- BaseColor: texture * TintColor -----------------------------------------------------------
    tint = expr(material, unreal.MaterialExpressionVectorParameter, -900, -700)
    tint.set_editor_property("parameter_name", "TintColor")
    tint.set_editor_property("default_value", DEFAULT_TINT)
    # A VectorParameter's default output is float4; BaseColor is float3. The mask makes the type
    # explicit rather than relying on the translator to truncate.
    tint_rgb = expr(material, unreal.MaterialExpressionComponentMask, -640, -700)
    tint_rgb.set_editor_property("r", True)
    tint_rgb.set_editor_property("g", True)
    tint_rgb.set_editor_property("b", True)
    tint_rgb.set_editor_property("a", False)
    wire(tint, tint_rgb, "TintColor->Mask", src_outs=("",), dst_ins=("", "Input"))

    base_color = expr(material, unreal.MaterialExpressionMultiply, -420, -560)
    wire(samplers["BC"], base_color, "BC->BaseColor.A", dst_ins=("A",))
    wire(tint_rgb, base_color, "Tint->BaseColor.B", src_outs=("",), dst_ins=("B",))

    # --- Normal: relief scaled by the inherited crack_depth ---------------------------------------
    flat = expr(material, unreal.MaterialExpressionConstant3Vector, -900, 60)
    flat.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    crack_depth = scalar(material, "crack_depth", 0.5, -900, 200)
    relief_bias = constant(material, 0.35, -900, 300)
    relief = expr(material, unreal.MaterialExpressionAdd, -640, 250)
    wire(crack_depth, relief, "crack_depth->relief.A", src_outs=("",), dst_ins=("A",))
    wire(relief_bias, relief, "bias->relief.B", src_outs=("",), dst_ins=("B",))
    normal = expr(material, unreal.MaterialExpressionLinearInterpolate, -420, 60)
    wire(flat, normal, "flat->Normal.A", src_outs=("",), dst_ins=("A",))
    wire(samplers["N"], normal, "N->Normal.B", dst_ins=("B",))
    wire(relief, normal, "relief->Normal.Alpha", src_outs=("",), dst_ins=("Alpha",))

    # --- Roughness: texture, softened where erosion_strength is low -------------------------------
    erosion = scalar(material, "erosion_strength", 0.75, -900, 520)
    soften = constant(material, 0.70, -900, 600)
    rough_soft = expr(material, unreal.MaterialExpressionMultiply, -640, 560)
    wire(samplers["R"], rough_soft, "R->soft.A", dst_ins=("A",))
    wire(soften, rough_soft, "0.7->soft.B", src_outs=("",), dst_ins=("B",))
    roughness = expr(material, unreal.MaterialExpressionLinearInterpolate, -420, 480)
    wire(rough_soft, roughness, "soft->Rough.A", src_outs=("",), dst_ins=("A",))
    wire(samplers["R"], roughness, "R->Rough.B", dst_ins=("B",))
    wire(erosion, roughness, "erosion->Rough.Alpha", src_outs=("",), dst_ins=("Alpha",))

    # --- Compatibility sink -----------------------------------------------------------------------
    #
    # The six retired scalars, plus HeightTexture (which has no consumer until this material grows a
    # parallax or tessellation path), are summed and multiplied by a hard zero, then added into
    # BaseColor. The arithmetic contributes nothing; the point is that every one of these parameters
    # is REACHABLE from a material output, which is what guarantees the engine enumerates it on a
    # material instance and therefore what guarantees the instances' authored overrides survive the
    # reparent. Leaving the expressions disconnected would rely on the engine's parameter-gathering
    # traversal including orphans -- true today, but not a contract, and not worth betting seven
    # hand-authored parameter sets on.
    legacy = [scalar(material, name, 0.0, -1400, 700 + i * 90)
              for i, name in enumerate(LEGACY_SCALARS_PRESERVED)]
    acc = legacy[0]
    for i, node in enumerate(legacy[1:]):
        nxt = expr(material, unreal.MaterialExpressionAdd, -1100, 700 + i * 90)
        wire(acc, nxt, "legacy sum %d.A" % i, src_outs=("",), dst_ins=("A",))
        wire(node, nxt, "legacy sum %d.B" % i, src_outs=("",), dst_ins=("B",))
        acc = nxt

    zero = constant(material, 0.0, -1100, 1300)
    sink_scalars = expr(material, unreal.MaterialExpressionMultiply, -860, 1150)
    wire(acc, sink_scalars, "legacy->sink.A", src_outs=("",), dst_ins=("A",))
    wire(zero, sink_scalars, "zero->sink.A", src_outs=("",), dst_ins=("B",))

    sink_height = expr(material, unreal.MaterialExpressionMultiply, -860, 1300)
    wire(samplers["H"], sink_height, "H->sink.A", dst_ins=("A",))
    wire(zero, sink_height, "zero->sink.B", src_outs=("",), dst_ins=("B",))

    sink = expr(material, unreal.MaterialExpressionAdd, -640, 1200)
    wire(sink_scalars, sink, "sink scalars", src_outs=("",), dst_ins=("A",))
    wire(sink_height, sink, "sink height", src_outs=("",), dst_ins=("B",))

    base_color_final = expr(material, unreal.MaterialExpressionAdd, -220, -500)
    wire(base_color, base_color_final, "BaseColor->final.A", src_outs=("",), dst_ins=("A",))
    wire(sink, base_color_final, "sink->final.B", src_outs=("",), dst_ins=("B",))

    # --- Outputs -----------------------------------------------------------------------------------
    wire_prop(base_color_final, unreal.MaterialProperty.MP_BASE_COLOR, "BaseColor", src_outs=("",))
    wire_prop(normal, unreal.MaterialProperty.MP_NORMAL, "Normal", src_outs=("",))
    wire_prop(roughness, unreal.MaterialProperty.MP_ROUGHNESS, "Roughness", src_outs=("",))
    wire_prop(samplers["AO"], unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, "AO")

    MEL.recompile_material(material)
    EAL.save_loaded_asset(material)
    return material


def main():
    # The old master's parameter surface is printed FIRST and in full: it is the specification the
    # new one has to match, and reading it off the asset is the only way to know it, rather than
    # trusting a name that looks right.
    old = EAL.load_asset(OLD_MASTER)
    if old is None:
        raise RuntimeError("could not load the existing master %s" % OLD_MASTER)
    old_params = param_report(old, "OLD")

    material = build_master()
    new_params = param_report(material, "NEW")

    missing = []
    for kind in ("scalar", "vector", "texture", "switch"):
        for name in old_params[kind]:
            if name not in new_params[kind]:
                missing.append("%s %s" % (kind, name))
    if missing:
        raise RuntimeError("the new master drops parameters the instances override: "
                           + ", ".join(missing))
    print("MATFORGE: every parameter of the old master is present on the new one")

    failures = []
    for suffix, set_name in BINDINGS:
        path = "%s/MI_Sanctuary_%s" % (INSTANCE_ROOT, suffix)
        mi = EAL.load_asset(path)
        if mi is None:
            failures.append("%s did not load" % path)
            continue

        before = {str(v.get_editor_property("parameter_info").get_editor_property("name")):
                  v.get_editor_property("parameter_value")
                  for v in mi.get_editor_property("scalar_parameter_values")}

        mi.set_editor_property("parent", material)
        for pname, channel in TEXTURE_PARAMS:
            MEL.set_material_instance_texture_parameter_value(mi, pname,
                                                              load_texture(set_name, channel))
        MEL.set_material_instance_scalar_parameter_value(mi, "TileScale", DEFAULT_TILE_SCALE)
        MEL.set_material_instance_vector_parameter_value(mi, "TintColor", DEFAULT_TINT)
        MEL.update_material_instance(mi)
        EAL.save_loaded_asset(mi)

        after = {str(v.get_editor_property("parameter_info").get_editor_property("name")):
                 v.get_editor_property("parameter_value")
                 for v in mi.get_editor_property("scalar_parameter_values")}
        lost = sorted(n for n in before if n not in after)
        if lost:
            failures.append("%s lost scalar override(s) %s" % (path, ", ".join(lost)))
        print("MATFORGE: MI_Sanctuary_%-12s -> %s  set=%s  scalars kept %d/%d"
              % (suffix, NEW_MASTER_NAME, set_name, len(before) - len(lost), len(before)))

    if failures:
        raise RuntimeError("MATFORGE FAILED: " + "; ".join(failures))
    print("MATFORGE: complete, %d instance(s) on %s" % (len(BINDINGS), NEW_MASTER))


main()
