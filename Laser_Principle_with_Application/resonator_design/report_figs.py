# -*- coding: utf-8 -*-
"""
report_figs.py —— 谐振腔设计的全部出图（由 cavity.py 调用，不单独运行）。

    fig_cavity            新手版腔结构示意：腔长/曲率/焦点（task01/report.typ 用）
    fig_envelope          腔结构 + 基模光束包络（report.typ 用）
    fig_gdiagram          g-g 稳定图：等比例坐标 + 原点放大 inset
    fig_length_tolerance  稳定性与光斑随腔长的变化
    fig_misalignment      失调灵敏度
    fig_stability_demo    q 映射：稳定腔有界振荡、失稳腔指数发散
    fig_aperture          孔径截断损耗

figsize 按报告版心宽度反推、1:1 出图，图内写多大字号打印出来就是多大
（老版本的图在报告里会被缩到 0.7 倍，8 pt 字缩成 5.6 pt）。
图内不放标题——标题由 Typst 图注承担。
"""

import math
import os

import numpy as np
import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

plt.rcParams["font.family"] = ["LXGW WenKai", "DejaVu Sans", "sans-serif"]
plt.rcParams["axes.unicode_minus"] = False

from cavity_core import (
    axis_offset,
    clip_loss,
    eigen_q,
    g_params,
    map_iterations,
    mode_fields,
    rt_matrix,
    w_from_q,
)

HERE = os.path.dirname(os.path.abspath(__file__))
FIGDIR = os.path.join(HERE, "task01", "figures")
os.makedirs(FIGDIR, exist_ok=True)


# ------------------------------------------------------------------ 图
def fig_envelope(des, z, w):
    """腔结构与基模高斯光束包络（横向尺寸相对轴向放大约 400 倍，仅示意）。"""
    L = des["L"]
    fig, ax = plt.subplots(figsize=(7.4, 3.2))
    zm, wm = z * 1e3, w * 1e3
    ax.fill_between(zm, -wm, wm, color="#1f5c8b", alpha=0.18, lw=0)
    ax.plot(zm, wm, lw=1.5, color="#1f5c8b", label="基模束宽 $w(z)$")
    ax.plot(zm, -wm, lw=1.5, color="#1f5c8b")
    ax.plot(zm, 3 * wm, lw=0.8, ls=":", color="#1f5c8b", label="$3w(z)$（含 99.999998% 功率）")
    ax.plot(zm, -3 * wm, lw=0.8, ls=":", color="#1f5c8b")
    ax.axhline(0, color="k", lw=0.6)

    # 放电管（毛细 bore Ø2 mm）
    b0, b1 = des["bore_z_range"][0] * 1e3, des["bore_z_range"][1] * 1e3
    ax.add_patch(plt.Rectangle((b0, -1.0), b1 - b0, 2.0, fc="#e8e8e8", ec="#9a9a9a",
                               lw=0.8, alpha=0.75, zorder=0))
    ax.annotate(f"放电管 bore Ø{2 * des['bore_r'] * 1e3:.0f} mm", ((b0 + b1) / 2, -1.62),
                ha="center", fontsize=8, color="#555")

    # 两面镜（画成竖线，曲率在此尺度下半径仅 30 μm 量级，看不出弧）
    # 镜片参数全部由 des 传入，不写死数字。
    for x0, lab in ((0.0, f"M1 凹面 $R_1$={des['R1'] * 1e3:.0f} mm\nHR ≥99.9%"),
                    (L * 1e3, f"M2 凹面 $R_2$={des['R2'] * 1e3:.0f} mm\nOC $T$=1.5%")):
        ax.plot([x0, x0], [-2.15, 2.15], lw=2.6, color="#8b0000", solid_capstyle="butt")
        ax.annotate(lab, (x0, -2.4), ha="center", va="top", fontsize=8.2)
    zw = des["waist_from_M1"] * 1e3
    ax.axvline(zw, ls="--", lw=0.9, color="#444")
    ax.annotate(f"束腰 $w_0$={des['w0']*1e6:.1f} μm（距 M1 {zw:.0f} mm）",
                (zw + 8, 1.75), ha="left", va="bottom", fontsize=8.2, color="#444")
    ax.annotate(f"$w_1$={des['w1']*1e6:.1f} μm", (12, des["w1"] * 1e3 + 0.25), fontsize=8.2)
    ax.annotate(f"$w_2$={des['w2']*1e6:.1f} μm", (L * 1e3 - 12, des["w2"] * 1e3 + 0.25),
                fontsize=8.2, ha="right")
    ax.annotate(f"L = {L*1e3:.0f} mm", (L * 1e3 / 2, 3.15), ha="center", fontsize=9)
    ax.annotate("", xy=(0, 3.0), xytext=(L * 1e3, 3.0),
                arrowprops=dict(arrowstyle="<->", lw=0.8, color="#333"))
    ax.set_xlim(-75, L * 1e3 + 75)
    ax.set_ylim(-3.6, 3.9)
    ax.set_xlabel("轴向位置 z / mm")
    ax.set_ylabel("半径 / mm")
    ax.legend(loc="lower center", fontsize=8, framealpha=0.95)
    fig.tight_layout()
    fig.savefig(os.path.join(FIGDIR, "envelope.png"), dpi=190)
    plt.close(fig)


def fig_cavity(des):
    """
    新手版腔结构示意图：只画几何光学的三件事——腔长、曲率、焦点（不画光束、
    孔径、图例、图内标题，图注交给 Typst）。figsize 宽 5.3 in 对应
    task01/report.typ 里 92% 版心宽度，1:1 出图，字号 9 pt 打印就是 9 pt。
    镜面曲率是放大画的（真实矢高只有几十 μm），由图注说明。
    """
    L = des["L"] * 1e3
    R1, R2 = des["R1"] * 1e3, des["R2"] * 1e3
    f1, f2 = 0.5 * R1, L - 0.5 * R2      # 两个焦点（几何光学 F = R/2，自 M1 起算）
    c1, c2 = R1, L - R2                  # 两个曲率中心

    fig, ax = plt.subplots(figsize=(5.3, 2.4))
    ax.set_axis_off()
    ax.set_xlim(-170, 720)
    ax.set_ylim(-30, 26)

    # 腔轴
    ax.plot([-160, 710], [0, 0], color="0.35", lw=0.9, zorder=1)

    # 两面凹面镜：画成开口朝向腔内的弧。
    # k 取到 |y|=13 处矢高 30 个坐标单位——横纵坐标比例差好几倍，取小了画出来
    # 根本看不出弧度（真实矢高只有几十 μm，本来就是放大示意）。
    k = 30.0 / 13.0 ** 2
    ys = np.linspace(-13.0, 13.0, 80)
    ax.plot(k * ys ** 2, ys, color="#8b0000", lw=3.0, solid_capstyle="round", zorder=4)
    ax.plot(L - k * ys ** 2, ys, color="#8b0000", lw=3.0, solid_capstyle="round", zorder=4)

    # ① 腔长 L
    ax.annotate("", xy=(0, 16), xytext=(L, 16),
                arrowprops=dict(arrowstyle="<->", lw=1.0, color="#333"))
    ax.text(L / 2, 17.5, f"$L$ = {L:.0f} mm", ha="center", va="bottom", fontsize=9.5)

    # ② 曲率 R：尺寸线一直画到各自的曲率中心，箭头终点即 C1 / C2
    ax.annotate("", xy=(c1, -14), xytext=(0, -14),
                arrowprops=dict(arrowstyle="<->", lw=0.9, color="#555"))
    ax.text(c1 / 2, -12.5, f"$R_1$ = {R1:.0f} mm", ha="center", va="bottom", fontsize=8.5)
    ax.text(c1 + 9, -14, "$C_1$", ha="left", va="center", fontsize=8, color="#555")
    ax.annotate("", xy=(c2, -24), xytext=(L, -24),
                arrowprops=dict(arrowstyle="<->", lw=0.9, color="#555"))
    ax.text((c2 + L) / 2, -22.5, f"$R_2$ = {R2:.0f} mm", ha="center", va="bottom", fontsize=8.5)
    ax.text(c2 - 9, -24, "$C_2$", ha="right", va="center", fontsize=8, color="#555")

    # ③ 焦点 F = R/2：对称近共焦时两焦点几乎重合（R = L 时才严格重合）
    for zf in (f1, f2):
        ax.plot([zf], [0], marker="x", ms=8, mew=1.8, color="#1f5c8b", zorder=5)
    ax.text((f1 + f2) / 2, 11, "焦点（$F=R/2$）", ha="center", va="bottom",
            fontsize=8, color="#1f5c8b")
    ax.text(f1 + 13, 3.5, f"$F_1$={f1:.0f}", ha="left", va="bottom", fontsize=8.5, color="#1f5c8b")
    ax.text(f2 - 13, 3.5, f"$F_2$={f2:.0f}", ha="right", va="bottom", fontsize=8.5, color="#1f5c8b")

    # 镜的类型（任务要求写明）
    ax.text(-15, 9, "M1 高反", ha="right", va="bottom", fontsize=8.5)
    ax.text(L + 15, 9, "M2 输出耦合", ha="left", va="bottom", fontsize=8.5)

    fig.tight_layout()
    fig.savefig(os.path.join(FIGDIR, "cavity.png"), dpi=190)
    plt.close(fig)


def fig_gdiagram(results):
    """
    g-g 稳定图，按报告排版反推尺寸：figsize 宽 4.6 in，报告里也放 4.6 in，
    字号 8.5–9 pt 打印出来就是 8.5–9 pt，不再被缩成 6 pt 小字。
    改动点（相对旧版）：等比例坐标、设计点高亮、标签一律走引线防重叠、
    原点（共焦简并点）放大 inset、去掉图内大标题（标题由 Typst 图注承担）。
    """
    fig, ax = plt.subplots(figsize=(4.6, 4.5))
    gg = np.linspace(-1.6, 1.6, 700)
    G1, G2 = np.meshgrid(gg, gg)
    P = G1 * G2
    region = (P > 0) & (P < 1)
    ax.contourf(G1, G2, region.astype(float), levels=[0.5, 1.01], colors=["#cfe4f2"], extend="max")
    cx = ax.contour(G1, G2, P, levels=[0.05, 0.2, 0.5, 0.8], colors="#8f8f8f",
                    linewidths=0.7, linestyles=":")
    # 手工定位等值线标签：clabel 自动放会被画布边框切字（"g1g2=0.2"出框只剩半截）
    ax.clabel(cx, manual=[(0.07, 0.714), (0.20, 1.00), (0.63, 0.79), (1.10, 0.73),
                          (-1.00, -0.20), (-0.63, -0.79), (-1.10, -0.73)],
              fmt="$g_1g_2$=%g", fontsize=7.5, colors="#666666")
    ax.contour(G1, G2, P, levels=[0.0, 1.0], colors="#8b0000", linewidths=1.4)
    ax.set_facecolor("#fafafa")

    des = next(r for r in results if r["name"].startswith("本设计"))
    # 对照点标签：全部走引线，长短名折成两行压窄宽度；四个标签分居四片空区——
    # 本设计/严格共焦在左半，平面-平面在上、近共焦在下（避开右下角 inset）
    label_off = {
        "平面-平面": ((-16, 30), "right"),
        "近平面": ((-16, -32), "right"),
        "近同心": ((18, 6), "left"),
        "严格共焦": ((-70, -28), "left"),
        "近共焦": ((16, -24), "left"),
    }
    short = {"平面-平面": "平面-平面\n（临界）", "近平面": "近平面\n（R=10 m）",
             "近同心": "近同心（R=280 mm）", "严格共焦": "严格共焦\n（R=550 mm，临界）",
             "近共焦": "近共焦（R=560 mm）"}
    for r in results:
        is_des = r is des
        ax.plot(r["g1"], r["g2"], "o", ms=11 if is_des else 6.5,
                mfc="#1f5c8b" if is_des else "#8b0000",
                mec="white" if is_des else "none", mew=1.1, zorder=6)
        if is_des:
            ax.annotate(f"本设计\n$g_1g_2$={r['prod']:.4f}", (r["g1"], r["g2"]),
                        xytext=(-95, 40), textcoords="offset points", fontsize=9,
                        color="#1f5c8b", ha="left", zorder=7,
                        arrowprops=dict(arrowstyle="-", color="#1f5c8b", lw=0.9))
            continue
        key = next((k for k in label_off if k in r["name"]), None)
        if key is None:
            continue
        (dx, dy), ha = label_off[key]
        ax.annotate(short[key], (r["g1"], r["g2"]), xytext=(dx, dy),
                    textcoords="offset points", fontsize=8.5, zorder=7,
                    ha=ha, va="top" if dy < 0 else "bottom",
                    arrowprops=dict(arrowstyle="-", color="0.45", lw=0.8))

    # 原点放大：本设计、严格共焦、近共焦三个点全挤在原点附近，主图分不开，
    # 用 inset 看清"本设计在蓝色区内、但贴着共焦简并点"。
    # 位置压到右下角最里面，给"近共焦"等标签让出上方的横条空地。
    axins = ax.inset_axes([0.66, 0.03, 0.33, 0.30])
    sub = np.linspace(-0.04, 0.18, 300)
    SG1, SG2 = np.meshgrid(sub, sub)
    SP = SG1 * SG2
    axins.contourf(SG1, SG2, ((SP > 0) & (SP < 1)).astype(float),
                   levels=[0.5, 1.01], colors=["#cfe4f2"], extend="max")
    axins.set_facecolor("#fafafa")
    axins.contour(SG1, SG2, SP, levels=[0.0], colors="#8b0000", linewidths=1.2)
    for r in results:
        if -0.04 <= r["g1"] <= 0.18 and -0.04 <= r["g2"] <= 0.18:
            is_des = r is des
            axins.plot(r["g1"], r["g2"], "o", ms=9 if is_des else 5,
                       mfc="#1f5c8b" if is_des else "#8b0000",
                       mec="white" if is_des else "none", mew=1.0, zorder=6)
    axins.annotate("严格共焦 (0,0)", (0.0, 0.0), xytext=(7, 7),
                   textcoords="offset points", fontsize=7.5)
    axins.annotate("本设计", (des["g1"], des["g2"]), xytext=(7, -15),
                   textcoords="offset points", fontsize=7.5, color="#1f5c8b")
    axins.text(0.98, 0.96, "原点放大", transform=axins.transAxes, ha="right", va="top",
               fontsize=7.5, color="0.45")
    axins.set_xlim(-0.04, 0.18)
    axins.set_ylim(-0.04, 0.18)
    axins.set_aspect("equal")
    axins.set_xticks([])
    axins.set_yticks([])
    for s in axins.spines.values():
        s.set_color("0.5")
        s.set_linewidth(0.8)

    ax.set_xlabel("$g_1=1-L/R_1$", fontsize=10)
    ax.set_ylabel("$g_2=1-L/R_2$", fontsize=10)
    ax.set_xlim(-1.6, 1.6)
    ax.set_ylim(-1.6, 1.6)
    ax.set_aspect("equal")  # 不设这个，g-g 图的对角对称会被纵横比例压歪
    ax.tick_params(labelsize=8.5)
    ax.text(-1.52, 1.50, "蓝色 = 稳定区", fontsize=8.5, color="0.35",
            ha="left", va="top")
    fig.tight_layout()
    fig.savefig(os.path.join(FIGDIR, "g_diagram.png"), dpi=190)
    plt.close(fig)


def fig_length_tolerance(des):
    lam, R1, R2 = des["lam"], des["R1"], des["R2"]
    Ls = np.linspace(0.05, 1.3, 560)
    prod, w0s, w1s, w2s = [], [], [], []
    for L in Ls:
        g1, g2 = g_params(L, R1, R2)
        prod.append(g1 * g2)
        M = rt_matrix(L, R1, R2)
        q = eigen_q(M, lam)
        if q is None:
            w0s.append(np.nan)
            w1s.append(np.nan)
            w2s.append(np.nan)
            continue
        w0s.append(w_from_q(complex(0, q.imag), lam))
        z, w, _ = mode_fields(q, L, lam)
        w1s.append(w[0])
        w2s.append(w[-1])
    prod = np.array(prod)

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(8.6, 3.4))
    stable = (prod > 0) & (prod < 1)
    ax1.plot(Ls * 1e3, prod, lw=1.5, color="#1f5c8b")
    ax1.fill_between(Ls * 1e3, 0, 1, where=stable, color="#cfe4f2", alpha=0.6)
    ax1.axhline(0, color="#8b0000", lw=1.0)
    ax1.axhline(1, color="#8b0000", lw=1.0)
    ax1.axvline(des["L"] * 1e3, ls="--", color="#444", lw=0.9)
    ax1.plot([des["L"] * 1e3], [des["prod"]], "o", color="#8b0000", ms=6)
    ax1.annotate(f"$L$={des['L']*1e3:.0f} mm\n$g_1g_2$={des['prod']:.3f}",
                 (des["L"] * 1e3, des["prod"]), textcoords="offset points", xytext=(10, 18), fontsize=8.5)
    # L=R1 是 g1g2 触 0 的点：对称腔在两侧都仍稳定（相切型简并点），
    # 真正失稳的边界是 L=R1+R2（g1g2=1）与 L→0，两者要分开标。
    ax1.axvline(R1 * 1e3, color="#8b0000", ls=":", lw=0.9)
    ax1.annotate("$g_1g_2$=0（$L=R_1$，简并点）\n两侧仍稳定", (R1 * 1e3, 0.34),
                 fontsize=7.5, color="#8b0000", ha="center")
    ax1.axvline((R1 + R2) * 1e3, color="#8b0000", ls="--", lw=0.9)
    ax1.annotate("$g_1g_2$=1 失稳\n（$L=R_1+R_2$）", ((R1 + R2) * 1e3, 0.72),
                 fontsize=7.5, color="#8b0000", ha="right")
    ax1.set_xlabel("腔长 L / mm")
    ax1.set_ylabel("$g_1g_2$")
    ax1.set_ylim(-0.05, 1.05)
    ax1.set_title("稳定性随腔长的变化", fontsize=10.5)

    ax2.plot(Ls * 1e3, np.array(w1s) * 1e3, lw=1.4, label="$w_1$（M1）")
    ax2.plot(Ls * 1e3, np.array(w2s) * 1e3, lw=1.4, label="$w_2$（M2）")
    ax2.plot(Ls * 1e3, np.array(w0s) * 1e3, lw=1.4, ls="--", label="$w_0$（束腰）")
    ax2.axvline(des["L"] * 1e3, ls="--", color="#444", lw=0.9)
    ax2.axvline(R1 * 1e3, color="#8b0000", ls=":", lw=0.9)
    ax2.set_xlabel("腔长 L / mm")
    ax2.set_ylabel("光斑半径 / mm")
    ax2.set_ylim(0, 2.0)
    ax2.legend(fontsize=8.5)
    ax2.set_title("基模光斑尺寸随腔长的变化", fontsize=10.5)
    fig.tight_layout()
    fig.savefig(os.path.join(FIGDIR, "length_tolerance.png"), dpi=190)
    plt.close(fig)


def fig_misalignment(allres):
    fig, ax = plt.subplots(figsize=(6.6, 3.7))
    al = np.linspace(0, 400, 200) * 1e-6
    for j, r in enumerate(allres):
        ys = [max(abs(axis_offset(r["L"], r["R1"], r["R2"], 0.0, a)[0]),
                  abs(axis_offset(r["L"], r["R1"], r["R2"], 0.0, a)[1])) for a in al]
        ax.plot(al * 1e6, np.array(ys) * 1e3,
                lw=2.2 if j == 0 else 1.3, label=r["name"])
        # 1/4 光斑判据交点
        thr = min(r["w1"], r["w2"]) / 4.0
        ys_arr = np.array(ys)
        if ys_arr.max() > thr:
            idx = int(np.argmax(ys_arr > thr))
            ax.plot(al[idx] * 1e6, thr * 1e3, "x", ms=7, mew=1.6, color="k")
            dx, dy = {"本设计": (10, 7), "近共焦": (-38, -15),
                      "近平面": (8, 6), "近同心": (8, 6)}[
                          next(kk for kk in ("本设计", "近共焦", "近平面", "近同心")
                               if kk in r["name"])]
            ax.annotate(f"{al[idx]*1e6:.0f} μrad", (al[idx] * 1e6, thr * 1e3),
                        textcoords="offset points", xytext=(dx, dy), fontsize=8)
        ax.axhline(thr * 1e3, lw=0.6, ls=":", color="gray")
    ax.set_xlabel("M2 倾斜角 / μrad")
    ax.set_ylabel("谐振轴最大偏移 / mm")
    ax.set_yscale("log")
    ax.set_title("失调灵敏度：M2 倾斜引起的谐振轴偏移（虚线为各自 $w_{min}/4$ 容差线）", fontsize=10)
    ax.legend(fontsize=7.8)
    fig.tight_layout()
    fig.savefig(os.path.join(FIGDIR, "misalignment.png"), dpi=190)
    plt.close(fig)


def fig_stability_demo(des, others):
    """稳定腔里 q 映射有界振荡；一旦越过 g-g 边界，模斑按指数发散 -> 图里两条曲线对比。"""
    lam = des["lam"]
    L, R1, R2 = des["L"], des["R1"], des["R2"]
    M_st = rt_matrix(L, R1, R2)
    # 失稳对照：取稳定区间右端之外的一点（对称腔要越过 L=R1+R2 才失稳，
    # 不能想当然用"大一点的 L"）
    hi = des["L_stable_interval"][1]
    L_un = (hi * 1.15 + 0.05) if math.isfinite(hi) else 2.0 * L
    M_un = rt_matrix(L_un, R1, R2)
    g1u, g2u = g_params(L_un, R1, R2)
    p_un = g1u * g2u
    q_seed = complex(0.0, 0.4)
    n = 40
    ws_st = [w_from_q(q, lam) for q in map_iterations(M_st, lam, q_seed, n)]
    seq_un = map_iterations(M_un, lam, q_seed, n)
    ws_un = [w_from_q(q, lam) for q in seq_un] if seq_un else []
    # 失稳时迭代可能翻转成发散光束（Im q -> 0），逐次手动算
    if not ws_un:
        A, B, Cc, D = M_un[0, 0], M_un[0, 1], M_un[1, 0], M_un[1, 1]
        q = complex(q_seed)
        ws_un = []
        for _ in range(n):
            q = (A * q + B) / (Cc * q + D)
            im = (1.0 / q).imag
            ws_un.append(math.sqrt(-lam / (math.pi * im)) if im < 0 else np.nan)
    fig, ax = plt.subplots(figsize=(6.0, 3.4))
    ax.plot(range(1, len(ws_st) + 1), np.array(ws_st) * 1e3, "-o", ms=3, lw=1.2,
            color="#1f5c8b",
            label=f"本设计 $L$={L * 1e3:.0f} mm（$g_1g_2$={des['prod']:.4f}）")
    ax.plot(range(1, len(ws_un) + 1), np.array(ws_un) * 1e3, "-s", ms=3, lw=1.2,
            color="#8b0000",
            label=f"同一副镜取 $L$={L_un * 1e3:.0f} mm（$g_1g_2$={p_un:.2f}，失稳）")
    ax.set_yscale("log")
    ax.set_xlabel("往返次数")
    ax.set_ylabel("M1 面模斑半径 w / mm")
    ax.set_title("q 映射迭代：稳定腔模斑有界振荡，失稳腔指数发散", fontsize=10.5)
    ax.legend(fontsize=8.5)
    fig.tight_layout()
    fig.savefig(os.path.join(FIGDIR, "stability_demo.png"), dpi=190)
    plt.close(fig)
    return float(max(ws_st)), float(np.nanmax(ws_un)), float(L_un)


def fig_aperture(des):
    lam, L = des["lam"], des["L"]
    M = rt_matrix(L, des["R1"], des["R2"])
    q = eigen_q(M, lam)
    radii = np.linspace(0.4e-3, 8e-3, 200)
    w1, w2 = des["w1"], des["w2"]
    loss_m = np.array([2 * 0 + clip_loss(a, w1) + clip_loss(a, w2) for a in radii])
    loss_bore_w = clip_loss(radii, des.get("bore_w_max", 0.5e-3))
    fig, ax = plt.subplots(figsize=(6.2, 3.5))
    ax.plot(radii * 1e3, loss_m * 100, lw=1.6, color="#1f5c8b", label="两镜口径（往返）")
    ax.plot(radii * 1e3, 2 * loss_bore_w * 100, lw=1.6, color="#8b0000", ls="--",
            label=f"放电管孔阑（往返，$w$={des.get('bore_w_max',0)*1e3:.2f} mm）")
    ax.set_yscale("log")
    ax.set_ylim(1e-20, 1e2)
    ax.axvline(des["aperture_mir"] * 1e3, color="#444", ls=":", lw=0.9)
    ax.annotate(f"1 英寸镜\n半径 {des['aperture_mir']*1e3:.2f} mm",
                (des["aperture_mir"] * 1e3, 1e-6), fontsize=8, ha="center")
    ax.axvline(1.0, color="#8b0000", ls=":", lw=0.9)
    ax.annotate(f"bore a=1.0 mm：往返 {des['loss_bore_rt']:.1e}", (1.05, 3e-6),
                fontsize=8, color="#8b0000")
    ax.annotate("两镜 a=6.35 mm：往返 <10$^-{{20}}$（图上出界）", (3.1, 1e-17),
                fontsize=8, color="#1f5c8b")
    ax.set_xlabel("光阑/口径半径 a / mm")
    ax.set_ylabel("往返衍射（截断）损耗 / %")
    ax.set_title("孔径截断损耗 $\\delta\\approx e^{-2a^2/w^2}$", fontsize=10.5)
    ax.legend(fontsize=8.5)
    fig.tight_layout()
    fig.savefig(os.path.join(FIGDIR, "aperture_loss.png"), dpi=190)
    plt.close(fig)


