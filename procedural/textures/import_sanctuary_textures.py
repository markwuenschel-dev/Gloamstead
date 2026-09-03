"""
Import the forged sanctuary textures into Gloamstead, headless.

Run with:
    UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript
        -script="procedural/textures/import_sanctuary_textures.py" -unattended -nullrhi -nosplash -nopause
    (the PNG source directory is read from GLOAM_TEXTURE_PNG_DIR)

The counterpart to forge_sanctuary_textures.py, and the direct sibling of
procedural/houdini/import_gloam_assets.py -- same shape, same contract: a deterministic generator
writes source files, this script turns them into .uassets the game loads, and no editor GUI is
involved at any step.

WHY THE PER-CHANNEL SETTINGS MATTER
    A texture imported with the wrong sRGB flag is not slightly wrong, it is wrong everywhere. A
    roughness map read as sRGB is gamma-decoded on sample, so every surface in the sanctuary ends up
    smoother than authored in the mid-tones. A normal map imported as a colour texture is stored in
    DXT1 and gamma-mangled, and the lighting reads as if the geometry were made of foil. So this
    script does not accept the importer's defaults: it states the intent per channel, applies it,
    and then reads it back off the saved asset.

Destination is /Game/Gloamstead/Kit/Textures. Nothing here writes outside it, and nothing here
touches vendor content or Content/Textures/Terrain.
"""
import os

import unreal

PNG_DIR = os.environ.get("GLOAM_TEXTURE_PNG_DIR", "")
DEST = "/Game/Gloamstead/Kit/Textures"

SETS = ["Stone", "Stone_Dark", "Paving", "Soil", "Iron", "Lichen", "Weathered"]

# suffix -> (sRGB, compression settings, human name)
#
# BC is the only channel that carries perceptual colour, so it is the only one that is sRGB. N is a
# vector field; R/AO/H are scalar data. TC_Masks is the right home for the latter three: it is
# uncompressed-alpha-free, linear, and does not apply the normal-map-specific reconstruction that
# TC_Normalmap does.
CHANNELS = {
    "BC": (True, unreal.TextureCompressionSettings.TC_DEFAULT, "BaseColor"),
    "N": (False, unreal.TextureCompressionSettings.TC_NORMALMAP, "Normal"),
    "R": (False, unreal.TextureCompressionSettings.TC_MASKS, "Roughness"),
    "AO": (False, unreal.TextureCompressionSettings.TC_MASKS, "AmbientOcclusion"),
    "H": (False, unreal.TextureCompressionSettings.TC_MASKS, "Height"),
}

EXPECTED_SIZE = 1024


def asset_names():
    for set_name in SETS:
        for suffix in CHANNELS:
            yield set_name, suffix, "T_Sanctuary_%s_%s" % (set_name, suffix)


def build_task(path, name):
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    # Saved explicitly after the per-channel properties are applied, not here -- saving at import
    # time would persist the importer's guess at sRGB and compression, and the corrected values
    # would then be a second, unnecessary revision of the package.
    task.save = False
    return task


def main():
    if not PNG_DIR or not os.path.isdir(PNG_DIR):
        raise RuntimeError("GLOAM_TEXTURE_PNG_DIR is unset or not a directory: %r" % PNG_DIR)

    tasks = []
    for _set_name, _suffix, name in asset_names():
        path = os.path.join(PNG_DIR, name + ".png")
        if not os.path.exists(path):
            raise RuntimeError("missing forged source: %s (run forge_sanctuary_textures.py first)" % path)
        tasks.append(build_task(path, name))

    print("TEXIMPORT: importing %d PNG(s) from %s into %s" % (len(tasks), PNG_DIR, DEST))
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    failures = []
    for _set_name, suffix, name in asset_names():
        obj_path = "%s/%s" % (DEST, name)
        asset = unreal.EditorAssetLibrary.load_asset(obj_path)
        if asset is None:
            failures.append("%s did not load after import" % name)
            continue
        if not isinstance(asset, unreal.Texture2D):
            failures.append("%s imported as %s, not a Texture2D" % (name, type(asset).__name__))
            continue

        srgb, compression, _label = CHANNELS[suffix]
        asset.set_editor_property("srgb", srgb)
        asset.set_editor_property("compression_settings", compression)
        # World is the right LOD group for surface detail: it inherits the project's world texture
        # budget instead of the UI/character ones.
        asset.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
        if suffix == "N":
            # The generator writes the DirectX / green-down orientation Unreal expects, so the
            # importer must not flip it back. Stated rather than left to the default.
            asset.set_editor_property("flip_green_channel", False)
        unreal.EditorAssetLibrary.save_loaded_asset(asset)

    # Verify by re-loading from disk, not by trusting the setters. A set_editor_property that did
    # not stick reports nothing at all.
    for _set_name, suffix, name in asset_names():
        obj_path = "%s/%s" % (DEST, name)
        asset = unreal.EditorAssetLibrary.load_asset(obj_path)
        if asset is None:
            failures.append("%s did not re-load after save" % name)
            continue
        want_srgb, want_compression, label = CHANNELS[suffix]
        got_srgb = asset.get_editor_property("srgb")
        got_compression = asset.get_editor_property("compression_settings")
        # UTexture's "imported_size" is not an exposed editor property in UE 5.8; the dimensions
        # come off the Texture2D accessors instead.
        width = int(asset.blueprint_get_size_x())
        height = int(asset.blueprint_get_size_y())

        if got_srgb != want_srgb:
            failures.append("%s sRGB is %r, expected %r" % (name, got_srgb, want_srgb))
        if got_compression != want_compression:
            failures.append("%s compression is %s, expected %s" % (name, got_compression, want_compression))
        if width != EXPECTED_SIZE or height != EXPECTED_SIZE:
            failures.append("%s is %dx%d, expected %dx%d"
                            % (name, width, height, EXPECTED_SIZE, EXPECTED_SIZE))

        print("TEXIMPORT: %-32s %-17s %4dx%-4d srgb=%-5s %s"
              % (name, label, width, height, got_srgb, got_compression))

    if failures:
        raise RuntimeError("TEXIMPORT FAILED: " + "; ".join(failures))
    print("TEXIMPORT: complete, %d texture(s) under %s" % (len(SETS) * len(CHANNELS), DEST))


main()
