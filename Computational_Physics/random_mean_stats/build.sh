#!/bin/sh
# 不经过 CMake 的单行编译，用于快速验证"纯 C89 能编过"
set -e
cd "$(dirname "$0")"
mkdir -p build

gcc -std=c89 -pedantic-errors -Wall -Wextra -O2 \
    -Iinclude \
    src/main.c src/rng.c src/sampling.c src/stats.c \
    src/histogram.c src/irwin_hall.c src/lagrange.c src/gauss_fit.c src/verify.c \
    -lm -o build/random_mean_stats

echo "==> 编译通过（-std=c89 -pedantic-errors 无警告）"
