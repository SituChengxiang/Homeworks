#!/usr/bin/env bash
# 对 WS 原胞的几何与物理量做自动校验
set -euo pipefail
cd "$(dirname "$0")/.."

out="$(mktemp)"
npx --yes esbuild@0.23.1 test/verify.js \
  --bundle --format=cjs --platform=node \
  --alias:three=./vendor/three.module.js \
  --outfile="$out" --log-level=warning
node "$out"
rm -f "$out"
