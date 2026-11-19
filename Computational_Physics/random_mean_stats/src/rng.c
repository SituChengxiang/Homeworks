#include <stdlib.h>

#include "rng.h"

/*
 * C89 的 rand() 只保证返回 [0, RAND_MAX]，RAND_MAX 的最小保证值是 32767。
 * 一次调用只有 15 位分辨率，所以这里用两次调用拼出约 30 位：
 *
 *     u = (rand() + rand() / (RAND_MAX + 1)) / (RAND_MAX + 1)
 *
 * 结果落在 [0, 1) 内，且与 RAND_MAX 的具体取值无关。
 */
void rng_seed(unsigned long seed)
{
    /* C89 的 srand 只保证接受非负 int；高低位都参与，避免不同种子被截成同一个 */
    srand((unsigned int)(seed & 0xFFFFFFFFUL));
}

double rng_uniform(void)
{
    double a = (double)rand();
    double b = (double)rand() / ((double)RAND_MAX + 1.0);
    return (a + b) / ((double)RAND_MAX + 1.0);
}
