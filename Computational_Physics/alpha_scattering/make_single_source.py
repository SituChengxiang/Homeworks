#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
把分模块的 C89 源码合并成单个 .c 文件。

为什么要这个：交作业要求"分模块写代码"，但老师那边不是专业开发，
Dev-C++ 里要一次把 9 个 .c 加进工程并不友好——只打开 main.c 按 F11
必然链接失败。所以保留模块化版本作为正式交付，另外生成一份
"单文件版"给演示用：打开一个文件、直接编译运行。

合并策略（不是简单 cat）：
  1. 按依赖顺序先输出所有头文件的声明（去掉 include guard 和内部 #include），
     这样每个函数的原型在使用点之前都已声明，不受 .c 先后顺序影响；
  2. 再按依赖顺序输出各 .c 的实现；
  3. 标准库头只在文件顶部放一份；
  4. 保留 #ifdef _WIN32 这类条件编译，只剥掉 include guard。

生成物与手写代码等价性由脚本外的编译+运行比对来保证（见 README 第 4 节）。
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
INCLUDE = os.path.join(HERE, "include")
SRC = os.path.join(HERE, "src")

OUT = os.path.join(HERE, "alpha_scattering_single.c")

# 依赖顺序：被依赖的在前
HEADERS = ["units.h", "cases.h", "coulomb.h", "dynamics.h", "stepper.h",
           "rk4.h", "trajectory.h", "rutherford.h", "export.h"]
SOURCES = ["cases.c", "coulomb.c", "dynamics.c", "stepper.c", "rk4.c",
           "trajectory.c", "rutherford.c", "export.c", "main.c"]

STD_INCLUDES = ["stdio.h", "stdlib.h", "string.h", "math.h"]

GUARD_RE = re.compile(r"^#ifndef\s+\w+\s*$")
GUARD_DEFINE_RE = re.compile(r"^#define\s+\w+\s*$")
GUARD_END_RE = re.compile(r"^#endif\s*(/\*.*\*/)?\s*$")
LOCAL_INCLUDE_RE = re.compile(r'^\s*#include\s+"')
STD_INCLUDE_RE = re.compile(r'^\s*#include\s+<([^>]+)>')


def strip_guard(text):
    """剥掉文件最外层的 #ifndef GUARD / #define GUARD ... #endif。"""
    lines = text.splitlines()
    # 去掉开头空白行
    while lines and not lines[0].strip():
        lines.pop(0)
    if lines and GUARD_RE.match(lines[0]):
        lines.pop(0)
        if lines and GUARD_DEFINE_RE.match(lines[0]):
            lines.pop(0)
        # 去掉最后一个 #endif
        for i in range(len(lines) - 1, -1, -1):
            if lines[i].strip():
                if GUARD_END_RE.match(lines[i]):
                    lines.pop(i)
                break
    return lines


def clean_source(text):
    """去掉内部 #include "x.h" 与标准库 #include（顶部统一给）。"""
    out = []
    for line in text.splitlines():
        if LOCAL_INCLUDE_RE.match(line):
            continue
        if STD_INCLUDE_RE.match(line):
            continue
        out.append(line)
    return out


def main():
    parts = []

    parts.append("/* =====================================================================\n"
                 " * α 粒子散射轨道计算 —— 单文件版（C89）\n"
                 " *\n"
                 " * 本文件由 make_single_source.py 从分模块源码自动合并生成，\n"
                 " * 内容与 include/ + src/ 下的模块化版本逐字对应，只是并成了一个\n"
                 " * 翻译单元，方便在 Dev-C++ 里直接打开单个文件按 F11 运行。\n"
                 " *\n"
                 " * 正式交付的模块化版本见 src/ 与 include/；两者行为完全一致。\n"
                 " *\n"
                 " * 编译：  gcc -std=c89 -pedantic-errors -O2 "
                 "alpha_scattering_single.c -o alpha -lm\n"
                 " * 运行：  ./alpha [输出目录] [-t 时长] [-dt 步长] [--manual] "
                 "[--ascii|--utf8]\n"
                 " * ===================================================================== */\n")

    parts.append("\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n"
                 "#include <math.h>\n")

    for h in HEADERS:
        path = os.path.join(INCLUDE, h)
        with open(path, encoding="utf-8") as fp:
            body = strip_guard(fp.read())
        body = [l for l in body if not LOCAL_INCLUDE_RE.match(l)
                and not STD_INCLUDE_RE.match(l)]
        parts.append("\n/* ---------- %s ---------- */\n%s\n"
                     % (h, "\n".join(body).rstrip()))

    for c in SOURCES:
        path = os.path.join(SRC, c)
        with open(path, encoding="utf-8") as fp:
            body = clean_source(fp.read())
        parts.append("\n/* ========== %s ========== */\n%s\n"
                     % (c, "\n".join(body).rstrip()))

    text = "\n".join(parts).rstrip() + "\n"
    with open(OUT, "w", encoding="utf-8") as fp:
        fp.write(text)

    print("已生成 %s（%d 行）" % (OUT, text.count("\n")))
    print("编译： gcc -std=c89 -pedantic-errors -Wall -Wextra -O2 "
          "%s -o alpha_single -lm" % os.path.basename(OUT))


if __name__ == "__main__":
    main()
