"""
Import the Houdini-forged gloam meshes into Gloamstead, headless.

Run with:
    UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript
        -script="procedural/houdini/import_gloam_assets.py" -unattended -nullrhi -nosplash -nopause
    (the FBX source directory is read from GLOAM_FORGE_FBX_DIR)

The counterpart to forge_gloam_assets.py. Together they are the whole pipeline from a deterministic
script to a .uasset the game loads, with no editor GUI at any step.

Destination is /Game/Gloamstead/World, which is one of the roots content_policy.json permits for
generated binary output. Nothing here writes outside it, and nothing here touches vendor content.
"""
import os

import unreal

FBX_DIR = os.environ.get("GLOAM_FORGE_FBX_DIR", "")
DEST = "/Game/Gloamstead/World"

# Houdini works in metres here; Unreal works in centimetres. Without this every forged mesh imports
# at 1/100 scale and reads as a speck sitting exactly where it should be, which is the most
# confusing possible failure.
UNIFORM_SCALE = 100.0

ASSETS = [
    "SM_Gloam_Growth_Small",
    "SM_Gloam_Growth_Medium",
    "SM_Gloam_Growth_Large",
    "SM_Gloam_Shroud_Gatherer",
    "SM_Gloam_Shroud_Borrowed",
    "SM_Gloam_Shroud_Bargainer",
    "SM_Gloam_Shroud_Echo",
]


def build_options():
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_textures = False
    options.import_materials = False
    options.import_as_skeletal = False
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    data = options.static_mesh_import_data
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("generate_lightmap_u_vs", True)
    data.set_editor_property("import_uniform_scale", UNIFORM_SCALE)
    # The forge script already computes normals; recomputing them here would discard the hard edges
    # that make a crystalline growth read as crystalline.
    data.set_editor_property("normal_import_method",
                             unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    return options


def main():
    if not FBX_DIR or not os.path.isdir(FBX_DIR):
        raise RuntimeError("GLOAM_FORGE_FBX_DIR is unset or not a directory: %r" % FBX_DIR)

    tasks = []
    for name in ASSETS:
        path = os.path.join(FBX_DIR, name + ".fbx")
        if not os.path.exists(path):
            raise RuntimeError("missing forged source: %s (run forge_gloam_assets.py first)" % path)
        task = unreal.AssetImportTask()
        task.filename = path
        task.destination_path = DEST
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = True
        task.options = build_options()
        tasks.append(task)

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

    # Verify by loading, not by trusting the importer's return. An import task reports success for a
    # file it produced nothing usable from.
    failures = []
    for name in ASSETS:
        asset = unreal.EditorAssetLibrary.load_asset("%s/%s" % (DEST, name))
        if asset is None:
            failures.append("%s did not load" % name)
            continue
        triangles = asset.get_num_triangles(0)
        vertices = asset.get_num_vertices(0)
        bounds = asset.get_bounds().box_extent
        print("GLOAMIMPORT: %-28s tris=%-6d verts=%-6d extent=(%.0f, %.0f, %.0f)"
              % (name, triangles, vertices, bounds.x, bounds.y, bounds.z))
        if triangles < 40:
            failures.append("%s imported only %d triangles" % (name, triangles))

    # Remove the feasibility spike if it is still sitting in the content tree. It proved the pipeline
    # and has no business shipping.
    spike = "%s/SM_Spike_HoudiniProof" % DEST
    if unreal.EditorAssetLibrary.does_asset_exist(spike):
        unreal.EditorAssetLibrary.delete_asset(spike)
        print("GLOAMIMPORT: removed the pipeline spike asset")

    if failures:
        raise RuntimeError("GLOAMIMPORT FAILED: " + "; ".join(failures))
    print("GLOAMIMPORT: complete, %d asset(s) under %s" % (len(ASSETS), DEST))


main()
