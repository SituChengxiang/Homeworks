#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy>=2.0", "matplotlib>=3.9", "scipy>=1.14"]
# ///
"""
Task 03：腔内基模的高斯光束描述、增益晶体位置与泵浦光斑匹配

腔型沿用 Task 02：R1 = R2 = 600 mm（双凹）、L = 550 mm、λ = 1064 nm、
有效通光口径 D = 1.6 mm。本脚本按作业的六条要求依次算：

  1 等价共焦腔：由 (R1, R2, L) 反解唯一等价的对称共焦腔 —— 共焦参数 f、
    等效腔长 Leq = 2f、等效镜半径 Req = 2f、两镜在等效坐标中的位置 z1、z2，
    并做 (R1,R2,L) <-> (f,d1,d2) 的正反演闭环自检，同时用 ABCD 本征 q 独立复算。
  2 束腰位置与尺寸。
  3 腔内高斯光束参数与整段包络：w(z)、等相位面曲率 R(z)、瑞利长度、发散角、
    古伊相移、两镜光斑、模体积。
  4 验证 Task 02：把 Task 02 的 Fox-Li 自再现模在腔内逐段自由传播，与高斯模型
    比光斑尺寸（二阶矩 / 1/e^2 穿越半径 / 核心区拟合）、横向剖面、往返损耗、往返相位。
  5 增益晶体（Nd:YAG 端泵）：位置与尺寸；晶体内光斑变化、口径裕度、
    插入晶体（平行平板）与热透镜对模式的实际影响、为什么放在束腰。
  6 泵浦匹配：泵浦束腰半径与位置，重叠因子
    xi = 4 wp^2 wl^2 / (wp^2 + wl^2)^2（含泵浦吸收加权平均）与聚焦光组。

结果写 results_task3.json / summary_task3.typ，图写 figures_task3/。

可复算
    uv run task3_gauss.py
    uv run task3_gauss.py --R1 600 --R2 600 --L 550 --lam 1064 --D 1.6 \
                          --dop 0.5 --Lc 10 --ratio 0.8
    uv run task3_gauss.py --quick          # 跳过第 4 项（Fox-Li 对比）

单位：CLI/报告用 mm·nm·um，函数内部一律 SI。
符号：凹面镜曲率半径取正（同 Task 01/02）；共焦参数 f = pi w0^2 / lambda
（数值上等于瑞利长度 zR），等价共焦腔满足 Leq = Req = 2f。
"""

import argparse
import json
import math
import os
import time

import numpy as np

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

plt.rcParams["font.family"] = ["LXGW WenKai", "DejaVu Sans", "sans-serif"]
plt.rcParams["axes.unicode_minus"] = False

import task2_foxli  # 复用 Task 02 的柱对称 Fresnel 传播算子（同目录）

C0 = 299_792_458.0
HERE = os.path.dirname(os.path.abspath(__file__))
FIGDIR = os.path.join(HERE, "figures_task3")
os.makedirs(FIGDIR, exist_ok=True)

# ------------------------------------------------------------------ Nd:YAG 物性
# Koechner, Solid-State Laser Engineering (6th ed.)；晶体厂商规格表
N_SITE_CM3 = 1.377e22     # Y3+ 格位密度：3 位/式单位 × rho(4.55)/M(596.8) × Na = 1.377e22 cm^-3
SIG_ABS = 4.2e-20         # 808 nm 吸收截面 cm^2（非偏振选择泵浦）
SIG_EM = 2.8e-19          # 1064 nm 发射截面 cm^2
TAU_S = 230e-6            # 上能级寿命 s
N_YAG = 1.82              # 1064 nm 折射率
K_YAG = 14.0              # 热导率 W/(m·K)
DN_DT = 7.3e-6            # 热光系数 1/K
A_TH = 7.5e-6             # 热膨胀系数 1/K

DEFAULTS = dict(R1=600.0, R2=600.0, L=550.0, lam=1064.0, D=1.6,
                dop=0.5, Lc=10.0, rod=3.0, lam_p=808.0, ratio=0.8,
                fiber=400.0, T=1.5, Pabs=0.4)


# ================================================================== ABCD
def free(d):
    return np.array([[1.0, d], [0.0, 1.0]])


def mirror(R):
    return np.array([[1.0, 0.0], [-2.0 / R, 1.0]])


def slab(t, n):
    """平行平面介质板：傍轴等效长度 t/n。"""
    return np.array([[1.0, t / n], [0.0, 1.0]])


def lens(f):
    return np.array([[1.0, 0.0], [-1.0 / f, 1.0]])


def round_trip(segments, R1, R2):
    """
    以"M1 反射后离镜"为参考面的往返矩阵。
    segments = [(自由段长度, 该段末端处的薄元件矩阵 或 None), ...]，自 M1 到 M2 排列。
    注意：平行平板写成 (0.0, slab(t, n))——等效长度 t/n 已含在矩阵里，自由段只填
    平板前后的空气间隔，否则这段长度会被重复计入。
    互易元件反向传播矩阵不变，只是次序相反。
    """
    fwd = []
    for d, el in segments:
        fwd.append(free(d))
        if el is not None:
            fwd.append(el)
    F = np.eye(2)
    for m in fwd:
        F = m @ F
    B = np.eye(2)
    for m in reversed(fwd):
        B = m @ B
    return mirror(R1) @ B @ mirror(R2) @ F


def eigen_q(M, lam):
    """解 C q^2 + (D-A) q - B = 0，取 Im(q) > 0 的根；非稳腔返回 None。"""
    A, B = M[0, 0], M[0, 1]
    C, D = M[1, 0], M[1, 1]
    disc = (D - A) ** 2 + 4.0 * B * C
    if disc >= 0.0:
        return None
    sq = math.sqrt(-disc)
    return next(q for q in (((A - D) + 1j * sq) / (2 * C), ((A - D) - 1j * sq) / (2 * C))
                if q.imag > 0)


def w_of_q(q, lam):
    return math.sqrt(-lam / (math.pi * (1.0 / q).imag))


def forward_matrix(segments):
    """M1 -> M2 的单程矩阵：每段先自由传播 d，再作用于该段末端的薄元件。"""
    F = np.eye(2)
    for d, el in segments:
        F = free(d) @ F
        if el is not None:
            F = el @ F
    return F


def q_map(F, q):
    """ABCD 定律：q -> (A q + B)/(C q + D)。"""
    return (F[0, 0] * q + F[0, 1]) / (F[1, 0] * q + F[1, 1])


def mode_from_segments(segments, R1, R2, L, lam):
    """含腔内元件时：往返矩阵 -> 本征 q -> 束腰/瑞利长度/两镜光斑/稳定性。"""
    M = round_trip(segments, R1, R2)
    tr = (M[0, 0] + M[1, 1]) / 2.0
    q = eigen_q(M, lam)
    out = dict(M=M, trace=tr, stable=bool(np.isfinite(tr) and abs(tr) < 1.0 and q is not None))
    if q is None:
        return out
    # M2 面的 q 必须用单程矩阵变换（只有真空自由传播才能直接加长度）
    q2 = q_map(forward_matrix(segments), q)
    out.update(q1=q, q2=q2, zR=q.imag, waist=-q.real,
               w0=math.sqrt(lam * q.imag / math.pi), w1=w_of_q(q, lam), w2=w_of_q(q2, lam),
               Rmode1=1.0 / (1.0 / q).real, Rmode2=1.0 / (1.0 / q2).real,
               opt_len=sum(d for d, el in segments) + sum(el[0, 1] for d, el in segments
                                                          if el is not None))
    return out


# ================================================================== 1~3 等价共焦腔与光束
def equiv_confocal(R1, R2, L, lam):
    """
    实际腔 (R1,R2,L) -> 唯一等价共焦腔。

    束腰到两镜距离 d1、d2（d1+d2=L）。镜面须与该高斯光束的等相位面重合：
        R_i = d_i + f^2/d_i   <=>   f^2 = d_i (R_i - d_i)
    两式相等 + d1+d2=L 解得
        d1 = L (R2 - L) / (R1 + R2 - 2L),  d2 = L (R1 - L) / (R1 + R2 - 2L)
    反演 R_i = d_i + f^2/d_i、L = d1+d2 立即还原 -> 一一对应。
    等价共焦腔自身：Leq = 2f、Req = 2f（共焦条件 Req = Leq）、两镜在 z = ±f，
    而 z = ±f 正是等相位面曲率取极小值 2f 的瑞利面 -> 等效腔镜 = 瑞利面。
    """
    den = R1 + R2 - 2.0 * L
    if den == 0.0:
        raise SystemExit("R1+R2=2L：严格共焦简并")
    d1, d2 = L * (R2 - L) / den, L * (R1 - L) / den
    f2 = d1 * (R1 - d1)
    if min(d1, d2, f2) <= 0.0:
        raise SystemExit(f"束腰不在腔内或非稳腔：d1={d1:.4e}, d2={d2:.4e}, f^2={f2:.4e}")
    f = math.sqrt(f2)
    w0 = math.sqrt(lam * f / math.pi)

    g1, g2 = 1.0 - L / R1, 1.0 - L / R2
    prod = g1 * g2
    if not 0.0 < prod < 1.0:
        raise SystemExit(f"该腔非稳定（g1 g2 = {prod:.4f}）")
    w1 = math.sqrt(lam * L / math.pi * math.sqrt(g2 / (g1 * (1.0 - prod))))
    w2 = math.sqrt(lam * L / math.pi * math.sqrt(g1 / (g2 * (1.0 - prod))))

    empty = mode_from_segments([(L, None)], R1, R2, L, lam)
    q1 = empty["q1"]
    chk = {
        "f2 两式相等": abs(d2 * (R2 - d2) - f2) / f2,
        "d1+d2=L": abs(d1 + d2 - L) / L,
        "镜面光斑 vs q 法": abs(q1.imag - f) / f,
        "束腰位置 vs q 法": abs(-q1.real - d1) / L,
        "w1 闭合公式": abs(empty["w1"] - w1) / w1,
        "w2 闭合公式": abs(empty["w2"] - w2) / w2,
        "反演 R1": abs((d1 + f2 / d1) - R1) / R1,
        "反演 R2": abs((d2 + f2 / d2) - R2) / R2,
    }
    for k, v in chk.items():
        assert v < 1e-9, f"等价共焦腔自检失败：{k} 残差 {v:.3e}"

    gouy_pass = math.atan(d1 / f) + math.atan(d2 / f)
    gouy_rt = 2.0 * math.acos(math.sqrt(prod))
    ec = dict(
        R1=R1, R2=R2, L=L, lam=lam, D=None,
        g1=g1, g2=g2, prod=prod, d1=d1, d2=d2, f=f, zR=f, w0=w0,
        Leq=2 * f, Req=2 * f, w1=w1, w2=w2, q1=q1,
        theta=lam / (math.pi * w0),
        gouy_pass=gouy_pass, gouy_rt=gouy_rt, gouy_diff=abs(2 * gouy_pass - gouy_rt),
        Rmin=2 * f, eq_out1=d1 - f, eq_out2=f - d2,
        Vmode=math.pi * w0 ** 2 / 2.0 * 2 * f,   # ±zR 内的模体积
        checks=chk, empty=empty,
    )
    return ec


def beam_along(ec, n=1200):
    z = np.linspace(0.0, ec["L"], n)
    s = z - ec["d1"]
    w = ec["w0"] * np.sqrt(1.0 + (s / ec["f"]) ** 2)
    with np.errstate(divide="ignore", invalid="ignore"):
        R = s * (1.0 + (ec["f"] / s) ** 2)
    return z, w, R


def w_model(ec, z):
    return ec["w0"] * np.sqrt(1.0 + ((np.asarray(z) - ec["d1"]) / ec["f"]) ** 2)


# ================================================================== 4 与 Task 02 对比
def moment_w(u, r, dr):
    I = np.abs(u) ** 2
    return math.sqrt(2.0 * np.sum(I * r ** 3 * dr) / np.sum(I * r * dr))


def edge_w(u, r, frac=math.exp(-2.0)):
    """强度降到峰值 e^-2（即振幅 e^-1，高斯半径 w 的定义处）的半径，对数线性插值。"""
    y = np.abs(u) ** 2
    y = y / y.max()
    idx = np.where(y < frac)[0]
    if idx.size == 0 or idx[0] == 0:
        return float("nan")
    i = idx[0]
    t = (y[i - 1] - frac) / (y[i - 1] - y[i])
    return float(r[i - 1] + t * (r[i] - r[i - 1]))


def fit_w(u, r, rmax):
    """核心区 ln I 对 r^2 线性拟合，斜率 = -2/w^2；返回 w 与最大残差。"""
    I = np.abs(u) ** 2
    sel = (r <= rmax) & (I > 1e-14 * I.max())
    p = np.polyfit(r[sel] ** 2, np.log(I[sel] / I[sel].max()), 1)
    wfit = math.sqrt(-2.0 / p[0])
    res = float(np.max(np.abs(np.log(I[sel] / I[sel].max()) + 2.0 * r[sel] ** 2 / wfit ** 2)))
    return wfit, res


def foxli_scan(a, R1, R2, L, lam, planes, n=1200, iters=3000):
    """Task 02 收敛场 + 它在各腔内截面上的三种"光斑半径"定义。"""
    res = task2_foxli.foxli(a, R1, R2, L, lam, n=n, iters=iters, tol=1e-9)
    r, dr, u = res["r"], res["dr"], res["u"]
    mom, edge, fit = [], [], []
    for d in planes:
        v = u if d == 0.0 else task2_foxli.prop_matrix(d, r, dr, lam) @ u
        mom.append(moment_w(v, r, dr))
        edge.append(edge_w(v, r))
        fit.append(fit_w(v, r, 0.45 * a)[0])
    return res, np.array(mom), np.array(edge), np.array(fit)


# ================================================================== 5 增益晶体
def crystal_design(ec, dop, Lc, rod, iris_D, T_oc, a_pump=None):
    L, lam, f, w0, zw = ec["L"], ec["lam"], ec["f"], ec["w0"], ec["d1"]
    R1, R2 = ec["R1"], ec["R2"]
    a_iris, a_rod = iris_D / 2.0, rod / 2.0

    # (a) 束腰平坦度：离开束腰 h 处的相对变化
    hscan = np.linspace(0.0, 30e-3, 601)
    rel = w_model(ec, zw + hscan) / w0 - 1.0
    rel_at = {}
    for hc in (2.5e-3, 5e-3, 7.5e-3, 10e-3, 15e-3, 20e-3):
        rel_at[hc] = float(rel[int(np.argmin(np.abs(hscan - hc)))])

    z_in, z_out = zw - Lc / 2.0, zw + Lc / 2.0
    w_in, w_out = float(w_model(ec, z_in)), float(w_model(ec, z_out))

    # (b) 同长晶体放在腔内别处
    pos = []
    for name, zc in (("腔心（=束腰）", zw), ("0.25 L", 0.25 * L), ("0.75 L", 0.75 * L),
                     ("0.05 L", 0.05 * L), ("0.95 L", 0.95 * L), ("0.025 L（近 M1）", 0.025 * L)):
        wm = float(w_model(ec, zc))
        wi = float(w_model(ec, zc - Lc / 2.0))
        wo = float(w_model(ec, zc + Lc / 2.0))
        pos.append(dict(name=name, zc=zc, w_mid=wm, dw=abs(wo - wi) / wi,
                        margin=a_iris / wm, loss=2.0 * math.exp(-2.0 * (a_iris / wm) ** 2)))

    # (c) 808 nm 泵浦吸收（alpha = sigma_abs * N_Nd；单位显式换算）
    N_nd = dop * 1e-2 * N_SITE_CM3                     # cm^-3
    alpha_cm = SIG_ABS * N_nd                          # cm^-1
    alpha_mm = alpha_cm / 10.0                         # mm^-1
    abs_frac = 1.0 - math.exp(-alpha_mm * (Lc * 1e3))

    # (d) 阈值增益核对：往返损耗 = OC 透过率 + 口径截断（Task 02 衍射损耗量级）
    loss_diff = 2.0 * math.exp(-2.0 * (a_iris / ec["w1"]) ** 2)
    rt_loss = T_oc * 1e-2 + loss_diff
    Lc_cm = Lc * 1.0e2                                 # cm
    g_th = rt_loss / (2.0 * Lc_cm)                     # cm^-1
    N_th = g_th / SIG_EM                               # cm^-3
    frac_inv = N_th / N_nd

    # (e) 插入晶体（平行平板）：自由段只填板前后的空气间隔，板厚含在矩阵 t/n 里
    ins = mode_from_segments([(z_in, None), (0.0, slab(Lc, N_YAG)), (L - z_out, None)],
                             R1, R2, L, lam)

    # (f) 热透镜：等效薄透镜放在晶体中心（= 束腰），零长度，不改变几何腔长
    therm = []
    for f_th in (math.inf, 20.0, 10.0, 5.0, 3.0, 2.0, 1.0, 0.5, 0.3, 0.2, 0.15, 0.10):
        m = mode_from_segments([(zw, lens(f_th)), (L - zw, None)], R1, R2, L, lam)
        m["f_th"] = f_th
        therm.append(m)
    bd = lens_boundaries(ec, zw)
    f_unst = bd["f_unst"]
    # 热透镜量级估计（Koechner 形式，端泵强吸收近似为线源，偏保守）：
    #   1/f_th ~ P_abs [dn/dT + alpha_th (n-1)] / (pi k_c w_p^2)
    # 失稳硬边界在 f_unst；按"设计焦距 = 2 f_unst"（安全系数 2）反解容许吸收功率。
    coeff = (DN_DT + A_TH * (N_YAG - 1.0)) / (math.pi * K_YAG * (a_pump or w_in) ** 2)
    P_abs_max = 1.0 / (2.0 * f_unst * coeff) if np.isfinite(f_unst) and f_unst > 0 else float("nan")
    # 热透镜工作点：把 1/f = coeff * P_abs 用在三个特征功率上
    P_deg = 1.0 / (coeff * bd["f_near"]) if bd["f_near"] else float("nan")     # 经过共焦简并档
    P_hard = 1.0 / (coeff * f_unst) if np.isfinite(f_unst) else float("nan")   # 共心侧硬失稳

    # 载入元件后仍须满足"波前贴合镜面"的自再现条件（独立校验 q 映射的实现）
    rows = [ins] + [t for t in therm if t.get("q2") is not None]
    rmatch = max(max(abs(abs(m["Rmode1"]) - R1) / R1, abs(abs(m["Rmode2"]) - R2) / R2)
                 for m in rows)

    return dict(
        zw=zw, z_in=z_in, z_out=z_out, w_in=w_in, w_out=w_out, dw=abs(w_out - w_in) / w_in,
        hscan=hscan, rel=rel, rel_at=rel_at, pos=pos,
        a_iris=a_iris, a_rod=a_rod, margin_iris=a_iris / w_in, margin_rod=a_rod / w_in,
        iris_loss=loss_diff, rt_loss=rt_loss, g_th=g_th, N_th=N_th, frac_inv=frac_inv,
        N_nd=N_nd, alpha_cm=alpha_cm, alpha_mm=alpha_mm, abs_depth=1.0 / alpha_mm,
        abs_frac=abs_frac, V_mode=math.pi * w0 ** 2 / 2.0 * Lc,
        ins=ins, therm=therm, bound=bd, f_conf=bd["f_conf"], f_unst=f_unst,
        f_near=bd["f_near"], prod_min=bd["prod_min"], tr_min=bd["tr_min"],
        P_abs_max=P_abs_max, coeff_th=coeff, f_th_at_1W=(1.0 / coeff if coeff else float("nan")),
        P_deg=P_deg, P_hard=P_hard, cross_conf=bd["cross_conf"], tr_min_fine=bd["tr_min_fine"],
        rmatch=rmatch,
    )


def lens_boundaries(ec, zw):
    """
    热透镜（放在束腰处）由弱到强扫描 1/f，记录
      f_conf —— tr/2 首次达到 -1（g1g2 = 0，共焦/平-平简并）的焦距，不越过则为 nan
      f_unst —— tr/2 首次达到 +1（g1g2 = 1，共心边界）的焦距
      f_near/tr_min/prod_min —— 全程最贴近共焦简并处的焦距、半迹与等效 g1g2
    本设计无透镜时 g1g2 只有 0.0069，正透镜先把 tr/2 推向 -1（共焦简并方向）。
    """
    L, R1, R2, lam = ec["L"], ec["R1"], ec["R2"], ec["lam"]

    def tr(f_th):
        return mode_from_segments([(zw, lens(f_th)), (L - zw, None)], R1, R2, L, lam)["trace"]

    out = {"f_conf": float("nan"), "f_unst": float("nan"),
           "f_near": float("nan"), "tr_min": float("nan"), "prod_min": float("nan")}
    fs = [float(x) for x in np.geomspace(5e3, 2e-3, 2000)]      # 由弱到强
    vals = [tr(x) for x in fs]
    k = int(np.argmin(vals))
    out["f_near"], out["tr_min"] = fs[k], vals[k]
    out["prod_min"] = 0.5 * (1.0 + vals[k])
    # 在极值附近细扫：确认是"相切于共焦简并"还是真的越过（对称腔+中心透镜通常只是相切）
    lo, hi = fs[k] / 1.05, fs[k] * 1.05
    fine = [tr(float(x)) for x in np.linspace(lo, hi, 2001)]
    out["cross_conf"] = bool(min(fine) < -1.0)
    out["tr_min_fine"] = float(min(fine))
    prev_f, prev_t = None, None
    for x, t in zip(fs, vals):
        if prev_t is not None:
            for key, target in (("f_conf", -1.0), ("f_unst", 1.0)):
                if math.isnan(out[key]) and (prev_t - target) * (t - target) <= 0.0:
                    a, b = prev_f, x
                    ta = prev_t
                    for _ in range(60):
                        mid = 0.5 * (a + b)
                        tm = tr(mid)
                        if (ta - target) * (tm - target) <= 0.0:
                            b = mid
                        else:
                            a, ta = mid, tm
                    out[key] = 0.5 * (a + b)
        prev_f, prev_t = x, t
    return out


# ================================================================== 6 泵浦匹配
def xi_of(wp, wl):
    """两归一化高斯强度分布的重叠因子（=1 当且仅当 wp = wl）。"""
    return 4.0 * wp ** 2 * wl ** 2 / (wp ** 2 + wl ** 2) ** 2


def pump_design(ec, cry, lam_p, ratio, fiber_um):
    w0, f, zw = ec["w0"], ec["f"], ec["d1"]
    Lc = cry["z_out"] - cry["z_in"]
    wp = ratio * w0
    zRp = math.pi * wp ** 2 / lam_p

    s = np.linspace(0.0, Lc, 401)
    wp_z = wp * np.sqrt(1.0 + (s / zRp) ** 2)                    # 泵浦束腰放在输入面
    wl_z = w_model(ec, zw - Lc / 2.0 + s)
    xi_z = xi_of(wp_z, wl_z)
    dep = np.exp(-cry["alpha_mm"] * (s * 1e3))                     # 剩余泵浦比例（s 换算成 mm）
    ds = s[1] - s[0]
    xi_bar = float(np.sum(xi_z * dep) * ds / max(np.sum(dep) * ds, 1e-30))
    V_pump = float(np.sum(math.pi * wp_z ** 2 / 2.0 * dep) * ds)
    V_mode = float(np.sum(math.pi * wl_z ** 2 / 2.0) * ds)

    band = {}
    rr = np.linspace(0.02, 4.0, 40000)
    curve = xi_of(rr, np.ones_like(rr))
    for tgt in (0.90, 0.95, 0.98):
        ok = rr[curve >= tgt]
        band[tgt] = (float(ok.min()), float(ok.max()))

    wp_M1 = wp * math.sqrt(1.0 + (cry["z_in"] / zRp) ** 2)
    theta_p = lam_p / (math.pi * wp)
    w_fib = fiber_um / 2.0 / math.sqrt(2.0) * 1e-6               # 阶跃模光纤近似的 1/e^2 半径
    beta = wp / w_fib
    # 聚焦光组：从常见球管目录里挑出倍率最接近所需 beta 的开伽利略望远系统
    catalog = [6.24, 7.5, 8, 9, 10, 12.5, 15, 16, 18, 20, 22.2, 25, 30, 35, 40, 45, 50, 60, 75, 80,
               100, 120, 150, 200]
    combos = []
    for a in catalog:
        for b in catalog:
            if b >= a:
                continue
            combos.append(dict(f1=a * 1e-3, f2=b * 1e-3, d=(a + b) * 1e-3, beta=a / b,
                               wp=a / b * w_fib))
    combos.sort(key=lambda c: (abs(c["beta"] - beta), c["d"]))
    return dict(wp=wp, ratio=ratio, zRp=zRp, wp_M1=wp_M1, theta_p=theta_p,
                wp_exit=float(wp_z[-1]), xi_in=float(xi_z[0]), xi_exit=float(xi_z[-1]),
                xi_bar=xi_bar, xi_min=float(xi_z.min()), V_pump=V_pump, V_mode=V_mode,
                band=band, s=s, wp_z=wp_z, wl_z=wl_z, xi_z=xi_z, dep=dep,
                w_fib=w_fib, beta=beta, combos=combos, pick=combos[0],
                I0_per_W=1.0 / (0.5 * math.pi * wp ** 2))


# ================================================================== 图
def _mirror_glyph(ax, zz, half_y, sag, color="#3a3a3a"):
    """镜形示意（挠度按几何放大若干倍，仅示意）：凹面朝向腔内。"""
    s = np.linspace(-1.0, 1.0, 50)
    lean = -1.0 if zz > 0 else 1.0          # 左镜向右凹、右镜向左凹
    ax.plot(zz + lean * sag * (1.0 - s ** 2), half_y * s, lw=2.6, color=color,
            solid_capstyle="round")


def fig_envelope(ec, cry, path):
    L, f, w0, zw, D = ec["L"], ec["f"], ec["w0"], ec["d1"], ec["D"]
    z, w, _ = beam_along(ec)
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(9.6, 3.9),
                                   gridspec_kw=dict(width_ratios=[1.65, 1.0]))
    zm, wm = z * 1e3, w * 1e6
    ax1.fill_between(zm, -wm, wm, color="#cfe3f2", alpha=0.9, label="基模高斯包络 $\\pm w(z)$")
    ax1.plot(zm, wm, lw=1.5, color="#1f5c8b")
    ax1.plot(zm, -wm, lw=1.5, color="#1f5c8b")
    ax1.axvspan((zw - f) * 1e3, (zw + f) * 1e3, color="#fff2cc", alpha=0.45,
                label="束腰两侧 $|\\Delta z|<z_R$（等效共焦腔内部）")
    for zq in ((zw - f) * 1e3, (zw + f) * 1e3):
        ax1.axvline(zq, color="#8a8a8a", ls=":", lw=1.0)
        ax1.annotate(f"$z_R$={f*1e3:.0f} mm", (zq, -560), fontsize=7.5, color="#666", ha="center")
    _mirror_glyph(ax1, 0.0, 430.0, 14.0)
    _mirror_glyph(ax1, L * 1e3, 430.0, 14.0)
    ax1.annotate(f"M1 $R_1$={ec['R1']*1e3:.0f} mm", (8, -480), fontsize=8, color="#333")
    ax1.annotate(f"M2（OC）$R_2$={ec['R2']*1e3:.0f} mm", (L * 1e3 - 8, -480), fontsize=8,
                 color="#333", ha="right")
    zi, zo = cry["z_in"] * 1e3, cry["z_out"] * 1e3
    ax1.axvspan(zi, zo, color="#a9dcb6", alpha=0.95, label=f"Nd:YAG {ec['dop_str']} 晶体（示意）")
    ax1.axvline(zw * 1e3, color="#8b0000", ls="--", lw=1.2)
    ax1.annotate(f"束腰 $w_0$={w0*1e6:.1f} μm\n腔心 {zw*1e3:.0f} mm",
                 (zw * 1e3, w0 * 1e6 + 150), fontsize=8.5, color="#8b0000", ha="center")
    ax1.annotate(f"$w_1$={ec['w1']*1e6:.1f} μm", (14, ec["w1"] * 1e6 + 40), fontsize=8, color="#1f5c8b")
    ax1.annotate(f"$w_2$={ec['w2']*1e6:.1f} μm", (L * 1e3 - 14, ec["w2"] * 1e6 + 40),
                 fontsize=8, color="#1f5c8b", ha="right")
    ax1.annotate("", xy=(zi, 330), xytext=(zo, 330),
                 arrowprops=dict(arrowstyle="<->", color="#1c5c2c", lw=1.0))
    ax1.text((zi + zo) / 2, 350, f"晶体 {ec['Lc_str']}", fontsize=8, ha="center", color="#1c5c2c")
    ax1.annotate(f"光阑 $D$={D*1e3:.1f} mm（{ec['iris_pos_str']}）", (L * 1e3 * 0.62, -300),
                 fontsize=8, color="#555")
    ax1.set_xlim(-58, L * 1e3 + 58)
    ax1.set_ylim(-620, 620)
    ax1.set_xlabel("$z$ / mm（自 M1）")
    ax1.set_ylabel("$w(z)$ / μm")
    ax1.set_title(f"腔内基模包络与束腰、晶体位置（$f$={f*1e3:.1f} mm）", fontsize=10)
    ax1.grid(alpha=0.25, lw=0.4)
    ax1.legend(fontsize=7.5, loc="upper center", ncol=2)

    ax2.axhline(0, color="#333", lw=0.8)
    for zq, lab in ((-f, "$z=-f$ 等效 M1"), (f, "$z=+f$ 等效 M2")):
        ax2.axvline(zq * 1e3, color="#8b0000", lw=1.5)
        ax2.annotate(lab, (zq * 1e3, 34), ha="center", fontsize=8, color="#8b0000")
    for zq, lab in ((-ec["d1"], "实际 M1"), (ec["d2"], "实际 M2")):
        ax2.axvline(zq * 1e3, color="#1f5c8b", ls="--", lw=1.6)
        ax2.annotate(f"{lab}\n$z={zq*1e3:+.1f}$ mm", (zq * 1e3, -58), ha="center",
                     fontsize=8, color="#1f5c8b")
    ax2.annotate("束腰 = 等效共焦腔中心", (0, 74), ha="center", fontsize=8.5)
    ax2.annotate("", xy=(-f * 1e3, 24), xytext=(f * 1e3, 24),
                 arrowprops=dict(arrowstyle="<->", color="#8b0000", lw=1.0))
    ax2.text(0, 44, f"$L_{{eq}}=R_{{eq}}=2f$={2*f*1e3:.1f} mm", ha="center", fontsize=8.5, color="#8b0000")
    ax2.annotate("", xy=(-ec["d1"] * 1e3, -100), xytext=(ec["d2"] * 1e3, -100),
                 arrowprops=dict(arrowstyle="<->", color="#1f5c8b", lw=1.0))
    ax2.text(0, -122, f"$L$={L*1e3:.0f} mm", ha="center", fontsize=8.5, color="#1f5c8b")
    span = 1.25 * max(f, ec["d1"], ec["d2"]) * 1e3
    ax2.set_xlim(-span, span)
    ax2.set_ylim(-140, 140)
    ax2.set_yticks([])
    ax2.set_xlabel("$z$ / mm（原点取在束腰 / 等效共焦腔中心）")
    ax2.set_title("等价共焦腔：两实际镜面在等效腔镜以内", fontsize=10)
    fig.tight_layout()
    fig.savefig(path, dpi=190)
    plt.close(fig)


def fig_wavefront(ec, path):
    z, w, R = beam_along(ec)
    L, f = ec["L"], ec["f"]
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(9.2, 3.5))
    ax1.plot(z * 1e3, R * 1e3, lw=1.7, color="#1f5c8b", label="光束波前曲率半径 $R(z)$")
    ax1.axhline(ec["R1"] * 1e3, ls="--", lw=1.1, color="#8b0000", label="镜曲率半径 $R_1=R_2$")
    ax1.plot([0, L * 1e3], [ec["R1"] * 1e3, ec["R2"] * 1e3], "o", ms=6.5, color="#8b0000")
    ax1.axvline(ec["d1"] * 1e3, color="#7a7a7a", ls=":", lw=1.0)
    ax1.annotate("束腰\n$R=\\infty$", (ec["d1"] * 1e3 + 4, 2900), fontsize=8, color="#555")
    ax1.annotate("两镜处 $R(z_i)=R_i$（自再现条件）", (6, 1500), fontsize=8.5, color="#8b0000")
    ax1.set_xlabel("$z$ / mm")
    ax1.set_ylabel("$R(z)$ / mm")
    ax1.set_ylim(-200, 3800)
    ax1.set_title("腔内波前曲率", fontsize=9.5)
    ax1.grid(alpha=0.25, lw=0.4)
    ax1.legend(fontsize=8, loc="upper right")

    s = np.linspace(-1.5 * f, 1.5 * f, 1200)
    ax2.plot(s * 1e3, w_model(ec, ec["d1"] + s) * 1e6, lw=1.7, color="#1f5c8b")
    ax2.axvspan(-ec["d1"] * 1e3, ec["d2"] * 1e3, color="#eef3f7")
    ax2.axvline(-f * 1e3, color="#8b0000", ls="-", lw=1.2)
    ax2.axvline(f * 1e3, color="#8b0000", ls="-", lw=1.2)
    ax2.plot([-f * 1e3, f * 1e3], [ec["w0"] * math.sqrt(2) * 1e6] * 2, "o", ms=5, color="#8b0000")
    ax2.annotate(f"瑞利面 $z=\\pm f$：$w=\\sqrt{{2}}w_0$={ec['w0']*math.sqrt(2)*1e6:.0f} μm，"
                 f"$R_{{min}}=2f$={2*f*1e3:.0f} mm", (0, ec["w0"] * math.sqrt(2) * 1e6 + 40),
                 ha="center", fontsize=8, color="#8b0000")
    ax2.annotate("腔内区间", (-20, 120), fontsize=8, color="#666")
    ax2.set_xlabel("离开束腰的距离 / mm")
    ax2.set_ylabel("$w$ / μm")
    ax2.set_title("束腰外侧：瑞利长度与最小波前曲率", fontsize=9.5)
    ax2.grid(alpha=0.25, lw=0.4)
    fig.tight_layout()
    fig.savefig(path, dpi=190)
    plt.close(fig)


def fig_verify(ec, planes, mom, edge, fit, res, cmp2, path):
    fig, axes = plt.subplots(1, 3, figsize=(9.8, 3.4))
    ax = axes[0]
    zm = np.asarray(planes) * 1e3
    zc, wc, _ = beam_along(ec, 1200)
    ax.plot(zc * 1e3, wc * 1e6, lw=1.7, color="#1f5c8b", label="Task 03 高斯模型 $w(z)$")
    ax.plot(zm, mom * 1e6, "o", ms=4.5, color="#c0504d", label="Task 02 Fox-Li 二阶矩")
    ax.plot(zm, edge * 1e6, "s", ms=4.0, mfc="none", color="#4f8a3c", label="Fox-Li $1/e^2$ 穿越半径")
    ax.plot(zm, fit * 1e6, "^", ms=4.0, mfc="none", color="#8064a2", label="Fox-Li 核心区拟合")
    ax.set_xlabel("$z$ / mm")
    ax.set_ylabel("光斑半径 / μm")
    ax.set_title("沿腔光斑尺寸：两套方法", fontsize=9.5)
    ax.grid(alpha=0.25, lw=0.4)
    ax.legend(fontsize=6.8, loc="upper center")

    ax = axes[1]
    dev = (mom - w_model(ec, planes)) / w_model(ec, planes) * 100.0
    ax.plot(zm, dev, "o-", ms=4, lw=1.2, color="#8b0000")
    ax.axhline(0, color="#333", lw=0.6)
    ax.set_xlabel("$z$ / mm")
    ax.set_ylabel("二阶矩相对偏差 / %")
    ax.set_title("Fox-Li 比高斯模型略大（口径截断）", fontsize=9.5)
    ax.grid(alpha=0.25, lw=0.4)
    ax.annotate(f"镜面 {dev[0]:+.2f}%\n最大 {dev.max():+.2f}% @ {zm[int(np.argmax(dev))]:.0f} mm",
                (0.30, 0.12), xycoords="axes fraction", fontsize=8, color="#8b0000")

    ax = axes[2]
    r = res["r"] * 1e6
    I = np.abs(res["u"]) ** 2
    I = I / I.max()
    ax.plot(r, I, lw=1.7, color="#1f5c8b", label="Fox-Li 自再现模（M1 面）")
    ax.plot(r, np.exp(-2 * (r / (ec["w1"] * 1e6)) ** 2), "--", lw=1.3, color="#8b0000",
            label=f"高斯 $w_1$={ec['w1']*1e6:.1f} μm")
    ax.axvline(ec["D"] * 1e6 / 2, color="#555", ls=":", lw=1.0)
    ax.annotate(f"口径 $a$={ec['D']*1e6/2:.0f} μm", (ec["D"] * 1e6 / 2 + 8, 2e-5), fontsize=7.5, color="#555")
    ax.set_yscale("log")
    ax.set_ylim(1e-6, 1.4)
    ax.set_xlim(0, 900)
    ax.set_xlabel("$r$ / μm")
    ax.set_ylabel("$I(r)/I_0$")
    ax.set_title("镜面横向强度分布", fontsize=9.5)
    ax.grid(alpha=0.25, which="both", lw=0.4)
    ax.legend(fontsize=7)
    fig.tight_layout()
    fig.savefig(path, dpi=190)
    plt.close(fig)


def fig_crystal(ec, cry, path):
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(9.4, 3.6))
    ax1.plot(cry["hscan"] * 1e3, cry["rel"] * 100, lw=1.7, color="#1f5c8b")
    for hc, c in ((7.5e-3, "#c0504d"), (15e-3, "#4f8a3c")):
        ax1.axvline(hc * 1e3, color=c, ls=":", lw=1.0)
        ax1.annotate(f"{2*hc*1e3:.0f} mm 晶体\n$\\Delta w/w_0$={cry['rel_at'][hc]*100:.3f}%",
                     (hc * 1e3 + 0.6, cry["rel_at"][hc] * 100 + 0.12), fontsize=8, color=c)
    ax1.axhline(1.0, color="#8a8a8a", ls="--", lw=0.8)
    ax1.annotate("1%", (26.5, 1.06), fontsize=8, color="#8a8a8a")
    ax1.set_xlabel("离开束腰的距离 / mm")
    ax1.set_ylabel("$w(z)/w_0-1$ / %")
    ax1.set_title("束腰处光斑尺寸几乎不随 $z$ 变", fontsize=9.5)
    ax1.set_xlim(0, 30)
    ax1.grid(alpha=0.25, lw=0.4)

    L = ec["L"]
    zz = np.linspace(0.0, L, 700)
    ax2.plot(zz * 1e3, w_model(ec, zz) * 1e6, lw=1.7, color="#333", label="$w(z)$")
    for name, c in (("腔心（=束腰）", "#1c5c2c"), ("0.05 L", "#c0504d"), ("0.95 L", "#4f8a3c")):
        row = next(r for r in cry["pos"] if r["name"] == name)
        zc = row["zc"]
        Lc = cry["z_out"] - cry["z_in"]
        ax2.axvspan((zc - Lc / 2) * 1e3, (zc + Lc / 2) * 1e3, color=c, alpha=0.25,
                    label=f"{name}：$w$={row['w_mid']*1e6:.0f} μm，晶体内 $\\Delta w$={row['dw']*100:.2f}%")
    ax2.set_xlabel("$z$ / mm")
    ax2.set_ylabel("$w$ / μm")
    ax2.set_title("同长晶体放在腔内不同位置", fontsize=9.5)
    ax2.set_ylim(300, 470)
    ax2.grid(alpha=0.25, lw=0.4)
    ax2.legend(fontsize=6.8, loc="upper center")
    fig.tight_layout()
    fig.savefig(path, dpi=190)
    plt.close(fig)


def fig_pump(pmp, path):
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(9.4, 3.6))
    rr = np.linspace(0.05, 2.5, 600)
    ax1.plot(rr, xi_of(rr, np.ones_like(rr)), lw=1.7, color="#1f5c8b")
    for tgt, c in ((0.98, "#1f5c8b"), (0.95, "#c0504d"), (0.90, "#4f8a3c")):
        lo, hi = pmp["band"][tgt]
        ax1.axhline(tgt, color=c, ls="--", lw=0.7)
        ax1.axvline(lo, color=c, ls=":", lw=0.8)
        ax1.axvline(hi, color=c, ls=":", lw=0.8)
    ax1.axvline(1.0, color="#8b0000", lw=1.1)
    ax1.annotate("课件判据\n$w_p\\leq w_0$", (1.03, 0.42), fontsize=8, color="#8b0000")
    ax1.plot([pmp["ratio"]], [pmp["xi_in"]], "o", ms=6.5, color="#8b0000")
    ax1.annotate(f"设计点 $w_p/w_0$={pmp['ratio']:.1f}\n$\\xi$={pmp['xi_in']:.3f}",
                 (pmp["ratio"] + 0.07, pmp["xi_in"] - 0.16), fontsize=8, color="#8b0000")
    ax1.set_xlabel("泵浦束腰 / 腔模束腰  $w_p/w_0$")
    ax1.set_ylabel("重叠因子 $\\xi$")
    ax1.set_title("空间匹配判据与允许带", fontsize=9.5)
    ax1.set_ylim(0, 1.03)
    ax1.grid(alpha=0.25, lw=0.4)

    s = pmp["s"] * 1e3
    ax2.fill_between(s, pmp["wp_z"] * 1e6, pmp["wl_z"] * 1e6, color="#f6dede", alpha=0.7)
    ax2.plot(s, pmp["wp_z"] * 1e6, lw=1.7, color="#c0504d", label="泵浦 $w_p(z)$ @808 nm")
    ax2.plot(s, pmp["wl_z"] * 1e6, lw=1.7, color="#1f5c8b", label="腔模 $w_l(z)$ @1064 nm")
    ax2.set_xlabel("晶体内深度 $s$ / mm（0=输入面，泵浦束腰在此）")
    ax2.set_ylabel("光斑半径 / μm")
    ax2.set_title("晶体内的泵浦光与腔模", fontsize=9.5)
    ax2.grid(alpha=0.25, lw=0.4)
    ax3 = ax2.twinx()
    ax3.plot(s, pmp["dep"], lw=1.2, ls="--", color="#4f8a3c")
    ax3.set_ylabel("剩余泵浦比例")
    ax3.set_ylim(0, 1.05)
    ax2.annotate(f"$\\xi$：{pmp['xi_in']:.3f}→{pmp['xi_exit']:.3f}，吸收加权 {pmp['xi_bar']:.3f}",
                 (0.03, 0.17), xycoords="axes fraction", fontsize=8, color="#333")
    ax2.legend(fontsize=7.5, loc="upper right")
    fig.tight_layout()
    fig.savefig(path, dpi=190)
    plt.close(fig)


# ================================================================== 输出
def write_typ(path, Q, tables):
    lines = ["// 由 task3_gauss.py 自动生成，请勿手改。", "#let Q = ("]
    for k, v in Q.items():
        lines.append(f'  {k}: "{v}",')
    lines.append(")")
    for name, rows in tables:
        body = ", ".join("(" + ", ".join(f'"{x}"' for x in row) + ")" for row in rows)
        lines.append(f"#let {name} = ({body})")
    with open(path, "w", encoding="utf-8") as fp:
        fp.write("\n".join(lines) + "\n")


def f2(v, nd=2):
    return f"{v:.{nd}f}"


def es(v, nd=2):
    return f"{v:.{nd}e}"


def build_Q(args, ec, cry, pmp, cmp2):
    Q = dict(
        R1=f2(args.R1, 1), R2=f2(args.R2, 1), L=f2(args.L, 1), lam=f2(args.lam, 0), D=f2(args.D, 2),
        g1=f2(ec["g1"], 4), g2=f2(ec["g2"], 4), prod=f2(ec["prod"], 4),
        fnum=f2(ec["f"] * 1e3), Leq=f2(ec["Leq"] * 1e3), Req=f2(ec["Req"] * 1e3, 1),
        d1=f2(ec["d1"] * 1e3), d2=f2(ec["d2"] * 1e3),
        z1=f2(-ec["d1"] * 1e3), z2=f2(ec["d2"] * 1e3),
        eqout=f2(-ec["eq_out1"] * 1e3), Rmin=f2(ec["Rmin"] * 1e3, 1),
        dleq=f2((ec["Leq"] - ec["L"]) * 1e3, 1), fminusL2=f2((ec["f"] - ec["L"] / 2.0) * 1e3, 2),
        fiber=f2(args.fiber, 0),
        chkmax=es(max(ec["checks"].values()), 1), gouydiff=es(ec["gouy_diff"], 1),
        w0=f2(ec["w0"] * 1e6), zR=f2(ec["zR"] * 1e3),
        w1=f2(ec["w1"] * 1e6), w2=f2(ec["w2"] * 1e6),
        w0eq=f2(ec["w0"] * 1e6), wR=f2(ec["w0"] * math.sqrt(2) * 1e6, 1),
        rw1=f2(ec["w1"] / ec["w0"], 3),
        th1=f2(ec["theta"] * 1e3, 3), thf=f2(2 * ec["theta"] * 1e3, 3),
        gouy1=f2(ec["gouy_pass"], 4), gouyrt=f2(ec["gouy_rt"], 4),
        fsr=f2(C0 / (2 * ec["L"]) / 1e6, 2), tmr=f2(math.acos(math.sqrt(ec["prod"])) / math.pi, 4),
        Vmode=f2(cry["V_mode"] * 1e9, 3), V2zR=f2(ec["Vmode"] * 1e9, 2),
        dop=f2(args.dop, 2), Lc=f2(args.Lc, 0), rod=f2(args.rod, 0),
        zin=f2(cry["z_in"] * 1e3), zout=f2(cry["z_out"] * 1e3),
        win=f2(cry["w_in"] * 1e6), wout=f2(cry["w_out"] * 1e6),
        dwcry=f2(cry["dw"] * 100, 3),
        dw5=f2(cry["rel_at"][5e-3] * 100, 3), dw7=f2(cry["rel_at"][7.5e-3] * 100, 3),
        dw10=f2(cry["rel_at"][10e-3] * 100, 3), dw15=f2(cry["rel_at"][15e-3] * 100, 3),
        dw20=f2(cry["rel_at"][20e-3] * 100, 3),
        mir=f2(cry["margin_iris"], 2), mrod=f2(cry["margin_rod"], 1),
        irisloss=es(cry["iris_loss"], 1), rtloss=f2(cry["rt_loss"] * 100, 2),
        gth=f2(cry["g_th"], 4), Nth=es(cry["N_th"], 2), finv=es(cry["frac_inv"], 2),
        finvpct=f2(cry["frac_inv"] * 100, 3),
        alpha=f2(cry["alpha_cm"], 2), adep=f2(cry["abs_depth"], 2), abst=f2(cry["abs_frac"] * 100, 1),
        insdf=f2((cry["ins"]["zR"] / ec["f"] - 1) * 100, 2),
        insdw0=f2((cry["ins"]["w0"] / ec["w0"] - 1) * 100, 2),
        insdw1=f2((cry["ins"]["w1"] / ec["w1"] - 1) * 100, 2),
        insLeq=f2(cry["ins"]["opt_len"] * 1e3, 1),
        insd1=f2((cry["ins"]["waist"] - ec["d1"]) * 1e3, 4),
        insw0=f2(cry["ins"]["w0"] * 1e6, 1), insf=f2(cry["ins"]["zR"] * 1e3, 1),
        insw1=f2(cry["ins"]["w1"] * 1e6, 1),
        funst=f2(cry["f_unst"] * 1e3, 0), fnear=f2(cry["f_near"] * 1e3, 0),
        trmin=f2(cry["tr_min_fine"], 7), prodmin=es(cry["prod_min"], 1),
        crossconf=("越过" if cry["cross_conf"] else "相切不越过"),
        pabs=f2(cry["P_abs_max"], 2), fth1w=f2(cry["f_th_at_1W"] * 1e3, 0),
        rmatch=es(cry["rmatch"], 1),
        coefth=f2(cry["coeff_th"], 2), pabsdes=f2(args.Pabs, 2),
        fthdes=f2(1.0 / (cry["coeff_th"] * args.Pabs) * 1e3, 0),
        pdeg=f2(cry["P_deg"], 2), phard=f2(cry["P_hard"], 2),
        posfarw=f2(cry["pos"][-1]["w_mid"] * 1e6, 1),
        posfarinc=f2((cry["pos"][-1]["w_mid"] / ec["w0"] - 1) * 100, 0),
        posfardw=f2(cry["pos"][-1]["dw"] * 100, 2),
        posfarmar=f2(cry["pos"][-1]["margin"], 2),
        posnarmar=f2(cry["margin_iris"] / cry["pos"][-1]["margin"], 2),
        posfarloss=es(cry["pos"][-1]["loss"], 1),
        heatfrac=f2(1.0 - args.lam_p / args.lam, 3),
        coefthq=f2(cry["coeff_th"] * (1.0 - args.lam_p / args.lam), 2),
        pdegq=f2(cry["P_deg"] / (1.0 - args.lam_p / args.lam), 2),
        phardq=f2(cry["P_hard"] / (1.0 - args.lam_p / args.lam), 2),
        lamp=f2(args.lam_p, 0), wp=f2(pmp["wp"] * 1e6, 1), ratio=f2(args.ratio, 2),
        zRp=f2(pmp["zRp"] * 1e3, 0), wpex=f2(pmp["wp_exit"] * 1e6, 1),
        wpm1=f2(pmp["wp_M1"] * 1e6, 1), thp=f2(pmp["theta_p"] * 1e3, 3),
        xiin=f2(pmp["xi_in"], 4), xiex=f2(pmp["xi_exit"], 4),
        xloss=f2((1.0 - pmp["xi_in"]) * 100, 1),
        xibar=f2(pmp["xi_bar"], 4), ximin=f2(pmp["xi_min"], 4),
        b90lo=f2(pmp["band"][0.90][0], 3), b90hi=f2(pmp["band"][0.90][1], 3),
        b95lo=f2(pmp["band"][0.95][0], 3), b95hi=f2(pmp["band"][0.95][1], 3),
        b98lo=f2(pmp["band"][0.98][0], 3), b98hi=f2(pmp["band"][0.98][1], 3),
        beta=f2(pmp["beta"], 3), wfib=f2(pmp["w_fib"] * 1e6, 1),
        dwp=f2(2 * pmp["wp"] * 1e3, 2), zratio=f2(ec["zR"] / (args.Lc), 0),
        dmode=f2(2 * ec["w0"] * 1e3, 2), airis=f2(args.D / 2 * 1e3, 0), arod=f2(args.rod / 2 * 1e3, 0),
        f1=f2(pmp["pick"]["f1"] * 1e3, 0), f2n=f2(pmp["pick"]["f2"] * 1e3, 1),
        lensd=f2(pmp["pick"]["d"] * 1e3, 1), lensbeta=f2(pmp["pick"]["beta"], 2),
        lenswp=f2(pmp["pick"]["wp"] * 1e6, 0),
        I1W=f2(pmp["I0_per_W"] * 1e-4, 0),
        Vpump=f2(pmp["V_pump"] * 1e9, 3), Vmode_c=f2(pmp["V_mode"] * 1e9, 3),
    )
    tables = [
        ("FLAT", [(f2(2 * hc * 1e3, 0), f2(hc * 1e3, 1), f2(cry["rel_at"][hc] * 100, 3),
                   f2(ec["w0"] * (1 + cry["rel_at"][hc]) * 1e6, 1))
                  for hc in (2.5e-3, 5e-3, 7.5e-3, 10e-3, 15e-3, 20e-3)]),
        ("POS", [(r["name"], f2(r["zc"] * 1e3, 1), f2(r["w_mid"] * 1e6, 1),
                  f2(r["dw"] * 100, 3), f2(r["margin"], 2), es(r["loss"], 1))
                 for r in cry["pos"]]),
        ("THERM", [(
            "无透镜" if math.isinf(m["f_th"]) else f2(m["f_th"] * 1e3, 0),
            f2(m["trace"], 4),
            "—" if not m["stable"] else f2(m["w0"] * 1e6, 1),
            "—" if not m["stable"] else f2(m["zR"] * 1e3, 1),
            "—" if not m["stable"] else f2(m["waist"] * 1e3, 1),
            "—" if not m["stable"] else f2(m["w1"] * 1e6, 1),
            "—" if not m["stable"] else f2(m["w2"] * 1e6, 1),
            "稳定" if m["stable"] else "失稳") for m in cry["therm"]]),
        ("PUMP", [(f2(r, 2), f2(r * ec["w0"] * 1e6, 1), f2(xi_of(r * ec["w0"], ec["w0"]), 4),
                   f2((1.0 - xi_of(r * ec["w0"], ec["w0"])) * 100, 1))
                  for r in (0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0, 1.2, 1.5)]),
    ]
    if cmp2:
        Q.update(
            foxedgedev=f2((cmp2["w_edge"] / ec["w1"] - 1) * 100, 1),
            foxfitdev=f2((cmp2["w_fit"] / ec["w1"] - 1) * 100, 1),
            losscmp=f2((cmp2["loss_gauss_clip"] / cmp2["loss_fox"] - 1) * 100, 0),
            foxw=f2(cmp2["w_fox"] * 1e6, 1), foxedge=f2(cmp2["w_edge"] * 1e6, 1),
            foxfit=f2(cmp2["w_fit"] * 1e6, 1), foxit=str(cmp2["iters"]),
            foxdev=f2(cmp2["dev_mir"] * 100, 2), foxmax=f2(cmp2["dev_max"] * 100, 2),
            foxmaxz=f2(cmp2["dev_max_z"], 1), zwfox=f2(cmp2["z_w_fox"], 1),
            zwdiff=f2(cmp2["z_w_fox"] - ec["d1"] * 1e3, 2),
            resid=f2(cmp2["resid"], 3), coredev=f2(cmp2["core_dev"], 3),
            coredevr=f2(cmp2["core_dev_r"] * 1e6, 0), coresign=f2(cmp2["core_sign"], 3),
            apdev=f2(cmp2["ap_dev"], 3),
            lossgauss=es(cmp2["loss_gauss_clip"], 2),
            lossfox=(es(cmp2["t2"]["loss_design"], 2) if cmp2["t2"] else "—"),
            wfoxT2=(f2(cmp2["t2"]["w_mode_um"], 1) if cmp2["t2"] else "—"),
            w2dT2=(f2(cmp2["t2"]["foxli2d"]["w_um"], 1) if cmp2["t2"] and cmp2["t2"].get("foxli2d") else "—"),
            wgaussT2=(f2(cmp2["t2"]["w1_um"], 1) if cmp2["t2"] else "—"),
            phasefox=(f2(cmp2["t2"]["modes"][0]["phase"], 4) if cmp2["t2"] else "—"),
            phdiff=(f2(abs(cmp2["t2"]["modes"][0]["phase"]) - ec["gouy_rt"], 4) if cmp2["t2"] else "—"),
            dminT2=(f2(cmp2["t2"]["dmin_mm"], 3) if cmp2["t2"] else "—"),
            lossmode=(es(cmp2["t2"]["modes"][1]["loss"], 2) if cmp2["t2"] else "—"),
        )
    return Q, tables


# ================================================================== 主流程
def main():
    ap = argparse.ArgumentParser(description="Task 03：腔内高斯光束、增益晶体与泵浦匹配")
    for k, v in DEFAULTS.items():
        ap.add_argument("--" + k, type=float, default=v)
    ap.add_argument("--quick", action="store_true", help="跳过 Fox-Li 对比（第 4 项）")
    args = ap.parse_args()

    R1, R2, L = args.R1 * 1e-3, args.R2 * 1e-3, args.L * 1e-3
    lam, D, lam_p = args.lam * 1e-9, args.D * 1e-3, args.lam_p * 1e-9
    Lc, rod = args.Lc * 1e-3, args.rod * 1e-3

    ec = equiv_confocal(R1, R2, L, lam)
    ec["D"] = D
    ec["dop_str"] = f"{args.dop:.1f} at.%"
    ec["Lc_str"] = f"Ø{args.rod:.0f}×{args.Lc:.0f} mm"
    ec["iris_pos_str"] = "晶体输入面处"

    print("=== 1~3 等价共焦腔与腔内高斯光束 ===")
    print(f"L={L*1e3:.1f} mm  R1={R1*1e3:.1f} mm  R2={R2*1e3:.1f} mm  "
          f"g1={ec['g1']:.6f}  g2={ec['g2']:.6f}  g1g2={ec['prod']:.6f}")
    print(f"共焦参数 f=zR={ec['f']*1e3:.3f} mm  Leq=2f={ec['Leq']*1e3:.3f} mm  Req=2f={ec['Req']*1e3:.3f} mm")
    print(f"束腰 w0={ec['w0']*1e6:.2f} um，距 M1 {ec['d1']*1e3:.2f} mm（{'腔心' if abs(ec['d1']-L/2)<1e-9 else '偏心'}）")
    print(f"等效共焦坐标：z1={-ec['d1']*1e3:+.2f} mm，z2={ec['d2']*1e3:+.2f} mm；"
          f"等效腔镜在真实镜外 {ec['eq_out1']*1e3:.2f} mm；Rmin=2f={ec['Rmin']*1e3:.1f} mm")
    print(f"镜面光斑 w1={ec['w1']*1e6:.2f} um（={ec['w1']/ec['w0']:.3f} w0） w2={ec['w2']*1e6:.2f} um")
    print(f"半发散角={ec['theta']*1e3:.4f} mrad（全角 {2*ec['theta']*1e3:.3f} mrad）")
    print(f"古伊相移 单程={ec['gouy_pass']:.6f} rad 往返={ec['gouy_rt']:.6f} rad "
          f"（2×单程 差={ec['gouy_diff']:.2e}）")
    print(f"FSR={C0/(2*L)/1e6:.2f} MHz  横/纵模间隔比={math.acos(math.sqrt(ec['prod']))/math.pi:.4f}  "
          f"±zR 模体积={ec['Vmode']*1e9:.2f} mm^3")
    print("自检残差：" + "  ".join(f"{k}={v:.1e}" for k, v in ec["checks"].items()))

    cmp2 = None
    if not args.quick:
        t0 = time.time()
        planes = np.linspace(0.0, L, 12)
        res, mom, edge, fit = foxli_scan(D / 2.0, R1, R2, L, lam, planes)
        dev = (mom - w_model(ec, planes)) / w_model(ec, planes)
        imin = int(np.argmin(mom))
        if 0 < imin < len(planes) - 1:
            x = planes[imin - 1:imin + 2] * 1e3
            c = np.polyfit(x, mom[imin - 1:imin + 2], 2)
            z_w_fox = -c[1] / (2 * c[0])
        else:
            z_w_fox = float(planes[imin] * 1e3)
        w_fit, resid = fit_w(res["u"], res["r"], 0.45 * D / 2.0)
        # 剖面与解析高斯（w = w1）的直接差：核心区（r<=0.4a）与整个口径内
        In = np.abs(res["u"]) ** 2
        In = In / In.max()
        lg = np.log(np.maximum(In, 1e-14)) - (-2.0 * (res["r"] / ec["w1"]) ** 2)
        m_core = res["r"] <= 0.4 * D / 2.0
        m_ap = res["r"] <= D / 2.0
        core_dev = float(np.max(np.abs(lg[m_core])))
        ap_dev = float(np.max(np.abs(lg[m_ap])))
        core_dev_r = float(res["r"][m_core][int(np.argmax(np.abs(lg[m_core])))])
        core_sign = float(np.mean(lg[m_core]))
        p2 = os.path.join(HERE, "results_task2.json")
        t2 = json.load(open(p2, encoding="utf-8")) if os.path.exists(p2) else None
        cmp2 = dict(iters=res["iters"], loss_fox=res["loss"], w_fox=res["w"],
                    w_edge=edge_w(res["u"], res["r"]), w_fit=w_fit, resid=resid,
                    dev_mir=dev[0], dev_max=dev.max(), dev_max_z=float(planes[int(np.argmax(dev))] * 1e3),
                    z_w_fox=z_w_fox, loss_gauss_clip=2 * math.exp(-2 * (D / 2 / ec["w1"]) ** 2),
                    core_dev=core_dev, ap_dev=ap_dev, core_dev_r=core_dev_r, core_sign=core_sign,
                    t2=t2, planes=planes.tolist(), mom=(mom * 1e6).tolist())
        print(f"\n=== 4 与 Task 02 对比（D={args.D} mm，{res['iters']} 次往返，{time.time()-t0:.0f}s）===")
        print(f"镜面光斑：高斯 w1={ec['w1']*1e6:.2f} um | Fox-Li 二阶矩 {res['w']*1e6:.2f} um "
              f"({dev[0]*100:+.2f}%) | 1/e^2 穿越 {cmp2['w_edge']*1e6:.2f} um | 核心区拟合 {w_fit*1e6:.2f} um")
        print(f"沿腔二阶矩偏差：{dev[0]*100:+.2f}%（镜面）… {dev.max()*100:+.2f}%（最大 @ z={cmp2['dev_max_z']:.1f} mm）")
        print(f"由 Fox-Li 场定出的束腰位置 {z_w_fox:.1f} mm（模型 {ec['d1']*1e3:.1f} mm，差 {z_w_fox-ec['d1']*1e3:+.2f} mm）")
        print(f"核心区高斯拟合最大残差 ln I = {resid:.3f}（口径内 r<0.45a，{w_fit*1e6:.1f} um）")
        print(f"剖面 vs 解析高斯（w=w1）：核心区 |Δln I| 最大 {core_dev:.3f}（@ r={core_dev_r*1e6:.0f} um，"
              f"平均 {core_sign:+.3f}=核心偏胖），全口径内最大 {ap_dev:.3f}")
        if t2:
            print(f"往返损耗：Task 02 Fox-Li {t2['loss_design']:.3e} vs 高斯截断估计 "
                  f"{cmp2['loss_gauss_clip']:.3e}；D_min(1%)={t2['dmin_mm']:.3f} mm")
            ph = t2["modes"][0]["phase"]
            print(f"往返相位：Fox-Li {ph:.6f} rad vs -古伊(2arccos√g1g2)={-ec['gouy_rt']:.6f} rad "
                  f"（差 {abs(ph)-ec['gouy_rt']:+.2e} rad）；次低阶模损耗 {t2['modes'][1]['loss']:.3e}")

    cry = crystal_design(ec, args.dop, Lc, rod, D, args.T, a_pump=args.ratio * ec["w0"])
    print(f"\n=== 5 增益晶体（Nd:YAG {args.dop:.1f} at.%，Ø{args.rod:.0f}×{args.Lc:.0f} mm，端泵）===")
    print(f"位置：距 M1 {cry['z_in']*1e3:.2f}~{cry['z_out']*1e3:.2f} mm（中心与束腰重合）；"
          f"端面 w={cry['w_in']*1e6:.2f}/{cry['w_out']*1e6:.2f} um，机内变化 {cry['dw']*100:.3f}%")
    print(f"口径：光阑 a/w={cry['margin_iris']:.2f}（截断损耗 {cry['iris_loss']:.2e}），"
          f"晶体半径 a/w={cry['margin_rod']:.1f}；机内模体积 {cry['V_mode']*1e9:.2f} mm^3")
    print(f"吸收：alpha={cry['alpha_cm']:.2f} cm^-1（1/e 深度 {cry['abs_depth']:.2f} mm），"
          f"{args.Lc:.0f} mm 吸收 {cry['abs_frac']*100:.1f}%")
    print(f"阈值：往返损耗 {cry['rt_loss']*100:.2f}% -> g_th={cry['g_th']:.4f} cm^-1，"
          f"N_th={cry['N_th']:.2e} cm^-3 = 掺杂离子数的 {cry['frac_inv']*100:.2f}%")
    ins = cry["ins"]
    print(f"插入晶体：等效腔长 {ins['opt_len']*1e3:.2f} mm，f {ins['zR']/ec['f']-1:+.2%}，"
          f"w0 {ins['w0']/ec['w0']-1:+.2%}（{ins['w0']*1e6:.1f} um），镜面光斑 {ins['w1']/ec['w1']-1:+.2%}，"
          f"束腰移动 {(ins['waist']-ec['d1'])*1e3:+.4f} mm")
    print(f"热透镜容差：正透镜把 tr/2 推向 -1，最贴近共焦简并出现在 f={cry['f_near']*1e3:.0f} mm "
          f"（细扫最小半迹={cry['tr_min_fine']:.7f}，{'越过' if cry['cross_conf'] else '不越过'} -1，"
          f"等效 g1g2={cry['prod_min']:.2e}）；"
          f"硬性失稳边界（tr/2=+1）在 f={cry['f_unst']*1e3:.0f} mm。"
          f"按设计焦距=2×边界 反解容许吸收功率 {cry['P_abs_max']:.2f} W"
          f"（本泵浦光斑下 1 W 吸收 ~ 1/f={1.0/cry['f_th_at_1W']:.2f} m^-1，量级估计）")
    for m in cry["therm"]:
        tag = "无" if math.isinf(m["f_th"]) else f"{m['f_th']*1e3:.0f} mm"
        print(f"   f_th={tag:>7s} tr/2={m['trace']:+.4f} {'稳定' if m['stable'] else '失稳 '}"
              + ("" if not m["stable"] else
                 f"  w0={m['w0']*1e6:5.1f} um  w1={m['w1']*1e6:5.1f} um  w2={m['w2']*1e6:5.1f} um  "
                 f"束腰={m['waist']*1e3:6.1f} mm"))
    print(f"   自再现校验：载入平板/透镜后 max | |R_波前| - R_镜 | / R = {cry['rmatch']:.1e}"
          f"（应到机器精度，否则单程 q 映射有误）")
    f_des = 1.0 / (cry["coeff_th"] * args.Pabs)
    print(f"   热透镜工作点（量级估计，面源公式）：1/f = {cry['coeff_th']:.2f}·P_abs [m^-1, W]；"
          f"取 P_abs={args.Pabs:.2f} W -> f_th={f_des*1e3:.0f} mm；"
          f"经过共焦简并档 P_abs={cry['P_deg']:.2f} W，共心侧硬失稳 P_abs={cry['P_hard']:.2f} W")
    hf = 1.0 - args.lam_p / args.lam
    print(f"   若只把量子缺陷部分（{hf*100:.1f}%）记为热功率：系数降为 "
          f"{cry['coeff_th']*hf:.2f} m^-1 W^-1 -> 相切档 P_abs={cry['P_deg']/hf:.2f} W、"
          f"硬失稳 P_abs={cry['P_hard']/hf:.2f} W（两种口径给出功率窗上下界）")
    for r in cry["pos"]:
        print(f"   位置 {r['name']:<12s}：w={r['w_mid']*1e6:6.1f} um  机内 dW/w={r['dw']*100:5.2f}%  "
              f"a/w={r['margin']:.2f}  截断损耗={r['loss']:.2e}")

    pmp = pump_design(ec, cry, lam_p, args.ratio, args.fiber)
    print(f"\n=== 6 泵浦匹配（{args.lam_p:.0f} nm，端泵，泵浦束腰在晶体输入面）===")
    print(f"w_p={args.ratio:.2f}×w0={pmp['wp']*1e6:.1f} um；z_Rp={pmp['zRp']*1e3:.0f} mm ≫ 晶体 "
          f"{args.Lc:.0f} mm；到 M1 处 {pmp['wp_M1']*1e6:.1f} um（半角 {pmp['theta_p']*1e3:.2f} mrad）")
    print(f"重叠因子：输入面 {pmp['xi_in']:.4f} -> 出射面 {pmp['xi_exit']:.4f}，吸收加权 "
          f"{pmp['xi_bar']:.4f}（机内最小 {pmp['xi_min']:.4f}）")
    print(f"允许带：xi>=0.90 -> {pmp['band'][0.90][0]:.3f}~{pmp['band'][0.90][1]:.3f}；"
          f">=0.95 -> {pmp['band'][0.95][0]:.3f}~{pmp['band'][0.95][1]:.3f}；"
          f">=0.98 -> {pmp['band'][0.98][0]:.3f}~{pmp['band'][0.98][1]:.3f}")
    print(f"有效泵浦体积 {pmp['V_pump']*1e9:.2f} mm^3（模体积 {pmp['V_mode']*1e9:.2f} mm^3）；"
          f"1 W 泵浦在输入面峰值强度 {pmp['I0_per_W']*1e-4:.0f} W/cm^2")
    print(f"聚焦：Ø{args.fiber:.0f} um 光纤等效半径 {pmp['w_fib']*1e6:.1f} um -> 倍率 β={pmp['beta']:.3f}；"
          f"光组 f1={pmp['pick']['f1']*1e3:.0f} mm / f2={pmp['pick']['f2']*1e3:.1f} mm（间距 "
          f"{pmp['pick']['d']*1e3:.1f} mm，β={pmp['pick']['beta']:.2f} -> {pmp['pick']['wp']*1e6:.0f} um）")

    fig_envelope(ec, cry, os.path.join(FIGDIR, "envelope.png"))
    fig_wavefront(ec, os.path.join(FIGDIR, "wavefront.png"))
    if cmp2:
        fig_verify(ec, planes, mom, edge, fit, res, cmp2, os.path.join(FIGDIR, "verify_task2.png"))
    fig_crystal(ec, cry, os.path.join(FIGDIR, "crystal.png"))
    fig_pump(pmp, os.path.join(FIGDIR, "pump.png"))
    print("\n图已写入", FIGDIR)

    Q, tables = build_Q(args, ec, cry, pmp, cmp2)
    write_typ(os.path.join(HERE, "summary_task3.typ"), Q, tables)
    raw = dict(
        inputs=dict(args.__dict__),
        equiv={k: (v if not isinstance(v, dict) else "…") for k, v in ec.items()
               if k not in ("checks", "empty", "M")},
        checks=ec["checks"],
        crystal={k: v for k, v in cry.items() if k not in ("hscan", "rel", "pos", "ins", "therm")} |
                dict(pos=cry["pos"], ins={k: v for k, v in cry["ins"].items() if k != "M"},
                     therm=[{k: v for k, v in m.items() if k != "M"} for m in cry["therm"]]),
        pump={k: v for k, v in pmp.items() if not isinstance(v, np.ndarray)},
        verify=(None if not cmp2 else {k: v for k, v in cmp2.items() if k != "t2"}),
        task2_source=None if not cmp2 or not cmp2["t2"] else cmp2["t2"],
    )
    with open(os.path.join(HERE, "results_task3.json"), "w", encoding="utf-8") as fp:
        json.dump(raw, fp, indent=2, ensure_ascii=False, default=str)
    print("summary_task3.typ / results_task3.json 已更新")


if __name__ == "__main__":
    main()
