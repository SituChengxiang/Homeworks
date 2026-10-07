#!/usr/bin/env bash
# 打包：生成 app.js（供 index.html 使用）和单文件 crystal-cells.html
set -euo pipefail
cd "$(dirname "$0")"

npx --yes esbuild@0.23.1 src/main.js \
  --bundle --format=iife --minify \
  --alias:three=./vendor/three.module.js \
  --outfile=app.js --log-level=warning

node -e '
const fs = require("fs");
let html = fs.readFileSync("index.html", "utf8");
const js = fs.readFileSync("app.js", "utf8").replace(/<\/script/gi, "<\\/script");
html = html.replace(
  /<script src="\.\/app\.js"><\/script>/,
  () => "<script>\n" + js + "\n</script>"
);
fs.writeFileSync("crystal-cells.html", html);
console.log("生成 crystal-cells.html (" + (Buffer.byteLength(html) / 1024).toFixed(0) + " KB)");
'
