#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""提交前隐私核查：扫描不该进仓库的实名等敏感串。

模式清单在仓库之外（默认 ~/.config/homeworks/privacy-patterns，可用
HOMEWORKS_PRIVACY_PATTERNS 覆盖）。清单本身绝不提交，否则这个脚本就成了
新的泄露点。输出只报「第 N 号模式」并把命中处掩码掉，真名不会出现在
终端、日志或截图里。

    python3 scripts/check_privacy.py              # 查暂存内容（提交前）
    python3 scripts/check_privacy.py --worktree    # 查全部已跟踪文件 + 本地产物
    python3 scripts/check_privacy.py --history     # 查所有 ref 的历史提交

退出码：0 干净，1 命中，2 配置缺失。
"""

import os
import re
import subprocess
import sys

DEFAULT_PATTERNS = os.path.expanduser("~/.config/homeworks/privacy-patterns")
MASK = "██"


def run(args, **kw):
    return subprocess.run(args, capture_output=True, text=True,
                          errors="replace", **kw).stdout


def load_patterns():
    path = os.environ.get("HOMEWORKS_PRIVACY_PATTERNS", DEFAULT_PATTERNS)
    if not os.path.exists(path):
        sys.stderr.write(
            "找不到模式清单：%s\n"
            "先建它（仓库外、权限 600），一行一个子串，regex: 前缀走正则。\n" % path)
        sys.exit(2)
    out = []
    with open(path, encoding="utf-8") as fp:
        for line in fp:
            line = line.rstrip("\n")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            if line.startswith("regex:"):
                out.append((line, re.compile(line[6:])))
            else:
                out.append((line, re.compile(re.escape(line))))
    if not out:
        sys.stderr.write("模式清单是空的：%s\n" % path)
        sys.exit(2)
    return out


def mask_context(text, match_start, match_end, width=42):
    lo = max(0, match_start - width)
    hi = min(len(text), match_end + width)
    frag = text[lo:hi].replace("\n", "↵")
    # 把命中处本身遮掉，只留下上下文
    return frag[: max(0, match_start - lo)] + MASK + frag[match_end - lo:]


def scan_text(label, text, patterns, hits):
    for idx, (_raw, rx) in enumerate(patterns, 1):
        m = rx.search(text)
        if m:
            hits.append((idx, label, mask_context(text, m.start(), m.end())))


def is_probably_binary(blob):
    return b"\x00" in blob[:4096]


def pdf_text(path):
    if not path.lower().endswith(".pdf"):
        return ""
    if subprocess.run(["bash", "-c", "command -v pdftotext"],
                      capture_output=True).returncode:
        return ""
    return run(["pdftotext", path, "-"])


def check_staged(patterns):
    hits = []
    paths = run(["git", "-c", "core.quotePath=false", "diff", "--cached",
                 "--name-only", "--diff-filter=ACMR"]).split("\n")
    for p in [x for x in paths if x]:
        scan_text("文件名 " + p, p, patterns, hits)
        blob = subprocess.run(["git", "show", ":" + p], capture_output=True).stdout
        if is_probably_binary(blob):
            continue
        scan_text(p, blob.decode("utf-8", "replace"), patterns, hits)
    return hits, len([x for x in paths if x])


def check_worktree(patterns):
    hits = []
    tracked = [x for x in run(["git", "-c", "core.quotePath=false",
                               "ls-files"]).split("\n") if x]
    for p in tracked:
        scan_text("文件名 " + p, p, patterns, hits)
    scanned = 0
    for p in tracked:
        if not os.path.isfile(p):        # 空 gitlink（stdserve-Back_end/Back-end）不是文件
            continue
        with open(p, "rb") as fp:
            blob = fp.read(4096)
        if is_probably_binary(blob) and not p.lower().endswith(".pdf"):
            continue
        text = pdf_text(p) if p.lower().endswith(".pdf") else \
            open(p, encoding="utf-8", errors="replace").read()
        scanned += 1
        scan_text(p, text, patterns, hits)
    # 交付产物按政策不入库，但本地有没有带名字仍然值得知道：单独列一类
    artifacts = []
    for root, dirs, files in os.walk("."):
        dirs[:] = [d for d in dirs if d not in (".git", "build", "__pycache__",
                                                "node_modules")]
        for f in files:
            if not f.lower().endswith(".pdf"):
                continue
            rel = os.path.join(root, f).lstrip("./")
            if rel in tracked:
                continue
            text = pdf_text(rel)
            found = [i for i, (_r, rx) in enumerate(patterns, 1) if rx.search(text)]
            if found:
                artifacts.append((rel, found))
    return hits, scanned, artifacts


def check_history(patterns):
    hits = []
    refs = [x for x in run(["git", "for-each-ref", "--format=%(refname)",
                            "refs/heads", "refs/tags", "refs/stash",
                            "refs/remotes"]).split("\n") if x]
    for idx, (raw, _rx) in enumerate(patterns, 1):
        needle = raw if not raw.startswith("regex:") else None
        if needle is None:
            continue
        out = run(["git", "log", "--all", "--oneline", "-S", needle, "--format=%h"])
        for c in [x for x in out.split("\n") if x]:
            hits.append((idx, "历史提交 " + c, ""))
    return hits, len(refs)


def report(title, hits):
    print("%s：%s" % (title, "干净" if not hits else "命中 %d 处" % len(hits)))
    seen = set()
    for idx, where, frag in hits:
        key = (idx, where)
        if key in seen:
            continue
        seen.add(key)
        print("  [模式#%d] %s" % (idx, where))
        if frag:
            print("          …%s…" % frag)
    return not hits


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "--staged"
    patterns = load_patterns()
    if mode == "--staged":
        hits, n = check_staged(patterns)
        ok = report("暂存内容（%d 个文件）" % n, hits)
    elif mode == "--worktree":
        hits, n, artifacts = check_worktree(patterns)
        ok = report("已跟踪文件（%d 个）" % n, hits)
        if artifacts:
            print("  提示：以下本地产物不入库，但文本层含敏感串（交完记得删）：")
            for rel, ids in artifacts:
                print("    %s  ← 模式 %s" % (rel, ids))
    elif mode == "--history":
        hits, nrefs = check_history(patterns)
        ok = report("历史（%d 个 ref 全查）" % nrefs, hits)
        if not ok:
            print("  说明：HEAD 干净但历史仍有，需要 git-filter-repo 重写才算清完。")
    else:
        sys.stderr.write(__doc__)
        sys.exit(2)
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
