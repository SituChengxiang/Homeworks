#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
交付图的像素级核对。

不去"看"图，而是把 PNG 读成数组，检查：
  1) 文件存在、尺寸合理、不是空白；
  2) 图例里承诺的每条曲线，其颜色在画布上真的出现了足够多的像素；
  3) 时空图用的是 viridis，端点色必须都在；
  4) 文字渲染出来了（深色像素数量级对）。
任何一项不达标就非零退出——图形交付和数字交付一样要能被证伪。
"""

import os
import sys

import numpy as np
from PIL import Image

# 与 plot_final.py 保持一致的配色
COLORS = {
    "u0":   (0x99, 0x99, 0x99),
    "t10":  (0x2c, 0x7f, 0xb8),
    "t20":  (0xe6, 0x91, 0x3a),
    "t30":  (0x2e, 0x8b, 0x57),
    "conv": (0xb2, 0x18, 0x2b),
}
VIRIDIS_LO = (0x44, 0x01, 0x54)
VIRIDIS_HI = (0xfd, 0xe7, 0x25)

checks = 0
fails = 0


def report(ok, label, detail):
    global checks, fails
    checks += 1
    if not ok:
        fails += 1
    print(f"  [{'PASS' if ok else 'FAIL'}] {label:44s} {detail}")


def load(path):
    img = Image.open(path).convert("RGB")
    return np.asarray(img).astype(int), img.size


def near(arr, rgb, tol=18):
    d = np.abs(arr - np.array(rgb)).max(axis=2)
    return (d <= tol).sum()


def check_main(path, need_conv_inset):
    arr, size = load(path)
    w, h = size
    report(w > 900 and h > 600, f"{os.path.basename(path)} 尺寸", f"{w}x{h}")
    nonwhite = (arr.max(axis=2) < 245).sum()
    frac = nonwhite / (w * h)
    report(frac > 0.02, f"{os.path.basename(path)} 非空白比例",
           f"{100*frac:.2f}% 像素有内容")
    counts = {k: near(arr, v) for k, v in COLORS.items()}
    # 每条曲线至少要有几百个像素：线宽 1.9pt、170dpi、跨 101 个点
    for name, minimum in (("u0", 400), ("t10", 600), ("t20", 600),
                          ("t30", 600), ("conv", 600)):
        report(counts[name] >= minimum,
               f"{os.path.basename(path)} 曲线 {name}",
               f"{counts[name]} px（需 >= {minimum}）")
    dark = (arr.max(axis=2) < 100).sum()
    report(dark > 3000, f"{os.path.basename(path)} 文字/线条渲染",
           f"{dark} 个深色像素")
    return counts


def check_compare(path, ref):
    """ref 是单网格主图的同色像素数。对照图每个时刻画细+粗两条曲线，
       同色像素必须明显更多；这比"某个颜色不该出现"可靠——文字抗锯齿
       本来就会产生各种灰度，拿灰色像素数当"没画初值线"的证据是假判据。"""
    arr, size = load(path)
    for name in ("t10", "t20", "t30"):
        got = near(arr, COLORS[name])
        report(got >= 1.4 * ref[name],
               f"{os.path.basename(path)} 双色对照 {name}",
               f"{got} px vs 单网格 {ref[name]} px（需 >= 1.4 倍）")
    # 下面板的两条零线（#333333）必须画出来
    report(near(arr, (0x33, 0x33, 0x33), tol=12) > 400,
           f"{os.path.basename(path)} 差值面板的零线",
           f"{near(arr, (0x33, 0x33, 0x33), tol=12)} px")


def check_evolution(path):
    arr, size = load(path)
    lo = near(arr, VIRIDIS_LO, tol=24)
    hi = near(arr, VIRIDIS_HI, tol=24)
    report(lo > 2000, f"{os.path.basename(path)} viridis 低端（0 度）",
           f"{lo} px")
    report(hi > 200, f"{os.path.basename(path)} viridis 高端（100 度）",
           f"{hi} px")
    report(near(arr, (0x2a, 0x78, 0x8e), tol=30) > 2000,
           f"{os.path.basename(path)} 中段温度存在", "青色像素充足")


def check_scheme(path):
    arr, size = load(path)
    # 左panel 的 r=0.69 锯齿用橙色与绿色；右panel 的 CN r=500 用红色
    for name, minimum in (("t10", 500), ("t20", 400), ("t30", 400),
                          ("conv", 400)):
        report(near(arr, COLORS[name]) >= minimum,
               f"{os.path.basename(path)} 方案对照 {name}",
               f"{near(arr, COLORS[name])} px")


def check_budget(path):
    arr, size = load(path)
    blue = near(arr, COLORS["t10"])
    red = near(arr, COLORS["conv"])
    report(blue > 1500, f"{os.path.basename(path)} 格式误差柱（蓝）",
           f"{blue} px")
    report(red > 1500, f"{os.path.basename(path)} 表示误差柱（红）",
           f"{red} px")


def main():
    datadir = sys.argv[1] if len(sys.argv) > 1 else "data"
    print("== 交付图像素核对 ==")

    mains = [("fig_d1_dirichlet_dx01.png", True),
             ("fig_d2_periodic_dx01.png", False),
             ("fig_e1_dirichlet_dx02.png", True),
             ("fig_e2_periodic_dx02.png", False)]
    ref_counts = None
    for name, inset in mains:
        path = os.path.join(datadir, name)
        if not os.path.exists(path):
            report(False, name, "文件不存在")
            continue
        counts = check_main(path, inset)
        if name.startswith("fig_d1"):
            ref_counts = counts
        pdf = os.path.splitext(path)[0] + ".pdf"
        report(os.path.exists(pdf) and os.path.getsize(pdf) > 5000,
               f"{name} 的 PDF 副本", f"{os.path.getsize(pdf)} 字节")

    extras = [("fig_dx_compare.png",
               lambda pth: check_compare(pth, ref_counts)),
              ("fig_evolution.png", check_evolution),
              ("fig_scheme_compare.png", check_scheme),
              ("fig_error_budget.png", check_budget)]
    for name, fn in extras:
        path = os.path.join(datadir, name)
        if not os.path.exists(path):
            report(False, name, "文件不存在")
            continue
        fn(path)

    print(f"\n== 图形核对：{checks} 项，{fails} 项失败 ==")
    if fails:
        return 1
    print("FIGURES: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
