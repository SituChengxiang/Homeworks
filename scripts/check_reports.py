#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Typst 报告封面自检：把仓库里每份报告编译到 /tmp，核对封面字段，绝不覆盖交付 PDF。

存在的理由：simple-zjut-report 0.3.0 起不再做「class 顶替 course、student-id 顶替
group」的借用回退，字段写错的表现是「少一行」。少一行肉眼很容易漏掉，所以把它变成
一条命令的机器检查——模板或某份报告的封面参数一改，跑一下就知有没有连带搞坏别的作业
（模板版本号 bump 后忘了改 import 这种事，以前是手工回归四份才发现的）。

用法：
    python3 scripts/check_reports.py        # 编译并核对全部报告
退出码：0 全部通过；1 有报告编译失败或封面字段缺失/串栏。
还会「只提示、不判失败」地列出中文续行：段落内换行在 Typst 里等于一个空格，
中文排版会多出缝隙，交稿前把段落压成一行就行（依据见 cjk_continuations 的注释）。
"""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = Path("/tmp/qoder-report-check")

# 每份报告：模式 + 封面上应该出现的标签（按顺序）+ 应该出现的取值。
# compile_only 的那些不查封面（用的是另一套模板 zjut-phy-hwk）。
REPORTS = [
    dict(
        name="计算物理·作业1 α散射",
        root=ROOT / "Computational_Physics/alpha_scattering",
        src=Path("doc/report1.typ"),
        labels=["课程", "专业班级", "姓名", "学号", "报告日期"],
        values=["计算物理", "应用物理基地班2401"],
    ),
    dict(
        name="计算物理·作业2 随机数统计",
        root=ROOT / "Computational_Physics/random_mean_stats",
        src=Path("doc/report.typ"),
        labels=["课程", "专业班级", "姓名", "学号", "报告日期"],
        values=["计算物理", "应用物理基地班2401"],
    ),
    dict(
        name="计算物理·作业3 一维热传导",
        root=ROOT / "Computational_Physics/heat_diffusion",
        src=Path("doc/report.typ"),
        labels=["课程", "专业班级", "姓名", "学号", "报告日期"],
        values=["计算物理", "应用物理基地班2401"],
    ),
    dict(
        name="激光原理·任务1",
        root=ROOT / "Laser_Principle_with_Application/resonator_design",
        src=Path("task01/report.typ"),
        labels=["课程", "成员", "小组", "报告日期"],
        values=["激光原理与技术", "5"],
    ),
    dict(
        name="激光原理·任务2",
        root=ROOT / "Laser_Principle_with_Application/resonator_design",
        src=Path("task02/report.typ"),
        labels=["课程", "成员", "小组", "报告日期"],
        values=["激光原理与技术", "5"],
    ),
    dict(
        name="激光原理·任务3",
        root=ROOT / "Laser_Principle_with_Application/resonator_design",
        src=Path("task03/report.typ"),
        labels=["课程", "成员", "小组", "报告日期"],
        values=["激光原理与技术", "10"],
    ),
    dict(
        name="激光原理·归档长版",
        root=ROOT / "Laser_Principle_with_Application/resonator_design",
        src=Path("archive/report_long.typ"),
        # 归档版没有组号，那一行整行不印；姓名/学号也不该再出现在封面上
        labels=["课程", "成员", "报告日期"],
        values=["激光原理与技术"],
        absent=["302024343017"],
    ),
    dict(
        name="激光原理·相干长度作业",
        root=ROOT / "Laser_Principle_with_Application",
        src=Path("shortSemester1.typ"),
        compile_only=True,
    ),
]

# 封面全部可能的标签；出现在不该有的模式里就是串栏
ALL_LABELS = ["课程", "专业班级", "姓名", "学号", "成员", "小组", "报告日期"]


def run(cmd):
    return subprocess.run(cmd, capture_output=True, text=True)


def cover_text(pdf):
    """第 1 页纯文本，去掉所有空白（含全角空格），标签才会连成「课程」而不是「课 程」。"""
    r = run(["pdftotext", "-f", "1", "-l", "1", str(pdf), "-"])
    if r.returncode != 0:
        return ""
    return re.sub(r"\s|\u3000", "", r.stdout)


def page_count(pdf):
    r = run(["pdfinfo", str(pdf)])
    m = re.search(r"^Pages:\s+(\d+)", r.stdout, re.M)
    return int(m.group(1)) if m else -1


# 中文段落里的「源码续行」：上一行以中日韩文字/标点收尾，下一行以中日韩文字开头。
# Typst 把段落内单个换行折叠成一个空格（只有空行才断段，见 docs/tutorial/writing-in-typst
# 「To add a new paragraph, just add a blank line」），而西文词间本来就有空格、看不出代价，
# 中文之间却会多出约一个空格宽的缝隙。上游 issue #792 就是这条，PR #7350（2026-09-18 合并）
# 才让 CJK 之间的换行空格塌缩；本机 typst 0.15.1 还没有，实测 你好↵世界（源码里断成两行） 会渲染出 3.0pt
# 缝隙（12pt 字号下一个空格的宽度），跟 text.lang 无关。
# 所以中文段落保持「一段一行」，别硬折。
_CJK = r"[\u3400-\u4dbf\u4e00-\u9fff\u3000-\u303f\uff00-\uffef]"
_BLOCK_START = re.compile(r"^\s*(?:=|\+|-|\*|#|//|\||&|,|\))")


def cjk_continuations(src_text):
    """返回疑似「被折断的中文行」的行号（1 起），这些位置会在 PDF 里多插一个空格。"""
    lines = src_text.split("\n")
    hits = []
    for i in range(len(lines) - 1):
        a, b = lines[i], lines[i + 1]
        if not a.strip() or not b.strip():
            continue
        # 块级起始行（标题、列表、set/rule、表格单元）本身就该换行，跳过
        if _BLOCK_START.match(a) or _BLOCK_START.match(b):
            continue
        if re.search(_CJK + r"$", a.rstrip()) and re.match(_CJK, b.lstrip()):
            hits.append(i + 1)
    return hits


def check_one(spec):
    OUT.mkdir(parents=True, exist_ok=True)
    slug = spec["src"].parent.name + "-" + spec["src"].stem
    pdf = OUT / (slug + ".pdf")
    src = spec["root"] / spec["src"]
    r = run(["typst", "compile", "--root", str(spec["root"]), str(src), str(pdf)])
    if r.returncode != 0:
        first = (r.stderr or r.stdout).strip().splitlines()
        return False, "编译失败：" + (first[0] if first else "未知错误"), ""
    src_text = src.read_text()
    # 中文续行只提示、不判失败：已经交出去的报告不回头追改，交稿前自己把段落压平。
    hits = cjk_continuations(src_text)
    warn = "" if not hits else (
        f"中文续行 {len(hits)} 处（第 {', '.join(map(str, hits[:8]))}"
        f"{' …' if len(hits) > 8 else ''} 行，每处会在 PDF 里多印一个空格）")
    if spec.get("compile_only"):
        return True, f"编译通过，{page_count(pdf)} 页（不查封面）", warn

    text = cover_text(pdf)
    problems = []

    # 1) 该出现的标签按顺序出现
    pos = -1
    for label in spec["labels"]:
        nxt = text.find(label)
        if nxt < 0:
            problems.append(f"封面缺行「{label}」")
        elif nxt <= pos:
            problems.append(f"「{label}」顺序不对（版面串栏）")
        else:
            pos = nxt

    # 2) 不该出现的标签（例如个人报告里冒出「小组」）
    for label in ALL_LABELS:
        if label not in spec["labels"] and label in text:
            problems.append(f"封面多出「{label}」（字段借用回来了）")

    # 3) 取值确实在封面上
    for v in spec.get("values", []):
        if re.sub(r"\s|\u3000", "", v) not in text:
            problems.append(f"取值「{v}」没印出来")

    # 4) 明确不该出现在封面上的东西（例如归档版残留的学号）
    for v in spec.get("absent", []):
        if v in text:
            problems.append(f"封面仍残留「{v}」")

    # 5) 封面没被顶到第二页：第 2 页不该再出现「报告日期」
    p2 = run(["pdftotext", "-f", "2", "-l", "2", str(pdf), "-"])
    if "报告日期" in re.sub(r"\s|　", "", p2.stdout):
        problems.append("封面溢出到第 2 页")

    # 5.5) Typst 的 ^ 与 _ 只吃紧跟的那一个原子：写成 x^-5 会把「−」单独上标、
    #      5 留在正文行。这类问题编译不报错，只能这样扫源码。
    for m in re.finditer(r"[_^][-+][0-9]", src_text):
        problems.append(f"指数/下标缺括号：{m.group(0)!r} 应写成 …^(-…)")

    # 6) 题注编号：模板的 show 规则会剥掉自动编号，所以编号是手写的。
    #    这里读源码里的 caption，核对「图 N」「表 N」各自从 1 开始连续；
    #    不读 PDF 正文，否则「见图 6」这类引用会被误当成题注。
    for kind in ("图", "表"):
        nums = [int(m) for m in re.findall(
            r"caption:\s*\[\s*" + kind + r"\s*(\d+)", src_text)]
        if nums and nums != list(range(1, len(nums) + 1)):
            problems.append(f"{kind}编号不连续：{nums}")

    if problems:
        return False, "；".join(problems), warn
    return True, f"{page_count(pdf)} 页，封面 {' / '.join(spec['labels'])}", warn


def main():
    bad = 0
    warns = 0
    for spec in REPORTS:
        ok, msg, warn = check_one(spec)
        bad += 0 if ok else 1
        warns += 1 if warn else 0
        print(f"  {'[ok]  ' if ok else '[FAIL]'} {spec['name']:<22} {msg}"
              + (f"\n         \u26a0 {warn}" if warn else ""))
    print(f"\n共 {len(REPORTS)} 份报告，{bad} 份有问题，{warns} 份有中文续行提示")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
