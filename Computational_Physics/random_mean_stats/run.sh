#!/bin/sh
# 一键流程：严格 C89 编译 -> 采样/分箱/插值/拟合 -> 最终图
set -e
cd "$(dirname "$0")"

mkdir -p data

sh build.sh
./build/random_mean_stats data "$@"
python3 plot_final.py data
