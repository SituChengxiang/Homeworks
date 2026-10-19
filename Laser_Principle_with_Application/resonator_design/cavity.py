#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy>=2.0", "matplotlib>=3.9", "scipy>=1.14"]
# ///
"""
两镜光学谐振腔设计核算（《激光原理与应用》工程任务）

设计对象
    λ = 632.8 nm，L = 550 mm，M1 凹面 R1 = 800 mm（高反），M2 凹面 R2 = 1200 mm（输出耦合）。

本脚本做四件事，全部结果写进 figures/ 与 summary.typ，报告直接引用，不手抄数字：

  1. 稳定性判据：g 参数、g1*g2、往返 ABCD 本征值（|λ|=1 ⟺ 光线有界）。
  2. 模式参数：本征 q 参数 -> 束腰位置/大小、镜面光斑、远场发散角、瑞利长度、
     纵模间隔 FSR 与横模间隔比；同时用闭式公式和 q 映射迭代两条独立路径互验。
  3. 鲁棒性：腔长漂移、曲率半径制造公差对 g1*g2 和光斑的影响；镜面倾斜引起的
     谐振轴偏移（失调灵敏度），并与三个"踩在临界状态上"的对照设计比较。
  4. 孔径核算：高斯截断损耗解析式 + Fox-Li 数值迭代，验证衍射损耗可忽略。

运行
    uv run cavity.py            # 需要 uv（自动装 numpy/matplotlib/scipy）
    python3 cavity.py           # 系统 python 里已有依赖时也可以
"""

import json
import math
import os

import numpy as np
from scipy.special import j0

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

plt.rcParams["font.family"] = ["LXGW WenKai", "DejaVu Sans", "sans-serif"]
plt.rcParams["axes.unicode_minus"] = False

C_LIGHT = 299_792_458.0  # m/s

HERE = os.path.dirname(os.path.abspath(__file__))
FIGDIR = os.path.join(HERE, "figures")
os.makedirs(FIGDIR, exist_ok=True)


# ------------------------------------------------------------------ 基本量
def g_params(L, R1, R2):
    """g_i = 1 - L / R_i，凹面镜 R 取正（业界通用符号约定）。"""
    return 1.0 - L / R1, 1.0 - L / R2


def rt_matrix(L, R1, R2):
    """以"M1 反射后离镜"为参考面的往返光线矩阵，作用顺序从右往左。"""
    P = np.array([[1.0, L], [0.0, 1.0]])
    M1 = np.array([[1.0, 0.0], [-2.0 / R1, 1.0]])
    M2 = np.array([[1.0, 0.0], [-2.0 / R2, 1.0]])
    return M1 @ P @ M2 @ P


def eigen_q(M, lam):
    """
    解 q = (A q + B)/(C q + D)，即 C q^2 + (D-A) q - B = 0，取 Im(q) > 0 的根。
    参考面在 M1，q1 = d + i zR（d 为参考面相对束腰的轴向坐标，传播方向为正）。
    """
    A, B = M[0, 0], M[0, 1]
    C, D = M[1, 0], M[1, 1]
    disc = (D - A) ** 2 + 4.0 * B * C
    if disc >= 0.0:
        return None  # 非稳腔：没有复的本征 q
    sq = math.sqrt(-disc)
    roots = [((A - D) + 1j * sq) / (2.0 * C), ((A - D) - 1j * sq) / (2.0 * C)]
    return next(q for q in roots if q.imag > 0)


def w_from_q(q, lam):
    """1/q = 1/Rm - i lam/(pi w^2) -> 光斑半径 w。"""
    return math.sqrt(-lam / (math.pi * (1.0 / q).imag))


def mode_fields(q1, L, lam, n=800):
    """沿轴的光斑半径 w(z) 与波前曲率半径 Rm(z)，z 从 M1 起算。"""
    z = np.linspace(0.0, L, n)
    q = q1 + z  # 自由传播时 q 线性变化
    invq = 1.0 / q
    w = np.sqrt(-lam / (math.pi * invq.imag))
    Rm = 1.0 / invq.real
    return z, w, Rm


def map_iterations(M, lam, q0, n=200):
    """把 q 映射 q <- (A q + B)/(C q + D) 迭代，检验本征模式是吸引子（稳定腔）。"""
    A, B, C, D = M[0, 0], M[0, 1], M[1, 0], M[1, 1]
    seq = []
    q = complex(q0)
    for _ in range(n):
        q = (A * q + B) / (C * q + D)
        if not np.isfinite(q) or abs(q) == 0:
            return None
        seq.append(q)
    return seq


# ------------------------------------------------------------------ 失调灵敏度
def _compose(f1, f2):
    """两个仿射光线映射 (M, b) 复合：f2 ∘ f1。"""
    M1, b1 = f1
    M2, b2 = f2
    return M2 @ M1, M2 @ b1 + b2


def axis_offset(L, R1, R2, alpha1=0.0, alpha2=0.0):
    """
    镜面绕自身顶点倾斜 alpha 时，腔的平衡光轴（谐振轴）在两镜处的横向偏移。
    倾斜在paraxial 下等价于给反射光线加 2*alpha 的角偏转（离轴聚焦项不变）。
    返回 (y_M1, y_M2)，单位与 L 一致（米）。
    """
    P = (np.array([[1.0, L], [0.0, 1.0]]), np.zeros(2))
    Ref1 = (np.array([[1.0, 0.0], [-2.0 / R1, 1.0]]), np.array([0.0, 2.0 * alpha1]))
    Ref2 = (np.array([[1.0, 0.0], [-2.0 / R2, 1.0]]), np.array([0.0, 2.0 * alpha2]))
    # 从 M1 反射后出发：P -> Ref2 -> P -> Ref1
    T = _compose(Ref1, _compose(P, _compose(Ref2, P)))
    M, b = T
    I = np.eye(2)
    try:
        v = np.linalg.solve(I - M, b)  # 平衡光轴：v = M v + b
    except np.linalg.LinAlgError:
        return (math.inf, math.inf)
    y1 = v[0]
    y2 = y1 + v[1] * L  # 从 M1 走 L 到 M2
    return y1, y2


# ------------------------------------------------------------------ 衍射损耗
def clip_loss(a, w):
    """半径 a 的圆孔径截断高斯光束（束宽 w）的单次功率损耗 = exp(-2 a^2 / w^2)。支持数组。"""
    a = np.asarray(a, dtype=float)
    w = np.asarray(w, dtype=float)
    with np.errstate(divide="ignore", over="ignore", invalid="ignore"):
        x = 2.0 * (a / w) ** 2
        out = np.exp(-np.minimum(x, 700.0))
    out = np.where(a <= 0, 1.0, out)
    if out.ndim == 0:
        return float(out)
    return out


def _prop_matrix(d, r, dr, lam):
    """柱对称 Fresnel（傍轴）传播算子，Nyström 离散：核用 J0 把方位角积分解析掉。"""
    k = 2.0 * math.pi / lam
    r1 = r[None, :]  # 输入面
    r2 = r[:, None]  # 输出面
    kern = (
        (-2j * math.pi / (lam * d))
        * np.exp(1j * k * (r2**2 + r1**2) / (2.0 * d))
        * j0(k * r2 * r1 / d)
    )
    return kern * (r1 * dr)  # 积分元 2*pi*r dr 已并入系数


def roundtrip_retention(q1, L, lam, a_mir, bore_r=None, bore_pos=None, n=3000, rmax=1.6e-3):
    """
    波动光学复核：把 q 法给出的本征模当作入射场，用柱对称 Fresnel 衍射积分走一个完整往返。

    反射的处理：模在镜面上的波前与镜面重合，反射即把横向相位共轭（u -> u*），
    振幅不变；因此造成功率损失的只有各处的孔径截取。返回

        (往返功率保持率 eta, 往返损耗 delta, 往返自再现度 overlap,
         M2 面数值光斑, M2 面解析光斑, M2 面数值波前曲率, M2 面解析波前曲率)

    overlap = |<u0, u_rt>| / sqrt(P0 * P_rt)，稳定腔里本征模应接近 1。
    腔内孔阑可选：作为一片薄光阑放在 bore_pos 处（往返各经过一次）。
    """
    r = np.linspace(0.0, rmax, n)
    dr = r[1] - r[0]
    k = 2.0 * math.pi / lam

    def w_of(q):
        return math.sqrt(-lam / (math.pi * (1.0 / q).imag))

    def roc_of(q):
        return 1.0 / (1.0 / q).real

    def iris(u, a):
        return np.where(r <= a, u, 0.0)

    def power(u):
        return float(np.sum(np.abs(u) ** 2 * r * dr))

    def overlap(u, v):
        s = float(np.abs(np.sum(np.conj(u) * v * r * dr)))
        return s / math.sqrt(power(u) * power(v))

    if bore_r is not None and bore_pos is not None:
        P1 = _prop_matrix(bore_pos, r, dr, lam)
        P2 = _prop_matrix(L - bore_pos, r, dr, lam)

        def go_fwd(u):
            u = iris(P1 @ u, bore_r)
            return P2 @ u

        def go_back(u):
            u = iris(P2 @ u, bore_r)
            return P1 @ u
    else:
        P = _prop_matrix(L, r, dr, lam)
        go_fwd = go_back = lambda u: P @ u

    w1 = w_of(q1)
    u0 = np.exp(-(r / w1) ** 2) * np.exp(1j * k * r**2 / (2.0 * roc_of(q1)))
    u0 = iris(u0, a_mir)
    p_in = power(u0)

    # M1 -> M2
    u = go_fwd(u0)
    w2_num = math.sqrt(2.0 * np.sum(np.abs(u) ** 2 * r**3 * dr) / np.sum(np.abs(u) ** 2 * r * dr))
    good = np.abs(u) > 1e-3 * np.max(np.abs(u))
    coef = np.polyfit(r[good] ** 2, np.unwrap(np.angle(u[good])), 1)
    roc_num = k / (2.0 * coef[0]) if coef[0] != 0 else float("nan")
    q2 = q1 + L

    # M2 反射 + 口径，M2 -> M1
    u = iris(np.conj(u), a_mir)
    u = go_back(u)
    # M1 反射 + 口径
    u = iris(np.conj(u), a_mir)

    p_out = power(u)
    eta = p_out / p_in
    return (eta, 1.0 - eta, overlap(u0, u), w2_num, w_of(q2), roc_num, roc_of(q2))

# ------------------------------------------------------------------ 单腔全部指标
def analyze(name, lam, L, R1, R2, aperture_mir=6.35e-3, bore_r=None, bore_z=None):
    g1, g2 = g_params(L, R1, R2)
    prod = g1 * g2
    M = rt_matrix(L, R1, R2)
    A, B, Cc, D = M[0, 0], M[0, 1], M[1, 0], M[1, 1]
    half_trace = (A + D) / 2.0
    stable = 0.0 < prod < 1.0 and abs(half_trace) < 1.0

    out = {
        "name": name,
        "lam": lam,
        "L": L,
        "R1": R1,
        "R2": R2,
        "g1": g1,
        "g2": g2,
        "prod": prod,
        "half_trace": half_trace,
        "stable": bool(stable),
        "margin_prod": min(prod, 1.0 - prod),
        "aperture_mir": aperture_mir,
        "rt_matrix": [float(A), float(B), float(Cc), float(D)],
    }
    ev = np.linalg.eigvals(M)
    out["rt_eig_abs"] = [float(abs(x)) for x in ev]

    if not stable:
        out["note"] = "非稳定腔：g1*g2 不在 (0,1) 内"
        return out

    q1 = eigen_q(M, lam)
    z, w, Rm = mode_fields(q1, L, lam)
    zR = q1.imag
    s_waist = -q1.real  # 束腰到 M1 的距离（朝 M2 为正）
    w0 = w_from_q(complex(0.0, zR), lam)

    w1, w2 = w[0], w[-1]
    # 闭式公式互验（g1,g2 同号且 >0 时适用）
    closed_ok = g1 > 0 and g2 > 0
    if closed_ok:
        w1c = math.sqrt(lam * L / math.pi * math.sqrt(g2 / (g1 * (1 - prod))))
        w2c = math.sqrt(lam * L / math.pi * math.sqrt(g1 / (g2 * (1 - prod))))
    else:
        w1c = w2c = float("nan")

    # q 映射迭代：稳定腔里它是上半平面内的旋转（保形、不收缩），
    # 所以判据不是"收敛"而是"有界"——模斑在一个有限环带内振荡，永不逸出。
    seed = complex(0.35 * L, 0.6 * zR)
    seq = map_iterations(M, lam, seed, 300)
    if seq:
        ws_iter = [w_from_q(qq, lam) for qq in seq]
        qmap_bounded = bool(np.all(np.isfinite(ws_iter)))
        qmap_w_lo, qmap_w_hi = float(min(ws_iter)), float(max(ws_iter))
    else:
        qmap_bounded = False
        qmap_w_lo = qmap_w_hi = float("nan")

    gouy = 2.0 * math.acos(math.sqrt(min(max(prod, 0.0), 1.0)))  # 往返 Gouy 相移
    fsr = C_LIGHT / (2.0 * L)
    ratio = gouy / (2.0 * math.pi)  # 横模间隔 / 纵模间隔

    out.update(
        {
            "q1": [float(q1.real), float(q1.imag)],
            "zR": zR,
            "waist_from_M1": s_waist,
            "waist_inside": bool(0.0 < s_waist < L),
            "w0": w0,
            "w1": w1,
            "w2": w2,
            "w1_closed": w1c,
            "w2_closed": w2c,
            "w1_closed_rel_err": abs(w1 - w1c) / w1c if closed_ok else None,
            "w2_closed_rel_err": abs(w2 - w2c) / w2c if closed_ok else None,
            "qmap_bounded": qmap_bounded,
            "qmap_w_lo": qmap_w_lo,
            "qmap_w_hi": qmap_w_hi,
            "Rmode_at_M1": float(Rm[0]),
            "Rmode_at_M2": float(Rm[-1]),
            "Rmatch_err_M1": abs(abs(Rm[0]) - R1) / R1,
            "Rmatch_err_M2": abs(abs(Rm[-1]) - R2) / R2,
            "theta_div": lam / (math.pi * w0),
            "gouy_round": gouy,
            "fsr": fsr,
            "tm_ratio": ratio,
            "tm_spacing": fsr * ratio,
        }
    )

    # 稳定区间（固定 R1,R2 时 L 的允许范围）与裕度
    # prod(L) = (1-L/R1)(1-L/R2)：零点在 L=R1,R2，等于 1 在 L=0 与 L=R1+R2
    bounds = sorted({0.0, R1, R2, R1 + R2})
    seg = None
    for lo_b, hi_b in zip(bounds[:-1], bounds[1:]):
        if hi_b <= lo_b:
            continue
        mid = 0.5 * (lo_b + hi_b)
        gm1, gm2 = g_params(mid, R1, R2)
        if 0.0 < gm1 * gm2 < 1.0 and lo_b <= L <= hi_b:
            seg = (lo_b, hi_b)
            break
    if seg is None:
        seg = (0.0, math.inf)
    out["L_stable_interval"] = [seg[0], seg[1]]
    out["margin_L"] = min(L - seg[0], seg[1] - L)
    out["margin_L_rel"] = out["margin_L"] / L

    # 工程裕度：g1*g2 限制在 [0.05, 0.95]
    def solve_prod(target):
        # (1-L/R1)(1-L/R2) = target  ->  二次方程
        a = 1.0 / (R1 * R2)
        b = -(1.0 / R1 + 1.0 / R2)
        c = 1.0 - target
        disc = b * b - 4 * a * c
        if disc < 0:
            return (float("nan"), float("nan"))
        s = math.sqrt(disc)
        return ((-b - s) / (2 * a), (-b + s) / (2 * a))

    lo = solve_prod(0.95)
    hi = solve_prod(0.05)
    out["L_for_prod_0p95"] = lo[0]
    out["L_for_prod_0p05"] = hi[0]

    # 光斑对腔长的敏感度（±10 mm）
    w1p = []
    for dL in (-0.010, 0.0, 0.010):
        Mx = rt_matrix(L + dL, R1, R2)
        qx = eigen_q(Mx, lam)
        w1p.append(w_from_q(qx, lam) if qx is not None else float("nan"))
    out["w1_at_Lm10mm"] = w1p[0]
    out["w1_at_Lp10mm"] = w1p[2]

    # 热漂移量级：铝间隔 23e-6/K，10 K -> dL ~ 0.13 mm
    out["dL_thermal_alu_10K"] = L * 23e-6 * 10.0
    Mth = rt_matrix(L + out["dL_thermal_alu_10K"], R1, R2)
    qth = eigen_q(Mth, lam)
    out["prod_at_thermal_drift"] = float(np.prod(g_params(L + out["dL_thermal_alu_10K"], R1, R2)))
    out["w1_at_thermal_drift"] = w_from_q(qth, lam) if qth is not None else float("nan")

    # 曲率半径制造公差 ±1%
    prods = []
    for f1 in (0.99, 1.0, 1.01):
        for f2 in (0.99, 1.0, 1.01):
            a1, a2 = g_params(L, R1 * f1, R2 * f2)
            prods.append(a1 * a2)
    out["prod_R_tol_1pct"] = [min(prods), max(prods)]

    # 失调灵敏度：M2 倾斜 100 urad 时谐振轴偏移
    y1, y2 = axis_offset(L, R1, R2, alpha1=0.0, alpha2=100e-6)
    out["mis100urad_offset_M1"] = abs(y1)
    out["mis100urad_offset_M2"] = abs(y2)
    # 容差：偏移达较小光斑 1/4 时的倾角
    alpha_tol = 100e-6 * (min(w1, w2) / 4.0) / max(abs(y1), abs(y2), 1e-12)
    out["mis_tolerance_urad"] = alpha_tol * 1e6

    # 孔径截断损耗（每往返：M1、M2 各一次；腔内光阑两次）
    dl = clip_loss(aperture_mir, w1) + clip_loss(aperture_mir, w2)
    if bore_r is not None:
        zb = np.linspace(0.0, L, 800)
        qz = q1 + zb
        wz = np.sqrt(-lam / (math.pi * (1.0 / qz).imag))
        inside = (zb >= bore_z[0]) & (zb <= bore_z[1])
        # 保守近似：把毛细管当成放在束宽最大处的一片薄光阑
        idx = np.arange(zb.size)[inside][np.argmax(wz[inside])]
        dl_bore = 2.0 * clip_loss(bore_r, float(wz[idx]))
        out["bore_w_max"] = float(wz[idx])
        out["bore_z_worst"] = float(zb[idx])
        out["bore_z_range"] = [bore_z[0], bore_z[1]]
        out["loss_bore_rt"] = dl_bore
    else:
        out["loss_bore_rt"] = 0.0
    out["loss_mirror_rt"] = dl
    out["loss_total_rt"] = dl + out["loss_bore_rt"]
    out["fresnel_N"] = aperture_mir**2 / (lam * L)
    out["a_over_w_max"] = aperture_mir / max(w1, w2)
    return out, z, w


def analyze_only(*a, **kw):
    res = analyze(*a, **kw)
    return res[0] if isinstance(res, tuple) else res


# ------------------------------------------------------------------ 设计输入
DESIGN = dict(
    name="本设计（双凹，g1g2≈0.17）",
    lam=632.8e-9,
    L=0.550,
    R1=0.800,
    R2=1.200,
    aperture_mir=6.35e-3,
    bore_r=1.0e-3,
    bore_z=(0.100, 0.400),  # He-Ne 管毛细段在腔内的位置（300 mm 长）
)

COMPARATORS = [
    dict(name="近平面（R=10 m）", lam=632.8e-9, L=0.550, R1=10.0, R2=10.0,
         aperture_mir=6.35e-3),
    dict(name="近共焦（R=560 mm）", lam=632.8e-9, L=0.550, R1=0.560, R2=0.560,
         aperture_mir=6.35e-3),
    dict(name="近同心（R=280 mm）", lam=632.8e-9, L=0.550, R1=0.280, R2=0.280,
         aperture_mir=6.35e-3),
    dict(name="平面-平面（临界）", lam=632.8e-9, L=0.550, R1=math.inf, R2=math.inf,
         aperture_mir=6.35e-3),
]


# ------------------------------------------------------------------ 图
def fig_envelope(des, z, w):
    """腔结构与基模高斯光束包络（横向尺寸相对轴向放大约 400 倍，仅示意）。"""
    L = des["L"]
    fig, ax = plt.subplots(figsize=(7.4, 3.2))
    zm, wm = z * 1e3, w * 1e3
    ax.fill_between(zm, -wm, wm, color="#1f5c8b", alpha=0.18, lw=0)
    ax.plot(zm, wm, lw=1.5, color="#1f5c8b", label="基模束宽 $w(z)$")
    ax.plot(zm, -wm, lw=1.5, color="#1f5c8b")
    ax.plot(zm, 3 * wm, lw=0.8, ls=":", color="#1f5c8b", label="$3w(z)$（含 99.98% 功率）")
    ax.plot(zm, -3 * wm, lw=0.8, ls=":", color="#1f5c8b")
    ax.axhline(0, color="k", lw=0.6)

    # 放电管（毛细 bore Ø2 mm）
    b0, b1 = des["bore_z_range"][0] * 1e3, des["bore_z_range"][1] * 1e3
    ax.add_patch(plt.Rectangle((b0, -1.0), b1 - b0, 2.0, fc="#e8e8e8", ec="#9a9a9a",
                               lw=0.8, alpha=0.75, zorder=0))
    ax.annotate("放电管 bore Ø2 mm", ((b0 + b1) / 2, -1.62), ha="center", fontsize=8, color="#555")

    # 两面镜（画成竖线，曲率在此尺度下半径仅 20 μm 量级，看不出弧）
    for x0, lab in ((0.0, "M1 凹面 R=800 mm\nHR 99.8%"), (L * 1e3, "M2 凹面 R=1200 mm\nOC T=1.5%")):
        ax.plot([x0, x0], [-2.15, 2.15], lw=2.6, color="#8b0000", solid_capstyle="butt")
        ax.annotate(lab, (x0, -2.4), ha="center", va="top", fontsize=8.2)
    zw = des["waist_from_M1"] * 1e3
    ax.axvline(zw, ls="--", lw=0.9, color="#444")
    ax.annotate(f"束腰 $w_0$={des['w0']*1e6:.1f} μm（距 M1 {zw:.0f} mm）",
                (zw, 1.75), ha="center", va="bottom", fontsize=8.2, color="#444")
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
    ax.legend(loc="lower right", fontsize=8, framealpha=0.95)
    ax.set_title("腔结构与基模高斯光束包络（横轴 550 mm、纵轴 ±4 mm，两轴比例不同）", fontsize=10.5)
    fig.tight_layout()
    fig.savefig(os.path.join(FIGDIR, "envelope.png"), dpi=190)
    plt.close(fig)


def fig_gdiagram(results):
    fig, ax = plt.subplots(figsize=(5.6, 4.6))
    gg = np.linspace(-1.6, 1.6, 700)
    G1, G2 = np.meshgrid(gg, gg)
    P = G1 * G2
    region = (P > 0) & (P < 1)
    ax.contourf(G1, G2, region.astype(float), levels=[0.5, 1.01], colors=["#cfe4f2"], extend="max")
    cx = ax.contour(G1, G2, P, levels=[0.2, 0.5, 0.8], colors="#8f8f8f",
                    linewidths=0.7, linestyles=":")
    for lev, pos in ((0.8, (1.28, 0.66)), (0.5, (0.95, 0.53)), (0.2, (0.55, 0.36))):
        ax.annotate(f"$g_1g_2$={lev}", pos, fontsize=7.5, color="#666666")
    ax.contour(G1, G2, P, levels=[0.0, 1.0], colors="#8b0000", linewidths=1.4)
    ax.set_facecolor("#fafafa")
    offs = {"本设计": (-118, 14), "近平面": (-96, -16), "近共焦": (10, -14),
            "近同心": (10, 6), "平面-平面": (-104, 8)}
    for r in results:
        ax.plot(r["g1"], r["g2"], "o", ms=7, color="#8b0000", zorder=5)
        key = next((kk for kk in offs if kk in r["name"]), "本设计")
        ax.annotate(r["name"], (r["g1"], r["g2"]), textcoords="offset points",
                    xytext=offs[key], fontsize=8, zorder=6)
    ax.axhline(0, lw=0.5, color="k")
    ax.axvline(0, lw=0.5, color="k")
    ax.set_xlabel("$g_1=1-L/R_1$")
    ax.set_ylabel("$g_2=1-L/R_2$")
    ax.set_title("g-g 稳定图（浅色为稳定区 $0<g_1g_2<1$）", fontsize=10.5)
    fig.tight_layout()
    fig.savefig(os.path.join(FIGDIR, "g_diagram.png"), dpi=190)
    plt.close(fig)


def fig_length_tolerance(des):
    lam, R1, R2 = des["lam"], des["R1"], des["R2"]
    Ls = np.linspace(0.05, 1.0, 500)
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
    ax1.annotate("失稳（共焦边界 $L=R_1$）", (R1 * 1e3, 0.12), fontsize=8, color="#8b0000", ha="center")
    ax1.axvline(R1 * 1e3, color="#8b0000", ls=":", lw=0.9)
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
    lam = DESIGN["lam"]
    L, R1, R2 = DESIGN["L"], DESIGN["R1"], DESIGN["R2"]
    M_st = rt_matrix(L, R1, R2)
    M_un = rt_matrix(0.86, R1, R2)  # L>R1，g1*g2<0 -> 失稳
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
            color="#1f5c8b", label="本设计 L=550 mm（$g_1g_2$=0.169）")
    ax.plot(range(1, len(ws_un) + 1), np.array(ws_un) * 1e3, "-s", ms=3, lw=1.2,
            color="#8b0000", label="同一副镜取 L=860 mm（$g_1g_2$<0，失稳）")
    ax.set_yscale("log")
    ax.set_xlabel("往返次数")
    ax.set_ylabel("M1 面模斑半径 w / mm")
    ax.set_title("q 映射迭代：稳定腔模斑有界振荡，失稳腔指数发散", fontsize=10.5)
    ax.legend(fontsize=8.5)
    fig.tight_layout()
    fig.savefig(os.path.join(FIGDIR, "stability_demo.png"), dpi=190)
    plt.close(fig)
    return float(max(ws_st)), float(np.nanmax(ws_un))


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


# ------------------------------------------------------------------ 主流程
def main():
    print("=" * 72)
    print("两镜稳定腔设计核算")
    print("=" * 72)

    des, z, w = analyze(**DESIGN)
    others = [analyze_only(**c) for c in COMPARATORS]
    allres = [des] + others

    print("\n--- 本设计 ---")
    for k in ("g1", "g2", "prod", "margin_prod", "half_trace", "stable", "zR",
              "waist_from_M1",
              "w0", "w1", "w2", "w1_closed", "w2_closed", "w1_closed_rel_err",
              "w2_closed_rel_err", "qmap_bounded", "qmap_w_lo", "qmap_w_hi", "Rmatch_err_M1", "Rmatch_err_M2",
              "theta_div", "fsr", "tm_ratio", "tm_spacing", "margin_L", "margin_L_rel",
              "L_stable_interval",
              "L_for_prod_0p05", "L_for_prod_0p95", "w1_at_Lm10mm", "w1_at_Lp10mm",
              "prod_R_tol_1pct", "mis100urad_offset_M1", "mis100urad_offset_M2",
              "mis_tolerance_urad", "bore_w_max", "bore_z_worst",
              "loss_mirror_rt", "loss_bore_rt", "loss_total_rt",
              "fresnel_N", "a_over_w_max"):
        print(f"{k:24s} {des[k]}")

    print("\n--- 对照设计 ---")
    hdr = f"{'名称':22s} {'g1g2':>8s} {'稳定':>4s} {'ΔL裕度/mm':>10s} {'w0/μm':>8s} {'失调/μrad':>10s}"
    print(hdr)
    for r in allres:
        if not r["stable"]:
            print(f"{r['name']:22s} {r['prod']:8.4f} {'否':>4s}")
            continue
        print(f"{r['name']:22s} {r['prod']:8.4f} {'是':>4s} "
              f"{r['margin_L']*1e3:10.1f} {r['w0']*1e6:8.1f} {r['mis_tolerance_urad']:10.1f}")

    # 波动光学复核：用柱对称 Fresnel 衍射积分把本征模走一个往返，孔径之外全部截掉
    # （相当于 Fox-Li 迭代的第一帧），与 q 法、解析截断式互相校验。
    print("\n--- 波动光学复核（Fresnel 往返）---")
    q1c = complex(*des["q1"])
    L, lam, am = DESIGN["L"], DESIGN["lam"], DESIGN["aperture_mir"]
    checks = [
        ("空腔（仅两镜 12.7 mm 口径）", None),
        ("腔内 Ø2.0 mm 孔阑（实际放电管）", DESIGN["bore_r"]),
        ("腔内 Ø1.0 mm 孔阑（刻意收紧，做定量校核）", 0.5e-3),
    ]
    for i, (label, br) in enumerate(checks):
        eta, delta, ovl, w2n, w2a, rocn, roca = roundtrip_retention(
            q1c, L, lam, am, bore_r=br, bore_pos=des["bore_z_worst"]
        )
        ana = des["loss_mirror_rt"] + (2.0 * clip_loss(br, des["bore_w_max"]) if br else 0.0)
        print(label)
        print(f"  往返功率保持率 eta = {eta:.8f} -> 往返损耗 delta = {delta:.3e}"
              f"（解析截断式 {ana:.3e}），自再现度 = {ovl:.6f}")
        print(f"  M2 光斑：数值 {w2n*1e3:.5f} mm / 解析 {w2a*1e3:.5f} mm，"
              f"偏差 {abs(w2n - w2a)/w2a:.2%}")
        print(f"  M2 波前曲率：数值 {rocn:.1f} m / 解析 {roca:.1f} m，"
              f"偏差 {abs(rocn - roca)/abs(roca):.2%}")
        if i == 0:
            des["rt_eta_bare"] = float(eta)
            des["rt_noise_floor"] = float(delta)   # 无孔阑时的偏离 = 网格离散化本底
            des["rt_overlap_bare"] = float(ovl)
            des["rt_w2_dev"] = float(abs(w2n - w2a) / w2a)
            des["rt_roc2_dev"] = float(abs(rocn - roca) / abs(roca))
        if i == 1:
            des["rt_loss_bore_num"] = float(delta)
        if i == 2:
            des["rt_loss_tight_num"] = float(delta)
            des["rt_loss_tight_overlap"] = float(ovl)
            des["rt_loss_tight_ana"] = float(ana)

    # 图
    fig_envelope(des, z, w)
    fig_gdiagram(allres)
    fig_length_tolerance(des)
    fig_misalignment([des] + [r for r in others if r["stable"]])
    w_st_max, w_un_max = fig_stability_demo(des, others)
    des["qmap_w_iter_max_stable"] = float(w_st_max)
    des["qmap_w_iter_max_unstable"] = float(w_un_max)
    print(f"\nq 映射迭代 40 次：稳定腔模斑最大 {w_st_max*1e3:.3f} mm；"
          f"失稳对照（L=860 mm）第 40 次已达 {w_un_max*1e3:.3g} mm")
    fig_aperture(des)
    print("\n图已写入", FIGDIR)

    with open(os.path.join(HERE, "results.json"), "w", encoding="utf-8") as fp:
        json.dump({"design": des, "comparators": others}, fp, indent=2, ensure_ascii=False,
                  default=lambda o: None if (isinstance(o, float) and not math.isfinite(o)) else o)

    write_summary_typst(des, others)
    print("summary.typ 已更新")


def fmt(v, nd=4, scale=1.0):
    """nd 为整数 -> 定点；nd == "sci" -> 科学计数法（用于极小的损耗、残差）。"""
    if v is None:
        return "none"
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, (int, float)):
        if nd == "sci":
            return f"{v * scale:.2e}"
        return f"{v * scale:.{nd}f}"
    return str(v)


def write_summary_typst(des, others):
    """把计算结果写成 typst 字典，报告里用 R.xxx 引用，避免手抄错数。"""
    def esc(s):
        return s.replace("\\", "\\\\").replace('"', '\\"')

    lines = ["// 由 cavity.py 自动生成，请勿手改。",
             "#let R = ("]
    keys = [
        ("lam_nm", "lam", 1, 1e9, None),
        ("L_mm", "L", 1, 1e3, None),
        ("R1_mm", "R1", 1, 1e3, None),
        ("R2_mm", "R2", 1, 1e3, None),
        ("g1", "g1", 4, 1.0, None),
        ("g2", "g2", 4, 1.0, None),
        ("prod", "prod", 4, 1.0, None),
        ("margin_prod", "margin_prod", 4, 1.0, None),
        ("half_trace", "half_trace", 4, 1.0, None),
        ("stable", "stable", 0, 1.0, None),
        ("zR_mm", "zR", 2, 1e3, None),
        ("waist_from_M1_mm", "waist_from_M1", 2, 1e3, None),
        ("waist_inside", "waist_inside", 0, 1.0, None),
        ("w0_um", "w0", 1, 1e6, None),
        ("w1_um", "w1", 1, 1e6, None),
        ("w2_um", "w2", 1, 1e6, None),
        ("w1_closed_um", "w1_closed", 1, 1e6, None),
        ("w2_closed_um", "w2_closed", 1, 1e6, None),
        ("w1_closed_rel_err_ppm", "w1_closed_rel_err", 3, 1e6, None),
        ("qmap_bounded", "qmap_bounded", 0, 1.0, None),
        ("qmap_w_lo_um", "qmap_w_lo", 1, 1e6, None),
        ("qmap_w_hi_um", "qmap_w_hi", 1, 1e6, None),
        ("Rmatch_err_M1_rel", "Rmatch_err_M1", "sci", 1.0, None),
        ("Rmatch_err_M2_rel", "Rmatch_err_M2", "sci", 1.0, None),
        ("theta_mrad", "theta_div", 3, 1e3, None),
        ("full_angle_mrad", "theta_div", 3, 2e3, None),
        ("fsr_MHz", "fsr", 2, 1e-6, None),
        ("tm_ratio", "tm_ratio", 4, 1.0, None),
        ("tm_spacing_MHz", "tm_spacing", 2, 1e-6, None),
        ("margin_L_mm", "margin_L", 1, 1e3, None),
        ("margin_L_rel_pct", "margin_L_rel", 1, 1e2, None),
        ("L_stable_lo_mm", "L_stable_interval", 1, 1e3, 0),
        ("L_stable_hi_mm", "L_stable_interval", 1, 1e3, 1),
        ("L_for_prod_0p05_mm", "L_for_prod_0p05", 1, 1e3, None),
        ("L_for_prod_0p95_mm", "L_for_prod_0p95", 1, 1e3, None),
        ("w1_at_Lm10mm_um", "w1_at_Lm10mm", 1, 1e6, None),
        ("w1_at_Lp10mm_um", "w1_at_Lp10mm", 1, 1e6, None),
        ("prod_Rtol_lo", "prod_R_tol_1pct", 4, 1.0, 0),
        ("prod_Rtol_hi", "prod_R_tol_1pct", 4, 1.0, 1),
        ("mis100u_offset_M1_um", "mis100urad_offset_M1", 1, 1e6, None),
        ("mis100u_offset_M2_um", "mis100urad_offset_M2", 1, 1e6, None),
        ("mis_tolerance_urad", "mis_tolerance_urad", 1, 1.0, None),
        ("loss_mirror_rt", "loss_mirror_rt", "sci", 1.0, None),
        ("bore_w_max_um", "bore_w_max", 1, 1e6, None),
        ("bore_z_worst_mm", "bore_z_worst", 1, 1e3, None),
        ("loss_bore_rt", "loss_bore_rt", "sci", 1.0, None),
        ("loss_total_rt", "loss_total_rt", "sci", 1.0, None),
        ("fresnel_N", "fresnel_N", 1, 1.0, None),
        ("a_over_w_max", "a_over_w_max", 1, 1.0, None),
        ("matA", "rt_matrix", 5, 1.0, 0),
        ("matB", "rt_matrix", 5, 1.0, 1),
        ("matC", "rt_matrix", 8, 1.0, 2),
        ("matD", "rt_matrix", 5, 1.0, 3),
        ("rt_eig_abs", "rt_eig_abs", "sci", 1.0, 0),
        ("gouy_round", "gouy_round", 4, 1.0, None),
        ("rt_noise_floor", "rt_noise_floor", "sci", 1.0, None),
        ("rt_loss_bore_num", "rt_loss_bore_num", "sci", 1.0, None),
        ("rt_overlap_bare", "rt_overlap_bare", 6, 1.0, None),
        ("dL_thermal_mm", "dL_thermal_alu_10K", 3, 1e3, None),
        ("prod_at_thermal_drift", "prod_at_thermal_drift", 6, 1.0, None),
        ("qmap_w_iter_max_stable_mm", "qmap_w_iter_max_stable", 3, 1e3, None),
        ("qmap_w_iter_max_unstable_mm", "qmap_w_iter_max_unstable", "sci", 1e3, None),
        ("rt_w2_dev", "rt_w2_dev", "sci", 1.0, None),
        ("rt_roc2_dev", "rt_roc2_dev", "sci", 1.0, None),
        ("rt_loss_tight_num_pct", "rt_loss_tight_num", 3, 1e2, None),
        ("rt_loss_tight_ana_pct", "rt_loss_tight_ana", 3, 1e2, None),
    ]
    for out, src, nd, scale, idx in keys:
        if src not in des:
            continue
        v = des[src]
        if isinstance(v, list):
            v = v[idx if idx is not None else 1]
        lines.append(f'  {out}: "{fmt(v, nd, scale)}",')
    lines.append(")")

    # 对照设计表
    rows = []
    for r in others + [des]:
        rows.append(
            "(name: \"%s\", prod: \"%s\", edge: \"%s\", stable: \"%s\", margin: \"%s\", "
            "w0: \"%s\", mis: \"%s\", tmr: \"%s\")"
            % (
                esc(r["name"]),
                f'{r["prod"]:.4f}',
                f'{min(r["prod"], 1 - r["prod"]):.4f}',
                "是" if r.get("stable") else "否",
                (f'{r["margin_L"]*1e3:.1f} mm' if r.get("stable") else "—"),
                (f'{r["w0"]*1e6:.1f} μm' if r.get("stable") else "—"),
                (f'{r["mis_tolerance_urad"]:.1f} μrad' if r.get("stable") else "—"),
                (f'{r["tm_ratio"]:.3f}' if r.get("stable") else "—"),
            )
        )
    lines.append("#let COMPARISON = (" + ", ".join(rows) + ")")
    with open(os.path.join(HERE, "summary.typ"), "w", encoding="utf-8") as fp:
        fp.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
