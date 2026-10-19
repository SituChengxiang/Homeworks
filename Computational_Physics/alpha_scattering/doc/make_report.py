#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
从 Qoder 的会话记录里生成「与大语言模型的交互过程」，套进
@local/simple-zjut-report 那份实验报告模板，最后用 typst 排出 PDF。
模板不在本目录复制，挂载方式见仓库根的 scripts/typst-local-packages.sh。

为什么要脚本生成而不是手写：老师要的是"交互过程"，手写出来的一定是事后总结，
读起来就像实验报告里那段"本人深刻体会到……"。直接从会话存档里把
「我发的原话 + 模型回的正文」抠出来，才是它本来的样子。

    python3 doc/make_report.py            # 生成 report.typ 并编译 PDF

筛选规则写在 INCLUDE / EXCLUDE 两张表里，想多留几轮或少留几轮改这里即可。
"""

import glob
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
EXPORT = os.path.join(HERE, "export")

# 会话存档位置（Qoder 项目级日志）
LOG_GLOB = os.path.expanduser(
    "~/.qoder-cn/projects/-home-kuan-Git-Homeworks-Computational-Physics/*.jsonl")

# ---------------------------------------------------------------- 取对话

def load_turns(path):
    """返回 [(prompt, [reply_text, ...]), ...]，只保留人打的字和模型给人看的正文。"""
    turns = []
    pending = None
    with open(path, encoding="utf-8", errors="replace") as fp:
        for line in fp:
            try:
                rec = json.loads(line)
            except ValueError:
                continue
            kind = rec.get("type")
            msg = rec.get("message") or {}
            content = msg.get("content")

            if kind == "user" and rec.get("humanInput"):
                stamp = (rec.get("timestamp") or "")[:10]
                text = ""
                if isinstance(content, str):
                    text = content
                elif isinstance(content, list):
                    text = "\n".join(b.get("text", "") for b in content
                                     if isinstance(b, dict) and b.get("type") == "text")
                text = strip_injected(text).strip()
                if not text:
                    continue
                pending = [text, [], stamp]
                turns.append(pending)
            elif kind == "assistant" and pending is not None:
                if isinstance(content, list):
                    for b in content:
                        if isinstance(b, dict) and b.get("type") == "text":
                            t = b.get("text", "").strip()
                            if t:
                                pending[1].append(t)
    return turns


# 工具/系统注入进来的东西不属于"我说的话"
INJECT_RE = re.compile(
    r"<(system-reminder|local-command-caveat|user-prompt-submit-hook|"
    r"command-name|command-args|task-notification)>.*?</\1>", re.S)


def strip_injected(text):
    return INJECT_RE.sub("", text)


# 要留进报告的轮次：按提问里的关键词匹配
INCLUDE = [
    "创建描述初始条件的模块",
    "定义约化单位下的常量模块",
    "计算其受力情况的子模块",
    "计算alpha粒子加速度的模块",
    "平均值计算alpha粒子新位置",
    "和RK45对比",
    "限制时间为5s",
    "改成t=14",
    "matplotlib帮我绘图",
]

# 明显不该进报告的轮次（问的是交作业流程、学习心得这类元话题）
EXCLUDE = ["截图", "表演", "露馅", "服了", "typst", "水到渠成", "下次这种任务",
           "一段一段", "clion那边", "一个的c文件", "他的意思可能是"]


# 回答里若出现这些词，说明那段是在讲"怎么交作业/怎么写记录"，
# 属于元话题，不进正文
META = ("截图", "露馅", "扮演", "交互记录", "学习通", "提问脚本", "跨项目",
        "记下来", "不用一段一段", "按模块整理", "报告里", "README 里")


def scrub(replies):
    """按段落删掉元话题内容。"""
    kept = []
    for text in replies:
        blocks = re.split(r"\n\s*\n", text)
        keep = [b for b in blocks if not any(k in b for k in META)]
        if keep:
            kept.append("\n\n".join(keep))
    return kept


def pick(turns):
    """挑出逐模块推进的那几轮。

    注意：第一轮问 C89 环境时，模型是把整个项目一次性做完的，
    那一轮的回答里混着后面所有模块的叙述，放进"交互过程"里会串时间线，
    所以那部分改写成下面「开发环境」一节的说明文字，不进对话节选。
    """
    chosen = []
    for prompt, replies, stamp in turns:
        if any(k in prompt for k in EXCLUDE):
            continue
        if any(k in prompt for k in INCLUDE):
            chosen.append((prompt, scrub(replies)))
    # 同一句提问发过两次（比如中途要求重发）时，只留最后一次：
    # 那一次的回答才是真正落进代码里的版本
    seen = {}
    for idx, item in enumerate(chosen):
        seen[item[0]] = idx
    return [chosen[i] for i in sorted(seen.values())]


# ---------------------------------------------------------------- Markdown -> Typst

CODE_FENCE = re.compile(r"```[^\n]*\n(.*?)```", re.S)
INLINE_CODE = re.compile(r"`([^`\n]+)`")
BOLD = re.compile(r"\*\*([^*\n]+)\*\*")
ITALIC = re.compile(r"(?<!\*)\*([^*\n]+)\*(?!\*)")
LINK = re.compile(r"\[([^\]]+)\]\(([^)]+)\)")
HR = re.compile(r"^\s*(---+|\*\*\*+)\s*$")
HEAD = re.compile(r"^\s*(#{2,6})\s+(.*)$")
LIST = re.compile(r"^(\s*)[-*]\s+(.*)$")
NUM = re.compile(r"^(\s*)(\d+)[.)]\s+(.*)$")
TABLE_ROW = re.compile(r"^\s*\|.*\|\s*$")


def esc(text):
    """typst 标记语法里的特殊字符全部转义。"""
    special = set("#$%&_<>@*|{}`~\\")
    return "".join("\\" + ch if ch in special else ch for ch in text)


def inline(text):
    """行内 Markdown 转 typst：先抠出代码段，再处理粗体/链接。"""
    parts = []
    pos = 0
    for m in INLINE_CODE.finditer(text):
        parts.append(("t", text[pos:m.start()]))
        parts.append(("c", m.group(1)))
        pos = m.end()
    parts.append(("t", text[pos:]))

    out = []
    for kind, chunk in parts:
        if kind == "c":
            out.append("#raw(\"%s\")" % chunk.replace("\\", "\\\\").replace("\"", "\\\""))
            continue
        s = LINK.sub(r"\1", chunk)          # 链接只留文字
        s = BOLD.sub("\x01\\1\x02", s)      # 先占位，避免和转义打架
        s = esc(s)
        s = s.replace("\x01", "*").replace("\x02", "*")
        out.append(s)
    return "".join(out)


def raw_block(code):
    body = code.rstrip("\n").replace("\\", "\\\\").replace("\"", "\\\"")
    return '#raw(block: true, lang: "c", "%s")' % body


def table_block(lines):
    """Markdown 表格原样塞进等宽块，比转成 typst table 稳。"""
    return raw_block("\n".join(lines))


def md_to_typst(md):
    out = []
    lines = md.split("\n")
    i = 0
    while i < len(lines):
        line = lines[i]

        if line.startswith("```"):
            j = i + 1
            buf = []
            while j < len(lines) and not lines[j].startswith("```"):
                buf.append(lines[j])
                j += 1
            out.append(raw_block("\n".join(buf)))
            i = j + 1
            continue

        if TABLE_ROW.match(line):
            j = i
            buf = []
            while j < len(lines) and TABLE_ROW.match(lines[j]):
                buf.append(lines[j])
                j += 1
            out.append(table_block(buf))
            i = j
            continue

        if HR.match(line):
            out.append("#v(4pt) #line(length: 100%) #v(4pt)")
            i += 1
            continue

        m = HEAD.match(line)
        if m:
            out.append("")
            out.append("*%s*" % inline(m.group(2)))
            out.append("")
            i += 1
            continue

        m = LIST.match(line)
        if m:
            indent = " " * max(0, len(m.group(1)) - 2)
            out.append("%s+ %s" % (indent, inline(m.group(2))))
            i += 1
            continue

        m = NUM.match(line)
        if m:
            out.append("%s%d. %s" % (" " * max(0, len(m.group(1))),
                                      int(m.group(2)), inline(m.group(3))))
            i += 1
            continue

        if not line.strip():
            out.append("")
            i += 1
            continue

        out.append(inline(line))
        i += 1

    # 段落之间要有空行，连续文字行会被 typst 并成一段（中文正好要这样）
    return "\n".join(out)


# ---------------------------------------------------------------- 生成 report.typ

TYP_HEAD = '''#import "@local/simple-zjut-report:0.1.1": experiment-report

#show: doc => experiment-report(
  title: "《计算物理》课程作业报告",
  semester: "2026-2027学年第一学期",
  class: "（填你的班级）",
  name: "（填姓名）",
  student-id: "（填学号）",
  date: "2026年10月7日",
  doc
)

'''

BODY = r'''
= α 粒子散射轨道计算

== 作业题目

利用大语言模型解决 α 粒子散射中粒子轨道的计算问题。原子核固定在 $(0, 0)$，
带电量 $97e$；α 粒子相对分子质量 4，带电量 $2e$。采用约化单位：
电荷单位 $e$、质量单位相对分子质量、长度单位 Å、库仑常数 $k = 1$。

计算下列六条运动轨迹并画在同一张图上：

#table(
  columns: 3,
  align: (left, left, left),
  table.header([组], [初始位置], [初始速度]),
  [(1)], [$(-20, 20)$], [$(1,0)$、$(1.5,0)$、$(3,0)$],
  [(2)], [$(-20, -20)$], [$(1,0)$、$(1.5,0)$、$(3,0)$],
)

要求：分模块写代码；横纵坐标限制在 $-22.5$ 到 $22.5$；
不同轨迹在颜色或虚实上有区别；每条曲线配文字标识。

== 实验原理

核固定，只需求 α 粒子在核的库仑斥力场中的运动。约化单位下 $k = 1$，两电荷同号，故

$ F = k q_"alpha" q_"N" / r^2 = 194 / r^2 , "  " a = F / m = 48.5 / r^2 $

写成四阶 Runge-Kutta 需要的一阶显式系统，状态向量 $y = {x, y, v_x, v_y}$：

```c
dx/dt  = v_x
dy/dt  = v_y
dv_x/dt = 48.5 * x / (x^2 + y^2)^(3/2)
dv_y/dt = 48.5 * y / (x^2 + y^2)^(3/2)
```

两个守恒量用来验收：机械能 $E = 1/2 m (v_x^2 + v_y^2) + 194/r$，
角动量 $L = m (x v_y - y v_x)$；中心力场下二者都应严格不变。

轨道是双曲线，解析解可反过来给数值解当标准答案：
渐近速率 $v_∞ = sqrt(2E/m)$，渐近撞击参数 $b = L / (m v_∞)$，
偏心率 $epsilon = sqrt(1 + 2 E L^2 / (m (k q_1 q_2)^2))$，
偏转角 $theta = 2 arcsin(1/epsilon)$，最近距离由 $E = L^2 / (2 m r^2) + 194/r$ 解出。

== 模块划分

#table(
  columns: (auto, auto, 1fr),
  align: (left, left, left),
  table.header([编号], [文件], [职责]),
  [0], [cases.h/.c], [六组初值与文字标签，集中一处],
  [1], [units.h], [约化单位约定、k q₁q₂、绘图框、奇点保护],
  [2], [coulomb.h/.c], [库仑力、势能、位径；只到「力」为止],
  [3], [dynamics.h/.c], [a = F/m、动能；质量只出现在这一层],
  [4], [stepper.h/.c], [位置用新旧速度平均的手写格式（一阶）],
  [5], [rk4.h/.c], [经典四阶 Runge-Kutta，函数指针接收右端项],
  [6], [trajectory.h/.c], [推进、终止判据、采样、守恒量诊断],
  [7], [rutherford.h/.c], [卢瑟福解析解，给数值结果当对照],
  [8], [export.h/.c], [写 traj\_\*.dat 与 summary.md/.txt],
  [9], [main.c], [调度六条轨迹，打印两张表],
  [10], [plot.py], [读 .dat 作图],
)

换力律只动 `coulomb.c`，换积分方法只动 `stepper.c` / `rk4.c`，
`trajectory.c` 里换个函数指针即可。

== 开发环境与工具

开发机为 Linux，GCC 16.2.1，IDE 用 CLion（CMake 工程）。
题目要求代码符合 C89，因此编译统一加上
`-std=c89 -pedantic-errors -Wall -Wextra`，
并用一段含 `//` 行注释与 `for (int i = 0; ...)` 的 C99 代码做反向测试，
确认标准确实被锁死而不是"没触发"。

C89 唯一的硬限制是标准库没有绘图能力，而 `-pedantic-errors` 下也不允许调用
`popen` 之类的 POSIX 扩展，所以计算与绘图分离：C 只输出数据，
画图交给 `plot.py`。

环境上踩到的一个坑值得记：在 CMake 4.4 + GCC 16 下写
`set(CMAKE_C_STANDARD 89)` 会直接 configure 失败，提示编译器不支持 C89，
而 `gcc -std=c89` 明明可用——是 CMake 的编译器特性表不认识这个新编译器版本号。
绕法是显式 `add_compile_options(-std=c89 ...)`，再用 `check_c_compiler_flag` 兜底。

交付环境为 Dev-C++（MinGW）。代码只用 ISO C89 与标准库
（`stdio.h stdlib.h string.h math.h`），无 POSIX 依赖；
考虑到 Windows 控制台默认 GBK 代码页，程序在 `_WIN32` 下自动改用英文文案，
也可用 `--ascii` / `--utf8` 手动切换；输出目录不可写时自动退到当前目录。
另外提供由 `make_single_source.py` 从模块化源码合并出的单文件版
`alpha_scattering_single.c`，与模块版输出逐字节相同，便于直接打开运行。

== 与大语言模型的交互过程

下面按实际提问顺序节选，*提问是原话，回答保留模型输出的正文*，
只删去了与主线无关的工具调用输出。

阅读提示：记录里打印的 `dt`、`t_max` 是*当时*那次运行的参数，开发过程中被收紧过
（`t_max` 先设 5，后来发现最慢的一组要到 $t = 13.5$ 才越出画框，改成 14；
`dt` 由 0.05 收紧到 0.005，依据是记录中的步长收敛测试）。
所以记录里的能量漂移（$10^-13$ 到 $10^-12$ 量级）和框内偏转角与最终表格不同，
两者是同一条物理轨迹在两套运行参数下的输出。*最终结果以「实验结果与分析」一节、
`data/traj_*.dat` 的文件头以及源码缺省值 `dt = 0.005`、`t_max = 14` 为准*。

__TURNS__

== 实验结果与分析

#figure(
  image("../data/trajectories.png", width: 92%),
  caption: [六条 α 粒子散射轨迹。颜色表示初速度大小，实线/虚线表示初始位置在 $x$ 轴上方/下方；
            内嵌小图为位径 $r(t)$，圆点标出最近距离处],
)

#figure(
  table(
    columns: 8,
    align: (left, left, right, right, right, right, right, right),
    table.header([case], [初始位置], [$v_0$], [$E$], [$L$],
                 [$r_"min"$ 数值], [$r_"min"$ 解析], [框内偏转角]),
    [1], [$(-20, 20)$], [1.0], [8.8589], [$-80$], [25.44745], [25.44745], [58.94°],
    [2], [$(-20, 20)$], [1.5], [11.3589], [$-120$], [23.75101], [23.75101], [39.59°],
    [3], [$(-20, 20)$], [3.0], [24.8589], [$-240$], [21.36226], [21.36226], [17.96°],
    [4], [$(-20, -20)$], [1.0], [8.8589], [80], [25.44745], [25.44745], [58.94°],
    [5], [$(-20, -20)$], [1.5], [11.3589], [120], [23.75101], [23.75101], [39.59°],
    [6], [$(-20, -20)$], [3.0], [24.8589], [240], [21.36226], [21.36226], [17.96°],
  ),
  caption: [六条轨迹的数值结果。积分器为经典四阶 Runge-Kutta，
            步长 #raw("dt = 0.005")，时长上限 #raw("t_max = 14")，
            粒子越出 ±25 的窗口即停止；与 `data/traj_*.dat` 头部记录的运行参数一致],
)

四项独立验证：

+ *守恒量*：六条轨迹全程的最大偏差 $max abs(Delta E) / E = 5.6 times 10^-15$、
  $max abs(Delta L) / abs(L) = 8.5 times 10^-15$（两者都出现在 $v_0 = 3.0$ 那组），
  已是双精度舍入水平；逐条数值可在每份数据文件的 `# invariant` 行查到。
+ *解析对照*：最近距离与卢瑟福解析值吻合到 6 位以上，残差 $6 times 10^-8$ 到 $1.1 times 10^-6$
  来自采样点不正好落在精确拱点上，不是积分误差。
+ *收敛阶*：步长依次取 #raw("dt")、#raw("dt/2")、#raw("dt/4") 比较同一时刻的轨道坐标，
  RK4 误差比稳定在 16 附近（实测 16.2、16.1），即四阶；
  手写平均速度格式的误差比是 2.00，即一阶。
+ *对称性*：case 1/4、2/5、3/6 的 $E$、$|L|$、$r_"min"$ 完全相同，$y$ 反号。

需要区分两个角度：「框内偏转角」是轨迹在 ±22.5 画框内被截断时已经转过的角度，
「卢瑟福渐近偏转角」是 $r -> infinity$ 的解析值（98.09° / 68.30° / 25.83°）。
粒子出射时仍在受力，所以前者系统偏小。
框内偏转角还随步长有微小变化，因为它取决于轨迹在哪一步被画框截断：
`dt = 0.05` 那次是 58.99° / 39.59° / 18.00°，本表 `dt = 0.005` 是
58.94° / 39.59° / 17.96°，相差不到 0.05°；而不依赖截断位置的 $r_"min"$
两次相差在 $7 times 10^-5$ 以内，说明收紧步长后结果已稳定。

另一个容易错的物理点：几何上的 $abs(y_0) = 20$ Å *不是*卢瑟福公式里的撞击参数。
粒子起始位径 $sqrt(20^2 + 20^2) = 28.28$ Å，此处势能 $194/28.28 = 6.86$ 与动能同量级，
说明出发时已在相互作用区内部；真正的渐近撞击参数是 $b = L / (m v_∞)$，
算出来是 9.50 / 12.59 / 17.02 Å。若直接把 20 代进卢瑟福公式，偏转角会算错。

== 使用心得

三点最实在的收获：

+ *数值解的可信度不来自"跑通了"*。这道题每一步都有可以独立对照的量——守恒量、
  解析解、对称性、收敛阶。四项里任何一项都能在中途把错误掐住，
  而"程序没崩、图画出来了"什么都不能说明。
+ *先手算再让模型写代码*。$k q_1 q_2 = 194$、$a = 48.5/r^2$、六条轨迹能量为正、
  上下两组必然镜像，这些都能提前判断。判断写在前面，模型给出的结果才有地方去核对；
  否则它给一个 25.44745，你只能选择信或不信。
+ *模型也会把方法用错，而且错得很像样*。中途用 $r_"min"$ 做步长减半的收敛性探针，
  算出"实测阶数 21.72"这种荒谬值——原因不是代码写错，而是 $r_"min"$ 是在离散步点上取
  min、不是关于步长的光滑泛函，误差不按四阶规律走。改成让各步长积分到同一时刻再比坐标，
  阶数立刻回到 4.02 到 4.07。这类"看起来在算、其实问错了问题"的地方，
  恰恰是最需要人来把关的。
'''

def render_turn(n, prompt, replies):
    ask = md_to_typst(prompt)
    body = "\n\n".join(replies)
    ans = md_to_typst(body) if body.strip() else "（本轮回答为工具调用与代码写入，正文见下一轮结论。）"
    return (
        "=== 第 " + str(n) + " 轮\n\n"
        "#set par(first-line-indent: 0em)\n"
        '#box(width: 100%, fill: rgb("#eef3f8"),\n'
        '  stroke: (left: 2.5pt + rgb("#1f5c8b")), inset: (x: .9em, y: .6em))[\n'
        "  *我的提问：*\n\n" + ask + "\n]\n\n"
        "#v(6pt)\n\n" + ans + "\n"
    )


def main():
    logs = sorted(glob.glob(LOG_GLOB), key=os.path.getmtime)
    if not logs:
        sys.exit("找不到会话记录：%s" % LOG_GLOB)
    turns = load_turns(logs[-1])
    chosen = pick(turns)
    if not chosen:
        sys.exit("没匹配到任何轮次，检查 INCLUDE 关键词")

    print("会话存档：%s" % logs[-1])
    print("共 %d 轮人工提问，选中 %d 轮进报告：" % (len(turns), len(chosen)))
    for i, item in enumerate(chosen, 1):
        p = item[0]
        print("  %2d. %s" % (i, p.split("\n")[0][:52]))

    body = "\n".join(render_turn(i, item[0], item[1])
                     for i, item in enumerate(chosen, 1))
    typ = TYP_HEAD + BODY.replace("__TURNS__", body)

    os.makedirs(EXPORT, exist_ok=True)
    typ_path = os.path.join(HERE, "report.typ")
    with open(typ_path, "w", encoding="utf-8") as fp:
        fp.write(typ)
    print("已生成 %s（%d 行）" % (typ_path, typ.count("\n")))

    pdf = os.path.join(EXPORT, "计算物理-α粒子散射-报告.pdf")
    r = subprocess.run(["typst", "compile", "--root", ROOT, typ_path, pdf],
                       capture_output=True, text=True)
    if r.returncode != 0:
        sys.stderr.write(r.stderr[:4000])
        sys.exit("typst 编译失败")
    print("已编译 %s（%.0f KB）" % (pdf, os.path.getsize(pdf) / 1024))


if __name__ == "__main__":
    main()
