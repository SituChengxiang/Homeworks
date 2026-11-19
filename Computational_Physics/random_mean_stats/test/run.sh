#!/bin/sh
# 验证入口：代入式核对 + C 单元自检 + Python 独立交叉核对 + 发生器敏感性
# 数据由 C 程序生成，核对脚本只用 Python/numpy 重算，不参与计算本身。
set -e
cd "$(dirname "$0")/.."

if [ ! -x build/random_mean_stats ]; then
    sh build.sh
fi
if [ ! -f data/histogram.dat ]; then
    mkdir -p data
    ./build/random_mean_stats data
fi

echo "=== [1/5] 代入式核对（程序读回 data 里的数字自己验）==="
mkdir -p data
./build/random_mean_stats data | tail -n +1 | sed -n '/代入式核对/,$p'

echo "=== [2/5] C 单元自检（test/selfcheck.c）==="
gcc -std=c89 -pedantic-errors -Wall -Wextra -O2 -Iinclude \
    test/selfcheck.c src/irwin_hall.c src/lagrange.c src/gauss_fit.c \
    -lm -o build/selfcheck
./build/selfcheck

echo "=== [3/5] Python 独立交叉核对（test/verify.py）==="
python3 test/verify.py

echo "=== [4/5] 发生器敏感性（同一流程换三种随机源，看结论是否变）==="
gcc -std=c89 -pedantic-errors -Wall -Wextra -O2 -Iinclude \
    test/rng_sensitivity.c src/rng.c src/histogram.c src/gauss_fit.c src/irwin_hall.c \
    -lm -o build/rng_sensitivity
./build/rng_sensitivity

echo "=== [5/5] 单文件版等价性（make_single_source.py 的生成物）==="
python3 make_single_source.py > /dev/null
gcc -std=c89 -pedantic-errors -Wall -Wextra -O2 \
    random_mean_stats_single.c -lm -o build/rms_single
rm -rf build/single_out
mkdir -p build/single_out
./build/rms_single build/single_out > /dev/null
bad=0
for f in samples.dat uniforms_head.dat histogram.dat lagrange_coeffs.dat \
         lagrange.dat gauss_fit.dat gauss_fit_params.dat; do
    if ! cmp -s "data/$f" "build/single_out/$f"; then
        echo "  DIFF  $f"
        bad=$((bad + 1))
    fi
done
if [ "$bad" -ne 0 ]; then
    echo "SINGLE: $bad 个文件与模块版不一致"
    exit 1
fi
echo "  SINGLE: 7 个数据文件与模块版逐字节一致"
