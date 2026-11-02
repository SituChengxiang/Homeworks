#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy>=2.0", "matplotlib>=3.9", "scipy>=1.14"]
# ///
"""
Task 02：两镜稳定腔的自再现模与衍射损耗（Fox-Li 迭代）

在 Task 01 的腔型（R1、R2、L）上，把光看成波：用傍轴 Fresnel 衍射积分
描述一次单程传播，用迭代寻找"往返之后仍然保持自身形状"的横向场。

算法
    1. 柱对称 Fox-Li：场只在半径方向变化（球面镜 + 圆孔阑 + 轴上对齐时的基模），
       方位角积分用 J0 解析掉，得到一维积分算子；每次往返 = 传播 L -> 镜面口径与
       相位 -> 传播 L -> 镜面口径与相位。优势是分辨率高、速度快，能算到 1e-4
       以下的损耗。
    2. 二维校验：单次 FFT 的 Fresnel 传播（采样自动匹配，算子酉），在同样的口径下
       独立跑一遍 Fox-Li，核对损耗与模斑，并给出 I(x,y)。
    3. 口径扫描：对每个有效口径 D=2a 重复 1，得到往返损耗-口径曲线，
       再用二分法/插值找 1% 损耗对应的最小口径 D_min。
    4. 模式判别：把往返算子建成矩阵，求本征值 -> 各阶模的损耗与相位，
       用"基模与次低阶模的损耗差"衡量单模运转能力。

可复算
    uv run task2_foxli.py                        # 默认 V1 腔型 + 设计口径
    uv run task2_foxli.py --R1 600 --R2 600 --L 550 --lam 1064 --D 1.6
    uv run task2_foxli.py --quick                 # 跳过二维校验，只跑柱对称部分
参数一改，图、summary_task2.typ、results_task2.json 全部重算。

数值细节
    * 传播算子用 Nyström 离散（J0 核），网格 dr 取到 ~1.5 µm，损耗本底约 4e-5；
      报告里只采用高于本底的损耗值，低损耗段用解析截断式交叉核对。
    * 每次自由传播后按功率归一，抵消离散化带来的微小非酉性（否则长迭代会漂移）。
    * 收敛判据：相位对齐后的场变化 ||u_{n+1}-u_n||/||u_n|| < 1e-9，
      同时报告本征值 |γ_n| 的漂移。
"""

import argparse
import json
import math
import os
import time

import numpy as np
from scipy.special import j0

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

plt.rcParams["font.family"] = ["LXGW WenKai", "DejaVu Sans", "sans-serif"]
plt.rcParams["axes.unicode_minus"] = False

C0 = 299_792_458.0
HERE = os.path.dirname(os.path.abspath(__file__))
FIGDIR = os.path.join(HERE, "figures_task2")
os.makedirs(FIGDIR, exist_ok=True)

# ------------------------------------------------------------------ 默认参数（V1 腔型）
DEFAULTS = dict(
    R1=600.0,   # mm，M1 曲率半径（凹）
    R2=600.0,   # mm，M2 曲率半径（凹）
    L=550.0,    # mm，腔长
    lam=1064.0, # nm，工作波长
    D=1.6,      # mm，设计有效通光口径（直径）
    n=1000,     # 柱对称网格点数
    iters=1200, # 最大迭代次数
)


# ------------------------------------------------------------------ 几何与解析量
def cavity(R1, R2, L, lam):
    """g 参数与稳定性；非稳腔直接退出（此时 Fox-Li 没有可收敛的封闭模）。"""
    g1, g2 = 1 - L / R1, 1 - L / R2
    prod = g1 * g2
    if not 0.0 < prod < 1.0:
        raise SystemExit(f"该腔非稳定（g1*g2={prod:.4f}），无法求自再现模")
    return dict(g1=g1, g2=g2, prod=prod)


def rt_matrix(L, R1, R2):
    P = np.array([[1.0, L], [0.0, 1.0]])
    M1 = np.array([[1.0, 0.0], [-2.0 / R1, 1.0]])
    M2 = np.array([[1.0, 0.0], [-2.0 / R2, 1.0]])
    return M1 @ P @ M2 @ P


def eigen_q(M, lam):
    A, B, C, D = M[0, 0], M[0, 1], M[1, 0], M[1, 1]
    disc = (D - A) ** 2 + 4 * B * C
    if disc >= 0:
        return None
    sq = math.sqrt(-disc)
    return next(q for q in (((A - D) + 1j * sq) / (2 * C), ((A - D) - 1j * sq) / (2 * C))
                if q.imag > 0)


def mode_params(R1, R2, L, lam):
    """解析基模参数：束腰位置、w0、zR、两镜光斑。"""
    cv = cavity(R1, R2, L, lam)
    q1 = eigen_q(rt_matrix(L, R1, R2), lam)
    zR = q1.imag
    s = -q1.real
    w1 = math.sqrt(-lam / (math.pi * (1 / q1).imag))
    q2 = q1 + L
    w2 = math.sqrt(-lam / (math.pi * (1 / q2).imag))
    cv.update(q1=q1, waist=s, w1=w1, w2=w2, zR=zR,
              w0=math.sqrt(lam * zR / math.pi))
    cv["fsr"] = C0 / (2 * L)
    cv["tm_ratio"] = math.acos(math.sqrt(cv["prod"])) / math.pi
    return cv


# ------------------------------------------------------------------ 柱对称传播算子
def prop_matrix(d, r, dr, lam):
    """U2(r2) = int K(r2,r1) U1(r1) r1 dr1，K = -2πi/(λd) e^{ik(r1²+r2²)/(2d)} J0(k r1 r2/d)。"""
    k = 2 * math.pi / lam
    r1 = r[None, :]
    r2 = r[:, None]
    return ((-2j * math.pi / (lam * d))
            * np.exp(1j * k * (r1 ** 2 + r2 ** 2) / (2 * d))
            * j0(k * r1 * r2 / d) * (r1 * dr))


def radial_setup(a, R1, R2, L, lam, n):
    """往返算子的分步形式：自由传播矩阵 P + 两镜面的口径/相位因子（一维数组）。"""
    rmax = 2.2 * a
    r = np.linspace(0.0, rmax, n)
    dr = r[1] - r[0]
    k = 2 * math.pi / lam
    P = prop_matrix(L, r, dr, lam)
    m = (r <= a).astype(float)
    D1 = m * np.exp(-1j * k * r ** 2 / R1)
    D2 = m * np.exp(-1j * k * r ** 2 / R2)
    return P, D1, D2, r, dr


def round_trip_matrix(a, R1, R2, L, lam, n):
    """矩阵形式（只用于本征谱）：T = D1 · P · D2 · P。"""
    P, D1, D2, r, dr = radial_setup(a, R1, R2, L, lam, n)
    return np.diag(D1) @ P @ np.diag(D2) @ P, r, dr


# ------------------------------------------------------------------ Fox-Li 迭代（柱对称）
def foxli(a, R1, R2, L, lam, n=1000, iters=1200, tol=1e-9, u0=None, record=0):
    """柱对称 Fox-Li。返回本征值、损耗、模斑、历史与（可选）若干帧强度分布。"""
    P, D1, D2, r, dr = radial_setup(a, R1, R2, L, lam, n)

    def power(u):
        return float(np.sum(np.abs(u) ** 2 * r * dr))

    def propagate(u):
        """自由传播；按功率归一，抵消离散算子的微小非酉性（不含孔径损耗）。"""
        v = P @ u
        return v * math.sqrt(power(u) / max(power(v), 1e-300))

    def apply(u):
        v = propagate(u) * D2
        v = propagate(v) * D1
        return v

    if u0 is None:
        # 平滑高斯初值：宽度取口径的 0.45 倍，与基模（w≈0.54a）明显不同，
        # 又不会在孔径边缘产生断点（否则离散传播算子会被高频振铃激发）
        w_seed = 0.45 * a
        u = np.exp(-(r / w_seed) ** 2).astype(complex) * (r <= a)
    else:
        u = np.asarray(u0, dtype=complex).copy()
    u /= np.linalg.norm(u)

    hist_d, hist_g, frames = [], [], {}
    for it in range(1, iters + 1):
        v = apply(u)
        nv = np.linalg.norm(v)
        ov = np.sum(np.conj(u) * v)
        ph = ov / abs(ov) if abs(ov) > 0 else 1.0
        vn = v / ph
        vn /= np.linalg.norm(vn)          # 先归一，再与 u 比较（否则 d 会停在 1-|γ|）
        d = float(np.linalg.norm(vn - u) / np.linalg.norm(u))
        u = vn
        hist_d.append(d)
        hist_g.append(float(nv))
        if record and it in record:
            frames[it] = np.abs(u) ** 2
        if d < tol:
            break
    I = np.abs(u) ** 2
    w = math.sqrt(2 * np.sum(I * r ** 3 * dr) / np.sum(I * r * dr))
    return dict(gamma=float(nv), loss=float(1 - nv ** 2), w=w, iters=it,
                d=float(d), hist_d=hist_d, hist_g=hist_g, frames=frames,
                u=u, r=r, dr=dr)


def spectrum(a, R1, R2, L, lam, n=1100, nmax=4):
    """往返算子的本征谱：各阶模的损耗与相位（越靠前损耗越低）。"""
    T, r, dr = round_trip_matrix(a, R1, R2, L, lam, n)
    ev = np.linalg.eigvals(T)
    ev = ev[np.argsort(-np.abs(ev))][:nmax]
    return [(float(abs(g)), float(1 - abs(g) ** 2), float(np.angle(g))) for g in ev]


# ------------------------------------------------------------------ 二维校验
def fresnel2d(u, dx, lam, z):
    """单次 FFT 的 Fresnel 传播；输入输出采样自动相同（dx2 = λz/(N dx)）。"""
    N = u.shape[0]
    k = 2 * math.pi / lam
    x = (np.arange(N) - N / 2) * dx
    X, Y = np.meshgrid(x, x)
    A = np.fft.fftshift(np.fft.fft2(np.fft.ifftshift(u * np.exp(1j * k * (X ** 2 + Y ** 2) / (2 * z)))))
    f = np.fft.fftshift(np.fft.fftfreq(N, d=dx))
    x2 = lam * z * f
    return (dx * dx / (1j * lam * z)) * np.exp(1j * k * z) * np.exp(
        1j * k * (x2[None, :] ** 2 + x2[:, None] ** 2) / (2 * z)) * A


def foxli2d(a, R1, R2, L, lam, N=640, iters=400, tol=1e-7, record=()):
    """二维 Fox-Li：给出 I(x,y) 与迭代快照，核对柱对称结果。"""
    dx = math.sqrt(lam * L / N)
    x = (np.arange(N) - N / 2) * dx
    X, Y = np.meshgrid(x, x)
    rr = X ** 2 + Y ** 2
    k = 2 * math.pi / lam
    m = (rr <= a * a).astype(complex)
    D1 = m * np.exp(-1j * k * rr / R1)
    D2 = m * np.exp(-1j * k * rr / R2)
    w_seed = 2.0 * math.sqrt(lam * L / math.pi)
    u = (np.exp(-rr / w_seed ** 2) * m).astype(complex)
    frames, hist = {}, []
    for it in range(1, iters + 1):
        v = fresnel2d(u, dx, lam, L)
        v *= D2
        v = fresnel2d(v, dx, lam, L)
        v *= D1
        nv = np.linalg.norm(v)
        ov = np.sum(np.conj(u) * v)
        ph = ov / abs(ov)
        vn = v / ph
        vn /= np.linalg.norm(vn)
        d = float(np.linalg.norm(vn - u) / np.linalg.norm(u))
        u = vn
        hist.append(d)
        if it in record:
            frames[it] = np.abs(u) ** 2
        if d < tol:
            break
    I = np.abs(u) ** 2
    w = math.sqrt(2 * np.sum(I * rr) / np.sum(I))
    return dict(gamma=float(nv), loss=float(1 - nv ** 2), w=w, iters=it, d=d,
                I=I, x=x, frames=frames, hist=hist)


# ------------------------------------------------------------------ 口径扫描
def sweep(R1, R2, L, lam, n, Ds_mm):
    """Ds_mm: 口径列表（mm）；内部换算成米。"""
    rows = []
    for Dmm in Ds_mm:
        a = 0.5 * Dmm * 1e-3
        t0 = time.time()
        res = foxli(a, R1, R2, L, lam, n=n, iters=4000, tol=1e-8)
        sp = spectrum(a, R1, R2, L, lam, n=min(n, 900))
        rows.append(dict(D=Dmm, a=a, loss=res["loss"], w=res["w"], iters=res["iters"],
                         d=res["d"], modes=sp, t=time.time() - t0))
        print(f"  D={Dmm:5.2f} mm  loss={res['loss']:.3e}  w={res['w']*1e6:6.1f} um  "
              f"iters={res['iters']:4d}  d={res['d']:.1e}  ({time.time()-t0:.1f}s)", flush=True)
    return rows


def dmin_for(rows, target=0.01):
    """在 (D, log loss) 上反解 loss=target 的口径。"""
    pts = sorted((r["D"], r["loss"]) for r in rows if r["loss"] > 0)
    for (d1, l1), (d2, l2) in zip(pts, pts[1:]):
        if (l1 - target) * (l2 - target) <= 0 and l1 != l2:
            t = (math.log(target) - math.log(l1)) / (math.log(l2) - math.log(l1))
            return d1 + t * (d2 - d1)
    return float("nan")


# ------------------------------------------------------------------ 图
def fig_iteration(frames, x, D, path):
    keys = sorted(frames)
    picks = [keys[0]] + [k for k in (2, 4, 10, 30, keys[-1]) if k in keys and k != keys[0]]
    picks = sorted(set(picks))[:6]
    fig, axes = plt.subplots(2, 3, figsize=(7.6, 5.1))
    ext = [x[0] * 1e3, x[-1] * 1e3, x[0] * 1e3, x[-1] * 1e3]
    for ax, it in zip(axes.ravel(), picks):
        ax.imshow(frames[it] / frames[it].max(), origin="lower", extent=ext,
                  cmap="inferno", vmin=0, vmax=1, interpolation="bilinear")
        ax.set_title(f"第 {it} 次往返", fontsize=9)
        ax.set_xlim(-1.2, 1.2); ax.set_ylim(-1.2, 1.2)
        ax.tick_params(labelsize=7)
        ax.set_xlabel("x / mm", fontsize=8); ax.set_ylabel("y / mm", fontsize=8)
    fig.suptitle(f"Fox-Li 迭代：从偏离基模的高斯初值逐渐形成自再现模（D = {D:.1f} mm，每帧各自归一）",
                 fontsize=10)
    fig.tight_layout()
    fig.savefig(path, dpi=190)
    plt.close(fig)


def fig_mode(res2d, ana, D, path):
    I, x = res2d["I"], res2d["x"]
    ext = [x[0] * 1e3, x[-1] * 1e3, x[0] * 1e3, x[-1] * 1e3]
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(8.2, 3.5),
                                   gridspec_kw=dict(width_ratios=[1.05, 1.2]))
    im = ax1.imshow(I / I.max(), origin="lower", extent=ext, cmap="inferno",
                    vmin=0, vmax=1, interpolation="bilinear")
    ax1.set_xlim(-1.2, 1.2); ax1.set_ylim(-1.2, 1.2)
    ax1.set_xlabel("x / mm"); ax1.set_ylabel("y / mm")
    ax1.set_title("镜面处归一化强度 $I(x,y)$", fontsize=10)
    fig.colorbar(im, ax=ax1, fraction=0.046, pad=0.03).ax.tick_params(labelsize=7)

    n = I.shape[0] // 2
    prof = I[n, :] / I[n, :].max()
    sel = np.abs(x) < 1.2e-3
    ax2.plot(x[sel] * 1e3, prof[sel], lw=1.6, color="#1f5c8b", label="二维 Fox-Li 截面")
    rr = res2d["w"]
    ax2.plot(x[sel] * 1e3, np.exp(-2 * (x[sel] / rr) ** 2), "--", lw=1.2, color="#8b0000",
             label=f"高斯拟合 $w$={rr*1e6:.1f} μm")
    ax2.set_xlim(-1.2, 1.2)
    ax2.set_yscale("log"); ax2.set_ylim(1e-6, 1.2)
    ax2.set_xlabel("x / mm"); ax2.set_ylabel("归一化强度")
    ax2.set_title("强度截面与高斯拟合", fontsize=10)
    ax2.legend(fontsize=8)
    ax2.grid(alpha=0.25, which="both", lw=0.4)
    fig.suptitle(f"最终自再现模（D = {D:.1f} mm，λ = {ana['lam_nm']:.0f} nm）", fontsize=10.5)
    fig.tight_layout()
    fig.savefig(path, dpi=190)
    plt.close(fig)


def fig_convergence(hist_by_D, path):
    fig, ax = plt.subplots(figsize=(6.4, 3.6))
    for D, hist in hist_by_D.items():
        ax.semilogy(np.arange(1, len(hist) + 1), hist, lw=1.5, label=f"D = {D:.1f} mm")
        n6 = next((i + 1 for i, v in enumerate(hist) if v < 1e-6), None)
        if n6:
            ax.plot([n6], [1e-6], "o", ms=4, color=ax.lines[-1].get_color())
            ax.annotate(f"{n6}", (n6, 1e-6), textcoords="offset points", xytext=(2, -12),
                        fontsize=7.5, color=ax.lines[-1].get_color())
    ax.axhline(1e-9, color="#8b0000", ls=":", lw=0.9)
    ax.annotate("收敛判据 $10^{-9}$", (0.02, 1.3e-9), xycoords=("axes fraction", "data"),
                fontsize=8, color="#8b0000")
    ax.set_xlabel("往返次数 n")
    ax.set_ylabel(r"$\|u_{n+1}-u_n\|/\|u_n\|$（去全局相位）")
    ax.set_title("迭代收敛曲线", fontsize=10.5)
    ax.grid(alpha=0.25, which="both", lw=0.4)
    ax.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(path, dpi=190)
    plt.close(fig)


def fig_aperture(rows, dmin, w_gauss, path):
    D = np.array([r["D"] for r in rows])
    loss = np.array([r["loss"] for r in rows])
    w = np.array([r["w"] for r in rows]) * 1e6
    loss1 = np.array([r["modes"][1][1] if len(r["modes"]) > 1 else np.nan for r in rows])
    Dg = np.linspace(0.8e-3, 3.2e-3, 200)      # m
    est = 2 * np.exp(-2 * (Dg / 2 / w_gauss) ** 2)  # 两镜各截断一次

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(8.6, 3.5))
    ax1.semilogy(D, loss, "o-", ms=4.5, lw=1.4, color="#1f5c8b", label="Fox-Li 基模")
    ax1.semilogy(Dg * 1e3, est, "--", lw=1.1, color="#8a8a8a", label="高斯截断估计 $2e^{-2a^2/w^2}$")
    ax1.axhline(0.01, color="#8b0000", lw=1.0, ls="-", label="指标 1%")
    ax1.axvline(dmin, color="#8b0000", ls=":", lw=1.0)
    ax1.annotate(f"$D_{{min}}$ = {dmin:.2f} mm", (dmin + 0.05, 0.014), fontsize=9,
                 color="#8b0000", ha="left")
    ax1.set_xlabel("有效通光口径 D / mm")
    ax1.set_ylabel("往返衍射损耗 $\\delta$")
    ax1.set_ylim(3e-5, 0.2)
    ax1.set_title("基模损耗随口径的变化", fontsize=10)
    ax1.grid(alpha=0.25, which="both", lw=0.4)
    ax1.legend(fontsize=8)

    ax2.semilogy(D, loss, "o-", ms=4, lw=1.3, color="#1f5c8b", label="TEM$_{00}$")
    ax2.semilogy(D, loss1, "s-", ms=4, lw=1.3, color="#c0504d", label="次低阶模")
    ax2.set_xlabel("有效通光口径 D / mm")
    ax2.set_ylabel("往返损耗")
    ax2.set_title("基模与次低阶模的损耗：口径越小判别越强", fontsize=10)
    ax2.grid(alpha=0.25, which="both", lw=0.4)
    ax2.legend(fontsize=8)
    fig.tight_layout()
    fig.savefig(path, dpi=190)
    plt.close(fig)


# ------------------------------------------------------------------ 主流程
def main():
    ap = argparse.ArgumentParser(description="Task 02：Fox-Li 自再现模与衍射损耗")
    for k, v in DEFAULTS.items():
        ap.add_argument("--" + k, type=type(v), default=v)
    ap.add_argument("--quick", action="store_true", help="跳过二维校验")
    ap.add_argument("--sweep", type=str,
                    default="1.10,1.20,1.30,1.36,1.40,1.50,1.60,1.80,2.00",
                    help="口径扫描列表 / mm（大于 2 mm 时各阶模损耗几乎相同，迭代无法选出单一模式）")
    args = ap.parse_args()

    R1, R2, L = args.R1 * 1e-3, args.R2 * 1e-3, args.L * 1e-3
    lam = args.lam * 1e-9
    D = args.D * 1e-3
    ana = mode_params(R1, R2, L, lam)
    ana.update(lam_nm=args.lam, R1_mm=args.R1, R2_mm=args.R2, L_mm=args.L, D_mm=args.D)
    print("=== 腔型 ===")
    print(f"L={args.L} mm, R1={args.R1} mm, R2={args.R2} mm, λ={args.lam} nm")
    print(f"g1*g2={ana['prod']:.4f}  w0={ana['w0']*1e6:.1f} um  "
          f"w1={ana['w1']*1e6:.1f} um  zR={ana['zR']*1e3:.1f} mm  "
          f"束腰距M1={ana['waist']*1e3:.1f} mm")
    print(f"FSR={ana['fsr']/1e6:.2f} MHz  横纵模间隔比={ana['tm_ratio']:.4f}")

    print("\n=== 设计口径下的 Fox-Li 迭代 ===")
    t0 = time.time()
    hist_by_D = {}
    res = foxli(D / 2, R1, R2, L, lam, n=max(args.n, 1200), iters=max(args.iters, 1500),
                tol=1e-9, record=(1, 2, 4, 10, 30, 200, 400))
    print(f"D={args.D} mm: |γ|={res['gamma']:.8f}  往返损耗={res['loss']:.3e}  "
          f"模斑 w={res['w']*1e6:.1f} um  迭代 {res['iters']} 次  d={res['d']:.1e}  "
          f"({time.time()-t0:.1f}s)")
    sp = spectrum(D / 2, R1, R2, L, lam, n=max(args.n, 1000))
    print("模式谱（损耗从低到高）：", " ".join(f"{x[1]:.2e}" for x in sp))

    print("\n=== 收敛对比（不同口径）===")
    for Dq, itmax in ((1.3, 3000), (1.4, 3000), (1.6, 3000), (2.0, 9000)):
        r = foxli(Dq * 1e-3 / 2, R1, R2, L, lam, n=args.n, iters=itmax, tol=1e-9)
        hist_by_D[Dq] = r["hist_d"]
        n1e6 = next((i + 1 for i, v in enumerate(r["hist_d"]) if v < 1e-6), len(r["hist_d"]))
        print(f"D={Dq:.1f} mm: 收敛到 1e-9 用 {r['iters']} 次，到 1e-6 用 {n1e6} 次，"
              f"loss={r['loss']:.3e}")
        hist_by_D[Dq] = r["hist_d"]

    print("\n=== 口径扫描 ===")
    Ds = [float(x) for x in args.sweep.split(",") if x.strip()]
    rows = sweep(R1, R2, L, lam, args.n, Ds)
    dmin = dmin_for(rows, 0.01)
    print(f"→ 损耗 1% 对应最小口径 D_min = {dmin:.3f} mm")

    print("\n=== 二维校验 ===")
    res2d = None
    if not args.quick:
        t0 = time.time()
        res2d = foxli2d(D / 2, R1, R2, L, lam, N=640, iters=600, tol=1e-7,
                        record=(1, 2, 4, 10, 30, 200, 400))
        print(f"2D: |γ|={res2d['gamma']:.8f}  loss={res2d['loss']:.3e}  "
              f"w={res2d['w']*1e6:.1f} um  iters={res2d['iters']} d={res2d['d']:.1e} "
              f"({time.time()-t0:.0f}s)")
        print(f"柱对称 vs 二维：损耗 {res['loss']:.3e} / {res2d['loss']:.3e}，"
              f"模斑 {res['w']*1e6:.1f} / {res2d['w']*1e6:.1f} um")

    # ---------------- 图
    if res2d is not None:
        fig_iteration(res2d["frames"], res2d["x"], args.D, os.path.join(FIGDIR, "iteration.png"))
        fig_mode(res2d, ana, args.D, os.path.join(FIGDIR, "mode.png"))
    else:
        # 用柱对称结果合成圆对称的迭代快照
        frames = {}
        r = res["r"]
        X, Y = np.meshgrid(r, r)
        rr = np.sqrt(X ** 2 + Y ** 2)
        for it, I in res["frames"].items():
            frames[it] = np.interp(rr, r, I).astype(float)
        fig_iteration(frames, r, args.D, os.path.join(FIGDIR, "iteration.png"))
    fig_convergence(hist_by_D, os.path.join(FIGDIR, "convergence.png"))
    fig_aperture(rows, dmin, ana["w1"], os.path.join(FIGDIR, "aperture.png"))
    print("\n图已写入", FIGDIR)

    # ---------------- 结果文件
    summary = dict(
        R1_mm=args.R1, R2_mm=args.R2, L_mm=args.L, lam_nm=args.lam, D_mm=args.D,
        prod=ana["prod"], g1=ana["g1"], g2=ana["g2"], w0_um=ana["w0"] * 1e6,
        w1_um=ana["w1"] * 1e6, w2_um=ana["w2"] * 1e6, zR_mm=ana["zR"] * 1e3,
        waist_mm=ana["waist"] * 1e3, fsr_MHz=ana["fsr"] / 1e6, tm_ratio=ana["tm_ratio"],
        loss_design=res["loss"], w_mode_um=res["w"] * 1e6, iters_design=res["iters"],
        d_design=res["d"], dmin_mm=dmin,
        modes=[dict(loss=x[1], phase=x[2]) for x in sp],
        sweep=[dict(D=r["D"], loss=r["loss"], w_um=r["w"] * 1e6,
                    mode1_loss=(r["modes"][1][1] if len(r["modes"]) > 1 else None))
               for r in rows],
        conv={str(k): dict(iters=len(v), n1e6=next((i + 1 for i, d in enumerate(v) if d < 1e-6), None))
              for k, v in hist_by_D.items()},
        foxli2d=(None if res2d is None else
                 dict(loss=res2d["loss"], w_um=res2d["w"] * 1e6, iters=res2d["iters"])),
    )
    with open(os.path.join(HERE, "results_task2.json"), "w", encoding="utf-8") as fp:
        json.dump(summary, fp, indent=2, ensure_ascii=False)
    write_summary_typst(summary)
    print("summary_task2.typ / results_task2.json 已更新")


def write_summary_typst(s):
    def f(v, nd=4):
        return f"{v:.{nd}f}"
    lines = ["// 由 task2_foxli.py 自动生成，请勿手改。", "#let Q = ("]
    for k, v in [
        ("R1", f(s["R1_mm"], 1)), ("R2", f(s["R2_mm"], 1)), ("L", f(s["L_mm"], 1)),
        ("lam", f(s["lam_nm"], 1)), ("D", f(s["D_mm"], 2)),
        ("prod", f(s["prod"], 4)), ("g1", f(s["g1"], 4)), ("g2", f(s["g2"], 4)),
        ("w0", f(s["w0_um"], 1)), ("w1", f(s["w1_um"], 1)), ("w2", f(s["w2_um"], 1)),
        ("zR", f(s["zR_mm"], 1)), ("waist", f(s["waist_mm"], 1)),
        ("fsr", f(s["fsr_MHz"], 2)), ("tmr", f(s["tm_ratio"], 4)),
        ("loss", f"{s['loss_design']:.2e}"), ("wmode", f(s["w_mode_um"], 1)),
        ("iters", str(s["iters_design"])), ("dconv", f"{s['d_design']:.1e}"),
        ("dmin", f(s["dmin_mm"], 3)), ("loss1", f"{s['modes'][1]['loss']:.2e}"),
        ("loss2", f"{s['modes'][2]['loss']:.2e}"),
        ("two_d_loss", "—" if not s["foxli2d"] else f"{s['foxli2d']['loss']:.2e}"),
        ("two_d_w", "—" if not s["foxli2d"] else f(s["foxli2d"]["w_um"], 1)),
    ]:
        lines.append(f'  {k}: "{v}",')
    rows = ", ".join(
        f'(D: "{r["D"]:.2f}", loss: "{r["loss"]:.2e}", w: "{r["w_um"]:.1f}", '
        f'm1: "{0 if r["mode1_loss"] is None else r["mode1_loss"]:.2e}", '
        f'gap: "{0 if r["mode1_loss"] is None else r["mode1_loss"] - r["loss"]:.2e}")'
        for r in s["sweep"])
    lines.append(")")
    lines.append(f"#let SWEEP = ({rows})")
    with open(os.path.join(HERE, "summary_task2.typ"), "w", encoding="utf-8") as fp:
        fp.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
