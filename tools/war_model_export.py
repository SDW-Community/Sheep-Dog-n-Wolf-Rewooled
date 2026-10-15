#!/usr/bin/env python3
"""Export a level's model (a mesh resource of its .WAR) to glTF 2.0 binary (.glb): mesh, skeleton, animations and the
.DAV texture pages it draws with. Blender (File > Import > glTF 2.0) opens it and can save it as FBX.

What it reads, as the game reads it (src/engine/bs_file.cpp, anim_mesh.cpp, mesh_part.cpp):
- The geometry record (tools/war_meshes.py): vertices are three s16 + pad, each in the space of the part that owns it;
  the parts own consecutive vertex ranges in part order (AnimMesh::DrawAll).
- A model (type 4) has a part table at +0x14 (u16 count after it): {u16 parent, s16 offset[3], u16 vertex count}; a
  parent always has a lower index. A part's matrix is rot * translate(pos + offset * parentScale) * parent (row
  vectors, MeshPart::BuildMatrices), with rot = SetRotYXZ(-rx, ry, -rz); its scale applies to its own vertices only.
- Animations at +0x10: u32 count, u32 offset per slot (0 = empty); a sequence is an 8-char name, u16 frame count, then
  per frame {u16 half duration ms, u16 joint entries, u16 length in words, u16} and the joint entries
  (BsFile::ReadJointTable). A frame starts as a copy of the previous one; a listed joint resets its absent channels.
- Polygons: per corner a vertex byte; textured kinds a (u, v) texel pair per corner and a texture id -> .DAV rectangle
  -> page (Bs_TexelToUV). A quad (corners 0 1 2 3) is drawn as triangles 0 3 2 and 0 1 3, as the decoders split it.

The game's space has y pointing down; the export turns it to glTF's y up (a half turn about x on the root node).
Corner colours: the textures are mostly grey and take their colour from the corners, PlayStation style (0x80 = full
brightness, as BsFile::ReadRgbHalved halves the untextured ones to match); written as COLOR_0, linear.

Usage:
  python3 tools/war_model_export.py Lvl-01 ralph ralph.glb      # Ralph: the model the level's Wolf record names
  python3 tools/war_model_export.py Lvl-01 2 sam.glb             # any model, by resource number (see --list)
  python3 tools/war_model_export.py Lvl-01 ralph ralph.glb --retexture   # use <game>/retexture/<Level>/page_NN.png
  python3 tools/war_model_export.py Lvl-01 ralph ralph.glb --fbx # also ralph.fbx, through Blender (--blender PATH)
  python3 tools/war_model_export.py Lvl-01 ralph ralph.glb --png # also the texture pages (ralph_page_NN.png) and
                                                                 # each image cut out (ralph_textures/)
  python3 tools/war_model_export.py Lvl-01 --list                # the level's models, with their animation counts
"""
import io
import shutil
import subprocess
import json
import math
import struct
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import retexture  # noqa: E402  page decoding (Pillow), PlayStation .DAV conversion
import war_meshes  # noqa: E402  the .WAR / .DAV readers

F3, F4, G3, G4, FT3, FT4, GT3, GT4 = range(8)
BF3, BF4, BFT3, BGT3 = 0x10, 0x11, 0x14, 0x15
MODEL = 4


# ---- reading ----

def read_parts(d, off):
    at, n = struct.unpack_from("<IH", d, off + 0x14)
    return [dict(zip(("parent", "ox", "oy", "oz", "verts"), struct.unpack_from("<H3hH", d, at + 10 * i)))
            for i in range(n)]


def angle12(a):
    return a * 2.0 * math.pi / 4096.0


def angle8(a):
    return a * 2.0 * math.pi / 128.0


def read_anims(d, off, n_parts):
    """[(name, [(duration ms, [pose per part])])]; a pose is (rot[3], pos[3], scale[3])."""
    at, = struct.unpack_from("<I", d, off + 0x10)
    n, = struct.unpack_from("<I", d, at)
    out = []
    for slot in range(n):
        o, = struct.unpack_from("<I", d, at + 4 + 4 * slot)
        if not o:
            continue
        name = d[o:o + 8].split(b"\0")[0].decode("latin1")
        keys, = struct.unpack_from("<H", d, o + 8)
        c = o + 10
        poses = [([0.0] * 3, [0.0] * 3, [1.0] * 3) for _ in range(n_parts)]
        frames = []
        for _ in range(keys):
            half, joints, words, _pad = struct.unpack_from("<4H", d, c)
            c += 8
            j = c
            poses = [(list(r), list(p), list(s)) for r, p, s in poses]
            for _ in range(joints):
                mask, = struct.unpack_from("<H", d, j)
                j += 2
                rot, pos, scl = [0.0] * 3, [0.0] * 3, [1.0] * 3
                packed = mask & 0x8000
                nb = 0
                for bit, chan, k in ((0x40, rot, 0), (0x80, rot, 1), (0x100, rot, 2), (0x200, pos, 0), (0x400, pos, 1),
                                     (0x800, pos, 2), (0x1000, scl, 0), (0x2000, scl, 1), (0x4000, scl, 2)):
                    if not mask & bit:
                        continue
                    if packed:
                        b, = struct.unpack_from("<b" if chan is pos else "<B", d, j)
                        if chan is rot:
                            b = struct.unpack("<b", bytes((b,)))[0]
                        j += 1
                        nb += 1
                        chan[k] = angle8(b) if chan is rot else float(b) if chan is pos else b / 128.0
                    else:
                        v, = struct.unpack_from("<h" if chan is not scl else "<H", d, j)
                        j += 2
                        chan[k] = angle12(v) if chan is rot else float(v) if chan is pos else v / 1024.0
                if packed and nb % 2:
                    j += 1
                poses[mask & 0x3f] = (rot, pos, scl)
            frames.append((half * 2.0, poses))
            c += words * 2
        out.append((name, frames))
    return out


def read_polys(d, off, dav):
    """[(vertex idx[3], uv[3] or None, colour[3] as (r, g, b) bytes, page or None)] in the game's order."""
    vtx_at, ent_at, n_vtx, n_ent = struct.unpack_from("<IIHH", d, off)
    rects, entries = dav["rects"], dav["entries"]
    out, skipped = [], {}
    o = ent_at

    def col(p):
        return d[p], d[p + 1], d[p + 2]

    def uv(texel_u, texel_v, rect):
        x, w, y, h, _page = rect
        return ((texel_u * ((w - 1) / w) + 0.5 + x) / 256.0, (texel_v * ((h - 1) / h) + 0.5 + y) / 256.0)

    for _ in range(n_ent):
        kind, count = struct.unpack_from("<HH", d, o)
        o += 4
        stride = war_meshes.STRIDE[kind]
        for i in range(count):
            p = o + i * stride
            v = d[p:p + 4]
            if kind in (F3, BF3, G3):
                cs = [col(p + 4)] * 3 if kind != G3 else [col(p + 4), col(p + 8), col(p + 12)]
                out.append(((v[0], v[1], v[2]), None, cs, None))
            elif kind in (F4, BF4, G4):
                cs = [col(p + 4)] * 4 if kind != G4 else [col(p + 4), col(p + 8), col(p + 12), col(p + 16)]
                out.append(((v[0], v[3], v[2]), None, (cs[0], cs[3], cs[2]), None))
                out.append(((v[0], v[1], v[3]), None, (cs[0], cs[1], cs[3]), None))
            elif kind in (FT3, BFT3, GT3, BGT3):
                t = d[p + 4:p + 10]
                tid, = struct.unpack_from("<I", d, p + 12)
                rect = rects[entries[tid]]
                cs = [col(p + 16)] * 3 if kind in (FT3, BFT3) else [col(p + 16), col(p + 20), col(p + 24)]
                out.append(((v[0], v[1], v[2]), [uv(t[0], t[1], rect), uv(t[2], t[3], rect), uv(t[4], t[5], rect)],
                            cs, rect[4]))
            elif kind in (FT4, GT4):
                t = d[p + 4:p + 12]
                tid, = struct.unpack_from("<I", d, p + 12)
                rect = rects[entries[tid]]
                cs = [col(p + 16)] * 4 if kind == FT4 else [col(p + 16), col(p + 20), col(p + 24), col(p + 28)]
                uvs = [uv(t[0], t[1], rect), uv(t[2], t[3], rect), uv(t[4], t[5], rect), uv(t[6], t[7], rect)]
                out.append(((v[0], v[3], v[2]), [uvs[0], uvs[3], uvs[2]], (cs[0], cs[3], cs[2]), rect[4]))
                out.append(((v[0], v[1], v[3]), [uvs[0], uvs[1], uvs[3]], (cs[0], cs[1], cs[3]), rect[4]))
            else:
                skipped[war_meshes.KIND[kind]] = skipped.get(war_meshes.KIND[kind], 0) + count
                break
        o += count * stride
    return out, skipped


def read_vertices(d, off):
    vtx_at, _e, n, _n = struct.unpack_from("<IIHH", d, off)
    return [struct.unpack_from("<3h", d, vtx_at + 8 * i) for i in range(n)]


# ---- maths (row vectors, as the game) ----

def rot_yxz(x, y, z):
    """Mat44::SetRotYXZ's 3x3."""
    cx, sx, cy, sy, cz, sz = math.cos(x), math.sin(x), math.cos(y), math.sin(y), math.cos(z), math.sin(z)
    return [[cy * cz - sx * sy * sz, cy * sz + sx * sy * cz, -cx * sy],
            [-cx * sz, cx * cz, sx],
            [sy * cz + sx * cy * sz, sy * sz - sx * cy * cz, cx * cy]]


def quat_of_row_matrix(m):
    """glTF quaternion (x, y, z, w) of the column-vector rotation m^T."""
    r = [[m[j][i] for j in range(3)] for i in range(3)]
    tr = r[0][0] + r[1][1] + r[2][2]
    if tr > 0:
        s = math.sqrt(tr + 1.0) * 2
        q = ((r[2][1] - r[1][2]) / s, (r[0][2] - r[2][0]) / s, (r[1][0] - r[0][1]) / s, 0.25 * s)
    elif r[0][0] > r[1][1] and r[0][0] > r[2][2]:
        s = math.sqrt(1.0 + r[0][0] - r[1][1] - r[2][2]) * 2
        q = (0.25 * s, (r[0][1] + r[1][0]) / s, (r[0][2] + r[2][0]) / s, (r[2][1] - r[1][2]) / s)
    elif r[1][1] > r[2][2]:
        s = math.sqrt(1.0 + r[1][1] - r[0][0] - r[2][2]) * 2
        q = ((r[0][1] + r[1][0]) / s, 0.25 * s, (r[1][2] + r[2][1]) / s, (r[0][2] - r[2][0]) / s)
    else:
        s = math.sqrt(1.0 + r[2][2] - r[0][0] - r[1][1]) * 2
        q = ((r[0][2] + r[2][0]) / s, (r[1][2] + r[2][1]) / s, 0.25 * s, (r[1][0] - r[0][1]) / s)
    n = math.sqrt(sum(c * c for c in q))
    return [c / n for c in q]


def part_quat(rot):
    return quat_of_row_matrix(rot_yxz(-rot[0], rot[1], -rot[2]))


# ---- glTF writing ----

def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


class Glb:
    def __init__(self):
        self.bin = bytearray()
        self.g = {"asset": {"version": "2.0", "generator": "sdw tools/war_model_export.py"}, "buffers": [],
                  "bufferViews": [], "accessors": [], "nodes": [], "meshes": [], "materials": [], "scenes": [],
                  "scene": 0}

    def view(self, data, target=None):
        while len(self.bin) % 4:
            self.bin.append(0)
        v = {"buffer": 0, "byteOffset": len(self.bin), "byteLength": len(data)}
        if target:
            v["target"] = target
        self.bin += data
        self.g["bufferViews"].append(v)
        return len(self.g["bufferViews"]) - 1

    def accessor(self, fmt, ctype, atype, rows, target=None, minmax=False, normalized=False):
        flat = [c for r in rows for c in (r if isinstance(r, (list, tuple)) else (r,))]
        a = {"bufferView": self.view(struct.pack("<%d%s" % (len(flat), fmt), *flat), target), "componentType": ctype,
             "count": len(rows), "type": atype}
        if normalized:
            a["normalized"] = True
        if minmax:
            width = len(rows[0]) if isinstance(rows[0], (list, tuple)) else 1
            a["min"] = [min(r[k] if width > 1 else r for r in rows) for k in range(width)]
            a["max"] = [max(r[k] if width > 1 else r for r in rows) for k in range(width)]
        self.g["accessors"].append(a)
        return len(self.g["accessors"]) - 1

    def write(self, path):
        while len(self.bin) % 4:
            self.bin.append(0)
        self.g["buffers"] = [{"byteLength": len(self.bin)}]
        for k in [k for k, v in self.g.items() if v == []]:
            del self.g[k]
        js = json.dumps(self.g, separators=(",", ":")).encode()
        js += b" " * (-len(js) % 4)
        out = struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(self.bin))
        out += struct.pack("<II", len(js), 0x4E4F534A) + js + struct.pack("<II", len(self.bin), 0x004E4942) + self.bin
        Path(path).write_bytes(out)


def export(level, res, out_path, use_retexture=False, write_png=False, game=retexture.DEFAULT_GAME):
    folder = game / "Levels" / level
    files = {f.suffix.lower(): f for f in folder.iterdir()}
    d = files[".war"].read_bytes()
    count, = struct.unpack_from("<I", d, 0x0C)
    table = struct.unpack_from("<%dI" % count, d, 0x10)
    rtype, off = (table[res] >> 24) & 0xBF, table[res] & 0xFFFFFF
    if rtype not in war_meshes.MESH_TYPES:
        sys.exit("%s: resource %d is not a mesh (type 0x%x)" % (level, res, rtype))
    with tempfile.TemporaryDirectory() as tmp:
        dav_path, _psx = retexture.page_file(level, files[".dav"], Path(tmp), None)
        dav, pages = retexture.decode_pages(dav_path)

    verts = read_vertices(d, off)
    polys, skipped = read_polys(d, off, dav)
    if rtype == MODEL:
        parts = read_parts(d, off)
        anims = read_anims(d, off, len(parts))
    else:
        parts = [{"parent": 0xFFFF, "ox": 0, "oy": 0, "oz": 0, "verts": len(verts)}]
        anims = []

    # the part owning each vertex, and each part's rest position in model space
    owner = []
    for k, p in enumerate(parts):
        owner += [k] * p["verts"]
    owner += [len(parts) - 1] * (len(verts) - len(owner))
    rest = []
    for k, p in enumerate(parts):
        base = rest[p["parent"]] if k and p["parent"] < k else (0.0, 0.0, 0.0)
        rest.append((base[0] + p["ox"], base[1] + p["oy"], base[2] + p["oz"]))
    scaled = any(s != 1.0 for _n, fr in anims for _t, poses in fr for _r, _p, sc in poses for s in sc)

    g = Glb()
    gl = g.g

    # skeleton: a node per part (rotation, translation); when an animation scales, a child node per part carries the
    # scale (the game's scale does not reach the children) and is the skin joint
    root = {"name": "%s_res%d" % (level, res), "rotation": [1.0, 0.0, 0.0, 0.0], "children": []}
    gl["nodes"].append(root)
    part_node, joint_node = [], []
    for k, p in enumerate(parts):
        n = {"name": "part%02d" % k, "translation": [float(p["ox"]), float(p["oy"]), float(p["oz"])]}
        gl["nodes"].append(n)
        part_node.append(len(gl["nodes"]) - 1)
        if k and p["parent"] < k:
            gl["nodes"][part_node[p["parent"]]].setdefault("children", []).append(part_node[k])
        else:
            root["children"].append(part_node[k])
        if scaled:
            gl["nodes"].append({"name": "part%02d_scale" % k})
            joint_node.append(len(gl["nodes"]) - 1)
            n.setdefault("children", []).append(joint_node[k])
        else:
            joint_node.append(part_node[k])

    # images and materials: one per texture page used, one for the untextured polygons
    used_pages = sorted({pg for _v, _uv, _c, pg in polys if pg is not None})
    retex = game / "retexture" / level
    mat_of_page = {}
    for pg in used_pages:
        override = retex / ("page_%02d.png" % pg)
        if use_retexture and override.is_file():
            png = override.read_bytes()
        else:
            buf = io.BytesIO()
            pages[pg].save(buf, "PNG")
            png = buf.getvalue()
        if write_png:
            Path(out_path).with_name("%s_page_%02d.png" % (Path(out_path).stem, pg)).write_bytes(png)
        gl.setdefault("images", []).append({"name": "page_%02d" % pg, "mimeType": "image/png",
                                            "bufferView": g.view(png)})
        gl.setdefault("samplers", [{"magFilter": 9728, "minFilter": 9728, "wrapS": 33071, "wrapT": 33071}])
        gl.setdefault("textures", []).append({"sampler": 0, "source": len(gl["images"]) - 1})
        opaque = dav["pages"][pg]["kind"] == "opaque"
        gl["materials"].append({"name": "page_%02d" % pg, "doubleSided": True,
                                "alphaMode": "OPAQUE" if opaque else "MASK",
                                "pbrMetallicRoughness": {"baseColorTexture": {"index": len(gl["textures"]) - 1},
                                                         "metallicFactor": 0.0, "roughnessFactor": 1.0}})
        mat_of_page[pg] = len(gl["materials"]) - 1
    gl["materials"].append({"name": "vertex_colour", "doubleSided": True,
                            "pbrMetallicRoughness": {"metallicFactor": 0.0, "roughnessFactor": 1.0}})
    mat_of_page[None] = len(gl["materials"]) - 1
    if write_png:  # each image the model uses, cut out of its page (original pixels), named by .DAV rectangle
        folder = Path(out_path).with_name(Path(out_path).stem + "_textures")
        folder.mkdir(exist_ok=True)
        for tid in sorted({x for en in war_meshes.read_mesh(d, off)["entries"] for x in en["ids"]}):
            r = dav["entries"][tid]
            x, w, y, h, pg = dav["rects"][r]
            pages[pg].crop((x, y, x + w, y + h)).save(folder / ("rect%03d_page%02d_%dx%d.png" % (r, pg, w, h)))

    # one primitive per material; corners welded when vertex, uv and colour agree; positions in rest (bind) space
    prims = []
    for pg in used_pages + [None]:
        index, pos, uvs, cols, joints, tris = {}, [], [], [], [], []
        for vi, puv, pc, ppg in polys:
            if ppg != pg:
                continue
            for c in range(3):
                v = vi[c]
                if v >= len(verts):
                    break
                colour = tuple(srgb_to_linear(min(1.0, x / 128.0)) for x in pc[c])
                key = (v, puv[c] if puv else None, colour)
                if key not in index:
                    index[key] = len(pos)
                    j = owner[v]
                    pos.append([verts[v][a] + rest[j][a] for a in range(3)])
                    if puv:
                        uvs.append(list(puv[c]))
                    cols.append(list(colour) + [1.0])
                    joints.append([j, 0, 0, 0])
                tris.append(index[key])
            else:
                continue
            del tris[len(tris) - len(tris) % 3:]
        if not tris:
            continue
        attrs = {"POSITION": g.accessor("f", 5126, "VEC3", pos, 34962, minmax=True),
                 "COLOR_0": g.accessor("f", 5126, "VEC4", cols, 34962),
                 "JOINTS_0": g.accessor("H", 5123, "VEC4", joints, 34962),
                 "WEIGHTS_0": g.accessor("f", 5126, "VEC4", [[1.0, 0.0, 0.0, 0.0]] * len(pos), 34962)}
        if uvs:
            attrs["TEXCOORD_0"] = g.accessor("f", 5126, "VEC2", uvs, 34962)
        prims.append({"attributes": attrs, "indices": g.accessor("H", 5123, "SCALAR", tris, 34963),
                      "material": mat_of_page[pg]})
    gl["meshes"].append({"name": "mesh", "primitives": prims})

    ibm = []
    for r in rest:
        ibm.append([1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -r[0], -r[1], -r[2], 1])
    gl["skins"] = [{"joints": joint_node, "skeleton": part_node[0],
                    "inverseBindMatrices": g.accessor("f", 5126, "MAT4", ibm)}]
    gl["nodes"].append({"name": "mesh", "mesh": 0, "skin": 0})
    root["children"].append(len(gl["nodes"]) - 1)
    gl["scenes"] = [{"nodes": [0]}]

    # animations: frame k's pose is reached at the sum of the earlier frames' durations
    for name, frames in anims:
        if not frames:
            continue
        times, t = [], 0.0
        for dur, _poses in frames:
            times.append(t / 1000.0)
            t += dur
        if len(frames) == 1:
            times = [0.0]
        tacc = g.accessor("f", 5126, "SCALAR", times, minmax=True)
        samplers, channels = [], []
        for k, p in enumerate(parts):
            pscale = [poses[p["parent"]][2] if k and p["parent"] < k else [1.0, 1.0, 1.0] for _d, poses in frames]
            rot = [part_quat(poses[k][0]) for _d, poses in frames]
            tr = [[poses[k][1][a] + (p["ox"], p["oy"], p["oz"])[a] * pscale[f][a] for a in range(3)]
                  for f, (_d, poses) in enumerate(frames)]
            for q in range(1, len(rot)):  # keep neighbouring quaternions on the same side
                if sum(a * b for a, b in zip(rot[q], rot[q - 1])) < 0:
                    rot[q] = [-c for c in rot[q]]
            chans = [("rotation", part_node[k], "VEC4", rot), ("translation", part_node[k], "VEC3", tr)]
            if scaled:
                chans.append(("scale", joint_node[k], "VEC3", [list(poses[k][2]) for _d, poses in frames]))
            for path, node, atype, rows in chans:
                samplers.append({"input": tacc, "output": g.accessor("f", 5126, atype, rows),
                                 "interpolation": "LINEAR"})
                channels.append({"sampler": len(samplers) - 1, "target": {"node": node, "path": path}})
        gl.setdefault("animations", []).append({"name": name, "samplers": samplers, "channels": channels})

    g.write(out_path)
    print("%s res %d -> %s: %d vertices, %d triangles, %d parts, %d animations, pages %s%s" % (
        level, res, out_path, len(verts), len(polys), len(parts), len(gl.get("animations", [])),
        " ".join(map(str, used_pages)) or "-", "; skipped %s" % skipped if skipped else ""))


def wolf_model(level, game=retexture.DEFAULT_GAME):
    """Ralph's model: the resource the Wolf's scenaric record (type 5, class 0 at +0x0A) names in its first u16
    (ScnObject::InitWithAltModels)."""
    folder = game / "Levels" / level
    war = next((f for f in folder.iterdir() if f.suffix.lower() == ".war"), None)
    if war is None:
        sys.exit("%s: no .WAR" % level)
    d = war.read_bytes()
    count, = struct.unpack_from("<I", d, 0x0C)
    for e in struct.unpack_from("<%dI" % count, d, 0x10):
        if (e >> 24) & 0xBF == 5:
            model, _r2, _x, _y, _z, cls = struct.unpack_from("<2H3hH", d, e & 0xFFFFFF)
            if cls == 0 and model != 0xFFFF:
                return model
    sys.exit("%s: no Wolf record" % level)


def list_models(level, game=retexture.DEFAULT_GAME):
    folder = game / "Levels" / level
    files = {f.suffix.lower(): f for f in folder.iterdir()}
    d = files[".war"].read_bytes()
    count, = struct.unpack_from("<I", d, 0x0C)
    for i, e in enumerate(struct.unpack_from("<%dI" % count, d, 0x10)):
        t, off = (e >> 24) & 0xBF, e & 0xFFFFFF
        if t != MODEL:
            continue
        _va, _ea, nv, _ne = struct.unpack_from("<IIHH", d, off)
        parts = read_parts(d, off)
        anims = read_anims(d, off, len(parts))
        print("  res %4d  %4d vertices %3d parts %3d animations  %s" % (
            i, nv, len(parts), len(anims), " ".join(n for n, _f in anims[:6]) + (" ..." if len(anims) > 6 else "")))


FBX_SCRIPT = """
import bpy, sys
src, dst = sys.argv[sys.argv.index("--") + 1:]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=src)
for o in [o for o in bpy.data.objects if o.type == "MESH" and not o.find_armature() and not o.parent]:
    bpy.data.objects.remove(o)  # the importer's bone-shape sphere
bpy.ops.export_scene.fbx(filepath=dst, path_mode="COPY", embed_textures=True, add_leaf_bones=False,
                         bake_anim=True, bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False,
                         bake_anim_force_startend_keying=True)
"""


def find_blender(given):
    for c in [given, shutil.which("blender"), "/Applications/Blender.app/Contents/MacOS/Blender"]:
        if c and Path(c).is_file():
            return c
    sys.exit("--fbx needs Blender: put it on PATH or pass --blender PATH")


def to_fbx(glb, blender):
    fbx = Path(glb).with_suffix(".fbx")
    r = subprocess.run([find_blender(blender), "-b", "--factory-startup", "--python-expr", FBX_SCRIPT, "--",
                        str(Path(glb).resolve()), str(fbx.resolve())], capture_output=True, text=True)
    if r.returncode != 0 or not fbx.is_file():
        sys.exit("Blender failed:\n" + (r.stderr or r.stdout)[-2000:])
    print("  -> %s" % fbx)


def main(argv):
    if len(argv) >= 2 and argv[1] == "--list":
        list_models(argv[0])
        return 0
    if len(argv) < 3:
        sys.exit(__doc__)
    rest = argv[3:]
    res = wolf_model(argv[0]) if argv[1].lower() in ("ralph", "wolf") else int(argv[1], 0)
    export(argv[0], res, argv[2], use_retexture="--retexture" in rest, write_png="--png" in rest)
    if "--fbx" in rest:
        to_fbx(argv[2], rest[rest.index("--blender") + 1] if "--blender" in rest else None)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
