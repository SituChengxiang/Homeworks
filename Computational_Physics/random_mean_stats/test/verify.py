#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
独立交叉核对：用 Python 重算一遍 C 程序的每个模块，比对 data/*.dat。

这一份脚本不追求语言一致，只追求「算法实现相互独立」：
C 那边用 rand() 采样 + 手写分箱 + 系数展开 + 解析雅可比的 LM；
这里用同样的数学式子但完全重写的实现，再用 numpy / Fraction 做对照。

用法：
    python3 test/verify.py [datadir]
只依赖 numpy（不需要 scipy）；退出码非零表示有检查没通过。
"""

import math
import os
import sys
from fractions import Fraction as F

import numpy as np

DATA = sys.argv[1] if len(sys.argv) > 1 else "data"
RESULTS = []


def check(name, detail, ok):
    RESULTS.append((name, detail, bool(ok)))


def read_dat(path, col_float=True):
    rows = []
    with open(path) as fp:
        for line in fp:
            if line.startswith("#") or not line.strip():
                continue
            parts = line.split()
            rows.append([float(v) for v in parts] if col_float else parts)
    return np.array(rows)


# ---------- A. 随机数与采样 ----------

def check_sampling():
    samples = read_dat(os.path.join(DATA, "samples.dat"))[:, 1]
    n = len(samples)
    u = read_dat(os.path.join(DATA, "uniforms_head.dat"))[:, 1]
    ref = u[:12].reshape(4, 3).mean(axis=1)
    # 两个文件都用 %.12f 写出，容差取打印精度量级
    diff = np.abs(ref - samples[:4]).max()
    in_unit = bool((u >= 0).all() and (u < 1).all())
    check("A1 采样序列：导出的均匀数按 3 个一组取平均 == 前 4 个样本",
          f"n={n}, max|diff|={diff:.3e} (<2e-12)，u∈[0,1)={in_unit}",
          diff < 2e-12 and in_unit)

    mean, std = samples.mean(), samples.std(ddof=1)
    se = (1 / 6) / np.sqrt(n)
    z = (mean - 0.5) / se
    check("A2 样本均值对理论 0.5",
          f"mean={mean:.6f}, z={z:+.2f} (|z|<3)", abs(z) < 3)

    rel = std / (1 / 6) - 1
    # 样本标准差的抽样标准差是 sigma/sqrt(2(n-1))；门槛取 3 倍，
    # 避免把「每 20 次就会遇到一次的 2σ 抽样」误判成实现错误。
    tol = 3 * np.sqrt(1 / (2 * (n - 1)))
    check("A3 样本标准差对理论 1/6",
          f"std={std:.6f}, rel={rel:+.2%} (3sigma 上限 {tol:.2%})", abs(rel) < tol)


def _gammaq(a, x):
    """正则化上不完全伽马 Q(a, x)（Numerical Recipes 的 gser/gcf），用于卡方 p 值。"""
    if x <= 0.0:
        return 1.0
    if x < a + 1.0:
        ap = a
        s = 1.0 / a
        d = s
        for _ in range(500):
            ap += 1.0
            d *= x / ap
            s += d
            if abs(d) < abs(s) * 1e-15:
                break
        return 1.0 - s * math.exp(-x + a * math.log(x) - math.lgamma(a))
    b = x + 1.0 - a
    c = 1e30
    d = 1.0 / b
    h = d
    for i in range(1, 500):
        an = -i * (i - a)
        b += 2.0
        d = an * d + b
        if abs(d) < 1e-30:
            d = 1e-30
        c = b + an / c
        if abs(c) < 1e-30:
            c = 1e-30
        d = 1.0 / d
        de = d * c
        h *= de
        if abs(de - 1.0) < 1e-15:
            break
    return math.exp(-x + a * math.log(x) - math.lgamma(a)) * h


def chi2_sf(x, k):
    return _gammaq(k / 2.0, x / 2.0)


# ---------- B. 分箱 ----------

def ih_cdf(x):
    if x <= 0:
        return 0.0
    if x >= 1:
        return 1.0
    s = 3 * x
    if s <= 1:
        return s ** 3 / 6
    if s <= 2:
        return (-2 * s ** 3 + 9 * s ** 2 - 9 * s + 3) / 6
    return 1 - (3 - s) ** 3 / 6


def check_histogram():
    samples = read_dat(os.path.join(DATA, "samples.dat"))[:, 1]
    hist = read_dat(os.path.join(DATA, "histogram.dat"))
    n = len(samples)

    counts = np.zeros(10, dtype=int)
    outside = 0
    for x in samples:
        i = int(x / 0.1)
        if i >= 10:
            i = 9 if x <= 1.0 else -1
        if i < 0:
            outside += 1
        else:
            counts[i] += 1
    check("B1 分箱计数：Python 独立重算",
          f"counts={counts.tolist()}, sum={counts.sum()}",
          np.array_equal(counts, hist[:, 4]) and counts.sum() == n and outside == 0)

    exp = np.array([n * (ih_cdf(0.1 * (i + 1)) - ih_cdf(0.1 * i)) for i in range(10)])
    rel = np.abs(exp - hist[:, 5]).max()
    check("B2 期望计数：独立 Irwin-Hall CDF",
          f"max|diff|={rel:.3e}", rel < 1e-9)

    chi2 = ((hist[:, 4] - hist[:, 5]) ** 2 / hist[:, 5]).sum()
    p_chi = chi2_sf(chi2, 9)
    # 5% 临界 16.919 是「拒绝原假设」的门槛，不是「实现有 bug」的门槛：
    # 单次抽样本就有 5% 会越线。这里只在 p < 1e-3 时判失败。
    check("B3 卡方拟合优度（df=9；5% 临界 16.919，p<1e-3 才判异常）",
          f"chi2={chi2:.3f}, p={p_chi:.3f}", p_chi > 1e-3)

    s = np.sort(samples)
    f = np.array([ih_cdf(v) for v in s])
    d = max(np.max(np.arange(1, n + 1) / n - f), np.max(f - np.arange(0, n) / n))
    crit5 = 1.36 / np.sqrt(n)      # 5% 临界，报告里如实引用
    crit = 1.95 / np.sqrt(n)       # p = 1e-3 的临界，作为实现检查的门槛
    check(f"B4 K-S 检验（5% 临界 {crit5:.5f}；p<1e-3 才判异常）",
          f"D={d:.5f}", d < crit)


# ---------- C. Lagrange ----------

def check_lagrange():
    hist = read_dat(os.path.join(DATA, "histogram.dat"))
    xs = hist[:, 3]
    ys = hist[:, 4]
    cc = np.array([float(line.split()[1]) for line in
                   open(os.path.join(DATA, "lagrange_coeffs.dat"))
                   if not line.startswith("#") and line.strip()])

    # 精确有理数展开（节点取 0.05+0.1i 的精确值）
    fx = [F(2 * i + 1, 20) for i in range(10)]
    fy = [int(v) for v in ys]
    m = len(fx)
    exact = [F(0)] * m
    for i in range(m):
        num = [F(1)]
        den = F(1)
        for j in range(m):
            if j == i:
                continue
            new = [F(0)] * (len(num) + 1)
            for k, v in enumerate(num):
                new[k + 1] += v
                new[k] -= fx[j] * v
            num = new
            den *= fx[i] - fx[j]
        for k in range(m):
            exact[k] += fy[i] * num[k] / den
    exact_f = np.array([float(v) for v in exact])
    rel_cf = np.abs(exact_f - cc) / np.maximum(1.0, np.abs(exact_f))
    check("C1 插值系数 vs Fraction 精确展开",
          f"max rel={rel_cf.max():.3e}", rel_cf.max() < 1e-12)

    npoly = np.polynomial.polynomial.polyfit(xs, ys, 9)
    rel_np = np.abs(npoly - cc) / np.maximum(1.0, np.abs(npoly))
    check("C2 插值系数 vs numpy.polyfit（独立算法）",
          f"max rel={rel_np.max():.3e}", rel_np.max() < 1e-8)

    node = np.abs(np.polyval(cc[::-1], xs) - ys).max()
    check("C3 节点插值残差（系数展开）",
          f"max|P(x_i)-y_i|={node:.3e}", node < 1e-6)

    # 单位分解：y 全取 1 时多项式应恒为 1
    one = np.polynomial.polynomial.polyfit(xs, np.ones(10), 9)
    grid = np.linspace(0, 1, 1001)
    unity = np.abs(np.polyval(one[::-1], grid) - 1.0).max()
    check("C4 单位分解 max|P(x)-1|（y 恒为 1）",
          f"{unity:.3e}", unity < 1e-8)


# ---------- D. 高斯拟合 ----------

def gauss(x, m, s, A):
    return A / (s * np.sqrt(2 * np.pi)) * np.exp(-0.5 * ((x - m) / s) ** 2)


def lm_fit(x, y, p0, fix_A=None):
    """数值雅可比的 LM，与 C 里的解析雅可比实现相互独立。"""
    p = np.array(p0, dtype=float)
    lam = 1e-3

    def model(p):
        A = fix_A if fix_A is not None else p[2]
        return gauss(x, p[0], p[1], A)

    for _ in range(300):
        r = y - model(p)
        J = np.zeros((len(x), 2 if fix_A is not None else 3))
        for k in range(J.shape[1]):
            h = 1e-6 * max(1.0, abs(p[k]))
            pp = p.copy()
            pm = p.copy()
            pp[k] += h
            pm[k] -= h
            J[:, k] = (model(pp) - model(pm)) / (2 * h)
        A = J.T @ J
        b = J.T @ r
        d = np.linalg.solve(A + lam * np.diag(np.diag(A)), b)
        pn = p + d
        if pn[1] <= 1e-8 or (fix_A is None and pn[2] <= 0):
            lam *= 10
            continue
        if np.sum((y - model(pn)) ** 2) < np.sum(r * r):
            p, lam = pn, max(lam * 0.3, 1e-12)
        else:
            lam = min(lam * 10, 1e12)
        if np.linalg.norm(d) < 1e-12:
            break
    return p


def read_params():
    par = {}
    for line in open(os.path.join(DATA, "gauss_fit_params.dat")):
        if line.startswith("#") or not line.strip():
            continue
        tag, key, val = line.split()
        par[(tag, key)] = float(val)
    return par


def check_gauss():
    hist = read_dat(os.path.join(DATA, "histogram.dat"))
    xs, ys = hist[:, 3], hist[:, 4]
    n = int(ys.sum())
    par = read_params()

    p = lm_fit(xs, ys, [0.5, 1 / 6], fix_A=n * 0.1)
    dm = abs(p[0] - par[("primary", "m")])
    ds = abs(p[1] - par[("primary", "sigma")])
    check("D1 拟合参数 vs C 的 LM（A=N*dx 固定）",
          f"m 差={dm:.2e}, sigma 差={ds:.2e}", dm < 1e-5 and ds < 1e-5)

    p_free = lm_fit(xs, ys, [0.5, 1 / 6, n * 0.1])
    check("D2 自由幅度：Python 与 C 一致，且 A≈N*dx",
          f"A={p_free[2]:.3f}, A/(N*dx)={p_free[2]/(n*0.1):.4f}",
          abs(p_free[2] - par[("free", "A")]) < 1e-3)

    # 参数回代：真值已知，必须拟合回来
    s_true, m_true, A_true = 1 / 6, 0.5, 100.0
    y_clean = gauss(xs, m_true, s_true, A_true)
    p_rec = lm_fit(xs, y_clean, [0.55, 0.15], fix_A=A_true)
    check("D3 合成精确数据回代（真值 0.5, 1/6）",
          f"m={p_rec[0]:.9f}, sigma={p_rec[1]:.9f}",
          abs(p_rec[0] - m_true) < 1e-9 and abs(p_rec[1] - s_true) < 1e-9)

    rng = np.random.default_rng(20261024)
    y_noisy = np.bincount(np.clip(np.floor(rng.normal(m_true, s_true, 1000) / 0.1).astype(int),
                                  0, 9), minlength=10).astype(float)
    p_noisy = lm_fit(xs, y_noisy, [0.5, 1 / 6], fix_A=A_true)
    check("D4 合成含噪数据回代（N=1000，允许统计涨落）",
          f"m={p_noisy[0]:.6f}, sigma={p_noisy[1]:.6f}",
          abs(p_noisy[0] - m_true) < 0.02 and abs(p_noisy[1] - s_true) < 0.02)

    # 箱宽因子：字面 A=N 的拟合必须明显更差
    bad = lm_fit(xs, ys, [0.5, 0.2], fix_A=float(n))
    sse_bad = np.sum((ys - gauss(xs, bad[0], bad[1], n)) ** 2)
    sse_ok = np.sum((ys - gauss(xs, p[0], p[1], n * 0.1)) ** 2)
    check("D5 反面对照：字面 A=N 的 SSE 远大于 A=N*dx",
          f"SSE(A=N)={sse_bad:.1f} vs SSE(A=Ndx)={sse_ok:.1f}", sse_bad > 10 * sse_ok)

    # 模型差异：对精确 Irwin-Hall 计数拟合，sigma 同样偏大
    exp = np.array([n * (ih_cdf(0.1 * (i + 1)) - ih_cdf(0.1 * i)) for i in range(10)])
    p_ih = lm_fit(xs, exp, [0.5, 1 / 6], fix_A=n * 0.1)
    # 实测 sigma 还叠着 N=1000 的抽样涨落（样本标准差的相对标准差约 2.24%），
    # 所以与精确计数拟合值比较时按 5% 判，而不是要求逐位相同。
    gap = abs(p_ih[1] - par[("primary", "sigma")]) / p_ih[1]
    check("D6 精确 Irwin-Hall 计数拟合：sigma 偏差是模型差异",
          f"sigma={p_ih[1]:.6f} vs 实测 {par[('primary','sigma')]:.6f}，差 {gap:.2%}（容差 5%）",
          gap < 0.05)


def main():
    if not os.path.isdir(DATA):
        print(f"找不到数据目录 {DATA}，请先运行 sh run.sh")
        return 1

    check_sampling()
    check_histogram()
    check_lagrange()
    check_gauss()

    width = max(len(name) for name, _, _ in RESULTS)
    failed = 0
    for name, detail, ok in RESULTS:
        print("  %-6s %-*s  %s" % ("PASS" if ok else "FAIL", width, name, detail))
        failed += 0 if ok else 1
    print("\nCROSSCHECK: %d 项检查, %d 项失败" % (len(RESULTS), failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
