#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
最终交付图：分箱散点 + 9 次 Lagrange 插值 + 正态拟合 + 拟合残差

上面板回答题目第 3~5 步的作图要求，下面板给出高斯拟合的残差，
用来说明「拟合形状对，但残差有系统结构」。
"""

import argparse
import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib import font_manager

from plot import load_histogram, load_samples, load_curve, load_params, pick_cjk_font


def main():
    ap = argparse.ArgumentParser(description="final figure for random-mean statistics")
    ap.add_argument("datadir", nargs="?", default="data")
    ap.add_argument("-o", "--output", default=None)
    args = ap.parse_args()

    idx, lo, hi, center, count, expected = load_histogram(
        os.path.join(args.datadir, "histogram.dat"))
    xs = load_samples(os.path.join(args.datadir, "samples.dat"))
    lx, ly = load_curve(os.path.join(args.datadir, "lagrange.dat"))
    gx, gy = load_curve(os.path.join(args.datadir, "gauss_fit.dat"))
    par = load_params(os.path.join(args.datadir, "gauss_fit_params.dat"))

    m = par[("primary", "m")]
    sigma = par[("primary", "sigma")]
    amp = par[("primary", "A")]

    fitted = amp / (sigma * np.sqrt(2.0 * np.pi)) * \
        np.exp(-0.5 * ((center - m) / sigma) ** 2)
    resid = count - fitted

    out = args.output or os.path.join(args.datadir, "result.png")

    font = pick_cjk_font()
    if font:
        plt.rcParams["font.sans-serif"] = [font]
        plt.rcParams["font.family"] = "sans-serif"
    plt.rcParams["axes.unicode_minus"] = False

    fig, (ax, axr) = plt.subplots(
        2, 1, figsize=(7.6, 5.9), dpi=170, sharex=True,
        gridspec_kw={"height_ratios": [3.0, 1.0]})

    ax.scatter(center, count, s=54, color="#2c7fb8", edgecolor="white",
               linewidth=0.9, zorder=3, label=f"分箱计数（N={len(xs)}）")
    ax.plot(lx, ly, color="#e6913a", linewidth=1.5, zorder=2,
            label="9 次 Lagrange 插值")
    ax.plot(gx, gy, color="#2e8b57", linewidth=2.1, zorder=4,
            label=f"正态拟合：m={m:.4f}, σ={sigma:.4f}")

    ax.set_xlim(-0.02, 1.02)
    ax.set_ylim(-110, 270)
    ax.set_ylabel("频数")
    ax.set_title("N=1000 个 x̄ 的分箱统计：Lagrange 插值与正态拟合")
    ax.grid(True, linestyle=":", linewidth=0.7, alpha=0.65)
    ax.set_axisbelow(True)
    ax.legend(loc="upper right", frameon=False, fontsize=9)
    ax.annotate(
        "Lagrange：9 次，节点 [0.05, 0.95]\n"
        "区间外 P(0)=1354、P(1)=−1631，不作外推\n"
        f"高斯拟合幅度 A=N·Δx={amp:.0f}",
        xy=(0.02, 0.97), xycoords="axes fraction",
        va="top", ha="left", fontsize=8,
        bbox=dict(boxstyle="round,pad=0.35", facecolor="white",
                  edgecolor="#cccccc", alpha=0.92))

    axr.axhline(0.0, color="#555555", linewidth=0.9)
    axr.bar(center, resid, width=0.06, color="#b2182b", alpha=0.75,
            edgecolor="white", linewidth=0.6)
    axr.set_ylim(-22, 22)
    axr.set_xlabel("x̄（三个均匀随机数的平均值）")
    axr.set_ylabel("残差")
    axr.grid(True, linestyle=":", linewidth=0.7, alpha=0.6)
    axr.set_axisbelow(True)
    axr.text(0.99, 0.90, "观测 − 拟合", transform=axr.transAxes,
             ha="right", va="top", fontsize=8, color="#b2182b")

    fig.tight_layout()
    fig.savefig(out)
    if out.endswith(".png"):
        fig.savefig(out[:-4] + ".pdf")
    plt.close(fig)

    print(f"N = {len(xs)}, sum of counts = {int(count.sum())}")
    print(f"primary fit: m = {m:.6f}, sigma = {sigma:.6f}, A = {amp:.3f}")
    print(f"residual: min = {resid.min():+.3f}, max = {resid.max():+.3f}, "
          f"max|r| = {np.abs(resid).max():.3f}")
    print(f"wrote {out}")


if __name__ == "__main__":
    main()
