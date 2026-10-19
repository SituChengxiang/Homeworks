#!/usr/bin/env bash
# 校验作业 A：反轴的群论事实、展开图数出来的量、分步脚本、页面 id 与单文件产物
set -euo pipefail
cd "$(dirname "$0")/../.."      # 回到 Condense_Physics 根目录

out="$(mktemp)"
npx --yes esbuild@0.23.1 rotoinversion/test/verify.js \
  --bundle --format=cjs --platform=node --outfile="$out" --log-level=warning
node "$out"
rm -f "$out"
