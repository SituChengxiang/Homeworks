#ifndef SAMPLING_H
#define SAMPLING_H

/*
 * 生成 n 个样本 x_i，每个 x_i 是 k 个 [0,1) 均匀随机数的算术平均。
 * 采样与随机数发生器分离：换分布只改这一层，rng.c 不用动。
 */
void sample_means(double *out, long n, int k);

#endif
