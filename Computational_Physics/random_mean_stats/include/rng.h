#ifndef RNG_H
#define RNG_H

/*
 * 均匀随机数：直接包一层 C89 的 rand()。
 *
 * srand(seed) 固定种子后，本机上每次运行得到同一串数，报告里印的数字
 * 与 data 目录里的 .dat 一一对应；但 C89 只规定 rand() 的取值范围（RAND_MAX >= 32767），
 * 不规定序列，所以换 libc（glibc / MinGW）会得到另一串数。统计结论不受影响
 * （见 test/rng_sensitivity.c 的对照），具体计数以提交的 data 目录下的数据文件为准。
 *
 * rand() 的精度可能只有 15 位，所以每次用两个 rand() 拼出约 30 位小数，
 * 免得采样结果的网格粗到影响分箱与拟合。
 */
void rng_seed(unsigned long seed);

/* 返回 [0, 1) 上的均匀随机数 */
double rng_uniform(void);

#endif
