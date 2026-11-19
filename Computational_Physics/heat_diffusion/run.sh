#!/bin/sh
# 一键流程：严格 C89 编译 -> 计算并导出 data/ -> 出图 -> 像素核对
set -e
cd "$(dirname "$0")"

mkdir -p data

sh build.sh
./build/heat_diffusion data "$@"
python3 plot_final.py data
python3 test/check_figs.py data
