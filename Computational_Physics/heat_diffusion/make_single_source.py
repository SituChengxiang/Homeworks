#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
把分模块的 C89 源码合并成单个 .c 文件。

作业要求分模块写，但演示或交作业时常要「打开一个文件、直接编译」。
所以保留 include/ + src/ 作为正式版本，另外生成一份单文件版；
两者的行为由 test/run.sh 逐字节比对全部数据文件来保证。

合并策略（不是简单 cat）：
  1. 先输出所有头文件的声明（剥掉 include guard 与内部 #include），
     这样函数原型都在使用点之前；
  2. 再按依赖顺序输出各 .c 的实现；
  3. 标准库头在顶部只放一份，由各文件实际用到的收集而来；
  4. 只剥 include guard，不动条件编译。

这一版顺带是个检查器：分模块编译时每个 .c 是独立翻译单元，
同名的 static 函数互不干涉；并成一个单元后重定义立刻编不过。
util_is_finite 原先在 solver.c 和 crank.c 各有一份 static 实现，
就是被这一步逼出来合并的。

用法：
    python3 make_single_source.py
"""

import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))
INCLUDE = os.path.join(HERE, "include")
SRC = os.path.join(HERE, "src")
OUT = os.path.join(HERE, "heat_diffusion_single.c")

# 依赖顺序：被依赖的在前
HEADERS = ["config.h", "util.h", "material.h", "grid.h", "solver.h",
           "crank.h", "analytic.h", "converge.h", "export.h", "verify.h"]
SOURCES = ["util.c", "material.c", "grid.c", "solver.c", "crank.c",
           "analytic.c", "converge.c", "export.c", "verify.c", "main.c"]

GUARD_RE = re.compile(r"^#ifndef\s+\w+\s*$")
GUARD_DEFINE_RE = re.compile(r"^#define\s+\w+\s*$")
GUARD_END_RE = re.compile(r"^#endif\s*(/\*.*\*/)?\s*$")
LOCAL_INCLUDE_RE = re.compile(r'^\s*#include\s+"')
STD_INCLUDE_RE = re.compile(r"^\s*#include\s+<([^>]+)>")


def strip_guard(text):
    """剥掉文件最外层的 #ifndef GUARD / #define GUARD ... #endif。"""
    lines = text.splitlines()
    while lines and not lines[0].strip():
        lines.pop(0)
    if lines and GUARD_RE.match(lines[0]):
        lines.pop(0)
        if lines and GUARD_DEFINE_RE.match(lines[0]):
            lines.pop(0)
        for i in range(len(lines) - 1, -1, -1):
            if lines[i].strip():
                if GUARD_END_RE.match(lines[i]):
                    lines.pop(i)
                break
    return lines


def drop_includes(lines):
    return [l for l in lines
            if not LOCAL_INCLUDE_RE.match(l) and not STD_INCLUDE_RE.match(l)]


def std_includes(path):
    with open(path, encoding="utf-8") as fp:
        return [m.group(1) for line in fp
                for m in [STD_INCLUDE_RE.match(line)] if m]


def main():
    collected = []
    for name in [os.path.join(INCLUDE, h) for h in HEADERS] + \
                [os.path.join(SRC, c) for c in SOURCES]:
        for inc in std_includes(name):
            if inc not in collected:
                collected.append(inc)

    parts = []
    parts.append("""/* ======================================================================
 * 一维热传导：两种边界条件下的铁棍温度演化 —— 单文件版（C89）
 *
 * 本文件由 make_single_source.py 从 include/ + src/ 自动合并生成，
 * 与模块化版本（正式交付）内容对应，只是并成一个翻译单元，
 * 方便只拷一个 .c 到别的机器上编译运行。
 *
 * 生成物，不要手改；改 src/ 再重跑本脚本。
 *
 * 编译：  gcc -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 \\
 *             heat_diffusion_single.c -o hd -lm
 * 运行：  ./hd [输出目录] [--verify-only]
 * ===================================================================== */
""")
    parts.append("".join("#include <%s>\n" % h for h in collected))

    for h in HEADERS:
        with open(os.path.join(INCLUDE, h), encoding="utf-8") as fp:
            body = drop_includes(strip_guard(fp.read()))
        parts.append("\n/* ---------- %s ---------- */\n%s\n"
                     % (h, "\n".join(body).rstrip()))

    for c in SOURCES:
        with open(os.path.join(SRC, c), encoding="utf-8") as fp:
            body = drop_includes(fp.read().splitlines())
        parts.append("\n/* ========== %s ========== */\n%s\n"
                     % (c, "\n".join(body).rstrip()))

    text = "\n".join(parts).rstrip() + "\n"
    with open(OUT, "w", encoding="utf-8") as fp:
        fp.write(text)

    print("已生成 %s（%d 行，标准库头 %s）"
          % (OUT, text.count("\n"), " ".join(collected)))
    print("编译： gcc -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 "
          "%s -o hd -lm" % os.path.basename(OUT))


if __name__ == "__main__":
    main()
