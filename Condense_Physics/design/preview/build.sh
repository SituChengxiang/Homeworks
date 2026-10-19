#!/usr/bin/env bash
# 构建符号层预览页 → 单文件 symbol-preview.html
set -euo pipefail
cd "$(dirname "$0")"

npx --yes vite@6 build
cp dist/index.html symbol-preview.html
rm -rf dist
printf '生成 design/preview/symbol-preview.html (%s KB)\n' \
  "$(( $(wc -c < symbol-preview.html) / 1024 ))"
