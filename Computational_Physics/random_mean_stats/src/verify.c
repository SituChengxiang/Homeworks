#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "verify.h"
#include "gauss_fit.h"

#define MAX_SAMPLES 2000000L
#define NB 10
#define BW 0.1

typedef struct {
    double m;
    double sigma;
    double amp;
    double sse;
} Fit;

static int g_fail = 0;

static void check(const char *name, double got, double want, double tol)
{
    double err = fabs(got - want);
    int ok = (err <= tol);

    if (!ok) {
        g_fail++;
    }
    printf("  %-46s % .10e  期望 %.3e  误差 %.2e  容差 %.0e  %s\n",
           name, got, want, err, tol, ok ? "PASS" : "FAIL");
}

/* 升幂系数按 Horner 求值：c[0] + c[1] x + ... */
static double horner(const double *c, int n, double x)
{
    int k;
    double s = 0.0;

    for (k = n - 1; k >= 0; k--) {
        s = s * x + c[k];
    }
    return s;
}

static void info(const char *name, double v)
{
    printf("  %-46s %.6f（参考量，不作判定）\n", name, v);
}

/* 读数据文件，跳过 # 注释行；每次读 ncol 个浮点数存一行 */
static long read_rows(FILE *fp, int ncol, double *buf, long cap)
{
    char line[512];
    long n = 0;

    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        char *tok;
        int k = 0;

        if (line[0] == '#' || line[0] == '\n' || line[0] == '\0') {
            continue;
        }
        if (n >= cap) {
            break;
        }
        tok = strtok(line, " \t\n");
        while (tok != NULL) {
            if (k < ncol) {
                buf[n * ncol + k] = atof(tok);
                k++;
            }
            tok = strtok(NULL, " \t\n");
        }
        if (k < ncol) {
            fprintf(stderr, "  数据行字段不足：%s", line);
            g_fail++;
            continue;
        }
        n++;
    }
    return n;
}

/* 读 tag key value 形式的参数文件，挑出 primary / free / literal 三组 */
static int read_fits(const char *path, Fit *prim, Fit *free_fit, Fit *lit)
{
    FILE *fp = fopen(path, "r");
    char line[256];
    char tag[32], key[32];
    double v;

    if (fp == NULL) {
        fprintf(stderr, "verify: 读不到 %s\n", path);
        return 0;
    }
    memset(prim, 0, sizeof(*prim));
    memset(free_fit, 0, sizeof(*free_fit));
    memset(lit, 0, sizeof(*lit));

    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        Fit *dst = NULL;

        if (line[0] == '#') {
            continue;
        }
        if (sscanf(line, "%31s %31s %lf", tag, key, &v) != 3) {
            continue;
        }
        if (strcmp(tag, "primary") == 0) {
            dst = prim;
        } else if (strcmp(tag, "free") == 0) {
            dst = free_fit;
        } else if (strcmp(tag, "literal") == 0) {
            dst = lit;
        } else {
            continue;
        }
        if (strcmp(key, "m") == 0) {
            dst->m = v;
        } else if (strcmp(key, "sigma") == 0) {
            dst->sigma = v;
        } else if (strcmp(key, "A") == 0) {
            dst->amp = v;
        } else if (strcmp(key, "SSE") == 0) {
            dst->sse = v;
        }
    }
    fclose(fp);
    return 1;
}

/*
 * 最小二乘驻点条件（法方程）：极小点处 J^T r = 0。
 * 按柯西不等式，|J^T r| <= sqrt(J^T J)·sqrt(r^T r)，所以用这个尺度归一化。
 */
static void stationarity(const char *label, const double *xs, const double *ys,
                         double m, double s, double A, int fit_amp)
{
    int i;
    double sse = 0.0;
    double gm = 0.0, gs = 0.0, ga = 0.0;
    double qm = 0.0, qs = 0.0, qa = 0.0;

    for (i = 0; i < NB; i++) {
        double z = (xs[i] - m) / s;
        double f = gauss_model(xs[i], m, s, A);
        double r = ys[i] - f;
        double jm = f * z / s;
        double js = f * (z * z - 1.0) / s;
        double ja = f / A;

        sse += r * r;
        gm += r * jm;
        gs += r * js;
        ga += r * ja;
        qm += jm * jm;
        qs += js * js;
        qa += ja * ja;
    }

    printf("\n  [%s]  m=%.6f  sigma=%.6f  A=%.3f  SSE=%.4f\n",
           label, m, s, A, sse);
    check("  |sum r·df/dm| / (|J_m|·|r|)", fabs(gm) / (sqrt(qm * sse) + 1e-300),
          0.0, 1e-6);
    check("  |sum r·df/dsigma| / (|J_sigma|·|r|)",
          fabs(gs) / (sqrt(qs * sse) + 1e-300), 0.0, 1e-6);
    if (fit_amp) {
        check("  |sum r·df/dA| / (|J_A|·|r|)",
              fabs(ga) / (sqrt(qa * sse) + 1e-300), 0.0, 1e-6);
    }
}

int verify_exported(const char *outdir)
{
    char path[512];
    FILE *fp;
    double *sbuf;
    double hist[NB][6];
    double coef[NB];
    double xs[NB], ys[NB];
    long n, i;
    long recount[NB];
    double sum_counts = 0.0;
    double sum_expected = 0.0;
    double worst_recount = 0.0;
    double worst_edge = 0.0;
    double worst_node = 0.0;
    double mid_sum = 0.0;
    double sse_recomputed = 0.0;
    long out_of_range = 0;
    unsigned long seed = 0;
    double worst_hand = 0.0;
    Fit prim, free_fit, lit;

    printf("== 代入式核对（只读 data 目录里的 .dat 里的数字，不重跑算法）==\n");

    sbuf = (double *)malloc(sizeof(double) * 2 * (size_t)MAX_SAMPLES);
    if (sbuf == NULL) {
        fprintf(stderr, "verify: out of memory\n");
        return 1;
    }

    /* ---- samples.dat：全部样本 + 按定义重数一遍分箱 ---- */
    sprintf(path, "%s/samples.dat", outdir);
    fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "verify: 读不到 %s，请先运行程序\n", path);
        free(sbuf);
        return 1;
    }
    {
        char hdr[512];

        /* 头一行是 # 注释，read_rows 本来就会跳过，这里先把它读出来取 seed */
        if (fgets(hdr, (int)sizeof(hdr), fp) != NULL) {
            char *p = strstr(hdr, "seed=");
            if (p != NULL) {
                seed = strtoul(p + 5, NULL, 10);
            }
        }
    }
    n = read_rows(fp, 2, sbuf, MAX_SAMPLES);
    fclose(fp);

    for (i = 0; i < NB; i++) {
        recount[i] = 0;
    }
    for (i = 0; i < n; i++) {
        double x = sbuf[2 * i + 1];
        long b;
        if (x < 0.0 || x > 1.0) {
            out_of_range++;
        }
        b = (long)(x / BW);
        if (b >= NB) {
            b = NB - 1;
        }
        if (b < 0) {
            b = 0;
        }
        recount[b]++;
    }
    printf("  样本数 N = %ld，seed = %lu\n", n, seed);

    /* V9：把导出的前 12 个均匀数按 3 个一组取平均，与前 4 个样本对 */
    if (n >= 4) {
        double ubuf[12 * 2];
        long got = -1;

        sprintf(path, "%s/uniforms_head.dat", outdir);
        fp = fopen(path, "r");
        if (fp != NULL) {
            got = read_rows(fp, 2, ubuf, 12);
            fclose(fp);
        }
        if (got != 12) {
            printf("  uniforms_head.dat 缺失或行数不足（读到 %ld 行），V9 判失败\n", got);
            worst_hand = 1.0;
        } else {
            long k;
            for (k = 0; k < 4; k++) {
                double sum = ubuf[(3 * k) * 2 + 1];
                double xhat, d;
                int j;
                for (j = 1; j < 3; j++) {
                    sum += ubuf[(3 * k + j) * 2 + 1];
                }
                xhat = sum / 3.0;
                d = fabs(xhat - sbuf[2 * k + 1]);
                if (d > worst_hand) {
                    worst_hand = d;
                }
            }
        }
        check("V9 前 4 个 x_i == 导出的均匀数按 3 个一组取平均（max 差）",
              worst_hand, 0.0, 1e-12);
    }
    check("V1 越界样本数（每个 x 都必须属于某一箱）", (double)out_of_range,
          0.0, 0.0);

    /* ---- histogram.dat ---- */
    sprintf(path, "%s/histogram.dat", outdir);
    fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "verify: 读不到 %s\n", path);
        free(sbuf);
        return 1;
    }
    if (read_rows(fp, 6, hist[0], NB) != NB) {
        printf("  直方图应为 %d 行\n", NB);
        g_fail++;
    }
    fclose(fp);

    for (i = 0; i < NB; i++) {
        xs[i] = hist[i][3];
        ys[i] = hist[i][4];
        sum_counts += hist[i][4];
        sum_expected += hist[i][5];
        if ((double)fabs(hist[i][4] - (double)recount[i]) > worst_recount) {
            worst_recount = fabs(hist[i][4] - (double)recount[i]);
        }
        if (i + 1 < NB && fabs(hist[i + 1][1] - hist[i][2]) > worst_edge) {
            worst_edge = fabs(hist[i + 1][1] - hist[i][2]);
        }
    }
    check("V2 计数之和 == N", sum_counts, (double)n, 0.0);
    check("V3 记录的计数 == 由样本重数的计数（max 差）", worst_recount, 0.0, 0.0);
    check("V4 期望计数之和 == N（CDF 伸缩和）", sum_expected, (double)n,
          1e-9 * (double)n);
    check("V5 相邻箱首尾相接（max |lo(i+1) - hi(i)|）", worst_edge, 0.0, 1e-12);

    /* ---- lagrange_coeffs.dat：把系数代回节点 ---- */
    sprintf(path, "%s/lagrange_coeffs.dat", outdir);
    fp = fopen(path, "r");
    if (fp == NULL) {
        fprintf(stderr, "verify: 读不到 %s\n", path);
        free(sbuf);
        return 1;
    }
    {
        double tmp[NB * 2];
        if (read_rows(fp, 2, tmp, NB) != NB) {
            printf("  系数应为 %d 个\n", NB);
            g_fail++;
        }
        fclose(fp);
        for (i = 0; i < NB; i++) {
            coef[i] = tmp[2 * i + 1];
        }
    }
    for (i = 0; i < NB; i++) {
        double d = fabs(horner(coef, NB, xs[i]) - ys[i]);
        if (d > worst_node) {
            worst_node = d;
        }
    }
    check("V6 文件系数代回节点 max|P(x_i) - y_i|", worst_node, 0.0, 1e-5);

    /* ---- gauss_fit_params.dat：三种口径都代回检查 ---- */
    sprintf(path, "%s/gauss_fit_params.dat", outdir);
    if (!read_fits(path, &prim, &free_fit, &lit)) {
        free(sbuf);
        return 1;
    }
    for (i = 0; i < NB; i++) {
        double r = ys[i] - gauss_model(xs[i], prim.m, prim.sigma, prim.amp);
        sse_recomputed += r * r;
        mid_sum += gauss_model(xs[i], prim.m, prim.sigma, prim.amp) * BW;
    }
    check("V7 记录的 SSE == 代回重算的 SSE（主拟合）", sse_recomputed, prim.sse,
          1e-6 * prim.sse + 1e-9);
    /* f 是「计数密度」：积分应等于幅度 A = N·dx，而不是 N */
    check("V8 sum f(x_i)·dx == A = N·dx（中点法）", mid_sum, prim.amp,
          0.01 * prim.amp);

    stationarity("主拟合 A=N·dx，拟合 m、sigma", xs, ys, prim.m, prim.sigma,
                 prim.amp, 0);
    stationarity("自由幅度拟合 m、sigma、A", xs, ys, free_fit.m, free_fit.sigma,
                 free_fit.amp, 1);

    printf("\n  [反面对照：字面 A=N]  m=%.6f  sigma=%.6f  A=%.1f  SSE=%.2f\n",
           lit.m, lit.sigma, lit.amp, lit.sse);
    info("  与主拟合的 SSE 之比", lit.sse / (prim.sse + 1e-300));
    printf("  说明：字面 A=N 同样满足上面的驻点条件（它确实是自己那个 SSE 的\n"
           "  极小点），所以「代入恒等式」能证明实现自洽，不能证明模型选得对；\n"
           "  模型好坏要靠 V7 的 SSE 比值和第 5 步的精确分布对照来判。\n");

    free(sbuf);

    if (g_fail) {
        printf("\nSUBSTITUTION: %d 项失败\n\n", g_fail);
    } else {
        printf("\nSUBSTITUTION: ALL PASS\n\n");
    }
    return g_fail;
}
