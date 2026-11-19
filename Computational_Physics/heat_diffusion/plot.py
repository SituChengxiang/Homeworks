#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
数据读取与共用设置。所有数字都来自 C 程序写出的 data/*.dat，
绘图脚本不做任何物理计算——要算的已经在 C 里算完并被第 6 步核对过。
"""

import os

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib import font_manager


def pick_cjk_font():
    """图上要写中文，挑一个本机确实装上的黑体。"""
    candidates = ["Source Han Sans CN", "Source Han Sans SC", "Noto Sans CJK SC",
                  "WenQuanYi Zen Hei", "SimHei", "Microsoft YaHei", "FZHei-B01",
                  "LXGW WenKai"]
    installed = {f.name for f in font_manager.fontManager.ttflist}
    for name in candidates:
        if name in installed:
            return name
    return None


def setup_style():
    font = pick_cjk_font()
    if font:
        plt.rcParams["font.sans-serif"] = [font]
        plt.rcParams["font.family"] = "sans-serif"
    plt.rcParams["axes.unicode_minus"] = False
    return font


def read_params(path):
    """从注释头按 key 取参数，和 C 那边的核对程序同一套约定。"""
    params = {}
    with open(path, "r", encoding="utf-8") as fp:
        for line in fp:
            if not line.startswith("#"):
                break
            if "param " not in line:
                continue
            for token in line.split():
                if "=" in token:
                    key, _, val = token.partition("=")
                    if key == "param":
                        continue
                    try:
                        params[key] = float(val)
                    except ValueError:
                        params[key] = val
    return params


def read_colnames(path):
    with open(path, "r", encoding="utf-8") as fp:
        for line in fp:
            if line.startswith("# columns:"):
                return line.split(":", 1)[1].split()
            if not line.startswith("#"):
                break
    return None


def load_profiles(datadir, dxname, bcname):
    """profiles_*.dat -> (x, dict(时刻 -> 剖面), 参数)"""
    path = os.path.join(datadir, f"profiles_{dxname}_{bcname}.dat")
    data = np.loadtxt(path)
    heads = read_colnames(path)
    params = read_params(path)
    x = data[:, 0]
    cols = {h: data[:, i + 1] for i, h in enumerate(heads[1:])}
    return x, cols, params, path


def load_columns(datadir, name):
    """通用多列文件 -> (第一列, dict(其余列), 参数)"""
    path = os.path.join(datadir, f"{name}.dat")
    data = np.loadtxt(path)
    heads = read_colnames(path)
    params = read_params(path)
    out = {h: data[:, i + 1] for i, h in enumerate(heads[1:])}
    return data[:, 0], out, params, path


def load_summary(datadir):
    path = os.path.join(datadir, "summary.dat")
    data = np.loadtxt(path)
    heads = read_colnames(path)
    return [dict(zip(heads, row)) for row in data], path


def load_evolution(datadir, dxname, bcname):
    """evolution_*.dat -> (t 数组, U 矩阵)，U[j] 是第 j 个时刻的整条剖面"""
    path = os.path.join(datadir, f"evolution_{dxname}_{bcname}.dat")
    data = np.loadtxt(path)
    return data[:, 0], data[:, 1:], path


def save(fig, out):
    fig.savefig(out)
    if out.endswith(".png"):
        fig.savefig(os.path.splitext(out)[0] + ".pdf")
    plt.close(fig)
    print(f"wrote {out}")
