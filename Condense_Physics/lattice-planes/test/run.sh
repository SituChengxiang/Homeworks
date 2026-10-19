#!/usr/bin/env bash
# 校验 {110} 晶面族的数学、分步脚本与页面 id 一致性
set -euo pipefail
cd "$(dirname "$0")/../.."      # 回到 Condense_Physics 根目录

out="$(mktemp)"
npx --yes esbuild@0.23.1 lattice-planes/test/verify.js \
  --bundle --format=cjs --platform=node \
  --alias:three=./vendor/three.module.js \
  --outfile="$out" --log-level=warning
node "$out"
rm -f "$out"
