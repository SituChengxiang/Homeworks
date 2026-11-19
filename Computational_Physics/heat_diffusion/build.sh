#!/bin/sh
# 不经过 CMake 的单行编译，用于快速验证"纯 C89 能编过"
set -e
cd "$(dirname "$0")"
mkdir -p build

gcc -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 \
    -Iinclude \
    src/main.c src/material.c src/grid.c src/solver.c src/analytic.c \
    src/crank.c src/converge.c src/export.c src/verify.c src/util.c \
    -lm -o build/heat_diffusion

echo "==> 编译通过（-std=c89 -pedantic-errors -Werror，警告即失败）"