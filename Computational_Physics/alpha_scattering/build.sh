#!/bin/sh
# 不经过 CMake 的单行编译，用于快速验证"纯 C89 能编过"
set -e
cd "$(dirname "$0")"
mkdir -p data build

gcc -std=c89 -pedantic-errors -Wall -Wextra -O2 \
    -Iinclude \
    src/main.c src/cases.c src/coulomb.c src/dynamics.c \
    src/stepper.c src/rk4.c \
    src/trajectory.c src/rutherford.c src/export.c \
    -lm -o build/alpha_scattering

echo "==> 编译通过（-std=c89 -pedantic-errors 无警告）"
