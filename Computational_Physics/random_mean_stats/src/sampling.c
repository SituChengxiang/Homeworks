#include "sampling.h"
#include "rng.h"

void sample_means(double *out, long n, int k)
{
    long i;
    int j;

    for (i = 0; i < n; i++) {
        double sum = 0.0;
        for (j = 0; j < k; j++) {
            sum += rng_uniform();
        }
        out[i] = sum / (double)k;
    }
}
