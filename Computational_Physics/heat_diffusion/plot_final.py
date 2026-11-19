#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
交付图。

题目要求的四张：
  (d) Dx=0.1 cm —— 两端 0 度、周期边界，各含 10/20/30 s 与完全收敛后
  (e) Dx=0.2 cm —— 同上再两张
附图：
  fig_dx_compare    —— 两种 Dx 的同图对照与逐点差（直接回答 (e)）
  fig_evolution     —— 完整 u(x,t) 时空图（英文版"all discrete points"）
  fig_scheme_compare—— FTCS 失稳与 CN 负温度
  fig_error_budget  —— 误差预算：格式误差 vs 初值表示误差

图上所有数字都从 data/*.dat 读，脚本本身不算物理。
"""

import argparse
import os

import matplotlib.pyplot as plt
import numpy as np

from plot import (load_columns, load_evolution, load_profiles, load_summary,
                  save, setup_style)

C0 = "#999999"     # 初值
C10 = "#2c7fb8"    # 10 s
C20 = "#e6913a"    # 20 s
C30 = "#2e8b57"    # 30 s
CCV = "#b2182b"    # 收敛后


def main_figure(datadir, dxname, bcname, out, dx_label, bc_label, target_label):
    x, cols, params, _ = load_profiles(datadir, dxname, bcname)
    xcm = x * 100.0
    u0 = cols["u_t0"]
    series = [(u0, C0, "初值 $u_0$（中间 1 cm 为 100 ℃）", "--", 1.6),
              (cols["u_t10"], C10, "t = 10 s", "-", 1.9),
              (cols["u_t20"], C20, "t = 20 s", "-", 1.9),
              (cols["u_t30"], C30, "t = 30 s", "-", 1.9),
              (cols["u_converged"], CCV, "完全收敛后", "-", 2.3)]

    fig, ax = plt.subplots(figsize=(7.4, 4.9), dpi=170)
    for y, color, label, style, lw in series:
        ax.plot(xcm, y, linestyle=style, linewidth=lw, color=color, label=label)
    ax.axhline(params["target"], color=CCV, linewidth=0.8, linestyle=":",
               alpha=0.7)

    ax.set_xlim(0.0, 10.0)
    ax.set_ylim(-2.0, 104.0)
    ax.set_xlabel("沿棍位置 x（cm）")
    ax.set_ylabel("温度 u（℃）")
    ax.set_title(f"{dx_label}，{bc_label}")
    ax.grid(True, linestyle=":", linewidth=0.7, alpha=0.6)
    ax.set_axisbelow(True)
    ax.legend(loc="upper right", frameon=False, fontsize=8.5)

    # Dirichlet 收敛后是 1e-6 量级，在主图上与横轴重合，用插图单独看
    if bcname == "dirichlet":
        ins = ax.inset_axes([0.11, 0.16, 0.34, 0.30])
        ins.plot(xcm, cols["u_converged"], color=CCV, linewidth=1.4)
        ins.set_ylim(0.0, params["eps"] * 1.6e6)
        ins.set_xlim(0.0, 10.0)
        ins.tick_params(labelsize=6)
        ins.set_title("收敛剖面放大", fontsize=7.5)
        ins.grid(True, linestyle=":", linewidth=0.5, alpha=0.5)

    ax.annotate(
        f"{dx_label}\nΔt = {params.get('dt', 0.01):.4f} s，终态 u∞ = "
        f"{target_label}\n"
        f"max u：10 s = {cols['u_t10'].max():.4f}，20 s = {cols['u_t20'].max():.4f}，"
        f"30 s = {cols['u_t30'].max():.4f}\n"
        f"收敛于 t = {params['t_conv']:.2f} s，"
        f"残差 {np.abs(cols['u_converged'] - params['target']).max():.2e} ℃"
        f"（判据 ε = {params['eps']:.0e}）",
        xy=(0.985, 0.03), xycoords="axes fraction",
        va="bottom", ha="right", fontsize=7.6,
        bbox=dict(boxstyle="round,pad=0.35", facecolor="white",
                  edgecolor="#cccccc", alpha=0.93))
    fig.tight_layout()
    save(fig, out)


def dx_compare_figure(datadir, out):
    fig, axes = plt.subplots(2, 2, figsize=(8.6, 6.2), dpi=170,
                             sharex=True,
                             gridspec_kw={"height_ratios": [2.2, 1.0]})
    times = [("u_t10", C10, "10 s"), ("u_t20", C20, "20 s"),
             ("u_t30", C30, "30 s")]
    for col, bcname, bc_label in ((0, "dirichlet", "两端 0 度（Dirichlet）"),
                                  (1, "periodic", "周期边界")):
        xf, cf, pf, _ = load_profiles(datadir, "fine", bcname)
        xc, cc, pc, _ = load_profiles(datadir, "coarse", bcname)
        ax = axes[0][col]
        for key, color, label in times:
            ax.plot(xf * 100, cf[key], color=color, linewidth=1.8,
                    label=f"{label}，Δx=0.1 cm")
            ax.plot(xc * 100, cc[key], color=color, linewidth=1.4,
                    linestyle="--", label=f"{label}，Δx=0.2 cm")
        ax.set_ylim(-2, 104)
        ax.set_title(bc_label, fontsize=10)
        ax.grid(True, linestyle=":", linewidth=0.7, alpha=0.6)
        ax.set_axisbelow(True)
        if col == 0:
            ax.set_ylabel("温度 u（℃）")
            ax.legend(loc="upper right", fontsize=6.6, ncol=2, frameon=False)

        # 逐点差：把粗网格插到细网格节点上再相减
        axd = axes[1][col]
        for key, color, label in times:
            diff = cc[key] - np.interp(xc, xf, cf[key])
            axd.plot(xc * 100, diff, color=color, linewidth=1.5, label=label)
        axd.axhline(0.0, color="#333333", linewidth=0.8)
        axd.set_xlim(0, 10)
        axd.set_xlabel("沿棍位置 x（cm）")
        axd.grid(True, linestyle=":", linewidth=0.7, alpha=0.6)
        axd.set_axisbelow(True)
        if col == 0:
            axd.set_ylabel("粗 − 细（℃）")
            mx = max(np.abs(cc[k] - np.interp(xc, xf, cf[k])).max()
                     for k, _, _ in times)
            axd.text(0.98, 0.90, f"最大 |粗−细| = {mx:.3f} ℃",
                     transform=axd.transAxes, ha="right", va="top", fontsize=7.5)
    fig.suptitle("Δx = 0.1 cm 与 0.2 cm 的同图对照（实线细网格、虚线粗网格）",
                 fontsize=11)
    fig.tight_layout(rect=(0, 0, 1, 0.96))
    save(fig, out)


def evolution_figure(datadir, out):
    fig, axes = plt.subplots(2, 2, figsize=(8.6, 6.4), dpi=170)
    norm_mesh = None
    nsl = 0
    for row, bcname, bc_label in ((0, "dirichlet", "两端 0 度（Dirichlet）"),
                                  (1, "periodic", "周期边界")):
        t, U, _ = load_evolution(datadir, "fine", bcname)
        nsl = U.shape[0]
        xcm = np.linspace(0.0, 10.0, U.shape[1])
        windows = ((0.0, 30.0, "0 ~ 30 s（题目要画的窗口）"),
                   (0.0, float(t.max()), f"0 ~ {t.max():.0f} s（到完全收敛）"))
        for col, (tlo, thi, ttl) in enumerate(windows):
            ax = axes[row][col]
            sel = (t >= tlo) & (t <= thi)
            norm_mesh = ax.pcolormesh(xcm, t[sel], U[sel], shading="nearest",
                                      cmap="viridis", vmin=0.0, vmax=100.0)
            ax.set_xlim(0.0, 10.0)
            # shading="nearest" 会把最后一格画到中心线外侧，留 2% 余量
            # 免得收敛那一片被 ylim 裁掉
            ax.set_ylim(tlo, thi * 1.02 if col == 1 else thi)
            ax.set_title(f"{bc_label}：{ttl}", fontsize=9)
            if col == 0:
                ax.set_ylabel("t（s）")
            if row == 1:
                ax.set_xlabel("沿棍位置 x（cm）")
        # 切片是几何级数排的：早期密、后期疏，右图能直接看出来
        axes[row][1].set_yscale("linear")

    fig.suptitle("完整时空演化 u(x, t)：每个离散点的温度随时间变化"
                 f"（Δx = 0.1 cm，共 {nsl} 个时间切片，早期密后期疏）",
                 fontsize=11)
    fig.tight_layout(rect=(0, 0.05, 0.91, 0.93))
    cax = fig.add_axes([0.925, 0.13, 0.015, 0.72])
    fig.colorbar(norm_mesh, cax=cax, label="u（℃）")
    save(fig, out)


def scheme_figure(datadir, out):
    x, stab, _, _ = load_columns(datadir, "stability")
    xc, over, _, _ = load_columns(datadir, "overshoot")
    xcm = x * 100.0

    fig, (axa, axb) = plt.subplots(1, 2, figsize=(9.0, 4.3), dpi=170)

    axa.plot(xcm, stab["ftcs_r0.23_t10s"], color=C10, linewidth=1.9,
             label="r = 0.23（稳定），t = 10 s")
    axa.plot(xcm, stab["ftcs_r0.69_step1"], color=C20, linewidth=1.4,
             label="r = 0.69，走 1 步")
    axa.plot(xcm, stab["ftcs_r0.69_step5"], color=C30, linewidth=1.4,
             label="r = 0.69，走 5 步")
    axa.axhline(0.0, color="#333333", linewidth=0.8)
    axa.set_ylim(-60, 120)
    big = np.abs(stab["ftcs_r0.69_step20"]).max()
    axa.annotate("r = 0.69 超过稳定限 1/2，每步放大 |1−4r| = 1.76。\n"
                 "走 20 步后 max|u| = %.2e，已画不进本图。\n"
                 "对比：r = 0.23 推到 10 s 仍是平滑剖面。" % big,
                 xy=(0.03, 0.03), xycoords="axes fraction",
                 va="bottom", fontsize=7.4,
                 bbox=dict(boxstyle="round,pad=0.3", facecolor="#fff5f5",
                           edgecolor="#cc8888"))
    axa.set_xlabel("x（cm）")
    axa.set_ylabel("u（℃）")
    axa.set_title("FTCS：r ≤ 1/2 是硬条件", fontsize=10)
    axa.grid(True, linestyle=":", alpha=0.6)
    axa.set_axisbelow(True)
    axa.legend(loc="upper right", fontsize=7.4, frameon=False)

    axb.plot(xcm, over["u_t0"], color=C0, linestyle="--", label="初值 $u_0$")
    for key, color, lab in (("cn_r0.23_t10s", C10, "r = 0.23"),
                           ("cn_r50_t10s", C20, "r = 50"),
                           ("cn_r500_t10s", CCV, "r = 500")):
        axb.plot(xcm, over[key], color=color, linewidth=1.6,
                 label=f"CN {lab}，t = 10 s")
    axb.axhline(0.0, color="#333333", linewidth=0.9)
    axb.set_xlabel("x（cm）")
    axb.set_title("Crank–Nicolson：稳定，但不保单调", fontsize=10)
    axb.grid(True, linestyle=":", alpha=0.6)
    axb.set_axisbelow(True)
    axb.legend(loc="upper right", fontsize=7.6, frameon=False)
    axb.annotate("r = 50 时 min u = %.2f ℃，r = 500 时 %.2f ℃。\n"
                 "右端矩阵对角权重 1−r 变负，前沿下方被打出\n"
                 "负温度——无条件稳定 ≠ 无条件物理合理。"
                 % (over["cn_r50_t10s"].min(), over["cn_r500_t10s"].min()),
                 xy=(0.03, 0.03), xycoords="axes fraction", va="bottom",
                 fontsize=7.4,
                 bbox=dict(boxstyle="round,pad=0.3", facecolor="#f2f7f2",
                           edgecolor="#88aa88"))
    fig.tight_layout()
    save(fig, out)


def error_figure(datadir, out):
    rows, _ = load_summary(datadir)
    dec = np.loadtxt(os.path.join(datadir, "decomposition.dat"))
    dxd, fmt, rep = dec[:, 0], dec[:, 1], dec[:, 2]

    fig, (axa, axb) = plt.subplots(1, 2, figsize=(9.0, 4.2), dpi=170)

    # 表示误差在 dx=0.002 时是 1e-14，对数轴画不出来：柱按下限画、
    # 数值照真实值写在标注里，不让图形替结论说话
    floor = 1e-6
    xpos = np.arange(len(dxd))
    axa.bar(xpos - 0.19, np.maximum(fmt, floor), width=0.36, color=C10,
            label="格式误差")
    axa.bar(xpos + 0.19, np.maximum(rep, floor), width=0.36, color=CCV,
            label="初值表示误差")
    axa.set_yscale("log")
    axa.set_ylim(8e-7, 3e-1)
    axa.set_xticks(xpos)
    axa.set_xticklabels([f"\u0394x={v:g}" for v in dxd], fontsize=8)
    axa.set_ylabel("t = 10 s 的最大误差（\u2103）")
    axa.set_title("\u603b\u8bef\u5dee\u7531\u8c01\u4e3b\u5bfc", fontsize=10)
    axa.grid(True, axis="y", linestyle=":", alpha=0.6)
    axa.set_axisbelow(True)
    axa.legend(fontsize=7.6, frameon=False, loc="upper right")
    share = 100.0 * rep[1] / (rep[1] + fmt[1])
    axa.annotate(
        "\u0394x=0.002\uff1a\u9ad8\u6e29\u6bb5\u8fb9\u754c\u6070\u597d\u843d\u5728\u5355\u5143\u8fb9\u754c\u4e0a\uff0c\n"
        "\u8868\u793a\u8bef\u5dee %.1e\uff08\u7b49\u4e8e\u96f6\uff0c\u67f1\u8d34\u4e0b\u9650\uff09\u3002\n"
        "\u0394x=0.001\uff1a\u8fb9\u754c\u843d\u5728\u8282\u70b9\u4e2d\u5fc3\uff0c\u591a\u51fa\u4e24\u4e2a 50 \u2103 \u534a\u683c\uff0c\n"
        "\u8868\u793a\u8bef\u5dee %.2e \u662f\u683c\u5f0f\u8bef\u5dee %.2e \u7684 %.1f \u500d\uff0c\n"
        "\u5360\u603b\u8bef\u5dee\u7ea6 %.0f%%\u3002\u6a2a\u8f74\u4e0b\u9650 1e-6 \u53ea\u662f\u753b\u56fe\u622a\u65ad\u3002"
        % (rep[0], rep[1], fmt[1], rep[1] / fmt[1], share),
        xy=(0.02, 0.60), xycoords="axes fraction", fontsize=6.8, va="top")

    labels = []
    for r in rows:
        tag = "\u7ec6" if r["dx"] < 0.0015 else "\u7c97"
        bc = "\u5468\u671f" if r["periodic"] > 0.5 else "Dirichlet"
        labels.append(f"{tag}\n{bc}")
    for key, color, lab in (("err_t10", C10, "10 s"),
                            ("err_t20", C20, "20 s"),
                            ("err_t30", C30, "30 s")):
        axb.plot(range(len(rows)), [r[key] for r in rows], "o-", color=color,
                 linewidth=1.6, markersize=6, label=lab)
    axb.set_yscale("log")
    axb.set_xticks(range(len(rows)))
    axb.set_xticklabels(labels, fontsize=7.4)
    axb.set_ylabel("\u6570\u503c\u89e3 vs \u89e3\u6790\u89e3\uff08\u2103\uff09")
    axb.set_title("\u56db\u4e2a\u4ea4\u4ed8\u7ec4\u5408\u7684\u8bef\u5dee", fontsize=10)
    axb.grid(True, linestyle=":", alpha=0.6)
    axb.set_axisbelow(True)
    axb.legend(fontsize=8, frameon=False)
    axb.annotate(
        "\u8bef\u5dee\u968f\u65f6\u95f4\u4e0b\u964d\uff1a\u6d3b\u7740\u7684\u9ad8\u9891\u6a21\u8d8a\u6765\u8d8a\u5c11\u3002\n"
        "\u0394x \u52a0\u500d\u540e\u8bef\u5dee\u7ea6\u5927 3.8 \u500d\uff0c\u7b26\u5408 dx\u00b2 \u6807\u5ea6\u3002",
        xy=(0.03, 0.04), xycoords="axes fraction", fontsize=7.2, va="bottom")
    fig.tight_layout()
    save(fig, out)

def main():
    ap = argparse.ArgumentParser(description="heat diffusion deliverable figures")
    ap.add_argument("datadir", nargs="?", default="data")
    ap.add_argument("-o", "--outdir", default=None)
    args = ap.parse_args()

    setup_style()
    outdir = args.outdir or args.datadir

    print("字体:", __import__("plot").pick_cjk_font())

    main_figure(args.datadir, "fine", "dirichlet",
                os.path.join(outdir, "fig_d1_dirichlet_dx01.png"),
                "Δx = 0.1 cm（题目 (d)）", "两端接触 0 度理想导热体", "0 ℃")
    main_figure(args.datadir, "fine", "periodic",
                os.path.join(outdir, "fig_d2_periodic_dx01.png"),
                "Δx = 0.1 cm（题目 (d)）", "周期边界", "10 ℃")
    main_figure(args.datadir, "coarse", "dirichlet",
                os.path.join(outdir, "fig_e1_dirichlet_dx02.png"),
                "Δx = 0.2 cm（题目 (e)）", "两端接触 0 度理想导热体", "0 ℃")
    main_figure(args.datadir, "coarse", "periodic",
                os.path.join(outdir, "fig_e2_periodic_dx02.png"),
                "Δx = 0.2 cm（题目 (e)）", "周期边界", "10 ℃")

    dx_compare_figure(args.datadir, os.path.join(outdir, "fig_dx_compare.png"))
    evolution_figure(args.datadir, os.path.join(outdir, "fig_evolution.png"))
    scheme_figure(args.datadir, os.path.join(outdir, "fig_scheme_compare.png"))
    error_figure(args.datadir, os.path.join(outdir, "fig_error_budget.png"))


if __name__ == "__main__":
    main()
