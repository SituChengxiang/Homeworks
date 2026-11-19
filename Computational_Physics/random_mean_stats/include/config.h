#ifndef CONFIG_H
#define CONFIG_H

/* 题目参数：每个 x_i 由 3 个 [0,1) 均匀随机数取算术平均 */
#define N_UNIFORM 3

/* 重复次数 N = 1000 */
#define DEFAULT_N_SAMPLES 1000L

/* 固定种子：任何人重跑都得到同一组数据，方便核对 */
#define RNG_DEFAULT_SEED 20261024UL

#endif
