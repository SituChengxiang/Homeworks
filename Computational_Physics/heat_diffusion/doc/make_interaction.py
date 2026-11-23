#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从 Qoder 会话存档生成热传导作业的交互过程（Typst 片段）。

输出 doc/interaction.typ，由 doc/report.typ 通过 #include 引入。
会话存档位于 ~/.qoder-cn/projects/-home-kuan-Git-Homeworks-Computational-Physics/*.jsonl，
脚本按提问内容自动选取热传导那一份，只保留与技术实现直接相关的轮次。

    python3 doc/make_interaction.py
"""

import glob
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
LOG_DIR = os.path.expanduser(
    "~/.qoder-cn/projects/-home-kuan-Git-Homeworks-Computational-Physics")
OUT = os.path.join(HERE, "interaction.typ")

# 用于识别"这是热传导作业的会话"
KEY = ("铁棍子", "热传导", "FTCS", "Crank", "继续步骤", "继续第",
       "英文版", "iron rod")
# 只保留技术轮次；元话题（写报告、提交、README、交互记录本身）不进附录。
# 注意不要用"提交"做排除词：题目原文 (c)(d) 就写着"提交"。
META = ("写一点报告", "继续吧", "commit", "check_reports",
        "AGENTS", "README", "作业2", "交流记录")

TITLES = [
    "题目与参数选择",
    "参数、材料与 FTCS 推进",
    "英文版原题核对",
    "步骤 3：Fourier 级数解析解",
    "步骤 4：Crank–Nicolson 对照",
    "步骤 5：收敛判据与数据导出",
    "步骤 6：代入式核对",
    "步骤 7：交付图",
    "步骤 8：三层验证与单文件版",
]


# ---------------------------------------------------------------- 取对话

INJECT_RE = re.compile(
    r"<(system-reminder|local-command-caveat|user-prompt-submit-hook|"
    r"command-name|command-args|task-notification)>.*?</\1>", re.S)


def strip_injected(text):
    return INJECT_RE.sub("", text)


# 会话原文里夹着不少本机绝对路径（我回答时引用文件用了全路径）。
# 提交稿不该带上用户名和目录布局，生成时一律折成相对路径。
HOME = os.path.expanduser("~")
REPO = HOME + "/Git/Homeworks/"
PROJ = REPO + "Computational_Physics/heat_diffusion/"
RUNTMP_RE = re.compile(re.escape(HOME) + r"/\.qoder-cn/tmp/[^\s`\])|]*")


def scrub(text):
    """去掉输出里的本机绝对路径。"""
    text = RUNTMP_RE.sub("/tmp", text)
    text = text.replace(PROJ, "")
    text = text.replace(REPO, "Homeworks/")
    return text.replace(HOME + "/", "~/")


def load_turns(path):
    """返回 [(prompt, [reply_text, ...], stamp), ...]。"""
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


def find_session():
    """按提问里的关键词给每份存档打分，取最匹配的一份。"""
    best_path, best_turns, best_score = None, [], -1
    for path in glob.glob(os.path.join(LOG_DIR, "*.jsonl")):
        turns = load_turns(path)
        score = sum(1 for p, _, _ in turns if any(k in p for k in KEY))
        if score > best_score:
            best_path, best_turns, best_score = path, turns, score
    if best_score <= 0:
        sys.exit("找不到热传导作业的会话存档：%s/*.jsonl" % LOG_DIR)
    return best_path, best_turns


def pick(turns):
    chosen = []
    for prompt, replies, stamp in turns:
        p = prompt.strip()
        if any(k in p for k in META):
            continue
        if p == "继续" or any(k in p for k in KEY):
            chosen.append((prompt, replies, stamp))
    return chosen


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
CJK = re.compile(r"[\u3000-\u303f\u4e00-\u9fff\uff00-\uffef]")


def esc(text):
    special = set("#$%&_<>@*|{}[]`~\\")
    return "".join("\\" + ch if ch in special else ch for ch in text)


# ---------------------------------------------------------------- LaTeX -> Typst
#
# 会话里的公式是 LaTeX 写的。直接转义会让 PDF 印出 "$$\\frac..." 这样的字面串，
# 读的人看到的是反斜杠而不是公式。这里把它们换成 Typst 数学。
# 判据：转换结果里若还剩反斜杠、花括号或 $，就认为没转干净，整段退回原样
# （宁可继续印字面 LaTeX，也不要产出一个编不过的 .typ）。

TEX_MACRO = {
    "alpha": "alpha", "beta": "beta", "gamma": "gamma", "delta": "delta",
    "Delta": "Delta", "epsilon": "epsilon", "varepsilon": "epsilon",
    "eta": "eta", "theta": "theta", "lambda": "lambda", "mu": "mu",
    "nu": "nu", "rho": "rho", "sigma": "sigma", "Sigma": "Sigma",
    "tau": "tau", "phi": "phi", "pi": "pi", "omega": "omega",
    "Omega": "Omega", "infty": "oo", "partial": "diff",
    "le": "<=", "leq": "<=", "ge": ">=", "geq": ">=", "neq": "!=",
    "ne": "!=", "approx": "approx", "sim": "similar", "propto": "prop",
    "ll": "<<", "gg": ">>", "times": "times", "cdot": "dot.op",
    "div": "div", "pm": "plus.minus", "mp": "minus.plus",
    "sum": "sum", "prod": "prod", "int": "integral", "oint": "contour.integral",
    "in": "element", "notin": "not.element", "subset": "subset",
    "subseteq": "subset.eq", "cup": "union", "cap": "sect",
    "to": "arrow.r", "rightarrow": "arrow.r", "Rightarrow": "dblarrow.r",
    "leftrightarrow": "arrow.l.r", "mapsto": "arrow.r.bar",
    "sin": "sin", "cos": "cos", "tan": "tan", "exp": "exp",
    "ln": "ln", "log": "log", "max": "max", "min": "min",
    "sup": "sup", "gcd": "gcd", "quad": "quad", "qquad": "quad",
    "ldots": "dots.c", "dots": "dots.c", "prime": "prime",
    "circ": "compose", "star": "star", "circledast": "star.circle",
    "lvert": "|", "rvert": "|", "vert": "|", "langle": "angle.l",
    "rangle": "angle.r", "hat": "hat", "bar": "macron", "vec": "arrow",
}
# \, \; \! \  \> 这类只是间距，Typst 里用空格即可（删掉会把两个标识符粘成一个）
TEX_SPACE = {",", ";", "!", ">", " ", "//"}
TEX_DROP = {"left", "right", "bigl", "bigr", "Bigl", "Bigr", "biggl",
            "biggr", "Big", "big", "displaystyle", "limits", "nolimits",
            "mathrm", "mathbf", "mathit", "operatorname", "centering"}
TEX_RE = re.compile(r"\\([a-zA-Z]+|.)")
# Typst 数学里合法的标识符 / 函数 / 符号名（含上表输出），其余多字母串拆成单字母
TY_IDENT = set(TEX_MACRO.values()) | {
    "frac", "floor", "ceil", "round", "sqrt", "binom", "mat", "vec", "cases",
    "align", "lr", "text", "upright", "italic", "bold", "dot", "op", "dots",
    "arrow", "hat", "macron", "accent", "tilde", "grave", "breve", "check",
    "similar", "prop", "element", "not", "and", "or", "union", "sect",
    "compose", "star", "circle", "angle", "prime", "gcd", "sup", "inf",
    "exp", "log", "ln", "sin", "cos", "tan", "cot", "sec", "csc", "max",
    "min", "sum", "prod", "diff", "limits", "scripts", "attach", "size",
    "stretch", "adjust", "medium", "thin", "big", "quad", "space", "line",
    "root", "index", "paren", "brack", "brace", "bb", "cal", "frak", "goth",
    "sans", "mono", "serif", "binom", "choose", "equiv", "colon", "mid",
}
# 符号名带点的（arrow.r、dblarrow.r、not.element…）按段登记，
# 否则拆分规则会把 dblarrow 拆成 d b l a r r o w
for _v in set(TEX_MACRO.values()):
    for _p in re.split(r"[.\-]", _v):
        if _p.isalpha():
            TY_IDENT.add(_p)
MATH_RE = re.compile(r"\$\$([^$]+?)\$\$|\$([^$\n]{1,200})\$")
MATH_DISP = re.compile(r"^\s*\$\$(.+?)\$\$\s*$")
CJK_PUNCT = "，。、；：（）「」『』……—～　"
# 正文里的 LaTeX 转义：\| 想表达竖线本身，\, \; 是间距。留着反斜杠会照印出来。
LATEX_PUNCT = re.compile(r"\\([|<>,;\[\]])")


def looks_like_math(body):
    """闸门：正文里两个 $ 之间的散文不是数学。
    带 CJK 标点就不是；含汉字又没有反斜杠命令的也不是（真含汉字的都来自 \text{}）。"""
    if any(ch in CJK_PUNCT for ch in body):
        return False
    if re.search(r"[\u4e00-\u9fff]", body) and "\\" not in body:
        return False
    return True


def _group(s, i):
    """s[i] == '{'，返回 (内容, 右括号之后的位置)；不配对返回 (None, i)。"""
    if i >= len(s) or s[i] != "{":
        return None, i
    depth = 0
    for j in range(i, len(s)):
        if s[j] == "{":
            depth += 1
        elif s[j] == "}":
            depth -= 1
            if depth == 0:
                return s[i + 1:j], j + 1
    return None, i


def _arg(s, i):
    """读一个 LaTeX 参数：{...} 或单个记号（字母/数字/命令）。"""
    while i < len(s) and s[i] == " ":
        i += 1
    if i >= len(s):
        return None, i
    if s[i] == "{":
        body, j = _group(s, i)
        if body is None:
            return None, i
        return _tex2typst(body), j
    m = re.match(r"\\[a-zA-Z]+|\\.|[0-9a-zA-Z]", s[i:])
    if not m:
        return None, i
    tok = m.group(0)
    if tok.startswith("\\"):
        return _tex2typst(tok), i + len(tok)
    return tok, i + 1


def _two_args(s, i, name):
    a, i = _arg(s, i)
    if a is None:
        return None, i
    b, i = _arg(s, i)
    if b is None:
        return None, i
    return "%s(%s, %s)" % (name, a, b), i


def _split_unknown_words(s):
    """Typst 把连续字母当标识符：LaTeX 的隐式乘法 uv、dt 会报 unknown variable。
    白名单以外的多字母串拆成单字母（u v、d t 正好是原意）。"..." 里的不动。"""
    quoted = []

    def mask(m):
        quoted.append(m.group(0))
        return "\x04%d\x04" % (len(quoted) - 1)

    s = re.sub(r'"[^"]*"', mask, s)

    def rep(m):
        w = m.group(0)
        return w if w in TY_IDENT else " ".join(w)

    s = re.sub(r"[a-zA-Z]+", rep, s)
    return re.sub("\x04(\\d+)\x04", lambda m: quoted[int(m.group(1))], s)


def _tex2typst(s):
    """LaTeX 数学串 -> Typst 数学串的内部实现；转不干净返回 None。"""
    # \text{...} -> "..."（中文和单位都是这样进来的）
    while True:
        k = s.find("\\text{")
        if k < 0:
            k = s.find("\\mathrm{")
        if k < 0:
            break
        head = k + (6 if s[k:k + 6] == "\\text{" else 8)
        body, j = _group(s, head - 1)
        if body is None:
            return None
        s = s[:k] + '"%s"' % body + s[j:]
    # \frac / \tfrac / \dfrac
    for name in ("frac", "tfrac", "dfrac"):
        pat = "\\\\" + name + r"\b"
        while re.search(pat, s):
            m = re.search(pat, s)
            out, k = _two_args(s, m.end(), "frac")
            if out is None:
                return None
            # 两侧留空格：否则 \sin\frac{..}{..} 会变成 \sinfrac(...) 认不出来
            s = s[:m.start()] + " " + out + " " + s[k:]
    # \lfloor x \rfloor -> floor(x)
    m = re.search(r"\\lfloor\s*(.+?)\s*\\rfloor", s, re.S)
    if m:
        s = s[:m.start()] + " floor(" + _tex2typst(m.group(1)) + ") " + s[m.end():]
    # \hat{x} \vec{x} \bar{x} -> hat(x) arrow(x) macron(x)
    for name, fn in (("hat", "hat"), ("vec", "arrow"), ("bar", "macron"),
                     ("overline", "macron")):
        pat = r"\\%s\s*\{([^{}]*)\}" % name
        while re.search(pat, s):
            m = re.search(pat, s)
            s = s[:m.start()] + " %s(%s) " % (fn, m.group(1)) + s[m.end():]
    # 上下标的花括号 -> 圆括号（Typst 数学用 ^(...) /_(...)）
    s = re.sub(r"([\^_])\{([^{}]*)\}", r"\1(\2)", s)
    # 剩下的命令查表
    def sub_cmd(m):
        tok = m.group(1)
        if tok in TEX_SPACE:
            return " "
        if tok in TEX_DROP:
            return ""
        if tok in TEX_MACRO:
            return " " + TEX_MACRO[tok] + " "
        if tok == "%":
            return "%"            # Typst 的 % 不是注释符，数学里直接印百分号
        if tok == "|":
            return " | "          # LaTeX 的 \| 在这里都是绝对值竖线
        return None               # 认不出的一律退回原样，不猜
    def mark(m):
        r = sub_cmd(m)
        if r is None:
            return "\x00"            # 空串是合法替换（\left），不能当失败
        return r
    out = TEX_RE.sub(mark, s)
    if "\x00" in out:
        return None                                  # 有认不出的命令
    if "\\" in out or "{" in out or "}" in out or "$" in out:
        return None                                  # 没转干净，退回原样
    out = _split_unknown_words(out)
    out = re.sub(r"[ \t]+", " ", out).strip()
    return out or None


def tex2typst_math(src):
    """入口：先判「这是不是数学」，再转换。内部片段不过这道闸。"""
    if not looks_like_math(src):
        return None
    return _tex2typst(src)


def protect_math(s):
    """把数学片段摘成占位符，返回 (新串, Typst 片段列表)。"""
    reps = []

    def sub(m):
        body = (m.group(1) if m.group(1) is not None else m.group(2)).strip()
        ty = tex2typst_math(body)
        if ty is None:
            return m.group(0)
        # Typst 里 "$ x $"（$ 后带空格）是显示式，会另起一行；行内必须写 "$x$"。
        # 真正的显示式由 md_to_typst 的整行 $$…$$ 分支负责。
        reps.append("$%s$" % ty)
        return "\x03%d\x03" % (len(reps) - 1)

    return MATH_RE.sub(sub, s), reps


def restore_math(s, reps):
    return re.sub("\x03(\\d+)\x03", lambda m: reps[int(m.group(1))], s)


def inline(text):
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
        s, reps = protect_math(chunk)
        s = LATEX_PUNCT.sub(lambda m: " " if m.group(1) in ",;!" else m.group(1), s)
        s = LINK.sub(r"\1", s)
        s = BOLD.sub("\x01\\1\x02", s)
        s = esc(s)
        s = s.replace("\x01", "*").replace("\x02", "*")
        s = restore_math(s, reps)
        out.append(s)
    return "".join(out)


def raw_block(code):
    body = code.rstrip("\n").replace("\\", "\\\\").replace("\"", "\\\"")
    return '#raw(block: true, "%s")' % body


def smart_join(lines):
    """把硬折行的段落拼回一行；中文相邻不加空格，西文之间保留一个空格。"""
    s = lines[0].strip()
    for nxt in lines[1:]:
        nxt = nxt.strip()
        if not nxt:
            continue
        if CJK.match(s[-1]) and CJK.match(nxt[0]):
            s += nxt
        else:
            s += " " + nxt
    return s


def is_special(line):
    return bool(CODE_FENCE.match(line) or line.startswith("```")
                or MATH_DISP.match(line)
                or TABLE_ROW.match(line) or HR.match(line) or HEAD.match(line)
                or LIST.match(line) or NUM.match(line))


def split_row(line):
    """markdown 表格一行 -> 单元格列表。\\| 是绝对值竖线，不能当分隔符。"""
    s = line.strip()
    if s.startswith("|"):
        s = s[1:]
    if s.endswith("|"):
        s = s[:-1]
    s = s.replace("\\|", "\x05")
    return [c.replace("\x05", "\\|").strip() for c in s.split("|")]


def is_sep_row(cells):
    return bool(cells) and all(re.fullmatch(r":?-{2,}:?", c) for c in cells if c)


def md_table(rows):
    """markdown 表格 -> Typst #table；单元格走 inline()，公式与粗体随之生效。"""
    parsed = [split_row(r) for r in rows]
    parsed = [p for p in parsed if not is_sep_row(p)]
    if not parsed:
        return None
    ncol = max(len(p) for p in parsed)
    if ncol < 2:
        return None
    cells = []
    for p in parsed:
        p = p + [""] * (ncol - len(p))
        for c in p:
            body = inline(c)
            cells.append("[" + body + "]")
    return (
        "#block(inset: 0pt, above: 4pt, below: 6pt)["
        "#set text(size: 8.5pt)\n"
        "#show table.cell.where(y: 0): set text(weight: 650)\n"
        "#table(columns: (%s), inset: (x: 4pt, y: 2.5pt), stroke: 0.4pt + luma(180), %s)]"
        % (", ".join(["1fr"] * ncol), ", ".join(cells))
    )


def md_to_typst(md):
    out = []
    lines = scrub(md).split("\n")
    i = 0
    while i < len(lines):
        line = lines[i]

        m = MATH_DISP.match(line)
        if m:
            ty = tex2typst_math(m.group(1).strip())
            if ty is not None:
                out.append("$\n  %s\n$" % ty)
                i += 1
                continue

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
            rows = []
            while j < len(lines) and TABLE_ROW.match(lines[j]):
                rows.append(lines[j])
                j += 1
            tbl = md_table(rows)
            out.append(tbl if tbl else raw_block("\n".join(rows)))
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

        buf = [line]
        j = i + 1
        while j < len(lines) and lines[j].strip() and not is_special(lines[j]):
            buf.append(lines[j])
            j += 1
        out.append(inline(smart_join(buf)))
        i = j

    return "\n".join(out)


def render_turn(n, title, prompt, replies):
    ask = md_to_typst(prompt)
    body = "\n\n".join(replies)
    ans = md_to_typst(body) if body.strip() else "（本轮无正文输出。）"
    head = "=== 第 %d 轮" % n
    if title:
        head += "：%s" % title
    return (
        head + "\n\n"
        '#box(width: 100%, fill: rgb("#eef3f8"), '
        'stroke: (left: 2.5pt + rgb("#1f5c8b")), inset: (x: .9em, y: .6em))[\n'
        '  #set par(first-line-indent: 0em)\n'
        "  *我的提问：*\n\n" + ask + "\n]\n\n"
        "#v(6pt)\n\n" + ans + "\n"
    )


def main():
    path, turns = find_session()
    chosen = pick(turns)
    if not chosen:
        sys.exit("没有匹配到任何技术轮次")

    print("会话存档：%s" % path)
    print("共 %d 轮提问，选中 %d 轮：" % (len(turns), len(chosen)))
    for i, (prompt, replies, stamp) in enumerate(chosen, 1):
        print("  %2d. %s  %s" % (i, stamp, prompt.split("\n")[0][:48]))

    parts = ["// 由 doc/make_interaction.py 从 Qoder 会话存档生成，请勿手改。",
             "// 存档：~/.qoder-cn/projects/…/%s" % os.path.basename(path),
             ""]
    for i, (prompt, replies, stamp) in enumerate(chosen, 1):
        title = TITLES[i - 1] if i <= len(TITLES) else ""
        parts.append(render_turn(i, title, prompt, replies))

    text = "\n".join(parts)
    with open(OUT, "w", encoding="utf-8") as fp:
        fp.write(text)
    print("已生成 %s（%d 行）" % (OUT, text.count("\n") + 1))


if __name__ == "__main__":
    main()