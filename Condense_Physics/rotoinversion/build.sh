#!/usr/bin/env bash
# 作业 A：构建 → 内联成单文件 → 落成可双击的成品
set -euo pipefail
cd "$(dirname "$0")"

npx --yes vite@6 build
cp dist/index.html rotoinversion.html
rm -rf dist
printf '生成 rotoinversion/rotoinversion.html (%s KB)\n' \
  "$(( $(wc -c < rotoinversion.html) / 1024 ))"
