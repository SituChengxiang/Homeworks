#!/bin/sh
# 验证入口：三层证据 + 稳健性 + 单文件等价性 + 图形核对 + 负向对照
#
# 数据由 C 程序生成，核对脚本只用 Python/numpy 重算，不参与计算本身。
# 最后一步「负向对照」不是可选项：一套不会失败的检查等于没有检查。
set -e
cd "$(dirname "$0")/.."

if [ ! -x build/heat_diffusion ]; then
    sh build.sh
fi

echo "=== [1/7] 程序内检查 + 代入式核对（完整跑并导出 data/）==="
./build/heat_diffusion data | tail -n 4

echo
echo "=== [2/7] C 单元自检（test/selfcheck.c，期望答案全部手工算出）==="
gcc -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 -Iinclude \
    test/selfcheck.c src/material.c src/grid.c src/solver.c src/crank.c \
    src/analytic.c src/converge.c src/util.c -lm -o build/selfcheck
./build/selfcheck | tail -n 3

echo
echo "=== [3/7] Python 独立实现交叉核对（test/verify.py）==="
python3 test/verify.py | tail -n 3

echo
echo "=== [4/7] 稳健性对照：换网格、换格式（test/sensitivity.c）==="
gcc -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 -Iinclude \
    test/sensitivity.c src/material.c src/grid.c src/solver.c src/crank.c \
    src/analytic.c src/converge.c src/util.c -lm -o build/sensitivity
./build/sensitivity | tail -n 2

echo
echo "=== [5/7] 单文件版等价性（make_single_source.py 的生成物）==="
python3 make_single_source.py > /dev/null
gcc -std=c89 -pedantic-errors -Wall -Wextra -Werror -O2 \
    heat_diffusion_single.c -lm -o build/hd_single
rm -rf build/single_out
mkdir -p build/single_out
./build/hd_single build/single_out > /dev/null
bad=0
tot=0
for f in data/*.dat; do
    b=$(basename "$f")
    tot=$((tot + 1))
    if ! cmp -s "data/$b" "build/single_out/$b"; then
        echo "  DIFF  $b"
        bad=$((bad + 1))
    fi
done
if [ "$bad" -ne 0 ]; then
    echo "SINGLE: $bad 个文件与模块版不一致"
    exit 1
fi
echo "  SINGLE: $tot 个数据文件与模块版逐字节一致"

echo
echo "=== [6/7] 交付图像素核对（test/check_figs.py）==="
python3 plot_final.py data > /dev/null
python3 test/check_figs.py data | tail -n 2

echo
echo "=== [7/7] 负向对照：三套检查都必须真的会失败 ==="
fail_layer=0

# (a) 篡改交付数据，只核对不重算，必须报错
rm -rf build/neg && cp -r data build/neg
python3 - <<'PY'
import glob, os
p = os.path.join("build", "neg", "profiles_fine_dirichlet.dat")
out = []
for line in open(p, encoding="utf-8"):
    if line.startswith("0.044999999999999998"):
        c = line.split()
        c[1] = "51"
        line = " ".join(c) + "\n"
    out.append(line)
open(p, "w", encoding="utf-8").writelines(out)
PY
if ./build/heat_diffusion --verify-only build/neg > build/neg_a.log 2>&1; then
    echo "  (a) 篡改数据未被发现 —— 不合格"
    fail_layer=1
else
    echo "  (a) 篡改 u_t0 一格 50->51：$(grep -o '[0-9]* 项失败' build/neg_a.log | head -1)（预期失败，合格）"
fi

# (b) 把初值强制置零跑稳健性层，必须报错
if ./build/sensitivity --sabotage > build/neg_b.log 2>&1; then
    if grep -q "如预期失败" build/neg_b.log; then
        echo "  (b) sabotage：$(grep -o '[0-9]* 项如预期失败' build/neg_b.log)（合格）"
    else
        echo "  (b) sabotage 没有让任何检查失败 —— 不合格"
        fail_layer=1
    fi
else
    echo "  (b) sabotage 退出码非 0，需检查其判定逻辑"
    fail_layer=1
fi

# (c) 抹掉一张交付图里的收敛曲线，图形核对必须报错
cp data/fig_d1_dirichlet_dx01.png build/neg_c.png
python3 - <<'PY'
import os
import numpy as np
from PIL import Image
p = os.path.join("build", "neg_c.png")
a = np.asarray(Image.open(p).convert("RGB")).astype(int)
m = (np.abs(a - np.array([0xB2, 0x18, 0x2B])).max(axis=2) <= 18)
a[m] = 255
Image.fromarray(a.astype("uint8")).save(p)
PY
mkdir -p build/negfig && cp data/*.png data/*.pdf build/negfig/ 2>/dev/null || true
cp build/neg_c.png build/negfig/fig_d1_dirichlet_dx01.png
if python3 test/check_figs.py build/negfig > build/neg_c.log 2>&1; then
    echo "  (c) 抹掉收敛曲线未被发现 —— 不合格"
    fail_layer=1
else
    echo "  (c) 抹掉收敛曲线：$(grep -o '[0-9]* 项失败' build/neg_c.log | head -1)（预期失败，合格）"
fi

rm -rf build/neg build/negfig build/neg_a.log build/neg_b.log build/neg_c.log build/neg_c.png
if [ "$fail_layer" -ne 0 ]; then
    echo "NEGATIVE-CONTROLS: FAIL"
    exit 1
fi
echo "NEGATIVE-CONTROLS: PASS"
