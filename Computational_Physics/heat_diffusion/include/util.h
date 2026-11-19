#ifndef UTIL_H
#define UTIL_H

/*
 * 跨模块共用的小工具。
 *
 * 之所以有这一个模块：`value_is_finite` 原先在 solver.c 和 crank.c 里
 * 各写了一份同名 static 函数。分开编译相安无事，但 make_single_source.py
 * 把两者并成一个翻译单元时就是重定义、直接编不过。单文件版不只是个演示
 * 品，它由 test/run.sh 逐字节比对模块版的输出，所以它同时也是
 * 「静态作用域有没有被滥用」的检查器。
 */

/*
 * C89 没有 isfinite / NAN（都是 C99 的）。用两条只依赖 IEEE 算术的判据：
 *   v != v        -> NaN
 *   v - v != 0    -> +-inf（inf - inf 是 NaN）；有限值必然满足 v-v == 0
 * 不开 -ffast-math，编译器不会把 v-v 优化成 0。
 */
int util_is_finite(double v);

/* 整个数组是否都有限 */
int util_all_finite(const double *v, long n);

#endif