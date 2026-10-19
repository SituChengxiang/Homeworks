#!/usr/bin/env bash
# 校验反轴与 Td 的数学、分步脚本、页面 id 一致性
set -euo pipefail
cd "$(dirname "$0")/../.."      # 回到 Condense_Physics 根目录

out="$(mktemp)"
npx --yes esbuild@0.23.1 symmetry/test/verify.js \
  --bundle --format=cjs --platform=node \
  --alias:three=./vendor/three.module.js \
  --outfile="$out" --log-level=warning
node "$out"
rm -f "$out"
