/*
 * 发生器敏感性对照：同一套采样 -> 分箱 -> 拟合流程，只换随机源，
 * 看结论（均值、标准差、卡方、拟合 sigma、自由幅度 A）会不会变。
 *
 * 这不是「正确性」检查，而是「稳健性」检查。程序本身用 C89 的 rand()
 * （见 src/rng.c），所以这里的第一行就是它；另外两行是独立的随机流：
 *   - MSVC / MinGW 的 15 位 rand()（RAND_MAX = 32767，C89 允许的下限）；
 *   - 32 位 LCG（1664525 / 1013904223），曾经用过的写法，现在只作对照。
 *
 * 判据（运行前写死）：越界样本数为 0、|均值 - 0.5| < 3 倍标准误、
 * 卡方 < 25（卡方(9) 超过 25 的概率约 0.3%）、拟合 sigma 在 [0.170, 0.190]、
 * 自由幅度 A 在 [99, 103]（= N*dx）。注意卡方本身随样本大幅涨落，
 * 不能拿 16.919 当每行的判据——单次 5% 检验本来就有 5% 会误拒。
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "histogram.h"
#include "gauss_fit.h"
#include "irwin_hall.h"
#include "rng.h"

#define N_SAMPLES 1000
#define N_BINS 10
#define K_UNIFORM 3
#define SEED 20261024UL
#define WIDTH 0.1

/* --- 发生器 B：MSVC / MinGW 的 15 位 rand()，两次调用拼精度 --- */
static unsigned long g_ms;

static double gen_msvc(void)
{
    double a, b;

    g_ms = (g_ms * 214013UL + 2531011UL) & 0x7FFFFFFFUL;
    a = (double)((g_ms >> 16) & 0x7FFFUL);
    g_ms = (g_ms * 214013UL + 2531011UL) & 0x7FFFFFFFUL;
    b = (double)((g_ms >> 16) & 0x7FFFUL);
    return (a + b / 32768.0) / 32768.0;
}

/* --- 发生器 C：32 位 LCG（对照） --- */
static unsigned long g_lcg;

static double gen_lcg(void)
{
    g_lcg = (1664525UL * g_lcg + 1013904223UL) & 0xFFFFFFFFUL;
    return (double)g_lcg / 4294967296.0;
}

static void seed_case(int which)
{
    if (which == 0) {
        rng_seed(SEED);
    } else if (which == 1) {
        g_ms = SEED;
    } else {
        g_lcg = SEED;
    }
}

static double draw(int which)
{
    if (which == 0) {
        return rng_uniform();
    }
    return which == 1 ? gen_msvc() : gen_lcg();
}

static int run_case(const char *label, int which)
{
    static double xs[N_SAMPLES];
    double cx[N_BINS], cy[N_BINS], expc[N_BINS];
    long counts[N_BINS];
    GaussFit fit, fit_free;
    double sum = 0.0, sum2 = 0.0, mean, sd, chisq = 0.0, bias;
    long i, j, out_of_range;
    int ok1, ok2, pass;

    seed_case(which);
    for (i = 0; i < N_SAMPLES; i++) {
        double s = 0.0;
        for (j = 0; j < K_UNIFORM; j++) {
            s += draw(which);
        }
        xs[i] = s / (double)K_UNIFORM;
        sum += xs[i];
        sum2 += xs[i] * xs[i];
    }
    mean = sum / (double)N_SAMPLES;
    sd = sqrt((sum2 - (double)N_SAMPLES * mean * mean) / (double)(N_SAMPLES - 1));

    for (i = 0; i < N_BINS; i++) {
        counts[i] = 0;
    }
    out_of_range = hist_compute(xs, N_SAMPLES, 0.0, WIDTH, N_BINS, counts);
    for (i = 0; i < N_BINS; i++) {
        expc[i] = (double)N_SAMPLES
                * (ih3_cdf_mean(WIDTH * (double)(i + 1)) - ih3_cdf_mean(WIDTH * (double)i));
        chisq += ((double)counts[i] - expc[i]) * ((double)counts[i] - expc[i]) / expc[i];
        cx[i] = hist_center(i, 0.0, WIDTH);
        cy[i] = (double)counts[i];
    }
    ok1 = gauss_fit(cx, cy, N_BINS, (double)N_SAMPLES * WIDTH, 1, &fit);
    ok2 = gauss_fit(cx, cy, N_BINS, (double)N_SAMPLES * WIDTH, 0, &fit_free);
    bias = fit.sigma / sd - 1.0;

    pass = (out_of_range == 0) && ok1 && ok2
        && chisq < 25.0
        && fabs(mean - 0.5) < 3.0 * 0.005270
        && fit.sigma > 0.170 && fit.sigma < 0.190
        && fit_free.amp > 99.0 && fit_free.amp < 103.0;

    printf("  %-9s mean=%.6f  sd=%.6f  chisq=%7.3f  sigma=%.6f (sd%+.2f%%)  A_free=%.3f  %s\n",
           label, mean, sd, chisq, fit.sigma, 100.0 * bias, fit_free.amp,
           pass ? "PASS" : "FAIL");
    if (!pass) {
        printf("            条件：越界=%ld（须 0）、chisq<25、|mean-0.5|<0.0158、"
               "0.170<sigma<0.190、99<A_free<103\n", out_of_range);
    }
    return pass ? 0 : 1;
}

int main(void)
{
    int bad = 0;

    printf("发生器敏感性（同一流程，只换随机源；N=%d, seed=%lu）\n",
           N_SAMPLES, SEED);
    printf("  第一行就是程序实际用的 rand()，应与主程序输出一致\n");
    bad += run_case("rand", 0);
    bad += run_case("rand-15", 1);
    bad += run_case("LCG-32", 2);

    printf("SENSITIVITY: %s\n", bad == 0 ? "ALL PASS" : "FAILED");
    return bad == 0 ? 0 : 1;
}
