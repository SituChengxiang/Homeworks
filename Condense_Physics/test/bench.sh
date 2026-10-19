#!/usr/bin/env bash
# 交互路径上的纯计算耗时基准
set -euo pipefail
cd "$(dirname "$0")/.."

out="$(mktemp)"
npx --yes esbuild@0.23.1 test/bench.js \
  --bundle --format=cjs --platform=node \
  --alias:three=./vendor/three.module.js \
  --outfile="$out" --log-level=warning
node "$out"
rm -f "$out"
