"""
Generate Gloamstead's gloam meshes in Houdini, headless.

Run with:
    hython forge_gloam_assets.py <output_dir>

This exists because the project's art was entirely hand-authored or absent: the night threats wore the
stock UE mannequin, and corruption - the thing the whole night phase is about - was communicated by
three flat decals. Both are visual gaps that need geometry, and geometry is what Houdini is for.

Everything here is deterministic. No scatter seeds, no randomness that could make two runs disagree:
the same script produces byte-comparable geometry every time, which is what makes a generated asset
reviewable rather than a one-off nobody can reproduce.

Two families:
  SM_Gloam_Growth_{Small,Medium,Large} - crystalline rot pushing up out of the ground at a corrupted
    ritual point. Severity picks the variant, so corruption reads as three-dimensional growth rather
    than as a stain that only exists when you look down.
  SM_Gloam_Shroud_{Gatherer,Borrowed,Bargainer,Echo} - a ragged shroud worn over the threat body.
    A bespoke skinned creature per archetype is real character work; a shroud over the existing
    skeleton is not, and it is the difference between "a UE mannequin is walking at me" and a
    silhouette that belongs to this game.
"""
import os
import sys

import hou


def set_parms(node, **parms):
    """Set parms, reporting any the node does not have instead of failing silently."""
    for name, value in parms.items():
        parm = node.parm(name)
        if parm is None:
            parm = node.parmTuple(name)
        if parm is None:
            print("  WARN %s has no parm '%s'" % (node.type().name(), name))
            continue
        parm.set(value)


def shard(geo, name, rad_base, rad_tip, height, translate, rotate, cols=6):
    """One tapered spike. The unit the growths are built from."""
    tube = geo.createNode("tube", name)
    # type=1 is Polygon. type=0 is Primitive - a single point and a single prim - which exports an
    # FBX that looks plausible by file size and contains no usable game geometry at all.
    set_parms(tube, type=1, cap=1, rad1=rad_base, rad2=rad_tip, height=height, cols=cols, rows=3)
    xf = geo.createNode("xform", name + "_xf")
    xf.setInput(0, tube)
    set_parms(xf, t=translate, r=rotate)
    return xf


def finish(geo, merged, noise_height, noise_size):
    """Roughen, unwrap and normal a merged shape so it imports as a usable game mesh."""
    mountain = geo.createNode("mountain", "rough")
    mountain.setInput(0, merged)
    set_parms(mountain, height=noise_height, elementsize=noise_size)

    uv = geo.createNode("uvunwrap", "unwrap")
    uv.setInput(0, mountain)

    normal = geo.createNode("normal", "normals")
    normal.setInput(0, uv)
    normal.setDisplayFlag(True)
    normal.setRenderFlag(True)
    return normal


def build_growth(obj, label, shard_count, scale):
    """Crystalline rot: a ring of shards leaning outward around a taller core."""
    geo = obj.createNode("geo", "growth_" + label)
    shards = []

    core = shard(geo, "core", 0.16 * scale, 0.012 * scale, 1.05 * scale, (0, 0.52 * scale, 0), (0, 0, 0))
    shards.append(core)

    import math
    for i in range(shard_count):
        angle = (360.0 / shard_count) * i
        rad = math.radians(angle)
        dist = 0.30 * scale
        lean = 26.0 + (i % 3) * 9.0
        height = (0.44 + 0.16 * (i % 3)) * scale
        shards.append(shard(
            geo, "shard%d" % i,
            0.085 * scale, 0.008 * scale, height,
            (math.cos(rad) * dist, height * 0.42, math.sin(rad) * dist),
            (lean * math.sin(rad), angle, -lean * math.cos(rad)),
            cols=5))

    merge = geo.createNode("merge", "merge")
    for idx, node in enumerate(shards):
        merge.setInput(idx, node)

    out = finish(geo, merge, 0.02 * scale, 0.30 * scale)
    geo.layoutChildren()
    return geo, out


def build_shroud(obj, label, height, flare, rag):
    """A tattered cloak: an open cone, cut into by noise so its hem reads as torn."""
    geo = obj.createNode("geo", "shroud_" + label)

    tube = geo.createNode("tube", "cloak")
    set_parms(tube, type=1, cap=0, rad1=flare * 0.34, rad2=flare, height=height, cols=18, rows=10)
    xf = geo.createNode("xform", "cloak_xf")
    xf.setInput(0, tube)
    set_parms(xf, t=(0, height * 0.5, 0))

    # Thicken so it is not a single-sided surface - a one-sided cloak vanishes from behind in UE.
    thick = geo.createNode("polyextrude", "thicken")
    thick.setInput(0, xf)
    set_parms(thick, dist=0.015, outputback=True)

    # Tear the hem. Heavy low-frequency noise pushed along the surface reads as cloth rot.
    tear = geo.createNode("mountain", "tear")
    tear.setInput(0, thick)
    set_parms(tear, height=rag, elementsize=0.42)

    uv = geo.createNode("uvunwrap", "unwrap")
    uv.setInput(0, tear)

    normal = geo.createNode("normal", "normals")
    normal.setInput(0, uv)
    normal.setDisplayFlag(True)
    normal.setRenderFlag(True)

    geo.layoutChildren()
    return geo, normal


def export(obj, geo_node, out_node, out_dir, asset_name, min_prims=64):
    geometry = out_node.geometry()
    points, prims = len(geometry.points()), len(geometry.prims())
    if points == 0 or prims == 0:
        raise RuntimeError("%s cooked empty (points=%d prims=%d)" % (asset_name, points, prims))
    # A floor, not just a non-zero check. The first run of this script exported seven FBXs of 80-180 KB
    # that each contained a handful of PRIMITIVE tubes - one point and one prim apiece - because the
    # Tube SOP's type parm defaults to Primitive, not Polygon. Every one of them "succeeded": non-zero
    # points, non-zero prims, plausible file size. Only a poly count too low to be a mesh catches it.
    if prims < min_prims:
        raise RuntimeError(
            "%s has %d prims, below the %d floor - this is what a Primitive-type tube looks like, "
            "and it exports a file that is not game geometry" % (asset_name, prims, min_prims))

    path = os.path.join(out_dir, asset_name + ".fbx")
    ropnet = obj.createNode("ropnet", "rop_" + asset_name)
    fbx = ropnet.createNode("filmboxfbx")
    set_parms(fbx, sopoutput=path, startnode=geo_node.path())
    fbx.render()

    size = os.path.getsize(path) if os.path.exists(path) else 0
    print("FORGE: %-34s points=%-6d prims=%-6d bytes=%d" % (asset_name, points, prims, size))
    if size == 0:
        raise RuntimeError("%s exported nothing" % asset_name)
    return path


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.getcwd()
    os.makedirs(out_dir, exist_ok=True)
    obj = hou.node("/obj")

    # Gloam growths. Severity is expressed as shard count AND scale, so a bad bloom is visibly a
    # bigger, busier thing rather than the same object drawn larger.
    for label, shard_count, scale, asset in (
        ("small",  5,  0.55, "SM_Gloam_Growth_Small"),
        ("medium", 8,  0.95, "SM_Gloam_Growth_Medium"),
        ("large",  12, 1.45, "SM_Gloam_Growth_Large"),
    ):
        geo, out = build_growth(obj, label, shard_count, scale)
        export(obj, geo, out, out_dir, asset)

    # Threat shrouds. The proportions carry the archetype: the Gatherer is squat and heavy, the
    # Bargainer tall and narrow because it stands at the edge of the light, the Echo small and
    # barely there because it drains nothing.
    for label, height, flare, rag, asset in (
        ("gatherer",  1.55, 0.62, 0.075, "SM_Gloam_Shroud_Gatherer"),
        ("borrowed",  1.75, 0.48, 0.045, "SM_Gloam_Shroud_Borrowed"),
        ("bargainer", 2.05, 0.42, 0.095, "SM_Gloam_Shroud_Bargainer"),
        ("echo",      1.30, 0.38, 0.120, "SM_Gloam_Shroud_Echo"),
    ):
        geo, out = build_shroud(obj, label, height, flare, rag)
        export(obj, geo, out, out_dir, asset)

    print("FORGE: complete, 7 asset(s) in %s" % out_dir)


if __name__ == "__main__":
    main()
