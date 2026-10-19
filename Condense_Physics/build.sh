#!/usr/bin/env bash
# 打包：每个页面都生成 打包脚本 + 可直接双击的单文件版本
set -euo pipefail
cd "$(dirname "$0")"

build_page() {
  local entry="$1" bundle="$2" page="$3" single="$4"

  npx --yes esbuild@0.23.1 "$entry" \
    --bundle --format=iife --minify \
    --alias:three=./vendor/three.module.js \
    --outfile="$bundle" --log-level=warning

  ENTRY="$entry" BUNDLE="$bundle" PAGE="$page" SINGLE="$single" node -e '
    const fs = require("fs");
    const { PAGE, BUNDLE, SINGLE } = process.env;
    let html = fs.readFileSync(PAGE, "utf8");
    const js = fs.readFileSync(BUNDLE, "utf8").replace(/<\/script/gi, "<\\/script");
    const tag = `<script src="./${BUNDLE}"></script>`;
    if (!html.includes(tag)) { console.error(`${PAGE} 里找不到 ${tag}`); process.exit(1); }
    html = html.replace(tag, () => `<script>\n${js}\n</script>`);
    fs.writeFileSync(SINGLE, html);
    console.log(`生成 ${SINGLE} (${(Buffer.byteLength(html) / 1024).toFixed(0)} KB)`);
  '
}

#     入口             打包产物           页面            单文件成品
build_page src/main.js     app.js          index.html      crystal-cells.html
build_page src/diamond.js  diamond-app.js  diamond.html    diamond-zincblende.html
build_page src/hcp.js      hcp-app.js      hcp.html        hcp-close-packed.html
