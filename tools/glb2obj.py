"""Convert a .glb (glTF 2.0 binary) into OBJ + MTL (+ extracted textures).

Usage: python -I glb2obj.py <input.glb> <output_dir>

- Bakes the node hierarchy transforms into the vertices (static meshes).
- Skinned meshes are posed in their bind/rest pose using the skin's
  joint world matrices and inverse bind matrices (linear blend skinning).
- Each glTF material becomes an OBJ material with Kd = baseColorFactor and
  map_Kd = extracted baseColorTexture (if any).
- V texture coordinate is flipped (glTF top-left -> OBJ bottom-left).
"""
import json
import os
import struct
import sys

import numpy as np

COMP = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
NCOMP = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


def load_glb(path):
    data = open(path, "rb").read()
    magic, _version, _length = struct.unpack("<4sII", data[:12])
    if magic != b"glTF":
        raise ValueError("not a GLB file")
    off, gltf, binchunk = 12, None, b""
    while off < len(data):
        clen, ctype = struct.unpack("<II", data[off:off + 8])
        chunk = data[off + 8:off + 8 + clen]
        if ctype == 0x4E4F534A:
            gltf = json.loads(chunk)
        elif ctype == 0x004E4942:
            binchunk = chunk
        off += 8 + clen
    return gltf, binchunk


def read_accessor(g, binchunk, idx):
    acc = g["accessors"][idx]
    view = g["bufferViews"][acc["bufferView"]]
    dtype = np.dtype(COMP[acc["componentType"]])
    n = NCOMP[acc["type"]]
    count = acc["count"]
    start = view.get("byteOffset", 0) + acc.get("byteOffset", 0)
    stride = view.get("byteStride", 0) or dtype.itemsize * n
    raw = np.frombuffer(binchunk, dtype=np.uint8, count=stride * (count - 1) + dtype.itemsize * n, offset=start)
    rows = np.lib.stride_tricks.as_strided(raw, shape=(count, dtype.itemsize * n), strides=(stride, 1))
    arr = np.ascontiguousarray(rows).view(dtype).reshape(count, n)
    if acc.get("normalized"):
        arr = arr.astype(np.float32) / np.iinfo(dtype).max
    return arr


def quat_to_mat(q):
    x, y, z, w = q
    return np.array([
        [1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
        [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
        [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)],
    ])


def local_matrix(node):
    if "matrix" in node:
        return np.array(node["matrix"], dtype=np.float64).reshape(4, 4).T
    m = np.eye(4)
    m[:3, :3] = quat_to_mat(node.get("rotation", [0, 0, 0, 1])) * np.array(node.get("scale", [1, 1, 1]))
    m[:3, 3] = node.get("translation", [0, 0, 0])
    return m


def world_matrices(g):
    nodes = g["nodes"]
    parent = {}
    for i, n in enumerate(nodes):
        for c in n.get("children", []):
            parent[c] = i
    cache = {}

    def world(i):
        if i not in cache:
            m = local_matrix(nodes[i])
            cache[i] = world(parent[i]) @ m if i in parent else m
        return cache[i]

    return [world(i) for i in range(len(nodes))]


def main(src, out_dir):
    g, binchunk = load_glb(src)
    os.makedirs(out_dir, exist_ok=True)
    worlds = world_matrices(g)

    # textures -> files
    tex_files = {}
    for ti, tex in enumerate(g.get("textures", [])):
        img = g["images"][tex["source"]]
        view = g["bufferViews"][img["bufferView"]]
        blob = binchunk[view.get("byteOffset", 0):view.get("byteOffset", 0) + view["byteLength"]]
        ext = ".png" if img.get("mimeType") == "image/png" else ".jpg"
        name = os.path.splitext(img.get("name") or f"texture{ti}")[0] + ext
        with open(os.path.join(out_dir, name), "wb") as f:
            f.write(blob)
        tex_files[ti] = name

    # materials -> mtl
    mat_names = []
    with open(os.path.join(out_dir, "materials.mtl"), "w") as mtl:
        for mi, mat in enumerate(g.get("materials", [])):
            name = f"mat{mi}_{mat.get('name', '')}".replace(" ", "_")
            mat_names.append(name)
            pbr = mat.get("pbrMetallicRoughness", {})
            r, gg, b, a = pbr.get("baseColorFactor", [1, 1, 1, 1])
            mtl.write(f"newmtl {name}\nKd {r:.4f} {gg:.4f} {b:.4f}\n")
            if a < 1:
                mtl.write(f"d {a:.4f}\n")
            if "baseColorTexture" in pbr:
                mtl.write(f"map_Kd {tex_files[pbr['baseColorTexture']['index']]}\n")
            mtl.write("\n")

    lines = [f"# converted from {os.path.basename(src)}", "mtllib materials.mtl"]
    vbase = 1
    allpos = []
    tri_total = 0
    for ni, node in enumerate(g["nodes"]):
        if "mesh" not in node:
            continue
        mesh = g["meshes"][node["mesh"]]
        skin = g["skins"][node["skin"]] if "skin" in node else None
        if skin:
            ibm = read_accessor(g, binchunk, skin["inverseBindMatrices"]).reshape(-1, 4, 4).transpose(0, 2, 1) \
                if "inverseBindMatrices" in skin else np.tile(np.eye(4), (len(skin["joints"]), 1, 1))
            joint_mats = np.array([worlds[j] for j in skin["joints"]]) @ ibm
        lines.append(f"o {node.get('name', 'node' + str(ni))}")
        for prim in mesh["primitives"]:
            if prim.get("mode", 4) != 4:
                continue
            at = prim["attributes"]
            pos = read_accessor(g, binchunk, at["POSITION"]).astype(np.float64)
            nrm = read_accessor(g, binchunk, at["NORMAL"]).astype(np.float64) if "NORMAL" in at else None
            uv = read_accessor(g, binchunk, at["TEXCOORD_0"]).astype(np.float64) if "TEXCOORD_0" in at else None
            hom = np.c_[pos, np.ones(len(pos))]
            if skin:
                joints = read_accessor(g, binchunk, at["JOINTS_0"]).astype(np.int64)
                weights = read_accessor(g, binchunk, at["WEIGHTS_0"]).astype(np.float64)
                weights /= np.maximum(weights.sum(1, keepdims=True), 1e-8)
                M = np.einsum("vk,vkij->vij", weights, joint_mats[joints])
            else:
                M = np.broadcast_to(worlds[ni], (len(pos), 4, 4))
            p = np.einsum("vij,vj->vi", M, hom)[:, :3]
            allpos.append(p)
            lines += [f"v {x:.6f} {y:.6f} {z:.6f}" for x, y, z in p]
            if uv is not None:
                lines += [f"vt {u:.6f} {1.0 - v:.6f}" for u, v in uv]
            if nrm is not None:
                nm = np.linalg.inv(M[:, :3, :3]).transpose(0, 2, 1)
                n = np.einsum("vij,vj->vi", nm, nrm)
                n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-8)
                lines += [f"vn {x:.6f} {y:.6f} {z:.6f}" for x, y, z in n]
            idx = read_accessor(g, binchunk, prim["indices"]).ravel() if "indices" in prim else np.arange(len(pos))
            if "material" in prim:
                lines.append(f"usemtl {mat_names[prim['material']]}")
            for t in idx.reshape(-1, 3) + vbase:
                fs = []
                for i in t:
                    if uv is not None and nrm is not None:
                        fs.append(f"{i}/{i}/{i}")
                    elif nrm is not None:
                        fs.append(f"{i}//{i}")
                    elif uv is not None:
                        fs.append(f"{i}/{i}")
                    else:
                        fs.append(str(i))
                lines.append("f " + " ".join(fs))
            tri_total += len(idx) // 3
            vbase += len(pos)

    with open(os.path.join(out_dir, "model.obj"), "w") as f:
        f.write("\n".join(lines) + "\n")
    allp = np.vstack(allpos)
    lo, hi = allp.min(0), allp.max(0)
    print(f"{src} -> {out_dir}/model.obj  triangles={tri_total}  textures={list(tex_files.values())}")
    print(f"  bbox min={np.round(lo, 3)} max={np.round(hi, 3)} size={np.round(hi - lo, 3)}")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
