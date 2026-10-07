#!/bin/sh
# 一键流程：严格 C89 编译 -> 积分六条轨迹 -> 作图
set -e
cd "$(dirname "$0")"

mkdir -p data build

echo "==> [1/3] 编译（-std=c89 -pedantic-errors -Wall -Wextra）"
gcc -std=c89 -pedantic-errors -Wall -Wextra -O2 \
    -Iinclude \
    src/main.c src/cases.c src/coulomb.c src/dynamics.c \
    src/stepper.c src/rk4.c src/trajectory.c src/rutherford.c src/export.c \
    -lm -o build/alpha_scattering
echo "    零警告通过"

echo "==> [2/3] 积分并导出 data/traj_1.dat ... traj_6.dat"
./build/alpha_scattering data

echo "==> [3/3] 作图 data/trajectories.png / .pdf"
python3 plot.py data
