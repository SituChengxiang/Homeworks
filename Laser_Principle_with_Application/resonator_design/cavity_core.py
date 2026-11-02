# -*- coding: utf-8 -*-
"""
cavity_core.py —— 两镜谐振腔的纯计算模块（不画图、不做文件 IO）。

内容：
    g 参数与稳定区间/两类腔长裕度（critical_points、stable_span）
    ABCD 往返矩阵、本征 q、模式尺寸与 q 映射迭代
    镜面失调引起的谐振轴偏移、孔径截断损耗、Fresnel 往返复核
    本设计（DESIGN）与对照腔（COMPARATORS）的定义

出图在 report_figs.py；汇总 results.json / summary.typ 的入口仍是：

    uv run cavity.py            # PEP 723 依赖声明写在 cavity.py 头上
"""

import math

import numpy as np
from scipy.special import j0

C_LIGHT = 299_792_458.0  # m/s


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

# ------------------------------------------------------------------ 稳定区间与裕度
def critical_points(R1, R2):
    """
    prod(L) = (1-L/R1)(1-L/R2) 只在 L = 0、R1、R2、R1+R2 处取 0 或 1，
    这些正是 g-g 图上的临界/简并点（平面-平面、共焦、同心），返回升序有限值。
    """
    pts = {0.0}
    for c in (R1, R2, R1 + R2):
        if math.isfinite(c) and c > 0.0:
            pts.add(c)
    return sorted(pts)


def stable_span(L, R1, R2):
    """
    包含 L 的最大区间 (a, b)，区间内始终 0 < prod < 1。

    关键细节：对称腔在 L=R 处只是 prod 触 0 又弹回（相切型简并点），
    两侧仍然稳定，所以扩展会跨过这种"touch 点"；只有 prod 真正越出 (0,1)
    （落到 g1g2=1 的平面-平面/同心边界上）才算失稳边界。
    L 正落在临界点上时返回 (L, L)；L 本身失稳时返回 None。
    """

    def ok(x):
        if x <= 0.0:
            return False
        g1, g2 = g_params(x, R1, R2)
        p = g1 * g2
        return 0.0 < p < 1.0

    crit = critical_points(R1, R2)
    for c in crit:
        if abs(L - c) <= 1e-12:
            return (L, L)

    edges = crit + [math.inf]
    spans = [(edges[i], edges[i + 1]) for i in range(len(edges) - 1)]

    def mid(a, b):
        if math.isfinite(b):
            return 0.5 * (a + b)
        # 最后一段 (crit[-1], +inf)：往右取一个"临界点间距"的步长当探测点
        step = a - crit[-2] if len(crit) > 1 else max(a, 1.0)
        return a + step

    idx = next((i for i, (a, b) in enumerate(spans) if a < L < b), None)
    if idx is None or not ok(mid(*spans[idx])):
        return None
    i = idx
    while i + 1 < len(spans) and ok(mid(*spans[i + 1])):
        i += 1
    hi = spans[i][1]
    i = idx
    while i - 1 >= 0 and ok(mid(*spans[i - 1])):
        i -= 1
    lo = spans[i][0]
    return (lo, hi)


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
        "bore_r": bore_r,
        # 几何光学的焦点（F = R/2，自 M1 起算；凹面镜曲率无穷时焦点也取无穷）
        "focus1": 0.5 * R1,
        "focus2": L - 0.5 * R2,
        "focus_sep": abs(0.5 * R1 - (L - 0.5 * R2)),
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

    # 稳定区间与两类腔长裕度：
    #   margin_L   —— 到真正失稳边界（prod 越出 (0,1)）的距离；
    #   margin_crit —— 到最近临界/简并点（prod 触 0 或 1 的 L）的距离。
    # 对称腔的 L=R 是 touch 点，prod 触 0 弹回、两侧都稳定，它只算"临界点"
    # 不算"失稳边界"，所以两个数一般不相等（近共焦设计尤其明显）。
    span = stable_span(L, R1, R2)
    if span is None:
        out["L_stable_interval"] = [L, L]
        out["margin_L"] = 0.0
        out["margin_L_rel"] = 0.0
    else:
        a, b = span
        out["L_stable_interval"] = [a, b]
        out["margin_L"] = min(L - a, b - L)
        out["margin_L_rel"] = out["margin_L"] / L
    out["margin_crit"] = min(abs(L - c) for c in critical_points(R1, R2))

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
    # 对称腔会出现两段满足 0.05 <= prod <= 0.95 的区间（L=R 把它劈开）：
    # 下支 (lo[0], hi[0])，上支 (hi[1], lo[1])；上支的两个根也存下来备查。
    out["L_for_prod_0p95"] = lo[0]
    out["L_for_prod_0p05"] = hi[0]
    out["L_for_prod_0p05_hi"] = hi[1]
    out["L_for_prod_0p95_hi"] = lo[1]

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
    name="本设计（对称近共焦，g1g2≈0.007）",
    lam=632.8e-9,
    L=0.550,
    R1=0.600,
    R2=0.600,
    aperture_mir=6.35e-3,
    bore_r=1.0e-3,
    bore_z=(0.100, 0.400),  # He-Ne 管毛细段在腔内的位置（300 mm 长）
)

COMPARATORS = [
    dict(name="严格共焦（R=550 mm，临界）", lam=632.8e-9, L=0.550, R1=0.550, R2=0.550,
         aperture_mir=6.35e-3),
    dict(name="近共焦（R=560 mm）", lam=632.8e-9, L=0.550, R1=0.560, R2=0.560,
         aperture_mir=6.35e-3),
    dict(name="近同心（R=280 mm）", lam=632.8e-9, L=0.550, R1=0.280, R2=0.280,
         aperture_mir=6.35e-3),
    dict(name="平面-平面（临界）", lam=632.8e-9, L=0.550, R1=math.inf, R2=math.inf,
         aperture_mir=6.35e-3),
    dict(name="近平面（R=10 m）", lam=632.8e-9, L=0.550, R1=10.0, R2=10.0,
         aperture_mir=6.35e-3),
]


