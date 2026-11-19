#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
第三级证据：Python 独立实现交叉核对。

这里用 numpy 从零重写一遍有限体积初值、FTCS、Crank-Nicolson、Fourier
级数解析解，然后和 C 写出的 data/*.dat 对比。它能抓住「C 写错了」，
但两份实现同时写错、错得自洽时挡不住，所以只作辅助层；
最强的仍是 verify.c 的代入式核对。

CN 这边特意用「建完整稠密矩阵 + np.linalg.solve」，和 C 的
Thomas + Sherman-Morrison 是完全不同的算法路径。

用法：python3 test/verify.py [datadir]
"""

import os
import sys

import numpy as np

ALPHA = 2.3e-5
L = 0.1
W = 0.01
HOT = 100.0
COLD = 0.0
LO = (L - W) / 2.0
HI = (L + W) / 2.0
DT = 0.01
SNAPS = (10.0, 20.0, 30.0)
EPS = 1e-6
PI = np.pi

checks = 0
fails = 0


def report(ok, label, detail):
    global checks, fails
    checks += 1
    if not ok:
        fails += 1
    print(f"  [{'PASS' if ok else 'FAIL'}] {label:46s} {detail}")


def _na_to_nan(tok):
    tok = tok.strip()
    return np.nan if tok in ("NA", "") else float(tok)


def load_na(path):
    """criterion_*.dat 里「未达到」的行写 NA，numpy 默认转不了 float。"""
    return np.loadtxt(path, converters={i: _na_to_nan for i in range(5)})


def params(path):
    out = {}
    with open(path, encoding="utf-8") as fp:
        for line in fp:
            if not line.startswith("#"):
                break
            if "param " not in line:
                continue
            for tok in line.split():
                if "=" in tok:
                    k, _, v = tok.partition("=")
                    if k == "param":
                        continue
                    try:
                        out[k] = float(v)
                    except ValueError:
                        out[k] = v
    return out


# ---------- 独立实现 ----------

def fv_init(dx, periodic):
    n = int(round(L / dx))
    x = np.arange(n + 1) * dx
    x[n] = L
    u = np.zeros(n + 1)
    for i in range(n + 1):
        a, b = x[i] - dx / 2.0, x[i] + dx / 2.0
        if periodic:
            ov = 0.0
            for sh in (-1, 0, 1):
                lo, hi = LO + sh * L, HI + sh * L
                ov += max(0.0, min(b, hi) - max(a, lo))
            w = dx
        else:
            a, b = max(a, 0.0), min(b, L)
            w = b - a
            ov = max(0.0, min(b, HI) - max(a, LO))
        u[i] = COLD + (HOT - COLD) * ov / w
    if periodic:
        u[n] = u[0]
    return n, x, u


def ftcs(u, n, dx, r, periodic, m):
    """双缓冲原地写：省掉每步一次数组分配，长程重跑能快两三倍。"""
    a = u.copy()
    b = u.copy()
    for _ in range(m):
        if periodic:
            np.add(a[:n], r * (np.roll(a[:n], -1) - 2 * a[:n] + np.roll(a[:n], 1)),
                   out=b[:n])
            b[n] = b[0]
        else:
            np.add(a[1:n], r * (a[2:] - 2 * a[1:n] + a[:-2]), out=b[1:n])
            b[0] = 0.0
            b[n] = 0.0
        a, b = b, a
    return a.copy()


def cn_matrices(n, dx, r, periodic):
    """稠密矩阵：与 C 的三对角/循环三对角解法完全不同的路径。"""
    if periodic:
        m = n
        idx = np.arange(m)
        A = np.zeros((m, m))
        B = np.zeros((m, m))
        A[idx, idx] = 1.0 + r
        B[idx, idx] = 1.0 - r
        A[idx, (idx + 1) % m] = -r / 2.0
        A[idx, (idx - 1) % m] = -r / 2.0
        B[idx, (idx + 1) % m] = r / 2.0
        B[idx, (idx - 1) % m] = r / 2.0
    else:
        m = n - 1
        idx = np.arange(m)
        A = np.zeros((m, m))
        B = np.zeros((m, m))
        A[idx, idx] = 1.0 + r
        B[idx, idx] = 1.0 - r
        A[idx[:-1], idx[:-1] + 1] = -r / 2.0
        A[idx[1:], idx[1:] - 1] = -r / 2.0
        B[idx[:-1], idx[:-1] + 1] = r / 2.0
        B[idx[1:], idx[1:] - 1] = r / 2.0
    return A, B


def cn_run(u, n, dx, dt, periodic, t):
    """
    独立复刻 crank_advance：整步走满，末步按剩余时间缩短。
    r = 50 / 500 时 dt 不整除 10 s，不缩短末步就落不到 t = 10 s 上——
    第一版这里按 round(t/dt) 走整步，r=500 干脆走了 0 步，所以差出 134。
    """
    r = ALPHA * dt / (dx * dx)
    A, B = cn_matrices(n, dx, r, periodic)
    lu = np.linalg.inv(A) @ B          # 显式逆只为走另一条算法路径，不为加速
    v = (u[:n] if periodic else u[1:n]).copy()
    now = 0.0
    eps = 1e-12 * max(1.0, t)
    nstep = 0
    while t - now > eps:
        remain = t - now
        if remain < dt:
            Ar, Br = cn_matrices(n, dx, ALPHA * remain / (dx * dx), periodic)
            v = np.linalg.solve(Ar, Br @ v)
            now = t
        else:
            v = lu @ v
            now += dt
        nstep += 1
    out = u.copy()
    if periodic:
        out[:n] = v
        out[n] = v[0]
    else:
        out[1:n] = v
        out[0] = out[n] = 0.0
    return out, nstep


def b_dir(k):
    k = np.asarray(k, dtype=float)
    return (400.0 / (k * PI)) * np.sin(k * PI / 2.0) * np.sin(k * PI * W / (2.0 * L))


def a_per(k):
    k = np.asarray(k, dtype=float)
    return (200.0 / (k * PI)) * ((-1.0) ** k) * np.sin(k * PI * W / L)


def analytic(x, t, periodic, K=4000):
    x = np.atleast_1d(np.asarray(x, dtype=float))
    k = np.arange(1, K + 1)
    if periodic:
        q = 2.0 * k * PI / L
        coef = a_per(k) * np.exp(-ALPHA * q * q * t)
        return 10.0 + coef @ np.cos(np.outer(q, x))
    q = k * PI / L
    coef = b_dir(k) * np.exp(-ALPHA * q * q * t)
    return coef @ np.sin(np.outer(q, x))


def heat(u, n, dx, periodic):
    if periodic:
        return float(np.sum(u[:n]) * dx)
    w = np.full(n + 1, dx)
    w[0] = w[n] = dx / 2.0
    return float(np.sum(u * w))


# ---------- 核对 ----------

def check_initial(datadir):
    for tag, dx in (("fine", 0.001), ("coarse", 0.002)):
        path = os.path.join(datadir, f"initial_{tag}.dat")
        d = np.loadtxt(path)
        n, x, u = fv_init(dx, False)
        report(len(d) == n + 1, f"initial_{tag} 行数", f"{len(d)} == {n+1}")
        report(np.abs(d[:, 2] - u).max() < 1e-12, f"initial_{tag} FV 初值",
               f"max 差 {np.abs(d[:, 2] - u).max():.3e}")
        exact = (HOT - COLD) * W
        report(abs(heat(u, n, dx, False) - exact) < 1e-12,
               f"initial_{tag} 总热量",
               f"{heat(u, n, dx, False):.15f} 对 {exact:.15f}")


def check_profiles(datadir):
    for dxname, dx in (("fine", 0.001), ("coarse", 0.002)):
        for bcname, per in (("dirichlet", 0), ("periodic", 1)):
            path = os.path.join(datadir, f"profiles_{dxname}_{bcname}.dat")
            d = np.loadtxt(path)
            p = params(path)
            n = int(p["n"])
            dxr = p["dx"]
            r = ALPHA * DT / (dxr * dxr)
            _, _, u0 = fv_init(dxr, per)
            u = u0.copy()
            got = {}
            for step in range(1, int(SNAPS[-1] / DT) + 1):
                u = ftcs(u, n, dxr, r, bool(per), 1)
                for t in SNAPS:
                    if abs(step * DT - t) < 1e-12:
                        got[f"u_t{int(t)}"] = u.copy()
            worst = max(np.abs(d[:, i + 2] - got[f"u_t{int(t)}"]).max()
                        for i, t in enumerate(SNAPS))
            report(worst < 1e-9, f"profiles_{dxname}_{bcname} 三条快照剖面",
                   f"max 差 {worst:.3e}")
            # 与解析解的误差列
            ea = [np.abs(got[f"u_t{int(t)}"] - analytic(d[:, 0], t, per)).max()
                  for t in SNAPS]
            sp = np.loadtxt(os.path.join(datadir, "summary.dat"))
            row = [q for q in sp if abs(q[0] - dx) < 1e-12 and q[1] == per][0]
            rel = max(abs(ea[i] / row[11 + i] - 1.0) for i in range(3))
            report(rel < 0.02, f"summary 的 err_t10/20/30 ({dxname},{bcname})",
                   f"最大相对差 {rel:.4f}")


def check_stepdt(datadir):
    for dxname, dx in (("fine", 0.001), ("coarse", 0.002)):
        for bcname, per in (("dirichlet", 0), ("periodic", 1)):
            path = os.path.join(datadir, f"stepdt_{dxname}_{bcname}.dat")
            d = np.loadtxt(path)
            p = params(path)
            n = int(p["n"])
            r = p["r"]
            u = ftcs(d[:, 1], n, p["dx"], r, bool(per), 1)
            worst = np.abs(u - d[:, 2]).max()
            report(worst < 1e-12, f"stepdt_{dxname}_{bcname} 单步递推",
                   f"max 差 {worst:.3e}")


def check_cn(datadir):
    path = os.path.join(datadir, "overshoot.dat")
    d = np.loadtxt(path)
    p = params(path)
    dx = d[1, 0] - d[0, 0]
    n = int(round(L / dx))
    _, _, u0 = fv_init(dx, False)
    for col, rr in ((2, 0.23), (3, 50.0), (4, 500.0)):
        dt = rr * dx * dx / ALPHA
        u, ns = cn_run(u0, n, dx, dt, False, SNAPS[0])
        worst = np.abs(u - d[:, col]).max()
        scale = max(1.0, np.abs(d[:, col]).max())
        report(worst / scale < 5e-9, f"overshoot CN r={rr:g} 稠密逆矩阵路径",
               f"max 差 {worst:.3e}（场幅 {scale:.1f}，{ns} 步，末步缩短）")


def check_convergence(datadir):
    sp = np.loadtxt(os.path.join(datadir, "summary.dat"))
    heads = None
    with open(os.path.join(datadir, "summary.dat"), encoding="utf-8") as fp:
        for line in fp:
            if line.startswith("# columns:"):
                heads = line.split(":", 1)[1].split()
                break
    for row in sp:
        rec = dict(zip(heads, row))
        dx = rec["dx"]
        per = int(rec["periodic"])
        # 独立重跑只在 dx=0.001 做：四种网格的扫描是 sensitivity.c 的活，
        # 这里再跑一遍只是交叉验证，Python 侧每步都有解释器开销
        if abs(dx - 0.001) < 1e-12:
            n, _, u = fv_init(dx, per)
            r = ALPHA * DT / (dx * dx)
            target = 10.0 if per else 0.0
            steps = 0
            dev = 1e9
            while dev > EPS and steps < 400000:
                u = ftcs(u, n, dx, r, bool(per), 500)
                steps += 500
                dev = np.abs(u - target).max()
            rel = abs(steps * DT / rec["t_conv"] - 1.0)
            report(rel < 0.01,
                   f"独立重跑到主判据的时刻 ({'周期' if per else 'Dirichlet'})",
                   f"Python {steps * DT:.2f} s 对 C {rec['t_conv']:.2f} s，差 {100*rel:.3f}%")
        # 单指数预测
        amp = float(abs(np.atleast_1d(a_per(1))[0]) if per else abs(np.atleast_1d(b_dir(1))[0]))
        tau = L * L / ((4 if per else 1) * PI * PI * ALPHA)
        pred = tau * np.log(amp / EPS)
        report(abs(rec["t_conv"] / pred - 1.0) < 0.05,
               f"t_conv vs tau*ln(A/eps) (dx={dx:g}, {'周期' if per else 'Dirichlet'})",
               f"{rec['t_conv']:.2f} 对 {pred:.2f}")


def check_criterion(datadir):
    """副判据停手时刻与残差：预测残差 = eps_step * tau/dt，且必须早于主判据。"""
    sp = np.loadtxt(os.path.join(datadir, "summary.dat"))
    tconv = {(row[0], int(row[1])): row[7] for row in sp}
    for dxname, dx in (("fine", 0.001), ("coarse", 0.002)):
        for bcname, per in (("dirichlet", 0), ("periodic", 1)):
            path = os.path.join(datadir, f"criterion_{dxname}_{bcname}.dat")
            d = load_na(path)
            p = params(path)
            tau_dt = p["tau"] / p["dt"]
            tc = tconv[(dx, per)]
            worst = 0.0
            ok = True
            reached = 0
            for row in d:
                es, st, tt, dv = row[0], row[1], row[2], row[3]
                if np.isnan(st) or st <= 0:
                    continue          # NA / 未达到
                reached += 1
                worst = max(worst, abs(dv / (es * tau_dt) - 1.0))
                if tt >= tc:
                    ok = False        # 副判据必须比主判据先停手
            report(worst < 0.05 and ok and reached >= 3,
                   f"criterion_{dxname}_{bcname} 副判据松紧",
                   f"{reached} 档达到，残差最大相对差 {100*worst:.3f}%，"
                   f"均早于 t_conv={tc:.2f} s")

def agree(a, b, rel=0.02, floor=1e-12):
    """
    判等要带绝对下限：dx=0.002 那一档的表示误差是 1e-14 量级的纯机器噪声
    （C 用 64 项级数、这里用 4000 项，噪声水平本来就不同），
    对两个噪声值比相对差毫无意义，会把「都等于零」读成「差 70%」。
    """
    return abs(a - b) <= max(floor, rel * abs(b))


def check_decomposition(datadir):
    d = np.loadtxt(os.path.join(datadir, "decomposition.dat"))
    K = 4000
    k = np.arange(1, K + 1)
    q = k * PI / L
    dec = np.exp(-ALPHA * q * q * 10.0)
    bp = b_dir(k)
    for row in d:
        dx = row[0]
        n, x, u = fv_init(dx, False)
        # 分段常数重构的精确正弦系数（与 verify.c 的 fv_coeff_dirichlet 同式）
        A = np.maximum(x - dx / 2.0, 0.0)
        Bv = np.minimum(x + dx / 2.0, L)
        I = (np.cos(np.outer(q, A)) - np.cos(np.outer(q, Bv))) / q[:, None]
        bf = (2.0 / L) * (u * I).sum(1)
        a_ex = (bp * dec) @ np.sin(np.outer(q, x))
        a_fv = (bf * dec) @ np.sin(np.outer(q, x))
        # C 那边三档都取 r = 0.23，dt 随 dx^2 缩
        dt = 0.23 * dx * dx / ALPHA
        u_num = ftcs(u, n, dx, ALPHA * dt / (dx * dx), False, int(round(10.0 / dt)))
        rep = np.abs(a_fv - a_ex).max()
        fmt = np.abs(u_num - a_fv).max()
        tot = np.abs(u_num - a_ex).max()
        bad = [nm for nm, mine, theirs in
               (("格式", fmt, row[1]), ("表示", rep, row[2]), ("总", tot, row[3]))
               if not agree(mine, theirs)]
        report(not bad, f"误差分解 dx={dx:g}",
               f"格式 {fmt:.4e} 表示 {rep:.4e} 总 {tot:.4e}"
               + ("（与 C 一致）" if not bad else "（不一致：" + ",".join(bad) + "）"))

def check_stability(datadir):
    d = np.loadtxt(os.path.join(datadir, "stability.dat"))
    dx = d[1, 0] - d[0, 0]
    n = int(round(L / dx))
    _, _, u0 = fv_init(dx, False)
    r = 0.69
    dt = r * dx * dx / ALPHA
    u = u0
    for m, col in ((1, 3), (5, 4), (20, 5)):
        u = ftcs(u0, n, dx, r, False, m)
        worst = np.abs(u - d[:, col]).max()
        scale = max(1.0, np.abs(d[:, col]).max())
        report(worst / scale < 1e-8, f"FTCS r=0.69 走 {m} 步",
               f"max|u| = {np.abs(u).max():.4e}（相对差 {worst/scale:.2e}）")
    # 最高频模的放大因子 |1-4r sin^2(q dx/2)|，q 取 k=n-1
    qq = (n - 1) * PI / L
    g = abs(1.0 - 4.0 * r * np.sin(qq * dx / 2.0) ** 2)
    got = np.abs(d[:, 5]).max()
    report(g > 1.0 and got > 1e3,
           "r=0.69 走 20 步已放大到锯齿",
           f"理论每步 |G|={g:.4f}，20 步 |G|^20={g**20:.3e}；"
           f"实测 max|u|={got:.3e}")


def check_gibbs():
    K1, K2 = 10000, 40000
    k = np.arange(1, K2 + 1)
    q = k * PI / L
    coef = b_dir(k)

    def peak(K):
        win = 8.0 * L / K
        xs = LO + np.linspace(0.0, win, 800, endpoint=False)
        s = (coef[:K, None] * np.sin(np.outer(q[:K], xs))).sum(0)
        return s.max()

    s1 = 100.0 * (peak(K1) / HOT - 1.0)
    s2 = 100.0 * (peak(K2) / HOT - 1.0)
    ext = (s1 * K1 - s2 * K2) / (K1 - K2)
    ideal = 8.9489872236084
    report(abs(ext - ideal) < 0.0005, "Gibbs 过冲 Richardson 外推",
           f"{ext:.6f}% 对理论 {ideal:.6f}%")


def check_order():
    dx = 0.001
    n = int(round(L / dx))
    x = np.arange(n + 1) * dx
    x[n] = L
    k = 3
    q = k * PI / L
    u0 = np.sin(q * x)
    r = ALPHA * DT / (dx * dx)
    d = []
    for dt in (0.02, 0.01, 0.005, 0.0025):
        rr = ALPHA * dt / (dx * dx)
        ua = ftcs(u0, n, dx, rr, False, int(round(10.0 / dt)))
        ub = ftcs(u0, n, dx, rr / 2.0, False, int(round(10.0 / (dt / 2.0))))
        d.append(np.abs(ua - ub).max())
    ratios = [d[i] / d[i + 1] for i in range(3)]
    report(all(abs(v - 2.0) < 0.02 for v in ratios),
           "FTCS 纯时间误差一阶（同网格相减）",
           "相邻比 " + " / ".join(f"{v:.4f}" for v in ratios))


def main():
    datadir = sys.argv[1] if len(sys.argv) > 1 else "data"
    print("== Python 独立实现交叉核对 ==")
    check_initial(datadir)
    check_profiles(datadir)
    check_stepdt(datadir)
    check_cn(datadir)
    check_convergence(datadir)
    check_criterion(datadir)
    check_decomposition(datadir)
    check_stability(datadir)
    check_gibbs()
    check_order()
    print(f"\n== Python 交叉核对：{checks} 项，{fails} 项失败 ==")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
