#!/usr/bin/env python3
"""{110} 晶面族：12 个面，2 个面一组开六组 + 一个组合。

只画简单立方（SC）：格点在整数位置，晶胞取 a = 1。
面片 = 平面 hx + ky + lz = 1 与晶体块 |x|,|y|,|z| <= 1 的截面（Sutherland-Hodgman 裁剪）。
组合那张把 12 张面各自沿法线推到距原点 d = a/sqrt(2) 处，围成菱形十二面体。

    python3 lattice-planes/plot110.py            # 出 figs/*.png
"""
from itertools import permutations, product
from pathlib import Path

import numpy as np
import matplotlib as mpl
mpl.use("Agg")
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d.art3d import Poly3DCollection

OUT = Path(__file__).parent / "figs"
OUT.mkdir(exist_ok=True)

HALF = 1.0          # 画出的晶体 = 2a × 2a × 2a
CELL = 0.5          # 参考晶胞 [-1/2, 1/2]^3
GROUP_COLORS = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b"]

mpl.rcParams.update({
    "font.sans-serif": ["Source Han Sans CN", "Source Han Sans SC", "Noto Sans CJK SC", "DejaVu Sans"],
    "axes.unicode_minus": False,
    "font.size": 10,
    "figure.facecolor": "white",
    "savefig.facecolor": "white",
})


def label(hkl: tuple[int, int, int]) -> str:
    """密勒指数的 mathtext 写法：负指标加上横线 → $(\\bar{1}10)$"""
    body = "".join(rf"\bar{{{abs(c)}}}" if c < 0 else str(c) for c in hkl)
    return f"$({body})$"


def directions(hkl: tuple[int, int, int]) -> list[tuple[int, int, int]]:
    """{hkl} 的全部等效面：指标的所有排列 × 所有符号组合，去重"""
    out = {tuple(s * v for s, v in zip(sig, p))
           for p in permutations(hkl) for sig in product((1, -1), repeat=3)}
    return sorted(out, key=lambda v: (sum(c < 0 for c in v), -max(i for i, c in enumerate(v) if c), v))


def pairs(hkl: tuple[int, int, int]):
    """12 个面配成 6 对平行面：(hkl) 与 (h̄k̄l̄) 一组，代表面取首个非零指标为正的"""
    seen: dict[tuple[int, int, int], list] = {}
    for v in directions(hkl):
        first = next(c for c in v if c)
        key = v if first > 0 else tuple(-c for c in v)
        seen.setdefault(key, []).append(v)
    def order_key(v):
        zero = next((i for i, c in enumerate(v) if c == 0), 3)
        return (sum(c < 0 for c in v), -zero, *v)
    ordered = sorted(seen.items(), key=lambda kv: order_key(kv[0]))
    return [(k, sorted(vs, key=lambda v: v != k)) for k, vs in ordered]


def clip(poly, n, t):
    """保留 n·p <= t 的一侧"""
    out = []
    for a, b in zip(poly, np.roll(poly, -1, axis=0)):
        fa, fb = n @ a - t, n @ b - t
        if fa <= 1e-12:
            out.append(a)
        if fa * fb < 0:
            out.append(a + (b - a) * fa / (fa - fb))
    return np.array(out) if out else None


def simplify(poly):
    """去掉重合点与共线点：否则多边形的边会画出多余的接缝"""
    out = []
    for p in poly:
        if not out or np.linalg.norm(p - out[-1]) > 1e-9:
            out.append(p)
    if len(out) > 1 and np.linalg.norm(out[0] - out[-1]) < 1e-9:
        out.pop()
    keep = []
    for i, p in enumerate(out):
        a, b, c = out[i - 1], p, out[(i + 1) % len(out)]
        if np.linalg.norm(np.cross(b - a, c - b)) > 1e-9:
            keep.append(p)
    return np.array(keep) if len(keep) >= 3 else None


def patch(n, t, half):
    """平面 n·r = t 与立方块 [-half, half]^3 的截面多边形"""
    n = np.asarray(n, float)
    u = np.cross(n, [0, 0, 1]) if abs(n[2]) < 0.9 else np.cross(n, [1, 0, 0])
    u /= np.linalg.norm(u)
    v = np.cross(n, u)
    base = n * t / (n @ n)
    poly = np.array([base + s * u + q * v for s, q in ((-9, -9), (9, -9), (9, 9), (-9, 9))])
    for axis in range(3):
        for s in (1, -1):
            if poly is None:
                return None
            e = np.zeros(3)
            e[axis] = s
            poly = clip(poly, e, half)      # 留 s·p[axis] <= half 的一侧
    return simplify(poly)


def lattice_points(half=HALF):
    g = np.arange(-1, 2)
    return np.array(np.meshgrid(g, g, g, indexing="ij")).reshape(3, -1).T


def frame(ax, pts, cell=True):
    """晶体块外框 + 参考晶胞 + 格点 + 晶轴"""
    h = HALF
    for a in (-h, h):
        for b in (-h, h):
            ax.plot([-h, h], [a, a], [b, b], lw=.6, color="#bbbbbb")
            ax.plot([a, a], [-h, h], [b, b], lw=.6, color="#bbbbbb")
            ax.plot([a, a], [b, b], [-h, h], lw=.6, color="#bbbbbb")
    if cell:
        c = CELL
        for a in (-c, c):
            for b in (-c, c):
                ax.plot([-c, c], [a, a], [b, b], lw=.7, ls=":", color="#777777")
                ax.plot([a, a], [-c, c], [b, b], lw=.7, ls=":", color="#777777")
                ax.plot([a, a], [b, b], [-c, c], lw=.7, ls=":", color="#777777")
    ax.scatter(*pts.T, s=26, c="#333333", depthshade=False, zorder=5)
    for e, name in zip(np.eye(3), "xyz"):
        ax.quiver(*(e * 1.32), *(e * -0.28), arrow_length_ratio=.09, lw=1.1,
                  color="#cc3311" if name == "x" else "#228833" if name == "y" else "#2255bb")
        ax.text(*(e * 1.46), name, fontsize=9)


def view(ax, n, zoom=1.0):
    """按面法线选机位：视线与法线成约 50°，否则两张平行面会互相挡住或看成一条缝"""
    n = np.asarray(n, float)
    side = np.cross(n, [0, 0, 1])
    d = 0.9 * n + 0.7 * (side / (np.linalg.norm(side) or 1)) + 0.5 * np.array([0, 0, 1])
    ax.view_init(elev=np.degrees(np.arcsin(d[2] / np.linalg.norm(d))),
                 azim=np.degrees(np.arctan2(d[1], d[0])))
    lim = 1.25 * zoom
    ax.set_xlim(-lim, lim); ax.set_ylim(-lim, lim); ax.set_zlim(-lim, lim)
    ax.set_box_aspect((1, 1, 1))
    ax.axis("off")


def group_figure(key, members, idx, path):
    pts = lattice_points()
    fig = plt.figure(figsize=(5.2, 4.6))
    ax = fig.add_subplot(111, projection="3d")
    frame(ax, pts)
    for side, hkl in enumerate(members):
        col = GROUP_COLORS[idx]
        n = np.asarray(hkl, float) / np.linalg.norm(hkl)
        u = np.cross(n, [0, 0, 1])
        u /= (np.linalg.norm(u) or 1)
        ax.add_collection3d(Poly3DCollection([patch(hkl, 1, HALF)], facecolor=col, alpha=.30,
                                             edgecolor="k", linewidth=1.0))
        c = patch(hkl, 1, HALF).mean(axis=0)
        # 正面标签往面内一侧推、反面往另一侧推，免得两个 (hkl) 叠在一起
        ax.text(*(c + n * 0.26 + u * (0.62 if side == 0 else -0.62)), label(hkl), fontsize=11)
        ax.quiver(*(n * 0.16), *(n * 1.0), arrow_length_ratio=.12, lw=1.5, color=col)
        ax.text(*(n * 1.30 - u * (0.16 if side == 0 else -0.16)),
                label(hkl).replace("(", "[").replace(")", "]"), fontsize=9, color=col)
    view(ax, key)
    d = 1 / np.linalg.norm(key)
    ax.set_title(f"第 {idx + 1} 组 {label(key)} ∥ {label(tuple(-c for c in key))}"
                 f"  d = a/√2 = {d:.3f}a", pad=-14, fontsize=11)
    fig.tight_layout()
    fig.savefig(path, dpi=200, bbox_inches="tight")
    plt.close(fig)


def form_face(n, cons):
    """晶形上属于面 n 的那张多边形：从一块大面片出发，依次用 12 对半空间裁出来"""
    u = np.cross(n, [0, 0, 1]) if abs(n[2]) < .9 else np.cross(n, [1, 0, 0])
    u /= np.linalg.norm(u)
    v = np.cross(n, u)
    poly = np.array([n / (n @ n) + s * 9 * u + q * 9 * v for s in (-1, 1) for q in (-1, 1)])
    for m in cons:
        if poly is None:
            return None
        poly = clip(poly, m, 1.0)
        if poly is not None:
            poly = clip(poly, -m, 1.0)
    return simplify(poly) if poly is not None else None


def form_figure(fam, path):
    """12 张面围成晶形：|n·r| <= 1 的交集 = 菱形十二面体"""
    cons = [np.asarray(v, float) for v in fam]
    fig = plt.figure(figsize=(5.6, 4.8))
    ax = fig.add_subplot(111, projection="3d")
    for i, n in enumerate(cons):
        poly = form_face(n, cons)
        if poly is None or len(poly) < 3:
            continue
        ax.add_collection3d(Poly3DCollection([poly], facecolor=GROUP_COLORS[i % 6], alpha=.28,
                                             edgecolor="k", linewidth=.9))
        ax.text(*(poly.mean(axis=0) * 1.28), label(tuple(int(x) for x in n)), fontsize=8.5)
    h = CELL
    for a in (-h, h):
        for b in (-h, h):
            ax.plot([-h, h], [a, a], [b, b], lw=.7, ls=":", color="#666666")
            ax.plot([a, a], [-h, h], [b, b], lw=.7, ls=":", color="#666666")
            ax.plot([a, a], [b, b], [-h, h], lw=.7, ls=":", color="#666666")
    verts = np.array([[x, y, z] for x in (-.5, .5) for y in (-.5, .5) for z in (-.5, .5)]
                     + [[s, 0, 0] for s in (-1, 1)] + [[0, s, 0] for s in (-1, 1)] + [[0, 0, s] for s in (-1, 1)])
    ax.scatter(*verts.T, s=24, c="#333333", depthshade=False)
    ax.view_init(elev=18, azim=-58)
    ax.set_xlim(-1.3, 1.3); ax.set_ylim(-1.3, 1.3); ax.set_zlim(-1.3, 1.3)
    ax.set_box_aspect((1, 1, 1)); ax.axis("off")
    ax.set_title("组合 12 个面围成菱形十二面体（12 面 · 14 顶点 · 24 棱）", pad=-10, fontsize=11)
    fig.tight_layout()
    fig.savefig(path, dpi=200, bbox_inches="tight")
    plt.close(fig)


def overview(grps, path):
    fig = plt.figure(figsize=(13.5, 9.6))
    pts = lattice_points()
    for i, (key, members) in enumerate(grps):
        ax = fig.add_subplot(2, 4, i + 1, projection="3d")
        frame(ax, pts, cell=False)
        for m in members:
            poly = patch(m, 1, HALF)
            ax.add_collection3d(Poly3DCollection([poly], facecolor=GROUP_COLORS[i], alpha=.30,
                                                 edgecolor="k", linewidth=.9))
            ax.text(*(poly.mean(axis=0) + np.asarray(m) / np.linalg.norm(m) * .24),
                    label(m), fontsize=9.5)
        view(ax, key, zoom=1.12)
        ax.set_title(f"{i + 1} {label(key)} ∥ {label(tuple(-c for c in key))}", fontsize=10.5)
    ax = fig.add_subplot(2, 4, 7, projection="3d")
    frame(ax, pts, cell=False)
    for i, (key, _) in enumerate(grps):
        for m in key, tuple(-c for c in key):
            poly = patch(m, 1, HALF)
            ax.add_collection3d(Poly3DCollection([poly], facecolor=GROUP_COLORS[i], alpha=.22,
                                                 edgecolor="k", linewidth=.7))
    view(ax, (1, 1, 1), zoom=1.05)
    ax.set_title("12 个面一起看", fontsize=10.5)
    ax8 = fig.add_subplot(2, 4, 8)
    ax8.axis("off")
    ax8.text(.02, .95, "{110} 晶面族（简单立方，a = 1）\n\n"
             "· 12 个面：指标绝对值是 {1,1,0} 的排列 → 3 × 4 = 12\n"
             "· 6 组平行对：法线互为反向的两个面算一组，12 ÷ 2 = 6 个朝向\n"
             "· 面间距 d(110) = a/√(h²+k²+l²) = a/√2 = 0.7071a\n"
             "· 面方程 x ± y = ±1 等，截距 (a, a, ∞) → 倒数 (1,1,0)\n"
             "· 法线 [hkl] ⊥ 面 (hkl)；相邻两面法线夹角 60°\n"
             "· 12 张面立在晶胞 12 条棱上 → 菱形十二面体\n"
             "· 菱形对角线比 √2，内角 70.53° / 109.47°",
             va="top", fontsize=10.5, linespacing=1.75)
    fig.tight_layout()
    fig.savefig(path, dpi=200, bbox_inches="tight")
    plt.close(fig)


def main():
    fam = directions((1, 1, 0))
    grps = pairs((1, 1, 0))
    assert len(fam) == 12 and len(grps) == 6, (len(fam), len(grps))
    for i, (key, members) in enumerate(grps):
        p = OUT / f"plane-110-group-{i + 1}.png"
        group_figure(key, members, i, p)
        print("wrote", p.relative_to(OUT.parent))
    form_figure(fam, OUT / "plane-110-form.png")
    print("wrote", (OUT / "plane-110-form.png").relative_to(OUT.parent))
    overview(grps, OUT / "plane-110-overview.png")
    print("wrote", (OUT / "plane-110-overview.png").relative_to(OUT.parent))


if __name__ == "__main__":
    main()
