#!/usr/bin/env bash
# 打包 {110} 晶面族这一页：esbuild 打包 + 把 three.js 和代码内联进一个可双击的离线 HTML。
# 与 symmetry/build.sh 用同一条命令，各页各出一个成品，互不影响。
set -euo pipefail
cd "$(dirname "$0")/.."

ENTRY=lattice-planes/src/main.js
BUNDLE=lattice-planes/plane-app.js
PAGE=lattice-planes/index.html
SINGLE=lattice-planes/plane-110.html

npx --yes esbuild@0.23.1 "$ENTRY" \
  --bundle --format=iife --minify \
  --alias:three=./vendor/three.module.js \
  --outfile="$BUNDLE" --log-level=warning

ENTRY="$ENTRY" BUNDLE="$BUNDLE" PAGE="$PAGE" SINGLE="$SINGLE" REF="$(basename "$BUNDLE")" node -e '
  const fs = require("fs");
  const { PAGE, BUNDLE, SINGLE, REF } = process.env;
  let html = fs.readFileSync(PAGE, "utf8");
  const js = fs.readFileSync(BUNDLE, "utf8").replace(/<\/script/gi, "<\\/script");
  const tag = `<script src="./${REF}"></script>`;   // 页面和打包产物在同一个目录里
  if (!html.includes(tag)) { console.error(`${PAGE} 里找不到 ${tag}`); process.exit(1); }
  html = html.replace(tag, () => `<script>\n${js}\n</script>`);
  fs.writeFileSync(SINGLE, html);
  console.log(`生成 ${SINGLE} (${(Buffer.byteLength(html) / 1024).toFixed(0)} KB)`);
'
