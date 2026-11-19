#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "config.h"
#include "rng.h"
#include "sampling.h"
#include "stats.h"
#include "histogram.h"
#include "irwin_hall.h"
#include "lagrange.h"
#include "gauss_fit.h"
#include "verify.h"

/* 单独抽一批原始均匀数，用来检查发生器本身是否均匀 */
#define RAW_DRAWS 30000L

/* 分箱设置：题目要求箱宽 0.1，x 的理论范围是 [0,1] */
#define BIN_WIDTH 0.1
#define BIN_COUNT 10L

static void usage(const char *prog)
{
    fprintf(stderr, "usage: %s [-n samples] [--seed seed] [outdir]\n", prog);
}

static int cmp_double(const void *pa, const void *pb)
{
    double a = *(const double *)pa;
    double b = *(const double *)pb;

    if (a < b) {
        return -1;
    }
    if (a > b) {
        return 1;
    }
    return 0;
}

static void print_step1(const double *x, long n, unsigned long seed)
{
    long i;
    double mean;
    double sd;
    double se;
    double z;
    double rel;

    mean = stats_mean(x, n);
    sd = stats_std(x, n);
    se = (1.0 / 6.0) / sqrt((double)n);
    z = (mean - 0.5) / se;
    rel = (sd - 1.0 / 6.0) / (1.0 / 6.0);

    printf("== Step 1: x_i = mean of %d uniform numbers ==\n", N_UNIFORM);
    printf("N = %ld, seed = %lu\n", n, seed);
    printf("first 5 x_i:");
    for (i = 0; i < 5 && i < n; i++) {
        printf(" %.6f", x[i]);
    }
    printf("\n");
    printf("min = %.6f, max = %.6f (must lie in [0,1])\n",
           stats_min(x, n), stats_max(x, n));
    printf("mean = %.6f  theory = 0.500000  SE = %.6f  z = %+.2f\n",
           mean, se, z);
    printf("std  = %.6f  theory = 0.166667  rel.dev = %+.2f%%\n",
           sd, 100.0 * rel);

    /* 前 3 个 u 与 x_1 的关系：数据由 uniforms_head.dat 导出，读者可代回核对 */
    {
        double u[3], xhat;
        int j;

        rng_seed(seed);
        for (j = 0; j < 3; j++) {
            u[j] = rng_uniform();
        }
        xhat = (u[0] + u[1] + u[2]) / 3.0;
        printf("  u1..u3 = %.12f %.12f %.12f\n", u[0], u[1], u[2]);
        printf("  x_hat_1 = (u1+u2+u3)/3 = %.12f  vs  x_1 = %.12f  差 = %.2e\n",
               xhat, x[0], fabs(xhat - x[0]));
        printf("\n");
    }

    {
        long m = RAW_DRAWS;
        long j;
        double s1 = 0.0;
        double s2 = 0.0;
        double lo = 1.0;
        double hi = 0.0;

        /* 重新开始，跳过采样用掉的前 3n 个数，让这批原始均匀数与样本错开 */
        rng_seed(seed);
        for (j = 0; j < 3L * n; j++) {
            (void)rng_uniform();
        }
        for (j = 0; j < m; j++) {
            double u = rng_uniform();
            s1 += u;
            s2 += u * u;
            if (u < lo) {
                lo = u;
            }
            if (u > hi) {
                hi = u;
            }
        }
        printf("raw uniform: %ld draws, mean = %.6f (0.5), "
               "var = %.6f (0.083333), min = %.6f, max = %.6f\n",
               m, s1 / (double)m,
               s2 / (double)m - (s1 / (double)m) * (s1 / (double)m),
               lo, hi);
    }

    if (fabs(z) > 3.0) {
        printf("WARNING: sample mean deviates by more than 3 standard errors\n");
    }
    if (fabs(rel) > 0.05) {
        printf("WARNING: sample std deviates by more than 5%%\n");
    }
    printf("\n");
}

static void expected_counts(long n, double *exp_counts)
{
    long i;

    for (i = 0; i < BIN_COUNT; i++) {
        double lo = BIN_WIDTH * (double)i;
        double hi = lo + BIN_WIDTH;
        exp_counts[i] = (double)n * (ih3_cdf_mean(hi) - ih3_cdf_mean(lo));
    }
}

static double ks_statistic(const double *sorted, long n)
{
    long i;
    double d = 0.0;

    for (i = 0; i < n; i++) {
        double f = ih3_cdf_mean(sorted[i]);
        double d1 = fabs((double)(i + 1) / (double)n - f);
        double d2 = fabs((double)i / (double)n - f);
        if (d1 > d) {
            d = d1;
        }
        if (d2 > d) {
            d = d2;
        }
    }
    return d;
}

static void print_step2(const double *x, long n, const long *counts,
                        const double *exp_counts)
{
    long i;
    long total = 0;
    double chi2 = 0.0;
    double *sorted;
    double d;
    double d_crit;

    for (i = 0; i < BIN_COUNT; i++) {
        total += counts[i];
    }

    printf("== Step 2: histogram, width = %.1f, %ld bins ==\n",
           BIN_WIDTH, BIN_COUNT);
    printf(" bin   range          center   count   expected\n");
    for (i = 0; i < BIN_COUNT; i++) {
        printf(" %2ld   [%.2f, %.2f)   %.2f   %5ld   %8.2f\n",
               i, BIN_WIDTH * (double)i, BIN_WIDTH * (double)(i + 1),
               hist_center(i, 0.0, BIN_WIDTH), counts[i], exp_counts[i]);
        if (exp_counts[i] > 0.0) {
            double dlt = (double)counts[i] - exp_counts[i];
            chi2 += dlt * dlt / exp_counts[i];
        }
    }

    sorted = (double *)malloc((size_t)n * sizeof(double));
    if (sorted == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    memcpy(sorted, x, (size_t)n * sizeof(double));
    qsort(sorted, (size_t)n, sizeof(double), cmp_double);
    d = ks_statistic(sorted, n);
    d_crit = 1.36 / sqrt((double)n);
    free(sorted);

    printf("\nsum of counts = %ld   (N = %ld, difference = %ld)\n",
           total, n, total - n);
    printf("chi-square = %.3f  (df = %ld, 5%% critical = 16.919)\n",
           chi2, BIN_COUNT - 1);
    printf("K-S D = %.5f   (5%% critical ~ %.5f)\n", d, d_crit);

    printf("\ncheck: P(x in [0.4,0.5)) = %.6f, expected count = %.2f\n",
           ih3_cdf_mean(0.5) - ih3_cdf_mean(0.4),
           (double)n * (ih3_cdf_mean(0.5) - ih3_cdf_mean(0.4)));
    printf("check: P(x in [0.9,1.0)) = %.6f, expected count = %.2f\n",
           ih3_cdf_mean(1.0) - ih3_cdf_mean(0.9),
           (double)n * (ih3_cdf_mean(1.0) - ih3_cdf_mean(0.9)));
    printf("check: symmetry count[i] vs count[%ld-i] (mirror about 0.5)\n",
           BIN_COUNT - 1);
    for (i = 0; i < BIN_COUNT / 2; i++) {
        printf("  bin %ld = %4ld   |   bin %ld = %4ld   | diff = %ld\n",
               i, counts[i], BIN_COUNT - 1 - i, counts[BIN_COUNT - 1 - i],
               counts[i] - counts[BIN_COUNT - 1 - i]);
    }

    if (total != n) {
        printf("WARNING: %ld samples fell outside the histogram range\n",
               n - total);
    }
    printf("\n");
}

static void run_lagrange(const long *counts, const char *outdir)
{
    int n = (int)BIN_COUNT;
    int i;
    int npts = 901;
    double xs[LAGRANGE_MAX_N];
    double ys[LAGRANGE_MAX_N];
    double c[LAGRANGE_MAX_N];
    double max_node_res = 0.0;
    double max_node_res_direct = 0.0;
    double max_diff = 0.0;
    double cmax = 0.0;
    double p_left;
    double p_right;
    double x0;
    double x1;
    double dx;
    char path[512];
    FILE *fp;

    for (i = 0; i < n; i++) {
        xs[i] = hist_center((long)i, 0.0, BIN_WIDTH);
        ys[i] = (double)counts[i];
    }
    lagrange_coeffs(xs, ys, n, c);

    printf("== Step 4: Lagrange interpolation through %d binned points ==\n", n);
    printf("degree = %d, nodes span [%.2f, %.2f]\n", n - 1, xs[0], xs[n - 1]);
    printf("coefficients (ascending powers):\n");
    for (i = 0; i < n; i++) {
        printf("  c%-2d = % .10e\n", i, c[i]);
        if (fabs(c[i]) > cmax) {
            cmax = fabs(c[i]);
        }
    }

    for (i = 0; i < n; i++) {
        double r = fabs(poly_eval(c, n, xs[i]) - ys[i]);
        double rd = fabs(lagrange_eval(xs, ys, n, xs[i]) - ys[i]);
        if (r > max_node_res) {
            max_node_res = r;
        }
        if (rd > max_node_res_direct) {
            max_node_res_direct = rd;
        }
    }

    /* [0,1] 上 1001 点：系数展开与直接基函数求和互相验证 */
    for (i = 0; i <= 1000; i++) {
        double t = (double)i / 1000.0;
        double d = fabs(poly_eval(c, n, t) - lagrange_eval(xs, ys, n, t));
        if (d > max_diff) {
            max_diff = d;
        }
    }

    p_left = poly_eval(c, n, 0.0);
    p_right = poly_eval(c, n, 1.0);

    printf("\nmax |P_direct(x_i) - y_i| over nodes = %.3e\n", max_node_res_direct);
    printf("max |P_monomial(x_i) - y_i|          = %.3e "
           "(degree-9 monomial basis loses digits)\n", max_node_res);
    printf("max |monomial - direct Lagrange|     = %.3e (1001 grid points)\n",
           max_diff);
    printf("extrapolation: P(0) = %.3f, P(1) = %.3f "
           "(nodes only cover [0.05, 0.95])\n", p_left, p_right);
    printf("largest |coefficient| = %.3e\n", cmax);

    /* 只导出节点区间：区间外的值属于外推，振荡没有物理意义 */
    x0 = xs[0];
    x1 = xs[n - 1];
    dx = (x1 - x0) / (double)(npts - 1);

    sprintf(path, "%s/lagrange.dat", outdir);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    fprintf(fp, "# x P(x)  Lagrange degree %d, %d nodes [%.2f, %.2f]\n",
            n - 1, n, x0, x1);
    for (i = 0; i < npts; i++) {
        double t = x0 + dx * (double)i;
        fprintf(fp, "%.6f %.10e\n", t, lagrange_eval(xs, ys, n, t));
    }
    fclose(fp);
    printf("wrote %s\n", path);

    sprintf(path, "%s/lagrange_coeffs.dat", outdir);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    fprintf(fp, "# degree %d, ascending coefficients c0..c%d\n", n - 1, n - 1);
    for (i = 0; i < n; i++) {
        fprintf(fp, "%d %.15e\n", i, c[i]);
    }
    fclose(fp);
    printf("wrote %s\n", path);
    printf("\n");
}

static void run_gauss_fit(const long *counts, long n_samples,
                          double raw_mean, double raw_std, const char *outdir)
{
    int n = (int)BIN_COUNT;
    int i;
    double xs[LAGRANGE_MAX_N];
    double ys[LAGRANGE_MAX_N];
    double amp_bin = (double)n_samples * BIN_WIDTH;
    GaussFit fit_bin;
    GaussFit fit_free;
    GaussFit fit_literal;
    char path[512];
    FILE *fp;
    int ok1;
    int ok2;
    int ok3;

    for (i = 0; i < n; i++) {
        xs[i] = hist_center((long)i, 0.0, BIN_WIDTH);
        ys[i] = (double)counts[i];
    }

    ok1 = gauss_fit(xs, ys, n, amp_bin, 1, &fit_bin);
    ok2 = gauss_fit(xs, ys, n, amp_bin, 0, &fit_free);
    ok3 = gauss_fit(xs, ys, n, (double)n_samples, 1, &fit_literal);

    printf("== Step 5: Gaussian fit to binned counts ==\n");
    printf("model: f(x) = A/(sigma*sqrt(2*pi)) * exp(-(x-m)^2/(2*sigma^2))\n");
    printf("N = %ld, bin width = %.1f, count scale A = N*dx = %.3f\n\n",
           n_samples, BIN_WIDTH, amp_bin);

    if (!ok1) {
        printf("primary fit failed\n");
        return;
    }

    printf("primary fit (A = N*dx fixed):\n");
    printf("  m = %.6f, sigma = %.6f, A = %.3f\n",
           fit_bin.m, fit_bin.sigma, fit_bin.amp);
    printf("  SSE = %.3f, max|res| = %.3f, iterations = %d\n",
           fit_bin.sse, fit_bin.max_abs_res, fit_bin.iterations);
    printf("  vs sample mean = %.6f  (difference %+.6f)\n",
           raw_mean, fit_bin.m - raw_mean);
    printf("  vs sample std  = %.6f  (sigma is %+.2f%%)\n",
           raw_std, 100.0 * (fit_bin.sigma / raw_std - 1.0));
    printf("  vs theory 1/6  = %.6f  (sigma is %+.2f%%)\n",
           1.0 / 6.0, 100.0 * (fit_bin.sigma / (1.0 / 6.0) - 1.0));

    if (ok2) {
        printf("\ncross-check, A free:\n");
        printf("  m = %.6f, sigma = %.6f, A = %.3f (A/(N*dx) = %.4f)\n",
               fit_free.m, fit_free.sigma, fit_free.amp,
               fit_free.amp / amp_bin);
        printf("  SSE = %.3f, max|res| = %.3f, iterations = %d\n",
               fit_free.sse, fit_free.max_abs_res, fit_free.iterations);
    }
    if (ok3) {
        printf("\ncross-check, literal A = N fixed (formula without bin width):\n");
        printf("  m = %.6f, sigma = %.6f, A = %.3f\n",
               fit_literal.m, fit_literal.sigma, fit_literal.amp);
        printf("  SSE = %.3f, max|res| = %.3f, iterations = %d\n",
               fit_literal.sse, fit_literal.max_abs_res,
               fit_literal.iterations);
    }

    printf("\n bin  center  count   fitted  residual\n");
    for (i = 0; i < n; i++) {
        double f = gauss_model(xs[i], fit_bin.m, fit_bin.sigma, fit_bin.amp);
        printf("  %2d  %.2f  %6.0f  %7.2f  %+8.2f\n",
               i, xs[i], ys[i], f, ys[i] - f);
    }

    sprintf(path, "%s/gauss_fit.dat", outdir);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    fprintf(fp, "# x fitted_count  Gaussian A=N*dx, m=%.6f sigma=%.6f\n",
            fit_bin.m, fit_bin.sigma);
    for (i = 0; i <= 500; i++) {
        double t = (double)i / 500.0;
        fprintf(fp, "%.6f %.10e\n", t,
                gauss_model(t, fit_bin.m, fit_bin.sigma, fit_bin.amp));
    }
    fclose(fp);
    printf("wrote %s\n", path);

    sprintf(path, "%s/gauss_fit_params.dat", outdir);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return;
    }
    fprintf(fp, "# model f(x)=A/(sigma*sqrt(2*pi))*exp(-(x-m)^2/(2*sigma^2))\n");
    fprintf(fp, "primary m %.12f\n", fit_bin.m);
    fprintf(fp, "primary sigma %.12f\n", fit_bin.sigma);
    fprintf(fp, "primary A %.12f\n", fit_bin.amp);
    fprintf(fp, "primary SSE %.12f\n", fit_bin.sse);
    if (ok2) {
        fprintf(fp, "free m %.12f\n", fit_free.m);
        fprintf(fp, "free sigma %.12f\n", fit_free.sigma);
        fprintf(fp, "free A %.12f\n", fit_free.amp);
        fprintf(fp, "free SSE %.12f\n", fit_free.sse);
    }
    if (ok3) {
        fprintf(fp, "literal m %.12f\n", fit_literal.m);
        fprintf(fp, "literal sigma %.12f\n", fit_literal.sigma);
        fprintf(fp, "literal A %.12f\n", fit_literal.amp);
        fprintf(fp, "literal SSE %.12f\n", fit_literal.sse);
    }
    fclose(fp);
    printf("wrote %s\n\n", path);
}

static int write_samples(const char *outdir, const double *x, long n,
                         unsigned long seed)
{
    char path[512];
    FILE *fp;
    long i;

    sprintf(path, "%s/samples.dat", outdir);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return 0;
    }
    fprintf(fp, "# i x_i  (mean of %d uniform numbers, N=%ld, seed=%lu)\n",
            N_UNIFORM, n, seed);
    for (i = 0; i < n; i++) {
        fprintf(fp, "%ld %.12f\n", i + 1, x[i]);
    }
    fclose(fp);
    printf("wrote %s\n", path);
    return 1;
}

static int write_uniforms_head(const char *outdir, unsigned long seed)
{
    char path[512];
    FILE *fp;
    long i;

    sprintf(path, "%s/uniforms_head.dat", outdir);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return 0;
    }
    /* 前 4 个样本用到的 12 个均匀数，供「u -> x」代入核对 */
    fprintf(fp, "# k u_k  (first %d draws after srand(%lu))\n",
            4 * N_UNIFORM, seed);
    rng_seed(seed);
    for (i = 0; i < 4L * N_UNIFORM; i++) {
        fprintf(fp, "%ld %.12f\n", i + 1, rng_uniform());
    }
    fclose(fp);
    printf("wrote %s\n", path);
    return 1;
}

static int write_histogram(const char *outdir, const long *counts,
                           const double *exp_counts, long n)
{
    char path[512];
    FILE *fp;
    long i;

    sprintf(path, "%s/histogram.dat", outdir);
    fp = fopen(path, "w");
    if (fp == NULL) {
        fprintf(stderr, "cannot write %s\n", path);
        return 0;
    }
    fprintf(fp, "# bin lo hi center count expected  (N=%ld, width=%.3f)\n",
            n, BIN_WIDTH);
    for (i = 0; i < BIN_COUNT; i++) {
        fprintf(fp, "%ld %.3f %.3f %.6f %ld %.6f\n",
                i, BIN_WIDTH * (double)i, BIN_WIDTH * (double)(i + 1),
                hist_center(i, 0.0, BIN_WIDTH), counts[i], exp_counts[i]);
    }
    fclose(fp);
    printf("wrote %s\n", path);
    return 1;
}

int main(int argc, char **argv)
{
    long n = DEFAULT_N_SAMPLES;
    unsigned long seed = RNG_DEFAULT_SEED;
    const char *outdir = "data";
    double *x;
    long *counts;
    double exp_counts[BIN_COUNT];
    long i;
    long outside;
    int argi;

    for (argi = 1; argi < argc; argi++) {
        if (strcmp(argv[argi], "-n") == 0 && argi + 1 < argc) {
            n = atol(argv[++argi]);
        } else if (strcmp(argv[argi], "--seed") == 0 && argi + 1 < argc) {
            seed = (unsigned long)strtoul(argv[++argi], NULL, 10);
        } else if (argv[argi][0] != '-') {
            outdir = argv[argi];
        } else {
            usage(argv[0]);
            return 1;
        }
    }
    if (n < 2) {
        fprintf(stderr, "n must be >= 2\n");
        return 1;
    }

    x = (double *)malloc((size_t)n * sizeof(double));
    counts = (long *)malloc((size_t)BIN_COUNT * sizeof(long));
    if (x == NULL || counts == NULL) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }
    for (i = 0; i < BIN_COUNT; i++) {
        counts[i] = 0;
    }

    rng_seed(seed);
    sample_means(x, n, N_UNIFORM);

    print_step1(x, n, seed);

    outside = hist_compute(x, n, 0.0, BIN_WIDTH, BIN_COUNT, counts);
    expected_counts(n, exp_counts);
    print_step2(x, n, counts, exp_counts);
    if (outside != 0) {
        printf("WARNING: %ld samples outside [0,1]\n", outside);
    }

    write_samples(outdir, x, n, seed);
    write_uniforms_head(outdir, seed);
    write_histogram(outdir, counts, exp_counts, n);
    run_lagrange(counts, outdir);
    run_gauss_fit(counts, n, stats_mean(x, n), stats_std(x, n), outdir);

    /* 代入式核对：把刚写进 data 目录里的 .dat 的数字读回来验恒等式 */
    {
        int bad = verify_exported(outdir);
        free(counts);
        free(x);
        return bad ? 1 : 0;
    }
}
