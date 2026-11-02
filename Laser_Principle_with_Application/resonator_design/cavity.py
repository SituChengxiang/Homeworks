#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# /// script
# requires-python = ">=3.11"
# dependencies = ["numpy>=2.0", "matplotlib>=3.9", "scipy>=1.14"]
# ///
"""
两镜光学谐振腔设计核算（《激光原理与技术》工程任务）——驱动入口。

设计对象
    λ = 632.8 nm，L = 550 mm，对称双凹 M1 = M2 = R = 600 mm（近共焦：
    g1*g2 = 0.007，比严格共焦 R = L = 550 mm 留了 50 mm 的简并点距离）；
    M1 高反，M2 输出耦合 T = 1.5%。

三块文件，各管一段（拆开是为了后面继续加功能时不动彼此）：
    cavity_core.py   纯计算：稳定性判据、模式参数、公差/失调/孔径核算
                     （以及 DESIGN / COMPARATORS 定义，改设计只动这里）
    report_figs.py   全部出图 -> task01/figures/
    cavity.py        本文件：算一遍 -> task01/results.json + task01/numbers.typ -> 调出图

报告（task01/report.typ，长版在 archive/report_long.typ）只引用 task01/numbers.typ 里的数字，不手抄。

运行
    uv run cavity.py            # 需要 uv（自动装 numpy/matplotlib/scipy）
"""

import json
import math
import os

from cavity_core import (
    COMPARATORS,
    DESIGN,
    analyze,
    analyze_only,
    clip_loss,
    roundtrip_retention,
)
from report_figs import (
    FIGDIR,
    fig_aperture,
    fig_cavity,
    fig_envelope,
    fig_gdiagram,
    fig_length_tolerance,
    fig_misalignment,
    fig_stability_demo,
)

HERE = os.path.dirname(os.path.abspath(__file__))


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
              "margin_crit",
              "L_stable_interval",
              "L_for_prod_0p05", "L_for_prod_0p95",
              "L_for_prod_0p05_hi", "L_for_prod_0p95_hi", "w1_at_Lm10mm", "w1_at_Lp10mm",
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
    fig_cavity(des)
    fig_envelope(des, z, w)
    fig_gdiagram(allres)
    fig_length_tolerance(des)
    fig_misalignment([des] + [r for r in others if r["stable"]])
    w_st_max, w_un_max, L_un = fig_stability_demo(des, others)
    des["qmap_w_iter_max_stable"] = float(w_st_max)
    des["qmap_w_iter_max_unstable"] = float(w_un_max)
    print(f"\nq 映射迭代 40 次：稳定腔模斑最大 {w_st_max*1e3:.3f} mm；"
          f"失稳对照（L={L_un*1e3:.0f} mm，g1g2>1）第 40 次已达 {w_un_max*1e3:.3g} mm")
    fig_aperture(des)
    print("\n图已写入", FIGDIR)

    with open(os.path.join(HERE, "task01", "results.json"), "w", encoding="utf-8") as fp:
        json.dump({"design": des, "comparators": others}, fp, indent=2, ensure_ascii=False,
                  default=lambda o: None if (isinstance(o, float) and not math.isfinite(o)) else o)

    write_summary_typst(des, others)
    print("task01/numbers.typ 已更新")


def tex_sci(v):
    """把 2.2e-7 这种数写成 Typst 公式串 '2.2 times 10^(-7)'，报告里可直接用。"""
    if v == 0:
        return "0"
    e = math.floor(math.log10(abs(v)))
    m = v / 10.0 ** e
    return f"{m:.1f} times 10^({int(e)})"


def fmt(v, nd=4, scale=1.0):
    """nd 为整数 -> 定点；nd == "sci" -> 科学计数法；nd == "tex" -> Typst 公式串。"""
    if v is None:
        return "none"
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, (int, float)):
        if nd == "sci":
            return f"{v * scale:.2e}"
        if nd == "tex":
            return tex_sci(v * scale)
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
        ("focus1_mm", "focus1", 1, 1e3, None),
        ("focus2_mm", "focus2", 1, 1e3, None),
        ("focus_sep_mm", "focus_sep", 1, 1e3, None),
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
        ("margin_crit_mm", "margin_crit", 1, 1e3, None),
        ("L_stable_lo_mm", "L_stable_interval", 1, 1e3, 0),
        ("L_stable_hi_mm", "L_stable_interval", 1, 1e3, 1),
        ("L_for_prod_0p05_mm", "L_for_prod_0p05", 1, 1e3, None),
        ("L_for_prod_0p95_mm", "L_for_prod_0p95", 1, 1e3, None),
        ("L_for_prod_0p05_hi_mm", "L_for_prod_0p05_hi", 1, 1e3, None),
        ("L_for_prod_0p95_hi_mm", "L_for_prod_0p95_hi", 1, 1e3, None),
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
        ("loss_total_tex", "loss_total_rt", "tex", 1.0, None),
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
            "crit: \"%s\", w0: \"%s\", mis: \"%s\", tmr: \"%s\")"
            % (
                esc(r["name"]),
                f'{r["prod"]:.4f}',
                f'{min(r["prod"], 1 - r["prod"]):.4f}',
                "是" if r.get("stable") else "否",
                (f'{r["margin_L"]*1e3:.1f} mm' if r.get("stable") else "—"),
                (f'{r["margin_crit"]*1e3:.1f} mm' if r.get("stable") else "—"),
                (f'{r["w0"]*1e6:.1f} μm' if r.get("stable") else "—"),
                (f'{r["mis_tolerance_urad"]:.1f} μrad' if r.get("stable") else "—"),
                (f'{r["tm_ratio"]:.3f}' if r.get("stable") else "—"),
            )
        )
    lines.append("#let COMPARISON = (" + ", ".join(rows) + ")")
    with open(os.path.join(HERE, "task01", "numbers.typ"), "w", encoding="utf-8") as fp:
        fp.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
