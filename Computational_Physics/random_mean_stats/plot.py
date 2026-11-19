#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
分步作图：分箱散点 / Lagrange 插值 / 正态拟合

读 C89 程序写出的文件：
  data/samples.dat    i x_i
  data/histogram.dat  bin lo hi center count expected
  data/lagrange.dat   x P(x)          （--lagrange）
  data/gauss_fit.dat  x fitted_count  （--gauss）

默认只画散点；--lagrange 叠加 9 次插值；--gauss 同时叠加正态拟合。
最终交付图见 plot_final.py。
"""

import argparse
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib import font_manager


def pick_cjk_font():
    """图上要写中文，挑一个本机确实装上的黑体。"""
    candidates = ["Source Han Sans CN", "Source Han Sans SC", "Noto Sans CJK SC",
                  "WenQuanYi Zen Hei", "SimHei", "Microsoft YaHei", "FZHei-B01"]
    installed = {f.name for f in font_manager.fontManager.ttflist}
    for name in candidates:
        if name in installed:
            return name
    return None


def load_histogram(path):
    """返回 (bin, lo, hi, center, count, expected) 六个一维数组。"""
    rows = []
    with open(path) as fp:
        for line in fp:
            if line.startswith("#") or not line.strip():
                continue
            rows.append([float(v) for v in line.split()])
    data = np.array(rows)
    return (data[:, 0].astype(int), data[:, 1], data[:, 2],
            data[:, 3], data[:, 4], data[:, 5])


def load_samples(path):
    xs = []
    with open(path) as fp:
        for line in fp:
            if line.startswith("#") or not line.strip():
                continue
            xs.append(float(line.split()[1]))
    return np.array(xs)


def load_curve(path):
    rows = []
    with open(path) as fp:
        for line in fp:
            if line.startswith("#") or not line.strip():
                continue
            rows.append([float(v) for v in line.split()])
    data = np.array(rows)
    return data[:, 0], data[:, 1]


def load_params(path):
    d = {}
    with open(path) as fp:
        for line in fp:
            if line.startswith("#") or not line.strip():
                continue
            p = line.split()
            d[(p[0], p[1])] = float(p[2])
    return d


def main():
    ap = argparse.ArgumentParser(description="Step 3/4/5: scatter, Lagrange, Gaussian fit")
    ap.add_argument("datadir", nargs="?", default="data")
    ap.add_argument("-o", "--output", default=None)
    ap.add_argument("--lagrange", action="store_true",
                    help="叠加 9 次 Lagrange 插值曲线")
    ap.add_argument("--gauss", action="store_true",
                    help="叠加正态拟合曲线（自动包含 Lagrange 曲线）")
    args = ap.parse_args()

    if args.gauss:
        args.lagrange = True

    hist_path = os.path.join(args.datadir, "histogram.dat")
    samp_path = os.path.join(args.datadir, "samples.dat")
    if args.output:
        out_path = args.output
    elif args.gauss:
        out_path = os.path.join(args.datadir, "step5_gauss.png")
    elif args.lagrange:
        out_path = os.path.join(args.datadir, "step4_lagrange.png")
    else:
        out_path = os.path.join(args.datadir, "step3_scatter.png")

    idx, lo, hi, center, count, expected = load_histogram(hist_path)
    xs = load_samples(samp_path)

    font = pick_cjk_font()
    if font:
        plt.rcParams["font.sans-serif"] = [font]
        plt.rcParams["font.family"] = "sans-serif"
    plt.rcParams["axes.unicode_minus"] = False

    fig, ax = plt.subplots(figsize=(7.2, 4.6), dpi=160)

    ax.scatter(center, count, s=52, color="#2c7fb8", edgecolor="white",
               linewidth=0.8, zorder=3, label=f"分箱计数（N={len(xs)}）")

    # 每个点旁边标出计数，避免只看高度估数
    for x, y in zip(center, count):
        ax.annotate(f"{int(y)}", (x, y), textcoords="offset points",
                    xytext=(0, 9), ha="center", fontsize=8,
                    color="#2c7fb8")

    ymin = 0.0
    ymax = count.max() * 1.18
    title = "N=1000 个样本在箱宽 0.1 下的统计"

    if args.lagrange:
        curve_path = os.path.join(args.datadir, "lagrange.dat")
        gx, gy = load_curve(curve_path)
        ax.plot(gx, gy, color="#e6913a", linewidth=1.6, zorder=2,
                label="9 次 Lagrange 插值")
        ymin = min(0.0, gy.min()) * 1.10
        ymax = max(count.max(), gy.max()) * 1.15
        title = "N=1000 个样本的分箱统计与 9 次 Lagrange 插值"
        ax.annotate(
            "插值节点区间 [0.05, 0.95]\n"
            "区间外外推：P(0)=1354，P(1)=−1631（未画出）",
            xy=(0.02, 0.96), xycoords="axes fraction",
            va="top", ha="left", fontsize=8,
            bbox=dict(boxstyle="round,pad=0.35", facecolor="white",
                      edgecolor="#cccccc", alpha=0.9))

    if args.gauss:
        gpath = os.path.join(args.datadir, "gauss_fit.dat")
        gx, gf = load_curve(gpath)
        par = load_params(os.path.join(args.datadir, "gauss_fit_params.dat"))
        gm = par[("primary", "m")]
        gs = par[("primary", "sigma")]
        ax.plot(gx, gf, color="#2e8b57", linewidth=1.9, zorder=4,
                label=f"正态拟合：m={gm:.4f}, σ={gs:.4f}")
        title = "分箱统计、9 次 Lagrange 插值与正态拟合"

    ax.set_xlim(-0.02, 1.02)
    ax.set_ylim(ymin, ymax)
    ax.set_xlabel("x（三个均匀随机数的平均值）")
    ax.set_ylabel("频数")
    ax.set_title(title)
    ax.grid(True, linestyle=":", linewidth=0.7, alpha=0.65)
    ax.set_axisbelow(True)
    ax.legend(loc="upper right", frameon=False, fontsize=9)

    fig.tight_layout()
    fig.savefig(out_path)
    if out_path.endswith(".png"):
        fig.savefig(out_path[:-4] + ".pdf")
    plt.close(fig)

    print(f"bins     = {len(idx)}")
    print(f"N        = {len(xs)}")
    print(f"counts   = {count.astype(int).tolist()}")
    print(f"sum      = {int(count.sum())}")
    print(f"x range  = [{xs.min():.6f}, {xs.max():.6f}]")
    print(f"wrote    = {out_path}")


if __name__ == "__main__":
    main()
