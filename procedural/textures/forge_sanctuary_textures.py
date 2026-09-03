"""
Forge the Gloamstead sanctuary texture set, deterministically, from nothing but the standard library.

WHY THIS EXISTS
    Content/Gloamstead shipped with zero textures of its own. Every MI_Sanctuary_* instance inherited
    a master that sampled Content/Textures/Terrain/T_Terrain_Rock_Desert_Ash_01_* -- a *desert rock*
    set. Desert sand is the one thing the locked art direction (docs/art/04_art_direction.md,
    "Withered Gothic Stylization") has no room for: the baseline world is ashen gray, cold blue,
    blue-black, deep umber, dead moss green, wet stone neutral, rusted iron brown, old bone beige.
    Warm yellow sand reads as the wrong game. This script authors the replacement.

WHY PURE STDLIB
    No numpy, no PIL. PNGs are emitted by hand with zlib + struct. The pipeline has to run on any
    machine that can run the engine, with no wheel to install and no version of a third-party
    library to drift underneath it. That is the same reason the Houdini forge scripts are
    dependency-light: a content pipeline that needs a bespoke environment is a content pipeline that
    stops working.

WHY DETERMINISTIC
    Every noise source is seeded from SEED below and from an explicit integer key -- there is no
    call to random, no time, no dict-iteration order and no floating-point accumulation that depends
    on anything but the inputs. Two runs produce byte-identical PNGs. That is what makes the
    generated .uasset reviewable: a diff in Content means somebody changed the *recipe*, not that
    the generator rolled different dice.

WHY THE RESTRAINT
    docs/art/04_art_direction.md, "Texture Rules": *avoid high-frequency noise everywhere* and
    *background decay should be quieter than meaningful structures*. Every fBm here runs with a
    falling amplitude and a hard octave cap, and DETAIL_GAIN globally damps the finest octaves. The
    structure a surface carries lives in its mid frequencies -- flagstone joints, crack networks,
    rust blooms, lichen patches -- not in a uniform sandpaper hiss laid over the whole tile.

USAGE
    python forge_sanctuary_textures.py <output-directory>

    Writes T_Sanctuary_<Set>_{BC,N,R,AO,H}.png at 1024x1024 and prints the SHA-256 of each file.
    The suffix vocabulary matches the repo's existing texture convention
    (T_Terrain_Rock_Desert_Ash_01_{AO,BC,H,N,R}) so the importer's naming is not a new dialect.

    The companion is import_sanctuary_textures.py, which reads this directory from
    GLOAM_TEXTURE_PNG_DIR.
"""

import hashlib
import math
import os
import struct
import sys
import zlib

# ----------------------------------------------------------------------------------------------
# Determinism
# ----------------------------------------------------------------------------------------------

# The single root of every random number in this file. Change it and the whole set changes; leave
# it alone and the set is reproducible forever.
SEED = 0x67104D  # "gloam"

SIZE = 1024

# Global damping on the finest octave of every fBm. See "WHY THE RESTRAINT" above -- this is the
# knob that keeps the art direction's "avoid high-frequency noise everywhere" rule enforceable in
# one place instead of re-argued per material.
DETAIL_GAIN = 0.5


def _rand(*key):
    """Deterministic float in [0, 1) from an arbitrary tuple of integers/strings.

    A hand-rolled FNV-ish mix rather than random.Random, because this must not depend on the
    behaviour of a stdlib PRNG across interpreter versions.
    """
    h = SEED & 0xFFFFFFFF
    for k in key:
        h = ((h ^ _int_of(k)) * 0x01000193) & 0xFFFFFFFF
        h ^= h >> 15
    h = (h * 0x2545F491) & 0xFFFFFFFF
    h ^= h >> 16
    h = (h * 0x27220A95) & 0xFFFFFFFF
    return (h ^ (h >> 15)) / 4294967296.0


def _int_of(k):
    if isinstance(k, str):
        v = 0
        for ch in k:
            v = ((v * 131) + ord(ch)) & 0xFFFFFFFF
        return v
    if isinstance(k, tuple):
        v = 0
        for sub in k:
            v = ((v * 131) + _int_of(sub)) & 0xFFFFFFFF
        return v
    return int(k) & 0xFFFFFFFF


# ----------------------------------------------------------------------------------------------
# Palette -- every colour in this file is sampled from docs/art/04_art_direction.md "Color Language"
# ----------------------------------------------------------------------------------------------
#
# These are sRGB display values. BaseColor PNGs import with sRGB ON, so the bytes written are the
# bytes the artist would have picked in a colour wheel -- no linearisation happens here, and none
# should. Roughness / AO / Height import with sRGB OFF and are therefore linear by construction.

PALETTE = {
    # Baseline world
    "ashen_gray_dark":   (0x38, 0x3B, 0x3E),
    "ashen_gray":        (0x56, 0x59, 0x5B),
    "ashen_gray_light":  (0x76, 0x79, 0x7A),
    "cold_blue":         (0x3A, 0x46, 0x54),
    "cold_blue_deep":    (0x24, 0x2E, 0x3A),
    "blue_black":        (0x13, 0x17, 0x1D),
    "blue_black_light":  (0x2B, 0x32, 0x3B),
    "deep_umber":        (0x2C, 0x22, 0x1A),
    "deep_umber_light":  (0x4B, 0x3C, 0x2C),
    "dead_moss_green":   (0x4A, 0x53, 0x3F),
    "dead_moss_light":   (0x61, 0x6C, 0x4E),
    "wet_stone":         (0x4E, 0x51, 0x52),
    "wet_stone_dark":    (0x30, 0x34, 0x36),
    "rusted_iron_brown": (0x6E, 0x40, 0x27),
    "rusted_iron_light": (0x8A, 0x55, 0x33),
    "old_bone_beige":    (0xAE, 0xA6, 0x92),
    "old_bone_dim":      (0x8B, 0x83, 0x72),
    "old_bone_gray":     (0x6C, 0x68, 0x60),
    "iron_metal":        (0x1E, 0x21, 0x25),
    "iron_metal_light":  (0x35, 0x3A, 0x40),
}


def C(name):
    return PALETTE[name]


# ----------------------------------------------------------------------------------------------
# Field primitives.  A "field" is a list of SIZE rows, each a list of SIZE floats.
# ----------------------------------------------------------------------------------------------

_AXIS_CACHE = {}


def _axis(period):
    """Index/weight tables mapping SIZE output samples onto a lattice of `period` cells.

    Wrapping is exact for ANY integer period, not only divisors of SIZE: sample x maps to
    t = x * period / SIZE, so x = SIZE lands on lattice cell `period`, which is cell 0 modulo
    `period`. That is the whole tiling argument, and it is why the period does not have to be a
    power of two -- 5x6 flagstones and a 150-cell lichen lattice are as seamless as a 64 is.
    """
    if period < 2:
        raise ValueError("lattice period must be >= 2, got %r" % (period,))
    tables = _AXIS_CACHE.get(period)
    if tables is None:
        i0, i1, wt = [], [], []
        scale = period / float(SIZE)
        for x in range(SIZE):
            t = x * scale
            a = int(t)
            f = t - a
            f = f * f * (3.0 - 2.0 * f)  # smoothstep: C1 across cell borders, no lattice grid
            i0.append(a % period)
            i1.append((a + 1) % period)
            wt.append(f)
        tables = (i0, i1, wt)
        _AXIS_CACHE[period] = tables
    return tables


def value_noise(period_x, period_y, key):
    """Tileable value noise. Separable upsample: expand lattice rows first, then blend between them."""
    period_x = min(period_x, SIZE)
    period_y = min(period_y, SIZE)
    xi0, xi1, xw = _axis(period_x)
    yi0, yi1, yw = _axis(period_y)
    lattice = [[_rand(key, j, i) for i in range(period_x)] for j in range(period_y)]
    rows = []
    for j in range(period_y):
        g = lattice[j]
        rows.append([g[a] + (g[b] - g[a]) * t for a, b, t in zip(xi0, xi1, xw)])
    out = []
    for y in range(SIZE):
        a = rows[yi0[y]]
        b = rows[yi1[y]]
        t = yw[y]
        out.append([u + (v - u) * t for u, v in zip(a, b)])
    return out


def fbm(period_x, period_y, octaves, key, gain=0.5):
    """Fractal sum of tileable value noise, normalised to [0, 1].

    The last octave is additionally scaled by DETAIL_GAIN. That is deliberate and art-directed: the
    finest scale is the one that turns a painterly surface into photoreal grime.
    """
    acc = [[0.0] * SIZE for _ in range(SIZE)]
    amp = 1.0
    total = 0.0
    # A lattice finer than one cell per pixel is wasted work, not extra detail; clamp rather than
    # skip, so an aggressive starting period can never silently produce an empty sum.
    px, py = min(period_x, SIZE), min(period_y, SIZE)
    for o in range(octaves):
        a = amp * (DETAIL_GAIN if o == octaves - 1 else 1.0)
        n = value_noise(px, py, (key, o))
        for y in range(SIZE):
            acc[y] = [p + a * v for p, v in zip(acc[y], n[y])]
        total += a
        amp *= gain
        px = min(px * 2, SIZE)
        py = min(py * 2, SIZE)
    inv = 1.0 / total
    return [[v * inv for v in r] for r in acc]


def ridged(field):
    """1 at the field's mid-level contour, 0 at its extremes -- a connected line network.

    This is how a crack reads: not a scattering of dark pixels but a branching curve that goes
    somewhere. docs/art/04_art_direction.md asks for "readable seams ... cracks", not speckle.
    """
    return [[1.0 - abs(2.0 * v - 1.0) for v in r] for r in field]


def smoothstep_field(edge0, edge1, field):
    inv = 1.0 / (edge1 - edge0)
    out = []
    for r in field:
        row = []
        for v in r:
            t = (v - edge0) * inv
            if t <= 0.0:
                row.append(0.0)
            elif t >= 1.0:
                row.append(1.0)
            else:
                row.append(t * t * (3.0 - 2.0 * t))
        out.append(row)
    return out


def lerp_fields(a, b, mask):
    return [[u + (v - u) * m for u, v, m in zip(ra, rb, rm)] for ra, rb, rm in zip(a, b, mask)]


def scale_field(field, k, bias=0.0):
    return [[bias + v * k for v in r] for r in field]


def add_fields(a, b, k=1.0):
    return [[u + v * k for u, v in zip(ra, rb)] for ra, rb in zip(a, b)]


def mul_fields(a, b):
    return [[u * v for u, v in zip(ra, rb)] for ra, rb in zip(a, b)]


def clamp01(field):
    return [[0.0 if v < 0.0 else (1.0 if v > 1.0 else v) for v in r] for r in field]


def normalize_field(field):
    lo = min(min(r) for r in field)
    hi = max(max(r) for r in field)
    if hi - lo < 1e-9:
        return [[0.5] * SIZE for _ in range(SIZE)]
    inv = 1.0 / (hi - lo)
    return [[(v - lo) * inv for v in r] for r in field]


def const_field(v):
    return [[v] * SIZE for _ in range(SIZE)]


def box_blur(field, radius):
    """Separable wrapping box blur via running sums. Wrapping, because the tile has no border."""
    n = SIZE
    w = 2 * radius + 1
    inv = 1.0 / w
    tmp = []
    for row in field:
        ext = row[-radius:] + row + row[:radius]
        s = sum(ext[:w])
        out = [s]
        for i in range(1, n):
            s += ext[i + w - 1] - ext[i - 1]
            out.append(s)
        tmp.append([v * inv for v in out])
    running = [0.0] * n
    for k in range(-radius, radius + 1):
        running = [a + b for a, b in zip(running, tmp[k % n])]
    result = []
    for y in range(n):
        result.append([v * inv for v in running])
        running = [a - b + c for a, b, c in zip(running, tmp[(y - radius) % n], tmp[(y + radius + 1) % n])]
    return result


def scatter(field, count, key, r_min, r_max, amp):
    """Stamp `count` smooth domes into `field`, wrapping at the tile edge.

    Used for the pebbles in soil and the pitting in iron -- things that are objects, not noise.
    Signed `amp`: positive raises (a stone), negative carves (a pit).
    """
    for i in range(count):
        cx = _rand(key, i, 1) * SIZE
        cy = _rand(key, i, 2) * SIZE
        r = r_min + (r_max - r_min) * _rand(key, i, 3)
        a = amp * (0.45 + 0.55 * _rand(key, i, 4))
        ir = int(r) + 1
        r2 = r * r
        icx = int(cx)
        icy = int(cy)
        for dy in range(-ir, ir + 1):
            row = field[(icy + dy) % SIZE]
            dy2 = dy * dy
            if dy2 > r2:
                continue
            span = int(math.sqrt(r2 - dy2))
            for dx in range(-span, span + 1):
                d = (dx * dx + dy2) / r2
                f = 1.0 - d
                x = (icx + dx) % SIZE
                row[x] += a * f * f
    return field


# ----------------------------------------------------------------------------------------------
# Colour ramps
# ----------------------------------------------------------------------------------------------

def ramp_lut(stops):
    """256-entry sRGB lookup built from (position, colour-name-or-rgb) stops.

    A LUT rather than a per-pixel ramp evaluation: it is a million lookups per map either way, and
    this way the arithmetic happens 256 times.
    """
    stops = sorted(((p, C(c) if isinstance(c, str) else c) for p, c in stops), key=lambda s: s[0])
    lut_r, lut_g, lut_b = [], [], []
    for i in range(256):
        t = i / 255.0
        lo = stops[0]
        hi = stops[-1]
        for j in range(len(stops) - 1):
            if stops[j][0] <= t <= stops[j + 1][0]:
                lo, hi = stops[j], stops[j + 1]
                break
        span = hi[0] - lo[0]
        f = 0.0 if span <= 0 else (t - lo[0]) / span
        lut_r.append(lo[1][0] + (hi[1][0] - lo[1][0]) * f)
        lut_g.append(lo[1][1] + (hi[1][1] - lo[1][1]) * f)
        lut_b.append(lo[1][2] + (hi[1][2] - lo[1][2]) * f)
    return lut_r, lut_g, lut_b


def apply_ramp(field, lut):
    lr, lg, lb = lut
    rows_r, rows_g, rows_b = [], [], []
    for row in field:
        idx = [0 if v < 0.0 else (255 if v > 1.0 else int(v * 255.0)) for v in row]
        rows_r.append([lr[i] for i in idx])
        rows_g.append([lg[i] for i in idx])
        rows_b.append([lb[i] for i in idx])
    return rows_r, rows_g, rows_b


def blend_rgb(base, over, mask):
    return (lerp_fields(base[0], over[0], mask),
            lerp_fields(base[1], over[1], mask),
            lerp_fields(base[2], over[2], mask))


def tint_rgb(rgb, mask, colour, strength=1.0):
    """Pull `rgb` toward a flat colour where `mask` is high -- e.g. cold blue settling in crevices."""
    r, g, b = rgb
    cr, cg, cb = C(colour) if isinstance(colour, str) else colour
    out = []
    for chan, cv in ((r, cr), (g, cg), (b, cb)):
        out.append([[v + (cv - v) * (m * strength) for v, m in zip(rv, rm)]
                    for rv, rm in zip(chan, mask)])
    return tuple(out)


# ----------------------------------------------------------------------------------------------
# Normal map derivation
# ----------------------------------------------------------------------------------------------

def height_to_normal(height, strength):
    """Sobel-derived tangent-space normal. No faked flat blue, no cheap central difference.

    Convention: image x runs right, image y runs DOWN, height runs up. The tangent frame is then
    T = (1, 0, dh/dx), B = (0, 1, dh/dy) and N = T x B = (-dh/dx, -dh/dy, 1). That is the
    green-down / DirectX orientation Unreal expects from a TC_Normalmap; flipping the sign of G
    here is the classic way to get lighting that reads inside-out on every surface in the game.
    """
    sqrt = math.sqrt
    rows = []
    for y in range(SIZE):
        hm = height[(y - 1) % SIZE]
        h0 = height[y]
        hp = height[(y + 1) % SIZE]
        hm_l = hm[-1:] + hm[:-1]
        hm_r = hm[1:] + hm[:1]
        h0_l = h0[-1:] + h0[:-1]
        h0_r = h0[1:] + h0[:1]
        hp_l = hp[-1:] + hp[:-1]
        hp_r = hp[1:] + hp[:1]
        gx = [((a + 2.0 * b + c) - (d + 2.0 * e + f)) * 0.125
              for a, b, c, d, e, f in zip(hm_r, h0_r, hp_r, hm_l, h0_l, hp_l)]
        gy = [((a + 2.0 * b + c) - (d + 2.0 * e + f)) * 0.125
              for a, b, c, d, e, f in zip(hp_l, hp, hp_r, hm_l, hm, hm_r)]
        out = bytearray()
        for dx, dy in zip(gx, gy):
            nx = -dx * strength
            ny = -dy * strength
            inv = 1.0 / sqrt(nx * nx + ny * ny + 1.0)
            nx *= inv
            ny *= inv
            nz = inv
            out.append(_b(nx * 0.5 + 0.5))
            out.append(_b(ny * 0.5 + 0.5))
            out.append(_b(nz * 0.5 + 0.5))
        rows.append(bytes(out))
    return rows


def _b(v):
    i = int(v * 255.0 + 0.5)
    return 0 if i < 0 else (255 if i > 255 else i)


# ----------------------------------------------------------------------------------------------
# PNG output -- 8-bit RGB, non-interlaced, filter type 0, written by hand
# ----------------------------------------------------------------------------------------------

def _chunk(tag, payload):
    return (struct.pack(">I", len(payload)) + tag + payload
            + struct.pack(">I", zlib.crc32(tag + payload) & 0xFFFFFFFF))


def write_png(path, rows):
    """rows: SIZE byte-strings of length SIZE*3. Returns (png_sha256, raw_pixel_sha256, bytes)."""
    raw = bytearray()
    for r in rows:
        raw.append(0)  # filter type 0 (None)
        raw += r
    raw = bytes(raw)
    ihdr = struct.pack(">IIBBBBB", SIZE, SIZE, 8, 2, 0, 0, 0)
    blob = (b"\x89PNG\r\n\x1a\n"
            + _chunk(b"IHDR", ihdr)
            + _chunk(b"IDAT", zlib.compress(raw, 6))
            + _chunk(b"IEND", b""))
    with open(path, "wb") as fh:
        fh.write(blob)
    pixels = b"".join(rows)
    return (hashlib.sha256(blob).hexdigest(),
            hashlib.sha256(pixels).hexdigest(),
            len(blob))


def rows_from_rgb(rgb):
    r, g, b = rgb
    rows = []
    for rr, gg, bb in zip(r, g, b):
        out = bytearray()
        for a, c, d in zip(rr, gg, bb):
            out.append(_b8(a))
            out.append(_b8(c))
            out.append(_b8(d))
        rows.append(bytes(out))
    return rows


def rows_from_gray(field):
    rows = []
    for r in field:
        out = bytearray()
        for v in r:
            i = _b(v)
            out.append(i)
            out.append(i)
            out.append(i)
        rows.append(bytes(out))
    return rows


def _b8(v):
    i = int(v + 0.5)
    return 0 if i < 0 else (255 if i > 255 else i)


# ----------------------------------------------------------------------------------------------
# Seam check -- the one failure this generator must never ship silently
# ----------------------------------------------------------------------------------------------

def seam_ratio(rows):
    """How much worse is the wrap-around edge than a typical interior neighbour pair?

    Returns (ratio, absolute_edge_difference). ~1.0 means the tile wraps: the edge is just another
    sample of the surface's own gradient statistics. A hard seam blows the ratio up, because a
    discontinuity is by definition not drawn from that distribution.

    The absolute number is reported alongside because on a very smooth map the interior denominator
    can be a fraction of a byte, and a ratio computed against near-zero is noise, not evidence.
    """
    stride = SIZE * 3
    edge_x = 0
    inner_x = 0
    edge_y = 0
    inner_y = 0
    step = 8  # every 8th row/column; enough samples, a fraction of the cost
    for y in range(0, SIZE, step):
        row = rows[y]
        edge_x += sum(abs(row[i] - row[stride - 3 + i]) for i in range(3))
        for x in range(0, SIZE - 1, step):
            o = x * 3
            inner_x += sum(abs(row[o + i] - row[o + 3 + i]) for i in range(3))
    top = rows[0]
    bot = rows[SIZE - 1]
    for x in range(0, SIZE, step):
        o = x * 3
        edge_y += sum(abs(top[o + i] - bot[o + i]) for i in range(3))
    for y in range(0, SIZE - 1, step):
        a = rows[y]
        b = rows[y + 1]
        for x in range(0, SIZE, step):
            o = x * 3
            inner_y += sum(abs(a[o + i] - b[o + i]) for i in range(3))
    nx = SIZE // step
    n_inner_x = nx * ((SIZE - 1 + step - 1) // step)
    n_inner_y = ((SIZE - 1 + step - 1) // step) * nx
    mx = (inner_x / max(1, n_inner_x)) or 1e-6
    my = (inner_y / max(1, n_inner_y)) or 1e-6
    ex = edge_x / nx
    ey = edge_y / nx
    return max(ex / mx, ey / my), max(ex, ey)


# ----------------------------------------------------------------------------------------------
# Material sets
# ----------------------------------------------------------------------------------------------

def _fracture(src, breakup, base, spread, soft):
    """Turn a noise field into a crack network.

    WHY THIS IS NOT JUST smoothstep(ridged(noise)):
        A fixed threshold on a ridged field draws the field's iso-contour, and the iso-contour of
        smooth value noise is a smooth closed loop of near-constant width. Do that and the rock
        ends up looking like a topographic map or a circuit board -- every "crack" the same weight,
        every one eventually closing on itself. Real fracture does not close and does not hold a
        constant width.

        So the threshold itself wanders, driven by `breakup`. Where the local threshold rises past
        1.0 the crack simply is not there, which is what makes a fracture terminate instead of
        loop; where it dips, the crack opens wider. `src` is an fBm rather than a single octave, so
        the line branches at more than one scale.
    """
    out = []
    for rs, rb in zip(src, breakup):
        row = []
        for v, b in zip(rs, rb):
            r = 1.0 - abs(2.0 * v - 1.0)
            e0 = base + spread * b
            e1 = e0 + soft
            t = (r - e0) / (e1 - e0)
            if t <= 0.0:
                row.append(0.0)
            elif t >= 1.0:
                row.append(1.0)
            else:
                row.append(t * t * (3.0 - 2.0 * t))
        out.append(row)
    return out


def _stone_structure(key, crack_depth):
    """Shared granite substrate: macro form, meso lumps, granite grain, and a fracture network.

    Stone, Stone_Dark and Lichen are the *same rock* in the fiction, so they are the same rock in
    the generator. Only palette and crevice depth differ.
    """
    macro = fbm(6, 6, 4, (key, "macro"), gain=0.55)
    meso = fbm(22, 22, 4, (key, "meso"), gain=0.5)
    micro = fbm(110, 110, 3, (key, "micro"), gain=0.55)

    # Two fracture scales: a few structural cracks that carry across the surface, and a finer set
    # that only shows up close. Both terminate rather than loop, and neither covers the whole tile.
    coarse = _fracture(fbm(11, 11, 4, (key, "cr_a"), gain=0.62),
                       fbm(7, 7, 3, (key, "brk_a"), gain=0.5),
                       base=0.845, spread=0.185, soft=0.055)
    fine = _fracture(fbm(29, 29, 3, (key, "cr_b"), gain=0.58),
                     fbm(13, 13, 3, (key, "brk_b"), gain=0.5),
                     base=0.900, spread=0.160, soft=0.040)
    cracks = clamp01(add_fields(coarse, fine, 0.55))

    height = [[0.50 * a + 0.30 * b + 0.20 * c for a, b, c in zip(ra, rb, rc)]
              for ra, rb, rc in zip(macro, meso, micro)]
    height = [[h - crack_depth * k for h, k in zip(rh, rk)] for rh, rk in zip(height, cracks)]
    return normalize_field(height), cracks, meso, micro


def _cavity(height, radius=11):
    """Occlusion proxy: how far below its own neighbourhood a point sits."""
    blurred = box_blur(height, radius)
    return clamp01([[(b - h) * 3.0 for b, h in zip(rb, rh)] for rb, rh in zip(blurred, height)])


def set_stone():
    """Cold wet granite. Ashen gray, fine cracks, subtle value variation."""
    height, cracks, meso, micro = _stone_structure("stone", 0.24)
    cav = _cavity(height)

    lut = ramp_lut([(0.00, "wet_stone_dark"), (0.35, "ashen_gray_dark"),
                    (0.70, "ashen_gray"), (1.00, "ashen_gray_light")])
    # Value variation is driven by the grain and lump fields, not by the height, so patches of
    # lighter and darker rock do not have to line up with the bumps -- that alignment is what makes
    # procedural stone look procedural. Granite is a speckle of differently-valued mineral, so the
    # grain gets the larger share.
    value = clamp01([[0.46 * g + 0.38 * m + 0.16 * h
                      for g, m, h in zip(rg, rm, rh)]
                     for rg, rm, rh in zip(micro, meso, height)])
    rgb = apply_ramp(value, lut)
    rgb = tint_rgb(rgb, cav, "cold_blue_deep", 0.65)          # damp cold settles in the low places
    rgb = tint_rgb(rgb, cracks, "blue_black", 0.55)

    # Wet: water pools in the crevices, so the crevices are the *smoother* places, not the rougher.
    rough = clamp01([[0.74 - 0.26 * c - 0.10 * v for c, v in zip(rc, rv)]
                     for rc, rv in zip(cav, cracks)])
    ao = clamp01([[1.0 - 0.55 * c for c in rc] for rc in cav])
    return {"BC": rgb, "R": rough, "AO": ao, "H": height, "_n": 2.2}


def set_stone_dark():
    """The same rock, blue-black, deeper crevices. The shaded half of the sanctuary."""
    height, cracks, meso, micro = _stone_structure("stonedark", 0.44)
    cav = _cavity(height)

    lut = ramp_lut([(0.00, "blue_black"), (0.40, "blue_black_light"),
                    (0.75, "cold_blue_deep"), (1.00, "wet_stone_dark")])
    value = clamp01([[0.44 * g + 0.38 * m + 0.18 * h
                      for g, m, h in zip(rg, rm, rh)]
                     for rg, rm, rh in zip(micro, meso, height)])
    rgb = apply_ramp(value, lut)
    rgb = tint_rgb(rgb, cav, (0x08, 0x0A, 0x0E), 0.80)
    rgb = tint_rgb(rgb, cracks, (0x05, 0x06, 0x09), 0.70)

    rough = clamp01([[0.66 - 0.28 * c - 0.12 * v for c, v in zip(rc, rv)]
                     for rc, rv in zip(cav, cracks)])
    ao = clamp01([[1.0 - 0.72 * c for c in rc] for rc in cav])
    return {"BC": rgb, "R": rough, "AO": ao, "H": height, "_n": 2.8}


def set_paving():
    """Cut flagstones with mortar joints and worn centres. Bone beige through gray.

    COLS/ROWS are integers and ROWS is even so the running-bond offset survives a tile repeat; the
    coordinate warp is itself tileable noise. Together that is why the joints line up across the
    seam instead of forming a visible grid discontinuity every 1024 units.
    """
    COLS, ROWS = 5, 6
    warp_x = value_noise(16, 16, ("pave", "wx"))
    warp_y = value_noise(16, 16, ("pave", "wy"))
    grain = fbm(64, 64, 3, ("pave", "grain"), gain=0.5)
    row_offset = [_rand("pave", "roff", j) for j in range(ROWS)]
    cell_value = [[_rand("pave", "cell", j, i) for i in range(COLS)] for j in range(ROWS)]
    cell_drop = [[_rand("pave", "drop", j, i) for i in range(COLS)] for j in range(ROWS)]

    joint = [[0.0] * SIZE for _ in range(SIZE)]
    stone_v = [[0.0] * SIZE for _ in range(SIZE)]
    dish = [[0.0] * SIZE for _ in range(SIZE)]

    WARP = 26.0          # px of hand-cut wobble on every joint
    JOINT_PX = 7.0       # mortar half-width
    SOFT = 4.0
    cw = SIZE / float(COLS)
    ch = SIZE / float(ROWS)
    # Push the grid off the tile origin by a fraction of a cell. Without this the tile edge lands
    # exactly on a mortar joint in both axes: the wrap is still mathematically seamless, but every
    # repeat of the tile puts a joint on the same line, which is precisely how a repeating texture
    # announces itself. Off-phase, the wrap cuts through the middle of a flagstone, where the
    # surface is continuous and there is nothing for the eye to lock onto.
    U_PHASE = 0.23
    V_PHASE = 0.37
    for y in range(SIZE):
        wyr = warp_y[y]
        wxr = warp_x[y]
        jr = joint[y]
        sr = stone_v[y]
        dr = dish[y]
        for x in range(SIZE):
            fy = y + (wyr[x] - 0.5) * WARP
            v = fy / ch + V_PHASE
            ry = int(math.floor(v)) % ROWS
            fv = v - math.floor(v)
            fx = x + (wxr[x] - 0.5) * WARP
            u = fx / cw + row_offset[ry] + U_PHASE
            rx = int(math.floor(u)) % COLS
            fu = u - math.floor(u)
            # Distance to the nearest joint, in pixels, so the mortar is a constant real width.
            du = min(fu, 1.0 - fu) * cw
            dv = min(fv, 1.0 - fv) * ch
            d = du if du < dv else dv
            if d >= JOINT_PX + SOFT:
                jr[x] = 0.0
            elif d <= JOINT_PX:
                jr[x] = 1.0
            else:
                t = (JOINT_PX + SOFT - d) / SOFT
                jr[x] = t * t * (3.0 - 2.0 * t)
            sr[x] = cell_value[ry][rx]
            # Worn centre: foot traffic dishes the middle of a flagstone, not its edges.
            cu = 1.0 - abs(fu - 0.5) * 2.0
            cv = 1.0 - abs(fv - 0.5) * 2.0
            dr[x] = cu * cv * (0.55 + 0.45 * cell_drop[ry][rx])

    joint_s = joint
    # The per-stone height offset is faded out inside the mortar by (1 - joint). Each flagstone sits
    # at its own level, so the offset is a step function across a joint; leaving that step in the
    # height field puts a hard crease straight down the middle of every mortar line in the derived
    # normal map. Fading it to zero where the mortar is means neighbouring stones meet at a common
    # level in the joint itself, which is also what mortar physically does.
    height = [[0.62 + 0.16 * g - 0.30 * j - 0.10 * d + 0.10 * s * (1.0 - j)
               for g, j, d, s in zip(rg, rj, rd, rs)]
              for rg, rj, rd, rs in zip(grain, joint_s, dish, stone_v)]
    height = normalize_field(height)
    cav = _cavity(height, 9)

    stone_lut = ramp_lut([(0.00, "old_bone_gray"), (0.45, "old_bone_dim"),
                          (0.80, "old_bone_beige"), (1.00, (0xC2, 0xBA, 0xA6))])
    value = clamp01([[0.62 * s + 0.38 * g for s, g in zip(rs, rg)]
                     for rs, rg in zip(stone_v, grain)])
    rgb = apply_ramp(value, stone_lut)
    # Worn centres are polished paler; the mortar is a cold umber-gray that never reads as sand.
    rgb = tint_rgb(rgb, dish, (0xC6, 0xBF, 0xAE), 0.22)
    mortar = apply_ramp(grain, ramp_lut([(0.0, (0x30, 0x2D, 0x29)), (1.0, (0x4A, 0x46, 0x40))]))
    rgb = blend_rgb(rgb, mortar, joint_s)
    rgb = tint_rgb(rgb, cav, "cold_blue_deep", 0.45)

    rough = clamp01([[0.62 + 0.30 * j - 0.22 * d for j, d in zip(rj, rd)]
                     for rj, rd in zip(joint_s, dish)])
    ao = clamp01([[1.0 - 0.60 * c - 0.28 * j for c, j in zip(rc, rj)]
                  for rc, rj in zip(cav, joint_s)])
    return {"BC": rgb, "R": rough, "AO": ao, "H": height, "_n": 3.0}


def set_soil():
    """Dark damp earth. Deep umber, organic clumping, small stones that are actually objects."""
    clump = fbm(6, 6, 6, ("soil", "clump"), gain=0.58)
    # Coarser and gentler than it was: soil is the quietest surface in the sanctuary and the art
    # direction asks for background decay to stay quieter than meaningful structure. A per-pixel
    # grit hiss competes with the flagstones and the ironwork for the eye, and loses nothing by
    # being calmed down.
    grit = fbm(56, 56, 3, ("soil", "grit"), gain=0.55)
    damp = fbm(10, 10, 3, ("soil", "damp"), gain=0.55)

    height = [[0.72 * a + 0.28 * b for a, b in zip(ra, rb)] for ra, rb in zip(clump, grit)]
    stones = [[0.0] * SIZE for _ in range(SIZE)]
    scatter(stones, 520, ("soil", "pebble"), 4.0, 11.0, 1.0)
    stones = clamp01(stones)
    height = add_fields(height, stones, 0.34)
    height = normalize_field(height)
    cav = _cavity(height, 13)

    earth = ramp_lut([(0.00, (0x1B, 0x14, 0x0F)), (0.35, "deep_umber"),
                      (0.75, "deep_umber_light"), (1.00, (0x5C, 0x4B, 0x38))])
    rgb = apply_ramp(clamp01([[0.55 * c + 0.45 * g for c, g in zip(rc, rg)]
                              for rc, rg in zip(clump, grit)]), earth)
    # Damp patches read darker and colder, and they are large and soft -- quiet background decay.
    wet = smoothstep_field(0.42, 0.72, damp)
    rgb = tint_rgb(rgb, wet, (0x17, 0x13, 0x10), 0.45)
    # The pebbles are a different material embedded in the earth, but they are *embedded*: only the
    # crown of a stone is clean, the rest is under dirt. A high threshold keeps the visible stone to
    # the top of each dome instead of painting the whole disc, and the colour is pulled a long way
    # toward the umber so they read as wet stone in soil rather than gravel dropped on a photo.
    stone_mask = scale_field(smoothstep_field(0.45, 0.88, stones), 0.72)
    pebble = apply_ramp(grit, ramp_lut([(0.0, (0x33, 0x2E, 0x28)), (1.0, (0x55, 0x52, 0x4C))]))
    rgb = blend_rgb(rgb, pebble, stone_mask)
    rgb = tint_rgb(rgb, cav, (0x10, 0x0C, 0x09), 0.55)

    rough = clamp01([[0.90 - 0.26 * s - 0.16 * w for s, w in zip(rs, rw)]
                     for rs, rw in zip(stone_mask, wet)])
    ao = clamp01([[1.0 - 0.70 * c for c in rc] for rc in cav])
    return {"BC": rgb, "R": rough, "AO": ao, "H": height, "_n": 2.4}


def set_iron():
    """Old oxidised iron. Rusted brown blooming over dark metal, with real pitting."""
    bloom = fbm(7, 7, 5, ("iron", "bloom"), gain=0.55)
    crust = fbm(40, 40, 3, ("iron", "crust"), gain=0.5)
    streak = value_noise(4, 96, ("iron", "streak"))   # rust runs downward with the water

    rust = clamp01([[0.62 * b + 0.24 * c + 0.14 * s for b, c, s in zip(rb, rc, rs)]
                    for rb, rc, rs in zip(bloom, crust, streak)])
    rust = smoothstep_field(0.40, 0.68, rust)

    height = [[0.5 + 0.10 * c for c in rc] for rc in crust]
    height = add_fields(height, rust, 0.16)           # rust crust stands proud of clean metal
    pits = [[0.0] * SIZE for _ in range(SIZE)]
    scatter(pits, 1400, ("iron", "pit"), 2.0, 6.5, -1.0)
    height = add_fields(height, pits, 0.30)
    height = normalize_field(height)
    cav = _cavity(height, 7)

    metal = apply_ramp(crust, ramp_lut([(0.0, "iron_metal"), (1.0, "iron_metal_light")]))
    oxide = apply_ramp(clamp01([[0.6 * c + 0.4 * s for c, s in zip(rc, rs)]
                                for rc, rs in zip(crust, streak)]),
                       ramp_lut([(0.00, (0x4A, 0x2A, 0x1B)), (0.50, "rusted_iron_brown"),
                                 (1.00, "rusted_iron_light")]))
    rgb = blend_rgb(metal, oxide, rust)
    rgb = tint_rgb(rgb, cav, (0x0B, 0x0C, 0x0E), 0.65)

    # Clean metal is smooth; oxide is chalk. That contrast is the whole read of the material.
    rough = clamp01([[0.34 + 0.56 * r - 0.10 * c for r, c in zip(rr, rc)]
                     for rr, rc in zip(rust, cav)])
    ao = clamp01([[1.0 - 0.62 * c for c in rc] for rc in cav])
    return {"BC": rgb, "R": rough, "AO": ao, "H": height, "_n": 2.6}


def set_lichen():
    """Dead moss green over the sanctuary's own stone. Patchy growth, low frequency, quiet."""
    height, cracks, meso, micro = _stone_structure("lichen", 0.30)

    # Growth follows the crevices -- lichen colonises where damp collects -- and is otherwise
    # governed by a large, soft patch field. Explicitly not a high-frequency speckle: a colony is a
    # thing with an outline, and the art direction wants an outline the player can read at distance.
    patch = fbm(4, 4, 4, ("lichen", "patch"), gain=0.60)
    colonise = clamp01([[0.82 * p + 0.18 * c for p, c in zip(rp, rc)]
                        for rp, rc in zip(patch, cracks)])
    growth = smoothstep_field(0.44, 0.62, colonise)
    # The colony's internal texture is coarse enough to read as crust, not as noise, and it only
    # modulates the colony -- it never introduces growth where the patch field said there is none.
    texture = fbm(48, 48, 3, ("lichen", "tex"), gain=0.55)
    growth = clamp01([[g * (0.78 + 0.22 * t) for g, t in zip(rg, rt)]
                      for rg, rt in zip(growth, texture)])

    height = add_fields(height, mul_fields(growth, texture), 0.10)
    height = normalize_field(height)
    cav = _cavity(height, 11)

    stone_rgb = apply_ramp(clamp01([[0.44 * g + 0.38 * m + 0.18 * h
                                     for g, m, h in zip(rg, rm, rh)]
                                    for rg, rm, rh in zip(micro, meso, height)]),
                           ramp_lut([(0.00, "wet_stone_dark"), (0.50, "ashen_gray_dark"),
                                     (1.00, "ashen_gray")]))
    moss = apply_ramp(texture, ramp_lut([(0.00, (0x33, 0x3A, 0x2C)), (0.50, "dead_moss_green"),
                                         (1.00, "dead_moss_light")]))
    rgb = blend_rgb(stone_rgb, moss, growth)
    rgb = tint_rgb(rgb, cav, "cold_blue_deep", 0.50)

    # Dry dead moss is the roughest thing in the sanctuary; the wet stone under it is not.
    rough = clamp01([[0.68 + 0.28 * g - 0.22 * c for g, c in zip(rg, rc)]
                     for rg, rc in zip(growth, cav)])
    ao = clamp01([[1.0 - 0.58 * c - 0.14 * g for c, g in zip(rc, rg)]
                  for rc, rg in zip(cav, growth)])
    return {"BC": rgb, "R": rough, "AO": ao, "H": height, "_n": 2.0}


def set_weathered():
    """Pale weathered timber / lime plaster. Old bone beige, grain, and flaking.

    The grain is anisotropic by construction: the lattice is 4 cells across and 256 down, so the
    noise is stretched ~64:1 along one axis. That is what makes it read as timber rather than as
    stone that happens to be beige.
    """
    grain = fbm(4, 256, 4, ("weath", "grain"), gain=0.55)
    fine_grain = value_noise(8, 512, ("weath", "fine"))
    wander = fbm(12, 12, 3, ("weath", "wander"), gain=0.5)   # boards are never perfectly straight
    plaster = fbm(24, 24, 4, ("weath", "plaster"), gain=0.5)

    base = [[0.55 * g + 0.20 * f + 0.25 * p for g, f, p in zip(rg, rf, rp)]
            for rg, rf, rp in zip(grain, fine_grain, plaster)]

    # Flaking: mid-frequency patches with a hard-ish edge, which is what a chip looks like.
    flake = smoothstep_field(0.58, 0.63, fbm(18, 18, 3, ("weath", "flake"), gain=0.5))
    flake = mul_fields(flake, smoothstep_field(0.35, 0.60, wander))

    height = [[b - 0.30 * fl for b, fl in zip(rb, rfl)] for rb, rfl in zip(base, flake)]
    height = normalize_field(height)
    cav = _cavity(height, 9)

    surface = ramp_lut([(0.00, "old_bone_gray"), (0.40, "old_bone_dim"),
                        (0.75, "old_bone_beige"), (1.00, (0xC5, 0xBE, 0xAB))])
    rgb = apply_ramp(clamp01([[0.60 * g + 0.40 * p for g, p in zip(rg, rp)]
                              for rg, rp in zip(grain, plaster)]), surface)
    # Weathering greys the exposed faces; what the flake reveals underneath is darker and browner.
    grey = smoothstep_field(0.30, 0.80, wander)
    rgb = tint_rgb(rgb, grey, "old_bone_gray", 0.35)
    rgb = tint_rgb(rgb, flake, (0x54, 0x4B, 0x3D), 0.80)
    rgb = tint_rgb(rgb, cav, (0x3A, 0x35, 0x2D), 0.40)

    rough = clamp01([[0.80 + 0.16 * fl - 0.10 * c for fl, c in zip(rfl, rc)]
                     for rfl, rc in zip(flake, cav)])
    ao = clamp01([[1.0 - 0.50 * c - 0.20 * fl for c, fl in zip(rc, rfl)]
                  for rc, rfl in zip(cav, flake)])
    return {"BC": rgb, "R": rough, "AO": ao, "H": height, "_n": 2.0}


SETS = [
    ("Stone", set_stone),
    ("Stone_Dark", set_stone_dark),
    ("Paving", set_paving),
    ("Soil", set_soil),
    ("Iron", set_iron),
    ("Lichen", set_lichen),
    ("Weathered", set_weathered),
]

# A tile fails the seam check only when the wrap edge is BOTH statistically anomalous and visible.
# Ratio alone false-alarms on maps so smooth that the interior denominator is a fraction of a byte;
# absolute difference alone false-alarms on maps that are legitimately high-contrast everywhere.
SEAM_LIMIT = 4.0
SEAM_ABS_LIMIT = 6.0  # summed over R+G+B, out of 765


def main():
    if len(sys.argv) < 2:
        raise SystemExit("usage: forge_sanctuary_textures.py <output-directory>")
    out_dir = os.path.abspath(sys.argv[1])
    os.makedirs(out_dir, exist_ok=True)

    print("FORGETEX: seed=0x%X size=%dx%d out=%s" % (SEED, SIZE, SIZE, out_dir))
    written = []
    seam_failures = []

    for set_name, builder in SETS:
        print("FORGETEX: building %s" % set_name)
        data = builder()
        strength = data["_n"]
        channels = [
            ("BC", rows_from_rgb(data["BC"])),
            ("N", height_to_normal(data["H"], strength)),
            ("R", rows_from_gray(data["R"])),
            ("AO", rows_from_gray(data["AO"])),
            ("H", rows_from_gray(data["H"])),
        ]
        for suffix, rows in channels:
            name = "T_Sanctuary_%s_%s.png" % (set_name, suffix)
            path = os.path.join(out_dir, name)
            png_sha, raw_sha, nbytes = write_png(path, rows)
            ratio, edge_abs = seam_ratio(rows)
            bad = ratio > SEAM_LIMIT and edge_abs > SEAM_ABS_LIMIT
            if bad:
                seam_failures.append("%s seam ratio %.2f edge %.2f" % (name, ratio, edge_abs))
            print("FORGETEX: %-34s %8d B  seam=%.2fx/%.2f  png=%s%s"
                  % (name, nbytes, ratio, edge_abs, png_sha, "  <<< SEAM" if bad else ""))
            print("FORGETEX:   %-42s raw=%s" % (name, raw_sha))
            written.append(name)
        del data

    if seam_failures:
        raise RuntimeError("FORGETEX FAILED: tile does not wrap: " + "; ".join(seam_failures))
    print("FORGETEX: complete, %d PNG(s) in %s" % (len(written), out_dir))


main()
