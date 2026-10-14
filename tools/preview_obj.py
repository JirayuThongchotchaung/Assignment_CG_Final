"""Quick front + side preview of an OBJ (texture / Kd coloured, painter's algorithm).

Usage: python -I preview_obj.py <model.obj> <out.png>
"""
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.collections import PolyCollection
from matplotlib.image import imread


def load(path):
    d = os.path.dirname(path)
    V, VT, faces, mats, cur = [], [], [], {}, None
    for line in open(path):
        t = line.split()
        if not t:
            continue
        if t[0] == "v":
            V.append([float(x) for x in t[1:4]])
        elif t[0] == "vt":
            VT.append([float(x) for x in t[1:3]])
        elif t[0] == "mtllib":
            name = None
            for ml in open(os.path.join(d, t[1])):
                m = ml.split()
                if not m:
                    continue
                if m[0] == "newmtl":
                    name = m[1]; mats[name] = {"Kd": (0.8, 0.8, 0.8), "tex": None}
                elif m[0] == "Kd":
                    mats[name]["Kd"] = tuple(float(x) for x in m[1:4])
                elif m[0] == "map_Kd":
                    mats[name]["tex"] = imread(os.path.join(d, m[1]))
        elif t[0] == "usemtl":
            cur = t[1]
        elif t[0] == "f":
            idx = [p.split("/") for p in t[1:]]
            vi = [int(p[0]) - 1 for p in idx]
            ti = [int(p[1]) - 1 if len(p) > 1 and p[1] else -1 for p in idx]
            faces.append((vi, ti, cur))
    return np.array(V), np.array(VT) if VT else None, faces, mats


def face_colour(f, VT, mats):
    vi, ti, m = f
    mat = mats.get(m, {"Kd": (0.8, 0.8, 0.8), "tex": None})
    if mat["tex"] is not None and VT is not None and ti[0] >= 0:
        u, v = VT[ti].mean(0)
        img = mat["tex"]
        h, w = img.shape[:2]
        c = img[int((1 - v % 1) * (h - 1)), int((u % 1) * (w - 1))][:3]
        return c * np.array(mat["Kd"])
    return mat["Kd"]


def main(path, out):
    V, VT, faces, mats = load(path)
    cols = [face_colour(f, VT, mats) for f in faces]
    views = {"front (-Z)": (0, 1, 2, 1), "side (+X)": (2, 1, 0, -1), "top (+Y)": (0, 2, 1, 1)}
    fig, axes = plt.subplots(1, 3, figsize=(12, 4.5))
    light = np.array([0.3, 0.6, 0.75]); light /= np.linalg.norm(light)
    for ax, (title, (a, b, depth, sgn)) in zip(axes, views.items()):
        polys, colours, depths = [], [], []
        for f, c in zip(faces, cols):
            p = V[f[0]]
            n = np.cross(p[1] - p[0], p[2] - p[0]); nn = np.linalg.norm(n)
            shade = 0.45 + 0.55 * abs(n @ light) / nn if nn > 0 else 1
            polys.append(p[:, [a, b]] if title[0] != "t" else np.c_[p[:, 0], -p[:, 2]])
            colours.append(np.clip(np.array(c) * shade, 0, 1))
            depths.append(sgn * p[:, depth].mean() if title[0] != "t" else p[:, 1].mean())
        order = np.argsort(depths)
        ax.add_collection(PolyCollection([polys[i] for i in order], facecolors=[colours[i] for i in order], edgecolors="none"))
        ax.autoscale(); ax.set_aspect("equal"); ax.set_title(title)
    fig.suptitle(os.path.basename(os.path.dirname(path)))
    fig.tight_layout(); fig.savefig(out, dpi=80)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
