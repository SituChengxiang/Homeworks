#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
模块九：可视化

读取 C89 程序写出的 data/traj_<id>.dat（正文 5 列 t x y vx vy，# 起注释头），
把六条轨迹画在一张图上。题目三条硬性要求：
  * 横纵坐标都限制在 -22.5 .. 22.5
  * 六条曲线在颜色或虚实上有区分
  * 每条曲线有文字标识
编码方案：
  颜色   -> 初速度大小三档
  实/虚  -> 初始位置在 x 轴上方 / 下方
"""

import glob
import os
import re
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib import font_manager
from matplotlib.lines import Line2D

LIMIT = 22.5                                   # 与 include/units.h 的 PLOT_LIMIT 一致
COLOR_BY_V = {1.0: "#2c7fb8", 1.5: "#e6913a", 3.0: "#b3121b"}
STYLE_BY_SIDE = {"up": ("-", 2.1), "down": ((0, (5.5, 2.6)), 1.9)}


def pick_cjk_font():
    """图上要写中文，挑一个本机确实装上的黑体，避免豆腐块。"""
    candidates = ["Source Han Sans CN", "Source Han Sans SC", "Noto Sans CJK SC",
                  "WenQuanYi Zen Hei", "SimHei", "Microsoft YaHei", "FZHei-B01"]
    installed = {f.name for f in font_manager.fontManager.ttflist}
    for name in candidates:
        if name in installed:
            return name
    return None


def load_case(path):
    """# key : value 进 dict，正文进数组。"""
    meta, rows = {}, []
    with open(path) as fp:
        for line in fp:
            if line.startswith("#"):
                m = re.match(r"#\s*([A-Za-z_]+)\s*:\s*(.*)", line)
                if m:
                    meta[m.group(1)] = m.group(2).strip()
            elif line.strip():
                rows.append([float(v) for v in line.split()])
    meta["id"] = int(re.search(r"traj_(\d+)", path).group(1))
    return meta, np.array(rows)


def parse_init(meta):
    """
    '# init : (x0,y0) = (-20.0000, 20.0000)   (vx0,vy0) = (1.0000, 0.0000)'
    必须按括号配对取数：直接对所有数字做 findall 会把 'x0'、'vy0' 里的
    那个 0 也当成数值，取到的就不是初值了。
    """
    pairs = re.findall(r"\(([^()]*)\)", meta.get("init", ""))
    vals = []
    for pair in pairs:
        parts = [v.strip() for v in pair.split(",")]
        try:
            vals += [float(v) for v in parts]
        except ValueError:
            continue          # 跳过 '(x0,y0)' 这种占位括号
    if len(vals) < 4:
        raise ValueError("init 行解析失败: " + meta.get("init", ""))
    return vals[0], vals[1], vals[2], vals[3]          # x0, y0, vx0, vy0


def visible_segment(x, y):
    """只保留画框内的部分。
    annotate 的锚点一旦落到 xlim/ylim 之外，matplotlib 会把整条标注
    （连文字带引导线）一起裁掉，所以锚点必须从可见段里取。
    """
    m = (np.abs(x) <= LIMIT) & (np.abs(y) <= LIMIT)
    return x[m], y[m]


def arc_point(x, y, frac):
    """按弧长取曲线 frac 处的点，作为标签引导线的终点。"""
    seg = np.hypot(np.diff(x), np.diff(y))
    s = np.concatenate([[0.0], np.cumsum(seg)])
    i = int(np.searchsorted(s, s[-1] * frac))
    i = max(0, min(i, len(x) - 1))
    return x[i], y[i]


# 标签：文字位置手工排布在图中央的空带上，引导线终点按可见段弧长自动取。
# 六条曲线全部贴着 y = +-20 的窄带走（最近距离就有 21 A），
# 所以空白区恰好在中间，标签放这里既不与曲线争位置，引导线也不会互相交叉。
LABELS = {
    1: {"text": "v\u2080 = 1.0,  y\u2080 = +20", "xy": (-21.6, 15.0), "frac": 1.0},
    2: {"text": "v\u2080 = 1.5,  y\u2080 = +20", "xy": (-12.6, 11.6), "frac": 1.0},
    3: {"text": "v\u2080 = 3.0,  y\u2080 = +20", "xy": (4.6, 15.0),   "frac": 1.0},
    4: {"text": "v\u2080 = 1.0,  y\u2080 = \u221220", "xy": (-21.6, -16.2), "frac": 1.0},
    5: {"text": "v\u2080 = 1.5,  y\u2080 = \u221220", "xy": (-12.6, -12.8), "frac": 1.0},
    6: {"text": "v\u2080 = 3.0,  y\u2080 = \u221220", "xy": (4.6, -16.2),   "frac": 1.0},
}


def main(datadir="data", outname="trajectories"):
    files = sorted(glob.glob(os.path.join(datadir, "traj_*.dat")))
    if not files:
        sys.exit(f"没找到 {datadir}/traj_*.dat —— 先跑 "
                 f"./build/alpha_scattering {datadir} -t 20")

    cjk = pick_cjk_font()
    if cjk:
        plt.rcParams["font.family"] = [cjk, "DejaVu Sans"]
        plt.rcParams["axes.unicode_minus"] = False

    fig, ax = plt.subplots(figsize=(7.8, 7.8))

    # 参考线：假如没有库仑力，粒子会沿 y = +-20 直行
    for yside in (20.0, -20.0):
        ax.plot([-LIMIT, LIMIT], [yside, yside], ls=":", lw=0.9,
                color="#9aa0a6", zorder=1)
        ax.text(LIMIT - 0.3, yside + (1.5 if yside > 0 else -1.5),
                "未偏转入射方向", fontsize=7.8, color="#6b7280",
                ha="right", va="center")

    drawn, marked = [], set()
    de_max = 0.0
    run_desc = ""
    for path in files:
        meta, arr = load_case(path)
        x, y = arr[:, 1], arr[:, 2]
        x0, y0, vx0, _ = parse_init(meta)
        de_max = max(de_max, float(re.search(r"([\d.eE+-]+)\s*$",
                                             meta["invariant"]).group(1)))
        run_desc = meta.get("run", "")
        v_key = min(COLOR_BY_V, key=lambda k: abs(k - abs(vx0)))
        side = "up" if y0 > 0 else "down"
        ls, lw = STYLE_BY_SIDE[side]
        col = COLOR_BY_V[v_key]

        ax.plot(x, y, ls=ls, lw=lw, color=col, zorder=3)

        # 每组起点只画一次：三条曲线共用车同一个出发点
        if (round(x0, 3), round(y0, 3)) not in marked:
            marked.add((round(x0, 3), round(y0, 3)))
            ax.plot(x0, y0, marker="o", ms=5, mfc="white", mec="#5b6166",
                    mew=1.2, zorder=4)
            ax.annotate("", xy=(x0 + 3.4, y0), xytext=(x0, y0),
                        arrowprops=dict(arrowstyle="-|>", color="#5b6166",
                                        lw=1.0, alpha=0.8), zorder=4)

        drawn.append((meta["id"], x, y, col, v_key, side,
                        arr[:, 0], np.hypot(x, y)))

    for cid, x, y, col, v_key, side, _t, _r in drawn:
        lab = LABELS[cid]
        vx, vy = visible_segment(x, y)
        ax_, ay_ = arc_point(vx, vy, lab["frac"])
        tx, ty = lab["xy"]
        ax.annotate(lab["text"], (ax_, ay_), xytext=(tx, ty),
                    fontsize=8.4, color=col, zorder=6,
                    ha="left", va="center",
                    bbox=dict(boxstyle="round,pad=0.26", fc="white",
                              ec=col, lw=0.9, alpha=0.94),
                    arrowprops=dict(arrowstyle="-", color=col, lw=0.65,
                                    alpha=0.55, linestyle=":",
                                    shrinkA=14, shrinkB=2))

    # 入射高度：从 x 轴到起点的竖直距离，也就是几何上的 |y0|
    ax.annotate("", xy=(-20.0, 20.0), xytext=(-20.0, 0.0),
                arrowprops=dict(arrowstyle="<->", color="#6b7280", lw=0.9,
                                shrinkA=0, shrinkB=0), zorder=4)
    ax.annotate("", xy=(-20.0, -20.0), xytext=(-20.0, 0.0),
                arrowprops=dict(arrowstyle="<->", color="#6b7280", lw=0.9,
                                shrinkA=0, shrinkB=0), zorder=4)
    ax.text(-19.2, 10.0, "|y$_0$| = 20 \u00c5", fontsize=7.6, color="#5b6166",
            rotation=90, va="center", ha="center")
    ax.text(-19.2, -10.0, "|y$_0$| = 20 \u00c5", fontsize=7.6, color="#5b6166",
            rotation=90, va="center", ha="center")

    # 散射中心
    ax.plot(0, 0, marker="*", ms=16, color="#b3121b", mec="#7a0d12",
            mew=0.8, zorder=5)
    ax.annotate("原子核 97e", (0, 0), xytext=(-10.6, 3.6),
                fontsize=8.2, color="#7a0d12", zorder=6,
                arrowprops=dict(arrowstyle="-|>", color="#b3121b", lw=0.9,
                                shrinkA=0, shrinkB=7))

    ax.set_xlim(-LIMIT, LIMIT)
    ax.set_ylim(-LIMIT, LIMIT)
    ax.set_aspect("equal")
    ax.set_xlabel("x  (\u00c5)", fontsize=10.5)
    ax.set_ylabel("y  (\u00c5)", fontsize=10.5)
    ax.set_title("\u03b1 粒子在 97e 核库仑场中的散射轨道", fontsize=12.5, pad=9)
    ax.tick_params(labelsize=9)
    ax.grid(True, ls=":", lw=0.5, color="#d5d8dc", alpha=0.85)
    for sp in ax.spines.values():
        sp.set_color("#9aa0a6")

    handles = [Line2D([], [], color=COLOR_BY_V[v], lw=2.1,
                      label=f"|v\u2080| = {v:g}") for v in (1.0, 1.5, 3.0)]
    handles += [Line2D([], [], color="#3d4349", lw=2.0, ls="-",
                       label="y\u2080 = +20  实线"),
                Line2D([], [], color="#3d4349", lw=2.0,
                       ls=(0, (5.5, 2.6)), label="y\u2080 = \u221220  虚线")]
    # 图例放在右下方：那里是子图下方的空带，不会压住竖尺和六条曲线
    leg = ax.legend(handles=handles, loc="center right",
                    bbox_to_anchor=(0.995, 0.128), fontsize=8.6, frameon=True,
                    framealpha=0.95, edgecolor="#9aa0a6", borderpad=0.7,
                    handletextpad=0.7, labelspacing=0.55)
    leg.set_zorder(7)

    # 内嵌小图：六条轨迹的位径 r(t)。主图里六条曲线都贴着上下两条窄带，
    # 谁离核更近、什么时候到达最近点，肉眼分不出来，这张小图就是回答这个的。
    ax_in = ax.inset_axes([0.575, 0.305, 0.405, 0.320])
    for cid, x, y, col, v_key, side, tt, rr in drawn:
        ls, lw = STYLE_BY_SIDE[side]
        ax_in.plot(tt, rr, ls=ls, lw=1.5, color=col, zorder=3)
        kmin = int(np.argmin(rr))
        ax_in.plot(tt[kmin], rr[kmin], marker="o", ms=3.0, color=col, zorder=4)
    ax_in.set_xlabel("t", fontsize=7.6)
    ax_in.set_ylabel("r  (\u00c5)", fontsize=7.6)
    ax_in.tick_params(labelsize=6.8, direction="in")
    ax_in.grid(True, ls=":", lw=0.45, color="#d5d8dc", alpha=0.9)
    ax_in.set_title("位径 r(t)，圆点为最近距离处\n（上下两组镜像，r(t) 完全重合）", fontsize=7.4, pad=3)
    for sp in ax_in.spines.values():
        sp.set_color("#9aa0a6"); sp.set_linewidth(0.7)

    # 积分方法与步长从 .dat 文件头里读出来，图上的说明永远和实际算的一致
    m_kind = re.search(r"integrator\s*=\s*(\S+)", run_desc)
    m_dt = re.search(r"dt\s*=\s*([\d.]+)", run_desc)
    kind = {"RK4": "四阶 Runge\u2013Kutta"}.get(
        m_kind.group(1) if m_kind else "", m_kind.group(1) if m_kind else "?")
    dt_txt = f"dt = {float(m_dt.group(1)):g}" if m_dt else "dt = ?"

    note = ("约化单位：电荷 e、质量 相对分子质量、长度 \u00c5、库仑常数 k = 1   ·   "
            f"{kind}，{dt_txt}，能量漂移 max|dE|/E \u2264 {de_max:.1e}")
    fig.text(0.5, 0.008, note, ha="center", fontsize=7.6, color="#5b6166")

    fig.subplots_adjust(left=0.082, right=0.988, top=0.955, bottom=0.085)

    png = os.path.join(datadir, outname + ".png")
    pdf = os.path.join(datadir, outname + ".pdf")
    fig.savefig(png, dpi=200)
    fig.savefig(pdf)
    print(f"已写出 {png}\n       {pdf}")


if __name__ == "__main__":
    main(*(sys.argv[1:2] or ["data"]))
